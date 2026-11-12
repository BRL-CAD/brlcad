/*                       F A C E D E F . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
 */
/** @file libged/facedef.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>


#include "vmath.h"
#include "bu/interrupt.h"
#include "bu/arg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "../ged_private.h"
#include "./ged_edit.h"


char *p_rotfb[] = {
    "Enter rot, fb angles: ",
    "Enter fb angle: ",
    "Enter fixed vertex(v#) or point(X Y Z): ",
    "Enter Y, Z of point: ",
    "Enter Z of point: "
};


char *p_3pts[] = {
    "Enter X, Y, Z of point",
    "Enter Y, Z of point",
    "Enter Z of point"
};


char *p_pleqn[] = {
    "Enter A, B, C, D of plane equation: ",
    "Enter B, C, D of plane equation: ",
    "Enter C, D of plane equation: ",
    "Enter D of plane equation: "
};


char *p_nupnt[] = {
    "Enter X, Y, Z of fixed point: ",
    "Enter Y, Z of fixed point: ",
    "Enter Z of fixed point: "
};

static int
parse_fastf_args(struct ged *gedp, fastf_t *values, size_t count,
	const char *argv[])
{
    for (size_t i = 0; i < count; ++i) {
	if (bu_arg_number_from_str(NULL, (fastf_t *)(&values[i]), argv[i]) != 1 ||
	    !isfinite(values[i])) {
	    bu_vls_printf(gedp->ged_result_str,
		    "facedef: bad numeric value - %s\n", argv[i]);
	    return BRLCAD_ERROR;
	}
    }

    return BRLCAD_OK;
}


/*
 * Gets the planar equation from the array argv[] and puts the result
 * into 'plane'.
 */
static int
get_pleqn(struct ged *gedp, fastf_t *plane, const char *argv[])
{
    if (parse_fastf_args(gedp, plane, ELEMENTS_PER_PLANE, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (NEAR_ZERO(MAGNITUDE(plane), SMALL_FASTF)) {
	bu_vls_printf(gedp->ged_result_str,
		"facedef: plane normal must be non-zero\n");
	return BRLCAD_ERROR;
    }

    VUNITIZE(&plane[0]);
    plane[W] *= gedp->dbip->dbi_local2base;
    return BRLCAD_OK;
}


/*
 * Gets three definite points from the array argv[] and finds the
 * planar equation from these points.  The resulting plane equation is
 * stored in 'plane'.
 *
 * Returns -
 * 0 success
 * -1 failure
 */
static int
get_3pts(struct ged *gedp, fastf_t *plane, const char *argv[], const struct bn_tol *tol)
{
    point_t a, b, c;

    if (parse_fastf_args(gedp, a, ELEMENTS_PER_POINT, argv) != BRLCAD_OK ||
	parse_fastf_args(gedp, b, ELEMENTS_PER_POINT, argv + 3) != BRLCAD_OK ||
	parse_fastf_args(gedp, c, ELEMENTS_PER_POINT, argv + 6) != BRLCAD_OK)
	return BRLCAD_ERROR;
    VSCALE(a, a, gedp->dbip->dbi_local2base);
    VSCALE(b, b, gedp->dbip->dbi_local2base);
    VSCALE(c, c, gedp->dbip->dbi_local2base);

    if (bg_make_plane_3pnts(plane, a, b, c, tol) < 0) {
	bu_vls_printf(gedp->ged_result_str, "facedef: not a plane\n");
	return -1;		/* failure */
    }
    return 0;			/* success */
}


/*
 * Gets information from the array argv[].  Finds the planar equation
 * given rotation and fallback angles, plus a fixed point. Result is
 * stored in 'plane'.  A named ARB vertex may be used as the fixed point.
 */
static int
get_rotfb(struct ged *gedp, fastf_t *plane, const char *argv[], const struct rt_arb_internal *arb)
{
    fastf_t rota, fb_a;
    point_t pt;

    fastf_t angles[2];
    if (parse_fastf_args(gedp, angles, 2, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;
    rota = angles[0] * DEG2RAD;
    fb_a = angles[1] * DEG2RAD;

    /* calculate normal vector (length=1) from rot, fb */
    plane[0] = cos(fb_a) * cos(rota);
    plane[1] = cos(fb_a) * sin(rota);
    plane[2] = sin(fb_a);

    if (argv[2][0] == 'v') {
	int vertex;
	const char *vertex_arg = argv[2] + 1;

	if (bu_arg_integer_from_str(NULL, (int *)(&vertex), vertex_arg, 0) != 1 ||
	    vertex < 1 || vertex > 8) {
	    bu_vls_printf(gedp->ged_result_str, "facedef: bad vertex - %s\n", argv[2]);
	    return -1;
	}
	plane[W]= VDOT(&plane[0], arb->pt[vertex - 1]);
    } else {
	/* definite point given */
	if (parse_fastf_args(gedp, pt, ELEMENTS_PER_POINT, argv + 2) != BRLCAD_OK)
	    return BRLCAD_ERROR;
	VSCALE(pt, pt, gedp->dbip->dbi_local2base);
	plane[W]=VDOT(&plane[0], pt);
    }

    return 0;
}


/*
 * Gets a point from the three strings in the 'argv' array.  The value
 * of D of 'plane' is changed such that the plane passes through the
 * input point.
 */
static int
get_nupnt(struct ged *gedp, fastf_t *plane, const char *argv[])
{
    point_t pt;

    if (parse_fastf_args(gedp, pt, ELEMENTS_PER_POINT, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;
    VSCALE(pt, pt, gedp->dbip->dbi_local2base);
    plane[W] = VDOT(&plane[0], pt);
    return BRLCAD_OK;
}

/*

 * Redefines one of the defining planes for a GENARB8. Finds which
 * plane to redefine and gets input, then shuttles the process over to
 * one of four functions before calculating new vertices.
 */
int
edarb_facedef(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    int type;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    short int i;
    int face, prod, plane;
    plane_t planes[6];
    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
    static const char *usage = "arb face [a|b|c|d parameters]";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    GED_DB_LOOKUP(gedp, dp, (char *)argv[2], LOOKUP_QUIET, BRLCAD_ERROR);
    GED_DB_GET_INTERN(gedp, &intern, dp, (matp_t)NULL, BRLCAD_ERROR);
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (intern.idb_type != ID_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: solid type must be ARB\n", argv[0], argv[1]);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    type = rt_arb_std_type(&intern, &wdbp->wdb_tol);
    if (type < ARB4 || type > ARB8) {
	bu_vls_printf(gedp->ged_result_str, "unrecognized ARB type\n");
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    /* find new planes to account for any editing */
    if (rt_arb_calc_planes(&error_msg, arb, type, planes, &wdbp->wdb_tol)) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&error_msg));
	bu_vls_free(&error_msg);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&error_msg);

    /* get face, initialize args and argcnt */
    if (bu_arg_integer_from_str(NULL, (int *)(&face), argv[3], 0) != 1) {
	bu_vls_printf(gedp->ged_result_str, "facedef: bad face - %s\n", argv[3]);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    /* use product of vertices to distinguish faces */
    for (i=0, prod=1;i<4;i++) {
	if (face > 0) {
	    prod *= face%10;
	    face /= 10;
	}
    }

    switch (prod) {
	case 6:			/* face 123 of arb4 */
	case 24:plane=0;	/* face 1234 of arb8 */
	    /* face 1234 of arb7 */
	    /* face 1234 of arb6 */
	    /* face 1234 of arb5 */
	    if (type==4 && prod==24)
		plane=2; 	/* face 234 of arb4 */
	    break;
	case 8:			/* face 124 of arb4 */
	case 180: 		/* face 2365 of arb6 */
	case 210:		/* face 567 of arb7 */
	case 1680:plane=1;      /* face 5678 of arb8 */
	    break;
	case 30:		/* face 235 of arb5 */
	case 120:		/* face 1564 of arb6 */
	case 20:      		/* face 145 of arb7 */
	case 160:plane=2;	/* face 1584 of arb8 */
	    if (type==5)
		plane=4; 	/* face 145 of arb5 */
	    break;
	case 12:		/* face 134 of arb4 */
	case 10:		/* face 125 of arb6 */
	case 252:plane=3;	/* face 2376 of arb8 */
	    /* face 2376 of arb7 */
	    if (type==5)
		plane=1; 	/* face 125 of arb5 */
	    break;
	case 72:               	/* face 346 of arb6 */
	case 60:plane=4;	/* face 1265 of arb8 */
	    /* face 1265 of arb7 */
	    if (type==5)
		plane=3; 	/* face 345 of arb5 */
	    break;
	case 420:		/* face 4375 of arb7 */
	case 672:plane=5;	/* face 4378 of arb8 */
	    break;
	default:
	    {
		bu_vls_printf(gedp->ged_result_str, "bad face (product=%d)\n", prod);
		rt_db_free_internal(&intern);
		return BRLCAD_ERROR;
	    }
    }

    if (argc < 5) {
	/* menu of choices for plane equation definition */
	bu_vls_printf(gedp->ged_result_str, "\ta   planar equation\n\
\tb   3 points\n\
\tc   rot, fb angles + fixed pt\n\
\td   same plane thru fixed pt\n\
\tq   quit\n\
Enter form of new face definition: ");
	rt_db_free_internal(&intern);
	return GED_MORE;
    }

    switch (argv[4][0]) {
	case 'a':
	    /* special case for arb7, because of 2 4-pt planes meeting */
	    if (type == 7)
		if (plane!=0 && plane!=3) {
		    bu_vls_printf(gedp->ged_result_str, "facedef: can't redefine that arb7 plane\n");
		    rt_db_free_internal(&intern);
		    return BRLCAD_ERROR;
		}
	    if (argc < 9) {
		/* total # of args under this option */
		bu_vls_printf(gedp->ged_result_str, "%s", p_pleqn[argc-5]);
		rt_db_free_internal(&intern);
		return GED_MORE;
	    }
	    if (get_pleqn(gedp, planes[plane], &argv[5]) != BRLCAD_OK) {
		rt_db_free_internal(&intern);
		return BRLCAD_ERROR;
	    }
	    break;
	case 'b':
	    /* special case for arb7, because of 2 4-pt planes meeting */
	    if (type == 7)
		if (plane!=0 && plane!=3) {
		    bu_vls_printf(gedp->ged_result_str, "facedef: can't redefine that arb7 plane\n");
		    rt_db_free_internal(&intern);
		    return BRLCAD_ERROR;
		}
	    if (argc < 14) {
		/* total # of args under this option */
		bu_vls_printf(gedp->ged_result_str, "%s %d: ", p_3pts[(argc-5)%3], (argc-2)/3);
		rt_db_free_internal(&intern);
		return GED_MORE;
	    }
	    if (get_3pts(gedp, planes[plane], &argv[5], &wdbp->wdb_tol)) {
		rt_db_free_internal(&intern);
		return BRLCAD_ERROR;
	    }
	    break;
	case 'c': {
	    const char *arb7_args[3];
	    const char **rotfb_args = &argv[5];

	    /* special case for arb7, because of 2 4-pt planes meeting */
	    if (type == 7 && (plane != 0 && plane != 3)) {
		if (argc < 7) {
		    bu_vls_printf(gedp->ged_result_str, "%s", p_rotfb[argc-5]);
		    rt_db_free_internal(&intern);
		    return GED_MORE;
		}

		arb7_args[0] = argv[5];
		arb7_args[1] = argv[6];
		arb7_args[2] = "v5";
		rotfb_args = arb7_args;
		bu_vls_printf(gedp->ged_result_str, "Fixed point is vertex five.\n");
	    }
	    /* total # of args under this option */
	    else if (argc < 8 || (argv[7][0] != 'v' && argc < 10)) {
		bu_vls_printf(gedp->ged_result_str, "%s", p_rotfb[argc-5]);
		rt_db_free_internal(&intern);
		return GED_MORE;
	    }
	    if (get_rotfb(gedp, planes[plane], rotfb_args, arb)) {
		rt_db_free_internal(&intern);
		return BRLCAD_ERROR;
	    }
	    break;
	}
	case 'd':
	    /* special case for arb7, because of 2 4-pt planes meeting */
	    if (type == 7)
		if (plane!=0 && plane!=3) {
		    bu_vls_printf(gedp->ged_result_str, "facedef: can't redefine that arb7 plane\n");
		    rt_db_free_internal(&intern);
		    return BRLCAD_ERROR;
		}
	    if (argc < 8) {
		bu_vls_printf(gedp->ged_result_str, "%s", p_nupnt[argc-5]);
		rt_db_free_internal(&intern);
		return GED_MORE;
	    }
	    if (get_nupnt(gedp, planes[plane], &argv[5]) != BRLCAD_OK) {
		rt_db_free_internal(&intern);
		return BRLCAD_ERROR;
	    }
	    break;
	case 'q':
	    rt_db_free_internal(&intern);
	    return BRLCAD_OK;
	default:
	    bu_vls_printf(gedp->ged_result_str, "facedef: %s is not an option\n", argv[4]);
	    rt_db_free_internal(&intern);
	    return BRLCAD_ERROR;
    }

    /* find all vertices from the plane equations */
    if (rt_arb_calc_points(arb, type, (const plane_t *)planes, &wdbp->wdb_tol) < 0) {
	bu_vls_printf(gedp->ged_result_str, "facedef:  unable to find points\n");
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);
    rt_db_free_internal(&intern);

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
