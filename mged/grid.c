/*
 *			G R I D . C
 *
 * Routines to implement MGED's snap to grid capability.
 *
 * Functions -
 *	draw_grid	draw the grid according to user specification
 *	snap_to_grid	snap values to the nearest grid point
 *
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *	Robert G. Parker
 *
 * Copyright Notice -
 *	This software is Copyright (C) 1998 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "";
#endif

#ifdef DO_SNAP_TO_GRID
#include "conf.h"

#include <math.h>
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"

void draw_grid();
void snap_to_grid();

void
draw_grid()
{
  register int i, j;
  register nh, nv;
  register nh_lines, nv_lines;
  register fastf_t fx, fy;
  register fastf_t sf;
  register fastf_t inv_sf;
  point_t model_grid_anchor;
  point_t view_grid_anchor;
  point_t view_lleft_corner;
  point_t grid_start_pt;
  point_t view_grid_anchor_local;
  point_t view_lleft_corner_local;
  point_t view_grid_start_pt_local;
  point_t mpt1, mpt2;
  point_t vpt1, vpt2;
  fastf_t inv_grid_res_h;
  fastf_t inv_grid_res_v;

  sf = Viewscale*base2local;
  inv_sf = 1 / sf;

  inv_grid_res_h= 1.0 / mged_variables->grid_res_h;
  inv_grid_res_v= 1.0 / mged_variables->grid_res_v;

  VSCALE(model_grid_anchor, mged_variables->grid_anchor, local2base);
  MAT4X3PNT(view_grid_anchor, model2view, model_grid_anchor);
  VSCALE(view_grid_anchor_local, view_grid_anchor, sf);

  VSET(view_lleft_corner, -1.0, -1.0, 0.0);
  VSCALE(view_lleft_corner_local, view_lleft_corner, sf);
  nh = (view_grid_anchor_local[X] - view_lleft_corner_local[X]) * inv_grid_res_h;
  nv = (view_grid_anchor_local[Y] - view_lleft_corner_local[Y]) * inv_grid_res_v;
  VSET(view_grid_start_pt_local,
       view_grid_anchor_local[X] - (nh * mged_variables->grid_res_h),
       view_grid_anchor_local[Y] - (nv * mged_variables->grid_res_v),
       0.0);

  nh_lines = 2.0 * sf * inv_grid_res_v + 2;
  nv_lines = 2.0 * sf * inv_grid_res_h + 2;

  DM_SET_COLOR(dmp,
	       mged_variables->grid_color[0], 
	       mged_variables->grid_color[1],
	       mged_variables->grid_color[2], 1);

  /* draw horizontal dots */
  for(i = 0; i < nh_lines; i += mged_variables->grid_res_major_v){
    fy = (view_grid_start_pt_local[Y] + (i * mged_variables->grid_res_v)) * inv_sf;

    for(j = 0; j < nv_lines; ++j){
      fx = (view_grid_start_pt_local[X] + (j * mged_variables->grid_res_h)) * inv_sf;
      DM_DRAW_POINT_2D(dmp, fx, fy);
    }
  }

  /* draw vertical dots */
  if(mged_variables->grid_res_major_v != 1){
    for(i = 0; i < nv_lines; i += mged_variables->grid_res_major_h){
      fx = (view_grid_start_pt_local[X] + (i * mged_variables->grid_res_h)) * inv_sf;

      for(j = 0; j < nh_lines; ++j){
	fy = (view_grid_start_pt_local[Y] + (j * mged_variables->grid_res_v)) * inv_sf;
	DM_DRAW_POINT_2D(dmp, fx, fy);
      }
    }
  }
}

void
snap_to_grid(mx, my)
fastf_t *mx;		/* input and return values */
fastf_t *my;		/* input and return values */
{
  register int nh, nv;
  point_t view_pt;
  point_t view_grid_anchor;
  point_t model_grid_anchor;
  fastf_t grid_units_h;
  fastf_t grid_units_v;
  register fastf_t sf;
  register fastf_t inv_sf;

  sf = Viewscale*base2local;
  inv_sf = 1 / sf;

  VSET(view_pt, *mx, *my, 0.0);
  VSCALE(view_pt, view_pt, sf);  /* view_pt now in local units */

  VSCALE(model_grid_anchor, mged_variables->grid_anchor, local2base);
  MAT4X3PNT(view_grid_anchor, model2view, model_grid_anchor);
  VSCALE(view_grid_anchor, view_grid_anchor, sf);  /* view_grid_anchor now in local units */

  grid_units_h = (view_grid_anchor[X] - view_pt[X]) / mged_variables->grid_res_h;
  grid_units_v = (view_grid_anchor[Y] - view_pt[Y]) / mged_variables->grid_res_v;
  nh = grid_units_h;
  nv = grid_units_v;

  grid_units_h -= nh;		/* now contains only the fraction part */
  grid_units_v -= nv;		/* now contains only the fraction part */

  if(grid_units_h <= -0.5)
    *mx = view_grid_anchor[X] - ((nh - 1) * mged_variables->grid_res_h);
  else if(0.5 <= grid_units_h)
    *mx = view_grid_anchor[X] - ((nh + 1) * mged_variables->grid_res_h);
  else
    *mx = view_grid_anchor[X] - (nh * mged_variables->grid_res_h);

  if(grid_units_v <= -0.5)
    *my = view_grid_anchor[Y] - ((nv - 1) * mged_variables->grid_res_v);
  else if(0.5 <= grid_units_v)
    *my = view_grid_anchor[Y] - ((nv + 1) * mged_variables->grid_res_v);
  else
    *my = view_grid_anchor[Y] - (nv  * mged_variables->grid_res_v);

  *mx *= inv_sf;
  *my *= inv_sf;
}
#endif
