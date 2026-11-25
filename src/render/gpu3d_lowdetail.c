/*
 * gpu3d_lowdetail.c: the low detail textured kernels of TM2.EXE's software GPU, ported from the
 * machine code (hand written assembly):
 *   0x004c7c70  used when 0x004e85c4 (MMX) = 0 and 0x004e861c (texture detail flag) = 1
 *   0x004c6590  used when 0x004e85c4 != 0 and 0x004e861c = 1 (MMX, ends in emms)
 * Called by every textured collect (0x004af7e8, and the 0xa0 collects 0x004afadc, 0x004b029c,
 * 0x004b07ec) in place of 0x004c8820 / 0x004c71f0 (gpu_tex_fill in gpu_tex.c).
 *
 * What they do: exactly what the full detail kernel of the same family does (0x004c7c70 as
 * 0x004c8820, 0x004c6590 as 0x004c71f0: texel addressing, the see through rules, the pixel pair
 * bugs, the open / cover bookkeeping), except for rows whose 8 pixels are all drawn, when render
 * flag 8 (see through texels) is clear: such a row at an even pixel row y of the tile (0, 2, 4,
 * 6) is written to rows y and y + 1 with the texels of row y, and row y + 1 is then skipped (its
 * own draw mask is not looked at). So in fully covered, opaque areas the texture has half
 * vertical resolution and the texel fetch work is halved. Fully drawn odd rows (reached when row
 * y - 1 was not fully drawn) are written normally; rows with see through texels never double.
 * 0x004c7c70 picks the doubling by bit 4 of its row counter (ch = 0x42 - 0x10 * row), 0x004c6590
 * by the row mask & 0x00ff00ff; both mean rows 0 and 2 of each 4 row half.
 *
 * ORIGINAL BUG: the doubled row y + 1 is written whatever its draw mask: pixels of an earlier
 * (nearer) primitive that already took those pixels get overwritten, and pixels just outside
 * the primitive (below its lower edge) get this primitive's texels (they stay open, so a farther
 * primitive drawn later covers them again). Only the cover bits of row y + 1 leave the open
 * mask, as in the full detail kernels.
 *
 * Texel: 0x004c7c70 pixels[((u & umask12) + (v & vmask12)) >> 12] (logical shift of the sum),
 * 0x004c6590 pixels[((u & umask12) >> 12) + ((v & vmask12) >> 12)] (psrad, arithmetic), as their
 * full detail siblings. u, v of pixel (x, y) = corner + y * ((lower - corner) >> 3) +
 * x * ((right - corner) >> 3), 32 bit wrap.
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

static const GpuTile* tile_at(const TM2Gpu* g, int32_t i) {
    return (i >= 0 && i < GPU_TILES) ? &g->tile[i] : NULL;
}

void gpu3d_lowdetail_fill(TM2Gpu* g) {
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
                    if (!see && (r & 1) == 0) {
                        /* low detail: this row's texels into rows y and y + 1 */
                        for (int c = 0; c < 8; c++) {
                            uint16_t col = COLOR(TEXEL(c));
                            put16(g, row + 2 * c, col);
                            put16(g, row + pitch + 2 * c, col);
                        }
                        r++;
                        continue;
                    }
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
                        /* index 0xff; a see through right pixel alone also gives back the left */
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
                    /* non MMX partial rows: palette color 0 is see through; a lone pixel gives
                     * back its neighbour too */
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
