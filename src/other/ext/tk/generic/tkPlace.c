/*
 * tkPlace.c --
 *
 *	This file contains code to implement a simple geometry manager for Tk
 *	based on absolute placement or "rubber-sheet" placement.
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkInt.h"

/*
 * Border modes for relative placement:
 *
 * BM_INSIDE:		relative distances computed using area inside all
 *			borders of container window.
 * BM_OUTSIDE:		relative distances computed using outside area that
 *			includes all borders of container.
 * BM_IGNORE:		border issues are ignored: place relative to container's
 *			actual window size.
 */

static const char *const borderModeStrings[] = {
    "inside", "outside", "ignore", NULL
};

typedef enum {BM_INSIDE, BM_OUTSIDE, BM_IGNORE} BorderMode;

/*
 * For each window whose geometry is managed by the placer there is a
 * structure of the following type:
 */

typedef struct Content {
    Tk_Window tkwin;		/* Tk's token for window. */
    Tk_Window inTkwin;		/* Token for the -in window. */
    struct Container *containerPtr;	/* Pointer to information for window relative
				 * to which tkwin is placed. This isn't
				 * necessarily the logical parent of tkwin.
				 * NULL means the container was deleted or never
				 * assigned. */
    struct Content *nextPtr;	/* Next in list of windows placed relative to
				 * same container (NULL for end of list). */
    Tk_OptionTable optionTable;	/* Table that defines configuration options
				 * available for this command. */
    /*
     * Geometry information for window; where there are both relative and
     * absolute values for the same attribute (e.g. x and relX) only one of
     * them is actually used, depending on flags.
     */

    int x, y;			/* X and Y pixel coordinates for tkwin. */
    Tcl_Obj *xPtr, *yPtr;	/* Tcl_Obj rep's of x, y coords, to keep pixel
				 * spec. information. */
    double relX, relY;		/* X and Y coordinates relative to size of
				 * container. */
    int width, height;		/* Absolute dimensions for tkwin. */
    Tcl_Obj *widthPtr;		/* Tcl_Obj rep of width, to keep pixel
				 * spec. */
    Tcl_Obj *heightPtr;		/* Tcl_Obj rep of height, to keep pixel
				 * spec. */
    double relWidth, relHeight;	/* Dimensions for tkwin relative to size of
				 * container. */
    Tcl_Obj *relWidthPtr;
    Tcl_Obj *relHeightPtr;
    Tk_Anchor anchor;		/* Which point on tkwin is placed at the given
				 * position. */
    BorderMode borderMode;	/* How to treat borders of container window. */
    int flags;			/* Various flags; see below for bit
				 * definitions. */
} Content;

/*
 * Type masks for options:
 */

#define IN_MASK		1

static const Tk_OptionSpec optionSpecs[] = {
    {TK_OPTION_ANCHOR, "-anchor", NULL, NULL, "nw", -1,
	 Tk_Offset(Content, anchor), 0, 0, 0},
    {TK_OPTION_STRING_TABLE, "-bordermode", NULL, NULL, "inside", -1,
	 Tk_Offset(Content, borderMode), 0, borderModeStrings, 0},
    {TK_OPTION_PIXELS, "-height", NULL, NULL, "", Tk_Offset(Content, heightPtr),
	 Tk_Offset(Content, height), TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_WINDOW, "-in", NULL, NULL, "", -1, Tk_Offset(Content, inTkwin),
	 0, 0, IN_MASK},
    {TK_OPTION_DOUBLE, "-relheight", NULL, NULL, "",
	 Tk_Offset(Content, relHeightPtr), Tk_Offset(Content, relHeight),
	 TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_DOUBLE, "-relwidth", NULL, NULL, "",
	 Tk_Offset(Content, relWidthPtr), Tk_Offset(Content, relWidth),
	 TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_DOUBLE, "-relx", NULL, NULL, "0", -1,
	 Tk_Offset(Content, relX), 0, 0, 0},
    {TK_OPTION_DOUBLE, "-rely", NULL, NULL, "0", -1,
	 Tk_Offset(Content, relY), 0, 0, 0},
    {TK_OPTION_PIXELS, "-width", NULL, NULL, "", Tk_Offset(Content, widthPtr),
	 Tk_Offset(Content, width), TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_PIXELS, "-x", NULL, NULL, "0", Tk_Offset(Content, xPtr),
	 Tk_Offset(Content, x), TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_PIXELS, "-y", NULL, NULL, "0", Tk_Offset(Content, yPtr),
	 Tk_Offset(Content, y), TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_END, NULL, NULL, NULL, NULL, 0, -1, 0, 0, 0}
};

/*
 * Flag definitions for Content structures:
 *
 * CHILD_WIDTH -		1 means -width was specified;
 * CHILD_REL_WIDTH -		1 means -relwidth was specified.
 * CHILD_HEIGHT -		1 means -height was specified;
 * CHILD_REL_HEIGHT -		1 means -relheight was specified.
 */

#define CHILD_WIDTH		1
#define CHILD_REL_WIDTH		2
#define CHILD_HEIGHT		4
#define CHILD_REL_HEIGHT	8

/*
 * For each container window that has a content managed by the placer there is a
 * structure of the following form:
 */

typedef struct Container {
    Tk_Window tkwin;		/* Tk's token for container window. */
    struct Content *contentPtr;	/* First in linked list of content placed
				 * relative to this container. */
    int *abortPtr;		/* If non-NULL, it means that there is a nested
				 * call to RecomputePlacement already working on
				 * this window.  *abortPtr may be set to 1 to
				 * abort that nested call.  This happens, for
				 * example, if tkwin or any of its content
				 * is deleted. */
    int flags;			/* See below for bit definitions. */
} Container;

/*
 * Flag definitions for containers:
 *
 * PARENT_RECONFIG_PENDING -	1 means that a call to RecomputePlacement is
 *				already pending via a Do_When_Idle handler.
 */

#define PARENT_RECONFIG_PENDING	1

/*
 * The following structure is the official type record for the placer:
 */

static void		PlaceRequestProc(ClientData clientData,
			    Tk_Window tkwin);
static void		PlaceLostContentProc(ClientData clientData,
			    Tk_Window tkwin);

static const Tk_GeomMgr placerType = {
    "place",			/* name */
    PlaceRequestProc,		/* requestProc */
    PlaceLostContentProc,		/* lostContentProc */
};

/*
 * Forward declarations for functions defined later in this file:
 */

static void		ContentStructureProc(ClientData clientData,
			    XEvent *eventPtr);
static int		ConfigureContent(Tcl_Interp *interp, Tk_Window tkwin,
			    Tk_OptionTable table, int objc,
			    Tcl_Obj *const objv[]);
static int		PlaceInfoCommand(Tcl_Interp *interp, Tk_Window tkwin);
static Content *		CreateContent(Tk_Window tkwin, Tk_OptionTable table);
static void		FreeContent(Content *contentPtr);
static Content *		FindContent(Tk_Window tkwin);
static Container *		CreateContainer(Tk_Window tkwin);
static Container *		FindContainer(Tk_Window tkwin);
static void		PlaceStructureProc(ClientData clientData,
			    XEvent *eventPtr);
static void		RecomputePlacement(ClientData clientData);
static void		UnlinkContent(Content *contentPtr);

/*
 *--------------------------------------------------------------
 *
 * Tk_PlaceObjCmd --
 *
 *	This function is invoked to process the "place" Tcl commands. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
Tk_PlaceObjCmd(
    ClientData clientData,	/* Interpreter main window. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tk_Window main_win = (Tk_Window)clientData;
    Tk_Window tkwin;
    Content *contentPtr;
    TkDisplay *dispPtr;
    Tk_OptionTable optionTable;
    static const char *const optionStrings[] = {
	"configure", "content", "forget", "info", "slaves", NULL
    };
    enum options { PLACE_CONFIGURE, PLACE_CONTENT, PLACE_FORGET, PLACE_INFO, PLACE_SLAVES };
    int index;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "option|pathName args");
	return TCL_ERROR;
    }

    /*
     * Create the option table for this widget class. If it has already been
     * created, the cached pointer will be returned.
     */

     optionTable = Tk_CreateOptionTable(interp, optionSpecs);

    /*
     * Handle special shortcut where window name is first argument.
     */

    if (Tcl_GetString(objv[1])[0] == '.') {
	if (TkGetWindowFromObj(interp, main_win, objv[1],
		&tkwin) != TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * Initialize, if that hasn't been done yet.
	 */

	dispPtr = ((TkWindow *) tkwin)->dispPtr;
	if (!dispPtr->placeInit) {
	    Tcl_InitHashTable(&dispPtr->masterTable, TCL_ONE_WORD_KEYS);
	    Tcl_InitHashTable(&dispPtr->slaveTable, TCL_ONE_WORD_KEYS);
	    dispPtr->placeInit = 1;
	}

	return ConfigureContent(interp, tkwin, optionTable, objc-2, objv+2);
    }

    /*
     * Handle more general case of option followed by window name followed by
     * possible additional arguments.
     */

    if (TkGetWindowFromObj(interp, main_win, objv[2],
	    &tkwin) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Initialize, if that hasn't been done yet.
     */

    dispPtr = ((TkWindow *) tkwin)->dispPtr;
    if (!dispPtr->placeInit) {
	Tcl_InitHashTable(&dispPtr->masterTable, TCL_ONE_WORD_KEYS);
	Tcl_InitHashTable(&dispPtr->slaveTable, TCL_ONE_WORD_KEYS);
	dispPtr->placeInit = 1;
    }

    if (Tcl_GetIndexFromObjStruct(interp, objv[1], optionStrings,
	    sizeof(char *), "option", 0, &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch ((enum options) index) {
    case PLACE_CONFIGURE:
	if (objc == 3 || objc == 4) {
	    Tcl_Obj *objPtr;

	    contentPtr = FindContent(tkwin);
	    if (contentPtr == NULL) {
		return TCL_OK;
	    }
	    objPtr = Tk_GetOptionInfo(interp, (char *)contentPtr, optionTable,
		    (objc == 4) ? objv[3] : NULL, tkwin);
	    if (objPtr == NULL) {
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, objPtr);
	    return TCL_OK;
	}
	return ConfigureContent(interp, tkwin, optionTable, objc-3, objv+3);

    case PLACE_FORGET:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "pathName");
	    return TCL_ERROR;
	}
	contentPtr = FindContent(tkwin);
	if (contentPtr == NULL) {
	    return TCL_OK;
	}
	if ((contentPtr->containerPtr != NULL) &&
		(contentPtr->containerPtr->tkwin != Tk_Parent(contentPtr->tkwin))) {
	    Tk_UnmaintainGeometry(contentPtr->tkwin, contentPtr->containerPtr->tkwin);
	}
	UnlinkContent(contentPtr);
	Tcl_DeleteHashEntry(Tcl_FindHashEntry(&dispPtr->slaveTable,
		(void *)tkwin));
	Tk_DeleteEventHandler(tkwin, StructureNotifyMask, ContentStructureProc,
		contentPtr);
	Tk_ManageGeometry(tkwin, NULL, NULL);
	Tk_UnmapWindow(tkwin);
	FreeContent(contentPtr);
	break;

    case PLACE_INFO:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "pathName");
	    return TCL_ERROR;
	}
	return PlaceInfoCommand(interp, tkwin);

    case PLACE_CONTENT:
    case PLACE_SLAVES: {
	Container *containerPtr;

	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "pathName");
	    return TCL_ERROR;
	}
	containerPtr = FindContainer(tkwin);
	if (containerPtr != NULL) {
	    Tcl_Obj *listPtr = Tcl_NewObj();

	    for (contentPtr = containerPtr->contentPtr; contentPtr != NULL;
		    contentPtr = contentPtr->nextPtr) {
		Tcl_ListObjAppendElement(NULL, listPtr,
			TkNewWindowObj(contentPtr->tkwin));
	    }
	    Tcl_SetObjResult(interp, listPtr);
	}
	break;
    }
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateContent --
 *
 *	Given a Tk_Window token, find the Content structure corresponding to
 *	that token, creating a new one if necessary.
 *
 * Results:
 *	Pointer to the Content structure.
 *
 * Side effects:
 *	A new Content structure may be created.
 *
 *----------------------------------------------------------------------
 */

static Content *
CreateContent(
    Tk_Window tkwin,		/* Token for desired content. */
    Tk_OptionTable table)
{
    Tcl_HashEntry *hPtr;
    Content *contentPtr;
    int isNew;
    TkDisplay *dispPtr = ((TkWindow *) tkwin)->dispPtr;

    hPtr = Tcl_CreateHashEntry(&dispPtr->slaveTable, (char *) tkwin, &isNew);
    if (!isNew) {
	return (Content *)Tcl_GetHashValue(hPtr);
    }

    /*
     * No preexisting content structure for that window, so make a new one and
     * populate it with some default values.
     */

    contentPtr = (Content *)ckalloc(sizeof(Content));
    memset(contentPtr, 0, sizeof(Content));
    contentPtr->tkwin = tkwin;
    contentPtr->inTkwin = NULL;
    contentPtr->anchor = TK_ANCHOR_NW;
    contentPtr->borderMode = BM_INSIDE;
    contentPtr->optionTable = table;
    Tcl_SetHashValue(hPtr, contentPtr);
    Tk_CreateEventHandler(tkwin, StructureNotifyMask, ContentStructureProc,
	    contentPtr);
    return contentPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeContent --
 *
 *	Frees the resources held by a Content structure.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Memory are freed.
 *
 *----------------------------------------------------------------------
 */

static void
FreeContent(
    Content *contentPtr)
{
    Tk_FreeConfigOptions((char *) contentPtr, contentPtr->optionTable,
	    contentPtr->tkwin);
    ckfree(contentPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FindContent --
 *
 *	Given a Tk_Window token, find the Content structure corresponding to
 *	that token. This is purely a lookup function; it will not create a
 *	record if one does not yet exist.
 *
 * Results:
 *	Pointer to Content structure; NULL if none exists.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Content *
FindContent(
    Tk_Window tkwin)		/* Token for desired content. */
{
    Tcl_HashEntry *hPtr;
    TkDisplay *dispPtr = ((TkWindow *) tkwin)->dispPtr;

    hPtr = Tcl_FindHashEntry(&dispPtr->slaveTable, (char *) tkwin);
    if (hPtr == NULL) {
	return NULL;
    }
    return (Content *)Tcl_GetHashValue(hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * UnlinkContent --
 *
 *	This function removes a content window from the chain of content in its
 *	container.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The content list of contentPtr's container changes.
 *
 *----------------------------------------------------------------------
 */

static void
UnlinkContent(
    Content *contentPtr)		/* Content structure to be unlinked. */
{
    Container *containerPtr;
    Content *prevPtr;

    containerPtr = contentPtr->containerPtr;
    if (containerPtr == NULL) {
	return;
    }
    if (containerPtr->contentPtr == contentPtr) {
	containerPtr->contentPtr = contentPtr->nextPtr;
    } else {
	for (prevPtr = containerPtr->contentPtr; ; prevPtr = prevPtr->nextPtr) {
	    if (prevPtr == NULL) {
		Tcl_Panic("UnlinkContent couldn't find slave to unlink");
	    }
	    if (prevPtr->nextPtr == contentPtr) {
		prevPtr->nextPtr = contentPtr->nextPtr;
		break;
	    }
	}
    }

    if (containerPtr->abortPtr != NULL) {
	*containerPtr->abortPtr = 1;
    }
    contentPtr->containerPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateContainer --
 *
 *	Given a Tk_Window token, find the Container structure corresponding to
 *	that token, creating a new one if necessary.
 *
 * Results:
 *	Pointer to the Container structure.
 *
 * Side effects:
 *	A new Container structure may be created.
 *
 *----------------------------------------------------------------------
 */

static Container *
CreateContainer(
    Tk_Window tkwin)		/* Token for desired container. */
{
    Tcl_HashEntry *hPtr;
    Container *containerPtr;
    int isNew;
    TkDisplay *dispPtr = ((TkWindow *) tkwin)->dispPtr;

    hPtr = Tcl_CreateHashEntry(&dispPtr->masterTable, (char *)tkwin, &isNew);
    if (isNew) {
	containerPtr = (Container *)ckalloc(sizeof(Container));
	containerPtr->tkwin = tkwin;
	containerPtr->contentPtr = NULL;
	containerPtr->abortPtr = NULL;
	containerPtr->flags = 0;
	Tcl_SetHashValue(hPtr, containerPtr);
	Tk_CreateEventHandler(containerPtr->tkwin, StructureNotifyMask,
		PlaceStructureProc, containerPtr);
    } else {
	containerPtr = (Container *)Tcl_GetHashValue(hPtr);
    }
    return containerPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FindContainer --
 *
 *	Given a Tk_Window token, find the Container structure corresponding to
 *	that token. This is simply a lookup function; a new record will not be
 *	created if one does not already exist.
 *
 * Results:
 *	Pointer to the Container structure; NULL if one does not exist for the
 *	given Tk_Window token.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Container *
FindContainer(
    Tk_Window tkwin)		/* Token for desired container. */
{
    Tcl_HashEntry *hPtr;
    TkDisplay *dispPtr = ((TkWindow *) tkwin)->dispPtr;

    hPtr = Tcl_FindHashEntry(&dispPtr->masterTable, (char *) tkwin);
    if (hPtr == NULL) {
	return NULL;
    }
    return (Container *)Tcl_GetHashValue(hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureContent --
 *
 *	This function is called to process an argv/argc list to reconfigure
 *	the placement of a window.
 *
 * Results:
 *	A standard Tcl result. If an error occurs then a message is left in
 *	the interp's result.
 *
 * Side effects:
 *	Information in contentPtr may change, and contentPtr's container is scheduled
 *	for reconfiguration.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureContent(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tk_Window tkwin,		/* Token for the window to manipulate. */
    Tk_OptionTable table,	/* Token for option table. */
    int objc,			/* Number of config arguments. */
    Tcl_Obj *const objv[])	/* Object values for arguments. */
{
    Container *containerPtr;
    Tk_SavedOptions savedOptions;
    int mask;
    Content *contentPtr;
    Tk_Window containerWin = NULL;
    TkWindow *container;

    if (Tk_TopWinHierarchy(tkwin)) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"can't use placer on top-level window \"%s\"; use "
		"wm command instead", Tk_PathName(tkwin)));
	Tcl_SetErrorCode(interp, "TK", "GEOMETRY", "TOPLEVEL", NULL);
	return TCL_ERROR;
    }

    contentPtr = CreateContent(tkwin, table);

    if (Tk_SetOptions(interp, (char *)contentPtr, table, objc, objv,
	    contentPtr->tkwin, &savedOptions, &mask) != TCL_OK) {
	goto error;
    }

    /*
     * Set content flags. First clear the field, then add bits as needed.
     */

    contentPtr->flags = 0;
    if (contentPtr->heightPtr) {
	contentPtr->flags |= CHILD_HEIGHT;
    }

    if (contentPtr->relHeightPtr) {
	contentPtr->flags |= CHILD_REL_HEIGHT;
    }

    if (contentPtr->relWidthPtr) {
	contentPtr->flags |= CHILD_REL_WIDTH;
    }

    if (contentPtr->widthPtr) {
	contentPtr->flags |= CHILD_WIDTH;
    }

    if (!(mask & IN_MASK) && (contentPtr->containerPtr != NULL)) {
	/*
	 * If no -in option was passed and the content is already placed then
	 * just recompute the placement.
	 */

	containerPtr = contentPtr->containerPtr;
	goto scheduleLayout;
    } else if (mask & IN_MASK) {
	/* -in changed */
	Tk_Window win;
	Tk_Window ancestor;

	win = contentPtr->inTkwin;

	/*
	 * Make sure that the new container is either the logical parent of the
	 * content or a descendant of that window, and that the container and content
	 * aren't the same.
	 */

	for (ancestor = win; ; ancestor = Tk_Parent(ancestor)) {
	    if (ancestor == Tk_Parent(contentPtr->tkwin)) {
		break;
	    }
	    if (Tk_TopWinHierarchy(ancestor)) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"can't place %s relative to %s",
			Tk_PathName(contentPtr->tkwin), Tk_PathName(win)));
		Tcl_SetErrorCode(interp, "TK", "GEOMETRY", "HIERARCHY", NULL);
		goto error;
	    }
	}
	if (contentPtr->tkwin == win) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "can't place %s relative to itself",
		    Tk_PathName(contentPtr->tkwin)));
	    Tcl_SetErrorCode(interp, "TK", "GEOMETRY", "LOOP", NULL);
	    goto error;
	}

	/*
	 * Check for management loops.
	 */

	for (container = (TkWindow *)win; container != NULL;
	     container = (TkWindow *)TkGetContainer(container)) {
	    if (container == (TkWindow *)contentPtr->tkwin) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "can't put %s inside %s, would cause management loop",
	            Tk_PathName(contentPtr->tkwin), Tk_PathName(win)));
		Tcl_SetErrorCode(interp, "TK", "GEOMETRY", "LOOP", NULL);
		goto error;
	    }
	}
	if (win != Tk_Parent(contentPtr->tkwin)) {
	    ((TkWindow *)contentPtr->tkwin)->maintainerPtr = (TkWindow *)win;
	}

	if ((contentPtr->containerPtr != NULL)
		&& (contentPtr->containerPtr->tkwin == win)) {
	    /*
	     * Re-using same old container. Nothing to do.
	     */

	    containerPtr = contentPtr->containerPtr;
	    goto scheduleLayout;
	}
	if ((contentPtr->containerPtr != NULL) &&
		(contentPtr->containerPtr->tkwin != Tk_Parent(contentPtr->tkwin))) {
	    Tk_UnmaintainGeometry(contentPtr->tkwin, contentPtr->containerPtr->tkwin);
	}
	UnlinkContent(contentPtr);
	containerWin = win;
    }

    /*
     * If there's no container specified for this content, use its Tk_Parent.
     */

    if (containerWin == NULL) {
	containerWin = Tk_Parent(contentPtr->tkwin);
	contentPtr->inTkwin = containerWin;
    }

    /*
     * Manage the content window in this container.
     */

    containerPtr = CreateContainer(containerWin);
    contentPtr->containerPtr = containerPtr;
    contentPtr->nextPtr = containerPtr->contentPtr;
    containerPtr->contentPtr = contentPtr;
    Tk_ManageGeometry(contentPtr->tkwin, &placerType, contentPtr);

    /*
     * Arrange for the container to be re-arranged at the first idle moment.
     */

  scheduleLayout:
    Tk_FreeSavedOptions(&savedOptions);

    if (!(containerPtr->flags & PARENT_RECONFIG_PENDING)) {
	containerPtr->flags |= PARENT_RECONFIG_PENDING;
	Tcl_DoWhenIdle(RecomputePlacement, containerPtr);
    }
    return TCL_OK;

    /*
     * Error while processing some option, cleanup and return.
     */

  error:
    Tk_RestoreSavedOptions(&savedOptions);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * PlaceInfoCommand --
 *
 *	Implementation of the [place info] subcommand. See the user
 *	documentation for information on what it does.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	If the given tkwin is managed by the placer, this function will put
 *	information about that placement in the interp's result.
 *
 *----------------------------------------------------------------------
 */

static int
PlaceInfoCommand(
    Tcl_Interp *interp,		/* Interp into which to place result. */
    Tk_Window tkwin)		/* Token for the window to get info on. */
{
    Content *contentPtr;
    Tcl_Obj *infoObj;

    contentPtr = FindContent(tkwin);
    if (contentPtr == NULL) {
	return TCL_OK;
    }
    infoObj = Tcl_NewObj();
    if (contentPtr->containerPtr != NULL) {
	Tcl_AppendToObj(infoObj, "-in", -1);
	Tcl_ListObjAppendElement(NULL, infoObj,
		TkNewWindowObj(contentPtr->containerPtr->tkwin));
	Tcl_AppendToObj(infoObj, " ", -1);
    }
    Tcl_AppendPrintfToObj(infoObj,
	    "-x %d -relx %.4g -y %d -rely %.4g",
	    contentPtr->x, contentPtr->relX, contentPtr->y, contentPtr->relY);
    if (contentPtr->flags & CHILD_WIDTH) {
	Tcl_AppendPrintfToObj(infoObj, " -width %d", contentPtr->width);
    } else {
	Tcl_AppendToObj(infoObj, " -width {}", -1);
    }
    if (contentPtr->flags & CHILD_REL_WIDTH) {
	Tcl_AppendPrintfToObj(infoObj,
		" -relwidth %.4g", contentPtr->relWidth);
    } else {
	Tcl_AppendToObj(infoObj, " -relwidth {}", -1);
    }
    if (contentPtr->flags & CHILD_HEIGHT) {
	Tcl_AppendPrintfToObj(infoObj, " -height %d", contentPtr->height);
    } else {
	Tcl_AppendToObj(infoObj, " -height {}", -1);
    }
    if (contentPtr->flags & CHILD_REL_HEIGHT) {
	Tcl_AppendPrintfToObj(infoObj,
		" -relheight %.4g", contentPtr->relHeight);
    } else {
	Tcl_AppendToObj(infoObj, " -relheight {}", -1);
    }

    Tcl_AppendPrintfToObj(infoObj, " -anchor %s -bordermode %s",
	    Tk_NameOfAnchor(contentPtr->anchor),
	    borderModeStrings[contentPtr->borderMode]);
    Tcl_SetObjResult(interp, infoObj);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RecomputePlacement --
 *
 *	This function is called as a when-idle handler. It recomputes the
 *	geometries of all the content of a given container.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Windows may change size or shape.
 *
 *----------------------------------------------------------------------
 */

static void
RecomputePlacement(
    ClientData clientData)	/* Pointer to Container record. */
{
    Container *containerPtr = (Container *)clientData;
    Content *contentPtr;
    int x, y, width, height, tmp;
    int containerWidth, containerHeight, containerX, containerY;
    double x1, y1, x2, y2;
    int abort;			/* May get set to non-zero to abort this
				 * placement operation. */

    containerPtr->flags &= ~PARENT_RECONFIG_PENDING;

    /*
     * Abort any nested call to RecomputePlacement for this window, since
     * we'll do everything necessary here, and set up so this call can be
     * aborted if necessary.
     */

    if (containerPtr->abortPtr != NULL) {
	*containerPtr->abortPtr = 1;
    }
    containerPtr->abortPtr = &abort;
    abort = 0;
    Tcl_Preserve(containerPtr);

    /*
     * Iterate over all the content for the container. Each content's geometry can
     * be computed independently of the other content. Changes to the window's
     * structure could cause almost anything to happen, including deleting the
     * parent or child. If this happens, we'll be told to abort.
     */

    for (contentPtr = containerPtr->contentPtr; contentPtr != NULL && !abort;
	    contentPtr = contentPtr->nextPtr) {
	/*
	 * Step 1: compute size and borderwidth of container, taking into account
	 * desired border mode.
	 */

	containerX = containerY = 0;
	containerWidth = Tk_Width(containerPtr->tkwin);
	containerHeight = Tk_Height(containerPtr->tkwin);
	if (contentPtr->borderMode == BM_INSIDE) {
	    containerX = Tk_InternalBorderLeft(containerPtr->tkwin);
	    containerY = Tk_InternalBorderTop(containerPtr->tkwin);
	    containerWidth -= containerX + Tk_InternalBorderRight(containerPtr->tkwin);
	    containerHeight -= containerY +
		    Tk_InternalBorderBottom(containerPtr->tkwin);
	} else if (contentPtr->borderMode == BM_OUTSIDE) {
	    containerX = containerY = -Tk_Changes(containerPtr->tkwin)->border_width;
	    containerWidth -= 2 * containerX;
	    containerHeight -= 2 * containerY;
	}

	/*
	 * Step 2: compute size of content (outside dimensions including border)
	 * and location of anchor point within container.
	 */

	x1 = contentPtr->x + containerX + (contentPtr->relX*containerWidth);
	x = (int) (x1 + ((x1 > 0) ? 0.5 : -0.5));
	y1 = contentPtr->y + containerY + (contentPtr->relY*containerHeight);
	y = (int) (y1 + ((y1 > 0) ? 0.5 : -0.5));
	if (contentPtr->flags & (CHILD_WIDTH|CHILD_REL_WIDTH)) {
	    width = 0;
	    if (contentPtr->flags & CHILD_WIDTH) {
		width += contentPtr->width;
	    }
	    if (contentPtr->flags & CHILD_REL_WIDTH) {
		/*
		 * The code below is a bit tricky. In order to round correctly
		 * when both relX and relWidth are specified, compute the
		 * location of the right edge and round that, then compute
		 * width. If we compute the width and round it, rounding
		 * errors in relX and relWidth accumulate.
		 */

		x2 = x1 + (contentPtr->relWidth*containerWidth);
		tmp = (int) (x2 + ((x2 > 0) ? 0.5 : -0.5));
		width += tmp - x;
	    }
	} else {
	    width = Tk_ReqWidth(contentPtr->tkwin)
		    + 2*Tk_Changes(contentPtr->tkwin)->border_width;
	}
	if (contentPtr->flags & (CHILD_HEIGHT|CHILD_REL_HEIGHT)) {
	    height = 0;
	    if (contentPtr->flags & CHILD_HEIGHT) {
		height += contentPtr->height;
	    }
	    if (contentPtr->flags & CHILD_REL_HEIGHT) {
		/*
		 * See note above for rounding errors in width computation.
		 */

		y2 = y1 + (contentPtr->relHeight*containerHeight);
		tmp = (int) (y2 + ((y2 > 0) ? 0.5 : -0.5));
		height += tmp - y;
	    }
	} else {
	    height = Tk_ReqHeight(contentPtr->tkwin)
		    + 2*Tk_Changes(contentPtr->tkwin)->border_width;
	}

	/*
	 * Step 3: adjust the x and y positions so that the desired anchor
	 * point on the content appears at that position. Also adjust for the
	 * border mode and container's border.
	 */

	switch (contentPtr->anchor) {
	case TK_ANCHOR_N:
	    x -= width/2;
	    break;
	case TK_ANCHOR_NE:
	    x -= width;
	    break;
	case TK_ANCHOR_E:
	    x -= width;
	    y -= height/2;
	    break;
	case TK_ANCHOR_SE:
	    x -= width;
	    y -= height;
	    break;
	case TK_ANCHOR_S:
	    x -= width/2;
	    y -= height;
	    break;
	case TK_ANCHOR_SW:
	    y -= height;
	    break;
	case TK_ANCHOR_W:
	    y -= height/2;
	    break;
	case TK_ANCHOR_NW:
	    break;
	case TK_ANCHOR_CENTER:
	    x -= width/2;
	    y -= height/2;
	    break;
	}

	/*
	 * Step 4: adjust width and height again to reflect inside dimensions
	 * of window rather than outside. Also make sure that the width and
	 * height aren't zero.
	 */

	width -= 2*Tk_Changes(contentPtr->tkwin)->border_width;
	height -= 2*Tk_Changes(contentPtr->tkwin)->border_width;
	if (width <= 0) {
	    width = 1;
	}
	if (height <= 0) {
	    height = 1;
	}

	/*
	 * Step 5: reconfigure the window and map it if needed. If the content
	 * is a child of the container, we do this ourselves. If the content isn't
	 * a child of the container, let Tk_MaintainGeometry do the work (it will
	 * re-adjust things as relevant windows map, unmap, and move).
	 */

	if (containerPtr->tkwin == Tk_Parent(contentPtr->tkwin)) {
	    if ((x != Tk_X(contentPtr->tkwin))
		    || (y != Tk_Y(contentPtr->tkwin))
		    || (width != Tk_Width(contentPtr->tkwin))
		    || (height != Tk_Height(contentPtr->tkwin))) {
		Tk_MoveResizeWindow(contentPtr->tkwin, x, y, width, height);
	    }
            if (abort) {
                break;
            }

	    /*
	     * Don't map the content unless the container is mapped: the content will
	     * get mapped later, when the container is mapped.
	     */

	    if (Tk_IsMapped(containerPtr->tkwin)) {
		Tk_MapWindow(contentPtr->tkwin);
	    }
	} else {
	    if ((width <= 0) || (height <= 0)) {
		Tk_UnmaintainGeometry(contentPtr->tkwin, containerPtr->tkwin);
		Tk_UnmapWindow(contentPtr->tkwin);
	    } else {
		Tk_MaintainGeometry(contentPtr->tkwin, containerPtr->tkwin,
			x, y, width, height);
	    }
	}
    }

    containerPtr->abortPtr = NULL;
    Tcl_Release(containerPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * PlaceStructureProc --
 *
 *	This function is invoked by the Tk event handler when StructureNotify
 *	events occur for a container window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Structures get cleaned up if the window was deleted. If the window was
 *	resized then content geometries get recomputed.
 *
 *----------------------------------------------------------------------
 */

static void
PlaceStructureProc(
    ClientData clientData,	/* Pointer to Container structure for window
				 * referred to by eventPtr. */
    XEvent *eventPtr)		/* Describes what just happened. */
{
    Container *containerPtr = (Container *)clientData;
    Content *contentPtr, *nextPtr;
    TkDisplay *dispPtr = ((TkWindow *) containerPtr->tkwin)->dispPtr;

    switch (eventPtr->type) {
    case ConfigureNotify:
	if ((containerPtr->contentPtr != NULL)
		&& !(containerPtr->flags & PARENT_RECONFIG_PENDING)) {
	    containerPtr->flags |= PARENT_RECONFIG_PENDING;
	    Tcl_DoWhenIdle(RecomputePlacement, containerPtr);
	}
	return;
    case DestroyNotify:
	for (contentPtr = containerPtr->contentPtr; contentPtr != NULL;
		contentPtr = nextPtr) {
	    contentPtr->containerPtr = NULL;
	    nextPtr = contentPtr->nextPtr;
	    contentPtr->nextPtr = NULL;
	}
	Tcl_DeleteHashEntry(Tcl_FindHashEntry(&dispPtr->masterTable,
		(char *) containerPtr->tkwin));
	if (containerPtr->flags & PARENT_RECONFIG_PENDING) {
	    Tcl_CancelIdleCall(RecomputePlacement, containerPtr);
	}
	containerPtr->tkwin = NULL;
	if (containerPtr->abortPtr != NULL) {
	    *containerPtr->abortPtr = 1;
	}
	Tcl_EventuallyFree(containerPtr, TCL_DYNAMIC);
	return;
    case MapNotify:
	/*
	 * When a container gets mapped, must redo the geometry computation so
	 * that all of its content get remapped.
	 */

	if ((containerPtr->contentPtr != NULL)
		&& !(containerPtr->flags & PARENT_RECONFIG_PENDING)) {
	    containerPtr->flags |= PARENT_RECONFIG_PENDING;
	    Tcl_DoWhenIdle(RecomputePlacement, containerPtr);
	}
	return;
    case UnmapNotify:
	/*
	 * Unmap all of the content when the container gets unmapped, so that they
	 * don't keep redisplaying themselves.
	 */

	for (contentPtr = containerPtr->contentPtr; contentPtr != NULL;
		contentPtr = contentPtr->nextPtr) {
	    Tk_UnmapWindow(contentPtr->tkwin);
	}
	return;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ContentStructureProc --
 *
 *	This function is invoked by the Tk event handler when StructureNotify
 *	events occur for a content window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Structures get cleaned up if the window was deleted.
 *
 *----------------------------------------------------------------------
 */

static void
ContentStructureProc(
    ClientData clientData,	/* Pointer to Content structure for window
				 * referred to by eventPtr. */
    XEvent *eventPtr)		/* Describes what just happened. */
{
    Content *contentPtr = (Content *)clientData;
    TkDisplay *dispPtr = ((TkWindow *) contentPtr->tkwin)->dispPtr;

    if (eventPtr->type == DestroyNotify) {
	if (contentPtr->containerPtr != NULL) {
	    UnlinkContent(contentPtr);
	}
	Tcl_DeleteHashEntry(Tcl_FindHashEntry(&dispPtr->slaveTable,
		(char *) contentPtr->tkwin));
	FreeContent(contentPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PlaceRequestProc --
 *
 *	This function is invoked by Tk whenever a content managed by us changes
 *	its requested geometry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window will get relayed out, if its requested size has anything to
 *	do with its actual size.
 *
 *----------------------------------------------------------------------
 */

static void
PlaceRequestProc(
    ClientData clientData,	/* Pointer to our record for content. */
    TCL_UNUSED(Tk_Window))		/* Window that changed its desired size. */
{
    Content *contentPtr = (Content *)clientData;
    Container *containerPtr;

    if ((contentPtr->flags & (CHILD_WIDTH|CHILD_REL_WIDTH))
	    && (contentPtr->flags & (CHILD_HEIGHT|CHILD_REL_HEIGHT))) {
        /*
         * Send a ConfigureNotify to indicate that the size change
         * request was rejected.
         */

        TkDoConfigureNotify((TkWindow *)(contentPtr->tkwin));
	return;
    }
    containerPtr = contentPtr->containerPtr;
    if (containerPtr == NULL) {
	return;
    }
    if (!(containerPtr->flags & PARENT_RECONFIG_PENDING)) {
	containerPtr->flags |= PARENT_RECONFIG_PENDING;
	Tcl_DoWhenIdle(RecomputePlacement, containerPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * PlaceLostContentProc --
 *
 *	This function is invoked by Tk whenever some other geometry claims
 *	control over a content window that used to be managed by us.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Forgets all placer-related information about the content window.
 *
 *--------------------------------------------------------------
 */

static void
PlaceLostContentProc(
    ClientData clientData,	/* Content structure for content window that was
				 * stolen away. */
    Tk_Window tkwin)		/* Tk's handle for the content window. */
{
    Content *contentPtr = (Content *)clientData;
    TkDisplay *dispPtr = ((TkWindow *) contentPtr->tkwin)->dispPtr;

    if (contentPtr->containerPtr->tkwin != Tk_Parent(contentPtr->tkwin)) {
	Tk_UnmaintainGeometry(contentPtr->tkwin, contentPtr->containerPtr->tkwin);
    }
    Tk_UnmapWindow(tkwin);
    UnlinkContent(contentPtr);
    Tcl_DeleteHashEntry(Tcl_FindHashEntry(&dispPtr->slaveTable,
	    (char *) tkwin));
    Tk_DeleteEventHandler(tkwin, StructureNotifyMask, ContentStructureProc,
	    contentPtr);
    FreeContent(contentPtr);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
