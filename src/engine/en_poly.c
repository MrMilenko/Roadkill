/*
 * en_poly.c: the scene's polygon renderer 0x0048e9b8 (see en_poly.h).
 */
#pragma STDC FP_CONTRACT OFF
#include "en_poly.h"

#include "en_camera.h"
#include "en_fog.h"
#include "en_quad.h"
#include "en_split.h"
#include "en_submit.h"
#include "../render/gpu.h"
#include "../math/tm2math.h"

#include <math.h>
#include <string.h>

#define D(x) ((double)(x))

static uint8_t* M(EnPoly* p, uint32_t a) { return p->mem(p->user, a); }
static uint8_t r8(EnPoly* p, uint32_t a) { return *M(p, a); }
static uint16_t r16(EnPoly* p, uint32_t a) { uint16_t v; memcpy(&v, M(p, a), 2); return v; }
static uint32_t r32(EnPoly* p, uint32_t a) { uint32_t v; memcpy(&v, M(p, a), 4); return v; }
static int16_t s16(EnPoly* p, uint32_t a) { return (int16_t)r16(p, a); }
static float fl(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static uint32_t fb(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

static uint32_t r32ofs(const uint8_t* b, uint32_t o) { uint32_t v; memcpy(&v, b + o, 4); return v; }

static int32_t fistp(double d) {
    if (!(d > -2147483648.5 && d < 2147483647.5)) return INT32_MIN;
    return (int32_t)nearbyint(d);
}

/* x87 compare of a with b (fld a; fcomp b): CF = a < b or unordered, ZF = a == b or unordered */
static int cf(float a, float b) { return !(a >= b); }
static int zf(float a, float b) { return a == b || a != a || b != b; }

static void xform_vert(EnPoly* p, uint32_t vaddr, float out[3]) {
    int16_t v[3] = { s16(p, vaddr), s16(p, vaddr + 2), s16(p, vaddr + 4) };
    en_xform_short(&p->xf, v, out);
}

static void quad_in(EnPoly* p, EnQuadEnv* q) {
    memset(q, 0, sizeof *q);
    q->vtx = p->vtx;
    q->vert_count = p->a.count;
    memcpy(q->ptr, p->a.v, sizeof q->ptr);
    q->tex_inv = p->tex_inv; q->fov_x = p->fov_x;
    q->size_x = p->size_x; q->size_y = p->size_y;
    q->zmax = p->zmax; q->zmin = p->zmin;
    q->render_flags = (uint8_t)p->render_flags;
    q->poly = M(p, p->poly);
    q->verts = M(p, p->verts);
    q->wave_a = p->wave_a; q->wave_b = p->wave_b;
}
static void quad_out(EnPoly* p, const EnQuadEnv* q) {
    p->a.count = q->vert_count;
    memcpy(p->a.v, q->ptr, sizeof q->ptr);
    p->size_x = q->size_x; p->size_y = q->size_y;
    p->zmax = q->zmax; p->zmin = q->zmin;
    p->render_flags = (p->render_flags & ~0xffu) | q->render_flags;
}

static void ot_full(void* u, int32_t bucket, uint32_t rec) { EnPoly* p = u; p->ot_insert(p->user, bucket, rec); }
static void ot_split(void* u, int32_t bucket, uint32_t rec) {
    EnPoly* p = u;
    p->ot_link(p->user, p->ot_base + 4u * (uint32_t)bucket, rec);
}
static void reproject(void* u, EnVertex* v) { EnPoly* p = u; en_project(&p->proj, v); }

static void submit(EnPoly* p) {
    EnSubmitEnv s;
    memset(&s, 0, sizeof s);
    s.clip = p->b_is_a ? &p->a : &p->b;
    s.tri = p->vtx;
    s.color = p->color; s.depth = p->depth; s.depth_fixed = p->depth_fixed; s.depth_bias = p->depth_bias;
    s.render_flags = (uint16_t)p->render_flags;
    s.page_ref = p->page_ref; s.fog = p->fog; s.fog_dist = p->fog_dist;
    s.zmin = p->zmin; s.zmax = p->zmax; s.fog_sub = p->q[0];
    s.normalize = p->clipped; s.format = p->format;
    memcpy(s.q, p->q, sizeof s.q);
    s.light_index = p->light_index; s.lights = p->lights;
    s.prim = p->prim; s.prim_addr = p->prim_addr; s.cursor = p->cursor;
    s.normalize_fn = reproject; s.user = p;
    if (p->split) {
        EnSplitExtra x = { p->shade, p->semi, p->blend, p->tex_table, p->tex_count, 0 };
        s.ot_insert = ot_split;
        en_split_build(&s, &x);
        p->end_split = x.end;
        p->color = s.color;
        p->render_flags = (p->render_flags & ~0xffffu) | s.render_flags;
    } else {
        s.ot_insert = ot_full;
        if (p->render_flags & 0x10) en_submit_gradients(&s);
        en_submit_prim(&s);
        p->end = s.end;
    }
    p->rec = s.rec;
    p->cursor = s.cursor;
}

/* clip passes and everything after; returns when the polygon is dropped or submitted */
static void finish(EnPoly* p, uint32_t node, uint32_t poly, uint32_t uv_end, uint32_t page_word, int special) {
    EnClipEnv ce = { &p->vtx[p->a.count], p->interp_mask, 0, p->depth_scale };
    ce.clipped = p->clipped;
    /* the pool is EN_POLY_POOL entries; the original's clip passes stay within it for <= 7 vertices */
    en_clip_plane(&ce, &p->a, &p->b, 2, 10, 1);
    p->clipped = ce.clipped;
    if (p->b.count < 3) return;
    en_clip_plane(&ce, &p->b, &p->a, 2, 0, 1); p->clipped = ce.clipped;
    if (p->a.count < 3) return;
    en_clip_plane(&ce, &p->a, &p->b, 2, 0, -1); p->clipped = ce.clipped;
    if (p->b.count < 3) return;
    en_clip_plane(&ce, &p->b, &p->a, 2, 1, 1); p->clipped = ce.clipped;
    if (p->a.count < 3) return;
    en_clip_plane(&ce, &p->a, &p->b, 2, 1, -1); p->clipped = ce.clipped;
    if (p->b.count < 3) return;
    if (p->far_clip) {
        en_clip_plane(&ce, &p->b, &p->a, 0xb, 2, 1); p->clipped = ce.clipped;
        if (p->a.count < 3) return;
        p->b_is_a = 1;
    }
    EnClipBuf* B = p->b_is_a ? &p->a : &p->b;

    if (r16(p, node + 0x1a) & 0x7ff8) {
        p->depth = (int32_t)((r32(p, node + 0x18) << 1) >> 20);
        p->depth_fixed = 1;
    } else {
        uint32_t obj = r32(p, p->item + 0x14);
        int32_t d = (int32_t)r32(p, obj);
        if (d != 0) {
            p->depth = d;
            p->depth_fixed = 1;
        } else {
            p->zmax = fl(0);
            p->zmin = 32768.0f;
            for (int32_t i = 0; i < B->count; i++) {
                float z = B->v[i]->z;
                if (!(cf(z, p->zmax) || zf(z, p->zmax))) p->zmax = z;   /* jbe skips */
                if (cf(z, p->zmin)) p->zmin = z;                         /* jae skips */
            }
            p->depth = fistp(p->zmax);
        }
    }

    if (r8(p, poly) & 0x7f) {
        p->page_ref = (p->page_ref & 0xffffu) | (uint32_t)(page_word & 0x1ff) << 16;
        if (p->alpha_test == 0 || cf(p->zmin, p->alpha_z)) p->render_flags |= 1;
        int32_t pg = (int32_t)p->page_ref >> 16;
        if (pg >= 0 && pg < p->tex_count && r32ofs(p->tex_table, 0x30u * (uint32_t)pg + 0x10) != 0) p->render_flags |= 8;
        p->blend = (r16(p, uv_end) & 0x3000u) >> 12;
    } else {
        p->blend = 3;
        p->render_flags &= ~0x11u;
    }
    for (int32_t i = 0; i < B->count; i++) en_project(&p->proj, B->v[i]);
    if (s16(p, poly + 0x10) == 0x2000 && !en_winding_check(B, p->split)) return;

    if (p->fog) {
        uint32_t obj = r32(p, p->item + 0x14);
        int16_t ov = s16(p, obj + 0x164);
        uint32_t w14 = r32(p, poly + 0x14);
        if (ov != 0) {
            p->fog_dist = (float)(D(p->fog_k) / D(ov));
        } else if ((w14 & 0x7e) == 0x7e) {
            p->fog_dist = 1e6f;
            p->render_flags |= 0x40;
        } else {
            uint32_t k = (w14 << 25) >> 26;
            p->fog_dist = (float)(D(k) * D(p->fog_k) * p->fog_k2);
            if (!(p->render_flags & 1)) {
                if (!cf(p->zmin, p->fog_dist)) p->color = p->fog_color;
                else p->color = en_color_interpolate(p->zmin, 0.0f, p->fog_dist, p->color, p->fog_color);
            } else if (!(cf(p->zmin, p->fog_dist) || zf(p->zmin, p->fog_dist))) {
                p->color = p->fog_color;
                p->render_flags &= ~0x11u;
            }
        }
    } else {
        p->fog_dist = 1e6f;
    }
    if (special == 1) p->render_flags |= 2;
    int32_t pg = (int32_t)p->page_ref >> 16;
    if (p->q_enable && (p->render_flags & 1) && pg >= 0 && pg < p->tex_count &&
        r32ofs(p->tex_table, 0x30u * (uint32_t)pg + 0x14) != 0) {
        EnFogEnv f;
        memset(&f, 0, sizeof f);
        f.lights = p->lights; f.clip = B; f.poly = M(p, p->poly); f.verts = M(p, p->verts);
        f.wave_a = p->wave_a; f.wave_b = p->wave_b; f.render_flags = p->render_flags;
        f.light_index = p->light_index;
        en_light_terms(&f);
        p->render_flags = f.render_flags; p->light_index = f.light_index;
        memcpy(p->q, f.q, sizeof p->q);
    } else {
        p->q[1] = fl(0); p->q[0] = fl(0); p->q[2] = fl(0);
    }
    submit(p);
}

void en_poly_render(EnPoly* p, uint32_t items, int32_t count, uint32_t mats) {
    double P = D(p->view_w) * p->fov_base;
    p->fov_x = (float)P;
    double ix = 1.0 / P;
    double Q = p->fov_base * D(p->view_h);
    p->fov_y = (float)Q;
    p->inv_fov_y = (float)(1.0 / Q);
    p->inv_fov_x = (float)ix;
    uint32_t t = (p->elapsed / 0x989u) * 100u;
    p->wave_a = ((int32_t)t / 0x14) % 0xfff;
    p->wave_b = ((int32_t)t / 0x1d) % 0xfff;
    p->fog_rgb_copy = p->fog_rgb;
    p->page_ref = (p->page_ref & 0xffff0000u) | (gpu_rgb24_to_16(p->format, p->fog_rgb) & 0xffffu);
    if (!p->split) p->interp_mask = 7;
    int32_t last_slot = -1;
    for (int32_t it = 0; it < count; it++) {
        uint32_t item = items + 0x20u * (uint32_t)it;
        p->item = item;
        uint32_t node = r32(p, item);
        int32_t pos[3] = { (int32_t)r32(p, item + 4), (int32_t)r32(p, item + 8), (int32_t)r32(p, item + 12) };
        en_view_load_pos(&p->xf, pos);
        int32_t slot = (int32_t)r32(p, item + 0x18);
        if (slot != last_slot) {
            last_slot = slot;
            int32_t m[9];
            for (int k = 0; k < 9; k++) m[k] = (int32_t)r32(p, mats + 0x30u * (uint32_t)slot + 4u * (uint32_t)k);
            en_view_load_rot(&p->xf, m);
        }
        p->verts = r32(p, node + 4);
        p->node8 = r32(p, node + 8);
        p->poly = r32(p, node + 0xc);
        p->kind = r16(p, node + 2);
        p->tex_shift = (int32_t)r32(p, item + 0x10);
        p->tex_inv = (float)(1.0 / D((int32_t)(1u << (p->tex_shift & 31))));
        int special;
        if (p->kind == 700) {
            if (p->special_mode == 0) continue;
            special = 1;
        } else {
            special = 0;
        }
        uint32_t root = r32(p, item + 0x14);
        if (p->camera_obj == root) {
            if (p->fog_mode == 0) continue;
            p->far_clip = 0;
            p->fog = 0;
        } else {
            p->fog = p->fog_on;
            p->far_clip = 1;
        }
        uint32_t npoly = r32(p, node + 0x10);
        for (uint32_t pi = 0; pi < npoly; pi++) {
            uint32_t poly = p->poly;
            p->render_flags = p->fog ? 0x10 : 0x50;
            uint32_t w10 = r32(p, poly + 0x10);
            if ((w10 & 0x70000) > 0x20000 && !special && (int16_t)w10 != 0x2000) {
                int32_t nz = (int16_t)w10;
                uint32_t va = p->verts + 8u * (uint32_t)(int32_t)s16(p, poly + 4);
                int64_t P0 = (int32_t)((uint32_t)pos[0] + (uint32_t)(int32_t)s16(p, va));
                int64_t P1 = (int32_t)((uint32_t)pos[1] + (uint32_t)(int32_t)s16(p, va + 2));
                int64_t P2 = (int32_t)((uint32_t)pos[2] + (uint32_t)(int32_t)s16(p, va + 4));
                int64_t sum = s16(p, poly + 0xe) * P1 + s16(p, poly + 0xc) * P0 + nz * P2;   /* exact in the x87 too */
                if (!(sum < 0)) goto next;
            }
            {
                p->b_is_a = 0;
                uint32_t cols = poly + 0x18;
                p->color = r32(p, cols) & 0xffffff;
                uint32_t uv = cols + 4u * ((w10 << 1) >> 29);
                p->clipped = 0;
                p->depth_fixed = 0;
                p->normals = cols + 4u * ((w10 << 4) >> 28);
                p->semi = r8(p, cols + 3) & 2;
                uint32_t uv_end = uv + 4u * (r8(p, poly) & 0x7f);
                if (p->split) {
                    p->shade = r8(p, poly + 1);
                    p->interp_mask = p->shade > 1 ? 0x39f : 0x1f;
                }
                p->depth_bias = (int32_t)(r32(p, poly + 0x14) << 19) >> 26;
                p->bits19 = (w10 << 10) >> 29;
                p->a.count = (int32_t)((w10 << 13) >> 29);
                uint32_t idx0 = p->verts + 8u * (uint32_t)(int32_t)s16(p, poly + 4);
                if (p->a.count < 2) {
                    float ds = fl(p->depth_scale);
                    for (int i = 0; i < 4; i++) {
                        EnVertex* v = &p->vtx[i];
                        p->a.v[i] = v;
                        v->near_z = fl(0x3dcccccd);
                        v->depth_scale = ds;
                        v->u = (float)(int32_t)((uint32_t)r16(p, uv) * 0x1000u + 0x800u);
                        v->v = (float)(int32_t)((uint32_t)r16(p, uv + 2) * 0x1000u + 0x800u);
                        uv += 4;
                    }
                    EnQuadEnv q;
                    int ok;
                    if (p->a.count == 1) {
                        int16_t len = s16(p, poly + 8);
                        int16_t a[3] = { s16(p, idx0), s16(p, idx0 + 2), (int16_t)(s16(p, idx0 + 4) + len) };
                        int16_t b[3] = { a[0], a[1], (int16_t)(s16(p, idx0 + 4) - len) };
                        p->size_x = (float)s16(p, poly + 6);
                        en_xform_short(&p->xf, a, &p->vtx[4].x);
                        en_xform_short(&p->xf, b, &p->vtx[5].x);
                        quad_in(p, &q);
                        ok = en_line_segment_quad(&q);
                    } else {
                        xform_vert(p, idx0, &p->a.v[0]->x);
                        p->size_x = (float)(D(s16(p, poly + 6)) * D(p->fov_x) * D(p->tex_inv));
                        p->size_y = (float)(D(s16(p, poly + 8)) * D(p->fov_y) * D(p->tex_inv));
                        quad_in(p, &q);
                        ok = en_billboard_quad(&q);
                    }
                    quad_out(p, &q);
                    if (!ok) goto next;
                } else if (p->a.count >= 3) {
                    p->zmax = fl(0);
                    p->zmin = fl(0x46fffe00);
                    for (int32_t i = 0; i < p->a.count; i++) {
                        EnVertex* v = &p->vtx[i];
                        p->a.v[i] = v;
                        memcpy(&v->near_z, &(uint32_t){ 0x3dcccccd }, 4);
                        memcpy(&v->depth_scale, &p->depth_scale, 4);
                        v->u = (float)(int32_t)((uint32_t)r16(p, uv) * 0x1000u + 0x800u);
                        v->v = (float)(int32_t)((uint32_t)r16(p, uv + 2) * 0x1000u + 0x800u);
                        uv += 4;
                        if (p->shade > 1) {
                            uint32_t c = r32(p, cols);
                            v->r = (float)((c & 0xff0000) >> 16);
                            v->g = (float)((c & 0xff00) >> 8);
                            v->b = (float)(c & 0xff);
                            cols += 4;
                        }
                        xform_vert(p, p->verts + 8u * (uint32_t)(int32_t)s16(p, poly + 4 + 2u * (uint32_t)i), &v->x);
                        double ti = D(p->tex_inv);
                        v->x = (float)(ti * D(v->x));
                        v->y = (float)(ti * D(v->y));
                        v->z = (float)(ti * D(v->z));
                        if (!cf(p->zmin, v->z)) p->zmin = v->z;       /* jae: zmin >= z */
                        if (cf(p->zmax, v->z)) p->zmax = v->z;        /* jb: zmax < z or NaN */
                    }
                    if ((int32_t)fb(p->zmax) <= 0x3f800000) goto next;
                    if (p->kind == 699) {
                        EnQuadEnv q;
                        quad_in(p, &q);
                        en_water_uv_animate(&q);
                        quad_out(p, &q);
                    }
                }
                finish(p, node, poly, uv_end, r16(p, uv), special);
            }
        next:
            p->poly = poly + 4u * r8(p, poly + 2);
        }
    }
}

/* 0x00489d20 fxDebrisDraw: the captured meshes of the active debris bursts (src/engine/en_debris.h,
 * 4 x 0x238 at bursts) in four pieces each. root: the scene root entity (csGetRoot 0x00492c44). */
void en_poly_debris(EnPoly* p, uint32_t bursts, uint32_t root) {
    for (uint32_t bi = 0; bi < 4; bi++) {
        uint32_t bu = bursts + 0x238u * bi;
        if (r32(p, bu) == 0) continue;
        int32_t bm[9], bt[9], vm[9], rm[9];
        for (int k = 0; k < 9; k++) {
            bm[k] = (int32_t)r32(p, bu + 0xc8 + 4u * (uint32_t)k);
            rm[k] = (int32_t)r32(p, root + 0x20 + 4u * (uint32_t)k);
        }
        tm2_matrix_transpose(bm, bt);
        tm2_matrix_load(rm);
        tm2_matrix_multiply(bt, vm);
        en_view_load_rot(&p->xf, vm);
        int32_t zero3[3] = { 0, 0, 0 };
        en_view_load_pos(&p->xf, zero3);
        int32_t nmesh = (int32_t)r32(p, bu + 4);
        for (int32_t mi = 0; mi < nmesh; mi++) {
            uint32_t ms = bu + 0x14u * (uint32_t)mi;
            /* the pieces, then (as the original's stack) the transposed burst matrix and 3 words
             * the original never wrote (piece index 7 reads stack garbage: zeros here) */
            int32_t pc[8][3];
            memset(pc, 0, sizeof pc);
            int32_t pos[3];
            for (int k = 0; k < 3; k++)
                pos[k] = (int32_t)(r32(p, ms + 0xfc + 4u * (uint32_t)k) + r32(p, root + 0x14 + 4u * (uint32_t)k));
            for (int k = 0; k < 4; k++) {
                for (int j = 0; j < 3; j++) pos[j] = (int32_t)((uint32_t)pos[j] + r32(p, bu + 0x44 + 12u * (uint32_t)k + 4u * (uint32_t)j));
                tm2_mat_vec_mul_col(bm, pos, pc[k]);
            }
            memcpy(&pc[4][0], bt, sizeof bt);
            uint32_t node = r32(p, ms + 0xf8);
            uint8_t shift = r8(p, ms + 0x108);
            uint32_t verts = r32(p, node + 4);
            uint32_t poly = r32(p, node + 0xc);
            uint32_t npoly = r32(p, node + 0x10);
            for (uint32_t pi = 0; pi < npoly; pi++, poly += 4u * r8(p, poly + 2)) {
                uint32_t k = (r32(p, poly + 0x14) << 16) >> 29;
                if ((int32_t)(r32(p, bu + 0x4c + 12u * k) + r32(p, ms + 0x104)) < 0) continue;
                uint32_t w10 = r32(p, poly + 0x10);
                p->a.count = (int32_t)((w10 << 13) >> 29);
                if (p->a.count < 3) continue;
                p->b_is_a = 0;
                p->render_flags = 0x10;
                uint32_t cols = poly + 0x18;
                p->clipped = 0;
                p->color = r32(p, cols);
                float zmax = fl(0);
                uint32_t uv = cols + 4u * ((w10 << 1) >> 29);
                for (int32_t i = 0; i < p->a.count; i++) {
                    uint32_t va = verts + 8u * (uint32_t)(int32_t)s16(p, poly + 4 + 2u * (uint32_t)i);
                    int16_t v3[3] = { (int16_t)(r16(p, va) + (uint16_t)pc[k][0]), (int16_t)(r16(p, va + 2) + (uint16_t)pc[k][1]),
                                      (int16_t)(r16(p, va + 4) + (uint16_t)pc[k][2]) };
                    EnVertex* v = &p->vtx[i];
                    p->a.v[i] = v;
                    v->near_z = fl(0x3dcccccd);
                    v->depth_scale = fl(p->depth_scale);
                    en_xform_short(&p->xf, v3, &v->x);
                    if (cf(zmax, v->z)) zmax = v->z;
                    if (r8(p, poly) & 0x7f) {
                        v->u = (float)(int32_t)((uint32_t)r16(p, uv) * 0x1000u + 0x800u);
                        v->v = (float)(int32_t)((uint32_t)r16(p, uv + 2) * 0x1000u + 0x800u);
                        uv += 4;
                    }
                    /* the gouraud colors under render flag 0x1000 are dead code: the flags are 0x10 */
                }
                p->depth = fistp(trunc(D(zmax))) >> (shift & 31);
                if (p->depth <= 0) continue;
                if (r16(p, node + 0x1a) & 0x7ff8) p->depth = (int32_t)((r32(p, node + 0x18) << 1) >> 20);
                if (r8(p, poly) & 0x7f) {
                    p->page_ref = (p->page_ref & 0xffffu) | (uint32_t)(r16(p, uv) & 0x1ff) << 16;
                    int32_t pg = (int32_t)p->page_ref >> 16;
                    if (pg >= 0 && pg < p->tex_count && r32ofs(p->tex_table, 0x30u * (uint32_t)pg + 0x10) != 0) p->render_flags |= 8;
                    p->render_flags |= 1;
                }
                EnClipEnv ce = { &p->vtx[p->a.count], p->interp_mask, 0, p->depth_scale };
                en_clip_plane(&ce, &p->a, &p->b, 2, 10, 1); p->clipped = ce.clipped;
                if (p->b.count < 3) continue;
                en_clip_plane(&ce, &p->b, &p->a, 2, 0, 1); p->clipped = ce.clipped;
                if (p->a.count < 3) continue;
                en_clip_plane(&ce, &p->a, &p->b, 2, 0, -1); p->clipped = ce.clipped;
                if (p->b.count < 3) continue;
                en_clip_plane(&ce, &p->b, &p->a, 2, 1, 1); p->clipped = ce.clipped;
                if (p->a.count < 3) continue;
                en_clip_plane(&ce, &p->a, &p->b, 2, 1, -1); p->clipped = ce.clipped;
                if (p->b.count < 3) continue;
                for (int32_t i = 0; i < p->b.count; i++) en_project(&p->proj, p->b.v[i]);
                if (!en_winding_check(&p->b, p->split)) continue;
                int split = p->split;
                p->split = 0;           /* always the full screen submit */
                submit(p);
                p->split = split;
            }
        }
    }
}
