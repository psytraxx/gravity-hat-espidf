#include "spectrum.h"

#include <math.h>

#define PI 3.14159265f

/* Quietest level told apart. */
#define SILENCE_DB -120.0f
/* Band floors start here, creep up this fast and drop at once. */
#define START_DB            -70.0f
#define FLOOR_RISE_DB_PER_S 3.0f
/* The shared peak sinks this fast and stays at least this far over the
 * floors. */
#define PEAK_FALL_DB_PER_S 6.0f
#define MIN_RANGE_DB       20.0f
/* Levels this close to a band's floor count as quiet. */
#define GATE_DB 6.0f

/* In-place radix-2 FFT of SPECTRUM_N complex values. */
static void fft(float re[SPECTRUM_N], float im[SPECTRUM_N])
{
    for (int i = 1, j = 0; i < SPECTRUM_N; i++) {
        int bit = SPECTRUM_N >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            float t = re[i];
            re[i] = re[j];
            re[j] = t;
            t = im[i];
            im[i] = im[j];
            im[j] = t;
        }
    }
    for (int len = 2; len <= SPECTRUM_N; len <<= 1) {
        const float a = -2.0f * PI / len;
        const float wr = cosf(a), wi = sinf(a);
        for (int i = 0; i < SPECTRUM_N; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                const int p = i + k, q = i + k + len / 2;
                const float tr = re[q] * cr - im[q] * ci;
                const float ti = re[q] * ci + im[q] * cr;
                re[q] = re[p] - tr;
                im[q] = im[p] - ti;
                re[p] += tr;
                im[p] += ti;
                const float n = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = n;
            }
        }
    }
}

void spectrum_bands(const float samples[SPECTRUM_N], float db[SPECTRUM_BANDS])
{
    static float re[SPECTRUM_N], im[SPECTRUM_N];
    /* Hann window, DC removed. */
    float mean = 0.0f;
    for (int i = 0; i < SPECTRUM_N; i++) {
        mean += samples[i];
    }
    mean /= SPECTRUM_N;
    for (int i = 0; i < SPECTRUM_N; i++) {
        const float w = 0.5f - 0.5f * cosf(2.0f * PI * i / (SPECTRUM_N - 1));
        re[i] = (samples[i] - mean) * w;
        im[i] = 0.0f;
    }
    fft(re, im);

    /* Power per band, scaled so a full-scale sine reads 0 dB: its power
     * over the positive bins is N^2 * 3/32 (Parseval, with the Hann
     * window's mean square of 3/8). */
    const float hz_per_bin = (float)SPECTRUM_RATE_HZ / SPECTRUM_N;
    const float scale = 32.0f / (3.0f * (float)SPECTRUM_N * SPECTRUM_N);
    const float ratio = powf(SPECTRUM_HIGH_HZ / SPECTRUM_LOW_HZ, 1.0f / SPECTRUM_BANDS);
    float lo = SPECTRUM_LOW_HZ;
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        const float hi = lo * ratio;
        /* Every band takes at least the bin nearest its middle. */
        int first = (int)ceilf(lo / hz_per_bin);
        int last = (int)ceilf(hi / hz_per_bin) - 1;
        if (last < first) {
            first = last = (int)lrintf(sqrtf(lo * hi) / hz_per_bin);
        }
        float power = 0.0f;
        for (int k = first; k <= last && k < SPECTRUM_N / 2; k++) {
            power += re[k] * re[k] + im[k] * im[k];
        }
        power *= scale;
        db[b] = power > 0.0f ? fmaxf(SILENCE_DB, 10.0f * log10f(power)) : SILENCE_DB;
        lo = hi;
    }
}

void spectrum_meter_init(spectrum_meter_t *m)
{
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        m->floor_db[b] = START_DB;
        m->bar[b] = 0.0f;
    }
    m->peak_db = MIN_RANGE_DB;
}

void spectrum_meter_feed(spectrum_meter_t *m, const float db[SPECTRUM_BANDS], float dt)
{
    float loudest = 0.0f;
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        if (db[b] < m->floor_db[b]) {
            m->floor_db[b] = db[b];
        } else {
            m->floor_db[b] = fminf(db[b], m->floor_db[b] + FLOOR_RISE_DB_PER_S * dt);
        }
        loudest = fmaxf(loudest, db[b] - m->floor_db[b]);
    }
    if (loudest > m->peak_db) {
        m->peak_db = loudest;
    } else {
        m->peak_db = fmaxf(MIN_RANGE_DB, m->peak_db - PEAK_FALL_DB_PER_S * dt);
    }

    const float range = m->peak_db - GATE_DB;
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        float level = (db[b] - m->floor_db[b] - GATE_DB) / range;
        level = level < 0.0f ? 0.0f : level > 1.0f ? 1.0f : level;
        m->bar[b] = fmaxf(level, m->bar[b] - SPECTRUM_FALL * dt);
    }
}

/* Hue of the lowest and the highest band. */
#define HUE_LOW  0.0f
#define HUE_HIGH 280.0f

float spectrum_band_hue(int b)
{
    return HUE_LOW + (HUE_HIGH - HUE_LOW) * b / (SPECTRUM_BANDS - 1);
}

int spectrum_loudest(const spectrum_meter_t *m)
{
    int best = 0;
    for (int b = 1; b < SPECTRUM_BANDS; b++) {
        if (m->bar[b] > m->bar[best]) {
            best = b;
        }
    }
    return best;
}

float spectrum_treble(const spectrum_meter_t *m)
{
    const int first = SPECTRUM_BANDS - SPECTRUM_BANDS / 4;
    float sum = 0.0f;
    for (int b = first; b < SPECTRUM_BANDS; b++) {
        sum += m->bar[b];
    }
    return sum / (SPECTRUM_BANDS - first);
}
