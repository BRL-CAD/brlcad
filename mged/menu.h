/*	SCCSID	%W%	%E%	*/
/*
	menu.h -- definitions for GED menu
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
