#include "sky.h"

#include <math.h>
#include <string.h>

#include "hat.h"

#define PI 3.14159265f

/* Angular size of a star and of the moon (radians), and how far round
 * they reach at all. */
#define STAR_SIZE  0.05f
#define STAR_REACH 0.15f
#define MOON_SIZE  0.22f
/* Colours at full, each adding up to the fluid's power or less: a star a
 * little blue, the moon a little yellow, a shooting star gold. */
static const float STAR_RGB[3] = {18.0f, 18.0f, 24.0f};
static const float MOON_RGB[3] = {22.0f, 20.0f, 12.0f};
static const float SHOOT_RGB[3] = {32.0f, 24.0f, 4.0f};
/* Afterglow kept per step when still, and when turning faster than
 * SKY_FAST radians a step (about 1.7 turns a second at 18 ms). */
#define GLOW_STILL 0.3f
#define GLOW_FAST  0.82f
#define SKY_FAST   0.18f
/* Chance per step of a shooting star, its speed (radians a step) and
 * length. */
#define SHOOT_CHANCE 0.012f
#define SHOOT_SPEED  0.06f
#define SHOOT_TAIL   0.25f
#define SHOOT_ARC    1.2f

/* Uniform in [0, 1). xorshift32. */
static float rnd(sky_t *s)
{
    uint32_t x = s->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->rng = x;
    return (float)(x >> 8) / 16777216.0f;
}

/* A direction uniform over the sphere. */
static vec3_t rnd_dir(sky_t *s)
{
    const float z = 2.0f * rnd(s) - 1.0f;
    const float a = 2.0f * PI * rnd(s);
    const float r = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    return v3(r * cosf(a), r * sinf(a), z);
}

void sky_init(sky_t *s, uint32_t seed)
{
    memset(s, 0, sizeof *s);
    s->rng = seed ? seed : 1u;
    for (int i = 0; i < SKY_STARS; i++) {
        s->star[i] = rnd_dir(s);
        const float m = 0.35f + 0.65f * rnd(s);
        s->star_level[i] = m * m;
    }
    const vec3_t moon = v3(0.8f, 0.3f, 0.35f);
    s->moon = v3_scale(moon, 1.0f / v3_len(moon));
}

/* Brightness of a point `size` wide at angle whose cosine is `c`. */
static float spot(float c, float size)
{
    /* Angle squared, from the cosine: 2 (1 - cos) for small angles. */
    const float a2 = 2.0f * (1.0f - c);
    return expf(-a2 / (size * size));
}

void sky_frame(sky_t *s, const attitude_t *a, float turn, float treble, frame_t *frame)
{
    const float keep = GLOW_STILL + (GLOW_FAST - GLOW_STILL) * clampf(fabsf(turn) / SKY_FAST, 0, 1);
    const float reach = cosf(STAR_REACH);
    treble = clampf(treble, 0.0f, 1.0f);

    if (!s->shooting && rnd(s) < SHOOT_CHANCE) {
        s->shoot_from = rnd_dir(s);
        const vec3_t axis = v3_cross(s->shoot_from, rnd_dir(s));
        const float n = v3_len(axis);
        if (n > 1e-3f) {
            s->shoot_axis = v3_scale(axis, 1.0f / n);
            s->shoot_at = 0.0f;
            s->shooting = 1;
        }
    }
    vec3_t head = {0, 0, 0}, tail = {0, 0, 0};
    if (s->shooting) {
        head = v3_rotate(s->shoot_from, v3_scale(s->shoot_axis, s->shoot_at));
        tail = v3_rotate(s->shoot_from, v3_scale(s->shoot_axis, s->shoot_at - SHOOT_TAIL));
    }
    /* This step's twinkle per star. */
    float level[SKY_STARS];
    for (int k = 0; k < SKY_STARS; k++) {
        level[k] = s->star_level[k] * (1.0f + treble * (1.6f * rnd(s) - 0.8f));
    }

    frame_clear(frame);
    for (int i = 0; i < LED_COUNT; i++) {
        vec3_t d = hat_led_offset(i);
        d = v3_scale(d, 1.0f / v3_len(d));
        const vec3_t w = attitude_to_room(a, d);
        float b = 0.0f;
        for (int k = 0; k < SKY_STARS; k++) {
            const float c = v3_dot(w, s->star[k]);
            if (c > reach) {
                b += level[k] * spot(c, STAR_SIZE);
            }
        }
        const float moon = spot(v3_dot(w, s->moon), MOON_SIZE);
        float shoot = 0.0f;
        if (s->shooting) {
            /* Along the trail from tail to head, brightest at the head. */
            for (int t = 0; t <= 4; t++) {
                const vec3_t p = v3_add(v3_scale(head, 1.0f - t / 4.0f), v3_scale(tail, t / 4.0f));
                const float c = v3_dot(w, p) / v3_len(p);
                if (c > reach) {
                    shoot += (1.0f - t / 5.0f) * spot(c, STAR_SIZE * 1.4f);
                }
            }
        }
        float sum = 0.0f;
        for (int ch = 0; ch < 3; ch++) {
            const float now = b * STAR_RGB[ch] + moon * MOON_RGB[ch] + shoot * SHOOT_RGB[ch];
            s->glow[i][ch] = fmaxf(now, s->glow[i][ch] * keep);
            sum += s->glow[i][ch];
        }
        /* No more than the fluid's power where lights overlap. */
        const float k = sum > FLUID_LEVEL ? FLUID_LEVEL / sum : 1.0f;
        frame_set_led(frame, i,
                      (color_t){(uint8_t)lrintf(s->glow[i][0] * k),
                                (uint8_t)lrintf(s->glow[i][1] * k),
                                (uint8_t)lrintf(s->glow[i][2] * k)});
    }
    if (s->shooting) {
        s->shoot_at += SHOOT_SPEED;
        if (s->shoot_at > SHOOT_ARC) {
            s->shooting = 0;
        }
    }
}
