/*                         H R T . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2024 United States Government as represented by
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
/** @file primitives/hrt/hrt.c
 *
 * Intersect a ray with a Heart
 *
 * Algorithm:
 *
 * Given V, X, Y, Z and d, there is a set of points on this heart
 *
 * { (x, y, z) | (x, y, z) is on heart defined by V, X, Y, Z and d }
 *
 * Through a series of Transformations, this set will be transformed
 * into a set of points on a heart centered at the origin
 * which lies on the X-Y plane (i.e., Z is on the Z axis).
 *
 * { (x', y', z') | (x', y', z') is on heart at origin }
 *
 * The transformation from X to X' is accomplished by:
 *
 * X' = S(R(X - V))
 *
 * where R(X) = (Z/(|X|))
 *		(Y/(|Y|)) . X
 *		(Z/(|Z|))
 *
 * and S(X) =	(1/|A|   0     0)
 *		(0    1/|A|   0) . X
 *		(0      0   1/|A|)
 * where |A| = d
 *
 * To find the intersection of a line with the heart, consider the
 * parametric line L:
 *
 * L : { P(n) | P + t(n) . D }
 *
 * Call W the actual point of intersection between L and the heart.
 * Let W' be the point of intersection between L' and the heart.
 *
 * L' : { P'(n) | P' + t(n) . D' }
 *
 * W = invR(invS(W')) + V
 *
 * Where W' = k D' + P'.
 *
 * Given a line, find the equation of the
 * heart in terms of the variable 't'.
 *
 * The equation for the heart is:
 *
 *    [ X**2 + 9/4*Y**2 + Z**2 - 1 ]**3 - Z**3 * (X**2 + 9/80 * Y**2)  =  0
 *
 * First, find X, Y, and Z in terms of 't' for this line, then
 * substitute them into the equation above.
 *
 *    Wx = Dx*t + Px
 *
 *    Wx**3 = Dx**3 * t**3 + 3 * Dx**2 * Px * t**2 + 3 * Dx * Px**2 * t + Px**3
 *
 * The real roots of the equation in 't' are the intersect points
 * along the parametric line.
 *
 * NORMALS.  Given the point W on the heart, what is the vector normal
 * to the tangent plane at that point?
 *
 * Map W onto the heart, i.e.: W' = S(R(W - V)).  In this case,
 * we find W' by solving the parametric line given k.
 *
 * The gradient of the heart at W' is in fact the normal vector.
 *
 * Given that the sextic equation for the heart is:
 *
 *    [ X**2 + 9/4 * Y**2 + Z**2 - 1 ]**3 - Z**3 * (X**2 + 9/80 * Y**2)  =  0.
 *
 * let w =  X**2 + 9/4*Y**2 + Z**2 - 1 , then the sextic equation becomes:
 *
 *    w**3 - Y**3 * (X**2 + 9/80 * Y**2)  =  0.
 *
 * For f(x, y, z) = 0, the gradient of f() is (df/dx, df/dy, df/dz).
 *
 *    df/dx = 6 * x * (w**2 - y**3)
 *    df/dy = 6 * (12/27 * w**2 * y**2 - 1/2 * y**2 * (x**2 + 9 / 80*z**3))
 *    df/dz = 6 * (w**2 * z - 160 / 9 * y**3 * z**2)
 *
 * Note that the normal vector produced above will not have
 * length.  Also, to make this useful for the original heart, it will
 * have to be rotated back to the orientation of the original heart.
 *
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
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../../librt_private.h"


const struct bu_structparse rt_hrt_parse[] = {
    { "%f", 3, "V", bu_offsetofarray(struct rt_hrt_internal, v, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "X", bu_offsetofarray(struct rt_hrt_internal, xdir, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "Y", bu_offsetofarray(struct rt_hrt_internal, ydir, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "Z", bu_offsetofarray(struct rt_hrt_internal, zdir, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%g", 1, "d", bu_offsetof(struct rt_hrt_internal, d), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


struct hrt_specific {
    vect_t hrt_V; /* Vector to center of heart */
    vect_t hrt_X; /* unit-length X vector */
    vect_t hrt_Y; /* unit-length Y vector */
    vect_t hrt_Z; /* unit-length Z vector */
    fastf_t hrt_d; /* for distance to upper and lower cusps */
    vect_t hrt_invsq; /* [ 1.0 / |X|**2, 1.0 / |Y|**2, 1.0 / |Z|**2 ] */
    mat_t hrt_SoR; /* Scale(Rot(vect)) */
    mat_t hrt_invRSSR; /* invRot(Scale(Scale(vect))) */
    mat_t hrt_invR; /* transposed rotation matrix */
};

#ifdef USE_OPENCL
/* largest data members first */
struct clt_hrt_specific {
    cl_double hrt_V[3];		/* Vector to center of heart */
    cl_double hrt_SoR[16];	/* Scale(Rot(vect)) */
};

size_t
clt_hrt_pack(struct bu_pool *pool, struct soltab *stp)
{
    struct hrt_specific *hrt =
        (struct hrt_specific *)stp->st_specific;
    struct clt_hrt_specific *args;

    const size_t size = sizeof(*args);
    args = (struct clt_hrt_specific*)bu_pool_alloc(pool, 1, size);

    VMOVE(args->hrt_V, hrt->hrt_V);
    MAT_COPY(args->hrt_SoR, hrt->hrt_SoR);
    return size;
}

#endif /* USE_OPENCL */


/**
 * Compute the bounding RPP for a heart.
 */
int
rt_hrt_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol))
{
    struct rt_hrt_internal *hip;
    fastf_t magsq_x, magsq_y, magsq_z;
    vect_t Xu, Yu, Zu;
    mat_t R;
    fastf_t f;

    hip = (struct rt_hrt_internal *)ip->idb_ptr;
    RT_HRT_CK_MAGIC(hip);

    magsq_x = MAGSQ(hip->xdir);
    magsq_y = MAGSQ(hip->ydir);
    magsq_z = MAGSQ(hip->zdir);

    /* Create unit length versions of X, Y, Z  */
    f = 1.0/sqrt(magsq_x);
    VSCALE(Xu, hip->xdir, f);
    f = 1.0/sqrt(magsq_y);
    VSCALE(Yu, hip->ydir, f);
    f = 1.0/sqrt(magsq_z);
    VSCALE(Zu, hip->zdir, f);

    MAT_IDN(R);
    VMOVE(&R[0], Xu);
    VMOVE(&R[4], Yu);
    VMOVE(&R[8], Zu);

    /**
     * View-points from directly above and besides the heart indicate that it is
     * elliptical, Y-axis is the major axis, X-axis the minor axis and the
     * ratio of the radius of the minor axis to the radius of the major axis
     * is 2/3
     */
    /* X */
    f = hip->xdir[X];
    (*min)[X] = hip->v[X] - f * 2/3;
    (*max)[X] = hip->v[X] + f * 2/3;

    /* Y */
    f = hip->ydir[Y];
    (*min)[Y] = hip->v[Y] - f;
    (*max)[Y] = hip->v[Y] + f;

    /* Z */
    f = hip->zdir[Z];
    /**
     * 1.0 stands for the length of zdir vector and 0.25 closely approximates
     * some value which encloses the displacement from upper cusp to highest
     * point of either lobe in the Z direction
     */
    (*min)[Z] = hip->v[Z] - f;
    (*max)[Z] = hip->v[Z] + f * 1.25;

    return 0;
}


/**
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid heart, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 HRT is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct hrt_specific is created, and its address is stored in
 * stp->st_specific for use by rt_hrt_shot().
 */
int
rt_hrt_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    register struct hrt_specific *hrt;
    struct rt_hrt_internal *hip;
    fastf_t magsq_x, magsq_y, magsq_z;
    mat_t R, TEMP;
    vect_t Xu, Yu, Zu;
    fastf_t f;

    hip = (struct rt_hrt_internal *)ip->idb_ptr;
    RT_HRT_CK_MAGIC(hip);

    /* Validate that |X| > 0, |Y| > 0 and |Z| > 0  */
    magsq_x = MAGSQ(hip->xdir);
    magsq_y = MAGSQ(hip->ydir);
    magsq_z = MAGSQ(hip->zdir);

    if (magsq_x < rtip->rti_tol.dist_sq || magsq_y < rtip->rti_tol.dist_sq || magsq_z < rtip->rti_tol.dist_sq) {
	bu_log("rt_hrt_prep(): hrt(%s) near-zero length X(%g), Y(%g), or Z(%g) vector\n",
	       stp->st_name, magsq_x, magsq_y, magsq_z);
	return 1;
    }
    if (hip->d < rtip->rti_tol.dist) {
	bu_log("rt_hrt_prep(): hrt(%s) near-zero length <d> distance to cusps (%g) causes problems\n", stp->st_name, hip->d);
	return 1;
    }
    if (hip->d > 10000.0) {
	bu_log("rt_hrt_prep(): hrt(%s) very large <d> distance to cusps (%g) causes problems\n", stp->st_name, hip->d);
	/* BAD */
    }

    /* Create unit length versions of X, Y, Z */
    f = 1.0/sqrt(magsq_x);
    VSCALE(Xu, hip->xdir, f);
    f = 1.0/sqrt(magsq_y);
    VSCALE(Yu, hip->ydir, f);
    f = 1.0/sqrt(magsq_z);
    VSCALE(Zu, hip->zdir, f);

    /* Check that X, Y, Z are perpendicular to each other */
    f = VDOT(Xu, Yu);
    if (! NEAR_ZERO(f, rtip->rti_tol.dist)) {
	bu_log("rt_hrt_prep(): hrt(%s) X not perpendicular to Y, f=%f\n", stp->st_name, f);
	return 1;
    }
    f = VDOT(Xu, Zu);
    if (! NEAR_ZERO(f, rtip->rti_tol.dist)) {
	bu_log("rt_hrt_prep(): hrt(%s) X not perpendicular to Z, f=%f\n", stp->st_name, f);
	return 1;
    }
    f = VDOT(Zu, Yu);
    if (! NEAR_ZERO(f, rtip->rti_tol.dist)) {
	bu_log("rt_hrt_prep(): hrt(%s) Z not perpendicular to Y, f=%f\n", stp->st_name, f);
	return 1;
    }

    /* Scale X and Y */
    f = 0.8/sqrt(magsq_x);
    VSCALE(Xu, hip->xdir, f);
    f = 0.8/sqrt(magsq_y);
    VSCALE(Yu, hip->ydir, f);

    /* Solid is OK, compute constant terms now  */

    BU_GET(hrt, struct hrt_specific);
    stp->st_specific = (void *)hrt;

    hrt->hrt_d = hip->d;

    VMOVE(hrt->hrt_V, hip->v);

    VSET(hrt->hrt_invsq, 1.0/magsq_x, 1.0/magsq_y, 1.0/magsq_z);
    VMOVE(hrt->hrt_X, Xu);
    VMOVE(hrt->hrt_Y, Yu);
    VMOVE(hrt->hrt_Z, Zu);

    /* compute the rotation matrix  */
    MAT_IDN(R);
    VMOVE(&R[0], Xu);
    VMOVE(&R[4], Yu);
    VMOVE(&R[8], Zu);
    bn_mat_trn(hrt->hrt_invR, R);

    /* compute invRSSR */
    MAT_IDN(hrt->hrt_invRSSR);
    MAT_IDN(TEMP);
    TEMP[0] = hrt->hrt_invsq[0];
    TEMP[5] = hrt->hrt_invsq[1];
    TEMP[10] = hrt->hrt_invsq[2];
    bn_mat_mul(TEMP, TEMP, R);
    bn_mat_mul(hrt->hrt_invRSSR, hrt->hrt_invR, TEMP);

    /* compute Scale(Rotate(vect)) */
    MAT_IDN(hrt->hrt_SoR);
    VSCALE(&hrt->hrt_SoR[0], hip->xdir, hrt->hrt_invsq[0]);
    VSCALE(&hrt->hrt_SoR[4], hip->ydir, hrt->hrt_invsq[1]);
    VSCALE(&hrt->hrt_SoR[8], hip->zdir, hrt->hrt_invsq[2]);

    /* compute bounding sphere  */
    VMOVE(stp->st_center, hrt->hrt_V);

    /**
     * 1.0 stands for the length of zdir vector and 0.25 closely approximates
     * some value which encloses the displacement from upper cusp to highest
     * point of either lobe in the Z direction
     */
    f = hip->zdir[Z] * 1.25;
    stp->st_aradius = stp->st_bradius = sqrt(f);

    /* compute bounding RPP */
    if (rt_hrt_bbox(ip, &(stp->st_min), &(stp->st_max), &rtip->rti_tol)) return 1;

    return 0;                /* OK */
}


void
rt_hrt_print(register const struct soltab *stp)
{
    register struct hrt_specific *hrt =
	(struct hrt_specific *)stp->st_specific;

    VPRINT("V", hrt->hrt_V);
    bn_mat_print("S o R", hrt->hrt_SoR);
    bn_mat_print("invRSSR", hrt->hrt_invRSSR);
}


/**
 * Intersect a ray with a heart, where all constant terms have been
 * precomputed by rt_hrt_prep().  If an intersection occurs, one or
 * two struct seg(s) will be acquired and filled in.
 *
 * NOTE: All lines in this function are represented parametrically by
 * a point, P(x0, y0, z0) and a direction normal, D = ax + by + cz.
 * Any point on a line can be expressed by one variable 't', where
 *
 * X = a*t + x0,	e.g., X = Dx*t + Px
 * Y = b*t + y0,
 * Z = c*t + z0.
 *
 * ^   ^     ^
 * |   |     |
 *
 * W = D*t + P
 *
 * First, convert the line to the coordinate system of a "standard"
 * heart.  This is a heart which lies in the X-Y plane, circles the
 * origin, and whose distance to cusps is one.
 *
 * Then find the equation of that line and the heart, which
 * turns out (by substituting X, Y, Z above into the sextic equation above)
 * to be a sextic equation S in 't' given below.
 *
 * S(t)=C6*t**6 + C5*t**5 + C4*t**4 + C3*t**3 + C2*t**2 + C1*t + C0 = 0.
 *
 * where C0, C1, C2, C3, C4, C5, C6 are coefficients of the equation.
 *
 * Solve the equation using a general polynomial root finder.
 * Use those values of 't' to compute the points of intersection
 * in the original coordinate system.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_hrt_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    register struct hrt_specific *hrt =
        (struct hrt_specific *)stp->st_specific;
    register struct seg *segp;
    vect_t dprime;              /* D' : The new shot direction */
    vect_t pprime;              /* P' : The new shot point */
    vect_t trans;               /* Translated shot vector */
    vect_t norm_pprime;         /* P' with normalized distance from heart */
    bn_poly_t Xsqr, Ysqr, Zsqr; /* X^2, 9/4*Y^2, Z^2 - 1 */
    bn_poly_t  Acube, S;    /* The sextic equation (of power 6) and A^3 */
    bn_poly_t Zcube, A;     /* Z^3 and  X^2 + 9/4 * Y^2 + Z^2 - 1 */
    bn_poly_t X2_Y2, Z3_X2_Y2;  /* X^2 + 9/80*Y^2 and Z^3*(X^2 + 9/80*Y^2) */
    bn_complex_t complex[6];    /* The complex roots */
    double real[6];             /* The real roots */
    int i;
    int j;

    /* Translate the ray point */
    (trans)[X] = (rp->r_pt)[X] - (hrt->hrt_V)[X];
    (trans)[Y] = (rp->r_pt)[Y] - (hrt->hrt_V)[Y];
    (trans)[Z] = (rp->r_pt)[Z] - (hrt->hrt_V)[Z];

    /* Scale and Rotate point to get P' */
    pprime[X] = (hrt->hrt_SoR[0]*trans[X] + hrt->hrt_SoR[1]*trans[Y] + hrt->hrt_SoR[2]*trans[Z]) * 1.0/(hrt->hrt_SoR[15]);
    pprime[Y] = (hrt->hrt_SoR[4]*trans[X] + hrt->hrt_SoR[5]*trans[Y] + hrt->hrt_SoR[6]*trans[Z]) * 1.0/(hrt->hrt_SoR[15]);
    pprime[Z] = (hrt->hrt_SoR[8]*trans[X] + hrt->hrt_SoR[9]*trans[Y] + hrt->hrt_SoR[10]*trans[Z]) * 1.0/(hrt->hrt_SoR[15]);
    /* Translate ray direction vector */
    MAT4X3VEC(dprime, hrt->hrt_SoR, rp->r_dir);
    VUNITIZE(dprime);

    /* Normalize distance from the heart. Substitutes a corrected ray
     * point, which contains a translation along the ray direction to
     * the closest approach to vertex of the heart.Translating the ray
     * along the direction of the ray to the closest point near the
     * heart's center vertex. Thus, the New ray origin is normalized.
     */
    VSCALE(norm_pprime, dprime, VDOT(pprime, dprime));
    VSUB2(norm_pprime, pprime, norm_pprime);

    /**
     * Generate the sextic equation S(t) = 0 to be passed through the root finder.
     */
    /* X**2 */
    Xsqr.dgr = 2;
    Xsqr.cf[0] = dprime[X] * dprime[X];
    Xsqr.cf[1] = 2 * dprime[X] * pprime[X];
    Xsqr.cf[2] = pprime[X] * pprime[X];

    /* 9/4 * Y**2*/
    Ysqr.dgr = 2;
    Ysqr.cf[0] = 9/4 * dprime[Y] * dprime[Y];
    Ysqr.cf[1] = 9/2 * dprime[Y] * pprime[Y];
    Ysqr.cf[2] = 9/4 * (pprime[Y] * pprime[Y]);

    /* Z**2 - 1 */
    Zsqr.dgr = 2;
    Zsqr.cf[0] = dprime[Z] * dprime[Z];
    Zsqr.cf[1] = 2 * dprime[Z] * pprime[Z];
    Zsqr.cf[2] = pprime[Z] * pprime[Z] - 1.0 ;

    /* A = X^2 + 9/4 * Y^2 + Z^2 - 1 */
    A.dgr = 2;
    A.cf[0] = Xsqr.cf[0] + Ysqr.cf[0] + Zsqr.cf[0];
    A.cf[1] = Xsqr.cf[1] + Ysqr.cf[1] + Zsqr.cf[1];
    A.cf[2] = Xsqr.cf[2] + Ysqr.cf[2] + Zsqr.cf[2];

    /* Z**3 */
    Zcube.dgr = 3;
    Zcube.cf[0] = dprime[Z] * Zsqr.cf[0];
    Zcube.cf[1] = 1.5 * dprime[Z] * Zsqr.cf[1];
    Zcube.cf[2] = 1.5 * pprime[Z] * Zsqr.cf[1];
    Zcube.cf[3] = pprime[Z] * ( Zsqr.cf[2] + 1.0 );

    Acube.dgr = 6;
    Acube.cf[0] = A.cf[0] * A.cf[0] * A.cf[0];
    Acube.cf[1] = 3.0 * A.cf[0] * A.cf[0] * A.cf[1];
    Acube.cf[2] = 3.0 * (A.cf[0] * A.cf[0] * A.cf[2] + A.cf[0] * A.cf[1] * A.cf[1]);
    Acube.cf[3] = 6.0 * A.cf[0] * A.cf[1] * A.cf[2] + A.cf[1] * A.cf[1] * A.cf[1];
    Acube.cf[4] = 3.0 * (A.cf[0] * A.cf[2] * A.cf[2] + A.cf[1] * A.cf[1] * A.cf[2]);
    Acube.cf[5] = 3.0 * A.cf[1] * A.cf[2] * A.cf[2];
    Acube.cf[6] = A.cf[2] * A.cf[2] * A.cf[2];

    /* X**2 + 9/80 Y**2 */
    X2_Y2.dgr = 2;
    X2_Y2.cf[0] = Xsqr.cf[0] + Ysqr.cf[0] / 20 ;
    X2_Y2.cf[1] = Xsqr.cf[1] + Ysqr.cf[1] / 20 ;
    X2_Y2.cf[2] = Xsqr.cf[2] + Ysqr.cf[2] / 20 ;

    /* Z**3 * (X**2 + 9/80 * Y**2) */
    Z3_X2_Y2.dgr = 5;
    Z3_X2_Y2.cf[0] = Zcube.cf[0] * X2_Y2.cf[0];
    Z3_X2_Y2.cf[1] = X2_Y2.cf[0] * Zcube.cf[1];
    Z3_X2_Y2.cf[2] = X2_Y2.cf[0] * Zcube.cf[2] + X2_Y2.cf[1] * Zcube.cf[0] + X2_Y2.cf[1] * Zcube.cf[1] + X2_Y2.cf[2] * Zcube.cf[0];
    Z3_X2_Y2.cf[3] = X2_Y2.cf[0] * Zcube.cf[3] + X2_Y2.cf[1] * Zcube.cf[2] + X2_Y2.cf[2] * Zcube.cf[1];
    Z3_X2_Y2.cf[4] = X2_Y2.cf[1] * Zcube.cf[3] + X2_Y2.cf[2] * Zcube.cf[2];
    Z3_X2_Y2.cf[5] = X2_Y2.cf[2] * Zcube.cf[3];

    S.dgr = 6;
    S.cf[0] = Acube.cf[0];
    S.cf[1] = Acube.cf[1] - Z3_X2_Y2.cf[0];
    S.cf[2] = Acube.cf[2] - Z3_X2_Y2.cf[1];
    S.cf[3] = Acube.cf[3] - Z3_X2_Y2.cf[2];
    S.cf[4] = Acube.cf[4] - Z3_X2_Y2.cf[3];
    S.cf[5] = Acube.cf[5] - Z3_X2_Y2.cf[4];
    S.cf[6] = Acube.cf[6] - Z3_X2_Y2.cf[5];

    /* It is known that the equation is sextic (of order 6). Therefore, if the
     * root finder returns other than 6 roots, return an error.
     */
    if ((i = rt_poly_roots(&S, complex, stp->st_dp->d_namep)) != 6) {
        if (i > 0) {
            bu_log("hrt:  rt_poly_roots() 6!=%d\n", i);
            bn_pr_roots(stp->st_name, complex, i);
        } else if (i < 0) {
            static int reported = 0;
            bu_log("The root solver failed to converge on a solution for %s\n", stp->st_dp->d_namep);
            if (!reported) {
                VPRINT("while shooting from:\t", rp->r_pt);
                VPRINT("while shooting at:\t", rp->r_dir);
                bu_log("Additional heart convergence failure details will be suppressed.\n");
                reported = 1;
            }
        }
        return 0;               /* MISS */
    }

    /* Only real roots indicate an intersection in real space.
     *
     * Look at each root returned; if the imaginary part is zero or
     * sufficiently close, then use the real part as one value of 't'
     * for the intersections
     */
    for (j = 0, i = 0; j < 6; j++) {
        if (NEAR_ZERO(complex[j].im, ap->a_rt_i->rti_tol.dist))
            real[i++] = complex[j].re;
    }
    /* Here, 'i' is number of points found */
    switch (i) {
        case 0:
            return 0;           /* No hit */

        default:
            bu_log("rt_hrt_shot: reduced 6 to %d roots\n", i);
            bn_pr_roots(stp->st_name, complex, 6);
            return 0;           /* No hit */

        case 2:
            {
                /* Sort most distant to least distant. */
                fastf_t u;
                if ((u=real[0]) < real[1]) {
                    /* bubble larger towards [0] */
                    real[0] = real[1];
                    real[1] = u;
                }
            }
            break;
        case 4:
            {
                short n;
                short lim;

                /* Inline rt_pnt_sort().  Sorts real[] into descending order. */
                for (lim = i-1; lim > 0; lim--) {
                    for (n = 0; n < lim; n++) {
                        fastf_t u;
                        if ((u=real[n]) < real[n+1]) {
                            /* bubble larger towards [0] */
                            real[n] = real[n+1];
                            real[n+1] = u;
                        }
                    }
                }
            }
            break;
        case 6:
            {
                short num;
                short limit;

                /* Inline rt_pnt_sort().  Sorts real[] into descending order. */
                for (limit = i-1; limit > 0; limit--) {
                    for (num = 0; num < limit; num++) {
                        fastf_t u;
                        if ((u=real[num]) < real[num+1]) {
                            /* bubble larger towards [0] */
                            real[num] = real[num+1];
                            real[num+1] = u;
                        }
                    }
                }
            }
            break;
    }

    /* Now, t[0] > t[npts-1] */
    /* real[1] is entry point, and real[0] is farthest exit point */
    RT_GET_SEG(segp, ap->a_resource);
    segp->seg_stp = stp;
    segp->seg_in.hit_dist = real[1];
    segp->seg_out.hit_dist = real[0];
    segp->seg_in.hit_surfno = segp->seg_out.hit_surfno = 0;
    /* Set aside vector for rt_hrt_norm() later */
    VJOIN1(segp->seg_in.hit_vpriv, pprime, real[1], dprime);
    VJOIN1(segp->seg_out.hit_vpriv, pprime, real[0], dprime);
    BU_LIST_INSERT(&(seghead->l), &(segp->l));

    if (i == 2)
        return 2;                       /* HIT */

    /* 4 points */
    /* real[3] is entry point, and real[2] is exit point */
    RT_GET_SEG(segp, ap->a_resource);
    segp->seg_stp = stp;
    segp->seg_in.hit_dist = real[3];
    segp->seg_out.hit_dist = real[2];
    segp->seg_in.hit_surfno = segp->seg_out.hit_surfno = 0;
    /* Set aside vector for rt_hrt_norm() later */
    VJOIN1(segp->seg_in.hit_vpriv, pprime, real[3], dprime);
    VJOIN1(segp->seg_out.hit_vpriv, pprime, real[2], dprime);
    BU_LIST_INSERT(&(seghead->l), &(segp->l));

    if (i == 4)
        return 4;                       /* HIT */

    /* 6 points */
    /* real[5] is entry point, and real[4] is exit point */
    RT_GET_SEG(segp, ap->a_resource);
    segp->seg_stp = stp;
    segp->seg_in.hit_dist = real[5];
    segp->seg_out.hit_dist = real[4];
    segp->seg_in.hit_surfno = segp->seg_out.hit_surfno = 0;
    /* Set aside vector for rt_hrt_norm() later */
    VJOIN1(segp->seg_in.hit_vpriv, pprime, real[5], dprime);
    VJOIN1(segp->seg_out.hit_vpriv, pprime, real[4], dprime);
    BU_LIST_INSERT(&(seghead->l), &(segp->l));
    return 6;
}


#define RT_HRT_SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;
/**
 * This is the Becker vector version
 */
void
rt_hrt_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
{
    register struct hrt_specific *hrt;
    vect_t dprime;
    vect_t pprime;
    vect_t work;
    vect_t norm_pprime;
    bn_poly_t Xsqr, Ysqr, Zsqr;
    bn_poly_t A, Acube, Zcube;
    bn_poly_t X2_Y2, Z3_X2_Y2;
    bn_poly_t *S;
    bn_complex_t (*complex)[6];
    int num_roots, num_zero;
    register int i;
    fastf_t *cor_proj;

    /* Allocate space for polynomials and roots */
    S = (bn_poly_t *)bu_malloc(n * sizeof(bn_poly_t) * 6, "hrt bn_complex_t");
    complex = (bn_complex_t (*)[6])bu_malloc(n * sizeof(bn_complex_t) * 6, "hrt bn_complex_t");
    cor_proj = (fastf_t *)bu_malloc(n * sizeof(fastf_t), "hrt_proj");

    /* Initialize seg_stp to assume hit (zero will then flag a miss) */
    for (i = 0; i < n; i++ )
	segp[i].seg_stp = stp[i];

    /* for each ray/heart pair */
    for (i = 0; i < n; i++) {
	if (segp[i].seg_stp == 0)
	    continue;		/* Skip this iteration */

	hrt = (struct hrt_specific *)stp[i]->st_specific;

	/* Translate ray direction vector */
	MAT4X3VEC(dprime, hrt->hrt_SoR, rp[i]->r_dir);
	VUNITIZE(dprime);

	/* Use segp[i].seg_in.hit_normal as tmp to hold dprime */
	VMOVE(segp[i].seg_in.hit_normal, dprime);
	VSUB2(work, rp[i]->r_pt, hrt->hrt_V);
	MAT4X3VEC(pprime, hrt->hrt_SoR, work);

	/* use segp[i].seg_out.hit_normal as tmp to hold pprime */
	VMOVE(segp[i].seg_out.hit_normal, pprime);

	/* Normalize distance from the heart. Substitutes a corrected ray
	 * point, which contains a translation along the ray direction to
	 * the closest approach to vertex of the heart.Translating the ray
	 * along the direction of the ray to the closest point near the
	 * heart's center vertex. Thus, the New ray origin is normalized.
	 */
	cor_proj[i] = VDOT(pprime, dprime);
	VSCALE(norm_pprime, dprime, cor_proj[i]);
	VSUB2(norm_pprime, pprime, norm_pprime);

	/*
	 * Generate sextic equation S(t) = 0 to be passed through the root finder
	 */
	/* X**2 */
	Xsqr.dgr = 2;
	Xsqr.cf[0] = dprime[X] * dprime[X];
        Xsqr.cf[1] = 2 * dprime[X] * pprime[X];
        Xsqr.cf[2] = pprime[X] * pprime[X];

        /* 9/4 * Y**2*/
        Ysqr.dgr = 2;
        Ysqr.cf[0] = 9/4 * dprime[Y] * dprime[Y];
        Ysqr.cf[1] = 9/2 * dprime[Y] * pprime[Y];
        Ysqr.cf[2] = 9/4 * (pprime[Y] * pprime[Y]);

        /* Z**2 - 1 */
        Zsqr.dgr = 2;
        Zsqr.cf[0] = dprime[Z] * dprime[Z];
        Zsqr.cf[1] = 2 * dprime[Z] * pprime[Z];
        Zsqr.cf[2] = pprime[Z] * pprime[Z] - 1.0 ;

        /* A = X^2 + 9/4 * Y^2 + Z^2 - 1 */
        A.dgr = 2;
        A.cf[0] = Xsqr.cf[0] + Ysqr.cf[0] + Zsqr.cf[0];
        A.cf[1] = Xsqr.cf[1] + Ysqr.cf[1] + Zsqr.cf[1];
        A.cf[2] = Xsqr.cf[2] + Ysqr.cf[2] + Zsqr.cf[2];

        /* Z**3 */
        Zcube.dgr = 3;
        Zcube.cf[0] = dprime[Z] * Zsqr.cf[0];
        Zcube.cf[1] = 1.5 * dprime[Z] * Zsqr.cf[1];
        Zcube.cf[2] = 1.5 * pprime[Z] * Zsqr.cf[1];
        Zcube.cf[3] = pprime[Z] * ( Zsqr.cf[2] + 1.0 );

	/* A**3 */
        Acube.dgr = 6;
        Acube.cf[0] = A.cf[0] * A.cf[0] * A.cf[0];
        Acube.cf[1] = 3.0 * A.cf[0] * A.cf[0] * A.cf[1];
        Acube.cf[2] = 3.0 * (A.cf[0] * A.cf[0] * A.cf[2] + A.cf[0] * A.cf[1] * A.cf[1]);
        Acube.cf[3] = 6.0 * A.cf[0] * A.cf[1] * A.cf[2] + A.cf[1] * A.cf[1] * A.cf[1];
        Acube.cf[4] = 3.0 * (A.cf[0] * A.cf[2] * A.cf[2] + A.cf[1] * A.cf[1] * A.cf[2]);
        Acube.cf[5] = 3.0 * A.cf[1] * A.cf[2] * A.cf[2];
        Acube.cf[6] = A.cf[2] * A.cf[2] * A.cf[2];

        /* X**2 + 9/80 Y**2 */
        X2_Y2.dgr = 2;
        X2_Y2.cf[0] = Xsqr.cf[0] + Ysqr.cf[0] / 20 ;
        X2_Y2.cf[1] = Xsqr.cf[1] + Ysqr.cf[1] / 20 ;
        X2_Y2.cf[2] = Xsqr.cf[2] + Ysqr.cf[2] / 20 ;

        /* Z**3 * (X**2 + 9/80 * Y**2) */
        Z3_X2_Y2.dgr = 5;
        Z3_X2_Y2.cf[0] = Zcube.cf[0] * X2_Y2.cf[0];
        Z3_X2_Y2.cf[1] = X2_Y2.cf[0] * Zcube.cf[1];
        Z3_X2_Y2.cf[2] = X2_Y2.cf[0] * Zcube.cf[2] + X2_Y2.cf[1] * Zcube.cf[0] + X2_Y2.cf[1] * Zcube.cf[1] + X2_Y2.cf[2] * Zcube.cf[0];
        Z3_X2_Y2.cf[3] = X2_Y2.cf[0] * Zcube.cf[3] + X2_Y2.cf[1] * Zcube.cf[2] + X2_Y2.cf[2] * Zcube.cf[1];
        Z3_X2_Y2.cf[4] = X2_Y2.cf[1] * Zcube.cf[3] + X2_Y2.cf[2] * Zcube.cf[2];
        Z3_X2_Y2.cf[5] = X2_Y2.cf[2] * Zcube.cf[3];

	/* S(t) = 0 */
        S[i].dgr = 6;
        S[i].cf[0] = Acube.cf[0];
        S[i].cf[1] = Acube.cf[1] - Z3_X2_Y2.cf[0];
        S[i].cf[2] = Acube.cf[2] - Z3_X2_Y2.cf[1];
        S[i].cf[3] = Acube.cf[3] - Z3_X2_Y2.cf[2];
        S[i].cf[4] = Acube.cf[4] - Z3_X2_Y2.cf[3];
        S[i].cf[5] = Acube.cf[5] - Z3_X2_Y2.cf[4];
        S[i].cf[6] = Acube.cf[6] - Z3_X2_Y2.cf[5];

    }
    for (i = 0; i < n; i++) {
	continue;		/* Skip this iteration */
    /*
     * It is known that the equation is sextic (of order 6).
     * Therefore, if the root finder returns other than six roots, Error
     */
    if ((num_roots = rt_poly_roots(&(S[i]), &(complex[i][0]), (*stp)->st_dp->d_namep)) != 6) {
	if (num_roots > 0) {
	    bu_log("hrt: rt_poly_roots() 6 != %d\n", num_roots);
	    bn_pr_roots("hrt", complex[i], num_roots);
	} else if (num_roots < 0) {
	    static int reported = 0;
	    bu_log("The root solver failed to converge on a solution for %s\n", stp[i]->st_dp->d_namep);
	    if (!reported) {
		VPRINT("while shooting from: \t", rp[i]->r_pt);
		VPRINT("while shooting at:\t", rp[i]->r_dir);
		bu_log("Additional heart convergence failure details will be suppressed.\n");
		reported = 1;
	    }
	}
	RT_HRT_SEG_MISS(segp[i]);
    }

    /* for each ray/heart pair */
    for (i = 0; i < n; i++) {
	if (segp[i].seg_stp == 0)
	    continue;		/* Skip current iteration */

    /* Only real roots indicate an intersection in real space.
     *
     * Look at each root returned; if the imaginary part is zero or
     * sufficiently close, then use the real part as one value of 't'
     * for the intersections
     *
     * Reverse translation by adding distance to all 'real' values
     * Reuse S to hold 'real' values
     */
    num_zero = 0;
    if (NEAR_ZERO(complex[i][0].im, ap->a_rt_i->rti_tol.dist)) {
	S[i].cf[num_zero++] = complex[i][0].re - cor_proj[i];
    }
    if (NEAR_ZERO(complex[i][1].im, ap->a_rt_i->rti_tol.dist)) {
	S[i].cf[num_zero++] = complex[i][1].re - cor_proj[i];
    }
    if (NEAR_ZERO(complex[i][2].im, ap->a_rt_i->rti_tol.dist)) {
	S[i].cf[num_zero++] = complex[i][2].re - cor_proj[i];
    }
    if (NEAR_ZERO(complex[i][3].im, ap->a_rt_i->rti_tol.dist)) {
	S[i].cf[num_zero++] = complex[i][3].re - cor_proj[i];
    }
    if (NEAR_ZERO(complex[i][4].im, ap->a_rt_i->rti_tol.dist)) {
	S[i].cf[num_zero++] = complex[i][4].re - cor_proj[i];
    }
    if (NEAR_ZERO(complex[i][5].im, ap->a_rt_i->rti_tol.dist)) {
	S[i].cf[num_zero++] = complex[i][5].re - cor_proj[i];
    }
    S[i].dgr = num_zero;

    /* Here 'i' is the number of points found */
    if (num_zero == 0) {
	RT_HRT_SEG_MISS(segp[i]);		/* MISS */
    } else if ((num_zero != 2 && num_zero != 4 ) && (num_zero != 6)) {
		RT_HRT_SEG_MISS(segp[i]);	/* MISS */
	    }
    }

    /* Process each one segment hit */
    for (i = 0; i < n; i++) {
	if (segp[i].seg_stp == 0)
	    continue;				/* Skip This Iteration */
	if (S[i].dgr != 2)
	    continue;				/* Not one segment */

	hrt = (struct hrt_specific *)stp[i]->st_specific;

	/* segp[i].seg_in.hit_normal holds dprime */
	/* segp[i].seg_out.hit_normal holds pprime */
	if (S[i].cf[1] < S[i].cf[0]) {
	    /* S[i].cf[1] is entry point */
	    segp[i].seg_in.hit_dist = S[i].cf[1];
	    segp[i].seg_in.hit_dist = S[i].cf[0];
	    /* Set aside vector for rt_hrt_norm() later */
	    VJOIN1(segp[i].seg_in.hit_vpriv, segp[i].seg_out.hit_normal, S[i].cf[1], segp[i].seg_in.hit_normal);
	    VJOIN1(segp[i].seg_in.hit_vpriv, segp[i].seg_out.hit_normal, S[i].cf[0], segp[i].seg_in.hit_normal);
	    } else {
		/* S[i].cf[0] is entry point */
		segp[i].seg_in.hit_dist = S[i].cf[0];
		segp[i].seg_in.hit_dist = S[i].cf[1];
		/* Set aside vector for rt_hrt_norm() later */
		VJOIN1(segp[i].seg_in.hit_vpriv, segp[i].seg_out.hit_normal, S[i].cf[0], segp[i].seg_in.hit_normal);
		VJOIN1(segp[i].seg_in.hit_vpriv, segp[i].seg_out.hit_normal, S[i].cf[1], segp[i].seg_in.hit_normal);
	    }
    }

    /* Process each two segment hit */
    for (i = 0; i < n; i++) {
	if (segp[i].seg_stp == 0)
	    continue;				/* Skip This Iteration */
	if (S[i].dgr != 4)
	    continue;				/* Not one segment */

	hrt = (struct hrt_specific *)stp[i]->st_specific;

	/* Sort most distant to least distant */
	rt_pnt_sort(S[i].cf, 4);
	/* Now, t[0] > t[npts - 1] */
	/* segp[i].seg_in.hit_normal holds dprime */
	VMOVE(dprime, segp[i].seg_out.hit_normal);
	/* segp[i].seg_out.hit_normal holds pprime */
	VMOVE(pprime, segp[i].seg_out.hit_normal);

	/* S[i].cf[1] is entry point */
	segp[i].seg_in.hit_dist = S[i].cf[3];
	segp[i].seg_in.hit_dist = S[i].cf[2];
	/* Set aside vector for rt_hrt_norm() later */
	VJOIN1(segp[i].seg_in.hit_vpriv, segp[i].seg_out.hit_normal, S[i].cf[1], segp[i].seg_in.hit_normal);
	VJOIN1(segp[i].seg_in.hit_vpriv, segp[i].seg_out.hit_normal, S[i].cf[0], segp[i].seg_in.hit_normal);
    }

    /* Process each three segment hit */
    for (i = 0; i < n; i++) {
	if (segp[i].seg_stp == 0)
	    continue;				/* Skip This Iteration */
	if (S[i].dgr != 6)
	    continue;				/* Not one segment */

	hrt = (struct hrt_specific *)stp[i]->st_specific;

	/* Sort most distant to least distant */
	rt_pnt_sort(S[i].cf, 6);
	/* Now, t[0] > t[npts - 1] */
	/* segp[i].seg_in.hit_normal holds dprime */
	VMOVE(dprime, segp[i].seg_out.hit_normal);
	/* segp[i].seg_out.hit_normal holds pprime */
	VMOVE(pprime, segp[i].seg_out.hit_normal);

	/* S[i].cf[1] is entry point */
	segp[i].seg_in.hit_dist = S[i].cf[5];
	segp[i].seg_in.hit_dist = S[i].cf[4];
	/* Set aside vector for rt_hrt_norm() later */
	VJOIN1(segp[i].seg_in.hit_vpriv, pprime, S[i].cf[5], dprime);
	VJOIN1(segp[i].seg_in.hit_vpriv, pprime, S[i].cf[4], dprime);
    }

    }

    /* Free tmp space used */
    bu_free((char *)S, "hrt S");
    bu_free((char *)complex, "hrt complex");
    bu_free((char *)cor_proj, "hrt_cor_proj");

}


/**
 * Compute the normal to the heart, given a point on the heart
 * centered at the origin on the X-Y plane.  The gradient of the heart
 * at that point is in fact the normal vector, which will have to be
 * given unit length.  To make this useful for the original heart, it
 * will have to be rotated back to the orientation of the original
 * heart.
 *
 * Given that the equation for the heart is:
 *
 * [ X**2 + 9/4 * Y**2 + Z**2 - 1 ]**3 - Z**3 * (X**2 + 9/80*Y**2) = 0.
 *
 * let w = X**2 + 9/4 * Y**2 + Z**2 - 1, then the equation becomes:
 *
 * w**3 - Z**3 * (X**2 + 9/80 * Y**2)  =  0.
 *
 * For f(x, y, z) = 0, the gradient of f() is (df/dx, df/dy, df/dz).
 *
 * df/dx = 6 * X * (w**2 - Z**3/3)
 * df/dy = 6 * Y * (12/27 * w**2 - 80/3 * Z**3)
 * df/dz = 6 * Z * ( w**2 - 1/2 * Z * (X**2 + 9/80 * Y**2))
 *
 * Since we rescale the gradient (normal) to unity, we divide the
 * above equations by six here.
 */
void
rt_hrt_norm(struct hit *hitp, struct soltab *UNUSED(stp), struct xray *rp)
{

    fastf_t w, fx, fy, fz;
    vect_t work;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    w = hitp->hit_vpriv[X] * hitp->hit_vpriv[X]
        + 9.0/4.0 * hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y]
        + hitp->hit_vpriv[Z] * hitp->hit_vpriv[Z] - 1.0;
    fx = (w * w - 1/3 * hitp->hit_vpriv[Z] * hitp->hit_vpriv[Z] * hitp->hit_vpriv[Z]) * hitp->hit_vpriv[X];
    fy = hitp->hit_vpriv[Y] * (12/27 * w * w - 80/3 * hitp->hit_vpriv[Z] * hitp->hit_vpriv[Z] * hitp->hit_vpriv[Z]);
    fz = (w * w - 0.5 * hitp->hit_vpriv[Z] * (hitp->hit_vpriv[X] * hitp->hit_vpriv[X] + 9/80 * hitp->hit_vpriv[Y] * hitp->hit_vpriv[Y])) * hitp->hit_vpriv[Z];
    VSET(work, fx, fy, fz);
    VMOVE(hitp->hit_normal, work);
}


void
rt_hrt_free(struct soltab *stp)
{
    struct hrt_specific *hrt =
	(struct hrt_specific *)stp->st_specific;

    BU_PUT(hrt, struct hrt_specific);
}


/**
 * Similar to code used by the ELL
 */

#define HRTOUT(n) ov+(n-1)*3
void
rt_hrt_24pts(fastf_t *ov, fastf_t *V, fastf_t *A, fastf_t *B)
{
    static fastf_t c, d, e, f, g, h, i, j, k, l;

    c = k = 0.95105;		/* cos(18.0 degrees) */
    d = j = 0.80901;		/* cos(36.0 degrees) */
    e = i = 0.58778;		/* cos(54.0 degrees) */
    f = h = 0.30901;            /* cos(72.0 degrees) */
    g = 0.00000;		/* cos(90.0 degrees) */
    l = 1.00000;		/* sin(90.0 degrees) */

    /*
     * For angle theta, compute surface point as
     *
     * V + cos(theta) * A + sin(theta) * B
     *
     * note that sin(theta) is cos(90-theta); arguments in degrees.
     */

    VADD2(HRTOUT(1), V, A);
    VJOIN2(HRTOUT(2), V, c, A, h, B);
    VJOIN2(HRTOUT(3), V, d, A, i, B);
    VJOIN2(HRTOUT(4), V, e, A, j, B);
    VJOIN2(HRTOUT(5), V, f, A, k, B);
    VJOIN2(HRTOUT(6), V, g, A, l, B);
    VADD2(HRTOUT(7), V, B);
    VJOIN2(HRTOUT(8), V, -g, A, l, B);
    VJOIN2(HRTOUT(9), V, -f, A, k, B);
    VJOIN2(HRTOUT(10), V, -e, A, j, B);
    VJOIN2(HRTOUT(11), V, -d, A, i, B);
    VJOIN2(HRTOUT(12), V, -c, A, h, B);
    VSUB2(HRTOUT(13), V, A);
    VJOIN2(HRTOUT(14), V, -c, A, -h, B);
    VJOIN2(HRTOUT(15), V, -d, A, -i, B);
    VJOIN2(HRTOUT(16), V, -e, A, -j, B);
    VJOIN2(HRTOUT(17), V, -f, A, -k, B);
    VJOIN2(HRTOUT(18), V, -g, A, -l, B);
    VSUB2(HRTOUT(19), V, B);
    VJOIN2(HRTOUT(20), V, g, A, -l, B);
    VJOIN2(HRTOUT(21), V, f, A, -k, B);
    VJOIN2(HRTOUT(22), V, e, A, -j, B);
    VJOIN2(HRTOUT(23), V, d, A, -i, B);
    VJOIN2(HRTOUT(24), V, c, A, -h, B);
}


int
rt_hrt_plot(struct bu_list *vhead, struct rt_db_internal *ip,const struct bg_tess_tol *ttol, const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    fastf_t c, dtol, mag_h, ntol = M_PI, r1, r2, **ellipses, theta_prev, theta_new;
    int *pts_dbl;
    int nseg; /* The number of line segments in a particular ellipse */
    int j, k, jj, na, nb;
    int nell; /* The number of ellipses in the parabola */
    int recalc_b, i, ellipse_below, ellipse_above;
    mat_t R;
    mat_t invR;
    point_t p1;
    struct rt_pnt_node *pos_a, *pos_b, *pts_a, *pts_b;
    vect_t A, Au, B, Bu, Cu;
    vect_t V, Work;
    vect_t lower_cusp, upper_cusp, upper_cusp_xdir;
    vect_t highest_point_left, highest_point_right, top_xdir, top_ydir;
    vect_t top01_center, top01_xdir, top01_ydir;
    vect_t top1_center, top1_xdir, top1_ydir;
    vect_t top02_center, top02_xdir, top02_ydir;
    vect_t v1_left, xdir1_left, ydir1_left, top1_center_left, top1_center_right, v1_right, xdir1_right, ydir1_right;
    vect_t v2_left, xdir2_left, ydir2_left, top2_center_left, top2_center_right, v2_right, xdir2_right, ydir2_right;
    vect_t v3_left, xdir3_left, ydir3_left, top3_center_left, top3_center_right, v3_right, xdir3_right, ydir3_right;
    vect_t v4_left, xdir4_left, ydir4_left, top4_center_left, top4_center_right, v4_right, xdir4_right, ydir4_right;

    struct rt_hrt_internal *hip;

    fastf_t top[24*3];
    fastf_t top01[24*3];
    fastf_t top02[24*3];
    fastf_t top1[24*3];
    fastf_t top1_left_lobe[24*3];
    fastf_t top1_right_lobe[24*3];
    fastf_t top2_left_lobe[24*3];
    fastf_t top2_right_lobe[24*3];
    fastf_t top3_left_lobe[24*3];
    fastf_t top3_right_lobe[24*3];
    fastf_t top4_left_lobe[24*3];
    fastf_t top4_right_lobe[24*3];
    fastf_t top5_upper_cusp[24*3];

    fastf_t angle_c = 0.64656;  /* sin(40.2) */
    fastf_t angle_d = 0.80901;  /* sin(54.0) */
    fastf_t angle_e = 0.90350;  /* sin(64.5) */
    fastf_t angle_f = 0.95000;  /* sin(71.8) */
    fastf_t angle_g = 0.33990;  /* sin(19.8) */
    fastf_t angle_h = 0.48357;  /* sin(29.0) */

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    hip = (struct rt_hrt_internal *)ip->idb_ptr;
    RT_HRT_CK_MAGIC(hip);

    VSET(top01_center, 0 , 0 , hip->zdir[Z] * angle_g );
    VSET(top02_center, 0 , 0 , hip->zdir[Z] * angle_h );
    VSET(top1_center_left, -0.77 * hip->xdir[X], 0, hip->zdir[Z]);
    VSET(top1_center_right, hip->xdir[X] * 0.77, 0, hip->zdir[Z]);
    VSET(top1_center, 0, 0, hip->zdir[Z] * 0.64);
    VSET(top2_center_left, -1.00 * hip->xdir[X] * 0.63, 0, hip->zdir[Z]);
    VSET(top2_center_right, hip->xdir[X] * 0.63, 0, hip->zdir[Z]);
    VSET(top3_center_left, -1.00 * hip->xdir[X] * 0.55, 0, hip->zdir[Z]);
    VSET(top3_center_right, hip->xdir[X] * 0.55, 0, hip->zdir[Z]);
    VSET(top4_center_left, -1.00 * hip->xdir[X] * 0.38, 0, hip->zdir[Z] * 0.77 );
    VSET(top4_center_right, hip->xdir[X] * 0.38, 0, hip->zdir[Z] * 0.77 );

    VSCALE(v1_left, top1_center_left, angle_c);
    VSCALE(v1_right, top1_center_right, angle_c);
    VSCALE(v2_left, top2_center_left, angle_d );
    VSCALE(v2_right, top2_center_right, angle_d );
    VSCALE(v3_left, top3_center_left, angle_e );
    VSCALE(v3_right, top3_center_right, angle_e );
    VSCALE(v4_left, top4_center_left, angle_f );
    VSCALE(v4_right, top4_center_right, angle_f);

    VSET(upper_cusp, (top3_center_left[X] + top3_center_right[X]) * 0.70,
		     (top3_center_left[Y] + top3_center_right[Y]) * 0.50,
    		     hip->zdir[Z] * 0.64);
    VSET(highest_point_left, hip->xdir[X] * -0.50 , 0 , hip->zdir[Z] );
    VSET(highest_point_right, hip->xdir[X] * 0.50 , 0 , hip->zdir[Z] );

    VSCALE(top_xdir, hip->xdir, 0.80);
    VSET(top01_xdir, hip->xdir[X] * 0.99, 0, 0);
    VSET(top02_xdir, hip->xdir[X] * 1.05, 0, 0);
    VSET(top1_xdir, hip->xdir[X] * 1.01, 0, 0);
    VSET(xdir1_left, v4_left[Z] * 0.68, 0, 0 );
    VSET(xdir1_right, v4_right[Z] * 0.68, 0, 0 );
    VSET(xdir2_left, v3_left[Z] * 0.45, 0, 0 );
    VSET(xdir2_right, v3_right[Z] * 0.45, 0, 0 );
    VSET(xdir3_left, v2_left[Z] * 0.40, 0, 0 );
    VSET(xdir3_right, v2_right[Z] * 0.40, 0, 0 );
    VSET(xdir4_left, v1_left[Z] * 0.01, 0, 0 );
    VSET(xdir4_right, v1_right[Z] * 0.01, 0, 0 );

    VSCALE(top_ydir, hip->ydir, 0.50);
    VSET(top01_ydir, 0, hip->ydir[Y] * 0.61, 0 );
    VSET(top02_ydir, 0, hip->ydir[Y] * 0.61, 0 );
    VSET(top1_ydir, 0, hip->ydir[Y] * 0.53, 0 );
    VSET(ydir1_left, 0, v4_left[Z] * 0.71, 0 );
    VSET(ydir1_right, 0, v4_right[Z] * 0.71, 0 );
    VSET(ydir2_left, 0, v3_left[Z] * 0.45, 0 );
    VSET(ydir2_right, 0, v3_right[Z] * 0.45, 0 );
    VSET(ydir3_left, 0, v2_left[Z] * 0.27, 0 );
    VSET(ydir3_right, 0, v2_right[Z] * 0.27, 0 );
    VSET(ydir4_left, 0, v1_left[Z] * 0.01, 0 );
    VSET(ydir4_right, 0, v1_right[Z] * 0.01, 0 );
    VSET(upper_cusp_xdir, 0 , v3_left[Z] * 0.01 , 0 );

    rt_hrt_24pts(top, hip->v, top_xdir, top_ydir);
    rt_hrt_24pts(top01, top01_center, top01_xdir, top01_ydir );
    rt_hrt_24pts(top02, top02_center, top02_xdir, top02_ydir );
    rt_hrt_24pts(top1, top1_center, top1_xdir, top1_ydir );
    rt_hrt_24pts(top1_left_lobe, v1_left, xdir1_left, ydir1_left);
    rt_hrt_24pts(top1_right_lobe, v1_right, xdir1_right, ydir1_right);
    rt_hrt_24pts(top2_left_lobe, v2_left, xdir2_left, ydir2_left);
    rt_hrt_24pts(top2_right_lobe, v2_right, xdir2_right, ydir2_right);
    rt_hrt_24pts(top3_left_lobe, v3_left, xdir3_left, ydir3_left);
    rt_hrt_24pts(top3_right_lobe, v3_right, xdir3_right, ydir3_right);
    rt_hrt_24pts(top4_left_lobe, highest_point_left, xdir4_left, ydir4_left);
    rt_hrt_24pts(top4_right_lobe, highest_point_right, xdir4_right, ydir4_right);
    rt_hrt_24pts(top5_upper_cusp, upper_cusp, upper_cusp_xdir, upper_cusp_xdir);

    BV_ADD_VLIST(vlfree, vhead, &top[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &top01[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top01[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &top02[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top02[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &top1[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top1[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &top1_left_lobe[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top1_left_lobe[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &top1_right_lobe[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top1_right_lobe[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &top2_left_lobe[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top2_left_lobe[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &top2_right_lobe[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top2_right_lobe[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &top3_left_lobe[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top3_left_lobe[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &top3_right_lobe[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top3_right_lobe[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &top4_left_lobe[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top4_left_lobe[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &top4_right_lobe[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top4_right_lobe[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &top5_upper_cusp[23*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i = 0; i < 24; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top5_upper_cusp[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    /*
      Make connections between ellipses
      This for repetition statement draws connectors between
      the nell-1 ellipses drawn above
     */

    for (k = 1; k < 24; k++) {
	BV_ADD_VLIST(vlfree, vhead,
	             &top01[k*ELEMENTS_PER_VECT],
	             BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead,
		     &top02[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_DRAW);
        BV_ADD_VLIST(vlfree, vhead,
		     &top02[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead,
		     &top1[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead,
		     &top1_left_lobe[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead,
	             &top2_left_lobe[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead,
	             &top1_right_lobe[k*ELEMENTS_PER_VECT],
	             BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead,
	             &top2_right_lobe[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead,
		     &top2_left_lobe[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead,
		     &top3_left_lobe[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead,
		     &top2_right_lobe[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead,
	             &top3_right_lobe[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead,
	             &top3_left_lobe[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead,
		     &top4_left_lobe[k*ELEMENTS_PER_VECT],
	             BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead,
	             &top3_right_lobe[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead,
		     &top4_right_lobe[k*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_DRAW);
    }

    mag_h = MAGNITUDE(hip->zdir);
    r1 = MAGNITUDE(hip->xdir) * 0.80;
    r2 = MAGNITUDE(hip->ydir) * 0.61;
    c = hip->d;

    /* make unit vectors in A, B, and AxB directions */
    VMOVE(Cu, hip->zdir);
    VUNITIZE(Cu);
    VMOVE(Au, hip->xdir);
    VUNITIZE(Au);
    VMOVE(Bu, hip->ydir);
    VUNITIZE(Bu);

    /* Compute R and Rinv matrices */
    MAT_IDN(R);
    VREVERSE(&R[0], Bu);
    VMOVE(&R[4], Au);
    VREVERSE(&R[8], Cu);
    bn_mat_trn(invR, R);			/* inv of rot mat is trn */

    dtol = primitive_get_absolute_tolerance(ttol, r2 * 2.00);

    /*
     * build ehy from 2 hyperbolas
     */

    /* approximate positive half of hyperbola along semi-minor axis */
    BU_ALLOC(pts_b, struct rt_pnt_node);
    BU_ALLOC(pts_b->next, struct rt_pnt_node);

    pts_b->next->next = NULL;
    VSET(pts_b->p,       0, 0, 0);
    VSET(pts_b->next->p, 0, 0, mag_h/3);
    /* 2 endpoints in 1st approximation */
    nb = 2;
    /* recursively break segment 'til within error tolerances */
    nb += rt_mk_hyperbola(pts_b, mag_h/3, mag_h, c, dtol, ntol);
    nell = nb - 1;	/* Number of ellipses needed */

    /*
     * construct positive half of hyperbola along semi-major axis of
     * ehy using same z coords as hyperbola along semi-minor axis
     */

    BU_ALLOC(pts_a, struct rt_pnt_node);
    VMOVE(pts_a->p, pts_b->p);	/* 1st pt is the apex */
    pts_a->next = NULL;
    pos_b = pts_b->next;
    pos_a = pts_a;
    while (pos_b) {
	/* copy node from b_hyperbola to a_hyperbola */
	BU_ALLOC(pos_a->next, struct rt_pnt_node);
	pos_a = pos_a->next;
	pos_a->p[Z] = pos_b->p[Z];
	/* at given z, find y on hyperbola */
	pos_a->p[Y] = r1
		    * sqrt(((pos_b->p[Z] + mag_h + c)
		    * (pos_b->p[Z] + mag_h + c) - c*c)
		    / (mag_h*(mag_h + 2*c)));
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
    /* if any segments were broken, recalculate hyperbola 'a' */
    if (recalc_b) {
	/* free mem for old approximation of hyperbola */
	pos_b = pts_b;
	while (pos_b) {
	    struct rt_pnt_node *next;

	    /* get next node before freeing */
	    next = pos_b->next;
	    bu_free((char *)pos_b, "rt_pnt_node");
	    pos_b = next;
    }

    /* construct hyperbola along semi-major axis of ehy using same
     * z coords as parabola along semi-minor axis
     */
    BU_ALLOC(pts_b, struct rt_pnt_node);
    pts_b->p[Z] = pts_a->p[Z];
    pts_b->next = NULL;
    pos_a = pts_a->next;
    pos_b = pts_b;
    while (pos_a) {
	/* copy node from a_hyperbola to b_hyperbola */
	BU_ALLOC(pos_b->next, struct rt_pnt_node);
	pos_b = pos_b->next;
	pos_b->p[Z] = pos_a->p[Z];
	/* at given z, find y on hyperbola */
	pos_b->p[Y] = r2 * sqrt(((pos_a->p[Z] + mag_h + c)
			 * (pos_a->p[Z] + mag_h + c) - c*c)
			 / (mag_h*(mag_h + 2*c)));
	pos_b->p[X] = 0;
	pos_a = pos_a->next;
	}
	pos_b->next = NULL;
    }

    /* make array of ptrs to ehy ellipses */
    ellipses = (fastf_t **)bu_malloc(nell * sizeof(fastf_t *), "fastf_t ell[]");
    /* keep track of whether pts in each ellipse are doubled or not */
    pts_dbl = (int *)bu_malloc(nell * sizeof(int), "dbl ints");

    /* make ellipses at different levels in the +Z direction */
    i = 0;
    nseg = 0;
    theta_prev = M_2PI;
    pos_a = pts_a->next;	/* skip over lower cusp of heart ( at pts_a ) */
    pos_b = pts_b->next;
    while (pos_a) {
	VSCALE(A, Au, pos_a->p[Y]);		/* semimajor axis */
	VSCALE(B, Bu, pos_b->p[Y] * 0.80);	/* semiminor axis */
	VJOIN1(V, hip->v, pos_a->p[Z], Cu);

	VSET(p1, 0.00, pos_b->p[Y], 0.00);
	theta_new = ell_angle(p1, pos_a->p[Y], pos_b->p[Y], dtol, ntol);
	if (nseg == 0) {
	    nseg = (int)(M_2PI / theta_new) + 1;
	    pts_dbl[i] = 0;
	} else if (theta_new < theta_prev) {
	    nseg *= 2;
	    pts_dbl[i] = 1;
	} else
	    pts_dbl[i] = 0;
	theta_prev = theta_new;

	ellipses[i] = (fastf_t *)bu_malloc(3*(nseg+1)*sizeof(fastf_t),"pts ell");
	rt_ell(ellipses[i], V, A, B, nseg);

	i++;
	pos_a = pos_a->next;
	pos_b = pos_b->next;
    }

    /* Draw the top (largest) ellipse in the XY plane */
    BV_ADD_VLIST(vlfree, vhead,
		 &ellipses[nell-1][(nseg-1)*ELEMENTS_PER_VECT],
		 BV_VLIST_LINE_MOVE);
    for (i = 0; i < nseg; i++) {
	BV_ADD_VLIST(vlfree, vhead,
		     &ellipses[nell-1][i*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_DRAW);
    }

    /* Connect ellipses skipping the top (largest) ellipse which is at index nell - 1 */
    for (i = nell-2; i >= 0; i--) {
	ellipse_above = i + 1;
	ellipse_below = i;
	if (pts_dbl[ellipse_above])
	    nseg /= 2;	/* Number of line segments in 'ellipse_below' ellipse is halved if points double */

	/* Draw the current ellipse */
	BV_ADD_VLIST(vlfree, vhead,
		     &ellipses[ellipse_below][(nseg-1)*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_MOVE);
	for (j = 0; j < nseg; j++) {
	    BV_ADD_VLIST(vlfree, vhead,
			 &ellipses[ellipse_below][j*ELEMENTS_PER_VECT],
			 BV_VLIST_LINE_DRAW);
	}

    /*
     * Make connections between ellipses
     * This for repetition statement draws connectors between
     * the nell-1 ellipses drawn above
     */

    for (j = 1; j < nseg; j++) {
	if (pts_dbl[ellipse_above])
	    jj = j + j;
	else
	    jj = j;
	BV_ADD_VLIST(vlfree, vhead,
		     &ellipses[ellipse_below][j*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead,
		     &ellipses[ellipse_above][jj*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_DRAW);
	}
    }

    VSET(lower_cusp,       0, 0, hip->zdir[Z] * -1.00);
    VADD2(Work, hip->v, lower_cusp);
    for (i = 0; i < nseg; i++) {
	/* Draw connector */
	BV_ADD_VLIST(vlfree, vhead, Work, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead,
		     &ellipses[0][i*ELEMENTS_PER_VECT],
		     BV_VLIST_LINE_DRAW);
    }

    /* free memory */
    for (i = 0; i < nell; i++) {
	bu_free((char *)ellipses[i], "pts ell");
    }
    bu_free((char *)ellipses, "fastf_t ell[]");
    bu_free((char *)pts_dbl, "dbl ints");

    return 0;
}


/**
 * The external form is:
 * V point
 * Xdir vector
 * Ydir vector
 * Zdir vector
 */
int
rt_hrt_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_hrt_internal *hip;

    /* must be double for import and export */
    double hec[ELEMENTS_PER_VECT*4 + 1];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_HRT) return -1;
    hip = (struct rt_hrt_internal *)ip->idb_ptr;
    RT_HRT_CK_MAGIC(hip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * (ELEMENTS_PER_VECT*4 + 1);
    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "hrt external");

    /* scale values to local buffer */
    VSCALE(&hec[0*ELEMENTS_PER_VECT], hip->v, local2mm);
    VSCALE(&hec[1*ELEMENTS_PER_VECT], hip->xdir, local2mm);
    VSCALE(&hec[2*ELEMENTS_PER_VECT], hip->ydir, local2mm);
    VSCALE(&hec[3*ELEMENTS_PER_VECT], hip->zdir, local2mm);
    hec[4*ELEMENTS_PER_VECT] = hip->d;

    /* Convert from internal (host) to database (network) format */
    bu_cv_htond(ep->ext_buf, (unsigned char *)hec, ELEMENTS_PER_VECT*4 + 1);

    return 0;
}


/**
 * Import a heart from the database format to the internal format.
 *
 */
int
rt_hrt_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_hrt_internal *hip;

    /* must be double for import and export */
    double hec[ELEMENTS_PER_VECT*4 + 1];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(ep);

    BU_ASSERT(ep->ext_nbytes == SIZEOF_NETWORK_DOUBLE * (ELEMENTS_PER_VECT*4 + 1));

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_HRT;
    ip->idb_meth = &OBJ[ID_HRT];
    BU_ALLOC(ip->idb_ptr, struct rt_hrt_internal);

    hip = (struct rt_hrt_internal *)ip->idb_ptr;
    hip->hrt_magic = RT_HRT_INTERNAL_MAGIC;

    /* Convert from database(network) to internal (host) format */
    bu_cv_ntohd((unsigned char *)hec, ep->ext_buf, ELEMENTS_PER_VECT*4 + 1);

    /* Apply modelling transformations */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(hip->v, mat, &hec[0*ELEMENTS_PER_VECT]);
    MAT4X3PNT(hip->xdir, mat, &hec[1*ELEMENTS_PER_VECT]);
    MAT4X3PNT(hip->ydir, mat, &hec[2*ELEMENTS_PER_VECT]);
    MAT4X3PNT(hip->zdir, mat, &hec[3*ELEMENTS_PER_VECT]);
    hip->d = hec[4*ELEMENTS_PER_VECT];

    return 0;        /* OK */
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_hrt_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_hrt_internal *hip =
	(struct rt_hrt_internal *)ip->idb_ptr;
    fastf_t mag_x, mag_y, mag_z;
    char buf[256];
    double angles[5];
    vect_t unit_vector;

    RT_HRT_CK_MAGIC(hip);
    bu_vls_strcat(str, "Heart (HRT)\n");

    sprintf(buf, "\tV (%g, %g, %g)\n",
	INTCLAMP(hip->v[X] * mm2local),
	INTCLAMP(hip->v[Y] * mm2local),
	INTCLAMP(hip->v[Z] * mm2local));
    bu_vls_strcat(str, buf);

    mag_x = MAGNITUDE(hip->xdir);
    mag_y = MAGNITUDE(hip->ydir);
    mag_z = MAGNITUDE(hip->zdir);

    sprintf(buf, "\tXdir (%g, %g, %g) mag=%g\n",
	INTCLAMP(hip->xdir[X] * mm2local),
	INTCLAMP(hip->xdir[Y] * mm2local),
	INTCLAMP(hip->xdir[Z] * mm2local),
	INTCLAMP(mag_x * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tYdir (%g, %g, %g) mag=%g\n",
	INTCLAMP(hip->ydir[X] * mm2local),
	INTCLAMP(hip->ydir[Y] * mm2local),
	INTCLAMP(hip->ydir[Z] * mm2local),
	INTCLAMP(mag_y * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tZdir (%g, %g, %g) mag=%g\n",
	INTCLAMP(hip->zdir[X] * mm2local),
	INTCLAMP(hip->zdir[Y] * mm2local),
	INTCLAMP(hip->zdir[Z] * mm2local),
	INTCLAMP(mag_z * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\td=%g\n", INTCLAMP(hip->d));
    bu_vls_strcat(str, buf);

    if (!verbose) return 0;

    VSCALE(unit_vector, hip->xdir, 1/mag_x);
    rt_find_fallback_angle(angles, unit_vector);
    rt_pr_fallback_angle(str, "\tXdir", angles);

    VSCALE(unit_vector, hip->ydir, 1/mag_y);
    rt_find_fallback_angle(angles, unit_vector);
    rt_pr_fallback_angle(str, "\tYdir", angles);

    VSCALE(unit_vector, hip->zdir, 1/mag_z);
    rt_find_fallback_angle(angles, unit_vector);
    rt_pr_fallback_angle(str, "\tZdir", angles);

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_hrt_ifree(struct rt_db_internal *ip)
{
    register struct rt_hrt_internal *hip;

    RT_CK_DB_INTERNAL(ip);

    hip = (struct rt_hrt_internal *)ip->idb_ptr;
    RT_HRT_CK_MAGIC(hip);

    bu_free((char *)hip, "rt_hrt_");
    ip->idb_ptr = ((void *)0);
}


int
rt_hrt_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;                /* OK */
}


void
rt_hrt_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    fastf_t area_hrt_YZ_plane;
    struct rt_hrt_internal *hip = (struct rt_hrt_internal *)ip->idb_ptr;
    RT_HRT_CK_MAGIC(hip);

    /* Area of ellipse in YZ-plane is PI * ydir[Y] *  ( zdir[Z] * 1.125 ) */
    area_hrt_YZ_plane = M_PI * hip->ydir[Y] * hip->zdir[Z] * 1.125;

    /* Area of heart = 180 * M_PI * Area of ellipse in YZ-plane
     * The 180 * M_PI scalar comes from http://mathworld.wolfram.com/HeartCurve.html
     */
    *area = 180 * M_PI * area_hrt_YZ_plane;
}


/**
 * Computes centroid of a heart
 */
void
rt_hrt_centroid(point_t *cent, const struct rt_db_internal *ip)
{
    struct rt_hrt_internal *hip = (struct rt_hrt_internal *)ip->idb_ptr;
    RT_HRT_CK_MAGIC(hip);
    VSET(*cent, hip->xdir[X], hip->ydir[Y], hip->zdir[Z] * 0.125);
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
