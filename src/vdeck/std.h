/*                           S T D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file vdeck/std.h
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

typedef char *pointer;		/* generic pointer (void *) */

#define	const		/* nothing */	/* (undefine for ANSI C) */
#define	signed		/* nothing */	/* (undefine for ANSI C) */
#define	volatile	/* nothing */	/* (undefine for ANSI C) */


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
#ifndef	unix
#  define	toascii( c )	((c) & 0177)	/* map host char set to ASCII */
#endif
#define tohostc( c )	(c)		/* map ASCII to host char set */
#define tonumber( c )	((c) - '0')	/* convt digit char to number */
#define todigit( n )	((n) + '0')	/* convt digit number to char */

#endif	/* _VLD_STD_H_ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
