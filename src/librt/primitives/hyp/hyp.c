/*                           H Y P . C
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
/** @file primitives/hyp/hyp.c
 *
 * Intersect a ray with an elliptical hyperboloid of one sheet.
 *
 * [ (x*x) / (r1*r1) ] + [ (y*y) / (r2*r2) ] - [ (z*z) * (c*c) / (r1*r1) ] = 1
 *
 * r1:	semi-major axis, along Au
 * r2:	semi-minor axis, along Au x H
 * c:	slope of asymptotic cone in the Au-H plane
 */
/** @} */

#include "common.h"

/* system headers */
#include <stdio.h>
#include <math.h>

/* interface headers */
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"

#include "../../librt_private.h"


/* ray tracing form of solid, including precomputed terms */
struct hyp_specific {
    point_t hyp_V;	/* scaled vector to hyp origin */
    vect_t hyp_H;       /* scaled height vector */
    vect_t hyp_Au;	/* unit vector along semi-major axis */
    fastf_t hyp_r1;	/* scalar semi-major axis length */
    fastf_t hyp_r2;     /* scalar semi-minor axis length */
    fastf_t hyp_c;	/* slope of asymptote cone */

    vect_t hyp_Hunit;	/* unit H vector */
    vect_t hyp_Aunit;	/* unit vector along semi-major axis */
    vect_t hyp_Bunit;	/* unit vector, H x A, semi-minor axis */
    fastf_t hyp_Hmag;	/* scaled height of hyperboloid */

    fastf_t hyp_rx;
    fastf_t hyp_ry;	/* hyp_r* store coeffs */
    fastf_t hyp_rz;

    fastf_t hyp_bounds;	/* const used to check if a ray hits the top/bottom surfaces */
};


struct hyp_specific *
hyp_internal_to_specific(struct rt_hyp_internal *hyp_in) {
    struct hyp_specific *hyp;
    BU_GETSTRUCT(hyp, hyp_specific);

    hyp->hyp_r1 = hyp_in->hyp_bnr * MAGNITUDE(hyp_in->hyp_A);
    hyp->hyp_r2 = hyp_in->hyp_bnr * hyp_in->hyp_b;
    hyp->hyp_c = sqrt(4 * MAGSQ(hyp_in->hyp_A) / MAGSQ(hyp_in->hyp_Hi) * (1 - hyp_in->hyp_bnr * hyp_in->hyp_bnr));

    VSCALE(hyp->hyp_H, hyp_in->hyp_Hi, 0.5);
    VADD2(hyp->hyp_V, hyp_in->hyp_Vi, hyp->hyp_H);
    VMOVE(hyp->hyp_Au, hyp_in->hyp_A);
    VUNITIZE(hyp->hyp_Au);

    hyp->hyp_rx = 1.0 / (hyp->hyp_r1 * hyp->hyp_r1);
    hyp->hyp_ry = 1.0 / (hyp->hyp_r2 * hyp->hyp_r2);
    hyp->hyp_rz = (hyp->hyp_c * hyp->hyp_c) / (hyp->hyp_r1 * hyp->hyp_r1);

    /* calculate height to use for top/bottom intersection planes */
    hyp->hyp_Hmag = MAGNITUDE(hyp->hyp_H);
    hyp->hyp_bounds = hyp->hyp_rz*hyp->hyp_Hmag*hyp->hyp_Hmag + 1.0;

    /* setup unit vectors for hyp_specific */
    VMOVE(hyp->hyp_Hunit, hyp->hyp_H);
    VMOVE(hyp->hyp_Aunit, hyp->hyp_Au);
    VCROSS(hyp->hyp_Bunit, hyp->hyp_Hunit, hyp->hyp_Aunit);

    VUNITIZE(hyp->hyp_Aunit);
    VUNITIZE(hyp->hyp_Bunit);
    VUNITIZE(hyp->hyp_Hunit);

    return hyp;
}


const struct bu_structparse rt_hyp_parse[] = {
    { "%f", 3, "V",   bu_offsetof(struct rt_hyp_internal, hyp_Vi[X]), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "H",   bu_offsetof(struct rt_hyp_internal, hyp_Hi[X]), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "A",   bu_offsetof(struct rt_hyp_internal, hyp_A[X]), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "b", bu_offsetof(struct rt_hyp_internal, hyp_b), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "bnr",   bu_offsetof(struct rt_hyp_internal, hyp_bnr), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

/**
 * R T _ H Y P _ B B O X
 *
 * Create a bounding RPP for an hyp
 */
int
rt_hyp_bbox(struct rt_db_internal *ip, point_t *min, point_t *max) {
    struct rt_hyp_internal *xip;
    vect_t hyp_Au, hyp_B, hyp_An, hyp_Bn, hyp_H;
    vect_t pt1, pt2, pt3, pt4, pt5, pt6, pt7, pt8;
    RT_CK_DB_INTERNAL(ip);
    xip = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(xip);

    VMOVE(hyp_H, xip->hyp_Hi);
    VUNITIZE(hyp_H);
    VMOVE(hyp_Au, xip->hyp_A);
    VUNITIZE(hyp_Au);
    VCROSS(hyp_B, hyp_Au, hyp_H);

    VSETALL((*min), MAX_FASTF);
    VSETALL((*max), -MAX_FASTF);

    VSCALE(hyp_B, hyp_B, xip->hyp_b);
    VREVERSE(hyp_An, xip->hyp_A);
    VREVERSE(hyp_Bn, hyp_B);

    VADD3(pt1, xip->hyp_Vi, xip->hyp_A, hyp_B);
    VADD3(pt2, xip->hyp_Vi, xip->hyp_A, hyp_Bn);
    VADD3(pt3, xip->hyp_Vi, hyp_An, hyp_B);
    VADD3(pt4, xip->hyp_Vi, hyp_An, hyp_Bn);
    VADD4(pt5, xip->hyp_Vi, xip->hyp_A, hyp_B, xip->hyp_Hi);
    VADD4(pt6, xip->hyp_Vi, xip->hyp_A, hyp_Bn, xip->hyp_Hi);
    VADD4(pt7, xip->hyp_Vi, hyp_An, hyp_B, xip->hyp_Hi);
    VADD4(pt8, xip->hyp_Vi, hyp_An, hyp_Bn, xip->hyp_Hi);

    /* Find the RPP of the rotated axis-aligned hyp bbox - that is,
     * the bounding box the given hyp would have if its height
     * vector were in the positive Z direction. This does not give 
     * us an optimal bbox except in the case where the hyp is 
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
 * R T _ H Y P _ P R E P
 *
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid HYP, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0  HYP is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct hyp_specific is created, and its address is stored in
 * stp->st_specific for use by hyp_shot().
 */
int
rt_hyp_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_hyp_internal *hyp_ip;
    struct hyp_specific *hyp;
#ifndef NO_MAGIC_CHECKING
    const struct bn_tol *tol = &rtip->rti_tol;
#endif

#ifndef NO_MAGIC_CHECKING
    RT_CK_DB_INTERNAL(ip);
    BN_CK_TOL(tol);
#endif
    hyp_ip = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(hyp_ip);

    /* TODO: check that this is a valid hyperboloid (assume it is, for now)  */

    /* set soltab ID */
    stp->st_id = ID_HYP;
    stp->st_meth = &rt_functab[ID_HYP];

    hyp =  hyp_internal_to_specific(hyp_ip);
    stp->st_specific = (genptr_t)hyp;

    /* calculate bounding sphere */
    VMOVE(stp->st_center, hyp->hyp_V);
    stp->st_aradius = sqrt((hyp->hyp_c*hyp->hyp_c + 1)*MAGSQ(hyp->hyp_H)
			   + (hyp->hyp_r1*hyp->hyp_r1));
    stp->st_bradius = stp->st_aradius;
  
    /* calculate bounding RPP */
    if (rt_hyp_bbox(ip, &(stp->st_min), &(stp->st_max))) return 1;
    return 0;			/* OK */
}


/**
 * R T _ H Y P _ P R I N T
 */
void
rt_hyp_print(const struct soltab *stp)
{
    const struct hyp_specific *hyp =
	(struct hyp_specific *)stp->st_specific;

    VPRINT("V", hyp->hyp_V);
    VPRINT("Hunit", hyp->hyp_Hunit);
    VPRINT("Aunit", hyp->hyp_Aunit);
    VPRINT("Bunit", hyp->hyp_Bunit);
    fprintf(stderr, "h = %g\n", hyp->hyp_Hmag);
}


/* hit_surfno is set to one of these */
#define HYP_NORM_BODY	(1)		/* compute normal */
#define HYP_NORM_TOP	(2)		/* copy hyp_Hunit */
#define HYP_NORM_BOTTOM	(3)		/* copy -hyp_Huint */


/**
 * R T _ H Y P _ S H O T
 *
 * Intersect a ray with a hyp.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_hyp_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct hyp_specific *hyp =	(struct hyp_specific *)stp->st_specific;
    struct seg *segp;

    struct hit hits[5];	/* 4 potential hits (top, bottom, 2 sides) */
    struct hit *hitp;	/* pointer to hitpoint */

    vect_t dp;
    vect_t pp;
    fastf_t k1, k2;
    vect_t xlated;

    fastf_t a, b, c;
    fastf_t disc;
    fastf_t hitX, hitY;

    fastf_t height;

    hitp = &hits[0];

    dp[X] = VDOT(hyp->hyp_Aunit, rp->r_dir);
    dp[Y] = VDOT(hyp->hyp_Bunit, rp->r_dir);
    dp[Z] = VDOT(hyp->hyp_Hunit, rp->r_dir);

    VSUB2(xlated, rp->r_pt, hyp->hyp_V);
    pp[X] = VDOT(hyp->hyp_Aunit, xlated);
    pp[Y] = VDOT(hyp->hyp_Bunit, xlated);
    pp[Z] = VDOT(hyp->hyp_Hunit, xlated);

    /* find roots to quadratic (hitpoints) */
    a = hyp->hyp_rx*dp[X]*dp[X] + hyp->hyp_ry*dp[Y]*dp[Y] - hyp->hyp_rz*dp[Z]*dp[Z];
    b = 2.0 * (hyp->hyp_rx*pp[X]*dp[X] + hyp->hyp_ry*pp[Y]*dp[Y] - hyp->hyp_rz*pp[Z]*dp[Z]);
    c = hyp->hyp_rx*pp[X]*pp[X] + hyp->hyp_ry*pp[Y]*pp[Y] - hyp->hyp_rz*pp[Z]*pp[Z] - 1.0;

    disc = b*b - (4.0 * a * c);
    if (!NEAR_ZERO(a, RT_PCOEF_TOL)) {
	if (disc > 0) {
	    disc = sqrt(disc);

	    k1 = (-b + disc) / (2.0 * a);
	    k2 = (-b - disc) / (2.0 * a);

	    VJOIN1(hitp->hit_vpriv, pp, k1, dp);
	    height = hitp->hit_vpriv[Z];
	    if (fabs(height) <= hyp->hyp_Hmag) {
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = k1;
		hitp->hit_surfno = HYP_NORM_BODY;
		hitp++;
	    }

	    VJOIN1(hitp->hit_vpriv, pp, k2, dp);
	    height = hitp->hit_vpriv[Z];
	    if (fabs(height) <= hyp->hyp_Hmag) {
		hitp->hit_magic = RT_HIT_MAGIC;
		hitp->hit_dist = k2;
		hitp->hit_surfno = HYP_NORM_BODY;
		hitp++;
	    }
	}
    } else if (!NEAR_ZERO(b, RT_PCOEF_TOL)) {
	k1 = -c / b;
	VJOIN1(hitp->hit_vpriv, pp, k1, dp);
	if (hitp->hit_vpriv[Z] >= -hyp->hyp_Hmag
	    && hitp->hit_vpriv[Z] <= hyp->hyp_Hmag) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = HYP_NORM_BODY;
	    hitp++;
	}
    }

    /* check top & bottom plates */
    k1 = (hyp->hyp_Hmag - pp[Z]) / dp[Z];
    k2 = (-hyp->hyp_Hmag - pp[Z]) / dp[Z];

    VJOIN1(hitp->hit_vpriv, pp, k1, dp);
    hitX = hitp->hit_vpriv[X];
    hitY = hitp->hit_vpriv[Y];
    /* check if hitpoint is on the top surface */
    if ((hyp->hyp_rx*hitX*hitX + hyp->hyp_ry*hitY*hitY) < hyp->hyp_bounds) {
	hitp->hit_magic = RT_HIT_MAGIC;
	hitp->hit_dist = k1;
	hitp->hit_surfno = HYP_NORM_TOP;
	hitp++;
    }

    VJOIN1(hitp->hit_vpriv, pp, k2, dp);
    hitX = hitp->hit_vpriv[X];
    hitY = hitp->hit_vpriv[Y];
    /* check if hitpoint is on the bottom surface */
    if ((hyp->hyp_rx*hitX*hitX + hyp->hyp_ry*hitY*hitY) < hyp->hyp_bounds) {
	hitp->hit_magic = RT_HIT_MAGIC;
	hitp->hit_dist = k2;
	hitp->hit_surfno = HYP_NORM_BOTTOM;
	hitp++;
    }

    if (hitp == &hits[0] || hitp == &hits[1] || hitp == &hits[3]) {
	return 0;	/* MISS */
    }

    if (hitp == &hits[2]) {
	/* 2 hits */
	if (hits[0].hit_dist < hits[1].hit_dist) {
	    /* entry is [0], exit is [1] */
	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;
	    segp->seg_in = hits[0];	/* struct copy */
	    segp->seg_out = hits[1];	/* struct copy */
	    BU_LIST_INSERT(&(seghead->l), &(segp->l));
	} else {
	    /* entry is [1], exit is [0] */

	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;
	    segp->seg_in = hits[1];	/* struct copy */
	    segp->seg_out = hits[0];	/* struct copy */
	    BU_LIST_INSERT(&(seghead->l), &(segp->l));
	}
	return 2;			/* HIT */
    } else {
	/* 4 hits:  0, 1 are sides, 2, 3 are top/bottom*/
	struct hit sorted[4];

	if (hits[0].hit_dist > hits[1].hit_dist) {
	    sorted[1] = hits[1];
	    sorted[2] = hits[0];
	} else {
	    sorted[1] = hits[0];
	    sorted[2] = hits[1];
	}
	if (hits[2].hit_dist > hits[3].hit_dist) {
	    sorted[0] = hits[3];
	    sorted[3] = hits[2];
	} else {
	    sorted[0] = hits[2];
	    sorted[3] = hits[3];
	}

	if (sorted[0].hit_dist > sorted[1].hit_dist
	    || sorted[1].hit_dist > sorted[2].hit_dist
	    || sorted[2].hit_dist > sorted[3].hit_dist) {
	    bu_log("sorting error\n");
	}

	/* hit segments are now (0, 1) and (2, 3) */
	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in = sorted[0];	/* struct copy */
	segp->seg_out = sorted[1];	/* struct copy */
	BU_LIST_INSERT(&(seghead->l), &(segp->l));

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in = sorted[2];	/* struct copy */
	segp->seg_out = sorted[3];	/* struct copy */
	BU_LIST_INSERT(&(seghead->l), &(segp->l));

	return 4;
    }
}


/**
 * R T _ H Y P _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_hyp_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    struct hyp_specific *hyp =
	(struct hyp_specific *)stp->st_specific;

    /* normal from basic hyperboloid and transformed normal */
    vect_t n, nT;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    switch (hitp->hit_surfno) {
	case HYP_NORM_TOP:
	    VMOVE(hitp->hit_normal, hyp->hyp_Hunit);
	    break;
	case HYP_NORM_BOTTOM:
	    VREVERSE(hitp->hit_normal, hyp->hyp_Hunit);
	    break;
	case HYP_NORM_BODY:
	    /* normal vector is VUNITIZE(z * dz/dx, z * dz/dy, -z) */
	    /* z = +- (c/a) * sqrt(x^2/a^2 + y^2/b^2 -1) */
	    VSET(n, hyp->hyp_rx * hitp->hit_vpriv[X],
		 hyp->hyp_ry * hitp->hit_vpriv[Y],
		 -hyp->hyp_rz * hitp->hit_vpriv[Z]);

	    nT[X] = (hyp->hyp_Aunit[X] * n[X])
		+ (hyp->hyp_Bunit[X] * n[Y])
		+ (hyp->hyp_Hunit[X] * n[Z]);
	    nT[Y] = (hyp->hyp_Aunit[Y] * n[X])
		+ (hyp->hyp_Bunit[Y] * n[Y])
		+ (hyp->hyp_Hunit[Y] * n[Z]);
	    nT[Z] = (hyp->hyp_Aunit[Z] * n[X])
		+ (hyp->hyp_Bunit[Z] * n[Y])
		+ (hyp->hyp_Hunit[Z] * n[Z]);

	    VUNITIZE(nT);
	    VMOVE(hitp->hit_normal, nT);
	    break;
	default:
	    bu_log("rt_hyp_norm: surfno=%d bad\n", hitp->hit_surfno);
	    break;
    }
}


/**
 * R T _ H Y P _ C U R V E
 *
 * Return the curvature of the hyp.
 */
void
rt_hyp_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    struct hyp_specific *hyp =
	(struct hyp_specific *)stp->st_specific;
    vect_t vert, horiz;
    point_t hp;
    fastf_t c, h, k1, k2, denom;
    fastf_t x, y, ratio;

    switch (hitp->hit_surfno) {
	case HYP_NORM_BODY:
	    /* calculate principle curvature directions */
	    VMOVE(hp, hitp->hit_vpriv);

	    VMOVE(vert, hitp->hit_normal);
	    vert[Z] += 10;
	    VCROSS(horiz, vert, hitp->hit_normal);
	    VUNITIZE(horiz);
	    VCROSS(vert, hitp->hit_normal, horiz);
	    VUNITIZE(vert);

	    /* vertical curvature */
	    c = hyp->hyp_c;
	    h = sqrt(hp[X]*hp[X] + hp[Y]*hp[Y]);

	    denom = 1 + (c*c*c*c)*(hp[Z]*hp[Z])/(h*h);
	    denom = sqrt(denom*denom*denom);

	    /* k1 is in the vert direction on the hyberbola */
	    k1 = fabs(c*c/h - (c*c*c*c)*(hp[Z]*hp[Z])/(h*h*h)) / denom;

	    /* horizontal curvature */
	    if (fabs(hp[Y]) >= fabs(hp[X])) {
		ratio = hyp->hyp_rx / hyp->hyp_ry;	/* (b/a)^2 */
		x = hp[X];
		y = hp[Y];
	    } else {
		/* flip x and y to avoid div by zero */
		ratio = hyp->hyp_ry / hyp->hyp_rx;
		x = hp[Y];
		y = hp[X];
	    }

	    denom = fabs(y*y*y + (ratio*ratio)*(x*x)*y);
	    denom = sqrt(denom*denom*denom);

	    /* k2 is in the horiz direction on the ellipse */
	    k2 = -fabs(ratio*y + (ratio*ratio)*(x*x)/y) / denom;

	    if (k1 < fabs(k2)) {
		VMOVE(cvp->crv_pdir, vert);
		cvp->crv_c1 = k1;
		cvp->crv_c2 = k2;
	    } else {
		VMOVE(cvp->crv_pdir, horiz);
		cvp->crv_c1 = k2;
		cvp->crv_c2 = k1;
	    }
	    break;
	case HYP_NORM_TOP:
	case HYP_NORM_BOTTOM:
	    cvp->crv_c1 = cvp->crv_c2 = 0;
	    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
	    break;
    }
}


/**
 * R T _ H Y P _ U V
 *
 * For a hit on the surface of an hyp, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 *
 * u = azimuth
 * v = elevation
 */
void
rt_hyp_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    struct hyp_specific *hyp =	(struct hyp_specific *)stp->st_specific;

    if (ap) RT_CK_APPLICATION(ap);

    /* u = (angle from semi-major axis on basic hyperboloid) / (2*pi) */
    uvp->uv_u = M_1_PI * 0.5
	* (atan2(-hitp->hit_vpriv[X] * hyp->hyp_r2, hitp->hit_vpriv[Y] * hyp->hyp_r1) + M_PI);

    /* v ranges (0, 1) on each plate */
    switch(hitp->hit_surfno) {
	case HYP_NORM_BODY:
	    /* v = (z + Hmag) / (2*Hmag) */
	    uvp->uv_v = (hitp->hit_vpriv[Z] + hyp->hyp_Hmag) / (2.0 * hyp->hyp_Hmag);
	    break;
	case HYP_NORM_TOP:
	    uvp->uv_v = 1.0 - sqrt(
		((hitp->hit_vpriv[X]*hitp->hit_vpriv[X])*hyp->hyp_rx
		 + (hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y])*hyp->hyp_ry)
		/ (1 + (hitp->hit_vpriv[Z]*hitp->hit_vpriv[Z])*hyp->hyp_rz));
	    break;
	case HYP_NORM_BOTTOM:
	    uvp->uv_v = sqrt(
		((hitp->hit_vpriv[X]*hitp->hit_vpriv[X])*hyp->hyp_rx
		 + (hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y])*hyp->hyp_ry)
		/ (1 + (hitp->hit_vpriv[Z]*hitp->hit_vpriv[Z])*hyp->hyp_rz));
	    break;
    }

    /* sanity checks */
    if (uvp->uv_u < 0.0)
	uvp->uv_u = 0.0;
    else if (uvp->uv_u > 1.0)
	uvp->uv_u = 1.0;
    if (uvp->uv_v < 0.0)
	uvp->uv_v = 0.0;
    else if (uvp->uv_v > 1.0)
	uvp->uv_v = 1.0;

    /* copied from g_ehy.c */
    uvp->uv_du = uvp->uv_dv = 0;
}


/**
 * R T _ H Y P _ F R E E
 */
void
rt_hyp_free(struct soltab *stp)
{
    struct hyp_specific *hyp =
	(struct hyp_specific *)stp->st_specific;

    bu_free((char *)hyp, "hyp_specific");
}


/**
 * R T _ H Y P _ P L O T
 */
int
rt_hyp_plot(struct bu_list *vhead, struct rt_db_internal *incoming, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
{
    int i, j;		/* loop indices */
    struct rt_hyp_internal *hyp_in;
    struct hyp_specific *hyp_ip;
    vect_t majorAxis[8],	/* vector offsets along major axis */
	minorAxis[8],		/* vector offsets along minor axis */
	heightAxis[7],		/* vector offsets for layers */
	Bunit;			/* unit vector along semi-minor axis */
    vect_t ell[16];		/* stores 16 points to draw ellipses */
    vect_t ribs[16][7];		/* assume 7 layers for now */
    fastf_t scale;		/* used to calculate semi-major/minor axes for top/bottom */
    fastf_t cos22_5 = 0.9238795325112867385;
    fastf_t cos67_5 = 0.3826834323650898373;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(incoming);
    hyp_in = (struct rt_hyp_internal *)incoming->idb_ptr;
    RT_HYP_CK_MAGIC(hyp_in);

    hyp_ip = hyp_internal_to_specific(hyp_in);

    VCROSS(Bunit, hyp_ip->hyp_H, hyp_ip->hyp_Au);
    VUNITIZE(Bunit);

    VMOVE(heightAxis[0], hyp_ip->hyp_H);
    VSCALE(heightAxis[1], heightAxis[0], 0.5);
    VSCALE(heightAxis[2], heightAxis[0], 0.25);
    VSETALL(heightAxis[3], 0);
    VREVERSE(heightAxis[4], heightAxis[2]);
    VREVERSE(heightAxis[5], heightAxis[1]);
    VREVERSE(heightAxis[6], heightAxis[0]);

    for (i = 0; i < 7; i++) {
	/* determine Z height depending on i */
	scale = sqrt(MAGSQ(heightAxis[i])*(hyp_ip->hyp_c * hyp_ip->hyp_c)/(hyp_ip->hyp_r1 * hyp_ip->hyp_r1) + 1);

	/* calculate vectors for offset */
	VSCALE(majorAxis[0], hyp_ip->hyp_Au, hyp_ip->hyp_r1 * scale);
	VSCALE(majorAxis[1], majorAxis[0], cos22_5);
	VSCALE(majorAxis[2], majorAxis[0], M_SQRT1_2);
	VSCALE(majorAxis[3], majorAxis[0], cos67_5);
	VREVERSE(majorAxis[4], majorAxis[3]);
	VREVERSE(majorAxis[5], majorAxis[2]);
	VREVERSE(majorAxis[6], majorAxis[1]);
	VREVERSE(majorAxis[7], majorAxis[0]);

	VSCALE(minorAxis[0], Bunit, hyp_ip->hyp_r2 * scale);
	VSCALE(minorAxis[1], minorAxis[0], cos22_5);
	VSCALE(minorAxis[2], minorAxis[0], M_SQRT1_2);
	VSCALE(minorAxis[3], minorAxis[0], cos67_5);
	VREVERSE(minorAxis[4], minorAxis[3]);
	VREVERSE(minorAxis[5], minorAxis[2]);
	VREVERSE(minorAxis[6], minorAxis[1]);
	VREVERSE(minorAxis[7], minorAxis[0]);

	/* calculate ellipse */
	VADD3(ell[ 0], hyp_ip->hyp_V, heightAxis[i], majorAxis[0]);
	VADD4(ell[ 1], hyp_ip->hyp_V, heightAxis[i], majorAxis[1], minorAxis[3]);
	VADD4(ell[ 2], hyp_ip->hyp_V, heightAxis[i], majorAxis[2], minorAxis[2]);
	VADD4(ell[ 3], hyp_ip->hyp_V, heightAxis[i], majorAxis[3], minorAxis[1]);
	VADD3(ell[ 4], hyp_ip->hyp_V, heightAxis[i], minorAxis[0]);
	VADD4(ell[ 5], hyp_ip->hyp_V, heightAxis[i], majorAxis[4], minorAxis[1]);
	VADD4(ell[ 6], hyp_ip->hyp_V, heightAxis[i], majorAxis[5], minorAxis[2]);
	VADD4(ell[ 7], hyp_ip->hyp_V, heightAxis[i], majorAxis[6], minorAxis[3]);
	VADD3(ell[ 8], hyp_ip->hyp_V, heightAxis[i], majorAxis[7]);
	VADD4(ell[ 9], hyp_ip->hyp_V, heightAxis[i], majorAxis[6], minorAxis[4]);
	VADD4(ell[10], hyp_ip->hyp_V, heightAxis[i], majorAxis[5], minorAxis[5]);
	VADD4(ell[11], hyp_ip->hyp_V, heightAxis[i], majorAxis[4], minorAxis[6]);
	VADD3(ell[12], hyp_ip->hyp_V, heightAxis[i], minorAxis[7]);
	VADD4(ell[13], hyp_ip->hyp_V, heightAxis[i], majorAxis[3], minorAxis[6]);
	VADD4(ell[14], hyp_ip->hyp_V, heightAxis[i], majorAxis[2], minorAxis[5]);
	VADD4(ell[15], hyp_ip->hyp_V, heightAxis[i], majorAxis[1], minorAxis[4]);

	/* draw ellipse */
	RT_ADD_VLIST(vhead, ell[15], BN_VLIST_LINE_MOVE);
	for (j=0; j<16; j++) {
	    RT_ADD_VLIST(vhead, ell[j], BN_VLIST_LINE_DRAW);
	}

	/* add ellipse's points to ribs */
	for (j=0; j<16; j++) {
	    VMOVE(ribs[j][i], ell[j]);
	}
    }

    /* draw ribs */
    for (i=0; i<16; i++) {
	RT_ADD_VLIST(vhead, ribs[i][0], BN_VLIST_LINE_MOVE);
	for (j=1; j<7; j++) {
	    RT_ADD_VLIST(vhead, ribs[i][j], BN_VLIST_LINE_DRAW);
	}

    }

    return 0;
}


/**
 * R T _ H Y P _ T E S S
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_hyp_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    fastf_t c, dtol, f, mag_a, mag_h, ntol, r1, r2, r3, cprime;
    fastf_t **ellipses, theta_new;
    int *pts_dbl, face, i, j, nseg;
    int jj, nell;
    mat_t invRoS;
    mat_t SoR;
    struct rt_hyp_internal *iip;
    struct hyp_specific *xip;
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

    MAT_ZERO(invRoS);
    MAT_ZERO(SoR);

    RT_CK_DB_INTERNAL(ip);
    iip = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(iip);

    xip = hyp_internal_to_specific(iip);

    /*
     * make sure hyp description is valid
     */

    /* compute |A| |H| */
    mag_a = MAGSQ(xip->hyp_Au);	/* should already be unit vector */
    mag_h = MAGNITUDE(xip->hyp_H);
    c = xip->hyp_c;
    cprime = c / mag_h;
    r1 = xip->hyp_r1;
    r2 = xip->hyp_r2;
    r3 = r1 / c;
    /* Check for |H| > 0, |A| == 1, r1 > 0, r2 > 0, c > 0 */
    if (NEAR_ZERO(mag_h, RT_LEN_TOL)
	|| !NEAR_EQUAL(mag_a, 1.0, RT_LEN_TOL)
	|| r1 <= 0.0 || r2 <= 0.0 || c <= 0.) {
	return 1;		/* BAD */
    }

    /* Check for A.H == 0 */
    f = VDOT(xip->hyp_Au, xip->hyp_H) / mag_h;
    if (! NEAR_ZERO(f, RT_DOT_TOL)) {
	return 1;		/* BAD */
    }

    /* make unit vectors in A, H, and HxA directions */
    VMOVE(Hu, xip->hyp_H);
    VUNITIZE(Hu);
    VMOVE(Au, xip->hyp_Au);
    VCROSS(Bu, Hu, Au);

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
	/* else, use absolute-ized relative tolerance */
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
     * build hyp from 2 hyperbolas
     */

    /* calculate major axis hyperbola */
    pts_a = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");

    /* set base, center, and top points */
    pos_a = pts_a;
    VSET(pos_a->p, sqrt((mag_h*mag_h) * (c*c) + (r1*r1)), 0, -mag_h);
    pos_a->next = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
    pos_a = pos_a->next;
    VSET(pos_a->p, r1, 0, 0);
    pos_a->next = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
    pos_a = pos_a->next;
    VSET(pos_a->p, sqrt((mag_h*mag_h) * (c*c) + (r1*r1)), 0, mag_h);
    pos_a->next = NULL;

    /* refine hyp according to tolerances */
    i = 1;
    {
	point_t p0, p1, p2;
	fastf_t mm, len, dist, ang0, ang2;
	vect_t v01, v02; /* vectors from p0->p1 and p0->p2 */
	vect_t nLine, nHyp;
	struct rt_pt_node *add;

	while (i) {
	    pos_a = pts_a;
	    i = 0;
	    while (pos_a->next) {

		VMOVE(p0, pos_a->p);
		VMOVE(p2, pos_a->next->p);
		/* either X or Y will be zero; so adding handles either case */
		mm = (p2[Z] - p0[Z]) / ((p2[X]+p2[Y]) - (p0[X]+p0[Y]));
		if (!ZERO(p0[X])) {
		    p1[X] = fabs(mm*c*r1) / sqrt(mm*mm*c*c - 1.0);
		    p1[Y] = 0.0;
		    p1[Z] = sqrt(p1[X]*p1[X] - r1*r1) / c;
		} else {
		    p1[X] = 0.0;
		    p1[Y] = fabs(mm*r2*r2*c) / sqrt(mm*mm*r2*r2*c*c - r1*r1);
		    p1[Z] = (r3/r2) * sqrt(p1[Y]*p1[Y] - r2*r2);
		}
		if (p0[Z] + p2[Z] < 0) p1[Z] = -p1[Z];

		VSUB2(v01, p1, p0);
		VSUB2(v02, p2, p0);
		VUNITIZE(v02);
		len = VDOT(v01, v02);
		VSCALE(v02, v02, len);
		VSUB2(nLine, v01, v02);
		dist = MAGNITUDE(nLine);
		VUNITIZE(nLine);

		VSET(nHyp, p0[X] / (r1*r1), p0[Y] / (r2*r2), p0[Z] / (r3*r3));
		VUNITIZE(nHyp);
		ang0 = fabs(acos(VDOT(nLine, nHyp)));
		VSET(nHyp, p2[X] / (r1*r1), p2[Y] / (r2*r2), p2[Z] / (r3*r3));
		VUNITIZE(nHyp);
		ang2 = fabs(acos(VDOT(nLine, nHyp)));

		if (dist > dtol || ang0 > ntol || ang2 > ntol) {
		    /* split segment */
		    add = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
		    VMOVE(add->p, p1);
		    add->next = pos_a->next;
		    pos_a->next = add;
		    pos_a = pos_a->next;
		    i = 1;
		}
		pos_a = pos_a->next;
	    }
	}

    }

    /* calculate minor axis hyperbola */
    pts_b = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");

    pos_a = pts_a;
    pos_b = pts_b;
    i = 0;
    while (pos_a) {
	pos_b->p[Z] = pos_a->p[Z];
	pos_b->p[X] = 0;
	pos_b->p[Y] = r2 * sqrt(pos_b->p[Z] * pos_b->p[Z]/(r3*r3) + 1.0);
	pos_a = pos_a->next;
	if (pos_a) {
	    pos_b->next = (struct rt_pt_node *)bu_malloc(sizeof(struct rt_pt_node), "rt_pt_node");
	    pos_b = pos_b->next;
	} else {
	    pos_b->next = NULL;
	}
	i++;
    }

    nell = i;

    /* make array of ptrs to hyp ellipses */
    ellipses = (fastf_t **)bu_malloc(nell * sizeof(fastf_t *), "fastf_t ell[]");

    /* keep track of whether pts in each ellipse are doubled or not */
    pts_dbl = (int *)bu_malloc(nell * sizeof(int), "dbl ints");

    /* make ellipses at each z level */
    i = 0;
    nseg = 0;
    pos_a = pts_a;	/*->next; */	/* skip over apex of hyp */
    pos_b = pts_b;	/*->next; */
    while (pos_a) {
	point_t p1;

	VSCALE(A, Au, pos_a->p[X]);	/* semimajor axis */
	VSCALE(B, Bu, pos_b->p[Y]);	/* semiminor axis */
	VJOIN1(V, xip->hyp_V, -pos_a->p[Z], Hu);

	VSET(p1, 0., pos_b->p[Y], 0.);
	theta_new = ell_angle(p1, pos_a->p[X], pos_b->p[Y], dtol, ntol);
	if (nseg == 0) {
	    nseg = (int)(bn_twopi / theta_new) + 1;
	    pts_dbl[i] = 0;
	    /* maximum number of faces needed for hyp */
	    face = 2*nseg*nell - 4;	/*nseg*(1 + 3*((1 << (nell-1)) - 1));*/
	    /* array for each triangular face */
	    outfaceuses = (struct faceuse **)
		bu_malloc((face+1) * sizeof(struct faceuse *), "hyp: *outfaceuses[]");
	} else {
	    pts_dbl[i] = 0;
	}

	ellipses[i] = (fastf_t *)bu_malloc(3*(nseg+1)*sizeof(fastf_t), "pts ell");
	rt_ell(ellipses[i], V, A, B, nseg);

	i++;
	pos_a = pos_a->next;
	pos_b = pos_b->next;
    }

    /*
     * put hyp geometry into nmg data structures
     */

    *r = nmg_mrsv(m);	/* Make region, empty shell, vertex */
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    /* vertices of ellipses of hyp */
    vells = (struct vertex ***)
	bu_malloc(nell*sizeof(struct vertex **), "vertex [][]");
    j = nseg;
    for (i = nell-1; i >= 0; i--) {
	vells[i] = (struct vertex **)
	    bu_malloc(j*sizeof(struct vertex *), "vertex []");
	if (i && pts_dbl[i])
	    j /= 2;
    }

    /* top face of hyp */
    for (i = 0; i < nseg; i++)
	vells[nell-1][i] = (struct vertex *)NULL;
    face = 0;
    BU_ASSERT_PTR(outfaceuses, !=, NULL);
    if ((outfaceuses[face++] = nmg_cface(s, vells[nell-1], nseg)) == 0) {
	bu_log("rt_hyp_tess() failure, top face\n");
	goto fail;
    }
    fu_top = outfaceuses[0];

    /* Mark edges of this face as real, this is the only real edge */
    for (BU_LIST_FOR (lu, loopuse, &outfaceuses[0]->lu_hd)) {
	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;
	for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
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
		    bu_log("rt_hyp_tess() failure\n");
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
		    bu_log("rt_hyp_tess() failure\n");
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
		    bu_log("rt_hyp_tess() failure\n");
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
		    bu_log("rt_hyp_tess() failure\n");
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
		    bu_log("rt_hyp_tess() failure\n");
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

    /* bottom face of hyp */
    for (i = 0; i < nseg; i++)
	vells[0][i] = (struct vertex *)NULL;

    BU_ASSERT_PTR(outfaceuses, !=, NULL);
    if ((outfaceuses[face++] = nmg_cface(s, vells[0], nseg)) == 0) {
	bu_log("rt_hyp_tess() failure, top face\n");
	goto fail;
    }
    fu_top = outfaceuses[face-1];

    /* Mark edges of this face as real, this is the only real edge */
    for (BU_LIST_FOR (lu, loopuse, &outfaceuses[face-1]->lu_hd)) {
	NMG_CK_LOOPUSE(lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;
	for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
	    struct edge *e;

	    NMG_CK_EDGEUSE(eu);
	    e = eu->e_p;
	    NMG_CK_EDGE(e);
	    e->is_real = 1;
	}
    }

    for (i = 0; i < nseg; i++) {
	NMG_CK_VERTEX(vells[0][i]);
	nmg_vertex_gv(vells[0][i], &ellipses[0][3*i]);
    }

    /* Associate the face geometry */
    for (i=0; i < face; i++) {
	if (nmg_fu_planeeqn(outfaceuses[i], tol) < 0) {
	    bu_log("planeeqn fail:\n\ti:\t%d\n\toutfaceuses:\n\tmin:\t%f\t%f\t%f\n\tmax:\t%f\t%f\t%f\n",
		   i, outfaceuses[i]->f_p->min_pt[0],
		   outfaceuses[i]->f_p->min_pt[1],
		   outfaceuses[i]->f_p->min_pt[2],
		   outfaceuses[i]->f_p->max_pt[0],
		   outfaceuses[i]->f_p->max_pt[1],
		   outfaceuses[i]->f_p->max_pt[2]);
	    goto fail;
	}
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

	VSUB2(tmp_pt, vg->coord, xip->hyp_V);
	MAT4X3VEC(pt_prime, SoR, tmp_pt);
	VSET(tmp_vect, pt_prime[X]*(2*cprime+1), pt_prime[Y]*(2*cprime+1), -(pt_prime[Z]+cprime+1));
	MAT4X3VEC(norm, invRoS, tmp_vect);
	VUNITIZE(norm);
	VREVERSE(rev_norm, norm);

	for (BU_LIST_FOR (vu, vertexuse, &v->vu_hd)) {
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
 * R T _ H Y P _ I M P O R T 5
 *
 * Import an HYP from the database format to the internal format.
 * Note that the data read will be in network order.  This means
 * Big-Endian integers and IEEE doubles for floating point.
 *
 * Apply modeling transformations as well.
 */
int
rt_hyp_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip)
{
    struct rt_hyp_internal *hyp_ip;
    fastf_t vec[ELEMENTS_PER_VECT*4];

    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);

    BU_ASSERT_LONG(ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 3 * 4);

    /* set up the internal structure */
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_HYP;
    ip->idb_meth = &rt_functab[ID_HYP];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_hyp_internal), "rt_hyp_internal");
    hyp_ip = (struct rt_hyp_internal *)ip->idb_ptr;
    hyp_ip->hyp_magic = RT_HYP_INTERNAL_MAGIC;

    /* Convert the data in ep->ext_buf into internal format.  Note the
     * conversion from network data (Big Endian ints, IEEE double
     * floating point) to host local data representations.
     */
    ntohd((unsigned char *)&vec, (const unsigned char *)ep->ext_buf, ELEMENTS_PER_VECT*4);

    /* Apply the modeling transformation */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(hyp_ip->hyp_Vi, mat, &vec[0*3]);
    MAT4X3VEC(hyp_ip->hyp_Hi, mat, &vec[1*3]);
    MAT4X3VEC(hyp_ip->hyp_A, mat, &vec[2*3]);

    if (!ZERO(mat[15]))
	hyp_ip->hyp_b = vec[ 9] / mat[15];
    else
	hyp_ip->hyp_b = INFINITY;
    hyp_ip->hyp_bnr = vec[10] ;

    return 0;			/* OK */
}


/**
 * R T _ H Y P _ E X P O R T 5
 *
 * Export an HYP from internal form to external format.  Note that
 * this means converting all integers to Big-Endian format and
 * floating point data to IEEE double.
 *
 * Apply the transformation to mm units as well.
 */
int
rt_hyp_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_hyp_internal *hyp_ip;
    fastf_t vec[ELEMENTS_PER_VECT * 4];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_HYP) return -1;
    hyp_ip = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(hyp_ip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT * 4;
    ep->ext_buf = (genptr_t)bu_calloc(1, ep->ext_nbytes, "hyp external");


    /* Since libwdb users may want to operate in units other than mm,
     * we offer the opportunity to scale the solid (to get it into mm)
     * on the way out.
     */
    VSCALE(&vec[0*3], hyp_ip->hyp_Vi, local2mm);
    VSCALE(&vec[1*3], hyp_ip->hyp_Hi, local2mm);
    VSCALE(&vec[2*3], hyp_ip->hyp_A, local2mm);
    vec[ 9] = hyp_ip->hyp_b * local2mm;
    vec[10] = hyp_ip->hyp_bnr * local2mm;

    /* Convert from internal (host) to database (network) format */
    htond(ep->ext_buf, (unsigned char *)vec, ELEMENTS_PER_VECT*4);

    return 0;
}


/**
 * R T _ H Y P _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_hyp_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_hyp_internal *hyp_ip;
    char buf[256];

    hyp_ip = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(hyp_ip);
    bu_vls_strcat(str, "truncated general hyp (HYP)\n");

    if (!verbose)
	return 0;

    sprintf(buf, "\tV (%g, %g, %g)\n",
	    INTCLAMP(hyp_ip->hyp_Vi[X] * mm2local),
	    INTCLAMP(hyp_ip->hyp_Vi[Y] * mm2local),
	    INTCLAMP(hyp_ip->hyp_Vi[Z] * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tH (%g, %g, %g) mag=%g\n",
	    INTCLAMP(hyp_ip->hyp_Hi[X] * mm2local),
	    INTCLAMP(hyp_ip->hyp_Hi[Y] * mm2local),
	    INTCLAMP(hyp_ip->hyp_Hi[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(hyp_ip->hyp_Hi) * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tA (%g, %g, %g)\n",
	    INTCLAMP(hyp_ip->hyp_A[X] * mm2local),
	    INTCLAMP(hyp_ip->hyp_A[Y] * mm2local),
	    INTCLAMP(hyp_ip->hyp_A[Z] * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tMag B=%g\n", INTCLAMP(hyp_ip->hyp_b * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tNeck to Base Ratio=%g\n", INTCLAMP(hyp_ip->hyp_bnr * mm2local));
    bu_vls_strcat(str, buf);

    return 0;
}


/**
 * R T _ H Y P _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_hyp_ifree(struct rt_db_internal *ip)
{
    struct rt_hyp_internal *hyp_ip;

    RT_CK_DB_INTERNAL(ip);

    hyp_ip = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(hyp_ip);
    hyp_ip->hyp_magic = 0;			/* sanity */

    bu_free((char *)hyp_ip, "hyp ifree");
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}


/**
 * R T _ H Y P _ P A R A M S
 */
int
rt_hyp_params(struct pc_pc_set * UNUSED(ps), const struct rt_db_internal *ip)
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
