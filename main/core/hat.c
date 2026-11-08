#include "hat.h"

#include <math.h>
#include <string.h>

#include "color.h"

#define TWO_PI 6.2831853f

/* The LEDs look at the fluid half a cell inside the wall, where cells
 * along the wall have their centres. */
#define SAMPLE_INSET 0.5f

/* Density thresholds for lighting an LED and for turning it off again
 * (rest density is FLUID_PARTICLES_PER_CELL). */
#define LIGHT_ON  1.0f
#define LIGHT_OFF 0.4f

void frame_clear(frame_t *f)
{
    memset(f->rgb, 0, sizeof f->rgb);
}

void frame_set_led(frame_t *f, int led, color_t color)
{
    if (led < 0 || led >= LED_COUNT) {
        return;
    }
    f->rgb[led * 3] = color.r;
    f->rgb[led * 3 + 1] = color.g;
    f->rgb[led * 3 + 2] = color.b;
}

/* LEDs per turn where the strip starts, and the change in it per turn
 * on from there. */
#define START_LEDS  (HAT_START_AT_TOP ? HAT_LEDS_PER_TURN_TOP : HAT_LEDS_PER_TURN_BOTTOM)
#define END_LEDS    (HAT_START_AT_TOP ? HAT_LEDS_PER_TURN_BOTTOM : HAT_LEDS_PER_TURN_TOP)
#define LEDS_CHANGE ((END_LEDS - START_LEDS) / HAT_TURNS)

/* Turns from LED 0 to LED `led`. The LEDs up to t turns on number
 * START_LEDS * t + LEDS_CHANGE * t^2 / 2; this solves that for t (in the
 * form that stays exact when LEDS_CHANGE is 0). */
static float turns(int led)
{
    const float i = (float)led;
    return 2.0f * i / (START_LEDS + sqrtf(START_LEDS * START_LEDS + 2.0f * LEDS_CHANGE * i));
}

float hat_led_at_turns(float t)
{
    return START_LEDS * t + 0.5f * LEDS_CHANGE * t * t;
}

float hat_led_angle(int led)
{
    const float t = turns(led);
    const float frac = t - floorf(t);
    const float a = TWO_PI * (HAT_CLOCKWISE ? 1.0f - frac : frac);
    return a >= TWO_PI ? 0.0f : a;
}

float hat_led_height(int led)
{
    const float up = (0.5f + turns(led)) * HAT_ROW_PITCH_CELLS;
    return HAT_START_AT_TOP ? HAT_HEIGHT - up : up;
}

vec3_t hat_led_pos(int led)
{
    const float a = hat_led_angle(led);
    const float z = hat_led_height(led);
    const float r = hat_radius_at(z);
    return v3(FLUID_NX / 2.0f + r * cosf(a), FLUID_NY / 2.0f + r * sinf(a), z);
}

void hat_view_init(hat_view_t *v)
{
    memset(v, 0, sizeof *v);
}

void hat_view_frame(hat_view_t *v, const fluid_t *f, uint32_t hue, float brightness, frame_t *frame)
{
    frame_clear(frame);
    for (int i = 0; i < LED_COUNT; i++) {
        const float a = hat_led_angle(i);
        const float z = hat_led_height(i);
        const float r = hat_radius_at(z) - SAMPLE_INSET;
        const vec3_t at = v3(FLUID_NX / 2.0f + r * cosf(a), FLUID_NY / 2.0f + r * sinf(a), z);
        float tint;
        const float density = fluid_sample(f, at, &tint);
        v->lit[i] = density >= (v->lit[i] ? LIGHT_OFF : LIGHT_ON);
        if (v->lit[i]) {
            const int h = (int)hue + 360 + (int)lrintf(tint);
            frame_set_led(frame, i, color_scale(color_from_hue((uint32_t)h), brightness));
        }
    }
}

/* Calibration colours, at about the fluid's brightness. */
static const color_t CAL_LINES[4] = {{60, 0, 0}, {0, 60, 0}, {0, 0, 60}, {30, 30, 0}};
static const color_t CAL_DOWN = {20, 20, 20};
/* Tilt (horizontal g) below which the white band stays off, and its half
 * width. */
#define CAL_MIN_TILT 0.1f
#define CAL_BAND     (TWO_PI / 16.0f)

/* Angle from `a` to `b`, in [-pi, pi). */
static float angle_between(float a, float b)
{
    return fmodf(b - a + 1.5f * TWO_PI, TWO_PI) - 0.5f * TWO_PI;
}

void hat_calibration_frame(frame_t *frame, vec3_t accel)
{
    frame_clear(frame);
    const bool tilted = accel.x * accel.x + accel.y * accel.y > CAL_MIN_TILT * CAL_MIN_TILT;
    const float down = atan2f(accel.y, accel.x);
    for (int i = 0; i < LED_COUNT; i++) {
        if (tilted && fabsf(angle_between(down, hat_led_angle(i))) < CAL_BAND) {
            frame_set_led(frame, i, CAL_DOWN);
        }
    }
    /* Each line's angle is a share of a turn from LED 0, the other way
     * round when the strip winds clockwise; on every turn the LED nearest
     * it lights. */
    for (int q = 0; q < 4; q++) {
        const float share = HAT_CLOCKWISE ? (float)((4 - q) % 4) / 4.0f : (float)q / 4.0f;
        for (int k = 0;; k++) {
            const int led = (int)floorf(hat_led_at_turns((float)k + share) + 0.5f);
            if (led >= LED_COUNT) {
                break;
            }
            frame_set_led(frame, led, CAL_LINES[q]);
        }
    }
}
