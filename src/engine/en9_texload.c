/*
 * en9_texload.c: the hardware 3D texture upload with its memory budget, TM2.EXE 0x00497390
 * (see en9_texload.h). Verified under emulation: tools/emu/check_en9_texload.sh.
 */
#include "en9_texload.h"
#include "en5_float.h"

#define PAGES 0x00a30c48u
#define RECS  0x00a36c58u
#define HWTEX 0x00a2dc48u
#define FONT  0x00a2dbb4u
#define WAIT_TEXT 0x004e2f24u
#define I_wsprintfA 0x00c90658u
#define OOM1 0x8876017cu
#define OOM2 0x887602d1u

static float rf(En2Env* e, uint32_t a) { float f; memcpy(&f, e->mem(e->user, a), 4); return f; }
static void wf(En2Env* e, uint32_t a, float f) { memcpy(e->mem(e->user, a), &f, 4); }
static uint32_t fistp(double d)
{
    if (!(d >= -2147483648.0 && d < 2147483648.0)) return 0x80000000u;
    return (uint32_t)(int32_t)d;
}
/* a record's bytes after scaling: chop(bytes / (sx * sy)) */
static uint32_t scaled(En2Env* e, uint32_t r)
{
    double s = (double)rf(e, r + 0xc) * (double)rf(e, r + 0x10);
    return fistp(en5_float_round((double)(int32_t)en2_r32(e, r + 4) / s));
}
static int oom(uint32_t r) { return r == OOM1 || r == OOM2; }

uint32_t en9_textures_load(En2Env* e)
{
    const uint32_t F = e->scratch;
    if ((en2_call0(e, 0x004958acu) & 0xff) != 1) return 0;
    uint32_t res = en2_call5(e, 0x004bfe50u, 0x10, 5, 5, 5, 1);   /* 16 bit, 5 5 5, alpha */
    int32_t bpp = (int32_t)en2_call1(e, 0x004bfed0u, res) / 8;
    int32_t total = 0, budget = 0x08000000, count = 0;
    for (uint32_t i = 0; i < 0x200; i++) {
        uint32_t r = RECS + 0x18 * i, p = PAGES + 0x30 * i;
        en2_w32(e, r, i);
        wf(e, r + 0x10, 1.0f);
        wf(e, r + 0xc, 1.0f);
        if (en2_r32(e, p) != 0) {
            en2_w32(e, r + 4, en2_r32(e, p + 4) * en2_r32(e, p + 8) * (uint32_t)bpp);
            en2_call3(e, 0x004bfc88u, i, r + 0xc, r + 0x10);   /* fit the size: the scales */
            count++;
        } else {
            en2_w32(e, r + 4, 0);
        }
        uint32_t s = scaled(e, r);
        total += (int32_t)s;
        en2_w32(e, r + 8, s);
    }
    float step = (float)((double)rf(e, 0x004e2f4eu) / (double)count);   /* 255 / count */
    en2_call4(e, 0x004c1132u, RECS, 0x200, 0x18, 0x004977ccu);   /* qsort */
    uint32_t pass = 1, rr = 0;
    do {
        for (uint32_t k = 0; k < 0x3000; k += 0x18)
            if (en2_r32(e, HWTEX + k) != 0) en2_call1(e, 0x004c101cu, HWTEX + k);
        while (total > budget) {   /* double the scales round robin until it fits */
            uint32_t r = RECS + 0x18 * rr, p = PAGES + 0x30 * en2_r32(e, r);
            if (en2_r32(e, p) != 0 && en2_r32(e, p + 0x28) == 0) {
                double k2 = (double)rf(e, 0x004e2f52u);
                double a = (double)rf(e, r + 0xc) * k2, b = k2 * (double)rf(e, r + 0x10);
                wf(e, r + 0xc, (float)a);
                wf(e, r + 0x10, (float)b);
                total -= (int32_t)en2_r32(e, r + 8);
                uint32_t s = scaled(e, r);
                total += (int32_t)s;
                en2_w32(e, r + 8, s);
            }
            if (++rr >= 0x200) rr = 0;
        }
        int32_t done = 0, made = 0;
        int32_t cx = (int32_t)en2_call0(e, 0x004958d8u) / 2;
        int32_t cy = (int32_t)en2_call0(e, 0x004958e4u) / 2;
        float grey = 0.0f;
        int32_t th = (int32_t)en2_call2(e, 0x004b3a90u, FONT, WAIT_TEXT);
        int32_t y0 = cy - 2 * th, y1 = cy - th, y2 = cy + th;
        for (uint32_t k = 0; k < 0x3000; k += 0x18) {
            uint32_t r = RECS + k, page = en2_r32(e, r);
            if (en2_r32(e, PAGES + page * 0x30) != 0) {
                en2_call0(e, 0x004a8e6cu);   /* messages */
                en2_call0(e, 0x00495b14u);
                en2_call1(e, 0x00495b48u, 0);
                double v = (double)grey + (double)step;
                grey = (float)v;
                uint32_t lum = fistp(en5_float_round(v));
                uint32_t rest = fistp(en5_float_round((double)rf(e, 0x004e2f4eu) - (double)grey));
                en2_call4(e, 0x004b3b84u, FONT, rest, lum, 0);
                en2_call3(e, I_wsprintfA, F, 0x004e2f30u, pass);          /* "Pass %i" */
                en2_call4(e, 0x004b3330u, FONT, F, (uint32_t)cx, (uint32_t)y0);
                en2_call3(e, I_wsprintfA, F, 0x004e2f38u, (uint32_t)made); /* "Loading %i" */
                made++;
                en2_call4(e, 0x004b3330u, FONT, F, (uint32_t)cx, (uint32_t)y1);
                en2_call4(e, 0x004b3330u, FONT, WAIT_TEXT, (uint32_t)cx, (uint32_t)y2);
                res = en2_call4(e, 0x004bff08u, HWTEX + page * 0x18, page, r + 0xc, r + 0x10);
            }
            uint32_t s = scaled(e, r);
            en2_w32(e, r + 8, s);
            if (oom(res)) break;
            done += (int32_t)s;
        }
        if (oom(res)) budget = done - (count - made) * 0xc0;
        pass++;
    } while (oom(res) && pass < 10);
    return 0;
}
