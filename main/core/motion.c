#include "motion.h"

void motion_filter_init(motion_filter_t *m, float smoothing, float gain)
{
    *m = (motion_filter_t){.smoothing = smoothing, .gain = gain};
}

vec3_t motion_filter_apply(motion_filter_t *m, vec3_t reading, vec3_t rotation)
{
    if (m->primed) {
        /* A vector fixed in the world turns the opposite way to the hat. */
        const vec3_t turned = v3_rotate(m->gravity, v3_scale(rotation, -1.0f));
        m->gravity = v3_add(turned, v3_scale(v3_sub(reading, turned), m->smoothing));
    } else {
        m->gravity = reading;
        m->primed = true;
    }
    return v3_add(m->gravity, v3_scale(v3_sub(reading, m->gravity), m->gain));
}
