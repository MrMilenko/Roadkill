/*
 * pf8_session.c: two unlisted TM2.EXE session methods (the network session object 0x0050f9a8; called from
 * 0x00465f60, see pf8_netrun.c). Ported over Pf2Env (pf2_env.h); emulator diff: tools/emu/check_pf8_session.sh.
 * Calls get the four registers as the original has them.
 *
 *   0x004aa900  when this +0x10 is set: refreshes the session list (0x00497c7c on +0x38 with +0x24, then the
 *               list view sync 0x00497e34 with the control edx, see pf8_sesslist.c)
 *   0x004aa974  copies the joined players' details into the player table this +0x34 (count +0x24, entries
 *               +0x04..): for each entry whose id (entry +0) 0x00497c50 finds in the session +0x38: +0x04..+0x10
 *               and +0x120..+0x12c, slot +0x114 = its index, +0x11c = -1, +0x130 = 0; returns the table
 *               (through the shared epilogue at 0x004aa7d5)
 */
#include "pf2_env.h"
#include "pf8_session.h"

#define R32 pf2_r32
#define W32 pf2_w32

static uint32_t c4(Pf2Env* e, uint32_t fn, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    uint32_t v[4] = {a, d, b, c};
    return e->call(e->user, fn, v, 4);
}

uint32_t pf8_004aa900(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    if (R32(e, a + 0x10) == 0) return a;
    c4(e, 0x00497c7cu, R32(e, a + 0x38), R32(e, a + 0x24), 0, 2);
    return c4(e, 0x00497e34u, R32(e, a + 0x38), d, 1, 2);
}

uint32_t pf8_004aa974(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    (void)d; (void)b; (void)c;
    for (uint32_t i = 0;; i++) {
        uint32_t t = R32(e, a + 0x34);
        if (!((int32_t)i < (int32_t)R32(e, t + 0x24))) return t;
        uint32_t k = 4 * i;
        uint32_t p = c4(e, 0x00497c50u, R32(e, a + 0x38), R32(e, R32(e, k + t + 4)), k, a);
        if (!p) continue;
#define ENT R32(e, R32(e, a + 0x34) + k + 4)
        static const uint8_t f1[4] = {0x04, 0x08, 0x0c, 0x10};
        for (int j = 0; j < 4; j++) { uint32_t q = ENT; W32(e, q + f1[j], R32(e, p + f1[j])); }
        { uint32_t q = ENT; W32(e, q + 0x114, i); }
        { uint32_t q = ENT; W32(e, q + 0x11c, 0xffffffffu); }
        { uint32_t q = ENT; W32(e, q + 0x130, 0); }
        for (uint32_t o = 0x120; o <= 0x12c; o += 4) { uint32_t q = ENT; W32(e, q + o, R32(e, p + o)); }
#undef ENT
    }
}
