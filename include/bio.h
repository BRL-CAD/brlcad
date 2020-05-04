/*                           B I O . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @addtogroup bu_bio
 *
 * @brief
 * BRL-CAD system compatibility wrapper header that provides declarations for
 * native and standard system INPUT/OUTPUT routines.
 *
 * This header is commonly used in lieu of including the following: stdio.h,
 * io.h, fcntl, unistd.h, and windows.h
 *
 * The logic in this header should not rely on common.h's HAVE_* defines and
 * should not be including the common.h header.  This is intended to be a
 * stand-alone portability header intended to be independent of build system,
 * reusable by external projects.
 */

/** @{ */
/** @file bio.h */

#ifndef BIO_H
#define BIO_H

#include <stdio.h>

/* strict mode may not declare fileno() */
# if !defined(fileno) && !defined(__cplusplus)
extern int fileno(FILE *stream);
# endif

#if defined(_WIN32) && !defined(__CYGWIN__)

#  ifdef WIN32_LEAN_AND_MEAN
#    undef WIN32_LEAN_AND_MEAN
#  endif
#  define WIN32_LEAN_AND_MEAN 434144 /* don't want winsock.h */

#  ifdef NOMINMAX
#    undef NOMINMAX
#  endif
#  define NOMINMAX 434144 /* don't break std::min and std::max */

#  include <windows.h>

#  undef WIN32_LEAN_AND_MEAN /* unset to not interfere with calling apps */
#  undef NOMINMAX
#  include <io.h>

#  undef rad1 /* Win32 radio button 1 */
#  undef rad2 /* Win32 radio button 2 */
#  undef small /* defined as part of the Microsoft Interface Definition Language (MIDL) */

#else

#  include <unistd.h>

/* provide a stub so we don't need to wrap all setmode() calls */
#  define setmode(a, b) /* poof */
#endif

/* needed for testing O_TEMPORARY and O_BINARY */
#include <fcntl.h>

/* _O_TEMPORARY on Windows removes file when last descriptor is closed */
#ifndef O_TEMPORARY
#  define O_TEMPORARY 0
#endif

/* _O_BINARY on Windows indicates whether to use binary or text (default) I/O */
#ifndef O_BINARY
#  define O_BINARY 0
#endif

/* the S_IS* macros should replace the S_IF*'s
   already defined in C99 compliant compilers
   this is the work-around for older compilers */
#ifndef S_ISDIR
#   define S_ISDIR(_st_mode) (((_st_mode) & S_IFMT) == S_IFDIR)
#endif

/* We want 64 bit (large file) I/O capabilities whenever they are available.
 * Always define this before we include sys/types.h */
#ifndef _FILE_OFFSET_BITS
#  define _FILE_OFFSET_BITS 64
#endif
#include <sys/types.h>

/* off_t is 32 bit size even on 64 bit Windows. In the past we have tried to
 * force off_t to be 64 bit but this is failing on newer Windows/Visual Studio
 * verions in 2020 - therefore, we instead introduce the b_off_t define to
 * properly substitute the correct numerical type for the correct platform.  */
#if defined(_WIN64)
#  include <sys/stat.h>
#  define b_off_t __int64
#  define fseek _fseeki64
#  define ftell _ftelli64
#  define fstat _fstati64
#  define lseek _lseeki64
#  define stat  _stati64
#elif defined (_WIN32)
#  include <sys/stat.h>
#  define b_off_t _off_t
#  define fstat _fstat
#  define lseek _lseek
#  define stat  _stat
#else
#  define b_off_t off_t
#endif

#endif /* BIO_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
