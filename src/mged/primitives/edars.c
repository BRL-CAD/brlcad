/*                         E D A R S . C
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
/** @file mged/primitives/edars.c
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

#define ECMD_ARS_PICK		5034	/* select an ARS point */
#define ECMD_ARS_NEXT_PT	5035	/* select next ARS point in same curve */
#define ECMD_ARS_PREV_PT	5036	/* select previous ARS point in same curve */
#define ECMD_ARS_NEXT_CRV	5037	/* select corresponding ARS point in next curve */
#define ECMD_ARS_PREV_CRV	5038	/* select corresponding ARS point in previous curve */
#define ECMD_ARS_MOVE_PT	5039	/* translate an ARS point */
#define ECMD_ARS_DEL_CRV	5040	/* delete an ARS curve */
#define ECMD_ARS_DEL_COL	5041	/* delete all corresponding points in each curve (a column) */
#define ECMD_ARS_DUP_CRV	5042	/* duplicate an ARS curve */
#define ECMD_ARS_DUP_COL	5043	/* duplicate an ARS column */
#define ECMD_ARS_MOVE_CRV	5044	/* translate an ARS curve */
#define ECMD_ARS_MOVE_COL	5045	/* translate an ARS column */
#define ECMD_ARS_PICK_MENU	5046	/* display the ARS pick menu */
#define ECMD_ARS_EDIT_MENU	5047	/* display the ARS edit menu */

int es_ars_crv;	/* curve and column identifying selected ARS point */
int es_ars_col;
point_t es_pt;		/* coordinates of selected ARS point */

/**
 * find which vertex of an ARS is nearest *the ray from "pt" in the
 * viewing direction (for vertex selection in MGED)
 */
void
find_ars_nearest_pnt(
    int *crv,
    int *col,
    struct rt_ars_internal *ars,
    point_t pick_pt,
    vect_t dir)
{
    size_t i, j;
    int closest_i=0, closest_j=0;
    fastf_t min_dist_sq=MAX_FASTF;

    RT_ARS_CK_MAGIC(ars);

    for (i=0; i<ars->ncurves; i++) {
	for (j=0; j<ars->pts_per_curve; j++) {
	    fastf_t dist_sq;

	    dist_sq = bg_distsq_line3_pnt3(pick_pt, dir, &ars->curves[i][j*3]);
	    if (dist_sq < min_dist_sq) {
		min_dist_sq = dist_sq;
		closest_i = i;
		closest_j = j;
	    }
	}
    }
    *crv = closest_i;
    *col = closest_j;
}

/*ARGSUSED*/
static void
ars_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->s_edit.edit_flag = arg;

    switch (arg) {
	case ECMD_ARS_MOVE_PT:
	case ECMD_ARS_MOVE_CRV:
	case ECMD_ARS_MOVE_COL:
	    s->s_edit.solid_edit_rotate = 0;
	    s->s_edit.solid_edit_translate = 1;
	    s->s_edit.solid_edit_scale = 0;
	    s->s_edit.solid_edit_pick = 0;
	    break;
	case ECMD_ARS_PICK:
	    s->s_edit.solid_edit_rotate = 0;
	    s->s_edit.solid_edit_translate = 0;
	    s->s_edit.solid_edit_scale = 0;
	    s->s_edit.solid_edit_pick = 1;
	    break;
	default:
	    mged_set_edflag(s, arg);
	    break;
    };

    sedit(s);
}
struct menu_item ars_pick_menu[] = {
    { "ARS PICK MENU", NULL, 0 },
    { "Pick Vertex", ars_ed, ECMD_ARS_PICK },
    { "Next Vertex", ars_ed, ECMD_ARS_NEXT_PT },
    { "Prev Vertex", ars_ed, ECMD_ARS_PREV_PT },
    { "Next Curve", ars_ed, ECMD_ARS_NEXT_CRV },
    { "Prev Curve", ars_ed, ECMD_ARS_PREV_CRV },
    { "", NULL, 0 }
};


struct menu_item ars_menu[] = {
    { "ARS MENU", NULL, 0 },
    { "Pick Vertex", ars_ed, ECMD_ARS_PICK_MENU },
    { "Move Point", ars_ed, ECMD_ARS_MOVE_PT },
    { "Delete Curve", ars_ed, ECMD_ARS_DEL_CRV },
    { "Delete Column", ars_ed, ECMD_ARS_DEL_COL },
    { "Dup Curve", ars_ed, ECMD_ARS_DUP_CRV },
    { "Dup Column", ars_ed, ECMD_ARS_DUP_COL },
    { "Move Curve", ars_ed, ECMD_ARS_MOVE_CRV },
    { "Move Column", ars_ed, ECMD_ARS_MOVE_COL },
    { "", NULL, 0 }
};

void
mged_ars_labels(
	int *UNUSED(num_lines),
	point_t *UNUSED(lines),
	struct rt_point_labels *pl,
	int max_pl,
	const mat_t xform,
	struct rt_db_internal *ip,
	struct bn_tol *tol)
{
    point_t work;
    int npl = 0;

#define POINT_LABEL_STR(_pt, _str) { \
    VMOVE(pl[npl].pt, _pt); \
    bu_strlcpy(pl[npl++].str, _str, sizeof(pl[0].str)); }

    struct rt_ars_internal *ars = (struct rt_ars_internal *)ip->idb_ptr;

    RT_ARS_CK_MAGIC(ars);
    npl = OBJ[ip->idb_type].ft_labels(pl, max_pl, xform, ip, tol);

    // Conditional additional labeling
    if (es_ars_crv >= 0 && es_ars_col >= 0) {
	point_t ars_pt;

	VMOVE(work, &ars->curves[es_ars_crv][es_ars_col*3]);
	MAT4X3PNT(ars_pt, xform, work);
	POINT_LABEL_STR(ars_pt, "pt");
    }

    pl[npl].str[0] = '\0';      /* Mark ending */
}

const char *
mged_ars_keypoint(
	point_t *pt,
	const char *UNUSED(keystr),
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *UNUSED(tol))
{
    point_t mpt = VINIT_ZERO;
    static const char *strp = "V";
    struct rt_ars_internal *ars = (struct rt_ars_internal *)ip->idb_ptr;
    RT_ARS_CK_MAGIC(ars);

    if (es_ars_crv < 0 || es_ars_col < 0) {
	VMOVE(mpt, es_pt);
    } else {
	VMOVE(mpt, &ars->curves[es_ars_crv][es_ars_col*3]);
    }

    MAT4X3PNT(*pt, mat, mpt);
    return strp;
}

struct menu_item *
mged_ars_menu_item(const struct bn_tol *UNUSED(tol))
{
    return ars_menu;
}

int
mged_ars_menu_str(struct bu_vls *mstr, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    if (!mstr || !ip)
	return BRLCAD_ERROR;

    struct menu_item *mip = NULL;
    struct bu_vls vls2 = BU_VLS_INIT_ZERO;

    /* build ARS PICK MENU list */

    mip = ars_pick_menu;
    /* title */
    bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);
    for (++mip; mip->menu_func != NULL; ++mip)
	bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

    mip = ars_menu;
    /* title */
    bu_vls_printf(mstr, " {{%s} {}}", mip->menu_string);

    /* pick vertex menu */
    bu_vls_printf(mstr, " {{%s} {%s}}", (++mip)->menu_string,
	    bu_vls_addr(&vls2));

    for (++mip; mip->menu_func != NULL; ++mip)
	bu_vls_printf(mstr, " {{%s} {}}", mip->menu_string);

    bu_vls_free(&vls2);

    return BRLCAD_OK;
}

void
ecmd_ars_pick(struct mged_state *s)
{
    struct rt_ars_internal *ars=
	(struct rt_ars_internal *)s->s_edit.es_int.idb_ptr;
    point_t pick_pt;
    vect_t view_dir;
    vect_t z_dir;
    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
    point_t selected_pt;

    RT_ARS_CK_MAGIC(ars);

    /* must convert to base units */
    s->s_edit.e_para[0] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[1] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[2] *= s->dbip->dbi_local2base;

    if (s->s_edit.e_mvalid) {
	VMOVE(pick_pt, s->s_edit.e_mparam);
    } else if (s->s_edit.e_inpara == 3) {
	if (s->s_edit.mv_context) {
	    /* apply s->s_edit.e_invmat to convert to real model space */
	    MAT4X3PNT(pick_pt, s->s_edit.e_invmat, s->s_edit.e_para);
	} else {
	    VMOVE(pick_pt, s->s_edit.e_para);
	}
    } else if (s->s_edit.e_inpara && s->s_edit.e_inpara != 3) {
	bu_vls_printf(s->s_edit.log_str, "x y z coordinates required for 'pick point'\n");
	mged_print_result(s, TCL_ERROR);
	return;
    } else if (!s->s_edit.e_mvalid && !s->s_edit.e_inpara)
	return;

    /* Get view direction vector */
    VSET(z_dir, 0.0, 0.0, 1.0);
    MAT4X3VEC(view_dir, view_state->vs_gvp->gv_view2model, z_dir);
    find_ars_nearest_pnt(&es_ars_crv, &es_ars_col, ars, pick_pt, view_dir);
    VMOVE(es_pt, &ars->curves[es_ars_crv][es_ars_col*3]);
    VSCALE(selected_pt, es_pt, s->dbip->dbi_base2local);
    bu_log("Selected point #%d from curve #%d (%f %f %f)\n",
	    es_ars_col, es_ars_crv, V3ARGS(selected_pt));

    bu_vls_printf(&tmp_vls, "Selected point #%d from curve #%d (%f %f %f)\n", es_ars_col, es_ars_crv, V3ARGS(selected_pt));
    bu_vls_printf(s->s_edit.log_str, "%s", bu_vls_cstr(&tmp_vls));
    mged_print_result(s, TCL_ERROR);
    bu_vls_free(&tmp_vls);
}

void
ecmd_ars_next_pt(struct mged_state *s)
{
    struct rt_ars_internal *ars=
	(struct rt_ars_internal *)s->s_edit.es_int.idb_ptr;
    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
    point_t selected_pt;

    RT_ARS_CK_MAGIC(ars);

    if (es_ars_crv >= 0 && es_ars_col >= 0) {
	es_ars_col++;
	if ((size_t)es_ars_col >= ars->pts_per_curve)
	    es_ars_col = 0;
	VMOVE(es_pt, &ars->curves[es_ars_crv][es_ars_col*3]);
	VSCALE(selected_pt, es_pt, s->dbip->dbi_base2local);
	bu_log("Selected point #%d from curve #%d (%f %f %f)\n",
		es_ars_col, es_ars_crv, V3ARGS(selected_pt));

	bu_vls_printf(&tmp_vls, "Selected point #%d from curve #%d (%f %f %f)\n", es_ars_col, es_ars_crv, V3ARGS(selected_pt));
	bu_vls_printf(s->s_edit.log_str, "%s", bu_vls_cstr(&tmp_vls));
	mged_print_result(s, TCL_ERROR);
	bu_vls_free(&tmp_vls);
    }
}

void
ecmd_ars_prev_pt(struct mged_state *s)
{
    struct rt_ars_internal *ars=
	(struct rt_ars_internal *)s->s_edit.es_int.idb_ptr;
    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
    point_t selected_pt;

    RT_ARS_CK_MAGIC(ars);

    if (es_ars_crv >= 0 && es_ars_col >= 0) {
	es_ars_col--;
	if (es_ars_col < 0)
	    es_ars_col = ars->pts_per_curve - 1;
	VMOVE(es_pt, &ars->curves[es_ars_crv][es_ars_col*3]);
	VSCALE(selected_pt, es_pt, s->dbip->dbi_base2local);
	bu_log("Selected point #%d from curve #%d (%f %f %f)\n",
		es_ars_col, es_ars_crv, V3ARGS(selected_pt));

	bu_vls_printf(&tmp_vls, "Selected point #%d from curve #%d (%f %f %f)\n", es_ars_col, es_ars_crv, V3ARGS(selected_pt));
	bu_vls_printf(s->s_edit.log_str, "%s", bu_vls_cstr(&tmp_vls));
	mged_print_result(s, TCL_ERROR);
	bu_vls_free(&tmp_vls);
    }
}

void
ecmd_ars_next_crv(struct mged_state *s)
{
    struct rt_ars_internal *ars=
	(struct rt_ars_internal *)s->s_edit.es_int.idb_ptr;
    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
    point_t selected_pt;

    RT_ARS_CK_MAGIC(ars);

    if (es_ars_crv >= 0 && es_ars_col >= 0) {
	es_ars_crv++;
	if ((size_t)es_ars_crv >= ars->ncurves)
	    es_ars_crv = 0;
	VMOVE(es_pt, &ars->curves[es_ars_crv][es_ars_col*3]);
	VSCALE(selected_pt, es_pt, s->dbip->dbi_base2local);
	bu_log("Selected point #%d from curve #%d (%f %f %f)\n",
		es_ars_col, es_ars_crv, V3ARGS(selected_pt));

	bu_vls_printf(&tmp_vls, "Selected point #%d from curve #%d (%f %f %f)\n", es_ars_col, es_ars_crv, V3ARGS(selected_pt));
	bu_vls_printf(s->s_edit.log_str, "%s", bu_vls_cstr(&tmp_vls));
	mged_print_result(s, TCL_ERROR);
	bu_vls_free(&tmp_vls);
    }
}

void
ecmd_ars_prev_crv(struct mged_state *s)
{
    struct rt_ars_internal *ars=
	(struct rt_ars_internal *)s->s_edit.es_int.idb_ptr;
    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
    point_t selected_pt;

    RT_ARS_CK_MAGIC(ars);

    if (es_ars_crv >= 0 && es_ars_col >= 0) {
	es_ars_crv--;
	if (es_ars_crv < 0)
	    es_ars_crv = ars->ncurves - 1;
	VMOVE(es_pt, &ars->curves[es_ars_crv][es_ars_col*3]);
	VSCALE(selected_pt, es_pt, s->dbip->dbi_base2local);
	bu_log("Selected point #%d from curve #%d (%f %f %f)\n",
		es_ars_col, es_ars_crv, V3ARGS(selected_pt));

	bu_vls_printf(&tmp_vls, "Selected point #%d from curve #%d (%f %f %f)\n", es_ars_col, es_ars_crv, V3ARGS(selected_pt));
	bu_vls_printf(s->s_edit.log_str, "%s", bu_vls_cstr(&tmp_vls));
	mged_print_result(s, TCL_ERROR);
	bu_vls_free(&tmp_vls);
    }
}

void
ecmd_ars_dup_crv(struct mged_state *s)
{
    struct rt_ars_internal *ars=
	(struct rt_ars_internal *)s->s_edit.es_int.idb_ptr;
    fastf_t **curves;

    RT_ARS_CK_MAGIC(ars);

    if (es_ars_crv < 0 || es_ars_col < 0) {
	bu_log("No ARS point selected\n");
	return;
    }

    curves = (fastf_t **)bu_malloc((ars->ncurves+1) * sizeof(fastf_t *),
	    "new curves");

    for (size_t i=0; i<ars->ncurves+1; i++) {
	size_t j, k;

	curves[i] = (fastf_t *)bu_malloc(ars->pts_per_curve * 3 * sizeof(fastf_t),
		"new curves[i]");

	if (i <= (size_t)es_ars_crv)
	    k = i;
	else
	    k = i - 1;

	for (j=0; j<ars->pts_per_curve*3; j++)
	    curves[i][j] = ars->curves[k][j];
    }

    for (size_t i=0; i<ars->ncurves; i++)
	bu_free((void *)ars->curves[i], "ars->curves[i]");
    bu_free((void *)ars->curves, "ars->curves");

    ars->curves = curves;
    ars->ncurves++;
}

void
ecmd_ars_dup_col(struct mged_state *s)
{
    struct rt_ars_internal *ars=
	(struct rt_ars_internal *)s->s_edit.es_int.idb_ptr;
    fastf_t **curves;

    RT_ARS_CK_MAGIC(ars);

    if (es_ars_crv < 0 || es_ars_col < 0) {
	bu_log("No ARS point selected\n");
	return;
    }

    curves = (fastf_t **)bu_malloc(ars->ncurves * sizeof(fastf_t *),
	    "new curves");

    for (size_t i=0; i<ars->ncurves; i++) {
	size_t j, k;

	curves[i] = (fastf_t *)bu_malloc((ars->pts_per_curve + 1) * 3 * sizeof(fastf_t),
		"new curves[i]");

	for (j=0; j<ars->pts_per_curve+1; j++) {
	    if (j <= (size_t)es_ars_col)
		k = j;
	    else
		k = j - 1;

	    curves[i][j*3] = ars->curves[i][k*3];
	    curves[i][j*3+1] = ars->curves[i][k*3+1];
	    curves[i][j*3+2] = ars->curves[i][k*3+2];
	}
    }

    for (size_t i=0; i<ars->ncurves; i++)
	bu_free((void *)ars->curves[i], "ars->curves[i]");
    bu_free((void *)ars->curves, "ars->curves");

    ars->curves = curves;
    ars->pts_per_curve++;
}

void
ecmd_ars_del_crv(struct mged_state *s)
{
    struct rt_ars_internal *ars=
	(struct rt_ars_internal *)s->s_edit.es_int.idb_ptr;
    fastf_t **curves;
    int k;

    RT_ARS_CK_MAGIC(ars);

    if (es_ars_crv < 0 || es_ars_col < 0) {
	bu_log("No ARS point selected\n");
	return;
    }

    if (es_ars_crv == 0 || (size_t)es_ars_crv == ars->ncurves-1) {
	bu_log("Cannot delete first or last curve\n");
	return;
    }

    curves = (fastf_t **)bu_malloc((ars->ncurves - 1) * sizeof(fastf_t *),
	    "new curves");

    k = 0;
    for (size_t i=0; i<ars->ncurves; i++) {
	size_t j;

	if (i == (size_t)es_ars_crv)
	    continue;

	curves[k] = (fastf_t *)bu_malloc(ars->pts_per_curve * 3 * sizeof(fastf_t),
		"new curves[k]");

	for (j=0; j<ars->pts_per_curve*3; j++)
	    curves[k][j] = ars->curves[i][j];

	k++;
    }

    for (size_t i=0; i<ars->ncurves; i++)
	bu_free((void *)ars->curves[i], "ars->curves[i]");
    bu_free((void *)ars->curves, "ars->curves");

    ars->curves = curves;
    ars->ncurves--;

    if ((size_t)es_ars_crv >= ars->ncurves)
	es_ars_crv = ars->ncurves - 1;
}

void
ecmd_ars_del_col(struct mged_state *s)
{
    struct rt_ars_internal *ars=
	(struct rt_ars_internal *)s->s_edit.es_int.idb_ptr;
    fastf_t **curves;

    RT_ARS_CK_MAGIC(ars);

    if (es_ars_crv < 0 || es_ars_col < 0) {
	bu_log("No ARS point selected\n");
	return;
    }

    if (es_ars_col == 0 || (size_t)es_ars_col == ars->ncurves - 1) {
	bu_log("Cannot delete first or last column\n");
	return;
    }

    if (ars->pts_per_curve < 3) {
	bu_log("Cannot create an ARS with less than two points per curve\n");
	return;
    }

    curves = (fastf_t **)bu_malloc(ars->ncurves * sizeof(fastf_t *),
	    "new curves");

    for (size_t i=0; i<ars->ncurves; i++) {
	size_t j, k;


	curves[i] = (fastf_t *)bu_malloc((ars->pts_per_curve - 1) * 3 * sizeof(fastf_t),
		"new curves[i]");

	k = 0;
	for (j=0; j<ars->pts_per_curve; j++) {
	    if (j == (size_t)es_ars_col)
		continue;

	    curves[i][k*3] = ars->curves[i][j*3];
	    curves[i][k*3+1] = ars->curves[i][j*3+1];
	    curves[i][k*3+2] = ars->curves[i][j*3+2];
	    k++;
	}
    }

    for (size_t i=0; i<ars->ncurves; i++)
	bu_free((void *)ars->curves[i], "ars->curves[i]");
    bu_free((void *)ars->curves, "ars->curves");

    ars->curves = curves;
    ars->pts_per_curve--;

    if ((size_t)es_ars_col >= ars->pts_per_curve)
	es_ars_col = ars->pts_per_curve - 1;
}

void
ecmd_ars_move_col(struct mged_state *s)
{
    struct rt_ars_internal *ars=
	(struct rt_ars_internal *)s->s_edit.es_int.idb_ptr;
    point_t new_pt = VINIT_ZERO;
    vect_t diff;

    RT_ARS_CK_MAGIC(ars);

    /* must convert to base units */
    s->s_edit.e_para[0] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[1] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[2] *= s->dbip->dbi_local2base;

    if (es_ars_crv < 0 || es_ars_col < 0) {
	bu_log("No ARS point selected\n");
	return;
    }

    if (s->s_edit.e_mvalid) {
	vect_t view_dir;
	plane_t view_pl;
	fastf_t dist;

	/* construct a plane perpendicular to view direction
	 * that passes through ARS point being moved
	 */
	VSET(view_dir, 0.0, 0.0, 1.0);
	MAT4X3VEC(view_pl, view_state->vs_gvp->gv_view2model, view_dir);
	VUNITIZE(view_pl);
	view_pl[W] = VDOT(view_pl, &ars->curves[es_ars_crv][es_ars_col*3]);

	/* project s->s_edit.e_mparam onto the plane */
	dist = DIST_PNT_PLANE(s->s_edit.e_mparam, view_pl);
	VJOIN1(new_pt, s->s_edit.e_mparam, -dist, view_pl);
    } else if (s->s_edit.e_inpara == 3) {
	if (s->s_edit.mv_context) {
	    /* apply s->s_edit.e_invmat to convert to real model space */
	    MAT4X3PNT(new_pt, s->s_edit.e_invmat, s->s_edit.e_para);
	} else {
	    VMOVE(new_pt, s->s_edit.e_para);
	}
    } else if (s->s_edit.e_inpara && s->s_edit.e_inpara != 3) {
	bu_vls_printf(s->s_edit.log_str, "x y z coordinates required for point movement\n");
	mged_print_result(s, TCL_ERROR);
	return;
    } else if (!s->s_edit.e_mvalid && !s->s_edit.e_inpara) {
	return;
    }

    VSUB2(diff, new_pt, &ars->curves[es_ars_crv][es_ars_col*3]);

    for (size_t i=0; i<ars->ncurves; i++)
	VADD2(&ars->curves[i][es_ars_col*3],
		&ars->curves[i][es_ars_col*3], diff);

}

void
ecmd_ars_move_crv(struct mged_state *s)
{
    struct rt_ars_internal *ars=
	(struct rt_ars_internal *)s->s_edit.es_int.idb_ptr;
    point_t new_pt = VINIT_ZERO;
    vect_t diff;

    RT_ARS_CK_MAGIC(ars);

    /* must convert to base units */
    s->s_edit.e_para[0] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[1] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[2] *= s->dbip->dbi_local2base;

    if (es_ars_crv < 0 || es_ars_col < 0) {
	bu_log("No ARS point selected\n");
	return;
    }

    if (s->s_edit.e_mvalid) {
	vect_t view_dir;
	plane_t view_pl;
	fastf_t dist;

	/* construct a plane perpendicular to view direction
	 * that passes through ARS point being moved
	 */
	VSET(view_dir, 0.0, 0.0, 1.0);
	MAT4X3VEC(view_pl, view_state->vs_gvp->gv_view2model, view_dir);
	VUNITIZE(view_pl);
	view_pl[W] = VDOT(view_pl, &ars->curves[es_ars_crv][es_ars_col*3]);

	/* project s->s_edit.e_mparam onto the plane */
	dist = DIST_PNT_PLANE(s->s_edit.e_mparam, view_pl);
	VJOIN1(new_pt, s->s_edit.e_mparam, -dist, view_pl);
    } else if (s->s_edit.e_inpara == 3) {
	if (s->s_edit.mv_context) {
	    /* apply s->s_edit.e_invmat to convert to real model space */
	    MAT4X3PNT(new_pt, s->s_edit.e_invmat, s->s_edit.e_para);
	} else {
	    VMOVE(new_pt, s->s_edit.e_para);
	}
    } else if (s->s_edit.e_inpara && s->s_edit.e_inpara != 3) {
	bu_vls_printf(s->s_edit.log_str, "x y z coordinates required for point movement\n");
	mged_print_result(s, TCL_ERROR);
	return;
    } else if (!s->s_edit.e_mvalid && !s->s_edit.e_inpara) {
	return;
    }

    VSUB2(diff, new_pt, &ars->curves[es_ars_crv][es_ars_col*3]);

    for (size_t i=0; i<ars->pts_per_curve; i++)
	VADD2(&ars->curves[es_ars_crv][i*3],
		&ars->curves[es_ars_crv][i*3], diff);

}

void
ecmd_ars_move_pt(struct mged_state *s)
{
    struct rt_ars_internal *ars=
	(struct rt_ars_internal *)s->s_edit.es_int.idb_ptr;
    point_t new_pt = VINIT_ZERO;

    RT_ARS_CK_MAGIC(ars);

    /* must convert to base units */
    s->s_edit.e_para[0] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[1] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[2] *= s->dbip->dbi_local2base;

    if (es_ars_crv < 0 || es_ars_col < 0) {
	bu_log("No ARS point selected\n");
	return;
    }

    if (s->s_edit.e_mvalid) {
	vect_t view_dir;
	plane_t view_pl;
	fastf_t dist;

	/* construct a plane perpendicular to view direction
	 * that passes through ARS point being moved
	 */
	VSET(view_dir, 0.0, 0.0, 1.0);
	MAT4X3VEC(view_pl, view_state->vs_gvp->gv_view2model, view_dir);
	VUNITIZE(view_pl);
	view_pl[W] = VDOT(view_pl, &ars->curves[es_ars_crv][es_ars_col*3]);

	/* project s->s_edit.e_mparam onto the plane */
	dist = DIST_PNT_PLANE(s->s_edit.e_mparam, view_pl);
	VJOIN1(new_pt, s->s_edit.e_mparam, -dist, view_pl);
    } else if (s->s_edit.e_inpara == 3) {
	if (s->s_edit.mv_context) {
	    /* apply s->s_edit.e_invmat to convert to real model space */
	    MAT4X3PNT(new_pt, s->s_edit.e_invmat, s->s_edit.e_para);
	} else {
	    VMOVE(new_pt, s->s_edit.e_para);
	}
    } else if (s->s_edit.e_inpara && s->s_edit.e_inpara != 3) {
	bu_vls_printf(s->s_edit.log_str, "x y z coordinates required for point movement\n");
	mged_print_result(s, TCL_ERROR);
	return;
    } else if (!s->s_edit.e_mvalid && !s->s_edit.e_inpara) {
	return;
    }

    VMOVE(&ars->curves[es_ars_crv][es_ars_col*3], new_pt);
}

int
mged_ars_edit(struct mged_state *s, int edflag)
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
	case ECMD_ARS_PICK_MENU:
	    /* put up point pick menu for ARS solid */
	    menu_state->ms_flag = 0;
	    s->s_edit.edit_flag = ECMD_ARS_PICK;
	    s->s_edit.solid_edit_rotate = 0;
	    s->s_edit.solid_edit_translate = 0;
	    s->s_edit.solid_edit_scale = 0;
	    s->s_edit.solid_edit_pick = 1;
	    mmenu_set(s, MENU_L1, ars_pick_menu);
	    break;
	case ECMD_ARS_EDIT_MENU:
	    /* put up main ARS edit menu */
	    menu_state->ms_flag = 0;
	    mged_set_edflag(s, IDLE);
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
    }

    return 0;
}

int
mged_ars_edit_xy(
	struct mged_state *s,
	int edflag,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;
    struct rt_db_internal *ip = &s->s_edit.es_int;

    switch (edflag) {
	case SSCALE:
	case PSCALE:
	    mged_generic_sscale_xy(s, mousevec);
	    mged_ars_edit(s, edflag);
	    return 0;
	case STRANS:
	    mged_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	case ECMD_ARS_PICK:
	case ECMD_ARS_MOVE_PT:
	case ECMD_ARS_MOVE_CRV:
	case ECMD_ARS_MOVE_COL:
	    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->s_edit.curr_e_axes_pos);
	    pos_view[X] = mousevec[X];
	    pos_view[Y] = mousevec[Y];
	    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
	    MAT4X3PNT(s->s_edit.e_mparam, s->s_edit.e_invmat, temp);
	    s->s_edit.e_mvalid = 1;
	    break;
	default:
	    bu_vls_printf(s->s_edit.log_str, "%s: XY edit undefined in solid edit mode %d\n", MGED_OBJ[ip->idb_type].ft_label, edflag);
	    mged_print_result(s, TCL_ERROR);
	    return TCL_ERROR;
    }

    update_edit_absolute_tran(s, pos_view);
    mged_ars_edit(s, edflag);

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
