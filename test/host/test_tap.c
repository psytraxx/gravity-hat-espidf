#include "tap.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* 250 Hz, as the firmware samples. */
#define PERIOD_MS 4

static const vec3_t REST = {0.0f, 0.0f, -1.0f};

typedef struct {
    tap_detector_t d;
    int64_t t;
    int doubles;
} rig_t;

static void rig_init(rig_t *r)
{
    tap_init(&r->d, 1.0f);
    r->t = 0;
    r->doubles = 0;
}

static void feed(rig_t *r, vec3_t a)
{
    r->doubles += tap_feed(&r->d, a, r->t);
    r->t += PERIOD_MS;
}

static void rest(rig_t *r, int ms)
{
    for (int i = 0; i < ms / PERIOD_MS; i++) {
        feed(r, REST);
    }
}

/* A tap: a 2 g spike for one sample, then a few samples of ringing. */
static void tap(rig_t *r)
{
    feed(r, v3(0.0f, 0.0f, 1.0f));
    feed(r, v3(0.0f, 0.0f, -1.5f));
    feed(r, v3(0.0f, 0.0f, -0.8f));
    feed(r, REST);
}

static void test_double_tap(void)
{
    rig_t r;
    rig_init(&r);
    rest(&r, 200);
    tap(&r);
    rest(&r, 150);
    tap(&r);
    rest(&r, 200);
    TEST_ASSERT_EQUAL_INT(1, r.doubles);
}

static void test_single_tap_is_not_double(void)
{
    rig_t r;
    rig_init(&r);
    rest(&r, 200);
    tap(&r);
    rest(&r, 1000);
    TEST_ASSERT_EQUAL_INT(0, r.doubles);
}

static void test_taps_too_far_apart(void)
{
    rig_t r;
    rig_init(&r);
    rest(&r, 200);
    tap(&r);
    rest(&r, 500);
    tap(&r);
    rest(&r, 200);
    TEST_ASSERT_EQUAL_INT(0, r.doubles);
}

/* Shaking: big jumps every few samples for a second. */
static void test_shaking_is_not_tapping(void)
{
    rig_t r;
    rig_init(&r);
    rest(&r, 200);
    for (int i = 0; i < 250; i++) {
        feed(&r, (i / 3) % 2 ? v3(1.5f, 0.0f, -1.0f) : v3(-1.5f, 0.0f, -1.0f));
    }
    rest(&r, 200);
    TEST_ASSERT_EQUAL_INT(0, r.doubles);
}

/* Slow tilts never jump by a full g between samples. */
static void test_tilting_is_not_tapping(void)
{
    rig_t r;
    rig_init(&r);
    for (int i = 0; i < 500; i++) {
        const float a = (float)i * 0.02f;
        feed(&r, v3(sinf(a), 0.0f, -cosf(a)));
    }
    TEST_ASSERT_EQUAL_INT(0, r.doubles);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_double_tap);
    RUN_TEST(test_single_tap_is_not_double);
    RUN_TEST(test_taps_too_far_apart);
    RUN_TEST(test_shaking_is_not_tapping);
    RUN_TEST(test_tilting_is_not_tapping);
    return UNITY_END();
}
