/*                        M A R K E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
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

/** \addtogroup plot */
/*@{*/
/** @file marker.c
 *
 *
 *	This routine places a specified character (either from
 * the ASCII set, or one of the 5 special marker characters)
 * centered about the current pen position (instead of above & to
 * the right of the current position, as characters usually go).
 * Calling sequence:
 *
@code
 *	char c		is the character to be used for a marker,
 *			or one of the following special markers -
 *				1 = plus
 *				2 = an "x"
 *				3 = a triangle
 *				4 = a square
 *				5 = an hourglass
@endcode
 *
 *  @author	Mike Muuss
 *@n	August 04, 1978
 *
 *  @par Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *@n	The U. S. Army Ballistic Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "plot3.h"

void
tp_2marker(FILE *fp, register int c, double x, double y, double scale)
{
	char	mark_str[4];

	mark_str[0] = (char)c;
	mark_str[1] = '\0';

	/* Draw the marker */
	tp_2symbol( fp, mark_str,
		(x - scale*0.5), (y - scale*0.5),
		scale, 0.0 );
}

void
PL_FORTRAN(f2mark, F2MARK)( fp, c, x, y, scale )
FILE	**fp;
int	*c;
float	*x;
float	*y;
float	*scale;
{
	tp_2marker( *fp, *c, *x, *y, *scale );
}

/*
 *			T P _ 3 M A R K E R
 */
void
tp_3marker(FILE *fp, register int c, double x, double y, double z, double scale)
{
	char	mark_str[4];
	mat_t	mat;
	vect_t	p;

	mark_str[0] = (char)c;
	mark_str[1] = '\0';
	MAT_IDN( mat );
	VSET( p, x - scale*0.5, y - scale*0.5, z );
	tp_3symbol( fp, mark_str, p, mat, scale );
}

void
PL_FORTRAN(f3mark, F3MARK)( fp, c, x, y, z, scale )
FILE	**fp;
int	*c;
float	*x;
float	*y;
float	*z;
float	*scale;
{
	tp_3marker( *fp, *c, *x, *y, *z, *scale );
}

/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
