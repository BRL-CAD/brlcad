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
static char RCSid[] = "";
#endif

#ifdef DO_RUBBER_BAND
#include "conf.h"

#include <math.h>
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"

#ifdef USE_FRAMEBUFFER
extern int fb_refresh();  /* from libfb/tcl.c */
#endif

extern void mged_center(); /* from chgview.c */
extern int mged_vscale();
extern struct bu_vls rubber_band_color;         /* from cmd.c */
extern struct bu_vls rubber_band_line_width;
extern struct bu_vls rubber_band_line_style;

void draw_rect();
void get_rect();
void set_rect();
void paint_rect_area();
void rt_rect_area();
void zoom_rect_area();

int f_get_rect();
int f_set_rect();

void
draw_rect()
{
  char *val;
  int r, g, b;
  int line_width;
  int line_style;
  struct rt_vlist h_vlist;
  struct rt_vlist vlist;
  fastf_t width, height;
  point_t view_pt;
  point_t model_pt;
  point_t first_pt;

  if(NEAR_ZERO(rect_width, (fastf_t)SMALL_FASTF) &&
     NEAR_ZERO(rect_height, (fastf_t)SMALL_FASTF))
    return;

  /* get rubber band color */
  val = Tcl_GetVar(interp, bu_vls_addr(&rubber_band_color), TCL_GLOBAL_ONLY);
  if(sscanf(val, "%d %d %d", &r, &g, &b) != 3){
    /* use default - white */
    r = 255;
    g = 255;
    b = 255;
  }

  /* get rubber band line width */
  val = Tcl_GetVar(interp, bu_vls_addr(&rubber_band_line_width), TCL_GLOBAL_ONLY);
  if(sscanf(val, "%d", &line_width) != 1)
    line_width = 1;

  if(line_width < 0)
    line_width = 1;

  /* get rubber band line style */
  val = Tcl_GetVar(interp, bu_vls_addr(&rubber_band_line_style), TCL_GLOBAL_ONLY);
  if(sscanf(val, "%d", &line_style) != 1){
    char c;

    if(sscanf(val, "%c", &c) != 1)
      line_style = 0; /* solid lines */
    else{
      if(c == 'd')
	line_style = 1;
      else
	line_style = 0; /* solid lines */
    }
  }

  if(line_style < 0 || line_style > 1)
    line_style = 0; /* solid lines */

  if(mged_variables->mouse_behavior == 'z'){
    if(rect_width >= 0.0)
      width = rect_width;
    else
      width = -rect_width;

    if(rect_height >= 0.0)
      height = rect_height;
    else
      height = -rect_height;

    if(width >= height){
      if(rect_height >= 0.0)
	rect_height = width;
      else
	rect_height = -width;
    }else{
      if(rect_width >= 0.0)
	rect_width = height;
      else
	rect_width = -height;
    }
  }

  BU_LIST_INIT(&h_vlist.l);
  BU_LIST_APPEND(&h_vlist.l, &vlist.l);

  VSET(view_pt, rect_x, rect_y, 1.0);
  MAT4X3PNT(first_pt, view2model, view_pt);
  VMOVE(vlist.pt[0], first_pt);
  vlist.cmd[0] = RT_VLIST_LINE_MOVE;

  VSET(view_pt, rect_x, rect_y + rect_height, 1.0);
  MAT4X3PNT(model_pt, view2model, view_pt);
  VMOVE(vlist.pt[1], model_pt);
  vlist.cmd[1] = RT_VLIST_LINE_DRAW;

  VSET(view_pt, rect_x + rect_width, rect_y + rect_height, 1.0);
  MAT4X3PNT(model_pt, view2model, view_pt);
  VMOVE(vlist.pt[2], model_pt);
  vlist.cmd[2] = RT_VLIST_LINE_DRAW;

  VSET(view_pt, rect_x + rect_width, rect_y, 1.0);
  MAT4X3PNT(model_pt, view2model, view_pt);
  VMOVE(vlist.pt[3], model_pt);
  vlist.cmd[3] = RT_VLIST_LINE_DRAW;

  VMOVE(vlist.pt[4], first_pt);
  vlist.cmd[4] = RT_VLIST_LINE_DRAW;

  vlist.nused = 5;

  /* draw axes */
  DM_SET_COLOR(dmp, r, g, b, 1);
  DM_SET_LINE_ATTR(dmp, line_width, line_style);
  DM_DRAW_VLIST(dmp, &h_vlist);
}

/*
 * Return the rectangle parameters in framebuffer/image format.
 */
void
get_rect(x, y, width, height)
int *x, *y;
int *width, *height;
{
  *x = dm_Normal2Xx(dmp, rect_x, 1);
  *y = dmp->dm_height - dm_Normal2Xy(dmp, rect_y);
  *width = rect_width * dmp->dm_width / 2.0;
  *height = rect_height * dmp->dm_height / 2.0;
}

/*
 * Given x, y, width and height in framebuffer/image format, calculate the
 * rectangle coordinates in normalized view coordinates.
 */
void
set_rect(x, y, width, height)
int x, y;
int width, height;
{
  rect_x = dm_Xx2Normal(dmp, x, 1);
  rect_y = dm_Xy2Normal(dmp, dmp->dm_height - y);
  rect_width = width * 2.0 / (fastf_t)dmp->dm_width;
  rect_height = height * 2.0 / (fastf_t)dmp->dm_height;
}

void
paint_rect_area()
{
  int x, y;
  int width, height;

  if(!fbp)
    return;

  get_rect(&x, &y, &width, &height);

#ifdef USE_FRAMEBUFFER
  (void)fb_refresh(fbp, x, y, width, height);
#endif
}

void
rt_rect_area()
{
  int xmin, xmax;
  int ymin, ymax;
  int width, height;
  struct bu_vls vls;

  if(!fbp)
    return;

  if(NEAR_ZERO(rect_width, (fastf_t)SMALL_FASTF) &&
     NEAR_ZERO(rect_height, (fastf_t)SMALL_FASTF))
    return;

  if(mged_variables->port < 0){
    bu_log("rt_rect_area: invalid port number - %d\n", mged_variables->port);
    return;
  }

  get_rect(&xmin, &ymin, &width, &height);

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
  bu_vls_printf(&vls, "rt -s %d -F %d -j %d,%d,%d,%d",
		dmp->dm_width, mged_variables->port,
		xmin, ymin, xmax, ymax);
  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
}

void
zoom_rect_area()
{
  fastf_t width, height;
  fastf_t sf;
  point_t old_model_center;
  point_t new_model_center;
  point_t old_view_center;
  point_t new_view_center;

  if(NEAR_ZERO(rect_width, (fastf_t)SMALL_FASTF) &&
     NEAR_ZERO(rect_height, (fastf_t)SMALL_FASTF))
    return;

  /* find old view center */
  MAT_DELTAS_GET_NEG(old_model_center, toViewcenter);
  MAT4X3PNT(old_view_center, model2view, old_model_center);

  /* calculate new view center */
  VSET(new_view_center,
       rect_x + rect_width / 2.0,
       rect_y + rect_height / 2.0,
       old_view_center[Z]);

  /* find new model center */
  MAT4X3PNT(new_model_center, view2model, new_view_center);
  mged_center(new_model_center);

  /* zoom in to fill rectangle */
  if(rect_width >= 0.0)
    width = rect_width;
  else
    width = -rect_width;

  if(rect_height >= 0.0)
    height = rect_height;
  else
    height = -rect_height;

  if(width >= height)
    sf = width / 2.0;
  else
    sf = height / 2.0;

  mged_vscale(sf);
}


/*
 *			T C L   C O M M A N D S
 */

int
f_get_rect(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  struct bu_vls vls;
  int x, y;
  int width, height;

  if(argc != 1){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  get_rect(&x, &y, &width, &height);

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "%d %d %d %d", x, y, width, height);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_OK;
}

int
f_set_rect(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  struct bu_vls vls;
  int x, y;
  int width, height;

  if(argc != 5){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%d", &x) != 1){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(sscanf(argv[2], "%d", &y) != 1){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(sscanf(argv[3], "%d", &width) != 1){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(sscanf(argv[4], "%d", &height) != 1){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  set_rect(x, y, width, height);

  if(mged_variables->rubber_band)
    dirty = 1;

  return TCL_OK;
}
#endif
