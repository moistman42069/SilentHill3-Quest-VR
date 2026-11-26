/* SH3 Quest VR: pthreads, dirent and the POSIX sleeps and clocks for the Windows PC test build, over Win32. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "pthread.h"
#include "dirent.h"
#include "unistd.h"

_Static_assert(sizeof(SRWLOCK) == sizeof(void *), "SRWLOCK is one pointer");
_Static_assert(sizeof(CONDITION_VARIABLE) == sizeof(void *), "CONDITION_VARIABLE is one pointer");

int clock_gettime(clockid_t clock, struct timespec *ts) {
    if (clock == CLOCK_MONOTONIC) {
        static LARGE_INTEGER freq;
        LARGE_INTEGER now;
        if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&now);
        ts->tv_sec = (time_t)(now.QuadPart / freq.QuadPart);
        ts->tv_nsec = (long)((now.QuadPart % freq.QuadPart) * 1000000000LL / freq.QuadPart);
        return 0;
    }
    return timespec_get(ts, TIME_UTC) ? 0 : -1;
}

unsigned sleep(unsigned seconds) { Sleep(seconds * 1000u); return 0; }
int usleep(unsigned long usec) { Sleep(usec >= 1000 ? (DWORD)(usec / 1000) : (usec ? 1 : 0)); return 0; }

int pthread_mutexattr_init(pthread_mutexattr_t *a) { a->type = PTHREAD_MUTEX_NORMAL; return 0; }
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type) { a->type = type; return 0; }
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) {
    memset(m, 0, sizeof *m);
    m->type = a ? a->type : PTHREAD_MUTEX_NORMAL;
    return 0;
}

/* Every mutex counts its owner, so a recursive one can be taken again by the thread holding it. */
int pthread_mutex_lock(pthread_mutex_t *m) {
    DWORD me = GetCurrentThreadId();
    if (m->type == PTHREAD_MUTEX_RECURSIVE && m->owner == me) { m->count++; return 0; }
    AcquireSRWLockExclusive((PSRWLOCK)&m->srw);
    m->owner = me;
    m->count = 1;
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *m) {
    DWORD me = GetCurrentThreadId();
    if (m->type == PTHREAD_MUTEX_RECURSIVE && m->owner == me) { m->count++; return 0; }
    if (!TryAcquireSRWLockExclusive((PSRWLOCK)&m->srw)) return EBUSY;
    m->owner = me;
    m->count = 1;
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *m) {
    if (--m->count > 0) return 0;
    m->owner = 0;
    ReleaseSRWLockExclusive((PSRWLOCK)&m->srw);
    return 0;
}

int pthread_condattr_init(pthread_condattr_t *a) { a->clock = CLOCK_REALTIME; return 0; }
int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t clock) { a->clock = clock; return 0; }
int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) {
    c->cv = NULL;
    c->clock = a ? a->clock : CLOCK_REALTIME;
    return 0;
}

static int cond_sleep(pthread_cond_t *c, pthread_mutex_t *m, DWORD ms) {
    int count = m->count;  /* a recursive mutex is released fully while waiting, then taken back as it was */
    m->owner = 0;
    m->count = 0;
    BOOL ok = SleepConditionVariableSRW((PCONDITION_VARIABLE)&c->cv, (PSRWLOCK)&m->srw, ms, 0);
    m->owner = GetCurrentThreadId();
    m->count = count;
    return ok ? 0 : ETIMEDOUT;
}

int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { return cond_sleep(c, m, INFINITE); }

int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *abstime) {
    struct timespec now;
    clock_gettime(c->clock, &now);
    long long ms = (long long)(abstime->tv_sec - now.tv_sec) * 1000 + (abstime->tv_nsec - now.tv_nsec) / 1000000;
    if (ms <= 0) return ETIMEDOUT;
    return cond_sleep(c, m, ms > 0x7FFFFFFF ? 0x7FFFFFFF : (DWORD)ms);
}

int pthread_cond_signal(pthread_cond_t *c) { WakeConditionVariable((PCONDITION_VARIABLE)&c->cv); return 0; }
int pthread_cond_broadcast(pthread_cond_t *c) { WakeAllConditionVariable((PCONDITION_VARIABLE)&c->cv); return 0; }

struct DIR { HANDLE h; WIN32_FIND_DATAA fd; int first; struct dirent ent; };

DIR *opendir(const char *path) {
    char pat[MAX_PATH + 4];
    snprintf(pat, sizeof pat, "%s\\*", *path ? path : ".");
    DIR *d = calloc(1, sizeof *d);
    d->h = FindFirstFileA(pat, &d->fd);
    if (d->h == INVALID_HANDLE_VALUE) { free(d); errno = ENOENT; return NULL; }
    d->first = 1;
    return d;
}

struct dirent *readdir(DIR *d) {
    if (!d->first && !FindNextFileA(d->h, &d->fd)) return NULL;
    d->first = 0;
    snprintf(d->ent.d_name, sizeof d->ent.d_name, "%s", d->fd.cFileName);
    return &d->ent;
}

int closedir(DIR *d) {
    if (!d) return -1;
    FindClose(d->h);
    free(d);
    return 0;
}
