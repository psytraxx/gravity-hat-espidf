#include "mode.h"

void mode_switch_init(mode_switch_t *m, hat_mode_t mode)
{
    *m = (mode_switch_t){.mode = mode, .armed = true};
}

bool mode_switch_feed(mode_switch_t *m, vec3_t accel, float dt)
{
    if (accel.z < -MODE_UPRIGHT_G) {
        m->armed = true;
    }
    if (!m->armed || accel.z < MODE_UPSIDE_DOWN_G) {
        m->held_s = 0.0f;
        return false;
    }
    m->held_s += dt;
    if (m->held_s < MODE_HOLD_S) {
        return false;
    }
    m->mode = (hat_mode_t)((m->mode + 1) % MODE_COUNT);
    m->held_s = 0.0f;
    m->armed = false;
    return true;
}

float mode_switch_progress(const mode_switch_t *m)
{
    return clampf(m->held_s / MODE_HOLD_S, 0.0f, 1.0f);
}

const char *mode_name(hat_mode_t mode)
{
    switch (mode) {
    case MODE_FLUID:
        return "fluid";
    case MODE_FIRE:
        return "fire";
    case MODE_SPECTRUM:
        return "spectrum";
    default:
        return "?";
    }
}
