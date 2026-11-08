#include <math.h>

#include "hat.h"
#include "mpu6500_decode.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define PI 3.14159265f

/* LED 0 sits at angle 0 on the bottom turn; each whole turn later the
 * strip is back at the same angle, a row higher. */
static void test_spiral_climbs_one_row_per_turn(void)
{
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, hat_led_angle(0));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f * HAT_ROW_PITCH_CELLS, hat_led_height(0));
    for (int t = 1; t <= (int)HAT_TURNS; t++) {
        const int led = (int)lrintf(hat_led_at_turns((float)t));
        const float a = hat_led_angle(led);
        TEST_ASSERT_TRUE(a < 0.1f || a > 2.0f * PI - 0.1f);
        TEST_ASSERT_FLOAT_WITHIN(0.02f, (0.5f + (float)t) * HAT_ROW_PITCH_CELLS,
                                 hat_led_height(led));
    }
    /* A quarter turn on is on the +y side counter-clockwise from above,
     * the -y side clockwise. */
    const vec3_t q = hat_led_pos((int)lrintf(hat_led_at_turns(0.25f)));
    const float side = HAT_CLOCKWISE ? -1.0f : 1.0f;
    TEST_ASSERT_GREATER_THAN_FLOAT(0.9f * hat_radius_at(q.z), side * (q.y - FLUID_NY / 2.0f));
}

/* The bottom turn holds HAT_LEDS_PER_TURN_BOTTOM LEDs, the top one
 * HAT_LEDS_PER_TURN_TOP, the strip ends after HAT_TURNS turns, and the
 * wider end has the more LEDs a turn. */
static void test_turns_shorten_as_the_can_narrows(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.5f, HAT_LEDS_PER_TURN_BOTTOM, hat_led_at_turns(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, HAT_LEDS_PER_TURN_TOP,
                             hat_led_at_turns(HAT_TURNS) - hat_led_at_turns(HAT_TURNS - 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, (float)LED_COUNT, hat_led_at_turns(HAT_TURNS));
    TEST_ASSERT_TRUE((hat_radius_at(0.0f) >= hat_radius_at(HAT_HEIGHT)) ==
                     (HAT_LEDS_PER_TURN_BOTTOM >= HAT_LEDS_PER_TURN_TOP));
}

/* Every LED sits on the hat's wall at its height, inside the grid. */
static void test_every_led_on_the_wall(void)
{
    for (int i = 0; i < LED_COUNT; i++) {
        const vec3_t p = hat_led_pos(i);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, hat_radius_at(p.z),
                                 hypotf(p.x - FLUID_NX / 2.0f, p.y - FLUID_NY / 2.0f));
        TEST_ASSERT_TRUE(p.z > 0.0f && p.z < HAT_HEIGHT);
        TEST_ASSERT_TRUE(p.x >= 0.0f && p.x <= FLUID_NX);
        TEST_ASSERT_TRUE(p.y >= 0.0f && p.y <= FLUID_NY);
    }
    TEST_ASSERT_TRUE(hat_led_height(LED_COUNT - 1) > hat_led_height(LED_COUNT - 2));
}

static int lit_count(const hat_view_t *v)
{
    int n = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        n += v->lit[i];
    }
    return n;
}

/* A settled half-full hat lights about the bottom half of the strip, every
 * lit LED in the hue's colour, and the picture holds still. */
static void test_half_full_can_lights_bottom_half(void)
{
    static fluid_t f;
    fluid_init(&f);
    fluid_populate(&f);
    const vec3_t down = {0.0f, 0.0f, -1.0f};
    for (int i = 0; i < 300; i++) {
        fluid_step(&f, down);
    }
    hat_view_t v;
    hat_view_init(&v);
    frame_t frame;
    hat_view_frame(&v, &f, FLUID_HUE, 1.0f, &frame);
    const int lit = lit_count(&v);
    TEST_ASSERT_INT_WITHIN(LED_COUNT / 8, LED_COUNT / 2, lit);
    TEST_ASSERT_TRUE(v.lit[0]);
    TEST_ASSERT_FALSE(v.lit[LED_COUNT - 1]);
    /* Green fluid: lit LEDs have green, dark ones nothing. */
    TEST_ASSERT_GREATER_THAN_UINT8(0, frame.rgb[1]);
    const int last = (LED_COUNT - 1) * 3;
    TEST_ASSERT_EQUAL_UINT8(0, frame.rgb[last] + frame.rgb[last + 1] + frame.rgb[last + 2]);

    int changes = 0;
    for (int s = 0; s < 100; s++) {
        bool before[LED_COUNT];
        for (int i = 0; i < LED_COUNT; i++) {
            before[i] = v.lit[i];
        }
        fluid_step(&f, down);
        hat_view_frame(&v, &f, FLUID_HUE, 1.0f, &frame);
        for (int i = 0; i < LED_COUNT; i++) {
            changes += v.lit[i] != before[i];
        }
    }
    /* Fewer than one LED change in five steps. */
    TEST_ASSERT_LESS_THAN_INT(20, changes);
}

/* Tilted towards +x the fluid lights the +x side higher up than the -x
 * side. */
static void test_tilt_lights_the_low_side(void)
{
    static fluid_t f;
    fluid_init(&f);
    fluid_populate(&f);
    const vec3_t tilt = {0.7f, 0.0f, -0.7f};
    for (int i = 0; i < 300; i++) {
        fluid_step(&f, tilt);
    }
    hat_view_t v;
    hat_view_init(&v);
    frame_t frame;
    hat_view_frame(&v, &f, FLUID_HUE, 1.0f, &frame);
    float east = 0.0f, west = 0.0f;
    for (int i = 0; i < LED_COUNT; i++) {
        if (!v.lit[i]) {
            continue;
        }
        const float a = hat_led_angle(i);
        const float z = hat_led_height(i);
        if (a < PI / 4.0f || a > 7.0f * PI / 4.0f) {
            east = fmaxf(east, z);
        } else if (a > 3.0f * PI / 4.0f && a < 5.0f * PI / 4.0f) {
            west = fmaxf(west, z);
        }
    }
    TEST_ASSERT_GREATER_THAN_FLOAT(west + 1.5f, east);
}

static bool is(const frame_t *f, int led, uint8_t r, uint8_t g, uint8_t b)
{
    return f->rgb[led * 3] == r && f->rgb[led * 3 + 1] == g && f->rgb[led * 3 + 2] == b;
}

/* Level: one red, green, blue and yellow LED per turn, a quarter turn
 * apart, nothing else. */
static void test_calibration_lines(void)
{
    static frame_t f;
    hat_calibration_frame(&f, (vec3_t){0.0f, 0.0f, -1.0f});
    int count[4] = {0, 0, 0, 0}, other = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        if (is(&f, i, 60, 0, 0)) {
            count[0]++;
        } else if (is(&f, i, 0, 60, 0)) {
            count[1]++;
        } else if (is(&f, i, 0, 0, 60)) {
            count[2]++;
        } else if (is(&f, i, 30, 30, 0)) {
            count[3]++;
        } else if (!is(&f, i, 0, 0, 0)) {
            other++;
        }
    }
    for (int q = 0; q < 4; q++) {
        TEST_ASSERT_INT_WITHIN(1, (int)HAT_TURNS, count[q]);
    }
    TEST_ASSERT_EQUAL_INT(0, other);
    TEST_ASSERT_TRUE(is(&f, 0, 60, 0, 0));
    /* A quarter turn on: green (+y) counter-clockwise, yellow (-y)
     * clockwise. */
    const int quarter = (int)floorf(hat_led_at_turns(0.25f) + 0.5f);
    TEST_ASSERT_TRUE(HAT_CLOCKWISE ? is(&f, quarter, 30, 30, 0) : is(&f, quarter, 0, 60, 0));
    TEST_ASSERT_TRUE(is(&f, (int)floorf(hat_led_at_turns(3.5f) + 0.5f), 0, 0, 60));
}

/* Tilted so the fluid falls towards +y: the white band lights LEDs on the
 * +y side, and none on the -y side. */
static void test_calibration_band_follows_tilt(void)
{
    static frame_t f;
    hat_calibration_frame(&f, (vec3_t){0.0f, 0.5f, -0.85f});
    int plus_y = 0, minus_y = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        if (!is(&f, i, 20, 20, 20)) {
            continue;
        }
        const float a = hat_led_angle(i);
        plus_y += a > PI / 4.0f && a < 3.0f * PI / 4.0f;
        minus_y += a > 5.0f * PI / 4.0f && a < 7.0f * PI / 4.0f;
    }
    TEST_ASSERT_GREATER_THAN_INT(3 * (int)HAT_TURNS, plus_y);
    TEST_ASSERT_EQUAL_INT(0, minus_y);
}

/* At rest, board flat with Z up: +1 g on sensor Z pulls towards the floor,
 * +1 g on sensor X (X pointing up) towards -x, and the gyro keeps its
 * axes. */
static void test_decode_rest_and_spin(void)
{
    /* accel (0, 0, +8192) = +1 g on Z; gyro Z = +328 = +10 °/s. */
    const uint8_t raw[MPU6500_SAMPLE_BYTES] = {0, 0, 0, 0, 0x20, 0x00, 0, 0, 0, 0, 0x01, 0x48};
    const imu_sample_t s = mpu6500_decode(raw);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, s.accel.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.0f, s.accel.z);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 10.0f * PI / 180.0f, s.gyro.z);
    const uint8_t up_x[MPU6500_SAMPLE_BYTES] = {0x20, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.0f, mpu6500_decode(up_x).accel.x);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_spiral_climbs_one_row_per_turn);
    RUN_TEST(test_turns_shorten_as_the_can_narrows);
    RUN_TEST(test_every_led_on_the_wall);
    RUN_TEST(test_half_full_can_lights_bottom_half);
    RUN_TEST(test_tilt_lights_the_low_side);
    RUN_TEST(test_calibration_lines);
    RUN_TEST(test_calibration_band_follows_tilt);
    RUN_TEST(test_decode_rest_and_spin);
    return UNITY_END();
}
