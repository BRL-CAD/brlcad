/*                             B N . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/**
 *
 * BRL-CAD's Tcl wrappers for libbn.
 *
 */

#include "common.h"

#define RESOURCE_INCLUDED 1
#include <tcl.h>
#ifdef HAVE_TK
#  include <tk.h>
#endif

#include "string.h" /* for strchr */

#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "tclcad.h"


/* Private headers */
#include "brlcad_version.h"
#include "./tclcad_private.h"


void
bn_quat_distance_wrapper(double *dp, quat_t q1, quat_t q2)
{
    *dp = quat_distance(q1, q2);
}


static void
bn_mat4x3vec(fastf_t *o, mat_t m, vect_t i)
{
    MAT4X3VEC(o, m, i);
}


static void
bn_hdivide(fastf_t *o, const hvect_t i)
{
    HDIVIDE(o, i);
}

static int
tclcad_bn_dist_pnt2_lseg2(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    point_t ptA, ptB, pca;
    point_t pt;
    fastf_t dist;
    int ret;
    static const struct bn_tol tol = {
	BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST*BN_TOL_DIST, 1e-6, 1-1e-6
    };

    if (argc != 4) {
	bu_vls_printf(&result,
		"Usage: bn_dist_pnt2_lseg2 pntA pntB pnt (%d args specified)", argc-1);
	goto error;
    }

    if (bn_decode_vect(ptA, argv[1]) < 2) {
	bu_vls_printf(&result, "bn_dist_pnt2_lseg2 no pntA: %s\n", argv[0]);
	goto error;
    }

    if (bn_decode_vect(ptB, argv[2]) < 2) {
	bu_vls_printf(&result, "bn_dist_pnt2_lseg2 no pntB: %s\n", argv[0]);
	goto error;
    }

    if (bn_decode_vect(pt, argv[3]) < 2) {
	bu_vls_printf(&result, "bn_dist_pnt2_lseg2 no pnt: %s\n", argv[0]);
	goto error;
    }

    ret = bn_dist_pnt2_lseg2(&dist, pca, ptA, ptB, pt, &tol);
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

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;

} 

static int
tclcad_bn_isect_line2_line2(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    fastf_t dist[2];
    point_t pt = VINIT_ZERO;
    point_t a = VINIT_ZERO;
    vect_t dir = VINIT_ZERO;
    vect_t c = VINIT_ZERO;
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

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}

static int
tclcad_bn_isect_line3_line3(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;

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
    bn_encode_vect(&result, a, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_mat_inv(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;

    mat_t o, a;

    if (argc < 2 || bn_decode_mat(a, argv[1]) < 16) {
	bu_vls_printf(&result, "usage: %s mat", argv[0]);
	goto error;
    }
    bn_mat_inv(o, a);
    bn_encode_mat(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_mat_trn(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;

    mat_t o, a;

    if (argc < 2 || bn_decode_mat(a, argv[1]) < 16) {
	bu_vls_printf(&result, "usage: %s mat", argv[0]);
	goto error;
    }
    bn_mat_trn(o, a);
    bn_encode_mat(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_matXvec(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;

    mat_t m;
    hvect_t i, o;
    if (argc < 3 || bn_decode_mat(m, argv[1]) < 16 ||
	    bn_decode_hvect(i, argv[2]) < 4) {
	bu_vls_printf(&result, "usage: %s mat hvect", argv[0]);
	goto error;
    }
    bn_matXvec(o, m, i);
    bn_encode_hvect(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}

static int
tclcad_bn_mat4x3vec(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    mat_t m;
    vect_t i, o;
    MAT_ZERO(m);
    if (argc < 3 || bn_decode_mat(m, argv[1]) < 16 ||
	    bn_decode_vect(i, argv[2]) < 3) {
	bu_vls_printf(&result, "usage: %s mat vect", argv[0]);
	goto error;
    }
    bn_mat4x3vec(o, m, i);
    bn_encode_vect(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_hdivide(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    hvect_t i;
    vect_t o;
    if (argc < 2 || bn_decode_hvect(i, argv[1]) < 4) {
	bu_vls_printf(&result, "usage: %s hvect", argv[0]);
	goto error;
    }
    bn_hdivide(o, i);
    bn_encode_vect(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}

static int
tclcad_bn_vjoin1(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
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
    bn_encode_vect(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}

static int
tclcad_bn_vblend(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
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
    bn_encode_vect(&result, a, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}

static int
tclcad_bn_ae_vec(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    fastf_t az, el;
    vect_t v;

    if (argc < 2 || bn_decode_vect(v, argv[1]) < 3) {
	bu_vls_printf(&result, "usage: %s vect", argv[0]);
	goto error;
    }

    bn_ae_vec(&az, &el, v);
    bu_vls_printf(&result, "%g %g", az, el);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}

static int
tclcad_bn_aet_vec(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
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

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}

static int
tclcad_bn_mat_angles(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
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
    bn_encode_mat(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}

static int
tclcad_bn_eigen2x2(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
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

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}

static int
tclcad_bn_mat_fromto(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
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
    bn_encode_mat(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_mat_xrot(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    mat_t o;
    double s, c;

    if (argc < 3) {
	bu_vls_printf(&result, "usage: %s sinAngle cosAngle", argv[0]);
	goto error;
    }
    if (Tcl_GetDouble(interp, argv[1], &s) != TCL_OK) goto error;
    if (Tcl_GetDouble(interp, argv[2], &c) != TCL_OK) goto error;

    bn_mat_xrot(o, s, c);
    bn_encode_mat(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_mat_yrot(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    mat_t o;
    double s, c;

    if (argc < 3) {
	bu_vls_printf(&result, "usage: %s sinAngle cosAngle", argv[0]);
	goto error;
    }
    if (Tcl_GetDouble(interp, argv[1], &s) != TCL_OK) goto error;
    if (Tcl_GetDouble(interp, argv[2], &c) != TCL_OK) goto error;

    bn_mat_yrot(o, s, c);
    bn_encode_mat(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_mat_zrot(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    mat_t o;
    double s, c;

    if (argc < 3) {
	bu_vls_printf(&result, "usage: %s sinAngle cosAngle", argv[0]);
	goto error;
    }
    if (Tcl_GetDouble(interp, argv[1], &s) != TCL_OK) goto error;
    if (Tcl_GetDouble(interp, argv[2], &c) != TCL_OK) goto error;

    bn_mat_zrot(o, s, c);
    bn_encode_mat(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_mat_lookat(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    mat_t o;
    vect_t dir;
    int yflip;
    if (argc < 3 || bn_decode_vect(dir, argv[1]) < 3) {
	bu_vls_printf(&result, "usage: %s dir yflip", argv[0]);
	goto error;
    }
    if (Tcl_GetBoolean(interp, argv[2], &yflip) != TCL_OK) goto error;

    bn_mat_lookat(o, dir, yflip);
    bn_encode_mat(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_vec_ortho(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    vect_t ov, vec;

    if (argc < 2 || bn_decode_vect(vec, argv[1]) < 3) {
	bu_vls_printf(&result, "usage: %s vec", argv[0]);
	goto error;
    }

    bn_vec_ortho(ov, vec);
    bn_encode_vect(&result, ov, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_vec_perp(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    vect_t ov, vec;

    if (argc < 2 || bn_decode_vect(vec, argv[1]) < 3) {
	bu_vls_printf(&result, "usage: %s vec", argv[0]);
	goto error;
    }

    bn_vec_perp(ov, vec);
    bn_encode_vect(&result, ov, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_mat_xform_about_pnt(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    mat_t o, xform;
    vect_t v;

    if (argc < 3 || bn_decode_mat(xform, argv[1]) < 16 ||
	    bn_decode_vect(v, argv[2]) < 3) {
	bu_vls_printf(&result, "usage: %s xform pt", argv[0]);
	goto error;
    }

    bn_mat_xform_about_pnt(o, xform, v);
    bn_encode_mat(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_mat_arb_rot(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
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
    bn_encode_mat(&result, o, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_quat_mat2quat(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    mat_t mat;
    quat_t quat;

    if (argc < 2 || bn_decode_mat(mat, argv[1]) < 16) {
	bu_vls_printf(&result, "usage: %s mat", argv[0]);
	goto error;
    }

    quat_mat2quat(quat, mat);
    bn_encode_quat(&result, quat, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_quat_quat2mat(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    mat_t mat;
    quat_t quat;

    if (argc < 2 || bn_decode_quat(quat, argv[1]) < 4) {
	bu_vls_printf(&result, "usage: %s quat", argv[0]);
	goto error;
    }

    quat_quat2mat(mat, quat);
    bn_encode_mat(&result, mat, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}

static int
tclcad_bn_quat_distance(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    quat_t q1, q2;
    double d;

    if (argc < 3 || bn_decode_quat(q1, argv[1]) < 4 ||
	    bn_decode_quat(q2, argv[2]) < 4) {
	bu_vls_printf(&result, "usage: %s quatA quatB", argv[0]);
	goto error;
    }

    bn_quat_distance_wrapper(&d, q1, q2);
    bu_vls_printf(&result, "%g", d);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}

static int
tclcad_bn_quat_double(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    quat_t oqot, q1, q2;

    if (argc < 3 || bn_decode_quat(q1, argv[1]) < 4 ||
	    bn_decode_quat(q2, argv[2]) < 4) {
	bu_vls_printf(&result, "usage: %s quatA quatB", argv[0]);
	goto error;
    }

    quat_double(oqot, q1, q2);
    bn_encode_quat(&result, oqot, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}



static int
tclcad_bn_quat_bisect(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    quat_t oqot, q1, q2;

    if (argc < 3 || bn_decode_quat(q1, argv[1]) < 4 ||
	    bn_decode_quat(q2, argv[2]) < 4) {
	bu_vls_printf(&result, "usage: %s quatA quatB", argv[0]);
	goto error;
    }

    quat_bisect(oqot, q1, q2);
    bn_encode_quat(&result, oqot, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}



static int
tclcad_bn_quat_make_nearest(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    quat_t oqot, q1;

    if (argc < 2 || bn_decode_quat(oqot, argv[1]) < 4) {
	bu_vls_printf(&result, "usage: %s orig_quat", argv[0]);
	goto error;
    }

    quat_make_nearest(q1, oqot);
    bn_encode_quat(&result, q1, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}

static int
tclcad_bn_quat_slerp(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    quat_t oq, q1, q2;
    double d;

    if (argc < 4 || bn_decode_quat(q1, argv[1]) < 4 ||
	    bn_decode_quat(q2, argv[2]) < 4) {
	bu_vls_printf(&result, "usage: %s quat1 quat2 factor", argv[0]);
	goto error;
    }
    if (Tcl_GetDouble(interp, argv[3], &d) != TCL_OK) goto error;

    quat_slerp(oq, q1, q2, d);
    bn_encode_quat(&result, oq, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}



static int
tclcad_bn_quat_sberp(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
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
    bn_encode_quat(&result, oq, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}



static int
tclcad_bn_quat_exp(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    quat_t qout, qin;

    if (argc < 2 || bn_decode_quat(qin, argv[1]) < 4) {
	bu_vls_printf(&result, "usage: %s quat", argv[0]);
	goto error;
    }

    quat_exp(qout, qin);
    bn_encode_quat(&result, qout, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


static int
tclcad_bn_quat_log(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    quat_t qout, qin;

    if (argc < 2 || bn_decode_quat(qin, argv[1]) < 4) {
	bu_vls_printf(&result, "usage: %s quat", argv[0]);
	goto error;
    }

    quat_log(qout, qin);
    bn_encode_quat(&result, qout, 1);

    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_OK;

error:
    Tcl_AppendResult(interp, bu_vls_addr(&result), (char *)NULL);
    bu_vls_free(&result);
    return TCL_ERROR;
}


#define BN_FUNC_TCL_CAST(_func) ((int (*)(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv))_func)

static struct math_func_link {
    const char *name;
    int (*func)(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
} math_funcs[] = {
    /* FIXME: migrate these all to libged */
    {"bn_dist_pnt2_lseg2",	 BN_FUNC_TCL_CAST(tclcad_bn_dist_pnt2_lseg2) },
    {"bn_isect_line2_line2",	 BN_FUNC_TCL_CAST(tclcad_bn_isect_line2_line2) },
    {"bn_isect_line3_line3",	 BN_FUNC_TCL_CAST(tclcad_bn_isect_line3_line3) },
    {"mat_inv",                  BN_FUNC_TCL_CAST(tclcad_bn_mat_inv) },
    {"mat_trn",                  BN_FUNC_TCL_CAST(tclcad_bn_mat_trn) },
    {"matXvec",                  BN_FUNC_TCL_CAST(tclcad_bn_matXvec) },
    {"mat4x3vec",                BN_FUNC_TCL_CAST(tclcad_bn_mat4x3vec) },
    {"hdivide",                  BN_FUNC_TCL_CAST(tclcad_bn_hdivide) },
    {"vjoin1",	                 BN_FUNC_TCL_CAST(tclcad_bn_vjoin1) },
    {"vblend",	                 BN_FUNC_TCL_CAST(tclcad_bn_vblend) },
    {"mat_ae_vec",               BN_FUNC_TCL_CAST(tclcad_bn_ae_vec) },
    {"mat_aet_vec",              BN_FUNC_TCL_CAST(tclcad_bn_aet_vec) },
    {"mat_angles",               BN_FUNC_TCL_CAST(tclcad_bn_mat_angles) },
    {"mat_eigen2x2",             BN_FUNC_TCL_CAST(tclcad_bn_eigen2x2) },
    {"mat_fromto",               BN_FUNC_TCL_CAST(tclcad_bn_mat_fromto) },
    {"mat_xrot",                 BN_FUNC_TCL_CAST(tclcad_bn_mat_xrot) },
    {"mat_yrot",                 BN_FUNC_TCL_CAST(tclcad_bn_mat_yrot) },
    {"mat_zrot",                 BN_FUNC_TCL_CAST(tclcad_bn_mat_zrot) },
    {"mat_lookat",               BN_FUNC_TCL_CAST(tclcad_bn_mat_lookat) },
    {"mat_vec_ortho",            BN_FUNC_TCL_CAST(tclcad_bn_vec_ortho) },
    {"mat_vec_perp",             BN_FUNC_TCL_CAST(tclcad_bn_vec_perp) },
    {"mat_xform_about_pnt",      BN_FUNC_TCL_CAST(tclcad_bn_mat_xform_about_pnt) },
    {"mat_arb_rot",              BN_FUNC_TCL_CAST(tclcad_bn_mat_arb_rot) },
    {"quat_mat2quat",            BN_FUNC_TCL_CAST(tclcad_bn_quat_mat2quat) },
    {"quat_quat2mat",            BN_FUNC_TCL_CAST(tclcad_bn_quat_quat2mat) },
    {"quat_distance",            BN_FUNC_TCL_CAST(tclcad_bn_quat_distance) },
    {"quat_double",              BN_FUNC_TCL_CAST(tclcad_bn_quat_double) },
    {"quat_bisect",              BN_FUNC_TCL_CAST(tclcad_bn_quat_bisect) },
    {"quat_make_nearest",        BN_FUNC_TCL_CAST(tclcad_bn_quat_make_nearest) },
    {"quat_slerp",               BN_FUNC_TCL_CAST(tclcad_bn_quat_slerp) },
    {"quat_sberp",               BN_FUNC_TCL_CAST(tclcad_bn_quat_sberp) },
    {"quat_exp",                 BN_FUNC_TCL_CAST(tclcad_bn_quat_exp) },
    {"quat_log",                 BN_FUNC_TCL_CAST(tclcad_bn_quat_log) },
    {0, 0}
};

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

    V_MAX(val, 0);

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
tclcad_bn_mat_print(Tcl_Interp *interp,
		 const char *title,
		 const mat_t m)
{
    char obuf[1024];	/* snprintf may be non-PARALLEL */

    bn_mat_print_guts(title, m, obuf, 1024);
    Tcl_AppendResult(interp, obuf, "\n", (char *)NULL);
}


void
tclcad_bn_setup(Tcl_Interp *interp)
{
    struct math_func_link *mp;

    for (mp = math_funcs; mp->name != NULL; mp++) {
	(void)Tcl_CreateCommand(interp, mp->name,
				(Tcl_CmdProc *)mp->func,
				(ClientData)NULL,
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
    tclcad_bn_setup(interp);
    return TCL_OK;
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
