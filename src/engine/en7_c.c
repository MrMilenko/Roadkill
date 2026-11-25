/*
 * en7_c.c: see en7_c.h.
 */
#include "en7_c.h"

#define EN7_GetMenu           0x00c904d0u
#define EN7_SetRect           0x00c90574u
#define EN7_AdjustWindowRect  0x00c90464u
#define EN7_FindResourceA     0x00c902b8u
#define EN7_LoadResource      0x00c902e8u
#define EN7_LockResource      0x00c902ecu
#define EN7_SetTextColor      0x00c90210u
#define EN7_OffsetRect        0x00c9052cu
#define EN7_DrawTextA         0x00c9049cu
#define EN7_wsprintfA         0x00c90658u
#define EN7_GetDriveTypeA     0x00c902c8u
#define EN7_CreateFileA       0x00c902a8u
#define EN7_GetFileSize       0x00c902d0u
#define EN7_QPF               0x00c902f4u
#define EN7_timeGetDevCaps    0x00c9063cu
#define EN7_timeBeginPeriod   0x00c90634u

/* 0x004a91b0 (widget_mgr_check: the runtime's inline strcmp, a word at a time): -1 / 0 / 1 by the
 * first differing byte (unsigned). */
uint32_t en7_strcmp(En2Env* e, uint32_t a, uint32_t b)
{
    if (a == b) return 0;
    for (;; a++, b++) {
        uint8_t x = en2_r8(e, a), y = en2_r8(e, b);
        if (x != y) return x < y ? 0xffffffffu : 1;
        if (x == 0) return 0;
    }
}

/* 0x004ac7f4: the window frame sizes for the window's style and menu: AdjustWindowRect of a 100 x 100
 * client rect -> out +0x18 (width), +0x1c (height); of 640 x 480 -> +0x20, +0x24. Returns 0. */
uint32_t en7_window_frame_sizes(En2Env* e, uint32_t o, uint32_t out)
{
    const uint32_t R0 = e->scratch, R1 = e->scratch + 0x10;
    uint32_t style = en2_call2(e, EN2_GetWindowLongA, en2_r32(e, o), (uint32_t)-16);
    uint32_t menu = en2_call1(e, EN7_GetMenu, en2_r32(e, o)) != 0;
    en2_call5(e, EN7_SetRect, R1, 0, 0, 100, 100);
    en2_call3(e, EN7_AdjustWindowRect, R1, style, menu);
    en2_call5(e, EN7_SetRect, R0, 0, 0, 640, 480);
    en2_call3(e, EN7_AdjustWindowRect, R0, style, menu);
    en2_w32(e, out + 0x18, en2_r32(e, R1 + 8) - en2_r32(e, R1));
    en2_w32(e, out + 0x1c, en2_r32(e, R1 + 12) - en2_r32(e, R1 + 4));
    en2_w32(e, out + 0x20, en2_r32(e, R0 + 8) - en2_r32(e, R0));
    en2_w32(e, out + 0x24, en2_r32(e, R0 + 12) - en2_r32(e, R0 + 4));
    return 0;
}

/* 0x004aa098: a bitmap resource (RT_BITMAP) into the DIB o: +0x10 the resource handle, +8 its
 * BITMAPINFO (LockResource), +4 the color count (biClrUsed, else 2 / 16 / 256 for 1 / 4 / 8 bits,
 * else 0), the palette (0x004aa1bc), +0x0c the bits (info + biSize + 4 colors). 1 when the resource
 * is missing or does not load, else 0. (A failed LockResource is not checked further: the original
 * reads through address 0 then.) */
uint32_t en7_bitmap_load(En2Env* e, uint32_t o, uint32_t hinst, uint32_t name)
{
    uint32_t r = en2_call3(e, EN7_FindResourceA, hinst, name, 2);
    if (r == 0) return 1;
    uint32_t h = en2_call2(e, EN7_LoadResource, hinst, r);
    en2_w32(e, o + 0x10, h);
    if (h == 0) return 1;
    uint32_t p = en2_call1(e, EN7_LockResource, h);
    en2_w32(e, o + 8, p);
    uint32_t n = 0;
    if (p != 0) {
        n = en2_r32(e, p + 0x20);
        if (n == 0) {
            uint16_t bits = en2_r16(e, p + 0x0e);
            n = bits == 1 ? 2 : bits == 4 ? 0x10 : bits == 8 ? 0x100 : 0;
        }
    }
    en2_w32(e, o + 4, n);
    en2_call1(e, 0x004aa1bcu, o);
    uint32_t bi = en2_r32(e, o + 8);
    en2_w32(e, o + 0x0c, bi + en2_r32(e, bi) + en2_r32(e, o + 4) * 4);
    return 0;
}

/* 0x004a5110: a button's text: pressed moves the caller's RECT (left, top) by 2; with shadow the
 * text is first drawn in color +0xd4 one pixel down right (a copy of the rect) and the color +0xd8
 * set; then DrawTextA(hdc, text, -1, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX
 * (0x125)); the old color restored after a shadow. Returns 0; pops two stack words. */
uint32_t en7_button_text_draw(En2Env* e, uint32_t o, uint32_t hdc, uint32_t rect, uint32_t text,
                              uint32_t pressed, uint32_t shadow)
{
    const uint32_t R = e->scratch;
    if (pressed != 0) {
        en2_w32(e, rect + 4, en2_r32(e, rect + 4) + 2);
        en2_w32(e, rect, en2_r32(e, rect) + 2);
    }
    uint32_t old = 0;
    if (shadow != 0) {
        for (uint32_t k = 0; k < 16; k += 4) en2_w32(e, R + k, en2_r32(e, rect + k));
        old = en2_call2(e, EN7_SetTextColor, hdc, en2_r32(e, o + 0xd4));
        en2_call3(e, EN7_OffsetRect, R, 1, 1);
        en2_call5(e, EN7_DrawTextA, hdc, text, 0xffffffffu, R, 0x125);
        en2_call2(e, EN7_SetTextColor, hdc, en2_r32(e, o + 0xd8));
    }
    en2_call5(e, EN7_DrawTextA, hdc, text, 0xffffffffu, rect, 0x125);
    if (shadow != 0) en2_call2(e, EN7_SetTextColor, hdc, old);
    return 0;
}

/* 0x00499884 (win32_register_window: it finds the game CD): for drives 'A'..'Z' of type
 * DRIVE_CDROM (GetDriveTypeA of "%c:\") the file "%c:\<name>" is opened (CreateFileA read
 * only); the first that opens is closed and its letter stored in 0x00a3a5d0 and returned, -1 if
 * none. (The file size is asked for and dropped.) wsprintfA is cdecl. */
uint32_t en7_find_cd_drive(En2Env* e, uint32_t name)
{
    const uint32_t path = e->scratch, root = e->scratch + 0x130;
    uint32_t c;
    for (c = 0x41; c <= 0x5a; c++) {
        en2_call3(e, EN7_wsprintfA, root, 0x004e2fefu, c);
        if (en2_call1(e, EN7_GetDriveTypeA, root) != 5) continue;
        en2_call4(e, EN7_wsprintfA, path, 0x004e2ff4u, c, name);
        uint32_t v[7] = {path, 0x80000000u, 1, 0, 3, 0x10000080u, 0};
        uint32_t h = e->call(e->user, EN7_CreateFileA, v, 7);
        if (h == 0xffffffffu) continue;
        en2_call2(e, EN7_GetFileSize, h, 0);
        en2_call1(e, EN2_CloseHandle, h);
        break;
    }
    if ((int32_t)c > 0x5a) c = 0xffffffffu;
    en2_w32(e, 0x00a3a5d0u, c);
    return c;
}

static double rdd(En2Env* e, uint32_t a) { double d; memcpy(&d, e->mem(e->user, a), 8); return d; }
static void wrd(En2Env* e, uint32_t a, double d) { memcpy(e->mem(e->user, a), &d, 8); }

/* 0x00492f30 (subsys_input_init: the timer setup): 0x00a0364c = 0, the refresh rate (display
 * 0x004b5988, 60 when unknown) -> 0x00a0365c; QueryPerformanceFrequency (BOOL -> 0x00a03668):
 * the frequency (high * 2^32 (EXE double 0x004e2e48) + low) -> 0x00a03660, then 0x004e2e50 (1e6)
 * / it, a frequency <= 0 clears 0x00a03668; without the counter the multimedia timer period is
 * the device's minimum (at least 1, at most its maximum) -> 0x00a0366c, timeBeginPeriod. */
void en7_timer_init(En2Env* e)
{
    const uint32_t li = e->scratch, tc = e->scratch + 8;
    en2_w32(e, 0x00a0364cu, 0);
    uint32_t r = en2_call1(e, 0x004b5988u, EN2_DISPLAY);
    en2_w32(e, 0x00a0365cu, r);
    if (r == 0) en2_w32(e, 0x00a0365cu, 0x3c);
    uint32_t ok = en2_call1(e, EN7_QPF, li);
    en2_w32(e, 0x00a03668u, ok);
    if (ok != 0) {
        double v = (double)(int32_t)en2_r32(e, li + 4) * rdd(e, 0x004e2e48u);
        v = v + (double)en2_r32(e, li);
        wrd(e, 0x00a03660u, v);
        if (!(0.0 >= rdd(e, 0x00a03660u))) wrd(e, 0x00a03660u, rdd(e, 0x004e2e50u) / rdd(e, 0x00a03660u));
        else en2_w32(e, 0x00a03668u, 0);
    }
    if (en2_r32(e, 0x00a03668u) != 0) return;
    en2_call2(e, EN7_timeGetDevCaps, tc, 8);
    uint32_t mn = en2_r32(e, tc), mx = en2_r32(e, tc + 4);
    uint32_t p = mn > 1 ? mn : 1;
    if (p >= mx) p = mx;
    en2_w32(e, 0x00a0366cu, p);
    en2_call1(e, EN7_timeBeginPeriod, p);
}
