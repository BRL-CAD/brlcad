/*                           R P C . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2025 United States Government as represented by
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
/** @file primitives/rpc/rpc.c
 *
 * Intersect a ray with a Right Parabolic Cylinder.
 *
 * Algorithm -
 *
 * Given V, H, R, and B, there is a set of points on this rpc
 *
 * { (x, y, z) | (x, y, z) is on rpc }
 *
 * Through a series of Affine Transformations, this set of points will
 * be transformed into a set of points on an rpc located at the origin
 * with a rectangular halfwidth R of 1 along the Y axis, a height H of
 * +1 along the -X axis, a distance B of 1 along the -Z axis between
 * the vertex V and the tip of the parabola.
 *
 *
 * { (x', y', z') | (x', y', z') is on rpc at origin }
 *
 * The transformation from X to X' is accomplished by:
 *
 * X' = S(R(X - V))
 *
 * where R(X) = (H/(-|H|))
 *  		(R/(|R|)) . X
 *  		(B/(-|B|))
 *
 * and S(X) =	(1/|H|   0     0)
 * 		(0    1/|R|   0) . X
 * 		(0      0   1/|B|)
 *
 * To find the intersection of a line with the surface of the rpc,
 * consider the parametric line L:
 *
 *  	L : { P(n) | P + t(n) . D }
 *
 * Call W the actual point of intersection between L and the rpc.
 * Let W' be the point of intersection between L' and the unit rpc.
 *
 *  	L' : { P'(n) | P' + t(n) . D' }
 *
 * W = invR(invS(W')) + V
 *
 * Where W' = k D' + P'.
 *
 * If Dy' and Dz' are both 0, then there is no hit on the rpc;
 * but the end plates need checking.  If there is now only 1 hit
 * point, the top plate needs to be checked as well.
 *
 * Line L' hits the infinitely long canonical rpc at W' when
 *
 *	A * k**2 + B * k + C = 0
 *
 * where
 *
 * A = Dy'**2
 * B = (2 * Dy' * Py') - Dz'
 * C = Py'**2 - Pz' - 1
 * b = |Breadth| = 1.0
 * h = |Height| = 1.0
 * r = 1.0
 *
 * The quadratic formula yields k (which is constant):
 *
 * k = [ -B +/- sqrt(B**2 - 4*A*C)] / (2*A)
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
 * The hit at ``k'' is a hit on the canonical rpc IFF
 * -1 <= Wx' <= 0 and -1 <= Wz' <= 0.
 *
 * NORMALS.  Given the point W on the surface of the rpc, what is the
 * vector normal to the tangent plane at that point?
 *
 * Map W onto the unit rpc, i.e.:  W' = S(R(W - V)).
 *
 * Plane on unit rpc at W' has a normal vector N' where
 *
 * N' = <0, Wy', -.5>.
 *
 * The plane transforms back to the tangent plane at W, and this new
 * plane (on the original rpc) has a normal vector of N, viz:
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
 * IFF -1 <= Wx' <= 0 and -1 <= Wy' <= 1
 *
 * If Dx' == 0, line L' is parallel to the end plates, so there is no
 * hit on the end plates.  Otherwise, rays intersect the front plate
 * with k = (0 - Px') / Dx' and the back plate with k = (-1 - Px') / Dx'.
 *
 * The solution W' is within an end plate IFF
 *
 *	Wy'**2 + Wz' <= 1.0 and Wz' <= 1.0
 *
 * The normal for a hit on the top plate is -Bunit.
 * The normal for a hit on the front plate is -Hunit, and
 * the normal for a hit on the back plate is +Hunit.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu/cv.h"
#include "vmath.h"
#include "rt/db4.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../../librt_private.h"

#if defined(HAVE_ASINH) && !defined(HAVE_DECL_ASINH)
extern double asinh(double x);
#endif

static int rpc_is_valid(struct rt_rpc_internal *rpc);

struct rpc_specific {
    point_t rpc_V;		/* vector to rpc origin */
    vect_t rpc_Bunit;	/* unit B vector */
    vect_t rpc_Hunit;	/* unit H vector */
    vect_t rpc_Runit;	/* unit vector, B x H */
    fastf_t rpc_b;		/* |B| */
    fastf_t rpc_inv_rsq;	/* 1/(r * r) */
    mat_t rpc_SoR;	/* Scale(Rot(vect)) */
    mat_t rpc_invRoS;	/* invRot(Scale(vect)) */
};

#ifdef USE_OPENCL
/* largest data members first */
struct clt_rpc_specific {
    cl_double rpc_V[3];		/* vector to rpc origin */
    cl_double rpc_Bunit[3];	/* unit B vector */
    cl_double rpc_Hunit[3];	/* unit H vector */
    cl_double rpc_Runit[3];	/* unit vector, B x H */
    cl_double rpc_SoR[16];	/* Scale(Rot(vect)) */
    cl_double rpc_invRoS[16];	/* invRot(Scale(vect)) */
};

size_t
clt_rpc_pack(struct bu_pool *pool, struct soltab *stp)
{
    struct rpc_specific *rpc =
    (struct rpc_specific *)stp->st_specific;
    struct clt_rpc_specific *args;

    const size_t size = sizeof(*args);
    args = (struct clt_rpc_specific*)bu_pool_alloc(pool, 1, size);

    VMOVE(args->rpc_V, rpc->rpc_V);
    VMOVE(args->rpc_Bunit, rpc->rpc_Bunit);
    VMOVE(args->rpc_Hunit, rpc->rpc_Hunit);
    VMOVE(args->rpc_Runit, rpc->rpc_Runit);
    MAT_COPY(args->rpc_SoR, rpc->rpc_SoR);
    MAT_COPY(args->rpc_invRoS, rpc->rpc_invRoS);

    return size;
}

#endif /* USE_OPENCL */

const struct bu_structparse rt_rpc_parse[] = {
    { "%f", 3, "V", bu_offsetofarray(struct rt_rpc_internal, rpc_V, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "H", bu_offsetofarray(struct rt_rpc_internal, rpc_H, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "B", bu_offsetofarray(struct rt_rpc_internal, rpc_B, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "r", bu_offsetof(struct rt_rpc_internal, rpc_r),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

/**
 * Calculate the RPP for an RPC
 */
int
rt_rpc_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol)) {
    struct rt_rpc_internal *xip;
    vect_t rinv, rvect, rv2, working;
    RT_CK_DB_INTERNAL(ip);
    xip = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(xip);

    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    VCROSS(rvect, xip->rpc_H, xip->rpc_B);
    VREVERSE(rinv, rvect);
    VUNITIZE(rvect);
    VUNITIZE(rinv);
    VSCALE(rvect, rvect, xip->rpc_r);
    VSCALE(rinv, rinv, xip->rpc_r);

    VADD2(working, xip->rpc_V, rvect);
    VMINMAX((*min), (*max), working);

    VADD2(working, xip->rpc_V, rinv);
    VMINMAX((*min), (*max), working);

    VADD3(working, xip->rpc_V, rvect, xip->rpc_H);
    VMINMAX((*min), (*max), working);

    VADD3(working, xip->rpc_V, rinv, xip->rpc_H);
    VMINMAX((*min), (*max), working);

    VADD2(rv2, xip->rpc_V, xip->rpc_B);

    VADD2(working, rv2, rvect);
    VMINMAX((*min), (*max), working);

    VADD2(working, rv2, rinv);
    VMINMAX((*min), (*max), working);

    VADD3(working, rv2, rvect, xip->rpc_H);
    VMINMAX((*min), (*max), working);

    VADD3(working, rv2, rinv, xip->rpc_H);
    VMINMAX((*min), (*max), working);

    return 0;
}


/**
 * Given a pointer to a GED database record, and a transformation matrix,
 * determine if this is a valid RPC, and if so, precompute various
 * terms of the formula.
 *
 * Returns -
 * 0 RPC is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct rpc_specific is created, and its address is stored in
 * stp->st_specific for use by rpc_shot().
 */
int
rt_rpc_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_rpc_internal *xip;
    struct rpc_specific *rpc;

    fastf_t magsq_b, magsq_h, magsq_r;
    fastf_t mag_b, mag_h, mag_r;
    mat_t R;
    mat_t Rinv;
    mat_t S;
    vect_t invsq;	/* [ 1/(|H|**2), 1/(|R|**2), 1/(|B|**2) ] */

    RT_CK_DB_INTERNAL(ip);

    xip = (struct rt_rpc_internal *)ip->idb_ptr;
    if (!rpc_is_valid(xip)) {
	return 1;
    }

    /* compute |B| |H| */
    mag_b = sqrt(magsq_b = MAGSQ(xip->rpc_B));
    mag_h = sqrt(magsq_h = MAGSQ(xip->rpc_H));
    mag_r = xip->rpc_r;
    magsq_r = mag_r * mag_r;

    stp->st_id = ID_RPC;		/* set soltab ID */
    stp->st_meth = &OBJ[ID_RPC];

    BU_GET(rpc, struct rpc_specific);
    stp->st_specific = (void *)rpc;
    rpc->rpc_b = mag_b;
    rpc->rpc_inv_rsq = 1 / magsq_r;

    /* make unit vectors in B, H, and BxH directions */
    VMOVE(rpc->rpc_Hunit, xip->rpc_H);
    VUNITIZE(rpc->rpc_Hunit);
    VMOVE(rpc->rpc_Bunit, xip->rpc_B);
    VUNITIZE(rpc->rpc_Bunit);
    VCROSS(rpc->rpc_Runit, rpc->rpc_Bunit, rpc->rpc_Hunit);

    VMOVE(rpc->rpc_V, xip->rpc_V);

    /* Compute R and Rinv matrices */
    MAT_IDN(R);
    VREVERSE(&R[0], rpc->rpc_Hunit);
    VMOVE(&R[4], rpc->rpc_Runit);
    VREVERSE(&R[8], rpc->rpc_Bunit);
    bn_mat_trn(Rinv, R);			/* inv of rot mat is trn */

    /* Compute S */
    VSET(invsq, 1.0/magsq_h, 1.0/magsq_r, 1.0/magsq_b);
    MAT_IDN(S);
    S[ 0] = sqrt(invsq[0]);
    S[ 5] = sqrt(invsq[1]);
    S[10] = sqrt(invsq[2]);

    /* Compute SoR and invRoS */
    bn_mat_mul(rpc->rpc_SoR, S, R);
    bn_mat_mul(rpc->rpc_invRoS, Rinv, S);

    /* Compute bounding sphere and RPP */
    /* bounding sphere center */
    VJOIN2(stp->st_center,	rpc->rpc_V,
	   mag_h / 2.0,	rpc->rpc_Hunit,
	   mag_b / 2.0,	rpc->rpc_Bunit);
    /* bounding radius */
    stp->st_bradius = 0.5 * sqrt(magsq_h + 4.0*magsq_r + magsq_b);
    /* approximate bounding radius */
    stp->st_aradius = stp->st_bradius;
    /* bounding RPP */
    if (rt_rpc_bbox(ip, &(stp->st_min), &(stp->st_max), &rtip->rti_tol)) return 1;
    return 0;			/* OK */
}


void
rt_rpc_print(const struct soltab *stp)
{
    const struct rpc_specific *rpc =
	(struct rpc_specific *)stp->st_specific;

    VPRINT("V", rpc->rpc_V);
    VPRINT("Bunit", rpc->rpc_Bunit);
    VPRINT("Hunit", rpc->rpc_Hunit);
    VPRINT("Runit", rpc->rpc_Runit);
    bn_mat_print("S o R", rpc->rpc_SoR);
    bn_mat_print("invR o S", rpc->rpc_invRoS);
}


/* hit_surfno is set to one of these */
#define RPC_NORM_BODY (1)		/* compute normal */
#define RPC_NORM_TOP (2)		/* copy tgc_N */
#define RPC_NORM_FRT (3)		/* copy reverse tgc_N */
#define RPC_NORM_BACK (4)

/**
 * Intersect a ray with a rpc.
 * If an intersection occurs, a struct seg will be acquired
 * and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_rpc_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct rpc_specific *rpc =
	(struct rpc_specific *)stp->st_specific;
    vect_t dprime;		/* D' */
    vect_t pprime;		/* P' */
    fastf_t k1, k2;		/* distance constants of solution */
    vect_t xlated;		/* translated vector */
    struct hit hits[3];	/* 2 potential hit points */
    struct hit *hitp;	/* pointer to hit point */

    hitp = &hits[0];

    /* out, Mat, vect */
    MAT4X3VEC(dprime, rpc->rpc_SoR, rp->r_dir);
    VSUB2(xlated, rp->r_pt, rpc->rpc_V);
    MAT4X3VEC(pprime, rpc->rpc_SoR, xlated);

    /* Find roots of the equation, using formula for quadratic */
    if (!NEAR_ZERO(dprime[Y], RT_PCOEF_TOL)) {
	fastf_t a, b, c;	/* coeffs of polynomial */
	fastf_t disc;		/* disc of radical */

	a = dprime[Y] * dprime[Y];
	b = 2.0 * dprime[Y] * pprime[Y] - dprime[Z];
	c = pprime[Y] * pprime[Y] - pprime[Z] - 1.0;
	disc = b*b - 4.0 * a * c;
	if (disc <= 0)
	    goto check_plates;
	disc = sqrt(disc);

	k1 = (-b + disc) / (2.0 * a);
	k2 = (-b - disc) / (2.0 * a);

	/*
	 * k1 and k2 are potential solutions to intersection with
	 * side.  See if they fall in range.
	 */
	VJOIN1(hitp->hit_vpriv, pprime, k1, dprime);	/* hit' */
	if (hitp->hit_vpriv[X] >= -1.0 && hitp->hit_vpriv[X] <= 0.0
	    && hitp->hit_vpriv[Z] <= 0.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = RPC_NORM_BODY;	/* compute N */
	    hitp++;
	}

	VJOIN1(hitp->hit_vpriv, pprime, k2, dprime);	/* hit' */
	if (hitp->hit_vpriv[X] >= -1.0 && hitp->hit_vpriv[X] <= 0.0
	    && hitp->hit_vpriv[Z] <= 0.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k2;
	    hitp->hit_surfno = RPC_NORM_BODY;	/* compute N */
	    hitp++;
	}
    } else if (!NEAR_ZERO(dprime[Z], RT_PCOEF_TOL)) {
	k1 = (pprime[Y] * pprime[Y] - pprime[Z] - 1.0) / dprime[Z];
	VJOIN1(hitp->hit_vpriv, pprime, k1, dprime);	/* hit' */
	if (hitp->hit_vpriv[X] >= -1.0 && hitp->hit_vpriv[X] <= 0.0
	    && hitp->hit_vpriv[Z] <= 0.0) {
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
    if (hitp < &hits[2]  &&  !NEAR_ZERO(dprime[X], RT_PCOEF_TOL)) {
	/* 0 or 1 hits so far, this is worthwhile */
	k1 = -pprime[X] / dprime[X];		/* front plate */
	k2 = (-1.0 - pprime[X]) / dprime[X];	/* back plate */

	VJOIN1(hitp->hit_vpriv, pprime, k1, dprime);	/* hit' */
	if (hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y]
	    - hitp->hit_vpriv[Z] <= 1.0
	    && hitp->hit_vpriv[Z] <= 0.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = RPC_NORM_FRT;	/* -H */
	    hitp++;
	}

	VJOIN1(hitp->hit_vpriv, pprime, k2, dprime);	/* hit' */
	if (hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y]
	    - hitp->hit_vpriv[Z] <= 1.0
	    && hitp->hit_vpriv[Z] <= 0.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k2;
	    hitp->hit_surfno = RPC_NORM_BACK;	/* +H */
	    hitp++;
	}
    }

    /* check top plate */
    if (hitp == &hits[1]  &&  !NEAR_ZERO(dprime[Z], RT_PCOEF_TOL)) {
	/* 1 hit so far, this is worthwhile */
	k1 = -pprime[Z] / dprime[Z];		/* top plate */

	VJOIN1(hitp->hit_vpriv, pprime, k1, dprime);	/* hit' */
	if (hitp->hit_vpriv[X] >= -1.0 &&  hitp->hit_vpriv[X] <= 0.0
	    && hitp->hit_vpriv[Y] >= -1.0
	    && hitp->hit_vpriv[Y] <= 1.0) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = RPC_NORM_TOP;	/* -B */
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
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_rpc_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    vect_t can_normal;	/* normal to canonical rpc */
    struct rpc_specific *rpc =
	(struct rpc_specific *)stp->st_specific;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    switch (hitp->hit_surfno) {
	case RPC_NORM_BODY:
	    VSET(can_normal, 0.0, hitp->hit_vpriv[Y], -0.5);
	    MAT4X3VEC(hitp->hit_normal, rpc->rpc_invRoS, can_normal);
	    VUNITIZE(hitp->hit_normal);
	    break;
	case RPC_NORM_TOP:
	    VREVERSE(hitp->hit_normal, rpc->rpc_Bunit);
	    break;
	case RPC_NORM_FRT:
	    VREVERSE(hitp->hit_normal, rpc->rpc_Hunit);
	    break;
	case RPC_NORM_BACK:
	    VMOVE(hitp->hit_normal, rpc->rpc_Hunit);
	    break;
	default:
	    bu_log("rt_rpc_norm: surfno=%d bad\n", hitp->hit_surfno);
	    break;
    }
}


/**
 * Return the curvature of the rpc.
 */
void
rt_rpc_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    fastf_t zp1, zp2;	/* 1st & 2nd derivatives */
    struct rpc_specific *rpc =
	(struct rpc_specific *)stp->st_specific;

    switch (hitp->hit_surfno) {
	case RPC_NORM_BODY:
	    /* most nearly flat direction */
	    VMOVE(cvp->crv_pdir, rpc->rpc_Hunit);
	    cvp->crv_c1 = 0;
	    /* k = z'' / (1 + z'^2) ^ 3/2 */
	    zp2 = 2.0 * rpc->rpc_b * rpc->rpc_inv_rsq;
	    zp1 = zp2 * hitp->hit_point[Y];
	    cvp->crv_c2 = zp2 / pow((1 + zp1*zp1), 1.5);
	    break;
	case RPC_NORM_BACK:
	case RPC_NORM_FRT:
	case RPC_NORM_TOP:
	    /* any tangent direction */
	    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
	    cvp->crv_c1 = cvp->crv_c2 = 0;
	    break;
    }
}


/**
 * For a hit on the surface of an rpc, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1
 * u = azimuth
 * v = elevation
 */
void
rt_rpc_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    struct rpc_specific *rpc = (struct rpc_specific *)stp->st_specific;

    vect_t work;
    vect_t pprime;
    fastf_t len;

    if (ap) RT_CK_APPLICATION(ap);

    /*
     * hit_point is on surface;  project back to unit rpc,
     * creating a vector from vertex to hit point.
     */
    VSUB2(work, hitp->hit_point, rpc->rpc_V);
    MAT4X3VEC(pprime, rpc->rpc_SoR, work);

    switch (hitp->hit_surfno) {
	case RPC_NORM_BODY:
	    /* Skin.  x, y coordinates define rotation.  radius = 1 */
	    len = sqrt(pprime[Y]*pprime[Y] + pprime[Z]*pprime[Z]);
	    uvp->uv_u = acos(pprime[Y]/len) * M_1_PI;
	    uvp->uv_v = -pprime[X];		/* height */
	    break;
	case RPC_NORM_FRT:
	case RPC_NORM_BACK:
	    /* end plates - circular mapping, not seamless w/body, top */
	    len = sqrt(pprime[Y]*pprime[Y] + pprime[Z]*pprime[Z]);
	    uvp->uv_u = acos(pprime[Y]/len) * M_1_PI;
	    uvp->uv_v = len;	/* rim v = 1 for both plates */
	    break;
	case RPC_NORM_TOP:
	/* Simplified next line:
	    uvp->uv_u = 1.0 - (pprime[Y] + 1.0)/2.0; */
	    uvp->uv_u = 0.5 - pprime[Y]/2.0;
	    uvp->uv_v = -pprime[X];		/* height */
	    break;
    }

    /* uv_du should be relative to rotation, uv_dv relative to height */
    uvp->uv_du = uvp->uv_dv = 0;
}


void
rt_rpc_free(struct soltab *stp)
{
    struct rpc_specific *rpc =
	(struct rpc_specific *)stp->st_specific;

    BU_PUT(rpc, struct rpc_specific);
}

/* A canonical parabola in the Y-Z plane has equation z = y^2 / 4p, and opens
 * toward positive z with vertex at the origin.
 *
 * The contour of an rpc in the plane B-R is a parabola with vertex at B,
 * opening toward -B. We can transform this parabola to get an equivalent
 * canonical parabola in the Y-Z plane, opening toward positive Z (-B) with
 * vertex at the origin (B).
 *
 * This parabola passes through the point (r, |B|). If we plug the point (r, |B|)
 * into our canonical equation, we see how p relates to r and |B|:
 *
 *   |B| = r^2 / 4p
 *     p = (r^2) / (4|B|)
 */
static fastf_t
rpc_parabola_p(fastf_t r, fastf_t mag_b)
{
    return (r * r) / (4.0 * mag_b);
}

static fastf_t
rpc_parabola_y(fastf_t p, fastf_t z)
{
    return sqrt(4.0 * p * z);
}

/* The contour of an rpc in the plane B-R is a parabola with vertex at B,
 * opening toward -B. We can transform this parabola to get an equivalent
 * parabola in the Y-Z plane, opening toward positive Z (-B) with vertex at
 * (0, -|B|).
 *
 * The part of this parabola that passes between (0, -|B|) and (r, 0) is
 * approximated by num_points points (including (0, -|B|) and (r, 0)).
 *
 * The constructed point list is returned (NULL returned on error). Because the
 * above transformation puts the rpc vertex at the origin and the parabola
 * vertex at (0, -|B|), multiplying the z values by -1 gives corresponding
 * distances along the rpc breadth vector B.
 */
static struct rt_pnt_node *
rpc_parabolic_curve(fastf_t mag_b, fastf_t r, int num_points)
{
    int count;
    struct rt_pnt_node *curve;

    if (num_points < 2) {
	return NULL;
    }

    BU_ALLOC(curve, struct rt_pnt_node);
    BU_ALLOC(curve->next, struct rt_pnt_node);

    curve->next->next = NULL;
    VSET(curve->p,       0.0, 0.0, -mag_b);
    VSET(curve->next->p, 0.0, r, 0.0);

    if (num_points < 3) {
	return curve;
    }

    count = approximate_parabolic_curve(curve, rpc_parabola_p(r, mag_b), num_points - 2);

    if (count != (num_points - 2)) {
	return NULL;
    }

    return curve;
}

/* plot half of a parabolic contour curve using the given (r, b) points (pts),
 * translation along H (rpc_H), and multiplier for r (rscale)
 */
static void
rpc_plot_parabolic_curve(
	struct bu_list *vlfree,
	struct bu_list *vhead,
	struct rpc_specific *rpc,
	struct rt_pnt_node *pts,
	vect_t rpc_H,
	fastf_t rscale)
{
    vect_t t, Ru, Bu;
    point_t p;
    struct rt_pnt_node *node;

    VADD2(t, rpc->rpc_V, rpc_H);
    VMOVE(Ru, rpc->rpc_Runit);
    VMOVE(Bu, rpc->rpc_Bunit);

    VJOIN2(p, t, rscale * pts->p[Y], Ru, -pts->p[Z], Bu);
    BV_ADD_VLIST(vlfree, vhead, p, BV_VLIST_LINE_MOVE);

    node = pts->next;
    while (node != NULL) {
	VJOIN2(p, t, rscale * node->p[Y], Ru, -node->p[Z], Bu);
	BV_ADD_VLIST(vlfree, vhead, p, BV_VLIST_LINE_DRAW);

	node = node->next;
    }
}

static void
rpc_plot_parabolas(
	struct bu_list *vlfree,
	struct bu_list *vhead,
	struct rt_rpc_internal *rpc,
	struct rt_pnt_node *pts)
{
    vect_t rpc_H;
    struct rpc_specific rpc_s;

    VMOVE(rpc_s.rpc_V, rpc->rpc_V);

    VMOVE(rpc_s.rpc_Bunit, rpc->rpc_B);
    VUNITIZE(rpc_s.rpc_Bunit);

    VCROSS(rpc_s.rpc_Runit, rpc_s.rpc_Bunit, rpc->rpc_H);
    VUNITIZE(rpc_s.rpc_Runit);

    /* plot parabolic contour curve of face containing V */
    VSETALL(rpc_H, 0.0);
    rpc_plot_parabolic_curve(vlfree, vhead, &rpc_s, pts, rpc_H, 1.0);
    rpc_plot_parabolic_curve(vlfree, vhead, &rpc_s, pts, rpc_H, -1.0);

    /* plot parabolic contour curve of opposing face */
    VMOVE(rpc_H, rpc->rpc_H);
    rpc_plot_parabolic_curve(vlfree, vhead, &rpc_s, pts, rpc_H, 1.0);
    rpc_plot_parabolic_curve(vlfree, vhead, &rpc_s, pts, rpc_H, -1.0);
}

static void
rpc_plot_curve_connections(
	struct bu_list *vlfree,
	struct bu_list *vhead,
	struct rt_rpc_internal *rpc,
	int num_connections)
{
    point_t pt;
    vect_t Yu, Zu;
    fastf_t mag_Z;
    fastf_t p, y, z, z_step;
    int connections_per_half;

    if (num_connections < 1) {
	return;
    }

    VMOVE(Zu, rpc->rpc_B);
    VCROSS(Yu, rpc->rpc_H, Zu);
    VUNITIZE(Yu);
    VUNITIZE(Zu);

    mag_Z = MAGNITUDE(rpc->rpc_B);

    p = rpc_parabola_p(rpc->rpc_r, mag_Z);

    connections_per_half = 0.5 + num_connections / 2.0;
    z_step = mag_Z / (connections_per_half + 1.0);

    for (z = 0.0; z <= mag_Z; z += z_step) {
	y = rpc_parabola_y(p, mag_Z - z);

	/* connect faces on one side of the curve */
	VJOIN2(pt, rpc->rpc_V, z, Zu, -y, Yu);
	BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_MOVE);

	VADD2(pt, pt, rpc->rpc_H);
	BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_DRAW);

	/* connect the faces on the other side */
	VJOIN2(pt, rpc->rpc_V, z, Zu, y, Yu);
	BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_MOVE);

	VADD2(pt, pt, rpc->rpc_H);
	BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_LINE_DRAW);
    }
}

static int
rpc_curve_points(
	struct rt_rpc_internal *rpc,
	fastf_t point_spacing)
{
    fastf_t height, halfwidth, est_curve_length;
    point_t p0, p1;

    height = -MAGNITUDE(rpc->rpc_B);
    halfwidth = rpc->rpc_r;

    VSET(p0, 0.0, 0.0, height);
    VSET(p1, 0.0, halfwidth, 0.0);

    est_curve_length = 2.0 * DIST_PNT_PNT(p0, p1);

    return est_curve_length / point_spacing;
}

int
rt_rpc_adaptive_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bn_tol *tol, const struct bview *v, fastf_t s_size)
{
    point_t p;
    vect_t rpc_R;
    int num_curve_points, num_connections;
    struct rt_rpc_internal *rpc;
    struct rt_pnt_node *pts, *node, *tmp;
    struct bu_list *vlfree = &rt_vlfree;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);

    rpc = (struct rt_rpc_internal *)ip->idb_ptr;
    if (!rpc_is_valid(rpc)) {
	return -2;
    }

    fastf_t point_spacing = solid_point_spacing(v, s_size);
    num_curve_points = rpc_curve_points(rpc, point_spacing);

    if (num_curve_points < 3) {
	num_curve_points = 3;
    }

    VCROSS(rpc_R, rpc->rpc_B, rpc->rpc_H);
    VUNITIZE(rpc_R);
    VSCALE(rpc_R, rpc_R, rpc->rpc_r);

    pts = rpc_parabolic_curve(MAGNITUDE(rpc->rpc_B), rpc->rpc_r, num_curve_points);
    rpc_plot_parabolas(vlfree, vhead, rpc, pts);

    node = pts;
    while (node != NULL) {
	tmp = node;
	node = node->next;

	bu_free(tmp, "rt_pnt_node");
    }

    /* connect both halves of the parabolic contours of the opposing faces */
    num_connections = primitive_curve_count(ip, tol, v->gv_ls.curve_scale, s_size);
    if (num_connections < 2) {
	num_connections = 2;
    }

    rpc_plot_curve_connections(vlfree, vhead, rpc, num_connections);

    /* plot rectangular face */
    VADD2(p, rpc->rpc_V, rpc_R);
    BV_ADD_VLIST(vlfree, vhead, p, BV_VLIST_LINE_MOVE);

    VADD2(p, p, rpc->rpc_H);
    BV_ADD_VLIST(vlfree, vhead, p, BV_VLIST_LINE_DRAW);

    VJOIN1(p, p, -2.0, rpc_R);
    BV_ADD_VLIST(vlfree, vhead, p, BV_VLIST_LINE_DRAW);

    VJOIN1(p, p, -1.0, rpc->rpc_H);
    BV_ADD_VLIST(vlfree, vhead, p, BV_VLIST_LINE_DRAW);

    VJOIN1(p, p, 2.0, rpc_R);
    BV_ADD_VLIST(vlfree, vhead, p, BV_VLIST_LINE_DRAW);

    return 0;
}

int
rt_rpc_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    struct rt_rpc_internal *xip;
    fastf_t *front;
    fastf_t *back;
    fastf_t b, dtol, ntol, rh;
    int i, n;
    struct rt_pnt_node *old, *pos, *pts;
    vect_t Bu, Hu, Ru, B, R;
    struct bu_list *vlfree = &rt_vlfree;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);

    xip = (struct rt_rpc_internal *)ip->idb_ptr;
    if (!rpc_is_valid(xip)) {
	return -2;
    }

    /* compute |B| |H| */
    b = MAGNITUDE(xip->rpc_B);	/* breadth */
    rh = xip->rpc_r;		/* rectangular halfwidth */

    /* make unit vectors in B, H, and BxH directions */
    VMOVE(Hu, xip->rpc_H);
    VUNITIZE(Hu);
    VMOVE(Bu, xip->rpc_B);
    VUNITIZE(Bu);
    VCROSS(Ru, Bu, Hu);

    if (rh < b) {
	dtol = primitive_get_absolute_tolerance(ttol, 2.0 * rh);
    } else {
	dtol = primitive_get_absolute_tolerance(ttol, 2.0 * b);
    }

    /* To ensure normal tolerance, remain below this angle */
    if (ttol->norm > 0.0)
	ntol = ttol->norm;
    else
	/* tolerate everything */
	ntol = M_PI;

    /* initial parabola approximation is a single segment */
    BU_ALLOC(pts, struct rt_pnt_node);
    BU_ALLOC(pts->next, struct rt_pnt_node);

    pts->next->next = NULL;
    VSET(pts->p,       0.0, -rh, 0.0);
    VSET(pts->next->p, 0.0,  rh, 0.0);
    /* 2 endpoints in 1st approximation */
    n = 2;
    /* recursively break segment 'til within error tolerances */
    n += rt_mk_parabola(pts, rh, b, dtol, ntol);

    /* get mem for arrays */
    front = (fastf_t *)bu_malloc(3*n * sizeof(fastf_t), "fastf_t");
    back  = (fastf_t *)bu_malloc(3*n * sizeof(fastf_t), "fastf_t");

    /* generate front & back plates in world coordinates */
    pos = pts;
    i = 0;
    while (pos) {
	/* get corresponding rpc contour point in B-R plane from the parabola
	 * point in the Y-Z plane
	 */
	VSCALE(R, Ru, pos->p[Y]);
	VSCALE(B, Bu, -pos->p[Z]);
	VADD2(&front[i], R, B);

	/* move to origin vertex origin */
	VADD2(&front[i], &front[i], xip->rpc_V);

	/* extrude front to create back plate */
	VADD2(&back[i], &front[i], xip->rpc_H);

	i += 3;
	old = pos;
	pos = pos->next;
	bu_free((char *)old, "rt_pnt_node");
    }

    /* Draw the front */
    BV_ADD_VLIST(vlfree, vhead, &front[(n-1)*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < n; i++) {
	BV_ADD_VLIST(vlfree, vhead, &front[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    /* Draw the back */
    BV_ADD_VLIST(vlfree, vhead, &back[(n-1)*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < n; i++) {
	BV_ADD_VLIST(vlfree, vhead, &back[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    /* Draw connections */
    for (i = 0; i < n; i++) {
	BV_ADD_VLIST(vlfree, vhead, &front[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, &back[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    bu_free((char *)front, "fastf_t");
    bu_free((char *)back,  "fastf_t");

    return 0;
}


/**
 * Approximate a parabola with line segments.  The initial single
 * segment is broken at the point farthest from the parabola if
 * that point is not already within the distance and normal error
 * tolerances.  The two resulting segments are passed recursively
 * to this routine until each segment is within tolerance.
 */
int
rt_mk_parabola(struct rt_pnt_node *pts, fastf_t r, fastf_t b, fastf_t dtol, fastf_t ntol)
{
    fastf_t dist, intr, m, theta0, theta1;
    int n;
    point_t mpt, p0, p1;
    vect_t norm_line, norm_parab;
    struct rt_pnt_node *newpt;

#define RPC_TOL .0001
    /* endpoints of segment approximating parabola */
    VMOVE(p0, pts->p);
    VMOVE(p1, pts->next->p);
    /* slope and intercept of segment */
    m = (p1[Z] - p0[Z]) / (p1[Y] - p0[Y]);
    intr = p0[Z] - m * p0[Y];
    /* point on parabola with max dist between parabola and line */
    mpt[X] = 0.0;
    mpt[Y] = (r * r * m) / (2.0 * b);
    if (NEAR_ZERO(mpt[Y], RPC_TOL))
	mpt[Y] = 0.0;
    mpt[Z] = (mpt[Y] * m / 2.0) - b;
    if (NEAR_ZERO(mpt[Z], RPC_TOL))
	mpt[Z] = 0.0;
    /* max distance between that point and line */
    dist = fabs(mpt[Z] + b + b + intr) / sqrt(m * m + 1.0);
    /* angles between normal of line and of parabola at line endpoints */
    VSET(norm_line, m, -1.0, 0.0);
    VSET(norm_parab, 2.0 * b / (r * r) * p0[Y], -1.0, 0.0);
    VUNITIZE(norm_line);
    VUNITIZE(norm_parab);
    theta0 = fabs(acos(VDOT(norm_line, norm_parab)));
    VSET(norm_parab, 2.0 * b / (r * r) * p1[Y], -1.0, 0.0);
    VUNITIZE(norm_parab);
    theta1 = fabs(acos(VDOT(norm_line, norm_parab)));
    /* split segment at widest point if not within error tolerances */
    if (dist > dtol || theta0 > ntol || theta1 > ntol) {
	/* split segment */
	BU_ALLOC(newpt, struct rt_pnt_node);
	VMOVE(newpt->p, mpt);
	newpt->next = pts->next;
	pts->next = newpt;
	/* keep track of number of pts added */
	n = 1;
	/* recurse on first new segment */
	n += rt_mk_parabola(pts, r, b, dtol, ntol);
	/* recurse on second new segment */
	n += rt_mk_parabola(newpt, r, b, dtol, ntol);
    } else
	n  = 0;
    return n;
}


/**
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_rpc_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    int i, j, n;
    fastf_t b, *back, *front, rh;
    fastf_t dtol, ntol;
    vect_t Bu, Hu, Ru;
    mat_t R;
    mat_t invR;
    struct rt_rpc_internal *xip;
    struct rt_pnt_node *old, *pos, *pts;
    struct shell *s;
    struct faceuse **outfaceuses;
    struct vertex **vfront, **vback, **vtemp, *vertlist[4];
    vect_t *norms;
    fastf_t r_sq_over_b;
    struct bu_list *vlfree = &rt_vlfree;

    NMG_CK_MODEL(m);
    BN_CK_TOL(tol);
    BG_CK_TESS_TOL(ttol);

    RT_CK_DB_INTERNAL(ip);

    xip = (struct rt_rpc_internal *)ip->idb_ptr;
    if (!rpc_is_valid(xip)) {
	return -2;
    }

    /* compute |B| |H| */
    b = MAGNITUDE(xip->rpc_B);	/* breadth */
    rh = xip->rpc_r;		/* rectangular halfwidth */

    /* make unit vectors in B, H, and BxH directions */
    VMOVE(Hu, xip->rpc_H);
    VUNITIZE(Hu);
    VMOVE(Bu, xip->rpc_B);
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
    if (ttol->norm > 0.0)
	ntol = ttol->norm;
    else
	/* tolerate everything */
	ntol = M_PI;

    /* initial parabola approximation is a single segment */
    BU_ALLOC(pts, struct rt_pnt_node);
    BU_ALLOC(pts->next, struct rt_pnt_node);

    pts->next->next = NULL;
    VSET(pts->p,       0.0, -rh, 0.0);
    VSET(pts->next->p, 0.0,  rh, 0.0);
    /* 2 endpoints in 1st approximation */
    n = 2;
    /* recursively break segment 'til within error tolerances */
    n += rt_mk_parabola(pts, rh, b, dtol, ntol);

    /* get mem for arrays */
    front = (fastf_t *)bu_malloc(3*n * sizeof(fastf_t), "fastf_t");
    back  = (fastf_t *)bu_malloc(3*n * sizeof(fastf_t), "fastf_t");
    norms = (vect_t *)bu_calloc(n, sizeof(vect_t), "rt_rpc_tess: norms");
    vfront = (struct vertex **)bu_malloc((n+1) * sizeof(struct vertex *), "vertex *");
    vback = (struct vertex **)bu_malloc((n+1) * sizeof(struct vertex *), "vertex *");
    vtemp = (struct vertex **)bu_malloc((n+1) * sizeof(struct vertex *), "vertex *");
    outfaceuses = (struct faceuse **)bu_malloc((n+2) * sizeof(struct faceuse *), "faceuse *");

    /* generate front & back plates in world coordinates */
    r_sq_over_b = rh * rh / b;
    pos = pts;
    i = 0;
    j = 0;
    while (pos) {
	vect_t tmp_norm;

	VSET(tmp_norm, 0.0, 2.0 * pos->p[Y], -r_sq_over_b);
	MAT4X3VEC(norms[j], invR, tmp_norm);
	VUNITIZE(norms[j]);
	/* rotate back to original position */
	MAT4X3VEC(&front[i], invR, pos->p);
	/* move to origin vertex origin */
	VADD2(&front[i], &front[i], xip->rpc_V);
	/* extrude front to create back plate */
	VADD2(&back[i], &front[i], xip->rpc_H);
	i += 3;
	j++;
	old = pos;
	pos = pos->next;
	bu_free((char *)old, "rt_pnt_node");
    }

    *r = nmg_mrsv(m);	/* Make region, empty shell, vertex */
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    for (i=0; i<n; i++) {
	vfront[i] = vtemp[i] = (struct vertex *)0;
    }

    /* Front face topology.  Verts are considered to go CCW */
    outfaceuses[0] = nmg_cface(s, vfront, n);

    (void)nmg_mark_edges_real(&outfaceuses[0]->l.magic, vlfree);

    /* Back face topology.  Verts must go in opposite dir (CW) */
    outfaceuses[1] = nmg_cface(s, vtemp, n);

    (void)nmg_mark_edges_real(&outfaceuses[1]->l.magic, vlfree);

    for (i=0; i<n; i++) vback[i] = vtemp[n-1-i];

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

    (void)nmg_mark_edges_real(&outfaceuses[n+1]->l.magic, vlfree);

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
	    /* free mem */
	    bu_free((char *)front, "fastf_t");
	    bu_free((char *)back, "fastf_t");
	    bu_free((char*)vfront, "vertex *");
	    bu_free((char*)vback, "vertex *");
	    bu_free((char*)vtemp, "vertex *");
	    bu_free((char*)outfaceuses, "faceuse *");

	    return -1;		/* FAIL */
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
    nmg_gluefaces(outfaceuses, n+2, vlfree, tol);

    /* Compute "geometry" for region and shell */
    nmg_region_a(*r, tol);

    /* free mem */
    bu_free((char *)front, "fastf_t");
    bu_free((char *)back, "fastf_t");
    bu_free((char*)vfront, "vertex *");
    bu_free((char*)vback, "vertex *");
    bu_free((char*)vtemp, "vertex *");
    bu_free((char*)outfaceuses, "faceuse *");

    return 0;
}


/**
 * Import an RPC from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_rpc_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_rpc_internal *xip;
    union record *rp;
    vect_t v1, v2, v3;

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != ID_SOLID) {
	bu_log("rt_rpc_import4: defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_RPC;
    ip->idb_meth = &OBJ[ID_RPC];
    BU_ALLOC(ip->idb_ptr, struct rt_rpc_internal);

    xip = (struct rt_rpc_internal *)ip->idb_ptr;
    xip->rpc_magic = RT_RPC_INTERNAL_MAGIC;

    /* Warning:  type conversion */
    if (mat == NULL) mat = bn_mat_identity;

    if (dbip && dbip->dbi_version < 0) {
	flip_fastf_float(v1, &rp->s.s_values[0*3], 1, 1);
	flip_fastf_float(v2, &rp->s.s_values[1*3], 1, 1);
	flip_fastf_float(v3, &rp->s.s_values[2*3], 1, 1);
    } else {
	VMOVE(v1, &rp->s.s_values[0*3]);
	VMOVE(v2, &rp->s.s_values[1*3]);
	VMOVE(v3, &rp->s.s_values[2*3]);
    }

    MAT4X3PNT(xip->rpc_V, mat, v1);
    MAT4X3VEC(xip->rpc_H, mat, v2);
    MAT4X3VEC(xip->rpc_B, mat, v3);

    if (dbip && dbip->dbi_version < 0) {
	v1[X] = flip_dbfloat(rp->s.s_values[3*3+0]);
    } else {
	v1[X] = rp->s.s_values[3*3+0];
    }

    xip->rpc_r = v1[X] / mat[15];

    if (xip->rpc_r <= SMALL_FASTF) {
	bu_log("rt_rpc_import4: r is zero\n");
	bu_free((char *)ip->idb_ptr, "rt_rpc_import4: ip->idp_ptr");
	return -1;
    }

    return 0;			/* OK */
}


/**
 * The name is added by the caller, in the usual place.
 */
int
rt_rpc_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_rpc_internal *xip;
    union record *rpc;
    fastf_t f, mag_b, mag_h;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_RPC) return -1;
    xip = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(xip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "rpc external");
    rpc = (union record *)ep->ext_buf;

    rpc->s.s_id = ID_SOLID;
    rpc->s.s_type = RPC;

    mag_b = MAGNITUDE(xip->rpc_B);
    mag_h = MAGNITUDE(xip->rpc_H);

    if (mag_b < RT_LEN_TOL || mag_h < RT_LEN_TOL || xip->rpc_r < RT_LEN_TOL) {
	bu_log("rt_rpc_export4: not all dimensions positive!\n");
	return -1;
    }

    f = VDOT(xip->rpc_B, xip->rpc_H) / (mag_b * mag_h);
    if (!NEAR_ZERO(f, RT_DOT_TOL)) {
	bu_log("rt_rpc_export4: B and H are not perpendicular! (dot = %g)\n", f);
	return -1;
    }

    /* Warning:  type conversion */
    VSCALE(&rpc->s.s_values[0*3], xip->rpc_V, local2mm);
    VSCALE(&rpc->s.s_values[1*3], xip->rpc_H, local2mm);
    VSCALE(&rpc->s.s_values[2*3], xip->rpc_B, local2mm);
    rpc->s.s_values[3*3] = xip->rpc_r * local2mm;

    return 0;
}

int
rt_rpc_mat(struct rt_db_internal *rop, const mat_t mat, const struct rt_db_internal *ip)
{
    if (!rop || !ip || !mat)
	return BRLCAD_OK;

    struct rt_rpc_internal *tip = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(tip);
    struct rt_rpc_internal *top = (struct rt_rpc_internal *)rop->idb_ptr;
    RT_RPC_CK_MAGIC(top);


    /* Sanity */
    if (tip->rpc_r / mat[15] <= SMALL_FASTF) {
	bu_log("rt_rpc_mat: r is zero\n");
	return BRLCAD_ERROR;
    }

    /* Apply modeling transformations */
    vect_t rV, rH, rB;
    VMOVE(rV, tip->rpc_V);
    VMOVE(rH, tip->rpc_H);
    VMOVE(rB, tip->rpc_B);
    MAT4X3PNT(top->rpc_V, mat, rV);
    MAT4X3VEC(top->rpc_H, mat, rH);
    MAT4X3VEC(top->rpc_B, mat, rB);
    top->rpc_r = tip->rpc_r / mat[15];

    return BRLCAD_OK;
}

/**
 * Import an RPC from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_rpc_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_rpc_internal *xip;

    /* must be double for import and export */
    double vec[10];

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    BU_ASSERT(ep->ext_nbytes == SIZEOF_NETWORK_DOUBLE * 10);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_RPC;
    ip->idb_meth = &OBJ[ID_RPC];
    BU_ALLOC(ip->idb_ptr, struct rt_rpc_internal);

    xip = (struct rt_rpc_internal *)ip->idb_ptr;
    xip->rpc_magic = RT_RPC_INTERNAL_MAGIC;

    /* Convert from database (network) to internal (host) format */
    bu_cv_ntohd((unsigned char *)vec, ep->ext_buf, 10);

    /* Sanity */
    if (mat == NULL) mat = bn_mat_identity;
    double rpc_r = vec[3*3] / mat[15];
    if (rpc_r <= SMALL_FASTF) {
	bu_log("rt_rpc_import4: r is zero\n");
	bu_free((char *)ip->idb_ptr, "rt_rpc_import4: ip->idp_ptr");
	return -1;
    }

    /* Assign parameters */
    VMOVE(xip->rpc_V, &vec[0*3]);
    VMOVE(xip->rpc_H, &vec[1*3]);
    VMOVE(xip->rpc_B, &vec[2*3]);
    xip->rpc_r = vec[3*3];

    /* Apply modeling transformations */
    return rt_rpc_mat(ip, mat, ip);
}


/**
 * The name is added by the caller, in the usual place.
 */
int
rt_rpc_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_rpc_internal *xip;
    fastf_t f, mag_b, mag_h;

    /* must be double for import and export */
    double vec[10];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_RPC) return -1;
    xip = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(xip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * 10;
    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "rpc external");

    mag_b = MAGNITUDE(xip->rpc_B);
    mag_h = MAGNITUDE(xip->rpc_H);

    if (mag_b < RT_LEN_TOL || mag_h < RT_LEN_TOL || xip->rpc_r < RT_LEN_TOL) {
	bu_log("rt_rpc_export4: not all dimensions positive!\n");
	return -1;
    }

    f = VDOT(xip->rpc_B, xip->rpc_H) / (mag_b * mag_h);
    if (!NEAR_ZERO(f, RT_DOT_TOL)) {
	bu_log("rt_rpc_export4: B and H are not perpendicular! (dot = %g)\n", f);
	return -1;
    }

    /* scale 'em into local buffer */
    VSCALE(&vec[0*3], xip->rpc_V, local2mm);
    VSCALE(&vec[1*3], xip->rpc_H, local2mm);
    VSCALE(&vec[2*3], xip->rpc_B, local2mm);
    vec[3*3] = xip->rpc_r * local2mm;

    /* Convert from internal (host) to database (network) format */
    bu_cv_htond(ep->ext_buf, (unsigned char *)vec, 10);

    return 0;
}


/**
 * Make human-readable formatted presentation of this solid.
 * First line describes type of solid.
 * Additional lines are indented one tab, and give parameter values.
 */
int
rt_rpc_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_rpc_internal *xip =
	(struct rt_rpc_internal *)ip->idb_ptr;
    char buf[256];

    RT_RPC_CK_MAGIC(xip);
    bu_vls_strcat(str, "Right Parabolic Cylinder (RPC)\n");

    if (!verbose)
	return 0;

    sprintf(buf, "\tV (%g, %g, %g)\n",
	    INTCLAMP(xip->rpc_V[X] * mm2local),
	    INTCLAMP(xip->rpc_V[Y] * mm2local),
	    INTCLAMP(xip->rpc_V[Z] * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tB (%g, %g, %g) mag=%g\n",
	    INTCLAMP(xip->rpc_B[X] * mm2local),
	    INTCLAMP(xip->rpc_B[Y] * mm2local),
	    INTCLAMP(xip->rpc_B[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(xip->rpc_B) * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tH (%g, %g, %g) mag=%g\n",
	    INTCLAMP(xip->rpc_H[X] * mm2local),
	    INTCLAMP(xip->rpc_H[Y] * mm2local),
	    INTCLAMP(xip->rpc_H[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(xip->rpc_H) * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tr=%g\n", INTCLAMP(xip->rpc_r * mm2local));
    bu_vls_strcat(str, buf);

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_rpc_ifree(struct rt_db_internal *ip)
{
    struct rt_rpc_internal *xip;

    RT_CK_DB_INTERNAL(ip);

    xip = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(xip);
    xip->rpc_magic = 0;		/* sanity */

    bu_free((char *)xip, "rpc ifree");
    ip->idb_ptr = ((void *)0);	/* sanity */
}

void
rt_rpc_make(const struct rt_functab *ftp, struct rt_db_internal *intern)
{
    struct rt_rpc_internal* rpc_ip;

    intern->idb_type = ID_RPC;
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;

    BU_ASSERT(&OBJ[intern->idb_type] == ftp);
    intern->idb_meth = ftp;

    BU_ALLOC(rpc_ip, struct rt_rpc_internal);
    intern->idb_ptr = (void *)rpc_ip;

    rpc_ip->rpc_magic = RT_RPC_INTERNAL_MAGIC;
    VSETALL(rpc_ip->rpc_V, 0);
    VSET(rpc_ip->rpc_H, 0.0, 0.0, 1.0);
    VSET(rpc_ip->rpc_B, 0.0, 1.0, 0.0);
    rpc_ip->rpc_r = 1.0;

}


int
rt_rpc_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


void
rt_rpc_volume(fastf_t *vol, const struct rt_db_internal *ip)
{
    fastf_t mag_h, mag_b, mag_r;
    struct rt_rpc_internal *xip = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(xip);

    mag_h = MAGNITUDE(xip->rpc_H);
    mag_b = MAGNITUDE(xip->rpc_B);
    mag_r = xip->rpc_r;

    *vol = 4.0/3.0 * mag_b * mag_r * mag_h;
}


void
rt_rpc_centroid(point_t *cent, const struct rt_db_internal *ip)
{
    struct rt_rpc_internal *xip = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(xip);

    /* centroid of a parabolic section is
     * 0.4 * h where h is a vector from
     * the base to the vertex of the parabola */
    VJOIN1(*cent, xip->rpc_V, 0.4, xip->rpc_B);

    /* cent now stores the centroid of the
     * parabolic section representing the base
     * of the rpc */
    VJOIN1(*cent, *cent, 0.5, xip->rpc_H);
}


void
rt_rpc_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    fastf_t area_base, area_shell, area_rect;
    fastf_t mag_b, mag_r, mag_h;
    fastf_t magsq_b, magsq_r;
    struct rt_rpc_internal *xip = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(xip);

    mag_h = MAGNITUDE(xip->rpc_H);
    magsq_b = MAGSQ(xip->rpc_B);
    mag_r = xip->rpc_r;
    mag_b = sqrt(magsq_b);
    magsq_r = mag_r * mag_r;

    area_base = 4.0/3.0 * mag_b * mag_r;

    area_shell = 0.5 * sqrt(magsq_r + 4.0 * magsq_b) + 0.25 * magsq_r /
	mag_b * asinh(2.0 * mag_b / mag_r);
    area_shell *= 2.0;

    area_rect = 2.0 * mag_r * mag_h;

    *area = 2.0 * area_base + area_rect + area_shell;
}

static int
rpc_is_valid(struct rt_rpc_internal *rpc)
{
    fastf_t mag_h, mag_b, cos_angle_bh;
    vect_t rpc_H, rpc_B;

    RT_RPC_CK_MAGIC(rpc);

    VMOVE(rpc_H, rpc->rpc_H);
    mag_h = MAGNITUDE(rpc_H);

    VMOVE(rpc_B, rpc->rpc_B);
    mag_b = MAGNITUDE(rpc_B);

    /* Check for |H| > 0, |B| > 0, |R| > 0 */
    if (NEAR_ZERO(mag_h, RT_LEN_TOL)
	|| NEAR_ZERO(mag_b, RT_LEN_TOL)
	|| NEAR_ZERO(rpc->rpc_r, RT_LEN_TOL))
    {
	return 0;
    }

    /* check B and H are orthogonal */
    cos_angle_bh = VDOT(rpc_B, rpc_H) / (mag_b * mag_h);
    if (!NEAR_ZERO(cos_angle_bh, RT_DOT_TOL)) {
	return 0;
    }

    return 1;
}

int
rt_rpc_labels(struct rt_point_labels *pl, int pl_max, const mat_t xform, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    int lcnt = 4;
    if (!pl || pl_max < lcnt || !ip)
	return 0;

    struct rt_rpc_internal *rpc = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(rpc);

    vect_t Ru;
    point_t work, pos_view;
    int npl = 0;

#define POINT_LABEL(_pt, _char) { \
    VMOVE(pl[npl].pt, _pt); \
    pl[npl].str[0] = _char; \
    pl[npl++].str[1] = '\0'; }

    MAT4X3PNT(pos_view, xform, rpc->rpc_V);
    POINT_LABEL(pos_view, 'V');

    VADD2(work, rpc->rpc_V, rpc->rpc_B);
    MAT4X3PNT(pos_view, xform, work);
    POINT_LABEL(pos_view, 'B');

    VADD2(work, rpc->rpc_V, rpc->rpc_H);
    MAT4X3PNT(pos_view, xform, work);
    POINT_LABEL(pos_view, 'H');

    VCROSS(Ru, rpc->rpc_B, rpc->rpc_H);
    VUNITIZE(Ru);
    VSCALE(Ru, Ru, rpc->rpc_r);
    VADD2(work, rpc->rpc_V, Ru);
    MAT4X3PNT(pos_view, xform, work);
    POINT_LABEL(pos_view, 'r');

    return lcnt;
}

const char *
rt_rpc_keypoint(point_t *pt, const char *keystr, const mat_t mat, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    if (!pt || !ip)
	return NULL;

    point_t mpt = VINIT_ZERO;
    struct rt_rpc_internal *rpc = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(rpc);

    static const char *default_keystr = "V";
    const char *k = (keystr) ? keystr : default_keystr;

    if (BU_STR_EQUAL(k, default_keystr)) {
	VMOVE(mpt, rpc->rpc_V);
	goto rpc_kpt_end;
    }

    // No keystr matches - failed
    return NULL;

rpc_kpt_end:

    MAT4X3PNT(*pt, mat, mpt);

    return k;
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
