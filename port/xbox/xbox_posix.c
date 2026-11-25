/*
 * xbox_posix.c: the C99 / POSIX calls Roadkill's host makes that the XDK's C library lacks
 * (compat/*.h map the names here). Port code.
 */
#include <xtl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <pthread.h>
#include <dirent.h>

#undef fopen
#undef getenv
#undef open
#undef stat
#undef fstat

/* ---- printf ---- */

int rk_vsnprintf(char* s, size_t n, const char* fmt, va_list ap)
{
    int need = _vscprintf(fmt, ap);
    if (n) {
        int r = _vsnprintf(s, n, fmt, ap);
        if (r < 0 || (size_t)r >= n) s[n - 1] = 0;
    }
    return need;
}

int rk_snprintf(char* s, size_t n, const char* fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = rk_vsnprintf(s, n, fmt, ap);
    va_end(ap);
    return r;
}

/* ---- the environment ---- */

static struct { char* name; char* value; } env[64];
static int nenv;

char* rk_getenv(const char* name)
{
    int i;
    for (i = 0; i < nenv; i++) if (!strcmp(env[i].name, name)) return env[i].value;
    return NULL;
}

int rk_setenv(const char* name, const char* value, int overwrite)
{
    int i;
    for (i = 0; i < nenv; i++)
        if (!strcmp(env[i].name, name)) {
            if (overwrite) { free(env[i].value); env[i].value = _strdup(value); }
            return 0;
        }
    if (nenv == 64) return -1;
    env[nenv].name = _strdup(name);
    env[nenv].value = _strdup(value);
    nenv++;
    return 0;
}

/* ---- paths: '\' and a drive ---- */

static const char* xpath(const char* in, char* out, size_t cap)
{
    size_t i;
    for (i = 0; in[i] && i + 1 < cap; i++) out[i] = in[i] == '/' ? '\\' : in[i];
    out[i] = 0;
    /* collapse "\\" runs (a join of "D:\data" and "\x") */
    {
        char* w = out;
        const char* r = out;
        while (*r) { *w++ = *r; if (*r == '\\') while (r[1] == '\\') r++; r++; }
        *w = 0;
    }
    return out;
}

FILE* rk_fopen(const char* path, const char* mode)
{
    char p[MAX_PATH];
    return fopen(xpath(path, p, sizeof p), mode);
}

int rk_open(const char* path, int flags, ...)
{
    char p[MAX_PATH];
    int mode = _S_IREAD | _S_IWRITE;
    return _open(xpath(path, p, sizeof p), flags | _O_BINARY, mode);
}

int rk_stat(const char* path, void* st)
{
    char p[MAX_PATH];
    return _stat(xpath(path, p, sizeof p), (struct _stat*)st);
}

int rk_fstat(int fd, void* st) { return _fstat(fd, (struct _stat*)st); }

/* ---- directories ---- */

struct RkDir { HANDLE h; WIN32_FIND_DATA fd; int first; struct dirent de; };

DIR* opendir(const char* path)
{
    char p[MAX_PATH];
    size_t l;
    DIR* d;
    xpath(path, p, sizeof p - 3);
    l = strlen(p);
    if (l && p[l - 1] != '\\') p[l++] = '\\';
    p[l++] = '*';
    p[l] = 0;
    d = (DIR*)calloc(1, sizeof *d);
    if (!d) return NULL;
    d->h = FindFirstFile(p, &d->fd);
    if (d->h == INVALID_HANDLE_VALUE) { free(d); return NULL; }
    d->first = 1;
    return d;
}

struct dirent* readdir(DIR* d)
{
    if (!d->first && !FindNextFile(d->h, &d->fd)) return NULL;
    d->first = 0;
    strncpy(d->de.d_name, d->fd.cFileName, sizeof d->de.d_name - 1);
    return &d->de;
}

int closedir(DIR* d)
{
    if (!d) return -1;
    FindClose(d->h);
    free(d);
    return 0;
}

/* ---- time ---- */

int usleep(unsigned int usec) { Sleep((usec + 999) / 1000); return 0; }
int sched_yield(void) { Sleep(0); return 0; }

int clock_gettime(int clock, struct timespec* t)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER c;
    (void)clock;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    t->tv_sec = (long)(c.QuadPart / freq.QuadPart);
    t->tv_nsec = (long)((c.QuadPart % freq.QuadPart) * 1000000000 / freq.QuadPart);
    return 0;
}
