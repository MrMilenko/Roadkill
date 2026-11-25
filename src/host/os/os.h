/*
 * os.h: the few things the host needs from the operating system that differ between Windows,
 * macOS and Linux (port code, not from the original). Everything else the host uses is C99, SDL2
 * or POSIX calls that MinGW-w64 also provides (pthreads, dirent, open / read / close).
 */
#ifndef ROADKILL_OS_H
#define ROADKILL_OS_H

#include <fcntl.h>
#include <stddef.h>

#ifndef O_BINARY
#define O_BINARY 0   /* Windows' open() translates line ends unless told otherwise */
#endif

/* n bytes of zeroed, readable and writable memory (the game's address space); NULL on failure */
void* os_reserve(size_t n);
void os_release(void* p, size_t n);
/* the highest address of the calling thread's stack */
char* os_stack_top(void);
/* one directory; 0 when it was made or is already there */
int os_mkdir(const char* path);
int os_setenv(const char* name, const char* value);

#endif
