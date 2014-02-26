/*                            B U . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @file bu.h
 *
 * Main header file for the BRL-CAD Utility Library, LIBBU.
 *
 * The two letters "BU" stand for "BRL-CAD" and "Utility".  This
 * library provides several layers of low-level utility routines,
 * providing features that make cross-platform coding easier.
 *
 * Parallel processing support:  threads, semaphores, parallel-malloc.
 * Consolidated logging support:  bu_log(), bu_exit(), and bu_bomb().
 *
 * The intention is that these routines are general extensions to the
 * data types offered by the C language itself, and to the basic C
 * runtime support provided by the system LIBC.  All routines in LIBBU
 * are designed to be "parallel-safe" (sometimes called "mp-safe" or
 * "thread-safe" if parallelism is via threading) to greatly ease code
 * development for multiprocessor systems.
 *
 * All of the data types provided by this library are defined in bu.h
 * or appropriate included files from the ./bu subdirectory; none of
 * the routines in this library will depend on data types defined in
 * other BRL-CAD header files, such as vmath.h.  Look for those
 * routines in LIBBN.
 *
 * All truly fatal errors detected by the library use bu_bomb() to
 * exit with a status of 12.  The LIBBU variants of system calls
 * (e.g., bu_malloc()) do not return to the caller (unless there's a
 * bomb hook defined) unless they succeed, thus sparing the programmer
 * from constantly having to check for NULL return codes.
 *
 */
#ifndef BU_H
#define BU_H


#include "./bu/defines.h"

__BEGIN_DECLS

/** @file libbu/vers.c
 *
 * version information about LIBBU
 *
 */

/**
 * returns the compile-time version of libbu
 */
BU_EXPORT extern const char *bu_version(void);

#include "./bu/cv.h"
#include "./bu/endian.h"
#include "./bu/list.h"
#include "./bu/bitv.h"
#include "./bu/hist.h"
#include "./bu/ptbl.h"
#include "./bu/mapped_file.h"
#include "./bu/vls.h"
#include "./bu/avs.h"
#include "./bu/vlb.h"
#include "./bu/debug.h"
#include "./bu/parse.h"
#include "./bu/color.h"
#include "./bu/rb.h"
#include "./bu/log.h"
#include "./bu/file.h"
#include "./bu/time.h"
#include "./bu/units.h"
#include "./bu/getopt.h"
#include "./bu/parallel.h"
#include "./bu/simd.h"
#include "./bu/malloc.h"
#include "./bu/str.h"
#include "./bu/hash.h"
#include "./bu/bu_tcl.h"

__END_DECLS

#endif  /* BU_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
