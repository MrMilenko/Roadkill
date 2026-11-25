/*
 * en_clip.c: float vertex leaves of the polygon path, ported from TM2.EXE 0x0048c62c,
 * 0x0048c6b8, 0x0048c890, 0x0048f59c, 0x00491ee4 (see en_clip.h).
 *
 * Every double operation below is one x87 operation of the original (precision control 53
 * bits), in its order; casts to float are its dword stores. Must be built without floating
 * point contraction (no fused multiply add): the pragma below plus -ffp-contract=off.
 */
#pragma STDC FP_CONTRACT OFF
#include "en_clip.h"

#include <string.h>

/* the vertex as 13 floats, by field index (x 0 .. w 12), the way the interpolation mask counts */
#define F(vp) ((float*)&(vp)->x)

static uint32_t fbits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

/* x87 "fldz; fcomp x; fnstsw; sahf" flags: CF = 0 < x or unordered, ZF = 0 == x or unordered */
static int cf0(float x) { return !(x <= 0.0f); }   /* 0 < x, or NaN */
static int zf0(float x) { return x == 0.0f || x != x; }

static void xform(const EnXform* m, double x, double y, double z, float out[3]) {
    /* 0x0048c62c: each coordinate gets its translation, then out[i] = (x * m0 + y * m1) + z * m2 */
    x = x + (double)m->t[0];
    y = y + (double)m->t[1];
    z = z + (double)m->t[2];
    double a = x * (double)m->m[0], b = x * (double)m->m[3], c = x * (double)m->m[6];
    a = a + y * (double)m->m[1];
    b = b + y * (double)m->m[4];
    c = c + y * (double)m->m[7];
    a = a + z * (double)m->m[2];
    b = b + z * (double)m->m[5];
    c = c + z * (double)m->m[8];
    out[0] = (float)a;
    out[1] = (float)b;
    out[2] = (float)c;
}

void en_xform_short(const EnXform* x, const int16_t in[3], float out[3]) {
    xform(x, (double)in[0], (double)in[1], (double)in[2], out);
}

void en_xform_int(const EnXform* x, const int32_t in[3], float out[3]) {
    xform(x, (double)in[0], (double)in[1], (double)in[2], out);
}

void en_project(const EnProj* p, EnVertex* v) {
    if ((fbits(F(v)[2]) & 0x7fffffffu) == 0) F(v)[2] = 1.0f;
    F(v)[12] = (float)(1.0 / (double)F(v)[2]);
    double x = (double)F(v)[0] * (double)p->sx;
    double y = (double)F(v)[1] * (double)p->sy;
    x = x * (double)F(v)[12];
    y = y * (double)F(v)[12];
    F(v)[5] = (float)(x + (double)p->cx);
    F(v)[6] = (float)(y + (double)p->cy);
}

static float plane(EnVertex* v, int a, int b, int sign) {
    return sign == 1 ? (float)((double)F(v)[a] - (double)F(v)[b]) : (float)((double)F(v)[a] + (double)F(v)[b]);
}

static void push(EnClipBuf* l, EnVertex* v) {
    if (l->count >= 0 && l->count < EN_CLIP_MAX) l->v[l->count] = v;
    l->count++;
}

void en_clip_plane(EnClipEnv* env, const EnClipBuf* in, EnClipBuf* out, int a, int b, int sign) {
    out->count = 0;
    EnVertex* prev = in->v[in->count - 1];
    float dp = plane(prev, a, b, sign);
    for (int i = 0; i < in->count; i++) {
        EnVertex* cur = in->v[i];
        float d = plane(cur, a, b, sign);
        int cross, cur_out = 0;
        if (!cf0(dp) && !zf0(dp)) {            /* 0 > dp */
            cross = cf0(d);                   /* and 0 < d (or NaN) */
            if (!cross) goto second;
        } else {
        second:
            if (!cf0(dp)) cross = 0;          /* 0 >= dp */
            else { cross = !(cf0(d) || zf0(d)); cur_out = cross; }   /* 0 > d */
        }
        if (cross) {
            env->clipped = 1;
            EnVertex* nv = env->next;
            uint32_t mask = env->interp_mask;
            int k = 0;
            if (cur_out) {
                /* cur outside: from prev towards cur */
                float t = (float)((double)dp / ((double)dp - (double)d));
                for (; mask; mask = (uint32_t)((int32_t)mask >> 1), k++)
                    if (mask & 1)
                        F(nv)[k] = (float)(((double)F(cur)[k] - (double)F(prev)[k]) * (double)t + (double)F(prev)[k]);
            } else {
                float t = (float)((double)d / ((double)d - (double)dp));
                for (; mask; mask = (uint32_t)((int32_t)mask >> 1), k++)
                    if (mask & 1)
                        F(nv)[k] = (float)(((double)F(prev)[k] - (double)F(cur)[k]) * (double)t + (double)F(cur)[k]);
            }
            F(nv)[10] = 0.1f;
            memcpy(&F(nv)[11], &env->depth_scale, 4);
            push(out, nv);
            env->next = nv + 1;
        }
        if (cf0(d) || zf0(d)) push(out, cur);     /* not (0 > d) */
        if (in->count == 2) {
            if (!cf0(dp) && !zf0(dp)) return;        /* 0 > dp */
            push(out, in->v[i + 1]);
            return;
        }
        prev = cur;
        dp = d;
    }
}

int en_winding_check(const EnClipBuf* l, int split) {
    int cnt = 0;
    for (int i = 0; i < l->count; i++) {
        int j = i + 1, k = i + 2;
        if (j >= l->count) j -= l->count;
        if (k >= l->count) k -= l->count;
        EnVertex *p = l->v[i], *q = l->v[j], *r = l->v[k];
        double s = (double)F(p)[5] * (double)F(q)[6];
        s = s + (double)F(q)[5] * (double)F(r)[6];
        s = s + (double)F(r)[5] * (double)F(p)[6];
        s = s - (double)F(p)[5] * (double)F(r)[6];
        s = s - (double)F(q)[5] * (double)F(p)[6];
        s = s - (double)F(r)[5] * (double)F(q)[6];
        float area = (float)s;
        if (split) {
            if (!cf0(area)) cnt++;                         /* area <= 0 */
        } else if ((int32_t)fbits(area) <= 0x43fa0000) {   /* signed bit compare with 500.0f */
            cnt++;
        }
    }
    if (split) return cnt > l->count - 2 ? 0 : 1;
    return cnt > 0 ? 0 : 1;
}
