/*                            H M . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
 *
 */
/** @file Hm.h
	Author:	Gary S. Moss
		U. S. Army Ballistic Research Laboratory
		Aberdeen Proving Ground
		Maryland 21005-5066

	$Header$ (BRL)
*/
/**
	<Hm.h> -- MUVES "Hm" (hierarchical menu) package definitions
**/
#ifndef Hm_H_INCLUDE
#define Hm_H_INCLUDE
/**

	This software and documentation is derived in part from the
	menuhit(9.3) manual pages (not source code) in AT&T 9th Edition
	UNIX, Version 1 Programmer's Manual.

	The Hm package provides a pop-up menu implementation which uses
	a terminal-independent screen management facility (Sc package)
	to simulate the necessary graphics using the ASCII character set.
	Only a few keyboard characters are required to control the menus,
	but when possible, as with DMD terminals running a MYX terminal
	emulator, the menus can be controlled with the mouse.

**/
/**
	Each menu is defined by the following structure:

	typedef struct
		{
		HmItem	*item;
		HmItem	*(*generator)();
		short	prevtop;
		short	prevhit;
		boolean	sticky;
		}
	HmMenu;

	A menu can be built as an array of HmItem's, pointed to by item,
	or with a generator function.  If item is (HmItem *) 0, the
	generator field is assumed to be valid.  The generator function
	is passed an integer parameter, and must return the pointer to
	an HmItem in a static area (the result is only needed until the
	next call).  The n's are guaranteed to start at 0 and increase
	by 1 each invocation until generator returns (HmItem *) 0.
	Prevtop will contain the index of the menu item which appeared
	at the top of the menu the last time it was displayed.  Prevhit
	will contain the index of the last item traversed (indices begin
	at 0).  If sticky is true, the menu will hang around after an
	entry is selected so that another entry may be chosen and the
	menu must be exited explicitly with the appropriate key-stroke
	or equivalent mouse operation.  Otherwise, the menu will exit
	after selection of an item (and any actions resulting from that
	selection such as submenus have finished).  WARNING: if a menu
	is to be used more than once during recursive calls to HmHit,
	there needs to be distinct copies (allocated storage) since the
	item field is filled in by the generator function, and the
	prevtop and prevhit fields should be private to the current
	invocation of HmHit.

**/

struct HmMenu;

typedef struct
	{
	char	*text;		/* menu item string			*/
	char	*help;		/* help string				*/
	struct HmMenu *next;	/* sub-menu pointer or NULL		*/
	void	(*dfn)();
	void	(*bfn)();
	void	(*hfn)();
	long	data;
	}
HmItem;

/**
	Menu items are defined by the following structure:

	typedef struct
		{
		char	*text;
		char	*help;
		HmMenu	*next;
		void	(*dfn)();
		void	(*bfn)();
		void	(*hfn)();
		long	data;
		}
	HmItem;

	The text field will be displayed as the name of the item.
	Characters with the 0200 bit set are regarded as fill characters.
	For example, the string "\240X" will appear in the menu as a
	right-justified X (040 is the ASCII space character).  Menu
	strings without fill characters are drawn centered in the menu.
	Whether generated statically or dynamically, the list of HmItem's
	must be terminated by a NULL text field.  The help string will be
	displayed when the user presses the help key.  If next is not
	equal to (HmMenu *) 0, it is assumed to point to a submenu,
	"dfn()" if non-zero is called just  before the submenu is invoked,
	and "bfn()" likewise just afterwards.  These functions are passed
	the current menu item.  If the menu item is selected and next is
	0, "hfn()" is called (if non-zero), also with the current menu
	item.  The "data" field is reserved for the user to pass
	information between these functions and the calling routine.

**/
typedef struct HmMenu
	{
	HmItem	*item;		/* List of menu items or 0.		*/
	HmItem	*(*generator)();/* If item == 0, generates items.	*/
	short	prevtop;	/* Top entry currently visable		*/
	short	prevhit;	/* Offset from top of last select	*/
	boolean	sticky;		/* If true, menu stays around after SELECT,
					and until QUIT. */
	}
HmMenu;

/* Structure used internally for screen management.  These are stacked
	dynamically with calls to HmHit() such that the head of the chain
	is the current menu, and the end of the chain such that next is
	0, is the top-level menu.
 */
typedef struct HmWin
	{
	struct HmWin *next;	/* Parent of this menu.			*/
	HmMenu	*menup;		/* Address of menu data structure.	*/
	int	menux;		/* Position on screen where top-left	*/
	int	menuy;		/*    corner of menu will be displayed.	*/
	int	width;		/* Width of menu, not including border.	*/
	int	height;		/* Number of menu entries.		*/
	int	*dirty;		/* Dynamically allocated bitmap. ON bits
					mean character needs a redraw.	*/
	}
HmWindow;

/**

	boolean	HmInit( int x, int y, int maxvis )

	HmInit() must be called before any other routines in the
	Hm package to initialize the screen position of the top-left
	corner of the top-level menu to x and y and to set the
	maximum number of menu entries which will be visable at one
	time to maxvis.  If the number of entries in a menu exceeds
	maxvis, the menu will scroll to accommodate them.  The values
	of x, y and maxvis are stored in these external variables:

	int	HmLftMenu
	int	HmTopMenu
	int	HmMaxVis

	If this routine is not called, default parameters will be used.

	HmInit() also opens "/dev/tty" for input and stores it's file
	descriptor in HmTtyFd and associated stream handle in HmTtyFp.

	int	HmTtyFd
	FILE	*HmTtyFp

	This routine returns true or false on success or failure to open
	"/dev/tty".
**/
#if __STDC__
extern boolean	HmInit( int x, int y, int maxvis );
#else
extern boolean	HmInit();
#endif
extern FILE	*HmTtyFp;
extern int	HmLftMenu;
extern int	HmTopMenu;
extern int	HmMaxVis;
extern int	HmLastMaxVis;
extern int	HmTtyFd;
/**

	HmItem	*HmHit( HmMenu	*menup )

	HmHit() presents the user with a menu specified by HmMenu
	pointer menup and returns a pointer to the selected HmItem
	or 0 if nothing was selected.  The menu is presented to
	the user with the item corresponding to prevhit (current
	item) highlighted.  If a menu has not been accessed yet,
	prevhit will be set to the first item.  The user may move
	the cursor to the item below the current one by pressing the
	'd' key, or move up by pressing the 'u'.  If the user
	presses 'h', the help message for the current item will be
	displayed until another key is struck.  To select an item,
	the user presses the space bar.  An actual selection has
	been made when the selected item does not have a submenu,
	otherwise, it may be termed a traversal of that item.
	Traversing an item does change the value of prevhit, but
	is not final as the submenu can be exited by the user
	pressing 'q', before selecting an item.  In case the screen
	becomes disturbed (i.e. by the output of another process),
	holding down the <control> key and striking an 'l' will
	re-display all of the menus.

	Both the help and warning (non-fatal error) messages will
	be displayed in the one-line window at HmYCOMMO.  For
	instance, if the user hits any keys other than the ones
	mentioned above, an error message will appear in this
	window.  Also, a one-line window is reserved for application
	prompts at HmYPROMPT.  The prompt window is cleared just
	prior to blocking on user input, and the message window
	is cleared when the user provides input (strikes a key).

	If a menu has more items than can be displayed at once,
	the corners of the menu will indicate this as follows:

	'+' means the adjacent displayed item is the actual first
	    (or last if a bottom corner) item in the menu.
	'v' means that the entry displayed in the previous line
	    is not the last, and the menu can be scrolled upwards.
	'^' means that the entry displayed on the next line is
	    not actually the first, and the menu can be scrolled
	    downwards.

	Attempting to move the cursor below the bottom entry of
	the menu will cause the menu to scroll half a page or
	until the entries run out, which ever comes first.  If
	there are no entries to scroll the terminal will beep.
	The analagous holds true for attempting to move upward
	past the top entry of the menu.  If a DMD terminal with
	MYX running is used, a special cursor will appear, and
	the user may use the mouse rather than the keyboard as
	follows:  Clicking button 1 of the mouse while the
	cursor is outside the "current menu", will cause the
	terminal to beep.  If the cursor is on an item, that item
	will be made "current".  If on the "current" item, that
	item will be selected.  If on the top border of the
	current menu, that menu will scroll down if possible, and
	if not, that menu will exit.  If on the bottom border,
	that menu will scroll upward if possible and beep if not.

**/
#if __STDC__
extern HmItem	*HmHit( HmMenu *menup );
#else
extern HmItem	*HmHit();
#endif

/**

	void	HmRedraw( void )

	HmRedraw() will force the entire set of active menus to
	be redrawn on the next call to HmHit().  This is useful
	when an application interferes with the portion of the
	screen used by the Hm package (from HmTopMenu to HmYCOMMO).

**/
#if __STDC__
extern void	HmRedraw( void );
#else
extern void	HmRedraw();
#endif

/**

	void	HmError( char *string )

	HmError() will display string on line HmYCOMMO.

**/
#if __STDC__
extern void	HmError( char *str );
#else
extern void	HmError();
#endif

#define HmYCOMMO	(HmTopMenu+HmMaxVis+HmHGTBORDER)
#define HmYPROMPT	(HmYCOMMO+1)
#define HmXPROMPT	1
#define HmYBORDER	(HmYPROMPT+1)
/**

	int	HmGetchar( void )
	int	HmUngetchar( int c )

	HmGetchar() and HmUngetchar() are used by the Hm package to
	read a character from the keyboard and to stuff one character
	back on the input stream.  They may be both be supplied by
	the application if the default behavior is not desirable.
	HmGetchar() returns the next character on the standard input.
	This command will block until input is available.
	HmUngetchar() inserts the character c on the standard input.
	An EOF will be returned if this is not possible or c is
	equal to EOF.  In general, this is guaranteed to work for one
	character assuming that something has already been read with
	HmGetchar() and the input stream is buffered.

**/
#if __STDC__
extern int	HmGetchar( void );
extern int	HmUngetchar( int c );
#else
extern int	HmGetchar();
extern int	HmUngetchar();
#endif
/**

	void	HmTtySet( void )
	void	HmTtyReset( void )

	HmTtySet() and HmTtyReset() set/restore the terminal modes for
	the menus to work properly.  These are mainly internal routines
	which are called from HmHit(), but are provided in case an
	escape from the program is provided by the application or job
	control is enabled in the underlying shell, in which case,
	these routines can be called from a menu function or signal
	handler.

**/
extern void	HmTtySet();
extern void	HmTtyReset();


#define HmMAXLINE 	132
#define HmHGTBORDER	2

#endif		/* Hm_H_INCLUDE */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
