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
 *
 * U. S. Army Ballistic Research Laboratory
 * August, 1984
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "vmath.h"
#include "raytrace.h"
#include "db.h"
#include "debug.h"

#include "./polyno.h"
#include "./complex.h"

HIDDEN int	stdTorus();
static void	PtSort();

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
 *  and S(X) =	 (  1/|H|   0     0   )
 *  		(    0    1/|H|   0    ) . X
 *  		 (   0      0   1/|H| )
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

#define SP_V	&vec[0*ELEMENTS_PER_VECT]
#define SP_H	&vec[1*ELEMENTS_PER_VECT]
#define SP_A	&vec[2*ELEMENTS_PER_VECT]
#define SP_B	&vec[3*ELEMENTS_PER_VECT]

	/*
	 * Apply 3x3 rotation mat only to A,B,H
	 */
	MAT4X3VEC( A, mat, SP_A );
	MAT4X3VEC( B, mat, SP_B );
	MAT4X3VEC( Hv, mat, SP_H );

	magsq_a = MAGSQ( A );
	magsq_b = MAGSQ( B );
	magsq_h = MAGSQ( Hv );
	r1 = sqrt(magsq_a);
	r2 = sqrt(magsq_h);

	/* Validate that |A| > 0, |B| > 0, |H| > 0 */
	if( NEAR_ZERO(magsq_a) || NEAR_ZERO(magsq_b) || NEAR_ZERO(magsq_h) ) {
		fprintf(stderr,"tor(%s):  zero length A, B, or H vector\n",
			stp->st_name );
		return(1);		/* BAD */
	}

	/* Validate that |A| == |B| (for now) */
	f = r1 - sqrt(magsq_b);
	if( ! NEAR_ZERO(f) )  {
		fprintf(stderr,"tor(%s):  (|A|=%f) != (|B|=%f) \n",
			stp->st_name, r1, sqrt(magsq_b) );
		return(1);		/* BAD */
	}

	/* Validate that A.B == 0, B.H == 0, A.H == 0 */
	f = VDOT( A, B );
	if( !NEAR_ZERO(f) )  {
		/* perhaps something with reldiff() might be better? */
		fprintf(stderr,"tor(%s):  A not perpendicular to B, f=%f\n",stp->st_name, f);
		return(1);		/* BAD */
	}
	f = VDOT( B, Hv );
	if( !NEAR_ZERO(f) )  {
		fprintf(stderr,"tor(%s):  B not perpendicular to H, f=%f\n",stp->st_name, f);
		return(1);		/* BAD */
	}
	f = VDOT( A, Hv );
	if( !NEAR_ZERO(f) )  {
		fprintf(stderr,"tor(%s):  A not perpendicular to H, f=%f\n",stp->st_name, f);
		return(1);		/* BAD */
	}

	/* Validate that 0 < r2 <= r1 for alpha computation */
	if( 0.0 >= r2  || r2 > r1 )  {
		fprintf(stderr,"r1 = %f, r2 = %f\n", r1, r2 );
		fprintf(stderr,"tor(%s):  0 < r2 <= r1 is not true\n", stp->st_name);
		return(1);		/* BAD */
	}

	/* Solid is OK, compute constant terms now */
	GETSTRUCT( tor, tor_specific );
	stp->st_specific = (int *)tor;

	MAT4X3PNT( tor->tor_V, mat, SP_V );
	tor->tor_alpha = r2/r1;

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
	tor->tor_SoR[15] *= r1;

	/* Compute bounding sphere */
	VMOVE( stp->st_center, tor->tor_V );
	f = r1 + r2;
	stp->st_radsq = f * f;

	/* Compute bounding RPP */
#define MINMAX(a,b,c)	{ FAST fastf_t ftemp;\
			if( (ftemp = (c)) < (a) )  a = ftemp;\
			if( ftemp > (b) )  b = ftemp; }

#define MM(v)	MINMAX( stp->st_min[X], stp->st_max[X], v[X] ); \
		MINMAX( stp->st_min[Y], stp->st_max[Y], v[Y] ); \
		MINMAX( stp->st_min[Z], stp->st_max[Z], v[Z] )

	/* Exterior radius is r1+r2;  rescale A and B here */
	f = r1+r2;
	VUNITIZE( A );
	VSCALE( A, A, f );
	VUNITIZE( B );
	VSCALE( B, B, f );
	VSCALE( Hv, Hv, r2 );

	/* There are 8 corners to the enclosing RPP;  find max and min */
	VADD3( temp, tor->tor_V, B, Hv );
	VADD2( work, temp, A ); MM( work );	/* V + A + B + Hv */
	VSUB2( work, temp, A ); MM( work );	/* V - A + B + Hv */

	VSUB2( temp, tor->tor_V, B );
	VADD2( temp, temp, Hv );
	VADD2( work, temp, A ); MM( work );	/* V + A - B + Hv */
	VSUB2( work, temp, A ); MM( work );	/* V - A - B + Hv */
	
	VSUB2( temp, tor->tor_V, Hv );
	VADD2( temp, temp, B );
	VADD2( work, temp, A ); MM( work );	/* V + A + B - Hv */
	VSUB2( work, temp, A ); MM( work );	/* V - A + B - Hv */

	VSUB3( temp, tor->tor_V, B, Hv );
	VADD2( work, temp, A ); MM( work );	/* V + A - B - Hv */
	VSUB2( work, temp, A ); MM( work );	/* V - A - B - Hv */

	return(0);			/* OK */
}

tor_print( stp )
register struct soltab *stp;
{
	register struct tor_specific *tor =
		(struct tor_specific *)stp->st_specific;

	fprintf(stderr,"r2/r1 (alpha) = %f\n", tor->tor_alpha);
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

	/* Convert vector into the space of the unit torus */
	MAT4X3VEC( dprime, tor->tor_SoR, rp->r_dir );
	VSUB2( work, rp->r_pt, tor->tor_V );
	MAT4X3VEC( pprime, tor->tor_SoR, work );

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
	X2_Y2.cf[1] = 2.0 * (dprime[X] * pprime[X] + dprime[Y] * pprime[Y]);
	X2_Y2.cf[2] = pprime[X] * pprime[X] + pprime[Y] * pprime[Y];

	/* A = X2_Y2 + Z2 */
	A.dgr = 2;
	A.cf[0] = X2_Y2.cf[0] + dprime[Z] * dprime[Z];
	A.cf[1] = X2_Y2.cf[1] + 2.0 * dprime[Z] * pprime[Z];
	A.cf[2] = X2_Y2.cf[2] + pprime[Z] * pprime[Z] +
		  1.0 - tor->tor_alpha * tor->tor_alpha;

	(void) polyMul( &A, &A, &Asqr );
	(void) polyScal( &X2_Y2, 4.0 );
	(void) polySub( &Asqr, &X2_Y2, &C );

	/*  It is known that the equation is 4th order.  Therefore,
	 *  if the root finder returns other than 4 roots, error.
	 */
	if ( (i = polyRoots( &C, val )) != 4 ){
		fprintf(stderr,"stdTorus:  polyRoots() 4!=%d\n", i);
		pr_roots( i, val );
		return(SEG_NULL);		/* MISS */
	}

	/*  Only real roots indicate an intersection in real space.
	 *
	 *  Look at each root returned; if the imaginary part is zero
	 *  or sufficiently close, then use the real part as one value
	 *  of 't' for the intersections
	 */
	for ( j=0, i=0; j < 4; j++ ){
		if( NEAR_ZERO( val[j].im ) )
			k[i++] = val[j].re;
	}
	/* Here, 'i' is number of points found */
	if( i == 0 )
		return(SEG_NULL);		/* No hit */
	if( i != 2 && i != 4 )  {
		fprintf(stderr,"tor_shot: reduced 4 to %d roots\n",i);
		pr_roots( 4, val );
		return(SEG_NULL);		/* No hit */
	}

	/* Most distant to least distant */
	PtSort( k, i );

	/* k[1] is entry point, and k[0] is exit point */
	GET_SEG(segp);
	segp->seg_stp = stp;

	segp->seg_in.hit_dist = k[1];
	segp->seg_out.hit_dist = k[0];
	segp->seg_flag = SEG_IN | SEG_OUT;

	/* Intersection point, entering */
	VJOIN1( segp->seg_in.hit_point, rp->r_pt, k[1], rp->r_dir );
	VJOIN1( work, pprime, k[1], dprime );
	tornormal( segp->seg_in.hit_normal, work, tor );

	/* Intersection point, exiting */
	VJOIN1( segp->seg_out.hit_point, rp->r_pt, k[0], rp->r_dir );
	VJOIN1( work, pprime, k[0], dprime );
	tornormal( segp->seg_out.hit_normal, work, tor );

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
	segp->seg_in.hit_dist = k[3];
	segp->seg_out.hit_dist = k[2];
	segp->seg_flag = SEG_IN | SEG_OUT;

	/* Intersection point, entering */
	VJOIN1( segp->seg_in.hit_point, rp->r_pt, k[3], rp->r_dir );
	VJOIN1( work, pprime, k[3], dprime );
	tornormal( segp->seg_in.hit_normal, work, tor );

	/* Intersection point, exiting */
	VJOIN1( segp->seg_out.hit_point, rp->r_pt, k[2], rp->r_dir );
	VJOIN1( work, pprime, k[2], dprime );
	tornormal( segp->seg_out.hit_normal, work, tor );

	return(segp);			/* HIT */
}

#define X	0
#define Y	1
#define Z	2
/*
 *			T O R N O R M A L
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
tornormal( norm, hit, tor )
register vectp_t norm, hit;
register struct tor_specific *tor;
{
	FAST fastf_t w;
	LOCAL vect_t work;

	w = hit[X]*hit[X] + hit[Y]*hit[Y] + hit[Z]*hit[Z] +
	    1.0 - tor->tor_alpha*tor->tor_alpha;
	VSET( work,
		(4.0 * w - 8.0 ) * hit[X],
		(4.0 * w - 8.0 ) * hit[Y],
		4.0 * w * hit[Z] );
	VUNITIZE( work );
	MAT3XVEC( norm, tor->tor_invR, work );
}

/*	>>>  s o r t ( )  <<<
 *
 *  Sorts the values of 't' in descending order.  The sort is
 *  simplified to deal with only 4 values.  Returns the address
 *  of the first 't' in the array.
 */
static void
PtSort( t, npts )
register double	t[];
{
	LOCAL double	u;
	register int	n;

#define XCH(a,b)	{u=a; a=b; b=u;}
	if( npts == 2 )  {
		if ( t[0] < t[1] )  {
			XCH( t[0], t[1] );
		}
		return;
	}

	for ( n=0; n < 2; ++n ){
		if ( t[n] < t[n+2] ){
			XCH( t[n], t[n+2] );
		}
	}
	for ( n=0; n < 3; ++n ){
		if ( t[n] < t[n+1] ){
			XCH( t[n], t[n+1] );
		}
	}
	return;
}
