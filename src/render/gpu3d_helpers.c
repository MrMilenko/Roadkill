/*
 * gpu3d_helpers.c: the integer helpers of the 3D (0xa0) path of TM2.EXE's software GPU, ported
 * from the machine code (see gpu3d.h, docs/RENDERER_3D.md):
 *   0x004b0cb4 gpu3d_fog_fill      textured kernel with per pixel fog palette rows (dithered)
 *   0x004b10ec gpu3d_overlay_fill  draws the overlay list at half intensity
 *   0x004b1554 gpu3d_overlay_mark  marks the pixels of a flag 2 primitive as overlay
 *
 * Tile fields used (GpuTile, src/render/gpu.h): +0x00 u, +0x04 v (.12, v pre shifted), +0x08
 * open, +0x10 cover, +0x18 draw, +0x24 screen offset, +0x28 / +0x2c overlay mask (f28 / f2c),
 * +0x30 noise byte offset (into noise_a 0x00bdff50, read as u16 per pixel), +0x34 fog value
 * (f34, 4096..8191; bits 8..12 / 8..13 pick a palette row), +0x38 palette. Each kernel
 * interpolates u, v and fog linearly from the tile corner, its right neighbour (tile + 1) and
 * its lower neighbour (tile + 0x54), per pixel (difference >> 3).
 * A tile is two halves (pixel rows 0..3 = mask word 0, rows 4..7 = word 1); a half with an
 * empty mask is skipped by stepping the row bases by (lower - corner) >> 1.
 * All arithmetic is 32 bit wrapping, shifts of differences arithmetic (sar).
 */
#include "gpu3d.h"

#include <string.h>

static uint8_t tex8(const TM2Gpu* g, uint32_t addr) {
    uint64_t o = (uint64_t)(uint32_t)(addr - g->tex_addr);
    return o < g->tex_size ? g->tex_mem[o] : 0;
}

static uint16_t tex16(const TM2Gpu* g, uint32_t addr) {
    return (uint16_t)(tex8(g, addr) | tex8(g, addr + 1) << 8);
}

static void put16(TM2Gpu* g, int64_t off, uint16_t v) {
    if (off < 0 || (uint64_t)off + 2 > g->screen_bytes) return;
    memcpy(g->screen + off, &v, 2);
}

/* noise_a read as u16 at byte offset o (0x00bdff50 + o); outside the table 0 */
static uint16_t noise16(const TM2Gpu* g, uint32_t o) {
    if (o + 2 > sizeof g->noise_a) return 0;
    uint16_t v;
    memcpy(&v, (const uint8_t*)g->noise_a + o, 2);
    return v;
}

static const GpuTile zero_tile;

static const GpuTile* tile_ro(const TM2Gpu* g, uint32_t i) {
    return i < GPU_TILES ? &g->tile[i] : &zero_tile;
}

static int32_t sar(uint32_t v, int n) { return (int32_t)v >> n; }

/* the gradient globals every helper writes from the tile corners */
static void corner_gradients(TM2Gpu* g, uint32_t i) {
    const GpuTile* t = tile_ro(g, i);
    const GpuTile* r = tile_ro(g, i + 1);
    const GpuTile* d = tile_ro(g, i + GPU_COLS);
    g->r.du8x = (int32_t)((uint32_t)r->u - (uint32_t)t->u);
    g->r.dv8x = (int32_t)((uint32_t)r->v - (uint32_t)t->v);
    g->r.du8y = (int32_t)((uint32_t)d->u - (uint32_t)t->u);
    g->r.dv8y = (int32_t)((uint32_t)d->v - (uint32_t)t->v);
    g->r.du_dx = g->r.du8x >> 3;
    g->r.dv_dx = g->r.dv8x >> 3;
    g->r.du_dy = g->r.du8y >> 3;
    g->r.dv_dy = g->r.dv8y >> 3;
}

/* texel index at (u, v): with fog on (0x004e85f8 set and render flag 0x40 clear) the two
 * masked coordinates are ORed, else added (both kernels; the same for in range values) */
static uint8_t texel(const TM2Gpu* g, uint32_t u, uint32_t v, int fog_path) {
    uint32_t a = (uint32_t)g->r.umask12 & u, b = (uint32_t)g->r.vmask12 & v;
    return tex8(g, g->r.tex_pixels + ((fog_path ? (a | b) : (a + b)) >> 12));
}

/* shared kernel body of 0x004b0cb4 (overlay 0) and 0x004b10ec (overlay 1) */
static void fill_tile(TM2Gpu* g, Gpu3d* s, uint32_t ti, int overlay) {
    GpuTile* t = &g->tile[ti];
    const GpuTile* rt = tile_ro(g, ti + 1);
    const GpuTile* dt = tile_ro(g, ti + GPU_COLS);
    uint32_t mask[2];
    if (overlay) {
        mask[0] = t->open[0] & t->f28 & t->cover[0];
        mask[1] = t->open[1] & t->f2c & t->cover[1];
    } else {
        mask[0] = t->draw[0];
        mask[1] = 0;              /* read from the tile after the first half (it may change) */
    }
    uint32_t f0 = (uint32_t)t->f34;
    corner_gradients(g, ti);
    uint32_t dfy = (uint32_t)dt->f34 - f0;
    s->fog_dy = sar(dfy, 3);
    s->fog_dx = sar((uint32_t)rt->f34 - f0, 3);
    int32_t fog_half = sar(dfy, 1);
    int64_t scr = (int64_t)(t->screen >> 1) * 2;
    uint32_t noise = (uint32_t)t->noise * 2;
    uint32_t clut = (uint32_t)t->clut;
    uint32_t bu = (uint32_t)t->u, bv = (uint32_t)t->v, bf = f0;
    uint32_t cu = bu, cv = bv, cf = bf;
    uint32_t acc = ~t->cover[0];
    int fog_path = g->r.fog != 0 && (g->r.render_flags & 0x40) == 0;
    int64_t pitch2 = (int64_t)g->r.pitch_px * 2, pitch_m8 = (int64_t)g->r.pitch_px_m8 * 2;
    uint32_t mk = mask[0];
    for (int half = 2; half > 0; half--) {
        uint32_t bit = 1, m = 0xff;
        if (mk == 0) {
            if (overlay || half == 2) {
                if (!overlay) {
                    t->open[0] &= acc;
                    t->cover[0] = 0xffffffffu;
                    acc = t->cover[1];
                    mk = t->draw[1];
                }
                scr += (int64_t)g->r.pitch_x4 * 2;
                bu += (uint32_t)(g->r.du8y >> 1);
                bv += (uint32_t)(g->r.dv8y >> 1);
                if (!overlay) acc = ~acc;
                bf += (uint32_t)fog_half;
                cu = bu; cv = bv; cf = bf;
            }
            if (!overlay) continue;
        } else {
            for (int row = 0; row < 4; row++) {
                if (mk & m) {
                    for (int k = 0; k < 8; k++) {
                        if (mk & bit) {
                            uint8_t x = texel(g, cu, cv, fog_path);
                            if (x == 0xff) {
                                acc |= bit;
                            } else {
                                uint32_t base = clut;
                                if (fog_path && s->dither)
                                    base = clut + (((cf - noise16(g, noise)) & (overlay ? 0x3f00u : 0x1f00u)) * 2);
                                uint16_t c = tex16(g, base + 2u * x);
                                if (overlay) c = (uint16_t)((c & g->r.half_mask) >> 1);
                                put16(g, scr, c);
                            }
                        }
                        scr += 2;
                        cu += (uint32_t)g->r.du_dx;
                        cv += (uint32_t)g->r.dv_dx;
                        bit <<= 1;
                        cf += (uint32_t)s->fog_dx;
                        noise += 2;
                    }
                    scr += pitch_m8;
                } else {
                    bit <<= 8;
                    scr += pitch2;
                }
                bu += (uint32_t)g->r.du_dy;
                bv += (uint32_t)g->r.dv_dy;
                bf += (uint32_t)s->fog_dy;
                cu = bu; cv = bv; cf = bf;
                m <<= 8;
            }
            if (!overlay) {
                if (half == 2) {
                    t->open[0] &= acc;
                    t->cover[0] = 0xffffffffu;
                    acc = ~t->cover[1];
                    mk = t->draw[1];
                }
                continue;
            }
        }
        /* overlay: end of a half (also after an empty one) */
        int w = half == 2 ? 0 : 1;
        if ((t->draw[0] | t->draw[1]) == 0) {
            t->cover[w] = 0xffffffffu;
            t->open[w] &= acc;
        } else {
            t->cover[w] &= ~acc;
        }
        if (w == 0) {
            t->f28 &= acc;
            acc = ~t->cover[1];
            mk = mask[1];
        } else {
            t->f2c &= acc;
        }
    }
    if (!overlay) {
        t->cover[1] = 0xffffffffu;
        t->open[1] &= acc;
    }
}

void gpu3d_fog_fill(TM2Gpu* g, Gpu3d* s) {
    for (int32_t i = 0; i < g->r.ntiles; i++) {
        uint32_t ti = g->list[i];
        if (ti < GPU_TILES) fill_tile(g, s, ti, 0);
    }
}

void gpu3d_overlay_fill(TM2Gpu* g, Gpu3d* s) {
    for (int32_t i = 0; i < g->r.f7fc; i++) {
        uint32_t ti = s->list2[i];
        if (ti < GPU_TILES) fill_tile(g, s, ti, 1);
    }
}

void gpu3d_overlay_mark(TM2Gpu* g, Gpu3d* s) {
    (void)s;
    for (int32_t i = 0; i < g->r.ntiles; i++) {
        uint32_t ti = g->list[i];
        if (ti >= GPU_TILES) continue;
        GpuTile* t = &g->tile[ti];
        corner_gradients(g, ti);
        uint32_t bu = (uint32_t)t->u, bv = (uint32_t)t->v;
        uint32_t acc = ~t->cover[0];
        uint32_t mk = t->draw[0];
        for (int half = 2; half > 0; half--) {
            uint32_t bit = 1, m = 0xff;
            if (mk == 0) {
                bu += (uint32_t)(g->r.du8y >> 1);
                bv += (uint32_t)(g->r.dv8y >> 1);
            } else {
                for (int row = 0; row < 4; row++) {
                    if (mk & m) {
                        uint32_t u = bu, v = bv;
                        for (int k = 0; k < 8; k++) {
                            if ((mk & bit) && texel(g, u, v, 0) == 0xff) acc |= bit;
                            u += (uint32_t)g->r.du_dx;
                            v += (uint32_t)g->r.dv_dx;
                            bit <<= 1;
                        }
                    } else {
                        bit <<= 8;
                    }
                    m <<= 8;
                    bu += (uint32_t)g->r.du_dy;
                    bv += (uint32_t)g->r.dv_dy;
                }
            }
            if (half == 2) {
                t->cover[0] = 0xffffffffu;
                t->f28 |= ~acc;
                mk = t->draw[1];
                acc = ~t->cover[1];
            } else {
                t->cover[1] = 0xffffffffu;
                t->f2c |= ~acc;
            }
        }
    }
}
