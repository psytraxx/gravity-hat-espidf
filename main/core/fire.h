/* Fire: flames on the hat's wall that always burn upwards in the room.
 *
 * Heat lives on a grid wrapped round the hat's wall, FIRE_COLS round by
 * FIRE_ROWS up. Each step the heat is carried along the wall the way up
 * runs there: the room's up (against the acceleration the hat feels),
 * projected onto the wall. Upright, flames climb from the brim to the
 * crown; tilted, they lean round towards the high side; upside down, they
 * climb from the crown to the brim. Hat on its side, they rise round both
 * ways from the low side.
 *
 * Fuel burns where the flow comes in: past the edge of the wall that up
 * points away from, and along the wall's lowest part in the room. Its
 * flicker is random per column and smoothed over a few steps, and the
 * heat cools by a share and a random amount each step, so flames lick up
 * and break off. Louder sound stokes the fire: more fuel, slower cooling.
 *
 * Units are cells (as the fluid's) and steps; the random numbers come
 * from a seeded generator, so a run repeats. */
#pragma once

#include <stdint.h>

#include "frame.h"
#include "vec3.h"

#define FIRE_COLS 40
#define FIRE_ROWS 28

/* Speed of the hot gas up the room, in cells per step: about 0.27 m/s
 * at an 18 ms step. */
#define FIRE_RISE 0.35f

typedef struct {
    float heat[FIRE_ROWS][FIRE_COLS];
    float next[FIRE_ROWS][FIRE_COLS];
    /* Fuel's flicker per column, 0 to 1. */
    float fuel[FIRE_COLS];
    /* Cosine and sine of each column's angle, and each row's radius. */
    float cos_a[FIRE_COLS], sin_a[FIRE_COLS];
    float radius[FIRE_ROWS];
    /* The room's up in hat axes: against the acceleration, smoothed, and
     * as a unit vector. */
    vec3_t lift;
    vec3_t up;
    uint32_t rng;
} fire_t;

/* Cold, with up along the hat's axis. `seed` must not be 0. */
void fire_init(fire_t *f, uint32_t seed);

/* Advance one step. `accel` is gravity plus motion in g, in hat axes,
 * pointing the way the fluid falls; `stoke` (0 to 1) is how loud it is. */
void fire_step(fire_t *f, vec3_t accel, float stoke);

/* Heat (0 to 1) at angle `a` (radians, counter-clockwise from +x seen from
 * above) and height `z` (cells), blended from the nearest grid points. */
float fire_heat_at(const fire_t *f, float a, float z);

/* Colour of heat `h` (0 to 1): dark, deep red, orange, yellow. Its
 * channels never add up to more than FLUID_LEVEL, the fluid's power. */
color_t fire_color(float h);

/* Light every LED with the heat behind it. */
void fire_frame(const fire_t *f, frame_t *frame);
