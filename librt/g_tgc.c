/*
 *			T G C . C
 *
 * Purpose -
 *	Intersect a ray with a Truncated General Cone.
 *
 * Method -
 *	TGC:  unknown
 *	REC:  a (common) special case, much easier.
 *
 * Authors -
 *	Edwin O. Davisson	(Analysis)
 *	Michael John Muuss	(Programming)
 *
 * U. S. Army Ballistic Research Laboratory
 * April 26, 1984
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "vmath.h"
#include "ray.h"
#include "db.h"

/* Define vector elements */
#define X	0
#define Y	1
#define Z	2

struct rec_specific {
	vect_t	rec_V;		/* Vector to center of base of cylinder  */
	vect_t	rec_Hunit;	/* Unit H vector */
	mat_t	rec_SoR;	/* Scale(Rot(vect)) */
	mat_t	rec_invRoS;	/* invRot(Scale(vect)) */
};

struct tgc_specific {
	/* Entirely unknown */
	vect_t	tgc_V;		/* Vector to center of base of TGC */
	mat_t	tgc_SoR;	/* Scale(Rot(vect)) */
	mat_t	tgc_invRSSR;	/* invRot(Scale(Scale(Rot(vect)))) */
};

#define SP_V	&vec[0*ELEMENTS_PER_VECT]
#define SP_H	&vec[1*ELEMENTS_PER_VECT]
#define SP_A	&vec[2*ELEMENTS_PER_VECT]
#define SP_B	&vec[3*ELEMENTS_PER_VECT]
#define SP_C	&vec[4*ELEMENTS_PER_VECT]
#define SP_D	&vec[5*ELEMENTS_PER_VECT]

#define MINMAX(a,b,c)	{ FAST fastf_t ftemp;\
			if( (ftemp = (c)) < (a) )  a = ftemp;\
			if( ftemp > (b) )  b = ftemp; }

/*
 *  			T G C _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid TGC, or perhaps the simpler REC,
 *  and if so, precompute various terms of the formulas.
 *  
 *  Returns -
 *  	0	TGC is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct rec_specific or tgc_specific is created,
 *	and it's address is stored in
 *  	stp->st_specific for use by rec_shot() or tgc_shot().
 *	If the TGC is really an REC, stp->st_id is modified to ID_REC.
 */
tgc_prep( vec, stp, mat )
register fastf_t *vec;
struct soltab *stp;
matp_t mat;			/* Homogenous 4x4, with translation, [15]=1 */
{
	register struct rec_specific *rec;
	static double	magsq_h, magsq_a, magsq_b, magsq_c, magsq_d;
	static mat_t	R;
	static mat_t	Rinv;
	static mat_t	S;
	static vect_t	H, A, B, C, D;
	static vect_t	invsq;	/* [ 1/(|A|**2), 1/(|B|**2), 1/(|H|**2) ] */
	static vect_t	work;
	static vect_t	temp;
	static fastf_t	f;

	/* Apply 3x3 rotation portion of mat to H, A,B,C,D */
	MAT3XVEC( H, mat, SP_H );
	MAT3XVEC( A, mat, SP_A );
	MAT3XVEC( B, mat, SP_B );
	MAT3XVEC( C, mat, SP_C );
	MAT3XVEC( D, mat, SP_D );

	/* Validate that |H| > 0, compute |A| |B| |C| |D| */
	magsq_h = MAGSQ( H );
	magsq_a = MAGSQ( A );
	magsq_b = MAGSQ( B );
	magsq_c = MAGSQ( C );
	magsq_d = MAGSQ( D );
	if( NEAR_ZERO(magsq_h) )  {
		printf("tgc(%s):  zero length H vector\n", stp->st_name );
		return(1);		/* BAD */
	}

	/* Validate that A.B == 0, C.D == 0 */
	f = VDOT( A, B );
	if( ! NEAR_ZERO(f) )  {
		printf("tgc(%s):  A not perpendicular to B\n",stp->st_name);
		return(1);		/* BAD */
	}
	f = VDOT( C, D );
	if( ! NEAR_ZERO(f) )  {
		printf("tgc(%s):  C not perpendicular to D\n",stp->st_name);
		return(1);		/* BAD */
	}

	/*
	 *  See if this TGC is an REC.  If so, much simpler formulas can
	 *  be used, so we will treat it as a separate case.
	 */

	/* Check for H.A == 0 and H.B == 0 */
	f = VDOT( H, A );
	if( ! NEAR_ZERO(f) )
		goto hard;
	f = VDOT( H, B );
	if( ! NEAR_ZERO(f) )
		goto hard;

	/* Check for |A| > 0, |B| > 0 */
	if( NEAR_ZERO(magsq_a) || NEAR_ZERO(magsq_b) )
		goto hard;

	/* Make sure that A == C, B == D */
	VSUB2( work, A, C );
	f = MAGSQ( work );
	if( ! NEAR_ZERO(f) )
		goto hard;
	VSUB2( work, B, D );
	f = MAGSQ( work );
	if( ! NEAR_ZERO(f) )
		goto hard;

	/*
	 *  This TGC is really an REC;  use simpler formulas.
	 */
	stp->st_id = ID_REC;		/* "fix" soltab ID */

	GETSTRUCT( rec, rec_specific );
	stp->st_specific = (int *)rec;

	VMOVE( rec->rec_Hunit, H );
	VUNITIZE( rec->rec_Hunit );

	/* Apply full 4x4mat to V.  No need for htov_vec, as [15]==0. */
	VMOVE( work, SP_V );
	matXvec( rec->rec_V, mat, work );

	VSET( invsq, 1.0/magsq_a, 1.0/magsq_b, 1.0/magsq_h );

	/* Compute R and Rinv matrices */
	mat_zero( R );
	f = 1.0/sqrt(magsq_a);
	VSCALE( &R[0], A, f );
	f = 1.0/sqrt(magsq_b);
	VSCALE( &R[4], B, f );
	f = 1.0/sqrt(magsq_h);
	VSCALE( &R[8], H, f );
	mat_trn( Rinv, R );			/* inv of rot mat is trn */

	/* Compute S.  Uses 3x3 of the 4x4 matrix */
	mat_zero( S );
	S[ 0] = sqrt( invsq[0] );
	S[ 5] = sqrt( invsq[1] );
	S[10] = sqrt( invsq[2] );

	/* Compute invRoS */
	mat_mul( rec->rec_invRoS, Rinv, S );

	/* Compute SoR */
	mat_zero( rec->rec_SoR );
	VSCALE( &rec->rec_SoR[0], A, invsq[0] );
	VSCALE( &rec->rec_SoR[4], B, invsq[1] );
	VSCALE( &rec->rec_SoR[8], H, invsq[2] );

	{
		static fastf_t xmax, ymax, zmax;/* For bounding sphere */
		static fastf_t xmin, ymin, zmin;/* For bounding sphere */
		static fastf_t dx, dy, dz;	/* For bounding sphere */

		/* init maxima and minima */
		xmax = ymax = zmax = -INFINITY;
		xmin = ymin = zmin =  INFINITY;

		/* Compute bounding sphere */
#define MM(v)	MINMAX( xmin, xmax, v[X] ); \
		MINMAX( ymin, ymax, v[Y] ); \
		MINMAX( zmin, zmax, v[Z] )

		VADD2( work, rec->rec_V, A ); MM( work );
		VSUB2( work, rec->rec_V, A ); MM( work );
		VADD2( work, rec->rec_V, B ); MM( work );
		VSUB2( work, rec->rec_V, B ); MM( work );

		VADD2( temp, rec->rec_V, H );
		VADD2( work, temp, A );  MM( work );
		VSUB2( work, temp, A );  MM( work );
		VADD2( work, temp, B );  MM( work );
		VSUB2( work, temp, B );  MM( work );

		VSET( stp->st_center,
			(xmax + xmin)/2, (ymax + ymin)/2, (zmax + zmin)/2 );

		dx = (xmax - xmin)/2;
		dy = (ymax - ymin)/2;
		dz = (zmax - zmin)/2;
		stp->st_radsq = dx*dx + dy*dy + dz*dz;
	}
	return(0);			/* OK */

	/*
	 *  This TGC is not an REC;  use completely general forumlas.
	 */
hard:
	printf("tgc(%s):  too hard\n", stp->st_name );
	return(1);		/* BAD */

#ifdef later
	/* Solid is OK, compute constant terms now */
	GETSTRUCT( tgc, tgc_specific );
	stp->st_specific = (int *)tgc;
#endif
}

/*
 *  			T G C _ P R I N T
 */
tgc_print( stp )
register struct soltab *stp;
{
	register struct tgc_specific *tgc =
		(struct tgc_specific *)stp->st_specific;

	VPRINT("V", tgc->tgc_V);
	mat_print("S o R", tgc->tgc_SoR );
}

/*
 *  			T G C _ S H O T
 *  
 *  Intersect a ray with an ellipsoid, where all constant terms have
 *  been precomputed by tgc_prep().  If an intersection occurs,
 *  a struct seg will be acquired and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *  	segp	HIT
 */
struct seg *
tgc_shot( stp, rp )
struct soltab *stp;
register struct ray *rp;
{
	register struct tgc_specific *tgc =
		(struct tgc_specific *)stp->st_specific;
	register struct seg *segp;

	printf("tgc_shot(%s):  unimplemented\n", stp->st_name );
	return( SEG_NULL );
}


/*
 *			Right Eliptical Cylinder
 *
 * Algorithm:
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
 */

/*
 *  			R E C _ P R I N T
 */
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
rec_shot( stp, rp )
struct soltab *stp;
register struct ray *rp;
{
	register struct rec_specific *rec =
		(struct rec_specific *)stp->st_specific;
	register struct seg *segp;
	static vect_t	dprime;		/* D' */
	static vect_t	pprime;		/* P' */
	static fastf_t	dx2dy2;		/* 1/(D'[x]**2 + D'[y]**2) */
	static fastf_t	b, c;		/* coeff of polynomial */
	static fastf_t	root;		/* root of radical */
	static fastf_t	k1, k2;		/* distance constants of solution */
	static vect_t	xlated;		/* translated vector */
	static point_t	point;		/* hit point */
	static struct hit hits[4];	/* 4 potential hit points */
	register struct hit *hitp;	/* pointer to hit point */

	hitp = &hits[0];
	/* ASSERT that MAGNITUDE(rp->r_dir) == 1 */

	/* out, Mat, vect */
	MAT3XVEC( dprime, rec->rec_SoR, rp->r_dir );
	VSUB2( xlated, rp->r_pt, rec->rec_V );
	MAT3XVEC( pprime, rec->rec_SoR, xlated );

	if( NEAR_ZERO(dprime[X]) && NEAR_ZERO(dprime[Y]) )
		goto check_plates;

	dx2dy2 = 1.0 / (dprime[X] * dprime[X] + dprime[Y] * dprime[Y]);
	b = 2 * ( dprime[X] * pprime[X] + dprime[Y] * pprime[Y] ) * dx2dy2;
	c = ( (pprime[X] * pprime[X] + pprime[Y] * pprime[Y]) - 1.0) * dx2dy2;

	root = b*b - ( 4 * c );			/* a == 1 */
	if( root < 0 )
		goto check_plates;
	root = sqrt(root);

	k1 = (-b + root) * 0.5;
	k2 = (-b - root) * 0.5;

	/*
	 * k1 and k2 are potential solutions to intersection with
	 * side, in no particular order.
	 *  Compute exact intersection points, and surface normal.
	 */
	VCOMPOSE1( point, pprime, k1, dprime );		/* hit' */
	if( point[Z] >= 0.0 && point[Z] <= 1.0 ) {
		hitp->hit_dist = k1;
		VCOMPOSE1( hitp->hit_point, rp->r_pt, k1, rp->r_dir );
		MAT3XVEC( hitp->hit_normal, rec->rec_invRoS, point );
		VUNITIZE( hitp->hit_normal );
		hitp++;
	}

	VCOMPOSE1( point, pprime, k2, dprime );		/* hit' */
	if( point[Z] >= 0.0 && point[Z] <= 1.0 )  {
		hitp->hit_dist = k2;
		VCOMPOSE1( hitp->hit_point, rp->r_pt, k2, rp->r_dir );
		MAT3XVEC( hitp->hit_normal, rec->rec_invRoS, point );
		VUNITIZE( hitp->hit_normal );
		hitp++;
	}

	/*
	 * Check for hitting the end plates.
	 */
check_plates:
	if( hitp < &hits[2]  &&  !NEAR_ZERO(dprime[Z]) )  {
		/* 0 or 1 hits so far, this is worthwhile */
		k1 = -pprime[Z] / dprime[Z];		/* bottom plate */
		k2 = (1.0 - pprime[Z]) / dprime[Z];	/* top plate */

		VCOMPOSE1( point, pprime, k1, dprime );		/* hit' */
		if( point[X] * point[X] + point[Y] * point[Y] <= 1.0 )  {
			hitp->hit_dist = k1;
			VCOMPOSE1( hitp->hit_point, rp->r_pt, k1, rp->r_dir );
			VREVERSE( hitp->hit_normal, rec->rec_Hunit ); /* -H */
			hitp++;
		}

		VCOMPOSE1( point, pprime, k2, dprime );		/* hit' */
		if( point[X] * point[X] + point[Y] * point[Y] <= 1.0 )  {
			hitp->hit_dist = k2;
			VCOMPOSE1( hitp->hit_point, rp->r_pt, k2, rp->r_dir );
			VMOVE( hitp->hit_normal, rec->rec_Hunit ); /* H */
			hitp++;
		}
	}
	if( hitp == &hits[0] )
		return(SEG_NULL);	/* MISS */

	if( hitp != &hits[2] )  {
		static struct hit *hp;	/* XXX */
		printf("ERROR: rec(%s):  %d hits??\n", stp->st_name,
			(&hits[0] - hitp) / sizeof(struct hit)   );
		for( hp = &hits[0]; hp < hitp; hp++ )
			hit_print( "?hit?", hp );
		return(SEG_NULL);	/* MISS */
	}
	GET_SEG(segp);
	segp->seg_stp = stp;
	segp->seg_flag = SEG_IN|SEG_OUT;
	if( hits[0].hit_dist < hits[1].hit_dist )  {
		/* entry is [0], exit is [1] */
		segp->seg_in = hits[0];		/* struct copy */
		segp->seg_out = hits[1];	/* struct copy */
	} else {
		/* entry is [1], exit is [0] */
		segp->seg_in = hits[1];		/* struct copy */
		segp->seg_out = hits[0];	/* struct copy */
	}
	return(segp);			/* HIT */
}
