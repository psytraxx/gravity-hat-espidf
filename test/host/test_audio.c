#include "audio.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define DT 0.018f

static audio_t a;

static float run(float db, int steps)
{
    float p = 0.0f;
    for (int i = 0; i < steps; i++) {
        p = audio_feed(&a, db, DT);
    }
    return p;
}

static void test_db_of_rms(void)
{
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, audio_db(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -20.0f, audio_db(0.1f));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -100.0f, audio_db(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -100.0f, audio_db(1e-9f));
}

/* Silence stays dark, at the minimum brightness. */
static void test_silence_stays_at_minimum(void)
{
    audio_init(&a, 0.25f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, run(-100.0f, 200));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, audio_brightness(&a));
}

/* A clap in a quiet room: full brightness at once, faded within half a
 * second. */
static void test_loud_sound_pulses_then_fades(void)
{
    audio_init(&a, 0.25f);
    run(-70.0f, 200);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, run(-20.0f, 1));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, audio_brightness(&a));
    const float after = run(-70.0f, 28);
    TEST_ASSERT_LESS_THAN_FLOAT(0.05f, after);
}

/* Steady noise, however loud, settles back to quiet once the floor has
 * caught up with it. */
static void test_steady_noise_settles(void)
{
    audio_init(&a, 0.25f);
    run(-70.0f, 100);
    run(-30.0f, 1000);
    TEST_ASSERT_LESS_THAN_FLOAT(0.05f, run(-30.0f, 1));
}

/* Over loud music, beats still pulse: a beat 12 dB above the music
 * between them lights far brighter than the music does. */
static void test_beats_over_music_pulse(void)
{
    audio_init(&a, 0.25f);
    float beat = 0.0f, between = 0.0f;
    for (int bar = 0; bar < 40; bar++) {
        beat = run(-18.0f, 3);
        between = run(-30.0f, 25);
    }
    TEST_ASSERT_GREATER_THAN_FLOAT(0.7f, beat);
    TEST_ASSERT_LESS_THAN_FLOAT(0.3f, between);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_db_of_rms);
    RUN_TEST(test_silence_stays_at_minimum);
    RUN_TEST(test_loud_sound_pulses_then_fades);
    RUN_TEST(test_steady_noise_settles);
    RUN_TEST(test_beats_over_music_pulse);
    return UNITY_END();
}
