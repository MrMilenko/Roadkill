/*
 * en9_polyset.c: the per primitive vertex setup, TM2.EXE 0x00491698 (see en9_polyset.h).
 * Verified under emulation: tools/emu/check_en9_polyset.sh.
 */
#include "en9_polyset.h"
#include "en5_float.h"

#define SLOTS 0x009f78b0u
#define TABLE 0x009f7de0u
#define G_N   0x009f7ddcu
#define G_ZMIN 0x009f7d2cu
#define G_ZMAX 0x009f7d1cu
#define G_DEPTH 0x009f7d58u
#define G_SCALE 0x009f7d14u
#define G_SIZE 0x009f7d00u
#define G_MODE 0x009f7d34u
#define G_FLAGS 0x00c846d8u
#define P_IN  0x004e828cu
#define P_OUT 0x004e8290u

static float rf(En2Env* e, uint32_t a) { float f; memcpy(&f, e->mem(e->user, a), 4); return f; }
static void wf(En2Env* e, uint32_t a, float f) { memcpy(e->mem(e->user, a), &f, 4); }
static double rd(En2Env* e, uint32_t a) { double d; memcpy(&d, e->mem(e->user, a), 8); return d; }
/* fistp at round to nearest of an integral value: out of range / NaN -> integer indefinite */
static uint32_t fistp(double d)
{
    if (!(d >= -2147483648.0 && d < 2147483648.0)) return 0x80000000u;
    return (uint32_t)(int32_t)d;
}
static uint32_t depth_of(uint32_t zbits)
{
    float z;
    memcpy(&z, &zbits, 4);
    return fistp(en5_float_round((double)z));
}
/* a vertex's u, v from the next uv pair */
static void uv_pair(En2Env* e, uint32_t v, uint32_t esi)
{
    wf(e, v + 0xc, (float)(double)(int32_t)(((uint32_t)en2_r16(e, esi) << 12) + 0x800));
    wf(e, v + 0x10, (float)(double)(int32_t)(((uint32_t)en2_r16(e, esi + 2) << 12) + 0x800));
}
static void colour(En2Env* e, uint32_t v, uint32_t c)
{
    wf(e, v + 0x1c, (float)(double)((c & 0xff0000) >> 16));
    wf(e, v + 0x20, (float)(double)((c & 0xff00) >> 8));
    wf(e, v + 0x24, (float)(double)(c & 0xff));
}
/* the view position scaled */
static void scale3(En2Env* e, uint32_t v, double s)
{
    wf(e, v, (float)(s * (double)rf(e, v)));
    wf(e, v + 4, (float)(s * (double)rf(e, v + 4)));
    wf(e, v + 8, (float)(s * (double)rf(e, v + 8)));
}
static void slot_init(En2Env* e, uint32_t k, uint32_t v)
{
    en2_w32(e, TABLE + 4 * k, v);
    en2_w32(e, v + 0x28, 0x3dcccccdu);   /* 0.1f */
}
/* after a line: depth = chop(zmax), zmax / zmin scaled by the doubles 0x004e2de8 (10) / 0x004e2df0 (0.1) */
static void line_depth(En2Env* e)
{
    double a = (double)rf(e, G_ZMAX), b = (double)rf(e, G_ZMIN);
    double ak = a * rd(e, 0x004e2de8u), bk = b * rd(e, 0x004e2df0u);
    en2_w32(e, G_DEPTH, fistp(en5_float_round(a)));
    wf(e, G_ZMAX, (float)ak);
    wf(e, G_ZMIN, (float)bk);
}

void en9_poly_setup(En2Env* e, uint32_t obj, uint32_t prim)
{
    const uint32_t F = e->scratch;
    int two = 0;
    en2_w32(e, 0x009f7d78u, en2_r32(e, obj + 0x8008));
    en2_w32(e, P_IN, G_N);
    en2_w32(e, 0x009f7d50u, (uint32_t)((int32_t)(en2_r32(e, prim + 0x38) << 19) >> 26));
    en2_w32(e, 0x009f7d54u, en2_r32(e, prim + 0x3c) & 0xffffff);
    en2_w32(e, G_FLAGS, 0x10);
    en2_w32(e, 0x009f7d80u, 0);
    en2_w32(e, 0x009f7d48u, 0);
    en2_w32(e, P_OUT, 0x009f7d84u);
    uint32_t col = prim + 0x3c;
    uint32_t esi = col + (((en2_r32(e, prim + 0x34) * 2) >> 29) << 2);   /* the uvs after the colours */
    en2_w32(e, 0x009f7d38u, en2_r8(e, col + 3) & 2);
    uint32_t uvp = esi + ((uint32_t)(en2_r8(e, prim) & 0x7f) << 2);       /* the texture word */
    en2_w32(e, G_MODE, en2_r8(e, prim + 1));
    uint32_t n = (en2_r32(e, prim + 0x34) << 13) >> 29;
    en2_w32(e, G_N, n);
    int32_t one = (int32_t)(1u << (en2_r8(e, 0x004e82c4u) & 31));
    en2_w32(e, F + 0x4c, (uint32_t)one);
    wf(e, G_SCALE, (float)(1.0 / (double)one));

    if ((int32_t)n < 2) {
        float k = rf(e, 0x004e82b4u);
        for (uint32_t j = 0; j < 4; j++) {
            uint32_t v = SLOTS + 0x34 * j;
            slot_init(e, j, v);
            wf(e, en2_r32(e, TABLE + 4 * j) + 0x2c, k);
            uv_pair(e, en2_r32(e, TABLE + 4 * j), esi);
            en2_w32(e, F + 0x4c, ((uint32_t)en2_r16(e, esi + 2) << 12) + 0x800);
            esi += 4;
        }
        if (en2_r32(e, G_N) == 1) {   /* a line */
            if (en2_r32(e, prim + 0x38) & 1) {
                wf(e, G_SIZE, (float)(double)(int32_t)en2_r32(e, prim + 0x1c));
                en2_call2(e, 0x0048c6b8u, prim + 4, 0x009f7980u);
                en2_call2(e, 0x0048c6b8u, prim + 0x10, 0x009f79b4u);
            } else {
                uint32_t x = en2_r32(e, prim + 4), y = en2_r32(e, prim + 8), z = en2_r32(e, prim + 0xc);
                uint32_t h = en2_r32(e, prim + 0x14);
                en2_w32(e, F + 0, x);
                en2_w32(e, F + 0xc, x);
                en2_w32(e, F + 4, y);
                en2_w32(e, F + 0x10, y);
                en2_w32(e, F + 8, z + h);
                en2_w32(e, F + 0x14, z - h);
                wf(e, G_SIZE, (float)(double)(int32_t)en2_r32(e, prim + 0x10));
                en2_call2(e, 0x0048c6b8u, F, 0x009f7980u);
                en2_call2(e, 0x0048c6b8u, F + 0xc, 0x009f79b4u);
            }
            if (en2_call0(e, 0x004908b0u) == 0) goto end;
            line_depth(e);
        } else {   /* a sprite */
            en2_call2(e, 0x0048c6b8u, prim + 4, en2_r32(e, TABLE));
            double a = (double)(int32_t)en2_r32(e, prim + 0x10) * (double)rf(e, 0x009f7ce0u);
            double b = (double)(int32_t)en2_r32(e, prim + 0x14) * (double)rf(e, 0x009f7ce8u);
            double sc = (double)rf(e, G_SCALE);
            a = a * sc;
            b = b * sc;
            wf(e, G_SIZE, (float)a);
            wf(e, 0x009f7d30u, (float)b);
            if (en2_call0(e, 0x00490780u) == 0) goto end;
            en2_w32(e, G_DEPTH, depth_of(en2_r32(e, G_ZMAX)));
        }
    } else {
        uint32_t vin = prim + 4, rgb = col;
        if (n >= 3) {   /* a polygon: depth range, uvs when textured */
            en2_w32(e, G_ZMAX, 0);
            en2_w32(e, F + 0x44, 0);
            en2_w32(e, G_ZMIN, 0x46fffe00u);   /* 32767.0f */
        } else {        /* two vertices */
            two = 1;
            en2_w32(e, F + 0x2c, 1);
            en2_w32(e, G_ZMAX, 0);
        }
        for (uint32_t j = 0; j < en2_r32(e, G_N); j++) {
            uint32_t v = SLOTS + 0x34 * j;
            slot_init(e, j, v);
            en2_w32(e, en2_r32(e, TABLE + 4 * j) + 0x2c, en2_r32(e, 0x004e82b4u));
            en2_call2(e, 0x0048c6b8u, vin, en2_r32(e, TABLE + 4 * j));
            v = en2_r32(e, TABLE + 4 * j);
            double sc = (double)rf(e, G_SCALE);
            scale3(e, v, sc);
            float z = rf(e, v + 8);
            if (!two) {
                if (rf(e, G_ZMIN) >= z) en2_w32(e, G_ZMIN, en2_r32(e, v + 8));   /* jae: ordered >= */
                if (!(rf(e, G_ZMAX) >= z)) en2_w32(e, G_ZMAX, en2_r32(e, v + 8)); /* jb: < or unordered */
                if (en2_r8(e, prim) & 0x7f) {
                    uv_pair(e, v, esi);
                    en2_w32(e, F + 0x4c, ((uint32_t)en2_r16(e, esi + 2) << 12) + 0x800);
                    esi += 4;
                }
                en2_w32(e, G_DEPTH, depth_of(en2_r32(e, G_ZMAX)));
            } else {
                uint32_t m = !(rf(e, G_ZMAX) >= z) ? en2_r32(e, v + 8) : en2_r32(e, G_ZMAX);
                en2_w32(e, F + 0x48, m);
                en2_w32(e, G_ZMAX, m);
                en2_w32(e, G_DEPTH, depth_of(m));
            }
            if (en2_r32(e, G_MODE) > 1) {
                uint32_t c = en2_r32(e, rgb);
                colour(e, v, c);
                en2_w32(e, F + (two ? 0x20 : 0x18), c & 0xff);
                en2_w32(e, F + (two ? 0x24 : 0x1c), 0);
                rgb += 4;
            }
            vin += 12;
            if (!two) en2_w32(e, F + 0x44, j + 1);
            else en2_w32(e, F + 0x40, SLOTS + 0x34 * (j + 1));
        }
    }
    /* the tail: forced depth, render mode, clipping, projection, submit */
    if (en2_r32(e, prim + 0x38) & 0x40) {
        en2_w32(e, 0x009f7d48u, 1);
        en2_w32(e, G_DEPTH, (uint32_t)((int32_t)(en2_r32(e, prim + 0x38) << 19) >> 26));
    }
    if ((int32_t)en2_r32(e, G_DEPTH) <= 0) goto end;
    if ((en2_call0(e, 0x004958acu) & 0xff) == 1)
        en2_w32(e, 0x009f7d44u, en2_r32(e, G_MODE) > 1 ? 0x39f : 0x1f);
    else
        en2_w32(e, 0x009f7d44u, 7);
    if (en2_r8(e, prim) & 0x7f) {
        uint16_t page = en2_r16(e, esi) & 0x1ff;
        en2_w16(e, 0x009f7e32u, page);
        int32_t p = (int16_t)en2_r16(e, 0x009f7e32u);
        if (en2_r32(e, 0x00a30c58u + (uint32_t)p * 48) != 0) en2_w8(e, G_FLAGS, en2_r8(e, G_FLAGS) | 8);
        en2_w8(e, G_FLAGS, en2_r8(e, G_FLAGS) | 1);
        en2_w32(e, 0x009f7d7cu, (uint32_t)(en2_r16(e, uvp) & 0x3000) >> 12);
    } else {
        en2_w32(e, 0x009f7d7cu, 3);
    }
    en2_w8(e, G_FLAGS, en2_r8(e, G_FLAGS) | 0x40);
    en2_w32(e, 0x009f7dd8u, SLOTS + 0x34 * en2_r32(e, G_N));
    {
        static const struct { uint8_t swap, b; int8_t sign; } pl[5] = {{0, 0xa, 1}, {1, 0, 1}, {0, 0, -1}, {1, 1, 1}, {0, 1, -1}};
        static const uint8_t check_n[5] = {1, 0, 1, 0, 1};   /* after each plane: 0x009f7d84 or 0x009f7ddc */
        for (int k = 0; k < 5; k++) {
            uint32_t a = en2_r32(e, pl[k].swap ? P_OUT : P_IN), b = en2_r32(e, pl[k].swap ? P_IN : P_OUT);
            en2_call5(e, 0x0048f59cu, a, b, 2, pl[k].b, (uint32_t)(int32_t)pl[k].sign);
            if ((int32_t)en2_r32(e, check_n[k] ? 0x009f7d84u : G_N) < 1) goto end;
        }
    }
    if (two) en2_w32(e, en2_r32(e, P_OUT), 2);
    for (uint32_t i = 0; (int32_t)i < (int32_t)en2_r32(e, en2_r32(e, P_OUT)); i++)
        en2_call1(e, 0x0048c890u, en2_r32(e, en2_r32(e, P_OUT) + 4 + 4 * i));
    if ((en2_call0(e, 0x004958acu) & 0xff) == 0) {
        if ((en2_r8(e, G_FLAGS) & 0x10) && en2_r32(e, en2_r32(e, P_OUT)) != 2) en2_call0(e, 0x0048f838u);
        en2_call1(e, 0x0048fd54u, obj);
    } else {
        en2_call1(e, 0x00491018u, obj);
    }
end:
    en2_w32(e, obj + 0x8008, en2_r32(e, 0x009f7d78u));
}
