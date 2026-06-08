/*                      S U P E R E L L . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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
/** @file primitives/superell/superell.c
 *
 * Intersect a ray with a Superquadratic Ellipsoid.
 *
 * NOTICE: This primitive will exhibit several instabilities in the
 * existing root solver with certain <n,e> value pairings.
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

#if defined(HAVE_TGAMMA) && !defined(HAVE_DECL_TGAMMA)
extern double tgamma(double x);
#endif

const struct bu_structparse rt_superell_parse[] = {
    { "%f", 3, "V", bu_offsetofarray(struct rt_superell_internal, v, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "A", bu_offsetofarray(struct rt_superell_internal, a, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "B", bu_offsetofarray(struct rt_superell_internal, b, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "C", bu_offsetofarray(struct rt_superell_internal, c, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%g", 1, "n", bu_offsetof(struct rt_superell_internal, n), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%g", 1, "e", bu_offsetof(struct rt_superell_internal, e), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/*
 * Algorithm:
 *
 * Given V, A, B, and C, there is a set of points on this superellipsoid
 *
 * { (x, y, z) | (x, y, z) is on superellipsoid defined by V, A, B, C }
 *
 * Through a series of Affine Transformations, this set will be
 * transformed into a set of points on a unit sphere at the origin
 *
 * { (x', y', z') | (x', y', z') is on Sphere at origin }
 *
 * The transformation from X to X' is accomplished by:
 *
 * X' = S(R(X - V))
 *
 * where R(X) = (A/(|A|))
 * 		(B/(|B|)) . X
 * 		(C/(|C|))
 *
 * and S(X) =	(1/|A|   0     0)
 * 		(0     1/|B|   0) . X
 * 		(0       0   1/|C|)
 *
 * To find the intersection of a line with the superellipsoid,
 * consider the parametric line L:
 *
 * 	L : { P(n) | P + t(n) . D }
 *
 * Call W the actual point of intersection between L and the
 * superellipsoid.  Let W' be the point of intersection between L' and
 * the unit sphere.
 *
 * 	L' : { P'(n) | P' + t(n) . D' }
 *
 * W = invR(invS(W')) + V
 *
 * Where W' = k D' + P'.
 *
 * Let dp = D' dot P'
 * Let dd = D' dot D'
 * Let pp = P' dot P'
 *
 * and k = [ -dp +/- sqrt(dp*dp - dd * (pp - 1)) ] / dd
 * which is constant.
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
 * NORMALS.  Given the point W on the superellipsoid, what is the
 * vector normal to the tangent plane at that point?
 *
 * Map W onto the unit sphere, i.e.:  W' = S(R(W - V)).
 *
 * Plane on unit sphere at W' has a normal vector of the same value(!).
 * N' = W'
 *
 * The plane transforms back to the tangent plane at W, and this new
 * plane (on the superellipsoid) has a normal vector of N, viz:
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
 *   = invR o S (W')
 *   = invR(S(S(R(W - V))))
 *
 * because inverse(R) = transpose(R), so R = transpose(invR),
 * and S = transpose(S).
 *
 * Note that the normal vector N produced above will not have unit
 * length.
 */

struct superell_specific {
    vect_t superell_V; /* Vector to center of superellipsoid */
    vect_t superell_Au; /* unit-length A vector */
    vect_t superell_Bu;
    vect_t superell_Cu;
    double superell_n; /* north-south curvature power */
    double superell_e; /* east-west curvature power */
    double superell_invmsAu; /* 1.0 / |Au|^2 */
    double superell_invmsBu; /* 1.0 / |Bu|^2 */
    double superell_invmsCu; /* 1.0 / |Cu|^2 */
    vect_t superell_invsq;
    mat_t superell_SoR; /* matrix for local coordinate system, Scale(Rotate(V))*/
    mat_t superell_invRSSR; /* invR(Scale(Scale(Rot(V)))) */
    mat_t superell_invR; /* transposed rotation matrix */
    double superell_invn; /* 2.0 / n */
    double superell_inve; /* 2.0 / e */
};
#define SUPERELL_NULL ((struct superell_specific *)0)

#ifdef USE_OPENCL
/* largest data members first */
struct clt_superell_specific {
    cl_double superell_V[3]; /* Vector to center of superellipsoid */
    cl_double superell_invmsAu; /* 1.0 / |Au|^2 */
    cl_double superell_invmsBu; /* 1.0 / |Bu|^2 */
    cl_double superell_invmsCu; /* 1.0 / |Cu|^2 */
    cl_double superell_SoR[16]; /* matrix for local coordinate system, Scale(Rotate(V))*/
    cl_double superell_invRSSR[16]; /* invR(Scale(Scale(Rot(V)))) */
    cl_double superell_invR[16]; /* transposed rotation matrix */
    cl_double superell_e;
    cl_double superell_n;
    cl_double superell_inve;
    cl_double superell_invn;
};

size_t
clt_superell_pack(struct bu_pool *pool, struct soltab *stp)
{
    struct superell_specific *superell =
	(struct superell_specific *)stp->st_specific;
    struct clt_superell_specific *args;

    const size_t size = sizeof(*args);
    args = (struct clt_superell_specific*)bu_pool_alloc(pool, 1, size);

    VMOVE(args->superell_V, superell->superell_V);
    MAT_COPY(args->superell_SoR, superell->superell_SoR);
    MAT_COPY(args->superell_invRSSR, superell->superell_invRSSR);
    MAT_COPY(args->superell_invR, superell->superell_invR);
    args->superell_invmsAu = superell->superell_invmsAu;
    args->superell_invmsBu = superell->superell_invmsBu;
    args->superell_invmsCu = superell->superell_invmsCu;
    args->superell_e = superell->superell_e;
    args->superell_n = superell->superell_n;
    args->superell_inve = superell->superell_inve;
    args->superell_invn = superell->superell_invn;
    return size;
}
#endif /* USE_OPENCL */

/**
 * Calculate a bounding RPP for a superell
 */
int
rt_superell_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol)) {

    struct rt_superell_internal *eip;
    fastf_t magsq_a, magsq_b, magsq_c;
    vect_t Au, Bu, Cu;
    mat_t R;
    vect_t w1, w2, P;	/* used for bounding RPP */
    fastf_t f;

    eip = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(eip);

    magsq_a = MAGSQ(eip->a);
    magsq_b = MAGSQ(eip->b);
    magsq_c = MAGSQ(eip->c);


    /* Create unit length versions of A, B, C */
    f = 1.0/sqrt(magsq_a);
    VSCALE(Au, eip->a, f);
    f = 1.0/sqrt(magsq_b);
    VSCALE(Bu, eip->b, f);
    f = 1.0/sqrt(magsq_c);
    VSCALE(Cu, eip->c, f);

    MAT_IDN(R);
    VMOVE(&R[0], Au);
    VMOVE(&R[4], Bu);
    VMOVE(&R[8], Cu);

    /* Compute bounding RPP */
    VSET(w1, magsq_a, magsq_b, magsq_c);

    /* X */
    VSET(P, 1.0, 0, 0);		/* bounding plane normal */
    MAT3X3VEC(w2, R, P);		/* map plane to local coord syst */
    VELMUL(w2, w2, w2);		/* square each term */
    f = VDOT(w1, w2);
    f = sqrt(f);
    (*min)[X] = eip->v[X] - f;	/* V.P +/- f */
    (*max)[X] = eip->v[X] + f;

    /* Y */
    VSET(P, 0, 1.0, 0);		/* bounding plane normal */
    MAT3X3VEC(w2, R, P);		/* map plane to local coord syst */
    VELMUL(w2, w2, w2);		/* square each term */
    f = VDOT(w1, w2);
    f = sqrt(f);
    (*min)[Y] = eip->v[Y] - f;	/* V.P +/- f */
    (*max)[Y] = eip->v[Y] + f;

    /* Z */
    VSET(P, 0, 0, 1.0);		/* bounding plane normal */
    MAT3X3VEC(w2, R, P);		/* map plane to local coord syst */
    VELMUL(w2, w2, w2);		/* square each term */
    f = VDOT(w1, w2);
    f = sqrt(f);
    (*min)[Z] = eip->v[Z] - f;	/* V.P +/- f */
    (*max)[Z] = eip->v[Z] + f;

    return 0;
}


/**
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid superellipsoid, and if so,
 * precompute various terms of the formula.
 *
 * Returns -
 * 0 SUPERELL is OK
 * !0 Error in description
 *
 * Implicit return - A struct superell_specific is created, and its
 * address is stored in stp->st_specific for use by rt_superell_shot()
 */
int
rt_superell_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{

    struct superell_specific *superell;
    struct rt_superell_internal *eip;
    fastf_t magsq_a, magsq_b, magsq_c;
    mat_t R, TEMP;
    vect_t Au, Bu, Cu;	/* A, B, C with unit length */
    fastf_t f;

    eip = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(eip);

    /* Validate that |A| > 0, |B| > 0, |C| > 0 */
    magsq_a = MAGSQ(eip->a);
    magsq_b = MAGSQ(eip->b);
    magsq_c = MAGSQ(eip->c);

    if (magsq_a < rtip->rti_tol.dist_sq || magsq_b < rtip->rti_tol.dist_sq || magsq_c < rtip->rti_tol.dist_sq) {
	bu_log("rt_superell_prep():  superell(%s) near-zero length A(%g), B(%g), or C(%g) vector\n",
	       stp->st_name, magsq_a, magsq_b, magsq_c);
	return 1;		/* BAD */
    }
    if (eip->n < rtip->rti_tol.dist || eip->e < rtip->rti_tol.dist) {
	bu_log("rt_superell_prep():  superell(%s) near-zero length <n, e> curvature (%g, %g) causes problems\n",
	       stp->st_name, eip->n, eip->e);
	/* BAD */
    }
    if (eip->n > 10000.0 || eip->e > 10000.0) {
	bu_log("rt_superell_prep():  superell(%s) very large <n, e> curvature (%g, %g) causes problems\n",
	       stp->st_name, eip->n, eip->e);
	/* BAD */
    }

    /* Create unit length versions of A, B, C */
    f = 1.0/sqrt(magsq_a);
    VSCALE(Au, eip->a, f);
    f = 1.0/sqrt(magsq_b);
    VSCALE(Bu, eip->b, f);
    f = 1.0/sqrt(magsq_c);
    VSCALE(Cu, eip->c, f);

    /* Validate that A.B == 0, B.C == 0, A.C == 0 (check dir only) */
    f = VDOT(Au, Bu);
    if (! NEAR_ZERO(f, rtip->rti_tol.dist)) {
	bu_log("rt_superell_prep():  superell(%s) A not perpendicular to B, f=%f\n", stp->st_name, f);
	return 1;		/* BAD */
    }
    f = VDOT(Bu, Cu);
    if (! NEAR_ZERO(f, rtip->rti_tol.dist)) {
	bu_log("rt_superell_prep():  superell(%s) B not perpendicular to C, f=%f\n", stp->st_name, f);
	return 1;		/* BAD */
    }
    f = VDOT(Au, Cu);
    if (! NEAR_ZERO(f, rtip->rti_tol.dist)) {
	bu_log("rt_superell_prep():  superell(%s) A not perpendicular to C, f=%f\n", stp->st_name, f);
	return 1;		/* BAD */
    }

    /* Solid is OK, compute constant terms now */

    BU_GET(superell, struct superell_specific);
    stp->st_specific = (void *)superell;

    superell->superell_n = eip->n;
    superell->superell_e = eip->e;
    superell->superell_invn = 2.0 / eip->n;
    superell->superell_inve = 2.0 / eip->e;

    VMOVE(superell->superell_V, eip->v);

    VSET(superell->superell_invsq, 1.0/magsq_a, 1.0/magsq_b, 1.0/magsq_c);
    VMOVE(superell->superell_Au, Au);
    VMOVE(superell->superell_Bu, Bu);
    VMOVE(superell->superell_Cu, Cu);

    /* compute the inverse magnitude square for equations during shot */
    superell->superell_invmsAu = 1.0 / magsq_a;
    superell->superell_invmsBu = 1.0 / magsq_b;
    superell->superell_invmsCu = 1.0 / magsq_c;

    /* compute the rotation matrix */
    MAT_IDN(R);
    VMOVE(&R[0], Au);
    VMOVE(&R[4], Bu);
    VMOVE(&R[8], Cu);
    bn_mat_trn(superell->superell_invR, R);

    /* computer invRSSR */
    MAT_IDN(superell->superell_invRSSR);
    MAT_IDN(TEMP);
    TEMP[0] = superell->superell_invsq[0];
    TEMP[5] = superell->superell_invsq[1];
    TEMP[10] = superell->superell_invsq[2];
    bn_mat_mul(TEMP, TEMP, R);
    bn_mat_mul(superell->superell_invRSSR, superell->superell_invR, TEMP);

    /* compute Scale(Rotate(vect)) */
    MAT_IDN(superell->superell_SoR);
    VSCALE(&superell->superell_SoR[0], eip->a, superell->superell_invsq[0]);
    VSCALE(&superell->superell_SoR[4], eip->b, superell->superell_invsq[1]);
    VSCALE(&superell->superell_SoR[8], eip->c, superell->superell_invsq[2]);

    /* Compute bounding sphere */
    VMOVE(stp->st_center, eip->v);
    f = magsq_a;
    if (magsq_b > f)
	f = magsq_b;
    if (magsq_c > f)
	f = magsq_c;
    stp->st_aradius = stp->st_bradius = sqrt(f);

    /* Compute bounding RPP */
    if (rt_superell_bbox(ip, &(stp->st_min), &(stp->st_max), &rtip->rti_tol)) return 1;

    return 0;			/* OK */
}


void
rt_superell_print(const struct soltab *stp)
{
    struct superell_specific *superell =
	(struct superell_specific *)stp->st_specific;

    VPRINT("V", superell->superell_V);
}


/* Equation of a superellipsoid:
 *
 * f(x) = [ (x^(2/e2) + y^(2/e2))^(e2/e1) + z^(2/e1) ]^(e1/2) - 1
 */

/**
 * Intersect a ray with an superellipsoid, where all constant terms
 * have been precomputed by rt_superell_prep().  If an intersection
 * occurs, a struct seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
static double
superell_eval(const struct superell_specific *superell, const vect_t P, const vect_t D, double t)
{
    double x = P[X] + t * D[X];
    double y = P[Y] + t * D[Y];
    double z = P[Z] + t * D[Z];

    double ax = sqrt(x * x + 1e-12);
    double ay = sqrt(y * y + 1e-12);
    double az = sqrt(z * z + 1e-12);

    double term_xy = pow(ax, superell->superell_inve) + pow(ay, superell->superell_inve);
    return pow(term_xy, superell->superell_e / superell->superell_n) + pow(az, superell->superell_invn) - 1.0;
}

static void
superell_grad(vect_t grad, const struct superell_specific *superell, const vect_t W)
{
    double ax = sqrt(W[X] * W[X] + 1e-12);
    double ay = sqrt(W[Y] * W[Y] + 1e-12);
    double az = sqrt(W[Z] * W[Z] + 1e-12);

    double inve = superell->superell_inve;
    double invn = superell->superell_invn;
    double e_over_n = superell->superell_e / superell->superell_n;

    double term_xy = pow(ax, inve) + pow(ay, inve);
    if (term_xy < 1e-12) term_xy = 1e-12;

    double df_dx = invn * pow(term_xy, e_over_n - 1.0) * pow(ax, inve - 1.0) * (W[X] / ax);
    double df_dy = invn * pow(term_xy, e_over_n - 1.0) * pow(ay, inve - 1.0) * (W[Y] / ay);
    double df_dz = invn * pow(az, invn - 1.0) * (W[Z] / az);

    VSET(grad, df_dx, df_dy, df_dz);
}

static double
superell_refine_root(const struct superell_specific *superell, const vect_t P, const vect_t D, double t0, double t1, double f0)
{
    double t = 0.5 * (t0 + t1);
    int iter;
    for (iter = 0; iter < 20; iter++) {
        double f = superell_eval(superell, P, D, t);
        if (fabs(f) < 1e-7) break;
        
        if (f * f0 < 0) {
            t1 = t;
        } else {
            t0 = t;
            f0 = f;
        }
        
        vect_t W, grad;
        VJOIN1(W, P, t, D);
        superell_grad(grad, superell, W);
        double df_dt = VDOT(grad, D);
        
        double t_next = t - f / df_dt;
        if (t_next > t0 && t_next < t1) {
            t = t_next;
        } else {
            t = 0.5 * (t0 + t1);
        }
    }
    return t;
}

int
rt_superell_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct superell_specific *superell = (struct superell_specific *)stp->st_specific;
    vect_t translated;
    vect_t P; /* P' */
    vect_t D; /* D' */
    double dp, dd, pp, disc;
    double t_near, t_far;
    int i, num_roots = 0;
    struct seg *segp;
    bn_poly_t equation;
    bn_complex_t complexRoot[4];
    double realRoot[6];
    int poly_success = 0;

    /* translate ray point */
    VSUB2(translated, rp->r_pt, superell->superell_V);

    /* scale and rotate point to get P' */
    MAT4X3VEC(P, superell->superell_SoR, translated);

    /* translate ray direction vector (unnormalized in local space) */
    MAT4X3VEC(D, superell->superell_SoR, rp->r_dir);

    dp = VDOT(D, P);
    dd = VDOT(D, D);
    pp = VDOT(P, P);

    /* Bounding sphere intersection */
    disc = dp * dp - dd * (pp - 3.0);
    if (disc < 0) return 0; /* Misses bounding sphere entirely */
    
    disc = sqrt(disc);
    t_near = (-dp - disc) / dd;
    t_far  = (-dp + disc) / dd;

    if (t_far < 0.0) return 0;
    if (t_near < 0.0) t_near = 0.0;

    /* Try closed-form polynomial solvers for integer cases */
    if (NEAR_EQUAL(superell->superell_n, 1.0, 1e-5) && NEAR_EQUAL(superell->superell_e, 1.0, 1e-5)) {
        equation.dgr = 2;
        equation.cf[2] = pp - 1.0;
        equation.cf[1] = 2.0 * dp;
        equation.cf[0] = dd;
        if (rt_poly_roots(&equation, complexRoot, stp->st_dp->d_namep) == 2) poly_success = 1;
    } else if (NEAR_EQUAL(superell->superell_n, 0.5, 1e-5) && NEAR_EQUAL(superell->superell_e, 0.5, 1e-5)) {
        equation.dgr = 4;
        equation.cf[4] = P[X]*P[X]*P[X]*P[X] + P[Y]*P[Y]*P[Y]*P[Y] + P[Z]*P[Z]*P[Z]*P[Z] - 1.0;
        equation.cf[3] = 4.0 * (P[X]*P[X]*P[X]*D[X] + P[Y]*P[Y]*P[Y]*D[Y] + P[Z]*P[Z]*P[Z]*D[Z]);
        equation.cf[2] = 6.0 * (P[X]*P[X]*D[X]*D[X] + P[Y]*P[Y]*D[Y]*D[Y] + P[Z]*P[Z]*D[Z]*D[Z]);
        equation.cf[1] = 4.0 * (P[X]*D[X]*D[X]*D[X] + P[Y]*D[Y]*D[Y]*D[Y] + P[Z]*D[Z]*D[Z]*D[Z]);
        equation.cf[0] = D[X]*D[X]*D[X]*D[X] + D[Y]*D[Y]*D[Y]*D[Y] + D[Z]*D[Z]*D[Z]*D[Z];
        if (rt_poly_roots(&equation, complexRoot, stp->st_dp->d_namep) == 4) poly_success = 1;
    } else if (NEAR_EQUAL(superell->superell_n, 0.5, 1e-5) && NEAR_EQUAL(superell->superell_e, 1.0, 1e-5)) {
        double Pxy = P[X]*P[X] + P[Y]*P[Y];
        double Dxy = D[X]*D[X] + D[Y]*D[Y];
        double PDxy = P[X]*D[X] + P[Y]*D[Y];
        equation.dgr = 4;
        equation.cf[4] = Pxy*Pxy + P[Z]*P[Z]*P[Z]*P[Z] - 1.0;
        equation.cf[3] = 4.0 * Pxy * PDxy + 4.0 * P[Z]*P[Z]*P[Z]*D[Z];
        equation.cf[2] = 2.0 * Pxy * Dxy + 4.0 * PDxy * PDxy + 6.0 * P[Z]*P[Z]*D[Z]*D[Z];
        equation.cf[1] = 4.0 * PDxy * Dxy + 4.0 * P[Z]*D[Z]*D[Z]*D[Z];
        equation.cf[0] = Dxy*Dxy + D[Z]*D[Z]*D[Z]*D[Z];
        if (rt_poly_roots(&equation, complexRoot, stp->st_dp->d_namep) == 4) poly_success = 1;
    }

    if (poly_success) {
        for (i = 0; i < (int)equation.dgr; i++) {
            if (NEAR_ZERO(complexRoot[i].im, 0.001)) {
                realRoot[num_roots++] = complexRoot[i].re;
            }
        }
    } else {
        /* Numerical Solver */
        int max_steps = 100;
        double step = (t_far - t_near) / max_steps;
        double t = t_near;
        double f_prev = superell_eval(superell, P, D, t);
        
        for (i = 0; i < max_steps; i++) {
            double t_next = t + step;
            double f_next = superell_eval(superell, P, D, t_next);
            
            if (f_prev * f_next <= 0.0) {
                double root = superell_refine_root(superell, P, D, t, t_next, f_prev);
                if (num_roots < 6) {
                    realRoot[num_roots++] = root;
                }
            }
            t = t_next;
            f_prev = f_next;
        }
    }

    if (num_roots < 2) return 0;

    /* Sort roots */
    for (i = 0; i < num_roots - 1; i++) {
        for (int j = 0; j < num_roots - i - 1; j++) {
            if (realRoot[j] > realRoot[j+1]) {
                double tmp = realRoot[j];
                realRoot[j] = realRoot[j+1];
                realRoot[j+1] = tmp;
            }
        }
    }

    /* Remove duplicates */
    int num_unique = 1;
    for (i = 1; i < num_roots; i++) {
        if (!NEAR_EQUAL(realRoot[i], realRoot[num_unique-1], 1e-4)) {
            realRoot[num_unique++] = realRoot[i];
        }
    }
    num_roots = num_unique;

    /* Output segs */
    for (i = 0; i + 1 < num_roots; i += 2) {
        RT_GET_SEG(segp, ap->a_resource);
        segp->seg_stp = stp;
        segp->seg_in.hit_dist = realRoot[i];
        segp->seg_out.hit_dist = realRoot[i+1];
        segp->seg_in.hit_surfno = segp->seg_out.hit_surfno = 0;
        BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }

    return num_roots;
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_superell_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    struct superell_specific *superell =
	(struct superell_specific *)stp->st_specific;

    vect_t xlated, P, grad;
    fastf_t scale;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    VSUB2(xlated, hitp->hit_point, superell->superell_V);
    
    /* Transform to local scaled coordinates */
    MAT4X3VEC(P, superell->superell_SoR, xlated);
    
    /* Compute local gradient */
    superell_grad(grad, superell, P);
    
    /* Transform gradient back to world space */
    MAT4X3VEC(hitp->hit_normal, superell->superell_invR, grad);
    
    scale = 1.0 / MAGNITUDE(hitp->hit_normal);
    VSCALE(hitp->hit_normal, hitp->hit_normal, scale);

    return;
}


/**
 * Return the curvature of the superellipsoid.
 */
void
rt_superell_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (!cvp || !hitp || !stp)
	return;
    RT_CK_HIT(hitp);
    RT_CK_SOLTAB(stp);

    bu_log("called rt_superell_curve()\n");
    return;
}


/**
 * For a hit on the surface of an SUPERELL, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 * u = azimuth
 * v = elevation
 */
void
rt_superell_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (!stp || !hitp || !uvp)
	return;
    RT_CK_SOLTAB(stp);
    RT_CK_HIT(hitp);

    bu_log("called rt_superell_uv()\n");
    return;
}


void
rt_superell_free(struct soltab *stp)
{
    struct superell_specific *superell =
	(struct superell_specific *)stp->st_specific;

    BU_PUT(superell, struct superell_specific);
}


/**
 * Also used by the TGC code
 */
#define SUPERELLOUT(n) ov+(n-1)*3
void
rt_superell_16pts(fastf_t *ov,
		  fastf_t *V,
		  fastf_t *A,
		  fastf_t *B)
{
    static fastf_t c, d, e, f, g, h;

    e = h = .92388;		/* cos(22.5 degrees) */
    c = d = M_SQRT1_2;		/* cos(45 degrees) */
    g = f = .382683;		/* cos(67.5 degrees) */

    /*
     * For angle theta, compute surface point as
     *
     * V + cos(theta) * A + sin(theta) * B
     *
     * note that sin(theta) is cos(90-theta); arguments in degrees.
     */

    VADD2(SUPERELLOUT(1), V, A);
    VJOIN2(SUPERELLOUT(2), V, e, A, f, B);
    VJOIN2(SUPERELLOUT(3), V, c, A, d, B);
    VJOIN2(SUPERELLOUT(4), V, g, A, h, B);
    VADD2(SUPERELLOUT(5), V, B);
    VJOIN2(SUPERELLOUT(6), V, -g, A, h, B);
    VJOIN2(SUPERELLOUT(7), V, -c, A, d, B);
    VJOIN2(SUPERELLOUT(8), V, -e, A, f, B);
    VSUB2(SUPERELLOUT(9), V, A);
    VJOIN2(SUPERELLOUT(10), V, -e, A, -f, B);
    VJOIN2(SUPERELLOUT(11), V, -c, A, -d, B);
    VJOIN2(SUPERELLOUT(12), V, -g, A, -h, B);
    VSUB2(SUPERELLOUT(13), V, B);
    VJOIN2(SUPERELLOUT(14), V, g, A, -h, B);
    VJOIN2(SUPERELLOUT(15), V, c, A, -d, B);
    VJOIN2(SUPERELLOUT(16), V, e, A, -f, B);
}


int
rt_superell_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    int i;
    struct rt_superell_internal *eip;
    fastf_t top[16*3];
    fastf_t middle[16*3];
    fastf_t bottom[16*3];

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    struct bu_list *vlfree = &rt_vlfree;
    eip = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(eip);

    rt_superell_16pts(top, eip->v, eip->a, eip->b);
    rt_superell_16pts(bottom, eip->v, eip->b, eip->c);
    rt_superell_16pts(middle, eip->v, eip->a, eip->c);

    BV_ADD_VLIST(vlfree, vhead, &top[15*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i=0; i<16; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &bottom[15*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i=0; i<16; i++) {
	BV_ADD_VLIST(vlfree, vhead, &bottom[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    BV_ADD_VLIST(vlfree, vhead, &middle[15*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i=0; i<16; i++) {
	BV_ADD_VLIST(vlfree, vhead, &middle[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }
    return 0;
}


struct superell_state {
    struct shell *s;
    struct rt_superell_internal *eip;
    mat_t invRinvS;
    mat_t invRoS;
    fastf_t theta_tol;
};


struct superell_vert_strip {
    int nverts_per_strip;
    int nverts;
    struct vertex **vp;
    vect_t *norms;
    int nfaces;
    struct faceuse **fu;
};


/**
 * Tesssuperellate an superellipsoid.
 *
 * The strategy is based upon the approach of Jon Leech 3/24/89, from
 * program "sphere", which generates a polygon mesh approximating a
 * sphere by recursive subdivision. First approximation is an
 * octahedron; each level of refinement increases the number of
 * polygons by a factor of 4.  Level 3 (128 polygons) is a good
 * tradeoff if gouraud shading is used to render the database.
 *
 * At the start, points ABC lie on surface of the unit sphere.  Pick
 * DEF as the midpoints of the three edges of ABC.  Normalize the new
 * points to lie on surface of the unit sphere.
 *
 *	  1
 *	  B
 *	 /\
 *   3  /  \  4
 *   D /____\ E
 *    /\    /\
 *   /	\  /  \
 *  /____\/____\
 * A      F     C
 * 0      5     2
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tesssuperellation.
 */
int
rt_superell_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
{
    if (r) NMG_CK_REGION(*r);
    if (m) NMG_CK_MODEL(m);
    if (ip) RT_CK_DB_INTERNAL(ip);

    bu_log("called rt_superell_tess()\n");
    return -1;
}


/**
 * Import an superellipsoid/sphere from the database format to the
 * internal structure.  Apply modeling transformations as wsuperell.
 */
int
rt_superell_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_superell_internal *eip;
    union record *rp;
    fastf_t vec[3*4 + 2];

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != ID_SOLID) {
	bu_log("rt_superell_import4():  defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_SUPERELL;
    ip->idb_meth = &OBJ[ID_SUPERELL];
    BU_ALLOC(ip->idb_ptr, struct rt_superell_internal);

    eip = (struct rt_superell_internal *)ip->idb_ptr;
    eip->magic = RT_SUPERELL_INTERNAL_MAGIC;

    /* Convert from database to internal format */
    flip_fastf_float(vec, rp->s.s_values, 4, (dbip && dbip->i->dbi_version < 0) ? 1 : 0);

    /* Apply modeling transformations */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(eip->v, mat, &vec[0*3]);
    MAT4X3VEC(eip->a, mat, &vec[1*3]);
    MAT4X3VEC(eip->b, mat, &vec[2*3]);
    MAT4X3VEC(eip->c, mat, &vec[3*3]);

    if (dbip && dbip->i->dbi_version < 0) {
	eip->n = flip_dbfloat(rp->s.s_values[12]);
	eip->e = flip_dbfloat(rp->s.s_values[13]);
    } else {
	eip->n = rp->s.s_values[12];
	eip->e = rp->s.s_values[13];
    }

    return 0;		/* OK */
}


int
rt_superell_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_superell_internal *tip;
    union record *rec;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_SUPERELL) return -1;
    tip = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(tip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "superell external");
    rec = (union record *)ep->ext_buf;

    rec->s.s_id = ID_SOLID;
    rec->s.s_type = SUPERELL;

    /* NOTE: This also converts to dbfloat_t */
    VSCALE(&rec->s.s_values[0], tip->v, local2mm);
    VSCALE(&rec->s.s_values[3], tip->a, local2mm);
    VSCALE(&rec->s.s_values[6], tip->b, local2mm);
    VSCALE(&rec->s.s_values[9], tip->c, local2mm);

    printf("SUPERELL: %g %g\n", tip->n, tip->e);

    rec->s.s_values[12] = tip->n;
    rec->s.s_values[13] = tip->e;

    return 0;
}

int
rt_superell_mat(struct rt_db_internal *rop, const mat_t mat, const struct rt_db_internal *ip)
{
    if (!rop || !ip || !mat)
	return BRLCAD_OK;

    struct rt_superell_internal *tip = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(tip);
    struct rt_superell_internal *top = (struct rt_superell_internal *)rop->idb_ptr;
    RT_SUPERELL_CK_MAGIC(top);

    vect_t sv, sa, sb ,sc;

    VMOVE(sv, tip->v);
    VMOVE(sa, tip->a);
    VMOVE(sb, tip->b);
    VMOVE(sc, tip->c);

    MAT4X3PNT(top->v, mat, sv);
    MAT4X3VEC(top->a, mat, sa);
    MAT4X3VEC(top->b, mat, sb);
    MAT4X3VEC(top->c, mat, sc);

    return BRLCAD_OK;
}


/**
 * Import an superellipsoid/sphere from the database format to the
 * internal structure.  Apply modeling transformations as wsuperell.
 */
int
rt_superell_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_superell_internal *eip;

    /* must be double for import and export */
    double vec[ELEMENTS_PER_VECT*4 + 2];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(ep);

    BU_ASSERT(ep->ext_nbytes == SIZEOF_NETWORK_DOUBLE * (ELEMENTS_PER_VECT*4 + 2));

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_SUPERELL;
    ip->idb_meth = &OBJ[ID_SUPERELL];
    BU_ALLOC(ip->idb_ptr, struct rt_superell_internal);

    eip = (struct rt_superell_internal *)ip->idb_ptr;
    eip->magic = RT_SUPERELL_INTERNAL_MAGIC;

    /* Convert from database (network) to internal (host) format */
    bu_cv_ntohd((unsigned char *)vec, ep->ext_buf, ELEMENTS_PER_VECT*4 + 2);

    /* Assign values */
    VMOVE(eip->v, &vec[0*ELEMENTS_PER_VECT]);
    VMOVE(eip->a, &vec[1*ELEMENTS_PER_VECT]);
    VMOVE(eip->b, &vec[2*ELEMENTS_PER_VECT]);
    VMOVE(eip->c, &vec[3*ELEMENTS_PER_VECT]);
    eip->n = vec[4*ELEMENTS_PER_VECT];
    eip->e = vec[4*ELEMENTS_PER_VECT + 1];

    /* Apply modeling transformations */
    if (mat == NULL) mat = bn_mat_identity;
    return rt_superell_mat(ip, mat, ip);
}


/**
 * The external format is:
 * V point
 * A vector
 * B vector
 * C vector
 */
int
rt_superell_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_superell_internal *eip;

    /* must be double for import and export */
    double vec[ELEMENTS_PER_VECT*4 + 2];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_SUPERELL) return -1;
    eip = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(eip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * (ELEMENTS_PER_VECT*4 + 2);
    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "superell external");

    /* scale 'em into local buffer */
    VSCALE(&vec[0*ELEMENTS_PER_VECT], eip->v, local2mm);
    VSCALE(&vec[1*ELEMENTS_PER_VECT], eip->a, local2mm);
    VSCALE(&vec[2*ELEMENTS_PER_VECT], eip->b, local2mm);
    VSCALE(&vec[3*ELEMENTS_PER_VECT], eip->c, local2mm);

    vec[4*ELEMENTS_PER_VECT] = eip->n;
    vec[4*ELEMENTS_PER_VECT + 1] = eip->e;

    /* Convert from internal (host) to database (network) format */
    bu_cv_htond(ep->ext_buf, (unsigned char *)vec, ELEMENTS_PER_VECT*4 + 2);

    return 0;
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_superell_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_superell_internal *tip =
	(struct rt_superell_internal *)ip->idb_ptr;
    fastf_t mag_a, mag_b, mag_c;
    char buf[256];
    double angles[5];
    vect_t unitv;

    RT_SUPERELL_CK_MAGIC(tip);
    bu_vls_strcat(str, "superellipsoid (SUPERELL)\n");

    sprintf(buf, "\tV (%g, %g, %g)\n",
	    INTCLAMP(tip->v[X] * mm2local),
	    INTCLAMP(tip->v[Y] * mm2local),
	    INTCLAMP(tip->v[Z] * mm2local));
    bu_vls_strcat(str, buf);

    mag_a = MAGNITUDE(tip->a);
    mag_b = MAGNITUDE(tip->b);
    mag_c = MAGNITUDE(tip->c);

    sprintf(buf, "\tA (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->a[X] * mm2local),
	    INTCLAMP(tip->a[Y] * mm2local),
	    INTCLAMP(tip->a[Z] * mm2local),
	    INTCLAMP(mag_a * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tB (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->b[X] * mm2local),
	    INTCLAMP(tip->b[Y] * mm2local),
	    INTCLAMP(tip->b[Z] * mm2local),
	    INTCLAMP(mag_b * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tC (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->c[X] * mm2local),
	    INTCLAMP(tip->c[Y] * mm2local),
	    INTCLAMP(tip->c[Z] * mm2local),
	    INTCLAMP(mag_c * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\t<n, e> (%g, %g)\n", INTCLAMP(tip->n), INTCLAMP(tip->e));
    bu_vls_strcat(str, buf);

    if (!verbose) return 0;

    VSCALE(unitv, tip->a, 1/mag_a);
    rt_find_fallback_angle(angles, unitv);
    rt_pr_fallback_angle(str, "\tA", angles);

    VSCALE(unitv, tip->b, 1/mag_b);
    rt_find_fallback_angle(angles, unitv);
    rt_pr_fallback_angle(str, "\tB", angles);

    VSCALE(unitv, tip->c, 1/mag_c);
    rt_find_fallback_angle(angles, unitv);
    rt_pr_fallback_angle(str, "\tC", angles);

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_superell_ifree(struct rt_db_internal *ip)
{
    RT_CK_DB_INTERNAL(ip);

    bu_free(ip->idb_ptr, "superell ifree");
    ip->idb_ptr = ((void *)0);
}


/* The U parameter runs south to north.  In order to orient loop CCW,
 * need to start with 0, 1-->0, 0 transition at the south pole.
 */
/* unused var:
static const fastf_t rt_superell_uvw[5*ELEMENTS_PER_VECT] = {
    0, 1, 0,
    0, 0, 0,
    1, 0, 0,
    1, 1, 0,
    0, 1, 0
};
*/

int
rt_superell_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


/**
 * Computes the volume of a superellipsoid
 *
 * Volume equation from http://lrv.fri.uni-lj.si/~franc/SRSbook/geometry.pdf
 * which also includes a derivation on page 32.
 */
void
rt_superell_volume(fastf_t *volume, const struct rt_db_internal *ip)
{
    if (volume == NULL || ip == NULL) {
	return;
    }

#ifdef HAVE_TGAMMA
    RT_CK_DB_INTERNAL(ip);
    struct rt_superell_internal *sip = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(sip);

    double mag_a = MAGNITUDE(sip->a);
    double mag_b = MAGNITUDE(sip->b);
    double mag_c = MAGNITUDE(sip->c);

    *volume = 2.0 * mag_a * mag_b * mag_c * sip->e * sip->n * (tgamma(sip->n/2.0 + 1.0) * tgamma(sip->n) / tgamma(3.0 * sip->n/2.0 + 1.0)) * (tgamma(sip->e / 2.0) * tgamma(sip->e / 2.0) / tgamma(sip->e));
#else
    /* tgamma unavailable: fall back to Cauchy-Crofton estimation */
    do { static const struct rt_crofton_params _p = {50000u, 0.0, 0.0}; rt_crofton_sample(NULL, volume, ip, &_p); } while (0);
#endif
}


static fastf_t
superell_surf_area_box(vect_t mags)
{
    return 8 * (mags[0] * mags[1] + mags[0] * mags[2] + mags[1] * mags[2]);
}


static fastf_t
sign(fastf_t x)
{
    if (x > 0) {
	return 1.0;
    } else if (x < 0) {
	return -1.0;
    } else {
	return 0.0;
    }
}


/* This is the c auxiliary function for superell_xyz_from_uv.
 * See http://en.wikipedia.org/wiki/Superellipsoid for more
 * information.
 */
static fastf_t
superell_c(fastf_t w, fastf_t m)
{
    return sign(cos(w)) * pow(fabs(cos(w)), m);
}


/* This is the s auxiliary function for superell_xyz_from_uv.
 * See http://en.wikipedia.org/wiki/Superellipsoid for more
 * information.
 */
static fastf_t
superell_s(fastf_t w, fastf_t m)
{
    return sign(sin(w)) * pow(fabs(sin(w)), m);
}


/* This calculates the xyz coordinates of a set of uv coordinates on
 * a superellipsoid, using the algorithm detailed in these places:
 *
 * http://en.wikipedia.org/wiki/Superellipsoid
 * http://paulbourke.net/geometry/superellipse/
 *
 * Since this code is called inside a loop through v-values inside a
 * loop through u-values, superell_c(u, sip->e) and superell_s(u,
 * sip->e) can be precomputed, which results in a significant
 * performance gain; for this reason, the function takes there values
 * and not he original u-value.
 */
static void
superell_xyz_from_uv(point_t pt, fastf_t u, fastf_t v, vect_t mags, const struct rt_superell_internal *sip)
{
    pt[X] = mags[0] * superell_c(v, sip->n) * superell_c(u, sip->e);
    pt[Y] = mags[1] * superell_c(v, sip->n) * superell_s(u, sip->e);
    pt[Z] = mags[2] * superell_s(v, sip->n);
}


/* This attempts to find the surface area by subdividing the uv plane
 * into squares, and finding the approximate surface area of each
 * square.
 */
static fastf_t
superell_surf_area_general(const struct rt_superell_internal *sip, vect_t mags, fastf_t side_length)
{
    fastf_t area = 0;
    fastf_t u, v;

    /* There are M_PI / side_length + 1 values because both ends have
       to be stored */
    int row_length = sizeof(point_t) * (M_PI / side_length + 1);

    point_t *row1 = (point_t *)bu_malloc(row_length, "superell_surf_area_general");
    point_t *row2 = (point_t *)bu_malloc(row_length, "superell_surf_area_general");

    int idx = 0;


    /* This function keeps a moving window of two rows at any time,
     * and calculates the area of space between those two rows. It
     * calculates the first row outside of the loop so that at each
     * iteration it only has to calculate the second. Following
     * standard definitions of u and v, u ranges from -pi to pi, and v
     * ranges from -pi/2 to pi/2. Using an extra index variable allows
     * the code to compute the index into the array very efficiently.
     */
    for (v = -M_PI_2; v < M_PI_2; v += side_length, idx++) {
	superell_xyz_from_uv(row1[idx], -M_PI, v, mags, sip);
    }


    /* This starts at -M_PI + side_length because the first row is
     * computed outside the loop, which allows the loop to always
     * calculate the second row of the pair.
     */
    for (u = -M_PI + side_length; u < M_PI; u += side_length) {
	idx = 0;
	for (v = -M_PI_2; v < M_PI_2; v += side_length, idx++) {
	    superell_xyz_from_uv(row2[idx], u + side_length, v, mags, sip);
	}

	idx = 0;

	/* This ends at -M_PI / 2 - side_length because if it kept
	 * going it would overflow the array, since it always looks at
	 * the square to the right of its current index.
	 */
	for (v = - M_PI_2; v < M_PI_2 - side_length; v += side_length, idx++) {
	    area +=
		bg_dist_pnt3_pnt3(row1[idx], row1[idx + 1]) *
		bg_dist_pnt3_pnt3(row1[idx], row2[idx]);
	}

	memcpy(row1, row2, row_length);
    }

    bu_free(row1, "superell_surf_area_general");
    bu_free(row2, "superell_surf_area_general");

    return area;
}


void
rt_superell_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    struct rt_superell_internal *sip;
    vect_t mags;

    RT_CK_DB_INTERNAL(ip);
    sip = (struct rt_superell_internal *)ip->idb_ptr;

    mags[0] = MAGNITUDE(sip->a);
    mags[1] = MAGNITUDE(sip->b);
    mags[2] = MAGNITUDE(sip->c);

    /* The parametric representation does not work correctly at n = e
     * = 0, so this uses a special calculation for boxes.
     */
    if ((NEAR_EQUAL(sip->e, 0, BN_TOL_DIST)) && NEAR_EQUAL(sip->n, 0, BN_TOL_DIST)) {
	*area = superell_surf_area_box(mags);
    } else {
	/* This number specifies the initial precision used. The
	 * precision is roughly the number of chunks that the uv-space
	 * is divided into during the approximation; the larger the
	 * number the higher the accuracy and the lower the
	 * performance.
	 */
	int precision = 1024;

	fastf_t previous_area = 0;
	fastf_t current_area = superell_surf_area_general(sip, mags, M_PI / precision);
	while (!(NEAR_EQUAL(current_area, previous_area, BN_TOL_DIST))) {
	    /* The precision is multiplied by a constant on each round
	     * through the loop to make sure that the precision
	     * continues to increase until a satisfactory value is
	     * found. If this value is very small, this approximation
	     * will likely converge fairly quickly and with lower
	     * accuracy: the smaller the distance between the inputs
	     * the more likely that the outputs will be within
	     * BN_TOL_DIST. A large value will result in slow
	     * convergence, but in high accuracy: when the values are
	     * finally within BN_TOL_DIST of each other the
	     * approximation must be very good, because a large
	     * increase in input precision resulted in a small
	     * increase in accuracy.
	     */
	    precision *= 2;
	    previous_area = current_area;
	    current_area = superell_surf_area_general(sip, mags, M_PI / precision);
	}
	*area = current_area;
    }
}

int
rt_superell_labels(struct rt_point_labels *pl, int pl_max, const mat_t xform, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    int lcnt = 4;
    if (!pl || pl_max < lcnt || !ip)
	return 0;

    struct rt_superell_internal *superell = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);

    point_t work, pos_view;
    int npl = 0;

#define POINT_LABEL(_pt, _char) { \
    VMOVE(pl[npl].pt, _pt); \
    pl[npl].str[0] = _char; \
    pl[npl++].str[1] = '\0'; }

    MAT4X3PNT(pos_view, xform, superell->v);
    POINT_LABEL(pos_view, 'V');

    VADD2(work, superell->v, superell->a);
    MAT4X3PNT(pos_view, xform, work);
    POINT_LABEL(pos_view, 'A');

    VADD2(work, superell->v, superell->b);
    MAT4X3PNT(pos_view, xform, work);
    POINT_LABEL(pos_view, 'B');

    VADD2(work, superell->v, superell->c);
    MAT4X3PNT(pos_view, xform, work);
    POINT_LABEL(pos_view, 'C');

    return lcnt;
}

const char *
rt_superell_keypoint(point_t *pt, const char *keystr, const mat_t mat, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    if (!pt || !ip)
	return NULL;

    point_t mpt = VINIT_ZERO;
    struct rt_superell_internal *superell = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);

    static const char *default_keystr = "V";
    const char *k = (keystr) ? keystr : default_keystr;

    if (BU_STR_EQUAL(k, default_keystr)) {
	VMOVE(mpt, superell->v);
	goto superell_kpt_end;
    }
    if (BU_STR_EQUAL(k, "A")) {
	VADD2(mpt, superell->v, superell->a);
	goto superell_kpt_end;
    }
    if (BU_STR_EQUAL(k, "B")) {
	VADD2(mpt, superell->v, superell->b);
	goto superell_kpt_end;
    }
    if (BU_STR_EQUAL(k, "C")) {
	VADD2(mpt, superell->v, superell->c);
	goto superell_kpt_end;
    }

    // No keystr matches - failed
    return NULL;

superell_kpt_end:

    MAT4X3PNT(*pt, mat, mpt);

    return k;
}


int
rt_superell_perturb(struct rt_db_internal **oip, const struct rt_db_internal *ip, int UNUSED(planar_only), fastf_t val)
{
    if (NEAR_ZERO(val, SMALL_FASTF))
	return BRLCAD_OK;

    if (!oip || !ip)
	return BRLCAD_ERROR;

    struct rt_superell_internal *osuper = (struct rt_superell_internal *)ip->idb_ptr;
    RT_SUPERELL_CK_MAGIC(osuper);

    struct rt_db_internal *nip;
    BU_GET(nip, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(nip);
    nip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    nip->idb_type = ID_SUPERELL;
    nip->idb_meth = &OBJ[ID_SUPERELL];
    struct rt_superell_internal *super = NULL;
    BU_ALLOC(super, struct rt_superell_internal);
    nip->idb_ptr = super;
    super->magic = RT_SUPERELL_INTERNAL_MAGIC;
    VMOVE(super->v, osuper->v);
    VMOVE(super->a, osuper->a);
    VMOVE(super->b, osuper->b);
    VMOVE(super->c, osuper->c);
    super->n = osuper->n;
    super->e = osuper->e;

    /* Scale each semi-axis outward.  The exponents n and e are preserved;
     * planar_only is not applicable since face planarity varies with n/e. */
    vect_t mvec;
    VMOVE(mvec, super->a); VUNITIZE(mvec); VSCALE(mvec, mvec, val);
    VADD2(super->a, super->a, mvec);

    VMOVE(mvec, super->b); VUNITIZE(mvec); VSCALE(mvec, mvec, val);
    VADD2(super->b, super->b, mvec);

    VMOVE(mvec, super->c); VUNITIZE(mvec); VSCALE(mvec, mvec, val);
    VADD2(super->c, super->c, mvec);

    *oip = nip;
    return BRLCAD_OK;
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
