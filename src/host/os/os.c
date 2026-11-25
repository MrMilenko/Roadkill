/*
 * os.c: os.h for Windows (MinGW-w64) and POSIX (macOS, Linux). Port code.
 */
#include "os.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>

void* os_reserve(size_t n) { return VirtualAlloc(NULL, n, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); }
void os_release(void* p, size_t n) { (void)n; VirtualFree(p, 0, MEM_RELEASE); }

char* os_stack_top(void)
{
    ULONG_PTR lo, hi;
    GetCurrentThreadStackLimits(&lo, &hi);
    return (char*)hi;
}

int os_mkdir(const char* path) { return _mkdir(path) == 0 || errno == EEXIST ? 0 : -1; }
int os_setenv(const char* name, const char* value) { return _putenv_s(name, value) == 0 ? 0 : -1; }

#else
#include <pthread.h>
#include <sys/mman.h>

void* os_reserve(size_t n)
{
    void* p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}
void os_release(void* p, size_t n) { munmap(p, n); }

char* os_stack_top(void)
{
#ifdef __APPLE__
    return (char*)pthread_get_stackaddr_np(pthread_self());
#else
    pthread_attr_t at;
    void* base;
    size_t size;
    pthread_getattr_np(pthread_self(), &at);
    pthread_attr_getstack(&at, &base, &size);
    pthread_attr_destroy(&at);
    return (char*)base + size;
#endif
}

int os_mkdir(const char* path) { return mkdir(path, 0755) == 0 || errno == EEXIST ? 0 : -1; }
int os_setenv(const char* name, const char* value) { return setenv(name, value, 1); }
#endif
