/*
 * en_view.c: viewport and light environment setters, ported from TM2.EXE 0x0048bb10, 0x0048bb94,
 * 0x00493880, 0x00494c48, 0x00494dc0, 0x00495864, 0x0049527c, 0x0049517c, 0x00495050
 * (see en_view.h).
 */
#include "en_view.h"

#include "../math/tm2math.h"
#include "en_math.h"

#include <string.h>

static int32_t shl(int32_t v, int s) { return (int32_t)((uint32_t)v << s); }

void en_viewport_set_origin(EnViewport* v, int32_t x, int32_t y, int hw) {
    if (hw) {
        v->y = y;
        v->cx = (float)x;
        v->cy = (float)y;
        v->x = x;
        return;
    }
    v->x = (int32_t)((uint32_t)x + 0x10);
    v->y = (int32_t)((uint32_t)y + 0x10);
    v->cx = (float)(int32_t)((uint32_t)shl(x, 5) + 0x200);
    v->cy = (float)(int32_t)((uint32_t)shl(y, 5) + 0x200);
}

void en_viewport_set_width(EnViewport* v, int32_t w, int hw) {
    v->w = w;
    v->sx = hw ? (float)w : (float)((double)shl(w, 5) + -0.5);
}

uint32_t en_viewport_data(int32_t cur) { return 0x00a03b70u + 0x38u + 0x94u * (uint32_t)cur; }

uint32_t en_view_entity(const uint32_t tab[2], int32_t i) { return i < 2 ? tab[i] : tab[0]; }

uint32_t en_fog_color(uint8_t r, uint8_t g, uint8_t b) { return (uint32_t)r << 16 | (uint32_t)g << 8 | b; }

uint32_t en_light_env_addr(int32_t i) { return i < 4 ? 0x00a041c0u + 0x6cu * (uint32_t)i : 0x00a041c0u; }

void en_light_set_pos(EnLights* l, int32_t e, int32_t j, int16_t x, int16_t y, int16_t z) {
    if (e > 4 || j >= 3 || e < 0 || j < 0) return;
    l->pos[j][0] = x; l->pos[j][1] = y; l->pos[j][2] = z;
    l->env[e][3 + j] = shl(x, 4);
    l->env[e][6 + j] = shl(y, 4);
    l->env[e][9 + j] = shl(z, 4);
}

/* direction (.12) from the angles: -(-(sin a cos b) / 0xfff), -(-(cos a cos b) / 0xfff), -sin(-b')
 * with a = az * 4096 / 360, b = el * 4096 / 360, b' = (-el) * 4096 / 360 (all truncating) */
static void dir(EnLights* l, int32_t e, int32_t j, int32_t az, int32_t el) {
    int32_t a = shl(az, 12) / 0x168;
    int32_t sa = tm2_sin(a);
    int32_t b = shl(el, 12) / 0x168;
    int32_t t0 = (int32_t)(0u - (uint32_t)(sa * tm2_cos(b))) / 0xfff;
    int32_t t1 = (int32_t)(0u - (uint32_t)(tm2_cos(a) * tm2_cos(b))) / 0xfff;
    int32_t s = tm2_sin(shl((int32_t)(0u - (uint32_t)el), 12) / 0x168);
    l->env[e][15 + 3 * j] = (int32_t)(0u - (uint32_t)t0);
    l->env[e][16 + 3 * j] = (int32_t)(0u - (uint32_t)t1);
    l->env[e][17 + 3 * j] = (int32_t)(0u - (uint32_t)s);
}

void en_light_set_dir(EnLights* l, int32_t e, int32_t j, int16_t az, int16_t el) {
    if (e > 4 || j >= 3 || e < 0 || j < 0) return;
    l->deg[j][1] = 0;
    l->deg[j][2] = az;
    l->deg[j][0] = el;
    dir(l, e, j, az, el);
}

void en_light_set(EnLights* l, int32_t e, int32_t j, int32_t az, int32_t el, int16_t x, int16_t y, int16_t z) {
    if (e > 4 || j >= 3 || e < 0 || j < 0) return;
    dir(l, e, j, az, el);
    l->env[e][6 + j] = shl(y, 4);
    l->env[e][3 + j] = shl(x, 4);
    l->env[e][9 + j] = shl(z, 4);
}

void en_views_init(EnViews* v, int32_t* cur) {
    memset(v, 0, sizeof *v);
    *cur = 0;
}

static int32_t half(int32_t v) { return (int32_t)((uint32_t)v - (uint32_t)(v >> 31)) >> 1; }   /* idiv by 2 */

void en_view_fixed_camera(EnViewport* vp, const EnViews* views, const uint8_t* vports, int32_t* root, const int32_t* cam,
                          int32_t i, int32_t dist, int32_t mode, int hw) {
    int32_t w, h;
    memcpy(&w, vports + 0x94 * (uint32_t)views->slot[i] + 0x18, 4);
    memcpy(&h, vports + 0x94 * (uint32_t)views->slot[i] + 0x1c, 4);
    w = half(w);
    h = half(h);
    int32_t m = h > w ? h : w;
    int32_t k = (int32_t)((uint32_t)m - 1u);
    int32_t* r = root + 5;   /* +0x14 */
    if (mode == 0) {
        static const int32_t t[12] = { 0, 0, 0, 0x1000, 0, 0, 0, -0x1000, 0, 0, 0, -0x1000 };
        memcpy(r, t, sizeof t);
        r[2] = (int32_t)((0u - (uint32_t)dist) * (uint32_t)k);
    } else {
        static const int32_t t[12] = { 0, 0, 0, 0x1000, 0, 0, 0, 0, -0x1000, 0, 0x1000, 0 };
        memcpy(r, t, sizeof t);
        r[1] = (int32_t)((uint32_t)dist * (uint32_t)k);
    }
    EnViewSize vs = { vp->w, vp->h, vp->sx, vp->sy };
    en_view_set_size(&vs, m, m, hw);
    vp->w = vs.w; vp->h = vs.h; vp->sx = vs.scale_x; vp->sy = vs.scale_y;
    en_viewport_set_origin(vp, w, (int32_t)((uint32_t)h - 1u), hw);
    memcpy(root + 0x50 / 4, cam + 0x50 / 4, 12 * sizeof(int32_t));
    memcpy(root + 0x80 / 4, cam + 0x80 / 4, 12 * sizeof(int32_t));
}

static int32_t vp32(const EnViewBlock* b, int32_t slot, uint32_t off) {
    int32_t v;
    memcpy(&v, (const uint8_t*)b->vports + 0x94 * (uint32_t)slot + off, 4);
    return v;
}

void en_view_proc(EnViewport* vp, EnViewBlock* b, int32_t* root, int32_t* cam, int32_t* tgt, int32_t trans[3],
                  int32_t i, int hw) {
    if (i >= 2) return;
    int16_t ang[3];
    memcpy(ang, (uint8_t*)cam + 0xe0, 6);
    tm2_build_matrix(ang, cam + 0xb0 / 4);
    memcpy(ang, (uint8_t*)tgt + 0xe0, 6);
    tm2_build_matrix(ang, tgt + 0xb0 / 4);
    int32_t slot = b->views.slot[i];
    b->rot[0] = vp32(b, slot, 0x38);
    b->rot[5] = (int32_t)(0u - (uint32_t)vp32(b, slot, 0x40));
    b->rot[7] = vp32(b, slot, 0x3c);
    int32_t t1[12], t2[12], t3[12], t4[12];
    tm2_matrix_load(b->rot);
    tm2_matrix_multiply(cam + 0xb0 / 4, t1);
    tm2_matrix_load(t1);
    tm2_matrix_multiply(tgt + 0xb0 / 4, root + 0x20 / 4);
    int32_t n[3], t[3];
    for (int k = 0; k < 3; k++) n[k] = (int32_t)(0u - (uint32_t)cam[0xe8 / 4 + k]);
    tm2_mat_vec_mul_row(tgt + 0xb0 / 4, n, t);
    for (int k = 0; k < 3; k++) root[0x14 / 4 + k] = (int32_t)((uint32_t)t[k] - (uint32_t)tgt[0xe8 / 4 + k]);
    en_mat_vec_mul(root + 0x20 / 4, root + 0x14 / 4, root + 0x44 / 4);
    memcpy(b->render_pos[i], root + 0x14 / 4, 12);
    memcpy(b->cam_mat[i], root + 0x20 / 4, 48);
    tm2_matrix_transpose(cam + 0xb0 / 4, t1);
    tm2_matrix_transpose(tgt + 0xb0 / 4, t4);
    tm2_matrix_load(t1);
    tm2_matrix_multiply(cam + 0x50 / 4, t3);
    tm2_matrix_load(t1);
    tm2_matrix_multiply(cam + 0x80 / 4, t2);
    tm2_matrix_load(t4);
    tm2_matrix_multiply(t3, root + 0x50 / 4);
    tm2_matrix_load(t4);
    tm2_matrix_multiply(t2, root + 0x80 / 4);
    trans[0] = trans[1] = trans[2] = 0;
    EnViewSize vs = { vp->w, vp->h, vp->sx, vp->sy };
    /* the size and origin come from viewport i, not the view's slot (the tilt above does) */
    en_view_set_size(&vs, vp32(b, i, 0x8), vp32(b, i, 0xc), hw);
    vp->w = vs.w; vp->h = vs.h; vp->sx = vs.scale_x; vp->sy = vs.scale_y;
    en_viewport_set_origin(vp, (int32_t)((uint32_t)half(vp32(b, i, 0x18)) + (uint32_t)vp32(b, i, 0x20)),
                           (int32_t)((uint32_t)vp32(b, i, 0x24) + (uint32_t)half(vp32(b, i, 0x1c))), hw);
}
