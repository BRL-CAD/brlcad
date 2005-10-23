/*                         G _ E P A . C
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

/** \addtogroup ellipsoids */

/*@{*/
/** @file g_epa.c
 *	Intersect a ray with an Elliptical Paraboloid.
 *
 *  Algorithm -
 *
 *  Given V, H, R, and B, there is a set of points on this epa
 *
 *  { (x,y,z) | (x,y,z) is on epa }
 *
 *  Through a series of Affine Transformations, this set of points will be
 *  transformed into a set of points on an epa located at the origin
 *  with a semi-major axis R1 along the +Y axis, a semi-minor axis R2
 *  along the -X axis, a height H along the -Z axis, and a vertex V at
 *  the origin.
 *
 *
 *  { (x',y',z') | (x',y',z') is on epa at origin }
 *
 *  The transformation from X to X' is accomplished by:
 *
 *  X' = S(R( X - V ))
 *
 *  where R(X) =  ( R1/(-|R1|) )
 *  		 (  R2/( |R2|)  ) . X
 *  		  ( H /(-|H |) )
 *
 *  and S(X) =	 (  1/|R1|   0     0   )
 *  		(    0    1/|R2|   0    ) . X
 *  		 (   0      0   1/|H | )
 *
 *  To find the intersection of a line with the surface of the epa,
 *  consider the parametric line L:
 *
 *  	L : { P(n) | P + t(n) . D }
 *
 *  Call W the actual point of intersection between L and the epa.
 *  Let W' be the point of intersection between L' and the unit epa.
 *
 *  	L' : { P'(n) | P' + t(n) . D' }
 *
 *  W = invR( invS( W' ) ) + V
 *
 *  Where W' = k D' + P'.
 *
 *  If Dy' and Dz' are both 0, then there is no hit on the epa;
 *  but the end plates need checking.  If there is now only 1 hit
 *  point, the top plate needs to be checked as well.
 *
 *  Line L' hits the infinitely long epa at W' when
 *
 *	A * k**2 + B * k + C = 0
 *
 *  where
 *
 *  A = Dx'**2 + Dy'**2
 *  B = 2 * (Dx' * Px' + Dy' * Py') - Dz'
 *  C = Px'**2 + Py'**2 - Pz' - 1
 *  b = |Breadth| = 1.0
 *  h = |Height| = 1.0
 *  r = 1.0
 *
 *  The quadratic formula yields k (which is constant):
 *
 *  k = [ -B +/- sqrt( B**2 - 4 * A * C )] / (2.0 * A)
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
 *  The hit at ``k'' is a hit on the canonical epa IFF
 *  Wz' <= 0.
 *
 *  NORMALS.  Given the point W on the surface of the epa,
 *  what is the vector normal to the tangent plane at that point?
 *
 *  Map W onto the unit epa, ie:  W' = S( R( W - V ) ).
 *
 *  Plane on unit epa at W' has a normal vector N' where
 *
 *  N' = <Wx', Wy', -.5>.
 *
 *  The plane transforms back to the tangent plane at W, and this
 *  new plane (on the original epa) has a normal vector of N, viz:
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
 *  THE TOP PLATE.
 *
 *  If Dz' == 0, line L' is parallel to the top plate, so there is no
 *  hit on the top plate.  Otherwise, rays intersect the top plate
 *  with k = (0 - Pz')/Dz'.  The solution is within the top plate
 *  IFF Wx'**2 + Wy'**2 <= 1.
 *
 *  The normal for a hit on the top plate is -Hunit.
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
static const char RCSepa[] = "@(#)$Header$ (BRL)";
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

struct epa_specific {
	point_t	epa_V;		/* vector to epa origin */
	vect_t	epa_Hunit;	/* unit H vector */
	vect_t	epa_Aunit;	/* unit vector along semi-major axis */
	vect_t	epa_Bunit;	/* unit vector, A x H */
	fastf_t	epa_h;		/* |H| */
	fastf_t	epa_inv_r1sq;	/* 1/(r1 * r1) */
	fastf_t	epa_inv_r2sq;	/* 1/(r2 * r2) */
	mat_t	epa_SoR;	/* Scale(Rot(vect)) */
	mat_t	epa_invRoS;	/* invRot(Scale(vect)) */
};

const struct bu_structparse rt_epa_parse[] = {
    { "%f", 3, "V",   offsetof(struct rt_epa_internal, epa_V[X]),  BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "H",   offsetof(struct rt_epa_internal, epa_H[X]),  BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "A",   offsetof(struct rt_epa_internal, epa_Au[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "r_1", offsetof(struct rt_epa_internal, epa_r1),    BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "r_2", offsetof(struct rt_epa_internal, epa_r2),    BU_STRUCTPARSE_FUNC_NULL },
    { {'\0','\0','\0','\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL }
};

/**
 *  			R T _ E P A _ P R E P
 *
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid EPA, and if so, precompute various
 *  terms of the formula.
 *
 *  Returns -
 *  	0	EPA is OK
 *  	!0	Error in description
 *
 *  Implicit return -
 *  	A struct epa_specific is created, and it's address is stored in
 *  	stp->st_specific for use by epa_shot().
 */
int
rt_epa_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	struct rt_epa_internal		*xip;
	register struct epa_specific	*epa;
#ifndef NO_MAGIC_CHECKING
	const struct bn_tol		*tol = &rtip->rti_tol;
#endif
	LOCAL fastf_t	magsq_h;
	LOCAL fastf_t	mag_a, mag_h;
	LOCAL fastf_t	f, r1, r2;
	LOCAL mat_t	R;
	LOCAL mat_t	Rinv;
	LOCAL mat_t	S;

#ifndef NO_MAGIC_CHECKING
	RT_CK_DB_INTERNAL(ip);
	BN_CK_TOL(tol);
#endif
	xip = (struct rt_epa_internal *)ip->idb_ptr;
	RT_EPA_CK_MAGIC(xip);

	/* compute |A| |H| */
	mag_a = sqrt( MAGSQ( xip->epa_Au ) );
	mag_h = sqrt( magsq_h = MAGSQ( xip->epa_H ) );
	r1 = xip->epa_r1;
	r2 = xip->epa_r2;
	/* Check for |H| > 0, |A| == 1, r1 >  0, r2 > 0 */
	if( NEAR_ZERO(mag_h, RT_LEN_TOL)
		|| !NEAR_ZERO(mag_a - 1.0, RT_LEN_TOL)
		|| r1 < 0.0 || r2 < 0.0 )  {
		return(1);		/* BAD, too small */
	}

	/* Check for A.H == 0 */
	f = VDOT( xip->epa_Au, xip->epa_H ) / mag_h;
	if( ! NEAR_ZERO(f, RT_DOT_TOL) )  {
		return(1);		/* BAD */
	}

	/*
	 *  EPA is ok
	 */
	stp->st_id = ID_EPA;		/* set soltab ID */
	stp->st_meth = &rt_functab[ID_EPA];

	BU_GETSTRUCT( epa, epa_specific );
	stp->st_specific = (genptr_t)epa;

	epa->epa_h = mag_h;
	epa->epa_inv_r1sq = 1 / (r1 * r1);
	epa->epa_inv_r2sq = 1 / (r2 * r2);

	/* make unit vectors in A, H, and BxH directions */
	VMOVE(    epa->epa_Hunit, xip->epa_H );
	VUNITIZE( epa->epa_Hunit );
	VMOVE(    epa->epa_Aunit, xip->epa_Au );
	VCROSS(   epa->epa_Bunit, epa->epa_Aunit, epa->epa_Hunit );

	VMOVE( epa->epa_V, xip->epa_V );

	/* Compute R and Rinv matrices */
	MAT_IDN( R );
	VREVERSE( &R[0], epa->epa_Bunit );
	VMOVE(    &R[4], epa->epa_Aunit );
	VREVERSE( &R[8], epa->epa_Hunit );
	bn_mat_trn( Rinv, R );			/* inv of rot mat is trn */

	/* Compute S */
	MAT_IDN( S );
	S[ 0] = 1.0/r2;
	S[ 5] = 1.0/r1;
	S[10] = 1.0/mag_h;

	/* Compute SoR and invRoS */
	bn_mat_mul( epa->epa_SoR, S, R );
	bn_mat_mul( epa->epa_invRoS, Rinv, S );

	/* Compute bounding sphere and RPP */
	/* bounding sphere center */
	VJOIN1( stp->st_center, epa->epa_V, mag_h / 2.0, epa->epa_Hunit );
	/* bounding radius */
	stp->st_bradius = sqrt(0.25*magsq_h + r2*r2 + r1*r1);
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
 *			R T _ E P A _ P R I N T
 */
void
rt_epa_print(register const struct soltab *stp)
{
	register const struct epa_specific *epa =
		(struct epa_specific *)stp->st_specific;

	VPRINT("V", epa->epa_V);
	VPRINT("Hunit", epa->epa_Hunit);
	VPRINT("Aunit", epa->epa_Aunit);
	VPRINT("Bunit", epa->epa_Bunit);
	bn_mat_print("S o R", epa->epa_SoR );
	bn_mat_print("invR o S", epa->epa_invRoS );
}

/* hit_surfno is set to one of these */
#define	EPA_NORM_BODY	(1)		/* compute normal */
#define	EPA_NORM_TOP	(2)		/* copy epa_N */

/**
 *  			R T _ E P A _ S H O T
 *
 *  Intersect a ray with a epa.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_epa_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	register struct epa_specific *epa =
		(struct epa_specific *)stp->st_specific;
	LOCAL vect_t	dprime;		/* D' */
	LOCAL vect_t	pprime;		/* P' */
	LOCAL fastf_t	k1, k2;		/* distance constants of solution */
	LOCAL vect_t	xlated;		/* translated vector */
	LOCAL struct hit hits[3];	/* 2 potential hit points */
	register struct hit *hitp;	/* pointer to hit point */

	hitp = &hits[0];

	/* out, Mat, vect */
	MAT4X3VEC( dprime, epa->epa_SoR, rp->r_dir );
	VSUB2( xlated, rp->r_pt, epa->epa_V );
	MAT4X3VEC( pprime, epa->epa_SoR, xlated );

	/* Find roots of the equation, using formula for quadratic */
	{
		FAST fastf_t	a, b, c;	/* coeffs of polynomial */
		FAST fastf_t	disc;		/* disc of radical */

		a = dprime[X] * dprime[X] + dprime[Y] * dprime[Y];
		b = 2*(dprime[X] * pprime[X] + dprime[Y] * pprime[Y])
			- dprime[Z];
		c = pprime[X] * pprime[X]
			+ pprime[Y] * pprime[Y] - pprime[Z] - 1.0;
		if ( !NEAR_ZERO(a, RT_PCOEF_TOL) ) {
			disc = b*b - 4 * a * c;
			if (disc <= 0)
				goto check_plates;
			disc = sqrt(disc);

			k1 = (-b + disc) / (2.0 * a);
			k2 = (-b - disc) / (2.0 * a);

			/*
			 *  k1 and k2 are potential solutions to intersection
			 *  with side.  See if they fall in range.
			 */
			VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );	/* hit' */
			if( hitp->hit_vpriv[Z] <= 0.0 ) {
				hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hit_dist = k1;
				hitp->hit_surfno = EPA_NORM_BODY;	/* compute N */
				hitp++;
			}

			VJOIN1( hitp->hit_vpriv, pprime, k2, dprime );	/* hit' */
			if( hitp->hit_vpriv[Z] <= 0.0 ) {
				hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hit_dist = k2;
				hitp->hit_surfno = EPA_NORM_BODY;	/* compute N */
				hitp++;
			}
		} else if ( !NEAR_ZERO(b, RT_PCOEF_TOL) ) {
			k1 = -c/b;
			VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );	/* hit' */
			if( hitp->hit_vpriv[Z] <= 0.0 ) {
				hitp->hit_magic = RT_HIT_MAGIC;
				hitp->hit_dist = k1;
				hitp->hit_surfno = EPA_NORM_BODY;	/* compute N */
				hitp++;
			}
		}
	}


	/*
	 * Check for hitting the top plate.
	 */
check_plates:
	/* check top plate */
	if( hitp == &hits[1]  &&  !NEAR_ZERO(dprime[Z], SMALL) )  {
		/* 1 hit so far, this is worthwhile */
		k1 = -pprime[Z] / dprime[Z];		/* top plate */

		VJOIN1( hitp->hit_vpriv, pprime, k1, dprime );	/* hit' */
		if( hitp->hit_vpriv[X] * hitp->hit_vpriv[X] +
			hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y] <= 1.0 ) {
			hitp->hit_magic = RT_HIT_MAGIC;
			hitp->hit_dist = k1;
			hitp->hit_surfno = EPA_NORM_TOP;	/* -H */
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

#define RT_EPA_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/**
 *			R T _ E P A _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_epa_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
             	               /* An array of solid pointers */
           		       /* An array of ray pointers */
                               /* array of segs (results returned) */
   		  	       /* Number of ray/object pairs */

{
	rt_vstub( stp, rp, segp, n, ap );
}

/**
 *  			R T _ E P A _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_epa_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
	fastf_t	scale;
	vect_t	can_normal;	/* normal to canonical epa */
	register struct epa_specific *epa =
		(struct epa_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	switch( hitp->hit_surfno )  {
	case EPA_NORM_BODY:
		VSET( can_normal,
			hitp->hit_vpriv[X],
			hitp->hit_vpriv[Y],
			-0.5 );
		MAT4X3VEC( hitp->hit_normal, epa->epa_invRoS, can_normal );
		scale = 1.0 / MAGNITUDE( hitp->hit_normal );
		VSCALE( hitp->hit_normal, hitp->hit_normal, scale );

		/* tuck away this scale for the curvature routine */
		hitp->hit_vpriv[X] = scale;
		break;
	case EPA_NORM_TOP:
		VREVERSE( hitp->hit_normal, epa->epa_Hunit );
		break;
	default:
		bu_log("rt_epa_norm: surfno=%d bad\n", hitp->hit_surfno);
		break;
	}
}

/**
 *			R T _ E P A _ C U R V E
 *
 *  Return the curvature of the epa.
 */
void
rt_epa_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
	fastf_t	a, b, c, scale;
	mat_t	M1, M2;
	register struct epa_specific *epa =
		(struct epa_specific *)stp->st_specific;
	vect_t	u, v;			/* basis vectors (with normal) */
	vect_t	vec1, vec2;		/* eigen vectors */
	vect_t	tmp;

	switch( hitp->hit_surfno )  {
	case EPA_NORM_BODY:
		/*
		 * choose a tangent plane coordinate system
		 *  (u, v, normal) form a right-handed triple
		 */
		bn_vec_ortho( u, hitp->hit_normal );
		VCROSS( v, hitp->hit_normal, u );

		/* get the saved away scale factor */
		scale = - hitp->hit_vpriv[X];

		MAT_IDN( M1 );
		M1[10] = 0;	/* M1[3,3] = 0 */
		/* M1 = invR * S * M1 * S * R */
		bn_mat_mul( M2, epa->epa_invRoS, M1);
		bn_mat_mul( M1, M2, epa->epa_SoR );

		/* find the second fundamental form */
		MAT4X3VEC( tmp, M1, u );
		a = VDOT(u, tmp) * scale;
		b = VDOT(v, tmp) * scale;
		MAT4X3VEC( tmp, M1, v );
		c = VDOT(v, tmp) * scale;

		eigen2x2( &cvp->crv_c1, &cvp->crv_c2, vec1, vec2, a, b, c );
		VCOMB2( cvp->crv_pdir, vec1[X], u, vec1[Y], v );
		VUNITIZE( cvp->crv_pdir );
		break;
	case EPA_NORM_TOP:
	 	cvp->crv_c1 = cvp->crv_c2 = 0;
		/* any tangent direction */
	 	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	 	break;
	}
}

/**
 *  			R T _ E P A _ U V
 *
 *  For a hit on the surface of an epa, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_epa_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
	register struct epa_specific *epa =
		(struct epa_specific *)stp->st_specific;
	LOCAL vect_t work;
	LOCAL vect_t pprime;
	FAST fastf_t len;

	/*
	 * hit_point is on surface;  project back to unit epa,
	 * creating a vector from vertex to hit point.
	 */
	VSUB2( work, hitp->hit_point, epa->epa_V );
	MAT4X3VEC( pprime, epa->epa_SoR, work );

	switch( hitp->hit_surfno )  {
	case EPA_NORM_BODY:
		/* top plate, polar coords */
		if (pprime[Z] == -1.0) {	/* bottom pt of body */
			uvp->uv_u = 0;
		} else {
			len = sqrt(pprime[X]*pprime[X] + pprime[Y]*pprime[Y]);
			uvp->uv_u = acos(pprime[X]/len) * bn_inv2pi;
		}
		uvp->uv_v = -pprime[Z];
		break;
	case EPA_NORM_TOP:
		/* top plate, polar coords */
		len = sqrt(pprime[X]*pprime[X] + pprime[Y]*pprime[Y]);
		uvp->uv_u = acos(pprime[X]/len) * bn_inv2pi;
		uvp->uv_v = 1.0 - len;
		break;
	}
	/* Handle other half of acos() domain */
	if( pprime[Y] < 0 )
		uvp->uv_u = 1.0 - uvp->uv_u;

	/* uv_du should be relative to rotation, uv_dv relative to height */
	uvp->uv_du = uvp->uv_dv = 0;
}

/**
 *		R T _ E P A _ F R E E
 */
void
rt_epa_free(register struct soltab *stp)
{
	register struct epa_specific *epa =
		(struct epa_specific *)stp->st_specific;


	bu_free( (char *)epa, "epa_specific" );
}

/**
 *			R T _ E P A _ C L A S S
 */
int
rt_epa_class(void)
{
	return(0);
}

/**
 *			R T _ E P A _ P L O T
 */
int
rt_epa_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	fastf_t		dtol, f, mag_a, mag_h, ntol, r1, r2;
	fastf_t		**ellipses, theta_new, theta_prev, rt_ell_ang(fastf_t *p1, fastf_t a, fastf_t b, fastf_t dtol, fastf_t ntol);
	int		*pts_dbl, i, j, nseg;
	int		jj, na, nb, nell, recalc_b;
	LOCAL mat_t	R;
	LOCAL mat_t	invR;
	LOCAL struct rt_epa_internal	*xip;
	point_t		p1;
	struct rt_pt_node	*pos_a, *pos_b, *pts_a, *pts_b, *rt_ptalloc(void);
	vect_t		A, Au, B, Bu, Hu, V, Work;

#ifndef NO_MAGIC_CHECKING
	RT_CK_DB_INTERNAL(ip);
#endif

	xip = (struct rt_epa_internal *)ip->idb_ptr;
	RT_EPA_CK_MAGIC(xip);

	/*
	 *	make sure epa description is valid
	 */

	/* compute |A| |H| */
	mag_a = MAGSQ( xip->epa_Au );	/* should already be unit vector */
	mag_h = MAGNITUDE( xip->epa_H );
	r1 = xip->epa_r1;
	r2 = xip->epa_r2;
	/* Check for |H| > 0, |A| == 1, r1 > 0, r2 > 0 */
	if( NEAR_ZERO(mag_h, RT_LEN_TOL)
		|| !NEAR_ZERO(mag_a - 1.0, RT_LEN_TOL)
		|| r1 <= 0.0 || r2 <= 0.0 )  {
		return(-2);		/* BAD */
	}

	/* Check for A.H == 0 */
	f = VDOT( xip->epa_Au, xip->epa_H ) / mag_h;
	if( ! NEAR_ZERO(f, RT_DOT_TOL) )  {
		return(-2);		/* BAD */
	}

	/* make unit vectors in A, H, and BxH directions */
	VMOVE(    Hu, xip->epa_H );
	VUNITIZE( Hu );
	VMOVE(    Au, xip->epa_Au );
	VCROSS(   Bu, Au, Hu );

	/* Compute R and Rinv matrices */
	MAT_IDN( R );
	VREVERSE( &R[0], Bu );
	VMOVE(    &R[4], Au );
	VREVERSE( &R[8], Hu );
	bn_mat_trn( invR, R );			/* inv of rot mat is trn */

	/*
	 *  Establish tolerances
	 */
	if( ttol->rel <= 0.0 || ttol->rel >= 1.0 )
		dtol = 0.0;		/* none */
	else
		/* Convert rel to absolute by scaling by smallest side */
		dtol = ttol->rel * 2 * r2;
	if( ttol->abs <= 0.0 )  {
		if( dtol <= 0.0 )
			/* No tolerance given, use a default */
			dtol = 2 * 0.10 * r2;	/* 10% */
		else
			/* Use absolute-ized relative tolerance */
			;
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

	/*
	 *	build epa from 2 parabolas
	 */

	/* approximate positive half of parabola along semi-minor axis */
	pts_b = rt_ptalloc();
	pts_b->next = rt_ptalloc();
	pts_b->next->next = NULL;
	VSET( pts_b->p,       0, 0, -mag_h);
	VSET( pts_b->next->p, 0, r2, 0);
	/* 2 endpoints in 1st approximation */
	nb = 2;
	/* recursively break segment 'til within error tolerances */
	nb += rt_mk_parabola( pts_b, r2, mag_h, dtol, ntol );
	nell = nb - 1;	/* # of ellipses needed */

	/* construct positive half of parabola along semi-major axis
	 * of epa using same z coords as parab along semi-minor axis
	 */
	pts_a = rt_ptalloc();
	VMOVE(pts_a->p, pts_b->p);	/* 1st pt is the apex */
	pts_a->next = NULL;
	pos_b = pts_b->next;
	pos_a = pts_a;
	while (pos_b) {
		/* copy node from b_parabola to a_parabola */
		pos_a->next = rt_ptalloc();
		pos_a = pos_a->next;
		pos_a->p[Z] = pos_b->p[Z];
		/* at given z, find y on parabola */
		pos_a->p[Y] = r1*sqrt( pos_a->p[Z] / mag_h + 1 );
		pos_a->p[X] = 0;
		pos_b = pos_b->next;
	}
	pos_a->next = NULL;

	/* see if any segments need further breaking up */
	recalc_b = 0;
	na = 0;
	pos_a = pts_a;
	while (pos_a->next) {
		na = rt_mk_parabola( pos_a, r1, mag_h, dtol, ntol );
		if (na != 0) {
			recalc_b = 1;
			nell += na;
		}
		pos_a = pos_a->next;
	}
	/* if any were broken, recalculate parabola 'a' */
	if ( recalc_b ) {
		/* free mem for old approximation of parabola */
		pos_b = pts_b;
		while ( pos_b ) {
			struct rt_pt_node *next;

			/* get next node before freeing */
			next = pos_b->next;
			bu_free( (char *)pos_b, "rt_pt_node" );
			pos_b = next;
		}
		/* construct parabola along semi-major axis of epa
		 * using same z coords as parab along semi-minor axis
		 */
		pts_b = rt_ptalloc();
		pts_b->p[Z] = pts_a->p[Z];
		pts_b->next = NULL;
		pos_a = pts_a->next;
		pos_b = pts_b;
		while (pos_a) {
			/* copy node from a_parabola to b_parabola */
			pos_b->next = rt_ptalloc();
			pos_b = pos_b->next;
			pos_b->p[Z] = pos_a->p[Z];
			/* at given z, find y on parabola */
			pos_b->p[Y] = r2*sqrt( pos_b->p[Z] / mag_h + 1 );
			pos_b->p[X] = 0;
			pos_a = pos_a->next;
		}
		pos_b->next = NULL;
	}

	/* make array of ptrs to epa ellipses */
	ellipses = (fastf_t **)bu_malloc( nell * sizeof(fastf_t *), "fastf_t ell[]");
	/* keep track of whether pts in each ellipse are doubled or not */
	pts_dbl = (int *)bu_malloc( nell * sizeof(int), "dbl ints" );

	/* make ellipses at each z level */
	i = 0;
	nseg = 0;
	theta_prev = bn_twopi;
	pos_a = pts_a->next;	/* skip over apex of epa */
	pos_b = pts_b->next;
	while (pos_a) {
		VSCALE( A, Au, pos_a->p[Y] );	/* semimajor axis */
		VSCALE( B, Bu, pos_b->p[Y] );	/* semiminor axis */
		VJOIN1( V, xip->epa_V, -pos_a->p[Z], Hu );

		VSET( p1, 0., pos_b->p[Y], 0. );
		theta_new = rt_ell_ang(p1, pos_a->p[Y], pos_b->p[Y], dtol, ntol);
		if (nseg == 0) {
			nseg = (int)(bn_twopi / theta_new) + 1;
			pts_dbl[i] = 0;
		} else if (theta_new < theta_prev) {
			nseg *= 2;
			pts_dbl[i] = 1;
		} else
			pts_dbl[i] = 0;
		theta_prev = theta_new;

		ellipses[i] = (fastf_t *)bu_malloc(3*(nseg+1)*sizeof(fastf_t),
			"pts ell");
		rt_ell( ellipses[i], V, A, B, nseg );

		i++;
		pos_a = pos_a->next;
		pos_b = pos_b->next;
	}
	/* Draw the top ellipse */
	RT_ADD_VLIST( vhead,
		&ellipses[nell-1][(nseg-1)*ELEMENTS_PER_VECT],
		BN_VLIST_LINE_MOVE );
	for( i = 0; i < nseg; i++ )  {
		RT_ADD_VLIST( vhead,
			&ellipses[nell-1][i*ELEMENTS_PER_VECT],
			BN_VLIST_LINE_DRAW );
	}

	/* connect ellipses */
	for (i = nell-2; i >= 0; i--) {	/* skip top ellipse */
		int bottom, top;

		top = i + 1;
		bottom = i;
		if (pts_dbl[top])
			nseg /= 2;	/* # segs in 'bottom' ellipse */

		/* Draw the current ellipse */
		RT_ADD_VLIST( vhead,
			&ellipses[bottom][(nseg-1)*ELEMENTS_PER_VECT],
			BN_VLIST_LINE_MOVE );
		for( j = 0; j < nseg; j++ )  {
			RT_ADD_VLIST( vhead,
				&ellipses[bottom][j*ELEMENTS_PER_VECT],
				BN_VLIST_LINE_DRAW );
		}

		/* make connections between ellipses */
		for (j = 0; j < nseg; j++) {
			if (pts_dbl[top])
				jj = j + j;	/* top ellipse index */
			else
				jj = j;
			RT_ADD_VLIST( vhead,
				&ellipses[bottom][j*ELEMENTS_PER_VECT],
				BN_VLIST_LINE_MOVE );
			RT_ADD_VLIST( vhead,
				&ellipses[top][jj*ELEMENTS_PER_VECT],
				BN_VLIST_LINE_DRAW );
		}
	}

	VADD2( Work, xip->epa_V, xip->epa_H );
	for (i = 0; i < nseg; i++) {
		/* Draw connector */
		RT_ADD_VLIST( vhead, Work, BN_VLIST_LINE_MOVE );
		RT_ADD_VLIST( vhead,
			&ellipses[0][i*ELEMENTS_PER_VECT],
			BN_VLIST_LINE_DRAW );
	}

	/* free mem */
	for (i = 0; i < nell; i++) {
		bu_free( (char *)ellipses[i], "pts ell");
	}
	bu_free( (char *)ellipses, "fastf_t ell[]");
	bu_free( (char *)pts_dbl, "dbl ints" );

	return(0);
}

#define ELLOUT(n)	ov+(n-1)*3

void
rt_ell_norms(register fastf_t *ov, fastf_t *A, fastf_t *B, fastf_t *h_vec, fastf_t t, int sides)
{
	fastf_t	ang, theta, x, y, sqrt_1mt;
	int	n;
	vect_t partial_t, partial_ang;

	sqrt_1mt = sqrt( 1.0 - t );
	if( sqrt_1mt < SMALL_FASTF )
		rt_bomb( "rt_epa_tess: rt_ell_norms: sqrt( 1.0 -t ) is zero\n" );
	theta = 2 * bn_pi / sides;
	ang = 0.;

	for (n = 1; n <= sides; n++, ang += theta) {
		x = cos( ang );
		y = sin( ang );
		VJOIN2( partial_t, h_vec, -x/(2.0*sqrt_1mt), A, -y/(2.0*sqrt_1mt), B );
		VBLEND2( partial_ang, x*sqrt_1mt, B, -y*sqrt_1mt, A );
		VCROSS( ELLOUT(n), partial_t, partial_ang );
		VUNITIZE( ELLOUT(n) );
	}
	VMOVE(ELLOUT(n), ELLOUT(1));
}


/**
 *			R T _ E L L
 *
 *  Generate an ellipsoid with the specified number of sides approximating it.
 */
void
rt_ell(register fastf_t *ov, register const fastf_t *V, const fastf_t *A, const fastf_t *B, int sides)
{
	fastf_t	ang, theta, x, y;
	int	n;

	theta = 2 * bn_pi / sides;
	ang = 0.;
	/* make ellipse regardless of whether it meets req's */
	for (n = 1; n <= sides; n++, ang += theta) {
		x = cos( ang );
		y = sin( ang );
		VJOIN2( ELLOUT(n), V, x, A, y, B );
	}
	VMOVE(ELLOUT(n), ELLOUT(1));
}

/**
 *	R T _ E L L _ A N G
 *
 *	Return angle required for smallest side to fall within
 *	tolerances for ellipse.  Smallest side is a side with
 *	an endpoint at (a, 0, 0) where a is the semi-major axis.
 */
fastf_t
rt_ell_ang(fastf_t *p1, fastf_t a, fastf_t b, fastf_t dtol, fastf_t ntol)
{
	fastf_t	dist, intr, m, theta0, theta1;
	point_t	mpt, p0;
	vect_t	norm_line, norm_ell;

	VSET( p0, a, 0., 0. );
	/* slope and intercept of segment */
	m = ( p1[Y] - p0[Y] ) / ( p1[X] - p0[X] );
	intr = p0[Y] - m * p0[X];
	/* point on ellipse with max dist between ellipse and line */
	mpt[X] = a / sqrt( b*b / (m*m*a*a) + 1 );
	mpt[Y] = b * sqrt( 1 - mpt[X] * mpt[X] / (a*a) );
	mpt[Z] = 0;
	/* max distance between that point and line */
	dist = fabs( m * mpt[X] - mpt[Y] + intr ) / sqrt( m * m + 1 );
	/* angles between normal of line and of ellipse at line endpoints */
	VSET( norm_line, m, -1., 0.);
	VSET( norm_ell, b * b * p0[X], a * a * p0[Y], 0. );
	VUNITIZE( norm_line );
	VUNITIZE( norm_ell );
	theta0 = fabs( acos( VDOT( norm_line, norm_ell )));
	VSET( norm_ell, b * b * p1[X], a * a * p1[Y], 0. );
	VUNITIZE( norm_ell );
	theta1 = fabs( acos( VDOT( norm_line, norm_ell )));
	/* split segment at widest point if not within error tolerances */
	if ( dist > dtol || theta0 > ntol || theta1 > ntol ) {
		/* split segment */
		return( rt_ell_ang( mpt, a, b, dtol, ntol ) );
	} else
		return( acos( VDOT(p0, p1)
			/ ( MAGNITUDE(p0) * MAGNITUDE(p1) ) ));
}

/**
 *			R T _ E P A _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_epa_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	fastf_t		dtol, f, mag_a, mag_h, ntol, r1, r2;
	fastf_t		**ellipses, **normals, theta_new, theta_prev;
	int		*pts_dbl, face, i, j, nseg;
	int		*segs_per_ell;
	int		jj, na, nb, nell, recalc_b;
	LOCAL mat_t	R;
	LOCAL mat_t	invR;
	LOCAL struct rt_epa_internal	*xip;
	point_t		p1;
	struct rt_pt_node	*pos_a, *pos_b, *pts_a, *pts_b, *rt_ptalloc(void);
	struct shell	*s;
	struct faceuse	**outfaceuses = NULL;
	struct vertex	*vertp[3];
	struct vertex	***vells = (struct vertex ***)NULL;
	vect_t		A, Au, B, Bu, Hu, V;
	vect_t		apex_norm,rev_norm;
	vect_t		A_orig,B_orig;
	struct vertex	*apex_v;
	struct vertexuse *vu;
	struct faceuse *fu;

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_epa_internal *)ip->idb_ptr;
	RT_EPA_CK_MAGIC(xip);

	/*
	 *	make sure epa description is valid
	 */

	/* compute |A| |H| */
	mag_a = MAGSQ( xip->epa_Au );	/* should already be unit vector */
	mag_h = MAGNITUDE( xip->epa_H );
	r1 = xip->epa_r1;
	r2 = xip->epa_r2;
	/* Check for |H| > 0, |A| == 1, r1 > 0, r2 > 0 */
	if( NEAR_ZERO(mag_h, RT_LEN_TOL)
		|| !NEAR_ZERO(mag_a - 1.0, RT_LEN_TOL)
		|| r1 <= 0.0 || r2 <= 0.0 )  {
		return(-2);		/* BAD */
	}

	/* Check for A.H == 0 */
	f = VDOT( xip->epa_Au, xip->epa_H ) / mag_h;
	if( ! NEAR_ZERO(f, RT_DOT_TOL) )  {
		return(-2);		/* BAD */
	}

	/* make unit vectors in A, H, and BxH directions */
	VMOVE(    Hu, xip->epa_H );
	VUNITIZE( Hu );
	VMOVE(    Au, xip->epa_Au );
	VCROSS(   Bu, Au, Hu );

	VSCALE( A_orig , Au , xip->epa_r1 );
	VSCALE( B_orig , Bu , xip->epa_r2 );

	/* Compute R and Rinv matrices */
	MAT_IDN( R );
	VREVERSE( &R[0], Bu );
	VMOVE(    &R[4], Au );
	VREVERSE( &R[8], Hu );
	bn_mat_trn( invR, R );			/* inv of rot mat is trn */

	/*
	 *  Establish tolerances
	 */
	if( ttol->rel <= 0.0 || ttol->rel >= 1.0 )
		dtol = 0.0;		/* none */
	else
		/* Convert rel to absolute by scaling by smallest side */
		dtol = ttol->rel * 2 * r2;
	if( ttol->abs <= 0.0 )  {
		if( dtol <= 0.0 )
			/* No tolerance given, use a default */
			dtol = 2 * 0.10 * r2;	/* 10% */
		else
			/* Use absolute-ized relative tolerance */
			;
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

	/*
	 *	build epa from 2 parabolas
	 */

	/* approximate positive half of parabola along semi-minor axis */
	pts_b = rt_ptalloc();
	pts_b->next = rt_ptalloc();
	pts_b->next->next = NULL;
	VSET( pts_b->p,       0, 0, -mag_h);
	VSET( pts_b->next->p, 0, r2, 0);
	/* 2 endpoints in 1st approximation */
	nb = 2;
	/* recursively break segment 'til within error tolerances */
	nb += rt_mk_parabola( pts_b, r2, mag_h, dtol, ntol );
	nell = nb - 1;	/* # of ellipses needed */

	/* construct positive half of parabola along semi-major axis
	 * of epa using same z coords as parab along semi-minor axis
	 */
	pts_a = rt_ptalloc();
	VMOVE(pts_a->p, pts_b->p);	/* 1st pt is the apex */
	pts_a->next = NULL;
	pos_b = pts_b->next;
	pos_a = pts_a;
	while (pos_b) {
		/* copy node from b_parabola to a_parabola */
		pos_a->next = rt_ptalloc();
		pos_a = pos_a->next;
		pos_a->p[Z] = pos_b->p[Z];
		/* at given z, find y on parabola */
		pos_a->p[Y] = r1*sqrt( pos_a->p[Z] / mag_h + 1 );
		pos_a->p[X] = 0;
		pos_b = pos_b->next;
	}
	pos_a->next = NULL;

	/* see if any segments need further breaking up */
	recalc_b = 0;
	na = 0;
	pos_a = pts_a;
	while (pos_a->next) {
		na = rt_mk_parabola( pos_a, r1, mag_h, dtol, ntol );
		if (na != 0) {
			recalc_b = 1;
			nell += na;
		}
		pos_a = pos_a->next;
	}
	/* if any were broken, recalculate parabola 'a' */
	if ( recalc_b ) {
		/* free mem for old approximation of parabola */
		pos_b = pts_b;
		while ( pos_b ) {
			struct rt_pt_node *tmp;

			tmp = pos_b->next;
			bu_free( (char *)pos_b, "rt_pt_node" );
			pos_b = tmp;
		}
		/* construct parabola along semi-major axis of epa
		 * using same z coords as parab along semi-minor axis
		 */
		pts_b = rt_ptalloc();
		pts_b->p[Z] = pts_a->p[Z];
		pts_b->next = NULL;
		pos_a = pts_a->next;
		pos_b = pts_b;
		while (pos_a) {
			/* copy node from a_parabola to b_parabola */
			pos_b->next = rt_ptalloc();
			pos_b = pos_b->next;
			pos_b->p[Z] = pos_a->p[Z];
			/* at given z, find y on parabola */
			pos_b->p[Y] = r2*sqrt( pos_b->p[Z] / mag_h + 1 );
			pos_b->p[X] = 0;
			pos_a = pos_a->next;
		}
		pos_b->next = NULL;
	}

	/* make array of ptrs to epa ellipses */
	ellipses = (fastf_t **)bu_malloc( nell * sizeof(fastf_t *), "fastf_t ell[]");
	/* keep track of whether pts in each ellipse are doubled or not */
	pts_dbl = (int *)bu_malloc( nell * sizeof(int), "dbl ints" );
	/* I don't understand this pts_dbl, so here is an array containing the length of
	 * each ellipses array
	 */
	segs_per_ell = (int *)bu_calloc( nell , sizeof( int ) , "rt_epa_tess: segs_per_ell" );

	/* and an array of normals */
	normals = (fastf_t **)bu_malloc( nell * sizeof(fastf_t *), "fastf_t normals[]");

	/* make ellipses at each z level */
	i = 0;
	nseg = 0;
	theta_prev = bn_twopi;
	pos_a = pts_a->next;	/* skip over apex of epa */
	pos_b = pts_b->next;
	while (pos_a) {
		fastf_t t;

		t = (-pos_a->p[Z] / mag_h);
		VSCALE( A, Au, pos_a->p[Y] );	/* semimajor axis */
		VSCALE( B, Bu, pos_b->p[Y] );	/* semiminor axis */
		VJOIN1( V, xip->epa_V, -pos_a->p[Z], Hu );

		VSET( p1, 0., pos_b->p[Y], 0. );
		theta_new = rt_ell_ang(p1, pos_a->p[Y], pos_b->p[Y], dtol, ntol);
		if (nseg == 0) {
			nseg = (int)(bn_twopi / theta_new) + 1;
			pts_dbl[i] = 0;
			/* maximum number of faces needed for epa */
			face = nseg*(1 + 3*((1 << (nell-1)) - 1));
			/* array for each triangular face */
			outfaceuses = (struct faceuse **)
			bu_malloc( (face+1) * sizeof(struct faceuse *), "faceuse []" );
		} else if (theta_new < theta_prev) {
			nseg *= 2;
			pts_dbl[i] = 1;
		} else {
			pts_dbl[i] = 0;
		}
		theta_prev = theta_new;

		ellipses[i] = (fastf_t *)bu_malloc(3*(nseg+1)*sizeof(fastf_t),
			"pts ell");
		segs_per_ell[i] = nseg;
		normals[i] = (fastf_t *)bu_malloc(3*(nseg+1)*sizeof(fastf_t), "rt_epa_tess_ normals" );
		rt_ell( ellipses[i], V, A, B, nseg );
		rt_ell_norms( normals[i], A_orig, B_orig, xip->epa_H, t, nseg );

		i++;
		pos_a = pos_a->next;
		pos_b = pos_b->next;
	}

	/*
	 *	put epa geometry into nmg data structures
	 */

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = BU_LIST_FIRST(shell, &(*r)->s_hd);

	/* vertices of ellipses of epa */
	vells = (struct vertex ***)
		bu_malloc(nell*sizeof(struct vertex **), "vertex [][]");
	j = nseg;
	for (i = nell-1; i >= 0; i--) {
	        vells[i] = (struct vertex **)
	        	bu_malloc(j*sizeof(struct vertex *), "vertex []");
		if (i && pts_dbl[i])
			j /= 2;
	}

	/* top face of epa */
	for (i = 0; i < nseg; i++)
		vells[nell-1][i] = (struct vertex *)0;
	face = 0;
	if ( (outfaceuses[face++] = nmg_cface(s, vells[nell-1], nseg)) == 0) {
		bu_log("rt_epa_tess() failure, top face\n");
		goto fail;
	}
	for (i = 0; i < nseg; i++) {
		NMG_CK_VERTEX( vells[nell-1][i] );
		nmg_vertex_gv( vells[nell-1][i], &ellipses[nell-1][3*i] );
	}

	/* Mark the edges of this face as real, this is the only real edge */
	(void)nmg_mark_edges_real( &outfaceuses[0]->l.magic );

	/* connect ellipses with triangles */
	for (i = nell-2; i >= 0; i--) {	/* skip top ellipse */
		int bottom, top;

		top = i + 1;
		bottom = i;
		if (pts_dbl[top])
			nseg /= 2;	/* # segs in 'bottom' ellipse */
		vertp[0] = (struct vertex *)0;

		/* make triangular faces */
		for (j = 0; j < nseg; j++) {
			jj = j + j;	/* top ellipse index */

			if (pts_dbl[top]) {
				/* first triangle */
			        vertp[1] = vells[top][jj+1];
			        vertp[2] = vells[top][jj];
				if ( (outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
					bu_log("rt_epa_tess() failure\n");
					goto fail;
				}
				if (j == 0)
				        vells[bottom][j] = vertp[0];

				/* second triangle */
			        vertp[2] = vertp[1];
				if (j == nseg-1)
					vertp[1] = vells[bottom][0];
				else
					vertp[1] = (struct vertex *)0;
				if ( (outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
					bu_log("rt_epa_tess() failure\n");
					goto fail;
				}
				if (j != nseg-1)
					vells[bottom][j+1] = vertp[1];

				/* third triangle */
				vertp[0] = vertp[1];
				if (j == nseg-1)
					vertp[1] = vells[top][0];
				else
					vertp[1] = vells[top][jj+2];
				if ( (outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
					bu_log("rt_epa_tess() failure\n");
					goto fail;
				}
			} else {
				/* first triangle */
				if (j == nseg-1)
					vertp[1] = vells[top][0];
				else
				        vertp[1] = vells[top][j+1];
			        vertp[2] = vells[top][j];
				if ( (outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
					bu_log("rt_epa_tess() failure\n");
					goto fail;
				}
				if (j == 0)
				        vells[bottom][j] = vertp[0];

				/* second triangle */
			        vertp[2] = vertp[0];
				if (j == nseg-1)
					vertp[0] = vells[bottom][0];
				else
					vertp[0] = (struct vertex *)0;
				if ( (outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
					bu_log("rt_epa_tess() failure\n");
					goto fail;
				}
				if (j != nseg-1)
					vells[bottom][j+1] = vertp[0];
			}
		}

		/* associate geometry with each vertex */
		for (j = 0; j < nseg; j++) {
			NMG_CK_VERTEX( vells[bottom][j] );
			nmg_vertex_gv( vells[bottom][j],
				&ellipses[bottom][3*j] );
		}
	}

	/* connect bottom of ellipse to apex of epa */
	VADD2(V, xip->epa_V, xip->epa_H);
        vertp[0] = (struct vertex *)0;
	vertp[1] = vells[0][1];
	vertp[2] = vells[0][0];
	if ( (outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
		bu_log("rt_epa_tess() failure\n");
		goto fail;
	}
	/* associate geometry with topology */
	NMG_CK_VERTEX(vertp[0]);
	nmg_vertex_gv( vertp[0], (fastf_t *)V );
	apex_v = vertp[0];
	/* create rest of faces around apex */
	for (i = 1; i < nseg; i++) {
		vertp[2] = vertp[1];
		if (i == nseg-1)
			vertp[1] = vells[0][0];
		else
			vertp[1] = vells[0][i+1];
		if ( (outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
			bu_log("rt_epa_tess() failure\n");
			goto fail;
		}
	}

	/* Associate the face geometry */
	for (i=0 ; i < face ; i++) {
		if( nmg_fu_planeeqn( outfaceuses[i], tol ) < 0 )
			goto fail;
	}

	/* Associate vertexuse normals */
	for( i=0 ; i<nell ; i++ )
	{
		for( j=0 ; j<segs_per_ell[i] ; j++ )
		{
			VREVERSE( rev_norm , &normals[i][j*3] );
			for( BU_LIST_FOR( vu , vertexuse , &vells[i][j]->vu_hd ) )
			{

				fu = nmg_find_fu_of_vu( vu );
				NMG_CK_FACEUSE( fu );

				if( fu == outfaceuses[0] || fu->fumate_p == outfaceuses[0] )
					continue;	/* don't assign normals to top faceuse (flat) */

				if( fu->orientation == OT_SAME )
					nmg_vertexuse_nv( vu , &normals[i][j*3] );
				else if( fu->orientation == OT_OPPOSITE )
					nmg_vertexuse_nv( vu , rev_norm );
			}
		}
	}
	/* and don't forget the apex */
	VMOVE( apex_norm , xip->epa_H );
	VUNITIZE( apex_norm );
	VREVERSE( rev_norm , apex_norm );
	for( BU_LIST_FOR( vu , vertexuse , &apex_v->vu_hd ) )
	{
		NMG_CK_VERTEXUSE( vu );
		fu = nmg_find_fu_of_vu( vu );
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
			nmg_vertexuse_nv( vu , apex_norm );
		else if( fu->orientation == OT_OPPOSITE )
			nmg_vertexuse_nv( vu , rev_norm );
	}

	/* Glue the edges of different outward pointing face uses together */
	nmg_gluefaces( outfaceuses, face, tol );

	/* Compute "geometry" for region and shell */
	nmg_region_a( *r, tol );

	/* XXX just for testing, to make up for loads of triangles ... */
	nmg_shell_coplanar_face_merge( s, tol, 1 );

	/* free mem */
	bu_free( (char *)outfaceuses, "faceuse []");
	for (i = 0; i < nell; i++) {
		bu_free( (char *)ellipses[i], "pts ell");
	        bu_free( (char *)vells[i], "vertex []");
	}
	bu_free( (char *)ellipses, "fastf_t ell[]");
	bu_free( (char *)pts_dbl, "dbl ints" );
	bu_free( (char *)vells, "vertex [][]");

	return(0);

fail:
	/* free mem */
	bu_free( (char *)outfaceuses, "faceuse []");
	for (i = 0; i < nell; i++) {
		bu_free( (char *)ellipses[i], "pts ell");
	        bu_free( (char *)vells[i], "vertex []");
	}
	bu_free( (char *)ellipses, "fastf_t ell[]");
	bu_free( (char *)pts_dbl, "dbl ints" );
	bu_free( (char *)vells, "vertex [][]");

	return(-1);
}

/**
 *			R T _ E P A _ I M P O R T
 *
 *  Import an EPA from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_epa_import(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	LOCAL struct rt_epa_internal	*xip;
	union record			*rp;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != ID_SOLID )  {
		bu_log("rt_epa_import: defective record\n");
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_EPA;
	ip->idb_meth = &rt_functab[ID_EPA];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_epa_internal), "rt_epa_internal");
	xip = (struct rt_epa_internal *)ip->idb_ptr;
	xip->epa_magic = RT_EPA_INTERNAL_MAGIC;

	/* Warning:  type conversion */
	MAT4X3PNT( xip->epa_V, mat, &rp->s.s_values[0*3] );
	MAT4X3VEC( xip->epa_H, mat, &rp->s.s_values[1*3] );
	MAT4X3VEC( xip->epa_Au, mat, &rp->s.s_values[2*3] );
	VUNITIZE( xip->epa_Au );
	xip->epa_r1 = rp->s.s_values[3*3] / mat[15];
	xip->epa_r2 = rp->s.s_values[3*3+1] / mat[15];

	if( xip->epa_r1 < SMALL_FASTF || xip->epa_r2 < SMALL_FASTF )
	{
		bu_log( "rt_epa_import: r1 or r2 are zero\n" );
		bu_free( (char *)ip->idb_ptr , "rt_epa_import: ip->idb_ptr" );
		return( -1 );
	}

	return(0);			/* OK */
}

/**
 *			R T _ E P A _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_epa_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_epa_internal	*xip;
	union record		*epa;
	fastf_t			mag_h;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_EPA )  return(-1);
	xip = (struct rt_epa_internal *)ip->idb_ptr;
	RT_EPA_CK_MAGIC(xip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "epa external");
	epa = (union record *)ep->ext_buf;

	epa->s.s_id = ID_SOLID;
	epa->s.s_type = EPA;

	if (!NEAR_ZERO( MAGNITUDE(xip->epa_Au) - 1., RT_LEN_TOL)) {
		bu_log("rt_epa_export: Au not a unit vector!\n");
		return(-1);
	}

	mag_h = MAGNITUDE(xip->epa_H);

	if ( mag_h < RT_LEN_TOL
		|| xip->epa_r1 < RT_LEN_TOL
		|| xip->epa_r2 < RT_LEN_TOL) {
		bu_log("rt_epa_export: not all dimensions positive!\n");
		return(-1);
	}

	if ( !NEAR_ZERO( VDOT(xip->epa_Au, xip->epa_H)/mag_h, RT_DOT_TOL) ) {
		bu_log("rt_epa_export: Au and H are not perpendicular!\n");
		return(-1);
	}

	if (xip->epa_r2 > xip->epa_r1) {
		bu_log("rt_epa_export: semi-minor axis cannot be longer than semi-major axis!\n");
		return(-1);
	}

	/* Warning:  type conversion */
	VSCALE( &epa->s.s_values[0*3], xip->epa_V, local2mm );
	VSCALE( &epa->s.s_values[1*3], xip->epa_H, local2mm );
	VMOVE( &epa->s.s_values[2*3], xip->epa_Au ); /* don't scale a unit vector */
	epa->s.s_values[3*3] = xip->epa_r1 * local2mm;
	epa->s.s_values[3*3+1] = xip->epa_r2 * local2mm;

	return(0);
}

/**
 *			R T _ E P A _ I M P O R T 5
 *
 *  Import an EPA from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_epa_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	LOCAL struct rt_epa_internal	*xip;
	fastf_t				vec[11];

	BU_CK_EXTERNAL( ep );

	BU_ASSERT_LONG( ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 11 );

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_EPA;
	ip->idb_meth = &rt_functab[ID_EPA];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_epa_internal), "rt_epa_internal");

	xip = (struct rt_epa_internal *)ip->idb_ptr;
	xip->epa_magic = RT_EPA_INTERNAL_MAGIC;

	/* Convert from database (network) to internal (host) format */
	ntohd( (unsigned char *)vec, ep->ext_buf, 11 );

	/* Apply modeling transformations */
	MAT4X3PNT( xip->epa_V, mat, &vec[0*3] );
	MAT4X3VEC( xip->epa_H, mat, &vec[1*3] );
	MAT4X3VEC( xip->epa_Au, mat, &vec[2*3] );
	VUNITIZE( xip->epa_Au );
	xip->epa_r1 = vec[3*3] / mat[15];
	xip->epa_r2 = vec[3*3+1] / mat[15];

	if( xip->epa_r1 < SMALL_FASTF || xip->epa_r2 < SMALL_FASTF )
	{
		bu_log( "rt_epa_import: r1 or r2 are zero\n" );
		bu_free( (char *)ip->idb_ptr , "rt_epa_import: ip->idb_ptr" );
		return( -1 );
	}

	return(0);			/* OK */
}

/**
 *			R T _ E P A _ E X P O R T 5
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_epa_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_epa_internal	*xip;
	fastf_t			vec[11];
	fastf_t			mag_h;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_EPA )  return(-1);
	xip = (struct rt_epa_internal *)ip->idb_ptr;
	RT_EPA_CK_MAGIC(xip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * 11;
	ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes, "epa external");

	if (!NEAR_ZERO( MAGNITUDE(xip->epa_Au) - 1., RT_LEN_TOL)) {
		bu_log("rt_epa_export: Au not a unit vector!\n");
		return(-1);
	}

	mag_h = MAGNITUDE(xip->epa_H);

	if ( mag_h < RT_LEN_TOL
		|| xip->epa_r1 < RT_LEN_TOL
		|| xip->epa_r2 < RT_LEN_TOL) {
		bu_log("rt_epa_export: not all dimensions positive!\n");
		return(-1);
	}

	if ( !NEAR_ZERO( VDOT(xip->epa_Au, xip->epa_H)/mag_h, RT_DOT_TOL) ) {
		bu_log("rt_epa_export: Au and H are not perpendicular!\n");
		return(-1);
	}

	if (xip->epa_r2 > xip->epa_r1) {
		bu_log("rt_epa_export: semi-minor axis cannot be longer than semi-major axis!\n");
		return(-1);
	}

	/* scale 'em into local buffer */
	VSCALE( &vec[0*3], xip->epa_V, local2mm );
	VSCALE( &vec[1*3], xip->epa_H, local2mm );
	VMOVE( &vec[2*3], xip->epa_Au ); /* don't scale a unit vector */
	vec[3*3] = xip->epa_r1 * local2mm;
	vec[3*3+1] = xip->epa_r2 * local2mm;

	/* Convert from internal (host) to database (network) format */
	htond( ep->ext_buf, (unsigned char *)vec, 11 );

	return(0);
}

/**
 *			R T _ E P A _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_epa_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	register struct rt_epa_internal	*xip =
		(struct rt_epa_internal *)ip->idb_ptr;
	char	buf[256];

	RT_EPA_CK_MAGIC(xip);
	bu_vls_strcat( str, "Elliptical Paraboloid (EPA)\n");

	sprintf(buf, "\tV (%g, %g, %g)\n",
		xip->epa_V[X] * mm2local,
		xip->epa_V[Y] * mm2local,
		xip->epa_V[Z] * mm2local );
	bu_vls_strcat( str, buf );

	sprintf(buf, "\tH (%g, %g, %g) mag=%g\n",
		xip->epa_H[X] * mm2local,
		xip->epa_H[Y] * mm2local,
		xip->epa_H[Z] * mm2local,
		MAGNITUDE(xip->epa_H) * mm2local);
	bu_vls_strcat( str, buf );

	sprintf(buf, "\tA=%g\n", xip->epa_r1 * mm2local);
	bu_vls_strcat( str, buf );

	sprintf(buf, "\tB=%g\n", xip->epa_r2 * mm2local);
	bu_vls_strcat( str, buf );

	return(0);
}

/**
 *			R T _ E P A _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_epa_ifree(struct rt_db_internal *ip)
{
	register struct rt_epa_internal	*xip;

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_epa_internal *)ip->idb_ptr;
	RT_EPA_CK_MAGIC(xip);
	xip->epa_magic = 0;		/* sanity */

	bu_free( (char *)xip, "epa ifree" );
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
