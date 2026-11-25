/* unistd.h for the Xbox build: the calls Roadkill makes (xbox_posix.c) */
#ifndef RK_UNISTD_H
#define RK_UNISTD_H
#include <io.h>
#include <stddef.h>
int usleep(unsigned int usec);
int rk_open(const char* path, int flags, ...);
#define open rk_open
#define read _read
#define write _write
#define close _close
#define lseek _lseek
#define unlink _unlink
typedef long ssize_t;
#endif
