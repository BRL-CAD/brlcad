/*
 *			R E C T . C
 *
 *  Routines to implement MGED's rubber band rectangle capability.
 *
 *  Author -
 *	Robert G. Parker
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "";
#endif

#include "conf.h"

#include <math.h>
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"

extern int fb_refresh();  /* from libfb/tcl.c */

extern void mged_center(); /* from chgview.c */
extern int mged_vscale();

static void adjust_rect_for_zoom();

struct _rubber_band default_rubber_band = {
/* rb_rc */		1,
/* rb_active */		0,
/* rb_draw */		0,
/* rb_linewidth */	0,
/* rb_linestyle */	's',
/* rb_pos */		{ 0, 0 },
/* rb_dim */		{ 0, 0 },
/* rb_x */		0.0,
/* rb_y */		0.0,
/* rb_width */		0.0,
/* rb_height */		0.0
};

#define RB_O(_m)        offsetof(struct _rubber_band, _m)
#define RB_OA(_m)	offsetofarray(struct _rubber_band, _m)
struct bu_structparse rubber_band_vparse[] = {
	{"%d",	1, "draw",	RB_O(rb_draw),		rb_set_dirty_flag },
	{"%d",	1, "linewidth",	RB_O(rb_linewidth),	rb_set_dirty_flag },
	{"%c",	1, "linestyle",	RB_O(rb_linestyle),	rb_set_dirty_flag },
	{"%d",	2, "pos",	RB_OA(rb_pos),		set_rect },
	{"%d",	2, "dim",	RB_OA(rb_dim),		set_rect },
	{"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};

void
rb_set_dirty_flag(void)
{
  struct dm_list *dmlp;

  FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
    if(dmlp->dml_rubber_band == rubber_band)
      dmlp->dml_dirty = 1;
}

/*
 * Given position and dimensions in normalized view coordinates, calculate
 * position and dimensions in image coordinates.
 */
void
rect_view2image(void)
{
  rubber_band->rb_pos[X] = dm_Normal2Xx(dmp, rubber_band->rb_x);
  rubber_band->rb_pos[Y] = dmp->dm_height - dm_Normal2Xy(dmp, rubber_band->rb_y, 1);
  rubber_band->rb_dim[X] = rubber_band->rb_width * dmp->dm_width * 0.5;
  rubber_band->rb_dim[Y] = rubber_band->rb_height * dmp->dm_width * 0.5;
}

/*
 * Given position and dimensions in image coordinates, calculate
 * position and dimensions in normalized view coordinates.
 */
void
rect_image2view(void)
{
  rubber_band->rb_x = dm_Xx2Normal(dmp, rubber_band->rb_pos[X]);
  rubber_band->rb_y = dm_Xy2Normal(dmp, dmp->dm_height - rubber_band->rb_pos[Y], 1);
  rubber_band->rb_width = rubber_band->rb_dim[X] * 2.0 / (fastf_t)dmp->dm_width;
  rubber_band->rb_height = rubber_band->rb_dim[Y] * 2.0 / (fastf_t)dmp->dm_width;
}

void
set_rect(void)
{
  rect_image2view();
  rb_set_dirty_flag();
}

/*
 * Adjust the rubber band to have the same aspect ratio as the window.
 */
static void
adjust_rect_for_zoom()
{
  fastf_t width, height;

  if(rubber_band->rb_width >= 0.0)
    width = rubber_band->rb_width;
  else
    width = -rubber_band->rb_width;

  if(rubber_band->rb_height >= 0.0)
    height = rubber_band->rb_height;
  else
    height = -rubber_band->rb_height;

  if(width >= height){
    if(rubber_band->rb_height >= 0.0)
      rubber_band->rb_height = width / dmp->dm_aspect;
    else
      rubber_band->rb_height = -width / dmp->dm_aspect;
  }else{
    if(rubber_band->rb_width >= 0.0)
      rubber_band->rb_width = height * dmp->dm_aspect;
    else
      rubber_band->rb_width = -height * dmp->dm_aspect;
  }
}

void
draw_rect(void)
{
  int line_style;

  if(NEAR_ZERO(rubber_band->rb_width, (fastf_t)SMALL_FASTF) &&
     NEAR_ZERO(rubber_band->rb_height, (fastf_t)SMALL_FASTF))
    return;

  if(rubber_band->rb_linestyle == 'd')
    line_style = 1; /* dashed lines */
  else
    line_style = 0; /* solid lines */

  if(rubber_band->rb_active && mged_variables->mv_mouse_behavior == 'z')
    adjust_rect_for_zoom();

  /* draw rectangle */
  DM_SET_FGCOLOR(dmp,
	       color_scheme->cs_rubber_band[0],
	       color_scheme->cs_rubber_band[1],
	       color_scheme->cs_rubber_band[2], 1);
  DM_SET_LINE_ATTR(dmp, rubber_band->rb_linewidth, line_style);

  DM_DRAW_LINE_2D(dmp,
		  rubber_band->rb_x,
		  rubber_band->rb_y * dmp->dm_aspect,
		  rubber_band->rb_x,
		  (rubber_band->rb_y + rubber_band->rb_height) * dmp->dm_aspect);
  DM_DRAW_LINE_2D(dmp,
		  rubber_band->rb_x,
		  (rubber_band->rb_y + rubber_band->rb_height) * dmp->dm_aspect,
		  rubber_band->rb_x + rubber_band->rb_width,
		  (rubber_band->rb_y + rubber_band->rb_height) * dmp->dm_aspect);
  DM_DRAW_LINE_2D(dmp,
		  rubber_band->rb_x + rubber_band->rb_width,
		  (rubber_band->rb_y + rubber_band->rb_height) * dmp->dm_aspect,
		  rubber_band->rb_x + rubber_band->rb_width,
		  rubber_band->rb_y * dmp->dm_aspect);
  DM_DRAW_LINE_2D(dmp,
		  rubber_band->rb_x + rubber_band->rb_width,
		  rubber_band->rb_y * dmp->dm_aspect,
		  rubber_band->rb_x,
		  rubber_band->rb_y * dmp->dm_aspect);
}

void
paint_rect_area(void)
{
  if(!fbp)
    return;

  (void)fb_refresh(fbp, rubber_band->rb_pos[X], rubber_band->rb_pos[Y],
		   rubber_band->rb_dim[X], rubber_band->rb_dim[Y]);
}

void
rt_rect_area(void)
{
  int xmin, xmax;
  int ymin, ymax;
  int width, height;
  struct bu_vls vls;

  if(!fbp)
    return;

  if(NEAR_ZERO(rubber_band->rb_width, (fastf_t)SMALL_FASTF) &&
     NEAR_ZERO(rubber_band->rb_height, (fastf_t)SMALL_FASTF))
    return;

  if(mged_variables->mv_port < 0){
    bu_log("rt_rect_area: invalid port number - %d\n", mged_variables->mv_port);
    return;
  }

  xmin = rubber_band->rb_pos[X];
  ymin = rubber_band->rb_pos[Y];
  width = rubber_band->rb_dim[X];
  height = rubber_band->rb_dim[Y];

  if(width >= 0){
    xmax = xmin + width;
  }else{
    xmax = xmin;
    xmin += width;
  }

  if(height >= 0){
    ymax = ymin + height;
  }else{
    ymax = ymin;
    ymin += height;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "rt -w %d -n %d -V %lf -F %d -j %d,%d,%d,%d -C%d/%d/%d",
		dmp->dm_width, dmp->dm_height, dmp->dm_aspect,
		mged_variables->mv_port, xmin, ymin, xmax, ymax,
		color_scheme->cs_bg[0], color_scheme->cs_bg[1], color_scheme->cs_bg[2]);
  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
  (void)Tcl_ResetResult(interp);
  bu_vls_free(&vls);
}

void
zoom_rect_area(void)
{
  fastf_t width, height;
  fastf_t sf;
  point_t old_model_center;
  point_t new_model_center;
  point_t old_view_center;
  point_t new_view_center;

  if(NEAR_ZERO(rubber_band->rb_width, (fastf_t)SMALL_FASTF) &&
     NEAR_ZERO(rubber_band->rb_height, (fastf_t)SMALL_FASTF))
    return;

  adjust_rect_for_zoom();

  /* find old view center */
#ifdef MGED_USE_VIEW_OBJ
  MAT_DELTAS_GET_NEG(old_model_center, view_state->vs_vop->vo_center);
  MAT4X3PNT(old_view_center, view_state->vs_vop->vo_model2view, old_model_center);
#else
  MAT_DELTAS_GET_NEG(old_model_center, view_state->vs_toViewcenter);
  MAT4X3PNT(old_view_center, view_state->vs_model2view, old_model_center);
#endif

  /* calculate new view center */
  VSET(new_view_center,
       rubber_band->rb_x + rubber_band->rb_width / 2.0,
       rubber_band->rb_y + rubber_band->rb_height / 2.0,
       old_view_center[Z]);

  /* find new model center */
#ifdef MGED_USE_VIEW_OBJ
  MAT4X3PNT(new_model_center, view_state->vs_vop->vo_view2model, new_view_center);
#else
  MAT4X3PNT(new_model_center, view_state->vs_view2model, new_view_center);
#endif
  mged_center(new_model_center);

  /* zoom in to fill rectangle */
  if(rubber_band->rb_width >= 0.0)
    width = rubber_band->rb_width;
  else
    width = -rubber_band->rb_width;

  if(rubber_band->rb_height >= 0.0)
    height = rubber_band->rb_height;
  else
    height = -rubber_band->rb_height;

  if(width >= height)
    sf = width / 2.0;
  else
    sf = height / 2.0 * dmp->dm_aspect;

  mged_vscale(sf);

  rubber_band->rb_x = -1.0;
  rubber_band->rb_y = -1.0 / dmp->dm_aspect;
  rubber_band->rb_width = 2.0;
  rubber_band->rb_height = 2.0 / dmp->dm_aspect;

  rect_view2image();
  rb_set_dirty_flag();
}
