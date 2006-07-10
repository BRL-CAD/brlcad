/*                       M A C H I N E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file machine.h
 *
 *  This header file defines all the
 *
 *	fundamental data types (lower case names, created with "typedef")
 *
 *  and
 *
 *	fundamental manifest constants (upper case, created with "#define")
 *
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
 *	genptr_t -
 *		A portable way of declaring a "generic" pointer that is
 *		wide enough to point to anything, which can be used on
 *		both ANSI C and K&R C environments.
 *		On some machines, pointers to functions can be wider than
 *		pointers to data bytes, so a declaration of "char *"
 *		isn't generic enough.
 *
 *	fastf_t -
 *		Intended to be the fastest floating point data type on
 *		the current machine, with at least 64 bits of precision.
 *		On 16 and 32 bit machine, this is typically "double",
 *		but on 64 bit machines, it is often "float".
 *		Virtually all floating point variables (and more complicated
 *		data types, like vect_t and mat_t) are defined as fastf_t.
 *		The one exception is when a subroutine return is a floating
 *		point value;  that is always declared as "double".
 *
 *	LOCAL -
 *		The fastest storage class for local variables within a
 *		subroutine.  On parallel machines, this needs to be "auto",
 *		but on serial machines there can sometimes be a performance
 *		advantage to using "static".
 *
 *	FAST -
 *		The fastest storage class for fastf_t variables.
 *		On most machines with abundant registers, this is "register",
 *		but on machines like the VAX with only 3 "register double"s
 *		available to C programmers, it is set to LOCAL.
 *		Thus, declaring a fast temporary fastf_t variable is done like:
 *			FAST fastf_t var;
 *
 *	HIDDEN -
 *		Functions intended to be local to one module should be
 *		declared HIDDEN.  For production use, and lint, it will
 *		be defined as "static", but for debugging it can be defined
 *		as NIL, so that the routine names can be made available
 *		to the debugger.
 *
 *	MAX_FASTF -
 *		Very close to the largest value that can be held by a
 *		fastf_t without overflow.  Typically specified as an
 *		integer power of ten, to make the value easy to spot when
 *		printed.
 *
 *	SQRT_MAX_FASTF -
 *		sqrt(MAX_FASTF), or slightly smaller.  Any number larger than
 *		this, if squared, can be expected to produce an overflow.
 *
 *	SMALL_FASTF -
 *		Very close to the smallest value that can be represented
 *		while still being greater than zero.  Any number smaller
 *		than this (and non-negative) can be considered to be
 *		zero;  dividing by such a number can be expected to produce
 *		a divide-by-zero error.
 *		All divisors should be checked against this value before
 *		actual division is performed.
 *
 *	SQRT_SMALL_FASTF -
 *		sqrt(SMALL_FASTF), or slightly larger.  The value of this
 *		is quite a lot larger than that of SMALL_FASTF.
 *		Any number smaller than this, when squared, can be expected
 *		to produce a zero result.
 *
 *	bzero(ptr,n) -
 *		Defined to be the fasted system-specific method for
 *		zeroing a block of 'n' bytes, where the pointer has
 *		arbitrary byte alignment.
 *
 *	bcopy(from,to,n) -
 *		Defined to be the fastest system-specific method for
 *		copying a block of 'n' bytes, where both the "from" and
 *		"to" pointers have arbitrary byte alignment.
 *
 *	bitv_t -
 *		The widest fast integer type available, used to implement bit
 *		vectors.  On most machines, this is "long", but on some
 *		machines a vendor-specific type such as "long long" can
 *		give access to wider integers.
 *
 *	BITV_SHIFT -
 *		log2( bits_wide(bitv_t) ).  Used to determine how many
 *		bits of a bit-vector subscript are index-of-bit in bitv_t
 *		word, and how many bits of the subscript are for word index.
 *		On a 32-bit machine, BITV_SHIFT is 5.
 *
 *	XXX The BYTE_ORDER handling needs to change to match the POSIX
 *	XXX recommendations.
 *
 *  PARALLEL Symbols Defined -
 *    These are used only for applications linked with LIBRT,
 *    and interact heavily with the support routines in librt/machine.c
 *    XXX These are likely to get new, more descriptive names sometime.
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
 *
 *	DEFAULT_PSW -
 *		The number of processors to use when the user has not
 *		specifically indicated the number of processors desired.
 *		On some machines like the Alliant, this should be MAX_PSW,
 *		because the parallel complex is allocated as a unit.
 *		On timesharing machines like the Cray, this should be 1,
 *		because running multi-tasking consumes special resources
 *		(and sometimes requires special queues/privs), so ordinary
 *		runs should just stay serial.
 *
 *	MALLOC_NOT_MP_SAFE -
 *		Defined when the system malloc() routine can not be
 *		safely used in a multi-processor (MP) execution.
 *		If defined, LIBBU will protect with BU_SEM_SYSCALL.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *
 *  Include Sequencing -
 *	#include "machine.h"
 *	#include "bu.h"
 *
 *  Libraries Used -
 *	LIBBU LIBBU_LIBES -lm -lc
 *
 *  $Header$
 */

#ifndef MACHINE_H
#define MACHINE_H seen

#include "common.h"

/* needed for FOPEN_MAX */
#include <stdio.h>


/*
 * Figure out the maximum number of files that can simultaneously be open
 * by a process.
 */

#if !defined(FOPEN_MAX) && defined(_NFILE)
#	define FOPEN_MAX	_NFILE
#endif
#if !defined(FOPEN_MAX) && defined(NOFILE)
#	define FOPEN_MAX	NOFILE
#endif
#if !defined(FOPEN_MAX) && defined(OPEN_MAX)
#	define FOPEN_MAX	OPEN_MAX
#endif
#if !defined(FOPEN_MAX) && defined(_SYS_OPEN)
#	define FOPEN_MAX	_SYS_OPEN
#endif
#if !defined(FOPEN_MAX)
#	define FOPEN_MAX	32
#endif

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
typedef double fastf_t;
#define LOCAL auto
#define FAST register
typedef long bitv_t;
#define BITV_SHIFT	5
/* assume only one processor for now */
#define MAX_PSW	4
#define DEFAULT_PSW	1
#define MALLOC_NOT_MP_SAFE 1

#endif /* _WIN32 */


#ifdef HEP
/********************************
 *				*
 *  Denelcor HEP H-1000		*
 *				*
 ********************************/
#define IBM_FLOAT 1		/* Uses IBM style floating point */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		128	/* Max number of process streams */
#define DEFAULT_PSW	MAX_PSW
#define PARALLEL	1
#endif


#if defined(__alpha)
/********************************
 *				*
 *	  DEC Alpha (AXP)	*
 *				*
 ********************************/
#define IEEE_FLOAT 1		/* Uses IEEE style floating point */
#if !defined(LITTLE_ENDIAN)
	/* Often defined in <alpha/endian.h> */
#	define LITTLE_ENDIAN	1	/* Under the influence of Intel Corp */
#endif
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		1	/* only one processor, max */
#define DEFAULT_PSW	1
#endif


#if defined(alliant) && !defined(i860)
/********************************
 *				*
 *	Alliant FX/8		*
 *				*
 ********************************/
#define IEEE_FLOAT 1		/* Uses IEEE style floating point */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		8	/* Max number of processors */
#define DEFAULT_PSW	MAX_PSW
#define PARALLEL	1

#endif


#if defined(alliant) && defined(i860)
/********************************
 *				*
 *	Alliant FX/2800		*
 *				*
 ********************************/
#define IEEE_FLOAT 1		/* Uses IEEE style floating point */
#define LITTLE_ENDIAN	1	/* Under the influence of Intel Corp */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		28	/* Max number of processors */
#define DEFAULT_PSW	MAX_PSW
#define PARALLEL	1

#endif


#ifdef CRAY
/********************************
 *				*
 *  Cray-X/MP, COS or UNICOS	*
 *  Cray-2 under "UNICOS"	*
 *				*
 ********************************/
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		4	/* Max number of processors */
#define DEFAULT_PSW	1
#define PARALLEL	1

#  if 0
#	define CRAY_COS	1	/* Running on Cray under COS w/bugs */
#  endif
#endif

#if defined(convex) || defined(__convex__)
/********************************
 *				*
 *  Convex C1 & C2		*
 *				*
 ********************************/
typedef double		fastf_t;/* double|float, "Fastest" float type */
#define LOCAL		auto	/* static|auto, for serial|parallel cpu */
#define FAST		register /* LOCAL|register, for fastest floats */
#if 1
typedef long long	bitv_t;	/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#else
typedef long		bitv_t;
#define BITV_SHIFT	5
#endif

#define MAX_PSW		4	/* Max number of processors */
#define DEFAULT_PSW	1	/* for now */
#define PARALLEL	1
#endif

#ifdef ardent
/********************************
 *				*
 *  Stardent (formerly Ardent) 	*
 *  "Titan" Workstation		*
 *				*
 ********************************/
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* for parallel cpus */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		4	/* # processors, max */
#define DEFAULT_PSW	1
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
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* for parallel cpus */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		1	/* only one processor, max */
#define DEFAULT_PSW	1
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
#define IEEE_FLOAT 1		/* Uses IEEE style floating point */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#if defined( _MIPS_SZLONG ) && _MIPS_SZLONG == 64
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#else
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */
#endif
#define const	const

#define MAX_PSW		1024
#define DEFAULT_PSW	MAX_PSW
#define PARALLEL	1

#endif

#ifdef apollo
/********************************
 *				*
 *  Apollo			*
 *  with SR 10			*
 *				*
 ********************************/
#if __STDC__
#define const	/**/		/* Does not support const keyword */
#define const	/**/		/* Does not support const keyword */
#endif

typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	static		/* static|auto, for serial|parallel cpu */
#define FAST	LOCAL		/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		1	/* only one processor, max */
#define DEFAULT_PSW	1
#define MALLOC_NOT_MP_SAFE 1

#endif


#ifdef n16
/********************************
 *				*
 *     Encore Multi-Max		*
 *				*
 ********************************/
#define IEEE_FLOAT	1	/* Uses IEEE style floating point */
#define LITTLE_ENDIAN	1	/* Under the influence of National Semiconductor */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		32	/* This number is uncertain */
#define DEFAULT_PSW	1
#define PARALLEL	1
#define MALLOC_NOT_MP_SAFE 1
#endif

#ifdef ipsc860
/********************************
 *				*
 *   Intel iPSC/860 Hypercube	*
 *				*
 ********************************/
/* icc compiler gets confused on const typedefs */
#define	const	/**/
#define	const	/**/
#define MALLOC_NOT_MP_SAFE 1
#endif

#if defined(SUNOS) && SUNOS >= 50
/********************************
 *				*
 *   Sun Running Solaris 2.X    *
 *   aka SunOS 5.X              *
 *				*
 ********************************/

#define IEEE_FLOAT 1		/* Uses IEEE style floating point */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		256	/* need to increase this for Super Dragon? */
#define DEFAULT_PSW	bu_avail_cpus()
#define PARALLEL	1

#endif

#if defined(hppa)
/********************************
 *				*
 *   HP 9000/700                *
 *   Running HP-UX 9.1          *
 *				*
 ********************************/

#define IEEE_FLOAT 1		/* Uses IEEE style floating point */
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	auto		/* static|auto, for serial|parallel cpu */
#define FAST	register	/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define const   /**/            /* Does not support const keyword */
#define const   /**/            /* Does not support const keyword */

#define MAX_PSW		1	/* only one processor, max */
#define DEFAULT_PSW	1
#define MALLOC_NOT_MP_SAFE 1

#endif

#ifdef __ppc__
/********************************
 *                              *
 *      Macintosh PowerPC       *
 *                              *
 ********************************/
#define IEEE_FLOAT      1       /* Uses IEEE style floating point */
typedef double  fastf_t;        /* double|float, "Fastest" float type */
#define LOCAL   auto            /* static|auto, for serial|parallel cpu */
#define FAST    register        /* LOCAL|register, for fastest floats */
typedef long    bitv_t;         /* could use long long */
#define BITV_SHIFT      5       /* log2( bits_wide(bitv_t) ) */
#define MAX_PSW         512       /* Unused, but useful for thread debugging */
#define DEFAULT_PSW     bu_avail_cpus()	/* use as many as we can */
#define PARALLEL        1
/* #define MALLOC_NOT_MP_SAFE 1 -- not confirmed */
#endif

#ifdef __sp3__
/********************************
 *                              *
 *      IBM SP3                 *
 *                              *
 ********************************/
#define IEEE_FLOAT      1       /* Uses IEEE style floating point */
typedef double  fastf_t;        /* double|float, "Fastest" float type */
#define LOCAL   auto            /* static|auto, for serial|parallel cpu */
#define FAST    register        /* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#if 1	/* Multi-CPU SP3 build */
#	define MAX_PSW		32     	/* they can go 32-way per single image */
#	define DEFAULT_PSW	bu_avail_cpus()	/* use as many as are configured by default */
#	define	PARALLEL	1
#	define	MALLOC_NOT_MP_SAFE	1	/* XXX Not sure about this */
#else	/* 1 CPU SP3 build */
#	define MAX_PSW		1	/* only one processor, max */
#	define DEFAULT_PSW	1
#endif

#endif

#ifdef __ia64__
/********************************
 *                              *
 *      SGI Altix               *
 *                              *
 ********************************/
#define IEEE_FLOAT      1       /* Uses IEEE style floating point */
typedef double  fastf_t;        /* double|float, "Fastest" float type */
#define LOCAL   auto            /* static|auto, for serial|parallel cpu */
#define FAST    register        /* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */

#if 1	/* Multi-CPU Altix build */
#	define DEFAULT_PSW	bu_avail_cpus()
#	define MAX_PSW		256
#	define	PARALLEL	1
#	define MALLOC_NOT_MP_SAFE	1	/* XXX Not sure about this */
#else	/* 1 CPU Altix build */
#	define MAX_PSW		1	/* only one processor, max */
#	define DEFAULT_PSW	1
#	define MALLOC_NOT_MP_SAFE	1	/* XXX Not sure about this */
#endif

#endif

#if defined(__sparc64__)
/********************************
 *                              *
 *      Sparc 64       		*
 *                              *
 ********************************/
#define IEEE_FLOAT      1       /* Uses IEEE style floating point */
typedef double  fastf_t;        /* double|float, "Fastest" float type */
#define LOCAL   auto            /* static|auto, for serial|parallel cpu */
#define FAST    register        /* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#define DEFAULT_PSW	bu_avail_cpus()
#define MAX_PSW		256
#define	PARALLEL	1
#define MALLOC_NOT_MP_SAFE	1	/* XXX Not sure about this */

#endif


#if defined(linux) && defined(__x86_64__)
/********************************
 *                              *
 *      AMD Opteron Linux       *
 *                              *
 ********************************/
#define IEEE_FLOAT      1       /* Uses IEEE style floating point */
typedef double  fastf_t;        /* double|float, "Fastest" float type */
#define LOCAL   auto            /* static|auto, for serial|parallel cpu */
#define FAST    register        /* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	6	/* log2( bits_wide(bitv_t) ) */
#define DEFAULT_PSW	bu_avail_cpus()
#define MAX_PSW		256
#define	PARALLEL	1
#define MALLOC_NOT_MP_SAFE	1	/* XXX Not sure about this */

#endif

#if defined (linux) && !defined(__ia64__) && !defined(__x86_64__) && !defined(__sparc64__)
/********************************
 *                              *
 *        Linux on IA32         *
 *                              *
 ********************************/
#define IEEE_FLOAT      1      /* Uses IEEE style floating point */
#define BITV_SHIFT      5      /* log2( bits_wide(bitv_t) ) */

typedef double fastf_t;       /* double|float, "Fastest" float type */
typedef long bitv_t;          /* could use long long */

/*
 * Note that by default a Linux installation supports parallel using
 * pthreads. For a 1 cpu installation, toggle these blocks
 */
# if 1 /* multi-cpu linux build */

# define LOCAL auto             /* static|auto, for serial|parallel cpu */
# define FAST register          /* LOCAL|register, for fastest floats */
# define MAX_PSW         16
# define DEFAULT_PSW     bu_avail_cpus()	/* use as many processors as are available */
# define PARALLEL        1
# define MALLOC_NOT_MP_SAFE 1   /* uncertain, but this is safer for now */

# else  /* 1 CPU Linux build */

# define LOCAL static		/* static|auto, for serial|parallel cpu */
# define FAST LOCAL		/* LOCAL|register, for fastest floats */
# define MAX_PSW        1	/* only one processor, max */
# define DEFAULT_PSW	1

# endif
#endif /* linux */

#ifndef LOCAL
/********************************
 *				*
 * Default 32-bit uniprocessor	*
 *  VAX, Gould, SUN, SGI	*
 *				*
 ********************************/
typedef double	fastf_t;	/* double|float, "Fastest" float type */
#define LOCAL	static		/* static|auto, for serial|parallel cpu */
#define FAST	LOCAL		/* LOCAL|register, for fastest floats */
typedef long	bitv_t;		/* largest integer type */
#define BITV_SHIFT	5	/* log2( bits_wide(bitv_t) ) */

#define MAX_PSW		4	/* allow for a dual core dual */
#define DEFAULT_PSW	bu_avail_cpus()	/* use as many as are available by default */

#endif

/*
 *  Definitions for big-endian -vs- little-endian.
 *	BIG_ENDIAN:	Byte [0] is on left side of word (msb).
 *	LITTLE_ENDIAN:	Byte [0] is on right side of word (lsb).
 */
#ifdef vax
# define LITTLE_ENDIAN	1
#endif

#if !defined(BIG_ENDIAN) && !defined(LITTLE_ENDIAN)
# define BIG_ENDIAN	1	/* The common case */
#endif

/*  Bit vector mask */
#define BITV_MASK	((1<<BITV_SHIFT)-1)

/*
 * Definitions about limits of floating point representation
 * Eventually, should be tied to type of hardware (IEEE, IBM, Cray)
 * used to implement the fastf_t type.
 */
#if defined(vax) || (defined(sgi) && !defined(mips))
	/* DEC VAX "D" format, the most restrictive */
#define MAX_FASTF		1.0e37	/* Very close to the largest number */
#define SQRT_MAX_FASTF		1.0e18	/* This squared just avoids overflow */
#define SMALL_FASTF		1.0e-37	/* Anything smaller is zero */
#define SQRT_SMALL_FASTF	1.0e-18	/* This squared gives zero */
#else
	/* IBM format, being the next most restrictive format */
#define MAX_FASTF		1.0e73	/* Very close to the largest number */
#define SQRT_MAX_FASTF		1.0e36	/* This squared just avoids overflow */
#define SMALL_FASTF		1.0e-77	/* Anything smaller is zero */
#if defined(aux)
#  define SQRT_SMALL_FASTF	1.0e-40 /* _doprnt error in libc */
#else
#  define SQRT_SMALL_FASTF	1.0e-39	/* This squared gives zero */
#endif
#endif
#define SMALL			SQRT_SMALL_FASTF

/*
 *  Definition of a "generic" pointer that can hold a pointer to anything.
 *  According to tradition, a (char *) was generic, but the ANSI folks
 *  worry about machines where (int *) might be wider than (char *),
 *  so here is the clean way of handling it.
 */
#if !defined(GENPTR_NULL)
#  if __STDC__
	typedef void	*genptr_t;
#  else
	typedef char	*genptr_t;
#  endif
#  define GENPTR_NULL	((genptr_t)0)
#endif


/* Even in C++ not all compilers know the "bool" keyword yet */
#if !defined(BOOL_T)
#  define BOOL_T int
#endif


/** provide bzero and bcopy */
#if !defined(bzero) && !defined(HAVE_BZERO)
#  include <string.h>
#  define bzero(str,n)		memset( str, 0, n )
#  define bcopy(from,to,count)	memcpy( to, from, count )
#endif

/* Functions local to one file should be declared HIDDEN:  (nil)|static */
/* To aid in using ADB, generally leave this as nil. */
#if !defined(HIDDEN)
# if defined(lint)
#	define HIDDEN	static
# else
#	define HIDDEN	/***/
# endif
#endif

/*
 *  ANSI and POSIX do not seem to have prototypes for the hypot() routine,
 *  but several vendors include it in their -lm math library.
 */
#if defined(_POSIX_SOURCE) && !defined(__USE_MISC)
	/* But the sgi -lm does have a hypot routine so lets use it */
#if defined(__sgi) || defined(__convexc__)
        extern double hypot(double, double);
#else
#  include <math.h>
#  define hypot(x,y)      sqrt( (x)*(x)+(y)*(y) )
#endif
#endif

#if defined(SUNOS) && SUNOS >= 52
#  include <math.h>
        extern double hypot(double, double);
#endif

#endif  /* MACHINE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
