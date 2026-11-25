/*
 * pf4_text.c: TM2.EXE's GDI text on the offscreen surface (docs/PLATFORM.md Part A, "Text style
 * object"), ported over Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf4_text.sh.
 *
 * Style: +0x00/+0x04/+0x08 text R, G, B; +0x0c/+0x10/+0x14 background; +0x18/+0x1c/+0x20 shadow;
 * +0x24 background mode; +0x34 shadow on, +0x38 shadow offset; +0x3c HFONT (0 = none), +0x40 the
 * previous font. COLORREF = R | G << 8 | B << 16 of the low bytes. The DC comes from
 * ddraw_get_dc 0x004b50a4(0x00c84840, &hdc, 0) and goes back with 0x004b50b8; GetDC's result is
 * not checked.
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define I_lstrlenA 0x00c90320u
#define I_SelectObject 0x00c90200u
#define I_SetBkColor 0x00c90208u
#define I_SetTextColor 0x00c90210u
#define I_SetBkMode 0x00c9020cu
#define I_TextOutA 0x00c90218u
#define I_GetTextExtentPoint32A 0x00c901f0u
#define F_GET_DC 0x004b50a4u
#define F_RELEASE_DC 0x004b50b8u

static uint32_t rgb(Pf2Env* e, uint32_t p) {
    return pf2_r8(e, p) | (uint32_t)pf2_r8(e, p + 4) << 8 | (uint32_t)pf2_r8(e, p + 8) << 16;
}

static int32_t half(int32_t v) { return (int32_t)((uint32_t)v - (uint32_t)(v >> 31)) >> 1; }

static void colors(Pf2Env* e, uint32_t s, uint32_t hdc, uint32_t fg) {
    pf2_call2(e, I_SetBkColor, hdc, rgb(e, s + 0xc));
    pf2_call2(e, I_SetTextColor, hdc, rgb(e, s + fg));
    pf2_call2(e, I_SetBkMode, hdc, R32(e, s + 0x24));
}

/* 0x004b31d0 text_draw(eax style, edx text, ebx x, ecx y): font selected; with the shadow on a
   first TextOutA at (x + off, y + off) in the shadow color; then TextOutA at (x, y) in the text
   color; font restored; returns the last TextOutA result */
uint32_t pf4_text_draw(Pf2Env* e, uint32_t s, uint32_t str, uint32_t x, uint32_t y) {
    uint32_t hp = e->scratch;
    pf2_call3(e, F_GET_DC, PF2_DISPLAY, hp, 0);
    uint32_t len = pf2_call1(e, I_lstrlenA, str);
    if (R32(e, s + 0x3c)) W32(e, s + 0x40, pf2_call2(e, I_SelectObject, R32(e, hp), R32(e, s + 0x3c)));
    if (R32(e, s + 0x34)) {
        colors(e, s, R32(e, hp), 0x18);
        uint32_t off = R32(e, s + 0x38);
        pf2_call5(e, I_TextOutA, R32(e, hp), off + x, y + off, str, len);
    }
    colors(e, s, R32(e, hp), 0);
    uint32_t r = pf2_call5(e, I_TextOutA, R32(e, hp), x, y, str, len);
    if (R32(e, s + 0x3c)) pf2_call2(e, I_SelectObject, R32(e, hp), R32(e, s + 0x40));
    pf2_call3(e, F_RELEASE_DC, PF2_DISPLAY, R32(e, hp), 0);
    return r;
}

/* 0x004b3330 text_draw_centered(eax style, edx text, ebx x, ecx y): as text_draw at (x - cx / 2,
   y - cy / 2) (signed halving), the size from GetTextExtentPoint32A measured before the font is
   selected (so with the DC's previous font) */
uint32_t pf4_text_draw_centered(Pf2Env* e, uint32_t s, uint32_t str, uint32_t x, uint32_t y) {
    uint32_t sz = e->scratch, hp = e->scratch + 8;
    pf2_call3(e, F_GET_DC, PF2_DISPLAY, hp, 0);
    uint32_t len = pf2_call1(e, I_lstrlenA, str);
    pf2_call4(e, I_GetTextExtentPoint32A, R32(e, hp), str, len, sz);
    int32_t cx = (int32_t)R32(e, sz), cy = (int32_t)R32(e, sz + 4);
    if (R32(e, s + 0x3c)) W32(e, s + 0x40, pf2_call2(e, I_SelectObject, R32(e, hp), R32(e, s + 0x3c)));
    uint32_t tx = x - (uint32_t)half(cx), ty = y - (uint32_t)half(cy);
    if (R32(e, s + 0x34)) {
        colors(e, s, R32(e, hp), 0x18);
        pf2_call5(e, I_TextOutA, R32(e, hp), tx + R32(e, s + 0x38), ty + R32(e, s + 0x38), str, len);
    }
    colors(e, s, R32(e, hp), 0);
    uint32_t r = pf2_call5(e, I_TextOutA, R32(e, hp), tx, ty, str, len);
    if (R32(e, s + 0x3c)) pf2_call2(e, I_SelectObject, R32(e, hp), R32(e, s + 0x40));
    pf2_call3(e, F_RELEASE_DC, PF2_DISPLAY, R32(e, hp), 0);
    return r;
}

static uint32_t extent(Pf2Env* e, uint32_t str, int which) {
    uint32_t sz = e->scratch, hp = e->scratch + 8;
    pf2_call3(e, F_GET_DC, PF2_DISPLAY, hp, 0);
    uint32_t len = pf2_call1(e, I_lstrlenA, str);
    pf2_call4(e, I_GetTextExtentPoint32A, R32(e, hp), str, len, sz);
    pf2_call3(e, F_RELEASE_DC, PF2_DISPLAY, R32(e, hp), 0);
    return R32(e, sz + 4 * which);
}

/* 0x004b3a44 text_width / 0x004b3a90 text_height(eax style (unused), edx text): the extent cx / cy */
uint32_t pf4_text_width(Pf2Env* e, uint32_t s, uint32_t str) { (void)s; return extent(e, str, 0); }
uint32_t pf4_text_height(Pf2Env* e, uint32_t s, uint32_t str) { (void)s; return extent(e, str, 1); }
