/*
 * xbox_pthread.c: compat/pthread.h over the Xbox kernel's threads (port code). Mutexes are critical
 * sections; condition variables are the classic two-semaphore kind (as SDL's generic one); both are
 * made on first use so PTHREAD_*_INITIALIZER works.
 */
#include <xtl.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct { CRITICAL_SECTION lock; HANDLE wait_sem, wait_done; int waiting, signals; } Cond;

static void make_once(volatile long* state, void (*make)(void*), void* arg)
{
    if (*state == 2) return;
    if (InterlockedCompareExchange((LONG*)state, 1, 0) == 0) { make(arg); *state = 2; return; }
    while (*state != 2) Sleep(0);
}

static void make_mutex(void* p)
{
    pthread_mutex_t* m = (pthread_mutex_t*)p;
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)malloc(sizeof *cs);
    InitializeCriticalSection(cs);
    m->m = cs;
}

static void make_cond(void* p)
{
    pthread_cond_t* c = (pthread_cond_t*)p;
    Cond* k = (Cond*)calloc(1, sizeof *k);
    InitializeCriticalSection(&k->lock);
    k->wait_sem = CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
    k->wait_done = CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
    c->c = k;
}

int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* a) { (void)a; m->state = 0; make_once(&m->state, make_mutex, m); return 0; }
int pthread_mutex_lock(pthread_mutex_t* m) { make_once(&m->state, make_mutex, m); EnterCriticalSection((CRITICAL_SECTION*)m->m); return 0; }
int pthread_mutex_unlock(pthread_mutex_t* m) { LeaveCriticalSection((CRITICAL_SECTION*)m->m); return 0; }

int pthread_cond_init(pthread_cond_t* c, const pthread_condattr_t* a) { (void)a; c->state = 0; make_once(&c->state, make_cond, c); return 0; }

static int cond_wait(pthread_cond_t* c, pthread_mutex_t* m, DWORD ms)
{
    Cond* k;
    DWORD r;
    make_once(&c->state, make_cond, c);
    k = (Cond*)c->c;
    EnterCriticalSection(&k->lock);
    k->waiting++;
    LeaveCriticalSection(&k->lock);
    pthread_mutex_unlock(m);
    r = WaitForSingleObject(k->wait_sem, ms);
    EnterCriticalSection(&k->lock);
    if (k->signals > 0) {
        if (r != WAIT_OBJECT_0) WaitForSingleObject(k->wait_sem, INFINITE);   /* the signal meant for us */
        ReleaseSemaphore(k->wait_done, 1, NULL);
        k->signals--;
    }
    k->waiting--;
    LeaveCriticalSection(&k->lock);
    pthread_mutex_lock(m);
    return r == WAIT_OBJECT_0 ? 0 : 138;   /* ETIMEDOUT (compat/errno.h) */
}

int pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m) { return cond_wait(c, m, INFINITE); }

int pthread_cond_timedwait(pthread_cond_t* c, pthread_mutex_t* m, const struct timespec* until)
{
    struct timespec now;
    double ms;
    clock_gettime(CLOCK_REALTIME, &now);
    ms = (until->tv_sec - now.tv_sec) * 1000.0 + (until->tv_nsec - now.tv_nsec) / 1e6;
    return cond_wait(c, m, ms <= 0 ? 0 : (DWORD)(ms + 0.5));
}

static int cond_wake(pthread_cond_t* c, int all)
{
    Cond* k;
    int n = 0;
    make_once(&c->state, make_cond, c);
    k = (Cond*)c->c;
    EnterCriticalSection(&k->lock);
    if (k->waiting > k->signals) {
        n = all ? k->waiting - k->signals : 1;
        k->signals += n;
        ReleaseSemaphore(k->wait_sem, n, NULL);
    }
    LeaveCriticalSection(&k->lock);
    while (n-- > 0) WaitForSingleObject(k->wait_done, INFINITE);
    return 0;
}
int pthread_cond_broadcast(pthread_cond_t* c) { return cond_wake(c, 1); }
int pthread_cond_signal(pthread_cond_t* c) { return cond_wake(c, 0); }

static void run_once(void* fn) { ((void (*)(void))fn)(); }
int pthread_once(pthread_once_t* o, void (*fn)(void)) { make_once(o, run_once, (void*)fn); return 0; }

int pthread_attr_init(pthread_attr_t* a) { a->stack = 0; return 0; }
int pthread_attr_destroy(pthread_attr_t* a) { (void)a; return 0; }
int pthread_attr_setstacksize(pthread_attr_t* a, size_t n) { a->stack = n; return 0; }

typedef struct { void* (*fn)(void*); void* arg; } Start;

static DWORD WINAPI thread_main(LPVOID p)
{
    Start s = *(Start*)p;
    free(p);
    s.fn(s.arg);
    rk_tls_release();
    return 0;
}

int pthread_create(pthread_t* t, const pthread_attr_t* a, void* (*fn)(void*), void* arg)
{
    DWORD id;
    HANDLE h;
    Start* s = (Start*)malloc(sizeof *s);
    s->fn = fn;
    s->arg = arg;
    /* the console's memory is scarce: 256 KB stacks (the desktop asks 8 MB) */
    h = CreateThread(NULL, a && a->stack ? (a->stack > 0x40000 ? 0x40000 : a->stack) : 0x40000, thread_main, s, 0, &id);
    if (!h) { free(s); return 11; }
    CloseHandle(h);
    *t = id;
    return 0;
}

int pthread_detach(pthread_t t) { (void)t; return 0; }
void pthread_exit(void* v) { (void)v; rk_tls_release(); ExitThread(0); }
pthread_t pthread_self(void) { return GetCurrentThreadId(); }
