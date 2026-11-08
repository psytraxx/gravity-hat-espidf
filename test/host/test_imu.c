#include "imu.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* Without samples: gravity towards the floor, no rotation. */
static void test_starts_level_and_still(void)
{
    imu_batch_t b;
    imu_batch_init(&b);
    imu_batch(&b, NULL, 0, 0.005f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, b.accel.z);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, b.rotation.z);
}

/* The mean acceleration, the gyro integrated over the samples, the last
 * rate; a frame with no samples keeps the acceleration but stops the
 * rotation. */
static void test_folds_samples(void)
{
    imu_batch_t b;
    imu_batch_init(&b);
    const imu_sample_t s[2] = {
        {.accel = {1.0f, 0.0f, -1.0f}, .gyro = {0.0f, 0.0f, 2.0f}},
        {.accel = {0.0f, 0.0f, -1.0f}, .gyro = {0.0f, 0.0f, 4.0f}},
    };
    imu_batch(&b, s, 2, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, b.accel.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, b.accel.z);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.06f, b.rotation.z);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 4.0f, b.rate.z);

    imu_batch(&b, s, 0, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, b.accel.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, b.rotation.z);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_level_and_still);
    RUN_TEST(test_folds_samples);
    return UNITY_END();
}
