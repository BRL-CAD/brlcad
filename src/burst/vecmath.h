/*                       V E C M A T H . H
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
 *
 */
/** @file vecmath.h
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066

	$Header$
*/
#ifndef _POLY9
#include <math.h>
#endif
#ifndef Abs
#define Abs( a )		((a) > 0 ? (a) : -(a))
#endif
#define AbsDotProd( A, B )	(Abs( Dot( A, B ) ))
#define AddVec( A, B )        { (A)[X] += (B)[X]; \
				(A)[Y] += (B)[Y]; \
				(A)[Z] += (B)[Z]; }
#define Add2Vec( A, B, C )    { (C)[X] = (A)[X]+(B)[X]; \
				(C)[Y] = (A)[Y]+(B)[Y]; \
				(C)[Z] = (A)[Z]+(B)[Z]; }
#define AproxEq( a, b, e )	(Abs( (a)-(b) ) < (e))
#define AproxEqVec( A, B, e ) ( AproxEq((A)[X],(B)[X],(e)) && \
				AproxEq((A)[Y],(B)[Y],(e)) && \
				AproxEq((A)[Z],(B)[Z],(e)) )
#define CopyVec( A, B )	      { (A)[X] = (B)[X]; \
				(A)[Y] = (B)[Y]; \
				(A)[Z] = (B)[Z]; }
#define CrossProd( A, B, C )  {	(C)[X] = (A)[Y]*(B)[Z]-(A)[Z]*(B)[Y]; \
				(C)[Y] = (A)[Z]*(B)[X]-(A)[X]*(B)[Z]; \
				(C)[Z] = (A)[X]*(B)[Y]-(A)[Y]*(B)[X]; }
#define DEGRAD	57.2957795130823208767981548141051703324054724665642
					/* degrees per radian */
#define DiffVec( A, B )       { (A)[X] -= (B)[X]; \
				(A)[Y] -= (B)[Y]; \
				(A)[Z] -= (B)[Z]; }
#define Diff2Vec( A, B, C )   { (C)[X] = (A)[X]-(B)[X]; \
				(C)[Y] = (A)[Y]-(B)[Y]; \
				(C)[Z] = (A)[Z]-(B)[Z]; }
#define Dist3d( A, B )		(sqrt(	Sqr((A)[X]-(B)[X])+\
					Sqr((A)[Y]-(B)[Y])+\
					Sqr((A)[Z]-(B)[Z]))\
				)
#define DivideVec( A, S )     { (A)[X] /= (S); \
				(A)[Y] /= (S); \
				(A)[Z] /= (S); }
#define Dot( A, B )		((A)[X]*(B)[X]+(A)[Y]*(B)[Y]+(A)[Z]*(B)[Z])
#ifndef	EPSILON
#define EPSILON	0.000001
#endif
#define Expand_Vec_Int( V )	(int)(V)[X], (int)(V)[Y], (int)(V)[Z]
#define LOG10E	0.43429448190325182765112891891660508229439700580367
					/* log of e to the base 10 */
#define Mag( A )	      	sqrt( AbsDotProd(A,A) )
#define Mag3(a1,a2,a3)		(sqrt(Sqr(a1)+Sqr(a2)+Sqr(a3)))
#ifndef Min
#define Min( a, b )		((a) < (b) ? (a) : (b))
#define Max( a, b )		((a) > (b) ? (a) : (b))
#endif
#define MinMax( m, M, a )    { m = Min( m, a ); M = Max( M, a ); }
#define MinMaxVec( A, B, C ) { (A)[X] = Min( (A)[X], (C)[X] ); \
			       (A)[Y] = Min( (A)[Y], (C)[Y] ); \
			       (A)[Z] = Min( (A)[Z], (C)[Z] ); \
			       (B)[X] = Max( (B)[X], (C)[X] ); \
			       (B)[Y] = Max( (B)[Y], (C)[Y] ); \
			       (B)[Z] = Max( (B)[Z], (C)[Z] ); }
#define NearZero( a )		((a) < EPSILON && (a) > -EPSILON)
#define NonZeroVec( V )	(!NearZero((V)[X]) || !NearZero((V)[Y])|| !NearZero((V)[Z]))
#ifndef PI
#define PI	3.14159265358979323846264338327950288419716939937511
#endif
					/* ratio of circumf. to diam. */
#define RelDist3d( A, B )	(Sqr((A)[X]-(B)[X])+\
				 Sqr((A)[Y]-(B)[Y])+\
				 Sqr((A)[Z]-(B)[Z]))
#define ScaleVec( A, s )      { (A)[X] *= (s); \
				(A)[Y] *= (s); \
				(A)[Z] *= (s); }
#define Scale2Vec( A, s, B )   { (B)[X] = (A)[X] * (s); \
				(B)[Y] = (A)[Y] * (s); \
				(B)[Z] = (A)[Z] * (s); }
#define Sqr(a)			((a)*(a))
#define TWO_PI		6.28318530717958647692528676655900576839433879875022
/* Scale vector 'a' to have magnitude 'l'.				*/
#define V_Length( a, l ) \
		{	double f, m; \
		if( (m=Mag(a)) == 0.0 ) \
			brst_log( "Magnitude is zero!\n" ); \
		else \
			{ \
			f = (l)/m; \
			(a)[X] *= f; (a)[Y] *= f; (a)[Z] *= f; \
			} \
		}

#define V_Print(a,b,func) \
		func( "%s\t<%12.6f,%12.6f,%12.6f>\n", a, (b)[0], (b)[1], (b)[2] )
#ifndef X
#define X		0
#define Y		1
#define Z		2
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
