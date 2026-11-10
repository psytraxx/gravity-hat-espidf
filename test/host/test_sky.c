#include <math.h>

#include "hat.h"
#include "sky.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define PI 3.14159265f

static sky_t s;
static frame_t frame;
static const vec3_t DOWN = {0.0f, 0.0f, -1.0f};

static int light(int i)
{
    return frame.rgb[3 * i] + frame.rgb[3 * i + 1] + frame.rgb[3 * i + 2];
}

/* An attitude turned `yaw` about the room's up. */
static attitude_t turned(float yaw)
{
    attitude_t a;
    attitude_init(&a);
    attitude_feed(&a, v3(0, 0, 0), DOWN);
    attitude_feed(&a, v3(0, 0, yaw), DOWN);
    return a;
}

/* Some LEDs light, most stay dark, none above the fluid's power. */
static void test_a_sky(void)
{
    sky_init(&s, 3);
    const attitude_t a = turned(0.0f);
    sky_frame(&s, &a, 0.0f, 0.0f, &frame);
    int lit = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        lit += light(i) > 4;
        TEST_ASSERT_LESS_OR_EQUAL_INT(FLUID_LEVEL + 1, light(i));
    }
    TEST_ASSERT_GREATER_THAN_INT(15, lit);
    TEST_ASSERT_LESS_THAN_INT(LED_COUNT / 3, lit);
}

/* The sky stays in the room: whichever way the hat faces, the LED that
 * looks closest to a bright star is lit. */
static void test_fixed_in_the_room(void)
{
    sky_init(&s, 3);
    int bright = 0;
    for (int k = 1; k < SKY_STARS; k++) {
        if (s.star_level[k] > s.star_level[bright]) {
            bright = k;
        }
    }
    int checked = 0;
    for (int t = 0; t < 8; t++) {
        const attitude_t a = turned(t * 0.8f);
        sky_init(&s, 3);
        sky_frame(&s, &a, 0.0f, 0.0f, &frame);
        int best = 0;
        float best_c = -2.0f;
        for (int i = 0; i < LED_COUNT; i++) {
            vec3_t d = hat_led_offset(i);
            d = v3_scale(d, 1.0f / v3_len(d));
            const float c = v3_dot(attitude_to_room(&a, d), s.star[bright]);
            if (c > best_c) {
                best_c = c;
                best = i;
            }
        }
        /* Only a star that some LED looks near. */
        if (best_c > cosf(0.05f)) {
            TEST_ASSERT_GREATER_THAN_INT(10, light(best));
            checked++;
        }
    }
    TEST_ASSERT_GREATER_THAN_INT(3, checked);
}

/* A fast spin leaves trails: more LEDs glow than with the hat still. */
static void test_spin_streaks(void)
{
    int lit[2];
    for (int spin = 0; spin < 2; spin++) {
        sky_init(&s, 3);
        float yaw = 0.0f;
        for (int i = 0; i < 10; i++) {
            yaw += spin ? 0.25f : 0.0f;
            const attitude_t a = turned(yaw);
            sky_frame(&s, &a, spin ? 0.25f : 0.0f, 0.0f, &frame);
        }
        lit[spin] = 0;
        for (int i = 0; i < LED_COUNT; i++) {
            lit[spin] += light(i) > 2;
        }
    }
    TEST_ASSERT_GREATER_THAN_INT(lit[0] * 3 / 2, lit[1]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_sky);
    RUN_TEST(test_fixed_in_the_room);
    RUN_TEST(test_spin_streaks);
    return UNITY_END();
}
