/*
 *			B N _ T C L . C
 *
 *  Tcl interfaces to all the LIBBN math routines.
 *
 *  Author -
 *	Glenn Durfee
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif


#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"


/* Support routines for the math functions */

int
bn_decode_mat(m, str)
mat_t m;
char *str;
{
	return sscanf(str,
	    "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
	    &m[0], &m[1], &m[2], &m[3], &m[4], &m[5], &m[6], &m[7],
	    &m[8], &m[9], &m[10], &m[11], &m[12], &m[13], &m[14], &m[15]);
}

int
bn_decode_quat(q, str)
quat_t q;
char *str;
{
	return sscanf(str, "%lf %lf %lf %lf", &q[0], &q[1], &q[2], &q[3]);
}

int
bn_decode_vect(v, str)
vect_t v;
char *str;
{
	return sscanf(str, "%lf %lf %lf", &v[0], &v[1], &v[2]);
}

void
bn_encode_mat(vp, m)
struct bu_vls *vp;
mat_t m;
{
	bu_vls_printf(vp, "%g %g %g %g  %g %g %g %g  %g %g %g %g  %g %g %g %g",
	    m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
	    m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
}

void
bn_encode_quat(vp, q)
struct bu_vls *vp;
quat_t q;
{
	bu_vls_printf(vp, "%g %g %g %g", V4ARGS(q));
}

void
bn_encode_vect(vp, v)
struct bu_vls *vp;
vect_t v;
{
	bu_vls_printf(vp, "%g %g %g", V3ARGS(v));
}

void
bn_quat_distance_wrapper(dp, q1, q2)
double *dp;
quat_t q1, q2;
{
	*dp = quat_distance(q1, q2);
}

void
bn_mat_scale_about_pt_wrapper(statusp, mat, pt, scale)
int *statusp;
mat_t mat;
CONST point_t pt;
CONST double scale;
{
	*statusp = bn_mat_scale_about_pt(mat, pt, scale);
}



/*
 *			B N _ M A T H _ C M D
 *
 * Tcl wrappers for the math functions.
 *
 * This is where you should put clauses, in the below "if" statement, to add
 * Tcl support for the LIBBN math routines.
 */

int
bn_math_cmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	void (*math_func)();
	struct bu_vls result;

	math_func = (void (*)())clientData;
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
	} else {
		bu_vls_printf(&result, "libbn/bn_tcl.c: math function %s not supported yet", argv[0]);
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
	"mat_mul",            bn_mat_mul,
	"mat_inv",            bn_mat_inv,
	"mat_trn",            bn_mat_trn,
	"mat_ae",             bn_mat_ae,
	"mat_ae_vec",         bn_ae_vec,
	"mat_aet_vec",        bn_aet_vec,
	"mat_angles",         bn_mat_angles,
	"mat_eigen2x2",       bn_eigen2x2,
	"mat_fromto",         bn_mat_fromto,
	"mat_xrot",           bn_mat_xrot,
	"mat_yrot",           bn_mat_yrot,
	"mat_zrot",           bn_mat_zrot,
	"mat_lookat",         bn_mat_lookat,
	"mat_vec_ortho",      bn_vec_ortho,
	"mat_vec_perp",       bn_vec_perp,
	"mat_scale_about_pt", bn_mat_scale_about_pt_wrapper,
	"mat_xform_about_pt", bn_mat_xform_about_pt,
	"mat_arb_rot",        bn_mat_arb_rot,
	"quat_mat2quat",      quat_mat2quat,
	"quat_quat2mat",      quat_quat2mat,
	"quat_distance",      bn_quat_distance_wrapper,
	"quat_double",        quat_double,
	"quat_bisect",        quat_bisect,
	"quat_slerp",         quat_bisect,
	"quat_sberp",         quat_sberp,
	"quat_make_nearest",  quat_make_nearest,
	"quat_exp",           quat_exp,
	"quat_log",           quat_log,
	0, 0
};

/*
 *			B N _ T C L _ S E T U P
 *
 *  Add all the supported Tcl interfaces to LIBBN routines to
 *  the list of commands known by the given interpreter.
 */
void
bn_tcl_setup(interp)
Tcl_Interp *interp;
{
	struct math_func_link *mp;

	for (mp = math_funcs; mp->name != NULL; mp++) {
		(void)Tcl_CreateCommand(interp, mp->name, bn_math_cmd,
		    (ClientData)mp->func,
		    (Tcl_CmdDeleteProc *)NULL);
	}

	Tcl_SetVar(interp, "bn_version", (char *)bn_version+5, TCL_GLOBAL_ONLY);
}
