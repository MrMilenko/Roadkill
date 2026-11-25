/*
 * pf8_netrun.c: TM2.EXE 0x00465f60, an unlisted function (reached from 0x0047a308, the network screens):
 * runs the network game dialog until it is left. Ported over Pf2Env (pf2_env.h); emulator diff:
 * tools/emu/check_pf8_netrun.sh. Calls get the four registers as the original has them (leftovers included;
 * a COM call leaves ecx / edx as the original had them before it, as the stubs do).
 *
 * Globals: 0x0050f9a8 the session (0x004aaa5c: host), 0x0050f9cc the IDirectPlay, 0x0050f9dc the player list
 * (+0x30 the local player id, +0x38 / +0x3c / +0x148 its state), 0x0050f9e0, 0x0050fa10 leave, 0x0050fa14 the
 * result (1 = start), 0x0050fa18 show the message box, 0x0050fa1c / 0x0050fa20 / 0x0050fa28 dialog state,
 * 0x0050fa08 / 0x0050fa0c, the controls 0x0050fa3c.. (see pf8_netdlg.c), 0x00624ea9.. the game settings.
 * Keys (0x004936ac): 0x14 leave, 0x15 start, 0x12 send the chat line (0x004a6ef4 / 0x004a6f9c / 0x00466f44).
 */
#include "pf2_env.h"
#include "pf8_netrun.h"

#define R32 pf2_r32
#define W32 pf2_w32

typedef struct { Pf2Env* e; uint32_t a, d, b, c; } Regs;
static uint32_t call4(Regs* r, uint32_t fn) {
    uint32_t v[4] = {r->a, r->d, r->b, r->c};
    return r->a = r->e->call(r->e->user, fn, v, 4);
}
#define CALL(fn) call4(&R, (fn))
#define SESSION 0x0050f9a8u
#define HOST 0x004aaa5cu
#define G(x) (0x0050fa00u + (x))

/* IDirectPlay slot 0x74 (the player data of the local player: id, &id, 0x138, 2) */
static void player_data(Pf2Env* e, uint32_t p) {
    PF2_C4(e, PF2_IUNK, 0x74, R32(e, 0x0050f9ccu), R32(e, p + 0x30), p + 0x30, 0x138, 2);
}

uint32_t pf8_00465f60(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t buf = e->scratch;
    R.d = 0;
    W32(e, G(0x1c), 0);
    CALL(0x004662b8u);
    R.a = SESSION;
    if (CALL(HOST)) {
        R.d = R32(e, 0x0050f9dcu);
        R.b = 1;
        R.a = R32(e, 0x0050f9e0u);
        R.d = R32(e, R.d + 0x30);
        CALL(0x00498250u);
        uint32_t p = R.a = R32(e, 0x0050f9dcu);
        R.c = 1;
        W32(e, p + 0x3c, 1);
        W32(e, p + 0x148, 4);
        R.d = 0;
        W32(e, p + 0x38, 1);
        R.a = R32(e, G(0x50));
        W32(e, G(0x28), 1);
        CALL(0x004a6dd8u);
        W32(e, G(0x20), 0);
    } else {
        R.d = R32(e, 0x0050f9dcu);
        R.a = R32(e, 0x0050f9e0u);
        R.b = 0;
        R.d = R32(e, R.d + 0x30);
        CALL(0x00498250u);
        R.d = R32(e, 0x0050f9dcu);
        R.b = 1;
        R.a = R32(e, 0x0050f9e0u);
        R.d = R32(e, R.d + 0x30);
        CALL(0x004981e0u);
        uint32_t p = R.a = R32(e, 0x0050f9dcu);
        W32(e, p + 0x3c, 0);
        R.b = 0;
        W32(e, p + 0x148, 1);
        W32(e, G(0x28), 0);
        W32(e, p + 0x38, 0);
    }
    {
        uint32_t p = R32(e, 0x0050f9dcu);
        R.b = p + 0x30;
        R.d = R32(e, 0x0050f9ccu);
        R.c = R32(e, R.d);
        player_data(e, p);
        R.a = 0;   /* the COM result: not used before the next call sets eax */
    }
    W32(e, G(0x10), 0);
    W32(e, G(0x14), 0);
    W32(e, G(0x18), 0);
    do {
        R.d = R32(e, G(0x40));
        R.a = SESSION;
        CALL(0x004aa900u);
        CALL(0x00463f78u);
        R.a = SESSION;
        if (CALL(HOST) && R32(e, G(0x28)) == 0) {
            CALL(0x00466e94u);
            CALL(0x004662b8u);
            R.d = R32(e, 0x0050f9dcu);
            R.a = R32(e, 0x0050f9e0u);
            R.b = 1;
            R.d = R32(e, R.d + 0x30);
            CALL(0x00498250u);
            uint32_t p = R32(e, 0x0050f9dcu);
            W32(e, p + 0x148, 4);
            W32(e, p + 0x3c, 1);
            W32(e, p + 0x38, 1);
            R.c = R32(e, p + 0x30);
            R.d = R32(e, 0x0050f9ccu);
            R.b = R32(e, R.d);
            player_data(e, p);
            R.a = R32(e, G(0x50));
            R.d = 0;
            W32(e, G(0x28), 1);
            CALL(0x004a6dd8u);
            W32(e, G(0x20), 0);
        }
        CALL(0x00467244u);
        CALL(0x00493748u);
        R.a = 0x14;
        R.d = 0;
        if (CALL(0x004936acu) & 0xff) {
            R.a = 0;
            W32(e, G(0x10), 1);
            W32(e, G(0x14), 0);
        }
        R.a = 0x15;
        R.d = 0;
        if (CALL(0x004936acu) & 0xff) {
            W32(e, G(0x14), 1);
            W32(e, G(0x10), 1);
        }
        R.a = 0x12;
        R.d = 0;
        if (CALL(0x004936acu) & 0xff) {
            R.b = 0x80;
            R.d = buf;
            R.a = R32(e, G(0x3c));
            CALL(0x004a6ef4u);
            R.a = R32(e, G(0x3c));
            if (CALL(0x004a6f9cu)) {
                R.d = 0x004e154cu;
                R.a = buf;
                if (CALL(0x004a91b0u)) {
                    R.a = R32(e, G(0x3c));
                    R.d = 0x004e154du;
                    CALL(0x00466f44u);
                    R.a = R32(e, G(0x3c));
                    CALL(0x004a6f08u);
                }
            }
        }
    } while (R32(e, G(0x10)) != 1);
    R.b = R32(e, G(0x14));
    if (R.b == 0 || R.b == 1) {
        R.a = SESSION;
        CALL(0x004aa638u);
        if (R32(e, G(0x18))) {
            uint32_t hwnd = CALL(0x004a8f34u);
            R.a = pf2_call4(e, 0x00c90524u, hwnd, 0x004e155fu, 0x004e154eu, 0x30);   /* MessageBoxA */
        }
    }
    if (R32(e, G(0x08)) || R32(e, G(0x0c))) {
        R.a = SESSION;
        uint32_t r = CALL(0x004aa68cu);
        R.b = R32(e, 0x0050f9dcu);
        R.d = r;
        R.a = R.b;
        CALL(0x00498508u);
        R.a = SESSION;
        CALL(0x004aa974u);
        R.a = SESSION;
        CALL(0x004aaa88u);
        R.a = R32(e, 0x0050f9dcu);
        R.b = (R.b & 0xffffff00u) | 0xff;
        CALL(0x00498968u);
        pf2_w8(e, 0x00624ea9u, (uint8_t)R.a);
        R.a &= 0xffff00ffu;
        R.d = 0;
        pf2_w8(e, 0x00624eabu, 0);
        do { R.d++; pf2_w8(e, 0x00624eb4u + R.d, 0xff); } while ((int32_t)R.d < 0xd);
        R.c = (R.c & 0xffffff00u) | 5;
        R.b &= 0xffff00ffu;
        R.d = 0;
        pf2_w8(e, 0x00624eaau, 0);
        for (;;) {
            R.a = R32(e, 0x0050f9dcu);
            if (!((int32_t)R.d < (int32_t)CALL(0x00498968u))) break;
            R.d++;
            pf2_w8(e, 0x00624ebeu + R.d, 5);
        }
    }
    CALL(0x00466e94u);
    R.a = SESSION;
    if (CALL(HOST)) {
        static const uint32_t g[2] = {0x48, 0x4c};
        for (int k = 0; k < 2; k++) {
            uint32_t o = R32(e, G(g[k]));
            R.a = o;
            if (o) {
                R.b = R32(e, o + 0x34);
                R.d = 2;
                CALL(R32(e, R.b));
            }
        }
    }
    return R32(e, G(0x14));
}
