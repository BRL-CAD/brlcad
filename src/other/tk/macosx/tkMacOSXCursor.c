/*
 * tkMacOSXCursor.c --
 *
 *	This file contains Macintosh specific cursor related routines.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2006-2007 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXPrivate.h"

/*
 * There are three different ways to set the cursor on the Mac. The default
 * theme cursors (listed in cursorNames below), color resource cursors, &
 * normal cursors.
 */

#define NONE		-1	/* Hidden cursor */
#define THEME		0	/* Theme cursors */
#define ANIMATED	1	/* Animated theme cursors */
#define COLOR		2	/* Cursors of type crsr. */
#define NORMAL		3	/* Cursors of type CURS. */

/*
 * The following data structure contains the system specific data necessary to
 * control Windows cursors.
 */

typedef struct {
    TkCursor info;		/* Generic cursor info used by tkCursor.c */
    Handle macCursor;		/* Resource containing Macintosh cursor. For
				 * theme cursors, this is -1. */
    int type;			/* Type of Mac cursor: for theme cursors this
				 * is the theme cursor constant, otherwise one
				 * of crsr or CURS. */
    int count;			/* For animating cursors, the count for the
				 * cursor. */
} TkMacOSXCursor;

/*
 * The table below is used to map from the name of a predefined cursor
 * to its resource identifier.
 */

struct CursorName {
    const char *name;
    int id;
};

static struct CursorName noneCursorName = {"none", 0};

static struct CursorName themeCursorNames[] = {
    {"arrow",			kThemeArrowCursor},
    {"copyarrow",		kThemeCopyArrowCursor},
    {"aliasarrow",		kThemeAliasArrowCursor},
    {"contextualmenuarrow",	kThemeContextualMenuArrowCursor},
    {"ibeam",			kThemeIBeamCursor},
    {"text",			kThemeIBeamCursor},
    {"xterm",			kThemeIBeamCursor},
    {"cross",			kThemeCrossCursor},
    {"crosshair",		kThemeCrossCursor},
    {"cross-hair",		kThemeCrossCursor},
    {"plus",			kThemePlusCursor},
    {"closedhand",		kThemeClosedHandCursor},
    {"openhand",		kThemeOpenHandCursor},
    {"pointinghand",		kThemePointingHandCursor},
    {"resizeleft",		kThemeResizeLeftCursor},
    {"resizeright",		kThemeResizeRightCursor},
    {"resizeleftright",		kThemeResizeLeftRightCursor},
    {"resizeup",		kThemeResizeUpCursor},
    {"resizedown",		kThemeResizeDownCursor},
    {"resizeupdown",		kThemeResizeUpDownCursor},
    {"notallowed",		kThemeNotAllowedCursor},
    {"poof",			kThemePoofCursor},
    {NULL,			0}
};

static struct CursorName animatedThemeCursorNames[] = {
    {"watch",			kThemeWatchCursor},
    {"countinguphand",		kThemeCountingUpHandCursor},
    {"countingdownhand",	kThemeCountingDownHandCursor},
    {"countingupanddownhand",	kThemeCountingUpAndDownHandCursor},
    {"spinning",		kThemeSpinningCursor},
    {NULL,			0}
};

/*
 * Declarations of static variables used in this file.
 */

static TkMacOSXCursor * gCurrentCursor = NULL;
				/* A pointer to the current cursor. */
static int gResizeOverride = false;
				/* A boolean indicating whether we should use
				 * the resize cursor during installations. */
static int gTkOwnsCursor = true;/* A boolean indicating whether Tk owns the
				 * cursor. If not (for instance, in the case
				 * where a Tk window is embedded in another
				 * app's window, and the cursor is out of the
				 * tk window, we will not attempt to adjust
				 * the cursor. */

/*
 * Declarations of procedures local to this file
 */

static void FindCursorByName(TkMacOSXCursor *macCursorPtr, const char *string);

/*
 *----------------------------------------------------------------------
 *
 * FindCursorByName --
 *
 *	Retrieve a system cursor by name, and fill the macCursorPtr
 *	structure. If the cursor cannot be found, the macCursor field will be
 *	NULL. The function first attempts to load a color cursor. If that
 *	fails it will attempt to load a black & white cursor.
 *
 * Results:
 *	Fills the macCursorPtr record.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
FindCursorByName(
    TkMacOSXCursor *macCursorPtr,
    const char *string)
{
    Handle resource;
    Str255 curName;
    int destWrote, inCurLen;
    Tcl_Encoding encoding;

    inCurLen = strlen(string);
    if (inCurLen > 255) {
	return;
    }

    /*
     * macRoman is the encoding that the resource fork uses.
     */

    encoding = Tcl_GetEncoding(NULL, "macRoman");
    Tcl_UtfToExternal(NULL, encoding, string, inCurLen, 0, NULL,
	    (char *) &curName[1], 255, NULL, &destWrote, NULL);
    curName[0] = destWrote;
    Tcl_FreeEncoding(encoding);

    resource = GetNamedResource('crsr', curName);
    if (resource) {
	short id;
	Str255 theName;
	ResType theType;

	GetResInfo(resource, &id, &theType, theName);
	macCursorPtr->macCursor = (Handle) GetCCursor(id);
	macCursorPtr->type = COLOR;
    } else {
	macCursorPtr->macCursor = GetNamedResource('CURS', curName);
	macCursorPtr->type = NORMAL;
    }
}

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
TkGetCursorByName(
    Tcl_Interp *interp,		/* Interpreter to use for error reporting. */
    Tk_Window tkwin,		/* Window in which cursor will be used. */
    Tk_Uid string)		/* Description of cursor. See manual entry
				 * for details on legal syntax. */
{
    struct CursorName *namePtr;
    TkMacOSXCursor *macCursorPtr;
    int count = -1;

    macCursorPtr = (TkMacOSXCursor *) ckalloc(sizeof(TkMacOSXCursor));
    macCursorPtr->info.cursor = (Tk_Cursor) macCursorPtr;

    /*
     * To find a cursor we must first determine if it is one of the builtin
     * cursors or the standard arrow cursor. Otherwise, we attempt to load the
     * cursor as a named Mac resource.
     */

    if (strcmp(noneCursorName.name, string) == 0) {
	namePtr = &noneCursorName;
	macCursorPtr->type = NONE;
    } else {
	for (namePtr = themeCursorNames; namePtr->name != NULL; namePtr++) {
	    if (strcmp(namePtr->name, string) == 0) {
		macCursorPtr->type = THEME;
		break;
	    }
	}
    }

    if (namePtr->name == NULL) {
	for (namePtr = animatedThemeCursorNames;
		namePtr->name != NULL; namePtr++) {
	    int namelen = strlen(namePtr->name);

	    if (strncmp(namePtr->name, string, namelen) == 0) {
		const char *numPtr = string + namelen;

		if (*numPtr) {
		    int result = Tcl_GetInt(NULL, numPtr, &count);

		    if (result != TCL_OK) {
			continue;
		    }
		}
		macCursorPtr->type = ANIMATED;
		break;
	    }
	}
    }

    if (namePtr->name != NULL) {
	macCursorPtr->macCursor = (Handle) namePtr;
	macCursorPtr->count = count;
    } else {
	FindCursorByName(macCursorPtr, string);

	if (macCursorPtr->macCursor == NULL) {
	    const char **argv;
	    int argc;

	    /*
	     * The user may be trying to specify an XCursor with fore & back
	     * colors. We don't want this to be an error, so pick off the
	     * first word, and try again.
	     */

	    if (Tcl_SplitList(interp, string, &argc, &argv) == TCL_OK ) {
		if (argc > 1) {
		    FindCursorByName(macCursorPtr, argv[0]);
		}
		ckfree((char *) argv);
	    }
	}
    }

    if (macCursorPtr->macCursor == NULL) {
	ckfree((char *)macCursorPtr);
	Tcl_AppendResult(interp, "bad cursor spec \"", string, "\"", NULL);
	return NULL;
    }
    return (TkCursor *) macCursorPtr;
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
TkCreateCursorFromData(
    Tk_Window tkwin,		/* Window in which cursor will be used. */
    CONST char *source,		/* Bitmap data for cursor shape. */
    CONST char *mask,		/* Bitmap data for cursor mask. */
    int width, int height,	/* Dimensions of cursor. */
    int xHot, int yHot,		/* Location of hot-spot in cursor. */
    XColor fgColor,		/* Foreground color for cursor. */
    XColor bgColor)		/* Background color for cursor. */
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
TkpFreeCursor(
    TkCursor *cursorPtr)
{
    TkMacOSXCursor *macCursorPtr = (TkMacOSXCursor *) cursorPtr;

    switch (macCursorPtr->type) {
    case COLOR:
	DisposeCCursor((CCrsrHandle) macCursorPtr->macCursor);
	break;
    case NORMAL:
	ReleaseResource(macCursorPtr->macCursor);
	break;
    }

    if (macCursorPtr == gCurrentCursor) {
	gCurrentCursor = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInstallCursor --
 *
 *	Installs either the current cursor as defined by TkpSetCursor or a
 *	resize cursor as the cursor the Macintosh should currently display.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the Macintosh mouse cursor.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXInstallCursor(
    int resizeOverride)
{
    TkMacOSXCursor *macCursorPtr = gCurrentCursor;
    CCrsrHandle ccursor;
    CursHandle cursor;
    static unsigned int cursorStep = 0;
    static int cursorHidden = 0;
    int cursorNone = 0;

    gResizeOverride = resizeOverride;

    if (resizeOverride) {
	cursor = (CursHandle) GetNamedResource('CURS', "\presize");
	if (cursor) {
	    SetCursor(*cursor);
	} else {
	    TkMacOSXDbgMsg("Resize cursor failed: %d", ResError());
	}
    } else if (macCursorPtr == NULL) {
	SetThemeCursor(kThemeArrowCursor);
    } else {
	struct CursorName *namePtr;

	switch (macCursorPtr->type) {
	case NONE:
	    if (!cursorHidden) {
		cursorHidden = 1;
		HideCursor();
	    }
	    cursorNone = 1;
	    break;
	case THEME:
	    namePtr = (struct CursorName *) macCursorPtr->macCursor;
	    SetThemeCursor(namePtr->id);
	    break;
	case ANIMATED:
	    namePtr = (struct CursorName *) macCursorPtr->macCursor;
	    if (macCursorPtr->count == -1) {
		SetAnimatedThemeCursor(namePtr->id, cursorStep++);
	    } else {
		SetAnimatedThemeCursor(namePtr->id, macCursorPtr->count);
	    }
	    break;
	case COLOR:
	    ccursor = (CCrsrHandle) macCursorPtr->macCursor;
	    SetCCursor(ccursor);
	    break;
	case NORMAL:
	    cursor = (CursHandle) macCursorPtr->macCursor;
	    SetCursor(*cursor);
	    break;
	}
    }
    if (cursorHidden && !cursorNone) {
	cursorHidden = 0;
	ShowCursor();
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpSetCursor --
 *
 *	Set the current cursor and install it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the current cursor.
 *
 *----------------------------------------------------------------------
 */

void
TkpSetCursor(
    TkpCursor cursor)
{
    int cursorChanged = 1;

    if (!gTkOwnsCursor) {
	return;
    }

    if (cursor == None) {
	/*
	 * This is a little tricky. We can't really tell whether
	 * gCurrentCursor is NULL because it was NULL last time around or
	 * because we just freed the current cursor. So if the input cursor is
	 * NULL, we always need to reset it, we can't trust the cursorChanged
	 * logic.
	 */

	gCurrentCursor = NULL;
    } else {
	if (gCurrentCursor == (TkMacOSXCursor *) cursor) {
	    cursorChanged = 0;
	}
	gCurrentCursor = (TkMacOSXCursor *) cursor;
    }

    if (Tk_MacOSXIsAppInFront() && cursorChanged) {
	TkMacOSXInstallCursor(gResizeOverride);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MacOSXTkOwnsCursor --
 *
 *	Sets whether Tk has the right to adjust the cursor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May keep Tk from changing the cursor.
 *
 *----------------------------------------------------------------------
 */

void
Tk_MacOSXTkOwnsCursor(
    int tkOwnsIt)
{
    gTkOwnsCursor = tkOwnsIt;
}
