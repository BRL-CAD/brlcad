/*
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */

/*	These functions evaluate a Rational B-Spline Curve */

#include <malloc.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"

#undef W		/* from vmath.h */

static float maxknot,*knots=NULL;
static int numknots=0;

/* Set the knot values */
Knot( n , values )
int n;		/* number of values in knot sequence */
float values[];	/* knot values */
{
	int i;

	if( knots != NULL )
		free( knots );
	knots = (float *)calloc( n , sizeof( float ) );

	numknots = n;

	for( i=0 ; i<n ; i++ )
		knots[i] = values[i];

	maxknot = knots[n-1];
}

Freeknots()
{
	free( knots );
}


/* Evaluate the Basis functions */
float Basis( i , k , t )
float t;	/* parameter value */
int i;		/* interval number ( 0 through k )
int k;		/* degree of basis function */
{
	float denom1,denom2,retval=0.0;

	if( (i+1) > (numknots-1) )
	{
		fprintf( stderr , "Error in evaluation of a B-spline Curve\n" );
		fprintf( stderr, "attempt to access knots out of range: numknots=%d i=%d, k=%d\n" , numknots , i , k );
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
B_spline( t , m , k , P , W , pt )
float t;	/* parameter value */
int k;		/* order */
int m;		/* upper limit of sum (number of control points - 1) */
float P[][3];	/* Control Points (x,y,z) */
float W[];	/* Weights for Control Points */
point_t pt;	/* Evaluated point on spline */
{

	double tmp,numer[3],denom=0.0;
	int i,j;

	for( j=0 ; j<3 ; j++ )
		numer[j] = 0.0;

	for( i=0 ; i<m ; i++ )
	{
		tmp = W[i]*Basis( i , k , t );
		denom += tmp;
		for( j=0 ; j<3 ; j++ )
			numer[j] += P[i][j]*tmp;
	}

	for( j=0 ; j<3 ; j++ )
		pt[j] = numer[j]/denom;
}


	
