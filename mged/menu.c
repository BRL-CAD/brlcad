/*	SCCSID	%W%	%E%	*/
/*
 *			E 1 1 . C
 *
 * Functions -
 *	menu_init		Clear global menu data
 *	menu_display		Add a list of items to the display list
 *	menu_select		Called by usepen() for menu pointing
 *
 * Authors -
 *	Bob Suckling
 *
 *		R E V I S I O N   H I S T O R Y
 *
 *	05/27/83  MJM	Adapted code to run on VAX;  numerous cleanups.
 *
 *	09-Sep-83 DAG	Overhauled.
 *
 *	11/02/83  MJM	Changed to use display manager.
 */

#include	"ged_types.h"
#include	"ged.h"
#include	"menu.h"
#include	"dm.h"

int	menuyy;		/* y-location of selected menu item */
int	menuflag;	/* flag indicating if a menu item is selected */
int	menu_on;	/* (User || Programmer) controlled by MENU_ON();*/
struct menu_item	*menu_list;	/* Pointer to the current menu list */

static int	i_menu_pen;
		/* Set in menu_pen() to index of menu item closest to pen. */
static int	menu_y;		/* VG tube space loc of the first menu item */
static int	x_old_menu;	/* Saves old pen loc.for debounce.*/
static int	y_old_menu;

/* MENU_INIT() ==== clear global data.*/

void
menu_init()
{
	x_old_menu = 999999;
	y_old_menu = 999999;
	i_menu_pen = -1;	/* pen not close to menu list.
					NOTE, setting this fixes the problem 
					of calling usepen before the call
					to do zoom.  So do not worry.*/
	MENU_ON(FALSE);		/* do not display empty menu.*/
	MENU_INSTALL((struct menu_item *)NULL);	/* no menu list.*/
/**	init_ars();	/*ARS INPUT ONLY*/
	/* run the ARS menus.*/
}

/* MENU_DISPLAY(y) ==== Add a list of items to the display list. */

void
menu_display( y )
int			y;	/*=== points @ the tube. */
{ 
	register int	i;
	register char	*str;

	if( menu_list == (struct menu_item *)NULL )
		return;

	dm_puts(menu_list->menu_string, XLIM, y, 0, DM_YELLOW );
	if( menu_on == FALSE )
		return;

	/* save the tube space location of the starting point of this menu. */
	menu_y = y + MENU_DY / 2;
	y += MENU_DY;		/* skip space for first item.*/
	for( i = 1; (str = (menu_list + i)->menu_string)[0] != '\0'; ++i )  {
		dm_puts( str, MENUX, y, 0, DM_YELLOW );
		if( i == i_menu_pen )  {
			/* pen is near, intensify */
			dm_puts(str,MENUX,y,0, DM_WHITE );
			dm_puts(str,MENUX,y,0, DM_WHITE );
		}
		y += MENU_DY; /* skip space for next item.*/
	}
}

/* MENU_SELECT(x,y) ==== called in pen input area, to activate the 
 *	===	pen menu select function. This sets the I_menu_pen
 *	===	integer when the pen is not pushed, else the function
 *	===	in the menu list is activated.
 *
 * Returns: 1 if menu claims these pen co-ordinates, 0 otherwise.
 */
int
menu_select( x, y, press )
int	x;
register int y;
int	press;
{ 
	struct menu_item	*mptr;
	int			yy;
	register int		i;

	if( x <= MENUX )
		return( 0 );		/* menu does not claim the pen */


	yy = menu_y;		/* set a finger @ the begining of the
					menu list on the tube.*/
	/*
	 * The pen is within the menu
	 */
	i_menu_pen = -1;	/* non selected value!!*/
	dmaflag = 1;		/* activate dozoom!!*/

	/* Find the pen close location. that is...
	 * Start at the top of the list and see if the pen is
	 * above here. The algorithim is ...
	 *	FOR all menu items DO probe next item
	 *	UNTIL selected item is found (i.e., selected
	 *	means pen is close to the item name)
	 *	THEN mark the item selected.
	 */
	for( i = 1; ((menu_list + i)->menu_string)[0] != '\0'; ++i )  {
		yy += MENU_DY;	/* skip an item space */
		if( y > yy )	/* UNTIL above here...*/
		{	
			i_menu_pen = i;	/* THEN select it.*/
			break;		/* EXIT FOR LOOP */
		}
	}

	if(	press
	    &&	( i_menu_pen != -1 )		/* pen on menu item */
	    &&	( x != x_old_menu)		/* debounce x and y */
	    &&	( y != y_old_menu)		)
	{
		/* Save for debounce when pen is pushed to select.*/
		x_old_menu = x;		
		y_old_menu = y;
		/* get pointer to item[i_menu_pen].*/
		mptr  = menu_list + i_menu_pen;
		menuyy = yy - (MENU_DY / 2);
		menuflag = 1;
		/* CALL the function corresponding to the users
		   menu item selected, and pass in the argument.*/
		(*(mptr->menu_func))(mptr->menu_arg);
	}
	return( 1 );	/* menu claims the pen value */
}
