/*
 * en10_view.c: view creation and the menu view, TM2.EXE 0x00494610 / 0x0049437c (see en10_view.h).
 * Verified under emulation: tools/emu/check_en10_view.sh.
 */
#include "en10_view.h"
#include "en5_float.h"

static float rf(En2Env* e, uint32_t a) { float f; memcpy(&f, e->mem(e->user, a), 4); return f; }
static int32_t ri(En2Env* e, uint32_t a) { return (int32_t)en2_r32(e, a); }
static uint32_t chop_int(double d)
{
    d = en5_float_round(d);
    if (!(d >= -2147483648.0 && d < 2147483648.0)) return 0x80000000u;
    return (uint32_t)(int32_t)d;
}

#define VIEWS 0x00a03b70u
#define ROOT  0x00a03ac8u
#define CAMS  0x00a03ad4u

void en10_view_create(En2Env* e, uint32_t view, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
                      uint32_t a5)
{
    if ((int32_t)view >= 2) return;
    if ((int32_t)view > (int32_t)en2_r32(e, 0x00a0413cu)) en2_w32(e, 0x00a0413cu, view);
    const uint32_t v = VIEWS + view * 0x94u;
    en2_w32(e, v + 0x8c, 0);
    en2_w32(e, v + 0x90, 0);
    {
        uint32_t args[7] = {view, a0, a1, a2, a3, a4, a5};
        e->call(e->user, 0x00493ed0u, args, 7);
    }
    {
        uint32_t l = en2_r32(e, v + 0x20), t = en2_r32(e, v + 0x24);
        uint32_t args[7] = {view, en2_r32(e, 0x004e83ecu), en2_r32(e, 0x004e83f0u), l,
                            l + en2_r32(e, v + 0x18) - 1, t, t + en2_r32(e, v + 0x1c) - 1};
        e->call(e->user, 0x004ad3d4u, args, 7);
    }
    en2_w32(e, 0x00a03ae4u + view * 4, view);
    if (en2_r32(e, ROOT) == 0) en2_w32(e, ROOT, en2_call0(e, 0x00492d90u));
    if (en2_r32(e, CAMS + view * 4) == 0) en2_w32(e, CAMS + view * 4, en2_call0(e, 0x00492d90u));
    uint32_t cam = en2_r32(e, CAMS + view * 4);
    en2_w32(e, cam + 0x10, en2_r32(e, ROOT));
    /* the matrix: view normals (rows) into the camera's columns */
    static const uint8_t src[18] = {0x44, 0x48, 0x4c, 0x50, 0x54, 0x58, 0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70,
                                    0x74, 0x78, 0x7c, 0x80, 0x84, 0x88};
    static const uint8_t dst[18] = {0x50, 0x5c, 0x68, 0x54, 0x60, 0x6c, 0x58, 0x64, 0x70, 0x80, 0x8c, 0x98,
                                    0x84, 0x90, 0x9c, 0x88, 0x94, 0xa0};
    for (int k = 0; k < 9; k++) en2_w32(e, en2_r32(e, CAMS + view * 4) + dst[k], en2_r32(e, v + src[k]));
    for (uint32_t o = 0x74; o <= 0x7c; o += 4) en2_w32(e, en2_r32(e, CAMS + view * 4) + o, 0);
    for (int k = 9; k < 18; k++) en2_w32(e, en2_r32(e, CAMS + view * 4) + dst[k], en2_r32(e, v + src[k]));
    for (uint32_t o = 0xa4; o <= 0xac; o += 4) en2_w32(e, en2_r32(e, CAMS + view * 4) + o, 0);
}

uint32_t en10_view_set_border(En2Env* e, uint32_t view, uint32_t border)
{
    const uint32_t v = VIEWS + view * 0x94u;
    int32_t b = (int32_t)border < 0 ? (int32_t)border : 0;
    en2_w32(e, v + 0x8c, (uint32_t)b);
    int32_t W = ri(e, 0x004e83ecu);
    if ((int32_t)((uint32_t)b * 2 + (uint32_t)W) < 100) en2_w32(e, v + 0x8c, (uint32_t)((100 - W) / 2));
    b = ri(e, v + 0x8c) / 8 * 8;
    en2_w32(e, v + 0x8c, (uint32_t)b);
    int32_t H = ri(e, 0x004e83f0u);
    int32_t by = (int32_t)((uint32_t)b * (uint32_t)H) / ri(e, 0x004e83ecu);
    double fx = (double)rf(e, v + 0x28), fy = (double)rf(e, v + 0x2c);
    double fw = (double)rf(e, v + 0x30), fh = (double)rf(e, v + 0x34);
    double dW = (double)ri(e, 0x004e83ecu), dH = (double)ri(e, 0x004e83f0u);
    en2_w32(e, v + 0x90, chop_int((double)by * fh / fw));
    en2_w32(e, v + 0x18, chop_int(fw * dW + (double)(int32_t)(ri(e, v + 0x8c) * 2u)));
    uint32_t left = chop_int(dW * fx - (double)ri(e, v + 0x8c));
    en2_w32(e, v + 0x1c, chop_int(fh * dH + (double)(int32_t)(en2_r32(e, v + 0x90) * 2u)));
    en2_w32(e, v + 0x20, left);
    en2_w32(e, v + 0x24, chop_int(dH * fy - (double)ri(e, v + 0x90)));
    en2_w32(e, v + 0x08, (uint32_t)(ri(e, v + 0x18) / 2));
    en2_w32(e, v + 0x0c, (uint32_t)(ri(e, v + 0x1c) / 2));
    uint32_t l = en2_r32(e, v + 0x20), t = en2_r32(e, v + 0x24);
    uint32_t args[7] = {view, (uint32_t)W, en2_r32(e, 0x004e83f0u), l, l + en2_r32(e, v + 0x18) - 1, t,
                        t + en2_r32(e, v + 0x1c) - 1};
    return e->call(e->user, 0x004ad3d4u, args, 7);
}
