/*
 * cb_drops.h: dropped weapons (mines and the like), TM2.EXE 0x00448b1c (PS1 weaponDropCheckHit).
 *
 * 25 records of 0x1C at 0x004ffb5c: +0 i8 kind (0x13 / 0x14 handled), +1 u8 armed, +4 i16 owner car
 * (1 based), +6 i16 shooter id, +8 i16 timer, +0x0A i16 ground height, +0x0C i32[3] position,
 * +0x18 its object (+8 visible, +0xF0 z).
 *   timer <= 0: an object that is visible (+8 == 1) is hidden and dropped, unless kind is 0x13.
 *   timer > 0: when the ground (+0x0A) is below z (+0x14): z = math_angle_step(z, ground, 5)
 *     (0x0048caec) and object +0xF0 = z; timer -= ticks60 (0x004931d4, 16 bit); the owner is
 *     ignored by the hit test except for kind 0x14 with timer < 0x30C and kind 0x13 with timer <
 *     0x7F87 (owner 0); hit = 0x004126b4(pos, owner, 12, kind) with z raised by 30 for the call.
 *     hit not 0 and not 9:
 *       kind 0x14: z += 30 (kept), fx_explosion12(pos, 0) (0x004508f4), weapon_hit 0x00415d1c(hit,
 *         0, 0, 15, 0x004e6a38, owner), object +8 = 0;
 *       kind 0x13: z += 30 (kept), fx_explosion16(pos, 0) (0x00450984); by event type (0x00474508,
 *         asked three times) 2 -> 0x00410890(pos), 1 -> 0x00411334(pos), 3 -> 0x00411e28(pos);
 *         areaDamage(pos, shooter, 1) (0x00449f10); the owner's human car (0x0045afbc(owner - 1)):
 *         when u8 +0x84E is set: it = 0, i16 +0x86A and +0x85C - 1; object +8 = 0; armed = 0;
 *       every kind: timer = 0.
 * ORIGINAL BUG (kept): the 30 added to z before the explosion is never taken off again.
 * ORIGINAL BUG: a falling record without an object writes z to address 0xF0 (the port needs the
 * object mapped).
 *
 * Verified against the original under emulation (tools/emu/check_combat_drops.sh).
 */
#ifndef TM2_CB_DROPS_H
#define TM2_CB_DROPS_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    int32_t (*angle_step)(void* user, int32_t cur, int32_t target, int32_t rate);    /* 0x0048caec */
    int32_t (*ticks60)(void* user);                                                  /* 0x004931d4 (ax) */
    int32_t (*hit_test)(void* user, uint32_t pos, int32_t owner, int32_t r, int32_t kind); /* 0x004126b4 */
    void (*explosion12)(void* user, uint32_t pos, int32_t flag);                     /* 0x004508f4 */
    void (*explosion16)(void* user, uint32_t pos, int32_t flag);                     /* 0x00450984 */
    /* weapon_hit 0x00415d1c(hit, a, b, c, kind, owner): the original passes 0, 0, 15, 0x004e6a38 */
    void (*weapon_hit)(void* user, int32_t hit, int32_t a, int32_t b, int32_t c, uint32_t kind, int32_t owner);
    int32_t (*event_type)(void* user);                                               /* 0x00474508 (al) */
    void (*event2)(void* user, uint32_t pos);                                        /* 0x00410890 */
    void (*event1)(void* user, uint32_t pos);                                        /* 0x00411334 */
    void (*event3)(void* user, uint32_t pos);                                        /* 0x00411e28 */
    void (*area_damage)(void* user, uint32_t pos, int32_t shooter, int32_t big);     /* 0x00449f10 */
    uint32_t (*human_car_get)(void* user, int32_t i);                                /* 0x0045afbc */
    void* user;
} CbDropsEnv;

void cb_weapon_drops_update(const CbDropsEnv* e);

#endif /* TM2_CB_DROPS_H */
