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


const char *
rt_solid_edit_generic_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	struct rt_solid_edit *s,
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
rt_solid_edit_generic_sscale(
	struct rt_solid_edit *s,
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
rt_solid_edit_generic_strans(
	struct rt_solid_edit *s,
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
rt_solid_edit_generic_srot(
	struct rt_solid_edit *s,
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
rt_solid_edit_generic_menu_str(struct bu_vls *mstr, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    if (!mstr || !ip)
	return BRLCAD_ERROR;

    struct rt_solid_edit_menu_item *mip = NULL;

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
rt_solid_edit_generic_edit(
	struct rt_solid_edit *s
	)
{
    switch (s->edit_flag) {
	case RT_SOLID_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    rt_solid_edit_generic_sscale(s, &s->es_int);
	    break;
	case RT_SOLID_EDIT_TRANS:
	    /* translate solid */
	    rt_solid_edit_generic_strans(s, &s->es_int);
	    break;
	case RT_SOLID_EDIT_ROT:
	    /* rot solid about vertex */
	    rt_solid_edit_generic_srot(s, &s->es_int);
	    break;
    }
    return 0;
}

void
rt_solid_edit_generic_sscale_xy(
	struct rt_solid_edit *s,
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

    s->edit_absolute_scale = s->acc_sc_sol - 1.0;
    if (s->edit_absolute_scale > 0)
	s->edit_absolute_scale /= 3.0;
}

/*
 * Use mouse to change solid's location.
 * Project solid's keypoint into view space,
 * replace X, Y (but NOT Z) components, and
 * project result back to model space.
 * Then move keypoint there.
 */
void
rt_solid_edit_generic_strans_xy(vect_t *pos_view,
	struct rt_solid_edit *s,
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

int
rt_solid_edit_generic_edit_xy(
	struct rt_solid_edit *s,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->es_int;
    bu_clbk_t f = NULL;
    void *d = NULL;

    switch (s->edit_flag) {
	case RT_SOLID_EDIT_SCALE:
	    rt_solid_edit_generic_sscale_xy(s, mousevec);
	    return 0;
	case RT_SOLID_EDIT_TRANS:
	    rt_solid_edit_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	default:
	    bu_vls_printf(s->log_str, "%s: XY edit undefined in solid edit mode %d\n", EDOBJ[ip->idb_type].ft_label, s->edit_flag);
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
    }

    rt_update_edit_absolute_tran(s, pos_view);

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
