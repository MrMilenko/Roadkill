/*
 * pf6_d3d.c: TM2.EXE's Direct3D materials and the refresh rate measure (docs/PLATFORM.md Part B),
 * ported over Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf6_d3d.sh. x87 at 0x127f
 * (color / 255 in double, stored as float); -ffp-contract=off.
 */
#include "pf2_env.h"
#include "../render/gpu3d_float.h"

#define R32 pf2_r32
#define W32 pf2_w32

/* 0x004b6428 d3d_set_background_color(eax D, dl r, bl g, cl b): no background material (+0x44):
   E_FAIL; no viewport (+0x2c): 0. D3DMATERIAL {0x50, diffuse r, g, b = byte * c (c = the double
   0x004e36b1, 1 / 255), alpha 0, dwRampSize 1} -> IDirect3DMaterial2::SetMaterial (returned) */
uint32_t pf6_d3d_set_background_color(Pf2Env* e, uint32_t d, uint32_t r, uint32_t g, uint32_t b) {
    if (R32(e, d + 0x44) == 0) return 0x80004005u;
    if (R32(e, d + 0x2c) == 0) return 0;
    double c;
    memcpy(&c, e->mem(e->user, 0x004e36b1u), 8);
    uint32_t m = e->scratch;
    pf2_zero(e, m, 0x50);
    W32(e, m, 0x50);
    pf2_wf(e, m + 4, g3_st(g3_mul((double)(r & 0xff), c)));
    pf2_wf(e, m + 8, g3_st(g3_mul((double)(g & 0xff), c)));
    W32(e, m + 0x4c, 1);
    pf2_wf(e, m + 0xc, g3_st(g3_mul(c, (double)(b & 0xff))));
    return PF2_C1(e, PF2_IMAT2, 0xc, R32(e, d + 0x44), m);
}

/* 0x004b6320 d3d_create_materials(eax D): background material (IDirect3D2::CreateMaterial into +0x44;
   failure returned) in the fog color 0x00494dc0 (0x00rrggbb) via set_background_color, its handle
   (GetHandle on the device) set as the viewport background; the default material +0x48: white
   diffuse and ambient, dwRampSize 16, its handle -> +0x4c; returns 0 */
uint32_t pf6_d3d_create_materials(Pf2Env* e, uint32_t d) {
    uint32_t r = PF2_C2(e, PF2_ID3D2, 0x14, R32(e, d + 0x24), d + 0x44, 0);
    if (r) return r;
    uint32_t fog = pf2_call0(e, 0x00494dc0u);
    pf6_d3d_set_background_color(e, d, (fog >> 16) & 0xff, (fog >> 8) & 0xff, fog & 0xff);
    uint32_t hp = e->scratch + 0x60;
    PF2_C2(e, PF2_IMAT2, 0x14, R32(e, d + 0x44), R32(e, d + 0x28), hp);
    PF2_C1(e, PF2_IVIEW2, 0x20, R32(e, d + 0x2c), R32(e, hp));
    r = PF2_C2(e, PF2_ID3D2, 0x14, R32(e, d + 0x24), d + 0x48, 0);
    if (r) return r;
    uint32_t m = e->scratch;
    pf2_zero(e, m, 0x50);
    W32(e, m, 0x50);
    for (uint32_t o = 4; o <= 0x1c; o += 4) if (o != 0x10) W32(e, m + o, 0x3f800000u);
    W32(e, m + 0x4c, 0x10);
    W32(e, m + 0x24, 0);
    W32(e, m + 0x28, 0);
    W32(e, m + 0x2c, 0);
    W32(e, m + 0x44, 0);
    PF2_C1(e, PF2_IMAT2, 0xc, R32(e, d + 0x48), m);
    PF2_C2(e, PF2_IMAT2, 0x14, R32(e, d + 0x48), R32(e, d + 0x28), hp);
    W32(e, d + 0x4c, R32(e, hp));
    return 0;
}

/* 0x004b598c ddraw_measure_refresh(eax D): no IDirectDraw2: E_FAIL. +0x68 = 0;
   IDirectDraw2::GetMonitorFrequency(&+0x68); on failure, in full screen (+0x70 == 1): clear
   (0x00495c94(0)), present 0x00495b14, t0 = 0x00493038 (time), then 50 x (present, t): average =
   (t - t0) / n (unsigned div), +0x68 = 1000000 / the last average; windowed: 0. Returns +0x68. */
uint32_t pf6_ddraw_measure_refresh(Pf2Env* e, uint32_t d) {
    if (R32(e, d + 8) == 0) return 0x80004005u;
    W32(e, d + 0x68, 0);
    if (PF2_C1(e, PF2_IDD2, 0x3c, R32(e, d + 8), d + 0x68) != 0) {
        if (R32(e, d + 0x70) == 1) {
            W32(e, d + 0x68, 0);
            pf2_call1(e, 0x00495c94u, 0);
            pf2_call0(e, 0x00495b14u);
            uint32_t t0 = pf2_call0(e, 0x00493038u), avg = 0;
            for (uint32_t n = 1; n <= 0x32; n++) {
                pf2_call0(e, 0x00495b14u);
                avg = (pf2_call0(e, 0x00493038u) - t0) / n;
            }
            W32(e, d + 0x68, 1000000u / avg);
        } else {
            W32(e, d + 0x68, 0);
        }
    }
    return R32(e, d + 0x68);
}
