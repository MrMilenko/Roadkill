/*
 * sh5_target_damage.h: damage to a destructible target, TM2.EXE 0x00470d3c (PS1 decomp:
 * targetDamage). Targets are 50 records of 0x14 bytes at 0x006226c8: +0 i16 type, +2 i16
 * instance, +4 i16 health, +6 u8 animated, +8..+0x10 i32 position.
 *
 * The last record with the type and instance (full 32 bit compares against the sign extended
 * words) and health > 0 takes the damage (16 bit). Still alive: returns 0; none found: 0.
 * Destroyed (health clamped to 0): its animation steps (0x00470320 step 1 when animated, else
 * 0x004707b0 step 0), then by type: 0x1f4 the satellite dish (crowd or event 12 sounds, debris
 * burst for objects with more than 2 parts), 0x1f5 one explosion, 0x1f6 two and 0x1f7 / 0x208
 * three big explosions around it (position raised by 0x50), 0x1f9..0x1fb three small ones
 * (position raised by 0x110; 0x1fa / 0x1fb also start the event effects 0x00456fb0 and come back
 * with health 0x19), 0x1f8 / 0x1fc / 0x1fd / 0x1fe / 0x207 two medium ones plus the scripted
 * events (0x00622ab0, 0x00622adc, the two records 0x00622af4). Returns 1.
 *
 * Verified against the original under emulation (tools/emu/check_sh5_target_damage.sh).
 */
#ifndef TM2_SH5_TARGET_DAMAGE_H
#define TM2_SH5_TARGET_DAMAGE_H

#include "sh3_screen.h"

/* fb = the original's frame base (esp after the prologue, 0x0c bytes: +0 an 8 byte vector
 * copied from TM2.EXE 0x004e7fec, +8 the type argument). Returns the original's al. */
uint8_t sh5_target_damage(const ShEnv* e, uint32_t fb, int32_t type, int32_t inst, uint32_t dmg);

#endif /* TM2_SH5_TARGET_DAMAGE_H */
