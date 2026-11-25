/*
 * cb_target.h: target pickers of TM2.EXE 0x00448de4 (in front) and 0x00449048 (behind), used by
 * the homing weapons. shooter i is a human car (is_human != 0) or an AI car; out receives the
 * target's position pointer (a game address: its world object + 0xE8), 0 when none.
 *
 *   An AI shooter in multiplayer (0x0045c348 == 0) always gets human car 0's object + 0xE8.
 *   Pass 1, every AI car j (0x0045b078 count): object = 0x0045ae2c(j), skipped when object + 0xE8
 *     is 0 or its car (0x0045b080(j)) has i32 +0x58C < 1. d = target - shooter position (both
 *     +0xE8, i32 wrap), local = M * d with M the shooter object's matrix (+0xB0, 9 i32;
 *     math_mat_vec_mul_col 0x0048cb80, the verified tm2_mat_vec_mul_col):
 *       front: local.y >= 12, |local.x| <= |local.y| <= 8000, local.z <= 400, nearer (local.y)
 *       behind: local.y <= -12, |local.x| <= |local.y| <= 8000, nearer (|local.y|)
 *   Pass 2 only when pass 1 found none: every human car j (0x0045afb4 count), object 0x0045ae24(j),
 *     not the shooter itself (for a human shooter), the same tests without the z test; the front
 *     picker also needs the car (0x0045afbc(j)) i32 +0x1D8 >= 1 and tests object + 0xE8 for 0
 *     before the self test, the rear one after it and has no health test.
 *   The shooter's object is fetched again for every candidate (0x0045ae24 / 0x0045ae2c).
 *   |INT_MIN| stays INT_MIN (cdq / xor / sub), as the original.
 *
 * Verified against the original under emulation (tools/emu/check_combat_target.sh).
 */
#ifndef TM2_CB_TARGET_H
#define TM2_CB_TARGET_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    int32_t (*singleplayer)(void* user);              /* 0x0045c348 (al) */
    uint32_t (*human_object)(void* user, int32_t i);  /* 0x0045ae24 */
    uint32_t (*ai_object)(void* user, int32_t i);     /* 0x0045ae2c */
    int32_t (*ai_count)(void* user);                  /* 0x0045b078 */
    uint32_t (*ai_get)(void* user, int32_t i);        /* 0x0045b080 */
    int32_t (*human_count)(void* user);               /* 0x0045afb4 */
    uint32_t (*human_get)(void* user, int32_t i);     /* 0x0045afbc */
    void* user;
} CbTargetEnv;

void cb_target_pick_front(const CbTargetEnv* e, int32_t i, uint32_t* out, int is_human);
void cb_target_pick_rear(const CbTargetEnv* e, int32_t i, uint32_t* out, int is_human);

#endif /* TM2_CB_TARGET_H */
