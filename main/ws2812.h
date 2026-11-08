/* The hat's WS2812B strip, through Espressif's led_strip component on
 * one RMT channel. */
#pragma once

#include "esp_err.h"
#include "hat.h"

/* Set up the RMT channel. Its interrupt runs on the calling core. */
esp_err_t ws2812_init(int gpio);

/* Send a frame and wait until it is out (about 15 ms for 500 LEDs).
 * An unchanged frame is skipped, so a settled hat keeps the data line
 * quiet. */
esp_err_t ws2812_show(const frame_t *frame);
