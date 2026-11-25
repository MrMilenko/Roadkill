/*
 * cb_guns.h: machine gun fire, TM2.EXE 0x00443c9c (PS1 UpdateGuns), human (is_human != 0) or AI car.
 *
 * Car layout (human / AI): index i16 +0 / i16 +4, owner = index + 1 / index + 50, type from
 * human_car_get(i) i16 +0x90 / ai_car_get(i) i16 +0x30, weapon block +0x9A / +0x68, stats +0x138 /
 * +0x4EC, angles block +0x108 / +0x98 (i16 +0x28 pitch offset, i16 +0x2C yaw offset), body +0x6C4 /
 * +0x708 (+0x20 i32[3] position, +0x2C / +0x34 i32 angles, +0x50 matrix), gun state +0x848 / +0x834
 * (u8 +7 left/right toggle, u8 +8 double, i16 +0x28 cooldown).
 *   Weapon block u8 +0 and u8 +0x0D clear: gun sound at the position (split screen locate view -1):
 *     human 0: 0x0046f840(9, 0, 0, 99, 0); other human: 0x0046f840(9, volume, pan, 99, index);
 *     AI: 0x0046f878(9, volume, pan (0 in split screen), 99, index) (stack argument last).
 *   Cooldown > 0 or block u8 +0x0D set: cooldown - 1. Otherwise fire: human 0: 0x00473568(7, toggle);
 *     v = (toggle ? -sx : sx, sy, sz) with s = i16 stats +0xD4 / +0xD6 / +0xD8; toggle = 1 - toggle;
 *     muzzle = M^T * v + position (math_mat_vec_mul_row); dir = (sin(a34 - yaw), cos(a34 - yaw),
 *     -sin(a2C - pitch)); bullet 0x00412dc4(owner, double ? 4 : 0, dir, muzzle, double ? 3 : 1);
 *     fxExplosionQuiet8(muzzle, 0); car type 0xE0 with the toggle now set fires a mirrored second
 *     round: v = (0, -sy, sz), muzzle = M^T * v + position, 0x00412dc4(owner, 4, -dir, muzzle, 3),
 *     fx at it; cooldown = 2 (human) / 5 (AI).
 * sin / cos are the verified tm2_sin / tm2_cos (the -0x1000 * k table bug of docs/STATUS.md).
 *
 * Verified against the original under emulation (tools/emu/check_combat_guns.sh).
 */
#ifndef TM2_CB_GUNS_H
#define TM2_CB_GUNS_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    uint32_t (*human_get)(void* user, int32_t i);              /* 0x0045afbc */
    uint32_t (*ai_get)(void* user, int32_t i);                 /* 0x0045b080 */
    int32_t (*split)(void* user);                              /* 0x0046cc54 */
    void (*locate)(void* user, uint32_t pos, int32_t view);    /* 0x0046f974 / 0x0046f980 */
    int32_t (*pan)(void* user);                                /* 0x0046f990 */
    int32_t (*volume)(void* user);                             /* 0x0046f988 */
    void (*play_human)(void* user, int32_t id, int32_t vol, int32_t pan, int32_t prio, int32_t who);  /* 0x0046f840 */
    void (*play_ai)(void* user, int32_t id, int32_t vol, int32_t pan, int32_t prio, int32_t who);     /* 0x0046f878 */
    void (*rumble)(void* user, int32_t a, int32_t b);          /* 0x00473568 */
    void (*bullet)(void* user, int32_t owner, int32_t kind, const int32_t dir[3], const int32_t pos[3], int32_t n); /* 0x00412dc4 */
    void (*muzzle_fx)(void* user, const int32_t pos[3], int32_t flag);   /* 0x00451430 */
    void* user;
} CbGunsEnv;

void cb_update_guns(const CbGunsEnv* e, uint32_t car, int is_human);

#endif /* TM2_CB_GUNS_H */
