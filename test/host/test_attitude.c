#include <math.h>

#include "attitude.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define PI 3.14159265f

static const vec3_t DOWN = {0.0f, 0.0f, -1.0f};

static void assert_vec(vec3_t want, vec3_t got, float tol)
{
    TEST_ASSERT_FLOAT_WITHIN(tol, want.x, got.x);
    TEST_ASSERT_FLOAT_WITHIN(tol, want.y, got.y);
    TEST_ASSERT_FLOAT_WITHIN(tol, want.z, got.z);
}

/* Upright and still, hat axes are room axes. */
static void test_upright_is_room(void)
{
    attitude_t a;
    attitude_init(&a);
    attitude_feed(&a, v3(0, 0, 0), DOWN);
    attitude_feed(&a, v3(0, 0, 0), DOWN);
    assert_vec(v3(1, 0, 0), attitude_to_room(&a, v3(1, 0, 0)), 1e-5f);
    assert_vec(v3(0, 0, 1), attitude_to_room(&a, v3(0, 0, 1)), 1e-5f);
}

/* Turned a quarter turn counter-clockwise about up: LED 0's side (hat
 * +x) now faces the room's +y. */
static void test_turning(void)
{
    attitude_t a;
    attitude_init(&a);
    attitude_feed(&a, v3(0, 0, 0), DOWN);
    for (int i = 0; i < 100; i++) {
        attitude_feed(&a, v3(0, 0, 0.5f * PI / 100), DOWN);
    }
    assert_vec(v3(0, 1, 0), attitude_to_room(&a, v3(1, 0, 0)), 1e-3f);
}

/* Tipped a quarter turn about the hat's x axis (gravity follows): the
 * hat's +y points up the room, and stays there with gravity's help. */
static void test_tilting(void)
{
    attitude_t a;
    attitude_init(&a);
    attitude_feed(&a, v3(0, 0, 0), DOWN);
    for (int i = 1; i <= 50; i++) {
        const float t = 0.5f * PI * i / 50;
        /* Gravity in hat axes as the hat tips up its +y. */
        attitude_feed(&a, v3(0.5f * PI / 50, 0, 0), v3(0, -sinf(t), -cosf(t)));
    }
    assert_vec(v3(0, 0, 1), attitude_to_room(&a, v3(0, 1, 0)), 1e-3f);
    assert_vec(v3(1, 0, 0), attitude_to_room(&a, v3(1, 0, 0)), 1e-3f);
}

/* A gyro that reads a tilt that is not there is pulled back level by
 * gravity. */
static void test_gravity_corrects_tilt(void)
{
    attitude_t a;
    attitude_init(&a);
    attitude_feed(&a, v3(0, 0, 0), DOWN);
    attitude_feed(&a, v3(0.3f, 0, 0), DOWN);
    for (int i = 0; i < 300; i++) {
        attitude_feed(&a, v3(0, 0, 0), DOWN);
    }
    assert_vec(v3(0, 0, 1), attitude_to_room(&a, v3(0, 0, 1)), 1e-3f);
}

/* Started upside down: up is the hat's -z. */
static void test_starts_from_gravity(void)
{
    attitude_t a;
    attitude_init(&a);
    attitude_feed(&a, v3(0, 0, 0), v3(0, 0, 1));
    assert_vec(v3(0, 0, -1), attitude_to_room(&a, v3(0, 0, 1)), 1e-5f);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_upright_is_room);
    RUN_TEST(test_turning);
    RUN_TEST(test_tilting);
    RUN_TEST(test_gravity_corrects_tilt);
    RUN_TEST(test_starts_from_gravity);
    return UNITY_END();
}
