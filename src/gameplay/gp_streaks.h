/*
 * gp_streaks.h: the light streak effects pass of a view, ported from TM2.EXE 0x0041321c (old names
 * streaksDraw / vehicle_position_update).
 *
 * gp_streaks_draw(view, arg): a light set on the stack (0x0048e468(L, 1), 0x0048e6d4(L, 0, 0)); the
 * screen flash fades (0x00415c8c(u32 *view)) while its frames (0x004f5df0) are > 0; the passes
 * 0x00448b1c(view, arg), 0x00449424(), 0x00447748(view, arg); then each of the 40 streak records
 * (0x38 bytes from 0x004f0910) with life i32 +0x28 > 0 sets light 0 / 1 directions from i32 +0x00..
 * +0x08 / +0x0C..+0x14 (0x0048e810(L, i, x, y, z)), colours (0x0048e7b8(L, i, r, g, b)): white for
 * both when u8 +0x34 is set, else c = (life & 31) + 0xDF: (c, c, c) and (c - 60, (c - 60) >> 1, 0)
 * (low bytes), and draws (0x00491698(view, L)).
 * The light set is the callee's business: the port hands an opaque 0x44 byte buffer around.
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_streaks.sh).
 */
#ifndef TM2_GP_STREAKS_H
#define TM2_GP_STREAKS_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;              /* the view record (its first word) */
    const uint8_t* streaks;       /* 0x004f0910, 40 x 0x38 */
    const int32_t* flash_frames;  /* 0x004f5df0 */
    void* user;
    void (*light_init)(void* user, uint8_t* l, int32_t a);                          /* 0x0048e468 */
    void (*light_mode)(void* user, uint8_t* l, int32_t a, int32_t b);               /* 0x0048e6d4 */
    void (*flash_fade)(void* user, uint32_t v);                                     /* 0x00415c8c */
    void (*pass_a)(void* user, uint32_t view, uint32_t arg);                        /* 0x00448b1c */
    void (*pass_b)(void* user);                                                     /* 0x00449424 */
    void (*pass_c)(void* user, uint32_t view, uint32_t arg);                        /* 0x00447748 */
    void (*light_dir)(void* user, uint8_t* l, int32_t i, int32_t x, int32_t y, int32_t z);   /* 0x0048e810 */
    void (*light_col)(void* user, uint8_t* l, int32_t i, int32_t r, int32_t g, int32_t b);   /* 0x0048e7b8 */
    void (*draw)(void* user, uint32_t view, uint8_t* l);                            /* 0x00491698 */
} GpStreakEnv;

void gp_streaks_draw(GpStreakEnv* env, uint32_t view, uint32_t arg);

#endif /* TM2_GP_STREAKS_H */
