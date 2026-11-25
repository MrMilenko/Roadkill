/*
 * gp3_roller.h: the rolling ground weapon slots per frame update, TM2.EXE 0x0044ad14 (Ghidra
 * FUN_0044ad14; no PS1 pair found). No arguments. Ten slots of 0x30 bytes at 0x004ff8d0:
 *
 *   +0x00 i8   damage (set every frame: (0x4B0 - timer) * 30 / 0x4B0, at least 15; doubled on
 *              the first bounce)
 *   +0x02 i16  owner car id          +0x04 i16 timer (slot live while > 0)
 *   +0x06 i16  bounce counter        +0x08 i32[3] velocity in the object frame
 *   +0x14 i32[3] velocity in the world (M^T of +0x08)
 *   +0x20 i32[3] position before this frame's move       +0x2C u32 object
 * Object: +0x08 i32 active, +0xB0 i32[9] matrix, +0xE4 i16 heading (+0xE5 its high byte),
 *         +0xE8 i32[3] position.
 *
 * Per live slot (timer > 0), in slot order:
 *   timer -= frame ticks; damage; matrix of angles (0, 0, heading) -> object +0xB0; world
 *   velocity; previous position = position; position += world velocity.
 *   Ground query 0x0049cec0(obj, &res, 1, obj + 0xB0; stack: &previous position; ret 4) fills a
 *   result: +0x04 i32 hit, +0x08 i32[3] normal, +0x14 i32 depth, +0x20 i16 id, +0x22 i16 type.
 *   type 0x29B / 0x29C / 0x191..0x1B8: no hit. Hit, owner id < 0x32 and type 0x1F4..0x208:
 *   0x00470d3c(type, id, damage); timer = 0, obj +0x08 = 0, z += 0x50, 0x004508f4(obj + 0xE8, 0).
 *   Hit (also after that): n = normal / 0x29 each, dot = v . n (world velocity, 32 bit wrap),
 *   r = v - 2 * n * dot / 10000 (each term divided separately), heading = atan2(r.x, r.y or 1);
 *   counter++; counter == 1: damage *= 2 (8 bit), 0x00453b10(obj, 100, 0x004e6a34); depth += 0x14
 *   (in the result); position += -normal * depth / 0xFFF each; the query again; type 0x29B /
 *   0x29C: no hit; still a hit: position -= world velocity, heading high byte += 8.
 *   The reflected vector is not stored (only its heading).
 *   Ground: g = 0x0042c2d4(obj + 0xE8) + 0x10; g <= z: z = math_angle_step(z, g, 13); z = g else.
 *   Human cars (count 0x0045afb4 read at entry; 0x0045afbc(i), model 0x0045ae24(i)): car i32
 *   +0x1D8 > 0 and (owner id != model +0x158 or timer <= 0x3FC) and max(|dx|, |dy|, |dz|) >> 3 <
 *   0x14 (object to car +0x6E4): timer = 0, obj +0x08 = 0, z += 0x50, 0x004508f4(obj + 0xE8, 0),
 *   0x00415d1c(model +0x158, 0, 0, damage; stack &world velocity, owner id),
 *   car_spin_hit 0x00421920(car, 1, 4, 1); a = heading - i16 car +0x6F8 wrapped (0x0048ca2c):
 *   0x200 < a < 0x600: u8 car +0xB8 = 1; 0x0048b5a0(&world velocity, &v) then
 *   0x00421800(car, 1, &v, 800).
 *   AI cars (count 0x0045b078, 0x0045b080(i), model 0x0045ae2c(i)): the same with +0x58C,
 *   timer <= 0x4AB, position +0x728, spin hit (car, 0, 4, 1), i16 +0x73C, |a| (abs) and u8
 *   +0x86, 0x00421800(car, 0, &v, 800).
 *   The 6 records (0x4C bytes) at 0x00471aec(): i16 +0x04 != -1 and the object at +0x48 within
 *   max >> 3 < 10: 0x004723bc(i, &copy of 0x004e6a68).
 *   timer == 100: 0x00453b10(obj, 100, 0x004e6a34). timer <= 0 and obj +0x08 != 0: obj +0x08 =
 *   0, z += 0x50, 0x004508f4(obj + 0xE8, 0).
 * DECOMPILER TRAP: Ghidra reads the i16 fields as (dword at +n) >> 16, which is the i16 at +n+2:
 * its "+0x04 == 1" test is the bounce counter +0x06.
 * PC detail: the human angle test has no abs (the AI one does), as in the machine code.
 *
 * Port: every object is a 32 bit handle (its game address); env->mem resolves one. Calls to code
 * not ported here go through env->call (game address, arguments; `local` is the stack buffer for
 * the one argument that points at the caller's stack: the query result, 0x0048b5a0's output,
 * 0x00421800's vector, 0x004723bc's vector). The constant vector 0x004e6a68 is read from the
 * player's TM2.EXE by the caller; 0x004e6a34 is passed by address.
 *
 * Verified against the original under emulation (tools/emu/check_gp3_roller.sh).
 */
#ifndef TM2_GP3_ROLLER_H
#define TM2_GP3_ROLLER_H

#include <stdint.h>

typedef struct {
    uint8_t* slots;               /* 0x004ff8d0, 10 x 0x30 */
    uint32_t slots_addr;          /* its game address (slot fields are passed by address) */
    const int32_t* rec_vec;       /* 0x004e6a68 i32[3], from TM2.EXE */
    uint32_t sound_addr;          /* 0x004e6a34, passed to 0x00453b10 */
    uint8_t* (*mem)(void* user, uint32_t handle);
    uint32_t (*call)(void* user, uint32_t addr, int n, const uint32_t* args, void* local);
    void* user;
} Gp3RollerEnv;

void gp3_roller_update(const Gp3RollerEnv* env);

#endif /* TM2_GP3_ROLLER_H */
