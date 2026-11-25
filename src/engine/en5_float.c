/*
 * en5_float.c: see en5_float.h.
 */
#include "en5_float.h"
#include <math.h>

#define EN5_QueryPerformanceCounter 0x00c902f0u
#define EN5_timeGetTime             0x00c90640u

/* 0x004979a2: frndint with the rounding control set to chop (the control word's high byte
 * becomes 0x1f: 64 bit precision, truncation), the caller's control word restored: trunc. */
double en5_float_round(double x) { return trunc(x); }

static float rdf(En2Env* e, uint32_t a) { float f; memcpy(&f, e->mem(e->user, a), 4); return f; }
static double rdd(En2Env* e, uint32_t a) { double d; memcpy(&d, e->mem(e->user, a), 8); return d; }

/* fistp of an integral value: the x87 "integer indefinite" for NaN and values out of range */
static uint32_t fist32(double v)
{
    if (!(v >= -2147483648.0 && v <= 2147483647.0)) return 0x80000000u;
    return (uint32_t)(int32_t)v;
}
static uint64_t fist64(double v)
{
    if (!(v >= -9223372036854775808.0 && v < 9223372036854775808.0)) return 0x8000000000000000ull;
    return (uint64_t)(int64_t)v;
}

/* 0x004977cc: qsort comparator of the texture records (0x18 bytes: +8 count, +0x0c / +0x10 two
 * floats): records with count 0 sort last (0 when both are); else trunc(a.w * a.h - b.w * b.h)
 * (floats multiplied and subtracted in double), and on a tie b.count - a.count. */
uint32_t en5_texture_compare(En2Env* e, uint32_t a, uint32_t b)
{
    uint32_t ca = en2_r32(e, a + 8);
    if (ca == 0) return en2_r32(e, b + 8) == 0 ? 0 : 1;
    uint32_t cb = en2_r32(e, b + 8);
    if (cb == 0) return 0xffffffffu;
    double pa = (double)rdf(e, a + 0x0c) * (double)rdf(e, a + 0x10);
    double pb = (double)rdf(e, b + 0x0c) * (double)rdf(e, b + 0x10);
    uint32_t r = fist32(en5_float_round(pa - pb));
    if (r == 0) r = cb - ca;
    return r;
}

/* the clock: QueryPerformanceCounter (its BOOL answer stored back into the "available" flag
 * 0x00a03668) -> (high * 2^32 (the EXE's double 0x004e2e58 / 0x004e2e68) + low) * the tick scale
 * 0x00a03660 (double), truncated, low 32 bits of the 64 bit integer; without the counter
 * timeGetTime() * 1000. */
static uint32_t clock_us(En2Env* e, uint32_t k)
{
    if (en2_r32(e, 0x00a03668u) == 0) return en2_call0(e, EN5_timeGetTime) * 1000u;
    uint32_t li = e->scratch;
    uint32_t ok = en2_call1(e, EN5_QueryPerformanceCounter, li);
    double v = (double)(int32_t)en2_r32(e, li + 4) * rdd(e, k);
    en2_w32(e, 0x00a03668u, ok);
    v = v + (double)en2_r32(e, li);
    v = v * rdd(e, 0x00a03660u);
    return (uint32_t)fist64(en5_float_round(v));
}

uint32_t en5_clock_us(En2Env* e) { return clock_us(e, 0x004e2e58u); }

/* 0x004931fc: the same with 0x004e2e68, stored in 0x00a03654 (the clock mark). */
uint32_t en5_clock_us_mark(En2Env* e)
{
    uint32_t t = clock_us(e, 0x004e2e68u);
    en2_w32(e, 0x00a03654u, t);
    return t;
}
