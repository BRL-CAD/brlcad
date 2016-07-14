/*                        L I B F U N C S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
/** @file libtclcad/libfuncs.c
 *
 * BRL-CAD's Tcl wrappers for various libraries.
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
#include "dm.h"
#include "fb.h"
#include "bu.h"
#include "bn.h"
#include "vmath.h"
#include "raytrace.h"
#include "tclcad.h"


/* Private headers */
#include "brlcad_version.h"
#include "tclcad_private.h"

static int
lwrapper_func(ClientData data, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct bu_cmdtab *ctp = (struct bu_cmdtab *)data;

    return ctp->ct_func(interp, argc, argv);
}



#define TINYBUFSIZ 32
#define SMALLBUFSIZ 256

/**
 * A wrapper for bu_mem_barriercheck.
 *
 * \@param clientData    - UNUSED, present for signature matching
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
HIDDEN int
tcl_bu_mem_barriercheck(void *UNUSED(clientData),
			int argc,
			const char **argv)
{
    int ret;

    if (argc > 1) {
	bu_log("Usage: %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    ret = bu_mem_barriercheck();
    if (UNLIKELY(ret < 0)) {
	bu_log("bu_mem_barriercheck() failed\n");
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


/**
 * A wrapper for bu_prmem. Prints map of memory currently in use, to
 * stderr.
 *
 * \@param clientData    - UNUSED, present for signature matching
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
HIDDEN int
tcl_bu_prmem(void *UNUSED(clientData),
	     int argc,
	     const char **argv)
{
    if (argc != 2) {
	bu_log("Usage: bu_prmem title\n");
	return BRLCAD_ERROR;
    }

    bu_prmem(argv[1]);
    return BRLCAD_OK;
}


/**
 * Given arguments of alternating keywords and values and a specific
 * keyword ("Iwant"), return the value associated with that keyword.
 *
 * example: bu_get_value_by_keyword Iwant az 35 elev 25 temp 9.6
 *
 * If only one argument is given after the search keyword, it is
 * interpreted as a list in the same format.
 *
 * example: bu_get_value_by_keyword Iwant {az 35 elev 25 temp 9.6}
 *
 * Search order is left-to-right, only first match is returned.
 *
 * Sample use:
 * bu_get_value_by_keyword V8 [concat type [.inmem get box.s]]
 *
 * @param clientData	- associated data/state
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
HIDDEN int
tcl_bu_get_value_by_keyword(void *clientData,
			    int argc,
			    const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    int i = 0;
    int listc = 0;
    const char *iwant = (const char *)NULL;
    const char **listv = (const char **)NULL;
    const char **tofree = (const char **)NULL;

    if (argc < 3) {
	char buf[TINYBUFSIZ];
	snprintf(buf, TINYBUFSIZ, "%d", argc);
	bu_log("bu_get_value_by_keyword: wrong # of args (%s).\n"
	       "Usage: bu_get_value_by_keyword iwant {list}\n"
	       "Usage: bu_get_value_by_keyword iwant key1 val1 key2 val2 ... keyN valN\n", buf);
	return BRLCAD_ERROR;
    }

    iwant = argv[1];

    if (argc == 3) {
	if (Tcl_SplitList(interp, argv[2], &listc, (const char ***)&listv) != TCL_OK) {
	    bu_log("bu_get_value_by_keyword: iwant='%s', unable to split '%s'\n", iwant, argv[2]);
	    return BRLCAD_ERROR;
	}
	tofree = listv;
    } else {
	/* Take search list from remaining arguments */
	listc = argc - 2;
	listv = argv + 2;
    }

    if ((listc & 1) != 0) {
	char buf[TINYBUFSIZ];
	snprintf(buf, TINYBUFSIZ, "%d", listc);
	bu_log("bu_get_value_by_keyword: odd # of items in list (%s).\n", buf);

	if (tofree)
	    Tcl_Free((char *)tofree); /* not bu_free() */
	return BRLCAD_ERROR;
    }

    for (i=0; i < listc; i += 2) {
	if (BU_STR_EQUAL(iwant, listv[i])) {
	    /* If value is a list, don't nest it in another list */
	    if (listv[i+1][0] == '{') {
		struct bu_vls str = BU_VLS_INIT_ZERO;

		/* Skip leading { */
		bu_vls_strcat(&str, &listv[i+1][1]);
		/* Trim trailing } */
		bu_vls_trunc(&str, -1);
		Tcl_AppendResult(interp, bu_vls_addr(&str), NULL);
		bu_vls_free(&str);
	    } else {
		Tcl_AppendResult(interp, listv[i+1], NULL);
	    }

	    if (tofree)
		Tcl_Free((char *)tofree); /* not bu_free() */
	    return BRLCAD_OK;
	}
    }

    /* Not found */

    if (tofree)
	Tcl_Free((char *)tofree); /* not bu_free() */
    return BRLCAD_ERROR;
}


/**
 * A wrapper for bu_rgb_to_hsv.
 *
 * @param clientData	- associated data/state
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
HIDDEN int
tcl_bu_rgb_to_hsv(void *clientData,
		  int argc,
		  const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    int rgb_int[3];
    unsigned char rgb[3];
    fastf_t hsv[3];
    struct bu_vls result = BU_VLS_INIT_ZERO;

    if (argc != 4) {
	bu_log("Usage: bu_rgb_to_hsv R G B\n");
	return BRLCAD_ERROR;
    }
    if (sscanf(argv[1], "%d", &rgb_int[0]) != 1
	|| sscanf(argv[2], "%d", &rgb_int[1]) != 1
	|| sscanf(argv[3], "%d", &rgb_int[2]) != 1
	|| (rgb_int[0] < 0) || (rgb_int[0] > 255)
	|| (rgb_int[1] < 0) || (rgb_int[1] > 255)
	|| (rgb_int[2] < 0) || (rgb_int[2] > 255)) {
	bu_vls_printf(&result, "bu_rgb_to_hsv: Bad RGB (%s, %s, %s)\n",
		      argv[1], argv[2], argv[3]);
	bu_log("ERROR: %s", bu_vls_addr(&result));
	bu_vls_free(&result);
	return BRLCAD_ERROR;
    }
    rgb[0] = rgb_int[0];
    rgb[1] = rgb_int[1];
    rgb[2] = rgb_int[2];

    bu_rgb_to_hsv(rgb, hsv);
    bu_vls_printf(&result, "%g %g %g", hsv[0], hsv[1], hsv[2]);
    Tcl_AppendResult(interp, bu_vls_addr(&result), NULL);
    bu_vls_free(&result);
    return BRLCAD_OK;

}


/**
 * A wrapper for bu_hsv_to_rgb.
 *
 * @param clientData	- associated data/state
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
HIDDEN int
tcl_bu_hsv_to_rgb(void *clientData,
		  int argc,
		  const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;

    double vals[3];
    fastf_t hsv[3];
    unsigned char rgb[3];
    struct bu_vls result = BU_VLS_INIT_ZERO;

    if (argc != 4) {
	bu_log("Usage: bu_hsv_to_rgb H S V\n");
	return BRLCAD_ERROR;
    }
    if (sscanf(argv[1], "%lf", &vals[0]) != 1
	|| sscanf(argv[2], "%lf", &vals[1]) != 1
	|| sscanf(argv[3], "%lf", &vals[2]) != 1)
    {
	bu_log("Bad HSV parsing (%s, %s, %s)\n", argv[1], argv[2], argv[3]);
	return BRLCAD_ERROR;
    }

    VMOVE(hsv, vals);
    if (bu_hsv_to_rgb(hsv, rgb) == 0) {
	bu_log("HSV to RGB conversion failed (%s, %s, %s)\n", argv[1], argv[2], argv[3]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(&result, "%d %d %d", rgb[0], rgb[1], rgb[2]);
    Tcl_AppendResult(interp, bu_vls_addr(&result), NULL);
    bu_vls_free(&result);
    return BRLCAD_OK;

}

/**
 * A wrapper for bu_brlcad_dir.
 *
 * @param clientData	- associated data/state
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
HIDDEN int
tcl_bu_brlcad_dir(void *clientData,
		   int argc,
		   const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    if (argc != 2) {
	bu_log("Usage: bu_brlcad_dir dirkey\n");
	return BRLCAD_ERROR;
    }
    Tcl_AppendResult(interp, bu_brlcad_dir(argv[1], 1), NULL);
    return BRLCAD_OK;
}

/**
 * A wrapper for bu_brlcad_root.
 *
 * @param clientData	- associated data/state
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
HIDDEN int
tcl_bu_brlcad_root(void *clientData,
		   int argc,
		   const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    if (argc != 2) {
	bu_log("Usage: bu_brlcad_root subdir\n");
	return BRLCAD_ERROR;
    }
    Tcl_AppendResult(interp, bu_brlcad_root(argv[1], 1), NULL);
    return BRLCAD_OK;
}


/**
 * A wrapper for bu_brlcad_data.
 *
 * @param clientData	- associated data/state
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
HIDDEN int
tcl_bu_brlcad_data(void *clientData,
		   int argc,
		   const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    if (argc != 2) {
	bu_log("Usage: bu_brlcad_data subdir\n");
	return BRLCAD_ERROR;
    }
    Tcl_AppendResult(interp, bu_brlcad_data(argv[1], 1), NULL);
    return BRLCAD_OK;
}


/**
 * A wrapper for bu_units_conversion.
 *
 * @param clientData	- associated data/state
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
HIDDEN int
tcl_bu_units_conversion(void *clientData,
			int argc,
			const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    double conv_factor;
    struct bu_vls result = BU_VLS_INIT_ZERO;

    if (argc != 2) {
	bu_log("Usage: bu_units_conversion units_string\n");
	return BRLCAD_ERROR;
    }

    conv_factor = bu_units_conversion(argv[1]);
    if (conv_factor <= 0.0) {
	bu_log("ERROR: bu_units_conversion: Unrecognized units string: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(&result, "%.12e", conv_factor);
    Tcl_AppendResult(interp, bu_vls_addr(&result), NULL);
    bu_vls_free(&result);
    return BRLCAD_OK;
}


static void
register_cmds(Tcl_Interp *interp, struct bu_cmdtab *cmds)
{
    struct bu_cmdtab *ctp = NULL;

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	(void)Tcl_CreateCommand(interp, ctp->ct_name, lwrapper_func, (ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
    }
}


int
Bu_Init(void *p)
{
    Tcl_Interp *interp = (Tcl_Interp *)p;

    static struct bu_cmdtab cmds[] = {
	{"bu_units_conversion",		tcl_bu_units_conversion},
	{"bu_brlcad_data",		tcl_bu_brlcad_data},
	{"bu_brlcad_dir",		tcl_bu_brlcad_dir},
	{"bu_brlcad_root",		tcl_bu_brlcad_root},
	{"bu_mem_barriercheck",		tcl_bu_mem_barriercheck},
	{"bu_prmem",			tcl_bu_prmem},
	{"bu_get_value_by_keyword",	tcl_bu_get_value_by_keyword},
	{"bu_rgb_to_hsv",		tcl_bu_rgb_to_hsv},
	{"bu_hsv_to_rgb",		tcl_bu_hsv_to_rgb},
	{(const char *)NULL, BU_CMD_NULL}
    };

    register_cmds(interp, cmds);

    Tcl_SetVar(interp, "BU_DEBUG_FORMAT", BU_DEBUG_FORMAT, TCL_GLOBAL_ONLY);
    Tcl_LinkVar(interp, "bu_debug", (char *)&bu_debug, TCL_LINK_INT);

    return BRLCAD_OK;
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

static int
tclcad_bn_dist_pt2_lseg2(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
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
tclcad_bn_mat_mul(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    mat_t o, a, b;
    if (argc < 3 || bn_decode_mat(a, argv[1]) < 16 ||
	    bn_decode_mat(b, argv[2]) < 16) {
	bu_vls_printf(&result, "usage: %s matA matB", argv[0]);
	goto error;
    }
    bn_mat_mul(o, a, b);
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
tclcad_bn_mat4x3pnt(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    mat_t m;
    point_t i, o;
    MAT_ZERO(m);
    if (argc < 3 || bn_decode_mat(m, argv[1]) < 16 ||
	    bn_decode_vect(i, argv[2]) < 3) {
	bu_vls_printf(&result, "usage: %s mat point", argv[0]);
	goto error;
    }
    bn_mat4x3pnt(o, m, i);
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
tclcad_bn_mat_ae(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    mat_t o;
    double az, el;

    if (argc < 3) {
	bu_vls_printf(&result, "usage: %s azimuth elevation", argv[0]);
	goto error;
    }
    if (Tcl_GetDouble(interp, argv[1], &az) != TCL_OK) goto error;
    if (Tcl_GetDouble(interp, argv[2], &el) != TCL_OK) goto error;

    bn_mat_ae(o, (fastf_t)az, (fastf_t)el);
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
tclcad_bn_mat_scale_about_pt_wrapper(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
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
tclcad_bn_mat_xform_about_pt(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    mat_t o, xform;
    vect_t v;

    if (argc < 3 || bn_decode_mat(xform, argv[1]) < 16 ||
	    bn_decode_vect(v, argv[2]) < 3) {
	bu_vls_printf(&result, "usage: %s xform pt", argv[0]);
	goto error;
    }

    bn_mat_xform_about_pt(o, xform, v);
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
    {"bn_dist_pt2_lseg2",	 BN_FUNC_TCL_CAST(tclcad_bn_dist_pt2_lseg2) },
    {"bn_isect_line2_line2",	 BN_FUNC_TCL_CAST(tclcad_bn_isect_line2_line2) },
    {"bn_isect_line3_line3",	 BN_FUNC_TCL_CAST(tclcad_bn_isect_line3_line3) },
    {"mat_mul",                  BN_FUNC_TCL_CAST(tclcad_bn_mat_mul) },
    {"mat_inv",                  BN_FUNC_TCL_CAST(tclcad_bn_mat_inv) },
    {"mat_trn",                  BN_FUNC_TCL_CAST(tclcad_bn_mat_trn) },
    {"matXvec",                  BN_FUNC_TCL_CAST(tclcad_bn_matXvec) },
    {"mat4x3vec",                BN_FUNC_TCL_CAST(tclcad_bn_mat4x3vec) },
    {"mat4x3pnt",                BN_FUNC_TCL_CAST(tclcad_bn_mat4x3pnt) },
    {"hdivide",                  BN_FUNC_TCL_CAST(tclcad_bn_hdivide) },
    {"vjoin1",	                 BN_FUNC_TCL_CAST(tclcad_bn_vjoin1) },
    {"vblend",	                 BN_FUNC_TCL_CAST(tclcad_bn_vblend) },
    {"mat_ae",                   BN_FUNC_TCL_CAST(tclcad_bn_mat_ae) },
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
    {"mat_scale_about_pt",       BN_FUNC_TCL_CAST(tclcad_bn_mat_scale_about_pt_wrapper) },
    {"mat_xform_about_pt",       BN_FUNC_TCL_CAST(tclcad_bn_mat_xform_about_pt) },
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

int
Sysv_Init(Tcl_Interp *UNUSED(interp))
{
    return TCL_OK;
}

int
Pkg_Init(Tcl_Interp *interp)
{
    if (!interp)
	return TCL_ERROR;
    return TCL_OK;
}

#define RT_FUNC_TCL_CAST(_func) ((int (*)(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv))_func)

static int tclcad_rt_shootray(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
static int tclcad_rt_onehit(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
static int tclcad_rt_no_bool(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
static int tclcad_rt_check(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
static int tclcad_rt_prep(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
static int tclcad_rt_set(ClientData clientData, Tcl_Interp *interp, int argc, const char *const*argv);


/************************************************************************
 *									*
 *		Tcl interface to Ray-tracing				*
 *									*
 ************************************************************************/

struct dbcmdstruct {
    char *cmdname;
    int (*cmdfunc)(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv);
};


static struct dbcmdstruct tclcad_rt_cmds[] = {
    {"shootray",	RT_FUNC_TCL_CAST(tclcad_rt_shootray)},
    {"onehit",		RT_FUNC_TCL_CAST(tclcad_rt_onehit)},
    {"no_bool",		RT_FUNC_TCL_CAST(tclcad_rt_no_bool)},
    {"check",		RT_FUNC_TCL_CAST(tclcad_rt_check)},
    {"prep",		RT_FUNC_TCL_CAST(tclcad_rt_prep)},
    {"cutter",		RT_FUNC_TCL_CAST(tclcad_rt_cutter)},
    {"set",		RT_FUNC_TCL_CAST(tclcad_rt_set)},
    {(char *)0,		RT_FUNC_TCL_CAST(0)}
};


int
tclcad_rt_parse_ray(Tcl_Interp *interp, struct xray *rp, const char *const*argv)
{
    if (bn_decode_vect(rp->r_pt,  argv[0]) != 3) {
	Tcl_AppendResult(interp,
			 "badly formatted point: ", argv[0], (char *)NULL);
	return TCL_ERROR;
    }
    if (bn_decode_vect(rp->r_dir, argv[2]) != 3) {
	Tcl_AppendResult(interp,
			 "badly formatted vector: ", argv[2], (char *)NULL);
	return TCL_ERROR;
    }
    switch (argv[1][0]) {
	case 'd':
	    /* [2] is direction vector */
	    break;
	case 'a':
	    /* [2] is target point, build a vector from start pt */
	    VSUB2(rp->r_dir, rp->r_dir, rp->r_pt);
	    break;
	default:
	    Tcl_AppendResult(interp,
			     "wrong ray keyword: '", argv[1],
			     "', should be one of 'dir' or 'at'",
			     (char *)NULL);
	    return TCL_ERROR;
    }
    VUNITIZE(rp->r_dir);
    return TCL_OK;
}


void
tclcad_rt_pr_cutter(Tcl_Interp *interp, const union cutter *cutp)
{
    static const char xyz[4] = "XYZ";
    struct bu_vls str = BU_VLS_INIT_ZERO;
    size_t i;

    switch (cutp->cut_type) {
	case CUT_CUTNODE:
	    bu_vls_printf(&str,
			  "type cutnode axis %c point %.25G",
			  xyz[cutp->cn.cn_axis], cutp->cn.cn_point);
	    break;
	case CUT_BOXNODE:
	    bu_vls_printf(&str,
			  "type boxnode min {%.25G %.25G %.25G}",
			  V3ARGS(cutp->bn.bn_min));
	    bu_vls_printf(&str,
			  " max {%.25G %.25G %.25G}",
			  V3ARGS(cutp->bn.bn_max));
	    bu_vls_printf(&str, " solids {");
	    for (i=0; i < cutp->bn.bn_len; i++) {
		bu_vls_strcat(&str, cutp->bn.bn_list[i]->st_name);
		bu_vls_putc(&str, ' ');
	    }
	    bu_vls_printf(&str, "} pieces {");
	    for (i = 0; i < cutp->bn.bn_piecelen; i++) {
		struct rt_piecelist *plp = &cutp->bn.bn_piecelist[i];
		size_t j;
		RT_CK_PIECELIST(plp);
		/* These can be taken by user positionally */
		bu_vls_printf(&str, "{%s {", plp->stp->st_name);
		for (j=0; j < plp->npieces; j++) {
		    bu_vls_printf(&str, "%ld ", plp->pieces[j]);
		}
		bu_vls_strcat(&str, "} } ");
	    }
	    bu_vls_strcat(&str, "}");
	    break;
	case CUT_NUGRIDNODE:
	    bu_vls_printf(&str, "type nugridnode");
	    for (i = 0; i < 3; i++) {
		bu_vls_printf(&str, " %c {", xyz[i]);
		bu_vls_printf(&str, "spos %.25G epos %.25G width %.25g",
			      cutp->nugn.nu_axis[i]->nu_spos,
			      cutp->nugn.nu_axis[i]->nu_epos,
			      cutp->nugn.nu_axis[i]->nu_width);
		bu_vls_printf(&str, " cells_per_axis %d",
			      cutp->nugn.nu_cells_per_axis[i]);
		bu_vls_printf(&str, " stepsize %d}",
			      cutp->nugn.nu_stepsize[i]);
	    }
	    break;
	default:
	    bu_vls_printf(&str, "tclcad_rt_pr_cutter() bad pointer cutp=%p",
			  (void *)cutp);
	    break;
    }
    Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
    bu_vls_free(&str);
}


/**
 * Obtain the 'n'th space partitioning cell along the given ray, and
 * return that to the user.
 *
 * Example -
 *	.rt cutter 7 {0 0 0} dir {0 0 -1}
 */
int
tclcad_rt_cutter(ClientData clientData, Tcl_Interp *interp, int argc, const char *const*argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;
    const union cutter *cutp;
    int n;

    if (argc != 6) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"",
			 argv[0], " ", argv[1], "cutnum {P} dir|at {V}\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    n = atoi(argv[2]);
    if (tclcad_rt_parse_ray(interp, &ap->a_ray, &argv[3]) == TCL_ERROR)
	return TCL_ERROR;

    cutp = rt_cell_n_on_ray(ap, n);
    if (cutp == CUTTER_NULL) {
	Tcl_AppendResult(interp, "rt_cell_n_on_ray() failed to find cell ", argv[2], (char *)NULL);
	return TCL_ERROR;
    }
    tclcad_rt_pr_cutter(interp, cutp);
    return TCL_OK;
}


void
tclcad_rt_pr_hit(Tcl_Interp *interp, struct hit *hitp, const struct seg *segp, int flipflag)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    vect_t norm;
    struct soltab *stp;
    const struct directory *dp;
    struct curvature crv = {{0.0, 0.0, 0.0}, 0.0, 0.0};

    RT_CK_SEG(segp);
    stp = segp->seg_stp;
    RT_CK_SOLTAB(stp);
    dp = stp->st_dp;
    RT_CK_DIR(dp);

    RT_HIT_NORMAL(norm, hitp, stp, rayp, flipflag);
    RT_CURVATURE(&crv, hitp, flipflag, stp);

    bu_vls_printf(&str, " {dist %g point {", hitp->hit_dist);
    bn_encode_vect(&str, hitp->hit_point, 1);
    bu_vls_printf(&str, "} normal {");
    bn_encode_vect(&str, norm, 1);
    bu_vls_printf(&str, "} c1 %g c2 %g pdir {",
		  crv.crv_c1, crv.crv_c2);
    bn_encode_vect(&str, crv.crv_pdir, 1);
    bu_vls_printf(&str, "} surfno %d", hitp->hit_surfno);
    if (stp->st_path.magic == DB_FULL_PATH_MAGIC) {
	/* Magic is left 0 if the path is not filled in. */
	char *sofar = db_path_to_string(&stp->st_path);
	bu_vls_printf(&str, " path ");
	bu_vls_strcat(&str, sofar);
	bu_free((void *)sofar, "path string");
    }
    bu_vls_printf(&str, " solid %s}", dp->d_namep);

    Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
    bu_vls_free(&str);
}


int
tclcad_rt_a_hit(struct application *ap,
	     struct partition *PartHeadp,
	     struct seg *UNUSED(segHeadp))
{
    Tcl_Interp *interp = (Tcl_Interp *)ap->a_uptr;
    register struct partition *pp;

    RT_CK_PT_HD(PartHeadp);

    for (pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	RT_CK_PT(pp);
	Tcl_AppendResult(interp, "{in", (char *)NULL);
	tclcad_rt_pr_hit(interp, pp->pt_inhit, pp->pt_inseg, pp->pt_inflip);
	Tcl_AppendResult(interp, "\nout", (char *)NULL);
	tclcad_rt_pr_hit(interp, pp->pt_outhit, pp->pt_outseg, pp->pt_outflip);
	Tcl_AppendResult(interp,
			 "\nregion ",
			 pp->pt_regionp->reg_name,
			 (char *)NULL);
	Tcl_AppendResult(interp, "}\n", (char *)NULL);
    }

    return 1;
}


int
tclcad_rt_a_miss(struct application *ap)
{
    if (ap) RT_CK_APPLICATION(ap);
    return 0;
}


/**
 * Usage -
 * procname shootray [-R] {P} dir|at {V}
 * -R option specifies no overlap reporting
 *
 * Example -
 * set glob_compat_mode 0
 *	.inmem rt_gettrees .rt all.g
 *	.rt shootray -R {0 0 0} dir {0 0 -1}
 *
 * set tgt [bu_get_value_by_keyword V [concat type [.inmem get LIGHT]]]
 *	.rt shootray {20 -13.5 20} at $tgt
 *
 *
 * Returns -
 * This "shootray" operation returns a nested set of lists. It
 * returns a list of zero or more partitions. Inside each
 * partition is a list containing an in, out, and region keyword,
 * each with an associated value. The associated value for each
 * "inhit" and "outhit" is itself a list containing a dist,
 * point, normal, surfno, and solid keyword, each with an
 * associated value.
 */
static int
tclcad_rt_shootray(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;
    int idx;

    if ((argc != 5 && argc != 6) || (argc == 6 && !BU_STR_EQUAL(argv[2], "-R"))) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"",
			 argv[0], " ", argv[1], " [-R] {P} dir|at {V}\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (argc == 6) {
	ap->a_logoverlap = rt_silent_logoverlap;
	idx = 3;
    } else {
	idx = 2;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    if (tclcad_rt_parse_ray(interp, &ap->a_ray, &argv[idx]) == TCL_ERROR)
	return TCL_ERROR;
    ap->a_hit = tclcad_rt_a_hit;
    ap->a_miss = tclcad_rt_a_miss;
    ap->a_uptr = (void *)interp;

    (void)rt_shootray(ap);

    return TCL_OK;
}


/**
 * Usage -
 * procname onehit
 * procname onehit #
 */
static int
tclcad_rt_onehit(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;
    char buf[64];

    if (argc < 2 || argc > 3) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"",
			 argv[0], " ", argv[1], " [#]\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    if (argc == 3) {
	ap->a_onehit = atoi(argv[2]);
    }
    sprintf(buf, "%d", ap->a_onehit);
    Tcl_AppendResult(interp, buf, (char *)NULL);
    return TCL_OK;
}


/**
 * Usage -
 * procname no_bool
 * procname no_bool #
 */
int
tclcad_rt_no_bool(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;
    char buf[64];

    if (argc < 2 || argc > 3) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"",
			 argv[0], " ", argv[1], " [#]\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    if (argc == 3) {
	ap->a_no_booleans = atoi(argv[2]);
    }
    sprintf(buf, "%d", ap->a_no_booleans);
    Tcl_AppendResult(interp, buf, (char *)NULL);
    return TCL_OK;
}


/**
 * Run some of the internal consistency checkers over the data
 * structures.
 *
 * Usage -
 * procname check
 */
int
tclcad_rt_check(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;

    if (argc != 2) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"",
			 argv[0], " ", argv[1], "\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    rt_ck(rtip);

    return TCL_OK;
}


/**
 * When run with no args, just prints current status of prepping.
 *
 * Usage -
 * procname prep
 * procname prep use_air [hasty_prep]
 */
int
tclcad_rt_prep(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (argc < 2 || argc > 4) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"",
			 argv[0], " ", argv[1],
			 " [hasty_prep]\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    if (argc >= 3 && !rtip->needprep) {
	Tcl_AppendResult(interp,
			 argv[0], " ", argv[1],
			 " invoked when model has already been prepped.\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (argc == 4) rtip->rti_hasty_prep = atoi(argv[3]);

    /* If args were given, prep now. */
    if (argc >= 3) rt_prep_parallel(rtip, 1);

    /* Now, describe the current state */
    bu_vls_printf(&str, "hasty_prep %d dont_instance %d useair %d needprep %d",
		  rtip->rti_hasty_prep,
		  rtip->rti_dont_instance,
		  rtip->useair,
		  rtip->needprep
	);

    bu_vls_printf(&str, " space_partition_type %s n_nugridnode %d n_cutnode %d n_boxnode %d n_empty %ld",
		  rtip->rti_space_partition == RT_PART_NUGRID ?
		  "NUGrid" : "NUBSP",
		  rtip->rti_ncut_by_type[CUT_NUGRIDNODE],
		  rtip->rti_ncut_by_type[CUT_CUTNODE],
		  rtip->rti_ncut_by_type[CUT_BOXNODE],
		  rtip->nempty_cells);
    bu_vls_printf(&str, " maxdepth %d maxlen %d",
		  rtip->rti_cut_maxdepth,
		  rtip->rti_cut_maxlen);
    if (rtip->rti_ncut_by_type[CUT_BOXNODE]) bu_vls_printf(&str, " avglen %g",
							   ((double)rtip->rti_cut_totobj) /
							   rtip->rti_ncut_by_type[CUT_BOXNODE]);

    Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
    bu_vls_free(&str);
    return TCL_OK;
}


/**
 * Set/get the rt object's settable variables.
 *
 * This replaces onehit and no_bool.
 *
 * Usage -
 * procname set
 * procname set vname
 * procname set vname val
 */
int
tclcad_rt_set(ClientData clientData, Tcl_Interp *interp, int argc, const char *const *argv)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    int val;
    const char *usage = "[vname [val]]";

    if (argc < 2 || argc > 4) {
	bu_vls_printf(&str, "%s %s: %s", argv[0], argv[1], usage);
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_ERROR;
    }

    RT_CK_APPLICATION(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    /* Return a list of the settable variables and their values */
    if (argc == 2) {
	bu_vls_printf(&str, "{onehit %d} ", ap->a_onehit);
	bu_vls_printf(&str, "{no_bool %d} ", ap->a_no_booleans);
	bu_vls_printf(&str, "{bot_reverse_normal_disabled %d}", ap->a_bot_reverse_normal_disabled);
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_OK;
    }

    if (argc == 4 && sscanf(argv[3], "%d", &val) != 1) {
	bu_vls_printf(&str, "%s %s: bad val - %s, must be an integer", argv[0], argv[1], argv[3]);
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_ERROR;
    }

    if (argv[2][0] == 'o' && !bu_strncmp(argv[2], "onehit", 6)) {
	if (argc == 3)
	    val = ap->a_onehit;
	else
	    ap->a_onehit = val;
    } else if (argv[2][0] == 'n' && !bu_strncmp(argv[2], "no_bool", 7)) {
	if (argc == 3)
	    val = ap->a_no_booleans;
	else
	    ap->a_no_booleans = val;
    } else if (argv[2][0] == 'b' && !bu_strncmp(argv[2], "bot_reverse_normal_disabled", 27)) {
	if (argc == 3)
	    val = ap->a_bot_reverse_normal_disabled;
	else
	    ap->a_bot_reverse_normal_disabled = val;
    } else {
	bu_vls_printf(&str, "%s %s: bad val - %s, must be one of the following: onehit, no_bool, or bot_reverse_normal_disabled",
		      argv[0], argv[1], argv[2]);
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_ERROR;
    }

    bu_vls_printf(&str, "%d", val);
    Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
    bu_vls_free(&str);
    return TCL_OK;
}


int
tclcad_rt(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    struct dbcmdstruct *dbcmd;

    if (argc < 2) {
	Tcl_AppendResult(interp,
			 "wrong # args: should be \"", argv[0],
			 " command [args...]\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    for (dbcmd = tclcad_rt_cmds; dbcmd->cmdname != NULL; dbcmd++) {
	if (BU_STR_EQUAL(dbcmd->cmdname, argv[1])) {
	    /* need proper cmd func pointer for actual call */
	    int (*_cmdfunc)(void*, Tcl_Interp*, int, const char* const*);
	    /* cast to the actual caller */
	    _cmdfunc = (int (*)(void*, Tcl_Interp*, int, const char* const*))(*dbcmd->cmdfunc);
	    return _cmdfunc(clientData, interp, argc, argv);
	}
    }


    Tcl_AppendResult(interp, "unknown LIBRT command '",
		     argv[1], "'; must be one of:",
		     (char *)NULL);
    for (dbcmd = tclcad_rt_cmds; dbcmd->cmdname != NULL; dbcmd++) {
	Tcl_AppendResult(interp, " ", dbcmd->cmdname, (char *)NULL);
    }
    return TCL_ERROR;
}


/************************************************************************
 *									*
 *		Tcl interface to Combination management			*
 *									*
 ************************************************************************/

int
tclcad_rt_import_from_path(Tcl_Interp *interp, struct rt_db_internal *ip, const char *path, struct rt_wdb *wdb)
{
    struct db_i *dbip;
    int status;

    /* Can't run RT_CK_DB_INTERNAL(ip), it hasn't been filled in yet */
    RT_CK_WDB(wdb);
    dbip = wdb->dbip;
    RT_CK_DBI(dbip);

    if (strchr(path, '/')) {
	/* This is a path */
	struct db_tree_state ts;
	struct db_full_path old_path;
	struct db_full_path new_path;
	struct directory *dp_curr;
	int ret;

	db_init_db_tree_state(&ts, dbip, &rt_uniresource);
	db_full_path_init(&old_path);
	db_full_path_init(&new_path);

	if (db_string_to_path(&new_path, dbip, path) < 0) {
	    Tcl_AppendResult(interp, "tclcad_rt_import_from_path: '",
			     path, "' contains unknown object names\n", (char *)NULL);
	    return TCL_ERROR;
	}

	dp_curr = DB_FULL_PATH_CUR_DIR(&new_path);
	if (!dp_curr)
	    return TCL_ERROR;

	ret = db_follow_path(&ts, &old_path, &new_path, LOOKUP_NOISY, 0);
	db_free_full_path(&old_path);
	db_free_full_path(&new_path);

	if (ret < 0) {
	    Tcl_AppendResult(interp, "tclcad_rt_import_from_path: '",
			     path, "' is a bad path\n", (char *)NULL);
	    return TCL_ERROR;
	}

	status = wdb_import(wdb, ip, dp_curr->d_namep, ts.ts_mat);
	if (status == -4) {
	    Tcl_AppendResult(interp, dp_curr->d_namep,
			     " not found in path ", path, "\n",
			     (char *)NULL);
	    return TCL_ERROR;
	}
	if (status < 0) {
	    Tcl_AppendResult(interp, "wdb_import failure: ",
			     dp_curr->d_namep, (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	status = wdb_import(wdb, ip, path, (matp_t)NULL);
	if (status == -4) {
	    Tcl_AppendResult(interp, path, ": not found\n",
			     (char *)NULL);
	    return TCL_ERROR;
	}
	if (status < 0) {
	    Tcl_AppendResult(interp, "wdb_import failure: ",
			     path, (char *)NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}


void
tclcad_rt_setup(Tcl_Interp *interp)
{
    Tcl_LinkVar(interp, "rt_bot_minpieces", (char *)&rt_bot_minpieces, TCL_LINK_WIDE_INT);

    Tcl_LinkVar(interp, "rt_bot_tri_per_piece",
		(char *)&rt_bot_tri_per_piece, TCL_LINK_WIDE_INT);
}


int
Rt_Init(Tcl_Interp *interp)
{
    /*XXX how much will this break? */
    if (!BU_LIST_IS_INITIALIZED(&RTG.rtg_vlfree)) {
	if (bu_avail_cpus() > 1) {
	    RTG.rtg_parallel = 1;
	}

	/* initialize RT's global state */
	BU_LIST_INIT(&RTG.rtg_vlfree);
	BU_LIST_INIT(&RTG.rtg_headwdb.l);
	if (rt_uniresource.re_magic != RESOURCE_MAGIC) {
	    rt_init_resource(&rt_uniresource, 0, NULL);
	}
    }

    tclcad_rt_setup(interp);

    Tcl_PkgProvide(interp,  "Rt", brlcad_version());

    return TCL_OK;
}


/* ====================================================================== */

/* TCL-oriented C support for LIBRT */


void
db_full_path_appendresult(Tcl_Interp *interp, const struct db_full_path *pp)
{
    size_t i;

    RT_CK_FULL_PATH(pp);

    for (i=0; i<pp->fp_len; i++) {
	Tcl_AppendResult(interp, "/", pp->fp_names[i]->d_namep, (char *)NULL);
    }
}


int
tcl_obj_to_int_array(Tcl_Interp *interp, Tcl_Obj *list, int **array, int *array_len)
{
    Tcl_Obj **obj_array;
    int len, i;

    if (Tcl_ListObjGetElements(interp, list, &len, &obj_array) != TCL_OK)
	return 0;

    if (len < 1)
	return 0;

    if (*array_len < 1) {
	*array = (int *)bu_calloc(len, sizeof(int), "array");
	*array_len = len;
    }

    for (i=0; i<len && i<*array_len; i++) {
	(*array)[i] = atoi(Tcl_GetStringFromObj(obj_array[i], NULL));
	Tcl_DecrRefCount(obj_array[i]);
    }

    return len < *array_len ? len : *array_len;
}


int
tcl_list_to_int_array(Tcl_Interp *interp, char *char_list, int **array, int *array_len)
{
    Tcl_Obj *obj;
    int ret;

    obj = Tcl_NewStringObj(char_list, -1);

    ret = tcl_obj_to_int_array(interp, obj, array, array_len);

    return ret;
}


int
tcl_obj_to_fastf_array(Tcl_Interp *interp, Tcl_Obj *list, fastf_t **array, int *array_len)
{
    Tcl_Obj **obj_array;
    int len, i;
    int ret;

    if ((ret=Tcl_ListObjGetElements(interp, list, &len, &obj_array)) != TCL_OK)
	return ret;

    if (len < 1)
	return 0;

    if (*array_len < 1) {
	*array = (fastf_t *)bu_calloc(len, sizeof(fastf_t), "array");
	*array_len = len;
    }

    for (i=0; i<len && i<*array_len; i++) {
	(*array)[i] = atof(Tcl_GetStringFromObj(obj_array[i], NULL));
	Tcl_DecrRefCount(obj_array[i]);
    }

    return len < *array_len ? len : *array_len;
}


int
tcl_list_to_fastf_array(Tcl_Interp *interp, const char *char_list, fastf_t **array, int *array_len)
{
    Tcl_Obj *obj;
    int ret;

    obj = Tcl_NewStringObj(char_list, -1);

    ret = tcl_obj_to_fastf_array(interp, obj, array, array_len);

    return ret;
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
