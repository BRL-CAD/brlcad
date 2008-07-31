/*                        C O M M O N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
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
#  ifdef _WIN32
#    include "config_win.h"
#  else
#    include "brlcad_config.h"
#  endif
#endif  /* _WIN32 */

/* provide declaration markers for header externals */
#ifdef __cplusplus
#  define __BEGIN_DECLS   extern "C" {
#  define __END_DECLS     }
#else
#  define __BEGIN_DECLS
#  define __END_DECLS
#endif

/* _O_TEMPORARY on Windows removes file when last descriptor is closed */
#ifndef O_TEMPORARY
#  define O_TEMPORARY 0
#endif

/* _O_BINARY on Windows indicates whether to use binary or text (default) I/O */
#ifndef O_BINARY
#  define O_BINARY 0
#endif

/* Functions local to one file should be declared HIDDEN.  This is
 * sometimes helpful to debuggers.
 */
#if !defined(HIDDEN)
#  if defined(lint)
#    define HIDDEN	static
#  else
#    define HIDDEN	/***/
#  endif
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
