/*                         E D S O L . C
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
/** @file mged/edsol.c
 *
 * TODO - probably should try setting up an equivalent to OBJ[] table
 * in librt to isolate per-primitive methods in the MGED code.  Might
 * be a little overkill structurally, but right now we've got a bunch
 * of ID_* conditional switch tables complicating the logic.  Some of
 * that we were able to push to librt (labels, keypoints) but it is
 * less clear if we can get away with that for the editing codes and
 * the menus are extremely unlikely to be suitable librt canddiates.
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "rt/arb_edit.h"
#include "wdb.h"
#include "rt/db4.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"
#include "./menu.h"
#include "./primitives/mged_functab.h"
#include "./primitives/edarb.h"
#include "./primitives/edbspline.h"

static void init_sedit_vars(struct mged_state *), init_oedit_vars(struct mged_state *), init_oedit_guts(struct mged_state *);

/* primitive specific externs.  Eventually these should all go away */
extern int es_ars_crv;  /* curve and column identifying selected ARS point */
extern int es_ars_col;
extern struct menu_item ars_menu[];
extern struct menu_item ars_pick_menu[];

extern int bot_verts[3];

extern struct wdb_metaball_pnt *es_metaball_pnt;

extern struct edgeuse *es_eu;
extern struct loopuse *lu_copy;
extern struct shell *es_s;

extern struct wdb_pipe_pnt *es_pipe_pnt;

/* data for solid editing */
int sedraw;	/* apply solid editing changes */

fastf_t es_m[3];		/* edge(line) slope */

void
set_e_axes_pos(struct mged_state *s, int both)
    /* if (!both) then set only s->edit_state.curr_e_axes_pos, otherwise
       set s->edit_state.e_axes_pos and s->edit_state.curr_e_axes_pos */
{
    update_views = 1;
    dm_set_dirty(DMP, 1);

    struct rt_db_internal *ip = &s->edit_state.es_int;

    if (MGED_OBJ[ip->idb_type].ft_e_axes_pos) {
	(*MGED_OBJ[ip->idb_type].ft_e_axes_pos)(s, ip, &s->tol.tol);
	return;
    } else {
	VMOVE(s->edit_state.curr_e_axes_pos, s->edit_state.e_keypoint);
    }

    if (both) {
	VMOVE(s->edit_state.e_axes_pos, s->edit_state.curr_e_axes_pos);

	if (EDIT_ROTATE) {
	    s->edit_state.e_edclass = EDIT_CLASS_ROTATE;
	    VSETALL(s->edit_state.edit_absolute_model_rotate, 0.0);
	    VSETALL(s->edit_state.edit_absolute_object_rotate, 0.0);
	    VSETALL(s->edit_state.edit_absolute_view_rotate, 0.0);
	    VSETALL(s->edit_state.last_edit_absolute_model_rotate, 0.0);
	    VSETALL(s->edit_state.last_edit_absolute_object_rotate, 0.0);
	    VSETALL(s->edit_state.last_edit_absolute_view_rotate, 0.0);
	} else if (EDIT_TRAN) {
	    s->edit_state.e_edclass = EDIT_CLASS_TRAN;
	    VSETALL(s->edit_state.edit_absolute_model_tran, 0.0);
	    VSETALL(s->edit_state.edit_absolute_view_tran, 0.0);
	    VSETALL(s->edit_state.last_edit_absolute_model_tran, 0.0);
	    VSETALL(s->edit_state.last_edit_absolute_view_tran, 0.0);
	} else if (EDIT_SCALE) {
	    s->edit_state.e_edclass = EDIT_CLASS_SCALE;

	    if (SEDIT_SCALE) {
		s->edit_state.edit_absolute_scale = 0.0;
		acc_sc_sol = 1.0;
	    }
	} else {
	    s->edit_state.e_edclass = EDIT_CLASS_NULL;
	}

	MAT_IDN(acc_rot_sol);

	for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	    struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	    m_dmp->dm_mged_variables->mv_transform = 'e';
	}
    }
}

/*
 * Keypoint in model space is established in "pt".
 * If "str" is set, then that point is used, else default
 * for this solid is selected and set.
 * "str" may be a constant string, in either upper or lower case,
 * or it may be something complex like "(3, 4)" for an ARS or spline
 * to select a particular vertex or control point.
 *
 * XXX Perhaps this should be done via solid-specific parse tables,
 * so that solids could be pretty-printed & structprint/structparse
 * processed as well?
 */
void
get_solid_keypoint(struct mged_state *s, point_t *pt, const char **strp, struct rt_db_internal *ip, fastf_t *mat)
{
    if (!strp)
	return;

    RT_CK_DB_INTERNAL(ip);

    if (MGED_OBJ[ip->idb_type].ft_keypoint) {
	*strp = (*MGED_OBJ[ip->idb_type].ft_keypoint)(pt, *strp, mat, ip, &s->tol.tol);
	return;
    }

    Tcl_AppendResult(s->interp, "get_solid_keypoint: unrecognized solid type (setting keypoint to origin)\n", (char *)NULL);
    VSETALL(*pt, 0.0);
    *strp = "(origin)";
}

int
f_get_solid_keypoint(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (s->edit_state.global_editing_state == ST_VIEW || s->edit_state.global_editing_state == ST_S_PICK || s->edit_state.global_editing_state == ST_O_PICK)
	return TCL_OK;

    get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag, &s->edit_state.es_int, s->edit_state.e_mat);
    return TCL_OK;
}


/*
 * First time in for this solid, set things up.
 * If all goes well, change state to ST_S_EDIT.
 * Solid editing is completed only via sedit_accept() / sedit_reject().
 */
void
init_sedit(struct mged_state *s)
{
    int type;
    int id;

    if (s->dbip == DBI_NULL || !illump)
	return;

    /*
     * Check for a processed region or other illegal solid.
     */
    if (illump->s_old.s_Eflag) {
	Tcl_AppendResult(s->interp,
			 "Unable to Solid_Edit a processed region;  select a primitive instead\n", (char *)NULL);
	return;
    }

    /* Read solid description into s->edit_state.es_int */
    if (!illump->s_u_data)
	return;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    if (rt_db_get_internal(&s->edit_state.es_int, LAST_SOLID(bdata),
			   s->dbip, NULL, &rt_uniresource) < 0) {
	if (bdata->s_fullpath.fp_len > 0) {
	    Tcl_AppendResult(s->interp, "init_sedit(",
		    LAST_SOLID(bdata)->d_namep,
		    "):  solid import failure\n", (char *)NULL);
	} else {
	    Tcl_AppendResult(s->interp, "sedit_reset(NULL):  solid import failure\n", (char *)NULL);
	}
	rt_db_free_internal(&s->edit_state.es_int);
	return;				/* FAIL */
    }
    RT_CK_DB_INTERNAL(&s->edit_state.es_int);
    id = s->edit_state.es_int.idb_type;

    s->edit_state.edit_menu = 0;

    // TODO - indicates we need per-primitive init of sedit state
    if (id == ID_ARB8) {
	struct rt_arb_internal *arb;
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;

	arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	type = rt_arb_std_type(&s->edit_state.es_int, &s->tol.tol);
	s->edit_state.e_type = type;

	if (rt_arb_calc_planes(&error_msg, arb, s->edit_state.e_type, es_peqn, &s->tol.tol)) {
	    Tcl_AppendResult(s->interp, bu_vls_addr(&error_msg),
			     "\nCannot calculate plane equations for ARB8\n",
			     (char *)NULL);
	    rt_db_free_internal(&s->edit_state.es_int);
	    bu_vls_free(&error_msg);
	    return;
	}
	bu_vls_free(&error_msg);
    } else if (id == ID_BSPLINE) {
	bspline_init_sedit(s);
    }

    /* Save aggregate path matrix */
    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, s->edit_state.e_mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(s->edit_state.e_invmat, s->edit_state.e_mat);

    /* Establish initial keypoint */
    s->edit_state.e_keytag = "";
    get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag, &s->edit_state.es_int, s->edit_state.e_mat);

    es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
    es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
    lu_copy = (struct loopuse *)NULL;
    es_ars_crv = (-1);
    es_ars_col = (-1);

    bot_verts[0] = -1;
    bot_verts[1] = -1;
    bot_verts[2] = -1;

    /* Finally, enter solid edit state */
    (void)chg_state(s, ST_S_PICK, ST_S_EDIT, "Keyboard illuminate");
    chg_l2menu(s, ST_S_EDIT);
    mged_set_edflag(s, IDLE);

    button(s, BE_S_EDIT);	/* Drop into edit menu right away */
    init_sedit_vars(s);

    {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_strcpy(&vls, "begin_edit_callback ");
	db_path_to_vls(&vls, &bdata->s_fullpath);
	(void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }
}


static void
init_sedit_vars(struct mged_state *s)
{
    MAT_IDN(acc_rot_sol);
    MAT_IDN(incr_change);

    VSETALL(s->edit_state.edit_absolute_model_rotate, 0.0);
    VSETALL(s->edit_state.edit_absolute_object_rotate, 0.0);
    VSETALL(s->edit_state.edit_absolute_view_rotate, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_model_rotate, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_object_rotate, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_view_rotate, 0.0);
    VSETALL(s->edit_state.edit_absolute_model_tran, 0.0);
    VSETALL(s->edit_state.edit_absolute_view_tran, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_model_tran, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_view_tran, 0.0);
    s->edit_state.edit_absolute_scale = 0.0;
    acc_sc_sol = 1.0;

    VSETALL(s->edit_state.edit_rate_model_rotate, 0.0);
    VSETALL(s->edit_state.edit_rate_object_rotate, 0.0);
    VSETALL(s->edit_state.edit_rate_view_rotate, 0.0);
    VSETALL(s->edit_state.edit_rate_model_tran, 0.0);
    VSETALL(s->edit_state.edit_rate_view_tran, 0.0);

    set_e_axes_pos(s, 1);
}


/*
 * All solid edit routines call this subroutine after
 * making a change to es_int or s->edit_state.e_mat.
 */
void
replot_editing_solid(struct mged_state *s)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    mat_t mat;
    struct bv_scene_obj *sp;
    struct directory *illdp;

    if (!illump) {
	return;
    }
    if (!illump->s_u_data)
	return;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    illdp = LAST_SOLID(bdata);

    gdlp = BU_LIST_NEXT(display_list, s->gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, s->gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (sp->s_u_data) {
		bdata = (struct ged_bv_data *)sp->s_u_data;
		if (LAST_SOLID(bdata) == illdp) {
		    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);
		    (void)replot_modified_solid(s, sp, &s->edit_state.es_int, mat);
		}
	    }
	}

	gdlp = next_gdlp;
    }
}


void
transform_editing_solid(
    struct mged_state *s,
    struct rt_db_internal *os,		/* output solid */
    const mat_t mat,
    struct rt_db_internal *is,		/* input solid */
    int freeflag)
{
    if (rt_matrix_transform(os, mat, is, freeflag, s->dbip, &rt_uniresource) < 0)
	bu_exit(EXIT_FAILURE, "transform_editing_solid failed to apply a matrix transform, aborting");
}


/*
 * Put up menu header
 */
void
sedit_menu(struct mged_state *s) {

    menu_state->ms_flag = 0;		/* No menu item selected yet */

    mmenu_set_all(s, MENU_L1, NULL);
    chg_l2menu(s, ST_S_EDIT);

    const struct rt_db_internal *ip = &s->edit_state.es_int;
    if (MGED_OBJ[ip->idb_type].ft_menu_item) {
	struct menu_item *mi = (*MGED_OBJ[ip->idb_type].ft_menu_item)(&s->tol.tol);
	mmenu_set_all(s, MENU_L1, mi);
    }

    mged_set_edflag(s, IDLE);	/* Drop out of previous edit mode */
    s->edit_state.edit_menu = 0;
}

const char *
get_file_name(struct mged_state *s, char *str)
{
    struct bu_vls cmd = BU_VLS_INIT_ZERO;
    struct bu_vls varname_vls = BU_VLS_INIT_ZERO;
    char *dir;
    char *fptr;
    char *ptr1;
    char *ptr2;

    bu_vls_strcpy(&varname_vls, "mged_gui(getFileDir)");

    if ((fptr=strrchr(str, '/'))) {
	dir = (char *)bu_malloc((strlen(str)+1)*sizeof(char), "get_file_name: dir");
	ptr1 = str;
	ptr2 = dir;
	while (ptr1 != fptr)
	    *ptr2++ = *ptr1++;
	*ptr2 = '\0';
	Tcl_SetVar(s->interp, bu_vls_addr(&varname_vls), dir, TCL_GLOBAL_ONLY);
	bu_free((void *)dir, "get_file_name: directory string");
    }

    if (dm_get_pathname(DMP)) {
	bu_vls_printf(&cmd,
		"getFile %s %s {{{All Files} {*}}} {Get File}",
		bu_vls_addr(dm_get_pathname(DMP)),
		bu_vls_addr(&varname_vls));
    }
    bu_vls_free(&varname_vls);

    if (Tcl_Eval(s->interp, bu_vls_addr(&cmd))) {
	bu_vls_free(&cmd);
	return (char *)NULL;
    } else if (Tcl_GetStringResult(s->interp)[0] != '\0') {
	bu_vls_free(&cmd);
	return Tcl_GetStringResult(s->interp);
    } else {
	bu_vls_free(&cmd);
    }
    return (char *)NULL;
}

/*
 * A great deal of magic takes place here, to accomplish solid editing.
 *
 * Called from mged main loop after any event handlers:
 * if (sedraw > 0) sedit(s);
 * to process any residual events that the event handlers were too
 * lazy to handle themselves.
 *
 * A lot of processing is deferred to here, so that the "p" command
 * can operate on an equal footing to mouse events.
 */
void
sedit(struct mged_state *s)
{
    if (s->dbip == DBI_NULL)
	return;

    sedraw = 0;
    ++update_views;

    // Handle any primitive specific flags TODO - will have to reserve integer
    // values for the generic operations so primitive specific methods don't
    // conflict with them.  Maybe something simple like all primitive specific
    // edflag vals need to be >RT_EDIT_OP_GENERIC_MAX?  Primitives could then
    // define with:
    // #define PRIM_OP_1 RT_EDIT_OP_GENERIC_MAX+1,
    // #define PRIM_OP_2 PRIM_OP_1+1
    // ...
    int had_method = 0;
    const struct rt_db_internal *ip = &s->edit_state.es_int;
    if (MGED_OBJ[ip->idb_type].ft_edit) {
	if ((*MGED_OBJ[ip->idb_type].ft_edit)(s, s->edit_state.edit_flag))
	    return;
	had_method = 1;
    }

    switch (s->edit_state.edit_flag) {

	case IDLE:
	    /* do nothing more */
	    --update_views;
	    break;
	default:
	    {
		if (had_method)
		    break;
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		bu_vls_printf(&tmp_vls, "sedit(s):  unknown edflag = %d.\n", s->edit_state.edit_flag);
		Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		mged_print_result(s, TCL_ERROR);
		bu_vls_free(&tmp_vls);
	    }
    }

    /* If the keypoint changed location, find about it here */
    if (!s->edit_state.e_keyfixed)
	get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag, &s->edit_state.es_int, s->edit_state.e_mat);

    set_e_axes_pos(s, 0);
    replot_editing_solid(s);

    if (update_views) {
	dm_set_dirty(DMP, 1);
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "active_edit_callback");
	(void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }

    s->edit_state.e_inpara = 0;
    s->edit_state.e_mvalid = 0;
}


/*
 * Mouse (pen) press in graphics area while doing Solid Edit.
 * mousevec [X] and [Y] are in the range -1.0...+1.0, corresponding
 * to viewspace.
 *
 * In order to allow the "p" command to do the same things that
 * a mouse event can, the preferred strategy is to store the value
 * corresponding to what the "p" command would give in s->edit_state.e_mparam,
 * set s->edit_state.e_mvalid = 1, set sedraw = 1, and return, allowing sedit(s)
 * to actually do the work.
 */
void
sedit_mouse(struct mged_state *s, const vect_t mousevec)
{
    if (s->edit_state.edit_flag <= 0)
	return;

    const struct rt_db_internal *ip = &s->edit_state.es_int;
    if (MGED_OBJ[ip->idb_type].ft_edit_xy)
	 (*MGED_OBJ[ip->idb_type].ft_edit_xy)(s, s->edit_state.edit_flag, mousevec);
}


void
sedit_abs_scale(struct mged_state *s)
{
    fastf_t old_acc_sc_sol;

    if (s->edit_state.edit_flag != SSCALE && s->edit_state.edit_flag != PSCALE)
	return;

    old_acc_sc_sol = acc_sc_sol;

    if (-SMALL_FASTF < s->edit_state.edit_absolute_scale && s->edit_state.edit_absolute_scale < SMALL_FASTF)
	acc_sc_sol = 1.0;
    else if (s->edit_state.edit_absolute_scale > 0.0)
	acc_sc_sol = 1.0 + s->edit_state.edit_absolute_scale * 3.0;
    else {
	if ((s->edit_state.edit_absolute_scale - MGED_SMALL_SCALE) < -1.0)
	    s->edit_state.edit_absolute_scale = -1.0 + MGED_SMALL_SCALE;

	acc_sc_sol = 1.0 + s->edit_state.edit_absolute_scale;
    }

    s->edit_state.es_scale = acc_sc_sol / old_acc_sc_sol;
    sedit(s);
}


/*
 * Object Edit
 */
void
objedit_mouse(struct mged_state *s, const vect_t mousevec)
{
    fastf_t scale = 1.0;
    vect_t pos_view;	 	/* Unrotated view space pos */
    vect_t pos_model;	/* Rotated screen space pos */
    vect_t tr_temp;		/* temp translation vector */
    vect_t temp;

    MAT_IDN(incr_change);
    if (movedir & SARROW) {
	/* scaling option is in effect */
	scale = 1.0 + (fastf_t)(mousevec[Y]>0 ?
				mousevec[Y] : -mousevec[Y]);
	if (mousevec[Y] <= 0)
	    scale = 1.0 / scale;

	/* switch depending on scaling option selected */
	switch (edobj) {

	    case BE_O_SCALE:
		/* global scaling */
		incr_change[15] = 1.0 / scale;

		acc_sc_obj /= incr_change[15];
		s->edit_state.edit_absolute_scale = acc_sc_obj - 1.0;
		if (s->edit_state.edit_absolute_scale > 0.0)
		    s->edit_state.edit_absolute_scale /= 3.0;
		break;

	    case BE_O_XSCALE:
		/* local scaling ... X-axis */
		incr_change[0] = scale;
		/* accumulate the scale factor */
		acc_sc[0] *= scale;
		s->edit_state.edit_absolute_scale = acc_sc[0] - 1.0;
		if (s->edit_state.edit_absolute_scale > 0.0)
		    s->edit_state.edit_absolute_scale /= 3.0;
		break;

	    case BE_O_YSCALE:
		/* local scaling ... Y-axis */
		incr_change[5] = scale;
		/* accumulate the scale factor */
		acc_sc[1] *= scale;
		s->edit_state.edit_absolute_scale = acc_sc[1] - 1.0;
		if (s->edit_state.edit_absolute_scale > 0.0)
		    s->edit_state.edit_absolute_scale /= 3.0;
		break;

	    case BE_O_ZSCALE:
		/* local scaling ... Z-axis */
		incr_change[10] = scale;
		/* accumulate the scale factor */
		acc_sc[2] *= scale;
		s->edit_state.edit_absolute_scale = acc_sc[2] - 1.0;
		if (s->edit_state.edit_absolute_scale > 0.0)
		    s->edit_state.edit_absolute_scale /= 3.0;
		break;
	}

	/* Have scaling take place with respect to keypoint,
	 * NOT the view center.
	 */
	VMOVE(temp, s->edit_state.e_keypoint);
	MAT4X3PNT(pos_model, modelchanges, temp);
	wrt_point(modelchanges, incr_change, modelchanges, pos_model);

	MAT_IDN(incr_change);
	new_edit_mats(s);
    } else if (movedir & (RARROW|UARROW)) {
	mat_t oldchanges;	/* temporary matrix */

	/* Vector from object keypoint to cursor */
	VMOVE(temp, s->edit_state.e_keypoint);
	MAT4X3PNT(pos_view, view_state->vs_model2objview, temp);

	if (movedir & RARROW)
	    pos_view[X] = mousevec[X];
	if (movedir & UARROW)
	    pos_view[Y] = mousevec[Y];

	MAT4X3PNT(pos_model, view_state->vs_gvp->gv_view2model, pos_view);/* NOT objview */
	MAT4X3PNT(tr_temp, modelchanges, temp);
	VSUB2(tr_temp, pos_model, tr_temp);
	MAT_DELTAS_VEC(incr_change, tr_temp);
	MAT_COPY(oldchanges, modelchanges);
	bn_mat_mul(modelchanges, incr_change, oldchanges);

	MAT_IDN(incr_change);
	new_edit_mats(s);

	update_edit_absolute_tran(s, pos_view);
    } else {
	Tcl_AppendResult(s->interp, "No object edit mode selected;  mouse press ignored\n", (char *)NULL);
	return;
    }
}


void
oedit_abs_scale(struct mged_state *s)
{
    fastf_t scale;
    vect_t temp;
    vect_t pos_model;
    mat_t incr_mat;

    MAT_IDN(incr_mat);

    if (-SMALL_FASTF < s->edit_state.edit_absolute_scale && s->edit_state.edit_absolute_scale < SMALL_FASTF)
	scale = 1;
    else if (s->edit_state.edit_absolute_scale > 0.0)
	scale = 1.0 + s->edit_state.edit_absolute_scale * 3.0;
    else {
	if ((s->edit_state.edit_absolute_scale - MGED_SMALL_SCALE) < -1.0)
	    s->edit_state.edit_absolute_scale = -1.0 + MGED_SMALL_SCALE;

	scale = 1.0 + s->edit_state.edit_absolute_scale;
    }

    /* switch depending on scaling option selected */
    switch (edobj) {

	case BE_O_SCALE:
	    /* global scaling */
	    incr_mat[15] = acc_sc_obj / scale;
	    acc_sc_obj = scale;
	    break;

	case BE_O_XSCALE:
	    /* local scaling ... X-axis */
	    incr_mat[0] = scale / acc_sc[0];
	    /* accumulate the scale factor */
	    acc_sc[0] = scale;
	    break;

	case BE_O_YSCALE:
	    /* local scaling ... Y-axis */
	    incr_mat[5] = scale / acc_sc[1];
	    /* accumulate the scale factor */
	    acc_sc[1] = scale;
	    break;

	case BE_O_ZSCALE:
	    /* local scaling ... Z-axis */
	    incr_mat[10] = scale / acc_sc[2];
	    /* accumulate the scale factor */
	    acc_sc[2] = scale;
	    break;
    }

    /* Have scaling take place with respect to keypoint,
     * NOT the view center.
     */
    VMOVE(temp, s->edit_state.e_keypoint);
    MAT4X3PNT(pos_model, modelchanges, temp);
    wrt_point(modelchanges, incr_mat, modelchanges, pos_model);

    new_edit_mats(s);
}


void
vls_solid(struct mged_state *s, struct bu_vls *vp, struct rt_db_internal *ip, const mat_t mat)
{
    struct rt_db_internal intern;
    int id;

    RT_DB_INTERNAL_INIT(&intern);

    if (s->dbip == DBI_NULL)
	return;

    BU_CK_VLS(vp);
    RT_CK_DB_INTERNAL(ip);

    id = ip->idb_type;
    transform_editing_solid(s, &intern, mat, (struct rt_db_internal *)ip, 0);

    if (id != ID_ARS && id != ID_POLY && id != ID_BOT) {
	if (OBJ[id].ft_describe(vp, &intern, 1 /*verbose*/, s->dbip->dbi_base2local) < 0)
	    Tcl_AppendResult(s->interp, "vls_solid: describe error\n", (char *)NULL);
    } else {
	if (OBJ[id].ft_describe(vp, &intern, 0 /* not verbose */, s->dbip->dbi_base2local) < 0)
	    Tcl_AppendResult(s->interp, "vls_solid: describe error\n", (char *)NULL);
    }

    if (id == ID_PIPE && es_pipe_pnt) {
	struct rt_pipe_internal *pipeip;
	struct wdb_pipe_pnt *ps=(struct wdb_pipe_pnt *)NULL;
	int seg_no = 0;

	pipeip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pipeip);

	for (BU_LIST_FOR(ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	    seg_no++;
	    if (ps == es_pipe_pnt)
		break;
	}

	if (ps == es_pipe_pnt)
	    rt_vls_pipe_pnt(vp, seg_no, &intern, s->dbip->dbi_base2local);
    }

    rt_db_free_internal(&intern);
}


static void
init_oedit_guts(struct mged_state *s)
{
    int id;
    const char *strp="";

    /* for safety sake */
    s->edit_state.edit_menu = 0;
    mged_set_edflag(s, -1);
    MAT_IDN(s->edit_state.e_mat);

    if (s->dbip == DBI_NULL || !illump) {
	return;
    }

    /*
     * Check for a processed region
     */
    if (illump->s_old.s_Eflag) {
	/* Have a processed (E'd) region - NO key solid.
	 * Use the 'center' as the key
	 */
	VMOVE(s->edit_state.e_keypoint, illump->s_center);

	/* The s_center takes the s->edit_state.e_mat into account already */
    }

    /* Not an evaluated region - just a regular path ending in a solid */
    if (!illump->s_u_data)
	return;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    if (rt_db_get_internal(&s->edit_state.es_int, LAST_SOLID(bdata),
			   s->dbip, NULL, &rt_uniresource) < 0) {
	if (bdata->s_fullpath.fp_len > 0) {
	    Tcl_AppendResult(s->interp, "init_oedit(",
		    LAST_SOLID(bdata)->d_namep,
		    "):  solid import failure\n", (char *)NULL);
	} else {
	    Tcl_AppendResult(s->interp, "sedit_reset(NULL):  solid import failure\n", (char *)NULL);
	}
	rt_db_free_internal(&s->edit_state.es_int);
	button(s, BE_REJECT);
	return;				/* FAIL */
    }
    RT_CK_DB_INTERNAL(&s->edit_state.es_int);
    id = s->edit_state.es_int.idb_type;

    if (id == ID_ARB8) {
	struct rt_arb_internal *arb;

	arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	s->edit_state.e_type = rt_arb_std_type(&s->edit_state.es_int, &s->tol.tol);
    }

    /* Save aggregate path matrix */
    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, s->edit_state.e_mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(s->edit_state.e_invmat, s->edit_state.e_mat);

    get_solid_keypoint(s, &s->edit_state.e_keypoint, &strp, &s->edit_state.es_int, s->edit_state.e_mat);
    init_oedit_vars(s);
}


static void
init_oedit_vars(struct mged_state *s)
{
    set_e_axes_pos(s, 1);

    VSETALL(s->edit_state.edit_absolute_model_rotate, 0.0);
    VSETALL(s->edit_state.edit_absolute_object_rotate, 0.0);
    VSETALL(s->edit_state.edit_absolute_view_rotate, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_model_rotate, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_object_rotate, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_view_rotate, 0.0);
    VSETALL(s->edit_state.edit_absolute_model_tran, 0.0);
    VSETALL(s->edit_state.edit_absolute_view_tran, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_model_tran, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_view_tran, 0.0);
    s->edit_state.edit_absolute_scale = 0.0;
    acc_sc_sol = 1.0;
    acc_sc_obj = 1.0;
    VSETALL(acc_sc, 1.0);

    VSETALL(s->edit_state.edit_rate_model_rotate, 0.0);
    VSETALL(s->edit_state.edit_rate_object_rotate, 0.0);
    VSETALL(s->edit_state.edit_rate_view_rotate, 0.0);
    VSETALL(s->edit_state.edit_rate_model_tran, 0.0);
    VSETALL(s->edit_state.edit_rate_view_tran, 0.0);

    MAT_IDN(modelchanges);
    MAT_IDN(acc_rot_sol);
}


void
init_oedit(struct mged_state *s)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    /* do real initialization work */
    init_oedit_guts(s);

    s->edit_state.e_edclass = EDIT_CLASS_NULL;

    /* begin edit callback */
    bu_vls_strcpy(&vls, "begin_edit_callback {}");
    (void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


void oedit_reject(struct mged_state *s);

static void
oedit_apply(struct mged_state *s, int continue_editing)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    /* matrices used to accept editing done from a depth
     * >= 2 from the top of the illuminated path
     */
    mat_t topm;	/* accum matrix from pathpos 0 to i-2 */
    mat_t inv_topm;	/* inverse */
    mat_t deltam;	/* final "changes":  deltam = (inv_topm)(modelchanges)(topm) */
    mat_t tempm;

    if (!illump || !illump->s_u_data)
	return;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

    switch (ipathpos) {
	case 0:
	    moveHobj(s, DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos),
		     modelchanges);
	    break;
	case 1:
	    moveHinstance(s, DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos-1),
			  DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos),
			  modelchanges);
	    break;
	default:
	    MAT_IDN(topm);
	    MAT_IDN(inv_topm);
	    MAT_IDN(deltam);
	    MAT_IDN(tempm);

	    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, topm, ipathpos-1, &rt_uniresource);

	    bn_mat_inv(inv_topm, topm);

	    bn_mat_mul(tempm, modelchanges, topm);
	    bn_mat_mul(deltam, inv_topm, tempm);

	    moveHinstance(s, DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos-1),
			  DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos),
			  deltam);
	    break;
    }

    /*
     * Redraw all solids affected by this edit.
     * Regenerate a new control list which does not
     * include the solids about to be replaced,
     * so we can safely fiddle the displaylist.
     */
    modelchanges[15] = 1000000000;	/* => small ratio */

    /* Now, recompute new chunks of displaylist */
    gdlp = BU_LIST_NEXT(display_list, s->gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, s->gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (sp->s_iflag == DOWN)
		continue;
	    (void)replot_original_solid(s, sp);

	    if (continue_editing == DOWN) {
		sp->s_iflag = DOWN;
	    }
	}

	gdlp = next_gdlp;
    }
}


void
oedit_accept(struct mged_state *s)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;

    if (s->dbip == DBI_NULL)
	return;

    if (s->dbip->dbi_read_only) {
	oedit_reject(s);

	gdlp = BU_LIST_NEXT(display_list, s->gedp->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, s->gedp->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (sp->s_iflag == DOWN)
		    continue;
		(void)replot_original_solid(s, sp);
		sp->s_iflag = DOWN;
	    }

	    gdlp = next_gdlp;
	}

	bu_log("Sorry, this database is READ-ONLY\n");
	pr_prompt(s);

	return;
    }

    oedit_apply(s, DOWN); /* finished editing */
    oedit_reject(s);
}


void
oedit_reject(struct mged_state *s)
{
    rt_db_free_internal(&s->edit_state.es_int);
}


/*
 * Gets the A, B, C of a planar equation from the command line and puts the
 * result into the array es_peqn[] at the position pointed to by the variable
 * 'edit_menu' which is the plane being redefined. This function is only callable
 * when in solid edit and rotating the face of a GENARB8.
 */
int
f_eqn(ClientData clientData, Tcl_Interp *UNUSED(interp), int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int ret = arb_f_eqn(s, argc, argv);
    if (ret != TCL_OK)
	return ret;

    /* draw the new version of the solid */
    replot_editing_solid(s);

    /* update display information */
    view_state->vs_flag = 1;

    return TCL_OK;
}


/* Hooks from buttons.c */

/*
 * Copied from sedit_accept - modified to optionally leave
 * solid edit state.
 */
static int
sedit_apply(struct mged_state *s, int accept_flag)
{
    struct directory *dp;

    es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
    es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
    bot_verts[0] = -1;
    bot_verts[1] = -1;
    bot_verts[2] = -1;

    /* make sure we are in solid edit mode */
    if (!illump) {
	return TCL_OK;
    }

    if (lu_copy) {
	struct model *m;

	m = nmg_find_model(&lu_copy->l.magic);
	nmg_km(m);
	lu_copy = (struct loopuse *)NULL;
    }

    /* write editing changes out to disc */
    if (!illump->s_u_data)
	return TCL_ERROR;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    dp = LAST_SOLID(bdata);
    if (!dp) {
	/* sanity check, unexpected error */
	return TCL_ERROR;
    }

    /* make sure that any BOT solid is minimally legal */
    if (s->edit_state.es_int.idb_type == ID_BOT) {
	struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;

	RT_BOT_CK_MAGIC(bot);
	if (bot->mode == RT_BOT_SURFACE || bot->mode == RT_BOT_SOLID) {
	    /* make sure facemodes and thicknesses have been freed */
	    if (bot->thickness) {
		bu_free((char *)bot->thickness, "BOT thickness");
		bot->thickness = NULL;
	    }
	    if (bot->face_mode) {
		bu_free((char *)bot->face_mode, "BOT face_mode");
		bot->face_mode = NULL;
	    }
	} else {
	    /* make sure face_modes and thicknesses exist */
	    if (!bot->thickness)
		bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t), "BOT thickness");
	    if (!bot->face_mode) {
		bot->face_mode = bu_bitv_new(bot->num_faces);
	    }
	}
    }

    /* Scale change on export is 1.0 -- no change */
    if (rt_db_put_internal(dp, s->dbip, &s->edit_state.es_int, &rt_uniresource) < 0) {
	Tcl_AppendResult(s->interp, "sedit_apply(", dp->d_namep,
			 "):  solid export failure\n", (char *)NULL);
	if (accept_flag) {
	    rt_db_free_internal(&s->edit_state.es_int);
	}
	return TCL_ERROR;				/* FAIL */
    }

    if (accept_flag) {
	menu_state->ms_flag = 0;
	movedir = 0;
	mged_set_edflag(s, -1);
	s->edit_state.e_edclass = EDIT_CLASS_NULL;

	rt_db_free_internal(&s->edit_state.es_int);
    } else {
	/* XXX hack to restore s->edit_state.es_int after rt_db_put_internal blows it away */
	/* Read solid description into s->edit_state.es_int again! Gaak! */
	if (rt_db_get_internal(&s->edit_state.es_int, LAST_SOLID(bdata),
			       s->dbip, NULL, &rt_uniresource) < 0) {
	    Tcl_AppendResult(s->interp, "sedit_apply(",
			     LAST_SOLID(bdata)->d_namep,
			     "):  solid reimport failure\n", (char *)NULL);
	    rt_db_free_internal(&s->edit_state.es_int);
	    return TCL_ERROR;
	}
    }


    return TCL_OK;
}


void
sedit_accept(struct mged_state *s)
{
    if (s->dbip == DBI_NULL)
	return;

    if (not_state(s, ST_S_EDIT, "Solid edit accept"))
	return;

    if (s->dbip->dbi_read_only) {
	sedit_reject(s);
	bu_log("Sorry, this database is READ-ONLY\n");
	pr_prompt(s);
	return;
    }

    if (sedraw > 0)
	sedit(s);

    (void)sedit_apply(s, 1);
}


void
sedit_reject(struct mged_state *s)
{
    if (not_state(s, ST_S_EDIT, "Solid edit reject") || !illump) {
	return;
    }

    if (sedraw > 0)
	sedit(s);

    es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
    es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
    bot_verts[0] = -1;
    bot_verts[1] = -1;
    bot_verts[2] = -1;
    es_ars_crv = (-1);
    es_ars_col = (-1);

    if (lu_copy) {
	struct model *m;
	m = nmg_find_model(&lu_copy->l.magic);
	nmg_km(m);
	lu_copy = (struct loopuse *)NULL;
    }

    /* Restore the original solid everywhere */
    {
	struct display_list *gdlp;
	struct display_list *next_gdlp;
	struct bv_scene_obj *sp;
	if (!illump->s_u_data)
	    return;
	struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

	gdlp = BU_LIST_NEXT(display_list, s->gedp->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, s->gedp->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (!sp->s_u_data)
		    continue;
		struct ged_bv_data *bdatas = (struct ged_bv_data *)sp->s_u_data;
		if (LAST_SOLID(bdatas) == LAST_SOLID(bdata))
		    (void)replot_original_solid(s, sp);
	    }

	    gdlp = next_gdlp;
	}
    }

    menu_state->ms_flag = 0;
    movedir = 0;
    mged_set_edflag(s, -1);
    s->edit_state.e_edclass = EDIT_CLASS_NULL;

    rt_db_free_internal(&s->edit_state.es_int);
}

int
mged_param(struct mged_state *s, Tcl_Interp *interp, int argc, fastf_t *argvect)
{
    int i;

    CHECK_DBI_NULL;

    if (s->edit_state.edit_flag <= 0) {
	Tcl_AppendResult(interp,
			 "A solid editor option not selected\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    s->edit_state.e_inpara = 0;
    for (i = 0; i < argc; i++) {
	s->edit_state.e_para[ s->edit_state.e_inpara++ ] = argvect[i];
    }

    sedit(s);

    if (SEDIT_TRAN) {
	vect_t diff;
	fastf_t inv_Viewscale = 1/view_state->vs_gvp->gv_scale;

	VSUB2(diff, s->edit_state.e_para, s->edit_state.e_axes_pos);
	VSCALE(s->edit_state.edit_absolute_model_tran, diff, inv_Viewscale);
	VMOVE(s->edit_state.last_edit_absolute_model_tran, s->edit_state.edit_absolute_model_tran);
    } else if (SEDIT_ROTATE) {
	VMOVE(s->edit_state.edit_absolute_model_rotate, s->edit_state.e_para);
    } else if (SEDIT_SCALE) {
	s->edit_state.edit_absolute_scale = acc_sc_sol - 1.0;
	if (s->edit_state.edit_absolute_scale > 0)
	    s->edit_state.edit_absolute_scale /= 3.0;
    }
    return TCL_OK;
}


/* Input parameter editing changes from keyboard */
/* Format: p dx [dy dz]		*/
int
f_param(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int i;
    vect_t argvect;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 4 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help p");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    for (i = 1; i < argc && i <= 3; i++) {
	argvect[i-1] = atof(argv[i]);
    }

    return mged_param(s, interp, argc-1, argvect);
}


/*
 * Put labels on the vertices of the currently edited solid.
 * XXX This really should use import/export interface! Or be part of it.
 */
void
label_edited_solid(
    struct mged_state *s,
    int *num_lines, // NOTE - currently used only for BOTs
    point_t *lines, // NOTE - currently used only for BOTs
    struct rt_point_labels pl[],
    int max_pl,
    const mat_t xform,
    struct rt_db_internal *ip)
{
    int npl = 0;

    // TODO - is es_int the same as ip here?  If not, why not?
    RT_CK_DB_INTERNAL(ip);

    // First, see if we have an edit-aware labeling method.  If we do, use it.
    if (MGED_OBJ[ip->idb_type].ft_labels) {
	(*MGED_OBJ[ip->idb_type].ft_labels)(num_lines, lines, pl, max_pl, xform, &s->edit_state.es_int, &s->tol.tol);
	return;
    }
    // If there is no editing-aware labeling, use standard librt labels
    if (OBJ[ip->idb_type].ft_labels) {
	npl = OBJ[ip->idb_type].ft_labels(pl, max_pl, xform, &s->edit_state.es_int, &s->tol.tol);
	return;
    }

    // If we have nothing, NULL the string
    pl[npl].str[0] = '\0';
}


/* -------------------------------- */

int
f_keypoint(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    CHECK_DBI_NULL;

    if (argc < 1 || 4 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help keypoint");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((s->edit_state.global_editing_state != ST_S_EDIT) && (s->edit_state.global_editing_state != ST_O_EDIT)) {
	state_err(s, "keypoint assignment");
	return TCL_ERROR;
    }

    switch (--argc) {
	case 0:
	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
		point_t key;

		VSCALE(key, s->edit_state.e_keypoint, s->dbip->dbi_base2local);
		bu_vls_printf(&tmp_vls, "%s (%g, %g, %g)\n", s->edit_state.e_keytag, V3ARGS(key));
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
	    }

	    break;
	case 3:
	    VSET(s->edit_state.e_keypoint,
		 atof(argv[1]) * s->dbip->dbi_local2base,
		 atof(argv[2]) * s->dbip->dbi_local2base,
		 atof(argv[3]) * s->dbip->dbi_local2base);
	    s->edit_state.e_keytag = "user-specified";
	    s->edit_state.e_keyfixed = 1;
	    break;
	case 1:
	    if (BU_STR_EQUAL(argv[1], "reset")) {
		s->edit_state.e_keytag = "";
		s->edit_state.e_keyfixed = 0;
		get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag,
				   &s->edit_state.es_int, s->edit_state.e_mat);
		break;
	    }
	    /* fall through */
	default:
	    Tcl_AppendResult(interp, "Usage: 'keypoint [<x y z> | reset]'\n", (char *)NULL);
	    return TCL_ERROR;
    }

    view_state->vs_flag = 1;
    return TCL_OK;
}


int
f_get_sedit_menus(ClientData clientData, Tcl_Interp *interp, int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    struct rt_db_internal *ip = &s->edit_state.es_int;

    struct menu_item *mip = (struct menu_item *)NULL;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (s->edit_state.global_editing_state != ST_S_EDIT)
	return TCL_ERROR;

    switch (s->edit_state.es_int.idb_type) {
	case ID_ARB8:
	    {
		struct bu_vls vls2 = BU_VLS_INIT_ZERO;

		/* title */
		bu_vls_printf(&vls, "{{ARB MENU} {}}");

		/* build "move edge" menu */
		mip = which_menu[s->edit_state.e_type-4];
		/* submenu title */
		bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
		for (++mip; mip->menu_func != NULL; ++mip)
		    bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

		bu_vls_printf(&vls, " {{%s} {%s}}", cntrl_menu[1].menu_string, bu_vls_addr(&vls2));
		bu_vls_trunc(&vls2, 0);

		/* build "move face" menu */
		mip = which_menu[s->edit_state.e_type+1];
		/* submenu title */
		bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
		for (++mip; mip->menu_func != NULL; ++mip)
		    bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

		bu_vls_printf(&vls, " {{%s} {%s}}", cntrl_menu[2].menu_string, bu_vls_addr(&vls2));
		bu_vls_trunc(&vls2, 0);

		/* build "rotate face" menu */
		mip = which_menu[s->edit_state.e_type+6];
		/* submenu title */
		bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
		for (++mip; mip->menu_func != NULL; ++mip)
		    bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

		bu_vls_printf(&vls, " {{%s} {%s}}", cntrl_menu[3].menu_string, bu_vls_addr(&vls2));
		bu_vls_free(&vls2);
	    }

	    break;
	case ID_ARS:
	    {
		struct bu_vls vls2 = BU_VLS_INIT_ZERO;

		/* build ARS PICK MENU Tcl list */

		mip = ars_pick_menu;
		/* title */
		bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);
		for (++mip; mip->menu_func != NULL; ++mip)
		    bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

		mip = ars_menu;
		/* title */
		bu_vls_printf(&vls, " {{%s} {}}", mip->menu_string);

		/* pick vertex menu */
		bu_vls_printf(&vls, " {{%s} {%s}}", (++mip)->menu_string,
			      bu_vls_addr(&vls2));

		for (++mip; mip->menu_func != NULL; ++mip)
		    bu_vls_printf(&vls, " {{%s} {}}", mip->menu_string);

		bu_vls_free(&vls2);
	    }

	    break;
	default:
	    if (MGED_OBJ[ip->idb_type].ft_menu_item)
		mip = (*MGED_OBJ[ip->idb_type].ft_menu_item)(&s->tol.tol);

	    if (mip == (struct menu_item *)NULL)
		break;

	    /* title */
	    bu_vls_printf(&vls, " {{%s} {}}", mip->menu_string);

	    for (++mip; mip->menu_func != NULL; ++mip)
		bu_vls_printf(&vls, " {{%s} {}}", mip->menu_string);

	    break;
    }

    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)0);
    bu_vls_free(&vls);

    return TCL_OK;
}


int
f_get_sedit(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int status;
    struct rt_db_internal ces_int;
    Tcl_Obj *pto;
    Tcl_Obj *pnto;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel get_sed");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (s->edit_state.global_editing_state != ST_S_EDIT || !illump) {
	Tcl_AppendResult(interp, "get_sed: must be in solid edit state", (char *)0);
	return TCL_ERROR;
    }

    if (illump || !illump->s_u_data)
	return TCL_ERROR;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

    if (argc == 1) {
	struct bu_vls logstr = BU_VLS_INIT_ZERO;

	/* get solid type and parameters */
	RT_CK_DB_INTERNAL(&s->edit_state.es_int);
	RT_CK_FUNCTAB(s->edit_state.es_int.idb_meth);
	status = s->edit_state.es_int.idb_meth->ft_get(&logstr, &s->edit_state.es_int, (char *)0);
	Tcl_AppendResult(interp, bu_vls_addr(&logstr), (char *)0);
	pto = Tcl_GetObjResult(interp);

	bu_vls_free(&logstr);

	pnto = Tcl_NewObj();
	/* insert solid name, type and parameters */
	Tcl_AppendStringsToObj(pnto, LAST_SOLID(bdata)->d_namep, " ",
			       Tcl_GetStringFromObj(pto, (int *)0), (char *)0);

	Tcl_SetObjResult(interp, pnto);
	return status;
    }

    if (argv[1][0] != '-' || argv[1][1] != 'c') {
	Tcl_AppendResult(interp, "Usage: get_sed [-c]", (char *)0);
	return TCL_ERROR;
    }

    /* apply matrices along the path */
    RT_DB_INTERNAL_INIT(&ces_int);
    transform_editing_solid(s, &ces_int, s->edit_state.e_mat, &s->edit_state.es_int, 0);

    /* get solid type and parameters */
    RT_CK_DB_INTERNAL(&ces_int);
    RT_CK_FUNCTAB(ces_int.idb_meth);
    {
	struct bu_vls logstr = BU_VLS_INIT_ZERO;

	status = ces_int.idb_meth->ft_get(&logstr, &ces_int, (char *)0);
	Tcl_AppendResult(interp, bu_vls_addr(&logstr), (char *)0);
	bu_vls_free(&logstr);
    }
    pto = Tcl_GetObjResult(interp);

    pnto = Tcl_NewObj();
    /* insert full pathname */
    {
	struct bu_vls str = BU_VLS_INIT_ZERO;

	db_path_to_vls(&str, &bdata->s_fullpath);
	Tcl_AppendStringsToObj(pnto, bu_vls_addr(&str), NULL);
	bu_vls_free(&str);
    }

    /* insert solid type and parameters */
    Tcl_AppendStringsToObj(pnto, " ", Tcl_GetStringFromObj(pto, (int *)0), (char *)0);

    Tcl_SetObjResult(interp, pnto);

    rt_db_free_internal(&ces_int);

    return status;
}


int
f_put_sedit(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    const struct rt_functab *ftp;
    uint32_t save_magic;
    int context;

    /*XXX needs better argument checking */
    if (argc < 6) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel put_sed");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (s->edit_state.global_editing_state != ST_S_EDIT) {
	Tcl_AppendResult(interp, "put_sed: must be in solid edit state", (char *)0);
	return TCL_ERROR;
    }

    /* look for -c */
    if (argv[1][0] == '-' && argv[1][1] == 'c') {
	context = 1;
	--argc;
	++argv;
    } else
	context = 0;

    ftp = rt_get_functab_by_label(argv[1]);
    if (ftp == NULL ||
	ftp->ft_parsetab == (struct bu_structparse *)NULL) {
	Tcl_AppendResult(interp, "put_sed: ", argv[1],
			 " object type is not supported for db get",
			 (char *)0);
	return TCL_ERROR;
    }

    RT_CK_FUNCTAB(s->edit_state.es_int.idb_meth);
    if (s->edit_state.es_int.idb_meth != ftp) {
	Tcl_AppendResult(interp,
			 "put_sed: idb_meth type mismatch",
			 (char *)0);
    }

    save_magic = *((uint32_t *)s->edit_state.es_int.idb_ptr);
    *((uint32_t *)s->edit_state.es_int.idb_ptr) = ftp->ft_internal_magic;
    {
	int ret;
	struct bu_vls vlog = BU_VLS_INIT_ZERO;

	ret = bu_structparse_argv(&vlog, argc-2, argv+2, ftp->ft_parsetab, (char *)s->edit_state.es_int.idb_ptr, NULL);
	Tcl_AppendResult(interp, bu_vls_addr(&vlog), (char *)NULL);
	bu_vls_free(&vlog);
	if (ret != BRLCAD_OK)
	    return TCL_ERROR;
    }
    *((uint32_t *)s->edit_state.es_int.idb_ptr) = save_magic;

    if (context)
	transform_editing_solid(s, &s->edit_state.es_int, s->edit_state.e_invmat, &s->edit_state.es_int, 1);

    /* must re-calculate the face plane equations for arbs
     * TODO - why do we need to do this here, and not in edarb.c code somewhere? */
    if (s->edit_state.es_int.idb_type == ID_ARB8) {
	struct rt_arb_internal *arb;
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;

	arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	if (rt_arb_calc_planes(&error_msg, arb, s->edit_state.e_type, es_peqn, &s->tol.tol) < 0)
	    Tcl_AppendResult(interp, bu_vls_addr(&error_msg), (char *)0);
	bu_vls_free(&error_msg);
    }

    if (!s->edit_state.e_keyfixed)
	get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag, &s->edit_state.es_int, s->edit_state.e_mat);

    set_e_axes_pos(s, 0);
    replot_editing_solid(s);

    return TCL_OK;
}


int
f_sedit_reset(ClientData clientData, Tcl_Interp *interp, int argc, const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (s->edit_state.global_editing_state != ST_S_EDIT || !illump)
	return TCL_ERROR;

    if (argc != 1) {
	bu_vls_printf(&vls, "helpdevel sed_reset");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* free old copy */
    rt_db_free_internal(&s->edit_state.es_int);

    /* reset */
    es_pipe_pnt = (struct wdb_pipe_pnt *)NULL;
    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL;
    es_s = (struct shell *)NULL;
    es_eu = (struct edgeuse *)NULL;

    /* read in a fresh copy */
    if (!illump || !illump->s_u_data)
	return TCL_ERROR;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    if (rt_db_get_internal(&s->edit_state.es_int, LAST_SOLID(bdata),
			   s->dbip, NULL, &rt_uniresource) < 0) {
	if (bdata->s_fullpath.fp_len > 0) {
	    Tcl_AppendResult(interp, "sedit_reset(",
		    LAST_SOLID(bdata)->d_namep,
		    "):  solid import failure\n", (char *)NULL);
	} else {
	    Tcl_AppendResult(interp, "sedit_reset(NULL):  solid import failure\n", (char *)NULL);

	}
	return TCL_ERROR;				/* FAIL */
    }
    RT_CK_DB_INTERNAL(&s->edit_state.es_int);
    replot_editing_solid(s);

    /* Establish initial keypoint */
    s->edit_state.e_keytag = "";
    get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag, &s->edit_state.es_int, s->edit_state.e_mat);

    /* Reset relevant variables */
    MAT_IDN(acc_rot_sol);
    VSETALL(s->edit_state.edit_absolute_model_rotate, 0.0);
    VSETALL(s->edit_state.edit_absolute_object_rotate, 0.0);
    VSETALL(s->edit_state.edit_absolute_view_rotate, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_model_rotate, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_object_rotate, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_view_rotate, 0.0);
    VSETALL(s->edit_state.edit_absolute_model_tran, 0.0);
    VSETALL(s->edit_state.edit_absolute_view_tran, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_model_tran, 0.0);
    VSETALL(s->edit_state.last_edit_absolute_view_tran, 0.0);
    s->edit_state.edit_absolute_scale = 0.0;
    acc_sc_sol = 1.0;
    VSETALL(s->edit_state.edit_rate_model_rotate, 0.0);
    VSETALL(s->edit_state.edit_rate_object_rotate, 0.0);
    VSETALL(s->edit_state.edit_rate_view_rotate, 0.0);
    VSETALL(s->edit_state.edit_rate_model_tran, 0.0);
    VSETALL(s->edit_state.edit_rate_view_tran, 0.0);

    set_e_axes_pos(s, 1);
    update_views = 1;
    dm_set_dirty(DMP, 1);

    /* active edit callback */
    bu_vls_printf(&vls, "active_edit_callback");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


int
f_sedit_apply(ClientData clientData, Tcl_Interp *interp, int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls vls = BU_VLS_INIT_ZERO;

    CHECK_DBI_NULL;

    if (not_state(s, ST_S_EDIT, "Solid edit apply")) {
	return TCL_ERROR;
    }

    if (sedraw > 0)
	sedit(s);

    init_sedit_vars(s);
    (void)sedit_apply(s, 0);

    /* active edit callback */
    bu_vls_printf(&vls, "active_edit_callback");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


int
f_oedit_reset(ClientData clientData, Tcl_Interp *interp, int argc, const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (s->edit_state.global_editing_state != ST_O_EDIT)
	return TCL_ERROR;

    if (argc != 1) {
	bu_vls_printf(&vls, "helpdevel oed_reset");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    oedit_reject(s);
    init_oedit_guts(s);

    new_edit_mats(s);
    update_views = 1;
    dm_set_dirty(DMP, 1);

    /* active edit callback */
    bu_vls_printf(&vls, "active_edit_callback");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


int
f_oedit_apply(ClientData clientData, Tcl_Interp *interp, int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls vls = BU_VLS_INIT_ZERO;
    const char *strp = "";

    CHECK_DBI_NULL;
    oedit_apply(s, UP); /* apply changes, but continue editing */

    if (!illump->s_u_data)
	return TCL_ERROR;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

    /* Save aggregate path matrix */
    MAT_IDN(s->edit_state.e_mat);
    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, s->edit_state.e_mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(s->edit_state.e_invmat, s->edit_state.e_mat);

    get_solid_keypoint(s, &s->edit_state.e_keypoint, &strp, &s->edit_state.es_int, s->edit_state.e_mat);
    init_oedit_vars(s);
    new_edit_mats(s);
    update_views = 1;
    dm_set_dirty(DMP, 1);

    /* active edit callback */
    bu_vls_printf(&vls, "active_edit_callback");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

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
