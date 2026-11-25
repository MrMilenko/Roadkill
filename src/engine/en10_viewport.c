/*
 * en10_viewport.c: the view's projection set up, TM2.EXE 0x00493ed0 (see en10_viewport.h).
 * Verified under emulation: tools/emu/check_en10_viewport.sh.
 */
#include <math.h>
#include "en10_viewport.h"
#include "en5_float.h"

/* Roadkill's display.widescreen (src/host/qol/display.c sets it when the switch is on): the
 * screen's aspect, 0 = off (the original). A variable here, so this port still builds and checks
 * on its own (the emulator check links no host; there it stays 0). */
double en10_widescreen_aspect = 0;

static float bits_f(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static float rf(En2Env* e, uint32_t a) { float f; memcpy(&f, e->mem(e->user, a), 4); return f; }
static void wf(En2Env* e, uint32_t a, float f) { memcpy(e->mem(e->user, a), &f, 4); }
static double rd(En2Env* e, uint32_t a) { double d; memcpy(&d, e->mem(e->user, a), 8); return d; }
static int32_t ri(En2Env* e, uint32_t a) { return (int32_t)en2_r32(e, a); }
/* fistp of a chopped value: out of range / NaN -> the integer indefinite */
static uint32_t chop_int(double d)
{
    d = en5_float_round(d);
    if (!(d >= -2147483648.0 && d < 2147483648.0)) return 0x80000000u;
    return (uint32_t)(int32_t)d;
}

void en10_level_setup_viewport(En2Env* e, uint32_t view, uint32_t fx, uint32_t fy, uint32_t fw, uint32_t fh,
                               uint32_t fov, uint32_t unused)
{
    (void)unused;
    const uint32_t v = 0x00a03b70u + view * 0x94u;
    en2_w32(e, v + 0x10, en2_r32(e, 0x004e83ecu));
    en2_w32(e, v + 0x14, en2_r32(e, 0x004e83f0u));
    en2_w32(e, v + 0x28, fx);
    en2_w32(e, v + 0x2c, fy);
    en2_w32(e, v + 0x30, fw);
    en2_w32(e, v + 0x34, fh);
    en2_w32(e, v + 0x00, fov);
    /* the viewport rectangle in pixels with the border (+0x8c / +0x90) on both sides; the
     * screen height goes through a float (fild, fstp m32), the width does not */
    double sw = (double)ri(e, 0x004e83ecu);
    float shf = (float)(double)ri(e, 0x004e83f0u);
    double wpix = (double)bits_f(fw) * sw;
    double hpix = (double)bits_f(fh) * (double)shf;
    double bx = (double)ri(e, v + 0x8c), by = (double)ri(e, v + 0x90);
    en2_w32(e, v + 0x18, chop_int((double)(int32_t)(en2_r32(e, v + 0x8c) * 2) + wpix));
    en2_w32(e, v + 0x1c, chop_int((double)(int32_t)(en2_r32(e, v + 0x90) * 2) + hpix));
    en2_w32(e, v + 0x20, chop_int(sw * (double)bits_f(fx) - bx));
    en2_w32(e, v + 0x24, chop_int((double)bits_f(fy) * (double)shf - by));
    en2_w32(e, v + 0x08, chop_int(wpix * rd(e, 0x004e2e98u) + rd(e, 0x004e2ea0u)));   /* w / 2 - 1 */
    en2_w32(e, v + 0x0c, (uint32_t)(ri(e, v + 0x1c) / 2));
    /* the projection: distance from the field of view, the vertical angle from the aspect */
    double ha = (double)ri(e, v + 0x00) * rd(e, 0x004e2e90u);   /* half angle in radians */
    wf(e, 0x00a04144u, (float)ha);
    double hx = (double)rf(e, 0x00a04144u);
    if (en10_widescreen_aspect > 0) {
        /* Roadkill's display.widescreen (src/host/qol/display.h), not the original: on a screen
         * wider than 4:3 the field of view is the original's on the 4:3 part, widened to the
         * screen's width: the vertical angle (from the aspect below) stays the original's */
        double W = (double)ri(e, 0x004e83ecu), H = (double)ri(e, 0x004e83f0u);
        if (W * 3.0 > H * 4.0) {
            wf(e, 0x00a04144u, (float)atan(tan(hx) * (W / H) * 0.75));
            hx = (double)rf(e, 0x00a04144u);
        }
    }
    double tx = tan(hx);
    double k = rd(e, 0x004e2eb8u);   /* 4096 */
    double va = atan((double)ri(e, v + 0x1c) * tx / (double)ri(e, v + 0x18));
    double dist = k / tx;
    en2_w32(e, v + 0x04, chop_int(va * rd(e, 0x004e2ea8u) * rd(e, 0x004e2eb0u)));
    en2_w32(e, v + 0x38, chop_int(dist));
    double hy = (double)ri(e, v + 0x04) * rd(e, 0x004e2e90u);
    wf(e, 0x00a04148u, (float)hy);
    en2_w32(e, v + 0x40, chop_int(k / tan(hy)));
    double cxa = cos(hx), cya = cos(hy), sxa = sin(hx), sya = sin(hy);
    en2_w32(e, v + 0x3c, 0xfff);
    en2_w32(e, v + 0x4c, 0);
    en2_w32(e, v + 0x58, 0);
    en2_w32(e, v + 0x5c, 0);
    en2_w32(e, v + 0x60, 0xfff);
    wf(e, 0x00a0414cu, (float)cxa);
    en2_w32(e, v + 0x64, 0);
    wf(e, 0x00a04154u, (float)cya);
    en2_w32(e, v + 0x68, 0);
    wf(e, 0x00a04150u, (float)sxa);
    en2_w32(e, v + 0x74, 0);
    en2_w32(e, v + 0x80, 0);
    wf(e, 0x00a04158u, (float)sya);
    en2_w32(e, v + 0x84, 0xfffff001u);
    uint32_t cx4 = chop_int(cxa * k), sx4 = chop_int(sxa * k), cy4 = chop_int(cya * k), sy4 = chop_int(k * sya);
    en2_w32(e, 0x00a0415cu, cx4);
    en2_w32(e, 0x00a04160u, sx4);
    en2_w32(e, v + 0x44, 0u - cx4);
    en2_w32(e, v + 0x48, sx4);
    en2_w32(e, v + 0x50, cx4);
    en2_w32(e, v + 0x54, sx4);
    en2_w32(e, 0x00a04168u, sy4);
    en2_w32(e, 0x00a04164u, cy4);
    en2_w32(e, v + 0x6c, sy4);
    en2_w32(e, v + 0x88, 0);
    en2_w32(e, v + 0x70, 0u - cy4);
    en2_w32(e, v + 0x78, sy4);
    en2_w32(e, v + 0x7c, cy4);
}
