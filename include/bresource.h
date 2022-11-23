/*                     B R E S O U R C E . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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
/** @addtogroup bu_resource
 *
 * BRL-CAD system compatibility wrapper header that provides
 * declarations for standard system RESOURCE LIMIT routines.
 *
 * This header is commonly used in lieu of including the following:
 * sys/resource.h sys/time.h sys/wait.h
 *
 * This header does not belong to any BRL-CAD library but may used by
 * all of them.
 *
 * The following preprocessor variables control header inclusions:
 *   HAVE_SYS_TIME_H
 *   HAVE_SYS_RESOURCE_H
 *   HAVE_SYS_WAIT_H
 */
/** @{ */
/** @file bresource.h */

#ifndef BRESOURCE_H
#define BRESOURCE_H

/* intentionally not included for header independence */
/* #include "common.h" */

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

/* this ugly hack overcomes a c89 + -pedantic-errors bug in glibc <2.9
 * where it raises a warning for a trailing comma when including the
 * sys/resource.h system header.
 *
 * need a better solution that preserves pedantic c89 compilation,
 * ideally without resorting to a sys/resource.h compilation feature
 * test (which will vary with build flags).
 */
#if !defined(__USE_GNU) && defined(__GLIBC__) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ < 9)
#  define __USE_GNU 1
#  define DEFINED_USE_GNU 1
#endif
#ifdef HAVE_SYS_RESOURCE_H
#  include <sys/resource.h>
#endif
#ifdef DEFINED_USE_GNU
#  undef __USE_GNU
#  undef DEFINED_USE_GNU
#endif

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#endif /* BRESOURCE_H */

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
