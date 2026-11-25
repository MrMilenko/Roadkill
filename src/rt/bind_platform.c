/*
 * bind_platform.c: the typed platform ports of src/platform (pf_display, pf_dsound, pf_mouse,
 * pf_sfx, pf2_d3d) and the pf5 C runtime / voice list entries the T(...) generator did not
 * record, called as their emulator diffs called them (tools/emu/diff_pf_*.py, diff_pf2_d3d.py,
 * diff_pf5_crt.py, diff_pf5_sfx.py).
 *
 * The typed ports keep host copies of the original's objects (PfDisplay, PfDsDevice, PfMouse,
 * PfSfxSlot, ...): each adapter loads the copy from game memory, calls the port, and stores back
 * the fields the port may change. Their opaque handles (PfSurface*, PfDsObj*, buffer handles)
 * are the original's 32 bit game addresses cast to pointers; the backends cast them back.
 * Each backend function makes the Win32 / COM call the original made at that point:
 * PF2_COM(interface, vtable offset) with a[0] = the object, or the import's IAT slot, with the
 * structures the original built on its stack (DDSURFACEDESC, DDBLTFX, POINT, ...) built in a
 * runtime frame. Game functions the ports call back go through rt_call by original address,
 * with the registers the harness recorded.
 */
#include "rt_env.h"
#include "platform/pf_display.h"
#include "platform/pf_dsound.h"
#include "platform/pf_mouse.h"
#include "platform/pf_sfx.h"
#include "platform/pf2_d3d.h"
#include "platform/pf5_crt.h"
#include "platform/pf5_sfx.h"

#include <stdlib.h>

#define A(i) rt_arg(a, n, (i))
#define I(i) ((int32_t)rt_arg(a, n, (i)))
#define H(p) ((uint32_t)(uintptr_t)(p))            /* an opaque handle back to its game address */
#define P(T, v) ((T*)(uintptr_t)(uint32_t)(v))     /* a game address as an opaque handle */

static uint32_t call0(uint32_t fn) { return rt_call(fn, NULL, 0); }
static uint32_t call1(uint32_t fn, uint32_t x) { return rt_call(fn, &x, 1); }
static uint32_t call2(uint32_t fn, uint32_t x, uint32_t y) { uint32_t v[2] = {x, y}; return rt_call(fn, v, 2); }
static uint32_t call3(uint32_t fn, uint32_t x, uint32_t y, uint32_t z)
{ uint32_t v[3] = {x, y, z}; return rt_call(fn, v, 3); }

/* a host buffer as game memory: its own address when it is in the address space, else a copy
 * in a frame (copied back by gbuf_done when out is set) */
typedef struct { uint32_t addr, size; void* host; int pushed; } GBuf;
static GBuf gbuf(void* host, uint32_t size)
{
    GBuf b = {0, size, host, 0};
    if (!host) return b;
    uint8_t* p = host;
    if (p >= rt_base && p + size <= rt_base + RT_SIZE) { b.addr = rt_addr(p); return b; }
    b.addr = rt_frame_push(size);
    b.pushed = 1;
    memcpy(rt_ptr(b.addr), host, size);
    return b;
}
static void gbuf_done(GBuf* b, int out)
{
    if (!b->pushed) return;
    if (out) memcpy(b->host, rt_ptr(b->addr), b->size);
    rt_frame_pop(b->size);
}

/* IAT slots, looked up at bind time */
static uint32_t s_GetSystemMetrics, s_GetAsyncKeyState, s_GetCursorPos, s_ScreenToClient, s_ClientToScreen,
    s_SetCursorPos;

/* ======================================================================================
 * pf_display: the display object 0x00c84840 (PF2_DISPLAY)
 * ==================================================================================== */

#define G_PIX 0x00a04674u
#define G_PITCH 0x00a04670u
#define G_DEV3D 0x00c85008u

typedef struct { PfDisplay* d; uint32_t obj; } DispCtx;

/* the port's state as the original's memory, before a nested call reads it */
static void disp_store(const DispCtx* c)
{
    rt_w32(c->obj + 0x60, (uint32_t)c->d->pitch);
    rt_w32(G_PIX, (uint32_t)c->d->screen_pixels);
    rt_w32(G_PITCH, (uint32_t)c->d->screen_pitch);
}

static int32_t db_lock(void* u, PfSurface* s, const void* rect, uintptr_t* pixels, int32_t* pitch)
{
    (void)u;
    uint32_t desc = rt_frame_push(0x6c);
    rt_w32(desc, 0x6c);   /* dwSize, the rest zeroed */
    uint32_t v[5] = {H(s), H(rect), desc, PF_DDLOCK_WAIT, 0};
    int32_t r = (int32_t)rt_call(PF2_COM(PF2_ISURF, 0x64), v, 5);
    if (r == 0) { *pixels = rt_r32(desc + 0x24); *pitch = (int32_t)rt_r32(desc + 0x10); }
    rt_frame_pop(0x6c);
    return r;
}
static int32_t db_unlock(void* u, PfSurface* s, const void* rect)
{ (void)u; return (int32_t)call2(PF2_COM(PF2_ISURF, 0x80), H(s), H(rect)); }
static int32_t db_is_lost(void* u, PfSurface* s) { (void)u; return (int32_t)call1(PF2_COM(PF2_ISURF, 0x60), H(s)); }
static int32_t db_restore(void* u, PfSurface* s) { (void)u; return (int32_t)call1(PF2_COM(PF2_ISURF, 0x6c), H(s)); }
static int32_t db_blt(void* u, PfSurface* dst, const PfRect* dr, PfSurface* src, const PfRect* sr, uint32_t flags,
                      uint32_t fill)
{
    DispCtx* c = u;
    /* the only rect the port passes is the display's client rect (+0x78) */
    uint32_t dra = dr == &c->d->client ? c->obj + 0x78 : H(dr);
    uint32_t fx = 0;
    if (flags & PF_DDBLT_COLORFILL) {
        fx = rt_frame_push(100);
        rt_w32(fx, 100);
        rt_w32(fx + 0x50, fill);
    }
    uint32_t v[6] = {H(dst), dra, H(src), H(sr), flags, fx};
    int32_t r = (int32_t)rt_call(PF2_COM(PF2_ISURF, 0x14), v, 6);
    if (fx) rt_frame_pop(100);
    return r;
}
static int32_t db_flip(void* u, PfSurface* s, PfSurface* t, uint32_t flags)
{ (void)u; return (int32_t)call3(PF2_COM(PF2_ISURF, 0x2c), H(s), H(t), flags); }
static void db_set_pixels(void* u, uintptr_t px) { disp_store(u); call1(0x004ad82cu, (uint32_t)px); }
static void db_set_pitch(void* u, int32_t pitch) { disp_store(u); call1(0x004ad760u, (uint32_t)pitch); }
static int32_t db_reload(void* u) { disp_store(u); return (int32_t)call0(0x00497390u); }
/* 0x004b45a0(eax display, edx w, ebx h, ecx bpp, stack extra) */
static int32_t db_create(void* u, int32_t w, int32_t h, int32_t bpp, int32_t extra)
{
    DispCtx* c = u;
    disp_store(c);
    uint32_t v[5] = {c->obj, (uint32_t)w, (uint32_t)h, (uint32_t)bpp, (uint32_t)extra};
    return (int32_t)rt_call(0x004b45a0u, v, 5);
}

static PfDisplayBackend s_disp_be = {NULL, db_lock, db_unlock, db_is_lost, db_restore, db_blt, db_flip,
                                     db_set_pixels, db_set_pitch, db_reload, db_create};

static void disp_load(PfDisplay* d, DispCtx* c, uint32_t o, int modes)
{
    memset(d, 0, sizeof *d);
    d->field0 = rt_r32(o);
    d->primary = P(PfSurface, rt_r32(o + 0x0c));
    d->back = P(PfSurface, rt_r32(o + 0x10));
    d->offscreen = P(PfSurface, rt_r32(o + 0x14));
    d->width = (int32_t)rt_r32(o + 0x58);
    d->height = (int32_t)rt_r32(o + 0x5c);
    d->pitch = (int32_t)rt_r32(o + 0x60);
    d->flip = rt_r32(o + 0x6c);
    d->full_screen = (int32_t)rt_r32(o + 0x70);
    d->present_mode = rt_r8(o + 0x74);
    memcpy(&d->client, rt_ptr(o + 0x78), sizeof d->client);
    d->mode_count = (int32_t)rt_r32(o + 0xa0);
    if (modes) memcpy(d->modes, rt_ptr(o + 0xa4), sizeof d->modes);
    d->screen_pixels = rt_r32(G_PIX);
    d->screen_pitch = (int32_t)rt_r32(G_PITCH);
    d->device3d = (int32_t)rt_r32(G_DEV3D);
    c->d = d;
    c->obj = o;
    d->be = &s_disp_be;
    s_disp_be.user = c;
}

/* each adapter saves and restores the backend's user, so a nested display call (present ->
 * restore runs inside the port, but a game callback may call back in) keeps its own context */
#define DISP_BEGIN(modes) \
    PfDisplay d; DispCtx c; void* saved = s_disp_be.user; disp_load(&d, &c, A(0), (modes))
#define DISP_END() do { disp_store(&c); s_disp_be.user = saved; } while (0)

/* 0x004b4418 lock(eax display, edx target, ebx out pixels, ecx out pitch, stack rect), ret 4 */
static uint32_t b_display_lock(const uint32_t* a, int n)
{
    DISP_BEGIN(0);
    uint32_t op = A(2), oq = A(3);
    uintptr_t px = op ? rt_r32(op) : 0;
    int32_t pt = oq ? (int32_t)rt_r32(oq) : 0;
    int32_t r = pf_display_lock(&d, P(PfSurface, A(1)), op ? &px : NULL, oq ? &pt : NULL, (const void*)(uintptr_t)A(4));
    if (op) rt_w32(op, (uint32_t)px);
    if (oq) rt_w32(oq, (uint32_t)pt);
    DISP_END();
    return (uint32_t)r;
}
/* 0x004b44e4 unlock(eax display, edx target, ebx rect) */
static uint32_t b_display_unlock(const uint32_t* a, int n)
{
    DISP_BEGIN(0);
    int32_t r = pf_display_unlock(&d, P(PfSurface, A(1)), (const void*)(uintptr_t)A(2));
    DISP_END();
    return (uint32_t)r;
}
/* 0x004b4508 restore(eax display) (no surfaces: the original returns the caller's edx, the
 * port 0) */
static uint32_t b_display_restore(const uint32_t* a, int n)
{
    DISP_BEGIN(0);
    int32_t r = pf_display_restore(&d);
    DISP_END();
    return (uint32_t)r;
}
/* 0x004b4d98 fill(eax display, edx color, ebx surface or 0) */
static uint32_t b_display_fill(const uint32_t* a, int n)
{
    DISP_BEGIN(0);
    int32_t r = pf_display_fill(&d, A(1), P(PfSurface, A(2)));
    DISP_END();
    return (uint32_t)r;
}
/* 0x004b5128 present(eax display) */
static uint32_t b_display_present(const uint32_t* a, int n)
{
    DISP_BEGIN(0);
    int32_t r = pf_display_present(&d);
    DISP_END();
    return (uint32_t)r;
}
/* 0x004b4558 set_mode_index(eax display, edx index): returns the create result (the original
 * returns an unset edi on success) */
static uint32_t b_display_set_mode(const uint32_t* a, int n)
{
    DISP_BEGIN(1);
    int32_t r = pf_display_set_mode_index(&d, I(1));
    s_disp_be.user = saved;   /* the create call rebuilt the object: nothing to store back */
    return (uint32_t)r;
}
static uint32_t b_display_mode_count(const uint32_t* a, int n)
{ DISP_BEGIN(0); int32_t r = pf_display_mode_count(&d); s_disp_be.user = saved; return (uint32_t)r; }
static uint32_t b_display_width(const uint32_t* a, int n)
{ DISP_BEGIN(0); int32_t r = pf_display_width(&d); s_disp_be.user = saved; return (uint32_t)r; }
static uint32_t b_display_height(const uint32_t* a, int n)
{ DISP_BEGIN(0); int32_t r = pf_display_height(&d); s_disp_be.user = saved; return (uint32_t)r; }
static uint32_t b_display_field0(const uint32_t* a, int n)
{ DISP_BEGIN(0); uint32_t r = pf_display_field0(&d); s_disp_be.user = saved; return r; }

/* ======================================================================================
 * pf_dsound: the sound device 0x00c850ec
 * ==================================================================================== */

#define DS_DEV 0x00c850ecu

static int32_t sb_play(void* u, PfDsObj* b, uint32_t r1, uint32_t r2, uint32_t f)
{ (void)u; uint32_t v[4] = {H(b), r1, r2, f}; return (int32_t)rt_call(PF2_COM(PF2_IDSBUF, 0x30), v, 4); }
static int32_t sb_stop(void* u, PfDsObj* b) { (void)u; return (int32_t)call1(PF2_COM(PF2_IDSBUF, 0x48), H(b)); }
static int32_t sb_setpos(void* u, PfDsObj* b, uint32_t p) { (void)u; return (int32_t)call2(PF2_COM(PF2_IDSBUF, 0x34), H(b), p); }
/* Release (slot 0x08) of a buffer, or of the IDirectSound itself (0x004b98ec) */
static int32_t sb_release(void* u, PfDsObj* o)
{
    (void)u;
    uint32_t h = H(o);
    return (int32_t)call1(h == rt_r32(DS_DEV) ? PF2_COM(PF2_IDS, 0x08) : PF2_COM(PF2_IDSBUF, 0x08), h);
}
static int32_t sb_lock(void* u, PfDsObj* b, uint32_t off, uint32_t bytes, void* p1, void* n1, void* p2, void* n2,
                       uint32_t f)
{
    (void)u;
    uint32_t v[8] = {H(b), off, bytes, H(p1), H(n1), H(p2), H(n2), f};
    return (int32_t)rt_call(PF2_COM(PF2_IDSBUF, 0x2c), v, 8);
}
static int32_t sb_unlock(void* u, PfDsObj* b, void* p1, uint32_t n1, void* p2, uint32_t n2)
{
    (void)u;
    uint32_t v[5] = {H(b), H(p1), n1, H(p2), n2};
    return (int32_t)rt_call(PF2_COM(PF2_IDSBUF, 0x4c), v, 5);
}
/* GetStatus / GetVolume into a host variable: through a frame word */
static int32_t sb_get_out(uint32_t slot, PfDsObj* b, uint32_t* out)
{
    uint32_t w = rt_frame_push(4);
    rt_w32(w, *out);
    int32_t r = (int32_t)call2(PF2_COM(PF2_IDSBUF, slot), H(b), w);
    *out = rt_r32(w);
    rt_frame_pop(4);
    return r;
}
static int32_t sb_status(void* u, PfDsObj* b, uint32_t* s) { (void)u; return sb_get_out(0x24, b, s); }
static int32_t sb_setvol(void* u, PfDsObj* b, int32_t v) { (void)u; return (int32_t)call2(PF2_COM(PF2_IDSBUF, 0x3c), H(b), (uint32_t)v); }
static int32_t sb_getvol(void* u, PfDsObj* b, int32_t* v) { (void)u; return sb_get_out(0x18, b, (uint32_t*)v); }
static int32_t sb_setpan(void* u, PfDsObj* b, int32_t p) { (void)u; return (int32_t)call2(PF2_COM(PF2_IDSBUF, 0x40), H(b), (uint32_t)p); }
static int32_t sb_setfreq(void* u, PfDsObj* b, uint32_t hz) { (void)u; return (int32_t)call2(PF2_COM(PF2_IDSBUF, 0x44), H(b), hz); }
static int32_t sb_getfreq(void* u, PfDsObj* b, void* hz) { (void)u; return (int32_t)call2(PF2_COM(PF2_IDSBUF, 0x20), H(b), H(hz)); }
static int32_t sb_dup(void* u, PfDsObj* ds, PfDsObj* src, PfDsObj** out)
{ (void)u; return (int32_t)call3(PF2_COM(PF2_IDS, 0x14), H(ds), H(src), H(out)); }
/* 0x004bb9c4 sfx_start(eax, edx, ebx, ecx volume, stack pan, stack arg) */
static int32_t sb_sfx_start(void* u, uint32_t x, uint32_t y, uint32_t z, int32_t vol, int32_t pan, uint32_t arg)
{
    (void)u;
    uint32_t v[6] = {x, y, z, (uint32_t)vol, (uint32_t)pan, arg};
    return (int32_t)rt_call(0x004bb9c4u, v, 6);
}

static const PfDsBackend s_ds_be = {NULL, sb_play, sb_stop, sb_setpos, sb_release, sb_lock, sb_unlock, sb_status,
                                    sb_setvol, sb_getvol, sb_setpan, sb_setfreq, sb_getfreq, sb_dup, sb_sfx_start};

static PfDsDevice ds_load(uint32_t o)
{
    PfDsDevice d;
    d.ds = P(PfDsObj, rt_r32(o));
    d.primary = P(PfDsObj, rt_r32(o + 4));
    d.field8 = rt_r32(o + 8);
    d.reverse_stereo = (int32_t)rt_r32(o + 0x1c);
    d.be = &s_ds_be;
    d.note_freq = (const float*)rt_ptr(0x004e86d0u);
    d.max_distance = (int32_t)rt_r32(0x00c85040u);
    return d;
}
static PfDsSample ds_sample(uint32_t s)
{
    PfDsSample r = {(int32_t)rt_r32(s + 0x18), (int32_t)rt_r32(s + 0x20), rt_r32(s + 0x24), P(PfDsObj, rt_r32(s + 0x40))};
    return r;
}

#define DEV PfDsDevice d = ds_load(A(0))
#define BUF P(PfDsObj, A(1))
static uint32_t b_ds_play(const uint32_t* a, int n) { DEV; return (uint32_t)pf_ds_play(&d, BUF, A(2)); }
static uint32_t b_ds_stop(const uint32_t* a, int n) { DEV; return (uint32_t)pf_ds_stop(&d, BUF); }
static uint32_t b_ds_relbuf(const uint32_t* a, int n) { DEV; return (uint32_t)pf_ds_release_buffer(&d, BUF); }
static uint32_t b_ds_reldev(const uint32_t* a, int n) { DEV; return (uint32_t)pf_ds_release_device(&d); }
static uint32_t b_ds_primary(const uint32_t* a, int n) { DEV; return H(pf_ds_primary(&d)); }
static uint32_t b_ds_field8(const uint32_t* a, int n) { DEV; return pf_ds_field8(&d); }
/* 0x004b990c (eax dev, edx out, ebx source) */
static uint32_t b_ds_dup(const uint32_t* a, int n)
{ DEV; return (uint32_t)pf_ds_duplicate(&d, P(PfDsObj, A(2)), P(PfDsObj*, A(1))); }
/* 0x004b992c (eax dev, edx buf, ebx bytes, ecx ptr1 out, stack len1, ptr2, len2) */
static uint32_t b_ds_lock(const uint32_t* a, int n)
{
    DEV;
    return (uint32_t)pf_ds_lock(&d, BUF, A(2), P(void, A(3)), P(void, A(4)), P(void, A(5)), P(void, A(6)));
}
/* 0x004b995c (eax dev, edx buf, ebx ptr1, ecx len1, stack ptr2, len2) */
static uint32_t b_ds_unlock(const uint32_t* a, int n)
{ DEV; return (uint32_t)pf_ds_unlock(&d, BUF, P(void, A(2)), A(3), P(void, A(4)), A(5)); }
static uint32_t b_ds_setpos(const uint32_t* a, int n) { DEV; return (uint32_t)pf_ds_set_position(&d, BUF, A(2)); }
/* 0x004b999c / 0x004b99cc (eax dev, edx buf, ebx out) */
static uint32_t b_ds_status(const uint32_t* a, int n)
{
    DEV;
    uint32_t o = A(2), v = o ? rt_r32(o) : 0;
    int32_t r = pf_ds_get_status(&d, BUF, &v);
    if (o && BUF) rt_w32(o, v);
    return (uint32_t)r;
}
static uint32_t b_ds_getvol(const uint32_t* a, int n)
{
    DEV;
    uint32_t o = A(2);
    int32_t v = o ? (int32_t)rt_r32(o) : 0;
    int32_t r = pf_ds_get_volume(&d, BUF, &v);
    if (o && BUF) rt_w32(o, (uint32_t)v);
    return (uint32_t)r;
}
static uint32_t b_ds_setvol(const uint32_t* a, int n) { DEV; return (uint32_t)pf_ds_set_volume(&d, BUF, I(2)); }
static uint32_t b_ds_setpan(const uint32_t* a, int n) { DEV; return (uint32_t)pf_ds_set_pan(&d, BUF, I(2)); }
static uint32_t b_ds_setfreq(const uint32_t* a, int n) { DEV; return (uint32_t)pf_ds_set_frequency(&d, BUF, A(2)); }
static uint32_t b_ds_getfreq(const uint32_t* a, int n) { DEV; return (uint32_t)pf_ds_get_frequency(&d, BUF, P(void, A(2))); }
#undef DEV
#undef BUF

/* the sample functions: eax sample record; the device is 0x00c850ec */
static uint32_t b_ds_sample_pan(const uint32_t* a, int n)
{
    PfDsDevice d = ds_load(DS_DEV);
    PfDsSample s = ds_sample(A(0));
    return (uint32_t)pf_ds_sample_set_pan(&d, &s, I(1));
}
static uint32_t b_ds_sample_pitch(const uint32_t* a, int n)
{
    PfDsDevice d = ds_load(DS_DEV);
    PfDsSample s = ds_sample(A(0));
    return (uint32_t)pf_ds_sample_set_pitch(&d, &s, I(1));
}
/* 0x004ba938 (eax sample, edx out) */
static uint32_t b_ds_sample_playing(const uint32_t* a, int n)
{
    PfDsDevice d = ds_load(DS_DEV);
    PfDsSample s = ds_sample(A(0));
    uint32_t o = A(1), v = rt_r32(o);
    pf_ds_sample_is_playing(&d, &s, &v);
    rt_w32(o, v);
    return 0;
}
/* 0x004b9168 play_at(eax, edx, ebx, ecx volume, stack distance, lateral, arg, unused) */
static uint32_t b_ds_play_at(const uint32_t* a, int n)
{
    PfDsDevice d = ds_load(DS_DEV);
    return (uint32_t)pf_ds_play_at(&d, A(0), A(1), A(2), I(3), I(4), I(5), A(6));
}

/* ======================================================================================
 * pf_sfx: the voice list slot update
 * ==================================================================================== */

/* the instance (from 0x004bae50) as the port's host copy; voices are 0x44 byte game records */
typedef struct { PfSfxInstance in; PfSfxVoice* voices; uint32_t addr, vaddr; } SfxInst;
static SfxInst* s_sfx_inst;   /* the one being applied (apply does not nest) */

static PfSfxInstance* xb_make(void* u, uintptr_t prog, int32_t key)
{
    (void)u;
    uint32_t in = call2(0x004bae50u, (uint32_t)prog, (uint32_t)key);
    SfxInst* s = s_sfx_inst;
    s->addr = in;
    if (!in) return NULL;
    int32_t nv = (int32_t)rt_r32(in + 0x10);
    s->vaddr = rt_r32(in + 0x14);
    s->in.volume = (int32_t)rt_r32(in);
    s->in.voice_count = nv;
    free(s->voices);
    s->voices = NULL;
    if (nv > 0) {
        s->voices = calloc((size_t)nv, sizeof *s->voices);
        for (int32_t i = 0; i < nv; i++) {
            uint32_t v = s->vaddr + 0x44u * (uint32_t)i;
            s->voices[i].volume = (int32_t)rt_r32(v + 4);
            s->voices[i].loop = (int32_t)rt_r32(v + 0x0c);
            s->voices[i].buffer = rt_r32(v + 0x40);
        }
    }
    s->in.voices = s->voices;
    return &s->in;
}
static uint32_t xb_voice(const PfSfxVoice* v) { return s_sfx_inst->vaddr + 0x44u * (uint32_t)(v - s_sfx_inst->voices); }
/* the device wrappers: eax = 0x00c850ec, edx = buffer, ebx = value */
static void xb_setvol(void* u, uintptr_t b, int32_t db) { (void)u; call3(0x004b99b4u, DS_DEV, (uint32_t)b, (uint32_t)db); }
static void xb_setpan(void* u, PfSfxVoice* v, int32_t p) { (void)u; call2(0x004ba76cu, xb_voice(v), (uint32_t)p); }
static void xb_setpitch(void* u, PfSfxVoice* v, int32_t p) { (void)u; call2(0x004ba890u, xb_voice(v), (uint32_t)p); }
static void xb_status(void* u, uintptr_t b, uint32_t* st)
{
    (void)u;
    uint32_t w = rt_frame_push(4);
    rt_w32(w, *st);
    call3(0x004b999cu, DS_DEV, (uint32_t)b, w);
    *st = rt_r32(w);
    rt_frame_pop(4);
}
static int32_t xb_play(void* u, uintptr_t b, int32_t loop) { (void)u; return (int32_t)call3(0x004b987cu, DS_DEV, (uint32_t)b, (uint32_t)loop); }

static const PfSfxBackend s_sfx_be = {NULL, xb_make, xb_setvol, xb_setpan, xb_setpitch, xb_status, xb_play};

static PfSfxSlot sfx_slot(uint32_t s)
{
    PfSfxSlot r;
    r.program = rt_r32(s);
    r.instance = NULL;
    r.key = (int32_t)rt_r32(s + 8);
    r.flags = rt_r32(s + 0x0c);
    r.volume = (int32_t)rt_r32(s + 0x10);
    r.pan = (int32_t)rt_r32(s + 0x14);
    r.pitch = (int32_t)rt_r32(s + 0x18);
    return r;
}
static void sfx_store(uint32_t s, const PfSfxSlot* r)
{
    rt_w32(s + 0x0c, r->flags);
    rt_w32(s + 0x10, (uint32_t)r->volume);
    rt_w32(s + 0x14, (uint32_t)r->pan);
    rt_w32(s + 0x18, (uint32_t)r->pitch);
}

/* 0x004bbaf8 (eax slot) */
static uint32_t b_sfx_apply(const uint32_t* a, int n)
{
    SfxInst inst = {{0, 0, NULL}, NULL, 0, 0};
    SfxInst* saved = s_sfx_inst;
    s_sfx_inst = &inst;
    PfSfxSlot s = sfx_slot(A(0));
    int32_t r = pf_sfx_slot_apply(&s_sfx_be, &s);
    rt_w32(A(0) + 4, s.instance ? inst.addr : 0);
    sfx_store(A(0), &s);
    free(inst.voices);
    s_sfx_inst = saved;
    return (uint32_t)r;
}
#define SFX_SET(name, call) \
    static uint32_t name(const uint32_t* a, int n) \
    { PfSfxSlot s = sfx_slot(A(0)); int32_t r = call; sfx_store(A(0), &s); return (uint32_t)r; }
SFX_SET(b_sfx_req_play, pf_sfx_request_play(&s))
SFX_SET(b_sfx_req_2, pf_sfx_request_2(&s))
SFX_SET(b_sfx_req_volume, pf_sfx_request_volume(&s, I(1)))
SFX_SET(b_sfx_req_pan, pf_sfx_request_pan(&s, I(1)))
SFX_SET(b_sfx_req_pitch, pf_sfx_request_pitch(&s, I(1)))

/* ======================================================================================
 * pf_mouse
 * ==================================================================================== */

#define G_VK 0x00c84810u
#define G_UNREAD 0x004e86a8u

/* the mouse module's stack locals the Win32 / registry calls take by address: the POINT, the
 * registry key word, the value buffer and its size; one area, allocated once */
static uint32_t s_mz;      /* +0x00 POINT, +0x08 key, +0x0c size, +0x10 buf 0x50, +0x60 strings */
static uint32_t s_hwnd;    /* app_get_hwnd's last answer: the window the conversions use */

static uint32_t mz(void)
{
    if (!s_mz) s_mz = rt_fake_alloc(0x100);
    return s_mz;
}
/* a host string as game memory: the EXE's own copy when it is the same text, else a copy */
static uint32_t mstr(const char* s, uint32_t known, uint32_t slot)
{
    if (known && strcmp((const char*)rt_ptr(known), s) == 0) return known;
    uint32_t a = mz() + 0x60 + slot;
    size_t l = strlen(s);
    if (l > 0x4f) l = 0x4f;
    memcpy(rt_ptr(a), s, l);
    rt_w8(a + (uint32_t)l, 0);
    return a;
}
static int32_t mb_gsm(void* u, int32_t i) { (void)u; return (int32_t)call1(s_GetSystemMetrics, (uint32_t)i); }
static int16_t mb_gaks(void* u, int32_t vk) { (void)u; return (int16_t)call1(s_GetAsyncKeyState, (uint32_t)vk); }
static int32_t mb_pt_call(uint32_t slot, int with_hwnd, PfPoint* pt)
{
    uint32_t p = mz();
    memcpy(rt_ptr(p), pt, 8);
    int32_t r = (int32_t)(with_hwnd ? call2(slot, s_hwnd, p) : call1(slot, p));
    memcpy(pt, rt_ptr(p), 8);
    return r;
}
static int32_t mb_gcp(void* u, PfPoint* pt) { (void)u; return mb_pt_call(s_GetCursorPos, 0, pt); }
static int32_t mb_s2c(void* u, PfPoint* pt) { (void)u; return mb_pt_call(s_ScreenToClient, 1, pt); }
static int32_t mb_c2s(void* u, PfPoint* pt) { (void)u; return mb_pt_call(s_ClientToScreen, 1, pt); }
static int32_t mb_scp(void* u, int32_t x, int32_t y) { (void)u; return (int32_t)call2(s_SetCursorPos, (uint32_t)x, (uint32_t)y); }
static void mb_hwnd(void* u) { (void)u; s_hwnd = call0(0x004a8f34u); }
static int32_t mb_w(void* u) { (void)u; return (int32_t)call0(0x004958d8u); }
static int32_t mb_h(void* u) { (void)u; return (int32_t)call0(0x004958e4u); }
/* the registry helpers as 0x004b2820 calls them: eax = &key, ebx = the subkey (0x004e3648) for
 * both 0x004ac4b0 (edx 0) and 0x004ac528 (edx root) */
static uint32_t s_mkey_sub;
static void mb_reg_init(void* u, const char* sub)
{
    (void)u;
    s_mkey_sub = mstr(sub, 0x004e3648u, 0);
    uint32_t v[3] = {mz() + 8, 0, s_mkey_sub};
    rt_call(0x004ac4b0u, v, 3);
}
static int32_t mb_reg_open(void* u, uint32_t root)
{
    (void)u;
    uint32_t v[3] = {mz() + 8, root, s_mkey_sub};
    return (int32_t)rt_call(0x004ac528u, v, 3);
}
/* 0x004ac5e4(eax &key, edx value name (0x004e365c), ebx buf, ecx &size) */
static int32_t mb_reg_read(void* u, const char* value, char* buf, int32_t* size)
{
    (void)u;
    uint32_t z = mz();
    uint32_t nm = mstr(value, 0x004e365cu, 0x50);
    int32_t sz = *size > 0x50 ? 0x50 : *size;
    memcpy(rt_ptr(z + 0x10), buf, (size_t)(sz > 0 ? sz : 0));
    rt_w32(z + 0x0c, (uint32_t)*size);
    uint32_t v[4] = {z + 8, nm, z + 0x10, z + 0x0c};
    int32_t r = (int32_t)rt_call(0x004ac5e4u, v, 4);
    *size = (int32_t)rt_r32(z + 0x0c);
    memcpy(buf, rt_ptr(z + 0x10), (size_t)(sz > 0 ? sz : 0));
    return r;
}
static void mb_reg_close(void* u) { (void)u; call1(0x004ac590u, mz() + 8); }

static const PfMouseBackend s_mouse_be = {NULL, mb_gsm, mb_gaks, mb_gcp, mb_s2c, mb_c2s, mb_scp, mb_hwnd, mb_w, mb_h,
                                          mb_reg_init, mb_reg_open, mb_reg_read, mb_reg_close};

static PfMouseSys msys_load(void)
{
    PfMouseSys g = {(int32_t)rt_r32(G_VK), (int32_t)rt_r32(G_VK + 4), (int32_t)rt_r32(G_VK + 8),
                    (int32_t)rt_r32(G_UNREAD), &s_mouse_be};
    return g;
}
static void msys_store(const PfMouseSys* g)
{
    rt_w32(G_VK, (uint32_t)g->vk_primary);
    rt_w32(G_VK + 4, (uint32_t)g->vk_secondary);
    rt_w32(G_VK + 8, (uint32_t)g->vk_middle);
    rt_w32(G_UNREAD, (uint32_t)g->registry_unread);
}
static PfMouse mouse_load(uint32_t m) { PfMouse r; memcpy(&r, rt_ptr(m), sizeof r); return r; }

/* 0x004b2820 (eax state): returns the state */
static uint32_t b_mouse_init(const uint32_t* a, int n)
{
    PfMouseSys g = msys_load();
    PfMouse m = mouse_load(A(0));
    pf_mouse_init(&g, &m);
    memcpy(rt_ptr(A(0)), &m, sizeof m);
    msys_store(&g);
    return A(0);
}
static uint32_t b_mouse_reread(const uint32_t* a, int n)
{
    (void)a; (void)n;
    PfMouseSys g = msys_load();
    pf_mouse_reread_swap(&g);
    msys_store(&g);
    return 0;
}
static uint32_t b_mouse_poll(const uint32_t* a, int n)
{
    PfMouseSys g = msys_load();
    PfMouse m = mouse_load(A(0));
    pf_mouse_poll(&g, &m);
    memcpy(rt_ptr(A(0)), &m, sizeof m);
    return 0;
}
/* 0x004b2ad4 (eax state, edx POINT*) */
static uint32_t b_mouse_cclient(const uint32_t* a, int n)
{
    PfMouseSys g = msys_load();
    PfMouse m = mouse_load(A(0));
    PfPoint pt;
    memcpy(&pt, rt_ptr(A(1)), 8);
    int32_t r = pf_mouse_cursor_client(&g, &m, &pt);
    memcpy(rt_ptr(A(1)), &pt, 8);
    return (uint32_t)r;
}
/* 0x004b2b00 (eax state, edx int* x, ebx int* y) */
static uint32_t b_mouse_centered(const uint32_t* a, int n)
{
    PfMouseSys g = msys_load();
    PfMouse m = mouse_load(A(0));
    int32_t x = (int32_t)rt_r32(A(1)), y = (int32_t)rt_r32(A(2));
    int32_t r = pf_mouse_centered_pos(&g, &m, &x, &y);
    rt_w32(A(1), (uint32_t)x);
    rt_w32(A(2), (uint32_t)y);
    return (uint32_t)r;
}
#define AXIS(name, fn) \
    static uint32_t name(const uint32_t* a, int n) { PfMouse m = mouse_load(A(0)); return (uint32_t)fn(&m); }
AXIS(b_mouse_right, pf_mouse_axis_right)
AXIS(b_mouse_left, pf_mouse_axis_left)
AXIS(b_mouse_down, pf_mouse_axis_down)
AXIS(b_mouse_up, pf_mouse_axis_up)
/* 0x004b2d00 (edx POINT*; eax unused) */
static uint32_t b_mouse_setpt(const uint32_t* a, int n)
{
    PfMouseSys g = msys_load();
    PfPoint p;
    memcpy(&p, rt_ptr(A(1)), 8);
    return (uint32_t)pf_mouse_set_cursor_pt(&g, &p);
}
/* 0x004b2d60 (edx x, ebx y) */
static uint32_t b_mouse_set(const uint32_t* a, int n)
{ PfMouseSys g = msys_load(); return (uint32_t)pf_mouse_set_cursor(&g, I(1), I(2)); }
/* 0x004b2db8 / 0x004b2e48 (eax state, edx value) */
static uint32_t b_mouse_setx(const uint32_t* a, int n)
{ PfMouseSys g = msys_load(); PfMouse m = mouse_load(A(0)); return (uint32_t)pf_mouse_set_cursor_x(&g, &m, I(1)); }
static uint32_t b_mouse_sety(const uint32_t* a, int n)
{ PfMouseSys g = msys_load(); PfMouse m = mouse_load(A(0)); return (uint32_t)pf_mouse_set_cursor_y(&g, &m, I(1)); }
static uint32_t b_mouse_prim(const uint32_t* a, int n)
{ (void)a; (void)n; PfMouseSys g = msys_load(); return (uint32_t)pf_mouse_primary_down(&g); }
static uint32_t b_mouse_sec(const uint32_t* a, int n)
{ (void)a; (void)n; PfMouseSys g = msys_load(); return (uint32_t)pf_mouse_secondary_down(&g); }
/* the field getters (eax state), as diff_pf_mouse.py's GETTERS */
static uint32_t b_mouse_ref_w(const uint32_t* a, int n) { return rt_r32(A(0) + 0x20); }
static uint32_t b_mouse_ref_h(const uint32_t* a, int n) { return rt_r32(A(0) + 0x24); }
static uint32_t b_mouse_x(const uint32_t* a, int n) { return rt_r32(A(0) + 0x08); }
static uint32_t b_mouse_y(const uint32_t* a, int n) { return rt_r32(A(0) + 0x0c); }
static uint32_t b_mouse_buttons(const uint32_t* a, int n) { return rt_r32(A(0) + 0x04); }
static uint32_t b_mouse_bits(const uint32_t* a, int n) { return rt_r32(A(0) + 0x10); }

/* ======================================================================================
 * pf2_d3d: the hardware 3D draw path (eax = the display object, edx = the argument)
 * ==================================================================================== */

static int32_t d3b_srs(void* u, Pf2Device* dev, uint32_t s, uint32_t v)
{ (void)u; return (int32_t)call3(PF2_COM(PF2_IDEV2, 0x5c), H(dev), s, v); }
static int32_t d3b_draw(void* u, Pf2Device* dev, uint32_t type, uint32_t vt, const void* verts, uint32_t count,
                        uint32_t flags)
{
    (void)u;
    GBuf b = gbuf((void*)(uintptr_t)verts, 32u * count);
    uint32_t v[6] = {H(dev), type, vt, b.addr, count, flags};
    int32_t r = (int32_t)rt_call(PF2_COM(PF2_IDEV2, 0x74), v, 6);
    gbuf_done(&b, 0);
    return r;
}
static const Pf2D3dBackend s_d3d_be = {NULL, d3b_srs, d3b_draw};

static Pf2D3d d3d_load(uint32_t disp)
{
    Pf2D3d d = {P(Pf2Device, rt_r32(disp + 0x28)), (uint32_t*)rt_ptr(0x004e86b0u), (uint32_t*)rt_ptr(0x00c84f74u),
                rt_ptr(PF2_TEX_BASE), rt_mem_cb, NULL, &s_d3d_be};
    return d;
}
static uint32_t b_d3d_filter(const uint32_t* a, int n) { Pf2D3d d = d3d_load(A(0)); return (uint32_t)pf2_d3d_set_filter(&d, A(1)); }
static uint32_t b_d3d_blend(const uint32_t* a, int n) { Pf2D3d d = d3d_load(A(0)); return (uint32_t)pf2_d3d_set_blend(&d, A(1)); }
static uint32_t b_d3d_prim(const uint32_t* a, int n) { Pf2D3d d = d3d_load(A(0)); return (uint32_t)pf2_d3d_draw_prim(&d, rt_ptr(A(1))); }
static uint32_t b_d3d_ot(const uint32_t* a, int n) { Pf2D3d d = d3d_load(A(0)); pf2_d3d_draw_ot(&d, A(1)); return 0; }

/* ======================================================================================
 * pf5: C runtime and voice list entries (T harness: regs as listed)
 * ==================================================================================== */

/* 0x004bf680 malloc(eax size): ds (edx under the harness) is 0, the flat selector */
static uint32_t b_malloc(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf5_malloc(&e, A(0), 0);
    rt_en2env_done();
    return r;
}
static uint32_t b_free(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    pf5_free(&e, A(0), 0);
    rt_en2env_done();
    return 0;
}
/* 0x004bc7ea crt_init_rtns(eax is_dll, edx, ebx module) */
static uint32_t b_crt_init_rtns(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf5_crt_init_rtns(&e, A(0), A(1), A(2));
    rt_en2env_done();
    return r;
}
/* 0x004bb9c4 sfx_start(eax bank, edx program, ebx key, ecx volume, stack pan, pitch), ret 8 */
static uint32_t b_sfx_start(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf5_sfx_start(&e, A(0), A(1), A(2), A(3), A(4), A(5));
    rt_en2env_done();
    return r;
}
/* 0x004bc31c sfx_slot_alloc(eax list, edx before) */
static uint32_t b_sfx_slot_alloc(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf5_sfx_slot_alloc(&e, A(0), A(1));
    rt_en2env_done();
    return r;
}
static uint32_t b_sfx_stop_all(const uint32_t* a, int n)
{
    (void)a; (void)n;
    Pf2Env e = rt_pf2env();
    uint32_t r = pf5_sfx_stop_all(&e);
    rt_en2env_done();
    return r;
}

void rt_bind_platform(void)
{
    s_GetSystemMetrics = rt_import_slot("GetSystemMetrics");
    s_GetAsyncKeyState = rt_import_slot("GetAsyncKeyState");
    s_GetCursorPos = rt_import_slot("GetCursorPos");
    s_ScreenToClient = rt_import_slot("ScreenToClient");
    s_ClientToScreen = rt_import_slot("ClientToScreen");
    s_SetCursorPos = rt_import_slot("SetCursorPos");

    rt_bind(0x004b4418u, b_display_lock, "display_lock");
    rt_bind(0x004b44e4u, b_display_unlock, "display_unlock");
    rt_bind(0x004b4508u, b_display_restore, "display_restore");
    rt_bind(0x004b454cu, b_display_mode_count, "display_mode_count");
    rt_bind(0x004b4554u, b_display_field0, "load_dword");
    rt_bind(0x004b4558u, b_display_set_mode, "display_set_mode_index");
    rt_bind(0x004b4d98u, b_display_fill, "display_fill");
    rt_bind(0x004b5128u, b_display_present, "display_present");
    rt_bind(0x004b5a10u, b_display_height, "display_height");
    rt_bind(0x004b5a14u, b_display_width, "display_width");

    rt_bind(0x004b9168u, b_ds_play_at, "sound_play_3d");
    rt_bind(0x004b987cu, b_ds_play, "dsbuf_play");
    rt_bind(0x004b9898u, b_ds_stop, "dsbuf_stop");
    rt_bind(0x004b98ccu, b_ds_relbuf, "dsbuf_release");
    rt_bind(0x004b98ecu, b_ds_reldev, "dsound_release");
    rt_bind(0x004b9908u, b_ds_primary, "dsound_primary");
    rt_bind(0x004b990cu, b_ds_dup, "dsound_duplicate_buffer");
    rt_bind(0x004b992cu, b_ds_lock, "dsbuf_lock");
    rt_bind(0x004b995cu, b_ds_unlock, "dsbuf_unlock");
    rt_bind(0x004b9984u, b_ds_setpos, "dsbuf_set_position");
    rt_bind(0x004b999cu, b_ds_status, "dsbuf_get_status");
    rt_bind(0x004b99b4u, b_ds_setvol, "dsbuf_set_volume");
    rt_bind(0x004b99ccu, b_ds_getvol, "dsbuf_get_volume");
    rt_bind(0x004b99f8u, b_ds_setpan, "dsbuf_set_pan");
    rt_bind(0x004b9a18u, b_ds_setfreq, "dsbuf_set_frequency");
    rt_bind(0x004b9a30u, b_ds_getfreq, "dsbuf_get_frequency");
    rt_bind(0x004b9a48u, b_ds_field8, "dsound_field8");
    rt_bind(0x004ba76cu, b_ds_sample_pan, "sample_set_pan");
    rt_bind(0x004ba890u, b_ds_sample_pitch, "sample_set_pitch");
    rt_bind(0x004ba938u, b_ds_sample_playing, "sample_is_playing");

    rt_bind(0x004b2820u, b_mouse_init, "mouse_init");
    rt_bind(0x004b295cu, b_mouse_reread, "mouse_reread_swap");
    rt_bind(0x004b29e0u, b_mouse_poll, "mouse_poll");
    rt_bind(0x004b2ad4u, b_mouse_cclient, "mouse_cursor_client");
    rt_bind(0x004b2b00u, b_mouse_centered, "mouse_get_centered_pos");
    rt_bind(0x004b2b78u, b_mouse_ref_w, "mouse_ref_w");
    rt_bind(0x004b2b7cu, b_mouse_ref_h, "mouse_ref_h");
    rt_bind(0x004b2b80u, b_mouse_x, "mouse_x");
    rt_bind(0x004b2b84u, b_mouse_right, "mouse_axis_right");
    rt_bind(0x004b2bdcu, b_mouse_left, "mouse_axis_left");
    rt_bind(0x004b2c40u, b_mouse_y, "mouse_y");
    rt_bind(0x004b2c44u, b_mouse_down, "mouse_axis_down");
    rt_bind(0x004b2c9cu, b_mouse_up, "mouse_axis_up");
    rt_bind(0x004b2d00u, b_mouse_setpt, "mouse_set_cursor_pt");
    rt_bind(0x004b2d60u, b_mouse_set, "mouse_set_cursor");
    rt_bind(0x004b2db8u, b_mouse_setx, "mouse_set_cursor_x");
    rt_bind(0x004b2e48u, b_mouse_sety, "mouse_set_cursor_y");
    rt_bind(0x004b2ed8u, b_mouse_buttons, "mouse_buttons");
    rt_bind(0x004b2edcu, b_mouse_bits, "mouse_bits");
    rt_bind(0x004b2ee0u, b_mouse_prim, "mouse_primary_down");
    rt_bind(0x004b2f00u, b_mouse_sec, "mouse_secondary_down");

    rt_bind(0x004bbaf8u, b_sfx_apply, "sfx_slot_apply");
    rt_bind(0x004bbd80u, b_sfx_req_play, "sfx_request_play");
    rt_bind(0x004bbd88u, b_sfx_req_2, "sfx_request_2");
    rt_bind(0x004bbd90u, b_sfx_req_volume, "sfx_request_volume");
    rt_bind(0x004bbda4u, b_sfx_req_pan, "sfx_request_pan");
    rt_bind(0x004bbdb8u, b_sfx_req_pitch, "sfx_request_pitch");

    rt_bind(0x004b611cu, b_d3d_filter, "d3d_set_filter");
    rt_bind(0x004b617cu, b_d3d_blend, "d3d_set_blend");
    rt_bind(0x004b6780u, b_d3d_ot, "d3d_draw_ot");
    rt_bind(0x004b67d0u, b_d3d_prim, "d3d_draw_prim");

    rt_bind(0x004bc7eau, b_crt_init_rtns, "crt_init_rtns");
    rt_bind(0x004bf680u, b_malloc, "malloc");
    rt_bind(0x004bf76fu, b_free, "free");
    rt_bind(0x004bb9c4u, b_sfx_start, "sfx_start");
    rt_bind(0x004bc31cu, b_sfx_slot_alloc, "sfx_slot_alloc");
    rt_bind(0x004bc5ccu, b_sfx_stop_all, "sfx_stop_all");
}

RT_BIND_AREA(platform)
