/*
 *			S C R O L L . C
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
#include <stdlib.h>

#include "tcl.h"

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./titles.h"
#include "./mged_dm.h"

#include "./mgedtcl.h"
#include "./sedit.h"

extern void set_scroll();   /* defined in set.c */

/************************************************************************
 *									*
 *	First part:  scroll bar definitions				*
 *									*
 ************************************************************************/

static void sl_tol();
static void sl_rrtol();
static void sl_artol();
static void sl_itol();
static void sl_adctol();

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
	{ "Xslew",	sl_tol,		0,	"aX" },
	{ "Yslew",	sl_tol,		1,	"aY" },
	{ "Zslew",	sl_tol,		2,	"aZ" },
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
 *			S L _ H A L T _ S C R O L L
 *
 *  Reset all scroll bars to the zero position.
 */
void
sl_halt_scroll()
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
sl_toggle_scroll()
{
  mged_variables.slidersflag = mged_variables.slidersflag ? 0 : 1;
  set_scroll();
}

/*
 *                      C M D _ S L I D E R S
 *
 * If no arguments are given, returns 1 if the sliders are displayed, and 0
 * if not.  If one argument is given, parses it as a boolean value and
 * turns on the sliders if it is 1, and turns them off it is 0.
 * Otherwise, returns an error;
 */

int
cmd_sliders(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  struct dm_list *p;

  if(argc < 1 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help sliders");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if (argc == 1) {
    Tcl_AppendResult(interp, mged_variables.slidersflag ? "1" : "0", (char *)NULL);
    return TCL_OK;
  }

  if (Tcl_GetBoolean(interp, argv[1], &mged_variables.slidersflag) == TCL_ERROR)
    return TCL_ERROR;

  if (mged_variables.slidersflag) {
    if(mged_variables.rateknobs)
      scroll_array[0] = sl_menu;
    else
      scroll_array[0] = sl_abs_menu;
  } else {
    scroll_array[0] = SCROLL_NULL;	
    scroll_array[1] = SCROLL_NULL;	
  }

  if(mged_variables.faceplate && mged_variables.orig_gui)
    dirty = 1;

  return TCL_OK;
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
sl_tol( mptr, val )
register struct scroll_item     *mptr;
double				val;
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
sl_rrtol( mptr, val )
register struct scroll_item *mptr;
double val;
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
sl_artol( mptr, val )
register struct scroll_item *mptr;
double val;
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
sl_adctol( mptr, val )
register struct scroll_item     *mptr;
double				val;
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
sl_itol( mptr, val )
register struct scroll_item     *mptr;
double				val;
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
  bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, val*2047.0);
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
scroll_display( y_top )
int y_top;
{ 
  register int		y;
  struct scroll_item	*mptr;
  struct scroll_item	**m;
  int		xpos;
  int second_menu = -1;
  fastf_t f;

  /* XXX this should be driven by the button event */
  if( mged_variables.adcflag && mged_variables.slidersflag )
    scroll_array[1] = sl_adc_menu;
  else
    scroll_array[1] = SCROLL_NULL;

  scroll_top = y_top;
  y = y_top;

  dmp->dm_setLineAttr(dmp, 1, 0);  /* linewidth - 1, not dashed */

  for( m = &scroll_array[0]; *m != SCROLL_NULL; m++ )  {
    ++second_menu;
    for( mptr = *m; mptr->scroll_string[0] != '\0'; mptr++ )  {
      y += SCROLL_DY;		/* y is now bottom line pos */

      switch(mptr->scroll_val){
      case 0:
	if(second_menu)
	  f = (double)dv_xadc / 2047.0;
	else {
	  if(EDIT_TRAN && mged_variables.transform == 'e'){
	    if(mged_variables.rateknobs)
	      f = edit_rate_tran[X];
	    else
	      f = edit_absolute_tran[X];

	    dmp->dm_setColor(dmp, DM_WHITE, 1);
	  }else{
	    if(mged_variables.rateknobs){
	      if(mged_variables.coords == 'm')
		f = rate_model_tran[X];
	      else
		f = rate_tran[X];
	    }else{
	      if(mged_variables.coords == 'm')
		f = absolute_model_tran[X];
	      else
		f = absolute_tran[X];
	    }

	    dmp->dm_setColor(dmp, DM_RED, 1);
	  }
	}
	break;
      case 1:
	if(second_menu)
	  f = (double)dv_yadc / 2047.0;
	else {
	  if(EDIT_TRAN && mged_variables.transform == 'e'){
	    if(mged_variables.rateknobs)
	      f = edit_rate_tran[Y];
	    else
	      f = edit_absolute_tran[Y];

	    dmp->dm_setColor(dmp, DM_WHITE, 1);
	  }else{
	    if(mged_variables.rateknobs){
	      if(mged_variables.coords == 'm')
		f = rate_model_tran[Y];
	      else
		f = rate_tran[Y];
	    }else{
	      if(mged_variables.coords == 'm')
		f = absolute_model_tran[Y];
	      else
		f = absolute_tran[Y];
	    }

	    dmp->dm_setColor(dmp, DM_RED, 1);
	  }
	}
	break;
      case 2:
	if(second_menu)
	  f = (double)dv_1adc / 2047.0;
	else {
	  if(EDIT_TRAN && mged_variables.transform == 'e'){
	    if(mged_variables.rateknobs)
	      f = edit_rate_tran[Z];
	    else
	      f = edit_absolute_tran[Z];

	    dmp->dm_setColor(dmp, DM_WHITE, 1);
	  }else{
	    if(mged_variables.rateknobs){
	      if(mged_variables.coords == 'm')
		f = rate_model_tran[Z];
	      else
		f = rate_tran[Z];
	    }else{
	      if(mged_variables.coords == 'm')
		f = absolute_model_tran[Z];
	      else
		f = absolute_tran[Z];
	    }

	    dmp->dm_setColor(dmp, DM_RED, 1);
	  }
	}
	break;
      case 3:
	if(second_menu)
	  f = (double)dv_2adc / 2047.0;
	else {
	  if(EDIT_SCALE && mged_variables.transform == 'e'){
	    if(mged_variables.rateknobs)
	      f = edit_rate_scale;
	    else
	      f = edit_absolute_scale;

	    dmp->dm_setColor(dmp, DM_WHITE, 1);
	  }else{
	    if(mged_variables.rateknobs)
	      f = rate_zoom;
	    else
	      f = absolute_zoom;

	    dmp->dm_setColor(dmp, DM_RED, 1);
	  }
	}
	break;
      case 4:
	if(second_menu)
	  f = (double)dv_distadc / 2047.0;
	else {
	  if(EDIT_ROTATE && mged_variables.transform == 'e'){
	    if(mged_variables.rateknobs)
	      f = edit_rate_rotate[X] / RATE_ROT_FACTOR;
	    else
	      f = edit_absolute_rotate[X] / ABS_ROT_FACTOR;

	    dmp->dm_setColor(dmp, DM_WHITE, 1);
	  }else{
	    if(mged_variables.rateknobs){
	      if(mged_variables.coords == 'm')
		f = rate_model_rotate[X] / RATE_ROT_FACTOR;
	      else
		f = rate_rotate[X] / RATE_ROT_FACTOR;
	    }else{
	      f = absolute_rotate[X] / ABS_ROT_FACTOR;
	    }

	    dmp->dm_setColor(dmp, DM_RED, 1);
	  }
	}
	break;
      case 5:
	if(second_menu)
	  Tcl_AppendResult(interp, "scroll_display: 2nd scroll menu is hosed\n",
			   (char *)NULL);
	else {
	  if(EDIT_ROTATE && mged_variables.transform == 'e'){
	    if(mged_variables.rateknobs)
	      f = edit_rate_rotate[Y] / RATE_ROT_FACTOR;
	    else
	      f = edit_absolute_rotate[Y] / ABS_ROT_FACTOR;

	    dmp->dm_setColor(dmp, DM_WHITE, 1);
	  }else{
	    if(mged_variables.rateknobs){
	      if(mged_variables.coords == 'm')
		f = rate_model_rotate[Y] / RATE_ROT_FACTOR;
	      else
		f = rate_rotate[Y] / RATE_ROT_FACTOR;
	    }else{
	      f = absolute_rotate[Y] / ABS_ROT_FACTOR;
	    }

	    dmp->dm_setColor(dmp, DM_RED, 1);
	  }
	}
	break;
      case 6:
	if(second_menu)
	  Tcl_AppendResult(interp, "scroll_display: 2nd scroll menu is hosed\n",
			   (char *)NULL);
	else {
	  if(EDIT_ROTATE && mged_variables.transform == 'e'){
	    if(mged_variables.rateknobs)
	      f = edit_rate_rotate[Z] / RATE_ROT_FACTOR;
	    else
	      f = edit_absolute_rotate[Z] / ABS_ROT_FACTOR;

	    dmp->dm_setColor(dmp, DM_WHITE, 1);
	  }else{
	    if(mged_variables.rateknobs){
	      if(mged_variables.coords == 'm')
		f = rate_model_rotate[Z] / RATE_ROT_FACTOR;
	      else
		f = rate_rotate[Z] / RATE_ROT_FACTOR;
	    }else{
	      f = absolute_rotate[Z] / ABS_ROT_FACTOR;
	    }

	    dmp->dm_setColor(dmp, DM_RED, 1);
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
	xpos = (f + SL_TOL) * 2047.0;
      else if(f < 0)
	xpos = (f - SL_TOL) * -MENUXLIM;
      else
	xpos = 0;

      dmp->dm_drawString2D( dmp, mptr->scroll_string,
			    xpos, y-SCROLL_DY/2, 0, 0 );
      dmp->dm_setColor(dmp, DM_YELLOW, 1);
      dmp->dm_drawLine2D(dmp, XMAX, y, MENUXLIM, y);
    }
  }

  if( y != y_top )  {
    /* Sliders were drawn, so make left vert edge */
    dmp->dm_setColor(dmp, DM_YELLOW, 1);
    dmp->dm_drawLine2D( dmp, MENUXLIM, scroll_top-1,
			MENUXLIM, y, 0, DM_YELLOW );
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
scroll_select( pen_x, pen_y, do_func )
int		pen_x;
register int	pen_y;
int do_func;
{ 
	register int		yy;
	struct scroll_item	**m;
	register struct scroll_item     *mptr;

	if( !mged_variables.slidersflag )  return(0);	/* not enabled */

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
				val = pen_x/2047.0;
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
