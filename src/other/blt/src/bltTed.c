/*
 * bltTed.c --
 *
 *	This module implements an editor for the  table geometry
 *	manager in the BLT toolkit.
 *
 * Copyright 1995 by AT&T Bell Laboratories.
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the
 * names of AT&T Bell Laboratories any of their entities not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * AT&T disclaims all warranties with regard to this software, including
 * all implied warranties of merchantability and fitness.  In no event
 * shall AT&T be liable for any special, indirect or consequential
 * damages or any damages whatsoever resulting from loss of use, data
 * or profits, whether in an action of contract, negligence or other
 * tortuous action, arising out of or in connection with the use or
 * performance of this software.
 *
 * Table editor was created by George Howlett.
 */

#include "bltInt.h"

#include "bltTable.h"

extern Tk_CustomOption bltDistanceOption;
extern Tk_CustomOption bltDashesOption;

typedef struct TedStruct Ted;

#define TABLE_THREAD_KEY	"BLT Table Data"

typedef struct {
    Blt_HashTable tableTable;	/* Hash table of table structures keyed by 
				 * the address of the reference Tk window */
} TableData;


typedef struct {
    int flags;
    Tcl_Interp *interp;
    Tk_Window tkwin;		/* Entry window */
    Entry *entryPtr;		/* Entry it represents */
    Table *tablePtr;		/* Table where it can be found */
    Ted *tedPtr;		/* Table editor */
    int mapped;			/* Indicates if the debugging windows are
				 * mapped */
} EntryRep;


typedef struct {
    Tk_Font font;
    XColor *widgetColor;
    XColor *cntlColor;
    XColor *normalFg, *normalBg;
    XColor *activeFg, *activeBg;

    Tk_Cursor cursor;		/* Cursor to display inside of this window */
    Pixmap stipple;

    GC drawGC;			/* GC to draw grid, outlines */
    GC fillGC;			/* GC to fill entry area */
    GC widgetFillGC;		/* GC to fill widget area */
    GC cntlGC;			/* GC to fill rectangles */

} EntryAttributes;

typedef struct {
    int count;
    XRectangle *array;
} Rectangles;

struct TedStruct {
    int gridLineWidth;		/* Width of grid lines */
    int buttonHeight;		/* Height of row/column buttons */
    int cavityPad;		/* Extra padding to add to entry cavity */
    int minSize;		/* Minimum size for partitions */

    EditorDrawProc *drawProc;
    EditorDestroyProc *destroyProc;

    Display *display;
    Tk_Font font;
    Table *tablePtr;		/* Pointer to table being debugged */
    Tcl_Interp *interp;
    int flags;
    Tk_Window tkwin;		/* Grid window */
    Tk_Window input;		/* InputOnly window to receive events */
    int inputIsSibling;

    /* Form the grid */
    XSegment *segArr;
    int nSegs;
    XRectangle *padRectArr;
    int nPadRects;
    XRectangle *widgetPadRectArr;
    int nWidgetPadRects;

    XRectangle *cntlRectArr;
    int nCntlRects;

    XRectangle *rectArr;
    int nRects;

    XRectangle activeRectArr[5];
    int spanActive;

    GC rectGC;			/* GC to fill rectangles */
    GC drawGC;			/* GC to draw grid, outlines */
    GC fillGC;			/* GC to fill window */
    GC spanGC;			/* GC to fill spans */
    GC padRectGC;		/* GC to draw padding  */

    Tk_3DBorder border;		/* Border to use with buttons */
    int relief;
    int borderWidth;		/* Border width of buttons */
    XColor *normalBg;
    XColor *padColor;
    XColor *gridColor;
    XColor *buttonColor;
    XColor *spanColor;

    Pixmap padStipple;
    Pixmap spanStipple;
    Blt_Dashes dashes;
    char *fileName;		/* If non-NULL, indicates name of file
				 * to write final table output to */
    int mapped;			/* Indicates if the debugging windows are
				 * mapped */
    int gripSize;
    int doubleBuffer;
    Tk_Cursor cursor;
    Blt_Chain *chainPtr;
    int nextWindowId;

    EntryAttributes attributes;	/* Entry attributes */
};

#define REDRAW_PENDING	(1<<0)	/* A DoWhenIdle handler has already
				 * been queued to redraw the window */
#define LAYOUT_PENDING	(1<<1)

/*  
 * 
 *
 *	|Cavity|1|2|
 *
 *
 */
#define DEF_ENTRY_ACTIVE_BG_MONO	RGB_BLACK
#define DEF_ENTRY_ACTIVE_FG_MONO	RGB_WHITE
#define DEF_ENTRY_ACTIVE_BACKGROUND	RGB_BLACK
#define DEF_ENTRY_ACTIVE_FOREGROUND	RGB_WHITE
#define DEF_ENTRY_CURSOR		(char *)NULL
#define DEF_ENTRY_FONT	 	"*-Courier-Bold-R-Normal-*-100-*"
#define DEF_ENTRY_NORMAL_BACKGROUND	RGB_BLUE
#define DEF_ENTRY_NORMAL_BG_MONO	RGB_BLACK
#define DEF_ENTRY_NORMAL_FOREGROUND	RGB_WHITE
#define DEF_ENTRY_NORMAL_FG_MONO	RGB_WHITE
#define DEF_ENTRY_WIDGET_BACKGROUND	RGB_GREEN
#define DEF_ENTRY_CONTROL_BACKGROUND	RGB_YELLOW
#define DEF_ENTRY_WIDGET_BG_MONO	RGB_BLACK
#define DEF_ENTRY_STIPPLE		"gray50"
#define DEF_GRID_BACKGROUND		RGB_WHITE
#define DEF_GRID_BG_MONO		RGB_WHITE
#define DEF_GRID_CURSOR			"crosshair"
#define DEF_GRID_DASHES			(char *)NULL
#define DEF_GRID_FOREGROUND		RGB_BLACK
#define DEF_GRID_FG_MONO		RGB_BLACK
#define DEF_GRID_FONT			"*-Courier-Bold-R-Normal-*-100-*"
#define DEF_GRID_LINE_WIDTH		"1"
#define DEF_GRID_PAD_COLOR		RGB_RED
#define DEF_GRID_PAD_MONO		RGB_BLACK
#define DEF_GRID_PAD_STIPPLE		"gray25"
#define DEF_GRID_PAD_CAVITY		"0"
#define DEF_GRID_PAD_MIN		"8"
#define DEF_ROWCOL_BACKGROUND		RGB_RED
#define DEF_ROWCOL_BG_MONO		RGB_BLACK
#define DEF_ROWCOL_BORDER_COLOR		RGB_RED
#define DEF_ROWCOL_BORDER_MONO		RGB_BLACK
#define DEF_ROWCOL_BORDERWIDTH		"2"
#define DEF_ROWCOL_HEIGHT		"8"
#define DEF_ROWCOL_RELIEF		"raised"
#define DEF_SPAN_STIPPLE		"gray50"
#define DEF_SPAN_COLOR			RGB_BLACK
#define DEF_SPAN_MONO			RGB_BLACK
#define DEF_SPAN_GRIP_SIZE		"5"
#define DEF_GRID_DOUBLE_BUFFER		"1"

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_BORDER, "-bg", "tedBorder", (char *)NULL,
	DEF_ROWCOL_BORDER_COLOR, Tk_Offset(Ted, border), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-bg", "tedBorder", (char *)NULL,
	DEF_ROWCOL_BORDER_MONO, Tk_Offset(Ted, border), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-background", "tedBackground", (char *)NULL,
	DEF_GRID_BACKGROUND, Tk_Offset(Ted, normalBg), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-background", "tedBackground", (char *)NULL,
	DEF_GRID_BG_MONO, Tk_Offset(Ted, normalBg), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CURSOR, "-cursor", "cursor", (char *)NULL,
	DEF_GRID_CURSOR, Tk_Offset(Ted, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-gridcolor", "gridColor", (char *)NULL,
	DEF_GRID_FOREGROUND, Tk_Offset(Ted, gridColor), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-gridcolor", "gridColor", (char *)NULL,
	DEF_GRID_FG_MONO, Tk_Offset(Ted, gridColor), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-buttoncolor", "buttonColor", (char *)NULL,
	DEF_ROWCOL_BACKGROUND, Tk_Offset(Ted, buttonColor), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-buttoncolor", "buttonColor", (char *)NULL,
	DEF_ROWCOL_BG_MONO, Tk_Offset(Ted, buttonColor), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-padcolor", "padColor", (char *)NULL,
	DEF_GRID_PAD_COLOR, Tk_Offset(Ted, padColor), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-padcolor", "padColor", (char *)NULL,
	DEF_GRID_PAD_MONO, Tk_Offset(Ted, padColor), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BITMAP, "-padstipple", "padStipple", (char *)NULL,
	DEF_GRID_PAD_STIPPLE, Tk_Offset(Ted, padStipple), TK_CONFIG_NULL_OK},
    {TK_CONFIG_FONT, "-font", "font", (char *)NULL,
	DEF_GRID_FONT, Tk_Offset(Ted, font), 0},
    {TK_CONFIG_CUSTOM, "-gridlinewidth", "gridLineWidth", (char *)NULL,
	DEF_GRID_LINE_WIDTH, Tk_Offset(Ted, gridLineWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-buttonheight", "buttonHeight", (char *)NULL,
	DEF_ROWCOL_HEIGHT, Tk_Offset(Ted, buttonHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-cavitypad", "cavityPad", (char *)NULL,
	DEF_GRID_PAD_CAVITY, Tk_Offset(Ted, cavityPad),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-minsize", "minSize", (char *)NULL,
	DEF_GRID_PAD_MIN, Tk_Offset(Ted, minSize),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-dashes", "dashes", (char *)NULL,
	DEF_GRID_DASHES, Tk_Offset(Ted, dashes),
	TK_CONFIG_NULL_OK, &bltDashesOption},
    {TK_CONFIG_RELIEF, "-relief", "relief", (char *)NULL,
	DEF_ROWCOL_RELIEF, Tk_Offset(Ted, relief), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-borderwidth", "borderWidth", (char *)NULL,
	DEF_ROWCOL_BORDERWIDTH, Tk_Offset(Ted, borderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CURSOR, "-entrycursor", "entryCursor", (char *)NULL,
	DEF_ENTRY_CURSOR, Tk_Offset(Ted, attributes.cursor),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_FONT, "-entryfont", "entryFont", (char *)NULL,
	DEF_ENTRY_FONT, Tk_Offset(Ted, attributes.font), 0},
    {TK_CONFIG_BITMAP, "-entrystipple", "entryStipple", (char *)NULL,
	DEF_ENTRY_STIPPLE, Tk_Offset(Ted, attributes.stipple),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-widgetbackground", "widgetBackground", (char *)NULL,
	DEF_ENTRY_WIDGET_BACKGROUND, Tk_Offset(Ted, attributes.widgetColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-widgetbackground", "widgetBackground", (char *)NULL,
	DEF_ENTRY_WIDGET_BG_MONO, Tk_Offset(Ted, attributes.widgetColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-controlbackground", "controlBackground", (char *)NULL,
	DEF_ENTRY_CONTROL_BACKGROUND, Tk_Offset(Ted, attributes.cntlColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-controlbackground", "controlBackground", (char *)NULL,
	DEF_ENTRY_WIDGET_BG_MONO, Tk_Offset(Ted, attributes.cntlColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-entrybackground", "entryBackground", (char *)NULL,
	DEF_ENTRY_NORMAL_BACKGROUND, Tk_Offset(Ted, attributes.normalBg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-entrybackground", "entryBackground", (char *)NULL,
	DEF_ENTRY_NORMAL_BG_MONO, Tk_Offset(Ted, attributes.normalBg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-entryactivebackground", "entryActiveBackground", (char *)NULL,
	DEF_ENTRY_ACTIVE_BACKGROUND, Tk_Offset(Ted, attributes.activeBg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-entryactivebackground", "entryActiveBackground", (char *)NULL,
	DEF_ENTRY_ACTIVE_BG_MONO, Tk_Offset(Ted, attributes.activeBg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-entryactiveforeground", "entryActiveForeground", (char *)NULL,
	DEF_ENTRY_ACTIVE_FOREGROUND, Tk_Offset(Ted, attributes.activeFg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-entryactiveforeground", "entryActiveForeground", (char *)NULL,
	DEF_ENTRY_ACTIVE_FG_MONO, Tk_Offset(Ted, attributes.activeFg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-entryforeground", "entryForeground", (char *)NULL,
	DEF_ENTRY_NORMAL_FOREGROUND, Tk_Offset(Ted, attributes.normalFg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-entryforeground", "entryForeground", (char *)NULL,
	DEF_ENTRY_NORMAL_FG_MONO, Tk_Offset(Ted, attributes.normalFg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-spancolor", "spanColor", (char *)NULL,
	DEF_SPAN_COLOR, Tk_Offset(Ted, spanColor), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-spancolor", "spanColor", (char *)NULL,
	DEF_SPAN_MONO, Tk_Offset(Ted, spanColor), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BITMAP, "-spanstipple", "spanStipple", (char *)NULL,
	DEF_SPAN_STIPPLE, Tk_Offset(Ted, spanStipple), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-gripsize", "gripSize", (char *)NULL,
	DEF_SPAN_GRIP_SIZE, Tk_Offset(Ted, gripSize),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_BOOLEAN, "-dbl", "doubleBuffer", (char *)NULL,
	DEF_GRID_DOUBLE_BUFFER, Tk_Offset(Ted, doubleBuffer),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};


static void DrawEditor _ANSI_ARGS_((Editor *editor));
static void DestroyEditor _ANSI_ARGS_((DestroyData destroyData));
static void DisplayTed _ANSI_ARGS_((ClientData clientData));
static void DestroyTed _ANSI_ARGS_((DestroyData destroyData));
static void DisplayEntry _ANSI_ARGS_((ClientData clientData));
static void DestroyEntry _ANSI_ARGS_((DestroyData destoryData));

static Tcl_CmdProc TedCmd;
static Tk_EventProc EntryEventProc;
static Tk_EventProc TedEventProc;

/*
 *----------------------------------------------------------------------
 *
 * EventuallyRedraw --
 *
 *	Queues a request to redraw the text window at the next idle
 *	point.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets redisplayed.  Right now we don't do selective
 *	redisplays:  the whole window will be redrawn.  This doesn't
 *	seem to hurt performance noticeably, but if it does then this
 *	could be changed.
 *
 *----------------------------------------------------------------------
 */
static void
EventuallyRedraw(tedPtr)
    Ted *tedPtr;		/* Information about editor. */
{
    if ((tedPtr->tkwin != NULL) && !(tedPtr->flags & REDRAW_PENDING)) {
	tedPtr->flags |= REDRAW_PENDING;
	Tcl_DoWhenIdle(DisplayTed, tedPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * EventuallyRedraw --
 *
 *	Queues a request to redraw the text window at the next idle
 *	point.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets redisplayed.  Right now we don't do selective
 *	redisplays:  the whole window will be redrawn.  This doesn't
 *	seem to hurt performance noticeably, but if it does then this
 *	could be changed.
 *
 *----------------------------------------------------------------------
 */
static void
EventuallyRedrawEntry(repPtr)
    EntryRep *repPtr;		/* Information about editor. */
{
    if ((repPtr->tkwin != NULL) && !(repPtr->flags & REDRAW_PENDING)) {
	repPtr->flags |= REDRAW_PENDING;
	Tcl_DoWhenIdle(DisplayEntry, repPtr);
    }
}

/*
 * --------------------------------------------------------------
 *
 * EntryEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on the editing grid for the table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 * --------------------------------------------------------------
 */
static void
EntryEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    EntryRep *repPtr = (EntryRep *) clientData;

    if (eventPtr->type == ConfigureNotify) {
	EventuallyRedrawEntry(repPtr);
    } else if (eventPtr->type == Expose) {
	if (eventPtr->xexpose.count == 0) {
	    EventuallyRedrawEntry(repPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	repPtr->tkwin = NULL;
	if (repPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayEntry, repPtr);
	}
	Tcl_EventuallyFree(repPtr, DestroyEntry);
    }
}

/*
 * --------------------------------------------------------------
 *
 * TedEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on the editing grid for the table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 * --------------------------------------------------------------
 */
static void
TedEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Ted *tedPtr = (Ted *) clientData;

    if (eventPtr->type == ConfigureNotify) {
	EventuallyRedraw(tedPtr);
    } else if (eventPtr->type == Expose) {
	if (eventPtr->xexpose.count == 0) {
	    EventuallyRedraw(tedPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	tedPtr->tkwin = NULL;
	if (tedPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayTed, tedPtr);
	}
	Tcl_EventuallyFree(tedPtr, DestroyTed);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * CreateGrid --
 *
 * ----------------------------------------------------------------------------
 */
static int
CreateGrid(tedPtr)
    Ted *tedPtr;
{
    Tcl_Interp *interp;
    Tk_Window tkwin;
    Tk_Window master;
    /*
     * Create a sibling window to cover the master window. It will
     * be stacked just above the master window.
     */
    interp = tedPtr->tablePtr->interp;
    master = tedPtr->tablePtr->tkwin;
    tkwin = Tk_CreateWindow(interp, master, "ted_%output%", (char *)NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    Tk_SetClass(tkwin, "BltTed");
    Tk_CreateEventHandler(tkwin, ExposureMask | StructureNotifyMask,
	TedEventProc, tedPtr);
    Tk_MoveResizeWindow(tkwin, 0, 0, Tk_Width(master), Tk_Height(master));
    Tk_RestackWindow(tkwin, Below, (Tk_Window)NULL);
    Tk_MapWindow(tkwin);
    tedPtr->tkwin = tkwin;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * CreateEventWindow --
 *
 * ----------------------------------------------------------------------------
 */
static int
CreateEventWindow(tedPtr)
    Ted *tedPtr;
{
    Tcl_Interp *interp;
    Tk_Window tkwin;
    Tk_Window master;
    Tk_Window parent;

    interp = tedPtr->tablePtr->interp;
    master = tedPtr->tablePtr->tkwin;
    /*
     * Create an InputOnly window which sits above the table to
     * collect and dispatch user events.
     */
    if (Tk_IsTopLevel(master)) {
	/*
	 * If master is a top-level window, it's also the parent of
	 * the widgets (it can't have a sibling).
	 * In this case, the InputOnly window is a child of the
	 * master instead of a sibling.
	 */
	parent = master;
	tkwin = Tk_CreateWindow(interp, parent, "ted_%input%", (char *)NULL);
	if (tkwin != NULL) {
	    Tk_ResizeWindow(tkwin, Tk_Width(parent), Tk_Height(parent));
	}
	tedPtr->inputIsSibling = 0;
    } else {
	char *namePtr;		/* Name of InputOnly window. */

	parent = Tk_Parent(master);
	namePtr = Blt_Malloc(strlen(Tk_Name(master)) + 5);
	sprintf(namePtr, "ted_%s", Tk_Name(master));
	tkwin = Tk_CreateWindow(interp, parent, namePtr, (char *)NULL);
	Blt_Free(namePtr);
	if (tkwin != NULL) {
	    Tk_MoveResizeWindow(tkwin, Tk_X(master), Tk_Y(master),
		Tk_Width(master), Tk_Height(master));
	}
	tedPtr->inputIsSibling = 1;
    }
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    Blt_MakeTransparentWindowExist(tkwin, Tk_WindowId(parent), TRUE);
    Tk_RestackWindow(tkwin, Above, (Tk_Window)NULL);
    Tk_MapWindow(tkwin);
    tedPtr->input = tkwin;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * CreateEntry --
 *
 * ----------------------------------------------------------------------------
 */
static int
CreateEntry(tedPtr, entryPtr)
    Ted *tedPtr;
    Entry *entryPtr;
{
    Tk_Window tkwin, master;
    char string[200];
    EntryRep *repPtr;
    Blt_ChainLink *linkPtr;

    repPtr = Blt_Calloc(1, sizeof(EntryRep));
    assert(repPtr);
    repPtr->tablePtr = tedPtr->tablePtr;
    repPtr->tedPtr = tedPtr;
    repPtr->interp = tedPtr->interp;
    repPtr->entryPtr = entryPtr;
    repPtr->mapped = 0;

    /*
     * Create a sibling window to cover the master window. It will
     * be stacked just above the master window.
     */

    master = tedPtr->tablePtr->tkwin;
    sprintf(string, "bltTed%d", tedPtr->nextWindowId);
    tedPtr->nextWindowId++;
    tkwin = Tk_CreateWindow(tedPtr->interp, master, string, (char *)NULL);
    if (tkwin == NULL) {
	Blt_Free(repPtr);
	return TCL_ERROR;
    }
    Tk_SetClass(tkwin, "BltTed");
    Tk_CreateEventHandler(tkwin, ExposureMask | StructureNotifyMask,
	EntryEventProc, repPtr);
    repPtr->tkwin = tkwin;
    linkPtr = Blt_ChainNewLink();
    Blt_ChainSetValue(linkPtr, repPtr);
    Blt_ChainLinkAfter(tedPtr->chainPtr, linkPtr, (Blt_ChainLink *)NULL);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * DestroyEntry --
 *
 * ----------------------------------------------------------------------------
 */
static void
DestroyEntry(data)
    DestroyData data;
{
    EntryRep *repPtr = (EntryRep *)data;
    Blt_ChainLink *linkPtr;
    Entry *entryPtr;

    for (linkPtr = Blt_ChainFirstLink(repPtr->tedPtr->chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	entryPtr = Blt_ChainGetValue(linkPtr);
	if (entryPtr == repPtr->entryPtr) {
	    Blt_ChainDeleteLink(repPtr->tedPtr->chainPtr, linkPtr);
	    Blt_Free(repPtr);
	    return;
	}
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * DisplayEntry --
 *
 * ----------------------------------------------------------------------------
 */
static void
DisplayEntry(clientData)
    ClientData clientData;
{
    EntryRep *repPtr = (EntryRep *) clientData;
    Ted *tedPtr;
    Entry *entryPtr;
    Tk_Window tkwin;
    int x, y, width, height;

    repPtr->flags &= ~REDRAW_PENDING;
    if ((repPtr->tkwin == NULL) || (repPtr->entryPtr == NULL)) {
	return;
    }
    if (!Tk_IsMapped(repPtr->tkwin)) {
	return;
    }
    tedPtr = repPtr->tedPtr;
    entryPtr = repPtr->entryPtr;
    tkwin = repPtr->tkwin;

    /*
     * Check if the entry size and position.
     * Move and resize the window accordingly.
     */
    x = Tk_X(entryPtr->tkwin) - (entryPtr->padLeft + tedPtr->cavityPad);
    y = Tk_Y(entryPtr->tkwin) - (entryPtr->padTop + tedPtr->cavityPad);
    width = Tk_Width(entryPtr->tkwin) + PADDING(entryPtr->padX) +
	(2 * tedPtr->cavityPad);
    height = Tk_Height(entryPtr->tkwin) + PADDING(entryPtr->padY) +
	(2 * tedPtr->cavityPad);


    if ((Tk_X(tkwin) != x) || (Tk_Y(tkwin) != y) || 
	(Tk_Width(tkwin) != width) || (Tk_Height(tkwin) != height)) {
	Tk_MoveResizeWindow(tkwin, x, y, width, height);
	Tk_RestackWindow(tkwin, Above, (Tk_Window)NULL);
    }
    /* Clear the background of the entry */

    XFillRectangle(Tk_Display(tkwin), Tk_WindowId(tkwin),
	tedPtr->attributes.fillGC, 0, 0, width, height);

    /* Draw the window */

    x = entryPtr->padLeft + tedPtr->cavityPad;
    y = entryPtr->padTop + tedPtr->cavityPad;

    XFillRectangle(Tk_Display(tkwin), Tk_WindowId(tkwin),
	tedPtr->attributes.widgetFillGC, x, y, Tk_Width(entryPtr->tkwin),
	Tk_Height(entryPtr->tkwin));
    XDrawRectangle(Tk_Display(tkwin), Tk_WindowId(tkwin),
	tedPtr->attributes.drawGC, x, y, Tk_Width(entryPtr->tkwin),
	Tk_Height(entryPtr->tkwin));
}

/*
 * ----------------------------------------------------------------------------
 *
 * FindEditor --
 *
 *	Searches for a table associated with the window given by its
 *	pathname.  This window represents the master window of the table.
 *
 *	Errors may occur because
 *	  1) pathName does not represent a valid Tk window or
 *	  2) the window is not associated with any table as its master.
 *
 * Results:
 *	If a table entry exists, a pointer to the Table structure is
 *	returned. Otherwise NULL is returned.
 *
 * ----------------------------------------------------------------------------
 */
static Ted *
FindEditor(clientData, interp, pathName)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;		/* Interpreter to report errors back to */
    char *pathName;		/* Path name of the master window */
{
    Table *tablePtr;

    if (Blt_GetTable(clientData, interp, pathName, &tablePtr) != TCL_OK) {
	return NULL;
    }
    if (tablePtr->editPtr == NULL) {
	Tcl_AppendResult(interp, "no editor exists for table \"",
	    Tk_PathName(tablePtr->tkwin), "\"", (char *)NULL);
	return NULL;
    }
    return (Ted *) tablePtr->editPtr;
}

/*
 * ----------------------------------------------------------------------------
 *
 * CreateTed --
 *
 * ----------------------------------------------------------------------------
 */
static Ted *
CreateTed(tablePtr, interp)
    Table *tablePtr;
    Tcl_Interp *interp;
{
    Ted *tedPtr;

    tedPtr = Blt_Calloc(1, sizeof(Ted));
    assert(tedPtr);
    tedPtr->nextWindowId = 0;
    tedPtr->interp = interp;
    tedPtr->tablePtr = tablePtr;
    tedPtr->gridLineWidth = 1;
    tedPtr->buttonHeight = 0;
    tedPtr->cavityPad = 0;
    tedPtr->minSize = 3;
    tedPtr->gripSize = 5;
    tedPtr->drawProc = DrawEditor;
    tedPtr->destroyProc = DestroyEditor;
    tedPtr->display = Tk_Display(tablePtr->tkwin);
    tedPtr->relief = TK_RELIEF_RAISED;
    tedPtr->borderWidth = 2;
    tedPtr->doubleBuffer = 1;
    tedPtr->chainPtr = Blt_ChainCreate();
    /* Create the grid window */

    if (CreateGrid(tedPtr) != TCL_OK) {
	return NULL;
    }
    /* Create an InputOnly window to collect user events */
    if (CreateEventWindow(tedPtr) != TCL_OK) {
	return NULL;
    }
    tablePtr->editPtr = (Editor *)tedPtr;
    return tedPtr;
}

/*
 * ----------------------------------------------------------------------------
 *
 * DestroyTed --
 *
 * ----------------------------------------------------------------------------
 */
static void
DestroyTed(freeProcData)
    DestroyData freeProcData;
{
    Ted *tedPtr = (Ted *) freeProcData;

    if (tedPtr->rectArr != NULL) {
	Blt_Free(tedPtr->rectArr);
    }
    if (tedPtr->segArr != NULL) {
	Blt_Free(tedPtr->segArr);
    }
    if (tedPtr->fillGC != NULL) {
	Tk_FreeGC(tedPtr->display, tedPtr->fillGC);
    }
    if (tedPtr->drawGC != NULL) {
	Tk_FreeGC(tedPtr->display, tedPtr->drawGC);
    }
    if (tedPtr->rectGC != NULL) {
	Tk_FreeGC(tedPtr->display, tedPtr->rectGC);
    }
    if (tedPtr->padRectGC != NULL) {
	Tk_FreeGC(tedPtr->display, tedPtr->padRectGC);
    }
    /* Is this save ? */
    tedPtr->tablePtr->editPtr = NULL;
    Blt_Free(tedPtr);

}

/*
 * ----------------------------------------------------------------------------
 *
 * ConfigureTed --
 *
 *	This procedure is called to process an argv/argc list in order to
 *	configure the table geometry manager.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Table configuration options (padx, pady, rows, columns, etc) get
 *	set.   The table is recalculated and arranged at the next idle
 *	point.
 *
 * ----------------------------------------------------------------------------
 */
static int
ConfigureTed(tedPtr, argc, argv, flags)
    Ted *tedPtr;
    int argc;
    char **argv;		/* Option-value pairs */
    int flags;
{
    XGCValues gcValues;
    GC newGC;
    unsigned long gcMask;

    if (Tk_ConfigureWidget(tedPtr->interp, tedPtr->tkwin, configSpecs,
	    argc, argv, (char *)tedPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    /* GC for filling background of edit window */

    gcMask = GCForeground;
    gcValues.foreground = tedPtr->normalBg->pixel;
    newGC = Tk_GetGC(tedPtr->tkwin, gcMask, &gcValues);
    if (tedPtr->fillGC != NULL) {
	Tk_FreeGC(tedPtr->display, tedPtr->fillGC);
    }
    tedPtr->fillGC = newGC;

    /* GC for drawing grid lines */

    gcMask = (GCForeground | GCBackground | GCLineWidth | GCLineStyle |
	GCCapStyle | GCJoinStyle | GCFont);
    gcValues.font = Tk_FontId(tedPtr->font);
    gcValues.foreground = tedPtr->gridColor->pixel;
    gcValues.background = tedPtr->normalBg->pixel;
    gcValues.line_width = LineWidth(tedPtr->gridLineWidth);
    gcValues.cap_style = CapRound;
    gcValues.join_style = JoinRound;
    gcValues.line_style = LineSolid;
    if (LineIsDashed(tedPtr->dashes)) {
	gcValues.line_style = LineOnOffDash;
    }
    newGC = Blt_GetPrivateGC(tedPtr->tkwin, gcMask, &gcValues);
    if (tedPtr->drawGC != NULL) {
	Blt_FreePrivateGC(tedPtr->display, tedPtr->drawGC);
    }
    if (LineIsDashed(tedPtr->dashes)) {
	XSetDashes(tedPtr->display, newGC, 0, 
		   (CONST char *)tedPtr->dashes.values,
		   strlen((char *)tedPtr->dashes.values));
    }
    tedPtr->drawGC = newGC;

    /* GC for button rectangles */

    gcMask = GCForeground;
    gcValues.foreground = tedPtr->buttonColor->pixel;
    newGC = Tk_GetGC(tedPtr->tkwin, gcMask, &gcValues);
    if (tedPtr->rectGC != NULL) {
	Tk_FreeGC(tedPtr->display, tedPtr->rectGC);
    }
    tedPtr->rectGC = newGC;

    /* GC for button rectangles */

    gcMask = GCForeground;
    gcValues.foreground = tedPtr->padColor->pixel;
    if (tedPtr->padStipple != None) {
	gcMask |= GCStipple | GCFillStyle;
	gcValues.stipple = tedPtr->padStipple;
	gcValues.fill_style = FillStippled;
    }
    newGC = Tk_GetGC(tedPtr->tkwin, gcMask, &gcValues);
    if (tedPtr->padRectGC != NULL) {
	Tk_FreeGC(tedPtr->display, tedPtr->padRectGC);
    }
    tedPtr->padRectGC = newGC;

    /* GC for filling entrys */

    gcMask = GCForeground;
    gcValues.foreground = tedPtr->attributes.normalBg->pixel;
    if (tedPtr->attributes.stipple != None) {
	gcMask |= GCStipple | GCFillStyle;
	gcValues.stipple = tedPtr->attributes.stipple;
	gcValues.fill_style = FillStippled;
    }
    newGC = Tk_GetGC(tedPtr->tkwin, gcMask, &gcValues);
    if (tedPtr->attributes.fillGC != NULL) {
	Tk_FreeGC(tedPtr->display, tedPtr->attributes.fillGC);
    }
    tedPtr->attributes.fillGC = newGC;

    /* GC for drawing entrys */

    gcMask = GCForeground | GCBackground | GCFont;
    gcValues.foreground = tedPtr->attributes.normalFg->pixel;
    gcValues.background = tedPtr->attributes.normalBg->pixel;
    gcValues.font = Tk_FontId(tedPtr->attributes.font);
    newGC = Tk_GetGC(tedPtr->tkwin, gcMask, &gcValues);
    if (tedPtr->attributes.drawGC != NULL) {
	Blt_FreePrivateGC(tedPtr->display, tedPtr->attributes.drawGC);
    }
    tedPtr->attributes.drawGC = newGC;

    /* GC for filling widget rectangles */

    gcMask = GCForeground;
    gcValues.foreground = tedPtr->attributes.widgetColor->pixel;
    newGC = Tk_GetGC(tedPtr->tkwin, gcMask, &gcValues);
    if (tedPtr->attributes.widgetFillGC != NULL) {
	Tk_FreeGC(tedPtr->display, tedPtr->attributes.widgetFillGC);
    }
    tedPtr->attributes.widgetFillGC = newGC;

    gcMask = GCForeground;
    gcValues.foreground = tedPtr->attributes.cntlColor->pixel;
    newGC = Tk_GetGC(tedPtr->tkwin, gcMask, &gcValues);
    if (tedPtr->attributes.cntlGC != NULL) {
	Tk_FreeGC(tedPtr->display, tedPtr->attributes.cntlGC);
    }
    tedPtr->attributes.cntlGC = newGC;

    /* GC for filling span rectangle */

    gcMask = GCForeground;
    gcValues.foreground = tedPtr->spanColor->pixel;
    if (tedPtr->spanStipple != None) {
	gcMask |= GCStipple | GCFillStyle;
	gcValues.stipple = tedPtr->spanStipple;
	gcValues.fill_style = FillStippled;
    }
    newGC = Tk_GetGC(tedPtr->tkwin, gcMask, &gcValues);
    if (tedPtr->spanGC != NULL) {
	Tk_FreeGC(tedPtr->display, tedPtr->spanGC);
    }
    tedPtr->spanGC = newGC;

    /* Define cursor for grid events */
    if (tedPtr->cursor != None) {
	Tk_DefineCursor(tedPtr->input, tedPtr->cursor);
    } else {
	Tk_UndefineCursor(tedPtr->input);
    }
    return TCL_OK;
}


static void
LayoutGrid(tedPtr)
    Ted *tedPtr;
{
    int needed;
    XSegment *segArr;
    Table *tablePtr;
    Blt_ChainLink *linkPtr;
    RowColumn *rcPtr;
    int startX, endX;
    int startY, endY;
    int count;

    tablePtr = tedPtr->tablePtr;
    if (tedPtr->segArr != NULL) {
	Blt_Free(tedPtr->segArr);
	tedPtr->segArr = NULL;
    }
    tedPtr->nSegs = 0;
    if ((tablePtr->nRows == 0) || (tablePtr->nColumns == 0)) {
	return;
    }
    needed = tablePtr->nRows + tablePtr->nColumns + 2;
    segArr = Blt_Calloc(needed, sizeof(XSegment));
    if (segArr == NULL) {
	return;
    }
    linkPtr = Blt_ChainFirstLink(tablePtr->columnInfo.chainPtr);
    rcPtr = Blt_ChainGetValue(linkPtr);
    startX = rcPtr->offset - tedPtr->gridLineWidth;

    linkPtr = Blt_ChainLastLink(tablePtr->columnInfo.chainPtr);
    rcPtr = Blt_ChainGetValue(linkPtr);
    endX = rcPtr->offset + rcPtr->size - 1;

    linkPtr = Blt_ChainFirstLink(tablePtr->rowInfo.chainPtr);
    rcPtr = Blt_ChainGetValue(linkPtr);
    startY = rcPtr->offset - tedPtr->gridLineWidth;

    linkPtr = Blt_ChainLastLink(tablePtr->rowInfo.chainPtr);
    rcPtr = Blt_ChainGetValue(linkPtr);
    endY = rcPtr->offset + rcPtr->size - 1;

    count = 0;			/* Reset segment counter */

    for (linkPtr = Blt_ChainFirstLink(tablePtr->rowInfo.chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	segArr[count].x1 = startX;
	segArr[count].x2 = endX;
	segArr[count].y1 = segArr[count].y2 = rcPtr->offset - 
	    tedPtr->gridLineWidth;
	count++;
    }
    segArr[count].x1 = startX;
    segArr[count].x2 = endX;
    segArr[count].y1 = segArr[count].y2 = endY;
    count++;

    for (linkPtr = Blt_ChainFirstLink(tablePtr->columnInfo.chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	segArr[count].y1 = startY;
	segArr[count].y2 = endY;
	segArr[count].x1 = segArr[count].x2 = rcPtr->offset - 
	    tedPtr->gridLineWidth;
	count++;
    }
    segArr[count].x1 = segArr[count].x2 = endX;
    segArr[count].y1 = startY;
    segArr[count].y2 = endY;
    count++;
    assert(count == needed);
    if (tedPtr->segArr != NULL) {
	Blt_Free(tedPtr->segArr);
    }
    tedPtr->segArr = segArr;
    tedPtr->nSegs = count;
}


static void
LayoutPads(tedPtr)
    Ted *tedPtr;
{
    int needed;
    XRectangle *rectArr, *rectPtr;
    Table *tablePtr;
    Blt_ChainLink *linkPtr;
    RowColumn *rcPtr;
    int startX, endX;
    int startY, endY;
    int count;

    tablePtr = tedPtr->tablePtr;
    if (tedPtr->padRectArr != NULL) {
	Blt_Free(tedPtr->padRectArr);
	tedPtr->padRectArr = NULL;
    }
    tedPtr->nPadRects = 0;
    if ((tablePtr->nRows == 0) || (tablePtr->nColumns == 0)) {
	return;
    }
    needed = 2 * (tablePtr->nRows + tablePtr->nColumns);
    rectArr = Blt_Calloc(needed, sizeof(XRectangle));
    if (rectArr == NULL) {
	return;
    }
    linkPtr = Blt_ChainFirstLink(tablePtr->columnInfo.chainPtr);
    rcPtr = Blt_ChainGetValue(linkPtr);
    startX = rcPtr->offset;

    linkPtr = Blt_ChainLastLink(tablePtr->columnInfo.chainPtr);
    rcPtr = Blt_ChainGetValue(linkPtr);
    endX = (rcPtr->offset + rcPtr->size);

    linkPtr = Blt_ChainFirstLink(tablePtr->rowInfo.chainPtr);
    rcPtr = Blt_ChainGetValue(linkPtr);
    startY = rcPtr->offset;

    linkPtr = Blt_ChainLastLink(tablePtr->rowInfo.chainPtr);
    rcPtr = Blt_ChainGetValue(linkPtr);
    endY = (rcPtr->offset + rcPtr->size);

    count = 0;			/* Reset segment counter */
    rectPtr = rectArr;
    for (linkPtr = Blt_ChainFirstLink(tablePtr->rowInfo.chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	if (rcPtr->pad.side1 > 0) {
	    rectPtr->x = startX;
	    rectPtr->y = rcPtr->offset;
	    rectPtr->height = rcPtr->pad.side1;
	    rectPtr->width = endX - startX - 1;
	    rectPtr++, count++;
	}
	if (rcPtr->pad.side2 > 0) {
	    rectPtr->x = startX;
	    rectPtr->y = rcPtr->offset + rcPtr->size - rcPtr->pad.side2 - 1;
	    rectPtr->height = rcPtr->pad.side2;
	    rectPtr->width = endX - startX - 1;
	    rectPtr++, count++;
	}
    }
    for (linkPtr = Blt_ChainFirstLink(tablePtr->columnInfo.chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	if (rcPtr->pad.side1 > 0) {
	    rectPtr->x = rcPtr->offset;
	    rectPtr->y = startY;
	    rectPtr->height = endY - startY - 1;
	    rectPtr->width = rcPtr->pad.side1;
	    rectPtr++, count++;
	}
	if (rcPtr->pad.side2 > 0) {
	    rectPtr->x = rcPtr->offset + rcPtr->size - rcPtr->pad.side2;
	    rectPtr->y = startY;
	    rectPtr->height = endY - startY - 1;
	    rectPtr->width = rcPtr->pad.side2;
	    rectPtr++, count++;
	}
    }
    if (count == 0) {
	Blt_Free(rectArr);
	return;
    }
    tedPtr->padRectArr = rectArr;
    tedPtr->nPadRects = count;
}

static void
LayoutEntries(tedPtr)
    Ted *tedPtr;
{
    Entry *entryPtr;
    XRectangle *rectArr;
    int needed;
    int count;
    Blt_ChainLink *linkPtr;

    if (tedPtr->widgetPadRectArr != NULL) {
	Blt_Free(tedPtr->widgetPadRectArr);
	tedPtr->widgetPadRectArr = NULL;
    }
    tedPtr->nWidgetPadRects = 0;

    needed = Blt_ChainGetLength(tedPtr->tablePtr->chainPtr);
    rectArr = Blt_Calloc(needed, sizeof(XRectangle));
    if (rectArr == NULL) {
	return;
    }
    /* Draw any entry windows */
    count = 0;
    for (linkPtr = Blt_ChainFirstLink(tedPtr->tablePtr->chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	entryPtr = Blt_ChainGetValue(linkPtr);
	if ((PADDING(entryPtr->padX) + PADDING(entryPtr->padY)) == 0) {
	    continue;
	}
	rectArr[count].x = Tk_X(entryPtr->tkwin) - entryPtr->padLeft;
	rectArr[count].y = Tk_Y(entryPtr->tkwin) - entryPtr->padTop;
	rectArr[count].width = Tk_Width(entryPtr->tkwin) +
	    PADDING(entryPtr->padX);
	rectArr[count].height = Tk_Height(entryPtr->tkwin) +
	    PADDING(entryPtr->padY);
	count++;
    }
    if (count == 0) {
	Blt_Free(rectArr);
	return;
    }
    tedPtr->widgetPadRectArr = rectArr;
    tedPtr->nWidgetPadRects = count;
}

static void
LayoutControlEntries(tedPtr)
    Ted *tedPtr;
{
    Entry *entryPtr;
    XRectangle *rectArr;
    int needed;
    int count;
    Table *tablePtr = tedPtr->tablePtr;
    Blt_ChainLink *linkPtr;
    RowColumn *rcPtr;

    if (tedPtr->cntlRectArr != NULL) {
	Blt_Free(tedPtr->cntlRectArr);
	tedPtr->cntlRectArr = NULL;
    }
    tedPtr->nCntlRects = 0;

    needed = (tablePtr->nRows + tablePtr->nColumns);
    rectArr = Blt_Calloc(needed, sizeof(XRectangle));
    if (rectArr == NULL) {
	return;
    }
    /* Draw any entry windows */
    count = 0;
    for (linkPtr = Blt_ChainFirstLink(tablePtr->columnInfo.chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	entryPtr = rcPtr->control;
	if (entryPtr != NULL) {
	    rectArr[count].x = Tk_X(entryPtr->tkwin) - entryPtr->padLeft;
	    rectArr[count].y = Tk_Y(entryPtr->tkwin) - entryPtr->padTop;
	    rectArr[count].width = Tk_Width(entryPtr->tkwin) +
		PADDING(entryPtr->padX);
	    rectArr[count].height = Tk_Height(entryPtr->tkwin) +
		PADDING(entryPtr->padY);
	    count++;
	}
    }
    for (linkPtr = Blt_ChainFirstLink(tablePtr->rowInfo.chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	entryPtr = rcPtr->control;
	if (entryPtr != NULL) {
	    rectArr[count].x = Tk_X(entryPtr->tkwin) - entryPtr->padLeft;
	    rectArr[count].y = Tk_Y(entryPtr->tkwin) - entryPtr->padTop;
	    rectArr[count].width = Tk_Width(entryPtr->tkwin) +
		PADDING(entryPtr->padX);
	    rectArr[count].height = Tk_Height(entryPtr->tkwin) +
		PADDING(entryPtr->padY);
	    count++;
	}
    }
    if (count == 0) {
	Blt_Free(rectArr);
	return;
    }
    tedPtr->cntlRectArr = rectArr;
    tedPtr->nCntlRects = count;
}

static void
LayoutButtons(tedPtr)
    Ted *tedPtr;
{
    int needed;
    XRectangle *rectArr;
    Table *tablePtr;
    Blt_ChainLink *linkPtr;
    RowColumn *rcPtr;
    int count;

    tablePtr = tedPtr->tablePtr;
    if ((tablePtr->nRows == 0) || (tablePtr->nColumns == 0)) {
	if (tedPtr->rectArr != NULL) {
	    Blt_Free(tedPtr->rectArr);
	}
	tedPtr->rectArr = NULL;
	tedPtr->nRects = 0;
	return;			/* Nothing to display, empty table */
    }
    needed = 2 * (tablePtr->nRows + tablePtr->nColumns);
    rectArr = Blt_Calloc(needed, sizeof(XRectangle));
    if (rectArr == NULL) {
	return;			/* Can't allocate rectangles */
    }
    count = 0;
    for (linkPtr = Blt_ChainFirstLink(tablePtr->rowInfo.chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	rectArr[count].x = 0;
	rectArr[count].y = rcPtr->offset - rcPtr->pad.side1;
	rectArr[count].width = tedPtr->buttonHeight;
	rectArr[count].height = rcPtr->size - 2;
	count++;
	rectArr[count].x = Tk_Width(tedPtr->tkwin) - tedPtr->buttonHeight;
	rectArr[count].y = rcPtr->offset - rcPtr->pad.side1;
	rectArr[count].width = tedPtr->buttonHeight;
	rectArr[count].height = rcPtr->size - 2;
	count++;
    }
    for (linkPtr = Blt_ChainFirstLink(tablePtr->columnInfo.chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	rectArr[count].x = rcPtr->offset - rcPtr->pad.side1;
	rectArr[count].y = 0;
	rectArr[count].width = rcPtr->size - 2;
	rectArr[count].height = tedPtr->buttonHeight;
	count++;
	rectArr[count].x = rcPtr->offset - rcPtr->pad.side1;
	rectArr[count].y = Tk_Height(tedPtr->tkwin) - tedPtr->buttonHeight;
	rectArr[count].width = rcPtr->size - 2;
	rectArr[count].height = tedPtr->buttonHeight;
	count++;
    }
    assert(count == needed);
    if (tedPtr->rectArr != NULL) {
	Blt_Free(tedPtr->rectArr);
    }
    tedPtr->rectArr = rectArr;
    tedPtr->nRects = count;
}


static void
DisplayTed(clientData)
    ClientData clientData;
{
    Ted *tedPtr = (Ted *) clientData;
    Tk_Window master;
    Tk_Window tkwin;
    Blt_ChainLink *linkPtr;
    EntryRep *repPtr;
    Drawable drawable;
    Pixmap pixmap;

#ifdef notdef
    fprintf(stderr, "display grid\n");
#endif
    tedPtr->flags &= ~REDRAW_PENDING;
    if (!Tk_IsMapped(tedPtr->tkwin)) {
	return;
    }
    /*
     * Check if the master window has changed size and resize the
     * grid and input windows accordingly.
     */
    master = tedPtr->tablePtr->tkwin;
    if ((Tk_Width(master) != Tk_Width(tedPtr->tkwin)) ||
	(Tk_Height(master) != Tk_Height(tedPtr->tkwin))) {
#ifdef notdef
	fprintf(stderr, "resizing windows\n");
#endif
	Tk_ResizeWindow(tedPtr->tkwin, Tk_Width(master), Tk_Height(master));
	Tk_ResizeWindow(tedPtr->input, Tk_Width(master), Tk_Height(master));
	if (tedPtr->inputIsSibling) {
	    Tk_MoveWindow(tedPtr->input, Tk_X(master), Tk_X(master));
	}
	tedPtr->flags |= LAYOUT_PENDING;
    }
    if (tedPtr->flags & LAYOUT_PENDING) {
#ifdef notdef
	fprintf(stderr, "layout of grid\n");
#endif
	LayoutPads(tedPtr);
	LayoutEntries(tedPtr);
	LayoutControlEntries(tedPtr);
	LayoutGrid(tedPtr);
	LayoutButtons(tedPtr);
	tedPtr->flags &= ~LAYOUT_PENDING;
    }
    tkwin = tedPtr->tkwin;

    pixmap = None;		/* Suppress compiler warning. */
    drawable = Tk_WindowId(tkwin);
    if (tedPtr->doubleBuffer) {
	/* Create an off-screen pixmap for semi-smooth scrolling. */
	pixmap = Tk_GetPixmap(tedPtr->display, Tk_WindowId(tkwin),
	    Tk_Width(tkwin), Tk_Height(tkwin), Tk_Depth(tkwin));
	drawable = pixmap;
    }
    /* Clear the background of the grid */

    XFillRectangle(Tk_Display(tkwin), drawable, tedPtr->fillGC, 0, 0,
	Tk_Width(tkwin), Tk_Height(tkwin));

    /* Draw the row and column buttons */

    if (tedPtr->nRects > 0) {
	int i;

	for (i = 0; i < tedPtr->nRects; i++) {
	    Blt_Fill3DRectangle(tkwin, drawable, tedPtr->border,
		tedPtr->rectArr[i].x, tedPtr->rectArr[i].y,
		tedPtr->rectArr[i].width, tedPtr->rectArr[i].height,
		tedPtr->borderWidth, tedPtr->relief);
	}
#ifdef notdef
	XFillRectangles(tedPtr->display, drawable, tedPtr->rectGC,
	    tedPtr->rectArr, tedPtr->nRects);
	XDrawRectangles(tedPtr->display, drawable, tedPtr->drawGC,
	    tedPtr->rectArr, tedPtr->nRects);
#endif
    }
    if (tedPtr->nPadRects > 0) {
	XFillRectangles(tedPtr->display, drawable, tedPtr->padRectGC,
	    tedPtr->padRectArr, tedPtr->nPadRects);
    }
    if (tedPtr->spanActive) {
	XFillRectangles(tedPtr->display, drawable, tedPtr->spanGC,
	    tedPtr->activeRectArr, 1);
	XFillRectangles(tedPtr->display, drawable, tedPtr->drawGC,
	    tedPtr->activeRectArr + 1, 4);
    }
    if (tedPtr->nWidgetPadRects > 0) {
	XFillRectangles(tedPtr->display, drawable, tedPtr->attributes.fillGC,
	    tedPtr->widgetPadRectArr, tedPtr->nWidgetPadRects);
    }
    if (tedPtr->nCntlRects > 0) {
	XFillRectangles(tedPtr->display, drawable, tedPtr->attributes.cntlGC,
	    tedPtr->cntlRectArr, tedPtr->nCntlRects);
    }
    /* Draw the grid lines */
    if (tedPtr->nSegs > 0) {
	XDrawSegments(tedPtr->display, drawable, tedPtr->drawGC,
	    tedPtr->segArr, tedPtr->nSegs);
    }
#ifndef notdef
    /* Draw any entry windows */
    for (linkPtr = Blt_ChainFirstLink(tedPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	repPtr = Blt_ChainGetValue(linkPtr);
	if (repPtr->mapped) {
	    DisplayEntry(repPtr);
	}
    }
#endif
    if (tedPtr->doubleBuffer) {
	XCopyArea(tedPtr->display, drawable, Tk_WindowId(tkwin), tedPtr->fillGC,
	    0, 0, Tk_Width(tkwin), Tk_Height(tkwin), 0, 0);
	Tk_FreePixmap(tedPtr->display, pixmap);
    }
}


static void
DrawEditor(editPtr)
    Editor *editPtr;
{
    Ted *tedPtr = (Ted *) editPtr;

    tedPtr->flags |= LAYOUT_PENDING;
    if ((tedPtr->tkwin != NULL) && !(tedPtr->flags & REDRAW_PENDING)) {
	tedPtr->flags |= REDRAW_PENDING;
#ifdef notdef
	fprintf(stderr, "from draw editor\n");
#endif
	Tcl_DoWhenIdle(DisplayTed, tedPtr);
    }
}

static void
DestroyEditor(destroyData)
    DestroyData destroyData;
{
    Ted *tedPtr = (Ted *) destroyData;

    tedPtr->tkwin = NULL;
    if (tedPtr->flags & REDRAW_PENDING) {
	Tcl_CancelIdleCall(DisplayTed, tedPtr);
    }
    Tcl_EventuallyFree(tedPtr, DestroyTed);
}

/*
 * ----------------------------------------------------------------------------
 *
 * EditOp --
 *
 *	Processes an argv/argc list of table entries to add and configure
 *	new widgets into the table.  A table entry consists of the
 *	window path name, table index, and optional configuration options.
 *	The first argument in the argv list is the name of the table.  If
 *	no table exists for the given window, a new one is created.
 *
 * Results:
 *	Returns a standard Tcl result.  If an error occurred, TCL_ERROR is
 *	returned and an error message is left in interp->result.
 *
 * Side Effects:
 *	Memory is allocated, a new master table is possibly created, etc.
 *	The table is re-computed and arranged at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
static int
EditOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;		/* Interpreter to return list of names to */
    int argc;			/* Number of arguments */
    char **argv;
{
    Table *tablePtr;
    Ted *tedPtr;

    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tablePtr->editPtr != NULL) {	/* Already editing this table */
	tedPtr = (Ted *) tablePtr->editPtr;
    } else {
	tedPtr = CreateTed(tablePtr, interp);
	if (tedPtr == NULL) {
	    return TCL_ERROR;
	}
    }
    if (ConfigureTed(tedPtr, argc - 3, argv + 3, 0) != TCL_OK) {
	tedPtr->tkwin = NULL;
	if (tedPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayTed, tedPtr);
	}
	Tcl_EventuallyFree(tedPtr, DestroyTed);
	return TCL_ERROR;
    }
    /* Rearrange the table */
    if (!(tablePtr->flags & ARRANGE_PENDING)) {
	tablePtr->flags |= ARRANGE_PENDING;
	Tcl_DoWhenIdle(tablePtr->arrangeProc, tablePtr);
    }
    interp->result = Tk_PathName(tedPtr->tkwin);
    tedPtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(tedPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * CgetCmd --
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;		/* Interpreter to report results back to */
    int argc;			/* Not used. */
    char **argv;		/* Option-value pairs */
{
    Ted *tedPtr;

    tedPtr = FindEditor(dataPtr, interp, argv[2]);
    if (tedPtr == NULL) {
	return TCL_ERROR;
    }
    return Tk_ConfigureValue(interp, tedPtr->tkwin, configSpecs,
	    (char *)tedPtr, argv[3], 0);
}

/*
 * ----------------------------------------------------------------------------
 *
 * ConfigureCmd --
 *
 *	This procedure is called to process an argv/argc list in order to
 *	configure the table geometry manager.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Table configuration options (padx, pady, rows, columns, etc) get
 *	set.   The table is recalculated and arranged at the next idle
 *	point.
 *
 * ----------------------------------------------------------------------------
 */
static int
ConfigureOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;		/* Interpreter to report results back to */
    int argc;
    char **argv;		/* Option-value pairs */
{
    Ted *tedPtr;

    tedPtr = FindEditor(dataPtr, interp, argv[2]);
    if (tedPtr == NULL) {
	return TCL_ERROR;
    }
    if (argc == 3) {
	return Tk_ConfigureInfo(interp, tedPtr->tkwin, configSpecs,
		(char *)tedPtr, (char *)NULL, 0);
    } else if (argc == 4) {
	return Tk_ConfigureInfo(interp, tedPtr->tkwin, configSpecs,
		(char *)tedPtr, argv[3], 0);
    }
    if (ConfigureTed(tedPtr, argc - 3, argv + 3,
	    TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    EventuallyRedraw(tedPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * SelectOp --
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SelectOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;		/* Interpreter to return list of names to */
    int argc;			/* Not used. */
    char **argv;
{
    Table *tablePtr;
    Ted *tedPtr;
    Entry *entryPtr;
    int active;
    int x, y, width, height;
    int ix, iy;
    Blt_ChainLink *linkPtr;
    Tk_Window tkwin;

    /* ted select master @x,y */
    tkwin = Tk_MainWindow(interp);
    tedPtr = FindEditor(dataPtr, interp, argv[2]);
    if (tedPtr == NULL) {
	return TCL_ERROR;
    }
    if (Blt_GetXY(interp, tkwin, argv[3], &ix, &iy) != TCL_OK) {
	return TCL_ERROR;
    }
    tablePtr = tedPtr->tablePtr;
    active = 0;
    for (linkPtr = Blt_ChainFirstLink(tablePtr->chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	entryPtr = Blt_ChainGetValue(linkPtr);
	x = entryPtr->x - entryPtr->padX.side1;
	y = entryPtr->y - entryPtr->padY.side1;
	width = Tk_Width(entryPtr->tkwin) + PADDING(entryPtr->padX);
	height = Tk_Height(entryPtr->tkwin) + PADDING(entryPtr->padY);
	if ((ix >= x) && (ix <= (x + width)) &&
	    (iy >= y) && (iy <= (y + height))) {
	    int left, right, top, bottom;
	    int last;
	    int grip;
	    RowColumn *rcPtr;

	    last = entryPtr->column.rcPtr->index + entryPtr->column.span - 1;
	    linkPtr = Blt_ChainGetNthLink(tablePtr->columnInfo.chainPtr, last);
	    rcPtr = Blt_ChainGetValue(linkPtr);

	    /* Calculate the span rectangle */
	    left = (entryPtr->column.rcPtr->offset -
		entryPtr->column.rcPtr->pad.side1);
	    right = (rcPtr->offset - rcPtr->pad.side1) + rcPtr->size;

	    top = (entryPtr->row.rcPtr->offset -
		entryPtr->row.rcPtr->pad.side1);

	    last = entryPtr->row.rcPtr->index + entryPtr->row.span - 1;
	    linkPtr = Blt_ChainGetNthLink(tablePtr->rowInfo.chainPtr, last);
	    rcPtr = Blt_ChainGetValue(linkPtr);
	    bottom = (rcPtr->offset - rcPtr->pad.side1) + rcPtr->size;

	    tedPtr->activeRectArr[0].x = left;
	    tedPtr->activeRectArr[0].y = top;
	    tedPtr->activeRectArr[0].width = (right - left);
	    tedPtr->activeRectArr[0].height = (bottom - top);

	    grip = tedPtr->gripSize;
	    tedPtr->activeRectArr[1].x = (left + right - grip) / 2;
	    tedPtr->activeRectArr[1].y = top;
	    tedPtr->activeRectArr[1].width = grip - 1;
	    tedPtr->activeRectArr[1].height = grip - 1;

	    tedPtr->activeRectArr[2].x = left;
	    tedPtr->activeRectArr[2].y = (top + bottom - grip) / 2;
	    tedPtr->activeRectArr[2].width = grip - 1;
	    tedPtr->activeRectArr[2].height = grip - 1;

	    tedPtr->activeRectArr[3].x = (left + right - grip) / 2;
	    tedPtr->activeRectArr[3].y = bottom - grip;
	    tedPtr->activeRectArr[3].width = grip - 1;
	    tedPtr->activeRectArr[3].height = grip - 1;

	    tedPtr->activeRectArr[4].x = right - grip;
	    tedPtr->activeRectArr[4].y = (top + bottom - grip) / 2;
	    tedPtr->activeRectArr[4].width = grip - 1;
	    tedPtr->activeRectArr[4].height = grip - 1;

	    interp->result = Tk_PathName(entryPtr->tkwin);
	    active = 1;
	    break;
	}
    }
    if ((active) || (active != tedPtr->spanActive)) {
	tedPtr->spanActive = active;
	EventuallyRedraw(tedPtr);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * EditOp --
 *
 *	Processes an argv/argc list of table entries to add and configure
 *	new widgets into the table.  A table entry consists of the
 *	window path name, table index, and optional configuration options.
 *	The first argument in the argv list is the name of the table.  If
 *	no table exists for the given window, a new one is created.
 *
 * Results:
 *	Returns a standard Tcl result.  If an error occurred, TCL_ERROR is
 *	returned and an error message is left in interp->result.
 *
 * Side Effects:
 *	Memory is allocated, a new master table is possibly created, etc.
 *	The table is re-computed and arranged at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
static int
RepOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;		/* Interpreter to return list of names to */
    int argc;			/* Number of arguments */
    char **argv;
{
    Tk_Window tkwin;
    Table *tablePtr;
    Ted *tedPtr;

    /* ted rep master index */
    tkwin = Tk_NameToWindow(interp, argv[3], Tk_MainWindow(interp));
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tablePtr->editPtr != NULL) {	/* Already editing this table */
	tedPtr = (Ted *) tablePtr->editPtr;
    } else {
	tedPtr = CreateTed(tablePtr, interp);
	if (tedPtr == NULL) {
	    return TCL_ERROR;
	}
    }
    if (ConfigureTed(tedPtr, argc - 3, argv + 3, 0) != TCL_OK) {
	tedPtr->tkwin = NULL;
	if (tedPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayTed, tedPtr);
	}
	Tcl_EventuallyFree(tedPtr, DestroyTed);
	return TCL_ERROR;
    }
    /* Rearrange the table */
    if (!(tablePtr->flags & ARRANGE_PENDING)) {
	tablePtr->flags |= ARRANGE_PENDING;
	Tcl_DoWhenIdle(tablePtr->arrangeProc, tablePtr);
    }
    interp->result = Tk_PathName(tedPtr->tkwin);
    tedPtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(tedPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * Command options for the table editor.
 *
 * The fields for Blt_OperSpec are as follows:
 *
 *   - option name
 *   - minimum number of characters required to disambiguate the option name.
 *   - function associated with command option.
 *   - minimum number of arguments required.
 *   - maximum number of arguments allowed (0 indicates no limit).
 *   - usage string
 *
 * ----------------------------------------------------------------------------
 */
static Blt_OpSpec opSpecs[] =
{
    {"cget", 2, (Blt_Op)CgetOp, 4, 4, "master option",},
    {"configure", 2, (Blt_Op)ConfigureOp, 3, 0,
	"master ?option...?",},
    {"edit", 1, (Blt_Op)EditOp, 3, 0, "master ?options...?",},
    {"rep", 1, (Blt_Op)RepOp, 2, 0, "master index ?options...?",},
    {"select", 1, (Blt_Op)SelectOp, 4, 0, "master @x,y",},
 /* {"forget", 1, (Blt_Op)ForgetOp, 3, 0, "master ?master...?",},
    {"index", 1, (Blt_Op)IndexOp, 3, 0, "master ?item...?",}, */
};
static int nSpecs = sizeof(opSpecs) / sizeof(Blt_OpSpec);

/*
 * ----------------------------------------------------------------------------
 *
 * TedCmd --
 *
 *	This procedure is invoked to process the Tcl command that
 *	corresponds to the table geometry manager.  See the user
 *	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 * ----------------------------------------------------------------------------
 */
static int
TedCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nSpecs, opSpecs, BLT_OP_ARG1, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (clientData, interp, argc, argv);
    return result;
}

static TableData *
GetTableInterpData(interp)
     Tcl_Interp *interp;
{
    TableData *dataPtr;
    Tcl_InterpDeleteProc *proc;

    dataPtr = (TableData *)Tcl_GetAssocData(interp, TABLE_THREAD_KEY, &proc);
    assert(dataPtr);
    return dataPtr;
}

/*
 * ----------------------------------------------------------------------------
 *
 * Blt_TedInit --
 *
 *	This procedure is invoked to initialize the Tcl command that
 *	corresponds to the table geometry manager.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates the new command and adds an entry into a global Tcl
 *	associative array.
 *
 * ---------------------------------------------------------------------------
 */
int
Blt_TedInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = {"ted", TedCmd, };
    TableData *dataPtr;

    dataPtr = GetTableInterpData(interp);
    cmdSpec.clientData = dataPtr;
    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}
