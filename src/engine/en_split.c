/*
 * en_split.c: split screen primitive build, ported from TM2.EXE 0x00491018 and 0x004966cc
 * (see en_split.h). Double operations are the original's x87 operations (control word 0x127f,
 * 53 bit precision) in its order; build with -ffp-contract=off.
 */
#pragma STDC FP_CONTRACT OFF
#include "en_split.h"

#include "en_submit.h"

#include <math.h>
#include <string.h>

/* fistp dword under round to nearest; out of range and NaN give the integer indefinite */
static int32_t fistp32(float f) {
    double d = f;
    if (!(d > -2147483648.5 && d < 2147483647.5)) return INT32_MIN;
    return (int32_t)nearbyint(d);
}

/* 0x004979a2 (frndint with chop) then fistp qword: low 32 bits */
static uint32_t chop_lo(float f) {
    double t = trunc((double)f);
    if (!(t >= -9223372036854775808.0 && t < 9223372036854775808.0)) return 0;   /* indefinite 0x8000000000000000 */
    return (uint32_t)(uint64_t)(int64_t)t;
}

static uint8_t* at(EnSplitEnv* e, uint32_t a) { return e->prim + (a - e->prim_addr); }
static void w8(EnSplitEnv* e, uint32_t a, uint8_t v) { *at(e, a) = v; }
static void w16(EnSplitEnv* e, uint32_t a, uint16_t v) { memcpy(at(e, a), &v, 2); }
static void w32(EnSplitEnv* e, uint32_t a, uint32_t v) { memcpy(at(e, a), &v, 4); }
static void wf(EnSplitEnv* e, uint32_t a, float v) { memcpy(at(e, a), &v, 4); }
static uint32_t fbits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

static uint32_t vcolor(const EnVertex* v, uint32_t alpha) {
    uint32_t r = chop_lo(v->r), g = chop_lo(v->g), b = chop_lo(v->b);
    return b | (g << 8 | (r << 16 | alpha));
}

static void insert(EnSplitEnv* e, uint32_t rec) {
    EnSubmitEnv s;
    memset(&s, 0, sizeof s);
    s.depth_fixed = e->depth_fixed;
    s.depth_bias = e->depth_bias;
    s.render_flags = e->render_flags;
    int32_t bucket = en_depth_bucket(&s, e->depth);
    e->ot_insert(e->user, e->ot_base + 4u * (uint32_t)bucket, rec);
}

static uint32_t tex32(const EnSplitEnv* e, int32_t page, uint32_t off) {
    if (page < 0 || page >= e->tex_count || !e->tex_table) return 0;
    uint32_t v;
    memcpy(&v, e->tex_table + 0x30 * (uint32_t)page + off, 4);
    return v;
}

void en_split_prim(EnSplitEnv* e) {
    const EnClipBuf* c = e->clip;
    if (e->shade > 1) e->render_flags |= 0x1000;
    uint32_t r = e->cursor;
    if (c->count == 2) {
        const EnVertex *a = c->v[0], *b = c->v[1];
        if (e->shade <= 1) {
            w32(e, r + 4, e->color);
            e->rec = r;
            w8(e, r + 7, 0x40);
            w16(e, r + 0x8, (uint16_t)fistp32(a->sx));
            w16(e, r + 0xa, (uint16_t)fistp32(a->sy));
            w16(e, r + 0xc, (uint16_t)fistp32(b->sx));
            w16(e, r + 0xe, (uint16_t)fistp32(b->sy));
            insert(e, r);
            e->cursor = r + 0x10;
            return;
        }
        e->rec = r;
        w32(e, r + 4, vcolor(a, 0xff000000u));
        w8(e, r + 7, 0x50);
        w32(e, r + 0xc, vcolor(b, 0xff000000u));
        w16(e, r + 0x8, (uint16_t)fistp32(a->sx));
        w16(e, r + 0xa, (uint16_t)fistp32(a->sy));
        w16(e, r + 0x10, (uint16_t)fistp32(b->sx));
        w16(e, r + 0x12, (uint16_t)fistp32(b->sy));
        insert(e, r);
        e->cursor = r + 0x14;
        return;
    }

    int tex = e->render_flags & 1;
    int32_t page = (int32_t)e->page_ref >> 16;
    uint32_t wrap = tex ? tex32(e, page, 0x10) : 0;
    if (tex && e->shade > 0) e->color = e->semi ? 0xc0ffffffu : 0xffffffffu;
    e->rec = r;
    w8(e, r + 7, 0x80);
    if (tex) w32(e, r + 8, (uint32_t)page);
    w32(e, r + 0xc, e->blend);
    w8(e, r + 6, (uint8_t)c->count);
    w16(e, r + 4, e->render_flags);
    if (e->semi) *at(e, r + 7) |= 2;

    float su = 0, sv = 0, ou = 0, ov = 0;
    if (tex) {
        double w = (double)tex32(e, page, 4), h = (double)tex32(e, page, 8);
        if (wrap && !tex32(e, page, 0x2c)) {
            /* half texel inset */
            double wc = w + -1.0, iw = 1.0 / w;
            double hc = -1.0 + h, ih = 1.0 / h;
            double a = wc * iw;
            double s0 = a / (w * 4096.0);
            double bb = hc * ih;
            double s1 = bb / (h * 4096.0);
            ou = (float)(iw * 0.5);
            ov = (float)(ih * 0.5);
            su = (float)s0;
            sv = (float)s1;
        } else {
            su = (float)(1.0 / (w * 4096.0));
            sv = (float)(1.0 / (h * 4096.0));
        }
    }

    uint32_t p = r + 0x14;
    for (int32_t i = 0; i < c->count; i++, p += 0x20) {
        EnVertex* v = c->v[i];
        if (!(v->sx >= 0.0f) && v->sx == v->sx) v->sx = 0.0f;   /* 0 > sx (ordered) */
        if (!(v->sy >= 0.0f) && v->sy == v->sy) v->sy = 0.0f;
        w32(e, p, fbits(v->sx));
        w32(e, p + 4, fbits(v->sy));
        wf(e, p + 8, v->z);
        if (tex) {
            if (wrap) {
                wf(e, p + 0x18, (float)((double)v->u * (double)su + (double)ou));
                wf(e, p + 0x1c, (float)((double)v->v * (double)sv + (double)ov));
            } else {
                wf(e, p + 0x18, (float)((double)v->u * (double)su));
                wf(e, p + 0x1c, (float)((double)v->v * (double)sv));
            }
        }
        w32(e, p + 0xc, fbits(v->w));
        uint32_t col;
        if (e->render_flags & 2) col = 0x3c000000u;
        else if (e->shade <= 1) col = e->color;
        else col = vcolor(v, e->semi ? 0x80000000u : 0xff000000u);
        w32(e, p + 0x10, col);
        w32(e, p + 0x14, 0);
    }
    e->end = p;
    insert(e, r);
    e->cursor = e->end;
}

void en_ot_link(uint32_t* slot, uint32_t* rec, uint32_t rec_addr) {
    *rec = *slot;
    *slot = rec_addr;
}

typedef struct { EnSubmitEnv* s; } Tramp;
static void tramp(void* user, uint32_t slot, uint32_t rec) {
    EnSubmitEnv* s = ((Tramp*)user)->s;
    s->ot_insert(s->user, (int32_t)(slot >> 2), rec);
}

void en_split_build(EnSubmitEnv* s, EnSplitExtra* x) {
    Tramp t = { s };
    EnSplitEnv e;
    memset(&e, 0, sizeof e);
    e.clip = s->clip;
    e.shade = x->shade; e.color = s->color; e.semi = x->semi; e.blend = x->blend;
    e.render_flags = s->render_flags; e.page_ref = s->page_ref;
    e.depth = s->depth; e.depth_fixed = s->depth_fixed; e.depth_bias = s->depth_bias;
    e.tex_table = x->tex_table; e.tex_count = x->tex_count;
    e.prim = s->prim; e.prim_addr = s->prim_addr; e.cursor = s->cursor; e.end = x->end; e.rec = s->rec;
    e.ot_insert = tramp; e.ot_base = 0; e.user = &t;
    en_split_prim(&e);
    s->color = e.color; s->render_flags = e.render_flags; s->cursor = e.cursor; s->rec = e.rec;
    x->end = e.end;
}
