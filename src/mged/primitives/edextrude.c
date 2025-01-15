/*                      E D E X T R U D E . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2024 United States Government as represented by
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
/** @file mged/primitives/edextrude.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../mged.h"
#include "../sedit.h"
#include "../mged_dm.h"
#include "./mged_functab.h"

#define ECMD_EXTR_SCALE_H	27073	/* scale extrusion vector */
#define ECMD_EXTR_MOV_H		27074	/* move end of extrusion vector */
#define ECMD_EXTR_ROT_H		27075	/* rotate extrusion vector */
#define ECMD_EXTR_SKT_NAME	27076	/* set sketch that the extrusion uses */

#define MENU_EXTR_SCALE_H	27103
#define MENU_EXTR_MOV_H		27104
#define MENU_EXTR_ROT_H		27105
#define MENU_EXTR_SKT_NAME	27106

/*ARGSUSED*/
static void
extr_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    s->s_edit.edit_flag = arg;

    switch (arg) {
	case ECMD_EXTR_ROT_H:
	    s->s_edit.solid_edit_rotate = 1;
	    s->s_edit.solid_edit_translate = 0;
	    s->s_edit.solid_edit_scale = 0;
	    s->s_edit.solid_edit_pick = 0;
	    break;
	case ECMD_EXTR_SCALE_H:
	    s->s_edit.solid_edit_rotate = 0;
	    s->s_edit.solid_edit_translate = 0;
	    s->s_edit.solid_edit_scale = 1;
	    s->s_edit.solid_edit_pick = 0;
	    break;
	case ECMD_EXTR_MOV_H:
	    s->s_edit.solid_edit_rotate = 0;
	    s->s_edit.solid_edit_translate = 1;
	    s->s_edit.solid_edit_scale = 0;
	    s->s_edit.solid_edit_pick = 0;
	    break;
	default:
	    mged_set_edflag(s, arg);
	    break;
    };

    sedit(s);
}
struct menu_item extr_menu[] = {
    { "EXTRUSION MENU",	NULL, 0 },
    { "Set H",		extr_ed, ECMD_EXTR_SCALE_H },
    { "Move End H",		extr_ed, ECMD_EXTR_MOV_H },
    { "Rotate H",		extr_ed, ECMD_EXTR_ROT_H },
    { "Referenced Sketch",	extr_ed, ECMD_EXTR_SKT_NAME },
    { "", NULL, 0 }
};

struct menu_item *
mged_extrude_menu_item(const struct bn_tol *UNUSED(tol))
{
    return extr_menu;
}

const char *
mged_extrude_keypoint(
	point_t *pt,
	const char *UNUSED(keystr),
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *tol)
{
    const char *strp = NULL;
    struct rt_extrude_internal *extr = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extr);
    if (extr->skt && extr->skt->verts) {
	static const char *vstr = "V1";
	strp = OBJ[ip->idb_type].ft_keypoint(pt, vstr, mat, ip, tol);
    } else {
	strp = OBJ[ip->idb_type].ft_keypoint(pt, NULL, mat, ip, tol);
    }
    return strp;
}

void
mged_extrude_e_axes_pos(
	struct mged_state *s,
	const struct rt_db_internal *ip,
	const struct bn_tol *UNUSED(tol))
{
    if (s->s_edit.edit_flag == ECMD_EXTR_MOV_H) {
	struct rt_extrude_internal *extr = (struct rt_extrude_internal *)ip->idb_ptr;
	point_t extr_v;
	vect_t extr_h;

	RT_EXTRUDE_CK_MAGIC(extr);

	MAT4X3PNT(extr_v, s->s_edit.e_mat, extr->V);
	MAT4X3VEC(extr_h, s->s_edit.e_mat, extr->h);
	VADD2(s->s_edit.curr_e_axes_pos, extr_h, extr_v);
    } else {
	VMOVE(s->s_edit.curr_e_axes_pos, s->s_edit.e_keypoint);
    }
}

void
ecmd_extr_skt_name(struct mged_state *s)
{
    struct rt_extrude_internal *extr =
	(struct rt_extrude_internal *)s->s_edit.es_int.idb_ptr;
    const char *sketch_name;
    int ret_tcl;
    struct directory *dp;
    struct rt_db_internal tmp_ip;
    struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;

    RT_EXTRUDE_CK_MAGIC(extr);

    bu_vls_printf(&tcl_cmd, "cad_input_dialog .get_sketch_name $mged_gui(mged,screen) {Select Sketch} {Enter the name of the sketch to be extruded} final_sketch_name %s 0 {{summary \"Enter sketch name\"}} APPLY DISMISS",
	    extr->sketch_name);
    ret_tcl = Tcl_Eval(s->interp, bu_vls_addr(&tcl_cmd));
    if (ret_tcl != TCL_OK) {
	bu_log("ERROR: %s\n", Tcl_GetStringResult(s->interp));
	bu_vls_free(&tcl_cmd);
	return;
    }

    if (atoi(Tcl_GetStringResult(s->interp)) == 1)
	return;

    bu_vls_free(&tcl_cmd);

    sketch_name = Tcl_GetVar(s->interp, "final_sketch_name", TCL_GLOBAL_ONLY);
    if (extr->sketch_name)
	bu_free((char *)extr->sketch_name, "extr->sketch_name");
    extr->sketch_name = bu_strdup(sketch_name);

    if (extr->skt) {
	/* free the old sketch */
	RT_DB_INTERNAL_INIT(&tmp_ip);
	tmp_ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	tmp_ip.idb_type = ID_SKETCH;
	tmp_ip.idb_ptr = (void *)extr->skt;
	tmp_ip.idb_meth = &OBJ[ID_SKETCH];
	rt_db_free_internal(&tmp_ip);
    }

    if ((dp = db_lookup(s->dbip, sketch_name, 0)) == RT_DIR_NULL) {
	bu_log("Warning: %s does not exist!\n",
		sketch_name);
	extr->skt = (struct rt_sketch_internal *)NULL;
    } else {
	/* import the new sketch */

	if (rt_db_get_internal(&tmp_ip, dp, s->dbip, bn_mat_identity, &rt_uniresource) != ID_SKETCH) {
	    bu_log("rt_extrude_import: ERROR: Cannot import sketch (%.16s) for extrusion\n",
		    sketch_name);
	    extr->skt = (struct rt_sketch_internal *)NULL;
	} else {
	    extr->skt = (struct rt_sketch_internal *)tmp_ip.idb_ptr;
	}
    }
}

int
ecmd_extr_mov_h(struct mged_state *s)
{
    vect_t work;
    struct rt_extrude_internal *extr =
	(struct rt_extrude_internal *)s->s_edit.es_int.idb_ptr;

    RT_EXTRUDE_CK_MAGIC(extr);
    if (s->s_edit.e_inpara) {
	if (s->s_edit.e_inpara != 3) {
	    Tcl_AppendResult(s->interp, "ERROR: three arguments needed\n", (char *)NULL);
	    s->s_edit.e_inpara = 0;
	    return TCL_ERROR;
	}

	/* must convert to base units */
	s->s_edit.e_para[0] *= s->dbip->dbi_local2base;
	s->s_edit.e_para[1] *= s->dbip->dbi_local2base;
	s->s_edit.e_para[2] *= s->dbip->dbi_local2base;

	if (mged_variables->mv_context) {
	    /* apply s->s_edit.e_invmat to convert to real model coordinates */
	    MAT4X3PNT(work, s->s_edit.e_invmat, s->s_edit.e_para);
	    VSUB2(extr->h, work, extr->V);
	} else {
	    VSUB2(extr->h, s->s_edit.e_para, extr->V);
	}
    }

    /* check for zero H vector */
    if (MAGNITUDE(extr->h) <= SQRT_SMALL_FASTF) {
	Tcl_AppendResult(s->interp, "Zero H vector not allowed, resetting to +Z\n",
		(char *)NULL);
	mged_print_result(s, TCL_ERROR);
	VSET(extr->h, 0.0, 0.0, 1.0);
	return TCL_ERROR;
    }

    return 0;
}

int
ecmd_extr_scale_h(struct mged_state *s)
{
    if (s->s_edit.e_inpara != 1) {
	Tcl_AppendResult(s->interp, "ERROR: only one argument needed\n", (char *)NULL);
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    }

    if (s->s_edit.e_para[0] <= 0.0) {
	Tcl_AppendResult(s->interp, "ERROR: SCALE FACTOR <= 0\n", (char *)NULL);
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    }

    /* must convert to base units */
    s->s_edit.e_para[0] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[1] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[2] *= s->dbip->dbi_local2base;

    struct rt_extrude_internal *extr =
	(struct rt_extrude_internal *)s->s_edit.es_int.idb_ptr;

    RT_EXTRUDE_CK_MAGIC(extr);

    if (s->s_edit.e_inpara) {
	/* take s->s_edit.e_mat[15] (path scaling) into account */
	s->s_edit.e_para[0] *= s->s_edit.e_mat[15];
	s->s_edit.es_scale = s->s_edit.e_para[0] / MAGNITUDE(extr->h);
	VSCALE(extr->h, extr->h, s->s_edit.es_scale);
    } else if (s->s_edit.es_scale > 0.0) {
	VSCALE(extr->h, extr->h, s->s_edit.es_scale);
	s->s_edit.es_scale = 0.0;
    }

    return 0;
}

/* rotate height vector */
int
ecmd_extr_rot_h(struct mged_state *s)
{
    struct rt_extrude_internal *extr =
	(struct rt_extrude_internal *)s->s_edit.es_int.idb_ptr;
    mat_t mat;
    mat_t mat1;
    mat_t edit;

    RT_EXTRUDE_CK_MAGIC(extr);
    if (s->s_edit.e_inpara) {
	if (s->s_edit.e_inpara != 3) {
	    Tcl_AppendResult(s->interp, "ERROR: three arguments needed\n", (char *)NULL);
	    s->s_edit.e_inpara = 0;
	    return TCL_ERROR;
	}

	static mat_t invsolr;
	/*
	 * Keyboard parameters:  absolute x, y, z rotations,
	 * in degrees.  First, cancel any existing rotations,
	 * then perform new rotation
	 */
	bn_mat_inv(invsolr, s->edit_state.acc_rot_sol);

	/* Build completely new rotation change */
	MAT_IDN(s->edit_state.model_changes);
	bn_mat_angles(s->edit_state.model_changes,
		s->s_edit.e_para[0],
		s->s_edit.e_para[1],
		s->s_edit.e_para[2]);
	/* Borrow s->edit_state.incr_change matrix here */
	bn_mat_mul(s->edit_state.incr_change, s->edit_state.model_changes, invsolr);
	MAT_COPY(s->edit_state.acc_rot_sol, s->edit_state.model_changes);

	/* Apply new rotation to solid */
	/* Clear out solid rotation */
	MAT_IDN(s->edit_state.model_changes);
    } else {
	/* Apply incremental changes already in s->edit_state.incr_change */
    }

    if (mged_variables->mv_context) {
	/* calculate rotations about keypoint */
	bn_mat_xform_about_pnt(edit, s->edit_state.incr_change, s->s_edit.e_keypoint);

	/* We want our final matrix (mat) to xform the original solid
	 * to the position of this instance of the solid, perform the
	 * current edit operations, then xform back.
	 * mat = s->s_edit.e_invmat * edit * s->s_edit.e_mat
	 */
	bn_mat_mul(mat1, edit, s->s_edit.e_mat);
	bn_mat_mul(mat, s->s_edit.e_invmat, mat1);
	MAT4X3VEC(extr->h, mat, extr->h);
    } else {
	MAT4X3VEC(extr->h, s->edit_state.incr_change, extr->h);
    }

    MAT_IDN(s->edit_state.incr_change);

    return 0;
}

/* Use mouse to change location of point V+H */
void
ecmd_extr_mov_h_mousevec(struct mged_state *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t tr_temp = VINIT_ZERO;	/* temp translation vector */
    vect_t temp = VINIT_ZERO;
    struct rt_extrude_internal *extr =
	(struct rt_extrude_internal *)s->s_edit.es_int.idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extr);

    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->s_edit.curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    /* Do NOT change pos_view[Z] ! */
    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
    MAT4X3PNT(tr_temp, s->s_edit.e_invmat, temp);
    VSUB2(extr->h, tr_temp, extr->V);
}

int
mged_extrude_edit(struct mged_state *s, int edflag)
{
    switch (edflag) {
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    return mged_generic_sscale(s, &s->s_edit.es_int);
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->s_edit.es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->s_edit.es_int);
	    break;
	case ECMD_EXTR_SKT_NAME:
	    ecmd_extr_skt_name(s);
	    break;
	case ECMD_EXTR_MOV_H:
	    return ecmd_extr_mov_h(s);
	case ECMD_EXTR_SCALE_H:
	    return ecmd_extr_scale_h(s);
	case ECMD_EXTR_ROT_H:
	    return ecmd_extr_rot_h(s);
    }

    return 0;
}

int
mged_extrude_edit_xy(
	struct mged_state *s,
	int edflag,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->s_edit.es_int;

    switch (edflag) {
	case SSCALE:
	case PSCALE:
	case ECMD_EXTR_SCALE_H:
	    mged_generic_sscale_xy(s, mousevec);
	    mged_extrude_edit(s, edflag);
	    return 0;
	case STRANS:
	    mged_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	case ECMD_EXTR_MOV_H:
	    ecmd_extr_mov_h_mousevec(s, mousevec);
	    break;
	default:
	    Tcl_AppendResult(s->interp, "%s: XY edit undefined in solid edit mode %d\n", MGED_OBJ[ip->idb_type].ft_label,   edflag);
	    mged_print_result(s, TCL_ERROR);
	    return TCL_ERROR;
    }

    update_edit_absolute_tran(s, pos_view);
    mged_extrude_edit(s, edflag);

    return 0;
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
