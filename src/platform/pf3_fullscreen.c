/*
 * pf3_fullscreen.c: TM2.EXE 0x004b4c80 ddraw_enter_fullscreen (docs/PLATFORM.md Part B), ported over
 * Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf3_fullscreen.sh.
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define COOP 0x004e86acu

/* 0x004b4c80 ddraw_enter_fullscreen(eax D): +0x70 = 1; release_surfaces; window style: WS_POPUP on,
   caption / system menu / minimize off (& 0x7f35ffff | 0x80000000); SetCooperativeLevel(hwnd, 0x13)
   unless cached (0x004e86ac); a failure other than DDERR_HWNDALREADYSET 0x8876023b: error box 0xe +
   fatal_quit (treated as not returning). RestoreDisplayMode; SetDisplayMode(w, h, bpp, refresh, 0)
   (+0x58 / +0x5c / +0x64 / +0x68; failure: error box (0xa, w, h, bpp) + fatal_quit). Surfaces: the
   flip chain 0x004b48f4 with hardware 3D or page flipping (+0x6c), else the window surfaces
   0x004b4b18; then the clipper 0x004b4bb4. Returns the surface call's result. */
uint32_t pf3_ddraw_enter_fullscreen(Pf2Env* e, uint32_t d) {
    W32(e, d + 0x70, 1);
    pf2_call1(e, 0x004b4374u, d);
    uint32_t st = pf2_call2(e, 0x00c904ecu, R32(e, d + 0x54), (uint32_t)-16);
    pf2_call3(e, 0x00c90578u, R32(e, d + 0x54), (uint32_t)-16, (st & 0x7f35ffffu) | 0x80000000u);
    uint32_t dd = R32(e, d + 8), r = 0;
    if (dd && R32(e, COOP) != 0x13) {
        W32(e, COOP, 0x13);
        r = PF2_C2(e, PF2_IDD2, 0x50, dd, R32(e, d + 0x54), 0x13);
    }
    if (r != 0 && r != 0x8876023bu) {
        pf2_call2(e, 0x004bc660u, 0xe, r & 0xfff);
        pf2_call0(e, 0x004a8f24u);
        return 0;
    }
    if (R32(e, d + 8)) PF2_C0(e, PF2_IDD2, 0x4c, R32(e, d + 8));
    r = PF2_C5(e, PF2_IDD2, 0x54, R32(e, d + 8), R32(e, d + 0x58), R32(e, d + 0x5c), R32(e, d + 0x64), R32(e, d + 0x68), 0);
    if (r != 0) {
        pf2_call4(e, 0x004bc660u, 0xa, R32(e, d + 0x58), R32(e, d + 0x5c), R32(e, d + 0x64));
        pf2_call0(e, 0x004a8f24u);
        return 0;
    }
    if (R32(e, 0x00c85008u) || R32(e, d + 0x6c)) r = pf2_call1(e, 0x004b48f4u, d);
    else r = pf2_call1(e, 0x004b4b18u, d);
    pf2_call1(e, 0x004b4bb4u, d);
    return r;
}
