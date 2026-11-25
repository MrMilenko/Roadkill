/*
 * en_submit.c: GPU primitive from a clipped polygon (see en_submit.h).
 */
#include "en_submit.h"

#include "../render/gpu.h"

#include <math.h>
#include <string.h>

/* fistp under round to nearest; out of range and NaN give the integer indefinite */
static int32_t fistp(double d) {
    if (!(d > -2147483648.5 && d < 2147483647.5)) return INT32_MIN;
    return (int32_t)nearbyint(d);
}

static uint8_t* at(EnSubmitEnv* e, uint32_t a) { return e->prim + (a - e->prim_addr); }
static void w8(EnSubmitEnv* e, uint32_t a, uint8_t v) { *at(e, a) = v; }
static void w16(EnSubmitEnv* e, uint32_t a, uint16_t v) { memcpy(at(e, a), &v, 2); }
static void w32(EnSubmitEnv* e, uint32_t a, uint32_t v) { memcpy(at(e, a), &v, 4); }
static void wf(EnSubmitEnv* e, uint32_t a, float v) { memcpy(at(e, a), &v, 4); }
static int32_t r32(EnSubmitEnv* e, uint32_t a) { int32_t v; memcpy(&v, at(e, a), 4); return v; }

/* 0x0048f798 */
int32_t en_depth_bucket(const EnSubmitEnv* e, int32_t d) {
    if (e->depth_fixed) {
        if (d >= 0xfff) return 0xffe;
        return d >= 1 ? d : 1;
    }
    int32_t b;
    if (d < 0x400) {
        b = d;
        if (d > 0 && !(e->render_flags & 0x2000)) {
            b = (int32_t)((uint32_t)e->depth_bias + (uint32_t)d);
            if (b < 1) b = 1;
        }
    } else if (d < 0x1000) {
        b = ((d - 0x400) >> 2) + 0x400;
    } else if (d < 0x4000) {
        b = ((d - 0x1000) >> 3) + 0x700;
    } else {
        b = ((d - 0x4000) >> 5) + 0xd00;
    }
    return b < 0xfff ? b : 0xffe;
}

/* the screen positions of the clip buffer's vertices from a */
static uint32_t put_xy(EnSubmitEnv* e, uint32_t a) {
    for (int32_t i = 0; i < e->clip->count; i++) {
        w32(e, a, (uint32_t)fistp(e->clip->v[i]->sx));
        w32(e, a + 4, (uint32_t)fistp(e->clip->v[i]->sy));
        *at(e, a) |= 1;
        *at(e, a + 4) |= 1;
        a += 8;
    }
    return a;
}

/* 0x0048fd54 */
void en_submit_prim(EnSubmitEnv* e) {
    uint32_t r = e->cursor;
    if (e->clip->count == 2) {
        w8(e, r + 4, (uint8_t)(e->color >> 16));
        w8(e, r + 7, 0x40);
        w8(e, r + 5, (uint8_t)(e->color >> 8));
        e->rec = r;
        w8(e, r + 6, (uint8_t)e->color);
        w16(e, r + 0x8, (uint16_t)((fistp(e->clip->v[0]->sx) >> 5) - 0x10));
        w16(e, r + 0xa, (uint16_t)((fistp(e->clip->v[0]->sy) >> 5) - 0x10));
        w16(e, r + 0xc, (uint16_t)((fistp(e->clip->v[1]->sx) >> 5) - 0x10));
        w16(e, r + 0xe, (uint16_t)((fistp(e->clip->v[1]->sy) >> 5) - 0x10));
        e->ot_insert(e->user, en_depth_bucket(e, e->depth), r);
        e->cursor = r + 0x10;
        return;
    }
    uint8_t f = (uint8_t)e->render_flags;
    if ((f & 1) && !(f & 0x10)) {
        if (e->normalize) {
            e->normalize_fn(e->user, &e->tri[0]);
            e->normalize_fn(e->user, &e->tri[1]);
            e->normalize_fn(e->user, &e->tri[2]);
        }
        w32(e, r + 0x44, (uint32_t)((int32_t)e->page_ref >> 16));
        w8(e, r + 7, 0x90);
        w8(e, r + 6, (uint8_t)e->clip->count);
        e->rec = r;
        w16(e, r + 4, e->render_flags);
        if (e->fog) {
            float t = (float)((double)4096.0f / (double)e->fog_dist * (double)e->zmin);
            if (f & 0x80) t = (float)((double)t - (double)e->fog_sub);
            w32(e, r + 8, (uint32_t)fistp(t));
        }
        if (r32(e, r + 8) < 0) w32(e, r + 8, 0);
        if (r32(e, r + 8) > 0xfff) w32(e, r + 8, 0xfff);
        w32(e, r + 8, (uint32_t)r32(e, r + 8) & 0x1f00);
        const EnVertex* t = e->tri;
        w32(e, r + 0x0c, (uint32_t)fistp(t[0].u));
        w32(e, r + 0x10, (uint32_t)fistp(t[0].v));
        w32(e, r + 0x14, (uint32_t)fistp(t[1].u));
        w32(e, r + 0x18, (uint32_t)fistp(t[1].v));
        w32(e, r + 0x1c, (uint32_t)fistp(t[2].u));
        w32(e, r + 0x20, (uint32_t)fistp(t[2].v));
        w32(e, r + 0x24, (uint32_t)fistp(t[0].sx));
        w32(e, r + 0x28, (uint32_t)fistp(t[0].sy));
        w32(e, r + 0x2c, (uint32_t)fistp(t[1].sx));
        w32(e, r + 0x30, (uint32_t)fistp(t[1].sy));
        w32(e, r + 0x34, (uint32_t)fistp(t[2].sx));
        w32(e, r + 0x38, (uint32_t)fistp(t[2].sy));
        e->end = put_xy(e, r + 0x48);
    } else if (f & 1) {
        float lo = (float)((double)e->zmin * 0.8), hi = (float)((double)e->zmax * 1.2);
        e->rec = r;
        w32(e, r + 8, e->color);
        w16(e, r + 0xc, (uint16_t)(e->page_ref >> 16));
        w8(e, r + 7, 0xa0);
        wf(e, r + 0x10, e->fog_dist);
        wf(e, r + 0x68, lo);
        w8(e, r + 6, (uint8_t)e->clip->count);
        wf(e, r + 0x6c, hi);
        w16(e, r + 4, e->render_flags);
        e->end = put_xy(e, r + 0x78);
    } else {
        e->rec = r;
        w16(e, r + 4, (uint16_t)gpu_rgb24_to_16(e->format, e->color));
        w8(e, r + 7, 0xb0);
        w8(e, r + 6, (uint8_t)e->clip->count);
        e->end = put_xy(e, r + 8);
    }
    e->ot_insert(e->user, en_depth_bucket(e, e->depth), r);
    e->cursor = e->end;
}


#define D(x) ((double)(x))

/* 0x0048f838: generated from the machine code's x87 sequence (each fst / fstp dword is a float
 * rounding, everything else stays on the 53 bit stack) */
void en_submit_gradients(EnSubmitEnv* e) {
    uint16_t f = e->render_flags;
    if (!(f & 0x4081)) return;
    if (e->normalize) {
        e->normalize_fn(e->user, &e->tri[0]);
        e->normalize_fn(e->user, &e->tri[1]);
        e->normalize_fn(e->user, &e->tri[2]);
    }
    const EnVertex *t0 = &e->tri[0], *t1 = &e->tri[1], *t2 = &e->tri[2];
    uint32_t r = e->cursor;
    e->rec = r;
    float te0, te4, tec, te8, tf0;
    te0 = (float) (D(t1->sx) - D(t2->sx));
    te4 = (float) (D(t0->sy) - D(t2->sy));
    tec = (float) (D(t0->sx) - D(t2->sx));
    te8 = (float) (D(t1->sy) - D(t2->sy));
    tf0 = (float) (1.0 / ((D(te0) * D(te4)) - (D(tec) * D(te8))));
    wf(e, r + 0x64, (float)D(t0->w));
    wf(e, r + 0x2c, (float)((((D(t1->w) - D(t2->w)) * D(te4)) - ((D(t0->w) - D(t2->w)) * D(te8))) * D(tf0)));
    wf(e, r + 0x48, (float)((((D(t0->w) - D(t2->w)) * D(te0)) - ((D(t1->w) - D(t2->w)) * D(tec))) * D(tf0)));
    wf(e, r + 0x70, (float)(D(t0->sx) + (-16.0)));
    wf(e, r + 0x74, (float)((-16.0) + D(t0->sy)));
    if (f & 1) {
        float t5c = (float) ((D(t1->w) * D(t1->v)) - (D(t2->w) * D(t2->v)));
        float t94 = (float) (D(t0->u) * D(t0->w));
        float t88 = (float) (D(t0->w) * D(t0->v));
        wf(e, r + 0x4c, (float)D(t94));
        wf(e, r + 0x50, (float)D(t88));
        wf(e, r + 0x14, (float)(((((D(t1->u) * D(t1->w)) - (D(t2->u) * D(t2->w))) * D(te4)) - (((D(t0->u) * D(t0->w)) - (D(t2->u) * D(t2->w))) * D(te8))) * D(tf0)));
        wf(e, r + 0x18, (float)(((((D(t1->w) * D(t1->v)) - (D(t2->w) * D(t2->v))) * D(te4)) - (((D(t0->w) * D(t0->v)) - (D(t2->w) * D(t2->v))) * D(te8))) * D(tf0)));
        wf(e, r + 0x30, (float)(((((D(t0->u) * D(t0->w)) - (D(t2->u) * D(t2->w))) * D(te0)) - (((D(t1->u) * D(t1->w)) - (D(t2->u) * D(t2->w))) * D(tec))) * D(tf0)));
        wf(e, r + 0x34, (float)(D(tf0) * ((((D(t0->w) * D(t0->v)) - (D(t2->w) * D(t2->v))) * D(te0)) - (D(t5c) * D(tec)))));
    }
    if (f & 0x80) {
        float tcc = (float) (D(e->q[0]) * D(t0->w));
        wf(e, r + 0x60, (float)D(tcc));
        wf(e, r + 0x28, (float)(((((D(e->q[1]) * D(t1->w)) - (D(e->q[2]) * D(t2->w))) * D(te4)) - (((D(e->q[0]) * D(t0->w)) - (D(e->q[2]) * D(t2->w))) * D(te8))) * D(tf0)));
        wf(e, r + 0x44, (float)(((((D(e->q[0]) * D(t0->w)) - (D(e->q[2]) * D(t2->w))) * D(te0)) - (((D(e->q[1]) * D(t1->w)) - (D(e->q[2]) * D(t2->w))) * D(tec))) * D(tf0)));
    }
    if (f & 0x4000) {
        w16(e, r + 0xe, (uint16_t)e->light_index);
        float L[3];
        memcpy(L, e->lights + 0x78 * e->light_index + 0x50, sizeof L);
        float t70 = (float) ((D(t0->x) - D(L[0])) * D(t0->w));
        float tc4 = (float) ((D(t1->x) - D(L[0])) * D(t1->w));
        float t68 = (float) ((D(t2->x) - D(L[0])) * D(t2->w));
        float ta8 = (float) (D(tc4) - D(t68));
        float t60 = (float) ((D(t1->y) - D(L[1])) * D(t1->w));
        float tbc = (float) ((D(t2->y) - D(L[1])) * D(t2->w));
        float ta4 = (float) (D(t60) - D(tbc));
        float t9c = (float) ((D(t1->z) - D(L[2])) * D(t1->w));
        float tb8 = (float) ((D(t2->z) - D(L[2])) * D(t2->w));
        float ta0 = (float) (D(t9c) - D(tb8));
        float tc0 = (float) ((D(t0->y) - D(L[1])) * D(t0->w));
        float t74 = (float) (((D(t0->y) - D(L[1])) * D(t0->w)) - D(tbc));
        float td0 = (float) ((D(t0->z) - D(L[2])) * D(t0->w));
        wf(e, r + 0x1c, (float)(((D(ta8) * D(te4)) - ((D(t70) - D(t68)) * D(te8))) * D(tf0)));
        wf(e, r + 0x54, (float)D(t70));
        wf(e, r + 0x58, (float)D(tc0));
        wf(e, r + 0x5c, (float)D(td0));
        wf(e, r + 0x20, (float)((((D(t60) - D(tbc)) * D(te4)) - (D(t74) * D(te8))) * D(tf0)));
        wf(e, r + 0x24, (float)(((D(te4) * (D(t9c) - D(tb8))) - ((((D(t0->z) - D(L[2])) * D(t0->w)) - D(tb8)) * D(te8))) * D(tf0)));
        wf(e, r + 0x38, (float)((((D(t70) - D(t68)) * D(te0)) - (D(ta8) * D(tec))) * D(tf0)));
        wf(e, r + 0x3c, (float)((((((D(t0->y) - D(L[1])) * D(t0->w)) - D(tbc)) * D(te0)) - (D(ta4) * D(tec))) * D(tf0)));
        wf(e, r + 0x40, (float)((((((D(t0->z) - D(L[2])) * D(t0->w)) - D(tb8)) * D(te0)) - (D(ta0) * D(tec))) * D(tf0)));
    }
}
#undef D
