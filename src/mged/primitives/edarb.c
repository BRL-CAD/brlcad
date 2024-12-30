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
/** @file mged/primitives/edarb.c
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

#include "../sedit.h"
#include "../mged.h"
#include "../mged_dm.h"
#include "../cmd.h"
#include "../menu.h"
#include "./edarb.h"

int newedge;

short int fixv;		/* used in ECMD_ARB_ROTATE_FACE, f_eqn(): fixed vertex */

static void
arb8_edge(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = EARB;
    if (arg == 12) {
	es_edflag = ECMD_ARB_MAIN_MENU;
	sedit(s);
    }

    set_e_axes_pos(s, 1);
}
struct menu_item edge8_menu[] = {
    { "ARB8 EDGES", NULL, 0 },
    { "Move Edge 12", arb8_edge, 0 },
    { "Move Edge 23", arb8_edge, 1 },
    { "Move Edge 34", arb8_edge, 2 },
    { "Move Edge 14", arb8_edge, 3 },
    { "Move Edge 15", arb8_edge, 4 },
    { "Move Edge 26", arb8_edge, 5 },
    { "Move Edge 56", arb8_edge, 6 },
    { "Move Edge 67", arb8_edge, 7 },
    { "Move Edge 78", arb8_edge, 8 },
    { "Move Edge 58", arb8_edge, 9 },
    { "Move Edge 37", arb8_edge, 10 },
    { "Move Edge 48", arb8_edge, 11 },
    { "RETURN",       arb8_edge, 12 },
    { "", NULL, 0 }
};


static void
arb7_edge(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = EARB;
    if (arg == 11) {
	/* move point 5 */
	es_edflag = PTARB;
	es_menu = 4;	/* location of point */
    }
    if (arg == 12) {
	es_edflag = ECMD_ARB_MAIN_MENU;
	sedit(s);
    }

    set_e_axes_pos(s, 1);
}
struct menu_item edge7_menu[] = {
    { "ARB7 EDGES", NULL, 0 },
    { "Move Edge 12", arb7_edge, 0 },
    { "Move Edge 23", arb7_edge, 1 },
    { "Move Edge 34", arb7_edge, 2 },
    { "Move Edge 14", arb7_edge, 3 },
    { "Move Edge 15", arb7_edge, 4 },
    { "Move Edge 26", arb7_edge, 5 },
    { "Move Edge 56", arb7_edge, 6 },
    { "Move Edge 67", arb7_edge, 7 },
    { "Move Edge 37", arb7_edge, 8 },
    { "Move Edge 57", arb7_edge, 9 },
    { "Move Edge 45", arb7_edge, 10 },
    { "Move Point 5", arb7_edge, 11 },
    { "RETURN",       arb7_edge, 12 },
    { "", NULL, 0 }
};

static void
arb6_edge(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = EARB;
    if (arg == 8) {
	/* move point 5, location = 4 */
	es_edflag = PTARB;
	es_menu = 4;
    }
    if (arg == 9) {
	/* move point 6, location = 6 */
	es_edflag = PTARB;
	es_menu = 6;
    }
    if (arg == 10) {
	es_edflag = ECMD_ARB_MAIN_MENU;
	sedit(s);
    }

    set_e_axes_pos(s, 1);
}
struct menu_item edge6_menu[] = {
    { "ARB6 EDGES", NULL, 0 },
    { "Move Edge 12", arb6_edge, 0 },
    { "Move Edge 23", arb6_edge, 1 },
    { "Move Edge 34", arb6_edge, 2 },
    { "Move Edge 14", arb6_edge, 3 },
    { "Move Edge 15", arb6_edge, 4 },
    { "Move Edge 25", arb6_edge, 5 },
    { "Move Edge 36", arb6_edge, 6 },
    { "Move Edge 46", arb6_edge, 7 },
    { "Move Point 5", arb6_edge, 8 },
    { "Move Point 6", arb6_edge, 9 },
    { "RETURN",       arb6_edge, 10 },
    { "", NULL, 0 }
};

static void
arb5_edge(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = EARB;
    if (arg == 8) {
	/* move point 5 at location 4 */
	es_edflag = PTARB;
	es_menu = 4;
    }
    if (arg == 9) {
	es_edflag = ECMD_ARB_MAIN_MENU;
	sedit(s);
    }

    set_e_axes_pos(s, 1);
}
struct menu_item edge5_menu[] = {
    { "ARB5 EDGES", NULL, 0 },
    { "Move Edge 12", arb5_edge, 0 },
    { "Move Edge 23", arb5_edge, 1 },
    { "Move Edge 34", arb5_edge, 2 },
    { "Move Edge 14", arb5_edge, 3 },
    { "Move Edge 15", arb5_edge, 4 },
    { "Move Edge 25", arb5_edge, 5 },
    { "Move Edge 35", arb5_edge, 6 },
    { "Move Edge 45", arb5_edge, 7 },
    { "Move Point 5", arb5_edge, 8 },
    { "RETURN",       arb5_edge, 9 },
    { "", NULL, 0 }
};

static void
arb4_point(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PTARB;
    if (arg == 5) {
	es_edflag = ECMD_ARB_MAIN_MENU;
	sedit(s);
    }

    set_e_axes_pos(s, 1);
}
struct menu_item point4_menu[] = {
    { "ARB4 POINTS", NULL, 0 },
    { "Move Point 1", arb4_point, 0 },
    { "Move Point 2", arb4_point, 1 },
    { "Move Point 3", arb4_point, 2 },
    { "Move Point 4", arb4_point, 4 },
    { "RETURN",       arb4_point, 5 },
    { "", NULL, 0 }
};

static void
arb8_mv_face(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg - 1;
    es_edflag = ECMD_ARB_MOVE_FACE;
    if (arg == 7) {
	es_edflag = ECMD_ARB_MAIN_MENU;
	sedit(s);
    }

    set_e_axes_pos(s, 1);
}
struct menu_item mv8_menu[] = {
    { "ARB8 FACES", NULL, 0 },
    { "Move Face 1234", arb8_mv_face, 1 },
    { "Move Face 5678", arb8_mv_face, 2 },
    { "Move Face 1584", arb8_mv_face, 3 },
    { "Move Face 2376", arb8_mv_face, 4 },
    { "Move Face 1265", arb8_mv_face, 5 },
    { "Move Face 4378", arb8_mv_face, 6 },
    { "RETURN",         arb8_mv_face, 7 },
    { "", NULL, 0 }
};

static void
arb7_mv_face(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg - 1;
    es_edflag = ECMD_ARB_MOVE_FACE;
    if (arg == 7) {
	es_edflag = ECMD_ARB_MAIN_MENU;
	sedit(s);
    }

    set_e_axes_pos(s, 1);
}
struct menu_item mv7_menu[] = {
    { "ARB7 FACES", NULL, 0 },
    { "Move Face 1234", arb7_mv_face, 1 },
    { "Move Face 2376", arb7_mv_face, 4 },
    { "RETURN",         arb7_mv_face, 7 },
    { "", NULL, 0 }
};

static void
arb6_mv_face(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg - 1;
    es_edflag = ECMD_ARB_MOVE_FACE;
    if (arg == 6) {
	es_edflag = ECMD_ARB_MAIN_MENU;
	sedit(s);
    }

    set_e_axes_pos(s, 1);
}
struct menu_item mv6_menu[] = {
    { "ARB6 FACES", NULL, 0 },
    { "Move Face 1234", arb6_mv_face, 1 },
    { "Move Face 2365", arb6_mv_face, 2 },
    { "Move Face 1564", arb6_mv_face, 3 },
    { "Move Face 125", arb6_mv_face, 4 },
    { "Move Face 346", arb6_mv_face, 5 },
    { "RETURN",         arb6_mv_face, 6 },
    { "", NULL, 0 }
};

static void
arb5_mv_face(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg - 1;
    es_edflag = ECMD_ARB_MOVE_FACE;
    if (arg == 6) {
	es_edflag = ECMD_ARB_MAIN_MENU;
	sedit(s);
    }

    set_e_axes_pos(s, 1);
}
struct menu_item mv5_menu[] = {
    { "ARB5 FACES", NULL, 0 },
    { "Move Face 1234", arb5_mv_face, 1 },
    { "Move Face 125", arb5_mv_face, 2 },
    { "Move Face 235", arb5_mv_face, 3 },
    { "Move Face 345", arb5_mv_face, 4 },
    { "Move Face 145", arb5_mv_face, 5 },
    { "RETURN",         arb5_mv_face, 6 },
    { "", NULL, 0 }
};

static void
arb4_mv_face(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg - 1;
    es_edflag = ECMD_ARB_MOVE_FACE;
    if (arg == 5) {
	es_edflag = ECMD_ARB_MAIN_MENU;
	sedit(s);
    }

    set_e_axes_pos(s, 1);
}
struct menu_item mv4_menu[] = {
    { "ARB4 FACES", NULL, 0 },
    { "Move Face 123", arb4_mv_face, 1 },
    { "Move Face 124", arb4_mv_face, 2 },
    { "Move Face 234", arb4_mv_face, 3 },
    { "Move Face 134", arb4_mv_face, 4 },
    { "RETURN",         arb4_mv_face, 5 },
    { "", NULL, 0 }
};

static void
arb8_rot_face(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg - 1;
    es_edflag = ECMD_ARB_SETUP_ROTFACE;
    if (arg == 7)
	es_edflag = ECMD_ARB_MAIN_MENU;

    sedit(s);
}
struct menu_item rot8_menu[] = {
    { "ARB8 FACES", NULL, 0 },
    { "Rotate Face 1234", arb8_rot_face, 1 },
    { "Rotate Face 5678", arb8_rot_face, 2 },
    { "Rotate Face 1584", arb8_rot_face, 3 },
    { "Rotate Face 2376", arb8_rot_face, 4 },
    { "Rotate Face 1265", arb8_rot_face, 5 },
    { "Rotate Face 4378", arb8_rot_face, 6 },
    { "RETURN",         arb8_rot_face, 7 },
    { "", NULL, 0 }
};

static void
arb7_rot_face(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg - 1;
    es_edflag = ECMD_ARB_SETUP_ROTFACE;
    if (arg == 7)
	es_edflag = ECMD_ARB_MAIN_MENU;

    sedit(s);
}
struct menu_item rot7_menu[] = {
    { "ARB7 FACES", NULL, 0 },
    { "Rotate Face 1234", arb7_rot_face, 1 },
    { "Rotate Face 567", arb7_rot_face, 2 },
    { "Rotate Face 145", arb7_rot_face, 3 },
    { "Rotate Face 2376", arb7_rot_face, 4 },
    { "Rotate Face 1265", arb7_rot_face, 5 },
    { "Rotate Face 4375", arb7_rot_face, 6 },
    { "RETURN",         arb7_rot_face, 7 },
    { "", NULL, 0 }
};

static void
arb6_rot_face(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg - 1;
    es_edflag = ECMD_ARB_SETUP_ROTFACE;
    if (arg == 6)
	es_edflag = ECMD_ARB_MAIN_MENU;

    sedit(s);
}
struct menu_item rot6_menu[] = {
    { "ARB6 FACES", NULL, 0 },
    { "Rotate Face 1234", arb6_rot_face, 1 },
    { "Rotate Face 2365", arb6_rot_face, 2 },
    { "Rotate Face 1564", arb6_rot_face, 3 },
    { "Rotate Face 125", arb6_rot_face, 4 },
    { "Rotate Face 346", arb6_rot_face, 5 },
    { "RETURN",         arb6_rot_face, 6 },
    { "", NULL, 0 }
};

static void
arb5_rot_face(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg - 1;
    es_edflag = ECMD_ARB_SETUP_ROTFACE;
    if (arg == 6)
	es_edflag = ECMD_ARB_MAIN_MENU;

    sedit(s);
}

struct menu_item rot5_menu[] = {
    { "ARB5 FACES", NULL, 0 },
    { "Rotate Face 1234", arb5_rot_face, 1 },
    { "Rotate Face 125", arb5_rot_face, 2 },
    { "Rotate Face 235", arb5_rot_face, 3 },
    { "Rotate Face 345", arb5_rot_face, 4 },
    { "Rotate Face 145", arb5_rot_face, 5 },
    { "RETURN",         arb5_rot_face, 6 },
    { "", NULL, 0 }
};

static void
arb4_rot_face(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg - 1;
    es_edflag = ECMD_ARB_SETUP_ROTFACE;
    if (arg == 5)
	es_edflag = ECMD_ARB_MAIN_MENU;

    sedit(s);
}
struct menu_item rot4_menu[] = {
    { "ARB4 FACES", NULL, 0 },
    { "Rotate Face 123", arb4_rot_face, 1 },
    { "Rotate Face 124", arb4_rot_face, 2 },
    { "Rotate Face 234", arb4_rot_face, 3 },
    { "Rotate Face 134", arb4_rot_face, 4 },
    { "RETURN",         arb4_rot_face, 5 },
    { "", NULL, 0 }
};

static void
arb_control(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = ECMD_ARB_SPECIFIC_MENU;
    sedit(s);
}
struct menu_item cntrl_menu[] = {
    { "ARB MENU", NULL, 0 },
    { "Move Edges", arb_control, MENU_ARB_MV_EDGE },
    { "Move Faces", arb_control, MENU_ARB_MV_FACE },
    { "Rotate Faces", arb_control, MENU_ARB_ROT_FACE },
    { "", NULL, 0 }
};

struct menu_item *which_menu[] = {
    point4_menu,
    edge5_menu,
    edge6_menu,
    edge7_menu,
    edge8_menu,
    mv4_menu,
    mv5_menu,
    mv6_menu,
    mv7_menu,
    mv8_menu,
    rot4_menu,
    rot5_menu,
    rot6_menu,
    rot7_menu,
    rot8_menu
};

const char *
mged_arb_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *tol)
{
    if (*keystr == 'V') {
	const char *strp = OBJ[ip->idb_type].ft_keypoint(pt, keystr, mat, ip, tol);
	return strp;
    }
    static const char *vstr = "V1";
    const char *strp = OBJ[ip->idb_type].ft_keypoint(pt, vstr, mat, ip, tol);
    return strp;
}

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
editarb(struct mged_state *s, vect_t pos_model)
{
    int ret = 0;
    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;

    ret = arb_edit(arb, es_peqn, es_menu, newedge, pos_model, &s->tol.tol);

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

    CHECK_DBI_NULL;

    if (argc < 3 || 3 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help extrude");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (not_state(s, ST_S_EDIT, "Extrude"))
	return TCL_ERROR;

    if (s->edit_state.es_int.idb_type != ID_ARB8) {
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
    dist = dist * es_mat[15] * s->dbip->dbi_local2base;

    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    if (arb_extrude(arb, face, dist, &s->tol.tol, es_peqn)) {
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

    if (argc < 3 || 3 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help mirface");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (not_state(s, ST_S_EDIT, "Mirface"))
	return TCL_ERROR;

    if (s->edit_state.es_int.idb_type != ID_ARB8) {
	Tcl_AppendResult(interp, "Mirface: solid type must be ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    face = atoi(argv[1]);

    if (arb_mirror_face_axis(arb, es_peqn, face, argv[2], &s->tol.tol)) {
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

    if (not_state(s, ST_S_EDIT, "Edgedir"))
	return TCL_ERROR;

    if (es_edflag != EARB) {
	Tcl_AppendResult(interp, "Not moving an ARB edge\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (s->edit_state.es_int.idb_type != ID_ARB8) {
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
    editarb(s, slope);
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
    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 2 < argc) {
	bu_vls_printf(&vls, "help permute");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (not_state(s, ST_S_EDIT, "Permute"))
	return TCL_ERROR;

    if (s->edit_state.es_int.idb_type != ID_ARB8) {
	Tcl_AppendResult(interp, "Permute: solid type must be an ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    if (arb_permute(arb, argv[1], &s->tol.tol)) {
	Tcl_AppendResult(interp, "Permute failed.\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* draw the updated solid */
    replot_editing_solid(s);
    view_state->vs_flag = 1;

    return TCL_OK;
}

void
ecmd_arb_main_menu(struct mged_state *s)
{
    /* put up control (main) menu for GENARB8s */
    menu_state->ms_flag = 0;
    es_edflag = IDLE;
    mmenu_set(s, MENU_L1, cntrl_menu);
}

int
ecmd_arb_specific_menu(struct mged_state *s)
{
    /* put up specific arb edit menus */
    menu_state->ms_flag = 0;
    es_edflag = IDLE;
    switch (es_menu) {
	case MENU_ARB_MV_EDGE:
	    mmenu_set(s, MENU_L1, which_menu[es_type-4]);
	    return BRLCAD_OK;
	case MENU_ARB_MV_FACE:
	    mmenu_set(s, MENU_L1, which_menu[es_type+1]);
	    return BRLCAD_OK;
	case MENU_ARB_ROT_FACE:
	    mmenu_set(s, MENU_L1, which_menu[es_type+6]);
	    return BRLCAD_OK;
	default:
	    Tcl_AppendResult(s->interp, "Bad menu item.\n", (char *)NULL);
	    mged_print_result(s, TCL_ERROR);
	    return BRLCAD_ERROR;
    }
}

void
ecmd_arb_move_face(struct mged_state *s)
{
    /* move face through definite point */
    if (inpara) {
	vect_t work;
	struct rt_arb_internal *arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	if (mged_variables->mv_context) {
	    /* apply es_invmat to convert to real model space */
	    MAT4X3PNT(work, es_invmat, es_para);
	} else {
	    VMOVE(work, es_para);
	}
	/* change D of planar equation */
	es_peqn[es_menu][W]=VDOT(&es_peqn[es_menu][0], work);
	/* find new vertices, put in record in vector notation */

	(void)rt_arb_calc_points(arb, es_type, (const plane_t *)es_peqn, &s->tol.tol);
    }
}

static int
get_rotation_vertex(struct mged_state *s)
{
    int i, j;
    int type, loc, valid;
    int vertex = -1;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls cmd = BU_VLS_INIT_ZERO;

    type = es_type - 4;

    loc = es_menu*4;
    valid = 0;

    bu_vls_printf(&str, "Enter fixed vertex number(");
    for (i=0; i<4; i++) {
	if (rt_arb_vertices[type][loc+i])
	    bu_vls_printf(&str, "%d ", rt_arb_vertices[type][loc+i]);
    }
    bu_vls_printf(&str, ") [%d]: ", rt_arb_vertices[type][loc]);

    const struct bu_vls *dnvp = dm_get_dname(s->mged_curr_dm->dm_dmp);

    bu_vls_printf(&cmd, "cad_input_dialog .get_vertex %s {Need vertex for solid rotate}\
 {%s} vertex_num %d 0 {{ summary \"Enter a vertex number to rotate about.\"}} OK",
		  (dnvp) ? bu_vls_addr(dnvp) : "id", bu_vls_addr(&str), rt_arb_vertices[type][loc]);

    while (!valid) {
	if (Tcl_Eval(s->interp, bu_vls_addr(&cmd)) != TCL_OK) {
	    Tcl_AppendResult(s->interp, "get_rotation_vertex: Error reading vertex\n", (char *)NULL);
	    /* Using default */
	    return rt_arb_vertices[type][loc];
	}

	vertex = atoi(Tcl_GetVar(s->interp, "vertex_num", TCL_GLOBAL_ONLY));
	for (j=0; j<4; j++) {
	    if (vertex==rt_arb_vertices[type][loc+j])
		valid = 1;
	}
    }

    bu_vls_free(&str);
    return vertex;
}

void
ecmd_arb_setup_rotface(struct mged_state *s)
{
    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    /* check if point 5 is in the face */
    static int pnt5 = 0;
    for (int i=0; i<4; i++) {
	if (rt_arb_vertices[es_type-4][es_menu*4+i]==5)
	    pnt5=1;
    }

    /* special case for arb7 */
    if (es_type == ARB7  && pnt5) {
	Tcl_AppendResult(s->interp, "\nFixed vertex is point 5.\n", (char *)NULL);
	fixv = 5;
    } else {
	fixv = get_rotation_vertex(s);
    }

    pr_prompt(s);
    fixv--;
    es_edflag = ECMD_ARB_ROTATE_FACE;
    view_state->vs_flag = 1;	/* draw arrow, etc. */
    set_e_axes_pos(s, 1);
}

void
ecmd_arb_rotate_face(struct mged_state *s)
{
    /* rotate a GENARB8 defining plane through a fixed vertex */
    fastf_t *eqp;

    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    if (inpara) {
	vect_t work;
	static mat_t invsolr;
	static vect_t tempvec;
	static float rota, fb_a;

	/*
	 * Keyboard parameters in degrees.
	 * First, cancel any existing rotations,
	 * then perform new rotation
	 */
	bn_mat_inv(invsolr, acc_rot_sol);
	eqp = &es_peqn[es_menu][0];	/* es_menu==plane of interest */
	VMOVE(work, eqp);
	MAT4X3VEC(eqp, invsolr, work);

	if (inpara == 3) {
	    /* 3 params:  absolute X, Y, Z rotations */
	    /* Build completely new rotation change */
	    MAT_IDN(modelchanges);
	    bn_mat_angles(modelchanges,
		    es_para[0],
		    es_para[1],
		    es_para[2]);
	    MAT_COPY(acc_rot_sol, modelchanges);

	    /* Borrow incr_change matrix here */
	    bn_mat_mul(incr_change, modelchanges, invsolr);
	    if (mged_variables->mv_context) {
		/* calculate rotations about keypoint */
		mat_t edit;
		bn_mat_xform_about_pnt(edit, incr_change, es_keypoint);

		/* We want our final matrix (mat) to xform the original solid
		 * to the position of this instance of the solid, perform the
		 * current edit operations, then xform back.
		 * mat = es_invmat * edit * es_mat
		 */
		mat_t mat, mat1;
		bn_mat_mul(mat1, edit, es_mat);
		bn_mat_mul(mat, es_invmat, mat1);
		MAT_IDN(incr_change);
		/* work contains original es_peqn[es_menu][0] */
		MAT4X3VEC(eqp, mat, work);
	    } else {
		VMOVE(work, eqp);
		MAT4X3VEC(eqp, modelchanges, work);
	    }
	} else if (inpara == 2) {
	    /* 2 parameters:  rot, fb were given */
	    rota= es_para[0] * DEG2RAD;
	    fb_a  = es_para[1] * DEG2RAD;

	    /* calculate normal vector (length = 1) from rot, struct fb */
	    es_peqn[es_menu][0] = cos(fb_a) * cos(rota);
	    es_peqn[es_menu][1] = cos(fb_a) * sin(rota);
	    es_peqn[es_menu][2] = sin(fb_a);
	} else {
	    Tcl_AppendResult(s->interp, "Must be < rot fb | xdeg ydeg zdeg >\n",
		    (char *)NULL);
	    mged_print_result(s, TCL_ERROR);
	    return;
	}

	/* point notation of fixed vertex */
	VMOVE(tempvec, arb->pt[fixv]);

	/* set D of planar equation to anchor at fixed vertex */
	/* es_menu == plane of interest */
	es_peqn[es_menu][W]=VDOT(eqp, tempvec);

	/* Clear out solid rotation */
	MAT_IDN(modelchanges);

    } else {
	/* Apply incremental changes */
	static vect_t tempvec;
	vect_t work;

	eqp = &es_peqn[es_menu][0];
	VMOVE(work, eqp);
	MAT4X3VEC(eqp, incr_change, work);

	/* point notation of fixed vertex */
	VMOVE(tempvec, arb->pt[fixv]);

	/* set D of planar equation to anchor at fixed vertex */
	/* es_menu == plane of interest */
	es_peqn[es_menu][W]=VDOT(eqp, tempvec);
    }

    (void)rt_arb_calc_points(arb, es_type, (const plane_t *)es_peqn, &s->tol.tol);
    MAT_IDN(incr_change);

    /* no need to calc_planes again */
    replot_editing_solid(s);

    inpara = 0;

}

void
edit_arb_element(struct mged_state *s)
{

    if (inpara) {
	vect_t work;
	if (mged_variables->mv_context) {
	    /* apply es_invmat to convert to real model space */
	    MAT4X3PNT(work, es_invmat, es_para);
	} else {
	    VMOVE(work, es_para);
	}
	editarb(s, work);
    }
}

void
arb_mv_pnt_to(struct mged_state *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;
    vect_t pos_model = VINIT_ZERO;	/* Rotated screen space pos */
    /* move an arb point to indicated point */
    /* point is located at es_values[es_menu*3] */
    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
    MAT4X3PNT(pos_model, es_invmat, temp);
    editarb(s, pos_model);
}

void
edarb_mousevec(struct mged_state *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;
    vect_t pos_model = VINIT_ZERO;	/* Rotated screen space pos */
    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
    MAT4X3PNT(pos_model, es_invmat, temp);
    editarb(s, pos_model);
}

void
edarb_move_face_mousevec(struct mged_state *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;
    vect_t pos_model = VINIT_ZERO;	/* Rotated screen space pos */
    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
    MAT4X3PNT(pos_model, es_invmat, temp);
    /* change D of planar equation */
    es_peqn[es_menu][W]=VDOT(&es_peqn[es_menu][0], pos_model);
    /* calculate new vertices, put in record as vectors */
    {
	struct rt_arb_internal *arb=
	    (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;

	RT_ARB_CK_MAGIC(arb);

	(void)rt_arb_calc_points(arb, es_type, (const plane_t *)es_peqn, &s->tol.tol);
    }
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
