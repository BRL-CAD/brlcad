/*
 *			T O R U S . C
 *
 * Purpose -
 *	Intersect a ray with a Torus
 *
 * Authors -
 *	Edwin O. Davisson	(Analysis)
 *	Jeff Hanes		(Programming)
 *	Michael John Muuss	(RT adaptation)
 *	Gary S. Moss		(Improvement)
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCStorus[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/db.h"
#include "../h/raytrace.h"
#include "debug.h"
#include "polyno.h"
#include "complex.h"

static void	TorPtSort();

/*
 * The TORUS has the following input fields:
 *	V	V from origin to center
 *	H	Radius Vector, Normal to plane of torus.  |H| = R2
 *	A,B	perpindicular, to CENTER of torus.  |A|==|B|==R1
 *	F5,F6	perpindicular, for inner edge (unused)
 *	F7,F8	perpindicular, for outer edge (unused)
 *
 */

/*
 *  Algorithm:
 *  
 *  Given V, H, A, and B, there is a set of points on this torus
 *  
 *  { (x,y,z) | (x,y,z) is on torus defined by V, H, A, B }
 *  
 *  Through a series of  Transformations, this set will be
 *  transformed into a set of points on a unit torus (R1==1)
 *  centered at the origin
 *  which lies on the X-Y plane (ie, H is on the Z axis).
 *  
 *  { (x',y',z') | (x',y',z') is on unit torus at origin }
 *  
 *  The transformation from X to X' is accomplished by:
 *  
 *  X' = S(R( X - V ))
 *  
 *  where R(X) =  ( A/(|A|) )
 *  		 (  B/(|B|)  ) . X
 *  		  ( H/(|H|) )
 *
 *  and S(X) =	 (  1/|A|   0     0   )
 *  		(    0    1/|A|   0    ) . X
 *  		 (   0      0   1/|A| )
 *  where |A| = R1
 *
 *  To find the intersection of a line with the torus, consider
 *  the parametric line L:
 *  
 *  	L : { P(n) | P + t(n) . D }
 *  
 *  Call W the actual point of intersection between L and the torus.
 *  Let W' be the point of intersection between L' and the unit torus.
 *  
 *  	L' : { P'(n) | P' + t(n) . D' }
 *  
 *  W = invR( invS( W' ) ) + V
 *  
 *  Where W' = k D' + P'.
 *  
 *
 *  Given a line and a ratio, alpha, finds the equation of the
 *  unit torus in terms of the variable 't'.
 *
 *  The equation for the torus is:
 *
 *      [ X**2 + Y**2 + Z**2 + (1 - alpha**2) ]**2 - 4*( X**2 + Y**2 )  =  0
 *
 *  First, find X, Y, and Z in terms of 't' for this line, then
 *  substitute them into the equation above.
 *
 *  	Wx = Dx*t + Px
 *
 *  	Wx**2 = Dx**2 * t**2  +  2 * Dx * Px  +  Px**2
 *
 *  The real roots of the equation in 't' are the intersect points
 *  along the parameteric line.
 *  
 *  NORMALS.  Given the point W on the torus, what is the vector
 *  normal to the tangent plane at that point?
 *  
 *  Map W onto the unit torus, ie:  W' = S( R( W - V ) ).
 *  In this case, we find W' by solving the parameteric line given k.
 *  
 *  The gradient of the torus at W' is in fact the
 *  normal vector.
 *
 *  Given that the equation for the unit torus is:
 *
 *	[ X**2 + Y**2 + Z**2 + (1 - alpha**2) ]**2 - 4*( X**2 + Y**2 )  =  0
 *
 *  let w = X**2 + Y**2 + Z**2 + (1 - alpha**2), then the equation becomes:
 *
 *	w**2 - 4*( X**2 + Y**2 )  =  0
 *
 *  For f(x,y,z) = 0, the gradient of f() is ( df/dx, df/dy, df/dz ).
 *
 *	df/dx = 2 * w * 2 * x - 8 * x	= (4 * w - 8) * x
 *	df/dy = 2 * w * 2 * y - 8 * y	= (4 * w - 8) * y
 *	df/dz = 2 * w * 2 * z		= 4 * w * z
 *
 *  Note that the normal vector produced above will not have unit length.
 *  Also, to make this useful for the original torus, it will have
 *  to be rotated back to the orientation of the original torus.
 */

struct tor_specific {
	fastf_t	tor_alpha;	/* 0 < (R2/R1) <= 1 */
	fastf_t	tor_r1;		/* for inverse scaling of k values. */
	vect_t	tor_V;		/* Vector to center of torus */
	mat_t	tor_SoR;	/* Scale(Rot(vect)) */
	mat_t	tor_invR;	/* invRot(vect') */
};

/*
 *  			T O R _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid torus, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	TOR is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct tor_specific is created, and it's address is stored in
 *  	stp->st_specific for use by tor_shot().
 */
tor_prep( vec, stp, mat )
register fastf_t *vec;
struct soltab *stp;
matp_t mat;			/* Homogenous 4x4, with translation, [15]=1 */
{
	register struct tor_specific *tor;
	LOCAL fastf_t	magsq_a, magsq_b, magsq_h;
	LOCAL mat_t	R;
	LOCAL vect_t	A, B, Hv;
	LOCAL vect_t	work, temp;
	FAST fastf_t	f;
	LOCAL fastf_t	r1, r2;	/* primary and secondary radius */
	LOCAL fastf_t	mag_b;

#define TOR_V	&vec[0*ELEMENTS_PER_VECT]
#define TOR_H	&vec[1*ELEMENTS_PER_VECT]
#define TOR_A	&vec[2*ELEMENTS_PER_VECT]
#define TOR_B	&vec[3*ELEMENTS_PER_VECT]

	/*
	 * Apply 3x3 rotation mat only to A,B,H
	 */
	MAT4X3VEC( A, mat, TOR_A );
	MAT4X3VEC( B, mat, TOR_B );
	MAT4X3VEC( Hv, mat, TOR_H );

	magsq_a = MAGSQ( A );
	magsq_b = MAGSQ( B );
	magsq_h = MAGSQ( Hv );
	r1 = sqrt(magsq_a);
	r2 = sqrt(magsq_h);
	mag_b = sqrt(magsq_b);

	/* Validate that |A| > 0, |B| > 0, |H| > 0 */
	if( NEAR_ZERO(magsq_a, 0.0001) ||
	    NEAR_ZERO(magsq_b, 0.0001) ||
	    NEAR_ZERO(magsq_h, 0.0001) ) {
		rt_log("tor(%s):  zero length A, B, or H vector\n",
			stp->st_name );
		return(1);		/* BAD */
	}

	/* Validate that |A| == |B| (for now) */
	if( rt_fdiff( r1, mag_b ) != 0 ) {
		rt_log("tor(%s):  (|A|=%f) != (|B|=%f) \n",
			stp->st_name, r1, mag_b );
		return(1);		/* BAD */
	}

	/* Validate that A.B == 0, B.H == 0, A.H == 0 */
	f = VDOT( A, B )/(r1*mag_b);

	if( ! NEAR_ZERO(f, 0.0001) )  {
		rt_log("tor(%s):  A not perpendicular to B, f=%f\n",
			stp->st_name, f);
		return(1);		/* BAD */
	}
	f = VDOT( B, Hv )/(mag_b*r2);
	if( ! NEAR_ZERO(f, 0.0001) )  {
		rt_log("tor(%s):  B not perpendicular to H, f=%f\n",
			stp->st_name, f);
		return(1);		/* BAD */
	}
	f = VDOT( A, Hv )/(r1*r2);
	if( ! NEAR_ZERO(f, 0.0001) )  {
		rt_log("tor(%s):  A not perpendicular to H, f=%f\n",
			stp->st_name, f);
		return(1);		/* BAD */
	}

	/* Validate that 0 < r2 <= r1 for alpha computation */
	if( 0.0 >= r2  || r2 > r1 )  {
		rt_log("r1 = %f, r2 = %f\n", r1, r2 );
		rt_log("tor(%s):  0 < r2 <= r1 is not true\n", stp->st_name);
		return(1);		/* BAD */
	}

	/* Solid is OK, compute constant terms now */
	GETSTRUCT( tor, tor_specific );
	stp->st_specific = (int *)tor;

	tor->tor_r1 = r1;

	MAT4X3PNT( tor->tor_V, mat, TOR_V );
	tor->tor_alpha = r2/tor->tor_r1;

	/* Compute R and invR matrices */
	VUNITIZE( Hv );

	mat_idn( R );
	VMOVE( &R[0], A );
	VUNITIZE( &R[0] );
	VMOVE( &R[4], B );
	VUNITIZE( &R[4] );
	VMOVE( &R[8], Hv );
	mat_inv( tor->tor_invR, R );

	/* Compute SoR.  Here, S = I / r1 */
	mat_copy( tor->tor_SoR, R );
	tor->tor_SoR[15] *= tor->tor_r1;

	VMOVE( stp->st_center, tor->tor_V );
	stp->st_aradius = stp->st_bradius = tor->tor_r1 + r2;

	/* Compute bounding RPP */
#define TOR_MINMAX(a,b,c)	{ FAST fastf_t ftemp;\
			if( (ftemp = (c)) < (a) )  a = ftemp;\
			if( ftemp > (b) )  b = ftemp; }

#define TOR_MM(v)	TOR_MINMAX( stp->st_min[X], stp->st_max[X], v[X] ); \
		TOR_MINMAX( stp->st_min[Y], stp->st_max[Y], v[Y] ); \
		TOR_MINMAX( stp->st_min[Z], stp->st_max[Z], v[Z] )

	/* Exterior radius is r1+r2;  rescale A and B here */
	f = tor->tor_r1+r2;
	VUNITIZE( A );
	VSCALE( A, A, f );
	VUNITIZE( B );
	VSCALE( B, B, f );
	VSCALE( Hv, Hv, r2 );

	/* There are 8 corners to the enclosing RPP;  find max and min */
	VADD3( temp, tor->tor_V, B, Hv );
	VADD2( work, temp, A ); TOR_MM( work );	/* V + A + B + Hv */
	VSUB2( work, temp, A ); TOR_MM( work );	/* V - A + B + Hv */

	VSUB2( temp, tor->tor_V, B );
	VADD2( temp, temp, Hv );
	VADD2( work, temp, A ); TOR_MM( work );	/* V + A - B + Hv */
	VSUB2( work, temp, A ); TOR_MM( work );	/* V - A - B + Hv */
	
	VSUB2( temp, tor->tor_V, Hv );
	VADD2( temp, temp, B );
	VADD2( work, temp, A ); TOR_MM( work );	/* V + A + B - Hv */
	VSUB2( work, temp, A ); TOR_MM( work );	/* V - A + B - Hv */

	VSUB3( temp, tor->tor_V, B, Hv );
	VADD2( work, temp, A ); TOR_MM( work );	/* V + A - B - Hv */
	VSUB2( work, temp, A ); TOR_MM( work );	/* V - A - B - Hv */

	return(0);			/* OK */
}

tor_print( stp )
register struct soltab *stp;
{
	register struct tor_specific *tor =
		(struct tor_specific *)stp->st_specific;

	rt_log("r2/r1 (alpha) = %f\n", tor->tor_alpha);
	rt_log("r1 = %f\n", tor->tor_r1);
	VPRINT("V", tor->tor_V);
	mat_print("S o R", tor->tor_SoR );
	mat_print("invR", tor->tor_invR );
}

/*
 *  			T O R _ S H O T
 *  
 *  Intersect a ray with an torus, where all constant terms have
 *  been precomputed by tor_prep().  If an intersection occurs,
 *  one or two struct seg(s) will be acquired and filled in.
 *
 *  NOTE:  All lines in this function are represented parametrically
 *  by a point,  P( x0, y0, z0 ) and a direction normal,
 *  D = ax + by + cz.  Any point on a line can be expressed
 *  by one variable 't', where
 *
 *	X = a*t + x0,	eg,  X = Dx*t + Px
 *	Y = b*t + y0,
 *	Z = c*t + z0.
 *
 *  First, convert the line to the coordinate system of a "stan-
 *  dard" torus.  This is a torus which lies in the X-Y plane,
 *  circles the origin, and whose primary radius is one.  The
 *  secondary radius is  alpha = ( R2/R1 )  of the original torus
 *  where  ( 0 < alpha <= 1 ).
 *
 *  Then find the equation of that line and the standard torus,
 *  which turns out to be a quartic equation in 't'.  Solve the
 *  equation using a general polynomial root finder.  Use those
 *  values of 't' to compute the points of intersection in the
 *  original coordinate system.
 *  
 *  Returns -
 *  	0	MISS
 *  	segp	HIT
 */
struct seg *
tor_shot( stp, rp )
struct soltab *stp;
register struct xray *rp;
{
	register struct tor_specific *tor =
		(struct tor_specific *)stp->st_specific;
	register struct seg *segp;
	LOCAL vect_t	dprime;		/* D' */
	LOCAL vect_t	pprime;		/* P' */
	LOCAL vect_t	work;		/* temporary vector */
	LOCAL poly	C;		/* The final equation */
	LOCAL complex	val[MAXP];	/* The complex roots */
	LOCAL double	k[4];		/* The real roots */
	register int	i;
	LOCAL int	j;
	LOCAL poly	A, Asqr;
	LOCAL poly	X2_Y2;		/* X**2 + Y**2 */
	LOCAL vect_t	cor_pprime;	/* new ray origin */
	LOCAL fastf_t	cor_proj;

	/* Convert vector into the space of the unit torus */
	MAT4X3VEC( dprime, tor->tor_SoR, rp->r_dir );
	VUNITIZE( dprime );

	VSUB2( work, rp->r_pt, tor->tor_V );
	MAT4X3VEC( pprime, tor->tor_SoR, work );

	/* normalize distance from torus.  substitute
	 * corrected pprime which contains a translation along ray
	 * direction to closest approach to vertex of torus.
	 * Translating ray origin along direction of ray to closest pt. to
	 * origin of solid's coordinate system, new ray origin is
	 * 'cor_pprime'.
	 */
	cor_proj = VDOT( pprime, dprime );
	VSCALE( cor_pprime, dprime, cor_proj );
	VSUB2( cor_pprime, pprime, cor_pprime );

	/*
	 *  Given a line and a ratio, alpha, finds the equation of the
	 *  unit torus in terms of the variable 't'.
	 *
	 *  The equation for the torus is:
	 *
	 * [ X**2 + Y**2 + Z**2 + (1 - alpha**2) ]**2 - 4*( X**2 + Y**2 ) = 0
	 *
	 *  First, find X, Y, and Z in terms of 't' for this line, then
	 *  substitute them into the equation above.
	 *
	 *  	Wx = Dx*t + Px
	 *
	 *  	Wx**2 = Dx**2 * t**2  +  2 * Dx * Px  +  Px**2
	 *  		[0]                [1]           [2]    dgr=2
	 */
	X2_Y2.dgr = 2;
	X2_Y2.cf[0] = dprime[X] * dprime[X] + dprime[Y] * dprime[Y];
	X2_Y2.cf[1] = 2.0 * (dprime[X] * cor_pprime[X] +
			     dprime[Y] * cor_pprime[Y]);
	X2_Y2.cf[2] = cor_pprime[X] * cor_pprime[X] +
		      cor_pprime[Y] * cor_pprime[Y];

	/* A = X2_Y2 + Z2 */
	A.dgr = 2;
	A.cf[0] = X2_Y2.cf[0] + dprime[Z] * dprime[Z];
	A.cf[1] = X2_Y2.cf[1] + 2.0 * dprime[Z] * cor_pprime[Z];
	A.cf[2] = X2_Y2.cf[2] + cor_pprime[Z] * cor_pprime[Z] +
		  1.0 - tor->tor_alpha * tor->tor_alpha;

	(void) polyMul( &A, &A, &Asqr );
	(void) polyScal( &X2_Y2, 4.0 );
	(void) polySub( &Asqr, &X2_Y2, &C );

	/*  It is known that the equation is 4th order.  Therefore,
	 *  if the root finder returns other than 4 roots, error.
	 */
	if ( (i = polyRoots( &C, val )) != 4 ){
		rt_log("tor:  polyRoots() 4!=%d\n", i);
		rt_pr_roots( i, val );
		return(SEG_NULL);		/* MISS */
	}

	/*  Only real roots indicate an intersection in real space.
	 *
	 *  Look at each root returned; if the imaginary part is zero
	 *  or sufficiently close, then use the real part as one value
	 *  of 't' for the intersections
	 */
	for ( j=0, i=0; j < 4; j++ ){
		if( NEAR_ZERO( val[j].im, 0.0001 ) )
			k[i++] = val[j].re;
	}

	/* reverse above translation by adding distance to all 'k' values. */
	for( j = 0; j < i; ++j )
		k[j] -= cor_proj;

	/* Here, 'i' is number of points found */
	if( i == 0 )
		return(SEG_NULL);		/* No hit */
	if( i != 2 && i != 4 )  {
		rt_log("tor_shot: reduced 4 to %d roots\n",i);
		rt_pr_roots( 4, val );
		return(SEG_NULL);		/* No hit */
	}

	/* Sort most distant to least distant. */
	TorPtSort( k, i );

	/* Now, t[0] > t[npts-1].  See if this is an easy out. */
	if( k[0] <= 0.0 )
		return(SEG_NULL);		/* No hit out front. */

	/* k[1] is entry point, and k[0] is farthest exit point */
	GET_SEG(segp);
	segp->seg_stp = stp;
	segp->seg_in.hit_dist = k[1]*tor->tor_r1;
	segp->seg_out.hit_dist = k[0]*tor->tor_r1;
	/* Set aside vector for tor_norm() later */
	VJOIN1( segp->seg_in.hit_vpriv, pprime, k[1], dprime );
	VJOIN1( segp->seg_out.hit_vpriv, pprime, k[0], dprime );

	if( i == 2 )
		return(segp);			/* HIT */
				
	/* 4 points */
	/* k[3] is entry point, and k[2] is exit point */
	{
		register struct seg *seg2p;		/* XXX */
		/* Attach last hit (above) to segment chain */
		GET_SEG(seg2p);
		seg2p->seg_next = segp;
		segp = seg2p;
	}
	segp->seg_stp = stp;
	segp->seg_in.hit_dist = k[3]*tor->tor_r1;
	segp->seg_out.hit_dist = k[2]*tor->tor_r1;
	VJOIN1( segp->seg_in.hit_vpriv, pprime, k[3], dprime );
	VJOIN1( segp->seg_out.hit_vpriv, pprime, k[2], dprime );
	return(segp);			/* HIT */
}

/*
 *			T O R _ N O R M
 *
 *  Compute the normal to the torus,
 *  given a point on the UNIT TORUS centered at the origin on the X-Y plane.
 *  The gradient of the torus at that point is in fact the
 *  normal vector, which will have to be given unit length.
 *  To make this useful for the original torus, it will have
 *  to be rotated back to the orientation of the original torus.
 *
 *  Given that the equation for the unit torus is:
 *
 *	[ X**2 + Y**2 + Z**2 + (1 - alpha**2) ]**2 - 4*( X**2 + Y**2 )  =  0
 *
 *  let w = X**2 + Y**2 + Z**2 + (1 - alpha**2), then the equation becomes:
 *
 *	w**2 - 4*( X**2 + Y**2 )  =  0
 *
 *  For f(x,y,z) = 0, the gradient of f() is ( df/dx, df/dy, df/dz ).
 *
 *	df/dx = 2 * w * 2 * x - 8 * x	= (4 * w - 8) * x
 *	df/dy = 2 * w * 2 * y - 8 * y	= (4 * w - 8) * y
 *	df/dz = 2 * w * 2 * z		= 4 * w * z
 */
tor_norm( hitp, stp, rp)
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct tor_specific *tor =
		(struct tor_specific *)stp->st_specific;
	FAST fastf_t w;
	LOCAL vect_t work;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	w = (hitp->hit_vpriv[X]*hitp->hit_vpriv[X] +
	     hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y] +
	     hitp->hit_vpriv[Z]*hitp->hit_vpriv[Z] +
	     1.0 - tor->tor_alpha*tor->tor_alpha) * 4.0;
	VSET( work,
		( w - 8.0 ) * hitp->hit_vpriv[X],
		( w - 8.0 ) * hitp->hit_vpriv[Y],
		  w * hitp->hit_vpriv[Z] );
	VUNITIZE( work );
	MAT3XVEC( hitp->hit_normal, tor->tor_invR, work );
}

/*	>>>  s o r t ( )  <<<
 *
 *  Sorts the values of 't' in descending order.
 *  When done, t[0] > t[npts-1]
 *  The sort is simplified to deal with only 4 values.
 */
static void
TorPtSort( t, npts )
register double	t[];
{
	LOCAL double	u;
	register int	n;

#define TOR_XCH(a,b)	{u=a; a=b; b=u;}
	if( npts == 2 )  {
		if ( t[0] < t[1] )  {
			TOR_XCH( t[0], t[1] );
		}
		return;
	}

	for ( n=0; n < 2; ++n ){
		if ( t[n] < t[n+2] ){
			TOR_XCH( t[n], t[n+2] );
		}
	}
	for ( n=0; n < 3; ++n ){
		if ( t[n] < t[n+1] ){
			TOR_XCH( t[n], t[n+1] );
		}
	}
	return;
}

tor_uv()
{
}
