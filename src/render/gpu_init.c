/*
 * gpu_init.c: renderer setup, ported from TM2.EXE (see gpu.h, docs/RENDERER.md).
 *   0x004ad360 init (minus CPUID), 0x004b1b90 mask table, 0x004b1a6c bit reverse,
 *   0x004b1aec 8x8 transpose, 0x004b2004 noise tables, 0x004ad19c cover reset,
 *   0x004ad760 pitch, 0x004ad3d4 viewport setup (+ 0x004ad1c4 row ranges, 0x004ad714 enable map),
 *   0x004ad5e0 viewport select, 0x004ad084 frame clear, 0x004ad848 frame begin.
 */
#include "gpu.h"

#include <string.h>

/* 0x004b1a6c: the bits of a byte in reverse order */
static uint8_t bit_reverse(uint8_t b) {
    uint8_t r = 0;
    for (int i = 0; i < 8; i++)
        if (b & (1u << i)) r |= (uint8_t)(0x80u >> i);
    return r;
}

static void reverse_rows(uint32_t m[2]) {
    uint32_t o[2] = {0, 0};
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < 4; i++)
            o[k] |= (uint32_t)bit_reverse((uint8_t)(m[k] >> (8 * i))) << (8 * i);
    m[0] = o[0];
    m[1] = o[1];
}

/* 0x004b1aec: transpose of the 8x8 bit matrix (byte = row, bit = column) */
static void transpose(uint32_t m[2]) {
    uint8_t in[8], out[8] = {0};
    for (int i = 0; i < 4; i++) {
        in[i] = (uint8_t)(m[0] >> (8 * i));
        in[i + 4] = (uint8_t)(m[1] >> (8 * i));
    }
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            out[x] |= (uint8_t)(((in[y] >> x) & 1) << y);
    m[0] = m[1] = 0;
    for (int i = 0; i < 4; i++) {
        m[0] |= (uint32_t)out[i] << (8 * i);
        m[1] |= (uint32_t)out[i + 4] << (8 * i);
    }
}

/* rows upside down (the original swaps the 8 bytes end for end) */
static void flip_rows(uint32_t m[2]) {
    uint8_t b[8];
    for (int i = 0; i < 4; i++) {
        b[i] = (uint8_t)(m[1] >> (8 * i));       /* the original's stack order: hi, then lo */
        b[i + 4] = (uint8_t)(m[0] >> (8 * i));
    }
    m[0] = m[1] = 0;
    for (int i = 0; i < 4; i++) {
        m[1] |= (uint32_t)b[7 - i] << (8 * i);
        m[0] |= (uint32_t)b[3 - i] << (8 * i);
    }
}

/*
 * 0x004b1b90. For error s (0..63, quarter pixels) and slope t (0..63), pixel (x, y) of the base
 * mask is set when 4 * y + 2 > trunc(4 * C + c), C = s * 0.25 - (7.5 - x) * (t * k) with
 * k = 0.01562255859375 and c = 0.49999 (the doubles at 0x004e3600 and 0x004e3618).
 * Floating point: the original computes on the x87 (rows 0..3 from the 80 bit C, rows 4..7 from C
 * stored to a double) and truncates with frndint in chop mode (0x004979a2). Plain double
 * arithmetic gives the same table: over all 64 * 64 * 8 values, 4 * C + c is never closer than
 * 6.8e-5 to an integer, far above any precision difference. The emulator diff checks all 0x8000
 * entries.
 * Index bits 12..14 pick a symmetry of the base mask:
 *   0 base, 1 ~mirror, 2 mirror, 3 ~base, 4 ~transpose, 5 mirror(transpose),
 *   6 ~transpose(flip), 7 transpose (mirror = columns reversed, flip = rows reversed).
 */
void gpu_build_masks(uint32_t (*mask)[2]) {
    const double k = 0.01562255859375, c = 0.49999, h = 7.5;
    for (int s = 0; s < 64; s++) {
        double a = s * 0.25;
        for (int t = 0; t < 64; t++) {
            double b = t * k;
            uint8_t lo[4] = {0}, hi[4] = {0};
            for (int y = 0; y < 4; y++) {
                int y_lo = (int)((y + 0.5) * 4.0 + c);           /* 4y + 2 */
                int y_hi = (int)((y + 0.5 + 4.0) * 4.0 + c);     /* 4y + 18 */
                for (int x = 0; x < 8; x++) {
                    double cc = a - (h - x) * b;
                    if (y_lo > (int)(cc * 4.0 + c)) lo[y] |= (uint8_t)(1u << x);
                    if (y_hi > (int)(cc * 4.0 + c)) hi[y] |= (uint8_t)(1u << x);
                }
            }
            int i = t * 64 + s;
            uint32_t m[2] = {0, 0}, w[2];
            for (int y = 0; y < 4; y++) {
                m[0] |= (uint32_t)lo[y] << (8 * y);
                m[1] |= (uint32_t)hi[y] << (8 * y);
            }
            mask[i][0] = m[0]; mask[i][1] = m[1];
            w[0] = m[0]; w[1] = m[1]; reverse_rows(w);
            mask[i | 0x1000][0] = ~w[0]; mask[i | 0x1000][1] = ~w[1];
            mask[i | 0x2000][0] = w[0]; mask[i | 0x2000][1] = w[1];
            mask[i | 0x3000][0] = ~m[0]; mask[i | 0x3000][1] = ~m[1];
            w[0] = m[0]; w[1] = m[1]; transpose(w);
            mask[i | 0x4000][0] = ~w[0]; mask[i | 0x4000][1] = ~w[1];
            mask[i | 0x7000][0] = w[0]; mask[i | 0x7000][1] = w[1];
            reverse_rows(w);
            mask[i | 0x5000][0] = w[0]; mask[i | 0x5000][1] = w[1];
            w[0] = m[0]; w[1] = m[1]; flip_rows(w); transpose(w);
            mask[i | 0x6000][0] = ~w[0]; mask[i | 0x6000][1] = ~w[1];
        }
    }
}

void gpu_init(TM2Gpu* g, int mmx, int (*rand_fn)(void* user), void* rand_user) {
    g->r.mmx = g->r.cpu_mmx = mmx ? -1 : 0;
    gpu_build_masks(g->mask);
    /* 0x004b2004: the first rand() of each entry is thrown away */
    for (int i = 0; i < 0x80; i++) {
        rand_fn(rand_user);
        uint32_t r1 = (uint32_t)(rand_fn(rand_user) >> 7) & 0xffff;
        uint32_t r2 = (uint32_t)(rand_fn(rand_user) >> 7) & 0xffff;
        uint32_t r3 = (uint32_t)(rand_fn(rand_user) >> 7) & 0xffff;
        uint32_t r4 = (uint32_t)(rand_fn(rand_user) >> 7) & 0xffff;
        g->noise_a[i][0] = r1 << 16 | r2;
        g->noise_a[i][1] = r3 << 16 | r4;
        g->noise_b[i] = 0;
    }
    for (int row = 0; row < GPU_ROWS; row++)
        for (int col = 0; col < GPU_COLS; col++)
            g->tile[row * GPU_COLS + col].noise = (int32_t)((uint32_t)(3 * row + 7 * col) & 0xfc);
    for (int row = 0; row < GPU_ROWS; row++)
        g->row_start[row] = row * GPU_COLS;
    /* 0x004ad19c */
    for (int i = 0; i < GPU_TILES; i++) {
        g->tile[i].cover[0] = g->tile[i].cover[1] = 0xffffffffu;
        g->tile[i].stamp = -1;
    }
    g->r.vp = (int32_t)sizeof(GpuViewport);     /* 0x00c813b8 = viewport 1 */
}

void gpu_set_pitch(TM2Gpu* g, int32_t pitch) {
    /* tiles (2..81, 2..61): 80 x 60 tiles = 640 x 480 pixels */
    for (int row = 0; row < 60; row++)
        for (int col = 0; col < 80; col++)
            g->tile[(row + 2) * GPU_COLS + col + 2].screen = (int32_t)((uint32_t)row * (uint32_t)pitch * 8u + (uint32_t)col * 16u);
    int32_t half = pitch / 2;
    g->r.pitch_px = half;
    g->r.pitch_bytes = half * 2;
    g->r.pitch_m16 = half * 2 - 0x10;
    g->r.pitch_q = pitch / 4;
    g->r.pitch_e = pitch / 8;
    g->r.pitch_px_m8 = half - 8;
    g->r.pitch_x4 = (int32_t)((uint32_t)half << 2);
    g->r.pitch_x8 = (int32_t)((uint32_t)half << 3);
}

/* 0x004ad1c4 + 0x004ad714 */
static void vp_tables(GpuViewport* v) {
    for (int row = 0; row < GPU_ROWS; row++) {
        v->rows[row].lo = v->col0 + row * GPU_COLS - 1;
        v->rows[row].hi = v->col1 + row * GPU_COLS + 1;
    }
    for (int row = 0; row < GPU_ROWS; row++)
        for (int col = 0; col < GPU_COLS; col++)
            v->enable[row * GPU_COLS + col] =
                (row >= v->row0 && row <= v->row1 && col >= v->col0 && col <= v->col1) ? 1 : 0;
}

void gpu_viewport_setup(TM2Gpu* g, int i, int32_t w, int32_t h, int32_t x0, int32_t x1,
                        int32_t y0, int32_t y1) {
    GpuViewport* v = &g->vp[i];
    int32_t sx0 = (int32_t)((uint32_t)x0 << 5), sx1 = (int32_t)((uint32_t)x1 << 5);
    int32_t sy0 = (int32_t)((uint32_t)y0 << 5), sy1 = (int32_t)((uint32_t)y1 << 5);
    v->row0 = (sy0 >> 8) + 2;
    v->row1 = (sy1 >> 8) + 2;
    v->col0 = (sx0 >> 8) + 2;
    v->col1 = (sx1 >> 8) + 2;
    v->ncols = v->col1 - v->col0 + 1;
    v->tile0 = v->row0 * GPU_COLS + v->col0;
    v->tile1 = v->row1 * GPU_COLS + v->col1;
    v->x0 = x0; v->x1 = x1; v->y0 = y0; v->y1 = y1;
    v->width = w; v->height = h;
    v->sx0 = sx0 + 0x200; v->sx1 = sx1 + 0x200;
    v->sy0 = sy0 + 0x200; v->sy1 = sy1 + 0x200;
    vp_tables(v);

    GpuViewport* s = &g->vp[2];
    int32_t sw = (int32_t)((uint32_t)w << 5), sh = (int32_t)((uint32_t)h << 5);
    s->row0 = 2;
    s->row1 = (sh >> 8) + 2;
    s->col0 = 2;
    s->col1 = (sw >> 8) + 2;
    s->x1 = w;
    s->y1 = h;
    s->tile0 = 0xaa;
    s->ncols = s->col1 - 2 + 1;
    s->width = w;
    s->height = h;
    s->x0 = 0;
    s->y0 = 0;
    s->tile1 = s->row1 * GPU_COLS + s->col1;
    s->sx0 = 0x200;
    s->sy0 = 0x200;
    s->sx1 = sw + 0x200;
    s->sy1 = sh + 0x200;
    vp_tables(s);
}

/*
 * ORIGINAL QUIRK (the decompiler shows it as the multipliers 0x1959 / 0x1968): the enable map
 * pointer is viewport + 0x454 + i and the row range pointer viewport + 0x44 + 0x10 * i, so
 * viewport 1 tests each tile against its right neighbour's enable byte and each row against the
 * next row's range, viewport 2 two over. Kept: the offsets are stored as in the original.
 */
void gpu_viewport_select(TM2Gpu* g, int i) {
    int32_t base = i * (int32_t)sizeof(GpuViewport);
    g->r.vp_index = i;
    g->r.vp = base;
    g->r.vp_enable = base + (int32_t)offsetof(GpuViewport, enable) + i;
    g->r.vp_rows = base + (int32_t)offsetof(GpuViewport, rows) + 0x10 * i;
}

void gpu_frame_clear(TM2Gpu* g) {
    const GpuViewport* s = &g->vp[2];
    int8_t wm = (int8_t)(s->width % 8), hm = (int8_t)(s->height % 8);
    /* 0x004e8620 / 0x004e8628 / 0x004e8648: the first w % 8 columns, the first h % 8 rows
     * (0 = all). Indexed with the signed remainder: negative sizes would read before them. */
    uint32_t cb = wm == 0 ? 0xffu : (1u << wm) - 1u;
    uint32_t cmask = cb | cb << 8 | cb << 16 | cb << 24;
    uint32_t rlo, rhi;
    if (hm == 0) { rlo = rhi = 0xffffffffu; }
    else if (hm <= 4) { rlo = hm == 4 ? 0xffffffffu : (1u << (8 * hm)) - 1u; rhi = 0; }
    else { rlo = 0xffffffffu; rhi = (1u << (8 * (hm - 4))) - 1u; }
    int32_t t = s->tile0;
    uint8_t nrows = (uint8_t)((uint8_t)s->row1 - (uint8_t)s->row0 + 1);
    uint8_t ncols = (uint8_t)s->ncols;
    for (uint8_t r = 0; r < nrows; r++) {
        GpuTile* p = &g->tile[t];
        for (uint8_t c = 0; c < ncols; c++, p++) {
            p->open[1] = 0xffffffffu;
            p->f28 = 0;
            p->f2c = 0;
            p->open[0] = 0xffffffffu;
        }
        /* the last column processed (with ncols == 0 the tile before the row) */
        p--;
        p->open[0] &= cmask;
        p->open[1] &= cmask;
        t += GPU_COLS;
    }
    if (hm != 0) {
        GpuTile* p = &g->tile[t - GPU_COLS];
        for (uint8_t c = 0; c < ncols; c++, p++) {
            p->open[1] &= rhi;
            p->open[0] &= rlo;
        }
    }
}

void gpu_frame_begin(TM2Gpu* g) {
    g->r.half_mask = g->r.format == 1 ? 0xf7de : 0x7bde;
    gpu_frame_clear(g);
    g->r.flip = ~g->r.flip;
}
