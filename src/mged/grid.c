/*                          G R I D . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2006 United States Government as represented by
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
/** @file grid.c
 *
 * Routines to implement MGED's snap to grid capability.
 *
 * Functions -
 *	draw_grid			Draw the grid according to user specifications
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
 */
#ifndef lint
static const char RCSid[] = "";
#endif

#include "common.h"

#include <math.h>
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"

#include "./ged.h"
#include "./mged_dm.h"


extern void mged_vls_struct_parse(struct bu_vls *vls, char *title, struct bu_structparse *how_to_parse, char *structp, int argc, char **argv); /* defined in vparse.c */
extern point_t e_axes_pos;  /* from edsol.c */
extern point_t curr_e_axes_pos;  /* from edsol.c */

void draw_grid(void);
void snap_to_grid(fastf_t *mx, fastf_t *my);
static void grid_set_dirty_flag(void);
static void set_grid_draw(void);
static void set_grid_res(void);


struct _grid_state default_grid_state = {
/* gr_rc */		1,
/* gr_draw */		0,
/* gr_snap */		0,
/* gr_anchor */		{ 0.0, 0.0, 0.0 },
/* gr_res_h */		1.0,
/* gr_res_v */		1.0,
/* gr_res_major_h */	5,
/* gr_res_major_v */	5,
};

#define GRID_O(_m)        offsetof(struct _grid_state, _m)
#define GRID_OA(_m)       offsetofarray(struct _grid_state, _m)
struct bu_structparse grid_vparse[] = {
	{"%d",  1, "draw",		GRID_O(gr_draw),		set_grid_draw },
	{"%d",  1, "snap",		GRID_O(gr_snap),		grid_set_dirty_flag },
	{"%f",	3, "anchor",		GRID_OA(gr_anchor),	grid_set_dirty_flag },
	{"%f",	1, "rh",		GRID_O(gr_res_h),		set_grid_res },
	{"%f",	1, "rv",		GRID_O(gr_res_v),		set_grid_res },
	{"%d",  1, "mrh",		GRID_O(gr_res_major_h),	set_grid_res },
	{"%d",  1, "mrv",		GRID_O(gr_res_major_v),	set_grid_res },
	{"",	0,  (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};


static void
grid_set_dirty_flag(void)
{
  struct dm_list *dmlp;

  FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
    if(dmlp->dml_grid_state == grid_state)
      dmlp->dml_dirty = 1;
}

static void
set_grid_draw(void)
{
  struct dm_list *dlp;

  if (dbip == DBI_NULL) {
	  grid_state->gr_draw = 0;
	  return;
  }

  grid_set_dirty_flag();

  /* This gets done at most one time. */
  if(grid_auto_size && grid_state->gr_draw){
    fastf_t res = view_state->vs_vop->vo_size*base2local / 64.0;

    grid_state->gr_res_h = res;
    grid_state->gr_res_v = res;
    FOR_ALL_DISPLAYS(dlp, &head_dm_list.l)
      if(dlp->dml_grid_state == grid_state)
	dlp->dml_grid_auto_size = 0;
  }
}

static void
set_grid_res(void)
{
  struct dm_list *dlp;

  grid_set_dirty_flag();

  if(grid_auto_size)
    FOR_ALL_DISPLAYS(dlp, &head_dm_list.l)
      if(dlp->dml_grid_state == grid_state)
	dlp->dml_grid_auto_size = 0;
}

void
draw_grid(void)
{
	register int		i, j;
	register int		nh, nv;
	register int		nv_dots, nh_dots;
	register fastf_t	fx, fy;
	register fastf_t	sf;
	register fastf_t	inv_sf;
	point_t			model_grid_anchor;
	point_t			view_grid_anchor;
	point_t			view_lleft_corner;
	point_t 		view_grid_anchor_local;
	point_t 		view_lleft_corner_local;
	point_t 		view_grid_start_pt_local;
	fastf_t 		inv_grid_res_h;
	fastf_t 		inv_grid_res_v;
	fastf_t 		inv_aspect;

	if (dbip == DBI_NULL ||
	    NEAR_ZERO(grid_state->gr_res_h, (fastf_t)SMALL_FASTF) ||
	    NEAR_ZERO(grid_state->gr_res_v, (fastf_t)SMALL_FASTF))
		return;

	inv_grid_res_h= 1.0 / grid_state->gr_res_h;
	inv_grid_res_v= 1.0 / grid_state->gr_res_v;

	sf = view_state->vs_vop->vo_scale*base2local;

	/* sanity - don't draw the grid if it would fill the screen */
	{
		fastf_t pixel_size = 2.0 * sf / dmp->dm_width;

		if( grid_state->gr_res_h < pixel_size || grid_state->gr_res_v < pixel_size )
		    return;
	}

	inv_sf = 1.0 / sf;
	inv_aspect = 1.0 / dmp->dm_aspect;

	nv_dots = 2.0 * inv_aspect * sf * inv_grid_res_v + (2 * grid_state->gr_res_major_v);
	nh_dots = 2.0 * sf * inv_grid_res_h + (2 * grid_state->gr_res_major_h);

	VSCALE(model_grid_anchor, grid_state->gr_anchor, local2base);
	MAT4X3PNT(view_grid_anchor, view_state->vs_vop->vo_model2view, model_grid_anchor);
	VSCALE(view_grid_anchor_local, view_grid_anchor, sf);

	VSET(view_lleft_corner, -1.0, -inv_aspect, 0.0);
	VSCALE(view_lleft_corner_local, view_lleft_corner, sf);
	nh = (view_grid_anchor_local[X] - view_lleft_corner_local[X]) * inv_grid_res_h;
	nv = (view_grid_anchor_local[Y] - view_lleft_corner_local[Y]) * inv_grid_res_v;

	{
		int nmh, nmv;

		nmh = nh / grid_state->gr_res_major_h + 1;
		nmv = nv / grid_state->gr_res_major_v + 1;
		VSET(view_grid_start_pt_local,
		     view_grid_anchor_local[X] - (nmh * grid_state->gr_res_h * grid_state->gr_res_major_h),
		     view_grid_anchor_local[Y] - (nmv * grid_state->gr_res_v * grid_state->gr_res_major_v),
		     0.0);
	}

	DM_SET_FGCOLOR(dmp,
		       color_scheme->cs_grid[0],
		       color_scheme->cs_grid[1],
		       color_scheme->cs_grid[2], 1, 1.0);
	DM_SET_LINE_ATTR(dmp, 1, 0);		/* solid lines */

	/* draw horizontal dots */
	for (i = 0; i < nv_dots; i += grid_state->gr_res_major_v) {
		fy = (view_grid_start_pt_local[Y] + (i * grid_state->gr_res_v)) * inv_sf;

		for (j = 0; j < nh_dots; ++j) {
			fx = (view_grid_start_pt_local[X] + (j * grid_state->gr_res_h)) * inv_sf;
			DM_DRAW_POINT_2D(dmp, fx, fy * dmp->dm_aspect);
		}
	}

	/* draw vertical dots */
	if (grid_state->gr_res_major_v != 1) {
		for (i = 0; i < nh_dots; i += grid_state->gr_res_major_h) {
			fx = (view_grid_start_pt_local[X] + (i * grid_state->gr_res_h)) * inv_sf;

			for (j = 0; j < nv_dots; ++j) {
				fy = (view_grid_start_pt_local[Y] + (j * grid_state->gr_res_v)) * inv_sf;
				DM_DRAW_POINT_2D(dmp, fx, fy * dmp->dm_aspect);
			}
		}
	}
}

void
snap_to_grid(
	fastf_t *mx,		/* input and return values */
	fastf_t *my)		/* input and return values */
{
  register int nh, nv;		/* whole grid units */
  point_t view_pt;
  point_t view_grid_anchor;
  point_t model_grid_anchor;
  fastf_t grid_units_h;		/* eventually holds only fractional horizontal grid units */
  fastf_t grid_units_v;		/* eventually holds only fractional vertical grid units */
  register fastf_t sf;
  register fastf_t inv_sf;

  if (dbip == DBI_NULL ||
      NEAR_ZERO(grid_state->gr_res_h, (fastf_t)SMALL_FASTF) ||
      NEAR_ZERO(grid_state->gr_res_v, (fastf_t)SMALL_FASTF))
    return;

  sf = view_state->vs_vop->vo_scale*base2local;
  inv_sf = 1 / sf;

  VSET(view_pt, *mx, *my, 0.0);
  VSCALE(view_pt, view_pt, sf);  /* view_pt now in local units */

  VSCALE(model_grid_anchor, grid_state->gr_anchor, local2base);
  MAT4X3PNT(view_grid_anchor, view_state->vs_vop->vo_model2view, model_grid_anchor);
  VSCALE(view_grid_anchor, view_grid_anchor, sf);  /* view_grid_anchor now in local units */

  grid_units_h = (view_grid_anchor[X] - view_pt[X]) / grid_state->gr_res_h;
  grid_units_v = (view_grid_anchor[Y] - view_pt[Y]) / grid_state->gr_res_v;
  nh = grid_units_h;
  nv = grid_units_v;

  grid_units_h -= nh;		/* now contains only the fraction part */
  grid_units_v -= nv;		/* now contains only the fraction part */

  if(grid_units_h <= -0.5)
    *mx = view_grid_anchor[X] - ((nh - 1) * grid_state->gr_res_h);
  else if(0.5 <= grid_units_h)
    *mx = view_grid_anchor[X] - ((nh + 1) * grid_state->gr_res_h);
  else
    *mx = view_grid_anchor[X] - (nh * grid_state->gr_res_h);

  if(grid_units_v <= -0.5)
    *my = view_grid_anchor[Y] - ((nv - 1) * grid_state->gr_res_v);
  else if(0.5 <= grid_units_v)
    *my = view_grid_anchor[Y] - ((nv + 1) * grid_state->gr_res_v);
  else
    *my = view_grid_anchor[Y] - (nv  * grid_state->gr_res_v);

  *mx *= inv_sf;
  *my *= inv_sf;
}

void
snap_keypoint_to_grid(void)
{
  point_t view_pt;
  point_t model_pt;
  struct bu_vls cmd;

  if (dbip == DBI_NULL)
	  return;

  if (state != ST_S_EDIT && state != ST_O_EDIT) {
	  bu_log("snap_keypoint_to_grid: must be in an edit state\n");
	  return;
  }

  if (state == ST_S_EDIT) {
	  MAT4X3PNT(view_pt, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
  } else {
	  MAT4X3PNT(model_pt, modelchanges, e_axes_pos);
	  MAT4X3PNT(view_pt, view_state->vs_vop->vo_model2view, model_pt);
  }
  snap_to_grid(&view_pt[X], &view_pt[Y]);
  MAT4X3PNT(model_pt, view_state->vs_vop->vo_view2model, view_pt);
  VSCALE(model_pt, model_pt, base2local);

  bu_vls_init(&cmd);
  if (state == ST_S_EDIT)
	  bu_vls_printf(&cmd, "p %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
  else
	  bu_vls_printf(&cmd, "translate %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
  (void)Tcl_Eval(interp, bu_vls_addr(&cmd));
  bu_vls_free(&cmd);

  /* save model_pt in local units */
  VMOVE(dml_work_pt, model_pt);
  dml_mouse_dx = dml_mouse_dy = 0;
}

void
snap_view_center_to_grid(void)
{
	point_t view_pt, model_pt;

	if (dbip == DBI_NULL)
		return;

	MAT_DELTAS_GET_NEG(model_pt, view_state->vs_vop->vo_center);
	MAT4X3PNT(view_pt, view_state->vs_vop->vo_model2view, model_pt);
	snap_to_grid(&view_pt[X], &view_pt[Y]);
	MAT4X3PNT(model_pt, view_state->vs_vop->vo_view2model, view_pt);

	MAT_DELTAS_VEC_NEG(view_state->vs_vop->vo_center, model_pt);
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
round_to_grid(fastf_t *view_dx, fastf_t *view_dy)
{
  fastf_t grid_units_h, grid_units_v;
  fastf_t sf, inv_sf;
  int nh, nv;

  if (dbip == DBI_NULL ||
      NEAR_ZERO(grid_state->gr_res_h, (fastf_t)SMALL_FASTF) ||
      NEAR_ZERO(grid_state->gr_res_v, (fastf_t)SMALL_FASTF))
    return;

  sf = view_state->vs_vop->vo_scale*base2local;
  inv_sf = 1 / sf;

  /* convert mouse distance to grid units */
  grid_units_h = *view_dx * sf / grid_state->gr_res_h;
  grid_units_v = *view_dy * sf /  grid_state->gr_res_v;
  nh = grid_units_h;
  nv = grid_units_v;
  grid_units_h -= nh;
  grid_units_v -= nv;

  if(grid_units_h <= -0.5)
    *view_dx = (nh - 1) * grid_state->gr_res_h;
  else if(0.5 <= grid_units_h)
    *view_dx = (nh + 1) * grid_state->gr_res_h;
  else
    *view_dx = nh * grid_state->gr_res_h;

  if(grid_units_v <= -0.5)
    *view_dy = (nv - 1) * grid_state->gr_res_v;
  else if(0.5 <= grid_units_v)
    *view_dy = (nv + 1) * grid_state->gr_res_v;
  else
    *view_dy = nv * grid_state->gr_res_v;

  *view_dx *= inv_sf;
  *view_dy *= inv_sf;
}

void
snap_view_to_grid(fastf_t view_dx, fastf_t view_dy)
{
  point_t model_pt, view_pt;
  point_t vcenter, diff;

  if (dbip == DBI_NULL ||
      NEAR_ZERO(grid_state->gr_res_h, (fastf_t)SMALL_FASTF) ||
      NEAR_ZERO(grid_state->gr_res_v, (fastf_t)SMALL_FASTF))
    return;

  round_to_grid(&view_dx, &view_dy);

  VSET(view_pt, view_dx, view_dy, 0.0);

  MAT4X3PNT(model_pt, view_state->vs_vop->vo_view2model, view_pt);
  MAT_DELTAS_GET_NEG(vcenter, view_state->vs_vop->vo_center);
  VSUB2(diff, model_pt, vcenter);
  VSCALE(diff, diff, base2local);
  VSUB2(model_pt, dml_work_pt, diff);

  VSCALE(model_pt, model_pt, local2base);
  MAT_DELTAS_VEC_NEG(view_state->vs_vop->vo_center, model_pt);
  new_mats();
}

void
update_grids(fastf_t sf)
{
  register struct dm_list *dlp;
  struct bu_vls save_result;
  struct bu_vls cmd;

  FOR_ALL_DISPLAYS(dlp, &head_dm_list.l){
    dlp->dml_grid_state->gr_res_h *= sf;
    dlp->dml_grid_state->gr_res_v *= sf;
    VSCALE(dlp->dml_grid_state->gr_anchor, dlp->dml_grid_state->gr_anchor, sf);
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

int
f_grid_set (ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  struct bu_vls vls;

  bu_vls_init(&vls);

  if(argc < 1 || 5 < argc){
    bu_vls_printf(&vls, "help grid_set");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  mged_vls_struct_parse(&vls, "Grid", grid_vparse,
			(char *)grid_state, argc, argv);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_OK;
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
