/*
 * pf6_movie.c: TM2.EXE's movie window / open (MCI avivideo; docs/PLATFORM.md Part C), ported over
 * Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf6_movie.sh.
 * Globals: 0x00c8510c movie window handle, 0x00c85110 playing, 0x00c85114 open, 0x00c85118 double size.
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define I_mciSendStringA 0x00c90618u
#define I_wsprintfA 0x00c90658u
#define I_mciGetErrorStringA 0x00c90610u

static uint32_t mcis(Pf2Env* e, uint32_t cmd, uint32_t ret, uint32_t n) { return pf2_call4(e, I_mciSendStringA, cmd, ret, n, 0); }

/* skip blanks / a token / blanks, as the original's byte loops (no end of string test) */
static uint32_t skip_sp(Pf2Env* e, uint32_t p) { while (pf2_r8(e, p) == ' ') p++; return p; }
static uint32_t skip_tok(Pf2Env* e, uint32_t p) { while (pf2_r8(e, p) != ' ') p++; return p; }
static uint32_t num(Pf2Env* e, uint32_t* p) {
    uint32_t v = 0;
    while (pf2_r8(e, *p) >= '0' && pf2_r8(e, *p) <= '9') { v = v * 10 + (uint32_t)(pf2_r8(e, *p) - '0'); (*p)++; }
    return v;
}

/* 0x004b9c84 movie_set_window(eax x, edx y, ebx w, ecx h): 0x004958d8, 0x004958e4; "where mov
   source" (0x004e3797) into a 0x100 byte buffer; the third and fourth numbers of the answer give
   w / h where the argument is -1 (the parse has no end of string test: an answer with fewer
   fields runs past it, ORIGINAL BUG; not tested); doubled when 0x00c85118 == 1; wsprintfA(cmd,
   "put mov window client at %i %i %i %i" 0x004e37a8, x, y, w, h); mciSendStringA(cmd) returned */
uint32_t pf6_movie_set_window(Pf2Env* e, uint32_t a, uint32_t b, uint32_t w, uint32_t h) {
    uint32_t buf = e->scratch, cmd = e->scratch + 0x100;
    pf2_call0(e, 0x004958d8u);
    pf2_call0(e, 0x004958e4u);
    mcis(e, 0x004e3797u, buf, 0x100);
    uint32_t p = skip_sp(e, buf);
    p = skip_tok(e, p);
    p = skip_sp(e, p);
    p = skip_tok(e, p);
    p = skip_sp(e, p);
    uint32_t x = num(e, &p);
    p = skip_sp(e, p);
    uint32_t y = num(e, &p);
    if (w == 0xffffffffu) w = x;
    if (h == 0xffffffffu) h = y;
    if (R32(e, 0x00c85118u) == 1) { h += h; w += w; }
    pf2_call6(e, I_wsprintfA, cmd, 0x004e37a8u, a, b, w, h);
    return mcis(e, cmd, 0, 0);
}

/* 0x004b9de0 movie_open(eax file, edx a, ebx b, ecx w, stack h; ret 4): a movie open (0x00c85114):
   if 1 "close mov" (0x004e377e); flags cleared. wsprintfA(cmd, "open %s alias mov style child
   parent %d" 0x004e37cd, file, main window) and send it; failure: error box (3, file), open = 0.
   Success: open = 1, movie_set_window(a, b, w, h), "window mov state show" 0x004e37f5, then
   "status mov window handle" 0x004e380b into the buffer: ok -> atoi 0x004c9f83(buffer) ->
   0x00c8510c (the movie window); failure: mciGetErrorStringA, error box 2, open = 0. Returns open == 0. */
uint32_t pf6_movie_open(Pf2Env* e, uint32_t file, uint32_t a, uint32_t b, uint32_t w, uint32_t h) {
    uint32_t buf = e->scratch + 0x100;
    uint32_t o = R32(e, 0x00c85114u);
    if (o) {
        if (o == 1) mcis(e, 0x004e377eu, 0, 0);
        W32(e, 0x00c85114u, 0);
        W32(e, 0x00c85110u, 0);
    }
    uint32_t hwnd = pf2_call1(e, 0x004a6dccu, 0x00bdfcd0u);
    pf2_call4(e, I_wsprintfA, buf, 0x004e37cdu, file, hwnd);
    if (mcis(e, buf, 0, 0) != 0) {
        pf2_call2(e, 0x004bc660u, 3, file);
        W32(e, 0x00c85114u, 0);
    } else {
        W32(e, 0x00c85114u, 1);
        pf6_movie_set_window(e, a, b, w, h);
        mcis(e, 0x004e37f5u, 0, 0);
        uint32_t r = mcis(e, 0x004e380bu, buf, 0x100);
        if (r == 0) {
            W32(e, 0x00c8510cu, pf2_call1(e, 0x004c9f83u, buf));
        } else {
            pf2_call3(e, I_mciGetErrorStringA, r, buf, 0x100);
            pf2_call1(e, 0x004bc660u, 2);
            W32(e, 0x00c85114u, 0);
        }
    }
    return R32(e, 0x00c85114u) == 0;
}
