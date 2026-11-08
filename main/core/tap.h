/* Double-tap detection on raw accelerometer samples.
 *
 * The MPU-6500 has no tap engine outside its DMP, so taps are found here:
 * a tap is a jump in acceleration
 * between consecutive samples (the slope) above a threshold. The samples
 * that follow within the shock time belong to the same tap. A tap only
 * counts after a quiet time with no jump at all, so shaking the hat does
 * not read as tapping; a second tap within the window makes a double tap.
 *
 * Feed it every sample, at the IMU's sample rate (the FIFO keeps them). */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "vec3.h"

/* Ringing after a tap that still belongs to it. */
#define TAP_SHOCK_MS 50
/* Stillness needed before a tap counts. */
#define TAP_QUIET_MS 30
/* The second tap must start within this time of the first. */
#define TAP_WINDOW_MS 375

typedef struct {
    float threshold_g;
    vec3_t last;
    bool primed;
    int64_t last_jump_ms; /* last sample above the threshold */
    int64_t shock_until_ms;
    int64_t first_tap_ms;
    bool have_first;
} tap_detector_t;

void tap_init(tap_detector_t *t, float threshold_g);

/* Feed one sample (in g) taken at `now_ms`; true on the second tap of a
 * double tap. */
bool tap_feed(tap_detector_t *t, vec3_t accel, int64_t now_ms);
