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

#include	"ged_types.h"
#include	"ged.h"
#include	"menu.h"
#include	"dm.h"

int	menuflag;	/* flag indicating if a menu item is selected */
int	menu_on;
struct menu_item *menu_list;	/* base of array of menu items */

static int	menu_current;	/* y-location of selected menu item */
static int	menu_top;	/* screen loc of the first menu item */

/* MENU_INIT() ==== clear global data.*/

void
menu_init()
{
	menu_on = 0;
	menuflag = 0;
	menu_list = MENU_NULL;
}

/* MENU_DISPLAY(y) ==== Add a list of items to the display list. */

void
menu_display( y_top )
int y_top;
{ 
	register struct menu_item	*mptr;
	register int y = y_top;

	if( menu_list == MENU_NULL || menu_on == 0 )
		return;

	menu_top = y - MENU_DY / 2;

	for( mptr = &menu_list[0];
	     mptr->menu_string[0] != '\0';
	     mptr++, y += MENU_DY )  {
		dmp->dmr_puts( mptr->menu_string, MENUX, y, 0, DM_YELLOW );
		dmp->dmr_2d_line(XLIM, y+(MENU_DY/2), 2047, y+(MENU_DY/2), 0);
	}

	dmp->dmr_2d_line( XLIM, menu_top, XLIM, y-(MENU_DY/2), 0 );

	/* prefix item selected with "==>" to let user know it is selected */
	if( menuflag ) {
		dmp->dmr_puts("==>", MENUX-114, menu_current, 0, DM_WHITE);
	}
}

/* MENU_SELECT(x,y)
 *
 * Returns: 1 if menu claims these pen co-ordinates, 0 otherwise.
 */
int
menu_select( pen_y )
register int pen_y;
{ 
	register struct menu_item	*mptr;
	register int			yy;

	if( pen_y > menu_top )
		return(0);	/* pen above menu area */

	/*
	 * Start at the top of the list and see if the pen is
	 * above here.
	 */
	yy = menu_top;
	for( mptr = &menu_list[0];
	     mptr->menu_string[0] != '\0';
	     mptr++ )  {
		yy += MENU_DY;
		if( pen_y <= yy )
			continue;	/* pen is below this item */
		if( mptr != &menu_list[0] )  {
			menu_current = yy - (MENU_DY / 2);
			menuflag = 1;	/* tag a non-title item */
		}
		if( mptr->menu_func != ((void (*)())0) )
			(*(mptr->menu_func))(mptr->menu_arg);
		dmaflag = 1;
		return( 1 );		/* menu claims pen value */
	}
	return( 0 );		/* pen below menu area */
}
