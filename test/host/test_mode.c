#include "mode.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define DT 0.018f

static const vec3_t UPRIGHT = {0.0f, 0.0f, -1.0f};
static const vec3_t UPSIDE_DOWN = {0.0f, 0.0f, 1.0f};
static const vec3_t ON_ITS_SIDE = {1.0f, 0.0f, 0.0f};

static mode_switch_t m;

/* Frames switched in `steps` frames at `accel`. */
static int hold(vec3_t accel, int steps)
{
    int switched = 0;
    for (int i = 0; i < steps; i++) {
        switched += mode_switch_feed(&m, accel, DT);
    }
    return switched;
}

static int frames(float seconds)
{
    return (int)(seconds / DT + 0.5f);
}

static void test_upright_never_switches(void)
{
    mode_switch_init(&m, MODE_FLUID);
    TEST_ASSERT_EQUAL_INT(0, hold(UPRIGHT, frames(30.0f)));
    TEST_ASSERT_EQUAL_INT(MODE_FLUID, m.mode);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mode_switch_progress(&m));
}

static void test_two_seconds_upside_down_switch(void)
{
    mode_switch_init(&m, MODE_FLUID);
    TEST_ASSERT_EQUAL_INT(0, hold(UPSIDE_DOWN, frames(1.9f)));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.95f, mode_switch_progress(&m));
    TEST_ASSERT_EQUAL_INT(1, hold(UPSIDE_DOWN, frames(0.2f)));
    TEST_ASSERT_EQUAL_INT(MODE_FIRE, m.mode);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mode_switch_progress(&m));
}

/* Held upside down for longer, it switches once; back upright and over
 * again, it switches on, round to the first mode. */
static void test_one_switch_per_flip(void)
{
    mode_switch_init(&m, MODE_FLUID);
    TEST_ASSERT_EQUAL_INT(1, hold(UPSIDE_DOWN, frames(10.0f)));
    TEST_ASSERT_EQUAL_INT(0, hold(ON_ITS_SIDE, frames(1.0f)));
    TEST_ASSERT_EQUAL_INT(0, hold(UPSIDE_DOWN, frames(3.0f)));
    TEST_ASSERT_EQUAL_INT(MODE_FIRE, m.mode);
    hold(UPRIGHT, 1);
    TEST_ASSERT_EQUAL_INT(1, hold(UPSIDE_DOWN, frames(2.1f)));
    TEST_ASSERT_EQUAL_INT((MODE_FIRE + 1) % MODE_COUNT, m.mode);
}

/* Turning away from upside down, even for a moment, starts the hold
 * again. */
static void test_hold_restarts(void)
{
    mode_switch_init(&m, MODE_FLUID);
    hold(UPSIDE_DOWN, frames(1.5f));
    hold(ON_ITS_SIDE, 1);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mode_switch_progress(&m));
    TEST_ASSERT_EQUAL_INT(0, hold(UPSIDE_DOWN, frames(1.5f)));
    TEST_ASSERT_EQUAL_INT(MODE_FLUID, m.mode);
}

/* Tipped 30 degrees short of upside down still counts; tipped 60 does
 * not. */
static void test_upside_down_within_45_degrees(void)
{
    mode_switch_init(&m, MODE_FLUID);
    TEST_ASSERT_EQUAL_INT(1, hold(v3(0.5f, 0.0f, 0.866f), frames(2.1f)));
    mode_switch_init(&m, MODE_FLUID);
    TEST_ASSERT_EQUAL_INT(0, hold(v3(0.866f, 0.0f, 0.5f), frames(5.0f)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_upright_never_switches);
    RUN_TEST(test_two_seconds_upside_down_switch);
    RUN_TEST(test_one_switch_per_flip);
    RUN_TEST(test_hold_restarts);
    RUN_TEST(test_upside_down_within_45_degrees);
    return UNITY_END();
}
