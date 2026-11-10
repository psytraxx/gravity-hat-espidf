/* Thunderstorm: rain running down the hat's wall in the room, and
 * lightning on a loud bang.
 *
 * Rain drops slide along the wall the way gravity points there (as the
 * fire's heat rises the other way, fire.h) and leave a fading streak on a
 * grid wrapped round the wall, STORM_COLS round by STORM_ROWS up; a drop
 * that leaves the wall starts again at the edge the rain comes in from.
 * Tilt the hat and the rain slants across the wall, still falling down
 * the room.
 *
 * A bang is a sound more than STORM_BANG_DB louder than its slow average.
 * It strikes a jagged bolt from the edge the rain comes in from to the
 * other, with a branch now and then, and a bright flash over the whole
 * hat that is gone within about a tenth of a second; the bolt flickers
 * and fades within about a third of a second.
 * The random numbers come from a seeded generator, so a run repeats. */
#pragma once

#include <stdint.h>

#include "frame.h"
#include "vec3.h"

#define STORM_COLS  40
#define STORM_ROWS  28
#define STORM_DROPS 45
/* Rise over the slow average that counts as a bang, in dB, and the
 * steps before the next one can strike. */
#define STORM_BANG_DB    15.0f
#define STORM_BANG_STEPS 20

typedef struct {
    float rain[STORM_ROWS][STORM_COLS];
    float bolt[STORM_ROWS][STORM_COLS];
    float drop_col[STORM_DROPS], drop_row[STORM_DROPS];
    float bolt_life; /* 1 at a strike, down to 0 */
    float flash;
    float slow_db;
    int quiet_steps; /* steps since the last bang */
    int primed;
    uint32_t rng;
} storm_t;

/* `seed` must not be 0. */
void storm_init(storm_t *s, uint32_t seed);

/* Feed this step's sound level (dB, audio_db); true on a bang, which
 * strikes lightning on the next storm_step. */
int storm_hear(storm_t *s, float level_db);

/* Advance one step. `gravity` is the way gravity points (hat axes, any
 * length). */
void storm_step(storm_t *s, vec3_t gravity);

/* Draw the rain, blue, and the lightning, white; a double tap does not
 * change them. */
void storm_frame(const storm_t *s, frame_t *frame);
