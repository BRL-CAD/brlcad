/*                           S E T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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
/** @file mged/set.c
 *
 */

#include "common.h"

#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "dg.h"

#include "./sedit.h"
#include "./mged.h"
#include "./mged_dm.h"

#include "tcl.h"

extern void predictor_hook(void);

extern void fbserv_set_port(void);

extern void set_perspective(void);

static void set_dirty_flag(void);
static void nmg_eu_dist_set(void);
static void set_dlist(void);
static void establish_perspective(void);
static void toggle_perspective(void);
static void set_coords(void);
static void set_rotate_about(void);

static char *read_var(ClientData clientData, Tcl_Interp *interp, char *name1, char *name2, int flags);
static char *write_var(ClientData clientData, Tcl_Interp *interp, char *name1, char *name2, int flags);
static char *unset_var(ClientData clientData, Tcl_Interp *interp, char *name1, char *name2, int flags);

void set_scroll_private(void);
void set_absolute_tran(void);
void set_absolute_view_tran(void);
void set_absolute_model_tran(void);

static int perspective_table[] = { 90, 30, 45, 60 };

struct _mged_variables default_mged_variables = {
    /* mv_rc */			1,
    /* mv_autosize */		1,
    /* mv_rateknobs */		0,
    /* mv_sliders */		0,
    /* mv_faceplate */		1,
    /* mv_orig_gui */		1,
    /* mv_linewidth */		1,
    /* mv_linestyle */		's',
    /* mv_hot_key */		0,
    /* mv_context */		1,
    /* mv_dlist */		0,
    /* mv_use_air */		0,
    /* mv_listen */		0,
    /* mv_port */		0,
    /* mv_fb */			0,
    /* mv_fb_all */		1,
    /* mv_fb_overlay */		0,
    /* mv_mouse_behavior */	'd',
    /* mv_coords */		'v',
    /* mv_rotate_about */	'v',
    /* mv_transform */		'v',
    /* mv_predictor */		0,
    /* mv_predictor_advance */	1.0,
    /* mv_predictor_length */	2.0,
    /* mv_perspective */	-1,
    /* mv_perspective_mode */	0,
    /* mv_toggle_perspective */	1,
    /* mv_nmg_eu_dist */	0.05,
    /* mv_eye_sep_dist */	0.0,
    /* mv_union lexeme */	"u",
    /* mv_intersection lexeme */"n",
    /* mv_difference lexeme */	"-"
};


#define MV_O(_m) bu_offsetof(struct _mged_variables, _m)
#define MV_OA(_m) bu_offsetofarray(struct _mged_variables, _m)
#define LINE RT_MAXLINE
struct bu_structparse mged_vparse[] = {
    {"%d", 1, "autosize",		MV_O(mv_autosize),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "rateknobs",		MV_O(mv_rateknobs),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "sliders",		MV_O(mv_sliders),		set_scroll_private, NULL, NULL },
    {"%d", 1, "faceplate",		MV_O(mv_faceplate),		set_dirty_flag, NULL, NULL },
    {"%d", 1, "orig_gui",		MV_O(mv_orig_gui),	        set_dirty_flag, NULL, NULL },
    {"%d", 1, "linewidth",		MV_O(mv_linewidth),		set_dirty_flag, NULL, NULL },
    {"%c", 1, "linestyle",		MV_O(mv_linestyle),		set_dirty_flag, NULL, NULL },
    {"%d", 1, "hot_key",		MV_O(mv_hot_key),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "context",		MV_O(mv_context),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "dlist",			MV_O(mv_dlist),			set_dlist, NULL, NULL },
    {"%d", 1, "use_air",		MV_O(mv_use_air),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "listen",			MV_O(mv_listen),		fbserv_set_port, NULL, NULL },
    {"%d", 1, "port",			MV_O(mv_port),			fbserv_set_port, NULL, NULL },
    {"%d", 1, "fb",			MV_O(mv_fb),			set_dirty_flag, NULL, NULL },
    {"%d", 1, "fb_all",			MV_O(mv_fb_all),		set_dirty_flag, NULL, NULL },
    {"%d", 1, "fb_overlay",		MV_O(mv_fb_overlay),		set_dirty_flag, NULL, NULL },
    {"%c", 1, "mouse_behavior",		MV_O(mv_mouse_behavior),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%c", 1, "coords",			MV_O(mv_coords),		set_coords, NULL, NULL },
    {"%c", 1, "rotate_about",		MV_O(mv_rotate_about),		set_rotate_about, NULL, NULL },
    {"%c", 1, "transform",		MV_O(mv_transform),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d", 1, "predictor",		MV_O(mv_predictor),		predictor_hook, NULL, NULL },
    {"%f", 1, "predictor_advance",	MV_O(mv_predictor_advance),	predictor_hook, NULL, NULL },
    {"%f", 1, "predictor_length",	MV_O(mv_predictor_length),	predictor_hook, NULL, NULL },
    {"%f", 1, "perspective",		MV_O(mv_perspective),		set_perspective, NULL, NULL },
    {"%d", 1, "perspective_mode",	MV_O(mv_perspective_mode),	establish_perspective, NULL, NULL },
    {"%d", 1, "toggle_perspective",	MV_O(mv_toggle_perspective),	toggle_perspective, NULL, NULL },
    {"%f", 1, "nmg_eu_dist",		MV_O(mv_nmg_eu_dist),		nmg_eu_dist_set, NULL, NULL },
    {"%f", 1, "eye_sep_dist",		MV_O(mv_eye_sep_dist),		set_dirty_flag, NULL, NULL },
    {"%s", LINE, "union_op",		MV_O(mv_union_lexeme[0]),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%s", LINE, "intersection_op",	MV_O(mv_intersection_lexeme[0]),BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%s", LINE, "difference_op",	MV_O(mv_difference_lexeme[0]),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, NULL,			0,				BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


static void
set_dirty_flag(void)
{
    struct dm_list *dmlp;

    FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
	if (dmlp->dml_mged_variables == mged_variables)
	    dmlp->dml_dirty = 1;
}


static void
nmg_eu_dist_set(void)
{
    struct bu_vls tmp_vls;

    nmg_eue_dist = mged_variables->mv_nmg_eu_dist;

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "New nmg_eue_dist = %g\n", nmg_eue_dist);
    Tcl_AppendResult(INTERP, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
}


/**
 ** R E A D _ V A R
 **
 ** Callback used when an MGED variable is read with either the Tcl "set"
 ** command or the Tcl dereference operator '$'.
 **
 **/

static char *
read_var(ClientData clientData, Tcl_Interp *interp, char *UNUSED(name1), char *UNUSED(name2), int flags)
    /* Contains pointer to bu_struct_parse entry */


{
    struct bu_structparse *sp = (struct bu_structparse *)clientData;
    struct bu_vls str;

    /* Ask the libbu structparser for the value of the variable */

    bu_vls_init(&str);
    bu_vls_struct_item(&str, sp, (const char *)mged_variables, ' ');

    /* Next, set the Tcl variable to this value */
    (void)Tcl_SetVar(interp, sp->sp_name, bu_vls_addr(&str),
		     (flags&TCL_GLOBAL_ONLY)|TCL_LEAVE_ERR_MSG);

    bu_vls_free(&str);

    return NULL;
}


/**
 ** W R I T E _ V A R
 **
 ** Callback used when an MGED variable is set with the Tcl "set" command.
 **
 **/

static char *
write_var(ClientData clientData, Tcl_Interp *interp, char *name1, char *name2, int flags)
{
    struct bu_structparse *sp = (struct bu_structparse *)clientData;
    struct bu_vls str;
    const char *newvalue;

    newvalue = Tcl_GetVar(interp, sp->sp_name,
			  (flags&TCL_GLOBAL_ONLY)|TCL_LEAVE_ERR_MSG);
    bu_vls_init(&str);
    bu_vls_printf(&str, "%s=\"%s\"", name1, newvalue);
    if (bu_struct_parse(&str, mged_vparse, (char *)mged_variables) < 0) {
	Tcl_AppendResult(interp, "ERROR OCCURED WHEN SETTING ", name1,
			 " TO ", newvalue, "\n", (char *)NULL);
    }

    bu_vls_free(&str);
    return read_var(clientData, interp, name1, name2,
		    (flags&(~TCL_TRACE_WRITES))|TCL_TRACE_READS);
}


/**
 ** U N S E T _ V A R
 **
 ** Callback used when an MGED variable is unset.  This function undoes that.
 **
 **/

static char *
unset_var(ClientData clientData, Tcl_Interp *interp, char *name1, char *name2, int flags)
{
    struct bu_structparse *sp = (struct bu_structparse *)clientData;

    if (flags & TCL_INTERP_DESTROYED)
	return NULL;

    Tcl_AppendResult(interp, "mged variables cannot be unset\n", (char *)NULL);
    Tcl_TraceVar(interp, sp->sp_name, TCL_TRACE_READS,
		 (Tcl_VarTraceProc *)read_var,
		 (ClientData)sp);
    Tcl_TraceVar(interp, sp->sp_name, TCL_TRACE_WRITES,
		 (Tcl_VarTraceProc *)write_var,
		 (ClientData)sp);
    Tcl_TraceVar(interp, sp->sp_name, TCL_TRACE_UNSETS,
		 (Tcl_VarTraceProc *)unset_var,
		 (ClientData)sp);
    read_var(clientData, interp, name1, name2,
	     (flags&(~TCL_TRACE_UNSETS))|TCL_TRACE_READS);
    return NULL;
}


/**
 ** M G E D _ V A R I A B L E _ S E T U P
 **
 ** Sets the variable traces for each of the MGED variables so they can be
 ** accessed with the Tcl "set" and "$" operators.
 **
 **/

void
mged_variable_setup(Tcl_Interp *interp)
{
    struct bu_structparse *sp;

    for (sp = &mged_vparse[0]; sp->sp_name != NULL; sp++) {
	read_var((ClientData)sp, interp, sp->sp_name, (char *)NULL, 0);
	Tcl_TraceVar(interp, sp->sp_name, TCL_TRACE_READS|TCL_GLOBAL_ONLY,
		     (Tcl_VarTraceProc *)read_var, (ClientData)sp);
	Tcl_TraceVar(interp, sp->sp_name, TCL_TRACE_WRITES|TCL_GLOBAL_ONLY,
		     (Tcl_VarTraceProc *)write_var, (ClientData)sp);
	Tcl_TraceVar(interp, sp->sp_name, TCL_TRACE_UNSETS|TCL_GLOBAL_ONLY,
		     (Tcl_VarTraceProc *)unset_var, (ClientData)sp);
    }
}


int
f_set(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    struct bu_vls vls;

    bu_vls_init(&vls);

    if (argc < 1 || 2 < argc) {
	bu_vls_printf(&vls, "help vars");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    mged_vls_struct_parse_old(&vls, "mged variables", mged_vparse,
			      (char *)&mged_variables, argc, argv);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


void
set_scroll_private(void)
{
    struct dm_list *dmlp;
    struct dm_list *save_dmlp;

    save_dmlp = curr_dm_list;

    FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
	if (dmlp->dml_mged_variables == save_dmlp->dml_mged_variables) {
	    curr_dm_list = dmlp;

	    if (mged_variables->mv_faceplate && mged_variables->mv_orig_gui) {
		if (mged_variables->mv_sliders)	/* zero slider variables */
		    mged_svbase();

		set_scroll();		/* set scroll_array for drawing the scroll bars */
		dirty = 1;
	    }
	}

    curr_dm_list = save_dmlp;
}


void
set_absolute_tran(void)
{
    /* calculate absolute_tran */
    set_absolute_view_tran();

    /* calculate absolute_model_tran */
    set_absolute_model_tran();
}


void
set_absolute_view_tran(void)
{
    /* calculate absolute_tran */
    MAT4X3PNT(view_state->vs_absolute_tran, view_state->vs_gvp->gv_model2view, view_state->vs_orig_pos);
    /* This is used in f_knob()  ---- needed in case absolute_tran is set from Tcl */
    VMOVE(view_state->vs_last_absolute_tran, view_state->vs_absolute_tran);
}


void
set_absolute_model_tran(void)
{
    point_t new_pos;
    point_t diff;

    /* calculate absolute_model_tran */
    MAT_DELTAS_GET_NEG(new_pos, view_state->vs_gvp->gv_center);
    VSUB2(diff, view_state->vs_orig_pos, new_pos);
    VSCALE(view_state->vs_absolute_model_tran, diff, 1/view_state->vs_gvp->gv_scale);
    /* This is used in f_knob()  ---- needed in case absolute_model_tran is set from Tcl */
    VMOVE(view_state->vs_last_absolute_model_tran, view_state->vs_absolute_model_tran);
}


static void
set_dlist(void)
{
    struct dm_list *dlp1;
    struct dm_list *dlp2;
    struct dm_list *save_dlp;

    /* save current display manager */
    save_dlp = curr_dm_list;

    if (mged_variables->mv_dlist) {
	/* create display lists */

	/* for each display manager dlp1 that shares its' dml_mged_variables with save_dlp */
	FOR_ALL_DISPLAYS(dlp1, &head_dm_list.l) {
	    if (dlp1->dml_mged_variables != save_dlp->dml_mged_variables) {
		continue;
	    }

	    if (dlp1->dml_dmp->dm_displaylist &&
		dlp1->dml_dlist_state->dl_active == 0) {
		curr_dm_list = dlp1;
		createDLists(&gedp->ged_gdp->gd_headDisplay);
		dlp1->dml_dlist_state->dl_active = 1;
		dlp1->dml_dirty = 1;
	    }
	}
    } else {
	/*
	 * Free display lists if not being used by another display manager
	 */

	/* for each display manager dlp1 that shares its' dml_mged_variables with save_dlp */
	FOR_ALL_DISPLAYS(dlp1, &head_dm_list.l) {
	    if (dlp1->dml_mged_variables != save_dlp->dml_mged_variables)
		continue;

	    if (dlp1->dml_dlist_state->dl_active) {
		/* for each display manager dlp2 that is sharing display lists with dlp1 */
		FOR_ALL_DISPLAYS(dlp2, &head_dm_list.l) {
		    if (dlp2->dml_dlist_state != dlp1->dml_dlist_state) {
			continue;
		    }

		    /* found a dlp2 that is actively using dlp1's display lists */
		    if (dlp2->dml_mged_variables->mv_dlist)
			break;
		}

		/* these display lists are not being used, so free them */
		if (BU_LIST_IS_HEAD(dlp2, &head_dm_list.l)) {
		    struct ged_display_list *gdlp;
		    struct ged_display_list *next_gdlp;

		    dlp1->dml_dlist_state->dl_active = 0;

		    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
		    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
			next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

			DM_FREEDLISTS(dlp1->dml_dmp,
				      BU_LIST_FIRST(solid, &gdlp->gdl_headSolid)->s_dlist,
				      BU_LIST_LAST(solid, &gdlp->gdl_headSolid)->s_dlist -
				      BU_LIST_FIRST(solid, &gdlp->gdl_headSolid)->s_dlist + 1);

			gdlp = next_gdlp;
		    }
		}
	    }
	}
    }

    /* restore current display manager */
    curr_dm_list = save_dlp;
}


extern void
set_perspective(void)
{
    /* if perspective is set to something greater than 0, turn perspective mode on */
    if (mged_variables->mv_perspective > 0)
	mged_variables->mv_perspective_mode = 1;
    else
	mged_variables->mv_perspective_mode = 0;

    /* keep view object in sync */
    view_state->vs_gvp->gv_perspective = mged_variables->mv_perspective;

    /* keep display manager in sync */
    dmp->dm_perspective = mged_variables->mv_perspective_mode;

    set_dirty_flag();
}


static void
establish_perspective(void)
{
    mged_variables->mv_perspective = mged_variables->mv_perspective_mode ?
	perspective_table[perspective_angle] : -1;

    /* keep view object in sync */
    view_state->vs_gvp->gv_perspective = mged_variables->mv_perspective;

    /* keep display manager in sync */
    dmp->dm_perspective = mged_variables->mv_perspective_mode;

    set_dirty_flag();
}


/*
  This routine toggles the perspective_angle if the
  toggle_perspective value is 0 or less. Otherwise, the
  perspective_angle is set to the value of (toggle_perspective - 1).
*/
static void
toggle_perspective(void)
{
    /* set perspective matrix */
    if (mged_variables->mv_toggle_perspective > 0)
	perspective_angle = mged_variables->mv_toggle_perspective <= 4 ?
	    mged_variables->mv_toggle_perspective - 1: 3;
    else if (--perspective_angle < 0) /* toggle perspective matrix */
	perspective_angle = 3;

    /*
      Just in case the "!" is used with the set command. This
      allows us to toggle through more than two values.
    */
    mged_variables->mv_toggle_perspective = 1;

    if (!mged_variables->mv_perspective_mode)
	return;

    mged_variables->mv_perspective = perspective_table[perspective_angle];

    /* keep view object in sync */
    view_state->vs_gvp->gv_perspective = mged_variables->mv_perspective;

    /* keep display manager in sync */
    dmp->dm_perspective = mged_variables->mv_perspective_mode;

    set_dirty_flag();
}


static void
set_coords(void)
{
    view_state->vs_gvp->gv_coord = mged_variables->mv_coords;
}


static void
set_rotate_about(void)
{
    view_state->vs_gvp->gv_rotate_about = mged_variables->mv_rotate_about;
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
