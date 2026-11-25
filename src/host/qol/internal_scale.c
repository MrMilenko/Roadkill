/*
 * host/qol/internal_scale.c: the renderer for the display switches (host/qol/display.h), on its GPU_HI build
 * (src/engine/render/gpu_hi.h). Port code, not from the original.
 *
 * With a switch on, src/rt/bind_render.c hands every drawing call here instead of to the
 * original's renderer (which then only keeps the state the game reads: viewports, registers):
 *
 *   L  draws into the game's own surface at the game's resolution, as the original renderer
 *      did, on the larger grid: the game surface may be wider than 640 (widescreen).
 *   P  (internal scale S > 1) draws the same primitives at S times the resolution into a host
 *      buffer: each record is copied and scaled (screen points * S, edge units (v - 0x200) * S +
 *      0x200, the 3D record's plane gradients / S), sprites step their texels 1 / S, lines are
 *      drawn S x S times side by side.
 *
 * The picture shown at internal scale S (qol_gpu_compose, called at present time): per pixel of
 * the large picture, P's pixel when P drew it this frame (its tile's open bit is clear), else the
 * game surface's pixel (S x S). What the game writes into its surface other than through the
 * renderer (fills and copies, GDI text) after the renderer drew there is reported by the host
 * (qol_gpu_overwritten) and then wins, as it does in the game surface.
 */
#include "engine/render/gpu_hi.h"
#include "display.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    TM2Gpu g;           /* TM2GpuHi */
    Gpu3d s;
    Gpu3dLights lights;
    int scale;
} Inst;

#define HB_PITCH (GPU_HI_MAX_W * 2)

static struct {
    Inst* l;            /* game resolution */
    Inst* p;            /* internal scale, NULL at scale 1 */
    int scale;
    int w, h;           /* the whole screen (viewport 2) */
    int p_ok;           /* w * scale x h * scale fits P's grid */
    uint16_t* hb;       /* P's screen, GPU_HI_MAX_W x GPU_HI_MAX_H */
    uint16_t* out;      /* the composed picture */
    uint8_t* block;     /* per large pixel: the game overwrote what P drew */
    int block_dirty;
    int drew;           /* P drew something since its frame began */
} H;

static int dummy_rand(void* u) { (void)u; return 0; }

static Inst* inst_new(int scale, const GpuRegs* r, const uint32_t noise_a[0x80][2], const uint32_t noise_b[0x80])
{
    Inst* x = calloc(1, sizeof *x);
    if (!x) return NULL;
    x->scale = scale;
    x->g.r = *r;
    gpuhi_init(&x->g, r->mmx != 0, dummy_rand, NULL);
    /* the noise as the original renderer drew it from the game's rand() (not drawn again: the
     * game's random sequence stays as without the switch) */
    memcpy(x->g.noise_a, noise_a, sizeof x->g.noise_a);
    memcpy(x->g.noise_b, noise_b, sizeof x->g.noise_b);
    x->g.g3d = &x->s;
    x->g.lights = &x->lights;
    x->g.hi_scale = scale;
    return x;
}

void qol_gpu_init(const void* regs, const uint32_t noise_a[0x80][2], const uint32_t noise_b[0x80])
{
    const GpuRegs* r = regs;
    if (!H.l) {
        H.scale = qol_display_scale();
        H.l = inst_new(1, r, noise_a, noise_b);
        if (H.scale > 1) {
            H.p = inst_new(H.scale, r, noise_a, noise_b);
            H.hb = calloc((size_t)GPU_HI_MAX_W * GPU_HI_MAX_H, 2);
            H.out = calloc((size_t)GPU_HI_MAX_W * GPU_HI_MAX_H, 2);
            H.block = calloc((size_t)GPU_HI_MAX_W * GPU_HI_MAX_H, 1);
            if (!H.p || !H.hb || !H.out || !H.block) { free(H.p); H.p = NULL; H.scale = 1; }
        }
        if (!H.l) abort();
    } else {
        /* the game ran its init again: the same tables */
        Inst* xs[2] = {H.l, H.p};
        for (int k = 0; k < 2; k++) {
            if (!xs[k]) continue;
            xs[k]->g.r = *r;
            gpuhi_init(&xs[k]->g, r->mmx != 0, dummy_rand, NULL);
            memcpy(xs[k]->g.noise_a, noise_a, sizeof xs[k]->g.noise_a);
            memcpy(xs[k]->g.noise_b, noise_b, sizeof xs[k]->g.noise_b);
        }
    }
    if (H.p) gpuhi_set_pitch(&H.p->g, HB_PITCH);
}

/* the registers an instance keeps for itself: its pitch (P: its own buffer) and its viewport
 * pointers (offsets into its own, larger, viewports) */
static void keep_own(GpuRegs* dst, const GpuRegs* own)
{
    dst->pitch_px = own->pitch_px;
    dst->pitch_bytes = own->pitch_bytes;
    dst->pitch_m16 = own->pitch_m16;
    dst->pitch_q = own->pitch_q;
    dst->pitch_e = own->pitch_e;
    dst->pitch_px_m8 = own->pitch_px_m8;
    dst->pitch_x4 = own->pitch_x4;
    dst->pitch_x8 = own->pitch_x8;
    dst->vp_index = own->vp_index;
    dst->vp = own->vp;
    dst->vp_enable = own->vp_enable;
    dst->vp_rows = own->vp_rows;
}

static void regs_in(Inst* x, const GpuRegs* r)
{
    GpuRegs own = x->g.r;
    x->g.r = *r;
    keep_own(&x->g.r, &own);
}

static void regs_out(const Inst* x, GpuRegs* r)
{
    GpuRegs game = *r;
    *r = x->g.r;
    keep_own(r, &game);
}

static void inputs(Inst* x, const QolGpuIn* in)
{
    if (x->scale == 1) {
        x->g.screen = in->screen;
        x->g.screen_bytes = in->screen_bytes;
        x->g.pitch = in->pitch;
    } else {
        x->g.screen = (uint8_t*)H.hb;
        x->g.screen_bytes = (size_t)HB_PITCH * GPU_HI_MAX_H;
        x->g.pitch = HB_PITCH;
    }
    x->g.pages = in->pages;
    x->g.tex_mem = in->tex_mem;
    x->g.tex_addr = in->tex_addr;
    x->g.tex_size = in->tex_size;
    x->lights.base = in->lights;
    x->lights.count = in->lights_count;
    x->s.dither = in->dither;
    x->s.special = in->special;
}

void qol_gpu_set_pitch(int32_t pitch)
{
    if (H.l) gpuhi_set_pitch(&H.l->g, pitch);
}

void qol_gpu_viewport_setup(int i, int32_t w, int32_t h, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    if (!H.l) return;
    gpuhi_viewport_setup(&H.l->g, i, w, h, x0, x1, y0, y1);
    if (!H.p) return;
    int s = H.scale;
    H.w = w;
    H.h = h;
    H.p_ok = w > 0 && h > 0 && (int64_t)w * s <= GPU_HI_MAX_W && (int64_t)h * s <= GPU_HI_MAX_H;
    if (H.p_ok)
        gpuhi_viewport_setup(&H.p->g, i, w * s, h * s, x0 * s, x1 * s + s - 1, y0 * s, y1 * s + s - 1);
}

void qol_gpu_viewport_select(int i)
{
    if (H.l) gpuhi_viewport_select(&H.l->g, i);
    if (H.p) gpuhi_viewport_select(&H.p->g, i);
}

void qol_gpu_frame_begin(void* regs, const QolGpuIn* in, int clear_only)
{
    if (!H.l) return;
    if (H.p && H.p_ok) {
        regs_in(H.p, regs);
        inputs(H.p, in);
        if (clear_only) gpuhi_frame_clear(&H.p->g);
        else gpuhi_frame_begin(&H.p->g);
        if (H.block_dirty) memset(H.block, 0, (size_t)GPU_HI_MAX_W * GPU_HI_MAX_H);
        H.block_dirty = 0;
        H.drew = 0;
    }
    regs_in(H.l, regs);
    inputs(H.l, in);
    if (clear_only) gpuhi_frame_clear(&H.l->g);
    else gpuhi_frame_begin(&H.l->g);
    regs_out(H.l, regs);
}

/* ---- the scaled copy of a record ---- */

static int16_t rd16(const uint8_t* p, int o) { int16_t v; memcpy(&v, p + o, 2); return v; }
static int32_t rd32(const uint8_t* p, int o) { int32_t v; memcpy(&v, p + o, 4); return v; }
static float rdf(const uint8_t* p, int o) { float v; memcpy(&v, p + o, 4); return v; }
static void wr16(uint8_t* p, int o, int64_t v)
{
    int16_t x = (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
    memcpy(p + o, &x, 2);
}
static void wr32(uint8_t* p, int o, int64_t v)
{
    int32_t x = (int32_t)(v < INT32_MIN ? INT32_MIN : v > INT32_MAX ? INT32_MAX : v);
    memcpy(p + o, &x, 4);
}
static void wrf(uint8_t* p, int o, float v) { memcpy(p + o, &v, 4); }

/* a screen point (pixels): * s */
static void pt16(uint8_t* p, int o, int s)
{
    wr16(p, o, (int64_t)rd16(p, o) * s);
    wr16(p, o + 2, (int64_t)rd16(p, o + 2) * s);
}
/* a point in edge units (1/32 pixel with the 0x200 guard band) */
static void pt32(uint8_t* p, int o, int s)
{
    wr32(p, o, ((int64_t)rd32(p, o) - 0x200) * s + 0x200);
    wr32(p, o + 4, ((int64_t)rd32(p, o + 4) - 0x200) * s + 0x200);
}

#define REC 0x100

static void scale_record(uint8_t* p, int s)
{
    int n;
    switch (p[7] & 0xfd) {
    case 0x20: for (int k = 0; k < 3; k++) pt16(p, 8 + 4 * k, s); break;
    case 0x28: for (int k = 0; k < 4; k++) pt16(p, 8 + 4 * k, s); break;
    case 0x24: for (int k = 0; k < 3; k++) pt16(p, 8 + 8 * k, s); break;
    case 0x2c: for (int k = 0; k < 4; k++) pt16(p, 8 + 8 * k, s); break;
    case 0x74: case 0x7c:
        pt16(p, 8, s);
        {
            uint16_t w, h;
            memcpy(&w, p + 0xc, 2);
            memcpy(&h, p + 0xe, 2);
            uint32_t ws = (uint32_t)w * (uint32_t)s, hs = (uint32_t)h * (uint32_t)s;
            w = (uint16_t)(ws > 0xffff ? 0xffff : ws);
            h = (uint16_t)(hs > 0xffff ? 0xffff : hs);
            memcpy(p + 0xc, &w, 2);
            memcpy(p + 0xe, &h, 2);
        }
        break;
    case 0xb0:
        n = p[6] > GPU_EDGES - 1 ? GPU_EDGES - 1 : p[6];
        for (int k = 0; k < n; k++) pt32(p, 8 + 8 * k, s);
        break;
    case 0x90:
        for (int k = 0; k < 3; k++) pt32(p, 0x24 + 8 * k, s);
        n = p[6] > GPU_EDGES - 1 ? GPU_EDGES - 1 : p[6];
        for (int k = 0; k < n; k++) pt32(p, 0x48 + 8 * k, s);
        break;
    case 0xa0:
        /* planes v0 + dx (x - ox) + dy (y - oy) over edge units: gradients / s, origin moved */
        for (int k = 0; k < 7; k++) {
            wrf(p, 0x14 + 4 * k, rdf(p, 0x14 + 4 * k) / (float)s);
            wrf(p, 0x30 + 4 * k, rdf(p, 0x30 + 4 * k) / (float)s);
        }
        wrf(p, 0x70, (rdf(p, 0x70) - 512.0f) * (float)s + 512.0f);
        wrf(p, 0x74, (rdf(p, 0x74) - 512.0f) * (float)s + 512.0f);
        n = p[6] > GPU_EDGES - 1 ? GPU_EDGES - 1 : p[6];
        for (int k = 0; k < n; k++) pt32(p, 0x78 + 8 * k, s);
        break;
    default:
        break;
    }
}

/* a line at scale s: s x s lines side by side */
static void p_line(int raw, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int s = H.scale;
    for (int j = 0; j < s; j++)
        for (int i = 0; i < s; i++) {
            if (raw) gpuhi_line_raw(&H.p->g, x0 * s + i, y0 * s + j, x1 * s + i, y1 * s + j);
            else gpuhi_line(&H.p->g, x0 * s + i, y0 * s + j, x1 * s + i, y1 * s + j);
        }
}

int qol_gpu_dispatch(void* regs, const QolGpuIn* in, const uint8_t* prim)
{
    if (!H.l) return 0;
    if (H.p && H.p_ok) {
        uint8_t rec[REC];
        memcpy(rec, prim, REC);
        regs_in(H.p, regs);
        inputs(H.p, in);
        uint8_t code = rec[7] & 0xfd;
        if (code == 0x40 || code == 0x50 || code == 0xc0) {
            int16_t x0 = rd16(rec, 8), y0 = rd16(rec, 10);
            int16_t x1 = rd16(rec, code == 0x50 ? 16 : 12), y1 = rd16(rec, code == 0x50 ? 18 : 14);
            if (code == 0x50) H.p->g.r.color16 = 0xfe;
            else {
                H.p->g.r.color24 = (uint32_t)rec[4] << 16 | (uint32_t)rec[5] << 8 | rec[6];
                H.p->g.r.color16 = (uint16_t)gpuhi_rgb24_to_16(H.p->g.r.format, H.p->g.r.color24);
            }
            p_line(code == 0xc0, x0, y0, x1, y1);
            H.drew = 1;
        } else {
            scale_record(rec, H.scale);
            gpuhi3d_dispatch_all(&H.p->g, &H.p->s, &H.p->lights, rec);
            if (H.p->g.r.hit) H.drew = 1;
        }
    }
    regs_in(H.l, regs);
    inputs(H.l, in);
    int r = gpuhi3d_dispatch_all(&H.l->g, &H.l->s, &H.l->lights, prim);
    regs_out(H.l, regs);
    return r;
}

void qol_gpu_line(void* regs, const QolGpuIn* in, int raw, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    if (!H.l) return;
    if (H.p && H.p_ok) {
        regs_in(H.p, regs);
        inputs(H.p, in);
        p_line(raw, x0, y0, x1, y1);
        H.drew = 1;
    }
    regs_in(H.l, regs);
    inputs(H.l, in);
    if (raw) gpuhi_line_raw(&H.l->g, x0, y0, x1, y1);
    else gpuhi_line(&H.l->g, x0, y0, x1, y1);
    regs_out(H.l, regs);
}

uint32_t qol_gpu_pixel_open(int32_t x, int32_t y) { return H.l ? gpuhi_pixel_open(&H.l->g, x, y) : 0; }

void qol_gpu_pixel_take(int32_t x, int32_t y)
{
    if (!H.l) return;
    gpuhi_pixel_take(&H.l->g, x, y);
    if (H.p && H.p_ok)
        for (int j = 0; j < H.scale; j++)
            for (int i = 0; i < H.scale; i++) gpuhi_pixel_take(&H.p->g, x * H.scale + i, y * H.scale + j);
}

/* ---- the picture ---- */

/* P drew pixel (x, y) of the large picture this frame */
static int p_drawn(int x, int y)
{
    const GpuTile* t = &H.p->g.tile[((y >> 3) + 2) * GPU_COLS + (x >> 3) + 2];
    return !((t->open[(y >> 2) & 1] >> (8 * (y & 3) + (x & 7))) & 1u);
}

void qol_gpu_overwritten(int x, int y, int w, int h, int surf_w, int surf_h)
{
    if (!H.p || !H.p_ok || !H.drew || surf_w != H.w || surf_h != H.h) return;
    int s = H.scale;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > H.w ? H.w : x + w, y1 = y + h > H.h ? H.h : y + h;
    for (int py = y0 * s; py < y1 * s; py++)
        for (int px = x0 * s; px < x1 * s; px++)
            if (p_drawn(px, py)) {
                H.block[(size_t)py * GPU_HI_MAX_W + px] = 1;
                H.block_dirty = 1;
            }
}

const uint16_t* qol_gpu_compose(const uint8_t* px, int lw, int lh, int pitch, int* ow, int* oh, int* opitch)
{
    if (!H.p || !H.p_ok || !H.drew || lw != H.w || lh != H.h) return NULL;
    int s = H.scale, W = lw * s, Ht = lh * s;
    for (int y = 0; y < Ht; y++) {
        const uint8_t* lrow = px + (size_t)(y / s) * (size_t)pitch;
        const uint16_t* hrow = H.hb + (size_t)y * GPU_HI_MAX_W;
        const uint8_t* brow = H.block + (size_t)y * GPU_HI_MAX_W;
        uint16_t* orow = H.out + (size_t)y * W;
        for (int x = 0; x < W; x++) {
            if (p_drawn(x, y) && !brow[x]) orow[x] = hrow[x];
            else {
                uint16_t v;
                memcpy(&v, lrow + 2 * (x / s), 2);
                orow[x] = v;
            }
        }
    }
    *ow = W;
    *oh = Ht;
    *opitch = W * 2;
    return H.out;
}
