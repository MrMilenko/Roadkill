/*
 * weapon_hit.h: what a weapon hit does to what it hit, ported from TM2.EXE 0x00415d1c
 * (PS1 decomp: weaponApplyHit, src/semantic/weapon_missile_trails.c).
 *
 * uint8_t weapon_apply_hit(hit, rtype, rc, damage, velocity, owner):
 *   attacker = the owner's car id (0x0045b178 finds it: car block +2 via 0x0045afbc, or AI block +6
 *              via 0x0045b080), -1 when the owner is no car
 *   hit 500..5999 except 0xDAF / 0xDB0: a level object: rtype / rc = i16 +0 / +2 of entry hit - 500
 *              (20 bytes each) of the table 0x00470d08 returns, hit becomes 9
 *   hit 0xDAF / 0xDB0: rtype = hit
 *   hit >= 6000: 0x004723bc(hit - 6000, velocity) (destructible object), then the rtype switch
 *   hit 1..99 except 9 (a car): 0x004210e4(hit, damage, normalized velocity, 1, attacker), returns 1
 *   otherwise by rtype (unsigned): 0x1F4..0x1FE, 0x207, 0x208: 0x00470d3c(rtype, rc, damage) when the
 *              owner is below 0x32, returns 1; 0xBBD / 0xBBE: 0x00411b30(damage) (Moscow dish), 1;
 *              0xDAF / 0xDB0: 0x00472b70((int16)(hit - 0xDAF), (int16)damage), 1; anything else 0.
 * DECOMPILER TRAP: Ghidra drops the attacker stack argument of 0x004210e4 and both arguments of
 *   0x00472b70, and shows 0x00470d3c's damage as 16 bit (all 32 bits are passed).
 * ORIGINAL BUG (or intent): 0x00472b70 gets hit - 0xDAF, not rtype - 0xDAF, so a level hit (9)
 *   whose record is 0xDAF / 0xDB0 passes (int16)(9 - 0xDAF).
 * The return value is only meaningful in AL (0 leaves the rest of EAX as it was).
 * The velocity normalize is 0x0048b5a0 (src/scene/car_collide.c collide_normalize).
 *
 * Verified against the original under emulation (tools/emu/check_weapons_hit.sh).
 */
#ifndef TM2_WEAPON_HIT_H
#define TM2_WEAPON_HIT_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;    /* car / AI blocks, the hit table, the velocity */
    void* user;
    /* 0x0045b178(owner, &is_car, &index): nonzero when the owner is a car */
    int (*car_lookup)(void* user, int32_t owner, uint8_t* is_car, int32_t* index);
    uint32_t (*car_data)(void* user, int32_t i);     /* 0x0045afbc */
    uint32_t (*ai_data)(void* user, int32_t i);      /* 0x0045b080 */
    uint32_t (*hit_table)(void* user);               /* 0x00470d08 */
    void (*object_hit)(void* user, int32_t id, const int32_t vel[3]);                 /* 0x004723bc */
    void (*car_damage)(void* user, int32_t car, int32_t dmg, const int32_t dir[3], int32_t one,
                       int32_t attacker);                                             /* 0x004210e4 */
    void (*level_damage)(void* user, int32_t rtype, int32_t rc, int32_t dmg);         /* 0x00470d3c */
    void (*moscow_dish)(void* user, int32_t dmg);                                     /* 0x00411b30 */
    void (*eiffel_hit)(void* user, int32_t which, int32_t dmg);                       /* 0x00472b70 */
} HitEnv;

uint8_t weapon_apply_hit(const HitEnv* env, int32_t hit, int32_t rtype, int32_t rc, int32_t damage, uint32_t vel,
                         int32_t owner);

#endif /* TM2_WEAPON_HIT_H */
