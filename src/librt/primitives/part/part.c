/*                          P A R T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2016 United States Government as represented by
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
/** @file primitives/part/part.c
 *
 * Intersect a ray with a "particle" solid, which can have three main
 * forms: sphere, hemisphere-tipped cylinder (lozenge), and
 * hemisphere-tipped cone.  This code draws on the examples of g_rec
 * (Davisson) & g_sph (Dykstra).
 *
 * Algorithm for the hemisphere-tipped cylinder and cone cases -
 *
 * Given V, H, vrad, and hrad, there is a set of points on this
 * cylinder
 *
 * { (x, y, z) | (x, y, z) is on cylinder }
 *
 * Through a series of Affine Transformations, this set of points will
 * be transformed into a set of points on a unit cylinder (or cone)
 * with the transformed base (V') located at the origin with a
 * transformed radius of 1 (vrad').  The height of the cylinder (or
 * cone) along the +Z axis is +1 (i.e., H' = (0, 0, 1)), with a
 * transformed radius of hrad/vrad.
 *
 *
 * { (x', y', z') | (x', y', z') is on cylinder at origin }
 *
 * The transformation from X to X' is accomplished by:
 *
 * finding two unit vectors A and B mutually perpendicular, and
 * perp. to H.
 *
 * X' = S(R(X - V))
 *
 * where R(X) rotates H to the +Z axis, and S(X) scales vrad' to 1 and
 * |H'| to 1.
 *
 * where R(X) = (A/(|A|))
 *  		(B/(|B|)) . X
 *  		(H/(|H|))
 *
 * and S(X) =	(1/|A|   0     0)
 * 		(0    1/|B|   0) . X
 * 		(0      0   1/|H|)
 *
 * To find the intersection of a line with the surface of the cylinder,
 * consider the parametric line L:
 *
 *  	L : { P(n) | P + t(n) . D }
 *
 * Call W the actual point of intersection between L and the cylinder.
 * Let W' be the point of intersection between L' and the unit cylinder.
 *
 *  	L' : { P'(n) | P' + t(n) . D' }
 *
 * W = invR(invS(W')) + V
 *
 * Where W' = k D' + P'.
 *
 * If Dx' and Dy' are both 0, then there is no hit on the cylinder;
 * but the end spheres need checking.
 *
 * The equation for the unit cylinder ranging along Z is
 *
 *	x**2 + y**2 - r**2 = 0
 *
 * and the equation for a unit cone ranging along Z is
 *
 *	x**2 + y**2 - f(z)**2 = 0
 *
 * where in this case f(z) linearly interpolates the radius of the
 * cylinder from vrad (r1) to hrad (r2) as z ranges from 0 to 1, i.e.:
 *
 *	f(z) = (r2-r1)/1 * z + r1
 *
 * let m = (r2-r1)/1, and substitute:
 *
 *	x**2 + y**2 - (m*z+r1)**2 = 0 .
 *
 * For the cylinder case, r1 == r2, so m == 0, and everything
 * simplifies.
 *
 * The parametric formulation for line L' is P' + t * D', or
 *
 *	x = Px' + t * Dx'
 *	y = Py' + t * Dy'
 *	z = Pz' + t * Dz' .
 *
 * Substituting these definitions into the formula for the unit cone gives
 *
 *	(Px'+t*Dx')**2 + (Py'+t*Dy')**2 + (m*(Pz'+t*Dz')+r1)**2 = 0
 *
 * Expanding and regrouping terms gives a quadratic in "t" which has
 * the form
 *
 *	a * t**2 + b * t + c = 0
 *
 * where
 *
 *	a = Dx'**2 + Dy'**2 - m**2 * Dz'**2
 *	b = 2 * (Px'*Dx' + Py'*Dy' - m**2 * Pz'*Dz' - m*r1*Dz')
 *	c = Px'**2 + Py'**2 - m**2 * Pz'**2 - 2*m*r1*Pz' - r1**2
 *
 * Line L' hits the infinitely tall unit cone at point(s) W' which
 * correspond to the roots of the quadratic.  The quadratic formula
 * yields values for "t"
 *
 *	t = [ -b +/- sqrt(b** - 4 * a * c) ] / (2 * a)
 *
 * This parameter "t" can be substituted into the formulas for either
 * L' or L, because affine transformations preserve distances along
 * lines.
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
 * Note that ``t'' is constant, and is the same in the formulations
 * for both W and W'.
 *
 * The hit at ``t'' is a hit on the height=1 unit cylinder IFF
 * 0 <= Wz' <= 1.
 *
 * NORMALS.  Given the point W on the surface of the cylinder, what is
 * the vector normal to the tangent plane at that point?
 *
 * Map W onto the unit cylinder, i.e.:  W' = S(R(W - V)).
 *
 * Plane on unit cylinder at W' has a normal vector N' of the same
 * value as W' in x and y, with z set to zero, i.e., (Wx', Wy', 0)
 *
 * The plane transforms back to the tangent plane at W, and this new
 * plane (on the original cylinder) has a normal vector of N, viz:
 *
 * N = inverse[ transpose(invR o invS) ] (N')
 *   = inverse[ transpose(invS) o transpose(invR) ] (N')
 *   = inverse[ inverse(S) o R ] (N')
 *   = invR o S (N')
 *
 * Note that the normal vector produced above will not have unit length.
 *
 * THE HEMISPHERES.
 *
 * THE "EQUIVALENT CONE":
 *
 * In order to have exact matching of the surface normals at the join
 * between the conical body of the particle and the hemispherical end,
 * it is necessary to alter the cone to form an "equivalent cone",
 * where the end caps of the cone are both shifted away from the large
 * hemisphere and towards the smaller one.  This makes the cone end
 * where it is tangent to the hemisphere.  The calculation for theta
 * come from a diagram drawn by PJT on 18-Nov-99.
 */
/** @} */

#include "common.h"

#include <stddef.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu/cv.h"
#include "vmath.h"
#include "rt/db4.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "nmg.h"
#include "../../librt_private.h"


struct part_specific {
    struct rt_part_internal part_int;
    mat_t part_SoR;	/* Scale(Rot(vect)) */
    mat_t part_invRoS;	/* invRot(Scale(vect)) */
    fastf_t part_vrad_prime;
    fastf_t part_hrad_prime;
    /* For the "equivalent cone" */
    fastf_t part_v_hdist;	/* dist of base plate on unit cone */
    fastf_t part_h_hdist;
    fastf_t part_v_erad;	/* radius of equiv. particle */
    fastf_t part_h_erad;
};


/* hit_surfno flags for which end was hit */
#define RT_PARTICLE_SURF_VSPHERE 1
#define RT_PARTICLE_SURF_BODY 2
#define RT_PARTICLE_SURF_HSPHERE 3

const struct bu_structparse rt_part_parse[] = {
    { "%f", 3, "V", bu_offsetofarray(struct rt_part_internal, part_V, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "H", bu_offsetofarray(struct rt_part_internal, part_H, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "r_v", bu_offsetof(struct rt_part_internal, part_vrad), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "r_h", bu_offsetof(struct rt_part_internal, part_hrad), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

/**
 * Compute the bounding RPP for a particle
 */
int
rt_part_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol)) {
    struct rt_part_internal *pip;
    vect_t tip_pt, tmp_min, tmp_max;
    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(pip);

    (*min)[X] = pip->part_V[X] - pip->part_vrad;
    (*max)[X] = pip->part_V[X] + pip->part_vrad;
    (*min)[Y] = pip->part_V[Y] - pip->part_vrad;
    (*max)[Y] = pip->part_V[Y] + pip->part_vrad;
    (*min)[Z] = pip->part_V[Z] - pip->part_vrad;
    (*max)[Z] = pip->part_V[Z] + pip->part_vrad;

    /* If we've got a sphere, we're done */
    if (pip->part_type == RT_PARTICLE_TYPE_SPHERE) {
	return 0;		/* OK */
    }

    VADD2(tip_pt, pip->part_V, pip->part_H);
    tmp_min[X] = tip_pt[X] - pip->part_hrad;
    tmp_max[X] = tip_pt[X] + pip->part_hrad;
    tmp_min[Y] = tip_pt[Y] - pip->part_hrad;
    tmp_max[Y] = tip_pt[Y] + pip->part_hrad;
    tmp_min[Z] = tip_pt[Z] - pip->part_hrad;
    tmp_max[Z] = tip_pt[Z] + pip->part_hrad;
    VMINMAX((*min), (*max), tmp_min);
    VMINMAX((*min), (*max), tmp_max);

    return 0;
}


/**
 * Given a pointer to a GED database record, and a transformation matrix,
 * determine if this is a valid particle, and if so, precompute various
 * terms of the formula.
 *
 * Returns -
 * 0 particle is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct part_specific is created, and its address is stored in
 * stp->st_specific for use by part_shot().
 */
int
rt_part_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    register struct part_specific *part;
    struct rt_part_internal *pip;
    vect_t Hunit;
    vect_t a, b;
    mat_t R, Rinv;
    mat_t S;
    fastf_t inv_hlen;
    fastf_t hlen;
    fastf_t hlen_sq;
    fastf_t r_diff;
    fastf_t r_diff_sq;
    fastf_t sin_theta;
    fastf_t cos_theta;

    if (rtip) RT_CK_RTI(rtip);
    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(pip);

    BU_GET(part, struct part_specific);
    stp->st_specific = (void *)part;
    part->part_int = *pip;			/* struct copy */
    pip = &part->part_int;

    if (rt_part_bbox(ip, &(stp->st_min), &(stp->st_max), &rtip->rti_tol)) return 1;

    if (pip->part_type == RT_PARTICLE_TYPE_SPHERE) {
	/* Compute bounding sphere*/
	VMOVE(stp->st_center, pip->part_V);
	stp->st_aradius = stp->st_bradius = pip->part_vrad;
	return 0;		/* OK */
    }

    /* Compute some essential terms */
    hlen_sq = MAGSQ(pip->part_H);
    if (hlen_sq <= SMALL) {
	bu_log("part(%s): 0-length H vector\n", stp->st_dp->d_namep);
	return 1;		/* BAD */
    }
    hlen = sqrt(hlen_sq);
    inv_hlen = 1/hlen;
    VSCALE(Hunit, pip->part_H, inv_hlen);
    bn_vec_ortho(a, Hunit);
    VCROSS(b, Hunit, a);

    /*
     * Compute parameters for the "equivalent cone"
     */

    /* Calculate changes in terms of the "unit particle" */
    if (pip->part_vrad >= pip->part_hrad) {
	/* V end is larger, H end is smaller */
	r_diff = (pip->part_vrad - pip->part_hrad) * inv_hlen;
	r_diff_sq = r_diff * r_diff;
	if (r_diff_sq > 1) {
	    /* No "equivalent cone" is possible, theta=90deg */
	    sin_theta = 1;
	    cos_theta = 0;
	} else {
	    sin_theta = sqrt(1 - r_diff_sq);
	    cos_theta = fabs(r_diff);
	}

	part->part_v_erad = pip->part_vrad / sin_theta;
	part->part_h_erad = pip->part_hrad / sin_theta;

	/* Move both plates towards H hemisphere */
	part->part_v_hdist = cos_theta * pip->part_vrad * inv_hlen;
	part->part_h_hdist = 1 + cos_theta * pip->part_hrad * inv_hlen;
    } else {
	/* H end is larger, V end is smaller */
	r_diff = (pip->part_hrad - pip->part_vrad) * inv_hlen;
	r_diff_sq = r_diff * r_diff;
	if (r_diff_sq > 1) {
	    /* No "equivalent cone" is possible, theta=90deg */
	    sin_theta = 1;
	    cos_theta = 0;
	} else {
	    sin_theta = sqrt(1 - r_diff_sq);
	    cos_theta = fabs(r_diff);
	}

	part->part_v_erad = pip->part_vrad / sin_theta;
	part->part_h_erad = pip->part_hrad / sin_theta;

	/* Move both plates towards V hemisphere */
	part->part_v_hdist = -cos_theta * pip->part_vrad * inv_hlen;
	part->part_h_hdist = 1 - cos_theta * pip->part_hrad * inv_hlen;
    }
    /* Thanks to matrix S, vrad_prime is always 1 */
/*#define VRAD_PRIME 1 */
/*#define HRAD_PRIME	(part->part_int.part_hrad / part->part_int.part_vrad) */
    part->part_vrad_prime = 1;
    part->part_hrad_prime = part->part_h_erad / part->part_v_erad;

    /* Compute R and Rinv */
    MAT_IDN(R);
    VMOVE(&R[0], a);		/* has unit length */
    VMOVE(&R[4], b);		/* has unit length */
    VMOVE(&R[8], Hunit);
    bn_mat_trn(Rinv, R);

    /* Compute scale matrix S, where |A| = |B| = equiv_Vradius */
    MAT_IDN(S);
    S[ 0] = 1.0 / part->part_v_erad;
    S[ 5] = S[0];
    S[10] = inv_hlen;

    bn_mat_mul(part->part_SoR, S, R);
    bn_mat_mul(part->part_invRoS, Rinv, S);

    /* RPP and bounding sphere */
    VJOIN1(stp->st_center, pip->part_V, 0.5, pip->part_H);

    stp->st_aradius = stp->st_bradius = pip->part_vrad;

    /* Determine bounding sphere from the RPP */
    {
	register fastf_t f;
	vect_t work;
	VSUB2SCALE(work, stp->st_max, stp->st_min, 0.5);	/* radius */
	f = work[X];
	if (work[Y] > f) f = work[Y];
	if (work[Z] > f) f = work[Z];
	stp->st_aradius = f;
	stp->st_bradius = MAGNITUDE(work);
    }
    return 0;			/* OK */
}


void
rt_part_print(register const struct soltab *stp)
{
    register const struct part_specific *part =
	(struct part_specific *)stp->st_specific;

    VPRINT("part_V", part->part_int.part_V);
    VPRINT("part_H", part->part_int.part_H);
    bu_log("part_vrad=%g\n", part->part_int.part_vrad);
    bu_log("part_hrad=%g\n", part->part_int.part_hrad);

    switch (part->part_int.part_type) {
	case RT_PARTICLE_TYPE_SPHERE:
	    bu_log("part_type = SPHERE\n");
	    break;
	case RT_PARTICLE_TYPE_CYLINDER:
	    bu_log("part_type = CYLINDER\n");
	    bn_mat_print("part_SoR", part->part_SoR);
	    bn_mat_print("part_invRoS", part->part_invRoS);
	    break;
	case RT_PARTICLE_TYPE_CONE:
	    bu_log("part_type = CONE\n");
	    bn_mat_print("part_SoR", part->part_SoR);
	    bn_mat_print("part_invRoS", part->part_invRoS);
	    break;
	default:
	    bu_log("part_type = %d ???\n", part->part_int.part_type);
	    break;
    }
}


/**
 * Intersect a ray with a part.
 * If an intersection occurs, a struct seg will be acquired
 * and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_part_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    register struct part_specific *part =
	(struct part_specific *)stp->st_specific;
    struct seg *segp;
    vect_t dprime;		/* D' */
    point_t pprime;		/* P' */
    point_t xlated;		/* translated ray start point */
    fastf_t t1, t2;		/* distance constants of solution */
    fastf_t f;
    struct hit hits[4];	/* 4 potential hit points */
    register struct hit *hitp = &hits[0];
    int check_v, check_h;

    if (part->part_int.part_type == RT_PARTICLE_TYPE_SPHERE) {
	vect_t ov;		/* ray origin to center (V - P) */
	fastf_t vrad_sq;
	fastf_t magsq_ov;	/* length squared of ov */
	fastf_t b;		/* second term of quadratic eqn */
	fastf_t root;		/* root of radical */

	VSUB2(ov, part->part_int.part_V, rp->r_pt);
	b = VDOT(rp->r_dir, ov);
	magsq_ov = MAGSQ(ov);

	if (magsq_ov >= (vrad_sq = part->part_int.part_vrad *
			 part->part_int.part_vrad)) {
	    /* ray origin is outside of sphere */
	    if (b < 0) {
		/* ray direction is away from sphere */
		return 0;		/* No hit */
	    }
	    root = b*b - magsq_ov + vrad_sq;
	    if (root <= 0) {
		/* no real roots */
		return 0;		/* No hit */
	    }
	} else {
	    root = b*b - magsq_ov + vrad_sq;
	}
	root = sqrt(root);

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;

	/* we know root is positive, so we know the smaller t */
	segp->seg_in.hit_magic = RT_HIT_MAGIC;
	segp->seg_in.hit_dist = b - root;
	segp->seg_in.hit_surfno = RT_PARTICLE_SURF_VSPHERE;
	segp->seg_out.hit_magic = RT_HIT_MAGIC;
	segp->seg_out.hit_dist = b + root;
	segp->seg_out.hit_surfno = RT_PARTICLE_SURF_VSPHERE;
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
	return 2;			/* HIT */
    }

    /* Transform ray to coordinate system of unit cone at origin */
    MAT4X3VEC(dprime, part->part_SoR, rp->r_dir);
    VSUB2(xlated, rp->r_pt, part->part_int.part_V);
    MAT4X3VEC(pprime, part->part_SoR, xlated);

    if (ZERO(dprime[X]) && ZERO(dprime[Y])) {
	check_v = check_h = 1;
	goto check_hemispheres;
    }
    check_v = check_h = 0;

    /* Find roots of the equation, using formula for quadratic */
    /* Note that vrad' = 1 and hrad' = hrad/vrad */
    if (part->part_int.part_type == RT_PARTICLE_TYPE_CYLINDER) {
	/* Cylinder case, hrad == vrad, m = 0 */
	fastf_t a, b, c;
	fastf_t root;		/* root of radical */

	a = dprime[X]*dprime[X] + dprime[Y]*dprime[Y];
	b = dprime[X]*pprime[X] + dprime[Y]*pprime[Y];
	c = pprime[X]*pprime[X] + pprime[Y]*pprime[Y] - 1;
	if ((root = b*b - a * c) <= 0)
	    goto check_hemispheres;
	root = sqrt(root);
	t1 = (root-b) / a;
	t2 = -(root+b) / a;
    } else {
	/* Cone case */
	fastf_t a, b, c;
	fastf_t root;		/* root of radical */
	fastf_t m, msq;

	m = part->part_hrad_prime - part->part_vrad_prime;

	/* This quadratic has had a factor of 2 divided out of "b"
	 * throughout.  More efficient, but the same answers.
	 */
	a = dprime[X]*dprime[X] + dprime[Y]*dprime[Y] -
	    (msq = m*m) * dprime[Z]*dprime[Z];
	b = dprime[X]*pprime[X] + dprime[Y]*pprime[Y] -
	    msq * dprime[Z]*pprime[Z] -
	    m * dprime[Z];		/* * part->part_vrad_prime */
	c = pprime[X]*pprime[X] + pprime[Y]*pprime[Y] -
	    msq * pprime[Z]*pprime[Z] -
	    2 * m * pprime[Z] - 1;
	/* was: ... -2m * vrad' * Pz' - vrad'**2 */

	if ((root = b*b - a * c) <= 0)
	    goto check_hemispheres;
	root = sqrt(root);

	t1 = (root-b) / a;
	t2 = -(root+b) / a;
    }

    /*
     * t1 and t2 are potential solutions to intersection with side.
     * Find hit' point, see if Z values fall in range.
     */
    if ((f = pprime[Z] + t1 * dprime[Z]) >= part->part_v_hdist) {
	check_h = 1;		/* may also hit off end */
	if (f <= part->part_h_hdist) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    /** VJOIN1(hitp->hit_vpriv, pprime, t1, dprime); **/
	    hitp->hit_vpriv[X] = pprime[X] + t1 * dprime[X];
	    hitp->hit_vpriv[Y] = pprime[Y] + t1 * dprime[Y];
	    hitp->hit_vpriv[Z] = f;
	    hitp->hit_dist = t1;
	    hitp->hit_surfno = RT_PARTICLE_SURF_BODY;
	    hitp++;
	}
    } else {
	check_v = 1;
    }

    if ((f = pprime[Z] + t2 * dprime[Z]) >= part->part_v_hdist) {
	check_h = 1;		/* may also hit off end */
	if (f <= part->part_h_hdist) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    /** VJOIN1(hitp->hit_vpriv, pprime, t2, dprime); **/
	    hitp->hit_vpriv[X] = pprime[X] + t2 * dprime[X];
	    hitp->hit_vpriv[Y] = pprime[Y] + t2 * dprime[Y];
	    hitp->hit_vpriv[Z] = f;
	    hitp->hit_dist = t2;
	    hitp->hit_surfno = RT_PARTICLE_SURF_BODY;
	    hitp++;
	}
    } else {
	check_v = 1;
    }

    /*
     * Check for hitting the end hemispheres.
     */
 check_hemispheres:
    if (check_v) {
	vect_t ov;		/* ray origin to center (V - P) */
	fastf_t rad_sq;
	fastf_t magsq_ov;	/* length squared of ov */
	fastf_t b;
	fastf_t root;		/* root of radical */

	/*
	 * First, consider a hit on V hemisphere.
	 */
	VSUB2(ov, part->part_int.part_V, rp->r_pt);
	b = VDOT(rp->r_dir, ov);
	magsq_ov = MAGSQ(ov);
	if (magsq_ov >= (rad_sq = part->part_int.part_vrad *
			 part->part_int.part_vrad)) {
	    /* ray origin is outside of sphere */
	    if (b < 0) {
		/* ray direction is away from sphere */
		goto do_check_h;
	    }
	    root = b*b - magsq_ov + rad_sq;
	    if (root <= 0) {
		/* no real roots */
		goto do_check_h;
	    }
	} else {
	    root = b*b - magsq_ov + rad_sq;
	}
	root = sqrt(root);
	t1 = b - root;
	/* see if hit'[Z] is below V end of cylinder */
	if (pprime[Z] + t1 * dprime[Z] <= part->part_v_hdist) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = t1;
	    hitp->hit_surfno = RT_PARTICLE_SURF_VSPHERE;
	    hitp++;
	}
	t2 = b + root;
	if (pprime[Z] + t2 * dprime[Z] <= part->part_v_hdist) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = t2;
	    hitp->hit_surfno = RT_PARTICLE_SURF_VSPHERE;
	    hitp++;
	}
    }

 do_check_h:
    if (check_h) {
	vect_t ov;		/* ray origin to center (V - P) */
	fastf_t rad_sq;
	fastf_t magsq_ov;	/* length squared of ov */
	fastf_t b;		/* second term of quadratic eqn */
	fastf_t root;		/* root of radical */

	/*
	 * Next, consider a hit on H hemisphere
	 */
	VADD2(ov, part->part_int.part_V, part->part_int.part_H);
	VSUB2(ov, ov, rp->r_pt);
	b = VDOT(rp->r_dir, ov);
	magsq_ov = MAGSQ(ov);
	if (magsq_ov >= (rad_sq = part->part_int.part_hrad *
			 part->part_int.part_hrad)) {
	    /* ray origin is outside of sphere */
	    if (b < 0) {
		/* ray direction is away from sphere */
		goto out;
	    }
	    root = b*b - magsq_ov + rad_sq;
	    if (root <= 0) {
		/* no real roots */
		goto out;
	    }
	} else {
	    root = b*b - magsq_ov + rad_sq;
	}
	root = sqrt(root);
	t1 = b - root;
	/* see if hit'[Z] is above H end of cylinder */
	if (pprime[Z] + t1 * dprime[Z] >= part->part_h_hdist) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = t1;
	    hitp->hit_surfno = RT_PARTICLE_SURF_HSPHERE;
	    hitp++;
	}
	t2 = b + root;
	if (pprime[Z] + t2 * dprime[Z] >= part->part_h_hdist) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = t2;
	    hitp->hit_surfno = RT_PARTICLE_SURF_HSPHERE;
	    hitp++;
	}
    }
 out:
    if (hitp == &hits[0])
	return 0;	/* MISS */
    if (hitp == &hits[1]) {
	/* Only one hit, make it a 0-thickness segment */
	hits[1] = hits[0];		/* struct copy */
	hitp++;
    } else if (hitp > &hits[2]) {
	/*
	 * More than two intersections found.
	 * This can happen when a ray grazes down along a tangent
	 * line; the intersection interval from the hemisphere
	 * may not quite join up with the interval from the cone.
	 * Since particles are convex, all we need to do is to
	 * return the maximum extent of the ray.
	 * Do this by sorting the intersections,
	 * and using the minimum and maximum values.
	 */
	rt_hitsort(hits, hitp - &hits[0]);

	/* [0] is minimum, make [1] be maximum (hitp is +1 off end) */
	hits[1] = hitp[-1];	/* struct copy */
    }

    if (hits[0].hit_dist < hits[1].hit_dist) {
	/* entry is [0], exit is [1] */
	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in = hits[0];		/* struct copy */
	segp->seg_out = hits[1];	/* struct copy */
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    } else {
	/* entry is [1], exit is [0] */
	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in = hits[1];		/* struct copy */
	segp->seg_out = hits[0];	/* struct copy */
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }
    return 2;			/* HIT */
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_part_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
    register struct part_specific *part =
	(struct part_specific *)stp->st_specific;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    switch (hitp->hit_surfno) {
	case RT_PARTICLE_SURF_VSPHERE:
	    VSUB2(hitp->hit_normal, hitp->hit_point, part->part_int.part_V);
	    VUNITIZE(hitp->hit_normal);
	    break;
	case RT_PARTICLE_SURF_HSPHERE:
	    VSUB3(hitp->hit_normal, hitp->hit_point,
		  part->part_int.part_V, part->part_int.part_H);
	    VUNITIZE(hitp->hit_normal);
	    break;
	case RT_PARTICLE_SURF_BODY:
	    /* compute it */
	    if (part->part_int.part_type == RT_PARTICLE_TYPE_CYLINDER) {
		/* The X' and Y' components of hit' are N' */
		hitp->hit_vpriv[Z] = 0;
		MAT4X3VEC(hitp->hit_normal, part->part_invRoS,
			  hitp->hit_vpriv);
		VUNITIZE(hitp->hit_normal);
	    } else {
		/* The cone case */
		fastf_t s, m;
		vect_t unorm;
		/* vpriv[Z] ranges from 0 to 1 (roughly) */
		/* Rescale X' and Y' into unit circle */
		m = part->part_hrad_prime - part->part_vrad_prime;
		s = 1/(part->part_vrad_prime + m * hitp->hit_vpriv[Z]);
		unorm[X] = hitp->hit_vpriv[X] * s;
		unorm[Y] = hitp->hit_vpriv[Y] * s;
		/* Z' is constant, from slope of cylinder wall*/
		unorm[Z] = -m / sqrt(m*m+1);
		MAT4X3VEC(hitp->hit_normal, part->part_invRoS, unorm);
		VUNITIZE(hitp->hit_normal);
	    }
	    break;
    }
}


/**
 * Return the curvature of the particle.
 * There are two cases:  hitting a hemisphere, and hitting the cylinder.
 */
void
rt_part_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
    register struct part_specific *part =
	(struct part_specific *)stp->st_specific;
    point_t hit_local;	/* hit_point, with V as origin */
    point_t hit_unit;	/* hit_point in unit coords, +Z along H */

    switch (hitp->hit_surfno) {
	case RT_PARTICLE_SURF_VSPHERE:
	    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
	    cvp->crv_c1 = cvp->crv_c2 = -part->part_int.part_vrad;
	    break;
	case RT_PARTICLE_SURF_HSPHERE:
	    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
	    cvp->crv_c1 = cvp->crv_c2 = -part->part_int.part_hrad;
	    break;
	case RT_PARTICLE_SURF_BODY:
	    /* Curvature in only one direction, around H */
	    VCROSS(cvp->crv_pdir, hitp->hit_normal, part->part_int.part_H);
	    VUNITIZE(cvp->crv_pdir);
	    /* Interpolate radius between vrad and hrad */
	    VSUB2(hit_local, hitp->hit_point, part->part_int.part_V);
	    MAT4X3VEC(hit_unit, part->part_SoR, hit_local);
	    /* hit_unit[Z] ranges from 0 at V to 1 at H, interpolate */
	    cvp->crv_c1 = -(
		part->part_v_erad * hit_unit[Z] +
		part->part_h_erad * (1 - hit_unit[Z]));
	    cvp->crv_c2 = 0;
	    break;
    }
}


/**
 * For a hit on the surface of a particle, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 * u = azimuth
 * v = elevation along H
 *
 * The 'u' coordinate wraps around the particle, once.
 * The 'v' coordinate covers the 'height' of the particle,
 * from V-r1 to (V+H)+r2.
 *
 * hit_point has already been computed.
 */
void
rt_part_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
    register const struct part_specific *part =
	(struct part_specific *)stp->st_specific;
    point_t hit_local;	/* hit_point, with V as origin */
    point_t hit_unit;	/* hit_poit in unit coords, +Z along H */
    fastf_t hsize;
    fastf_t hmag_inv;
    fastf_t vrad_unit;
    fastf_t r;
    fastf_t minrad;

    RT_PART_CK_MAGIC(&part->part_int.part_magic);

    hmag_inv = 1.0/MAGNITUDE(part->part_int.part_H);
    hsize = 1 + (vrad_unit = part->part_v_erad*hmag_inv) +
	part->part_h_erad*hmag_inv;

    /* Transform hit point into unit particle coords */
    VSUB2(hit_local, hitp->hit_point, part->part_int.part_V);
    MAT4X3VEC(hit_unit, part->part_SoR, hit_local);
    /* normalize 0..1 */
    uvp->uv_v = (hit_unit[Z] + vrad_unit) / hsize;

    /* U is azimuth, atan() range: -pi to +pi */
    uvp->uv_u = bn_atan2(hit_unit[Y], hit_unit[X]) * M_1_2PI;
    if (uvp->uv_u < 0)
	uvp->uv_u += 1.0;

    /* approximation: beam_r / (solid_circumference = 2 * pi * radius) */
    minrad = part->part_v_erad;
    V_MIN(minrad, part->part_h_erad);
    r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
    uvp->uv_du = uvp->uv_dv =
	M_1_2PI * r / minrad;
}


void
rt_part_free(register struct soltab *stp)
{
    register struct part_specific *part =
	(struct part_specific *)stp->st_specific;

    BU_PUT(part, struct part_specific);
    stp->st_specific = ((void *)0);
}


/**
 * Produce a crude approximation to a hemisphere,
 * 8 points around the rim [0]..[7],
 * 4 points around a midway latitude [8]..[11], and
 * 1 point at the pole [12].
 *
 * For the dome, connect up:
 * 0 8 12 10 4
 * 2 9 12 11 6
 */
HIDDEN void
rt_part_hemisphere(register point_t (*ov), register fastf_t *v, fastf_t *a, fastf_t *b, fastf_t *h)
{
    /* M_SQRT1_2 is cos45 */

    /* This is the top of the dome */
    VADD2(ov[12], v, h);

    VADD2(ov[0], v, a);
    VJOIN2(ov[1], v, M_SQRT1_2, a, M_SQRT1_2, b);
    VADD2(ov[2], v, b);
    VJOIN2(ov[3], v, -M_SQRT1_2, a, M_SQRT1_2, b);
    VSUB2(ov[4], v, a);
    VJOIN2(ov[5], v, -M_SQRT1_2, a, -M_SQRT1_2, b);
    VSUB2(ov[6], v, b);
    VJOIN2(ov[7], v, M_SQRT1_2, a, -M_SQRT1_2, b);

    VJOIN2(ov[8], v, M_SQRT1_2, a, M_SQRT1_2, h);
    VJOIN2(ov[10], v, -M_SQRT1_2, a, M_SQRT1_2, h);

    VJOIN2(ov[9], v, M_SQRT1_2, b, M_SQRT1_2, h);
    VJOIN2(ov[11], v, -M_SQRT1_2, b, M_SQRT1_2, h);
    /* Obviously, this could be optimized quite a lot more */
}


int
rt_part_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
{
    struct rt_part_internal *pip;
    point_t tail;
    point_t sphere_rim[16];
    point_t vhemi[13];
    point_t hhemi[13];
    vect_t a, b, c;		/* defines coord sys of part */
    vect_t as, bs, hs;		/* scaled by radius */
    vect_t Hunit;
    register int i;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(pip);

    if (pip->part_type == RT_PARTICLE_TYPE_SPHERE) {
	/* For the sphere, 3 rings of 16 points */
	VSET(a, pip->part_vrad, 0, 0);
	VSET(b, 0, pip->part_vrad, 0);
	VSET(c, 0, 0, pip->part_vrad);

	rt_ell_16pts(&sphere_rim[0][X], pip->part_V, a, b);
	RT_ADD_VLIST(vhead, sphere_rim[15], BN_VLIST_LINE_MOVE);
	for (i=0; i<16; i++) {
	    RT_ADD_VLIST(vhead, sphere_rim[i], BN_VLIST_LINE_DRAW);
	}

	rt_ell_16pts(&sphere_rim[0][X], pip->part_V, b, c);
	RT_ADD_VLIST(vhead, sphere_rim[15], BN_VLIST_LINE_MOVE);
	for (i=0; i<16; i++) {
	    RT_ADD_VLIST(vhead, sphere_rim[i], BN_VLIST_LINE_DRAW);
	}

	rt_ell_16pts(&sphere_rim[0][X], pip->part_V, a, c);
	RT_ADD_VLIST(vhead, sphere_rim[15], BN_VLIST_LINE_MOVE);
	for (i=0; i<16; i++) {
	    RT_ADD_VLIST(vhead, sphere_rim[i], BN_VLIST_LINE_DRAW);
	}
	return 0;		/* OK */
    }

    VMOVE(Hunit, pip->part_H);
    VUNITIZE(Hunit);
    bn_vec_perp(a, Hunit);
    VUNITIZE(a);
    VCROSS(b, Hunit, a);
    VUNITIZE(b);

    VSCALE(as, a, pip->part_vrad);
    VSCALE(bs, b, pip->part_vrad);
    VSCALE(hs, Hunit, -pip->part_vrad);
    rt_part_hemisphere(vhemi, pip->part_V, as, bs, hs);

    VADD2(tail, pip->part_V, pip->part_H);
    VSCALE(as, a, pip->part_hrad);
    VSCALE(bs, b, pip->part_hrad);
    VSCALE(hs, Hunit, pip->part_hrad);
    rt_part_hemisphere(hhemi, tail, as, bs, hs);

    /* Draw V end hemisphere */
    RT_ADD_VLIST(vhead, vhemi[0], BN_VLIST_LINE_MOVE);
    for (i=7; i >= 0; i--) {
	RT_ADD_VLIST(vhead, vhemi[i], BN_VLIST_LINE_DRAW);
    }
    RT_ADD_VLIST(vhead, vhemi[8], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, vhemi[12], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, vhemi[10], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, vhemi[4], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, vhemi[2], BN_VLIST_LINE_MOVE);
    RT_ADD_VLIST(vhead, vhemi[9], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, vhemi[12], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, vhemi[11], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, vhemi[6], BN_VLIST_LINE_DRAW);

    /* Draw H end hemisphere */
    RT_ADD_VLIST(vhead, hhemi[0], BN_VLIST_LINE_MOVE);
    for (i=7; i >= 0; i--) {
	RT_ADD_VLIST(vhead, hhemi[i], BN_VLIST_LINE_DRAW);
    }
    RT_ADD_VLIST(vhead, hhemi[8], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, hhemi[12], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, hhemi[10], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, hhemi[4], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, hhemi[2], BN_VLIST_LINE_MOVE);
    RT_ADD_VLIST(vhead, hhemi[9], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, hhemi[12], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, hhemi[11], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, hhemi[6], BN_VLIST_LINE_DRAW);

    /* Draw 4 connecting lines */
    RT_ADD_VLIST(vhead, vhemi[0], BN_VLIST_LINE_MOVE);
    RT_ADD_VLIST(vhead, hhemi[0], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, vhemi[2], BN_VLIST_LINE_MOVE);
    RT_ADD_VLIST(vhead, hhemi[2], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, vhemi[4], BN_VLIST_LINE_MOVE);
    RT_ADD_VLIST(vhead, hhemi[4], BN_VLIST_LINE_DRAW);
    RT_ADD_VLIST(vhead, vhemi[6], BN_VLIST_LINE_MOVE);
    RT_ADD_VLIST(vhead, hhemi[6], BN_VLIST_LINE_DRAW);

    return 0;
}


struct part_state {
    struct shell *s;
    mat_t upper_invRinvS;
    mat_t upper_invRoS;
    mat_t lower_invRinvS;
    mat_t lower_invRoS;
    fastf_t theta_tol;
};


struct part_vert_strip {
    int nverts_per_strip;
    int nverts;
    struct vertex **vp;
    vect_t *norms;
    int nfaces;
    struct faceuse **fu;
};


/**
 * Based upon the tesselator for the ellipsoid.
 *
 * Break the particle into three parts:
 * Upper hemisphere	0..nsegs		H	North
 * middle cylinder	nsegs..nsegs+1
 * lower hemisphere	nsegs+1..nstrips-1	V	South
 */
int
rt_part_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    struct rt_part_internal *pip;
    mat_t R;
    mat_t S;
    mat_t invR;
    mat_t invS;
    vect_t zz;
    vect_t hcenter;
    struct part_state state;
    register int i;
    fastf_t radius;
    int nsegs;
    int nstrips;
    struct part_vert_strip *strips;
    int j;
    struct vertex **vertp[5];
    int faceno;
    int stripno;
    int boff;		/* base offset */
    int toff;		/* top offset */
    int blim;		/* base subscript limit */
    int tlim;		/* top subscript limit */
    fastf_t dtol;	/* Absolutized relative tolerance */

    RT_CK_DB_INTERNAL(ip);
    pip = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(pip);

    if (pip->part_type == RT_PARTICLE_TYPE_SPHERE)
	return -1;
    /* For now, concentrate on the most important kind. */

    VADD2(hcenter, pip->part_V, pip->part_H);

    /* Compute R and Rinv matrices */
    /* R is the same for both upper and lower hemispheres */
    /* R is rotation from model coords to unit sphere */
    /* For rotation of the particle, +H (model) becomes +Z (unit sph) */
    VSET(zz, 0, 0, 1);
    bn_mat_fromto(R, pip->part_H, zz, tol);
    bn_mat_trn(invR, R);			/* inv of rot mat is trn */

    /*** Upper (H) ***/

    /* Compute S and invS matrices */
    /* invS is just 1/diagonal elements */
    MAT_IDN(S);
    S[ 0] = S[ 5] = S[10] = 1/pip->part_hrad;
    bn_mat_inv(invS, S);

    /* invRinvS, for converting points from unit sphere to model */
    bn_mat_mul(state.upper_invRinvS, invR, invS);

    /* invRoS, for converting normals from unit sphere to model */
    bn_mat_mul(state.upper_invRoS, invR, S);

    /*** Lower (V) ***/

    /* Compute S and invS matrices */
    /* invS is just 1/diagonal elements */
    MAT_IDN(S);
    S[ 0] = S[ 5] = S[10] = 1/pip->part_vrad;
    bn_mat_inv(invS, S);

    /* invRinvS, for converting points from unit sphere to model */
    bn_mat_mul(state.lower_invRinvS, invR, invS);

    /* invRoS, for converting normals from unit sphere to model */
    bn_mat_mul(state.lower_invRoS, invR, S);

    /* Find the larger of two hemispheres */
    radius = pip->part_vrad;
    if (pip->part_hrad > radius)
	radius = pip->part_hrad;

    dtol = primitive_get_absolute_tolerance(ttol, radius);

    if (dtol > radius) {
	dtol = radius;
    }

    /*
     * Convert distance tolerance into a maximum permissible
     * angle tolerance.  'radius' is largest radius.
     */
    state.theta_tol = 2.0 * acos(1.0 - dtol / radius);

    /* To ensure normal tolerance, remain below this angle */
    if (ttol->norm > 0.0 && ttol->norm < state.theta_tol) {
	state.theta_tol = ttol->norm;
    }

    *r = nmg_mrsv(m);	/* Make region, empty shell, vertex */
    state.s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    /* Find the number of segments to divide 90 degrees worth into */
    nsegs = M_PI_2 / state.theta_tol + 0.999;
    if (nsegs < 2) nsegs = 2;

    /* Find total number of strips of vertices that will be needed.
     * nsegs for each hemisphere, plus one equator each.
     * The two equators will be stitched together to make the cylinder.
     * Note that faces are listed in the stripe ABOVE, i.e., toward
     * the poles.  Thus, strips[0] will have 4 faces.
     */
    nstrips = 2 * nsegs + 2;
    strips = (struct part_vert_strip *)bu_calloc(nstrips,
						 sizeof(struct part_vert_strip), "strips[]");

    /* North pole (Upper hemisphere, H end) */
    strips[0].nverts = 1;
    strips[0].nverts_per_strip = 0;
    strips[0].nfaces = 4;
    /* South pole (Lower hemisphere, V end) */
    strips[nstrips-1].nverts = 1;
    strips[nstrips-1].nverts_per_strip = 0;
    strips[nstrips-1].nfaces = 4;
    /* upper equator (has faces) */
    strips[nsegs].nverts = nsegs * 4;
    strips[nsegs].nverts_per_strip = nsegs;
    strips[nsegs].nfaces = nsegs * 4;
    /* lower equator (no faces) */
    strips[nsegs+1].nverts = nsegs * 4;
    strips[nsegs+1].nverts_per_strip = nsegs;
    strips[nsegs+1].nfaces = 0;

    for (i=1; i<nsegs; i++) {
	strips[i].nverts_per_strip =
	    strips[nstrips-1-i].nverts_per_strip = i;
	strips[i].nverts =
	    strips[nstrips-1-i].nverts = i * 4;
	strips[i].nfaces =
	    strips[nstrips-1-i].nfaces = (2 * i + 1)*4;
    }
    /* All strips have vertices and normals */
    for (i=0; i<nstrips; i++) {
	strips[i].vp = (struct vertex **)bu_calloc(strips[i].nverts,
						   sizeof(struct vertex *), "strip vertex[]");
	strips[i].norms = (vect_t *)bu_calloc(strips[i].nverts,
					      sizeof(vect_t), "strip normals[]");
    }
    /* All strips have faces, except for the one (marked) equator */
    for (i=0; i < nstrips; i++) {
	if (strips[i].nfaces <= 0) {
	    strips[i].fu = (struct faceuse **)NULL;
	    continue;
	}
	strips[i].fu = (struct faceuse **)bu_calloc(strips[i].nfaces,
						    sizeof(struct faceuse *), "strip faceuse[]");
    }

    /* First, build the triangular mesh topology */
    /* Do the top. "toff" in i-1 is UP, towards +B */
    for (i = 1; i <= nsegs; i++) {
	faceno = 0;
	tlim = strips[i-1].nverts;
	blim = strips[i].nverts;
	for (stripno=0; stripno<4; stripno++) {
	    toff = stripno * strips[i-1].nverts_per_strip;
	    boff = stripno * strips[i].nverts_per_strip;

	    /* Connect this quarter strip */
	    for (j = 0; j < strips[i].nverts_per_strip; j++) {

		/* "Right-side-up" triangle */
		vertp[0] = &(strips[i].vp[j+boff]);
		vertp[1] = &(strips[i].vp[(j+1+boff)%blim]);
		vertp[2] = &(strips[i-1].vp[(j+toff)%tlim]);
		if ((strips[i-1].fu[faceno++] = nmg_cmface(state.s, vertp, 3)) == 0) {
		    bu_log("rt_part_tess() nmg_cmface failure\n");
		    goto fail;
		}
		if (j+1 >= strips[i].nverts_per_strip) break;

		/* Follow with interior "Up-side-down" triangle */
		vertp[0] = &(strips[i].vp[(j+1+boff)%blim]);
		vertp[1] = &(strips[i-1].vp[(j+1+toff)%tlim]);
		vertp[2] = &(strips[i-1].vp[(j+toff)%tlim]);
		if ((strips[i-1].fu[faceno++] = nmg_cmface(state.s, vertp, 3)) == 0) {
		    bu_log("rt_part_tess() nmg_cmface failure\n");
		    goto fail;
		}
	    }
	}
    }

    /* Connect the two equators with rectangular (4-point) faces */
    i = nsegs+1;
    {
	faceno = 0;
	tlim = strips[i-1].nverts;
	blim = strips[i].nverts;
	{
	    /* Connect this whole strip */
	    for (j = 0; j < strips[i].nverts; j++) {

		/* "Right-side-up" rectangle */
		vertp[3] = &(strips[i].vp[(j)%blim]);
		vertp[2] = &(strips[i-1].vp[(j)%tlim]);
		vertp[1] = &(strips[i-1].vp[(j+1)%tlim]);
		vertp[0] = &(strips[i].vp[(j+1)%blim]);
		if ((strips[i-1].fu[faceno++] = nmg_cmface(state.s, vertp, 4)) == 0) {
		    bu_log("rt_part_tess() nmg_cmface failure\n");
		    goto fail;
		}
	    }
	}
    }

    /* Do the bottom.  Everything is upside down. "toff" in i+1 is DOWN */
    for (i = nsegs+1; i < nstrips; i++) {
	faceno = 0;
	tlim = strips[i+1].nverts;
	blim = strips[i].nverts;
	for (stripno=0; stripno<4; stripno++) {
	    toff = stripno * strips[i+1].nverts_per_strip;
	    boff = stripno * strips[i].nverts_per_strip;

	    /* Connect this quarter strip */
	    for (j = 0; j < strips[i].nverts_per_strip; j++) {

		/* "Right-side-up" triangle */
		vertp[0] = &(strips[i].vp[j+boff]);
		vertp[1] = &(strips[i+1].vp[(j+toff)%tlim]);
		vertp[2] = &(strips[i].vp[(j+1+boff)%blim]);
		if ((strips[i+1].fu[faceno++] = nmg_cmface(state.s, vertp, 3)) == 0) {
		    bu_log("rt_part_tess() nmg_cmface failure\n");
		    goto fail;
		}
		if (j+1 >= strips[i].nverts_per_strip) break;

		/* Follow with interior "Up-side-down" triangle */
		vertp[0] = &(strips[i].vp[(j+1+boff)%blim]);
		vertp[1] = &(strips[i+1].vp[(j+toff)%tlim]);
		vertp[2] = &(strips[i+1].vp[(j+1+toff)%tlim]);
		if ((strips[i+1].fu[faceno++] = nmg_cmface(state.s, vertp, 3)) == 0) {
		    bu_log("rt_part_tess() nmg_cmface failure\n");
		    goto fail;
		}
	    }
	}
    }

    /* Compute the geometry of each vertex.
     * Start with the location in the unit sphere, and project back.
     * i=0 is "straight up" along +H.
     */
    for (i=0; i < nstrips; i++) {
	double alpha;		/* decline down from B to A */
	double beta;		/* angle around equator (azimuth) */
	fastf_t cos_alpha, sin_alpha;
	fastf_t cos_beta, sin_beta;
	point_t sphere_pt;
	point_t model_pt;

	/* Compensate for extra equator */
	if (i <= nsegs)
	    alpha = (((double)i) / (nstrips-1-1));
	else
	    alpha = (((double)i-1) / (nstrips-1-1));
	cos_alpha = cos(alpha*M_PI);
	sin_alpha = sin(alpha*M_PI);
	for (j=0; j < strips[i].nverts; j++) {

	    beta = ((double)j) / strips[i].nverts;
	    cos_beta = cos(beta*M_2PI);
	    sin_beta = sin(beta*M_2PI);
	    VSET(sphere_pt,
		 cos_beta * sin_alpha,
		 sin_beta * sin_alpha,
		 cos_alpha);
	    /* Convert from ideal sphere coordinates */
	    if (i <= nsegs) {
		MAT4X3PNT(model_pt, state.upper_invRinvS, sphere_pt);
		VADD2(model_pt, model_pt, hcenter);
		/* Convert sphere normal to ellipsoid normal */
		MAT4X3VEC(strips[i].norms[j], state.upper_invRoS, sphere_pt);
	    } else {
		MAT4X3PNT(model_pt, state.lower_invRinvS, sphere_pt);
		VADD2(model_pt, model_pt, pip->part_V);
		/* Convert sphere normal to ellipsoid normal */
		MAT4X3VEC(strips[i].norms[j], state.lower_invRoS, sphere_pt);
	    }

	    /* May not be unit length anymore */
	    VUNITIZE(strips[i].norms[j]);
	    /* Associate vertex geometry */
	    nmg_vertex_gv(strips[i].vp[j], model_pt);
	}
    }

    /* Associate face geometry.  lower Equator has no faces */
    for (i=0; i < nstrips; i++) {
	for (j=0; j < strips[i].nfaces; j++) {
	    if (nmg_fu_planeeqn(strips[i].fu[j], tol) < 0)
		goto fail;
	}
    }

    /* Associate normals with vertexuses */
    for (i=0; i < nstrips; i++) {
	for (j=0; j < strips[i].nverts; j++) {
	    struct faceuse *fu;
	    struct vertexuse *vu;
	    vect_t norm_opp;

	    NMG_CK_VERTEX(strips[i].vp[j]);
	    VREVERSE(norm_opp, strips[i].norms[j]);

	    for (BU_LIST_FOR(vu, vertexuse, &strips[i].vp[j]->vu_hd)) {
		fu = nmg_find_fu_of_vu(vu);
		NMG_CK_FACEUSE(fu);
		/* get correct direction of normals depending on
		 * faceuse orientation
		 */
		if (fu->orientation == OT_SAME)
		    nmg_vertexuse_nv(vu, strips[i].norms[j]);
		else if (fu->orientation == OT_OPPOSITE)
		    nmg_vertexuse_nv(vu, norm_opp);
	    }
	}
    }

    /* Compute "geometry" for region and shell */
    nmg_region_a(*r, tol);

    /* Release memory */
    /* All strips have vertices and normals */
    for (i=0; i<nstrips; i++) {
	bu_free((char *)strips[i].vp, "strip vertex[]");
	bu_free((char *)strips[i].norms, "strip norms[]");
    }
    /* All strips have faces, except for equator */
    for (i=0; i < nstrips; i++) {
	if (strips[i].fu == (struct faceuse **)0) continue;
	bu_free((char *)strips[i].fu, "strip faceuse[]");
    }
    bu_free((char *)strips, "strips[]");
    return 0;
 fail:
    /* Release memory */
    /* All strips have vertices and normals */
    for (i=0; i<nstrips; i++) {
	bu_free((char *)strips[i].vp, "strip vertex[]");
	bu_free((char *)strips[i].norms, "strip norms[]");
    }
    /* All strips have faces, except for equator */
    for (i=0; i < nstrips; i++) {
	if (strips[i].fu == (struct faceuse **)0) continue;
	bu_free((char *)strips[i].fu, "strip faceuse[]");
    }
    bu_free((char *)strips, "strips[]");
    return -1;
}


int
rt_part_import4(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
    fastf_t maxrad, minrad;
    union record *rp;
    struct rt_part_internal *part;

    /* must be double for import and export */
    double v[ELEMENTS_PER_POINT];
    double h[ELEMENTS_PER_VECT];
    double vrad;
    double hrad;

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != DBID_PARTICLE) {
	bu_log("rt_part_import4: defective record\n");
	return -1;
    }

    /* Convert from database to internal format */
    bu_cv_ntohd((unsigned char *)v, rp->part.p_v, ELEMENTS_PER_POINT);
    bu_cv_ntohd((unsigned char *)h, rp->part.p_h, ELEMENTS_PER_VECT);
    bu_cv_ntohd((unsigned char *)&vrad, rp->part.p_vrad, 1);
    bu_cv_ntohd((unsigned char *)&hrad, rp->part.p_hrad, 1);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_PARTICLE;
    ip->idb_meth = &OBJ[ID_PARTICLE];
    BU_ALLOC(ip->idb_ptr, struct rt_part_internal);

    part = (struct rt_part_internal *)ip->idb_ptr;
    part->part_magic = RT_PART_INTERNAL_MAGIC;

    /* Apply modeling transformations */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(part->part_V, mat, v);
    MAT4X3VEC(part->part_H, mat, h);
    if ((part->part_vrad = vrad / mat[15]) < 0) {
	bu_log("unable to import particle, negative v radius\n");
	bu_free(ip->idb_ptr, "rt_part_internal");
	ip->idb_ptr=NULL;
	return -2;
    }
    if ((part->part_hrad = hrad / mat[15]) < 0) {
	bu_log("unable to import particle, negative h radius\n");
	bu_free(ip->idb_ptr, "rt_part_internal");
	ip->idb_ptr=NULL;
	return -3;
    }

    if (part->part_vrad > part->part_hrad) {
	maxrad = part->part_vrad;
	minrad = part->part_hrad;
    } else {
	maxrad = part->part_hrad;
	minrad = part->part_vrad;
    }
    if (maxrad <= 0) {
	bu_log("unable to import particle, negative radius\n");
	bu_free(ip->idb_ptr, "rt_part_internal");
	ip->idb_ptr=NULL;
	return -4;
    }

    if (MAGSQ(part->part_H) * 1000000 < maxrad * maxrad) {
	/* Height vector is insignificant, particle is a sphere */
	part->part_vrad = part->part_hrad = maxrad;
	VSETALL(part->part_H, 0);		/* sanity */
	part->part_type = RT_PARTICLE_TYPE_SPHERE;
	return 0;		/* OK */
    }

    if ((maxrad - minrad) / maxrad < 0.001) {
	/* radii are nearly equal, particle is a cylinder (lozenge) */
	part->part_vrad = part->part_hrad = maxrad;
	part->part_type = RT_PARTICLE_TYPE_CYLINDER;
	return 0;		/* OK */
    }

    part->part_type = RT_PARTICLE_TYPE_CONE;
    return 0;		/* OK */
}


int
rt_part_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_part_internal *pip;
    union record *rec;

    /* must be double for import and export */
    double vert[ELEMENTS_PER_POINT];
    double hi[ELEMENTS_PER_VECT];
    double vrad;
    double hrad;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_PARTICLE) return -1;
    pip = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(pip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record);
    ep->ext_buf = (uint8_t*)bu_calloc(1, ep->ext_nbytes, "part external");
    rec = (union record *)ep->ext_buf;

    /* Convert from user units to mm */
    VSCALE(vert, pip->part_V, local2mm);
    VSCALE(hi, pip->part_H, local2mm);
    vrad = pip->part_vrad * local2mm;
    hrad = pip->part_hrad * local2mm;
    /* pip->part_type is not converted -- internal only */

    rec->part.p_id = DBID_PARTICLE;
    bu_cv_htond(rec->part.p_v, (unsigned char *)vert, ELEMENTS_PER_POINT);
    bu_cv_htond(rec->part.p_h, (unsigned char *)hi, ELEMENTS_PER_VECT);
    bu_cv_htond(rec->part.p_vrad, (unsigned char *)&vrad, 1);
    bu_cv_htond(rec->part.p_hrad, (unsigned char *)&hrad, 1);

    return 0;
}


int
rt_part_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
    fastf_t maxrad, minrad;
    struct rt_part_internal *part;

    /* must be double for import and export */
    double vec[8];

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);

    BU_ASSERT_LONG(ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 8);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_PARTICLE;
    ip->idb_meth = &OBJ[ID_PARTICLE];
    BU_ALLOC(ip->idb_ptr, struct rt_part_internal);

    part = (struct rt_part_internal *)ip->idb_ptr;
    part->part_magic = RT_PART_INTERNAL_MAGIC;

    /* Convert from database (network) to internal (host) format */
    bu_cv_ntohd((unsigned char *)vec, ep->ext_buf, 8);

    /* Apply modeling transformations */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(part->part_V, mat, &vec[0*3]);
    MAT4X3VEC(part->part_H, mat, &vec[1*3]);
    if ((part->part_vrad = vec[2*3] / mat[15]) < 0) {
	bu_free(ip->idb_ptr, "rt_part_internal");
	ip->idb_ptr=NULL;
	bu_log("unable to import particle, negative v radius\n");
	return -2;
    }
    if ((part->part_hrad = vec[2*3+1] / mat[15]) < 0) {
	bu_free(ip->idb_ptr, "rt_part_internal");
	ip->idb_ptr=NULL;
	bu_log("unable to import particle, negative h radius\n");
	return -3;
    }

    if (part->part_vrad > part->part_hrad) {
	maxrad = part->part_vrad;
	minrad = part->part_hrad;
    } else {
	maxrad = part->part_hrad;
	minrad = part->part_vrad;
    }
    if (maxrad <= 0) {
	bu_free(ip->idb_ptr, "rt_part_internal");
	ip->idb_ptr=NULL;
	bu_log("unable to import particle, negative radius\n");
	return -4;
    }

    if (MAGSQ(part->part_H) * 1000000 < maxrad * maxrad) {
	/* Height vector is insignificant, particle is a sphere */
	part->part_vrad = part->part_hrad = maxrad;
	VSETALL(part->part_H, 0);		/* sanity */
	part->part_type = RT_PARTICLE_TYPE_SPHERE;
	return 0;		/* OK */
    }

    if ((maxrad - minrad) / maxrad < 0.001) {
	/* radii are nearly equal, particle is a cylinder (lozenge) */
	part->part_vrad = part->part_hrad = maxrad;
	part->part_type = RT_PARTICLE_TYPE_CYLINDER;
	return 0;		/* OK */
    }

    part->part_type = RT_PARTICLE_TYPE_CONE;
    return 0;		/* OK */
}


int
rt_part_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_part_internal *pip;

    /* must be double for import and export */
    double vec[8];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_PARTICLE) return -1;
    pip = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(pip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * 8;
    ep->ext_buf = (uint8_t*)bu_malloc(ep->ext_nbytes, "part external");

    /* scale 'em into local buffer */
    VSCALE(&vec[0*3], pip->part_V, local2mm);
    VSCALE(&vec[1*3], pip->part_H, local2mm);
    vec[2*3] = pip->part_vrad * local2mm;
    vec[2*3+1] = pip->part_hrad * local2mm;

    /* !!! should make sure the values are proper (no negative radius) */

    /* Convert from internal (host) to database (network) format */
    bu_cv_htond(ep->ext_buf, (unsigned char *)vec, 8);

    return 0;
}


/**
 * Make human-readable formatted presentation of this solid.
 * First line describes type of solid.
 * Additional lines are indented one tab, and give parameter values.
 */
int
rt_part_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    register struct rt_part_internal *pip =
	(struct rt_part_internal *)ip->idb_ptr;
    char buf[256];

    RT_PART_CK_MAGIC(pip);
    switch (pip->part_type) {
	case RT_PARTICLE_TYPE_SPHERE:
	    bu_vls_strcat(str, "spherical particle\n");
	    if (!verbose)
		return 0;
	    sprintf(buf, "\tV (%g, %g, %g)\n",
		    INTCLAMP(pip->part_V[X] * mm2local),
		    INTCLAMP(pip->part_V[Y] * mm2local),
		    INTCLAMP(pip->part_V[Z] * mm2local));
	    bu_vls_strcat(str, buf);
	    sprintf(buf, "\tradius = %g\n",
		    INTCLAMP(pip->part_vrad * mm2local));
	    bu_vls_strcat(str, buf);

	    break;
	case RT_PARTICLE_TYPE_CYLINDER:
	    bu_vls_strcat(str, "cylindrical particle (lozenge)\n");
	    if (!verbose)
		return 0;
	    sprintf(buf, "\tV (%g, %g, %g)\n",
		    INTCLAMP(pip->part_V[X] * mm2local),
		    INTCLAMP(pip->part_V[Y] * mm2local),
		    INTCLAMP(pip->part_V[Z] * mm2local));
	    bu_vls_strcat(str, buf);
	    sprintf(buf, "\tH (%g, %g, %g)\n",
		    INTCLAMP(pip->part_H[X] * mm2local),
		    INTCLAMP(pip->part_H[Y] * mm2local),
		    INTCLAMP(pip->part_H[Z] * mm2local));
	    bu_vls_strcat(str, buf);
	    sprintf(buf, "\tradius = %g\n",
		    INTCLAMP(pip->part_vrad * mm2local));
	    bu_vls_strcat(str, buf);

	    break;
	case RT_PARTICLE_TYPE_CONE:
	    bu_vls_strcat(str, "conical particle\n");
	    if (!verbose)
		return 0;
	    sprintf(buf, "\tV (%g, %g, %g)\n",
		    INTCLAMP(pip->part_V[X] * mm2local),
		    INTCLAMP(pip->part_V[Y] * mm2local),
		    INTCLAMP(pip->part_V[Z] * mm2local));
	    bu_vls_strcat(str, buf);
	    sprintf(buf, "\tH (%g, %g, %g)\n",
		    INTCLAMP(pip->part_H[X] * mm2local),
		    INTCLAMP(pip->part_H[Y] * mm2local),
		    INTCLAMP(pip->part_H[Z] * mm2local));
	    bu_vls_strcat(str, buf);
	    sprintf(buf, "\tv end radius = %g\n",
		    INTCLAMP(pip->part_vrad * mm2local));
	    bu_vls_strcat(str, buf);
	    sprintf(buf, "\th end radius = %g\n",
		    INTCLAMP(pip->part_hrad * mm2local));
	    bu_vls_strcat(str, buf);
	    break;
	default:
	    bu_vls_strcat(str, "Unknown particle type\n");
	    return -1;
    }
    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_part_ifree(struct rt_db_internal *ip)
{
    RT_CK_DB_INTERNAL(ip);

    bu_free(ip->idb_ptr, "particle ifree");
    ip->idb_ptr = ((void *)0);
}


int
rt_part_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


void
rt_part_volume(fastf_t *vol, const struct rt_db_internal *ip)
{
    fastf_t vrad, hrad, mag_h;
    struct rt_part_internal *pip = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(pip);

    vrad = pip->part_vrad;
    hrad = pip->part_hrad;
    mag_h = MAGNITUDE(pip->part_H);

    if (EQUAL(vrad, hrad)) {
	*vol = M_PI * vrad * vrad * (4.0/3.0 * vrad + mag_h);
    } else {
	fastf_t vrad3, hrad3, mid_section;
	vrad3 = vrad * vrad * vrad;
	hrad3 = hrad * hrad * hrad;
	mid_section = M_PI * mag_h * (hrad * hrad + vrad * vrad + hrad * vrad) / 3.0;
	*vol = 2.0/3.0 * M_PI * (vrad3 + hrad3) + mid_section;
    }
}


void
rt_part_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    fastf_t vrad, hrad, mag_h;
    struct rt_part_internal *pip = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(pip);

    vrad = pip->part_vrad;
    hrad = pip->part_hrad;
    mag_h = MAGNITUDE(pip->part_H);

    if (EQUAL(vrad, hrad)) {
	*area = M_2PI * vrad * (2.0 * vrad + mag_h);
    } else {
	fastf_t mid_section;
	mid_section = M_PI * ((vrad + hrad) * sqrt((vrad - hrad) * (vrad - hrad) + mag_h * mag_h));
	*area = M_2PI * (vrad * vrad + hrad * hrad) + mid_section;
    }
}


void
rt_part_centroid(point_t *cent, const struct rt_db_internal *ip)
{
    fastf_t vrad, hrad, mag_h, nm, dm, c_frst, cv_hem, ch_hem;
    vect_t hvec, hvec_n;
    point_t vpt, fcent, hhcent, cvcent;
    struct rt_part_internal *pip = (struct rt_part_internal *)ip->idb_ptr;
    int idx;
    RT_PART_CK_MAGIC(pip);

    vrad = pip->part_vrad;
    hrad = pip->part_hrad;
    VSET(hvec,pip->part_H[0], pip->part_H[1], pip->part_H[2]);
    VSET(hvec_n,pip->part_H[0], pip->part_H[1], pip->part_H[2]);
    VUNITIZE(hvec_n);
    VSET(vpt,pip->part_V[0], pip->part_V[1], pip->part_V[2]);
    mag_h = MAGNITUDE(hvec);

    /* conical frustum centroid, see http://mathworld.wolfram.com/ConicalFrustum.html */
    nm = mag_h * (hrad * hrad + 2.0 * hrad * vrad + 3.0 * vrad * vrad);
    dm = 4.0 * (hrad * hrad + hrad * vrad + vrad * vrad);
    c_frst = nm / dm;

    /* hemisphere centroids, see http://mathworld.wolfram.com/Hemisphere.html */
    cv_hem = -3.0 * vrad / 8.0;
    ch_hem =  3.0 * hrad / 8.0;

    /* find frustum and hemisphere centroids separately, weight points */
    for (idx=0; idx < 3; idx++) {
        fcent[idx]  = (hvec_n[idx] * c_frst + vpt[idx]) / 3.0;
        hhcent[idx] = (hvec_n[idx] * ch_hem + vpt[idx]) / 3.0;
        cvcent[idx] = (hvec_n[idx] * cv_hem + vpt[idx] + hvec[idx]) / 3.0;
    }

    VADD3(*cent, fcent, hhcent, cvcent);
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
