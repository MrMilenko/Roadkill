/*
 * cb_round.h: weapon state reset at the start of a round, TM2.EXE 0x00447b70 (PS1 weaponsRoundReset).
 *
 * Memory through a WMem (game addresses; objects pointed to must be mapped too):
 *   0x004ffed8 i32  = (300 - 4 * event type) >> min(difficulty, 2)   (event type: i8 of 0x00474508,
 *                     difficulty: 0x0045b3dc, signed compare; the shift count is taken mod 32)
 *   0x004ff308  30 ground fires of 0x28: +0 u8, +4 i16, +8, +0x0C, +0x10, +0x14..+0x24 i32 = 0
 *   0x004ff7fc  5 x 0x14: +0 i16 = -1           0x004ff7b8, 0x004ff7d8 i16 = 0
 *   0x004ff8d4  10 x 0x30: +0 i16 = 0, object +0x28: +8 = 0 and dropped when set
 *   0x004ffac8  4 x 0xC: +0 i16, +2 i16, +4 i32 = 0, object +8: +8 = 0 and dropped when set
 *   0x004ffe42 i16 = 0, 0x004ffe34 u8 = 0; when i16 0x004ffe20 > 0: it = 0 and the object
 *                     0x004ffe1c (when set) gets +8 = 0 (kept)
 *   0x004ffe74, 0x004ffe76 i16 = 0
 *   0x004ff85c  claw records of 0x12, count i32 0x004ff8a4 (when > 0): +0 0, +1 1, +2 0, +3 1,
 *                     +7 0, +0x0B 0 (u8), +0x10 i16 0
 * then scene_rotate_a 0x00453f10(0x2E3) and scene_rotate_b 0x00453ed0(-0x71).
 *
 * Verified against the original under emulation (tools/emu/check_combat_round.sh).
 */
#ifndef TM2_CB_ROUND_H
#define TM2_CB_ROUND_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    int32_t (*difficulty)(void* user);                 /* ai_get_difficulty 0x0045b3dc */
    int32_t (*event_type)(void* user);                 /* 0x00474508 (its al is used) */
    void (*scene_rotate_a)(void* user, int32_t angle); /* 0x00453f10 */
    void (*scene_rotate_b)(void* user, int32_t angle); /* 0x00453ed0 */
    void* user;
} CbRoundEnv;

void cb_weapons_round_reset(const CbRoundEnv* e);

#endif /* TM2_CB_ROUND_H */
