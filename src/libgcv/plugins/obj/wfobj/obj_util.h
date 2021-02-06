/*                       O B J _ U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 2010-2021 United States Government as represented by
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

#ifndef WFOBJ_OBJ_UTIL_H
#define WFOBJ_OBJ_UTIL_H

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#ifndef WFOBJ_EXPORT
#  if defined(WFOBJ_DLL_EXPORTS) && defined(WFOBJ_DLL_IMPORTS)
#    error "Only WFOBJ_DLL_EXPORTS or WFOBJ_DLL_IMPORTS can be defined, not both."
#  elif defined(STATIC_BUILD)
#    define WFOBJ_EXPORT
#  elif defined(WFOBJ_DLL_EXPORTS)
#    define WFOBJ_EXPORT COMPILER_DLLEXPORT
#  elif defined(WFOBJ_DLL_IMPORTS)
#    define WFOBJ_EXPORT COMPILER_DLLIMPORT
#  else
#    define WFOBJ_EXPORT
#  endif
#endif

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

#ifndef GCC_PREREQ
#if defined __GNUC__
#  define GCC_PREREQ(major, minor) __GNUC__ > (major) || (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor))
#else
#  define GCC_PREREQ(major, minor) 0
#endif
#endif

#ifndef ICC_PREREQ
#if defined __INTEL_COMPILER
#  define ICC_PREREQ(version) (__INTEL_COMPILER >= (version))
#else
#  define ICC_PREREQ(version) 0
#endif
#endif

#ifndef UNLIKELY
#if GCC_PREREQ(3, 0) || ICC_PREREQ(800)
#  define UNLIKELY(expression) __builtin_expect((expression), 0)
#else
#  define UNLIKELY(expression) (expression)
#endif
#endif

#define WFOBJ_GET(_ptr, _type) _ptr = (_type *)calloc(1, sizeof(_type))
#define WFOBJ_PUT(_ptr, _type) do { *(uint8_t *)(_type *)(_ptr) = /*zap*/ 0; *((uint32_t *)_ptr) = 0xFFFFFFFF;  free(_ptr); _ptr = NULL; } while (0)

__BEGIN_DECLS
char *wfobj_strdup(const char *cp);
size_t wfobj_strlcpy(char *dst, const char *src, size_t size);
__END_DECLS

#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
