/* 
 * tkWinCursor.c --
 *
 *	This file contains Win32 specific cursor related routines.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkWinInt.h"

/*
 * The following data structure contains the system specific data
 * necessary to control Windows cursors.
 */

typedef struct {
    TkCursor info;		/* Generic cursor info used by tkCursor.c */
    HCURSOR winCursor;		/* Win32 cursor handle. */
    int system;			/* 1 if cursor is a system cursor, else 0. */
} TkWinCursor;

/*
 * The table below is used to map from the name of a predefined cursor
 * to its resource identifier.
 */

static struct CursorName {
    char *name;
    LPCTSTR id;
} cursorNames[] = {
    {"starting",		IDC_APPSTARTING},
    {"arrow",			IDC_ARROW},
    {"ibeam",			IDC_IBEAM},
    {"icon",			IDC_ICON},
    {"no",			IDC_NO},
    {"size",			IDC_SIZE},
    {"size_ne_sw",		IDC_SIZENESW},
    {"size_ns",			IDC_SIZENS},
    {"size_nw_se",		IDC_SIZENWSE},
    {"size_we",			IDC_SIZEWE},
    {"uparrow",			IDC_UPARROW},
    {"wait",			IDC_WAIT},
    {"crosshair",		IDC_CROSS},
    {"fleur",			IDC_SIZE},
    {"sb_v_double_arrow",	IDC_SIZENS},
    {"sb_h_double_arrow",	IDC_SIZEWE},
    {"center_ptr",		IDC_UPARROW},
    {"watch",			IDC_WAIT},
    {"xterm",			IDC_IBEAM},
    {NULL,			0}
};

/*
 * The default cursor is used whenever no other cursor has been specified.
 */

#define TK_DEFAULT_CURSOR	IDC_ARROW


/*
 *----------------------------------------------------------------------
 *
 * TkGetCursorByName --
 *
 *	Retrieve a system cursor by name.  
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.  
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkGetCursorByName(interp, tkwin, string)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tk_Window tkwin;		/* Window in which cursor will be used. */
    Tk_Uid string;		/* Description of cursor.  See manual entry
				 * for details on legal syntax. */
{
    struct CursorName *namePtr;
    TkWinCursor *cursorPtr;
    int argc;
    CONST char **argv = NULL;

    /*
     * All cursor names are valid lists of one element (for
     * Unix-compatability), even unadorned system cursor names.
     */

    if (Tcl_SplitList(interp, string, &argc, &argv) != TCL_OK) {
	return NULL;
    }
    if (argc != 1) {
	goto badCursorSpec;
    }

    cursorPtr = (TkWinCursor *) ckalloc(sizeof(TkWinCursor));
    cursorPtr->info.cursor = (Tk_Cursor) cursorPtr;
    cursorPtr->winCursor = NULL;
    cursorPtr->system = 0;

    if (argv[0][0] == '@') {
	/*
	 * Check for system cursor of type @<filename>, where only
	 * the name is allowed.  This accepts any of:
	 *	-cursor @/winnt/cursors/globe.ani
	 *	-cursor @C:/Winnt/cursors/E_arrow.cur
	 *	-cursor {@C:/Program\ Files/Cursors/bart.ani}
	 *      -cursor {{@C:/Program Files/Cursors/bart.ani}}
	 *	-cursor [list @[file join "C:/Program Files" Cursors bart.ani]]
	 */

	if (Tcl_IsSafe(interp)) {
	    Tcl_AppendResult(interp, "can't get cursor from a file in",
		    " a safe interpreter", (char *) NULL);
	    ckfree((char *) argv);
	    ckfree((char *) cursorPtr);
	    return NULL;
	}
	cursorPtr->winCursor = LoadCursorFromFile(&(argv[0][1]));
    } else {
	/*
	 * Check for the cursor in the system cursor set.
	 */
	for (namePtr = cursorNames; namePtr->name != NULL; namePtr++) {
	    if (strcmp(namePtr->name, argv[0]) == 0) {
		cursorPtr->winCursor = LoadCursor(NULL, namePtr->id);
		break;
	    }
	}

	if (cursorPtr->winCursor == NULL) {
	    /*
	     * Hmm, it is not in the system cursor set.  Check to see
	     * if it is one of our application resources.
	     */
	    cursorPtr->winCursor = LoadCursor(Tk_GetHINSTANCE(), argv[0]);
	} else {
	    cursorPtr->system = 1;
	}
    }

    if (cursorPtr->winCursor == NULL) {
	ckfree((char *) cursorPtr);
    badCursorSpec:
	ckfree((char *) argv);
	Tcl_AppendResult(interp, "bad cursor spec \"", string, "\"",
		(char *) NULL);
	return NULL;
    } else {
	ckfree((char *) argv);
	return (TkCursor *) cursorPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkCreateCursorFromData --
 *
 *	Creates a cursor from the source and mask bits.
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkCreateCursorFromData(tkwin, source, mask, width, height, xHot, yHot,
	fgColor, bgColor)
    Tk_Window tkwin;		/* Window in which cursor will be used. */
    CONST char *source;		/* Bitmap data for cursor shape. */
    CONST char *mask;		/* Bitmap data for cursor mask. */
    int width, height;		/* Dimensions of cursor. */
    int xHot, yHot;		/* Location of hot-spot in cursor. */
    XColor fgColor;		/* Foreground color for cursor. */
    XColor bgColor;		/* Background color for cursor. */
{
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpFreeCursor --
 *
 *	This procedure is called to release a cursor allocated by
 *	TkGetCursorByName.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor data structure is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TkpFreeCursor(cursorPtr)
    TkCursor *cursorPtr;
{
    TkWinCursor *winCursorPtr = (TkWinCursor *) cursorPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpSetCursor --
 *
 *	Set the global cursor.  If the cursor is None, then use the
 *	default Tk cursor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the mouse cursor.
 *
 *----------------------------------------------------------------------
 */

void
TkpSetCursor(cursor)
    TkpCursor cursor;
{
    HCURSOR hcursor;
    TkWinCursor *winCursor = (TkWinCursor *) cursor;

    if (winCursor == NULL || winCursor->winCursor == NULL) {
	hcursor = LoadCursor(NULL, TK_DEFAULT_CURSOR);
    } else {
	hcursor = winCursor->winCursor;
    }

    if (hcursor != NULL) {
	SetCursor(hcursor);
    }
}
