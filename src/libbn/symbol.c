/*                        S Y M B O L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @addtogroup plot */
/** @{ */
/** @file symbol.c
 *
 *	Terminal Independant Graphics Display Package.
 *		Mike Muuss  July 31, 1978
 *
 *	This routine is used to plot a string of ASCII symbols
 *  on the plot being generated, using a built-in set of fonts
 *  drawn as vector lists.
 *
 *	Internally, the basic font resides in a 10x10 unit square.
 *  Externally, each character can be thought to occupy one square
 *  plotting unit;  the 'scale'
 *  parameter allows this to be changed as desired, although scale
 *  factors less than 10.0 are unlikely to be legible.
 *
 *  @author
 *	Michael John Muuss
 *
 *  @par Source -
 *	The U. S. Army Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */


#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "plot3.h"
#include "vectfont.h"

/*
 *			T P _ 3 S Y M B O L
 */
void
tp_3symbol(FILE *fp, char *string, fastf_t *origin, fastf_t *rot, double scale)

    	        		/* string of chars to be plotted */
       	       			/* lower left corner of 1st char */
     	    			/* Transform matrix (WARNING: may xlate) */
      	      			/* scale factor to change 1x1 char sz */
{
	register unsigned char *cp;
	double	offset;			/* offset of char from given x,y */
	int	ysign;			/* sign of y motion, either +1 or -1 */
	vect_t	temp;
	vect_t	loc;
	mat_t	xlate_to_origin;
	mat_t	mat;

	if( string == NULL || *string == '\0' )
		return;			/* done before begun! */

	/*
	 *  The point "origin" will be the center of the axis rotation.
	 *  The text is located in a local coordinate system with the
	 *  lower left corner of the first character at (0,0,0), with
	 *  the text proceeding onward towards +X.
	 *  We need to rotate the text around it's local (0,0,0),
	 *  and then translate to the user's designated "origin".
	 *  If the user provided translation or
	 *  scaling in his matrix, it will *also* be applied.
	 */
	MAT_IDN( xlate_to_origin );
	MAT_DELTAS( xlate_to_origin,	origin[X], origin[Y], origin[Z] );
	bn_mat_mul( mat, xlate_to_origin, rot );

	/* Check to see if initialization is needed */
	if( tp_cindex[040] == 0 )  tp_setup();

	/* Draw each character in the input string */
	offset = 0;
	for( cp = (unsigned char *)string ; *cp; cp++, offset += scale )  {
		register TINY	*p;	/* pointer to stroke table */
		register int	stroke;

		VSET( temp, offset, 0, 0 );
		MAT4X3PNT( loc, mat, temp );
		pdv_3move( fp, loc );

		for( p = tp_cindex[*cp]; (stroke= *p) != LAST; p++ )  {
			int	draw;

			if( stroke==NEGY )  {
				ysign = (-1);
				stroke = *++p;
			} else
				ysign = 1;

			/* Detect & process pen control */
			if( stroke < 0 )  {
				stroke = -stroke;
				draw = 0;
			} else
				draw = 1;

			/* stroke co-ordinates in string coord system */
			VSET( temp, (stroke/11) * 0.1 * scale + offset,
				   (ysign * (stroke%11)) * 0.1 * scale, 0 );
			MAT4X3PNT( loc, mat, temp );
			if( draw )
				pdv_3cont( fp, loc );
			else
				pdv_3move( fp, loc );
		}
	}
}


/*
 *			T P _ 2 S Y M B O L
 */
void
tp_2symbol(FILE *fp, char *string, double x, double y, double scale, double theta)

    	        		/* string of chars to be plotted */
      	  			/* x,y of lower left corner of 1st char */

      	      			/* scale factor to change 1x1 char sz */
      	      			/* degrees ccw from X-axis */
{
	mat_t	mat;
	vect_t	p;

	bn_mat_angles( mat, 0.0, 0.0, theta );
	VSET( p, x, y, 0 );
	tp_3symbol( fp, string, p, mat, scale );
}

/*
 *  This FORTRAN interface expects REAL args (single precision).
 */
void
PL_FORTRAN(f2symb, F2SYMB)( fp, string, x, y, scale, theta )
FILE	**fp;
char	*string;
float	*x;
float	*y;
float	*scale;
float	*theta;
{
	char buf[128];

	strncpy( buf, string, sizeof(buf)-1 );
	buf[sizeof(buf)-1] = '\0';
	tp_2symbol( *fp, buf, *x, *y, *scale, *theta );
}
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
