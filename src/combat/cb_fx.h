/*
 * cb_fx.h: the remaining effect sprite variants and the effect animation loaders of TM2.EXE
 * (0x0045075c..0x00451594). They sit on top of fx_sprite_spawn (src/weapons/fx_sprite.c, 0x00450380)
 * and the jitter point (src/weapons/fx_ground.c, 0x0044a40c); same FxEnv, same record layout
 * (see src/weapons/fx_sprite.h).
 *
 * Every variant returns what the original leaves in eax: the sprite record's game address, 0 when
 * the pool is full (the "void" ones in the decompiler output too; fx_flame_pair: the second one).
 *
 *   address     port                   size step frames table  count  after a successful spawn
 *   0x0045075c  cb_fx_spark_blue          5   1     4   +0x280   1   +2..5 = 1,0,0x14,100, +0x28 = 3
 *   0x00450b94  cb_fx_blast12            12   2  0x10   +0x200   1   -
 *   0x00450be0  cb_fx_blast24          0x18   1  0x10   +0x200   1   -
 *   0x00450de4  cb_fx_flame5              5   1     8   +0x500   1   +0 = 1, +0x0E = 100, +0x0C = 0x32
 *   0x00450e24  cb_fx_flame10            10   1     8   +0x500   1   +0 = 1, +0x0E = 200, +0x0C = 0x32
 *   0x00450e64  cb_fx_flame_pair         twice: jitter (0x199, 0x199, 0x555) then size 10 flame,
 *                                        +0x0E = 300 (as fx_ground_flame)
 *   0x00451014  cb_fx_smoke_a2            2   1  0x10   +0x320   1   grey 0x40, +0x28 = 6
 *   0x004510e0  cb_fx_smoke_a8            8   2  0x10   +0x320   0   grey 0x40, +0x28 = 15
 *   0x00451164  cb_fx_smoke_b2            2   1  0x10   +0x3A0   1   grey 0x80, +0x28 = 6
 *   0x004511a8  cb_fx_smoke_b3            3   2  0x10   +0x3A0   0   grey 0x80, +0x28 = 10
 *   0x004511ec  cb_fx_smoke_b5            5   2  0x10   +0x3A0   0   grey 0x80, +0x28 = 15
 *   0x00451230  cb_fx_smoke_c1            1   2  0x10   +0x420   1   grey 0x10, +0x28 = 1
 *   0x00451274  cb_fx_smoke_c2            2   2  0x10   +0x420   1   grey 0x10, +0x28 = 3
 *   0x004512b4  cb_fx_smoke_c3            3   2  0x10   +0x420   1   grey 0x10, +0x28 = 6
 *   0x0045133c  cb_fx_lightning           8   1    10   +0x558   1   +2..5 = 1,0xFF,200,0x32, +0x0C i16 = 5
 *   0x00451430  cb_fx_explosion_quiet8    8   1     2   +0x080   0   - (no sound)
 * (table: offset from the frame tables 0x00501f26; "grey g": +0x02 = 1, +0x03..+0x05 = g.)
 *
 * Animation loaders: 0x00496d04(src, dst, n) parses a sprite rect record into n frames of 8 bytes.
 *   0x004514ac +0x080 16 (explosion)   0x004514c0 +0x100 16 (smoke burst)   0x004514d4 +0x180 16 (freeze)
 *   0x004514e8 +0x200 16 (blast)       0x004514fc +0x320 16 (smoke a)       0x00451510 +0x3A0 16 (smoke b)
 *   0x00451524 +0x420 16 (smoke c)     0x00451538 +0x2C0 8 (ricochet)       0x0045154c +0x280 4 (spark)
 *   0x00451560 +0x2A0 4 (puff)         0x00451574 +0x4A0 8, then 0x00412638(+0x4A0) (contrail cells,
 *   stored at 0x004f11d8)              0x00451498 +0x500 10 (flame)         0x00451594 +0x558 30 (lightning)
 * They return what 0x00496d04 returns (0x00451574: 0x00412638's eax, the table address it stored).
 *
 * PC differs from PS1: the PS1 names (fxSparkBlue, fxSmokeA12, fxSmokeC6 ...) carry PS1 sizes; the
 * PC sizes are other values (smoke "A12" is size 2 here, "A128" 8, "B48" 5, "C6" 1, "C12" 2, "C24" 3).
 *
 * Verified against the original under emulation (tools/emu/check_combat_fx.sh).
 */
#ifndef TM2_CB_FX_H
#define TM2_CB_FX_H

#include <stdint.h>

#include "weapons/fx_ground.h"

uint32_t cb_fx_spark_blue(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_blast12(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_blast24(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_flame5(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_flame10(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_flame_pair(const GroundEnv* genv, uint32_t pos, uint8_t flag);
uint32_t cb_fx_smoke_a2(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_smoke_a8(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_smoke_b2(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_smoke_b3(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_smoke_b5(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_smoke_c1(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_smoke_c2(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_smoke_c3(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_lightning(const FxEnv* env, uint32_t pos, uint8_t flag);
uint32_t cb_fx_explosion_quiet8(const FxEnv* env, uint32_t pos, uint8_t flag);

typedef struct {
    uint32_t frames;    /* game address 0x00501f26 */
    int32_t (*parse)(void* user, uint32_t src, uint32_t dst, int32_t n);   /* 0x00496d04 */
    int32_t (*set_contrail_cells)(void* user, uint32_t table);             /* 0x00412638 */
    void* user;
} CbAnimEnv;

/* which: the loader, CB_ANIM_* (one per original function) */
enum {
    CB_ANIM_EXPLOSION, CB_ANIM_SMOKE_BURST, CB_ANIM_FREEZE, CB_ANIM_BLAST, CB_ANIM_SMOKE_A, CB_ANIM_SMOKE_B,
    CB_ANIM_SMOKE_C, CB_ANIM_RICOCHET, CB_ANIM_SPARK, CB_ANIM_PUFF, CB_ANIM_CONTRAIL, CB_ANIM_FLAME,
    CB_ANIM_LIGHTNING, CB_ANIM_COUNT
};
extern const uint32_t cb_anim_address[CB_ANIM_COUNT];   /* the original function of each */
int32_t cb_fx_anim_store(const CbAnimEnv* env, int which, uint32_t src);

#endif /* TM2_CB_FX_H */
