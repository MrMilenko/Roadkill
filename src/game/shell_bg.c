/*
 * shell_bg.c: front end TIM backgrounds (see shell_bg.h).
 */
#include "shell_bg.h"

#include "../core/blit.h"
#include "../formats/tim.h"

int bg_tim_load(uint8_t* buf, size_t size, int to_565) {
    TimBlock clut, image;
    size_t end = tim_parse_headers(buf, size, &clut, &image);
    if (!end) return -1;
    size_t px = end - image.length + 12;
    if (px + (size_t)image.w * image.h * 2 > size) return -1;
    tim_convert_pixels(buf + px, (size_t)image.w * image.h, to_565);
    return 0;
}

void bg_tim_draw(const uint8_t* buf, size_t size, TM2Display* d) {
    TimBlock clut, image;
    size_t end = tim_parse_headers(buf, size, &clut, &image);
    if (!end) return;
    /* the original copies from 0x0058a9ac, the pixels of a TIM without a palette (header 8 bytes
     * + image block header 12); all 45 disc TIMs are like that */
    const uint8_t* px = buf + end - image.length + 12;
    if (display_lock(d) != 0) d->pixels = 0;
    /* the original also asks for the screen width and height here and drops both */
    tm2_blit16(px, 0, 0, image.w, image.h, (int32_t)image.w * 2, d->pixels, 0, 0, d->pitch);
    display_unlock(d);
}
