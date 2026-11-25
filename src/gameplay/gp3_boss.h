/*
 * gp3_boss.h: per frame weapon logic of one car, with the Dark Tooth boss part, TM2.EXE
 * 0x0043e6e0 (Ghidra FUN_0043e6e0; PS1 decomp aiDarkToothBossUpdate,
 * src/semantic/ai_dark_tooth_boss.c). eax = car block, dl = 1 human car / 0 AI car.
 *
 * Human (dl != 0): weapon select latch u8 +0xE6: set when +0xE3, +0xE4 and i8 +0xE5 are all 0;
 *   while set, +0xE3 -> 0x004469bc(ws, 1, 8), else +0xE4 -> (ws, -1, 8), else i8 +0xE5 > 0 ->
 *   (ws, +0xE5, 8), each clearing the latch. ws = +0x848, input +0xCC, flags +0x9A, id i16 +0x00.
 * AI (dl == 0): ws = +0x834, input +0x40, flags +0x68, id i16 +0x04.
 *   If u8 +0x32: level n = 4, 3 (+0x36), 2 (+0x37), 1 (+0x38) (the last set wins);
 *   a = |i16 +0x20|: a < 0x88, i32 +0x28 < 4000 and !+0x71: u8 +0x5C = 1.
 *   a < n * 0x71: rand / 0x148 < u8 +0x83E: +0x839 = 0, (+0x5D, +0x5B) = i8 +0x841 < 8 ? (0, 1)
 *   : (1, 0); when i16 +0x30 == 0x30: +0x841 = 0 if +0x36 else (1 if +0x841 == 0). Otherwise
 *   +0x5B = 0 and i16 +0x85A = i16 0x004ffed8 when i16 +0x85A < 1.
 *   u8 +0xC8 == 1 and a > ((0xB4 - 10 n) << 12) / 0x168: the same draw with +0x839 = 1; the
 *   miss clears +0x5D and +0x5B.
 *   Boss part: game event type 0x00474508() == 8 and i16 +0x30 == 0xF0 (Dark Tooth):
 *     +0x5C = 0; +0x75 (dead) -> i16 0x004fef2c = 100.
 *     Fire: c1 = i16 0x004e69d4 (post increment); when the old value > i16 0x004fef28 and
 *     alive: c1 = 0, 0x004fef28 = rand / 500 + 10, side toggle u8 0x004fef2a ^= 1; the launch
 *     point M^T (+0x758) (x, y, z) + position +0x728 with (x, y, z) = (word 0x004fef2a ? -0x48 :
 *     0x48, -0xE0, 0x38) and target 0x00449048(i16 +0x04, &t, 0) when u8 +0xC8 == 1, else
 *     (word ? -0x78 : 0x78, 0x110, 0x68) and target 0x00448de4(i16 +0x04, &t, 0, 0); then
 *     0x004142ac(model 0x0045ae2c(i16 +0x04) +0x158, t, &{0xAA, 0, (i16 +0x20 + i32 +0x73C) &
 *     0xFFF}, &point; 5).
 *     Bomb (object 0x004ffed4): its position saved to 0x004fef38; z offset 0x004fef30 = -0x18
 *     when 0x0045a8bc(z) else 0xF0. c2 (before its increment) == i16 0x004fef2c - 0x1E: sound 0x3D
 *     at the bomb.
 *     c2 = i16 0x004e69d6 (post increment); old > i16 0x004fef2c and alive: target car
 *     i16 0x004fef50 = 0x0045acb8(+0x728, -1), 0x004fef34 = its position (0x0045afbc + 0x6E4);
 *     within max(|d|) >> 3 < 500: c2 = 0, 0x004fef2c = rand / 0x147 + 100, mode i16 0x004fef2e
 *     = 1, sound 0x42 (third of the volume).
 *     Mode 0: the bomb eases (step 20) to M (+0x758) (0, 0, z offset) + position; alive: roll
 *     i16 +0xE2 += i16 0x004e69da (bounces between +-0x2AA as +-100), heading += i16 0x004e69d8,
 *     matrix. Mode 1: eases (step 13) to the target, bobs z += sin(c2 * 200) * 0xA0 / 0xFFF,
 *     heading eased (rate 3) to atan2 toward the target, matrix; within < 0x14: 0x00450984,
 *     push vector (bomb - saved) to 0x004fef44 (+0x4fef4c = 0), 0x004210e4(model(target) +0x158,
 *     7, 0x004fef44, 0; i16 +0x06), and when the target car's +0xB4 / +0xA8 / +0xBB / +0xBD
 *     are all clear: +0xB4 = 1, i16 +0x15A = 0, +0x88C = 0x00450de4(target, 1), 0x00451014,
 *     0x004508f4, sound 6; mode 0. Else 0x0048df8c(bomb +0x158, -1, bomb + 0xE8, &l) != 0 ->
 *     0x00450984, mode 0. Mode 0 also when the car is >= 500 (>> 3) from the target or c2 > 0x28.
 * Both: weapon state ws: when i32 ws+0 != 0 or AI: i16 ws+0x0E..+0x20 = 1, ws+0x32 = 0x3DE,
 *   ws+0x30 = 3000, ws+0x1E = 0. i16 ws+0x2E += ticks; >= i16 ws+0x2C: ws+0x0E++, ws+0x2E = 0,
 *   ws+0x0E = min(ws+0x0E, i8 ws+0x0B), i8 ws+0x0D >= 8 and ws+0x0E >= 1: ws+0x0D = 0.
 *   ws+0x30 += ticks, at most 3000. weaponFireSpecial 0x004401fc(car, human), special trigger
 *   0x0044342c(car, human). Input +0x1C set: cleared, 0x00443c9c(car, human), flags +0 or +0xD:
 *   human 0x0046f860(9, id) / AI 0x0046f898(9, id). Not set: human id 0 -> 0x0046f860(9, 0),
 *   else 0x0046feb0() == 0 -> 0x0046f860(9, id); AI 0x0046f898(9, id).
 *
 * The sound, bob and give-up tests read c2 (0x004e69d6), not c1: (dword 0x004e69d4) >> 16.
 * DECOMPILER TRAPS: Ghidra's launch side test is "== 1 -> +x"; the machine tests the word at
 * 0x004fef2a != 0 -> -x. Its (dword at a) >> 16 reads are the i16 at a + 2 throughout.
 *
 * Port: objects are 32 bit handles (game addresses), env->mem resolves one; calls go through
 * env->call (game address, arguments; `local` / `local2` for the arguments that point at the
 * caller's stack, in argument order). Globals by pointer.
 *
 * Verified against the original under emulation (tools/emu/check_gp3_boss.sh).
 */
#ifndef TM2_GP3_BOSS_H
#define TM2_GP3_BOSS_H

#include <stdint.h>

typedef struct {
    uint8_t* g_count;     /* 0x004e69d4: i16 c1, i16 c2, i16 yaw rate, i16 roll rate */
    uint8_t* g_bomb;      /* 0x004fef28 .. 0x004fef53 */
    uint32_t g_bomb_addr; /* 0x004fef28 (0x004fef44 is passed by address) */
    uint8_t* g_obj;       /* 0x004ffed4: u32 bomb object, i32 AI special cooldown */
    uint8_t* (*mem)(void* user, uint32_t handle);
    uint32_t (*call)(void* user, uint32_t addr, int n, const uint32_t* args, void* local, void* local2);
    void* user;
} Gp3BossEnv;

void gp3_boss_update(const Gp3BossEnv* env, uint32_t car, uint8_t human);

#endif /* TM2_GP3_BOSS_H */
