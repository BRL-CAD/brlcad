/*                         G E N E R I C . C
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

point_t e_axes_pos;
point_t curr_e_axes_pos;

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

    if (inpara > 1) {
	Tcl_AppendResult(s->interp, "ERROR: only one argument needed\n", (char *)NULL);
	inpara = 0;
	return TCL_ERROR;
    }

    if (inpara) {
	/* accumulate the scale factor */
	s->edit_state.es_scale = es_para[0] / acc_sc_sol;
	acc_sc_sol = es_para[0];
    }

    bn_mat_scale_about_pnt(scalemat, es_keypoint, s->edit_state.es_scale);
    bn_mat_mul(mat1, scalemat, es_mat);
    bn_mat_mul(mat, es_invmat, mat1);
    transform_editing_solid(s, ip, mat, ip, 1);

    /* reset solid scale factor */
    s->edit_state.es_scale = 1.0;

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

    if (inpara) {
	/* Need vector from current vertex/keypoint
	 * to desired new location.
	 */

	/* must convert to base units */
	es_para[0] *= s->dbip->dbi_local2base;
	es_para[1] *= s->dbip->dbi_local2base;
	es_para[2] *= s->dbip->dbi_local2base;

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
    transform_editing_solid(s, ip, mat, ip, 1);

    MAT_IDN(incr_change);
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
	    mged_generic_sscale(s, &s->edit_state.es_int);
	    break;
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->edit_state.es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->edit_state.es_int);
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
    s->edit_state.es_scale = 1.0 + 0.25 * ((fastf_t)
	    (mousevec[Y] > 0 ? mousevec[Y] : -mousevec[Y]));
    if (mousevec[Y] <= 0)
	s->edit_state.es_scale = 1.0 / s->edit_state.es_scale;

    /* accumulate scale factor */
    acc_sc_sol *= s->edit_state.es_scale;

    s->edit_state.edit_absolute_scale = acc_sc_sol - 1.0;
    if (s->edit_state.edit_absolute_scale > 0)
	s->edit_state.edit_absolute_scale /= 3.0;
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
    vect_t raw_kp = VINIT_ZERO;         /* es_keypoint with es_invmat applied */
    vect_t raw_mp = VINIT_ZERO;         /* raw model position */
    mat_t mat;

    MAT4X3PNT((*pos_view), view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
    (*pos_view)[X] = mousevec[X];
    (*pos_view)[Y] = mousevec[Y];
    MAT4X3PNT(pt, view_state->vs_gvp->gv_view2model, (*pos_view));

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

int
mged_generic_edit_xy(
	struct mged_state *s,
	int edflag,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->edit_state.es_int;

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
	    Tcl_AppendResult(s->interp, "%s: XY edit undefined in solid edit mode %d\n", MGED_OBJ[ip->idb_type].ft_label, edflag);
	    mged_print_result(s, TCL_ERROR);
	    return TCL_ERROR;
    }

    update_edit_absolute_tran(s, pos_view);

    if (MGED_OBJ[ip->idb_type].ft_edit)
	return (*MGED_OBJ[ip->idb_type].ft_edit)(s, edflag);

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
