/*                        C O M M O N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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

/** @addtogroup common
 *  This header wraps the system-specific encapsulation of
 *  brlcad_config.h and removes need to conditionally include
 *  brlcad_config.h everywhere based on HAVE_CONFIG_H.  The common
 *  definitions are symbols common to the platform being built that
 *  are either detected via configure or hand crafted, as is the case
 *  for the win32 platform.
 *
 */
/** @{ */
/** @brief Header file for the BRL-CAD common definitions. */
/** @file common.h */

#ifndef COMMON_H
#define COMMON_H

/* include the venerable config.h file.  use a pregenerated one for
 * windows when we cannot auto-generate it easily. do not include
 * config.h if this file has been installed.  (public header files
 * should not use config defines)
 */
#if defined(BRLCADBUILD) && defined(HAVE_CONFIG_H)

#  if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(CMAKE_HEADERS)
#    include "config_win.h"
#    include "brlcad_config.h"
#  else
#    include "brlcad_config.h"
#  endif  /* _WIN32 */

/* Simulates drand48() functionality using rand() which is assumed to
 * exist everywhere. The range is [0, 1).
 */
#  ifndef HAVE_DRAND48
#    define drand48() ((double)rand() / (double)(RAND_MAX + 1))
#    define HAVE_DRAND48 1
#    define srand48(seed) (srand(seed))
#  endif

/* make sure lrint() is provided */
#  if !defined(__cplusplus) && !defined(HAVE_LRINT) && defined(HAVE_WORKING_LRINT_MACRO)
#    define lrint(_x) (((_x) < 0.0) ? (long int)ceil((_x)-0.5) : (long int)floor((_x)+0.5))
#    define HAVE_LRINT 1
#  elif !defined(__cplusplus) && defined(HAVE_LRINT) && !defined(HAVE_DECL_LRINT)
long int lrint(double x);
#    define HAVE_LRINT 1
#  endif

#endif  /* BRLCADBUILD & HAVE_CONFIG_H */

/* provide declaration markers for header externals */
#ifndef __BEGIN_DECLS
#  ifdef __cplusplus
#    define __BEGIN_DECLS   extern "C" {   /**< if C++, set to extern "C" { */
#    define __END_DECLS     }              /**< if C++, set to } */
#  else
#    define __BEGIN_DECLS /**< if C++, set to extern "C" { */
#    define __END_DECLS   /**< if C++, set to } */
#  endif
#endif

/**
 * Functions local to one file IN A LIBRARY should be declared HIDDEN.
 * Disabling the static classifier is sometimes helpful for debugging.
 * It can help prevent some compilers from inlining functions that one
 * might want to set a breakpoint on.  Do not use on variables.
 */
#if !defined(HIDDEN)
#  if defined(NDEBUG)
#    define HIDDEN	static
#  else
#    define HIDDEN	/***/
#  endif
#endif

/* ANSI c89 does not allow the 'inline' keyword, check if GNU inline
 * rules are in effect.
 *
 * TODO: test removal of __STRICT_ANSI__ on Windows.
 */
#if !defined __cplusplus && (defined(__STRICT_ANSI__) || defined(__GNUC_GNU_INLINE__))
#  ifndef inline
#    define inline /***/
#  endif
#endif

/** Find and return the maximum value */
#ifndef FMAX
#  define FMAX(a, b)	(((a)>(b))?(a):(b))
#endif
/** Find and return the minimum value */
#ifndef FMIN
#  define FMIN(a, b)	(((a)<(b))?(a):(b))
#endif

/**
 * C99 does not provide a ssize_t even though it is provided by SUS97.
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

/**
 * Provide a means to conveniently test the version of the GNU
 * compiler.  Use it like this:
 *
 * @code
 * #if GCC_PREREQ(2,8)
 * ... code requiring gcc 2.8 or later ...
 * #endif
 * @endcode
 *
 * WARNING: THIS MACRO IS CONSIDERED PRIVATE AND SHOULD NOT BE USED
 * OUTSIDE OF THIS HEADER FILE.  DO NOT RELY ON IT.
 */
#ifdef GCC_PREREQ
#  warning "GCC_PREREQ unexpectedly defined.  Ensure common.h is included first."
#  undef GCC_PREREQ
#endif
#if defined __GNUC__
#  define GCC_PREREQ(major, minor) __GNUC__ > (major) || (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor))
#else
#  define GCC_PREREQ(major, minor) 0
#endif

/**
 * Provide a means to conveniently test the version of the Intel
 * compiler.  Use it like this:
 *
 * @code
 * #if ICC_PREREQ(800)
 * ... code requiring icc 8.0 or later ...
 * #endif
 * @endcode
 *
 * WARNING: THIS MACRO IS CONSIDERED PRIVATE AND SHOULD NOT BE USED
 * OUTSIDE OF THIS HEADER FILE.  DO NOT RELY ON IT.
 */
/* provide a means to conveniently test the version of ICC */
#ifdef ICC_PREREQ
#  warning "ICC_PREREQ unexpectedly defined.  Ensure common.h is included first."
#  undef ICC_PREREQ
#endif
#if defined __INTEL_COMPILER
#  define ICC_PREREQ(version) (__INTEL_COMPILER >= (version))
#else
#  define ICC_PREREQ(version) 0
#endif

/* This is so we can use gcc's "format string vs arguments"-check for
 * various printf-like functions, and still maintain compatibility.
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
#ifdef UNUSED
#  warning "UNUSED unexpectedly defined.  Ensure common.h is included first."
#  undef UNUSED
#endif
#if GCC_PREREQ(2, 5)
   /* GCC-style compilers have an attribute */
#  define UNUSED(parameter) UNUSED_ ## parameter __attribute__((unused))
#elif defined(__cplusplus)
   /* C++ allows the name to go away */
#  define UNUSED(parameter) /* parameter */
#else
   /* some are asserted when !NDEBUG */
#  define UNUSED(parameter) (parameter)
#endif

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
#ifdef LIKELY
#  undef LIKELY
#  warning "LIKELY unexpectedly defined.  Ensure common.h is included first."
#endif
#if GCC_PREREQ(3, 0) || ICC_PREREQ(800)
#  define LIKELY(expression) __builtin_expect((expression), 1)
#else
#  define LIKELY(expression) (expression)
#endif

/**
 * UNLIKELY provides a common mechanism for providing branch
 * prediction hints to the compiler so that it can better optimize.
 * It should be used when it's exceptionally unlikely that a given code
 * path will ever be executed.  Use it like this:
 *
 *  if (UNLIKELY(x == 0)) {
 *    ... unexpected code path ...
 *  }
 *
 */
#ifdef UNLIKELY
#  undef UNLIKELY
#  warning "UNLIKELY unexpectedly defined.  Ensure common.h is included first."
#endif
#if GCC_PREREQ(3, 0) || ICC_PREREQ(800)
#  define UNLIKELY(expression) __builtin_expect((expression), 0)
#else
#  define UNLIKELY(expression) (expression)
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
#ifdef DEPRECATED
#  undef DEPRECATED
#  warning "DEPRECATED unexpectedly defined.  Ensure common.h is included first."
#endif
#if GCC_PREREQ(3, 1) || ICC_PREREQ(800)
#  define DEPRECATED __attribute__((deprecated))
#elif defined(_WIN32)
#  define DEPRECATED __declspec(deprecated("This function is DEPRECATED.  Please update code to new API."))
#else
#  define DEPRECATED /* deprecated */
#endif

/* ActiveState Tcl doesn't include this catch in tclPlatDecls.h, so we
 * have to add it for them
 */
#if defined(_MSC_VER) && defined(__STDC__)
#  include <tchar.h>
   /* MSVC++ misses this. */
   typedef _TCHAR TCHAR;
#endif

/* Avoid -Wundef warnings for system headers that use __STDC_VERSION__ without
 * checking if it's defined.
 */
#if !defined(__STDC_VERSION__)
#  define __STDC_VERSION__ 0
#endif

/**
 * globally disable certain warnings.  do NOT add new warnings here
 * without discussion and research.  only warnings that cannot be
 * quieted without objectively decreasing code quality should be
 * added!  even warnings that are innocuous or produce false-positive
 * should be quelled when possible.
 *
 * any warnings added should include a description and justification.
 */
#if defined(_MSC_VER)

/* /W1 warning C4351: new behavior: elements of array '...' will be default initialized
 *
 * i.e., this is the "we now implement constructor member
 * initialization correctly" warning that tells the user an
 * initializer like this:
 *
 * Class::Class() : some_array() {}
 *
 * will now initialize all members of some_array.  previous to
 * MSVC2005, behavior was to not initialize in some cases...
 */
#  pragma warning( disable : 4351 )

/* dubious warnings that are not yet intentionally disabled:
 *
 * /W3 warning C4800: 'int' : forcing value to bool 'true' or 'false' (performance warning)
 *
 * this warning is caused by assigning an int (or other non-boolean
 * value) to a bool like this:
 *
 * int i = 1; bool b = i;
 *
 * there is something to be said for making such assignments explicit,
 * e.g., "b = (i != 0);", but this arguably decreases readability or
 * clarity and the fix has potential for introducing logic errors.
 */
/*#  pragma warning( disable : 4800 ) */

#endif

/**
 * Provide a macro for different treatment of initialized extern const
 * variables between C and C++.  In C the following initialization
 * (definition) is acceptable for external linkage:
 *
 *   const int var = 10;
 *
 * but in C++ const is implicitly internal linkage so it must have
 * extern qualifier:
 *
 *   extern const int var = 10;
 */
#if defined(__cplusplus)
  #define EXTERNVARINIT extern
#else
  #define EXTERNVARINIT
#endif

/**
 * Provide canonical preprocessor stringification.
 *
 * #define abc 123
 * CPP_STR(abc) => "abc"
 */
#ifndef CPP_STR
#  define CPP_STR(x) # x
#endif

/**
 * Provide canonical preprocessor expanded stringification.
 *
 * #define abc 123
 * CPP_XSTR(abc) => "123"
 */
#ifndef CPP_XSTR
#  define CPP_XSTR(x) CPP_STR(x)
#endif

/**
 * Provide canonical preprocessor concatenation.
 *
 * #define abc 123
 * CPP_GLUE(abc, 123) => abc123
 * CPP_STR(CPP_GLUE(abc, 123)) => "CPP_GLUE(abc, 123)"
 * CPP_XSTR(CPP_GLUE(abc, 123)) => "abc123"
 * #define abc123 "xyz"
 * CPP_GLUE(abc, 123) => abc123 => "xyz"
 */
#ifndef CPP_GLUE
#  define CPP_GLUE(a, b) a ## b
#endif

/**
 * Provide canonical preprocessor expanded concatenation.
 *
 * #define abc 123
 * CPP_XGLUE(abc, 123) => 123123
 * CPP_STR(CPP_XGLUE(abc, 123)) => "CPP_XGLUE(abc, 123)"
 * CPP_XSTR(CPP_XGLUE(abc, 123)) => "123123"
 */
#ifndef CPP_XGLUE
#  define CPP_XGLUE(a, b) CPP_GLUE
#endif

/**
 * Provide format specifier string tied to a size (e.g., "%123s")
 *
 * #define STR_LEN 10+1
 * char str[STR_LEN] = {0};
 * scanf(CPP_SCANSIZE(STR_LEN) "\n", str);
 */
#ifndef CPP_SCAN
#  define CPP_SCAN(sz) "%" CPP_XSTR(sz) "s"
#endif

/**
 * Provide the current filename and linenumber as a static
 * preprocessor string in "file"":""line" format (e.g., "file:123").
 */
#ifndef CPP_FILELINE
#  define CPP_FILELINE __FILE__ ":" CPP_XSTR(__LINE__)
#endif


#endif  /* COMMON_H */

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
