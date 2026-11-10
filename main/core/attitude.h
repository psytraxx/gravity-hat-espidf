/* How the hat lies in the room: a rotation from hat axes to room axes,
 * kept as a unit quaternion.
 *
 * Each step the gyro's rotation turns it; then it is pulled a little
 * towards the measured gravity, so the room's up stays up however long
 * it runs. The heading round the room's up has nothing to correct it, so
 * it drifts by the gyro's leftover zero offset, as heading.h does. The
 * room's x axis is where the hat's x axis (LED 0's side) pointed, level,
 * when it started. */
#pragma once

#include <stdbool.h>

#include "vec3.h"

typedef struct {
    float w, x, y, z;
} quat_t;

typedef struct {
    quat_t q; /* hat to room */
    bool primed;
} attitude_t;

/* Share of the tilt error corrected each step. */
#define ATTITUDE_GRAVITY_GAIN 0.04f

void attitude_init(attitude_t *a);

/* Feed the rotation since the last step (rotation vector, hat axes) and
 * the way gravity points (hat axes, any length). */
void attitude_feed(attitude_t *a, vec3_t rotation, vec3_t gravity);

/* `v` in hat axes, turned into room axes. */
vec3_t attitude_to_room(const attitude_t *a, vec3_t v);
