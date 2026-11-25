/*
 * ai_misc.c: small helpers paired with the PS1 AI files, ported from TM2.EXE (see ai.h).
 * Verified against the original under emulation (tools/emu/check_ai_setup.sh).
 */
#include "ai.h"
#include "ai_mem.h"

#include "../core/tm2rand.h"
#include "../physics/grp.h"

void math_jitter_pos(const int32_t pos[3], int32_t dx, int32_t dy, int32_t dz, int32_t out[3]) {
    const int32_t div[3] = { dx, dy, dz };
    for (int k = 0; k < 3; k++) {
        if (div[k] == 0) continue;
        if (tm2_rand() & 1) out[k] = (int32_t)((uint32_t)(tm2_rand() / div[k]) + (uint32_t)pos[k]);
        else out[k] = wsub(pos[k], tm2_rand() / div[k]);
    }
}

void event_objects_find_zones(uint8_t* records, const uint8_t* grp) {
    for (int i = 0; i < 5; i++) {
        uint8_t* r = records + 0xc4 * i;
        if (r[0]) w16(r, 0xc, grp_find_zone(grp, i32(r, 0x44), i32(r, 0x48), i32(r, 0x4c)));
    }
}
