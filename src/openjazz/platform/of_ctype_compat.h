// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: (c) 2026 Evgeny Ugreninov
#ifndef OF_CTYPE_COMPAT_H
#define OF_CTYPE_COMPAT_H
/*
 * Force-included before every C++ TU. The xPack libstdc++ was built
 * --with-newlib and its <bits/ctype_base.h> spells the ctype facet masks
 * with newlib's _U/_L/... macros, which musl's <ctype.h> does not define.
 * Only the constants are needed at compile time; the runtime _ctype_
 * table lives in of_libc_compat.c.
 */
#ifndef _U
#define _U 01
#define _L 02
#define _N 04
#define _S 010
#define _P 020
#define _C 040
#define _X 0100
#define _B 0200
#endif
#endif
