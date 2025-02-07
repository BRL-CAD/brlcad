/*                      E D E X T R U D E . C
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
/** @file primitives/edextrude.c
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

#include "../edit_private.h"

#define ECMD_EXTR_SCALE_H	27073	/* scale extrusion vector */
#define ECMD_EXTR_MOV_H		27074	/* move end of extrusion vector */
#define ECMD_EXTR_ROT_H		27075	/* rotate extrusion vector */
#define ECMD_EXTR_SKT_NAME	27076	/* set sketch that the extrusion uses */

/*ARGSUSED*/
static void
extr_ed(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_flag = arg;

    switch (arg) {
	case ECMD_EXTR_ROT_H:
	    s->solid_edit_rotate = 1;
	    s->solid_edit_translate = 0;
	    s->solid_edit_scale = 0;
	    s->solid_edit_pick = 0;
	    break;
	case ECMD_EXTR_SCALE_H:
	    s->solid_edit_rotate = 0;
	    s->solid_edit_translate = 0;
	    s->solid_edit_scale = 1;
	    s->solid_edit_pick = 0;
	    break;
	case ECMD_EXTR_MOV_H:
	    s->solid_edit_rotate = 0;
	    s->solid_edit_translate = 1;
	    s->solid_edit_scale = 0;
	    s->solid_edit_pick = 0;
	    break;
	default:
	    rt_solid_edit_set_edflag(s, arg);
	    break;
    };

    rt_solid_edit_process(s);
}
struct rt_solid_edit_menu_item extr_menu[] = {
    { "EXTRUSION MENU",	NULL, 0 },
    { "Set H",		extr_ed, ECMD_EXTR_SCALE_H },
    { "Move End H",		extr_ed, ECMD_EXTR_MOV_H },
    { "Rotate H",		extr_ed, ECMD_EXTR_ROT_H },
    { "Referenced Sketch",	extr_ed, ECMD_EXTR_SKT_NAME },
    { "", NULL, 0 }
};

struct rt_solid_edit_menu_item *
rt_solid_edit_extrude_menu_item(const struct bn_tol *UNUSED(tol))
{
    return extr_menu;
}

const char *
rt_solid_edit_extrude_keypoint(
	point_t *pt,
	const char *UNUSED(keystr),
	const mat_t mat,
	struct rt_solid_edit *s,
	const struct bn_tol *tol)
{
    struct rt_db_internal *ip = &s->es_int;
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
rt_solid_edit_extrude_e_axes_pos(
	struct rt_solid_edit *s,
	const struct rt_db_internal *ip,
	const struct bn_tol *UNUSED(tol))
{
    if (s->edit_flag == ECMD_EXTR_MOV_H) {
	struct rt_extrude_internal *extr = (struct rt_extrude_internal *)ip->idb_ptr;
	point_t extr_v;
	vect_t extr_h;

	RT_EXTRUDE_CK_MAGIC(extr);

	MAT4X3PNT(extr_v, s->e_mat, extr->V);
	MAT4X3VEC(extr_h, s->e_mat, extr->h);
	VADD2(s->curr_e_axes_pos, extr_h, extr_v);
    } else {
	VMOVE(s->curr_e_axes_pos, s->e_keypoint);
    }
}

void
ecmd_extr_skt_name(struct rt_solid_edit *s)
{
    struct rt_extrude_internal *extr = (struct rt_extrude_internal *)s->es_int.idb_ptr;
    struct rt_db_internal tmp_ip;

    RT_EXTRUDE_CK_MAGIC(extr);

    if (extr->skt) {
	/* free the old sketch */
	RT_DB_INTERNAL_INIT(&tmp_ip);
	tmp_ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	tmp_ip.idb_type = ID_SKETCH;
	tmp_ip.idb_ptr = (void *)extr->skt;
	tmp_ip.idb_meth = &OBJ[ID_SKETCH];
	rt_db_free_internal(&tmp_ip);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EXTR_SKT_NAME, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, s);
}

int
ecmd_extr_mov_h(struct rt_solid_edit *s)
{
    vect_t work;
    struct rt_extrude_internal *extr =
	(struct rt_extrude_internal *)s->es_int.idb_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    RT_EXTRUDE_CK_MAGIC(extr);
    if (s->e_inpara) {
	if (s->e_inpara != 3) {
	    bu_vls_printf(s->log_str, "ERROR: three arguments needed\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}

	/* must convert to base units */
	s->e_para[0] *= s->local2base;
	s->e_para[1] *= s->local2base;
	s->e_para[2] *= s->local2base;

	if (s->mv_context) {
	    /* apply s->e_invmat to convert to real model coordinates */
	    MAT4X3PNT(work, s->e_invmat, s->e_para);
	    VSUB2(extr->h, work, extr->V);
	} else {
	    VSUB2(extr->h, s->e_para, extr->V);
	}
    }

    /* check for zero H vector */
    if (MAGNITUDE(extr->h) <= SQRT_SMALL_FASTF) {
	bu_vls_printf(s->log_str, "Zero H vector not allowed, resetting to +Z\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	VSET(extr->h, 0.0, 0.0, 1.0);
	return BRLCAD_ERROR;
    }

    return 0;
}

int
ecmd_extr_scale_h(struct rt_solid_edit *s)
{
    if (s->e_inpara != 1) {
	bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    if (s->e_para[0] <= 0.0) {
	bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    /* must convert to base units */
    s->e_para[0] *= s->local2base;
    s->e_para[1] *= s->local2base;
    s->e_para[2] *= s->local2base;

    struct rt_extrude_internal *extr =
	(struct rt_extrude_internal *)s->es_int.idb_ptr;

    RT_EXTRUDE_CK_MAGIC(extr);

    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(extr->h);
	VSCALE(extr->h, extr->h, s->es_scale);
    } else if (s->es_scale > 0.0) {
	VSCALE(extr->h, extr->h, s->es_scale);
	s->es_scale = 0.0;
    }

    return 0;
}

/* rotate height vector */
int
ecmd_extr_rot_h(struct rt_solid_edit *s)
{
    struct rt_extrude_internal *extr =
	(struct rt_extrude_internal *)s->es_int.idb_ptr;
    mat_t mat;
    mat_t mat1;
    mat_t edit;

    RT_EXTRUDE_CK_MAGIC(extr);
    if (s->e_inpara) {
	if (s->e_inpara != 3) {
	    bu_vls_printf(s->log_str, "ERROR: three arguments needed\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}

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

    if (s->mv_context) {
	/* calculate rotations about keypoint */
	bn_mat_xform_about_pnt(edit, s->incr_change, s->e_keypoint);

	/* We want our final matrix (mat) to xform the original solid
	 * to the position of this instance of the solid, perform the
	 * current edit operations, then xform back.
	 * mat = s->e_invmat * edit * s->e_mat
	 */
	bn_mat_mul(mat1, edit, s->e_mat);
	bn_mat_mul(mat, s->e_invmat, mat1);
	MAT4X3VEC(extr->h, mat, extr->h);
    } else {
	MAT4X3VEC(extr->h, s->incr_change, extr->h);
    }

    MAT_IDN(s->incr_change);

    return 0;
}

/* Use mouse to change location of point V+H */
void
ecmd_extr_mov_h_mousevec(struct rt_solid_edit *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t tr_temp = VINIT_ZERO;	/* temp translation vector */
    vect_t temp = VINIT_ZERO;
    struct rt_extrude_internal *extr =
	(struct rt_extrude_internal *)s->es_int.idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extr);

    MAT4X3PNT(pos_view, s->vp->gv_model2view, s->curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    /* Do NOT change pos_view[Z] ! */
    MAT4X3PNT(temp, s->vp->gv_view2model, pos_view);
    MAT4X3PNT(tr_temp, s->e_invmat, temp);
    VSUB2(extr->h, tr_temp, extr->V);
}

int
rt_solid_edit_extrude_edit(struct rt_solid_edit *s)
{
    switch (s->edit_flag) {
	case RT_SOLID_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    return rt_solid_edit_generic_sscale(s, &s->es_int);
	case RT_SOLID_EDIT_TRANS:
	    /* translate solid */
	    rt_solid_edit_generic_strans(s, &s->es_int);
	    break;
	case RT_SOLID_EDIT_ROT:
	    /* rot solid about vertex */
	    rt_solid_edit_generic_srot(s, &s->es_int);
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
rt_solid_edit_extrude_edit_xy(
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
	case RT_SOLID_EDIT_PSCALE:
	case ECMD_EXTR_SCALE_H:
	    rt_solid_edit_generic_sscale_xy(s, mousevec);
	    rt_solid_edit_extrude_edit(s);
	    return 0;
	case RT_SOLID_EDIT_TRANS:
	    rt_solid_edit_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	case ECMD_EXTR_MOV_H:
	    ecmd_extr_mov_h_mousevec(s, mousevec);
	    break;
	default:
	    bu_vls_printf(s->log_str, "%s: XY edit undefined in solid edit mode %d\n", EDOBJ[ip->idb_type].ft_label, s->edit_flag);
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
    }

    rt_update_edit_absolute_tran(s, pos_view);
    rt_solid_edit_extrude_edit(s);

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
