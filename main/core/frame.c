#include "frame.h"

#include <string.h>

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
