/*                         E D A R B . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2014 United States Government as represented by
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
/** @file libged/edarb.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "rt/arb_edit.h"
#include "ged_private.h"

/*
 * An ARB edge is moved by finding the direction of the line
 * containing the edge and the 2 "bounding" planes.  The new edge is
 * found by intersecting the new line location with the bounding
 * planes.  The two "new" planes thus defined are calculated and the
 * affected points are calculated by intersecting planes.  This keeps
 * ALL faces planar.
 *
 */
static int
editarb(struct ged *gedp, struct rt_arb_internal *arb, int type, int edge, vect_t pos_model, int newedge)
{
    static int pt1, pt2, bp1, bp2, newp, p1, p2, p3;
    const short *edptr;		/* pointer to arb edit array */
    const short *final;		/* location of points to redo */
    static int i;
    const int *iptr;
    int pflag = 0;
    fastf_t peqn[7][4];
    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
    const short earb8[12][18] = earb8_edit_array;
    const short earb7[12][18] = earb7_edit_array;
    const short earb6[10][18] = earb6_edit_array;
    const short earb5[9][18] = earb5_edit_array;
    const short earb4[5][18] = earb4_edit_array;

    if (rt_arb_calc_planes(&error_msg, arb, type, peqn, &gedp->ged_wdbp->wdb_tol)) {
	bu_vls_printf(gedp->ged_result_str, "%s. Cannot calculate plane equations for faces\n", bu_vls_addr(&error_msg));
	bu_vls_free(&error_msg);
	return GED_ERROR;
    }

    /* set the pointer */
    switch (type) {
	case ARB4:
	    edptr = &earb4[edge][0];
	    final = &earb4[edge][16];
	    pflag = 1;
	    break;
	case ARB5:
	    edptr = &earb5[edge][0];
	    final = &earb5[edge][16];
	    if (edge == 8)
		pflag = 1;
	    break;
	case ARB6:
	    edptr = &earb6[edge][0];
	    final = &earb6[edge][16];
	    if (edge > 7)
		pflag = 1;
	    break;
	case ARB7:
	    edptr = &earb7[edge][0];
	    final = &earb7[edge][16];
	    if (edge == 11)
		pflag = 1;
	    break;
	case ARB8:
	    edptr = &earb8[edge][0];
	    final = &earb8[edge][16];
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "edarb: unknown ARB type\n");
	    return GED_ERROR;
    }

    /* do the arb editing */

    if (pflag) {
	/* moving a point - not an edge */
	VMOVE(arb->pt[edge], pos_model);
	edptr += 4;
    } else {
	vect_t edge_dir;

	/* moving an edge */
	pt1 = *edptr++;
	pt2 = *edptr++;
	/* direction of this edge */
	if (newedge) {
	    /* edge direction comes from edgedir() in pos_model */
	    VMOVE(edge_dir, pos_model);
	    VMOVE(pos_model, arb->pt[pt1]);
	    newedge = 0;
	} else {
	    /* must calculate edge direction */
	    VSUB2(edge_dir, arb->pt[pt2], arb->pt[pt1]);
	}
	if (ZERO(MAGNITUDE(edge_dir)))
	    goto err;
	/* bounding planes bp1, bp2 */
	bp1 = *edptr++;
	bp2 = *edptr++;

	/* move the edge */
	if (mv_edge(arb, pos_model, bp1, bp2, pt1, pt2, edge_dir ,&gedp->ged_wdbp->wdb_tol, peqn)) {
	    bu_vls_printf(gedp->ged_result_str, "edge (direction) parallel to face normal\n");
	    goto err;
	}
    }

    /* editing is done - insure planar faces */
    /* redo plane eqns that changed */
    newp = *edptr++; 	/* plane to redo */

    if (newp == 9) {
	/* special flag --> redo all the planes */
	if (rt_arb_calc_planes(&error_msg, arb, type, peqn, &gedp->ged_wdbp->wdb_tol)) {
	    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&error_msg));
	    bu_vls_free(&error_msg);
	    goto err;
	}
    }
    bu_vls_free(&error_msg);

    if (newp >= 0 && newp < 6) {
	for (i=0; i<3; i++) {
	    /* redo this plane (newp), use points p1, p2, p3 */
	    p1 = *edptr++;
	    p2 = *edptr++;
	    p3 = *edptr++;
	    if (bn_mk_plane_3pts(peqn[newp], arb->pt[p1], arb->pt[p2],
				 arb->pt[p3], &gedp->ged_wdbp->wdb_tol))
		goto err;

	    /* next plane */
	    if ((newp = *edptr++) == -1 || newp == 8)
		break;
	}
    }
    if (newp == 8) {
	const int arb_faces[5][24] = rt_arb_faces;
	/* special...redo next planes using pts defined in faces */
	for (i=0; i<3; i++) {
	    if ((newp = *edptr++) == -1)
		break;
	    iptr = &arb_faces[type-4][4*newp];
	    p1 = *iptr++;
	    p2 = *iptr++;
	    p3 = *iptr++;
	    if (bn_mk_plane_3pts(peqn[newp], arb->pt[p1], arb->pt[p2],
				 arb->pt[p3], &gedp->ged_wdbp->wdb_tol))
		goto err;
	}
    }

    /* the changed planes are all redone push necessary points back
     * into the planes.
     */
    edptr = final;	/* point to the correct location */
    for (i=0; i<2; i++) {
	if ((p1 = *edptr++) == -1)
	    break;
	/* intersect proper planes to define vertex p1 */
	if (rt_arb_3face_intersect(arb->pt[p1], (const plane_t *)peqn, type, p1*3))
	    goto err;
    }

    /* Special case for ARB7: move point 5 .... must recalculate plane
     * 2 = 456
     */
    if (type == ARB7 && pflag) {
	if (bn_mk_plane_3pts(peqn[2], arb->pt[4], arb->pt[5], arb->pt[6], &gedp->ged_wdbp->wdb_tol))
	    goto err;
    }

    /* carry along any like points */
    switch (type) {
	case ARB8:
	    break;

	case ARB7:
	    VMOVE(arb->pt[7], arb->pt[4]);
	    break;

	case ARB6:
	    VMOVE(arb->pt[5], arb->pt[4]);
	    VMOVE(arb->pt[7], arb->pt[6]);
	    break;

	case ARB5:
	    for (i=5; i<8; i++)
		VMOVE(arb->pt[i], arb->pt[4]);
	    break;

	case ARB4:
	    VMOVE(arb->pt[3], arb->pt[0]);
	    for (i=5; i<8; i++)
		VMOVE(arb->pt[i], arb->pt[4]);
	    break;
    }

    return GED_OK;

 err:
    /* Error handling */
    bu_vls_printf(gedp->ged_result_str, "cannot move edge: %d%d\n", pt1+1, pt2+1);

    return GED_ERROR;
}


/* Extrude command - project an arb face */
/* Format: extrude face distance */
static int
edarb_extrude(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    int type;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    int face;
    fastf_t dist;
    fastf_t peqn[7][4];
    static const char *usage = "arb face distance";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_ERROR;
    }

    GED_DB_LOOKUP(gedp, dp, (char *)argv[2], LOOKUP_QUIET, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, GED_ERROR);

    if (intern.idb_type != ID_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: solid type must be ARB\n", argv[0], argv[1]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    type = rt_arb_std_type(&intern, &gedp->ged_wdbp->wdb_tol);
    if (type != 8 && type != 6 && type != 4) {
	bu_vls_printf(gedp->ged_result_str, "ARB%d: extrusion of faces not allowed\n", type);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    face = atoi(argv[3]);

    /* get distance to project face */
    dist = atof(argv[4]);

    /* convert from the local unit (as input) to the base unit */
    dist = dist * gedp->ged_wdbp->dbip->dbi_local2base;

    if (arb_extrude(arb, face, dist, &gedp->ged_wdbp->wdb_tol, peqn)) {
	bu_vls_printf(gedp->ged_result_str, "ARB%d: error extruding face\n", type);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    rt_db_free_internal(&intern);

    return GED_OK;
}


/* Mirface command - mirror an arb face */
/* Format: mirror face axis */
int
edarb_mirface(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    int type;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    int i, j, k;
    static int face;
    static int pt[4];
    static int prod;
    static vect_t work;
    fastf_t peqn[7][4];
    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
    static const char *usage = "arb face axis";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_ERROR;
    }

    GED_DB_LOOKUP(gedp, dp, (char *)argv[2], LOOKUP_QUIET, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, GED_ERROR);

    if (intern.idb_type != ID_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: solid type must be ARB\n", argv[0], argv[1]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    type = rt_arb_std_type(&intern, &gedp->ged_wdbp->wdb_tol);
    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    if (type != ARB8 && type != ARB6) {
	bu_vls_printf(gedp->ged_result_str, "ARB%d: mirroring of faces not allowed\n", type);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }
    face = atoi(argv[3]);
    if (face > 9999 || (face < 1000 && type != ARB6)) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s bad face\n", argv[3]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }
    /* check which axis */
    k = -1;
    if (BU_STR_EQUAL(argv[4], "x"))
	k = 0;
    if (BU_STR_EQUAL(argv[4], "y"))
	k = 1;
    if (BU_STR_EQUAL(argv[4], "z"))
	k = 2;
    if (k < 0) {
	bu_vls_printf(gedp->ged_result_str, "axis must be x, y or z\n");
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    work[0] = work[1] = work[2] = 1.0;
    work[k] = -1.0;

    if (type == ARB6 && face < 1000) {
	/* 3 point face */
	pt[0] = face / 100;
	i = face - (pt[0]*100);
	pt[1] = i / 10;
	pt[2] = i - (pt[1]*10);
	pt[3] = 1;
    } else {
	pt[0] = face / 1000;
	i = face - (pt[0]*1000);
	pt[1] = i / 100;
	i = i - (pt[1]*100);
	pt[2] = i / 10;
	pt[3] = i - (pt[2]*10);
    }

    /* user can input face in any order - will use product of
     * face points to distinguish faces:
     *    product       face
     *       24         1234 for ARB8
     *     1680         5678 for ARB8
     *      252         2367 for ARB8
     *      160         1548 for ARB8
     *      672         4378 for ARB8
     *       60         1256 for ARB8
     *	     10	         125 for ARB6
     *	     72	         346 for ARB6
     */
    prod = 1;
    for (i = 0; i <= 3; i++) {
	prod *= pt[i];
	pt[i]--;
	if (pt[i] > 7) {
	    bu_vls_printf(gedp->ged_result_str, "bad face: %s\n", argv[3]);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}
    }

    /* mirror the selected face */
    switch (prod) {

	case 24:   /* mirror face 1234 */
	    if (type == ARB6) {
		bu_vls_printf(gedp->ged_result_str, "ARB6: mirroring of face %s not allowed\n", argv[3]);
		rt_db_free_internal(&intern);
		return GED_ERROR;
	    }
	    for (i = 0; i < 4; i++) {
		j = i + 4;
		VELMUL(arb->pt[j], arb->pt[i], work);
	    }
	    break;

	case 1680:   /* mirror face 5678 */
	    for (i = 0; i < 4; i++) {
		j = i + 4;
		VELMUL(arb->pt[i], arb->pt[j], work);
	    }
	    break;

	case 60:   /* mirror face 1256 */
	case 10:	/* mirror face 125 of ARB6 */
	    VELMUL(arb->pt[3], arb->pt[0], work);
	    VELMUL(arb->pt[2], arb->pt[1], work);
	    VELMUL(arb->pt[7], arb->pt[4], work);
	    VELMUL(arb->pt[6], arb->pt[5], work);
	    break;

	case 672:   /* mirror face 4378 */
	case 72:	/* mirror face 346 of ARB6 */
	    VELMUL(arb->pt[0], arb->pt[3], work);
	    VELMUL(arb->pt[1], arb->pt[2], work);
	    VELMUL(arb->pt[5], arb->pt[6], work);
	    VELMUL(arb->pt[4], arb->pt[7], work);
	    break;

	case 252:   /* mirror face 2367 */
	    VELMUL(arb->pt[0], arb->pt[1], work);
	    VELMUL(arb->pt[3], arb->pt[2], work);
	    VELMUL(arb->pt[4], arb->pt[5], work);
	    VELMUL(arb->pt[7], arb->pt[6], work);
	    break;

	case 160:   /* mirror face 1548 */
	    VELMUL(arb->pt[1], arb->pt[0], work);
	    VELMUL(arb->pt[5], arb->pt[4], work);
	    VELMUL(arb->pt[2], arb->pt[3], work);
	    VELMUL(arb->pt[6], arb->pt[7], work);
	    break;

	case 120:
	case 180:
	    bu_vls_printf(gedp->ged_result_str, "ARB6: mirroring of face %s not allowed\n", argv[3]);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad face: %s\n", argv[3]);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
    }

    /* redo the plane equations */
    if (rt_arb_calc_planes(&error_msg, arb, type, peqn, &gedp->ged_wdbp->wdb_tol)) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&error_msg));
	bu_vls_free(&error_msg);
	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    rt_db_free_internal(&intern);

    return GED_OK;
}


/* Edgedir command: define the direction of an arb edge being moved
 * Format: edgedir deltax deltay deltaz OR edgedir rot fb
*/
static int
edarb_edgedir(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    int type;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    int ret;
    int i;
    int edge;
    vect_t slope;
    fastf_t rot, fb;
    static const char *usage = "arb edge [delta_x delta_y delta_z] | [rot fb]";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (argc < 6 || 7 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_ERROR;
    }

    if (sscanf(argv[3], "%d", &edge) != 1) {
	bu_vls_printf(gedp->ged_result_str, "bad edge - %s", argv[3]);
	return GED_ERROR;
    }
    edge -= 1;

    GED_DB_LOOKUP(gedp, dp, (char *)argv[2], LOOKUP_QUIET, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, GED_ERROR);

    if (intern.idb_type != ID_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: solid type must be ARB\n", argv[0], argv[1]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    type = rt_arb_std_type(&intern, &gedp->ged_wdbp->wdb_tol);
    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    /* set up slope -
     * if 2 values input assume rot, fb used
     * else assume delta_x, delta_y, delta_z
     */
    if (argc == 6) {
	rot = atof(argv[1]) * DEG2RAD;
	fb = atof(argv[2]) * DEG2RAD;
	slope[0] = cos(fb) * cos(rot);
	slope[1] = cos(fb) * sin(rot);
	slope[2] = sin(fb);
    } else {
	for (i=0; i<3; i++) {
	    /* put edge slope in slope[] array */
	    slope[i] = atof(argv[i+4]);
	}
    }

    if (ZERO(MAGNITUDE(slope))) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: BAD slope\n", argv[0], argv[1]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    ret = editarb(gedp, arb, type, edge, slope, 1);
    if (ret == GED_OK) {
	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    }

    rt_db_free_internal(&intern);

    return ret;
}


/* Permute command - permute the vertex labels of an ARB
 * Format: permute tuple */

/*
 *	     Minimum and maximum tuple lengths
 *     ------------------------------------------------
 *	Solid	# vertices needed	# vertices
 *	type	 to disambiguate	in THE face
 *     ------------------------------------------------
 *	ARB4		3		    3
 *	ARB5		2		    4
 *	ARB6		2		    4
 *	ARB7		1		    4
 *	ARB8		3		    4
 *     ------------------------------------------------
 */
static int
edarb_permute(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    static const char *usage = "arb tuple";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: edarb %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: edarb %s %s %s", argv[0], argv[1], usage);
	return GED_ERROR;
    }

    GED_DB_LOOKUP(gedp, dp, (char *)argv[2], LOOKUP_QUIET, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, GED_ERROR);

    if (intern.idb_type != ID_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: solid type must be ARB\n", argv[0], argv[1]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    if (arb_permute(arb, argv[3], &gedp->ged_wdbp->wdb_tol)) {
	bu_vls_printf(gedp->ged_result_str, "Permute failed\n");
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    rt_db_free_internal(&intern);

    return GED_OK;
}


int
ged_edarb(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    static struct bu_cmdtab arb_cmds[] = {
	{"edgedir",		edarb_edgedir},
	{"extrude",		edarb_extrude},
	{"facedef",		edarb_facedef},
	{"mirface",		edarb_mirface},
	{"permute",		edarb_permute},
	{(const char *)NULL, BU_CMD_NULL}
    };
    static const char *usage = "edgedir|extrude|mirror|permute arb [args]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }


    if (bu_cmd(arb_cmds, argc, argv, 1, gedp, &ret) == BRLCAD_OK)
	return ret;

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return GED_ERROR;
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
