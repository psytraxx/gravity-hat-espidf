#include "mic.h"

#include <math.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "mic";

#define SAMPLE_RATE_HZ 16000
/* 20 ms of stereo frames. */
#define CHUNK_FRAMES (SAMPLE_RATE_HZ / 50)

static i2s_chan_handle_t s_rx;
static int32_t s_buf[CHUNK_FRAMES * 2];
/* Latest RMS, and whether the microphone sends anything. */
static volatile float s_rms;
static volatile bool s_present;

/* RMS of one slot (0 left, 1 right) of the chunk, DC removed, full scale
 * 1.0. Sets *any if a sample is not zero. */
static float slot_rms(const int32_t *buf, int frames, int slot, bool *any)
{
    double sum = 0.0, sum2 = 0.0;
    for (int i = 0; i < frames; i++) {
        const int32_t s = buf[2 * i + slot];
        if (s != 0) {
            *any = true;
        }
        /* 24-bit samples, left-aligned in 32 bits. */
        const double x = (double)(s >> 8) / 8388608.0;
        sum += x;
        sum2 += x * x;
    }
    const double mean = sum / frames;
    const double var = sum2 / frames - mean * mean;
    return var > 0.0 ? (float)sqrt(var) : 0.0f;
}

static void mic_task(void *arg)
{
    for (;;) {
        size_t bytes = 0;
        if (i2s_channel_read(s_rx, s_buf, sizeof s_buf, &bytes, portMAX_DELAY) != ESP_OK) {
            continue;
        }
        const int frames = (int)(bytes / (2 * sizeof s_buf[0]));
        if (frames == 0) {
            continue;
        }
        bool any = false;
        const float left = slot_rms(s_buf, frames, 0, &any);
        const float right = slot_rms(s_buf, frames, 1, &any);
        s_rms = fmaxf(left, right);
        s_present = any;
    }
}

esp_err_t mic_start(void)
{
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan, NULL, &s_rx), TAG, "channel");
    const i2s_std_config_t std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE_HZ),
        .slot_cfg =
            I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = CONFIG_HAT_MIC_SCK,
                .ws = CONFIG_HAT_MIC_WS,
                .dout = I2S_GPIO_UNUSED,
                .din = CONFIG_HAT_MIC_SD,
            },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx, &std), TAG, "std mode");
    /* Without a microphone the data line reads zeros, not noise. */
    gpio_pulldown_en(CONFIG_HAT_MIC_SD);
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_rx), TAG, "enable");
    xTaskCreatePinnedToCore(mic_task, "mic", 3072, NULL, 3, NULL, 1);
    ESP_LOGI(TAG, "I2S microphone: SD %d, SCK %d, WS %d, %d Hz", CONFIG_HAT_MIC_SD,
             CONFIG_HAT_MIC_SCK, CONFIG_HAT_MIC_WS, SAMPLE_RATE_HZ);
    return ESP_OK;
}

bool mic_level(float *rms)
{
    *rms = s_rms;
    return s_present;
}
