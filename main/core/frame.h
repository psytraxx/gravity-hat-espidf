/* One frame: an RGB colour per LED, in strip order. */
#pragma once

#include <stdint.h>

#include "color.h"
#include "config.h"

typedef struct {
    uint8_t rgb[LED_COUNT * 3];
} frame_t;

void frame_clear(frame_t *f);

/* Set LED `led` (strip order); LEDs outside the strip are left alone. */
void frame_set_led(frame_t *f, int led, color_t color);
