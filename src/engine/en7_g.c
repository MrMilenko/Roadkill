/*
 * en7_g.c: see en7_g.h. Network object o: +0 DirectPlay, +0x1f4 list mutex, +0x1f8 state mutex,
 * +0x208 state (0 = enumerate sessions, 1 = refresh players, 2 = busy), +0x20c running, session
 * list +0x214 head / +0x218 tail / +0x220 count (next +0x1cc), +0x224 list box.
 */
#include "en7_g.h"

#define WAIT  0x00c9030cu
#define REL   0x00c902fcu
#define PEEK  0x00c90530u
#define SLEEP 0x00c90304u

static void lock(En2Env* e, uint32_t mtx) { en2_call2(e, WAIT, mtx, 0xffffffffu); }

/* 0x004ac134: the network thread (stdcall; its first argument is o): runs while +0x20c (a posted
 * WM_QUIT clears it) every 50 ms: with no thread message and a DirectPlay object, state 0 marks all
 * sessions stale and enumerates them (0x004ab43c), then state 1; state 1 purges stale sessions
 * (0x004ab2f0) and refreshes each one's players (0x00497c7c(node + 0x1b0, dp, node + 4, 1)), then
 * state 0. Returns 0; pops two stack words. */
uint32_t en7_net_thread(En2Env* e, uint32_t o, uint32_t arg1)
{
    (void)arg1;
    const uint32_t msg = e->scratch;
    en2_w32(e, o + 0x20c, 1);
    while (en2_r32(e, o + 0x20c) != 0) {
        uint32_t v[5] = {msg, 0, 0, 0, 1};
        if (e->call(e->user, PEEK, v, 5) != 0) {
            if (en2_r32(e, msg + 4) == 0x12) en2_w32(e, o + 0x20c, 0);
        } else if (en2_r32(e, o) != 0) {
            lock(e, en2_r32(e, o + 0x1f8));
            uint32_t mtx = en2_r32(e, o + 0x1f8);
            uint32_t st = en2_r32(e, o + 0x208);
            en2_call1(e, REL, mtx);
            if (st == 0) {
                lock(e, en2_r32(e, o + 0x1f8));
                mtx = en2_r32(e, o + 0x1f8);
                en2_w32(e, o + 0x208, 2);
                en2_call1(e, REL, mtx);
                lock(e, en2_r32(e, o + 0x1f4));
                for (uint32_t p = en2_r32(e, o + 0x214); p != 0; p = en2_r32(e, p + 0x1cc)) en2_w32(e, p, 1);
                en2_call1(e, REL, en2_r32(e, o + 0x1f4));
                en2_call1(e, 0x004ab43cu, o);
                lock(e, en2_r32(e, o + 0x1f8));
                mtx = en2_r32(e, o + 0x1f8);
                en2_w32(e, o + 0x208, 1);
                en2_call1(e, REL, mtx);
            } else if (st == 1) {
                en2_call1(e, 0x004ab2f0u, o);
                lock(e, en2_r32(e, o + 0x1f4));
                for (uint32_t p = en2_r32(e, o + 0x214); p != 0; p = en2_r32(e, p + 0x1cc))
                    en2_call4(e, 0x00497c7cu, p + 0x1b0, en2_r32(e, o), p + 4, 1);
                en2_call1(e, REL, en2_r32(e, o + 0x1f4));
                lock(e, en2_r32(e, o + 0x1f8));
                mtx = en2_r32(e, o + 0x1f8);
                en2_w32(e, o + 0x208, 0);
                en2_call1(e, REL, mtx);
            }
        }
        en2_call1(e, SLEEP, 0x32);
    }
    return 0;
}

/* 0x004ac2f0: EnumSessionsCallback2 (stdcall; DPSESSIONDESC2 sd, timeout, flags, o): DPESC_TIMEDOUT
 * ends (0). Under the list mutex the session with guidInstance (sd + 8) is found or a new node
 * (operator new 0x1d0, constructor 0x004979fc on node + 0x1b0) appended (count +0x220, its name
 * [sd + 0x30] added to the list box +0x224 by 0x004a53c4); the node: +0 = 0 (not stale), GUID at
 * +4, +0x14 / +0x18 = sd +0x28 / +0x2c (max / current players), name at +0x20, +0x1a0..+0x1ac =
 * sd +0x40..+0x4c (the user words). Returns 1. ORIGINAL BUG: when the node cannot be made it
 * returns 0 with the mutex still held. */
uint32_t en7_enum_sessions_cb(En2Env* e, uint32_t sd, uint32_t timeout, uint32_t flags, uint32_t o)
{
    (void)timeout;
    if (flags & 1) return 0;
    lock(e, en2_r32(e, o + 0x1f4));
    uint32_t p = en2_r32(e, o + 0x214);
    for (; p != 0; p = en2_r32(e, p + 0x1cc)) {
        int eq = 1;
        for (uint32_t k = 0; k < 16; k++) if (en2_r8(e, sd + 8 + k) != en2_r8(e, p + 4 + k)) { eq = 0; break; }
        if (eq) break;
    }
    if (p == 0) {
        p = en2_call1(e, 0x004a8f48u, 0x1d0);
        if (p != 0) p = en2_call1(e, 0x004979fcu, p + 0x1b0) - 0x1b0;
        if (p == 0) return 0;
        en2_w32(e, p + 0x1cc, 0);
        if (en2_r32(e, o + 0x214) == 0) {
            en2_w32(e, o + 0x218, p);
            en2_w32(e, o + 0x214, p);
        } else {
            en2_w32(e, en2_r32(e, o + 0x218) + 0x1cc, p);
            en2_w32(e, o + 0x218, p);
        }
        en2_w32(e, o + 0x220, en2_r32(e, o + 0x220) + 1);
        uint32_t lb = en2_r32(e, o + 0x224);
        if (lb != 0) en2_call3(e, 0x004a53c4u, lb, en2_r32(e, sd + 0x30), p);
    }
    en2_w32(e, p, 0);
    for (uint32_t k = 0; k < 16; k += 4) en2_w32(e, p + 4 + k, en2_r32(e, sd + 8 + k));
    en2_w32(e, p + 0x14, en2_r32(e, sd + 0x28));
    en2_w32(e, p + 0x18, en2_r32(e, sd + 0x2c));
    en2_call2(e, 0x00c9031cu, p + 0x20, en2_r32(e, sd + 0x30));
    for (uint32_t k = 0; k < 16; k += 4) en2_w32(e, p + 0x1a0 + k, en2_r32(e, sd + 0x40 + k));
    en2_call1(e, REL, en2_r32(e, o + 0x1f4));
    return 1;
}
