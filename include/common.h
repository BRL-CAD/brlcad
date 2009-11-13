/*                        C O M M O N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2009 United States Government as represented by
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
#  if defined(_WIN32) && !defined(__CYGWIN__)
#    include "config_win.h"
#  else
#    include "brlcad_config.h"
#  endif  /* _WIN32 */

/**
 *  Simulates drand48() functionality using rand() which
 *  is assumed to exist everywhere. The range is [0, 1).
 */
#  ifndef HAVE_DRAND48
#    define HAVE_DRAND48 1
#    define drand48() ((double)rand() / (double)(RAND_MAX + 1))
#  endif

#endif  /* BRLCADBUILD & HAVE_CONFIG_H */

/* support ANSI function declarations */
#ifndef USE_PROTOTYPES
#  define USE_PROTOTYPES 1
#endif

/* provide declaration markers for header externals */
#ifdef __cplusplus
#  define __BEGIN_DECLS   extern "C" {
#  define __END_DECLS     }
#else
#  define __BEGIN_DECLS
#  define __END_DECLS
#endif

/* Functions local to one file should be declared HIDDEN.  This is
 * sometimes helpful to debuggers.
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
