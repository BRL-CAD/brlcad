/*                         E D G E N E R I C . C
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
/** @file primitives/edgeneric.c
 *
 * Editing routines not specific to any one primitive.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "./edit_private.h"

void
edit_abs_tra(struct rt_edit *s, vect_t view_pos)
{
    vect_t model_pos;
    vect_t ea_view_pos;
    vect_t diff;
    fastf_t inv_Viewscale = 1/s->vp->gv_scale;

    MAT4X3PNT(model_pos, s->vp->gv_view2model, view_pos);
    VSUB2(diff, model_pos, s->e_axes_pos);
    VSCALE(s->k.tra_m_abs, diff, inv_Viewscale);
    VMOVE(s->k.tra_m_abs_last, s->k.tra_m_abs);

    MAT4X3PNT(ea_view_pos, s->vp->gv_model2view, s->e_axes_pos);
    VSUB2(s->k.tra_v_abs, view_pos, ea_view_pos);
    VMOVE(s->k.tra_v_abs_last, s->k.tra_v_abs);
}

const char *
edit_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	struct rt_edit *s,
	const struct bn_tol *tol)
{
    struct rt_db_internal *ip = &s->es_int;
    static const char *vert_str = "V";
    const char *strp = OBJ[ip->idb_type].ft_keypoint(pt, keystr, mat, ip, tol);
    if (strp)
	strp = OBJ[ip->idb_type].ft_keypoint(pt, vert_str, mat, ip, tol);
    return strp;
}

int
edit_sscale(
	struct rt_edit *s,
	struct rt_db_internal *ip
	)
{
    mat_t mat, mat1, scalemat;

    if (s->e_inpara > 1) {
	bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    if (s->e_inpara) {
	/* accumulate the scale factor */
	s->es_scale = s->e_para[0] / s->acc_sc_sol;
	s->acc_sc_sol = s->e_para[0];
    }

    bn_mat_scale_about_pnt(scalemat, s->e_keypoint, s->es_scale);
    bn_mat_mul(mat1, scalemat, s->e_mat);
    bn_mat_mul(mat, s->e_invmat, mat1);
    if (OBJ[ip->idb_type].ft_mat)
	(*OBJ[ip->idb_type].ft_mat)(ip, mat, ip);

    /* reset solid scale factor */
    s->es_scale = 1.0;

    return 0;
}

void
edit_stra(
	struct rt_edit *s,
	struct rt_db_internal *ip
	)
{
    mat_t mat;
    static vect_t work;
    vect_t delta;

    if (s->e_inpara) {
	/* Need vector from current vertex/keypoint
	 * to desired new location.
	 */

	/* must convert to base units */
	s->e_para[0] *= s->local2base;
	s->e_para[1] *= s->local2base;
	s->e_para[2] *= s->local2base;

	if (s->mv_context) {
	    /* move solid so that s->e_keypoint is at position s->e_para */
	    vect_t raw_para;

	    MAT4X3PNT(raw_para, s->e_invmat, s->e_para);
	    MAT4X3PNT(work, s->e_invmat, s->e_keypoint);
	    VSUB2(delta, work, raw_para);
	    MAT_IDN(mat);
	    MAT_DELTAS_VEC_NEG(mat, delta);
	} else {
	    /* move solid to position s->e_para */
	    MAT4X3PNT(work, s->e_invmat, s->e_keypoint);
	    VSUB2(delta, work, s->e_para);
	    MAT_IDN(mat);
	    MAT_DELTAS_VEC_NEG(mat, delta);
	}
	if (OBJ[ip->idb_type].ft_mat)
	    (*OBJ[ip->idb_type].ft_mat)(ip, mat, ip);
    }
}

void
edit_srot(
	struct rt_edit *s,
	struct rt_db_internal *ip
	)
{
    static vect_t work;
    point_t rot_point;
    mat_t mat, mat1, edit;

    if (s->e_inpara) {
	static mat_t invsolr;
	/*
	 * Keyboard parameters:  absolute x, y, z rotations,
	 * in degrees.  First, cancel any existing rotations,
	 * then perform new rotation
	 */
	bn_mat_inv(invsolr, s->acc_rot_sol);

	/* Build completely new rotation change */
	MAT_IDN(s->model_changes);
	bn_mat_angles(s->model_changes,
		s->e_para[0],
		s->e_para[1],
		s->e_para[2]);
	/* Borrow s->incr_change matrix here */
	bn_mat_mul(s->incr_change, s->model_changes, invsolr);
	MAT_COPY(s->acc_rot_sol, s->model_changes);

	/* Apply new rotation to solid */
	/* Clear out solid rotation */
	MAT_IDN(s->model_changes);
    } else {
	/* Apply incremental changes already in s->incr_change */
    }
    /* Apply changes to solid */
    /* xlate keypoint to origin, rotate, then put back. */
    switch (s->vp->gv_rotate_about) {
	case 'v':       /* View Center */
	    VSET(work, 0.0, 0.0, 0.0);
	    MAT4X3PNT(rot_point, s->vp->gv_view2model, work);
	    break;
	case 'e':       /* Eye */
	    VSET(work, 0.0, 0.0, 1.0);
	    MAT4X3PNT(rot_point, s->vp->gv_view2model, work);
	    break;
	case 'm':       /* Model Center */
	    VSETALL(rot_point, 0.0);
	    break;
	case 'k':       /* Key Point */
	default:
	    VMOVE(rot_point, s->e_keypoint);
	    break;
    }

    if (s->mv_context) {
	/* calculate rotations about keypoint */
	bn_mat_xform_about_pnt(edit, s->incr_change, rot_point);

	/* We want our final matrix (mat) to xform the original solid
	 * to the position of this instance of the solid, perform the
	 * current edit operations, then xform back.
	 * mat = s->e_invmat * edit * s->e_mat
	 */
	bn_mat_mul(mat1, edit, s->e_mat);
	bn_mat_mul(mat, s->e_invmat, mat1);
    } else {
	MAT4X3PNT(work, s->e_invmat, rot_point);
	bn_mat_xform_about_pnt(mat, s->incr_change, work);
    }
    if (OBJ[ip->idb_type].ft_mat)
	(*OBJ[ip->idb_type].ft_mat)(ip, mat, ip);

    MAT_IDN(s->incr_change);
}

int
edit_menu_str(struct bu_vls *mstr, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    if (!mstr || !ip)
	return BRLCAD_ERROR;

    struct rt_edit_menu_item *mip = NULL;

    if (EDOBJ[ip->idb_type].ft_menu_item)
	mip = (*EDOBJ[ip->idb_type].ft_menu_item)(tol);

    if (!mip)
	return BRLCAD_OK;

    /* title */
    bu_vls_printf(mstr, " {{%s} {}}", mip->menu_string);

    for (++mip; mip->menu_func != NULL; ++mip)
	bu_vls_printf(mstr, " {{%s} {}}", mip->menu_string);

    return BRLCAD_OK;
}

int
edit_generic(
	struct rt_edit *s
	)
{
    if (!s)
	return BRLCAD_ERROR;

    struct rt_db_internal *ip = &s->es_int;
    bu_clbk_t f = NULL;
    void *d = NULL;

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    edit_sscale(s, &s->es_int);
	    return BRLCAD_OK;
	case RT_PARAMS_EDIT_TRANS:
	    /* translate solid */
	    edit_stra(s, &s->es_int);
	    return BRLCAD_OK;
	case RT_PARAMS_EDIT_ROT:
	    /* rot solid about vertex */
	    edit_srot(s, &s->es_int);
	    return BRLCAD_OK;
	case RT_MATRIX_EDIT_SCALE:
	case RT_MATRIX_EDIT_SCALE_X:
	case RT_MATRIX_EDIT_SCALE_Y:
	case RT_MATRIX_EDIT_SCALE_Z:
	    bu_log("RT_MATRIX_EDIT scaling not implemented\n");
	    return BRLCAD_ERROR;
	case RT_MATRIX_EDIT_TRANS_VIEW_XY:
	case RT_MATRIX_EDIT_TRANS_VIEW_X:
	case RT_MATRIX_EDIT_TRANS_VIEW_Y:
	    bu_log("RT_MATRIX_EDIT translating not implemented\n");
	    return BRLCAD_ERROR;
	case RT_MATRIX_EDIT_ROT:
	    // TODO - I think this may need to define a knob call?  check with main MGED implementation
	    bu_vls_printf(s->log_str, "XY rotation editing setup unimplemented\n");
	    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	default:
	    // Primitives should handle their specific editing cases before calling the generic function - if
	    // we got here, something isn't right
	    bu_vls_printf(s->log_str, "%s: edit ID is not a generic edit: %d\n", EDOBJ[ip->idb_type].ft_label, s->edit_flag);
	    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
    }

    // Shouldn't get here
    return BRLCAD_ERROR;
}

void
edit_sscale_xy(
	struct rt_edit *s,
	const vect_t mousevec
	)
{
    /* use mouse to get a scale factor */
    s->es_scale = 1.0 + 0.25 * ((fastf_t)
	    (mousevec[Y] > 0 ? mousevec[Y] : -mousevec[Y]));
    if (mousevec[Y] <= 0)
	s->es_scale = 1.0 / s->es_scale;

    /* accumulate scale factor */
    s->acc_sc_sol *= s->es_scale;

    s->k.sca_abs = s->acc_sc_sol - 1.0;
    if (s->k.sca_abs > 0)
	s->k.sca_abs /= 3.0;
}

/*
 * Use mouse to change solid's location.
 * Project solid's keypoint into view space,
 * replace X, Y (but NOT Z) components, and
 * project result back to model space.
 * Then move keypoint there.
 */
void
edit_stra_xy(vect_t *pos_view,
	struct rt_edit *s,
	const vect_t mousevec
	)
{
    point_t pt;
    vect_t delta;
    vect_t raw_kp = VINIT_ZERO;         /* s->e_keypoint with s->e_invmat applied */
    vect_t raw_mp = VINIT_ZERO;         /* raw model position */
    mat_t mat;

    MAT4X3PNT((*pos_view), s->vp->gv_model2view, s->curr_e_axes_pos);
    (*pos_view)[X] = mousevec[X];
    (*pos_view)[Y] = mousevec[Y];
    MAT4X3PNT(pt, s->vp->gv_view2model, (*pos_view));

    /* Need vector from current vertex/keypoint
     * to desired new location.
     */
    MAT4X3PNT(raw_mp, s->e_invmat, pt);
    MAT4X3PNT(raw_kp, s->e_invmat, s->curr_e_axes_pos);
    VSUB2(delta, raw_kp, raw_mp);
    MAT_IDN(mat);
    MAT_DELTAS_VEC_NEG(mat, delta);
    struct rt_db_internal *ip = &s->es_int;
    if (OBJ[ip->idb_type].ft_mat)
	(*OBJ[ip->idb_type].ft_mat)(ip, mat, ip);
}

void
edit_mscale_xy(
	struct rt_edit *s,
	const vect_t mousevec
	)
{
    fastf_t scale = 1.0;
    mat_t incr_mat;
    MAT_IDN(incr_mat);

    // The Y mousevec value is always what determines the scale,
    // regardless of whether we're doing a uniform, X, Y or Z
    // scale operation.
    scale = 1.0 + (fastf_t)(mousevec[Y]>0 ? mousevec[Y] : -mousevec[Y]);
    if (mousevec[Y] <= 0)
	scale = 1.0 / scale;

    /* switch depending on scaling option selected */
    switch (s->edit_flag) {

	case RT_MATRIX_EDIT_SCALE:
	    /* global scaling */
	    incr_mat[15] = 1.0 / scale;

	    s->acc_sc_obj /= incr_mat[15];
	    s->k.sca_abs = s->acc_sc_obj - 1.0;
	    if (s->k.sca_abs > 0.0)
		s->k.sca_abs /= 3.0;
	    break;

	case RT_MATRIX_EDIT_SCALE_X:
	    /* local scaling ... X-axis */
	    incr_mat[0] = scale;
	    /* accumulate the scale factor */
	    s->acc_sc[0] *= scale;
	    s->k.sca_abs = s->acc_sc[0] - 1.0;
	    if (s->k.sca_abs > 0.0)
		s->k.sca_abs /= 3.0;
	    break;

	case RT_MATRIX_EDIT_SCALE_Y:
	    /* local scaling ... Y-axis */
	    incr_mat[5] = scale;
	    /* accumulate the scale factor */
	    s->acc_sc[1] *= scale;
	    s->k.sca_abs = s->acc_sc[1] - 1.0;
	    if (s->k.sca_abs > 0.0)
		s->k.sca_abs /= 3.0;
	    break;

	case RT_MATRIX_EDIT_SCALE_Z:
	    /* local scaling ... Z-axis */
	    incr_mat[10] = scale;
	    /* accumulate the scale factor */
	    s->acc_sc[2] *= scale;
	    s->k.sca_abs = s->acc_sc[2] - 1.0;
	    if (s->k.sca_abs > 0.0)
		s->k.sca_abs /= 3.0;
	    break;

	default:
	    bu_log("mscale_xy: incorrect matrix edit flag supplied: %d\n", s->edit_flag);
	    return;
    }

    mat_t t;
    vect_t pos_model;
    VMOVE(t, s->e_keypoint);
    MAT4X3PNT(pos_model, s->model_changes, t);

    /* Have scaling take place with respect to keypoint, NOT the view
     * center.  model_changes is the matrix that will ultimately be used to
     * alter the geometry on disk. */
    mat_t out;
    VMOVE(t, s->e_keypoint);
    bn_mat_xform_about_pnt(t, incr_mat, pos_model);
    bn_mat_mul(out, t, s->model_changes);
    MAT_COPY(s->model_changes, out);
}

void
edit_tra_xy(vect_t *pos_view,
	struct rt_edit *s,
	const vect_t mousevec
	)
{
    mat_t incr_mat;
    MAT_IDN(incr_mat);

    vect_t temp, pos_model;

    /* Vector from object keypoint to cursor - need to incorporate
     * any current model2objview updates.
     *
     * NOTE!  If an application switches views during the same edit operation,
     * it is the responsibility of the application to calculate and set the
     * appropriate model2objview matrix.  The librt code will maintain the
     * matrix as long as the solid edit processing is not switching views, but
     * if the editing view changes there is no way for the library itself to
     * know what happened. */
    VMOVE(temp, s->e_keypoint);
    MAT4X3PNT(*pos_view, s->model2objview, temp);

    switch (s->edit_flag) {
	case RT_MATRIX_EDIT_TRANS_VIEW_X:
	    (*pos_view)[X] = mousevec[X];
	    break;
	case RT_MATRIX_EDIT_TRANS_VIEW_Y:
	    (*pos_view)[Y] = mousevec[Y];
	    break;
	default:
	    (*pos_view)[X] = mousevec[X];
	    (*pos_view)[Y] = mousevec[Y];
    }

    MAT4X3PNT(pos_model, s->vp->gv_view2model, *pos_view);/* NOT objview */

    edit_mtra(s, pos_model);
}

void
edit_mtra(
	struct rt_edit *s,
	const vect_t pos_model
	)
{
    mat_t incr_mat;
    MAT_IDN(incr_mat);
    mat_t oldchanges;  // tmp matrix
    vect_t temp, tr_temp;

    VMOVE(temp, s->e_keypoint);
    MAT4X3PNT(tr_temp, s->model_changes, temp);
    VSUB2(tr_temp, pos_model, tr_temp);
    MAT_DELTAS_VEC(incr_mat, tr_temp);
    MAT_COPY(oldchanges, s->model_changes);
    bn_mat_mul(s->model_changes, incr_mat, oldchanges);
}

int
edit_generic_xy(
	struct rt_edit *s,
	const vect_t mousevec
	)
{
    if (!s)
	return BRLCAD_ERROR;

    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->es_int;
    bu_clbk_t f = NULL;
    void *d = NULL;

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    edit_sscale_xy(s, mousevec);
	    return BRLCAD_OK;
	case RT_PARAMS_EDIT_TRANS:
	    edit_stra_xy(&pos_view, s, mousevec);
	    edit_abs_tra(s, pos_view);
	    return BRLCAD_OK;
	case RT_PARAMS_EDIT_ROT:
	    // TODO - I think this may need to define a knob call?  check with main MGED implementation
	    bu_vls_printf(s->log_str, "XY rotation editing setup unimplemented\n");
	    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	case RT_MATRIX_EDIT_SCALE:
	case RT_MATRIX_EDIT_SCALE_X:
	case RT_MATRIX_EDIT_SCALE_Y:
	case RT_MATRIX_EDIT_SCALE_Z:
	    edit_mscale_xy(s, mousevec);
	    return BRLCAD_OK;
	case RT_MATRIX_EDIT_TRANS_VIEW_XY:
	case RT_MATRIX_EDIT_TRANS_VIEW_X:
	case RT_MATRIX_EDIT_TRANS_VIEW_Y:
	    edit_tra_xy(&pos_view, s, mousevec);
	    edit_abs_tra(s, pos_view);
	    return BRLCAD_OK;
	case RT_MATRIX_EDIT_ROT:
	    // TODO - I think this may need to define a knob call?  check with main MGED implementation
	    bu_vls_printf(s->log_str, "XY rotation editing setup unimplemented\n");
	    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	default:
	    // Primitives should handle their specific editing cases before calling the generic function - if
	    // we got here, something isn't right
	    bu_vls_printf(s->log_str, "%s: (XY) edit ID is not a generic edit: %d\n", EDOBJ[ip->idb_type].ft_label, s->edit_flag);
	    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
    }

    // Shouldn't get here
    return BRLCAD_ERROR;
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
