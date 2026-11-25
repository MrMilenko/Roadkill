/*
 * gpu_edge.c: polygon edges, ported from TM2.EXE (see gpu.h, docs/RENDERER.md).
 *   0x004af118 gpu_slope, 0x004af164 gpu_edge_setup, 0x004c6000 gpu_edge_walk.
 */
#include "gpu.h"

#include <string.h>

int32_t gpu_slope(int32_t minor, int32_t major, int32_t one) {
    if (major == 0) return one ? 0xffff : 0;
    if (major == minor) return one ? 0 : 0xffff;   /* x major 45 degrees: 0xffff; y major: 0 */
    /* idiv of (minor << 16) sign extended; |minor| <= |major| here so it cannot overflow */
    return (int32_t)((uint32_t)minor << 16) / major;
}

/* per octant: x major, up, minor tile step, major tile step, major dcol, major drow,
 * ends swapped (start at p1) */
#ifndef GPU_HI
typedef int8_t oct_t;
#else
typedef int16_t oct_t;   /* GPU_HI: tile steps past 127 */
#endif
static const oct_t OCT[8][7] = {
    {1, 0, GPU_COLS, 1, 1, 0, 0},
    {1, 0, GPU_COLS, -1, 1, 0, 0},
    {1, 1, GPU_COLS, -1, 1, 0, 1},
    {1, 1, GPU_COLS, 1, 1, 0, 1},
    {0, 0, 1, GPU_COLS, 0, 1, 0},
    {0, 0, -1, GPU_COLS, 0, 1, 0},
    {0, 1, -1, GPU_COLS, 0, 1, 1},
    {0, 1, 1, GPU_COLS, 0, 1, 1},
};

void gpu_edge_setup(GpuEdge* e, const int32_t* p0, const int32_t* p1) {
    int32_t dx = (int32_t)((uint32_t)p1[0] - (uint32_t)p0[0]);
    int32_t dy = (int32_t)((uint32_t)p1[1] - (uint32_t)p0[1]);
    /* ORIGINAL BUG: the signs are taken from bit 15 (>> 14 & 2, >> 15 & 1, >> 13 & 4), not
     * bit 31: differences of 0x8000 (1024 pixels) or more are put in the wrong octant. */
#ifndef GPU_HI
    uint32_t oct = (uint32_t)(dy >> 14) & 2u;
    oct |= (uint32_t)(dx >> 15) & 1u;
    int32_t ax = dx < 0 ? (int32_t)(0u - (uint32_t)dx) : dx;
    int32_t ay = dy < 0 ? (int32_t)(0u - (uint32_t)dy) : dy;
    oct |= (uint32_t)((int32_t)((uint32_t)ax - (uint32_t)ay) >> 13) & 4u;
#else
    /* GPU_HI (Roadkill's larger screens, not the original): the signs from bit 31, so edges of
     * 1024 pixels and more keep their octant */
    uint32_t oct = (dy < 0 ? 2u : 0u) | (dx < 0 ? 1u : 0u);
    int32_t ax = dx < 0 ? (int32_t)(0u - (uint32_t)dx) : dx;
    int32_t ay = dy < 0 ? (int32_t)(0u - (uint32_t)dy) : dy;
    oct |= ax < ay ? 4u : 0u;
#endif
    e->octant = oct;
    const oct_t* o = OCT[oct];
    e->slope = o[0] ? gpu_slope(ay, ax, 0) : gpu_slope(ax, ay, 1);
    e->xmajor = o[0];
    e->up = o[1];
    e->minor_tile = o[2];
    e->major_tile = o[3];
    e->major_dcol = o[4];
    e->major_drow = o[5];
    const int32_t* a = o[6] ? p1 : p0;
    const int32_t* b = o[6] ? p0 : p1;
    e->col0 = a[0] >> 8;
    e->row0 = a[1] >> 8;
    e->col1 = b[0] >> 8;
    e->row1 = b[1] >> 8;
    e->end_tile = (int32_t)((uint32_t)e->row1 * (uint32_t)GPU_COLS + (uint32_t)e->col1);
    uint32_t s = (uint32_t)e->slope, c0 = (uint32_t)e->col0 << 8, r0 = (uint32_t)e->row0 << 8;
    uint32_t ax0 = (uint32_t)a[0], ay0 = (uint32_t)a[1];
    uint32_t major, minor;   /* error along the major axis (times slope), distance on the minor */
    switch (oct) {
    case 0: case 3: major = c0 + 0x100u - ax0; minor = ay0 - r0; break;
    case 1: case 2: major = ax0 - c0; minor = ay0 - r0; break;
    case 4: case 7: major = r0 + 0x100u - ay0; minor = ax0 - c0; break;
    default: major = r0 + 0x100u - ay0; minor = c0 + 0x100u - ax0; break;   /* 5, 6 */
    }
    e->err = (int32_t)(s * major + (minor << 16));
    e->err_step = (int32_t)(s << 8);
}

static int32_t rd32(const uint8_t* p) {
    int32_t v;
    memcpy(&v, p, 4);
    return v;
}

/* reads the original makes through the viewport pointers, from the viewport block (port: 0
 * outside the block, where the original reads whatever memory follows) */
static int32_t vp_read32(const TM2Gpu* g, int64_t off) {
    if (off < 0 || off + 4 > (int64_t)(sizeof g->vp + sizeof g->vp_slack)) return 0;
    return rd32((const uint8_t*)g->vp + off);
}

static uint8_t vp_read8(const TM2Gpu* g, int64_t off) {
    if (off < 0 || off >= (int64_t)(sizeof g->vp + sizeof g->vp_slack)) return 0;
    return ((const uint8_t*)g->vp)[off];
}

void gpu_edge_walk(TM2Gpu* g, GpuEdge* e) {
    uint32_t mbase = (e->octant & 7u) << 12 | ((uint32_t)(e->slope >> 4) & 0xfc0u);
    int32_t count = e->col1 - e->col0;
    if (count < 0) count = e->col0 - e->col1;
    count += 1;
    int32_t k = 0;
    int32_t row = e->row0;
    int32_t tile = (row >= 0 && row < GPU_ROWS ? g->row_start[row] : row * GPU_COLS) + e->col0;
    int32_t vrow0 = vp_read32(g, g->r.vp + offsetof(GpuViewport, row0));
    int32_t vrow1 = vp_read32(g, g->r.vp + offsetof(GpuViewport, row1));
    for (;;) {
        int32_t v = (int32_t)((uint32_t)(e->row1 - row) | (uint32_t)(count - k));
        if (v < 0) break;
        int64_t ro = (int64_t)g->r.vp_rows + 16 * (int64_t)row;
        int32_t rhi = vp_read32(g, ro), rlo = vp_read32(g, ro + 4);
        /* ORIGINAL QUIRK: the row range test uses the loop test value v, not the tile, so it
         * practically never rejects; the enable map below does the clipping. */
        int32_t t = (int32_t)(((uint32_t)rhi - (uint32_t)v) | ((uint32_t)v - (uint32_t)rlo) |
                              ((uint32_t)vrow1 - (uint32_t)row) | ((uint32_t)row - (uint32_t)vrow0));
        if (t < 0) {
            if (g->r.row_min > row) g->r.row_min = row;
            if (row > g->r.row_max) g->r.row_max = row;
            if (vp_read8(g, (int64_t)g->r.vp_enable + tile) != 0 && tile >= 0 && tile < GPU_TILES) {
                uint32_t mi = ((uint32_t)(e->err >> 19) | mbase) & (GPU_MASKS - 1);
                g->tile[tile].cover[0] &= g->mask[mi][0];
                g->tile[tile].cover[1] &= g->mask[mi][1];
                g->r.hit = 0xff;
            }
            if (row >= 0 && row < GPU_ROWS) {
                GpuSpan* s = &g->span[row];
                if (e->up) {
                    if (s->lo_stamp != g->r.stamp || tile < s->lo) {
                        s->lo_stamp = g->r.stamp;
                        s->lo = tile;
                    }
                } else {
                    if (s->hi_stamp != g->r.stamp || tile > s->hi) {
                        s->hi_stamp = g->r.stamp;
                        s->hi = tile;
                    }
                }
            }
        }
        if (tile == e->end_tile) break;
        if (e->err > 0x1000000) {
            e->err = (int32_t)((uint32_t)e->err - 0x1000000u);
            tile += e->minor_tile;
            k += e->major_drow;
            row += e->major_dcol;
        } else {
            e->err = (int32_t)((uint32_t)e->err + (uint32_t)e->err_step);
            tile += e->major_tile;
            k += e->major_dcol;
            row += e->major_drow;
        }
    }
    if (g->r.row_min < vrow0) g->r.row_min = vrow0;
    if (vrow1 < g->r.row_max) g->r.row_max = vrow1;
}
