/*
 * tim.h: PS1 TIM images as TM2.EXE reads them.
 *   tim_parse_headers   0x00474060: palette block header (if flags bit 3) and image block header
 *   tim_convert_pixels  0x004978d8: in place PS1 15 bit BGR -> display 555 or 565
 * All 45 TIMs on the disc are flags 2 (16 bit direct, no palette).
 * Verified against the original under emulation (tools/emu/check_tim.sh).
 */
#ifndef TM2_TIM_H
#define TM2_TIM_H

#include <stddef.h>
#include <stdint.h>

/* A TIM block header: byte length (including these 12 bytes), VRAM x/y, width (in 16 bit units)/height. */
typedef struct {
    uint32_t length;
    uint16_t x, y;
    uint16_t w, h;
} TimBlock;

/* Fills clut (all zero when the file has no palette) and image from the TIM at buf + 0.
 * Returns the offset just past the image block, or 0 if the buffer is too small.
 * Pixels start at image block offset + 12, i.e. returned offset - image.length + 12. */
size_t tim_parse_headers(const uint8_t* buf, size_t size, TimBlock* clut, TimBlock* image);

/* Converts count 16 bit pixels in place. to_565 = the original's FUN_00496284() == 1. */
void tim_convert_pixels(uint8_t* pixels, size_t count, int to_565);

#endif /* TM2_TIM_H */
