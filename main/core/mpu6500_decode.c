#include "mpu6500_decode.h"

static inline float be16(const uint8_t *p)
{
    return (float)(int16_t)((uint16_t)p[0] << 8 | p[1]);
}

static vec3_t to_can(const float s[3])
{
    return v3(MPU6500_AXIS_MAP[0].sign * s[MPU6500_AXIS_MAP[0].axis],
              MPU6500_AXIS_MAP[1].sign * s[MPU6500_AXIS_MAP[1].axis],
              MPU6500_AXIS_MAP[2].sign * s[MPU6500_AXIS_MAP[2].axis]);
}

/* The accelerometer measures the push opposite to gravity, which the axis
 * map turns around, so the map is minus the sensor-to-hat transform T.
 * Rotation rate is an axial vector and maps as det(T) * T: with det(-T) =
 * -det(T) for 3x3, that is det(map) * map. */
static float map_det(void)
{
    float det = 1.0f;
    for (int i = 0; i < 3; i++) {
        det *= MPU6500_AXIS_MAP[i].sign;
        for (int j = i + 1; j < 3; j++) {
            if (MPU6500_AXIS_MAP[i].axis > MPU6500_AXIS_MAP[j].axis) {
                det = -det;
            }
        }
    }
    return det;
}

imu_sample_t mpu6500_decode(const uint8_t raw[MPU6500_SAMPLE_BYTES])
{
    const float accel[3] = {be16(raw), be16(raw + 2), be16(raw + 4)};
    const float gyro[3] = {be16(raw + 6), be16(raw + 8), be16(raw + 10)};
    return (imu_sample_t){
        .accel = v3_scale(to_can(accel), MPU6500_ACCEL_SCALE),
        .gyro = v3_scale(to_can(gyro), map_det() * MPU6500_GYRO_SCALE),
    };
}
