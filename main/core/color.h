/* Fluid colours: any hue at the same power. */
#pragma once

#include <stdint.h>

#include "config.h"

/* The colour at `hue` degrees (0 red, 120 green, 240 blue), blended
 * between two channels whose sum is always FLUID_LEVEL: every hue draws
 * the current of one channel at FLUID_LEVEL. */
color_t color_from_hue(uint32_t hue);

/* `c` at `brightness` (0 to 1) of its level, rounded. */
color_t color_scale(color_t c, float brightness);
