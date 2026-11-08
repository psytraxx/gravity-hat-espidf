#include "audio.h"

#include <math.h>

/* Quietest level told apart, and where the tracker starts. */
#define SILENCE_DB -100.0f
#define START_DB   -60.0f
/* The floor creeps up this fast through louder sound, and drops at once. */
#define FLOOR_RISE_DB_PER_S 3.0f
/* Peaks sink this fast after a loud sound, and jump up at once. */
#define PEAK_FALL_DB_PER_S 6.0f
/* The peak stays at least this far above the floor, so steady noise does
 * not read as loud. */
#define MIN_RANGE_DB 12.0f
/* Levels this close to the floor count as quiet. */
#define GATE_DB 3.0f
/* The pulse fades with this time constant. */
#define RELEASE_S 0.15f

void audio_init(audio_t *a, float min_brightness)
{
    a->floor_db = START_DB;
    a->peak_db = START_DB + MIN_RANGE_DB;
    a->pulse = 0.0f;
    a->min_brightness = min_brightness;
}

float audio_db(float rms)
{
    if (!(rms > 0.0f)) {
        return SILENCE_DB;
    }
    const float db = 20.0f * log10f(rms);
    return db < SILENCE_DB ? SILENCE_DB : db;
}

float audio_feed(audio_t *a, float level_db, float dt)
{
    if (level_db < a->floor_db) {
        a->floor_db = level_db;
    } else {
        a->floor_db = fminf(level_db, a->floor_db + FLOOR_RISE_DB_PER_S * dt);
    }
    if (level_db > a->peak_db) {
        a->peak_db = level_db;
    } else {
        a->peak_db -= PEAK_FALL_DB_PER_S * dt;
    }
    a->peak_db = fmaxf(a->peak_db, a->floor_db + MIN_RANGE_DB);

    const float range = a->peak_db - a->floor_db - GATE_DB;
    float level = (level_db - a->floor_db - GATE_DB) / range;
    level = level < 0.0f ? 0.0f : level > 1.0f ? 1.0f : level;

    a->pulse *= expf(-dt / RELEASE_S);
    if (level > a->pulse) {
        a->pulse = level;
    }
    return a->pulse;
}

float audio_brightness(const audio_t *a)
{
    return a->min_brightness + (1.0f - a->min_brightness) * a->pulse;
}
