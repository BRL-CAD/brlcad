/*                    R L E _ C O N F I G . H
 * BRL-CAD
 *
 * Copyright (c) 2004 United States Government as represented by the
 * U.S. Army Research Laboratory.
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
/** @file rle_config.h
 *
 */

/* rle_config.h
 * 
 * All of the relevant #defines that the Utah Raster Toolkit needs should be
 * done in "machine.h" when the toolkit is compiled with BRLCAD
 * 
 */
#include "machine.h"

#define CONST_DECL const

#if __STDC__
#	define VOID_STAR		/* for the Utah Raster Toolkit */
#endif

#if __STDC__
#	define USE_STDARG	1	/* Use <stdarg.h> not <varargs.h> */
#endif

#if BSD && !SYSV
#  define SYS_TIME_H	/* time_t is defined through sys/time.h not time.h */
#endif

#if !BSD && SYSV
#	define rindex strrchr
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
