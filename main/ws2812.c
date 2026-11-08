#include "ws2812.h"

#include <stdbool.h>
#include <string.h>

#include "esp_check.h"
#include "led_strip.h"

static const char *TAG = "ws2812";

/* Four of the ESP32's eight 64-symbol RMT blocks: the driver refills one
 * half while the other goes out, so the refill interrupt has ~160 us of
 * slack instead of ~40. */
#define RMT_MEM_SYMBOLS 256

static led_strip_handle_t s_strip;
static frame_t s_sent;
static bool s_have_sent;

esp_err_t ws2812_init(int gpio)
{
    const led_strip_config_t strip_cfg = {
        .strip_gpio_num = gpio,
        .max_leds = LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    const led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .mem_block_symbols = RMT_MEM_SYMBOLS,
    };
    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip), TAG, "strip");
    return led_strip_clear(s_strip);
}

esp_err_t ws2812_show(const frame_t *frame)
{
    if (s_have_sent && memcmp(frame->rgb, s_sent.rgb, sizeof s_sent.rgb) == 0) {
        return ESP_OK;
    }
    for (int i = 0; i < LED_COUNT; i++) {
        const uint8_t *px = &frame->rgb[i * 3];
        ESP_RETURN_ON_ERROR(led_strip_set_pixel(s_strip, i, px[0], px[1], px[2]), TAG, "pixel");
    }
    /* Blocks until the frame and its reset code are out. */
    ESP_RETURN_ON_ERROR(led_strip_refresh(s_strip), TAG, "refresh");
    s_sent = *frame;
    s_have_sent = true;
    return ESP_OK;
}
