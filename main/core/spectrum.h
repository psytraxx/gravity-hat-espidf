/* Sound spectrum: band levels from the microphone's samples, and a
 * meter that follows them. The rings take their colour from the loudest
 * band, and the stars twinkle with the treble.
 *
 * spectrum_bands takes the latest SPECTRUM_N samples, applies a Hann
 * window and an FFT, and sums the power into SPECTRUM_BANDS bands spaced
 * evenly in pitch from SPECTRUM_LOW_HZ to SPECTRUM_HIGH_HZ, in dB below
 * full scale.
 *
 * The meter turns band levels into bars, each from 0 to 1. Each band
 * has its own floor (the quiet in that band: it drops at once to a
 * quieter level and creeps up through louder ones), so a steady hum or
 * the bass that music always carries does not hold its bar up. The
 * loudest band over its floor sets one shared peak, which jumps up at
 * once and sinks slowly after, so loud bands stand out against quiet
 * ones. A bar jumps up to its level and falls at SPECTRUM_FALL. */
#pragma once

#include <stdint.h>

#define SPECTRUM_N       512
#define SPECTRUM_RATE_HZ 16000
#define SPECTRUM_BANDS   12
#define SPECTRUM_LOW_HZ  60.0f
#define SPECTRUM_HIGH_HZ 7000.0f

/* Band levels in dB below full scale (1.0) from the last SPECTRUM_N
 * samples, oldest first. Not reentrant: it works in static buffers. */
void spectrum_bands(const float samples[SPECTRUM_N], float db[SPECTRUM_BANDS]);

/* Bars fall this share of their full height a second. */
#define SPECTRUM_FALL 1.5f

typedef struct {
    float floor_db[SPECTRUM_BANDS];
    float peak_db;
    float bar[SPECTRUM_BANDS]; /* 0 to 1 */
} spectrum_meter_t;

void spectrum_meter_init(spectrum_meter_t *m);

/* Feed the band levels for the last `dt` seconds. */
void spectrum_meter_feed(spectrum_meter_t *m, const float db[SPECTRUM_BANDS], float dt);

/* Band `b`'s colour in degrees of hue: red for the bass to violet for
 * the treble. */
float spectrum_band_hue(int b);

/* The band whose bar stands highest. */
int spectrum_loudest(const spectrum_meter_t *m);

/* Mean bar height of the top quarter of the bands, 0 to 1. */
float spectrum_treble(const spectrum_meter_t *m);
