/*
 * cb_level.h: small level entity, pickup and weapon object helpers of TM2.EXE (0x0043a6d8,
 * 0x004469bc, 0x00447d30..0x00457600). Mostly registration tables of world objects and resets.
 *
 * Memory: these functions work on the original's globals and on objects they point to, all kept at
 * their game addresses and reached through a WMem (src/weapons/wmem.h): every address named below
 * must be mapped. Object pointers stored in the tables stay game addresses.
 *
 * Globals (game addresses):
 *   0x004fefbc  50 level entities of 0x10: +0 i16 kind, +2 i16 used, +4 i32[3] position
 *               (count i32 0x004ff2dc right after them, then i32[10] 0x004ff2e0, +0x0C is 1)
 *   0x004ff8fc  10 x 0x30 records (+0 i32)          0x004ffacc  4 x 0xC (+0, +4 i32)
 *   0x004ffaf8  23 object pointers                  0x004ffb54  2 object pointers
 *   0x004ffb5c  25 pickup records of 0x1C: +0 u8, +1 u8, +8 i16, +0x18 object pointer
 *   0x004ffe18, 0x004ffe1c, 0x004ffe30, 0x004ffed0 i32; 0x004ffed4 object pointer (its +0x15C read)
 *   0x004ffedc  i32 table indexed 0..0x26 (pickup counters)
 *   0x00500014  i32
 *   0x00502818  u8[7] render mode bytes
 *   0x00502834..0x00502880  registered kind 5 objects (5 tables, see cb_obj5_register)
 *   0x005028c0  10 pairs {i32 value, i32 key} (light flicker table)
 *   0x00502910  15 car body objects, 0x0050294c 15 kind 1 objects (indexed (id >> 4) - 1)
 *   0x00502e90  slam arm object, 0x00502e94 slam claw object, 0x00502e98 / 0x00502e9c their angles
 *   0x00502f18  platform table pointer (0x38 per slot, +0x34 entity, entity +0xE2 i32)
 *   0x00502f1c i32, 0x00502f20 / 0x00502f34 / 0x00502f48 5 kind 4 objects each
 *   0x00502f5c i32, 0x00502f60 target object, 0x00502f64 5 objects (free when +8 == 0), 0x00502f80 u8[]
 *   0x004e7414, 0x004e7418, 0x004e741c i32 flags
 *
 * Verified against the original under emulation (tools/emu/check_combat_level.sh).
 */
#ifndef TM2_CB_LEVEL_H
#define TM2_CB_LEVEL_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    /* pickup_set_visible 0x004707b0(id, slot, visible) */
    void (*pickup_set_visible)(void* user, int32_t id, int32_t slot, int32_t visible);
    void (*model_remove)(void* user, uint32_t obj);    /* world_model_remove 0x004a14c0 */
    void (*model_add)(void* user, uint32_t obj);       /* world_model_add 0x004a14a0 */
    /* 0x0048c50c(angle, out): X rotation matrix of angle into out (game address) */
    void (*rot_x)(void* user, int32_t angle, uint32_t out);
    void* user;
    /* math_build_matrix 0x0048c360(angles, out): out is a game address */
    void (*build_matrix)(void* user, const int16_t angles[3], uint32_t out);
} CbLevelEnv;

/* 0x0043a6d8: preset handling values of a car stats block (applied to 0x00507af8 when the boss
 * dies): i16 +0x44 30, +0x46 60, +0x48 50, +0x4C 115, +0x4E 40, +0x50 -40, +0xD4 -20, +0xD6 20,
 * +0xD8 0, +0xDC 0, +0xDE 0, +0xE0 20, +0x3A 110, +0x7A 50, +0x42 50; i32 +0xA0 = i32 +0x38 >> 16. */
void cb_stats_preset(uint8_t* stats);

/* 0x004469bc: weapon selection of a weapon block (+0x0D i8 current, i16 ammo at +0x0E + 2 * k).
 * A current outside 0..7 becomes 0. dir 1: previous with ammo (wraps to n - 1), -1: next with ammo
 * (wraps to 0 at n), stops back at the start; any other dir selects slot dir - 100 if it has ammo.
 * Finally a current slot without ammo becomes n.
 * ORIGINAL BUG: with dir +-1, a start slot >= n and no ammo in 0..n-1, it never returns. */
void cb_weapon_select(uint8_t* block, int32_t dir, int32_t n);

/* 0x00447d30 (old name level_init_pickups): clears the weapon object tables (see header). */
void cb_weapon_objects_reset(const CbLevelEnv* e);
/* 0x00448098: pickup records: +8 i16 = 0, +0 = +1 = 0, an attached object gets +8 = 0 and is
 * dropped; the objects of 0x004ffaf8 / 0x004ffb54 get +8 = 0 (pointers kept). */
void cb_weapon_objects_detach(const CbLevelEnv* e);

/* 0x00449844 (level_entities_init): 50 entities unused, count 0, 0x004ff2e0[10] = 2, [3] = 1. */
void cb_level_entities_init(const CbLevelEnv* e);
/* 0x00449888: entity i (signed, <= 50) takes kind and the position at pos when unused, count + 1.
 * ORIGINAL BUG: i = 50 is accepted: its record overlaps the count (0x004ff2dc) and the table after
 * it; negative i writes before the table. */
void cb_level_entity_add(const CbLevelEnv* e, uint32_t pos, int16_t kind, int32_t i);
/* 0x00449eb4: 15 */
int32_t cb_level_entity_kinds(void);
/* 0x00449ebc / 0x00449ef4: pickup_set_visible(id, slot, 1 / 0), entity slot +2 = 1 / 0 */
void cb_level_entity_show(const CbLevelEnv* e, int32_t id, int32_t slot);
void cb_level_entity_hide(const CbLevelEnv* e, int32_t id, int32_t slot);

/* 0x0044df04: *(0x004ffed4) +0x15C;  0x0044df10: 0x004ffed4 */
uint32_t cb_target_object_field(const CbLevelEnv* e);
uint32_t cb_target_object(const CbLevelEnv* e);
/* 0x0044eee4: pickup counter i (unsigned): 0x12, 0x13, 0x16..0x1E are taken (returned and
 * cleared); 0x22 and above 0x26 give 0; the others are read. */
int32_t cb_pickup_counter_take(const CbLevelEnv* e, uint32_t i);
/* 0x0044ef60: combo block init: +0 = id, +4..+0x10 and +0x14..+0x20 i32 = 0, +0x28..+0x2B u8 = 0,
 * +0x2C = 0, +0x54 u8 = 0, +0x30 = -1. */
void cb_combo_block_init(uint8_t* block, uint32_t id);

/* 0x0045029c: 0x00500014 = 0 */
void cb_fx_counter_clear(const CbLevelEnv* e);
/* 0x00452d28, 0x00453b68 (the ret of 0x00453b10), 0x004570a0: empty */
void cb_nothing(void);
/* 0x00452d48 (game_set_render_mode): mode 0..6 -> 0x00502818[mode] = ED F6 F0 F5 F5 F9 EE; other
 * modes (unsigned) nothing. Returns mode (eax is not changed). */
uint32_t cb_render_mode_set(const CbLevelEnv* e, uint32_t mode);
/* 0x00452d90 (game_clear_state): clears the world object tables, angles 0x00502e98 / 0x00502e9c = 11,
 * flags 0x004e7414 / 18 / 1C = 1. */
void cb_world_objects_clear(const CbLevelEnv* e);

/* kind 5 objects by their +0x34 u16 index (1..max): table base, max
 *   0x00453400 0x00502830 4   0x00453428 0x00502858 6   0x00453450 0x00502848 4
 *   0x00453478 0x00502870 4   0x004534a0 0x00502840 2 */
enum { CB_OBJ5_A, CB_OBJ5_B, CB_OBJ5_C, CB_OBJ5_D, CB_OBJ5_E, CB_OBJ5_COUNT };
void cb_obj5_register(const CbLevelEnv* e, int which, uint32_t obj);
/* 0x00453b10: light flicker table: the pair whose key is key gets value; else the first pair with
 * value 0 gets (value, key); else nothing. */
void cb_light_flicker_set(const CbLevelEnv* e, int32_t key, uint32_t value);
/* 0x00453c00 / 0x00453c38: car body object of car id ((id >> 4) - 1) shown (model_remove then
 * model_add) / hidden (model_remove) when it is a kind 5 object. */
void cb_car_body_show(const CbLevelEnv* e, int32_t id);
void cb_car_body_hide(const CbLevelEnv* e, int32_t id);
/* 0x00453e60: a kind 5 object becomes the slam arm: 0x00502e90 = obj, 0x00502e98 = -0x71,
 * rot_x(-0x71, obj + 4). */
void cb_slam_arm_register(const CbLevelEnv* e, uint32_t obj);
/* 0x00453f50: a kind 1 object for car id: 0x0050294c[(id >> 4) - 1] (signed index < 15). */
void cb_car_object_register(const CbLevelEnv* e, int32_t id, uint32_t obj);
/* 0x00453f70: 0x0050294c[(id >> 4) - 1] */
uint32_t cb_car_object_get(const CbLevelEnv* e, int32_t id);
/* 0x00455fe8: platform i entity +0xE2 i32 >> 16 */
int32_t cb_platform_height(const CbLevelEnv* e, int32_t i);
/* 0x004570a4: 0x00502f5c = v */
void cb_world_set_502f5c(const CbLevelEnv* e, uint32_t v);
/* kind 4 objects by +0x10 u16 index (< 5): 0x00457118 0x00502f20, 0x0045721c 0x00502f34,
 * 0x004572f4 0x00502f48 */
enum { CB_OBJ4_A, CB_OBJ4_B, CB_OBJ4_C, CB_OBJ4_COUNT };
void cb_obj4_register(const CbLevelEnv* e, int which, uint32_t obj);
/* 0x004573d4: target object 0x00502f60 = obj, obj +8 = 1 */
void cb_target_register(const CbLevelEnv* e, uint32_t obj);
/* 0x004573e4: 0x00502f64[i] = obj, obj +8 = 0 */
void cb_pool_object_register(const CbLevelEnv* e, uint32_t obj, int32_t i);
/* 0x004573f8: first of the 5 0x00502f64 objects that is set with +8 == 0, else 0 */
uint32_t cb_pool_object_free(const CbLevelEnv* e);
/* 0x004575b8: 0x00502f80[i] = v */
void cb_world_set_502f80(const CbLevelEnv* e, uint8_t v, int32_t i);

/* 0x0045600c: platform i entity + 0xE8 (its position, a game address) */
uint32_t cb_platform_target_pos(const CbLevelEnv* e, int32_t i);
/* 0x00456f84: platform slot i + 0x28 (a game address) */
uint32_t cb_platform_step_vec(const CbLevelEnv* e, int32_t i);
/* 0x00456f9c: platform slot i i32 +0x08 */
uint32_t cb_platform_step_angle(const CbLevelEnv* e, int32_t i);
/* 0x00453ed0 / 0x00453f10 (scene_rotate_b / _a): when the slam arm (0x00502e90) / claw (0x00502e94)
 * object is set: its angle 0x00502e98 / 0x00502e9c = a, matrix (obj + 4) = build_matrix({(i16)a, 0, 0}). */
void cb_slam_arm_rotate(const CbLevelEnv* e, int32_t a);
void cb_slam_claw_rotate(const CbLevelEnv* e, int32_t a);
/* 0x0044f11c / 0x0044f120: combo block + 0x30 (tap list) / i32 +0x2C (tap count); block is a game address */
uint32_t cb_combo_taps(uint32_t block);
uint32_t cb_combo_count(const CbLevelEnv* e, uint32_t block);

#endif /* TM2_CB_LEVEL_H */
