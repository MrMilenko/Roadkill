/*
 * en7_m.c: see en7_m.h.
 */
#include "en7_m.h"

/* 0x004a3494 (PS1 pair screenLoadCardImages by the matcher; here the PCX decode of a background
 * image o: +0 width, +4 height, +8 pitch, +0x14 the PCX file (data from +0x80), +0x18 pixels,
 * +0x24 the DirectDraw surface, +0x75 planes, +0x76 bytes per plane line): the surface is cleared
 * to black (0x004b4d98 with 0x00496590(0, 0, 0)) and locked (+0x64, DDSURFACEDESC 0x6c bytes,
 * DDLOCK_WAIT; failure -> its HRESULT) for lpSurface / lPitch -> +0x18 / +0x08; then the RLE runs
 * (0xc0 | count, value; else one value) are ORed in as 16 bit pixels, plane by plane, each value's
 * top 5 bits moved by the plane's shift (the 3 entry table at [0x00ad3200]: left for a positive
 * shift, right otherwise), runs clipped at the width; then Unlock (+0x80). Returns 0. */
uint32_t en7_pcx_to_surface(En2Env* e, uint32_t o)
{
    const uint32_t D = e->scratch;
    uint32_t plane = 0, row = 0;
    uint32_t black = en2_call3(e, 0x00496590u, 0, 0, 0);
    en2_call3(e, 0x004b4d98u, EN2_DISPLAY, black & 0xffff, en2_r32(e, o + 0x24));
    en2_call3(e, 0x004bf830u, D, 0, 0x6c);
    en2_w32(e, D, 0x6c);
    uint32_t s = en2_r32(e, o + 0x24);
    uint32_t v[5] = {s, 0, D, 1, 0};
    uint32_t r = e->call(e->user, en2_r32(e, en2_r32(e, s) + 0x64), v, 5);
    if (r != 0) return r;
    en2_w32(e, o + 0x18, en2_r32(e, D + 0x24));
    en2_w32(e, o + 8, en2_r32(e, D + 0x10));
    int32_t shift = (int32_t)en2_r32(e, en2_r32(e, 0x00ad3200u) + plane * 4);
    uint32_t src = en2_r32(e, o + 0x14) + 0x80;
    uint32_t p = en2_r32(e, o + 0x18);
    int32_t x = 0;
    while ((int32_t)row < (int32_t)en2_r32(e, o + 4)) {
        uint8_t b = en2_r8(e, src++);
        int32_t n;
        if ((b & 0xc0) == 0xc0) {
            n = b & 0x3f;
            src++;
            b = en2_r8(e, src - 1);
        } else {
            n = 1;
        }
        uint32_t c = b & 0xf8;
        c = shift > 0 ? c << ((uint32_t)shift & 31) : c >> ((uint32_t)(-shift) & 31);
        x += n;
        if (x >= (int32_t)en2_r32(e, o)) n -= x - (int32_t)en2_r32(e, o);
        for (; n > 0; n--, p += 2) en2_w16(e, p, (uint16_t)(en2_r16(e, p) | c));
        if (x >= (int32_t)en2_r16(e, o + 0x76)) {
            plane++;
            if ((int32_t)en2_r8(e, o + 0x75) <= (int32_t)plane) {
                row++;
                plane = 0;
            }
            shift = (int32_t)en2_r32(e, en2_r32(e, 0x00ad3200u) + plane * 4);
            p = en2_r32(e, o + 0x18) + row * en2_r32(e, o + 8);
            x = 0;
        }
    }
    en2_call2(e, en2_r32(e, en2_r32(e, s) + 0x80), en2_r32(e, o + 0x24), 0);
    return 0;
}
