/*
 * cb_fx_draw.h: draws the 20 impact particles, TM2.EXE 0x0045186c (old name hud_draw_base; the
 * PS1 fxImpactParticlesDraw). Particles: 0x24 byte records at 0x00500034 (see
 * src/weapons/fx_sprite.h): +0 i32[3] position, +0x18 u8 frame, +0x1C i32 life, +0x20 frame table
 * (8 bytes per frame: u8 u0, v0, u1, v1, u16 +4 texture page).
 *
 * One primitive record on the stack (0x74 bytes, prim) set up once:
 *   0x0048e468(prim, 5), [0x0048e6cc: an empty function], 0x0048e810(prim, 1, 20, 20, 0) (size),
 *   0x0048e700(prim, 0), 0x0048e6d4(prim, 0, 0), 0x0048e7b8(prim, 0, 0xFF, 0xFF, 0xFF) (colour)
 * then per living particle (life > 0): 0x0048e810(prim, 0, x, y, z), 0x0048e8a4(prim, u0, v0, u1, v1),
 *   0x0048e798(prim, page), 0x00491698(view, prim).
 * The primitive helpers belong to the renderer; they are callbacks here (args in the original's
 * register order eax, edx, ebx, ecx, then the stack argument).
 *
 * Verified against the original under emulation (tools/emu/check_combat_fxdraw.sh).
 */
#ifndef TM2_CB_FX_DRAW_H
#define TM2_CB_FX_DRAW_H

#include <stdint.h>

#include "weapons/wmem.h"

#define CB_PRIM_SIZE 0x74

typedef struct {
    const WMem* mem;
    void (*prim_init)(void* user, uint8_t* prim, int32_t kind);                                   /* 0x0048e468 */
    void (*prim_vec)(void* user, uint8_t* prim, int32_t which, int32_t x, int32_t y, int32_t z);  /* 0x0048e810 */
    void (*prim_700)(void* user, uint8_t* prim, int32_t a);                                       /* 0x0048e700 */
    void (*prim_6d4)(void* user, uint8_t* prim, int32_t a, int32_t b);                            /* 0x0048e6d4 */
    void (*prim_color)(void* user, uint8_t* prim, int32_t a, int32_t r, int32_t g, int32_t b);    /* 0x0048e7b8 */
    void (*prim_uv)(void* user, uint8_t* prim, int32_t u0, int32_t v0, int32_t u1, int32_t v1);   /* 0x0048e8a4 */
    void (*prim_page)(void* user, uint8_t* prim, int32_t page);                                   /* 0x0048e798 */
    void (*draw)(void* user, uint32_t view, uint8_t* prim);                                       /* 0x00491698 */
    void* user;
    /* used by cb_fx_sprites_draw only */
    void (*prim_6c0)(void* user, uint8_t* prim, int32_t a);                                       /* 0x0048e6c0 */
    void (*particles_draw)(void* user, uint32_t view, int32_t mode);  /* 0x0045186c (cb_fx_particles_draw) */
} CbFxDrawEnv;

void cb_fx_particles_draw(const CbFxDrawEnv* e, uint32_t view);

/* 0x0045052c (old name hud_render; PS1 fxSpritesDraw): prim_init(prim, 5), 0x0048e6c0(prim, 1),
 * [0x0048e6cc empty], the impact particles (0x0045186c(view, mode)), then every running sprite of
 * the 150 at 0x00500304 (i8 +0x0A >= 0; with mode > 0 a sprite at frame 1 whose i16 +0x0C is 0 is
 * skipped):
 *   u8 +1 set: position +0x14..+0x1C = the 3 i32 at the game address +0x2C (follows its owner);
 *   0x0048e6d4(prim, 0, -8) when i8 +6 is 1, (prim, 1, 8) when it is 8, else (prim, 0, 0);
 *   u8 +2 set: [0x0048e6cc] colour(prim, 0, +3, +4, +5);
 *   frame record = +0x10 + 8 * (i8 +0x0A / u8 +9) (C division): uv(prim, b0, b1, b2, b3), page(prim, u16 +4);
 *   vec(prim, 0, x, y, z), vec(prim, 1, size, size, 0) with size = u8 +8 * (first frame's u8 +2) / 8;
 *   0x0048e700(prim, u8 +0 == 1); draw(view, prim).
 * Finally u8 0x00501f24 = 1 - itself (the blink phase).
 * ORIGINAL BUG: a running sprite with step (u8 +9) 0 divides by zero. */
void cb_fx_sprites_draw(const CbFxDrawEnv* e, uint32_t view, int32_t mode);

#endif /* TM2_CB_FX_DRAW_H */
