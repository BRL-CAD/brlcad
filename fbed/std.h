/*
	std.h -- Douglas A. Gwyn's standard C programming definitions
						(UNIX System V version)

	Prerequisite:	<math.h> (if you invoke Round())

	last edit:	86/05/12	D A Gwyn

	SCCS ID:	@(#)std.h	1.16

	This file is to be modified by the VMB Software Development Team
	leader only.  Currently, this is Douglas A. Gwyn <Gwyn@BRL.ARPA>.
*/

#ifndef	_VLD_STD_H_
#define	_VLD_STD_H_			/* once-only latch */

/* Extended data types */

#ifndef NULL
#define NULL	0			/* null pointer, all types */
#endif

typedef int bool;			/* Boolean data */
#define 	false	0
#define 	true	1

#if defined(mips) && ! defined(IRIX3_3)
#define IRIX3_3		1	/* Assume running release 3.3 or later. */
#endif

typedef char *pointer;		/* generic pointer (void *) */

#if !defined(__STDC__)
#define	const		/* nothing */	/* (undefine for ANSI C) */
#define	signed		/* nothing */	/* (undefine for ANSI C) */
#define	volatile	/* nothing */	/* (undefine for ANSI C) */
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
#define PI	3.14159265358979323846264338327950288419716939937511
					/* ratio of circumf. to diam. */

/* Useful macros */

/* arbitrary numerical arguments and value: */
#define Abs( x )	((x) < 0 ? -(x) : (x))
#define Max( a, b )	((a) > (b) ? (a) : (b))
#define Min( a, b )	((a) < (b) ? (a) : (b))

/* floating-point arguments and value: */
#define Round( d )	floor( (d) + 0.5 )

/* arbitrary numerical arguments, integer value: */
#define	Sgn( x )	((x) == 0 ? 0 : (x) > 0 ? 1 : -1)

/* integer (or character) arguments and value: */
#define tohostc( c )	(c)		/* map ASCII to host char set */
#define tonumber( c )	((c) - '0')	/* convt digit char to number */
#define todigit( n )	((n) + '0')	/* convt digit number to char */

#endif	/* _VLD_STD_H_ */
