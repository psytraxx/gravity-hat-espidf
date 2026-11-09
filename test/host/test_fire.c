#include <math.h>

#include "fire.h"
#include "hat.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define PI 3.14159265f

static fire_t f;

static void run(vec3_t accel, float stoke, int steps)
{
    for (int i = 0; i < steps; i++) {
        fire_step(&f, accel, stoke);
    }
}

/* Mean heat over `steps` steps at heights within `band` cells of `z`, all
 * the way round. */
static float heat_near(vec3_t accel, float z, int steps)
{
    float sum = 0.0f;
    for (int i = 0; i < steps; i++) {
        fire_step(&f, accel, 0.0f);
        for (int k = 0; k < 32; k++) {
            sum += fire_heat_at(&f, 2.0f * PI * k / 32, z);
        }
    }
    return sum / (32.0f * steps);
}

/* Heat over `steps` steps at angle `a`, averaged over the middle of the
 * hat's height. */
static float heat_at_angle(vec3_t accel, float a, int steps)
{
    float sum = 0.0f;
    for (int i = 0; i < steps; i++) {
        fire_step(&f, accel, 0.0f);
        for (int k = 1; k < 8; k++) {
            sum += fire_heat_at(&f, a, HAT_HEIGHT * k / 8.0f);
        }
    }
    return sum / (7.0f * steps);
}

static void test_starts_cold(void)
{
    fire_init(&f, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fire_heat_at(&f, 1.0f, HAT_HEIGHT / 2));
}

/* Upright, the brim burns and the crown stays dark. */
static void test_upright_burns_up_from_the_brim(void)
{
    fire_init(&f, 7);
    const vec3_t down = v3(0.0f, 0.0f, -1.0f);
    run(down, 0.0f, 100);
    const float brim = heat_near(down, HAT_HEIGHT * 0.1f, 200);
    const float crown = heat_near(down, HAT_HEIGHT * 0.9f, 200);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.3f, brim);
    TEST_ASSERT_LESS_THAN_FLOAT(0.1f, crown);
}

/* Upside down, flames burn from the crown towards the brim. */
static void test_upside_down_burns_from_the_crown(void)
{
    fire_init(&f, 7);
    const vec3_t up = v3(0.0f, 0.0f, 1.0f);
    run(up, 0.0f, 100);
    const float brim = heat_near(up, HAT_HEIGHT * 0.1f, 200);
    const float crown = heat_near(up, HAT_HEIGHT * 0.9f, 200);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.3f, crown);
    TEST_ASSERT_LESS_THAN_FLOAT(0.1f, brim);
}

/* On its side with gravity towards -x, the low side (angle pi) burns and
 * the high side (angle 0) stays dark. */
static void test_on_its_side_burns_on_the_low_side(void)
{
    fire_init(&f, 7);
    const vec3_t side = v3(-1.0f, 0.0f, 0.0f);
    run(side, 0.0f, 100);
    const float low = heat_at_angle(side, PI, 200);
    const float high = heat_at_angle(side, 0.0f, 200);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.3f, low);
    TEST_ASSERT_LESS_THAN_FLOAT(0.05f, high);
}

/* Loud sound makes taller flames. */
static void test_sound_stokes_the_fire(void)
{
    const vec3_t down = v3(0.0f, 0.0f, -1.0f);
    float mid[2];
    for (int loud = 0; loud < 2; loud++) {
        fire_init(&f, 3);
        float sum = 0.0f;
        for (int i = 0; i < 300; i++) {
            fire_step(&f, down, (float)loud);
            for (int k = 0; k < 32; k++) {
                sum += fire_heat_at(&f, 2.0f * PI * k / 32, HAT_HEIGHT * 0.5f);
            }
        }
        mid[loud] = sum;
    }
    TEST_ASSERT_GREATER_THAN_FLOAT(mid[0] * 1.5f, mid[1]);
}

/* The palette: dark when cold, red, then towards yellow, and never more
 * power than the fluid. */
static void test_colours(void)
{
    const color_t cold = fire_color(0.0f);
    TEST_ASSERT_EQUAL_UINT8(0, cold.r + cold.g + cold.b);
    const color_t warm = fire_color(0.4f);
    TEST_ASSERT_GREATER_THAN_UINT8(0, warm.r);
    TEST_ASSERT_EQUAL_UINT8(0, warm.g);
    const color_t hot = fire_color(1.0f);
    TEST_ASSERT_GREATER_THAN_UINT8(hot.g, hot.r);
    TEST_ASSERT_GREATER_THAN_UINT8(10, hot.g);
    for (int i = 0; i <= 100; i++) {
        const color_t c = fire_color(i / 100.0f);
        TEST_ASSERT_LESS_OR_EQUAL_INT(FLUID_LEVEL, c.r + c.g + c.b);
        TEST_ASSERT_EQUAL_UINT8(0, c.b);
    }
}

/* The frame shows the heat behind each LED. */
static void test_frame_follows_heat(void)
{
    fire_init(&f, 7);
    run(v3(0.0f, 0.0f, -1.0f), 0.0f, 100);
    static frame_t frame;
    fire_frame(&f, &frame);
    int lit_low = 0, lit_high = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        const int sum = frame.rgb[3 * i] + frame.rgb[3 * i + 1] + frame.rgb[3 * i + 2];
        if (sum > 0) {
            if (i < LED_COUNT / 4) {
                lit_low++;
            } else if (i >= LED_COUNT * 3 / 4) {
                lit_high++;
            }
        }
    }
    TEST_ASSERT_GREATER_THAN_INT(LED_COUNT / 8, lit_low);
    TEST_ASSERT_LESS_THAN_INT(LED_COUNT / 20, lit_high);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_cold);
    RUN_TEST(test_upright_burns_up_from_the_brim);
    RUN_TEST(test_upside_down_burns_from_the_crown);
    RUN_TEST(test_on_its_side_burns_on_the_low_side);
    RUN_TEST(test_sound_stokes_the_fire);
    RUN_TEST(test_colours);
    RUN_TEST(test_frame_follows_heat);
    return UNITY_END();
}
