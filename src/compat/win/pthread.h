/* SH3 Quest VR: the part of pthreads sh3-nx's runtime uses, for the Windows PC test build (clang, MSVC
 * libraries). Implemented in compat_win.c over SRW locks and condition variables; this header stays free of
 * windows.h, whose WINAPI macro the runtime defines for itself. */
#pragma once
#include <time.h>

typedef struct { void *srw; unsigned long owner; int count; int type; } pthread_mutex_t;
typedef struct { int type; } pthread_mutexattr_t;
typedef struct { void *cv; int clock; } pthread_cond_t;
typedef struct { int clock; } pthread_condattr_t;

#define PTHREAD_MUTEX_INITIALIZER {0, 0, 0, 0}
#define PTHREAD_COND_INITIALIZER {0, 0}
#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
typedef int clockid_t;
int clock_gettime(clockid_t clock, struct timespec *ts);
#endif

int pthread_mutexattr_init(pthread_mutexattr_t *a);
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type);
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a);
int pthread_mutex_lock(pthread_mutex_t *m);
int pthread_mutex_trylock(pthread_mutex_t *m);
int pthread_mutex_unlock(pthread_mutex_t *m);
int pthread_condattr_init(pthread_condattr_t *a);
int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t clock);
int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a);
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *abstime);
int pthread_cond_signal(pthread_cond_t *c);
int pthread_cond_broadcast(pthread_cond_t *c);
