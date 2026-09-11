// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: (c) 2026 Evgeny Ugreninov
/*
 * of_libc_compat.c -- newlib -> musl glue for the toolchain's libstdc++.
 * Same role as openfpgaOS/Diablo src/diablo/platform/of_libc_compat.c.
 */
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

extern int *__errno_location(void);

/* The slot FS has no directories; OpenJazz never calls mkdir on this
 * platform (xdg.cpp is compiled out), but libstdc++ references it. */
int mkdir(const char *path, mode_t mode) { (void)path; (void)mode; return 0; }
int truncate(const char *path, off_t length) { (void)path; (void)length; return 0; }
int ftruncate(int fd, off_t length) { (void)fd; (void)length; return 0; }

static char of_fake_reent[1024] __attribute__((aligned(16)));
void *_impure_ptr = of_fake_reent;

void *__dso_handle = 0;
int *__errno(void) { return __errno_location(); }
int __locale_mb_cur_max(void) { return 1; }

char _ctype_[1 + 256];

__attribute__((constructor(101)))
static void of_fill_ctype(void) {
	for (int c = 0; c < 256; c++) {
		unsigned m = 0;
		if (isupper(c))  m |= _U;
		if (islower(c))  m |= _L;
		if (isdigit(c))  m |= _N;
		if (isspace(c))  m |= _S;
		if (ispunct(c))  m |= _P;
		if (iscntrl(c))  m |= _C;
		if (isxdigit(c)) m |= _X;
		if (c == ' ')    m |= _B;
		_ctype_[c + 1] = (char)m;
	}
	_ctype_[0] = 0;
}
