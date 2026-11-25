/*
 * gp3_tornado.h: the tornado special weapon per frame update, TM2.EXE 0x0044bd3c (Ghidra
 * FUN_0044bd3c; PS1 decomp weaponTornadoUpdate, src/semantic/weapon_tornado.c). No arguments.
 *
 * Globals (game addresses):
 *   0x004ffe1c u32  tornado object      (+0x08 i32 active, +0xb0 i32[9] matrix, +0xe0 i16[3]
 *                                         angles, +0xe8 i32[3] position)
 *   0x004ffe20 i16  timer (ticks left; nothing runs while <= 0)
 *   0x004ffe24 i16  spin rate (eased to 0x2AA, rate 5; subtracted from angle +0xe4)
 *   0x004ffe28 u32  pointer to an i32 (the owner's health: <= 0 ends the tornado)
 *   0x004ffe2c u32  pointer to a block whose +0x0d u8 set ends the tornado
 *   0x004ffe30 u32  owner object (the firing car's model: +0x08 i32, +0xe8 i32[3] position,
 *                                  +0x158 i32 id)
 *   0x004fef7c u32  last effect handle;  0x004fef80 i32[3] offset of the caught car from the
 *   tornado;  0x004fef8c i32[9] the rotation matrix of the fixed angles at 0x004e6a28 (i16[3]).
 *   0x004e6a74 i32[3] constant vector handed to 0x004723bc (a copy on the stack).
 *
 * Per frame, while the timer is > 0:
 *   timer -= frame ticks (0 if negative); the object takes the owner's position, z eased up by
 *   math_angle_step(z, z + 0x78, 5); angle x eased to 0x31C (rate 3); spin eased to 0x2AA (rate 5),
 *   angle z -= spin; matrix rebuilt. 1 in 10 (rand % 10 == 0): effect 0x0044a40c(owner + 0xe8,
 *   0x333 x3) -> 0x004fef7c, then 0x00451274(fx, 0), 0x00450718(fx, 0).
 *   Every human car (0x0045afbc(i), count 0x0045afb4; model 0x0045ae24(i)), then every AI car
 *   (0x0045b080(i), count 0x0045b078; model 0x0045ae2c(i)): d = max(|dx|, |dy|, |dz|) between the
 *   owner and the car (signed compares: |INT_MIN| stays negative); when d >> 3 < 0x23 and the car
 *   is not the owner:
 *     human only: u8 +0xb0 = 1 (caught);
 *     offset = car - owner -> 0x004fef80; first catch (u8 +0xa7 human / +0x75 AI == 0): flag = 1,
 *     i16 +0x152 / +0x506 = 0x3C, car_spin_hit 0x00421920(car, human, 3, 1),
 *     0x00415d1c(model +0x158, 0, 0, 4; stack 0x004fef80, owner +0x158);
 *     0x00430b5c(owner + 0xe8); effect 0x0044a40c(owner + 0xe8, 0x333 x3) -> 0x004fef7c; rand odd:
 *     0x00451164(fx, 0); matrix of 0x004e6a28 -> 0x004fef8c; car position = owner position +
 *     M^T offset; human: velocity +0x6f0 / +0x6f4 / +0x6f8 each -= 0x71; AI: +0x734 / +0x738 -=
 *     0x71, +0x73c -= 0x155; when i16 +0x152 / +0x506 < 1: offset y = 1 if 0 (stored),
 *     0x00432340(i16 +0x90 / +0x30, (i16)atan2(offset x, offset y), 0).
 *     Human cars out of range (or the owner itself): u8 +0xb0 = 0. AI cars: nothing.
 *   When the owner's i32 +0x158 < 0x32: the 50 records (20 bytes) at 0x00470d08(): i16 +0x04 > 0
 *     and within range of +0x08 / +0x0c / +0x10: 0x00470d3c(i16 +0x00, i16 +0x02, 4).
 *   The 6 records (0x4c bytes) at 0x00471aec(): i16 +0x04 != -1 and the object at +0x48 within
 *     range (its +0xe8 position): 0x004723bc(i, &copy of 0x004e6a74).
 *   End: timer <= 0, *health <= 0 or block +0x0d set: object +0x08 = 0, timer = 0, owner +0x08 =
 *     1 when *health > 0; 0x00453c38(0xa0) when *health <= 0 else 0x00453c00(0xa0);
 *     0x004511a8(owner + 0xe8, 0); every human car: u8 +0xb0 = 0.
 * The car / AI counts are read at entry, before the timer test (the calls happen every frame).
 *
 * DECOMPILER TRAP: Ghidra writes the i16 tests as (*(int *)(p + 2) >> 16) and the globals as
 * 0x004ffe20 / 0x004ffe22 halves: the dword at +2 shifted down is the i16 at +4.
 *
 * Port: every object is a 32 bit handle (its game address); env->mem resolves one to memory.
 * Every call to code not ported here goes through env->call with the game address of the
 * callee, its register / stack arguments in order and, for 0x004723bc, the vector. The two
 * constant tables (0x004e6a28, 0x004e6a74) are read from the player's TM2.EXE by the caller.
 *
 * Verified against the original under emulation (tools/emu/check_gp3_tornado.sh).
 */
#ifndef TM2_GP3_TORNADO_H
#define TM2_GP3_TORNADO_H

#include <stdint.h>

typedef struct {
    uint8_t* g_obj;          /* 0x004ffe1c .. 0x004ffe33 (6 x u32: obj, timer / -, spin, health ptr, block ptr, owner) */
    uint8_t* g_fx;           /* 0x004fef7c .. 0x004fefaf (fx handle, offset i32[3], matrix i32[9]) */
    uint32_t g_fx_addr;      /* the game address of g_fx (0x004fef7c): 0x004fef80 is passed by address */
    const int16_t* push_angles;   /* 0x004e6a28 i16[3], from TM2.EXE */
    const int32_t* fx_vec;        /* 0x004e6a74 i32[3], from TM2.EXE */
    uint8_t* (*mem)(void* user, uint32_t handle);
    uint32_t (*call)(void* user, uint32_t addr, int n, const uint32_t* args, const int32_t* vec);
    void* user;
} Gp3TornadoEnv;

void gp3_tornado_update(const Gp3TornadoEnv* env);

#endif /* TM2_GP3_TORNADO_H */
