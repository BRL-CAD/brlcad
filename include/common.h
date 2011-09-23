/*                        C O M M O N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @addtogroup fixme */
/** @{ */
/** @file common.h
 *
 * @brief
 *  Header file for the BRL-CAD common definitions.
 *
 *  This header wraps the system-specific encapsulation of
 *  brlcad_config.h and removes need to conditionally include
 *  brlcad_config.h everywhere based on HAVE_CONFIG_H.  The common
 *  definitions are symbols common to the platform being built that
 *  are either detected via configure or hand crafted, as is the case
 *  for the win32 platform.
 *
 */

#ifndef __COMMON_H__
#define __COMMON_H__

/* include the venerable config.h file.  use a pregenerated one for
 * windows when we cannot autogenerate it easily. do not include
 * config.h if this file has been installed.  (public header files
 * should not use config defines)
 */
#if defined(BRLCADBUILD) && defined(HAVE_CONFIG_H)

#  if defined(_WIN32) && !defined(__CYGWIN__) && !defined(CMAKE_HEADERS)
#    include "config_win.h"
#  else
#    include "brlcad_config.h"
#  endif  /* _WIN32 */

/* Simulates drand48() functionality using rand() which is assumed to
 * exist everywhere. The range is [0, 1).
 */
#  ifndef HAVE_DRAND48
#    define drand48() ((double)rand() / (double)(RAND_MAX + 1))
#    define HAVE_DRAND48 1
#	 define srand48(seed) (srand(seed))
#  endif

#endif  /* BRLCADBUILD & HAVE_CONFIG_H */

/* provide declaration markers for header externals */
#ifdef __cplusplus
#  define __BEGIN_DECLS   extern "C" {
#  define __END_DECLS     }
#else
#  define __BEGIN_DECLS
#  define __END_DECLS
#endif

/* Functions local to one file IN A LIBRARY should be declared HIDDEN.
 * Disabling the static classifier is sometimes helpful for debugging.
 * It can help prevent some compilers from inlining functions that one
 * might want to set a breakpoint on.
 */
#if !defined(HIDDEN)
#  if defined(NDEBUG)
#    define HIDDEN	static
#  else
#    define HIDDEN	/***/
#  endif
#endif

/* ansi c89 does not allow the 'inline' keyword */
#ifdef __STRICT_ANSI__
#  ifndef inline
#    define inline /***/
#  endif
#endif

#ifndef FMAX
#  define FMAX(a, b)	(((a)>(b))?(a):(b))
#endif
#ifndef FMIN
#  define FMIN(a, b)	(((a)<(b))?(a):(b))
#endif

/* C99 does not provide a ssize_t even though it is provided by SUS97.
 * regardless, we use it so make sure it's declared by using the
 * similar POSIX ptrdiff_t type.
 */
#ifndef HAVE_SSIZE_T
#  ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
#  endif
#  include <limits.h>
#  include <stddef.h>
#  ifndef SSIZE_MAX
typedef ptrdiff_t ssize_t;
#    define HAVE_SSIZE_T 1
#  endif
#endif

/* make sure most of the C99 stdint types are provided including the
 * optional uintptr_t type.
 */
#if !defined(INT8_MAX) || !defined(INT16_MAX) || !defined(INT32_MAX) || !defined(INT64_MAX)
#  if (defined _MSC_VER && (_MSC_VER <= 1500))	
     /* Older Versions of Visual C++ seem to need pstdint.h 
      * but still pass the tests below, so force it based on
      * version (ugh.) */
#    include "pstdint.h"
#  elif defined(__STDC__) || defined(__STRICT_ANSI__) || defined(__SIZE_TYPE__) || defined(HAVE_STDINT_H)
#    define __STDC_LIMIT_MACROS 1
#    define __STDC_CONSTANT_MACROS 1
#    include <stdint.h>
#  else
#    include "pstdint.h"
#  endif
#endif

/* Provide a means to conveniently test the version of the GNU
 * compiler.  Use it like this:
 *
 * #if GCC_PREREQ(2,8)
 * ... code requiring gcc 2.8 or later ...
 * #endif
 *
 * WARNING: THIS MACRO IS CONSIDERED PRIVATE AND SHOULD NOT BE USED
 * OUTSIDE OF THIS HEADER FILE.  DO NOT RELY ON IT.
 */
#ifndef GCC_PREREQ
#  if defined __GNUC__
#    define GCC_PREREQ(major, minor) __GNUC__ > (major) || (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor))
#  else
#    define GCC_PREREQ(major, minor) 0
#  endif
#else
#  warning "GCC_PREREQ is already defined.  See the common.h header."
#endif

/* Provide a means to conveniently test the version of the Intel
 * compiler.  Use it like this:
 *
 * #if ICC_PREREQ(800)
 * ... code requiring icc 8.0 or later ...
 * #endif
 *
 * WARNING: THIS MACRO IS CONSIDERED PRIVATE AND SHOULD NOT BE USED
 * OUTSIDE OF THIS HEADER FILE.  DO NOT RELY ON IT.
 */
/* provide a means to conveniently test the version of ICC */
#ifndef ICC_PREREQ
#  if defined __INTEL_COMPILER
#    define ICC_PREREQ(version) (__INTEL_COMPILER >= (version))
#  else
#    define ICC_PREREQ(version) 0
#  endif
#else
#  warning "ICC_PREREQ is already defined.  See the common.h header."
#endif

/* This is so we can use gcc's "format string vs arguments"-check for
 * various printf-like functions, and still maintain compatability.
 */
#ifndef __attribute__
/* This feature is only available in gcc versions 2.5 and later. */
#  if !GCC_PREREQ(2, 5)
#    define __attribute__(ignore) /* empty */
#  endif
/* The __-protected variants of `format' and `printf' attributes
 * are accepted by gcc versions 2.6.4 (effectively 2.7) and later.
 */
#  if !GCC_PREREQ(2, 7)
#    define __format__ format
#    define __printf__ printf
#    define __noreturn__ noreturn
#  endif
#endif

/* gcc 3.4 doesn't seem to support always_inline with -O0 (yet -Os
 * reportedly works), so turn it off.
 */
#if !GCC_PREREQ(3, 5)
#  define always_inline noinline
#endif

/**
 * UNUSED provides a common mechanism for declaring unused parameters.
 * Use it like this:
 *
 * int
 * my_function(int argc, char **UNUSED(argv))
 * {
 *   ...
 * }
 *
 */
#ifndef UNUSED
#  if GCC_PREREQ(2, 5)
     /* GCC-style */
#    define UNUSED(parameter) UNUSED_ ## parameter __attribute__((unused))
#  else
     /* MSVC/C++ */
#    ifdef __cplusplus
#      if defined(NDEBUG)
#        define UNUSED(parameter) /* parameter */
#      else /* some of them are asserted */
#         define UNUSED(parameter) (parameter)
#      endif
#    else
#      if defined(_MSC_VER)
         /* disable reporting an "unreferenced formal parameter" */
#        pragma warning( disable : 4100 )
#      endif
#      define UNUSED(parameter) (parameter)
#    endif
#  endif
#else
#  undef UNUSED
#  define UNUSED(parameter) (parameter)
#  warning "UNUSED was previously defined.  Parameter declaration behavior is unknown, see common.h"
#endif

/**
 * IGNORE provides a common mechanism for innocuously ignoring a
 * parameter that is sometimes used and sometimes not.  It should
 * "practically" result in nothing of concern happening.  It's
 * commonly used by macros that disable functionality based on
 * compilation settings (e.g., BU_ASSERT()) and shouldn't normally
 * need to be used directly by code.
 *
 * We can't use (void)(sizeof((parameter)) because MSVC2010 will
 * reportedly report a warning about the value being unused.
 * (Consequently calls into question (void)(parameter) but untested.)
 *
 * Possible alternative:
 * ((void)(1 ? 0 : sizeof((parameter)) - sizeof((parameter))))
 */
#define IGNORE(parameter) (void)(parameter)

/**
 * LIKELY provides a common mechanism for providing branch prediction
 * hints to the compiler so that it can better optimize.  It should be
 * used when it's exceptionally likely that an expected code path will
 * almost always be executed.  Use it like this:
 *
 *  if (LIKELY(x == 1)) {
 *    ... expected code path ...
 *  }
 *
 */
#ifndef LIKELY
#  if GCC_PREREQ(3, 0) || ICC_PREREQ(800)

#    define LIKELY(expression) __builtin_expect((expression), 1)
#  else
#    define LIKELY(expression) (expression)
#  endif
#else
#  undef LIKELY
#  define LIKELY(expression) (expression)
#  warning "LIKELY was previously defined.  Unable to provide branch hinting."
#endif

/**
 * UNLIKELY provides a common mechanism for providing branch
 * prediction hints to the compiler so that it can better optimize.
 * It should be used when it's exceptionaly unlikely that a given code
 * path will ever be executed.  Use it like this:
 *
 *  if (UNLIKELY(x == 0)) {
 *    ... unexpected code path ...
 *  }
 *
 */
#ifndef UNLIKELY
#  if GCC_PREREQ(3, 0) || ICC_PREREQ(800)
#    define UNLIKELY(expression) __builtin_expect((expression), 0)
#  else
#    define UNLIKELY(expression) (expression)
#  endif
#else
#  undef UNLIKELY
#  define UNLIKELY(expression) (expression)
#  warning "UNLIKELY was previously defined.  Unable to provide branch hinting."
#endif

/**
 * DEPRECATED provides a common mechanism for denoting public API
 * (e.g., functions, typedefs, variables) that is considered
 * deprecated.  Use it like this:
 *
 * DEPRECATED int my_function(void);
 *
 * typedef struct karma some_type DEPRECATED;
 */
#ifndef DEPRECATED
#  if GCC_PREREQ(3, 1) || ICC_PREREQ(800)
#    define DEPRECATED __attribute__((deprecated))
#  elif defined(_WIN32)
#    define DEPRECATED __declspec(deprecated("This function is DEPRECATED.  Please update code to new API."))
#  else
#    define DEPRECATED /* deprecated */
#  endif
#else
#  undef DEPRECATED
#  define DEPRECATED /* deprecated */
#  warning "DEPRECATED was previously defined.  Disabling the declaration."
#endif

/* ActiveState Tcl doesn't include this catch in tclPlatDecls.h, so we
 * have to add it for them
 */
#if defined(_MSC_VER) && defined(__STDC__)
   #include <tchar.h>
   /* MSVC++ misses this. */
   typedef _TCHAR TCHAR;
#endif


#endif  /* __COMMON_H__ */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
