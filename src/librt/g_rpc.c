/*                         G _ R P C . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
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

/** \addtogroup cc */

/*@{*/
/** @file g_rpc.c
 *	Intersect a ray with a Right Parabolic Cylinder.
 *
 *  Algorithm -
 *
 *  Given V, H, R, and B, there is a set of points on this rpc
 *
 *  { (x,y,z) | (x,y,z) is on rpc }
 *
 *  Through a series of Affine Transformations, this set of points will be
 *  transformed into a set of points on an rpc located at the origin
 *  with a rectangular halfwidth R of 1 along the Y axis, a height H of +1
 *  along the -X axis, a distance B of 1 along the -Z axis between the
 *  vertex V and the tip of the parabola.
 *
 *
 *  { (x',y',z') | (x',y',z') is on rpc at origin }
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
 *  To find the intersection of a line with the surface of the rpc,
 *  consider the parametric line L:
 *
 *  	L : { P(n) | P + t(n) . D }
 *
 *  Call W the actual point of intersection between L and the rpc.
 *  Let W' be the point of intersection between L' and the unit rpc.
 *
 *  	L' : { P'(n) | P' + t(n) . D' }
 *
 *  W = invR( invS( W' ) ) + V
 *
 *  Where W' = k D' + P'.
 *
 *  If Dy' and Dz' are both 0, then there is no hit on the rpc;
 *  but the end plates need checking.  If there is now only 1 hit
 *  point, the top plate needs to be checked as well.
 *
 *  Line L' hits the infinitely long canonical rpc at W' when
 *
 *	A * k**2 + B * k + C = 0
 *
 *  where
 *
 *  A = Dy'**2
 *  B = (2 * Dy' * Py') - Dz'
 *  C = Py'**2 - Pz' - 1
 *  b = |Breadth| = 1.0
 *  h = |Height| = 1.0
 *  r = 1.0
 *
 *  The quadratic formula yields k (which is constant):
 *
 *  k = [ -B +/- sqrt( B**2 - 4*A*C )] / (2*A)
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
 *  The hit at ``k'' is a hit on the canonical rpc IFF
 *  -1 <= Wx' <= 0 and -1 <= Wz' <= 0.
 *
 *  NORMALS.  Given the point W on the surface of the rpc,
 *  what is the vector normal to the tangent plane at that point?
 *
 *  Map W onto the unit rpc, ie:  W' = S( R( W - V ) ).
 *
 *  Plane on unit rpc at W' has a normal vector N' where
 *
 *  N' = <0, Wy', -.5>.
 *
 *  The plane transforms back to the tangent plane at W, and this
 *  new plane (on the original rpc) has a normal vector of N, viz:
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
 *  with k = (0 - Px') / Dx' and the back plate with k = (-1 - Px') / Dx'.
 *
 *  The solution W' is within an end plate IFF
 *
 *	Wy'**2 + Wz' <= 1.0  and  Wz' <= 1.0
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
 */
/*@}*/

#ifndef lint
static const char RCSrpc[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"

struct rpc_specific {
	point_t	rpc_V;		/* vector to rpc origin */
	vect_t	rpc_Bunit;	/* unit B vector */
	vect_t	rpc_Hunit;	/* unit H vector */
	vect_t	rpc_Runit;	/* unit vector, B x H */
	fastf_t	rpc_b;		/* |B| */
	fastf_t	rpc_inv_rsq;	/* 1/(r * r) */
	mat_t	rpc_SoR;	/* Scale(Rot(vect)) */
	mat_t	rpc_invRoS;	/* invRot(Scale(vect)) */
};

const struct bu_structparse rt_rpc_parse[] = {
    { "%f", 3, "V", offsetof(struct rt_rpc_internal, rpc_V[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "H", offsetof(struct rt_rpc_internal, rpc_H[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "B", offsetof(struct rt_rpc_internal, rpc_B[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "r", offsetof(struct rt_rpc_internal, rpc_r),    BU_STRUCTPARSE_FUNC_NULL },
    { {'\0','\0','\0','\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL }
 };

/**
 *  			R T _ R P C _ P R E P
 *
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid RPC, and if so, precompute various
 *  terms of the formula.
 *
 *  Returns -
 *  	0	RPC is OK
 *  	!0	Error in description
 *
 *  Implicit return -
 *  	A struct rpc_specific is created, and it's address is stored in
 *  	stp->st_specific for use by rpc_shot().
 */
int
rt_rpc_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	struct rt_rpc_internal		*xip;
	register struct rpc_specific	*rpc;
#ifndef NO_BOMBING_MACROS
	const struct bn_tol		*tol = &rtip->rti_tol;  /* only used to check a if tolerance is valid */
#endif
	LOCAL fastf_t	magsq_b, magsq_h, magsq_r;
	LOCAL fastf_t	mag_b, mag_h, mag_r;
	LOCAL fastf_t	f;
	LOCAL mat_t	R;
	LOCAL mat_t	Rinv;
	LOCAL mat_t	S;
	LOCAL vect_t	invsq;	/* [ 1/(|H|**2), 1/(|R|**2), 1/(|B|**2) ] */

	RT_CK_DB_INTERNAL(ip);
	BN_CK_TOL(tol);
	xip = (struct rt_rpc_internal *)ip->idb_ptr;
	RT_RPC_CK_MAGIC(xip);

	/* compute |B| |H| */
	mag_b = sqrt( magsq_b = MAGSQ( xip->rpc_B ) );
	mag_h = sqrt( magsq_h = MAGSQ( xip->rpc_H ) );
	mag_r = xip->rpc_r;
	magsq_r = mag_r * mag_r;

	/* Check for |H| > 0, |B| > 0, |R| > 0 */
	if( NEAR_ZERO(mag_h, RT_LEN_TOL) || NEAR_ZERO(mag_b, RT_LEN_TOL)
	 || NEAR_ZERO(mag_r, RT_LEN_TOL) )  {
		return(1);		/* BAD, too small */
	}

	/* Check for B.H == 0 */
	f = VDOT( xip->rpc_B, xip->rpc_H ) / (mag_b * mag_h);
	if( ! NEAR_ZERO(f, RT_DOT_TOL) )  {
		return(1);		/* BAD */
	}

	/*
	 *  RPC is ok
	 */
	stp->st_id = ID_RPC;		/* set soltab ID */
	stp->st_meth = &rt_functab[ID_RPC];

	BU_GETSTRUCT( rpc, rpc_specific );
	stp->st_specific = (genptr_t)rpc;
	rpc->rpc_b = mag_b;
	rpc->rpc_inv_rsq = 1 / magsq_r;

	/* make unit vectors in B, H, and BxH directions */
	VMOVE(    rpc->rpc_Hunit, xip->rpc_H );
	VUNITIZE( rpc->rpc_Hunit );
	VMOVE(    rpc->rpc_Bunit, xip->rpc_B );
	VUNITIZE( rpc->rpc_Bunit );
	VCROSS(   rpc->rpc_Runit, rpc->rpc_Bunit, rpc->rpc_Hunit );

	VMOVE( rpc->rpc_V, xip->rpc_V );

	/* Compute R and Rinv matrices */
	MAT_IDN( R );
	VREVERSE( &R[0], rpc->rpc_Hunit );
	VMOVE(    &R[4], rpc->rpc_Runit );
	VREVERSE( &R[8], rpc->rpc_Bunit );
	bn_mat_trn( Rinv, R );			/* inv of rot mat is trn */

	/* Compute S */
	VSET( invsq, 1.0/magsq_h, 1.0/magsq_r, 1.0/magsq_b );
	MAT_IDN( S );
	S[ 0] = sqrt( invsq[0] );
	S[ 5] = sqrt( invsq[1] );
	S[10] = sqrt( invsq[2] );

	/* Compute SoR and invRoS */
	bn_mat_mul( rpc->rpc_SoR, S, R );
	bn_mat_mul( rpc->rpc_invRoS, Rinv, S );

	/* Compute bounding sphere and RPP */
	/* bounding sphere center */
	VJOIN2( stp->st_center,	rpc->rpc_V,
		mag_h / 2.0,	rpc->rpc_Hunit,
		mag_b / 2.0,	rpc->rpc_Bunit );
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

/**
 *			R T _ R P C _ P R I N T
 */
void
rt_rpc_print(register const struct soltab *stp)
{
	register const struct rpc_specific *rpc =
		(struct rpc_specific *)stp->st_specific;

	VPRINT("V", rpc->rpc_V);
	VPRINT("Bunit", rpc->rpc_Bunit);
	VPRINT("Hunit", rpc->rpc_Hunit);
	VPRINT("Runit", rpc->rpc_Runit);
	bn_mat_print("S o R", rpc->rpc_SoR );
	bn_mat_print("invR o S", rpc->rpc_invRoS );
}

/* hit_surfno is set to one of these */
#define	RPC_NORM_BODY	(1)		/* compute normal */
#define	RPC_NORM_TOP	(2)		/* copy tgc_N */
#define	RPC_NORM_FRT	(3)		/* copy reverse tgc_N */
#define RPC_NORM_BACK	(4)

/**
 *  			R T _ R P C _ S H O T
 *
 *  Intersect a ray with a rpc.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_rpc_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	register struct rpc_specific *rpc =
		(struct rpc_specific *)stp->st_specific;
	LOCAL vect_t	dprime;		/* D' */
	LOCAL vect_t	pprime;		/* P' */
	LOCAL fastf_t	k1, k2;		/* distance constants of solution */
	LOCAL vect_t	xlated;		/* translated vector */
	LOCAL struct hit hits[3];	/* 2 potential hit points */
	register struct hit *hitp;	/* pointer to hit point */
/*?????	const struct bn_tol	*tol = &rtip->rti_tol; ?????*/

	hitp = &hits[0];

	/* out, Mat, vect */
	MAT4X3VEC( dprime, rpc->rpc_SoR, rp->r_dir );
	VSUB2( xlated, rp->r_pt, rpc->rpc_V );
	MAT4X3VEC( pprime, rpc->rpc_SoR, xlated );

	/* Find roots of the equation, using formula for quadratic */
	if ( !NEAR_ZERO(dprime[Y], RT_PCOEF_TOL) ) {
		FAST fastf_t	a, b, c;	/* coeffs of polynomial */
		FAST fastf_t	disc;		/* disc of radical */

		a = dprime[Y] * dprime[Y];
		b = 2.0 * dprime[Y] * pprime[Y] - dprime[Z];
		c = pprime[Y] * pprime[Y] - pprime[Z] - 1;
		disc = b*b - 4 * a * c;
		if (disc <= 0)
			goto check_plates;
		disc = sqrt(disc);

		k1 = (-b + disc) / (2.0 * a);
		k2 = (-b - disc) / (2.0 * a);

		/*
		 *  k1 and k2 are potential solutions to intersection with
		 *  side.  See if they fall in range.
		 */
		VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );	/* hit' */
		if( hitp->hit_vpriv[X] >= -1.0 && hitp->hit_vpriv[X] <= 0.0
			&& hitp->hit_vpriv[Z] <= 0.0 ) {
			hitp->hit_magic = RT_HIT_MAGIC;
			hitp->hit_dist = k1;
			hitp->hit_surfno = RPC_NORM_BODY;	/* compute N */
			hitp++;
		}

		VJOIN1( hitp->hit_vpriv, pprime, k2, dprime );	/* hit' */
		if( hitp->hit_vpriv[X] >= -1.0 && hitp->hit_vpriv[X] <= 0.0
			&& hitp->hit_vpriv[Z] <= 0.0 ) {
			hitp->hit_magic = RT_HIT_MAGIC;
			hitp->hit_dist = k2;
			hitp->hit_surfno = RPC_NORM_BODY;	/* compute N */
			hitp++;
		}
	} else if ( !NEAR_ZERO(dprime[Z], RT_PCOEF_TOL) ) {
		k1 = (pprime[Y] * pprime[Y] - pprime[Z] - 1.0) / dprime[Z];
		VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );	/* hit' */
		if( hitp->hit_vpriv[X] >= -1.0 && hitp->hit_vpriv[X] <= 0.0
			&& hitp->hit_vpriv[Z] <= 0.0 ) {
			hitp->hit_magic = RT_HIT_MAGIC;
			hitp->hit_dist = k1;
			hitp->hit_surfno = RPC_NORM_BODY;	/* compute N */
			hitp++;
		}
	}

	/*
	 * Check for hitting the end plates.
	 */

check_plates:
	/* check front and back plates */
	if( hitp < &hits[2]  &&  !NEAR_ZERO(dprime[X], RT_PCOEF_TOL) )  {
		/* 0 or 1 hits so far, this is worthwhile */
		k1 = -pprime[X] / dprime[X];		/* front plate */
		k2 = (-1.0 - pprime[X]) / dprime[X];	/* back plate */

		VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );	/* hit' */
		if( hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y]
			- hitp->hit_vpriv[Z] <= 1.0
			&& hitp->hit_vpriv[Z] <= 0.0)  {
			hitp->hit_magic = RT_HIT_MAGIC;
			hitp->hit_dist = k1;
			hitp->hit_surfno = RPC_NORM_FRT;	/* -H */
			hitp++;
		}

		VJOIN1( hitp->hit_vpriv, pprime, k2, dprime );	/* hit' */
		if( hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y]
			- hitp->hit_vpriv[Z] <= 1.0
			&& hitp->hit_vpriv[Z] <= 0.0)  {
			hitp->hit_magic = RT_HIT_MAGIC;
			hitp->hit_dist = k2;
			hitp->hit_surfno = RPC_NORM_BACK;	/* +H */
			hitp++;
		}
	}

	/* check top plate */
	if( hitp == &hits[1]  &&  !NEAR_ZERO(dprime[Z], RT_PCOEF_TOL) )  {
		/* 1 hit so far, this is worthwhile */
		k1 = -pprime[Z] / dprime[Z];		/* top plate */

		VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );	/* hit' */
		if( hitp->hit_vpriv[X] >= -1.0 &&  hitp->hit_vpriv[X] <= 0.0
			&& hitp->hit_vpriv[Y] >= -1.0
			&& hitp->hit_vpriv[Y] <= 1.0 ) {
			hitp->hit_magic = RT_HIT_MAGIC;
			hitp->hit_dist = k1;
			hitp->hit_surfno = RPC_NORM_TOP;	/* -B */
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
		BU_LIST_INSERT( &(seghead->l), &(segp->l) );
	} else {
		/* entry is [1], exit is [0] */
		register struct seg *segp;

		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in = hits[1];		/* struct copy */
		segp->seg_out = hits[0];	/* struct copy */
		BU_LIST_INSERT( &(seghead->l), &(segp->l) );
	}
	return(2);			/* HIT */
}

#define RT_RPC_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/**
 *			R T _ R P C _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_rpc_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
             	               /* An array of solid pointers */
           		       /* An array of ray pointers */
                               /* array of segs (results returned) */
   		  	       /* Number of ray/object pairs */

{
	rt_vstub( stp, rp, segp, n, ap );
}

/**
 *  			R T _ R P C _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_rpc_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
	vect_t can_normal;	/* normal to canonical rpc */
	register struct rpc_specific *rpc =
		(struct rpc_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	switch( hitp->hit_surfno )  {
	case RPC_NORM_BODY:
		VSET( can_normal, 0.0, hitp->hit_vpriv[Y], -0.5 );
		MAT4X3VEC( hitp->hit_normal, rpc->rpc_invRoS, can_normal );
		VUNITIZE( hitp->hit_normal );
		break;
	case RPC_NORM_TOP:
		VREVERSE( hitp->hit_normal, rpc->rpc_Bunit );
		break;
	case RPC_NORM_FRT:
		VREVERSE( hitp->hit_normal, rpc->rpc_Hunit );
		break;
	case RPC_NORM_BACK:
		VMOVE( hitp->hit_normal, rpc->rpc_Hunit );
		break;
	default:
		bu_log("rt_rpc_norm: surfno=%d bad\n", hitp->hit_surfno);
		break;
	}
}

/**
 *			R T _ R P C _ C U R V E
 *
 *  Return the curvature of the rpc.
 */
void
rt_rpc_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
	fastf_t	zp1, zp2;	/* 1st & 2nd derivatives */
	register struct rpc_specific *rpc =
		(struct rpc_specific *)stp->st_specific;

	switch( hitp->hit_surfno )  {
	case RPC_NORM_BODY:
		/* most nearly flat direction */
		VMOVE( cvp->crv_pdir, rpc->rpc_Hunit );
		cvp->crv_c1 = 0;
		/* k = z'' / (1 + z'^2) ^ 3/2 */
		zp2 = 2 * rpc->rpc_b * rpc->rpc_inv_rsq;
		zp1 = zp2 * hitp->hit_point[Y];
		cvp->crv_c2 = zp2 / pow( (1 + zp1*zp1), 1.5);
		break;
	case RPC_NORM_BACK:
	case RPC_NORM_FRT:
	case RPC_NORM_TOP:
		/* any tangent direction */
	 	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	 	cvp->crv_c1 = cvp->crv_c2 = 0;
		break;
	}
}

/**
 *  			R T _ R P C _ U V
 *
 *  For a hit on the surface of an rpc, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_rpc_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
	register struct rpc_specific *rpc =
		(struct rpc_specific *)stp->st_specific;
	LOCAL vect_t work;
	LOCAL vect_t pprime;
	FAST fastf_t len;

	/*
	 * hit_point is on surface;  project back to unit rpc,
	 * creating a vector from vertex to hit point.
	 */
	VSUB2( work, hitp->hit_point, rpc->rpc_V );
	MAT4X3VEC( pprime, rpc->rpc_SoR, work );

	switch( hitp->hit_surfno )  {
	case RPC_NORM_BODY:
		/* Skin.  x,y coordinates define rotation.  radius = 1 */
		len = sqrt(pprime[Y]*pprime[Y] + pprime[Z]*pprime[Z]);
		uvp->uv_u = acos(pprime[Y]/len) * bn_invpi;
		uvp->uv_v = -pprime[X];		/* height */
		break;
	case RPC_NORM_FRT:
	case RPC_NORM_BACK:
		/* end plates - circular mapping, not seamless w/body, top */
		len = sqrt(pprime[Y]*pprime[Y] + pprime[Z]*pprime[Z]);
		uvp->uv_u = acos(pprime[Y]/len) * bn_invpi;
		uvp->uv_v = len;	/* rim v = 1 for both plates */
		break;
	case RPC_NORM_TOP:
		uvp->uv_u = 1.0 - (pprime[Y] + 1.0)/2.0;
		uvp->uv_v = -pprime[X];		/* height */
		break;
	}

	/* uv_du should be relative to rotation, uv_dv relative to height */
	uvp->uv_du = uvp->uv_dv = 0;
}

/**
 *		R T _ R P C _ F R E E
 */
void
rt_rpc_free(register struct soltab *stp)
{
	register struct rpc_specific *rpc =
		(struct rpc_specific *)stp->st_specific;

	bu_free( (char *)rpc, "rpc_specific" );
}

/**
 *			R T _ R P C _ C L A S S
 */
int
rt_rpc_class(void)
{
	return(0);
}

/**
 *			R T _ R P C _ P L O T
 */
int
rt_rpc_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	LOCAL struct rt_rpc_internal	*xip;
        fastf_t *front;
	fastf_t *back;
	fastf_t b, dtol, f, h, ntol, rh;
	int	i, n;
	LOCAL mat_t	R;
	LOCAL mat_t	invR;
	struct rt_pt_node	*old, *pos, *pts;
	vect_t	Bu, Hu, Ru;

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_rpc_internal *)ip->idb_ptr;
	RT_RPC_CK_MAGIC(xip);

	/* compute |B| |H| */
	b = MAGNITUDE( xip->rpc_B );	/* breadth */
	rh = xip->rpc_r;		/* rectangular halfwidth */
	h = MAGNITUDE( xip->rpc_H );	/* height */

	/* Check for |H| > 0, |B| > 0, |R| > 0 */
	if( NEAR_ZERO(h, RT_LEN_TOL) || NEAR_ZERO(b, RT_LEN_TOL)
	 || NEAR_ZERO(rh, RT_LEN_TOL) )  {
		bu_log("rt_rpc_plot():  zero length H, B, or rh\n");
		return(-2);		/* BAD */
	}

	/* Check for B.H == 0 */
	f = VDOT( xip->rpc_B, xip->rpc_H ) / (b * h);
	if( ! NEAR_ZERO(f, RT_DOT_TOL) )  {
		bu_log("rt_rpc_plot(): B not perpendicular to H, f=%f\n", f);
		return(-3);		/* BAD */
	}

	/* make unit vectors in B, H, and BxH directions */
	VMOVE(    Hu, xip->rpc_H );
	VUNITIZE( Hu );
	VMOVE(    Bu, xip->rpc_B );
	VUNITIZE( Bu );
	VCROSS(   Ru, Bu, Hu );

	/* Compute R and Rinv matrices */
	MAT_IDN( R );
	VREVERSE( &R[0], Hu );
	VMOVE(    &R[4], Ru );
	VREVERSE( &R[8], Bu );
	bn_mat_trn( invR, R );			/* inv of rot mat is trn */

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
		ntol = bn_pi;

#if 1
	/* initial parabola approximation is a single segment */
	pts = rt_ptalloc();
	pts->next = rt_ptalloc();
	pts->next->next = NULL;
	VSET( pts->p,       0, -rh, 0);
	VSET( pts->next->p, 0,  rh, 0);
	/* 2 endpoints in 1st approximation */
	n = 2;
	/* recursively break segment 'til within error tolerances */
	n += rt_mk_parabola( pts, rh, b, dtol, ntol );

	/* get mem for arrays */
	front = (fastf_t *)bu_malloc(3*n * sizeof(fastf_t), "fastf_t");
	back  = (fastf_t *)bu_malloc(3*n * sizeof(fastf_t), "fastf_t");

	/* generate front & back plates in world coordinates */
	pos = pts;
	i = 0;
	while (pos) {
		/* rotate back to original position */
		MAT4X3VEC( &front[i], invR, pos->p );
		/* move to origin vertex origin */
		VADD2( &front[i], &front[i], xip->rpc_V );
		/* extrude front to create back plate */
		VADD2( &back[i], &front[i], xip->rpc_H );
		i += 3;
		old = pos;
		pos = pos->next;
		bu_free( (char *)old, "rt_pt_node" );
	}
#else
	/* initial parabola approximation is a single segment */
	pts = rt_ptalloc();
	pts->next = rt_ptalloc();
	pts->next->next = NULL;
	VSET( pts->p,       0,   0, -b);
	VSET( pts->next->p, 0,  rh,  0);
	/* 2 endpoints in 1st approximation */
	n = 2;
	/* recursively break segment 'til within error tolerances */
	n += rt_mk_parabola( pts, rh, b, dtol, ntol );

	/* get mem for arrays */
	front = (fastf_t *)bu_malloc((2*3*n-1) * sizeof(fastf_t), "fastf_t");
	back  = (fastf_t *)bu_malloc((2*3*n-1) * sizeof(fastf_t), "fastf_t");

	/* generate front & back plates in world coordinates */
	pos = pts;
	i = 0;
	while (pos) {
		/* rotate back to original position */
		MAT4X3VEC( &front[i], invR, pos->p );
		/* move to origin vertex origin */
		VADD2( &front[i], &front[i], xip->rpc_V );
		/* extrude front to create back plate */
		VADD2( &back[i], &front[i], xip->rpc_H );
		i += 3;
		old = pos;
		pos = pos->next;
		bu_free( (char *)old, "rt_pt_node" );
	}
	for (i = 3*n; i < 6*n-3; i+=3) {
		VMOVE( &front[i], &front[6*n-i-6] );
		front[i+1] = -front[i+1];
		VMOVE( &back[i], &back[6*n-i-6] );
		back[i+1] = -back[i+1];
	}
	n = 2*n - 1;
#endif

	/* Draw the front */
	RT_ADD_VLIST( vhead, &front[(n-1)*ELEMENTS_PER_VECT],
		BN_VLIST_LINE_MOVE );
	for( i = 0; i < n; i++ )  {
		RT_ADD_VLIST( vhead, &front[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
	}

	/* Draw the back */
	RT_ADD_VLIST( vhead, &back[(n-1)*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE );
	for( i = 0; i < n; i++ )  {
		RT_ADD_VLIST( vhead, &back[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
	}

	/* Draw connections */
	for( i = 0; i < n; i++ )  {
		RT_ADD_VLIST( vhead, &front[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vhead, &back[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
	}

	bu_free( (char *)front, "fastf_t" );
	bu_free( (char *)back,  "fastf_t" );

	return(0);
}

/**
 *	R T _ M K _ P A R A B O L A
 *
 *	Approximate a parabola with line segments.  The initial single
 *	segment is broken at the point farthest from the parabola if
 *	that point is not aleady within the distance and normal error
 *	tolerances.  The two resulting segments are passed recursively
 *	to this routine until each segment is within tolerance.
 */
int
rt_mk_parabola(struct rt_pt_node *pts, fastf_t r, fastf_t b, fastf_t dtol, fastf_t ntol)
{
	fastf_t	dist, intr, m, theta0, theta1;
	int	n;
	point_t	mpt, p0, p1;
	vect_t	norm_line, norm_parab;
	struct rt_pt_node *new, *rt_ptalloc(void);

#define RPC_TOL .0001
	/* endpoints of segment approximating parabola */
	VMOVE( p0, pts->p );
	VMOVE( p1, pts->next->p );
	/* slope and intercept of segment */
	m = ( p1[Z] - p0[Z] ) / ( p1[Y] - p0[Y] );
	intr = p0[Z] - m * p0[Y];
	/* point on parabola with max dist between parabola and line */
	mpt[X] = 0;
	mpt[Y] = (r * r * m) / (2 * b);
	if (NEAR_ZERO( mpt[Y], RPC_TOL))
		mpt[Y] = 0.;
	mpt[Z] = (mpt[Y] * m / 2) - b;
	if (NEAR_ZERO( mpt[Z], RPC_TOL))
		mpt[Z] = 0.;
	/* max distance between that point and line */
	dist = fabs( mpt[Z] + b + b + intr ) / sqrt( m * m + 1 );
	/* angles between normal of line and of parabola at line endpoints */
	VSET( norm_line, m, -1., 0.);
	VSET( norm_parab, 2. * b / (r * r) * p0[Y], -1., 0.);
	VUNITIZE( norm_line );
	VUNITIZE( norm_parab );
	theta0 = fabs( acos( VDOT( norm_line, norm_parab )));
	VSET( norm_parab, 2. * b / (r * r) * p1[Y], -1., 0.);
	VUNITIZE( norm_parab );
	theta1 = fabs( acos( VDOT( norm_line, norm_parab )));
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
		n += rt_mk_parabola( pts, r, b, dtol, ntol );
		/* recurse on second new segment */
		n += rt_mk_parabola( new, r, b, dtol, ntol );
	} else
		n  = 0;
	return( n );
}

/*
 *			R T _ P T A L L O C
 */
struct rt_pt_node *
rt_ptalloc(void)
{
	struct rt_pt_node *mem;

	mem = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
	if (!mem) {
		fprintf(stderr, "rt_ptalloc: no more memory!\n");
		exit(-1);
	}
	return(mem);
}

/**
 *			R T _ R P C _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_rpc_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	int		i, j, n;
	fastf_t		b, *back, f, *front, h, rh;
	fastf_t		dtol, ntol;
	vect_t		Bu, Hu, Ru;
	LOCAL mat_t	R;
	LOCAL mat_t	invR;
	LOCAL struct rt_rpc_internal	*xip;
	struct rt_pt_node	*old, *pos, *pts;
	struct shell	*s;
	struct faceuse	**outfaceuses;
	struct vertex	**vfront, **vback, **vtemp, *vertlist[4];
	vect_t		*norms;
	fastf_t		r_sq_over_b;

	NMG_CK_MODEL( m );
	BN_CK_TOL( tol );
	RT_CK_TESS_TOL( ttol );

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_rpc_internal *)ip->idb_ptr;
	RT_RPC_CK_MAGIC(xip);

	/* compute |B| |H| */
	b = MAGNITUDE( xip->rpc_B );	/* breadth */
	rh = xip->rpc_r;		/* rectangular halfwidth */
	h = MAGNITUDE( xip->rpc_H );	/* height */

	/* Check for |H| > 0, |B| > 0, |R| > 0 */
	if( NEAR_ZERO(h, RT_LEN_TOL) || NEAR_ZERO(b, RT_LEN_TOL)
	 || NEAR_ZERO(rh, RT_LEN_TOL) )  {
		bu_log("rt_rpc_tess():  zero length H, B, or rh\n");
		return(-2);		/* BAD */
	}

	/* Check for B.H == 0 */
	f = VDOT( xip->rpc_B, xip->rpc_H ) / (b * h);
	if( ! NEAR_ZERO(f, RT_DOT_TOL) )  {
		bu_log("rt_rpc_tess(): B not perpendicular to H, f=%f\n", f);
		return(-3);		/* BAD */
	}

	/* make unit vectors in B, H, and BxH directions */
	VMOVE(    Hu, xip->rpc_H );
	VUNITIZE( Hu );
	VMOVE(    Bu, xip->rpc_B );
	VUNITIZE( Bu );
	VCROSS(   Ru, Bu, Hu );

	/* Compute R and Rinv matrices */
	MAT_IDN( R );
	VREVERSE( &R[0], Hu );
	VMOVE(    &R[4], Ru );
	VREVERSE( &R[8], Bu );
	bn_mat_trn( invR, R );			/* inv of rot mat is trn */

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
		ntol = bn_pi;

	/* initial parabola approximation is a single segment */
	pts = rt_ptalloc();
	pts->next = rt_ptalloc();
	pts->next->next = NULL;
	VSET( pts->p,       0, -rh, 0);
	VSET( pts->next->p, 0,  rh, 0);
	/* 2 endpoints in 1st approximation */
	n = 2;
	/* recursively break segment 'til within error tolerances */
	n += rt_mk_parabola( pts, rh, b, dtol, ntol );

	/* get mem for arrays */
	front = (fastf_t *)bu_malloc(3*n * sizeof(fastf_t), "fastf_t");
	back  = (fastf_t *)bu_malloc(3*n * sizeof(fastf_t), "fastf_t");
	norms = (vect_t *)bu_calloc( n , sizeof( vect_t ) , "rt_rpc_tess: norms" );
	vfront = (struct vertex **)bu_malloc((n+1) * sizeof(struct vertex *), "vertex *");
	vback = (struct vertex **)bu_malloc((n+1) * sizeof(struct vertex *), "vertex *");
	vtemp = (struct vertex **)bu_malloc((n+1) * sizeof(struct vertex *), "vertex *");
	outfaceuses =
		(struct faceuse **)bu_malloc((n+2) * sizeof(struct faceuse *), "faceuse *");
	if (!front || !back || !vfront || !vback || !vtemp || !outfaceuses) {
		fprintf(stderr, "rt_rpc_tess: no memory!\n");
		exit(-1);
	}

	/* generate front & back plates in world coordinates */
	r_sq_over_b = rh * rh / b;
	pos = pts;
	i = 0;
	j = 0;
	while (pos) {
		vect_t tmp_norm;

		VSET( tmp_norm , 0.0 , 2.0 * pos->p[Y] , -r_sq_over_b );
		MAT4X3VEC( norms[j] , invR , tmp_norm );
		VUNITIZE( norms[j] );
		/* rotate back to original position */
		MAT4X3VEC( &front[i], invR, pos->p );
		/* move to origin vertex origin */
		VADD2( &front[i], &front[i], xip->rpc_V );
		/* extrude front to create back plate */
		VADD2( &back[i], &front[i], xip->rpc_H );
		i += 3;
		j++;
		old = pos;
		pos = pos->next;
		bu_free( (char *)old, "rt_pt_node" );
	}

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = BU_LIST_FIRST(shell, &(*r)->s_hd);

	for( i=0; i<n; i++ )  {
		vfront[i] = vtemp[i] = (struct vertex *)0;
	}

	/* Front face topology.  Verts are considered to go CCW */
	outfaceuses[0] = nmg_cface(s, vfront, n);

	(void)nmg_mark_edges_real( &outfaceuses[0]->l.magic );

	/* Back face topology.  Verts must go in opposite dir (CW) */
	outfaceuses[1] = nmg_cface(s, vtemp, n);

	(void)nmg_mark_edges_real( &outfaceuses[1]->l.magic );

	for( i=0; i<n; i++ )  vback[i] = vtemp[n-1-i];

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

	(void)nmg_mark_edges_real( &outfaceuses[n+1]->l.magic );

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
			/* free mem */
			bu_free( (char *)front, "fastf_t");
			bu_free( (char *)back, "fastf_t");
			bu_free( (char*)vfront, "vertex *");
			bu_free( (char*)vback, "vertex *");
			bu_free( (char*)vtemp, "vertex *");
			bu_free( (char*)outfaceuses, "faceuse *");

			return(-1);		/* FAIL */
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
		for( BU_LIST_FOR( vu , vertexuse , &vfront[i]->vu_hd ) )
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
		for( BU_LIST_FOR( vu , vertexuse , &vback[i]->vu_hd ) )
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

	/* free mem */
	bu_free( (char *)front, "fastf_t");
	bu_free( (char *)back, "fastf_t");
	bu_free( (char*)vfront, "vertex *");
	bu_free( (char*)vback, "vertex *");
	bu_free( (char*)vtemp, "vertex *");
	bu_free( (char*)outfaceuses, "faceuse *");

	return(0);
}

/**
 *			R T _ R P C _ I M P O R T
 *
 *  Import an RPC from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_rpc_import(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	LOCAL struct rt_rpc_internal	*xip;
	union record			*rp;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != ID_SOLID )  {
		bu_log("rt_rpc_import: defective record\n");
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_RPC;
	ip->idb_meth = &rt_functab[ID_RPC];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_rpc_internal), "rt_rpc_internal");
	xip = (struct rt_rpc_internal *)ip->idb_ptr;
	xip->rpc_magic = RT_RPC_INTERNAL_MAGIC;

	/* Warning:  type conversion */
	MAT4X3PNT( xip->rpc_V, mat, &rp->s.s_values[0*3] );
	MAT4X3VEC( xip->rpc_H, mat, &rp->s.s_values[1*3] );
	MAT4X3VEC( xip->rpc_B, mat, &rp->s.s_values[2*3] );
	xip->rpc_r = rp->s.s_values[3*3] / mat[15];

	if( xip->rpc_r < SMALL_FASTF )
	{
		bu_log( "rt_rpc_import: r is zero\n" );
		bu_free( (char *)ip->idb_ptr , "rt_rpc_import: ip->idp_ptr" );
		return( -1 );
	}

	return(0);			/* OK */
}

/**
 *			R T _ R P C _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_rpc_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_rpc_internal	*xip;
	union record		*rpc;
	fastf_t			f,mag_b,mag_h;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_RPC )  return(-1);
	xip = (struct rt_rpc_internal *)ip->idb_ptr;
	RT_RPC_CK_MAGIC(xip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "rpc external");
	rpc = (union record *)ep->ext_buf;

	rpc->s.s_id = ID_SOLID;
	rpc->s.s_type = RPC;

	mag_b = MAGNITUDE( xip->rpc_B );
	mag_h = MAGNITUDE( xip->rpc_H );

	if( mag_b < RT_LEN_TOL || mag_h < RT_LEN_TOL || xip->rpc_r < RT_LEN_TOL) {
		bu_log("rt_rpc_export: not all dimensions positive!\n");
		return(-1);
	}

	f = VDOT(xip->rpc_B, xip->rpc_H) / (mag_b * mag_h );
	if ( !NEAR_ZERO( f , RT_DOT_TOL) ) {
		bu_log("rt_rpc_export: B and H are not perpendicular! (dot = %g)\n",f );
		return(-1);
	}

	/* Warning:  type conversion */
	VSCALE( &rpc->s.s_values[0*3], xip->rpc_V, local2mm );
	VSCALE( &rpc->s.s_values[1*3], xip->rpc_H, local2mm );
	VSCALE( &rpc->s.s_values[2*3], xip->rpc_B, local2mm );
	rpc->s.s_values[3*3] = xip->rpc_r * local2mm;

	return(0);
}

/**
 *			R T _ R P C _ I M P O R T 5
 *
 *  Import an RPC from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_rpc_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	struct rt_rpc_internal	*xip;
	fastf_t			vec[10];

	BU_CK_EXTERNAL( ep );

	BU_ASSERT_LONG( ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 10 );

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_RPC;
	ip->idb_meth = &rt_functab[ID_RPC];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_rpc_internal), "rt_rpc_internal");

	xip = (struct rt_rpc_internal *)ip->idb_ptr;
	xip->rpc_magic = RT_RPC_INTERNAL_MAGIC;

	/* Convert from database (network) to internal (host) format */
	ntohd( (unsigned char *)vec, ep->ext_buf, 10 );

	/* Apply modeling transformations */
	MAT4X3PNT( xip->rpc_V, mat, &vec[0*3] );
	MAT4X3VEC( xip->rpc_H, mat, &vec[1*3] );
	MAT4X3VEC( xip->rpc_B, mat, &vec[2*3] );
	xip->rpc_r = vec[3*3] / mat[15];

	if( xip->rpc_r < SMALL_FASTF )
	{
		bu_log( "rt_rpc_import: r is zero\n" );
		bu_free( (char *)ip->idb_ptr , "rt_rpc_import: ip->idp_ptr" );
		return( -1 );
	}

	return(0);			/* OK */
}

/**
 *			R T _ R P C _ E X P O R T 5
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_rpc_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_rpc_internal	*xip;
	fastf_t			vec[10];
	fastf_t			f,mag_b,mag_h;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_RPC )  return(-1);
	xip = (struct rt_rpc_internal *)ip->idb_ptr;
	RT_RPC_CK_MAGIC(xip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * 10;
	ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes, "rpc external");

	mag_b = MAGNITUDE( xip->rpc_B );
	mag_h = MAGNITUDE( xip->rpc_H );

	if( mag_b < RT_LEN_TOL || mag_h < RT_LEN_TOL || xip->rpc_r < RT_LEN_TOL) {
		bu_log("rt_rpc_export: not all dimensions positive!\n");
		return(-1);
	}

	f = VDOT(xip->rpc_B, xip->rpc_H) / (mag_b * mag_h );
	if ( !NEAR_ZERO( f , RT_DOT_TOL) ) {
		bu_log("rt_rpc_export: B and H are not perpendicular! (dot = %g)\n",f );
		return(-1);
	}

	/* scale 'em into local buffer */
	VSCALE( &vec[0*3], xip->rpc_V, local2mm );
	VSCALE( &vec[1*3], xip->rpc_H, local2mm );
	VSCALE( &vec[2*3], xip->rpc_B, local2mm );
	vec[3*3] = xip->rpc_r * local2mm;

	/* Convert from internal (host) to database (network) format */
	htond( ep->ext_buf, (unsigned char *)vec, 10 );

	return(0);
}

/**
 *			R T _ R P C _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_rpc_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	register struct rt_rpc_internal	*xip =
		(struct rt_rpc_internal *)ip->idb_ptr;
	char	buf[256];

	RT_RPC_CK_MAGIC(xip);
	bu_vls_strcat( str, "Right Parabolic Cylinder (RPC)\n");

	sprintf(buf, "\tV (%g, %g, %g)\n",
		INTCLAMP(xip->rpc_V[X] * mm2local),
		INTCLAMP(xip->rpc_V[Y] * mm2local),
		INTCLAMP(xip->rpc_V[Z] * mm2local) );
	bu_vls_strcat( str, buf );

	sprintf(buf, "\tB (%g, %g, %g) mag=%g\n",
		INTCLAMP(xip->rpc_B[X] * mm2local),
		INTCLAMP(xip->rpc_B[Y] * mm2local),
		INTCLAMP(xip->rpc_B[Z] * mm2local),
		INTCLAMP(MAGNITUDE(xip->rpc_B) * mm2local));
	bu_vls_strcat( str, buf );

	sprintf(buf, "\tH (%g, %g, %g) mag=%g\n",
		INTCLAMP(xip->rpc_H[X] * mm2local),
		INTCLAMP(xip->rpc_H[Y] * mm2local),
		INTCLAMP(xip->rpc_H[Z] * mm2local),
		INTCLAMP(MAGNITUDE(xip->rpc_H) * mm2local));
	bu_vls_strcat( str, buf );

	sprintf(buf, "\tr=%g\n", INTCLAMP(xip->rpc_r * mm2local));
	bu_vls_strcat( str, buf );

	return(0);
}

/**
 *			R T _ R P C _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_rpc_ifree(struct rt_db_internal *ip)
{
	register struct rt_rpc_internal	*xip;

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_rpc_internal *)ip->idb_ptr;
	RT_RPC_CK_MAGIC(xip);
	xip->rpc_magic = 0;		/* sanity */

	bu_free( (char *)xip, "rpc ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
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
