/*
 * en7_b.c: see en7_b.h.
 */
#include "en7_b.h"

#define EN7_DirectInputCreateA 0x00c90810u
#define EN7_SelectPalette      0x00c90204u
#define EN7_RealizePalette     0x00c901fcu
#define EN7_StretchDIBits      0x00c90214u
#define EN7_PeekMessageA       0x00c90530u
#define EN7_TranslateAccel     0x00c90588u
#define EN7_TranslateMessage   0x00c9058cu
#define EN7_DispatchMessageA   0x00c90494u

static uint32_t m(En2Env* e, uint32_t obj, uint32_t off) { return en2_r32(e, en2_r32(e, obj) + off); }

/* 0x004a7be0: opens the joystick manager o: hinst 0x004a8f40, old devices released (0x004a7c94),
 * DirectInputCreateA(hinst, 0x500, &+0x2d8, 0), EnumDevices (+0x10)(di, DIDEVTYPE_JOYSTICK,
 * callback 0x004a8284, o, DIEDFL_ATTACHEDONLY) (failure: DirectInput released); with devices
 * (+0x50c9c > 0): current 0 (+0x2cc), +0 = device 0 (+0x2dc), Acquire, +0x2c8 = +0x2c4 = 1 -> 1.
 * Failures call 0x004937b8 and return 0; no device returns 0. */
uint32_t en7_joy_open_device(En2Env* e, uint32_t o)
{
    uint32_t hinst = en2_call0(e, 0x004a8f40u);
    en2_call1(e, 0x004a7c94u, o);
    if (en2_call4(e, EN7_DirectInputCreateA, hinst, 0x500, o + 0x2d8, 0) != 0) {
        en2_call0(e, 0x004937b8u);
        return 0;
    }
    uint32_t di = en2_r32(e, o + 0x2d8);
    uint32_t v[5] = {di, 4, 0x004a8284u, o, 1};
    if (e->call(e->user, m(e, di, 0x10), v, 5) != 0) {
        en2_call0(e, 0x004937b8u);
        di = en2_r32(e, o + 0x2d8);
        en2_call1(e, m(e, di, 8), di);
        return 0;
    }
    if ((int32_t)en2_r32(e, o + 0x50c9c) <= 0) return 0;
    en2_w32(e, o + 0x2cc, 0);
    uint32_t d = en2_r32(e, o + 0x2dc);
    en2_w32(e, o, d);
    if (en2_call1(e, m(e, d, 0x1c), d) != 0) {
        en2_call0(e, 0x004937b8u);
        return 0;
    }
    en2_w32(e, o + 0x2c8, 1);
    en2_w32(e, o + 0x2c4, 1);
    return 1;
}

/* 0x00495758: the view's light setup: ambient position 0x00a041c0..c8 = the i16 0x00a0416c / 70 / 74
 * sign extended; three lights 0x00495050(0, k, color, direction; the high halves of the three
 * dwords at 0x00a0419a / 0x00a041a6 / 0x00a041b2 + 0, 4, 8) with (direction, color) = (0x00a04178,
 * 0x00a04180), (0x00a04184, 0x00a0418c), (0x00a04190, 0x00a04198); then the empty 0x0048bc38 on
 * 0x00a041cc, 0x00a041fc and the ambient position. */
uint32_t en7_view_lights_setup(En2Env* e)
{
    en2_w32(e, 0x00a041c0u, (uint32_t)(int32_t)(int16_t)en2_r16(e, 0x00a0416cu));
    en2_w32(e, 0x00a041c4u, (uint32_t)(int32_t)(int16_t)en2_r16(e, 0x00a04170u));
    en2_w32(e, 0x00a041c8u, (uint32_t)(int32_t)(int16_t)en2_r16(e, 0x00a04174u));
    static const uint32_t L[3][3] = {{0x00a04178u, 0x00a04180u, 0x00a0419au},
                                     {0x00a04184u, 0x00a0418cu, 0x00a041a6u},
                                     {0x00a04190u, 0x00a04198u, 0x00a041b2u}};
    for (uint32_t k = 0; k < 3; k++) {
        uint32_t a = L[k][2];
        uint32_t v[7] = {0, k, en2_r32(e, L[k][1]), en2_r32(e, L[k][0]),
                         (uint32_t)((int32_t)en2_r32(e, a) >> 16), (uint32_t)((int32_t)en2_r32(e, a + 4) >> 16),
                         (uint32_t)((int32_t)en2_r32(e, a + 8) >> 16)};
        e->call(e->user, 0x00495050u, v, 7);
    }
    en2_call1(e, 0x0048bc38u, 0x00a041ccu);
    en2_call1(e, 0x0048bc38u, 0x00a041fcu);
    return en2_call1(e, 0x0048bc38u, en2_r32(e, 0x00a041c0u));
}

/* 0x004a8284: DIEnumDevicesCallback (stdcall; DIDEVICEINSTANCE ddi, joystick manager o): device =
 * CreateDevice(+0x0c)(di +0x2d8, &ddi.guidInstance, &device, 0) (failure: 0x004937b8, continue),
 * SetDataFormat(+0x2c; c_dfDIJoystick 0x004d1380) (failure: release, continue),
 * SetCooperativeLevel(+0x34; hwnd 0x004a8f34(), 6) (failure: release and stop: 0), then
 * 0x004a7dbc(o, device, ddi) keeps it (an IDirectInputDevice2) and this reference is released.
 * Returns 1 (continue) except as said. */
uint32_t en7_joy_enum_device_cb(En2Env* e, uint32_t ddi, uint32_t o)
{
    const uint32_t L = e->scratch;
    uint32_t di = en2_r32(e, o + 0x2d8);
    uint32_t hwnd = en2_call0(e, 0x004a8f34u);
    uint32_t v[4] = {di, ddi + 4, L, 0};
    if (e->call(e->user, m(e, di, 0x0c), v, 4) != 0) {
        en2_call0(e, 0x004937b8u);
        return 1;
    }
    uint32_t d = en2_r32(e, L);
    if (en2_call2(e, m(e, d, 0x2c), d, 0x004d1380u) != 0) {
        en2_call0(e, 0x004937b8u);
        d = en2_r32(e, L);
        en2_call1(e, m(e, d, 8), d);
        return 1;
    }
    d = en2_r32(e, L);
    if (en2_call3(e, m(e, d, 0x34), d, hwnd, 6) != 0) {
        en2_call0(e, 0x004937b8u);
        d = en2_r32(e, L);
        en2_call1(e, m(e, d, 8), d);
        return 0;
    }
    en2_call3(e, 0x004a7dbcu, o, en2_r32(e, L), ddi);
    d = en2_r32(e, L);
    en2_call1(e, m(e, d, 8), d);
    return 1;
}

/* 0x004aa2d8: draws the DIB o (+0 palette, +8 BITMAPINFO, +0x0c bits) on hdc: with a palette
 * SelectPalette(hdc, pal, 0) and RealizePalette; StretchDIBits(hdc, dst rect (x, y, w, h), source
 * rect (x, biHeight - bottom (bottom up DIB), w, h), bits, info, DIB_RGB_COLORS, SRCCOPY). No
 * BITMAPINFO: 0. (0x004aa33a / 0x004aa33c, the "no BITMAPINFO" height of 0, are dead.) */
uint32_t en7_dib_stretch(En2Env* e, uint32_t o, uint32_t hdc, uint32_t dst, uint32_t src)
{
    if (en2_r32(e, o + 8) == 0) return 0;
    uint32_t pal = en2_r32(e, o);
    if (pal != 0) {
        en2_call3(e, EN7_SelectPalette, hdc, pal, 0);
        en2_call1(e, EN7_RealizePalette, hdc);
    }
    uint32_t bi = en2_r32(e, o + 8);
    uint32_t v[13] = {hdc, en2_r32(e, dst), en2_r32(e, dst + 4), en2_r32(e, dst + 8) - en2_r32(e, dst),
                      en2_r32(e, dst + 12) - en2_r32(e, dst + 4), en2_r32(e, src),
                      en2_r32(e, bi + 8) - en2_r32(e, src + 12), en2_r32(e, src + 8) - en2_r32(e, src),
                      en2_r32(e, src + 12) - en2_r32(e, src + 4), en2_r32(e, o + 0x0c), en2_r32(e, o + 8), 0,
                      0x00cc0020u};
    return e->call(e->user, EN7_StretchDIBits, v, 13);
}

/* 0x004a490c (widget_create_text_button): widget_base_init(eax, edx, ebx, ecx; (s0 & 0xffff0000)
 * | 0x0b, s1..s4), nine components from +0xdc and three from +0x190 cleared (0x004aa020, each on the
 * previous answer + 0x14; the widget is derived back from the answers as in 0x004a42c0), vtable
 * 0x004e5d80, style +0xc0 = s0, +0xc8 = 2 + (style bit 3), +0xc4 = +0xbc = 0, colors +0xd0
 * 0xffff00, +0xcc 0x808000, +0xd8 0x7f7f7f, +0xd4 0xffffff, then vtable +0xc0 (w, 1). Returns w;
 * pops five stack words. */
uint32_t en7_widget_create_text_button(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                                       uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4)
{
    uint32_t v[9] = {eax, edx, ebx, ecx, (s0 & 0xffff0000u) | 0x0bu, s1, s2, s3, s4};
    uint32_t c = e->call(e->user, 0x004a42c0u, v, 9);
    c = en2_call1(e, 0x004aa020u, c + 0xdc);
    for (int k = 0; k < 8; k++) c = en2_call1(e, 0x004aa020u, c + 0x14);
    uint32_t w = c - 0x17c;
    c = en2_call1(e, 0x004aa020u, w + 0x190);
    c = en2_call1(e, 0x004aa020u, c + 0x14);
    c = en2_call1(e, 0x004aa020u, c + 0x14);
    w = c - 0x1b8;
    en2_w32(e, w + 0x34, 0x004e5d80u);
    en2_w32(e, w + 0xc0, s0);
    uint32_t b = (en2_r8(e, w + 0xc0) & 8) != 0;
    en2_w32(e, w + 0xc4, 0);
    en2_w32(e, w + 0xbc, 0);
    en2_w32(e, w + 0xd0, 0xffff00u);
    en2_w32(e, w + 0xcc, 0x808000u);
    en2_w32(e, w + 0xd8, 0x7f7f7fu);
    en2_w32(e, w + 0xd4, 0xffffffu);
    en2_w32(e, w + 0xc8, b + 2);
    uint32_t vt = en2_r32(e, w + 0x34);
    en2_call4(e, en2_r32(e, vt + 0xc0), w, 1, vt, ecx);
    return w;
}

/* 0x004a8e6c (game_loop_run): the message pump. Each round PeekMessageA(&msg, 0, 0, 0, PM_REMOVE);
 * with no message game_tick 0x004ac714(0x00bdfcd0) is asked and a nonzero answer returns
 * msg.wParam; otherwise (ORIGINAL quirk: also when there was no message, the previous msg is
 * handled again) WM_ACTIVATEAPP (0x1c) for the game window with wParam 0 deactivates DirectDraw
 * (0x004b42b8), WM_QUIT (0x12) longjmps to 0x00bdfaf0 with 1 (never returns), and the message is
 * translated (accelerators of 0x004a6dd4 first) and dispatched. */
uint32_t en7_game_loop_run(En2Env* e)
{
    const uint32_t msg = e->scratch;
    uint32_t hwnd = en2_call1(e, 0x004a6dccu, 0x00bdfcd0u);
    for (;;) {
        uint32_t v[5] = {msg, 0, 0, 0, 1};
        if (e->call(e->user, EN7_PeekMessageA, v, 5) == 0 && en2_call1(e, 0x004ac714u, 0x00bdfcd0u) != 0)
            return en2_r32(e, msg + 8);
        uint32_t acc = en2_call1(e, 0x004a6dd4u, 0x00bdfcd0u);
        if (en2_r32(e, msg + 4) == 0x1c && hwnd == en2_r32(e, msg) && en2_r32(e, msg + 8) == 0)
            en2_call1(e, 0x004b42b8u, EN2_DISPLAY);
        if (en2_r32(e, msg + 4) == 0x12) {
            en2_call2(e, 0x004c4827u, 0x00bdfaf0u, 1);
            return 0;   /* (longjmp: not reached in the game) */
        }
        if (acc != 0 && en2_call3(e, EN7_TranslateAccel, hwnd, acc, msg) != 0) continue;
        en2_call1(e, EN7_TranslateMessage, msg);
        en2_call1(e, EN7_DispatchMessageA, msg);
    }
}
