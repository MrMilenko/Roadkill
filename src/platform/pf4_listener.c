/*
 * pf4_listener.c: TM2.EXE's 3D sound listener math (docs/PLATFORM.md Part C), ported over Pf2Env
 * (pf2_env.h). Emulator diff: tools/emu/check_pf4_listener.sh.
 *
 * Globals (0x00c8500c..0x00c8505c): 0x00c8500c distance, 0x00c85010 second view's distance,
 * 0x00c85014 view matrix, 0x00c85018 view position (3 i32), 0x00c8501c relative position (3 i32),
 * 0x00c85028 position in view space (3 i32, from 0x0048cb80), 0x00c85034 divisor, 0x00c85038 /
 * 0x00c8503c lateral limits, 0x00c85040 maximum distance, 0x00c85044, 0x00c85048 / 0x00c8504c
 * clamp max / min, 0x00c85050, 0x00c85054, 0x00c85058 lateral (view space x) of the result,
 * 0x00c8505c the second view's lateral.
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define G(o) (0x00c85000u + (o))

#define F_VIEW_POS 0x00494e64u     /* (eax view) -> int32[3] */
#define F_VIEW_MATRIX 0x00494e84u  /* (eax view) -> matrix */
#define F_TRANSFORM 0x0048cb80u    /* (eax matrix, edx in vec, ebx out vec) */

/* 0x004b7fa4 sound_listener_defaults: the constants */
void pf4_sound_listener_defaults(Pf2Env* e) {
    W32(e, G(0x44), 0);
    W32(e, G(0x48), 0xd1d);
    W32(e, G(0x4c), 0xfffff2e3u);
    W32(e, G(0x38), 0x280);
    W32(e, G(0x3c), 0xfffffd80u);
    W32(e, G(0x34), 2);
    W32(e, G(0x50), 0x16b7);
    W32(e, G(0x40), 0x16b7);
    W32(e, G(0x54), 0x1f4);
}

/* the original's inline integer square root (signed compares: jl after sub) */
static uint32_t isqrt(uint32_t c) {
    uint32_t a = 0, b = 0x40000000u;
    do {
        if ((int32_t)c >= (int32_t)b && (int32_t)(c - b) >= (int32_t)a) {
            c = c - b - a;
            a = (a >> 1) | b;
        } else {
            a >>= 1;
        }
        b >>= 2;
    } while (b);
    return a;
}

static int32_t clampv(Pf2Env* e, int32_t v) {
    int32_t mx = (int32_t)R32(e, G(0x48)), mn = (int32_t)R32(e, G(0x4c));
    if (v >= mx) v = mx;
    if (v < mn) v = mn;
    return v;
}

/* the position relative to a view, in view space, divided and clamped; returns x^2 + y^2 + z^2
   (32 bit) */
static uint32_t locate(Pf2Env* e, uint32_t pos, uint32_t view, int zero, uint32_t lateral) {
    uint32_t m = pf2_call1(e, F_VIEW_POS, view);
    W32(e, G(0x18), m);
    uint32_t t = pf2_call1(e, F_VIEW_MATRIX, view);
    if (zero) W32(e, G(0x14), t);
    W32(e, G(0x1c), R32(e, pos) + R32(e, m));
    if (zero) {
        W32(e, G(0x28), 0);
        W32(e, G(0x2c), 0);
        W32(e, G(0x30), 0);
    }
    W32(e, G(0x20), R32(e, pos + 4) + R32(e, m + 4));
    W32(e, G(0x24), R32(e, pos + 8) + R32(e, m + 8));
    if (!zero) W32(e, G(0x14), t);
    pf2_call3(e, F_TRANSFORM, t, G(0x1c), G(0x28));
    int32_t dv = (int32_t)R32(e, G(0x34));
    for (uint32_t o = 0x28; o <= 0x30; o += 4) W32(e, G(o), (uint32_t)((int32_t)R32(e, G(o)) / dv));
    W32(e, G(lateral), R32(e, G(0x28)));
    for (uint32_t o = 0x28; o <= 0x30; o += 4) W32(e, G(o), (uint32_t)clampv(e, (int32_t)R32(e, G(o))));
    uint32_t x = R32(e, G(0x28)), y = R32(e, G(0x2c)), z = R32(e, G(0x30));
    return x * x + y * y + z * z;
}

/* 0x004b8010 sound_listener_locate(eax int32[3] position, edx view): view != -1: distance from
   that view -> 0x00c8500c, lateral -> 0x00c85058. -1 (split screen): views 0 and 1; the nearer
   distance (unsigned) and its lateral are kept. Returns the distance; in the split case view 1's
   distance, or, when view 1 is nearer, its lateral (eax reused: ORIGINAL BUG). */
uint32_t pf4_sound_listener_locate(Pf2Env* e, uint32_t pos, uint32_t view) {
    if (view != 0xffffffffu) {
        uint32_t d2 = locate(e, pos, view, 0, 0x58);
        W32(e, G(0x0c), d2);
        uint32_t d = isqrt(d2);
        W32(e, G(0x0c), d);
        return d;
    }
    uint32_t d2 = locate(e, pos, 0, 0, 0x58);
    W32(e, G(0x0c), d2);
    uint32_t d0 = isqrt(d2);
    W32(e, G(0x0c), d0);
    uint32_t d12 = locate(e, pos, 1, 1, 0x5c);
    W32(e, G(0x10), d12);
    uint32_t d1 = isqrt(d12);
    W32(e, G(0x10), d1);
    uint32_t ret = d1;
    if (d1 < d0) {
        d0 = d1;
        ret = R32(e, G(0x5c));          /* eax is reused for the lateral copy */
        W32(e, G(0x58), ret);
    }
    W32(e, G(0x0c), d0);
    return ret;
}
