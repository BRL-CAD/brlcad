/*
 *			M A T H . C
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
#include <signal.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/time.h>
#include <time.h>

#include "tcl.h"
#include "tk.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "externs.h"
#include "./ged.h"
#include "./mged_solid.h"

#include "./mgedtcl.h"


/* Support routines for the math functions */

static int
decode_mat(m, str)
mat_t m;
char *str;
{
	return sscanf(str,
	    "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
	    &m[0], &m[1], &m[2], &m[3], &m[4], &m[5], &m[6], &m[7],
	    &m[8], &m[9], &m[10], &m[11], &m[12], &m[13], &m[14], &m[15]);
}

static int
decode_quat(q, str)
quat_t q;
char *str;
{
	return sscanf(str, "%lf %lf %lf %lf", &q[0], &q[1], &q[2], &q[3]);
}

static int
decode_vect(v, str)
vect_t v;
char *str;
{
	return sscanf(str, "%lf %lf %lf", &v[0], &v[1], &v[2]);
}

static void
encode_mat(vp, m)
struct bu_vls *vp;
mat_t m;
{
	bu_vls_printf(vp, "%g %g %g %g  %g %g %g %g  %g %g %g %g  %g %g %g %g",
	    m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
	    m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
}

static void
encode_quat(vp, q)
struct bu_vls *vp;
quat_t q;
{
	bu_vls_printf(vp, "%g %g %g %g", V4ARGS(q));
}

static void
encode_vect(vp, v)
struct bu_vls *vp;
vect_t v;
{
	bu_vls_printf(vp, "%g %g %g", V3ARGS(v));
}

static void
quat_distance_wrapper(dp, q1, q2)
double *dp;
quat_t q1, q2;
{
	*dp = quat_distance(q1, q2);
}

static void
mat_scale_about_pt_wrapper(statusp, mat, pt, scale)
int *statusp;
mat_t mat;
CONST point_t pt;
CONST double scale;
{
	*statusp = mat_scale_about_pt(mat, pt, scale);
}



/*                      C M D _ M A T H
 *
 * Tcl wrappers for the math functions.
 *
 * This is where you should put clauses, in the below "if" statement, to add
 * Tcl support for the librt math routines.
 */

int
cmd_math(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	void (*math_func)();
	struct bu_vls result;

	math_func = (void (*)())clientData;
	bu_vls_init(&result);

	if (math_func == mat_mul) {
		mat_t o, a, b;
		if (argc < 3 || decode_mat(a, argv[1]) < 16 ||
		    decode_mat(b, argv[2]) < 16) {
			bu_vls_printf(&result, "usage: %s matA matB", argv[0]);
			goto error;
		}
		(*math_func)(o, a, b);
		encode_mat(&result, o);
	} else if (math_func == mat_inv || math_func == mat_trn) {
		mat_t o, a;

		if (argc < 2 || decode_mat(a, argv[1]) < 16) {
			bu_vls_printf(&result, "usage: %s mat", argv[0]);
			goto error;
		}
		(*math_func)(o, a);
		encode_mat(&result, o);
	} else if (math_func == mat_ae) {
		mat_t o;
		double az, el;

		if (argc < 3) {
			bu_vls_printf(&result, "usage: %s azimuth elevation", argv[0]);
			goto error;
		}
		if (Tcl_GetDouble(interp, argv[1], &az) != TCL_OK) goto error;
		if (Tcl_GetDouble(interp, argv[2], &el) != TCL_OK) goto error;

		(*math_func)(o, (fastf_t)az, (fastf_t)el);
		encode_mat(&result, o);
	} else if (math_func == mat_ae_vec) {
		fastf_t az, el;
		vect_t v;

		if (argc < 2 || decode_vect(v, argv[1]) < 3) {
			bu_vls_printf(&result, "usage: %s vect", argv[0]);
			goto error;
		}

		(*math_func)(&az, &el, v);
		bu_vls_printf(&result, "%g %g", az, el);
	} else if (math_func == mat_aet_vec) {
		fastf_t az, el, twist, accuracy;
		vect_t vec_ae, vec_twist;

		if (argc < 4 || decode_vect(vec_ae, argv[1]) < 3 ||
		    decode_vect(vec_twist, argv[2]) < 3 ||
		    sscanf(argv[3], "%lf", &accuracy) < 1) {
		  bu_vls_printf(&result, "usage: %s vec_ae vec_twist accuracy",
				argv[0]);
		  goto error;
		}

		(*math_func)(&az, &el, &twist, vec_ae, vec_twist, accuracy);
		bu_vls_printf(&result, "%g %g %g", az, el, twist);
	} else if (math_func == mat_angles) {
		mat_t o;
		double alpha, beta, ggamma;

		if (argc < 4) {
			bu_vls_printf(&result, "usage: %s alpha beta gamma", argv[0]);
			goto error;
		}
		if (Tcl_GetDouble(interp, argv[1], &alpha) != TCL_OK)  goto error;
		if (Tcl_GetDouble(interp, argv[2], &beta) != TCL_OK)   goto error;
		if (Tcl_GetDouble(interp, argv[3], &ggamma) != TCL_OK) goto error;

		(*math_func)(o, alpha, beta, ggamma);
		encode_mat(&result, o);
	} else if (math_func == mat_eigen2x2) {
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

		(*math_func)(&val1, &val2, vec1, vec2, (fastf_t)a, (fastf_t)b,
		    (fastf_t)c);
		bu_vls_printf(&result, "%g %g {%g %g %g} {%g %g %g}", val1, val2,
		    V3ARGS(vec1), V3ARGS(vec2));
	} else if (math_func == mat_fromto) {
		mat_t o;
		vect_t from, to;

		if (argc < 3 || decode_vect(from, argv[1]) < 3 ||
		    decode_vect(to, argv[2]) < 3) {
			bu_vls_printf(&result, "usage: %s vecFrom vecTo", argv[0]);
			goto error;
		}
		(*math_func)(o, from, to);
		encode_mat(&result, o);
	} else if (math_func == mat_xrot || math_func == mat_yrot ||
	    math_func == mat_zrot) {
		mat_t o;
		double s, c;
		if (argc < 3) {
			bu_vls_printf(&result, "usage: %s sinAngle cosAngle", argv[0]);
			goto error;
		}
		if (Tcl_GetDouble(interp, argv[1], &s) != TCL_OK) goto error;
		if (Tcl_GetDouble(interp, argv[2], &c) != TCL_OK) goto error;

		(*math_func)(o, s, c);
		encode_mat(&result, o);
	} else if (math_func == mat_lookat) {
		mat_t o;
		vect_t dir;
		int yflip;
		if (argc < 3 || decode_vect(dir, argv[1]) < 3) {
			bu_vls_printf(&result, "usage: %s dir yflip", argv[0]);
			goto error;
		}
		if (Tcl_GetBoolean(interp, argv[2], &yflip) != TCL_OK) goto error;

		(*math_func)(o, dir, yflip);
		encode_mat(&result, o);
	} else if (math_func == mat_vec_ortho || math_func == mat_vec_perp) {
		vect_t ov, vec;

		if (argc < 2 || decode_vect(vec, argv[1]) < 3) {
			bu_vls_printf(&result, "usage: %s vec", argv[0]);
			goto error;
		}

		(*math_func)(ov, vec);
		encode_vect(&result, ov);
	} else if (math_func == mat_scale_about_pt_wrapper) {
		mat_t o;
		vect_t v;
		double scale;
		int status;

		if (argc < 3 || decode_vect(v, argv[1]) < 3) {
			bu_vls_printf(&result, "usage: %s pt scale", argv[0]);
			goto error;
		}
		if (Tcl_GetDouble(interp, argv[2], &scale) != TCL_OK) goto error;

		(*math_func)(&status, o, v, scale);
		if (status != 0) {
			bu_vls_printf(&result, "error performing calculation");
			goto error;
		}
		encode_mat(&result, o);
	} else if (math_func == mat_xform_about_pt) {
		mat_t o, xform;
		vect_t v;

		if (argc < 3 || decode_mat(xform, argv[1]) < 16 ||
		    decode_vect(v, argv[2]) < 3) {
			bu_vls_printf(&result, "usage: %s xform pt", argv[0]);
			goto error;
		}

		(*math_func)(o, xform, v);
		encode_mat(&result, o);
	} else if (math_func == mat_arb_rot) {
		mat_t o;
		point_t pt;
		vect_t dir;
		double angle;

		if (argc < 4 || decode_vect(pt, argv[1]) < 3 ||
		    decode_vect(dir, argv[2]) < 3) {
			bu_vls_printf(&result, "usage: %s pt dir angle", argv[0]);
			goto error;
		}
		if (Tcl_GetDouble(interp, argv[3], &angle) != TCL_OK)
			return TCL_ERROR;

		(*math_func)(o, pt, dir, (fastf_t)angle);
		encode_mat(&result, o);
	} else if (math_func == quat_mat2quat) {
		mat_t mat;
		quat_t quat;

		if (argc < 2 || decode_mat(mat, argv[1]) < 16) {
			bu_vls_printf(&result, "usage: %s mat", argv[0]);
			goto error;
		}

		(*math_func)(quat, mat);
		encode_quat(&result, quat);
	} else if (math_func == quat_quat2mat) {
		mat_t mat;
		quat_t quat;

		if (argc < 2 || decode_quat(quat, argv[1]) < 4) {
			bu_vls_printf(&result, "usage: %s quat", argv[0]);
			goto error;
		}

		(*math_func)(mat, quat);
		encode_mat(&result, mat);
	} else if (math_func == quat_distance_wrapper) {
		quat_t q1, q2;
		double d;

		if (argc < 3 || decode_quat(q1, argv[1]) < 4 ||
		    decode_quat(q2, argv[2]) < 4) {
			bu_vls_printf(&result, "usage: %s quatA quatB", argv[0]);
			goto error;
		}

		(*math_func)(&d, q1, q2);
		bu_vls_printf(&result, "%g", d);
	} else if (math_func == quat_double || math_func == quat_bisect ||
	    math_func == quat_make_nearest) {
		quat_t oqot, q1, q2;

		if (argc < 3 || decode_quat(q1, argv[1]) < 4 ||
		    decode_quat(q2, argv[2]) < 4) {
			bu_vls_printf(&result, "usage: %s quatA quatB", argv[0]);
			goto error;
		}

		(*math_func)(oqot, q1, q2);
		encode_quat(&result, oqot);
	} else if (math_func == quat_slerp) {
		quat_t oq, q1, q2;
		double d;

		if (argc < 4 || decode_quat(q1, argv[1]) < 4 ||
		    decode_quat(q2, argv[2]) < 4) {
			bu_vls_printf(&result, "usage: %s quat1 quat2 factor", argv[0]);
			goto error;
		}
		if (Tcl_GetDouble(interp, argv[3], &d) != TCL_OK) goto error;

		(*math_func)(oq, q1, q2, d);
		encode_quat(&result, oq);
	} else if (math_func == quat_sberp) {
		quat_t oq, q1, qa, qb, q2;
		double d;

		if (argc < 6 || decode_quat(q1, argv[1]) < 4 ||
		    decode_quat(qa, argv[2]) < 4 || decode_quat(qb, argv[3]) < 4 ||
		    decode_quat(q2, argv[4]) < 4) {
			bu_vls_printf(&result, "usage: %s quat1 quatA quatB quat2 factor",
			    argv[0]);
			goto error;
		}
		if (Tcl_GetDouble(interp, argv[5], &d) != TCL_OK) goto error;

		(*math_func)(oq, q1, qa, qb, q2, d);
		encode_quat(&result, oq);
	} else if (math_func == quat_exp || math_func == quat_log) {
		quat_t qout, qin;

		if (argc < 2 || decode_quat(qin, argv[1]) < 4) {
			bu_vls_printf(&result, "usage: %s quat", argv[0]);
			goto error;
		}

		(*math_func)(qout, qin);
		encode_quat(&result, qout);
	} else {
		bu_vls_printf(&result, "math function %s not supported yet", argv[0]);
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
	"mat_mul",            mat_mul,
	"mat_inv",            mat_inv,
	"mat_trn",            mat_trn,
	"mat_ae",             mat_ae,
	"mat_ae_vec",         mat_ae_vec,
	"mat_aet_vec",        mat_aet_vec,
	"mat_angles",         mat_angles,
	"mat_eigen2x2",       mat_eigen2x2,
	"mat_fromto",         mat_fromto,
	"mat_xrot",           mat_xrot,
	"mat_yrot",           mat_yrot,
	"mat_zrot",           mat_zrot,
	"mat_lookat",         mat_lookat,
	"mat_vec_ortho",      mat_vec_ortho,
	"mat_vec_perp",       mat_vec_perp,
	"mat_scale_about_pt", mat_scale_about_pt_wrapper,
	"mat_xform_about_pt", mat_xform_about_pt,
	"mat_arb_rot",        mat_arb_rot,
	"quat_mat2quat",      quat_mat2quat,
	"quat_quat2mat",      quat_quat2mat,
	"quat_distance",      quat_distance_wrapper,
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
 *			M A T H _ S E T U P
 *
 *  Called from cmd_setup()
 */
void
math_setup()
{
	struct math_func_link *mp;

	for (mp = math_funcs; mp->name != NULL; mp++) {
		(void)Tcl_CreateCommand(interp, mp->name, cmd_math,
		    (ClientData)mp->func,
		    (Tcl_CmdDeleteProc *)NULL);
	}
}
