/*
 *			A D C . C
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
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
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

void
adc_model_To_adc_view()
{
  MAT4X3PNT(adc_pos_view, model2view, adc_pos_model);
  dv_xadc = adc_pos_view[X] * GED_MAX;
  dv_yadc = adc_pos_view[Y] * GED_MAX;
}

void
adc_grid_To_adc_view()
{
  point_t model_pt;
  point_t view_pt;

  VSETALL(model_pt, 0.0);
  MAT4X3PNT(view_pt, model2view, model_pt);
  VADD2(adc_pos_view, view_pt, adc_pos_grid);
  dv_xadc = adc_pos_view[X] * GED_MAX;
  dv_yadc = adc_pos_view[Y] * GED_MAX;
}

void
adc_view_To_adc_grid()
{
  fastf_t f;
  point_t model_pt;
  point_t view_pt;

  VSETALL(model_pt, 0.0);
  MAT4X3PNT(view_pt, model2view, model_pt);
  VSUB2(adc_pos_grid, adc_pos_view, view_pt);
}

static void
draw_ticks(angle)
fastf_t angle;
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
  c_tdist = ((fastf_t)(dv_distadc) + 2047.0) * M_SQRT2_DIV2;

  d1 = c_tdist * cos (angle);
  d2 = c_tdist * sin (angle);
  t1 = 20.0 * sin (angle);
  t2 = 20.0 * cos (angle);

  /* Quadrant 1 */
  x1 = dv_xadc + d1 + t1;
  Y1 = dv_yadc + d2 - t2;
  x2 = dv_xadc + d1 -t1;
  y2 = dv_yadc + d2 + t2;
  if(clip(&x1, &Y1, &x2, &y2) == 0){
    DM_DRAW_LINE_2D(dmp, 
		    GED2PM1(x1), GED2PM1(Y1),
		    GED2PM1(x2), GED2PM1(y2));
  }

  /* Quadrant 2 */
  x1 = dv_xadc - d2 + t2;
  Y1 = dv_yadc + d1 + t1;
  x2 = dv_xadc - d2 - t2;
  y2 = dv_yadc + d1 - t1;
  if(clip (&x1, &Y1, &x2, &y2) == 0){
    DM_DRAW_LINE_2D(dmp,
		    GED2PM1(x1), GED2PM1(Y1),
		    GED2PM1(x2), GED2PM1(y2));
  }

  /* Quadrant 3 */
  x1 = dv_xadc - d1 - t1;
  Y1 = dv_yadc - d2 + t2;
  x2 = dv_xadc - d1 + t1;
  y2 = dv_yadc - d2 - t2;
  if(clip (&x1, &Y1, &x2, &y2) == 0){
    DM_DRAW_LINE_2D(dmp,
		    GED2PM1(x1), GED2PM1(Y1),
		    GED2PM1(x2), GED2PM1(y2));
  }

  /* Quadrant 4 */
  x1 = dv_xadc + d2 - t2;
  Y1 = dv_yadc - d1 - t1;
  x2 = dv_xadc + d2 + t2;
  y2 = dv_yadc - d1 + t1;
  if(clip (&x1, &Y1, &x2, &y2) == 0){
    DM_DRAW_LINE_2D(dmp,
		    GED2PM1(x1), GED2PM1(Y1),
		    GED2PM1(x2), GED2PM1(y2) );
  }
}

/*
 *			A D C U R S O R
 *
 * Compute and display the angle/distance cursor.
 */
void
adcursor()
{
  fastf_t x1, Y1;	/* not "y1", due to conflict with math lib */
  fastf_t x2, y2;
  fastf_t x3, y3;
  fastf_t x4, y4;
  fastf_t d1, d2;
  fastf_t t1, t2;
  fastf_t angle1, angle2;
  long idxy[2];
  fastf_t dx, dy;
  point_t model_pt;
  point_t view_pt;

  /*
   * Calculate a-d cursor displacement.
   */

  if(adc_anchor_pos == 1){
    adc_model_To_adc_view();
    adc_view_To_adc_grid();
  }else if(adc_anchor_pos == 2){
    adc_grid_To_adc_view();
    MAT4X3PNT(adc_pos_model, view2model, adc_pos_view);
  }else{
    adc_view_To_adc_grid();
    MAT4X3PNT(adc_pos_model, view2model, adc_pos_view);
  }

  if(adc_anchor_a1){
    fastf_t angle;

    MAT4X3PNT(view_pt, model2view, adc_anchor_pt_a1);
    dx = view_pt[X] * GED_MAX - dv_xadc;
    dy = view_pt[Y] * GED_MAX - dv_yadc;

    if(dx != 0 || dy != 0)
      adc_a1 = RAD2DEG*atan2(dy, dx);
  }

  if(adc_anchor_a2){
    fastf_t angle;

    MAT4X3PNT(view_pt, model2view, adc_anchor_pt_a2);
    dx = view_pt[X] * GED_MAX - dv_xadc;
    dy = view_pt[Y] * GED_MAX - dv_yadc;

    if(dx != 0 || dy != 0)
      adc_a2 = RAD2DEG*atan2(dy, dx);
  }

  if(adc_anchor_dst){
    fastf_t dist;

    MAT4X3PNT(view_pt, model2view, adc_anchor_pt_dst);

    dx = view_pt[X] * GED_MAX - dv_xadc;
    dy = view_pt[Y] * GED_MAX - dv_yadc;
    dist = sqrt(dx * dx + dy * dy);
    adc_dst = dist * INV_GED;
    dv_distadc = (dist / M_SQRT2_DIV2) - 2047;
  }else
    adc_dst = (dv_distadc * INV_GED + 1.0) * M_SQRT2_DIV2;

#if 0
  idxy[0] = dv_xadc;
  idxy[1] = dv_yadc;

  idxy[0] = (idxy[0] < GED_MIN ? GED_MIN : idxy[0]);
  idxy[0] = (idxy[0] > GED_MAX ? GED_MAX : idxy[0]);
  idxy[1] = (idxy[1] < GED_MIN ? GED_MIN : idxy[1]);
  idxy[1] = (idxy[1] > GED_MAX ? GED_MAX : idxy[1]);
#endif

  DM_SET_COLOR(dmp, DM_YELLOW_R, DM_YELLOW_G, DM_YELLOW_B, 1);
  DM_SET_LINE_ATTR(dmp, mged_variables->linewidth, 0);
  DM_DRAW_LINE_2D(dmp,
		  GED2PM1(GED_MIN), GED2PM1(dv_yadc),
		  GED2PM1(GED_MAX), GED2PM1(dv_yadc)); /* Horizontal */
  DM_DRAW_LINE_2D(dmp,
		  GED2PM1(dv_xadc), GED2PM1(GED_MAX),
		  GED2PM1(dv_xadc), GED2PM1(GED_MIN));  /* Vertical */

  angle1 = adc_a1 * DEG2RAD;
  angle2 = adc_a2 * DEG2RAD;

  /* sin for X and cos for Y to reverse sense of knob */
  d1 = cos (angle1) * 8000.0;
  d2 = sin (angle1) * 8000.0;
  x1 = dv_xadc + d1;
  Y1 = dv_yadc + d2;
  x2 = dv_xadc - d1;
  y2 = dv_yadc - d2;
  (void)clip ( &x1, &Y1, &x2, &y2 );

  x3 = dv_xadc + d2;
  y3 = dv_yadc - d1;
  x4 = dv_xadc - d2;
  y4 = dv_yadc + d1;
  (void)clip ( &x3, &y3, &x4, &y4 );

  DM_DRAW_LINE_2D(dmp, GED2PM1(x1), GED2PM1(Y1),
		   GED2PM1(x2), GED2PM1(y2));
  DM_DRAW_LINE_2D(dmp,
		  GED2PM1(x3), GED2PM1(y3),
		  GED2PM1(x4), GED2PM1(y4));

  d1 = cos(angle2) * 8000.0;
  d2 = sin(angle2) * 8000.0;
  x1 = dv_xadc + d1;
  Y1 = dv_yadc + d2;
  x2 = dv_xadc - d1;
  y2 = dv_yadc - d2;
  (void)clip(&x1, &Y1, &x2, &y2);

  x3 = dv_xadc + d2;
  y3 = dv_yadc - d1;
  x4 = dv_xadc - d2;
  y4 = dv_yadc + d1;
  (void)clip(&x3, &y3, &x4, &y4);

  DM_SET_LINE_ATTR(dmp, mged_variables->linewidth, 1);
  DM_DRAW_LINE_2D(dmp,
		  GED2PM1(x1), GED2PM1(Y1),
		  GED2PM1(x2), GED2PM1(y2));
  DM_DRAW_LINE_2D(dmp,
		  GED2PM1(x3), GED2PM1(y3),
		  GED2PM1(x4), GED2PM1(y4));
  DM_SET_LINE_ATTR(dmp, mged_variables->linewidth, 0);

  draw_ticks(0.0);
  draw_ticks(angle1);
  draw_ticks(angle2);
}

static void
adc_reset()
{
  dv_xadc = dv_yadc = 0;
  dv_1adc = dv_2adc = 0;
  dv_distadc = 0;

  VSETALL(adc_pos_view, 0.0);
  MAT4X3PNT(adc_pos_model, view2model, adc_pos_view);
  adc_dst = (dv_distadc * INV_GED + 1.0) * M_SQRT2_DIV2;
  adc_a1 = adc_a2 = 45.0;
  adc_view_To_adc_grid();

  VSETALL(adc_anchor_pt_a1, 0.0);
  VSETALL(adc_anchor_pt_a2, 0.0);
  VSETALL(adc_anchor_pt_dst, 0.0);

  adc_anchor_pos = 0;
  adc_anchor_a1 = 0;
  adc_anchor_a2 = 0;
  adc_anchor_dst = 0;
}

/*
 *			F _ A D C
 */

static char	adc_syntax[] = "\
 adc			toggle display of angle/distance cursor\n\
 adc draw [#]		set or get the draw parameter\n\
 adc a1 [#]		set or get angle1\n\
 adc a2 [#]		set or get angle2\n\
 adc dst [#]		set or get radius (distance) of tick\n\
 adc hv [# #]		set or get position (grid coordinates)\n\
 adc xyz [# # #]	set or get position (model coordinates)\n\
 adc dh #		add to horizontal position (grid coordinates)\n\
 adc dv #		add to vertical position (grid coordinates)\n\
 adc dx #		add to X position (model coordinates)\n\
 adc dy #		add to Y position (model coordinates)\n\
 adc dz #		add to Z position (model coordinates)\n\
 adc anchor_pos		anchor ADC to current position in model coordinates\n\
 adc anchor_a1		anchor angle1 to go through anchorpoint_a1\n\
 adc anchor_a2		anchor angle2 to go through anchorpoint_a2\n\
 adc anchor_dst		anchor tick distance to go through anchorpoint_dst\n\
 adc anchorpoint_a1 [# # #]	set or get anchor point for angle1\n\
 adc anchorpoint_a2 [# # #]	set or get anchor point for angle2\n\
 adc anchorpoint_dst [# # #]	set or get anchor point for tick distance\n\
 adc -i			any of the above appropriate commands will interpret parameters as increments\n\
 adc reset		reset angles, location, and tick distance\n\
 adc help		prints this help message\n\
";
int
f_adc (clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  struct bu_vls vls;
  char *parameter;
  char **argp = argv;
  point_t view_pt;
  point_t model_pt;
  point_t user_pt;		/* Value(s) provided by user */
  point_t diff;
  point_t scaled_pos;
  int incr_flag;
  int i;

  if(dbip == DBI_NULL)
    return TCL_OK;

  if(6 < argc){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help adc");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if(argc == 1){
    if(adc_draw)
      adc_draw = 0;
    else
      adc_draw = 1;

    if(adc_auto){
      adc_reset();
      adc_auto = 0;
    }

    dirty = 1;
    return TCL_OK;
  }

  if(strcmp(argv[1], "-i") == 0){
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
      bu_vls_printf(&vls, "%d", adc_draw);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      i = (int)user_pt[X];

      if(i)
	adc_draw = 1;
      else
	adc_draw = 0;

      dirty = 1;
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc draw' command accepts 0 or 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "a1") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e", adc_a1);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      if(!adc_anchor_a1){
	if(incr_flag)
	  adc_a1 += user_pt[0];
	else
	  adc_a1 = user_pt[0];

	dirty = 1;
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc a1' command accepts only 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "a2") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e", adc_a2);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      if(!adc_anchor_a2){
	if(incr_flag)
	  adc_a2 += user_pt[0];
	else
	  adc_a2 = user_pt[0];

	dirty = 1;
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc a2' command accepts 0 or 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dst") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e", adc_dst * Viewscale * base2local);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      if(!adc_anchor_dst){
	if(incr_flag)
	  adc_dst += user_pt[0] / (Viewscale * base2local);
	else
	  adc_dst = user_pt[0] / (Viewscale * base2local);

	dv_distadc = (adc_dst / M_SQRT2_DIV2 - 1.0) * GED_MAX;

	dirty = 1;
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dst' command accepts 0 or 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dh") == 0){
    if(argc == 1){
      if(!adc_anchor_pos){
	adc_pos_grid[X] += user_pt[0] / (Viewscale * base2local);
	adc_grid_To_adc_view();
	MAT4X3PNT(adc_pos_model, view2model, adc_pos_view);

	dirty = 1;
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dh' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dv") == 0){
    if(argc == 1){
      if(!adc_anchor_pos){
	adc_pos_grid[Y] += user_pt[0] / (Viewscale * base2local);
	adc_grid_To_adc_view();
	MAT4X3PNT(adc_pos_model, view2model, adc_pos_view);

	dirty = 1;
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
		    adc_pos_grid[X] * Viewscale * base2local,
		    adc_pos_grid[Y] * Viewscale * base2local);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 2){
      if(!adc_anchor_pos){
	if(incr_flag){
	  adc_pos_grid[X] += user_pt[X] / (Viewscale * base2local);
	  adc_pos_grid[Y] += user_pt[Y] / (Viewscale * base2local);
	}else{
	  adc_pos_grid[X] = user_pt[X] / (Viewscale * base2local);
	  adc_pos_grid[Y] = user_pt[Y] / (Viewscale * base2local);
	}

	adc_pos_grid[Z] = 0.0;
	adc_grid_To_adc_view();
	MAT4X3PNT(adc_pos_model, view2model, adc_pos_model);

	dirty = 1;
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc hv' command requires 0 or 2 arguments\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dx") == 0){
    if(argc == 1){
      if(!adc_anchor_pos){
	adc_pos_model[X] += user_pt[0] * local2base;
	adc_model_To_adc_view();
	adc_view_To_adc_grid();

	dirty = 1;
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dx' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dy") == 0){
    if(argc == 1){
      if(!adc_anchor_pos){
	adc_pos_model[Y] += user_pt[0] * local2base;
	adc_model_To_adc_view();
	adc_view_To_adc_grid();

	dirty = 1;
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dy' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dz") == 0){
    if(argc == 1){
      if(!adc_anchor_pos){
	adc_pos_model[Z] += user_pt[0] * local2base;
	adc_model_To_adc_view();
	adc_view_To_adc_grid();

	dirty = 1;
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dz' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "xyz") == 0){
    if(argc == 0){
      VSCALE(scaled_pos, adc_pos_model, base2local);

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(scaled_pos));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 3) {
      if(!adc_anchor_pos){
	VSCALE(user_pt, user_pt, local2base);

	if(incr_flag){
	  VADD2(adc_pos_model, adc_pos_model, user_pt);
	}else{
	  VMOVE(adc_pos_model, user_pt);
	}

	adc_model_To_adc_view();
	adc_view_To_adc_grid();

	dirty = 1;
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc xyz' command requires 0 or 3 arguments\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchor_pos") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", adc_anchor_pos);
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

      adc_anchor_pos = i;

      dirty = 1;
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchor_pos' command accepts 0 or 1 argument\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchor_a1") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", adc_anchor_a1);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      i = (int)user_pt[X];

      if(i)
	adc_anchor_a1 = 1;
      else
	adc_anchor_a1 = 0;

      dirty = 1;
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchor_a1' command accepts 0 or 1 argument\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchorpoint_a1") == 0){
    if(argc == 0){
      VSCALE(scaled_pos, adc_anchor_pt_a1, base2local);

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(scaled_pos));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 3){
      VSCALE(user_pt, user_pt, local2base);

      if(incr_flag){
	VADD2(adc_anchor_pt_a1, adc_anchor_pt_a1, user_pt);
      }else{
	VMOVE(adc_anchor_pt_a1, user_pt);
      }

      dirty = 1;
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchorpoint_a1' command accepts 0 or 3 arguments\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchor_a2") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", adc_anchor_a2);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      i = (int)user_pt[X];

      if(i)
	adc_anchor_a2 = 1;
      else
	adc_anchor_a2 = 0;

      dirty = 1;
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchor_a2' command accepts 0 or 1 argument\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchorpoint_a2") == 0){
    if(argc == 0){
      VSCALE(scaled_pos, adc_anchor_pt_a2, base2local);

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(scaled_pos));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 3){
      VSCALE(user_pt, user_pt, local2base);

      if(incr_flag){
	VADD2(adc_anchor_pt_a2, adc_anchor_pt_a2, user_pt);
      }else{
	VMOVE(adc_anchor_pt_a2, user_pt);
      }

      dirty = 1;
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchorpoint_a2' command accepts 0 or 3 arguments\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchor_dst") == 0){
    if(argc == 0){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d", adc_anchor_dst);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 1){
      i = (int)user_pt[X];

      if(i)
	adc_anchor_dst = 1;
      else
	adc_anchor_dst = 0;

      dirty = 1;
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchor_dst' command accepts 0 or 1 argument\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchorpoint_dst") == 0){
    if(argc == 0){
      VSCALE(scaled_pos, adc_anchor_pt_dst, base2local);

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(scaled_pos));
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 3){
      VSCALE(user_pt, user_pt, local2base);

      if(incr_flag){
	VADD2(adc_anchor_pt_dst, adc_anchor_pt_dst, user_pt);
      }else{
	VMOVE(adc_anchor_pt_dst, user_pt);
      }

      dirty = 1;
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc anchorpoint_dst' command accepts 0 or 3 arguments\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if( strcmp(parameter, "reset") == 0)  {
    if (argc == 0) {
      adc_reset();

      dirty = 1;
      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc reset' command accepts no arguments\n", (char *)NULL);
    return TCL_ERROR;
  }

  if( strcmp(parameter, "help") == 0)  {
    Tcl_AppendResult(interp, "Usage:\n", adc_syntax, (char *)NULL);
    return TCL_OK;
  }

  Tcl_AppendResult(interp, "ADC: unrecognized command: '",
		   argv[1], "'\nUsage:\n", adc_syntax, (char *)NULL);
  return TCL_ERROR;
}
