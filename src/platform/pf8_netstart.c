/*
 * pf8_netstart.c: TM2.EXE 0x004672fc, an unlisted function (the network dialog's start / ready button
 * handler; its address is stored by 0x004662b8, see pf8_netdlg.c). Ported over Pf2Env (pf2_env.h); emulator
 * diff: tools/emu/check_pf8_netstart.sh. Calls get the four registers as the original has them.
 *
 * Host (0x004aaa5c): updates the session (0x004aa68c / 0x00498508), counts the players (0x00498968, 0x00498534,
 * 0x00497c50 +0x10); too few for the player count (0 ready, 1 with more than 3, 2 with more than 6) shows a
 * message box (0x004b27e6 sprintf of 0x004e16a0 with the 1 / 2 / 3 player word forms, MessageBoxA on the
 * window 0x004a6dcc(0x00bdfcd0)); else sends the start message (0x00463e38: message 0x11, byte 1 = 0x15,
 * 0x20 bytes) and leaves the dialog (0x0050fa14 = 5, 0x0050fa10 = 0x0050fa08 = 1).
 * Client: toggles ready (0x0050fa1c): not ready -> 0x00498194 / 0x004981e0, player state +0x148 = 2, +0x38 = 1,
 * IDirectPlay slot 0x74 player data; ready -> two 0x14 messages, 0x00498194 / 0x004981e0; then the button
 * 0x0050fa50 text (0x004a4bdc: 0x9d / 0x9e or 0x93 / 0x94) and 0x004a6e2c.
 */
#include "pf2_env.h"
#include "pf8_netstart.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define G(x) (0x0050fa00u + (x))
#define SESSION 0x0050f9a8u

typedef struct { Pf2Env* e; uint32_t a, d, b, c; } Regs;
static uint32_t callk(Regs* r, uint32_t fn, const uint32_t* s, int ns) {
    uint32_t v[8] = {r->a, r->d, r->b, r->c};
    for (int i = 0; i < ns; i++) v[4 + i] = s[i];
    return r->a = r->e->call(r->e->user, fn, v, 4 + ns);
}
#define CALL(fn) callk(&R, (fn), 0, 0)

uint32_t pf8_004672fc(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t pkt = e->scratch, txt = e->scratch + 0x400;
    uint32_t edi = 1;
    R.a = SESSION;
    uint32_t host = CALL(0x004aaa5cu);
    R.c = 0;
    if (host) {
        R.a = SESSION;
        uint32_t s = CALL(0x004aa68cu);
        R.b = R32(e, 0x0050f9dcu);
        R.d = s;
        R.a = R.b;
        CALL(0x00498508u);
        R.a = R32(e, 0x0050f9dcu);
        int32_t n = (int32_t)CALL(0x00498968u);
        R.b = 0;
        for (int32_t i = 0; i < n;) {
            R.a = R32(e, 0x0050f9dcu);
            R.d = R.b;
            uint32_t pl = CALL(0x00498534u);
            R.d = R32(e, pl);
            R.a = R32(e, 0x0050f9e0u);
            R.a = R32(e, CALL(0x00497c50u) + 0x10) != 0;
            R.b = (uint32_t)++i;
            R.c += R.a;
        }
        uint32_t k = R.c;
        if (k == 0 || (k == 1 && n > 3) || (k == 2 && n > 6)) {
            R.a = 0x00bdfcd0u;
            uint32_t hwnd = CALL(0x004a6dccu);
            R.d = hwnd;
            if (n > 3) edi = 2;
            if (n > 6) edi = 3;
            int many = (int32_t)edi > 1;
            uint32_t s7[7] = {txt, 0x004e16a0u, many ? 0x004e1695u : 0x004e169fu, edi,
                              many ? 0x004e1692u : 0x004e1694u, (uint32_t)n, many ? 0x004e168fu : 0x004e1691u};
            e->call(e->user, 0x004b27e6u, s7, 7);   /* sprintf (cdecl, no register arguments) */
            return pf2_call4(e, 0x00c90524u, hwnd, txt, 0x004e16c6u, 0x30);   /* MessageBoxA */
        }
        pf2_w8(e, pkt + 1, 0x15);
        R.b = 0x20;
        R.a = 0x11;
        R.d = pkt;
        R.c = 5;
        uint32_t r = CALL(0x00463e38u);
        W32(e, G(0x14), 5);
        W32(e, G(0x10), 1);
        W32(e, G(0x08), 1);
        return r;
    }
    uint32_t f = R.d = R32(e, G(0x1c));
    if (f == 0) {
        R.d = R32(e, 0x0050f9dcu);
        R.a = R32(e, 0x0050f9e0u);
        R.b = edi;
        R.d = R32(e, R.d + 0x30);
        CALL(0x00498194u);
        R.d = R32(e, 0x0050f9dcu);
        R.b = 2;
        R.a = R32(e, 0x0050f9e0u);
        R.d = R32(e, R.d + 0x30);
        CALL(0x004981e0u);
        uint32_t p = R32(e, 0x0050f9dcu);
        W32(e, p + 0x148, 2);
        W32(e, p + 0x38, edi);
        p = R.d = R32(e, 0x0050f9dcu);
        R.b = p + 0x30;
        R.a = R32(e, 0x0050f9ccu);
        R.c = R32(e, R.a);
        R.a = PF2_C4(e, PF2_IUNK, 0x74, R.a, R32(e, p + 0x30), p + 0x30, 0x138, 2);
    } else if (f == edi) {
        pf2_w8(e, pkt + 1, 0x14);
        for (int k = 0; k < 2; k++) {
            R.b = 0x20;
            R.d = pkt;
            R.a = 0x11;
            CALL(0x00463e38u);
        }
        R.d = R32(e, 0x0050f9dcu);
        R.a = R32(e, 0x0050f9e0u);
        R.b = 0;
        R.d = R32(e, R.d + 0x30);
        CALL(0x00498194u);
        R.d = R32(e, 0x0050f9dcu);
        R.a = R32(e, 0x0050f9e0u);
        R.b = edi;
        R.d = R32(e, R.d + 0x30);
        CALL(0x004981e0u);
    }
    uint32_t g = R32(e, G(0x1c));
    if (g == 0 || g == 1) {
        static const uint32_t z[1] = {0};
        R.a = R32(e, G(0x50));
        R.c = 0;
        W32(e, G(0x1c), g == 0);
        R.b = g == 0 ? 0x9d : 0x93;
        R.d = g == 0 ? 0x9e : 0x94;
        callk(&R, 0x004a4bdcu, z, 1);
    }
    R.a = R32(e, G(0x50));
    R.b = 0;
    R.d = 0;
    return CALL(0x004a6e2cu);
}
