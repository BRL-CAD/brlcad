/*
 *			A X E S . C
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
 *
 * Copyright Notice -
 *      This software is Copyright (C) 1998 by the United States Army.
 *      All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./mged_dm.h"

extern point_t es_keypoint;
extern point_t e_axes_pos;
extern point_t curr_e_axes_pos;

static void ax_set_dirty_flag(void);
static void draw_axes(fastf_t *vpos, fastf_t *rot_mat, fastf_t size, int *axes_color, int *label_color, int linewidth);

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
ax_set_dirty_flag(void)
{
  struct dm_list *dmlp;

  FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
    if(dmlp->dml_axes_state == axes_state)
      dmlp->dml_dirty = 1;
}

static void
draw_axes(fastf_t *vpos, fastf_t *rot_mat, fastf_t size, int *axes_color, int *label_color, int linewidth)
{
  register fastf_t half_size;			/* half the length of an axis */
  register fastf_t xlx, xly;			/* X axis label position */
  register fastf_t ylx, yly;			/* Y axis label position */
  register fastf_t zlx, zly;			/* Z axis label position */
  register fastf_t l_offset = 0.0078125;	/* axis label offset from axis endpoints */
  point_t v1, v2;
  point_t rv1, rv2;
  point_t o_rv2;

  half_size = size * 0.5;

  /* set axes color */
  DM_SET_FGCOLOR(dmp, axes_color[0], axes_color[1], axes_color[2], 1);

  /* set axes line width */
  DM_SET_LINE_ATTR(dmp, linewidth, 0);  /* solid lines */

  /* build X axis about view center */
  VSET(v1, -half_size, 0.0, 0.0);
  VSET(v2, half_size, 0.0, 0.0);

  /* rotate X axis into position */
  MAT4X3PNT(rv1, rot_mat, v1)
  MAT4X3PNT(rv2, rot_mat, v2);

  /* find the X axis label position about view center */
  VSET(v2, v2[X] + l_offset, v2[Y] + l_offset, v2[Z] + l_offset);
  MAT4X3PNT(o_rv2, rot_mat, v2);
  xlx = o_rv2[X];
  xly = o_rv2[Y];

  /* draw X axis with x/y offsets */
  DM_DRAW_LINE_2D(dmp, rv1[X] + vpos[X], (rv1[Y] + vpos[Y]) * dmp->dm_aspect,
		  rv2[X] + vpos[X], (rv2[Y] + vpos[Y]) * dmp->dm_aspect);

  /* build Y axis about view center */
  VSET(v1, 0.0, -half_size, 0.0);
  VSET(v2, 0.0, half_size, 0.0);

  /* rotate Y axis into position */
  MAT4X3PNT(rv1, rot_mat, v1)
  MAT4X3PNT(rv2, rot_mat, v2);

  /* find the Y axis label position about view center */
  VSET(v2, v2[X] + l_offset, v2[Y] + l_offset, v2[Z] + l_offset);
  MAT4X3PNT(o_rv2, rot_mat, v2);
  ylx = o_rv2[X];
  yly = o_rv2[Y];

  /* draw Y axis with x/y offsets */
  DM_DRAW_LINE_2D(dmp, rv1[X] + vpos[X], (rv1[Y] + vpos[Y]) * dmp->dm_aspect,
		  rv2[X] + vpos[X], (rv2[Y] + vpos[Y]) * dmp->dm_aspect);

  /* build Z axis about view center */
  VSET(v1, 0.0, 0.0, -half_size);
  VSET(v2, 0.0, 0.0, half_size);

  /* rotate Z axis into position */
  MAT4X3PNT(rv1, rot_mat, v1)
  MAT4X3PNT(rv2, rot_mat, v2);

  /* find the Z axis label position about view center */
  VSET(v2, v2[X] + l_offset, v2[Y] + l_offset, v2[Z] + l_offset);
  MAT4X3PNT(o_rv2, rot_mat, v2);
  zlx = o_rv2[X];
  zly = o_rv2[Y];

  /* draw Z axis with x/y offsets */
  DM_DRAW_LINE_2D(dmp, rv1[X] + vpos[X], (rv1[Y] + vpos[Y]) * dmp->dm_aspect,
		  rv2[X] + vpos[X], (rv2[Y] + vpos[Y]) * dmp->dm_aspect);

  /* set axes string color */
  DM_SET_FGCOLOR(dmp, label_color[0], label_color[1], label_color[2], 1);

  /* draw axes strings/labels with x/y offsets */
  DM_DRAW_STRING_2D(dmp, "X", xlx + vpos[X], xly + vpos[Y], 1, 1);
  DM_DRAW_STRING_2D(dmp, "Y", ylx + vpos[X], yly + vpos[Y], 1, 1);
  DM_DRAW_STRING_2D(dmp, "Z", zlx + vpos[X], zly + vpos[Y], 1, 1);
}

void
draw_e_axes(void)
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

	draw_axes(v_ap1,
		  view_state->vs_vop->vo_rotation,
		  axes_state->ax_edit_size1 * INV_GED,
		  color_scheme->cs_edit_axes1,
		  color_scheme->cs_edit_axes_label1,
		  axes_state->ax_edit_linewidth1);

	bn_mat_mul(rot_mat, view_state->vs_vop->vo_rotation, acc_rot_sol);
	draw_axes(v_ap2,
		  rot_mat,
		  axes_state->ax_edit_size2 * INV_GED,
		  color_scheme->cs_edit_axes2,
		  color_scheme->cs_edit_axes_label2,
		  axes_state->ax_edit_linewidth2);
}

void
draw_m_axes(void)
{
	point_t m_ap;			/* axes position in model coordinates, mm */
	point_t v_ap;			/* axes position in view coordinates */

	VSCALE(m_ap, axes_state->ax_model_pos, local2base);
	MAT4X3PNT(v_ap, view_state->vs_vop->vo_model2view, m_ap);
	draw_axes(v_ap,
		  view_state->vs_vop->vo_rotation,
		  axes_state->ax_model_size * INV_GED,
		  color_scheme->cs_model_axes,
		  color_scheme->cs_model_axes_label,
		  axes_state->ax_model_linewidth);
}

void
draw_v_axes(void)
{
  point_t v_ap;			/* axes position in view coordinates */

  VSET(v_ap,
       axes_state->ax_view_pos[X] * INV_GED,
       axes_state->ax_view_pos[Y] * INV_GED / dmp->dm_aspect,
       0.0);

  draw_axes(v_ap,
	    view_state->vs_vop->vo_rotation,
	    axes_state->ax_view_size * INV_GED,
	    color_scheme->cs_view_axes,
	    color_scheme->cs_view_axes_label,
	    axes_state->ax_view_linewidth);
}
