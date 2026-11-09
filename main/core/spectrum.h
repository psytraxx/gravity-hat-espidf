/* Sound spectrum: band levels from the microphone's samples, and bars
 * that follow them.
 *
 * spectrum_bands takes the latest SPECTRUM_N samples, applies a Hann
 * window and an FFT, and sums the power into SPECTRUM_BANDS bands spaced
 * evenly in pitch from SPECTRUM_LOW_HZ to SPECTRUM_HIGH_HZ, in dB below
 * full scale.
 *
 * The meter turns band levels into bar heights. Each band has its own
 * floor (the quiet in that band: it drops at once to a quieter level and
 * creeps up through louder ones), so a steady hum or the bass that music
 * always carries does not hold its bar up. The loudest band over its
 * floor sets one shared peak, which jumps up at once and sinks slowly
 * after, so loud bands stand out against quiet ones. A bar jumps up to
 * its level and falls at SPECTRUM_FALL; a peak mark stays on its highest
 * point for a moment and then drops. */
#pragma once

#include <stdint.h>

#include "frame.h"

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
/* Peak marks stay up this long, then fall like a bar. */
#define SPECTRUM_HOLD_S 0.4f

typedef struct {
    float floor_db[SPECTRUM_BANDS];
    float peak_db;
    float bar[SPECTRUM_BANDS];  /* 0 to 1 */
    float mark[SPECTRUM_BANDS]; /* 0 to 1 */
    float mark_age[SPECTRUM_BANDS];
} spectrum_meter_t;

void spectrum_meter_init(spectrum_meter_t *m);

/* Feed the band levels for the last `dt` seconds. */
void spectrum_meter_feed(spectrum_meter_t *m, const float db[SPECTRUM_BANDS], float dt);

/* The spectrum round the hat, fixed in the room: the bands stand round
 * the hat from the bass to the treble, counter-clockwise seen from
 * above, as bars rising from the brim, each in its own colour from red to
 * violet with a white mark at its peak. The bottom turn of every bar
 * always glows dimly in its colour, so the mode shows in silence too. `yaw` is how far the hat has
 * turned in the room (heading.h); the bars stay where they are while the
 * hat turns under them, with the bass where LED 0 faced at yaw 0. */
void spectrum_frame(const spectrum_meter_t *m, float yaw, frame_t *frame);
