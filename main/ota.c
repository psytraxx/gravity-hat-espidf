#include "ota.h"

#include <string.h>

#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "ota";

/* Core 0 is busy with the physics. */
#define NET_CORE 1

/* How long a new image has to reach the network and run the main loop. */
#define VERIFY_TIMEOUT_MS (120 * 1000)

/* Time for the reply to leave before the restart. */
#define RESTART_DELAY_MS 500

/* Reconnect delay after a dropped or failed join: doubles per failure. */
#define RECONNECT_MIN_MS 1000
#define RECONNECT_MAX_MS (60 * 1000)

#define GOT_IP BIT0
#define APP_OK BIT1

static EventGroupHandle_t s_events;
static esp_timer_handle_t s_reconnect_timer;
static uint32_t s_reconnect_ms = RECONNECT_MIN_MS;

static bool wifi_enabled(void)
{
    return CONFIG_HAT_WIFI_SSID[0] != '\0';
}

static void reconnect(void *arg)
{
    esp_wifi_connect();
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = data;
        ESP_LOGW(TAG, "Wi-Fi disconnected (reason %d), retrying in %lu ms", event->reason,
                 (unsigned long)s_reconnect_ms);
        xEventGroupClearBits(s_events, GOT_IP);
        esp_timer_stop(s_reconnect_timer);
        ESP_ERROR_CHECK(esp_timer_start_once(s_reconnect_timer, s_reconnect_ms * 1000ULL));
        s_reconnect_ms =
            s_reconnect_ms * 2 > RECONNECT_MAX_MS ? RECONNECT_MAX_MS : s_reconnect_ms * 2;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = data;
        ESP_LOGI(TAG, "Wi-Fi up: http://" IPSTR "/ or http://%s.local/", IP2STR(&event->ip_info.ip),
                 CONFIG_HAT_HOSTNAME);
        s_reconnect_ms = RECONNECT_MIN_MS;
        xEventGroupSetBits(s_events, GOT_IP);
    }
}

static void wifi_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_netif_set_hostname(netif, CONFIG_HAT_HOSTNAME));

    const esp_timer_create_args_t timer = {.callback = reconnect, .name = "wifi_retry"};
    ESP_ERROR_CHECK(esp_timer_create(&timer, &s_reconnect_timer));

    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL));

    wifi_config_t config = {0};
    strlcpy((char *)config.sta.ssid, CONFIG_HAT_WIFI_SSID, sizeof(config.sta.ssid));
    strlcpy((char *)config.sta.password, CONFIG_HAT_WIFI_PASSWORD, sizeof(config.sta.password));
    config.sta.threshold.authmode =
        CONFIG_HAT_WIFI_PASSWORD[0] != '\0' ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    /* Ignore WPA3 compatibility (RSN override) elements. Some routers, such as
     * the FRITZ!Box 4040, advertise an RSNXE override in the beacon but leave
     * it out of 4-way handshake message 3, which fails every join with reason
     * 17 (IE differs in 4-way handshake). */
    config.sta.disable_wpa3_compatible_mode = 1;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Joining Wi-Fi \"%s\"", CONFIG_HAT_WIFI_SSID);
}

static void mdns_init_hat(void)
{
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(CONFIG_HAT_HOSTNAME));
    ESP_ERROR_CHECK(mdns_instance_name_set("Gravity hat"));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
}

/* With a token set, requests need "Authorization: Bearer <token>". */
static bool authorized(httpd_req_t *req)
{
    static const char expected[] = "Bearer " CONFIG_HAT_OTA_TOKEN;
    if (CONFIG_HAT_OTA_TOKEN[0] == '\0') {
        return true;
    }
    char header[sizeof(expected) + 1];
    return httpd_req_get_hdr_value_str(req, "Authorization", header, sizeof(header)) == ESP_OK &&
           strcmp(header, expected) == 0;
}

static esp_err_t get_status(httpd_req_t *req)
{
    const esp_app_desc_t *app = esp_app_get_description();
    char body[128];
    snprintf(body, sizeof(body), "%s %s, running from %s\n", app->project_name, app->version,
             esp_ota_get_running_partition()->label);
    return httpd_resp_sendstr(req, body);
}

/* The body is the app image, build/gravity-hat.bin. */
static esp_err_t post_ota(httpd_req_t *req)
{
    if (!authorized(req)) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Wrong or missing token");
    }
    const esp_partition_t *slot = esp_ota_get_next_update_partition(NULL);
    if (req->content_len == 0 || req->content_len > slot->size) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Image empty or too big");
    }
    ESP_LOGI(TAG, "Receiving %u bytes into %s", (unsigned)req->content_len, slot->label);

    /* Erases as it goes rather than the whole slot up front, so flash
     * stalls the physics for short moments only. */
    esp_ota_handle_t ota;
    esp_err_t err = esp_ota_begin(slot, OTA_WITH_SEQUENTIAL_WRITES, &ota);
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
    }

    /* The server handles one request at a time. */
    static char buf[4096];
    size_t left = req->content_len;
    while (left > 0) {
        const int n = httpd_req_recv(req, buf, left < sizeof(buf) ? left : sizeof(buf));
        if (n <= 0) {
            ESP_LOGW(TAG, "Upload broke off with %u bytes to go", (unsigned)left);
            esp_ota_abort(ota);
            return ESP_FAIL;
        }
        err = esp_ota_write(ota, buf, n);
        if (err != ESP_OK) {
            esp_ota_abort(ota);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, esp_err_to_name(err));
        }
        left -= n;
    }

    /* Checks the image: chip, header and checksum. */
    err = esp_ota_end(ota);
    if (err == ESP_OK) {
        err = esp_ota_set_boot_partition(slot);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Update rejected: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Update written to %s, restarting", slot->label);
    httpd_resp_sendstr(req, "Update written, restarting\n");
    vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
    esp_restart();
    return ESP_OK;
}

static void server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.core_id = NET_CORE;
    config.stack_size = 8192;
    httpd_handle_t server;
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    static const httpd_uri_t status = {.uri = "/", .method = HTTP_GET, .handler = get_status};
    static const httpd_uri_t update = {.uri = "/ota", .method = HTTP_POST, .handler = post_ota};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &status));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &update));
}

/* A new image stays only once it is on the network and the main loop runs;
 * otherwise go back to the previous one, which could take updates. */
static void verify_image(void)
{
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) != ESP_OK ||
        state != ESP_OTA_IMG_PENDING_VERIFY) {
        return;
    }
    ESP_LOGI(TAG, "New firmware: waiting for Wi-Fi and the main loop before keeping it");
    const EventBits_t need = APP_OK | (wifi_enabled() ? GOT_IP : 0);
    const EventBits_t got =
        xEventGroupWaitBits(s_events, need, pdFALSE, pdTRUE, pdMS_TO_TICKS(VERIFY_TIMEOUT_MS));
    if ((got & need) == need) {
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
        ESP_LOGI(TAG, "New firmware kept");
    } else {
        ESP_LOGE(TAG, "New firmware failed its check, going back to the previous one");
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

static void net_task(void *arg)
{
    if (wifi_enabled()) {
        wifi_init();
        mdns_init_hat();
        server_start();
    } else {
        ESP_LOGW(TAG, "No Wi-Fi SSID set: no updates over Wi-Fi");
    }
    verify_image();
    vTaskDelete(NULL);
}

void ota_start(void)
{
    s_events = xEventGroupCreate();
    xTaskCreatePinnedToCore(net_task, "net", 4096, NULL, 4, NULL, NET_CORE);
}

void ota_app_ok(void)
{
    xEventGroupSetBits(s_events, APP_OK);
}
