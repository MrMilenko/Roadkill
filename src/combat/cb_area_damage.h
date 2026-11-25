/*
 * cb_area_damage.h: blast damage around a point, TM2.EXE 0x00449f10 (PS1 areaDamage).
 *
 * cb_area_damage(p, shooter, big): range = big (i16) ? 100 : 75. Distance d = the largest of
 * |dx|, |dy|, |dz| (cdq / xor / sub, so |INT_MIN| stays negative) >> 3 (arithmetic).
 *   Every human car j (0x0045afb4 / 0x0045afbc; position +0x6E4): d < range and u8 +0x13D != 4:
 *     0x00421920(car, 1, 5, 0); damage 0x004210e4(car_object_id(j, 1), (u8 +0xA4 ? 50 : 25) - (d >> 2),
 *     0x004e6a38, 2, shooter) (the fifth argument is on the stack: the decompiled C drops it);
 *     fx_explosion16({x, y, z + 200}, 0) (0x00450984).
 *   Every AI car j (0x0045b078 / 0x0045b080; position +0x728, flag u8 +0x4F1, armour u8 +0x72):
 *     the same with 0x00421920(car, 0, 5, 0) and car_object_id(j, 0).
 *   The 50 destructibles of 0x00470d08() (0x14 bytes: +0 i16 a, +2 i16 b, +4 i16 health, +8 i32[3]
 *     position): health > 0 and d < 50: 0x00470d3c(a, b, 25).
 *   The 6 records of 0x00471aec() (0x4C bytes: +4 i16 (-1 = unused), +0x48 object, its +0xE8
 *     position): d < 50: 0x004723bc(i, v) with v a copy of the 3 i32 at 0x004e6a50.
 * Positions are game addresses read through the WMem.
 *
 * Verified against the original under emulation (tools/emu/check_combat_area.sh).
 */
#ifndef TM2_CB_AREA_DAMAGE_H
#define TM2_CB_AREA_DAMAGE_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    int32_t (*car_count)(void* user);                     /* 0x0045afb4 */
    uint32_t (*car_get)(void* user, int32_t i);           /* 0x0045afbc */
    int32_t (*ai_count)(void* user);                      /* 0x0045b078 */
    uint32_t (*ai_get)(void* user, int32_t i);            /* 0x0045b080 */
    void (*car_hit)(void* user, uint32_t car, int32_t a, int32_t b, int32_t c);  /* 0x00421920 */
    int32_t (*object_id)(void* user, int32_t i, int32_t human);                  /* 0x0045b154 */
    /* 0x004210e4(id, amount, kind, b, shooter): the original passes kind 0x004e6a38, b 2 */
    void (*damage)(void* user, int32_t id, int32_t amount, uint32_t kind, int32_t b, int32_t shooter);
    void (*explosion16)(void* user, const int32_t pos[3], int32_t flag);         /* 0x00450984 */
    uint32_t (*destructibles)(void* user);                                       /* 0x00470d08 */
    void (*destructible_hit)(void* user, int32_t a, int32_t b, int32_t c);       /* 0x00470d3c */
    uint32_t (*props)(void* user);                                               /* 0x00471aec */
    void (*prop_hit)(void* user, int32_t i, int32_t* v);                         /* 0x004723bc */
    void* user;
} CbAreaEnv;

void cb_area_damage(const CbAreaEnv* e, uint32_t p, int16_t shooter, int16_t big);

#endif /* TM2_CB_AREA_DAMAGE_H */
