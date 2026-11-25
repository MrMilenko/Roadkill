/*
 * pf5_grid.c: grid_cell_triangle, TM2.EXE 0x004bcb70 (see pf5_grid.h).
 */
#include "pf5_grid.h"

static int layer(Pf2Env* e, uint32_t cell, uint32_t ux, uint32_t uz, uint32_t mx, uint32_t mz, uint32_t out) {
    uint32_t u = ux;
    if (pf2_r8(e, cell + 6) & 2) u = ~u;
    if ((u & mx) > (uz & mz)) {
        if (!(pf2_r8(e, cell + 6) & 1)) return 0;
        pf2_w16(e, out, pf2_r16(e, cell));
        pf2_w16(e, out + 2, pf2_r16(e, cell + 2));
        pf2_w16(e, out + 4, pf2_r16(e, cell + 4));
        pf2_w16(e, out + 6, pf2_r16(e, cell + 0x10));
    } else {
        if (!(pf2_r8(e, cell + 0xe) & 1)) return 0;
        pf2_w16(e, out, pf2_r16(e, cell + 8));
        pf2_w16(e, out + 2, pf2_r16(e, cell + 0xa));
        pf2_w16(e, out + 4, pf2_r16(e, cell + 0xc));
        pf2_w16(e, out + 6, pf2_r16(e, cell + 0x12));
    }
    return 1;
}

uint32_t pf5_grid_cell_triangle(Pf2Env* e, uint32_t obj, uint32_t pt, uint32_t extra, uint32_t out1, uint32_t out2) {
    uint32_t h = pf2_r32(e, obj + 4);
    int32_t px = (int32_t)pf2_r32(e, pt), pz = (int32_t)pf2_r32(e, pt + 4);
    int32_t x0 = (int16_t)pf2_r16(e, h + 8), z0 = (int16_t)pf2_r16(e, h + 0xa);
    if (x0 > px || (int32_t)((uint32_t)pf2_r16(e, h + 4) + (uint32_t)x0) < px) return 0;
    if (z0 > pz || (int32_t)((uint32_t)pf2_r16(e, h + 6) + (uint32_t)z0) < pz) return 0;
    uint32_t sx = pf2_r8(e, h + 0xe) + extra, sz = pf2_r8(e, h + 0x10) + extra;
    uint32_t dx = (uint32_t)px - (uint32_t)x0, dz = (uint32_t)pz - (uint32_t)z0;
    uint32_t subx = pf2_r8(e, h + 0xf), subz = pf2_r8(e, h + 0x11);
    uint32_t mx = (1u << ((sx - subx) & 31)) - 1, mz = (1u << ((sz - subz) & 31)) - 1;
    uint32_t cx = dx >> (sx & 31), cz = dz >> (sz & 31);
    uint32_t wc = (uint32_t)pf2_r16(e, h + 4) >> (sx & 31);
    uint32_t two = pf2_r8(e, h + 0x12) & 1;
    uint32_t cell = h + 0x14 + (cz * wc + cx) * (two ? 0x28u : 0x14u);
    uint32_t ux = dx >> (subx & 31), uz = dz >> (subz & 31);
    uint32_t r = 1;
    if (!layer(e, cell, ux, uz, mx, mz, out1)) r = 0;
    if (pf2_r8(e, h + 0x12) & 1) {
        if (!layer(e, cell + 0x14, ux, uz, mx, mz, out2)) r = 0;
    }
    return r;
}
