/*                      D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
#    define BU_EXPORT COMPILER_DLLEXPORT
#  elif defined(BU_DLL_IMPORTS)
#    define BU_EXPORT COMPILER_DLLIMPORT
#  else
#    define BU_EXPORT
#  endif
#endif

/** All okay return code, not a maskable result.  Callers should not rely on
 * the numerical value. */
#define BRLCAD_OK    0x0000

/**
 * Possible maskable return codes from BRL-CAD functions.  Callers should not
 * rely on the actual values or exact numerical equalities but should instead
 * test via masking.
 */
#define BRLCAD_ERROR   0x0001 /**< something went wrong, the action was not performed */
#define BRLCAD_HELP    0x0002 /**< invalid specification, result contains usage */
#define BRLCAD_MORE    0x0004 /**< incomplete specification, can specify again interactively */
#define BRLCAD_QUIET   0x0008 /**< don't set or modify the result string */
#define BRLCAD_UNKNOWN 0x0010 /**< argv[0] was not a known command */
#define BRLCAD_EXIT    0x0020 /**< command is requesting a clean application shutdown */

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
 * shorthand declaration of a printf-style functions
 */
#ifdef HAVE_PRINTF12_ATTRIBUTE
#  define _BU_ATTR_PRINTF12 __attribute__((__format__ (__printf__, 1, 2)))
#elif !defined(_BU_ATTR_PRINTF12)
#  define _BU_ATTR_PRINTF12
#endif
#ifdef HAVE_PRINTF23_ATTRIBUTE
#  define _BU_ATTR_PRINTF23 __attribute__((__format__ (__printf__, 2, 3)))
#elif !defined(_BU_ATTR_PRINTF23)
#  define _BU_ATTR_PRINTF23
#endif
#ifdef HAVE_SCANF23_ATTRIBUTE
#  define _BU_ATTR_SCANF23 __attribute__((__format__ (__scanf__, 2, 3)))
#elif !defined(_BU_ATTR_SCANF23)
#  define _BU_ATTR_SCANF23
#endif

/**
 * shorthand declaration of a function that doesn't return
 */
#ifdef HAVE_NORETURN_ATTRIBUTE
#  define _BU_ATTR_NORETURN __attribute__((__noreturn__))
#else
#  define _BU_ATTR_NORETURN
#endif

/* For the moment, we need to specially flag some functions
 * for clang.  It's not clear if we will always need to do
 * this, but for now this suppresses a lot of noise in the
 * reports */
#ifdef HAVE_ANALYZER_NORETURN_ATTRIBUTE
#  define _BU_ATTR_ANALYZE_NORETURN __attribute__((analyzer_noreturn))
#else
#  define _BU_ATTR_ANALYZE_NORETURN
#endif

/**
 * shorthand declaration of a function that should always be inline
 */

#ifdef HAVE_ALWAYS_INLINE_ATTRIBUTE
#  define _BU_ATTR_ALWAYS_INLINE __attribute__((always_inline))
#else
#  define _BU_ATTR_ALWAYS_INLINE
#endif

/**
 * shorthand declaration of a function that will return the exact same
 * value for the exact same arguments.  this implies it's a function
 * that doesn't examine into any pointer values, doesn't call any
 * non-cost functions, doesn't read globals, and has no effects except
 * the return value.
 */
#ifdef HAVE_CONST_ATTRIBUTE
#  define _BU_ATTR_CONST __attribute__((const))
#else
#  define _BU_ATTR_CONST
#endif

/**
 * shorthand declaration of a function that depends only on its
 * parameters and/or global variables.  this implies it's a function
 * that has no effects except the return value and, as such, can be
 * subject to common subexpression elimination and loop optimization
 * just as an arithmetic operator would be.
 */
#ifdef HAVE_PURE_ATTRIBUTE
#  define _BU_ATTR_PURE __attribute__((pure))
#else
#  define _BU_ATTR_PURE
#endif

/**
 * shorthand declaration of a function that is not likely to be
 * called.  this is typically for debug logging and error routines.
 */
#ifdef HAVE_COLD_ATTRIBUTE
#  define _BU_ATTR_COLD __attribute__((cold))
#else
#  define _BU_ATTR_COLD
#endif

/**
 * shorthand declaration of a function that doesn't accept NULL
 * pointer arguments.  if a null pointer is detected during
 * compilation, a warning/error can be emitted.
 */
#ifdef HAVE_NONNULL_ATTRIBUTE
#  define _BU_ATTR_NONNULL __attribute__((nonnull))
#else
#  define _BU_ATTR_NONNULL
#endif

/**
 * shorthand declaration of a function whose return value should not
 * be ignored.  a warning / error will be emitted if the caller does
 * not use the return value.
 */
#ifdef HAVE_WARN_UNUSED_RESULT_ATTRIBUTE
#  define _BU_ATTR_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#  define _BU_ATTR_WARN_UNUSED_RESULT
#endif


/**
 * shorthand placed before a function _definition_ indicating to some
 * compilers that it should inline most of the function calls within
 * the function.  this should be used sparingly on functions that are
 * demonstrably hot, as indicated by a profiler.
 */
#ifdef HAVE_FLATTEN_ATTRIBUTE
#  define _BU_ATTR_FLATTEN __attribute__((flatten))
#else
#  define _BU_ATTR_FLATTEN
#endif

/**
 * This macro is used to take the 'C' function name, and convert it at
 * compile time to the FORTRAN calling convention.
 *
 * Lower case, with a trailing underscore.
 */
#define BU_FORTRAN(lc, uc) lc ## _


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
