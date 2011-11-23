/*                          S Y S V . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @file sysv.h
 *
 * The "System V" library is a compatibility library for older systems
 * that may not have various BSD and POSIX interfaces that BRL-CAD
 * expects.  Only the portions that are missing should end up getting
 * declared and compiled.
 *
 * The majority of the code in this compatibility library comes from
 * sources external to BRL-CAD and as such, refer to each individual
 * file for license information.  All code included is either covered
 * by the LGPL, BSD license, or is in the public domain.
 *
 */

#ifndef __SYSV_H__
#define __SYSV_H__

#include "common.h"

/* for size_t */
#include <stddef.h>

__BEGIN_DECLS

#ifndef SYSV_EXPORT
#  if defined(SYSV_DLL_EXPORTS) && defined(SYSV_DLL_IMPORTS)
#    error "Only SYSV_DLL_EXPORTS or SYSV_DLL_IMPORTS can be defined, not both."
#  elif defined(SYSV_DLL_EXPORTS)
#    define SYSV_EXPORT __declspec(dllexport)
#  elif defined(SYSV_DLL_IMPORTS)
#    define SYSV_EXPORT __declspec(dllimport)
#  else
#    define SYSV_EXPORT
#  endif
#endif

#ifndef HAVE_MEMSET
SYSV_EXPORT extern void *memset(void *s, int c, size_t n);
#endif

#ifndef HAVE_STRCHR
SYSV_EXPORT extern char *strchr(char *sp, char c);
#endif

#ifndef HAVE_STRDUP
SYSV_EXPORT extern char *strdup(const char *cp);
#endif

#ifndef HAVE_STRSEP
SYSV_EXPORT extern char *strsep(char **stringp, const char *delim);
#endif

#ifndef HAVE_STRTOK
SYSV_EXPORT extern char *strtok(char *s, const char *delim);
#endif

__END_DECLS

#endif /* __SYSV_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
