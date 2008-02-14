/*                       M A C H I N E . H
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
/** @addtogroup deprecated */
/** @{ */
/** @file machine.h
 *
 * @deprecated
 * The machine.h header is DEPRECATED -- use bu.h instead.
 *
 */

#ifndef __MACHINE_H__
#define __MACHINE_H__

#include "common.h"


/**********************************
 *                                *
 *  Machine specific definitions  *
 *  Choose for maximum speed      *
 *				  *
 **********************************/


#ifdef _WIN32
/********************************
 *				*
 *  Windows Windows		*
 *				*
 ********************************/
/* assume only one processor for now */
#endif /* _WIN32 */


#ifdef HEP
/********************************
 *				*
 *  Denelcor HEP H-1000		*
 *				*
 ********************************/
#define PARALLEL	1
#endif


#if defined(alliant) && !defined(i860)
/********************************
 *				*
 *	Alliant FX/8		*
 *				*
 ********************************/
#define PARALLEL	1
#endif


#if defined(alliant) && defined(i860)
/********************************
 *				*
 *	Alliant FX/2800		*
 *				*
 ********************************/
#define PARALLEL	1
#endif


#ifdef CRAY
/********************************
 *				*
 *  Cray-X/MP, COS or UNICOS	*
 *  Cray-2 under "UNICOS"	*
 *				*
 ********************************/
#define PARALLEL	1
#endif /* CRAY */

#if defined(convex) || defined(__convex__)
/********************************
 *				*
 *  Convex C1 & C2		*
 *				*
 ********************************/
#define PARALLEL	1
#endif

#ifdef ardent
/********************************
 *				*
 *  Stardent (formerly Ardent) 	*
 *  "Titan" Workstation		*
 *				*
 ********************************/
#define PARALLEL	1
#endif

#ifdef __stardent
/********************************
 *				*
 *  Stardent VISTRA Workstation	*
 *  based on Intel i860 chip	*
 *				*
 ********************************/
#define __unix	1		/* It really is unix */
#endif

#if	(defined(__sgi) && defined(__mips))
/* Strict ANSI C does not define CPP symbols that don't start with __ */
#	define sgi	1
#	define mips	1
#endif
#if	(defined(sgi) && defined(mips))
/********************************
 *				*
 *  SGI 4D, multi-processor	*
 *				*
 ********************************/
#define PARALLEL	1
#endif

#ifdef apollo
/********************************
 *				*
 *  Apollo			*
 *  with SR 10			*
 *				*
 ********************************/
#endif


#ifdef n16
/********************************
 *				*
 *     Encore Multi-Max		*
 *				*
 ********************************/
#define PARALLEL	1
#endif


#if defined(SUNOS) && SUNOS >= 50
/********************************
 *				*
 *   Sun Running Solaris 2.X    *
 *   aka SunOS 5.X              *
 *				*
 ********************************/
#define PARALLEL	1
#endif

#if defined(hppa)
/********************************
 *				*
 *   HP 9000/700                *
 *   Running HP-UX 9.1          *
 *				*
 ********************************/
#endif

#ifdef __APPLE__
/********************************
 *                              *
 *      Mac OS X                *
 *                              *
 ********************************/
#define PARALLEL        1
#endif

#ifdef __sp3__
/********************************
 *                              *
 *      IBM SP3                 *
 *                              *
 ********************************/
#define	PARALLEL	1
#endif

#ifdef __ia64__
/********************************
 *                              *
 *      SGI Altix               *
 *                              *
 ********************************/
#define	PARALLEL	1
#endif

#if defined(__sparc64__) && !defined(__FreeBSD__)
/********************************
 *                              *
 *      Sparc 64       		*
 *                              *
 ********************************/
#define	PARALLEL	1
#endif

#if defined(linux) && defined(__x86_64__)
/********************************
 *                              *
 *      AMD Opteron Linux       *
 *                              *
 ********************************/
#define	PARALLEL	1
#endif

#if defined (linux) && !defined(__ia64__) && !defined(__x86_64__) && !defined(__sparc64__)
/********************************
 *                              *
 *        Linux on IA32         *
 *                              *
 ********************************/
#define PARALLEL        1
#endif /* linux */

/********************************
 *                              *
 *    FreeBSD/NetBSD/OpenBSD    *
 *                              *
 ********************************/
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
# define	PARALLEL	1
/* amd64 */
#endif /* BSD */

#endif  /* __MACHINE_H__ */

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
