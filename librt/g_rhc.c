/*
 *			G _ R H C . C
 *
 *  Purpose -
 *	Intersect a ray with a Right Hyperbolic Cylinder.
 *
 *  Algorithm -
 *  
 *  Given V, H, R, and B, there is a set of points on this rhc
 *  
 *  { (x,y,z) | (x,y,z) is on rhc }
 *  
 *  Through a series of Affine Transformations, this set of points will be
 *  transformed into a set of points on an rhc located at the origin
 *  with a rectangular halfwidth R of 1 along the Y axis, a height H of +1
 *  along the -X axis, a distance B of 1 along the -Z axis between the
 *  vertex V and the tip of the hyperbola, and a distance c between the
 *  tip of the hyperbola and the vertex of the asymptotic cone.
 *  
 *  
 *  { (x',y',z') | (x',y',z') is on rhc at origin }
 *
 *  The transformation from X to X' is accomplished by:
 *  
 *  X' = S(R( X - V ))
 *  
 *  where R(X) =  ( H/(-|H|) )
 *  		 (  R/( |R|)  ) . X
 *  		  ( B/(-|B|) )
 *  
 *  and S(X) =	 (  1/|H|   0     0   )
 *  		(    0    1/|R|   0    ) . X
 *  		 (   0      0   1/|B| )
 *  
 *  To find the intersection of a line with the surface of the rhc,
 *  consider the parametric line L:
 *  
 *  	L : { P(n) | P + t(n) . D }
 *  
 *  Call W the actual point of intersection between L and the rhc.
 *  Let W' be the point of intersection between L' and the unit rhc.
 *  
 *  	L' : { P'(n) | P' + t(n) . D' }
 *  
 *  W = invR( invS( W' ) ) + V
 *  
 *  Where W' = k D' + P'.
 *  
 *  If Dy' and Dz' are both 0, then there is no hit on the rhc;
 *  but the end plates need checking.  If there is now only 1 hit
 *  point, the top plate needs to be checked as well.
 *
 *  Line L' hits the infinitely long canonical rhc at W' when
 *
 *	A * k**2 + B * k + C = 0
 *
 *  where
 *
 *  A = Dz'**2 - Dy'**2 * (1 + 2*c')
 *  B = 2 * ((1 + c' + Pz') * Dz' - (1 + 2*c') * Dy' * Py'
 *  C = (Pz' + c' + 1)**2 - (1 + 2*c') * Py'**2 - c'**2
 *  b = |Breadth| = 1.0
 *  h = |Height| = 1.0
 *  r = 1.0
 *  c' = c / |Breadth|
 *  
 *  The quadratic formula yields k (which is constant):
 *
 *  k = [ -B +/- sqrt( B**2 - 4 * A * C )] / (2 * A)
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
 *  The hit at ``k'' is a hit on the canonical rhc IFF
 *  -1 <= Wx' <= 0 and -1 <= Wz' <= 0.
 *
 *  NORMALS.  Given the point W on the surface of the rhc,
 *  what is the vector normal to the tangent plane at that point?
 *  
 *  Map W onto the unit rhc, ie:  W' = S( R( W - V ) ).
 *  
 *  Plane on unit rhc at W' has a normal vector N' where
 *
 *  N' = <0, Wy'*(1 + 2*c), -z-c-1>.
 *  
 *  The plane transforms back to the tangent plane at W, and this
 *  new plane (on the original rhc) has a normal vector of N, viz:
 *  
 *  N = inverse[ transpose( inverse[ S o R ] ) ] ( N' )
 *
 *  because if H is perpendicular to plane Q, and matrix M maps from
 *  Q to Q', then inverse[ transpose(M) ] (H) is perpendicular to Q'.
 *  Here, H and Q are in "prime space" with the unit sphere.
 *  [Somehow, the notation here is backwards].
 *  So, the mapping matrix M = inverse( S o R ), because
 *  S o R maps from normal space to the unit sphere.
 *
 *  N = inverse[ transpose( inverse[ S o R ] ) ] ( N' )
 *    = inverse[ transpose(invR o invS) ] ( N' )
 *    = inverse[ transpose(invS) o transpose(invR) ] ( N' )
 *    = inverse[ inverse(S) o R ] ( N' )
 *    = invR o S ( N' )
 *
 *  because inverse(R) = transpose(R), so R = transpose( invR ),
 *  and S = transpose( S ).
 *
 *  Note that the normal vector produced above will not have unit length.
 *
 *  THE TOP AND END PLATES.
 *
 *  If Dz' == 0, line L' is parallel to the top plate, so there is no
 *  hit on the top plate.  Otherwise, rays intersect the top plate
 *  with k = (0 - Pz')/Dz'.  The solution is within the top plate
 *  IFF  -1 <= Wx' <= 0 and -1 <= Wy' <= 1.
 *
 *  If Dx' == 0, line L' is parallel to the end plates, so there is no
 *  hit on the end plates.  Otherwise, rays intersect the front plate
 *  line L' hits the front plate with k = (0 - Px') / Dx', and
 *  and hits the back plate with k = (-1 - Px') / Dx'.
 *
 *  The solution W' is within an end plate IFF
 *
 *	(Wz' + c + 1)**2 - (Wy'**2 * (2*c + 1) >= c**2  and  Wz' <= 1.0
 *
 *  The normal for a hit on the top plate is -Bunit.
 *  The normal for a hit on the front plate is -Hunit, and
 *  the normal for a hit on the back plate is +Hunit.
 *
 *  Authors -
 *	Michael J. Markowski
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSrhc[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"

struct rhc_specific {
	point_t	rhc_V;		/* vector to rhc origin */
	vect_t	rhc_Bunit;	/* unit B vector */
	vect_t	rhc_Hunit;	/* unit H vector */
	vect_t	rhc_Runit;	/* unit vector, B x H */
	mat_t	rhc_SoR;	/* Scale(Rot(vect)) */
	mat_t	rhc_invRoS;	/* invRot(Scale(vect)) */
	fastf_t	rhc_b;		/* |B| */
	fastf_t	rhc_c;		/* c */
	fastf_t	rhc_cprime;	/* c / |B| */
	fastf_t	rhc_rsq;	/* r * r */
};

struct pt_node {
	point_t		p;	/* a point */
	struct pt_node	*next;	/* ptr to next pt */
};

struct bu_structparse rt_rhc_parse[] = {
    { "%f", 3, "V", offsetof(struct rt_rhc_internal, rhc_V[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "H", offsetof(struct rt_rhc_internal, rhc_H[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "B", offsetof(struct rt_rhc_internal, rhc_B[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "r", offsetof(struct rt_rhc_internal, rhc_r),    BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "c", offsetof(struct rt_rhc_internal, rhc_c),    BU_STRUCTPARSE_FUNC_NULL },
    {0} };

/*
 *  			R T _ R H C _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid RHC, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	RHC is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct rhc_specific is created, and it's address is stored in
 *  	stp->st_specific for use by rhc_shot().
 */
int
rt_rhc_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_rhc_internal		*xip;
	register struct rhc_specific	*rhc;
	LOCAL fastf_t	magsq_b, magsq_h, magsq_r;
	LOCAL fastf_t	mag_b, mag_h, mag_r;
	LOCAL fastf_t	f;
	LOCAL mat_t	R;
	LOCAL mat_t	Rinv;
	LOCAL mat_t	S;
	LOCAL vect_t	invsq;	/* [ 1/(|H|**2), 1/(|R|**2), 1/(|B|**2) ] */

	RT_CK_DB_INTERNAL(ip);
	RT_CK_RTI(rtip);
	xip = (struct rt_rhc_internal *)ip->idb_ptr;
	RT_RHC_CK_MAGIC(xip);

	/* compute |B| |H| */
	mag_b = sqrt( magsq_b = MAGSQ( xip->rhc_B ) );
	mag_h = sqrt( magsq_h = MAGSQ( xip->rhc_H ) );
	mag_r = xip->rhc_r;
	magsq_r = mag_r * mag_r;

	/* Check for |H| > 0, |B| > 0, |R| > 0 */
	if( NEAR_ZERO(mag_h, RT_LEN_TOL) || NEAR_ZERO(mag_b, RT_LEN_TOL)
	 || NEAR_ZERO(mag_r, RT_LEN_TOL) )  {
		return(1);		/* BAD, too small */
	}

	/* Check for B.H == 0 */
	f = VDOT( xip->rhc_B, xip->rhc_H ) / (mag_b * mag_h);
	if( ! NEAR_ZERO(f, RT_DOT_TOL) )  {
		return(1);		/* BAD */
	}

	/*
	 *  RHC is ok
	 */
	stp->st_id = ID_RHC;		/* set soltab ID */

	GETSTRUCT( rhc, rhc_specific );
	stp->st_specific = (genptr_t)rhc;
	rhc->rhc_b = mag_b;
	rhc->rhc_rsq = magsq_r;
	rhc->rhc_c = xip->rhc_c;

	/* make unit vectors in B, H, and BxH directions */
	VMOVE(    rhc->rhc_Hunit, xip->rhc_H );
	VUNITIZE( rhc->rhc_Hunit );
	VMOVE(    rhc->rhc_Bunit, xip->rhc_B );
	VUNITIZE( rhc->rhc_Bunit );
	VCROSS(   rhc->rhc_Runit, rhc->rhc_Bunit, rhc->rhc_Hunit );

	VMOVE( rhc->rhc_V, xip->rhc_V );
	rhc->rhc_cprime = xip->rhc_c / mag_b;

	/* Compute R and Rinv matrices */
	mat_idn( R );
	VREVERSE( &R[0], rhc->rhc_Hunit );
	VMOVE(    &R[4], rhc->rhc_Runit );
	VREVERSE( &R[8], rhc->rhc_Bunit );
	mat_trn( Rinv, R );			/* inv of rot mat is trn */

	/* Compute S */
	VSET( invsq, 1.0/magsq_h, 1.0/magsq_r, 1.0/magsq_b );
	mat_idn( S );
	S[ 0] = sqrt( invsq[0] );
	S[ 5] = sqrt( invsq[1] );
	S[10] = sqrt( invsq[2] );

	/* Compute SoR and invRoS */
	mat_mul( rhc->rhc_SoR, S, R );
	mat_mul( rhc->rhc_invRoS, Rinv, S );

	/* Compute bounding sphere and RPP */
	/* bounding sphere center */
	VJOIN2( stp->st_center,	rhc->rhc_V,
		mag_h / 2.0,	rhc->rhc_Hunit,
		mag_b / 2.0,	rhc->rhc_Bunit );
	/* bounding radius */
	stp->st_bradius = 0.5 * sqrt(magsq_h + 4.0*magsq_r + magsq_b);
	/* approximate bounding radius */
	stp->st_aradius = stp->st_bradius;
	
	/* cheat, make bounding RPP by enclosing bounding sphere */
	stp->st_min[X] = stp->st_center[X] - stp->st_bradius;
	stp->st_max[X] = stp->st_center[X] + stp->st_bradius;
	stp->st_min[Y] = stp->st_center[Y] - stp->st_bradius;
	stp->st_max[Y] = stp->st_center[Y] + stp->st_bradius;
	stp->st_min[Z] = stp->st_center[Z] - stp->st_bradius;
	stp->st_max[Z] = stp->st_center[Z] + stp->st_bradius;

	return(0);			/* OK */
}

/*
 *			R T _ R H C _ P R I N T
 */
void
rt_rhc_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct rhc_specific *rhc =
		(struct rhc_specific *)stp->st_specific;

	VPRINT("V", rhc->rhc_V);
	VPRINT("Bunit", rhc->rhc_Bunit);
	VPRINT("Hunit", rhc->rhc_Hunit);
	VPRINT("Runit", rhc->rhc_Runit);
	mat_print("S o R", rhc->rhc_SoR );
	mat_print("invR o S", rhc->rhc_invRoS );
}

/* hit_surfno is set to one of these */
#define	RHC_NORM_BODY	(1)		/* compute normal */
#define	RHC_NORM_TOP	(2)		/* copy rhc_N */
#define	RHC_NORM_FRT	(3)		/* copy reverse rhc_N */
#define RHC_NORM_BACK	(4)

/*
 *  			R T _ R H C _ S H O T
 *  
 *  Intersect a ray with a rhc.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_rhc_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct rhc_specific *rhc =
		(struct rhc_specific *)stp->st_specific;
	LOCAL vect_t	dprime;		/* D' */
	LOCAL vect_t	pprime;		/* P' */
	LOCAL fastf_t	k1, k2;		/* distance constants of solution */
	LOCAL fastf_t	x;
	LOCAL vect_t	xlated;		/* translated vector */
	LOCAL struct hit hits[3];	/* 2 potential hit points */
	register struct hit *hitp;	/* pointer to hit point */

	hitp = &hits[0];

	/* out, Mat, vect */
	MAT4X3VEC( dprime, rhc->rhc_SoR, rp->r_dir );
	VSUB2( xlated, rp->r_pt, rhc->rhc_V );
	MAT4X3VEC( pprime, rhc->rhc_SoR, xlated );
	
	x = rhc->rhc_cprime;

	if ( NEAR_ZERO(dprime[Y], SMALL) && NEAR_ZERO(dprime[Z], SMALL) )
		goto check_plates;

	/* Find roots of the equation, using formula for quadratic */
	{
		FAST fastf_t	a, b, c;	/* coeffs of polynomial */
		FAST fastf_t	disc;		/* disc of radical */

		a = dprime[Z] * dprime[Z] - dprime[Y] * dprime[Y] * (1 + 2*x);
		b = 2*((pprime[Z] + x + 1) * dprime[Z]
			- (2*x + 1) * dprime[Y] * pprime[Y]);
		c = (pprime[Z]+x+1)*(pprime[Z]+x+1)
			- (2*x + 1) * pprime[Y] * pprime[Y] - x*x;
		if ( !NEAR_ZERO(a, RT_PCOEF_TOL) ) {
			disc = b*b - 4 * a * c;
			if (disc <= 0)
				goto check_plates;
			disc = sqrt(disc);

			k1 = (-b + disc) / (2.0 * a);
			k2 = (-b - disc) / (2.0 * a);

			/*
			 *  k1 and k2 are potential solutions to intersection with side.
			 *  See if they fall in range.
			 */
			VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );		/* hit' */
			if( hitp->hit_vpriv[X] >= -1.0
				&& hitp->hit_vpriv[X] <= 0.0
				&& hitp->hit_vpriv[Z] >= -1.0
				&& hitp->hit_vpriv[Z] <= 0.0 ) {
				hitp->hit_dist = k1;
				hitp->hit_surfno = RHC_NORM_BODY;	/* compute N */
				hitp++;
			}

			VJOIN1( hitp->hit_vpriv, pprime, k2, dprime );		/* hit' */
			if( hitp->hit_vpriv[X] >= -1.0
				&& hitp->hit_vpriv[X] <= 0.0
				&& hitp->hit_vpriv[Z] >= -1.0
				&& hitp->hit_vpriv[Z] <= 0.0 ) {
				hitp->hit_dist = k2;
				hitp->hit_surfno = RHC_NORM_BODY;	/* compute N */
				hitp++;
			}
		} else if ( !NEAR_ZERO(b, RT_PCOEF_TOL) ) {
			k1 = -c/b;
			VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );		/* hit' */
			if( hitp->hit_vpriv[X] >= -1.0
				&& hitp->hit_vpriv[X] <= 0.0
				&& hitp->hit_vpriv[Z] >= -1.0
				&& hitp->hit_vpriv[Z] <= 0.0 ) {
				hitp->hit_dist = k1;
				hitp->hit_surfno = RHC_NORM_BODY;	/* compute N */
				hitp++;
			}
		}
	}


	/*
	 * Check for hitting the top and end plates.
	 */
check_plates:
	/* check front and back plates */
	if( hitp < &hits[2]  &&  !NEAR_ZERO(dprime[X], SMALL) )  {
		/* 0 or 1 hits so far, this is worthwhile */
		k1 = -pprime[X] / dprime[X];		/* front plate */
		k2 = (-1.0 - pprime[X]) / dprime[X];	/* back plate */

		VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );	/* hit' */
		if( (hitp->hit_vpriv[Z] + x + 1.0)
			* (hitp->hit_vpriv[Z] + x + 1.0)
			- hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y]
			* (1.0 + 2*x) >= x*x
			&& hitp->hit_vpriv[Z] >= -1.0
			&& hitp->hit_vpriv[Z] <= 0.0)  {
			hitp->hit_dist = k1;
			hitp->hit_surfno = RHC_NORM_FRT;	/* -H */
			hitp++;
		}

		VJOIN1( hitp->hit_vpriv, pprime, k2, dprime );	/* hit' */
		if( (hitp->hit_vpriv[Z] + x + 1.0)
			* (hitp->hit_vpriv[Z] + x + 1.0)
			- hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y]
			* (1.0 + 2*x) >= x*x
			&& hitp->hit_vpriv[Z] >= -1.0
			&& hitp->hit_vpriv[Z] <= 0.0)  {
			hitp->hit_dist = k2;
			hitp->hit_surfno = RHC_NORM_BACK;	/* +H */
			hitp++;
		}
	}
	
	/* check top plate */
	if( hitp == &hits[1]  &&  !NEAR_ZERO(dprime[Z], SMALL) )  {
		/* 0 or 1 hits so far, this is worthwhile */
		k1 = -pprime[Z] / dprime[Z];		/* top plate */

		VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );	/* hit' */
		if( hitp->hit_vpriv[X] >= -1.0 &&  hitp->hit_vpriv[X] <= 0.0
			&& hitp->hit_vpriv[Y] >= -1.0
			&& hitp->hit_vpriv[Y] <= 1.0 ) {
			hitp->hit_dist = k1;
			hitp->hit_surfno = RHC_NORM_TOP;	/* -B */
			hitp++;
		}
	}
	
	if( hitp != &hits[2] )
		return(0);	/* MISS */

	if( hits[0].hit_dist < hits[1].hit_dist )  {
		/* entry is [0], exit is [1] */
		register struct seg *segp;

		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in = hits[0];		/* struct copy */
		segp->seg_out = hits[1];	/* struct copy */
		RT_LIST_INSERT( &(seghead->l), &(segp->l) );
	} else {
		/* entry is [1], exit is [0] */
		register struct seg *segp;

		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in = hits[1];		/* struct copy */
		segp->seg_out = hits[0];	/* struct copy */
		RT_LIST_INSERT( &(seghead->l), &(segp->l) );
	}
	return(2);			/* HIT */
}

#define RT_RHC_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ R H C _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_rhc_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ R H C _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_rhc_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	fastf_t	c;
	vect_t	can_normal;	/* normal to canonical rhc */
	register struct rhc_specific *rhc =
		(struct rhc_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	switch( hitp->hit_surfno )  {
	case RHC_NORM_BODY:
		c = rhc->rhc_cprime;
		VSET( can_normal,
			0.0,
			hitp->hit_vpriv[Y] * (1.0 + 2.0*c),
			-hitp->hit_vpriv[Z] - c - 1.0 );
		MAT4X3VEC( hitp->hit_normal, rhc->rhc_invRoS, can_normal );
		VUNITIZE( hitp->hit_normal );
		break;
	case RHC_NORM_TOP:
		VREVERSE( hitp->hit_normal, rhc->rhc_Bunit );
		break;
	case RHC_NORM_FRT:
		VREVERSE( hitp->hit_normal, rhc->rhc_Hunit );
		break;
	case RHC_NORM_BACK:
		VMOVE( hitp->hit_normal, rhc->rhc_Hunit );
		break;
	default:
		rt_log("rt_rhc_norm: surfno=%d bad\n", hitp->hit_surfno);
		break;
	}
}

/*
 *			R T _ R H C _ C U R V E
 *
 *  Return the curvature of the rhc.
 */
void
rt_rhc_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	fastf_t	b, c, rsq, y;
	fastf_t	zp1_sq, zp2;	/* 1st deriv sqr, 2nd deriv */
	register struct rhc_specific *rhc =
		(struct rhc_specific *)stp->st_specific;

	switch( hitp->hit_surfno )  {
	case RHC_NORM_BODY:
		/* most nearly flat direction */
		VMOVE( cvp->crv_pdir, rhc->rhc_Hunit );
		cvp->crv_c1 = 0;
		/* k = z'' / (1 + z'^2) ^ 3/2 */
		b = rhc->rhc_b;
		c = rhc->rhc_c;
		y = hitp->hit_point[Y];
		rsq = rhc->rhc_rsq;
		zp1_sq = y * (b*b + 2*b*c) / rsq;
		zp1_sq *= zp1_sq / (c*c + y*y*(b*b + 2*b*c)/rsq);
		zp2 = c*c / (rsq*c*c + y*y*(b*b + 2*b*c));
		cvp->crv_c2 = zp2 / pow( (1 + zp1_sq), 1.5);
		break;
	case RHC_NORM_BACK:
	case RHC_NORM_FRT:
	case RHC_NORM_TOP:
		/* any tangent direction */
	 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	 	cvp->crv_c1 = cvp->crv_c2 = 0;
		break;
	}
}

/*
 *  			R T _ R H C _ U V
 *  
 *  For a hit on the surface of an rhc, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_rhc_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct rhc_specific *rhc =
		(struct rhc_specific *)stp->st_specific;
	LOCAL vect_t work;
	LOCAL vect_t pprime;
	FAST fastf_t len;

	/*
	 * hit_point is on surface;  project back to unit rhc,
	 * creating a vector from vertex to hit point.
	 */
	VSUB2( work, hitp->hit_point, rhc->rhc_V );
	MAT4X3VEC( pprime, rhc->rhc_SoR, work );

	switch( hitp->hit_surfno )  {
	case RHC_NORM_BODY:
		/* Skin.  x,y coordinates define rotation.  radius = 1 */
		len = sqrt(pprime[Y]*pprime[Y] + pprime[Z]*pprime[Z]);
		uvp->uv_u = acos(pprime[Y]/len) * rt_invpi;
		uvp->uv_v = -pprime[X];		/* height */
		break;
	case RHC_NORM_FRT:
	case RHC_NORM_BACK:
		/* end plates - circular mapping, not seamless w/body, top */
		len = sqrt(pprime[Y]*pprime[Y] + pprime[Z]*pprime[Z]);
		uvp->uv_u = acos(pprime[Y]/len) * rt_invpi;
		uvp->uv_v = len;	/* rim v = 1 for both plates */
		break;
	case RHC_NORM_TOP:
		uvp->uv_u = 1.0 - (pprime[Y] + 1.0)/2.0;
		uvp->uv_v = -pprime[X];		/* height */
		break;
	}

	/* uv_du should be relative to rotation, uv_dv relative to height */
	uvp->uv_du = uvp->uv_dv = 0;
}

/*
 *		R T _ R H C _ F R E E
 */
void
rt_rhc_free( stp )
register struct soltab *stp;
{
	register struct rhc_specific *rhc =
		(struct rhc_specific *)stp->st_specific;

	rt_free( (char *)rhc, "rhc_specific" );
}

/*
 *			R T _ R H C _ C L A S S
 */
int
rt_rhc_class()
{
	return(0);
}


/*
 *			R T _ R H C _ P L O T
 */
int
rt_rhc_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	int		i, n;
	fastf_t		b, c, *back, f, *front, h, rh;
	fastf_t		dtol, ntol;
	vect_t		Bu, Hu, Ru;
	LOCAL mat_t	R;
	LOCAL mat_t	invR;
	LOCAL struct rt_rhc_internal	*xip;
	struct pt_node	*old, *pos, *pts, *rt_ptalloc();

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_rhc_internal *)ip->idb_ptr;
	RT_RHC_CK_MAGIC(xip);

	/* compute |B| |H| */
	b = MAGNITUDE( xip->rhc_B );	/* breadth */
	rh = xip->rhc_r;		/* rectangular halfwidth */
	h = MAGNITUDE( xip->rhc_H );	/* height */
	c = xip->rhc_c;			/* dist to asympt origin */

	/* Check for |H| > 0, |B| > 0, rh > 0, c > 0 */
	if( NEAR_ZERO(h, RT_LEN_TOL) || NEAR_ZERO(b, RT_LEN_TOL)
	 || NEAR_ZERO(rh, RT_LEN_TOL) || NEAR_ZERO(c, RT_LEN_TOL))  {
		rt_log("rt_rhc_plot:  zero length H, B, c, or rh\n");
		return(-2);		/* BAD */
	}

	/* Check for B.H == 0 */
	f = VDOT( xip->rhc_B, xip->rhc_H ) / (b * h);
	if( ! NEAR_ZERO(f, RT_DOT_TOL) )  {
		rt_log("rt_rhc_plot: B not perpendicular to H, f=%f\n", f);
		return(-3);		/* BAD */
	}

	/* make unit vectors in B, H, and BxH directions */
	VMOVE(    Hu, xip->rhc_H );
	VUNITIZE( Hu );
	VMOVE(    Bu, xip->rhc_B );
	VUNITIZE( Bu );
	VCROSS(   Ru, Bu, Hu );

	/* Compute R and Rinv matrices */
	mat_idn( R );
	VREVERSE( &R[0], Hu );
	VMOVE(    &R[4], Ru );
	VREVERSE( &R[8], Bu );
	mat_trn( invR, R );			/* inv of rot mat is trn */

	/*
	 *  Establish tolerances
	 */
	if( ttol->rel <= 0.0 || ttol->rel >= 1.0 )  {
		dtol = 0.0;		/* none */
	} else {
		/* Convert rel to absolute by scaling by smallest side */
		if (rh < b)
			dtol = ttol->rel * 2 * rh;
		else
			dtol = ttol->rel * 2 * b;
	}
	if( ttol->abs <= 0.0 )  {
		if( dtol <= 0.0 )  {
			/* No tolerance given, use a default */
			if (rh < b)
				dtol = 2 * 0.10 * rh;	/* 10% */
			else
				dtol = 2 * 0.10 * b;	/* 10% */
		} else {
			/* Use absolute-ized relative tolerance */
		}
	} else {
		/* Absolute tolerance was given, pick smaller */
		if( ttol->rel <= 0.0 || dtol > ttol->abs )
			dtol = ttol->abs;
	}

	/* To ensure normal tolerance, remain below this angle */
	if( ttol->norm > 0.0 )
		ntol = ttol->norm;
	else
		/* tolerate everything */
		ntol = rt_pi;

	/* initial hyperbola approximation is a single segment */
	pts = rt_ptalloc();
	pts->next = rt_ptalloc();
	pts->next->next = NULL;
	VSET( pts->p,       0, -rh, 0);
	VSET( pts->next->p, 0,  rh, 0);
	/* 2 endpoints in 1st approximation */
	n = 2;
	/* recursively break segment 'til within error tolerances */
	n += rt_mk_hyperbola( pts, rh, b, c, dtol, ntol );

	/* get mem for arrays */
	front = (fastf_t *)rt_malloc(3*n * sizeof(fastf_t), "fast_t");
	back  = (fastf_t *)rt_malloc(3*n * sizeof(fastf_t), "fast_t");
	
	/* generate front & back plates in world coordinates */
	pos = pts;
	i = 0;
	while (pos) {
		/* rotate back to original position */
		MAT4X3VEC( &front[i], invR, pos->p );
		/* move to origin vertex origin */
		VADD2( &front[i], &front[i], xip->rhc_V );
		/* extrude front to create back plate */
		VADD2( &back[i], &front[i], xip->rhc_H );
		i += 3;
		old = pos;
		pos = pos->next;
		rt_free ( (char *)old, "pt_node" );
	}

	/* Draw the front */
	RT_ADD_VLIST( vhead, &front[(n-1)*ELEMENTS_PER_VECT],
		RT_VLIST_LINE_MOVE );
	for( i = 0; i < n; i++ )  {
		RT_ADD_VLIST( vhead, &front[i*ELEMENTS_PER_VECT], RT_VLIST_LINE_DRAW );
	}

	/* Draw the back */
	RT_ADD_VLIST( vhead, &back[(n-1)*ELEMENTS_PER_VECT], RT_VLIST_LINE_MOVE );
	for( i = 0; i < n; i++ )  {
		RT_ADD_VLIST( vhead, &back[i*ELEMENTS_PER_VECT], RT_VLIST_LINE_DRAW );
	}

	/* Draw connections */
	for( i = 0; i < n; i++ )  {
		RT_ADD_VLIST( vhead, &front[i*ELEMENTS_PER_VECT], RT_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vhead, &back[i*ELEMENTS_PER_VECT], RT_VLIST_LINE_DRAW );
	}

	/* free mem */
	rt_free( (char *)front, "fastf_t");
	rt_free( (char *)back, "fastf_t");

	return(0);
}

/*
 *	R T _ M K _ H Y P E R B O L A
 *
 *
 */
int
rt_mk_hyperbola( pts, r, b, c, dtol, ntol )
fastf_t	r, b, c, dtol, ntol;
struct pt_node *pts;
{
	fastf_t	A, B, C, discr, dist, intr, j, k, m, theta0, theta1, z0;
	int	n;
	point_t	mpt, p0, p1;
	vect_t	norm_line, norm_hyperb;
	struct pt_node *new, *rt_ptalloc();
	
#define MIKE_TOL .0001
	/* endpoints of segment approximating hyperbola */
	VMOVE( p0, pts->p );
	VMOVE( p1, pts->next->p );
	/* slope and intercept of segment */
	m = ( p1[Z] - p0[Z] ) / ( p1[Y] - p0[Y] );
	intr = p0[Z] - m * p0[Y];
	/* find point on hyperbola with max dist between hyperbola and line */
	j = b + c;
	k = 1 - m*m*r*r/(b*(b + 2*c));
	A = k;
	B = 2*j*k;
	C = j*j*k - c*c;
	discr = sqrt(B*B - 4*A*C);
	z0 = (-B + discr) / (2. * A);
	if ( z0+MIKE_TOL >= -b )	/* use top sheet of hyperboloid */
		mpt[Z] = z0;
	else
		mpt[Z] = (-B - discr) / (2. * A);
	if (NEAR_ZERO( mpt[Z], MIKE_TOL))
		mpt[Z] = 0.;
	mpt[X] = 0;
	mpt[Y] = ((mpt[Z] + b + c) * (mpt[Z] + b + c) - c*c) / (b*(b + 2*c));
	if (NEAR_ZERO( mpt[Y], MIKE_TOL ))
		mpt[Y] = 0.;
	mpt[Y] = r * sqrt( mpt[Y] );
	if (p0[Y] < 0.)
		mpt[Y] = -mpt[Y];
	/* max distance between that point and line */
	dist = fabs( m * mpt[Y] - mpt[Z] + intr ) / sqrt( m * m + 1 );
	/* angles between normal of line and of hyperbola at line endpoints */
	VSET( norm_line, m, -1., 0.);
	VSET( norm_hyperb, 0., (2*c + 1) / (p0[Z] + c + 1), -1.);
	VUNITIZE( norm_line );
	VUNITIZE( norm_hyperb );
	theta0 = fabs( acos( VDOT( norm_line, norm_hyperb )));
	VSET( norm_hyperb, 0., (2*c + 1) / (p1[Z] + c + 1), -1.);
	VUNITIZE( norm_hyperb );
	theta1 = fabs( acos( VDOT( norm_line, norm_hyperb )));
	/* split segment at widest point if not within error tolerances */
	if ( dist > dtol || theta0 > ntol || theta1 > ntol ) {
		/* split segment */
		new = rt_ptalloc();
		VMOVE( new->p, mpt );
		new->next = pts->next;
		pts->next = new;
		/* keep track of number of pts added */
		n = 1;
		/* recurse on first new segment */
		n += rt_mk_hyperbola( pts, r, b, c, dtol, ntol );
		/* recurse on second new segment */
		n += rt_mk_hyperbola( new, r, b, c, dtol, ntol );
	} else
		n  = 0;
	return( n );
}

/*
 *			R T _ R H C _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_rhc_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	int		i, j, n;
	fastf_t		b, c, *back, f, *front, h, rh;
	fastf_t		dtol, ntol;
	vect_t		Bu, Hu, Ru;
	LOCAL mat_t	R;
	LOCAL mat_t	invR;
	LOCAL struct rt_rhc_internal	*xip;
	struct pt_node	*old, *pos, *pts, *rt_ptalloc();
	struct shell	*s;
	struct faceuse	**outfaceuses;
	struct vertex	**vfront, **vback, **vtemp, *vertlist[4];
	vect_t		*norms;
	fastf_t		bb_plus_2bc,b_plus_c,r_sq;
	int		failure=0;

	NMG_CK_MODEL( m );
	RT_CK_TOL( tol );
	RT_CK_TESS_TOL( ttol );

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_rhc_internal *)ip->idb_ptr;
	RT_RHC_CK_MAGIC(xip);

	/* compute |B| |H| */
	b = MAGNITUDE( xip->rhc_B );	/* breadth */
	rh = xip->rhc_r;		/* rectangular halfwidth */
	h = MAGNITUDE( xip->rhc_H );	/* height */
	c = xip->rhc_c;			/* dist to asympt origin */

	/* Check for |H| > 0, |B| > 0, rh > 0, c > 0 */
	if( NEAR_ZERO(h, RT_LEN_TOL) || NEAR_ZERO(b, RT_LEN_TOL)
	 || NEAR_ZERO(rh, RT_LEN_TOL) || NEAR_ZERO(c, RT_LEN_TOL))  {
		rt_log("rt_rhc_tess:  zero length H, B, c, or rh\n");
		return(-2);		/* BAD */
	}

	/* Check for B.H == 0 */
	f = VDOT( xip->rhc_B, xip->rhc_H ) / (b * h);
	if( ! NEAR_ZERO(f, RT_DOT_TOL) )  {
		rt_log("rt_rhc_tess: B not perpendicular to H, f=%f\n", f);
		return(-3);		/* BAD */
	}

	/* make unit vectors in B, H, and BxH directions */
	VMOVE(    Hu, xip->rhc_H );
	VUNITIZE( Hu );
	VMOVE(    Bu, xip->rhc_B );
	VUNITIZE( Bu );
	VCROSS(   Ru, Bu, Hu );

	/* Compute R and Rinv matrices */
	mat_idn( R );
	VREVERSE( &R[0], Hu );
	VMOVE(    &R[4], Ru );
	VREVERSE( &R[8], Bu );
	mat_trn( invR, R );			/* inv of rot mat is trn */

	/*
	 *  Establish tolerances
	 */
	if( ttol->rel <= 0.0 || ttol->rel >= 1.0 )  {
		dtol = 0.0;		/* none */
	} else {
		/* Convert rel to absolute by scaling by smallest side */
		if (rh < b)
			dtol = ttol->rel * 2 * rh;
		else
			dtol = ttol->rel * 2 * b;
	}
	if( ttol->abs <= 0.0 )  {
		if( dtol <= 0.0 )  {
			/* No tolerance given, use a default */
			if (rh < b)
				dtol = 2 * 0.10 * rh;	/* 10% */
			else
				dtol = 2 * 0.10 * b;	/* 10% */
		} else {
			/* Use absolute-ized relative tolerance */
		}
	} else {
		/* Absolute tolerance was given, pick smaller */
		if( ttol->rel <= 0.0 || dtol > ttol->abs )
			dtol = ttol->abs;
	}

	/* To ensure normal tolerance, remain below this angle */
	if( ttol->norm > 0.0 )
		ntol = ttol->norm;
	else
		/* tolerate everything */
		ntol = rt_pi;

	/* initial hyperbola approximation is a single segment */
	pts = rt_ptalloc();
	pts->next = rt_ptalloc();
	pts->next->next = NULL;
	VSET( pts->p,       0, -rh, 0);
	VSET( pts->next->p, 0,  rh, 0);
	/* 2 endpoints in 1st approximation */
	n = 2;
	/* recursively break segment 'til within error tolerances */
	n += rt_mk_hyperbola( pts, rh, b, c, dtol, ntol );

	/* get mem for arrays */
	front = (fastf_t *)rt_malloc(3*n * sizeof(fastf_t), "fastf_t");
	back  = (fastf_t *)rt_malloc(3*n * sizeof(fastf_t), "fastf_t");
	norms = (vect_t *)rt_calloc( n , sizeof( vect_t ) , "rt_rhc_tess: norms" );
	vfront = (struct vertex **)rt_malloc((n+1) * sizeof(struct vertex *), "vertex *");
	vback = (struct vertex **)rt_malloc((n+1) * sizeof(struct vertex *), "vertex *");
	vtemp = (struct vertex **)rt_malloc((n+1) * sizeof(struct vertex *), "vertex *");
	outfaceuses =
		(struct faceuse **)rt_malloc((n+2) * sizeof(struct faceuse *), "faceuse *");
	if (!front || !back || !vfront || !vback || !vtemp || !outfaceuses) {
		fprintf(stderr, "rt_rhc_tess: no memory!\n");
		goto fail;
	}
	
	/* generate front & back plates in world coordinates */
	bb_plus_2bc = b*b + 2.0*b*c;
	b_plus_c = b + c;
	r_sq = rh*rh;
	pos = pts;
	i = 0;
	j = 0;
	while (pos) {
		vect_t tmp_norm;

		/* calculate normal for 2D hyperbola */
		VSET( tmp_norm , 0.0 , pos->p[Y]*bb_plus_2bc , (-r_sq*(pos->p[Z]+b_plus_c)) );
		MAT4X3VEC( norms[j] , invR , tmp_norm );
		VUNITIZE( norms[j] );
		/* rotate back to original position */
		MAT4X3VEC( &front[i], invR, pos->p );
		/* move to origin vertex origin */
		VADD2( &front[i], &front[i], xip->rhc_V );
		/* extrude front to create back plate */
		VADD2( &back[i], &front[i], xip->rhc_H );
		i += 3;
		j++;
		old = pos;
		pos = pos->next;
		rt_free ( (char *)old, "pt_node" );
	}

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = RT_LIST_FIRST(shell, &(*r)->s_hd);

	for( i=0; i<n; i++ )  {
		vfront[i] = vtemp[i] = (struct vertex *)0;
	}

	/* Front face topology.  Verts are considered to go CCW */
	outfaceuses[0] = nmg_cface(s, vfront, n);

	(void)nmg_mark_edges_real( &outfaceuses[0]->l );

	/* Back face topology.  Verts must go in opposite dir (CW) */
	outfaceuses[1] = nmg_cface(s, vtemp, n);
	for( i=0; i<n; i++ )  vback[i] = vtemp[n-1-i];

	(void)nmg_mark_edges_real( &outfaceuses[1]->l );

	/* Duplicate [0] as [n] to handle loop end condition, below */
	vfront[n] = vfront[0];
	vback[n] = vback[0];

	/* Build topology for all the rectangular side faces (n of them)
	 * connecting the front and back faces.
	 * increasing indices go towards counter-clockwise (CCW).
	 */
	for( i=0; i<n; i++ )  {
		vertlist[0] = vfront[i];	/* from top, */
		vertlist[1] = vback[i];		/* straight down, */
		vertlist[2] = vback[i+1];	/* to left, */
		vertlist[3] = vfront[i+1];	/* straight up. */
		outfaceuses[2+i] = nmg_cface(s, vertlist, 4);
	}

	(void)nmg_mark_edges_real( &outfaceuses[n+1]->l );

	for( i=0; i<n; i++ )  {
		NMG_CK_VERTEX(vfront[i]);
		NMG_CK_VERTEX(vback[i]);
	}

	/* Associate the vertex geometry, CCW */
	for( i=0; i<n; i++ )  {
		nmg_vertex_gv( vfront[i], &front[3*(i)] );
	}
	for( i=0; i<n; i++ )  {
		nmg_vertex_gv( vback[i], &back[3*(i)] );
	}

	/* Associate the face geometry */
	for (i=0 ; i < n+2 ; i++) {
		if( nmg_fu_planeeqn( outfaceuses[i], tol ) < 0 )
		{
			failure = (-1);
			goto fail;
		}
	}

	/* Associate vertexuse normals */
	for( i=0 ; i<n ; i++ )
	{
		struct vertexuse *vu;
		struct faceuse *fu;
		vect_t rev_norm;

		VREVERSE( rev_norm , norms[i] );

		/* do "front" vertices */
		NMG_CK_VERTEX( vfront[i] );
		for( RT_LIST_FOR( vu , vertexuse , &vfront[i]->vu_hd ) )
		{
			NMG_CK_VERTEXUSE( vu );
			fu = nmg_find_fu_of_vu( vu );
			NMG_CK_FACEUSE( fu );
			if( fu->f_p == outfaceuses[0]->f_p ||
			    fu->f_p == outfaceuses[1]->f_p ||
			    fu->f_p == outfaceuses[n+1]->f_p )
					continue;	/* skip flat faces */

			if( fu->orientation == OT_SAME )
				nmg_vertexuse_nv( vu , norms[i] );
			else if( fu->orientation == OT_OPPOSITE )
				nmg_vertexuse_nv( vu , rev_norm );
		}

		/* and "back" vertices */
		NMG_CK_VERTEX( vback[i] );
		for( RT_LIST_FOR( vu , vertexuse , &vback[i]->vu_hd ) )
		{
			NMG_CK_VERTEXUSE( vu );
			fu = nmg_find_fu_of_vu( vu );
			NMG_CK_FACEUSE( fu );
			if( fu->f_p == outfaceuses[0]->f_p ||
			    fu->f_p == outfaceuses[1]->f_p ||
			    fu->f_p == outfaceuses[n+1]->f_p )
					continue;	/* skip flat faces */

			if( fu->orientation == OT_SAME )
				nmg_vertexuse_nv( vu , norms[i] );
			else if( fu->orientation == OT_OPPOSITE )
				nmg_vertexuse_nv( vu , rev_norm );
		}
	}

	/* Glue the edges of different outward pointing face uses together */
	nmg_gluefaces( outfaceuses, n+2, tol );

	/* Compute "geometry" for region and shell */
	nmg_region_a( *r, tol );

fail:
	/* free mem */
	rt_free( (char *)front, "fastf_t");
	rt_free( (char *)back, "fastf_t");
	rt_free( (char*)vfront, "vertex *");
	rt_free( (char*)vback, "vertex *");
	rt_free( (char*)vtemp, "vertex *");
	rt_free( (char *)norms , "rt_rhc_tess: norms" );
	rt_free( (char*)outfaceuses, "faceuse *");

	return( failure );
}

/*
 *			R T _ R H C _ I M P O R T
 *
 *  Import an RHC from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_rhc_import( ip, ep, mat )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
register CONST mat_t		mat;
{
	LOCAL struct rt_rhc_internal	*xip;
	union record			*rp;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != ID_SOLID )  {
		rt_log("rt_rhc_import: defective record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_RHC;
	ip->idb_ptr = rt_malloc( sizeof(struct rt_rhc_internal), "rt_rhc_internal");
	xip = (struct rt_rhc_internal *)ip->idb_ptr;
	xip->rhc_magic = RT_RHC_INTERNAL_MAGIC;

	/* Warning:  type conversion */
	MAT4X3PNT( xip->rhc_V, mat, &rp->s.s_values[0*3] );
	MAT4X3VEC( xip->rhc_H, mat, &rp->s.s_values[1*3] );
	MAT4X3VEC( xip->rhc_B, mat, &rp->s.s_values[2*3] );
	xip->rhc_r = rp->s.s_values[3*3] / mat[15];
	xip->rhc_c = rp->s.s_values[3*3+1] / mat[15];

	if( xip->rhc_r < SMALL_FASTF || xip->rhc_c < SMALL_FASTF )
	{
		rt_log( "rt_rhc_import: r or c are zero\n" );
		rt_free( (char *)ip->idb_ptr , "rt_rhc_import: ip->idb_ptr" );
		return( -1 );
	}

	return(0);			/* OK */
}

/*
 *			R T _ R H C _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_rhc_export( ep, ip, local2mm )
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
{
	struct rt_rhc_internal	*xip;
	union record		*rhc;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_RHC )  return(-1);
	xip = (struct rt_rhc_internal *)ip->idb_ptr;
	RT_RHC_CK_MAGIC(xip);

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "rhc external");
	rhc = (union record *)ep->ext_buf;

	rhc->s.s_id = ID_SOLID;
	rhc->s.s_type = RHC;

	if (MAGNITUDE(xip->rhc_B) < RT_LEN_TOL
		|| MAGNITUDE(xip->rhc_H) < RT_LEN_TOL
		|| xip->rhc_r < RT_LEN_TOL
		|| xip->rhc_c < RT_LEN_TOL) {
		rt_log("rt_rhc_export: not all dimensions positive!\n");
		return(-1);
	}
	
	if ( !NEAR_ZERO( VDOT(xip->rhc_B, xip->rhc_H), RT_DOT_TOL) ) {
		rt_log("rt_rhc_export: B and H are not perpendicular!\n");
		return(-1);
	}

	/* Warning:  type conversion */
	VSCALE( &rhc->s.s_values[0*3], xip->rhc_V, local2mm );
	VSCALE( &rhc->s.s_values[1*3], xip->rhc_H, local2mm );
	VSCALE( &rhc->s.s_values[2*3], xip->rhc_B, local2mm );
	rhc->s.s_values[3*3] = xip->rhc_r * local2mm;
	rhc->s.s_values[3*3+1] = xip->rhc_c * local2mm;

	return(0);
}

/*
 *			R T _ R H C _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_rhc_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_rhc_internal	*xip =
		(struct rt_rhc_internal *)ip->idb_ptr;
	char	buf[256];

	RT_RHC_CK_MAGIC(xip);
	rt_vls_strcat( str, "Right Hyperbolic Cylinder (RHC)\n");

	sprintf(buf, "\tV (%g, %g, %g)\n",
		xip->rhc_V[X] * mm2local,
		xip->rhc_V[Y] * mm2local,
		xip->rhc_V[Z] * mm2local );
	rt_vls_strcat( str, buf );

	sprintf(buf, "\tB (%g, %g, %g) mag=%g\n",
		xip->rhc_B[X] * mm2local,
		xip->rhc_B[Y] * mm2local,
		xip->rhc_B[Z] * mm2local,
		MAGNITUDE(xip->rhc_B) * mm2local);
	rt_vls_strcat( str, buf );

	sprintf(buf, "\tH (%g, %g, %g) mag=%g\n",
		xip->rhc_H[X] * mm2local,
		xip->rhc_H[Y] * mm2local,
		xip->rhc_H[Z] * mm2local,
		MAGNITUDE(xip->rhc_H) * mm2local);
	rt_vls_strcat( str, buf );
	
	sprintf(buf, "\tr=%g\n", xip->rhc_r * mm2local);
	rt_vls_strcat( str, buf );
	
	sprintf(buf, "\tc=%g\n", xip->rhc_c * mm2local);
	rt_vls_strcat( str, buf );

	return(0);
}

/*
 *			R T _ R H C _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_rhc_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_rhc_internal	*xip;

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_rhc_internal *)ip->idb_ptr;
	RT_RHC_CK_MAGIC(xip);
	xip->rhc_magic = 0;		/* sanity */

	rt_free( (char *)xip, "rhc ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
