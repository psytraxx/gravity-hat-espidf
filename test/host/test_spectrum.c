#include <math.h>

#include "spectrum.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define PI 3.14159265f
#define DT 0.018f

static float samples[SPECTRUM_N];
static float db[SPECTRUM_BANDS];

static void sine(float hz, float amplitude)
{
    for (int i = 0; i < SPECTRUM_N; i++) {
        samples[i] = amplitude * sinf(2.0f * PI * hz * i / SPECTRUM_RATE_HZ);
    }
}

/* Band holding `hz`. */
static int band_of(float hz)
{
    return (int)floorf(logf(hz / SPECTRUM_LOW_HZ) / logf(SPECTRUM_HIGH_HZ / SPECTRUM_LOW_HZ) *
                       SPECTRUM_BANDS);
}

static int loudest(void)
{
    int best = 0;
    for (int b = 1; b < SPECTRUM_BANDS; b++) {
        if (db[b] > db[best]) {
            best = b;
        }
    }
    return best;
}

static void test_silence_reads_silent(void)
{
    sine(0.0f, 0.0f);
    spectrum_bands(samples, db);
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        TEST_ASSERT_LESS_THAN_FLOAT(-100.0f, db[b]);
    }
}

/* A tone lands in its band, near its level, and well above the bands
 * away from it. */
static void test_tone_lands_in_its_band(void)
{
    const float tones[] = {100.0f, 440.0f, 1000.0f, 3000.0f, 6000.0f};
    for (unsigned t = 0; t < sizeof tones / sizeof *tones; t++) {
        sine(tones[t], 0.5f);
        spectrum_bands(samples, db);
        const int b = band_of(tones[t]);
        TEST_ASSERT_INT_WITHIN(1, b, loudest());
        /* A sine of amplitude 0.5 is 6 dB below full scale; a band may hold
         * only part of its window's main lobe. */
        TEST_ASSERT_FLOAT_WITHIN(4.0f, -6.0f, db[loudest()]);
        for (int o = 0; o < SPECTRUM_BANDS; o++) {
            if (o < b - 2 || o > b + 2) {
                TEST_ASSERT_LESS_THAN_FLOAT(db[loudest()] - 30.0f, db[o]);
            }
        }
    }
}

/* Quiet, then a tone: its bar jumps up and the rest stay down; then
 * quiet again, and the bar falls. */
static void test_meter_bars(void)
{
    spectrum_meter_t m;
    spectrum_meter_init(&m);
    float quiet[SPECTRUM_BANDS];
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        quiet[b] = -80.0f;
    }
    for (int i = 0; i < 100; i++) {
        spectrum_meter_feed(&m, quiet, DT);
    }
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, m.bar[b]);
    }

    float loud[SPECTRUM_BANDS];
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        loud[b] = b == 5 ? -30.0f : -80.0f;
    }
    spectrum_meter_feed(&m, loud, DT);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, m.bar[5]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, m.bar[4]);

    for (int i = 0; i < 10; i++) {
        spectrum_meter_feed(&m, quiet, DT);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.0f - 10 * DT * SPECTRUM_FALL, m.bar[5]);
    for (int i = 0; i < 100; i++) {
        spectrum_meter_feed(&m, quiet, DT);
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, m.bar[5]);
}

/* A steady level settles: the floor creeps up to it and its bar sinks. */
static void test_steady_hum_settles(void)
{
    spectrum_meter_t m;
    spectrum_meter_init(&m);
    float hum[SPECTRUM_BANDS];
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        hum[b] = b == 0 ? -20.0f : -80.0f;
    }
    for (int i = 0; i < (int)(30.0f / DT); i++) {
        spectrum_meter_feed(&m, hum, DT);
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, m.bar[0]);
}

/* The loudest band, the treble and each band's hue. */
static void test_helpers(void)
{
    spectrum_meter_t m;
    spectrum_meter_init(&m);
    m.bar[3] = 0.8f;
    m.bar[5] = 0.4f;
    TEST_ASSERT_EQUAL_INT(3, spectrum_loudest(&m));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, spectrum_treble(&m));
    for (int k = SPECTRUM_BANDS - SPECTRUM_BANDS / 4; k < SPECTRUM_BANDS; k++) {
        m.bar[k] = 0.6f;
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.6f, spectrum_treble(&m));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, spectrum_band_hue(0));
    TEST_ASSERT_GREATER_THAN_FLOAT(spectrum_band_hue(4), spectrum_band_hue(5));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_silence_reads_silent);
    RUN_TEST(test_tone_lands_in_its_band);
    RUN_TEST(test_meter_bars);
    RUN_TEST(test_steady_hum_settles);
    RUN_TEST(test_helpers);
    return UNITY_END();
}
