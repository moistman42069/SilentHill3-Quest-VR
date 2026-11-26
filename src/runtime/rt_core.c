/* Guest memory, the exe loader, the guest heap and call dispatch. */
#include "rt.h"
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include "host.h"
#include "exe_ref.h"  /* 0.1.4: the 2003-09-03 build: its imports, code range and data CRCs (tools/exe_ref.py) */
#ifdef __ANDROID__
#include <android/log.h>
#include <SDL2/SDL.h>  /* main is SDL_main there: SDL2's SDLActivity calls it */
#endif
#ifdef __SWITCH__
#include <switch.h>
#include <netinet/in.h>  /* __nxlink_host */
#endif

/* ---- register file and runtime globals the generated code links against ---- */
#ifndef RECOMP_MMX_DEFINED  /* recomp_types.h only declares it for generated code */
#define RECOMP_MMX_DEFINED
typedef union RecompMmx { int8_t b[8]; uint8_t ub[8]; int16_t w[4]; uint16_t uw[4];
                          int32_t d[2]; uint32_t ud[2]; uint64_t q; } RecompMmx;
#endif
ptrdiff_t g_xbox_mem_offset;
uint32_t g_xbox_code_lo, g_xbox_code_hi;
int g_force_return;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_ebp;
RECOMP_TLS uint32_t g_fs_base, g_seh_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_control_word = 0x027F, g_fp_cc;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3, g_xmm4, g_xmm5, g_xmm6, g_xmm7;
RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3, g_mm4, g_mm5, g_mm6, g_mm7;
RECOMP_TLS volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE];  /* per thread: see recomp_types.h */
RECOMP_TLS volatile uint32_t g_icall_trace_idx;
RECOMP_TLS volatile uint64_t g_icall_count;

int rt_trace;
char rt_game_dir[512] = ".";

#ifdef __SWITCH__
/* Off unless launched by nxlink or asked for with log=1 in sh3.ini: writing sh3.log costs an SD card write
 * per line (crashes still go to sh3-crash.log). */
static int log_on;
#endif

void rt_log(const char *fmt, ...) {
#ifdef __SWITCH__
    if (!log_on) return;
#endif
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
#ifdef __ANDROID__
    va_start(ap, fmt);  /* SH3 Quest VR: logcat too */
    __android_log_vprint(ANDROID_LOG_INFO, "SH3QuestVR", fmt, ap);
    va_end(ap);
#endif
#if defined(__SWITCH__) || defined(__ANDROID__)
    /* No console on the Switch or the Quest: sh3.log in the game folder, always on the Quest (the launcher shows it
     * and sends it with a report; the last run's is kept as sh3-previous.log). */
    static FILE *log;
    static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&log_lock);
    if (!log) {
#ifdef __ANDROID__
        remove("sh3-previous.log");
        rename("sh3.log", "sh3-previous.log");
#endif
        log = fopen("sh3.log", "w");
    }
    if (log) {
        va_start(ap, fmt);
        vfprintf(log, fmt, ap);
        va_end(ap);
        fputc('\n', log);
        fflush(log);
    }
    pthread_mutex_unlock(&log_lock);
#endif
}

static void dump_icall_trace(FILE *f) {
    fprintf(f, "last indirect calls:");
    for (int i = 0; i < ICALL_TRACE_SIZE; i++)
        fprintf(f, " %08X", g_icall_trace[(g_icall_trace_idx + i) & (ICALL_TRACE_SIZE - 1)]);
    fprintf(f, "\nregs: eax=%08X ecx=%08X edx=%08X ebx=%08X esi=%08X edi=%08X esp=%08X\n",
            g_eax, g_ecx, g_edx, g_ebx, g_esi, g_edi, g_esp);
}

/* Guest return addresses on the guest stack: the nearest thing to a guest backtrace. */
static void dump_guest_stack(FILE *f) {
    fprintf(f, "guest stack code addresses:");
    for (uint32_t a = g_esp, n = 0; n < 24 && a < g_esp + 0x2000; a += 4) {
        uint32_t v = MEM32(a);
        if (v >= g_xbox_code_lo && v < g_xbox_code_hi) { fprintf(f, " %08X", v); n++; }
    }
    fputc('\n', f);
}

/* Where this thread is, guest and host side: for hangs (SH3_DUMP_AFTER) and fatal errors. */
void rt_dump_thread(void) {
    dump_icall_trace(stderr);
    dump_guest_stack(stderr);
    host_backtrace(2);
}

/* Fatal errors go to stderr and to sh3-crash.log in the game folder (the Switch has no console). */
void game_crash_info(FILE *f);  /* src/game/sh3.c */
_Noreturn void rt_fatal(const char *fmt, ...) {
    /* A fault while this thread writes the report: what is written stays, and the run ends. Another thread's fatal
     * error waits for the first report (which ends the run). */
    static _Thread_local int nested;
    static pthread_mutex_t one_report = PTHREAD_MUTEX_INITIALIZER;
    if (nested++) _exit(2);
    pthread_mutex_lock(&one_report);
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_FATAL, "SH3QuestVR", "FATAL: %s (details in sh3-crash.log)", msg);
#endif
    FILE *log = fopen("sh3-crash.log", "a");  /* appends to what an exception handler wrote first */
    FILE *outs[2] = {stderr, log};
    for (int i = 0; i < 2 && outs[i]; i++) {
        fprintf(outs[i], "FATAL: %s\n", msg);
        game_crash_info(outs[i]);  /* src/game/sh3.c: frame, mode, Heather, VR */
        dump_icall_trace(outs[i]);
        dump_guest_stack(outs[i]);
        fflush(outs[i]);
        host_backtrace(fileno(outs[i]));
    }
    if (log) fclose(log);
    _exit(1);
}

/* The game quits (ExitProcess, TerminateProcess, the headset ending the session). exit() would run the exit-time
 * teardown (C++ destructors, the OpenXR loader, the C library's locks) while the game's sound thread still runs: on
 * the Quest that thread then locks a destroyed mutex and the quit ends in a crash ("FORTIFY: pthread_mutex_lock called
 * on a destroyed mutex", owner 86's logs, every quit since 0.1.0). So every open file (the saves, the logs) is flushed
 * and the process ends at once; the system closes the rest. */
_Noreturn void rt_leave(int code) {
    rt_log("leaving (code %d): open files flushed, the process ends without the exit-time teardown", code);
    fflush(NULL);
    _exit(code);
}

static void on_crash(uintptr_t a) {
    uintptr_t base = (uintptr_t)g_xbox_mem_offset;
    if (a >= base && a - base < 0x100000000ull) rt_fatal("crash at guest address %08X", (uint32_t)(a - base));
    rt_fatal("crash at host address %p", (void *)a);
}

/* ---- guest address space: one 4 GB arena, guest VA = offset into it ---- */
#define GUEST_LOW_GUARD 0x10000u     /* null pointer reads fault */
#define HEAP_LO 0x10000000u
#define HEAP_HI 0xF0000000u
#define HOSTFN_BASE 0xFE000000u      /* synthetic call targets, never dereferenced */
#define HOSTFN_STRIDE 16u

/* Only the exe image and the heap are backed; anything else the guest touches faults, on Linux
 * as on the Switch. */
static void arena_init(void) { g_xbox_mem_offset = (ptrdiff_t)host_arena_reserve(); }
static uint32_t image_lo, image_hi;  /* the loaded exe */

/* ---- guest heap ----
 * ponytail: power-of-two size classes with per-class free lists, never returned to the
 * system. Wastes up to half of each block; swap for a real allocator if Switch RAM runs out. */
typedef struct { uint32_t cls, req, magic, pad; } BlockHdr;
#define BLOCK_MAGIC 0x53483248u
static uint32_t heap_top = HEAP_LO, heap_committed = HEAP_LO, free_head[33];
#define HEAP_COMMIT_STEP (8u << 20)
static pthread_mutex_t heap_mx = PTHREAD_MUTEX_INITIALIZER;

uint32_t galloc(uint32_t size) {
    uint32_t cls = 5;
    while ((1ull << cls) < (uint64_t)size + sizeof(BlockHdr)) cls++;
    pthread_mutex_lock(&heap_mx);
    uint32_t blk = free_head[cls];
    if (blk) free_head[cls] = ((BlockHdr *)GPTR(blk))->pad;  /* the free list links through the header */
    else {
        if ((uint64_t)heap_top + (1ull << cls) > HEAP_HI) rt_fatal("guest heap exhausted (%u bytes)", size);
        blk = heap_top;
        heap_top += 1u << cls;
        if (heap_top > heap_committed) {
            uint32_t to = (heap_top + HEAP_COMMIT_STEP - 1) & ~(HEAP_COMMIT_STEP - 1);
            host_arena_commit(heap_committed, to - heap_committed);
            heap_committed = to;
        }
    }
    pthread_mutex_unlock(&heap_mx);
    BlockHdr *h = GPTR(blk);
    *h = (BlockHdr){cls, size, BLOCK_MAGIC, 0};
    memset(h + 1, 0, size);
    return blk + sizeof(BlockHdr);
}

static BlockHdr *hdr(uint32_t va) {
    BlockHdr *h = GPTR(va - sizeof(BlockHdr));
    if (va < HEAP_LO || h->magic != BLOCK_MAGIC) rt_fatal("bad guest heap pointer %08X", va);
    return h;
}

/* Freed blocks wait in a quarantine before they are reused, and their contents stay intact: the game
 * (written for Windows' heap, which does not hand a block back at once) still reads a block just after
 * freeing it, and an immediate LIFO reuse turned that into a crash at some resolutions (the screen-sized
 * image surface and the Konami logo texture share a size class at 1280x720). */
#define QUARANTINE_BYTES (64u << 20)
#define QUARANTINE_SLOTS 4096
static uint32_t quarantine[QUARANTINE_SLOTS], q_head, q_count, q_bytes;

static void release_block(uint32_t blk) {
    BlockHdr *h = GPTR(blk);
    h->pad = free_head[h->cls];
    free_head[h->cls] = blk;
}

void gfree(uint32_t va) {
    if (!va) return;
    BlockHdr *h = hdr(va);
    pthread_mutex_lock(&heap_mx);
    h->magic = 0;
    if (q_count == QUARANTINE_SLOTS || q_bytes + (1u << h->cls) > QUARANTINE_BYTES) {
        while (q_count && (q_count == QUARANTINE_SLOTS || q_bytes + (1u << h->cls) > QUARANTINE_BYTES)) {
            uint32_t old = quarantine[q_head];
            q_head = (q_head + 1) % QUARANTINE_SLOTS;
            q_count--;
            q_bytes -= 1u << ((BlockHdr *)GPTR(old))->cls;
            release_block(old);
        }
    }
    if (q_bytes + (1u << h->cls) > QUARANTINE_BYTES) release_block(va - sizeof(BlockHdr));  /* bigger than the quarantine */
    else {
        quarantine[(q_head + q_count++) % QUARANTINE_SLOTS] = va - sizeof(BlockHdr);
        q_bytes += 1u << h->cls;
    }
    pthread_mutex_unlock(&heap_mx);
}

uint32_t gsize(uint32_t va) { return hdr(va)->req; }
uint32_t rt_heap_used(void) { return heap_top - HEAP_LO; }

uint32_t grealloc(uint32_t va, uint32_t size) {
    if (!va) return galloc(size);
    BlockHdr *h = hdr(va);
    if ((uint64_t)size + sizeof(BlockHdr) <= (1ull << h->cls)) {
        if (size > h->req) memset((uint8_t *)GPTR(va) + h->req, 0, size - h->req);
        h->req = size;
        return va;
    }
    uint32_t n = galloc(size);
    memcpy(GPTR(n), GPTR(va), h->req);
    gfree(va);
    return n;
}

uint32_t gstrdup(const char *s) {
    uint32_t va = galloc(strlen(s) + 1);
    strcpy(GPTR(va), s);
    return va;
}

/* ---- host functions callable from the guest ---- */
typedef struct { const char *name; void (*fn)(void); } HostFn;
static HostFn hostfn[8192];
static uint32_t n_hostfn;

uint32_t rt_host_fn(const char *name, void (*fn)(void)) {
    if (n_hostfn == sizeof hostfn / sizeof *hostfn) rt_fatal("host function table full");
    hostfn[n_hostfn] = (HostFn){name, fn};
    return HOSTFN_BASE + HOSTFN_STRIDE * n_hostfn++;
}

uint32_t com_vtable(const ComEntry *e, int n) {
    uint32_t vt = galloc(4 * n);
    for (int i = 0; i < n; i++) MEM32(vt + 4 * i) = rt_host_fn(e[i].name, e[i].fn);
    return vt;
}

uint32_t com_new(uint32_t vtable, uint32_t size) {
    uint32_t o = galloc(size);
    MEM32(o) = vtable;
    return o;
}

static void trace_and_call(uint32_t i) {
    rt_log("[%x] %s(%08X, %08X, %08X, %08X) <- %08X", MEM32(g_fs_base + 0x24), hostfn[i].name,
           MEM32(g_esp + 4), MEM32(g_esp + 8), MEM32(g_esp + 12), MEM32(g_esp + 16), MEM32(g_esp));
}

/* ---- oracle snapshots (SH3_ORACLE=<hex va>[,<nth call>]): with the function built with
 * --trace-functions, its entry and exit state go to oracle_in.snap / oracle_out.snap, together with
 * what every bridged call returned in between. tools/oracle.py replays it on the original x86. ---- */
static uint32_t oracle_va, oracle_nth;
static __thread int oracle_depth, oracle_armed;
static __thread recomp_func_t oracle_pending;
static __thread FILE *oracle_calls;

static void snap_write(const char *path) {
    FILE *f = fopen(path, "wb");
    uint32_t regs[10] = {g_eax, g_ecx, g_edx, g_ebx, g_esp, g_seh_ebp, g_esi, g_edi, g_fs_base, (uint32_t)g_fp_top};
    fwrite(regs, 4, 10, f);
    fwrite(g_fp_stack, 8, 8, f);
    uint32_t ranges[3][2] = {{image_lo, image_hi}, {HEAP_LO, heap_top}, {0, 0}};
    for (int r = 0; ranges[r][1]; r++) {
        uint32_t lo = ranges[r][0], n = ranges[r][1] - lo;
        fwrite(&lo, 4, 1, f);
        fwrite(&n, 4, 1, f);
        fwrite(GPTR(lo), 1, n, f);
    }
    fclose(f);
}

/* SH3_COVER=frame (trace build): logs each function the first time it runs at or after that frame, with
 * the guest stack, to find what code an in-game event runs. SH3_COVER=-frame logs every function that runs
 * from that frame on, once, whether or not it ran before: what the game is doing in a given state. */
int rt_frame;
static int cover_from = -1, cover_all;
static uint8_t *cover;

void recomp_trace_enter(const char *name, uint32_t va) {
    (void)name;
    if (cover && va - g_xbox_code_lo < g_xbox_code_hi - g_xbox_code_lo && (!cover_all || rt_frame >= cover_from)) {
        uint32_t i = va - g_xbox_code_lo;
        if (!(cover[i >> 3] & (1 << (i & 7)))) {
            cover[i >> 3] |= 1 << (i & 7);
            if (rt_frame >= cover_from) { rt_log("cover frame %d: %08X", rt_frame, va); dump_guest_stack(stderr); }
        }
    }
    if (va != oracle_va) return;
    if (oracle_armed) { oracle_depth++; return; }
    if (oracle_nth--) return;
    rt_log("oracle: entering %08X, writing oracle_in.snap", va);
    snap_write("oracle_in.snap");
    oracle_calls = fopen("oracle_calls.txt", "w");
    oracle_armed = 1;
}

void recomp_trace_exit(const char *name, uint32_t va) {
    (void)name;
    if (va != oracle_va || !oracle_armed) return;
    if (oracle_depth) { oracle_depth--; return; }
    snap_write("oracle_out.snap");
    fclose(oracle_calls);
    rt_log("oracle: wrote oracle_out.snap, stopping");
    _exit(0);
}

void recomp_trace_esp(const char *name, const char *tag) { (void)name; (void)tag; }

static void oracle_record(void) {
    recomp_func_t fn = oracle_pending;
    uint32_t target = MEM32(g_esp);  /* the return address identifies the call site */
    fn();
    fprintf(oracle_calls, "%08X %08X %08X\n", target, g_eax, g_edx);
}

recomp_func_t recomp_lookup_kernel(uint32_t va) {
    uint32_t i = (va - HOSTFN_BASE) / HOSTFN_STRIDE;
    if (va < HOSTFN_BASE || i >= n_hostfn) return NULL;
    if (!hostfn[i].fn) rt_fatal("unimplemented import %s", hostfn[i].name);
    if (rt_trace) trace_and_call(i);
    if (oracle_armed) { oracle_pending = hostfn[i].fn; return oracle_record; }
    return hostfn[i].fn;
}


recomp_func_t rt_resolve(uint32_t va) {
    recomp_func_t fn = recomp_lookup_manual(va);
    if (!fn) fn = recomp_lookup(va);
    if (!fn) fn = recomp_lookup_kernel(va);
    if (!fn) rt_fatal("no code at guest address %08X", va);
    return fn;
}

uint32_t guest_call(uint32_t va, int nargs, const uint32_t *args, int stdcall) {
    recomp_func_t fn = rt_resolve(va);
    for (int i = nargs - 1; i >= 0; i--) PUSH32(g_esp, args[i]);
    PUSH32(g_esp, 0xFFFFFFF0u);  /* return address: guest code may read it, never jumps to it */
    fn();
    if (!stdcall) g_esp += 4 * nargs;
    return g_eax;
}

/* ---- non-local jumps: each guest jmp_buf is paired with a native one taken at the setjmp call
 * site (see recomp_types.h); a guest longjmp restores registers and jumps natively. ---- */
#define JMP_SLOTS 32
static __thread struct { uint32_t buf_va; jmp_buf native; } jmp_slot[JMP_SLOTS];
static __thread int jmp_used;

jmp_buf *recomp_setjmp_slot(uint32_t buf_va) {
    for (int i = 0; i < jmp_used; i++)
        if (jmp_slot[i].buf_va == buf_va) { jmp_used = i + 1; return &jmp_slot[i].native; }
    if (jmp_used == JMP_SLOTS) jmp_used--;
    jmp_slot[jmp_used].buf_va = buf_va;
    return &jmp_slot[jmp_used++].native;
}

int recomp_guest_longjmp(uint32_t buf_va, uint32_t value) {
    for (int i = jmp_used - 1; i >= 0; i--) {
        if (jmp_slot[i].buf_va != buf_va) continue;
        g_ebx = MEM32(buf_va + 4);
        g_edi = MEM32(buf_va + 8);
        g_esi = MEM32(buf_va + 12);
        g_esp = MEM32(buf_va + 16) + 4;
        g_seh_ebp = g_ebp = MEM32(buf_va);
        MEM32(g_fs_base) = MEM32(buf_va + 24);  /* SEH chain as it was at setjmp */
        jmp_used = i + 1;
        longjmp(jmp_slot[i].native, value ? (int)value : 1);
    }
    return 0;
}

/* -DRECOMP_ABI_CHECK: a call that came back with ebx/esi/edi changed or esp unpopped. */
void recomp_abi_violation_log(uint32_t va, uint32_t ebx0, uint32_t esi0, uint32_t edi0, uint32_t esp0) {
    static uint32_t seen[256];
    for (int i = 0; i < 256; i++) {
        if (seen[i] == va) return;
        if (!seen[i]) { seen[i] = va; break; }
    }
    rt_log("ABI: call to %08X changed ebx %08X->%08X esi %08X->%08X edi %08X->%08X esp %08X->%08X",
           va, ebx0, g_ebx, esi0, g_esi, edi0, g_edi, esp0, g_esp);
}

void recomp_icall_fail_log(uint32_t va) { rt_fatal("indirect call to untranslated address %08X", va); }
void recomp_icall_not_code_log(uint32_t va) { rt_fatal("indirect call to non-code address %08X", va); }

void recomp_unimpl(const char *text, uint32_t va) {
    static int n;
    if (n++ < 64) rt_log("unimplemented instruction %s at %08X", text, va);
}

/* The guest sees a Pentium III without MMX/SSE/3DNow!, so it takes its plain x87 paths. */
void recomp_cpuid(void) {
    uint32_t leaf = g_eax;
    g_eax = g_ebx = g_ecx = g_edx = 0;
    if (leaf == 0) { g_eax = 1; g_ebx = 0x756E6547; g_edx = 0x49656E69; g_ecx = 0x6C65746E; }  /* GenuineIntel */
    else if (leaf == 1) { g_eax = 0x683; g_edx = 0x1 | 0x10 | 0x100 | 0x8000; }            /* FPU TSC CX8 CMOV */
    else if (leaf == 0x80000000u) g_eax = 0x80000000u;                                       /* no extended leaves */
}

/* ---- loading sh3.exe at its own addresses ---- */
static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

#ifdef _WIN32  /* SH3 Quest VR: the section's ends, placed before and after every $m entry by the linker */
__attribute__((used, section("game_bridges$a"), aligned(8))) const Bridge rt_bridges_first = {0};
__attribute__((used, section("game_bridges$z"), aligned(8))) const Bridge rt_bridges_last = {0};
#endif

static recomp_func_t find_bridge(const char *name) {
    for (const Bridge *b = __start_game_bridges; b < __stop_game_bridges; b++)
        if (b->name && !strcmp(b->name, name)) return b->fn;  /* the PE linker may pad with zeros */
    return NULL;
}

/* SH3 Quest VR 0.1.4: how the loaded exe's data compares with the 2003-09-03 build's, page by page (exe_ref.h's
 * CRC-32s, the import tables as zeros), for the log: the launcher has already checked it (ExeCheck.java). */
static uint32_t crc32_update(uint32_t crc, uint8_t byte) {
    static uint32_t table[256];
    if (!table[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    return table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
}

static void exe_data_check(void) {
    const unsigned n_pages = sizeof exe_ref_pages / sizeof *exe_ref_pages;
    unsigned changed = 0;
    uint32_t first = 0;
    for (unsigned p = 0; p < n_pages; p++) {
        uint32_t crc = 0xFFFFFFFFu, va = exe_ref_pages[p].va;
        for (uint32_t a = va; a < va + exe_ref_pages[p].len; a++) {
            uint8_t b = MEM8(a);
            for (unsigned s = 0; s < sizeof exe_ref_skips / sizeof *exe_ref_skips; s++)
                if (a >= exe_ref_skips[s].va && a < exe_ref_skips[s].va + exe_ref_skips[s].size) b = 0;
            crc = crc32_update(crc, b);
        }
        if ((crc ^ 0xFFFFFFFFu) != exe_ref_pages[p].crc && !changed++) first = va;
    }
    if (!changed)
        rt_log("sh3.exe: the 2003-09-03 build's data, all %u pages the same", n_pages);
    else
        rt_log("sh3.exe: %u of the 2003-09-03 build's %u data pages differ (the first at %08X): %s", changed, n_pages,
               first, changed <= 16 ? "a patch to the game's data (a fix): it should still play"
                                    : "another build, or one still encrypted by a disc's copy protection: the port's "
                                      "code expects the 2003-09-03 build's data");
}

/* 0.1.4: the recompiled code still reads data from the code section: MSVC's switch tables (and their byte index
 * tables), and the libraries' data linked into it (run vr260: DirectInput's joystick format, read through the pointer
 * the game gives SetDataFormat). A disc's copy protection encrypts that section, so the build's data there
 * (src/recomp/gen/exe_code_data.c, written by tools/exe_ref.py from the exe the code was recompiled from: every byte
 * outside the functions found, not padding) is put in place whatever the file has. */
extern const uint32_t exe_code_ranges[][3];  /* va, length, offset in exe_code_bytes */
extern const unsigned exe_code_range_count;
extern const uint8_t exe_code_bytes[];

static void exe_code_tables(const char *path) {
    unsigned differ = 0, bytes = 0;
    uint32_t first = 0;
    for (unsigned i = 0; i < exe_code_range_count; i++) {
        uint32_t va = exe_code_ranges[i][0], len = exe_code_ranges[i][1];
        const uint8_t *want = exe_code_bytes + exe_code_ranges[i][2];
        bytes += len;
        if (memcmp(GPTR(va), want, len)) {
            if (!differ++) first = va;
            memcpy(GPTR(va), want, len);
        }
    }
    if (!differ)
        rt_log("%s: the data in its code section (%u stretches, %u bytes: switch tables and the like) is the build's",
               path, exe_code_range_count, bytes);
    else
        rt_log("%s: %u of the %u stretches of data in its code section differ from the build's (the first at %08X: code "
               "encrypted by a disc's copy protection, or patched): the build's are used", path, differ,
               exe_code_range_count, first);
}

static uint32_t load_exe(const char *path) {
    /* 0.1.4 (owner 93): its name as the folder spells it (SH3.EXE from some installs; the headset's storage minds case,
     * the launcher's check does not), as every file the game opens (kernel32.c rt_path). */
    char spelled[512];
    FILE *f = fopen(rt_path(path, spelled, sizeof spelled), "rb");
    if (!f) rt_fatal("cannot open %s/%s", rt_game_dir, path);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    uint8_t *d = malloc(n);
    fseek(f, 0, SEEK_SET);
    if (fread(d, 1, n, f) != (size_t)n) rt_fatal("cannot read %s", path);
    fclose(f);

    if (n < 0x400 || rd32(d + 0x3C) > (uint32_t)n - 0x200) rt_fatal("%s/%s is not a Windows program", rt_game_dir, path);
    const uint8_t *pe = d + rd32(d + 0x3C), *opt = pe + 24;
    uint32_t base = rd32(opt + 28), entry = base + rd32(opt + 16), stamp = rd32(pe + 8);
    const int build = base == EXE_REF_BASE;  /* 0.1.4: the 2003-09-03 build's addresses (the launcher checks the rest) */
    image_lo = base;
    image_hi = base + rd32(opt + 56);  /* SizeOfImage */
    if (build && image_hi < base + EXE_REF_SIZE_OF_IMAGE) image_hi = base + EXE_REF_SIZE_OF_IMAGE;
    host_arena_commit(base, image_hi - base);
    memcpy(GPTR(base), d, n < 0x1000 ? (size_t)n : 0x1000);  /* PE headers: the CRT checks its own MZ/PE signature */
    unsigned nsec = pe[6] | pe[7] << 8;
    const uint8_t *sec = opt + (pe[20] | pe[21] << 8);
    char names[160] = "";
    for (unsigned i = 0; i < nsec && sec + 40 <= d + n; i++, sec += 40) {
        uint32_t vs = rd32(sec + 8), va = base + rd32(sec + 12), rs = rd32(sec + 16), ro = rd32(sec + 20);
        uint32_t len = rs < vs ? rs : vs;
        if (ro > (uint32_t)n) len = 0;
        else if (len > (uint32_t)n - ro) len = (uint32_t)n - ro;   /* a file cut short: the rest stays zero */
        if (va < image_lo || va > image_hi || len > image_hi - va) len = 0;
        if (len) memcpy(GPTR(va), d + ro, len);
        if (!memcmp(sec, ".text", 6)) { g_xbox_code_lo = va; g_xbox_code_hi = va + vs; }
        snprintf(names + strlen(names), sizeof names - strlen(names), "%s%.8s", i ? " " : "", (const char *)sec);
    }
    free(d);
    if (build) { g_xbox_code_lo = EXE_REF_CODE_VA; g_xbox_code_hi = EXE_REF_CODE_VA + EXE_REF_CODE_SIZE; }
    rt_log("%s: %ld bytes, PE timestamp %08X (%s), sections %s", path, n, stamp,
           stamp == EXE_REF_TIMESTAMP ? "the 2003-09-03 build's" : "not the 2003-09-03 build's: a protection or a patch "
           "can change it", names);
    if (build) {
        exe_data_check();
        exe_code_tables(path);
    }

    /* Point every IAT slot at its bridge. */
    unsigned missing = 0;
    if (build) {
        /* SH3 Quest VR 0.1.4 (owners 90-91: "it keeps saying they have the wrong version installed", "try to extend
         * compatibility however you can"): the port runs its own recompiled code, so any sh3.exe of the 2003-09-03
         * build will do whatever its code is (a disc's copy protection, a no-CD/no-DVD patch, a fix). Its import slots
         * are filled from the build's own list (exe_ref.h), as a protection rewrites the file's import table; the
         * file's own table is only read to say how it compares. */
        unsigned own = 0, same = 0;
        const unsigned n_ref = sizeof exe_ref_imports / sizeof *exe_ref_imports;
        uint32_t desc = base + MEM32(base + MEM32(base + 0x3C) + 24 + 104);
        for (int k = 0; k < 64 && desc >= image_lo && desc + 20 <= image_hi && MEM32(desc + 12); k++, desc += 20) {
            uint32_t dll = base + MEM32(desc + 12);
            uint32_t ilt = base + (MEM32(desc) ? MEM32(desc) : MEM32(desc + 16)), iat = base + MEM32(desc + 16);
            if (dll < image_lo || dll >= image_hi - 64) break;
            for (int j = 0; j < 1024 && ilt >= image_lo && ilt + 4 <= image_hi && MEM32(ilt); j++, ilt += 4, iat += 4) {
                uint32_t e = MEM32(ilt);
                char name[128];
                if (e & 0x80000000u) {
                    snprintf(name, sizeof name, "%.*s#%u", (int)strcspn(GSTR(dll), "."), GSTR(dll), e & 0xFFFF);
                } else if (base + e + 2 >= image_lo && base + e + 2 < image_hi - 64) {
                    snprintf(name, sizeof name, "%.100s", GSTR(base + e + 2));
                } else {
                    break;
                }
                own++;
                for (unsigned r = 0; r < n_ref; r++)
                    if (exe_ref_imports[r].slot == iat && !strcmp(exe_ref_imports[r].name, name)) { same++; break; }
            }
        }
        for (unsigned r = 0; r < n_ref; r++) {
            recomp_func_t fn = find_bridge(exe_ref_imports[r].name);
            if (!fn) { missing++; if (rt_trace) rt_log("no bridge for %s", exe_ref_imports[r].name); }
            MEM32(exe_ref_imports[r].slot) = rt_host_fn(exe_ref_imports[r].name, fn);
        }
        if (same == n_ref && own == n_ref)
            rt_log("%s: its import table is the build's (%u functions)", path, n_ref);
        else
            rt_log("%s: its own import table has %u of the build's %u functions in their places (%u listed): a disc's "
                   "copy protection or a rebuilt exe; the %u slots filled from the build's own list", path, same, n_ref,
                   own, n_ref);
    } else {
        rt_log("%s: not the 2003-09-03 build's addresses (its base %08X, the build's %08X): the port's code expects that "
               "build (the launcher's game check tells which copies work)", path, base, EXE_REF_BASE);
        for (uint32_t desc = base + MEM32(base + MEM32(base + 0x3C) + 24 + 104); MEM32(desc + 12); desc += 20) {
            const char *dll = GSTR(base + MEM32(desc + 12));
            uint32_t ilt = base + (MEM32(desc) ? MEM32(desc) : MEM32(desc + 16)), iat = base + MEM32(desc + 16);
            for (; MEM32(ilt); ilt += 4, iat += 4) {
                uint32_t e = MEM32(ilt);
                char name[128];
                if (e & 0x80000000u) snprintf(name, sizeof name, "%.*s#%u", (int)strcspn(dll, "."), dll, e & 0xFFFF);
                else snprintf(name, sizeof name, "%s", GSTR(base + e + 2));
                recomp_func_t fn = find_bridge(name);
                if (!fn) { missing++; if (rt_trace) rt_log("no bridge for %s!%s", dll, name); }
                MEM32(iat) = rt_host_fn(strdup(name), fn);
            }
        }
    }
    rt_log("loaded %s: entry %08X, %u imports without a bridge", path, entry, missing);
    return entry;
}

extern void xbe_entry_point(void);
void game_init(void);  /* src/game/sh3.c */
void launcher_run(void);  /* src/d3d8/launcher.c */

/* sh3.ini in the game folder: resolution=WxH (default 1280x720), cores=1 to keep the guest threads on one
 * core, trace=1 to log every bridged call, log=1 to write sh3.log on the Switch. */
uint32_t rt_res_w = 1280, rt_res_h = 720;
static void read_ini(void) {
    FILE *ini = fopen("sh3.ini", "r");
    char line[128];
    unsigned a, b, cores = 3;
    while (ini && fgets(line, sizeof line, ini)) {
        if (sscanf(line, " resolution = %ux%u", &a, &b) == 2 && a >= 320 && b >= 240 && a <= 1920 && b <= 1440)
            rt_res_w = a, rt_res_h = b;
        if (sscanf(line, " cores = %u", &a) == 1) cores = a;
        if (sscanf(line, " trace = %u", &a) == 1) rt_trace = a;
#ifdef __SWITCH__
        if (sscanf(line, " log = %u", &a) == 1) log_on = a;
#endif
    }
    if (ini) fclose(ini);
    host_use_single_core(cores >= 3 ? -1 : 0);
}

/* Once the launcher is done (src/d3d8/launcher.c): its settings, the exe and the game's own setup. */
void rt_game_setup(void) {
    read_ini();
    load_exe("sh3.exe");
    game_init();
    if (cover_from >= 0) cover = calloc((g_xbox_code_hi - g_xbox_code_lo) / 8 + 1, 1);
}

/* The game runs on a thread of its own: lifted code recurses as deep as the guest and the main
 * thread's stack is not ours to size on every platform. */
static void game_thread(void *unused) {
    (void)unused;
    rt_thread_init_main(1u << 20);
    launcher_run();
    xbe_entry_point();
    /* 0.1.5: the game's own loop ended (its window closed: the headset's app closed, SDL's quit as WM_QUIT): left as
     * ExitProcess is (rt_leave), never through exit(), whose teardown the sound thread outlives (the FORTIFY "mutex
     * locked after it was destroyed" crash in the 0.1.3 logs). */
    rt_log("entry point returned");
    rt_leave(0);
}

int main(int argc, char **argv) {
#ifdef __SWITCH__
    const char *dir = "sdmc:/switch/sh3-nx";
    (void)argc; (void)argv;
    if (__nxlink_host.s_addr) {  /* launched by nxlink: stdout/stderr and the log go back to the PC */
        socketInitializeDefault();
        if (nxlinkStdio() >= 0) log_on = 1;
        else socketExit();
    }
#elif defined(__ANDROID__)
    /* SH3 Quest VR: SDLActivity passes QuestGameActivity.getArguments(): the game folder the launcher set up. */
    const char *dir = argc > 1 ? argv[1] : "/sdcard/Android/data/dev.sh3quest.vr/files/game";
    rt_log("SH3 Quest VR: game folder %s", dir);
#else
    const char *dir = argc > 1 ? argv[1] : getenv("SH3_DIR");
#endif
    if (dir) snprintf(rt_game_dir, sizeof rt_game_dir, "%s", dir);
    if (chdir(rt_game_dir)) rt_fatal("cannot enter %s", rt_game_dir);
    /* SH3 Quest VR: the last run's crash report is kept as sh3-crash-previous.log, so sh3-crash.log is only ever
     * this run's (rt_fatal appends to it). */
    remove("sh3-crash-previous.log");
    rename("sh3-crash.log", "sh3-crash-previous.log");
    rt_trace = getenv("SH3_TRACE") != NULL;
    if (getenv("SH3_COVER")) cover_from = abs(atoi(getenv("SH3_COVER"))), cover_all = atoi(getenv("SH3_COVER")) < 0;
    if (getenv("SH3_ORACLE")) sscanf(getenv("SH3_ORACLE"), "%x,%u", &oracle_va, &oracle_nth);

    host_crash_handlers(on_crash);
    arena_init();
    recomp_dispatch_init();
    host_thread_start(game_thread, NULL, 16u << 20);
    for (;;) sleep(1000);
}
