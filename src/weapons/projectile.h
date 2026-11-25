/*
 * projectile.h: the 16 slot missile / projectile array, ported from TM2.EXE
 * (PS1 decomp: weaponProjectileUpdate, fxRecordsUpdate, weaponFire*Missile in
 * src/semantic/weapon_fire_missiles.c, weapon_fire_specials.c, weapon_special_missiles.c,
 * weapon_missile_trails.c, projectile_hit.c).
 *
 *   0x00412670 proj_store_model        keeps a missile model pointer by model id
 *   0x00413e90 proj_object_init        places the slot's render object (angles, position, matrix)
 *   0x0041354c proj_trail_start        resets the smoke trail strip behind a new missile
 *   0x004136d4 proj_trail_uvs          writes the trail quads' texture frames
 *   0x00413a44 proj_trail_step         moves the trail head (every 2nd frame a new segment)
 *   0x00414000 proj_spawn_plain        type 0x2B missile (life 50)
 *   0x00414458 proj_fire_homing        type 0x0B homing missile (life 50, target position)
 *   0x004146f8 proj_fire_colored       type 0x29 missile (life 100), no trail texture frames
 *   0x00414114 proj_fire_freeze        homing + type 0x0E (freeze missile)
 *   0x004142ac proj_fire_dark_tooth    homing + type 0x3D (life x5, random wobble mode)
 *   0x00414328 proj_fire_ghost         colored + type 0x31
 *   0x00414f98 proj_tick               one slot: special motion, flight, hit tests, impacts
 *   0x00415b54 proj_update_all         ends expired slots and ticks the rest
 *
 * Slot (16 of 0x4BC bytes at 0x004f11dc):
 *   +0x00 i8 own index (set elsewhere; the end of flight writes through it)
 *   +0x02 i16 type      +0x04 i16 life (< 1 = slot free; 0 ends it, -1 ended)
 *   +0x06 i16 speed (eased up by 5 a frame to 150)   +0x08 i16 mode (wobble 99 / homing aim)
 *   +0x0A i16 damage    +0x0C i32 what was hit (entity id, 9 = level)
 *   +0x28 i32[3] start position   +0x34 u32 render object   +0x38 u32 homing target position
 *   +0x3C u32 homing target object   +0x40 i8 trail head segment   +0x41 i8 trail head vertex group
 *   +0x42 i16 owner     +0x44 i8 trail segments (6 when placed, 1 for 0x2B)  +0x45 i8 trail countdown
 *   +0x46 i16 drop still to apply (16 a frame)
 *   +0x48 trail vertices: 7 groups of 3, 8 bytes each (i16 x, y, z relative to +0x28)
 *   +0xF0 u32 trail render object   +0x104 i32 trail polygon count (3 x segments)
 *   +0x160 trail quads: segment s, quad q at +0x160 + s*0x90 + q*0x30: i16 vertex indices
 *          +0x00..+0x06, u16 uv pairs +0x18..+0x27, u32 +0x28 page
 * Render object: +0x08 i32 visible, +0xB0 i32[9] matrix, +0xE0 i16 angles x / y / z
 *   (+0xE0 / +0xE2 / +0xE4), +0xE8 i32[3] position, +0x158 entity id, +0x15C model.
 * Model pointers 0x004f5d9c..0x004f5dbc (9 u32, models[0..8]); trail frame table pointer
 * 0x004f11d8 (8 bytes a frame: u8 u, v, w, h, u16 page).
 *
 * DECOMPILER TRAP: 0x004146f8 pops two stack arguments (ret 8; Ghidra shows one): the second is
 *   unused (0x00414328 passes a copy of the 8 bytes at 0x004e66e0). 0x004136d4 also gets the slot
 *   index in EDX, unused.
 * ORIGINAL BUG: proj_tick ends a slot through its own index byte (+0x00), not the slot it runs on.
 *   A type 0x0F missile whose entity test hits 9 on the first (half step) test reads the level
 *   record before any level test ran (a null pointer); the test never feeds it that case.
 *   The freeze (0x0E) and dark tooth types are set after proj_object_init, so its 0x0E pitch
 *   change (-0xAA) never applies to 0x00414114; only callers that set the type first get it.
 * The flight effects run through env->fx by address; the ported ones are in fx_sprite.c
 * (0x00450c08, 0x004512f8, 0x00450c50, 0x004508f4, 0x00451058, 0x00450a10, 0x0045109c, 0x004507a0)
 * and fx_ground.c (0x0044a40c jitter, 0x0044a4cc ground fire), the hit response in weapon_hit.c.
 *
 * Verified against the original under emulation (tools/emu/check_weapons_proj.sh).
 */
#ifndef TM2_PROJECTILE_H
#define TM2_PROJECTILE_H

#include <stdint.h>

#include "weapons/wmem.h"

#define PROJ_SLOTS 16
#define PROJ_SLOT_SIZE 0x4BC
#define PROJ_SLOTS_ADDR 0x004f11dcu
#define PROJ_MODELS_ADDR 0x004f5d9cu

/* effect functions the flight code starts, by their address in TM2.EXE */
enum {
    PROJ_FX_SPARKS = 0x00450c08,      /* freeze hit */
    PROJ_FX_FREEZE_HIT = 0x004512f8,
    PROJ_FX_SMOKE_BURST = 0x00450c50, /* src/weapons/fx_sprite.c fx_smoke_burst */
    PROJ_FX_EXPLOSION = 0x004508f4,
    PROJ_FX_DEBRIS = 0x00451058,
    PROJ_FX_BIG_EXPLOSION = 0x00450a10,
    PROJ_FX_FLASH = 0x0045109c,
    PROJ_FX_GROUND_FIRE = 0x0044a4cc,
    PROJ_FX_TRAIL_PUFF = 0x004507a0,  /* at a jittered point (pos = NULL, pos_addr = the point) */
};

typedef struct {
    const WMem* mem;          /* render objects, targets, car blocks, collision record, frame table */
    uint8_t* slots;           /* 0x004f11dc */
    uint32_t* models;         /* 0x004f5d9c, 9 entries */
    uint32_t trail_frames;    /* value of 0x004f11d8 */
    int32_t split;            /* 0x00520e90 */
    const int32_t* snd_volume;   /* 0x00c8500c */
    const int32_t* snd_pan;      /* 0x00c85058 */
    void* user;

    /* sound placement 0x004b8010 (view 0, or -1 in split screen) and play 0x0046f8b0 */
    void (*sound_locate)(void* user, const int32_t pos[3], int32_t view);
    void (*sound_play)(void* user, int32_t id, int32_t vol, int32_t pan, int32_t prio);
    /* 0x0046f934(type, slot + 1): ends the missile's engine sound */
    void (*sound_end)(void* user, int32_t type, int32_t slot1);
    /* an effect at pos (pos_addr: the render object's position when the original passes that, else 0) */
    void (*fx)(void* user, uint32_t fn, const int32_t pos[3], uint32_t pos_addr);
    /* 0x0044a40c(pos, a, b, c): a random point near pos, returns its game address */
    uint32_t (*jitter)(void* user, const int32_t pos[3], uint32_t pos_addr, int32_t a, int32_t b, int32_t c);
    /* 0x00474508 game event type (low byte), 0x0045b0b8(0xF0) entity by type (game address or 0) */
    int32_t (*event_type)(void* user);
    uint32_t (*entity_by_type)(void* user, int32_t type);
    /* 0x004147c8: homing steering of a slot with a target (slot game address) */
    void (*steer)(void* user, uint32_t slot);
    /* 0x004126b4(pos, owner, radius, type): entity hit, 0 none */
    int32_t (*hit_test)(void* user, const int32_t pos[3], int32_t owner, int32_t radius, int32_t type);
    /* 0x0048df8c(owner, model id, pos, &hit): level hit; returns the collision record (game address)
     * and sets *hit */
    uint32_t (*level_hit)(void* user, int32_t owner, uint32_t model, const int32_t pos[3], int32_t* hit);
    /* 0x0042c2d4: ground height at the render object's position */
    int32_t (*ground)(void* user, const int32_t pos[3], uint32_t pos_addr);
    /* 0x00415d1c(hit, record +0x0A, record +0x0C, damage, velocity, owner) */
    void (*apply_hit)(void* user, int32_t hit, int32_t rtype, int32_t rc, int32_t dmg, const int32_t vel[3],
                      int32_t owner);
    /* 0x0045afb4 car count, 0x0045ae24(i) car model slot (result unused), 0x0045afbc(i) car block */
    int32_t (*car_count)(void* user);
    void (*car_model_slot)(void* user, int32_t i);
    uint32_t (*car_data)(void* user, int32_t i);
} ProjEnv;

void proj_store_model(ProjEnv* env, uint32_t model, uint32_t id);
void proj_object_init(ProjEnv* env, uint8_t* slot, uint32_t angles, uint32_t pos);
void proj_trail_start(ProjEnv* env, int32_t index);
void proj_trail_uvs(ProjEnv* env, uint8_t* slot);
void proj_trail_step(ProjEnv* env, uint8_t* slot);
/* the fire functions return the slot index or -1 (all 16 busy) */
void proj_spawn_plain(ProjEnv* env, int32_t owner, uint32_t angles, uint32_t pos, int32_t damage);
int32_t proj_fire_homing(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage);
int32_t proj_fire_colored(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage);
void proj_fire_freeze(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage);
int32_t proj_fire_dark_tooth(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage);
int32_t proj_fire_ghost(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage);
void proj_tick(ProjEnv* env, uint8_t* slot);
void proj_update_all(ProjEnv* env);

#endif /* TM2_PROJECTILE_H */
