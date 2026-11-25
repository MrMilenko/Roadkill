/*
 * sh_registry.h: the object registries and the target pool, ported from TM2.EXE
 * 0x00470320..0x00470d08, 0x0047129c, 0x00472e50 (PS1 decomp: object_registry.c,
 * registry_reset.c, target_pool.c).
 *
 * When a level or shell scene is built, its DPC objects are registered by object id so game code
 * can step their animations (animSetStep 0x004a0c94 moves a type 9 "choice" node to a step).
 * Every table holds 32 bit game addresses of nodes / objects; all of it is reached through ShEnv
 * (sh_mem.h) by game address:
 *   0x0062119c u32[16]     choice node per car group (id >> 4), registered from an object +0x15C
 *   0x006211dc u32[16][3]  scene choice nodes by group and slot (node u16 +2 id, u16 +4 slot 1..3)
 *   0x0062129c u32[16]     id % 16 == 6 objects per group; 0x006212dc u32[16]: id % 16 == 7
 *   0x0062131c, 0x00621320 u32 (ids 0x914, 0x915; not cleared by the reset)
 *   0x00621324 u32[7] (ids 0x202, 0x204, 0x205)    0x00621340 u32 (id 0x2b1)
 *   0x00621344 u32 (id 0xc1f)    0x00621348 u32[5] (id 0x2ac)    0x0062135c u32[5] (id 0x2b3)
 *   0x00621370 u32[5] (id 0x2ae)    0x00621384 u32 (id 0x29d)
 *   0x00621388 u32[50]     pickup objects (ids 401..430) by their index (type 1 u16 +0x16, type 4 u16 +0x10)
 *   0x00621450 u32[50]     target objects (ids 0x1f4..0x1fe, 0x207, 0x208)    0x00621518 u32[50]
 *   0x006215e0 u32[5] (id 0xfa0)    0x006215f4 u32[5] (id 0xfa5)    0x00621608 u32[5] (id 0xbbb)
 *   0x0062161c / 0x00621620 / 0x00621624 / 0x00621628 u32 (ids 0xbbc, 0xbbd, 0xc1d, 0xc80)
 *   0x0062162c, 0x00621630, 0x00621638, 0x0062163c u32    0x00621634 u32 (id 0xc)
 *   0x00621640 [16] 0x108 byte car choice records (built by sh_car_choice_build)
 *   0x006226c0 u32         the car choice template (an object: +0x14, +0x18, +0x1C node pointers)
 *   0x006226c8 [50] 0x14 byte targets: +0 i16 type, +2 i16 instance, +4 i16 kind (-1 = free),
 *              +6 u8 special, +8 i32 x, y, z
 *   0x00622ab2 / 0x00622ade / 0x00622ae0 i16: instances of types 0x1fc / 0x207 / 0x1fd
 *   0x00622b4c / 0x00622b50 i32: target index of types 0x1fa / 0x1fb (tracked positions)
 *   0x00622b54 i32 target count?, 0x00622d20 i32 actor count; 0x00622b58 6 actors x 0x4C
 *              (+4 i16 = -1 when free, +0x48 u32)
 *
 * Verified against the original under emulation (tools/emu/check_shell_registry.sh).
 */
#ifndef TM2_SH_REGISTRY_H
#define TM2_SH_REGISTRY_H

#include <stdint.h>
#include "sh_mem.h"

/* 0x00470320: step the registered node(s) of object id (index picks the entry of the per id
 * arrays) with animSetStep; ids below 400 with id % 16 == 6 / 7 also step their group's
 * 0x0062129c / 0x006212dc node. The call is made even when the entry is 0 (animSetStep ignores
 * it). The original returns whatever eax holds (callers ignore it). */
void sh_registry_set_step(const ShEnv* e, int32_t id, int32_t index, uint32_t step);
/* 0x00470650: obj +0x15C -> type 1 node whose +0x18 is a type 9 node: that node is group
 * (id >> 4)'s choice node, its step (+7) reset to 0. */
void sh_registry_choice_register(const ShEnv* e, uint32_t obj, int32_t id);
/* 0x0047068c: group (id >> 4)'s choice node to state (2 -> 1, 3 -> 2) when below its count +6. */
void sh_registry_choice_state(const ShEnv* e, int32_t id, uint32_t state);
/* 0x004706c4: a type 9 node into the scene choice table by its id (+2) group and slot (+4). */
void sh_registry_scene_choice_add(const ShEnv* e, uint32_t node);
/* 0x00470770: pickup objects (ids 401..430) by index. */
void sh_registry_pickup_add(const ShEnv* e, uint32_t obj, int32_t id);
/* 0x004707f8: every registry cleared. */
void sh_registries_reset(const ShEnv* e);
/* 0x00470938: the car choice template. */
void sh_registry_set_car_template(const ShEnv* e, uint32_t obj);
/* 0x00470964: car group (id >> 4)'s choice record 0x00621640 + 0x108 * g from the template:
 * 5 words of the template, then (when the template's +0x14 / +0x1C node, or its +0x18 child,
 * is a type 1 node whose +0x18 is a type 3 node) a copy of that node's 9 words plus its child
 * list at +0x20 (count +0x0B) at record +0x20 / +0x94; record +0x14 / +0x1C point at the
 * copies, +0x18 = template +0x18. obj +0x20 = the record. */
void sh_car_choice_build(const ShEnv* e, int32_t id, uint32_t obj);
/* 0x00470b14: target object index i. */
uint32_t sh_target_object(const ShEnv* e, int32_t i);
/* 0x00470b20: every target free, actor list reset. */
void sh_target_pool_reset(const ShEnv* e);
/* 0x0047129c: the target actor list reset. */
void sh_target_actors_reset(const ShEnv* e);
/* 0x00470bc0: a target into the first free slot (error callback when there is none). */
void sh_target_register(const ShEnv* e, int32_t type, int32_t instance, const int32_t pos[3]);
/* 0x00470d08: the target pool (0x006226c8). */
uint32_t sh_target_pool(void);
/* 0x00472e50: the tracked targets (types 0x1fa / 0x1fb) get a new position. */
void sh_target_track_pos(const ShEnv* e, int32_t type, const int32_t pos[3]);

#endif /* TM2_SH_REGISTRY_H */
