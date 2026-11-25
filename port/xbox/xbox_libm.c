/*
 * xbox_libm.c: C99 math the compiler may call by name (it lowers some operations to trunc / rint),
 * which Visual C++ 2003's library lacks. Port code; compat/math.h has the inline versions.
 */
double floor(double);
double ceil(double);
double fmod(double, double);

double trunc(double x) { return x < 0 ? ceil(x) : floor(x); }
float truncf(float x) { return (float)trunc(x); }
double rint(double x)
{
    double f = floor(x), d = x - f;
    if (d > 0.5 || (d == 0.5 && fmod(f, 2.0) != 0.0)) f += 1.0;
    return f;
}
double nearbyint(double x) { return rint(x); }
