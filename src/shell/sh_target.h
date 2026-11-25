/*
 * sh_target.h: target actors and the level / player event records, ported from TM2.EXE
 * 0x004712c8, 0x0047274c, 0x00472a14, 0x00472b70 (PS1 decomp: target_actor_lifecycle.c,
 * target_secondary_events.c). Game memory through ShEnv (sh_mem.h) by game address.
 *
 * Target actors 0x00622b58: 6 x 0x4c bytes, count 0x00622b54:
 *   +2 u8 (0), +4 i16 object id, +6 i16 instance, +0xa i16 (0), +0x48 u32 entity
 * The entity (allocated by 0x00492d90): +8 u32 (1), +0xc u32 (0), +0x158 i32 object id 6000 + slot.
 *
 * Level event 0x00622adc: +0 u8, +1 u8, +6 i16, +8 i16 (0x14), +0xa i16 (0x50), +0xc i32 (0x3ff8),
 * +0x10 i32 (0x4d50), +0x14 i32 (0x15e0).
 * Player events 0x00622af4 + 0x2c * (player - 1):
 *   +0 u8, +1 u8 (0), +2 i16 = object +4, +4 i16 (0), +6 i16 (0x14), +8 i16 (0x32), +0xc u32 object,
 *   +0x10 / +0x14 / +0x18 i32 position (player 2: 0xf40, 0xee8, 0x2a8; else 0xf70, -0xff8, 0x2a8),
 *   +0x1c / +0x1e i16 random drift (+-rand() / 8), +0x20 i16 0xfff, +0x24 / +0x26 / +0x28 i16 0.
 *
 * Verified against the original under emulation (tools/emu/check_shell_target.sh).
 */
#ifndef TM2_SH_TARGET_H
#define TM2_SH_TARGET_H

#include <stdint.h>
#include "sh_mem.h"

/* 0x004712c8: a new target actor for a type 9 node while fewer than 6: entity allocated
 * (entity_alloc(count); nothing more when 0), initialised from the node, inserted, placed
 * (actor_place), then object id's registry entry index stepped to 1 (sh_registry_set_step),
 * count + 1. */
void sh_target_actor_spawn(const ShEnv* e, int32_t id, int16_t instance, uint32_t node);
/* 0x004723bc: target actor i hit along vector v (v[1] = 0 is set to 1 in place): once only
 * (+2 becomes 1): unless object 0x205, a hit sound (0x0046f974 at the entity position, or
 * 0x0046f980(pos, -1) when 0x0046cc54() != 0) and a flash (0x0046f8b0(0x19, 0x0046f988() * 2,
 * 0x0046f990(), 0x32)); +8 = 0, +0xa = 0x19, +0x18 matrix of (0, 0, atan2(v)), registry step 2,
 * an effect at the entity position raised by 0x28 (0x00457398). ORIGINAL BUG not reproduced:
 * with |v[0]| or |v[1]| >= 2^22 the original's atan2 (0x0048b71c) overflows x << 9 and reads
 * outside its table; tm2_atan2 differs there. */
void sh_target_actor_hit(const ShEnv* e, int32_t i, uint32_t v);
/* 0x0047274c: the level event record defaults. */
void sh_target_level_event_init(const ShEnv* e);
/* 0x00472a14: player (1 based) event record for obj, drift from tm2_rand (4 calls). Returns
 * (player - 1) * 11 (what the original leaves in eax). */
int32_t sh_target_player_event_init(const ShEnv* e, uint32_t obj, int32_t player);
/* 0x004724c4: the timed event record 0x00622ab0 (same layout as a player event record, see
 * above) for obj: position (0xffffb848, 0x32f0, 0x730), drift from tm2_rand (4 calls). */
void sh_target_timed_event_init(const ShEnv* e, uint32_t obj);
/* 0x00472b70: empty (a bare ret; the per frame update hook of an event kind with nothing to do). */
void sh_target_event_nop(void);

#endif /* TM2_SH_TARGET_H */
