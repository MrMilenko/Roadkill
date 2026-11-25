/*
 * cb2_ground_fires.h: groundFiresUpdate, TM2.EXE 0x0044a59c (PS1 decomp: groundFiresUpdate), the
 * burning patches on the ground (napalm, flame weapons) setting cars, target actors and the flame
 * markers alight.
 *
 * Fire records, 30 x 0x28 bytes at 0x004ff308:
 *   +0x00 u8 active   +0x04 i16 age   +0x08 i32[3] position   +0x14 u32[5] flame objects (0 = none)
 * Distance used everywhere: the original's "box" distance d = max(|dx|, |dy|, |dz|) computed as
 *   m = |dz| > |dy| ? |dz| : |dy|;  d = |dx| < m ? m : |dx|   (abs by cdq / xor / sub: |INT_MIN| stays
 *   negative; signed compares), and then d >> 3 (arithmetic).
 * Object position: +0xE8 / +0xEC / +0xF0 i32 (x, y, z).
 *
 * Per active record: age += frame_get_ticks60 (16 bit).
 *   age >= 300: inactive; each non zero flame object gets +0x0C i16 = 0.
 *   20 < age < 300:
 *     human cars i < human_car_count (read once): object 0x0045ae24(i), car 0x0045afbc(i):
 *       d >> 3 < 25, car +0xB4 == 0, +0xA8 == 0, (+0xCF == 0 or i16 +0x87A < 50), +0xBB == 0,
 *       +0xBD == 0 -> +0xB4 = 1, i16 +0x15A = 0, +0x88C = fx_flame5 0x00450de4(car + 0x6E4, 1).
 *     AI cars i < ai_car_count: object 0x0045ae2c(i), car 0x0045b080(i): d >> 3 < 35, +0x82,
 *       +0x89, +0x8B all 0 -> +0x82 = 1, i16 +0x50E = 0, +0x878 = fx_flame5(car + 0x728, 1).
 *     6 target actors (0x00471aec, 0x4C bytes each) whose i16 +0x04 != -1, object at +0x48:
 *       d >> 3 < 35 -> targetActorHit 0x004723bc(i, copy of the 3 i32 at 0x004e6a5c).
 *     flame markers i = 1..3 (0x00411e14(i): +0x00 i32 lit, +0x08 i32[3] position):
 *       d >> 3 < 35 and lit == 0 -> lit = 1, fx_flame_pair 0x00450e64(marker + 8, 0),
 *       fx_explosion16 0x00450984(marker + 8, 0), clutAnimStartSweep 0x00499984(0x13EC, i,
 *       event type == 3 ? 0 : 2).
 * Then every human car with +0xB4 set: if i32 +0x1DC == 1 and rand % 4 == 0 (signed) and i32
 *   +0x1D8 > 5: +0x1D8 -= 1. i16 +0x15A += ticks; i16 +0x15A >= 300 -> +0xB4 = 0 and, if +0x88C is
 *   not 0, that object's +0x0C i16 = 0.
 * Every AI car with +0x82 set: rand % 4 == 0 and i32 +0x58C > 5 and u8 +0x32 != 0 -> +0x58C -= 1
 *   (rand is called for every burning AI car, unlike human cars). i16 +0x50E += ticks; >= 300 ->
 *   +0x82 = 0, object +0x878's +0x0C i16 = 0 when not 0.
 * Getters (counts, cars, objects, actors, markers) are pure accessors and may be called any number
 * of times. Object handles (+0x14.., +0x88C, +0x878, actor +0x48) go through env->resolve.
 *
 * Verified against the original under emulation (tools/emu/check_cb2_ground_fires.sh).
 */
#ifndef TM2_CB2_GROUND_FIRES_H
#define TM2_CB2_GROUND_FIRES_H

#include <stdint.h>

typedef struct {
    uint8_t* fires;                                          /* 0x004ff308 */
    const int32_t* hit_vec;                                  /* 0x004e6a5c, 3 i32 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    int32_t (*human_count)(void* user);                      /* 0x0045afb4 */
    int32_t (*ai_count)(void* user);                         /* 0x0045b078 */
    uint8_t* (*human_object)(void* user, int32_t i);         /* 0x0045ae24 */
    uint8_t* (*human_car)(void* user, int32_t i);            /* 0x0045afbc */
    uint8_t* (*ai_object)(void* user, int32_t i);            /* 0x0045ae2c */
    uint8_t* (*ai_car)(void* user, int32_t i);               /* 0x0045b080 */
    uint8_t* (*actors)(void* user);                          /* 0x00471aec */
    uint8_t* (*marker)(void* user, int32_t i);               /* 0x00411e14 */
    int16_t (*ticks)(void* user);                            /* 0x004931d4 */
    uint32_t (*rand)(void* user);                            /* 0x004899c2 */
    uint32_t (*flame5)(void* user, uint8_t* pos, int32_t a); /* 0x00450de4, returns a handle */
    void (*actor_hit)(void* user, int32_t i, int32_t* vec);  /* 0x004723bc */
    void (*flame_pair)(void* user, uint8_t* pos, int32_t a); /* 0x00450e64 */
    void (*explosion16)(void* user, uint8_t* pos, int32_t a);/* 0x00450984 */
    uint8_t (*event_type)(void* user);                       /* 0x00474508 */
    void (*clut_sweep)(void* user, int32_t a, int32_t b, int32_t c);  /* 0x00499984 */
    void* user;
} Cb2GroundFiresEnv;

void cb2_ground_fires_update(const Cb2GroundFiresEnv* env);

#endif /* TM2_CB2_GROUND_FIRES_H */
