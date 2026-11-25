/*
 * gp2_phit.h: projectile hit test, ported from TM2.EXE 0x004126b4 (PS1 pair: projectileHitTest).
 *
 * gp2_projectile_hit_test(env, pos, self, radius, kind) -> what pos (i32 x, y, z) hits:
 *   the object id (world object u32 +0x158) of a human car (0x0045afb4 count, 0x0045ae24 world
 *   object, 0x0045afbc block), then of an AI car (0x0045b078, 0x0045ae2c, 0x0045b080), then
 *   500 + i for a target pool entry (0x00470d08, only when self < 50), then 6000 + i for a target
 *   actor (0x00471aec), else 0.
 * A car counts when its world object +0x08 is set or its dead flag is clear (human +0x9A, AI
 * +0x68; an AI car without a world object is skipped) and the largest axis distance (32 bit
 * wrap, world object +0xE8 position) >> 3 is under radius + 1 (AI type 0xF0, the boss: + 25).
 * The car whose object id is self is skipped; a human one returns self when kind is 0x33.
 * Pool entries (20 bytes): i16 +4 > 0, i16 +0 != -self, position +8. Actors (0x4C bytes): i16 +4
 * != -1, i8 +2 != 2, object u32 +0x48 with position +0xE8.
 *
 * Effects on the car hit, by kind:
 *   0x0E (freeze): human (not when multiplayer with i16 +0 > 0): u8 +0xA7 and +0xA8 clear ->
 *     +0xA7 = 1, +0x152 i16 300, +0x154 i16 1, 0x00470940(i16 +0x90, 1), +0x87C i16 0; else +0x152
 *     300. AI: +0x75 clear -> +0x75 = 1, +0x506 300, +0x508 1, +0x50A 0, 0x00470940(i16 +0x30,
 *     1); else +0x506 300.
 *   0x0F / 0x2F (fire): human +0xB4 / +0xA8 / +0xBB / +0xBD clear -> +0xB4 = 1, u32 +0x88C =
 *     0x00450de4(car + 0x6E4, 1); then +0x15A i16 0. AI: +0x82 / +0x89 / +0x8B, +0x82, u32 +0x878
 *     = 0x00450de4(car + 0x728, 1), +0x50E i16 0.
 *   0x14: 0x00421920(car, human, 0, 1).
 *
 * Verified against the original under emulation (tools/emu/check_gp2_phit.sh).
 */
#ifndef TM2_GP2_PHIT_H
#define TM2_GP2_PHIT_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;     /* cars, world objects, the pool and actor tables */
    void* user;
    int32_t (*human_count)(void* user);                                  /* 0x0045afb4 */
    int32_t (*ai_count)(void* user);                                     /* 0x0045b078 */
    uint32_t (*human_obj)(void* user, int32_t i);                        /* 0x0045ae24 */
    uint32_t (*human_car)(void* user, int32_t i);                        /* 0x0045afbc */
    uint32_t (*ai_obj)(void* user, int32_t i);                           /* 0x0045ae2c */
    uint32_t (*ai_car)(void* user, int32_t i);                           /* 0x0045b080 */
    uint8_t (*multiplayer)(void* user);                                  /* 0x004632b0 */
    void (*freeze_state)(void* user, int32_t type, int32_t v);           /* 0x00470940 */
    uint32_t (*flame)(void* user, uint32_t pos, int32_t v);              /* 0x00450de4 */
    void (*spin_hit)(void* user, uint32_t car, uint8_t human, int32_t a, int32_t b); /* 0x00421920 */
    uint32_t (*pool_get)(void* user);                                    /* 0x00470d08 */
    uint32_t (*actors_get)(void* user);                                  /* 0x00471aec */
} Gp2PhitEnv;

int32_t gp2_projectile_hit_test(Gp2PhitEnv* env, uint32_t pos, int32_t self, int32_t radius, int32_t kind);

#endif /* TM2_GP2_PHIT_H */
