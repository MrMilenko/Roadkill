/*
 * win32_kernel.c: the KERNEL32 imports TM2.EXE calls, on POSIX (port code, not emulation), and
 * the shared host pieces of win32_host.h (handles, paths, the host lock, allocations, the printf
 * formatter, PE resources).
 *
 * Files: the game's paths (drive letter, '\') are mapped under the data root, TM2_DATA (default
 * disc/files): "D:\GRY\X.TPC" and "GRY\x.tpc" both open <root>/GRY/X.TPC, matched case
 * insensitively per component. The disc shows up as drive TM2_CD (default D:, DRIVE_CDROM);
 * C: is a fixed drive, other letters have no root.
 *
 * Threads: CreateThread runs the game's thread procedure (stdcall, one word at a[0]) on a pthread via
 * rt_call. All game code runs holding the host lock (main takes it at start up); a thread gives
 * it up only while it blocks (Sleep, WaitForSingleObject, its end). NOT YET SAFE: rt.c keeps one
 * call chain (depth) and one frame stack (rt_frame_push) for all threads, so a thread that
 * blocks inside frames while another pushes / pops corrupts the stack. The core needs a frame
 * stack top and call depth per thread (see the report / RUNTIME.md). TerminateThread marks the
 * thread; it ends the next time it blocks.
 *
 * Memory: GlobalAlloc / VirtualAlloc take game memory from the runtime heap (rt_heap_grow) with
 * a host side free list by size class (freed blocks are reused, never returned).
 */
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include "win32_host.h"
#include "host/settings/settings.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "host/os/os.h"
#include "host/data/data_dir.h"

uint32_t w32_last_error;

/* ---- strings ---- */

void w32_put_str(uint32_t dst, const char* s, uint32_t cap)
{
    size_t l = strlen(s);
    if (cap && l >= cap) l = cap - 1;
    memcpy(rt_ptr(dst), s, l);
    rt_w8(dst + (uint32_t)l, 0);
}

uint32_t w32_static_str(const char* s)
{
    uint32_t a = rt_fake_alloc((uint32_t)strlen(s) + 1);
    w32_put_str(a, s, 0);
    return a;
}

/* ---- handles ---- */

#define NHANDLES 1024
#define HBASE 0x00001000u
static struct { int type; void* p; } handles[NHANDLES];

uint32_t w32_handle_new(int type, void* p)
{
    for (int i = 1; i < NHANDLES; i++)
        if (handles[i].type == H_FREE) { handles[i].type = type; handles[i].p = p; return HBASE + 4u * (uint32_t)i; }
    rt_stop("host handle table full", 0);
    return 0;
}
static int hidx(uint32_t h)
{
    if (h < HBASE || h >= HBASE + 4u * NHANDLES || (h & 3)) return -1;
    return (int)((h - HBASE) / 4u);
}
void* w32_handle_get(uint32_t h, int type)
{
    int i = hidx(h);
    return i > 0 && handles[i].type == type ? handles[i].p : NULL;
}
int w32_handle_type(uint32_t h) { int i = hidx(h); return i > 0 ? handles[i].type : H_FREE; }
void w32_handle_free(uint32_t h) { int i = hidx(h); if (i > 0) { handles[i].type = H_FREE; handles[i].p = NULL; } }

/* ---- paths ---- */

const char* w32_data_root(void)
{
    return data_dir();   /* host/data/data_dir.h */
}
char w32_cd_letter(void)
{
    return (char)toupper((unsigned char)*settings_str("debug.cd_letter", "D"));
}

const char* w32_host_path(const char* game, char* out, size_t cap)
{
    const char* g = game;
    if (isalpha((unsigned char)g[0]) && g[1] == ':') g += 2;
    while (*g == '\\' || *g == '/') g++;
    while (g[0] == '.' && (g[1] == '\\' || g[1] == '/')) g += 2;
    snprintf(out, cap, "%s", w32_data_root());
    size_t len = strlen(out);
    while (*g) {
        char comp[256];
        size_t k = 0;
        while (*g && *g != '\\' && *g != '/') { if (k + 1 < sizeof comp) comp[k++] = *g; g++; }
        comp[k] = 0;
        while (*g == '\\' || *g == '/') g++;
        if (!k || !strcmp(comp, ".")) continue;
        /* match the component case insensitively in what is there so far */
        const char* use = comp;
        DIR* d = opendir(out);
        struct dirent* de = NULL;
        if (d) {
            while ((de = readdir(d)) != NULL) if (!strcasecmp(de->d_name, comp)) { use = de->d_name; break; }
        }
        if (len + 1 + strlen(use) < cap) {
            out[len++] = '/';
            strcpy(out + len, use);
            len += strlen(use);
        }
        if (d) closedir(d);
    }
    return out;
}

/* ---- the host lock and threads ---- */

static pthread_mutex_t big = PTHREAD_MUTEX_INITIALIZER;
static int waiting;   /* threads blocked on `big` (atomic) */
static void big_lock(void)
{
    __atomic_add_fetch(&waiting, 1, __ATOMIC_SEQ_CST);
    pthread_mutex_lock(&big);
    __atomic_sub_fetch(&waiting, 1, __ATOMIC_SEQ_CST);
}
void w32_lock(void) { big_lock(); }

/* The main thread never blocks while a match runs (PeekMessage, no waits), so a game thread (the
 * CD monitor, the network thread) would never get the lock: when one is waiting, hand it over and
 * wait until it has taken it (bounded), then take it back. Called from the main thread's
 * PeekMessageA. */
void w32_yield(void)
{
    if (!__atomic_load_n(&waiting, __ATOMIC_SEQ_CST)) return;
    int before = __atomic_load_n(&waiting, __ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&big);
    for (int i = 0; i < 200 && __atomic_load_n(&waiting, __ATOMIC_SEQ_CST) >= before; i++) usleep(50);
    big_lock();
}
void w32_unlock(void) { pthread_mutex_unlock(&big); }

typedef struct Thread {
    pthread_t pt;
    uint32_t id, proc, param, code, handle;
    int done, terminate;
} Thread;
static __thread Thread* self;
static uint32_t next_tid = 2;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;   /* a thread ended, an event / mutex changed */

uint32_t w32_thread_id(void) { return self ? self->id : 1u; }

void w32_block_begin(void) { pthread_mutex_unlock(&big); }
void w32_block_end(void)
{
    big_lock();
    if (self && self->terminate) {
        self->done = 1;
        self->code = 0;
        pthread_cond_broadcast(&changed);
        pthread_mutex_unlock(&big);
        pthread_exit(NULL);
    }
}

static void* thread_main(void* p)
{
    Thread* t = p;
    self = t;
    rt_thread_begin();   /* its own frame slice and call chain */
    big_lock();
    uint32_t args[1] = {t->param};   /* compact: the one stack word at a[0] */
    t->code = rt_call(t->proc, args, 1);
    t->done = 1;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&big);
    return NULL;
}

/* CreateThread(sec, stack, proc, param, flags, &id) */
static uint32_t k_CreateThread(const uint32_t* a, int n)
{
    Thread* t = calloc(1, sizeof *t);
    t->proc = W(2);
    t->param = W(3);
    t->id = next_tid++;
    t->handle = w32_handle_new(H_THREAD, t);
    if (W(5)) rt_w32(W(5), t->id);
    if (W(4) & 4) fprintf(stderr, "tm2: CreateThread suspended: started at once\n");
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 8u << 20);
    pthread_create(&t->pt, &at, thread_main, t);
    pthread_attr_destroy(&at);
    pthread_detach(t->pt);
    return t->handle;
}
static uint32_t k_ExitThread(const uint32_t* a, int n)
{
    if (!self) exit((int)W(0));
    self->code = W(0);
    self->done = 1;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&big);
    pthread_exit(NULL);
    return 0;
}
static uint32_t k_TerminateThread(const uint32_t* a, int n)
{
    Thread* t = w32_handle_get(W(0), H_THREAD);
    if (!t) return 0;
    if (t == self) return k_ExitThread(a + 1, n - 1);
    if (!t->done) { t->terminate = 1; t->code = W(1); }
    return 1;
}

typedef struct { int manual, signaled; uint32_t owner, count; } Sync;

/* CreateMutexA(sec, initial owner, name) */
static uint32_t k_CreateMutexA(const uint32_t* a, int n)
{
    Sync* s = calloc(1, sizeof *s);
    if (W(1)) { s->owner = w32_thread_id(); s->count = 1; }
    return w32_handle_new(H_MUTEX, s);
}
static uint32_t k_ReleaseMutex(const uint32_t* a, int n)
{
    Sync* s = w32_handle_get(W(0), H_MUTEX);
    if (!s || s->owner != w32_thread_id() || !s->count) return 0;
    if (--s->count == 0) { s->owner = 0; pthread_cond_broadcast(&changed); }
    return 1;
}
/* CreateEventA(sec, manual reset, initial state, name) */
static uint32_t k_CreateEventA(const uint32_t* a, int n)
{
    Sync* s = calloc(1, sizeof *s);
    s->manual = W(1) != 0;
    s->signaled = W(2) != 0;
    return w32_handle_new(H_EVENT, s);
}
static uint32_t k_SetEvent(const uint32_t* a, int n)
{
    Sync* s = w32_handle_get(W(0), H_EVENT);
    if (!s) return 0;
    s->signaled = 1;
    pthread_cond_broadcast(&changed);
    return 1;
}

/* 1 when the object is signaled for this thread (and takes it) */
static int try_take(uint32_t h)
{
    Sync* s;
    Thread* t;
    if ((t = w32_handle_get(h, H_THREAD)) != NULL) return t->done;
    if ((s = w32_handle_get(h, H_MUTEX)) != NULL) {
        uint32_t me = w32_thread_id();
        if (s->count && s->owner != me) return 0;
        s->owner = me;
        s->count++;
        return 1;
    }
    if ((s = w32_handle_get(h, H_EVENT)) != NULL) {
        if (!s->signaled) return 0;
        if (!s->manual) s->signaled = 0;
        return 1;
    }
    return 1;   /* anything else: signaled */
}

/* WaitForSingleObject(h, ms): 0 WAIT_OBJECT_0, 0x102 WAIT_TIMEOUT */
static uint32_t k_WaitForSingleObject(const uint32_t* a, int n)
{
    uint32_t h = W(0), ms = W(1);
    if (try_take(h)) return 0;
    if (ms == 0) return 0x102;
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    if (ms != 0xffffffffu) {
        until.tv_sec += ms / 1000;
        until.tv_nsec += (long)(ms % 1000) * 1000000L;
        if (until.tv_nsec >= 1000000000L) { until.tv_sec++; until.tv_nsec -= 1000000000L; }
    }
    /* the condition waits on the big lock itself: waiting gives it up */
    for (;;) {
        int r;
        if (ms == 0xffffffffu) {
            struct timespec t;   /* wake up now and then: a terminate, an object nobody signals */
            clock_gettime(CLOCK_REALTIME, &t);
            t.tv_nsec += 50000000L;
            if (t.tv_nsec >= 1000000000L) { t.tv_sec++; t.tv_nsec -= 1000000000L; }
            r = pthread_cond_timedwait(&changed, &big, &t);
            r = 0;
        } else r = pthread_cond_timedwait(&changed, &big, &until);
        if (self && self->terminate) { w32_block_begin(); w32_block_end(); }
        if (try_take(h)) return 0;
        if (r == ETIMEDOUT) return 0x102;
    }
}

static uint32_t k_Sleep(const uint32_t* a, int n)
{
    uint32_t ms = W(0);
    w32_block_begin();
    if (ms) usleep(ms * 1000u); else sched_yield();
    w32_block_end();
    return 0;
}

static uint32_t k_CloseHandle(const uint32_t* a, int n);

/* ---- time ---- */

static struct timespec t0;
static uint64_t now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)(t.tv_sec - t0.tv_sec) * 1000000u + (uint64_t)((t.tv_nsec - t0.tv_nsec) / 1000);
}
uint32_t w32_ms(void) { return (uint32_t)(now_us() / 1000u); }

static uint32_t k_QueryPerformanceCounter(const uint32_t* a, int n)
{
    uint64_t v = now_us();
    rt_w32(W(0), (uint32_t)v);
    rt_w32(W(0) + 4, (uint32_t)(v >> 32));
    return 1;
}
static uint32_t k_QueryPerformanceFrequency(const uint32_t* a, int n)
{
    rt_w32(W(0), 1000000u);
    rt_w32(W(0) + 4, 0);
    return 1;
}

/* ---- memory ---- */

typedef struct Blk { uint32_t addr, size; int used; } Blk;
static Blk* blks;
static int nblks, capblks;

uint32_t w32_alloc(uint32_t n)
{
    n = (n + 15u) & ~15u;
    if (!n) n = 16;
    for (int i = 0; i < nblks; i++)
        if (!blks[i].used && blks[i].size >= n && blks[i].size <= n + n / 4 + 64) {
            blks[i].used = 1;
            memset(rt_ptr(blks[i].addr), 0, blks[i].size);
            return blks[i].addr;
        }
    uint32_t p = rt_heap_grow(n);
    if (!p) return 0;
    if (nblks == capblks) { capblks = capblks ? capblks * 2 : 256; blks = realloc(blks, sizeof(Blk) * (size_t)capblks); }
    blks[nblks++] = (Blk){p, n, 1};
    return p;
}
static Blk* blk_of(uint32_t p)
{
    for (int i = 0; i < nblks; i++) if (blks[i].addr == p) return &blks[i];
    return NULL;
}
void w32_free(uint32_t p) { Blk* b = blk_of(p); if (b) b->used = 0; }
uint32_t w32_alloc_size(uint32_t p) { Blk* b = blk_of(p); return b ? b->size : 0; }

/* GlobalAlloc(flags, size): the handle is the address (GMEM_MOVEABLE too) */
static uint32_t k_GlobalAlloc(const uint32_t* a, int n) { return w32_alloc(W(1)); }
static uint32_t k_GlobalFree(const uint32_t* a, int n) { w32_free(W(0)); return 0; }
static uint32_t k_GlobalLock(const uint32_t* a, int n) { return W(0); }
static uint32_t k_GlobalUnlock(const uint32_t* a, int n) { (void)a; (void)n; return 0; }
/* VirtualAlloc(addr, size, type, protect): a fixed address request is honoured only when it is
 * one of ours (MEM_COMMIT of a reserved block) */
static uint32_t k_VirtualAlloc(const uint32_t* a, int n)
{
    if (W(0) && blk_of(W(0))) return W(0);
    return w32_alloc(W(1));
}
static uint32_t k_VirtualFree(const uint32_t* a, int n) { if (W(2) & 0x8000u) w32_free(W(0)); return 1; }

/* ---- files ---- */

typedef struct { int fd; } File;

/* CreateFileA(name, access, share, sec, disposition, flags, template) */
static uint32_t k_CreateFileA(const uint32_t* a, int n)
{
    const char* name = w32_str(W(0));
    char path[1024];
    w32_host_path(name, path, sizeof path);
    uint32_t acc = W(1), disp = W(4);
    int fl = (acc & 0x40000000u) ? ((acc & 0x80000000u) ? O_RDWR : O_WRONLY) : O_RDONLY;
    switch (disp) {
    case 1: fl |= O_CREAT | O_EXCL; break;    /* CREATE_NEW */
    case 2: fl |= O_CREAT | O_TRUNC; break;   /* CREATE_ALWAYS */
    case 3: break;                            /* OPEN_EXISTING */
    case 4: fl |= O_CREAT; break;             /* OPEN_ALWAYS */
    case 5: fl |= O_TRUNC; break;             /* TRUNCATE_EXISTING */
    }
    int fd = open(path, fl | O_BINARY, 0644);
    if (settings_bool("debug.trace_files", 0)) fprintf(stderr, "tm2: CreateFileA %s -> %s %s\n", name, path, fd < 0 ? "FAILED" : "ok");
    if (fd < 0) { w32_last_error = errno == ENOENT ? 2u : 5u; return 0xffffffffu; }
    struct stat st;
    if (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode)) { close(fd); w32_last_error = 5; return 0xffffffffu; }
    File* f = calloc(1, sizeof *f);
    f->fd = fd;
    w32_last_error = 0;
    return w32_handle_new(H_FILE, f);
}

static int fd_of(uint32_t h)
{
    if (h == 0xfffffff6u || h == 0x10u) return 0;        /* the std handles (GetStdHandle) */
    if (h == 0xfffffff5u || h == 0x11u) return 1;
    if (h == 0xfffffff4u || h == 0x12u) return 2;
    File* f = w32_handle_get(h, H_FILE);
    return f ? f->fd : -1;
}

/* ReadFile(h, buf, n, &read, overlapped) */
static uint32_t k_ReadFile(const uint32_t* a, int n)
{
    int fd = fd_of(W(0));
    if (W(3)) rt_w32(W(3), 0);
    if (fd < 0) { w32_last_error = 6; return 0; }
    uint32_t want = W(2), got = 0;
    if (W(1) + (uint64_t)want > RT_SIZE) want = RT_SIZE - W(1);
    while (got < want) {
        ssize_t r = read(fd, rt_ptr(W(1) + got), want - got);
        if (r <= 0) break;
        got += (uint32_t)r;
    }
    if (W(3)) rt_w32(W(3), got);
    return 1;
}
static uint32_t k_WriteFile(const uint32_t* a, int n)
{
    int fd = fd_of(W(0));
    if (W(3)) rt_w32(W(3), 0);
    if (fd < 0) { w32_last_error = 6; return 0; }
    ssize_t r = write(fd, rt_ptr(W(1)), W(2));
    if (r < 0) return 0;
    if (W(3)) rt_w32(W(3), (uint32_t)r);
    return 1;
}
/* SetFilePointer(h, low, &high, method) */
static uint32_t k_SetFilePointer(const uint32_t* a, int n)
{
    int fd = fd_of(W(0));
    if (fd < 0) { w32_last_error = 6; return 0xffffffffu; }
    int64_t off = (int32_t)W(1);
    if (W(2)) off = (int64_t)((uint64_t)rt_r32(W(2)) << 32 | W(1));
    off_t r = lseek(fd, (off_t)off, W(3) == 0 ? SEEK_SET : W(3) == 1 ? SEEK_CUR : SEEK_END);
    if (r < 0) { w32_last_error = 0x83; return 0xffffffffu; }
    if (W(2)) rt_w32(W(2), (uint32_t)((uint64_t)r >> 32));
    return (uint32_t)r;
}
/* GetFileSize(h, &high) */
static uint32_t k_GetFileSize(const uint32_t* a, int n)
{
    int fd = fd_of(W(0));
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) return 0xffffffffu;
    if (W(1)) rt_w32(W(1), (uint32_t)((uint64_t)st.st_size >> 32));
    return (uint32_t)st.st_size;
}
static uint32_t k_GetFileType(const uint32_t* a, int n)
{
    int fd = fd_of(W(0));
    if (fd < 0) return 0;
    return fd <= 2 && isatty(fd) ? 2u : 1u;   /* FILE_TYPE_CHAR / FILE_TYPE_DISK */
}
static uint32_t k_DeleteFileA(const uint32_t* a, int n)
{
    char path[1024];
    return unlink(w32_host_path(w32_str(W(0)), path, sizeof path)) == 0;
}
static uint32_t k_GetStdHandle(const uint32_t* a, int n) { return W(0); }   /* -10/-11/-12 stay */
static uint32_t k_SetStdHandle(const uint32_t* a, int n) { (void)a; (void)n; return 1; }

static uint32_t k_CloseHandle(const uint32_t* a, int n)
{
    uint32_t h = W(0);
    File* f = w32_handle_get(h, H_FILE);
    if (f) { close(f->fd); free(f); w32_handle_free(h); return 1; }
    switch (w32_handle_type(h)) {
    case H_MUTEX: case H_EVENT: free(w32_handle_get(h, w32_handle_type(h))); w32_handle_free(h); return 1;
    case H_THREAD: return 1;   /* the thread record stays: it may still run */
    case H_FREE: return 0;
    default: w32_handle_free(h); return 1;
    }
}

/* GetDriveTypeA("X:\") */
static uint32_t k_GetDriveTypeA(const uint32_t* a, int n)
{
    const char* r = w32_str(W(0));
    if (!r || !isalpha((unsigned char)r[0])) return 1;
    char c = (char)toupper((unsigned char)r[0]);
    if (c == w32_cd_letter()) return 5;   /* DRIVE_CDROM */
    if (c == 'C') return 3;               /* DRIVE_FIXED */
    return 1;                             /* DRIVE_NO_ROOT_DIR */
}

/* ---- strings ---- */

static uint32_t k_lstrcpyA(const uint32_t* a, int n)
{
    memmove(rt_ptr(W(0)), rt_ptr(W(1)), strlen(w32_str(W(1))) + 1);
    return W(0);
}
static uint32_t k_lstrcatA(const uint32_t* a, int n)
{
    char* d = (char*)rt_ptr(W(0));
    memmove(d + strlen(d), rt_ptr(W(1)), strlen(w32_str(W(1))) + 1);
    return W(0);
}
static uint32_t k_lstrlenA(const uint32_t* a, int n) { return W(0) ? (uint32_t)strlen(w32_str(W(0))) : 0; }
/* lstrcmpA: Windows compares by word sort (case insensitive first); close enough for the game's
 * uses (equality tests): strcmp with a case insensitive first pass */
static uint32_t k_lstrcmpA(const uint32_t* a, int n)
{
    const char* x = w32_str(W(0)) ? w32_str(W(0)) : "";
    const char* y = w32_str(W(1)) ? w32_str(W(1)) : "";
    int r = strcasecmp(x, y);
    if (!r) r = strcmp(x, y);
    return (uint32_t)(r < 0 ? -1 : r > 0);
}

/* ---- the process ---- */

static uint32_t cmdline, modname, envstr;
static uint32_t k_GetCommandLineA(const uint32_t* a, int n)
{
    (void)a; (void)n;
    if (!cmdline) {
        const char* c = settings_str("debug.cmdline", NULL);
        char buf[512];
        snprintf(buf, sizeof buf, "%c:\\TM2.EXE%s%s", w32_cd_letter(), c && *c ? " " : "", c ? c : "");
        cmdline = w32_static_str(buf);
    }
    return cmdline;
}
/* GetModuleFileNameA(module, buf, size) */
static uint32_t k_GetModuleFileNameA(const uint32_t* a, int n)
{
    if (!modname) { char b[16]; snprintf(b, sizeof b, "%c:\\TM2.EXE", w32_cd_letter()); modname = w32_static_str(b); }
    w32_put_str(W(1), w32_str(modname), W(2));
    return (uint32_t)strlen(w32_str(W(1)));
}
static uint32_t k_GetModuleHandleA(const uint32_t* a, int n) { return W(0) ? 0 : 0x00400000u; }
static uint32_t k_GetVersion(const uint32_t* a, int n) { (void)a; (void)n; return 0xc0000004u; }   /* Windows 95 */
static uint32_t k_GetEnvironmentStrings(const uint32_t* a, int n)
{
    (void)a; (void)n;
    if (!envstr) { envstr = rt_fake_alloc(16); rt_w32(envstr, 0); }
    return envstr;
}
static uint32_t k_GetLastError(const uint32_t* a, int n) { (void)a; (void)n; return w32_last_error; }
static uint32_t k_GetCurrentProcessId(const uint32_t* a, int n) { (void)a; (void)n; return 0x100u; }
static uint32_t k_GetCurrentThreadId(const uint32_t* a, int n) { (void)a; (void)n; return w32_thread_id(); }
static uint32_t k_GetCurrentThread(const uint32_t* a, int n) { (void)a; (void)n; return 0xfffffffeu; }
static uint32_t k_ExitProcess(const uint32_t* a, int n) { fflush(NULL); exit((int)W(0)); }
/* CreateProcessA: not supported (the game runs its setup / help this way) */
static uint32_t k_CreateProcessA(const uint32_t* a, int n)
{
    fprintf(stderr, "tm2: CreateProcessA(%s, %s) not supported\n", W(0) ? w32_str(W(0)) : "", W(1) ? w32_str(W(1)) : "");
    w32_last_error = 2;
    return 0;
}
static uint32_t k_GetExitCodeProcess(const uint32_t* a, int n) { if (W(1)) rt_w32(W(1), 0); return 1; }

/* LoadLibraryA: a module handle that is never a real one (DLL code is never run) */
static uint32_t k_LoadLibraryA(const uint32_t* a, int n)
{
    const char* s = w32_str(W(0));
    return w32_handle_new(H_MODULE, s ? strdup(s) : NULL);
}
static uint32_t k_FreeLibrary(const uint32_t* a, int n)
{
    void* p = w32_handle_get(W(0), H_MODULE);
    if (!p && w32_handle_type(W(0)) != H_MODULE) return 0;
    free(p);
    w32_handle_free(W(0));
    return 1;
}
/* GetProcAddress(module, name): the import slot of a function the game imports anyway, else 0 */
static uint32_t k_GetProcAddress(const uint32_t* a, int n)
{
    if (W(1) < 0x10000u) return 0;
    uint32_t s = rt_import_slot(w32_str(W(1)));
    if (!s) fprintf(stderr, "tm2: GetProcAddress(%s) not available\n", w32_str(W(1)));
    return s;
}

/* TLS: per thread slots */
static __thread uint32_t tls[64];
static uint32_t tls_used;
static uint32_t k_TlsAlloc(const uint32_t* a, int n)
{
    (void)a; (void)n;
    for (uint32_t i = 0; i < 32; i++) if (!(tls_used & (1u << i))) { tls_used |= 1u << i; return i; }
    return 0xffffffffu;
}
static uint32_t k_TlsFree(const uint32_t* a, int n) { if (W(0) < 32) tls_used &= ~(1u << W(0)); return 1; }
static uint32_t k_TlsGetValue(const uint32_t* a, int n) { return W(0) < 64 ? tls[W(0)] : 0; }
static uint32_t k_TlsSetValue(const uint32_t* a, int n) { if (W(0) < 64) tls[W(0)] = W(1); return 1; }

/* the console: TM2.EXE is a GUI program, the runtime only probes it */
static uint32_t k_GetConsoleMode(const uint32_t* a, int n) { (void)a; (void)n; return 0; }
static uint32_t k_SetConsoleMode(const uint32_t* a, int n) { (void)a; (void)n; return 0; }
static uint32_t k_ReadConsoleInputA(const uint32_t* a, int n) { if (W(3)) rt_w32(W(3), 0); return 0; }
static uint32_t k_WriteConsoleA(const uint32_t* a, int n)
{
    fwrite(rt_ptr(W(1)), 1, W(2), stderr);
    if (W(3)) rt_w32(W(3), W(2));
    return 1;
}
static uint32_t k_SetConsoleCtrlHandler(const uint32_t* a, int n) { (void)a; (void)n; return 1; }
/* RtlUnwind: only the original's longjmp calls it (the host longjmp does not, watcom_crt.c) */
static uint32_t k_RtlUnwind(const uint32_t* a, int n) { (void)a; (void)n; return 0; }

/* ---- resources (the image's .rsrc, loaded at its address) ---- */

static uint32_t rsrc_base(void)
{
    uint32_t pe = 0x00400000u + rt_r32(0x0040003cu);
    uint32_t rva = rt_r32(pe + 0x88);   /* data directory 2: resources */
    return rva ? 0x00400000u + rva : 0;
}
/* the entry in directory d for id (int) or name (game string; "#n" = n) */
static uint32_t rsrc_find(uint32_t base, uint32_t d, uint32_t key)
{
    uint32_t nnamed = rt_r16(d + 12), nid = rt_r16(d + 14);
    const char* s = key >= 0x10000u ? w32_str(key) : NULL;
    if (s && s[0] == '#') { key = (uint32_t)strtoul(s + 1, NULL, 10); s = NULL; }
    for (uint32_t i = 0; i < nnamed + nid; i++) {
        uint32_t e = d + 16 + 8 * i, nm = rt_r32(e), off = rt_r32(e + 4);
        int match;
        if (nm & 0x80000000u) {
            if (!s) continue;
            uint32_t str = base + (nm & 0x7fffffffu);
            uint32_t len = rt_r16(str);
            match = strlen(s) == len;
            for (uint32_t k = 0; match && k < len; k++)
                match = toupper((unsigned char)s[k]) == toupper(rt_r16(str + 2 + 2 * k) & 0xff);
        } else match = !s && nm == key;
        if (match) return off;
    }
    return 0xffffffffu;
}
uint32_t w32_find_resource(uint32_t type, uint32_t name)
{
    uint32_t base = rsrc_base();
    if (!base) return 0;
    uint32_t o = rsrc_find(base, base, type);
    if (o == 0xffffffffu || !(o & 0x80000000u)) return 0;
    uint32_t d = base + (o & 0x7fffffffu);
    o = rsrc_find(base, d, name);
    if (o == 0xffffffffu) return 0;
    if (o & 0x80000000u) {   /* the language level: the first entry */
        d = base + (o & 0x7fffffffu);
        if (rt_r16(d + 12) + rt_r16(d + 14) == 0) return 0;
        o = rt_r32(d + 16 + 4);
    }
    return o & 0x80000000u ? 0 : base + o;
}
/* FindResourceA(module, name, type) */
static uint32_t k_FindResourceA(const uint32_t* a, int n) { return w32_find_resource(W(2), W(1)); }
static uint32_t k_LoadResource(const uint32_t* a, int n) { return W(1); }
static uint32_t k_LockResource(const uint32_t* a, int n) { return W(0) ? 0x00400000u + rt_r32(W(0)) : 0; }
static uint32_t k_FreeResource(const uint32_t* a, int n) { (void)a; (void)n; return 0; }

/* ---- the formatter (wsprintfA, the Watcom sprintf family) ---- */

int w32_format(char* out, size_t cap, const char* f, W32Next next, void* user)
{
    size_t o = 0;
#define PUT(c) do { if (o + 1 < cap) out[o] = (char)(c); o++; } while (0)
    while (*f) {
        if (*f != '%') { PUT(*f++); continue; }
        const char* st = f++;
        char spec[32];
        size_t k = 0;
        spec[k++] = '%';
        while (*f && strchr("-+ #0", *f)) { if (k < 20) spec[k++] = *f; f++; }
        if (*f == '*') { k += (size_t)snprintf(spec + k, sizeof spec - k, "%d", (int32_t)next(user)); f++; }
        while (isdigit((unsigned char)*f)) { if (k < 24) spec[k++] = *f; f++; }
        if (*f == '.') {
            spec[k++] = *f++;
            if (*f == '*') { k += (size_t)snprintf(spec + k, sizeof spec - k, "%d", (int32_t)next(user)); f++; }
            while (isdigit((unsigned char)*f)) { if (k < 28) spec[k++] = *f; f++; }
        }
        int lng = 0;
        while (*f == 'l' || *f == 'h' || *f == 'L' || *f == 'N' || *f == 'F' || *f == 'w') { if (*f == 'L') lng = 2; f++; }
        char c = *f ? *f++ : 0;
        char tmp[512];
        tmp[0] = 0;
        switch (c) {
        case 'd': case 'i': spec[k++] = 'd'; spec[k] = 0; snprintf(tmp, sizeof tmp, spec, (int32_t)next(user)); break;
        case 'u': case 'x': case 'X': case 'o':
            spec[k++] = c; spec[k] = 0; snprintf(tmp, sizeof tmp, spec, (unsigned)next(user)); break;
        case 'p': snprintf(tmp, sizeof tmp, "%08X", (unsigned)next(user)); break;
        case 'c': spec[k++] = 'c'; spec[k] = 0; snprintf(tmp, sizeof tmp, spec, (int)(next(user) & 0xff)); break;
        case 's': {
            uint32_t p = next(user);
            spec[k++] = 's'; spec[k] = 0;
            snprintf(tmp, sizeof tmp, spec, p ? w32_str(p) : "(null)");
            break;
        }
        case 'f': case 'e': case 'E': case 'g': case 'G': {
            uint64_t lo = next(user), hi = next(user);
            if (lng == 2) (void)next(user);   /* long double: Watcom's is a double too, 8 bytes */
            uint64_t bits = lo | hi << 32;
            double v;
            memcpy(&v, &bits, 8);
            spec[k++] = c; spec[k] = 0;
            snprintf(tmp, sizeof tmp, spec, v);
            break;
        }
        case 'n': { uint32_t p = next(user); if (p) rt_w32(p, (uint32_t)o); break; }
        case '%': tmp[0] = '%'; tmp[1] = 0; break;
        default:   /* unknown: copied as is */
            for (const char* q = st; q < f && o + 1 < cap; q++) PUT(*q);
            continue;
        }
        for (const char* q = tmp; *q; q++) PUT(*q);
    }
    if (cap) out[o < cap ? o : cap - 1] = 0;
#undef PUT
    return (int)o;
}

typedef struct { const uint32_t* a; int n, i; uint32_t va; } Words;
static uint32_t next_word(void* u)
{
    Words* w = u;
    if (w->va) { uint32_t v = rt_r32(w->va); w->va += 4; return v; }
    return w->i < w->n ? w->a[w->i++] : 0;
}
/* wsprintfA(buf, fmt, ...), cdecl: at most 1024 bytes out */
static uint32_t k_wsprintfA(const uint32_t* a, int n)
{
    char buf[1025];
    Words w = {a, n, 2, 0};
    w32_format(buf, sizeof buf, w32_str(W(1)), next_word, &w);
    w32_put_str(W(0), buf, 1025);
    return (uint32_t)strlen(buf);
}
/* wvsprintfA(buf, fmt, va_list address) */
static uint32_t k_wvsprintfA(const uint32_t* a, int n)
{
    char buf[1025];
    Words w = {a, n, 0, W(2)};
    w32_format(buf, sizeof buf, w32_str(W(1)), next_word, &w);
    w32_put_str(W(0), buf, 1025);
    return (uint32_t)strlen(buf);
}

/* ---- ole32 ---- */
static uint32_t k_CoInitialize(const uint32_t* a, int n) { (void)a; (void)n; return 0; }
static uint32_t k_CoUninitialize(const uint32_t* a, int n) { (void)a; (void)n; return 0; }

void rt_bind_win32_kernel(void)
{
    clock_gettime(CLOCK_MONOTONIC, &t0);
    w32_lock();   /* the main thread runs game code holding the host lock */
    /* the IAT slots hold their own address, so a function pointer read from one (a window
     * procedure, GetProcAddress) calls the import's binding */
    for (uint32_t s = 0x00c90000u; s < 0x00c91600u; s += 4)
        if (rt_import_name(s)) rt_w32(s, s);
    static const struct { const char* n; RtFn f; } t[] = {
        {"CloseHandle", k_CloseHandle}, {"CreateFileA", k_CreateFileA}, {"CreateMutexA", k_CreateMutexA},
        {"CreateProcessA", k_CreateProcessA}, {"CreateThread", k_CreateThread}, {"FindResourceA", k_FindResourceA},
        {"FreeLibrary", k_FreeLibrary}, {"FreeResource", k_FreeResource}, {"GetCommandLineA", k_GetCommandLineA},
        {"GetDriveTypeA", k_GetDriveTypeA}, {"GetExitCodeProcess", k_GetExitCodeProcess},
        {"GetFileSize", k_GetFileSize}, {"GlobalAlloc", k_GlobalAlloc}, {"GlobalFree", k_GlobalFree},
        {"GlobalLock", k_GlobalLock}, {"GlobalUnlock", k_GlobalUnlock}, {"LoadLibraryA", k_LoadLibraryA},
        {"LoadResource", k_LoadResource}, {"LockResource", k_LockResource},
        {"QueryPerformanceCounter", k_QueryPerformanceCounter},
        {"QueryPerformanceFrequency", k_QueryPerformanceFrequency}, {"ReadFile", k_ReadFile},
        {"ReleaseMutex", k_ReleaseMutex}, {"SetFilePointer", k_SetFilePointer}, {"Sleep", k_Sleep},
        {"TerminateThread", k_TerminateThread}, {"WaitForSingleObject", k_WaitForSingleObject},
        {"WriteFile", k_WriteFile}, {"lstrcatA", k_lstrcatA}, {"lstrcmpA", k_lstrcmpA}, {"lstrcpyA", k_lstrcpyA},
        {"lstrlenA", k_lstrlenA}, {"CreateEventA", k_CreateEventA}, {"DeleteFileA", k_DeleteFileA},
        {"ExitProcess", k_ExitProcess}, {"ExitThread", k_ExitThread}, {"GetConsoleMode", k_GetConsoleMode},
        {"GetCurrentProcessId", k_GetCurrentProcessId}, {"GetCurrentThreadId", k_GetCurrentThreadId},
        {"GetCurrentThread", k_GetCurrentThread}, {"GetEnvironmentStrings", k_GetEnvironmentStrings},
        {"GetFileType", k_GetFileType}, {"GetLastError", k_GetLastError},
        {"GetModuleFileNameA", k_GetModuleFileNameA}, {"GetModuleHandleA", k_GetModuleHandleA},
        {"GetProcAddress", k_GetProcAddress}, {"GetStdHandle", k_GetStdHandle}, {"GetVersion", k_GetVersion},
        {"ReadConsoleInputA", k_ReadConsoleInputA}, {"RtlUnwind", k_RtlUnwind},
        {"SetConsoleCtrlHandler", k_SetConsoleCtrlHandler}, {"SetConsoleMode", k_SetConsoleMode},
        {"SetEvent", k_SetEvent}, {"SetStdHandle", k_SetStdHandle}, {"TlsAlloc", k_TlsAlloc},
        {"TlsFree", k_TlsFree}, {"TlsGetValue", k_TlsGetValue}, {"TlsSetValue", k_TlsSetValue},
        {"VirtualAlloc", k_VirtualAlloc}, {"VirtualFree", k_VirtualFree}, {"WriteConsoleA", k_WriteConsoleA},
        {"wsprintfA", k_wsprintfA}, {"wvsprintfA", k_wvsprintfA},
        {"CoInitialize", k_CoInitialize}, {"CoUninitialize", k_CoUninitialize},
    };
    /* the same function imported from KERNEL32.dll and KERNEL32.DLL has two slots: bind both */
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        static const char* dlls[] = {"KERNEL32.dll!", "KERNEL32.DLL!", "USER32.dll!", "USER32.DLL!", "ole32.dll!"};
        int any = 0;
        for (size_t d = 0; d < sizeof dlls / sizeof dlls[0]; d++) {
            char nm[96];
            snprintf(nm, sizeof nm, "%s%s", dlls[d], t[i].n);
            if (rt_import_slot(nm)) { rt_bind_import(strdup(nm), t[i].f); any = 1; }
        }
        if (!any) fprintf(stderr, "tm2: host: %s is not imported\n", t[i].n);
    }
}

RT_BIND_AREA(win32_kernel)
