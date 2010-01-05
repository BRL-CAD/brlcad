/*                           R H C . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup primitives */
/** @{ */
/** @file rhc.c
 *
 * Intersect a ray with a Right Hyperbolic Cylinder.
 *
 * Algorithm -
 *
 * Given V, H, R, and B, there is a set of points on this rhc
 *
 * { (x, y, z) | (x, y, z) is on rhc }
 *
 * Through a series of Affine Transformations, this set of points will
 * be transformed into a set of points on an rhc located at the origin
 * with a rectangular halfwidth R of 1 along the Y axis, a height H of
 * +1 along the -X axis, a distance B of 1 along the -Z axis between
 * the vertex V and the tip of the hyperbola, and a distance c between
 * the tip of the hyperbola and the vertex of the asymptotic cone.
 *
 *
 * { (x', y', z') | (x', y', z') is on rhc at origin }
 *
 * The transformation from X to X' is accomplished by:
 *
 * X' = S(R(X - V))
 *
 * where R(X) = (H/(-|H|))
 * 		(R/(|R|)) . X
 * 		(B/(-|B|))
 *
 * and S(X) =	(1/|H|   0     0)
 * 		(0    1/|R|   0) . X
 * 		(0      0   1/|B|)
 *
 * To find the intersection of a line with the surface of the rhc,
 * consider the parametric line L:
 *
 *  	L : { P(n) | P + t(n) . D }
 *
 * Call W the actual point of intersection between L and the rhc.
 * Let W' be the point of intersection between L' and the unit rhc.
 *
 *  	L' : { P'(n) | P' + t(n) . D' }
 *
 * W = invR(invS(W')) + V
 *
 * Where W' = k D' + P'.
 *
 * If Dy' and Dz' are both 0, then there is no hit on the rhc; but the
 * end plates need checking.  If there is now only 1 hit point, the
 * top plate needs to be checked as well.
 *
 * Line L' hits the infinitely long canonical rhc at W' when
 *
 *	A * k**2 + B * k + C = 0
 *
 * where
 *
 * A = Dz'**2 - Dy'**2 * (1 + 2*c')
 * B = 2 * ((1 + c' + Pz') * Dz' - (1 + 2*c') * Dy' * Py'
 * C = (Pz' + c' + 1)**2 - (1 + 2*c') * Py'**2 - c'**2
 * b = |Breadth| = 1.0
 * h = |Height| = 1.0
 * r = 1.0
 * c' = c / |Breadth|
 *
 * The quadratic formula yields k (which is constant):
 *
 * k = [ -B +/- sqrt(B**2 - 4 * A * C)] / (2 * A)
 *
 * Now, D' = S(R(D))
 * and  P' = S(R(P - V))
 *
 * Substituting,
 *
 * W = V + invR(invS[ k *(S(R(D))) + S(R(P - V)) ])
 *   = V + invR((k * R(D)) + R(P - V))
 *   = V + k * D + P - V
 *   = k * D + P
 *
 * Note that ``k'' is constant, and is the same in the formulations
 * for both W and W'.
 *
 * The hit at ``k'' is a hit on the canonical rhc IFF
 * -1 <= Wx' <= 0 and -1 <= Wz' <= 0.
 *
 * NORMALS.  Given the point W on the surface of the rhc, what is the
 * vector normal to the tangent plane at that point?
 *
 * Map W onto the unit rhc, ie:  W' = S(R(W - V)).
 *
 * Plane on unit rhc at W' has a normal vector N' where
 *
 * N' = <0, Wy'*(1 + 2*c), -z-c-1>.
 *
 * The plane transforms back to the tangent plane at W, and this new
 * plane (on the original rhc) has a normal vector of N, viz:
 *
 * N = inverse[ transpose(inverse[ S o R ]) ] (N')
 *
 * because if H is perpendicular to plane Q, and matrix M maps from
 * Q to Q', then inverse[ transpose(M) ] (H) is perpendicular to Q'.
 * Here, H and Q are in "prime space" with the unit sphere.
 * [Somehow, the notation here is backwards].
 * So, the mapping matrix M = inverse(S o R), because
 * S o R maps from normal space to the unit sphere.
 *
 * N = inverse[ transpose(inverse[ S o R ]) ] (N')
 *   = inverse[ transpose(invR o invS) ] (N')
 *   = inverse[ transpose(invS) o transpose(invR) ] (N')
 *   = inverse[ inverse(S) o R ] (N')
 *   = invR o S (N')
 *
 * because inverse(R) = transpose(R), so R = transpose(invR),
 * and S = transpose(S).
 *
 * Note that the normal vector produced above will not have unit
 * length.
 *
 * THE TOP AND END PLATES.
 *
 * If Dz' == 0, line L' is parallel to the top plate, so there is no
 * hit on the top plate.  Otherwise, rays intersect the top plate
 * with k = (0 - Pz')/Dz'.  The solution is within the top plate
 * IFF -1 <= Wx' <= 0 and -1 <= Wy' <= 1.
 *
 * If Dx' == 0, line L' is parallel to the end plates, so there is no
 * hit on the end plates.  Otherwise, rays intersect the front plate
 * line L' hits the front plate with k = (0 - Px') / Dx', and and hits
 * the back plate with k = (-1 - Px') / Dx'.
 *
 * The solution W' is within an end plate IFF
 *
 *	(Wz' + c + 1)**2 - (Wy'**2 * (2*c + 1) >= c**2 and Wz' <= 1.0
 *
 * The normal for a hit on the top plate is -Bunit.
 * The normal for a hit on the front plate is -Hunit, and
 * the normal for a hit on the back plate is +Hunit.
 *
 * Authors -
 * Michael J. Markowski
 *
 */
/** @} */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"


struct rhc_specific {
    point_t rhc_V;		/* vector to rhc origin */
    vect_t rhc_Bunit;	/* unit B vector */
    vect_t rhc_Hunit;	/* unit H vector */
    vect_t rhc_Runit;	/* unit vector, B x H */
    mat_t rhc_SoR;	/* Scale(Rot(vect)) */
    mat_t rhc_invRoS;	/* invRot(Scale(vect)) */
    fastf_t rhc_b;		/* |B| */
    fastf_t rhc_c;		/* c */
    fastf_t rhc_cprime;	/* c / |B| */
    fastf_t rhc_rsq;	/* r * r */
};


const struct bu_structparse rt_rhc_parse[] = {
    { "%f", 3, "V", bu_offsetof(struct rt_rhc_internal, rhc_V[X]), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "H", bu_offsetof(struct rt_rhc_internal, rhc_H[X]), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "B", bu_offsetof(struct rt_rhc_internal, rhc_B[X]), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "r", bu_offsetof(struct rt_rhc_internal, rhc_r),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "c", bu_offsetof(struct rt_rhc_internal, rhc_c),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/**
 * R T _ R H C _ P R E P
 *
 * Given a pointer to a GED database record, and a transformation matrix,
 * determine if this is a valid RHC, and if so, precompute various
 * terms of the formula.
 *
 * Returns -
 * 0 RHC is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct rhc_specific is created, and it's address is stored in
 * stp->st_specific for use by rhc_shot().
 */
int
rt_rhc_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_rhc_internal *xip;
    struct rhc_specific *rhc;
    fastf_t magsq_b, magsq_h, magsq_r;
    fastf_t mag_b, mag_h, mag_r;
    fastf_t f;
    mat_t R;
    mat_t Rinv;
    mat_t S;
    vect_t invsq;	/* [ 1/(|H|**2), 1/(|R|**2), 1/(|B|**2) ] */

    RT_CK_DB_INTERNAL(ip);
    RT_CK_RTI(rtip);
    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    RT_RHC_CK_MAGIC(xip);

    /* compute |B| |H| */
    mag_b = sqrt(magsq_b = MAGSQ(xip->rhc_B));
    mag_h = sqrt(magsq_h = MAGSQ(xip->rhc_H));
    mag_r = xip->rhc_r;
    magsq_r = mag_r * mag_r;

    /* Check for |H| > 0, |B| > 0, |R| > 0 */
    if (NEAR_ZERO(mag_h, RT_LEN_TOL) || NEAR_ZERO(mag_b, RT_LEN_TOL)
	|| NEAR_ZERO(mag_r, RT_LEN_TOL)) {
	return(1);		/* BAD, too small */
    }

    /* Check for B.H == 0 */
    f = VDOT(xip->rhc_B, xip->rhc_H) / (mag_b * mag_h);
    if (! NEAR_ZERO(f, RT_DOT_TOL)) {
	return(1);		/* BAD */
    }

    /*
     * RHC is ok
     */
    stp->st_id = ID_RHC;		/* set soltab ID */
    stp->st_meth = &rt_functab[ID_RHC];

    BU_GETSTRUCT(rhc, rhc_specific);
    stp->st_specific = (genptr_t)rhc;
    rhc->rhc_b = mag_b;
    rhc->rhc_rsq = magsq_r;
    rhc->rhc_c = xip->rhc_c;

    /* make unit vectors in B, H, and BxH directions */
    VMOVE(rhc->rhc_Hunit, xip->rhc_H);
    VUNITIZE(rhc->rhc_Hunit);
    VMOVE(rhc->rhc_Bunit, xip->rhc_B);
    VUNITIZE(rhc->rhc_Bunit);
    VCROSS(rhc->rhc_Runit, rhc->rhc_Bunit, rhc->rhc_Hunit);

    VMOVE(rhc->rhc_V, xip->rhc_V);
    rhc->rhc_cprime = xip->rhc_c / mag_b;

    /* Compute R and Rinv matrices */
    MAT_IDN(R);
    VREVERSE(&R[0], rhc->rhc_Hunit);
    VMOVE(&R[4], rhc->rhc_Runit);
    VREVERSE(&R[8], rhc->rhc_Bunit);
    bn_mat_trn(Rinv, R);			/* inv of rot mat is trn */

    /* Compute S */
    VSET(invsq, 1.0/magsq_h, 1.0/magsq_r, 1.0/magsq_b);
    MAT_IDN(S);
    S[ 0] = sqrt(invsq[0]);
    S[ 5] = sqrt(invsq[1]);
    S[10] = sqrt(invsq[2]);

    /* Compute SoR and invRoS */
    bn_mat_mul(rhc->rhc_SoR, S, R);
    bn_mat_mul(rhc->rhc_invRoS, Rinv, S);

    /* Compute bounding sphere and RPP */
    /* bounding sphere center */
    VJOIN2(stp->st_center,	rhc->rhc_V,
	   mag_h / 2.0,	rhc->rhc_Hunit,
	   mag_b / 2.0,	rhc->rhc_Bunit);
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
 * R T _ R H C _ P R I N T
 */
void
rt_rhc_print(const struct soltab *stp)
{
    const struct rhc_specific *rhc =
	(struct rhc_specific *)stp->st_specific;

    VPRINT("V", rhc->rhc_V);
    VPRINT("Bunit", rhc->rhc_Bunit);
    VPRINT("Hunit", rhc->rhc_Hunit);
    VPRINT("Runit", rhc->rhc_Runit);
    bn_mat_print("S o R", rhc->rhc_SoR);
    bn_mat_print("invR o S", rhc->rhc_invRoS);
}


/* hit_surfno is set to one of these */
#define RHC_NORM_BODY (1)		/* compute normal */
#define RHC_NORM_TOP (2)		/* copy rhc_N */
#define RHC_NORM_FRT (3)		/* copy reverse rhc_N */
#define RHC_NORM_BACK (4)

/**
 * R T _ R H C _ S H O T
 *
 * Intersect a ray with a rhc.
 * If an intersection occurs, a struct seg will be acquired
 * and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_rhc_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct rhc_specific *rhc =
	(struct rhc_specific *)stp->st_specific;
    vect_t dprime;		/* D' */
    vect_t pprime;		/* P' */
    fastf_t k1, k2;		/* distance constants of solution */
    fastf_t x;
    vect_t xlated;		/* translated vector */
    struct hit hits[3];	/* 2 potential hit points */
    struct hit *hitp;	/* pointer to hit point */

    hitp = &hits[0];

    /* out, Mat, vect */
    MAT4X3VEC(dprime, rhc->rhc_SoR, rp->r_dir);
    VSUB2(xlated, rp->r_pt, rhc->rhc_V);
    MAT4X3VEC(pprime, rhc->rhc_SoR, xlated);

    x = rhc->rhc_cprime;

    if (NEAR_ZERO(dprime[Y], SMALL) && NEAR_ZERO(dprime[Z], SMALL))
	goto check_plates;

    /* Find roots of the equation, using formula for quadratic */
    {
	fastf_t a, b, c;	/* coeffs of polynomial */
	fastf_t disc;		/* disc of radical */

	a = dprime[Z] * dprime[Z] - dprime[Y] * dprime[Y] * (1 + 2*x);
	b = 2*((pprime[Z] + x + 1) * dprime[Z]
	       - (2*x + 1) * dprime[Y] * pprime[Y]);
	c = (pprime[Z]+x+1)*(pprime[Z]+x+1)
	    - (2*x + 1) * pprime[Y] * pprime[Y] - x*x;
	if (!NEAR_ZERO(a, RT_PCOEF_TOL)) {
	    disc = b*b - 4 * a * c;
	    if (disc <= 0)
		goto check_plates;
	    disc = sqrt(disc);

	    k1 = (-b + disc) / (2.0 * a);
	    k2 = (-b - disc) / (2.0 * a);

	    /*
	     * k1 and k2 are potential solutions to intersection with side.
	     * See if they fall in range.
	     */
	    VJOIN1(hitp->hit_vpriv, pprime, k1, dprime);		/* hit' */
	    if (hitp->hit_vpriv[X] >= -1.0
		&& hitp->hit_vpriv[X] <= 0.0
		&& hitp->hit_vpriv[Z] >= -1.0
		&& hitp->hit_vpriv[Z] <= 0.0) {
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = k1;
		hitp->hit_surfno = RHC_NORM_BODY;	/* compute N */
		hitp++;
	    }

	    VJOIN1(hitp->hit_vpriv, pprime, k2, dprime);		/* hit' */
	    if (hitp->hit_vpriv[X] >= -1.0
		&& hitp->hit_vpriv[X] <= 0.0
		&& hitp->hit_vpriv[Z] >= -1.0
		&& hitp->hit_vpriv[Z] <= 0.0) {
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = k2;
		hitp->hit_surfno = RHC_NORM_BODY;	/* compute N */
		hitp++;
	    }
	} else if (!NEAR_ZERO(b, RT_PCOEF_TOL)) {
	    k1 = -c/b;
	    VJOIN1(hitp->hit_vpriv, pprime, k1, dprime);		/* hit' */
	    if (hitp->hit_vpriv[X] >= -1.0
		&& hitp->hit_vpriv[X] <= 0.0
		&& hitp->hit_vpriv[Z] >= -1.0
		&& hitp->hit_vpriv[Z] <= 0.0) {
		hitp->hit_magic = RT_HIT_MAGIC;
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
    if (hitp < &hits[2]  &&  !NEAR_ZERO(dprime[X], SMALL)) {
	/* 0 or 1 hits so far, this is worthwhile */
	k1 = -pprime[X] / dprime[X];		/* front plate */
	k2 = (-1.0 - pprime[X]) / dprime[X];	/* back plate */

	VJOIN1(hitp->hit_vpriv, pprime, k1, dprime);	/* hit' */
	if ((hitp->hit_vpriv[Z] + x + 1.0)
	    * (hitp->hit_vpriv[Z] + x + 1.0)
	    - hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y]
	    * (1.0 + 2*x) >= x*x
	    && hitp->hit_vpriv[Z] >= -1.0
	    && hitp->hit_vpriv[Z] <= 0.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = RHC_NORM_FRT;	/* -H */
	    hitp++;
	}

	VJOIN1(hitp->hit_vpriv, pprime, k2, dprime);	/* hit' */
	if ((hitp->hit_vpriv[Z] + x + 1.0)
	    * (hitp->hit_vpriv[Z] + x + 1.0)
	    - hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y]
	    * (1.0 + 2*x) >= x*x
	    && hitp->hit_vpriv[Z] >= -1.0
	    && hitp->hit_vpriv[Z] <= 0.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k2;
	    hitp->hit_surfno = RHC_NORM_BACK;	/* +H */
	    hitp++;
	}
    }

    /* check top plate */
    if (hitp == &hits[1]  &&  !NEAR_ZERO(dprime[Z], SMALL)) {
	/* 0 or 1 hits so far, this is worthwhile */
	k1 = -pprime[Z] / dprime[Z];		/* top plate */

	VJOIN1(hitp->hit_vpriv, pprime, k1, dprime);	/* hit' */
	if (hitp->hit_vpriv[X] >= -1.0 &&  hitp->hit_vpriv[X] <= 0.0
	    && hitp->hit_vpriv[Y] >= -1.0
	    && hitp->hit_vpriv[Y] <= 1.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = RHC_NORM_TOP;	/* -B */
	    hitp++;
	}
    }

    if (hitp != &hits[2])
	return(0);	/* MISS */

    if (hits[0].hit_dist < hits[1].hit_dist) {
	/* entry is [0], exit is [1] */
	struct seg *segp;

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in = hits[0];		/* struct copy */
	segp->seg_out = hits[1];	/* struct copy */
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    } else {
	/* entry is [1], exit is [0] */
	struct seg *segp;

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in = hits[1];		/* struct copy */
	segp->seg_out = hits[0];	/* struct copy */
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }
    return(2);			/* HIT */
}


/**
 * R T _ R H C _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_rhc_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    fastf_t c;
    vect_t can_normal;	/* normal to canonical rhc */
    struct rhc_specific *rhc =
	(struct rhc_specific *)stp->st_specific;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    switch (hitp->hit_surfno) {
	case RHC_NORM_BODY:
	    c = rhc->rhc_cprime;
	    VSET(can_normal,
		 0.0,
		 hitp->hit_vpriv[Y] * (1.0 + 2.0*c),
		 -hitp->hit_vpriv[Z] - c - 1.0);
	    MAT4X3VEC(hitp->hit_normal, rhc->rhc_invRoS, can_normal);
	    VUNITIZE(hitp->hit_normal);
	    break;
	case RHC_NORM_TOP:
	    VREVERSE(hitp->hit_normal, rhc->rhc_Bunit);
	    break;
	case RHC_NORM_FRT:
	    VREVERSE(hitp->hit_normal, rhc->rhc_Hunit);
	    break;
	case RHC_NORM_BACK:
	    VMOVE(hitp->hit_normal, rhc->rhc_Hunit);
	    break;
	default:
	    bu_log("rt_rhc_norm: surfno=%d bad\n", hitp->hit_surfno);
	    break;
    }
}


/**
 * R T _ R H C _ C U R V E
 *
 * Return the curvature of the rhc.
 */
void
rt_rhc_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    fastf_t b, c, rsq, y;
    fastf_t zp1_sq, zp2;	/* 1st deriv sqr, 2nd deriv */
    struct rhc_specific *rhc =
	(struct rhc_specific *)stp->st_specific;

    switch (hitp->hit_surfno) {
	case RHC_NORM_BODY:
	    /* most nearly flat direction */
	    VMOVE(cvp->crv_pdir, rhc->rhc_Hunit);
	    cvp->crv_c1 = 0;
	    /* k = z'' / (1 + z'^2) ^ 3/2 */
	    b = rhc->rhc_b;
	    c = rhc->rhc_c;
	    y = hitp->hit_point[Y];
	    rsq = rhc->rhc_rsq;
	    zp1_sq = y * (b*b + 2*b*c) / rsq;
	    zp1_sq *= zp1_sq / (c*c + y*y*(b*b + 2*b*c)/rsq);
	    zp2 = c*c / (rsq*c*c + y*y*(b*b + 2*b*c));
	    cvp->crv_c2 = zp2 / pow((1 + zp1_sq), 1.5);
	    break;
	case RHC_NORM_BACK:
	case RHC_NORM_FRT:
	case RHC_NORM_TOP:
	    /* any tangent direction */
	    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
	    cvp->crv_c1 = cvp->crv_c2 = 0;
	    break;
    }
}


/**
 * R T _ R H C _ U V
 *
 * For a hit on the surface of an rhc, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 * u = azimuth
 * v = elevation
 */
void
rt_rhc_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    struct rhc_specific *rhc = (struct rhc_specific *)stp->st_specific;

    vect_t work;
    vect_t pprime;
    fastf_t len;

    if (ap) RT_CK_APPLICATION(ap);

    /*
     * hit_point is on surface;  project back to unit rhc,
     * creating a vector from vertex to hit point.
     */
    VSUB2(work, hitp->hit_point, rhc->rhc_V);
    MAT4X3VEC(pprime, rhc->rhc_SoR, work);

    switch (hitp->hit_surfno) {
	case RHC_NORM_BODY:
	    /* Skin.  x, y coordinates define rotation.  radius = 1 */
	    len = sqrt(pprime[Y]*pprime[Y] + pprime[Z]*pprime[Z]);
	    uvp->uv_u = acos(pprime[Y]/len) * bn_invpi;
	    uvp->uv_v = -pprime[X];		/* height */
	    break;
	case RHC_NORM_FRT:
	case RHC_NORM_BACK:
	    /* end plates - circular mapping, not seamless w/body, top */
	    len = sqrt(pprime[Y]*pprime[Y] + pprime[Z]*pprime[Z]);
	    uvp->uv_u = acos(pprime[Y]/len) * bn_invpi;
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


/**
 * R T _ R H C _ F R E E
 */
void
rt_rhc_free(struct soltab *stp)
{
    struct rhc_specific *rhc =
	(struct rhc_specific *)stp->st_specific;

    bu_free((char *)rhc, "rhc_specific");
}


/**
 * R T _ R H C _ C L A S S
 */
int
rt_rhc_class(void)
{
    return(0);
}


/**
 * R T _ R H C _ P L O T
 */
int
rt_rhc_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol __attribute__((unused)))
{
    int i, n;
    fastf_t b, c, *back, f, *front, h, rh;
    fastf_t dtol, ntol;
    vect_t Bu, Hu, Ru;
    mat_t R;
    mat_t invR;
    struct rt_rhc_internal *xip;
    struct rt_pt_node *old, *pos, *pts, *rt_ptalloc(void);

    RT_CK_DB_INTERNAL(ip);
    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    RT_RHC_CK_MAGIC(xip);

    /* compute |B| |H| */
    b = MAGNITUDE(xip->rhc_B);	/* breadth */
    rh = xip->rhc_r;		/* rectangular halfwidth */
    h = MAGNITUDE(xip->rhc_H);	/* height */
    c = xip->rhc_c;			/* dist to asympt origin */

    /* Check for |H| > 0, |B| > 0, rh > 0, c > 0 */
    if (NEAR_ZERO(h, RT_LEN_TOL) || NEAR_ZERO(b, RT_LEN_TOL)
	|| NEAR_ZERO(rh, RT_LEN_TOL) || NEAR_ZERO(c, RT_LEN_TOL)) {
	bu_log("rt_rhc_plot:  zero length H, B, c, or rh\n");
	return(-2);		/* BAD */
    }

    /* Check for B.H == 0 */
    f = VDOT(xip->rhc_B, xip->rhc_H) / (b * h);
    if (! NEAR_ZERO(f, RT_DOT_TOL)) {
	bu_log("rt_rhc_plot: B not perpendicular to H, f=%f\n", f);
	return(-3);		/* BAD */
    }

    /* make unit vectors in B, H, and BxH directions */
    VMOVE(Hu, xip->rhc_H);
    VUNITIZE(Hu);
    VMOVE(Bu, xip->rhc_B);
    VUNITIZE(Bu);
    VCROSS(Ru, Bu, Hu);

    /* Compute R and Rinv matrices */
    MAT_IDN(R);
    VREVERSE(&R[0], Hu);
    VMOVE(&R[4], Ru);
    VREVERSE(&R[8], Bu);
    bn_mat_trn(invR, R);			/* inv of rot mat is trn */

    /*
     * Establish tolerances
     */
    if (ttol->rel <= 0.0 || ttol->rel >= 1.0) {
	dtol = 0.0;		/* none */
    } else {
	/* Convert rel to absolute by scaling by smallest side */
	if (rh < b)
	    dtol = ttol->rel * 2 * rh;
	else
	    dtol = ttol->rel * 2 * b;
    }
    if (ttol->abs <= 0.0) {
	if (dtol <= 0.0) {
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
	if (ttol->rel <= 0.0 || dtol > ttol->abs)
	    dtol = ttol->abs;
    }

    /* To ensure normal tolerance, remain below this angle */
    if (ttol->norm > 0.0)
	ntol = ttol->norm;
    else
	/* tolerate everything */
	ntol = bn_pi;

    /* initial hyperbola approximation is a single segment */
    pts = rt_ptalloc();
    pts->next = rt_ptalloc();
    pts->next->next = NULL;
    VSET(pts->p,       0, -rh, 0);
    VSET(pts->next->p, 0,  rh, 0);
    /* 2 endpoints in 1st approximation */
    n = 2;
    /* recursively break segment 'til within error tolerances */
    n += rt_mk_hyperbola(pts, rh, b, c, dtol, ntol);

    /* get mem for arrays */
    front = (fastf_t *)bu_malloc(3*n * sizeof(fastf_t), "fast_t");
    back  = (fastf_t *)bu_malloc(3*n * sizeof(fastf_t), "fast_t");

    /* generate front & back plates in world coordinates */
    pos = pts;
    i = 0;
    while (pos) {
	/* rotate back to original position */
	MAT4X3VEC(&front[i], invR, pos->p);
	/* move to origin vertex origin */
	VADD2(&front[i], &front[i], xip->rhc_V);
	/* extrude front to create back plate */
	VADD2(&back[i], &front[i], xip->rhc_H);
	i += 3;
	old = pos;
	pos = pos->next;
	bu_free ((char *)old, "rt_pt_node");
    }

    /* Draw the front */
    RT_ADD_VLIST(vhead, &front[(n-1)*ELEMENTS_PER_VECT],
		 BN_VLIST_LINE_MOVE);
    for (i = 0; i < n; i++) {
	RT_ADD_VLIST(vhead, &front[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
    }

    /* Draw the back */
    RT_ADD_VLIST(vhead, &back[(n-1)*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);
    for (i = 0; i < n; i++) {
	RT_ADD_VLIST(vhead, &back[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
    }

    /* Draw connections */
    for (i = 0; i < n; i++) {
	RT_ADD_VLIST(vhead, &front[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);
	RT_ADD_VLIST(vhead, &back[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
    }

    /* free mem */
    bu_free((char *)front, "fastf_t");
    bu_free((char *)back, "fastf_t");

    return(0);
}


/**
 * R T _ M K _ H Y P E R B O L A
 */
/*
  r: rectangular halfwidth
  b: breadth
  c: distance to asymptote origin
*/
int
rt_mk_hyperbola(struct rt_pt_node *pts, fastf_t r, fastf_t b, fastf_t c, fastf_t dtol, fastf_t ntol)
{
    fastf_t A, B, C, discr, dist, intr, j, k, m, theta0, theta1, z0;
    int n;
    point_t mpt, p0, p1;
    vect_t norm_line, norm_hyperb;
    struct rt_pt_node *new, *rt_ptalloc(void);

#define RHC_TOL .0001
    /* endpoints of segment approximating hyperbola */
    VMOVE(p0, pts->p);
    VMOVE(p1, pts->next->p);
    /* slope and intercept of segment */
    m = (p1[Z] - p0[Z]) / (p1[Y] - p0[Y]);
    intr = p0[Z] - m * p0[Y];
    /* find point on hyperbola with max dist between hyperbola and line */
    j = b + c;
    k = 1 - m*m*r*r/(b*(b + 2*c));
    A = k;
    B = 2*j*k;
    C = j*j*k - c*c;
    discr = sqrt(B*B - 4*A*C);
    z0 = (-B + discr) / (2. * A);
    if (z0+RHC_TOL >= -b)	/* use top sheet of hyperboloid */
	mpt[Z] = z0;
    else
	mpt[Z] = (-B - discr) / (2. * A);
    if (NEAR_ZERO(mpt[Z], RHC_TOL))
	mpt[Z] = 0.;
    mpt[X] = 0;
    mpt[Y] = ((mpt[Z] + b + c) * (mpt[Z] + b + c) - c*c) / (b*(b + 2*c));
    if (NEAR_ZERO(mpt[Y], RHC_TOL))
	mpt[Y] = 0.;
    mpt[Y] = r * sqrt(mpt[Y]);
    if (p0[Y] < 0.)
	mpt[Y] = -mpt[Y];
    /* max distance between that point and line */
    dist = fabs(m * mpt[Y] - mpt[Z] + intr) / sqrt(m * m + 1);
    /* angles between normal of line and of hyperbola at line endpoints */
    VSET(norm_line, m, -1., 0.);
    VSET(norm_hyperb, 0., (2*c + 1) / (p0[Z] + c + 1), -1.);
    VUNITIZE(norm_line);
    VUNITIZE(norm_hyperb);
    theta0 = fabs(acos(VDOT(norm_line, norm_hyperb)));
    VSET(norm_hyperb, 0., (2*c + 1) / (p1[Z] + c + 1), -1.);
    VUNITIZE(norm_hyperb);
    theta1 = fabs(acos(VDOT(norm_line, norm_hyperb)));
    /* split segment at widest point if not within error tolerances */
    if (dist > dtol || theta0 > ntol || theta1 > ntol) {
	/* split segment */
	new = rt_ptalloc();
	VMOVE(new->p, mpt);
	new->next = pts->next;
	pts->next = new;
	/* keep track of number of pts added */
	n = 1;
	/* recurse on first new segment */
	n += rt_mk_hyperbola(pts, r, b, c, dtol, ntol);
	/* recurse on second new segment */
	n += rt_mk_hyperbola(new, r, b, c, dtol, ntol);
    } else
	n  = 0;
    return(n);
}


/**
 * R T _ R H C _ T E S S
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_rhc_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    int i, j, n;
    fastf_t b, c, *back, f, *front, h, rh;
    fastf_t dtol, ntol;
    vect_t Bu, Hu, Ru;
    mat_t R;
    mat_t invR;
    struct rt_rhc_internal *xip;
    struct rt_pt_node *old, *pos, *pts, *rt_ptalloc(void);
    struct shell *s;
    struct faceuse **outfaceuses;
    struct vertex **vfront, **vback, **vtemp, *vertlist[4];
    vect_t *norms;
    fastf_t bb_plus_2bc, b_plus_c, r_sq;
    int failure=0;

    NMG_CK_MODEL(m);
    BN_CK_TOL(tol);
    RT_CK_TESS_TOL(ttol);

    RT_CK_DB_INTERNAL(ip);
    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    RT_RHC_CK_MAGIC(xip);

    /* compute |B| |H| */
    b = MAGNITUDE(xip->rhc_B);	/* breadth */
    rh = xip->rhc_r;		/* rectangular halfwidth */
    h = MAGNITUDE(xip->rhc_H);	/* height */
    c = xip->rhc_c;			/* dist to asympt origin */

    /* Check for |H| > 0, |B| > 0, rh > 0, c > 0 */
    if (NEAR_ZERO(h, RT_LEN_TOL) || NEAR_ZERO(b, RT_LEN_TOL)
	|| NEAR_ZERO(rh, RT_LEN_TOL) || NEAR_ZERO(c, RT_LEN_TOL)) {
	bu_log("rt_rhc_tess:  zero length H, B, c, or rh\n");
	return(-2);		/* BAD */
    }

    /* Check for B.H == 0 */
    f = VDOT(xip->rhc_B, xip->rhc_H) / (b * h);
    if (! NEAR_ZERO(f, RT_DOT_TOL)) {
	bu_log("rt_rhc_tess: B not perpendicular to H, f=%f\n", f);
	return(-3);		/* BAD */
    }

    /* make unit vectors in B, H, and BxH directions */
    VMOVE(Hu, xip->rhc_H);
    VUNITIZE(Hu);
    VMOVE(Bu, xip->rhc_B);
    VUNITIZE(Bu);
    VCROSS(Ru, Bu, Hu);

    /* Compute R and Rinv matrices */
    MAT_IDN(R);
    VREVERSE(&R[0], Hu);
    VMOVE(&R[4], Ru);
    VREVERSE(&R[8], Bu);
    bn_mat_trn(invR, R);			/* inv of rot mat is trn */

    /*
     * Establish tolerances
     */
    if (ttol->rel <= 0.0 || ttol->rel >= 1.0) {
	dtol = 0.0;		/* none */
    } else {
	/* Convert rel to absolute by scaling by smallest side */
	if (rh < b)
	    dtol = ttol->rel * 2 * rh;
	else
	    dtol = ttol->rel * 2 * b;
    }
    if (ttol->abs <= 0.0) {
	if (dtol <= 0.0) {
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
	if (ttol->rel <= 0.0 || dtol > ttol->abs)
	    dtol = ttol->abs;
    }

    /* To ensure normal tolerance, remain below this angle */
    if (ttol->norm > 0.0)
	ntol = ttol->norm;
    else
	/* tolerate everything */
	ntol = bn_pi;

    /* initial hyperbola approximation is a single segment */
    pts = rt_ptalloc();
    pts->next = rt_ptalloc();
    pts->next->next = NULL;
    VSET(pts->p,       0, -rh, 0);
    VSET(pts->next->p, 0,  rh, 0);
    /* 2 endpoints in 1st approximation */
    n = 2;
    /* recursively break segment 'til within error tolerances */
    n += rt_mk_hyperbola(pts, rh, b, c, dtol, ntol);

    /* get mem for arrays */
    front = (fastf_t *)bu_malloc(3*n * sizeof(fastf_t), "fastf_t");
    back  = (fastf_t *)bu_malloc(3*n * sizeof(fastf_t), "fastf_t");
    norms = (vect_t *)bu_calloc(n, sizeof(vect_t), "rt_rhc_tess: norms");
    vfront = (struct vertex **)bu_malloc((n+1) * sizeof(struct vertex *), "vertex *");
    vback = (struct vertex **)bu_malloc((n+1) * sizeof(struct vertex *), "vertex *");
    vtemp = (struct vertex **)bu_malloc((n+1) * sizeof(struct vertex *), "vertex *");
    outfaceuses =
	(struct faceuse **)bu_malloc((n+2) * sizeof(struct faceuse *), "faceuse *");
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
	VSET(tmp_norm, 0.0, pos->p[Y]*bb_plus_2bc, (-r_sq*(pos->p[Z]+b_plus_c)));
	MAT4X3VEC(norms[j], invR, tmp_norm);
	VUNITIZE(norms[j]);
	/* rotate back to original position */
	MAT4X3VEC(&front[i], invR, pos->p);
	/* move to origin vertex origin */
	VADD2(&front[i], &front[i], xip->rhc_V);
	/* extrude front to create back plate */
	VADD2(&back[i], &front[i], xip->rhc_H);
	i += 3;
	j++;
	old = pos;
	pos = pos->next;
	bu_free ((char *)old, "rt_pt_node");
    }

    *r = nmg_mrsv(m);	/* Make region, empty shell, vertex */
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    for (i=0; i<n; i++) {
	vfront[i] = vtemp[i] = (struct vertex *)0;
    }

    /* Front face topology.  Verts are considered to go CCW */
    outfaceuses[0] = nmg_cface(s, vfront, n);

    (void)nmg_mark_edges_real(&outfaceuses[0]->l.magic);

    /* Back face topology.  Verts must go in opposite dir (CW) */
    outfaceuses[1] = nmg_cface(s, vtemp, n);
    for (i=0; i<n; i++) vback[i] = vtemp[n-1-i];

    (void)nmg_mark_edges_real(&outfaceuses[1]->l.magic);

    /* Duplicate [0] as [n] to handle loop end condition, below */
    vfront[n] = vfront[0];
    vback[n] = vback[0];

    /* Build topology for all the rectangular side faces (n of them)
     * connecting the front and back faces.
     * increasing indices go towards counter-clockwise (CCW).
     */
    for (i=0; i<n; i++) {
	vertlist[0] = vfront[i];	/* from top, */
	vertlist[1] = vback[i];		/* straight down, */
	vertlist[2] = vback[i+1];	/* to left, */
	vertlist[3] = vfront[i+1];	/* straight up. */
	outfaceuses[2+i] = nmg_cface(s, vertlist, 4);
    }

    (void)nmg_mark_edges_real(&outfaceuses[n+1]->l.magic);

    for (i=0; i<n; i++) {
	NMG_CK_VERTEX(vfront[i]);
	NMG_CK_VERTEX(vback[i]);
    }

    /* Associate the vertex geometry, CCW */
    for (i=0; i<n; i++) {
	nmg_vertex_gv(vfront[i], &front[3*(i)]);
    }
    for (i=0; i<n; i++) {
	nmg_vertex_gv(vback[i], &back[3*(i)]);
    }

    /* Associate the face geometry */
    for (i=0; i < n+2; i++) {
	if (nmg_fu_planeeqn(outfaceuses[i], tol) < 0) {
	    failure = (-1);
	    goto fail;
	}
    }

    /* Associate vertexuse normals */
    for (i=0; i<n; i++) {
	struct vertexuse *vu;
	struct faceuse *fu;
	vect_t rev_norm;

	VREVERSE(rev_norm, norms[i]);

	/* do "front" vertices */
	NMG_CK_VERTEX(vfront[i]);
	for (BU_LIST_FOR(vu, vertexuse, &vfront[i]->vu_hd)) {
	    NMG_CK_VERTEXUSE(vu);
	    fu = nmg_find_fu_of_vu(vu);
	    NMG_CK_FACEUSE(fu);
	    if (fu->f_p == outfaceuses[0]->f_p ||
		fu->f_p == outfaceuses[1]->f_p ||
		fu->f_p == outfaceuses[n+1]->f_p)
		continue;	/* skip flat faces */

	    if (fu->orientation == OT_SAME)
		nmg_vertexuse_nv(vu, norms[i]);
	    else if (fu->orientation == OT_OPPOSITE)
		nmg_vertexuse_nv(vu, rev_norm);
	}

	/* and "back" vertices */
	NMG_CK_VERTEX(vback[i]);
	for (BU_LIST_FOR(vu, vertexuse, &vback[i]->vu_hd)) {
	    NMG_CK_VERTEXUSE(vu);
	    fu = nmg_find_fu_of_vu(vu);
	    NMG_CK_FACEUSE(fu);
	    if (fu->f_p == outfaceuses[0]->f_p ||
		fu->f_p == outfaceuses[1]->f_p ||
		fu->f_p == outfaceuses[n+1]->f_p)
		continue;	/* skip flat faces */

	    if (fu->orientation == OT_SAME)
		nmg_vertexuse_nv(vu, norms[i]);
	    else if (fu->orientation == OT_OPPOSITE)
		nmg_vertexuse_nv(vu, rev_norm);
	}
    }

    /* Glue the edges of different outward pointing face uses together */
    nmg_gluefaces(outfaceuses, n+2, tol);

    /* Compute "geometry" for region and shell */
    nmg_region_a(*r, tol);

 fail:
    /* free mem */
    bu_free((char *)front, "fastf_t");
    bu_free((char *)back, "fastf_t");
    bu_free((char*)vfront, "vertex *");
    bu_free((char*)vback, "vertex *");
    bu_free((char*)vtemp, "vertex *");
    bu_free((char *)norms, "rt_rhc_tess: norms");
    bu_free((char*)outfaceuses, "faceuse *");

    return(failure);
}


/**
 * R T _ R H C _ I M P O R T
 *
 * Import an RHC from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_rhc_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_rhc_internal *xip;
    union record *rp;

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != ID_SOLID) {
	bu_log("rt_rhc_import4: defective record\n");
	return(-1);
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_RHC;
    ip->idb_meth = &rt_functab[ID_RHC];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_rhc_internal), "rt_rhc_internal");
    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    xip->rhc_magic = RT_RHC_INTERNAL_MAGIC;

    /* Warning:  type conversion */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(xip->rhc_V, mat, &rp->s.s_values[0*3]);
    MAT4X3VEC(xip->rhc_H, mat, &rp->s.s_values[1*3]);
    MAT4X3VEC(xip->rhc_B, mat, &rp->s.s_values[2*3]);
    xip->rhc_r = rp->s.s_values[3*3] / mat[15];
    xip->rhc_c = rp->s.s_values[3*3+1] / mat[15];

    if (xip->rhc_r <= SMALL_FASTF || xip->rhc_c <= SMALL_FASTF) {
	bu_log("rt_rhc_import4: r or c are zero\n");
	bu_free((char *)ip->idb_ptr, "rt_rhc_import4: ip->idb_ptr");
	return(-1);
    }

    return(0);			/* OK */
}


/**
 * R T _ R H C _ E X P O R T
 *
 * The name is added by the caller, in the usual place.
 */
int
rt_rhc_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_rhc_internal *xip;
    union record *rhc;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_RHC) return(-1);
    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    RT_RHC_CK_MAGIC(xip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record);
    ep->ext_buf = (genptr_t)bu_calloc(1, ep->ext_nbytes, "rhc external");
    rhc = (union record *)ep->ext_buf;

    rhc->s.s_id = ID_SOLID;
    rhc->s.s_type = RHC;

    if (MAGNITUDE(xip->rhc_B) < RT_LEN_TOL
	|| MAGNITUDE(xip->rhc_H) < RT_LEN_TOL
	|| xip->rhc_r < RT_LEN_TOL
	|| xip->rhc_c < RT_LEN_TOL) {
	bu_log("rt_rhc_export4: not all dimensions positive!\n");
	return(-1);
    }

    {
	vect_t ub, uh;

	VMOVE(ub, xip->rhc_B);
	VUNITIZE(ub);
	VMOVE(uh, xip->rhc_H);
	VUNITIZE(uh);
	if (!NEAR_ZERO(VDOT(ub, uh), RT_DOT_TOL)) {
	    bu_log("rt_rhc_export4: B and H are not perpendicular!\n");
	    return(-1);
	}
    }

    /* Warning:  type conversion */
    VSCALE(&rhc->s.s_values[0*3], xip->rhc_V, local2mm);
    VSCALE(&rhc->s.s_values[1*3], xip->rhc_H, local2mm);
    VSCALE(&rhc->s.s_values[2*3], xip->rhc_B, local2mm);
    rhc->s.s_values[3*3] = xip->rhc_r * local2mm;
    rhc->s.s_values[3*3+1] = xip->rhc_c * local2mm;

    return(0);
}


/**
 * R T _ R H C _ I M P O R T 5
 *
 * Import an RHC from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_rhc_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_rhc_internal *xip;
    fastf_t vec[11];

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    BU_ASSERT_LONG(ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 11);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_RHC;
    ip->idb_meth = &rt_functab[ID_RHC];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_rhc_internal), "rt_rhc_internal");

    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    xip->rhc_magic = RT_RHC_INTERNAL_MAGIC;

    /* Convert from database (network) to internal (host) format */
    ntohd((unsigned char *)vec, ep->ext_buf, 11);

    /* Apply modeling transformations */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(xip->rhc_V, mat, &vec[0*3]);
    MAT4X3VEC(xip->rhc_H, mat, &vec[1*3]);
    MAT4X3VEC(xip->rhc_B, mat, &vec[2*3]);
    xip->rhc_r = vec[3*3] / mat[15];
    xip->rhc_c = vec[3*3+1] / mat[15];

    if (xip->rhc_r <= SMALL_FASTF || xip->rhc_c <= SMALL_FASTF) {
	bu_log("rt_rhc_import4: r or c are zero\n");
	bu_free((char *)ip->idb_ptr, "rt_rhc_import4: ip->idb_ptr");
	return(-1);
    }

    return(0);			/* OK */
}


/**
 * R T _ R H C _ E X P O R T 5
 *
 * The name is added by the caller, in the usual place.
 */
int
rt_rhc_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_rhc_internal *xip;
    fastf_t vec[11];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_RHC) return(-1);
    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    RT_RHC_CK_MAGIC(xip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * 11;
    ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes, "rhc external");

    if (MAGNITUDE(xip->rhc_B) < RT_LEN_TOL
	|| MAGNITUDE(xip->rhc_H) < RT_LEN_TOL
	|| xip->rhc_r < RT_LEN_TOL
	|| xip->rhc_c < RT_LEN_TOL) {
	bu_log("rt_rhc_export4: not all dimensions positive!\n");
	return(-1);
    }

    {
	vect_t ub, uh;

	VMOVE(ub, xip->rhc_B);
	VUNITIZE(ub);
	VMOVE(uh, xip->rhc_H);
	VUNITIZE(uh);
	if (!NEAR_ZERO(VDOT(ub, uh), RT_DOT_TOL)) {
	    bu_log("rt_rhc_export4: B and H are not perpendicular!\n");
	    return(-1);
	}
    }

    /* scale 'em into local buffer */
    VSCALE(&vec[0*3], xip->rhc_V, local2mm);
    VSCALE(&vec[1*3], xip->rhc_H, local2mm);
    VSCALE(&vec[2*3], xip->rhc_B, local2mm);
    vec[3*3] = xip->rhc_r * local2mm;
    vec[3*3+1] = xip->rhc_c * local2mm;

    /* Convert from internal (host) to database (network) format */
    htond(ep->ext_buf, (unsigned char *)vec, 11);

    return(0);
}


/**
 * R T _ R H C _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this solid.
 * First line describes type of solid.
 * Additional lines are indented one tab, and give parameter values.
 */
int
rt_rhc_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_rhc_internal *xip =
	(struct rt_rhc_internal *)ip->idb_ptr;
    char buf[256];

    RT_RHC_CK_MAGIC(xip);
    bu_vls_strcat(str, "Right Hyperbolic Cylinder (RHC)\n");

    if (!verbose)
	return 0;

    sprintf(buf, "\tV (%g, %g, %g)\n",
	    INTCLAMP(xip->rhc_V[X] * mm2local),
	    INTCLAMP(xip->rhc_V[Y] * mm2local),
	    INTCLAMP(xip->rhc_V[Z] * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tB (%g, %g, %g) mag=%g\n",
	    INTCLAMP(xip->rhc_B[X] * mm2local),
	    INTCLAMP(xip->rhc_B[Y] * mm2local),
	    INTCLAMP(xip->rhc_B[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(xip->rhc_B) * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tH (%g, %g, %g) mag=%g\n",
	    INTCLAMP(xip->rhc_H[X] * mm2local),
	    INTCLAMP(xip->rhc_H[Y] * mm2local),
	    INTCLAMP(xip->rhc_H[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(xip->rhc_H) * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tr=%g\n", INTCLAMP(xip->rhc_r * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tc=%g\n", INTCLAMP(xip->rhc_c * mm2local));
    bu_vls_strcat(str, buf);

    return 0;
}


/**
 * R T _ R H C _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_rhc_ifree(struct rt_db_internal *ip)
{
    struct rt_rhc_internal *xip;

    RT_CK_DB_INTERNAL(ip);

    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    RT_RHC_CK_MAGIC(xip);
    xip->rhc_magic = 0;		/* sanity */

    bu_free((char *)xip, "rhc ifree");
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}


/**
 * R T _ R H C _ P A R A M S
 *
 */
int
rt_rhc_params(struct pc_pc_set *ps, const struct rt_db_internal *ip)
{
    ps = ps; /* quellage */
    if (ip) RT_CK_DB_INTERNAL(ip);

    return(0);			/* OK */
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
