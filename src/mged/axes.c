/*                          A X E S . C
 * BRL-CAD
 *
 * Copyright (C) 1998-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file axes.c
 *
 * Functions -
 *	draw_axes	Common axes drawing routine that draws axes at the
 *				specifed point and orientation.
 *	draw_e_axes	Draw the edit axes.
 *	draw_m_axes	Draw the model axes.
 *	draw_v_axes	Draw the view axes.
 *
 * Author -
 *	Robert G. Parker
 *
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	berdeen Proving Ground, Maryland  21005
 */

#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"

extern point_t es_keypoint;
extern point_t e_axes_pos;
extern point_t curr_e_axes_pos;

static void ax_set_dirty_flag();

struct _axes_state default_axes_state = {
/* ax_rc */			1,
/* ax_model_draw */    	        0,
/* ax_model_size */		500,
/* ax_model_linewidth */	1,
/* ax_model_pos */		{ 0.0, 0.0, 0.0 },
/* ax_view_draw */    	        0,
/* ax_view_size */		500,
/* ax_view_linewidth */		1,
/* ax_view_pos */		{ 0, 0 },
/* ax_edit_draw */		0,
/* ax_edit_size1 */		500,
/* ax_edit_size2 */		500,
/* ax_edit_linewidth1 */	1,
/* ax_edit_linewidth2 */	1
};

#define AX_O(_m)	offsetof(struct _axes_state, _m)
#define AX_OA(_m)	offsetofarray(struct _axes_state, _m)
struct bu_structparse axes_vparse[] = {
	{"%d",  1, "model_draw",	AX_O(ax_model_draw),		ax_set_dirty_flag },
	{"%d",  1, "model_size",	AX_O(ax_model_size),		ax_set_dirty_flag },
	{"%d",  1, "model_linewidth",	AX_O(ax_model_linewidth),	ax_set_dirty_flag },
	{"%f",	3, "model_pos",		AX_OA(ax_model_pos),		ax_set_dirty_flag },
	{"%d",  1, "view_draw",		AX_O(ax_view_draw),		ax_set_dirty_flag },
	{"%d",  1, "view_size",		AX_O(ax_view_size),		ax_set_dirty_flag },
	{"%d",  1, "view_linewidth",	AX_O(ax_view_linewidth),	ax_set_dirty_flag },
	{"%d",  2, "view_pos",		AX_OA(ax_view_pos),		ax_set_dirty_flag },
	{"%d",  1, "edit_draw",		AX_O(ax_edit_draw),		ax_set_dirty_flag },
	{"%d",  1, "edit_size1",	AX_O(ax_edit_size1),		ax_set_dirty_flag },
	{"%d",  1, "edit_size2",	AX_O(ax_edit_size2),		ax_set_dirty_flag },
	{"%d",  1, "edit_linewidth1",	AX_O(ax_edit_linewidth1),	ax_set_dirty_flag },
	{"%d",  1, "edit_linewidth2",	AX_O(ax_edit_linewidth2),	ax_set_dirty_flag },
	{"",	0, (char *)0,		0,				BU_STRUCTPARSE_FUNC_NULL }
};

static void
ax_set_dirty_flag()
{
  struct dm_list *dmlp;

  FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
    if(dmlp->dml_axes_state == axes_state)
      dmlp->dml_dirty = 1;
}

void
draw_e_axes()
{
	point_t v_ap1;                 /* axes position in view coordinates */
	point_t v_ap2;                 /* axes position in view coordinates */
	mat_t rot_mat;

	if (state == ST_S_EDIT) {
		MAT4X3PNT(v_ap1, view_state->vs_vop->vo_model2view, e_axes_pos);
		MAT4X3PNT(v_ap2, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
	} else if(state == ST_O_EDIT) {
		point_t m_ap2;

		MAT4X3PNT(v_ap1, view_state->vs_vop->vo_model2view, es_keypoint);
		MAT4X3PNT(m_ap2, modelchanges, es_keypoint);
		MAT4X3PNT(v_ap2, view_state->vs_vop->vo_model2view, m_ap2);
	} else
		return;

	dmo_drawAxes_cmd(dmp,
			 view_state->vs_vop->vo_size,
			 view_state->vs_vop->vo_rotation,
			 v_ap1,
			 axes_state->ax_edit_size1 * INV_GED,
			 color_scheme->cs_edit_axes1,
			 color_scheme->cs_edit_axes_label1,
			 axes_state->ax_edit_linewidth1,
			 0, /* positive direction only */
			 0, /* three colors (i.e. X-red, Y-green, Z-blue) */
			 0, /* no ticks */
			 0, /* tick len */
			 0, /* major tick len */
			 0, /* tick interval */
			 0, /* ticks per major */
			 NULL, /* tick color */
			 NULL, /* major tick color */
			 0 /* tick threshold */);

	bn_mat_mul(rot_mat, view_state->vs_vop->vo_rotation, acc_rot_sol);
	dmo_drawAxes_cmd(dmp,
			 view_state->vs_vop->vo_size,
			 rot_mat,
			 v_ap2,
			 axes_state->ax_edit_size2 * INV_GED,
			 color_scheme->cs_edit_axes2,
			 color_scheme->cs_edit_axes_label2,
			 axes_state->ax_edit_linewidth2,
			 0, /* positive direction only */
			 0, /* three colors (i.e. X-red, Y-green, Z-blue) */
			 0, /* no ticks */
			 0, /* tick len */
			 0, /* major tick len */
			 0, /* tick interval */
			 0, /* ticks per major */
			 NULL, /* tick color */
			 NULL, /* major tick color */
			 0 /* tick threshold */);
}

void
draw_m_axes()
{
	point_t m_ap;			/* axes position in model coordinates, mm */
	point_t v_ap;			/* axes position in view coordinates */

	VSCALE(m_ap, axes_state->ax_model_pos, local2base);
	MAT4X3PNT(v_ap, view_state->vs_vop->vo_model2view, m_ap);
	dmo_drawAxes_cmd(dmp,
			 view_state->vs_vop->vo_size,
			 view_state->vs_vop->vo_rotation,
			 v_ap,
			 axes_state->ax_model_size * INV_GED,
			 color_scheme->cs_model_axes,
			 color_scheme->cs_model_axes_label,
			 axes_state->ax_model_linewidth,
			 0, /* positive direction only */
			 0, /* three colors (i.e. X-red, Y-green, Z-blue) */
			 0, /* no ticks */
			 0, /* tick len */
			 0, /* major tick len */
			 0, /* tick interval */
			 0, /* ticks per major */
			 NULL, /* tick color */
			 NULL, /* major tick color */
			 0 /* tick threshold */);
}

void
draw_v_axes()
{
  point_t v_ap;			/* axes position in view coordinates */

  VSET(v_ap,
       axes_state->ax_view_pos[X] * INV_GED,
       axes_state->ax_view_pos[Y] * INV_GED / dmp->dm_aspect,
       0.0);

  dmo_drawAxes_cmd(dmp,
		   view_state->vs_vop->vo_size,
		   view_state->vs_vop->vo_rotation,
		   v_ap,
		   axes_state->ax_view_size * INV_GED,
		   color_scheme->cs_view_axes,
		   color_scheme->cs_view_axes_label,
		   axes_state->ax_view_linewidth,
		   0, /* positive direction only */
		   0, /* three colors (i.e. X-red, Y-green, Z-blue) */
		   0, /* no ticks */
		   0, /* tick len */
		   0, /* major tick len */
		   0, /* tick interval */
		   0, /* ticks per major */
		   NULL, /* tick color */
		   NULL, /* major tick color */
		   0 /* tick threshold */);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
