/*
 *			M E N U . C
 *
 * Functions -
 *	mmenu_init		Clear global menu data
 *	mmenu_display		Add a list of items to the display list
 *	mmenu_select		Called by usepen() for menu pointing
 *	mmenu_pntr		Reset the pointer to a menu item
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

#include "conf.h"

#include "tcl.h"

#include <stdio.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./titles.h"
#include "./menu.h"
#include "./dm.h"

#include "./mgedtcl.h"

int	menuflag;	/* flag indicating if a menu item is selected */
struct menu_item *menu_array[NMENU];	/* base of array of menu items */

static int	menu_top;	/* screen loc of the first menu item */
int	cur_menu;	/* index of selected menu in list */
int	cur_item;	/* index of selected item in menu */

void set_menucurrent();
int set_arrowloc();


int
cmd_mmenu_get(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
    int index;
    
    if (argc > 2) {
	Tcl_AppendResult(interp, "wrong # args: must be \"mmenu_get ?index?\"",
			 (char *)NULL);
	return TCL_ERROR;
    }

    if (argc == 2) {
	register struct menu_item **m, *mptr;

	if (Tcl_GetInt(interp, argv[1], &index) != TCL_OK)
	    return TCL_ERROR;

	if (index < 0 || index > NMENU) {
	    Tcl_AppendResult(interp, "index out of range", (char *)NULL);
	    return TCL_ERROR;
	}

	m = menu_array+index;
	if (*m == MENU_NULL)
	    return TCL_OK;

	for (mptr = *m; mptr->menu_string[0] != '\0'; mptr++)
	    Tcl_AppendElement(interp, mptr->menu_string);
    } else {
	register struct menu_item **m;
	struct bu_vls result;
	int status;

	bu_vls_init(&result);
	bu_vls_strcat(&result, "list");
	for (m = menu_array; m - menu_array < NMENU; m++)
	    bu_vls_printf(&result, " [%s %d]", argv[0], m-menu_array);

	status = Tcl_Eval(interp, bu_vls_addr(&result));
	bu_vls_free(&result);

	return status;
    }

    return TCL_OK;
}


/*
 *			M M E N U _ I N I T
 *
 * Clear global data
 */
void
mmenu_init()
{
	menuflag = 0;
	menu_array[MENU_L1] = MENU_NULL;
	menu_array[MENU_L2] = MENU_NULL;
	menu_array[MENU_GEN] = MENU_NULL;
	(void)Tcl_CreateCommand(interp, "mmenu_set", cmd_nop, (ClientData)NULL,
				(Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "mmenu_get", cmd_mmenu_get,
				(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
}


/*
 *			M M E N U _ S E T
 */

void
mmenu_set( index, value )
int index;
struct menu_item *value;
{
    Tcl_DString ds_menu;
    struct bu_vls menu_string;

    menu_array[index] = value;  /* Change the menu internally */

    bu_vls_init(&menu_string);
    Tcl_DStringInit(&ds_menu);

    bu_vls_printf(&menu_string, "mmenu_set %d ", index);

    Tcl_DStringStartSublist(&ds_menu);
    if (value != MENU_NULL)
	for (; value->menu_string[0] != '\0'; value++)
	    (void)Tcl_DStringAppendElement(&ds_menu, value->menu_string);
    Tcl_DStringEndSublist(&ds_menu);

    bu_vls_strcat(&menu_string, Tcl_DStringValue(&ds_menu));
    (void)Tcl_Eval(interp, bu_vls_addr(&menu_string));

    Tcl_DStringFree(&ds_menu);
    bu_vls_free(&menu_string);

    update_views = 1;
}

/*
 *			M M E N U _ D I S P L A Y
 *
 *  Draw one or more menus onto the display.
 *  If "menuflag" is non-zero, then the last selected
 *  menu item will be indicated with an arrow.
 */
void
mmenu_display( y_top )
int y_top;
{ 
	static int menu, item;
	register struct menu_item	**m;
	register struct menu_item	*mptr;
	register int y = y_top;

	menu_top = y - MENU_DY / 2;
	dmp->dmr_2d_line(MENUXLIM, menu_top, XMIN, menu_top, 0);

	for( menu=0, m = menu_array; m < &menu_array[NMENU]; m++,menu++ )  {
		if( *m == MENU_NULL )  continue;
		for( item=0, mptr = *m;
		     mptr->menu_string[0] != '\0' && y > TITLE_YBASE;
		     mptr++, y += MENU_DY, item++ )  {
			dmp->dmr_puts( mptr->menu_string, MENUX, y-15, 0,
				mptr == *m ? DM_RED : DM_YELLOW );
			dmp->dmr_2d_line(MENUXLIM, y+(MENU_DY/2), XMIN, y+(MENU_DY/2), 0);
			if( cur_item == item && cur_menu == menu && menuflag )  {
				/* prefix item selected with "==>" */
				dmp->dmr_puts("==>", XMIN, y-15, 0, DM_WHITE);
			}
		}
	}
	if( y == y_top )  return;	/* no active menus */

	dmp->dmr_2d_line( MENUXLIM, menu_top-1, MENUXLIM, y-(MENU_DY/2), 0 );
}

/*
 *			M M E N U _ S E L E C T
 *
 *  Called with Y coordinate of pen in menu area.
 *
 * Returns:	1 if menu claims these pen co-ordinates,
 *		0 if pen is BELOW menu
 *		-1 if pen is ABOVE menu	(error)
 */
int
mmenu_select( pen_y )
register int pen_y;
{ 
	static int menu, item;
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

	for( menu=0, m=menu_array; m < &menu_array[NMENU]; m++,menu++ )  {
		if( *m == MENU_NULL )  continue;
		for( item=0, mptr = *m;
		     mptr->menu_string[0] != '\0';
		     mptr++, item++ )  {
			yy += MENU_DY;
			if( pen_y <= yy )
				continue;	/* pen is below this item */
			cur_item = item;
			cur_menu = menu;
			menuflag = 1;
		     	/* It's up to the menu_func to set menuflag=0
		     	 * if no arrow is desired */
			if( mptr->menu_func != ((void (*)())0) )
				(*(mptr->menu_func))(mptr->menu_arg, menu, item);

			return( 1 );		/* menu claims pen value */
		}
	}
	return( 0 );		/* pen below menu area */
}

/*
 *			M M E N U _ P N T R
 *
 *  Routine to allow user to reset the arrow to any menu & item desired.
 *  Example:  menu_pntr( MENU_L1, 3 ).
 *  The arrow can always be eliminated by setting menuflag=0, dmaflag=1.
 */
void
mmenu_pntr( menu, item )
{
	cur_menu = menu;
	cur_item = item;
	if( cur_menu >= 0 )
		menuflag = 1;
}
