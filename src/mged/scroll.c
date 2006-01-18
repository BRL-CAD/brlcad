/*                        S C R O L L . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United States Government as represented by
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
/** @file scroll.c
 *
 * Functions -
 *	scroll_display		Add a list of items to the display list
 *	scroll_select		Called by usepen() for pointing
 *
 * Authors -
 *	Bill Mermagen Jr.
 *	Michael John Muuss
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
#include <stdlib.h>

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./titles.h"
#include "./mged_dm.h"

#include "./sedit.h"

extern int mged_svbase(void);   /* defined in chgview.c */
extern void set_scroll(void);   /* defined in set.c */

/************************************************************************
 *									*
 *	First part:  scroll bar definitions				*
 *									*
 ************************************************************************/

static void sl_tol(register struct scroll_item *mptr, double val);
static void sl_atol(register struct scroll_item *mptr, double val);
static void sl_rrtol(register struct scroll_item *mptr, double val);
static void sl_artol(register struct scroll_item *mptr, double val);
static void sl_itol(register struct scroll_item *mptr, double val);
static void sl_adctol(register struct scroll_item *mptr, double val);

struct scroll_item sl_menu[] = {
	{ "xslew",	sl_tol,		0,	"X" },
	{ "yslew",	sl_tol,		1,	"Y" },
	{ "zslew",	sl_tol,		2,	"Z" },
	{ "scale",	sl_tol,		3,	"S" },
	{ "xrot",	sl_rrtol,	4,	"x" },
	{ "yrot",	sl_rrtol,	5,	"y" },
	{ "zrot",	sl_rrtol,	6,	"z" },
	{ "",		(void (*)())NULL, 0,	"" }
};

struct scroll_item sl_abs_menu[] = {
	{ "Xslew",	sl_atol,	0,	"aX" },
	{ "Yslew",	sl_atol,	1,	"aY" },
	{ "Zslew",	sl_atol,	2,	"aZ" },
	{ "Scale",	sl_tol,		3,	"aS" },
	{ "Xrot",	sl_artol,	4,	"ax" },
	{ "Yrot",	sl_artol,	5,	"ay" },
	{ "Zrot",	sl_artol,	6,	"az" },
	{ "",		(void (*)())NULL, 0,	"" }
};

struct scroll_item sl_adc_menu[] = {
	{ "xadc",	sl_itol,	0,	"xadc" },
	{ "yadc",	sl_itol,	1,	"yadc" },
	{ "ang 1",	sl_adctol,	2,	"ang1" },
	{ "ang 2",	sl_adctol,	3,	"ang2" },
	{ "tick",	sl_itol,	4,	"distadc" },
	{ "",		(void (*)())NULL, 0, "" }
};

/************************************************************************
 *									*
 *	Second part: Event Handlers called from menu items by buttons.c *
 *									*
 ************************************************************************/


/*
 *			S E T _ S C R O L L
 *
 * Set scroll_array.
 */
void
set_scroll(void)
{
  if (mged_variables->mv_sliders) {
    if(mged_variables->mv_rateknobs)
      scroll_array[0] = sl_menu;
    else
      scroll_array[0] = sl_abs_menu;

    if(adc_state->adc_draw)
      scroll_array[1] = sl_adc_menu;
    else
      scroll_array[1] = SCROLL_NULL;

  }else{
    scroll_array[0] = SCROLL_NULL;
    scroll_array[1] = SCROLL_NULL;
  }
}

/*
 *			S L _ H A L T _ S C R O L L
 *
 *  Reset all scroll bars to the zero position.
 */
void
sl_halt_scroll(void)
{
  struct bu_vls vls;

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "knob zero");
  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
}

/*
 *			S L _ T O G G L E _ S C R O L L
 */
void
sl_toggle_scroll(void)
{
  mged_variables->mv_sliders = mged_variables->mv_sliders ? 0 : 1;

  set_scroll_private();
}

/************************************************************************
 *									*
 *	Third part:  event handlers called from tables, above		*
 *									*
 *  Where the floating point value pointed to by scroll_val		*
 *  in the range -1.0 to +1.0 is the only desired result,		*
 *  everything can be handled by sl_tol().				*
 *									*
 ************************************************************************/

static void
sl_tol(register struct scroll_item *mptr, double val)
{
  struct bu_vls vls;

  if( val < -SL_TOL )   {
    val += SL_TOL;
  } else if( val > SL_TOL )   {
    val -= SL_TOL;
  } else {
    val = 0.0;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, val);
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
}

static void
sl_atol(register struct scroll_item *mptr, double val)
{
  struct bu_vls vls;

  if (dbip == DBI_NULL)
	  return;

  if( val < -SL_TOL )   {
    val += SL_TOL;
  } else if( val > SL_TOL )   {
    val -= SL_TOL;
  } else {
    val = 0.0;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, val*view_state->vs_vop->vo_scale*base2local);
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
}

static void
sl_rrtol(register struct scroll_item *mptr, double val)
{
  struct bu_vls vls;

  if( val < -SL_TOL )   {
    val += SL_TOL;
  } else if( val > SL_TOL )   {
    val -= SL_TOL;
  } else {
    val = 0.0;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, val * RATE_ROT_FACTOR);
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
}


static void
sl_artol(register struct scroll_item *mptr, double val)
{
  struct bu_vls vls;

  if( val < -SL_TOL )   {
    val += SL_TOL;
  } else if( val > SL_TOL )   {
    val -= SL_TOL;
  } else {
    val = 0.0;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, val*ABS_ROT_FACTOR);
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
}


static void
sl_adctol(register struct scroll_item *mptr, double val)
{
  struct bu_vls vls;

  if( val < -SL_TOL )   {
    val += SL_TOL;
  } else if( val > SL_TOL )   {
    val -= SL_TOL;
  } else {
    val = 0.0;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, 45.0 - val*45.0);
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
}


static void
sl_itol(register struct scroll_item *mptr, double val)
{
  struct bu_vls vls;

  if( val < -SL_TOL )   {
    val += SL_TOL;
  } else if( val > SL_TOL )   {
    val -= SL_TOL;
  } else {
    val = 0.0;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, val*GED_MAX);
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
}


/************************************************************************
 *									*
 *	Fourth part:  general-purpose interface mechanism		*
 *									*
 ************************************************************************/

/*
 *			S C R O L L _ D I S P L A Y
 *
 *  The parameter is the Y pixel address of the starting
 *  screen Y to be used, and the return value is the last screen Y
 *  position used.
 */
int
scroll_display( int y_top )
{
  register int		y;
  struct scroll_item	*mptr;
  struct scroll_item	**m;
  int		xpos;
  int second_menu = -1;
  fastf_t f = 0;

  scroll_top = y_top;
  y = y_top;

#if 1
  DM_SET_LINE_ATTR(dmp, mged_variables->mv_linewidth, 0);
#else
  DM_SET_LINE_ATTR(dmp, 1, 0);  /* linewidth - 1, not dashed */
#endif

  for( m = &scroll_array[0]; *m != SCROLL_NULL; m++ )  {
    ++second_menu;
    for( mptr = *m; mptr->scroll_string[0] != '\0'; mptr++ )  {
      y += SCROLL_DY;		/* y is now bottom line pos */

      switch(mptr->scroll_val){
      case 0:
	if (second_menu) {
	  f = (double)adc_state->adc_dv_x * INV_GED;

	  DM_SET_FGCOLOR(dmp,
			 color_scheme->cs_slider_text2[0],
			 color_scheme->cs_slider_text2[1],
			 color_scheme->cs_slider_text2[2], 1, 1.0);
	} else {
	  if(EDIT_TRAN && mged_variables->mv_transform == 'e'){
	    switch(mged_variables->mv_coords){
	    case 'm':
	    case 'o':
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_model_tran[X];
	      else
		f = edit_absolute_model_tran[X];
	      break;
	    case 'v':
	    default:
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_view_tran[X];
	      else
		f = edit_absolute_view_tran[X];
	      break;
	    }

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text1[0],
			   color_scheme->cs_slider_text1[1],
			   color_scheme->cs_slider_text1[2], 1, 1.0);
	  }else{
	    if(mged_variables->mv_rateknobs){
	      if(mged_variables->mv_coords == 'm')
		f = view_state->vs_rate_model_tran[X];
	      else
		f = view_state->vs_rate_tran[X];
	    }else{
	      if(mged_variables->mv_coords == 'm')
		f = view_state->vs_absolute_model_tran[X];
	      else
		f = view_state->vs_absolute_tran[X];
	    }

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text2[0],
			   color_scheme->cs_slider_text2[1],
			   color_scheme->cs_slider_text2[2], 1, 1.0);
	  }
	}
	break;
      case 1:
	if (second_menu) {
	  f = (double)adc_state->adc_dv_y * INV_GED;

	  DM_SET_FGCOLOR(dmp,
			 color_scheme->cs_slider_text2[0],
			 color_scheme->cs_slider_text2[1],
			 color_scheme->cs_slider_text2[2], 1, 1.0);
	} else {
	  if(EDIT_TRAN && mged_variables->mv_transform == 'e'){
	    switch(mged_variables->mv_coords){
	    case 'm':
	    case 'o':
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_model_tran[Y];
	      else
		f = edit_absolute_model_tran[Y];
	      break;
	    case 'v':
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_view_tran[Y];
	      else
		f = edit_absolute_view_tran[Y];
	      break;
	    }

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text1[0],
			   color_scheme->cs_slider_text1[1],
			   color_scheme->cs_slider_text1[2], 1, 1.0);
	  }else{
	    if(mged_variables->mv_rateknobs){
	      if(mged_variables->mv_coords == 'm')
		f = view_state->vs_rate_model_tran[Y];
	      else
		f = view_state->vs_rate_tran[Y];
	    }else{
	      if(mged_variables->mv_coords == 'm')
		f = view_state->vs_absolute_model_tran[Y];
	      else
		f = view_state->vs_absolute_tran[Y];
	    }

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text2[0],
			   color_scheme->cs_slider_text2[1],
			   color_scheme->cs_slider_text2[2], 1, 1.0);
	  }
	}
	break;
      case 2:
	if (second_menu) {
	  f = (double)adc_state->adc_dv_a1 * INV_GED;

	  DM_SET_FGCOLOR(dmp,
			 color_scheme->cs_slider_text2[0],
			 color_scheme->cs_slider_text2[1],
			 color_scheme->cs_slider_text2[2], 1, 1.0);
	} else {
	  if(EDIT_TRAN && mged_variables->mv_transform == 'e'){
	    switch(mged_variables->mv_coords){
	    case 'm':
	    case 'o':
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_model_tran[Z];
	      else
		f = edit_absolute_model_tran[Z];
	      break;
	    case 'v':
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_view_tran[Z];
	      else
		f = edit_absolute_view_tran[Z];
	      break;
	    }

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text1[0],
			   color_scheme->cs_slider_text1[1],
			   color_scheme->cs_slider_text1[2], 1, 1.0);
	  }else{
	    if(mged_variables->mv_rateknobs){
	      if(mged_variables->mv_coords == 'm')
		f = view_state->vs_rate_model_tran[Z];
	      else
		f = view_state->vs_rate_tran[Z];
	    }else{
	      if(mged_variables->mv_coords == 'm')
		f = view_state->vs_absolute_model_tran[Z];
	      else
		f = view_state->vs_absolute_tran[Z];
	    }

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text2[0],
			   color_scheme->cs_slider_text2[1],
			   color_scheme->cs_slider_text2[2], 1, 1.0);
	  }
	}
	break;
      case 3:
	if (second_menu) {
	  f = (double)adc_state->adc_dv_a2 * INV_GED;

	  DM_SET_FGCOLOR(dmp,
			 color_scheme->cs_slider_text2[0],
			 color_scheme->cs_slider_text2[1],
			 color_scheme->cs_slider_text2[2], 1, 1.0);
	} else {
	  if(EDIT_SCALE && mged_variables->mv_transform == 'e'){
	    if(mged_variables->mv_rateknobs)
	      f = edit_rate_scale;
	    else
	      f = edit_absolute_scale;

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text1[0],
			   color_scheme->cs_slider_text1[1],
			   color_scheme->cs_slider_text1[2], 1, 1.0);
	  }else{
	    if(mged_variables->mv_rateknobs)
	      f = view_state->vs_rate_scale;
	    else
	      f = view_state->vs_absolute_scale;

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text2[0],
			   color_scheme->cs_slider_text2[1],
			   color_scheme->cs_slider_text2[2], 1, 1.0);
	  }
	}
	break;
      case 4:
	if (second_menu) {
	  f = (double)adc_state->adc_dv_dist * INV_GED;

	  DM_SET_FGCOLOR(dmp,
			 color_scheme->cs_slider_text2[0],
			 color_scheme->cs_slider_text2[1],
			 color_scheme->cs_slider_text2[2], 1, 1.0);
	} else {
	  if(EDIT_ROTATE && mged_variables->mv_transform == 'e'){
	    switch(mged_variables->mv_coords){
	    case 'm':
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_model_rotate[X] / RATE_ROT_FACTOR;
	      else
		f = edit_absolute_model_rotate[X] / ABS_ROT_FACTOR;
	      break;
	    case 'o':
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_object_rotate[X] / RATE_ROT_FACTOR;
	      else
		f = edit_absolute_object_rotate[X] / ABS_ROT_FACTOR;
	      break;
	    case 'v':
	    default:
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_view_rotate[X] / RATE_ROT_FACTOR;
	      else
		f = edit_absolute_view_rotate[X] / ABS_ROT_FACTOR;
	      break;
	    }

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text1[0],
			   color_scheme->cs_slider_text1[1],
			   color_scheme->cs_slider_text1[2], 1, 1.0);
	  }else{
	    if(mged_variables->mv_rateknobs){
	      if(mged_variables->mv_coords == 'm')
		f = view_state->vs_rate_model_rotate[X] / RATE_ROT_FACTOR;
	      else
		f = view_state->vs_rate_rotate[X] / RATE_ROT_FACTOR;
	    }else{
	      if(mged_variables->mv_coords == 'm')
		f = view_state->vs_absolute_model_rotate[X] / ABS_ROT_FACTOR;
	      else
		f = view_state->vs_absolute_rotate[X] / ABS_ROT_FACTOR;
	    }

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text2[0],
			   color_scheme->cs_slider_text2[1],
			   color_scheme->cs_slider_text2[2], 1, 1.0);
	  }
	}
	break;
      case 5:
	if(second_menu)
	  Tcl_AppendResult(interp, "scroll_display: 2nd scroll menu is hosed\n",
			   (char *)NULL);
	else {
	  if(EDIT_ROTATE && mged_variables->mv_transform == 'e'){
	    switch(mged_variables->mv_coords){
	    case 'm':
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_model_rotate[Y] / RATE_ROT_FACTOR;
	      else
		f = edit_absolute_model_rotate[Y] / ABS_ROT_FACTOR;
	      break;
	    case 'o':
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_object_rotate[Y] / RATE_ROT_FACTOR;
	      else
		f = edit_absolute_object_rotate[Y] / ABS_ROT_FACTOR;
	      break;
	    case 'v':
	    default:
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_view_rotate[Y] / RATE_ROT_FACTOR;
	      else
		f = edit_absolute_view_rotate[Y] / ABS_ROT_FACTOR;
	      break;
	    }

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text1[0],
			   color_scheme->cs_slider_text1[1],
			   color_scheme->cs_slider_text1[2], 1, 1.0);
	  }else{
	    if(mged_variables->mv_rateknobs){
	      if(mged_variables->mv_coords == 'm')
		f = view_state->vs_rate_model_rotate[Y] / RATE_ROT_FACTOR;
	      else
		f = view_state->vs_rate_rotate[Y] / RATE_ROT_FACTOR;
	    }else{
	      if(mged_variables->mv_coords == 'm')
		f = view_state->vs_absolute_model_rotate[Y] / ABS_ROT_FACTOR;
	      else
		f = view_state->vs_absolute_rotate[Y] / ABS_ROT_FACTOR;
	    }

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text2[0],
			   color_scheme->cs_slider_text2[1],
			   color_scheme->cs_slider_text2[2], 1, 1.0);
	  }
	}
	break;
      case 6:
	if(second_menu)
	  Tcl_AppendResult(interp, "scroll_display: 2nd scroll menu is hosed\n",
			   (char *)NULL);
	else {
	  if(EDIT_ROTATE && mged_variables->mv_transform == 'e'){
	    switch(mged_variables->mv_coords){
	    case 'm':
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_model_rotate[Z] / RATE_ROT_FACTOR;
	      else
		f = edit_absolute_model_rotate[Z] / ABS_ROT_FACTOR;
	      break;
	    case 'o':
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_object_rotate[Z] / RATE_ROT_FACTOR;
	      else
		f = edit_absolute_object_rotate[Z] / ABS_ROT_FACTOR;
	      break;
	    case 'v':
	    default:
	      if(mged_variables->mv_rateknobs)
		f = edit_rate_view_rotate[Z] / RATE_ROT_FACTOR;
	      else
		f = edit_absolute_view_rotate[Z] / ABS_ROT_FACTOR;
	      break;
	    }

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text1[0],
			   color_scheme->cs_slider_text1[1],
			   color_scheme->cs_slider_text1[2], 1, 1.0);
	  }else{
	    if(mged_variables->mv_rateknobs){
	      if(mged_variables->mv_coords == 'm')
		f = view_state->vs_rate_model_rotate[Z] / RATE_ROT_FACTOR;
	      else
		f = view_state->vs_rate_rotate[Z] / RATE_ROT_FACTOR;
	    }else{
	      if(mged_variables->mv_coords == 'm')
		f = view_state->vs_absolute_model_rotate[Z] / ABS_ROT_FACTOR;
	      else
		f = view_state->vs_absolute_rotate[Z] / ABS_ROT_FACTOR;
	    }

	    DM_SET_FGCOLOR(dmp,
			   color_scheme->cs_slider_text2[0],
			   color_scheme->cs_slider_text2[1],
			   color_scheme->cs_slider_text2[2], 1, 1.0);
	  }
	}
	break;
      default:
	if(second_menu)
	  Tcl_AppendResult(interp, "scroll_display: 2nd scroll menu is hosed\n",
			   (char *)NULL);
	else
	  Tcl_AppendResult(interp, "scroll_display: first scroll menu is hosed\n",
			   (char *)NULL);
      }

      if(f > 0)
	xpos = (f + SL_TOL) * GED_MAX;
      else if(f < 0)
	xpos = (f - SL_TOL) * -MENUXLIM;
      else
	xpos = 0;

      DM_DRAW_STRING_2D( dmp, mptr->scroll_string,
			 GED2PM1(xpos), GED2PM1(y-SCROLL_DY/2), 0, 0 );
      DM_SET_FGCOLOR(dmp,
		     color_scheme->cs_slider_line[0],
		     color_scheme->cs_slider_line[1],
		     color_scheme->cs_slider_line[2], 1, 1.0);
      DM_DRAW_LINE_2D(dmp,
		      GED2PM1(XMAX), GED2PM1(y),
		      GED2PM1(MENUXLIM), GED2PM1(y));
    }
  }

  if( y != y_top )  {
    /* Sliders were drawn, so make left vert edge */
    DM_SET_FGCOLOR(dmp,
		   color_scheme->cs_slider_line[0],
		   color_scheme->cs_slider_line[1],
		   color_scheme->cs_slider_line[2], 1, 1.0);
    DM_DRAW_LINE_2D(dmp,
		    GED2PM1(MENUXLIM), GED2PM1(scroll_top-1),
		    GED2PM1(MENUXLIM), GED2PM1(y));
  }
  return( y );
}

/*
 *			S C R O L L _ S E L E C T
 *
 *  Called with Y coordinate of pen in menu area.
 *
 * Returns:	1 if menu claims these pen co-ordinates,
 *		0 if pen is BELOW scroll
 *		-1 if pen is ABOVE scroll	(error)
 */
int
scroll_select( int pen_x, int pen_y, int do_func )
{
	register int		yy;
	struct scroll_item	**m;
	register struct scroll_item     *mptr;

	if( !mged_variables->mv_sliders )  return(0);	/* not enabled */

	if( pen_y > scroll_top )
		return(-1);	/* pen above menu area */

	/*
	 * Start at the top of the list and see if the pen is
	 * above here.
	 */
	yy = scroll_top;
	for( m = &scroll_array[0]; *m != SCROLL_NULL; m++ )  {
		for( mptr = *m; mptr->scroll_string[0] != '\0'; mptr++ )  {
			fastf_t	val;
			yy += SCROLL_DY;	/* bottom line pos */
			if( pen_y < yy )
				continue;	/* pen below this item */

			/*
			 *  Record the location of scroll marker.
			 *  Note that the left side has less width than
			 *  the right side, due to the presence of the
			 *  menu text area on the left.
			 */
			if( pen_x >= 0 )  {
				val = pen_x * INV_GED;
			} else {
				val = pen_x/(double)(-MENUXLIM);
			}

			/* See if hooked function has been specified */
			if( mptr->scroll_func == ((void (*)())0) )  continue;

			if(do_func)
			  (*(mptr->scroll_func))(mptr, val);

			return( 1 );		/* scroll claims pen value */
		}
	}
	return( 0 );		/* pen below scroll area */
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
