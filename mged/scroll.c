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
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./titles.h"
#include "./scroll.h"
#include "./dm.h"

static int	scroll_top;	/* screen loc of the first menu item */

static int	scroll_enabled = 0;

struct scroll_item *scroll_array[6];	/* Active scroll bar definitions */

/************************************************************************
 *									*
 *	First part:  scroll bar definitions				*
 *									*
 ************************************************************************/

static void sl_tol();

struct scroll_item scr_menu[] = {
	{ "xslew",	sl_tol,		&rate_slew[X] },
	{ "yslew",	sl_tol,		&rate_slew[Y] },
	{ "zslew",	sl_tol,		&rate_slew[Z] },
	{ "zoom",	sl_tol,		&rate_zoom },
	{ "xrot",	sl_tol,		&rate_rotate[X] },
	{ "yrot",	sl_tol,		&rate_rotate[Y] },
	{ "zrot",	sl_tol,		&rate_rotate[Z] },
	{ "",		(void (*)())NULL, 0 }
};

static void	sl_xadc(), sl_yadc(), sl_1adc(), sl_2adc(), sl_distadc();
static double	sld_xadc, sld_yadc, sld_1adc, sld_2adc, sld_distadc;

struct scroll_item sl_adc_menu[] = {
	{ "xadc",	sl_xadc,	&sld_xadc },
	{ "yadc",	sl_yadc,	&sld_yadc },
	{ "ang 1",	sl_1adc,	&sld_1adc },
	{ "ang 2",	sl_2adc,	&sld_2adc },
	{ "tick",	sl_distadc,	&sld_distadc },
	{ "",		(void (*)())NULL, 0 }
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
void sl_halt_scroll()
{
	/* The 'knob' command will zero the rate_slew[] array, etc. */
	rt_vls_printf(&dm_values.dv_string, "knob zero\n");
}

/*
 *			S L _ T O G G L E _ S C R O L L
 */
void sl_toggle_scroll()
{

	scroll_enabled = (scroll_enabled == 0) ? 1 : 0;
	if( scroll_enabled )  {
		scroll_array[0] = scr_menu;
	} else {
		scroll_array[0] = SCROLL_NULL;	
		scroll_array[1] = SCROLL_NULL;	
	}
	dmaflag++;
}

/************************************************************************
 *									*
 *	Third part:  event handlers called from tables, above		*
 *									*
 *  Where the floating point value pointed to by scroll_val		*
 *  in the range -1.0 to +1.0 is the only desired result,		*
 *  everything can bel handled by sl_tol().				*
 *  When conversion to integer is required, an individual handler	*
 *  is required for each slider.					*
 *									*
 ************************************************************************/
#define SL_TOL	0.015		/* size of dead spot, 0.015 = 32/2047 */

static void sl_tol( mptr )
register struct scroll_item     *mptr;
{

	if( *mptr->scroll_val < -SL_TOL )   {
		*(mptr->scroll_val) += SL_TOL;
	} else if( *mptr->scroll_val > SL_TOL )   {
		*(mptr->scroll_val) -= SL_TOL;
	} else {
		*(mptr->scroll_val) = 0.0;
	}
	check_nonzero_rates();
}

static void sl_xadc( mptr )
register struct scroll_item     *mptr;
{
	rt_vls_printf(&dm_values.dv_string, "knob xadc %d\n",
		(int)(*(mptr->scroll_val) * 2047) );
}

static void sl_yadc( mptr )
register struct scroll_item     *mptr;
{
	rt_vls_printf(&dm_values.dv_string, "knob yadc %d\n",
		(int)(*(mptr->scroll_val) * 2047) );
}

static void sl_1adc( mptr )
register struct scroll_item     *mptr;
{
	rt_vls_printf(&dm_values.dv_string, "knob ang1 %d\n",
		(int)(*(mptr->scroll_val) * 2047) );
}

static void sl_2adc( mptr )
register struct scroll_item     *mptr;
{
	rt_vls_printf(&dm_values.dv_string, "knob ang2 %d\n",
		(int)(*(mptr->scroll_val) * 2047) );
}

static void sl_distadc( mptr )
register struct scroll_item     *mptr;
{
	rt_vls_printf(&dm_values.dv_string, "knob distadc %d\n",
		(int)(*(mptr->scroll_val) * 2047) );
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

	/* XXX this should be driven by the button event */
	if( adcflag && scroll_enabled )
		scroll_array[1] = sl_adc_menu;
	else
		scroll_array[1] = SCROLL_NULL;

	scroll_top = y_top;
	y = y_top;
	for( m = &scroll_array[0]; *m != SCROLL_NULL; m++ )  {
		for( mptr = *m; mptr->scroll_string[0] != '\0'; mptr++ )  {
		     	y += SCROLL_DY;		/* y is now bottom line pos */
			if( *(mptr->scroll_val) >= 0 )  {
			     	xpos = *(mptr->scroll_val) * 2047;
			} else {
				/* The menu gets in the way */
			     	xpos = *(mptr->scroll_val) * -MENUXLIM;
			}
			dmp->dmr_puts( mptr->scroll_string,
				xpos, y-SCROLL_DY/2, 0, DM_RED );
			dmp->dmr_2d_line(XMAX, y, MENUXLIM, y, 0);
		}
	}
	if( y != y_top )  {
		/* Sliders were drawn, so make left vert edge */
		dmp->dmr_2d_line( MENUXLIM, scroll_top-1,
			MENUXLIM, y, 0 );
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
scroll_select( pen_x, pen_y )
int		pen_x;
register int	pen_y;
{ 
	register int		yy;
	struct scroll_item	**m;
	register struct scroll_item     *mptr;

	if( !scroll_enabled )  return(0);	/* not enabled */

	if( pen_y > scroll_top )
		return(-1);	/* pen above menu area */

	/*
	 * Start at the top of the list and see if the pen is
	 * above here.
	 */
	yy = scroll_top;
	for( m = &scroll_array[0]; *m != SCROLL_NULL; m++ )  {
		for( mptr = *m; mptr->scroll_string[0] != '\0'; mptr++ )  {
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
				*(mptr->scroll_val) = pen_x/2047.0;
			} else {
				*(mptr->scroll_val) = pen_x/(double)(-MENUXLIM);
			}

			/* See if hooked function has been specified */
			if( mptr->scroll_func == ((void (*)())0) )  continue;

			(*(mptr->scroll_func))(mptr);
			dmaflag = 1;
			return( 1 );		/* scroll claims pen value */
		}
	}
	return( 0 );		/* pen below scroll area */
}
