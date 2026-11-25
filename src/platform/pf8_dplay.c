/*
 * pf8_dplay.c: TM2.EXE's DirectPlay player list (unlisted functions, tools/rt/unlisted.tsv): 0x00498508
 * enumerates the players of a session into the player list object and 0x004989d0 is its callback.
 * Ported over Pf2Env (pf2_env.h); emulator diff: tools/emu/check_pf8_dplay.sh.
 *
 * Player list object: +0x04 + 4 i the player records (0x138 bytes, made by 0x00498498), +0x24 players
 * listed, +0x28 remote players, +0x2c next free record, +0x30 the local player's id. Record: +0x00 id,
 * +0x04..+0x0c 0, +0x14 short name, +0x94 long name, +0x114 the record index (0 = local), +0x118 = 5,
 * +0x11c..+0x130 = -1.
 */
#include "pf2_env.h"
#include "pf8_dplay.h"

#define R32 pf2_r32
#define W32 pf2_w32

/* 0x004989d0 EnumPlayersCallback (stdcall: id, type, DPNAME*, flags, context; ret 0x14): up to 8 players;
   the local player (id == +0x30) goes to record 0 (only its slot fields are set), the others to the next
   free record with their names (lstrcpyA of the DPNAME's short (+8) / long (+0xc) name). Returns 1 to go
   on, 0 when the list is full. */
uint32_t pf8_004989d0(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c, uint32_t id, uint32_t type,
                      uint32_t name, uint32_t flags, uint32_t ctx) {
    (void)a; (void)d; (void)b; (void)c; (void)type; (void)flags;
    if ((int32_t)R32(e, ctx + 0x24) >= 8) return 0;
    if (id == R32(e, ctx + 0x30)) {
        uint32_t p = R32(e, ctx + 4);
        W32(e, p + 0x114, 0);
        W32(e, ctx + 0x24, R32(e, ctx + 0x24) + 1);
        W32(e, p + 0x118, 5);
        W32(e, p + 0x11c, 0xffffffffu);
        W32(e, p + 0x130, 0xffffffffu);
        return 1;
    }
    uint32_t k = R32(e, ctx + 0x2c);
    uint32_t p = R32(e, ctx + 4 * k + 4);
    W32(e, p + 0x114, k);
    W32(e, ctx + 0x2c, k + 1);
    W32(e, ctx + 0x28, R32(e, ctx + 0x28) + 1);
    W32(e, ctx + 0x24, R32(e, ctx + 0x24) + 1);
    W32(e, p + 4, 0);
    W32(e, p + 8, 0);
    W32(e, p + 0xc, 0);
    W32(e, p + 0x118, 5);
    for (uint32_t o = 0x11c; o <= 0x130; o += 4) W32(e, p + o, 0xffffffffu);
    W32(e, p, id);
    pf2_call2(e, 0x00c9031cu, p + 0x14, R32(e, name + 8));
    pf2_call2(e, 0x00c9031cu, p + 0x94, R32(e, name + 0xc));
    return 1;
}

/* 0x00498508 (eax list, edx IDirectPlay*): clears the list (0x00498380(list, 1)), then
   IDirectPlay::EnumPlayers (slot 0x30)(NULL, 0x004989d0, list, 0); returns the players listed (+0x24) */
uint32_t pf8_00498508(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    pf2_call4(e, 0x00498380u, a, 1, a, d);
    if (d) PF2_C4(e, PF2_IUNK, 0x30, d, 0, 0x004989d0u, a, 0);
    return R32(e, a + 0x24);
}
