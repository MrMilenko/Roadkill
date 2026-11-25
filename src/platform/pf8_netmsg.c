/*
 * pf8_netmsg.c: six unlisted TM2.EXE helpers of the network game dialog (handlers stored by 0x004662b8, see
 * pf8_netdlg.c, and the message senders they use). Ported over Pf2Env (pf2_env.h); emulator diff:
 * tools/emu/check_pf8_netmsg.sh. Calls get the four registers as the original has them (leftovers included).
 * Messages are 0x20 byte headers on the stack (byte 1 the kind, +0x10.. the words; the rest is left as it is).
 *
 *   0x00463d1c (eax kind, edx, ebx, ecx, stack s; ret 4)   message 0x11 to all (0x00463e38): kind, edx, ebx, ecx, s
 *   0x00463d58 (eax player, dl kind, ebx, ecx, stack s0 s1; ret 8)   the same to one player (0x00463d9c)
 *   0x00467018 (eax control, edx player)   sends the control's text (0x004a6ef4) to the player as kind 3, and
 *               adds a chat row (0x004a63d8 / 0x004a64c4 / 0x004a6564 x3: our name 0x004988e0, the text, the
 *               player's name = 0x00497c50 +0x14) selected with 0x004a625c
 *   0x004675c0  the private chat: for every selected player row of 0x0050fa40 (0x004a6404 / 0x004a6390) sends the
 *               line (0x00467018), then clears the edit control 0x0050fa3c (0x004a6f08 with 0x004e16d4)
 *   0x00467638  toggles the "ready" button 0x0050fa4c (0x0050fa24; 0x004aaa88 / 0x004aaacc, 0x004a4bdc) and sends
 *               message kind 0x0e with it (0x00463d1c), then 0x004a6e2c
 *   0x004676d0  on the host: sends kind 0x0d to every selected player row of 0x0050fa40 (0x00463d58)
 */
#include <string.h>
#include "pf2_env.h"
#include "pf8_netmsg.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define G(x) (0x0050fa00u + (x))

typedef struct { Pf2Env* e; uint32_t a, d, b, c; } Regs;
static uint32_t callk(Regs* r, uint32_t fn, const uint32_t* s, int ns) {
    uint32_t v[8] = {r->a, r->d, r->b, r->c};
    for (int i = 0; i < ns; i++) v[4 + i] = s[i];
    return r->a = r->e->call(r->e->user, fn, v, 4 + ns);
}
#define CALL(fn) callk(&R, (fn), 0, 0)
static const uint32_t Z2[2] = {0, 0};

uint32_t pf8_00463d1c(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c, uint32_t s) {
    uint32_t p = e->scratch;
    pf2_w8(e, p + 1, (uint8_t)a);
    W32(e, p + 0x10, d);
    W32(e, p + 0x14, b);
    W32(e, p + 0x1c, s);
    W32(e, p + 0x18, c);
    Regs R = {e, 0x11, p, 0x20, c};
    return CALL(0x00463e38u);
}

uint32_t pf8_00463d58(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c, uint32_t s0, uint32_t s1) {
    uint32_t p = e->scratch;
    W32(e, p + 0x10, b);
    pf2_w8(e, p + 1, (uint8_t)d);
    W32(e, p + 0x18, s0);
    W32(e, p + 0x14, c);
    W32(e, p + 0x1c, s1);
    Regs R = {e, a, d & 0xff, p, 0x20};
    return CALL(0x00463d9cu);
}

uint32_t pf8_00467018(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t sp = e->scratch, txt = sp + 0x400, who = d;
    R.b = 0x80;
    R.d = txt;
    CALL(0x004a6ef4u);
    uint32_t n = 0;
    while (pf2_r8(e, txt + n)) n++;
    memmove(e->mem(e->user, sp + 0x20), e->mem(e->user, txt), n + 1);
    pf2_w8(e, sp + 1, 3);
    R.b = sp;
    R.c = n + 1 + 0x20;
    R.d = 3;
    R.a = who;
    CALL(0x00463d9cu);
    R.a = R32(e, 0x0050f9e0u);
    R.d = who;
    uint32_t info = CALL(0x00497c50u);
    R.c = 0;
    uint32_t list = R32(e, G(0x44));
    R.a = list;
    R.d = 0x004e168au;
    uint32_t row = CALL(0x004a63d8u);
    R.b = row;
    R.a = list;
    CALL(0x004a64c4u);
    R.a = R32(e, 0x0050f9dcu);
    uint32_t name = CALL(0x004988e0u);
    for (uint32_t k = 0; k < 3; k++) {
        R.b = row;
        R.c = k;
        R.d = k == 0 ? name : k == 1 ? sp + 0x20 : info + 0x14;
        R.a = list;
        CALL(0x004a6564u);
    }
    R.d = row;
    R.a = list;
    R.b = 0;
    return CALL(0x004a625cu);
}

uint32_t pf8_004675c0(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    R.a = R32(e, G(0x40));
    int32_t n = (int32_t)CALL(0x004a63d8u);
    R.b = 0;
    R.c = (uint32_t)n;
    for (int32_t i = 0; i < n; i++) {
        R.b = (uint32_t)i;
        R.a = R32(e, G(0x40));
        R.d = (uint32_t)i;
        if (!CALL(0x004a6404u)) continue;
        R.a = R32(e, G(0x40));
        R.d = (uint32_t)i;
        R.d = R32(e, CALL(0x004a6390u));
        R.a = R32(e, G(0x3c));
        CALL(0x00467018u);
    }
    if (n > 0) R.b = (uint32_t)n;
    R.d = 0x004e16d4u;
    R.a = R32(e, G(0x3c));
    return CALL(0x004a6f08u);
}

uint32_t pf8_00467638(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t z[1] = {0};
    R.d = R32(e, G(0x24));
    if (R.d == 0) {
        R.a = 0x0050f9a8u;
        R.b = 0x9b;
        CALL(0x004aaa88u);
        R.c = 0;
        z[0] = R.d;
        R.a = R32(e, G(0x4c));
        R.d = 0x9c;
        callk(&R, 0x004a4bdcu, z, 1);
        R.d = 1;
        W32(e, G(0x24), 1);
    } else {
        R.a = 0x0050f9a8u;
        R.b = 0x8d;
        R.d = 0x8e;
        CALL(0x004aaaccu);
        R.c = 0;
        R.a = R32(e, G(0x4c));
        callk(&R, 0x004a4bdcu, z, 1);
        R.d = 0;
        W32(e, G(0x24), 0);
    }
    R.a = 0xe;
    R.b = 0;
    R.c = 0;
    z[0] = 0;
    callk(&R, 0x00463d1cu, z, 1);
    R.a = R32(e, G(0x4c));
    R.b = 0;
    R.d = 0;
    return CALL(0x004a6e2cu);
}

uint32_t pf8_004676d0(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    R.a = 0x0050f9a8u;
    if (!CALL(0x004aaa5cu)) return R.a;
    R.a = R32(e, G(0x40));
    int32_t n = (int32_t)CALL(0x004a63d8u);
    for (int32_t i = 0; i < n; i++) {
        R.a = R32(e, G(0x40));
        R.d = (uint32_t)i;
        if (!CALL(0x004a6404u)) continue;
        R.a = R32(e, G(0x40));
        R.d = (uint32_t)i;
        uint32_t row = CALL(0x004a6390u);
        R.c = 0;
        R.b = 0;
        R.d = 0xd;
        R.a = R32(e, row);
        callk(&R, 0x00463d58u, Z2, 2);
    }
    return R.a;
}
