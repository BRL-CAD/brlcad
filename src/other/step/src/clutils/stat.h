
#ifdef __O3DB__
#include <OpenOODB.h>
#endif

/*
 * Copyright (c) 1989 Stanford University
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Stanford not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Stanford makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * STANFORD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
 * IN NO EVENT SHALL STANFORD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * C++ interface to Unix file system stat structure
 */

#ifndef sys_stat_h

#if defined(__cplusplus)
extern "C" {
#endif

#define KERNEL
#include "//usr/include/sys/stat.h"
#undef KERNEL

/* just in case standard header didn't */
#ifndef sys_stat_h
#define sys_stat_h
#endif

extern int stat(const char* path, struct stat*);
extern int fstat(int fd, struct stat*);
extern int lstat(const char* path, struct stat*);

#if defined(__cplusplus)
}
#endif

#endif
