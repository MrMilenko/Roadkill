/*
 * pf3_mode.c: TM2.EXE 0x004b45a0 ddraw_set_mode (docs/PLATFORM.md Part B), ported over Pf2Env
 * (pf2_env.h). Emulator diff: tools/emu/check_pf3_mode.sh.
 *
 * ddraw_set_mode(eax D, edx width, ebx height, ecx bpp, stack refresh; ret 4). D = the display
 * object (+0x00 mode index, +0x08 IDirectDraw2, +0x54 HWND, +0x58 / +0x5c / +0x64 / +0x68 width /
 * height / bpp / refresh, +0x70 full screen, +0x78 client rect, +0x90 last ShowCursor value);
 * the mode list 0x00c848e0 {count, 16 byte {w, h, bpp, refresh}}. Returns 0 or the failing result.
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define HW3D 0x00c85008u
#define COOP 0x004e86acu

#define I_ShowCursor 0x00c90580u
#define I_GetWindowLongA 0x00c904ecu
#define I_SetWindowLongA 0x00c90578u
#define I_SetRect 0x00c90574u
#define I_ClipCursor 0x00c90474u

#define F_CLEAR 0x00495c94u          /* clear the screen buffers (eax 0) */
#define F_D3D_RELEASE 0x004b5ee8u
#define F_RELEASE_SURFACES 0x004b4374u
#define F_FULLSCREEN 0x004b4c80u     /* ddraw_enter_fullscreen */
#define F_CURSOR_WANTED 0x0049e108u
#define F_ERROR_BOX 0x004bc660u
#define F_FATAL 0x004a8f24u          /* fatal_quit: does not return (the port returns 0 after it) */
#define F_WINDOW_SURFACES 0x004b4b18u
#define F_ATTACH_CLIPPER 0x004b4bb4u
#define F_RESIZE_WINDOW 0x004b5a94u  /* (eax D, edx x, ebx y, ecx w, stack h) */
#define F_PIXEL_FORMAT 0x0049628cu
#define F_VIEW_RESIZE_ALL 0x004941f0u
#define F_VIEW_49433C 0x0049433cu
#define F_D3D_CREATE_DEVICE 0x004b5e00u
#define F_RELOAD_TEXTURES 0x00497390u
#define F_MEASURE_REFRESH 0x004b598cu

static void cursor(Pf2Env* e, uint32_t d, uint32_t v) {
    if (R32(e, d + 0x90) == v) return;
    W32(e, d + 0x90, v);
    pf2_call1(e, I_ShowCursor, v);
}

uint32_t pf3_ddraw_set_mode(Pf2Env* e, uint32_t d, uint32_t w, uint32_t h, uint32_t bpp, uint32_t refresh) {
    pf2_call1(e, F_CLEAR, 0);
    if (R32(e, HW3D)) pf2_call1(e, F_D3D_RELEASE, d);
    pf2_call1(e, F_RELEASE_SURFACES, d);   /* (edx = width there: release_surfaces' no surface test) */

    /* the mode's index in the list (refresh 0 matches any), else -1 */
    uint32_t idx = 0xffffffffu, n = R32(e, 0x00c848e0u);
    if ((int32_t)n > 0) {
        for (uint32_t k = 0; (int32_t)(16 * k) < (int32_t)(n << 4); k++) {
            uint32_t m = 0x00c848e4u + 16 * k;
            if (w == R32(e, m) && h == R32(e, m + 4) && bpp == R32(e, m + 8) && (refresh == 0 || refresh == R32(e, m + 12))) {
                idx = k;
                break;
            }
        }
    }
    W32(e, d, idx);
    W32(e, d + 0x64, bpp);
    W32(e, d + 0x58, w);
    W32(e, d + 0x5c, h);
    W32(e, d + 0x68, refresh);
    W32(e, d + 0x80, R32(e, d + 0x78) + R32(e, d + 0x58));
    W32(e, d + 0x84, R32(e, d + 0x7c) + R32(e, d + 0x5c));

    uint32_t r;
    if (R32(e, d + 0x70)) {
        r = pf2_call1(e, F_FULLSCREEN, d);
        if (r == 0) cursor(e, d, pf2_call0(e, F_CURSOR_WANTED) == 0 ? 1 : 0);
    } else {
        uint32_t st = pf2_call2(e, I_GetWindowLongA, R32(e, d + 0x54), (uint32_t)-16);
        pf2_call3(e, I_SetWindowLongA, R32(e, d + 0x54), (uint32_t)-16, (st & 0x7f35ffffu) | 0x00ca0000u);
        uint32_t dd = R32(e, d + 8), c = 0;
        if (dd && R32(e, COOP) != 8) {
            W32(e, COOP, 8);
            c = PF2_C2(e, PF2_IDD2, 0x50, dd, R32(e, d + 0x54), 8);
        }
        if (c) {
            pf2_call2(e, F_ERROR_BOX, 0xe, c & 0xfff);
            pf2_call0(e, F_FATAL);
            return 0;
        }
        pf2_call1(e, F_RELEASE_SURFACES, d);
        if (R32(e, d + 8)) PF2_C0(e, PF2_IDD2, 0x4c, R32(e, d + 8));
        r = pf2_call1(e, F_WINDOW_SURFACES, d);
        pf2_call1(e, F_ATTACH_CLIPPER, d);
    }
    uint32_t a[5] = {d, R32(e, d + 0x78), R32(e, d + 0x7c), R32(e, d + 0x58), R32(e, d + 0x5c)};
    e->call(e->user, F_RESIZE_WINDOW, a, 5);
    pf2_call0(e, F_PIXEL_FORMAT);
    pf2_call1(e, F_CLEAR, 0);
    if (r == 0) {
        pf2_call2(e, F_VIEW_RESIZE_ALL, R32(e, d + 0x58), R32(e, d + 0x5c));
        pf2_call2(e, F_VIEW_49433C, 0, 0);
    }
    if (R32(e, HW3D)) {
        uint32_t x = pf2_call1(e, F_D3D_CREATE_DEVICE, d);
        if (x) return x;
        r = pf2_call0(e, F_RELOAD_TEXTURES);
        if (r) return r;
    }
    if (R32(e, d + 0x70) == 0) {
        pf2_call1(e, I_ClipCursor, 0);
    } else {
        uint32_t rc = e->scratch;
        pf2_call5(e, I_SetRect, rc, 0, 0, R32(e, d + 0x58), R32(e, d + 0x5c));
        pf2_call1(e, I_ClipCursor, rc);
    }
    pf2_call1(e, F_MEASURE_REFRESH, d);
    return r;
}
