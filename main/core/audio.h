/* Loudness to brightness: turns the microphone's level into a pulse that
 * jumps up on a loud sound and fades away after it.
 *
 * Levels are in dB below full scale. The tracker follows the quiet
 * between sounds (the floor: drops straight to a quieter level, creeps up
 * through louder ones) and the loud peaks (jumps to a louder level, sinks
 * slowly after), and places each level between the two, so it works for
 * a quiet room and loud music alike. The pulse takes that place at once
 * when it is higher and fades towards it otherwise. */
#pragma once

typedef struct {
    float floor_db;
    float peak_db;
    float pulse;
    float min_brightness;
} audio_t;

/* `min_brightness` (0 to 1) is the brightness in silence. */
void audio_init(audio_t *a, float min_brightness);

/* Level of a stretch of sound from its RMS, full scale 1.0, in dB. */
float audio_db(float rms);

/* Feed the level of the sound over the last `dt` seconds; returns the
 * pulse, 0 to 1. */
float audio_feed(audio_t *a, float level_db, float dt);

/* Brightness for the current pulse, min_brightness to 1. */
float audio_brightness(const audio_t *a);
