// SPDX-License-Identifier: GPL-3.0-or-later
// SH3 Quest VR, the PC rig: a sampling profiler. It attaches to a running process, samples every thread's instruction
// pointer (suspend, read, resume) for the given seconds, then names the places with the process's own symbols (the
// PDB next to build/pc/sh3.exe) and prints each busy thread's functions by their share of the samples.
// 0.1.5: names of system functions to explain (argument 4, comma separated, e.g. NtQueryInformationThread): a sample
// that lands in one has its stack walked while the thread is stopped, and the callers' chains are printed by count
// (vr264's main thread spent 14% in NtQueryInformationThread with no caller of it in the port).
//
// Build (from a VS 2022 x64 prompt or with clang):
//   clang -O2 tools/prof_sample.c -o build/prof_sample.exe -ldbghelp -lwinmm
// Use:  build/prof_sample.exe <pid> [seconds=20] [top=40] [Function,Function...]
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { DWORD tid; HANDLE h; DWORD64 *pcs; size_t n, cap; } Thread;
typedef struct { char name[1024]; size_t count; } Entry;
typedef struct { DWORD tid; int n; DWORD64 pc[10]; } Chain;
typedef struct { DWORD64 lo, hi; const char *name; } Target;

static int by_count(const void *a, const void *b) {
    const Entry *x = a, *y = b;
    return x->count < y->count ? 1 : x->count > y->count ? -1 : 0;
}

static HANDLE proc;

static void name_of(DWORD64 pc, char *out, size_t size) {
    char buf[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO *s = (SYMBOL_INFO *)buf;
    memset(buf, 0, sizeof buf);
    s->SizeOfStruct = sizeof(SYMBOL_INFO);
    s->MaxNameLen = 255;
    DWORD64 disp = 0;
    IMAGEHLP_MODULE64 m = {sizeof m};
    const char *mod = SymGetModuleInfo64(proc, pc, &m) ? m.ModuleName : "?";
    if (SymFromAddr(proc, pc, &disp, s)) {
        IMAGEHLP_LINE64 line = {sizeof line};
        DWORD ld = 0;
        if (SymGetLineFromAddr64(proc, pc, &ld, &line)) {
            const char *file = strrchr(line.FileName, '\\');
            snprintf(out, size, "%s!%s (%s:%lu)", mod, s->Name, file ? file + 1 : line.FileName, line.LineNumber);
        } else {
            snprintf(out, size, "%s!%s", mod, s->Name);
        }
    } else {
        snprintf(out, size, "%s!%llx", mod, (unsigned long long)(pc & ~0xFFFull));
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: prof_sample <pid> [seconds] [top] [Function,...]\n"); return 1; }
    DWORD pid = (DWORD)atoi(argv[1]);
    double seconds = argc > 2 ? atof(argv[2]) : 20.0;
    int top = argc > 3 ? atoi(argv[3]) : 40;
    proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) { fprintf(stderr, "OpenProcess %lu failed: %lu\n", pid, GetLastError()); return 1; }
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    if (!SymInitialize(proc, NULL, TRUE)) fprintf(stderr, "SymInitialize failed: %lu\n", GetLastError());
    // the system functions to explain: ntdll, kernelbase and kernel32 sit at the same place in every process of a boot
    Target targets[16];
    int ntargets = 0;
    if (argc > 4) {
        static char list[1024];
        snprintf(list, sizeof list, "%s", argv[4]);
        const char *mods[] = {"ntdll.dll", "kernelbase.dll", "kernel32.dll"};
        for (char *name = strtok(list, ","); name && ntargets < 16; name = strtok(NULL, ",")) {
            for (int m = 0; m < 3; m++) {
                HMODULE h = GetModuleHandleA(mods[m]);
                FARPROC fn = h ? GetProcAddress(h, name) : NULL;
                if (!fn) continue;
                targets[ntargets++] = (Target){(DWORD64)(uintptr_t)fn, (DWORD64)(uintptr_t)fn + 0x40, name};
                printf("explaining %s at %p (%s)\n", name, (void *)fn, mods[m]);
                break;
            }
        }
    }
    size_t nchain = 0, chain_cap = 1 << 16;
    Chain *chains = malloc(chain_cap * sizeof *chains);
    Thread th[256];
    int nth = 0;
    timeBeginPeriod(1);
    LARGE_INTEGER f, t0, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    size_t rounds = 0;
    for (;;) {
        QueryPerformanceCounter(&t);
        if ((double)(t.QuadPart - t0.QuadPart) / f.QuadPart > seconds) break;
        // new threads, every 100 rounds
        if (rounds % 100 == 0) {
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            THREADENTRY32 te = {sizeof te};
            for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
                if (te.th32OwnerProcessID != pid) continue;
                int seen = 0;
                for (int i = 0; i < nth; i++) seen |= th[i].tid == te.th32ThreadID;
                if (seen || nth >= 256) continue;
                HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE,
                                      te.th32ThreadID);
                if (h) th[nth++] = (Thread){te.th32ThreadID, h, NULL, 0, 0};
            }
            CloseHandle(snap);
        }
        for (int i = 0; i < nth; i++) {
            if (SuspendThread(th[i].h) == (DWORD)-1) continue;
            CONTEXT c = {0};
            c.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext(th[i].h, &c)) {
                if (th[i].n == th[i].cap) {
                    th[i].cap = th[i].cap ? th[i].cap * 2 : 4096;
                    th[i].pcs = realloc(th[i].pcs, th[i].cap * sizeof *th[i].pcs);
                }
                th[i].pcs[th[i].n++] = c.Rip;
                for (int k = 0; k < ntargets && nchain < chain_cap; k++) {
                    if (c.Rip < targets[k].lo || c.Rip >= targets[k].hi) continue;
                    // walk while the thread is stopped: its stack is as the sample saw it
                    STACKFRAME64 sf = {0};
                    sf.AddrPC.Offset = c.Rip, sf.AddrPC.Mode = AddrModeFlat;
                    sf.AddrFrame.Offset = c.Rbp, sf.AddrFrame.Mode = AddrModeFlat;
                    sf.AddrStack.Offset = c.Rsp, sf.AddrStack.Mode = AddrModeFlat;
                    Chain *ch = &chains[nchain++];
                    ch->tid = th[i].tid;
                    ch->n = 0;
                    CONTEXT walk = c;
                    while (ch->n < 10 && StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, th[i].h, &sf, &walk, NULL,
                                                     SymFunctionTableAccess64, SymGetModuleBase64, NULL) &&
                           sf.AddrPC.Offset)
                        ch->pc[ch->n++] = sf.AddrPC.Offset;
                    break;
                }
            }
            ResumeThread(th[i].h);
        }
        rounds++;
        Sleep(1);
    }
    printf("%zu rounds over %.1f s, %d threads\n", rounds, seconds, nth);
    for (int i = 0; i < nth; i++) {
        if (th[i].n < rounds / 20) continue;
        Entry *e = calloc(th[i].n, sizeof *e);
        size_t ne = 0;
        for (size_t k = 0; k < th[i].n; k++) {
            char name[1024];
            name_of(th[i].pcs[k], name, sizeof name);
            size_t j = 0;
            while (j < ne && strcmp(e[j].name, name)) j++;
            if (j == ne) { strcpy(e[ne].name, name); e[ne++].count = 0; }
            e[j].count++;
        }
        qsort(e, ne, sizeof *e, by_count);
        printf("\n== thread %lu: %zu samples\n", th[i].tid, th[i].n);
        for (size_t j = 0; j < ne && (int)j < top; j++)
            printf("%6.2f%%  %s\n", 100.0 * e[j].count / th[i].n, e[j].name);
        free(e);
    }
    if (ntargets) {
        // each walked sample's callers (frames 1-6), grouped
        Entry *e = calloc(nchain + 1, sizeof *e);
        size_t ne = 0;
        for (size_t k = 0; k < nchain; k++) {
            char line[1024];
            int used = snprintf(line, sizeof line, "thread %lu:", chains[k].tid);
            for (int d = 1; d < chains[k].n && d <= 6 && used < (int)sizeof line - 2; d++) {
                char name[300];
                name_of(chains[k].pc[d], name, sizeof name);
                used += snprintf(line + used, sizeof line - used, " <- %s", name);
            }
            size_t j = 0;
            while (j < ne && strcmp(e[j].name, line)) j++;
            if (j == ne) { snprintf(e[ne].name, sizeof e[ne].name, "%s", line); e[ne++].count = 0; }
            e[j].count++;
        }
        qsort(e, ne, sizeof *e, by_count);
        printf("\n== callers of the explained functions: %zu samples walked\n", nchain);
        for (size_t j = 0; j < ne && j < 25; j++) printf("%6zu  %s\n", e[j].count, e[j].name);
        free(e);
    }
    return 0;
}
