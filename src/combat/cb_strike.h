/*
 * cb_strike.h: the flying strike object (PS1 weaponShadowStrikeUpdate), TM2.EXE 0x0044d1f4.
 *
 * State (game addresses): 0x004ffe70 i32 (u8 +0 flag, i16 +2 owner), i16 0x004ffe76 timer,
 * 0x004ffe74 i16, 0x004ffe78 i32[3] local velocity, 0x004ffe84 i32[3] world velocity, 0x004ffe90
 * i32[3] last position, 0x004ffe9c 4 x i32[3] local corner points, 0x004ffecc a record (i16 +0x0E
 * count), 0x004ffed0 the object (+0xB0 matrix, +0xE0 i16[3] angles, +0xE8 i32[3] position).
 *
 * While the timer > 0: timer -= ticks60; matrix = math_build_matrix(angles); world velocity =
 * M^T * (vx, vy * ticks, vz) (math_mat_vec_mul_row); last position = position; position += ticks *
 * world velocity (ticks60 asked for each axis, i16, 32 bit products); the 4 corners = M^T * corner
 * + position, their ground heights 0x0042c2d4(corner); pitch +0xE0 = -atan2(h0 - h1, 80), roll
 * +0xE2 = atan2(h3 - h2, 80); matrix rebuilt; hit = 0x004126b4(position, owner, 20, 0x915);
 * hit > 500, or hit > 0 without the flag: timer and 0x004ffe74 = 0, 0x00470320(0x915, 0, 0),
 * fx_explosion16(position, 0), weapon_hit 0x00415d1c(hit, 0, 0, 25, 0x004ffe84, owner);
 * timer <= 0 (after that): both = 0, object +8 = 0, 0x00470320(0x915, 0, 0), z += 80,
 * fx_explosion24(position, 0) (0x00450a10), record +0x0E - 1.
 * The math runs through the verified src/math ports (tables loaded by tm2_math_load_tables).
 *
 * Verified against the original under emulation (tools/emu/check_combat_strike.sh).
 */
#ifndef TM2_CB_STRIKE_H
#define TM2_CB_STRIKE_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    int32_t (*ticks60)(void* user);                                            /* 0x004931d4 (ax) */
    int32_t (*ground)(void* user, const int32_t v[3]);                         /* 0x0042c2d4 */
    int32_t (*hit_test)(void* user, uint32_t pos, int32_t owner, int32_t r, int32_t kind);  /* 0x004126b4 */
    void (*set_step)(void* user, int32_t id, int32_t a, int32_t b);            /* 0x00470320 */
    void (*explosion16)(void* user, uint32_t pos, int32_t flag);               /* 0x00450984 */
    void (*explosion24)(void* user, uint32_t pos, int32_t flag);               /* 0x00450a10 */
    void (*weapon_hit)(void* user, int32_t hit, int32_t a, int32_t b, int32_t c, uint32_t vel, int32_t owner); /* 0x00415d1c */
    void* user;
} CbStrikeEnv;

void cb_strike_update(const CbStrikeEnv* e);

#endif /* TM2_CB_STRIKE_H */
