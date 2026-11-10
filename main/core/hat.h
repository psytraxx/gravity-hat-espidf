/* The hat's LEDs: where each sits on the spiral, and frames that show the
 * fluid behind them.
 *
 * LED 0 sits on the +x side of the hat (angle 0), on the bottom turn, or
 * the top one with HAT_START_AT_TOP. The strip runs counter-clockwise
 * seen from above (from +x towards +y), or clockwise with HAT_CLOCKWISE,
 * and climbs (or drops) HAT_ROW_PITCH LED spacings per turn. A turn holds
 * HAT_LEDS_PER_TURN_BOTTOM LEDs at the bottom and HAT_LEDS_PER_TURN_TOP
 * at the top, changing evenly in between; each LED sits on the circle of
 * the hat's radius at its height. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "fluid.h"
#include "frame.h"
#include "shape.h"
#include "vec3.h"

/* Angle of LED `led` round the hat in radians, counter-clockwise from +x
 * seen from above, in [0, 2 pi). */
float hat_led_angle(int led);

/* Height of LED `led` above the floor, in cells. */
float hat_led_height(int led);

/* Where LED `led` sits on the hat's wall, in grid coordinates. */
vec3_t hat_led_pos(int led);

/* Where LED `led` sits relative to the middle of the hat (on its axis,
 * half way up), in cells. */
vec3_t hat_led_offset(int led);

/* The LED index, not rounded, `t` turns on from LED 0. */
float hat_led_at_turns(float t);

/* Which LEDs show fluid. Each LED lights once the fluid density just
 * inside the wall in front of it reaches a threshold and goes dark again
 * below a lower one, so a still surface does not flicker. */
typedef struct {
    bool lit[LED_COUNT];
} hat_view_t;

void hat_view_init(hat_view_t *v);

/* Light every LED with fluid behind it in `hue` plus the fluid's tint
 * there, at `brightness` (0 to 1); the rest stay dark. */
void hat_view_frame(hat_view_t *v, const fluid_t *f, uint32_t hue, float brightness,
                    frame_t *frame);

/* While the hat is held upside down to change mode: a white arc grows
 * round the bottom turn, the brim, (on top while the hat is upside down)
 * as `progress` runs from 0 to 1. LEDs past it are left alone. */
void hat_progress_overlay(frame_t *frame, float progress);

/* Calibration picture. Four coloured lines run up the hat, the LED
 * nearest each angle on every turn: red at LED 0's angle (+x), then green
 * (+y), blue (-x) and yellow (-y) a quarter turn apart, counter-clockwise
 * seen from above. Straight lines mean the LEDs per turn are right, the
 * colour order that the winding direction is. While the hat tilts, a
 * white band lights the side `accel` (in g, the way the fluid falls)
 * points to, under the lines. */
void hat_calibration_frame(frame_t *frame, vec3_t accel);
