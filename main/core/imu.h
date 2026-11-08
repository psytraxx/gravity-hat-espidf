/* A frame's worth of IMU samples, folded into one reading. */
#pragma once

#include "mpu6500_decode.h"

typedef struct {
    vec3_t accel;    /* mean acceleration over the samples, in g */
    vec3_t rotation; /* how far the hat turned over them (axis times radians) */
    vec3_t rate;     /* the last sample's rotation rate, in rad/s */
} imu_batch_t;

/* Start with gravity towards the floor and no rotation. */
void imu_batch_init(imu_batch_t *b);

/* Fold `n` samples taken `dt` seconds apart into `b`. With none, the
 * acceleration and rate keep their values and the rotation is zero. */
void imu_batch(imu_batch_t *b, const imu_sample_t *samples, int n, float dt);
