/*                        N U M B E R . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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

/** \addtogroup libbn */
/*@{*/
/** @file number.c
 *	This routine is used to convert a floating point number into
 * a string of ASCII characters, and then call tp_symbol().
 *
 *  Author -
 *	Mike Muuss
 *	August 01, 1978
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "plot3.h"

void
tp_2number(FILE *fp, double input, int x, int y, int cscale, double theta, int digits)
    	    
      	      		/* number to be plotted */
   	  		/* first char position */
   	  
   	       		/* char scale */
      	      		/* degrees ccw from X-axis */
   	       		/* # digits wide */
{
	char	fmt[32];
	char	buf[32];

	if( digits < 1 )
		digits = 1;
#if 0
	sprintf( fmt, "%%%dg", digits, digits );
#else
	sprintf( fmt, "%%%dg", digits );
#endif
	sprintf( buf, fmt, input );
	tp_2symbol( fp, buf, (double)x, (double)y, (double)cscale, theta );
}

void
PL_FORTRAN(f2numb, F2NUMB)( fp, input, x, y, cscale, theta, digits )
FILE	**fp;
float	*input;
int	*x;
int	*y;
float	*cscale;
float	*theta;
int     *digits;
{
	tp_2number( *fp, *input, *x, *y, *cscale, *theta, *digits);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
