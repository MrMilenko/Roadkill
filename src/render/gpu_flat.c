/*
 * gpu_flat.c: flat (one color) primitives, ported from TM2.EXE (see gpu.h, docs/RENDERER.md).
 *   0x0049653c / 0x00496590 color conversion, 0x004af694 flat collect, 0x004c6400 / 0x004c6280
 *   flat fill (plain / MMX), 0x004ad030 / 0x004acfe0 pixel open bit, 0x004b17bc / 0x004b1934 lines.
 */
#include "gpu.h"

#include <string.h>

uint32_t gpu_rgb24_to_16(uint8_t format, uint32_t rgb) {
    uint32_t b = rgb >> 3 & 0x1f;
    if (format == 0) return (rgb >> 9 & 0x7c00) | b | (rgb >> 6 & 0x3e0);
    if (format == 1) return (rgb >> 8 & 0xf800) | b | (rgb >> 5 & 0x7e0);
    return 0;
}

uint32_t gpu_rgb_to_16(uint8_t format, uint8_t r, uint8_t g, uint8_t b) {
    if (format == 0) return (uint32_t)(b >> 3) | (uint32_t)(r & 0xf8) << 7 | (uint32_t)(g & 0xf8) << 2;
    if (format == 1) return (uint32_t)(b >> 3) | (uint32_t)(r & 0xf8) << 8 | (uint32_t)(g & 0xfc) << 3;
    return 0;
}

static void put16(TM2Gpu* g, int64_t off, uint16_t v) {
    if (off < 0 || (uint64_t)off + 2 > g->screen_bytes) return;
    memcpy(g->screen + off, &v, 2);
}

static void put32(TM2Gpu* g, int64_t off, uint32_t v) {
    if (off < 0 || (uint64_t)off + 4 > g->screen_bytes) return;
    memcpy(g->screen + off, &v, 4);
}

static int32_t vp32(const TM2Gpu* g, int64_t off) {
    int32_t v = 0;
    if (off >= 0 && off + 4 <= (int64_t)(sizeof g->vp + sizeof g->vp_slack))
        memcpy(&v, (const uint8_t*)g->vp + off, 4);
    return v;
}

/*
 * 0x004c6400 (plain) and 0x004c6280 (MMX): every listed tile, its draw mask in two halves of
 * four pixel rows. A whole half (mask 0xffffffff) is written as 4 rows of 16 bytes; otherwise
 * per row, per pixel pair: both bits = one 32 bit store, one bit = a 16 bit store of the low
 * color word. The plain kernel's 32 bit stores use color32 (0x00c846c8), the MMX kernel's use
 * color64[0] and its whole rows the 64 bit color64 (0x00c84668). The flat primitives always set
 * all three to the same pixel repeated, so both kernels give the same pixels.
 */
void gpu_flat_fill(TM2Gpu* g) {
    int64_t pitch = g->r.pitch_bytes;
    uint32_t c32 = g->r.mmx ? g->r.color64[0] : g->r.color32;
    uint32_t q0 = g->r.mmx ? g->r.color64[0] : g->r.color32;
    uint32_t q1 = g->r.mmx ? g->r.color64[1] : g->r.color32;
    uint16_t c16 = (uint16_t)c32;
    for (int32_t n = 0; n < g->r.ntiles; n++) {
        const GpuTile* t = &g->tile[g->list[n]];
        int64_t p = t->screen;
        for (int half = 0; half < 2; half++) {
            uint32_t m = t->draw[half];
            if (m == 0) { p += 4 * pitch; continue; }
            if (m == 0xffffffffu) {
                for (int r = 0; r < 4; r++, p += pitch) {
                    put32(g, p, q0); put32(g, p + 4, q1);
                    put32(g, p + 8, q0); put32(g, p + 12, q1);
                }
                continue;
            }
            for (int r = 0; r < 4; r++, m >>= 8, p += pitch) {
                uint32_t b = m & 0xff;
                for (int pair = 0; pair < 4; pair++) {
                    uint32_t two = b >> (2 * pair) & 3;
                    if (two == 3) put32(g, p + 4 * pair, c32);
                    else {
                        if (two & 1) put16(g, p + 4 * pair, c16);
                        if (two & 2) put16(g, p + 4 * pair + 2, c16);
                    }
                }
            }
        }
    }
}

/*
 * 0x004af694. For each tile row the edges touched: when both ends were set by this primitive,
 * clamps them to the viewport row range and moves every tile between them from cover to draw
 * (draw = open & cover, open &= ~cover, cover = ~0), listing tiles with something to draw.
 * Otherwise only resets cover across the viewport row.
 * ORIGINAL BUG: in that second case the row pointers (span and viewport row range) advance by
 * two rows while the row counter advances by one, so the rows after it use the data of the rows
 * below them. Kept. (The textured collect 0x004af7e8 does not have it.)
 */
void gpu_flat_collect(TM2Gpu* g) {
    int32_t n = g->r.ntiles;
    int32_t prow = g->r.row_min;      /* row the (drifting) pointers are at */
    for (int32_t row = g->r.row_min; row <= g->r.row_max; row++) {
        int64_t vo = (int64_t)g->r.vp + offsetof(GpuViewport, rows) + 16 * (int64_t)prow;
        int32_t vhi = vp32(g, vo), vlo = vp32(g, vo + 4);
        GpuSpan* s = (prow >= 0 && prow < GPU_ROWS) ? &g->span[prow] : NULL;
        if (s && s->hi_stamp == g->r.stamp && s->lo_stamp == g->r.stamp) {
            if (vhi - 1 < s->hi) s->hi = vhi - 1;
            if (vlo + 1 > s->lo) s->lo = vlo + 1;
            for (int32_t i = s->lo; i <= s->hi; i++) {
                if (i < 0 || i >= GPU_TILES) continue;
                GpuTile* t = &g->tile[i];
                t->draw[0] = t->open[0] & t->cover[0];
                t->draw[1] = t->open[1] & t->cover[1];
                t->open[0] &= ~t->cover[0];
                t->open[1] &= ~t->cover[1];
                t->cover[0] = t->cover[1] = 0xffffffffu;
                if (t->draw[0] | t->draw[1]) g->list[n++] = (uint32_t)i;
            }
            prow += 1;
        } else {
            for (int32_t i = vlo + 1; i <= vhi - 1; i++) {
                if (i < 0 || i >= GPU_TILES) continue;
                g->tile[i].cover[0] = g->tile[i].cover[1] = 0xffffffffu;
            }
            prow += 2;
        }
    }
    g->r.ntiles = n;
    gpu_flat_fill(g);
}

static int32_t open_tile(const TM2Gpu* g, int32_t x, int32_t y) {
    int32_t r = (y >> 3) + 2;
    int32_t rs = (r >= 0 && r < GPU_ROWS) ? g->row_start[r] : r * GPU_COLS;
    return (x >> 3) + rs + 2;
}

uint32_t gpu_pixel_open(const TM2Gpu* g, int32_t x, int32_t y) {
    int32_t t = open_tile(g, x, y);
    if (t < 0 || t >= GPU_TILES) return 0;
    uint8_t b = (uint8_t)(g->tile[t].open[(y & 4) ? 1 : 0] >> (8 * (y & 3)));
    return b & (1u << (x & 7));
}

void gpu_pixel_take(TM2Gpu* g, int32_t x, int32_t y) {
    int32_t t = open_tile(g, x, y);
    if (t < 0 || t >= GPU_TILES) return;
    g->tile[t].open[(y & 4) ? 1 : 0] &= ~((1u << (x & 7)) << (8 * (y & 3)));
}

static int64_t px_off(const TM2Gpu* g, int32_t x, int32_t y) {
    int32_t half = g->pitch / 2;
    return (int64_t)(int32_t)((uint32_t)half * (uint32_t)y * 2u + (uint32_t)x * 2u);
}

/* Bresenham as in the original: error starts at minor - major / 2, steps on > 0 */
static void line(TM2Gpu* g, int32_t x, int32_t y, int32_t x1, int32_t y1, int clip) {
    int32_t dx = x1 - x, dy = y1 - y;
    int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int32_t sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    int32_t vp = g->r.vp;
    for (int xmajor = ay < ax, e = xmajor ? ay - (ax >> 1) : ax - (ay >> 1);;) {
        int ok = 1;
        if (clip) {
            ok = x >= vp32(g, vp + offsetof(GpuViewport, x0)) && x <= vp32(g, vp + offsetof(GpuViewport, x1)) &&
                 y >= vp32(g, vp + offsetof(GpuViewport, y0)) && y <= vp32(g, vp + offsetof(GpuViewport, y1)) &&
                 gpu_pixel_open(g, x, y) != 0;
        }
        if (ok) {
            put16(g, px_off(g, x, y), g->r.color16);
            if (clip) gpu_pixel_take(g, x, y);
        }
        if (xmajor) {
            if (x == x1) break;
            if (e > 0) { e -= ax; y += sy; }
            e += ay;
            x += sx;
        } else {
            if (y == y1) break;
            if (e > 0) { e -= ay; x += sx; }
            e += ax;
            y += sy;
        }
    }
}

void gpu_line(TM2Gpu* g, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    line(g, x0, y0, x1, y1, 1);
}

void gpu_line_raw(TM2Gpu* g, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    line(g, x0, y0, x1, y1, 0);
}
