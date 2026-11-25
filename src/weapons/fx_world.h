/*
 * fx_world.h: arena set pieces hit by weapons, ported from TM2.EXE (PS1 decomp:
 * src/semantic/fx_explosions.c, fx_debris_splash.c, weapon_fire_specials.c, eiffel_tower_emitters.c).
 *
 *   0x00457138 fx_splash_at          water splash: next of 4 splash objects (0x00502f20[1..4], cursor
 *                                    0x004e7414 wraps 5 -> 1 before use) moved to pos, animation
 *                                    0x00470320(0x2AC, i, 1); sound 0x21 (|speed| < 0x4B0) or 0x22 when
 *                                    the placed sound's |volume| (0x00c8500c) is below 0x5DC
 *   0x00457314 fx_pooled_blast       same with 0x00502f48[1..4] / cursor 0x004e741c / animation 0x2AE,
 *                                    only when that blast's animation is done (0x00470640 byte +7 == 0);
 *                                    the cursor wraps after use; returns 1 when started
 *   0x004534c8 light_flicker_tick    10 flickering lights (8 bytes at 0x005028c0: i32 frames, u32 object):
 *                                    model 0x00495864(rand() % 3 == 0) while counting, model 0 at the end
*   0x00455954 fxw_collapse_unlink (same function as src/world/collapse.c world_collapse_unlink; kept here so this area builds alone) clears (0xFF) every link to piece p in the 35 collapse records
 *                                    (0x24 bytes at 0x005029a4, six i8 links at +0..+5)
 *   0x00411e28 eiffel_tower_start_fall  a hit within 0x4B0 (x and y) of the tower base starts the fall:
 *                                    state at 0x004f08e0 (+0 u8 falling, +1 u8 = 20, +2 u8 blocked,
 *                                    +4 i16 = 0, +6 i16 = 75, +8 i16 = 0, +0x0A i16 = 50 + rand() / 0x290,
 *                                    +0x0C u32 object, +0x1C i32[3] pos, +0x28 i16[3] angles): explosion
 *                                    0x00450aa0 (fx_explosion48) at pos + (0, 600, 800), then z = 0x540
 *                                    and the object's matrix (+4) from the angles
 * PC differs from PS1: the splash sound choice reads the placed volume (distance) and the speed.
 *
 * Verified against the original under emulation (tools/emu/check_weapons_world.sh).
 */
#ifndef TM2_FX_WORLD_H
#define TM2_FX_WORLD_H

#include <stdint.h>

#include "weapons/fx_sprite.h"

typedef struct {
    const FxEnv* fx;             /* sound placement and play, sprites, mem (objects, tower block) */
    int32_t* splash_next;        /* 0x004e7414 */
    const uint32_t* splash_objs; /* 0x00502f20, 5 entries */
    int32_t* blast_next;         /* 0x004e741c */
    const uint32_t* blast_objs;  /* 0x00502f48, 5 entries */
    const uint32_t* blast_anims; /* 0x00621370 */
    uint8_t* flicker;            /* 0x005028c0 */
    uint8_t* collapse;           /* 0x005029a4 */
    uint8_t* tower;              /* 0x004f08e0 */
    uint32_t tower_addr;
    void* user;
    void (*anim_start)(void* user, int32_t id, int32_t i, int32_t on);    /* 0x00470320 */
    uint32_t (*model_variant)(void* user, int32_t i);                     /* 0x00495864 */
} WorldEnv;

void fx_splash_at(const WorldEnv* env, uint32_t pos, int32_t speed);
uint8_t fx_pooled_blast(const WorldEnv* env, uint32_t pos);
void light_flicker_tick(const WorldEnv* env);
void fxw_collapse_unlink(const WorldEnv* env, int32_t piece);
void eiffel_tower_start_fall(const WorldEnv* env, uint32_t pos);

#endif /* TM2_FX_WORLD_H */
