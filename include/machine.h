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
 *  Many of these fundamental data types are machine (vendor) dependent.
 *  Some may assume different values on the same machine, depending on
 *  which version of the compiler is being used.
 *
 *  Additions will need to be made here when porting BRL-CAD to a new machine
 *  which is anything but a 32-bit big-endian uniprocessor.
 *
 *  General Symbols and Types Defined -
 *
 *	HIDDEN - Functions intended to be local to one module should
 *	be declared HIDDEN.  For production use, and lint, it will be
 *	defined as "static", but for debugging it can be defined as
 *	NIL, so that the routine names can be made available to the
 *	debugger.
 *
 *	MAX_FASTF - Very close to the largest value that can be held
 *	by a fastf_t without overflow.  Typically specified as an
 *	integer power of ten, to make the value easy to spot when
 *	printed.
 *      TODO: macro function syntax instead of constant, move to libbn
 *      (DEPRECATED)
 *
 *	SQRT_MAX_FASTF - sqrt(MAX_FASTF), or slightly smaller.  Any
 *	number larger than this, if squared, can be expected to
 *	produce an overflow.
 *      TODO: macro function syntax instead of constant (DEPRECATED)
 *
 *	SMALL_FASTF - Very close to the smallest value that can be
 *	represented while still being greater than zero.  Any number
 *	smaller than this (and non-negative) can be considered to be
 *	zero; dividing by such a number can be expected to produce a
 *	divide-by-zero error.  All divisors should be checked against
 *	this value before actual division is performed.
 *      TODO: macro function sytax instead of constant (DEPRECATED)
 *
 *	SQRT_SMALL_FASTF -sqrt(SMALL_FASTF), or slightly larger.  The
 *	value of this is quite a lot larger than that of SMALL_FASTF.
 *	Any number smaller than this, when squared, can be expected to
 *	produce a zero result.
 *      TODO: macro function syntax instead of constant (DEPRECATED)
 *
 *	BITV_SHIFT - log2( bits_wide(bitv_t) ).  Used to determine how
 *	many bits of a bit-vector subscript are index-of-bit in bitv_t
 *	word, and how many bits of the subscript are for word index.
 *	On a 32-bit machine, BITV_SHIFT is 5.
 *      DEPRECATED: needs to be detected at run-time
 *
 *	XXX The BYTE_ORDER handling needs to change to match the POSIX
 *	XXX recommendations.
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
 *	MAX_PSW -
 *		The maximum number of processors that can be expected on
 *		this hardware.  Used to allocate application-specific
 *		per-processor tables.
 *		The actual number of processors is found at runtime by calling
 *		rt_avail_cpus().
 *              TODO: moving to a libbu function (DEPRECATED)
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
#define BITV_SHIFT	5
/* assume only one processor for now */
#define MAX_PSW	4
#endif /* _WIN32 */


#ifdef HEP
/********************************
 *				*
 *  Denelcor HEP H-1000		*
 *				*
 ********************************/
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		128	/* Max number of process streams */
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
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		1	/* only one processor, max */
#endif


#if defined(alliant) && !defined(i860)
/********************************
 *				*
 *	Alliant FX/8		*
 *				*
 ********************************/
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		8	/* Max number of processors */
#define PARALLEL	1
#endif


#if defined(alliant) && defined(i860)
/********************************
 *				*
 *	Alliant FX/2800		*
 *				*
 ********************************/
#define LITTLE_ENDIAN	1	/* Under the influence of Intel Corp */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		28	/* Max number of processors */
#define PARALLEL	1
#endif


#ifdef CRAY
/********************************
 *				*
 *  Cray-X/MP, COS or UNICOS	*
 *  Cray-2 under "UNICOS"	*
 *				*
 ********************************/
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		4	/* Max number of processors */
#define PARALLEL	1
#endif /* CRAY */

#if defined(convex) || defined(__convex__)
/********************************
 *				*
 *  Convex C1 & C2		*
 *				*
 ********************************/
#if 1
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#else
#define BITV_SHIFT	5
#endif
#define MAX_PSW		4	/* Max number of processors */
#define PARALLEL	1
#endif

#ifdef ardent
/********************************
 *				*
 *  Stardent (formerly Ardent) 	*
 *  "Titan" Workstation		*
 *				*
 ********************************/
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		4	/* # processors, max */
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
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		1	/* only one processor, max */
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
#if defined( _MIPS_SZLONG ) && _MIPS_SZLONG == 64
#  define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#else
#  define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#endif
#define MAX_PSW		1024
#define PARALLEL	1
#endif

#ifdef apollo
/********************************
 *				*
 *  Apollo			*
 *  with SR 10			*
 *				*
 ********************************/
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		1	/* only one processor, max */
#endif


#ifdef n16
/********************************
 *				*
 *     Encore Multi-Max		*
 *				*
 ********************************/
#define LITTLE_ENDIAN	1	/* Under the influence of National Semiconductor */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		32	/* This number is uncertain */
#define PARALLEL	1
#endif


#if defined(SUNOS) && SUNOS >= 50
/********************************
 *				*
 *   Sun Running Solaris 2.X    *
 *   aka SunOS 5.X              *
 *				*
 ********************************/
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		256	/* need to increase this for Super Dragon? */
#define PARALLEL	1
#endif

#if defined(hppa)
/********************************
 *				*
 *   HP 9000/700                *
 *   Running HP-UX 9.1          *
 *				*
 ********************************/
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		1	/* only one processor, max */
#endif

#ifdef __APPLE__
/********************************
 *                              *
 *      Mac OS X                *
 *                              *
 ********************************/
#define BITV_SHIFT      5       /* log2( bits_wide(bitv_t) ) */
#define MAX_PSW         512       /* Unused, but useful for thread debugging */
#define PARALLEL        1
#endif

#ifdef __sp3__
/********************************
 *                              *
 *      IBM SP3                 *
 *                              *
 ********************************/
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		32     	/* they can go 32-way per single image */
#define	PARALLEL	1
#endif

#ifdef __ia64__
/********************************
 *                              *
 *      SGI Altix               *
 *                              *
 ********************************/
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		256
#define	PARALLEL	1
#endif

#if defined(__sparc64__) && !defined(__FreeBSD__)
/********************************
 *                              *
 *      Sparc 64       		*
 *                              *
 ********************************/
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		256
#define	PARALLEL	1
#endif

#if defined(linux) && defined(__x86_64__)
/********************************
 *                              *
 *      AMD Opteron Linux       *
 *                              *
 ********************************/
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		256
#define	PARALLEL	1
#endif

#if defined (linux) && !defined(__ia64__) && !defined(__x86_64__) && !defined(__sparc64__)
/********************************
 *                              *
 *        Linux on IA32         *
 *                              *
 ********************************/
#define BITV_SHIFT      5      /* log2( bits_wide(bitv_t) ) */
#define MAX_PSW         16
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
# if defined(__x86_64__) || defined(__sparc64__) || defined(__ia64__)
#  define BITV_SHIFT	6
#  define MAX_PSW		256
/* ia32 */
# else
#  define BITV_SHIFT	5
#  define MAX_PSW	16
# endif
#endif /* BSD */

#ifndef BITV_SHIFT
/********************************
 *				*
 * Default 32-bit uniprocessor	*
 *  VAX, Gould, SUN, SGI	*
 *				*
 ********************************/
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#define MAX_PSW		4	/* allow for a dual core dual */
#endif

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
