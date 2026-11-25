/*
 * pf4_d3d.c: TM2.EXE's Direct3D device setup / teardown and DirectDraw release (docs/PLATFORM.md
 * Parts A / B), ported over Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf4_d3d.sh.
 * x87 at 0x127f (viewport scale in d3d_create_device); -ffp-contract=off.
 */
#include "pf2_env.h"
#include "../render/gpu3d_float.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define COOP 0x004e86acu
#define HW3D 0x00c85008u

/* 0x004b5bf0 d3d_init(eax D): render state cache 0x00c84f74 (0x6c bytes) zeroed then the defaults
   (SHADEMODE 1, FILLMODE 3, ...); D+0x24..+0x30 = 0; IDirectDraw2::QueryInterface(IID_IDirect3D2
   0x004e5564, &D+0x24); returns its result */
uint32_t pf4_d3d_init(Pf2Env* e, uint32_t d) {
    static const uint32_t v[][2] = {{0xc84f7c, 0}, {0xc84f80, 0}, {0xc84f84, 0}, {0xc84f90, 0}, {0xc84f94, 0},
                                    {0xc84f98, 0}, {0xc84f9c, 4}, {0xc84fa0, 4}, {0xc84fa8, 0}, {0xc84fd4, 0},
                                    {0xc84f74, 1}, {0xc84f78, 3}, {0xc84f88, 1}, {0xc84f8c, 1}, {0xc84fa4, 2},
                                    {0xc84fac, 1}};
    pf2_zero(e, 0x00c84f74u, 0x6c);
    for (unsigned k = 0; k < sizeof v / sizeof v[0]; k++) W32(e, v[k][0], v[k][1]);
    for (uint32_t o = 0x24; o <= 0x30; o += 4) W32(e, d + o, 0);
    return PF2_C2(e, PF2_IDD2, 0, R32(e, d + 8), 0x004e5564u, d + 0x24);
}

/* 0x004b5ce8 d3d_get_device_caps(eax D, edx block): two D3DDEVICEDESC (0xcc, zeroed, dwSize) at
   +0 (hardware) and +0xcc (software); IDirect3DDevice2::GetCaps; failure: its result. Then +0x198 =
   hardware (dcmColorModel != 0) and from that description (else the software one): +0x1a4 =
   +0x84 & 0x20, +0x1b0 / +0x1b4 / +0x1a8 / +0x1ac = +0xac / +0xb0 / +0xb4 / +0xb8, +0x1b8 = byte
   +0x0d & 0x10, +0x1bc / +0x1c0 / +0x1c4 = byte +0x81 & 0x20 / 0x10 / 0x40, 0x00c84fe0 = byte
   +0x6d & 0x80 (as 0 / 1); returns 0 */
uint32_t pf4_d3d_get_device_caps(Pf2Env* e, uint32_t d, uint32_t b) {
    pf2_zero(e, b, 0xcc);
    W32(e, b, 0xcc);
    pf2_zero(e, b + 0xcc, 0xcc);
    W32(e, b + 0xcc, 0xcc);
    uint32_t r = PF2_C2(e, PF2_IDEV2, 0x0c, R32(e, d + 0x28), b, b + 0xcc);
    if (r) return r;
    uint32_t hw = R32(e, b + 8) != 0;
    W32(e, b + 0x198, hw);
    uint32_t s = hw ? b : b + 0xcc;
    W32(e, b + 0x1a4, R32(e, s + 0x84) & 0x20);
    W32(e, b + 0x1b0, R32(e, s + 0xac));
    W32(e, b + 0x1b4, R32(e, s + 0xb0));
    W32(e, b + 0x1a8, R32(e, s + 0xb4));
    W32(e, b + 0x1ac, R32(e, s + 0xb8));
    W32(e, b + 0x1b8, (pf2_r8(e, s + 0xd) & 0x10) != 0);
    W32(e, b + 0x1bc, (pf2_r8(e, s + 0x81) & 0x20) != 0);
    W32(e, b + 0x1c0, (pf2_r8(e, s + 0x81) & 0x10) != 0);
    W32(e, b + 0x1c4, (pf2_r8(e, s + 0x81) & 0x40) != 0);
    W32(e, 0x00c84fe0u, (pf2_r8(e, s + 0x6d) & 0x80) != 0);
    return 0;
}

/* 0x004b5ee8 d3d_release_device(eax D): Release the materials +0x44 / +0x48, DeleteViewport and
   Release the viewport +0x2c, 0x00497828 (the texture table), Release the device +0x28 (each
   field zeroed); returns 0 */
uint32_t pf4_d3d_release_device(Pf2Env* e, uint32_t d) {
    if (R32(e, d + 0x44)) { PF2_C0(e, PF2_IMAT2, 8, R32(e, d + 0x44)); W32(e, d + 0x44, 0); }
    if (R32(e, d + 0x48)) { PF2_C0(e, PF2_IMAT2, 8, R32(e, d + 0x48)); W32(e, d + 0x48, 0); }
    uint32_t vp = R32(e, d + 0x2c);
    if (vp) {
        if (R32(e, d + 0x28)) PF2_C1(e, PF2_IDEV2, 0x1c, R32(e, d + 0x28), vp);
        PF2_C0(e, PF2_IVIEW2, 8, R32(e, d + 0x2c));
        W32(e, d + 0x2c, 0);
    }
    pf2_call0(e, 0x00497828u);
    if (R32(e, d + 0x28)) { PF2_C0(e, PF2_IDEV2, 8, R32(e, d + 0x28)); W32(e, d + 0x28, 0); }
    return 0;
}

/* 0x004b5e00 d3d_create_device(eax D): IDirect3D2::CreateDevice(&D+0x34 GUID, offscreen +0x14,
   &D+0x28), device caps into 0x00c84dac, CreateViewport(&D+0x2c, NULL), AddViewport (these results
   unchecked); SetViewport2 {0x2c, 0, 0, W, H, clip x -1, clip y = q * c2, clip w 2, clip h q,
   min z 0, max z 1} with q = H * c1 / W (c1 = the double 0x004e36a5, c2 = the float 0x004e36ad);
   SetCurrentViewport; then apply_render_states 0x004b5fc8 and create_materials 0x004b6320
   (returned). A failing SetViewport2 / SetCurrentViewport is returned. */
uint32_t pf4_d3d_create_device(Pf2Env* e, uint32_t d) {
    PF2_C3(e, PF2_ID3D2, 0x20, R32(e, d + 0x24), d + 0x34, R32(e, d + 0x14), d + 0x28);
    pf4_d3d_get_device_caps(e, d, 0x00c84dacu);
    PF2_C2(e, PF2_ID3D2, 0x18, R32(e, d + 0x24), d + 0x2c, 0);
    PF2_C1(e, PF2_IDEV2, 0x18, R32(e, d + 0x28), R32(e, d + 0x2c));
    uint32_t v = e->scratch;
    pf2_zero(e, v, 0x2c);
    double c1;
    memcpy(&c1, e->mem(e->user, 0x004e36a5u), 8);
    double q = g3_div(g3_mul((double)(int32_t)R32(e, d + 0x5c), c1), (double)(int32_t)R32(e, d + 0x58));
    W32(e, v, 0x2c);
    W32(e, v + 4, 0);
    W32(e, v + 8, 0);
    W32(e, v + 0xc, R32(e, d + 0x58));
    W32(e, v + 0x10, R32(e, d + 0x5c));
    W32(e, v + 0x1c, 0x40000000u);
    W32(e, v + 0x14, 0xbf800000u);
    pf2_wf(e, v + 0x20, g3_st(q));
    W32(e, v + 0x24, 0);
    W32(e, v + 0x28, 0x3f800000u);
    pf2_wf(e, v + 0x18, g3_st(g3_mul(q, (double)pf2_rf(e, 0x004e36adu))));
    uint32_t r = PF2_C1(e, PF2_IVIEW2, 0x44, R32(e, d + 0x2c), v);
    if (r) return r;
    r = PF2_C1(e, PF2_IDEV2, 0x34, R32(e, d + 0x28), R32(e, d + 0x2c));
    if (r) return r;
    pf2_call1(e, 0x004b5fc8u, d);
    return pf2_call1(e, 0x004b6320u, d);
}

/* 0x004b41a0 ddraw_release(eax D, edx also release DirectDraw): with IDirectDraw2: the 3D device
   (d3d_release_device), Release +0x30 and the IDirect3D2 +0x24; SetCooperativeLevel NORMAL (cache
   0x004e86ac); release_surfaces 0x004b4374; RestoreDisplayMode; edx: Release IDirectDraw2 +0x08 and
   IDirectDraw +0x04 (zeroed). Returns eax as the last call left it. */
uint32_t pf4_ddraw_release(Pf2Env* e, uint32_t d, uint32_t all) {
    if (R32(e, d + 8)) {
        pf4_d3d_release_device(e, d);
        if (R32(e, d + 0x30)) { PF2_C0(e, PF2_IUNK, 8, R32(e, d + 0x30)); W32(e, d + 0x30, 0); }
        if (R32(e, d + 0x24)) { PF2_C0(e, PF2_ID3D2, 8, R32(e, d + 0x24)); W32(e, d + 0x24, 0); }
    }
    uint32_t dd = R32(e, d + 8);
    if (dd && R32(e, COOP) != 8) {
        W32(e, COOP, 8);
        PF2_C2(e, PF2_IDD2, 0x50, dd, R32(e, d + 0x54), 8);
    }
    uint32_t r = pf2_call1(e, 0x004b4374u, d);
    if (R32(e, d + 8)) r = PF2_C0(e, PF2_IDD2, 0x4c, R32(e, d + 8));
    if (all) {
        r = R32(e, d + 8);            /* eax = the field (0 when there is none) */
        if (r) { r = PF2_C0(e, PF2_IDD2, 8, R32(e, d + 8)); W32(e, d + 8, 0); }
        if (R32(e, d + 4)) { r = PF2_C0(e, PF2_IUNK, 8, R32(e, d + 4)); W32(e, d + 4, 0); }
    }
    return r;
}

/* 0x004b42c8 ddraw_restore_and_clip(eax D): with a primary: display_restore 0x004b4508 with the 3D
   flag cleared around it; with hardware 3D a valid mode index (+0) sets the mode again
   (0x004b45a0 with the list entry) -- ORIGINAL BUG: the result returned is then edi as the caller
   left it (edi_in here), E_FAIL for an invalid index; without 3D the restore result. Then
   ClipCursor(NULL) windowed, else SetRect(0, 0, W, H) + ClipCursor. */
uint32_t pf4_ddraw_restore_and_clip(Pf2Env* e, uint32_t d, uint32_t edi_in) {
    uint32_t r = 0;
    if (R32(e, d + 0xc)) {
        uint32_t f = R32(e, HW3D);
        W32(e, HW3D, 0);
        r = pf2_call1(e, 0x004b4508u, d);
        W32(e, HW3D, f);
        if (f) {
            int32_t i = (int32_t)R32(e, d);
            if (i < (int32_t)R32(e, 0x00c848e0u) && i >= 0) {
                uint32_t m = 0x00c848e4u + 16u * (uint32_t)i;
                uint32_t a[5] = {d, R32(e, m), R32(e, m + 4), R32(e, m + 8), R32(e, m + 12)};
                e->call(e->user, 0x004b45a0u, a, 5);
                r = edi_in;
            } else {
                r = 0x80004005u;
            }
        }
    }
    if (R32(e, d + 0x70) == 0) {
        pf2_call1(e, 0x00c90474u, 0);
    } else {
        uint32_t rc = e->scratch;
        pf2_call5(e, 0x00c90574u, rc, 0, 0, R32(e, d + 0x58), R32(e, d + 0x5c));
        pf2_call1(e, 0x00c90474u, rc);
    }
    return r;
}
