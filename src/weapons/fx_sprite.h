/*
 * fx_sprite.h: effect sprites (explosions, smoke, flame bits) and bouncing impact particles,
 * ported from TM2.EXE (PS1 decomp: src/semantic/fx_sprite_spawn.c, fx_explosions.c).
 *
 *   0x00450380 fx_sprite_spawn        take a free sprite record and start it
 *   0x00450864 fx_explosion8          explosion sound 8 (prio 75) + sprite size 8
 *   0x00450984 fx_explosion16         explosion sound 7 (prio 75) + sprite size 16
 *   0x00450aa0 fx_explosion48         explosion sound 7 (volume / 10, prio 99) + sprite size 48, 8 loops
 *   0x00450c50 fx_smoke_burst         explosion sound 8 (prio 75) + rising smoke sprite size 8
 *   0x004508f4 fx_explosion12         explosion sound 8 (prio 99) + sprite size 12
 *   0x00450a10 fx_explosion24         explosion sound 7 (volume / 2, prio 99) + sprite size 24
 *   0x00450c08 fx_freeze_burst        blue burst, rises 6 (freeze missile hit)
 *   0x004512f8 fx_freeze_glow         dim size 4 glow, rises 10
 *   0x00451058 fx_smoke_small         grey size 3 smoke, rises 10
 *   0x0045109c fx_smoke               grey size 5 smoke, rises 15
 *   0x004507a0 fx_puff                trail puff, size 5, 4 frames, rises 3
 *   0x00450718 fx_bullet_spark        orange size 5 spark, 4 frames, rises 3 (bullet hit)
 *   0x00450fd4 fx_bullet_smoke        grey size 1 smoke, rises 3
 *   0x004513c8 fx_ricochet            size 8, one of 6 random 3 frame animations
 *   0x00450f04 fx_ground_flame        burning ground flame, size 10, +0x00 = 1, +0x0C 50, +0x0E 300
 *   0x00450f44 fx_flame_bit           small random flame bit (size 3, random frame, random drift)
 *   0x004516bc fx_impact_debris       debris sound 6 + n bouncing particles
 *   0x004517e0 fx_impact_particles_update   moves the 20 particles (gravity 4, bounce, frame cycle)
 *
 * Sprite record (150 of 0x30 bytes at 0x00500304; free while i8 +0x0A < 0):
 *   +0x00 u8 = 0          +0x01 u8 flag (caller)   +0x02 u8 = 1
 *   +0x03..+0x05 u8 colour/fade bytes (0xFF, callers change them)
 *   +0x06 u8 loop count   +0x08 u8 size ("kind")   +0x09 u8 frame step
 *   +0x0A i8 frame (0 = running, < 0 = free)       +0x0B u8 step * frames (8 bit product)
 *   +0x0C i16 = 0         +0x0E i16 (ground flame: 300)   +0x10 u32 frame table (game address, 8 bytes per frame)
 *   +0x14 i32[3] position (z + 120 when size >= 0x60)   +0x20 i32[3] velocity
 *   +0x2C u32 the position it was spawned at (game address)
 * Particle record (20 of 0x24 bytes at 0x00500034):
 *   +0x00 i32[3] pos  +0x0C i32[3] vel  +0x18 u8 frame 0..3  +0x1C i32 life (> 0 = alive)
 *   +0x20 u32 frame table (game address)
 *
 * The explosion sound plays only when the cooldown (0x00500030, counted up elsewhere) is above 5,
 * and resets it. Sound placement 0x004b8010(pos, view) (via 0x0046f974 view 0 / 0x0046f980
 * view -1 in split screen) leaves volume and pan in 0x00c8500c / 0x00c85058 (read by 0x0046f988 /
 * 0x0046f990); split screen plays with pan 0. 0x0046f8b0(id, volume, pan, prio) plays.
 *
 * PC differs from PS1: sizes are other values (PS1 12/48/192 and 9 for the flame bit),
 * fx_sprite_spawn sets +0x02 = 1 and +0x03..+0x05 = 0xFF (PS1: 0), the explosion's volume
 * comes from the sound placement (the PS1 decomp reads it as PadChkMtap), and 0x00450c50 is the
 * PS1 fxSmokeBurstPink, not fxExplosion6.
 *
 * Verified against the original under emulation (tools/emu/check_weapons_fx.sh).
 */
#ifndef TM2_FX_SPRITE_H
#define TM2_FX_SPRITE_H

#include <stdint.h>

#include "weapons/wmem.h"

#define FX_SPRITE_COUNT 150
#define FX_SPRITE_SIZE 0x30
#define FX_PARTICLE_COUNT 20
#define FX_PARTICLE_SIZE 0x24

typedef struct {
    const WMem* mem;          /* positions passed in are game addresses */
    uint8_t* sprites;         /* 0x00500304 */
    uint32_t sprites_addr;    /* its game address (fx_sprite_spawn returns game addresses) */
    uint8_t* particles;       /* 0x00500034 */
    int32_t* boom_cooldown;   /* 0x00500030 */
    int32_t split;            /* split screen mode 0x00520e90 (0x0046cc54) */
    const int32_t* snd_volume;   /* 0x00c8500c, read after sound_locate */
    const int32_t* snd_pan;      /* 0x00c85058 */
    uint32_t frames;          /* game address 0x00501f26: the effect frame tables */
    void (*sound_locate)(void* user, uint32_t pos, int32_t view);                 /* 0x004b8010 */
    void (*sound_play)(void* user, int32_t id, int32_t vol, int32_t pan, int32_t prio); /* 0x0046f8b0 */
    void* user;
} FxEnv;

/* returns the record's game address, 0 when all 150 are in use */
uint32_t fx_sprite_spawn(const FxEnv* env, uint32_t pos, int32_t kind, uint8_t step, uint8_t frames,
                         uint32_t table, uint8_t count, uint8_t flag);
void fx_explosion8(const FxEnv* env, uint32_t pos, uint8_t flag);
void fx_explosion16(const FxEnv* env, uint32_t pos, uint8_t flag);
void fx_explosion48(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t fx_smoke_burst(const FxEnv* env, uint32_t pos, uint8_t flag);
void fx_explosion12(const FxEnv* env, uint32_t pos, uint8_t flag);
void fx_explosion24(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t fx_freeze_burst(const FxEnv* env, uint32_t pos, uint8_t flag);
void fx_freeze_glow(const FxEnv* env, uint32_t pos, uint8_t flag);
void fx_smoke_small(const FxEnv* env, uint32_t pos, uint8_t flag);
void fx_smoke(const FxEnv* env, uint32_t pos, uint8_t flag);
void fx_puff(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t fx_ground_flame(const FxEnv* env, uint32_t pos, uint8_t flag);
void fx_bullet_spark(const FxEnv* env, uint32_t pos, uint8_t flag);
void fx_bullet_smoke(const FxEnv* env, uint32_t pos, uint8_t flag);
void fx_ricochet(const FxEnv* env, uint32_t pos, uint8_t flag);
void fx_flame_bit(const FxEnv* env, uint32_t pos);
void fx_impact_debris(const FxEnv* env, int32_t n, uint32_t pos);
void fx_impact_particles_update(const FxEnv* env);

#endif /* TM2_FX_SPRITE_H */
