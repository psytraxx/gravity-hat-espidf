#include "tap.h"

void tap_init(tap_detector_t *t, float threshold_g)
{
    *t = (tap_detector_t){
        .threshold_g = threshold_g,
        .last_jump_ms = INT64_MIN / 2,
        .shock_until_ms = INT64_MIN / 2,
    };
}

bool tap_feed(tap_detector_t *t, vec3_t accel, int64_t now_ms)
{
    const vec3_t jump = v3_sub(accel, t->last);
    const bool primed = t->primed;
    t->last = accel;
    t->primed = true;
    if (!primed || v3_dot(jump, jump) <= t->threshold_g * t->threshold_g) {
        return false;
    }

    const int64_t previous_jump = t->last_jump_ms;
    t->last_jump_ms = now_ms;
    if (now_ms < t->shock_until_ms) {
        return false; /* still the same tap */
    }
    t->shock_until_ms = now_ms + TAP_SHOCK_MS;

    if (now_ms - previous_jump < TAP_QUIET_MS) {
        /* No quiet since the last jump: shaking, not tapping. */
        t->have_first = false;
        return false;
    }
    if (t->have_first && now_ms - t->first_tap_ms <= TAP_WINDOW_MS) {
        t->have_first = false;
        return true;
    }
    t->first_tap_ms = now_ms;
    t->have_first = true;
    return false;
}
