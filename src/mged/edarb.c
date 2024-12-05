/*                         E D A R B . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
#include <signal.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "rt/geom.h"
#include "rt/arb_edit.h"
#include "ged.h"
#include "rt/db4.h"

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
    int ret = 0;
    struct rt_arb_internal *arb;
    arb = (struct rt_arb_internal *)es_int.idb_ptr;

    ret = arb_edit(arb, es_peqn, es_menu, newedge, pos_model, &mged_tol);

    // arb_edit doesn't zero out our global any more as a library call, so
    // reset once operation is complete.
    newedge = 0;

    if (ret) {
	es_edflag = IDLE;
    }

    return ret;
}

/* Extrude command - project an arb face */
/* Format: extrude face distance */
int
f_extrude(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

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
    replot_editing_solid(s);
    update_views = 1;
    dm_set_dirty(DMP, 1);

    return TCL_OK;
}


/* Mirface command - mirror an arb face */
/* Format: mirror face axis */
int
f_mirface(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int face;
    struct rt_arb_internal *arb;

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

    face = atoi(argv[1]);

    if (arb_mirror_face_axis(arb, es_peqn, face, argv[2], &mged_tol)) {
	Tcl_AppendResult(interp, "Mirface: mirror operation failed\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* draw the updated solid */
    replot_editing_solid(s);
    view_state->vs_flag = 1;

    return TCL_OK;
}


/* Edgedir command: define the direction of an arb edge being moved
 * Format: edgedir deltax deltay deltaz OR edgedir rot fb
*/
int
f_edgedir(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int i;
    vect_t slope;
    fastf_t rot, fb_a;

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
	fb_a = atof(argv[2]) * DEG2RAD;
	slope[0] = cos(fb_a) * cos(rot);
	slope[1] = cos(fb_a) * sin(rot);
	slope[2] = sin(fb_a);
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
    sedit(s);
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
f_permute(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

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
    replot_editing_solid(s);
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
