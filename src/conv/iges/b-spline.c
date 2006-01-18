/*                      B - S P L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
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
/** @file b-spline.c
 *  Authors -
 *	John R. Anderson
 *	Susanne L. Muuss
 *	Earl P. Weaver
 *
 *  Source -
 *	VLD/ASB Building 1065
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

/*	These functions evaluate a Rational B-Spline Curve */

#include "common.h"



#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"		/* for declaration of bu_calloc() */

#undef W		/* from vmath.h */

static fastf_t *knots=(fastf_t *)NULL;
static int numknots=0;

/* Set the knot values */
void
Knot( n , values )
int n;		/* number of values in knot sequence */
fastf_t values[];	/* knot values */
{
	int i;

	if( n < 2 )
	{
		bu_log( "Knot: ERROR %d knot values\n" , n );
		rt_bomb( "Knot: cannot have less than 2 knot values\n" );
	}

	if( numknots )
		bu_free( (char *)knots , "Knot: knots" );

	knots = (fastf_t *)bu_calloc( n , sizeof( fastf_t ) , "Knot: knots" );

	numknots = n;

	for( i=0 ; i<n ; i++ )
		knots[i] = values[i];

}

void
Freeknots()
{
	bu_free( (char *)knots , "Freeknots: knots" );
	numknots = 0;
}


/* Evaluate the Basis functions */
fastf_t
Basis( i , k , t )
fastf_t t;	/* parameter value */
int i;		/* interval number ( 0 through k ) */
int k;		/* degree of basis function */
{
	fastf_t denom1,denom2,retval=0.0;

	if( (i+1) > (numknots-1) )
	{
		bu_log( "Error in evaluation of a B-spline Curve\n" );
		bu_log( "attempt to access knots out of range: numknots=%d i=%d, k=%d\n" , numknots , i , k );
		return( 0.0 );
	}

	if( k == 1 )
	{
		if( t >= knots[i] && t < knots[i+1] )
			return( 1.0 );
		else
			return( 0.0 );
	}
	else
	{
		denom1 = knots[i+k-1] - knots[i];
		denom2 = knots[i+k] - knots[i+1];

		if(denom1 != 0.0 )
			retval += (t - knots[i])*Basis( i , k-1 , t )/denom1;

		if( denom2 != 0.0 )
			retval += (knots[i+k] - t)*Basis( i+1 , k-1 , t )/denom2;

		return( retval );
	}
}

/* Evaluate a B-Spline curve */
void
B_spline( t , m , k , P , W , pt )
fastf_t t;	/* parameter value */
int k;		/* order */
int m;		/* upper limit of sum (number of control points - 1) */
point_t P[];	/* Control Points (x,y,z) */
fastf_t W[];	/* Weights for Control Points */
point_t pt;	/* Evaluated point on spline */
{

	fastf_t tmp,numer[3],denom=0.0;
	int i,j;

	for( j=0 ; j<3 ; j++ )
		numer[j] = 0.0;

	for( i=0 ; i<=m ; i++ )
	{
		tmp = W[i]*Basis( i , k , t );
		denom += tmp;
		for( j=0 ; j<3 ; j++ )
			numer[j] += P[i][j]*tmp;
	}

	for( j=0 ; j<3 ; j++ )
		pt[j] = numer[j]/denom;
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
