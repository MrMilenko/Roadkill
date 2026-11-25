/*
 * gp_hud.h: HUD primitive setup, ported from TM2.EXE.
 *
 *   0x00435c54 gp_hud_panels_init   both views' (stride 0x800) panel primitives: colours (10, 10, 10),
 *                                   codes 0x28, a quad at the screen offset (0x004e673c, 0x004e6740)
 *                                   + (0x5A, 10), two small coloured prims; and the rear view mirror
 *                                   prims (0x00435e78) in every view pass when split mode (0x0046cc54)
 *                                   is 0
 *   0x00435e78 gp_hud_mirror_init   the rear view mirror frame prims at 0x004f71e0..0x004f723b
 *   0x00436ff4 gp_hud_radar_arrows  for ids 0x50A..0x50D: two images from data into the radar arrow
 *                                   slot pair 0x004fb11c + 0x10 k and + 8 (0x00496da0(data, slot, 1)),
 *                                   k = 0, 3, 1, 2 for 0x50A, 0x50B, 0x50C, 0x50D; other ids: nothing
 *   0x00435934 gp_hud_dash_icons    dashboard icons from a weapon icon set (data: u8 +0x00 == 1 and
 *                                   u8 +0x14 >= 5, else nothing): weapon icons (count = the first
 *                                   load's result, i16 0x004f6b14), the selected icon, a 10 image
 *                                   strip loaded straight into 0x004f70d8, the bars, the second
 *                                   icon row (count reloaded into 0x004f6b14); then the panels
 *                                   (0x00435c54). Runs for view 0 only (the loop counter starts at 1
 *                                   and stops at -1).
 *                                   ORIGINAL BUG: with a first count <= 0 the bar slot index is an
 *                                   uninitialised stack word (the tests keep counts 1..10; a count
 *                                   above 10 reads past the 10 record stack buffer).
 * Memory: env->d is game 0x004f6a40..0x004f7a3f (offsets below are from 0x004f6a40).
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_hud.sh).
 */
#ifndef TM2_GP_HUD_H
#define TM2_GP_HUD_H

#include <stdint.h>

#define GP_HUD_BASE 0x004f6a40u

typedef struct {
    uint8_t* d;                 /* 0x004f6a40 .. 0x004f7a3f */
    const uint16_t* scr_x;      /* 0x004e673c */
    const uint16_t* scr_y;      /* 0x004e6740 */
    void* user;
    int32_t (*split)(void* user);                                       /* 0x0046cc54 */
    void (*image_load)(void* user, uint32_t data, uint32_t slot, int32_t one);   /* 0x00496da0 */
    /* 0x00496da0(src, out, n) as gp_hud_dash_icons uses it: n image records of 8 bytes (u8 u, v, w, h,
     * u16 page, 2 more) into out (game address out_addr, or 0 for the function's stack buffer);
     * returns a value whose low 16 bits are stored */
    uint32_t (*images)(void* user, uint32_t src, uint8_t* out, uint32_t out_addr, int32_t n);
} GpHudEnv;

void gp_hud_panels_init(GpHudEnv* env);
void gp_hud_mirror_init(GpHudEnv* env);
void gp_hud_radar_arrows(GpHudEnv* env, int32_t id, uint32_t data);
void gp_hud_dash_icons(GpHudEnv* env, const uint8_t* data);

#endif /* TM2_GP_HUD_H */
