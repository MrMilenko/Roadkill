/*
 * gp_steer.h: homing missile steering, ported from TM2.EXE 0x004147c8 (PS1 pair:
 * weaponProjectileSteer). The slot layout is in src/weapons/projectile.h; this is its env->steer.
 *
 * gp_proj_steer(slot): turn rate s = (i16 +0x08 * 20) / fps (16 bit). Toward the target position
 * (+0x38, game address): yaw = atan2(dx, dy) mod 0x1000, pitch from atan2(dz, |max| + |min| / 2)
 * folded into -0x400..0x400. Yaw (object +0xE4) and pitch (+0xE0) turn by s toward them (wrapped to
 * the nearest turn) or snap when closer than s, then wrap into 0..0xFFF.
 *   type 0x29: loses its target when more than 0x155 off; wobbles (yaw +- 2s, pitch +- s by rand());
 *              past 2400 from its start (+0x28, max axis distance >> 3 > 300) its damage +0x0A drops by
 *              1 a frame (at least 2) and its model flips between models[1] and [2] by a rand() bit
 *   type 0x31: wobbles the same way (no fade)
 *   type 0x33: past 2400 it locks on the target object (+0x3C) position (on a retarget: mode 30,
 *              speed 50, damage x4); within 160 of the object (max axis >> 3 < 20) the slot named by
 *              its own index byte ends (life -1, object hidden); yaw eases (math_angle_step 9) and roll
 *              (+0xE2) leans into the turn (+-0x400 at rate 13, back to 0 at rate 5)
 * then the object's matrix is rebuilt from its angles.
 * ORIGINAL BUG: the end of the slot goes through the own index byte (+0x00), as in proj_tick.
 * The original divides by s * 4 / s * 2 for the wobble and faults when s is 0 (never for these types).
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_steer.sh).
 */
#ifndef TM2_GP_STEER_H
#define TM2_GP_STEER_H

#include "weapons/projectile.h"

void gp_proj_steer(ProjEnv* env, uint8_t* slot);

#endif /* TM2_GP_STEER_H */
