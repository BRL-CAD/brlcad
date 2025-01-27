/*                         G E N E R I C . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2025 United States Government as represented by
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
/** @file mged/primitives/generic.c
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

const char *
mged_generic_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *tol)
{
    static const char *vert_str = "V";
    const char *strp = OBJ[ip->idb_type].ft_keypoint(pt, keystr, mat, ip, tol);
    if (strp)
	strp = OBJ[ip->idb_type].ft_keypoint(pt, vert_str, mat, ip, tol);
    return strp;
}

int
mged_generic_sscale(
	struct mged_state *s,
	struct rt_db_internal *ip
	)
{
    mat_t mat, mat1, scalemat;

    if (s->s_edit->e_inpara > 1) {
	bu_vls_printf(s->s_edit->log_str, "ERROR: only one argument needed\n");
	s->s_edit->e_inpara = 0;
	return TCL_ERROR;
    }

    if (s->s_edit->e_inpara) {
	/* accumulate the scale factor */
	s->s_edit->es_scale = s->s_edit->e_para[0] / s->s_edit->acc_sc_sol;
	s->s_edit->acc_sc_sol = s->s_edit->e_para[0];
    }

    bn_mat_scale_about_pnt(scalemat, s->s_edit->e_keypoint, s->s_edit->es_scale);
    bn_mat_mul(mat1, scalemat, s->s_edit->e_mat);
    bn_mat_mul(mat, s->s_edit->e_invmat, mat1);
    transform_editing_solid(s, ip, mat, ip, 1);

    /* reset solid scale factor */
    s->s_edit->es_scale = 1.0;

    return 0;
}

void
mged_generic_strans(
	struct mged_state *s,
	struct rt_db_internal *ip
	)
{
    mat_t mat;
    static vect_t work;
    vect_t delta;

    if (s->s_edit->e_inpara) {
	/* Need vector from current vertex/keypoint
	 * to desired new location.
	 */

	/* must convert to base units */
	s->s_edit->e_para[0] *= s->dbip->dbi_local2base;
	s->s_edit->e_para[1] *= s->dbip->dbi_local2base;
	s->s_edit->e_para[2] *= s->dbip->dbi_local2base;

	if (s->s_edit->mv_context) {
	    /* move solid so that s->s_edit->e_keypoint is at position s->s_edit->e_para */
	    vect_t raw_para;

	    MAT4X3PNT(raw_para, s->s_edit->e_invmat, s->s_edit->e_para);
	    MAT4X3PNT(work, s->s_edit->e_invmat, s->s_edit->e_keypoint);
	    VSUB2(delta, work, raw_para);
	    MAT_IDN(mat);
	    MAT_DELTAS_VEC_NEG(mat, delta);
	} else {
	    /* move solid to position s->s_edit->e_para */
	    /* move solid to position s->s_edit->e_para */
	    MAT4X3PNT(work, s->s_edit->e_invmat, s->s_edit->e_keypoint);
	    VSUB2(delta, work, s->s_edit->e_para);
	    MAT_IDN(mat);
	    MAT_DELTAS_VEC_NEG(mat, delta);
	}
	transform_editing_solid(s, ip, mat, ip, 1);
    }
}

void
mged_generic_srot(
	struct mged_state *s,
	struct rt_db_internal *ip
	)
{
    static vect_t work;
    point_t rot_point;
    mat_t mat, mat1, edit;

    if (s->s_edit->e_inpara) {
	static mat_t invsolr;
	/*
	 * Keyboard parameters:  absolute x, y, z rotations,
	 * in degrees.  First, cancel any existing rotations,
	 * then perform new rotation
	 */
	bn_mat_inv(invsolr, s->s_edit->acc_rot_sol);

	/* Build completely new rotation change */
	MAT_IDN(s->s_edit->model_changes);
	bn_mat_angles(s->s_edit->model_changes,
		s->s_edit->e_para[0],
		s->s_edit->e_para[1],
		s->s_edit->e_para[2]);
	/* Borrow s->s_edit->incr_change matrix here */
	bn_mat_mul(s->s_edit->incr_change, s->s_edit->model_changes, invsolr);
	MAT_COPY(s->s_edit->acc_rot_sol, s->s_edit->model_changes);

	/* Apply new rotation to solid */
	/* Clear out solid rotation */
	MAT_IDN(s->s_edit->model_changes);
    } else {
	/* Apply incremental changes already in s->s_edit->incr_change */
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
	    VMOVE(rot_point, s->s_edit->e_keypoint);
	    break;
    }

    if (s->s_edit->mv_context) {
	/* calculate rotations about keypoint */
	bn_mat_xform_about_pnt(edit, s->s_edit->incr_change, rot_point);

	/* We want our final matrix (mat) to xform the original solid
	 * to the position of this instance of the solid, perform the
	 * current edit operations, then xform back.
	 * mat = s->s_edit->e_invmat * edit * s->s_edit->e_mat
	 */
	bn_mat_mul(mat1, edit, s->s_edit->e_mat);
	bn_mat_mul(mat, s->s_edit->e_invmat, mat1);
    } else {
	MAT4X3PNT(work, s->s_edit->e_invmat, rot_point);
	bn_mat_xform_about_pnt(mat, s->s_edit->incr_change, work);
    }
    transform_editing_solid(s, ip, mat, ip, 1);

    MAT_IDN(s->s_edit->incr_change);
}

int
mged_generic_menu_str(struct bu_vls *mstr, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    if (!mstr || !ip)
	return BRLCAD_ERROR;

    struct menu_item *mip = NULL;

    if (MGED_OBJ[ip->idb_type].ft_menu_item)
	mip = (*MGED_OBJ[ip->idb_type].ft_menu_item)(tol);

    if (!mip)
       return BRLCAD_OK;

    /* title */
    bu_vls_printf(mstr, " {{%s} {}}", mip->menu_string);

    for (++mip; mip->menu_func != NULL; ++mip)
	bu_vls_printf(mstr, " {{%s} {}}", mip->menu_string);

    return BRLCAD_OK;
}

int
mged_generic_edit(
	struct mged_state *s,
	int edflag
	)
{
    switch (edflag) {
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    mged_generic_sscale(s, &s->s_edit->es_int);
	    break;
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->s_edit->es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->s_edit->es_int);
	    break;
    }
    return 0;
}

void
mged_generic_sscale_xy(
	struct mged_state *s,
	const vect_t mousevec
	)
{
    /* use mouse to get a scale factor */
    s->s_edit->es_scale = 1.0 + 0.25 * ((fastf_t)
	    (mousevec[Y] > 0 ? mousevec[Y] : -mousevec[Y]));
    if (mousevec[Y] <= 0)
	s->s_edit->es_scale = 1.0 / s->s_edit->es_scale;

    /* accumulate scale factor */
    s->s_edit->acc_sc_sol *= s->s_edit->es_scale;

    s->s_edit->edit_absolute_scale = s->s_edit->acc_sc_sol - 1.0;
    if (s->s_edit->edit_absolute_scale > 0)
	s->s_edit->edit_absolute_scale /= 3.0;
}

/*
 * Use mouse to change solid's location.
 * Project solid's keypoint into view space,
 * replace X, Y (but NOT Z) components, and
 * project result back to model space.
 * Then move keypoint there.
 */
void
mged_generic_strans_xy(vect_t *pos_view,
	struct mged_state *s,
	const vect_t mousevec
	)
{
    point_t pt;
    vect_t delta;
    vect_t raw_kp = VINIT_ZERO;         /* s->s_edit->e_keypoint with s->s_edit->e_invmat applied */
    vect_t raw_mp = VINIT_ZERO;         /* raw model position */
    mat_t mat;

    MAT4X3PNT((*pos_view), view_state->vs_gvp->gv_model2view, s->s_edit->curr_e_axes_pos);
    (*pos_view)[X] = mousevec[X];
    (*pos_view)[Y] = mousevec[Y];
    MAT4X3PNT(pt, view_state->vs_gvp->gv_view2model, (*pos_view));

    /* Need vector from current vertex/keypoint
     * to desired new location.
     */
    MAT4X3PNT(raw_mp, s->s_edit->e_invmat, pt);
    MAT4X3PNT(raw_kp, s->s_edit->e_invmat, s->s_edit->curr_e_axes_pos);
    VSUB2(delta, raw_kp, raw_mp);
    MAT_IDN(mat);
    MAT_DELTAS_VEC_NEG(mat, delta);
    transform_editing_solid(s, &s->s_edit->es_int, mat, &s->s_edit->es_int, 1);
}

void
update_edit_absolute_tran(struct mged_state *s, vect_t view_pos)
{
    vect_t model_pos;
    vect_t ea_view_pos;
    vect_t diff;
    fastf_t inv_Viewscale = 1/view_state->vs_gvp->gv_scale;

    MAT4X3PNT(model_pos, view_state->vs_gvp->gv_view2model, view_pos);
    VSUB2(diff, model_pos, s->s_edit->e_axes_pos);
    VSCALE(s->s_edit->edit_absolute_model_tran, diff, inv_Viewscale);
    VMOVE(s->s_edit->last_edit_absolute_model_tran, s->s_edit->edit_absolute_model_tran);

    MAT4X3PNT(ea_view_pos, view_state->vs_gvp->gv_model2view, s->s_edit->e_axes_pos);
    VSUB2(s->s_edit->edit_absolute_view_tran, view_pos, ea_view_pos);
    VMOVE(s->s_edit->last_edit_absolute_view_tran, s->s_edit->edit_absolute_view_tran);
}

int
mged_generic_edit_xy(
	struct mged_state *s,
	int edflag,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->s_edit->es_int;
    bu_clbk_t f = NULL;
    void *d = NULL;

    switch (edflag) {
	case SSCALE:
	case PSCALE:
	    mged_generic_sscale_xy(s, mousevec);
	    if (MGED_OBJ[ip->idb_type].ft_edit)
		return (*MGED_OBJ[ip->idb_type].ft_edit)(s, edflag);
	    return 0;
	case STRANS:
	    mged_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	default:
	    bu_vls_printf(s->s_edit->log_str, "%s: XY edit undefined in solid edit mode %d\n", MGED_OBJ[ip->idb_type].ft_label, edflag);
	    mged_state_clbk_get(&f, &d, s, 0, ECMD_PRINT_RESULTS, 0, GED_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return TCL_ERROR;
    }

    update_edit_absolute_tran(s, pos_view);

    if (MGED_OBJ[ip->idb_type].ft_edit)
	return (*MGED_OBJ[ip->idb_type].ft_edit)(s, edflag);

    return 0;
}



void
mged_set_edflag(struct mged_state *s, int edflag)
{
    if (!s->s_edit)
	return;

    s->s_edit->edit_flag = edflag;
    s->s_edit->solid_edit_pick = 0;

    switch (edflag) {
	case SROT:
	    s->s_edit->solid_edit_rotate = 1;
	    s->s_edit->solid_edit_translate = 0;
	    s->s_edit->solid_edit_scale = 0;
	    break;
	case STRANS:
	    s->s_edit->solid_edit_rotate = 0;
	    s->s_edit->solid_edit_translate = 1;
	    s->s_edit->solid_edit_scale = 0;
	    break;
	case SSCALE:
	case PSCALE:
	    s->s_edit->solid_edit_rotate = 0;
	    s->s_edit->solid_edit_translate = 0;
	    s->s_edit->solid_edit_scale = 1;
	    break;
	default:
	    s->s_edit->solid_edit_rotate = 0;
	    s->s_edit->solid_edit_translate = 0;
	    s->s_edit->solid_edit_scale = 0;
	    break;
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
