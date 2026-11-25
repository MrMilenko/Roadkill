/*
 * en_view2.c: the small view accessors and the scene module init, ported from TM2.EXE (see
 * en_view2.h for the address list).
 */
#include "en_view2.h"

#include <math.h>
#include <string.h>

static int32_t vp32(const EnViewBlock* b, int32_t slot, uint32_t off) {
    int32_t v;
    memcpy(&v, (const uint8_t*)b->vports + 0x94 * (uint32_t)slot + off, 4);
    return v;
}
static void vpw(EnViewBlock* b, int32_t slot, uint32_t off, int32_t v) {
    memcpy((uint8_t*)b->vports + 0x94 * (uint32_t)slot + off, &v, 4);
}
static uint8_t* at(EnMem* m, uint32_t a, size_t n) {
    uint32_t o = a - m->base;
    return ((size_t)o <= m->size && m->size - o >= n) ? m->mem + o : NULL;
}
static void w32(EnMem* m, uint32_t a, uint32_t v) { uint8_t* p = at(m, a, 4); if (p) memcpy(p, &v, 4); }
static uint32_t r32(EnMem* m, uint32_t a) { uint32_t v = 0; uint8_t* p = at(m, a, 4); if (p) memcpy(&v, p, 4); return v; }
static void cpy_out(EnMem* m, uint32_t a, uint8_t* out, size_t n) { uint8_t* p = at(m, a, n); if (p) memcpy(out, p, n); }
static void cpy_in(EnMem* m, uint32_t a, const uint8_t* in, size_t n) { uint8_t* p = at(m, a, n); if (p) memcpy(p, in, n); }
static int32_t half(int32_t v) { return (int32_t)((uint32_t)v - (uint32_t)(v >> 31)) >> 1; }
static int32_t add(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }

void en_view_screen_size(const EnViewBlock* b, int32_t* w, int32_t* h) {
    int32_t s = b->views.slot[0];
    *w = vp32(b, s, 0x10);
    *h = vp32(b, s, 0x14);
}

uint32_t en_view_rect16(const EnViewBlock* b, int16_t out[4]) {
    int32_t s = b->views.slot[0];
    out[0] = (int16_t)vp32(b, s, 0x20);
    out[1] = (int16_t)vp32(b, s, 0x24);
    out[2] = (int16_t)vp32(b, s, 0x18);
    out[3] = (int16_t)vp32(b, s, 0x1c);
    return 0x00a03a90u;
}

void en_view_rect(const EnViewBlock* b, int32_t i, int32_t* r) {
    if (!r) return;
    int32_t s = b->views.slot[i];
    r[0] = vp32(b, s, 0x20);
    r[1] = vp32(b, s, 0x24);
    r[2] = add(r[0], vp32(b, s, 0x18));
    r[3] = add(r[1], vp32(b, s, 0x1c));
}

/* 0x004979a2 (chop) then fistp dword */
static int32_t chop32(double d) {
    double t = trunc(d);
    if (!(t >= -2147483648.0 && t < 2147483648.0)) return INT32_MIN;
    return (int32_t)t;
}

void en_view_resize_all(EnViewBlock* b, EnResizeEnv* e, int32_t w, int32_t h) {
    *e->screen_h = h;
    *e->screen_w = w;
    for (int32_t i = 0; i <= *e->count; i++) {
        int32_t bd = vp32(b, i, 0x8c);
        if (bd) {
            int32_t q = (int32_t)((uint32_t)bd * (uint32_t)*e->screen_w) / vp32(b, i, 0x10);
            float f;
            memcpy(&f, (const uint8_t*)b->vports + 0x94 * (uint32_t)i + 0x30, 4);
            int32_t c = chop32((double)*e->screen_w * (double)f);
            vpw(b, i, 0x8c, q);
            if (add(add(q, q), c) < 100) vpw(b, i, 0x8c, half((int32_t)(100u - (uint32_t)c)));
            int32_t v = vp32(b, i, 0x8c);
            v = (int32_t)((uint32_t)v + (v < 0 ? 7u : 0u)) >> 3;
            v = (int32_t)((uint32_t)v << 3);
            vpw(b, i, 0x8c, v);
            vpw(b, i, 0x90, (int32_t)((uint32_t)v * (uint32_t)*e->screen_h) / *e->screen_w);
        }
        int32_t args[6] = { vp32(b, i, 0x28), vp32(b, i, 0x2c), vp32(b, i, 0x30), vp32(b, i, 0x34), vp32(b, i, 0x0), vp32(b, i, 0x4) };
        e->setup(e->user, i, args);
        int32_t x = vp32(b, i, 0x20), y = vp32(b, i, 0x24);
        e->gpu_viewport(e->user, i, *e->screen_w, *e->screen_h, x, add(add(x, vp32(b, i, 0x18)), -1), y,
                        add(add(y, vp32(b, i, 0x1c)), -1));
    }
}

void en_view_border(const EnViewBlock* b, int32_t i, int32_t* out) { *out = vp32(b, i, 0x8c); }

void en_view_border_call(const EnViewBlock* b, int32_t i, int32_t x, void (*fn)(void*, int32_t, int32_t), void* user) {
    fn(user, i, add(x, vp32(b, i, 0x8c)));
}

void en_view_set_target(const EnViewBlock* b, EnMem* m, uint32_t t, int32_t i) {
    if (i >= 2) return;
    uint32_t cam = b->views.camera[i];
    if (t && cam) { w32(m, cam + 0x10, t); return; }
    if (cam) w32(m, cam + 0x10, b->views.root);
}

void en_view_set_viewer0(EnViewBlock* b, EnMem* m, uint32_t p) {
    b->views.entity[0] = p;
    w32(m, p, 0xffe);
}

void en_view_set_viewer_pos(const EnViewBlock* b, EnMem* m, const int32_t v[3], int32_t i) {
    if (i >= 2) return;
    uint32_t e = b->views.entity[i];
    if (!e) return;
    w32(m, e + 0xe8, (uint32_t)v[0]);
    w32(m, e + 0xec, (uint32_t)v[1]);
    w32(m, e + 0xf0, (uint32_t)v[2]);
}

void en_view_get_cam_angles(const EnViewBlock* b, EnMem* m, uint8_t out[8], int32_t i) {
    if (i < 2) cpy_out(m, b->views.camera[i] + 0xe0, out, 8);
}
void en_view_get_cam_pos(const EnViewBlock* b, EnMem* m, uint8_t out[12], int32_t i) {
    if (i < 2) cpy_out(m, b->views.camera[i] + 0xe8, out, 12);
}
void en_view_set_cam_angles(const EnViewBlock* b, EnMem* m, const uint8_t in[8], int32_t i) {
    if (i < 2 && b->views.camera[i]) cpy_in(m, b->views.camera[i] + 0xe0, in, 8);
}
void en_view_set_cam_pos(const EnViewBlock* b, EnMem* m, const uint8_t in[12], int32_t i) {
    if (i < 2 && b->views.camera[i]) cpy_in(m, b->views.camera[i] + 0xe8, in, 12);
}
uint32_t en_view_cam_field(const EnViewBlock* b, int32_t i, uint32_t off) { return b->views.camera[i] + off; }
uint32_t en_view_cam_target(const EnViewBlock* b, EnMem* m, int32_t i) {
    if (i >= 2) return 0;
    uint32_t cam = b->views.camera[i];
    return cam ? r32(m, cam + 0x10) : 0;
}

void en_fog_set(EnViewBlock* b, int32_t r, int32_t g, int32_t bl, int hw,
                void (*device)(void*, uint32_t, uint32_t, uint32_t), void* user) {
    memcpy(b->fog, &r, 4);
    memcpy(b->fog + 4, &g, 4);
    memcpy(b->fog + 8, &bl, 4);
    if (hw) device(user, (uint32_t)r & 0xff, (uint32_t)g & 0xff, (uint32_t)bl & 0xff);
}

uint32_t en_view_render_pos_addr(int32_t i) { return i < 2 ? 0x00a03aecu + 12u * (uint32_t)i : 0x00a03aecu; }
uint32_t en_view_cam_mat_addr(int32_t i) { return i < 2 ? 0x00a03b04u + 0x30u * (uint32_t)i : 0x00a03b04u; }

void en_scene_init(EnScene* s, void (*init)(void*, int, uint32_t), void* user) {
    init(user, 0, 0x00a5eb68u);
    init(user, 1, 0x00a5eb68u);
    init(user, 2, 0x00a5eb68u);
    init(user, 3, 0x00a6d5c8u);
    s->cull_force = 0;
    s->w->lod_disabled = 0;
    s->force_visible = 0;
    s->anim_count = 0;
    s->anim_on = 0;
    memset(s->anim_node, 0, sizeof s->anim_node);
    memset(s->extra, 0, sizeof s->extra);
}
