/*
 *			R E C . C
 *
 *  Purpose -
 *	Intersect a ray with a Right Eliptical Cylinder.
 *	This is a special (but common) case of the TGC,
 *	which is handled separately.
 *
 *  Algorithm -
 *  
 *  Given V, H, A, and B, there is a set of points on this cylinder
 *  
 *  { (x,y,z) | (x,y,z) is on cylinder }
 *  
 *  Through a series of Affine Transformations, this set of points will be
 *  transformed into a set of points on a unit cylinder located at the origin
 *  with a radius of 1, and a height of +1 along the +Z axis.
 *  
 *  { (x',y',z') | (x',y',z') is on cylinder at origin }
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
 *  		(    0    1/|B|   0    ) . X
 *  		 (   0      0   1/|H| )
 *  
 *  To find the intersection of a line with the surface of the cylinder,
 *  consider the parametric line L:
 *  
 *  	L : { P(n) | P + t(n) . D }
 *  
 *  Call W the actual point of intersection between L and the cylinder.
 *  Let W' be the point of intersection between L' and the unit cylinder.
 *  
 *  	L' : { P'(n) | P' + t(n) . D' }
 *  
 *  W = invR( invS( W' ) ) + V
 *  
 *  Where W' = k D' + P'.
 *  
 *  If Dx' and Dy' are both 0, then there is no hit on the cylinder;
 *  but the end plates need checking.
 *
 *  Line L' hits the infinitely tall unit cylinder at W' when
 *
 *	k**2 + b * k + c = 0
 *
 *  where
 *
 *  b = 2 * ( Dx' * Px' + Dy' * Py' ) / ( Dx'**2 + Dy'**2 )
 *  c = ( ( Px'**2 + Py'**2 ) - r**2 ) / ( Dx'**2 + Dy'**2 )
 *  r = 1.0
 *  
 *  The qudratic formula yields k (which is constant):
 *
 *  k = [ -b +/- sqrt( b**2 - 4 * c ] / 2.0
 *  
 *  Now, D' = S( R( D ) )
 *  and  P' = S( R( P - V ) )
 *  
 *  Substituting,
 *  
 *  W = V + invR( invS[ k *( S( R( D ) ) ) + S( R( P - V ) ) ] )
 *    = V + invR( ( k * R( D ) ) + R( P - V ) )
 *    = V + k * D + P - V
 *    = k * D + P
 *  
 *  Note that ``k'' is constant, and is the same in the formulations
 *  for both W and W'.
 *
 *  The hit at ``k'' is a hit on the height=1 unit cylinder IFF
 *  0 <= Wz' <= 1.
 *  
 *  NORMALS.  Given the point W on the surface of the cylinder,
 *  what is the vector normal to the tangent plane at that point?
 *  
 *  Map W onto the unit cylinder, ie:  W' = S( R( W - V ) ).
 *  
 *  Plane on unit cylinder at W' has a normal vector N' of the same value
 *  as W' in x and y, with z set to zero, ie, (Wx', Wy', 0)
 *  
 *  The plane transforms back to the tangent plane at W, and this
 *  new plane (on the original cylinder) has a normal vector of N, viz:
 *  
 *  N = inverse[ transpose(invR o invS) ] ( N' )
 *    = inverse[ transpose(invS) o transpose(invR) ] ( N' )
 *    = inverse[ inverse(S) o R ] ( N' )
 *    = invR o S ( N' )
 *
 *  Note that the normal vector produced above will not have unit length.
 *
 *  THE END PLATES.
 *
 *  If Dz' == 0, line L' is parallel to the end plates, so there is no hit.
 *
 *  Otherwise, the line L' hits the bottom plate with k = (0 - Pz') / Dz',
 *  and hits the top plate with k = (1 - Pz') / Dz'.
 *
 *  The solution W' is within the end plate IFF
 *
 *	Wx'**2 + Wy'**2 <= 1.0
 *
 *  The normal for a hit on the bottom plate is -Hunit, and
 *  the normal for a hit on the top plate is +Hunit.
 *
 *  Authors -
 *	Edwin O. Davisson	(Analysis)
 *	Michael John Muuss	(Programming)
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
static char RCSrec[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

struct rec_specific {
	vect_t	rec_V;		/* Vector to center of base of cylinder  */
	vect_t	rec_Hunit;	/* Unit H vector */
	mat_t	rec_SoR;	/* Scale(Rot(vect)) */
	mat_t	rec_invRoS;	/* invRot(Scale(vect)) */
	vect_t	rec_A;		/* One axis of ellipse */
	vect_t	rec_B;		/* Other axis of ellipse */
	fastf_t	rec_iAsq;	/* 1/MAGSQ(A) */
	fastf_t	rec_iBsq;	/* 1/MAGSQ(B) */
};

#define REC_V	&vec[0*ELEMENTS_PER_VECT]
#define REC_H	&vec[1*ELEMENTS_PER_VECT]
#define REC_A	&vec[2*ELEMENTS_PER_VECT]
#define REC_B	&vec[3*ELEMENTS_PER_VECT]
#define REC_C	&vec[4*ELEMENTS_PER_VECT]
#define REC_D	&vec[5*ELEMENTS_PER_VECT]

/*
 *  			R E C _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid REC,
 *  and if so, precompute various terms of the formulas.
 *  
 *  Returns -
 *  	0	REC is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct rec_specific is created,
 *	and it's address is stored in
 *  	stp->st_specific for use by rec_shot().
 *	If the TGC is really an REC, stp->st_id is modified to ID_REC.
 */
int
rec_prep( vec, stp, mat, rtip )
register fastf_t	*vec;
struct soltab		*stp;
matp_t			mat;
struct rt_i		*rtip;
{
	register struct rec_specific *rec;
	static double	magsq_h, magsq_a, magsq_b, magsq_c, magsq_d;
	static double	mag_h, mag_a, mag_b, mag_c, mag_d;
	static mat_t	R;
	static mat_t	Rinv;
	static mat_t	S;
	static vect_t	Hv, A, B, C, D;
	static vect_t	invsq;	/* [ 1/(|A|**2), 1/(|B|**2), 1/(|Hv|**2) ] */
	static vect_t	work;
	static vect_t	temp;
	static fastf_t	f;

	/* Apply rotation to Hv, A,B,C,D */
	MAT4X3VEC( Hv, mat, REC_H );
	MAT4X3VEC( A, mat, REC_A );
	MAT4X3VEC( B, mat, REC_B );
	MAT4X3VEC( C, mat, REC_C );
	MAT4X3VEC( D, mat, REC_D );

	/* Validate that |H| > 0, compute |A| |B| |C| |D| */
	mag_h = sqrt( magsq_h = MAGSQ( Hv ) );
	mag_a = sqrt( magsq_a = MAGSQ( A ) );
	mag_b = sqrt( magsq_b = MAGSQ( B ) );
	mag_c = sqrt( magsq_c = MAGSQ( C ) );
	mag_d = sqrt( magsq_d = MAGSQ( D ) );

	/* Check for |H| > 0, |A| > 0, |B| > 0 */
	if( NEAR_ZERO(mag_h, RT_LEN_TOL) || NEAR_ZERO(mag_a, RT_LEN_TOL)
	 || NEAR_ZERO(mag_b, RT_LEN_TOL) )  {
		return(1);		/* BAD, too small */
	}

	/* Make sure that A == C, B == D */
	VSUB2( work, A, C );
	f = MAGSQ( work );
	if( ! NEAR_ZERO(f, 0.0001) )  {
		return(1);		/* BAD, !cylinder */
	}
	VSUB2( work, B, D );
	f = MAGSQ( work );
	if( ! NEAR_ZERO(f, 0.0001) )  {
		return(1);		/* BAD, !cylinder */
	}

	/* Check for A.B == 0, H.A == 0 and H.B == 0 */
	f = VDOT( A, B ) / (mag_a * mag_b);
	if( ! NEAR_ZERO(f, RT_DOT_TOL) )  {
		return(1);		/* BAD */
	}
	f = VDOT( Hv, A ) / (mag_h * mag_a);
	if( ! NEAR_ZERO(f, RT_DOT_TOL) )  {
		return(1);		/* BAD */
	}
	f = VDOT( Hv, B ) / (mag_h * mag_b);
	if( ! NEAR_ZERO(f, RT_DOT_TOL) )  {
		return(1);		/* BAD */
	}

	/*
	 *  This TGC is really an REC
	 */
	stp->st_id = ID_REC;		/* "fix" soltab ID */

	GETSTRUCT( rec, rec_specific );
	stp->st_specific = (int *)rec;

	VMOVE( rec->rec_Hunit, Hv );
	VUNITIZE( rec->rec_Hunit );

	{
		register fastf_t *p = REC_V;
		MAT4X3PNT( rec->rec_V, mat, p );
	}
	VMOVE( rec->rec_A, REC_A );
	VMOVE( rec->rec_B, REC_B );
	rec->rec_iAsq = 1.0/magsq_a;
	rec->rec_iBsq = 1.0/magsq_b;

	VSET( invsq, 1.0/magsq_a, 1.0/magsq_b, 1.0/magsq_h );

	/* Compute R and Rinv matrices */
	mat_idn( R );
	f = 1.0/mag_a;
	VSCALE( &R[0], A, f );
	f = 1.0/mag_b;
	VSCALE( &R[4], B, f );
	f = 1.0/mag_h;
	VSCALE( &R[8], Hv, f );
	mat_trn( Rinv, R );			/* inv of rot mat is trn */

	/* Compute S */
	mat_idn( S );
	S[ 0] = sqrt( invsq[0] );
	S[ 5] = sqrt( invsq[1] );
	S[10] = sqrt( invsq[2] );

	/* Compute SoR and invRoS */
	mat_mul( rec->rec_SoR, S, R );
	mat_mul( rec->rec_invRoS, Rinv, S );

	/* Compute bounding sphere and RPP */
	{
		LOCAL fastf_t	dx, dy, dz;	/* For bounding sphere */
		LOCAL vect_t	P, w1;
		LOCAL fastf_t	f, tmp, z;

		/* X */
		VSET( P, 1.0, 0, 0 );		/* bounding plane normal */
		MAT3X3VEC( w1, R, P );		/* map plane into local coord syst */
		/* 1st end ellipse (no Z part) */
		tmp = magsq_a * w1[X] * w1[X] + magsq_b * w1[Y] * w1[Y];
		if( tmp > SMALL )
			f = tmp/sqrt(tmp);	/* XY part */
		else
			f = 0;
		stp->st_min[X] = rec->rec_V[X] - f;	/* V.P +/- f */
		stp->st_max[X] = rec->rec_V[X] + f;
		/* 2nd end ellipse */
		z = w1[Z] * mag_h;		/* Z part */
		tmp = magsq_c * w1[X] * w1[X] + magsq_d * w1[Y] * w1[Y];
		if( tmp > SMALL )
			f = tmp/sqrt(tmp);	/* XY part */
		else
			f = 0;
		if( rec->rec_V[X] - f + z < stp->st_min[X] )
			stp->st_min[X] = rec->rec_V[X] - f + z;
		if( rec->rec_V[X] + f + z > stp->st_max[X] )
			stp->st_max[X] = rec->rec_V[X] + f + z;

		/* Y */
		VSET( P, 0, 1.0, 0 );		/* bounding plane normal */
		MAT3X3VEC( w1, R, P );		/* map plane into local coord syst */
		/* 1st end ellipse (no Z part) */
		tmp = magsq_a * w1[X] * w1[X] + magsq_b * w1[Y] * w1[Y];
		if( tmp > SMALL )
			f = tmp/sqrt(tmp);	/* XY part */
		else
			f = 0;
		stp->st_min[Y] = rec->rec_V[Y] - f;	/* V.P +/- f */
		stp->st_max[Y] = rec->rec_V[Y] + f;
		/* 2nd end ellipse */
		z = w1[Z] * mag_h;		/* Z part */
		tmp = magsq_c * w1[X] * w1[X] + magsq_d * w1[Y] * w1[Y];
		if( tmp > SMALL )
			f = tmp/sqrt(tmp);	/* XY part */
		else
			f = 0;
		if( rec->rec_V[Y] - f + z < stp->st_min[Y] )
			stp->st_min[Y] = rec->rec_V[Y] - f + z;
		if( rec->rec_V[Y] + f + z > stp->st_max[Y] )
			stp->st_max[Y] = rec->rec_V[Y] + f + z;

		/* Z */
		VSET( P, 0, 0, 1.0 );		/* bounding plane normal */
		MAT3X3VEC( w1, R, P );		/* map plane into local coord syst */
		/* 1st end ellipse (no Z part) */
		tmp = magsq_a * w1[X] * w1[X] + magsq_b * w1[Y] * w1[Y];
		if( tmp > SMALL )
			f = tmp/sqrt(tmp);	/* XY part */
		else
			f = 0;
		stp->st_min[Z] = rec->rec_V[Z] - f;	/* V.P +/- f */
		stp->st_max[Z] = rec->rec_V[Z] + f;
		/* 2nd end ellipse */
		z = w1[Z] * mag_h;		/* Z part */
		tmp = magsq_c * w1[X] * w1[X] + magsq_d * w1[Y] * w1[Y];
		if( tmp > SMALL )
			f = tmp/sqrt(tmp);	/* XY part */
		else
			f = 0;
		if( rec->rec_V[Z] - f + z < stp->st_min[Z] )
			stp->st_min[Z] = rec->rec_V[Z] - f + z;
		if( rec->rec_V[Z] + f + z > stp->st_max[Z] )
			stp->st_max[Z] = rec->rec_V[Z] + f + z;

		VSET( stp->st_center,
			(stp->st_max[X] + stp->st_min[X])/2,
			(stp->st_max[Y] + stp->st_min[Y])/2,
			(stp->st_max[Z] + stp->st_min[Z])/2 );

		dx = (stp->st_max[X] - stp->st_min[X])/2;
		f = dx;
		dy = (stp->st_max[Y] - stp->st_min[Y])/2;
		if( dy > f )  f = dy;
		dz = (stp->st_max[Z] - stp->st_min[Z])/2;
		if( dz > f )  f = dz;
		stp->st_aradius = f;
		stp->st_bradius = sqrt(dx*dx + dy*dy + dz*dz);
	}
	return(0);			/* OK */
}

/*
 *  			R E C _ P R I N T
 */
void
rec_print( stp )
register struct soltab *stp;
{
	register struct rec_specific *rec =
		(struct rec_specific *)stp->st_specific;

	VPRINT("V", rec->rec_V);
	VPRINT("Hunit", rec->rec_Hunit);
	mat_print("S o R", rec->rec_SoR );
	mat_print("invR o S", rec->rec_invRoS );
}

/* To be clean, hit_private (a char *), is set to one of these */
static char rec_compute[4];

/*
 *  			R E C _ S H O T
 *  
 *  Intersect a ray with a right elliptical cylinder,
 *  where all constant terms have
 *  been precomputed by rec_prep().  If an intersection occurs,
 *  a struct seg will be acquired and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *  	segp	HIT
 */
struct seg *
rec_shot( stp, rp, ap )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
{
	register struct rec_specific *rec =
		(struct rec_specific *)stp->st_specific;
	LOCAL vect_t	dprime;		/* D' */
	LOCAL vect_t	pprime;		/* P' */
	LOCAL fastf_t	k1, k2;		/* distance constants of solution */
	LOCAL vect_t	xlated;		/* translated vector */
	LOCAL struct hit hits[3];	/* 4 potential hit points */
	register struct hit *hitp;	/* pointer to hit point */

	hitp = &hits[0];

	/* out, Mat, vect */
	MAT4X3VEC( dprime, rec->rec_SoR, rp->r_dir );
	VSUB2( xlated, rp->r_pt, rec->rec_V );
	MAT4X3VEC( pprime, rec->rec_SoR, xlated );

	if( NEAR_ZERO(dprime[X], SMALL) && NEAR_ZERO(dprime[Y], SMALL) )
		goto check_plates;

	/* Find roots of the equation, using forumla for quadratic w/ a=1 */
	{
		FAST fastf_t	b;		/* coeff of polynomial */
		FAST fastf_t	root;		/* root of radical */
		FAST fastf_t	dx2dy2;

		b = 2 * ( dprime[X]*pprime[X] + dprime[Y]*pprime[Y] ) * 
		   (dx2dy2 = 1 / (dprime[X]*dprime[X] + dprime[Y]*dprime[Y]));
		if( (root = b*b - 4 * dx2dy2 *
		    (pprime[X]*pprime[X] + pprime[Y]*pprime[Y] - 1)) <= 0 )
			goto check_plates;
		root = sqrt(root);

		if( (k1 = (root-b) * 0.5) > (k2 = (root+b) * (-0.5)) )  {
			root = k2;
			k2 = k1;
			k1 = root;
		}
	}

	/*
	 *  k1 and k2 are potential solutions to intersection with side.
	 *  See if they fall in range.
	 */
	VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );		/* hit' */
	if( hitp->hit_vpriv[Z] >= 0.0 && hitp->hit_vpriv[Z] <= 1.0 ) {
		hitp->hit_dist = k1;
		hitp->hit_private = &rec_compute[0];	/* compute N */
		hitp++;
	}

	VJOIN1( hitp->hit_vpriv, pprime, k2, dprime );		/* hit' */
	if( hitp->hit_vpriv[Z] >= 0.0 && hitp->hit_vpriv[Z] <= 1.0 )  {
		hitp->hit_dist = k2;
		hitp->hit_private = &rec_compute[0];	/* compute N */
		hitp++;
	}

	/*
	 * Check for hitting the end plates.
	 */
check_plates:
	if( hitp < &hits[2]  &&  !NEAR_ZERO(dprime[Z], SMALL) )  {
		/* 0 or 1 hits so far, this is worthwhile */
		k1 = -pprime[Z] / dprime[Z];		/* bottom plate */
		k2 = (1.0 - pprime[Z]) / dprime[Z];	/* top plate */

		VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );	/* hit' */
		if( hitp->hit_vpriv[X] * hitp->hit_vpriv[X] +
		    hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y] <= 1.0 )  {
			hitp->hit_dist = k1;
			hitp->hit_private = &rec_compute[2];	/* -H */
			hitp++;
		}

		VJOIN1( hitp->hit_vpriv, pprime, k2, dprime );	/* hit' */
		if( hitp->hit_vpriv[X] * hitp->hit_vpriv[X] +
		    hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y] <= 1.0 )  {
			hitp->hit_dist = k2;
			hitp->hit_private = &rec_compute[1];	/* +H */
			hitp++;
		}
	}
	if( hitp != &hits[2] )
		return(SEG_NULL);	/* MISS */

	if( hits[0].hit_dist < hits[1].hit_dist )  {
		/* entry is [0], exit is [1] */
		register struct seg *segp;

		GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in = hits[0];		/* struct copy */
		segp->seg_out = hits[1];	/* struct copy */
		return(segp);			/* HIT */
	} else {
		/* entry is [1], exit is [0] */
		register struct seg *segp;

		GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in = hits[1];		/* struct copy */
		segp->seg_out = hits[0];	/* struct copy */
		return(segp);			/* HIT */
	}
	/* NOTREACHED */
}

/*
 *  			R E C _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 *  hit_private is a flag indicating if normal needs to be computed or not.
 */
void
rec_norm( hitp, stp, rp )
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct rec_specific *rec =
		(struct rec_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	switch( hitp->hit_private-rec_compute )  {
	case 0:
		/* compute it */
		hitp->hit_vpriv[Z] = 0.0;
		MAT4X3VEC( hitp->hit_normal, rec->rec_invRoS,
			hitp->hit_vpriv );
		VUNITIZE( hitp->hit_normal );
		break;
	case 1:
		VMOVE( hitp->hit_normal, rec->rec_Hunit );
		break;
	case 2:
		VREVERSE( hitp->hit_normal, rec->rec_Hunit );
		break;
	default:
		rt_log("rec_norm: bad flag x%x\n", (int)hitp->hit_private);
		break;
	}
}

/*
 *			R E C _ C U R V E
 *
 *  Return the "curvature" of the cylinder.  If an endplate,
 *  pick a principle direction orthogonal to the normal, and 
 *  indicate no curvature.  Otherwise, compute curvature.
 *  Normal must have been computed before calling this routine.
 */
void
rec_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{
	register struct rec_specific *rec =
		(struct rec_specific *)stp->st_specific;
	vect_t	uu;
	fastf_t	ax, bx, q;

	switch( hitp->hit_private-rec_compute )  {
	case 0:
		/* This could almost certainly be simpler if we used
		 * inverse A rather than inverse A squared, right Ed?
		 */
		VMOVE( cvp->crv_pdir, rec->rec_Hunit );
		VSUB2( uu, hitp->hit_point, rec->rec_V );
		cvp->crv_c1 = 0;
		ax = VDOT( uu, rec->rec_A ) * rec->rec_iAsq;
		bx = VDOT( uu, rec->rec_B ) * rec->rec_iBsq;
		q = sqrt( ax * ax * rec->rec_iAsq + bx * bx * rec->rec_iBsq );
		cvp->crv_c2 = - rec->rec_iAsq * rec->rec_iBsq / (q*q*q);
		break;
	case 1:
	case 2:
		vec_ortho( cvp->crv_pdir, hitp->hit_normal );
		cvp->crv_c1 = cvp->crv_c2 = 0;
		break;
	default:
		rt_log("rec_curve: bad flag x%x\n", (int)hitp->hit_private);
		break;
	}
}

/*
 *  			R E C _ U V
 *  
 *  For a hit on the surface of an REC, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *
 *  u is the rotation around the cylinder, and
 *  v is the displacement along H.
 */
extern double rt_inv2pi;

void
rec_uv( ap, stp, hitp, uvp )
struct application *ap;
struct soltab *stp;
register struct hit *hitp;
register struct uvcoord *uvp;
{
	register struct rec_specific *rec =
		(struct rec_specific *)stp->st_specific;
	LOCAL vect_t work;
	LOCAL vect_t pprime;
	FAST fastf_t len;

	/* hit_point is on surface;  project back to unit cylinder,
	 * creating a vector from vertex to hit point.
	 */
	VSUB2( work, hitp->hit_point, rec->rec_V );
	MAT4X3VEC( pprime, rec->rec_SoR, work );

	switch( hitp->hit_private-rec_compute )  {
	case 0:
		/* Skin.  x,y coordinates define rotation.  radius = 1 */
		uvp->uv_u = acos(pprime[Y]) * rt_inv2pi;
		uvp->uv_v = pprime[Z];		/* height */
		break;
	case 1:
		/* top plate */
		len = sqrt(pprime[X]*pprime[X]+pprime[Y]*pprime[Y]);
		uvp->uv_u = acos(pprime[Y]/len) * rt_inv2pi;
		uvp->uv_v = len;		/* rim v = 1 */
		break;
	case 2:
		/* bottom plate */
		len = sqrt(pprime[X]*pprime[X]+pprime[Y]*pprime[Y]);
		uvp->uv_u = acos(pprime[Y]/len) * rt_inv2pi;
		uvp->uv_v = 1 - len;	/* rim v = 0 */
		break;
	}
	/* Handle other half of acos() domain */
	if( pprime[X] < 0 )
		uvp->uv_u = 1.0 - uvp->uv_u;

	/* uv_du should be relative to rotation, uv_dv relative to height */
	uvp->uv_du = uvp->uv_dv = 0;
}

/*
 *			R E C _ F R E E
 */
void
rec_free( stp )
struct soltab *stp;
{
	register struct rec_specific *rec =
		(struct rec_specific *)stp->st_specific;

	rt_free( (char *)rec, "rec_specific");
}

int
rec_class()
{
	return(0);
}

void
rec_plot()
{
}
