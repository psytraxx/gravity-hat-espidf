/* MPU-6500 accelerometer and gyroscope on SPI.
 *
 * Samples at 250 Hz into the chip's FIFO (accel ±4 g, gyro ±1000 °/s,
 * 92 Hz low-pass), so the main loop collects every sample since the last
 * frame: the tap detector needs them all, and the gyro is integrated over
 * them. */
#pragma once

#include "esp_err.h"
#include "mpu6500_decode.h"

#define MPU6500_SAMPLE_RATE_HZ 250
#define MPU6500_SAMPLE_MS      (1000 / MPU6500_SAMPLE_RATE_HZ)

/* Set up the bus, configure the chip and calibrate the gyro. The hat
 * should be still while this runs (about half a second). */
esp_err_t mpu6500_init(void);

/* Read up to `max` samples from the FIFO, oldest first, gyro bias removed,
 * turned by the IMU rotation in menuconfig into the hat's axes.
 * Returns the number read, 0 if none are waiting or the FIFO overflowed
 * (it is then reset), or -1 on a bus error. */
int mpu6500_read(imu_sample_t *out, int max);
