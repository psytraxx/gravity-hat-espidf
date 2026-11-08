/* The hat's shape, from the numbers in config.h: how many turns the
 * spiral makes, how high the hat is, and its radius at each height.
 * Heights and radii are in fluid cells. */
#pragma once

#include <math.h>

#include "config.h"

/* Turns of the spiral: the LEDs per turn change evenly from one end to
 * the other, so the mean of the two ends sets how many turns the strip
 * makes. */
#define HAT_TURNS           (2.0f * HAT_LED_COUNT / (HAT_LEDS_PER_TURN_BOTTOM + HAT_LEDS_PER_TURN_TOP))
#define HAT_ROW_PITCH_CELLS (HAT_ROW_PITCH / HAT_CELL_LEDS)
/* The hat reaches half a row beyond the first and the last turn. */
#define HAT_HEIGHT ((HAT_TURNS + 1.0f) * HAT_ROW_PITCH_CELLS)
/* Radius of the widest turn. */
#define HAT_RADIUS_MAX                                                                             \
    (fmaxf(HAT_LEDS_PER_TURN_BOTTOM, HAT_LEDS_PER_TURN_TOP) / (6.2831853f * HAT_CELL_LEDS))

/* Turns from the bottom of the spiral at height z, 0 to HAT_TURNS. */
static inline float hat_turns_at_height(float z)
{
    const float t = z / HAT_ROW_PITCH_CELLS - 0.5f;
    return t < 0.0f ? 0.0f : t > HAT_TURNS ? HAT_TURNS : t;
}

/* Radius of the hat at height z: its LEDs per turn there, round a circle
 * of cells. */
static inline float hat_radius_at(float z)
{
    const float n = HAT_LEDS_PER_TURN_BOTTOM + (HAT_LEDS_PER_TURN_TOP - HAT_LEDS_PER_TURN_BOTTOM) *
                                                   hat_turns_at_height(z) / HAT_TURNS;
    return n / (6.2831853f * HAT_CELL_LEDS);
}
