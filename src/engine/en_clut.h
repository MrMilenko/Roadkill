/*
 * en_clut.h: palette (CLUT) animations, ported from TM2.EXE (PS1 clutAnim names):
 *
 *   0x00499ee8 clutAnimNewDef     new definition for (a, b); 0 when it exists or the table is full
 *   0x00499f50 clutAnimFindDef    definition for (a, b) or 0 (searches entries 0..last)
 *   0x00499fa4 (find slot)        running slot whose definition is (a, b) or 0
 *   0x00499ff0 clutAnimFreeSlot   first free slot or 0
 *   0x0049a020 (last differing)   slot +0x1c = last palette index whose color differs from the
 *                                 final entry's (count - 1); unchanged when all are equal
 *   0x00499984 (start sweep)      kind 1: lit band runs through the palette
 *   0x00499a34 (start fade)       kind 2: palette blanked then restored, `speed` entries a frame
 *   0x00499ae8 (stop fade)        restore phase of a running fade (step 2)
 *   0x00499b00 clutAnimStartCycle kinds 0 / 3: palette rotation, every `speed` + 1 frames
 *   0x00499b9c clutAnimRegister   definition from a mesh node (page from its first polygon)
 *   0x00499c74 (update, old name sound_update: wrong) per frame step of every running slot
 *   0x00499cd4 / 0x00499d80 / 0x00499e08  sweep / fade / cycle steps
 *
 * Everything lives in the game's 32 bit address space (the slots store palette addresses and the
 * cycle points a texture page's palette at a slot's own buffer), so the port works on addresses
 * through env->mem(), like the original:
 *   definitions 25 x 0x18 at 0x00a3a5e0: +0 u16 a (mesh node +0x02), +2 u16 b (node +0x14),
 *     +4 i32 kind (0 / 3 cycle, 1 sweep, 2 fade), +0x10 i32 colors (0x100), +0x14 i32 page;
 *     last used index 0x004e842c (-1 = none)
 *   slots 15 x 0x228 at 0x00a3a838: +0 active, +4 position, +8 tail, +0x0c counter / phase,
 *     +0x10 sweep start (length >> 2), +0x14 speed, +0x18 toggle, +0x1c length, +0x20 definition,
 *     +0x24 palette address, +0x28 u16[256] copy of the palette
 *   texture pages 0x30 bytes at 0x00a30c48: +0x0c palette address, +0x10 animated flag
 */
#ifndef TM2_EN_CLUT_H
#define TM2_EN_CLUT_H

#include <stdint.h>

typedef struct {
    uint8_t* (*mem)(void* user, uint32_t addr);   /* game address -> bytes */
    void* user;
    uint32_t defs;          /* 0x00a3a5e0 */
    uint32_t slots;         /* 0x00a3a838 */
    uint32_t pages;         /* 0x00a30c48 */
    int32_t last;           /* 0x004e842c */
    void (*bad_node)(void* user, int type);   /* printf 0x004a1f1e "Expecting an object node for clut anim %d" */
} EnClutEnv;

uint32_t en_clut_new_def(EnClutEnv* e, uint32_t a, uint32_t b);
uint32_t en_clut_find_def(EnClutEnv* e, uint32_t a, uint32_t b);
uint32_t en_clut_find_slot(EnClutEnv* e, uint32_t a, uint32_t b);
uint32_t en_clut_free_slot(EnClutEnv* e);
void en_clut_last_differing(EnClutEnv* e, uint32_t slot);
void en_clut_start_sweep(EnClutEnv* e, uint32_t a, uint32_t b, int32_t speed);
/* divisor must be nonzero (the original divides by it) */
void en_clut_start_fade(EnClutEnv* e, uint32_t a, uint32_t b, int32_t divisor);
void en_clut_stop_fade(EnClutEnv* e, uint32_t a, uint32_t b);
void en_clut_start_cycle(EnClutEnv* e, uint32_t a, uint32_t b, int32_t speed);
void en_clut_register(EnClutEnv* e, uint32_t node, int32_t kind);
void en_clut_update(EnClutEnv* e);
void en_clut_step_sweep(EnClutEnv* e, uint32_t slot);
void en_clut_step_fade(EnClutEnv* e, uint32_t slot);
void en_clut_step_cycle(EnClutEnv* e, uint32_t slot);

#endif /* TM2_EN_CLUT_H */
