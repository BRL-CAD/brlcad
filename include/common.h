/*                        C O M M O N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
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
 *  @author
 *	Christopher Sean Morrison
 *
 *  @par Source
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  $Header$
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

#endif  /* __COMMON_H__ */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

