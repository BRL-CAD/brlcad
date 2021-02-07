/*                         D E B U G . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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

OPTICAL_EXPORT extern unsigned int optical_debug;

/* When in production mode, no debug checking is performed, hence the
 * R_DEBUG define causes sections of debug code to go "poof"
 */
#ifdef NO_DEBUG_CHECKING
#	define	OPTICAL_DEBUG	0
#else
#	define	OPTICAL_DEBUG	optical_debug
#endif


/*
 *  Debugging flags for thr RT program itself.
 *  These flags follow the "-X" (cap X) option to the RT program.
 *  librt debugging is separately controlled.
 */

/* These definitions are each for one bit */
/* Should be reorganized to put most useful ones first */
#define OPTICAL_DEBUG_HITS      0x00000001	/* 1 Print hits used by view() */
#define OPTICAL_DEBUG_MATERIAL  0x00000002	/* 2 Material properties */
#define OPTICAL_DEBUG_SHOWERR   0x00000004	/* 3 Colorful markers on errors */
#define OPTICAL_DEBUG_RTMEM     0x00000008	/* 4 Debug librt mem after startup */
#define OPTICAL_DEBUG_SHADE     0x00000010	/* 5 Shading calculation */
#define OPTICAL_DEBUG_PARSE     0x00000020	/* 6 Command parsing */
#define OPTICAL_DEBUG_LIGHT     0x00000040	/* 7 Debug lighting */
#define OPTICAL_DEBUG_REFRACT   0x00000080	/* 8 Debug reflection & refraction */

#define OPTICAL_DEBUG_STATS     0x00000200	/* 10 Print more statistics */
#define OPTICAL_DEBUG_RTMEM_END 0x00000400	/* 11 Print librt mem use on 'clean' */

/* These will cause binary debugging output */
#define OPTICAL_DEBUG_MISSPLOT  0x20000000	/* 30 plot(5) missed rays to stdout */
#define OPTICAL_DEBUG_RAYWRITE  0x40000000	/* 31 Ray(5V) view rays to stdout */
#define OPTICAL_DEBUG_RAYPLOT   0x80000000	/* 32 plot(5) rays to stdout */

#define OPTICAL_DEBUG_FORMAT    "\020" /* print hex */ \
    "\040RAYPLOT" \
    "\037RAYWRITE" \
    "\036MISSPLOT" \
    /* many unused */ \
    "\013RTMEM_END" \
    "\012STATS" \
    "\011UNUSED" \
    "\010REFRACT" \
    "\7LIGHT" \
    "\6PARSE" \
    "\5SHADE" \
    "\4RTMEM" \
    "\3SHOWERR" \
    "\2MATERIAL" \
    "\1HITS"

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
