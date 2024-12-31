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

static void init_sedit_vars(struct mged_state *), init_oedit_vars(struct mged_state *), init_oedit_guts(struct mged_state *);


point_t e_axes_pos;
point_t curr_e_axes_pos;

extern short int fixv;		/* used in ECMD_ARB_ROTATE_FACE, f_eqn(): fixed vertex */


/* data for solid editing */
int sedraw;	/* apply solid editing changes */

int es_type;		/* COMGEOM solid type */
int es_edflag;		/* type of editing for this solid */
int es_edclass;		/* type of editing class for this solid */
fastf_t es_peqn[7][4];		/* ARBs defining plane equations */
fastf_t es_m[3];		/* edge(line) slope */
mat_t es_mat;			/* accumulated matrix of path */
mat_t es_invmat;		/* inverse of es_mat KAA */


point_t es_keypoint;		/* center of editing xforms */
const char *es_keytag;		/* string identifying the keypoint */
int es_keyfixed;		/* keypoint specified by user? */

vect_t es_para;	/* keyboard input param. Only when inpara set.  */
int inpara;		/* es_para valid.  es_mvalid must = 0 */

vect_t es_mparam;	/* mouse input param.  Only when es_mvalid set */
int es_mvalid;	/* es_mparam valid.  inpara must = 0 */

/* These values end up in es_menu, as do ARB vertex numbers */
int es_menu;		/* item selected from menu */

#define PARAM_1ARG (es_edflag == SSCALE || \
		    es_edflag == PSCALE || \
		    es_edflag == ECMD_BOT_THICK || \
		    es_edflag == ECMD_VOL_THRESH_LO || \
		    es_edflag == ECMD_VOL_THRESH_HI || \
		    es_edflag == ECMD_DSP_SCALE_X || \
		    es_edflag == ECMD_DSP_SCALE_Y || \
		    es_edflag == ECMD_DSP_SCALE_ALT || \
		    es_edflag == ECMD_EBM_HEIGHT || \
		    es_edflag == ECMD_CLINE_SCALE_H || \
		    es_edflag == ECMD_CLINE_SCALE_R || \
		    es_edflag == ECMD_CLINE_SCALE_T || \
		    es_edflag == ECMD_EXTR_SCALE_H)
#define PARAM_2ARG (es_edflag == ECMD_DSP_FSIZE || \
		    es_edflag == ECMD_EBM_FSIZE)

void
set_e_axes_pos(struct mged_state *s, int both)
    /* if (!both) then set only curr_e_axes_pos, otherwise
       set e_axes_pos and curr_e_axes_pos */
{
    update_views = 1;
    dm_set_dirty(DMP, 1);

    struct rt_db_internal *ip = &s->edit_state.es_int;

    if (MGED_OBJ[ip->idb_type].ft_e_axes_pos) {
	(*MGED_OBJ[ip->idb_type].ft_e_axes_pos)(ip, &s->tol.tol);
	return;
    } else {
	VMOVE(curr_e_axes_pos, es_keypoint);
    }

    if (both) {
	VMOVE(e_axes_pos, curr_e_axes_pos);

	if (EDIT_ROTATE) {
	    es_edclass = EDIT_CLASS_ROTATE;
	    VSETALL(s->edit_state.edit_absolute_model_rotate, 0.0);
	    VSETALL(s->edit_state.edit_absolute_object_rotate, 0.0);
	    VSETALL(s->edit_state.edit_absolute_view_rotate, 0.0);
	    VSETALL(s->edit_state.last_edit_absolute_model_rotate, 0.0);
	    VSETALL(s->edit_state.last_edit_absolute_object_rotate, 0.0);
	    VSETALL(s->edit_state.last_edit_absolute_view_rotate, 0.0);
	} else if (EDIT_TRAN) {
	    es_edclass = EDIT_CLASS_TRAN;
	    VSETALL(s->edit_state.edit_absolute_model_tran, 0.0);
	    VSETALL(s->edit_state.edit_absolute_view_tran, 0.0);
	    VSETALL(s->edit_state.last_edit_absolute_model_tran, 0.0);
	    VSETALL(s->edit_state.last_edit_absolute_view_tran, 0.0);
	} else if (EDIT_SCALE) {
	    es_edclass = EDIT_CLASS_SCALE;

	    if (SEDIT_SCALE) {
		s->edit_state.edit_absolute_scale = 0.0;
		acc_sc_sol = 1.0;
	    }
	} else {
	    es_edclass = EDIT_CLASS_NULL;
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

    if (GEOM_EDIT_STATE == ST_VIEW || GEOM_EDIT_STATE == ST_S_PICK || GEOM_EDIT_STATE == ST_O_PICK)
	return TCL_OK;

    get_solid_keypoint(s, &es_keypoint, &es_keytag, &s->edit_state.es_int, es_mat);
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

    es_menu = 0;
    if (id == ID_ARB8) {
	struct rt_arb_internal *arb;
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;

	arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	type = rt_arb_std_type(&s->edit_state.es_int, &s->tol.tol);
	es_type = type;

	if (rt_arb_calc_planes(&error_msg, arb, es_type, es_peqn, &s->tol.tol)) {
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
    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, es_mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(es_invmat, es_mat);

    /* Establish initial keypoint */
    es_keytag = "";
    get_solid_keypoint(s, &es_keypoint, &es_keytag, &s->edit_state.es_int, es_mat);

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
    es_edflag = IDLE;

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
 * making a change to es_int or es_mat.
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

    struct rt_db_internal *ip = &s->edit_state.es_int; 
    if (MGED_OBJ[ip->idb_type].ft_menu_item) {
	struct menu_item *mi = (*MGED_OBJ[ip->idb_type].ft_menu_item)(&s->tol.tol);
	mmenu_set_all(s, MENU_L1, mi);
    }

    es_edflag = IDLE;	/* Drop out of previous edit mode */
    es_menu = 0;
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
 * Partial scaling of a solid.
 */
void
pscale(struct mged_state *s)
{
    switch (es_menu) {

	case MENU_VOL_CSIZE:
	    menu_vol_csize(s);
	    break;
	case MENU_TGC_SCALE_H:
	    menu_tgc_scale_h(s);
	    break;
	case MENU_TGC_SCALE_H_V:
	    menu_tgc_scale_h_v(s);
	    break;
	case MENU_TGC_SCALE_H_CD:
	    menu_tgc_scale_h_cd(s);
	    break;
	case MENU_TGC_SCALE_H_V_AB:
	    menu_tgc_scale_h_v_ab(s);
	    break;
	case MENU_TOR_R1:
	    menu_tor_r1(s);
	    break;
	case MENU_TOR_R2:
	    menu_tor_r2(s);
	    break;
	case MENU_ETO_R:
	    menu_eto_r(s);
	    break;
	case MENU_ETO_RD:
	    menu_eto_rd(s);
	    break;
	case MENU_ETO_SCALE_C:
	    menu_eto_scale_c(s);
	    break;
	case MENU_RPC_B:
	    menu_rpc_b(s);
	    break;
	case MENU_RPC_H:
	    menu_rpc_h(s);
	    break;
	case MENU_RPC_R:
	    menu_rpc_r(s);
	    break;
	case MENU_RHC_B:
	    menu_rhc_b(s);
	    break;
	case MENU_RHC_H:
	    menu_rhc_h(s);
	    break;
	case MENU_RHC_R:
	    menu_rhc_r(s);
	    break;
	case MENU_RHC_C:
	    menu_rhc_c(s);
	    break;
	case MENU_EPA_H:
	    menu_epa_h(s);
	    break;
	case MENU_EPA_R1:
	    menu_epa_r1(s);
	    break;
	case MENU_EPA_R2:
	    menu_epa_r2(s);
	    break;
	case MENU_EHY_H:
	    menu_ehy_h(s);
	    break;
	case MENU_EHY_R1:
	    menu_ehy_r1(s);
	    break;
	case MENU_EHY_R2:
	    menu_ehy_r2(s);
	    break;
	case MENU_EHY_C:
	    menu_ehy_c(s);
	    break;
	case MENU_HYP_H:
	    menu_hyp_h(s);
	    break;
	case MENU_HYP_SCALE_A:
	    menu_hyp_scale_a(s);
	    break;
	case MENU_HYP_SCALE_B:
	    menu_hyp_scale_b(s);
	    break;
	case MENU_HYP_C:
	    menu_hyp_c(s);
	    break;
	case MENU_TGC_SCALE_A:
	    menu_tgc_scale_a(s);
	    break;
	case MENU_TGC_SCALE_B:
	    menu_tgc_scale_b(s);
	    break;
	case MENU_ELL_SCALE_A:
	    menu_ell_scale_a(s);
	    break;
	case MENU_ELL_SCALE_B:
	    menu_ell_scale_b(s);
	    break;
	case MENU_ELL_SCALE_C:
	    menu_ell_scale_c(s);
	    break;
	case MENU_TGC_SCALE_C:
	    menu_tgc_scale_c(s);
	    break;
	case MENU_TGC_SCALE_D:
	    menu_tgc_scale_d(s);
	    break;
	case MENU_TGC_SCALE_AB:
	    menu_tgc_scale_ab(s);
	    break;
	case MENU_TGC_SCALE_CD:
    	    menu_tgc_scale_cd(s);
	    break;
	case MENU_TGC_SCALE_ABCD:
    	    menu_tgc_scale_abcd(s);
	    break;
	case MENU_ELL_SCALE_ABC:
	    menu_ell_scale_abc(s);
	    break;
	    /* begin super ellipse menu options */
	case MENU_SUPERELL_SCALE_A:
	    menu_superell_scale_a(s);
	    break;
	case MENU_SUPERELL_SCALE_B:
	    menu_superell_scale_b(s);
	    break;
	case MENU_SUPERELL_SCALE_C:
	    menu_superell_scale_c(s);
	    break;
	case MENU_SUPERELL_SCALE_ABC:
    	    menu_superell_scale_abc(s);
	    break;
	case MENU_PIPE_PT_OD:	/* scale OD of one pipe segment */
	    menu_pipe_pt_od(s);
	    break;
	case MENU_PIPE_PT_ID:	/* scale ID of one pipe segment */
	    menu_pipe_pt_od(s);
	    break;
	case MENU_PIPE_PT_RADIUS:	/* scale bend radius at selected point */
	    menu_pipe_pt_radius(s);
	    break;
	case MENU_PIPE_SCALE_OD:	/* scale entire pipe OD */
	    menu_pipe_scale_od(s);
	    break;
	case MENU_PIPE_SCALE_ID:	/* scale entire pipe ID */
	    menu_pipe_scale_id(s);
	    break;
	case MENU_PIPE_SCALE_RADIUS:	/* scale entire pipr bend radius */
	    menu_pipe_scale_radius(s);
	    break;
	case MENU_PART_H:
	    menu_part_h(s);
	    break;
	case MENU_PART_v:
	    menu_part_v(s);
	    break;
	case MENU_PART_h:
	    menu_part_h_end_r(s);
	    break;
	case MENU_METABALL_SET_THRESHOLD:
	    menu_metaball_set_threshold(s);
	    break;
	case MENU_METABALL_SET_METHOD:
	    menu_metaball_set_method(s);
	    break;
	case MENU_METABALL_PT_SET_GOO:
	    menu_metaball_pt_set_goo(s);
	    break;
	case MENU_METABALL_PT_FLDSTR:
	    menu_metaball_pt_fldstr(s);
	    break;
    }
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
    struct rt_arb_internal *arb;
    static vect_t work;
    mat_t mat;
    mat_t mat1;
    mat_t edit;
    point_t rot_point;

    if (s->dbip == DBI_NULL)
	return;

    sedraw = 0;
    ++update_views;

    switch (es_edflag) {

	case IDLE:
	    /* do nothing more */
	    --update_views;
	    break;

	case ECMD_DSP_SCALE_X:
	    ecmd_dsp_scale_x(s);
	    break;
	case ECMD_DSP_SCALE_Y:
	    ecmd_dsp_scale_y(s);
	    break;
	case ECMD_DSP_SCALE_ALT:
	    ecmd_dsp_scale_alt(s);
	    break;
	case ECMD_DSP_FNAME:
	    if (ecmd_dsp_fname(s) != BRLCAD_OK)
		return;
	    break;
	case ECMD_EBM_FSIZE:	/* set file size */
	    if (ecmd_ebm_fsize(s) != BRLCAD_OK)
		return;
	    break;

	case ECMD_EBM_FNAME:
	    if (ecmd_ebm_fname(s) != BRLCAD_OK)
		return;
	    break;
	case ECMD_EBM_HEIGHT:	/* set extrusion depth */
	     if (ecmd_ebm_height(s) != BRLCAD_OK)
		return;
	    break;
	case ECMD_VOL_CSIZE:
    	    ecmd_vol_csize(s);
	    break;
	case ECMD_VOL_FSIZE:
    	    ecmd_vol_fsize(s);
	    break;
	case ECMD_VOL_THRESH_LO:
	    ecmd_vol_thresh_lo(s);
	    break;
	case ECMD_VOL_THRESH_HI:
	    ecmd_vol_thresh_hi(s);
	    break;
	case ECMD_VOL_FNAME:
	    ecmd_vol_fname(s);
	    break;
	case ECMD_BOT_MODE:
	    ecmd_bot_mode(s);
	    break;
	case ECMD_BOT_ORIENT:
	    ecmd_bot_orient(s);
	    break;
	case ECMD_BOT_THICK:
	    ecmd_bot_thick(s);
	    break;
	case ECMD_BOT_FLAGS:
	    ecmd_bot_flags(s);
	    break;
	case ECMD_BOT_FMODE:
	    ecmd_bot_fmode(s);
	    break;
	case ECMD_BOT_FDEL:
	    if (ecmd_bot_fdel(s) != BRLCAD_OK)
		return;
	    break;
	case ECMD_EXTR_SKT_NAME:
	    ecmd_extr_skt_name(s);
	    break;
	case ECMD_EXTR_MOV_H:
	    ecmd_extr_mov_h(s);
	    break;
	case ECMD_EXTR_SCALE_H:
	    ecmd_extr_scale_h(s);
	    break;
	case ECMD_ARB_MAIN_MENU:
	    ecmd_arb_main_menu(s);
	    break;
	case ECMD_ARB_SPECIFIC_MENU:
	    if (ecmd_arb_specific_menu(s) != BRLCAD_OK)
		return;
	    break;
	case ECMD_ARB_MOVE_FACE:
	    ecmd_arb_move_face(s);
	    break;
	case ECMD_ARB_SETUP_ROTFACE:
	    ecmd_arb_setup_rotface(s);
	    break;
	case ECMD_ARB_ROTATE_FACE:
	    ecmd_arb_rotate_face(s);
	    return;
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    {
		mat_t scalemat;

		es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
		es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
		es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
		bot_verts[0] = -1;
		bot_verts[1] = -1;
		bot_verts[2] = -1;
		if (inpara) {
		    /* accumulate the scale factor */
		    s->edit_state.es_scale = es_para[0] / acc_sc_sol;
		    acc_sc_sol = es_para[0];
		}

		bn_mat_scale_about_pnt(scalemat, es_keypoint, s->edit_state.es_scale);
		bn_mat_mul(mat1, scalemat, es_mat);
		bn_mat_mul(mat, es_invmat, mat1);
		transform_editing_solid(s, &s->edit_state.es_int, mat, &s->edit_state.es_int, 1);

		/* reset solid scale factor */
		s->edit_state.es_scale = 1.0;
	    }
	    break;

	case STRANS:
	    /* translate solid */
	    {
		vect_t delta;

		es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
		es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
		es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
		bot_verts[0] = -1;
		bot_verts[1] = -1;
		bot_verts[2] = -1;
		if (inpara) {
		    /* Need vector from current vertex/keypoint
		     * to desired new location.
		     */
		    if (mged_variables->mv_context) {
			/* move solid so that es_keypoint is at position es_para */
			vect_t raw_para;

			MAT4X3PNT(raw_para, es_invmat, es_para);
			MAT4X3PNT(work, es_invmat, es_keypoint);
			VSUB2(delta, work, raw_para);
			MAT_IDN(mat);
			MAT_DELTAS_VEC_NEG(mat, delta);
		    } else {
			/* move solid to position es_para */
			/* move solid to position es_para */
			MAT4X3PNT(work, es_invmat, es_keypoint);
			VSUB2(delta, work, es_para);
			MAT_IDN(mat);
			MAT_DELTAS_VEC_NEG(mat, delta);
		    }
		    transform_editing_solid(s, &s->edit_state.es_int, mat, &s->edit_state.es_int, 1);
		}
	    }
	    break;
	case ECMD_VTRANS:
	    // I think this is bspline only??
	    ecmd_vtrans(s);
	    break;
	case ECMD_CLINE_SCALE_H:
	    ecmd_cline_scale_h(s);
	    break;
	case ECMD_CLINE_SCALE_R:
	    ecmd_cline_scale_r(s);
	    break;
	case ECMD_CLINE_SCALE_T:
	    ecmd_cline_scale_t(s);
	    break;
	case ECMD_CLINE_MOVE_H:
	    ecmd_cline_move_h(s);
	    break;
	case ECMD_TGC_MV_H:
	    ecmd_tgc_mv_h(s);
	    break;

	case ECMD_TGC_MV_HH:
	    ecmd_tgc_mv_hh(s);
	    break;
	case PSCALE:
	    es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
	    bot_verts[0] = -1;
	    bot_verts[1] = -1;
	    bot_verts[2] = -1;
	    pscale(s);
	    break;

	case PTARB:	/* move an ARB point */
	case EARB:      /* edit an ARB edge */
	    edit_arb_element(s);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    {
		es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
		es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
		es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
		bot_verts[0] = -1;
		bot_verts[1] = -1;
		bot_verts[2] = -1;
		if (inpara) {
		    static mat_t invsolr;
		    /*
		     * Keyboard parameters:  absolute x, y, z rotations,
		     * in degrees.  First, cancel any existing rotations,
		     * then perform new rotation
		     */
		    bn_mat_inv(invsolr, acc_rot_sol);

		    /* Build completely new rotation change */
		    MAT_IDN(modelchanges);
		    bn_mat_angles(modelchanges,
				  es_para[0],
				  es_para[1],
				  es_para[2]);
		    /* Borrow incr_change matrix here */
		    bn_mat_mul(incr_change, modelchanges, invsolr);
		    MAT_COPY(acc_rot_sol, modelchanges);

		    /* Apply new rotation to solid */
		    /* Clear out solid rotation */
		    MAT_IDN(modelchanges);
		} else {
		    /* Apply incremental changes already in incr_change */
		}
		/* Apply changes to solid */
		/* xlate keypoint to origin, rotate, then put back. */
		switch (mged_variables->mv_rotate_about) {
		    case 'v':       /* View Center */
			VSET(work, 0.0, 0.0, 0.0);
			MAT4X3PNT(rot_point, view_state->vs_gvp->gv_view2model, work);
			break;
		    case 'e':       /* Eye */
			VSET(work, 0.0, 0.0, 1.0);
			MAT4X3PNT(rot_point, view_state->vs_gvp->gv_view2model, work);
			break;
		    case 'm':       /* Model Center */
			VSETALL(rot_point, 0.0);
			break;
		    case 'k':       /* Key Point */
		    default:
			VMOVE(rot_point, es_keypoint);
			break;
		}

		if (mged_variables->mv_context) {
		    /* calculate rotations about keypoint */
		    bn_mat_xform_about_pnt(edit, incr_change, rot_point);

		    /* We want our final matrix (mat) to xform the original solid
		     * to the position of this instance of the solid, perform the
		     * current edit operations, then xform back.
		     * mat = es_invmat * edit * es_mat
		     */
		    bn_mat_mul(mat1, edit, es_mat);
		    bn_mat_mul(mat, es_invmat, mat1);
		} else {
		    MAT4X3PNT(work, es_invmat, rot_point);
		    bn_mat_xform_about_pnt(mat, incr_change, work);
		}
		transform_editing_solid(s, &s->edit_state.es_int, mat, &s->edit_state.es_int, 1);

		MAT_IDN(incr_change);
	    }
	    break;

	case ECMD_EXTR_ROT_H:
	    ecmd_extr_rot_h(s);
	    break;
	case ECMD_TGC_ROT_H:
	    ecmd_tgc_rot_h(s);
	    break;
	case ECMD_TGC_ROT_AB:
	    ecmd_tgc_rot_ab(s);
	    break;
	case ECMD_HYP_ROT_H:
	    ecmd_hyp_rot_h(s);
	    break;
	case ECMD_ETO_ROT_C:
	    ecmd_eto_rot_c(s);
	    break;
	case ECMD_NMG_EPICK:
	    /* XXX Nothing to do here (yet), all done in mouse routine. */
	    break;
	case ECMD_NMG_EMOVE:
	    ecmd_nmg_emove(s);
	    break;
	case ECMD_NMG_EKILL:
	    ecmd_nmg_ekill(s);
	    /* fall through */
	case ECMD_NMG_ESPLIT:
	    ecmd_nmg_esplit(s);
	    break;
	case ECMD_NMG_LEXTRU:
	    ecmd_nmg_lextru(s);
	    break;
	case ECMD_PIPE_PICK:
	    ecmd_pipe_pick(s);
	    break;
	case ECMD_PIPE_SPLIT:
	    ecmd_pipe_split(s);
	    break;
	case ECMD_PIPE_PT_MOVE:
	    ecmd_pipe_pt_move(s);
	    break;
	case ECMD_PIPE_PT_ADD:
	    ecmd_pipe_pt_add(s);
	    break;
	case ECMD_PIPE_PT_INS:
	    ecmd_pipe_pt_ins(s);
	    break;
	case ECMD_PIPE_PT_DEL:
	    ecmd_pipe_pt_del(s);
	    break;
	case ECMD_ARS_PICK_MENU:
	    /* put up point pick menu for ARS solid */
	    menu_state->ms_flag = 0;
	    es_edflag = ECMD_ARS_PICK;
	    mmenu_set(s, MENU_L1, ars_pick_menu);
	    break;
	case ECMD_ARS_EDIT_MENU:
	    /* put up main ARS edit menu */
	    menu_state->ms_flag = 0;
	    es_edflag = IDLE;
	    mmenu_set(s, MENU_L1, ars_menu);
	    break;
	case ECMD_ARS_PICK:
	    ecmd_ars_pick(s);
	    break;
	case ECMD_ARS_NEXT_PT:
	    ecmd_ars_next_pt(s);
	    break;
	case ECMD_ARS_PREV_PT:
	    ecmd_ars_prev_pt(s);
	    break;
	case ECMD_ARS_NEXT_CRV:
	    ecmd_ars_next_crv(s);
	    break;
	case ECMD_ARS_PREV_CRV:
	    ecmd_ars_prev_crv(s);
	    break;
	case ECMD_ARS_DUP_CRV:
	    ecmd_ars_dup_crv(s);
	    break;
	case ECMD_ARS_DUP_COL:
	    ecmd_ars_dup_col(s);
	    break;
	case ECMD_ARS_DEL_CRV:
	    ecmd_ars_del_crv(s);
	    break;
	case ECMD_ARS_DEL_COL:
	    ecmd_ars_del_col(s);
	    break;
	case ECMD_ARS_MOVE_COL:
	    ecmd_ars_move_col(s);
	    break;
	case ECMD_ARS_MOVE_CRV:
	    ecmd_ars_move_crv(s);
	    break;
	case ECMD_ARS_MOVE_PT:
	    ecmd_ars_move_pt(s);
	    break;
	case ECMD_BOT_MOVEV:
	    ecmd_bot_movev(s);
	    break;
	case ECMD_BOT_MOVEE:
	    ecmd_bot_movee(s);
	    break;
	case ECMD_BOT_MOVET:
	    ecmd_bot_movet(s);
	    break;
	case ECMD_BOT_PICKV:
	case ECMD_BOT_PICKE:
	case ECMD_BOT_PICKT:
	    break;

	case ECMD_METABALL_PT_PICK:
	    ecmd_metaball_pt_pick(s);
	    break;
	case ECMD_METABALL_PT_MOV:
	    ecmd_metaball_pt_mov(s);
	    break;
	case ECMD_METABALL_PT_DEL:
	    ecmd_metaball_pt_del(s);
	    break;
	case ECMD_METABALL_PT_ADD:
	    ecmd_metaball_pt_add(s);
	    break;

	default:
	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		bu_vls_printf(&tmp_vls, "sedit(s):  unknown edflag = %d.\n", es_edflag);
		Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		mged_print_result(s, TCL_ERROR);
		bu_vls_free(&tmp_vls);
	    }
    }

    /* must re-calculate the face plane equations for arbs */
    if (s->edit_state.es_int.idb_type == ID_ARB8) {
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;

	arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	if (rt_arb_calc_planes(&error_msg, arb, es_type, es_peqn, &s->tol.tol) < 0)
	    Tcl_AppendResult(s->interp, bu_vls_addr(&error_msg), (char *)0);
	bu_vls_free(&error_msg);
    }

    /* If the keypoint changed location, find about it here */
    if (!es_keyfixed)
	get_solid_keypoint(s, &es_keypoint, &es_keytag, &s->edit_state.es_int, es_mat);

    set_e_axes_pos(s, 0);
    replot_editing_solid(s);

    if (update_views) {
	dm_set_dirty(DMP, 1);
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "active_edit_callback");
	(void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }

    inpara = 0;
    es_mvalid = 0;
}


void
update_edit_absolute_tran(struct mged_state *s, vect_t view_pos)
{
    vect_t model_pos;
    vect_t ea_view_pos;
    vect_t diff;
    fastf_t inv_Viewscale = 1/view_state->vs_gvp->gv_scale;

    MAT4X3PNT(model_pos, view_state->vs_gvp->gv_view2model, view_pos);
    VSUB2(diff, model_pos, e_axes_pos);
    VSCALE(s->edit_state.edit_absolute_model_tran, diff, inv_Viewscale);
    VMOVE(s->edit_state.last_edit_absolute_model_tran, s->edit_state.edit_absolute_model_tran);

    MAT4X3PNT(ea_view_pos, view_state->vs_gvp->gv_model2view, e_axes_pos);
    VSUB2(s->edit_state.edit_absolute_view_tran, view_pos, ea_view_pos);
    VMOVE(s->edit_state.last_edit_absolute_view_tran, s->edit_state.edit_absolute_view_tran);
}


/*
 * Mouse (pen) press in graphics area while doing Solid Edit.
 * mousevec [X] and [Y] are in the range -1.0...+1.0, corresponding
 * to viewspace.
 *
 * In order to allow the "p" command to do the same things that
 * a mouse event can, the preferred strategy is to store the value
 * corresponding to what the "p" command would give in es_mparam,
 * set es_mvalid = 1, set sedraw = 1, and return, allowing sedit(s)
 * to actually do the work.
 */
void
sedit_mouse(struct mged_state *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;
    vect_t raw_kp = VINIT_ZERO;        	/* es_keypoint with es_invmat applied */
    vect_t raw_mp = VINIT_ZERO;        	/* raw model position */
    mat_t mat;

    if (es_edflag <= 0)
	return;

    switch (es_edflag) {
	case SSCALE:
	case PSCALE:
	case ECMD_DSP_SCALE_X:
	case ECMD_DSP_SCALE_Y:
	case ECMD_DSP_SCALE_ALT:
	case ECMD_VOL_CSIZE:
	case ECMD_VOL_THRESH_LO:
	case ECMD_VOL_THRESH_HI:
	case ECMD_EBM_HEIGHT:
	case ECMD_EXTR_SCALE_H:
	case ECMD_CLINE_SCALE_H:
	case ECMD_CLINE_SCALE_T:
	case ECMD_CLINE_SCALE_R:
	    /* use mouse to get a scale factor */
	    s->edit_state.es_scale = 1.0 + 0.25 * ((fastf_t)
				     (mousevec[Y] > 0 ? mousevec[Y] : -mousevec[Y]));
	    if (mousevec[Y] <= 0)
		s->edit_state.es_scale = 1.0 / s->edit_state.es_scale;

	    /* accumulate scale factor */
	    acc_sc_sol *= s->edit_state.es_scale;

	    s->edit_state.edit_absolute_scale = acc_sc_sol - 1.0;
	    if (s->edit_state.edit_absolute_scale > 0)
		s->edit_state.edit_absolute_scale /= 3.0;

	    sedit(s);

	    return;
	case STRANS:
	    /*
	     * Use mouse to change solid's location.
	     * Project solid's keypoint into view space,
	     * replace X, Y (but NOT Z) components, and
	     * project result back to model space.
	     * Then move keypoint there.
	     */
	    {
		point_t pt;
		vect_t delta;

		MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
		pos_view[X] = mousevec[X];
		pos_view[Y] = mousevec[Y];
		MAT4X3PNT(pt, view_state->vs_gvp->gv_view2model, pos_view);

		/* Need vector from current vertex/keypoint
		 * to desired new location.
		 */
		MAT4X3PNT(raw_mp, es_invmat, pt);
		MAT4X3PNT(raw_kp, es_invmat, curr_e_axes_pos);
		VSUB2(delta, raw_kp, raw_mp);
		MAT_IDN(mat);
		MAT_DELTAS_VEC_NEG(mat, delta);
		transform_editing_solid(s, &s->edit_state.es_int, mat, &s->edit_state.es_int, 1);
	    }

	    break;
	case ECMD_VTRANS:
	    /*
	     * Use mouse to change a vertex location.
	     * Project vertex (in solid keypoint) into view space,
	     * replace X, Y (but NOT Z) components, and
	     * project result back to model space.
	     * Leave desired location in es_mparam.
	     */

	    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
	    pos_view[X] = mousevec[X];
	    pos_view[Y] = mousevec[Y];
	    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
	    MAT4X3PNT(es_mparam, es_invmat, temp);
	    es_mvalid = 1;	/* es_mparam is valid */
	    /* Leave the rest to code in sedit(s) */

	    break;
	case ECMD_TGC_MV_H:
	case ECMD_TGC_MV_HH:
	    ecmd_tgc_mv_h_mousevec(s, mousevec);
	    break;
	case ECMD_EXTR_MOV_H:
	    ecmd_extr_mov_h_mousevec(s, mousevec);
	    break;
	case ECMD_CLINE_MOVE_H:
	    ecmd_cline_move_h_mousevec(s, mousevec);
	    break;
	case PTARB:
	    arb_mv_pnt_to(s, mousevec);
	    break;
	case EARB:
	    edarb_mousevec(s, mousevec);
	    break;
	case ECMD_ARB_MOVE_FACE:
	    edarb_move_face_mousevec(s, mousevec);
	    break;
	case ECMD_BOT_PICKV:
	    if (ecmd_bot_pickv(s, mousevec) != BRLCAD_OK)
		return;
	    break;
	case ECMD_BOT_PICKE:
	    if (ecmd_bot_picke(s, mousevec) != BRLCAD_OK)
		return;
	    break;
	case ECMD_BOT_PICKT:
	    ecmd_bot_pickt(s, mousevec);
	    break;
	case ECMD_NMG_EPICK:
	    /* XXX Should just leave desired location in es_mparam for sedit(s) */
	    ecmd_nmg_epick(s, mousevec);
	    break;
	case ECMD_NMG_LEXTRU:
	case ECMD_NMG_EMOVE:
	case ECMD_NMG_ESPLIT:
	case ECMD_PIPE_PICK:
	case ECMD_PIPE_SPLIT:
	case ECMD_PIPE_PT_MOVE:
	case ECMD_PIPE_PT_ADD:
	case ECMD_PIPE_PT_INS:
	case ECMD_ARS_PICK:
	case ECMD_ARS_MOVE_PT:
	case ECMD_ARS_MOVE_CRV:
	case ECMD_ARS_MOVE_COL:
	case ECMD_BOT_MOVEV:
	case ECMD_BOT_MOVEE:
	case ECMD_BOT_MOVET:
	case ECMD_METABALL_PT_PICK:
	case ECMD_METABALL_PT_MOV:
	case ECMD_METABALL_PT_ADD:

	    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
	    pos_view[X] = mousevec[X];
	    pos_view[Y] = mousevec[Y];
	    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
	    MAT4X3PNT(es_mparam, es_invmat, temp);
	    es_mvalid = 1;

	    break;
	default:
	    Tcl_AppendResult(s->interp, "mouse press undefined in this solid edit mode\n", (char *)NULL);
	    mged_print_result(s, TCL_ERROR);
	    return;
    }

    update_edit_absolute_tran(s, pos_view);
    sedit(s);
}


void
sedit_abs_scale(struct mged_state *s)
{
    fastf_t old_acc_sc_sol;

    if (es_edflag != SSCALE && es_edflag != PSCALE)
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
	VMOVE(temp, es_keypoint);
	MAT4X3PNT(pos_model, modelchanges, temp);
	wrt_point(modelchanges, incr_change, modelchanges, pos_model);

	MAT_IDN(incr_change);
	new_edit_mats(s);
    } else if (movedir & (RARROW|UARROW)) {
	mat_t oldchanges;	/* temporary matrix */

	/* Vector from object keypoint to cursor */
	VMOVE(temp, es_keypoint);
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
    VMOVE(temp, es_keypoint);
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
    es_menu = 0;
    es_edflag = -1;
    MAT_IDN(es_mat);

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
	VMOVE(es_keypoint, illump->s_center);

	/* The s_center takes the es_mat into account already */
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

	es_type = rt_arb_std_type(&s->edit_state.es_int, &s->tol.tol);
    }

    /* Save aggregate path matrix */
    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, es_mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(es_invmat, es_mat);

    get_solid_keypoint(s, &es_keypoint, &strp, &s->edit_state.es_int, es_mat);
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

    es_edclass = EDIT_CLASS_NULL;

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
 * 'es_menu' which is the plane being redefined. This function is only callable
 * when in solid edit and rotating the face of a GENARB8.
 */
int
f_eqn(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    short int i;
    vect_t tempvec;
    struct rt_arb_internal *arb;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 4 || 4 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help eqn");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (GEOM_EDIT_STATE != ST_S_EDIT) {
	Tcl_AppendResult(interp, "Eqn: must be in solid edit\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (s->edit_state.es_int.idb_type != ID_ARB8) {
	Tcl_AppendResult(interp, "Eqn: type must be GENARB8\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (es_edflag != ECMD_ARB_ROTATE_FACE) {
	Tcl_AppendResult(interp, "Eqn: must be rotating a face\n", (char *)NULL);
	return TCL_ERROR;
    }

    arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    /* get the A, B, C from the command line */
    for (i=0; i<3; i++)
	es_peqn[es_menu][i]= atof(argv[i+1]);
    VUNITIZE(&es_peqn[es_menu][0]);

    VMOVE(tempvec, arb->pt[fixv]);
    es_peqn[es_menu][W]=VDOT(es_peqn[es_menu], tempvec);

    if (rt_arb_calc_points(arb, es_type, (const plane_t *)es_peqn, &s->tol.tol))
	return CMD_BAD;

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
	es_edflag = -1;
	es_edclass = EDIT_CLASS_NULL;

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
    es_edflag = -1;
    es_edclass = EDIT_CLASS_NULL;

    rt_db_free_internal(&s->edit_state.es_int);
}


int
mged_param(struct mged_state *s, Tcl_Interp *interp, int argc, fastf_t *argvect)
{
    int i;

    CHECK_DBI_NULL;

    if (es_edflag <= 0) {
	Tcl_AppendResult(interp,
			 "A solid editor option not selected\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    inpara = 0;
    for (i = 0; i < argc; i++) {
	es_para[ inpara++ ] = argvect[i];
    }

    if (PARAM_1ARG) {
	if (inpara != 1) {
	    Tcl_AppendResult(interp, "ERROR: only one argument needed\n", (char *)NULL);
	    inpara = 0;
	    return TCL_ERROR;
	}

	if (es_menu == MENU_PIPE_PT_OD || es_menu == MENU_PIPE_PT_ID || es_menu == MENU_PIPE_SCALE_ID
	    || es_menu == MENU_METABALL_SET_THRESHOLD || es_menu == MENU_METABALL_SET_METHOD
	    || es_menu == MENU_METABALL_PT_SET_GOO)
	{
	    if (es_para[0] < 0.0) {
		Tcl_AppendResult(interp, "ERROR: SCALE FACTOR < 0\n", (char *)NULL);
		inpara = 0;
		return TCL_ERROR;
	    }
	} else {
	    if (es_para[0] <= 0.0) {
		Tcl_AppendResult(interp, "ERROR: SCALE FACTOR <= 0\n", (char *)NULL);
		inpara = 0;
		return TCL_ERROR;
	    }
	}
    } else if (PARAM_2ARG) {
	if (inpara != 2) {
	    Tcl_AppendResult(interp, "ERROR: two arguments needed\n", (char *)NULL);
	    inpara = 0;
	    return TCL_ERROR;
	}

	if (es_para[0] <= 0.0) {
	    Tcl_AppendResult(interp, "ERROR: X SIZE <= 0\n", (char *)NULL);
	    inpara = 0;
	    return TCL_ERROR;
	} else if (es_para[1] <= 0.0) {
	    Tcl_AppendResult(interp, "ERROR: Y SIZE <= 0\n", (char *)NULL);
	    inpara = 0;
	    return TCL_ERROR;
	}
    } else {
	if (inpara != 3) {
	    Tcl_AppendResult(interp, "ERROR: three arguments needed\n", (char *)NULL);
	    inpara = 0;
	    return TCL_ERROR;
	}
    }

    /* check if need to convert input values to the base unit */
    switch (es_edflag) {

	case STRANS:
	case ECMD_VTRANS:
	case PSCALE:
	case EARB:
	case ECMD_ARB_MOVE_FACE:
	case ECMD_TGC_MV_H:
	case ECMD_TGC_MV_HH:
	case PTARB:
	case ECMD_NMG_ESPLIT:
	case ECMD_NMG_EMOVE:
	case ECMD_NMG_LEXTRU:
	case ECMD_PIPE_PICK:
	case ECMD_PIPE_SPLIT:
	case ECMD_PIPE_PT_MOVE:
	case ECMD_PIPE_PT_ADD:
	case ECMD_PIPE_PT_INS:
	case ECMD_ARS_PICK:
	case ECMD_ARS_MOVE_PT:
	case ECMD_ARS_MOVE_CRV:
	case ECMD_ARS_MOVE_COL:
	case ECMD_VOL_CSIZE:
	case ECMD_DSP_SCALE_X:
	case ECMD_DSP_SCALE_Y:
	case ECMD_DSP_SCALE_ALT:
	case ECMD_EBM_HEIGHT:
	case ECMD_EXTR_SCALE_H:
	case ECMD_CLINE_SCALE_H:
	case ECMD_CLINE_SCALE_T:
	case ECMD_CLINE_SCALE_R:
	case ECMD_CLINE_MOVE_H:
	case ECMD_EXTR_MOV_H:
	case ECMD_BOT_THICK:
	case ECMD_BOT_MOVET:
	case ECMD_BOT_MOVEE:
	case ECMD_BOT_MOVEV:
	case ECMD_METABALL_PT_PICK:
	case ECMD_METABALL_PT_MOV:
	case ECMD_METABALL_PT_ADD:
	    /* must convert to base units */
	    es_para[0] *= s->dbip->dbi_local2base;
	    es_para[1] *= s->dbip->dbi_local2base;
	    es_para[2] *= s->dbip->dbi_local2base;
	    /* fall through */
	default:
	    break;
    }

    sedit(s);

    if (SEDIT_TRAN) {
	vect_t diff;
	fastf_t inv_Viewscale = 1/view_state->vs_gvp->gv_scale;

	VSUB2(diff, es_para, e_axes_pos);
	VSCALE(s->edit_state.edit_absolute_model_tran, diff, inv_Viewscale);
	VMOVE(s->edit_state.last_edit_absolute_model_tran, s->edit_state.edit_absolute_model_tran);
    } else if (SEDIT_ROTATE) {
	VMOVE(s->edit_state.edit_absolute_model_rotate, es_para);
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

    if ((GEOM_EDIT_STATE != ST_S_EDIT) && (GEOM_EDIT_STATE != ST_O_EDIT)) {
	state_err(s, "keypoint assignment");
	return TCL_ERROR;
    }

    switch (--argc) {
	case 0:
	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
		point_t key;

		VSCALE(key, es_keypoint, s->dbip->dbi_base2local);
		bu_vls_printf(&tmp_vls, "%s (%g, %g, %g)\n", es_keytag, V3ARGS(key));
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
	    }

	    break;
	case 3:
	    VSET(es_keypoint,
		 atof(argv[1]) * s->dbip->dbi_local2base,
		 atof(argv[2]) * s->dbip->dbi_local2base,
		 atof(argv[3]) * s->dbip->dbi_local2base);
	    es_keytag = "user-specified";
	    es_keyfixed = 1;
	    break;
	case 1:
	    if (BU_STR_EQUAL(argv[1], "reset")) {
		es_keytag = "";
		es_keyfixed = 0;
		get_solid_keypoint(s, &es_keypoint, &es_keytag,
				   &s->edit_state.es_int, es_mat);
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

    if (GEOM_EDIT_STATE != ST_S_EDIT)
	return TCL_ERROR;

    switch (s->edit_state.es_int.idb_type) {
	case ID_ARB8:
	    {
		struct bu_vls vls2 = BU_VLS_INIT_ZERO;

		/* title */
		bu_vls_printf(&vls, "{{ARB MENU} {}}");

		/* build "move edge" menu */
		mip = which_menu[es_type-4];
		/* submenu title */
		bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
		for (++mip; mip->menu_func != NULL; ++mip)
		    bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

		bu_vls_printf(&vls, " {{%s} {%s}}", cntrl_menu[1].menu_string, bu_vls_addr(&vls2));
		bu_vls_trunc(&vls2, 0);

		/* build "move face" menu */
		mip = which_menu[es_type+1];
		/* submenu title */
		bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
		for (++mip; mip->menu_func != NULL; ++mip)
		    bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

		bu_vls_printf(&vls, " {{%s} {%s}}", cntrl_menu[2].menu_string, bu_vls_addr(&vls2));
		bu_vls_trunc(&vls2, 0);

		/* build "rotate face" menu */
		mip = which_menu[es_type+6];
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

    if (GEOM_EDIT_STATE != ST_S_EDIT || !illump) {
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
    transform_editing_solid(s, &ces_int, es_mat, &s->edit_state.es_int, 0);

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

    if (GEOM_EDIT_STATE != ST_S_EDIT) {
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
	transform_editing_solid(s, &s->edit_state.es_int, es_invmat, &s->edit_state.es_int, 1);

    /* must re-calculate the face plane equations for arbs */
    if (s->edit_state.es_int.idb_type == ID_ARB8) {
	struct rt_arb_internal *arb;
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;

	arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	if (rt_arb_calc_planes(&error_msg, arb, es_type, es_peqn, &s->tol.tol) < 0)
	    Tcl_AppendResult(interp, bu_vls_addr(&error_msg), (char *)0);
	bu_vls_free(&error_msg);
    }

    if (!es_keyfixed)
	get_solid_keypoint(s, &es_keypoint, &es_keytag, &s->edit_state.es_int, es_mat);

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

    if (GEOM_EDIT_STATE != ST_S_EDIT || !illump)
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
    es_keytag = "";
    get_solid_keypoint(s, &es_keypoint, &es_keytag, &s->edit_state.es_int, es_mat);

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

    if (GEOM_EDIT_STATE != ST_O_EDIT)
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
    MAT_IDN(es_mat);
    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, es_mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(es_invmat, es_mat);

    get_solid_keypoint(s, &es_keypoint, &strp, &s->edit_state.es_int, es_mat);
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
