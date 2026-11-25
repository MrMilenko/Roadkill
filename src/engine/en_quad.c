/*
 * en_quad.c: polygon renderer float leaves, ported from TM2.EXE 0x00490780, 0x004908b0,
 * 0x00490e74, 0x00492914 (see en_quad.h). Each double below is one x87 register value of the
 * original (53 bit precision), each (float) store one `fstp dword`, in the original's order.
 */
#include "en_quad.h"

#include "../math/tm2math.h"

#include <math.h>
#include <string.h>

/* a * b + c must stay two roundings, as on the x87 (no fused multiply add) */
#pragma STDC FP_CONTRACT OFF

static int32_t fbits(float f) { int32_t i; memcpy(&i, &f, 4); return i; }

/* fistp dword with round to nearest; out of range / NaN gives the integer indefinite */
static int32_t fist(double x) {
    double r = nearbyint(x);
    if (!(r >= -2147483648.0 && r <= 2147483647.0)) return INT32_MIN;
    return (int32_t)r;
}

int en_billboard_quad(EnQuadEnv* e) {
    EnVertex* v0 = e->ptr[0];
    float z = (float)((double)e->tex_inv * v0->z);
    v0->z = z;
    e->zmax = z;
    if (!(fbits(e->zmax) > 0x3f800000)) return 0;
    double t = e->tex_inv;
    double tx = t * v0->x, ty = t * v0->y;
    double w = e->size_x;
    double w2 = w * 2.0;
    double h2 = 2.0 * e->size_y;
    double w_ = e->size_x, h_ = e->size_y;
    v0->x = (float)tx;
    v0->y = (float)ty;
    double x = (double)v0->x - w_;
    double y = (double)v0->y - h_;
    e->size_x = (float)w2;
    v0->x = (float)x;
    double x1 = x + e->size_x;
    v0->y = (float)y;
    EnVertex* v1 = e->ptr[1];
    v1->x = (float)x1;
    v1->y = v0->y;
    v1->z = v0->z;
    EnVertex* v2 = e->ptr[2];
    double x2 = (double)v0->x + e->size_x;
    e->size_y = (float)h2;
    v2->x = (float)x2;
    double y2 = (double)v0->y + e->size_y;
    float zm = e->zmax;
    v2->y = (float)y2;
    v2->z = v0->z;
    EnVertex* v3 = e->ptr[3];
    v3->x = v0->x;
    double y3 = (double)v0->y + e->size_y;
    v3->y = (float)y3;
    v3->z = v0->z;
    e->zmin = zm;
    e->vert_count = 4;
    e->render_flags = (uint8_t)((e->render_flags | 0x20) & 0xef);
    return 1;
}

int en_line_segment_quad(EnQuadEnv* e) {
    EnVertex* a = &e->vtx[4];   /* 0x009f7980 */
    EnVertex* b = &e->vtx[5];   /* 0x009f79b4 */
    EnVertex* d = &e->vtx[6];   /* 0x009f79e8 */
    EnVertex* n = &e->vtx[7];   /* 0x009f7a1c */
    float far;
    if ((double)a->z > (double)b->z) { far = a->z; e->zmin = b->z; }   /* jbe: not taken */
    else { far = b->z; e->zmin = a->z; }                               /* incl. unordered */
    e->zmax = far;
    e->zmax = (float)((double)e->tex_inv * e->zmax);
    if (!(fbits(e->zmax) > 0x3f800000)) return 0;

    double az = a->z, bz = b->z;
    double t8 = bz - az;
    double ay = a->y;
    d->z = (float)t8;
    double t10 = ay * d->z;
    double by = b->y, ax = a->x, bx = b->x;
    double t14 = bx - ax;
    double t15 = by - ay;
    d->x = (float)t14;
    double t16 = az * d->x;
    d->y = (float)t15;
    double t17 = ax * d->y;
    double t18 = az * d->y;
    double t19 = ax * d->z;
    double t20 = ay * d->x;
    double t21 = t10 - t18;
    double t22 = t16 - t19;
    n->x = (float)t21;
    double t23 = t21 * n->x;
    n->y = (float)t22;
    double t24 = t22 * n->y;
    double t25 = t17 - t20;
    double t26 = t24 + t23;
    n->z = (float)t25;
    double t27 = t25 * n->z;
    double len = sqrt(t26 + t27);
    double zn = (double)e->tex_inv * e->zmin;
    float lenf = (float)len;
    e->zmin = (float)zn;
    if ((fbits(lenf) & 0x7fffffff) == 0) return 0;

    double t = e->tex_inv;
    double s = (double)e->size_x * e->fov_x / lenf;
    double t36 = t * a->x, t38 = t * a->y, t40 = t * a->z, t42 = t * b->x;
    float sf = (float)(s * e->tex_inv);
    double t45 = (double)n->x * sf, t47 = (double)n->y * sf, t49 = (double)n->z * sf;
    a->x = (float)t36;
    double t51 = t * b->y;
    a->y = (float)t38;
    double t53 = t * b->z;
    a->z = (float)t40;
    b->x = (float)t42;
    b->y = (float)t51;
    b->z = (float)t53;
    e->vert_count = 4;
    n->x = (float)t45;
    n->y = (float)t47;
    n->z = (float)t49;
    EnVertex* q = e->vtx;
    q[0].x = (float)((double)a->x + n->x);
    q[0].y = (float)((double)a->y + n->y);
    q[0].z = (float)((double)a->z + n->z);
    q[1].x = (float)((double)a->x - n->x);
    q[1].y = (float)((double)a->y - n->y);
    e->render_flags = (uint8_t)(e->render_flags | 0x20);
    q[1].z = (float)((double)a->z - n->z);
    q[2].x = (float)((double)b->x - n->x);
    q[2].y = (float)((double)b->y - n->y);
    q[2].z = (float)((double)b->z - n->z);
    q[3].x = (float)((double)b->x + n->x);
    q[3].y = (float)((double)b->y + n->y);
    q[3].z = (float)((double)b->z + n->z);
    return 1;
}

void en_water_uv_animate(EnQuadEnv* e) {
    for (int32_t i = 0; i < e->vert_count; i++) {
        const uint8_t* p = e->poly + 4 + 2 * i;
        int32_t idx = (int16_t)(p[0] | p[1] << 8);
        const uint8_t* vp = e->verts + (intptr_t)idx * 8;
        int32_t x = (int16_t)(vp[0] | vp[1] << 8);
        int32_t y = (int16_t)(vp[2] | vp[3] << 8);
        int32_t s1 = tm2_sin((int32_t)(((uint32_t)x * 17u + (uint32_t)e->wave_a) & 0xfff));
        int32_t s2 = tm2_sin((int32_t)(((uint32_t)y * 9u + (uint32_t)e->wave_b) & 0xfff));
        int32_t s3 = tm2_sin((int32_t)(((uint32_t)x * 7u + (uint32_t)e->wave_b) & 0xfff));
        int32_t s4 = tm2_sin((int32_t)(((uint32_t)y * 23u + (uint32_t)e->wave_a) & 0xfff));
        float s3f = (float)s3, s4f = (float)s4;
        EnVertex* v = e->ptr[i];
        double t5 = ((double)s1 * 2.6) * 0.5;
        double t10 = 0.5 * ((double)s2 * 4.3);
        v->u = (float)(t5 + v->u);
        v->u = (float)(t10 + v->u);
        double t17 = ((double)s3f * 3.0) * 0.5;
        double t22 = 0.5 * ((double)s4f * 3.8);
        v->v = (float)(t17 + v->v);
        v->v = (float)(t22 + v->v);
    }
}

uint32_t en_color_interpolate(float z, float z0, float z1, uint32_t c0, uint32_t c1) {
    float tf = (float)(((double)z - z0) / ((double)z1 - z0));
    double r0 = (double)(c0 >> 16 & 0xff), r1 = (double)(c1 >> 16 & 0xff);
    float g0 = (float)(c0 >> 8 & 0xff), b0 = (float)(c0 & 0xff);
    float g1 = (float)(c1 >> 8 & 0xff), b1 = (float)(c1 & 0xff);
    int32_t r = fist(r0 + (r1 - r0) * tf);
    int32_t g = fist(((double)g1 - g0) * tf + g0);
    int32_t b = fist(((double)b1 - b0) * tf + b0);
    return (uint32_t)(r & 0xff) << 16 | (uint32_t)(g & 0xff) << 8 | (uint32_t)(b & 0xff);
}
