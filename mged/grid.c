/*
 *			G R I D . C
 *
 * Routines to implement MGED's snap to grid capability.
 *
 * Functions -
 *	draw_grid			Draw the grid according to user specification
 *	snap_to_grid			Snap values to the nearest grid point
 *	snap_view_center_to_grid	Make the grid point nearest the view center
 *					the new view center.
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

extern vect_t curr_e_axes_pos;  /* from edsol.c */

void draw_grid();
void snap_to_grid();

void
draw_grid()
{
  register int i, j;
  register int nh, nv;
  register int nh_lines, nv_lines;
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

  if(NEAR_ZERO(mged_variables->grid_res_h, (fastf_t)SMALL_FASTF) ||
     NEAR_ZERO(mged_variables->grid_res_v, (fastf_t)SMALL_FASTF))
    return;

  inv_grid_res_h= 1.0 / mged_variables->grid_res_h;
  inv_grid_res_v= 1.0 / mged_variables->grid_res_v;

  sf = Viewscale*base2local;
  inv_sf = 1 / sf;

  VSCALE(model_grid_anchor, mged_variables->grid_anchor, local2base);
  MAT4X3PNT(view_grid_anchor, model2view, model_grid_anchor);
  VSCALE(view_grid_anchor_local, view_grid_anchor, sf);

  VSET(view_lleft_corner, -1.0, -1.0, 0.0);
  VSCALE(view_lleft_corner_local, view_lleft_corner, sf);
  nh = (view_grid_anchor_local[X] - view_lleft_corner_local[X]) * inv_grid_res_h;
  nv = (view_grid_anchor_local[Y] - view_lleft_corner_local[Y]) * inv_grid_res_v;

  {
    int nmh, nmv;

    nmh = nh / mged_variables->grid_res_major_h + 1;
    nmv = nv / mged_variables->grid_res_major_v + 1;
    VSET(view_grid_start_pt_local,
	 view_grid_anchor_local[X] - (nmh * mged_variables->grid_res_h * mged_variables->grid_res_major_h),
	 view_grid_anchor_local[Y] - (nmv * mged_variables->grid_res_v * mged_variables->grid_res_major_v),
	 0.0);
  }

  nh_lines = 2.0 * sf * inv_grid_res_v + (2 * mged_variables->grid_res_major_v);
  nv_lines = 2.0 * sf * inv_grid_res_h + (2 * mged_variables->grid_res_major_h);

  DM_SET_COLOR(dmp,
	       mged_variables->grid_color[0], 
	       mged_variables->grid_color[1],
	       mged_variables->grid_color[2], 1);
  DM_SET_LINE_ATTR(dmp, 1, 0);		/* solid lines */

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
  register int nh, nv;		/* whole grid units */
  point_t view_pt;
  point_t view_grid_anchor;
  point_t model_grid_anchor;
  fastf_t grid_units_h;		/* eventually holds only fractional horizontal grid units */
  fastf_t grid_units_v;		/* eventually holds only fractional vertical grid units */
  register fastf_t sf;
  register fastf_t inv_sf;

  if(NEAR_ZERO(mged_variables->grid_res_h, (fastf_t)SMALL_FASTF) ||
     NEAR_ZERO(mged_variables->grid_res_v, (fastf_t)SMALL_FASTF))
    return;

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

void
snap_keypoint_to_grid()
{
  point_t view_pt;
  point_t model_pt;
  struct bu_vls cmd;

  MAT4X3PNT(view_pt, model2view, curr_e_axes_pos);
  snap_to_grid(&view_pt[X], &view_pt[Y]);
  MAT4X3PNT(model_pt, view2model, view_pt);
  VSCALE(model_pt, model_pt, base2local);

  bu_vls_init(&cmd);
  bu_vls_printf(&cmd, "p %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
  (void)Tcl_Eval(interp, bu_vls_addr(&cmd));
  bu_vls_free(&cmd);

  /* save model_pt in local units */
  VMOVE(dml_work_pt, model_pt);
  dml_mouse_dx = dml_mouse_dy = 0;
}

void
snap_view_center_to_grid()
{
  point_t view_pt, model_pt;

  MAT_DELTAS_GET_NEG(model_pt, toViewcenter);
  MAT4X3PNT(view_pt, model2view, model_pt);
  snap_to_grid(&view_pt[X], &view_pt[Y]);
  MAT4X3PNT(model_pt, view2model, view_pt);

  MAT_DELTAS_VEC_NEG(toViewcenter, model_pt);
  new_mats();

  VSCALE(model_pt, model_pt, base2local);

  /* save new center in local units */
  VMOVE(dml_work_pt, model_pt);
  dml_mouse_dx = dml_mouse_dy = 0;
}

/*
 * Expect values in the +-2.0 range,
 * Return values in the +-2.0 range that have been snapped to the nearest grid distance.
 */
void
round_to_grid(view_dx, view_dy)
fastf_t *view_dx, *view_dy;
{
  fastf_t grid_units_h, grid_units_v;
  fastf_t sf, inv_sf;
  fastf_t dx, dy;
  int nh, nv;

  if(NEAR_ZERO(mged_variables->grid_res_h, (fastf_t)SMALL_FASTF) ||
     NEAR_ZERO(mged_variables->grid_res_v, (fastf_t)SMALL_FASTF))
    return;

  sf = Viewscale*base2local;
  inv_sf = 1 / sf;

  /* convert mouse distance to grid units */
  grid_units_h = *view_dx * sf / mged_variables->grid_res_h;
  grid_units_v = *view_dy * sf /  mged_variables->grid_res_v;
  nh = grid_units_h;
  nv = grid_units_v;
  grid_units_h -= nh;
  grid_units_v -= nv;

  if(grid_units_h <= -0.5)
    *view_dx = (nh - 1) * mged_variables->grid_res_h;
  else if(0.5 <= grid_units_h)
    *view_dx = (nh + 1) * mged_variables->grid_res_h;
  else
    *view_dx = nh * mged_variables->grid_res_h;

  if(grid_units_v <= -0.5)
    *view_dy = (nv - 1) * mged_variables->grid_res_v;
  else if(0.5 <= grid_units_v)
    *view_dy = (nv + 1) * mged_variables->grid_res_v;
  else
    *view_dy = nv * mged_variables->grid_res_v;

  *view_dx *= inv_sf;
  *view_dy *= inv_sf;
}

void
snap_view_to_grid(view_dx, view_dy)
fastf_t view_dx, view_dy;
{
  point_t model_pt, view_pt;
  point_t vcenter, diff;

  if(NEAR_ZERO(mged_variables->grid_res_h, (fastf_t)SMALL_FASTF) ||
     NEAR_ZERO(mged_variables->grid_res_v, (fastf_t)SMALL_FASTF))
    return;

  round_to_grid(&view_dx, &view_dy);

  VSET(view_pt, view_dx, view_dy, 0.0);
  MAT4X3PNT(model_pt, view2model, view_pt);

  MAT_DELTAS_GET_NEG(vcenter, toViewcenter);
  VSUB2(diff, model_pt, vcenter);
  VSCALE(diff, diff, base2local);
  VSUB2(model_pt, dml_work_pt, diff);

  VSCALE(model_pt, model_pt, local2base);
  MAT_DELTAS_VEC_NEG(toViewcenter, model_pt);
  new_mats();
}

void
update_grids(sf)
fastf_t sf;
{
  register struct dm_list *dlp;
  struct bu_vls save_result;
  struct bu_vls cmd;

  FOR_ALL_DISPLAYS(dlp, &head_dm_list.l){
    dlp->_mged_variables->grid_res_h *= sf;
    dlp->_mged_variables->grid_res_v *= sf;
    VSCALE(dlp->_mged_variables->grid_anchor, dlp->_mged_variables->grid_anchor, sf);
  }

  bu_vls_init(&save_result);
  bu_vls_init(&cmd);

  bu_vls_strcpy(&save_result, interp->result);

  bu_vls_printf(&cmd, "grid_control_update %lf\n", sf);
  (void)Tcl_Eval(interp, bu_vls_addr(&cmd));

  Tcl_SetResult(interp, bu_vls_addr(&save_result), TCL_VOLATILE);

  bu_vls_free(&save_result);
  bu_vls_free(&cmd);
}
#endif
