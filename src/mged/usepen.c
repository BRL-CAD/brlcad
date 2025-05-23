/*                        U S E P E N . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
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
/** @file mged/usepen.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bn.h"

#include "./mged.h"
#include "./mged_dm.h"

#include "./sedit.h"
#include "./menu.h"


struct display_list *illum_gdlp = GED_DISPLAY_LIST_NULL;
struct bv_scene_obj *illump = NULL;	/* == 0 if none, else points to ill. solid */
int ipathpos = 0;	/* path index of illuminated element */


/*
 * All solids except for the illuminated one have s_iflag set to DOWN.
 * The illuminated one has s_iflag set to UP, and also has the global
 * variable "illump" pointing at it.
 */
static void
illuminate(struct mged_state *s, int y) {
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int count;
    struct bv_scene_obj *sp;

    /*
     * Divide the mouse into 's->mged_curr_dm->dm_ndrawn' VERTICAL
     * zones, and use the zone number as a sequential position among
     * solids which are drawn.
     */
    count = ((fastf_t)y + BV_MAX) * s->mged_curr_dm->dm_ndrawn / BV_RANGE;

    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));
    while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    /* Only consider solids which are presently in view */
	    if (sp->s_flag == UP) {
		if (count-- == 0) {
		    sp->s_iflag = UP;
		    illump = sp;
		    illum_gdlp = gdlp;
		} else {
		    /* All other solids have s_iflag set DOWN */
		    sp->s_iflag = DOWN;
		}
	    }
	}

	gdlp = next_gdlp;
    }

    s->update_views = 1;
    dm_set_dirty(DMP, 1);
}


/*
 * advance illump or ipathpos
 */
int
f_aip(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct display_list *gdlp;
    struct bv_scene_obj *sp;
    struct ged_bv_data *bdata = NULL;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel aip");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (!(s->mged_curr_dm->dm_ndrawn)) {
	return TCL_OK;
    } else if (s->global_editing_state != ST_S_PICK && s->global_editing_state != ST_O_PICK  && s->global_editing_state != ST_O_PATH) {
	return TCL_OK;
    }

    if (illump != NULL && illump->s_u_data != NULL)
	bdata = (struct ged_bv_data *)illump->s_u_data;

    if (s->global_editing_state == ST_O_PATH && bdata) {
	if (argc == 1 || *argv[1] == 'f') {
	    ++ipathpos;
	    if ((size_t)ipathpos >= bdata->s_fullpath.fp_len)
		ipathpos = 0;
	} else if (*argv[1] == 'b') {
	    --ipathpos;
	    if (ipathpos < 0)
		ipathpos = bdata->s_fullpath.fp_len-1;
	} else {
	    Tcl_AppendResult(interp, "aip: bad parameter - ", argv[1], "\n", (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	if (illump == NULL)
	    return TCL_ERROR;
	gdlp = illum_gdlp;
	sp = illump;
	sp->s_iflag = DOWN;
	if (argc == 1 || *argv[1] == 'f') {
	    if (BU_LIST_NEXT_IS_HEAD(sp, &gdlp->dl_head_scene_obj)) {
		/* Advance the gdlp (i.e. display list) */
		if (BU_LIST_NEXT_IS_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp)))
		    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));
		else
		    gdlp = BU_LIST_PNEXT(display_list, gdlp);


		sp = BU_LIST_NEXT(bv_scene_obj, &gdlp->dl_head_scene_obj);
	    } else
		sp = BU_LIST_PNEXT(bv_scene_obj, sp);
	} else if (*argv[1] == 'b') {
	    if (BU_LIST_PREV_IS_HEAD(sp, &gdlp->dl_head_scene_obj)) {
		/* Advance the gdlp (i.e. display list) */
		if (BU_LIST_PREV_IS_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp)))
		    gdlp = BU_LIST_PREV(display_list, (struct bu_list *)ged_dl(s->gedp));
		else
		    gdlp = BU_LIST_PLAST(display_list, gdlp);

		sp = BU_LIST_PREV(bv_scene_obj, &gdlp->dl_head_scene_obj);
	    } else
		sp = BU_LIST_PLAST(bv_scene_obj, sp);
	} else {
	    Tcl_AppendResult(interp, "aip: bad parameter - ", argv[1], "\n", (char *)NULL);
	    return TCL_ERROR;
	}

	sp->s_iflag = UP;
	illump = sp;
	illum_gdlp = gdlp;
    }

    s->update_views = 1;
    dm_set_dirty(DMP, 1);
    return TCL_OK;
}


/*
 * Given a model-space transformation matrix "change", return a matrix
 * which applies the change with-respect-to the view center.
 */
void
wrt_view(struct mged_state *s, mat_t out, const mat_t change, const mat_t in)
{
    static mat_t t1, t2;

    bn_mat_mul(t1, view_state->vs_gvp->gv_center, in);
    bn_mat_mul(t2, change, t1);

    /* Build "fromViewcenter" matrix */
    MAT_IDN(t1);
    MAT_DELTAS(t1, -view_state->vs_gvp->gv_center[MDX], -view_state->vs_gvp->gv_center[MDY], -view_state->vs_gvp->gv_center[MDZ]);
    bn_mat_mul(out, t1, t2);
}


/*
 * Given a model-space transformation matrix "change", return a matrix
 * which applies the change with-respect-to "point".
 */
void
wrt_point(mat_t out, const mat_t change, const mat_t in, const point_t point)
{
    mat_t t;

    bn_mat_xform_about_pnt(t, change, point);

    if (out == in)
	bn_mat_mul2(t, out);
    else
	bn_mat_mul(out, t, in);
}


/*
 * When in O_PATH state, select the arc which contains the matrix
 * which is going to be "object edited".  The choice is recorded in
 * variable "ipathpos".
 *
 * There are two syntaxes:
 * matpick a/b Pick arc between a and b.
 * matpick # Similar to internal interface.
 * 0 = top level object is a solid.
 * n = edit arc from path [n-1] to [n]
 */
int
f_matpick(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    char *cp;
    size_t j;
    int illum_only = 0;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    struct ged_bv_data *bdata = NULL;

    CHECK_DBI_NULL;

    if (argc < 2 || 3 < argc) {
	bu_vls_printf(&vls, "help matpick");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (BU_STR_EQUAL("-n", argv[1])) {
	illum_only = 1;
	--argc;
	++argv;
    }

    if (argc != 2) {
	bu_vls_printf(&vls, "help matpick");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (not_state(s, ST_O_PATH, "Object Edit matrix pick"))
	return TCL_ERROR;

    if (!illump->s_u_data)
	return TCL_ERROR;

    bdata = (struct ged_bv_data *)illump->s_u_data;

    if ((cp = strchr(argv[1], '/')) != NULL) {
	struct directory *d0, *d1;
	if ((d1 = db_lookup(s->dbip, cp+1, LOOKUP_NOISY)) == RT_DIR_NULL)
	    return TCL_ERROR;
	*cp = '\0';		/* modifies argv[1] */
	if ((d0 = db_lookup(s->dbip, argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	    return TCL_ERROR;
	/* Find arc on illump path which runs from d0 to d1 */
	for (j=1; j < bdata->s_fullpath.fp_len; j++) {
	    if (DB_FULL_PATH_GET(&bdata->s_fullpath, j-1) != d0) continue;
	    if (DB_FULL_PATH_GET(&bdata->s_fullpath, j-0) != d1) continue;
	    ipathpos = j;
	    goto got;
	}
	Tcl_AppendResult(interp, "matpick: unable to find arc ", d0->d_namep,
			 "/", d1->d_namep, " in current selection.  Re-specify.\n",
			 (char *)NULL);
	return TCL_ERROR;
    } else {
	ipathpos = atoi(argv[1]);
	if (ipathpos < 0) ipathpos = 0;
	else if ((size_t)ipathpos >= bdata->s_fullpath.fp_len)
	    ipathpos = bdata->s_fullpath.fp_len-1;
    }
 got:
    /* Include all solids with same tree top */
    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));
    while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdatas = (struct ged_bv_data *)sp->s_u_data;
	    for (j = 0; j <= (size_t)ipathpos; j++) {
		if (DB_FULL_PATH_GET(&bdatas->s_fullpath, j) !=
		    DB_FULL_PATH_GET(&bdata->s_fullpath, j))
		    break;
	    }
	    /* Only accept if top of tree is identical */
	    if (j == (size_t)ipathpos+1)
		sp->s_iflag = UP;
	    else
		sp->s_iflag = DOWN;
	}

	gdlp = next_gdlp;
    }

    if (!illum_only) {
	(void)chg_state(s, ST_O_PATH, ST_O_EDIT, "mouse press");
	chg_l2menu(s, ST_O_EDIT);

	/* begin object editing - initialize */
	init_oedit(s);
    }

    s->update_views = 1;
    dm_set_dirty(DMP, 1);
    return TCL_OK;
}


/*
 * X and Y are expected to be in -2048 <= x, y <= +2047 range.  The
 * "up" flag is 1 on the not-pressed to pressed transition, and 0 on
 * the pressed to not-pressed transition.
 *
 * Note -
 * The mouse is the focus of much of the editing activity in GED.  The
 * editor operates in one of seven basic editing states, recorded in
 * the variable called "state".  When no editing is taking place, the
 * editor is in state ST_VIEW.  There are two paths out of ST_VIEW:
 *
 * BE_S_ILLUMINATE, when pressed, takes the editor into ST_S_PICK,
 * where the mouse is used to pick a solid to edit, using our unusual
 * "illuminate" technique.  Moving the mouse varies the solid being
 * illuminated.  When the mouse is pressed, the editor moves into
 * state ST_S_EDIT, and solid editing may begin.  Solid editing is
 * terminated via BE_ACCEPT and BE_REJECT.
 *
 * BE_O_ILLUMINATE, when pressed, takes the editor into ST_O_PICK,
 * again performing the illuminate procedure.  When the mouse is
 * pressed, the editor moves into state ST_O_PATH.  Now, moving the
 * mouse allows the user to choose the portion of the path relation to
 * be edited.  When the mouse is pressed, the editor moves into state
 * ST_O_EDIT, and object editing may begin.  Object editing is
 * terminated via BE_ACCEPT and BE_REJECT.
 *
 * The only way to exit the intermediate states (non-VIEW, non-EDIT)
 * is by completing the sequence, or pressing BE_REJECT.
 */
int
f_mouse(
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct ged_bv_data *bdata = NULL;
    vect_t mousevec;		/* float pt -1..+1 mouse pos vect */
    int isave;
    int up;
    int xpos;
    int ypos;

    if (argc < 4 || 4 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help M");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (illump && illump->s_u_data)
	bdata = (struct ged_bv_data *)illump->s_u_data;

    up = atoi(argv[1]);
    xpos = atoi(argv[2]);
    ypos = atoi(argv[3]);

    /* Build floating point mouse vector, -1 to +1 */
    mousevec[X] =  xpos * INV_BV;
    mousevec[Y] =  ypos * INV_BV;
    mousevec[Z] = 0;

    if (mged_variables->mv_faceplate && mged_variables->mv_orig_gui && up) {
	/*
	 * If mouse press is in scroll area, see if scrolling, and if so,
	 * divert this mouse press.
	 */
	if ((xpos >= MENUXLIM) || scroll_active) {
	    int i;

	    if (scroll_active)
		ypos = scroll_y;

	    if ((i = scroll_select(s, xpos, ypos, 1)) < 0) {
		Tcl_AppendResult(interp,
				 "mouse press outside valid scroll area\n",
				 (char *)NULL);
		return TCL_ERROR;
	    }

	    if (i > 0) {
		scroll_active = 1;
		scroll_y = ypos;

		/* Scroller bars claimed button press */
		return TCL_OK;
	    }
	    /* Otherwise, fall through */
	}

	/*
	 * If menu is active, and mouse press is in menu area, divert
	 * this mouse press for menu purposes.
	 */
	if (xpos < MENUXLIM) {
	    int i;

	    if ((i = mmenu_select(s, ypos, 1)) < 0) {
		Tcl_AppendResult(interp,
				 "mouse press outside valid menu\n",
				 (char *)NULL);
		return TCL_ERROR;
	    }

	    if (i > 0) {
		/* Menu claimed button press */
		return TCL_OK;
	    }
	    /* Otherwise, fall through */
	}
    }

    /*
     * In the best of all possible worlds, nothing should happen when
     * the mouse is not pressed; this would relax the requirement for
     * the host being informed when the mouse changes position.
     * However, for now, illuminate mode makes this impossible.
     */
    if (up == 0) switch (s->global_editing_state) {

	case ST_VIEW:
	case ST_S_EDIT:
	case ST_O_EDIT:
	default:
	    return TCL_OK;		/* Take no action in these states */

	case ST_O_PICK:
	case ST_S_PICK:
	    /*
	     * Use the mouse for illuminating a solid
	     */
	    illuminate(s, ypos);
	    return TCL_OK;

	case ST_O_PATH:
	    /*
	     * Convert DT position to path element select
	     */
	    isave = ipathpos;
	    if (bdata)
		ipathpos = bdata->s_fullpath.fp_len-1 - (
			(ypos+(int)BV_MAX) * (bdata->s_fullpath.fp_len) / (int)BV_RANGE);
	    if (ipathpos != isave)
		view_state->vs_flag = 1;
	    return TCL_OK;

    } else switch (s->global_editing_state) {

	case ST_VIEW:
	    /*
	     * Use the DT for moving view center.  Make indicated
	     * point be new view center (NEW).
	     */
	    slewview(s, mousevec);
	    return TCL_OK;

	case ST_O_PICK:
	    ipathpos = 0;
	    (void)chg_state(s, ST_O_PICK, ST_O_PATH, "mouse press");
	    view_state->vs_flag = 1;
	    return TCL_OK;

	case ST_S_PICK:
	    /* Check details, Init menu, set state */
	    init_sedit(s);		/* does chg_state */
	    view_state->vs_flag = 1;
	    return TCL_OK;

	case ST_S_EDIT:
	    if ((SEDIT_TRAN || SEDIT_SCALE || SEDIT_PICK) && mged_variables->mv_transform == 'e')
		sedit_mouse(s, mousevec);
	    else
		slewview(s, mousevec);
	    return TCL_OK;

	case ST_O_PATH:
	    /*
	     * Set combination "illuminate" mode.  This code assumes
	     * that the user has already illuminated a single solid,
	     * and wishes to move a collection of objects of which the
	     * illuminated solid is a part.  The whole combination
	     * will not illuminate (to save vector drawing time), but
	     * all the objects should move/scale in unison.
	     */
	    {
		const char *av[3];
		char num[8];
		(void)sprintf(num, "%d", ipathpos);
		av[0] = "matpick";
		av[1] = num;
		av[2] = (char *)NULL;
		(void)f_matpick(clientData, interp, 2, av);
		/* How to record this in the journal file? */
		return TCL_OK;
	    }

	case ST_S_VPICK:
	    sedit_vpick(s, mousevec);
	    return TCL_OK;

	case ST_O_EDIT:
	    if ((OEDIT_TRAN || OEDIT_SCALE) && mged_variables->mv_transform == 'e')
		objedit_mouse(s, mousevec);
	    else
		slewview(s, mousevec);

	    return TCL_OK;

	default:
	    state_err(s, "mouse press");
	    return TCL_ERROR;
    }
    /* NOTREACHED */
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
