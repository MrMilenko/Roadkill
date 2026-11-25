/*
 * cb_leap.h: the homing leap special, TM2.EXE 0x0044cee0 (PS1 weaponHomingLeapUpdate).
 *
 * State (game addresses): u8 0x004ffe34 active, 0x004ffe38 -> i16 (the car's state, < 2 = dead),
 * i16 0x004ffe40 shooter, i16 0x004ffe42 time, i16 0x004ffe44 target id, i32 0x004ffe48 the leap
 * speed target, 0x004ffe4c -> i32 and i32 0x004ffe50 (a frame stamp: more than 5 apart ends it),
 * i32[3] 0x004ffe54 local velocity (+4 eased toward 0x004ffe48 by 5), 0x004ffe60 -> the leaping
 * position, 0x004ffe64 -> an object (+8 heading), 0x004ffe68 -> the target position.
 *   Active and (time > 300 or state < 2 or the stamps more than 5 apart): inactive,
 *     0x00470320(0x66, 0, 0); state < 2: fx_explosion12 + fx_smoke_small at the position.
 *   Active: d = target - position; r = isqrt(dx * dx + dy * dy) (the original's bit by bit integer
 *     square root over 32 bit wrapped sums, signed compares); dy 0 and r 0 count as 1; yaw =
 *     atan2(dx, dy), pitch = atan2(dz, r); object +8 = math_angle_interp(+8, yaw, 11); time +=
 *     ticks60; time > 6: +0x58 = math_angle_step(+0x58, 0x004ffe48, 5), position += M^T * velocity
 *     with M = math_build_matrix({-pitch, 0, yaw}), 0x00470320(0x66, 0, 1). Then when the largest
 *     of |d| >> 3 < 15: inactive, fx_explosion12 + fx_smoke_small at the target, damage 0x004210e4(
 *     target id, 20, v, 2, shooter) (fifth argument on the stack; the decompiled C drops it),
 *     0x00470320(0x66, 0, 0).
 * ORIGINAL BUG: v is the last moved step (M^T * velocity) only when it moved this frame; otherwise
 * the original passes a stack vector it never wrote. The port passes the step or zeros.
 *
 * Verified against the original under emulation (tools/emu/check_combat_leap.sh).
 */
#ifndef TM2_CB_LEAP_H
#define TM2_CB_LEAP_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    int32_t (*ticks60)(void* user);                                       /* 0x004931d4 (ax) */
    void (*set_step)(void* user, int32_t id, int32_t a, int32_t b);       /* 0x00470320 */
    void (*explosion12)(void* user, uint32_t pos, int32_t flag);          /* 0x004508f4 */
    void (*smoke_small)(void* user, uint32_t pos, int32_t flag);          /* 0x00451058 */
    void (*damage)(void* user, int32_t id, int32_t amount, const int32_t v[3], int32_t b, int32_t shooter); /* 0x004210e4 */
    void* user;
} CbLeapEnv;

/* the original's integer square root (exposed for tests) */
uint32_t cb_isqrt(uint32_t s);

void cb_homing_leap_update(const CbLeapEnv* e);

#endif /* TM2_CB_LEAP_H */
