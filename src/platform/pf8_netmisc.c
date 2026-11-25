/*
 * pf8_netmisc.c: three unlisted TM2.EXE helpers of the network game dialog (called from 0x00465f60, see
 * pf8_netrun.c). Ported over Pf2Env (pf2_env.h); emulator diff: tools/emu/check_pf8_netmisc.sh.
 * Calls get the four registers as the original has them (leftovers included).
 *
 *   0x00466e94  hides the dialog: the controls 0x0050fa40 / 3c / 44 / 50 / 54 / 58 / 5c / 60 get their vtable
 *               (+0x34) slot 0 with edx 2, then the window 0x0050fa64 is passed to the import at 0x00c90800
 *               (DestroyWindow); returns its result
 *   0x00466f44  sends the chat line: the text of the control eax (0x004a6ef4, 0x80 bytes) goes out as message
 *               0x11 (0x00463e38: a 0x20 byte header with byte 1 = 2, then the text); then the chat list
 *               0x0050fa44 gets a row (0x004a63d8 / 0x004a64c4) with the local player's name (0x004988e0),
 *               the text and 0x004e1686, and is selected (0x004a625c)
 *   0x00467244  on the host: when every player row of 0x0050fa40 (0x004a6390, +8) is ready, enables the start
 *               button 0x0050fa50 (0x004a6dd8 / 0x004a6e2c; 0x0050fa20 = 1), else disables it again
 */
#include <string.h>
#include "pf2_env.h"
#include "pf8_netmisc.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define G(x) (0x0050fa00u + (x))

typedef struct { Pf2Env* e; uint32_t a, d, b, c; } Regs;
static uint32_t call4(Regs* r, uint32_t fn) {
    uint32_t v[4] = {r->a, r->d, r->b, r->c};
    return r->a = r->e->call(r->e->user, fn, v, 4);
}
#define CALL(fn) call4(&R, (fn))

uint32_t pf8_00466e94(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    static const uint8_t g[8] = {0x40, 0x3c, 0x44, 0x50, 0x54, 0x58, 0x5c, 0x60};
    for (int k = 0; k < 8; k++) {
        uint32_t o = R.a = R32(e, G(g[k]));
        if (!o) continue;
        R.b = R32(e, o + 0x34);
        R.d = 2;
        CALL(R32(e, R.b));
    }
    uint32_t v[1] = {R32(e, G(0x64))};
    return e->call(e->user, 0x00c90800u, v, 1);
}

uint32_t pf8_00466f44(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t sp = e->scratch, txt = sp + 0x400;
    R.b = 0x80;
    R.d = txt;
    CALL(0x004a6ef4u);
    uint32_t n = 0;
    while (pf2_r8(e, txt + n)) n++;
    memmove(e->mem(e->user, sp + 0x20), e->mem(e->user, txt), n + 1);
    pf2_w8(e, sp + 1, 2);
    R.a = 0x11;
    R.d = sp;
    R.b = n + 1 + 0x20;
    R.c = 0;
    CALL(0x00463e38u);
    uint32_t list = R32(e, G(0x44));
    R.c = 0;
    R.a = list;
    R.d = 0x004e1685u;
    uint32_t row = CALL(0x004a63d8u);
    R.b = row;
    R.a = list;
    CALL(0x004a64c4u);
    R.a = R32(e, 0x0050f9dcu);
    uint32_t name = CALL(0x004988e0u);
    static const uint32_t col[3] = {0, 0, 0x004e1686u};
    for (uint32_t k = 0; k < 3; k++) {
        R.b = row;
        R.c = k;
        R.d = k == 0 ? name : k == 1 ? sp + 0x20 : col[k];
        R.a = list;
        CALL(0x004a6564u);
    }
    R.d = row;
    R.a = list;
    R.b = 0;
    return CALL(0x004a625cu);
}

uint32_t pf8_00467244(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    R.a = 0x0050f9a8u;
    if (!CALL(0x004aaa5cu)) return R.a;
    R.a = R32(e, G(0x40));
    R.c = 1;
    int32_t n = (int32_t)CALL(0x004a63d8u);
    R.b = 0;
    for (int32_t i = 0; i < n; i++) {
        R.b = (uint32_t)i;
        R.a = R32(e, G(0x40));
        R.d = (uint32_t)i;
        if (R32(e, CALL(0x004a6390u) + 8) == 0) R.c = 0;
    }
    if (n > 0) R.b = (uint32_t)n;   /* the loop counter ends at the count */
    if (R.c == 1) {
        if (R32(e, G(0x20))) return R.a;
        R.d = 1;
        R.a = R32(e, G(0x50));
        CALL(0x004a6dd8u);
    } else {
        if (R32(e, G(0x20)) != 1) return R.a;
        R.a = R32(e, G(0x50));
        R.d = 0;
        CALL(0x004a6dd8u);
    }
    uint32_t on = R.c == 1;
    R.b = 0;
    R.a = R32(e, G(0x50));
    R.d = 0;
    uint32_t r = CALL(0x004a6e2cu);
    W32(e, G(0x20), on);
    return r;
}
