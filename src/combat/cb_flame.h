/*
 * cb_flame.h: the flamethrower streams, TM2.EXE 0x00449424 (PS1 weaponFlameUpdate).
 *
 * Two streams (0x20 apart): i16 0x004ff7b8 timer, i16 0x004ff7ba length, 0x004ff7d4 the firing
 * object (+0xB0 matrix, +0xE8 position, +0x158 owner id). While the timer > 0: timer -= ticks60;
 * d = M^T * (0, 32, 0) (math_mat_vec_mul_row); p = position + (2 d.x, 2 d.y, d.z); n = low byte of
 * the length (then length + 1), at most 15; n steps: p += (d.x, d.y, d.z + 1), p.z = 7 when below 8
 * and the object's z (+0xF0) > 0; fxFlameBit(p) (0x00450f44); rand() % 50 == 0: fx_bullet_smoke(p, 0);
 * every step with bit 1 set until the first hit: hit = 0x004126b4(p, owner, 20, -1); hit > 0:
 * weapon_hit 0x00415d1c(hit, 0, 0, 1, d, owner); car_find_by_id 0x0045b178(hit, &human, &i): the
 * car (human: 0x0045afbc, block +0x9A, stats +0x138, body +0x6C4, gun +0x848, flags +0xCC; AI:
 * 0x0045b080, +0x68, +0x4EC, +0x708, +0x834, +0x40) catches fire when its block u8 +0x1A, +0x0E,
 * +0x21, +0x23 are clear and (flags u8 +3 clear or i16 gun +0x32 < 50): +0x1A = 1, i16 stats +0x22
 * = 0, gun +0x44 = fx_flame5(body + 0x20, 1), fx_smoke_a2 and fx_explosion12 there. Then each of the
 * 3 targets i (0x00411e14(i), asked again for every field as the original does: +0 burning, +8
 * i32[3] position) within (largest |d|) >> 3 < 35 and not burning: burning = 1, fxFlamePair(+8, 0),
 * fx_explosion16(+8, 0), 0x00499984(0x13EC, i, event type 3 ? 0 : 2).
 *
 * Verified against the original under emulation (tools/emu/check_combat_flame.sh).
 */
#ifndef TM2_CB_FLAME_H
#define TM2_CB_FLAME_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    int32_t (*ticks60)(void* user);                                     /* 0x004931d4 */
    void (*flame_bit)(void* user, const int32_t p[3]);                  /* 0x00450f44 */
    int32_t (*rand)(void* user);                                        /* 0x004899c2 */
    void (*bullet_smoke)(void* user, const int32_t p[3], int32_t flag); /* 0x00450fd4 */
    int32_t (*hit_test)(void* user, const int32_t p[3], int32_t owner, int32_t r, int32_t kind);  /* 0x004126b4 */
    void (*weapon_hit)(void* user, int32_t hit, int32_t a, int32_t b, int32_t c, const int32_t d[3], int32_t owner); /* 0x00415d1c */
    int32_t (*car_find)(void* user, int32_t id, uint8_t* human, int32_t* index); /* 0x0045b178 (al) */
    uint32_t (*human_get)(void* user, int32_t i);                       /* 0x0045afbc */
    uint32_t (*ai_get)(void* user, int32_t i);                          /* 0x0045b080 */
    uint32_t (*flame5)(void* user, uint32_t pos, int32_t flag);         /* 0x00450de4 */
    void (*smoke_a2)(void* user, uint32_t pos, int32_t flag);           /* 0x00451014 */
    void (*explosion12)(void* user, uint32_t pos, int32_t flag);        /* 0x004508f4 */
    uint32_t (*target)(void* user, int32_t i);                          /* 0x00411e14 */
    void (*flame_pair)(void* user, uint32_t pos, int32_t flag);         /* 0x00450e64 */
    void (*explosion16)(void* user, uint32_t pos, int32_t flag);        /* 0x00450984 */
    int32_t (*event_type)(void* user);                                  /* 0x00474508 (al) */
    void (*notify)(void* user, int32_t id, int32_t i, int32_t v);       /* 0x00499984 */
    void* user;
} CbFlameEnv;

void cb_flame_update(const CbFlameEnv* e);

#endif /* TM2_CB_FLAME_H */
