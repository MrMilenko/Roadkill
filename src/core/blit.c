/*
 * blit.c: 16 bit rectangle copy, ported from TM2.EXE 0x00497854 (see blit.h).
 */
#include "blit.h"

#include <string.h>

int tm2_blit16(const uint8_t* src, int32_t sx, int32_t sy, uint32_t w, int32_t h, int32_t src_pitch,
               uint8_t* dst, int32_t dx, int32_t dy, int32_t dst_pitch) {
    if (!src || !dst) return 0;
    const uint8_t* s = src + (intptr_t)sy * src_pitch + (intptr_t)sx * 2;
    uint8_t* d = dst + (intptr_t)dy * dst_pitch + (intptr_t)dx * 2;
    size_t row = (size_t)((w & 0x7fffffffu) >> 1) * 4 + ((w * 2) & 3);
    for (int32_t y = 0; y < h; y++) {
        memcpy(d, s, row);
        s += src_pitch;
        d += dst_pitch;
    }
    return 1;
}
