/* math.h for the Xbox build: the CRT's (Visual C++ 2003) plus the C99 functions Roadkill uses */
#ifndef RK_MATH_H
#define RK_MATH_H
#include_next <math.h>
#include <float.h>
#ifndef INFINITY
#define INFINITY ((float)(1e300 * 1e300))
#endif
#ifndef NAN
#define NAN (INFINITY - INFINITY)
#endif
double trunc(double x);
float truncf(float x);
double nearbyint(double x);   /* round half to even, as the x87 (xbox_libm.c) */
double rint(double x);
static __inline long lrint(double x) { return (long)nearbyint(x); }
static __inline double round(double x) { return x < 0 ? -floor(-x + 0.5) : floor(x + 0.5); }
static __inline float roundf(float x) { return (float)round(x); }
static __inline long lround(double x) { return (long)round(x); }
static __inline double fmin(double a, double b) { return a < b ? a : b; }
static __inline double fmax(double a, double b) { return a > b ? a : b; }
static __inline float fminf(float a, float b) { return a < b ? a : b; }
static __inline float fmaxf(float a, float b) { return a > b ? a : b; }
#ifndef isnan
#define isnan(x) _isnan(x)
#define isfinite(x) _finite(x)
#define isinf(x) (!_finite(x) && !_isnan(x))
#endif
#endif
