/*                      D E F I N E S . H
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

/** @addtogroup bu_defines
 *
 * @brief
 * These are definitions specific to libbu, used throughout the library.
 *
 */
/** @{ */
/** @file bu/defines.h */

#ifndef BU_DEFINES_H
#define BU_DEFINES_H

#include "common.h"
#include <stddef.h>
#include <sys/types.h>

#ifndef BU_EXPORT
#  if defined(BU_DLL_EXPORTS) && defined(BU_DLL_IMPORTS)
#    error "Only BU_DLL_EXPORTS or BU_DLL_IMPORTS can be defined, not both."
#  elif defined(BU_DLL_EXPORTS)
#    define BU_EXPORT __declspec(dllexport)
#  elif defined(BU_DLL_IMPORTS)
#    define BU_EXPORT __declspec(dllimport)
#  else
#    define BU_EXPORT
#  endif
#endif

/* NOTE: do not rely on these values */
#define BRLCAD_OK 0
#define BRLCAD_ERROR 1

/**
 * @def BU_DIR_SEPARATOR
 * the default directory separator character
 */
#ifdef DIR_SEPARATOR
#  define BU_DIR_SEPARATOR DIR_SEPARATOR
#else
#  ifdef DIR_SEPARATOR_2
#    define BU_DIR_SEPARATOR DIR_SEPARATOR_2
#  else
#    ifdef _WIN32
#      define BU_DIR_SEPARATOR '\\'
#    else
#      define BU_DIR_SEPARATOR '/'
#    endif  /* _WIN32 */
#  endif  /* DIR_SEPARATOR_2 */
#endif  /* DIR_SEPARATOR */

/**
 * Maximum length of a filesystem path.  Typically defined in a system
 * file but if it isn't set, we create it.
 */
#ifndef MAXPATHLEN
#  ifdef PATH_MAX
#    define MAXPATHLEN PATH_MAX
#  else
#    ifdef _MAX_PATH
#      define MAXPATHLEN _MAX_PATH
#    else
#      define MAXPATHLEN 1024
#    endif
#  endif
#endif

/**
 * set to the path list separator character
 */
#if defined(PATH_SEPARATOR)
#  define BU_PATH_SEPARATOR PATH_SEPARATOR
#else
#  if defined(_WIN32)
#    define BU_PATH_SEPARATOR ';'
#  else
#    define BU_PATH_SEPARATOR ':'
#  endif  /* _WIN32 */
#endif  /* PATH_SEPARATOR */


/**
 * @def BU_FLSTR
 *
 * Macro for getting a concatenated string of the current file and
 * line number.  Produces something of the form: "filename.c"":""1234"
 */
#define bu_cpp_str(s) # s
#define bu_cpp_xstr(s) bu_cpp_str(s)
#define bu_cpp_glue(a, b) a ## b
#define bu_cpp_xglue(a, b) bu_cpp_glue(a, b)
#define BU_FLSTR __FILE__ ":" bu_cpp_xstr(__LINE__)


/**
 * shorthand declaration of a printf-style functions
 */
#if !defined(_BU_ATTR_PRINTF12)
#define _BU_ATTR_PRINTF12 __attribute__((__format__ (__printf__, 1, 2)))
#endif
#if !defined(_BU_ATTR_PRINTF23)
#define _BU_ATTR_PRINTF23 __attribute__((__format__ (__printf__, 2, 3)))
#endif
#if !defined(_BU_ATTR_SCANF23)
#define _BU_ATTR_SCANF23 __attribute__((__format__ (__scanf__, 2, 3)))
#endif

/**
 * shorthand declaration of a function that doesn't return
 */
#define _BU_ATTR_NORETURN __attribute__((__noreturn__))

/* For the moment, we need to specially flag some functions
 * for clang.  It's not clear if we will always need to do
 * this, but for now this suppresses a lot of noise in the
 * reports */
#ifdef __clang__
#  define _BU_ATTR_ANALYZE_NORETURN __attribute__((analyzer_noreturn))
#else
#  define _BU_ATTR_ANALYZE_NORETURN
#endif

/**
 * shorthand declaration of a function that should always be inline
 */
#define _BU_ATTR_ALWAYS_INLINE __attribute__((always_inline))

/**
 *  If we're compiling strict, turn off "format string vs arguments"
 *  checks - BRL-CAD customizes the arguments to some of these
 *  function types (adding bu_vls support) and that is a problem with
 *  strict checking.
 */
#if defined(STRICT_FLAGS)
#  undef _BU_ATTR_PRINTF12
#  undef _BU_ATTR_PRINTF23
#  undef _BU_ATTR_SCANF23
#  undef _BU_ATTR_NORETURN
#  define _BU_ATTR_PRINTF12
#  define _BU_ATTR_PRINTF23
#  define _BU_ATTR_SCANF23
#  define _BU_ATTR_NORETURN
#endif

/**
 * This macro is used to take the 'C' function name, and convert it at
 * compile time to the FORTRAN calling convention.
 *
 * Lower case, with a trailing underscore.
 */
#define BU_FORTRAN(lc, uc) lc ## _


/**
 * @def BU_ASSERT(eqn)
 * Quick and easy macros to generate an informative error message and
 * abort execution if the specified condition does not hold true.
 *
 * @def BU_ASSERT_PTR(eqn)
 * Quick and easy macros to generate an informative error message and
 * abort execution if the specified condition does not hold true.
 *
 * @def BU_ASSERT_LONG(eqn)
 * Quick and easy macros to generate an informative error message and
 * abort execution if the specified condition does not hold true.
 *
 * @def BU_ASSERT_DOUBLE(eqn)
 * Quick and easy macros to generate an informative error message and
 * abort execution if the specified condition does not hold true.
 *
 * Example: BU_ASSERT_LONG(j+7, <, 42);
 */
#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT(_equation) (void)(_equation)
#else
#  define BU_ASSERT(_equation)	\
    if (UNLIKELY(!(_equation))) { \
	bu_log("BU_ASSERT(" #_equation ") failed, file %s, line %d\n", \
	       __FILE__, __LINE__); \
	bu_bomb("BU_ASSERT failure\n"); \
    }
#endif

#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT_PTR(_lhs, _relation, _rhs) (void)(_lhs); (void)(_rhs)
#else
#  define BU_ASSERT_PTR(_lhs, _relation, _rhs)	\
    if (UNLIKELY(!((_lhs) _relation (_rhs)))) { \
	bu_log("BU_ASSERT_PTR(" #_lhs #_relation #_rhs ") failed, lhs=%p, rhs=%p, file %s, line %d\n", \
	       (void *)(_lhs), (void *)(_rhs), \
	       __FILE__, __LINE__); \
	bu_bomb("BU_ASSERT_PTR failure\n"); \
    }
#endif


#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT_LONG(_lhs, _relation, _rhs) (void)(_lhs); (void)(_rhs)
#else
#  define BU_ASSERT_LONG(_lhs, _relation, _rhs)	\
    if (UNLIKELY(!((_lhs) _relation (_rhs)))) { \
	bu_log("BU_ASSERT_LONG(" #_lhs #_relation #_rhs ") failed, lhs=%ld, rhs=%ld, file %s, line %d\n", \
	       (long)(_lhs), (long)(_rhs), \
	       __FILE__, __LINE__); \
	bu_bomb("BU_ASSERT_LONG failure\n"); \
    }
#endif


#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT_SIZE_T(_lhs, _relation, _rhs) (void)(_lhs); (void)(_rhs)
#else
#  define BU_ASSERT_SIZE_T(_lhs, _relation, _rhs)	\
    if (UNLIKELY(!((_lhs) _relation (_rhs)))) { \
	bu_log("BU_ASSERT_SIZE_T(" #_lhs #_relation #_rhs ") failed, lhs=%zd, rhs=%zd, file %s, line %d\n", \
	       (size_t)(_lhs), (size_t)(_rhs), \
	       __FILE__, __LINE__); \
	bu_bomb("BU_ASSERT_SIZE_T failure\n"); \
    }
#endif


#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT_SSIZE_T(_lhs, _relation, _rhs) (void)(_lhs); (void)(_rhs)
#else
#  define BU_ASSERT_SSIZE_T(_lhs, _relation, _rhs)	\
    if (UNLIKELY(!((_lhs) _relation (_rhs)))) { \
	bu_log("BU_ASSERT_SSIZE_T(" #_lhs #_relation #_rhs ") failed, lhs=%zd, rhs=%zd, file %s, line %d\n", \
	       (ssize_t)(_lhs), (ssize_t)(_rhs), \
	       __FILE__, __LINE__); \
	bu_bomb("BU_ASSERT_SSIZE_T failure\n"); \
    }
#endif


#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT_DOUBLE(_lhs, _relation, _rhs) (void)(_lhs); (void)(_rhs)
#else
#  define BU_ASSERT_DOUBLE(_lhs, _relation, _rhs)	\
    if (UNLIKELY(!((_lhs) _relation (_rhs)))) { \
	bu_log("BU_ASSERT_DOUBLE(" #_lhs #_relation #_rhs ") failed, lhs=%lf, rhs=%lf, file %s, line %d\n", \
	       (double)(_lhs), (double)(_rhs), \
	       __FILE__, __LINE__); \
	bu_bomb("BU_ASSERT_DOUBLE failure\n"); \
    }
#endif

/**
 * fastf_t - Intended to be the fastest floating point data type on
 * the current machine, with at least 64 bits of precision.  On 16 and
 * 32 bit machines, this is typically "double", but on 64 bit machines,
 * it is often "float".  Virtually all floating point variables (and
 * more complicated data types, like vect_t and mat_t) are defined as
 * fastf_t.  The one exception is when a subroutine return is a
 * floating point value; that is always declared as "double".
 *
 * TODO: If used pervasively, it should eventually be possible to make
 * fastf_t a GMP C++ type for fixed-precision computations.
 */
typedef double fastf_t;

/**
 * Definitions about limits of floating point representation
 * Eventually, should be tied to type of hardware (IEEE, IBM, Cray)
 * used to implement the fastf_t type.
 *
 * MAX_FASTF - Very close to the largest value that can be held by a
 * fastf_t without overflow.  Typically specified as an integer power
 * of ten, to make the value easy to spot when printed.  TODO: macro
 * function syntax instead of constant (DEPRECATED)
 *
 * SQRT_MAX_FASTF - sqrt(MAX_FASTF), or slightly smaller.  Any number
 * larger than this, if squared, can be expected to * produce an
 * overflow.  TODO: macro function syntax instead of constant
 * (DEPRECATED)
 *
 * SMALL_FASTF - Very close to the smallest value that can be
 * represented while still being greater than zero.  Any number
 * smaller than this (and non-negative) can be considered to be
 * zero; dividing by such a number can be expected to produce a
 * divide-by-zero error.  All divisors should be checked against
 * this value before actual division is performed.  TODO: macro
 * function syntax instead of constant (DEPRECATED)
 *
 * SQRT_SMALL_FASTF - sqrt(SMALL_FASTF), or slightly larger.  The
 * value of this is quite a lot larger than that of SMALL_FASTF.  Any
 * number smaller than this, when squared, can be expected to produce
 * a zero result.  TODO: macro function syntax instead of constant
 * (DEPRECATED)
 *
 */
#if defined(vax)
/* DEC VAX "D" format, the most restrictive */
#  define MAX_FASTF		1.0e37	/* Very close to the largest number */
#  define SQRT_MAX_FASTF	1.0e18	/* This squared just avoids overflow */
#  define SMALL_FASTF		1.0e-37	/* Anything smaller is zero */
#  define SQRT_SMALL_FASTF	1.0e-18	/* This squared gives zero */
#else
/* IBM format, being the next most restrictive format */
#  define MAX_FASTF		1.0e73	/* Very close to the largest number */
#  define SQRT_MAX_FASTF	1.0e36	/* This squared just avoids overflow */
#  define SMALL_FASTF		1.0e-77	/* Anything smaller is zero */
#  if defined(aux)
#    define SQRT_SMALL_FASTF	1.0e-40 /* _doprnt error in libc */
#  else
#    define SQRT_SMALL_FASTF	1.0e-39	/* This squared gives zero */
#  endif
#endif

/** DEPRECATED, do not use */
#define SMALL SQRT_SMALL_FASTF

/** @} */

#endif  /* BU_DEFINES_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
