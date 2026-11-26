/* time.h for the Xbox build: the CRT's plus struct timespec and nanosleep (xbox_posix.c) */
#ifndef RK_TIME_H
#define RK_TIME_H
#include_next <time.h>
#ifndef RK_TIMESPEC
#define RK_TIMESPEC
struct timespec { long tv_sec; long tv_nsec; };
#endif
int nanosleep(const struct timespec* t, struct timespec* left);
#endif
