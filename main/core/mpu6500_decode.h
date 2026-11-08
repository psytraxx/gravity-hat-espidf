/* MPU-6500 sample decoding: raw FIFO bytes to acceleration and rotation
 * rate in hat axes. */
#pragma once

#include <stdint.h>

#include "vec3.h"

/* One FIFO record: accel X, Y, Z then gyro X, Y, Z, each a big-endian
 * int16 (the FIFO keeps register order, and temperature is not stored). */
#define MPU6500_SAMPLE_BYTES 12

/* ±4 g leaves headroom so shaking the hat does not clip:
 * 32768 / 4 = 8192 LSB per g. */
#define MPU6500_ACCEL_SCALE (1.0f / 8192.0f)

/* ±1000 °/s covers spinning the hat by hand: 32.8 LSB per °/s, as rad/s. */
#define MPU6500_GYRO_SCALE (3.14159265f / 180.0f / 32.8f)

typedef struct {
    vec3_t accel; /* gravity plus motion in g, pointing the way the fluid falls */
    vec3_t gyro;  /* rotation rate of the hat in rad/s */
} imu_sample_t;

/* Sensor-to-hat axis mapping: hat axis i = sign * sensor axis `axis`.
 *
 * The board lies flat in the hat's crown with its Z axis pointing
 * up and its X axis pointing at LED 0, so the hat's axes are the
 * sensor's. The accelerometer measures the push opposite to gravity: at
 * rest it reads +1 g on Z, which has to pull the fluid towards the floor
 * (-z), so every axis is turned round. If tilting moves the fluid the
 * wrong way on an axis, flip that axis' sign here; the gyro follows. */
typedef struct {
    uint8_t axis;
    int8_t sign;
} axis_map_t;

static const axis_map_t MPU6500_AXIS_MAP[3] = {{0, -1}, {1, -1}, {2, -1}};

/* Decode one FIFO record (or the same 12 bytes read from ACCEL_XOUT_H with
 * the 2 temperature bytes removed). */
imu_sample_t mpu6500_decode(const uint8_t raw[MPU6500_SAMPLE_BYTES]);
