/*                         H M E N U . H
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file hmenu.h
	SCCS id:	%Z% %M%	%I%
	Modified: 	%G% at %U%
	Retrieved: 	%H% at %T%
	SCCS archive:	%P%

	Authors:	Gary S. Moss
			Douglas A. Gwyn
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647

	This code is derived in part from menuhit(9.3) in AT&T 9th Edition UNIX,
		Version 1 Programmer's Manual.

*/
#ifndef INCL_HM
#define INCL_HM
struct HMenu;

/*	"dfn()" is called just before the submenu is invoked, and "bfn()"
	is called just afterwards.  These functions are called with the
	current menu item. "hfn()" is called only when a menu item is
	selected.  Its argument is a NULL pointer for now.  "data" is set
	to the return value from "hfn()".
 */
typedef struct HMitem
	{
	char		*text;	/* menu item string			*/
	char		*help;	/* help string				*/
	struct HMenu	*next;	/* sub-menu pointer or NULL		*/
	void		(*dfn)(), (*bfn)();
	int		(*hfn)();
	long		data;
	}
HMitem;

/*	"item" is an array of HMitems, terminated by an item with a
		zero "text" field.
	"generator()" takes an integer parameter and returns a pointer
		to a HMitem.  It only is significant if "item" == 0.
 */
typedef struct HMenu
	{
	HMitem	*item;
	HMitem	*(*generator)();
	short	prevtop;	/* Top entry currently visable		*/
	short	prevhit;	/* Offset from top of last select	*/
	short	sticky;		/* If set, menu stays around after SELECT,
					and until QUIT. */
	void	(*func)();	/* Execute after selection is made.	*/
	}
HMenu;

typedef struct HWin
	{
	struct HWin	*next;
	HMenu	*menup;
	int	menux;
	int	menuy;
	int	width;
	int	height;
	int	submenu;	/* At least one entry has a submenu.	*/
	int	*dirty;		/* Dynamically allocated bitmap. ON bits
					mean character needs a redraw.	*/
	}
HWindow;

extern HMitem	*hmenuhit(HMenu *menup, int menux, int menuy);	/* Application's calls menu.		*/
extern int	hm_getchar(void);	/* Can be supplied by application.	*/
extern int	hm_ungetchar(int c);	/* Can be supplied by application.	*/
extern void	hmredraw(void);	/* Application signals need for redraw.	*/

#define MAXARGS		100
#define MAXLINE 	132
#define MAXVISABLE	10
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
