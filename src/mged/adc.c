/*                           A D C . C
 * BRL-CAD
 *
 * Copyright (C) 1985-2005 United States Government as represented by
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
/** @file adc.c
 *
 * Functions -
 *	adcursor	implement the angle/distance cursor
 *	f_adc		control angle/distance cursor from keyboard
 *
 * Authors -
 *	Gary Steven Moss
 *	Paul J. Tanenbaum
 *	Robert G. Parker
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"

#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880
#endif

#ifndef M_SQRT2_DIV2
#define M_SQRT2_DIV2       0.70710678118654752440
#endif

static void adc_print_vars(void);

void
adc_set_dirty_flag(void)
{
  struct dm_list *dmlp;

  FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
    if(dmlp->dml_adc_state == adc_state)
      dmlp->dml_dirty = 1;
}

void
adc_set_scroll(void)
{
  struct dm_list *dmlp;
  struct dm_list *save_dmlp;

  save_dmlp = curr_dm_list;

  FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
    if(dmlp->dml_adc_state == adc_state){
      curr_dm_list = dmlp;
      set_scroll();
      dirty = 1;
    }

  curr_dm_list = save_dmlp;
}

static void
adc_model_To_adc_view(void)
{
  MAT4X3PNT(adc_state->adc_pos_view, view_state->vs_vop->vo_model2view, adc_state->adc_pos_model);
  adc_state->adc_dv_x = adc_state->adc_pos_view[X] * GED_MAX;
  adc_state->adc_dv_y = adc_state->adc_pos_view[Y] * GED_MAX;
}

static void
adc_grid_To_adc_view(void)
{
  point_t model_pt;
  point_t view_pt;

  VSETALL(model_pt, 0.0);
  MAT4X3PNT(view_pt, view_state->vs_vop->vo_model2view, model_pt);
  VADD2(adc_state->adc_pos_view, view_pt, adc_state->adc_pos_grid);
  adc_state->adc_dv_x = adc_state->adc_pos_view[X] * GED_MAX;
  adc_state->adc_dv_y = adc_state->adc_pos_view[Y] * GED_MAX;
}

static void
adc_view_To_adc_grid(void)
{
  point_t model_pt;
  point_t view_pt;

  VSETALL(model_pt, 0.0);
  MAT4X3PNT(view_pt, view_state->vs_vop->vo_model2view, model_pt);
  VSUB2(adc_state->adc_pos_grid, adc_state->adc_pos_view, view_pt);
}

static void
calc_adc_pos(void)
{
  if(adc_state->adc_anchor_pos == 1){
    adc_model_To_adc_view();
    adc_view_To_adc_grid();
  }else if(adc_state->adc_anchor_pos == 2){
    adc_grid_To_adc_view();
    MAT4X3PNT(adc_state->adc_pos_model, view_state->vs_vop->vo_view2model, adc_state->adc_pos_view);
  }else{
    adc_view_To_adc_grid();
    MAT4X3PNT(adc_state->adc_pos_model, view_state->vs_vop->vo_view2model, adc_state->adc_pos_view);
  }
}

static void
calc_adc_a1(void)
{
  if(adc_state->adc_anchor_a1){
    fastf_t dx, dy;
    point_t view_pt;

    MAT4X3PNT(view_pt, view_state->vs_vop->vo_model2view, adc_state->adc_anchor_pt_a1);
    dx = view_pt[X] * GED_MAX - adc_state->adc_dv_x;
    dy = view_pt[Y] * GED_MAX - adc_state->adc_dv_y;

    if(dx != 0.0 || dy != 0.0){
      adc_state->adc_a1 = RAD2DEG*atan2(dy, dx);
      adc_state->adc_dv_a1 = (1.0 - (adc_state->adc_a1 / 45.0)) * GED_MAX;
    }
  }
}

static void
calc_adc_a2(void)
{
  if(adc_state->adc_anchor_a2){
    fastf_t dx, dy;
    point_t view_pt;

    MAT4X3PNT(view_pt, view_state->vs_vop->vo_model2view, adc_state->adc_anchor_pt_a2);
    dx = view_pt[X] * GED_MAX - adc_state->adc_dv_x;
    dy = view_pt[Y] * GED_MAX - adc_state->adc_dv_y;

    if(dx != 0.0 || dy != 0.0){
      adc_state->adc_a2 = RAD2DEG*atan2(dy, dx);
      adc_state->adc_dv_a2 = (1.0 - (adc_state->adc_a2 / 45.0)) * GED_MAX;
    }
  }
}

static void
calc_adc_dst(void)
{
  if(adc_state->adc_anchor_dst){
    fastf_t dist;
    fastf_t dx, dy;
    point_t view_pt;

    MAT4X3PNT(view_pt, view_state->vs_vop->vo_model2view, adc_state->adc_anchor_pt_dst);

    dx = view_pt[X] * GED_MAX - adc_state->adc_dv_x;
    dy = view_pt[Y] * GED_MAX - adc_state->adc_dv_y;
    dist = sqrt(dx * dx + dy * dy);
    adc_state->adc_dst = dist * INV_GED;
    adc_state->adc_dv_dist = (dist / M_SQRT2_DIV2) - GED_MAX;
  }else
    adc_state->adc_dst = (adc_state->adc_dv_dist * INV_GED + 1.0) * M_SQRT2_DIV2;
}

static void
draw_ticks(fastf_t angle)
{
  fastf_t c_tdist;
  fastf_t d1, d2;
  fastf_t t1, t2;
  fastf_t x1, Y1;       /* not "y1", due to conflict with math lib */
  fastf_t x2, y2;

  /*
   * Position tic marks from dial 9.
   */
  /* map -2048 - 2047 into 0 - 2048 * sqrt (2) */
  /* Tick distance */
  c_tdist = ((fastf_t)(adc_state->adc_dv_dist) + GED_MAX) * M_SQRT2_DIV2;

  d1 = c_tdist * cos (angle);
  d2 = c_tdist * sin (angle);
  t1 = 20.0 * sin (angle);
  t2 = 20.0 * cos (angle);

  /* Quadrant 1 */
  x1 = adc_state->adc_dv_x + d1 + t1;
  Y1 = adc_state->adc_dv_y + d2 - t2;
  x2 = adc_state->adc_dv_x + d1 -t1;
  y2 = adc_state->adc_dv_y + d2 + t2;
  if(clip(&x1, &Y1, &x2, &y2) == 0){
    DM_DRAW_LINE_2D(dmp, 
		    GED2PM1(x1), GED2PM1(Y1) * dmp->dm_aspect,
		    GED2PM1(x2), GED2PM1(y2) * dmp->dm_aspect);
  }

  /* Quadrant 2 */
  x1 = adc_state->adc_dv_x - d2 + t2;
  Y1 = adc_state->adc_dv_y + d1 + t1;
  x2 = adc_state->adc_dv_x - d2 - t2;
  y2 = adc_state->adc_dv_y + d1 - t1;
  if(clip (&x1, &Y1, &x2, &y2) == 0){
    DM_DRAW_LINE_2D(dmp,
		    GED2PM1(x1), GED2PM1(Y1) * dmp->dm_aspect,
		    GED2PM1(x2), GED2PM1(y2) * dmp->dm_aspect);
  }

  /* Quadrant 3 */
  x1 = adc_state->adc_dv_x - d1 - t1;
  Y1 = adc_state->adc_dv_y - d2 + t2;
  x2 = adc_state->adc_dv_x - d1 + t1;
  y2 = adc_state->adc_dv_y - d2 - t2;
  if(clip (&x1, &Y1, &x2, &y2) == 0){
    DM_DRAW_LINE_2D(dmp,
		    GED2PM1(x1), GED2PM1(Y1) * dmp->dm_aspect,
		    GED2PM1(x2), GED2PM1(y2) * dmp->dm_aspect);
  }

  /* Quadrant 4 */
  x1 = adc_state->adc_dv_x + d2 - t2;
  Y1 = adc_state->adc_dv_y - d1 - t1;
  x2 = adc_state->adc_dv_x + d2 + t2;
  y2 = adc_state->adc_dv_y - d1 + t1;
  if(clip (&x1, &Y1, &x2, &y2) == 0){
    DM_DRAW_LINE_2D(dmp,
		    GED2PM1(x1), GED2PM1(Y1) * dmp->dm_aspect,
		    GED2PM1(x2), GED2PM1(y2) * dmp->dm_aspect);
  }
}

/*
 *			A D C U R S O R
 *
 * Compute and display the angle/distance cursor.
 */
void
adcursor(void)
{
  fastf_t x1, Y1;	/* not "y1", due to conflict with math lib */
  fastf_t x2, y2;
  fastf_t x3, y3;
  fastf_t x4, y4;
  fastf_t d1, d2;
  fastf_t angle1, angle2;

  calc_adc_pos();
  calc_adc_a1();
  calc_adc_a2();
  calc_adc_dst();

  DM_SET_FGCOLOR(dmp,
		 color_scheme->cs_adc_line[0],
		 color_scheme->cs_adc_line[1],
		 color_scheme->cs_adc_line[2], 1, 1.0);
  DM_SET_LINE_ATTR(dmp, mged_variables->mv_linewidth, 0);

  /* Horizontal */
  DM_DRAW_LINE_2D(dmp,
		  GED2PM1(GED_MIN), GED2PM1(adc_state->adc_dv_y) * dmp->dm_aspect,
		  GED2PM1(GED_MAX), GED2PM1(adc_state->adc_dv_y) * dmp->dm_aspect);

  /* Vertical */
  DM_DRAW_LINE_2D(dmp,
		  GED2PM1(adc_state->adc_dv_x), GED2PM1(GED_MAX),
		  GED2PM1(adc_state->adc_dv_x), GED2PM1(GED_MIN));

  angle1 = adc_state->adc_a1 * DEG2RAD;
  angle2 = adc_state->adc_a2 * DEG2RAD;

  /* sin for X and cos for Y to reverse sense of knob */
  d1 = cos (angle1) * 8000.0;
  d2 = sin (angle1) * 8000.0;
  x1 = adc_state->adc_dv_x + d1;
  Y1 = adc_state->adc_dv_y + d2;
  x2 = adc_state->adc_dv_x - d1;
  y2 = adc_state->adc_dv_y - d2;

  x3 = adc_state->adc_dv_x + d2;
  y3 = adc_state->adc_dv_y - d1;
  x4 = adc_state->adc_dv_x - d2;
  y4 = adc_state->adc_dv_y + d1;

  DM_DRAW_LINE_2D(dmp,
		  GED2PM1(x1), GED2PM1(Y1) * dmp->dm_aspect,
		  GED2PM1(x2), GED2PM1(y2) * dmp->dm_aspect);
  DM_DRAW_LINE_2D(dmp,
		  GED2PM1(x3), GED2PM1(y3) * dmp->dm_aspect,
		  GED2PM1(x4), GED2PM1(y4) * dmp->dm_aspect);

  d1 = cos(angle2) * 8000.0;
  d2 = sin(angle2) * 8000.0;
  x1 = adc_state->adc_dv_x + d1;
  Y1 = adc_state->adc_dv_y + d2;
  x2 = adc_state->adc_dv_x - d1;
  y2 = adc_state->adc_dv_y - d2;

  x3 = adc_state->adc_dv_x + d2;
  y3 = adc_state->adc_dv_y - d1;
  x4 = adc_state->adc_dv_x - d2;
  y4 = adc_state->adc_dv_y + d1;

  DM_SET_LINE_ATTR(dmp, mged_variables->mv_linewidth, 1);
  DM_DRAW_LINE_2D(dmp,
		  GED2PM1(x1), GED2PM1(Y1) * dmp->dm_aspect,
		  GED2PM1(x2), GED2PM1(y2) * dmp->dm_aspect);
  DM_DRAW_LINE_2D(dmp,
		  GED2PM1(x3), GED2PM1(y3) * dmp->dm_aspect,
		  GED2PM1(x4), GED2PM1(y4) * dmp->dm_aspect);
  DM_SET_LINE_ATTR(dmp, mged_variables->mv_linewidth, 0);

  DM_SET_FGCOLOR(dmp,
		 color_scheme->cs_adc_tick[0],
		 color_scheme->cs_adc_tick[1],
		 color_scheme->cs_adc_tick[2], 1, 1.0);
  draw_ticks(0.0);
  draw_ticks(angle1);
  draw_ticks(angle2);
}

static void
adc_reset(void)
{
  adc_state->adc_dv_x = adc_state->adc_dv_y = 0;
  adc_state->adc_dv_a1 = adc_state->adc_dv_a2 = 0;
  adc_state->adc_dv_dist = 0;

  VSETALL(adc_state->adc_pos_view, 0.0);
  MAT4X3PNT(adc_state->adc_pos_model, view_state->vs_vop->vo_view2model, adc_state->adc_pos_view);
  adc_state->adc_dst = (adc_state->adc_dv_dist * INV_GED + 1.0) * M_SQRT2_DIV2;
  adc_state->adc_a1 = adc_state->adc_a2 = 45.0;
  adc_view_To_adc_grid();

  VSETALL(adc_state->adc_anchor_pt_a1, 0.0);
  VSETALL(adc_state->adc_anchor_pt_a2, 0.0);
  VSETALL(adc_state->adc_anchor_pt_dst, 0.0);

  adc_state->adc_anchor_pos = 0;
  adc_state->adc_anchor_a1 = 0;
  adc_state->adc_anchor_a2 = 0;
  adc_state->adc_anchor_dst = 0;
}

/*
 *			F _ A D C
 */

static char	adc_syntax[] = "\
 adc			toggle display of angle/distance cursor\n\
 adc vars		print a list of all variables (i.e. var = val)\n\
 adc draw [0|1]		set or get the draw parameter\n\
 adc a1 [#]		set or get angle1\n\
 adc a2 [#]		set or get angle2\n\
 adc dst [#]		set or get radius (distance) of tick\n\
 adc odst [#]		set or get radius (distance) of tick (+-2047)\n\
 adc hv [# #]		set or get position (grid coordinates)\n\
 adc xyz [# # #]	set or get position (model coordinates)\n\
 adc x [#]		set or get horizontal position (+-2047)\n\
 adc y [#]		set or get vertical position (+-2047)\n\
 adc dh #		add to horizontal position (grid coordinates)\n\
 adc dv #		add to vertical position (grid coordinates)\n\
 adc dx #		add to X position (model coordinates)\n\
 adc dy #		add to Y position (model coordinates)\n\
 adc dz #		add to Z position (model coordinates)\n\
 adc anchor_pos	[0|1]	anchor ADC to current position in model coordinates\n\
 adc anchor_a1	[0|1]	anchor angle1 to go through anchorpoint_a1\n\
 adc anchor_a2	[0|1]	anchor angle2 to go through anchorpoint_a2\n\
 adc anchor_dst	[0|1]	anchor tick distance to go through anchorpoint_dst\n\
 adc anchorpoint_a1 [# # #]	set or get anchor point for angle1\n\
 adc anchorpoint_a2 [# # #]	set or get anchor point for angle2\n\
 adc anchorpoint_dst [# # #]	set or get anchor point for tick distance\n\
 adc -i			any of the above appropriate commands will interpret parameters as increments\n\
 adc reset		reset angles, location, and tick distance\n\
 adc help		prints this help message\n\
";

int
f_adc (
	ClientData clientData,
	Tcl_Interp *interp,
	int	argc,
	char	**argv)
{
  struct bu_vls vls;
  char *parameter;
  char **argp = argv;
  point_t user_pt;		/* Value(s) provided by user */
  point_t scaled_pos;
  int incr_flag;
  int i;

  CHECK_DBI_NULL;

  if(6 < argc){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help adc");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if(argc == 1){
    if(adc_state->adc_draw)
      adc_state->adc_draw = 0;
    else
      adc_state->adc_draw = 1;

    if(adc_auto){
      adc_reset();
      adc_auto = 0;
    }

    adc_set_scroll();

    return TCL_OK;
  }

  if (strcmp(argv[1], "-i") == 0) {
	  if (argc < 4) {
		  bu_vls_init(&vls);
		  bu_vls_printf(&vls, "adc: -i option specified without an op-val pair");
		  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		  bu_vls_free(&vls);

		  return TCL_ERROR;
	  }

	  incr_flag = 1;
	  parameter = argv[2];
	  argc -= 3;
	  argp += 3;
  }else{
    incr_flag = 0;
    parameter = argv[1];
    argc -= 2;
    argp += 2;
  }

  for (i = 0; i < argc; ++i)
    user_pt[i] = atof(argp[i]);

  if(strcmp(parameter, "draw") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", adc_state->adc_draw);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      i = (int)user_pt[X];

      if(i)
	adc_state->adc_draw = 1;
      else
	adc_state->adc_draw = 0;

      adc_set_scroll();

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc draw' command accepts 0 or 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "a1") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e", adc_state->adc_a1);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      if(!adc_state->adc_anchor_a1){
	if(incr_flag)
	  adc_state->adc_a1 += user_pt[0];
	else
	  adc_state->adc_a1 = user_pt[0];

	adc_state->adc_dv_a1 = (1.0 - (adc_state->adc_a1 / 45.0)) * GED_MAX;
	adc_set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc a1' command accepts only 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "a2") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e", adc_state->adc_a2);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      if(!adc_state->adc_anchor_a2){
	if(incr_flag)
	  adc_state->adc_a2 += user_pt[0];
	else
	  adc_state->adc_a2 = user_pt[0];

	adc_state->adc_dv_a2 = (1.0 - (adc_state->adc_a2 / 45.0)) * GED_MAX;
	adc_set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc a2' command accepts 0 or 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dst") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e", adc_state->adc_dst * view_state->vs_vop->vo_scale * base2local);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      if(!adc_state->adc_anchor_dst){
	if(incr_flag)
	  adc_state->adc_dst += user_pt[0] / (view_state->vs_vop->vo_scale * base2local);
	else
	  adc_state->adc_dst = user_pt[0] / (view_state->vs_vop->vo_scale * base2local);

	adc_state->adc_dv_dist = (adc_state->adc_dst / M_SQRT2_DIV2 - 1.0) * GED_MAX;

	adc_set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dst' command accepts 0 or 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "odst") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", adc_state->adc_dv_dist);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      if(!adc_state->adc_anchor_dst){
	if(incr_flag)
	  adc_state->adc_dv_dist += user_pt[0];
	else
	  adc_state->adc_dv_dist = user_pt[0];

	adc_state->adc_dst = (adc_state->adc_dv_dist * INV_GED + 1.0) * M_SQRT2_DIV2;
	adc_set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc odst' command accepts 0 or 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dh") == 0){
    if(argc == 1){
      if(!adc_state->adc_anchor_pos){
	adc_state->adc_pos_grid[X] += user_pt[0] / (view_state->vs_vop->vo_scale * base2local);
	adc_grid_To_adc_view();
	MAT4X3PNT(adc_state->adc_pos_model, view_state->vs_vop->vo_view2model, adc_state->adc_pos_view);

	adc_set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dh' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dv") == 0){
    if(argc == 1){
      if(!adc_state->adc_anchor_pos){
	adc_state->adc_pos_grid[Y] += user_pt[0] / (view_state->vs_vop->vo_scale * base2local);
	adc_grid_To_adc_view();
	MAT4X3PNT(adc_state->adc_pos_model, view_state->vs_vop->vo_view2model, adc_state->adc_pos_view);

	adc_set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dv' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "hv") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e %.15e",
		    adc_state->adc_pos_grid[X] * view_state->vs_vop->vo_scale * base2local,
		    adc_state->adc_pos_grid[Y] * view_state->vs_vop->vo_scale * base2local);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 2){
      if(!adc_state->adc_anchor_pos){
	if(incr_flag){
	  adc_state->adc_pos_grid[X] += user_pt[X] / (view_state->vs_vop->vo_scale * base2local);
	  adc_state->adc_pos_grid[Y] += user_pt[Y] / (view_state->vs_vop->vo_scale * base2local);
	}else{
	  adc_state->adc_pos_grid[X] = user_pt[X] / (view_state->vs_vop->vo_scale * base2local);
	  adc_state->adc_pos_grid[Y] = user_pt[Y] / (view_state->vs_vop->vo_scale * base2local);
	}

	adc_state->adc_pos_grid[Z] = 0.0;
	adc_grid_To_adc_view();
	MAT4X3PNT(adc_state->adc_pos_model, view_state->vs_vop->vo_view2model, adc_state->adc_pos_model);

	adc_set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc hv' command requires 0 or 2 arguments\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dx") == 0){
    if(argc == 1){
      if(!adc_state->adc_anchor_pos){
	adc_state->adc_pos_model[X] += user_pt[0] * local2base;
	adc_model_To_adc_view();
	adc_view_To_adc_grid();

	adc_set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dx' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dy") == 0){
    if(argc == 1){
      if(!adc_state->adc_anchor_pos){
	adc_state->adc_pos_model[Y] += user_pt[0] * local2base;
	adc_model_To_adc_view();
	adc_view_To_adc_grid();

	adc_set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dy' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dz") == 0){
    if(argc == 1){
      if(!adc_state->adc_anchor_pos){
	adc_state->adc_pos_model[Z] += user_pt[0] * local2base;
	adc_model_To_adc_view();
	adc_view_To_adc_grid();

	adc_set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dz' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "xyz") == 0){
    if(argc == 0){
      VSCALE(scaled_pos, adc_state->adc_pos_model, base2local);

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(scaled_pos));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 3) {
      VSCALE(user_pt, user_pt, local2base);

      if(incr_flag){
	VADD2(adc_state->adc_pos_model, adc_state->adc_pos_model, user_pt);
      }else{
	VMOVE(adc_state->adc_pos_model, user_pt);
      }

      adc_model_To_adc_view();
      adc_view_To_adc_grid();

      adc_set_dirty_flag();

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc xyz' command requires 0 or 3 arguments\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "x") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", adc_state->adc_dv_x);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1) {
      if(!adc_state->adc_anchor_pos){
	if(incr_flag){
	  adc_state->adc_dv_x += user_pt[0];
	}else{
	  adc_state->adc_dv_x = user_pt[0];
	}

	adc_state->adc_pos_view[X] = adc_state->adc_dv_x * INV_GED;
	adc_state->adc_pos_view[Y] = adc_state->adc_dv_y * INV_GED;
	adc_view_To_adc_grid();
	MAT4X3PNT(adc_state->adc_pos_model, view_state->vs_vop->vo_view2model, adc_state->adc_pos_view);

	adc_set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc x' command requires 0 or 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "y") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", adc_state->adc_dv_y);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1) {
      if(!adc_state->adc_anchor_pos){
	if(incr_flag){
	  adc_state->adc_dv_y += user_pt[0];
	}else{
	  adc_state->adc_dv_y = user_pt[0];
	}

	adc_state->adc_pos_view[X] = adc_state->adc_dv_x * INV_GED;
	adc_state->adc_pos_view[Y] = adc_state->adc_dv_y * INV_GED;
	adc_view_To_adc_grid();
	MAT4X3PNT(adc_state->adc_pos_model, view_state->vs_vop->vo_view2model, adc_state->adc_pos_view);

	adc_set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc y' command requires 0 or 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchor_pos") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", adc_state->adc_anchor_pos);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      i = (int)user_pt[X];

      if(i < 0 || 2 < i){
	Tcl_AppendResult(interp, "The 'adc anchor_pos parameter accepts values of 0, 1, or 2.",
			 (char *)NULL);
	return TCL_ERROR;
      }

      adc_state->adc_anchor_pos = i;

      calc_adc_pos();
      adc_set_dirty_flag();
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchor_pos' command accepts 0 or 1 argument\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchor_a1") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", adc_state->adc_anchor_a1);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      i = (int)user_pt[X];

      if(i)
	adc_state->adc_anchor_a1 = 1;
      else
	adc_state->adc_anchor_a1 = 0;

      calc_adc_a1();
      adc_set_dirty_flag();
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchor_a1' command accepts 0 or 1 argument\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchorpoint_a1") == 0){
    if(argc == 0){
      VSCALE(scaled_pos, adc_state->adc_anchor_pt_a1, base2local);

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(scaled_pos));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 3){
      VSCALE(user_pt, user_pt, local2base);

      if(incr_flag){
	VADD2(adc_state->adc_anchor_pt_a1, adc_state->adc_anchor_pt_a1, user_pt);
      }else{
	VMOVE(adc_state->adc_anchor_pt_a1, user_pt);
      }

      calc_adc_a1();
      adc_set_dirty_flag();
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchorpoint_a1' command accepts 0 or 3 arguments\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchor_a2") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", adc_state->adc_anchor_a2);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      i = (int)user_pt[X];

      if(i)
	adc_state->adc_anchor_a2 = 1;
      else
	adc_state->adc_anchor_a2 = 0;

      calc_adc_a2();
      adc_set_dirty_flag();
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchor_a2' command accepts 0 or 1 argument\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchorpoint_a2") == 0){
    if(argc == 0){
      VSCALE(scaled_pos, adc_state->adc_anchor_pt_a2, base2local);

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(scaled_pos));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 3){
      VSCALE(user_pt, user_pt, local2base);

      if(incr_flag){
	VADD2(adc_state->adc_anchor_pt_a2, adc_state->adc_anchor_pt_a2, user_pt);
      }else{
	VMOVE(adc_state->adc_anchor_pt_a2, user_pt);
      }

      calc_adc_a2();
      adc_set_dirty_flag();
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchorpoint_a2' command accepts 0 or 3 arguments\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchor_dst") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", adc_state->adc_anchor_dst);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      i = (int)user_pt[X];

      if(i){
	adc_state->adc_anchor_dst = 1;
      }else
	adc_state->adc_anchor_dst = 0;

      calc_adc_dst();
      adc_set_dirty_flag();
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchor_dst' command accepts 0 or 1 argument\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchorpoint_dst") == 0){
    if(argc == 0){
      VSCALE(scaled_pos, adc_state->adc_anchor_pt_dst, base2local);

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(scaled_pos));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 3){
      VSCALE(user_pt, user_pt, local2base);

      if(incr_flag){
	VADD2(adc_state->adc_anchor_pt_dst, adc_state->adc_anchor_pt_dst, user_pt);
      }else{
	VMOVE(adc_state->adc_anchor_pt_dst, user_pt);
      }

      calc_adc_dst();
      adc_set_dirty_flag();
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchorpoint_dst' command accepts 0 or 3 arguments\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "reset") == 0){
    if (argc == 0) {
      adc_reset();

      adc_set_dirty_flag();
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc reset' command accepts no arguments\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "vars") == 0){
    adc_print_vars();
    return TCL_OK;
  }

  if(strcmp(parameter, "help") == 0){
    Tcl_AppendResult(interp, "Usage:\n", adc_syntax, (char *)NULL);
    return TCL_OK;
  }

  Tcl_AppendResult(interp, "ADC: unrecognized command: '",
		   argv[1], "'\nUsage:\n", adc_syntax, (char *)NULL);
  return TCL_ERROR;
}

static void
adc_print_vars(void)
{
  struct bu_vls vls;

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "draw = %d\n", adc_state->adc_draw);
  bu_vls_printf(&vls, "a1 = %.15e\n", adc_state->adc_a1);
  bu_vls_printf(&vls, "a2 = %.15e\n", adc_state->adc_a2);
  bu_vls_printf(&vls, "dst = %.15e\n", adc_state->adc_dst * view_state->vs_vop->vo_scale * base2local);
  bu_vls_printf(&vls, "odst = %d\n", adc_state->adc_dv_dist);
  bu_vls_printf(&vls, "hv = %.15e %.15e\n",
		adc_state->adc_pos_grid[X] * view_state->vs_vop->vo_scale * base2local,
		adc_state->adc_pos_grid[Y] * view_state->vs_vop->vo_scale * base2local);
  bu_vls_printf(&vls, "xyz = %.15e %.15e %.15e\n",
		adc_state->adc_pos_model[X] * base2local,
		adc_state->adc_pos_model[Y] * base2local,
		adc_state->adc_pos_model[Z] * base2local);
  bu_vls_printf(&vls, "x = %d\n", adc_state->adc_dv_x);
  bu_vls_printf(&vls, "y = %d\n", adc_state->adc_dv_y);
  bu_vls_printf(&vls, "anchor_pos = %d\n", adc_state->adc_anchor_pos);
  bu_vls_printf(&vls, "anchor_a1 = %d\n", adc_state->adc_anchor_a1);
  bu_vls_printf(&vls, "anchor_a2 = %d\n", adc_state->adc_anchor_a2);
  bu_vls_printf(&vls, "anchor_dst = %d\n", adc_state->adc_anchor_dst);
  bu_vls_printf(&vls, "anchorpoint_a1 = %.15e %.15e %.15e\n",
		adc_state->adc_anchor_pt_a1[X] * base2local,
		adc_state->adc_anchor_pt_a1[Y] * base2local,
		adc_state->adc_anchor_pt_a1[Z] * base2local);
  bu_vls_printf(&vls, "anchorpoint_a2 = %.15e %.15e %.15e\n",
		adc_state->adc_anchor_pt_a2[X] * base2local,
		adc_state->adc_anchor_pt_a2[Y] * base2local,
		adc_state->adc_anchor_pt_a2[Z] * base2local);
  bu_vls_printf(&vls, "anchorpoint_dst = %.15e %.15e %.15e\n",
		adc_state->adc_anchor_pt_dst[X] * base2local,
		adc_state->adc_anchor_pt_dst[Y] * base2local,
		adc_state->adc_anchor_pt_dst[Z] * base2local);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);
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
