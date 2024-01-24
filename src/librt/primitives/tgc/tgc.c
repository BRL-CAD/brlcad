/*                           T G C . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2023 United States Government as represented by
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
/** @file primitives/tgc/tgc.c
 *
 * Intersect a ray with a Truncated General Cone.
 *
 * Method -
 * TGC:  solve quartic equation of cone and line
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


struct tgc_specific {
    vect_t tgc_V;		/* Vector to center of base of TGC */
    fastf_t tgc_sH;		/* magnitude of sheared H vector */
    fastf_t tgc_A;		/* magnitude of A vector */
    fastf_t tgc_B;		/* magnitude of B vector */
    fastf_t tgc_C;		/* magnitude of C vector */
    fastf_t tgc_D;		/* magnitude of D vector */
    fastf_t tgc_CdAm1;		/* (C/A - 1) */
    fastf_t tgc_DdBm1;		/* (D/B - 1) */
    fastf_t tgc_AAdCC;		/* (|A|**2)/(|C|**2) */
    fastf_t tgc_BBdDD;		/* (|B|**2)/(|D|**2) */
    vect_t tgc_N;		/* normal at 'top' of cone */
    mat_t tgc_ScShR;		/* Scale(Shear(Rot(vect))) */
    mat_t tgc_invRtShSc;	/* invRot(trnShear(Scale(vect))) */
    char tgc_AD_CB;		/* boolean:  A*D == C*B */
};


extern int rt_rec_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip);

static void rt_tgc_rotate(fastf_t *A, fastf_t *B, fastf_t *Hv, fastf_t *Rot, fastf_t *Inv, struct tgc_specific *tgc);
static void rt_tgc_shear(const fastf_t *vect, int axis, fastf_t *Shr, fastf_t *Trn, fastf_t *Inv);
static void rt_tgc_scale(fastf_t a, fastf_t b, fastf_t h, fastf_t *Scl, fastf_t *Inv);

void rt_pnt_sort(register fastf_t *t, int npts);


#define VLARGE 1000000.0

#define T_OUT 0
#define T_IN 1

/* hit_surfno is set to one of these */
#define TGC_NORM_BODY (1)	/* compute normal */
#define TGC_NORM_TOP (2)	/* copy tgc_N */
#define TGC_NORM_BOT (3)	/* copy reverse tgc_N */

#define RT_TGC_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

#define ALPHA(x, y, c, d)	((x)*(x)*(c) + (y)*(y)*(d))

/* determines the class of tgc given vector magnitudes a, b, c, d */
#define GET_TGC_TYPE(type, a, b, c, d) \
    { \
	if (EQUAL((a), (b)) && EQUAL((c), (d))) { \
	    if (EQUAL((a), (c))) { \
		(type) = RCC; \
	    } else { \
		(type) = TRC; \
	    } \
	} else { \
	    if (EQUAL((a), (c)) && EQUAL((b), (d))) { \
		(type) = REC; \
	    } else { \
		(type) = TEC; \
	    } \
	} \
    }


const struct bu_structparse rt_tgc_parse[] = {
    { "%f", 3, "V", bu_offsetofarray(struct rt_tgc_internal, v, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "H", bu_offsetofarray(struct rt_tgc_internal, h, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "A", bu_offsetofarray(struct rt_tgc_internal, a, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "B", bu_offsetofarray(struct rt_tgc_internal, b, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "C", bu_offsetofarray(struct rt_tgc_internal, c, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "D", bu_offsetofarray(struct rt_tgc_internal, d, fastf_t, X), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

#ifdef USE_OPENCL
/* largest data members first */
struct clt_tgc_specific {
    cl_double tgc_V[3];             /* Vector to center of base of TGC */
    cl_double tgc_CdAm1;            /* (C/A - 1) */
    cl_double tgc_DdBm1;            /* (D/B - 1) */
    cl_double tgc_AAdCC;            /* (|A|**2)/(|C|**2) */
    cl_double tgc_BBdDD;            /* (|B|**2)/(|D|**2) */
    cl_double tgc_N[3];             /* normal at 'top' of cone */
    cl_double tgc_ScShR[16];        /* Scale(Shear(Rot(vect))) */
    cl_double tgc_invRtShSc[16];    /* invRot(trnShear(Scale(vect))) */
    cl_char tgc_AD_CB;              /* boolean:  A*D == C*B */
};


size_t
clt_tgc_pack(struct bu_pool *pool, struct soltab *stp)
{
    struct tgc_specific *tgc =
        (struct tgc_specific *)stp->st_specific;
    struct clt_tgc_specific *args;

    const size_t size = sizeof(*args);
    args = (struct clt_tgc_specific*)bu_pool_alloc(pool, 1, size);

    VMOVE(args->tgc_V, tgc->tgc_V);
    args->tgc_CdAm1 = tgc->tgc_CdAm1;
    args->tgc_DdBm1 = tgc->tgc_DdBm1;
    args->tgc_AAdCC = tgc->tgc_AAdCC;
    args->tgc_BBdDD = tgc->tgc_BBdDD;
    VMOVE(args->tgc_N, tgc->tgc_N);
    MAT_COPY(args->tgc_ScShR, tgc->tgc_ScShR);
    MAT_COPY(args->tgc_invRtShSc, tgc->tgc_invRtShSc);
    args->tgc_AD_CB = tgc->tgc_AD_CB;
    return size;
}


#endif /* USE_OPENCL */

/**
 * Compute the bounding RPP for a truncated general cone
 */
int
rt_tgc_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol)) {
    vect_t work, temp;

    struct rt_tgc_internal *tip = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tip);

    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    /* There are 8 corners to the bounding RPP */
    /* This may not be minimal, but does fully contain the TGC */
    VADD2(temp, tip->v, tip->a);
    VADD2(work, temp, tip->b);
    VMINMAX((*min), (*max), work);	/* V + A + B */
    VSUB2(work, temp, tip->b);
    VMINMAX((*min), (*max), work);	/* V + A - B */

    VSUB2(temp, tip->v, tip->a);
    VADD2(work, temp, tip->b);
    VMINMAX((*min), (*max), work);	/* V - A + B */
    VSUB2(work, temp, tip->b);
    VMINMAX((*min), (*max), work);	/* V - A - B */

    VADD3(temp, tip->v, tip->h, tip->c);
    VADD2(work, temp, tip->d);
    VMINMAX((*min), (*max), work);	/* V + H + C + D */
    VSUB2(work, temp, tip->d);
    VMINMAX((*min), (*max), work);	/* V + H + C - D */

    VADD2(temp, tip->v, tip->h);
    VSUB2(temp, temp, tip->c);
    VADD2(work, temp, tip->d);
    VMINMAX((*min), (*max), work);	/* V + H - C + D */
    VSUB2(work, temp, tip->d);
    VMINMAX((*min), (*max), work);	/* V + H - C - D */
    return 0;
}


/**
 * Given the parameters (in vector form) of a truncated general cone,
 * compute the constant terms and a transformation matrix needed for
 * solving the intersection of a ray with the cone.
 *
 * Also compute the return transformation for normals in the
 * transformed space to the original space.  This NOT the inverse of
 * the transformation matrix (if you really want to know why, talk to
 * Ed Davisson).
 */
int
rt_tgc_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_tgc_internal *tip;
    register struct tgc_specific *tgc;
    register fastf_t f;
    fastf_t prod_ab, prod_cd;
    fastf_t magsq_a, magsq_b, magsq_c, magsq_d;
    fastf_t mag_h, mag_a, mag_b, mag_c, mag_d;
    mat_t Rot, Shr, Scl;
    mat_t iRot, tShr, iShr, iScl;
    mat_t tmp;
    vect_t nH;
    vect_t work;

    tip = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tip);

    /*
     * For a fast way out, hand this solid off to the REC routine.
     * If it takes it, then there is nothing to do, otherwise
     * the solid is a TGC.
     */
    if (rt_rec_prep(stp, ip, rtip) == 0)
	return 0;		/* OK */

    /* Validate that |H| > 0, compute |A| |B| |C| |D|		*/
    mag_h = sqrt(MAGSQ(tip->h));
    mag_a = sqrt(magsq_a = MAGSQ(tip->a));
    mag_b = sqrt(magsq_b = MAGSQ(tip->b));
    mag_c = sqrt(magsq_c = MAGSQ(tip->c));
    mag_d = sqrt(magsq_d = MAGSQ(tip->d));
    prod_ab = mag_a * mag_b;
    prod_cd = mag_c * mag_d;

    if (NEAR_ZERO(mag_h, RT_LEN_TOL)) {
	bu_log("tgc(%s):  zero length H vector\n", stp->st_name);
	return 1;		/* BAD */
    }

    /* Validate that figure is not two-dimensional */
    if (NEAR_ZERO(mag_a, RT_LEN_TOL) &&
	NEAR_ZERO(mag_c, RT_LEN_TOL)) {
	bu_log("tgc(%s):  vectors A, C zero length\n", stp->st_name);
	return 1;
    }
    if (NEAR_ZERO(mag_b, RT_LEN_TOL) &&
	NEAR_ZERO(mag_d, RT_LEN_TOL)) {
	bu_log("tgc(%s):  vectors B, D zero length\n", stp->st_name);
	return 1;
    }

    /* Validate that both ends are not degenerate */
    if (prod_ab <= SMALL) {
	/* AB end is degenerate */
	if (prod_cd <= SMALL) {
	    bu_log("tgc(%s):  Both ends degenerate\n", stp->st_name);
	    return 1;		/* BAD */
	}
	/* Exchange ends, so that in solids with one degenerate end,
	 * the CD end always is always the degenerate one
	 */
	VADD2(tip->v, tip->v, tip->h);
	VREVERSE(tip->h, tip->h);
	VSWAP(tip->a, tip->c);
	VSWAP(tip->b, tip->d);
	bu_log("NOTE: tgc(%s): degenerate end exchanged\n", stp->st_name);
    }

    /* Ascertain whether H lies in A-B plane */
    VCROSS(work, tip->a, tip->b);
    f = VDOT(tip->h, work) / (prod_ab*mag_h);
    if (NEAR_ZERO(f, RT_DOT_TOL)) {
	bu_log("tgc(%s):  H lies in A-B plane\n", stp->st_name);
	return 1;		/* BAD */
    }

    if (prod_ab > SMALL) {
	/* Validate that A.B == 0 */
	f = VDOT(tip->a, tip->b) / prod_ab;
	if (! NEAR_ZERO(f, RT_DOT_TOL)) {
	    bu_log("tgc(%s):  A not perpendicular to B, f=%g\n",
		   stp->st_name, f);
	    bu_log("tgc: dot=%g / a*b=%g\n",
		   VDOT(tip->a, tip->b),  prod_ab);
	    return 1;		/* BAD */
	}
    }
    if (prod_cd > SMALL) {
	/* Validate that C.D == 0 */
	f = VDOT(tip->c, tip->d) / prod_cd;
	if (! NEAR_ZERO(f, RT_DOT_TOL)) {
	    bu_log("tgc(%s):  C not perpendicular to D, f=%g\n",
		   stp->st_name, f);
	    bu_log("tgc: dot=%g / c*d=%g\n",
		   VDOT(tip->c, tip->d), prod_cd);
	    return 1;		/* BAD */
	}
    }

    if (mag_a * mag_c > SMALL) {
	/* Validate that A || C */
	f = 1.0 - VDOT(tip->a, tip->c) / (mag_a * mag_c);
	if (! NEAR_ZERO(f, RT_DOT_TOL)) {
	    bu_log("tgc(%s):  A not parallel to C, f=%g\n",
		   stp->st_name, f);
	    return 1;		/* BAD */
	}
    }

    if (mag_b * mag_d > SMALL) {
	/* Validate that B || D, for parallel planes */
	f = 1.0 - VDOT(tip->b, tip->d) / (mag_b * mag_d);
	if (! NEAR_ZERO(f, RT_DOT_TOL)) {
	    bu_log("tgc(%s):  B not parallel to D, f=%g\n",
		   stp->st_name, f);
	    return 1;		/* BAD */
	}
    }

    /* solid is OK, compute constant terms, etc. */
    BU_GET(tgc, struct tgc_specific);
    stp->st_specific = (void *)tgc;

    VMOVE(tgc->tgc_V, tip->v);
    tgc->tgc_A = mag_a;
    tgc->tgc_B = mag_b;
    tgc->tgc_C = mag_c;
    tgc->tgc_D = mag_d;

    if (RT_G_DEBUG&RT_DEBUG_SOLIDS)
	bu_log("%s: a is %.20f, b is %.20f, c is %.20f, d is %.20f\n",
	       stp->st_name, magsq_a, magsq_b, magsq_c, magsq_d);

    /* Part of computing ALPHA() */
    if (ZERO(magsq_c)) {
	tgc->tgc_AAdCC = VLARGE;
    } else {
	tgc->tgc_AAdCC = magsq_a / magsq_c;
    }
    if (ZERO(magsq_d)) {
	tgc->tgc_BBdDD = VLARGE;
    } else {
	tgc->tgc_BBdDD = magsq_b / magsq_d;
    }

    /* If the eccentricities of the two ellipses are the same, then
     * the cone equation reduces to a much simpler quadratic form.
     * Otherwise it is a (gah!) quartic equation.
     */
    tgc->tgc_AD_CB = NEAR_EQUAL((tgc->tgc_A*tgc->tgc_D), (tgc->tgc_C*tgc->tgc_B), 0.0001);		/* A*D == C*B */
    rt_tgc_rotate(tip->a, tip->b, tip->h, Rot, iRot, tgc);
    MAT4X3VEC(nH, Rot, tip->h);
    tgc->tgc_sH = nH[Z];

    tgc->tgc_CdAm1 = tgc->tgc_C/tgc->tgc_A - 1.0;
    tgc->tgc_DdBm1 = tgc->tgc_D/tgc->tgc_B - 1.0;
    if (ZERO(tgc->tgc_CdAm1))
	tgc->tgc_CdAm1 = 0.0;
    if (ZERO(tgc->tgc_DdBm1))
	tgc->tgc_DdBm1 = 0.0;

    /*
     * Added iShr parameter to tgc_shear().  Changed inverse
     * transformation of normal vectors of std.  solid intersection to
     * include shear inverse (tgc_invRtShSc).  Fold in scaling
     * transformation into the transformation to std.  space from
     * target space (tgc_ScShR).
     */
    rt_tgc_shear(nH, Z, Shr, tShr, iShr);
    rt_tgc_scale(tgc->tgc_A, tgc->tgc_B, tgc->tgc_sH, Scl, iScl);

    bn_mat_mul(tmp, Shr, Rot);
    bn_mat_mul(tgc->tgc_ScShR, Scl, tmp);

    bn_mat_mul(tmp, tShr, Scl);
    bn_mat_mul(tgc->tgc_invRtShSc, iRot, tmp);

    /* Compute bounding sphere and RPP */
    {
	fastf_t dx, dy, dz;	/* For bounding sphere */
	if (stp->st_meth->ft_bbox(ip, &(stp->st_min), &(stp->st_max), &(rtip->rti_tol))) return 1;
	VSET(stp->st_center,
	     (stp->st_max[X] + stp->st_min[X])/2,
	     (stp->st_max[Y] + stp->st_min[Y])/2,
	     (stp->st_max[Z] + stp->st_min[Z])/2);

	dx = (stp->st_max[X] - stp->st_min[X])/2;
	f = dx;
	dy = (stp->st_max[Y] - stp->st_min[Y])/2;
	if (dy > f) f = dy;
	dz = (stp->st_max[Z] - stp->st_min[Z])/2;
	if (dz > f) f = dz;
	stp->st_aradius = f;
	stp->st_bradius = sqrt(dx*dx + dy*dy + dz*dz);
    }
    return 0;
}


/**
 * To rotate vectors A and B (where A is perpendicular to B) to the X
 * and Y axes respectively, create a rotation matrix
 *
 *     | A' |
 * R = | B' |
 *     | C' |
 *
 * where A', B' and C' are vectors such that
 *
 * A' = A/|A|	B' = B/|B|	C' = C/|C|
 *
 * where C = H - (H.A')A' - (H.B')B'
 *
 * The last operation (Gram Schmidt method) finds the component of the
 * vector H perpendicular A and to B.  This is, therefore the normal
 * for the planar sections of the truncated cone.
 */
static void
rt_tgc_rotate(fastf_t *A, fastf_t *B, fastf_t *Hv, fastf_t *Rot, fastf_t *Inv, struct tgc_specific *tgc)
{
    vect_t uA, uB, uC;	/* unit vectors */
    fastf_t mag_ha;	/* magnitude of H in the */
    fastf_t mag_hb;	/* A and B directions */

    /* copy A and B, then 'unitize' the results */
    VMOVE(uA, A);
    VUNITIZE(uA);
    VMOVE(uB, B);
    VUNITIZE(uB);

    /* Find component of H in the A direction */
    mag_ha = VDOT(Hv, uA);
    /* Find component of H in the B direction */
    mag_hb = VDOT(Hv, uB);

    /* Subtract the A and B components of H to find the component
     * perpendicular to both, then 'unitize' the result.
     */
    VJOIN2(uC, Hv, -mag_ha, uA, -mag_hb, uB);
    VUNITIZE(uC);
    VMOVE(tgc->tgc_N, uC);

    MAT_IDN(Rot);
    MAT_IDN(Inv);

    Rot[0] = Inv[0] = uA[X];
    Rot[1] = Inv[4] = uA[Y];
    Rot[2] = Inv[8] = uA[Z];

    Rot[4] = Inv[1] = uB[X];
    Rot[5] = Inv[5] = uB[Y];
    Rot[6] = Inv[9] = uB[Z];

    Rot[8]  = Inv[2]  = uC[X];
    Rot[9]  = Inv[6]  = uC[Y];
    Rot[10] = Inv[10] = uC[Z];
}


/**
 * To shear the H vector to the Z axis, every point must be shifted in
 * the X direction by -(Hx/Hz)*z, and in the Y direction by -(Hy/Hz)*z
 * This operation makes the equation for the standard cone much easier
 * to work with.
 *
 * NOTE: This computes the TRANSPOSE of the shear matrix rather than
 * the inverse.
 *
 * Begin changes GSM, EOD -- Added INVERSE (Inv) calculation.
 */
static void
rt_tgc_shear(const fastf_t *vect, int axis, fastf_t *Shr, fastf_t *Trn, fastf_t *Inv)
{
    MAT_IDN(Shr);
    MAT_IDN(Trn);
    MAT_IDN(Inv);

    if (ZERO(vect[axis]))
	bu_bomb("rt_tgc_shear() divide by zero\n");

    if (axis == X) {
	Inv[4] = -(Shr[4] = Trn[1] = -vect[Y]/vect[X]);
	Inv[8] = -(Shr[8] = Trn[2] = -vect[Z]/vect[X]);
    } else if (axis == Y) {
	Inv[1] = -(Shr[1] = Trn[4] = -vect[X]/vect[Y]);
	Inv[9] = -(Shr[9] = Trn[6] = -vect[Z]/vect[Y]);
    } else if (axis == Z) {
	Inv[2] = -(Shr[2] = Trn[8] = -vect[X]/vect[Z]);
	Inv[6] = -(Shr[6] = Trn[9] = -vect[Y]/vect[Z]);
    }
}


static void
rt_tgc_scale(fastf_t a, fastf_t b, fastf_t h, fastf_t *Scl, fastf_t *Inv)
{
    MAT_IDN(Scl);
    MAT_IDN(Inv);
    Scl[0]  /= a;
    Scl[5]  /= b;
    Scl[10] /= h;
    Inv[0]  = a;
    Inv[5]  = b;
    Inv[10] = h;
    return;
}


void
rt_tgc_print(register const struct soltab *stp)
{
    register const struct tgc_specific *tgc =
	(struct tgc_specific *)stp->st_specific;

    VPRINT("V", tgc->tgc_V);
    bu_log("mag sheared H = %f\n", tgc->tgc_sH);
    bu_log("mag A = %f\n", tgc->tgc_A);
    bu_log("mag B = %f\n", tgc->tgc_B);
    bu_log("mag C = %f\n", tgc->tgc_C);
    bu_log("mag D = %f\n", tgc->tgc_D);
    VPRINT("Top normal", tgc->tgc_N);

    bn_mat_print("Sc o Sh o R", tgc->tgc_ScShR);
    bn_mat_print("invR o trnSh o Sc", tgc->tgc_invRtShSc);

    if (tgc->tgc_AD_CB) {
	bu_log("A*D == C*B.  Equal eccentricities gives quadratic equation.\n");
    } else {
	bu_log("A*D != C*B.  Quartic equation.\n");
    }
    bu_log("(C/A - 1) = %f\n", tgc->tgc_CdAm1);
    bu_log("(D/B - 1) = %f\n", tgc->tgc_DdBm1);
    bu_log("(|A|**2)/(|C|**2) = %f\n", tgc->tgc_AAdCC);
    bu_log("(|B|**2)/(|D|**2) = %f\n", tgc->tgc_BBdDD);
}


/**
 * Intersect a ray with a truncated general cone, where all constant
 * terms have been computed by rt_tgc_prep().
 *
 * NOTE: All lines in this function are represented parametrically by
 * a point, P(Px, Py, Pz) and a unit direction vector, D = iDx + jDy +
 * kDz.  Any point on a line can be expressed by one variable 't',
 * where
 *
 * X = Dx*t + Px,
 * Y = Dy*t + Py,
 * Z = Dz*t + Pz.
 *
 * First, convert the line to the coordinate system of a "standard"
 * cone.  This is a cone whose base lies in the X-Y plane, and whose H
 * (now H') vector is lined up with the Z axis.
 *
 * Then find the equation of that line and the standard cone as an
 * equation in 't'.  Solve the equation using a general polynomial
 * root finder.  Use those values of 't' to compute the points of
 * intersection in the original coordinate system.
 */
int
rt_tgc_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    register const struct tgc_specific *tgc =
	(struct tgc_specific *)stp->st_specific;
    register struct seg *segp;
    vect_t pprime;
    vect_t dprime;
    vect_t work;
#define MAX_TGC_HITS 4+2 /* 4 on side cylinder, 1 per end ellipse */
    fastf_t k[MAX_TGC_HITS] = {0};
    int hit_type[MAX_TGC_HITS] = {0};
    fastf_t t, zval, dir;
    fastf_t t_scale;
    int npts;
    int intersect;
    vect_t cor_pprime;	/* corrected P prime */
    fastf_t cor_proj = 0;	/* corrected projected dist */
    int i;
    bn_poly_t C;	/* final equation */
    bn_poly_t Xsqr, Ysqr;
    bn_poly_t R, Rsqr;

    /* find rotated point and direction */
    MAT4X3VEC(dprime, tgc->tgc_ScShR, rp->r_dir);

    /* A vector of unit length in model space (r_dir) changes length
     * in the special unit-tgc space.  This scale factor will restore
     * proper length after hit points are found.
     */
    t_scale = MAGNITUDE(dprime);
    if (ZERO(t_scale)) {
	bu_log("tgc(%s) dprime=(%g, %g, %g), t_scale=%e, miss.\n", stp->st_dp->d_namep,
	       V3ARGS(dprime), t_scale);
	return 0;
    }
    t_scale = 1/t_scale;
    VSCALE(dprime, dprime, t_scale);	/* VUNITIZE(dprime); */

    if (NEAR_ZERO(dprime[Z], RT_PCOEF_TOL)) {
	dprime[Z] = 0.0;	/* prevent rootfinder heartburn */
    }

    VSUB2(work, rp->r_pt, tgc->tgc_V);
    MAT4X3VEC(pprime, tgc->tgc_ScShR, work);

    /* Translating ray origin along direction of ray to closest pt. to
     * origin of solids coordinate system, new ray origin is
     * 'cor_pprime'.
     */
    cor_proj = -VDOT(pprime, dprime);
    VJOIN1(cor_pprime, pprime, cor_proj, dprime);

    /* The TGC is defined in "unit" space, so the parametric distance
     * from one side of the TGC to the other is on the order of 2.
     * Therefore, any vector/point coordinates that are very small
     * here may be considered to be zero, since double precision only
     * has 18 digits of significance.  If these tiny values were left
     * in, then as they get squared (below) they will cause
     * difficulties.
     */
    for (i=0; i<3; i++) {
	/* Direction cosines */
	if (NEAR_ZERO(dprime[i], RT_PCOEF_TOL)) {
	    dprime[i] = 0;
	}
	/* Position in -1..+1 coordinates */
	if (ZERO(cor_pprime[i])) {
	    cor_pprime[i] = 0;
	}
    }

    /* Given a line and the parameters for a standard cone, finds the
     * roots of the equation for that cone and line.  Returns the
     * number of real roots found.
     *
     * Given a line and the cone parameters, finds the equation of the
     * cone in terms of the variable 't'.
     *
     * The equation for the cone is:
     *
     * X**2 * Q**2 + Y**2 * R**2 - R**2 * Q**2 = 0
     *
     * where R = a + ((c - a)/|H'|)*Z
     * Q = b + ((d - b)/|H'|)*Z
     *
     * First, find X, Y, and Z in terms of 't' for this line, then
     * substitute them into the equation above.
     *
     * Express each variable (X, Y, and Z) as a linear equation in
     * 'k', e.g., (dprime[X] * k) + cor_pprime[X], and substitute into
     * the cone equation.
     */
    Xsqr.dgr = 2;
    Xsqr.cf[0] = dprime[X] * dprime[X];
    Xsqr.cf[1] = 2.0 * dprime[X] * cor_pprime[X];
    Xsqr.cf[2] = cor_pprime[X] * cor_pprime[X];

    Ysqr.dgr = 2;
    Ysqr.cf[0] = dprime[Y] * dprime[Y];
    Ysqr.cf[1] = 2.0 * dprime[Y] * cor_pprime[Y];
    Ysqr.cf[2] = cor_pprime[Y] * cor_pprime[Y];

    R.dgr = 1;
    R.cf[0] = dprime[Z] * tgc->tgc_CdAm1;
    /* A vector is unitized (tgc->tgc_A == 1.0) */
    R.cf[1] = (cor_pprime[Z] * tgc->tgc_CdAm1) + 1.0;

    /* (void) rt_poly_mul(&Rsqr, &R, &R); */
    Rsqr.dgr = 2;
    Rsqr.cf[0] = R.cf[0] * R.cf[0];
    Rsqr.cf[1] = R.cf[0] * R.cf[1] * 2.0;
    Rsqr.cf[2] = R.cf[1] * R.cf[1];

    /* If the eccentricities of the two ellipses are the same, then
     * the cone equation reduces to a much simpler quadratic form.
     * Otherwise it is a (gah!) quartic equation.
     *
     * this can only be done when C.cf[0] is not too small! (JRA)
     */
    C.cf[0] = Xsqr.cf[0] + Ysqr.cf[0] - Rsqr.cf[0];
    if (tgc->tgc_AD_CB && !NEAR_ZERO(C.cf[0], RT_PCOEF_TOL)) {
	fastf_t roots;

	/*
	 * (void) bn_poly_add(&sum, &Xsqr, &Ysqr);
	 * (void) bn_poly_sub(&C, &sum, &Rsqr);
	 */
	C.dgr = 2;
	C.cf[1] = Xsqr.cf[1] + Ysqr.cf[1] - Rsqr.cf[1];
	C.cf[2] = Xsqr.cf[2] + Ysqr.cf[2] - Rsqr.cf[2];

	/* Find the real roots the easy way.  C.dgr==2 */
	if ((roots = C.cf[1]*C.cf[1] - 4.0 * C.cf[0] * C.cf[2]) < 0) {
	    npts = 0;	/* no real roots */
	} else {
	    register fastf_t f;
	    roots = sqrt(roots);
	    k[0] = (roots - C.cf[1]) * (f = 0.5 / C.cf[0]);
	    hit_type[0] = TGC_NORM_BODY;
	    k[1] = (roots + C.cf[1]) * -f;
	    hit_type[1] = TGC_NORM_BODY;
	    npts = 2;
	}
    } else {
	bn_poly_t Q, Qsqr;
	bn_complex_t val[MAX_TGC_HITS-2]; /* roots of final equation */
	register int l;
	register int nroots;

	Q.dgr = 1;
	Q.cf[0] = dprime[Z] * tgc->tgc_DdBm1;
	/* B vector is unitized (tgc->tgc_B == 1.0) */
	Q.cf[1] = (cor_pprime[Z] * tgc->tgc_DdBm1) + 1.0;

	/* (void) bn_poly_mul(&Qsqr, &Q, &Q); */
	Qsqr.dgr = 2;
	Qsqr.cf[0] = Q.cf[0] * Q.cf[0];
	Qsqr.cf[1] = Q.cf[0] * Q.cf[1] * 2;
	Qsqr.cf[2] = Q.cf[1] * Q.cf[1];

	/*
	 * (void) bn_poly_mul(&T1, &Qsqr, &Xsqr);
	 * (void) bn_poly_mul(&T2 &Rsqr, &Ysqr);
	 * (void) bn_poly_mul(&T1, &Rsqr, &Qsqr);
	 * (void) bn_poly_add(&sum, &T1, &T2);
	 * (void) bn_poly_sub(&C, &sum, &T3);
	 */
	C.dgr = 4;
	C.cf[0] = Qsqr.cf[0] * Xsqr.cf[0] +
	    Rsqr.cf[0] * Ysqr.cf[0] -
	    (Rsqr.cf[0] * Qsqr.cf[0]);
	C.cf[1] = Qsqr.cf[0] * Xsqr.cf[1] + Qsqr.cf[1] * Xsqr.cf[0] +
	    Rsqr.cf[0] * Ysqr.cf[1] + Rsqr.cf[1] * Ysqr.cf[0] -
	    (Rsqr.cf[0] * Qsqr.cf[1] + Rsqr.cf[1] * Qsqr.cf[0]);
	C.cf[2] = Qsqr.cf[0] * Xsqr.cf[2] + Qsqr.cf[1] * Xsqr.cf[1] +
	    Qsqr.cf[2] * Xsqr.cf[0] +
	    Rsqr.cf[0] * Ysqr.cf[2] + Rsqr.cf[1] * Ysqr.cf[1] +
	    Rsqr.cf[2] * Ysqr.cf[0] -
	    (Rsqr.cf[0] * Qsqr.cf[2] + Rsqr.cf[1] * Qsqr.cf[1] +
	     Rsqr.cf[2] * Qsqr.cf[0]);
	C.cf[3] = Qsqr.cf[1] * Xsqr.cf[2] + Qsqr.cf[2] * Xsqr.cf[1] +
	    Rsqr.cf[1] * Ysqr.cf[2] + Rsqr.cf[2] * Ysqr.cf[1] -
	    (Rsqr.cf[1] * Qsqr.cf[2] + Rsqr.cf[2] * Qsqr.cf[1]);
	C.cf[4] = Qsqr.cf[2] * Xsqr.cf[2] +
	    Rsqr.cf[2] * Ysqr.cf[2] -
	    (Rsqr.cf[2] * Qsqr.cf[2]);

	/* main 'sides' of a TGC (i.e., the cylindrical surface) is a
	 * quartic equation, so we expect to find 0 to 4 roots.
	 */
	nroots = rt_poly_roots(&C, val, stp->st_dp->d_namep);

	/* Retain real roots, ignore the rest.
	 *
	 * If the imaginary part is zero or sufficiently close, then
	 * we pretend it's real since it could be a root solver or
	 * floating point artifact.  we use rt_poly_root's internal
	 * tolerance of 1e-5.
	 */
	for (l=0, npts=0; l < nroots; l++) {
	    if (NEAR_ZERO(val[l].im, RT_ROOT_TOL)) {
		hit_type[npts] = TGC_NORM_BODY;
		k[npts++] = val[l].re;
	    }
	}

	/* sane roots? */
	if (npts > MAX_TGC_HITS-2) {
	    /* shouldn't be possible, but ensure no overflow */
	    npts = MAX_TGC_HITS-2;
	} else if (npts < 0) {
	    static size_t reported = 0;

	    if (reported < 10) {
		bu_log("Root solver failed to converge on a solution for %s\n", stp->st_dp->d_namep);
		/* these are printed in 'mm' regardless of local units */
		VPRINT("\tshooting point (units mm): ", rp->r_pt);
		VPRINT("\tshooting direction:        ", rp->r_dir);
	    } else if (reported == 10) {
		bu_log("Too many convergence failures.  Suppressing further TGC root finder reports.\n");
	    }
	    reported++;
	}
    }

    /*
     * Reverse above translation by adding distance to all 'k' values.
     */
    for (i = 0; i < npts; ++i) {
	k[i] += cor_proj;
    }

    /*
     * Eliminate side cylinder hits beyond the end ellipses.
     */
    {
	int skiplist[MAX_TGC_HITS] = {0};
	int j;
	i = 0;
	/* mark baddies */
	while (i < npts) {
	    zval = k[i]*dprime[Z] + pprime[Z];
	    /* Height vector is unitized (tgc->tgc_sH == 1.0) */
	    if (zval >= 1.0 || zval <= 0.0) {
		skiplist[i] = 1;
	    }
	    i++;
	}
	/* keep goodies */
	i = j = 0;
	while (i < npts) {
	    if (!skiplist[i]) {
		k[j] = k[i];
		hit_type[j] = hit_type[i];
		j++;
	    }
	    i++;
	}
	npts = j;
    }

    /*
     * Consider intersections with the end ellipses
     */
    dir = VDOT(tgc->tgc_N, rp->r_dir);
    if (!ZERO(dprime[Z]) && !NEAR_ZERO(dir, RT_DOT_TOL)) {
	fastf_t alf1, alf2, b;
	b = (-pprime[Z])/dprime[Z];
	/* Height vector is unitized (tgc->tgc_sH == 1.0) */
	t = (1.0 - pprime[Z])/dprime[Z];

	/* the top end */
	VJOIN1(work, pprime, b, dprime);
	/* A and B vectors are unitized (tgc->tgc_A == _B == 1.0) */
	/* alf1 = ALPHA(work[X], work[Y], 1.0, 1.0) */
	alf1 = work[X]*work[X] + work[Y]*work[Y];

	/* the bottom end */
	VJOIN1(work, pprime, t, dprime);
	/* Must scale C and D vectors */
	alf2 = ALPHA(work[X], work[Y], tgc->tgc_AAdCC, tgc->tgc_BBdDD);

	if (alf1 <= 1.0) {
	    hit_type[npts] = TGC_NORM_BOT;
	    k[npts++] = b;
	}
	if (alf2 <= 1.0) {
	    hit_type[npts] = TGC_NORM_TOP;
	    k[npts++] = t;
	}
    }

    /* Sort Most distant to least distant: rt_pnt_sort(k, npts) */
    /* TODO: USE A SORTING NETWORK */
    {
	register fastf_t u;
	register short lim, n;
	register int type;

	for (lim = npts-1; lim > 0; lim--) {
	    for (n = 0; n < lim; n++) {
		if ((u=k[n]) < k[n+1]) {
		    /* bubble larger towards [0] */
		    type = hit_type[n];
		    hit_type[n] = hit_type[n+1];
		    hit_type[n+1] = type;
		    k[n] = k[n+1];
		    k[n+1] = u;
		}
	    }
	}
    }
    /* Now, k[0] > k[npts-1] */

    /* we expect and need an even number of hits */
    if (npts == 1) {
	/* assume we grazed so close, we missed a cap/edge */
	hit_type[1] = hit_type[0];
	k[1] = k[0] + SMALL_FASTF;
	npts++;
    } else if (npts % 2) {
	int rectified = 0;

	/* perhaps we got two hits on an edge.  check for
	 * duplicate hit distances that we can collapse to one.
	 */
	for (i=npts-1; i>0; i--) {
	    fastf_t diff;

	    diff = k[i-1] - k[i];
	    /* should be non-negative due to sorting */
	    if (diff < ap->a_rt_i->rti_tol.dist) {
		/* remove this duplicate */
		int j;

		npts--;
		for (j=i; j<npts; j++) {
		    hit_type[j] = hit_type[j+1];
		    k[j] = k[j+1];
		}

		/* now have an even number */
		rectified = 1;
		break;
	    }
	}

	/* still odd? */
	if (!rectified) {
	    static size_t tgc_msgs = 0;
	    if (tgc_msgs < 10) {
		bu_log("Root solver reported %d intersections != {0, 2, 4} on %s\n", npts, stp->st_name);
		/* these are printed in 'mm' regardless of local units */
		VPRINT("\tshooting point (units mm): ", rp->r_pt);
		VPRINT("\tshooting direction:        ", rp->r_dir);
		for (i = 0; i < npts; i++) {
		    bu_log("\t%g", k[i]*t_scale);
		}
		bu_log("\n");
	    } else if (tgc_msgs == 10) {
		bu_log("Too many grazings.  Suppressing further TGC odd hit reports.\n");
	    }
	    tgc_msgs++;

	    return 0;			/* No hit */
	}
    }

    intersect = 0;
    for (i=npts-1; i>0; i -= 2) {
	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;

	segp->seg_in.hit_dist = k[i] * t_scale;
	segp->seg_in.hit_surfno = hit_type[i];
	if (segp->seg_in.hit_surfno == TGC_NORM_BODY) {
	    VJOIN1(segp->seg_in.hit_vpriv, pprime, k[i], dprime);
	} else {
	    if (dir > 0.0) {
		segp->seg_in.hit_surfno = TGC_NORM_BOT;
	    } else {
		segp->seg_in.hit_surfno = TGC_NORM_TOP;
	    }
	}

	segp->seg_out.hit_dist = k[i-1] * t_scale;
	segp->seg_out.hit_surfno = hit_type[i-1];
	if (segp->seg_out.hit_surfno == TGC_NORM_BODY) {
	    VJOIN1(segp->seg_out.hit_vpriv, pprime, k[i-1], dprime);
	} else {
	    if (dir > 0.0) {
		segp->seg_out.hit_surfno = TGC_NORM_TOP;
	    } else {
		segp->seg_out.hit_surfno = TGC_NORM_BOT;
	    }
	}
	intersect++;
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }


    return intersect;
}


/**
 * The Homer vectorized version.
 */
void
rt_tgc_vshot(struct soltab **stp, register struct xray **rp, struct seg *segp, int n, struct application *ap)


/* array of segs (results returned) */
/* Number of ray/object pairs */

{
    register struct tgc_specific *tgc;
    register int ix;
    vect_t pprime;
    vect_t dprime;
    vect_t work;
    fastf_t k[4], pt[2];
    fastf_t t, b, zval, dir;
    fastf_t t_scale = 0;
    fastf_t alf1, alf2;
    int npts;
    int intersect;
    vect_t cor_pprime;	/* corrected P prime */
    fastf_t cor_proj = 0;	/* corrected projected dist */
    int i;
    bn_poly_t *C;	/* final equation */
    bn_poly_t Xsqr, Ysqr;
    bn_poly_t R, Rsqr;

    VSETALLN(k, 0, 4);
    VSETALLN(pt, 0, 2);

    if (ap) RT_CK_APPLICATION(ap);

    /* Allocate space for polys and roots */
    C = (bn_poly_t *)bu_malloc(n * sizeof(bn_poly_t), "tor bn_poly_t");

    /* Initialize seg_stp to assume hit (zero will then flag miss) */
    for (ix = 0; ix < n; ix++) segp[ix].seg_stp = stp[ix];

    /* for each ray/cone pair */
    for (ix = 0; ix < n; ix++) {
	if (segp[ix].seg_stp == 0) continue; /* == 0 signals skip ray */

	tgc = (struct tgc_specific *)stp[ix]->st_specific;

	/* find rotated point and direction */
	MAT4X3VEC(dprime, tgc->tgc_ScShR, rp[ix]->r_dir);

	/*
	 * A vector of unit length in model space (r_dir) changes length in
	 * the special unit-tgc space.  This scale factor will restore
	 * proper length after hit points are found.
	 */
	t_scale = 1/MAGNITUDE(dprime);
	VSCALE(dprime, dprime, t_scale);	/* VUNITIZE(dprime); */

	if (NEAR_ZERO(dprime[Z], RT_PCOEF_TOL))
	    dprime[Z] = 0.0;	/* prevent rootfinder heartburn */

	/* Use segp[ix].seg_in.hit_normal as tmp to hold dprime */
	VMOVE(segp[ix].seg_in.hit_normal, dprime);

	VSUB2(work, rp[ix]->r_pt, tgc->tgc_V);
	MAT4X3VEC(pprime, tgc->tgc_ScShR, work);

	/* Use segp[ix].seg_out.hit_normal as tmp to hold pprime */
	VMOVE(segp[ix].seg_out.hit_normal, pprime);

	/* Translating ray origin along direction of ray to closest
	 * pt. to origin of solids coordinate system, new ray origin
	 * is 'cor_pprime'.
	 */
	cor_proj = VDOT(pprime, dprime);
	VSCALE(cor_pprime, dprime, cor_proj);
	VSUB2(cor_pprime, pprime, cor_pprime);

	/*
	 * Given a line and the parameters for a standard cone, finds
	 * the roots of the equation for that cone and line.  Returns
	 * the number of real roots found.
	 *
	 * Given a line and the cone parameters, finds the equation of
	 * the cone in terms of the variable 't'.
	 *
	 * The equation for the cone is:
	 *
	 * X**2 * Q**2 + Y**2 * R**2 - R**2 * Q**2 = 0
	 *
	 * where:
	 * R = a + ((c - a)/|H'|)*Z
	 * Q = b + ((d - b)/|H'|)*Z
	 *
	 * First, find X, Y, and Z in terms of 't' for this line, then
	 * substitute them into the equation above.
	 *
	 * Express each variable (X, Y, and Z) as a linear equation in
	 * 'k', e.g., (dprime[X] * k) + cor_pprime[X], and substitute
	 * into the cone equation.
	 */
	Xsqr.dgr = 2;
	Xsqr.cf[0] = dprime[X] * dprime[X];
	Xsqr.cf[1] = 2.0 * dprime[X] * cor_pprime[X];
	Xsqr.cf[2] = cor_pprime[X] * cor_pprime[X];

	Ysqr.dgr = 2;
	Ysqr.cf[0] = dprime[Y] * dprime[Y];
	Ysqr.cf[1] = 2.0 * dprime[Y] * cor_pprime[Y];
	Ysqr.cf[2] = cor_pprime[Y] * cor_pprime[Y];

	R.dgr = 1;
	R.cf[0] = dprime[Z] * tgc->tgc_CdAm1;
	/* A vector is unitized (tgc->tgc_A == 1.0) */
	R.cf[1] = (cor_pprime[Z] * tgc->tgc_CdAm1) + 1.0;

	/* (void) bn_poly_mul(&Rsqr, &R, &R); manual expansion: */
	Rsqr.dgr = 2;
	Rsqr.cf[0] = R.cf[0] * R.cf[0];
	Rsqr.cf[1] = R.cf[0] * R.cf[1] * 2;
	Rsqr.cf[2] = R.cf[1] * R.cf[1];

	/*
	 * If the eccentricities of the two ellipses are the same,
	 * then the cone equation reduces to a much simpler quadratic
	 * form.  Otherwise it is a (gah!) quartic equation.
	 */
	if (tgc->tgc_AD_CB) {
	    /* (void) bn_poly_add(&sum, &Xsqr, &Ysqr); and */
	    /* (void) bn_poly_sub(&C, &sum, &Rsqr); manual expansion: */
	    C[ix].dgr = 2;
	    C[ix].cf[0] = Xsqr.cf[0] + Ysqr.cf[0] - Rsqr.cf[0];
	    C[ix].cf[1] = Xsqr.cf[1] + Ysqr.cf[1] - Rsqr.cf[1];
	    C[ix].cf[2] = Xsqr.cf[2] + Ysqr.cf[2] - Rsqr.cf[2];
	} else {
	    bn_poly_t Q, Qsqr;

	    Q.dgr = 1;
	    Q.cf[0] = dprime[Z] * tgc->tgc_DdBm1;
	    /* B vector is unitized (tgc->tgc_B == 1.0) */
	    Q.cf[1] = (cor_pprime[Z] * tgc->tgc_DdBm1) + 1.0;

	    /* (void) bn_poly_mul(&Qsqr, &Q, &Q); manual expansion: */
	    Qsqr.dgr = 2;
	    Qsqr.cf[0] = Q.cf[0] * Q.cf[0];
	    Qsqr.cf[1] = Q.cf[0] * Q.cf[1] * 2;
	    Qsqr.cf[2] = Q.cf[1] * Q.cf[1];

	    /* (void) bn_poly_mul(&T1, &Qsqr, &Xsqr); manual expansion: */
	    C[ix].dgr = 4;
	    C[ix].cf[0] = Qsqr.cf[0] * Xsqr.cf[0];
	    C[ix].cf[1] = Qsqr.cf[0] * Xsqr.cf[1] +
		Qsqr.cf[1] * Xsqr.cf[0];
	    C[ix].cf[2] = Qsqr.cf[0] * Xsqr.cf[2] +
		Qsqr.cf[1] * Xsqr.cf[1] +
		Qsqr.cf[2] * Xsqr.cf[0];
	    C[ix].cf[3] = Qsqr.cf[1] * Xsqr.cf[2] +
		Qsqr.cf[2] * Xsqr.cf[1];
	    C[ix].cf[4] = Qsqr.cf[2] * Xsqr.cf[2];

	    /* (void) bn_poly_mul(&T2, &Rsqr, &Ysqr); and */
	    /* (void) bn_poly_add(&sum, &T1, &T2); manual expansion: */
	    C[ix].cf[0] += Rsqr.cf[0] * Ysqr.cf[0];
	    C[ix].cf[1] += Rsqr.cf[0] * Ysqr.cf[1] +
		Rsqr.cf[1] * Ysqr.cf[0];
	    C[ix].cf[2] += Rsqr.cf[0] * Ysqr.cf[2] +
		Rsqr.cf[1] * Ysqr.cf[1] +
		Rsqr.cf[2] * Ysqr.cf[0];
	    C[ix].cf[3] += Rsqr.cf[1] * Ysqr.cf[2] +
		Rsqr.cf[2] * Ysqr.cf[1];
	    C[ix].cf[4] += Rsqr.cf[2] * Ysqr.cf[2];

	    /* (void) bn_poly_mul(&T3, &Rsqr, &Qsqr); and */
	    /* (void) bn_poly_sub(&C, &sum, &T3); manual expansion: */
	    C[ix].cf[0] -= Rsqr.cf[0] * Qsqr.cf[0];
	    C[ix].cf[1] -= Rsqr.cf[0] * Qsqr.cf[1] +
		Rsqr.cf[1] * Qsqr.cf[0];
	    C[ix].cf[2] -= Rsqr.cf[0] * Qsqr.cf[2] +
		Rsqr.cf[1] * Qsqr.cf[1] +
		Rsqr.cf[2] * Qsqr.cf[0];
	    C[ix].cf[3] -= Rsqr.cf[1] * Qsqr.cf[2] +
		Rsqr.cf[2] * Qsqr.cf[1];
	    C[ix].cf[4] -= Rsqr.cf[2] * Qsqr.cf[2];
	}

    }

    /* It seems impractical to try to vectorize finding and sorting roots. */
    for (ix = 0; ix < n; ix++) {
	if (segp[ix].seg_stp == 0) continue; /* == 0 signals skip ray */

	/* Again, check for the equal eccentricities case. */
	if (C[ix].dgr == 2) {
	    fastf_t roots;

	    /* Find the real roots the easy way. */
	    if ((roots = C[ix].cf[1]*C[ix].cf[1]-4*C[ix].cf[0]*C[ix].cf[2]
		    ) < 0) {
		npts = 0;	/* no real roots */
	    } else {
		roots = sqrt(roots);
		k[0] = (roots - C[ix].cf[1]) * 0.5 / C[ix].cf[0];
		k[1] = (roots + C[ix].cf[1]) * (-0.5) / C[ix].cf[0];
		npts = 2;
	    }
	} else {
	    bn_complex_t val[4];	/* roots of final equation */
	    register int l;
	    register int nroots;

	    /* The equation is 4th order, so we expect 0 to 4 roots */
	    nroots = rt_poly_roots(&C[ix], val, (*stp)->st_dp->d_namep);

	    /* Only real roots indicate an intersection in real space.
	     *
	     * Look at each root returned; if the imaginary part is zero
	     * or sufficiently close, then use the real part as one value
	     * of 't' for the intersections
	     */
	    for (l=0, npts=0; l < nroots; l++) {
		if (NEAR_ZERO(val[l].im, 0.0001))
		    k[npts++] = val[l].re;
	    }
	    /* Here, 'npts' is number of points being returned */
	    if (npts != 0 && npts != 2 && npts != 4 && npts > 0) {
		bu_log("tgc:  reduced %d to %d roots\n", nroots, npts);
		bn_pr_roots("tgc", val, nroots);
	    } else if (nroots < 0) {
		static size_t reported = 0;

		if (reported < 10) {
		    bu_log("Root solver failed to converge on a solution for %s\n", stp[ix]->st_dp->d_namep);
		    /* these are printed in 'mm' regardless of local units */
		    VPRINT("\tshooting point (units mm): ", rp[ix]->r_pt);
		    VPRINT("\tshooting direction:        ", rp[ix]->r_dir);
		} else if (reported == 10) {
		    bu_log("Too many convergence failures.  Suppressing further TGC root finder reports.\n");
		}
		reported++;
	    }
	}

	/*
	 * Reverse above translation by adding distance to all 'k' values.
	 */
	for (i = 0; i < npts; ++i)
	    k[i] -= cor_proj;

	if (npts != 0 && npts != 2 && npts != 4) {
	    bu_log("tgc(%s):  %d intersects != {0, 2, 4}\n",
		   stp[ix]->st_name, npts);
	    RT_TGC_SEG_MISS(segp[ix]);		/* No hit */
	    continue;
	}

	/* Most distant to least distant */
	rt_pnt_sort(k, npts);

	/* Now, k[0] > k[npts-1] */

	/* General Cone may have 4 intersections, but Truncated Cone
	 * may only have 2.
	 */

	/* Truncation Procedure
	 *
	 * Determine whether any of the intersections found are
	 * between the planes truncating the cone.
	 */
	intersect = 0;
	for (i=0; i < npts; i++) {
	    /* segp[ix].seg_in.hit_normal holds dprime */
	    /* segp[ix].seg_out.hit_normal holds pprime */
	    zval = k[i]*segp[ix].seg_in.hit_normal[Z] +
		segp[ix].seg_out.hit_normal[Z];
	    /* Height vector is unitized (tgc->tgc_sH == 1.0) */
	    if (zval < 1.0 && zval > 0.0) {
		if (++intersect == 2) {
		    pt[T_IN] = k[i];
		}  else
		    pt[T_OUT] = k[i];
	    }
	}
	/* Reuse C to hold values of intersect and k. */
	C[ix].dgr = intersect;
	C[ix].cf[T_OUT] = pt[T_OUT];
	C[ix].cf[T_IN]  = pt[T_IN];
    }

    /* for each ray/cone pair */
    for (ix = 0; ix < n; ix++) {
	if (segp[ix].seg_stp == 0) continue; /* Skip */

	tgc = (struct tgc_specific *)stp[ix]->st_specific;
	intersect = C[ix].dgr;
	pt[T_OUT] = C[ix].cf[T_OUT];
	pt[T_IN]  = C[ix].cf[T_IN];
	/* segp[ix].seg_out.hit_normal holds pprime */
	VMOVE(pprime, segp[ix].seg_out.hit_normal);
	/* segp[ix].seg_in.hit_normal holds dprime */
	VMOVE(dprime, segp[ix].seg_in.hit_normal);

	if (intersect == 2) {
	    /* If two between-plane intersections exist, they are
	     * the hit points for the ray.
	     */
	    segp[ix].seg_in.hit_dist = pt[T_IN] * t_scale;
	    segp[ix].seg_in.hit_surfno = TGC_NORM_BODY;	/* compute N */
	    VJOIN1(segp[ix].seg_in.hit_vpriv, pprime, pt[T_IN], dprime);

	    segp[ix].seg_out.hit_dist = pt[T_OUT] * t_scale;
	    segp[ix].seg_out.hit_surfno = TGC_NORM_BODY;	/* compute N */
	    VJOIN1(segp[ix].seg_out.hit_vpriv, pprime, pt[T_OUT], dprime);
	} else if (intersect == 1) {
	    int nflag;
	    /*
	     * If only one between-plane intersection exists (pt[T_OUT]),
	     * then the other intersection must be on
	     * one of the planar surfaces (pt[T_IN]).
	     *
	     * Find which surface it lies on by calculating the
	     * X and Y values of the line as it intersects each
	     * plane (in the standard coordinate system), and test
	     * whether this lies within the governing ellipse.
	     */
	    if (ZERO(dprime[Z])) {
		RT_TGC_SEG_MISS(segp[ix]);
		continue;
	    }
	    b = (-pprime[Z])/dprime[Z];
	    /* Height vector is unitized (tgc->tgc_sH == 1.0) */
	    t = (1.0 - pprime[Z])/dprime[Z];

	    VJOIN1(work, pprime, b, dprime);
	    /* A and B vectors are unitized (tgc->tgc_A == _B == 1.0) */
	    /* alf1 = ALPHA(work[X], work[Y], 1.0, 1.0) */
	    alf1 = work[X]*work[X] + work[Y]*work[Y];

	    VJOIN1(work, pprime, t, dprime);
	    /* Must scale C and D vectors */
	    alf2 = ALPHA(work[X], work[Y], tgc->tgc_AAdCC, tgc->tgc_BBdDD);

	    if (alf1 <= 1.0) {
		pt[T_IN] = b;
		nflag = TGC_NORM_BOT; /* copy reverse normal */
	    } else if (alf2 <= 1.0) {
		pt[T_IN] = t;
		nflag = TGC_NORM_TOP;	/* copy normal */
	    } else {
		/* intersection apparently invalid */
		RT_TGC_SEG_MISS(segp[ix]);
		continue;
	    }

	    /* pt[T_OUT] on skin, pt[T_IN] on end */
	    if (pt[T_OUT] >= pt[T_IN]) {
		segp[ix].seg_in.hit_dist = pt[T_IN] * t_scale;
		segp[ix].seg_in.hit_surfno = nflag;

		segp[ix].seg_out.hit_dist = pt[T_OUT] * t_scale;
		segp[ix].seg_out.hit_surfno = TGC_NORM_BODY;	/* compute N */
		/* transform-space vector needed for normal */
		VJOIN1(segp[ix].seg_out.hit_vpriv, pprime, pt[T_OUT], dprime);
	    } else {
		segp[ix].seg_in.hit_dist = pt[T_OUT] * t_scale;
		/* transform-space vector needed for normal */
		segp[ix].seg_in.hit_surfno = TGC_NORM_BODY;	/* compute N */
		VJOIN1(segp[ix].seg_in.hit_vpriv, pprime, pt[T_OUT], dprime);

		segp[ix].seg_out.hit_dist = pt[T_IN] * t_scale;
		segp[ix].seg_out.hit_surfno = nflag;
	    }
	} else {

	    /* If all conic intersections lie outside the plane, then
	     * check to see whether there are two planar intersections
	     * inside the governing ellipses.
	     *
	     * But first, if the direction is parallel (or nearly so)
	     * to the planes, it (obviously) won't intersect either of
	     * them.
	     */
	    if (ZERO(dprime[Z])) {
		RT_TGC_SEG_MISS(segp[ix]);
		continue;
	    }

	    dir = VDOT(tgc->tgc_N, rp[ix]->r_dir);	/* direc */
	    if (NEAR_ZERO(dir, RT_DOT_TOL)) {
		RT_TGC_SEG_MISS(segp[ix]);
		continue;
	    }

	    b = (-pprime[Z])/dprime[Z];
	    /* Height vector is unitized (tgc->tgc_sH == 1.0) */
	    t = (1.0 - pprime[Z])/dprime[Z];

	    VJOIN1(work, pprime, b, dprime);
	    /* A and B vectors are unitized (tgc->tgc_A == _B == 1.0) */
	    /* alpf = ALPHA(work[0], work[1], 1.0, 1.0) */
	    alf1 = work[X]*work[X] + work[Y]*work[Y];

	    VJOIN1(work, pprime, t, dprime);
	    /* Must scale C and D vectors. */
	    alf2 = ALPHA(work[X], work[Y], tgc->tgc_AAdCC, tgc->tgc_BBdDD);

	    /* It should not be possible for one planar intersection
	     * to be outside its ellipse while the other is inside ...
	     * but I wouldn't take any chances.
	     */
	    if (alf1 > 1.0 || alf2 > 1.0) {
		RT_TGC_SEG_MISS(segp[ix]);
		continue;
	    }

	    /* Use the dot product (found earlier) of the plane normal
	     * with the direction vector to determine the orientation
	     * of the intersections.
	     */
	    if (dir > 0.0) {
		segp[ix].seg_in.hit_dist = b * t_scale;
		segp[ix].seg_in.hit_surfno = TGC_NORM_BOT;	/* reverse normal */

		segp[ix].seg_out.hit_dist = t * t_scale;
		segp[ix].seg_out.hit_surfno = TGC_NORM_TOP;	/* normal */
	    } else {
		segp[ix].seg_in.hit_dist = t * t_scale;
		segp[ix].seg_in.hit_surfno = TGC_NORM_TOP;	/* normal */

		segp[ix].seg_out.hit_dist = b * t_scale;
		segp[ix].seg_out.hit_surfno = TGC_NORM_BOT;	/* reverse normal */
	    }
	}
    } /* end for each ray/cone pair */
    bu_free((char *)C, "tor bn_poly_t");
}


/**
 * Sorts the values in t[] in descending order.
 *
 * TODO: CONVERT THIS TO A FIXED SIZE SORTING NETWORK
 */
void
rt_pnt_sort(fastf_t t[], int npts)
{
    fastf_t u;
    short lim, n;

    for (lim = npts-1; lim > 0; lim--) {
	for (n = 0; n < lim; n++) {
	    if ((u=t[n]) < t[n+1]) {
		/* bubble larger towards [0] */
		t[n] = t[n+1];
		t[n+1] = u;
	    }
	}
    }
}


/**
 * Compute the normal to the cone, given a point on the STANDARD CONE
 * centered at the origin of the X-Y plane.
 *
 * The gradient of the cone at that point is the normal vector in the
 * standard space.  This vector will need to be transformed back to
 * the coordinate system of the original cone in order to be useful.
 * Then the transformed vector must be 'unitized.'
 *
 * NOTE: The transformation required is NOT the inverse of the of the
 * rotation to the standard cone, due to the shear involved in the
 * mapping.  The inverse maps points back to points, but it is the
 * transpose which maps normals back to normals.  If you really want
 * to know why, talk to Ed Davisson or Peter Stiller.
 *
 * The equation for the standard cone *without* scaling is:
 * (rotated the sheared)
 *
 * f(X, Y, Z) =  X**2 * Q**2 + Y**2 * R**2 - R**2 * Q**2 = 0
 *
 * where:
 * R = a + ((c - a)/|H'|)*Z
 * Q = b + ((d - b)/|H'|)*Z
 *
 * When the equation is scaled so the A, B, and the sheared H are unit
 * length, as is done here, the equation can be coerced back into this
 * same form with R and Q now being:
 *
 * R = 1 + (c/a - 1)*Z
 * Q = 1 + (d/b - 1)*Z
 *
 * The gradient of f(x, y, z) = 0 is:
 *
 * df/dx = 2 * x * Q**2
 * df/dy = 2 * y * R**2
 * df/dz = x**2 * 2 * Q * dQ/dz + y**2 * 2 * R * dR/dz
 * - R**2 * 2 * Q * dQ/dz - Q**2 * 2 * R * dR/dz
 *	      = 2 [(x**2 - R**2) * Q * dQ/dz + (y**2 - Q**2) * R * dR/dz]
 *
 * where:
 * dR/dz = (c/a - 1)
 * dQ/dz = (d/b - 1)
 *
 * [in the *unscaled* case these would be:
 *  (c - a)/|H'| and (d - b)/|H'|]
 *
 * Since the gradient (normal) needs to be rescaled to unit length
 * after mapping back to absolute coordinates, we divide the 2 out of
 * the above expressions.
 */
void
rt_tgc_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
    register struct tgc_specific *tgc =
	(struct tgc_specific *)stp->st_specific;

    fastf_t Q;
    fastf_t R;
    vect_t stdnorm;

    /* Hit point */
    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);

    /* Hits on the end plates are easy */
    switch (hitp->hit_surfno) {
	case TGC_NORM_TOP:
	    VMOVE(hitp->hit_normal, tgc->tgc_N);
	    break;
	case TGC_NORM_BOT:
	    VREVERSE(hitp->hit_normal, tgc->tgc_N);
	    break;
	case TGC_NORM_BODY:
	    /* Compute normal, given hit point on standard (unit) cone */
	    R = 1 + tgc->tgc_CdAm1 * hitp->hit_vpriv[Z];
	    Q = 1 + tgc->tgc_DdBm1 * hitp->hit_vpriv[Z];
	    stdnorm[X] = hitp->hit_vpriv[X] * Q * Q;
	    stdnorm[Y] = hitp->hit_vpriv[Y] * R * R;
	    stdnorm[Z] = (hitp->hit_vpriv[X]*hitp->hit_vpriv[X] - R*R)
		* Q * tgc->tgc_DdBm1
		+ (hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y] - Q*Q)
		* R * tgc->tgc_CdAm1;
	    MAT4X3VEC(hitp->hit_normal, tgc->tgc_invRtShSc, stdnorm);
	    /*XXX - save scale */
	    VUNITIZE(hitp->hit_normal);
	    break;
	default:
	    bu_log("rt_tgc_norm: bad surfno=%d\n", hitp->hit_surfno);
	    break;
    }
}


void
rt_tgc_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
    struct tgc_specific *tgc = (struct tgc_specific *)stp->st_specific;

    vect_t work;
    vect_t pprime;
    fastf_t len;

    if (ap) RT_CK_APPLICATION(ap);

    /* hit_point is on surface;  project back to unit cylinder,
     * creating a vector from vertex to hit point.
     */
    VSUB2(work, hitp->hit_point, tgc->tgc_V);
    MAT4X3VEC(pprime, tgc->tgc_ScShR, work);

    switch (hitp->hit_surfno) {
	case TGC_NORM_BODY:
	    /* scale coords to unit circle (they are already scaled by bottom plate radii) */
	    pprime[X] *= tgc->tgc_A / (tgc->tgc_A*(1.0 - pprime[Z]) + tgc->tgc_C*pprime[Z]);
	    pprime[Y] *= tgc->tgc_B / (tgc->tgc_B*(1.0 - pprime[Z]) + tgc->tgc_D*pprime[Z]);
	    uvp->uv_u = atan2(pprime[Y], pprime[X]) / M_2PI + 0.5;
	    uvp->uv_v = pprime[Z];		/* height */
	    break;
	case TGC_NORM_TOP:
	    /* top plate */
	    /* scale coords to unit circle (they are already scaled by bottom plate radii) */
	    pprime[X] *= tgc->tgc_A / tgc->tgc_C;
	    pprime[Y] *= tgc->tgc_B / tgc->tgc_D;
	    uvp->uv_u = atan2(pprime[Y], pprime[X]) / M_2PI + 0.5;
	    len = sqrt(pprime[X]*pprime[X]+pprime[Y]*pprime[Y]);
	    uvp->uv_v = len;		/* rim v = 1 */
	    break;
	case TGC_NORM_BOT:
	    /* bottom plate */
	    len = sqrt(pprime[X]*pprime[X]+pprime[Y]*pprime[Y]);
	    uvp->uv_u = atan2(pprime[Y], pprime[X]) / M_2PI + 0.5;
	    uvp->uv_v = 1 - len;	/* rim v = 0 */
	    break;
    }

    if (uvp->uv_u < 0.0)
	uvp->uv_u = 0.0;
    else if (uvp->uv_u > 1.0)
	uvp->uv_u = 1.0;
    if (uvp->uv_v < 0.0)
	uvp->uv_v = 0.0;
    else if (uvp->uv_v > 1.0)
	uvp->uv_v = 1.0;

    /* uv_du should be relative to rotation, uv_dv relative to height */
    uvp->uv_du = uvp->uv_dv = 0;
}


void
rt_tgc_free(struct soltab *stp)
{
    register struct tgc_specific *tgc =
	(struct tgc_specific *)stp->st_specific;

    BU_PUT(tgc, struct tgc_specific);
}


/**
 * Import a TGC from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_tgc_import4(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_tgc_internal *tip;
    union record *rp;
    fastf_t vec[3*6];

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != ID_SOLID) {
	bu_log("rt_tgc_import4: defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_TGC;
    ip->idb_meth = &OBJ[ID_TGC];
    BU_ALLOC(ip->idb_ptr, struct rt_tgc_internal);

    tip = (struct rt_tgc_internal *)ip->idb_ptr;
    tip->magic = RT_TGC_INTERNAL_MAGIC;

    /* Convert from database to internal format */
    flip_fastf_float(vec, rp->s.s_values, 6, (dbip && dbip->dbi_version < 0) ? 1 : 0);

    /* Apply modeling transformations */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(tip->v, mat, &vec[0*3]);
    MAT4X3VEC(tip->h, mat, &vec[1*3]);
    MAT4X3VEC(tip->a, mat, &vec[2*3]);
    MAT4X3VEC(tip->b, mat, &vec[3*3]);
    MAT4X3VEC(tip->c, mat, &vec[4*3]);
    MAT4X3VEC(tip->d, mat, &vec[5*3]);

    return 0;		/* OK */
}


int
rt_tgc_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_tgc_internal *tip;
    union record *rec;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_TGC && ip->idb_type != ID_REC) return -1;
    tip = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "tgc external");
    rec = (union record *)ep->ext_buf;

    rec->s.s_id = ID_SOLID;
    rec->s.s_type = GENTGC;

    /* NOTE: This also converts to dbfloat_t */
    VSCALE(&rec->s.s_values[0], tip->v, local2mm);
    VSCALE(&rec->s.s_values[3], tip->h, local2mm);
    VSCALE(&rec->s.s_values[6], tip->a, local2mm);
    VSCALE(&rec->s.s_values[9], tip->b, local2mm);
    VSCALE(&rec->s.s_values[12], tip->c, local2mm);
    VSCALE(&rec->s.s_values[15], tip->d, local2mm);

    return 0;
}


/**
 * Import a TGC from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_tgc_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_tgc_internal *tip;

    /* must be double for import and export */
    double vec[ELEMENTS_PER_VECT*6];

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    BU_ASSERT(ep->ext_nbytes == SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT*6);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_TGC;
    ip->idb_meth = &OBJ[ID_TGC];
    BU_ALLOC(ip->idb_ptr, struct rt_tgc_internal);

    tip = (struct rt_tgc_internal *)ip->idb_ptr;
    tip->magic = RT_TGC_INTERNAL_MAGIC;

    /* Convert from database (network) to internal (host) format */
    bu_cv_ntohd((unsigned char *)vec, ep->ext_buf, ELEMENTS_PER_VECT*6);

    /* Apply modeling transformations */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT(tip->v, mat, &vec[0*ELEMENTS_PER_VECT]);
    MAT4X3VEC(tip->h, mat, &vec[1*ELEMENTS_PER_VECT]);
    MAT4X3VEC(tip->a, mat, &vec[2*ELEMENTS_PER_VECT]);
    MAT4X3VEC(tip->b, mat, &vec[3*ELEMENTS_PER_VECT]);
    MAT4X3VEC(tip->c, mat, &vec[4*ELEMENTS_PER_VECT]);
    MAT4X3VEC(tip->d, mat, &vec[5*ELEMENTS_PER_VECT]);

    return 0;		/* OK */
}


int
rt_tgc_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_tgc_internal *tip;

    /* must be double for import and export */
    double vec[ELEMENTS_PER_VECT*6];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_TGC && ip->idb_type != ID_REC) return -1;
    tip = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT*6;
    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "tgc external");

    /* scale 'em into local buffer */
    VSCALE(&vec[0*ELEMENTS_PER_VECT], tip->v, local2mm);
    VSCALE(&vec[1*ELEMENTS_PER_VECT], tip->h, local2mm);
    VSCALE(&vec[2*ELEMENTS_PER_VECT], tip->a, local2mm);
    VSCALE(&vec[3*ELEMENTS_PER_VECT], tip->b, local2mm);
    VSCALE(&vec[4*ELEMENTS_PER_VECT], tip->c, local2mm);
    VSCALE(&vec[5*ELEMENTS_PER_VECT], tip->d, local2mm);

    /* Convert from internal (host) to database (network) format */
    bu_cv_htond(ep->ext_buf, (unsigned char *)vec, ELEMENTS_PER_VECT*6);

    return 0;
}


/**
 * Make human-readable formatted presentation of this solid.
 * First line describes type of solid.
 * Additional lines are indented one tab, and give parameter values.
 */
int
rt_tgc_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_tgc_internal *tip = (struct rt_tgc_internal *)ip->idb_ptr;
    char buf[256];
    double angles[5];
    vect_t unitv;
    fastf_t Hmag;

    RT_TGC_CK_MAGIC(tip);
    bu_vls_strcat(str, "truncated general cone (TGC)\n");

    if (!verbose)
	return 0;

    sprintf(buf, "\tV (%g, %g, %g)\n",
	    INTCLAMP(tip->v[X] * mm2local),
	    INTCLAMP(tip->v[Y] * mm2local),
	    INTCLAMP(tip->v[Z] * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tTop (%g, %g, %g)\n",
	    INTCLAMP((tip->v[X] + tip->h[X]) * mm2local),
	    INTCLAMP((tip->v[Y] + tip->h[Y]) * mm2local),
	    INTCLAMP((tip->v[Z] + tip->h[Z]) * mm2local));
    bu_vls_strcat(str, buf);

    Hmag = MAGNITUDE(tip->h);
    sprintf(buf, "\tH (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->h[X] * mm2local),
	    INTCLAMP(tip->h[Y] * mm2local),
	    INTCLAMP(tip->h[Z] * mm2local),
	    INTCLAMP(Hmag * mm2local));
    bu_vls_strcat(str, buf);
    if (Hmag < VDIVIDE_TOL) {
	bu_vls_strcat(str, "H vector is zero!\n");
    } else {
	register double f = 1/Hmag;
	VSCALE(unitv, tip->h, f);
	rt_find_fallback_angle(angles, unitv);
	rt_pr_fallback_angle(str, "\tH", angles);
    }

    sprintf(buf, "\tA (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->a[X] * mm2local),
	    INTCLAMP(tip->a[Y] * mm2local),
	    INTCLAMP(tip->a[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(tip->a) * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tB (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->b[X] * mm2local),
	    INTCLAMP(tip->b[Y] * mm2local),
	    INTCLAMP(tip->b[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(tip->b) * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tC (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->c[X] * mm2local),
	    INTCLAMP(tip->c[Y] * mm2local),
	    INTCLAMP(tip->c[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(tip->c) * mm2local));
    bu_vls_strcat(str, buf);

    sprintf(buf, "\tD (%g, %g, %g) mag=%g\n",
	    INTCLAMP(tip->d[X] * mm2local),
	    INTCLAMP(tip->d[Y] * mm2local),
	    INTCLAMP(tip->d[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(tip->d) * mm2local));
    bu_vls_strcat(str, buf);

    VCROSS(unitv, tip->c, tip->d);
    VUNITIZE(unitv);
    rt_find_fallback_angle(angles, unitv);
    rt_pr_fallback_angle(str, "\tAxB", angles);

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_tgc_ifree(struct rt_db_internal *ip)
{
    RT_CK_DB_INTERNAL(ip);

    bu_free(ip->idb_ptr, "tgc ifree");
    ip->idb_ptr = ((void *)0);
}


struct ellipse {
    point_t center;
    vect_t axis_a;
    vect_t axis_b;
};


static void
draw_lines_between_rec_ellipses(
    struct bu_list *vlfree,
    struct bu_list *vhead,
    struct ellipse ellipse1,
    vect_t h,
    int num_lines)
{
    int i;
    point_t ellipse1_point, ellipse2_point;
    fastf_t radian_step = M_2PI / num_lines;

    for (i = 0; i < num_lines; ++i) {
	ellipse_point_at_radian(ellipse1_point, ellipse1.center,
				ellipse1.axis_a, ellipse1.axis_b, i * radian_step);
	VADD2(ellipse2_point, ellipse1_point, h);

	BV_ADD_VLIST(vlfree, vhead, ellipse1_point, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, ellipse2_point, BV_VLIST_LINE_DRAW);
    }
}


static void
draw_lines_between_ellipses(
    struct bu_list *vlfree,
    struct bu_list *vhead,
    struct ellipse ellipse1,
    struct ellipse ellipse2,
    int num_lines)
{
    int i;
    point_t ellipse1_point, ellipse2_point;
    fastf_t radian_step = M_2PI / num_lines;

    for (i = 0; i < num_lines; ++i) {
	ellipse_point_at_radian(ellipse1_point, ellipse1.center,
				ellipse1.axis_a, ellipse1.axis_b, i * radian_step);
	ellipse_point_at_radian(ellipse2_point, ellipse2.center,
				ellipse2.axis_a, ellipse2.axis_b, i * radian_step);

	BV_ADD_VLIST(vlfree, vhead, ellipse1_point, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, ellipse2_point, BV_VLIST_LINE_DRAW);
    }
}


static int
tgc_points_per_ellipse(const struct rt_db_internal *ip, fastf_t point_spacing)
{
    struct rt_tgc_internal *tgc;
    fastf_t avg_radius, avg_circumference;
    fastf_t tgc_mag_a, tgc_mag_b, tgc_mag_c, tgc_mag_d;

    RT_CK_DB_INTERNAL(ip);
    tgc = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    tgc_mag_a = MAGNITUDE(tgc->a);
    tgc_mag_b = MAGNITUDE(tgc->b);
    tgc_mag_c = MAGNITUDE(tgc->c);
    tgc_mag_d = MAGNITUDE(tgc->d);

    avg_radius = (tgc_mag_a + tgc_mag_b + tgc_mag_c + tgc_mag_d) / 4.0;
    avg_circumference = M_2PI * avg_radius;

    return avg_circumference / point_spacing;
}


static fastf_t
ramanujan_approx_circumference(fastf_t major_len, fastf_t minor_len)
{
    fastf_t a = major_len, b = minor_len;

    return M_PI * (3.0 * (a + b) - sqrt(10.0 * a * b + 3.0 * (a * a + b * b)));
}


static int
tgc_connecting_lines(
    const struct rt_tgc_internal *tgc,
    fastf_t curve_scale,
    fastf_t s_size)
{
    fastf_t mag_a, mag_b, mag_c, mag_d, avg_circumference;

    mag_a = MAGNITUDE(tgc->a);
    mag_b = MAGNITUDE(tgc->b);
    mag_c = MAGNITUDE(tgc->c);
    mag_d = MAGNITUDE(tgc->d);

    avg_circumference = ramanujan_approx_circumference(mag_a, mag_b);
    avg_circumference += ramanujan_approx_circumference(mag_c, mag_d);
    avg_circumference /= 2.0;

    fastf_t curve_spacing = s_size / 2.0;
    curve_spacing /= curve_scale;
    return avg_circumference / curve_spacing;
}



int
rt_tgc_adaptive_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bn_tol *tol, const struct bview *v, fastf_t s_size)
{
    int points_per_ellipse, connecting_lines;
    struct rt_tgc_internal *tip;
    struct ellipse ellipse1, ellipse2;
    fastf_t point_spacing;
    fastf_t avg_diameter;
    fastf_t tgc_mag_a, tgc_mag_b, tgc_mag_c, tgc_mag_d;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    tip = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tip);

    tgc_mag_a = MAGNITUDE(tip->a);
    tgc_mag_b = MAGNITUDE(tip->b);
    tgc_mag_c = MAGNITUDE(tip->c);
    tgc_mag_d = MAGNITUDE(tip->d);
    avg_diameter = tgc_mag_a + tgc_mag_b + tgc_mag_c + tgc_mag_d;
    avg_diameter /= 2.0;
    point_spacing = solid_point_spacing(v, avg_diameter);

    points_per_ellipse = tgc_points_per_ellipse(ip, point_spacing);

    if (points_per_ellipse < 6) {
	point_t p;

	VADD2(p, tip->v, tip->h);
	BV_ADD_VLIST(vlfree, vhead, tip->v, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, p, BV_VLIST_LINE_DRAW);

	return 0;
    }

    connecting_lines = tgc_connecting_lines(tip, v->gv_s->curve_scale, s_size);

    if (connecting_lines < 4) {
	connecting_lines = 4;
    }

    VMOVE(ellipse1.center, tip->v);
    VMOVE(ellipse1.axis_a, tip->a);
    VMOVE(ellipse1.axis_b, tip->b);

    VADD2(ellipse2.center, tip->v, tip->h);
    VMOVE(ellipse2.axis_a, tip->c);
    VMOVE(ellipse2.axis_b, tip->d);

    /* looks like a right elliptical cylinder */
    if (VNEAR_EQUAL(tip->a, tip->c, tol->dist) &&
	VNEAR_EQUAL(tip->b, tip->d, tol->dist))
    {
	int i;
	point_t *pts;
	fastf_t radian, radian_step;

	pts = (point_t *)bu_malloc(sizeof(point_t) * points_per_ellipse,
				   "tgc points");

	radian_step = M_2PI / points_per_ellipse;

	/* calculate and plot first ellipse */
	ellipse_point_at_radian(pts[0], tip->v, tip->a, tip->b,
				radian_step * (points_per_ellipse - 1));
	BV_ADD_VLIST(vlfree, vhead, pts[0], BV_VLIST_LINE_MOVE);

	radian = 0;
	for (i = 0; i < points_per_ellipse; ++i) {
	    ellipse_point_at_radian(pts[i], tip->v, tip->a, tip->b, radian);
	    BV_ADD_VLIST(vlfree, vhead, pts[i], BV_VLIST_LINE_DRAW);

	    radian += radian_step;
	}

	/* calculate and plot second ellipse */
	for (i = 0; i < points_per_ellipse; ++i) {
	    VADD2(pts[i], tip->h, pts[i]);
	}

	BV_ADD_VLIST(vlfree, vhead, pts[points_per_ellipse - 1], BV_VLIST_LINE_MOVE);
	for (i = 0; i < points_per_ellipse; ++i) {
	    BV_ADD_VLIST(vlfree, vhead, pts[i], BV_VLIST_LINE_DRAW);
	}

	bu_free(pts, "tgc points");

	draw_lines_between_rec_ellipses(vlfree, vhead, ellipse1, tip->h,
					connecting_lines);
    } else {
	plot_ellipse(vlfree, vhead, ellipse1.center, ellipse1.axis_a, ellipse1.axis_b,
		     points_per_ellipse);

	plot_ellipse(vlfree, vhead, ellipse2.center, ellipse2.axis_a, ellipse2.axis_b,
		     points_per_ellipse);

	draw_lines_between_ellipses(vlfree, vhead, ellipse1, ellipse2,
				    connecting_lines);
    }

    return 0;
}


int
rt_tgc_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    struct rt_tgc_internal *tip;
    register int i;
    fastf_t top[16*ELEMENTS_PER_VECT];
    fastf_t bottom[16*ELEMENTS_PER_VECT];
    vect_t work;		/* Vec addition work area */

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    struct bu_list *vlfree = &RTG.rtg_vlfree;
    tip = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tip);

    rt_ell_16pnts(bottom, tip->v, tip->a, tip->b);
    VADD2(work, tip->v, tip->h);
    rt_ell_16pnts(top, work, tip->c, tip->d);

    /* Draw the top */
    BV_ADD_VLIST(vlfree, vhead, &top[15*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i=0; i<16; i++) {
	BV_ADD_VLIST(vlfree, vhead, &top[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    /* Draw the bottom */
    BV_ADD_VLIST(vlfree, vhead, &bottom[15*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
    for (i=0; i<16; i++) {
	BV_ADD_VLIST(vlfree, vhead, &bottom[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }

    /* Draw connections */
    for (i=0; i<16; i += 4) {
	BV_ADD_VLIST(vlfree, vhead, &top[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, &bottom[i*ELEMENTS_PER_VECT], BV_VLIST_LINE_DRAW);
    }
    return 0;
}


/**
 * Return the curvature of the TGC.
 */
void
rt_tgc_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
    register struct tgc_specific *tgc =
	(struct tgc_specific *)stp->st_specific;
    fastf_t R, Q, R2, Q2;
    mat_t M, dN, mtmp;
    vect_t gradf, tmp, u, v;
    fastf_t a, b, c, scale;
    vect_t vec1, vec2;

    if (hitp->hit_surfno != TGC_NORM_BODY) {
	/* We hit an end plate.  Choose any tangent vector. */
	bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
	cvp->crv_c1 = cvp->crv_c2 = 0;
	return;
    }

    R = 1 + tgc->tgc_CdAm1 * hitp->hit_vpriv[Z];
    Q = 1 + tgc->tgc_DdBm1 * hitp->hit_vpriv[Z];
    R2 = R*R;
    Q2 = Q*Q;

    /*
     * Compute derivatives of the gradient (normal) field
     * in ideal coords.  This is a symmetric matrix with
     * the columns (dNx, dNy, dNz).
     */
    MAT_IDN(dN);
    dN[0] = Q2;
    dN[2] = dN[8] = 2.0*Q*tgc->tgc_DdBm1 * hitp->hit_vpriv[X];
    dN[5] = R2;
    dN[6] = dN[9] = 2.0*R*tgc->tgc_CdAm1 * hitp->hit_vpriv[Y];
    dN[10] = tgc->tgc_DdBm1*tgc->tgc_DdBm1 * hitp->hit_vpriv[X]*hitp->hit_vpriv[X]
	+ tgc->tgc_CdAm1*tgc->tgc_CdAm1 * hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y]
	- tgc->tgc_DdBm1*tgc->tgc_DdBm1 * R2
	- tgc->tgc_CdAm1*tgc->tgc_CdAm1 * Q2
	- 4.0*tgc->tgc_CdAm1*tgc->tgc_DdBm1 * R*Q;

    /* M = At * dN * A */
    bn_mat_mul(mtmp, dN, tgc->tgc_ScShR);
    bn_mat_mul(M, tgc->tgc_invRtShSc, mtmp);

    /* XXX - determine the scaling */
    gradf[X] = Q2 * hitp->hit_vpriv[X];
    gradf[Y] = R2 * hitp->hit_vpriv[Y];
    gradf[Z] = (hitp->hit_vpriv[X]*hitp->hit_vpriv[X] - R2) * Q * tgc->tgc_DdBm1 +
	(hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y] - Q2) * R * tgc->tgc_CdAm1;
    MAT4X3VEC(tmp, tgc->tgc_invRtShSc, gradf);
    scale = -1.0 / MAGNITUDE(tmp);
    /* XXX */

    /*
     * choose a tangent plane coordinate system
     * (u, v, normal) form a right-handed triple
     */
    bn_vec_ortho(u, hitp->hit_normal);
    VCROSS(v, hitp->hit_normal, u);

    /* find the second fundamental form */
    MAT4X3VEC(tmp, M, u);
    a = VDOT(u, tmp) * scale;
    b = VDOT(v, tmp) * scale;
    MAT4X3VEC(tmp, M, v);
    c = VDOT(v, tmp) * scale;

    bn_eigen2x2(&cvp->crv_c1, &cvp->crv_c2, vec1, vec2, a, b, c);
    VCOMB2(cvp->crv_pdir, vec1[X], u, vec1[Y], v);
    VUNITIZE(cvp->crv_pdir);
}


int
rt_tgc_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


void
rt_tgc_volume(fastf_t *vol, const struct rt_db_internal *ip)
{
    int tgc_type = 0;
    fastf_t mag_a, mag_b, mag_c, mag_d, mag_h;
    struct rt_tgc_internal *tip = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tip);

    mag_a = MAGNITUDE(tip->a);
    mag_b = MAGNITUDE(tip->b);
    mag_c = MAGNITUDE(tip->c);
    mag_d = MAGNITUDE(tip->d);
    mag_h = MAGNITUDE(tip->h);

    GET_TGC_TYPE(tgc_type, mag_a, mag_b, mag_c, mag_d);

    switch (tgc_type) {
	case RCC:
	case REC:
	    *vol = M_PI * mag_h * mag_a * mag_b;
	    break;
	case TRC:
	    /* TRC could fall through, but this formula avoids a sqrt and
	     * so will probably be more accurate */
	    *vol = M_PI * mag_h * (mag_a * mag_a + mag_c * mag_c + mag_a * mag_c) / 3.0;
	    break;
	case TEC:
	    *vol = M_PI * mag_h * (mag_a * mag_b + mag_c * mag_d + sqrt(mag_a * mag_b * mag_c * mag_d)) / 3.0;
	    break;
	default:
	    /* never reached */
	    bu_log("rt_tgc_volume(): cannot find volume\n");
    }
}


void
rt_tgc_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    int tgc_type = 0;
    fastf_t mag_a, mag_b, mag_c, mag_d, mag_h;
    fastf_t magsq_a, magsq_c, magsq_h;
    fastf_t c;
    fastf_t area_base;
    struct rt_tgc_internal *tip = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tip);

    magsq_a = MAGSQ(tip->a);
    magsq_c = MAGSQ(tip->c);
    magsq_h = MAGSQ(tip->h);

    mag_a = sqrt(magsq_a);
    mag_b = MAGNITUDE(tip->b);
    mag_c = sqrt(magsq_c);
    mag_d = MAGNITUDE(tip->d);
    mag_h = sqrt(magsq_h);

    GET_TGC_TYPE(tgc_type, mag_a, mag_b, mag_c, mag_d);

    switch (tgc_type) {
	case RCC:
	    *area = M_2PI * mag_a * (mag_a + mag_h);
	    break;
	case TRC:
	    *area = M_PI * ((mag_a + mag_c) * sqrt((mag_a - mag_c) * (mag_a - mag_c) + magsq_h) + magsq_a + magsq_c);
	    break;
	case REC:
	    area_base = M_PI * mag_a * mag_b;
	    /* approximation */
	    c = ELL_CIRCUMFERENCE(mag_a, mag_b);
	    *area = c * mag_h + 2.0 * area_base;
	    break;
	case TEC:
	default:
	    bu_log("rt_tgc_surf_area(): cannot find surface area\n");
    }
}


void
rt_tgc_centroid(point_t *cent, const struct rt_db_internal *ip)
{
    int tgc_type = 0;
    fastf_t mag_a, mag_b, mag_c, mag_d;
    fastf_t magsq_a, magsq_c;
    fastf_t scalar;
    struct rt_tgc_internal *tip = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tip);

    magsq_a = MAGSQ(tip->a);
    magsq_c = MAGSQ(tip->c);

    mag_a = sqrt(magsq_a);
    mag_b = MAGNITUDE(tip->b);
    mag_c = sqrt(magsq_c);
    mag_d = MAGNITUDE(tip->d);

    GET_TGC_TYPE(tgc_type, mag_a, mag_b, mag_c, mag_d);

    switch (tgc_type) {
	case RCC:
	case REC:
	    scalar = 0.5;
	    VJOIN1(*cent, tip->v, scalar, tip->h);
	    break;
	case TRC:
	    scalar = 0.25 * (magsq_a + 2.0 * mag_a * mag_c + 3.0 * magsq_c) /
		(magsq_a + mag_a * mag_c + magsq_c);
	    VJOIN1(*cent, tip->v, scalar, tip->h);
	    break;
	case TEC:
	    /* need to confirm formula */
	default:
	    bu_log("rt_tgc_centroid(): cannot find centroid\n");
    }
}

void
rt_tgc_labels(struct bv_scene_obj *ps, const struct rt_db_internal *ip)
{
    if (!ps || !ip)
	return;

    struct rt_tgc_internal *tgc = (struct rt_tgc_internal *)ip->idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    // Set up the containers
    struct bv_label *l[5];
    for (int i = 0; i < 5; i++) {
	struct bv_scene_obj *s = bv_obj_get_child(ps);
	struct bv_label *la;
	BU_GET(la, struct bv_label);
	s->s_i_data = (void *)la;

	BU_LIST_INIT(&(s->s_vlist));
	VSET(s->s_color, 255, 255, 0);
	s->s_type_flags |= BV_DBOBJ_BASED;
	s->s_type_flags |= BV_LABELS;
	BU_VLS_INIT(&la->label);

	l[i] = la;
    }

    // Do the specific data assignments for each label
    bu_vls_sprintf(&l[0]->label, "V");
    VMOVE(l[0]->p, tgc->v);

    bu_vls_sprintf(&l[1]->label, "A");
    VADD2(l[1]->p, tgc->v, tgc->a);

    bu_vls_sprintf(&l[2]->label, "B");
    VADD2(l[2]->p, tgc->v, tgc->b);

    bu_vls_sprintf(&l[3]->label, "C");
    VADD3(l[3]->p, tgc->v, tgc->h, tgc->c);

    bu_vls_sprintf(&l[4]->label, "D");
    VADD3(l[4]->p, tgc->v, tgc->h, tgc->d);
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
