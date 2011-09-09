/*                         T O R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file primitives/tor/tor.c
 *
 * Intersect a ray with a Torus
 *
 * Algorithm:
 *
 * Given V, H, A, and B, there is a set of points on this torus
 *
 * { (x, y, z) | (x, y, z) is on torus defined by V, H, A, B }
 *
 * Through a series of Transformations, this set will be transformed
 * into a set of points on a unit torus (R1==1) centered at the origin
 * which lies on the X-Y plane (ie, H is on the Z axis).
 *
 * { (x', y', z') | (x', y', z') is on unit torus at origin }
 *
 * The transformation from X to X' is accomplished by:
 *
 * X' = S(R(X - V))
 *
 * where R(X) = (A/(|A|))
 *		(B/(|B|)) . X
 *		(H/(|H|))
 *
 * and S(X) =	(1/|A|   0     0)
 *		(0    1/|A|   0) . X
 *		(0      0   1/|A|)
 * where |A| = R1
 *
 * To find the intersection of a line with the torus, consider the
 * parametric line L:
 *
 * L : { P(n) | P + t(n) . D }
 *
 * Call W the actual point of intersection between L and the torus.
 * Let W' be the point of intersection between L' and the unit torus.
 *
 * L' : { P'(n) | P' + t(n) . D' }
 *
 * W = invR(invS(W')) + V
 *
 * Where W' = k D' + P'.
 *
 * Given a line and a ratio, alpha, finds the equation of the unit
 * torus in terms of the variable 't'.
 *
 * The equation for the torus is:
 *
 * [ X**2 + Y**2 + Z**2 + (1 - alpha**2) ]**2 - 4*(X**2 + Y**2)  =  0
 *
 * First, find X, Y, and Z in terms of 't' for this line, then
 * substitute them into the equation above.
 *
 * Wx = Dx*t + Px
 *
 * Wx**2 = Dx**2 * t**2 +  2 * Dx * Px +  Px**2
 *
 * The real roots of the equation in 't' are the intersect points
 * along the parameteric line.
 *
 * NORMALS.  Given the point W on the torus, what is the vector normal
 * to the tangent plane at that point?
 *
 * Map W onto the unit torus, ie: W' = S(R(W - V)).  In this case,
 * we find W' by solving the parameteric line given k.
 *
 * The gradient of the torus at W' is in fact the normal vector.
 *
 * Given that the equation for the unit torus is:
 *
 * [ X**2 + Y**2 + Z**2 + (1 - alpha**2) ]**2 - 4*(X**2 + Y**2)  =  0
 *
 * let w = X**2 + Y**2 + Z**2 + (1 - alpha**2), then the equation becomes:
 *
 * w**2 - 4*(X**2 + Y**2)  =  0
 *
 * For f(x, y, z) = 0, the gradient of f() is (df/dx, df/dy, df/dz).
 *
 * df/dx = 2 * w * 2 * x - 8 * x	= (4 * w - 8) * x
 * df/dy = 2 * w * 2 * y - 8 * y	= (4 * w - 8) * y
 * df/dz = 2 * w * 2 * z		= 4 * w * z
 *
 * Note that the normal vector produced above will not have unit
 * length.  Also, to make this useful for the original torus, it will
 * have to be rotated back to the orientation of the original torus.
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


/*
 * The TORUS has the following input fields:
 *	V	V from origin to center
 *	H	Radius Vector, Normal to plane of torus.  |H| = R2
 *	A, B	perpindicular, to CENTER of torus.  |A|==|B|==R1
 *	F5, F6	perpindicular, for inner edge (unused)
 *	F7, F8	perpindicular, for outer edge (unused)
 *
 */

const struct bu_structparse rt_tor_parse[] = {
    {"%f", 3, "V",   bu_offsetof(struct rt_tor_internal, v[X]), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 3, "H",   bu_offsetof(struct rt_tor_internal, h[X]), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 1, "r_a", bu_offsetof(struct rt_tor_internal, r_a),  BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 1, "r_h", bu_offsetof(struct rt_tor_internal, r_h),  BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {{'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};


struct tor_specific {
    fastf_t tor_alpha;	/* 0 < (R2/R1) <= 1 */
    fastf_t tor_r1;		/* for inverse scaling of k values. */
    fastf_t tor_r2;		/* for curvature */
    vect_t tor_V;		/* Vector to center of torus */
    vect_t tor_N;		/* unit normal to plane of torus */
    mat_t tor_SoR;	/* Scale(Rot(vect)) */
    mat_t tor_invR;	/* invRot(vect') */
};

/**
 * R T _ T O R _ B B O X
 *
 * Compute the bounding RPP for a circular torus.
 */
int
rt_tor_bbox(struct rt_db_internal *ip, point_t *min, point_t *max) {
    vect_t P, w1;	/* for RPP calculation */
    fastf_t f;
    struct rt_tor_internal *tip = (struct rt_tor_internal *)ip->idb_ptr;
    RT_TOR_CK_MAGIC(tip);

   /* Compute the bounding RPP planes for a circular torus.
    *
    * Given a circular torus with vertex V, vector N, and radii r1
    * and r2.  A bounding plane with direction vector P will touch
    * the surface of the torus at the points:
    *
    * V +/- [r2 + r1 * |N x P|] P
    */
   /* X */
   VSET(P, 1.0, 0, 0);		/* bounding plane normal */
   VCROSS(w1, tip->h, P);	/* for sin(angle N P) */
   f = tip->r_h + tip->r_a * MAGNITUDE(w1);
   VSCALE(w1, P, f);
   f = fabs(w1[X]);
   (*min)[X] = tip->v[X] - f;
   (*max)[X] = tip->v[X] + f;

   /* Y */
   VSET(P, 0, 1.0, 0);		/* bounding plane normal */
   VCROSS(w1, tip->h, P);	/* for sin(angle N P) */
   f = tip->r_h + tip->r_a * MAGNITUDE(w1);
   VSCALE(w1, P, f);
   f = fabs(w1[Y]);
   (*min)[Y] = tip->v[Y] - f;
   (*max)[Y] = tip->v[Y] + f;

   /* Z */
   VSET(P, 0, 0, 1.0);		/* bounding plane normal */
   VCROSS(w1, tip->h, P);	/* for sin(angle N P) */
   f = tip->r_h + tip->r_a * MAGNITUDE(w1);
   VSCALE(w1, P, f);
   f = fabs(w1[Z]);
   (*min)[Z] = tip->v[Z] - f;
   (*max)[Z] = tip->v[Z] + f;

   return 0;
}


/**
 * R T _ T O R _ P R E P
 *
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid torus, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 TOR is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct tor_specific is created, and its address is stored in
 * stp->st_specific for use by rt_tor_shot().
 */
int
rt_tor_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    register struct tor_specific *tor;
    struct rt_tor_internal *tip;

    mat_t R;
    fastf_t f;

    tip = (struct rt_tor_internal *)ip->idb_ptr;
    RT_TOR_CK_MAGIC(tip);

    /* Validate that |A| == |B| (for now) */
    if (!NEAR_EQUAL(tip->r_a, tip->r_b, 0.001)) {
	bu_log("tor(%s):  (|A|=%f) != (|B|=%f) \n",
	       stp->st_name, tip->r_a, tip->r_b);
	return 1;		/* BAD */
    }

    /* Validate that A.B == 0, B.H == 0, A.H == 0 */
    f = VDOT(tip->a, tip->b)/(tip->r_a*tip->r_b);

    if (!NEAR_ZERO(f, rtip->rti_tol.dist)) {
	bu_log("tor(%s):  A not perpendicular to B, f=%f\n",
	       stp->st_name, f);
	return 1;		/* BAD */
    }
    f = VDOT(tip->b, tip->h)/(tip->r_b);
    if (!NEAR_ZERO(f, rtip->rti_tol.dist)) {
	bu_log("tor(%s):  B not perpendicular to H, f=%f\n",
	       stp->st_name, f);
	return 1;		/* BAD */
    }
    f = VDOT(tip->a, tip->h)/(tip->r_a);
    if (!NEAR_ZERO(f, rtip->rti_tol.dist)) {
	bu_log("tor(%s):  A not perpendicular to H, f=%f\n",
	       stp->st_name, f);
	return 1;		/* BAD */
    }

    /* Validate that 0 < r2 <= r1 for alpha computation */
    if (0.0 >= tip->r_h  || tip->r_h > tip->r_a) {
	bu_log("r1 = %f, r2 = %f\n", tip->r_a, tip->r_h);
	bu_log("tor(%s):  0 < r2 <= r1 is not true\n", stp->st_name);
	return 1;		/* BAD */
    }

    /* Solid is OK, compute constant terms now */
    BU_GETSTRUCT(tor, tor_specific);
    stp->st_specific = (genptr_t)tor;

    tor->tor_r1 = tip->r_a;
    tor->tor_r2 = tip->r_h;

    VMOVE(tor->tor_V, tip->v);
    tor->tor_alpha = tip->r_h/tor->tor_r1;

    /* Compute R and invR matrices */
    VMOVE(tor->tor_N, tip->h);

    MAT_IDN(R);
    VSCALE(&R[0], tip->a, 1.0/tip->r_a);
    VSCALE(&R[4], tip->b, 1.0/tip->r_b);
    VMOVE(&R[8], tor->tor_N);
    bn_mat_inv(tor->tor_invR, R);

    /* Compute SoR.  Here, S = I / r1 */
    MAT_COPY(tor->tor_SoR, R);
    tor->tor_SoR[15] *= tor->tor_r1;

    VMOVE(stp->st_center, tor->tor_V);
    stp->st_aradius = stp->st_bradius = tor->tor_r1 + tip->r_h;

    if (stp->st_meth->ft_bbox(ip, &(stp->st_min), &(stp->st_max))) {
	return 1;
    }
    
    return 0;			/* OK */
}


/**
 * R T _ T O R _ P R I N T
 */
void
rt_tor_print(register const struct soltab *stp)
{
    register const struct tor_specific *tor =
	(struct tor_specific *)stp->st_specific;

    bu_log("r2/r1 (alpha) = %f\n", tor->tor_alpha);
    bu_log("r1 = %f\n", tor->tor_r1);
    bu_log("r2 = %f\n", tor->tor_r2);
    VPRINT("V", tor->tor_V);
    VPRINT("N", tor->tor_N);
    bn_mat_print("S o R", tor->tor_SoR);
    bn_mat_print("invR", tor->tor_invR);
}


/**
 * R T _ T O R _ S H O T
 *
 * Intersect a ray with an torus, where all constant terms have been
 * precomputed by rt_tor_prep().  If an intersection occurs, one or
 * two struct seg(s) will be acquired and filled in.
 *
 * NOTE: All lines in this function are represented parametrically by
 * a point, P(x0, y0, z0) and a direction normal, D = ax + by + cz.
 * Any point on a line can be expressed by one variable 't', where
 *
 * X = a*t + x0,	eg, X = Dx*t + Px
 * Y = b*t + y0,
 * Z = c*t + z0.
 *
 * First, convert the line to the coordinate system of a "stan- dard"
 * torus.  This is a torus which lies in the X-Y plane, circles the
 * origin, and whose primary radius is one.  The secondary radius is
 * alpha = (R2/R1) of the original torus where (0 < alpha <= 1).
 *
 * Then find the equation of that line and the standard torus, which
 * turns out to be a quartic equation in 't'.  Solve the equation
 * using a general polynomial root finder.  Use those values of 't' to
 * compute the points of intersection in the original coordinate
 * system.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_tor_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    register struct tor_specific *tor =
	(struct tor_specific *)stp->st_specific;
    register struct seg *segp;
    vect_t dprime;		/* D' */
    vect_t pprime;		/* P' */
    vect_t work;		/* temporary vector */
    bn_poly_t C;		/* The final equation */
    bn_complex_t val[4];		/* The complex roots */
    double k[4];		/* The real roots */
    register int i;
    int j;
    bn_poly_t A, Asqr;
    bn_poly_t X2_Y2;		/* X**2 + Y**2 */
    vect_t cor_pprime;	/* new ray origin */
    fastf_t cor_proj;

    /* Convert vector into the space of the unit torus */
    MAT4X3VEC(dprime, tor->tor_SoR, rp->r_dir);
    VUNITIZE(dprime);

    VSUB2(work, rp->r_pt, tor->tor_V);
    MAT4X3VEC(pprime, tor->tor_SoR, work);

    /* normalize distance from torus.  substitute corrected pprime
     * which contains a translation along ray direction to closest
     * approach to vertex of torus.  Translating ray origin along
     * direction of ray to closest pt. to origin of solid's coordinate
     * system, new ray origin is 'cor_pprime'.
     */
    cor_proj = VDOT(pprime, dprime);
    VSCALE(cor_pprime, dprime, cor_proj);
    VSUB2(cor_pprime, pprime, cor_pprime);

    /* Given a line and a ratio, alpha, finds the equation of the unit
     * torus in terms of the variable 't'.
     *
     * The equation for the torus is:
     *
     * [ X**2 + Y**2 + Z**2 + (1 - alpha**2) ]**2 - 4*(X**2 + Y**2) = 0
     *
     * First, find X, Y, and Z in terms of 't' for this line, then
     * substitute them into the equation above.
     *
     * Wx = Dx*t + Px
     *
     * Wx**2 = Dx**2 * t**2  +  2 * Dx * Px  +  Px**2
     *		[0]                [1]           [2]    dgr=2
     */
    X2_Y2.dgr = 2;
    X2_Y2.cf[0] = dprime[X] * dprime[X] + dprime[Y] * dprime[Y];
    X2_Y2.cf[1] = 2.0 * (dprime[X] * cor_pprime[X] +
			 dprime[Y] * cor_pprime[Y]);
    X2_Y2.cf[2] = cor_pprime[X] * cor_pprime[X] +
	cor_pprime[Y] * cor_pprime[Y];

    /* A = X2_Y2 + Z2 */
    A.dgr = 2;
    A.cf[0] = X2_Y2.cf[0] + dprime[Z] * dprime[Z];
    A.cf[1] = X2_Y2.cf[1] + 2.0 * dprime[Z] * cor_pprime[Z];
    A.cf[2] = X2_Y2.cf[2] + cor_pprime[Z] * cor_pprime[Z] +
	1.0 - tor->tor_alpha * tor->tor_alpha;

    /* Inline expansion of (void) bn_poly_mul(&Asqr, &A, &A) */
    /* Both polys have degree two */
    Asqr.dgr = 4;
    Asqr.cf[0] = A.cf[0] * A.cf[0];
    Asqr.cf[1] = A.cf[0] * A.cf[1] + A.cf[1] * A.cf[0];
    Asqr.cf[2] = A.cf[0] * A.cf[2] + A.cf[1] * A.cf[1] + A.cf[2] * A.cf[0];
    Asqr.cf[3] = A.cf[1] * A.cf[2] + A.cf[2] * A.cf[1];
    Asqr.cf[4] = A.cf[2] * A.cf[2];

    /* Inline expansion of bn_poly_scale(&X2_Y2, 4.0) and
     * bn_poly_sub(&C, &Asqr, &X2_Y2).
     */
    C.dgr   = 4;
    C.cf[0] = Asqr.cf[0];
    C.cf[1] = Asqr.cf[1];
    C.cf[2] = Asqr.cf[2] - X2_Y2.cf[0] * 4.0;
    C.cf[3] = Asqr.cf[3] - X2_Y2.cf[1] * 4.0;
    C.cf[4] = Asqr.cf[4] - X2_Y2.cf[2] * 4.0;

    /* It is known that the equation is 4th order.  Therefore, if the
     * root finder returns other than 4 roots, error.
     */
    if ((i = rt_poly_roots(&C, val, stp->st_dp->d_namep)) != 4) {
	if (i > 0) {
	    bu_log("tor:  rt_poly_roots() 4!=%d\n", i);
	    bn_pr_roots(stp->st_name, val, i);
	} else if (i < 0) {
	    static int reported=0;
	    bu_log("The root solver failed to converge on a solution for %s\n", stp->st_dp->d_namep);
	    if (!reported) {
		VPRINT("while shooting from:\t", rp->r_pt);
		VPRINT("while shooting at:\t", rp->r_dir);
		bu_log("Additional torus convergence failure details will be suppressed.\n");
		reported=1;
	    }
	}
	return 0;		/* MISS */
    }

    /* Only real roots indicate an intersection in real space.
     *
     * Look at each root returned; if the imaginary part is zero or
     * sufficiently close, then use the real part as one value of 't'
     * for the intersections
     */
    for (j=0, i=0; j < 4; j++) {
	if (NEAR_ZERO(val[j].im, ap->a_rt_i->rti_tol.dist))
	    k[i++] = val[j].re;
    }

    /* reverse above translation by adding distance to all 'k' values.
     */
    for (j = 0; j < i; ++j)
	k[j] -= cor_proj;

    /* Here, 'i' is number of points found */
    switch (i) {
	case 0:
	    return 0;		/* No hit */

	default:
	    bu_log("rt_tor_shot: reduced 4 to %d roots\n", i);
	    bn_pr_roots(stp->st_name, val, 4);
	    return 0;		/* No hit */

	case 2:
	    {
		/* Sort most distant to least distant. */
		fastf_t u;
		if ((u=k[0]) < k[1]) {
		    /* bubble larger towards [0] */
		    k[0] = k[1];
		    k[1] = u;
		}
	    }
	    break;
	case 4:
	    {
		register short n;
		register short lim;

		/* Inline rt_pt_sort().  Sorts k[] into descending order. */
		for (lim = i-1; lim > 0; lim--) {
		    for (n = 0; n < lim; n++) {
			fastf_t u;
			if ((u=k[n]) < k[n+1]) {
			    /* bubble larger towards [0] */
			    k[n] = k[n+1];
			    k[n+1] = u;
			}
		    }
		}
	    }
	    break;
    }

    /* Now, t[0] > t[npts-1] */
    /* k[1] is entry point, and k[0] is farthest exit point */
    RT_GET_SEG(segp, ap->a_resource);
    segp->seg_stp = stp;
    segp->seg_in.hit_dist = k[1]*tor->tor_r1;
    segp->seg_out.hit_dist = k[0]*tor->tor_r1;
    segp->seg_in.hit_surfno = segp->seg_out.hit_surfno = 0;
    /* Set aside vector for rt_tor_norm() later */
    VJOIN1(segp->seg_in.hit_vpriv, pprime, k[1], dprime);
    VJOIN1(segp->seg_out.hit_vpriv, pprime, k[0], dprime);
    BU_LIST_INSERT(&(seghead->l), &(segp->l));

    if (i == 2)
	return 2;			/* HIT */

    /* 4 points */
    /* k[3] is entry point, and k[2] is exit point */
    RT_GET_SEG(segp, ap->a_resource);
    segp->seg_stp = stp;
    segp->seg_in.hit_dist = k[3]*tor->tor_r1;
    segp->seg_out.hit_dist = k[2]*tor->tor_r1;
    segp->seg_in.hit_surfno = segp->seg_out.hit_surfno = 1;
    VJOIN1(segp->seg_in.hit_vpriv, pprime, k[3], dprime);
    VJOIN1(segp->seg_out.hit_vpriv, pprime, k[2], dprime);
    BU_LIST_INSERT(&(seghead->l), &(segp->l));
    return 4;			/* HIT */
}


#define RT_TOR_SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;
/**
 * R T _ T O R _ V S H O T
 *
 * This is the Becker vector version
 */
void
rt_tor_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
    /* An array of solid pointers */
    /* An array of ray pointers */
    /* array of segs (results returned) */
    /* Number of ray/object pairs */

{
    register int i;
    register struct tor_specific *tor;
    vect_t dprime;		/* D' */
    vect_t pprime;		/* P' */
    vect_t work;		/* temporary vector */
    bn_poly_t *C;		/* The final equation */
    bn_complex_t (*val)[4];	/* The complex roots */
    int num_roots;
    int num_zero;
    bn_poly_t A, Asqr;
    bn_poly_t X2_Y2;		/* X**2 + Y**2 */
    vect_t cor_pprime;	/* new ray origin */
    fastf_t *cor_proj;

    /* Allocate space for polys and roots */
    C = (bn_poly_t *)bu_malloc(n * sizeof(bn_poly_t), "tor bn_poly_t");
    val = (bn_complex_t (*)[4])bu_malloc(n * sizeof(bn_complex_t) * 4,
					 "tor bn_complex_t");
    cor_proj = (fastf_t *)bu_malloc(n * sizeof(fastf_t), "tor proj");

    /* Initialize seg_stp to assume hit (zero will then flag miss) */
    for (i = 0; i < n; i++) segp[i].seg_stp = stp[i];

    /* for each ray/torus pair */
    for (i = 0; i < n; i++) {
	if (segp[i].seg_stp == 0) continue;	/* Skip */
	tor = (struct tor_specific *)stp[i]->st_specific;

	/* Convert vector into the space of the unit torus */
	MAT4X3VEC(dprime, tor->tor_SoR, rp[i]->r_dir);
	VUNITIZE(dprime);

	/* Use segp[i].seg_in.hit_normal as tmp to hold dprime */
	VMOVE(segp[i].seg_in.hit_normal, dprime);

	VSUB2(work, rp[i]->r_pt, tor->tor_V);
	MAT4X3VEC(pprime, tor->tor_SoR, work);

	/* Use segp[i].seg_out.hit_normal as tmp to hold pprime */
	VMOVE(segp[i].seg_out.hit_normal, pprime);

	/* normalize distance from torus.  substitute corrected pprime
	 * which contains a translation along ray direction to closest
	 * approach to vertex of torus.  Translating ray origin along
	 * direction of ray to closest pt. to origin of solid's
	 * coordinate system, new ray origin is 'cor_pprime'.
	 */
	cor_proj[i] = VDOT(pprime, dprime);
	VSCALE(cor_pprime, dprime, cor_proj[i]);
	VSUB2(cor_pprime, pprime, cor_pprime);

	/*
	 * Given a line and a ratio, alpha, finds the equation of the
	 * unit torus in terms of the variable 't'.
	 *
	 * The equation for the torus is:
	 *
	 * [X**2 + Y**2 + Z**2 + (1 - alpha**2)]**2 - 4*(X**2 + Y**2) =0
	 *
	 * First, find X, Y, and Z in terms of 't' for this line, then
	 * substitute them into the equation above.
	 *
	 * Wx = Dx*t + Px
	 *
	 * Wx**2 = Dx**2 * t**2  +  2 * Dx * Px  +  Px**2
	 *		[0]             [1]           [2]    dgr=2
	 */
	X2_Y2.dgr = 2;
	X2_Y2.cf[0] = dprime[X] * dprime[X] + dprime[Y] * dprime[Y];
	X2_Y2.cf[1] = 2.0 * (dprime[X] * cor_pprime[X] +
			     dprime[Y] * cor_pprime[Y]);
	X2_Y2.cf[2] = cor_pprime[X] * cor_pprime[X] +
	    cor_pprime[Y] * cor_pprime[Y];

	/* A = X2_Y2 + Z2 */
	A.dgr = 2;
	A.cf[0] = X2_Y2.cf[0] + dprime[Z] * dprime[Z];
	A.cf[1] = X2_Y2.cf[1] + 2.0 * dprime[Z] * cor_pprime[Z];
	A.cf[2] = X2_Y2.cf[2] + cor_pprime[Z] * cor_pprime[Z] +
	    1.0 - tor->tor_alpha * tor->tor_alpha;

	/* Inline expansion of (void) bn_poly_mul(&Asqr, &A, &A) */
	/* Both polys have degree two */
	Asqr.dgr = 4;
	Asqr.cf[0] = A.cf[0] * A.cf[0];
	Asqr.cf[1] = A.cf[0] * A.cf[1] +
	    A.cf[1] * A.cf[0];
	Asqr.cf[2] = A.cf[0] * A.cf[2] +
	    A.cf[1] * A.cf[1] +
	    A.cf[2] * A.cf[0];
	Asqr.cf[3] = A.cf[1] * A.cf[2] +
	    A.cf[2] * A.cf[1];
	Asqr.cf[4] = A.cf[2] * A.cf[2];

	/* Inline expansion of (void) bn_poly_scale(&X2_Y2, 4.0) */
	X2_Y2.cf[0] *= 4.0;
	X2_Y2.cf[1] *= 4.0;
	X2_Y2.cf[2] *= 4.0;

	/* Inline expansion of (void) bn_poly_sub(&C, &Asqr, &X2_Y2) */
	/* offset is know to be 2 */
	C[i].dgr	= 4;
	C[i].cf[0] = Asqr.cf[0];
	C[i].cf[1] = Asqr.cf[1];
	C[i].cf[2] = Asqr.cf[2] - X2_Y2.cf[0];
	C[i].cf[3] = Asqr.cf[3] - X2_Y2.cf[1];
	C[i].cf[4] = Asqr.cf[4] - X2_Y2.cf[2];
    }

    /* Unfortunately finding the 4th order roots are too ugly to
     * inline.
     */
    for (i = 0; i < n; i++) {
	if (segp[i].seg_stp == 0) continue;	/* Skip */

	/* It is known that the equation is 4th order.  Therefore, if
	 * the root finder returns other than 4 roots, error.
	 */
	if ((num_roots = rt_poly_roots(&(C[i]), &(val[i][0]), (*stp)->st_dp->d_namep)) != 4) {
	    if (num_roots > 0) {
		bu_log("tor:  rt_poly_roots() 4!=%d\n", num_roots);
		bn_pr_roots("tor", val[i], num_roots);
	    } else if (num_roots < 0) {
		static int reported=0;
		bu_log("The root solver failed to converge on a solution for %s\n", stp[i]->st_dp->d_namep);
		if (!reported) {
		    VPRINT("while shooting from:\t", rp[i]->r_pt);
		    VPRINT("while shooting at:\t", rp[i]->r_dir);
		    bu_log("Additional torus convergence failure details will be suppressed.\n");
		    reported=1;
		}
	    }
	    RT_TOR_SEG_MISS(segp[i]);		/* MISS */
	}
    }

    /* for each ray/torus pair */
    for (i = 0; i < n; i++) {
	if (segp[i].seg_stp == 0) continue; /* Skip */

	/* Only real roots indicate an intersection in real space.
	 *
	 * Look at each root returned; if the imaginary part is zero
	 * or sufficiently close, then use the real part as one value
	 * of 't' for the intersections
	 */
	/* Also reverse translation by adding distance to all 'k' values. */
	/* Reuse C to hold k values */
	num_zero = 0;
	if (NEAR_ZERO(val[i][0].im, ap->a_rt_i->rti_tol.dist))
	    C[i].cf[num_zero++] = val[i][0].re - cor_proj[i];
	if (NEAR_ZERO(val[i][1].im, ap->a_rt_i->rti_tol.dist)) {
	    C[i].cf[num_zero++] = val[i][1].re - cor_proj[i];
	}
	if (NEAR_ZERO(val[i][2].im, ap->a_rt_i->rti_tol.dist)) {
	    C[i].cf[num_zero++] = val[i][2].re - cor_proj[i];
	}
	if (NEAR_ZERO(val[i][3].im, ap->a_rt_i->rti_tol.dist)) {
	    C[i].cf[num_zero++] = val[i][3].re - cor_proj[i];
	}
	C[i].dgr   = num_zero;

	/* Here, 'i' is number of points found */
	if (num_zero == 0) {
	    RT_TOR_SEG_MISS(segp[i]);		/* MISS */
	} else if (num_zero != 2 && num_zero != 4) {
	    RT_TOR_SEG_MISS(segp[i]);		/* MISS */
	}
    }

    /* Process each one segment hit */
    for (i = 0; i < n; i++) {
	if (segp[i].seg_stp == 0) continue; /* Skip */
	if (C[i].dgr != 2) continue;  /* Not one segment */

	tor = (struct tor_specific *)stp[i]->st_specific;

	/* segp[i].seg_in.hit_normal holds dprime */
	/* segp[i].seg_out.hit_normal holds pprime */
	if (C[i].cf[1] < C[i].cf[0]) {
	    /* C[i].cf[1] is entry point */
	    segp[i].seg_in.hit_dist = C[i].cf[1]*tor->tor_r1;
	    segp[i].seg_out.hit_dist = C[i].cf[0]*tor->tor_r1;
	    /* Set aside vector for rt_tor_norm() later */
	    VJOIN1(segp[i].seg_in.hit_vpriv,
		   segp[i].seg_out.hit_normal,
		   C[i].cf[1], segp[i].seg_in.hit_normal);
	    VJOIN1(segp[i].seg_out.hit_vpriv,
		   segp[i].seg_out.hit_normal,
		   C[i].cf[0], segp[i].seg_in.hit_normal);
	} else {
	    /* C[i].cf[0] is entry point */
	    segp[i].seg_in.hit_dist = C[i].cf[0]*tor->tor_r1;
	    segp[i].seg_out.hit_dist = C[i].cf[1]*tor->tor_r1;
	    /* Set aside vector for rt_tor_norm() later */
	    VJOIN1(segp[i].seg_in.hit_vpriv,
		   segp[i].seg_out.hit_normal,
		   C[i].cf[0], segp[i].seg_in.hit_normal);
	    VJOIN1(segp[i].seg_out.hit_vpriv,
		   segp[i].seg_out.hit_normal,
		   C[i].cf[1], segp[i].seg_in.hit_normal);
	}
    }

    /* Process each two segment hit */
    for (i = 0; i < n; i++) {

	if (segp[i].seg_stp == 0) continue;	/* Skip */
	if (C[i].dgr != 4) continue;  /* Not two segment */

	tor = (struct tor_specific *)stp[i]->st_specific;

	/* Sort most distant to least distant. */
	rt_pt_sort(C[i].cf, 4);
	/* Now, t[0] > t[npts-1] */

	/* segp[i].seg_in.hit_normal holds dprime */
	VMOVE(dprime, segp[i].seg_in.hit_normal);
	/* segp[i].seg_out.hit_normal holds pprime */
	VMOVE(pprime, segp[i].seg_out.hit_normal);

	/* C[i].cf[1] is entry point */
	segp[i].seg_in.hit_dist =  C[i].cf[1]*tor->tor_r1;
	segp[i].seg_out.hit_dist = C[i].cf[0]*tor->tor_r1;
	/* Set aside vector for rt_tor_norm() later */
	VJOIN1(segp[i].seg_in.hit_vpriv, pprime, C[i].cf[1], dprime);
	VJOIN1(segp[i].seg_out.hit_vpriv, pprime, C[i].cf[0], dprime);
    }

    /* Free tmp space used */
    bu_free((char *)C, "tor C");
    bu_free((char *)val, "tor val");
    bu_free((char *)cor_proj, "tor cor_proj");
}


/**
 * R T _ T O R _ N O R M
 *
 * Compute the normal to the torus, given a point on the UNIT TORUS
 * centered at the origin on the X-Y plane.  The gradient of the torus
 * at that point is in fact the normal vector, which will have to be
 * given unit length.  To make this useful for the original torus, it
 * will have to be rotated back to the orientation of the original
 * torus.
 *
 * Given that the equation for the unit torus is:
 *
 *	[ X**2 + Y**2 + Z**2 + (1 - alpha**2) ]**2 - 4*(X**2 + Y**2)  =  0
 *
 * let w = X**2 + Y**2 + Z**2 + (1 - alpha**2), then the equation becomes:
 *
 *	w**2 - 4*(X**2 + Y**2)  =  0
 *
 * For f(x, y, z) = 0, the gradient of f() is (df/dx, df/dy, df/dz).
 *
 *	df/dx = 2 * w * 2 * x - 8 * x	= (4 * w - 8) * x
 *	df/dy = 2 * w * 2 * y - 8 * y	= (4 * w - 8) * y
 *	df/dz = 2 * w * 2 * z		= 4 * w * z
 *
 * Since we rescale the gradient (normal) to unity, we divide the
 * above equations by four here.
 */
void
rt_tor_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
    register struct tor_specific *tor =
	(struct tor_specific *)stp->st_specific;

    fastf_t w;
    vect_t work;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    w = hitp->hit_vpriv[X]*hitp->hit_vpriv[X] +
	hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y] +
	hitp->hit_vpriv[Z]*hitp->hit_vpriv[Z] +
	1.0 - tor->tor_alpha*tor->tor_alpha;
    VSET(work,
	 (w - 2.0) * hitp->hit_vpriv[X],
	 (w - 2.0) * hitp->hit_vpriv[Y],
	 w * hitp->hit_vpriv[Z]);
    VUNITIZE(work);

    MAT3X3VEC(hitp->hit_normal, tor->tor_invR, work);
}


/**
 * R T _ T O R _ C U R V E
 *
 * Return the curvature of the torus.
 */
void
rt_tor_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
    register struct tor_specific *tor =
	(struct tor_specific *)stp->st_specific;
    vect_t w4, w5;
    fastf_t nx, ny, nz, x_1, y_1, z_1;
    fastf_t d;

    nx = tor->tor_N[X];
    ny = tor->tor_N[Y];
    nz = tor->tor_N[Z];

    /* vector from V to hit point */
    VSUB2(w4, hitp->hit_point, tor->tor_V);

    if (!NEAR_ZERO(nz, 0.00001)) {
	z_1 = w4[Z]*nx*nx + w4[Z]*ny*ny - w4[X]*nx*nz - w4[Y]*ny*nz;
	x_1 = (nx*(z_1-w4[Z])/nz) + w4[X];
	y_1 = (ny*(z_1-w4[Z])/nz) + w4[Y];
    } else if (!NEAR_ZERO(ny, 0.00001)) {
	y_1 = w4[Y]*nx*nx + w4[Y]*nz*nz - w4[X]*nx*ny - w4[Z]*ny*nz;
	x_1 = (nx*(y_1-w4[Y])/ny) + w4[X];
	z_1 = (nz*(y_1-w4[Y])/ny) + w4[Z];
    } else {
	x_1 = w4[X]*ny*ny + w4[X]*nz*nz - w4[Y]*nx*ny - w4[Z]*nz*nx;
	y_1 = (ny*(x_1-w4[X])/nx) + w4[Y];
	z_1 = (nz*(x_1-w4[X])/nx) + w4[Z];
    }
    d = sqrt(x_1*x_1 + y_1*y_1 + z_1*z_1);

    cvp->crv_c1 = (tor->tor_r1 - d) / (d * tor->tor_r2);
    cvp->crv_c2 = -1.0 / tor->tor_r2;

    w4[X] = x_1 / d;
    w4[Y] = y_1 / d;
    w4[Z] = z_1 / d;
    VCROSS(w5, tor->tor_N, w4);
    VCROSS(cvp->crv_pdir, w5, hitp->hit_normal);
    VUNITIZE(cvp->crv_pdir);
}


/**
 * R T _ T O R _ U V
 */
void
rt_tor_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    struct tor_specific *tor = (struct tor_specific *) stp->st_specific;
    vect_t work;
    vect_t pprime;
    vect_t pprime2;
    fastf_t costheta;

    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);
    if (!hitp || !uvp)
	return;
    RT_CK_HIT(hitp);

    VSUB2(work, hitp->hit_point, tor->tor_V);
    MAT4X3VEC(pprime, tor->tor_SoR, work);
    /*
     * -pi/2 <= atan2(x, y) <= pi/2
     */
    uvp->uv_u = atan2(pprime[Y], pprime[X]) * bn_inv2pi + 0.5;

    VSET(work, pprime[X], pprime[Y], 0.0);
    VUNITIZE(work);
    VSUB2(pprime2, pprime, work);
    VUNITIZE(pprime2);
    costheta = VDOT(pprime2, work);
    uvp->uv_v = atan2(pprime2[Z], costheta) * bn_inv2pi + 0.5;
}


/**
 * R T _ T O R _ F R E E
 */
void
rt_tor_free(struct soltab *stp)
{
    register struct tor_specific *tor =
	(struct tor_specific *)stp->st_specific;

    bu_free((char *)tor, "tor_specific");
}


int
rt_tor_class(void)
{
    return 0;
}


/**
 * R T _ N U M _ C I R C U L A R _ S E G M E N T S
 *
 * Given a circle with a specified radius, determine the minimum
 * number of straight line segments that the circle can be
 * approximated with, while still meeting the given maximum
 * permissible error distance.  Form a chord (straight line) by
 * connecting the start and end points found when sweeping a 'radius'
 * arc through angle 'theta'.
 *
 * The error distance is the distance between where a radius line at
 * angle theta/2 hits the chord, and where it hits the circle (at
 * 'radius' distance).
 *
 * error_distance = radius * (1 - cos(theta/2))
 *
 * or
 *
 * theta = 2 * acos(1 - error_distance / radius)
 *
 * Returns -
 * number of segments.  Always at least 6.
 */
int
rt_num_circular_segments(double maxerr, double radius)
{
    register fastf_t cos_half_theta;
    register fastf_t half_theta;
    int n;

    if (radius <= SMALL_FASTF || maxerr <= SMALL_FASTF || maxerr >= radius) {
	/* Return a default number of segments */
	return 6;
    }
    cos_half_theta = 1.0 - maxerr / radius;

    /* There does not seem to be any reasonable way to express the
     * acos in terms of an atan2(), so extra checking is done.  only
     * proceed if the cosine is between 0 and 1.
     */
    if (cos_half_theta <= SMALL_FASTF || cos_half_theta-1.0 >= -SMALL_FASTF) {
	/* Return a default number of segments */
	return 6;
    }

    half_theta = acos(cos_half_theta);

    if (half_theta <= SMALL_FASTF) {
	/* A very large number of segments will be needed.  Impose an
	 * upper bound here
	 */
	return 360*10;
    }
    n = (bn_pi / half_theta) + 0.99;

    /* Impose the limits again */
    if (n <= 6) return 6;
    if (n >= 360*10) return 360*10;
    return n;
}


/**
 * R T _ T O R _ P L O T
 *
 * The TORUS has the following input fields:
 * ti.v V from origin to center
 * ti.h Radius Vector, Normal to plane of torus
 * ti.a, ti.b perpindicular, to CENTER of torus (for top, bottom)
 */
int
rt_tor_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
{
    fastf_t alpha;
    fastf_t beta;
    fastf_t cos_alpha, sin_alpha;
    fastf_t cos_beta, sin_beta;
    fastf_t dist_to_rim;
    struct rt_tor_internal *tip;
    int w;
    int nw = 8;
    int len;
    int nlen = 16;
    fastf_t *pts;
    vect_t G;
    vect_t radius;
    vect_t edge;
    fastf_t rel;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    tip = (struct rt_tor_internal *)ip->idb_ptr;
    RT_TOR_CK_MAGIC(tip);

    if (ttol->rel <= 0.0 || ttol->rel >= 1.0) {
	rel = 0.0;		/* none */
    } else {
	/* Convert relative tolerance to absolute tolerance
	 * by scaling w.r.t. the torus diameter.
	 */
	rel = ttol->rel * 2 * (tip->r_a+tip->r_h);
    }
    /* Take tighter of two (absolute) tolerances */
    if (ttol->abs <= 0.0) {
	/* No absolute tolerance given */
	if (rel <= 0.0) {
	    /* User has no tolerance for this kind of drink */
	    nw = 8;
	    nlen = 16;
	} else {
	    /* Use the absolute-ized relative tolerance */
	    nlen = rt_num_circular_segments(rel, tip->r_a);
	    nw = rt_num_circular_segments(rel, tip->r_h);
	}
    } else {
	/* Absolute tolerance was given */
	if (rel <= 0.0 || rel > ttol->abs)
	    rel = ttol->abs;
	nlen = rt_num_circular_segments(rel, tip->r_a);
	nw = rt_num_circular_segments(rel, tip->r_h);
    }

    /* Implement surface-normal tolerance, if given:
     *
     * nseg = (2 * pi) / (2 * tol)
     *
     * For a facet which subtends angle theta, surface normal is exact
     * in the center, and off by theta/2 at the edges.  Note: 1 degree
     * tolerance requires 180*180 tessellation!
     */
    if (ttol->norm > 0.0) {
	register int nseg;
	nseg = (bn_pi / ttol->norm) + 0.99;
	if (nseg > nlen) nlen = nseg;
	if (nseg > nw) nw = nseg;
    }

    /* Compute the points on the surface of the torus */
    dist_to_rim = tip->r_h/tip->r_a;
    pts = (fastf_t *)bu_malloc(nw * nlen * sizeof(point_t),
			       "rt_tor_plot pts[]");

#define TOR_PT(www, lll)	((((www)%nw)*nlen)+((lll)%nlen))
#define TOR_PTA(ww, ll)	(&pts[TOR_PT(ww, ll)*3])
#define TOR_NORM_A(ww, ll)	(&norms[TOR_PT(ww, ll)*3])

    for (len = 0; len < nlen; len++) {
	beta = bn_twopi * len / nlen;
	cos_beta = cos(beta);
	sin_beta = sin(beta);
	/* G always points out to rim, along radius vector */
	VCOMB2(radius, cos_beta, tip->a, sin_beta, tip->b);
	/* We assume that |radius| = |A|.  Circular */
	VSCALE(G, radius, dist_to_rim);
	for (w = 0; w < nw; w++) {
	    alpha = bn_twopi * w / nw;
	    cos_alpha = cos(alpha);
	    sin_alpha = sin(alpha);
	    VCOMB2(edge, cos_alpha, G, sin_alpha*tip->r_h, tip->h);
	    VADD3(TOR_PTA(w, len), tip->v, edge, radius);
	}
    }

    /* Draw lengthwise (around outside rim) */
    for (w = 0; w < nw; w++) {
	len = nlen-1;
	RT_ADD_VLIST(vhead, TOR_PTA(w, len), BN_VLIST_LINE_MOVE);
	for (len = 0; len < nlen; len++) {
	    RT_ADD_VLIST(vhead, TOR_PTA(w, len), BN_VLIST_LINE_DRAW);
	}
    }
    /* Draw around the "width" (1 cross section) */
    for (len = 0; len < nlen; len++) {
	w = nw-1;
	RT_ADD_VLIST(vhead, TOR_PTA(w, len), BN_VLIST_LINE_MOVE);
	for (w = 0; w < nw; w++) {
	    RT_ADD_VLIST(vhead, TOR_PTA(w, len), BN_VLIST_LINE_DRAW);
	}
    }

    bu_free((char *)pts, "rt_tor_plot pts[]");
    return 0;
}


/**
 * R T _ T O R _ T E S S
 */
int
rt_tor_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    fastf_t alpha;
    fastf_t beta;
    fastf_t cos_alpha, sin_alpha;
    fastf_t cos_beta, sin_beta;
    fastf_t dist_to_rim;
    struct rt_tor_internal *tip;
    int w;
    int nw = 6;
    int len;
    int nlen = 6;
    fastf_t *pts;
    vect_t G;
    vect_t radius;
    vect_t edge;
    struct shell *s;
    struct vertex **verts;
    struct faceuse **faces;
    fastf_t *norms;
    struct vertex **vertp[4];
    int nfaces;
    int i;
    fastf_t rel;

    RT_CK_DB_INTERNAL(ip);
    tip = (struct rt_tor_internal *)ip->idb_ptr;
    RT_TOR_CK_MAGIC(tip);

    if (ttol->rel <= 0.0 || ttol->rel >= 1.0) {
	rel = 0.0;		/* none */
    } else {
	/* Convert relative tolerance to absolute tolerance by scaling
	 * w.r.t. the torus diameter.
	 */
	rel = ttol->rel * 2 * (tip->r_a+tip->r_h);
    }
    /* Take tighter of two (absolute) tolerances */
    if (ttol->abs <= 0.0) {
	/* No absolute tolerance given */
	if (rel <= 0.0) {
	    /* User has no tolerance for this kind of drink */
	    nw = 8;
	    nlen = 16;
	} else {
	    /* Use the absolute-ized relative tolerance */
	    nlen = rt_num_circular_segments(rel, tip->r_a);
	    nw = rt_num_circular_segments(rel, tip->r_h);
	}
    } else {
	/* Absolute tolerance was given */
	if (rel <= 0.0 || rel > ttol->abs)
	    rel = ttol->abs;
	nlen = rt_num_circular_segments(rel, tip->r_a);
	nw = rt_num_circular_segments(rel, tip->r_h);
    }

    /* Implement surface-normal tolerance, if given
     *
     * nseg = (2 * pi) / (2 * tol)
     *
     * For a facet which subtends angle theta, surface normal is exact
     * in the center, and off by theta/2 at the edges.  Note: 1 degree
     * tolerance requires 180*180 tessellation!
     */
    if (ttol->norm > 0.0) {
	register int nseg;
	nseg = (bn_pi / ttol->norm) + 0.99;
	if (nseg > nlen) nlen = nseg;
	if (nseg > nw) nw = nseg;
    }

    /* Compute the points on the surface of the torus */
    dist_to_rim = tip->r_h/tip->r_a;
    pts = (fastf_t *)bu_malloc(nw * nlen * sizeof(point_t),
			       "rt_tor_tess pts[]");
    norms = (fastf_t *)bu_malloc(nw * nlen * sizeof(vect_t), "rt_tor_tess: norms[]");

    for (len = 0; len < nlen; len++) {
	beta = bn_twopi * len / nlen;
	cos_beta = cos(beta);
	sin_beta = sin(beta);
	/* G always points out to rim, along radius vector */
	VCOMB2(radius, cos_beta, tip->a, sin_beta, tip->b);
	/* We assume that |radius| = |A|.  Circular */
	VSCALE(G, radius, dist_to_rim);
	for (w = 0; w < nw; w++) {
	    alpha = bn_twopi * w / nw;
	    cos_alpha = cos(alpha);
	    sin_alpha = sin(alpha);
	    VCOMB2(edge, cos_alpha, G, sin_alpha*tip->r_h, tip->h);
	    VADD3(TOR_PTA(w, len), tip->v, edge, radius);

	    VMOVE(TOR_NORM_A(w, len), edge);
	    VUNITIZE(TOR_NORM_A(w, len));
	}
    }

    *r = nmg_mrsv(m);	/* Make region, empty shell, vertex */
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    verts = (struct vertex **)bu_calloc(nw*nlen, sizeof(struct vertex *),
					"rt_tor_tess *verts[]");
    faces = (struct faceuse **)bu_calloc(nw*nlen, sizeof(struct faceuse *),
					 "rt_tor_tess *faces[]");

    /* Build the topology of the torus */
    /* Note that increasing 'w' goes to the left (alpha goes ccw) */
    nfaces = 0;
    for (w = 0; w < nw; w++) {
	for (len = 0; len < nlen; len++) {
	    vertp[0] = &verts[ TOR_PT(w+0, len+0) ];
	    vertp[1] = &verts[ TOR_PT(w+0, len+1) ];
	    vertp[2] = &verts[ TOR_PT(w+1, len+1) ];
	    vertp[3] = &verts[ TOR_PT(w+1, len+0) ];
	    if ((faces[nfaces++] = nmg_cmface(s, vertp, 4)) == (struct faceuse *)0) {
		bu_log("rt_tor_tess() nmg_cmface failed, w=%d/%d, len=%d/%d\n",
		       w, nw, len, nlen);
		nfaces--;
	    }
	}
    }

    /* Associate vertex geometry */
    for (w = 0; w < nw; w++) {
	for (len = 0; len < nlen; len++) {
	    nmg_vertex_gv(verts[TOR_PT(w, len)], TOR_PTA(w, len));
	}
    }

    /* Associate face geometry */
    for (i=0; i < nfaces; i++) {
	if (nmg_fu_planeeqn(faces[i], tol) < 0)
	    return -1;		/* FAIL */
    }

    /* Associate vertexuse normals */
    for (w=0; w<nw; w++) {
	for (len=0; len<nlen; len++) {
	    struct vertexuse *vu;
	    vect_t rev_norm;

	    VREVERSE(rev_norm, TOR_NORM_A(w, len));

	    for (BU_LIST_FOR(vu, vertexuse, &verts[TOR_PT(w, len)]->vu_hd)) {
		struct faceuse *fu;

		NMG_CK_VERTEXUSE(vu);

		fu = nmg_find_fu_of_vu(vu);
		NMG_CK_FACEUSE(fu);

		if (fu->orientation == OT_SAME)
		    nmg_vertexuse_nv(vu, TOR_NORM_A(w, len));
		else if (fu->orientation == OT_OPPOSITE)
		    nmg_vertexuse_nv(vu, rev_norm);
	    }
	}
    }

    /* kill zero length edgeuse */
    (void)nmg_keu_zl(s, tol);

    /* Compute "geometry" for region and shell */
    nmg_region_a(*r, tol);

    bu_free((char *)pts, "rt_tor_tess pts[]");
    bu_free((char *)verts, "rt_tor_tess *verts[]");
    bu_free((char *)faces, "rt_tor_tess *faces[]");
    bu_free((char *)norms, "rt_tor_tess norms[]");
    return 0;
}


/**
 * R T _ T O R _ I M P O R T
 *
 * Import a torus from the database format to the internal format.
 * Apply modeling transformations at the same time.
 */
int
rt_tor_import4(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_tor_internal *tip;
    union record *rp;
    fastf_t vec[3*4];
    vect_t axb;
    register fastf_t f;

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != ID_SOLID) {
	bu_log("rt_tor_import4: defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_TOR;
    ip->idb_meth = &rt_functab[ID_TOR];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_tor_internal), "rt_tor_internal");
    tip = (struct rt_tor_internal *)ip->idb_ptr;
    tip->magic = RT_TOR_INTERNAL_MAGIC;

    /* Convert from database to internal format */
    flip_fastf_float(vec, rp->s.s_values, 4, dbip->dbi_version < 0 ? 1 : 0);

    /* Apply modeling transformations */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(tip->v, mat, &vec[0*3]);
    MAT4X3VEC(tip->h, mat, &vec[1*3]);
    MAT4X3VEC(tip->a, mat, &vec[2*3]);
    MAT4X3VEC(tip->b, mat, &vec[3*3]);

    /* Make the vectors unit length */
    tip->r_a = MAGNITUDE(tip->a);
    tip->r_b = MAGNITUDE(tip->b);
    tip->r_h = MAGNITUDE(tip->h);
    if (tip->r_a <= SMALL_FASTF || tip->r_b <= SMALL_FASTF || tip->r_h <= SMALL_FASTF) {
	bu_log("rt_tor_import4:  zero length A, B, or H vector\n");
	return -1;
    }
    /* In memory, the H vector is unit length */
    f = 1.0/tip->r_h;
    VSCALE(tip->h, tip->h, f);

    /* If H does not point in the direction of A cross B, reverse H. */
    /* Somehow, database records have been written with this problem. */
    VCROSS(axb, tip->a, tip->b);
    if (VDOT(axb, tip->h) < 0) {
	VREVERSE(tip->h, tip->h);
    }

    return 0;		/* OK */
}


/**
 * R T _ T O R _ E X P O R T 5
 */
int
rt_tor_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    double vec[2*3+2];
    struct rt_tor_internal *tip;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_TOR) return -1;
    tip = (struct rt_tor_internal *)ip->idb_ptr;
    RT_TOR_CK_MAGIC(tip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * (2*3+2);
    ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes, "tor external");

    /* scale values into local buffer */
    VSCALE(&vec[0*3], tip->v, local2mm);
    VMOVE(&vec[1*3], tip->h);		/* UNIT vector, not scaled */
    vec[2*3+0] = tip->r_a*local2mm;		/* r1 */
    vec[2*3+1] = tip->r_h*local2mm;		/* r2 */

    /* convert from internal (host) to database (network) format */
    htond(ep->ext_buf, (unsigned char *)vec, 2*3+2);

    return 0;
}


/**
 * R T _ T O R _ E X P O R T
 *
 * The name will be added by the caller.
 */
int
rt_tor_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_tor_internal *tip;
    union record *rec;
    vect_t norm;
    vect_t cross1, cross2;
    fastf_t r1, r2;
    fastf_t r3, r4;
    double m2;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_TOR) return -1;
    tip = (struct rt_tor_internal *)ip->idb_ptr;
    RT_TOR_CK_MAGIC(tip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record);
    ep->ext_buf = (genptr_t)bu_calloc(1, ep->ext_nbytes, "tor external");
    rec = (union record *)ep->ext_buf;

    rec->s.s_id = ID_SOLID;
    rec->s.s_type = TOR;

    r1 = tip->r_a;
    r2 = tip->r_h;

    /* Validate that 0 < r2 <= r1 */
    if (r2 <= 0.0) {
	bu_log("rt_tor_export4:  illegal r2=%.12e <= 0\n", r2);
	return -1;
    }
    if (r2 > r1) {
	bu_log("rt_tor_export4:  illegal r2=%.12e > r1=%.12e\n",
	       r2, r1);
	return -1;
    }

    r1 *= local2mm;
    r2 *= local2mm;
    VSCALE(&rec->s.s_values[0*3], tip->v, local2mm);

    VMOVE(norm, tip->h);
    m2 = MAGNITUDE(norm);		/* F2 is NORMAL to torus */
    if (m2 <= SQRT_SMALL_FASTF) {
	bu_log("rt_tor_export4: normal magnitude is zero!\n");
	return -1;		/* failure */
    }
    m2 = 1.0/m2;
    VSCALE(norm, norm, m2);	/* Give normal unit length */
    VSCALE(&rec->s.s_values[1*3], norm, r2); /* F2: normal radius len */

    /* Create two mutually perpendicular vectors, perpendicular to Norm */
    /* Ensure that AxB points in direction of N */
    bn_vec_ortho(cross1, norm);
    VCROSS(cross2, norm, cross1);
    VUNITIZE(cross2);

    /* F3, F4 are perpendicular, goto center of solid part */
    VSCALE(&rec->s.s_values[2*3], cross1, r1);
    VSCALE(&rec->s.s_values[3*3], cross2, r1);

    /*
     * The rest of these provide no real extra information,
     * and exist for compatability with old versions of MGED.
     */
    r3=r1-r2;	/* Radius to inner circular edge */
    r4=r1+r2;	/* Radius to outer circular edge */

    /* F5, F6 are perpendicular, goto inner edge of ellipse */
    VSCALE(&rec->s.s_values[4*3], cross1, r3);
    VSCALE(&rec->s.s_values[5*3], cross2, r3);

    /* F7, F8 are perpendicular, goto outer edge of ellipse */
    VSCALE(&rec->s.s_values[6*3], cross1, r4);
    VSCALE(&rec->s.s_values[7*3], cross2, r4);

    return 0;
}


/**
 * R T _ T O R _ I M P O R T 5
 *
 * Taken from the database record:
 * v vertex (point) of center of torus.
 * h unit vector in the normal direction of the torus
 * major radius of ring from 'v' to center of ring
 * minor radius of the ring
 *
 * Calculate:
 * 2nd radius of ring (==1st radius)
 * ring unit vector 1
 * ring unit vector 2
 */
int
rt_tor_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_tor_internal *tip;
    struct rec {
	double v[3];
	double h[3];
	double ra;	/* r1 */
	double rh;	/* r2 */
    } rec;

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    BU_ASSERT_LONG(ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * (2*3+2));

    RT_CK_DB_INTERNAL(ip);

    if (mat == NULL) mat = bn_mat_identity;
    if (bn_mat_is_non_unif (mat)) {
	bu_log("------------------ WARNING ----------------\nNon-uniform matrix transform on torus.  Ignored\n");
    }

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_TOR;
    ip->idb_meth = &rt_functab[ID_TOR];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_tor_internal), "rt_tor_internal");
    tip = (struct rt_tor_internal *)ip->idb_ptr;

    tip->magic = RT_TOR_INTERNAL_MAGIC;

    ntohd((unsigned char *)&rec, ep->ext_buf, 2*3+2);

    /* Apply modeling transformations */
    MAT4X3PNT(tip->v, mat, rec.v);
    MAT4X3VEC(tip->h, mat, rec.h);
    VUNITIZE(tip->h);			/* just to be sure */

    tip->r_a = rec.ra / mat[15];
    tip->r_h = rec.rh / mat[15];

    /* Prepare the extra information */
    tip->r_b = tip->r_a;

    /* Calculate two mutually perpendicular vectors, perpendicular to N */
    bn_vec_ortho(tip->a, tip->h);		/* a has unit length */
    VCROSS(tip->b, tip->h, tip->a);	/* |A| = |H| = 1, so |B|=1 */

    VSCALE(tip->a, tip->a, tip->r_a);
    VSCALE(tip->b, tip->b, tip->r_b);
    return 0;
}


/**
 * R T _ T O R _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_tor_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    register struct rt_tor_internal *tip =
	(struct rt_tor_internal *)ip->idb_ptr;
    double r3, r4;

    RT_TOR_CK_MAGIC(tip);
    bu_vls_strcat(str, "torus (TOR)\n");

    bu_vls_printf(str, "\tV (%g, %g, %g), r1=%g (A), r2=%g (H)\n",
		  INTCLAMP(tip->v[X] * mm2local),
		  INTCLAMP(tip->v[Y] * mm2local),
		  INTCLAMP(tip->v[Z] * mm2local),
		  INTCLAMP(tip->r_a * mm2local),
		  INTCLAMP(tip->r_h * mm2local));

    bu_vls_printf(str, "\tN=(%g, %g, %g)\n",
		  INTCLAMP(tip->h[X] * mm2local),
		  INTCLAMP(tip->h[Y] * mm2local),
		  INTCLAMP(tip->h[Z] * mm2local));

    if (!verbose) return 0;

    bu_vls_printf(str, "\tA=(%g, %g, %g)\n",
		  INTCLAMP(tip->a[X] * mm2local / tip->r_a),
		  INTCLAMP(tip->a[Y] * mm2local / tip->r_a),
		  INTCLAMP(tip->a[Z] * mm2local / tip->r_a));

    bu_vls_printf(str, "\tB=(%g, %g, %g)\n",
		  INTCLAMP(tip->b[X] * mm2local / tip->r_b),
		  INTCLAMP(tip->b[Y] * mm2local / tip->r_b),
		  INTCLAMP(tip->b[Z] * mm2local / tip->r_b));

    r3 = tip->r_a - tip->r_h;
    bu_vls_printf(str, "\tvector to inner edge = (%g, %g, %g)\n",
		  INTCLAMP(tip->a[X] * mm2local / tip->r_a * r3),
		  INTCLAMP(tip->a[Y] * mm2local / tip->r_a * r3),
		  INTCLAMP(tip->a[Z] * mm2local / tip->r_a * r3));

    r4 = tip->r_a + tip->r_h;
    bu_vls_printf(str, "\tvector to outer edge = (%g, %g, %g)\n",
		  INTCLAMP(tip->a[X] * mm2local / tip->r_a * r4),
		  INTCLAMP(tip->a[Y] * mm2local / tip->r_a * r4),
		  INTCLAMP(tip->a[Z] * mm2local / tip->r_a * r4));

    return 0;
}


/**
 * R T _ T O R _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_tor_ifree(struct rt_db_internal *ip)
{
    register struct rt_tor_internal *tip;

    RT_CK_DB_INTERNAL(ip);

    tip = (struct rt_tor_internal *)ip->idb_ptr;
    RT_TOR_CK_MAGIC(tip);

    bu_free((char *)tip, "rt_tor_internal");
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}


/**
 * R T _ T O R _ P A R A M S
 *
 */
int
rt_tor_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
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
