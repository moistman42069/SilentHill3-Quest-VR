#ifndef _WIN32  /* SH3 Quest VR: Windows has its own layer, host_win.c */
/* What differs between Linux and the Switch: threads (and pausing them), the guest address window,
 * crash handlers and backtraces. */
#include "host.h"
#include "rt.h"
#include <pthread.h>
#ifdef __SWITCH__
#include <switch.h>
#include <malloc.h>
#include <stdalign.h>
#endif

struct HostThread {
    void (*fn)(void *);
    void *arg;
#ifdef __SWITCH__
    Thread thr;
    Mutex m;
    CondVar cv;
    int wakes, stopped;  /* resumes not yet used up; paused by another thread (svcSetThreadActivity) */
#else
    pthread_t pt;
    int wake[2];
#endif
};

static __thread HostThread *self;
HostThread *host_thread_self(void) { return self; }

#ifdef __SWITCH__
/* ======================================================================== Switch */

static void thread_entry(void *p) {
    self = p;
    self->fn(self->arg);
}

/* Guest threads run at 0x3B, the one priority Horizon time-slices, spread over the three cores the
 * application gets: the game spins on Sleep(0) and on its own flags, and at any other priority a
 * spinning thread keeps its core from everything at or below it. */
static int only_core = -1;

/* The game's CRI audio engine races when its threads run in parallel (the "sound loops forever" bug the
 * PC fix works around by pinning the game to one core): with a core given here, every guest thread,
 * the calling one included, is confined to it. */
void host_use_single_core(int core) {
    only_core = core;
    if (core >= 0) svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, 1u << core);
}

HostThread *host_thread_start(void (*fn)(void *), void *arg, size_t stack) {
    static int next_core;
    HostThread *t = calloc(1, sizeof *t);
    t->fn = fn;
    t->arg = arg;
    Result rc = threadCreate(&t->thr, thread_entry, t, NULL, stack, 0x3B, only_core >= 0 ? only_core : next_core++ % 3);
    if (R_FAILED(rc)) rt_fatal("threadCreate: %x", rc);
    threadStart(&t->thr);
    return t;
}

HostThread *host_thread_adopt(void) {
    self = calloc(1, sizeof *self);
    return self;
}

void host_thread_exit(void) { threadExit(); }

/* A resume may land before the pause it answers (SuspendThread counts under a lock, then pauses
 * outside it, and CRI's worker suspends itself while another thread resumes it): it is kept as a
 * wake that the pause then uses up instead of stopping. */
void host_thread_pause(HostThread *t) {
    mutexLock(&t->m);
    if (t == self) {  /* Horizon pauses other threads only: a thread suspending itself waits here */
        while (!t->wakes) condvarWait(&t->cv, &t->m);
        t->wakes--;
    } else if (t->wakes) t->wakes--;
    else if (t->thr.handle) {
        t->stopped = 1;
        svcSetThreadActivity(t->thr.handle, ThreadActivity_Paused);
    }
    mutexUnlock(&t->m);
}

void host_thread_resume(HostThread *t) {
    mutexLock(&t->m);
    if (t->stopped) {
        t->stopped = 0;
        svcSetThreadActivity(t->thr.handle, ThreadActivity_Runnable);
    } else {
        t->wakes++;
        condvarWakeOne(&t->cv);
    }
    mutexUnlock(&t->m);
}

/* ---- the guest window: 4 GB of address space, memory mapped in as the guest needs it ----
 * Heap memory is aliased as code memory (svcMapProcessCodeMemory), which may then be mapped at any
 * address of the window (svcMapProcessMemory); the same sequence the NFSMW port uses. */
#define WINDOW (1ull << 32)
#define CHUNK_ALIGN 0x200000u
static uint8_t *window;

static Handle proc(void) { return envGetOwnProcessHandle(); }

void *host_arena_reserve(void) {
    virtmemLock();
    window = virtmemFindAslr(WINDOW, CHUNK_ALIGN);
    if (window) virtmemAddReservation(window, WINDOW);
    virtmemUnlock();
    if (!window) rt_fatal("cannot reserve the guest window");
    return window;
}

void host_arena_commit(uint32_t va, uint32_t len) {
    uint32_t lo = va & ~0xFFFu, hi = (va + len + 0xFFFu) & ~0xFFFu;
    size_t n = hi - lo, align = (n >= CHUNK_ALIGN && !(lo & (CHUNK_ALIGN - 1))) ? CHUNK_ALIGN : 0x1000;
    void *backing = memalign(align, n);
    if (!backing) rt_fatal("out of memory committing %u KB of guest memory", (unsigned)(n >> 10));
    memset(backing, 0, n);
    virtmemLock();
    void *shadow = virtmemFindCodeMemory(n, align);
    Result rc = shadow ? svcMapProcessCodeMemory(proc(), (u64)shadow, (u64)backing, n) : MAKERESULT(Module_Libnx, 1);
    virtmemUnlock();
    if (R_SUCCEEDED(rc)) rc = svcSetProcessMemoryPermission(proc(), (u64)shadow, n, Perm_Rw);
    if (R_SUCCEEDED(rc)) rc = svcMapProcessMemory(window + lo, proc(), (u64)shadow, n);
    if (R_FAILED(rc)) rt_fatal("mapping guest memory %08X+%X failed: %x", lo, (unsigned)n, rc);
}

/* libnx calls this for any CPU exception in the process. The log gets pc/lr relative to main(), so
 * they can be looked up in the ELF (addr2line -e build/switch/sh3.elf) with main's link address. */
static void (*crash_cb)(uintptr_t);
alignas(16) u8 __nx_exception_stack[0x10000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);
extern int main(int, char **);

void __libnx_exception_handler(ThreadExceptionDump *ctx) {
    FILE *f = fopen("sh3-crash.log", "w");
    if (f) {
        fprintf(f, "exception %u: pc main+%llX lr main+%llX far %llX\n", ctx->error_desc,
                (unsigned long long)(ctx->pc.x - (u64)main), (unsigned long long)(ctx->lr.x - (u64)main),
                (unsigned long long)ctx->far.x);
        fclose(f);
    }
    if (crash_cb) crash_cb(ctx->far.x);  /* rt_fatal appends the guest view and exits */
}

void host_crash_handlers(void (*on_crash)(uintptr_t addr)) { crash_cb = on_crash; }
void host_backtrace(int fd) { (void)fd; }

#else
/* ======================================================================== Linux */
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#ifdef __ANDROID__
/* SH3 Quest VR: bionic has no execinfo before Android 13, so crash reports unwind with the compiler's unwinder and
 * name each frame with dladdr (library, symbol, offset: enough to find the line with the build's symbols). */
#include <unwind.h>
#include <dlfcn.h>
#include <ucontext.h>
static uintptr_t crash_pc, crash_lr, crash_sp;
#else
#include <execinfo.h>
#endif

/* SIGUSR2 parks a thread in this handler until host_thread_resume writes a byte to its pipe. A resume
 * may land before the pause it answers (see the Switch side): its byte is already there. */
static void on_pause(int sig) {
    (void)sig;
    char c;
    while (self && read(self->wake[0], &c, 1) < 0 && errno == EINTR) {}
}

static void alt_stack(void);
void host_use_single_core(int core) { (void)core; }

static void *thread_entry(void *p) {
    self = p;
    alt_stack();
    self->fn(self->arg);
    return NULL;
}

static HostThread *new_thread(void) {
    HostThread *t = calloc(1, sizeof *t);
    if (pipe(t->wake)) rt_fatal("pipe");
    static int installed;
    if (!installed++) {
        struct sigaction sa = {.sa_handler = on_pause, .sa_flags = SA_RESTART};
        sigaction(SIGUSR2, &sa, NULL);
    }
    return t;
}

HostThread *host_thread_start(void (*fn)(void *), void *arg, size_t stack) {
    HostThread *t = new_thread();
    t->fn = fn;
    t->arg = arg;
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, stack);
    if (pthread_create(&t->pt, &a, thread_entry, t)) rt_fatal("pthread_create failed");
    pthread_detach(t->pt);
    return t;
}

HostThread *host_thread_adopt(void) {
    self = new_thread();
    self->pt = pthread_self();
    return self;
}

void host_thread_exit(void) { pthread_exit(NULL); }

void host_thread_pause(HostThread *t) {
    if (t == self) on_pause(SIGUSR2);
    else pthread_kill(t->pt, SIGUSR2);
}

void host_thread_resume(HostThread *t) {
    if (write(t->wake[1], "", 1) < 0) rt_log("resume: wake failed");
}

void host_thread_signal(HostThread *t, int sig) { pthread_kill(t->pt, sig); }

void *host_arena_reserve(void) {
    void *p = mmap(NULL, 1ull << 32, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED) rt_fatal("cannot reserve the 4 GB guest arena");
    return p;
}

void host_arena_commit(uint32_t va, uint32_t len) {
    uintptr_t lo = va & ~0xFFFu, hi = ((uintptr_t)va + len + 0xFFF) & ~(uintptr_t)0xFFF;
    if (mprotect((uint8_t *)g_xbox_mem_offset + lo, hi - lo, PROT_READ | PROT_WRITE))
        rt_fatal("cannot commit guest memory %08X", va);
}

static void (*crash_cb)(uintptr_t);
static void on_signal(int sig, siginfo_t *si, void *uc) {
    (void)sig;
#if defined(__ANDROID__) && defined(__aarch64__)
    if (uc) {  /* where the fault was, for the crash report's first lines (host_backtrace) */
        const mcontext_t *m = &((ucontext_t *)uc)->uc_mcontext;
        crash_pc = m->pc, crash_lr = m->regs[30], crash_sp = m->sp;
    }
#else
    (void)uc;
#endif
    crash_cb((uintptr_t)si->si_addr);
}

/* Each thread gets an alternate signal stack, so a stack overflow still reaches the handler. */
static void alt_stack(void) {
    stack_t ss = {.ss_sp = malloc(1 << 16), .ss_size = 1 << 16};
    sigaltstack(&ss, NULL);
}

void host_crash_handlers(void (*on_crash)(uintptr_t addr)) {
    crash_cb = on_crash;
    alt_stack();
    struct sigaction sa = {.sa_sigaction = on_signal, .sa_flags = SA_SIGINFO | SA_ONSTACK};
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);  /* glibc's heap checks (double free...) end here */
}

#ifdef __ANDROID__
static void frame_line(int fd, const char *what, uintptr_t pc) {
    Dl_info di;
    if (pc && dladdr((void *)pc, &di) && di.dli_fname) {
        const char *lib = strrchr(di.dli_fname, '/');
        lib = lib ? lib + 1 : di.dli_fname;
        if (di.dli_sname)
            dprintf(fd, "  %s %016llx %s+%llx (%s+%llx)\n", what, (unsigned long long)pc, lib,
                    (unsigned long long)(pc - (uintptr_t)di.dli_fbase), di.dli_sname,
                    (unsigned long long)(pc - (uintptr_t)di.dli_saddr));
        else
            dprintf(fd, "  %s %016llx %s+%llx\n", what, (unsigned long long)pc, lib,
                    (unsigned long long)(pc - (uintptr_t)di.dli_fbase));
    } else
        dprintf(fd, "  %s %016llx\n", what, (unsigned long long)pc);
}

typedef struct { int fd, n; } Unwind;
static _Unwind_Reason_Code unwind_frame(struct _Unwind_Context *ctx, void *arg) {
    Unwind *u = arg;
    uintptr_t pc = _Unwind_GetIP(ctx);
    if (!pc) return _URC_END_OF_STACK;
    char what[16];
    snprintf(what, sizeof what, "#%02d", u->n);
    frame_line(u->fd, what, pc);
    return ++u->n >= 64 ? _URC_END_OF_STACK : _URC_NO_REASON;
}

void host_backtrace(int fd) {
    if (crash_pc) {
        dprintf(fd, "host fault: pc, lr (sp %016llx)\n", (unsigned long long)crash_sp);
        frame_line(fd, "pc ", crash_pc);
        frame_line(fd, "lr ", crash_lr);
    }
    dprintf(fd, "host stack:\n");
    Unwind u = {fd, 0};
    _Unwind_Backtrace(unwind_frame, &u);
}
#else
void host_backtrace(int fd) {
    void *bt[48];
    backtrace_symbols_fd(bt, backtrace(bt, 48), fd);
}
#endif
#endif
#endif  /* !_WIN32 */
