/*
 * gp_misc.h: small gameplay state helpers, ported from TM2.EXE.
 *
 *   0x00412d58 gp_effects_reset_all  clears the 40 effect records (u32 +0 of each 0x38 record at
 *                                    0x004f0938), ends every projectile slot (life -1, its render
 *                                    object hidden: +0x08 = 0 when the pointer +0x34 is set) and the
 *                                    screen flash frames 0x004f5df0
 *   0x00412dac gp_effects_reset      only the 40 effect records
 *   0x00415c8c gp_flash_fade         screen flash frames - 1; under 8 the quad colour (0x004f5dc4 r g b)
 *                                    eases to grey ((3c + 0x80) >> 2) and is copied to the second quad
 *                                    (0x004f5ddc)
 *   0x00436010 gp_hud_msg_reset      HUD message pair: both owners from 0x0045bab8, priorities 50
 *   0x00436040 gp_hud_msg_post       a message with a lower priority value than the current one
 *                                    replaces it (second owner from 0x0045bab8, both timers 50);
 *                                    otherwise it becomes the second one
 *   0x00436120 gp_hud_color          bytes +4 / +5 / +6 of a primitive by kind 0..3 (other = 3)
 *   0x00436f60 gp_toggle_a           u8 0x004e685e[i] = 1 - itself
 *   0x00436f80 gp_toggle_b           u8 0x004e6860[i] = 1 - itself
 *   0x00436f98 gp_cycle              u8 0x004f6a38[i] + 1, wrapping after 1 (after 2 in multiplayer,
 *                                    0x004632b0)
 * HUD message words (i32 at 0x004fb15c): +0 owner, +4 second owner, +8 priority, +0x0C second
 * priority, +0x10 timer.
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_misc.sh).
 */
#ifndef TM2_GP_MISC_H
#define TM2_GP_MISC_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;          /* projectile render objects */
    uint8_t* effects;         /* 0x004f0938, 40 x 0x38 */
    uint8_t* slots;           /* 0x004f11dc, 16 x 0x4BC */
    int32_t* flash_frames;    /* 0x004f5df0 */
    uint8_t* flash;           /* 0x004f5dc4 .. 0x004f5ddf */
    int32_t* msg;             /* 0x004fb15c, 5 words */
    uint8_t* toggle_a;        /* 0x004e685e */
    uint8_t* toggle_b;        /* 0x004e6860 */
    uint8_t* cycle;           /* 0x004f6a38 */
    void* user;
    int32_t (*msg_owner)(void* user);      /* 0x0045bab8 */
    int32_t (*multiplayer)(void* user);    /* 0x004632b0 (low byte tested) */
} GpMiscEnv;

void gp_effects_reset_all(GpMiscEnv* env);
void gp_effects_reset(GpMiscEnv* env);
void gp_flash_fade(GpMiscEnv* env);
void gp_hud_msg_reset(GpMiscEnv* env);
void gp_hud_msg_post(GpMiscEnv* env, int32_t owner, int32_t prio);
void gp_hud_color(int32_t kind, uint8_t* prim);
void gp_toggle_a(GpMiscEnv* env, int32_t i);
void gp_toggle_b(GpMiscEnv* env, int32_t i);
void gp_cycle(GpMiscEnv* env, int32_t i);

#endif /* TM2_GP_MISC_H */
