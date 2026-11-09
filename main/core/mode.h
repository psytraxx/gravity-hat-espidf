/* What the hat shows, and the gesture that changes it: hold the hat
 * upside down for MODE_HOLD_S seconds and it moves on to the next mode.
 *
 * Upside down means gravity points more than MODE_UPSIDE_DOWN_G of a g
 * towards the hat's top, as when the hat is held crown down. The hold
 * restarts whenever the hat leaves that pose, and after a switch the hat
 * has to come back the right way up before the next one, so holding it
 * upside down for longer switches only once. */
#pragma once

#include <stdbool.h>

#include "vec3.h"

typedef enum {
    MODE_FLUID,
    MODE_FIRE,
    MODE_SPECTRUM,
    MODE_COUNT,
} hat_mode_t;

/* Seconds upside down that switch the mode. */
#define MODE_HOLD_S 2.0f
/* Share of a g towards the hat's top (z) that counts as upside down:
 * the hat within about 45 degrees of crown down. */
#define MODE_UPSIDE_DOWN_G 0.7f
/* Share of a g towards the floor that counts as the right way up again. */
#define MODE_UPRIGHT_G 0.3f

typedef struct {
    hat_mode_t mode;
    float held_s;
    bool armed;
} mode_switch_t;

void mode_switch_init(mode_switch_t *m, hat_mode_t mode);

/* Feed this frame's acceleration (in g, hat axes, the way the fluid falls)
 * and the time since the last frame; true when the mode has just
 * changed. */
bool mode_switch_feed(mode_switch_t *m, vec3_t accel, float dt);

/* How far through the hold the hat is, 0 to 1; 0 when not holding. */
float mode_switch_progress(const mode_switch_t *m);

/* The mode's name, for the log. */
const char *mode_name(hat_mode_t mode);
