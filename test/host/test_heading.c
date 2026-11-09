#include "heading.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define PI 3.14159265f

static const vec3_t DOWN = {0.0f, 0.0f, -1.0f};

static void test_starts_at_zero(void)
{
    heading_t h;
    heading_init(&h);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, h.yaw);
}

/* Turning counter-clockwise (seen from above) adds up, and wraps round. */
static void test_turning_round(void)
{
    heading_t h;
    heading_init(&h);
    for (int i = 0; i < 100; i++) {
        heading_feed(&h, v3(0.0f, 0.0f, 0.01f), DOWN);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, h.yaw);
    for (int i = 0; i < 100; i++) {
        heading_feed(&h, v3(0.0f, 0.0f, -0.02f), DOWN);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.0f * PI - 1.0f, h.yaw);
}

/* Nodding or tilting turns about a level axis: no turn in the room. */
static void test_nodding_does_not_turn(void)
{
    heading_t h;
    heading_init(&h);
    heading_feed(&h, v3(0.3f, -0.2f, 0.0f), DOWN);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, h.yaw);
}

/* Tilted, only the part of the turn about the room's up counts. */
static void test_tilted_hat(void)
{
    heading_t h;
    heading_init(&h);
    /* Tipped 60 degrees about x: up is (0, -sin 60, cos 60) in hat axes;
     * gravity points the other way. */
    const vec3_t g = v3(0.0f, 0.866f, -0.5f);
    heading_feed(&h, v3(0.0f, 0.0f, 0.2f), g);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.1f, h.yaw);
    heading_feed(&h, v3(0.0f, -0.866f * 0.2f, 0.5f * 0.2f), g);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.3f, h.yaw);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_at_zero);
    RUN_TEST(test_turning_round);
    RUN_TEST(test_nodding_does_not_turn);
    RUN_TEST(test_tilted_hat);
    return UNITY_END();
}
