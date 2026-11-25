/*
 * en_fog.c: polygon light terms, ported from TM2.EXE 0x0049024c (see en_fog.h).
 */
#include "en_fog.h"

#include "../math/tm2math.h"

#include <string.h>

/* a * b + c must stay two roundings, as on the x87 */
#pragma STDC FP_CONTRACT OFF

static float rf(const uint8_t* p) { float f; memcpy(&f, p, 4); return f; }
static int32_t ri32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static int32_t fbits(float f) { int32_t i; memcpy(&i, &f, 4); return i; }
static int32_t i16at(const uint8_t* p) { return (int16_t)(p[0] | p[1] << 8); }

/* shimmer term at one vertex: sin(x * 17 + a) + sin(y * 9 + b), clamped to 8191 */
static void shimmer(EnFogEnv* e, int k, uint32_t a, uint32_t b, double intensity, float* q) {
    const uint8_t* v = e->verts + (intptr_t)i16at(e->poly + 4 + 2 * k) * 8;
    int32_t s1 = tm2_sin((int32_t)(((uint32_t)i16at(v) * 17u + a) & 0xfff));
    int32_t s2 = tm2_sin((int32_t)(((uint32_t)i16at(v + 2) * 9u + b) & 0xfff));
    float s = (float)s1;
    s = (float)((double)s2 + s);
    if (fbits(s) > 0x45fff800) s = 8191.0f;
    if (!(0.0 >= (double)s))   /* fcomp 0 with s; jae skips */
        *q = (float)((double)s * (double)0.0001220703125f * intensity + *q);
}

void en_light_terms(EnFogEnv* e) {
    float best = 1e6f;
    e->q[2] = 0.0f; e->q[1] = 0.0f; e->q[0] = 0.0f;
    for (int i = 0; i < 16; i++) {
        const uint8_t* L = e->lights + 0x78 * i;
        if (ri32(L + 4) == 0 || ri32(L + 8) == 0) continue;
        if (L[0] == 0 && ri32(L + 0xc) != 0) {
            for (int j = 0; j < e->clip->count; j++) {
                const EnVertex* v = e->clip->v[j];
                double dy = (double)v->y - rf(L + 0x54);
                double dx = (double)v->x - rf(L + 0x50);
                float fy = (float)dy;
                double t5 = dy * fy;
                float fx = (float)dx;
                double t6 = dx * fx;
                double dz = (double)v->z - rf(L + 0x58);
                double t9 = t5 + t6;
                float fz = (float)dz;
                double d = t9 + dz * fz;
                if (!(d >= (double)best)) {   /* jae skips; unordered updates */
                    best = (float)d;
                    e->render_flags |= 0x4000;
                    e->light_index = i;
                }
            }
        } else if (L[0] == 1) {
            uint32_t a = (uint32_t)e->wave_a, b = (uint32_t)e->wave_b;
            double in = rf(L + 0x6c);
            shimmer(e, 0, a * 2u, b, in, &e->q[0]);
            shimmer(e, 1, a, b * 2u, in, &e->q[1]);
            shimmer(e, 2, a, b, in, &e->q[2]);
        }
        /* kind 2: unreachable in the original (see en_fog.h) */
    }
    if ((fbits(e->q[0]) & 0x7fffffff) || (fbits(e->q[1]) & 0x7fffffff) || (fbits(e->q[2]) & 0x7fffffff))
        e->render_flags |= 0x80;
}
