/*                          A X E S . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
/** @file mged/axes.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "ged.h"

#include "./mged.h"
#include "./mged_dm.h"


extern point_t es_keypoint;
extern point_t e_axes_pos;
extern point_t curr_e_axes_pos;

static void ax_set_dirty_flag();

struct _axes_state default_axes_state = {
    /* ax_rc */			1,
    /* ax_model_draw */    	0,
    /* ax_model_size */		500,
    /* ax_model_linewidth */	1,
    /* ax_model_pos */		VINIT_ZERO,
    /* ax_view_draw */    	0,
    /* ax_view_size */		500,
    /* ax_view_linewidth */	1,
    /* ax_view_pos */		{ 0, 0 },
    /* ax_edit_draw */		0,
    /* ax_edit_size1 */		500,
    /* ax_edit_size2 */		500,
    /* ax_edit_linewidth1 */	1,
    /* ax_edit_linewidth2 */	1
};


#define AX_O(_m) bu_offsetof(struct _axes_state, _m)
#define AX_OA(_m) bu_offsetofarray(struct _axes_state, _m)
struct bu_structparse axes_vparse[] = {
    {"%d", 1, "model_draw",	AX_O(ax_model_draw),		ax_set_dirty_flag, NULL, NULL },
    {"%d", 1, "model_size",	AX_O(ax_model_size),		ax_set_dirty_flag, NULL, NULL },
    {"%d", 1, "model_linewidth",AX_O(ax_model_linewidth),	ax_set_dirty_flag, NULL, NULL },
    {"%f", 3, "model_pos",	AX_OA(ax_model_pos),		ax_set_dirty_flag, NULL, NULL },
    {"%d", 1, "view_draw",	AX_O(ax_view_draw),		ax_set_dirty_flag, NULL, NULL },
    {"%d", 1, "view_size",	AX_O(ax_view_size),		ax_set_dirty_flag, NULL, NULL },
    {"%d", 1, "view_linewidth",	AX_O(ax_view_linewidth),	ax_set_dirty_flag, NULL, NULL },
    {"%d", 2, "view_pos",	AX_OA(ax_view_pos),		ax_set_dirty_flag, NULL, NULL },
    {"%d", 1, "edit_draw",	AX_O(ax_edit_draw),		ax_set_dirty_flag, NULL, NULL },
    {"%d", 1, "edit_size1",	AX_O(ax_edit_size1),		ax_set_dirty_flag, NULL, NULL },
    {"%d", 1, "edit_size2",	AX_O(ax_edit_size2),		ax_set_dirty_flag, NULL, NULL },
    {"%d", 1, "edit_linewidth1",AX_O(ax_edit_linewidth1),	ax_set_dirty_flag, NULL, NULL },
    {"%d", 1, "edit_linewidth2",AX_O(ax_edit_linewidth2),	ax_set_dirty_flag, NULL, NULL },
    {"",   0, (char *)0,	0,			 BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


static void
ax_set_dirty_flag()
{
    struct dm_list *dmlp;

    FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
	if (dmlp->dml_axes_state == axes_state)
	    dmlp->dml_dirty = 1;
}


void
draw_e_axes()
{
    point_t v_ap1;                 /* axes position in view coordinates */
    point_t v_ap2;                 /* axes position in view coordinates */
    mat_t rot_mat;
    struct ged_axes_state gas;

    if (STATE == ST_S_EDIT) {
	MAT4X3PNT(v_ap1, view_state->vs_gvp->gv_model2view, e_axes_pos);
	MAT4X3PNT(v_ap2, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
    } else if (STATE == ST_O_EDIT) {
	point_t m_ap2;

	MAT4X3PNT(v_ap1, view_state->vs_gvp->gv_model2view, es_keypoint);
	MAT4X3PNT(m_ap2, modelchanges, es_keypoint);
	MAT4X3PNT(v_ap2, view_state->vs_gvp->gv_model2view, m_ap2);
    } else
	return;

    memset(&gas, 0, sizeof(struct ged_axes_state));
    VMOVE(gas.gas_axes_pos, v_ap1);
    gas.gas_axes_size = axes_state->ax_edit_size1 * INV_GED;
    VMOVE(gas.gas_axes_color, color_scheme->cs_edit_axes1);
    VMOVE(gas.gas_label_color, color_scheme->cs_edit_axes_label1);
    gas.gas_line_width = axes_state->ax_edit_linewidth1;

    dm_draw_axes(dmp, view_state->vs_gvp->gv_size, view_state->vs_gvp->gv_rotation, &gas);

    memset(&gas, 0, sizeof(struct ged_axes_state));
    VMOVE(gas.gas_axes_pos, v_ap2);
    gas.gas_axes_size = axes_state->ax_edit_size2 * INV_GED;
    VMOVE(gas.gas_axes_color, color_scheme->cs_edit_axes2);
    VMOVE(gas.gas_label_color, color_scheme->cs_edit_axes_label2);
    gas.gas_line_width = axes_state->ax_edit_linewidth2;

    bn_mat_mul(rot_mat, view_state->vs_gvp->gv_rotation, acc_rot_sol);
    dm_draw_axes(dmp, view_state->vs_gvp->gv_size, rot_mat, &gas);
}


void
draw_m_axes()
{
    point_t m_ap;			/* axes position in model coordinates, mm */
    point_t v_ap;			/* axes position in view coordinates */
    struct ged_axes_state gas;

    VSCALE(m_ap, axes_state->ax_model_pos, local2base);
    MAT4X3PNT(v_ap, view_state->vs_gvp->gv_model2view, m_ap);

    memset(&gas, 0, sizeof(struct ged_axes_state));
    VMOVE(gas.gas_axes_pos, v_ap);
    gas.gas_axes_size = axes_state->ax_model_size * INV_GED;
    VMOVE(gas.gas_axes_color, color_scheme->cs_model_axes);
    VMOVE(gas.gas_label_color, color_scheme->cs_model_axes_label);
    gas.gas_line_width = axes_state->ax_model_linewidth;

    dm_draw_axes(dmp, view_state->vs_gvp->gv_size, view_state->vs_gvp->gv_rotation, &gas);
}


void
draw_v_axes()
{
    point_t v_ap;			/* axes position in view coordinates */
    struct ged_axes_state gas;

    VSET(v_ap,
	 axes_state->ax_view_pos[X] * INV_GED,
	 axes_state->ax_view_pos[Y] * INV_GED / dmp->dm_aspect,
	 0.0);

    memset(&gas, 0, sizeof(struct ged_axes_state));
    VMOVE(gas.gas_axes_pos, v_ap);
    gas.gas_axes_size = axes_state->ax_view_size * INV_GED;
    VMOVE(gas.gas_axes_color, color_scheme->cs_view_axes);
    VMOVE(gas.gas_label_color, color_scheme->cs_view_axes_label);
    gas.gas_line_width = axes_state->ax_view_linewidth;

    dm_draw_axes(dmp, view_state->vs_gvp->gv_size, view_state->vs_gvp->gv_rotation, &gas);
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
