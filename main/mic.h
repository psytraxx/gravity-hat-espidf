/* I2S microphone (INMP441 or alike): its loudness and spectrum, measured
 * on core 1.
 *
 * The microphone sends 24-bit samples in 32-bit slots at 16 kHz. A task
 * reads them in 20 ms stretches and keeps the RMS of the latest one, its
 * DC removed, and the band levels of the latest SPECTRUM_N samples
 * (spectrum.h). The microphone's L/R pin picks its slot; the louder of
 * the two is taken, so either works. A data line that reads only zeros
 * (no microphone; the pin is pulled down) reports no microphone. */
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "spectrum.h"

/* Set up the I2S channel and start the task on core 1. */
esp_err_t mic_start(void);

/* RMS of the latest 20 ms, full scale 1.0. False if no microphone is
 * sending. */
bool mic_level(float *rms);

/* Band levels of the latest SPECTRUM_N samples, in dB below full scale.
 * False if no microphone is sending. */
bool mic_bands(float db[SPECTRUM_BANDS]);
