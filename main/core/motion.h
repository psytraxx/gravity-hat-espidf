/* Splits an accelerometer reading into slow gravity and fast motion, and
 * amplifies the motion so that shaking or jerking the hat visibly sloshes
 * the fluid.
 *
 * Gravity is tracked with a complementary filter: each reading the estimate
 * is first turned by the gyro's rotation (gravity is fixed in the world, so
 * a tilt moves it immediately), then nudged towards the accelerometer to
 * cancel gyro drift. Whatever the reading adds on top of it is the hat's
 * own acceleration (already pointing the way the fluid should lag), and is
 * scaled by `gain` before being added back.
 *
 * Without the gyro (a zero rotation) the estimate lags behind a tilt, and
 * the tilt itself is amplified as if it were a shake. */
#pragma once

#include <stdbool.h>

#include "vec3.h"

typedef struct {
    vec3_t gravity;
    bool primed;
    float smoothing; /* fraction of each reading folded into the estimate */
    float gain;      /* multiplier for motion on top of gravity */
} motion_filter_t;

void motion_filter_init(motion_filter_t *m, float smoothing, float gain);

/* Feed one reading and return gravity plus amplified motion. `rotation` is
 * the angle the hat turned since the last reading, as a rotation vector
 * (axis times radians) in the same axes as `reading`. */
vec3_t motion_filter_apply(motion_filter_t *m, vec3_t reading, vec3_t rotation);
