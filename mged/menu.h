/*
 *			M E N U . H
 *
 *  Author -
 *	Bob Suckling
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 *
 *  $Header$
 */

/*
 * If a menu is installed, menu_on is true, and menu_list is a pointer
 * to an array of menu items.  The first ([0]) menu item is the title
 * for the menu, and the remaining items are individual menu entries.
 */
struct	menu_item  {
	char	*menu_string;
	void	(*menu_func)();
	int	menu_arg;
};
	
/* defined in menu.c */
extern int	menu_on;
extern struct menu_item	*menu_list;	/* Pointer to current menu array */

#define MENU_NULL		((struct menu_item *)0)
#define	MENU_ON(setting)	menu_on = (setting); dmaflag = 1;
#define MENU_INSTALL(ptr)	menu_list = (ptr)

/* Helpful macros.*/
#define TRUE	1
#define FALSE	0
