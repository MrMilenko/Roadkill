/*
 * dx_ddraw.c: DirectDraw for the game on SDL2 (port code, not from the original; docs/PLATFORM.md
 * "Display", Part A / Part B list every call the game makes and with which arguments).
 *
 *   DirectDrawCreate (DDRAW.dll, import slot and the thunk 0x004d18ea) -> an IDirectDraw object;
 *   QueryInterface(IID_IDirectDraw2) -> an IDirectDraw2 object with its own reference count.
 *   Surfaces are 16 bit pixel buffers in game memory (dx_mem): Lock writes their game address and
 *   pitch into the caller's DDSURFACEDESC, the software renderer draws there. The primary
 *   surface is the window: a Blt to it or a Flip of its chain presents it through dx_sdl.c.
 *   Flip rotates the pixel buffers of the chain as DirectDraw does, so the back buffer the game
 *   locks next is the old front.
 *   GetCaps reports a plain 2D card: blits, colour fill, stretch, source colour key; no 3D, no
 *   bank switched memory. Modes: 16 bit, 320 x 200 .. 1024 x 768 at 60 Hz (the game keeps those
 *   no wider than 640).
 *
 * Direct3D: absent. IDirectDraw2::QueryInterface(IID_IDirect3D2) answers E_NOINTERFACE and the
 * caps have no DDCAPS_3D, so the game stays on its software PS1 GPU path (src/render). What the
 * hardware path (display +0x24 .. +0x4c, docs/PLATFORM.md 0x004b5bf0 .. 0x004b67d0, 0x004bf920 ..)
 * would need: IDirect3D2 (CreateDevice on the offscreen surface, CreateViewport, CreateMaterial,
 * EnumTextureFormats), IDirect3DDevice2 (GetCaps, AddViewport, SetCurrentViewport, BeginScene /
 * EndScene, SetRenderState, SetLightState, DrawPrimitive of D3DTLVERTEX triangle fans and line
 * strips, GetDirect3D), IDirect3DViewport2 (SetViewport2, SetBackground), IDirect3DMaterial2
 * (SetMaterial, GetHandle) and IDirect3DTexture2 on 8 / 16 bit texture surfaces with palettes
 * and colour keys (GetHandle, Load): on SDL that is a GL or SDL_RenderGeometry backend drawing
 * the pre-transformed vertices with the PS1 blend modes of 0x004b617c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dx_com.h"
#include "dx_mem.h"
#include "dx_sdl.h"
#include "platform/pf2_env.h"
#include "qol/display.h"
#include "host/settings/settings.h"

#define DDERR_ALREADYINITIALIZED 0x88760005u
#define DDERR_NOCLIPPERATTACHED  0x887600cdu
#define DDERR_NOCOLORKEY         0x887600d7u
#define DDERR_NOPALETTEATTACHED  0x887600e1u
#define DDERR_NOTFOUND           0x887600ffu
#define DDERR_NOCLIPLIST         0x887600cdu

#define DDSD_CAPS 1u
#define DDSD_HEIGHT 2u
#define DDSD_WIDTH 4u
#define DDSD_PITCH 8u
#define DDSD_BACKBUFFERCOUNT 0x20u
#define DDSD_LPSURFACE 0x800u
#define DDSD_PIXELFORMAT 0x1000u
#define DDSD_REFRESHRATE 0x40000u

#define DDSCAPS_BACKBUFFER 0x4u
#define DDSCAPS_COMPLEX 0x8u
#define DDSCAPS_FLIP 0x10u
#define DDSCAPS_FRONTBUFFER 0x20u
#define DDSCAPS_OFFSCREENPLAIN 0x40u
#define DDSCAPS_PRIMARYSURFACE 0x200u
#define DDSCAPS_SYSTEMMEMORY 0x800u
#define DDSCAPS_VIDEOMEMORY 0x4000u
#define DDSCAPS_VISIBLE 0x8000u
#define DDSCAPS_LOCALVIDMEM 0x10000000u

#define DDBLT_COLORFILL 0x400u
#define DDBLT_KEYSRC 0x8000u
#define DDBLTFAST_SRCCOLORKEY 1u
#define DDCKEY_SRCBLT 8u

static DxIface I_DD = {"IDirectDraw", 0, {0x6c14db80u}};
static DxIface I_DD2 = {"IDirectDraw2", 0, {0xb3a6f3e0u}};
static DxIface I_SURF = {"IDirectDrawSurface", 0, {0x6c14db81u, 0x57805885u, 0xda044e00u}};
static DxIface I_CLIP = {"IDirectDrawClipper", 0, {0x6c14db85u}};

typedef struct Surf {
    uint32_t px;                 /* game address of the pixels */
    int w, h, pitch;
    uint32_t caps;
    int primary;
    DxObj* back[2];              /* the flip chain behind a primary */
    int nback;
    DxObj* chain_front;          /* a back buffer's primary */
    DxObj* clipper;
    uint32_t hdc;                /* fake HDC from GetDC, 0 until asked */
    uint8_t* dc_copy;            /* display.internal_scale: the pixels at GetDC (host/qol/display.h) */
    int has_ck;
    uint32_t ck_lo, ck_hi;
    uint32_t palette;
    int ext_w, ext_h;            /* windowed primary: the extent blitted into it */
} Surf;

typedef struct { uint32_t hwnd; } Clip;

static struct {
    uint32_t hwnd, coop;
    int mode_w, mode_h, mode_bpp, mode_hz;   /* SetDisplayMode, 0 = desktop (windowed) */
    DxObj* primary;
    DxObj* surfs[256];
    int nsurfs;
    uint32_t vblank_tick;
} D;

static const struct { int w, h; } MODES[] = {
    {320, 200}, {320, 240}, {400, 300}, {512, 384}, {640, 400}, {640, 480}, {800, 600}, {1024, 768},
};

/* ---- helpers ---- */

static Surf* surf_of(uint32_t a) { DxObj* o = dx_obj_get(a, &I_SURF); return o ? o->host : NULL; }

static void write_pf(uint32_t p)
{
    memset(rt_ptr(p), 0, 0x20);
    rt_w32(p, 0x20);
    rt_w32(p + 4, 0x40);    /* DDPF_RGB */
    rt_w32(p + 0xc, 16);
    if (dx_sdl_is_565()) { rt_w32(p + 0x10, 0xf800); rt_w32(p + 0x14, 0x07e0); }
    else { rt_w32(p + 0x10, 0x7c00); rt_w32(p + 0x14, 0x03e0); }
    rt_w32(p + 0x18, 0x001f);
}

/* fills a DDSURFACEDESC (0x6c) for s; px != 0 also writes lpSurface */
static void write_desc(uint32_t desc, const Surf* s, uint32_t px)
{
    uint32_t size = rt_r32(desc);
    memset(rt_ptr(desc + 4), 0, 0x68);
    rt_w32(desc, size ? size : 0x6c);
    uint32_t fl = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT;
    if (s->nback) fl |= DDSD_BACKBUFFERCOUNT;
    if (s->primary) fl |= DDSD_REFRESHRATE;
    if (px) fl |= DDSD_LPSURFACE;
    rt_w32(desc + 4, fl);
    rt_w32(desc + 8, (uint32_t)s->h);
    rt_w32(desc + 0xc, (uint32_t)s->w);
    rt_w32(desc + 0x10, (uint32_t)s->pitch);
    rt_w32(desc + 0x14, (uint32_t)s->nback);
    if (s->primary) rt_w32(desc + 0x18, 60);
    rt_w32(desc + 0x24, px);
    write_pf(desc + 0x48);
    rt_w32(desc + 0x68, s->caps);
}

typedef struct { int x, y, w, h; } Rc;

static Rc rect_or_whole(uint32_t r, const Surf* s)
{
    Rc o = {0, 0, s->w, s->h};
    if (r) {
        int32_t l = (int32_t)rt_r32(r), t = (int32_t)rt_r32(r + 4), rr = (int32_t)rt_r32(r + 8), b = (int32_t)rt_r32(r + 12);
        o.x = l; o.y = t; o.w = rr - l; o.h = b - t;
    }
    return o;
}

static Rc clip(Rc r, const Surf* s)
{
    int x0 = r.x < 0 ? 0 : r.x, y0 = r.y < 0 ? 0 : r.y;
    int x1 = r.x + r.w > s->w ? s->w : r.x + r.w, y1 = r.y + r.h > s->h ? s->h : r.y + r.h;
    Rc o = {x0, y0, x1 - x0, y1 - y0};
    if (o.w < 0) o.w = 0;
    if (o.h < 0) o.h = 0;
    return o;
}

static void fill(Surf* s, Rc r, uint16_t c)
{
    r = clip(r, s);
    for (int y = 0; y < r.h; y++) {
        uint8_t* p = rt_ptr(s->px + (uint32_t)((r.y + y) * s->pitch + r.x * 2));
        for (int x = 0; x < r.w; x++) { p[2 * x] = (uint8_t)c; p[2 * x + 1] = (uint8_t)(c >> 8); }
    }
}

/* copies src rect sr of a into dst rect dr of d (nearest when the sizes differ); key: skip source
 * pixels in [lo, hi] */
static void copy(Surf* d, Rc dr, Surf* a, Rc sr, int key, uint32_t lo, uint32_t hi)
{
    if (dr.w <= 0 || dr.h <= 0 || sr.w <= 0 || sr.h <= 0) return;
    Rc dc = clip(dr, d), sc = clip(sr, a);
    int same = dr.w == sr.w && dr.h == sr.h;
    if (same && !key && dc.x == dr.x && dc.y == dr.y && dc.w == dr.w && dc.h == dr.h && sc.x == sr.x && sc.y == sr.y &&
        sc.w == sr.w && sc.h == sr.h) {
        /* whole rows; bottom up when copying down within one surface */
        int up = !(d == a && dr.y > sr.y);
        for (int k = 0; k < dr.h; k++) {
            int y = up ? k : dr.h - 1 - k;
            memmove(rt_ptr(d->px + (uint32_t)((dr.y + y) * d->pitch + dr.x * 2)),
                    rt_ptr(a->px + (uint32_t)((sr.y + y) * a->pitch + sr.x * 2)), (size_t)dr.w * 2);
        }
        return;
    }
    for (int y = dc.y; y < dc.y + dc.h; y++) {
        int sy = sr.y + (int)((int64_t)(y - dr.y) * sr.h / dr.h);
        if (sy < 0 || sy >= a->h) continue;
        uint8_t* dp = rt_ptr(d->px + (uint32_t)(y * d->pitch));
        const uint8_t* sp = rt_ptr(a->px + (uint32_t)(sy * a->pitch));
        for (int x = dc.x; x < dc.x + dc.w; x++) {
            int sx = sr.x + (int)((int64_t)(x - dr.x) * sr.w / dr.w);
            if (sx < 0 || sx >= a->w) continue;
            uint32_t v = (uint32_t)sp[2 * sx] | (uint32_t)sp[2 * sx + 1] << 8;
            if (key && v >= lo && v <= hi) continue;
            dp[2 * x] = (uint8_t)v;
            dp[2 * x + 1] = (uint8_t)(v >> 8);
        }
    }
}

/* the primary to the screen. A windowed primary (no SetDisplayMode) is shown to the extent the
 * game blitted into it (the client area); a full screen one whole. */
static void present(Surf* p, Rc dirty)
{
    int w = p->w, h = p->h;
    if (!D.mode_w) {
        Rc c = clip(dirty, p);
        if (c.x + c.w > p->ext_w) p->ext_w = c.x + c.w;
        if (c.y + c.h > p->ext_h) p->ext_h = c.y + c.h;
        if (p->ext_w > 0 && p->ext_h > 0) { w = p->ext_w; h = p->ext_h; }
    }
    if (qol_display_scale() > 1) {
        /* display.internal_scale: the renderer's large picture over this one */
        int ow, oh, op;
        const uint16_t* big = qol_gpu_compose(rt_ptr(p->px), w, h, p->pitch, &ow, &oh, &op);
        if (big) { dx_sdl_present_host(big, ow, oh, op, w, h); return; }
    }
    dx_sdl_present(p->px, w, h, p->pitch);
}

/* ---- surfaces ---- */

static void surf_destroy(DxObj* o)
{
    Surf* s = o->host;
    for (int k = 0; k < s->nback; k++) {
        DxObj* b = s->back[k];
        if (!b) continue;
        ((Surf*)b->host)->chain_front = NULL;
        b->refs = 1;   /* the chain goes with its primary, whatever references the game kept */
        dx_obj_release(b);
    }
    if (s->clipper) dx_obj_release(s->clipper);
    free(s->dc_copy);
    if (D.primary == o) D.primary = NULL;
    for (int k = 0; k < D.nsurfs; k++)
        if (D.surfs[k] == o) { D.surfs[k] = D.surfs[--D.nsurfs]; break; }
    dx_mem_free(s->px);
    free(s);
}

static DxObj* surf_new(int w, int h, uint32_t caps)
{
    if (D.nsurfs >= 256) return NULL;
    Surf* s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->w = w;
    s->h = h;
    s->pitch = (w * 2 + 3) & ~3;
    s->caps = caps;
    s->px = dx_mem_alloc((uint32_t)(s->pitch * h));
    if (!s->px) { free(s); return NULL; }
    DxObj* o = dx_obj_new(&I_SURF, s, surf_destroy);
    if (!o) { dx_mem_free(s->px); free(s); return NULL; }
    D.surfs[D.nsurfs++] = o;
    return o;
}

int dx_sdl_hdc_surface(uint32_t hdc, uint32_t* pixels, int* width, int* height, int* pitch)
{
    if (!hdc) return 0;
    for (int k = 0; k < D.nsurfs; k++) {
        Surf* s = D.surfs[k]->host;
        if (s->hdc != hdc) continue;
        *pixels = s->px;
        *width = s->w;
        *height = s->h;
        *pitch = s->pitch;
        return 1;
    }
    return 0;
}

/* 0x14 Blt(dst rect, src surface, src rect, flags, DDBLTFX*) */
static uint32_t s_blt(const uint32_t* a, int n)
{
    DxObj* od = dx_obj_get(dx_arg(a, n, 0), &I_SURF);
    if (!od) return DX_E_INVALIDARG;
    Surf* d = od->host;
    uint32_t drect = dx_arg(a, n, 1), src = dx_arg(a, n, 2), srect = dx_arg(a, n, 3);
    uint32_t flags = dx_arg(a, n, 4), fx = dx_arg(a, n, 5);
    Rc dr = rect_or_whole(drect, d);
    /* windowed (no SetDisplayMode): the game blits the whole offscreen surface to the client
     * rect in screen coordinates (display +0x78, present mode 0); the primary here is the client
     * area itself, so that rect is taken at 0, 0 */
    if (d->primary && !D.mode_w && drect && src && !srect) dr.x = dr.y = 0;
    if (flags & DDBLT_COLORFILL) {
        if (!fx) return DX_E_INVALIDARG;
        fill(d, dr, (uint16_t)rt_r32(fx + 0x50));
    } else if (src) {
        Surf* s = surf_of(src);
        if (!s) return DX_E_INVALIDARG;
        int key = (flags & DDBLT_KEYSRC) && s->has_ck;
        copy(d, dr, s, rect_or_whole(srect, s), key, s->ck_lo, s->ck_hi);
    } else {
        dx_log_once("IDirectDrawSurface::Blt: flags %08x without a source (ignored)", flags);
    }
    if (!d->primary && qol_display_scale() > 1) qol_gpu_overwritten(dr.x, dr.y, dr.w, dr.h, d->w, d->h);
    if (d->primary) present(d, dr);
    return DX_OK;
}

/* 0x1c BltFast(x, y, src surface, src rect, trans) */
static uint32_t s_bltfast(const uint32_t* a, int n)
{
    Surf* d = surf_of(dx_arg(a, n, 0));
    Surf* s = surf_of(dx_arg(a, n, 3));
    if (!d || !s) return DX_E_INVALIDARG;
    Rc sr = rect_or_whole(dx_arg(a, n, 4), s);
    Rc dr = {(int32_t)dx_arg(a, n, 1), (int32_t)dx_arg(a, n, 2), sr.w, sr.h};
    int key = (dx_arg(a, n, 5) & DDBLTFAST_SRCCOLORKEY) && s->has_ck;
    copy(d, dr, s, sr, key, s->ck_lo, s->ck_hi);
    if (!d->primary && qol_display_scale() > 1) qol_gpu_overwritten(dr.x, dr.y, dr.w, dr.h, d->w, d->h);
    if (d->primary) present(d, dr);
    return DX_OK;
}

/* 0x2c Flip(target override, flags): the chain's buffers move one step towards the front */
static uint32_t s_flip(const uint32_t* a, int n)
{
    Surf* p = surf_of(dx_arg(a, n, 0));
    if (!p || !p->primary || !p->nback) return DX_E_INVALIDARG;
    uint32_t front = p->px;
    p->px = ((Surf*)p->back[0]->host)->px;
    for (int k = 0; k + 1 < p->nback; k++) ((Surf*)p->back[k]->host)->px = ((Surf*)p->back[k + 1]->host)->px;
    ((Surf*)p->back[p->nback - 1]->host)->px = front;
    Rc whole = {0, 0, p->w, p->h};
    present(p, whole);
    return DX_OK;
}

/* 0x30 GetAttachedSurface(DDSCAPS*, out) */
static uint32_t s_get_attached(const uint32_t* a, int n)
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    uint32_t caps = dx_arg(a, n, 1), out = dx_arg(a, n, 2);
    if (!s || !out) return DX_E_INVALIDARG;
    uint32_t want = caps ? rt_r32(caps) : 0;
    DxObj* found = NULL;
    if (s->nback && (want & (DDSCAPS_BACKBUFFER | DDSCAPS_FLIP))) found = s->back[0];
    else if (s->chain_front) {
        /* a back buffer's next in the chain: the next back buffer, or the front */
        Surf* p = s->chain_front->host;
        for (int k = 0; k < p->nback; k++)
            if (p->back[k]->host == s) found = k + 1 < p->nback ? p->back[k + 1] : s->chain_front;
    }
    if (!found) { rt_w32(out, 0); return DDERR_NOTFOUND; }
    dx_obj_addref(found);
    rt_w32(out, found->addr);
    return DX_OK;
}

/* 0x24 EnumAttachedSurfaces(context, callback(surface, desc, context)) */
static uint32_t s_enum_attached(const uint32_t* a, int n)
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    uint32_t ctx = dx_arg(a, n, 1), cb = dx_arg(a, n, 2);
    if (!s || !cb) return DX_E_INVALIDARG;
    if (s->nback) {
        DxObj* b = s->back[0];
        uint32_t desc = rt_frame_push(0x6c);
        rt_w32(desc, 0x6c);
        write_desc(desc, b->host, 0);
        dx_obj_addref(b);
        uint32_t w[3] = {b->addr, desc, ctx};
        dx_call_cb(cb, w, 3);
        rt_frame_pop(0x6c);
    }
    return DX_OK;
}

static uint32_t s_getcaps(const uint32_t* a, int n)   /* 0x38 GetCaps(DDSCAPS*) */
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    uint32_t c = dx_arg(a, n, 1);
    if (!s || !c) return DX_E_INVALIDARG;
    rt_w32(c, s->caps);
    return DX_OK;
}

static uint32_t s_getclipper(const uint32_t* a, int n)   /* 0x3c GetClipper(out) */
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    uint32_t out = dx_arg(a, n, 1);
    if (!s || !out) return DX_E_INVALIDARG;
    if (!s->clipper) { rt_w32(out, 0); return DDERR_NOCLIPPERATTACHED; }
    dx_obj_addref(s->clipper);
    rt_w32(out, s->clipper->addr);
    return DX_OK;
}

static uint32_t s_setclipper(const uint32_t* a, int n)   /* 0x70 SetClipper(clipper or NULL) */
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    if (!s) return DX_E_INVALIDARG;
    DxObj* c = dx_obj_get(dx_arg(a, n, 1), &I_CLIP);
    if (dx_arg(a, n, 1) && !c) return DX_E_INVALIDARG;
    if (c) dx_obj_addref(c);
    if (s->clipper) dx_obj_release(s->clipper);
    s->clipper = c;
    return DX_OK;
}

static uint32_t s_getcolorkey(const uint32_t* a, int n)   /* 0x40 GetColorKey(flags, DDCOLORKEY*) */
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    uint32_t ck = dx_arg(a, n, 2);
    if (!s || !ck) return DX_E_INVALIDARG;
    if (!(dx_arg(a, n, 1) & DDCKEY_SRCBLT) || !s->has_ck) return DDERR_NOCOLORKEY;
    rt_w32(ck, s->ck_lo);
    rt_w32(ck + 4, s->ck_hi);
    return DX_OK;
}

static uint32_t s_setcolorkey(const uint32_t* a, int n)   /* 0x74 SetColorKey(flags, DDCOLORKEY* or NULL) */
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    uint32_t ck = dx_arg(a, n, 2);
    if (!s) return DX_E_INVALIDARG;
    if (!(dx_arg(a, n, 1) & DDCKEY_SRCBLT)) return DX_OK;   /* destination / overlay keys: not used */
    s->has_ck = ck != 0;
    if (ck) {
        s->ck_lo = rt_r32(ck) & 0xffff;
        s->ck_hi = rt_r32(ck + 4) & 0xffff;
        if (s->ck_hi < s->ck_lo) s->ck_hi = s->ck_lo;
    }
    return DX_OK;
}

static uint32_t s_getdc(const uint32_t* a, int n)   /* 0x44 GetDC(HDC*) */
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    uint32_t out = dx_arg(a, n, 1);
    if (!s || !out) return DX_E_INVALIDARG;
    if (!s->hdc) s->hdc = rt_fake_alloc(16);
    rt_w32(out, s->hdc);
    if (qol_display_scale() > 1 && !s->primary) {
        /* display.internal_scale: what GDI writes until ReleaseDC is found by comparing */
        size_t sz = (size_t)s->pitch * (size_t)s->h;
        if (!s->dc_copy) s->dc_copy = malloc(sz);
        if (s->dc_copy) memcpy(s->dc_copy, rt_ptr(s->px), sz);
    }
    return DX_OK;
}

static uint32_t s_releasedc(const uint32_t* a, int n)   /* 0x68 ReleaseDC(hdc) */
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    if (s && s->dc_copy && qol_display_scale() > 1) {
        const uint8_t* now = rt_ptr(s->px);
        int x0 = s->w, y0 = s->h, x1 = -1, y1 = -1;
        for (int y = 0; y < s->h; y++)
            for (int x = 0; x < s->w; x++) {
                size_t o = (size_t)y * (size_t)s->pitch + 2u * (size_t)x;
                if (now[o] == s->dc_copy[o] && now[o + 1] == s->dc_copy[o + 1]) continue;
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
        if (x1 >= 0) qol_gpu_overwritten(x0, y0, x1 - x0 + 1, y1 - y0 + 1, s->w, s->h);
        free(s->dc_copy);
        s->dc_copy = NULL;
    }
    return s ? DX_OK : DX_E_INVALIDARG;
}

static uint32_t s_getpalette(const uint32_t* a, int n)   /* 0x50 GetPalette(out) */
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    uint32_t out = dx_arg(a, n, 1);
    if (out) rt_w32(out, 0);
    return s ? DDERR_NOPALETTEATTACHED : DX_E_INVALIDARG;
}

static uint32_t s_setpalette(const uint32_t* a, int n)   /* 0x7c SetPalette(palette): kept, unused (16 bit) */
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    if (!s) return DX_E_INVALIDARG;
    s->palette = dx_arg(a, n, 1);
    return DX_OK;
}

static uint32_t s_getpixelformat(const uint32_t* a, int n)   /* 0x54 GetPixelFormat(DDPIXELFORMAT*) */
{
    uint32_t p = dx_arg(a, n, 1);
    if (!surf_of(dx_arg(a, n, 0)) || !p) return DX_E_INVALIDARG;
    write_pf(p);
    return DX_OK;
}

static uint32_t s_getsurfacedesc(const uint32_t* a, int n)   /* 0x58 GetSurfaceDesc(DDSURFACEDESC*) */
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    uint32_t desc = dx_arg(a, n, 1);
    if (!s || !desc) return DX_E_INVALIDARG;
    write_desc(desc, s, 0);
    return DX_OK;
}

/* 0x64 Lock(RECT* or NULL, DDSURFACEDESC*, flags, event): the pixels' game address (at the
 * rectangle's top left, as DirectDraw does) and the pitch */
static uint32_t s_lock(const uint32_t* a, int n)
{
    Surf* s = surf_of(dx_arg(a, n, 0));
    uint32_t r = dx_arg(a, n, 1), desc = dx_arg(a, n, 2);
    if (!s || !desc) return DX_E_INVALIDARG;
    uint32_t px = s->px;
    if (r) {
        Rc c = clip(rect_or_whole(r, s), s);
        px += (uint32_t)(c.y * s->pitch + c.x * 2);
    }
    write_desc(desc, s, px);
    return DX_OK;
}

static uint32_t s_unlock(const uint32_t* a, int n)   /* 0x80 Unlock(pointer or RECT*) */
{
    return surf_of(dx_arg(a, n, 0)) ? DX_OK : DX_E_INVALIDARG;
}

/* IsLost, Restore, GetBltStatus, GetFlipStatus, PageLock / PageUnlock: system memory, never lost */
static uint32_t s_ok(const uint32_t* a, int n) { return surf_of(dx_arg(a, n, 0)) ? DX_OK : DX_E_INVALIDARG; }

static uint32_t s_unsupported(const uint32_t* a, int n)   /* overlays */
{
    (void)a; (void)n;
    return DX_E_NOTIMPL;
}

static uint32_t s_initialize(const uint32_t* a, int n) { (void)a; (void)n; return DDERR_ALREADYINITIALIZED; }

static uint32_t s_add_attached(const uint32_t* a, int n)   /* 0x0c AddAttachedSurface: z buffers etc. */
{
    (void)a; (void)n;
    dx_log_once("IDirectDrawSurface::AddAttachedSurface (ignored)");
    return DX_OK;
}

static uint32_t s_get_dd(const uint32_t* a, int n)   /* 0x90 GetDDInterface(out) (Surface2) */
{
    (void)a; (void)n;
    return DX_E_NOTIMPL;
}

/* ---- clipper ---- */

static Clip* clip_of(uint32_t a) { DxObj* o = dx_obj_get(a, &I_CLIP); return o ? o->host : NULL; }
static void clip_destroy(DxObj* o) { free(o->host); }

static uint32_t c_sethwnd(const uint32_t* a, int n)   /* 0x20 SetHWnd(flags, hwnd) */
{
    Clip* c = clip_of(dx_arg(a, n, 0));
    if (!c) return DX_E_INVALIDARG;
    c->hwnd = dx_arg(a, n, 2);
    return DX_OK;
}

static uint32_t c_gethwnd(const uint32_t* a, int n)   /* 0x10 GetHWnd(out) */
{
    Clip* c = clip_of(dx_arg(a, n, 0));
    uint32_t out = dx_arg(a, n, 1);
    if (!c || !out) return DX_E_INVALIDARG;
    rt_w32(out, c->hwnd);
    return DX_OK;
}

static uint32_t c_getcliplist(const uint32_t* a, int n) { (void)a; (void)n; return DDERR_NOCLIPLIST; }
static uint32_t c_ischanged(const uint32_t* a, int n)   /* 0x18 IsClipListChanged(BOOL*) */
{
    if (dx_arg(a, n, 1)) rt_w32(dx_arg(a, n, 1), 0);
    return DX_OK;
}
static uint32_t c_ok(const uint32_t* a, int n) { (void)a; (void)n; return DX_OK; }

/* ---- IDirectDraw / IDirectDraw2 ---- */

static uint32_t dd_create_surface(const uint32_t* a, int n)   /* 0x18 CreateSurface(desc, out, outer) */
{
    uint32_t desc = dx_arg(a, n, 1), out = dx_arg(a, n, 2);
    if (!desc || !out) return DX_E_INVALIDARG;
    rt_w32(out, 0);
    uint32_t fl = rt_r32(desc + 4), caps = (fl & DDSD_CAPS) ? rt_r32(desc + 0x68) : 0;
    int w = (fl & DDSD_WIDTH) ? (int)rt_r32(desc + 0xc) : 0, h = (fl & DDSD_HEIGHT) ? (int)rt_r32(desc + 8) : 0;
    int nback = (fl & DDSD_BACKBUFFERCOUNT) ? (int)rt_r32(desc + 0x14) : 0;
    if ((fl & DDSD_PIXELFORMAT) && rt_r32(desc + 0x54) != 16 && rt_r32(desc + 0x54) != 0) {
        dx_log_once("CreateSurface: %u bit pixel format refused (16 bit only)", rt_r32(desc + 0x54));
        return DX_E_INVALIDARG;
    }
    uint32_t mem = (caps & DDSCAPS_VIDEOMEMORY) ? DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM : DDSCAPS_SYSTEMMEMORY;
    if (caps & DDSCAPS_PRIMARYSURFACE) {
        if (D.primary) return 0x887600c8u;   /* DDERR_PRIMARYSURFACEALREADYEXISTS */
        dx_sdl_client_size(&w, &h);
        if (D.mode_w) { w = D.mode_w; h = D.mode_h; }
        else if (qol_display_aspect() > 0) {
            /* display.widescreen, windowed: the client area takes the widest picture (the game's
             * wide 640 x 480 mode); the picture shown is the extent blitted (present) */
            int ww = qol_display_wide_width(640, 480);
            if (w < ww) w = ww;
            if (h < 480) h = 480;
        }
        if (nback < 0 || nback > 2) return DX_E_INVALIDARG;
        uint32_t pc = (caps & ~(DDSCAPS_SYSTEMMEMORY | DDSCAPS_VIDEOMEMORY)) | mem | DDSCAPS_VISIBLE;
        if (nback) pc |= DDSCAPS_FRONTBUFFER;
        DxObj* o = surf_new(w, h, pc);
        if (!o) return 0x8007000eu;   /* DDERR_OUTOFMEMORY */
        Surf* s = o->host;
        s->primary = 1;
        for (int k = 0; k < nback; k++) {
            uint32_t bc = (caps & ~(DDSCAPS_PRIMARYSURFACE | DDSCAPS_SYSTEMMEMORY | DDSCAPS_VIDEOMEMORY)) | mem;
            if (k == 0) bc |= DDSCAPS_BACKBUFFER;
            DxObj* b = surf_new(w, h, bc);
            if (!b) { dx_obj_release(o); return 0x8007000eu; }
            ((Surf*)b->host)->chain_front = o;
            s->back[s->nback++] = b;
        }
        D.primary = o;
        rt_w32(out, o->addr);
        return DX_OK;
    }
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return DX_E_INVALIDARG;
    uint32_t sc = (caps & ~(DDSCAPS_SYSTEMMEMORY | DDSCAPS_VIDEOMEMORY)) | mem;
    if (!(sc & 0x1000u)) sc |= DDSCAPS_OFFSCREENPLAIN;   /* unless DDSCAPS_TEXTURE */
    DxObj* o = surf_new(w, h, sc);
    if (!o) return 0x8007000eu;
    rt_w32(out, o->addr);
    return DX_OK;
}

static uint32_t dd_create_clipper(const uint32_t* a, int n)   /* 0x10 CreateClipper(flags, out, outer) */
{
    uint32_t out = dx_arg(a, n, 2);
    if (!out) return DX_E_INVALIDARG;
    Clip* c = calloc(1, sizeof *c);
    DxObj* o = c ? dx_obj_new(&I_CLIP, c, clip_destroy) : NULL;
    if (!o) { free(c); rt_w32(out, 0); return 0x8007000eu; }
    rt_w32(out, o->addr);
    return DX_OK;
}

static uint32_t dd_create_palette(const uint32_t* a, int n)   /* 0x14: no 8 bit modes */
{
    if (dx_arg(a, n, 3)) rt_w32(dx_arg(a, n, 3), 0);
    dx_log_once("IDirectDraw::CreatePalette refused (16 bit only)");
    return DX_E_NOTIMPL;
}

static uint32_t dd_duplicate_surface(const uint32_t* a, int n)   /* 0x1c */
{
    if (dx_arg(a, n, 2)) rt_w32(dx_arg(a, n, 2), 0);
    return DX_E_NOTIMPL;
}

/* 0x20 EnumDisplayModes(flags, DDSURFACEDESC* filter, context, callback(desc, context)) */
static uint32_t dd_enum_modes(const uint32_t* a, int n)
{
    uint32_t filt = dx_arg(a, n, 2), ctx = dx_arg(a, n, 3), cb = dx_arg(a, n, 4);
    if (!cb) return DX_E_INVALIDARG;
    uint32_t desc = rt_frame_push(0x6c);
    for (size_t k = 0; k < sizeof MODES / sizeof MODES[0]; k++) {
        if (filt) {
            uint32_t ff = rt_r32(filt + 4);
            if ((ff & DDSD_WIDTH) && rt_r32(filt + 0xc) != (uint32_t)MODES[k].w) continue;
            if ((ff & DDSD_HEIGHT) && rt_r32(filt + 8) != (uint32_t)MODES[k].h) continue;
        }
        Surf m = {0};
        m.w = MODES[k].w;
        m.h = MODES[k].h;
        m.pitch = m.w * 2;
        m.primary = 1;
        rt_w32(desc, 0x6c);
        write_desc(desc, &m, 0);
        rt_w32(desc + 4, DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_REFRESHRATE);
        rt_w32(desc + 0x68, 0);
        uint32_t w[2] = {desc, ctx};
        uint32_t before = ctx ? rt_r32(ctx) : 0;
        uint32_t ret = dx_call_cb(cb, w, 2);
        int wide = qol_display_wide_width(m.w, m.h);
        if (wide != m.w && ctx && rt_r32(ctx) == before + 1 && before < 30) {
            /* display.widescreen: the game's callback (0x004b592c, docs/PLATFORM.md) keeps modes
             * no wider than 640 in its list (count, then {w, h, bpp, refresh} entries); a 4:3
             * mode it kept gets the wide width there, so the game sets it (the match's display
             * modes) and sizes its views for it */
            uint32_t e = ctx + 4 + 16 * before;
            if (rt_r32(e) == (uint32_t)m.w && rt_r32(e + 4) == (uint32_t)m.h) rt_w32(e, (uint32_t)wide);
        }
        if (ret == 0) break;   /* DDENUMRET_CANCEL */
    }
    rt_frame_pop(0x6c);
    return DX_OK;
}

/* 0x2c GetCaps(hardware DDCAPS*, emulation DDCAPS*): a 2D card, no 3D */
static void write_caps(uint32_t c)
{
    if (!c) return;
    uint32_t size = rt_r32(c);
    if (size < 0x88) return;
    memset(rt_ptr(c + 4), 0, size - 4);
    rt_w32(c + 0x04, 0x00000040u | 0x00000200u | 0x00400000u | 0x04000000u);   /* BLT, BLTSTRETCH, COLORKEY, BLTCOLORFILL */
    rt_w32(c + 0x08, 0);
    rt_w32(c + 0x0c, 0x00000200u);    /* DDCKEYCAPS_SRCBLT */
    rt_w32(c + 0x3c, 0x00400000u);    /* dwVidMemTotal 4 MB */
    rt_w32(c + 0x40, 0x00400000u);    /* dwVidMemFree */
    rt_w32(c + 0x84, DDSCAPS_BACKBUFFER | DDSCAPS_COMPLEX | DDSCAPS_FLIP | DDSCAPS_FRONTBUFFER | DDSCAPS_OFFSCREENPLAIN |
                         DDSCAPS_PRIMARYSURFACE | DDSCAPS_SYSTEMMEMORY | DDSCAPS_VIDEOMEMORY | DDSCAPS_VISIBLE);
}

static uint32_t dd_getcaps(const uint32_t* a, int n)
{
    write_caps(dx_arg(a, n, 1));
    write_caps(dx_arg(a, n, 2));
    return DX_OK;
}

static uint32_t dd_get_display_mode(const uint32_t* a, int n)   /* 0x30 GetDisplayMode(desc) */
{
    uint32_t desc = dx_arg(a, n, 1);
    if (!desc) return DX_E_INVALIDARG;
    Surf m = {0};
    dx_sdl_client_size(&m.w, &m.h);
    if (D.mode_w) { m.w = D.mode_w; m.h = D.mode_h; }
    m.pitch = m.w * 2;
    m.primary = 1;
    write_desc(desc, &m, 0);
    return DX_OK;
}

static uint32_t dd_get_fourcc(const uint32_t* a, int n)   /* 0x34 GetFourCCCodes(&count, codes) */
{
    if (dx_arg(a, n, 1)) rt_w32(dx_arg(a, n, 1), 0);
    return DX_OK;
}

static uint32_t dd_get_gdi_surface(const uint32_t* a, int n)   /* 0x38 GetGDISurface(out) */
{
    uint32_t out = dx_arg(a, n, 1);
    if (!out) return DX_E_INVALIDARG;
    if (!D.primary) { rt_w32(out, 0); return DDERR_NOTFOUND; }
    dx_obj_addref(D.primary);
    rt_w32(out, D.primary->addr);
    return DX_OK;
}

static uint32_t dd_get_monitor_frequency(const uint32_t* a, int n)   /* 0x3c (&hz) */
{
    uint32_t out = dx_arg(a, n, 1);
    if (!out) return DX_E_INVALIDARG;
    rt_w32(out, D.mode_hz ? (uint32_t)D.mode_hz : 60u);
    return DX_OK;
}

static uint32_t dd_get_scanline(const uint32_t* a, int n)   /* 0x40: DDERR_UNSUPPORTED */
{
    (void)a; (void)n;
    return DX_E_NOTIMPL;
}

static uint32_t dd_get_vblank_status(const uint32_t* a, int n)   /* 0x44 (&BOOL in vertical blank) */
{
    if (dx_arg(a, n, 1)) rt_w32(dx_arg(a, n, 1), (SDL_GetTicks() % 17) < 2);
    return DX_OK;
}

static uint32_t dd_restore_display_mode(const uint32_t* a, int n)   /* 0x4c */
{
    (void)a; (void)n;
    D.mode_w = D.mode_h = D.mode_bpp = D.mode_hz = 0;
    dx_sdl_set_mode(0, 0, 0);
    return DX_OK;
}

static uint32_t dd_set_coop(const uint32_t* a, int n)   /* 0x50 SetCooperativeLevel(hwnd, flags) */
{
    D.hwnd = dx_arg(a, n, 1);
    D.coop = dx_arg(a, n, 2);
    dx_sdl_set_hwnd(D.hwnd);
    return DX_OK;
}

/* 0x54 SetDisplayMode(w, h, bpp[, refresh, flags] (IDirectDraw2)) */
static uint32_t dd_set_display_mode(const uint32_t* a, int n)
{
    int w = (int)dx_arg(a, n, 1), h = (int)dx_arg(a, n, 2), bpp = (int)dx_arg(a, n, 3);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return DX_E_INVALIDARG;
    if (bpp != 16) {
        dx_log_once("SetDisplayMode %dx%dx%d refused (16 bit only)", w, h, bpp);
        return 0x88760244u;   /* DDERR_UNSUPPORTEDMODE */
    }
    D.mode_w = w;
    D.mode_h = h;
    D.mode_bpp = bpp;
    D.mode_hz = (int)dx_arg(a, n, 4);
    dx_sdl_set_mode(w, h, (D.coop & 1u) != 0);   /* DDSCL_FULLSCREEN */
    return DX_OK;
}

/* 0x58 WaitForVerticalBlank(flags, event): waits for the next 60 Hz tick (TM2_VBLANK=0: no wait) */
static uint32_t dd_wait_vblank(const uint32_t* a, int n)
{
    (void)a; (void)n;
    static int on = -1;
    if (on < 0) on = settings_bool("debug.vblank", 1);
    if (!on) return DX_OK;
    uint64_t now = SDL_GetTicks();
    uint64_t next = (now * 60u / 1000u + 1u) * 1000u / 60u;   /* the next 1/60 s boundary */
    if (next > now && next - now <= 17) SDL_Delay((Uint32)(next - now));
    return DX_OK;
}

static uint32_t dd_get_vidmem(const uint32_t* a, int n)   /* 0x5c GetAvailableVidMem(caps, &total, &free) */
{
    if (dx_arg(a, n, 2)) rt_w32(dx_arg(a, n, 2), 0x00400000u);
    if (dx_arg(a, n, 3)) rt_w32(dx_arg(a, n, 3), 0x00400000u);
    return DX_OK;
}

static uint32_t dd_ok(const uint32_t* a, int n) { (void)a; (void)n; return DX_OK; }
static uint32_t dd_initialize(const uint32_t* a, int n) { (void)a; (void)n; return DDERR_ALREADYINITIALIZED; }

/* QueryInterface between IDirectDraw and IDirectDraw2 makes the other object; Direct3D is
 * refused (see the head of this file) */
static int qi_hook(DxObj* o, uint32_t d1, uint32_t out, uint32_t* hr)
{
    if ((o->iface == &I_DD && d1 == I_DD2.iids[0]) || (o->iface == &I_DD2 && d1 == I_DD.iids[0])) {
        DxObj* x = dx_obj_new(o->iface == &I_DD ? &I_DD2 : &I_DD, NULL, NULL);
        if (!x) { rt_w32(out, 0); *hr = 0x8007000eu; return 1; }
        rt_w32(out, x->addr);
        *hr = DX_OK;
        return 1;
    }
    if (o->iface == &I_DD || o->iface == &I_DD2) {
        if (d1 == 0x3bba0080u || d1 == 0x6aae1ec1u || d1 == 0xbb223240u) {
            dx_log_once("no Direct3D: the game uses its software renderer");
            rt_w32(out, 0);
            *hr = DX_E_NOINTERFACE;
            return 1;
        }
    }
    return 0;
}

/* DirectDrawCreate(GUID*, IDirectDraw** out, outer), DDRAW.dll */
static uint32_t imp_directdraw_create(const uint32_t* a, int n)
{
    uint32_t out = dx_arg(a, n, 1);
    if (!out) return DX_E_INVALIDARG;
    DxObj* o = dx_obj_new(&I_DD, NULL, NULL);
    if (!o) { rt_w32(out, 0); return 0x8007000eu; }
    rt_w32(out, o->addr);
    return DX_OK;
}

static const DxMethod M_DD[] = {
    {0x00, dx_unk_qi, "IDirectDraw::QueryInterface"},
    {0x04, dx_unk_addref, "IDirectDraw::AddRef"},
    {0x08, dx_unk_release, "IDirectDraw::Release"},
    {0x0c, dd_ok, "IDirectDraw::Compact"},
    {0x10, dd_create_clipper, "IDirectDraw::CreateClipper"},
    {0x14, dd_create_palette, "IDirectDraw::CreatePalette"},
    {0x18, dd_create_surface, "IDirectDraw::CreateSurface"},
    {0x1c, dd_duplicate_surface, "IDirectDraw::DuplicateSurface"},
    {0x20, dd_enum_modes, "IDirectDraw::EnumDisplayModes"},
    {0x24, dd_ok, "IDirectDraw::EnumSurfaces"},
    {0x28, dd_ok, "IDirectDraw::FlipToGDISurface"},
    {0x2c, dd_getcaps, "IDirectDraw::GetCaps"},
    {0x30, dd_get_display_mode, "IDirectDraw::GetDisplayMode"},
    {0x34, dd_get_fourcc, "IDirectDraw::GetFourCCCodes"},
    {0x38, dd_get_gdi_surface, "IDirectDraw::GetGDISurface"},
    {0x3c, dd_get_monitor_frequency, "IDirectDraw::GetMonitorFrequency"},
    {0x40, dd_get_scanline, "IDirectDraw::GetScanLine"},
    {0x44, dd_get_vblank_status, "IDirectDraw::GetVerticalBlankStatus"},
    {0x48, dd_initialize, "IDirectDraw::Initialize"},
    {0x4c, dd_restore_display_mode, "IDirectDraw::RestoreDisplayMode"},
    {0x50, dd_set_coop, "IDirectDraw::SetCooperativeLevel"},
    {0x54, dd_set_display_mode, "IDirectDraw::SetDisplayMode"},
    {0x58, dd_wait_vblank, "IDirectDraw::WaitForVerticalBlank"},
    {0x5c, dd_get_vidmem, "IDirectDraw2::GetAvailableVidMem"},
};

static const DxMethod M_SURF[] = {
    {0x00, dx_unk_qi, "IDirectDrawSurface::QueryInterface"},
    {0x04, dx_unk_addref, "IDirectDrawSurface::AddRef"},
    {0x08, dx_unk_release, "IDirectDrawSurface::Release"},
    {0x0c, s_add_attached, "IDirectDrawSurface::AddAttachedSurface"},
    {0x10, s_unsupported, "IDirectDrawSurface::AddOverlayDirtyRect"},
    {0x14, s_blt, "IDirectDrawSurface::Blt"},
    {0x18, s_unsupported, "IDirectDrawSurface::BltBatch"},
    {0x1c, s_bltfast, "IDirectDrawSurface::BltFast"},
    {0x20, s_ok, "IDirectDrawSurface::DeleteAttachedSurface"},
    {0x24, s_enum_attached, "IDirectDrawSurface::EnumAttachedSurfaces"},
    {0x28, s_unsupported, "IDirectDrawSurface::EnumOverlayZOrders"},
    {0x2c, s_flip, "IDirectDrawSurface::Flip"},
    {0x30, s_get_attached, "IDirectDrawSurface::GetAttachedSurface"},
    {0x34, s_ok, "IDirectDrawSurface::GetBltStatus"},
    {0x38, s_getcaps, "IDirectDrawSurface::GetCaps"},
    {0x3c, s_getclipper, "IDirectDrawSurface::GetClipper"},
    {0x40, s_getcolorkey, "IDirectDrawSurface::GetColorKey"},
    {0x44, s_getdc, "IDirectDrawSurface::GetDC"},
    {0x48, s_ok, "IDirectDrawSurface::GetFlipStatus"},
    {0x4c, s_unsupported, "IDirectDrawSurface::GetOverlayPosition"},
    {0x50, s_getpalette, "IDirectDrawSurface::GetPalette"},
    {0x54, s_getpixelformat, "IDirectDrawSurface::GetPixelFormat"},
    {0x58, s_getsurfacedesc, "IDirectDrawSurface::GetSurfaceDesc"},
    {0x5c, s_initialize, "IDirectDrawSurface::Initialize"},
    {0x60, s_ok, "IDirectDrawSurface::IsLost"},
    {0x64, s_lock, "IDirectDrawSurface::Lock"},
    {0x68, s_releasedc, "IDirectDrawSurface::ReleaseDC"},
    {0x6c, s_ok, "IDirectDrawSurface::Restore"},
    {0x70, s_setclipper, "IDirectDrawSurface::SetClipper"},
    {0x74, s_setcolorkey, "IDirectDrawSurface::SetColorKey"},
    {0x78, s_unsupported, "IDirectDrawSurface::SetOverlayPosition"},
    {0x7c, s_setpalette, "IDirectDrawSurface::SetPalette"},
    {0x80, s_unlock, "IDirectDrawSurface::Unlock"},
    {0x84, s_unsupported, "IDirectDrawSurface::UpdateOverlay"},
    {0x88, s_unsupported, "IDirectDrawSurface::UpdateOverlayDisplay"},
    {0x8c, s_unsupported, "IDirectDrawSurface::UpdateOverlayZOrder"},
    {0x90, s_get_dd, "IDirectDrawSurface2::GetDDInterface"},
    {0x94, s_ok, "IDirectDrawSurface2::PageLock"},
    {0x98, s_ok, "IDirectDrawSurface2::PageUnlock"},
};

static const DxMethod M_CLIP[] = {
    {0x00, dx_unk_qi, "IDirectDrawClipper::QueryInterface"},
    {0x04, dx_unk_addref, "IDirectDrawClipper::AddRef"},
    {0x08, dx_unk_release, "IDirectDrawClipper::Release"},
    {0x0c, c_getcliplist, "IDirectDrawClipper::GetClipList"},
    {0x10, c_gethwnd, "IDirectDrawClipper::GetHWnd"},
    {0x14, dd_initialize, "IDirectDrawClipper::Initialize"},
    {0x18, c_ischanged, "IDirectDrawClipper::IsClipListChanged"},
    {0x1c, c_ok, "IDirectDrawClipper::SetClipList"},
    {0x20, c_sethwnd, "IDirectDrawClipper::SetHWnd"},
};

#define NM(m) ((int)(sizeof m / sizeof m[0]))

static void rt_bind_host_dx_ddraw(void)
{
    /* IDirectDraw and IDirectDraw2 share one method table; the PF2_COM ids (the pf2 ports only
     * name IDirectDraw2) are bound with the IDirectDraw2 vtable */
    dx_iface_build(&I_DD, M_DD, NM(M_DD), 0x60 / 4, 0);
    dx_iface_build(&I_DD2, M_DD, NM(M_DD), 0x60 / 4, PF2_IDD2);
    dx_iface_build(&I_SURF, M_SURF, NM(M_SURF), 0x9c / 4, PF2_ISURF);
    dx_iface_build(&I_CLIP, M_CLIP, NM(M_CLIP), 0x24 / 4, PF2_ICLIP);
    dx_qi_hook = qi_hook;
    rt_bind_import("DirectDrawCreate", imp_directdraw_create);
}

RT_BIND_AREA(host_dx_ddraw)
