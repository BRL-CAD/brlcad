/*
 *			T P _ N U M B E R
 *
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
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

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
