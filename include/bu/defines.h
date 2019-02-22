/*                      D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2019 United States Government as represented by
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
 */
#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT(_equation) (void)(_equation)
#else
#  define BU_ASSERT(_equation)	\
    if (UNLIKELY(!(_equation))) { \
	const char *_equation_buf = #_equation; \
	bu_log("BU_ASSERT(%s), failed, file %s, line %d\n", \
	       _equation_buf, __FILE__, __LINE__); \
	bu_bomb("BU_ASSERT failure\n"); \
    }
#endif


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
