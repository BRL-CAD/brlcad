/*                           T C L . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2014 United States Government as represented by
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

/** @addtogroup bntcl */
/** @{ */
/** @file libbn/tcl.c
 *
 * @brief
 * Tcl interfaces to all the LIBBN math routines.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "tcl.h"

#include "bu/str.h"
#include "vmath.h"
#include "bn.h"


int
bn_decode_mat(fastf_t *mat, const char *str)
{
    double m[16];
    int ret;

    if (BU_STR_EQUAL(str, "I")) {
	MAT_IDN(m);
	return 16;
    }
    if (*str == '{') str++;

    ret = sscanf(str,
		 "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
		 &m[0], &m[1], &m[2], &m[3], &m[4], &m[5], &m[6], &m[7],
		 &m[8], &m[9], &m[10], &m[11], &m[12], &m[13], &m[14], &m[15]);
    MAT_COPY(mat, m);

    return ret;
}


int
bn_decode_quat(fastf_t *quat, const char *str)
{
    double q[4];
    int ret;

    if (*str == '{') str++;
    ret = sscanf(str, "%lf %lf %lf %lf", &q[0], &q[1], &q[2], &q[3]);
    HMOVE(quat, q);

    return ret;
}


int
bn_decode_vect(fastf_t *vec, const char *str)
{
    double v[3];
    int ret;

    if (*str == '{') str++;
    ret = sscanf(str, "%lf %lf %lf", &v[0], &v[1], &v[2]);
    VMOVE(vec, v);

    return ret;
}


int
bn_decode_hvect(fastf_t *v, const char *str)
{
    return bn_decode_quat(v, str);
}


void
bn_encode_mat(struct bu_vls *vp, const mat_t m)
{
    if (m == NULL) {
	bu_vls_putc(vp, 'I');
	return;
    }

    bu_vls_printf(vp, "%g %g %g %g  %g %g %g %g  %g %g %g %g  %g %g %g %g",
		  INTCLAMP(m[0]), INTCLAMP(m[1]), INTCLAMP(m[2]), INTCLAMP(m[3]),
		  INTCLAMP(m[4]), INTCLAMP(m[5]), INTCLAMP(m[6]), INTCLAMP(m[7]),
		  INTCLAMP(m[8]), INTCLAMP(m[9]), INTCLAMP(m[10]), INTCLAMP(m[11]),
		  INTCLAMP(m[12]), INTCLAMP(m[13]), INTCLAMP(m[14]), INTCLAMP(m[15]));
}


void
bn_encode_quat(struct bu_vls *vp, const mat_t q)
{
    bu_vls_printf(vp, "%g %g %g %g", V4INTCLAMPARGS(q));
}


void
bn_encode_vect(struct bu_vls *vp, const mat_t v)
{
    bu_vls_printf(vp, "%g %g %g", V3INTCLAMPARGS(v));
}


void
bn_encode_hvect(struct bu_vls *vp, const mat_t v)
{
    bu_vls_printf(vp, "%g %g %g %g", V4INTCLAMPARGS(v));
}


void
bn_quat_distance_wrapper(double *dp, mat_t q1, mat_t q2)
{
    *dp = quat_distance(q1, q2);
}


void
bn_mat_scale_about_pt_wrapper(int *statusp, mat_t mat, const point_t pt, const double scale)
{
    *statusp = bn_mat_scale_about_pt(mat, pt, scale);
}


static void
bn_mat4x3pnt(fastf_t *o, mat_t m, point_t i)
{
    MAT4X3PNT(o, m, i);
}


static void
bn_mat4x3vec(fastf_t *o, mat_t m, vect_t i)
{
    MAT4X3VEC(o, m, i);
}


static void
bn_hdivide(fastf_t *o, const mat_t i)
{
    HDIVIDE(o, i);
}


static void
bn_vjoin1(fastf_t *o, const point_t pnt, double scale, const vect_t dir)
{
    VJOIN1(o, pnt, scale, dir);
}


static void bn_vblend(mat_t a, fastf_t b, mat_t c, fastf_t d, mat_t e)
{
    VBLEND2(a, b, c, d, e);
}


#define MATH_FUNC_VOID_CAST(_func) ((void (*)(void))_func)

static struct math_func_link {
    const char *name;
    void (*func)(void);
} math_funcs[] = {
    {"bn_dist_pt2_lseg2",	 MATH_FUNC_VOID_CAST(bn_dist_pt2_lseg2)},
    {"bn_isect_line2_line2",	 MATH_FUNC_VOID_CAST(bn_isect_line2_line2)},
    {"bn_isect_line3_line3",	 MATH_FUNC_VOID_CAST(bn_isect_line3_line3)},
    {"mat_mul",                  MATH_FUNC_VOID_CAST(bn_mat_mul)},
    {"mat_inv",                  MATH_FUNC_VOID_CAST(bn_mat_inv)},
    {"mat_trn",                  MATH_FUNC_VOID_CAST(bn_mat_trn)},
    {"matXvec",                  MATH_FUNC_VOID_CAST(bn_matXvec)},
    {"mat4x3vec",                MATH_FUNC_VOID_CAST(bn_mat4x3vec)},
    {"mat4x3pnt",                MATH_FUNC_VOID_CAST(bn_mat4x3pnt)},
    {"hdivide",                  MATH_FUNC_VOID_CAST(bn_hdivide)},
    {"vjoin1",	                 MATH_FUNC_VOID_CAST(bn_vjoin1)},
    {"vblend",	                 MATH_FUNC_VOID_CAST(bn_vblend)},
    {"mat_ae",                   MATH_FUNC_VOID_CAST(bn_mat_ae)},
    {"mat_ae_vec",               MATH_FUNC_VOID_CAST(bn_ae_vec)},
    {"mat_aet_vec",              MATH_FUNC_VOID_CAST(bn_aet_vec)},
    {"mat_angles",               MATH_FUNC_VOID_CAST(bn_mat_angles)},
    {"mat_eigen2x2",             MATH_FUNC_VOID_CAST(bn_eigen2x2)},
    {"mat_fromto",               MATH_FUNC_VOID_CAST(bn_mat_fromto)},
    {"mat_xrot",                 MATH_FUNC_VOID_CAST(bn_mat_xrot)},
    {"mat_yrot",                 MATH_FUNC_VOID_CAST(bn_mat_yrot)},
    {"mat_zrot",                 MATH_FUNC_VOID_CAST(bn_mat_zrot)},
    {"mat_lookat",               MATH_FUNC_VOID_CAST(bn_mat_lookat)},
    {"mat_vec_ortho",            MATH_FUNC_VOID_CAST(bn_vec_ortho)},
    {"mat_vec_perp",             MATH_FUNC_VOID_CAST(bn_vec_perp)},
    {"mat_scale_about_pt",       MATH_FUNC_VOID_CAST(bn_mat_scale_about_pt_wrapper)},
    {"mat_xform_about_pt",       MATH_FUNC_VOID_CAST(bn_mat_xform_about_pt)},
    {"mat_arb_rot",              MATH_FUNC_VOID_CAST(bn_mat_arb_rot)},
    {"quat_mat2quat",            MATH_FUNC_VOID_CAST(quat_mat2quat)},
    {"quat_quat2mat",            MATH_FUNC_VOID_CAST(quat_quat2mat)},
    {"quat_distance",            MATH_FUNC_VOID_CAST(bn_quat_distance_wrapper)},
    {"quat_double",              MATH_FUNC_VOID_CAST(quat_double)},
    {"quat_bisect",              MATH_FUNC_VOID_CAST(quat_bisect)},
    {"quat_slerp",               MATH_FUNC_VOID_CAST(quat_slerp)},
    {"quat_sberp",               MATH_FUNC_VOID_CAST(quat_sberp)},
    {"quat_make_nearest",        MATH_FUNC_VOID_CAST(quat_make_nearest)},
    {"quat_exp",                 MATH_FUNC_VOID_CAST(quat_exp)},
    {"quat_log",                 MATH_FUNC_VOID_CAST(quat_log)},
    {0, 0}
};


/**
 *@brief
 * Tcl wrappers for the math functions.
 *
 * This is where you should put clauses, in the below "if" statement, to add
 * Tcl support for the LIBBN math routines.
 */
int
bn_math_cmd(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    void (*math_func)(void);
    struct bu_vls result = BU_VLS_INIT_ZERO;
    struct math_func_link *mfl;

    mfl = (struct math_func_link *)clientData;
    math_func = mfl->func;

    if (math_func == MATH_FUNC_VOID_CAST(bn_mat_mul)) {
	mat_t o, a, b;
	if (argc < 3 || bn_decode_mat(a, argv[1]) < 16 ||
	    bn_decode_mat(b, argv[2]) < 16) {
	    bu_vls_printf(&result, "usage: %s matA matB", argv[0]);
	    goto error;
	}
	bn_mat_mul(o, a, b);
	bn_encode_mat(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_mat_inv)
	       || math_func == MATH_FUNC_VOID_CAST(bn_mat_trn)) {
	mat_t o, a;
	/* need new math func pointer of correct signature */
        void (*_math_func)(mat_t, register const mat_t);
	/* cast math_func to new func pointer */
	_math_func = (void (*)(mat_t, register const mat_t))math_func;

	if (argc < 2 || bn_decode_mat(a, argv[1]) < 16) {
	    bu_vls_printf(&result, "usage: %s mat", argv[0]);
	    goto error;
	}
	_math_func(o, a);
	bn_encode_mat(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_matXvec)) {
	mat_t m;
	hvect_t i, o;
	if (argc < 3 || bn_decode_mat(m, argv[1]) < 16 ||
	    bn_decode_hvect(i, argv[2]) < 4) {
	    bu_vls_printf(&result, "usage: %s mat hvect", argv[0]);
	    goto error;
	}
	bn_matXvec(o, m, i);
	bn_encode_hvect(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_mat4x3pnt)) {
	mat_t m;
	point_t i, o;
	MAT_ZERO(m);
	if (argc < 3 || bn_decode_mat(m, argv[1]) < 16 ||
	    bn_decode_vect(i, argv[2]) < 3) {
	    bu_vls_printf(&result, "usage: %s mat point", argv[0]);
	    goto error;
	}
	bn_mat4x3pnt(o, m, i);
	bn_encode_vect(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_mat4x3vec)) {
	mat_t m;
	vect_t i, o;
	MAT_ZERO(m);
	if (argc < 3 || bn_decode_mat(m, argv[1]) < 16 ||
	    bn_decode_vect(i, argv[2]) < 3) {
	    bu_vls_printf(&result, "usage: %s mat vect", argv[0]);
	    goto error;
	}
	bn_mat4x3vec(o, m, i);
	bn_encode_vect(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_hdivide)) {
	hvect_t i;
	vect_t o;
	if (argc < 2 || bn_decode_hvect(i, argv[1]) < 4) {
	    bu_vls_printf(&result, "usage: %s hvect", argv[0]);
	    goto error;
	}
	bn_hdivide(o, i);
	bn_encode_vect(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_vjoin1)) {
	point_t o;
	point_t b, d;
	double c;

	if (argc < 4) {
	    bu_vls_printf(&result, "usage: %s pnt scale dir", argv[0]);
	    goto error;
	}
	if (bn_decode_vect(b, argv[1]) < 3) goto error;
	if (Tcl_GetDouble(interp, argv[2], &c) != TCL_OK) goto error;
	if (bn_decode_vect(d, argv[3]) < 3) goto error;

	VJOIN1(o, b, c, d);	/* bn_vjoin1(o, b, c, d) */
	bn_encode_vect(&result, o);

    } else if (math_func == MATH_FUNC_VOID_CAST(bn_vblend)) {
	point_t a, c, e;
	double b, d;

	if (argc < 5) {
	    bu_vls_printf(&result, "usage: %s scale pnt scale pnt", argv[0]);
	    goto error;
	}

	if (Tcl_GetDouble(interp, argv[1], &b) != TCL_OK) goto error;
	if (bn_decode_vect(c, argv[2]) < 3) goto error;
	if (Tcl_GetDouble(interp, argv[3], &d) != TCL_OK) goto error;
	if (bn_decode_vect(e, argv[4]) < 3) goto error;

	VBLEND2(a, b, c, d, e);
	bn_encode_vect(&result, a);

    } else if (math_func == MATH_FUNC_VOID_CAST(bn_mat_ae)) {
	mat_t o;
	double az, el;

	if (argc < 3) {
	    bu_vls_printf(&result, "usage: %s azimuth elevation", argv[0]);
	    goto error;
	}
	if (Tcl_GetDouble(interp, argv[1], &az) != TCL_OK) goto error;
	if (Tcl_GetDouble(interp, argv[2], &el) != TCL_OK) goto error;

	bn_mat_ae(o, (fastf_t)az, (fastf_t)el);
	bn_encode_mat(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_ae_vec)) {
	fastf_t az, el;
	vect_t v;

	if (argc < 2 || bn_decode_vect(v, argv[1]) < 3) {
	    bu_vls_printf(&result, "usage: %s vect", argv[0]);
	    goto error;
	}

	bn_ae_vec(&az, &el, v);
	bu_vls_printf(&result, "%g %g", az, el);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_aet_vec)) {
	double acc;
	fastf_t az, el, twist, accuracy;
	vect_t vec_ae, vec_twist;

	if (argc < 4 || bn_decode_vect(vec_ae, argv[1]) < 3 ||
	    bn_decode_vect(vec_twist, argv[2]) < 3 ||
	    sscanf(argv[3], "%lf", &acc) < 1) {
	    bu_vls_printf(&result, "usage: %s vec_ae vec_twist accuracy",
			  argv[0]);
	    goto error;
	}
	accuracy = acc;

	bn_aet_vec(&az, &el, &twist, vec_ae, vec_twist, accuracy);
	bu_vls_printf(&result, "%g %g %g", az, el, twist);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_mat_angles)) {
	mat_t o;
	double alpha, beta, ggamma;

	if (argc < 4) {
	    bu_vls_printf(&result, "usage: %s alpha beta gamma", argv[0]);
	    goto error;
	}
	if (Tcl_GetDouble(interp, argv[1], &alpha) != TCL_OK) goto error;
	if (Tcl_GetDouble(interp, argv[2], &beta) != TCL_OK) goto error;
	if (Tcl_GetDouble(interp, argv[3], &ggamma) != TCL_OK) goto error;

	bn_mat_angles(o, alpha, beta, ggamma);
	bn_encode_mat(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_eigen2x2)) {
	fastf_t val1, val2;
	vect_t vec1, vec2;
	double a, b, c;

	if (argc < 4) {
	    bu_vls_printf(&result, "usage: %s a b c", argv[0]);
	    goto error;
	}
	if (Tcl_GetDouble(interp, argv[1], &a) != TCL_OK) goto error;
	if (Tcl_GetDouble(interp, argv[2], &c) != TCL_OK) goto error;
	if (Tcl_GetDouble(interp, argv[3], &b) != TCL_OK) goto error;

	bn_eigen2x2(&val1, &val2, vec1, vec2, (fastf_t)a, (fastf_t)b,
		    (fastf_t)c);
	bu_vls_printf(&result, "%g %g {%g %g %g} {%g %g %g}", INTCLAMP(val1), INTCLAMP(val2),
		      V3INTCLAMPARGS(vec1), V3INTCLAMPARGS(vec2));
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_mat_fromto)) {
	mat_t o;
	vect_t from, to;
	static const struct bn_tol tol = {
	    BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST*BN_TOL_DIST, 1e-6, 1-1e-6
	};

	if (argc < 3 || bn_decode_vect(from, argv[1]) < 3 ||
	    bn_decode_vect(to, argv[2]) < 3) {
	    bu_vls_printf(&result, "usage: %s vecFrom vecTo", argv[0]);
	    goto error;
	}
	bn_mat_fromto(o, from, to, &tol);
	bn_encode_mat(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_mat_xrot)
	       || math_func == MATH_FUNC_VOID_CAST(bn_mat_yrot)
	       || math_func == MATH_FUNC_VOID_CAST(bn_mat_zrot)) {
	mat_t o;
	double s, c;
	/* need new math func pointer of correct signature */
        void (*_math_func)(fastf_t *, double, double);
	/* cast math_func to new func pointer */
	_math_func = (void (*)(fastf_t *, double, double))math_func;

	if (argc < 3) {
	    bu_vls_printf(&result, "usage: %s sinAngle cosAngle", argv[0]);
	    goto error;
	}
	if (Tcl_GetDouble(interp, argv[1], &s) != TCL_OK) goto error;
	if (Tcl_GetDouble(interp, argv[2], &c) != TCL_OK) goto error;

	_math_func(o, s, c);
	bn_encode_mat(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_mat_lookat)) {
	mat_t o;
	vect_t dir;
	int yflip;
	if (argc < 3 || bn_decode_vect(dir, argv[1]) < 3) {
	    bu_vls_printf(&result, "usage: %s dir yflip", argv[0]);
	    goto error;
	}
	if (Tcl_GetBoolean(interp, argv[2], &yflip) != TCL_OK) goto error;

	bn_mat_lookat(o, dir, yflip);
	bn_encode_mat(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_vec_ortho)
	       || math_func == MATH_FUNC_VOID_CAST(bn_vec_perp)) {
	vect_t ov, vec;
	/* need new math func pointer of correct signature */
        void (*_math_func)(vect_t, const vect_t);
	/* cast math_func to new func pointer */
	_math_func = (void (*)(vect_t, const vect_t))math_func;

	if (argc < 2 || bn_decode_vect(vec, argv[1]) < 3) {
	    bu_vls_printf(&result, "usage: %s vec", argv[0]);
	    goto error;
	}

	_math_func(ov, vec);
	bn_encode_vect(&result, ov);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_mat_scale_about_pt_wrapper)) {
	mat_t o;
	vect_t v;
	double scale;
	int status;

	if (argc < 3 || bn_decode_vect(v, argv[1]) < 3) {
	    bu_vls_printf(&result, "usage: %s pt scale", argv[0]);
	    goto error;
	}
	if (Tcl_GetDouble(interp, argv[2], &scale) != TCL_OK) goto error;

	bn_mat_scale_about_pt_wrapper(&status, o, v, scale);
	if (status != 0) {
	    bu_vls_printf(&result, "error performing calculation");
	    goto error;
	}
	bn_encode_mat(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_mat_xform_about_pt)) {
	mat_t o, xform;
	vect_t v;

	if (argc < 3 || bn_decode_mat(xform, argv[1]) < 16 ||
	    bn_decode_vect(v, argv[2]) < 3) {
	    bu_vls_printf(&result, "usage: %s xform pt", argv[0]);
	    goto error;
	}

	bn_mat_xform_about_pt(o, xform, v);
	bn_encode_mat(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_mat_arb_rot)) {
	mat_t o;
	point_t pt;
	vect_t dir;
	double angle;

	if (argc < 4 || bn_decode_vect(pt, argv[1]) < 3 ||
	    bn_decode_vect(dir, argv[2]) < 3) {
	    bu_vls_printf(&result, "usage: %s pt dir angle", argv[0]);
	    goto error;
	}
	if (Tcl_GetDouble(interp, argv[3], &angle) != TCL_OK)
	    return TCL_ERROR;

	bn_mat_arb_rot(o, pt, dir, (fastf_t)angle);
	bn_encode_mat(&result, o);
    } else if (math_func == MATH_FUNC_VOID_CAST(quat_mat2quat)) {
	mat_t mat;
	quat_t quat;

	if (argc < 2 || bn_decode_mat(mat, argv[1]) < 16) {
	    bu_vls_printf(&result, "usage: %s mat", argv[0]);
	    goto error;
	}

	quat_mat2quat(quat, mat);
	bn_encode_quat(&result, quat);
    } else if (math_func == MATH_FUNC_VOID_CAST(quat_quat2mat)) {
	mat_t mat;
	quat_t quat;

	if (argc < 2 || bn_decode_quat(quat, argv[1]) < 4) {
	    bu_vls_printf(&result, "usage: %s quat", argv[0]);
	    goto error;
	}

	quat_quat2mat(mat, quat);
	bn_encode_mat(&result, mat);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_quat_distance_wrapper)) {
	quat_t q1, q2;
	double d;

	if (argc < 3 || bn_decode_quat(q1, argv[1]) < 4 ||
	    bn_decode_quat(q2, argv[2]) < 4) {
	    bu_vls_printf(&result, "usage: %s quatA quatB", argv[0]);
	    goto error;
	}

	bn_quat_distance_wrapper(&d, q1, q2);
	bu_vls_printf(&result, "%g", d);
    } else if (math_func == MATH_FUNC_VOID_CAST(quat_double)
	       || math_func == MATH_FUNC_VOID_CAST(quat_bisect)
	       || math_func == MATH_FUNC_VOID_CAST(quat_make_nearest)) {
	quat_t oqot, q1, q2;
        void (*_math_func)(fastf_t *, const fastf_t *, const fastf_t *);
	/* cast math_func to new func pointer */
	_math_func = (void (*)(fastf_t *, const fastf_t *, const fastf_t *))math_func;

	if (argc < 3 || bn_decode_quat(q1, argv[1]) < 4 ||
	    bn_decode_quat(q2, argv[2]) < 4) {
	    bu_vls_printf(&result, "usage: %s quatA quatB", argv[0]);
	    goto error;
	}

	_math_func(oqot, q1, q2);
	bn_encode_quat(&result, oqot);
    } else if (math_func == MATH_FUNC_VOID_CAST(quat_slerp)) {
	quat_t oq, q1, q2;
	double d;

	if (argc < 4 || bn_decode_quat(q1, argv[1]) < 4 ||
	    bn_decode_quat(q2, argv[2]) < 4) {
	    bu_vls_printf(&result, "usage: %s quat1 quat2 factor", argv[0]);
	    goto error;
	}
	if (Tcl_GetDouble(interp, argv[3], &d) != TCL_OK) goto error;

	quat_slerp(oq, q1, q2, d);
	bn_encode_quat(&result, oq);
    } else if (math_func == MATH_FUNC_VOID_CAST(quat_sberp)) {
	quat_t oq, q1, qa, qb, q2;
	double d;

	if (argc < 6 || bn_decode_quat(q1, argv[1]) < 4 ||
	    bn_decode_quat(qa, argv[2]) < 4 || bn_decode_quat(qb, argv[3]) < 4 ||
	    bn_decode_quat(q2, argv[4]) < 4) {
	    bu_vls_printf(&result, "usage: %s quat1 quatA quatB quat2 factor",
			  argv[0]);
	    goto error;
	}
	if (Tcl_GetDouble(interp, argv[5], &d) != TCL_OK) goto error;

	quat_sberp(oq, q1, qa, qb, q2, d);
	bn_encode_quat(&result, oq);
    } else if (math_func == MATH_FUNC_VOID_CAST(quat_exp)
	       || math_func == MATH_FUNC_VOID_CAST(quat_log)) {
	quat_t qout, qin;
	/* need new math func pointer of correct signature */
        void (*_math_func)(fastf_t *, const fastf_t *);
	/* cast math_func to new func pointer */
	_math_func = (void (*)(fastf_t *, const fastf_t *))math_func;

	if (argc < 2 || bn_decode_quat(qin, argv[1]) < 4) {
	    bu_vls_printf(&result, "usage: %s quat", argv[0]);
	    goto error;
	}

	_math_func(qout, qin);
	bn_encode_quat(&result, qout);
    } else if (math_func == MATH_FUNC_VOID_CAST(bn_isect_line3_line3)) {
	fastf_t t, u;
	point_t pt, a;
	vect_t dir, c;
	int i;
	static const struct bn_tol tol = {
	    BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST*BN_TOL_DIST, 1e-6, 1-1e-6
	};
	if (argc != 5) {
	    bu_vls_printf(&result,
			  "Usage: bn_isect_line3_line3 pt dir pt dir (%d args specified)",
			  argc-1);
	    goto error;
	}

	if (bn_decode_vect(pt, argv[1]) < 3) {
	    bu_vls_printf(&result, "bn_isect_line3_line3 no pt: %s\n", argv[0]);
	    goto error;
	}
	if (bn_decode_vect(dir, argv[2]) < 3) {
	    bu_vls_printf(&result, "bn_isect_line3_line3 no dir: %s\n", argv[0]);
	    goto error;
	}
	if (bn_decode_vect(a, argv[3]) < 3) {
	    bu_vls_printf(&result, "bn_isect_line3_line3 no a pt: %s\n", argv[0]);
	    goto error;
	}
	if (bn_decode_vect(c, argv[4]) < 3) {
	    bu_vls_printf(&result, "bn_isect_line3_line3 no c dir: %s\n", argv[0]);
	    goto error;
	}
	i = bn_isect_line3_line3(&t, &u, pt, dir, a, c, &tol);
	if (i != 1) {
	    bu_vls_printf(&result, "bn_isect_line3_line3 no intersection: %s\n", argv[0]);
	    goto error;
	}

	VJOIN1(a, pt, t, dir);
	bn_encode_vect(&result, a);

    } else if (math_func == MATH_FUNC_VOID_CAST(bn_isect_line2_line2)) {
	fastf_t dist[2];
	point_t pt, a;
	vect_t dir, c;
	int i;
	static const struct bn_tol tol = {
	    BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST*BN_TOL_DIST, 1e-6, 1-1e-6
	};

	if (argc != 5) {
	    bu_vls_printf(&result,
			  "Usage: bn_isect_line2_line2 pt dir pt dir (%d args specified)",
			  argc-1);
	    goto error;
	}

	/* i = bn_isect_line2_line2 {0 0} {1 0} {1 1} {0 -1} */

	VSETALL(pt, 0.0);
	VSETALL(dir, 0.0);
	VSETALL(a, 0.0);
	VSETALL(c, 0.0);

	if (bn_decode_vect(pt, argv[1]) < 2) {
	    bu_vls_printf(&result, "bn_isect_line2_line2 no pt: %s\n", argv[0]);
	    goto error;
	}
	if (bn_decode_vect(dir, argv[2]) < 2) {
	    bu_vls_printf(&result, "bn_isect_line2_line2 no dir: %s\n", argv[0]);
	    goto error;
	}
	if (bn_decode_vect(a, argv[3]) < 2) {
	    bu_vls_printf(&result, "bn_isect_line2_line2 no a pt: %s\n", argv[0]);
	    goto error;
	}
	if (bn_decode_vect(c, argv[4]) < 2) {
	    bu_vls_printf(&result, "bn_isect_line2_line2 no c dir: %s\n", argv[0]);
	    goto error;
	}
	i = bn_isect_line2_line2(dist, pt, dir, a, c, &tol);
	if (i != 1) {
	    bu_vls_printf(&result, "bn_isect_line2_line2 no intersection: %s\n", argv[0]);
	    goto error;
	}

	VJOIN1(a, pt, dist[0], dir);
	bu_vls_printf(&result, "%g %g", V2INTCLAMPARGS(a));

    } else if (math_func == MATH_FUNC_VOID_CAST(bn_dist_pt2_lseg2)) {
	point_t ptA, ptB, pca;
	point_t pt;
	fastf_t dist;
	int ret;
	static const struct bn_tol tol = {
	    BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST*BN_TOL_DIST, 1e-6, 1-1e-6
	};

	if (argc != 4) {
	    bu_vls_printf(&result,
			  "Usage: bn_dist_pt2_lseg2 ptA ptB pt (%d args specified)", argc-1);
	    goto error;
	}

	if (bn_decode_vect(ptA, argv[1]) < 2) {
	    bu_vls_printf(&result, "bn_dist_pt2_lseg2 no ptA: %s\n", argv[0]);
	    goto error;
	}

	if (bn_decode_vect(ptB, argv[2]) < 2) {
	    bu_vls_printf(&result, "bn_dist_pt2_lseg2 no ptB: %s\n", argv[0]);
	    goto error;
	}

	if (bn_decode_vect(pt, argv[3]) < 2) {
	    bu_vls_printf(&result, "bn_dist_pt2_lseg2 no pt: %s\n", argv[0]);
	    goto error;
	}

	ret = bn_dist_pt2_lseg2(&dist, pca, ptA, ptB, pt, &tol);
	switch (ret) {
	    case 0:
	    case 1:
	    case 2:
		dist = 0.0;
		break;
	    default:
		break;
	}

	bu_vls_printf(&result, "%g", dist);
    } else {
	bu_vls_printf(&result, "libbn/bn_tcl.c: math function %s not supported yet\n", argv[0]);
	goto error;
    }

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


int
bn_cmd_noise_perlin(ClientData UNUSED(clientData),
		    Tcl_Interp *interp,
		    int argc,
		    char **argv)
{
    point_t pt;
    double v;

    if (argc != 4) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 argv[0], " X Y Z \"",
			 NULL);
	return TCL_ERROR;
    }

    pt[X] = atof(argv[1]);
    pt[Y] = atof(argv[2]);
    pt[Z] = atof(argv[3]);

    v = bn_noise_perlin(pt);
    Tcl_SetObjResult(interp, Tcl_NewDoubleObj(v));

    return TCL_OK;
}


/*
 * usage: bn_noise_fbm X Y Z h_val lacunarity octaves
 *
 */
int
bn_cmd_noise(ClientData UNUSED(clientData),
	     Tcl_Interp *interp,
	     int argc,
	     char **argv)
{
    point_t pt;
    double h_val;
    double lacunarity;
    double octaves;
    double val;

    if (argc != 7) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 argv[0], " X Y Z h_val lacunarity octaves\"",
			 NULL);
	return TCL_ERROR;
    }

    pt[0] = atof(argv[1]);
    pt[1] = atof(argv[2]);
    pt[2] = atof(argv[3]);

    h_val = atof(argv[4]);
    lacunarity = atof(argv[5]);
    octaves = atof(argv[6]);


    if (BU_STR_EQUAL("bn_noise_turb", argv[0])) {
	val = bn_noise_turb(pt, h_val, lacunarity, octaves);

	Tcl_SetObjResult(interp, Tcl_NewDoubleObj(val));
    } else if (BU_STR_EQUAL("bn_noise_fbm", argv[0])) {
	val = bn_noise_fbm(pt, h_val, lacunarity, octaves);
	Tcl_SetObjResult(interp, Tcl_NewDoubleObj(val));
    } else {
	Tcl_AppendResult(interp, "Unknown noise type \"",
			 argv[0], "\"",	 NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}


/**
 * @brief
 * usage: noise_slice xdim ydim inv h_val lac octaves dX dY dZ sX [sY sZ]
 *
 * The idea here is to get a whole slice of noise at once, thereby
 * avoiding the overhead of doing this in Tcl.
 */
int
bn_cmd_noise_slice(ClientData UNUSED(clientData),
		   Tcl_Interp *interp,
		   int argc,
		   char **argv)
{
    double h_val;
    double lacunarity;
    double octaves;

    vect_t delta; 	/* translation to noise space */
    vect_t scale; 	/* scale to noise space */
    unsigned xdim;	/* # samples X direction */
    unsigned ydim;	/* # samples Y direction */
    unsigned xval, yval;
#define NOISE_FBM 0
#define NOISE_TURB 1

#define COV186_UNUSED_CODE 0
#if COV186_UNUSED_CODE
    int noise_type = NOISE_FBM;
#endif
    double val;
    point_t pt;

    if (argc != 7) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 argv[0], " Xdim Ydim Zval h_val lacunarity octaves\"",
			 NULL);
	return TCL_ERROR;
    }

    xdim = atoi(argv[0]);
    ydim = atoi(argv[1]);
    VSETALL(delta, 0.0);
    VSETALL(scale, 1.);
    pt[Z] = delta[Z] = atof(argv[2]);
    h_val = atof(argv[3]);
    lacunarity = atof(argv[4]);
    octaves = atof(argv[5]);

#define COV186_UNUSED_CODE 0
    /* Only NOISE_FBM is possible at this time, so comment out the switching for
     * NOISE_TURB. This may need to be deleted. */
#if COV186_UNUSED_CODE
    switch (noise_type) {
	case NOISE_FBM:
#endif
	    for (yval = 0; yval < ydim; yval++) {

		pt[Y] = yval * scale[Y] + delta[Y];

		for (xval = 0; xval < xdim; xval++) {
		    pt[X] = xval * scale[X] + delta[X];

		    bn_noise_fbm(pt, h_val, lacunarity, octaves);

		}
	    }
#if COV186_UNUSED_CODE
	    break;
	case NOISE_TURB:
	    for (yval = 0; yval < ydim; yval++) {

		pt[Y] = yval * scale[Y] + delta[Y];

		for (xval = 0; xval < xdim; xval++) {
		    pt[X] = xval * scale[X] + delta[X];

		    val = bn_noise_turb(pt, h_val, lacunarity, octaves);

		}
	    }
	    break;
    }
#endif


    pt[0] = atof(argv[1]);
    pt[1] = atof(argv[2]);
    pt[2] = atof(argv[3]);

    h_val = atof(argv[4]);
    lacunarity = atof(argv[5]);
    octaves = atof(argv[6]);


    if (BU_STR_EQUAL("bn_noise_turb", argv[0])) {
	val = bn_noise_turb(pt, h_val, lacunarity, octaves);
	Tcl_SetObjResult(interp, Tcl_NewDoubleObj(val));
    } else if (BU_STR_EQUAL("bn_noise_fbm", argv[0])) {
	val = bn_noise_fbm(pt, h_val, lacunarity, octaves);
	Tcl_SetObjResult(interp, Tcl_NewDoubleObj(val));
    } else {
	Tcl_AppendResult(interp, "Unknown noise type \"",
			 argv[0], "\"",	 NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}


int
bn_cmd_random(ClientData UNUSED(clientData),
	      Tcl_Interp *interp,
	      int argc,
	      char **argv)
{
    int val;
    const char *str;
    double rnd;
    char buf[32];

    if (argc != 2) {
	Tcl_AppendResult(interp, "Wrong # args:  Should be \"",
			 argv[0], " varname\"", NULL);
	return TCL_ERROR;
    }

    str=Tcl_GetVar(interp, argv[1], 0);
    if (!str) {
	Tcl_AppendResult(interp, "Error getting variable ",
			 argv[1], NULL);
	return TCL_ERROR;
    }
    val = atoi(str);

    if (val < 0) val = 0;

    rnd = BN_RANDOM(val);

    snprintf(buf, 32, "%d", val);

    if (!Tcl_SetVar(interp, argv[1], buf, 0)) {
	Tcl_AppendResult(interp, "Error setting variable ",
			 argv[1], NULL);
	return TCL_ERROR;
    }

    snprintf(buf, 32, "%g", rnd);
    Tcl_AppendResult(interp, buf, NULL);
    return TCL_OK;
}


void
bn_tcl_mat_print(Tcl_Interp *interp,
		 const char *title,
		 const mat_t m)
{
    char obuf[1024];	/* snprintf may be non-PARALLEL */

    bn_mat_print_guts(title, m, obuf, 1024);
    Tcl_AppendResult(interp, obuf, "\n", (char *)NULL);
}


void
bn_tcl_setup(Tcl_Interp *interp)
{
    struct math_func_link *mp;

    for (mp = math_funcs; mp->name != NULL; mp++) {
	(void)Tcl_CreateCommand(interp, mp->name,
				(Tcl_CmdProc *)bn_math_cmd,
				(ClientData)mp,
				(Tcl_CmdDeleteProc *)NULL);
    }

    (void)Tcl_CreateCommand(interp, "bn_noise_perlin",
			    (Tcl_CmdProc *)bn_cmd_noise_perlin, (ClientData)NULL,
			    (Tcl_CmdDeleteProc *)NULL);

    (void)Tcl_CreateCommand(interp, "bn_noise_turb",
			    (Tcl_CmdProc *)bn_cmd_noise, (ClientData)NULL,
			    (Tcl_CmdDeleteProc *)NULL);

    (void)Tcl_CreateCommand(interp, "bn_noise_fbm",
			    (Tcl_CmdProc *)bn_cmd_noise, (ClientData)NULL,
			    (Tcl_CmdDeleteProc *)NULL);

    (void)Tcl_CreateCommand(interp, "bn_noise_slice",
			    (Tcl_CmdProc *)bn_cmd_noise_slice, (ClientData)NULL,
			    (Tcl_CmdDeleteProc *)NULL);

    (void)Tcl_CreateCommand(interp, "bn_random",
			    (Tcl_CmdProc *)bn_cmd_random, (ClientData)NULL,
			    (Tcl_CmdDeleteProc *)NULL);
}


int
Bn_Init(Tcl_Interp *interp)
{
    bn_tcl_setup(interp);
    return TCL_OK;
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
