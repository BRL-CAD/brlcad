/*
 *			M E N U . C
 *
 * Functions -
 *	menu_init		Clear global menu data
 *	menu_display		Add a list of items to the display list
 *	menu_select		Called by usepen() for menu pointing
 *
 * Authors -
 *	Bob Suckling
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

#include "./machine.h"	/* special copy */
#include "vmath.h"
#include "./ged.h"
#include "./menu.h"
#include "./dm.h"

int	menuflag;	/* flag indicating if a menu item is selected */
struct menu_item *menu_array[NMENU];	/* base of array of menu items */

static int	menu_current;	/* y-location of selected menu item */
static int	menu_top;	/* screen loc of the first menu item */

/* MENU_INIT() ==== clear global data.*/

void
menu_init()
{
	menuflag = 0;
	menu_array[MENU_L1] = MENU_NULL;
	menu_array[MENU_L2] = MENU_NULL;
	menu_array[MENU_GEN] = MENU_NULL;
}

/* MENU_DISPLAY(y) ==== Add a list of items to the display list. */

void
menu_display( y_top )
int y_top;
{ 
	register struct menu_item	**m;
	register struct menu_item	*mptr;
	register int y = y_top;

	menu_top = y - MENU_DY / 2;
	dmp->dmr_2d_line(MENUXLIM, menu_top, XMIN, menu_top, 0);

	for( m = menu_array; m < &menu_array[NMENU]; m++ )  {
		if( *m == MENU_NULL )  continue;
		for( mptr = *m;
		     mptr->menu_string[0] != '\0' && y > TITLE_YBASE;
		     mptr++, y += MENU_DY )  {
			dmp->dmr_puts( mptr->menu_string, MENUX, y, 0,
				mptr == *m ? DM_RED : DM_YELLOW );
			dmp->dmr_2d_line(MENUXLIM, y+(MENU_DY/2), XMIN, y+(MENU_DY/2), 0);
		}
	}
	if( y == y_top )  return;	/* no active menus */

	dmp->dmr_2d_line( MENUXLIM, menu_top-1, MENUXLIM, y-(MENU_DY/2), 0 );

	/* prefix item selected with "==>" to let user know it is selected */
	if( menuflag ) {
		dmp->dmr_puts("==>", XMIN, menu_current, 0, DM_WHITE);
	}
}

/* MENU_SELECT(x,y)
 *
 * Returns:	1 if menu claims these pen co-ordinates,
 *		0 if pen is BELOW menu
 *		-1 if pen is ABOVE menu	(error)
 */
int
menu_select( pen_y )
register int pen_y;
{ 
	struct menu_item	**m;
	register struct menu_item	*mptr;
	register int			yy;

	if( pen_y > menu_top )
		return(-1);	/* pen above menu area */

	/*
	 * Start at the top of the list and see if the pen is
	 * above here.
	 */
	yy = menu_top;

	for( m = menu_array; m < &menu_array[NMENU]; m++ )  {
		if( *m == MENU_NULL )  continue;
		for( mptr = *m;
		     mptr->menu_string[0] != '\0';
		     mptr++ )  {
			yy += MENU_DY;
			if( pen_y <= yy )
				continue;	/* pen is below this item */
			if( mptr != *m )  {
				menu_current = yy - (MENU_DY / 2);
				menuflag = 1;	/* tag a non-title item */
			}
			if( mptr->menu_func != ((void (*)())0) )
				(*(mptr->menu_func))(mptr->menu_arg);
			dmaflag = 1;
			return( 1 );		/* menu claims pen value */
		}
	}
	return( 0 );		/* pen below menu area */
}
