/*                       C H G V I E W . C
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
/** @file mged/chgview.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#include "vmath.h"
#include "bu/getopt.h"
#include "bn.h"
#include "raytrace.h"
#include "nmg.h"
#include "tclcad.h"
#include "./sedit.h"
#include "./mged.h"
#include "./mged_dm.h"
#include "./cmd.h"


extern void mged_color_soltab(struct mged_state *s);

int
mged_erot(struct mged_state *s,
	char coords,
	  char rotate_about,
	  mat_t newrot)
{
    int save_edflag;
    mat_t temp1, temp2;

    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    switch (coords) {
	case 'm':
	    break;
	case 'o':
	    bn_mat_inv(temp1, s->s_edit->acc_rot_sol);

	    /* transform into object rotations */
	    bn_mat_mul(temp2, s->s_edit->acc_rot_sol, newrot);
	    bn_mat_mul(newrot, temp2, temp1);
	    break;
	case 'v':
	    bn_mat_inv(temp1, view_state->vs_gvp->gv_rotation);

	    /* transform into model rotations */
	    bn_mat_mul(temp2, temp1, newrot);
	    bn_mat_mul(newrot, temp2, view_state->vs_gvp->gv_rotation);
	    break;
    }

    if (s->global_editing_state == ST_S_EDIT) {
	char save_rotate_about;

	save_rotate_about = mged_variables->mv_rotate_about;
	mged_variables->mv_rotate_about = rotate_about;

	save_edflag = s->s_edit->edit_flag;

	if (!SEDIT_ROTATE) {
	    s->s_edit->edit_flag = SROT;
	}

	s->s_edit->e_inpara = 0;
	MAT_COPY(s->s_edit->incr_change, newrot);
	bn_mat_mul2(s->s_edit->incr_change, s->s_edit->acc_rot_sol);
	sedit(s);

	mged_variables->mv_rotate_about = save_rotate_about;
	s->s_edit->edit_flag = save_edflag;
    } else {
	point_t point;
	vect_t work;

	bn_mat_mul2(newrot, s->s_edit->acc_rot_sol);

	/* find point for rotation to take place wrt */
	switch (rotate_about) {
	    case 'v':       /* View Center */
		VSET(work, 0.0, 0.0, 0.0);
		MAT4X3PNT(point, view_state->vs_gvp->gv_view2model, work);
		break;
	    case 'e':       /* Eye */
		VSET(work, 0.0, 0.0, 1.0);
		MAT4X3PNT(point, view_state->vs_gvp->gv_view2model, work);
		break;
	    case 'm':       /* Model Center */
		VSETALL(point, 0.0);
		break;
	    case 'k':
	    default:
		MAT4X3PNT(point, s->s_edit->model_changes, s->s_edit->e_keypoint);
	}

	/*
	 * Apply newrot to the s->s_edit->model_changes matrix wrt "point"
	 */
	wrt_point(s->s_edit->model_changes, newrot, s->s_edit->model_changes, point);

	new_edit_mats(s);
    }

    return TCL_OK;
}


int
mged_erot_xyz(struct mged_state *s,
	char rotate_about,
	vect_t rvec)
{
    mat_t newrot;

    MAT_IDN(newrot);
    bn_mat_angles(newrot, rvec[X], rvec[Y], rvec[Z]);

    return mged_erot(s, mged_variables->mv_coords, rotate_about, newrot);
}

int
mged_mtran(struct mged_state *s, const vect_t tvec)
{
    point_t delta;
    point_t vc, nvc;

    VSCALE(delta, tvec, s->dbip->dbi_local2base);
    MAT_DELTAS_GET_NEG(vc, view_state->vs_gvp->gv_center);
    VSUB2(nvc, vc, delta);
    MAT_DELTAS_VEC_NEG(view_state->vs_gvp->gv_center, nvc);
    new_mats(s);

    /* calculate absolute_tran */
    set_absolute_view_tran(s);

    return TCL_OK;
}


int
mged_otran(struct mged_state *s, const vect_t tvec)
{
    vect_t work = VINIT_ZERO;

    if (s->global_editing_state == ST_S_EDIT || s->global_editing_state == ST_O_EDIT) {
	/* apply s->s_edit->acc_rot_sol to tvec */
	MAT4X3PNT(work, s->s_edit->acc_rot_sol, tvec);
    }

    return mged_mtran(s, work);
}

int
mged_etran(struct mged_state *s,
	char coords,
	vect_t tvec)
{
    point_t p2;
    int save_edflag;
    point_t delta;
    point_t vcenter;
    point_t work;
    mat_t xlatemat;

    /* compute delta */
    switch (coords) {
	case 'm':
	    VSCALE(delta, tvec, s->dbip->dbi_local2base);
	    break;
	case 'o':
	    VSCALE(p2, tvec, s->dbip->dbi_local2base);
	    MAT4X3PNT(delta, s->s_edit->acc_rot_sol, p2);
	    break;
	case 'v':
	default:
	    VSCALE(p2, tvec, s->dbip->dbi_local2base / view_state->vs_gvp->gv_scale);
	    MAT4X3PNT(work, view_state->vs_gvp->gv_view2model, p2);
	    MAT_DELTAS_GET_NEG(vcenter, view_state->vs_gvp->gv_center);
	    VSUB2(delta, work, vcenter);

	    break;
    }

    if (s->global_editing_state == ST_S_EDIT) {
	s->s_edit->e_keyfixed = 0;
	get_solid_keypoint(s, s->s_edit->e_keypoint, &s->s_edit->e_keytag,
			   &s->s_edit->es_int, s->s_edit->e_mat);
	save_edflag = s->s_edit->edit_flag;

	if (!SEDIT_TRAN) {
	    s->s_edit->edit_flag = STRANS;
	}

	VADD2(s->s_edit->e_para, delta, s->s_edit->curr_e_axes_pos);
	s->s_edit->e_inpara = 3;
	sedit(s);
	s->s_edit->edit_flag = save_edflag;
    } else {
	MAT_IDN(xlatemat);
	MAT_DELTAS_VEC(xlatemat, delta);
	bn_mat_mul2(xlatemat, s->s_edit->model_changes);

	new_edit_mats(s);
	s->update_views = 1;
	dm_set_dirty(DMP, 1);
    }

    return TCL_OK;
}



int
mged_vtran(struct mged_state *s, const vect_t tvec)
{
    vect_t tt;
    point_t delta;
    point_t work;
    point_t vc, nvc;

    VSCALE(tt, tvec, s->dbip->dbi_local2base / view_state->vs_gvp->gv_scale);
    MAT4X3PNT(work, view_state->vs_gvp->gv_view2model, tt);
    MAT_DELTAS_GET_NEG(vc, view_state->vs_gvp->gv_center);
    VSUB2(delta, work, vc);
    VSUB2(nvc, vc, delta);
    MAT_DELTAS_VEC_NEG(view_state->vs_gvp->gv_center, nvc);

    new_mats(s);

    /* calculate absolute_model_tran */
    set_absolute_model_tran(s);

    return TCL_OK;
}

int
mged_vrot(struct mged_state *s, char origin, fastf_t *newrot)
{
    mat_t newinv;

    bn_mat_inv(newinv, newrot);

    if (origin != 'v') {
	point_t rot_pt;
	point_t new_origin;
	mat_t viewchg, viewchginv;
	point_t new_cent_view;
	point_t new_cent_model;

	if (origin == 'e') {
	    /* "VR driver" method: rotate around "eye" point (0, 0, 1) viewspace */
	    VSET(rot_pt, 0.0, 0.0, 1.0);		/* point to rotate around */
	} else if (origin == 'k' && s->global_editing_state == ST_S_EDIT) {
	    /* rotate around keypoint */
	    MAT4X3PNT(rot_pt, view_state->vs_gvp->gv_model2view, s->s_edit->curr_e_axes_pos);
	} else if (origin == 'k' && s->global_editing_state == ST_O_EDIT) {
	    point_t kpWmc;

	    MAT4X3PNT(kpWmc, s->s_edit->model_changes, s->s_edit->e_keypoint);
	    MAT4X3PNT(rot_pt, view_state->vs_gvp->gv_model2view, kpWmc);
	} else {
	    /* rotate around model center (0, 0, 0) */
	    VSET(new_origin, 0.0, 0.0, 0.0);
	    MAT4X3PNT(rot_pt, view_state->vs_gvp->gv_model2view, new_origin);  /* point to rotate around */
	}

	bn_mat_xform_about_pnt(viewchg, newrot, rot_pt);
	bn_mat_inv(viewchginv, viewchg);

	/* Convert origin in new (viewchg) coords back to old view coords */
	VSET(new_origin, 0.0, 0.0, 0.0);
	MAT4X3PNT(new_cent_view, viewchginv, new_origin);
	MAT4X3PNT(new_cent_model, view_state->vs_gvp->gv_view2model, new_cent_view);
	MAT_DELTAS_VEC_NEG(view_state->vs_gvp->gv_center, new_cent_model);

	/* XXX This should probably capture the translation too */
	/* XXX I think the only consumer of ModelDelta is the predictor frame */
	wrt_view(s, view_state->vs_ModelDelta, newinv, view_state->vs_ModelDelta);		/* pure rotation */
    } else
	/* Traditional method:  rotate around view center (0, 0, 0) viewspace */
    {
	wrt_view(s, view_state->vs_ModelDelta, newinv, view_state->vs_ModelDelta);
    }

    /* Update the rotation component of the model2view matrix */
    bn_mat_mul2(newrot, view_state->vs_gvp->gv_rotation); /* pure rotation */
    new_mats(s);

    set_absolute_tran(s);

    return TCL_OK;
}

int
mged_vrot_xyz(struct mged_state *s,
	      char origin,
	      char coords,
	      vect_t rvec)
{
    mat_t newrot;
    mat_t temp1, temp2;

    MAT_IDN(newrot);
    bn_mat_angles(newrot, rvec[X], rvec[Y], rvec[Z]);

    if (coords == 'm') {
	/* transform model rotations into view rotations */
	bn_mat_inv(temp1, view_state->vs_gvp->gv_rotation);
	bn_mat_mul(temp2, view_state->vs_gvp->gv_rotation, newrot);
	bn_mat_mul(newrot, temp2, temp1);
    } else if ((s->global_editing_state == ST_S_EDIT || s->global_editing_state == ST_O_EDIT) && coords == 'o') {
	/* first, transform object rotations into model rotations */
	bn_mat_inv(temp1, s->s_edit->acc_rot_sol);
	bn_mat_mul(temp2, s->s_edit->acc_rot_sol, newrot);
	bn_mat_mul(newrot, temp2, temp1);

	/* now transform model rotations into view rotations */
	bn_mat_inv(temp1, view_state->vs_gvp->gv_rotation);
	bn_mat_mul(temp2, view_state->vs_gvp->gv_rotation, newrot);
	bn_mat_mul(newrot, temp2, temp1);
    } /* else assume already view rotations */

    return mged_vrot(s, origin, newrot);
}



int
knob_rot(struct mged_state *s,
	vect_t rvec,
	char origin,
	int model_flag,
	int view_flag,
	int edit_flag)
{
    if (EDIT_ROTATE && ((mged_variables->mv_transform == 'e' &&
			 !view_flag && !model_flag) || edit_flag)) {
	mged_erot_xyz(s, origin, rvec);
    } else if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
	mged_vrot_xyz(s, origin, 'm', rvec);
    } else if (mged_variables->mv_coords == 'o') {
	mged_vrot_xyz(s, origin, 'o', rvec);
    } else {
	mged_vrot_xyz(s, origin, 'v', rvec);
    }

    return TCL_OK;
}

int
knob_tran(struct mged_state *s,
	vect_t tvec,
	int model_flag,
	int view_flag,
	int edit_flag)
{
    if (EDIT_TRAN && ((mged_variables->mv_transform == 'e' &&
		       !view_flag && !model_flag) || edit_flag)) {
	mged_etran(s, mged_variables->mv_coords, tvec);
    } else if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
	mged_mtran(s, tvec);
    } else if (mged_variables->mv_coords == 'o') {
	mged_otran(s, tvec);
    } else {
	mged_vtran(s, tvec);
    }

    return TCL_OK;
}


/* DEBUG -- force view center */
/* Format: C x y z */
int
cmd_center(ClientData clientData,
	   Tcl_Interp *interp,
	   int argc,
	   const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    int ret;
    Tcl_DString ds;

    if (s->gedp == GED_NULL) {
	return TCL_OK;
    }

    ret = ged_exec(s->gedp, argc, (const char **)argv);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret != BRLCAD_OK) {
	return TCL_ERROR;
    }

    if (argc > 1) {
	(void)mged_svbase(s);
	view_state->vs_flag = 1;
    }

    return TCL_OK;
}


void
mged_center(struct mged_state *s, point_t center)
{
    char *av[5];
    char xbuf[32];
    char ybuf[32];
    char zbuf[32];

    if (s->gedp == GED_NULL) {
	return;
    }

    snprintf(xbuf, 32, "%f", center[X]);
    snprintf(ybuf, 32, "%f", center[Y]);
    snprintf(zbuf, 32, "%f", center[Z]);

    av[0] = "center";
    av[1] = xbuf;
    av[2] = ybuf;
    av[3] = zbuf;
    av[4] = (char *)0;
    ged_exec_center(s->gedp, 4, (const char **)av);
    (void)mged_svbase(s);
    view_state->vs_flag = 1;
}


/* DEBUG -- force viewsize */
/* Format: view size */
int
cmd_size(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    int ret;
    Tcl_DString ds;

    if (s->gedp == GED_NULL) {
	return TCL_OK;
    }

    ret = ged_exec(s->gedp, argc, (const char **)argv);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret == BRLCAD_OK) {
	view_state->vs_gvp->gv_a_scale = 1.0 - view_state->vs_gvp->gv_scale / view_state->vs_gvp->gv_i_scale;

	if (view_state->vs_gvp->gv_a_scale < 0.0) {
	    view_state->vs_gvp->gv_a_scale /= 9.0;
	}

	if (!ZERO(view_state->k.tra_v_abs[X])
	    || !ZERO(view_state->k.tra_v_abs[Y])
	    || !ZERO(view_state->k.tra_v_abs[Z]))
	{
	    set_absolute_tran(s);
	}

	if (argc > 1) {
	    view_state->vs_flag = 1;
	}

	return TCL_OK;
    }

    return TCL_ERROR;
}


/*
 * Reset view size and view center so that all solids in the solid table
 * are in view.
 * Caller is responsible for calling new_mats(s).
 */
void
size_reset(struct mged_state *s)
{
    if (s->gedp == GED_NULL)
	return;

    const char *av[1] = {"autoview"};
    ged_exec_autoview(s->gedp, 1, (const char **)av);
    view_state->vs_gvp->gv_i_scale = view_state->vs_gvp->gv_scale;
    view_state->vs_flag = 1;
}


/*
 * B and e commands use this area as common
 */
int
edit_com(struct mged_state *s,
	int argc,
	const char *argv[])
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct mged_dm *save_m_dmp;
    struct cmd_list *save_cmd_list;
    int ret;
    int initial_blank_screen = 1;

    int flag_A_attr = 0;
    int flag_R_noresize = 0;
    int flag_o_nonunique = 1;

    size_t i;
    int last_opt = 0;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    CHECK_DBI_NULL;

    /* Common part of illumination */
    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));

    while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	if (BU_LIST_NON_EMPTY(&gdlp->dl_head_scene_obj)) {
	    initial_blank_screen = 0;
	    break;
	}

	gdlp = next_gdlp;
    }

    /* check args for "-A" (attributes) and "-o" and "-R" */
    bu_vls_strcpy(&vls, argv[0]);

    for (i = 1; i < (size_t)argc; i++) {
	char *ptr_A = NULL;
	char *ptr_o = NULL;
	char *ptr_R = NULL;
	const char *c;

	if (*argv[i] != '-') {
	    break;
	}

	if ((ptr_A = strchr(argv[i], 'A'))) {
	    flag_A_attr = 1;
	}

	if ((ptr_o = strchr(argv[i], 'o'))) {
	    flag_o_nonunique = 2;
	}

	if ((ptr_R = strchr(argv[i], 'R'))) {
	    flag_R_noresize = 1;
	}

	last_opt = i;

	if (!ptr_A && !ptr_o && !ptr_R) {
	    bu_vls_putc(&vls, ' ');
	    bu_vls_strcat(&vls, argv[i]);
	    continue;
	}

	if (strlen(argv[i]) == (size_t)(1 + (ptr_A != NULL) + (ptr_o != NULL) + (ptr_R != NULL))) {
	    /* argv[i] is just a "-A" or "-o" or "-R" */
	    continue;
	}

	/* copy args other than "-A" or "-o" or "-R" */
	bu_vls_putc(&vls, ' ');
	c = argv[i];

	while (*c != '\0') {
	    if (*c != 'A' && *c != 'o' && *c != 'R') {
		bu_vls_putc(&vls, *c);
	    }

	    c++;
	}
    }

    if (flag_A_attr) {
	/* args are attribute name/value pairs */
	struct bu_attribute_value_set avs;
	int max_count = 0;
	int remaining_args = 0;
	int new_argc = 0;
	char **new_argv = NULL;
	struct bu_ptbl *tbl;

	remaining_args = argc - last_opt - 1;

	if (remaining_args < 2 || remaining_args % 2) {
	    bu_log("Error: must have even number of arguments (name/value pairs)\n");
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	bu_avs_init(&avs, (argc - last_opt) / 2, "edit_com avs");
	i = 1;

	while (i < (size_t)argc) {
	    if (*argv[i] == '-') {
		i++;
		continue;
	    }

	    /* this is a name/value pair */
	    if (flag_o_nonunique == 2) {
		bu_avs_add_nonunique(&avs, argv[i], argv[i + 1]);
	    } else {
		bu_avs_add(&avs, argv[i], argv[i + 1]);
	    }

	    i += 2;
	}

	tbl = db_lookup_by_attr(s->dbip, RT_DIR_REGION | RT_DIR_SOLID | RT_DIR_COMB, &avs, flag_o_nonunique);
	bu_avs_free(&avs);

	if (!tbl) {
	    bu_log("Error: db_lookup_by_attr() failed!!\n");
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	if (BU_PTBL_LEN(tbl) < 1) {
	    /* nothing matched, just return */
	    bu_vls_free(&vls);
	    return TCL_OK;
	}

	for (i = 0; i < BU_PTBL_LEN(tbl); i++) {
	    struct directory *dp;

	    dp = (struct directory *)BU_PTBL_GET(tbl, i);
	    bu_vls_putc(&vls, ' ');
	    bu_vls_strcat(&vls, dp->d_namep);
	}

	max_count = BU_PTBL_LEN(tbl) + last_opt + 1;
	bu_ptbl_free(tbl);
	bu_free((char *)tbl, "edit_com ptbl");
	new_argv = (char **)bu_calloc(max_count + 1, sizeof(char *), "edit_com new_argv");
	new_argc = bu_argv_from_string(new_argv, max_count, bu_vls_addr(&vls));

	ret = ged_exec(s->gedp, new_argc, (const char **)new_argv);

	if (ret & BRLCAD_ERROR) {
	    bu_log("ERROR: %s\n", bu_vls_addr(s->gedp->ged_result_str));
	    bu_vls_free(&vls);
	    bu_free((char *)new_argv, "edit_com new_argv");
	    return ret;
	} else if (ret & GED_HELP) {
	    bu_log("%s\n", bu_vls_addr(s->gedp->ged_result_str));
	    bu_vls_free(&vls);
	    bu_free((char *)new_argv, "edit_com new_argv");
	    return ret;
	}

	bu_vls_free(&vls);
	bu_free((char *)new_argv, "edit_com new_argv");
    } else {
	bu_vls_free(&vls);

	ret = ged_exec(s->gedp, argc, (const char **)argv);

	if (ret == BRLCAD_ERROR) {
	    bu_log("ERROR: %s\n", bu_vls_addr(s->gedp->ged_result_str));
	    return TCL_ERROR;
	} else if (ret == GED_HELP) {
	    bu_log("%s\n", bu_vls_addr(s->gedp->ged_result_str));
	    return TCL_OK;
	}
    }

    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    if (flag_R_noresize) {
	/* we're done */
	return TCL_OK;
    }

    /* update and resize the views */

    save_m_dmp = s->mged_curr_dm;
    save_cmd_list = curr_cmd_list;
    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	int non_empty = 0; /* start out empty */

	set_curr_dm(s, m_dmp);

	if (s->mged_curr_dm->dm_tie) {
	    curr_cmd_list = s->mged_curr_dm->dm_tie;
	} else {
	    curr_cmd_list = &head_cmd_list;
	}

	s->gedp->ged_gvp = view_state->vs_gvp;

	gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));

	while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    if (BU_LIST_NON_EMPTY(&gdlp->dl_head_scene_obj)) {
		non_empty = 1;
		break;
	    }

	    gdlp = next_gdlp;
	}

	/* If we went from blank screen to non-blank, resize */
	if (mged_variables->mv_autosize && initial_blank_screen && non_empty) {
	    struct view_ring *vrp;
	    char *av[2];

	    av[0] = "autoview";
	    av[1] = (char *)0;
	    ged_exec_autoview(s->gedp, 1, (const char **)av);

	    (void)mged_svbase(s);

	    for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l)) {
		vrp->vr_scale = view_state->vs_gvp->gv_scale;
	    }
	}
    }

    set_curr_dm(s, save_m_dmp);
    curr_cmd_list = save_cmd_list;
    s->gedp->ged_gvp = view_state->vs_gvp;

    return TCL_OK;
}

int
cmd_autoview(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    struct mged_dm *save_m_dmp;
    struct cmd_list *save_cmd_list;

    if (argc > 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_log("Unexpected parameter [%s]\n", argv[2]);
	bu_vls_printf(&vls, "help autoview");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (s->gedp == GED_NULL) {
	return TCL_OK;
    }

    save_m_dmp = s->mged_curr_dm;
    save_cmd_list = curr_cmd_list;
    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	struct view_ring *vrp;

	set_curr_dm(s, m_dmp);

	if (s->mged_curr_dm->dm_tie) {
	    curr_cmd_list = s->mged_curr_dm->dm_tie;
	} else {
	    curr_cmd_list = &head_cmd_list;
	}

	s->gedp->ged_gvp = view_state->vs_gvp;

	{
	    int ac = 1;
	    const char *av[3];

	    av[0] = "autoview";
	    av[1] = NULL;
	    av[2] = NULL;

	    if (argc > 1) {
		av[1] = argv[1];
		ac = 2;
	    }

	    ged_exec_autoview(s->gedp, ac, (const char **)av);
	    view_state->vs_flag = 1;
	}
	(void)mged_svbase(s);

	for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l)) {
	    vrp->vr_scale = view_state->vs_gvp->gv_scale;
	}
    }
    set_curr_dm(s, save_m_dmp);
    curr_cmd_list = save_cmd_list;
    s->gedp->ged_gvp = view_state->vs_gvp;

    return TCL_OK;
}


void
solid_list_callback(struct mged_state *s)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    Tcl_Obj *save_obj;

    /* save result */
    save_obj = Tcl_GetObjResult(s->interp);
    Tcl_IncrRefCount(save_obj);

    bu_vls_strcpy(&vls, "solid_list_callback");
    (void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    /* restore result */
    Tcl_SetObjResult(s->interp, save_obj);
    Tcl_DecrRefCount(save_obj);
}


/*
 * Display-manager specific "hardware register" debugging.
 */
int
f_regdebug(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    static int regdebug = 0;
    static char debug_str[10];

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help regdebug");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (argc == 1) {
	regdebug = !regdebug;    /* toggle */
    } else {
	regdebug = atoi(argv[1]);
    }

    sprintf(debug_str, "%d", regdebug);

    Tcl_AppendResult(interp, "regdebug=", debug_str, "\n", (char *)NULL);

    dm_set_debug(DMP, regdebug);

    return TCL_OK;
}


/* ZAP the display -- everything dropped */
/* Format: Z */
int
cmd_zap(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    void (*tmp_callback)(void *, unsigned int, int) = s->gedp->ged_destroy_vlist_callback;
    const char *av[1] = {"zap"};

    CHECK_DBI_NULL;

    s->update_views = 1;
    dm_set_dirty(DMP, 1);
    s->gedp->ged_destroy_vlist_callback = freeDListsAll;

    /* FIRST, reject any editing in progress */
    if (s->global_editing_state != ST_VIEW) {
	button(s, BE_REJECT);
    }

    ged_exec_zap(s->gedp, 1, (const char **)av);

    (void)chg_state(s, s->global_editing_state, s->global_editing_state, "zap");
    solid_list_callback(s);

    s->gedp->ged_destroy_vlist_callback = tmp_callback;

    return TCL_OK;
}


static void
mged_bn_mat_print(Tcl_Interp *interp,
		 const char *title,
		 const mat_t m)
{
    char obuf[1024];	/* snprintf may be non-PARALLEL */

    bn_mat_print_guts(title, m, obuf, 1024);
    Tcl_AppendResult(interp, obuf, "\n", (char *)NULL);
}

int
f_status(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls vls = BU_VLS_INIT_ZERO;

    CHECK_DBI_NULL;

    if (argc < 1 || 2 < argc) {
	bu_vls_printf(&vls, "help status");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc == 1) {
	bu_vls_printf(&vls, "s->global_editing_state=%s, ", state_str[s->global_editing_state]);
	bu_vls_printf(&vls, "Viewscale=%f (%f mm)\n",
		      view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local, view_state->vs_gvp->gv_scale);
	bu_vls_printf(&vls, "s->dbip->dbi_base2local=%f\n", s->dbip->dbi_base2local);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	mged_bn_mat_print(interp, "toViewcenter", view_state->vs_gvp->gv_center);
	mged_bn_mat_print(interp, "Viewrot", view_state->vs_gvp->gv_rotation);
	mged_bn_mat_print(interp, "model2view", view_state->vs_gvp->gv_model2view);
	mged_bn_mat_print(interp, "view2model", view_state->vs_gvp->gv_view2model);

	if (s->global_editing_state != ST_VIEW) {
	    mged_bn_mat_print(interp, "model2objview", view_state->vs_model2objview);
	    mged_bn_mat_print(interp, "objview2model", view_state->vs_objview2model);
	}

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "state")) {
	Tcl_AppendResult(interp, state_str[s->global_editing_state], (char *)NULL);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "Viewscale")) {
	bu_vls_printf(&vls, "%f", view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "s->dbip->dbi_base2local")) {
	bu_vls_printf(&vls, "%f", s->dbip->dbi_base2local);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "s->dbip->dbi_local2base")) {
	bu_vls_printf(&vls, "%f", s->dbip->dbi_local2base);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "toViewcenter")) {
	mged_bn_mat_print(interp, "toViewcenter", view_state->vs_gvp->gv_center);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "Viewrot")) {
	mged_bn_mat_print(interp, "Viewrot", view_state->vs_gvp->gv_rotation);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "model2view")) {
	mged_bn_mat_print(interp, "model2view", view_state->vs_gvp->gv_model2view);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "view2model")) {
	mged_bn_mat_print(interp, "view2model", view_state->vs_gvp->gv_view2model);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "model2objview")) {
	mged_bn_mat_print(interp, "model2objview", view_state->vs_model2objview);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "objview2model")) {
	mged_bn_mat_print(interp, "objview2model", view_state->vs_objview2model);
	return TCL_OK;
    }

    bu_vls_printf(&vls, "help status");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    if (BU_STR_EQUAL(argv[1], "help")) {
	return TCL_OK;
    }

    return TCL_ERROR;
}


int
f_refresh(ClientData clientData, Tcl_Interp *interp, int argc, const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    if (argc < 1 || 1 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help refresh");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    view_state->vs_flag = 1;
    return TCL_OK;
}


static char **path_parse (char *path);

/* Illuminate the named object */
int
f_ill(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct directory *dp;
    struct bv_scene_obj *sp;
    struct bv_scene_obj *lastfound = NULL;
    int i, j;
    int nmatch;
    int c;
    int ri = 0;
    size_t nm_pieces;
    int illum_only = 0;
    int exact = 0;
    char **path_piece = 0;
    char *mged_basename;
    char *sname;

    int early_out = 0;

    char **nargv;
    char **orig_nargv;
    struct bu_vls vlsargv = BU_VLS_INIT_ZERO;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    CHECK_DBI_NULL;

    if (argc < 2 || 6 < argc) {
	bu_vls_printf(&vls, "help ill");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_from_argv(&vlsargv, argc, argv);
    nargv = (char **)bu_calloc(argc + 1, sizeof(char *), "calloc f_ill nargv");
    orig_nargv = nargv;
    c = bu_argv_from_string(nargv, argc, bu_vls_addr(&vlsargv));

    if (c != argc) {
	Tcl_AppendResult(interp, "ERROR: unable to processes command arguments for f_ill()\n", (char *)NULL);

	bu_free(orig_nargv, "free f_ill nargv");
	bu_vls_free(&vlsargv);

	return TCL_ERROR;
    }

    bu_optind = 1;

    while ((c = bu_getopt(argc, nargv, "ei:nh?")) != -1) {
	switch (c) {
	    case 'e':
		exact = 1;
		break;
	    case 'n':
		illum_only = 1;
		break;
	    case 'i':
		sscanf(bu_optarg, "%d", &ri);

		if (ri <= 0) {
		    Tcl_AppendResult(interp,
				     "the reference index must be greater than 0\n",
				     (char *)NULL);

		    early_out = 1;
		    break;
		}
		break;
	    default:
	    {
		bu_vls_printf(&vls, "help ill");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		early_out = 1;
		break;
	    }
	}
    }

    if (early_out) {
	return TCL_ERROR;
    }

    argc -= (bu_optind - 1);
    nargv += (bu_optind - 1);

    if (argc != 2) {
	bu_vls_printf(&vls, "help ill");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	bu_free(orig_nargv, "free f_ill nargv");
	bu_vls_free(&vlsargv);

	return TCL_ERROR;
    }

    if (s->global_editing_state != ST_S_PICK && s->global_editing_state != ST_O_PICK) {
	state_err(s, "keyboard illuminate pick");
	goto bail_out;
    }

    path_piece = path_parse(nargv[1]);

    if (path_piece == NULL) {
	Tcl_AppendResult(interp, "Path parse failed: '", nargv[1], "'\n", (char *)NULL);
	goto bail_out;
    }

    for (nm_pieces = 0; path_piece[nm_pieces] != 0; ++nm_pieces)
	;

    if (nm_pieces == 0) {
	Tcl_AppendResult(interp, "Bad solid path: '", nargv[1], "'\n", (char *)NULL);
	goto bail_out;
    }

    mged_basename = path_piece[nm_pieces - 1];

    if ((dp = db_lookup(s->dbip,  mged_basename, LOOKUP_NOISY)) == RT_DIR_NULL) {
	Tcl_AppendResult(interp, "db_lookup failed for '", mged_basename, "'\n", (char *)NULL);
	goto bail_out;
    }

    nmatch = 0;

    if (!(dp->d_flags & RT_DIR_SOLID)) {
	Tcl_AppendResult(interp, mged_basename, " is not a solid\n", (char *)NULL);
	goto bail_out;
    }

    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));

    while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    int a_new_match;
	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

	    if (exact && nm_pieces != bdata->s_fullpath.fp_len)
		continue;

	    /* XXX Could this make use of db_full_path_subset()? */
	    if (nmatch == 0 || nmatch != ri) {
		i = bdata->s_fullpath.fp_len - 1;

		if (DB_FULL_PATH_GET(&bdata->s_fullpath, i) == dp) {
		    a_new_match = 1;
		    j = nm_pieces - 1;

		    for (; a_new_match && (i >= 0) && (j >= 0); --i, --j) {
			sname = DB_FULL_PATH_GET(&bdata->s_fullpath, i)->d_namep;

			if ((*sname != *(path_piece[j]))
			    || !BU_STR_EQUAL(sname, path_piece[j])) {
			    a_new_match = 0;
			}
		    }

		    if (a_new_match && ((i >= 0) || (j < 0))) {
			lastfound = sp;
			++nmatch;
		    }
		}
	    }

	    sp->s_iflag = DOWN;
	}

	gdlp = next_gdlp;
    }

    if (nmatch == 0) {
	Tcl_AppendResult(interp, nargv[1], " not being displayed\n", (char *)NULL);
	goto bail_out;
    }

    /* preserve same old behavior */
    if (ri == 0) {
	if (nmatch > 1) {
	    Tcl_AppendResult(interp, nargv[1], " multiply referenced\n", (char *)NULL);
	    goto bail_out;
	}
    } else {
	if (ri > nmatch) {
	    Tcl_AppendResult(interp,
			     "the reference index must not be greater than the number of references\n",
			     (char *)NULL);
	    goto bail_out;
	}
    }

    /* Make the specified solid the illuminated solid */
    illump = lastfound;
    illump->s_iflag = UP;

    if (!illum_only) {
	if (s->global_editing_state == ST_O_PICK) {
	    ipathpos = 0;
	    (void)chg_state(s, ST_O_PICK, ST_O_PATH, "Keyboard illuminate");
	} else {
	    /* Check details, Init menu, set state=ST_S_EDIT */
	    init_sedit(s);
	}
    }

    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    if (path_piece) {
	for (i = 0; path_piece[i] != 0; ++i) {
	    bu_free((void *)path_piece[i], "f_ill: char *");
	}

	bu_free((void *) path_piece, "f_ill: char **");
    }

    bu_free(orig_nargv, "free f_ill nargv");
    bu_vls_free(&vlsargv);

    return TCL_OK;

bail_out:

    if (s->global_editing_state != ST_VIEW) {
	bu_vls_printf(&vls, "%s", Tcl_GetStringResult(interp));
	button(s, BE_REJECT);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

	bu_vls_free(&vls);
    }

    if (path_piece) {
	for (i = 0; path_piece[i] != 0; ++i) {
	    bu_free((void *)path_piece[i], "f_ill: char *");
	}

	bu_free((void *) path_piece, "f_ill: char **");
    }

    bu_free(orig_nargv, "free f_ill nargv");
    bu_vls_free(&vlsargv);

    return TCL_ERROR;
}


/* Simulate pressing "Solid Edit" and doing an ILLuminate command */
int
f_sed(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int is_empty = 1;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 5 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help sed");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (not_state(s, ST_VIEW, "keyboard solid edit start")) {
	return TCL_ERROR;
    }

    /* Common part of illumination */
    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));

    while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	if (BU_LIST_NON_EMPTY(&gdlp->dl_head_scene_obj)) {
	    is_empty = 0;
	    break;
	}

	gdlp = next_gdlp;
    }

    if (is_empty) {
	Tcl_AppendResult(interp, "no solids being displayed\n",  (char *)NULL);
	return TCL_ERROR;
    }

    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    button(s, BE_S_ILLUMINATE);	/* To ST_S_PICK */

    argv[0] = "ill";

    /* Illuminate named solid --> ST_S_EDIT */
    if (f_ill(clientData, interp, argc, argv) == TCL_ERROR) {
	Tcl_Obj *save_result;

	save_result = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(save_result);
	button(s, BE_REJECT);
	Tcl_SetObjResult(interp, save_result);
	Tcl_DecrRefCount(save_result);
	return TCL_ERROR;
    }

    return TCL_OK;
}

/* Descriptor for a 3-component rate vector and its flag */
struct rate_vec_desc {
    fastf_t *v;
    int     *flag;
    int      use_small;
};

static void
process_rate_vecs(struct rate_vec_desc *dlist)
{
    for (struct rate_vec_desc *d = dlist; d->v; d++) {
	if (d->use_small) {
	    *(d->flag) = ((fabs(d->v[0]) > SMALL_FASTF) ||
			  (fabs(d->v[1]) > SMALL_FASTF) ||
			  (fabs(d->v[2]) > SMALL_FASTF)) ? 1 : 0;
	} else {
	    *(d->flag) = (!ZERO(d->v[0]) ||
			  !ZERO(d->v[1]) ||
			  !ZERO(d->v[2])) ? 1 : 0;
	}
    }
}

static void
check_nonzero_rates(struct mged_state *s)
{
    /* View (non-edit) vectors */
    struct rate_vec_desc view_vecs[] = {
	{view_state->k.rot_m, &view_state->k.rot_m_flag, 0},
	{view_state->k.tra_m, &view_state->k.tra_m_flag, 0},
	{view_state->k.rot_v, &view_state->k.rot_v_flag, 0},
	{view_state->k.tra_v, &view_state->k.tra_v_flag, 0},
	{NULL, NULL, 0}
    };

    /* Edit vectors */
    struct rate_vec_desc edit_vecs[] = {
	{s->s_edit->k.tra_m, &s->s_edit->k.tra_m_flag, 0},
	{s->s_edit->k.tra_v, &s->s_edit->k.tra_v_flag, 0},
	{s->s_edit->k.rot_m, &s->s_edit->k.rot_m_flag, 0},
	{s->s_edit->k.rot_o, &s->s_edit->k.rot_o_flag, 0},
	{s->s_edit->k.rot_v, &s->s_edit->k.rot_v_flag, 0},
	{NULL, NULL, 0}
    };

    process_rate_vecs(view_vecs);
    process_rate_vecs(edit_vecs);

    /* Scalar scale rates */
    view_state->k.sca_flag = (!ZERO(view_state->k.sca)) ? 1 : 0;
    s->s_edit->k.sca_flag  = (s->s_edit->k.sca > SMALL_FASTF) ? 1 : 0;

    view_state->vs_flag = 1;
}

void
knob_update_rate_vars(struct mged_state *s)
{
    if (!mged_variables->mv_rateknobs) {
	return;
    }
}


int
mged_print_knobvals(struct mged_state *s, Tcl_Interp *interp)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (mged_variables->mv_rateknobs) {
	if (s->es_edclass == EDIT_CLASS_ROTATE && mged_variables->mv_transform == 'e') {
	    bu_vls_printf(&vls, "x = %f\n", s->s_edit->k.rot_m[X]);
	    bu_vls_printf(&vls, "y = %f\n", s->s_edit->k.rot_m[Y]);
	    bu_vls_printf(&vls, "z = %f\n", s->s_edit->k.rot_m[Z]);
	} else {
	    bu_vls_printf(&vls, "x = %f\n", view_state->k.rot_v[X]);
	    bu_vls_printf(&vls, "y = %f\n", view_state->k.rot_v[Y]);
	    bu_vls_printf(&vls, "z = %f\n", view_state->k.rot_v[Z]);
	}

	if (s->es_edclass == EDIT_CLASS_SCALE && mged_variables->mv_transform == 'e') {
	    bu_vls_printf(&vls, "S = %f\n", s->s_edit->k.sca);
	} else {
	    bu_vls_printf(&vls, "S = %f\n", view_state->k.sca);
	}

	if (s->es_edclass == EDIT_CLASS_TRAN && mged_variables->mv_transform == 'e') {
	    bu_vls_printf(&vls, "X = %f\n", s->s_edit->k.tra_m[X]);
	    bu_vls_printf(&vls, "Y = %f\n", s->s_edit->k.tra_m[Y]);
	    bu_vls_printf(&vls, "Z = %f\n", s->s_edit->k.tra_m[Z]);
	} else {
	    bu_vls_printf(&vls, "X = %f\n", view_state->k.tra_v[X]);
	    bu_vls_printf(&vls, "Y = %f\n", view_state->k.tra_v[Y]);
	    bu_vls_printf(&vls, "Z = %f\n", view_state->k.tra_v[Z]);
	}
    } else {
	if (s->es_edclass == EDIT_CLASS_ROTATE && mged_variables->mv_transform == 'e') {
	    bu_vls_printf(&vls, "ax = %f\n", s->s_edit->k.rot_m_abs[X]);
	    bu_vls_printf(&vls, "ay = %f\n", s->s_edit->k.rot_m_abs[Y]);
	    bu_vls_printf(&vls, "az = %f\n", s->s_edit->k.rot_m_abs[Z]);
	} else {
	    bu_vls_printf(&vls, "ax = %f\n", view_state->k.rot_v_abs[X]);
	    bu_vls_printf(&vls, "ay = %f\n", view_state->k.rot_v_abs[Y]);
	    bu_vls_printf(&vls, "az = %f\n", view_state->k.rot_v_abs[Z]);
	}

	if (s->es_edclass == EDIT_CLASS_SCALE && mged_variables->mv_transform == 'e') {
	    bu_vls_printf(&vls, "aS = %f\n", s->s_edit->k.sca_abs);
	} else {
	    bu_vls_printf(&vls, "aS = %f\n", view_state->vs_gvp->gv_a_scale);
	}

	if (s->es_edclass == EDIT_CLASS_TRAN && mged_variables->mv_transform == 'e') {
	    bu_vls_printf(&vls, "aX = %f\n", s->s_edit->k.tra_m_abs[X]);
	    bu_vls_printf(&vls, "aY = %f\n", s->s_edit->k.tra_m_abs[Y]);
	    bu_vls_printf(&vls, "aZ = %f\n", s->s_edit->k.tra_m_abs[Z]);
	} else {
	    bu_vls_printf(&vls, "aX = %f\n", view_state->k.tra_v_abs[X]);
	    bu_vls_printf(&vls, "aY = %f\n", view_state->k.tra_v_abs[Y]);
	    bu_vls_printf(&vls, "aZ = %f\n", view_state->k.tra_v_abs[Z]);
	}
    }

    if (adc_state->adc_draw) {
	bu_vls_printf(&vls, "xadc = %d\n", adc_state->adc_dv_x);
	bu_vls_printf(&vls, "yadc = %d\n", adc_state->adc_dv_y);
	bu_vls_printf(&vls, "ang1 = %d\n", adc_state->adc_dv_a1);
	bu_vls_printf(&vls, "ang2 = %d\n", adc_state->adc_dv_a2);
	bu_vls_printf(&vls, "distadc = %d\n", adc_state->adc_dv_dist);
    }

    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


/* ------------------------------------------------------------------------ */
/* Knob command helpers (rate/absolute rotation, translation, scale)
 *
 * NOTE - future integration can map the helpers here to librt functions (e.g.,
 * bv_knobs_cmd_process style) once stable.
 *
 * Key Semantics:
 *  - 'x','y','z'  : rotation rate (degrees/sec conceptually) in chosen coord
 *  - 'X','Y','Z'  : translation rate (unit/sec conceptually)
 *  - 'S'          : scale rate (view or edit)
 *  - 'ax','ay','az' : absolute rotation
 *  - 'aX','aY','aZ' : absolute translation
 *  - 'aS'         : absolute scale (view or edit)
 *  - zap/zero, calibrate, adc passthroughs
 *  - -i (increment mode), -m / -v coord flags, -e force-edit
 *  - Angle wrap to [-180,180]
 *  - Absolute translation delta calculation identical to legacy
 *
 * Processing steps:
 *  1. Parse global options.
 *  2. Loop token/value pairs; classify each token.
 *  3. Update internal state arrays via concise helper functions.
 *  4. Accumulate rvec/tvec if a rotation or translation needs to be
 *     applied after the parse loop (mirrors original behavior).
 *  5. Apply operations (knob_rot / knob_tran) once per category.
 *  6. Run check_nonzero_rates(s).
 */

typedef enum {
    KNOB_NONE = 0,
    KNOB_ROT_RATE,
    KNOB_ROT_ABS,
    KNOB_TRAN_RATE,
    KNOB_TRAN_ABS,
    KNOB_SCALE_RATE,
    KNOB_SCALE_ABS,
    KNOB_ADC,        /* xadc, yadc, ang1, ang2, distadc */
    KNOB_MISC        /* zap/zero, calibrate */
} knob_kind_t;

typedef enum {
    AXIS_NONE = -1,
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2
} knob_axis_t;

struct knob_token_desc {
    const char *name;
    knob_kind_t kind;
    knob_axis_t axis;   /* For axis-bearing kinds */
};

/* Classification table */
static const struct knob_token_desc knob_tokens[] = {
    {"x",   KNOB_ROT_RATE,  AXIS_X},
    {"y",   KNOB_ROT_RATE,  AXIS_Y},
    {"z",   KNOB_ROT_RATE,  AXIS_Z},
    {"X",   KNOB_TRAN_RATE, AXIS_X},
    {"Y",   KNOB_TRAN_RATE, AXIS_Y},
    {"Z",   KNOB_TRAN_RATE, AXIS_Z},
    {"S",   KNOB_SCALE_RATE, AXIS_NONE},

    {"ax",  KNOB_ROT_ABS,   AXIS_X},
    {"ay",  KNOB_ROT_ABS,   AXIS_Y},
    {"az",  KNOB_ROT_ABS,   AXIS_Z},
    {"aX",  KNOB_TRAN_ABS,  AXIS_X},
    {"aY",  KNOB_TRAN_ABS,  AXIS_Y},
    {"aZ",  KNOB_TRAN_ABS,  AXIS_Z},
    {"aS",  KNOB_SCALE_ABS, AXIS_NONE},

    {"xadc",    KNOB_ADC, AXIS_X},
    {"yadc",    KNOB_ADC, AXIS_Y},
    {"ang1",    KNOB_ADC, AXIS_NONE},
    {"ang2",    KNOB_ADC, AXIS_NONE},
    {"distadc", KNOB_ADC, AXIS_NONE},

    {"zap",      KNOB_MISC, AXIS_NONE},
    {"zero",     KNOB_MISC, AXIS_NONE},
    {"calibrate",KNOB_MISC, AXIS_NONE},

    {NULL, KNOB_NONE, AXIS_NONE}
};

/* Return descriptor or NULL if unrecognized */
static const struct knob_token_desc *
knob_lookup_token(const char *tok)
{
    for (const struct knob_token_desc *d = knob_tokens; d->name; d++) {
	if (BU_STR_EQUAL(d->name, tok))
	    return d;
    }
    return NULL;
}

/* Decide if (token) should be handled as an editing command for this call. */
static int
knob_is_edit_cmd(struct mged_state *s,
	const char *token,
	int model_flag,
	int view_flag,
	int force_edit)
{
    if (!token || !*token)
	return 0;

    /* Rotation and translation editing required mv_transform=='e'
     * and no -v and no -m unless -e supplied.
     * Scale editing ignored the presence of -m (model_flag) and
     * only required mv_transform=='e' and no -v (unless forced.)
     */
    int scale_tok = 0;
    if ((token[0] == 'S' && token[1] == '\0') ||
	    (token[0] == 'a' && token[1] == 'S' && token[2] == '\0')) {
	scale_tok = 1;
    }

    int base_edit_condition;
    if (scale_tok) {
	/* Ignore model_flag for scale to match legacy behavior */
	base_edit_condition = (mged_variables->mv_transform == 'e' && !view_flag);
    } else {
	base_edit_condition = (mged_variables->mv_transform == 'e' && !view_flag && !model_flag);
    }

    int edit_mode_ok = base_edit_condition || force_edit;

    if (!edit_mode_ok)
	return 0;

    if (token[0] == 'a' && token[1] && !token[2]) {
	/* absolute axis form ax/ay/az/aX/aY/aZ/aS */
	char c = token[1];
	if (c == 'x' || c == 'y' || c == 'z')
	    return EDIT_ROTATE ? 1 : 0;
	if (c == 'X' || c == 'Y' || c == 'Z')
	    return EDIT_TRAN ? 1 : 0;
	if (c == 'S')
	    return EDIT_SCALE ? 1 : 0;
	return 0;
    }

    /* single char tokens */
    char c = token[0];
    if (token[1] != '\0')
	return 0;

    switch (c) {
	case 'x': case 'y': case 'z':
	    return EDIT_ROTATE ? 1 : 0;
	case 'X': case 'Y': case 'Z':
	    return EDIT_TRAN ? 1 : 0;
	case 'S':
	    return EDIT_SCALE ? 1 : 0;
	default:
	    return 0;
    }
}

/* Clamp absolute rotation angle in-place to [-180,180] */
static inline fastf_t
wrap_angle_180(fastf_t a)
{
    if (a < -180.0) a += 360.0;
    else if (a > 180.0) a -= 360.0;
    return a;
}

/* --- Rotation (Rate) Update ---
 * incr: if 1, add value; else set
 */
static void
knob_apply_rotation_rate(struct mged_state *s,
	knob_axis_t axis,
	fastf_t val,
	int incr,
	int model_flag,
	int view_flag,
	int edit_this_cmd,
	char origin)
{
    if (axis < 0) return;

    if (edit_this_cmd) {
	/* Choose coordinate system for edit rotation */
	switch (mged_variables->mv_coords) {
	    case 'm':
		if (incr)
		    s->s_edit->k.rot_m[axis] += val;
		else
		    s->s_edit->k.rot_m[axis] = val;
		s->s_edit->k.origin_m = origin;
		s->s_edit->edit_rate_mr_dm = s->mged_curr_dm;
		break;
	    case 'o':
		if (incr)
		    s->s_edit->k.rot_o[axis] += val;
		else
		    s->s_edit->k.rot_o[axis] = val;
		s->s_edit->k.origin_o = origin;
		s->s_edit->edit_rate_or_dm = s->mged_curr_dm;
		break;
	    case 'v':
	    default:
		if (incr)
		    s->s_edit->k.rot_v[axis] += val;
		else
		    s->s_edit->k.rot_v[axis] = val;
		s->s_edit->k.origin_v = origin;
		s->s_edit->edit_rate_vr_dm = s->mged_curr_dm;
		break;
	}
    } else {
	/* View (model or view coords) */
	if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
	    if (incr)
		view_state->k.rot_m[axis] += val;
	    else
		view_state->k.rot_m[axis] = val;
	    view_state->k.origin_m = origin;
	} else {
	    if (incr)
		view_state->k.rot_v[axis] += val;
	    else
		view_state->k.rot_v[axis] = val;
	    view_state->k.origin_v = origin;
	}
    }
}

/* --- Rotation (Absolute) ---
 * For incr: just add to absolute value. For set: compute delta = new - last.
 * We also wrap angles and update last.
 * Returns the delta to place in rvec[axis] when non-incremental (set mode)
 * or the raw value (in incremental)
 */
static fastf_t
knob_apply_rotation_abs(struct mged_state *s,
	knob_axis_t axis,
	fastf_t val,
	int incr,
	int model_flag,
	int view_flag,
	int edit_this_cmd)
{
    if (axis < 0) return 0.0;
    fastf_t delta = 0.0;

    /* Helpers to pick the right pointer sets */
    fastf_t *abs_arr = NULL, *last_arr = NULL;

    if (edit_this_cmd) {
	switch (mged_variables->mv_coords) {
	    case 'm':
		abs_arr = s->s_edit->k.rot_m_abs; last_arr = s->s_edit->k.rot_m_abs_last; break;
	    case 'o':
		abs_arr = s->s_edit->k.rot_o_abs; last_arr = s->s_edit->k.rot_o_abs_last; break;
	    case 'v':
	    default:
		abs_arr = s->s_edit->k.rot_v_abs; last_arr = s->s_edit->k.rot_v_abs_last; break;
	}
    } else {
	if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
	    abs_arr  = view_state->k.rot_m_abs;
	    last_arr = view_state->k.rot_m_abs_last;
	} else {
	    abs_arr  = view_state->k.rot_v_abs;
	    last_arr = view_state->k.rot_v_abs_last;
	}
    }

    if (incr) {
	abs_arr[axis] += val;
	delta = val; /* legacy rvec[axis] = f in incremental mode */
    } else {
	delta = val - last_arr[axis];
	abs_arr[axis] = val;
    }

    /* Wrap & sync last */
    abs_arr[axis] = wrap_angle_180(abs_arr[axis]);
    last_arr[axis] = abs_arr[axis];

    return delta;
}

/* --- Translation (Rate) ---
 * Very similar to rotation rate but without origin.
 */
static void
knob_apply_translation_rate(struct mged_state *s,
	knob_axis_t axis,
	fastf_t val,
	int incr,
	int model_flag,
	int view_flag,
	int edit_this_cmd)
{
    if (axis < 0) return;

    if (edit_this_cmd) {
	switch (mged_variables->mv_coords) {
	    case 'm':
	    case 'o': /* object shares tra_m in legacy editing */
		if (incr)
		    s->s_edit->k.tra_m[axis] += val;
		else
		    s->s_edit->k.tra_m[axis] = val;
		s->s_edit->edit_rate_mt_dm = s->mged_curr_dm;
		break;
	    case 'v':
	    default:
		if (incr)
		    s->s_edit->k.tra_v[axis] += val;
		else
		    s->s_edit->k.tra_v[axis] = val;
		s->s_edit->edit_rate_vt_dm = s->mged_curr_dm;
		break;
	}
    } else {
	if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
	    if (incr)
		view_state->k.tra_m[axis] += val;
	    else
		view_state->k.tra_m[axis] = val;
	} else {
	    if (incr)
		view_state->k.tra_v[axis] += val;
	    else
		view_state->k.tra_v[axis] = val;
	}
    }
}

/* --- Translation (Absolute) ---
 * Legacy semantics:
 *  - sf = f * local2base / gv_scale (view or edit paths that require conversion)
 *  - incr: add sf to absolute value; last = new; tvec[axis] = original f
 *  - set  : tvec[axis] = f - (last_val * scale * base2local); store sf; last = sf
 * We replicate exactly.  Returns the tvec[axis] contribution.
 */
static fastf_t
knob_apply_translation_abs(struct mged_state *s,
	knob_axis_t axis,
	fastf_t f_user,
	int incr,
	int model_flag,
	int view_flag,
	int edit_this_cmd)
{
    if (axis < 0) return 0.0;

    fastf_t sf = f_user * s->dbip->dbi_local2base / view_state->vs_gvp->gv_scale;
    fastf_t *abs_arr = NULL, *last_arr = NULL;

    if (edit_this_cmd) {
	switch (mged_variables->mv_coords) {
	    case 'm':
	    case 'o':
		abs_arr  = s->s_edit->k.tra_m_abs;
		last_arr = s->s_edit->k.tra_m_abs_last;
		break;
	    case 'v':
	    default:
		abs_arr  = s->s_edit->k.tra_v_abs;
		last_arr = s->s_edit->k.tra_v_abs_last;
		break;
	}
    } else {
	if (model_flag || (mged_variables->mv_coords == 'm' && !view_flag)) {
	    abs_arr  = view_state->k.tra_m_abs;
	    last_arr = view_state->k.tra_m_abs_last;
	} else {
	    abs_arr  = view_state->k.tra_v_abs;
	    last_arr = view_state->k.tra_v_abs_last;
	}
    }

    fastf_t tvec_val = 0.0;
    if (incr) {
	abs_arr[axis] += sf;
	last_arr[axis] = abs_arr[axis];
	tvec_val = f_user;
    } else {
	/* delta = f_user - last * scale * base2local */
	tvec_val = f_user - last_arr[axis] * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local;
	abs_arr[axis] = sf;
	last_arr[axis] = abs_arr[axis];
    }

    return tvec_val;
}

/* --- Scale (Rate) ---
 * edit vs view.  Legacy: immediate modify of s->s_edit->k.sca or view_state->k.sca.
 */
static void
knob_apply_scale_rate(struct mged_state *s,
	fastf_t val,
	int incr,
	int edit_this_cmd)
{
    if (edit_this_cmd) {
	if (incr)
	    s->s_edit->k.sca += val;
	else
	    s->s_edit->k.sca  = val;
    } else {
	if (incr)
	    view_state->k.sca += val;
	else
	    view_state->k.sca  = val;
    }
}

/* --- Scale (Absolute) ---
 * For view scale: manipulate gv_a_scale + abs_zoom logic (legacy).
 * For edit scale: updates s->s_edit->k.sca_abs and calls sedit_abs_scale or oedit_abs_scale.
 */
static void
knob_apply_scale_abs(struct mged_state *s,
	fastf_t val,
	int incr,
	int edit_this_cmd)
{
    if (edit_this_cmd) {
	if (incr)
	    s->s_edit->k.sca_abs += val;
	else
	    s->s_edit->k.sca_abs = val;

	if (s->global_editing_state == ST_S_EDIT) {
	    sedit_abs_scale(s);
	} else {
	    oedit_abs_scale(s);
	}
    } else {
	if (incr)
	    view_state->vs_gvp->gv_a_scale += val;
	else
	    view_state->vs_gvp->gv_a_scale  = val;
	{
	    char *av[2] = {"zoom", "1.0"};
	    /* abs_zoom inline (kept original function in file) */
	    if (-SMALL_FASTF < view_state->vs_gvp->gv_a_scale &&
		    view_state->vs_gvp->gv_a_scale < SMALL_FASTF) {
		view_state->vs_gvp->gv_scale = view_state->vs_gvp->gv_i_scale;
	    } else {
		if (view_state->vs_gvp->gv_a_scale > 0) {
		    view_state->vs_gvp->gv_scale =
			view_state->vs_gvp->gv_i_scale * (1.0 - view_state->vs_gvp->gv_a_scale);
		} else {
		    view_state->vs_gvp->gv_scale =
			view_state->vs_gvp->gv_i_scale * (1.0 + (view_state->vs_gvp->gv_a_scale * -9.0));
		}
	    }
	    if (view_state->vs_gvp->gv_scale < BV_MINVIEWSIZE)
		view_state->vs_gvp->gv_scale = BV_MINVIEWSIZE;

	    ged_exec_zoom(s->gedp, 2, (const char **)av);

	    if (!ZERO(view_state->k.tra_v_abs[X]) ||
		    !ZERO(view_state->k.tra_v_abs[Y]) ||
		    !ZERO(view_state->k.tra_v_abs[Z])) {
		set_absolute_tran(s);
	    }
	}
    }
}

/* --- ADC pass-through helper --- */
static void
knob_apply_adc(struct mged_state *UNUSED(s),
	Tcl_Interp *interp,
	const char *token,
	fastf_t fval,
	int ival,
	int incr)
{
    struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;
    bu_vls_printf(&tcl_cmd, "adc ");
    if (incr) bu_vls_printf(&tcl_cmd, "-i ");

    if (BU_STR_EQUAL(token, "xadc")) {
	bu_vls_printf(&tcl_cmd, "x %d", ival);
    } else if (BU_STR_EQUAL(token, "yadc")) {
	bu_vls_printf(&tcl_cmd, "y %d", ival);
    } else if (BU_STR_EQUAL(token, "ang1")) {
	bu_vls_printf(&tcl_cmd, "a1 %f", fval);
    } else if (BU_STR_EQUAL(token, "ang2")) {
	bu_vls_printf(&tcl_cmd, "a2 %f", fval);
    } else if (BU_STR_EQUAL(token, "distadc")) {
	bu_vls_printf(&tcl_cmd, "odst %d", ival);
    }
    Tcl_Eval(interp, bu_vls_cstr(&tcl_cmd));
    bu_vls_free(&tcl_cmd);
}

/* --- zap / calibrate / zero & misc --- */
static int
knob_apply_misc(struct mged_state *s,
	Tcl_Interp *interp,
	const char *token)
{
    if (BU_STR_EQUAL(token, "zap") || BU_STR_EQUAL(token, "zero")) {
	VSETALL(view_state->k.rot_m, 0.0);
	VSETALL(view_state->k.tra_m, 0.0);
	VSETALL(view_state->k.rot_v, 0.0);
	VSETALL(view_state->k.tra_v, 0.0);
	view_state->k.sca = 0.0;

	VSETALL(s->s_edit->k.rot_m, 0.0);
	VSETALL(s->s_edit->k.rot_o, 0.0);
	VSETALL(s->s_edit->k.rot_v, 0.0);
	VSETALL(s->s_edit->k.tra_m, 0.0);
	VSETALL(s->s_edit->k.tra_v, 0.0);
	s->s_edit->k.sca = 0.0;

	knob_update_rate_vars(s);
	Tcl_Eval(interp, "adc reset");
	(void)mged_svbase(s);
	return 1;
    }
    if (BU_STR_EQUAL(token, "calibrate")) {
	/* Reset BOTH model and view absolute translation baselines and their last arrays.
	 * This avoids stale last values if the user changes coordinate modes after calibrate. */
	VSETALL(view_state->k.tra_v_abs, 0.0);
	VSETALL(view_state->k.tra_v_abs_last, 0.0);
	VSETALL(view_state->k.tra_m_abs, 0.0);
	VSETALL(view_state->k.tra_m_abs_last, 0.0);
	return 1;
    }
    return 0;
}

int
f_knob(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int incr_flag = 0;
    int view_flag = 0;
    int model_flag = 0;
    int force_edit = 0;
    char origin = '\0';

    vect_t rvec; VSETALL(rvec, 0.0);
    vect_t tvec; VSETALL(tvec, 0.0);
    int do_rot = 0;
    int do_tran = 0;

    CHECK_DBI_NULL;

    if (argc < 1) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&vls, "help knob");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* Parse options */
    {
	int c;
	bu_optind = 1;
	while ((c = bu_getopt(argc, (char * const *)argv, "eimo:v")) != -1) {
	    switch (c) {
		case 'e':
		    force_edit = 1;
		    break;
		case 'i':
		    incr_flag  = 1;
		    break;
		case 'm':
		    model_flag = 1;
		    break;
		case 'v':
		    view_flag  = 1;
		    break;
		case 'o':
		    origin = *bu_optarg;
		    break;
		default:
		    break;
	    }
	}
	argv += (bu_optind - 1);
	argc -= (bu_optind - 1);
    }

    if (origin != 'v' && origin != 'm' && origin != 'e' && origin != 'k')
	origin = mged_variables->mv_rotate_about;

    /* No extra args => print current values */
    if (argc == 1) {
	return mged_print_knobvals(s, interp);
    }

    /* Process token/value pairs */
    --argc; ++argv;
    while (argc > 0) {
	const char *token = argv[0];
	const char *valstr = (argc > 1) ? argv[1] : NULL;

	if (!valstr) {
	    /* Need a numeric value unless misc/zero handled separately */
	    if (!knob_apply_misc(s, interp, token)) {
		goto usage;
	    }
	    --argc; ++argv;
	    continue;
	}

	/* Look up token classification */
	const struct knob_token_desc *desc = knob_lookup_token(token);
	if (!desc) {
	    /* Could be zap/zero etc. */
	    if (!knob_apply_misc(s, interp, token))
		goto usage;
	    --argc; ++argv;
	    continue;
	}

	/* Handle misc tokens immediately, consuming ONLY the token.
	 * Any following numeric word will be treated as a separate token
	 * and (as in legacy) will trigger usage() since it's unknown.
	 * This restores original behavior where "knob zap 5" produced an error.
	 */
	if (desc->kind == KNOB_MISC) {
	    knob_apply_misc(s, interp, token);
	    --argc; ++argv;
	    continue;
	}

	/* Extract numeric */
	int ival = atoi(valstr);
	fastf_t fval = atof(valstr);

	/* Immediate ADC or misc path? */
	if (desc->kind == KNOB_ADC) {
	    knob_apply_adc(s, interp, token, fval, ival, incr_flag);
	    argc -= 2; argv += 2;
	    continue;
	}

	/* Determine if this token is handled as edit */
	int edit_this_cmd = knob_is_edit_cmd(s, token, model_flag, view_flag, force_edit);

	switch (desc->kind) {

	    case KNOB_ROT_RATE:
		knob_apply_rotation_rate(s, desc->axis, fval, incr_flag,
			model_flag, view_flag,
			edit_this_cmd, origin);
		break;

	    case KNOB_ROT_ABS: {
				   fastf_t delta = knob_apply_rotation_abs(s, desc->axis, fval, incr_flag,
					   model_flag, view_flag,
					   edit_this_cmd);
				   rvec[desc->axis] = delta;
				   do_rot = 1;
				   break;
			       }

	    case KNOB_TRAN_RATE:
			       knob_apply_translation_rate(s, desc->axis, fval, incr_flag,
				       model_flag, view_flag,
				       edit_this_cmd);
			       break;

	    case KNOB_TRAN_ABS: {
				    fastf_t delta = knob_apply_translation_abs(s, desc->axis, fval, incr_flag,
					    model_flag, view_flag,
					    edit_this_cmd);
				    tvec[desc->axis] = delta;
				    do_tran = 1;
				    break;
				}

	    case KNOB_SCALE_RATE:
				knob_apply_scale_rate(s, fval, incr_flag, edit_this_cmd);
				break;

	    case KNOB_SCALE_ABS:
				knob_apply_scale_abs(s, fval, incr_flag, edit_this_cmd);
				break;

	    default:
				goto usage;
	}

	argc -= 2;
	argv += 2;
    }

    /* Apply any accumulated translation or rotation (non-abs scale already applied) */
    if (do_tran) {
	knob_tran(s, tvec, model_flag, view_flag, force_edit);
    }
    if (do_rot) {
	knob_rot(s, rvec, origin, model_flag, view_flag, force_edit);
    }

    check_nonzero_rates(s);

    /* Synchronize MGED's authoritative knob state into the active bview so that
     * subsequent "view knob" (libged) commands see the accumulated rates and
     * absolute values.  Without this, mixing "knob ..." then "view knob ..."
     * would drop the earlier MGED changes because vs_gvp->k lags behind
     * view_state->k. */
    if (view_state && view_state->vs_gvp) {
	view_state->vs_gvp->k = view_state->k;
    }

    return TCL_OK;

usage:
    Tcl_AppendResult(interp,
	    "knob: x, y, z for rotation in degrees\n"
	    "knob: S for scale; X, Y, Z for slew (rates, range -1..+1)\n"
	    "knob: ax, ay, az for absolute rotation in degrees; aS for absolute scale.\n"
	    "knob: aX, aY, aZ for absolute slew.  calibrate to set current slew to 0\n"
	    "knob: xadc, yadc, distadc (values, range -2048..+2047)\n"
	    "knob: ang1, ang2 for adc angles in degrees\n"
	    "knob: zero (cancel motion)\n", (char *)NULL);
    return TCL_ERROR;
}


int
mged_zoom(struct mged_state *s, double val)
{
    int ret;
    char *av[3];
    char buf[32];
    Tcl_DString ds;

    if (s->gedp == GED_NULL) {
	return TCL_OK;
    }

    if (val > 0.0)
	snprintf(buf, 32, "%f", val);
    else
	snprintf(buf, 32, "%f", 1.0); /* do nothing */

    av[0] = "zoom";
    av[1] = buf;

    ret = ged_exec_zoom(s->gedp, 2, (const char **)av);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
    Tcl_DStringResult(s->interp, &ds);

    if (ret != BRLCAD_OK) {
	return TCL_ERROR;
    }

    view_state->vs_gvp->gv_a_scale = 1.0 - view_state->vs_gvp->gv_scale / view_state->vs_gvp->gv_i_scale;

    if (view_state->vs_gvp->gv_a_scale < 0.0) {
	view_state->vs_gvp->gv_a_scale /= 9.0;
    }

    if (!ZERO(view_state->k.tra_v_abs[X])
	|| !ZERO(view_state->k.tra_v_abs[Y])
	|| !ZERO(view_state->k.tra_v_abs[Z]))
    {
	set_absolute_tran(s);
    }

    ret = TCL_OK;
    if (s->gedp->ged_gvp && s->gedp->ged_gvp->gv_s->adaptive_plot_csg &&
	s->gedp->ged_gvp->gv_s->redraw_on_zoom)
    {
	ret = redraw_visible_objects(s);
    }

    view_state->vs_flag = 1;

    return ret;
}


/*
 * A scale factor of 2 will increase the view size by a factor of 2,
 * (i.e., a zoom out) which is accomplished by reducing Viewscale in half.
 */
int
cmd_zoom(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    double zval;

    if (argc != 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help zoom");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    /* sanity check the zoom value */
    zval = atof(argv[1]);
    if (zval > 0.0)
	return mged_zoom(s, zval);

    return TCL_ERROR;
}


/*
 * Break up a path string into its constituents.
 *
 * This function has one parameter:  a slash-separated path.
 * path_parse() allocates storage for and copies each constituent
 * of path.  It returns a null-terminated array of these copies.
 *
 * It is the caller's responsibility to free the copies and the
 * pointer to them.
 */
static char **
path_parse (char *path)
{
    int nm_constituents;
    int i;
    char *pp;
    char *start_addr;
    char **result;

    nm_constituents = ((*path != '/') && (*path != '\0'));

    for (pp = path; *pp != '\0'; ++pp)
	if (*pp == '/') {
	    while (*++pp == '/')
		;

	    if (*pp != '\0') {
		++nm_constituents;
	    }
	}

    result = (char **) bu_malloc((nm_constituents + 1) * sizeof(char *),
				 "array of strings");

    for (i = 0, pp = path; i < nm_constituents; ++i) {
	while (*pp == '/') {
	    ++pp;
	}

	start_addr = pp;

	while ((*++pp != '/') && (*pp != '\0'))
	    ;

	result[i] = (char *) bu_malloc((pp - start_addr + 1) * sizeof(char), "string");
	bu_strlcpy(result[i], start_addr, (pp - start_addr + 1) * sizeof(char));
    }

    result[nm_constituents] = 0;

    return result;
}


int
cmd_setview(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    int ret;
    Tcl_DString ds;

    if (s->gedp == GED_NULL) {
	return TCL_OK;
    }

    ret = ged_exec(s->gedp, argc, (const char **)argv);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret != BRLCAD_OK) {
	return TCL_ERROR;
    }

    if (!ZERO(view_state->k.tra_v_abs[X])
	|| !ZERO(view_state->k.tra_v_abs[Y])
	|| !ZERO(view_state->k.tra_v_abs[Z]))
    {
	set_absolute_tran(s);
    }

    view_state->vs_flag = 1;

    return TCL_OK;
}


int
f_slewview(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    int ret;
    Tcl_DString ds;
    point_t old_model_center;
    point_t new_model_center;
    vect_t diff;
    mat_t delta;

    if (s->gedp == GED_NULL) {
	return TCL_OK;
    }

    /* this is for the ModelDelta calculation below */
    MAT_DELTAS_GET_NEG(old_model_center, view_state->vs_gvp->gv_center);

    Tcl_DStringInit(&ds);

    ret = ged_exec(s->gedp, argc, (const char **)argv);
    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret != BRLCAD_OK) {
	return TCL_ERROR;
    }

    view_state->vs_flag = 1;

    /* all this for ModelDelta */
    MAT_DELTAS_GET_NEG(new_model_center, view_state->vs_gvp->gv_center);
    VSUB2(diff, new_model_center, old_model_center);
    MAT_IDN(delta);
    MAT_DELTAS_VEC(delta, diff);
    bn_mat_mul2(delta, view_state->vs_ModelDelta);	/* updates ModelDelta */

    set_absolute_tran(s);
    return TCL_OK;
}


/* set view reference base */
int
mged_svbase(struct mged_state *s)
{
    MAT_DELTAS_GET_NEG(view_state->vs_orig_pos, view_state->vs_gvp->gv_center);
    view_state->vs_gvp->gv_i_scale = view_state->vs_gvp->gv_scale;

    /* reset absolute slider values */
    VSETALL(view_state->k.rot_v_abs, 0.0);
    VSETALL(view_state->k.rot_v_abs_last, 0.0);
    VSETALL(view_state->k.rot_m_abs, 0.0);
    VSETALL(view_state->k.rot_m_abs_last, 0.0);
    VSETALL(view_state->k.tra_v_abs, 0.0);
    VSETALL(view_state->k.tra_v_abs_last, 0.0);
    VSETALL(view_state->k.tra_m_abs, 0.0);
    VSETALL(view_state->k.tra_m_abs_last, 0.0);
    // TODO - should we be modding vs_gvp here when everything else is in view_state?
    // Alternately, should we just use vs_gvp rather than values in view_state?
    view_state->vs_gvp->gv_a_scale = 0.0;
    view_state->k.sca_abs = view_state->vs_gvp->gv_a_scale;

    if (mged_variables->mv_faceplate && mged_variables->mv_orig_gui) {
	s->mged_curr_dm->dm_dirty = 1;
	dm_set_dirty(DMP, 1);
    }

    return TCL_OK;
}


int
f_svbase(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    int status;

    if (argc < 1 || 1 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	if (argv && argc > 1) {
	    bu_log("Unexpected parameter [%s]\n", argv[1]);
	}

	bu_vls_printf(&vls, "helpdevel svb");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    status = mged_svbase(s);

    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	/* if sharing view while faceplate and original gui (i.e. button menu, sliders) are on */
	if (m_dmp->dm_view_state == view_state &&
	    m_dmp->dm_mged_variables->mv_faceplate &&
	    m_dmp->dm_mged_variables->mv_orig_gui) {
	    m_dmp->dm_dirty = 1;
	    dm_set_dirty(m_dmp->dm_dmp, 1);
	}
    }

    return status;
}


/*
 * Set the view.  Angles are DOUBLES, in degrees.
 *
 * Given that viewvec = scale . rotate . (xlate to view center) . modelvec,
 * we just replace the rotation matrix.
 * (This assumes rotation around the view center).
 */
void
setview(struct mged_state *s,
	double a1,
	double a2,
	double a3)		/* DOUBLE angles, in degrees */
{
    char *av[5];
    char xbuf[32];
    char ybuf[32];
    char zbuf[32];

    if (s->gedp == GED_NULL) {
	return;
    }

    snprintf(xbuf, 32, "%f", a1);
    snprintf(ybuf, 32, "%f", a2);
    snprintf(zbuf, 32, "%f", a3);

    av[0] = "setview";
    av[1] = xbuf;
    av[2] = ybuf;
    av[3] = zbuf;
    av[4] = (char *)0;
    ged_exec_setview(s->gedp, 4, (const char **)av);

    if (!ZERO(view_state->k.tra_v_abs[X])
	|| !ZERO(view_state->k.tra_v_abs[Y])
	|| !ZERO(view_state->k.tra_v_abs[Z]))
    {
	set_absolute_tran(s);
    }

    view_state->vs_flag = 1;
}


/*
 * Given a position in view space,
 * make that point the new view center.
 */
void
slewview(struct mged_state *s, vect_t view_pos)
{
    point_t old_model_center;
    point_t new_model_center;
    vect_t diff;
    mat_t delta;
    char *av[5];
    char xbuf[32];
    char ybuf[32];
    char zbuf[32];

    if (s->gedp == GED_NULL) {
	return;
    }

    /* this is for the ModelDelta calculation below */
    MAT_DELTAS_GET_NEG(old_model_center, view_state->vs_gvp->gv_center);

    snprintf(xbuf, 32, "%f", view_pos[X]);
    snprintf(ybuf, 32, "%f", view_pos[Y]);
    snprintf(zbuf, 32, "%f", view_pos[Z]);

    av[0] = "slew";
    av[1] = xbuf;
    av[2] = ybuf;
    av[3] = zbuf;
    av[4] = (char *)0;
    ged_exec_slew(s->gedp, 4, (const char **)av);

    /* all this for ModelDelta */
    MAT_DELTAS_GET_NEG(new_model_center, view_state->vs_gvp->gv_center);
    VSUB2(diff, new_model_center, old_model_center);
    MAT_IDN(delta);
    MAT_DELTAS_VEC(delta, diff);
    bn_mat_mul2(delta, view_state->vs_ModelDelta);	/* updates ModelDelta */

    set_absolute_tran(s);

    view_state->vs_flag = 1;
}


/*
 * Initialize vsp1 using vsp2 if vsp2 is not null.
 */
void
view_ring_init(struct _view_state *vsp1, struct _view_state *vsp2)
{
    struct view_ring *vrp1;
    struct view_ring *vrp2;

    BU_LIST_INIT(&vsp1->vs_headView.l);

    if (vsp2 != (struct _view_state *)NULL) {
	struct view_ring *vrp1_current_view = NULL;
	struct view_ring *vrp1_last_view = NULL;

	for (BU_LIST_FOR(vrp2, view_ring, &vsp2->vs_headView.l)) {
	    BU_ALLOC(vrp1, struct view_ring);
	    /* append to last list element */
	    BU_LIST_APPEND(vsp1->vs_headView.l.back, &vrp1->l);

	    MAT_COPY(vrp1->vr_rot_mat, vrp2->vr_rot_mat);
	    MAT_COPY(vrp1->vr_tvc_mat, vrp2->vr_tvc_mat);
	    vrp1->vr_scale = vrp2->vr_scale;
	    vrp1->vr_id = vrp2->vr_id;

	    if (vsp2->vs_current_view == vrp2) {
		vrp1_current_view = vrp1;
	    }

	    if (vsp2->vs_last_view == vrp2) {
		vrp1_last_view = vrp1;
	    }
	}

	vsp1->vs_current_view = vrp1_current_view;
	vsp1->vs_last_view = vrp1_last_view;
    } else {
	BU_ALLOC(vrp1, struct view_ring);
	BU_LIST_APPEND(&vsp1->vs_headView.l, &vrp1->l);

	vrp1->vr_id = 1;
	vsp1->vs_current_view = vrp1;
	vsp1->vs_last_view = vrp1;
    }
}


void
view_ring_destroy(struct mged_dm *dlp)
{
    struct view_ring *vrp;

    while (BU_LIST_NON_EMPTY(&dlp->dm_view_state->vs_headView.l)) {
	vrp = BU_LIST_FIRST(view_ring, &dlp->dm_view_state->vs_headView.l);
	BU_LIST_DEQUEUE(&vrp->l);
	bu_free((void *)vrp, "view_ring_destroy: vrp");
    }
}


/*
 * SYNOPSIS
 * view_ring add|next|prev|toggle
 * view_ring delete # delete view #
 * view_ring goto # goto view #
 * view_ring get [-a]			get the current view
 *
 * DESCRIPTION
 *
 * EXAMPLES
 *
 */
int
f_view_ring(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    int n;
    struct view_ring *vrp;
    struct view_ring *lv;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc < 2 || 3 < argc) {
	bu_vls_printf(&vls, "helpdevel view_ring");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "add")) {
	if (argc != 2) {
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* save current Viewrot */
	MAT_COPY(view_state->vs_current_view->vr_rot_mat, view_state->vs_gvp->gv_rotation);

	/* save current toViewcenter */
	MAT_COPY(view_state->vs_current_view->vr_tvc_mat, view_state->vs_gvp->gv_center);

	/* save current Viewscale */
	view_state->vs_current_view->vr_scale = view_state->vs_gvp->gv_scale;

	/* allocate memory and append to list */
	BU_ALLOC(vrp, struct view_ring);
	lv = BU_LIST_LAST(view_ring, &view_state->vs_headView.l);
	BU_LIST_APPEND(&lv->l, &vrp->l);

	/* assign a view number */
	vrp->vr_id = lv->vr_id + 1;

	view_state->vs_last_view = view_state->vs_current_view;
	view_state->vs_current_view = vrp;
	(void)mged_svbase(s);

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "next")) {
	if (argc != 2) {
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* check to see if this is the last view in the list */
	if (BU_LIST_IS_HEAD(view_state->vs_current_view->l.forw, &view_state->vs_headView.l) &&
	    BU_LIST_IS_HEAD(view_state->vs_current_view->l.back, &view_state->vs_headView.l)) {
	    return TCL_OK;
	}

	/* save current Viewrot */
	MAT_COPY(view_state->vs_current_view->vr_rot_mat, view_state->vs_gvp->gv_rotation);

	/* save current toViewcenter */
	MAT_COPY(view_state->vs_current_view->vr_tvc_mat, view_state->vs_gvp->gv_center);

	/* save current Viewscale */
	view_state->vs_current_view->vr_scale = view_state->vs_gvp->gv_scale;

	view_state->vs_last_view = view_state->vs_current_view;
	view_state->vs_current_view = BU_LIST_PNEXT(view_ring, view_state->vs_current_view);

	if (BU_LIST_IS_HEAD(view_state->vs_current_view, &view_state->vs_headView.l)) {
	    view_state->vs_current_view = BU_LIST_FIRST(view_ring, &view_state->vs_headView.l);
	}

	MAT_COPY(view_state->vs_gvp->gv_rotation, view_state->vs_current_view->vr_rot_mat);
	MAT_COPY(view_state->vs_gvp->gv_center, view_state->vs_current_view->vr_tvc_mat);
	view_state->vs_gvp->gv_scale = view_state->vs_current_view->vr_scale;

	new_mats(s);
	(void)mged_svbase(s);

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "prev")) {
	if (argc != 2) {
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* check to see if this is the last view in the list */
	if (BU_LIST_IS_HEAD(view_state->vs_current_view->l.forw, &view_state->vs_headView.l) &&
	    BU_LIST_IS_HEAD(view_state->vs_current_view->l.back, &view_state->vs_headView.l)) {
	    return TCL_OK;
	}

	/* save current Viewrot */
	MAT_COPY(view_state->vs_current_view->vr_rot_mat, view_state->vs_gvp->gv_rotation);

	/* save current toViewcenter */
	MAT_COPY(view_state->vs_current_view->vr_tvc_mat, view_state->vs_gvp->gv_center);

	/* save current Viewscale */
	view_state->vs_current_view->vr_scale = view_state->vs_gvp->gv_scale;

	view_state->vs_last_view = view_state->vs_current_view;
	view_state->vs_current_view = BU_LIST_PLAST(view_ring, view_state->vs_current_view);

	if (BU_LIST_IS_HEAD(view_state->vs_current_view, &view_state->vs_headView.l)) {
	    view_state->vs_current_view = BU_LIST_LAST(view_ring, &view_state->vs_headView.l);
	}

	MAT_COPY(view_state->vs_gvp->gv_rotation, view_state->vs_current_view->vr_rot_mat);
	MAT_COPY(view_state->vs_gvp->gv_center, view_state->vs_current_view->vr_tvc_mat);
	view_state->vs_gvp->gv_scale = view_state->vs_current_view->vr_scale;

	new_mats(s);
	(void)mged_svbase(s);

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "toggle")) {
	struct view_ring *save_last_view;

	if (argc != 2) {
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* save current Viewrot */
	MAT_COPY(view_state->vs_current_view->vr_rot_mat, view_state->vs_gvp->gv_rotation);

	/* save current toViewcenter */
	MAT_COPY(view_state->vs_current_view->vr_tvc_mat, view_state->vs_gvp->gv_center);

	/* save current Viewscale */
	view_state->vs_current_view->vr_scale = view_state->vs_gvp->gv_scale;

	save_last_view = view_state->vs_last_view;
	view_state->vs_last_view = view_state->vs_current_view;
	view_state->vs_current_view = save_last_view;
	MAT_COPY(view_state->vs_gvp->gv_rotation, view_state->vs_current_view->vr_rot_mat);
	MAT_COPY(view_state->vs_gvp->gv_center, view_state->vs_current_view->vr_tvc_mat);
	view_state->vs_gvp->gv_scale = view_state->vs_current_view->vr_scale;

	new_mats(s);
	(void)mged_svbase(s);

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "delete")) {
	if (argc != 3) {
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* search for view with id of n */
	n = atoi(argv[2]);

	for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l)) {
	    if (vrp->vr_id == n) {
		break;
	    }
	}

	if (BU_LIST_IS_HEAD(vrp, &view_state->vs_headView.l)) {
	    Tcl_AppendResult(interp, "view_ring delete: ", argv[2], " is not a valid view\n",
			     (char *)NULL);
	    return TCL_ERROR;
	}

	/* check to see if this is the last view in the list */
	if (BU_LIST_IS_HEAD(vrp->l.forw, &view_state->vs_headView.l) &&
	    BU_LIST_IS_HEAD(vrp->l.back, &view_state->vs_headView.l)) {
	    Tcl_AppendResult(interp, "view_ring delete: Cannot delete the only remaining view!\n", (char *)NULL);
	    return TCL_ERROR;
	}

	if (vrp == view_state->vs_current_view) {
	    if (view_state->vs_current_view == view_state->vs_last_view) {
		view_state->vs_current_view = BU_LIST_PNEXT(view_ring, view_state->vs_last_view);
		view_state->vs_last_view = view_state->vs_current_view;
	    } else {
		view_state->vs_current_view = view_state->vs_last_view;
	    }

	    MAT_COPY(view_state->vs_gvp->gv_rotation, view_state->vs_current_view->vr_rot_mat);
	    MAT_COPY(view_state->vs_gvp->gv_center, view_state->vs_current_view->vr_tvc_mat);
	    view_state->vs_gvp->gv_scale = view_state->vs_current_view->vr_scale;
	    new_mats(s);
	    (void)mged_svbase(s);
	} else if (vrp == view_state->vs_last_view) {
	    view_state->vs_last_view = view_state->vs_current_view;
	}

	BU_LIST_DEQUEUE(&vrp->l);
	bu_free((void *)vrp, "view_ring delete");

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "goto")) {
	if (argc != 3) {
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* search for view with id of n */
	n = atoi(argv[2]);

	for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l)) {
	    if (vrp->vr_id == n) {
		break;
	    }
	}

	if (BU_LIST_IS_HEAD(vrp, &view_state->vs_headView.l)) {
	    Tcl_AppendResult(interp, "view_ring goto: ", argv[2], " is not a valid view\n",
			     (char *)NULL);
	    return TCL_ERROR;
	}

	/* nothing to do */
	if (vrp == view_state->vs_current_view) {
	    return TCL_OK;
	}

	/* save current Viewrot */
	MAT_COPY(view_state->vs_current_view->vr_rot_mat, view_state->vs_gvp->gv_rotation);

	/* save current toViewcenter */
	MAT_COPY(view_state->vs_current_view->vr_tvc_mat, view_state->vs_gvp->gv_center);

	/* save current Viewscale */
	view_state->vs_current_view->vr_scale = view_state->vs_gvp->gv_scale;

	view_state->vs_last_view = view_state->vs_current_view;
	view_state->vs_current_view = vrp;
	MAT_COPY(view_state->vs_gvp->gv_rotation, view_state->vs_current_view->vr_rot_mat);
	MAT_COPY(view_state->vs_gvp->gv_center, view_state->vs_current_view->vr_tvc_mat);
	view_state->vs_gvp->gv_scale = view_state->vs_current_view->vr_scale;

	new_mats(s);
	(void)mged_svbase(s);

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "get")) {
	/* return current view */
	if (argc == 2) {
	    bu_vls_printf(&vls, "%d", view_state->vs_current_view->vr_id);
	    Tcl_AppendElement(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_OK;
	}

	if (!BU_STR_EQUAL("-a", argv[2])) {
	    bu_vls_printf(&vls, "help view_ring");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	for (BU_LIST_FOR(vrp, view_ring, &view_state->vs_headView.l)) {
	    bu_vls_printf(&vls, "%d", vrp->vr_id);
	    Tcl_AppendElement(interp, bu_vls_addr(&vls));
	    bu_vls_trunc(&vls, 0);
	}

	bu_vls_free(&vls);
	return TCL_OK;
    }

    Tcl_AppendResult(interp, "view_ring: unrecognized command - ", argv[1], (char *)NULL);
    return TCL_ERROR;
}



int
cmd_mrot(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    Tcl_DString ds;

    if (s->gedp == GED_NULL) {
	return TCL_OK;
    }

    if ((s->global_editing_state == ST_S_EDIT || s->global_editing_state == ST_O_EDIT) &&
	mged_variables->mv_transform == 'e') {
	char coord; /* dummy argument for ged_rot_args */
	mat_t rmat;

	if (argc != 2 && argc != 4) {
	    Tcl_DStringInit(&ds);
	    Tcl_DStringAppend(&ds, "Usage: ", -1);
	    Tcl_DStringAppend(&ds, argv[0], -1);
	    Tcl_DStringAppend(&ds, " x y z", -1);
	    Tcl_DStringResult(interp, &ds);

	    return TCL_ERROR;
	}

	/* We're only interested in getting rmat set */
	if (ged_rot_args(s->gedp, argc, (const char **)argv, &coord, rmat) != BRLCAD_OK) {
	    Tcl_DStringInit(&ds);
	    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
	    Tcl_DStringResult(interp, &ds);

	    return TCL_ERROR;
	}

	return mged_erot(s, view_state->vs_gvp->gv_coord, view_state->vs_gvp->gv_rotate_about, rmat);
    } else {
	int ret;

	Tcl_DStringInit(&ds);

	ret = ged_exec(s->gedp, argc, (const char **)argv);
	Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
	Tcl_DStringResult(interp, &ds);

	if (ret != BRLCAD_OK) {
	    return TCL_ERROR;
	}

	view_state->vs_flag = 1;

	return TCL_OK;
    }
}


int
cmd_vrot(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    int ret;
    Tcl_DString ds;

    if (s->gedp == GED_NULL) {
	return TCL_OK;
    }

    Tcl_DStringInit(&ds);

    ret = ged_exec(s->gedp, argc, (const char **)argv);
    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret != BRLCAD_OK) {
	return TCL_ERROR;
    }

    view_state->vs_flag = 1;
    set_absolute_tran(s);

    return TCL_OK;
}


int
cmd_rot(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    Tcl_DString ds;

    if (s->gedp == GED_NULL) {
	return TCL_OK;
    }

    if ((s->global_editing_state == ST_S_EDIT || s->global_editing_state == ST_O_EDIT) &&
	mged_variables->mv_transform == 'e') {
	char coord;
	mat_t rmat;

	if (ged_rot_args(s->gedp, argc, (const char **)argv, &coord, rmat) != BRLCAD_OK) {
	    Tcl_DStringInit(&ds);
	    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
	    Tcl_DStringResult(interp, &ds);

	    return TCL_ERROR;
	}

	return mged_erot(s, coord, view_state->vs_gvp->gv_rotate_about, rmat);
    } else {
	int ret;

	Tcl_DStringInit(&ds);

	ret = ged_exec(s->gedp, argc, (const char **)argv);
	Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
	Tcl_DStringResult(interp, &ds);

	if (ret != BRLCAD_OK) {
	    return TCL_ERROR;
	}

	view_state->vs_flag = 1;

	return TCL_OK;
    }
}


int
cmd_arot(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    Tcl_DString ds;
    /* static const char *usage = "x y z angle"; */

    if (s->gedp == GED_NULL) {
	return TCL_OK;
    }

    if ((s->global_editing_state == ST_S_EDIT || s->global_editing_state == ST_O_EDIT) &&
	mged_variables->mv_transform == 'e') {
	mat_t rmat;

	if (ged_arot_args(s->gedp, argc, (const char **)argv, rmat) != BRLCAD_OK) {
	    Tcl_DStringInit(&ds);
	    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
	    Tcl_DStringResult(interp, &ds);

	    return TCL_ERROR;
	}

	return mged_erot(s, view_state->vs_gvp->gv_coord, view_state->vs_gvp->gv_rotate_about, rmat);
    } else {
	int ret;

	Tcl_DStringInit(&ds);

	ret = ged_exec(s->gedp, argc, (const char **)argv);
	Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
	Tcl_DStringResult(interp, &ds);

	if (ret != BRLCAD_OK) {
	    return TCL_ERROR;
	}

	view_state->vs_flag = 1;

	return TCL_OK;
    }
}

int
cmd_tra(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    Tcl_DString ds;

    if (s->gedp == GED_NULL) {
	return TCL_OK;
    }

    if ((s->global_editing_state == ST_S_EDIT || s->global_editing_state == ST_O_EDIT) &&
	mged_variables->mv_transform == 'e') {
	char coord;
	vect_t tvec;

	if (ged_tra_args(s->gedp, argc, (const char **)argv, &coord, tvec) != BRLCAD_OK) {
	    Tcl_DStringInit(&ds);
	    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
	    Tcl_DStringResult(interp, &ds);

	    return TCL_ERROR;
	}

	return mged_etran(s, coord, tvec);
    } else {
	int ret;

	Tcl_DStringInit(&ds);

	ret = ged_exec(s->gedp, argc, (const char **)argv);
	Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
	Tcl_DStringResult(interp, &ds);

	if (ret != BRLCAD_OK) {
	    return TCL_ERROR;
	}

	view_state->vs_flag = 1;

	return TCL_OK;
    }
}


int
mged_escale(struct mged_state *s, fastf_t sfactor)
{
    fastf_t old_scale;

    if (-SMALL_FASTF < sfactor && sfactor < SMALL_FASTF) {
	return TCL_OK;
    }

    if (s->global_editing_state == ST_S_EDIT) {
	int save_edflag;

	save_edflag = s->s_edit->edit_flag;

	if (!SEDIT_SCALE) {
	    s->s_edit->edit_flag = SSCALE;
	}

	s->s_edit->es_scale = sfactor;
	old_scale = s->s_edit->acc_sc_sol;
	s->s_edit->acc_sc_sol *= sfactor;

	if (s->s_edit->acc_sc_sol < MGED_SMALL_SCALE) {
	    s->s_edit->acc_sc_sol = old_scale;
	    s->s_edit->edit_flag = save_edflag;
	    return TCL_OK;
	}

	if (s->s_edit->acc_sc_sol >= 1.0) {
	    s->s_edit->k.sca_abs = (s->s_edit->acc_sc_sol - 1.0) / 3.0;
	} else {
	    s->s_edit->k.sca_abs = s->s_edit->acc_sc_sol - 1.0;
	}

	sedit(s);

	s->s_edit->edit_flag = save_edflag;
    } else {
	point_t temp;
	point_t pos_model;
	mat_t smat;
	fastf_t inv_sfactor;

	inv_sfactor = 1.0 / sfactor;
	MAT_IDN(smat);

	switch (edobj) {
	    case BE_O_XSCALE:			    /* local scaling ... X-axis */
		smat[0] = sfactor;
		old_scale = s->s_edit->acc_sc[X];
		s->s_edit->acc_sc[X] *= sfactor;

		if (s->s_edit->acc_sc[X] < MGED_SMALL_SCALE) {
		    s->s_edit->acc_sc[X] = old_scale;
		    return TCL_OK;
		}

		break;
	    case BE_O_YSCALE:			    /* local scaling ... Y-axis */
		smat[5] = sfactor;
		old_scale = s->s_edit->acc_sc[Y];
		s->s_edit->acc_sc[Y] *= sfactor;

		if (s->s_edit->acc_sc[Y] < MGED_SMALL_SCALE) {
		    s->s_edit->acc_sc[Y] = old_scale;
		    return TCL_OK;
		}

		break;
	    case BE_O_ZSCALE:			    /* local scaling ... Z-axis */
		smat[10] = sfactor;
		old_scale = s->s_edit->acc_sc[Z];
		s->s_edit->acc_sc[Z] *= sfactor;

		if (s->s_edit->acc_sc[Z] < MGED_SMALL_SCALE) {
		    s->s_edit->acc_sc[Z] = old_scale;
		    return TCL_OK;
		}

		break;
	    case BE_O_SCALE:			     /* global scaling */
	    default:
		smat[15] = inv_sfactor;
		old_scale = s->s_edit->acc_sc_sol;
		s->s_edit->acc_sc_sol *= inv_sfactor;

		if (s->s_edit->acc_sc_sol < MGED_SMALL_SCALE) {
		    s->s_edit->acc_sc_sol = old_scale;
		    return TCL_OK;
		}

		break;
	}

	/* Have scaling take place with respect to keypoint,
	 * NOT the view center.
	 *
	 * The changes written to model_changes are what are ultimately used by
	 * moveHobj or moveHinstance to change the on-disk geometry.  The job
	 * of updating what is being shown in the MGED window for editing
	 * purposes is handled by the new_edit_mats call, which updates
	 * view_state->vs_model2objview.  dozoom.c uses that matrix to modify
	 * how solids involved in editing are displayed.
	 *
	 * Note that this means wireframes for editing solids in oed mode
	 * aren't going to be redrawn to reflect the new solid parameters that
	 * would result from matrix application (for example, if a solid edit
	 * of the same operation would have produced a denser wireframe, that
	 * wireframe will not be produced in oed mode - instead the original
	 * will be distorted by the matrix.)  It is quite possible that the
	 * oed-distorted shape of a primitive will not be representable using
	 * valid primitive parameters, so regeneration of new wireframes in
	 * response to the new shape isn't feasible.
	 */
	VMOVE(temp, s->s_edit->e_keypoint);
	MAT4X3PNT(pos_model, s->s_edit->model_changes, temp);
	wrt_point(s->s_edit->model_changes, smat, s->s_edit->model_changes, pos_model);

	new_edit_mats(s);
    }

    return TCL_OK;
}


int
mged_vscale(struct mged_state *s, fastf_t sfactor)
{
    fastf_t f;

    if (-SMALL_FASTF < sfactor && sfactor < SMALL_FASTF) {
	return TCL_OK;
    }

    view_state->vs_gvp->gv_scale *= sfactor;

    if (view_state->vs_gvp->gv_scale < BV_MINVIEWSIZE) {
	view_state->vs_gvp->gv_scale = BV_MINVIEWSIZE;
    }

    f = view_state->vs_gvp->gv_scale / view_state->vs_gvp->gv_i_scale;

    if (f >= 1.0) {
	view_state->vs_gvp->gv_a_scale = (f - 1.0) / -9.0;
    } else {
	view_state->vs_gvp->gv_a_scale = 1.0 - f;
    }

    new_mats(s);
    return TCL_OK;
}


int
cmd_sca(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    Tcl_DString ds;

    if (s->gedp == GED_NULL) {
	return TCL_OK;
    }

    if ((s->global_editing_state == ST_S_EDIT || s->global_editing_state == ST_O_EDIT) && mged_variables->mv_transform == 'e') {
	fastf_t sf1 = 0.0; /* combined xyz scale or x scale */
	fastf_t sf2 = 0.0; /* y scale */
	fastf_t sf3 = 0.0; /* z scale */
	int save_edobj;
	int ret;

	if (ged_scale_args(s->gedp, argc, (const char **)argv, &sf1, &sf2, &sf3) != BRLCAD_OK) {
	    Tcl_DStringInit(&ds);
	    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
	    Tcl_DStringResult(interp, &ds);
	    return TCL_ERROR;
	}

	/* argc is 2 or 4 because otherwise ged_scale_args fails */
	if (argc == 2) {
	    if (sf1 <= SMALL_FASTF || INFINITY < sf1) {
		return TCL_OK;
	    }

	    return mged_escale(s, sf1);
	} else {
	    if (sf1 <= SMALL_FASTF || INFINITY < sf1) {
		return TCL_OK;
	    }

	    if (sf2 <= SMALL_FASTF || INFINITY < sf2) {
		return TCL_OK;
	    }

	    if (sf3 <= SMALL_FASTF || INFINITY < sf3) {
		return TCL_OK;
	    }

	    if (s->global_editing_state == ST_O_EDIT) {
		save_edobj = edobj;
		edobj = BE_O_XSCALE;

		if ((ret = mged_escale(s, sf1)) == TCL_OK) {
		    edobj = BE_O_YSCALE;

		    if ((ret = mged_escale(s, sf2)) == TCL_OK) {
			edobj = BE_O_ZSCALE;
			ret = mged_escale(s, sf3);
		    }
		}

		edobj = save_edobj;
		return ret;
	    } else {
		/* argc was 4 but state was ST_S_EDIT so do nothing */
		bu_log("ERROR: Can only scale primitives uniformly (one scale factor).\n");
		return TCL_OK;
	    }
	}
    } else {
	int ret;
	fastf_t f;

	Tcl_DStringInit(&ds);
	ret = ged_exec(s->gedp, argc, (const char **)argv);
	Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
	Tcl_DStringResult(interp, &ds);

	if (ret != BRLCAD_OK) {
	    return TCL_ERROR;
	}

	f = view_state->vs_gvp->gv_scale / view_state->vs_gvp->gv_i_scale;

	if (f >= 1.0) {
	    view_state->vs_gvp->gv_a_scale = (f - 1.0) / -9.0;
	} else {
	    view_state->vs_gvp->gv_a_scale = 1.0 - f;
	}

	view_state->vs_flag = 1;

	return TCL_OK;
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
