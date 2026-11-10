/* Gravity hat: a fluid in a hat whose LED strip, wound round its crown in a
 * spiral, shows the liquid inside. Tilt it and the surface tilts; shake
 * it and the fluid sloshes; its brightness pulses with the sound. Held
 * upside down for two seconds, it moves on to the next mode: fire that
 * burns upwards in the room, stoked by the sound; the sound's spectrum
 * standing round the hat, fixed in the room as the hat turns; rings of
 * light rising level with the floor, one per beat; a night sky fixed in
 * the room; a thunderstorm whose lightning strikes on loud bangs; then
 * the fluid again.
 *
 * Core 0 (this task) reads the IMU, runs the physics and builds frames;
 * core 1 sends them to the LEDs. The RMT send takes ~15 ms per changed
 * frame, so the two overlap: core 0 computes the next frame while core 1
 * sends the current one, and only waits if core 1 is still busy. Wi-Fi
 * and firmware updates run on core 1 too (ota.c). */
#include <math.h>

#include "attitude.h"
#include "audio.h"
#include "hat.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "fire.h"
#include "fluid.h"
#include "heading.h"
#include "imu.h"
#include "mode.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mic.h"
#include "motion.h"
#include "mpu6500.h"
#include "ota.h"
#include "rings.h"
#include "sdkconfig.h"
#include "sky.h"
#include "spectrum.h"
#include "storm.h"
#include "tap.h"
#include "ws2812.h"

static const char *TAG = "hat";

/* Track gravity with the gyro and triple anything else, so shaking the
 * hat sloshes the fluid. */
#define MOTION_SMOOTHING 0.05f
#define MOTION_GAIN      3.0f

/* FIFO samples read per frame at most; the FIFO holds 42. */
#define MAX_SAMPLES 48

#define LOG_EVERY 100

/* Frames from core 0 to core 1. One slot. */
static QueueHandle_t s_frames;
/* Duration of the last LED send, for the status log. */
static volatile int64_t s_led_send_us;

static void led_task(void *arg)
{
    /* The RMT interrupt runs on the core that sets the channel up. */
    ESP_ERROR_CHECK(ws2812_init(CONFIG_HAT_LED_GPIO));
    ESP_LOGI(TAG, "%d LEDs on GPIO %d, sent from core %d", LED_COUNT, CONFIG_HAT_LED_GPIO,
             xPortGetCoreID());
    static frame_t frame;
    for (;;) {
        xQueueReceive(s_frames, &frame, portMAX_DELAY);
        const int64_t start = esp_timer_get_time();
        const esp_err_t err = ws2812_show(&frame);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "LED refresh failed: %s", esp_err_to_name(err));
        }
        s_led_send_us = esp_timer_get_time() - start;
    }
}

#if CONFIG_HAT_CALIBRATION
/* Show the calibration picture for good, the white band following the
 * IMU. The image counts as working once the picture is out, so updates
 * over Wi-Fi keep it. */
static void calibration(bool have_imu)
{
    ESP_LOGI(TAG, "Calibration: red +x (LED 0), green +y, blue -x, yellow -y; "
                  "white on the low side");
    static imu_sample_t samples[MAX_SAMPLES];
    static frame_t frame;
    imu_batch_t imu;
    imu_batch_init(&imu);
    uint32_t loops = 0;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        const int n = have_imu ? mpu6500_read(samples, MAX_SAMPLES) : 0;
        imu_batch(&imu, samples, n, MPU6500_SAMPLE_MS / 1000.0f);
        hat_calibration_frame(&frame, imu.accel);
        xQueueSend(s_frames, &frame, portMAX_DELAY);
        if (++loops == 1) {
            ota_app_ok();
        }
        if (loops % LOG_EVERY == 0) {
            ESP_LOGI(TAG, "Calibration: accel (%.2f, %.2f, %.2f) g, low side at %.0f deg",
                     imu.accel.x, imu.accel.y, imu.accel.z,
                     atan2f(imu.accel.y, imu.accel.x) * 57.2958f);
        }
        xTaskDelayUntil(&wake, pdMS_TO_TICKS(CONFIG_HAT_FRAME_MS));
    }
}
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "Gravity hat, %d LEDs in %.1f turns", LED_COUNT, HAT_TURNS);

    /* Joins Wi-Fi on core 1 while the rest starts up. */
    ota_start();

    s_frames = xQueueCreate(1, sizeof(frame_t));
    xTaskCreatePinnedToCore(led_task, "leds", 4096, NULL, 5, NULL, 1);

    /* Calibrates the gyro, so the hat should be still. */
    const bool have_imu = mpu6500_init() == ESP_OK;
    if (!have_imu) {
        ESP_LOGW(TAG, "No MPU-6500: check the wiring (README.md#wiring). "
                      "Running with gravity towards the floor, no spin or taps.");
    }

#if CONFIG_HAT_CALIBRATION
    calibration(have_imu);
#endif

#if CONFIG_HAT_MIC
    if (mic_start() != ESP_OK) {
        ESP_LOGW(TAG, "No I2S microphone: full brightness");
    }
#endif
    audio_t audio;
#if CONFIG_HAT_MIC
    audio_init(&audio, CONFIG_HAT_MIC_MIN_BRIGHTNESS / 100.0f);
#else
    audio_init(&audio, 1.0f);
#endif
    float sound_db = -100.0f;

    static fluid_t fluid;
    fluid_init(&fluid);
    fluid_populate(&fluid);
    ESP_LOGI(TAG, "%d fluid particles; double-tap the hat to change colour", fluid.n);
    uint32_t hue = FLUID_HUE;

    static fire_t fire;
    fire_init(&fire, esp_random() | 1u);
    spectrum_meter_t meter;
    spectrum_meter_init(&meter);
    heading_t heading;
    heading_init(&heading);
    attitude_t attitude;
    attitude_init(&attitude);
    static rings_t rings;
    rings_init(&rings);
    static sky_t sky;
    sky_init(&sky, esp_random() | 1u);
    static storm_t storm;
    storm_init(&storm, esp_random() | 1u);
    mode_switch_t modes;
    mode_switch_init(&modes, MODE_FLUID);
    ESP_LOGI(TAG, "Hold the hat upside down for %.0f s to change mode", MODE_HOLD_S);

    tap_detector_t taps;
    tap_init(&taps, CONFIG_HAT_TAP_THRESHOLD_MG / 1000.0f);
    motion_filter_t motion;
    motion_filter_init(&motion, MOTION_SMOOTHING, MOTION_GAIN);

    static imu_sample_t samples[MAX_SAMPLES];
    static frame_t frame;
    static hat_view_t view;
    hat_view_init(&view);
    /* No IMU (or no new sample): gravity towards the floor. */
    imu_batch_t imu;
    imu_batch_init(&imu);
    int64_t sample_ms = 0;
    uint32_t loops = 0;
    TickType_t wake = xTaskGetTickCount();

    for (;;) {
        /* Every sample since the last frame: each one goes through the tap
         * detector, the gyro is integrated over all of them, and their
         * mean is this frame's acceleration. */
        const int n = have_imu ? mpu6500_read(samples, MAX_SAMPLES) : 0;
        if (n < 0) {
            ESP_LOGW(TAG, "IMU read failed");
        }
        for (int i = 0; i < n; i++) {
            sample_ms += MPU6500_SAMPLE_MS;
            if (tap_feed(&taps, samples[i].accel, sample_ms)) {
                /* At least 60 degrees from the last hue, so the change shows. */
                hue = (hue + 60 + esp_random() % 241) % 360;
                ESP_LOGI(TAG, "Double tap: hue %lu", (unsigned long)hue);
            }
        }
        imu_batch(&imu, samples, n, MPU6500_SAMPLE_MS / 1000.0f);

        if (mode_switch_feed(&modes, imu.accel, CONFIG_HAT_FRAME_MS / 1000.0f)) {
            ESP_LOGI(TAG, "Mode: %s", mode_name(modes.mode));
        }

        /* The latest 20 ms of sound sets the brightness; without a
         * microphone sending, it stays full. */
        float rms = 0.0f;
        bool have_sound = false;
#if CONFIG_HAT_MIC
        have_sound = mic_level(&rms);
#endif
        float brightness = 1.0f;
        float pulse = 0.0f;
        if (have_sound) {
            sound_db = audio_db(rms);
            pulse = audio_feed(&audio, sound_db, CONFIG_HAT_FRAME_MS / 1000.0f);
            brightness = audio_brightness(&audio);
        }
#if CONFIG_HAT_MIC
        /* The spectrum shows in its own mode, colours the rings and
         * twinkles the stars. */
        {
            float bands[SPECTRUM_BANDS];
            if (mic_bands(bands)) {
                spectrum_meter_feed(&meter, bands, CONFIG_HAT_FRAME_MS / 1000.0f);
            }
        }
#endif
        if (have_sound && storm_hear(&storm, sound_db) && modes.mode == MODE_STORM) {
            ESP_LOGI(TAG, "Bang: lightning");
        }

        /* Only the mode on show runs; the fluid waits where it is while
         * the fire burns, stoked by the sound. */
        const int64_t start = esp_timer_get_time();
        const vec3_t accel = motion_filter_apply(&motion, imu.accel, imu.rotation);
        heading_feed(&heading, imu.rotation, motion.gravity);
        attitude_feed(&attitude, imu.rotation, motion.gravity);
        switch (modes.mode) {
        case MODE_FIRE:
            fire_step(&fire, accel, pulse);
            break;
        case MODE_SPECTRUM:
        case MODE_SKY:
            break;
        case MODE_RINGS:
            rings_step(&rings, motion.gravity, pulse, spectrum_band_hue(spectrum_loudest(&meter)));
            break;
        case MODE_STORM:
            storm_step(&storm, motion.gravity);
            break;
        default:
            fluid_set_rotation(&fluid, imu.rotation);
            fluid_step(&fluid, accel);
            break;
        }
        const int64_t physics_us = esp_timer_get_time() - start;

        switch (modes.mode) {
        case MODE_FIRE:
            fire_frame(&fire, &frame);
            break;
        case MODE_SPECTRUM:
            spectrum_frame(&meter, heading.yaw, &frame);
            break;
        case MODE_RINGS:
            rings_frame(&rings, &frame);
            break;
        case MODE_SKY:
            sky_frame(&sky, &attitude, v3_len(imu.rotation), spectrum_treble(&meter), &frame);
            break;
        case MODE_STORM:
            storm_frame(&storm, &frame);
            break;
        default:
            hat_view_frame(&view, &fluid, hue, brightness, &frame);
            break;
        }
        hat_progress_overlay(&frame, mode_switch_progress(&modes));
        xQueueSend(s_frames, &frame, portMAX_DELAY);
        const int64_t frame_us = esp_timer_get_time() - start;

        if (++loops % LOG_EVERY == 0) {
            ESP_LOGI(
                TAG,
                "Loop %lu, %s: accel (%.2f, %.2f, %.2f) |%.2f| g, gyro (%.2f, %.2f, %.2f) rad/s, "
                "sound %.0f dB%s, brightness %.0f %%, physics %lld us, frame %lld us, "
                "LED send %lld us",
                (unsigned long)loops, mode_name(modes.mode), imu.accel.x, imu.accel.y, imu.accel.z,
                v3_len(imu.accel), imu.rate.x, imu.rate.y, imu.rate.z, sound_db,
                have_sound ? "" : " (no microphone)", 100.0f * brightness, physics_us, frame_us,
                s_led_send_us);
        }
        if (loops == LOG_EVERY) {
            ota_app_ok();
        }

        /* Fixed step rate: the physics constants are per step. If a frame
         * ran over, still give the idle task a tick. */
        if (xTaskDelayUntil(&wake, pdMS_TO_TICKS(CONFIG_HAT_FRAME_MS)) == pdFALSE) {
            vTaskDelay(1);
            wake = xTaskGetTickCount();
        }
    }
}
