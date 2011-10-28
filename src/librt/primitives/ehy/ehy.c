/*                           E H Y . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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
/** @file primitives/ehy/ehy.c
 *
 * Intersect a ray with an Elliptical Hyperboloid.
 *
 * Algorithm -
 *
 * Given V, H, R, and B, there is a set of points on this ehy
 *
 * { (x, y, z) | (x, y, z) is on ehy }
 *
 * Through a series of Affine Transformations, this set of points will
 * be transformed into a set of points on an ehy located at the origin
 * with a semi-major axis R1 along the +Y axis, a semi-minor axis R2
 * along the -X axis, a height H along the -Z axis, a vertex V at the
 * origin, and a distance c between the tip of the hyperboloid and
 * vertex of the asymptotic cone.
 *
 *
 * { (x', y', z') | (x', y', z') is on ehy at origin }
 *
 * The transformation from X to X' is accomplished by:
 *
 * X' = S(R(X - V))
 *
 * where R(X) =	(R2/(-|R2|))
 *  		(R1/(|R1|)) . X
 * 		(H /(-|H |))
 *
 * and S(X) =	(1/|R2|   0     0)
 * 		(0    1/|R1|   0) . X
 * 		(0      0   1/|H |)
 *
 * To find the intersection of a line with the surface of the ehy,
 * consider the parametric line L:
 *
 * L : { P(n) | P + t(n) . D }
 *
 * Call W the actual point of intersection between L and the ehy.  Let
 * W' be the point of intersection between L' and the unit ehy.
 *
 * L' : { P'(n) | P' + t(n) . D' }
 *
 * W = invR(invS(W')) + V
 *
 * Where W' = k D' + P'.
 *
 * If Dy' and Dz' are both 0, then there is no hit on the ehy; but the
 * end plates need checking.  If there is now only 1 hit point, the
 * top plate needs to be checked as well.
 *
 * Line L' hits the infinitely long canonical ehy at W' when
 *
 * A * k**2 + B * k + C = 0
 *
 * where
 *
 * A = Dz'**2 - (2*c' + 1)*(Dx'**2 + Dy'**2)
 * B = 2*((Pz' + c' + 1)*Dz' - (2*c' + 1)*(Dx'*Px' + Dy'Py'))
 * C = Pz'**2 - (2*c' + 1)*(Px'**2 + Py'**2 - 1) + 2*(c' + 1)*Pz'
 * b = |Breadth| = 1.0
 * h = |Height| = 1.0
 * r = 1.0
 * c' = c / |Breadth|
 *
 * The quadratic formula yields k (which is constant):
 *
 * k = [ -B +/- sqrt(B**2 - 4 * A * C)] / (2.0 * A)
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
 * The hit at ``k'' is a hit on the canonical ehy IFF
 * -1 <= Wz' <= 0.
 *
 * NORMALS.  Given the point W on the surface of the ehy, what is the
 * vector normal to the tangent plane at that point?
 *
 * Map W onto the unit ehy, ie:  W' = S(R(W - V)).
 *
 * Plane on unit ehy at W' has a normal vector N' where
 *
 * N' = <Wx', Wy', -(z + c + 1) / (2*c + 1)>
 *
 * The plane transforms back to the tangent plane at W, and this new
 * plane (on the original ehy) has a normal vector of N, viz:
 *
 * N = inverse[ transpose(inverse[ S o R ]) ] (N')
 *
 * because if H is perpendicular to plane Q, and matrix M maps from Q
 * to Q', then inverse[ transpose(M) ] (H) is perpendicular to Q'.
 * Here, H and Q are in "prime space" with the unit sphere.  [Somehow,
 * the notation here is backwards].  So, the mapping matrix M =
 * inverse(S o R), because S o R maps from normal space to the unit
 * sphere.
 *
 * N = inverse[ transpose(inverse[ S o R ]) ] (N')
 *   = inverse[ transpose(invR o invS) ] (N')
 *   = inverse[ transpose(invS) o transpose(invR) ] (N')
 *   = inverse[ inverse(S) o R ] (N')
 *   = invR o S (N')
 *
 * because inverse(R) = transpose(R), so R = transpose(invR), and S =
 * transpose(S).
 *
 * Note that the normal vector produced above will not have unit
 * length.
 *
 * THE TOP PLATE.
 *
 * If Dz' == 0, line L' is parallel to the top plate, so there is no
 * hit on the top plate.  Otherwise, rays intersect the top plate with
 * k = (0 - Pz')/Dz'.  The solution is within the top plate IFF Wx'**2
 * + Wy'**2 <= 1.
 *
 * The normal for a hit on the top plate is -Hunit.
 *
 */

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


struct ehy_specific {
    point_t ehy_V;	/* vector to ehy origin */
    vect_t ehy_Hunit;	/* unit H vector */
    vect_t ehy_Aunit;	/* unit vector along semi-major axis */
    vect_t ehy_Bunit;	/* unit vector, A x H, semi-minor axis */
    mat_t ehy_SoR;	/* Scale(Rot(vect)) */
    mat_t ehy_invRoS;	/* invRot(Scale(vect)) */
    fastf_t ehy_cprime;	/* c / |H| */
};


const struct bu_structparse rt_ehy_parse[] = {
    { "%f", 3, "V",   bu_offsetof(struct rt_ehy_internal, ehy_V[X]),  BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "H",   bu_offsetof(struct rt_ehy_internal, ehy_H[X]),  BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "A",   bu_offsetof(struct rt_ehy_internal, ehy_Au[X]), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "r_1", bu_offsetof(struct rt_ehy_internal, ehy_r1),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "r_2", bu_offsetof(struct rt_ehy_internal, ehy_r2),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "c",   bu_offsetof(struct rt_ehy_internal, ehy_c),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

/**
 * R T _ E H Y _ B B O X
 *
 * Create a bounding RPP for an ehy
 */
int
rt_ehy_bbox(struct rt_db_internal *ip, point_t *min, point_t *max) {
    struct rt_ehy_internal *xip;
    vect_t ehy_A, ehy_B, ehy_An, ehy_Bn, ehy_H;
    vect_t pt1, pt2, pt3, pt4, pt5, pt6, pt7, pt8;
    RT_CK_DB_INTERNAL(ip);
    xip = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(xip);

    VMOVE(ehy_H, xip->ehy_H);
    VUNITIZE(ehy_H);
    VMOVE(ehy_A, xip->ehy_Au);
    VCROSS(ehy_B, ehy_A, ehy_H);

    VSETALL((*min), MAX_FASTF);
    VSETALL((*max), -MAX_FASTF);

    VSCALE(ehy_A, ehy_A, xip->ehy_r1);
    VSCALE(ehy_B, ehy_B, xip->ehy_r2);
    VREVERSE(ehy_An, ehy_A);
    VREVERSE(ehy_Bn, ehy_B);

    VADD3(pt1, xip->ehy_V, ehy_A, ehy_B);
    VADD3(pt2, xip->ehy_V, ehy_A, ehy_Bn);
    VADD3(pt3, xip->ehy_V, ehy_An, ehy_B);
    VADD3(pt4, xip->ehy_V, ehy_An, ehy_Bn);
    VADD4(pt5, xip->ehy_V, ehy_A, ehy_B, xip->ehy_H);
    VADD4(pt6, xip->ehy_V, ehy_A, ehy_Bn, xip->ehy_H);
    VADD4(pt7, xip->ehy_V, ehy_An, ehy_B, xip->ehy_H);
    VADD4(pt8, xip->ehy_V, ehy_An, ehy_Bn, xip->ehy_H);

    /* Find the RPP of the rotated axis-aligned ehy bbox - that is,
     * the bounding box the given ehy would have if its height
     * vector were in the positive Z direction. This does not give 
     * us an optimal bbox except in the case where the ehy is 
     * actually axis aligned to start with, but it's usually 
     * at least a bit better than the bounding sphere RPP. */
    VMINMAX((*min), (*max), pt1);
    VMINMAX((*min), (*max), pt2);
    VMINMAX((*min), (*max), pt3);
    VMINMAX((*min), (*max), pt4);
    VMINMAX((*min), (*max), pt5);
    VMINMAX((*min), (*max), pt6);
    VMINMAX((*min), (*max), pt7);
    VMINMAX((*min), (*max), pt8);
    return 0;
}


/**
 * R T _ E H Y _ P R E P
 *
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid EHY, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 EHY is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct ehy_specific is created, and its address is stored in
 * stp->st_specific for use by ehy_shot().
 */
int
rt_ehy_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *UNUSED(rtip))
{
    struct rt_ehy_internal *xip;
    struct ehy_specific *ehy;

    fastf_t magsq_h;
    fastf_t mag_a, mag_h;
    fastf_t c, f, r1, r2;
    mat_t R;
    mat_t Rinv;
    mat_t S;

    RT_CK_DB_INTERNAL(ip);

    xip = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(xip);

    /* compute |A| |H| */
    mag_a = sqrt(MAGSQ(xip->ehy_Au));
    mag_h = sqrt(magsq_h = MAGSQ(xip->ehy_H));
    r1 = xip->ehy_r1;
    r2 = xip->ehy_r2;
    c = xip->ehy_c;
    /* Check for |H| > 0, |A| == 1, r1 > 0, r2 > 0, c > 0 */
    if (NEAR_ZERO(mag_h, RT_LEN_TOL)
	|| !NEAR_EQUAL(mag_a, 1.0, RT_LEN_TOL)
	|| r1 < 0.0 || r2 < 0.0 || c < 0.0) {
	return -2;		/* BAD, too small */
    }

    /* Check for A.H == 0 */
    f = VDOT(xip->ehy_Au, xip->ehy_H) / mag_h;
    if (!NEAR_ZERO(f, RT_DOT_TOL)) {
	return -2;		/* BAD */
    }

    /*
     * EHY is ok
     */
    stp->st_id = ID_EHY;		/* set soltab ID */
    stp->st_meth = &rt_functab[ID_EHY];

    BU_GETSTRUCT(ehy, ehy_specific);
    stp->st_specific = (genptr_t)ehy;

    /* make unit vectors in A, H, and BxH directions */
    VMOVE(ehy->ehy_Hunit, xip->ehy_H);
    VUNITIZE(ehy->ehy_Hunit);
    VMOVE(ehy->ehy_Aunit, xip->ehy_Au);
    VCROSS(ehy->ehy_Bunit, ehy->ehy_Aunit, ehy->ehy_Hunit);

    VMOVE(ehy->ehy_V, xip->ehy_V);
    ehy->ehy_cprime = c / mag_h;

    /* Compute R and Rinv matrices */
    MAT_IDN(R);
    VREVERSE(&R[0], ehy->ehy_Bunit);
    VMOVE(&R[4], ehy->ehy_Aunit);
    VREVERSE(&R[8], ehy->ehy_Hunit);
    bn_mat_trn(Rinv, R);			/* inv of rot mat is trn */

    /* Compute S */
    MAT_IDN(S);
    S[ 0] = 1.0/r2;
    S[ 5] = 1.0/r1;
    S[10] = 1.0/mag_h;

    /* Compute SoR and invRoS */
    bn_mat_mul(ehy->ehy_SoR, S, R);
    bn_mat_mul(ehy->ehy_invRoS, Rinv, S);

    /* Compute bounding sphere */
    /* bounding sphere center */
    VJOIN1(stp->st_center, ehy->ehy_V, mag_h / 2.0, ehy->ehy_Hunit);
    /* bounding radius */
    stp->st_bradius = sqrt(0.25*magsq_h + r2*r2 + r1*r1);
    /* approximate bounding radius */
    stp->st_aradius = stp->st_bradius;

    if (rt_ehy_bbox(ip, &(stp->st_min), &(stp->st_max))) return 1;

    return 0;			/* OK */
}


/**
 * R T _ E H Y _ P R I N T
 */
void
rt_ehy_print(const struct soltab *stp)
{
    const struct ehy_specific *ehy =
	(struct ehy_specific *)stp->st_specific;

    VPRINT("V", ehy->ehy_V);
    VPRINT("Hunit", ehy->ehy_Hunit);
    VPRINT("Aunit", ehy->ehy_Aunit);
    VPRINT("Bunit", ehy->ehy_Bunit);
    fprintf(stderr, "c = %g\n", ehy->ehy_cprime);
    bn_mat_print("S o R", ehy->ehy_SoR);
    bn_mat_print("invR o S", ehy->ehy_invRoS);
}


/* hit_surfno is set to one of these */
#define EHY_NORM_BODY (1)		/* compute normal */
#define EHY_NORM_TOP (2)		/* copy ehy_N */


/**
 * R T _ E H Y _ S H O T
 *
 * Intersect a ray with a ehy.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_ehy_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct ehy_specific *ehy =
	(struct ehy_specific *)stp->st_specific;
    vect_t dp;		/* D' */
    vect_t pp;		/* P' */
    fastf_t k1, k2;	/* distance constants of solution */
    fastf_t cp;		/* c' */
    vect_t xlated;	/* translated vector */
    struct hit hits[3];	/* 2 potential hit points */
    struct hit *hitp;	/* pointer to hit point */

    /* for finding roots */
    fastf_t a, b, c;	/* coeffs of polynomial */
    fastf_t disc;	/* discriminant */

    hitp = &hits[0];

    /* out, Mat, vect */
    MAT4X3VEC(dp, ehy->ehy_SoR, rp->r_dir);
    VSUB2(xlated, rp->r_pt, ehy->ehy_V);
    MAT4X3VEC(pp, ehy->ehy_SoR, xlated);

    cp = ehy->ehy_cprime;

    /* Find roots of the equation, using formula for quadratic */

    a = dp[Z] * dp[Z]
	- (2 * cp + 1) * (dp[X] * dp[X] + dp[Y] * dp[Y]);
    b = 2.0 * (dp[Z] * (pp[Z] + cp + 1)
	       - (2 * cp + 1) * (dp[X] * pp[X] + dp[Y] * pp[Y]));
    c = pp[Z] * pp[Z]
	- (2 * cp + 1) * (pp[X] * pp[X] + pp[Y] * pp[Y] - 1.0)
	+ 2 * (cp + 1) * pp[Z];
    if (!NEAR_ZERO(a, RT_PCOEF_TOL)) {
	disc = b*b - 4 * a * c;
	if (disc <= 0)
	    goto check_plates;
	disc = sqrt(disc);

	k1 = (-b + disc) / (2.0 * a);
	k2 = (-b - disc) / (2.0 * a);

	/*
	 * k1 and k2 are potential solutions to intersection with
	 * side.  See if they fall in range.
	 */
	VJOIN1(hitp->hit_vpriv, pp, k1, dp);	/* hit' */
	if (hitp->hit_vpriv[Z] >= -1.0
	    && hitp->hit_vpriv[Z] <= 0.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = EHY_NORM_BODY;	/* compute N */
	    hitp++;
	}

	VJOIN1(hitp->hit_vpriv, pp, k2, dp);	/* hit' */
	if (hitp->hit_vpriv[Z] >= -1.0
	    && hitp->hit_vpriv[Z] <= 0.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k2;
	    hitp->hit_surfno = EHY_NORM_BODY;	/* compute N */
	    hitp++;
	}
    } else if (!NEAR_ZERO(b, RT_PCOEF_TOL)) {
	k1 = -c/b;
	VJOIN1(hitp->hit_vpriv, pp, k1, dp);	/* hit' */
	if (hitp->hit_vpriv[Z] >= -1.0
	    && hitp->hit_vpriv[Z] <= 0.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = EHY_NORM_BODY;	/* compute N */
	    hitp++;
	}
    }


    /*
     * Check for hitting the top plate.
     */
 check_plates:
    /* check top plate */
    if (hitp == &hits[1]  &&  !ZERO(dp[Z])) {
	/* 1 hit so far, this is worthwhile */
	k1 = -pp[Z] / dp[Z];		/* top plate */

	VJOIN1(hitp->hit_vpriv, pp, k1, dp);	/* hit' */
	if (hitp->hit_vpriv[X] * hitp->hit_vpriv[X] +
	    hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y] <= 1.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = EHY_NORM_TOP;	/* -H */
	    hitp++;
	}
    }

    if (hitp != &hits[2])
	return 0;	/* MISS */

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
 * R T _ E H Y _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_ehy_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    vect_t can_normal;	/* normal to canonical ehy */
    fastf_t cp, scale;
    struct ehy_specific *ehy =
	(struct ehy_specific *)stp->st_specific;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    switch (hitp->hit_surfno) {
	case EHY_NORM_BODY:
	    cp = ehy->ehy_cprime;
	    VSET(can_normal,
		 hitp->hit_vpriv[X] * (2 * cp + 1),
		 hitp->hit_vpriv[Y] * (2 * cp + 1),
		 -(hitp->hit_vpriv[Z] + cp + 1));
	    MAT4X3VEC(hitp->hit_normal, ehy->ehy_invRoS, can_normal);
	    scale = 1.0 / MAGNITUDE(hitp->hit_normal);
	    VSCALE(hitp->hit_normal, hitp->hit_normal, scale);

	    /* tuck away this scale for the curvature routine */
	    hitp->hit_vpriv[X] = scale;
	    break;
	case EHY_NORM_TOP:
	    VREVERSE(hitp->hit_normal, ehy->ehy_Hunit);
	    break;
	default:
	    bu_log("rt_ehy_norm: surfno=%d bad\n", hitp->hit_surfno);
	    break;
    }
}


/**
 * R T _ E H Y _ C U R V E
 *
 * Return the curvature of the ehy.
 */
void
rt_ehy_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    fastf_t a, b, c, scale;
    mat_t M1, M2;
    struct ehy_specific *ehy =
	(struct ehy_specific *)stp->st_specific;
    vect_t u, v;			/* basis vectors (with normal) */
    vect_t vec1, vec2;		/* eigen vectors */
    vect_t tmp;

    switch (hitp->hit_surfno) {
	case EHY_NORM_BODY:
	    /*
	     * choose a tangent plane coordinate system (u, v, normal)
	     * form a right-handed triple
	     */
	    bn_vec_ortho(u, hitp->hit_normal);
	    VCROSS(v, hitp->hit_normal, u);

	    /* get the saved away scale factor */
	    scale = - hitp->hit_vpriv[X];

	    MAT_IDN(M1);
	    M1[0] = M1[5] = (4 * ehy->ehy_cprime + 2)
		/ (ehy->ehy_cprime * ehy->ehy_cprime);
	    M1[10] = -1.;
	    /* M1 = invR * S * M1 * S * R */
	    bn_mat_mul(M2, ehy->ehy_invRoS, M1);
	    bn_mat_mul(M1, M2, ehy->ehy_SoR);

	    /* find the second fundamental form */
	    MAT4X3VEC(tmp, ehy->ehy_invRoS, u);
	    a = VDOT(u, tmp) * scale;
	    b = VDOT(v, tmp) * scale;
	    MAT4X3VEC(tmp, ehy->ehy_invRoS, v);
	    c = VDOT(v, tmp) * scale;

	    bn_eigen2x2(&cvp->crv_c1, &cvp->crv_c2, vec1, vec2, a, b, c);
	    VCOMB2(cvp->crv_pdir, vec1[X], u, vec1[Y], v);
	    VUNITIZE(cvp->crv_pdir);
	    break;
	case EHY_NORM_TOP:
	    cvp->crv_c1 = cvp->crv_c2 = 0;
	    /* any tangent direction */
	    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
	    break;
    }

}


/**
 * R T _ E H Y _ U V
 *
 * For a hit on the surface of an ehy, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 *
 * u = azimuth
 * v = elevation
 */
void
rt_ehy_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    struct ehy_specific *ehy =
	(struct ehy_specific *)stp->st_specific;
    vect_t work;
    vect_t pprime;
    fastf_t len;

    if (ap) RT_CK_APPLICATION(ap);

    /*
     * hit_point is on surface; project back to unit ehy, creating a
     * vector from vertex to hit point.
     */
    VSUB2(work, hitp->hit_point, ehy->ehy_V);
    MAT4X3VEC(pprime, ehy->ehy_SoR, work);

    switch (hitp->hit_surfno) {
	case EHY_NORM_BODY:
	    /* top plate, polar coords */
	    if (ZERO(pprime[Z] + 1.0)) { /* i.e., == -1.0 */
		/* bottom pt of body */
		uvp->uv_u = 0;
	    } else {
		len = sqrt(pprime[X]*pprime[X] + pprime[Y]*pprime[Y]);
		uvp->uv_u = acos(pprime[X]/len) * bn_inv2pi;
	    }
	    uvp->uv_v = -pprime[Z];
	    break;
	case EHY_NORM_TOP:
	    /* top plate, polar coords */
	    len = sqrt(pprime[X]*pprime[X] + pprime[Y]*pprime[Y]);
	    uvp->uv_u = acos(pprime[X]/len) * bn_inv2pi;
	    uvp->uv_v = 1.0 - len;
	    break;
    }
    /* Handle other half of acos() domain */
    if (pprime[Y] < 0)
	uvp->uv_u = 1.0 - uvp->uv_u;

    /* uv_du should be relative to rotation, uv_dv relative to height
     */
    uvp->uv_du = uvp->uv_dv = 0;
}


/**
 * R T _ E H Y _ F R E E
 */
void
rt_ehy_free(struct soltab *stp)
{
    struct ehy_specific *ehy =
	(struct ehy_specific *)stp->st_specific;


    bu_free((char *)ehy, "ehy_specific");
}


/**
 * R T _ E H Y _ C L A S S
 */
int
rt_ehy_class(void)
{
    return 0;
}


/**
 * R T _ E H Y _ P L O T
 */
int
rt_ehy_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
{
    fastf_t c, dtol, f, mag_a, mag_h, ntol, r1, r2;
    fastf_t **ellipses, theta_prev, theta_new;
    int *pts_dbl, i, j, nseg;
    int jj, na, nb, nell, recalc_b;
    mat_t R;
    mat_t invR;
    point_t p1;
    struct rt_pt_node *pos_a, *pos_b, *pts_a, *pts_b;
    struct rt_ehy_internal *xip;
    vect_t A, Au, B, Bu, Hu, V, Work;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    xip = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(xip);

    /*
     * make sure ehy description is valid
     */

    /* compute |A| |H| */
    mag_a = MAGSQ(xip->ehy_Au);	/* should already be unit vector */
    mag_h = MAGNITUDE(xip->ehy_H);
    c = xip->ehy_c;
    r1 = xip->ehy_r1;
    r2 = xip->ehy_r2;
    /* Check for |H| > 0, |A| == 1, r1 > 0, r2 > 0, c > 0 */
    if (NEAR_ZERO(mag_h, RT_LEN_TOL)
	|| !NEAR_EQUAL(mag_a, 1.0, RT_LEN_TOL)
	|| r1 <= 0.0 || r2 <= 0.0 || c <= 0.) {
	return -2;		/* BAD */
    }

    /* Check for A.H == 0 */
    f = VDOT(xip->ehy_Au, xip->ehy_H) / mag_h;
    if (! NEAR_ZERO(f, RT_DOT_TOL)) {
	return -2;		/* BAD */
    }

    /* make unit vectors in A, H, and BxH directions */
    VMOVE(Hu, xip->ehy_H);
    VUNITIZE(Hu);
    VMOVE(Au, xip->ehy_Au);
    VCROSS(Bu, Au, Hu);

    /* Compute R and Rinv matrices */
    MAT_IDN(R);
    VREVERSE(&R[0], Bu);
    VMOVE(&R[4], Au);
    VREVERSE(&R[8], Hu);
    bn_mat_trn(invR, R);			/* inv of rot mat is trn */

    /*
     * Establish tolerances
     */
    if (ttol->rel <= 0.0 || ttol->rel >= 1.0)
	dtol = 0.0;		/* none */
    else
	/* Convert rel to absolute by scaling by smallest side */
	dtol = ttol->rel * 2 * r2;
    if (ttol->abs <= 0.0) {
	if (dtol <= 0.0) {
	    /* No tolerance given, use a default */
	    dtol = 2 * 0.10 * r2;	/* 10% */
	}
	/* Use absolute-ized relative tolerance */
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

    /*
     * build ehy from 2 hyperbolas
     */

    /* approximate positive half of hyperbola along semi-minor axis */
    pts_b = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
    pts_b->next = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
    pts_b->next->next = NULL;
    VSET(pts_b->p,       0, 0, -mag_h);
    VSET(pts_b->next->p, 0, r2, 0);
    /* 2 endpoints in 1st approximation */
    nb = 2;
    /* recursively break segment 'til within error tolerances */
    nb += rt_mk_hyperbola(pts_b, r2, mag_h, c, dtol, ntol);
    nell = nb - 1;	/* # of ellipses needed */

    /* construct positive half of hyperbola along semi-major axis of
     * ehy using same z coords as hyperbola along semi-minor axis
     */
    pts_a = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
    VMOVE(pts_a->p, pts_b->p);	/* 1st pt is the apex */
    pts_a->next = NULL;
    pos_b = pts_b->next;
    pos_a = pts_a;
    while (pos_b) {
	/* copy node from b_hyperbola to a_hyperbola */
	pos_a->next = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
	pos_a = pos_a->next;
	pos_a->p[Z] = pos_b->p[Z];
	/* at given z, find y on hyperbola */
	pos_a->p[Y] = r1
	    * sqrt(
		((pos_b->p[Z] + mag_h + c)
		 * (pos_b->p[Z] + mag_h + c) - c*c)
		/ (mag_h*(mag_h + 2*c))
		);
	pos_a->p[X] = 0;
	pos_b = pos_b->next;
    }
    pos_a->next = NULL;

    /* see if any segments need further breaking up */
    recalc_b = 0;
    pos_a = pts_a;
    while (pos_a->next) {
	na = rt_mk_hyperbola(pos_a, r1, mag_h, c, dtol, ntol);
	if (na != 0) {
	    recalc_b = 1;
	    nell += na;
	}
	pos_a = pos_a->next;
    }
    /* if any were broken, recalculate hyperbola 'a' */
    if (recalc_b) {
	/* free mem for old approximation of hyperbola */
	pos_b = pts_b;
	while (pos_b) {
	    struct rt_pt_node *next;

	    /* get next node before freeing */
	    next = pos_b->next;
	    bu_free((char *)pos_b, "rt_pt_node");
	    pos_b = next;
	}
	/* construct hyperbola along semi-major axis of ehy using same
	 * z coords as parab along semi-minor axis
	 */
	pts_b = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
	pts_b->p[Z] = pts_a->p[Z];
	pts_b->next = NULL;
	pos_a = pts_a->next;
	pos_b = pts_b;
	while (pos_a) {
	    /* copy node from a_hyperbola to b_hyperbola */
	    pos_b->next = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
	    pos_b = pos_b->next;
	    pos_b->p[Z] = pos_a->p[Z];
	    /* at given z, find y on hyperbola */
	    pos_b->p[Y] = r2
		* sqrt(
		    ((pos_a->p[Z] + mag_h + c)
		     * (pos_a->p[Z] + mag_h + c) - c*c)
		    / (mag_h*(mag_h + 2*c))
		    );
	    pos_b->p[X] = 0;
	    pos_a = pos_a->next;
	}
	pos_b->next = NULL;
    }

    /* make array of ptrs to ehy ellipses */
    ellipses = (fastf_t **)bu_malloc(nell * sizeof(fastf_t *), "fastf_t ell[]");
    /* keep track of whether pts in each ellipse are doubled or not */
    pts_dbl = (int *)bu_malloc(nell * sizeof(int), "dbl ints");

    /* make ellipses at each z level */
    i = 0;
    nseg = 0;
    theta_prev = bn_twopi;
    pos_a = pts_a->next;	/* skip over apex of ehy */
    pos_b = pts_b->next;
    while (pos_a) {
	VSCALE(A, Au, pos_a->p[Y]);	/* semimajor axis */
	VSCALE(B, Bu, pos_b->p[Y]);	/* semiminor axis */
	VJOIN1(V, xip->ehy_V, -pos_a->p[Z], Hu);

	VSET(p1, 0., pos_b->p[Y], 0.);
	theta_new = ell_angle(p1, pos_a->p[Y], pos_b->p[Y], dtol, ntol);
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
	rt_ell(ellipses[i], V, A, B, nseg);

	i++;
	pos_a = pos_a->next;
	pos_b = pos_b->next;
    }

    /* Draw the top ellipse */
    RT_ADD_VLIST(vhead,
		 &ellipses[nell-1][(nseg-1)*ELEMENTS_PER_VECT],
		 BN_VLIST_LINE_MOVE);
    for (i = 0; i < nseg; i++) {
	RT_ADD_VLIST(vhead,
		     &ellipses[nell-1][i*ELEMENTS_PER_VECT],
		     BN_VLIST_LINE_DRAW);
    }

    /* connect ellipses */
    for (i = nell-2; i >= 0; i--) {
	/* skip top ellipse */
	int bottom, top;

	top = i + 1;
	bottom = i;
	if (pts_dbl[top])
	    nseg /= 2;	/* # segs in 'bottom' ellipse */

	/* Draw the current ellipse */
	RT_ADD_VLIST(vhead,
		     &ellipses[bottom][(nseg-1)*ELEMENTS_PER_VECT],
		     BN_VLIST_LINE_MOVE);
	for (j = 0; j < nseg; j++) {
	    RT_ADD_VLIST(vhead,
			 &ellipses[bottom][j*ELEMENTS_PER_VECT],
			 BN_VLIST_LINE_DRAW);
	}

	/* make connections between ellipses */
	for (j = 0; j < nseg; j++) {
	    if (pts_dbl[top])
		jj = j + j;	/* top ellipse index */
	    else
		jj = j;
	    RT_ADD_VLIST(vhead,
			 &ellipses[bottom][j*ELEMENTS_PER_VECT],
			 BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead,
			 &ellipses[top][jj*ELEMENTS_PER_VECT],
			 BN_VLIST_LINE_DRAW);
	}
    }

    VADD2(Work, xip->ehy_V, xip->ehy_H);
    for (i = 0; i < nseg; i++) {
	/* Draw connector */
	RT_ADD_VLIST(vhead, Work, BN_VLIST_LINE_MOVE);
	RT_ADD_VLIST(vhead,
		     &ellipses[0][i*ELEMENTS_PER_VECT],
		     BN_VLIST_LINE_DRAW);
    }

    /* free mem */
    for (i = 0; i < nell; i++) {
	bu_free((char *)ellipses[i], "pts ell");
    }
    bu_free((char *)ellipses, "fastf_t ell[]");
    bu_free((char *)pts_dbl, "dbl ints");

    return 0;
}


/**
 * R T _ E H Y _ T E S S
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_ehy_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    fastf_t c, dtol, f, mag_a, mag_h, ntol, r1, r2, cprime;
    fastf_t **ellipses, theta_prev, theta_new;
    int *pts_dbl, face, i, j, nseg;
    int jj, na, nb, nell, recalc_b;
    mat_t R;
    mat_t invR;
    mat_t invRoS;
    mat_t S;
    mat_t SoR;
    struct rt_ehy_internal *xip;
    point_t p1;
    struct rt_pt_node *pos_a, *pos_b, *pts_a, *pts_b;
    struct shell *s;
    struct faceuse **outfaceuses = NULL;
    struct faceuse *fu_top;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct vertex *vertp[3];
    struct vertex ***vells = (struct vertex ***)NULL;
    vect_t A, Au, B, Bu, Hu, V;
    struct bu_ptbl vert_tab;

    RT_CK_DB_INTERNAL(ip);
    xip = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(xip);

    /*
     * make sure ehy description is valid
     */

    /* compute |A| |H| */
    mag_a = MAGSQ(xip->ehy_Au);	/* should already be unit vector */
    mag_h = MAGNITUDE(xip->ehy_H);
    c = xip->ehy_c;
    cprime = c / mag_h;
    r1 = xip->ehy_r1;
    r2 = xip->ehy_r2;
    /* Check for |H| > 0, |A| == 1, r1 > 0, r2 > 0, c > 0 */
    if (NEAR_ZERO(mag_h, RT_LEN_TOL)
	|| !NEAR_EQUAL(mag_a, 1.0, RT_LEN_TOL)
	|| r1 <= 0.0 || r2 <= 0.0 || c <= 0.) {
	return 1;		/* BAD */
    }

    /* Check for A.H == 0 */
    f = VDOT(xip->ehy_Au, xip->ehy_H) / mag_h;
    if (! NEAR_ZERO(f, RT_DOT_TOL)) {
	return 1;		/* BAD */
    }

    /* make unit vectors in A, H, and BxH directions */
    VMOVE(Hu, xip->ehy_H);
    VUNITIZE(Hu);
    VMOVE(Au, xip->ehy_Au);
    VCROSS(Bu, Au, Hu);

    /* Compute R and Rinv matrices */
    MAT_IDN(R);
    VREVERSE(&R[0], Bu);
    VMOVE(&R[4], Au);
    VREVERSE(&R[8], Hu);
    bn_mat_trn(invR, R);			/* inv of rot mat is trn */

    /* Compute S */
    MAT_IDN(S);
    S[ 0] = 1.0/r2;
    S[ 5] = 1.0/r1;
    S[10] = 1.0/mag_h;

    /* Compute SoR and invRoS */
    bn_mat_mul(SoR, S, R);
    bn_mat_mul(invRoS, invR, S);

    /*
     * Establish tolerances
     */
    if (ttol->rel <= 0.0 || ttol->rel >= 1.0)
	dtol = 0.0;		/* none */
    else
	/* Convert rel to absolute by scaling by smallest side */
	dtol = ttol->rel * 2 * r2;
    if (ttol->abs <= 0.0) {
	if (dtol <= 0.0) {
	    /* No tolerance given, use a default */
	    dtol = 2 * 0.10 * r2;	/* 10% */
	}
	/* Use absolute-ized relative tolerance */
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

    /*
     * build ehy from 2 hyperbolas
     */

    /* approximate positive half of hyperbola along semi-minor axis */
    pts_b = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
    pts_b->next = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
    pts_b->next->next = NULL;
    VSET(pts_b->p,       0, 0, -mag_h);
    VSET(pts_b->next->p, 0, r2, 0);
    /* 2 endpoints in 1st approximation */
    nb = 2;
    /* recursively break segment 'til within error tolerances */
    nb += rt_mk_hyperbola(pts_b, r2, mag_h, c, dtol, ntol);
    nell = nb - 1;	/* # of ellipses needed */

    /* construct positive half of hyperbola along semi-major axis of
     * ehy using same z coords as parab along semi-minor axis
     */
    pts_a = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
    VMOVE(pts_a->p, pts_b->p);	/* 1st pt is the apex */
    pts_a->next = NULL;
    pos_b = pts_b->next;
    pos_a = pts_a;
    while (pos_b) {
	/* copy node from b_hyperbola to a_hyperbola */
	pos_a->next = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
	pos_a = pos_a->next;
	pos_a->p[Z] = pos_b->p[Z];
	/* at given z, find y on hyperbola */
	pos_a->p[Y] = r1
	    * sqrt(
		((pos_b->p[Z] + mag_h + c)
		 * (pos_b->p[Z] + mag_h + c) - c*c)
		/ (mag_h*(mag_h + 2*c))
		);
	pos_a->p[X] = 0;
	pos_b = pos_b->next;
    }
    pos_a->next = NULL;

    /* see if any segments need further breaking up */
    recalc_b = 0;
    pos_a = pts_a;
    while (pos_a->next) {
	na = rt_mk_hyperbola(pos_a, r1, mag_h, c, dtol, ntol);
	if (na != 0) {
	    recalc_b = 1;
	    nell += na;
	}
	pos_a = pos_a->next;
    }
    /* if any were broken, recalculate hyperbola 'a' */
    if (recalc_b) {
	/* free mem for old approximation of hyperbola */
	pos_b = pts_b;
	while (pos_b) {
	    struct rt_pt_node *tmp;

	    tmp = pos_b->next;
	    bu_free((char *)pos_b, "rt_pt_node");
	    pos_b = tmp;
	}
	/* construct hyperbola along semi-major axis of ehy using same
	 * z coords as parab along semi-minor axis
	 */
	pts_b = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
	pts_b->p[Z] = pts_a->p[Z];
	pts_b->next = NULL;
	pos_a = pts_a->next;
	pos_b = pts_b;
	while (pos_a) {
	    /* copy node from a_hyperbola to b_hyperbola */
	    pos_b->next = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
	    pos_b = pos_b->next;
	    pos_b->p[Z] = pos_a->p[Z];
	    /* at given z, find y on hyperbola */
	    pos_b->p[Y] = r2
		* sqrt(
		    ((pos_a->p[Z] + mag_h + c)
		     * (pos_a->p[Z] + mag_h + c) - c*c)
		    / (mag_h*(mag_h + 2*c))
		    );
	    pos_b->p[X] = 0;
	    pos_a = pos_a->next;
	}
	pos_b->next = NULL;
    }

    /* make array of ptrs to ehy ellipses */
    ellipses = (fastf_t **)bu_malloc(nell * sizeof(fastf_t *), "fastf_t ell[]");

    /* keep track of whether pts in each ellipse are doubled or not */
    pts_dbl = (int *)bu_malloc(nell * sizeof(int), "dbl ints");

    /* make ellipses at each z level */
    i = 0;
    nseg = 0;
    theta_prev = bn_twopi;
    pos_a = pts_a->next;	/* skip over apex of ehy */
    pos_b = pts_b->next;
    while (pos_a) {
	VSCALE(A, Au, pos_a->p[Y]);	/* semimajor axis */
	VSCALE(B, Bu, pos_b->p[Y]);	/* semiminor axis */
	VJOIN1(V, xip->ehy_V, -pos_a->p[Z], Hu);

	VSET(p1, 0., pos_b->p[Y], 0.);
	theta_new = ell_angle(p1, pos_a->p[Y], pos_b->p[Y], dtol, ntol);
	if (nseg == 0) {
	    nseg = (int)(bn_twopi / theta_new) + 1;
	    pts_dbl[i] = 0;
	    /* maximum number of faces needed for ehy */
	    face = nseg*(1 + 3*((1 << (nell-1)) - 1));
	    /* array for each triangular face */
	    outfaceuses = (struct faceuse **)
		bu_malloc((face+1) * sizeof(struct faceuse *), "ehy: *outfaceuses[]");
	} else if (theta_new < theta_prev) {
	    nseg *= 2;
	    pts_dbl[i] = 1;
	} else {
	    pts_dbl[i] = 0;
	}
	theta_prev = theta_new;

	ellipses[i] = (fastf_t *)bu_malloc(3*(nseg+1)*sizeof(fastf_t),
					   "pts ell");
	rt_ell(ellipses[i], V, A, B, nseg);

	i++;
	pos_a = pos_a->next;
	pos_b = pos_b->next;
    }

    /*
     * put ehy geometry into nmg data structures
     */

    *r = nmg_mrsv(m);	/* Make region, empty shell, vertex */
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    /* vertices of ellipses of ehy */
    vells = (struct vertex ***)
	bu_malloc(nell*sizeof(struct vertex **), "vertex [][]");
    j = nseg;
    for (i = nell-1; i >= 0; i--) {
	vells[i] = (struct vertex **)
	    bu_malloc(j*sizeof(struct vertex *), "vertex []");
	if (i && pts_dbl[i])
	    j /= 2;
    }

    /* top face of ehy */
    for (i = 0; i < nseg; i++)
	vells[nell-1][i] = (struct vertex *)NULL;
    face = 0;
    BU_ASSERT_PTR(outfaceuses, !=, NULL);
    if ((outfaceuses[face++] = nmg_cface(s, vells[nell-1], nseg)) == 0) {
	bu_log("rt_ehy_tess() failure, top face\n");
	goto fail;
    }
    fu_top = outfaceuses[0];

    /* Mark edges of this face as real, this is the only real edge */
    for (BU_LIST_FOR(lu, loopuse, &outfaceuses[0]->lu_hd)) {
	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    struct edge *e;

	    NMG_CK_EDGEUSE(eu);
	    e = eu->e_p;
	    NMG_CK_EDGE(e);
	    e->is_real = 1;
	}
    }

    for (i = 0; i < nseg; i++) {
	NMG_CK_VERTEX(vells[nell-1][i]);
	nmg_vertex_gv(vells[nell-1][i], &ellipses[nell-1][3*i]);
    }

    /* connect ellipses with triangles */
    for (i = nell-2; i >= 0; i--) {
	/* skip top ellipse */
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
		if ((outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
		    bu_log("rt_ehy_tess() failure\n");
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
		if ((outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
		    bu_log("rt_ehy_tess() failure\n");
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
		if ((outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
		    bu_log("rt_ehy_tess() failure\n");
		    goto fail;
		}
	    } else {
		/* first triangle */
		if (j == nseg-1)
		    vertp[1] = vells[top][0];
		else
		    vertp[1] = vells[top][j+1];
		vertp[2] = vells[top][j];
		if ((outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
		    bu_log("rt_ehy_tess() failure\n");
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
		if ((outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
		    bu_log("rt_ehy_tess() failure\n");
		    goto fail;
		}
		if (j != nseg-1)
		    vells[bottom][j+1] = vertp[0];
	    }
	}

	/* associate geometry with each vertex */
	for (j = 0; j < nseg; j++) {
	    NMG_CK_VERTEX(vells[bottom][j]);
	    nmg_vertex_gv(vells[bottom][j],
			  &ellipses[bottom][3*j]);
	}
    }

    /* connect bottom of ellipse to apex of ehy */
    VADD2(V, xip->ehy_V, xip->ehy_H);
    vertp[0] = (struct vertex *)0;
    vertp[1] = vells[0][1];
    vertp[2] = vells[0][0];
    if ((outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
	bu_log("rt_ehy_tess() failure\n");
	goto fail;
    }
    /* associate geometry with topology */
    NMG_CK_VERTEX(vertp[0]);
    nmg_vertex_gv(vertp[0], (fastf_t *)V);
    /* create rest of faces around apex */
    for (i = 1; i < nseg; i++) {
	vertp[2] = vertp[1];
	if (i == nseg-1)
	    vertp[1] = vells[0][0];
	else
	    vertp[1] = vells[0][i+1];
	if ((outfaceuses[face++] = nmg_cface(s, vertp, 3)) == 0) {
	    bu_log("rt_ehy_tess() failure\n");
	    goto fail;
	}
    }

    /* Associate the face geometry */
    for (i=0; i < face; i++) {
	if (nmg_fu_planeeqn(outfaceuses[i], tol) < 0)
	    goto fail;
    }

    /* Glue the edges of different outward pointing face uses together */
    nmg_gluefaces(outfaceuses, face, tol);

    /* Compute "geometry" for region and shell */
    nmg_region_a(*r, tol);

    /* XXX just for testing, to make up for loads of triangles ... */
    nmg_shell_coplanar_face_merge(s, tol, 1);

    /* free mem */
    bu_free((char *)outfaceuses, "faceuse []");
    for (i = 0; i < nell; i++) {
	bu_free((char *)ellipses[i], "pts ell");
	bu_free((char *)vells[i], "vertex []");
    }
    bu_free((char *)ellipses, "fastf_t ell[]");
    bu_free((char *)vells, "vertex [][]");

    /* Assign vertexuse normals */
    nmg_vertex_tabulate(&vert_tab, &s->l.magic);
    for (i=0; i<BU_PTBL_END(&vert_tab); i++) {
	point_t pt_prime, tmp_pt;
	vect_t norm, rev_norm, tmp_vect;
	struct vertex_g *vg;
	struct vertex *v;
	struct vertexuse *vu;

	v = (struct vertex *)BU_PTBL_GET(&vert_tab, i);
	NMG_CK_VERTEX(v);
	vg = v->vg_p;
	NMG_CK_VERTEX_G(vg);

	VSUB2(tmp_pt, vg->coord, xip->ehy_V);
	MAT4X3VEC(pt_prime, SoR, tmp_pt);
	VSET(tmp_vect, pt_prime[X]*(2*cprime+1), pt_prime[Y]*(2*cprime+1), -(pt_prime[Z]+cprime+1));
	MAT4X3VEC(norm, invRoS, tmp_vect);
	VUNITIZE(norm);
	VREVERSE(rev_norm, norm);

	for (BU_LIST_FOR(vu, vertexuse, &v->vu_hd)) {
	    struct faceuse *fu;

	    NMG_CK_VERTEXUSE(vu);
	    fu = nmg_find_fu_of_vu(vu);

	    /* don't assign vertexuse normals to top face (flat) */
	    if (fu == fu_top || fu->fumate_p == fu_top)
		continue;

	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation == OT_SAME)
		nmg_vertexuse_nv(vu, norm);
	    else if (fu->orientation == OT_OPPOSITE)
		nmg_vertexuse_nv(vu, rev_norm);
	}
    }

    bu_ptbl_free(&vert_tab);
    return 0;

 fail:
    /* free mem */
    bu_free((char *)outfaceuses, "faceuse []");
    for (i = 0; i < nell; i++) {
	bu_free((char *)ellipses[i], "pts ell");
	bu_free((char *)vells[i], "vertex []");
    }
    bu_free((char *)ellipses, "fastf_t ell[]");
    bu_free((char *)vells, "vertex [][]");

    return -1;
}


/**
 * R T _ E H Y _ I M P O R T
 *
 * Import an EHY from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_ehy_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_ehy_internal *xip;
    union record *rp;
    vect_t v1, v2, v3;

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != ID_SOLID) {
	bu_log("rt_ehy_import4: defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_EHY;
    ip->idb_meth = &rt_functab[ID_EHY];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_ehy_internal), "rt_ehy_internal");
    xip = (struct rt_ehy_internal *)ip->idb_ptr;
    xip->ehy_magic = RT_EHY_INTERNAL_MAGIC;

    /* Warning:  type conversion */
    if (mat == NULL) mat = bn_mat_identity;

    if (dbip->dbi_version < 0) {
	flip_fastf_float(v1, &rp->s.s_values[0*3], 1, 1);
	flip_fastf_float(v2, &rp->s.s_values[1*3], 1, 1);
	flip_fastf_float(v3, &rp->s.s_values[2*3], 1, 1);
    } else {
	VMOVE(v1, &rp->s.s_values[0*3]);
	VMOVE(v2, &rp->s.s_values[1*3]);
	VMOVE(v3, &rp->s.s_values[2*3]);
    }

    MAT4X3PNT(xip->ehy_V, mat, v1);
    MAT4X3VEC(xip->ehy_H, mat, v2);
    MAT4X3VEC(xip->ehy_Au, mat, v3);

    VUNITIZE(xip->ehy_Au);

    if (dbip->dbi_version < 0) {
	v1[X] = flip_dbfloat(rp->s.s_values[3*3+0]);
	v1[Y] = flip_dbfloat(rp->s.s_values[3*3+1]);
	v1[Z] = flip_dbfloat(rp->s.s_values[3*3+2]);
    } else {
	v1[X] = rp->s.s_values[3*3+0];
	v1[Y] = rp->s.s_values[3*3+1];
	v1[Z] = rp->s.s_values[3*3+2];
    }

    xip->ehy_r1 = v1[X] / mat[15];
    xip->ehy_r2 = v1[Y] / mat[15];
    xip->ehy_c  = v1[Z] / mat[15];

    if (xip->ehy_r1 <= SMALL_FASTF || xip->ehy_r2 <= SMALL_FASTF || xip->ehy_c <= SMALL_FASTF) {
	bu_log("rt_ehy_import4: r1, r2, or c are zero\n");
	bu_free((char *)ip->idb_ptr, "rt_ehy_import4: ip->idb_ptr");
	return -1;
    }

    return 0;			/* OK */
}


/**
 * R T _ E H Y _ E X P O R T
 *
 * The name is added by the caller, in the usual place.
 */
int
rt_ehy_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_ehy_internal *xip;
    union record *ehy;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_EHY) return -1;
    xip = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(xip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record);
    ep->ext_buf = (genptr_t)bu_calloc(1, ep->ext_nbytes, "ehy external");
    ehy = (union record *)ep->ext_buf;

    ehy->s.s_id = ID_SOLID;
    ehy->s.s_type = EHY;

    if (!NEAR_EQUAL(MAGNITUDE(xip->ehy_Au), 1.0, RT_LEN_TOL)) {
	bu_log("rt_ehy_export4: Au not a unit vector!\n");
	return -1;
    }

    if (MAGNITUDE(xip->ehy_H) < RT_LEN_TOL
	|| xip->ehy_c < RT_LEN_TOL
	|| xip->ehy_r1 < RT_LEN_TOL
	|| xip->ehy_r2 < RT_LEN_TOL) {
	bu_log("rt_ehy_export4: not all dimensions positive!\n");
	return -1;
    }

    if (!NEAR_ZERO(VDOT(xip->ehy_Au, xip->ehy_H), RT_DOT_TOL)) {
	bu_log("rt_ehy_export4: Au and H are not perpendicular!\n");
	return -1;
    }

    if (xip->ehy_r2 > xip->ehy_r1) {
	bu_log("rt_ehy_export4: semi-minor axis cannot be longer than semi-major axis!\n");
	return -1;
    }

    /* Warning:  type conversion */
    VSCALE(&ehy->s.s_values[0*3], xip->ehy_V, local2mm);
    VSCALE(&ehy->s.s_values[1*3], xip->ehy_H, local2mm);
    /* don't scale ehy_Au (unit vector!!) */
    VMOVE(&ehy->s.s_values[2*3], xip->ehy_Au);
    ehy->s.s_values[3*3] = xip->ehy_r1 * local2mm;
    ehy->s.s_values[3*3+1] = xip->ehy_r2 * local2mm;
    ehy->s.s_values[3*3+2] = xip->ehy_c * local2mm;

    return 0;
}


/**
 * R T _ E H Y _ I M P O R T 5
 *
 * Import an EHY from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_ehy_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_ehy_internal *xip;
    fastf_t vec[3*4];

    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);

    BU_ASSERT_LONG(ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 3*4);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_EHY;
    ip->idb_meth = &rt_functab[ID_EHY];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_ehy_internal), "rt_ehy_internal");

    xip = (struct rt_ehy_internal *)ip->idb_ptr;
    xip->ehy_magic = RT_EHY_INTERNAL_MAGIC;

    /* Convert from database (network) to internal (host) format */
    ntohd((unsigned char *)vec, ep->ext_buf, 3*4);

    /* Apply modeling transformations */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(xip->ehy_V, mat, &vec[0*3]);
    MAT4X3VEC(xip->ehy_H, mat, &vec[1*3]);
    MAT4X3VEC(xip->ehy_Au, mat, &vec[2*3]);
    VUNITIZE(xip->ehy_Au);
    xip->ehy_r1 = vec[3*3] / mat[15];
    xip->ehy_r2 = vec[3*3+1] / mat[15];
    xip->ehy_c  = vec[3*3+2] / mat[15];

    if (xip->ehy_r1 <= SMALL_FASTF || xip->ehy_r2 <= SMALL_FASTF || xip->ehy_c <= SMALL_FASTF) {
	bu_log("rt_ehy_import5: r1, r2, or c are zero\n");
	bu_free((char *)ip->idb_ptr, "rt_ehy_import5: ip->idb_ptr");
	return -1;
    }

    return 0;			/* OK */
}


/**
 * R T _ E H Y _ E X P O R T 5
 *
 * The name is added by the caller, in the usual place.
 */
int
rt_ehy_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_ehy_internal *xip;
    fastf_t vec[3*4];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_EHY) return -1;
    xip = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(xip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * 3*4;
    ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes, "ehy external");

    if (!NEAR_EQUAL(MAGNITUDE(xip->ehy_Au), 1.0, RT_LEN_TOL)) {
	bu_log("rt_ehy_export4: Au not a unit vector!\n");
	return -1;
    }

    if (MAGNITUDE(xip->ehy_H) < RT_LEN_TOL
	|| xip->ehy_c < RT_LEN_TOL
	|| xip->ehy_r1 < RT_LEN_TOL
	|| xip->ehy_r2 < RT_LEN_TOL) {
	bu_log("rt_ehy_export4: not all dimensions positive!\n");
	return -1;
    }

    if (!NEAR_ZERO(VDOT(xip->ehy_Au, xip->ehy_H), RT_DOT_TOL)) {
	bu_log("rt_ehy_export4: Au and H are not perpendicular!\n");
	return -1;
    }

    if (xip->ehy_r2 > xip->ehy_r1) {
	bu_log("rt_ehy_export4: semi-minor axis cannot be longer than semi-major axis!\n");
	return -1;
    }

    /* Warning:  type conversion */
    VSCALE(&vec[0*3], xip->ehy_V, local2mm);
    VSCALE(&vec[1*3], xip->ehy_H, local2mm);
    /* don't scale ehy_Au (unit vector!!) */
    VMOVE(&vec[2*3], xip->ehy_Au);
    vec[3*3] = xip->ehy_r1 * local2mm;
    vec[3*3+1] = xip->ehy_r2 * local2mm;
    vec[3*3+2] = xip->ehy_c * local2mm;

    /* Convert from internal (host) to database (network) format */
    htond(ep->ext_buf, (unsigned char *)vec, 3*4);

    return 0;
}


/**
 * R T _ E H Y _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_ehy_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_ehy_internal *xip = (struct rt_ehy_internal *)ip->idb_ptr;
    char buf[256];

    RT_EHY_CK_MAGIC(xip);
    bu_vls_strcat(str, "Elliptical Hyperboloid (EHY)\n");

    sprintf(buf, "\tV (%g, %g, %g)\n",
	    INTCLAMP(xip->ehy_V[X] * mm2local),
	    INTCLAMP(xip->ehy_V[Y] * mm2local),
	    INTCLAMP(xip->ehy_V[Z] * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tH (%g, %g, %g) mag=%g\n",
	    INTCLAMP(xip->ehy_H[X] * mm2local),
	    INTCLAMP(xip->ehy_H[Y] * mm2local),
	    INTCLAMP(xip->ehy_H[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(xip->ehy_H) * mm2local));
    bu_vls_strcat(str, buf);

    if (!verbose)
	return 0;

    sprintf(buf, "\tA=%g\n", INTCLAMP(xip->ehy_r1 * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tB=%g\n", INTCLAMP(xip->ehy_r2 * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tc=%g\n", INTCLAMP(xip->ehy_c * mm2local));
    bu_vls_strcat(str, buf);

    return 0;
}


/**
 * R T _ E H Y _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_ehy_ifree(struct rt_db_internal *ip)
{
    struct rt_ehy_internal *xip;

    RT_CK_DB_INTERNAL(ip);

    xip = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(xip);
    xip->ehy_magic = 0;		/* sanity */

    bu_free((char *)xip, "ehy ifree");
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}


/**
 * R T _ E H Y _ P A R A M S
 *
 */
int
rt_ehy_params(struct pc_pc_set *ps, const struct rt_db_internal *ip)
{
    if (!ps) return 0;
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
