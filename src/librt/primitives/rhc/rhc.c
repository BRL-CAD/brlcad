/*                           R H C . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2013 United States Government as represented by
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
/** @file primitives/rhc/rhc.c
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
 * L : { P(n) | P + t(n) . D }
 *
 * Call W the actual point of intersection between L and the rhc.
 * Let W' be the point of intersection between L' and the unit rhc.
 *
 * L' : { P'(n) | P' + t(n) . D' }
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
 * A * k**2 + B * k + C = 0
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
 * Map W onto the unit rhc, i.e.:  W' = S(R(W - V)).
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
 * (Wz' + c + 1)**2 - (Wy'**2 * (2*c + 1) >= c**2 and Wz' <= 1.0
 *
 * The normal for a hit on the top plate is -Bunit.
 * The normal for a hit on the front plate is -Hunit, and
 * the normal for a hit on the back plate is +Hunit.
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

#include "../../librt_private.h"

static int
rhc_is_valid(struct rt_rhc_internal *rhc);

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
    { "%f", 3, "V", bu_offsetofarray(struct rt_rhc_internal, rhc_V, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "H", bu_offsetofarray(struct rt_rhc_internal, rhc_H, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "B", bu_offsetofarray(struct rt_rhc_internal, rhc_B, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "r", bu_offsetof(struct rt_rhc_internal, rhc_r),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "c", bu_offsetof(struct rt_rhc_internal, rhc_c),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/**
 * R T _ R H C _ B B O X
 *
 * Calculate the bounding RPP for an RHC
 */
int
rt_rhc_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol))
{

    struct rt_rhc_internal *xip;
    vect_t rinv, rvect, rv2, working;

    RT_CK_DB_INTERNAL(ip);
    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    RT_RHC_CK_MAGIC(xip);

    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    VCROSS(rvect, xip->rhc_H, xip->rhc_B);
    VREVERSE(rinv, rvect);
    VUNITIZE(rvect);
    VUNITIZE(rinv);
    VSCALE(rvect, rvect, xip->rhc_r);
    VSCALE(rinv, rinv, xip->rhc_r);

    VADD2(working, xip->rhc_V, rvect);
    VMINMAX((*min), (*max), working);

    VADD2(working, xip->rhc_V, rinv);
    VMINMAX((*min), (*max), working);

    VADD3(working, xip->rhc_V, rvect, xip->rhc_H);
    VMINMAX((*min), (*max), working);

    VADD3(working, xip->rhc_V, rinv, xip->rhc_H);
    VMINMAX((*min), (*max), working);

    VADD2(rv2, xip->rhc_V, xip->rhc_B);

    VADD2(working, rv2, rvect);
    VMINMAX((*min), (*max), working);

    VADD2(working, rv2, rinv);
    VMINMAX((*min), (*max), working);

    VADD3(working, rv2, rvect, xip->rhc_H);
    VMINMAX((*min), (*max), working);

    VADD3(working, rv2, rinv, xip->rhc_H);
    VMINMAX((*min), (*max), working);

    return 0;
}


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
 * A struct rhc_specific is created, and its address is stored in
 * stp->st_specific for use by rhc_shot().
 */
int
rt_rhc_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_rhc_internal *xip;
    struct rhc_specific *rhc;
    fastf_t magsq_b, magsq_h, magsq_r;
    fastf_t mag_b, mag_h, mag_r;
    mat_t R;
    mat_t Rinv;
    mat_t S;
    vect_t invsq;	/* [ 1/(|H|**2), 1/(|R|**2), 1/(|B|**2) ] */

    RT_CK_DB_INTERNAL(ip);
    RT_CK_RTI(rtip);

    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    if (!rhc_is_valid(xip)) {
	return 1;
    }

    /* compute |B| |H| */
    mag_b = sqrt(magsq_b = MAGSQ(xip->rhc_B));
    mag_h = sqrt(magsq_h = MAGSQ(xip->rhc_H));
    mag_r = xip->rhc_r;
    magsq_r = mag_r * mag_r;

    stp->st_id = ID_RHC;		/* set soltab ID */
    stp->st_meth = &OBJ[ID_RHC];

    BU_GET(rhc, struct rhc_specific);
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
    VSET(invsq, 1.0 / magsq_h, 1.0 / magsq_r, 1.0 / magsq_b);
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
    stp->st_bradius = 0.5 * sqrt(magsq_h + 4.0 * magsq_r + magsq_b);
    /* approximate bounding radius */
    stp->st_aradius = stp->st_bradius;

    if (rt_rhc_bbox(ip, &(stp->st_min), &(stp->st_max), &rtip->rti_tol)) {
	return 1;
    }

    return 0;			/* OK */
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

    if (ZERO(dprime[Y]) && ZERO(dprime[Z])) {
	goto check_plates;
    }

    /* Find roots of the equation, using formula for quadratic */
    {
	fastf_t a, b, c;	/* coeffs of polynomial */
	fastf_t disc;		/* disc of radical */

	a = dprime[Z] * dprime[Z] - dprime[Y] * dprime[Y] * (1 + 2 * x);
	b = 2 * ((pprime[Z] + x + 1) * dprime[Z]
		 - (2 * x + 1) * dprime[Y] * pprime[Y]);
	c = (pprime[Z] + x + 1) * (pprime[Z] + x + 1)
	    - (2 * x + 1) * pprime[Y] * pprime[Y] - x * x;

	if (!NEAR_ZERO(a, RT_PCOEF_TOL)) {
	    disc = b * b - 4 * a * c;

	    if (disc <= 0) {
		goto check_plates;
	    }

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
	    k1 = -c / b;
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
    if (hitp < &hits[2]  &&  !ZERO(dprime[X])) {
	/* 0 or 1 hits so far, this is worthwhile */
	k1 = -pprime[X] / dprime[X];		/* front plate */
	k2 = (-1.0 - pprime[X]) / dprime[X];	/* back plate */

	VJOIN1(hitp->hit_vpriv, pprime, k1, dprime);	/* hit' */

	if ((hitp->hit_vpriv[Z] + x + 1.0)
	    * (hitp->hit_vpriv[Z] + x + 1.0)
	    - hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y]
	    * (1.0 + 2 * x) >= x * x
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
	    * (1.0 + 2 * x) >= x * x
	    && hitp->hit_vpriv[Z] >= -1.0
	    && hitp->hit_vpriv[Z] <= 0.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k2;
	    hitp->hit_surfno = RHC_NORM_BACK;	/* +H */
	    hitp++;
	}
    }

    /* check top plate */
    if (hitp == &hits[1]  &&  !ZERO(dprime[Z])) {
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

    if (hitp != &hits[2]) {
	return 0;    /* MISS */
    }

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

    return 2;			/* HIT */
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
		 hitp->hit_vpriv[Y] * (1.0 + 2.0 * c),
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
	    zp1_sq = y * (b * b + 2 * b * c) / rsq;
	    zp1_sq *= zp1_sq / (c * c + y * y * (b * b + 2 * b * c) / rsq);
	    zp2 = c * c / (rsq * c * c + y * y * (b * b + 2 * b * c));
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

    if (ap) {
	RT_CK_APPLICATION(ap);
    }

    /*
     * hit_point is on surface;  project back to unit rhc,
     * creating a vector from vertex to hit point.
     */
    VSUB2(work, hitp->hit_point, rhc->rhc_V);
    MAT4X3VEC(pprime, rhc->rhc_SoR, work);

    switch (hitp->hit_surfno) {
	case RHC_NORM_BODY:
	    /* Skin.  x, y coordinates define rotation.  radius = 1 */
	    len = sqrt(pprime[Y] * pprime[Y] + pprime[Z] * pprime[Z]);
	    uvp->uv_u = acos(pprime[Y] / len) * bn_invpi;
	    uvp->uv_v = -pprime[X];		/* height */
	    break;

	case RHC_NORM_FRT:
	case RHC_NORM_BACK:
	    /* end plates - circular mapping, not seamless w/body, top */
	    len = sqrt(pprime[Y] * pprime[Y] + pprime[Z] * pprime[Z]);
	    uvp->uv_u = acos(pprime[Y] / len) * bn_invpi;
	    uvp->uv_v = len;	/* rim v = 1 for both plates */
	    break;

	case RHC_NORM_TOP:
	    uvp->uv_u = 1.0 - (pprime[Y] + 1.0) / 2.0;
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

    BU_PUT(rhc, struct rhc_specific);
}


/**
 * R T _ R H C _ C L A S S
 */
int
rt_rhc_class(void)
{
    return 0;
}


/* Our canonical hyperbola in the Y-Z plane has equation
 * z = +- (a/b) * sqrt(b^2 + y^2), and opens toward +Z and -Z with asymptote
 * origin at the origin.
 *
 * The contour of an rhc in the plane B-R is the positive half of a hyperbola
 * with asymptote origin at ((|B| + c)Bu), opening toward -B. We can transform
 * this hyperbola to get an equivalent canonical hyperbola in the Y-Z plane,
 * opening toward +Z (-B) with asymptote origin at the origin.
 *
 * This hyperbola passes through the point (r, |B| + a) (a = c). If we plug the
 * point (r, |H| + a) into our canonical equation, we can derive b from |B|, a,
 * and r.
 *
 *                             |B| + a = (a/b) * sqrt(b^2 + r^2)
 *                       (|B| + a) / a = b * sqrt(b^2 + r^2)
 *                   (|B| + a)^2 / a^2 = 1 + (r^2 / b^2)
 *           ((|B| + a)^2 - a^2) / a^2 = r^2 / b^2
 *   (a^2 * r^2) / ((|B| + a)^2 - a^2) = b^2
 *      (ar) / sqrt((|B| + a)^2 - a^2) = b
 *         (ar) / sqrt(|B| (|B| + 2a)) = b
 */
static fastf_t
rhc_hyperbola_b(fastf_t mag_b, fastf_t c, fastf_t r)
{
    return (c * r) / sqrt(mag_b * (mag_b + 2.0 * c));
}


static fastf_t
rhc_hyperbola_y(fastf_t b, fastf_t c, fastf_t z)
{
    return sqrt(b * b * (((z * z) / (c * c)) - 1.0));
}


/* The contour of an rhc in the plane B-R is the positive half of a hyperbola
 * with asymptote origin at ((|B| + c)Bu), opening toward -B. We can transform
 * this hyperbola to get an equivalent hyperbola in the Y-Z plane, opening
 * toward +Z (-B) with asymptote origin at (0, -(|B| + c)).
 *
 * The part of this hyperbola that passes between (0, -(|B| + c)) and (r, 0) is
 * approximated by num_points points (including (0, -|B|) and (r, 0)).
 *
 * The constructed point list is returned (NULL returned on error). Because the
 * above transformation puts the rhc vertex at the origin and the hyperbola
 * asymptote origin at (0, -|B| + c), multiplying the z values by -1 gives
 * corresponding distances along the rhc breadth vector B.
 */
static struct rt_pt_node *
rhc_hyperbolic_curve(fastf_t mag_b, fastf_t c, fastf_t r, int num_points)
{
    int count;
    struct rt_pt_node *curve;

    if (num_points < 2) {
	return NULL;
    }

    BU_ALLOC(curve, struct rt_pt_node);
    BU_ALLOC(curve->next, struct rt_pt_node);

    curve->next->next = NULL;
    VSET(curve->p,       0, 0, -mag_b);
    VSET(curve->next->p, 0, r, 0);

    if (num_points < 3) {
	return curve;
    }

    count = approximate_hyperbolic_curve(curve, c, rhc_hyperbola_b(mag_b, c, r), num_points - 2);

    if (count != (num_points - 2)) {
	return NULL;
    }

    return curve;
}


/* plot half of a hyperbolic contour curve using the given (r, b) points (pts),
 * translation along H (rhc_H), and multiplier for r (rscale)
 */
static void
rhc_plot_hyperbolic_curve(
    struct bu_list *vhead,
    struct rhc_specific *rhc,
    struct rt_pt_node *pts,
    vect_t rhc_H,
    fastf_t rscale)
{
    vect_t t, Ru, Bu;
    point_t p;
    struct rt_pt_node *node;

    VADD2(t, rhc->rhc_V, rhc_H);
    VMOVE(Ru, rhc->rhc_Runit);
    VMOVE(Bu, rhc->rhc_Bunit);

    VJOIN2(p, t, rscale * pts->p[Y], Ru, -pts->p[Z], Bu);
    RT_ADD_VLIST(vhead, p, BN_VLIST_LINE_MOVE);

    node = pts->next;
    while (node != NULL) {
	VJOIN2(p, t, rscale * node->p[Y], Ru, -node->p[Z], Bu);
	RT_ADD_VLIST(vhead, p, BN_VLIST_LINE_DRAW);

	node = node->next;
    }
}


static void
rhc_plot_hyperbolas(
    struct bu_list *vhead,
    struct rt_rhc_internal *rhc,
    struct rt_pt_node *pts)
{
    vect_t rhc_H;
    struct rhc_specific rhc_s;

    VMOVE(rhc_s.rhc_V, rhc->rhc_V);

    VMOVE(rhc_s.rhc_Bunit, rhc->rhc_B);
    VUNITIZE(rhc_s.rhc_Bunit);

    VCROSS(rhc_s.rhc_Runit, rhc_s.rhc_Bunit, rhc->rhc_H);
    VUNITIZE(rhc_s.rhc_Runit);

    /* plot hyperbolic contour curve of face containing V */
    VSETALL(rhc_H, 0.0);
    rhc_plot_hyperbolic_curve(vhead, &rhc_s, pts, rhc_H, 1.0);
    rhc_plot_hyperbolic_curve(vhead, &rhc_s, pts, rhc_H, -1.0);

    /* plot hyperbolic contour curve of opposing face */
    VMOVE(rhc_H, rhc->rhc_H);
    rhc_plot_hyperbolic_curve(vhead, &rhc_s, pts, rhc_H, 1.0);
    rhc_plot_hyperbolic_curve(vhead, &rhc_s, pts, rhc_H, -1.0);
}


static void
rhc_plot_curve_connections(
    struct bu_list *vhead,
    const struct rt_rhc_internal *rhc,
    int num_connections)
{
    point_t p;
    vect_t Yu, Zu;
    fastf_t b, y, z, z_step;
    fastf_t mag_Z, zmax;
    int connections_per_half;

    if (num_connections < 1) {
	return;
    }

    VMOVE(Zu, rhc->rhc_B);
    VCROSS(Yu, rhc->rhc_H, Zu);
    VUNITIZE(Yu);
    VUNITIZE(Zu);

    mag_Z = MAGNITUDE(rhc->rhc_B);

    b = rhc_hyperbola_b(mag_Z, rhc->rhc_c, rhc->rhc_r);

    connections_per_half = .5 + num_connections / 2.0;
    z_step = mag_Z / (connections_per_half + 1.0);

    zmax = mag_Z + rhc->rhc_c;
    for (z = 0.0; z <= mag_Z; z += z_step) {
	y = rhc_hyperbola_y(b, rhc->rhc_c, zmax - z);

	/* connect faces on one side of the curve */
	VJOIN2(p, rhc->rhc_V, z, Zu, -y, Yu);
	RT_ADD_VLIST(vhead, p, BN_VLIST_LINE_MOVE);

	VADD2(p, p, rhc->rhc_H);
	RT_ADD_VLIST(vhead, p, BN_VLIST_LINE_DRAW);

	/* connect the faces on the other side */
	VJOIN2(p, rhc->rhc_V, z, Zu, y, Yu);
	RT_ADD_VLIST(vhead, p, BN_VLIST_LINE_MOVE);

	VADD2(p, p, rhc->rhc_H);
	RT_ADD_VLIST(vhead, p, BN_VLIST_LINE_DRAW);
    }
}


static int
rhc_curve_points(
    struct rt_rhc_internal *rhc,
    const struct rt_view_info *info)
{
    fastf_t height, halfwidth, est_curve_length;
    point_t p0, p1;

    height = -MAGNITUDE(rhc->rhc_B);
    halfwidth = rhc->rhc_r;

    VSET(p0, 0, 0, height);
    VSET(p1, 0, halfwidth, 0);

    est_curve_length = 2.0 * DIST_PT_PT(p0, p1);

    return est_curve_length / info->point_spacing;
}


int
rt_rhc_adaptive_plot(struct rt_db_internal *ip, const struct rt_view_info *info)
{
    point_t p;
    vect_t rhc_R;
    int num_curve_points, num_connections;
    struct rt_rhc_internal *rhc;
    struct rt_pt_node *pts, *node, *tmp;

    BU_CK_LIST_HEAD(info->vhead);
    RT_CK_DB_INTERNAL(ip);

    rhc = (struct rt_rhc_internal *)ip->idb_ptr;
    if (!rhc_is_valid(rhc)) {
	return -2;
    }

    num_curve_points = rhc_curve_points(rhc, info);

    if (num_curve_points < 3) {
	num_curve_points = 3;
    }

    VCROSS(rhc_R, rhc->rhc_B, rhc->rhc_H);
    VUNITIZE(rhc_R);
    VSCALE(rhc_R, rhc_R, rhc->rhc_r);

    pts = rhc_hyperbolic_curve(MAGNITUDE(rhc->rhc_B), rhc->rhc_c, rhc->rhc_r, num_curve_points);
    rhc_plot_hyperbolas(info->vhead, rhc, pts);

    node = pts;
    while (node != NULL) {
	tmp = node;
	node = node->next;

	bu_free(tmp, "rt_pt_node");
    }

    /* connect both halves of the hyperbolic contours of the opposing faces */
    num_connections = primitive_curve_count(ip, info);
    if (num_connections < 2) {
	num_connections = 2;
    }

    rhc_plot_curve_connections(info->vhead, rhc, num_connections);

    /* plot rectangular face */
    VADD2(p, rhc->rhc_V, rhc_R);
    RT_ADD_VLIST(info->vhead, p, BN_VLIST_LINE_MOVE);

    VADD2(p, p, rhc->rhc_H);
    RT_ADD_VLIST(info->vhead, p, BN_VLIST_LINE_DRAW);

    VJOIN1(p, p, -2.0, rhc_R);
    RT_ADD_VLIST(info->vhead, p, BN_VLIST_LINE_DRAW);

    VJOIN1(p, p, -1.0, rhc->rhc_H);
    RT_ADD_VLIST(info->vhead, p, BN_VLIST_LINE_DRAW);

    VJOIN1(p, p, 2.0, rhc_R);
    RT_ADD_VLIST(info->vhead, p, BN_VLIST_LINE_DRAW);

    return 0;
}


/**
 * R T _ R H C _ P L O T
 */
int
rt_rhc_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
{
    int i, n;
    fastf_t b, c, *back, *front, rh;
    fastf_t dtol, ntol;
    vect_t Bu, Hu, Ru;
    mat_t R;
    mat_t invR;
    struct rt_rhc_internal *xip;
    struct rt_pt_node *old, *pos, *pts;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);

    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    if (!rhc_is_valid(xip)) {
	return -2;
    }

    /* compute |B| |H| */
    b = MAGNITUDE(xip->rhc_B);	/* breadth */
    rh = xip->rhc_r;		/* rectangular halfwidth */
    c = xip->rhc_c;			/* dist to asympt origin */

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

    if (rh < b) {
	dtol = primitive_get_absolute_tolerance(ttol, 2.0 * rh);
    } else {
	dtol = primitive_get_absolute_tolerance(ttol, 2.0 * b);
    }

    /* To ensure normal tolerance, remain below this angle */
    if (ttol->norm > 0.0) {
	ntol = ttol->norm;
    } else {
	/* tolerate everything */
	ntol = bn_pi;
    }

    /* initial hyperbola approximation is a single segment */
    BU_ALLOC(pts, struct rt_pt_node);
    BU_ALLOC(pts->next, struct rt_pt_node);

    pts->next->next = NULL;
    VSET(pts->p,       0, -rh, 0);
    VSET(pts->next->p, 0,  rh, 0);
    /* 2 endpoints in 1st approximation */
    n = 2;
    /* recursively break segment 'til within error tolerances */
    n += rt_mk_hyperbola(pts, rh, b, c, dtol, ntol);

    /* get mem for arrays */
    front = (fastf_t *)bu_malloc(3 * n * sizeof(fastf_t), "fast_t");
    back  = (fastf_t *)bu_malloc(3 * n * sizeof(fastf_t), "fast_t");

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
    RT_ADD_VLIST(vhead, &front[(n - 1)*ELEMENTS_PER_VECT],
		 BN_VLIST_LINE_MOVE);

    for (i = 0; i < n; i++) {
	RT_ADD_VLIST(vhead, &front[i * ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
    }

    /* Draw the back */
    RT_ADD_VLIST(vhead, &back[(n - 1)*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);

    for (i = 0; i < n; i++) {
	RT_ADD_VLIST(vhead, &back[i * ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
    }

    /* Draw connections */
    for (i = 0; i < n; i++) {
	RT_ADD_VLIST(vhead, &front[i * ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE);
	RT_ADD_VLIST(vhead, &back[i * ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW);
    }

    /* free mem */
    bu_free((char *)front, "fastf_t");
    bu_free((char *)back, "fastf_t");

    return 0;
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
    struct rt_pt_node *newpt;

#define RHC_TOL .0001
    /* endpoints of segment approximating hyperbola */
    VMOVE(p0, pts->p);
    VMOVE(p1, pts->next->p);
    /* slope and intercept of segment */
    m = (p1[Z] - p0[Z]) / (p1[Y] - p0[Y]);
    intr = p0[Z] - m * p0[Y];
    /* find point on hyperbola with max dist between hyperbola and line */
    j = b + c;
    k = 1 - m * m * r * r / (b * (b + 2 * c));
    A = k;
    B = 2 * j * k;
    C = j * j * k - c * c;
    discr = sqrt(B * B - 4 * A * C);
    z0 = (-B + discr) / (2. * A);

    if (z0 + RHC_TOL >= -b) {
	/* use top sheet of hyperboloid */
	mpt[Z] = z0;
    } else {
	mpt[Z] = (-B - discr) / (2. * A);
    }

    if (NEAR_ZERO(mpt[Z], RHC_TOL)) {
	mpt[Z] = 0.;
    }

    mpt[X] = 0;
    mpt[Y] = ((mpt[Z] + b + c) * (mpt[Z] + b + c) - c * c) / (b * (b + 2 * c));

    if (NEAR_ZERO(mpt[Y], RHC_TOL)) {
	mpt[Y] = 0.;
    }

    mpt[Y] = r * sqrt(mpt[Y]);

    if (p0[Y] < 0.) {
	mpt[Y] = -mpt[Y];
    }

    /* max distance between that point and line */
    dist = fabs(m * mpt[Y] - mpt[Z] + intr) / sqrt(m * m + 1);
    /* angles between normal of line and of hyperbola at line endpoints */
    VSET(norm_line, m, -1., 0.);
    VSET(norm_hyperb, 0., (2 * c + 1) / (p0[Z] + c + 1), -1.);
    VUNITIZE(norm_line);
    VUNITIZE(norm_hyperb);
    theta0 = fabs(acos(VDOT(norm_line, norm_hyperb)));
    VSET(norm_hyperb, 0., (2 * c + 1) / (p1[Z] + c + 1), -1.);
    VUNITIZE(norm_hyperb);
    theta1 = fabs(acos(VDOT(norm_line, norm_hyperb)));

    /* split segment at widest point if not within error tolerances */
    if (dist > dtol || theta0 > ntol || theta1 > ntol) {
	/* split segment */
	BU_ALLOC(newpt, struct rt_pt_node);
	VMOVE(newpt->p, mpt);
	newpt->next = pts->next;
	pts->next = newpt;
	/* keep track of number of pts added */
	n = 1;
	/* recurse on first new segment */
	n += rt_mk_hyperbola(pts, r, b, c, dtol, ntol);
	/* recurse on second new segment */
	n += rt_mk_hyperbola(newpt, r, b, c, dtol, ntol);
    } else {
	n  = 0;
    }

    return n;
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
    fastf_t b, c, *back, *front, rh;
    fastf_t dtol, ntol;
    vect_t Bu, Hu, Ru;
    mat_t R;
    mat_t invR;
    struct rt_rhc_internal *xip;
    struct rt_pt_node *old, *pos, *pts;
    struct shell *s;
    struct faceuse **outfaceuses;
    struct vertex **vfront, **vback, **vtemp, *vertlist[4];
    vect_t *norms;
    fastf_t bb_plus_2bc, b_plus_c, r_sq;
    int failure = 0;

    NMG_CK_MODEL(m);
    BN_CK_TOL(tol);
    RT_CK_TESS_TOL(ttol);

    RT_CK_DB_INTERNAL(ip);

    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    if (!rhc_is_valid(xip)) {
	return -2;
    }

    /* compute |B| |H| */
    b = MAGNITUDE(xip->rhc_B);	/* breadth */
    rh = xip->rhc_r;		/* rectangular halfwidth */
    c = xip->rhc_c;			/* dist to asympt origin */

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

    if (rh < b) {
	dtol = primitive_get_absolute_tolerance(ttol, 2.0 * rh);
    } else {
	dtol = primitive_get_absolute_tolerance(ttol, 2.0 * b);
    }

    /* To ensure normal tolerance, remain below this angle */
    if (ttol->norm > 0.0) {
	ntol = ttol->norm;
    } else {
	/* tolerate everything */
	ntol = bn_pi;
    }

    /* initial hyperbola approximation is a single segment */
    BU_ALLOC(pts, struct rt_pt_node);
    BU_ALLOC(pts->next, struct rt_pt_node);

    pts->next->next = NULL;
    VSET(pts->p,       0, -rh, 0);
    VSET(pts->next->p, 0,  rh, 0);
    /* 2 endpoints in 1st approximation */
    n = 2;
    /* recursively break segment 'til within error tolerances */
    n += rt_mk_hyperbola(pts, rh, b, c, dtol, ntol);

    /* get mem for arrays */
    front = (fastf_t *)bu_malloc(3 * n * sizeof(fastf_t), "fastf_t");
    back  = (fastf_t *)bu_malloc(3 * n * sizeof(fastf_t), "fastf_t");
    norms = (vect_t *)bu_calloc(n, sizeof(vect_t), "rt_rhc_tess: norms");
    vfront = (struct vertex **)bu_malloc((n + 1) * sizeof(struct vertex *), "vertex *");
    vback = (struct vertex **)bu_malloc((n + 1) * sizeof(struct vertex *), "vertex *");
    vtemp = (struct vertex **)bu_malloc((n + 1) * sizeof(struct vertex *), "vertex *");
    outfaceuses =
	(struct faceuse **)bu_malloc((n + 2) * sizeof(struct faceuse *), "faceuse *");

    if (!front || !back || !vfront || !vback || !vtemp || !outfaceuses) {
	fprintf(stderr, "rt_rhc_tess: no memory!\n");
	goto fail;
    }

    /* generate front & back plates in world coordinates */
    bb_plus_2bc = b * b + 2.0 * b * c;
    b_plus_c = b + c;
    r_sq = rh * rh;
    pos = pts;
    i = 0;
    j = 0;

    while (pos) {
	vect_t tmp_norm;

	/* calculate normal for 2D hyperbola */
	VSET(tmp_norm, 0.0, pos->p[Y]*bb_plus_2bc, (-r_sq * (pos->p[Z] + b_plus_c)));
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

    for (i = 0; i < n; i++) {
	vfront[i] = vtemp[i] = (struct vertex *)0;
    }

    /* Front face topology.  Verts are considered to go CCW */
    outfaceuses[0] = nmg_cface(s, vfront, n);

    (void)nmg_mark_edges_real(&outfaceuses[0]->l.magic);

    /* Back face topology.  Verts must go in opposite dir (CW) */
    outfaceuses[1] = nmg_cface(s, vtemp, n);

    for (i = 0; i < n; i++) {
	vback[i] = vtemp[n - 1 - i];
    }

    (void)nmg_mark_edges_real(&outfaceuses[1]->l.magic);

    /* Duplicate [0] as [n] to handle loop end condition, below */
    vfront[n] = vfront[0];
    vback[n] = vback[0];

    /* Build topology for all the rectangular side faces (n of them)
     * connecting the front and back faces.
     * increasing indices go towards counter-clockwise (CCW).
     */
    for (i = 0; i < n; i++) {
	vertlist[0] = vfront[i];	/* from top, */
	vertlist[1] = vback[i];		/* straight down, */
	vertlist[2] = vback[i + 1];	/* to left, */
	vertlist[3] = vfront[i + 1];	/* straight up. */
	outfaceuses[2 + i] = nmg_cface(s, vertlist, 4);
    }

    (void)nmg_mark_edges_real(&outfaceuses[n + 1]->l.magic);

    for (i = 0; i < n; i++) {
	NMG_CK_VERTEX(vfront[i]);
	NMG_CK_VERTEX(vback[i]);
    }

    /* Associate the vertex geometry, CCW */
    for (i = 0; i < n; i++) {
	nmg_vertex_gv(vfront[i], &front[3 * (i)]);
    }

    for (i = 0; i < n; i++) {
	nmg_vertex_gv(vback[i], &back[3 * (i)]);
    }

    /* Associate the face geometry */
    for (i = 0; i < n + 2; i++) {
	if (nmg_fu_planeeqn(outfaceuses[i], tol) < 0) {
	    failure = (-1);
	    goto fail;
	}
    }

    /* Associate vertexuse normals */
    for (i = 0; i < n; i++) {
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
		fu->f_p == outfaceuses[n + 1]->f_p) {
		continue;    /* skip flat faces */
	    }

	    if (fu->orientation == OT_SAME) {
		nmg_vertexuse_nv(vu, norms[i]);
	    } else if (fu->orientation == OT_OPPOSITE) {
		nmg_vertexuse_nv(vu, rev_norm);
	    }
	}

	/* and "back" vertices */
	NMG_CK_VERTEX(vback[i]);

	for (BU_LIST_FOR(vu, vertexuse, &vback[i]->vu_hd)) {
	    NMG_CK_VERTEXUSE(vu);
	    fu = nmg_find_fu_of_vu(vu);
	    NMG_CK_FACEUSE(fu);

	    if (fu->f_p == outfaceuses[0]->f_p ||
		fu->f_p == outfaceuses[1]->f_p ||
		fu->f_p == outfaceuses[n + 1]->f_p) {
		continue;    /* skip flat faces */
	    }

	    if (fu->orientation == OT_SAME) {
		nmg_vertexuse_nv(vu, norms[i]);
	    } else if (fu->orientation == OT_OPPOSITE) {
		nmg_vertexuse_nv(vu, rev_norm);
	    }
	}
    }

    /* Glue the edges of different outward pointing face uses together */
    nmg_gluefaces(outfaceuses, n + 2, tol);

    /* Compute "geometry" for region and shell */
    nmg_region_a(*r, tol);

fail:
    /* free mem */
    bu_free((char *)front, "fastf_t");
    bu_free((char *)back, "fastf_t");
    bu_free((char *)vfront, "vertex *");
    bu_free((char *)vback, "vertex *");
    bu_free((char *)vtemp, "vertex *");
    bu_free((char *)norms, "rt_rhc_tess: norms");
    bu_free((char *)outfaceuses, "faceuse *");

    return failure;
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
    vect_t v1, v2, v3;

    if (dbip) {
	RT_CK_DBI(dbip);
    }

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;

    /* Check record type */
    if (rp->u_id != ID_SOLID) {
	bu_log("rt_rhc_import4: defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_RHC;
    ip->idb_meth = &OBJ[ID_RHC];
    BU_ALLOC(ip->idb_ptr, struct rt_rhc_internal);

    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    xip->rhc_magic = RT_RHC_INTERNAL_MAGIC;

    /* Warning:  type conversion */
    if (mat == NULL) {
	mat = bn_mat_identity;
    }

    if (dbip->dbi_version < 0) {
	flip_fastf_float(v1, &rp->s.s_values[0 * 3], 1, 1);
	flip_fastf_float(v2, &rp->s.s_values[1 * 3], 1, 1);
	flip_fastf_float(v3, &rp->s.s_values[2 * 3], 1, 1);
    } else {
	VMOVE(v1, &rp->s.s_values[0 * 3]);
	VMOVE(v2, &rp->s.s_values[1 * 3]);
	VMOVE(v3, &rp->s.s_values[2 * 3]);
    }

    MAT4X3PNT(xip->rhc_V, mat, v1);
    MAT4X3VEC(xip->rhc_H, mat, v2);
    MAT4X3VEC(xip->rhc_B, mat, v3);

    if (dbip->dbi_version < 0) {
	v1[X] = flip_dbfloat(rp->s.s_values[3 * 3 + 0]);
	v1[Y] = flip_dbfloat(rp->s.s_values[3 * 3 + 1]);
    } else {
	v1[X] = rp->s.s_values[3 * 3 + 0];
	v1[Y] = rp->s.s_values[3 * 3 + 1];
    }

    xip->rhc_r = v1[X] / mat[15];
    xip->rhc_c = v2[Y] / mat[15];

    if (xip->rhc_r <= SMALL_FASTF || xip->rhc_c <= SMALL_FASTF) {
	bu_log("rt_rhc_import4: r or c are zero\n");
	bu_free((char *)ip->idb_ptr, "rt_rhc_import4: ip->idb_ptr");
	return -1;
    }

    return 0;			/* OK */
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

    if (dbip) {
	RT_CK_DBI(dbip);
    }

    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_type != ID_RHC) {
	return -1;
    }

    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    if (!rhc_is_valid(xip)) {
	return -1;
    }

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record);
    ep->ext_buf = (genptr_t)bu_calloc(1, ep->ext_nbytes, "rhc external");
    rhc = (union record *)ep->ext_buf;

    rhc->s.s_id = ID_SOLID;
    rhc->s.s_type = RHC;

    /* Warning:  type conversion */
    VSCALE(&rhc->s.s_values[0 * 3], xip->rhc_V, local2mm);
    VSCALE(&rhc->s.s_values[1 * 3], xip->rhc_H, local2mm);
    VSCALE(&rhc->s.s_values[2 * 3], xip->rhc_B, local2mm);
    rhc->s.s_values[3 * 3] = xip->rhc_r * local2mm;
    rhc->s.s_values[3 * 3 + 1] = xip->rhc_c * local2mm;

    return 0;
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

    /* must be double for import and export */
    double vec[11];

    if (dbip) {
	RT_CK_DBI(dbip);
    }

    BU_CK_EXTERNAL(ep);
    BU_ASSERT_LONG(ep->ext_nbytes, == , SIZEOF_NETWORK_DOUBLE * 11);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_RHC;
    ip->idb_meth = &OBJ[ID_RHC];
    BU_ALLOC(ip->idb_ptr, struct rt_rhc_internal);

    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    xip->rhc_magic = RT_RHC_INTERNAL_MAGIC;

    /* Convert from database (network) to internal (host) format */
    bu_cv_ntohd((unsigned char *)vec, ep->ext_buf, 11);

    /* Apply modeling transformations */
    if (mat == NULL) {
	mat = bn_mat_identity;
    }

    MAT4X3PNT(xip->rhc_V, mat, &vec[0 * 3]);
    MAT4X3VEC(xip->rhc_H, mat, &vec[1 * 3]);
    MAT4X3VEC(xip->rhc_B, mat, &vec[2 * 3]);
    xip->rhc_r = vec[3 * 3] / mat[15];
    xip->rhc_c = vec[3 * 3 + 1] / mat[15];

    if (xip->rhc_r <= SMALL_FASTF || xip->rhc_c <= SMALL_FASTF) {
	bu_log("rt_rhc_import4: r or c are zero\n");
	bu_free((char *)ip->idb_ptr, "rt_rhc_import4: ip->idb_ptr");
	return -1;
    }

    return 0;			/* OK */
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

    /* must be double for import and export */
    double vec[11];

    if (dbip) {
	RT_CK_DBI(dbip);
    }

    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_type != ID_RHC) {
	return -1;
    }

    xip = (struct rt_rhc_internal *)ip->idb_ptr;
    if (!rhc_is_valid(xip)) {
	return -1;
    }

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * 11;
    ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes, "rhc external");

    /* scale 'em into local buffer */
    VSCALE(&vec[0 * 3], xip->rhc_V, local2mm);
    VSCALE(&vec[1 * 3], xip->rhc_H, local2mm);
    VSCALE(&vec[2 * 3], xip->rhc_B, local2mm);
    vec[3 * 3] = xip->rhc_r * local2mm;
    vec[3 * 3 + 1] = xip->rhc_c * local2mm;

    /* Convert from internal (host) to database (network) format */
    bu_cv_htond(ep->ext_buf, (unsigned char *)vec, 11);

    return 0;
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

    if (!verbose) {
	return 0;
    }

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
rt_rhc_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) {
	RT_CK_DB_INTERNAL(ip);
    }

    return 0;			/* OK */
}


static int
rhc_is_valid(struct rt_rhc_internal *rhc)
{
    fastf_t mag_b, mag_h, cos_angle_bh;

    RT_RHC_CK_MAGIC(rhc);

    mag_b = MAGNITUDE(rhc->rhc_B);
    mag_h = MAGNITUDE(rhc->rhc_H);

    /* check for |H| > 0, |B| > 0, |R| > 0, c > 0 */
    if (NEAR_ZERO(mag_h, RT_LEN_TOL)
	|| NEAR_ZERO(mag_b, RT_LEN_TOL)
	|| NEAR_ZERO(rhc->rhc_r, RT_LEN_TOL)
	|| NEAR_ZERO(rhc->rhc_c, RT_LEN_TOL))
    {
	return 0;
    }

    /* check B orthogonal to H */
    cos_angle_bh = VDOT(rhc->rhc_B, rhc->rhc_H) / (mag_b * mag_h);
    if (!NEAR_ZERO(cos_angle_bh, RT_DOT_TOL)) {
	return 0;
    }

    return 1;
}


void
rt_rhc_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    struct rt_rhc_internal *rip;
    fastf_t A, arclen, integralArea, a, b, magB, sqrt_ra, height;

    fastf_t h;
    fastf_t sumodds = 0, sumevens = 0, x = 0;
    int i, j;

    /**
     * n is the number of divisions to use when using Simpson's
     * composite rule below to approximate the integral.
     *
     * A value of n = 1000000 should be enough to ensure that the
     * approximation is accurate to at least 10 decimal places.  The
     * accuracy of the approximation increases by about 2 d.p with
     * each added 0 onto the end of the number (i.e. multiply by 10),
     * so there is a compromise between accuracy and performance,
     * although performance might only be an issue on old slow
     * hardware.
     *
     * I wouldn't recommend setting this less than about 10000,
     * because this might cause accuracy to be unsuitable for
     * professional or mission critical use.
     */
    int n = 1000000;

    if (area == NULL || ip == NULL) {
	return;
    }

    RT_CK_DB_INTERNAL(ip);
    rip = (struct rt_rhc_internal *)ip->idb_ptr;
    RT_RHC_CK_MAGIC(rip);

    b = rip->rhc_c;
    magB = MAGNITUDE(rip->rhc_B);
    height = MAGNITUDE(rip->rhc_H);
    a = (rip->rhc_r * b) / sqrt(magB * (2 * rip->rhc_c + magB));
    sqrt_ra = sqrt(rip->rhc_r * rip->rhc_r + b * b);
    integralArea = (b / a) * ((2 * rip->rhc_r * sqrt_ra) / 2 + ((a * a) / 2) * (log(sqrt_ra + rip->rhc_r) - log(sqrt_ra - rip->rhc_r)));
    A = 2 * rip->rhc_r * (rip->rhc_c + magB) - integralArea;

    h = (2 * rip->rhc_r) / n;
    for (i = 1; i <= (n / 2) - 1; i++) {
	x = -rip->rhc_r + 2 * i * h;
	sumodds += sqrt((b * b * x * x) / (a * a * x * x + pow(a, 4)) + 1);
    }
    for (j = 1; j <= (n / 2); j++) {
	x = -rip->rhc_r + (2 * j - 1) * h;
	sumevens += sqrt((b * b * x * x) / (a * a * x * x + pow(a, 4)) + 1);
    }
    arclen = (h / 3) * (sqrt((b * b * rip->rhc_r * rip->rhc_r) / (a * a * rip->rhc_r * rip->rhc_r + pow(a, 4)) + 1) + 2 * sumodds + 4 * sumevens + sqrt((b * b * rip->rhc_r * rip->rhc_r) / (a * a * rip->rhc_r * rip->rhc_r + pow(a, 4)) + 1));

    *area = 2 * A + 2 * rip->rhc_r * height + arclen * height;
}

/**
 * R T _ R H C _ C E N T R O I D
 *
 * Computes centroid of a right hyperbolic cylinder
 */
void
rt_rhc_centroid(point_t *cent, const struct rt_db_internal *ip)
{
    if (cent != NULL && ip != NULL) {
	struct rt_rhc_internal *rip;
	fastf_t totalArea, guessArea, a, b, magB, sqrt_xa, sqrt_ga, xf, epsilon, high, low, guess, scale_factor;
	vect_t shift_h;

	RT_CK_DB_INTERNAL(ip);
	rip = (struct rt_rhc_internal *)ip->idb_ptr;
	RT_RHC_CK_MAGIC(rip);

	magB = MAGNITUDE(rip->rhc_B);
	b = rip->rhc_c;
	a = (rip->rhc_r * b) / sqrt(magB * (2 * rip->rhc_c + magB));
	xf = magB + a;

	/* epsilon is an upperbound on the error */

	epsilon = 0.0001;

	sqrt_xa = sqrt((xf * xf) - (a * a));
	totalArea = (b / a) * ((xf * sqrt_xa) - ((a * a) * log(sqrt_xa + xf)) - ((a * a) * log(xf)));

	low = a;
	high = xf;

	while (abs(high - low) > epsilon) {
	    guess = (high + low) / 2;
	    sqrt_ga = sqrt((guess * guess) - (a * a));
	    guessArea = (b / a) * ((guess * sqrt_ga) - ((a * a) * log(sqrt_ga + guess)) - ((a * a) * log(guess)));

	    if (guessArea > totalArea / 2) {
		high = guess;
	    } else {
		low = guess;
	    }
	}

	scale_factor = 1 - ((guess - a) / magB);

	VSCALE(shift_h, rip->rhc_H, 0.5);
	VSCALE(*cent, rip->rhc_B, scale_factor);
	VADD2(*cent, shift_h, *cent);
    }
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
