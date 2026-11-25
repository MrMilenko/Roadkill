/*
 * pf5_grid.h: grid_cell_triangle, TM2.EXE 0x004bcb70 (game code in the platform range; old
 * matcher name hdpPush: wrong). eax = object (+0x04 grid header), edx = point {i32 x, i32 z},
 * ebx = extra cell shift, ecx = out1 (4 x u16), stack: out2 (4 x u16); ret 4.
 *
 * Header: +0x04 u16 width, +0x06 u16 depth, +0x08 i16 x0, +0x0a i16 z0, +0x0e u8 cell shift x,
 * +0x0f u8 sub shift x, +0x10 u8 cell shift z, +0x11 u8 sub shift z, +0x12 bit 0 two layers;
 * cells from +0x14, 0x14 bytes each (0x28 with two layers).
 * Outside x0 <= x <= x0 + width / z0 <= z <= z0 + depth (signed): returns 0. Else, with
 * sx = cell shift x + ebx, sz = cell shift z + ebx (32 bit, shift counts taken mod 32 as x86 does),
 * dx = x - x0, dz = z - z0 (shifts are logical): cell = ((dz >> sz) * (width >> sx) + (dx >> sx));
 * ux = dx >> sub x, uz = dz >> sub z; masks mx = (1 << (sx - sub x)) - 1, mz likewise. Per layer:
 * u = ux, inverted when the cell's +0x06 bit 1 is set; (u & mx) > (uz & mz) (unsigned) picks the
 * first triangle (u16 +0, +2, +4, +0x10; flag +0x06) else the second (+8, +0xa, +0xc, +0x12; flag
 * +0x0e); the triangle's flag bit 0 clear: result 0 and nothing copied, else it is copied to out.
 * Layer 2 (two layers only) from the cell + 0x14 into out2; a first layer miss does not stop it.
 * Returns 1 or 0.
 *
 * Verified against the original under emulation (tools/emu/check_pf5_grid.sh).
 */
#ifndef PF5_GRID_H
#define PF5_GRID_H

#include "pf2_env.h"

uint32_t pf5_grid_cell_triangle(Pf2Env* e, uint32_t obj, uint32_t pt, uint32_t extra, uint32_t out1, uint32_t out2);

#endif /* PF5_GRID_H */
