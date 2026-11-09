/* Which way the hat faces in the room: the angle it has turned about the
 * room's up since it started, from the gyro.
 *
 * Each step adds the part of the hat's rotation about the room's up, so
 * tilting or nodding does not turn it, only turning round does. It is
 * the gyro added up with nothing to correct it, so it drifts by as much
 * as the gyro's zero offset is off: a fraction of a degree a second
 * after the start-up calibration. */
#pragma once

#include "vec3.h"

typedef struct {
    float yaw; /* radians, counter-clockwise seen from above, in [0, 2 pi) */
} heading_t;

void heading_init(heading_t *h);

/* Feed the rotation since the last step (rotation vector, hat axes) and
 * the way gravity points (hat axes, any length). */
void heading_feed(heading_t *h, vec3_t rotation, vec3_t gravity);
