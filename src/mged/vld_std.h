/*                       V L D _ S T D . H
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file vld_std.h
	std.h -- Douglas A. Gwyn's standard C programming definitions

	Prerequisites:	<math.h> (if you invoke Round())
			<string.h> (if you invoke StrEq())

	last edit:	90/10/26	D A Gwyn

	SCCS ID:	@(#)std.h	1.36

	The master source file is to be modified only by Douglas A. Gwyn
	<Gwyn@BRL.MIL>.  When installing a VLD/VMB software distribution,
	this file may need to be tailored slightly to fit the target system.
	Usually this just involves enabling some of the "kludges for deficient
	C implementations" at the end of this file.
*/

#ifndef	VLD_STD_H_
#define	VLD_STD_H_			/* once-only latch */

/* Extended data types */

typedef int	bool;			/* Boolean data */
#define 	false	0
#define 	true	1

typedef int	bs_type;		/* 3-way "bug/status" result type */
#define 	bs_good	1
#define 	bs_bad	0
#define 	bs_ugly	(-1)

/* ANSI C definitions */

/* Defense against some silly systems defining __STDC__ to random things. */
#ifdef STD_C
#undef STD_C
#endif
#ifdef __STDC__
#if __STDC__ > 0
#define	STD_C	__STDC__		/* use this instead of __STDC__ */
#endif
#endif

#ifdef STD_C
typedef void	*pointer;		/* generic pointer */
#else
typedef char	*pointer;		/* generic pointer */
#define	const		/* nothing */	/* ANSI C type qualifier */
/* There really is no substitute for the following, but these might work: */
#define	signed		/* nothing */	/* ANSI C type specifier */
#define	volatile	/* nothing */	/* ANSI C type qualifier */
#endif

#ifndef EXIT_SUCCESS
#define	EXIT_SUCCESS	0
#endif

#ifndef EXIT_FAILURE
#define	EXIT_FAILURE	1
#endif

#ifndef NULL
#define NULL	0			/* null pointer constant, all types */
#endif

/* Universal constants */

#define DEGRAD	57.2957795130823208767981548141051703324054724665642
					/* degrees per radian */
#define	E	2.71828182845904523536028747135266249775724709369996
					/* base of natural logs */
#define	GAMMA	0.57721566490153286061
					/* Euler's constant */
#define LOG10E	0.43429448190325182765112891891660508229439700580367
					/* log of e to the base 10 */
#define PHI	1.618033988749894848204586834365638117720309180
					/* golden ratio */
#if !defined(PI)	/* sometimes found in math.h */
#define PI	3.14159265358979323846264338327950288419716939937511
					/* ratio of circumf. to diam. */
#endif

/* Useful macros */

/*
	The comment "UNSAFE" means that the macro argument(s) may be evaluated
	more than once, so the programmer must realize that the macro doesn't
	quite act like a genuine function.  This matters only when evaluating
	an argument produces "side effects".
*/

/* arbitrary numerical arguments and value: */
#define Abs( x )	((x) < 0 ? -(x) : (x))			/* UNSAFE */
#define Max( a, b )	((a) > (b) ? (a) : (b))			/* UNSAFE */
#define Min( a, b )	((a) < (b) ? (a) : (b))			/* UNSAFE */

/* floating-point arguments and value: */
#define Round( d )	floor( (d) + 0.5 )		/* requires <math.h> */

/* arbitrary numerical arguments, integer value: */
#define	Sgn( x )	((x) == 0 ? 0 : (x) > 0 ? 1 : -1)	/* UNSAFE */

/* string arguments, boolean value: */
#ifdef gould	/* UTX-32 2.0 compiler has problems with "..."[] */
#define	StrEq( a, b )	(strcmp( a, b ) == 0)			/* UNSAFE */
#else
#define	StrEq( a, b )	(*(a) == *(b) && strcmp( a, b ) == 0)	/* UNSAFE */
#endif

/* array argument, integer value: */
#define	Elements( a )	(sizeof a / sizeof a[0])

/* integer (or character) arguments and value: */
#define fromhostc( c )	(c)		/* map host char set to ASCII */
#define tohostc( c )	(c)		/* map ASCII to host char set */
#define tonumber( c )	((c) - '0')	/* convt digit char to number */
#define todigit( n )	((n) + '0')	/* convt digit number to char */

/* weird macros for special tricks with source code symbols: */
#ifdef STD_C
#define	PASTE( a, b )	a ## b
					/* paste together two token strings */
#define	STRINGIZE( s )	# s
					/* convert tokens to string literal */
#else
/* Q8JOIN is for internal <std.h> use only: */
#define	Q8JOIN( s )	s
#define	PASTE( a, b )	Q8JOIN(a)b
					/* paste together two token strings */
	/* WARNING:  This version of PASTE may expand its arguments
	   before pasting, unlike the Standard C version. */
#define	STRINGIZE( s )	"s"		/* (Reiser cpp behavior assumed) */
					/* convert tokens to string literal */
	/* WARNING:  This version of STRINGIZE does not properly handle " and
	   \ characters in character-constant and string-literal tokens. */
#endif

#if defined(sgi) && defined(mips)	/* missing from <signal.h>: */
extern void	(*signal(int, void (*)(int)))(int);
#endif

#endif	/* VLD_STD_H_ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
