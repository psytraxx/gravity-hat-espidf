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

/* Mean heat round the hat at height `z` right now. */
static float heat_round(float z)
{
    float sum = 0.0f;
    for (int k = 0; k < 32; k++) {
        sum += fire_heat_at(&f, 2.0f * PI * k / 32, z);
    }
    return sum / 32.0f;
}

/* A beat throws the flames up to the crown within a few steps, and they
 * fall back once it fades. */
static void test_beat_throws_flames_up(void)
{
    fire_init(&f, 11);
    const vec3_t down = v3(0.0f, 0.0f, -1.0f);
    run(down, 0.0f, 200);
    const float z = HAT_HEIGHT * 0.85f;
    TEST_ASSERT_LESS_THAN_FLOAT(0.05f, heat_round(z));
    run(down, 1.0f, 8);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.3f, heat_round(z));
    run(down, 0.0f, 30);
    TEST_ASSERT_LESS_THAN_FLOAT(0.05f, heat_round(z));
}

/* Mean heat at 60 % of the hat's height over `steps` steps at `accel`. */
static float mid_heat(vec3_t accel, int steps)
{
    float sum = 0.0f;
    for (int i = 0; i < steps; i++) {
        fire_step(&f, accel, 0.0f);
        sum += heat_round(HAT_HEIGHT * 0.6f);
    }
    return sum / steps;
}

/* Pressed upwards the flames reach higher; in free fall they sink back
 * to the brim. */
static void test_buoyancy(void)
{
    fire_init(&f, 13);
    run(v3(0.0f, 0.0f, -1.0f), 0.0f, 100);
    const float normal = mid_heat(v3(0.0f, 0.0f, -1.0f), 200);
    run(v3(0.0f, 0.0f, -2.0f), 0.0f, 20);
    const float pressed = mid_heat(v3(0.0f, 0.0f, -2.0f), 200);
    run(v3(0.0f, 0.0f, -0.05f), 0.0f, 20);
    const float falling = mid_heat(v3(0.0f, 0.0f, -0.05f), 40);
    TEST_ASSERT_GREATER_THAN_FLOAT(normal * 1.5f, pressed);
    TEST_ASSERT_LESS_THAN_FLOAT(0.02f, falling);
}

/* A jolt fans the fire: flames leap up as on a beat. */
static void test_jolt_fans_the_fire(void)
{
    fire_init(&f, 11);
    const vec3_t down = v3(0.0f, 0.0f, -1.0f);
    run(down, 0.0f, 200);
    const float z = HAT_HEIGHT * 0.85f;
    TEST_ASSERT_LESS_THAN_FLOAT(0.05f, heat_round(z));
    /* Shaken side to side, 1 g each way, for 8 steps. */
    for (int i = 0; i < 8; i++) {
        fire_step(&f, v3(i % 2 ? 1.0f : -1.0f, 0.0f, -1.0f), 0.0f);
    }
    TEST_ASSERT_GREATER_THAN_FLOAT(0.3f, heat_round(z));
}

/* Shoved towards +x, the flames lean towards +x at once, into the
 * shove, as a helium balloon in a car leans forwards as it speeds up. */
static void test_shove_leans_the_flames(void)
{
    fire_init(&f, 5);
    run(v3(0.0f, 0.0f, -1.0f), 0.0f, 100);
    for (int i = 0; i < 3; i++) {
        /* The hat accelerates towards +x: the fluid would fall towards -x. */
        fire_step(&f, v3(-1.5f, 0.0f, -1.0f), 0.0f);
    }
    TEST_ASSERT_GREATER_THAN_FLOAT(0.5f, f.up.x);
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
    RUN_TEST(test_beat_throws_flames_up);
    RUN_TEST(test_buoyancy);
    RUN_TEST(test_jolt_fans_the_fire);
    RUN_TEST(test_shove_leans_the_flames);
    RUN_TEST(test_colours);
    RUN_TEST(test_frame_follows_heat);
    return UNITY_END();
}
