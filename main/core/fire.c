#include "fire.h"

#include <math.h>
#include <string.h>

#include "hat.h"
#include "shape.h"

#define TWO_PI 6.2831853f

/* Height of a row, in cells; row r's centre is at (r + 0.5) * ROW_H. */
#define ROW_H (HAT_HEIGHT / FIRE_ROWS)

/* Share of the heat left after a step, and the most a step's random
 * cooling takes away; both at no sound. */
#define FIRE_KEEP 0.95f
#define FIRE_COOL 0.09f
/* Full sound keeps this much more heat a step and burns this much more
 * fuel. */
#define FIRE_STOKE_KEEP 0.05f
#define FIRE_STOKE_FUEL 0.35f
/* Share of a column's fuel flicker replaced by a new random value each
 * step. */
#define FIRE_FLICKER 0.35f
/* Fuel burns this far (cells) above the wall's lowest point in the
 * room. */
#define FIRE_LOW_BAND 1.0f
/* Share of the heat spread to the columns either side each step. */
#define FIRE_SPREAD 0.1f
/* Share of a new reading of up folded in each step. */
#define FIRE_UP_SMOOTHING 0.25f
/* Heat below this stays dark. */
#define FIRE_DARK 0.06f

/* Uniform in [0, 1). xorshift32. */
static float rnd(fire_t *f)
{
    uint32_t x = f->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    f->rng = x;
    return (float)(x >> 8) / 16777216.0f;
}

void fire_init(fire_t *f, uint32_t seed)
{
    memset(f, 0, sizeof *f);
    f->lift = v3(0.0f, 0.0f, 1.0f);
    f->up = f->lift;
    f->rng = seed ? seed : 1u;
    for (int c = 0; c < FIRE_COLS; c++) {
        f->fuel[c] = rnd(f);
        f->cos_a[c] = cosf(TWO_PI * c / FIRE_COLS);
        f->sin_a[c] = sinf(TWO_PI * c / FIRE_COLS);
    }
    for (int r = 0; r < FIRE_ROWS; r++) {
        f->radius[r] = hat_radius_at((r + 0.5f) * ROW_H);
    }
}

static int wrap_col(int c)
{
    return ((c % FIRE_COLS) + FIRE_COLS) % FIRE_COLS;
}

/* Fuel flicker at fractional column `x`. */
static float fuel_at(const fire_t *f, float x)
{
    const float x0 = floorf(x);
    const float t = x - x0;
    const int c = wrap_col((int)x0);
    return f->fuel[c] * (1.0f - t) + f->fuel[wrap_col(c + 1)] * t;
}

/* Heat at fractional column `x` and row `y`. Past the bottom or the top
 * row the fuel bed burns at `bed`. */
static float sample(const fire_t *f, float x, float y, float bed)
{
    if (y < -0.5f || y > FIRE_ROWS - 0.5f) {
        return bed * fuel_at(f, x);
    }
    const float x0 = floorf(x);
    const float tx = x - x0;
    const int c0 = wrap_col((int)x0);
    const int c1 = wrap_col(c0 + 1);
    const float yc = clampf(y, 0.0f, FIRE_ROWS - 1.0f);
    const int r0 = (int)fminf(floorf(yc), FIRE_ROWS - 2.0f);
    const float ty = yc - r0;
    const float lo = f->heat[r0][c0] * (1.0f - tx) + f->heat[r0][c1] * tx;
    const float hi = f->heat[r0 + 1][c0] * (1.0f - tx) + f->heat[r0 + 1][c1] * tx;
    return lo * (1.0f - ty) + hi * ty;
}

void fire_step(fire_t *f, vec3_t accel, float stoke)
{
    stoke = clampf(stoke, 0.0f, 1.0f);
    /* Smoothed before it is made a unit vector, so a flip turns it over
     * too. */
    f->lift = v3_add(f->lift, v3_scale(v3_sub(v3_scale(accel, -1.0f), f->lift), FIRE_UP_SMOOTHING));
    const float len = v3_len(f->lift);
    if (len > 1e-3f) {
        f->up = v3_scale(f->lift, 1.0f / len);
    }
    const vec3_t up = f->up;

    for (int c = 0; c < FIRE_COLS; c++) {
        /* Squared, so a few columns flare and the rest smoulder. */
        const float r = rnd(f);
        f->fuel[c] += (r * r * 1.4f - f->fuel[c]) * FIRE_FLICKER;
    }
    const float bed = 1.0f + FIRE_STOKE_FUEL * stoke;

    /* The wall's lowest point in the room: its height along up. */
    float lowest = INFINITY;
    for (int r = 0; r < FIRE_ROWS; r += FIRE_ROWS - 1) {
        const float z = (r + 0.5f) * ROW_H;
        for (int c = 0; c < FIRE_COLS; c++) {
            const float h = f->radius[r] * (up.x * f->cos_a[c] + up.y * f->sin_a[c]) + up.z * z;
            lowest = fminf(lowest, h);
        }
    }

    const float keep = FIRE_KEEP + FIRE_STOKE_KEEP * stoke;
    for (int r = 0; r < FIRE_ROWS; r++) {
        const float z = (r + 0.5f) * ROW_H;
        const float rad = f->radius[r];
        for (int c = 0; c < FIRE_COLS; c++) {
            const float ca = f->cos_a[c], sa = f->sin_a[c];
            /* Up along the wall: round the hat (towards larger angles) and
             * up its axis. Where the wall leans, up along it is shorter;
             * the gas still rises at the square root of that share of
             * FIRE_RISE, so flames get away from a wall that is nearly
             * level. */
            float round = -up.x * sa + up.y * ca;
            float rise = up.z;
            const float along = sqrtf(round * round + rise * rise);
            const float speed = along > 1e-3f ? FIRE_RISE / sqrtf(along) : 0.0f;
            round *= speed;
            rise *= speed;
            /* Where this point's gas was a step ago. */
            const float x = c - round / (TWO_PI * rad) * FIRE_COLS;
            const float y = r - rise / ROW_H;
            float h = sample(f, x, y, bed);

            const float height = rad * (up.x * ca + up.y * sa) + up.z * z - lowest;
            if (height < FIRE_LOW_BAND) {
                h = fmaxf(h, bed * f->fuel[c] * (1.0f - height / FIRE_LOW_BAND));
            }
            h = h * keep - FIRE_COOL * rnd(f);
            f->next[r][c] = h > 0.0f ? h : 0.0f;
        }
    }
    for (int r = 0; r < FIRE_ROWS; r++) {
        for (int c = 0; c < FIRE_COLS; c++) {
            const float side = f->next[r][wrap_col(c - 1)] + f->next[r][wrap_col(c + 1)];
            f->heat[r][c] =
                fminf(1.0f, f->next[r][c] * (1.0f - FIRE_SPREAD) + side * 0.5f * FIRE_SPREAD);
        }
    }
}

float fire_heat_at(const fire_t *f, float a, float z)
{
    return sample(f, a / TWO_PI * FIRE_COLS, z / ROW_H - 0.5f, 0.0f);
}

color_t fire_color(float h)
{
    h = clampf(h, 0.0f, 1.0f);
    if (h < FIRE_DARK) {
        return (color_t){0, 0, 0};
    }
    /* Red rises to full power by half heat; past it, green takes over a
     * share of the same power, from red to orange to yellow. */
    const float level = FLUID_LEVEL;
    if (h < 0.5f) {
        const float t = (h - FIRE_DARK) / (0.5f - FIRE_DARK);
        return (color_t){(uint8_t)(level * (0.08f + 0.92f * t * t) + 0.5f), 0, 0};
    }
    const uint8_t g = (uint8_t)(level * 0.45f * (h - 0.5f) / 0.5f + 0.5f);
    return (color_t){(uint8_t)(FLUID_LEVEL - g), g, 0};
}

void fire_frame(const fire_t *f, frame_t *frame)
{
    frame_clear(frame);
    for (int i = 0; i < LED_COUNT; i++) {
        frame_set_led(frame, i, fire_color(fire_heat_at(f, hat_led_angle(i), hat_led_height(i))));
    }
}
