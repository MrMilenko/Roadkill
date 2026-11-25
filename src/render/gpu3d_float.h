/*
 * gpu3d_float.h: the x87 arithmetic of the 0xa0 path, reproduced in IEEE double.
 *
 * TM2.EXE runs the FPU with control word 0x127f (set once by the Watcom startup 0x004c15bd
 * from 0x004e94dc): precision control 53 bits, rounding to nearest even, every exception
 * masked. With 53 bit precision an x87 fadd / fsub / fmul / fdiv rounds its exact result to a
 * 53 bit significand, which is the IEEE double operation, except for the x87's wider exponent:
 * a result beyond the double range (|x| > 1.8e308 or < 2.2e-308) would stay normal in the x87
 * register. In the 0xa0 functions every operand is a float (|x| <= 3.4e38, >= 1.4e-45), an
 * integer below 2^31 or 1.0, and at most four multiplications deep ((g * r)^2 in 0x004afadc:
 * <= (3.4e38 * 3.4e38)^2 = 1.3e154, >= (1.4e-45)^4 = 3.8e-180), so no value ever leaves the
 * double range and every x87 result equals the double result bit for bit.
 * The port therefore:
 *   - keeps every intermediate the original keeps in an x87 register in a double, in the
 *     original's operation order, and rounds to float exactly where the original stores a
 *     dword (fst / fstp m32);
 *   - is compiled with -ffp-contract=off (no fused multiply add);
 *   - converts with fistp semantics: round to nearest even (the control word's rounding), and
 *     the "integer indefinite" 0x80000000 for NaN or a result outside int32;
 *   - follows the x87 NaN rules, which differ from ARM / SSE: an invalid operation (inf - inf,
 *     0 * inf, 0 / 0) gives the negative "real indefinite" NaN; with NaN operands the result is
 *     the operand NaN of larger significand, made quiet. This matters because 0x004b029c and
 *     0x004afadc compare the fog value's bits as a signed integer (a negative NaN is not
 *     clamped, a positive one is).
 *   - x87 compares (fcomp + fnstsw + sahf): "unordered" sets C0 = C2 = C3 = 1, so jae is not
 *     taken and jbe is; the C conditions are written to give the same result for NaN.
 */
#ifndef TM2_RENDER_GPU3D_FLOAT_H
#define TM2_RENDER_GPU3D_FLOAT_H

#include <stdint.h>
#include <string.h>

static inline uint64_t g3_dbits(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
static inline double g3_dfrom(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }
static inline uint32_t g3_fbits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static inline float g3_ffrom(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static inline int g3_isnan(double d) { return (g3_dbits(d) & 0x7fffffffffffffffull) > 0x7ff0000000000000ull; }

/* the x87 NaN result of an operation on a and b whose IEEE result r is NaN */
static inline double g3_nan(double a, double b, double r) {
    if (!g3_isnan(r)) return r;
    int na = g3_isnan(a), nb = g3_isnan(b);
    if (!na && !nb) return g3_dfrom(0xfff8000000000000ull);          /* real indefinite */
    uint64_t ua = g3_dbits(a), ub = g3_dbits(b);
    if (na && nb) {
        uint64_t ma = (ua & 0x000fffffffffffffull) | 0x0008000000000000ull;
        uint64_t mb = (ub & 0x000fffffffffffffull) | 0x0008000000000000ull;
        return g3_dfrom((mb > ma ? ub : ua) | 0x0008000000000000ull);
    }
    return g3_dfrom((na ? ua : ub) | 0x0008000000000000ull);
}

static inline double g3_add(double a, double b) { return g3_nan(a, b, a + b); }
static inline double g3_sub(double a, double b) { return g3_nan(a, b, a - b); }
static inline double g3_mul(double a, double b) { return g3_nan(a, b, a * b); }
static inline double g3_div(double a, double b) { return g3_nan(a, b, a / b); }

/* fld m32: a float widened (exact; a signalling NaN becomes quiet in both) */
static inline double g3_ld(float f) { return (double)f; }
/* fst m32 */
static inline float g3_st(double d) { return (float)d; }

/* fld m32 + fstp m32: the value unchanged but a signalling NaN made quiet (done on the bits:
 * a C compiler folds (float)(double)f to f) */
static inline float g3_fldst(float f) {
    uint32_t u = g3_fbits(f);
    if ((u & 0x7fffffffu) > 0x7f800000u) u |= 0x00400000u;
    return g3_ffrom(u);
}

/* fistp m32 in round to nearest even */
static inline int32_t g3_fistp(double d) {
    if (g3_isnan(d)) return INT32_MIN;
    /* round half to even without depending on the C library's rounding mode */
    double t = __builtin_rint(d);
    if (!(t >= -2147483648.0 && t <= 2147483647.0)) return INT32_MIN;
    return (int32_t)t;
}

#endif /* TM2_RENDER_GPU3D_FLOAT_H */
