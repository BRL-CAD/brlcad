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
static void draw_axes();

void
draw_axes(vpos, rot_mat, size, axes_color, label_color, linewidth)
point_t vpos;
mat_t rot_mat;
fastf_t size;
int *axes_color;
int *label_color;
int linewidth;
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
  DM_SET_COLOR(dmp, axes_color[0], axes_color[1], axes_color[2], 1);

  /* set axes line width */
  DM_SET_LINE_ATTR(dmp, linewidth, 0);  /* solid lines */

  /* build X axis about view center */
  VSET(v1, -half_size, 0, 0);
  VSET(v2, half_size, 0, 0);

  /* rotate X axis into position */
  MAT4X3PNT(rv1, rot_mat, v1)
  MAT4X3PNT(rv2, rot_mat, v2);

  /* find the X axis label position about view center */
  VSET(v2, v2[X] + l_offset, v2[Y] + l_offset, v2[Z] + l_offset);
  MAT4X3PNT(o_rv2, rot_mat, v2);
  xlx = o_rv2[X];
  xly = o_rv2[Y];

  /* draw X axis with x/y offsets */
  DM_DRAW_LINE_2D(dmp, rv1[X] + vpos[X], rv1[Y] + vpos[Y],
		  rv2[X] + vpos[X], rv2[Y] + vpos[Y]);

  /* build Y axis about view center */
  VSET(v1, 0, -half_size, 0);
  VSET(v2, 0, half_size, 0);

  /* rotate Y axis into position */
  MAT4X3PNT(rv1, rot_mat, v1)
  MAT4X3PNT(rv2, rot_mat, v2);

  /* find the Y axis label position about view center */
  VSET(v2, v2[X] + l_offset, v2[Y] + l_offset, v2[Z] + l_offset);
  MAT4X3PNT(o_rv2, rot_mat, v2);
  ylx = o_rv2[X];
  yly = o_rv2[Y];

  /* draw Y axis with x/y offsets */
  DM_DRAW_LINE_2D(dmp, rv1[X] + vpos[X], rv1[Y] + vpos[Y],
		  rv2[X] + vpos[X], rv2[Y] + vpos[Y]);

  /* build Z axis about view center */
  VSET(v1, 0, 0, -half_size);
  VSET(v2, 0, 0, half_size);

  /* rotate Z axis into position */
  MAT4X3PNT(rv1, rot_mat, v1)
  MAT4X3PNT(rv2, rot_mat, v2);

  /* find the Z axis label position about view center */
  VSET(v2, v2[X] + l_offset, v2[Y] + l_offset, v2[Z] + l_offset);
  MAT4X3PNT(o_rv2, rot_mat, v2);
  zlx = o_rv2[X];
  zly = o_rv2[Y];

  /* draw Z axis with x/y offsets */
  DM_DRAW_LINE_2D(dmp, rv1[X] + vpos[X], rv1[Y] + vpos[Y],
		  rv2[X] + vpos[X], rv2[Y] + vpos[Y]);

  /* set axes string color */
  DM_SET_COLOR(dmp, label_color[0], label_color[1], label_color[2], 1);

  /* draw axes strings/labels with x/y offsets */
  DM_DRAW_STRING_2D(dmp, "X", xlx + vpos[X], xly + vpos[Y], 1, 1);
  DM_DRAW_STRING_2D(dmp, "Y", ylx + vpos[X], yly + vpos[Y], 1, 1);
  DM_DRAW_STRING_2D(dmp, "Z", zlx + vpos[X], zly + vpos[Y], 1, 1);
}

void
draw_e_axes()
{
  point_t v_ap1;                 /* axes position in view coordinates */
  point_t v_ap2;                 /* axes position in view coordinates */
  mat_t rot_mat;

  if(state == ST_S_EDIT){
    MAT4X3PNT(v_ap1, model2view, e_axes_pos);
    MAT4X3PNT(v_ap2, model2view, curr_e_axes_pos);
  }else if(state == ST_O_EDIT){
    point_t m_ap2;

    MAT4X3PNT(v_ap1, model2view, es_keypoint);
    MAT4X3PNT(m_ap2, modelchanges, es_keypoint);
    MAT4X3PNT(v_ap2, model2view, m_ap2);
  }else
    return;

  draw_axes(v_ap1,
	    Viewrot,
	    mged_variables->e_axes_size1 * INV_GED_FACTOR,
	    mged_variables->e_axes_color1,
	    mged_variables->e_axes_label_color1,
	    mged_variables->e_axes_linewidth1);

  bn_mat_mul(rot_mat, Viewrot, acc_rot_sol);
  draw_axes(v_ap2,
	    rot_mat,
	    mged_variables->e_axes_size2 * INV_GED_FACTOR,
	    mged_variables->e_axes_color2,
	    mged_variables->e_axes_label_color2,
	    mged_variables->e_axes_linewidth2);
}

void
draw_m_axes()
{
  point_t m_ap;			/* axes position in model coordinates, mm */
  point_t v_ap;			/* axes position in view coordinates */

  VSCALE(m_ap, mged_variables->m_axes_pos, local2base);
  MAT4X3PNT(v_ap, model2view, m_ap);
  draw_axes(v_ap,
	    Viewrot,
	    mged_variables->m_axes_size * INV_GED_FACTOR,
	    mged_variables->m_axes_color,
	    mged_variables->m_axes_label_color,
	    mged_variables->m_axes_linewidth);
}

void
draw_v_axes()
{
  point_t v_ap;			/* axes position in view coordinates */

  VSET(v_ap,
       mged_variables->v_axes_pos[X] * INV_GED_FACTOR,
       mged_variables->v_axes_pos[Y] * INV_GED_FACTOR,
       0.0);
  draw_axes(v_ap,
	    Viewrot,
	    mged_variables->v_axes_size * INV_GED_FACTOR,
	    mged_variables->v_axes_color,
	    mged_variables->v_axes_label_color,
	    mged_variables->v_axes_linewidth);
}
