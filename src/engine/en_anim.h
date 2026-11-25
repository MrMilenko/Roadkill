/*
 * en_anim.h: scene culling test and DPC animation switch steps, ported from TM2.EXE.
 *
 *   0x004a043c en_sphere_visible  culling test of the tree walk (src/scene/dpc_walk.c callback)
 *   0x004a09f8 en_anim_loop       type 3 node, kind 0: advance a frame, wrap to the loop start
 *   0x004a0a78 en_anim_pingpong   kind 1: advance by the direction byte, bounce at both ends
 *   0x004a0b08 en_anim_random     kind 2: random frame 0 .. count - 2
 *   0x004a0b7c en_anim_weighted   kind 3: random frame 1 .. count - 1, low frames more likely
 *   0x004a0c1c en_anim_delay      new frame delay, random in the on or off range
 *   0x004a0c94 en_anim_set_step   set the frame of a type 9 switch node, trigger at its target
 * Verified under emulation: tools/emu/check_engine_anim.sh.
 *
 * Type 3 (animation switch) node fields used here (byte offsets):
 *   +0x05 u8  trigger frame: entering it calls the trigger with node +0x1C
 *   +0x06 u16 frame delay (frame ticks until the next step)
 *   +0x08 u8  first frame of the loop (kind 0) / low end (kind 1)
 *   +0x09 u8  last frame (kind 0, 1)
 *   +0x0B u8  frame count (kind 2, 3)
 *   +0x0C u8  current frame (the walk descends into child +0x20 + 4 * it)
 *   +0x0D u8  direction (kind 1: 1 or 0xff)
 *   +0x0E u16 ticks accumulated
 *   +0x13 u8  trigger mode passed with +0x1C
 *   +0x14 u16, +0x16 u16 delay range while the frame is not 0 ("on")
 *   +0x18 u16, +0x1A u16 delay range for frame 0 ("off")
 *   +0x1C u32 node to trigger (0 = none), passed to 0x004a0ccc (src/scene/dpc_collide.c dpc_trigger)
 * Type 9 (switch) node: +0x06 u8 count, +0x07 u8 selected, +0x08 u8 trigger mode, +0x09 u8
 * trigger frame, +0x0C u32 node to trigger.
 */
#ifndef TM2_EN_ANIM_H
#define TM2_EN_ANIM_H

#include <stdint.h>

typedef struct {
    /* 0x004899c2 rand(), 0..0x7fff */
    int32_t (*rand)(void* user);
    /* 0x004a0ccc (node, mode) */
    void (*trigger)(void* user, uint32_t node, uint32_t mode);
    void* user;
    uint16_t ticks;     /* 0x00a81c48 frame ticks (set by the scene draw from frame_get_ticks60) */
} EnAnimEnv;

/* node: the node's bytes (at least 0x20 readable) */
void en_anim_delay(const EnAnimEnv* e, uint8_t* node, int on);
void en_anim_loop(const EnAnimEnv* e, uint8_t* node);
void en_anim_pingpong(const EnAnimEnv* e, uint8_t* node);
/* kind 2: ORIGINAL BUG a count of 1 divides by zero (count 0 divides by -1 and gives frame 0) */
void en_anim_random(const EnAnimEnv* e, uint8_t* node);
/* kind 3: ORIGINAL BUG a count of 0 or 1 divides by zero. Does not reset the tick counter nor pick
 * a new delay, so once the first delay has run out it picks a new frame every frame. */
void en_anim_weighted(const EnAnimEnv* e, uint8_t* node);
/* by kind byte +0x0A (0..3; others do nothing) */
void en_anim_step(const EnAnimEnv* e, uint8_t* node);
/* 0x004a0c94: node may be NULL (does nothing) */
void en_anim_set_step(const EnAnimEnv* e, uint8_t* node, uint32_t frame);

/* 0x004a043c: is the sphere (bounds center + pos, radius) inside the view: transformed by mat_c
 * (M^T v, fx12) all three coordinates >= -radius, then by mat_d x, y >= -radius and
 * z >= -radius - (depth << shift). force (0x00a81c3c) set: always visible. Returns 1 or 0. */
typedef struct {
    int32_t force;      /* 0x00a81c3c */
    int32_t depth;      /* 0x004e8440 */
} EnCullEnv;
int en_sphere_visible(const EnCullEnv* e, const int32_t bounds[3], int32_t radius, const int32_t pos[3],
                      const int32_t mat_c[9], const int32_t mat_d[9], uint8_t shift);

#endif /* TM2_EN_ANIM_H */
