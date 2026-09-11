// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: (c) 2026 Evgeny Ugreninov
// Desktop-only: report peak C++ heap usage on exit, to size the Pocket's
// 3 MB application heap. Opt-in via `make test MEMSTAT=1` (see PC_SRCS_CXX
// in the Makefile) — never part of the cross build, which has its own
// allocator and no malloc_size().
#if defined(OF_PC) && defined(MEMSTAT)

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <new>
#include <malloc/malloc.h>

static size_t g_cur = 0;
static size_t g_peak = 0;

void* operator new(size_t n) {
    void* p = malloc(n);
    if (p) {
        g_cur += malloc_size(p);
        if (g_cur > g_peak) g_peak = g_cur;
    }
    return p;
}

void operator delete(void* p) noexcept {
    if (p) g_cur -= malloc_size(p);
    free(p);
}

void operator delete(void* p, size_t) noexcept {
    if (p) g_cur -= malloc_size(p);
    free(p);
}

namespace {

void print_peak() {
    fprintf(stderr, "peak C++ heap: %zu KB\n", g_peak / 1024);
}

// The measurement run has no way to quit through the window (nothing
// drives the SDL event loop headlessly), so it's killed with a timer/
// interrupt signal instead. A signal's default action skips static
// destructors, so trap the signals that measurement run uses and report
// before exiting; the destructor below still covers a normal quit.
void on_signal(int) {
    print_peak();
    _exit(0);
}

struct Report {
    Report() {
        signal(SIGALRM, on_signal);
        signal(SIGINT, on_signal);
        signal(SIGTERM, on_signal);
    }
    ~Report() { print_peak(); }
} g_report;

}

#endif // defined(OF_PC) && defined(MEMSTAT)
