/*
 * gpu_tex.c: textured primitives, ported from TM2.EXE (see gpu.h, docs/RENDERER.md).
 *   the setups of codes 0x24 0x2c 0x74 0x7c 0x90 inside command_dispatch 0x004ad874,
 *   0x004af7e8 textured collect, 0x004c8820 textured kernel (the plain one: no MMX, 0x004e861c 0).
 *
 * Texture coordinates are .12 fixed point; v is pre shifted by log2(page width), so the texel is
 * pixels[((u & umask12) + (v & vmask12)) >> 12] (8 bit index) and the color clut[index] (16 bit).
 * The collect writes u, v at the corner of every listed tile and of its right and lower
 * neighbours; the kernel interpolates linearly inside the tile from those three corners.
 */
#include "gpu3d.h"
#include "gpu.h"

#include <string.h>

static int16_t i16at(const uint8_t* p, int o) { int16_t v; memcpy(&v, p + o, 2); return v; }
static uint16_t u16at(const uint8_t* p, int o) { uint16_t v; memcpy(&v, p + o, 2); return v; }
static int32_t i32at(const uint8_t* p, int o) { int32_t v; memcpy(&v, p + o, 4); return v; }
static int32_t sub(int32_t v) { return (int32_t)(((uint32_t)v << 5) + 0x200u); }
static int32_t shl(int32_t v, int32_t n) { return (int32_t)((uint32_t)v << (n & 31)); }
static int32_t mul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
/* idiv: the original faults on a zero divisor (and on INT_MIN / -1); the port gives 0 / wraps */
static int32_t idiv(int32_t a, int32_t b) {
    if (b == 0) return 0;
    if (b == -1) return (int32_t)(0u - (uint32_t)a);
    return a / b;
}

static uint8_t tex8(const TM2Gpu* g, uint32_t addr) {
    uint64_t o = (uint64_t)(uint32_t)(addr - g->tex_addr);
    return o < g->tex_size ? g->tex_mem[o] : 0;
}

static uint16_t tex16(const TM2Gpu* g, uint32_t addr) {
    return (uint16_t)(tex8(g, addr) | tex8(g, addr + 1) << 8);
}

/* page lookup + the derived texture globals shared by every textured code */
static void tex_page(TM2Gpu* g, uint32_t word, int32_t clut_add) {
    uint32_t p = word & 0xfff;
    TpcPage z = {0};
    /* ORIGINAL: the index is 12 bits but the table has 512 pages; the port reads 0 beyond */
    const TpcPage* t = p < TPC_MAX_PAGES ? &g->pages[p] : &z;
    g->r.tex_pixels = t->pixels;
    g->r.tex_clut = t->clut + (uint32_t)clut_add;
    g->r.tex_w = t->width;
    g->r.tex_h = t->height;
    int32_t lg = 8;
    if (t->width != 0) {
        lg = 3;
        for (uint32_t w = t->width >> 4; w != 0; w >>= 1) lg++;
    }
    g->r.tex_shift = lg;
    g->r.tex_log_a = lg;
    g->r.umask = (int32_t)t->width - 1;
    g->r.vmask = shl((int32_t)t->height - 1, lg);
    g->r.umask12 = mul(g->r.umask, 0x1000);
    g->r.tex_log_b = lg;
    g->r.vmask12 = shl(g->r.vmask, 12);
}

static void steps8(TM2Gpu* g) {
    g->r.du8x = shl(g->r.du_dx, 3);
    g->r.du8y = shl(g->r.du_dy, 3);
    g->r.dv8x = shl(g->r.dv_dx, 3);
    g->r.dv8y = shl(g->r.dv_dy, 3);
}

/* affine gradients of a textured triangle from integer screen points and texel deltas
 * (0x24 / 0x2c): base vertex b, deltas a = vertex 1 - b, c = vertex 0 - b */
static void tri_gradients(TM2Gpu* g, int32_t dx0, int32_t dy0, int32_t dx1, int32_t dy1,
                          int32_t du0, int32_t du1, int32_t dv0, int32_t dv1, int32_t lg) {
    int32_t det = mul(dx1, dy0) - mul(dx0, dy1);
    if (det == 0) {
        g->r.dv_dx = g->r.dv_dy = g->r.du_dy = g->r.du_dx = 0;
        return;
    }
    g->r.du_dx = idiv(mul(mul(du1, dy0) - mul(du0, dy1), 0x1000), det);
    g->r.dv_dx = shl(idiv(mul(mul(dy0, dv1) - mul(dy1, dv0), 0x1000), det), lg);
    g->r.du_dy = idiv(mul(mul(du0, dx1) - mul(du1, dx0), 0x1000), det);
    g->r.dv_dy = shl(idiv(mul(mul(dx1, dv0) - mul(dx0, dv1), 0x1000), det), lg);
}

static void walk(TM2Gpu* g, int32_t (*v)[2], int n) {
    for (int i = 0; i < n; i++) gpu_edge_setup(GPU_EDGE(g, i), v[i], v[(i + 1) % n]);
    for (int i = 0; i < n; i++) gpu_edge_walk(g, GPU_EDGE(g, i));
}

void gpu_tex_prim(TM2Gpu* g, const uint8_t* p, uint8_t code) {
    g->r.row_min = 1000;
    g->r.row_max = 0;
    if (code == 0x24 || code == 0x2c) {
        /* 0x24: vertices +8 / +0x10 / +0x18, uv +0xc / +0x14 / +0x1c, page +0x20.
         * 0x2c: the same with the third vertex at +0x20 (uv +0x24), page +0x28: ORIGINAL QUIRK,
         * a textured quad is drawn as the triangle of its vertices 0, 1, 3 (vertex 2 unused). */
        int c = code == 0x24 ? 0x18 : 0x20;
        int32_t v[3][2] = {{sub(i16at(p, 8)), sub(i16at(p, 10))}, {sub(i16at(p, 16)), sub(i16at(p, 18))},
                           {sub(i16at(p, c)), sub(i16at(p, c + 2))}};
        tex_page(g, (uint32_t)i32at(p, code == 0x24 ? 0x20 : 0x28), 0);
        g->r.render_flags = 9;
        int32_t lg = g->r.tex_shift;
        g->r.u0 = (int32_t)((uint32_t)p[0xc] << 12);
        g->r.v0 = shl(p[0xd], lg + 12);
        g->r.org_x = v[0][0] >> 5;
        g->r.org_y = v[0][1] >> 5;
        int32_t xb = i16at(p, c), yb = i16at(p, c + 2);
        tri_gradients(g, i16at(p, 8) - xb, i16at(p, 10) - yb, i16at(p, 16) - xb, i16at(p, 18) - yb,
                      p[0xc] - p[c + 4], p[0x14] - p[c + 4], p[0xd] - p[c + 5], p[0x15] - p[c + 5], lg);
        steps8(g);
        g->r.tex_shift = 12 - lg;
        walk(g, v, 3);
        if (g->r.hit) gpu_tex_collect(g);
        return;
    }
    if (code == 0x74 || code == 0x7c) {
        /* sprites: +8 i16 x, +0xa i16 y, +0xc u16 w, +0xe u16 h; 0x74 page +0x10 (texel =
         * pixel from (0, 0)); 0x7c u0 +0x10, v0 +0x11, u1 +0x12, v1 +0x13, page +0x14 */
        int32_t x = i16at(p, 8), y = i16at(p, 10), w = u16at(p, 12), h = u16at(p, 14);
        int32_t x0 = sub(x), y0 = sub(y), x1 = sub(x + w), y1 = sub(y + h);
        g->r.org_x = x + 16;
        g->r.org_y = y + 16;
        tex_page(g, (uint32_t)i32at(p, code == 0x74 ? 0x10 : 0x14), 0);
        g->r.render_flags = 9;
        int32_t lg = g->r.tex_shift;
        g->r.du_dy = 0;
        g->r.dv_dx = 0;
        if (code == 0x74) {
            g->r.u0 = 0;
            g->r.v0 = 0;
            g->r.du_dx = 0x1000;
            g->r.dv_dy = shl(1, lg + 12);
        } else {
            g->r.u0 = (int32_t)((uint32_t)p[0x10] << 12);
            g->r.v0 = shl(p[0x11], lg + 12);
            /* ORIGINAL BUG: w or h = 0 divides by zero (a CPU fault); the port gives 0 */
            g->r.du_dx = idiv(mul(p[0x12] - p[0x10], 0x1000), w);
            g->r.dv_dy = shl(idiv(mul(p[0x13] - p[0x11], 0x1000), h), lg);
        }
        steps8(g);
        g->r.tex_shift = 12 - lg;
        int32_t v[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
        walk(g, v, 4);
        if (g->r.hit) gpu_tex_collect(g);
        return;
    }
    /* 0x90: +4 u16 flags, +6 u8 n, +8 i32 palette row, +0xc..+0x20 i32 u, v at three points
     * (.12), +0x24..+0x38 those points (edge units), +0x44 page, +0x48 n x (i32 x, i32 y) */
    int n = p[6];
    if (n > GPU_EDGES - 1) n = GPU_EDGES - 1;   /* the original runs into the tile grid */
    int32_t v[GPU_EDGES][2];
    for (int i = 0; i < n; i++) { v[i][0] = i32at(p, 0x48 + 8 * i); v[i][1] = i32at(p, 0x4c + 8 * i); }
    for (int i = 0; i < n - 1; i++) gpu_edge_setup(GPU_EDGE(g, i), v[i], v[i + 1]);
    if (n > 0) {
        gpu_edge_setup(GPU_EDGE(g, n - 1), v[n - 1], v[0]);
    } else {
        int32_t a[2] = {i32at(p, 0x40), i32at(p, 0x44)}, b[2] = {i32at(p, 0x48), i32at(p, 0x4c)};
        gpu_edge_setup(GPU_EDGE(g, -1), a, b);
    }
    for (int i = 0; i < n; i++) gpu_edge_walk(g, GPU_EDGE(g, i));
    if (!g->r.hit) return;
    uint16_t flags = u16at(p, 4);
    g->r.render_flags = flags;
    int32_t add = (g->r.fog != 0 && (flags & 0x40) == 0) ? mul(i32at(p, 8), 2) : 0;
    tex_page(g, (uint32_t)i32at(p, 0x44), add);
    int32_t lg = g->r.tex_shift;
    g->r.u0 = i32at(p, 0xc);
    g->r.v0 = shl(i32at(p, 0x10), lg);
    g->r.org_x = i32at(p, 0x24) >> 5;
    g->r.org_y = i32at(p, 0x28) >> 5;
    int32_t du1 = i32at(p, 0x14) - i32at(p, 0x1c), du0 = i32at(p, 0xc) - i32at(p, 0x1c);
    int32_t dv1 = i32at(p, 0x18) - i32at(p, 0x20), dv0 = i32at(p, 0x10) - i32at(p, 0x20);
    int32_t dx1 = (i32at(p, 0x2c) - i32at(p, 0x34)) >> 5, dx0 = (i32at(p, 0x24) - i32at(p, 0x34)) >> 5;
    int32_t dy0 = (i32at(p, 0x28) - i32at(p, 0x38)) >> 5, dy1 = (i32at(p, 0x30) - i32at(p, 0x38)) >> 5;
    int32_t det = mul(dx1, dy0) - mul(dx0, dy1);
    if (det == 0) {
        g->r.du_dy = g->r.dv_dy = g->r.dv_dx = g->r.du_dx = 0;
    } else {
        g->r.du_dx = idiv(mul(du1, dy0) - mul(du0, dy1), det);
        g->r.dv_dx = shl(idiv(mul(dv1, dy0) - mul(dy1, dv0), det), lg);
        g->r.du_dy = idiv(mul(du0, dx1) - mul(du1, dx0), det);
        g->r.dv_dy = shl(idiv(mul(dv0, dx1) - mul(dv1, dx0), det), lg);
    }
    steps8(g);
    g->r.tex_shift = 12 - lg;
    gpu_tex_collect(g);
}

static int32_t vp32(const TM2Gpu* g, int64_t off) {
    int32_t v = 0;
    if (off >= 0 && off + 4 <= (int64_t)(sizeof g->vp + sizeof g->vp_slack))
        memcpy(&v, (const uint8_t*)g->vp + off, 4);
    return v;
}

static GpuTile* tile_at(TM2Gpu* g, int32_t i) {
    return (i >= 0 && i < GPU_TILES) ? &g->tile[i] : NULL;
}

void gpu_tex_collect(TM2Gpu* g) {
    for (int32_t row = g->r.row_min; row <= g->r.row_max; row++) {
        int64_t vo = (int64_t)g->r.vp + offsetof(GpuViewport, rows) + 16 * (int64_t)row;
        int32_t vhi = vp32(g, vo), vlo = vp32(g, vo + 4);
        GpuSpan* s = (row >= 0 && row < GPU_ROWS) ? &g->span[row] : NULL;
        if (s && s->hi_stamp == g->r.stamp && s->lo_stamp == g->r.stamp) {
            if (vhi - 1 < s->hi) s->hi = vhi - 1;
            if (vlo + 1 > s->lo) s->lo = vlo + 1;
            int32_t ry = row * 8 - g->r.org_y;
            int32_t cx = (s->lo - g->row_start[row]) * 8 - g->r.org_x;
            int32_t u = mul(g->r.du_dy, ry) + mul(g->r.du_dx, cx) + g->r.u0;
            int32_t v = mul(ry, g->r.dv_dy) + mul(cx, g->r.dv_dx) + g->r.v0;
            for (int32_t i = s->lo; i <= s->hi; i++) {
                int32_t nu = u + g->r.du8x, nv = v + g->r.dv8x;
                GpuTile* t = tile_at(g, i);
                if (t) {
                    t->draw[0] = t->open[0] & t->cover[0];
                    t->draw[1] = t->open[1] & t->cover[1];
                    if (t->draw[0] | t->draw[1]) {
                        g->list[g->r.ntiles++] = (uint32_t)i;
                        t->u = u; t->v = v; t->stamp = g->r.stamp; t->clut = (int32_t)g->r.tex_clut;
                        GpuTile* b = tile_at(g, i + GPU_COLS);
                        if (b) {
                            b->stamp = g->r.stamp;
                            b->u = u + g->r.du8y;
                            b->v = v + g->r.dv8y;
                            b->clut = (int32_t)g->r.tex_clut;
                        }
                        GpuTile* r = tile_at(g, i + 1);
                        if (r) {
                            r->stamp = g->r.stamp;
                            r->u = nu;
                            r->v = nv;
                            r->clut = (int32_t)g->r.tex_clut;
                        }
                    } else {
                        t->cover[0] = t->cover[1] = 0xffffffffu;
                    }
                }
                u = nu;
                v = nv;
            }
        } else {
            for (int32_t i = vlo + 1; i <= vhi - 1; i++) {
                GpuTile* t = tile_at(g, i);
                if (!t) continue;
                t->cover[1] = 0xffffffffu;
                t->f28 = t->f2c = 0;
                t->cover[0] = 0xffffffffu;
            }
        }
    }
    gpu_tex_fill(g);
}

static void put16(TM2Gpu* g, int64_t off, uint16_t v) {
    if (off < 0 || (uint64_t)off + 2 > g->screen_bytes) return;
    memcpy(g->screen + off, &v, 2);
}

/*
 * 0x004c8820 (plain) and 0x004c71f0 (MMX, picked by 0x004e85c4). Per listed tile and half (4
 * pixel rows): pixels of the draw mask get the palette color of their texel. With render flag 8
 * (all codes but 0x90 set it) some texels are see through; their pixels are not written and stay
 * open (open &= ~cover | see through). The two kernels disagree on what is see through:
 *   plain, rows whose 8 pixels are all drawn: texel index 0xff;
 *   plain, other rows: palette color 0x0000;
 *   MMX: texel index 0xff everywhere.
 * ORIGINAL BUG (both): a see through pixel alone in its pixel pair gives back its neighbour too,
 * which this primitive did not draw (it may hold an earlier primitive's pixel, which can then be
 * drawn over again). Plain: either pixel of the pair; MMX: only when the right one is see through.
 * The MMX kernel also forms the texel offset as (u & umask12) >> 12 + (v & vmask12) >> 12 with
 * arithmetic shifts (psrad), the plain one as a logical shift of the sum: the same for pages
 * of width 1..0x80000.
 * Afterwards cover is reset to ~0. The 0x004e861c kernels (0x004c7c70, 0x004c6590) are not
 * ported: the port draws as above for them.
 */
void gpu_tex_fill(TM2Gpu* g) {
    /* low detail mode (0x004e861c = 1): the original's collects call the low detail kernels
     * 0x004c7c70 / 0x004c6590 instead (src/render/gpu3d_lowdetail.c) */
    if (g->r.tex_mode != 0) { gpu3d_lowdetail_fill(g); return; }
    int64_t pitch = g->r.pitch_bytes;
    uint32_t um = (uint32_t)g->r.umask12, vm = (uint32_t)g->r.vmask12, px = g->r.tex_pixels;
    int see = (g->r.render_flags & 8) != 0;
    int mmx = g->r.mmx != 0;
    for (int32_t n = 0; n < g->r.ntiles; n++) {
        int32_t ti = (int32_t)g->list[n];
        GpuTile* t = &g->tile[ti];
        const GpuTile* rt = tile_at(g, ti + 1);
        const GpuTile* bt = tile_at(g, ti + GPU_COLS);
        int32_t u0 = t->u, v0 = t->v;
        int32_t dux = ((rt ? rt->u : 0) - u0) >> 3, dvx = ((rt ? rt->v : 0) - v0) >> 3;
        int32_t duy = ((bt ? bt->u : 0) - u0) >> 3, dvy = ((bt ? bt->v : 0) - v0) >> 3;
        uint32_t clut = (uint32_t)t->clut;
        for (int half = 0; half < 2; half++) {
            uint32_t m = t->draw[half], keep = ~t->cover[half];
            for (int r = 0; m && r < 4; r++) {
                uint32_t rb = m >> (8 * r) & 0xff;
                if (!rb) continue;
                int32_t y = half * 4 + r;
                uint32_t ur = (uint32_t)u0 + (uint32_t)y * (uint32_t)duy;
                uint32_t vr = (uint32_t)v0 + (uint32_t)y * (uint32_t)dvy;
                int64_t row = (int64_t)t->screen + y * pitch;
#define UU(c) ((ur + (uint32_t)(c) * (uint32_t)dux) & um)
#define VV(c) ((vr + (uint32_t)(c) * (uint32_t)dvx) & vm)
#define TEXEL(c) tex8(g, mmx ? px + (uint32_t)(((int32_t)UU(c) >> 12) + ((int32_t)VV(c) >> 12)) \
                             : px + ((UU(c) + VV(c)) >> 12))
#define COLOR(i) tex16(g, clut + 2u * (i))
                if (rb == 0xff) {
                    for (int c = 0; c < 8; c++) {
                        uint8_t i = TEXEL(c);
                        if (see && i == 0xff) keep |= 1u << (8 * r + c);
                        else put16(g, row + 2 * c, COLOR(i));
                    }
                    continue;
                }
                for (int k = 0; k < 4; k++) {
                    uint32_t b = rb >> (2 * k) & 3, s = 8 * r + 2 * k;
                    int c = 2 * k;
                    if (!b) continue;
                    if (!see) {
                        if (b & 1) put16(g, row + 2 * c, COLOR(TEXEL(c)));
                        if (b & 2) put16(g, row + 2 * c + 2, COLOR(TEXEL(c + 1)));
                        continue;
                    }
                    if (mmx) {
                        if (b == 3) {
                            uint8_t i0 = TEXEL(c), i1 = TEXEL(c + 1);
                            if (i0 != 0xff) put16(g, row + 2 * c, COLOR(i0));
                            else keep |= 1u << s;
                            if (i1 != 0xff) put16(g, row + 2 * c + 2, COLOR(i1));
                            else keep |= 2u << s;
                        } else if (b == 1) {
                            uint8_t i = TEXEL(c);
                            if (i != 0xff) put16(g, row + 2 * c, COLOR(i));
                            else keep |= 1u << s;
                        } else {
                            uint8_t i = TEXEL(c + 1);
                            if (i != 0xff) put16(g, row + 2 * c + 2, COLOR(i));
                            else keep |= 3u << s;
                        }
                        continue;
                    }
                    if (b == 3) {
                        uint16_t c0 = COLOR(TEXEL(c)), c1 = COLOR(TEXEL(c + 1));
                        if (c0 == 0) {
                            if (c1 == 0) keep |= 3u << s;
                            else { put16(g, row + 2 * c + 2, c1); keep |= 1u << s; }
                        } else if (c1 == 0) {
                            put16(g, row + 2 * c, c0);
                            keep |= 2u << s;
                        } else {
                            put16(g, row + 2 * c, c0);
                            put16(g, row + 2 * c + 2, c1);
                        }
                    } else {
                        int cc = b == 1 ? c : c + 1;
                        uint16_t col = COLOR(TEXEL(cc));
                        if (col) put16(g, row + 2 * cc, col);
                        else keep |= 3u << s;
                    }
                }
#undef TEXEL
#undef COLOR
#undef UU
#undef VV
            }
            t->open[half] &= keep;
        }
        t->cover[0] = t->cover[1] = 0xffffffffu;
    }
}
