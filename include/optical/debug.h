/*                         D E B U G . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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
/** @addtogroup liboptical
 *
 * @brief
 *  Debugging logic for the BRL-CAD Optical Library, LIBOPTICAL.
 *
 */
/** @{ */
/** @file optical/debug.h */

#ifndef OPTICAL_DEBUG_H
#define OPTICAL_DEBUG_H

#include "common.h"
#include "optical/defines.h"

__BEGIN_DECLS

OPTICAL_EXPORT extern int	rdebug;

/* When in production mode, no debug checking is performed, hence the
 * R_DEBUG define causes sections of debug code to go "poof"
 */
#ifdef NO_DEBUG_CHECKING
#	define	R_DEBUG	0
#else
#	define	R_DEBUG	rdebug
#endif


/*
 *  Debugging flags for thr RT program itself.
 *  These flags follow the "-X" (cap X) option to the RT program.
 *  librt debugging is separately controlled.
 */

/* These definitions are each for one bit */
/* Should be reorganized to put most useful ones first */
#define RDEBUG_HITS	0x00000001	/* 1 Print hits used by view() */
#define RDEBUG_MATERIAL	0x00000002	/* 2 Material properties */
#define RDEBUG_SHOWERR	0x00000004	/* 3 Colorful markers on errors */
#define RDEBUG_RTMEM	0x00000008	/* 4 Debug librt mem after startup */
#define RDEBUG_SHADE	0x00000010	/* 5 Shading calculation */
#define RDEBUG_PARSE	0x00000020	/* 6 Command parsing */
#define RDEBUG_LIGHT	0x00000040	/* 7 Debug lighting */
#define RDEBUG_REFRACT	0x00000080	/* 8 Debug reflection & refraction */

#define RDEBUG_STATS	0x00000200	/* 10 Print more statistics */
#define RDEBUG_RTMEM_END 0x00000400	/* 11 Print librt mem use on 'clean' */

/* These will cause binary debugging output */
#define RDEBUG_MISSPLOT	0x20000000	/* 30 plot(5) missed rays to stdout */
#define RDEBUG_RAYWRITE	0x40000000	/* 31 Ray(5V) view rays to stdout */
#define RDEBUG_RAYPLOT	0x80000000	/* 32 plot(5) rays to stdout */

/* Format string for rt_printb() */
#define RDEBUG_FORMAT	\
"\020\040RAYPLOT\037RAYWRITE\036MISSPLOT\
\013RTMEM_END\
\012STATS\010REFRACT\
\7LIGHT\6PARSE\5SHADE\4RTMEM\3SHOWERR\2MATERIAL\1HITS"


/*
 *	A Bit vector to determine how much stuff rt prints when not in
 *	debugging mode.
 *
 */
OPTICAL_EXPORT extern int	rt_verbosity;
/*	   flag_name		value		prints */
#define VERBOSE_LIBVERSIONS  0x00000001	/* Library version strings */
#define VERBOSE_MODELTITLE   0x00000002	/* model title */
#define VERBOSE_TOLERANCE    0x00000004	/* model tolerance */
#define VERBOSE_STATS	     0x00000008	/* stats about rt_gettrees() */
#define VERBOSE_FRAMENUMBER  0x00000010	/* current frame number */
#define VERBOSE_VIEWDETAIL   0x00000020	/* view specifications */
#define VERBOSE_LIGHTINFO    0x00000040	/* scene lights */
#define VERBOSE_INCREMENTAL  0x00000080	/* progressive/incremental state */
#define VERBOSE_MULTICPU     0x00000100	/* #  of CPU's to be used */
#define VERBOSE_OUTPUTFILE   0x00000200	/* name of output image */

#define VERBOSE_FORMAT \
"\012OUTPUTFILE\011MULTICPU\010INCREMENTAL\7LIGHTINFO\6VIEWDETAIL\
\5FRAMENUMBER\4STATS\3TOLERANCE\2MODELTITLE\1LIBVERSIONS"

__END_DECLS

#endif /* OPTICAL_DEBUG_H */

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
