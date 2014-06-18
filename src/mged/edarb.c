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
/** @file mged/edarb.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <string.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "rtgeom.h"
#include "rt/arb_edit.h"
#include "ged.h"
#include "db.h"

#include "./sedit.h"
#include "./mged.h"
#include "./mged_dm.h"
#include "./cmd.h"

extern struct rt_db_internal es_int;
extern struct rt_db_internal es_int_orig;

int newedge;


/*
 * An ARB edge is moved by finding the direction of the line
 * containing the edge and the 2 "bounding" planes.  The new edge is
 * found by intersecting the new line location with the bounding
 * planes.  The two "new" planes thus defined are calculated and the
 * affected points are calculated by intersecting planes.  This keeps
 * ALL faces planar.
 *
 */
int
editarb(vect_t pos_model)
{
    static int pt1, pt2, bp1, bp2, newp, p1, p2, p3;
    const short *edptr;		/* pointer to arb edit array */
    const short *final;		/* location of points to redo */
    static int i;
    const int *iptr;
    struct rt_arb_internal *arb;
    const short earb8[12][18] = earb8_edit_array;
    const short earb7[12][18] = earb7_edit_array;
    const short earb6[10][18] = earb6_edit_array;
    const short earb5[9][18] = earb5_edit_array;
    const short earb4[5][18] = earb4_edit_array;

    arb = (struct rt_arb_internal *)es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    /* set the pointer */
    switch (es_type) {

	case ARB4:
	    edptr = &earb4[es_menu][0];
	    final = &earb4[es_menu][16];
	    break;

	case ARB5:
	    edptr = &earb5[es_menu][0];
	    final = &earb5[es_menu][16];
	    if (es_edflag == PTARB) {
		edptr = &earb5[8][0];
		final = &earb5[8][16];
	    }
	    break;

	case ARB6:
	    edptr = &earb6[es_menu][0];
	    final = &earb6[es_menu][16];
	    if (es_edflag == PTARB) {
		i = 9;
		if (es_menu == 4)
		    i = 8;
		edptr = &earb6[i][0];
		final = &earb6[i][16];
	    }
	    break;

	case ARB7:
	    edptr = &earb7[es_menu][0];
	    final = &earb7[es_menu][16];
	    if (es_edflag == PTARB) {
		edptr = &earb7[11][0];
		final = &earb7[11][16];
	    }
	    break;

	case ARB8:
	    edptr = &earb8[es_menu][0];
	    final = &earb8[es_menu][16];
	    break;

	default:
	    Tcl_AppendResult(INTERP, "edarb: unknown ARB type\n", (char *)NULL);

	    return 1;
    }

    /* do the arb editing */

    if (es_edflag == PTARB) {
	/* moving a point - not an edge */
	VMOVE(arb->pt[es_menu], pos_model);
	edptr += 4;
    } else if (es_edflag == EARB) {
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
	RT_ARB_CK_MAGIC((struct rt_arb_internal *)es_int.idb_ptr);
	if (mv_edge((struct rt_arb_internal *)es_int.idb_ptr, pos_model, bp1, bp2, pt1, pt2, edge_dir, &mged_tol, es_peqn)){
	    Tcl_AppendResult(INTERP, "edge (direction) parallel to face normal\n", (char *)NULL);
	    goto err;
	}
    }

    /* editing is done - insure planar faces */
    /* redo plane eqns that changed */
    newp = *edptr++; 	/* plane to redo */

    if (newp == 9) {
	/* special flag --> redo all the planes */
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;

	if (rt_arb_calc_planes(&error_msg, arb, es_type, es_peqn, &mged_tol)) {
	    Tcl_AppendResult(INTERP, bu_vls_addr(&error_msg), (char *)0);
	    bu_vls_free(&error_msg);
	    goto err;
	}
	bu_vls_free(&error_msg);
    }

    if (newp >= 0 && newp < 6) {
	for (i=0; i<3; i++) {
	    /* redo this plane (newp), use points p1, p2, p3 */
	    p1 = *edptr++;
	    p2 = *edptr++;
	    p3 = *edptr++;
	    if (bn_mk_plane_3pts(es_peqn[newp], arb->pt[p1], arb->pt[p2],
				 arb->pt[p3], &mged_tol))
		goto err;

	    /* next plane */
	    if ((newp = *edptr++) == -1 || newp == 8)
		break;
	}
    }
    if (newp == 8) {
	/* special...redo next planes using pts defined in faces */
	const int local_arb_faces[5][24] = rt_arb_faces;
	for (i=0; i<3; i++) {
	    if ((newp = *edptr++) == -1)
		break;
	    iptr = &local_arb_faces[es_type-4][4*newp];
	    p1 = *iptr++;
	    p2 = *iptr++;
	    p3 = *iptr++;
	    if (bn_mk_plane_3pts(es_peqn[newp], arb->pt[p1], arb->pt[p2],
				 arb->pt[p3], &mged_tol))
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
	if (rt_arb_3face_intersect(arb->pt[p1], (const plane_t *)es_peqn, es_type, p1*3))
	    goto err;
    }

    /* Special case for ARB7: move point 5 .... must recalculate plane
     * 2 = 456
     */
    if (es_type == ARB7 && es_edflag == PTARB) {
	if (bn_mk_plane_3pts(es_peqn[2], arb->pt[4], arb->pt[5], arb->pt[6], &mged_tol))
	    goto err;
    }

    /* carry along any like points */
    switch (es_type) {
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

    return 0;		/* OK */

 err:
    /* Error handling */
    {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "cannot move edge: %d%d\n", pt1+1, pt2+1);
	Tcl_AppendResult(INTERP, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }

    es_edflag = IDLE;

    return 1;		/* BAD */
}

/* Extrude command - project an arb face */
/* Format: extrude face distance */
int
f_extrude(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    static int face;
    static fastf_t dist;
    struct rt_arb_internal *arb;

    CHECK_DBI_NULL;

    if (argc < 3 || 3 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help extrude");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (not_state(ST_S_EDIT, "Extrude"))
	return TCL_ERROR;

    if (es_int.idb_type != ID_ARB8) {
	Tcl_AppendResult(interp, "Extrude: solid type must be ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (es_type != ARB8 && es_type != ARB6 && es_type != ARB4) {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "ARB%d: extrusion of faces not allowed\n", es_type);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);

	return TCL_ERROR;
    }

    face = atoi(argv[1]);

    /* get distance to project face */
    dist = atof(argv[2]);
    /* apply es_mat[15] to get to real model space */
    /* convert from the local unit (as input) to the base unit */
    dist = dist * es_mat[15] * local2base;

    arb = (struct rt_arb_internal *)es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    if (arb_extrude(arb, face, dist, &mged_tol, es_peqn)) {
	Tcl_AppendResult(interp, "Error extruding ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* draw the updated solid */
    replot_editing_solid();
    update_views = 1;

    return TCL_OK;
}


/* Mirface command - mirror an arb face */
/* Format: mirror face axis */
int
f_mirface(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int i, j, k;
    static int face;
    static int pt[4];
    static int prod;
    static vect_t work;
    struct rt_arb_internal *arb;
    struct rt_arb_internal larb;	/* local copy of solid */
    struct bu_vls error_msg = BU_VLS_INIT_ZERO;

    if (argc < 3 || 3 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help mirface");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (not_state(ST_S_EDIT, "Mirface"))
	return TCL_ERROR;

    if (es_int.idb_type != ID_ARB8) {
	Tcl_AppendResult(interp, "Mirface: solid type must be ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    arb = (struct rt_arb_internal *)es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    if (es_type != ARB8 && es_type != ARB6) {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "ARB%d: mirroring of faces not allowed\n", es_type);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);

	return TCL_ERROR;
    }
    face = atoi(argv[1]);
    if (face > 9999 || (face < 1000 && es_type != ARB6)) {
	Tcl_AppendResult(interp, "ERROR: ", argv[1], " bad face\n", (char *)NULL);
	return TCL_ERROR;
    }
    /* check which axis */
    k = -1;
    if (BU_STR_EQUAL(argv[2], "x"))
	k = 0;
    if (BU_STR_EQUAL(argv[2], "y"))
	k = 1;
    if (BU_STR_EQUAL(argv[2], "z"))
	k = 2;
    if (k < 0) {
	Tcl_AppendResult(interp, "axis must be x, y or z\n", (char *)NULL);
	return TCL_ERROR;
    }

    work[0] = work[1] = work[2] = 1.0;
    work[k] = -1.0;

    /* make local copy of arb */
    memcpy((char *)&larb, (char *)arb, sizeof(struct rt_arb_internal));

    if (es_type == ARB6 && face < 1000) {
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
	    Tcl_AppendResult(interp, "bad face: ", argv[1], "\n", (char *)NULL);
	    return TCL_ERROR;
	}
    }

    /* mirror the selected face */
    switch (prod) {

	case 24:   /* mirror face 1234 */
	    if (es_type == ARB6) {
		Tcl_AppendResult(interp, "ARB6: mirroring of face ", argv[1],
				 " not allowed\n", (char *)NULL);
		return TCL_ERROR;
	    }
	    for (i = 0; i < 4; i++) {
		j = i + 4;
		VELMUL(larb.pt[j], larb.pt[i], work);
	    }
	    break;

	case 1680:   /* mirror face 5678 */
	    for (i = 0; i < 4; i++) {
		j = i + 4;
		VELMUL(larb.pt[i], larb.pt[j], work);
	    }
	    break;

	case 60:   /* mirror face 1256 */
	case 10:	/* mirror face 125 of ARB6 */
	    VELMUL(larb.pt[3], larb.pt[0], work);
	    VELMUL(larb.pt[2], larb.pt[1], work);
	    VELMUL(larb.pt[7], larb.pt[4], work);
	    VELMUL(larb.pt[6], larb.pt[5], work);
	    break;

	case 672:   /* mirror face 4378 */
	case 72:	/* mirror face 346 of ARB6 */
	    VELMUL(larb.pt[0], larb.pt[3], work);
	    VELMUL(larb.pt[1], larb.pt[2], work);
	    VELMUL(larb.pt[5], larb.pt[6], work);
	    VELMUL(larb.pt[4], larb.pt[7], work);
	    break;

	case 252:   /* mirror face 2367 */
	    VELMUL(larb.pt[0], larb.pt[1], work);
	    VELMUL(larb.pt[3], larb.pt[2], work);
	    VELMUL(larb.pt[4], larb.pt[5], work);
	    VELMUL(larb.pt[7], larb.pt[6], work);
	    break;

	case 160:   /* mirror face 1548 */
	    VELMUL(larb.pt[1], larb.pt[0], work);
	    VELMUL(larb.pt[5], larb.pt[4], work);
	    VELMUL(larb.pt[2], larb.pt[3], work);
	    VELMUL(larb.pt[6], larb.pt[7], work);
	    break;

	case 120:
	case 180:
	    Tcl_AppendResult(interp, "ARB6: mirroring of face ", argv[1],
			     " not allowed\n", (char *)NULL);
	    return TCL_ERROR;
	default:
	    Tcl_AppendResult(interp, "bad face: ", argv[1], "\n", (char *)NULL);
	    return TCL_ERROR;
    }

    /* redo the plane equations */
    if (rt_arb_calc_planes(&error_msg, &larb, es_type, es_peqn, &mged_tol)) {
	Tcl_AppendResult(interp, bu_vls_addr(&error_msg), (char *)0);
	bu_vls_free(&error_msg);
	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    /* copy to original */
    memcpy((char *)arb, (char *)&larb, sizeof(struct rt_arb_internal));

    /* draw the updated solid */
    replot_editing_solid();
    view_state->vs_flag = 1;

    return TCL_OK;
}


/* Edgedir command: define the direction of an arb edge being moved
 * Format: edgedir deltax deltay deltaz OR edgedir rot fb
*/
int
f_edgedir(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int i;
    vect_t slope;
    fastf_t rot, fb;

    if (argc < 3 || 4 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help edgedir");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (not_state(ST_S_EDIT, "Edgedir"))
	return TCL_ERROR;

    if (es_edflag != EARB) {
	Tcl_AppendResult(interp, "Not moving an ARB edge\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (es_int.idb_type != ID_ARB8) {
	Tcl_AppendResult(interp, "Edgedir: solid type must be an ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* set up slope -
     * if 2 values input assume rot, fb used
     * else assume delta_x, delta_y, delta_z
     */
    if (argc == 3) {
	rot = atof(argv[1]) * DEG2RAD;
	fb = atof(argv[2]) * DEG2RAD;
	slope[0] = cos(fb) * cos(rot);
	slope[1] = cos(fb) * sin(rot);
	slope[2] = sin(fb);
    } else {
	for (i=0; i<3; i++) {
	    /* put edge slope in slope[] array */
	    slope[i] = atof(argv[i+1]);
	}
    }

    if (ZERO(MAGNITUDE(slope))) {
	Tcl_AppendResult(interp, "BAD slope\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* get it done */
    newedge = 1;
    editarb(slope);
    sedit();
    return TCL_OK;
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
int
f_permute(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    /*
     * 1) Why were all vars declared static?
     * 2) Recompute plane equations?
     */
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    struct rt_arb_internal *arb;
    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 2 < argc) {
	bu_vls_printf(&vls, "help permute");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (not_state(ST_S_EDIT, "Permute"))
	return TCL_ERROR;

    if (es_int.idb_type != ID_ARB8) {
	Tcl_AppendResult(interp, "Permute: solid type must be an ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    arb = (struct rt_arb_internal *)es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    if (arb_permute(arb, argv[1], &mged_tol)) {
	Tcl_AppendResult(interp, "Permute failed.\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* draw the updated solid */
    replot_editing_solid();
    view_state->vs_flag = 1;

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
