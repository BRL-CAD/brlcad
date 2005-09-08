
/*
 * bltContainer.c --
 *
 *	This module implements a container widget for the BLT toolkit.
 *
 * Copyright 1998 Lucent Technologies, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies or any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 *
 *	Container widget created by George A. Howlett
 */

#include "bltInt.h"

#ifndef NO_CONTAINER
#include "bltChain.h"
#ifndef WIN32
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#endif

#define XDEBUG

#define SEARCH_TRIES	100	/* Maximum number of attempts to check for
				 * a given window before failing. */
#define SEARCH_INTERVAL 20	/* Number milliseconds to wait after each 
				 * attempt to find a window. */

#define SEARCH_TKWIN	(1<<0)	/* Search via Tk window pathname. */
#define SEARCH_XID	(1<<1)	/* Search via an XID 0x0000000. */
#define SEARCH_CMD	(1<<2)	/* Search via a command-line arguments. */
#define SEARCH_NAME	(1<<3)	/* Search via the application name. */
#define SEARCH_ALL	(SEARCH_TKWIN | SEARCH_XID | SEARCH_CMD | SEARCH_NAME)

#define CONTAINER_REDRAW		(1<<1)
#define CONTAINER_MAPPED		(1<<2)
#define CONTAINER_FOCUS			(1<<4)
#define CONTAINER_INIT			(1<<5)
#define CONTAINER_MOVE			(1<<7)

#define DEF_CONTAINER_BG_MONO		STD_NORMAL_BG_MONO
#define DEF_CONTAINER_BACKGROUND		STD_NORMAL_BACKGROUND
#define DEF_CONTAINER_BORDERWIDTH	STD_BORDERWIDTH
#define DEF_CONTAINER_COMMAND		(char *)NULL
#define DEF_CONTAINER_CURSOR		(char *)NULL
#define DEF_CONTAINER_HEIGHT		"0"
#define DEF_CONTAINER_HIGHLIGHT_BACKGROUND STD_NORMAL_BACKGROUND
#define DEF_CONTAINER_HIGHLIGHT_BG_MONO	STD_NORMAL_BG_MONO
#define DEF_CONTAINER_HIGHLIGHT_COLOR	RGB_BLACK
#define DEF_CONTAINER_HIGHLIGHT_WIDTH	"2"
#define DEF_CONTAINER_RELIEF		"sunken"
#define DEF_CONTAINER_TAKE_FOCUS	"0"
#define DEF_CONTAINER_TIMEOUT		"20"
#define DEF_CONTAINER_WIDTH		"0"
#define DEF_CONTAINER_WINDOW		(char *)NULL

#if (TK_MAJOR_VERSION == 4)
#define TK_REPARENTED			0x2000
#endif

typedef struct SearchInfoStruct SearchInfo;
typedef void (SearchProc) _ANSI_ARGS_((Display *display, Window window, 
       SearchInfo *searchPtr));

struct SearchInfoStruct {
    SearchProc *proc;
    char *pattern;		/* Search pattern. */

    Window window;		/* XID of last window that matches criteria. */
    int nMatches;		/* Number of windows that match the pattern. */
    int saveNames;		/* Indicates to save the names of the
				 * window XIDs that match the search
				 * criteria. */
    Tcl_DString dString;	/* Will contain the strings of the
				 * window XIDs matching the search
				 * criteria. */
};

typedef struct {
    Tk_Window tkwin;		/* Window that embodies the widget.
                                 * NULL means that the window has been
                                 * destroyed but the data structures
                                 * haven't yet been cleaned up.*/

    Display *display;		/* Display containing widget; needed,
                                 * among other things, to release
                                 * resources after tkwin has already
                                 * gone away. */

    Tcl_Interp *interp;		/* Interpreter associated with widget. */

    Tcl_Command cmdToken;	/* Token for widget's command. */

    unsigned int flags;		/* For bit-field definitions, see above. */

    int inset;			/* Total width of borders; focus
				 * highlight and 3-D border. Indicates
				 * the offset from outside edges to
				 * leave room for borders. */

    Tk_Cursor cursor;		/* X Cursor */

    Tk_3DBorder border;		/* 3D border surrounding the adopted
				 * window. */
    int borderWidth;		/* Width of 3D border. */
    int relief;			/* 3D border relief. */

    Tk_Window tkToplevel;	/* Toplevel (wrapper) window of
				 * container.  It's used to track the
				 * location of the container. If it
				 * moves we need to notify the
				 * embedded application. */
    /*
     * Focus highlight ring
     */
    int highlightWidth;		/* Width in pixels of highlight to
				 * draw around widget when it has the
				 * focus.  <= 0 means don't draw a
				 * highlight. */
    XColor *highlightBgColor;	/* Color for drawing traversal
				 * highlight area when highlight is
				 * off. */
    XColor *highlightColor;	/* Color for drawing traversal highlight. */

    GC highlightGC;		/* GC for focus highlight. */

    char *takeFocus;		/* Says whether to select this widget during
				 * tab traveral operations.  This value isn't
				 * used in C code, but for the widget's Tcl
				 * bindings. */

    int reqWidth, reqHeight;	/* Requested dimensions of the container
				 * window. */

    Window adopted;		/* X window Id or Win32 handle of adopted 
				 * window contained by the widget.  If None, 
				 * no window has been reparented. */
    Tk_Window tkAdopted;	/* Non-NULL if this is a Tk window that's 
				 * been adopted. */
    int adoptedX, adoptedY;	/* Current position of the adopted window. */
    int adoptedWidth;		/* Current width of the adopted window. */
    int adoptedHeight;		/* Current height of the adopted window. */

    int origX, origY;
    int origWidth, origHeight;	/* Dimensions of the window when it
				 * was adopted.  When the window is
				 * released it's returned to it's
				 * original dimensions. */

    int timeout;
} Container;


static Tk_OptionParseProc StringToXID;
static Tk_OptionPrintProc XIDToString;

static Tk_CustomOption XIDOption =
{
    StringToXID, XIDToString, (ClientData)(SEARCH_TKWIN | SEARCH_XID),
};

#ifndef WIN32
static Tk_CustomOption XIDNameOption =
{
    StringToXID, XIDToString, (ClientData)SEARCH_NAME,
};

static Tk_CustomOption XIDCmdOption =
{
    StringToXID, XIDToString, (ClientData)SEARCH_CMD,
};
#endif

extern Tk_CustomOption bltDistanceOption;
extern Tk_CustomOption bltPositiveCountOption;

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_CONTAINER_BG_MONO, Tk_Offset(Container, border),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_CONTAINER_BACKGROUND, Tk_Offset(Container, border),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_CONTAINER_BORDERWIDTH, Tk_Offset(Container, borderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
#ifndef WIN32
    {TK_CONFIG_CUSTOM, "-command", "command", "Command",
	DEF_CONTAINER_WINDOW, Tk_Offset(Container, adopted),
	TK_CONFIG_DONT_SET_DEFAULT, &XIDCmdOption},
#endif
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_CONTAINER_CURSOR, Tk_Offset(Container, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-height", "height", "Height",
	DEF_CONTAINER_HEIGHT, Tk_Offset(Container, reqHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_CONTAINER_HIGHLIGHT_BACKGROUND, 
	Tk_Offset(Container, highlightBgColor), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_CONTAINER_HIGHLIGHT_BG_MONO, 
	Tk_Offset(Container, highlightBgColor), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_CONTAINER_HIGHLIGHT_COLOR, 
	Tk_Offset(Container, highlightColor), 0},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_CONTAINER_HIGHLIGHT_WIDTH, Tk_Offset(Container, highlightWidth),
	TK_CONFIG_DONT_SET_DEFAULT},
#ifndef WIN32
    {TK_CONFIG_CUSTOM, "-name", "name", "Name",
	DEF_CONTAINER_WINDOW, Tk_Offset(Container, adopted),
	TK_CONFIG_DONT_SET_DEFAULT, &XIDNameOption},
#endif
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_CONTAINER_RELIEF, Tk_Offset(Container, relief), 0},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_CONTAINER_TAKE_FOCUS, Tk_Offset(Container, takeFocus),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-timeout", "timeout", "Timeout",
	DEF_CONTAINER_TIMEOUT, Tk_Offset(Container, timeout),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPositiveCountOption},
    {TK_CONFIG_CUSTOM, "-width", "width", "Width",
	DEF_CONTAINER_WIDTH, Tk_Offset(Container, reqWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-window", "window", "Window",
	DEF_CONTAINER_WINDOW, Tk_Offset(Container, adopted),
	TK_CONFIG_DONT_SET_DEFAULT, &XIDOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

/* Forward Declarations */
static Tcl_IdleProc DisplayContainer;
static Tcl_CmdProc ContainerInstCmd;
static Tcl_CmdDeleteProc ContainerInstCmdDeleteProc;
static Tk_EventProc ToplevelEventProc;
static Tk_GenericProc AdoptedWindowEventProc;
static Tk_EventProc ContainerEventProc;
static Tcl_FreeProc DestroyContainer;
static Tcl_CmdProc ContainerCmd;

static void EventuallyRedraw _ANSI_ARGS_((Container *cntrPtr));

#ifdef notdef
/*
 *----------------------------------------------------------------------
 *
 * GetWindowId --
 *
 *      Returns the XID for the Tk_Window given.  Starting in Tk 8.0,
 *      the toplevel widgets are wrapped by another window.
 *      Currently there's no way to get at that window, other than
 *      what is done here: query the X window hierarchy and grab the
 *      parent.
 *
 * Results:
 *      Returns the X Window ID of the widget.  If it's a toplevel, then
 *	the XID of the wrapper is returned.
 *
 *----------------------------------------------------------------------
 */
Window
GetXID(tkwin)
    Tk_Window tkwin;
{
    HWND hWnd;
    TkWinWindow *twdPtr;

    hWnd = Tk_GetHWND(Tk_WindowId(tkwin));
#if (TK_MAJOR_VERSION > 4)
    if (Tk_IsTopLevel(tkwin)) {
	hWnd = GetParent(hWnd);
    }
#endif /* TK_MAJOR_VERSION > 4 */
    twdPtr = Blt_Malloc(sizeof(TkWinWindow));
    twdPtr->handle = hWnd;
    twdPtr->type = TWD_WINDOW;
    twdPtr->winPtr = tkwin;
    return (Window)twdPtr;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * NameOfId --
 *
 *	Returns a string representing the given XID.
 *
 * Results:
 *	A static string containing either the hexidecimal number or
 *	the pathname of a Tk window.
 *
 *----------------------------------------------------------------------
 */
static char *
NameOfId(display, window)
    Display *display;		/* Display containing both the container widget
				 * and the adopted window. */
    Window window;		/* XID of the adopted window. */
{
    if (window != None) {
	Tk_Window tkwin;
	static char string[200];

	/* See first if it's a window that Tk knows about.  */
	/*
	 * Note:  If the wrapper window is reparented, Tk pretends it's
	 *        no longer connected to the toplevel, so if you look for
	 *	  the child of the wrapper tkwin, it's NULL.  
	 */
	tkwin = Tk_IdToWindow(display, window); 
	if ((tkwin != NULL) && (Tk_PathName(tkwin) != NULL)) {
	    return Tk_PathName(tkwin); 
	} 
	sprintf(string, "0x%x", (unsigned int)window);
	return string;
    }
    return "";			/* Return empty string if XID is None. */
}

#ifndef WIN32
/*
 *----------------------------------------------------------------------
 *
 * XGeometryErrorProc --
 *
 *	Flags errors generated from XGetGeometry calls to the X server.
 *
 * Results:
 *	Always returns 0.
 *
 * Side Effects:
 *	Sets a flag, indicating an error occurred.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static int
XGeometryErrorProc(clientData, eventPtr)
    ClientData clientData;
    XErrorEvent *eventPtr;	/* Not used. */
{
    int *errorPtr = clientData;

    *errorPtr = TCL_ERROR;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * GetAdoptedWindowGeometry --
 *
 *	Computes the requested geometry of the container using the 
 *	size of adopted window as a reference.  
 *
 * Results:
 *	A standard Tcl result. 
 *
 * Side Effects:
 *	Sets a flag, indicating an error occurred.
 *
 *----------------------------------------------------------------------
 */
static int
GetAdoptedWindowGeometry(interp, cntrPtr)
    Tcl_Interp *interp;
    Container *cntrPtr;
{
    int x, y, width, height, borderWidth, depth;
    int xOffset, yOffset;
    Window root, dummy;
    Tk_ErrorHandler handler;
    int result;
    int any = -1;
    
    width = height = 1;
    xOffset = yOffset = 0;
    if (cntrPtr->adopted != None) {
	handler = Tk_CreateErrorHandler(cntrPtr->display, any, X_GetGeometry, 
		any, XGeometryErrorProc, &result);
	root = RootWindow(cntrPtr->display, Tk_ScreenNumber(cntrPtr->tkwin));
	XTranslateCoordinates(cntrPtr->display, cntrPtr->adopted,
		      root, 0, 0, &xOffset, &yOffset, &dummy);
	result = XGetGeometry(cntrPtr->display, cntrPtr->adopted, &root, 
		&x, &y, (unsigned int *)&width, (unsigned int *)&height,
	      (unsigned int *)&borderWidth, (unsigned int *)&depth);
	Tk_DeleteErrorHandler(handler);
	XSync(cntrPtr->display, False);
	if (result == 0) {
	    Tcl_AppendResult(interp, "can't get geometry for \"", 
		     NameOfId(cntrPtr->display, cntrPtr->adopted), "\"", 
		     (char *)NULL);
	    return TCL_ERROR;
	}
	cntrPtr->origX = xOffset;
	cntrPtr->origY = yOffset;
	cntrPtr->origWidth = width;
	cntrPtr->origHeight = height;
    } else {
	cntrPtr->origX = cntrPtr->origY = 0;
	cntrPtr->origWidth = cntrPtr->origHeight = 0;
    }
    cntrPtr->adoptedX = x;
    cntrPtr->adoptedY = y;
    cntrPtr->adoptedWidth = width;
    cntrPtr->adoptedHeight = height;
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetChildren --
 *
 *	Returns a chain of the child windows according to their stacking
 *	order.  The window ids are ordered from top to bottom.
 *
 * ------------------------------------------------------------------------
 */
static Blt_Chain *
GetChildren(display, window)
    Display *display;
    Window window;
{
    Window *children;
    unsigned int nChildren;
    Window dummy;

    if (!XQueryTree(display, window, &dummy /*parent*/, &dummy /*root*/, 
		   &children, &nChildren)) {
	return NULL;
    }
    if (nChildren > 0) {
	Blt_Chain *chainPtr;
	register int i;

	chainPtr = Blt_ChainCreate();
	for (i = 0; i < nChildren; i++) {
	    /*
	     *  XQuery returns windows in bottom to top order.
	     *  We'll reverse the order.
	     */
	    Blt_ChainPrepend(chainPtr, (ClientData)children[i]);
	}
	if (children != NULL) {
	    XFree((char *)children);
	}
	return chainPtr;
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * NameSearch --
 *
 *	Traverses the entire window hierarchy, searching for windows 
 *	matching the name field in the SearchInfo structure. This 
 *	routine is recursively called for each successive level in 
 *	the window hierarchy.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The SearchInfo structure will track the number of windows that 
 *	match the given criteria.
 *	
 *----------------------------------------------------------------------
 */
static void
NameSearch(display, window, searchPtr)
    Display *display;
    Window window;
    SearchInfo *searchPtr;
{
    Blt_Chain *chainPtr;
    char *wmName;

    if (XFetchName(display, window, &wmName)) {
	/* Compare the name of the window to the search pattern. */
	if (Tcl_StringMatch(wmName, searchPtr->pattern)) {
	    if (searchPtr->saveNames) { /* Record names of matching windows. */
		Tcl_DStringAppendElement(&(searchPtr->dString), 
			 NameOfId(display, window));
		Tcl_DStringAppendElement(&(searchPtr->dString), wmName);
	    }
	    searchPtr->window = window;
	    searchPtr->nMatches++;
	}
	XFree(wmName);
    }
    /* Process the window's descendants. */
    chainPtr = GetChildren(display, window);
    if (chainPtr != NULL) {
	Blt_ChainLink *linkPtr;
	Window child;

	for (linkPtr = Blt_ChainFirstLink(chainPtr); linkPtr != NULL;
	     linkPtr = Blt_ChainNextLink(linkPtr)) {
	    child = (Window)Blt_ChainGetValue(linkPtr);
	    NameSearch(display, child, searchPtr);
	}
	Blt_ChainDestroy(chainPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CmdSearch --
 *
 *	Traverses the entire window hierarchy, searching for windows 
 *	matching the command-line specified in the SearchInfo structure.  
 *	This routine is recursively called for each successive level
 *	in the window hierarchy.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The SearchInfo structure will track the number of windows that 
 *	match the given command-line.
 *	
 *----------------------------------------------------------------------
 */
static void
CmdSearch(display, window, searchPtr)
    Display *display;
    Window window;
    SearchInfo *searchPtr;
{
    Blt_Chain *chainPtr;
    int cmdArgc;
    char **cmdArgv;

    if (XGetCommand(display, window, &cmdArgv, &cmdArgc)) {
	char *string;

	string = Tcl_Merge(cmdArgc, cmdArgv);
	XFreeStringList(cmdArgv);
	if (Tcl_StringMatch(string, searchPtr->pattern)) {
	    if (searchPtr->saveNames) { /* Record names of matching windows. */
		Tcl_DStringAppendElement(&(searchPtr->dString), 
			 NameOfId(display, window));
		Tcl_DStringAppendElement(&(searchPtr->dString), string);
	    }
	    searchPtr->window = window;
	    searchPtr->nMatches++;
	}
	Blt_Free(string);
    }
    /* Process the window's descendants. */
    chainPtr = GetChildren(display, window);
    if (chainPtr != NULL) {
	Blt_ChainLink *linkPtr;
	Window child;

	for (linkPtr = Blt_ChainFirstLink(chainPtr); linkPtr != NULL;
	     linkPtr = Blt_ChainNextLink(linkPtr)) {
	    child = (Window)Blt_ChainGetValue(linkPtr);
	    CmdSearch(display, child, searchPtr);
	}
	Blt_ChainDestroy(chainPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TimeoutProc --
 *
 *	Procedure called when the timer event elapses.  Used to wait
 *	between attempts checking for the designated window.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Sets a flag, indicating the timeout occurred.
 *
 *----------------------------------------------------------------------
 */
static void
TimeoutProc(clientData)
    ClientData clientData;
{
    int *expirePtr = clientData;

    *expirePtr = TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * TestAndWaitForWindow --
 *
 *	Searches, possibly multiple times, for windows matching the
 *	criteria given, using the search proc also given.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Sets a flag, indicating the timeout occurred.
 *
 *----------------------------------------------------------------------
 */
static void
TestAndWaitForWindow(cntrPtr, searchPtr)
    Container *cntrPtr;		/* Container widget record. */
    SearchInfo *searchPtr;	/* Search criteria. */
{
    Window root;
    Tcl_TimerToken timerToken;
    int expire;
    int i;

    /* Get the root window to start the search.  */
    root = RootWindow(cntrPtr->display, Tk_ScreenNumber(cntrPtr->tkwin));
    timerToken = NULL;
    for (i = 0; i < SEARCH_TRIES; i++) {
	searchPtr->nMatches = 0;
	(*searchPtr->proc)(cntrPtr->display, root, searchPtr);
	if (searchPtr->nMatches > 0) {
	    if (timerToken != NULL) {
		Tcl_DeleteTimerHandler(timerToken);
	    }
	    return;
	}
	expire = FALSE;
	/*   
	 * If the X11 application associated with the adopted window
	 * was just started (via "exec" or "bgexec"), the window may
	 * not exist yet.  We have to wait a little bit for the program
	 * to start up.  Create a timer event break us out of an wait 
	 * loop.  We'll wait for a given interval for the adopted window
	 * to appear.
	 */
	timerToken = Tcl_CreateTimerHandler(cntrPtr->timeout, TimeoutProc, 
		&expire);
	while (!expire) {
	    /* Should file events be allowed? */
	    Tcl_DoOneEvent(TCL_TIMER_EVENTS | TCL_WINDOW_EVENTS | 
			   TCL_FILE_EVENTS);
	}
    }	
}
#else 


/*
 * ------------------------------------------------------------------------
 *
 *  GetChildren --
 *
 *	Returns a chain of the child windows according to their stacking
 *	order.  The window ids are ordered from top to bottom.
 *
 * ------------------------------------------------------------------------
 */
static Blt_Chain *
GetChildren(Display *display, Window window)
{
    Blt_Chain *chainPtr;
    HWND hWnd;
    HWND parent;

    parent = Tk_GetHWND(window);
    chainPtr = Blt_ChainCreate();
    for (hWnd = GetTopWindow(parent); hWnd != NULL;
	hWnd = GetNextWindow(hWnd, GW_HWNDNEXT)) {
	Blt_ChainAppend(chainPtr, (ClientData)hWnd);
    }
    return chainPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * GetAdoptedWindowGeometry --
 *
 *	Computes the requested geometry of the container using the 
 *	size of adopted window as a reference.  
 *
 * Results:
 *	A standard Tcl result. 
 *
 * Side Effects:
 *	Sets a flag, indicating an error occurred.
 *
 *----------------------------------------------------------------------
 */
static int
GetAdoptedWindowGeometry(Tcl_Interp *interp, Container *cntrPtr)
{
    int x, y, width, height;
    int xOffset, yOffset;
    Window root, dummy;
    
    width = height = 1;
    xOffset = yOffset = 0;
    x = y = 0;
    if (cntrPtr->adopted != None) {
	HWND hWnd;
	RECT rect;

	hWnd = Tk_GetHWND(cntrPtr->adopted);
	if (GetWindowRect(hWnd, &rect)) {
	    x = rect.left;
	    y = rect.top;
	    width = rect.right - rect.left + 1;
	    height = rect.bottom - rect.top + 1;
	} else {
	    Tcl_AppendResult(interp, "can't get geometry for \"", 
		     NameOfId(cntrPtr->display, cntrPtr->adopted), "\"", 
		     (char *)NULL);
	    return TCL_ERROR;
	}
	root = RootWindow(cntrPtr->display, Tk_ScreenNumber(cntrPtr->tkwin));
	XTranslateCoordinates(cntrPtr->display, cntrPtr->adopted,
		      root, 0, 0, &xOffset, &yOffset, &dummy);
	cntrPtr->origX = xOffset;
	cntrPtr->origY = yOffset;
	cntrPtr->origWidth = width;
	cntrPtr->origHeight = height;
    } else {
	cntrPtr->origX = cntrPtr->origY = 0;
	cntrPtr->origWidth = cntrPtr->origHeight = 0;
    }
    cntrPtr->adoptedX = x;
    cntrPtr->adoptedY = y;
    cntrPtr->adoptedWidth = width;
    cntrPtr->adoptedHeight = height;
    return TCL_OK;
}

#endif /*WIN32*/

/*
 * ------------------------------------------------------------------------
 *
 *  MapTree --
 *
 *	Maps each window in the hierarchy.  This is needed because 
 *
 *  Results:
 *	None.
 *
 *  Side Effects:
 *	Each window in the hierarchy is mapped.
 *
 * ------------------------------------------------------------------------
 */
static void
MapTree(display, window)
    Display *display;
    Window window;
{
    Blt_Chain *chainPtr;

    XMapWindow(display, window);
    chainPtr = GetChildren(display, window);
    if (chainPtr != NULL) {
	Blt_ChainLink *linkPtr;
	Window child;

	for (linkPtr = Blt_ChainFirstLink(chainPtr); linkPtr != NULL;
	     linkPtr = Blt_ChainNextLink(linkPtr)) {
	    child = (Window)Blt_ChainGetValue(linkPtr);
	    MapTree(display, child);
	}
	Blt_ChainDestroy(chainPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * StringToXID --
 *
 *	Converts a string into an X window Id.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *---------------------------------------------------------------------- */
/*ARGSUSED*/
static int
StringToXID(clientData, interp, parent, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window parent;		/* Parent window */
    char *string;		/* String representation. */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset to field in structure */
{
    unsigned int flags = (int)clientData;
    Container *cntrPtr = (Container *)widgRec;
    Window *winPtr = (Window *) (widgRec + offset);
    Tk_Window tkAdopted;
    Window window;

    tkAdopted = NULL;
    window = None;
    if ((flags & SEARCH_TKWIN) && (string[0] == '.')) {
	Tk_Window tkwin;

	tkwin = Tk_NameToWindow(interp, string, Tk_MainWindow(interp));
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	if (!Tk_IsTopLevel(tkwin)) {
	    Tcl_AppendResult(interp, "can't reparent non-toplevel Tk windows",
			     (char *)NULL);
	    return TCL_ERROR;
	}
	tkAdopted = tkwin;
	Tk_MakeWindowExist(tkwin);
	window = Blt_GetRealWindowId(tkwin);
#ifndef WIN32
    } else if ((flags & SEARCH_XID) && (string[0] == '0') && 
	       (string[1] == 'x')) {
	int token;

	/* Hexidecimal string specifying the Window token. */
	if (Tcl_GetInt(interp, string, &token) != TCL_OK) {
	    return TCL_ERROR;
	}
	window = token;
    } else if ((string == NULL) || (string[0] == '\0')) {
	window = None;
    } else {
	SearchInfo search;

	memset(&search, 0, sizeof(search));
	if (flags & (SEARCH_NAME | SEARCH_CMD)) {
	    search.pattern = string;
	    if (flags & SEARCH_NAME) {
		search.proc = NameSearch;
	    } else if (flags & SEARCH_CMD) {
		search.proc = CmdSearch;
	    }
	    TestAndWaitForWindow(cntrPtr, &search);
	    if (search.nMatches > 1) {
		Tcl_AppendResult(interp, "more than one window matches \"", 
			 string, "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	}
	if (search.nMatches == 0) {
	    Tcl_AppendResult(interp, "can't find window from pattern \"", 
			     string, "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	window = search.window;
#endif /*WIN32*/
    }
    if (*winPtr != None) {
	Window root;

	root = RootWindow(cntrPtr->display, Tk_ScreenNumber(cntrPtr->tkwin));
	if (Blt_ReparentWindow(cntrPtr->display, *winPtr, root, 
		       cntrPtr->origX, cntrPtr->origY) 
	    != TCL_OK) {
	    Tcl_AppendResult(interp, "can't restore \"", 
			 NameOfId(cntrPtr->display, *winPtr), 
			"\" window to root", (char *)NULL);
	    return TCL_ERROR;
	}
	cntrPtr->flags &= ~CONTAINER_MAPPED;
	if (cntrPtr->tkAdopted == NULL) {
	    /* This wasn't a Tk window.  So deselect the event mask. */
	    XSelectInput(cntrPtr->display, *winPtr, 0);
	} else {
	    MapTree(cntrPtr->display, *winPtr);
	}
	XMoveResizeWindow(cntrPtr->display, *winPtr, cntrPtr->origX,
		  cntrPtr->origY, cntrPtr->origWidth, cntrPtr->origHeight);
    }
    cntrPtr->tkAdopted = tkAdopted;
    *winPtr = window;
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * XIDToString --
 *
 *	Converts the Tk window back to its string representation (i.e.
 *	its name).
 *
 * Results:
 *	The name of the window is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
XIDToString(clientData, parent, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window parent;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of field in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    Container *cntrPtr = (Container *) widgRec;
    Window window = *(Window *)(widgRec + offset);

    if (cntrPtr->tkAdopted != NULL) {
	return Tk_PathName(cntrPtr->tkAdopted);
    } 
    return NameOfId(cntrPtr->display, window);
}


/*
 *----------------------------------------------------------------------
 *
 * EventuallyRedraw --
 *
 *	Queues a request to redraw the widget at the next idle point.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets redisplayed.  Right now we don't do selective
 *	redisplays:  the whole window will be redrawn.
 *
 *----------------------------------------------------------------------
 */
static void
EventuallyRedraw(cntrPtr)
    Container *cntrPtr;
{
    if ((cntrPtr->tkwin != NULL) && !(cntrPtr->flags & CONTAINER_REDRAW)) {
	cntrPtr->flags |= CONTAINER_REDRAW;
	Tcl_DoWhenIdle(DisplayContainer, cntrPtr);
    }
}

/*
 * --------------------------------------------------------------
 *
 * AdoptedWindowEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on the encapsulated window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets resized or exposed, it's redisplayed.
 *
 * --------------------------------------------------------------
 */
static int
AdoptedWindowEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about the tab window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Container *cntrPtr = (Container *) clientData;

    if (eventPtr->xany.window != cntrPtr->adopted) {
	return 0;
    }
    if (eventPtr->type == DestroyNotify) {
	cntrPtr->adopted = None;
	EventuallyRedraw(cntrPtr);
    }
    return 1;
}

/*
 * --------------------------------------------------------------
 *
 * ContainerEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on container widgets.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 * --------------------------------------------------------------
 */
static void
ContainerEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Container *cntrPtr = clientData;

    switch (eventPtr->type) {
    case Expose:
	if (eventPtr->xexpose.count == 0) {
	    EventuallyRedraw(cntrPtr);
	}
	break;

    case FocusIn:
    case FocusOut:
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    if (eventPtr->type == FocusIn) {
		cntrPtr->flags |= CONTAINER_FOCUS;
	    } else {
		cntrPtr->flags &= ~CONTAINER_FOCUS;
	    }
	    EventuallyRedraw(cntrPtr);
	}
	break;

    case ConfigureNotify:
	EventuallyRedraw(cntrPtr);
	break;

    case DestroyNotify:
	if (cntrPtr->tkwin != NULL) {
	    cntrPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(cntrPtr->interp, cntrPtr->cmdToken);
	}
	if (cntrPtr->flags & CONTAINER_REDRAW) {
	    Tcl_CancelIdleCall(DisplayContainer, cntrPtr);
	}
	Tcl_EventuallyFree(cntrPtr, DestroyContainer);
	break;
    }
}

/*
 * --------------------------------------------------------------
 *
 * ToplevelEventProc --
 *
 *	Some applications assume that they are always a toplevel
 *	window and play tricks accordingly.  For example, Netscape
 *	positions menus in relation to the toplevel.  But if the
 *	container's toplevel is moved, this positioning is wrong.  
 *	So watch if the toplevel is moved.  
 *
 *	[This would be easier and cleaner if Tk toplevels weren't so
 *	botched by the addition of menubars.  It's not enough to
 *	track the )
 *
 * Results:
 *	None.
 *
 * --------------------------------------------------------------
 */
static void
ToplevelEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about the tab window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Container *cntrPtr = clientData;

    if ((cntrPtr->adopted != None) && (cntrPtr->tkwin != NULL) &&
	(eventPtr->type == ConfigureNotify)) {
	cntrPtr->flags |= CONTAINER_MOVE;
	EventuallyRedraw(cntrPtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyContainer --
 *
 * 	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 * 	to clean up the internal structure of the widget at a safe
 * 	time (when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Everything associated with the widget is freed up.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyContainer(dataPtr)
    DestroyData dataPtr;	/* Pointer to the widget record. */
{
    Container *cntrPtr = (Container *) dataPtr;

    if (cntrPtr->highlightGC != NULL) {
	Tk_FreeGC(cntrPtr->display, cntrPtr->highlightGC);
    }
    if (cntrPtr->flags & CONTAINER_INIT) {
	Tk_DeleteGenericHandler(AdoptedWindowEventProc, cntrPtr);
    }
    if (cntrPtr->tkToplevel != NULL) {
	Tk_DeleteEventHandler(cntrPtr->tkToplevel, StructureNotifyMask, 
		ToplevelEventProc, cntrPtr);
    }
    Tk_FreeOptions(configSpecs, (char *)cntrPtr, cntrPtr->display, 0);
    Blt_Free(cntrPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureContainer --
 *
 * 	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	the widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 * 	returned, then interp->result contains an error message.
 *
 * Side Effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for cntrPtr; old resources get freed, if there
 *	were any.  The widget is redisplayed.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureContainer(interp, cntrPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Interpreter to report errors. */
    Container *cntrPtr;		/* Information about widget; may or
			         * may not already have values for
			         * some fields. */
    int argc;
    char **argv;
    int flags;
{
    XGCValues gcValues;
    unsigned long gcMask;
    GC newGC;
    int width, height;

    if (Tk_ConfigureWidget(interp, cntrPtr->tkwin, configSpecs, argc, argv,
	    (char *)cntrPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    cntrPtr->inset = cntrPtr->borderWidth + cntrPtr->highlightWidth;
    if (Tk_WindowId(cntrPtr->tkwin) == None) {
	Tk_MakeWindowExist(cntrPtr->tkwin);
    }
    if (GetAdoptedWindowGeometry(interp, cntrPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_ConfigModified(configSpecs, "-window", "-name", "-command", 
			   (char *)NULL)) {
	cntrPtr->flags &= ~CONTAINER_MAPPED;
	if (cntrPtr->adopted != None) {
	    if (Blt_ReparentWindow(cntrPtr->display, cntrPtr->adopted,
		    Tk_WindowId(cntrPtr->tkwin), cntrPtr->inset,
		    cntrPtr->inset) != TCL_OK) {
		Tcl_AppendResult(interp, "can't adopt window \"", 
			 NameOfId(cntrPtr->display, cntrPtr->adopted), 
			 "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    XSelectInput(cntrPtr->display, cntrPtr->adopted, 
		 StructureNotifyMask);
	    if ((cntrPtr->flags & CONTAINER_INIT) == 0) {
		Tk_CreateGenericHandler(AdoptedWindowEventProc, cntrPtr);
		cntrPtr->flags |= CONTAINER_INIT;
	    }
	}
    }
    /* Add the designated inset to the requested dimensions. */
    width = cntrPtr->origWidth + 2 * cntrPtr->inset; 
    height = cntrPtr->origHeight + 2 * cntrPtr->inset;

    if (cntrPtr->reqWidth > 0) {
	width = cntrPtr->reqWidth;
    } 
    if (cntrPtr->reqHeight > 0) {
	height = cntrPtr->reqHeight;
    } 
    /* Set the requested width and height for the container. */
    if ((Tk_ReqWidth(cntrPtr->tkwin) != width) ||
	(Tk_ReqHeight(cntrPtr->tkwin) != height)) {
	Tk_GeometryRequest(cntrPtr->tkwin, width, height);
    }

    /*
     * GC for focus highlight.
     */
    gcMask = GCForeground;
    gcValues.foreground = cntrPtr->highlightColor->pixel;
    newGC = Tk_GetGC(cntrPtr->tkwin, gcMask, &gcValues);
    if (cntrPtr->highlightGC != NULL) {
	Tk_FreeGC(cntrPtr->display, cntrPtr->highlightGC);
    }
    cntrPtr->highlightGC = newGC;

    EventuallyRedraw(cntrPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ContainerInstCmdDeleteProc --
 *
 *	This procedure can be called if the window was destroyed
 *	(tkwin will be NULL) and the command was deleted
 *	automatically.  In this case, we need to do nothing.
 *
 *	Otherwise this routine was called because the command was
 *	deleted.  Then we need to clean-up and destroy the widget.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */
static void
ContainerInstCmdDeleteProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Container *cntrPtr = clientData;

    if (cntrPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = cntrPtr->tkwin;
	cntrPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
#ifdef ITCL_NAMESPACES
	Itk_SetWidgetCommand(tkwin, (Tcl_Command) NULL);
#endif /* ITCL_NAMESPACES */
    }
}

/*
 * ------------------------------------------------------------------------
 *
 * ContainerCmd --
 *
 * 	This procedure is invoked to process the Tcl command that
 * 	corresponds to a widget managed by this module. See the user
 * 	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	See the user documentation.
 *
 * -----------------------------------------------------------------------
 */
/* ARGSUSED */
static int
ContainerCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Container *cntrPtr;
    Tk_Window tkwin;
    unsigned int mask;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " pathName ?option value?...\"", (char *)NULL);
	return TCL_ERROR;
    }
    tkwin = Tk_MainWindow(interp);
    tkwin = Tk_CreateWindowFromPath(interp, tkwin, argv[1], (char *)NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    cntrPtr = Blt_Calloc(1, sizeof(Container));
    assert(cntrPtr);
    cntrPtr->tkwin = tkwin;
    cntrPtr->display = Tk_Display(tkwin);
    cntrPtr->interp = interp;
    cntrPtr->flags = 0;
    cntrPtr->timeout = SEARCH_INTERVAL;
    cntrPtr->borderWidth = cntrPtr->highlightWidth = 2;
    cntrPtr->relief = TK_RELIEF_SUNKEN;
    Tk_SetClass(tkwin, "Container");
#if (TK_MAJOR_VERSION > 4)
    Blt_SetWindowInstanceData(tkwin, cntrPtr);
#endif
    if (ConfigureContainer(interp, cntrPtr, argc - 2, argv + 2, 0) != TCL_OK) {
	Tk_DestroyWindow(cntrPtr->tkwin);
	return TCL_ERROR;
    }
    mask = (StructureNotifyMask | ExposureMask | FocusChangeMask);
    Tk_CreateEventHandler(tkwin, mask, ContainerEventProc, cntrPtr);
    cntrPtr->cmdToken = Tcl_CreateCommand(interp, argv[1], ContainerInstCmd,
	cntrPtr, ContainerInstCmdDeleteProc);
#ifdef ITCL_NAMESPACES
    Itk_SetWidgetCommand(cntrPtr->tkwin, cntrPtr->cmdToken);
#endif
    Tk_MakeWindowExist(tkwin);

    Tcl_SetResult(interp, Tk_PathName(cntrPtr->tkwin), TCL_VOLATILE);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * DisplayContainer --
 *
 * 	This procedure is invoked to display the widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 * 	The widget is redisplayed.
 *
 * ----------------------------------------------------------------------
 */
static void
DisplayContainer(clientData)
    ClientData clientData;	/* Information about widget. */
{
    Container *cntrPtr = clientData;
    Drawable drawable;
    int width, height;

    cntrPtr->flags &= ~CONTAINER_REDRAW;
    if (cntrPtr->tkwin == NULL) {
	return;			/* Window has been destroyed. */
    }
    if (!Tk_IsMapped(cntrPtr->tkwin)) {
	return;
    }
    drawable = Tk_WindowId(cntrPtr->tkwin);

#ifndef WIN32
    if (cntrPtr->tkToplevel == NULL) {
	Window window;
	Tk_Window tkToplevel;

	/* Create an event handler for the toplevel of the container. */
	tkToplevel = Blt_Toplevel(cntrPtr->tkwin);
	window = Blt_GetRealWindowId(tkToplevel);
	cntrPtr->tkToplevel = Tk_IdToWindow(cntrPtr->display, window); 
	if (cntrPtr->tkToplevel != NULL) {
	    Tk_CreateEventHandler(cntrPtr->tkToplevel, StructureNotifyMask, 
		ToplevelEventProc, cntrPtr);
	}
    }
#endif /* WIN32 */
    if (cntrPtr->adopted != None) {
#ifndef WIN32
	if (cntrPtr->flags & CONTAINER_MOVE) {
	    /* 
	     * Some applications like Netscape cache its location to
	     * position its popup menus. But when it's reparented, it
	     * thinks it's always at the same position.  It doesn't
	     * know when the container's moved.  The hack here is to
	     * force the application to update its coordinates by
	     * moving the adopted window (over by 1 pixel and
	     * then back in case the application is comparing the last
	     * location).  
	     */
	    XMoveWindow(cntrPtr->display, cntrPtr->adopted,
			cntrPtr->inset + 1, cntrPtr->inset + 1);
	    XMoveWindow(cntrPtr->display, cntrPtr->adopted,
			cntrPtr->inset, cntrPtr->inset);
	    cntrPtr->flags &= ~CONTAINER_MOVE;
	}
#endif /* WIN32 */
	/* Compute the available space inside the container. */
	width = Tk_Width(cntrPtr->tkwin) - (2 * cntrPtr->inset);
	height = Tk_Height(cntrPtr->tkwin) - (2 * cntrPtr->inset);

	if ((cntrPtr->adoptedX != cntrPtr->inset) || 
	    (cntrPtr->adoptedY != cntrPtr->inset) ||
	    (cntrPtr->adoptedWidth != width) || 
	    (cntrPtr->adoptedHeight != height)) {
	    /* Resize the window to the new size */
	    if (width < 1) {
		width = 1;
	    }
	    if (height < 1) {
		height = 1;
	    }
	    XMoveResizeWindow(cntrPtr->display, cntrPtr->adopted,
		cntrPtr->inset, cntrPtr->inset, width, height);
	    cntrPtr->adoptedWidth = width;
	    cntrPtr->adoptedHeight = height;
	    cntrPtr->adoptedX = cntrPtr->adoptedY = cntrPtr->inset;
	    if (cntrPtr->tkAdopted != NULL) {
		Tk_ResizeWindow(cntrPtr->tkAdopted, width, height);
	    }
	}
#ifndef WIN32
	if (!(cntrPtr->flags & CONTAINER_MAPPED)) {
	    XMapWindow(cntrPtr->display, cntrPtr->adopted);
	    cntrPtr->flags |= CONTAINER_MAPPED;
	}
#endif
	if (cntrPtr->borderWidth > 0) {
	    Blt_Draw3DRectangle(cntrPtr->tkwin, drawable, cntrPtr->border,
		cntrPtr->highlightWidth, cntrPtr->highlightWidth,
		Tk_Width(cntrPtr->tkwin) - 2 * cntrPtr->highlightWidth,
		Tk_Height(cntrPtr->tkwin) - 2 * cntrPtr->highlightWidth,
		cntrPtr->borderWidth, cntrPtr->relief);
	}
    } else {
	Blt_Fill3DRectangle(cntrPtr->tkwin, drawable, cntrPtr->border,
	    cntrPtr->highlightWidth, cntrPtr->highlightWidth,
	    Tk_Width(cntrPtr->tkwin) - 2 * cntrPtr->highlightWidth,
	    Tk_Height(cntrPtr->tkwin) - 2 * cntrPtr->highlightWidth,
	    cntrPtr->borderWidth, cntrPtr->relief);
    }

    /* Draw focus highlight ring. */
    if (cntrPtr->highlightWidth > 0) {
	XColor *color;
	GC gc;

	color = (cntrPtr->flags & CONTAINER_FOCUS)
	    ? cntrPtr->highlightColor : cntrPtr->highlightBgColor;
	gc = Tk_GCForColor(color, drawable);
	Tk_DrawFocusHighlight(cntrPtr->tkwin, gc, cntrPtr->highlightWidth,
	    drawable);
    }
}

#ifdef notdef
/*
 *----------------------------------------------------------------------
 *
 * SendOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SendOp(cntrPtr, interp, argc, argv)
    Container *cntrPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{

    if (cntrPtr->adopted != None) {
	XEvent event;
	char *p;
	KeySym symbol;
	int xid;
	Window window;

	if (Tcl_GetInt(interp, argv[2], &xid) != TCL_OK) {
	    return TCL_ERROR;
	}
	window = (Window)xid;
	event.xkey.type = KeyPress;
	event.xkey.serial = 0;
	event.xkey.display = cntrPtr->display;
	event.xkey.window = event.xkey.subwindow = window;
	event.xkey.time = CurrentTime;
	event.xkey.x = event.xkey.x = 100;
	event.xkey.root = 
	    RootWindow(cntrPtr->display, Tk_ScreenNumber(cntrPtr->tkwin));	
	event.xkey.x_root = Tk_X(cntrPtr->tkwin) + cntrPtr->inset;
	event.xkey.x_root = Tk_Y(cntrPtr->tkwin) + cntrPtr->inset;
	event.xkey.state = 0;
	event.xkey.same_screen = TRUE;
	
	for (p = argv[3]; *p != '\0'; p++) {
	    if (*p == '\r') {
		symbol = XStringToKeysym("Return");
	    } else if (*p == ' ') {
		symbol = XStringToKeysym("space");
	    } else {
		char save;

		save = *(p+1);
		*(p+1) = '\0';
		symbol = XStringToKeysym(p);
		*(p+1) = save;
	    }
	    event.xkey.keycode = XKeysymToKeycode(cntrPtr->display, symbol);
	    event.xkey.type = KeyPress;
	    if (!XSendEvent(cntrPtr->display, window, False, KeyPress, &event)) {
		fprintf(stderr, "send press event failed\n");
	    }
	    event.xkey.type = KeyRelease;
	    if (!XSendEvent(cntrPtr->display, window, False, KeyRelease, 
			    &event)) {
		fprintf(stderr, "send release event failed\n");
	    }
	}
    }
    return TCL_OK;
}
#endif

#ifndef WIN32
/*
 *----------------------------------------------------------------------
 *
 * FindOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
FindOp(cntrPtr, interp, argc, argv)
    Container *cntrPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Window root;
    SearchInfo search;

    memset(&search, 0, sizeof(search));
    search.pattern = argv[3];
    Tcl_DStringInit(&(search.dString));
    search.saveNames = TRUE;	/* Indicates to record all matching XIDs. */
    if (strcmp(argv[2], "-name") == 0) {
	search.proc = NameSearch;
    } else if (strcmp(argv[2], "-command") == 0) {
	search.proc = CmdSearch;
    } else {
	Tcl_AppendResult(interp, "missing \"-name\" or \"-command\" switch",
			 (char *)NULL);
	return TCL_ERROR;
    }
    root = RootWindow(cntrPtr->display, Tk_ScreenNumber(cntrPtr->tkwin));
    (*search.proc)(cntrPtr->display, root, &search);
    Tcl_DStringResult(interp, &search.dString);
    return TCL_OK;
}
#endif /*WIN32*/

/*
 *----------------------------------------------------------------------
 *
 * CgetOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOp(cntrPtr, interp, argc, argv)
    Container *cntrPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    return Tk_ConfigureValue(interp, cntrPtr->tkwin, configSpecs,
	(char *)cntrPtr, argv[2], 0);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 * 	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	the widget.
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side Effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for cntrPtr; old resources get freed, if there
 *	were any.  The widget is redisplayed.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOp(cntrPtr, interp, argc, argv)
    Container *cntrPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (argc == 2) {
	return Tk_ConfigureInfo(interp, cntrPtr->tkwin, configSpecs,
	    (char *)cntrPtr, (char *)NULL, 0);
    } else if (argc == 3) {
	return Tk_ConfigureInfo(interp, cntrPtr->tkwin, configSpecs,
	    (char *)cntrPtr, argv[2], 0);
    }
    if (ConfigureContainer(interp, cntrPtr, argc - 2, argv + 2,
	    TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    EventuallyRedraw(cntrPtr);
    return TCL_OK;
}

/*
 * --------------------------------------------------------------
 *
 * ContainerCmd --
 *
 * 	This procedure is invoked to process the "container" command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 * --------------------------------------------------------------
 */
static Blt_OpSpec opSpecs[] =
{
    {"cget", 2, (Blt_Op)CgetOp, 3, 3, "option",},
    {"configure", 2, (Blt_Op)ConfigureOp, 2, 0, "?option value?...",},
#ifndef WIN32
    {"find", 1, (Blt_Op)FindOp, 3, 4, "?-command|-name? pattern",},
#endif /*WIN32*/
#ifdef notdef
    {"send", 1, (Blt_Op)SendOp, 4, 4, "window string",},
#endif
};

static int nSpecs = sizeof(opSpecs) / sizeof(Blt_OpSpec);

static int
ContainerInstCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about the widget. */
    Tcl_Interp *interp;		/* Interpreter to report errors back to. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Vector of argument strings. */
{
    Blt_Op proc;
    Container *cntrPtr = clientData;
    int result;

    proc = Blt_GetOp(interp, nSpecs, opSpecs, BLT_OP_ARG1, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve(cntrPtr);
    result = (*proc)(cntrPtr, interp, argc, argv);
    Tcl_Release(cntrPtr);
    return result;
}

int
Blt_ContainerInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec =
    {
	"container", ContainerCmd,
    };
    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#endif /* NO_CONTAINER */
