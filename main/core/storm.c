#include "storm.h"

#include <math.h>
#include <string.h>

#include "hat.h"
#include "shape.h"

#define TWO_PI 6.2831853f
#define ROW_H  (HAT_HEIGHT / STORM_ROWS)

/* Speed of a drop down a vertical wall, in cells per step: about
 * 0.16 m/s at 18 ms. */
#define RAIN_SPEED 0.2f
/* Share of a streak left after a step. */
#define RAIN_KEEP 0.55f
/* Share of the slow average's gap to the level closed each step: about a
 * second's time constant at 18 ms. */
#define SLOW_SHARE 0.018f
/* The bolt's fade per step, and the share of the flash left after a step:
 * the flash lights the whole hat brightly and is gone within about a
 * tenth of a second. */
#define BOLT_FADE  0.09f
#define FLASH_KEEP 0.6f
/* Colours at full, each adding up to the fluid's power or less: blue
 * rain, a white bolt and flash. */
static const float RAIN_RGB[3] = {4.0f, 10.0f, 26.0f};
static const float BOLT_RGB[3] = {18.0f, 18.0f, 24.0f};
static const float FLASH_RGB[3] = {14.0f, 14.0f, 18.0f};

/* Uniform in [0, 1). xorshift32. */
static float rnd(storm_t *s)
{
    uint32_t x = s->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->rng = x;
    return (float)(x >> 8) / 16777216.0f;
}

static int wrap(int c)
{
    return ((c % STORM_COLS) + STORM_COLS) % STORM_COLS;
}

void storm_init(storm_t *s, uint32_t seed)
{
    memset(s, 0, sizeof *s);
    s->rng = seed ? seed : 1u;
    for (int i = 0; i < STORM_DROPS; i++) {
        s->drop_col[i] = rnd(s) * STORM_COLS;
        s->drop_row[i] = rnd(s) * STORM_ROWS;
    }
    s->quiet_steps = STORM_BANG_STEPS;
}

int storm_hear(storm_t *s, float level_db)
{
    if (!s->primed) {
        s->slow_db = level_db;
        s->primed = 1;
    }
    s->quiet_steps++;
    const int bang = level_db > s->slow_db + STORM_BANG_DB && s->quiet_steps >= STORM_BANG_STEPS;
    s->slow_db += (level_db - s->slow_db) * SLOW_SHARE;
    if (bang) {
        s->quiet_steps = 0;
        s->bolt_life = 1.0f + BOLT_FADE; /* lit from the next step */
        s->flash = 1.0f;
        memset(s->bolt, 0, sizeof s->bolt);
    }
    return bang;
}

/* Downhill along the wall at column `c`, in columns and rows per step. */
static void downhill(vec3_t g, float c, float *dc, float *dr)
{
    const float a = TWO_PI * c / STORM_COLS;
    const float round = -g.x * sinf(a) + g.y * cosf(a);
    *dc = round * RAIN_SPEED / (TWO_PI * hat_radius_at(0.5f * HAT_HEIGHT)) * STORM_COLS;
    *dr = g.z * RAIN_SPEED / ROW_H;
}

/* Add `v` at fractional column `c`, row `r` of `grid`, spread over the
 * four nearest points. */
static void splat(float grid[STORM_ROWS][STORM_COLS], float c, float r, float v)
{
    const float c0 = floorf(c), r0 = floorf(r);
    const float tc = c - c0, tr = r - r0;
    for (int k = 0; k < 4; k++) {
        const int rr = (int)r0 + (k >> 1), cc = wrap((int)c0 + (k & 1));
        if (rr < 0 || rr >= STORM_ROWS) {
            continue;
        }
        const float w = ((k & 1) ? tc : 1.0f - tc) * ((k >> 1) ? tr : 1.0f - tr);
        grid[rr][cc] = fminf(1.0f, grid[rr][cc] + v * w);
    }
}

/* Draw a jagged bolt into s->bolt, starting from column `c`, row `r` and
 * running downhill; branches go off for a few steps. */
static void strike(storm_t *s, vec3_t g, float c, float r, int steps)
{
    for (int k = 0; k < steps; k++) {
        splat(s->bolt, c, r, 1.0f);
        float dc, dr;
        downhill(g, c, &dc, &dr);
        const float n = sqrtf(dc * dc + dr * dr);
        if (n < 1e-3f) {
            return;
        }
        /* One row's step downhill, jagged sideways. */
        const float side = 1.2f * (rnd(s) - 0.5f);
        c += dc / n * 1.2f - dr / n * side;
        r += dr / n * 1.2f + dc / n * side;
        if (r < -0.5f || r > STORM_ROWS - 0.5f) {
            return;
        }
        if (steps > 6 && rnd(s) < 0.08f) {
            strike(s, g, c + (rnd(s) < 0.5f ? -1.0f : 1.0f), r, 5);
        }
    }
}

void storm_step(storm_t *s, vec3_t gravity)
{
    const float len = v3_len(gravity);
    const vec3_t g = len > 1e-3f ? v3_scale(gravity, 1.0f / len) : v3(0.0f, 0.0f, -1.0f);
    for (int r = 0; r < STORM_ROWS; r++) {
        for (int c = 0; c < STORM_COLS; c++) {
            s->rain[r][c] *= RAIN_KEEP;
        }
    }
    for (int i = 0; i < STORM_DROPS; i++) {
        float dc, dr;
        downhill(g, s->drop_col[i], &dc, &dr);
        s->drop_col[i] = fmodf(s->drop_col[i] + dc + STORM_COLS, STORM_COLS);
        s->drop_row[i] += dr;
        const int off = s->drop_row[i] < -0.5f || s->drop_row[i] > STORM_ROWS - 0.5f;
        if (off || dc * dc + dr * dr < 0.01f) {
            /* Again from the edge the rain comes in from; with the hat on
             * its side, anywhere. */
            s->drop_col[i] = rnd(s) * STORM_COLS;
            s->drop_row[i] = fabsf(g.z) < 0.3f ? rnd(s) * STORM_ROWS
                             : g.z < 0.0f      ? STORM_ROWS - 0.5f
                                               : -0.5f;
        }
        splat(s->rain, s->drop_col[i], s->drop_row[i], 1.0f);
    }
    if (s->bolt_life > 1.0f) {
        const float r = g.z < 0.0f ? STORM_ROWS - 0.5f : -0.5f;
        strike(s, g, rnd(s) * STORM_COLS, r, 2 * STORM_ROWS);
    }
    s->bolt_life = fmaxf(0.0f, fminf(s->bolt_life, 1.0f) - BOLT_FADE);
    s->flash *= FLASH_KEEP;
}

/* `grid` at fractional column `c`, row `r`. */
static float sample(const float grid[STORM_ROWS][STORM_COLS], float c, float r)
{
    r = clampf(r, 0.0f, STORM_ROWS - 1.0f);
    const float c0 = floorf(c);
    const int r0 = (int)fminf(floorf(r), STORM_ROWS - 2.0f);
    const float tc = c - c0, tr = r - r0;
    const int a = wrap((int)c0), b = wrap((int)c0 + 1);
    const float lo = grid[r0][a] * (1.0f - tc) + grid[r0][b] * tc;
    const float hi = grid[r0 + 1][a] * (1.0f - tc) + grid[r0 + 1][b] * tc;
    return lo * (1.0f - tr) + hi * tr;
}

void storm_frame(const storm_t *s, frame_t *frame)
{
    frame_clear(frame);
    /* The bolt flickers as it fades. */
    const float bolt = s->bolt_life * ((s->rng & 0x700) ? 1.0f : 0.3f);
    for (int i = 0; i < LED_COUNT; i++) {
        const float c = hat_led_angle(i) / TWO_PI * STORM_COLS;
        const float r = hat_led_height(i) / ROW_H - 0.5f;
        const float rain = sample(s->rain, c, r);
        const float b = bolt > 0.0f ? bolt * sample(s->bolt, c, r) : 0.0f;
        float rgb[3], sum = 0.0f;
        for (int ch = 0; ch < 3; ch++) {
            rgb[ch] = rain * RAIN_RGB[ch] + b * BOLT_RGB[ch] + s->flash * FLASH_RGB[ch];
            sum += rgb[ch];
        }
        const float k = sum > FLUID_LEVEL ? FLUID_LEVEL / sum : 1.0f;
        frame_set_led(frame, i,
                      (color_t){(uint8_t)lrintf(rgb[0] * k), (uint8_t)lrintf(rgb[1] * k),
                                (uint8_t)lrintf(rgb[2] * k)});
    }
}
