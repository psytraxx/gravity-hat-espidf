#include "imu.h"

void imu_batch_init(imu_batch_t *b)
{
    *b = (imu_batch_t){.accel = v3(0.0f, 0.0f, -1.0f)};
}

void imu_batch(imu_batch_t *b, const imu_sample_t *samples, int n, float dt)
{
    b->rotation = v3(0.0f, 0.0f, 0.0f);
    if (n <= 0) {
        return;
    }
    vec3_t sum = v3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < n; i++) {
        sum = v3_add(sum, samples[i].accel);
        b->rotation = v3_add(b->rotation, v3_scale(samples[i].gyro, dt));
    }
    b->accel = v3_scale(sum, 1.0f / (float)n);
    b->rate = samples[n - 1].gyro;
}
