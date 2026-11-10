#include <math.h>

#include "hat.h"
#include "rings.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static rings_t r;
static frame_t frame;
static const vec3_t DOWN = {0.0f, 0.0f, -1.0f};

static int light(int i)
{
    return frame.rgb[3 * i] + frame.rgb[3 * i + 1] + frame.rgb[3 * i + 2];
}

/* Light-weighted mean and spread of the lit LEDs' heights in the room
 * (along -gravity). */
static void lit_heights(vec3_t gravity, float *mean, float *spread)
{
    const vec3_t up = v3_scale(gravity, -1.0f / v3_len(gravity));
    float w = 0, s = 0, s2 = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        const float h = v3_dot(hat_led_offset(i), up);
        w += light(i);
        s += light(i) * h;
        s2 += light(i) * h * h;
    }
    *mean = w > 0 ? s / w : -100.0f;
    *spread = w > 0 ? sqrtf(fmaxf(0.0f, s2 / w - *mean * *mean)) : 0.0f;
}

static void test_quiet_is_dark(void)
{
    rings_init(&r);
    for (int i = 0; i < 50; i++) {
        rings_step(&r, DOWN, 0.1f, 0.0f);
    }
    rings_frame(&r, &frame);
    for (int i = 0; i < LED_COUNT; i++) {
        TEST_ASSERT_EQUAL_INT(0, light(i));
    }
}

/* A beat starts a ring at the brim that rises up the hat. */
static void test_beat_ring_rises(void)
{
    rings_init(&r);
    rings_step(&r, DOWN, 1.0f, 120.0f);
    float before, after, spread;
    for (int i = 0; i < 4; i++) {
        rings_step(&r, DOWN, 1.0f, 120.0f);
    }
    rings_frame(&r, &frame);
    lit_heights(DOWN, &before, &spread);
    for (int i = 0; i < 10; i++) {
        rings_step(&r, DOWN, 0.0f, 120.0f);
    }
    rings_frame(&r, &frame);
    lit_heights(DOWN, &after, &spread);
    TEST_ASSERT_LESS_THAN_FLOAT(-HAT_HEIGHT / 4, before);
    TEST_ASSERT_GREATER_THAN_FLOAT(before + 1.5f, after);
    /* Green, the hue it was given. */
    int green = 0, other = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        green += frame.rgb[3 * i + 1];
        other += frame.rgb[3 * i] + frame.rgb[3 * i + 2];
    }
    TEST_ASSERT_GREATER_THAN_INT(10 * other, green);
}

/* Tilted 40 degrees, the ring is level in the room: its LEDs all sit at
 * nearly one height along the room's up, though they span most of the
 * hat's height. */
static void test_ring_is_level(void)
{
    const vec3_t g = v3(sinf(0.7f), 0.0f, -cosf(0.7f));
    rings_init(&r);
    rings_step(&r, g, 1.0f, 0.0f);
    for (int i = 0; i < 12; i++) {
        rings_step(&r, g, 0.0f, 0.0f);
    }
    rings_frame(&r, &frame);
    float mean, spread;
    lit_heights(g, &mean, &spread);
    TEST_ASSERT_LESS_THAN_FLOAT(0.5f, spread);
    float zmin = 100, zmax = -100;
    for (int i = 0; i < LED_COUNT; i++) {
        if (light(i) > FLUID_LEVEL / 2) {
            zmin = fminf(zmin, hat_led_height(i));
            zmax = fmaxf(zmax, hat_led_height(i));
        }
    }
    TEST_ASSERT_GREATER_THAN_FLOAT(HAT_HEIGHT / 2, zmax - zmin);
}

/* A hue turned past a whole turn either way is the same colour. */
static void test_hue_wraps(void)
{
    int green[2];
    for (int k = 0; k < 2; k++) {
        rings_init(&r);
        rings_step(&r, DOWN, 1.0f, k ? 120.0f - 720.0f : 120.0f + 360.0f);
        for (int i = 0; i < 4; i++) {
            rings_step(&r, DOWN, 1.0f, 0.0f);
        }
        rings_frame(&r, &frame);
        green[k] = 0;
        for (int i = 0; i < LED_COUNT; i++) {
            green[k] += frame.rgb[3 * i + 1] - frame.rgb[3 * i] - frame.rgb[3 * i + 2];
        }
    }
    TEST_ASSERT_GREATER_THAN_INT(100, green[0]);
    TEST_ASSERT_EQUAL_INT(green[0], green[1]);
}

/* A held loud sound is one beat; it has to drop before the next. */
static void test_one_ring_per_beat(void)
{
    rings_init(&r);
    for (int i = 0; i < 20; i++) {
        rings_step(&r, DOWN, 0.9f, 0.0f);
    }
    int n = 0;
    for (int k = 0; k < RINGS_MAX; k++) {
        n += r.strength[k] > 0.0f;
    }
    TEST_ASSERT_EQUAL_INT(1, n);
    rings_step(&r, DOWN, 0.1f, 0.0f);
    rings_step(&r, DOWN, 0.9f, 0.0f);
    n = 0;
    for (int k = 0; k < RINGS_MAX; k++) {
        n += r.strength[k] > 0.0f;
    }
    TEST_ASSERT_EQUAL_INT(2, n);
}

/* Loud music holds the pulse up between beats; every beat still makes a
 * ring, and the off-beat hi-hat does not. */
static void test_beats_over_loud_music(void)
{
    rings_init(&r);
    int rings = 0, beats = 0;
    for (int i = 0; i < 28 * 8; i++) {
        const int at = i % 28;
        float pulse = 0.63f;
        if (at < 6) {
            pulse = 1.0f - 0.06f * at; /* the kick */
        } else if (at >= 14 && at < 16) {
            pulse = 0.73f; /* the hi-hat */
        }
        beats += at == 0;
        const int since = r.since;
        rings_step(&r, DOWN, pulse, 0.0f);
        rings += r.since == 0 && since != 0;
    }
    TEST_ASSERT_EQUAL_INT(beats, rings);
}

/* Many rings at once stay within the fluid's power per LED. */
static void test_power(void)
{
    rings_init(&r);
    for (int i = 0; i < 200; i++) {
        rings_step(&r, DOWN, i % 2 ? 1.0f : 0.0f, 60.0f * i);
        rings_frame(&r, &frame);
        for (int k = 0; k < LED_COUNT; k++) {
            TEST_ASSERT_LESS_OR_EQUAL_INT(FLUID_LEVEL + 1, light(k));
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_quiet_is_dark);
    RUN_TEST(test_beat_ring_rises);
    RUN_TEST(test_ring_is_level);
    RUN_TEST(test_hue_wraps);
    RUN_TEST(test_one_ring_per_beat);
    RUN_TEST(test_beats_over_loud_music);
    RUN_TEST(test_power);
    return UNITY_END();
}
