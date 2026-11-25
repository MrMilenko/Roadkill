/*
 * pthread.h for the Xbox build: the POSIX threads Roadkill's host uses, over the kernel's threads
 * (xbox_pthread.c). Mutexes and condition variables are made on first use, so the static
 * initializers work.
 */
#ifndef RK_PTHREAD_H
#define RK_PTHREAD_H
#include <stddef.h>
#include <time.h>

typedef struct { volatile long state; void* m; } pthread_mutex_t;
typedef struct { volatile long state; void* c; } pthread_cond_t;
typedef unsigned long pthread_t;
typedef struct { size_t stack; } pthread_attr_t;
typedef int pthread_mutexattr_t;
typedef int pthread_condattr_t;
typedef volatile long pthread_once_t;

#define PTHREAD_MUTEX_INITIALIZER { 0, 0 }
#define PTHREAD_COND_INITIALIZER { 0, 0 }
#define PTHREAD_ONCE_INIT 0

#ifndef RK_TIMESPEC
#define RK_TIMESPEC
struct timespec { long tv_sec; long tv_nsec; };
#endif
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
int clock_gettime(int clock, struct timespec* t);
int sched_yield(void);

int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* a);
int pthread_mutex_lock(pthread_mutex_t* m);
int pthread_mutex_unlock(pthread_mutex_t* m);
int pthread_cond_init(pthread_cond_t* c, const pthread_condattr_t* a);
int pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m);
int pthread_cond_timedwait(pthread_cond_t* c, pthread_mutex_t* m, const struct timespec* until);
int pthread_cond_broadcast(pthread_cond_t* c);
int pthread_cond_signal(pthread_cond_t* c);
int pthread_once(pthread_once_t* o, void (*fn)(void));
int pthread_attr_init(pthread_attr_t* a);
int pthread_attr_destroy(pthread_attr_t* a);
int pthread_attr_setstacksize(pthread_attr_t* a, size_t n);
int pthread_create(pthread_t* t, const pthread_attr_t* a, void* (*fn)(void*), void* arg);
int pthread_detach(pthread_t t);
void pthread_exit(void* v);
pthread_t pthread_self(void);
#endif
