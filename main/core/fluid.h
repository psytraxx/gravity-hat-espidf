/* Fluid: an incompressible FLIP fluid (inviscid Navier-Stokes) in the
 * hat, which stands in a grid of FLUID_NX x FLUID_NY x FLUID_NZ cells.
 * The hat is the cells whose centres lie within its radius at their
 * height (hat_radius_at) and below HAT_HEIGHT; its walls are their outer
 * faces, so it is round (and narrows, where the hat does) as far as a grid of
 * cells allows.
 *
 * Particles carry the fluid's velocity. Each step they are accelerated by
 * gravity and by the hat's rotation, move, and are pushed apart so they
 * stay evenly spread. Their velocity is then spread onto a staggered (MAC)
 * grid with one cell per voxel, where a Gauss-Seidel pressure solve makes
 * the flow divergence-free. The particles take back the change in grid
 * velocity (FLIP, blended with a little of the grid velocity itself to
 * damp noise).
 *
 * Units are cells and steps: positions run from 0.0 to the grid size per
 * axis, cell c spans [c, c + 1), velocities are cells per step. The hat's
 * axis runs up the middle of the grid. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "particle.h"

#define FLUID_CELLS (FLUID_NX * FLUID_NY * FLUID_NZ)
/* Faces carrying one velocity component: one more than the cells along
 * that component's axis. Sized for the axis with the most. */
#define FLUID_FACES_X  ((FLUID_NX + 1) * FLUID_NY * FLUID_NZ)
#define FLUID_FACES_Y  (FLUID_NX * (FLUID_NY + 1) * FLUID_NZ)
#define FLUID_FACES_Z  (FLUID_NX * FLUID_NY * (FLUID_NZ + 1))
#define FLUID_FACES_XY (FLUID_FACES_X > FLUID_FACES_Y ? FLUID_FACES_X : FLUID_FACES_Y)
#define FLUID_FACES    (FLUID_FACES_XY > FLUID_FACES_Z ? FLUID_FACES_XY : FLUID_FACES_Z)

/* Maximum velocity per axis, in cells per step: a particle never skips a
 * cell. */
#define FLUID_MAX_VELOCITY 1.0f

typedef struct {
    particle_t p[FLUID_PARTICLES];
    int n;
    /* Face velocities per axis after the pressure solve, before it, and
     * the particle weight behind each (0: no particle nearby). */
    float vel[3][FLUID_FACES];
    float prev[3][FLUID_FACES];
    float weight[3][FLUID_FACES];
    /* Particle density at each cell's centre, the density-weighted mean
     * tint there (speed included), and particles per cell. */
    float density[FLUID_CELLS];
    float tint[FLUID_CELLS];
    uint8_t count[FLUID_CELLS];
    /* Per cell: whether it lies in the hat, and which faces are open
     * (OPEN_* in fluid.c). Set by fluid_init. */
    uint8_t open[FLUID_CELLS];
    /* Per face: open, wall, or outside the hat (FACE_* in fluid.c). Set
     * by fluid_init. */
    uint8_t face[3][FLUID_FACES];
    /* Particles per cell as linked lists, for pushing them apart: first
     * particle per cell, next in the same cell; UINT16_MAX ends a list. */
    uint16_t head[FLUID_CELLS];
    uint16_t next[FLUID_PARTICLES];
    /* Rotation of the hat per step (axis times radians), in hat axes,
     * this step and the last. */
    vec3_t rotation;
    vec3_t last_rotation;
} fluid_t;

/* Empty the hat and set up its walls. */
void fluid_init(fluid_t *f);

/* Fill the bottom half of the hat's cells with particles at rest,
 * FLUID_PARTICLES_PER_CELL per cell. Tints run
 * across and up the fluid from -FLUID_TINT_SPREAD to +FLUID_TINT_SPREAD. */
void fluid_populate(fluid_t *f);

/* Advance one step. `accel` is gravity plus motion in g, in hat axes,
 * pointing the way the fluid falls. */
void fluid_step(fluid_t *f, vec3_t accel);

/* How far the hat turned since the last step, as a rotation vector in
 * hat axes. Applies to the following steps until set again. */
void fluid_set_rotation(fluid_t *f, vec3_t rotation);

/* Particle density at `pos`, blended from the nearest cell centres (rest
 * density is FLUID_PARTICLES_PER_CELL). If `tint` is not NULL, it gets
 * the hue offset in degrees there: the particles' tints, plus
 * FLUID_SPEED_TINT for their speed. */
float fluid_sample(const fluid_t *f, vec3_t pos, float *tint);
