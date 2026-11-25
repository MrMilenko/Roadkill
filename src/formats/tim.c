/*
 * tim.c: TIM header parse and pixel conversion, ported from TM2.EXE (see tim.h).
 */
#include "tim.h"

static uint32_t rd32(const uint8_t* b, size_t o) { return b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24; }
static uint16_t rd16(const uint8_t* b, size_t o) { return (uint16_t)(b[o] | b[o + 1] << 8); }

static void block(const uint8_t* b, size_t o, TimBlock* out) {
    out->length = rd32(b, o);
    out->x = rd16(b, o + 4); out->y = rd16(b, o + 6);
    out->w = rd16(b, o + 8); out->h = rd16(b, o + 10);
}

size_t tim_parse_headers(const uint8_t* buf, size_t size, TimBlock* clut, TimBlock* image) {
    size_t o = 8;
    if (size < o + 12) return 0;
    if ((buf[4] & 8) == 0) {
        clut->length = 0; clut->x = clut->y = clut->w = clut->h = 0;
    } else {
        block(buf, o, clut);
        o += clut->length;
        if (o + 12 > size || o < 8) return 0;
    }
    block(buf, o, image);
    return o + image->length;
}

void tim_convert_pixels(uint8_t* p, size_t count, int to_565) {
    for (size_t i = 0; i < count; i++, p += 2) {
        uint16_t u = (uint16_t)(p[0] | p[1] << 8), v;
        if (!to_565) v = (uint16_t)(((u & 0x1f) << 10) | ((u & 0x7c00) >> 10) | (u & 0x83e0));
        else v = (uint16_t)((((u >> 8) & 0x7c) << 8 >> 10) | (u << 11) | ((u & 0x3e0) * 2));
        p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    }
}
