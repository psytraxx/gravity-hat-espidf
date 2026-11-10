#include "rings.h"

#include <math.h>
#include <string.h>

#include "color.h"
#include "hat.h"

/* Half width of a ring in cells, at no and at full strength. */
#define RING_WIDTH 0.25f
/* A ring at the top of the hat keeps this share of its brightness. */
#define RING_TOP 0.3f

void rings_init(rings_t *r)
{
    memset(r, 0, sizeof *r);
    r->up = v3(0.0f, 0.0f, 1.0f);
    r->since = RINGS_GAP;
}

void rings_step(rings_t *r, vec3_t gravity, float pulse, float hue)
{
    const float g = v3_len(gravity);
    if (g > 1e-3f) {
        r->up = v3_scale(gravity, -1.0f / g);
    }
    /* The hat's lowest and highest LED in the room. */
    r->low = INFINITY;
    r->high = -INFINITY;
    for (int i = 0; i < LED_COUNT; i += 3) {
        const float h = v3_dot(hat_led_offset(i), r->up);
        r->low = fminf(r->low, h);
        r->high = fmaxf(r->high, h);
    }

    for (int k = 0; k < RINGS_MAX; k++) {
        if (r->strength[k] > 0.0f) {
            r->height[k] += RINGS_RISE * (1.0f + r->strength[k]);
            if (r->height[k] > r->high + 1.0f) {
                r->strength[k] = 0.0f;
            }
        }
    }
    r->since++;
    r->trough = fminf(r->trough, pulse);
    if (r->since >= RINGS_GAP && pulse - r->trough >= RINGS_JUMP) {
        r->since = 0;
        r->trough = pulse;
        /* The free slot, or else the highest ring. */
        int slot = 0;
        for (int k = 0; k < RINGS_MAX; k++) {
            if (r->strength[k] <= 0.0f) {
                slot = k;
                break;
            }
            if (r->height[k] > r->height[slot]) {
                slot = k;
            }
        }
        r->height[slot] = r->low - RING_WIDTH;
        r->colour[slot] = r->next;
        r->palette[r->next] = hue;
        r->next ^= 1;
        r->strength[slot] = fminf(1.0f, pulse);
    }
}

void rings_frame(const rings_t *r, frame_t *frame)
{
    frame_clear(frame);
    const float span = fmaxf(r->high - r->low, 1e-3f);
    for (int i = 0; i < LED_COUNT; i++) {
        const float h = v3_dot(hat_led_offset(i), r->up);
        float rgb[3] = {0.0f, 0.0f, 0.0f};
        for (int k = 0; k < RINGS_MAX; k++) {
            if (r->strength[k] <= 0.0f) {
                continue;
            }
            const float w = RING_WIDTH * (1.0f + r->strength[k]);
            const float d = (h - r->height[k]) / w;
            if (fabsf(d) > 3.0f) {
                continue;
            }
            const float climbed = clampf((r->height[k] - r->low) / span, 0.0f, 1.0f);
            const float a = expf(-d * d) * r->strength[k] * (1.0f - (1.0f - RING_TOP) * climbed);
            const float h = r->palette[r->colour[k]];
            const float hue = fmodf(fmodf(h, 360.0f) + 360.0f, 360.0f);
            const color_t c = color_from_hue((uint32_t)lrintf(hue));
            rgb[0] += a * c.r;
            rgb[1] += a * c.g;
            rgb[2] += a * c.b;
        }
        /* Overlapping rings add up to the fluid's power at most. */
        const float sum = rgb[0] + rgb[1] + rgb[2];
        const float k = sum > FLUID_LEVEL ? FLUID_LEVEL / sum : 1.0f;
        frame_set_led(frame, i,
                      (color_t){(uint8_t)lrintf(rgb[0] * k), (uint8_t)lrintf(rgb[1] * k),
                                (uint8_t)lrintf(rgb[2] * k)});
    }
}
