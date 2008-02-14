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
/** @addtogroup fixme */
/** @{ */
/** @file machine.h
 *
 * @brief
 *  This header file defines all the
 *	fundamental data types (lower case names, created with "typedef")
 *  and
 *	fundamental manifest constants (upper case, created with "#define")
 *  used throughout the BRL-CAD Package.  Virtually all other BRL-CAD
 *  header files depend on this header file being included first.
 *
 *  General Symbols and Types Defined -
 *
 *	BU_BITV_SHIFT - log2( bits_wide(bitv_t) ).  Used to determine how
 *	many bits of a bit-vector subscript are index-of-bit in bitv_t
 *	word, and how many bits of the subscript are for word index.
 *	On a 32-bit machine, BU_BITV_SHIFT is 5.
 *      DEPRECATED: needs to be detected at run-time
 *
 *  Parallel Computation Symbols -
 *
 *    These are used only for applications linked with LIBRT and
 *    LIBBU.  XXX These are likely to get new, more descriptive names
 *    sometime, consider them all DEPRECATED.
 *
 *	PARALLEL -
 *		When defined, the code is being compiled for a parallel processor.
 *		This has implications for signal handling, math library
 *		exception handling, etc.
 *
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


#if defined(__alpha)
/********************************
 *				*
 *	  DEC Alpha (AXP)	*
 *				*
 ********************************/
#if !defined(LITTLE_ENDIAN)
	/* Often defined in <alpha/endian.h> */
#	define LITTLE_ENDIAN	1	/* Under the influence of Intel Corp */
#endif
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
#define LITTLE_ENDIAN	1	/* Under the influence of Intel Corp */
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
#define LITTLE_ENDIAN	1	/* Under the influence of Intel Corp */
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
#define LITTLE_ENDIAN	1	/* Under the influence of National Semiconductor */
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


/*
 *  Definitions for big-endian -vs- little-endian.
 *	BIG_ENDIAN:	Byte [0] is on left side of word (msb).
 *	LITTLE_ENDIAN:	Byte [0] is on right side of word (lsb).
 */
#ifdef vax
#  define LITTLE_ENDIAN	1
#endif

#if !defined(BIG_ENDIAN) && !defined(LITTLE_ENDIAN)
#  define BIG_ENDIAN	1	/* The common case */
#endif

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
