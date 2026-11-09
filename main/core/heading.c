#include "heading.h"

#define TWO_PI 6.2831853f

void heading_init(heading_t *h)
{
    h->yaw = 0.0f;
}

void heading_feed(heading_t *h, vec3_t rotation, vec3_t gravity)
{
    const float g = v3_len(gravity);
    if (g < 1e-3f) {
        return;
    }
    /* Up is against gravity. */
    const float turn = -v3_dot(rotation, gravity) / g;
    h->yaw = fmodf(h->yaw + turn, TWO_PI);
    if (h->yaw < 0.0f) {
        h->yaw += TWO_PI;
    }
}
