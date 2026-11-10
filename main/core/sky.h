/* Night sky: stars and the moon, fixed in the room.
 *
 * SKY_STARS stars and a moon sit at fixed directions round the hat (a
 * seeded generator places the stars). Each LED looks out from the hat's
 * middle through its own place on the wall; turned into the room by the
 * hat's attitude (attitude.h), that direction lights the LED as it nears
 * a star or the moon. So turning, tilting or nodding moves the sky round
 * the hat while it stays put in the room. Each LED keeps an afterglow
 * that fades slowly while the hat turns fast, so a quick spin smears the
 * stars into streaks. Treble in the sound makes the stars twinkle, and
 * now and then a shooting star crosses. */
#pragma once

#include <stdint.h>

#include "attitude.h"
#include "frame.h"

#define SKY_STARS 38

typedef struct {
    vec3_t star[SKY_STARS];
    float star_level[SKY_STARS]; /* 0 to 1 */
    vec3_t moon;
    float glow[LED_COUNT][3];
    /* Shooting star: start, axis it turns about, how far it has gone
     * (radians); `shooting` is 0 when there is none. */
    vec3_t shoot_from, shoot_axis;
    float shoot_at;
    int shooting;
    uint32_t rng;
} sky_t;

/* `seed` must not be 0. */
void sky_init(sky_t *s, uint32_t seed);

/* Draw the sky for the hat lying as `a` says. `turn` is the angle the hat
 * turned this step (radians), `treble` the treble's level (0 to 1). */
void sky_frame(sky_t *s, const attitude_t *a, float turn, float treble, frame_t *frame);
