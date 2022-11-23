#include "common.cl"


/* hit_surfno is set to one of these */
#define HYP_NORM_BODY	(1)	/* compute normal */
#define HYP_NORM_TOP	(2)	/* copy hyp_Hunit */
#define HYP_NORM_BOTTOM	(3)	/* copy -hyp_Hunit */


/* ray tracing form of solid, including precomputed terms */
struct hyp_specific {
    double hyp_V[3];		/* scaled vector to hyp origin */

    double hyp_Hunit[3];	/* unit H vector */
    double hyp_Aunit[3];	/* unit vector along semi-major axis */
    double hyp_Bunit[3];	/* unit vector, H x A, semi-minor axis */
    double hyp_Hmag;		/* scaled height of hyperboloid */

    double hyp_rx;
    double hyp_ry;		/* hyp_r* store coeffs */
    double hyp_rz;

    double hyp_bounds;		/* const used to check if a ray hits the top/bottom surfaces */
};

/**
 * Intersect a ray with a hyp.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int hyp_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct hyp_specific *hyp)
{
    struct hit hits[5];	// 4 potential hits (top, bottom, 2 sides)
    struct hit *hitp;	// pointer to hitpoint

    double3 dp;
    double3 pp;
    double k1, k2;
    double3 xlated;

    double a, b, c;
    double disc;
    double hitX, hitY;

    double height;

    hitp = &hits[0];

    dp.x = dot(vload3(0, hyp->hyp_Aunit), r_dir);
    dp.y = dot(vload3(0, hyp->hyp_Bunit), r_dir);
    dp.z = dot(vload3(0, hyp->hyp_Hunit), r_dir);

    xlated = r_pt - vload3(0, hyp->hyp_V);
    pp.x = dot(vload3(0, hyp->hyp_Aunit), xlated);
    pp.y = dot(vload3(0, hyp->hyp_Bunit), xlated);
    pp.z = dot(vload3(0, hyp->hyp_Hunit), xlated);

    /* find roots to quadratic (hitpoints) */
    a = hyp->hyp_rx*dp.x*dp.x + hyp->hyp_ry*dp.y*dp.y - hyp->hyp_rz*dp.z*dp.z;
    b = 2.0 * (hyp->hyp_rx*pp.x*dp.x + hyp->hyp_ry*pp.y*dp.y - hyp->hyp_rz*pp.z*dp.z);
    c = hyp->hyp_rx*pp.x*pp.x + hyp->hyp_ry*pp.y*pp.y - hyp->hyp_rz*pp.z*pp.z - 1.0;

    disc = b*b - (4.0 * a * c);
    if (!NEAR_ZERO(a, RT_PCOEF_TOL)) {
	if (disc > 0) {
	    disc = sqrt(disc);

	    k1 = (-b + disc) / (2.0 * a);
	    k2 = (-b - disc) / (2.0 * a);

	    hitp->hit_vpriv = pp + k1 * dp;
	    height = hitp->hit_vpriv.z;
	    if (fabs(height) <= hyp->hyp_Hmag) {
		hitp->hit_dist = k1;
		hitp->hit_surfno = HYP_NORM_BODY;
		hitp++;
	    }

	    hitp->hit_vpriv = pp + k2 * dp;
	    height = hitp->hit_vpriv.z;
	    if (fabs(height) <= hyp->hyp_Hmag) {
		hitp->hit_dist = k2;
		hitp->hit_surfno = HYP_NORM_BODY;
		hitp++;
	    }
	}
    } else if (!NEAR_ZERO(b, RT_PCOEF_TOL)) {
	k1 = -c / b;
	hitp->hit_vpriv = pp + k1 * dp;
	if (hitp->hit_vpriv.z >= -hyp->hyp_Hmag && hitp->hit_vpriv.z <= hyp->hyp_Hmag) {
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = HYP_NORM_BODY;
	    hitp++;
	}
    }

    /* check top & bottom plates */
    k1 = (hyp->hyp_Hmag - pp.z) / dp.z;
    k2 = (-hyp->hyp_Hmag - pp.z) / dp.z;

    hitp->hit_vpriv = pp + k1 * dp;
    hitX = hitp->hit_vpriv.x;
    hitY = hitp->hit_vpriv.y;
    /* check if hitpoint is on the top surface */
    if ((hyp->hyp_rx*hitX*hitX + hyp->hyp_ry*hitY*hitY) < hyp->hyp_bounds) {
	hitp->hit_dist = k1;
	hitp->hit_surfno = HYP_NORM_TOP;
	hitp++;
    }

    hitp->hit_vpriv = pp + k2 * dp;
    hitX = hitp->hit_vpriv.x;
    hitY = hitp->hit_vpriv.y;
    /* check if hitpoint is on the bottom surface */
    if ((hyp->hyp_rx*hitX*hitX + hyp->hyp_ry*hitY*hitY) < hyp->hyp_bounds) {
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
	    do_segp(res, idx, &hits[0], &hits[1]);
	} else {
	    /* entry is [1], exit is [0] */
	    do_segp(res, idx, &hits[1], &hits[0]);
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

	/* hit segments are now (0, 1) and (2, 3) */
	do_segp(res, idx, &sorted[0], &sorted[1]);
	do_segp(res, idx, &sorted[2], &sorted[3]);

	return 4;
    }
}

/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void hyp_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct hyp_specific *hyp)
{
    /* normal from basic hyperboloid and transformed normal */
    double3 n, nT;

    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;
    switch (hitp->hit_surfno) {
	case HYP_NORM_TOP:
	    hitp->hit_normal = vload3(0, hyp->hyp_Hunit);
	    break;
	
	case HYP_NORM_BOTTOM:
	    hitp->hit_normal = -vload3(0, hyp->hyp_Hunit);
	    break;
	
	case HYP_NORM_BODY:
	    /* normal vector is VUNITIZE(z * dz/dx, z * dz/dy, -z) */
	    /* z = +- (c/a) * sqrt(x^2/a^2 + y^2/b^2 -1) */
	    n = (double3){hyp->hyp_rx * hitp->hit_vpriv.x, hyp->hyp_ry * hitp->hit_vpriv.y, -hyp->hyp_rz * hitp->hit_vpriv.z};

	    nT.x = (hyp->hyp_Aunit[0] * n.x) + (hyp->hyp_Bunit[0] * n.y) + (hyp->hyp_Hunit[0] * n.z);
	    nT.y = (hyp->hyp_Aunit[1] * n.x) + (hyp->hyp_Bunit[1] * n.y) + (hyp->hyp_Hunit[1] * n.z);
	    nT.z = (hyp->hyp_Aunit[2] * n.x) + (hyp->hyp_Bunit[2] * n.y) + (hyp->hyp_Hunit[2] * n.z);

	    hitp->hit_normal = normalize(nT);
	    break;
	default:
	    break;
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
