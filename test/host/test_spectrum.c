#include <math.h>

#include "hat.h"
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
 * quiet again, and the bar falls while its peak mark holds a moment. */
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
    TEST_ASSERT_EQUAL_FLOAT(1.0f, m.mark[5]);
    for (int i = 0; i < 100; i++) {
        spectrum_meter_feed(&m, quiet, DT);
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, m.bar[5]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, m.mark[5]);
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

/* Lit LEDs of the frame whose room angle is within the first band, and
 * all lit LEDs. */
static void count_lit(const spectrum_meter_t *m, float yaw, int *in_band, int *all)
{
    static frame_t frame;
    spectrum_frame(m, yaw, &frame);
    *in_band = *all = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        if (frame.rgb[3 * i] + frame.rgb[3 * i + 1] + frame.rgb[3 * i + 2] == 0) {
            continue;
        }
        (*all)++;
        const float a = fmodf(hat_led_angle(i) + yaw, 2.0f * PI);
        if (a < 2.0f * PI / SPECTRUM_BANDS) {
            (*in_band)++;
        }
    }
}

/* One full bar stands in the same place in the room whichever way the
 * hat faces. */
static void test_bars_stay_in_the_room(void)
{
    spectrum_meter_t m;
    spectrum_meter_init(&m);
    m.bar[0] = 1.0f;
    for (int k = 0; k < 8; k++) {
        int in_band, all;
        count_lit(&m, k * 0.8f, &in_band, &all);
        TEST_ASSERT_GREATER_THAN_INT(15, all);
        TEST_ASSERT_EQUAL_INT(all, in_band);
    }
}

/* A bar lights LEDs up to its height, from the brim. */
static void test_bar_height(void)
{
    spectrum_meter_t m;
    spectrum_meter_init(&m);
    int full, half, dummy;
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        m.bar[b] = 1.0f;
    }
    count_lit(&m, 0.0f, &dummy, &full);
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        m.bar[b] = 0.5f;
    }
    count_lit(&m, 0.0f, &dummy, &half);
    TEST_ASSERT_INT_WITHIN(full / 10, full / 2, half);
    /* The bars leave gaps between them. */
    TEST_ASSERT_LESS_THAN_INT(LED_COUNT * 8 / 10, full);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_silence_reads_silent);
    RUN_TEST(test_tone_lands_in_its_band);
    RUN_TEST(test_meter_bars);
    RUN_TEST(test_steady_hum_settles);
    RUN_TEST(test_bars_stay_in_the_room);
    RUN_TEST(test_bar_height);
    return UNITY_END();
}
