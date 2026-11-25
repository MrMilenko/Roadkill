/* errno.h for the Xbox build: the CRT's plus the POSIX codes Roadkill compares with */
#ifndef RK_ERRNO_H
#define RK_ERRNO_H
#include_next <errno.h>
#ifndef ETIMEDOUT
#define ETIMEDOUT 138
#endif
#endif
