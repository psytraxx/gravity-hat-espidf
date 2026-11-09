#include <math.h>
#include <stdbool.h>

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

/* Light of the frame (channels added up) on LEDs from `lo` to `hi` of the
 * hat's height. */
static float light_between(const spectrum_meter_t *m, float lo, float hi)
{
    static frame_t frame;
    spectrum_frame(m, 0.0f, &frame);
    float sum = 0.0f;
    for (int i = 0; i < LED_COUNT; i++) {
        const float up = hat_led_height(i) / HAT_HEIGHT;
        if (up >= lo && up < hi) {
            sum += frame.rgb[3 * i] + frame.rgb[3 * i + 1] + frame.rgb[3 * i + 2];
        }
    }
    return sum;
}

/* One full bar stands in the same place in the room whichever way the
 * hat faces, and straight: on every turn above the base, its light, as
 * the eye sees it, is centred on the bar's angle to within a tenth of an
 * LED, though the LEDs of one turn do not sit above those of the next. */
static void test_bars_stand_straight_in_the_room(void)
{
    spectrum_meter_t m;
    spectrum_meter_init(&m);
    m.bar[0] = 1.0f;
    const float centre = PI / SPECTRUM_BANDS;
    const float led = 2.0f * PI / HAT_LEDS_PER_TURN_BOTTOM;
    static frame_t frame;
    for (int k = 0; k < 16; k++) {
        const float yaw = k * 0.41f;
        spectrum_frame(&m, yaw, &frame);
        /* Each pass of the spiral through the bar is a run of lit LEDs:
         * the light-weighted mean of their angles from the centre. The
         * eye sees about the square root of an LED's power, as the
         * frame's coverage gamma assumes. */
        int passes = 0;
        float sum = 0.0f, weight = 0.0f;
        for (int i = 0; i <= LED_COUNT; i++) {
            const float light = i < LED_COUNT
                                    ? frame.rgb[3 * i] + frame.rgb[3 * i + 1] + frame.rgb[3 * i + 2]
                                    : 0.0f;
            const bool above_base =
                i < LED_COUNT && hat_led_height(i) / HAT_ROW_PITCH_CELLS - 0.5f >= 1.0f;
            if (light > 0.0f && above_base) {
                const float d = fmodf(hat_led_angle(i) + yaw - centre + 3.0f * PI, 2.0f * PI) - PI;
                TEST_ASSERT_LESS_THAN_FLOAT(PI / SPECTRUM_BANDS + led, fabsf(d));
                sum += sqrtf(light) * d;
                weight += sqrtf(light);
            } else if (weight > 0.0f) {
                TEST_ASSERT_FLOAT_WITHIN(0.1f * led, 0.0f, sum / weight);
                passes++;
                sum = weight = 0.0f;
            }
        }
        TEST_ASSERT_GREATER_OR_EQUAL_INT((int)HAT_TURNS - 1, passes);
    }
}

/* A bar lights the hat up to its height, from the brim, and leaves gaps
 * between the bars. */
static void test_bar_height(void)
{
    spectrum_meter_t m;
    spectrum_meter_init(&m);
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        m.bar[b] = 1.0f;
    }
    const float full_low = light_between(&m, 0.2f, 0.45f);
    const float full_high = light_between(&m, 0.55f, 1.0f);
    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        m.bar[b] = 0.5f;
    }
    TEST_ASSERT_FLOAT_WITHIN(full_low * 0.02f, full_low, light_between(&m, 0.2f, 0.45f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, light_between(&m, 0.55f, 1.0f));
    /* The bars light less than the whole wall would at full power. */
    const int leds_high = (int)(LED_COUNT * 0.45f);
    TEST_ASSERT_LESS_THAN_FLOAT(0.8f * FLUID_LEVEL * leds_high, full_high);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.4f * FLUID_LEVEL * leds_high, full_high);
}

/* In silence the bottom turn glows dimly all the way round and nothing
 * above it lights. */
static void test_silence_shows_the_base(void)
{
    spectrum_meter_t m;
    spectrum_meter_init(&m);
    static frame_t frame;
    spectrum_frame(&m, 0.0f, &frame);
    int base = 0, above = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        const int sum = frame.rgb[3 * i] + frame.rgb[3 * i + 1] + frame.rgb[3 * i + 2];
        if (sum == 0) {
            continue;
        }
        TEST_ASSERT_LESS_THAN_INT(FLUID_LEVEL / 2, sum);
        if (i < (int)HAT_LEDS_PER_TURN_BOTTOM) {
            base++;
        } else {
            above++;
        }
    }
    TEST_ASSERT_GREATER_THAN_INT(HAT_LEDS_PER_TURN_BOTTOM / 2, base);
    TEST_ASSERT_LESS_THAN_INT(4, above);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_silence_reads_silent);
    RUN_TEST(test_tone_lands_in_its_band);
    RUN_TEST(test_meter_bars);
    RUN_TEST(test_steady_hum_settles);
    RUN_TEST(test_bars_stand_straight_in_the_room);
    RUN_TEST(test_bar_height);
    RUN_TEST(test_silence_shows_the_base);
    return UNITY_END();
}
