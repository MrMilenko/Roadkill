/*
 * pf4_present.c: TM2.EXE 0x004b52b8 ddraw_present_rect (docs/PLATFORM.md Part B), ported over Pf2Env
 * (pf2_env.h). Emulator diff: tools/emu/check_pf4_present.sh.
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define LOST 0x887601c2u
#define WAIT 0x01000000u
#define F_RESTORE 0x004b4508u   /* display_restore */

static uint32_t blt(Pf2Env* e, uint32_t dst, uint32_t drect, uint32_t src, uint32_t srect) {
    return PF2_C5(e, PF2_ISURF, 0x14, dst, drect, src, srect, WAIT, 0);
}

/* 0x004b52b8 ddraw_present_rect(eax D, edx RECT* or NULL). A rectangle equal to {0, 0, W, H} counts
   as NULL (a full present); the local rect = *rect, or SetRect(0, 0, W, H). No primary: 0.
   IsLost(primary) == DDERR_SURFACELOST: display_restore. Sync: Lock(primary, NULL, &desc {0x6c},
   DDLOCK_WAIT, NULL), Unlock on success. Then by the present mode +0x74 (two 5 entry jump tables):
   full present with page flipping (+0x6c): 0 Blt(primary, &+0x78, offscreen), 1 Blt(primary, NULL,
   offscreen), 3 Blt(back buffer, NULL, offscreen) + Flip(primary, NULL, WAIT), 4 Flip; otherwise:
   0 Blt(primary, &+0x78, offscreen) (rectangle ignored), 1 Blt(primary, NULL, offscreen), 3
   Blt(primary, &local, offscreen, rect), 4 Blt(primary, rect, offscreen, rect). Mode 2 or > 4: the
   sync result. A final DDERR_SURFACELOST: display_restore. Returns the last result. */
uint32_t pf4_ddraw_present_rect(Pf2Env* e, uint32_t d, uint32_t rect) {
    uint32_t r = e->scratch, desc = e->scratch + 0x10;
    if (rect && R32(e, rect) == 0 && R32(e, rect + 4) == 0 && R32(e, d + 0x58) == R32(e, rect + 8) &&
        R32(e, d + 0x5c) == R32(e, rect + 0xc))
        rect = 0;
    if (!rect) {
        pf2_call5(e, 0x00c90574u, r, 0, 0, R32(e, d + 0x58), R32(e, d + 0x5c));
    } else {
        for (uint32_t k = 0; k < 16; k += 4) W32(e, r + k, R32(e, rect + k));
    }
    uint32_t p = R32(e, d + 0xc);
    if (!p) return 0;
    if (PF2_C0(e, PF2_ISURF, 0x60, p) == LOST) pf2_call1(e, F_RESTORE, d);
    pf2_zero(e, desc, 0x6c);
    W32(e, desc, 0x6c);
    uint32_t x = PF2_C4(e, PF2_ISURF, 0x64, R32(e, d + 0xc), 0, desc, 1, 0);
    if (x == 0) x = PF2_C1(e, PF2_ISURF, 0x80, R32(e, d + 0xc), 0);
    uint8_t mode = pf2_r8(e, d + 0x74);
    uint32_t prim = R32(e, d + 0xc), off = R32(e, d + 0x14);
    if (R32(e, d + 0x6c) != 0 && rect == 0) {
        switch (mode) {
        case 0: x = blt(e, prim, d + 0x78, off, 0); break;
        case 1: x = blt(e, prim, 0, off, 0); break;
        case 3:
            blt(e, R32(e, d + 0x10), 0, R32(e, d + 0x14), 0);
            /* fall through */
        case 4: x = PF2_C2(e, PF2_ISURF, 0x2c, R32(e, d + 0xc), 0, 1); break;
        default: break;
        }
    } else {
        switch (mode) {
        case 0: x = blt(e, prim, d + 0x78, off, 0); break;
        case 1: x = blt(e, prim, 0, off, 0); break;
        case 3: x = blt(e, prim, r, off, rect); break;
        case 4: x = blt(e, prim, rect, off, rect); break;
        default: break;
        }
    }
    if (x == LOST) pf2_call1(e, F_RESTORE, d);
    return x;
}
