/*
 * gpu3d.c: the 3D float n-gon of TM2.EXE's software GPU, ported from the machine code:
 *   command_dispatch 0x004ad874 code 0xa0 (edges + float setup),
 *   0x004afadc collect with gouraud light / overlay list / dither kernel,
 *   0x004b029c collect with fog (the common case),
 *   0x004b07ec collect without fog (flag 0x40, or flag 2: overlay).
 * See gpu3d.h for the record and state, gpu3d_float.h for how the x87 arithmetic is matched,
 * docs/RENDERER_3D.md for the whole path.
 *
 * Perspective: the record gives three planes over the screen (in edge units: 1/32 pixel with
 * the guard band, the tile corner (col, row) is (col * 256, row * 256)): w (1/z), u * w and
 * v * w (v pre shifted by the texture's log2 width). Each listed tile gets u = U / W, v = V / W
 * (1 / W clamped to [rmin, rmax]) at its corner and at its right and lower neighbours' corners,
 * so the affine kernels (gpu_tex_fill) interpolate inside the 8 x 8 tile; with fog the tile also
 * gets a fog value 4096 + fogk / W (clamped to 8191, minus a light term in 0x004afadc) at +0x34
 * and the palette row (f34 & 0x1f00) of the page palette - 0x2000 at +0x38.
 */
#include "gpu3d.h"
#include "gpu3d_float.h"

#include <string.h>

static uint16_t u16at(const uint8_t* p, int o) { uint16_t v; memcpy(&v, p + o, 2); return v; }
static int32_t i32at(const uint8_t* p, int o) { int32_t v; memcpy(&v, p + o, 4); return v; }
static uint32_t u32at(const uint8_t* p, int o) { uint32_t v; memcpy(&v, p + o, 4); return v; }
static float fat(const uint8_t* p, int o) { float v; memcpy(&v, p + o, 4); return v; }
static int32_t shl(int32_t v, int32_t n) { return (int32_t)((uint32_t)v << (n & 31)); }

/* --- tiles: the original indexes the tile table without bounds; the port drops accesses
 * outside it (reads give 0) --- */
static GpuTile g3_zero;
static GpuTile* tile_at(TM2Gpu* g, int32_t i) {
    if (i < 0 || i >= GPU_TILES) { memset(&g3_zero, 0, sizeof g3_zero); return &g3_zero; }
    return &g->tile[i];
}

/* ------------------------------------------------------------------ dispatch 0xa0 */

int gpu3d_dispatch(TM2Gpu* g, Gpu3d* s, const Gpu3dLights* lights, const uint8_t* p) {
    g->r.ntiles = 0;
    g->r.f7fc = 0;
    g->r.stamp++;
    g->r.hit = 0;
    gpu3d_prim(g, s, lights, p);
    return 0;
}

void gpu3d_prim(TM2Gpu* g, Gpu3d* s, const Gpu3dLights* lights, const uint8_t* p) {
    g->r.row_max = 0;
    g->r.row_min = 1000;
    /* edges: i -> i + 1 for i < n - 1, the closing edge n - 1 -> 0 in slot n - 1 (slot -1 from
     * +0x70 to +0x78 when n = 0); n > 8 runs into the tile grid in the original, capped here */
    int n = p[6];
    if (n > GPU_EDGES - 1) n = GPU_EDGES - 1;
    int32_t v[GPU_EDGES][2];
    for (int i = 0; i < n; i++) { v[i][0] = i32at(p, 0x78 + 8 * i); v[i][1] = i32at(p, 0x7c + 8 * i); }
    for (int i = 0; i < n - 1; i++) gpu_edge_setup(GPU_EDGE(g, i), v[i], v[i + 1]);
    if (n > 0) {
        gpu_edge_setup(GPU_EDGE(g, n - 1), v[n - 1], v[0]);
    } else {
        int32_t a[2] = {i32at(p, 0x70), i32at(p, 0x74)}, b[2] = {i32at(p, 0x78), i32at(p, 0x7c)};
        gpu_edge_setup(GPU_EDGE(g, -1), a, b);
    }
    for (int i = 0; i < n; i++) gpu_edge_walk(g, GPU_EDGE(g, i));
    if (!g->r.hit) return;

    uint32_t fl = u16at(p, 4);
    g->r.render_flags = (int32_t)fl;
    s->color = u32at(p, 8);
    s->color_r = s->color >> 16 & 0xff;
    s->color_g = s->color >> 8 & 0xff;
    s->z = g3_fldst(fat(p, 0x10));
    s->color_b = s->color & 0xff;
    if (g->r.fog != 0 && !(fl & 0x40)) s->fogk = g3_st(g3_div(4096.0, g3_ld(fat(p, 0x10))));
    else s->fogk = g3_ffrom(0);

    if (fl & 1) {
        uint32_t pg = u16at(p, 0xc) & 0xfff;
        TpcPage z = {0};
        /* ORIGINAL: 12 bit index into a 512 page table; the port reads 0 beyond */
        const TpcPage* t = pg < TPC_MAX_PAGES ? &g->pages[pg] : &z;
        g->r.tex_pixels = t->pixels;
        g->r.tex_clut = t->clut;
        g->r.tex_w = t->width;
        g->r.tex_h = t->height;
        int32_t lg = 8;
        if (t->width != 0) {
            lg = 3;
            for (uint32_t w = t->width >> 4; w != 0; w >>= 1) lg++;
        }
        const double k256 = 256.0;      /* the double 0x004e35e4 */
        double a14 = g3_mul(g3_ld(fat(p, 0x14)), k256), a2c = g3_mul(g3_ld(fat(p, 0x2c)), k256);
        double a30 = g3_mul(g3_ld(fat(p, 0x30)), k256), a48 = g3_mul(g3_ld(fat(p, 0x48)), k256);
        g->r.tex_log_a = lg;
        g->r.umask = (int32_t)t->width - 1;
        g->r.vmask = shl((int32_t)t->height - 1, lg);
        g->r.tex_shift = 12 - lg;
        g->r.umask12 = shl(g->r.umask, 12);
        g->r.tex_log_b = lg;
        g->r.vmask12 = shl(g->r.vmask, 12);
        int32_t one = shl(1, lg);
        double kx = g3_mul((double)one, k256);           /* fild * 256.0, kept as float */
        float kxf = g3_st(kx), onef = g3_st((double)one);
        double b18 = g3_mul(g3_ld(fat(p, 0x18)), g3_ld(onef));
        double b34 = g3_mul(g3_ld(fat(p, 0x34)), g3_ld(onef));
        memcpy(&s->u_0, p + 0x4c, 4);
        memcpy(&s->w_0, p + 0x64, 4);
        memcpy(&s->rmin, p + 0x68, 4);
        memcpy(&s->rmax, p + 0x6c, 4);
        memcpy(&s->org_x, p + 0x70, 4);
        memcpy(&s->org_y, p + 0x74, 4);
        memcpy(&s->u_dx, p + 0x14, 4);
        memcpy(&s->w_dx, p + 0x2c, 4);
        memcpy(&s->u_dy, p + 0x30, 4);
        memcpy(&s->w_dy, p + 0x48, 4);
        double b50 = g3_mul(g3_ld(fat(p, 0x50)), g3_ld(onef));
        s->u_tx = g3_st(a14);
        double c18 = g3_mul(g3_ld(fat(p, 0x18)), g3_ld(kxf));
        s->w_tx = g3_st(a2c);
        double c34 = g3_mul(g3_ld(fat(p, 0x34)), g3_ld(kxf));
        s->u_ty = g3_st(a30);
        s->w_ty = g3_st(a48);
        s->v_dx = g3_st(b18);
        s->v_dy = g3_st(b34);
        s->v_0 = g3_st(b50);
        s->v_tx = g3_st(c18);
        s->v_ty = g3_st(c34);
    }
    if (fl & 0x4000) {
        const double k = 256.0;         /* the float 0x004e35ec */
        static const int dx[3] = {0x1c, 0x20, 0x24}, dy[3] = {0x38, 0x3c, 0x40}, o0[3] = {0x54, 0x58, 0x5c};
        for (int i = 0; i < 3; i++) {
            s->g_tx[i] = g3_st(g3_mul(g3_ld(fat(p, dx[i])), k));
            s->g_ty[i] = g3_st(g3_mul(g3_ld(fat(p, dy[i])), k));
            memcpy(&s->g_0[i], p + o0[i], 4);
            memcpy(&s->g_dx[i], p + dx[i], 4);
            s->g_dy[i] = g3_fldst(fat(p, dy[i]));           /* fld / fstp */
        }
        uint32_t li = u16at(p, 0xe);
        uint8_t e[0x10] = {0};
        if (lights && lights->base && li < lights->count) memcpy(e, lights->base + 0x78 * li, 0x10);
        double l0 = g3_ld(fat(e, 0));
        s->light_r2 = g3_st(g3_mul(l0, l0));
        s->light_k = g3_fldst(fat(e, 0xc));
        s->light_s = u32at(e, 4);
    }
    if (fl & 0x80) {
        const double k256 = 256.0;
        s->e_tx = g3_st(g3_mul(g3_ld(fat(p, 0x28)), k256));
        s->e_ty = g3_st(g3_mul(g3_ld(fat(p, 0x44)), k256));
        s->e_0 = g3_fldst(fat(p, 0x60));     /* fld / fstp */
        s->e_dx = g3_fldst(fat(p, 0x28));
        s->e_dy = g3_fldst(fat(p, 0x44));
    }
    if (((fl & 0x40) && !(fl & 0x4080)) || (fl & 2)) gpu3d_collect_plain(g, s);
    else if (s->special == 0 && !(fl & 0x4080)) gpu3d_collect_fog(g, s);
    else gpu3d_collect_light(g, s);
}

/* ------------------------------------------------------------------ collects */

enum { K_LIGHT, K_FOG, K_PLAIN };

typedef struct {
    float w, u, v, gl[3];        /* the interpolants at the current tile corner (stored floats) */
} Acc;

/* 1 / w clamped: the first compare uses the double quotient, the second the stored float */
static float recip(const Gpu3d* s, double w) {
    double rd = g3_div(1.0, w);
    float rf = g3_st(rd);
    if (!(rd >= g3_ld(s->rmin))) rf = s->rmin;
    else if (g3_ld(rf) > g3_ld(s->rmax)) rf = s->rmax;
    return rf;
}

/* the corner values of tile t. lower: the lower neighbour (interpolants + one tile row,
 * summed in double, not stored) */
static void corner(TM2Gpu* g, Gpu3d* s, int kind, GpuTile* t, const Acc* a, int lower, int32_t clut0) {
    double w = g3_ld(a->w), u = g3_ld(a->u), v = g3_ld(a->v);
    if (lower) {
        w = g3_add(w, g3_ld(s->w_ty));
    }
    float r = recip(s, w);
    double rd = g3_ld(r);
    if (lower) {
        u = g3_add(u, g3_ld(s->u_ty));
        v = g3_add(v, g3_ld(s->v_ty));
    }
    t->u = g3_fistp(g3_mul(u, rd));
    t->v = g3_fistp(g3_mul(v, rd));
    if (kind == K_PLAIN) {
        t->clut = (int32_t)g->r.tex_clut;
        return;
    }
    float f = g3_st(g3_add(g3_mul(rd, g3_ld(s->fogk)), 4096.0));
    if ((int32_t)g3_fbits(f) > 0x45fff800) f = g3_ffrom(0x45fff800);   /* 8191.0 */
    if (kind == K_LIGHT && (g->r.render_flags & 0x4000)) {
        double c[3];
        for (int i = 0; i < 3; i++) {
            c[i] = g3_ld(a->gl[i]);
            if (lower) c[i] = g3_add(c[i], g3_ld(s->g_ty[i]));
        }
        double x0 = g3_mul(c[0], rd), x1 = g3_mul(c[1], rd), x2 = g3_mul(rd, c[2]);
        double sum = g3_add(g3_add(g3_mul(x0, x0), g3_mul(x1, x1)), g3_mul(x2, x2));
        float sf = g3_st(sum);
        if (!(sum >= g3_ld(s->light_r2))) {
            double d = g3_mul(g3_sub(1.0, g3_mul(g3_ld(sf), g3_ld(s->light_k))), g3_ld(g3_ffrom(s->light_s)));
            f = g3_st(g3_sub(g3_ld(f), d));
        }
    }
    t->f34 = g3_fistp(g3_ld(f));
    t->clut = (int32_t)((uint32_t)clut0 + ((uint32_t)t->f34 & 0x1f00) * 2);
}

static void advance(const Gpu3d* s, int kind, Acc* a) {
    a->w = g3_st(g3_add(g3_ld(a->w), g3_ld(s->w_tx)));
    a->u = g3_st(g3_add(g3_ld(a->u), g3_ld(s->u_tx)));
    a->v = g3_st(g3_add(g3_ld(a->v), g3_ld(s->v_tx)));
    if (kind == K_LIGHT)
        for (int i = 0; i < 3; i++) a->gl[i] = g3_st(g3_add(g3_ld(a->gl[i]), g3_ld(s->g_tx[i])));
}

static void collect(TM2Gpu* g, Gpu3d* s, int kind) {
    int32_t clut0 = (int32_t)(g->r.tex_clut - 0x2000u);
    int32_t stamp = g->r.stamp;
    int plain_overlay = kind == K_PLAIN && (g->r.render_flags & 2);
    for (int32_t row = g->r.row_min; row <= g->r.row_max; row++) {
        /* the span and the selected viewport's row range (vp base + 0x44 + 0x10 * row: not the
         * vp_rows pointer the flat collect uses) */
        GpuSpan zs = {0, 0, 0, 0};
        GpuSpan* sp = row >= 0 && row < GPU_ROWS ? &g->span[row] : &zs;
        int32_t vp_hi = 0, vp_lo = 0;
        {
            int64_t off = (int64_t)g->r.vp + 0x44 + 0x10 * (int64_t)row;
            const uint8_t* base = (const uint8_t*)g->vp;
            size_t lim = sizeof g->vp + sizeof g->vp_slack;
            if (off >= 0 && (size_t)off + 8 <= lim) { memcpy(&vp_hi, base + off, 4); memcpy(&vp_lo, base + off + 4, 4); }
        }
        if (stamp != sp->hi_stamp || stamp != sp->lo_stamp) {
            for (int32_t i = vp_lo + 1; i <= vp_hi - 1; i++) {
                GpuTile* t = tile_at(g, i);
                t->cover[1] = 0xffffffffu;
                if (kind != K_PLAIN) { t->f28 = 0; t->f2c = 0; }
                t->cover[0] = 0xffffffffu;
            }
            continue;
        }
        if (vp_hi - 1 < sp->hi) sp->hi = vp_hi - 1;
        if (sp->lo < vp_lo + 1) sp->lo = vp_lo + 1;
        int32_t rs = row >= 0 && row < GPU_ROWS ? g->row_start[row] : 0;
        double fy = g3_sub((double)shl(row, 8), g3_ld(s->org_y));
        int32_t ti = sp->lo;
        double fx = g3_sub((double)shl(ti - rs, 8), g3_ld(s->org_x));
        Acc a;
        double A = g3_mul(fy, g3_ld(s->w_dy)), B = g3_mul(fy, g3_ld(s->u_dy)), C = g3_mul(fy, g3_ld(s->v_dy));
        a.w = g3_st(g3_add(g3_add(A, g3_mul(fx, g3_ld(s->w_dx))), g3_ld(s->w_0)));
        if (kind == K_LIGHT) {
            /* 0x004afadc: only w uses the double x; the others use x stored as a float */
            double xf = g3_ld(g3_st(fx));
            a.u = g3_st(g3_add(g3_add(B, g3_mul(xf, g3_ld(s->u_dx))), g3_ld(s->u_0)));
            a.v = g3_st(g3_add(g3_add(C, g3_mul(xf, g3_ld(s->v_dx))), g3_ld(s->v_0)));
            for (int i = 0; i < 3; i++) {
                double D = g3_mul(fy, g3_ld(s->g_dy[i]));
                a.gl[i] = g3_st(g3_add(g3_add(D, g3_mul(xf, g3_ld(s->g_dx[i]))), g3_ld(s->g_0[i])));
            }
        } else {
            a.u = g3_st(g3_add(g3_add(B, g3_mul(fx, g3_ld(s->u_dx))), g3_ld(s->u_0)));
            a.v = g3_st(g3_add(g3_add(C, g3_mul(fx, g3_ld(s->v_dx))), g3_ld(s->v_0)));
            a.gl[0] = a.gl[1] = a.gl[2] = 0;
        }
        for (; ti <= sp->hi; ti++) {
            GpuTile* t = tile_at(g, ti);
            t->draw[0] = t->open[0] & t->cover[0];
            t->draw[1] = t->open[1] & t->cover[1];
            if ((t->draw[0] | t->draw[1]) == 0) {
                t->cover[0] = 0xffffffffu;
                t->cover[1] = 0xffffffffu;
                advance(s, kind, &a);
                continue;
            }
            if (kind != K_FOG) {
                if ((t->f28 & t->draw[0]) || (t->f2c & t->draw[1])) {
                    if (g->r.f7fc >= 0 && g->r.f7fc < GPU_TILES) s->list2[g->r.f7fc] = (uint32_t)ti;
                    g->r.f7fc++;
                }
                if (!plain_overlay) {
                    t->draw[0] &= ~t->f28;
                    t->draw[1] &= ~t->f2c;
                }
            }
            if (kind == K_FOG || (t->draw[0] | t->draw[1])) {
                if (g->r.ntiles >= 0 && g->r.ntiles < GPU_TILES) g->list[g->r.ntiles] = (uint32_t)ti;
                g->r.ntiles++;
            }
            if (t->stamp != stamp) {
                t->stamp = stamp;
                corner(g, s, kind, t, &a, 0, clut0);
            }
            GpuTile* lo = tile_at(g, ti + GPU_COLS);
            lo->stamp = stamp;
            corner(g, s, kind, lo, &a, 1, clut0);
            advance(s, kind, &a);
            GpuTile* rt = tile_at(g, ti + 1);
            if (rt->stamp != stamp) {
                rt->stamp = stamp;
                corner(g, s, kind, rt, &a, 0, clut0);
            }
        }
    }
}

void gpu3d_tex_kernel(TM2Gpu* g) {
    if (g->r.tex_mode == 0) gpu_tex_fill(g);
    else gpu3d_lowdetail_fill(g);
}

void gpu3d_collect_light(TM2Gpu* g, Gpu3d* s) {
    collect(g, s, K_LIGHT);
    if (g->r.f7fc != 0) gpu3d_overlay_fill(g, s);
    if (g->r.ntiles != 0) {
        if (s->dither != 0) gpu3d_fog_fill(g, s);
        else gpu3d_tex_kernel(g);
    }
}

void gpu3d_collect_fog(TM2Gpu* g, Gpu3d* s) {
    collect(g, s, K_FOG);
    if (g->r.ntiles != 0) gpu3d_tex_kernel(g);
}

void gpu3d_collect_plain(TM2Gpu* g, Gpu3d* s) {
    collect(g, s, K_PLAIN);
    if ((g->r.render_flags & 2) && g->r.ntiles != 0) {
        gpu3d_overlay_mark(g, s);
        return;
    }
    if (g->r.f7fc != 0) gpu3d_overlay_fill(g, s);
    if (g->r.ntiles != 0) gpu3d_tex_kernel(g);
}

int gpu3d_dispatch_all(TM2Gpu* g, Gpu3d* s, const Gpu3dLights* lights, const uint8_t* prim) {
    if ((prim[7] & 0xfd) == 0xa0) return gpu3d_dispatch(g, s, lights, prim);
    return gpu_dispatch(g, prim);
}
