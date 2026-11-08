/* Fluid particles. */
#pragma once

#include <stdint.h>

#include "config.h"
#include "vec3.h"

typedef struct {
    vec3_t pos; /* 0.0 .. grid size on each axis; cell c spans [c, c + 1) */
    vec3_t vel; /* cells per step */
    float tint; /* hue offset in degrees, carried along with the fluid */
} particle_t;
