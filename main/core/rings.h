/* Sonar rings: each beat sends a ring of light up the hat, level with
 * the floor.
 *
 * A beat is the sound's pulse (audio.h) jumping RINGS_JUMP or more above
 * its lowest point since the last beat, at most one every RINGS_GAP
 * steps: loud music never lets the pulse fall far, but each beat still
 * jumps out of it. It starts a ring at the hat's lowest
 * point in the room, in the colour of the loudest spectrum band at that
 * moment; the ring rises through the room, faster and thicker the louder
 * the beat, fading as it climbs, and leaves at the top. Each LED lights
 * by how close its height in the room is to each ring, so the rings stay
 * level however the hat is tilted. */
#pragma once

#include <stdint.h>

#include "frame.h"
#include "vec3.h"

#define RINGS_MAX 8
/* Jump of the pulse that makes a beat, and the steps from one beat to
 * the next at least (about 0.15 s at 18 ms). */
#define RINGS_JUMP 0.25f
#define RINGS_GAP  8
/* Rise per step in cells, at no and at full strength. */
#define RINGS_RISE 0.12f

typedef struct {
    float height[RINGS_MAX]; /* in the room, cells from the hat's middle */
    float hue[RINGS_MAX];
    float strength[RINGS_MAX]; /* 0: no ring */
    float low, high;           /* the hat's lowest and highest point in the room */
    vec3_t up;                 /* the room's up in hat axes, unit */
    float trough;              /* the pulse's lowest point since the last beat */
    int since;                 /* steps since the last beat */
} rings_t;

void rings_init(rings_t *r);

/* Advance one step. `gravity` is the way gravity points (hat axes, any
 * length), `pulse` the sound's pulse (0 to 1) and `hue` the loudest
 * band's hue in degrees, any number of turns either way. */
void rings_step(rings_t *r, vec3_t gravity, float pulse, float hue);

void rings_frame(const rings_t *r, frame_t *frame);
