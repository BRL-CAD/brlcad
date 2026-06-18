/*                        U S E P E N . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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
#include "bsg/interaction.h"
#include "bsg/selection.h"
#include "ged/view.h"

#include "./mged.h"
#include "./mged_dm.h"

#include "./sedit.h"
#include "./menu.h"


struct mged_highlight_state mged_highlight = {GED_DRAW_SHAPE_REF_NULL_INIT, 0};

ged_draw_shape_ref
mged_highlight_shape_ref(struct mged_state *s)
{
    if (!s || !s->gedp || ged_draw_shape_ref_is_null(mged_highlight.shape))
	return GED_DRAW_SHAPE_REF_NULL;
    struct ged_draw_shape_record rec;
    if (!ged_draw_shape_record_get(s->gedp, mged_highlight.shape, &rec))
	return GED_DRAW_SHAPE_REF_NULL;
    return mged_highlight.shape;
}

int
mged_highlight_shape_record(struct mged_state *s, struct ged_draw_shape_record *out)
{
    if (!s || !s->gedp || !out || ged_draw_shape_ref_is_null(mged_highlight.shape))
	return 0;
    return ged_draw_shape_record_get(s->gedp, mged_highlight.shape, out);
}

void
mged_highlight_set_shape_ref(struct mged_state *s, ged_draw_shape_ref ref)
{
    if (!s || !s->gedp || ged_draw_shape_ref_is_null(ref)) {
	mged_highlight.shape = GED_DRAW_SHAPE_REF_NULL;
	if (s && s->gedp)
	    ged_draw_set_highlighted_shape_ref(s->gedp, GED_DRAW_SHAPE_REF_NULL);
	return;
    }
    mged_highlight.shape = ref;
    ged_draw_set_highlighted_shape_ref(s->gedp, ref);
}

void
mged_highlight_clear(struct mged_state *s)
{
    mged_highlight_set_shape_ref(s, GED_DRAW_SHAPE_REF_NULL);
}

static void
_mged_selection_set_highlighted_ref(struct mged_state *s, struct bsg_view *gvp, ged_draw_shape_ref ref)
{
    struct bsg_selection *selection = bsg_view_selection(gvp);

    if (!selection)
	return;

    struct bsg_interaction_result *result = bsg_interaction_result_create();
    if (!result)
	return;
    if (!ged_draw_shape_ref_is_null(ref)) {
	struct bsg_interaction_record *record =
	    ged_draw_shape_interaction_record(s->gedp, ref, BSG_INTERACTION_HIGHLIGHTED_REF);
	if (record)
	    bsg_interaction_result_append(result, record);
    }
    bsg_interaction_selection_apply(selection, result,
	    BSG_INTERACTION_APPLY_SET);
    bsg_interaction_result_free(result);
}


/* Callback: select the shape at position 'count' in display order. */
struct _highlight_pick_data {
    int count;
    ged_draw_shape_ref ref;
};

static int
_highlight_pick_cb(const struct ged_draw_shape_record *rec, void *ud)
{
    struct _highlight_pick_data *d = (struct _highlight_pick_data *)ud;
    if (rec && rec->visible) {
	if (d->count-- == 0) {
	    d->ref = rec->ref;
	    return 0;
	}
    }
    return 1;
}


/*
 * All shapes except the highlighted one are unhighlighted.  The highlighted
 * shape is recorded as a GED draw ref for MGED edit paths.
 */
static void
highlight_from_y(struct mged_state *s, int y) {
    int count;
    int drawn_count = bsg_view_refresh_drawn_count(view_state->vs_gvp);

    /*
     * Divide the mouse into one vertical zone per shape painted in the last
     * frame, and use the zone number as a sequential drawn-shape position.
     */
    count = ((fastf_t)y + BSG_VIEW_MAX) * drawn_count / BSG_VIEW_RANGE;

    struct _highlight_pick_data d;
    d.count = count;
    d.ref = GED_DRAW_SHAPE_REF_NULL;
    ged_draw_foreach_shape_record(s->gedp, _highlight_pick_cb, &d);
    mged_highlight_set_shape_ref(s, d.ref);

    /* Mirror the highlighted shape into the view interaction selection so
     * highlight rendering and resolved appearance see a typed record rather
     * than only the legacy global. */
    _mged_selection_set_highlighted_ref(s, view_state->vs_gvp, mged_highlight.shape);

    mged_refresh_request_all(s, BSG_VIEW_REFRESH_ALL);
    mged_dm_repaint_request(s->mged_curr_dm, MGED_REPAINT_INTERACTION);
}


/*
 * advance highlighted_shape or highlight_path_pos
 */
int
f_aip(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct ged_draw_shape_record hrec;
    int have_highlight = mged_highlight_shape_record(s, &hrec);

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel aip");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (!bsg_view_refresh_drawn_count(view_state->vs_gvp)) {
	return TCL_OK;
    } else if (s->global_editing_state != ST_S_PICK && s->global_editing_state != ST_O_PICK  && s->global_editing_state != ST_O_PATH) {
	return TCL_OK;
    }

    if (s->global_editing_state == ST_O_PATH && have_highlight && hrec.fullpath) {
	if (argc == 1 || *argv[1] == 'f') {
	    ++highlight_path_pos;
	    if ((size_t)highlight_path_pos >= hrec.fullpath->fp_len)
		highlight_path_pos = 0;
	} else if (*argv[1] == 'b') {
	    --highlight_path_pos;
	    if (highlight_path_pos < 0)
		highlight_path_pos = hrec.fullpath->fp_len-1;
	} else {
	    Tcl_AppendResult(interp, "aip: bad parameter - ", argv[1], "\n", (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	if (ged_draw_shape_ref_is_null(mged_highlight_shape_ref(s)))
	    return TCL_ERROR;

	/* Advance using snapshotted DFS integer index — single snapshot
	 * build, O(N) total.  ged_draw_advance_shape_ref wraps circularly. */
	int delta = (argc == 1 || *argv[1] == 'f') ? +1
	            : (*argv[1] == 'b')             ? -1
	            : 0;
	if (delta == 0) {
	    Tcl_AppendResult(interp, "aip: bad parameter - ", argv[1], "\n", (char *)NULL);
	    return TCL_ERROR;
	}
	ged_draw_shape_ref next_ref = ged_draw_advance_shape_ref(s->gedp, mged_highlight.shape, delta);
	if (ged_draw_shape_ref_is_null(next_ref)) {
	    /* No shapes drawn — nothing to advance to */
	    return TCL_OK;
	}
	mged_highlight_set_shape_ref(s, next_ref);
    }

    /* Keep interaction selection in sync with the highlighted draw ref. */
    {
	_mged_selection_set_highlighted_ref(s, view_state->vs_gvp, mged_highlight.shape);
    }

    mged_refresh_request_all(s, BSG_VIEW_REFRESH_ALL);
    mged_dm_repaint_request(s->mged_curr_dm, MGED_REPAINT_INTERACTION);
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
 * variable "highlight_path_pos".
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

    char *cp;
    size_t j;
    int illum_only = 0;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    struct ged_draw_shape_record hrec;

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

    if (!mged_highlight_shape_record(s, &hrec) || !hrec.fullpath)
	return TCL_ERROR;

    if ((cp = strchr(argv[1], '/')) != NULL) {
	struct directory *d0, *d1;
	if ((d1 = db_lookup(s->dbip, cp+1, LOOKUP_NOISY)) == RT_DIR_NULL)
	    return TCL_ERROR;
	*cp = '\0';		/* modifies argv[1] */
	if ((d0 = db_lookup(s->dbip, argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	    return TCL_ERROR;
	/* Find arc on highlighted_shape path which runs from d0 to d1 */
	for (j=1; j < hrec.fullpath->fp_len; j++) {
	    if (DB_FULL_PATH_GET(hrec.fullpath, j-1) != d0) continue;
	    if (DB_FULL_PATH_GET(hrec.fullpath, j-0) != d1) continue;
	    highlight_path_pos = j;
	    goto got;
	}
	Tcl_AppendResult(interp, "matpick: unable to find arc ", d0->d_namep,
			 "/", d1->d_namep, " in current selection.  Re-specify.\n",
			 (char *)NULL);
	return TCL_ERROR;
    } else {
	highlight_path_pos = atoi(argv[1]);
	if (highlight_path_pos < 0) highlight_path_pos = 0;
	else if ((size_t)highlight_path_pos >= hrec.fullpath->fp_len)
	    highlight_path_pos = hrec.fullpath->fp_len-1;
    }
 got:
    /* Include all solids with same tree top */
    {
	(void)ged_draw_set_highlighted_path_prefix(s->gedp, hrec.fullpath,
		(size_t)highlight_path_pos, 1);
    }

    if (!illum_only) {
	(void)chg_state(s, ST_O_PATH, ST_O_EDIT, "mouse press");
	chg_l2menu(s, ST_O_EDIT);

	/* begin object editing - initialize */
	init_oedit(s);
    }

    mged_refresh_request_all(s, BSG_VIEW_REFRESH_ALL);
    mged_dm_repaint_request(s->mged_curr_dm, MGED_REPAINT_INTERACTION);
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

    struct ged_draw_shape_record hrec;
    int have_highlight = mged_highlight_shape_record(s, &hrec);
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
	    highlight_from_y(s, ypos);
	    return TCL_OK;

	case ST_O_PATH:
	    /*
	     * Convert DT position to path element select
	     */
	    isave = highlight_path_pos;
	    if (have_highlight && hrec.fullpath)
		highlight_path_pos = hrec.fullpath->fp_len-1 - (
			(ypos+(int)BSG_VIEW_MAX) * (hrec.fullpath->fp_len) / (int)BSG_VIEW_RANGE);
	    if (highlight_path_pos != isave)
		mged_refresh_request_view(s, view_state, BSG_VIEW_REFRESH_VIEW);
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
	    highlight_path_pos = 0;
	    (void)chg_state(s, ST_O_PICK, ST_O_PATH, "mouse press");
	    mged_refresh_request_view(s, view_state, BSG_VIEW_REFRESH_VIEW);
	    return TCL_OK;

	case ST_S_PICK:
	    /* Check details, Init menu, set state */
	    init_sedit(s);		/* does chg_state */
	    mged_refresh_request_view(s, view_state, BSG_VIEW_REFRESH_VIEW);
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
	     * highlighted shape is a part.  The whole combination
	     * will not illuminate (to save vector drawing time), but
	     * all the objects should move/scale in unison.
	     */
	    {
		const char *av[3];
		char num[8];
		(void)sprintf(num, "%d", highlight_path_pos);
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
