/*
 * en7_j.c: see en7_j.h.
 */
#include "en7_j.h"

#define EN7_SetRect        0x00c90574u
#define EN7_ClientToScreen 0x00c90470u
#define EN7_OffsetRect     0x00c9052cu
#define EN7_IntersectRect  0x00c904f4u

/* 0x004a2ffc (render_configure): blits the background image o (+0 width, +4 height, +0x24 its
 * DirectDraw surface, +0x28 color keyed, +0x2c window) at (x, y) (-1 = keep +0x1c / +0x20, else
 * stored there) onto surf (0 = the display's back surface 0x00c84854). For the primary surface
 * (0x00c8484c) the point and the screen rect move to screen coordinates (ClientToScreen). The
 * destination rect is clipped to the screen (IntersectRect); nothing left -> 1. Blt (+0x14)(surf,
 * &dst, image surface, &src (dst moved back by x, y), DDBLT_WAIT | DDBLT_KEYSRC when keyed, 0);
 * DDERR_SURFACELOST restores the image surface (+0x6c). Returns 0. */
uint32_t en7_background_blit(En2Env* e, uint32_t o, uint32_t x, uint32_t y, uint32_t surf)
{
    const uint32_t SR = e->scratch, DR = e->scratch + 0x10, SRC = e->scratch + 0x20, PT = e->scratch + 0x30;
    if (x == 0xffffffffu) x = en2_r32(e, o + 0x1c); else en2_w32(e, o + 0x1c, x);
    if (y == 0xffffffffu) y = en2_r32(e, o + 0x20); else en2_w32(e, o + 0x20, y);
    if (surf == 0) surf = en2_r32(e, 0x00c84854u);
    uint32_t w = en2_call0(e, 0x004958d8u);
    uint32_t h = en2_call0(e, 0x004958e4u);
    en2_call5(e, EN7_SetRect, SR, 0, 0, w, h);
    if (surf == en2_r32(e, 0x00c8484cu)) {
        en2_w32(e, PT, x);
        en2_w32(e, PT + 4, y);
        en2_call2(e, EN7_ClientToScreen, en2_r32(e, o + 0x2c), PT);
        en2_call3(e, EN7_OffsetRect, SR, en2_r32(e, PT) - x, en2_r32(e, PT + 4) - y);
        x = en2_r32(e, PT);
        y = en2_r32(e, PT + 4);
    }
    en2_call5(e, EN7_SetRect, DR, x, y, en2_r32(e, o) + x, en2_r32(e, o + 4) + y);
    en2_call3(e, EN7_IntersectRect, DR, DR, SR);
    if (en2_r32(e, DR) == 0 && en2_r32(e, DR + 4) == 0 && en2_r32(e, DR + 8) == 0 && en2_r32(e, DR + 12) == 0)
        return 1;
    en2_call5(e, EN7_SetRect, SRC, en2_r32(e, DR) - x, en2_r32(e, DR + 4) - y, en2_r32(e, DR + 8) - x,
              en2_r32(e, DR + 12) - y);
    uint32_t flags = en2_r32(e, o + 0x28) != 0 ? 0x01008000u : 0x01000000u;
    uint32_t v[6] = {surf, DR, en2_r32(e, o + 0x24), SRC, flags, 0};
    if (e->call(e->user, en2_r32(e, en2_r32(e, surf) + 0x14), v, 6) == 0x887601c2u) {
        uint32_t s = en2_r32(e, o + 0x24);
        en2_call1(e, en2_r32(e, en2_r32(e, s) + 0x6c), s);
    }
    return 0;
}
