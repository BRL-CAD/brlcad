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
 * shape of menu array cells
 */
struct	menu_item  {
	char	*menu_string;
	void	(*menu_func)();
	int	menu_arg;
};
	
/* defined in e11.c */
extern int	menu_on;	/* (User || Programmer) controlled by MENU_ON();*/
extern struct menu_item	*menu_list;	/* Pointer to the current menu list */

/*
 * Menu display on/off switch.
 *	TRUE prints the menu,
 *	FALSE removes the display and the ability to call menu functions.
 * Set dozoom to cause update this cycyle
 */
#define	MENU_ON(setting)	menu_on = (setting);\
				dmaflag = 1;
				
/*
 * Handing over the address of the menu list
 */
#define MENU_INSTALL(ptr)	menu_list = (ptr)

/* Helpful macros.*/
#define TRUE	1
#define FALSE	0
