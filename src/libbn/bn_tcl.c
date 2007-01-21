/*                        B N _ T C L . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup bntcl */
/** @{ */
/** @file bn_tcl.c
 *
 * @brief
 *  Tcl interfaces to all the LIBBN math routines.
 *
 *  @author
 *	Glenn Durfee
 *
 *  @par Source
 *	The U. S. Army Research Laboratory
 *  @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */


#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"

/* Support routines for the math functions */

/* XXX Really need a decode_array function that uses atof(),
 * XXX so that junk like leading { and commas between inputs
 * XXX don't spoil the conversion.
 */

int
bn_decode_mat(fastf_t *m, const char *str)
{
	if( strcmp( str, "I" ) == 0 )  {
		MAT_IDN( m );
		return 16;
	}
	if( *str == '{' )  str++;

	return sscanf(str,
	    "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
	    &m[0], &m[1], &m[2], &m[3], &m[4], &m[5], &m[6], &m[7],
	    &m[8], &m[9], &m[10], &m[11], &m[12], &m[13], &m[14], &m[15]);
}

int
bn_decode_quat(fastf_t *q, const char *str)
{
	if( *str == '{' )  str++;
	return sscanf(str, "%lf %lf %lf %lf", &q[0], &q[1], &q[2], &q[3]);
}

int
bn_decode_vect(fastf_t *v, const char *str)
{
	if( *str == '{' )  str++;
	return sscanf(str, "%lf %lf %lf", &v[0], &v[1], &v[2]);
}

int
bn_decode_hvect(fastf_t *v, const char *str)
{
	if( *str == '{' )  str++;
	return sscanf(str, "%lf %lf %lf %lf", &v[0], &v[1], &v[2], &v[3]);
}

void
bn_encode_mat(struct bu_vls *vp, const mat_t m)
{
	if( m == NULL )  {
		bu_vls_putc(vp, 'I');
		return;
	}

	bu_vls_printf(vp, "%g %g %g %g  %g %g %g %g  %g %g %g %g  %g %g %g %g",
	    m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
	    m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
}

void
bn_encode_quat(struct bu_vls *vp, const mat_t q)
{
	bu_vls_printf(vp, "%g %g %g %g", V4ARGS(q));
}

void
bn_encode_vect(struct bu_vls *vp, const mat_t v)
{
	bu_vls_printf(vp, "%g %g %g", V3ARGS(v));
}

void
bn_encode_hvect(struct bu_vls *vp, const mat_t v)
{
	bu_vls_printf(vp, "%g %g %g %g", V4ARGS(v));
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
	VJOIN1( o, pnt, scale, dir );
}


static void bn_vblend(mat_t a, fastf_t b, mat_t c, fastf_t d, mat_t e)
{
	VBLEND2( a, b, c, d, e );
}

/**
 *			B N _ M A T H _ C M D
 *@brief
 * Tcl wrappers for the math functions.
 *
 * This is where you should put clauses, in the below "if" statement, to add
 * Tcl support for the LIBBN math routines.
 */

int
bn_math_cmd(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	void (*math_func)();
	struct bu_vls result;

	math_func = (void (*)())clientData; /* object-to-function cast */
	bu_vls_init(&result);

	if (math_func == bn_mat_mul) {
		mat_t o, a, b;
		if (argc < 3 || bn_decode_mat(a, argv[1]) < 16 ||
		    bn_decode_mat(b, argv[2]) < 16) {
			bu_vls_printf(&result, "usage: %s matA matB", argv[0]);
			goto error;
		}
		bn_mat_mul(o, a, b);
		bn_encode_mat(&result, o);
	} else if (math_func == bn_mat_inv || math_func == bn_mat_trn) {
		mat_t o, a;

		if (argc < 2 || bn_decode_mat(a, argv[1]) < 16) {
			bu_vls_printf(&result, "usage: %s mat", argv[0]);
			goto error;
		}
		(*math_func)(o, a);
		bn_encode_mat(&result, o);
	} else if (math_func == bn_matXvec) {
		mat_t m;
		hvect_t i, o;
		if (argc < 3 || bn_decode_mat(m, argv[1]) < 16 ||
		    bn_decode_hvect(i, argv[2]) < 4) {
			bu_vls_printf(&result, "usage: %s mat hvect", argv[0]);
			goto error;
		}
		bn_matXvec(o, m, i);
		bn_encode_hvect(&result, o);
	} else if (math_func == bn_mat4x3pnt) {
		mat_t m;
		point_t i, o;
		if (argc < 3 || bn_decode_mat(m, argv[1]) < 16 ||
		    bn_decode_vect(i, argv[2]) < 3) {
			bu_vls_printf(&result, "usage: %s mat point", argv[0]);
			goto error;
		}
		bn_mat4x3pnt(o, m, i);
		bn_encode_vect(&result, o);
	} else if (math_func == bn_mat4x3vec) {
		mat_t m;
		vect_t i, o;
		if (argc < 3 || bn_decode_mat(m, argv[1]) < 16 ||
		    bn_decode_vect(i, argv[2]) < 3) {
			bu_vls_printf(&result, "usage: %s mat vect", argv[0]);
			goto error;
		}
		bn_mat4x3vec(o, m, i);
		bn_encode_vect(&result, o);
	} else if (math_func == bn_hdivide) {
		hvect_t i;
		vect_t o;
		if (argc < 2 || bn_decode_hvect(i, argv[1]) < 4) {
			bu_vls_printf(&result, "usage: %s hvect", argv[0]);
			goto error;
		}
		bn_hdivide(o, i);
		bn_encode_vect(&result, o);
	} else if (math_func == bn_vjoin1) {
		point_t o;
		point_t b, d;
		fastf_t c;

		if (argc < 4) {
			bu_vls_printf(&result, "usage: %s pnt scale dir", argv[0]);
			goto error;
		}
		if( bn_decode_vect(b, argv[1]) < 3) goto error;
		if (Tcl_GetDouble(interp, argv[2], &c) != TCL_OK) goto error;
		if( bn_decode_vect(d, argv[3]) < 3) goto error;

		VJOIN1( o, b, c, d );	/* bn_vjoin1( o, b, c, d ) */
		bn_encode_vect(&result, o);

	} else if ( math_func == bn_vblend) {
		point_t a, c, e;
		fastf_t b, d;

		if( argc < 5 ) {
			bu_vls_printf(&result, "usage: %s scale pnt scale pnt", argv[0]);
			goto error;
		}

		if( Tcl_GetDouble(interp, argv[1], &b) != TCL_OK) goto error;
		if( bn_decode_vect( c, argv[2] ) < 3) goto error;
		if( Tcl_GetDouble(interp, argv[3], &d) != TCL_OK) goto error;
		if( bn_decode_vect( e, argv[4] ) < 3) goto error;

		VBLEND2( a, b, c, d, e )
		bn_encode_vect( &result, a );

	} else if (math_func == bn_mat_ae) {
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
	} else if (math_func == bn_ae_vec) {
		fastf_t az, el;
		vect_t v;

		if (argc < 2 || bn_decode_vect(v, argv[1]) < 3) {
			bu_vls_printf(&result, "usage: %s vect", argv[0]);
			goto error;
		}

		bn_ae_vec(&az, &el, v);
		bu_vls_printf(&result, "%g %g", az, el);
	} else if (math_func == bn_aet_vec) {
		fastf_t az, el, twist, accuracy;
		vect_t vec_ae, vec_twist;

		if (argc < 4 || bn_decode_vect(vec_ae, argv[1]) < 3 ||
		    bn_decode_vect(vec_twist, argv[2]) < 3 ||
		    sscanf(argv[3], "%lf", &accuracy) < 1) {
		  bu_vls_printf(&result, "usage: %s vec_ae vec_twist accuracy",
				argv[0]);
		  goto error;
		}

		bn_aet_vec(&az, &el, &twist, vec_ae, vec_twist, accuracy);
		bu_vls_printf(&result, "%g %g %g", az, el, twist);
	} else if (math_func == bn_mat_angles) {
		mat_t o;
		double alpha, beta, ggamma;

		if (argc < 4) {
			bu_vls_printf(&result, "usage: %s alpha beta gamma", argv[0]);
			goto error;
		}
		if (Tcl_GetDouble(interp, argv[1], &alpha) != TCL_OK)  goto error;
		if (Tcl_GetDouble(interp, argv[2], &beta) != TCL_OK)   goto error;
		if (Tcl_GetDouble(interp, argv[3], &ggamma) != TCL_OK) goto error;

		bn_mat_angles(o, alpha, beta, ggamma);
		bn_encode_mat(&result, o);
	} else if (math_func == bn_eigen2x2) {
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
		bu_vls_printf(&result, "%g %g {%g %g %g} {%g %g %g}", val1, val2,
		    V3ARGS(vec1), V3ARGS(vec2));
	} else if (math_func == bn_mat_fromto) {
		mat_t o;
		vect_t from, to;

		if (argc < 3 || bn_decode_vect(from, argv[1]) < 3 ||
		    bn_decode_vect(to, argv[2]) < 3) {
			bu_vls_printf(&result, "usage: %s vecFrom vecTo", argv[0]);
			goto error;
		}
		bn_mat_fromto(o, from, to);
		bn_encode_mat(&result, o);
	} else if (math_func == bn_mat_xrot || math_func == bn_mat_yrot ||
	    math_func == bn_mat_zrot) {
		mat_t o;
		double s, c;
		if (argc < 3) {
			bu_vls_printf(&result, "usage: %s sinAngle cosAngle", argv[0]);
			goto error;
		}
		if (Tcl_GetDouble(interp, argv[1], &s) != TCL_OK) goto error;
		if (Tcl_GetDouble(interp, argv[2], &c) != TCL_OK) goto error;

		(*math_func)(o, s, c);
		bn_encode_mat(&result, o);
	} else if (math_func == bn_mat_lookat) {
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
	} else if (math_func == bn_vec_ortho || math_func == bn_vec_perp) {
		vect_t ov, vec;

		if (argc < 2 || bn_decode_vect(vec, argv[1]) < 3) {
			bu_vls_printf(&result, "usage: %s vec", argv[0]);
			goto error;
		}

		(*math_func)(ov, vec);
		bn_encode_vect(&result, ov);
	} else if (math_func == bn_mat_scale_about_pt_wrapper) {
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
	} else if (math_func == bn_mat_xform_about_pt) {
		mat_t o, xform;
		vect_t v;

		if (argc < 3 || bn_decode_mat(xform, argv[1]) < 16 ||
		    bn_decode_vect(v, argv[2]) < 3) {
			bu_vls_printf(&result, "usage: %s xform pt", argv[0]);
			goto error;
		}

		bn_mat_xform_about_pt(o, xform, v);
		bn_encode_mat(&result, o);
	} else if (math_func == bn_mat_arb_rot) {
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
	} else if (math_func == quat_mat2quat) {
		mat_t mat;
		quat_t quat;

		if (argc < 2 || bn_decode_mat(mat, argv[1]) < 16) {
			bu_vls_printf(&result, "usage: %s mat", argv[0]);
			goto error;
		}

		quat_mat2quat(quat, mat);
		bn_encode_quat(&result, quat);
	} else if (math_func == quat_quat2mat) {
		mat_t mat;
		quat_t quat;

		if (argc < 2 || bn_decode_quat(quat, argv[1]) < 4) {
			bu_vls_printf(&result, "usage: %s quat", argv[0]);
			goto error;
		}

		quat_quat2mat(mat, quat);
		bn_encode_mat(&result, mat);
	} else if (math_func == bn_quat_distance_wrapper) {
		quat_t q1, q2;
		double d;

		if (argc < 3 || bn_decode_quat(q1, argv[1]) < 4 ||
		    bn_decode_quat(q2, argv[2]) < 4) {
			bu_vls_printf(&result, "usage: %s quatA quatB", argv[0]);
			goto error;
		}

		bn_quat_distance_wrapper(&d, q1, q2);
		bu_vls_printf(&result, "%g", d);
	} else if (math_func == quat_double || math_func == quat_bisect ||
	    math_func == quat_make_nearest) {
		quat_t oqot, q1, q2;

		if (argc < 3 || bn_decode_quat(q1, argv[1]) < 4 ||
		    bn_decode_quat(q2, argv[2]) < 4) {
			bu_vls_printf(&result, "usage: %s quatA quatB", argv[0]);
			goto error;
		}

		(*math_func)(oqot, q1, q2);
		bn_encode_quat(&result, oqot);
	} else if (math_func == quat_slerp) {
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
	} else if (math_func == quat_sberp) {
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
	} else if (math_func == quat_exp || math_func == quat_log) {
		quat_t qout, qin;

		if (argc < 2 || bn_decode_quat(qin, argv[1]) < 4) {
			bu_vls_printf(&result, "usage: %s quat", argv[0]);
			goto error;
		}

		(*math_func)(qout, qin);
		bn_encode_quat(&result, qout);
	} else if (math_func == (void (*)())bn_isect_line3_line3) {
	    double t, u;
	    point_t pt, a;
	    vect_t dir, c;
	    int i;
	    const static struct bn_tol tol = {
		BN_TOL_MAGIC, 0.005, 0.005*0.005, 1e-6, 1-1e-6
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

	} else if (math_func == (void (*)())bn_isect_line2_line2) {
	    double dist[2];
	    point_t pt, a;
	    vect_t dir, c;
	    int i;
	    const static struct bn_tol tol = {
		BN_TOL_MAGIC, 0.005, 0.005*0.005, 1e-6, 1-1e-6
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
	    bu_vls_printf(&result, "%g %g", a[0], a[1]);

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

static struct math_func_link {
	char *name;
	void (*func)();
} math_funcs[] = {
    {"bn_isect_line2_line2",	(void (*)())bn_isect_line2_line2},
    {"bn_isect_line3_line3",	(void (*)())bn_isect_line3_line3},
	{"mat_mul",            bn_mat_mul},
	{"mat_inv",            bn_mat_inv},
	{"mat_trn",            bn_mat_trn},
	{"matXvec",            bn_matXvec},
	{"mat4x3vec",          bn_mat4x3vec},
	{"mat4x3pnt",          bn_mat4x3pnt},
	{"hdivide",            bn_hdivide},
	{"vjoin1",	      bn_vjoin1},
	{"vblend",		bn_vblend},
	{"mat_ae",             bn_mat_ae},
	{"mat_ae_vec",         bn_ae_vec},
	{"mat_aet_vec",        bn_aet_vec},
	{"mat_angles",         bn_mat_angles},
	{"mat_eigen2x2",       bn_eigen2x2},
	{"mat_fromto",         bn_mat_fromto},
	{"mat_xrot",           bn_mat_xrot},
	{"mat_yrot",           bn_mat_yrot},
	{"mat_zrot",           bn_mat_zrot},
	{"mat_lookat",         bn_mat_lookat},
	{"mat_vec_ortho",      bn_vec_ortho},
	{"mat_vec_perp",       bn_vec_perp},
	{"mat_scale_about_pt", bn_mat_scale_about_pt_wrapper},
	{"mat_xform_about_pt", bn_mat_xform_about_pt},
	{"mat_arb_rot",        bn_mat_arb_rot},
	{"quat_mat2quat",      quat_mat2quat},
	{"quat_quat2mat",      quat_quat2mat},
	{"quat_distance",      bn_quat_distance_wrapper},
	{"quat_double",        quat_double},
	{"quat_bisect",        quat_bisect},
	{"quat_slerp",         quat_slerp},
	{"quat_sberp",         quat_sberp},
	{"quat_make_nearest",  quat_make_nearest},
	{"quat_exp",           quat_exp},
	{"quat_log",           quat_log},
	{0, 0}
};

int
bn_cmd_noise_perlin(ClientData clientData,
		  Tcl_Interp *interp,
		  int argc,
		  char **argv)
{
	point_t pt;
	double	v;

	if (argc != 4) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
				 argv[0], " X Y Z \"",
				 NULL);
		return TCL_ERROR;
	}

	pt[X] = atof(argv[1]);
	pt[Y] = atof(argv[2]);
	pt[Z] = atof(argv[3]);

	v = bn_noise_perlin( pt );
	sprintf(interp->result, "%g", v );

	return TCL_OK;
}

/*
 *  usage: bn_noise_fbm X Y Z h_val lacunarity octaves
 *
 */
int
bn_cmd_noise(ClientData clientData,
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


	if (!strcmp("bn_noise_turb", argv[0])) {
		val = bn_noise_turb(pt, h_val, lacunarity, octaves);

		sprintf(interp->result, "%g", val );
	} else 	if (!strcmp("bn_noise_fbm", argv[0])) {
		val = bn_noise_fbm(pt, h_val, lacunarity, octaves);
		sprintf(interp->result, "%g", val );
	} else {
		Tcl_AppendResult(interp, "Unknown noise type \"",
				 argv[0], "\"",	 NULL);
		return TCL_ERROR;
	}
	return TCL_OK;
}


/**
 * @brief
 *	usage: noise_slice xdim ydim inv h_val lac octaves dX dY dZ sX [sY sZ]
 *
 *	The idea here is to get a whole slice of noise at once, thereby
 *	avoiding the overhead of doing this in Tcl.
 */
int
bn_cmd_noise_slice(ClientData clientData,
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

	int noise_type = NOISE_FBM;
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

	switch (noise_type) {
	case NOISE_FBM:
		for (yval = 0 ; yval < ydim ; yval++) {

		    pt[Y] = yval * scale[Y] + delta[Y];

		    for (xval = 0 ; xval < xdim ; xval++) {
			pt[X] = xval * scale[X] + delta[X];

			val = bn_noise_fbm(pt, h_val, lacunarity, octaves);

		    }
		}
		break;
	case NOISE_TURB:
		for (yval = 0 ; yval < ydim ; yval++) {

		    pt[Y] = yval * scale[Y] + delta[Y];

		    for (xval = 0 ; xval < xdim ; xval++) {
			pt[X] = xval * scale[X] + delta[X];

			val = bn_noise_turb(pt, h_val, lacunarity, octaves);

		    }
		}
		break;
	}


	pt[0] = atof(argv[1]);
	pt[1] = atof(argv[2]);
	pt[2] = atof(argv[3]);

	h_val = atof(argv[4]);
	lacunarity = atof(argv[5]);
	octaves = atof(argv[6]);


	if (!strcmp("bn_noise_turb", argv[0])) {
		val = bn_noise_turb(pt, h_val, lacunarity, octaves);

		sprintf(interp->result, "%g", val );
	} else 	if (!strcmp("bn_noise_fbm", argv[0])) {
		val = bn_noise_fbm(pt, h_val, lacunarity, octaves);
		sprintf(interp->result, "%g", val );
	} else {
		Tcl_AppendResult(interp, "Unknown noise type \"",
				 argv[0], "\"",	 NULL);
		return TCL_ERROR;
	}
	return TCL_OK;
}


int
bn_cmd_random(ClientData clientData,
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

	if (! (str=Tcl_GetVar(interp, argv[1], 0))) {
		Tcl_AppendResult(interp, "Error getting variable ",
				 argv[1], NULL);
		return TCL_ERROR;
	}
	val = atoi(str);

	if (val < 0) val = 0;

	rnd = BN_RANDOM(val);

	sprintf(buf, "%d", val);

	if (!Tcl_SetVar(interp, argv[1], buf, 0)) {
		Tcl_AppendResult(interp, "Error setting variable ",
				 argv[1], NULL);
		return TCL_ERROR;
	}

	sprintf(buf, "%g", rnd);
	Tcl_AppendResult(interp, buf, NULL);
	return TCL_OK;
}

/**
 *			B N _ M A T _ P R I N T
 */
void
bn_tcl_mat_print(Tcl_Interp		*interp,
		 const char		*title,
		 const mat_t		m)
{
	char		obuf[1024];	/* sprintf may be non-PARALLEL */

	bn_mat_print_guts(title, m, obuf);
	Tcl_AppendResult(interp, obuf, "\n", (char *)NULL);
}

/**
 *			B N _ T C L _ S E T U P
 *@brief
 *  Add all the supported Tcl interfaces to LIBBN routines to
 *  the list of commands known by the given interpreter.
 */
void
bn_tcl_setup(Tcl_Interp *interp)
{
	struct math_func_link *mp;

	for (mp = math_funcs; mp->name != NULL; mp++) {
		(void)Tcl_CreateCommand(interp, mp->name,
		    (Tcl_CmdProc *)bn_math_cmd,
		    (ClientData)mp->func, /* Function-to-Object pointer cast */
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


	Tcl_SetVar(interp, "bn_version", (char *)bn_version+5, TCL_GLOBAL_ONLY);
}

/**
 *			B N _ I N I T
 *@brief
 *  Allows LIBBN to be dynamically loade to a vanilla tclsh/wish with
 *  "load /usr/brlcad/lib/libbn.so"
 *
 *  The name of this function is specified by TCL.
 */
int
#ifdef BRLCAD_DEBUG
Bn_d_Init(Tcl_Interp *interp)
#else
Bn_Init(Tcl_Interp *interp)
#endif
{
	bn_tcl_setup(interp);
	return TCL_OK;
}


double bn_noise_fbm(point_t point,double h_val,double lacunarity,double octaves);
double bn_noise_turb(point_t point,double h_val,double lacunarity,double octaves);
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
