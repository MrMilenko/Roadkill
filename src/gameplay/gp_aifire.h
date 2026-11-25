/*
 * gp_aifire.h: AI weapon choice, ported from TM2.EXE 0x00410010 (PS1 pair: aiWeaponFireDecide).
 *
 * AI car block (0x880 bytes at 0x0050760c + i * 0x880):
 *   +0x30 i16 car type (0xA0 and 0xF0 matter here), +0x32 u8 armed flag, +0xC8 u8 weapon set (0 / 1),
 *   +0x85A i16 hold frames, +0x86C / +0x870 u32 game address of 9 / 10 u8 weights (sum <= 100),
 *   +0x840 u8 chosen action, +0x841 i8 chosen weapon.
 * Unless the car is unarmed (+0x32 == 0) and not type 0xF0: when the hold +0x85A is > 1 or the set
 * is >= 2, nothing is chosen. Else r = rand() / 0x148 (0..99) picks the first weight index whose
 * running sum exceeds r (9 / 10 when none does):
 *   set 0: index < 8 -> (0x6B, index; type 0xA0 turns index 0 into 1 when the current +0x841 is 0),
 *          8 -> (0x64, 8), 9 -> (0x6B, 8)
 *   set 1: index < 8 -> (0x6B, index), 8 -> (0x64, 8), 9 -> (0x65, 8), 10 -> (0x6B, 8)
 * then 0x0043e6e0(car, 0) runs (also on the hold and set >= 2 paths, not on the unarmed one).
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_aifire.sh).
 */
#ifndef TM2_GP_AIFIRE_H
#define TM2_GP_AIFIRE_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;            /* the weight tables */
    void* user;
    void (*act)(void* user, uint8_t* car, int32_t arg);    /* 0x0043e6e0(car, 0) */
} GpAiFireEnv;

void gp_ai_weapon_fire_decide(GpAiFireEnv* env, uint8_t* car);

#endif /* TM2_GP_AIFIRE_H */
