#include <math.h>

#include "hat.h"
#include "storm.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static storm_t s;
static frame_t frame;
static const vec3_t DOWN = {0.0f, 0.0f, -1.0f};

static int light(int i)
{
    return frame.rgb[3 * i] + frame.rgb[3 * i + 1] + frame.rgb[3 * i + 2];
}

/* A steady sound never bangs; a jump of 20 dB does, once. */
static void test_bang(void)
{
    storm_init(&s, 1);
    for (int i = 0; i < 200; i++) {
        TEST_ASSERT_FALSE(storm_hear(&s, -50.0f));
    }
    TEST_ASSERT_TRUE(storm_hear(&s, -30.0f));
    TEST_ASSERT_FALSE(storm_hear(&s, -30.0f));
    for (int i = 0; i < STORM_BANG_STEPS; i++) {
        storm_hear(&s, -50.0f);
    }
    TEST_ASSERT_TRUE(storm_hear(&s, -30.0f));
}

/* Upright the drops fall towards the brim; upside down, towards the
 * crown. */
static void test_rain_falls_down_the_room(void)
{
    for (int flip = 0; flip < 2; flip++) {
        storm_init(&s, 2);
        const vec3_t g = flip ? v3(0, 0, 1) : DOWN;
        storm_step(&s, g);
        float moved = 0.0f;
        for (int k = 0; k < 5; k++) {
            float before[STORM_DROPS];
            for (int i = 0; i < STORM_DROPS; i++) {
                before[i] = s.drop_row[i];
            }
            storm_step(&s, g);
            for (int i = 0; i < STORM_DROPS; i++) {
                const float d = s.drop_row[i] - before[i];
                if (fabsf(d) < 3.0f) { /* not started again at an edge */
                    moved += d;
                }
            }
        }
        if (flip) {
            TEST_ASSERT_GREATER_THAN_FLOAT(10.0f, moved);
        } else {
            TEST_ASSERT_LESS_THAN_FLOAT(-10.0f, moved);
        }
    }
}

/* Rain lights scattered streaks within the fluid's power. */
static void test_rain_frame(void)
{
    storm_init(&s, 4);
    for (int i = 0; i < 50; i++) {
        storm_step(&s, DOWN);
    }
    storm_frame(&s, &frame);
    int lit = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        lit += light(i) > 8;
        TEST_ASSERT_LESS_OR_EQUAL_INT(FLUID_LEVEL + 1, light(i));
    }
    TEST_ASSERT_GREATER_THAN_INT(20, lit);
    TEST_ASSERT_LESS_THAN_INT(LED_COUNT / 2, lit);
}

/* The rain is blue. */
static void test_rain_colour(void)
{
    int rgb[3] = {0};
    storm_init(&s, 4);
    for (int i = 0; i < 50; i++) {
        storm_step(&s, DOWN);
    }
    storm_frame(&s, &frame);
    for (int i = 0; i < LED_COUNT; i++) {
        for (int ch = 0; ch < 3; ch++) {
            rgb[ch] += frame.rgb[3 * i + ch];
        }
    }
    TEST_ASSERT_GREATER_THAN_INT(rgb[0] + rgb[1], rgb[2]);
}

/* A bang strikes a bolt from the crown to the brim, which fades within a
 * third of a second. */
static void test_lightning(void)
{
    storm_init(&s, 5);
    storm_hear(&s, -50.0f);
    storm_hear(&s, -20.0f);
    storm_step(&s, DOWN);
    int top = 0, bottom = 0;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < STORM_COLS; c++) {
            bottom += s.bolt[r][c] > 0.2f;
            top += s.bolt[STORM_ROWS - 1 - r][c] > 0.2f;
        }
    }
    TEST_ASSERT_GREATER_THAN_INT(0, top);
    TEST_ASSERT_GREATER_THAN_INT(0, bottom);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.8f, s.bolt_life);
    for (int i = 0; i < 18; i++) {
        storm_step(&s, DOWN);
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, s.bolt_life);
}

/* A bang lights the whole hat brightly for a moment, then the flash is
 * gone within a tenth of a second. */
static void test_flash(void)
{
    storm_init(&s, 6);
    storm_hear(&s, -50.0f);
    storm_hear(&s, -20.0f);
    storm_step(&s, DOWN);
    storm_frame(&s, &frame);
    int lit = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        lit += light(i) >= FLUID_LEVEL / 3;
    }
    TEST_ASSERT_GREATER_THAN_INT(LED_COUNT * 9 / 10, lit);
    for (int i = 0; i < 6; i++) {
        storm_step(&s, DOWN);
    }
    TEST_ASSERT_LESS_THAN_FLOAT(0.05f, s.flash);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bang);
    RUN_TEST(test_rain_falls_down_the_room);
    RUN_TEST(test_rain_frame);
    RUN_TEST(test_rain_colour);
    RUN_TEST(test_lightning);
    RUN_TEST(test_flash);
    return UNITY_END();
}
