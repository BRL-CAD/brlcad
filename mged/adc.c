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

extern void set_dirty_flag();		/* defined in set.c */
extern void set_adc_xyz();		/* defined in set.c */
extern void set_adc_xyz_grid();		/* defined in set.c */

#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880
#endif

#ifndef M_SQRT2_DIV2
#define M_SQRT2_DIV2       0.70710678118654752440
#endif

void
adc_xyz_To_dv_xyadc()
{
  point_t model_pt;
  point_t view_pt;

  VSCALE(model_pt, mged_variables->adc_xyz, local2base);
  MAT4X3PNT(view_pt, model2view, model_pt);
  dv_xadc = view_pt[X] * GED_MAX;
  dv_yadc = view_pt[Y] * GED_MAX;
}

void
dv_xyadc_To_adc_xyz()
{
  point_t model_pt;
  point_t view_pt;

  VSET(view_pt, dv_xadc * INV_GED, dv_yadc * INV_GED, 0.0);
  MAT4X3PNT(model_pt, view2model, view_pt);
  VSCALE(mged_variables->adc_xyz, model_pt, base2local);
}

void
adc_xyz_grid_To_dv_xyadc()
{
  fastf_t f;
  point_t model_pt;
  point_t view_pt;

  VSETALL(model_pt, 0.0);
  MAT4X3PNT(view_pt, model2view, model_pt);
  f = Viewscale * base2local;
  VSCALE(view_pt, view_pt, f);

  f = 2047.0 / (Viewscale * base2local);
  dv_xadc = (view_pt[X] + mged_variables->adc_xyz_grid[X]) * f;
  dv_yadc = (view_pt[Y] + mged_variables->adc_xyz_grid[Y]) * f;
}

void
dv_xyadc_To_adc_xyz_grid()
{
  fastf_t f;
  point_t model_pt;
  point_t view_pt;

  VSETALL(model_pt, 0.0);
  MAT4X3PNT(view_pt, model2view, model_pt);
  VSCALE(view_pt, view_pt, GED_MAX);

  f = INV_GED * Viewscale * base2local;
  mged_variables->adc_xyz_grid[X] = (dv_xadc - view_pt[X]) * f;
  mged_variables->adc_xyz_grid[Y] = (dv_yadc - view_pt[Y]) * f;
}

void
adc_xyz_To_adc_xyz_grid()
{
  fastf_t f;
  point_t model_pt;
  point_t xyz_view_pt;		/* ADC's xyz in view space */
  point_t mo_view_pt;		/* model origin in view space */
  point_t diff;
  
  VSCALE(model_pt, mged_variables->adc_xyz, local2base);
  MAT4X3PNT(xyz_view_pt, model2view, model_pt);

  VSETALL(model_pt, 0.0);
  MAT4X3PNT(mo_view_pt, model2view, model_pt);

  VSUB2(diff, mo_view_pt, xyz_view_pt);
  f = Viewscale * base2local;
  mged_variables->adc_xyz_grid[X] = diff[X] * f;
  mged_variables->adc_xyz_grid[Y] = diff[Y] * f;
}

void
adc_xyz_grid_To_adc_xyz()
{
  fastf_t f;
  point_t model_pt;
  point_t xyz_view_pt;		/* ADC's xyz in view space */
  point_t mo_view_pt;		/* model origin in view space */
  point_t diff;

  VSETALL(model_pt, 0.0);
  MAT4X3PNT(mo_view_pt, model2view, model_pt);

  f = 1.0 / (Viewscale * base2local);
  diff[X] = mged_variables->adc_xyz_grid[X] * f;
  diff[Y] = mged_variables->adc_xyz_grid[Y] * f;
  diff[Z] = 0.0;

  VADD2(xyz_view_pt, mo_view_pt, diff);
  MAT4X3PNT(mged_variables->adc_xyz, view2model, xyz_view_pt);

  f = Viewscale * base2local;
  VSCALE(mged_variables->adc_xyz, mged_variables->adc_xyz, f);
}

void
adc_dst_To_dv_distadc()
{
  dv_distadc = (mged_variables->adc_dst /
		(Viewscale * base2local * M_SQRT2_DIV2) - 1.0) * GED_MAX;
}

void
dv_distadc_To_adc_dst()
{
  mged_variables->adc_dst = (dv_distadc * INV_GED + 1.0) * Viewscale * base2local * M_SQRT2_DIV2;
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

  if(mged_variables->adc_anchor_xyz == 1){
    adc_xyz_To_dv_xyadc();
    dv_xyadc_To_adc_xyz_grid();
  }else if(mged_variables->adc_anchor_xyz == 2){
    adc_xyz_grid_To_dv_xyadc();
    dv_xyadc_To_adc_xyz();
  }
    
  if(mged_variables->adc_anchor_a1){
    fastf_t angle;

    VSCALE(model_pt, mged_variables->adc_anchor_pt_a1, local2base);
    MAT4X3PNT(view_pt, model2view, model_pt);
    dx = view_pt[X] * GED_MAX - dv_xadc;
    dy = view_pt[Y] * GED_MAX - dv_yadc;

    if(dx != 0 || dy != 0)
      mged_variables->adc_a1 = RAD2DEG*atan2(dy, dx);
  }

  if(mged_variables->adc_anchor_a2){
    fastf_t angle;

    VSCALE(model_pt, mged_variables->adc_anchor_pt_a2, local2base);
    MAT4X3PNT(view_pt, model2view, model_pt);
    dx = view_pt[X] * GED_MAX - dv_xadc;
    dy = view_pt[Y] * GED_MAX - dv_yadc;

    if(dx != 0 || dy != 0)
      mged_variables->adc_a2 = RAD2DEG*atan2(dy, dx);
  }

  if(mged_variables->adc_anchor_tick){
    fastf_t dist;

    VSCALE(model_pt, mged_variables->adc_anchor_pt_tick, local2base);
    MAT4X3PNT(view_pt, model2view, model_pt);

    dx = view_pt[X] * GED_MAX - dv_xadc;
    dy = view_pt[Y] * GED_MAX - dv_yadc;
    dist = sqrt(dx * dx + dy * dy);
    mged_variables->adc_dst = dist * INV_GED * Viewscale * base2local;
    dv_distadc = (dist / M_SQRT2_DIV2) - 2047;
  }

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

  angle1 = mged_variables->adc_a1 * DEG2RAD;
  angle2 = mged_variables->adc_a2 * DEG2RAD;

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

  dv_xyadc_To_adc_xyz();
  dv_xyadc_To_adc_xyz_grid();
  dv_distadc_To_adc_dst();
  mged_variables->adc_a1 = mged_variables->adc_a2 = 45.0;

  mged_variables->adc_anchor_xyz = 0;
  mged_variables->adc_anchor_a1 = 0;
  mged_variables->adc_anchor_a2 = 0;
  mged_variables->adc_anchor_tick = 0;

  set_dirty_flag();
}

/*
 *			F _ A D C
 */

static char	adc_syntax[] = "\
 adc		toggle display of angle/distance cursor\n\
 adc a1 #	set angle1\n\
 adc a2 #	set angle2\n\
 adc dst #	set radius (distance) of tick\n\
 adc hv # #	reposition (view coordinates)\n\
 adc xyz # # #	reposition in front of a point (model coordinates)\n\
 adc reset	reset angles, location, and tick distance\n\
 adc help       prints this help message\n\
";
int
f_adc (clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  char *parameter;
  char **argp = argv;
  point_t view_pt;
  point_t model_pt;
  point_t user_pt;		/* Value(s) provided by user */
  point_t diff;
  int incr_flag;
  int i;

  if(dbip == DBI_NULL)
    return TCL_OK;

  if(6 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help adc");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if (argc == 1){
    if (mged_variables->adcflag)
      mged_variables->adcflag = 0;
    else
      mged_variables->adcflag = 1;

    if(adc_auto){
      struct dm_list *dlp;

      adc_reset();

      FOR_ALL_DISPLAYS(dlp, &head_dm_list.l)
	if(dlp->_mged_variables == mged_variables)
	  dlp->_adc_auto = 0;
    }

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

  if(strcmp(parameter, "a1") == 0){
    if(argc == 1){
      if(!mged_variables->adc_anchor_a1){
	if(incr_flag)
	  mged_variables->adc_a1 += user_pt[0];
	else
	  mged_variables->adc_a1 = user_pt[0];

	set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc a1' command accepts only 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "a2") == 0){
    if(argc == 1){
      if(!mged_variables->adc_anchor_a2){
	if(incr_flag)
	  mged_variables->adc_a2 += user_pt[0];
	else
	  mged_variables->adc_a2 = user_pt[0];

	set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc a2' command accepts only 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dst") == 0){
    if(argc == 1){
      if(!mged_variables->adc_anchor_tick){
	if(incr_flag)
	  mged_variables->adc_dst += user_pt[0];
	else
	  mged_variables->adc_dst = user_pt[0];

	adc_dst_To_dv_distadc();
	set_dirty_flag();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dst' command accepts only 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dh") == 0){
    if(argc == 1){
      mged_variables->adc_xyz_grid[X] += user_pt[0];

      set_adc_xyz_grid();
    }

    Tcl_AppendResult(interp, "The 'adc dh' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dv") == 0){
    if(argc == 1){
      mged_variables->adc_xyz_grid[Y] += user_pt[0];

      set_adc_xyz_grid();
    }

    Tcl_AppendResult(interp, "The 'adc dv' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "hv") == 0){
    if(argc == 2){
      if(!mged_variables->adc_anchor_xyz){
	if(incr_flag){
	  mged_variables->adc_xyz_grid[X] += user_pt[X];
	  mged_variables->adc_xyz_grid[Y] += user_pt[Y];
	}else{
	  mged_variables->adc_xyz_grid[X] = user_pt[X];
	  mged_variables->adc_xyz_grid[Y] = user_pt[Y];
	}

	set_adc_xyz_grid();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc hv' command requires 2 arguments\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dx") == 0){
    if(argc == 1){
      if(!mged_variables->adc_anchor_xyz){
	mged_variables->adc_xyz[X] += user_pt[0];

	set_adc_xyz();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dx' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dy") == 0){
    if(argc == 1){
      if(!mged_variables->adc_anchor_xyz){
	mged_variables->adc_xyz[Y] += user_pt[0];

	set_adc_xyz();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dy' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "dz") == 0){
    if(argc == 1){
      if(!mged_variables->adc_anchor_xyz){
	mged_variables->adc_xyz[Z] += user_pt[0];

	set_adc_xyz();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc dz' command requires 1 argument\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "xyz") == 0){
    if(argc == 3) {
      if(!mged_variables->adc_anchor_xyz){
	if(incr_flag){
	  VADD2(mged_variables->adc_xyz, mged_variables->adc_xyz, user_pt);
	}else{
	  VMOVE(mged_variables->adc_xyz, user_pt);
	}

	set_adc_xyz();
      }

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "The 'adc xyz' command requires 2 arguments\n", (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(parameter, "anchor") == 0){
    mged_variables->adc_anchor_xyz = 1;
    set_dirty_flag();

    return TCL_OK;
  }

  if(strcmp(parameter, "noanchor") == 0){
    mged_variables->adc_anchor_xyz = 0;
    set_dirty_flag();

    return TCL_OK;
  }

  if(strcmp(parameter, "anchor_grid") == 0){
    mged_variables->adc_anchor_xyz = 2;
    set_dirty_flag();

    return TCL_OK;
  }

  if(strcmp(parameter, "noanchor_grid") == 0){
    mged_variables->adc_anchor_xyz = 0;
    set_dirty_flag();

    return TCL_OK;
  }

  if(strcmp(parameter, "anchor_a1") == 0){
    mged_variables->adc_anchor_a1 = 1;
    set_dirty_flag();

    return TCL_OK;
  }

  if(strcmp(parameter, "noanchor_a1") == 0){
    mged_variables->adc_anchor_a1 = 0;
    set_dirty_flag();

    return TCL_OK;
  }

  if(strcmp(parameter, "anchorpoint_a1") == 0){
    VMOVE(mged_variables->adc_anchor_pt_a1, user_pt);
    set_dirty_flag();

    return TCL_OK;
  }

  if(strcmp(parameter, "anchor_a2") == 0){
    mged_variables->adc_anchor_a2 = 1;
    set_dirty_flag();

    return TCL_OK;
  }

  if(strcmp(parameter, "noanchor_a2") == 0){
    mged_variables->adc_anchor_a2 = 0;
    set_dirty_flag();

    return TCL_OK;
  }

  if(strcmp(parameter, "anchorpoint_a2") == 0){
    VMOVE(mged_variables->adc_anchor_pt_a2, user_pt);
    set_dirty_flag();

    return TCL_OK;
  }

  if(strcmp(parameter, "anchor_tick") == 0){
    mged_variables->adc_anchor_tick = 1;
    set_dirty_flag();

    return TCL_OK;
  }

  if(strcmp(parameter, "noanchor_tick") == 0){
    mged_variables->adc_anchor_tick = 0;
    set_dirty_flag();

    return TCL_OK;
  }

  if(strcmp(parameter, "anchorpoint_tick") == 0){
    VMOVE(mged_variables->adc_anchor_pt_tick, user_pt);
    set_dirty_flag();

    return TCL_OK;
  }

  if( strcmp(parameter, "reset") == 0)  {
    if (argc == 0) {
      adc_reset();

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
