/* SH3 Quest VR: the platform layer for the Windows PC test build (host.h): threads and pausing them, the
 * 4 GB guest window, the crash handler and backtraces. Linux, Android and the Switch use host.c. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <io.h>
#include <stdio.h>
#include <time.h>
#undef WINAPI  /* rt.h has its own WINAPI(...) for bridges; windows.h's prototypes are already read */
#include "host.h"
#include "rt.h"

struct HostThread {
    void (*fn)(void *);
    void *arg;
    HANDLE h;
    SRWLOCK m;
    CONDITION_VARIABLE cv;
    int wakes, stopped;  /* resumes not yet used up; paused by another thread (SuspendThread) */
};

static __declspec(thread) HostThread *self;
HostThread *host_thread_self(void) { return self; }
void host_use_single_core(int core) { (void)core; }

static DWORD __stdcall thread_entry(void *p) {
    self = p;
    self->fn(self->arg);
    return 0;
}

HostThread *host_thread_start(void (*fn)(void *), void *arg, size_t stack) {
    HostThread *t = calloc(1, sizeof *t);
    t->fn = fn;
    t->arg = arg;
    InitializeSRWLock(&t->m);
    InitializeConditionVariable(&t->cv);
    t->h = CreateThread(NULL, stack, thread_entry, t, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (!t->h) rt_fatal("CreateThread failed: %lu", GetLastError());
    return t;
}

HostThread *host_thread_adopt(void) {
    self = calloc(1, sizeof *self);
    InitializeSRWLock(&self->m);
    InitializeConditionVariable(&self->cv);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &self->h, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    return self;
}

void host_thread_exit(void) { ExitThread(0); }

/* As on the Switch: a resume may land before the pause it answers (SuspendThread counts under a lock, then
 * pauses outside it, and CRI's worker suspends itself while another thread resumes it), so it is kept as a wake
 * that the pause then uses up instead of stopping. A thread pausing itself waits on its condition variable. */
void host_thread_pause(HostThread *t) {
    AcquireSRWLockExclusive(&t->m);
    if (t == self) {
        while (!t->wakes) SleepConditionVariableSRW(&t->cv, &t->m, INFINITE, 0);
        t->wakes--;
    } else if (t->wakes) t->wakes--;
    else if (t->h) {
        t->stopped = 1;
        SuspendThread(t->h);
    }
    ReleaseSRWLockExclusive(&t->m);
}

void host_thread_resume(HostThread *t) {
    AcquireSRWLockExclusive(&t->m);
    if (t->stopped) {
        t->stopped = 0;
        ResumeThread(t->h);
    } else {
        t->wakes++;
        WakeConditionVariable(&t->cv);
    }
    ReleaseSRWLockExclusive(&t->m);
}

void host_thread_signal(HostThread *t, int sig) { (void)t; (void)sig; }

void *host_arena_reserve(void) {
    void *p = VirtualAlloc(NULL, 1ull << 32, MEM_RESERVE, PAGE_NOACCESS);
    if (!p) rt_fatal("cannot reserve the 4 GB guest arena (error %lu)", GetLastError());
    return p;
}

void host_arena_commit(uint32_t va, uint32_t len) {
    uintptr_t lo = va & ~0xFFFu, hi = ((uintptr_t)va + len + 0xFFF) & ~(uintptr_t)0xFFF;
    if (!VirtualAlloc((uint8_t *)g_xbox_mem_offset + lo, hi - lo, MEM_COMMIT, PAGE_READWRITE))
        rt_fatal("cannot commit guest memory %08X+%X (error %lu)", va, (unsigned)(hi - lo), GetLastError());
}

static void (*crash_cb)(uintptr_t);

/* "function (file:line) module+offset" for a code address, from the build's .pdb next to sh3.exe. */
static void describe(const void *pc, char *out, size_t size) {
    static int sym_ready;
    if (!sym_ready) {
        sym_ready = 1;
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        SymInitialize(GetCurrentProcess(), NULL, TRUE);
    }
    HMODULE mod = NULL;
    char modpath[MAX_PATH] = "?";
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (const char *)pc, &mod);
    if (mod) GetModuleFileNameA(mod, modpath, sizeof modpath);
    const char *modname = strrchr(modpath, '\\') ? strrchr(modpath, '\\') + 1 : modpath;
    union { SYMBOL_INFO s; char buf[sizeof(SYMBOL_INFO) + 256]; } sym = {0};
    sym.s.SizeOfStruct = sizeof(SYMBOL_INFO);
    sym.s.MaxNameLen = 255;
    DWORD64 disp = 0;
    DWORD ldisp = 0;
    IMAGEHLP_LINE64 line = {sizeof line};
    int n = 0;
    if (SymFromAddr(GetCurrentProcess(), (DWORD64)(uintptr_t)pc, &disp, &sym.s)) {
        n = snprintf(out, size, "%s", sym.s.Name);
        if (SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)(uintptr_t)pc, &ldisp, &line)) {
            const char *file = strrchr(line.FileName, '\\') ? strrchr(line.FileName, '\\') + 1 : line.FileName;
            n += snprintf(out + n, size - n, " (%s:%lu)", file, line.LineNumber);
        }
        n += snprintf(out + n, size - n, " ");
    }
    snprintf(out + n, size - n, "%s+%llX", modname, (unsigned long long)((uintptr_t)pc - (uintptr_t)mod));
}

static const char *exception_name(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "access violation";
    case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
    case EXCEPTION_INT_OVERFLOW: return "integer overflow";
    case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
    case EXCEPTION_IN_PAGE_ERROR: return "in-page error";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "misaligned data";
    case 0xC0000409: return "fast fail (stack buffer overrun or abort)";
    case 0xC0000374: return "heap corruption";
    default: return NULL;
    }
}

/* Every exception that ends the process reaches the runtime's crash report (rt_fatal adds the guest's view). */
static LONG __stdcall on_exception(EXCEPTION_POINTERS *e) {
    DWORD code = e->ExceptionRecord->ExceptionCode;
    const char *name = exception_name(code);
    if (!name) return EXCEPTION_CONTINUE_SEARCH;  /* C++ exceptions, debugger events, ... */
    static volatile LONG busy;
    if (InterlockedExchange(&busy, 1)) { Sleep(INFINITE); }
    uintptr_t addr = code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR
                         ? (uintptr_t)e->ExceptionRecord->ExceptionInformation[1]
                         : (uintptr_t)e->ExceptionRecord->ExceptionAddress;
    HMODULE mod = NULL;
    char modname[MAX_PATH] = "?";
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (const char *)e->ExceptionRecord->ExceptionAddress, &mod);
    if (mod) GetModuleFileNameA(mod, modname, sizeof modname);
    char where[600];
    describe(e->ExceptionRecord->ExceptionAddress, where, sizeof where);
    rt_log("host exception %08lX (%s) at %p (%s+%llX), data address %p", code, name,
           e->ExceptionRecord->ExceptionAddress, modname,
           (unsigned long long)((uintptr_t)e->ExceptionRecord->ExceptionAddress - (uintptr_t)mod), (void *)addr);
    /* A fresh report for this crash (the runtime's guest view and the host frames are appended to it). */
    FILE *f = fopen("sh3-crash.log", "w");
    if (f) {
        time_t now = time(NULL);
        char when[64];
        strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(f, "SH3 Quest VR crash, %s\n%s (exception %08lX) in %s\n", when, name, code, where);
        if (code == EXCEPTION_ACCESS_VIOLATION)
            fprintf(f, "the code %s address %p\n",
                    e->ExceptionRecord->ExceptionInformation[0] == 1 ? "wrote to" :
                    e->ExceptionRecord->ExceptionInformation[0] == 8 ? "ran" : "read from", (void *)addr);
        fclose(f);
    }
    crash_cb(addr);
    return EXCEPTION_CONTINUE_SEARCH;
}

void host_crash_handlers(void (*on_crash)(uintptr_t addr)) {
    crash_cb = on_crash;
    AddVectoredExceptionHandler(0, on_exception);  /* last: the generated code's own handlers go first */
    SetUnhandledExceptionFilter(on_exception);
    ULONG guarantee = 64 * 1024;  /* room for the report after a stack overflow */
    SetThreadStackGuarantee(&guarantee);
}

/* Host frames named from the build's .pdb (function, file:line, module+offset); a return address is looked up
 * one byte back, inside the call. The recompiled game's functions show as sub_<guest address>. */
void host_backtrace(int fd) {
    void *bt[48];
    USHORT n = CaptureStackBackTrace(1, 48, bt, NULL);
    for (USHORT i = 0; i < n; i++) {
        char where[600], line[700];
        describe((const char *)bt[i] - 1, where, sizeof where);
        int len = snprintf(line, sizeof line, "  host #%u %s\n", i, where);
        _write(fd, line, len);
    }
}
#endif
