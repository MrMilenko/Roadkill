/*
 * pf4_fog.c: TM2.EXE 0x004b4e98 d3d_clear_fog (hardware 3D path; docs/PLATFORM.md Part B), ported
 * over Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf4_fog.sh. x87 at 0x127f: the sizes are
 * W + c and c + H in double (c = the double at 0x004e369d), stored as floats; -ffp-contract=off.
 */
#include "pf2_env.h"
#include "../render/gpu3d_float.h"

#define R32 pf2_r32
#define W32 pf2_w32

/* 0x004b4e98 d3d_clear_fog(eax D): four D3DTLVERTEX (zeroed; sz 10000.0, rhw 0, color = fog color
   0x00494dc0 | 0xff000000) as a full screen quad {0,0} {W',0} {W',H'} {0,H'} with W' = W + c,
   H' = c + H; TEXTUREHANDLE 0 (cache 0x004e86b0 = 0x7fff), SHADEMODE 1 (cache 0x00c84f74),
   set_blend 3, DrawPrimitive(TRIANGLEFAN, TLVERTEX, v, 4, 0); then the quad shrunk to 2 x 2 at the
   origin drawn ten times. Returns the last DrawPrimitive result. */
uint32_t pf4_d3d_clear_fog(Pf2Env* e, uint32_t d) {
    double c;
    memcpy(&c, e->mem(e->user, 0x004e369du), 8);
    float wf = g3_st(g3_add((double)(int32_t)R32(e, d + 0x58), c));
    float hf = g3_st(g3_add(c, (double)(int32_t)R32(e, d + 0x5c)));
    uint32_t col = pf2_call0(e, 0x00494dc0u) | 0xff000000u;
    uint32_t v = e->scratch;
    pf2_zero(e, v, 0x80);
    for (uint32_t k = 0; k < 4; k++) {
        W32(e, v + 0x20 * k + 8, 0x461c4000u);
        W32(e, v + 0x20 * k + 0x10, col);
    }
    pf2_wf(e, v + 0x20, wf);
    pf2_wf(e, v + 0x40, wf);
    pf2_wf(e, v + 0x44, hf);
    pf2_wf(e, v + 0x64, hf);
    uint32_t dev = R32(e, d + 0x28);
    W32(e, 0x004e86b0u, 0x7fff);
    PF2_C2(e, PF2_IDEV2, 0x5c, dev, 1, 0);
    if (R32(e, 0x00c84f74u) != 1) {
        W32(e, 0x00c84f74u, 1);
        PF2_C2(e, PF2_IDEV2, 0x5c, R32(e, d + 0x28), 9, 1);
    }
    pf2_call2(e, 0x004b617cu, d, 3);
    uint32_t r = PF2_C5(e, PF2_IDEV2, 0x74, R32(e, d + 0x28), 6, 3, v, 4, 0);
    W32(e, v + 0x20, 0x40000000u);
    W32(e, v + 0x40, 0x40000000u);
    W32(e, v + 0x44, 0x40000000u);
    W32(e, v + 0x64, 0x40000000u);
    for (int k = 0; k < 10; k++) r = PF2_C5(e, PF2_IDEV2, 0x74, R32(e, d + 0x28), 6, 3, v, 4, 0);
    return r;
}
