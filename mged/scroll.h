/*
 *			S C R O L L . H
 *
 *  Data structures for "scroll-bar" support
 *
 *  Author -
 *	Bill Mermagen Jr.
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 *
 *  $Header$
 */
struct	scroll_item  {
	char	*scroll_string;
	void	(*scroll_func)();
        int	scroll_val;
	char	*scroll_cmd;
};
#define SCROLL_NULL		((struct scroll_item *)0)
