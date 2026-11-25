/*
 * fx_ground.h: burning ground patches left by napalm style hits, ported from TM2.EXE
 * (PS1 decomp: groundFireSpawn, mathJitterPos in src/semantic/fx_sprite_spawn.c, fx_ground_fires.c).
 *
 *   0x0044a40c fx_jitter          a random point near pos (axes with spread 0 keep the last value)
 *   0x0044a4cc ground_fire_spawn  takes a free ground fire record and lights 5 flames around pos
 *
 * Jitter point: 3 i32 at 0x004fef70 (the returned address). Per axis with spread d != 0:
 * rand() & 1 ? pos + rand() / d : pos - rand() / d (C division).
 * Ground fire record (30 of 0x28 bytes at 0x004ff308): +0x00 u8 active, +0x04 i16 age,
 * +0x08 i32[3] pos, +0x14 u32[5] the flame sprites (game addresses, 0 when the pool was full).
 * Each flame: fx_ground_flame and fx_smoke at a jitter point (spread 0xCC, 0xCC, 0x555).
 * The free search reads one record past the end when all 30 burn (its value does not matter).
 *
 * Verified against the original under emulation (tools/emu/check_weapons_fx.sh).
 */
#ifndef TM2_FX_GROUND_H
#define TM2_FX_GROUND_H

#include <stdint.h>

#include "weapons/fx_sprite.h"

#define GROUND_FIRE_COUNT 30
#define GROUND_FIRE_SIZE 0x28

typedef struct {
    const FxEnv* fx;      /* its mem must map jit_addr to jit */
    uint8_t* fires;       /* 0x004ff308 */
    int32_t* jit;         /* 0x004fef70 */
    uint32_t jit_addr;
} GroundEnv;

uint32_t fx_jitter(const GroundEnv* env, uint32_t pos, int32_t dx, int32_t dy, int32_t dz);
void ground_fire_spawn(const GroundEnv* env, uint32_t pos);

#endif /* TM2_FX_GROUND_H */
