/*
 * gp_proj.h: missile fire variants on the projectile slot array, ported from TM2.EXE. They reuse
 * the verified slot code in src/weapons/projectile.c (slot layout there).
 *
 *   0x00413f4c gp_proj_fire_type0a   homing fire (0x00414458), then type 0x0A, mode 10
 *   0x00413f7c gp_proj_fire_type33   homing fire, model 6 if loaded, type 0x33, mode 5, speed 0x4B,
 *                                    life x4, +0x44 trail segments 3, +0x3C target object (2nd stack arg)
 *   0x00414244 gp_proj_fire_type3d   homing fire, type 0x3D, mode 0x0F, speed 0x25, life x4, model 8
 *   0x00414178 gp_proj_spawn_type0c  like the plain spawn 0x00414000 (model 0, type 0x0A, life 0x32,
 *                                    speed 0x96, no target, trail) but ends as type 0x0C
 *   0x00414530 gp_proj_fire_type0f   type 0x0F, life 0x96, speed 0x1E, target position, mode 0x0F
 *                                    for owners < 0x32 else 7, drop +0x46 = 100, model 2; visible when
 *                                    the model is set
 *   0x00414618 gp_proj_fire_type0f_b as 0x00414530 with speed 0x4B and mode 0x32 for every owner
 *
 * All return the slot index (sign extended 16 bit) or -1 when the 16 slots are busy. The life
 * multiply is 16 bit (shl of the low word).
 * DECOMPILER TRAP: 0x00413f4c returns the index in EAX (Ghidra: void); 0x00414178 also returns it
 *   (EAX = cwde of the index) and stores type 0x0A before the object init, then 0x0C at the end
 *   (Ghidra shows only the 0x0A store in the middle of the setup).
 * Arguments as in the original: owner EAX, target position EDX, angles EBX, position ECX (game
 * addresses), damage on the stack; 0x00414178 has no target (owner, angles, position, damage in
 * registers, ret 0).
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_proj.sh).
 */
#ifndef TM2_GP_PROJ_H
#define TM2_GP_PROJ_H

#include <stdint.h>

#include "weapons/projectile.h"

int32_t gp_proj_fire_type0a(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage);
int32_t gp_proj_fire_type33(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage,
                            uint32_t target_obj);
int32_t gp_proj_fire_type3d(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage);
int32_t gp_proj_spawn_type0c(ProjEnv* env, int32_t owner, uint32_t angles, uint32_t pos, int32_t damage);
int32_t gp_proj_fire_type0f(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage);
int32_t gp_proj_fire_type0f_b(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage);

#endif /* TM2_GP_PROJ_H */
