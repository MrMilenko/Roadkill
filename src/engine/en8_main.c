/*
 * en8_main.c: the game's WinMain, TM2.EXE 0x004a8b04 (see en8_main.h).
 * Verified under emulation: tools/emu/check_en8_main.sh.
 */
#include "en8_main.h"

#define R_SET_PATH 0x004ac4b0u
#define R_DEFAULT  0x004ac4e8u
#define R_CREATE   0x004ac550u
#define R_OPEN     0x004ac528u
#define R_INT      0x004ac5bcu
#define R_WRITE    0x004ac614u
#define R_FLUSH    0x004ac590u
#define WINDOW     0x00bdfcd0u
#define I_CoInitialize 0x00c9015cu
#define I_CoUninitialize 0x00c90160u
#define I_SetRect 0x00c90574u
#define I_AdjustWindowRect 0x00c90464u
#define I_GetDC 0x00c904c4u
#define I_GetClientRect 0x00c904bcu
#define I_GetStockObject 0x00c901ecu
#define I_FillRect 0x00c904acu
#define I_ReleaseDC 0x00c90544u
#define I_GetWindowRect 0x00c904f0u
#define I_ClipCursor 0x00c90474u

enum { M_RT = 0, M_CLIENT = 0x1e0, M_RECT = 0x1f0, M_KEY = 0x200, M_X = 0x204, M_Y = 0x208 };

static int32_t r32s(En2Env* e, uint32_t a) { return (int32_t)en2_r32(e, a); }

uint32_t en8_game_main(En2Env* e, uint32_t edi_in, uint32_t inst, uint32_t prev, uint32_t cmdline, uint32_t show)
{
    (void)prev;
    (void)cmdline;
    const uint32_t F = e->scratch;
    uint32_t hwnd = edi_in, r = 0;
    en2_call2(e, R_SET_PATH, F + M_KEY, 0);
    en2_w32(e, 0x00bdfb24u, inst);
    en2_w32(e, F + M_X, 0x30);
    en2_w32(e, F + M_Y, 0x30);
    en2_call1(e, 0x004a6b78u, inst);
    en2_call1(e, R_DEFAULT, 0x004e31dcu);
    en2_call3(e, R_CREATE, F + M_KEY, 0x80000002u, 0);
    en2_call1(e, R_FLUSH, F + M_KEY);
    en2_call1(e, 0x004c45c0u, F + M_RT);
    en2_call2(e, 0x00499884u, 0x004e320cu, 0x004e3214u);
    en2_call2(e, 0x004c4558u, 0x00bdfb28u, 0x004e3214u);   /* edx: left from the call before */
    en2_call0(e, 0x004b9bf0u);                              /* movie_init */
    if ((int32_t)en2_call1(e, I_CoInitialize, 0) < 0) return 1;
    if (en2_call1(e, 0x004c47e8u, 0x00bdfaf0u) == 0) {      /* setjmp: the first return */
        if (en2_call1(e, 0x00498ddcu, 0x004e321fu) == 0) return 0;
        en2_call5(e, I_SetRect, F + M_RECT, 0, 0, 0x280, 0x1e0);
        en2_call3(e, I_AdjustWindowRect, F + M_RECT, 0x02ca0000u, 0);
        uint32_t w = en2_r32(e, F + M_RECT + 8) - en2_r32(e, F + M_RECT);
        uint32_t h = en2_r32(e, F + M_RECT + 0xc) - en2_r32(e, F + M_RECT + 4);
        en2_w32(e, F + M_RECT + 8, w);
        en2_w32(e, F + M_RECT, 0);
        en2_w32(e, F + M_RECT + 4, 0);
        en2_w32(e, F + M_RECT + 0xc, h);
        en2_call3(e, R_OPEN, F + M_KEY, 0x80000002u, 0);
        en2_call3(e, R_INT, F + M_KEY, 0x004e3227u, F + M_X);
        en2_call3(e, R_INT, F + M_KEY, 0x004e3233u, F + M_Y);
        en2_call1(e, R_FLUSH, F + M_KEY);
        /* kept on the screen */
        if (r32s(e, F + M_X) + (int32_t)en2_r32(e, F + M_RECT + 8) > r32s(e, F + 0x1c4))
            en2_w32(e, F + M_X, en2_r32(e, F + 0x1c4) - en2_r32(e, F + M_RECT + 8));
        if (r32s(e, F + M_X) < 0) en2_w32(e, F + M_X, 0);
        if (r32s(e, F + M_Y) + (int32_t)en2_r32(e, F + M_RECT + 0xc) > r32s(e, F + 0x1c8))
            en2_w32(e, F + M_Y, en2_r32(e, F + 0x1c8) - en2_r32(e, F + M_RECT + 0xc));
        if (r32s(e, F + M_Y) < 0) en2_w32(e, F + M_Y, 0);
        en2_call4(e, 0x004a6b80u, 0x004e323fu, inst, 0xb, 0);   /* register the class */
        {
            uint32_t v[12] = {WINDOW, 0x40000u, 0x004e3265u, 0x004e3255u, 0x02ca0000u, en2_r32(e, F + M_X),
                              en2_r32(e, F + M_Y), en2_r32(e, F + M_RECT + 8), en2_r32(e, F + M_RECT + 0xc), 0, 0, inst};
            e->call(e->user, 0x004a6be8u, v, 12);
        }
        hwnd = en2_call1(e, 0x004a6dccu, WINDOW);
        en2_call2(e, 0x004a6ee4u, WINDOW, show);
        uint32_t dc = en2_call1(e, I_GetDC, hwnd);
        en2_call2(e, I_GetClientRect, hwnd, F + M_CLIENT);
        uint32_t brush = en2_call1(e, I_GetStockObject, 4 /* BLACK_BRUSH */);
        en2_call3(e, I_FillRect, dc, F + M_CLIENT, brush);
        en2_call2(e, I_ReleaseDC, hwnd, dc);
        en2_call1(e, 0x004c471cu, F + M_RT);
        en2_call1(e, 0x004c4718u, F + M_RT);
        en2_call0(e, 0x00477124u);                          /* game_flow_init: the game */
    }
    en2_call0(e, 0x004775a8u);
    if (en2_call2(e, I_GetWindowRect, hwnd, F + M_RECT) != 0) {
        en2_call3(e, R_OPEN, F + M_KEY, 0x80000002u, 0);
        en2_call3(e, R_WRITE, F + M_KEY, 0x004e327bu, en2_r32(e, F + M_RECT));
        en2_call3(e, R_WRITE, F + M_KEY, 0x004e3287u, en2_r32(e, F + M_RECT + 4));
        r = en2_call1(e, R_FLUSH, F + M_KEY);
    }
    en2_call1(e, 0x004a6c98u, WINDOW);
    en2_call1(e, I_ClipCursor, 0);
    en2_call0(e, I_CoUninitialize);
    en2_call1(e, 0x004c4584u, 0x00bdfb28u);
    return r;
}
