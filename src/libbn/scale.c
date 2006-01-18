/*                         S C A L E . C
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

/** \addtogroup libbn */
/*@{*/
/** @file scale.c
 *	This routine is intended to take an array of
 * data points as input (either integer, floating, or
 * double), and scale it to fit in a space of LENGTH units.
 * An output array is returned
 * which contains the scaled information stored in 16-bit integers.  The input
 * and output arrays may overlap, as the input will never
 * occupy more space than the output.  Also output are
 * the minimum value encountered (MIN), and a delta
 * factor showing the increase in value each XXXX.
 * This DX factor is rounded to 1,2,4,5,8,or 10 to
 * produce nicer looking axes.
 *
 * where
 *
 *	int *idata	INPUT	This pointer contains the address
 *				of the input array to be scaled.
 *				Actual type of array is determined
 *				by MODE parameter.
 *
 *	int elements	INPUT	Number of elements in IDATA to be used.
 *
 *	int mode	INPUT	Specifies type of data that IDATA points
 *				to;  should be one of:
 *					'd' - double precision
 *					'f' - float (single precision)
 *					'i' - integer
 *
 *	int length	INPUT	Contains the length (in 1/1000ths of an
 *				inch) of the region in which the data is
 *				to be scaled into.  Note that the actual
 *				amount of space needed may be this value
 *				rounded up to the next inch.
 *
 *	int *odata	OUTPUT	This pointer contains the address of the
 *				output array, which will always be of
 *				integer type.
 *
 *	double *min	OUTPUT	This pointer contains the address of the
 *				location for minimum point found to be
 *				placed in.
 *
 *	double *dx	OUTPUT	This pointer addresses the delta value
 *				of the data which corresponds to the width
 *				of EACH tick.
 *				This implies that:
 *				  1)	This is exactly the number to divide
 *					raw data by to scale it to this scale
 *					(ex:  2 graphs with one scale factor)
 *				  2)	When this value is fed to the AXIS
 *					routine, it must be multiplied
 *					by 1000.0 first (to specify increment
 *					between one INCH ticks).
 *
 * The fact that this routine returns variables of type DOUBLE has
 * important implications for FORTRAN users.  These variables must
 * be declared of type DOUBLE PRECISION to reserve enough space.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "plot3.h"

void
tp_scale(int *idata, int elements, register int mode, int length, int *odata, double *min, double *dx)
{
	double xmax, xmin, x, workdx;
	register int i;			/* Index variable */
	static double log_10;		/* Saved value for log base-2(10) */
	float *ifloatp;			/* Used to convert pointer-to-int to float */
	double *idoublep;		/* Used to convert pointer-to-int to double */
	double fractional;		/* Fractional part of DX */
	int integral;			/* Integral part of DX */

	/* Prepare to use a pointer to an array of variable type */
	ifloatp = (float *)idata;
	idoublep = (double *)idata;
	/* Find the maximum and minimum data values */
	xmax = xmin = 0.0;
	for( i=0; i<elements; i++ )  {
		x = (mode=='f')
			? ifloatp[i]
			: ( (mode=='d')
				? idoublep[i]
				: idata[i]
			);
		if( x > xmax )
			xmax = x;
		if( x < xmin )
			xmin = x;
	}

	/* Split initial DX into integral and fractional exponents of 10 */
	if( log_10 <= 0.0 )
		log_10 = log(10.0);

	fractional = log( (xmax-xmin)/length ) / log_10;	/* LOG10(DX) */
	integral = fractional;			/* truncate! */
	fractional -= integral;			/* leave only fract */

	if( fractional < 0.0 )  {
		fractional += 1.0;		/* ?? */
		integral -= 1;
	}

	fractional = pow( 10.0, fractional );
	i = fractional - 0.01;
	switch( i )  {

	case 1:
		fractional = 2.0;
		break;

	case 2:
	case 3:
		fractional = 4.0;
		break;

	case 4:
		fractional = 5.0;
		break;

	case 5:
	case 6:
	case 7:
		fractional = 8.0;
		break;

	case 8:
	case 9:
		fractional = 10.0;

	}

	/* Compute DX factor, combining power of ten & adjusted co-efficient */
	workdx = pow( 10.0, (double)integral ) * fractional;

	/* Apply the MIN and DX values to the users input data */
	for( i=0; i<elements; i++ )  {
		if( mode == 'f' )
			odata[i] = (ifloatp[i] - xmin) / workdx;
		else
			if( mode == 'd' )
				odata[i] = (idoublep[i] - xmin) / workdx;
			else
				odata[i] = (idata[i] - xmin) / workdx;
	}

	/* Send MIN and DX back to the user */
	*min = xmin;
	*dx = workdx;
}



/*
 *	FORTRAN Interface
 */
void
PL_FORTRAN(fscale, FSCALE)( idata, elements, mode, length, odata, min, dx )
int	idata[];
int	*elements;
char	*mode;
int	*length;
int	odata[];
double	*min;
double	*dx;
{
	tp_scale( idata, *elements, *mode, *length, odata, min, dx );
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
