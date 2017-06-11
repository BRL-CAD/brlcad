/* 
 * tkTable.c --
 *
 *	This module implements table widgets for the Tk
 *	toolkit.  An table displays a 2D array of strings
 *	and allows the strings to be edited.
 *
 * Based on Tk3 table widget written by Roland King
 *
 * Updates 1996 by:
 * Jeffrey Hobbs	jeff at hobbs org
 * John Ellson		ellson@lucent.com
 * Peter Bruecker	peter@bj-ig.de
 * Tom Moore		tmoore@spatial.ca
 * Sebastian Wangnick	wangnick@orthogon.de
 *
 * Copyright (c) 1997-2002 Jeffrey Hobbs
 *
 * See the file "license.txt" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tktable_cfg.h"
#include "tkTable.h"

#ifdef DEBUG
#include "dprint.h"
#endif

static char **	StringifyObjects(int objc, Tcl_Obj *CONST objv[]);

static int	Tk_TableObjCmd(ClientData clientData, Tcl_Interp *interp,
			int objc, Tcl_Obj *CONST objv[]);

static int	TableWidgetObjCmd(ClientData clientData, Tcl_Interp *interp,
			int objc, Tcl_Obj *CONST objv[]);
static int	TableConfigure(Tcl_Interp *interp, Table *tablePtr,
			int objc, Tcl_Obj *CONST objv[],
			int flags, int forceUpdate);
#ifdef HAVE_TCL84
static void	TableWorldChanged(ClientData instanceData);
#endif
static void	TableDestroy(ClientData clientdata);
static void	TableEventProc(ClientData clientData, XEvent *eventPtr);
static void	TableCmdDeletedProc(ClientData clientData);

static void	TableRedrawHighlight(Table *tablePtr);
static void	TableGetGc(Display *display, Drawable d,
			TableTag *tagPtr, GC *tagGc);

static void	TableDisplay(ClientData clientdata);
static void	TableFlashEvent(ClientData clientdata);
static char *	TableVarProc(ClientData clientData, Tcl_Interp *interp,
			char *name, char *index, int flags);
static void	TableCursorEvent(ClientData clientData);
static int	TableFetchSelection(ClientData clientData,
			int offset, char *buffer, int maxBytes);
static Tk_RestrictAction TableRestrictProc(ClientData arg, XEvent *eventPtr);

/*
 * The following tables define the widget commands (and sub-
 * commands) and map the indexes into the string tables into 
 * enumerated types used to dispatch the widget command.
 */

static CONST84 char *selCmdNames[] = {
    "anchor", "clear", "includes", "present", "set", (char *)NULL
};
enum selCommand {
    CMD_SEL_ANCHOR, CMD_SEL_CLEAR, CMD_SEL_INCLUDES, CMD_SEL_PRESENT,
    CMD_SEL_SET
};

static CONST84 char *commandNames[] = {
    "activate", "bbox", "border", "cget", "clear", "configure",
    "curselection", "curvalue", "delete", "get", "height",
    "hidden", "icursor", "index", "insert",
#ifdef POSTSCRIPT
    "postscript",
#endif
    "reread", "scan", "see", "selection", "set",
    "spans", "tag", "validate", "version", "window", "width",
    "xview", "yview", (char *)NULL
};
enum command {
    CMD_ACTIVATE, CMD_BBOX, CMD_BORDER, CMD_CGET, CMD_CLEAR, CMD_CONFIGURE,
    CMD_CURSEL, CMD_CURVALUE, CMD_DELETE, CMD_GET, CMD_HEIGHT,
    CMD_HIDDEN, CMD_ICURSOR, CMD_INDEX, CMD_INSERT,
#ifdef POSTSCRIPT
    CMD_POSTSCRIPT,
#endif
    CMD_REREAD, CMD_SCAN, CMD_SEE, CMD_SELECTION, CMD_SET,
    CMD_SPANS, CMD_TAG, CMD_VALIDATE, CMD_VERSION, CMD_WINDOW, CMD_WIDTH,
    CMD_XVIEW, CMD_YVIEW
};

/* -selecttype selection type options */
static Cmd_Struct sel_vals[]= {
    {"row",	 SEL_ROW},
    {"col",	 SEL_COL},
    {"both",	 SEL_BOTH},
    {"cell",	 SEL_CELL},
    {"",	 0 }
};

/* -resizeborders options */
static Cmd_Struct resize_vals[]= {
    {"row",	 SEL_ROW},		/* allow rows to be dragged */
    {"col",	 SEL_COL},		/* allow cols to be dragged */
    {"both",	 SEL_ROW|SEL_COL},	/* allow either to be dragged */
    {"none",	 SEL_NONE},		/* allow nothing to be dragged */
    {"",	 0 }
};

/* drawmode values */
/* The display redraws with a pixmap using TK function calls */
#define	DRAW_MODE_SLOW		(1<<0)
/* The redisplay is direct to the screen, but TK function calls are still
 * used to give correct 3-d border appearance and thus remain compatible
 * with other TK apps */
#define	DRAW_MODE_TK_COMPAT	(1<<1)
/* the redisplay goes straight to the screen and the 3d borders are rendered
 * with a single pixel wide line only. It cheats and uses the internal
 * border structure to do the borders */
#define DRAW_MODE_FAST		(1<<2)
#define DRAW_MODE_SINGLE	(1<<3)

static Cmd_Struct drawmode_vals[] = {
    {"fast",		DRAW_MODE_FAST},
    {"compatible",	DRAW_MODE_TK_COMPAT},
    {"slow",		DRAW_MODE_SLOW},
    {"single",		DRAW_MODE_SINGLE},
    {"", 0}
};

/* stretchmode values */
#define	STRETCH_MODE_NONE	(1<<0)	/* No additional pixels will be
					   added to rows or cols */
#define	STRETCH_MODE_UNSET	(1<<1)	/* All default rows or columns will
					   be stretched to fill the screen */
#define STRETCH_MODE_ALL	(1<<2)	/* All rows/columns will be padded
					   to fill the window */
#define STRETCH_MODE_LAST	(1<<3)	/* Stretch last elememt to fill
					   window */
#define STRETCH_MODE_FILL       (1<<4)	/* More ROWS in Window */

static Cmd_Struct stretch_vals[] = {
    {"none",	STRETCH_MODE_NONE},
    {"unset",	STRETCH_MODE_UNSET},
    {"all",	STRETCH_MODE_ALL},
    {"last",	STRETCH_MODE_LAST},
    {"fill",	STRETCH_MODE_FILL},
    {"", 0}
};

static Cmd_Struct state_vals[]= {
    {"normal",	 STATE_NORMAL},
    {"disabled", STATE_DISABLED},
    {"",	 0 }
};

/* The widget configuration table */
static Tk_CustomOption drawOpt		= { Cmd_OptionSet, Cmd_OptionGet,
					    (ClientData)(&drawmode_vals) };
static Tk_CustomOption resizeTypeOpt	= { Cmd_OptionSet, Cmd_OptionGet,
					    (ClientData)(&resize_vals) };
static Tk_CustomOption stretchOpt	= { Cmd_OptionSet, Cmd_OptionGet,
					    (ClientData)(&stretch_vals) };
static Tk_CustomOption selTypeOpt	= { Cmd_OptionSet, Cmd_OptionGet,
					    (ClientData)(&sel_vals) };
static Tk_CustomOption stateTypeOpt	= { Cmd_OptionSet, Cmd_OptionGet,
					    (ClientData)(&state_vals) };
static Tk_CustomOption bdOpt		= { TableOptionBdSet, TableOptionBdGet,
					    (ClientData) BD_TABLE };

Tk_ConfigSpec tableSpecs[] = {
    {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor", "center",
     Tk_Offset(Table, defaultTag.anchor), 0},
    {TK_CONFIG_BOOLEAN, "-autoclear", "autoClear", "AutoClear", "0",
     Tk_Offset(Table, autoClear), 0},
    {TK_CONFIG_BORDER, "-background", "background", "Background", NORMAL_BG,
     Tk_Offset(Table, defaultTag.bg), 0},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_CURSOR, "-bordercursor", "borderCursor", "Cursor", "crosshair",
     Tk_Offset(Table, bdcursor), TK_CONFIG_NULL_OK },
    {TK_CONFIG_CUSTOM, "-borderwidth", "borderWidth", "BorderWidth", "1",
     Tk_Offset(Table, defaultTag), TK_CONFIG_NULL_OK, &bdOpt },
    {TK_CONFIG_STRING, "-browsecommand", "browseCommand", "BrowseCommand", "",
     Tk_Offset(Table, browseCmd), TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-browsecmd", "browseCommand", (char *)NULL,
     (char *)NULL, 0, TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-cache", "cache", "Cache", "0",
     Tk_Offset(Table, caching), 0},
    {TK_CONFIG_INT, "-colorigin", "colOrigin", "Origin", "0",
     Tk_Offset(Table, colOffset), 0},
    {TK_CONFIG_INT, "-cols", "cols", "Cols", "10",
     Tk_Offset(Table, cols), 0},
    {TK_CONFIG_STRING, "-colseparator", "colSeparator", "Separator", NULL,
     Tk_Offset(Table, colSep), TK_CONFIG_NULL_OK },
    {TK_CONFIG_CUSTOM, "-colstretchmode", "colStretch", "StretchMode", "none",
     Tk_Offset (Table, colStretch), 0 , &stretchOpt },
    {TK_CONFIG_STRING, "-coltagcommand", "colTagCommand", "TagCommand", NULL,
     Tk_Offset(Table, colTagCmd), TK_CONFIG_NULL_OK },
    {TK_CONFIG_INT, "-colwidth", "colWidth", "ColWidth", "10",
     Tk_Offset(Table, defColWidth), 0},
    {TK_CONFIG_STRING, "-command", "command", "Command", "",
     Tk_Offset(Table, command), TK_CONFIG_NULL_OK},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor", "xterm",
     Tk_Offset(Table, cursor), TK_CONFIG_NULL_OK },
    {TK_CONFIG_CUSTOM, "-drawmode", "drawMode", "DrawMode", "compatible",
     Tk_Offset(Table, drawMode), 0, &drawOpt },
    {TK_CONFIG_STRING, "-ellipsis", "ellipsis", "Ellipsis", "",
     Tk_Offset(Table, defaultTag.ellipsis), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-exportselection", "exportSelection",
     "ExportSelection", "1", Tk_Offset(Table, exportSelection), 0},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_BOOLEAN, "-flashmode", "flashMode", "FlashMode", "0",
     Tk_Offset(Table, flashMode), 0},
    {TK_CONFIG_INT, "-flashtime", "flashTime", "FlashTime", "2",
     Tk_Offset(Table, flashTime), 0},
    {TK_CONFIG_FONT, "-font", "font", "Font",  DEF_TABLE_FONT,
     Tk_Offset(Table, defaultTag.tkfont), 0},
    {TK_CONFIG_BORDER, "-foreground", "foreground", "Foreground", "black",
     Tk_Offset(Table, defaultTag.fg), 0},
#ifdef PROCS
    {TK_CONFIG_BOOLEAN, "-hasprocs", "hasProcs", "hasProcs", "0",
     Tk_Offset(Table, hasProcs), 0},
#endif
    {TK_CONFIG_INT, "-height", "height", "Height", "0",
     Tk_Offset(Table, maxReqRows), 0},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
     "HighlightBackground", NORMAL_BG, Tk_Offset(Table, highlightBgColorPtr), 0},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
     HIGHLIGHT, Tk_Offset(Table, highlightColorPtr), 0},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
     "HighlightThickness", "2", Tk_Offset(Table, highlightWidth), 0},
    {TK_CONFIG_BORDER, "-insertbackground", "insertBackground", "Foreground",
     "Black", Tk_Offset(Table, insertBg), 0},
    {TK_CONFIG_PIXELS, "-insertborderwidth", "insertBorderWidth", "BorderWidth",
     "0", Tk_Offset(Table, insertBorderWidth), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_PIXELS, "-insertborderwidth", "insertBorderWidth", "BorderWidth",
     "0", Tk_Offset(Table, insertBorderWidth), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_INT, "-insertofftime", "insertOffTime", "OffTime", "300",
     Tk_Offset(Table, insertOffTime), 0},
    {TK_CONFIG_INT, "-insertontime", "insertOnTime", "OnTime", "600",
     Tk_Offset(Table, insertOnTime), 0},
    {TK_CONFIG_PIXELS, "-insertwidth", "insertWidth", "InsertWidth", "2",
     Tk_Offset(Table, insertWidth), 0},
    {TK_CONFIG_BOOLEAN, "-invertselected", "invertSelected", "InvertSelected",
     "0", Tk_Offset(Table, invertSelected), 0},
    {TK_CONFIG_PIXELS, "-ipadx", "ipadX", "Pad", "0",
     Tk_Offset(Table, ipadX), 0},
    {TK_CONFIG_PIXELS, "-ipady", "ipadY", "Pad", "0",
     Tk_Offset(Table, ipadY), 0},
    {TK_CONFIG_JUSTIFY, "-justify", "justify", "Justify", "left",
     Tk_Offset(Table, defaultTag.justify), 0 },
    {TK_CONFIG_PIXELS, "-maxheight", "maxHeight", "MaxHeight", "600",
     Tk_Offset(Table, maxReqHeight), 0},
    {TK_CONFIG_PIXELS, "-maxwidth", "maxWidth", "MaxWidth", "800",
     Tk_Offset(Table, maxReqWidth), 0},
    {TK_CONFIG_BOOLEAN, "-multiline", "multiline", "Multiline", "1",
     Tk_Offset(Table, defaultTag.multiline), 0},
    {TK_CONFIG_PIXELS, "-padx", "padX", "Pad", "0", Tk_Offset(Table, padX), 0},
    {TK_CONFIG_PIXELS, "-pady", "padY", "Pad", "0", Tk_Offset(Table, padY), 0},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief", "sunken",
     Tk_Offset(Table, defaultTag.relief), 0},
    {TK_CONFIG_CUSTOM, "-resizeborders", "resizeBorders", "ResizeBorders",
     "both", Tk_Offset(Table, resize), 0, &resizeTypeOpt },
    {TK_CONFIG_INT, "-rowheight", "rowHeight", "RowHeight", "1",
     Tk_Offset(Table, defRowHeight), 0},
    {TK_CONFIG_INT, "-roworigin", "rowOrigin", "Origin", "0",
     Tk_Offset(Table, rowOffset), 0},
    {TK_CONFIG_INT, "-rows", "rows", "Rows", "10", Tk_Offset(Table, rows), 0},
    {TK_CONFIG_STRING, "-rowseparator", "rowSeparator", "Separator", NULL,
     Tk_Offset(Table, rowSep), TK_CONFIG_NULL_OK },
    {TK_CONFIG_CUSTOM, "-rowstretchmode", "rowStretch", "StretchMode", "none",
     Tk_Offset(Table, rowStretch), 0 , &stretchOpt },
    {TK_CONFIG_STRING, "-rowtagcommand", "rowTagCommand", "TagCommand", NULL,
     Tk_Offset(Table, rowTagCmd), TK_CONFIG_NULL_OK },
    {TK_CONFIG_SYNONYM, "-selcmd", "selectionCommand", (char *)NULL,
     (char *)NULL, 0, TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-selectioncommand", "selectionCommand",
     "SelectionCommand", NULL, Tk_Offset(Table, selCmd), TK_CONFIG_NULL_OK },
    {TK_CONFIG_STRING, "-selectmode", "selectMode", "SelectMode", "browse",
     Tk_Offset(Table, selectMode), TK_CONFIG_NULL_OK },
    {TK_CONFIG_BOOLEAN, "-selecttitles", "selectTitles", "SelectTitles", "0",
     Tk_Offset(Table, selectTitles), 0},
    {TK_CONFIG_CUSTOM, "-selecttype", "selectType", "SelectType", "cell",
     Tk_Offset(Table, selectType), 0, &selTypeOpt },
#ifdef PROCS
    {TK_CONFIG_BOOLEAN, "-showprocs", "showProcs", "showProcs", "0",
     Tk_Offset(Table, showProcs), 0},
#endif
    {TK_CONFIG_BOOLEAN, "-sparsearray", "sparseArray", "SparseArray", "1",
     Tk_Offset(Table, sparse), 0},
    {TK_CONFIG_CUSTOM, "-state", "state", "State", "normal",
     Tk_Offset(Table, state), 0, &stateTypeOpt},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus", (char *)NULL,
     Tk_Offset(Table, takeFocus), TK_CONFIG_NULL_OK },
    {TK_CONFIG_INT, "-titlecols", "titleCols", "TitleCols", "0",
     Tk_Offset(Table, titleCols), TK_CONFIG_NULL_OK },
#ifdef TITLE_CURSOR
    {TK_CONFIG_CURSOR, "-titlecursor", "titleCursor", "Cursor", "arrow",
     Tk_Offset(Table, titleCursor), TK_CONFIG_NULL_OK },
#endif
    {TK_CONFIG_INT, "-titlerows", "titleRows", "TitleRows", "0",
     Tk_Offset(Table, titleRows), TK_CONFIG_NULL_OK },
    {TK_CONFIG_BOOLEAN, "-usecommand", "useCommand", "UseCommand", "1",
     Tk_Offset(Table, useCmd), 0},
    {TK_CONFIG_STRING, "-variable", "variable", "Variable", (char *)NULL,
     Tk_Offset(Table, arrayVar), TK_CONFIG_NULL_OK },
    {TK_CONFIG_BOOLEAN, "-validate", "validate", "Validate", "0",
     Tk_Offset(Table, validate), 0},
    {TK_CONFIG_STRING, "-validatecommand", "validateCommand", "ValidateCommand",
     "", Tk_Offset(Table, valCmd), TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-vcmd", "validateCommand", (char *)NULL,
     (char *)NULL, 0, TK_CONFIG_NULL_OK},
    {TK_CONFIG_INT, "-width", "width", "Width", "0",
     Tk_Offset(Table, maxReqCols), 0},
    {TK_CONFIG_BOOLEAN, "-wrap", "wrap", "Wrap", "0",
     Tk_Offset(Table, defaultTag.wrap), 0},
    {TK_CONFIG_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
     NULL, Tk_Offset(Table, xScrollCmd), TK_CONFIG_NULL_OK },
    {TK_CONFIG_STRING, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
     NULL, Tk_Offset(Table, yScrollCmd), TK_CONFIG_NULL_OK },
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
     (char *)NULL, 0, 0}
};

/*
 * This specifies the configure options that will cause an update to
 * occur, so we should have a quick lookup table for them.
 * Keep this in sync with the above values.
 */

static CONST84 char *updateOpts[] = {
    "-anchor",		"-background",	"-bg",		"-bd",	
    "-borderwidth",	"-cache",	"-command",	"-colorigin",
    "-cols",		"-colstretchmode",		"-coltagcommand",
    "-drawmode",	"-fg",		"-font",	"-foreground",
    "-hasprocs",	"-height",	"-highlightbackground",
    "-highlightcolor",	"-highlightthickness",		"-insertbackground",
    "-insertborderwidth",		"-insertwidth",	"-invertselected",
    "-ipadx",		"-ipady",
    "-maxheight",	"-maxwidth",	"-multiline",
    "-padx",		"-pady",	"-relief",	"-roworigin",
    "-rows",		"-rowstretchmode",		"-rowtagcommand",
    "-showprocs",	"-state",	"-titlecols",	"-titlerows",
    "-usecommand",	"-variable",	"-width",	"-wrap",	
    "-xscrollcommand",	"-yscrollcommand", (char *) NULL
};

#ifdef HAVE_TCL84
/*
 * The structure below defines widget class behavior by means of procedures
 * that can be invoked from generic window code.
 */

static Tk_ClassProcs tableClass = {
    sizeof(Tk_ClassProcs),	/* size */
    TableWorldChanged,		/* worldChangedProc */
    NULL,			/* createProc */
    NULL			/* modalProc */
};
#endif

#ifdef WIN32
/*
 * Some code from TkWinInt.h that we use to correct and speed up
 * drawing of cells that need clipping in TableDisplay.
 */
typedef struct {
    int type;
    HWND handle;
    void *winPtr;
} TkWinWindow;

typedef struct {
    int type;
    HBITMAP handle;
    Colormap colormap;
    int depth;
} TkWinBitmap;

typedef struct {
    int type;
    HDC hdc;
} TkWinDC;

typedef union {
    int type;
    TkWinWindow window;
    TkWinBitmap bitmap;
    TkWinDC winDC;
} TkWinDrawable;
#endif

/*
 * END HEADER INFORMATION
 */

/*
 *---------------------------------------------------------------------------
 *
 * StringifyObjects -- (from tclCmdAH.c)
 *
 *	Helper function to bridge the gap between an object-based procedure
 *	and an older string-based procedure.
 * 
 *	Given an array of objects, allocate an array that consists of the
 *	string representations of those objects.
 *
 * Results:
 *	The return value is a pointer to the newly allocated array of
 *	strings.  Elements 0 to (objc-1) of the string array point to the
 *	string representation of the corresponding element in the source
 *	object array; element objc of the string array is NULL.
 *
 * Side effects:
 *	Memory allocated.  The caller must eventually free this memory
 *	by calling ckfree() on the return value.
 *
	    int result;
	    char **argv;
	    argv   = StringifyObjects(objc, objv);
	    result = StringBasedCmd(interp, objc, argv);
	    ckfree((char *) argv);
	    return result;
 *
 *---------------------------------------------------------------------------
 */

static char **
StringifyObjects(objc, objv)
     int objc;			/* Number of arguments. */
     Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int i;
    char **argv;
    
    argv = (char **) ckalloc((objc + 1) * sizeof(char *));
    for (i = 0; i < objc; i++) {
    	argv[i] = Tcl_GetString(objv[i]);
    }
    argv[i] = NULL;
    return argv;
}

/*
 * As long as we wait for the Function in general
 *
 * This parses the "-class" option for the table.
 */
static int
Tk_ClassOptionObjCmd(Tk_Window tkwin, char *defaultclass,
		     int objc, Tcl_Obj *CONST objv[])
{
    char *classname = defaultclass;
    int offset = 0;

    if ((objc >= 4) && STREQ(Tcl_GetString(objv[2]),"-class")) {
	classname = Tcl_GetString(objv[3]);
	offset = 2;
    }
    Tk_SetClass(tkwin, classname);
    return offset;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_TableObjCmd --
 *	This procedure is invoked to process the "table" Tcl
 *	command.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */
static int
Tk_TableObjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Table *tablePtr;
    Tk_Window tkwin, mainWin = (Tk_Window) clientData;
    int offset;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "pathName ?options?");
	return TCL_ERROR;
    }

    tkwin = Tk_CreateWindowFromPath(interp, mainWin, Tcl_GetString(objv[1]),
	    (char *)NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }

    tablePtr			= (Table *) ckalloc(sizeof(Table));
    memset((VOID *) tablePtr, 0, sizeof(Table));

    /*
     * Set the structure elments that aren't 0/NULL by default,
     * and that won't be set by the initial configure call.
     */
    tablePtr->tkwin		= tkwin;
    tablePtr->display		= Tk_Display(tkwin);
    tablePtr->interp		= interp;
    tablePtr->widgetCmd	= Tcl_CreateObjCommand(interp,
	    Tk_PathName(tablePtr->tkwin), TableWidgetObjCmd,
	    (ClientData) tablePtr, (Tcl_CmdDeleteProc *) TableCmdDeletedProc);

    tablePtr->anchorRow		= -1;
    tablePtr->anchorCol		= -1;
    tablePtr->activeRow		= -1;
    tablePtr->activeCol		= -1;
    tablePtr->oldTopRow		= -1;
    tablePtr->oldLeftCol	= -1;
    tablePtr->oldActRow		= -1;
    tablePtr->oldActCol		= -1;
    tablePtr->seen[0]		= -1;

    tablePtr->dataSource	= DATA_NONE;
    tablePtr->activeBuf		= ckalloc(1);
    *(tablePtr->activeBuf)	= '\0';

    tablePtr->cursor		= None;
    tablePtr->bdcursor		= None;

    tablePtr->defaultTag.justify	= TK_JUSTIFY_LEFT;
    tablePtr->defaultTag.state		= STATE_UNKNOWN;

    /* misc tables */
    tablePtr->tagTable	= (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tablePtr->tagTable, TCL_STRING_KEYS);
    tablePtr->winTable	= (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tablePtr->winTable, TCL_STRING_KEYS);

    /* internal value cache */
    tablePtr->cache	= (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tablePtr->cache, TCL_STRING_KEYS);

    /* style hash tables */
    tablePtr->colWidths = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tablePtr->colWidths, TCL_ONE_WORD_KEYS);
    tablePtr->rowHeights = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tablePtr->rowHeights, TCL_ONE_WORD_KEYS);

    /* style hash tables */
    tablePtr->rowStyles = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tablePtr->rowStyles, TCL_ONE_WORD_KEYS);
    tablePtr->colStyles = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tablePtr->colStyles, TCL_ONE_WORD_KEYS);
    tablePtr->cellStyles = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tablePtr->cellStyles, TCL_STRING_KEYS);

    /* special style hash tables */
    tablePtr->flashCells = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tablePtr->flashCells, TCL_STRING_KEYS);
    tablePtr->selCells = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tablePtr->selCells, TCL_STRING_KEYS);

    /*
     * List of tags in priority order.  30 is a good default number to alloc.
     */
    tablePtr->tagPrioMax = 30;
    tablePtr->tagPrioNames = (char **) ckalloc(
	sizeof(char *) * tablePtr->tagPrioMax);
    tablePtr->tagPrios = (TableTag **) ckalloc(
	sizeof(TableTag *) * tablePtr->tagPrioMax);
    tablePtr->tagPrioSize = 0;
    for (offset = 0; offset < tablePtr->tagPrioMax; offset++) {
	tablePtr->tagPrioNames[offset] = (char *) NULL;
	tablePtr->tagPrios[offset] = (TableTag *) NULL;
    }

#ifdef PROCS
    tablePtr->inProc = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tablePtr->inProc, TCL_STRING_KEYS);
#endif

    /*
     * Handle class name and selection handlers
     */
    offset = 2 + Tk_ClassOptionObjCmd(tkwin, "Table", objc, objv);
#ifdef HAVE_TCL84
    Tk_SetClassProcs(tkwin, &tableClass, (ClientData) tablePtr);
#endif
    Tk_CreateEventHandler(tablePtr->tkwin,
	    PointerMotionMask|ExposureMask|StructureNotifyMask|FocusChangeMask|VisibilityChangeMask,
	    TableEventProc, (ClientData) tablePtr);
    Tk_CreateSelHandler(tablePtr->tkwin, XA_PRIMARY, XA_STRING,
	    TableFetchSelection, (ClientData) tablePtr, XA_STRING);

    if (TableConfigure(interp, tablePtr, objc - offset, objv + offset,
	    0, 1 /* force update */) != TCL_OK) {
	Tk_DestroyWindow(tkwin);
	return TCL_ERROR;
    }
    TableInitTags(tablePtr);

    Tcl_SetObjResult(interp,
	    Tcl_NewStringObj(Tk_PathName(tablePtr->tkwin), -1));
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TableWidgetObjCmd --
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a widget managed by this module.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */
static int
TableWidgetObjCmd(clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;			/* Number of arguments. */
     Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Table *tablePtr = (Table *) clientData;
    int row, col, i, cmdIndex, result = TCL_OK;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    /* parse the first parameter */
    result = Tcl_GetIndexFromObj(interp, objv[1], commandNames,
				 "option", 0, &cmdIndex);
    if (result != TCL_OK) {
	return result;
    }

    Tcl_Preserve((ClientData) tablePtr);
    switch ((enum command) cmdIndex) {
	case CMD_ACTIVATE:
	    result = Table_ActivateCmd(clientData, interp, objc, objv);
	    break;

	case CMD_BBOX:
	    result = Table_BboxCmd(clientData, interp, objc, objv);
	    break;

	case CMD_BORDER:
	    result = Table_BorderCmd(clientData, interp, objc, objv);
	    break;

	case CMD_CGET:
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "option");
		result = TCL_ERROR;
	    } else {
		result = Tk_ConfigureValue(interp, tablePtr->tkwin, tableSpecs,
			(char *) tablePtr, Tcl_GetString(objv[2]), 0);
	    }
	    break;

	case CMD_CLEAR:
	    result = Table_ClearCmd(clientData, interp, objc, objv);
	    break;

	case CMD_CONFIGURE:
	    if (objc < 4) {
		result = Tk_ConfigureInfo(interp, tablePtr->tkwin, tableSpecs,
			(char *) tablePtr, (objc == 3) ?
			Tcl_GetString(objv[2]) : (char *) NULL, 0);
	    } else {
		result = TableConfigure(interp, tablePtr, objc - 2, objv + 2,
			TK_CONFIG_ARGV_ONLY, 0);
	    }
	    break;

	case CMD_CURSEL:
	    result = Table_CurselectionCmd(clientData, interp, objc, objv);
	    break;

	case CMD_CURVALUE:
	    result = Table_CurvalueCmd(clientData, interp, objc, objv);
	    break;

	case CMD_DELETE:
	case CMD_INSERT:
	    result = Table_EditCmd(clientData, interp, objc, objv);
	    break;

	case CMD_GET:
	    result = Table_GetCmd(clientData, interp, objc, objv);
	    break;

	case CMD_HEIGHT:
	case CMD_WIDTH:
	    result = Table_AdjustCmd(clientData, interp, objc, objv);
	    break;

	case CMD_HIDDEN:
	    result = Table_HiddenCmd(clientData, interp, objc, objv);
	    break;

	case CMD_ICURSOR:
	    if (objc != 2 && objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "?cursorPos?");
		result = TCL_ERROR;
		break;
	    }
	    if (!(tablePtr->flags & HAS_ACTIVE) ||
		    (tablePtr->flags & ACTIVE_DISABLED) ||
		    tablePtr->state == STATE_DISABLED) {
		Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
		break;
	    } else if (objc == 3) {
		if (TableGetIcursorObj(tablePtr, objv[2], NULL) != TCL_OK) {
		    result = TCL_ERROR;
		    break;
		}
		TableRefresh(tablePtr, tablePtr->activeRow,
			tablePtr->activeCol, CELL);
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(tablePtr->icursor));
	    break;

	case CMD_INDEX: {
	    char *which = NULL;

	    if (objc == 4) {
		which = Tcl_GetString(objv[3]);
	    }
	    if ((objc < 3 || objc > 4) ||
		    ((objc == 4) && (strcmp(which, "row")
			    && strcmp(which, "col")))) {
		Tcl_WrongNumArgs(interp, 2, objv, "<index> ?row|col?");
		result = TCL_ERROR;
	    } else if (TableGetIndexObj(tablePtr, objv[2], &row, &col)
		    != TCL_OK) {
		result = TCL_ERROR;
	    } else if (objc == 3) {
		char buf[INDEX_BUFSIZE];
		/* recreate the index, just in case it got bounded */
		TableMakeArrayIndex(row, col, buf);
		Tcl_SetObjResult(interp, Tcl_NewStringObj(buf, -1));
	    } else {	/* INDEX row|col */
		Tcl_SetObjResult(interp,
			Tcl_NewIntObj((*which == 'r') ? row : col));
	    }
	    break;
	}

#ifdef POSTSCRIPT
	case CMD_POSTSCRIPT:
	    result = Table_PostscriptCmd(clientData, interp, objc, objv);
	    break;
#endif

	case CMD_REREAD:
	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		result = TCL_ERROR;
	    } else if ((tablePtr->flags & HAS_ACTIVE) &&
		    !(tablePtr->flags & ACTIVE_DISABLED) &&
		    tablePtr->state != STATE_DISABLED) {
		TableGetActiveBuf(tablePtr);
		TableRefresh(tablePtr, tablePtr->activeRow,
			tablePtr->activeCol, CELL|INV_FORCE);
	    }
	    break;

	case CMD_SCAN:
	    result = Table_ScanCmd(clientData, interp, objc, objv);
	    break;

	case CMD_SEE:
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "index");
		result = TCL_ERROR;
	    } else if (TableGetIndexObj(tablePtr, objv[2],
		    &row, &col) == TCL_ERROR) {
		result = TCL_ERROR;
	    } else {
		/* Adjust from user to master coords */
		row -= tablePtr->rowOffset;
		col -= tablePtr->colOffset;
		if (!TableCellVCoords(tablePtr, row, col, &i, &i, &i, &i, 1)) {
		    tablePtr->topRow  = row-1;
		    tablePtr->leftCol = col-1;
		    TableAdjustParams(tablePtr);
		}
	    }
	    break;

	case CMD_SELECTION:
	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "option ?arg arg ...?");
		result = TCL_ERROR;
		break;
	    }
	    if (Tcl_GetIndexFromObj(interp, objv[2], selCmdNames,
		    "selection option", 0, &cmdIndex) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    switch ((enum selCommand) cmdIndex) {
		case CMD_SEL_ANCHOR:
		    result = Table_SelAnchorCmd(clientData, interp,
			    objc, objv);
		    break;
		case CMD_SEL_CLEAR:
		    result = Table_SelClearCmd(clientData, interp, objc, objv);
		    break;
		case CMD_SEL_INCLUDES:
		    result = Table_SelIncludesCmd(clientData, interp,
			    objc, objv);
		    break;
		case CMD_SEL_PRESENT: {
		    Tcl_HashSearch search;
		    int present = (Tcl_FirstHashEntry(tablePtr->selCells,
				    &search) != NULL);

		    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(present));
		    break;
		}
		case CMD_SEL_SET:
		    result = Table_SelSetCmd(clientData, interp, objc, objv);
		    break;
	    }
	    break;

	case CMD_SET:
	    result = Table_SetCmd(clientData, interp, objc, objv);
	    break;

	case CMD_SPANS:
	    result = Table_SpanCmd(clientData, interp, objc, objv);
	    break;

	case CMD_TAG:
	    result = Table_TagCmd(clientData, interp, objc, objv);
	    break;

	case CMD_VALIDATE:
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "index");
		result = TCL_ERROR;
	    } else if (TableGetIndexObj(tablePtr, objv[2],
		    &row, &col) == TCL_ERROR) {
		result = TCL_ERROR;
	    } else {
		i = tablePtr->validate;
		tablePtr->validate = 1;
		result = TableValidateChange(tablePtr, row, col, (char *) NULL,
			(char *) NULL, -1);
		tablePtr->validate = i;
		Tcl_SetObjResult(interp, Tcl_NewBooleanObj(result == TCL_OK));
		result = TCL_OK;
	    }
	    break;

	case CMD_VERSION:
	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		result = TCL_ERROR;
	    } else {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(PACKAGE_VERSION, -1));
	    }
	    break;

	case CMD_WINDOW:
	    result = Table_WindowCmd(clientData, interp, objc, objv);
	    break;

	case CMD_XVIEW:
	case CMD_YVIEW:
	    result = Table_ViewCmd(clientData, interp, objc, objv);
	    break;
    }

    Tcl_Release((ClientData) tablePtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TableDestroy --
 *	This procedure is invoked by Tcl_EventuallyFree
 *	to clean up the internal structure of a table at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the table is freed up (hopefully).
 *
 *----------------------------------------------------------------------
 */
static void
TableDestroy(ClientData clientdata)
{
    register Table *tablePtr = (Table *) clientdata;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;

    /* These may be repetitive from DestroyNotify, but it doesn't hurt */
    /* cancel any pending update or timer */
    if (tablePtr->flags & REDRAW_PENDING) {
	Tcl_CancelIdleCall(TableDisplay, (ClientData) tablePtr);
	tablePtr->flags &= ~REDRAW_PENDING;
    }
    Tcl_DeleteTimerHandler(tablePtr->cursorTimer);
    Tcl_DeleteTimerHandler(tablePtr->flashTimer);

    /* delete the variable trace */
    if (tablePtr->arrayVar != NULL) {
	Tcl_UntraceVar(tablePtr->interp, tablePtr->arrayVar,
		TCL_TRACE_WRITES | TCL_TRACE_UNSETS | TCL_GLOBAL_ONLY,
		(Tcl_VarTraceProc *)TableVarProc, (ClientData) tablePtr);
    }

    /* free the int arrays */
    if (tablePtr->colPixels) ckfree((char *) tablePtr->colPixels);
    if (tablePtr->rowPixels) ckfree((char *) tablePtr->rowPixels);
    if (tablePtr->colStarts) ckfree((char *) tablePtr->colStarts);
    if (tablePtr->rowStarts) ckfree((char *) tablePtr->rowStarts);

    /* delete cached active tag and string */
    if (tablePtr->activeTagPtr) ckfree((char *) tablePtr->activeTagPtr);
    if (tablePtr->activeBuf != NULL) ckfree(tablePtr->activeBuf);

    /*
     * Delete the various hash tables, make sure to clear the STRING_KEYS
     * tables that allocate their strings:
     *   cache, spanTbl (spanAffTbl shares spanTbl info)
     */
    Table_ClearHashTable(tablePtr->cache);
    ckfree((char *) (tablePtr->cache));
    Tcl_DeleteHashTable(tablePtr->rowStyles);
    ckfree((char *) (tablePtr->rowStyles));
    Tcl_DeleteHashTable(tablePtr->colStyles);
    ckfree((char *) (tablePtr->colStyles));
    Tcl_DeleteHashTable(tablePtr->cellStyles);
    ckfree((char *) (tablePtr->cellStyles));
    Tcl_DeleteHashTable(tablePtr->flashCells);
    ckfree((char *) (tablePtr->flashCells));
    Tcl_DeleteHashTable(tablePtr->selCells);
    ckfree((char *) (tablePtr->selCells));
    Tcl_DeleteHashTable(tablePtr->colWidths);
    ckfree((char *) (tablePtr->colWidths));
    Tcl_DeleteHashTable(tablePtr->rowHeights);
    ckfree((char *) (tablePtr->rowHeights));
#ifdef PROCS
    Tcl_DeleteHashTable(tablePtr->inProc);
    ckfree((char *) (tablePtr->inProc));
#endif
    if (tablePtr->spanTbl) {
	Table_ClearHashTable(tablePtr->spanTbl);
	ckfree((char *) (tablePtr->spanTbl));
	Tcl_DeleteHashTable(tablePtr->spanAffTbl);
	ckfree((char *) (tablePtr->spanAffTbl));
    }

    /* Now free up all the tag information */
    for (entryPtr = Tcl_FirstHashEntry(tablePtr->tagTable, &search);
	 entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&search)) {
	TableCleanupTag(tablePtr, (TableTag *) Tcl_GetHashValue(entryPtr));
	ckfree((char *) Tcl_GetHashValue(entryPtr));
    }
    /* free up the stuff in the default tag */
    TableCleanupTag(tablePtr, &(tablePtr->defaultTag));
    /* And delete the actual hash table */
    Tcl_DeleteHashTable(tablePtr->tagTable);
    ckfree((char *) (tablePtr->tagTable));
    ckfree((char *) (tablePtr->tagPrios));
    ckfree((char *) (tablePtr->tagPrioNames));

    /* Now free up all the embedded window info */
    for (entryPtr = Tcl_FirstHashEntry(tablePtr->winTable, &search);
	 entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&search)) {
	EmbWinDelete(tablePtr, (TableEmbWindow *) Tcl_GetHashValue(entryPtr));
    }
    /* And delete the actual hash table */
    Tcl_DeleteHashTable(tablePtr->winTable);
    ckfree((char *) (tablePtr->winTable));

    /* free the configuration options in the widget */
    Tk_FreeOptions(tableSpecs, (char *) tablePtr, tablePtr->display, 0);

    /* and free the widget memory at last! */
    ckfree((char *) (tablePtr));
}

/*
 *----------------------------------------------------------------------
 *
 * TableConfigure --
 *	This procedure is called to process an objc/objv list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	a table widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width, etc.
 *	get set for tablePtr; old resources get freed, if there were any.
 *	Certain values might be constrained.
 *
 *----------------------------------------------------------------------
 */
static int
TableConfigure(interp, tablePtr, objc, objv, flags, forceUpdate)
     Tcl_Interp *interp;	/* Used for error reporting. */
     register Table *tablePtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
     int objc;			/* Number of arguments. */
     Tcl_Obj *CONST objv[];	/* Argument objects. */
     int flags;			/* Flags to pass to Tk_ConfigureWidget. */
     int forceUpdate;		/* Whether to force an update - required
				 * for initial configuration */
{
    Tcl_HashSearch search;
    int oldUse, oldCaching, oldExport, oldTitleRows, oldTitleCols;
    int result = TCL_OK;
    char *oldVar = NULL, **argv;
    Tcl_DString error;
    Tk_FontMetrics fm;

    oldExport	= tablePtr->exportSelection;
    oldCaching	= tablePtr->caching;
    oldUse	= tablePtr->useCmd;
    oldTitleRows	= tablePtr->titleRows;
    oldTitleCols	= tablePtr->titleCols;
    if (tablePtr->arrayVar != NULL) {
	oldVar = ckalloc(strlen(tablePtr->arrayVar) + 1);
	strcpy(oldVar, tablePtr->arrayVar);
    }

    /* Do the configuration */
    argv = StringifyObjects(objc, objv);
    result = Tk_ConfigureWidget(interp, tablePtr->tkwin, tableSpecs,
	    objc, (CONST84 char **) argv, (char *) tablePtr, flags);
    ckfree((char *) argv);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }

    Tcl_DStringInit(&error);

    /* Any time we configure, reevaluate what our data source is */
    tablePtr->dataSource = DATA_NONE;
    if (tablePtr->caching) {
	tablePtr->dataSource |= DATA_CACHE;
    }
    if (tablePtr->command && tablePtr->useCmd) {
	tablePtr->dataSource |= DATA_COMMAND;
    } else if (tablePtr->arrayVar) {
	tablePtr->dataSource |= DATA_ARRAY;
    }

    /* Check to see if the array variable was changed */
    if (strcmp((tablePtr->arrayVar ? tablePtr->arrayVar : ""),
	    (oldVar ? oldVar : ""))) {
	/* only do the following if arrayVar is our data source */
	if (tablePtr->dataSource & DATA_ARRAY) {
	    /*
	     * ensure that the cache will flush later
	     * so it gets the new values
	     */
	    oldCaching = !(tablePtr->caching);
	}
	/* remove the trace on the old array variable if there was one */
	if (oldVar != NULL)
	    Tcl_UntraceVar(interp, oldVar,
		    TCL_TRACE_WRITES|TCL_TRACE_UNSETS|TCL_GLOBAL_ONLY,
		    (Tcl_VarTraceProc *)TableVarProc, (ClientData) tablePtr);
	/* Check whether variable is an array and trace it if it is */
	if (tablePtr->arrayVar != NULL) {
	    /* does the variable exist as an array? */
	    if (Tcl_SetVar2(interp, tablePtr->arrayVar, TEST_KEY, "",
		    TCL_GLOBAL_ONLY) == NULL) {
		Tcl_DStringAppend(&error, "invalid variable value \"", -1);
		Tcl_DStringAppend(&error, tablePtr->arrayVar, -1);
		Tcl_DStringAppend(&error, "\": could not be made an array",
			-1);
		ckfree(tablePtr->arrayVar);
		tablePtr->arrayVar = NULL;
		tablePtr->dataSource &= ~DATA_ARRAY;
		result = TCL_ERROR;
	    } else {
		Tcl_UnsetVar2(interp, tablePtr->arrayVar, TEST_KEY,
			TCL_GLOBAL_ONLY);
		/* remove the effect of the evaluation */
		/* set a trace on the variable */
		Tcl_TraceVar(interp, tablePtr->arrayVar,
			TCL_TRACE_WRITES|TCL_TRACE_UNSETS|TCL_GLOBAL_ONLY,
			(Tcl_VarTraceProc *)TableVarProc,
			(ClientData) tablePtr);

		/* only do the following if arrayVar is our data source */
		if (tablePtr->dataSource & DATA_ARRAY) {
		    /* get the current value of the selection */
		    TableGetActiveBuf(tablePtr);
		}
	    }
	}
    }

    /* Free oldVar if it was allocated */
    if (oldVar != NULL) ckfree(oldVar);

    if ((tablePtr->command && tablePtr->useCmd && !oldUse) ||
	(tablePtr->arrayVar && !(tablePtr->useCmd) && oldUse)) {
	/*
	 * Our effective data source changed, so flush and
	 * retrieve new active buffer
	 */
	Table_ClearHashTable(tablePtr->cache);
	Tcl_InitHashTable(tablePtr->cache, TCL_STRING_KEYS);
	TableGetActiveBuf(tablePtr);
	forceUpdate = 1;
    } else if (oldCaching != tablePtr->caching) {
	/*
	 * Caching changed, so just clear the cache for safety
	 */
	Table_ClearHashTable(tablePtr->cache);
	Tcl_InitHashTable(tablePtr->cache, TCL_STRING_KEYS);
	forceUpdate = 1;
    }

    /*
     * Set up the default column width and row height
     */
    Tk_GetFontMetrics(tablePtr->defaultTag.tkfont, &fm);
    tablePtr->charWidth  = Tk_TextWidth(tablePtr->defaultTag.tkfont, "0", 1);
    tablePtr->charHeight = fm.linespace + 2;

    if (tablePtr->insertWidth <= 0) {
	tablePtr->insertWidth = 2;
    }
    if (tablePtr->insertBorderWidth > tablePtr->insertWidth/2) {
	tablePtr->insertBorderWidth = tablePtr->insertWidth/2;
    }
    tablePtr->highlightWidth = MAX(0,tablePtr->highlightWidth);

    /*
     * Ensure that certain values are within proper constraints
     */
    tablePtr->rows		= MAX(1, tablePtr->rows);
    tablePtr->cols		= MAX(1, tablePtr->cols);
    tablePtr->padX		= MAX(0, tablePtr->padX);
    tablePtr->padY		= MAX(0, tablePtr->padY);
    tablePtr->ipadX		= MAX(0, tablePtr->ipadX);
    tablePtr->ipadY		= MAX(0, tablePtr->ipadY);
    tablePtr->maxReqCols	= MAX(0, tablePtr->maxReqCols);
    tablePtr->maxReqRows	= MAX(0, tablePtr->maxReqRows);
    CONSTRAIN(tablePtr->titleRows, 0, tablePtr->rows);
    CONSTRAIN(tablePtr->titleCols, 0, tablePtr->cols);

    /*
     * Handle change of default border style
     * The default borderwidth must be >= 0.
     */
    if (tablePtr->drawMode & (DRAW_MODE_SINGLE|DRAW_MODE_FAST)) {
	/*
	 * When drawing fast or single, the border must be <= 1.
	 * We have to do this after the normal configuration
	 * to base the borders off the first value given.
	 */
	tablePtr->defaultTag.bd[0]	= MIN(1, tablePtr->defaultTag.bd[0]);
	tablePtr->defaultTag.borders	= 1;
	ckfree((char *) tablePtr->defaultTag.borderStr);
	tablePtr->defaultTag.borderStr	= (char *) ckalloc(2);
	strcpy(tablePtr->defaultTag.borderStr,
		tablePtr->defaultTag.bd[0] ? "1" : "0");
    }

    /*
     * Claim the selection if we've suddenly started exporting it and
     * there is a selection to export.
     */
    if (tablePtr->exportSelection && !oldExport &&
	(Tcl_FirstHashEntry(tablePtr->selCells, &search) != NULL)) {
	Tk_OwnSelection(tablePtr->tkwin, XA_PRIMARY, TableLostSelection,
		(ClientData) tablePtr);
    }

    if ((tablePtr->titleRows < oldTitleRows) ||
	(tablePtr->titleCols < oldTitleCols)) {
	/*
	 * Prevent odd movement due to new possible topleft index
	 */
	if (tablePtr->titleRows < oldTitleRows)
	    tablePtr->topRow -= oldTitleRows - tablePtr->titleRows;
	if (tablePtr->titleCols < oldTitleCols)
	    tablePtr->leftCol -= oldTitleCols - tablePtr->titleCols;
	/*
	 * If our title area shrank, we need to check that the items
	 * within the new title area don't try to span outside it.
	 */
	TableSpanSanCheck(tablePtr);
    }

    /*
     * Only do the full reconfigure if absolutely necessary
     */
    if (!forceUpdate) {
	int i, dummy;
	for (i = 0; i < objc-1; i += 2) {
	    if (Tcl_GetIndexFromObj(NULL, objv[i], updateOpts, "", 0, &dummy)
		    == TCL_OK) {
		forceUpdate = 1;
		break;
	    }
	}
    }
    if (forceUpdate) {
	/* 
	 * Calculate the row and column starts 
	 * Adjust the top left corner of the internal display 
	 */
	TableAdjustParams(tablePtr);
	/* reset the cursor */
	TableConfigCursor(tablePtr);
	/* set up the background colour in the window */
	Tk_SetBackgroundFromBorder(tablePtr->tkwin, tablePtr->defaultTag.bg);
	/* set the geometry and border */
	TableGeometryRequest(tablePtr);
	Tk_SetInternalBorder(tablePtr->tkwin, tablePtr->highlightWidth);
	/* invalidate the whole table */
	TableInvalidateAll(tablePtr, INV_HIGHLIGHT);
    }
    /*
     * FIX this is goofy because the result could be munged by other
     * functions.  Could be improved.
     */
    Tcl_ResetResult(interp);
    if (result == TCL_ERROR) {
	Tcl_AddErrorInfo(interp, "\t(configuring table widget)");
	Tcl_DStringResult(interp, &error);
    }
    Tcl_DStringFree(&error);
    return result;
}
#ifdef HAVE_TCL84
/*
 *---------------------------------------------------------------------------
 *
 * TableWorldChanged --
 *
 *      This procedure is called when the world has changed in some
 *      way and the widget needs to recompute all its graphics contexts
 *	and determine its new geometry.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Entry will be relayed out and redisplayed.
 *
 *---------------------------------------------------------------------------
 */
 
static void
TableWorldChanged(instanceData)
    ClientData instanceData;	/* Information about widget. */
{
    Table *tablePtr = (Table *) instanceData;
    Tk_FontMetrics fm;

    /*
     * Set up the default column width and row height
     */
    Tk_GetFontMetrics(tablePtr->defaultTag.tkfont, &fm);
    tablePtr->charWidth  = Tk_TextWidth(tablePtr->defaultTag.tkfont, "0", 1);
    tablePtr->charHeight = fm.linespace + 2;

    /*
     * Recompute the window's geometry and arrange for it to be redisplayed.
     */

    TableAdjustParams(tablePtr);
    TableGeometryRequest(tablePtr);
    Tk_SetInternalBorder(tablePtr->tkwin, tablePtr->highlightWidth);
    /* invalidate the whole table */
    TableInvalidateAll(tablePtr, INV_HIGHLIGHT);
}
#endif
/*
 *--------------------------------------------------------------
 *
 * TableEventProc --
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on tables.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */
static void
TableEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Table *tablePtr = (Table *) clientData;
    int row, col;

    switch (eventPtr->type) {
	case MotionNotify:
	    if (!(tablePtr->resize & SEL_NONE)
		    && (tablePtr->bdcursor != None) &&
		    TableAtBorder(tablePtr, eventPtr->xmotion.x,
			    eventPtr->xmotion.y, &row, &col) &&
		    ((row>=0 && (tablePtr->resize & SEL_ROW)) ||
			    (col>=0 && (tablePtr->resize & SEL_COL)))) {
		/*
		 * The bordercursor is defined and we meet the criteria for
		 * being over a border.  Set the cursor to border if not
		 * already done.
		 */
		if (!(tablePtr->flags & OVER_BORDER)) {
		    tablePtr->flags |= OVER_BORDER;
		    Tk_DefineCursor(tablePtr->tkwin, tablePtr->bdcursor);
		}
	    } else if (tablePtr->flags & OVER_BORDER) {
		tablePtr->flags &= ~OVER_BORDER;
		if (tablePtr->cursor != None) {
		    Tk_DefineCursor(tablePtr->tkwin, tablePtr->cursor);
		} else {
		    Tk_UndefineCursor(tablePtr->tkwin);
		}
#ifdef TITLE_CURSOR
	    } else if (tablePtr->flags & (OVER_BORDER|OVER_TITLE)) {
		Tk_Cursor cursor = tablePtr->cursor;

		//tablePtr->flags &= ~(OVER_BORDER|OVER_TITLE);

		if (tablePtr->titleCursor != None) {
		    TableWhatCell(tablePtr, eventPtr->xmotion.x,
			    eventPtr->xmotion.y, &row, &col);
		    if ((row < tablePtr->titleRows) ||
			    (col < tablePtr->titleCols)) {
			if (tablePtr->flags & OVER_TITLE) {
			    break;
			}
			tablePtr->flags |= OVER_TITLE;
			cursor = tablePtr->titleCursor;
		    }
		}
		if (cursor != None) {
		    Tk_DefineCursor(tablePtr->tkwin, cursor);
		} else {
		    Tk_UndefineCursor(tablePtr->tkwin);
		}
	    } else if (tablePtr->titleCursor != None) {
		Tk_Cursor cursor = tablePtr->cursor;

		TableWhatCell(tablePtr, eventPtr->xmotion.x,
			eventPtr->xmotion.y, &row, &col);
		if ((row < tablePtr->titleRows) ||
			(col < tablePtr->titleCols)) {
		    if (tablePtr->flags & OVER_TITLE) {
			break;
		    }
		    tablePtr->flags |= OVER_TITLE;
		    cursor = tablePtr->titleCursor;
		}
#endif
	    }
	    break;

	case Expose:
	    TableInvalidate(tablePtr, eventPtr->xexpose.x, eventPtr->xexpose.y,
		    eventPtr->xexpose.width, eventPtr->xexpose.height,
		    INV_HIGHLIGHT);
	    break;

	case DestroyNotify:
	    /* remove the command from the interpreter */
	    if (tablePtr->tkwin != NULL) {
		tablePtr->tkwin = NULL;
		Tcl_DeleteCommandFromToken(tablePtr->interp,
			tablePtr->widgetCmd);
	    }

	    /* cancel any pending update or timer */
	    if (tablePtr->flags & REDRAW_PENDING) {
		Tcl_CancelIdleCall(TableDisplay, (ClientData) tablePtr);
		tablePtr->flags &= ~REDRAW_PENDING;
	    }
	    Tcl_DeleteTimerHandler(tablePtr->cursorTimer);
	    Tcl_DeleteTimerHandler(tablePtr->flashTimer);

	    Tcl_EventuallyFree((ClientData) tablePtr,
		    (Tcl_FreeProc *) TableDestroy);
	    break;

	case MapNotify: /* redraw table when remapped if it changed */
	    if (tablePtr->flags & REDRAW_ON_MAP) {
		tablePtr->flags &= ~REDRAW_ON_MAP;
		Tcl_Preserve((ClientData) tablePtr);
		TableAdjustParams(tablePtr);
		TableInvalidateAll(tablePtr, INV_HIGHLIGHT);
		Tcl_Release((ClientData) tablePtr);
	    }
	    break;

	case ConfigureNotify:
	    Tcl_Preserve((ClientData) tablePtr);
	    TableAdjustParams(tablePtr);
	    TableInvalidateAll(tablePtr, INV_HIGHLIGHT);
	    Tcl_Release((ClientData) tablePtr);
	    break;

	case FocusIn:
	case FocusOut:
	    if (eventPtr->xfocus.detail != NotifyInferior) {
		tablePtr->flags |= REDRAW_BORDER;
		if (eventPtr->type == FocusOut) {
		    tablePtr->flags &= ~HAS_FOCUS;
		} else {
		    tablePtr->flags |= HAS_FOCUS;
		}
		TableRedrawHighlight(tablePtr);
		/* cancel the timer */
		TableConfigCursor(tablePtr);
	    }
	    break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TableCmdDeletedProc --
 *
 *	This procedure is invoked when a widget command is deleted.  If
 *	the widget isn't already in the process of being destroyed,
 *	this command destroys it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */
static void
TableCmdDeletedProc(ClientData clientData)
{
    Table *tablePtr = (Table *) clientData;
    Tk_Window tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tablePtr->tkwin != NULL) {
	tkwin = tablePtr->tkwin;
	tablePtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/* 
 *----------------------------------------------------------------------
 *
 * TableRedrawHighlight --
 *	Redraws just the highlight for the window
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */
static void
TableRedrawHighlight(Table *tablePtr)
{
    if ((tablePtr->flags & REDRAW_BORDER) && tablePtr->highlightWidth > 0) {
	GC gc = Tk_GCForColor((tablePtr->flags & HAS_FOCUS)
		? tablePtr->highlightColorPtr : tablePtr->highlightBgColorPtr,
		Tk_WindowId(tablePtr->tkwin));
	Tk_DrawFocusHighlight(tablePtr->tkwin, gc, tablePtr->highlightWidth,
		Tk_WindowId(tablePtr->tkwin));
    }
    tablePtr->flags &= ~REDRAW_BORDER;
}

/*
 *----------------------------------------------------------------------
 *
 * TableRefresh --
 *	Refreshes an area of the table based on the mode.
 *	row,col in real coords (0-based)
 *
 * Results:
 *	Will cause redraw for visible cells
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
TableRefresh(register Table *tablePtr, int row, int col, int mode)
{
    int x, y, w, h;

    if ((row < 0) || (col < 0)) {
	/*
	 * Invalid coords passed in.  This can happen when the "active" cell
	 * is refreshed, but doesn't really exist (row==-1 && col==-1).
	 */
	return;
    }
    if (mode & CELL) {
	if (TableCellVCoords(tablePtr, row, col, &x, &y, &w, &h, 0)) {
	    TableInvalidate(tablePtr, x, y, w, h, mode);
	}
    } else if (mode & ROW) {
	/* get the position of the leftmost cell in the row */
	if ((mode & INV_FILL) && row < tablePtr->topRow) {
	    /* Invalidate whole table */
	    TableInvalidateAll(tablePtr, mode);
	} else if (TableCellVCoords(tablePtr, row, tablePtr->leftCol,
		&x, &y, &w, &h, 0)) {
	    /* Invalidate from this row, maybe to end */
	    TableInvalidate(tablePtr, 0, y, Tk_Width(tablePtr->tkwin),
		    (mode&INV_FILL)?Tk_Height(tablePtr->tkwin):h, mode);
	}
    } else if (mode & COL) {
	/* get the position of the topmost cell on the column */
	if ((mode & INV_FILL) && col < tablePtr->leftCol) {
	    /* Invalidate whole table */
	    TableInvalidateAll(tablePtr, mode);
	} else if (TableCellVCoords(tablePtr, tablePtr->topRow, col,
		&x, &y, &w, &h, 0)) {
	    /* Invalidate from this column, maybe to end */
	    TableInvalidate(tablePtr, x, 0,
		    (mode&INV_FILL)?Tk_Width(tablePtr->tkwin):w,
		    Tk_Height(tablePtr->tkwin), mode);
	}
    }
}

/* 
 *----------------------------------------------------------------------
 *
 * TableGetGc --
 *	Gets a GC corresponding to the tag structure passed.
 *
 * Results:
 *	Returns usable GC.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */
static void
TableGetGc(Display *display, Drawable d, TableTag *tagPtr, GC *tagGc)
{
    XGCValues gcValues;
    gcValues.foreground = Tk_3DBorderColor(tagPtr->fg)->pixel;
    gcValues.background = Tk_3DBorderColor(tagPtr->bg)->pixel;
    gcValues.font = Tk_FontId(tagPtr->tkfont);
    if (*tagGc == NULL) {
	gcValues.graphics_exposures = False;
	*tagGc = XCreateGC(display, d,
		GCForeground|GCBackground|GCFont|GCGraphicsExposures,
		&gcValues);
    } else {
	XChangeGC(display, *tagGc, GCForeground|GCBackground|GCFont,
		&gcValues);
    }
}

#define TableFreeGc	XFreeGC

/*
 *--------------------------------------------------------------
 *
 * TableUndisplay --
 *	This procedure removes the contents of a table window
 *	that have been moved offscreen.
 *
 * Results:
 *	Embedded windows can be unmapped.
 *
 * Side effects:
 *	Information disappears from the screen.
 *
 *--------------------------------------------------------------
 */
static void
TableUndisplay(register Table *tablePtr)
{
    register int *seen = tablePtr->seen;
    int row, col;

    /* We need to find out the true last cell, not considering spans */
    tablePtr->flags |= AVOID_SPANS;
    TableGetLastCell(tablePtr, &row, &col);
    tablePtr->flags &= ~AVOID_SPANS;

    if (seen[0] != -1) {
	if (seen[0] < tablePtr->topRow) {
	    /* Remove now hidden rows */
	    EmbWinUnmap(tablePtr, seen[0], MIN(seen[2],tablePtr->topRow-1),
		    seen[1], seen[3]);
	    /* Also account for the title area */
	    EmbWinUnmap(tablePtr, seen[0], MIN(seen[2],tablePtr->topRow-1),
		    0, tablePtr->titleCols-1);
	}
	if (seen[1] < tablePtr->leftCol) {
	    /* Remove now hidden cols */
	    EmbWinUnmap(tablePtr, seen[0], seen[2],
		    seen[1], MAX(seen[3],tablePtr->leftCol-1));
	    /* Also account for the title area */
	    EmbWinUnmap(tablePtr, 0, tablePtr->titleRows-1,
		    seen[1], MAX(seen[3],tablePtr->leftCol-1));
	}
	if (seen[2] > row) {
	    /* Remove now off-screen rows */
	    EmbWinUnmap(tablePtr, MAX(seen[0],row+1), seen[2],
		    seen[1], seen[3]);
	    /* Also account for the title area */
	    EmbWinUnmap(tablePtr, MAX(seen[0],row+1), seen[2],
		    0, tablePtr->titleCols-1);
	}
	if (seen[3] > col) {
	    /* Remove now off-screen cols */
	    EmbWinUnmap(tablePtr, seen[0], seen[2],
		    MAX(seen[1],col+1), seen[3]);
	    /* Also account for the title area */
	    EmbWinUnmap(tablePtr, 0, tablePtr->titleRows-1,
		    MAX(seen[1],col+1), seen[3]);
	}
    }
    seen[0] = tablePtr->topRow;
    seen[1] = tablePtr->leftCol;
    seen[2] = row;
    seen[3] = col;
}

/*
 * Generally we should be able to use XSetClipRectangles on X11, but
 * the addition of Xft drawing to Tk 8.5+ completely ignores the clip
 * rectangles.  Thus turn it off for all cases until clip rectangles
 * are known to be respected. [Bug 1805350]
 */
#if 1 || defined(MAC_TCL) || defined(UNDER_CE) || (defined(WIN32) && defined(TCL_THREADS)) || defined(MAC_OSX_TK)
#define NO_XSETCLIP
#endif
/*
 *--------------------------------------------------------------
 *
 * TableDisplay --
 *	This procedure redraws the contents of a table window.
 *	The conditional code in this function is due to these factors:
 *		o Lack of XSetClipRectangles on Macintosh
 *		o Use of alternative routine for Windows
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information appears on the screen.
 *
 *--------------------------------------------------------------
 */
static void
TableDisplay(ClientData clientdata)
{
    register Table *tablePtr = (Table *) clientdata;
    Tk_Window tkwin = tablePtr->tkwin;
    Display *display = tablePtr->display;
    Drawable window;
#ifdef NO_XSETCLIP
    Drawable clipWind;
#elif defined(WIN32)
    TkWinDrawable *twdPtr;
    HDC dc;
    HRGN clipR;
#else
    XRectangle clipRect;
#endif
    int rowFrom, rowTo, colFrom, colTo,
	invalidX, invalidY, invalidWidth, invalidHeight,
	x, y, width, height, itemX, itemY, itemW, itemH,
	row, col, urow, ucol, hrow=0, hcol=0, cx, cy, cw, ch, borders, bd[6],
	numBytes, new, boundW, boundH, maxW, maxH, cellType,
	originX, originY, activeCell, shouldInvert, ipadx, ipady, padx, pady;
    GC tagGc = NULL, topGc, bottomGc;
    char *string = NULL;
    char buf[INDEX_BUFSIZE];
    TableTag *tagPtr = NULL, *titlePtr, *selPtr, *activePtr, *flashPtr,
	*rowPtr, *colPtr;
    Tcl_HashEntry *entryPtr;
    static XPoint rect[3] = { {0, 0}, {0, 0}, {0, 0} };
    Tcl_HashTable *colTagsCache = NULL;
    Tcl_HashTable *drawnCache = NULL;
    Tk_TextLayout textLayout = NULL;
    TableEmbWindow *ewPtr;
    Tk_FontMetrics fm;
    Tk_Font ellFont = NULL;
    char *ellipsis = NULL;
    int ellLen = 0, useEllLen = 0, ellEast = 0;

    tablePtr->flags &= ~REDRAW_PENDING;
    if ((tkwin == NULL) || !Tk_IsMapped(tkwin)) {
	return;
    }

    boundW = Tk_Width(tkwin) - tablePtr->highlightWidth;
    boundH = Tk_Height(tkwin) - tablePtr->highlightWidth;

    /* Constrain drawable to not include highlight borders */
    invalidX = MAX(tablePtr->highlightWidth, tablePtr->invalidX);
    invalidY = MAX(tablePtr->highlightWidth, tablePtr->invalidY);
    invalidWidth  = MIN(tablePtr->invalidWidth, MAX(1, boundW-invalidX));
    invalidHeight = MIN(tablePtr->invalidHeight, MAX(1, boundH-invalidY));

    ipadx = tablePtr->ipadX;
    ipady = tablePtr->ipadY;
    padx  = tablePtr->padX;
    pady  = tablePtr->padY;

#ifndef WIN32
    /* 
     * if we are using the slow drawing mode with a pixmap 
     * create the pixmap and adjust x && y for offset in pixmap
     * FIX: Ignore slow mode for Win32 as the fast ClipRgn trick
     * below does not work for bitmaps.
     */
    if (tablePtr->drawMode == DRAW_MODE_SLOW) {
	window = Tk_GetPixmap(display, Tk_WindowId(tkwin),
		invalidWidth, invalidHeight, Tk_Depth(tkwin));
    } else
#endif
	window = Tk_WindowId(tkwin);
#ifdef NO_XSETCLIP
    clipWind = Tk_GetPixmap(display, window,
	    invalidWidth, invalidHeight, Tk_Depth(tkwin));
#endif

    /* set up the permanent tag styles */
    entryPtr	= Tcl_FindHashEntry(tablePtr->tagTable, "title");
    titlePtr	= (TableTag *) Tcl_GetHashValue(entryPtr);
    entryPtr	= Tcl_FindHashEntry(tablePtr->tagTable, "sel");
    selPtr	= (TableTag *) Tcl_GetHashValue(entryPtr);
    entryPtr	= Tcl_FindHashEntry(tablePtr->tagTable, "active");
    activePtr	= (TableTag *) Tcl_GetHashValue(entryPtr);
    entryPtr	= Tcl_FindHashEntry(tablePtr->tagTable, "flash");
    flashPtr	= (TableTag *) Tcl_GetHashValue(entryPtr);

    /* We need to find out the true cell span, not considering spans */
    tablePtr->flags |= AVOID_SPANS;
    /* find out the cells represented by the invalid region */
    TableWhatCell(tablePtr, invalidX, invalidY, &rowFrom, &colFrom);
    TableWhatCell(tablePtr, invalidX+invalidWidth-1,
	    invalidY+invalidHeight-1, &rowTo, &colTo);
    tablePtr->flags &= ~AVOID_SPANS;

#ifdef DEBUG
    tcl_dprintf(tablePtr->interp, "%d,%d => %d,%d",
	    rowFrom+tablePtr->rowOffset, colFrom+tablePtr->colOffset,
	    rowTo+tablePtr->rowOffset, colTo+tablePtr->colOffset);
#endif

    /* 
     * Initialize colTagsCache hash table to cache column tag names.
     */
    colTagsCache = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(colTagsCache, TCL_ONE_WORD_KEYS);
    /* 
     * Initialize drawnCache hash table to cache drawn cells.
     * This is necessary to prevent spanning cells being drawn multiple times.
     */
    drawnCache = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(drawnCache, TCL_STRING_KEYS);

    /*
     * Create the tag here.  This will actually create a JoinTag
     * That will handle the priority management of merging for us.
     * We only need one allocated, and we'll reset it for each cell.
     */
    tagPtr = TableNewTag(tablePtr);

    /* Cycle through the cells and display them */
    for (row = rowFrom; row <= rowTo; row++) {
	/* 
	 * are we in the 'dead zone' between the
	 * title rows and the first displayed row 
	 */
	if (row < tablePtr->topRow && row >= tablePtr->titleRows) {
	    row = tablePtr->topRow;
	}

	/* Cache the row in user terms */
	urow = row+tablePtr->rowOffset;

	/* Get the row tag once for all iterations of col */
	rowPtr = FindRowColTag(tablePtr, urow, ROW);

	for (col = colFrom; col <= colTo; col++) {
	    activeCell = 0;
	    /* 
	     * Adjust to first viewable column if we are in the 'dead zone'
	     * between the title cols and the first displayed column.
	     */
	    if (col < tablePtr->leftCol && col >= tablePtr->titleCols) {
		col = tablePtr->leftCol;
	    }

	    /*
	     * Get the coordinates for the cell before possible rearrangement
	     * of row,col due to spanning cells
	     */
	    cellType = TableCellCoords(tablePtr, row, col,
		    &x, &y, &width, &height);
	    if (cellType == CELL_HIDDEN) {
		/*
		 * width,height holds the real start row,col of the span.
		 * Put the use cell ref into a buffer for the hash lookups.
		 */
		TableMakeArrayIndex(width, height, buf);
		Tcl_CreateHashEntry(drawnCache, buf, &new);
		if (!new) {
		    /* Not new in the entry, so it's already drawn */
		    continue;
		}
		hrow = row; hcol = col;
		row = width-tablePtr->rowOffset;
		col = height-tablePtr->colOffset;
		TableCellVCoords(tablePtr, row, col,
			&x, &y, &width, &height, 0);
		/* We have to adjust the coords back onto the visual display */
		urow = row+tablePtr->rowOffset;
		rowPtr = FindRowColTag(tablePtr, urow, ROW);
	    }

	    /* Constrain drawn size to the visual boundaries */
	    if (width > boundW-x)	{ width  = boundW-x; }
	    if (height > boundH-y)	{ height = boundH-y; }

	    /* Cache the col in user terms */
	    ucol = col+tablePtr->colOffset;

	    /* put the use cell ref into a buffer for the hash lookups */
	    TableMakeArrayIndex(urow, ucol, buf);
	    if (cellType != CELL_HIDDEN) {
		Tcl_CreateHashEntry(drawnCache, buf, &new);
	    }

	    /*
	     * Make sure we start with a clean tag (set to table defaults).
	     */
	    TableResetTag(tablePtr, tagPtr);

	    /*
	     * Check to see if we have an embedded window in this cell.
	     */
	    entryPtr = Tcl_FindHashEntry(tablePtr->winTable, buf);
	    if (entryPtr != NULL) {
		ewPtr = (TableEmbWindow *) Tcl_GetHashValue(entryPtr);

		if (ewPtr->tkwin != NULL) {
		    /* Display embedded window instead of text */

		    /* if active, make it disabled to avoid
		     * unnecessary editing */
		    if ((tablePtr->flags & HAS_ACTIVE)
			    && row == tablePtr->activeRow
			    && col == tablePtr->activeCol) {
			tablePtr->flags |= ACTIVE_DISABLED;
		    }

		    /*
		     * The EmbWinDisplay function may modify values in
		     * tagPtr, so reference those after this call.
		     */
		    EmbWinDisplay(tablePtr, window, ewPtr, tagPtr,
			    x, y, width, height);

#ifndef WIN32
		    if (tablePtr->drawMode == DRAW_MODE_SLOW) {
			/* Correctly adjust x && y with the offset */
			x -= invalidX;
			y -= invalidY;
		    }
#endif

		    Tk_Fill3DRectangle(tkwin, window, tagPtr->bg, x, y, width,
			    height, 0, TK_RELIEF_FLAT);

		    /* border width for cell should now be properly set */
		    borders = TableGetTagBorders(tagPtr, &bd[0], &bd[1],
			    &bd[2], &bd[3]);
		    bd[4] = (bd[0] + bd[1])/2;
		    bd[5] = (bd[2] + bd[3])/2;

		    goto DrawBorder;
		}
	    }

	    /*
	     * Don't draw what won't be seen.
	     * Embedded windows handle this in EmbWinDisplay. 
	     */
	    if ((width <= 0) || (height <= 0)) { continue; }

#ifndef WIN32
	    if (tablePtr->drawMode == DRAW_MODE_SLOW) {
		/* Correctly adjust x && y with the offset */
		x -= invalidX;
		y -= invalidY;
	    }
#endif

	    shouldInvert = 0;
	    /*
	     * Get the combined tag structure for the cell.
	     * First clear out a new tag structure that we will build in
	     * then add tags as we realize they belong.
	     *
	     * Tags have their own priorities which TableMergeTag will
	     * take into account when merging tags.
	     */

	    /*
	     * Merge colPtr if it exists
	     * let's see if we have the value cached already
	     * if not, run the findColTag routine and cache the value
	     */
	    entryPtr = Tcl_CreateHashEntry(colTagsCache, (char *)ucol, &new);
	    if (new) {
		colPtr = FindRowColTag(tablePtr, ucol, COL);
		Tcl_SetHashValue(entryPtr, colPtr);
	    } else {
		colPtr = (TableTag *) Tcl_GetHashValue(entryPtr);
	    }
	    if (colPtr != (TableTag *) NULL) {
		TableMergeTag(tablePtr, tagPtr, colPtr);
	    }
	    /* Merge rowPtr if it exists */
	    if (rowPtr != (TableTag *) NULL) {
		TableMergeTag(tablePtr, tagPtr, rowPtr);
	    }
	    /* Am I in the titles */
	    if (row < tablePtr->titleRows || col < tablePtr->titleCols) {
		TableMergeTag(tablePtr, tagPtr, titlePtr);
	    }
	    /* Does this have a cell tag */
	    entryPtr = Tcl_FindHashEntry(tablePtr->cellStyles, buf);
	    if (entryPtr != NULL) {
		TableMergeTag(tablePtr, tagPtr,
			(TableTag *) Tcl_GetHashValue(entryPtr));
	    }
	    /* is this cell active? */
	    if ((tablePtr->flags & HAS_ACTIVE) &&
		    (tablePtr->state == STATE_NORMAL) &&
		    row == tablePtr->activeRow && col == tablePtr->activeCol) {
		if (tagPtr->state == STATE_DISABLED) {
		    tablePtr->flags |= ACTIVE_DISABLED;
		} else {
		    TableMergeTag(tablePtr, tagPtr, activePtr);
		    activeCell = 1;
		    tablePtr->flags &= ~ACTIVE_DISABLED;
		}
	    }
	    /* is this cell selected? */
	    if (Tcl_FindHashEntry(tablePtr->selCells, buf) != NULL) {
		if (tablePtr->invertSelected && !activeCell) {
		    shouldInvert = 1;
		} else {
		    TableMergeTag(tablePtr, tagPtr, selPtr);
		}
	    }
	    /* if flash mode is on, is this cell flashing? */
	    if (tablePtr->flashMode &&
		    Tcl_FindHashEntry(tablePtr->flashCells, buf) != NULL) {
		TableMergeTag(tablePtr, tagPtr, flashPtr);
	    }

	    if (shouldInvert) {
		TableInvertTag(tagPtr);
	    }

	    /*
	     * Borders for cell should now be properly set
	     */
	    borders = TableGetTagBorders(tagPtr, &bd[0], &bd[1],
		    &bd[2], &bd[3]);
	    bd[4] = (bd[0] + bd[1])/2;
	    bd[5] = (bd[2] + bd[3])/2;

	    /*
	     * First fill in a blank rectangle.
	     */
	    Tk_Fill3DRectangle(tkwin, window, tagPtr->bg,
		    x, y, width, height, 0, TK_RELIEF_FLAT);

	    /*
	     * Correct the dimensions to enforce padding constraints
	     */
	    width  -= bd[0] + bd[1] + (2 * padx);
	    height -= bd[2] + bd[3] + (2 * pady);

	    /*
	     * Don't draw what won't be seen, based on border constraints.
	     */
	    if ((width <= 0) || (height <= 0)) {
		/*
		 * Re-Correct the dimensions before border drawing
		 */
		width  += bd[0] + bd[1] + (2 * padx);
		height += bd[2] + bd[3] + (2 * pady);
		goto DrawBorder;
	    }

	    /*
	     * If an image is in the tag, draw it
	     */
	    if (tagPtr->image != NULL) {
		Tk_SizeOfImage(tagPtr->image, &itemW, &itemH);
		/* Handle anchoring of image in cell space */
		switch (tagPtr->anchor) {
		    case TK_ANCHOR_NW:
		    case TK_ANCHOR_W:
		    case TK_ANCHOR_SW:		/* western position */
			originX = itemX = 0;
			break;
		    case TK_ANCHOR_N:
		    case TK_ANCHOR_S:
		    case TK_ANCHOR_CENTER:	/* centered position */
			itemX	= MAX(0, (itemW - width) / 2);
			originX	= MAX(0, (width - itemW) / 2);
			break;
		    default:			/* eastern position */
			itemX	= MAX(0, itemW - width);
			originX	= MAX(0, width - itemW);
		}
		switch (tagPtr->anchor) {
		    case TK_ANCHOR_N:
		    case TK_ANCHOR_NE:
		    case TK_ANCHOR_NW:		/* northern position */
			originY = itemY = 0;
			break;
		    case TK_ANCHOR_W:
		    case TK_ANCHOR_E:
		    case TK_ANCHOR_CENTER:	/* centered position */
			itemY	= MAX(0, (itemH - height) / 2);
			originY	= MAX(0, (height - itemH) / 2);
			break;
		    default:			/* southern position */
			itemY	= MAX(0, itemH - height);
			originY	= MAX(0, height - itemH);
		}
		Tk_RedrawImage(tagPtr->image, itemX, itemY,
			MIN(itemW, width-originX), MIN(itemH, height-originY),
			window, x + originX + bd[0] + padx,
			y + originY + bd[2] + pady);
		/*
		 * If we don't want to display the text as well, then jump.
		 */
		if (tagPtr->showtext == 0) {
		    /*
		     * Re-Correct the dimensions before border drawing
		     */
		    width  += bd[0] + bd[1] + (2 * padx);
		    height += bd[2] + bd[3] + (2 * pady);
		    goto DrawBorder;
		}
	    }

	    /*
	     * Get the GC for this particular blend of tags.
	     * This creates the GC if it never existed, otherwise it
	     * modifies the one we have, so we only need the one
	     */
	    TableGetGc(display, window, tagPtr, &tagGc);

	    /* if this is the active cell, use the buffer */
	    if (activeCell) {
		string = tablePtr->activeBuf;
	    } else {
		/* Is there a value in the cell? If so, draw it  */
		string = TableGetCellValue(tablePtr, urow, ucol);
	    }

#ifdef TCL_UTF_MAX
	    /*
	     * We have to use strlen here because otherwise it stops
	     * at the first \x00 unicode char it finds (!= '\0'),
	     * although there can be more to the string than that
	     */
	    numBytes = Tcl_NumUtfChars(string, (int) strlen(string));
#else
	    numBytes = strlen(string);
#endif

	    /* If there is a string, show it */
	    if (activeCell || numBytes) {
		register int x0 = x + bd[0] + padx;
		register int y0 = y + bd[2] + pady;

		/* get the dimensions of the string */
		textLayout = Tk_ComputeTextLayout(tagPtr->tkfont,
			string, numBytes,
			(tagPtr->wrap > 0) ? width : 0, tagPtr->justify,
			(tagPtr->multiline > 0) ? 0 : TK_IGNORE_NEWLINES,
			&itemW, &itemH);

		/*
		 * Set the origin coordinates of the string to draw using
		 * the anchor.  origin represents the (x,y) coordinate of
		 * the lower left corner of the text box, relative to the
		 * internal (inside the border) window
		 */

		/* set the X origin first */
		switch (tagPtr->anchor) {
		    case TK_ANCHOR_NW:
		    case TK_ANCHOR_W:
		    case TK_ANCHOR_SW:		/* western position */
			originX = ipadx;
			break;
		    case TK_ANCHOR_N:
		    case TK_ANCHOR_S:
		    case TK_ANCHOR_CENTER:	/* centered position */
			originX = (width - itemW) / 2;
			break;
		    default:			/* eastern position */
			originX = width - itemW - ipadx;
		}

		/* then set the Y origin */
		switch (tagPtr->anchor) {
		    case TK_ANCHOR_N:
		    case TK_ANCHOR_NE:
		    case TK_ANCHOR_NW:		/* northern position */
			originY = ipady;
			break;
		    case TK_ANCHOR_W:
		    case TK_ANCHOR_E:
		    case TK_ANCHOR_CENTER:	/* centered position */
			originY = (height - itemH) / 2;
			break;
		    default:			/* southern position */
			originY = height - itemH - ipady;
		}

		/*
		 * If this is the active cell and we are editing,
		 * ensure that the cursor will be displayed
		 */
		if (activeCell) {
		    Tk_CharBbox(textLayout, tablePtr->icursor,
			    &cx, &cy, &cw, &ch);
		    /* we have to fudge with maxW because of odd width
		     * determination for newlines at the end of a line */
		    maxW = width - tablePtr->insertWidth
			- (cx + MIN(tablePtr->charWidth, cw));
		    maxH = height - (cy + ch);
		    if (originX < bd[0] - cx) {
			/* cursor off cell to the left */
			/* use western positioning to cet cursor at left
			 * with slight variation to show some text */
			originX = bd[0] - cx
			    + MIN(cx, width - tablePtr->insertWidth);
		    } else if (originX > maxW) {
			/* cursor off cell to the right */
			/* use eastern positioning to cet cursor at right */
			originX = maxW;
		    }
		    if (originY < bd[2] - cy) {
			/* cursor before top of cell */
			/* use northern positioning to cet cursor at top */
			originY = bd[2] - cy;
		    } else if (originY > maxH) {
			/* cursor beyond bottom of cell */
			/* use southern positioning to cet cursor at bottom */
			originY = maxH;
		    }
		    tablePtr->activeTagPtr	= tagPtr;
		    tablePtr->activeX		= originX;
		    tablePtr->activeY		= originY;
		}

		/*
		 * Use a clip rectangle only if necessary as it means
		 * updating the GC in the server which slows everything down.
		 * We can't fudge the width or height, just in case the user
		 * wanted empty pad space.
		 */
		if ((originX < 0) || (originY < 0) ||
			(originX+itemW > width) || (originY+itemH > height)) {
		    if (!activeCell
			    && (tagPtr->ellipsis != NULL)
			    && (tagPtr->wrap <= 0)
			    && (tagPtr->multiline <= 0)
			) {
			/*
			 * Check which side to draw ellipsis on
			 */
			switch (tagPtr->anchor) {
			    case TK_ANCHOR_NE:
			    case TK_ANCHOR_E:
			    case TK_ANCHOR_SE:		/* eastern position */
				ellEast = 0;
				break;
			    default:			/* western position */
				ellEast = 1;
			}
			if ((ellipsis != tagPtr->ellipsis)
				|| (ellFont != tagPtr->tkfont)) {
			    /*
			     * Different ellipsis from last cached
			     */
			    ellFont  = tagPtr->tkfont;
			    ellipsis = tagPtr->ellipsis;
			    ellLen = Tk_TextWidth(ellFont,
				    ellipsis, (int) strlen(ellipsis));
			    Tk_GetFontMetrics(tagPtr->tkfont, &fm);
			}
			useEllLen = MIN(ellLen, width);
		    } else {
			ellEast = 0;
			useEllLen = 0;
		    }

		    /*
		     * The text wants to overflow the boundaries of the
		     * displayed cell, so we must clip in some way
		     */
#ifdef NO_XSETCLIP
		    /*
		     * This code is basically for the Macintosh.
		     * Copy the the current contents of the cell into the
		     * clipped window area.  This keeps any fg/bg and image
		     * data intact.
		     * x0 - x == pad area
		     */
		    XCopyArea(display, window, clipWind, tagGc, x0, y0,
			    width, height, x0 - x, y0 - y);
		    /*
		     * Now draw into the cell space on the special window.
		     * Don't use x,y base offset for clipWind.
		     */
		    Tk_DrawTextLayout(display, clipWind, tagGc, textLayout,
			    x0 - x + originX, y0 - y + originY, 0, -1);

		    if (useEllLen) {
			/*
			 * Recopy area the ellipse covers (not efficient)
			 */
			XCopyArea(display, window, clipWind, tagGc,
				x0 + (ellEast ? width - useEllLen : 0), y0,
				useEllLen, height,
				x0 - x + (ellEast ? width - useEllLen : 0),
				y0 - y);
			Tk_DrawChars(display, clipWind, tagGc, ellFont,
				ellipsis, (int) strlen(ellipsis),
				x0 - x + (ellEast ? width - useEllLen : 0),
				y0 - y + originY + fm.ascent);
		    }
		    /*
		     * Now copy back only the area that we want the
		     * text to be drawn on.
		     */
		    XCopyArea(display, clipWind, window, tagGc,
			    x0 - x, y0 - y, width, height, x0, y0);
#elif defined(WIN32)
		    /*
		     * This is evil, evil evil! but the XCopyArea
		     * doesn't work in all cases - Michael Teske.
		     * The general structure follows the comments below.
		     */
		    twdPtr = (TkWinDrawable *) window;
		    dc     = GetDC(twdPtr->window.handle);

		    clipR = CreateRectRgn(x0 + (ellEast ? 0 : useEllLen), y0,
			x0 + width - (ellEast ? useEllLen : 0), y0 + height);

		    SelectClipRgn(dc, clipR);
		    DeleteObject(clipR);
		    /* OffsetClipRgn(dc, 0, 0); */

		    Tk_DrawTextLayout(display, window, tagGc, textLayout,
			    x0 + originX, y0 + originY, 0, -1);

		    if (useEllLen) {
			clipR = CreateRectRgn(x0, y0, x0 + width, y0 + height);
			SelectClipRgn(dc, clipR);
			DeleteObject(clipR);
			Tk_DrawChars(display, window, tagGc, ellFont,
				ellipsis, (int) strlen(ellipsis),
				x0 + (ellEast? width-useEllLen : 0),
				y0 + originY + fm.ascent);
		    }
		    SelectClipRgn(dc, NULL);
		    ReleaseDC(twdPtr->window.handle, dc);
#else
		    /*
		     * Use an X clipping rectangle.  The clipping is the
		     * rectangle just for the actual text space (to allow
		     * for empty padding space).
		     */
		    clipRect.x      = x0 + (ellEast ? 0 : useEllLen);
		    clipRect.y      = y0;
		    clipRect.width  = width - (ellEast ? useEllLen : 0);
		    clipRect.height = height;
		    XSetClipRectangles(display, tagGc, 0, 0, &clipRect, 1,
			    Unsorted);
		    Tk_DrawTextLayout(display, window, tagGc, textLayout,
			    x0 + originX,
			    y0 + originY, 0, -1);
		    if (useEllLen) {
			clipRect.x     = x0;
			clipRect.width = width;
			XSetClipRectangles(display, tagGc, 0, 0, &clipRect, 1,
				Unsorted);
			Tk_DrawChars(display, window, tagGc, ellFont,
				ellipsis, (int) strlen(ellipsis),
				x0 + (ellEast? width-useEllLen : 0),
				y0 + originY + fm.ascent);
		    }
		    XSetClipMask(display, tagGc, None);
#endif
		} else {
		    Tk_DrawTextLayout(display, window, tagGc, textLayout,
			    x0 + originX, y0 + originY, 0, -1);
		}

		/* if this is the active cell draw the cursor if it's on.
		 * this ignores clip rectangles. */
		if (activeCell && (tablePtr->flags & CURSOR_ON) &&
			(originY + cy + bd[2] + pady < height) &&
			(originX + cx + bd[0] + padx -
				(tablePtr->insertWidth / 2) >= 0)) {
		    /* make sure it will fit in the box */
		    maxW = MAX(0, originY + cy + bd[2] + pady);
		    maxH = MIN(ch, height - maxW + bd[2] + pady);
		    Tk_Fill3DRectangle(tkwin, window, tablePtr->insertBg,
			    x0 + originX + cx - (tablePtr->insertWidth/2),
			    y + maxW, tablePtr->insertWidth,
			    maxH, 0, TK_RELIEF_FLAT);
		}
	    }

	    /*
	     * Re-Correct the dimensions before border drawing
	     */
	    width  += bd[0] + bd[1] + (2 * padx);
	    height += bd[2] + bd[3] + (2 * pady);

	    DrawBorder:
	    /* Draw the 3d border on the pixmap correctly offset */
	    if (tablePtr->drawMode == DRAW_MODE_SINGLE) {
		topGc = Tk_3DBorderGC(tkwin, tagPtr->bg, TK_3D_DARK_GC);
		/* draw a line with single pixel width */
		rect[0].x = x;
		rect[0].y = y + height - 1;
		rect[1].y = -height + 1;
		rect[2].x = width - 1;
		XDrawLines(display, window, topGc, rect, 3, CoordModePrevious);
	    } else if (tablePtr->drawMode == DRAW_MODE_FAST) {
		/*
		 * This depicts a full 1 pixel border.
		 *
		 * Choose the GCs to get the best approximation
		 * to the desired drawing style.
		 */
		switch(tagPtr->relief) {
		    case TK_RELIEF_FLAT:
			topGc = bottomGc = Tk_3DBorderGC(tkwin, tagPtr->bg,
				TK_3D_FLAT_GC);
			break;
		    case TK_RELIEF_RAISED:
		    case TK_RELIEF_RIDGE:
			topGc    = Tk_3DBorderGC(tkwin, tagPtr->bg,
				TK_3D_LIGHT_GC);
			bottomGc = Tk_3DBorderGC(tkwin, tagPtr->bg,
				TK_3D_DARK_GC);
			break;
		    default: /* TK_RELIEF_SUNKEN TK_RELIEF_GROOVE */
			bottomGc = Tk_3DBorderGC(tkwin, tagPtr->bg,
				TK_3D_LIGHT_GC);
			topGc    = Tk_3DBorderGC(tkwin, tagPtr->bg,
				TK_3D_DARK_GC);
			break;
		}
	
		/* draw a line with single pixel width */
		rect[0].x = x + width - 1;
		rect[0].y = y;
		rect[1].y = height - 1;
		rect[2].x = -width + 1;
		XDrawLines(display, window, bottomGc, rect, 3,
			CoordModePrevious);
		rect[0].x = x;
		rect[0].y = y + height - 1;
		rect[1].y = -height + 1;
		rect[2].x = width - 1;
		XDrawLines(display, window, topGc, rect, 3,
			CoordModePrevious);
	    } else {
		if (borders > 1) {
		    if (bd[0]) {
			Tk_3DVerticalBevel(tkwin, window, tagPtr->bg,
				x, y, bd[0], height,
				1 /* left side */, tagPtr->relief);
		    }
		    if (bd[1]) {
			Tk_3DVerticalBevel(tkwin, window, tagPtr->bg,
				x + width - bd[1], y, bd[1], height,
				0 /* right side */, tagPtr->relief);
		    }
		    if ((borders == 4) && bd[2]) {
			Tk_3DHorizontalBevel(tkwin, window, tagPtr->bg,
				x, y, width, bd[2],
				1, 1, 1 /* top */, tagPtr->relief);
		    }
		    if ((borders == 4) && bd[3]) {
			Tk_3DHorizontalBevel(tkwin, window, tagPtr->bg,
				x, y + height - bd[3], width, bd[3],
				0, 0, 0 /* bottom */, tagPtr->relief);
		    }
		} else if (borders == 1) {
		    Tk_Draw3DRectangle(tkwin, window, tagPtr->bg, x, y,
			    width, height, bd[0], tagPtr->relief);
		}
	    }

	    /* clean up the necessaries */
	    if (tagPtr == tablePtr->activeTagPtr) {
		/*
		 * This means it was the activeCell with text displayed.
		 * We buffer the active tag for the 'activate' command.
		 */
		tablePtr->activeTagPtr = TableNewTag(NULL);
		memcpy((VOID *) tablePtr->activeTagPtr,
			(VOID *) tagPtr, sizeof(TableTag));
	    }
	    if (textLayout) {
		Tk_FreeTextLayout(textLayout);
		textLayout = NULL;
	    }
	    if (cellType == CELL_HIDDEN) {
		/* the last cell was a hidden one,
		 * rework row stuff back to normal */
		row = hrow; col = hcol;
		urow = row+tablePtr->rowOffset;
		rowPtr = FindRowColTag(tablePtr, urow, ROW);
	    }
	}
    }
    ckfree((char *) tagPtr);
#ifdef NO_XSETCLIP
    Tk_FreePixmap(display, clipWind);
#endif

    /* Take care of removing embedded windows that are no longer in view */
    TableUndisplay(tablePtr);

#ifndef WIN32
    /* copy over and delete the pixmap if we are in slow mode */
    if (tablePtr->drawMode == DRAW_MODE_SLOW) {
	/* Get a default valued GC */
	TableGetGc(display, window, &(tablePtr->defaultTag), &tagGc);
	XCopyArea(display, window, Tk_WindowId(tkwin), tagGc, 0, 0,
		(unsigned) invalidWidth, (unsigned) invalidHeight,
		invalidX, invalidY);
	Tk_FreePixmap(display, window);
	window = Tk_WindowId(tkwin);
    }
#endif

    /* 
     * If we are at the end of the table, clear the area after the last
     * row/col.  We discount spans here because we just need the coords
     * for the area that would be the last physical cell.
     */
    tablePtr->flags |= AVOID_SPANS;
    TableCellCoords(tablePtr, tablePtr->rows-1, tablePtr->cols-1,
	    &x, &y, &width, &height);
    tablePtr->flags &= ~AVOID_SPANS;

    /* This should occur before moving pixmap, but this simplifies things
     *
     * Could use Tk_Fill3DRectangle instead of XFillRectangle
     * for best compatibility, and XClearArea could be used on Unix
     * for best speed, so this is the compromise w/o #ifdef's
     */
    if (x+width < invalidX+invalidWidth) {
	XFillRectangle(display, window,
		Tk_3DBorderGC(tkwin, tablePtr->defaultTag.bg, TK_3D_FLAT_GC),
		x+width, invalidY, (unsigned) invalidX+invalidWidth-x-width,
		(unsigned) invalidHeight);
    }

    if (y+height < invalidY+invalidHeight) {
	XFillRectangle(display, window,
		Tk_3DBorderGC(tkwin, tablePtr->defaultTag.bg, TK_3D_FLAT_GC),
		invalidX, y+height, (unsigned) invalidWidth,
		(unsigned) invalidY+invalidHeight-y-height);
    }

    if (tagGc != NULL) {
	TableFreeGc(display, tagGc);
    }
    TableRedrawHighlight(tablePtr);
    /* 
     * Free the hash table used to cache evaluations.
     */
    Tcl_DeleteHashTable(colTagsCache);
    ckfree((char *) (colTagsCache));
    Tcl_DeleteHashTable(drawnCache);
    ckfree((char *) (drawnCache));
}

/* 
 *----------------------------------------------------------------------
 *
 * TableInvalidate --
 *	Invalidates a rectangle and adds it to the total invalid rectangle
 *	waiting to be redrawn.  If the INV_FORCE flag bit is set,
 *	it does an update instantly else waits until Tk is idle.
 *
 * Results:
 *	Will schedule table (re)display.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */
void
TableInvalidate(Table * tablePtr, int x, int y,
		int w, int h, int flags)
{
    Tk_Window tkwin = tablePtr->tkwin;
    int hl	= tablePtr->highlightWidth;
    int height	= Tk_Height(tkwin);
    int width	= Tk_Width(tkwin);

    /*
     * Make sure that the window hasn't been destroyed already.
     * Avoid allocating 0 sized pixmaps which would be fatal,
     * and check if rectangle is even on the screen.
     */
    if ((tkwin == NULL)
	    || (w <= 0) || (h <= 0) || (x > width) || (y > height)) {
	return;
    }

    /* If not even mapped, wait for the remap to redraw all */
    if (!Tk_IsMapped(tkwin)) {
	tablePtr->flags |= REDRAW_ON_MAP;
	return;
    }

    /*
     * If no pending updates exist, then replace the rectangle.
     * Otherwise find the bounding rectangle.
     */
    if ((flags & INV_HIGHLIGHT) &&
	    (x < hl || y < hl || x+w >= width-hl || y+h >= height-hl)) {
	tablePtr->flags |= REDRAW_BORDER;
    }

    if (tablePtr->flags & REDRAW_PENDING) {
	tablePtr->invalidWidth = MAX(x + w,
		tablePtr->invalidX+tablePtr->invalidWidth);
	tablePtr->invalidHeight = MAX(y + h,
		tablePtr->invalidY+tablePtr->invalidHeight);
	if (tablePtr->invalidX > x) tablePtr->invalidX = x;
	if (tablePtr->invalidY > y) tablePtr->invalidY = y;
	tablePtr->invalidWidth  -= tablePtr->invalidX;
	tablePtr->invalidHeight -= tablePtr->invalidY;
	/* Do we want to force this update out? */
	if (flags & INV_FORCE) {
	    Tcl_CancelIdleCall(TableDisplay, (ClientData) tablePtr);
	    TableDisplay((ClientData) tablePtr);
	}
    } else {
	tablePtr->invalidX = x;
	tablePtr->invalidY = y;
	tablePtr->invalidWidth = w;
	tablePtr->invalidHeight = h;
	if (flags & INV_FORCE) {
	    TableDisplay((ClientData) tablePtr);
	} else {
	    tablePtr->flags |= REDRAW_PENDING;
	    Tcl_DoWhenIdle(TableDisplay, (ClientData) tablePtr);
	}
    }
}

/* 
 *----------------------------------------------------------------------
 *
 * TableFlashEvent --
 *	Called when the flash timer goes off.
 *
 * Results:
 *	Decrements all the entries in the hash table and invalidates
 *	any cells that expire, deleting them from the table.  If the
 *	table is now empty, stops the timer, else reenables it.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
TableFlashEvent(ClientData clientdata)
{
    Table *tablePtr = (Table *) clientdata;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    int entries, count, row, col;

    entries = 0;
    for (entryPtr = Tcl_FirstHashEntry(tablePtr->flashCells, &search);
	 entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&search)) {
	count = (int) Tcl_GetHashValue(entryPtr);
	if (--count <= 0) {
	    /* get the cell address and invalidate that region only */
	    TableParseArrayIndex(&row, &col,
		    Tcl_GetHashKey(tablePtr->flashCells, entryPtr));

	    /* delete the entry from the table */
	    Tcl_DeleteHashEntry(entryPtr);

	    TableRefresh(tablePtr, row-tablePtr->rowOffset,
		    col-tablePtr->colOffset, CELL);
	} else {
	    Tcl_SetHashValue(entryPtr, (ClientData) count);
	    entries++;
	}
    }

    /* do I need to restart the timer */
    if (entries && tablePtr->flashMode) {
	tablePtr->flashTimer = Tcl_CreateTimerHandler(250, TableFlashEvent,
		(ClientData) tablePtr);
    } else {
	tablePtr->flashTimer = 0;
    }
}

/* 
 *----------------------------------------------------------------------
 *
 * TableAddFlash --
 *	Adds a flash on cell row,col (real coords) with the default timeout
 *	if flashing is enabled and flashtime > 0.
 *
 * Results:
 *	Cell will flash.
 *
 * Side effects:
 *	Will start flash timer if it didn't exist.
 *
 *----------------------------------------------------------------------
 */
void
TableAddFlash(Table *tablePtr, int row, int col)
{
    char buf[INDEX_BUFSIZE];
    int dummy;
    Tcl_HashEntry *entryPtr;

    if (!tablePtr->flashMode || tablePtr->flashTime < 1) {
	return;
    }

    /* create the array index in user coords */
    TableMakeArrayIndex(row+tablePtr->rowOffset, col+tablePtr->colOffset, buf);

    /* add the flash to the hash table */
    entryPtr = Tcl_CreateHashEntry(tablePtr->flashCells, buf, &dummy);
    Tcl_SetHashValue(entryPtr, tablePtr->flashTime);

    /* now set the timer if it's not already going and invalidate the area */
    if (tablePtr->flashTimer == NULL) {
	tablePtr->flashTimer = Tcl_CreateTimerHandler(250, TableFlashEvent,
		(ClientData) tablePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TableSetActiveIndex --
 *	Sets the "active" index of the associated array to the current
 *	value of the active buffer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Traces on the array can cause side effects.
 *
 *----------------------------------------------------------------------
 */
void
TableSetActiveIndex(register Table *tablePtr)
{
    if (tablePtr->arrayVar) {
	tablePtr->flags |= SET_ACTIVE;
	Tcl_SetVar2(tablePtr->interp, tablePtr->arrayVar, "active",
		tablePtr->activeBuf, TCL_GLOBAL_ONLY);
	tablePtr->flags &= ~SET_ACTIVE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TableGetActiveBuf --
 *	Get the current selection into the buffer and mark it as unedited.
 *	Set the position to the end of the string.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	tablePtr->activeBuf will change.
 *
 *----------------------------------------------------------------------
 */
void
TableGetActiveBuf(register Table *tablePtr)
{
    char *data = "";

    if (tablePtr->flags & HAS_ACTIVE) {
	data = TableGetCellValue(tablePtr,
		tablePtr->activeRow+tablePtr->rowOffset,
		tablePtr->activeCol+tablePtr->colOffset);
    }

    if (STREQ(tablePtr->activeBuf, data)) {
	/* this forced SetActiveIndex is necessary if we change array vars and
	 * they happen to have these cells equal, we won't properly set the
	 * active index for the new array var unless we do this here */
	TableSetActiveIndex(tablePtr);
	return;
    }
    /* is the buffer long enough */
    tablePtr->activeBuf = (char *)ckrealloc(tablePtr->activeBuf,
	    strlen(data)+1);
    strcpy(tablePtr->activeBuf, data);
    TableGetIcursor(tablePtr, "end", (int *)0);
    tablePtr->flags &= ~TEXT_CHANGED;
    TableSetActiveIndex(tablePtr);
}

/* 
 *----------------------------------------------------------------------
 *
 * TableVarProc --
 *	This is the trace procedure associated with the Tcl array.  No
 *	validation will occur here because this only triggers when the
 *	array value is directly set, and we can't maintain the old value.
 *
 * Results:
 *	Invalidates changed cell.
 *
 * Side effects:
 *	Creates/Updates entry in the cache if we are caching.
 *
 *----------------------------------------------------------------------
 */
static char *
TableVarProc(clientData, interp, name, index, flags)
     ClientData clientData;	/* Information about table. */
     Tcl_Interp *interp;		/* Interpreter containing variable. */
     char *name;			/* Not used. */
     char *index;		/* Not used. */
     int flags;			/* Information about what happened. */
{
    Table *tablePtr = (Table *) clientData;
    int row, col, update = 1;

    /* This is redundant, as the name should always == arrayVar */
    name = tablePtr->arrayVar;

    /* is this the whole var being destroyed or just one cell being deleted */
    if ((flags & TCL_TRACE_UNSETS) && index == NULL) {
	/* if this isn't the interpreter being destroyed reinstate the trace */
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_SetVar2(interp, name, TEST_KEY, "", TCL_GLOBAL_ONLY);
	    Tcl_UnsetVar2(interp, name, TEST_KEY, TCL_GLOBAL_ONLY);
	    Tcl_ResetResult(interp);

	    /* set a trace on the variable */
	    Tcl_TraceVar(interp, name,
		    TCL_TRACE_WRITES | TCL_TRACE_UNSETS | TCL_GLOBAL_ONLY,
		    (Tcl_VarTraceProc *)TableVarProc, (ClientData) tablePtr);

	    /* only do the following if arrayVar is our data source */
	    if (tablePtr->dataSource & DATA_ARRAY) {
		/* clear the selection buffer */
		TableGetActiveBuf(tablePtr);
		/* flush any cache */
		Table_ClearHashTable(tablePtr->cache);
		Tcl_InitHashTable(tablePtr->cache, TCL_STRING_KEYS);
		/* and invalidate the table */
		TableInvalidateAll(tablePtr, 0);
	    }
	}
	return (char *)NULL;
    }
    /* only continue if arrayVar is our data source */
    if (!(tablePtr->dataSource & DATA_ARRAY)) {
	return (char *)NULL;
    }
    /* get the cell address and invalidate that region only.
     * Make sure that it is a valid cell address. */
    if (STREQ("active", index)) {
	if (tablePtr->flags & SET_ACTIVE) {
	    /* If we are already setting the active cell, the update
	     * will occur in other code */
	    update = 0;
	} else {
	    /* modified TableGetActiveBuf */
	    CONST char *data = "";

	    row = tablePtr->activeRow;
	    col = tablePtr->activeCol;
	    if (tablePtr->flags & HAS_ACTIVE)
		data = Tcl_GetVar2(interp, name, index, TCL_GLOBAL_ONLY);
	    if (!data) data = "";

	    if (STREQ(tablePtr->activeBuf, data)) {
		return (char *)NULL;
	    }
	    tablePtr->activeBuf = (char *)ckrealloc(tablePtr->activeBuf,
		    strlen(data)+1);
	    strcpy(tablePtr->activeBuf, data);
	    /* set cursor to the last char */
	    TableGetIcursor(tablePtr, "end", (int *)0);
	    tablePtr->flags |= TEXT_CHANGED;
	}
    } else if (TableParseArrayIndex(&row, &col, index) == 2) {
	char buf[INDEX_BUFSIZE];

	/* Make sure it won't trigger on array(2,3extrastuff) */
	TableMakeArrayIndex(row, col, buf);
	if (strcmp(buf, index)) {
	    return (char *)NULL;
	}
	if (tablePtr->caching) {
	    Tcl_HashEntry *entryPtr;
	    int new;
	    char *val, *data;

	    entryPtr = Tcl_CreateHashEntry(tablePtr->cache, buf, &new);
	    if (!new) {
		data = (char *) Tcl_GetHashValue(entryPtr);
		if (data) { ckfree(data); }
	    }
	    data = (char *) Tcl_GetVar2(interp, name, index, TCL_GLOBAL_ONLY);
	    if (data && *data != '\0') {
		val = (char *)ckalloc(strlen(data)+1);
		strcpy(val, data);
	    } else {
		val = NULL;
	    }
	    Tcl_SetHashValue(entryPtr, val);
	}
	/* convert index to real coords */
	row -= tablePtr->rowOffset;
	col -= tablePtr->colOffset;
	/* did the active cell just update */
	if (row == tablePtr->activeRow && col == tablePtr->activeCol) {
	    TableGetActiveBuf(tablePtr);
	}
	/* Flash the cell */
	TableAddFlash(tablePtr, row, col);
    } else {
	return (char *)NULL;
    }

    if (update) {
	TableRefresh(tablePtr, row, col, CELL);
    }

    return (char *)NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TableGeometryRequest --
 *	This procedure is invoked to request a new geometry from Tk.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Geometry information is updated and a new requested size is
 *	registered for the widget.  Internal border info is also set.
 *
 *----------------------------------------------------------------------
 */
void
TableGeometryRequest(tablePtr)
     register Table *tablePtr;
{
    int x, y;

    /* Do the geometry request
     * If -width #cols was not specified or it is greater than the real
     * number of cols, use maxWidth as a lower bound, with the other lower
     * bound being the upper bound of the window's user-set width and the
     * value of -maxwidth set by the programmer
     * Vice versa for rows/height
     */
    x = MIN((tablePtr->maxReqCols==0 || tablePtr->maxReqCols > tablePtr->cols)?
	    tablePtr->maxWidth : tablePtr->colStarts[tablePtr->maxReqCols],
	    tablePtr->maxReqWidth) + 2*tablePtr->highlightWidth;
    y = MIN((tablePtr->maxReqRows==0 || tablePtr->maxReqRows > tablePtr->rows)?
	    tablePtr->maxHeight : tablePtr->rowStarts[tablePtr->maxReqRows],
	    tablePtr->maxReqHeight) + 2*tablePtr->highlightWidth;
    Tk_GeometryRequest(tablePtr->tkwin, x, y);
}

/*
 *----------------------------------------------------------------------
 *
 * TableAdjustActive --
 *	This procedure is called by AdjustParams and CMD_ACTIVATE to
 *	move the active cell.
 *
 * Results:
 *	Old and new active cell indices will be invalidated.
 *
 * Side effects:
 *	If the old active cell index was edited, it will be saved.
 *	The active buffer will be updated.
 *
 *----------------------------------------------------------------------
 */
void
TableAdjustActive(tablePtr)
     register Table *tablePtr;		/* Widget record for table */
{
    if (tablePtr->flags & HAS_ACTIVE) {
	/*
	 * Make sure the active cell has a reasonable real index
	 */
	CONSTRAIN(tablePtr->activeRow, 0, tablePtr->rows-1);
	CONSTRAIN(tablePtr->activeCol, 0, tablePtr->cols-1);
    }

    /*
     * Check the new value of active cell against the original,
     * Only invalidate if it changed.
     */
    if (tablePtr->oldActRow == tablePtr->activeRow &&
	    tablePtr->oldActCol == tablePtr->activeCol) {
	return;
    }

    if (tablePtr->oldActRow >= 0 && tablePtr->oldActCol >= 0) {
	/* 
	 * Set the value of the old active cell to the active buffer
	 * SetCellValue will check if the value actually changed
	 */
	if (tablePtr->flags & TEXT_CHANGED) {
	    /* WARNING an outside trace will be triggered here and if it
	     * calls something that causes TableAdjustParams to be called
	     * again, we are in data consistency trouble */
	    /* HACK - turn TEXT_CHANGED off now to possibly avoid the
	     * above data inconsistency problem.  */
	    tablePtr->flags &= ~TEXT_CHANGED;
	    TableSetCellValue(tablePtr,
		    tablePtr->oldActRow + tablePtr->rowOffset,
		    tablePtr->oldActCol + tablePtr->colOffset,
		    tablePtr->activeBuf);
	}
	/*
	 * Invalidate the old active cell
	 */
	TableRefresh(tablePtr, tablePtr->oldActRow, tablePtr->oldActCol, CELL);
    }

    /*
     * Store the new active cell value into the active buffer
     */
    TableGetActiveBuf(tablePtr);

    /*
     * Invalidate the new active cell
     */
    TableRefresh(tablePtr, tablePtr->activeRow, tablePtr->activeCol, CELL);

    /*
     * Cache the old active row/col for the next time this is called
     */
    tablePtr->oldActRow = tablePtr->activeRow;
    tablePtr->oldActCol = tablePtr->activeCol;
}

/*
 *----------------------------------------------------------------------
 *
 * TableAdjustParams --
 *	Calculate the row and column starts.  Adjusts the topleft corner
 *	variable to keep it within the screen range, out of the titles
 *	and keep the screen full make sure the selected cell is in the
 *	visible area checks to see if the top left cell has changed at
 *	all and invalidates the table if it has.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Number of rows can change if -rowstretchmode == fill.
 *	topRow && leftCol can change to fit display.
 *	activeRow/Col can change to ensure it is a valid cell.
 *
 *----------------------------------------------------------------------
 */
void
TableAdjustParams(register Table *tablePtr)
{
    int topRow, leftCol, row, col, total, i, value, x, y, width, height,
	w, h, hl, px, py, recalc, bd[4],
	diff, unpreset, lastUnpreset, pad, lastPad, numPixels,
	defColWidth, defRowHeight;
    Tcl_HashEntry *entryPtr;

    /*
     * Cache some values for many upcoming calculations
     */
    hl = tablePtr->highlightWidth;
    w  = Tk_Width(tablePtr->tkwin) - (2 * hl);
    h  = Tk_Height(tablePtr->tkwin) - (2 * hl);
    TableGetTagBorders(&(tablePtr->defaultTag),
	    &bd[0], &bd[1], &bd[2], &bd[3]);
    px = bd[0] + bd[1] + (2 * tablePtr->padX);
    py = bd[2] + bd[3] + (2 * tablePtr->padY);

    /*
     * Account for whether default dimensions are in chars (>0) or
     * pixels (<=0).  Border and Pad space is added in here for convenience.
     *
     * When a value in pixels is specified, we take that exact amount,
     * not adding in padding.
     */
    if (tablePtr->defColWidth > 0) {
	defColWidth = tablePtr->charWidth * tablePtr->defColWidth + px;
    } else {
	defColWidth = -(tablePtr->defColWidth);
    }
    if (tablePtr->defRowHeight > 0) {
	defRowHeight = tablePtr->charHeight * tablePtr->defRowHeight + py;
    } else {
	defRowHeight = -(tablePtr->defRowHeight);
    }

    /*
     * Set up the arrays to hold the col pixels and starts.
     * ckrealloc was fixed in 8.2.1 to handle NULLs, so we can't rely on it.
     */
    if (tablePtr->colPixels) ckfree((char *) tablePtr->colPixels);
    tablePtr->colPixels = (int *) ckalloc(tablePtr->cols * sizeof(int));
    if (tablePtr->colStarts) ckfree((char *) tablePtr->colStarts);
    tablePtr->colStarts = (int *) ckalloc((tablePtr->cols+1) * sizeof(int));

    /*
     * Get all the preset columns and set their widths
     */
    lastUnpreset = 0;
    numPixels = 0;
    unpreset = 0;
    for (i = 0; i < tablePtr->cols; i++) {
	entryPtr = Tcl_FindHashEntry(tablePtr->colWidths, (char *) i);
	if (entryPtr == NULL) {
	    tablePtr->colPixels[i] = -1;
	    unpreset++;
	    lastUnpreset = i;
	} else {
	    value = (int) Tcl_GetHashValue(entryPtr);
	    if (value > 0) {
		tablePtr->colPixels[i] = value * tablePtr->charWidth + px;
	    } else {
		/*
		 * When a value in pixels is specified, we take that exact
		 * amount, not adding in pad or border values.
		 */
		tablePtr->colPixels[i] = -value;
	    }
	    numPixels += tablePtr->colPixels[i];
	}
    }

    /*
     * Work out how much to pad each col depending on the mode.
     */
    diff  = w - numPixels - (unpreset * defColWidth);
    total = 0;

    /*
     * Now do the padding and calculate the column starts.
     * Diff lower than 0 means we can't see the entire set of columns,
     * thus no special stretching will occur & we optimize the calculation.
     */
    if (diff <= 0) {
	for (i = 0; i < tablePtr->cols; i++) {
	    if (tablePtr->colPixels[i] == -1) {
		tablePtr->colPixels[i] = defColWidth;
	    }
	    tablePtr->colStarts[i] = total;
	    total += tablePtr->colPixels[i];
	}
    } else {
	switch (tablePtr->colStretch) {
	case STRETCH_MODE_NONE:
	    pad		= 0;
	    lastPad	= 0;
	    break;
	case STRETCH_MODE_UNSET:
	    if (unpreset == 0) {
		pad	= 0;
		lastPad	= 0;
	    } else {
		pad	= diff / unpreset;
		lastPad	= diff - pad * (unpreset - 1);
	    }
	    break;
	case STRETCH_MODE_LAST:
	    pad		= 0;
	    lastPad	= diff;
	    lastUnpreset = tablePtr->cols - 1;
	    break;
	default:	/* STRETCH_MODE_ALL, but also FILL for cols */
	    pad		= diff / tablePtr->cols;
	    /* force it to be applied to the last column too */
	    lastUnpreset = tablePtr->cols - 1;
	    lastPad	= diff - pad * lastUnpreset;
	}

	for (i = 0; i < tablePtr->cols; i++) {
	    if (tablePtr->colPixels[i] == -1) {
		tablePtr->colPixels[i] = defColWidth
		    + ((i == lastUnpreset) ? lastPad : pad);
	    } else if (tablePtr->colStretch == STRETCH_MODE_ALL) {
		tablePtr->colPixels[i] += (i == lastUnpreset) ? lastPad : pad;
	    }
	    tablePtr->colStarts[i] = total;
	    total += tablePtr->colPixels[i];
	}
    }
    tablePtr->colStarts[i] = tablePtr->maxWidth = total;

    /*
     * The 'do' loop is only necessary for rows because of FILL mode
     */
    recalc = 0;
    do {
	/* Set up the arrays to hold the row pixels and starts */
	/* FIX - this can be moved outside 'do' if you check >row size */
	if (tablePtr->rowPixels) ckfree((char *) tablePtr->rowPixels);
	tablePtr->rowPixels = (int *) ckalloc(tablePtr->rows * sizeof(int));

	/* get all the preset rows and set their heights */
	lastUnpreset	= 0;
	numPixels	= 0;
	unpreset	= 0;
	for (i = 0; i < tablePtr->rows; i++) {
	    entryPtr = Tcl_FindHashEntry(tablePtr->rowHeights, (char *) i);
	    if (entryPtr == NULL) {
		tablePtr->rowPixels[i] = -1;
		unpreset++;
		lastUnpreset = i;
	    } else {
		value = (int) Tcl_GetHashValue(entryPtr);
		if (value > 0) {
		    tablePtr->rowPixels[i] = value * tablePtr->charHeight + py;
		} else {
		    /*
		     * When a value in pixels is specified, we take that exact
		     * amount, not adding in pad or border values.
		     */
		    tablePtr->rowPixels[i] = -value;
		}
		numPixels += tablePtr->rowPixels[i];
	    }
	}

	/* work out how much to pad each row depending on the mode */
	diff = h - numPixels - (unpreset * defRowHeight);
	switch(tablePtr->rowStretch) {
	case STRETCH_MODE_NONE:
	    pad		= 0;
	    lastPad	= 0;
	    break;
	case STRETCH_MODE_UNSET:
	    if (unpreset == 0)  {
		pad	= 0;
		lastPad	= 0;
	    } else {
		pad	= MAX(0,diff) / unpreset;
		lastPad	= MAX(0,diff) - pad * (unpreset - 1);
	    }
	    break;
	case STRETCH_MODE_LAST:
	    pad		= 0;
	    lastPad	= MAX(0,diff);
	    /* force it to be applied to the last column too */
	    lastUnpreset = tablePtr->rows - 1;
	    break;
	case STRETCH_MODE_FILL:
	    pad		= 0;
	    lastPad	= diff;
	    if (diff && !recalc) {
		tablePtr->rows += (diff/defRowHeight);
		if (diff < 0 && tablePtr->rows <= 0) {
		    tablePtr->rows = 1;
		}
		lastUnpreset = tablePtr->rows - 1;
		recalc = 1;
		continue;
	    } else {
		lastUnpreset = tablePtr->rows - 1;
		recalc = 0;
	    }
	    break;
	default:	/* STRETCH_MODE_ALL */
	    pad		= MAX(0,diff) / tablePtr->rows;
	    /* force it to be applied to the last column too */
	    lastUnpreset = tablePtr->rows - 1;
	    lastPad	= MAX(0,diff) - pad * lastUnpreset;
	}
    } while (recalc);

    if (tablePtr->rowStarts) ckfree((char *) tablePtr->rowStarts);
    tablePtr->rowStarts = (int *) ckalloc((tablePtr->rows+1)*sizeof(int));
    /*
     * Now do the padding and calculate the row starts
     */
    total = 0;
    for (i = 0; i < tablePtr->rows; i++) {
	if (tablePtr->rowPixels[i] == -1) {
	    tablePtr->rowPixels[i] = defRowHeight
		+ ((i==lastUnpreset)?lastPad:pad);
	} else if (tablePtr->rowStretch == STRETCH_MODE_ALL) {
	    tablePtr->rowPixels[i] += (i==lastUnpreset)?lastPad:pad;
	}
	/* calculate the start of each row */
	tablePtr->rowStarts[i] = total;
	total += tablePtr->rowPixels[i];
    }
    tablePtr->rowStarts[i] = tablePtr->maxHeight = total;

    /*
     * Make sure the top row and col have reasonable real indices
     */
    CONSTRAIN(tablePtr->topRow, tablePtr->titleRows, tablePtr->rows-1);
    CONSTRAIN(tablePtr->leftCol, tablePtr->titleCols, tablePtr->cols-1);

    /*
     * If we don't have the info, don't bother to fix up the other parameters
     */
    if (Tk_WindowId(tablePtr->tkwin) == None) {
	tablePtr->oldTopRow = tablePtr->oldLeftCol = -1;
	return;
    }

    topRow  = tablePtr->topRow;
    leftCol = tablePtr->leftCol;
    w += hl;
    h += hl;
    /* 
     * If we use this value of topRow, will we fill the window?
     * if not, decrease it until we will, or until it gets to titleRows 
     * make sure we don't cut off the bottom row
     */
    for (; topRow > tablePtr->titleRows; topRow--) {
	if ((tablePtr->maxHeight-(tablePtr->rowStarts[topRow-1] -
		tablePtr->rowStarts[tablePtr->titleRows])) > h) {
	    break;
	}
    }
    /* 
     * If we use this value of topCol, will we fill the window?
     * if not, decrease it until we will, or until it gets to titleCols 
     * make sure we don't cut off the left column
     */
    for (; leftCol > tablePtr->titleCols; leftCol--) {
	if ((tablePtr->maxWidth-(tablePtr->colStarts[leftCol-1] -
		tablePtr->colStarts[tablePtr->titleCols])) > w) {
	    break;
	}
    }

    tablePtr->topRow  = topRow;
    tablePtr->leftCol = leftCol;

    /*
     * Now work out where the bottom right is for scrollbar update and to test
     * for one last stretch.  Avoid the confusion that spans could cause for
     * determining the last cell dimensions.
     */
    tablePtr->flags |= AVOID_SPANS;
    TableGetLastCell(tablePtr, &row, &col);
    TableCellVCoords(tablePtr, row, col, &x, &y, &width, &height, 0);
    tablePtr->flags &= ~AVOID_SPANS;

    /*
     * Do we have scrollbars, if so, calculate and call the TCL functions In
     * order to get the scrollbar to be completely full when the whole screen
     * is shown and there are titles, we have to arrange for the scrollbar
     * range to be 0 -> rows-titleRows etc.  This leads to the position
     * setting methods, toprow and leftcol, being relative to the titles, not
     * absolute row and column numbers.
     */
    if (tablePtr->yScrollCmd != NULL || tablePtr->xScrollCmd != NULL) {
	Tcl_Interp *interp = tablePtr->interp;
	char buf[INDEX_BUFSIZE];
	double first, last;

	/*
	 * We must hold onto the interpreter because the data referred to at
	 * tablePtr might be freed as a result of the call to Tcl_VarEval.
	 */
	Tcl_Preserve((ClientData) interp);

	/* Do we have a Y-scrollbar and rows to scroll? */
	if (tablePtr->yScrollCmd != NULL) {
	    if (row < tablePtr->titleRows) {
		first = 0;
		last  = 1;
	    } else {
		diff = tablePtr->rowStarts[tablePtr->titleRows];
		last = (double) (tablePtr->rowStarts[tablePtr->rows]-diff);
		if (last <= 0.0) {
		    first = 0;
		    last  = 1;
		} else {
		    first = (tablePtr->rowStarts[topRow]-diff) / last;
		    last  = (height+tablePtr->rowStarts[row]-diff) / last;
		}
	    }
	    sprintf(buf, " %g %g", first, last);
	    if (Tcl_VarEval(interp, tablePtr->yScrollCmd,
		    buf, (char *)NULL) != TCL_OK) {
		Tcl_AddErrorInfo(interp,
			"\n\t(vertical scrolling command executed by table)");
		Tcl_BackgroundError(interp);
	    }
	}
	/* Do we have a X-scrollbar and cols to scroll? */
	if (tablePtr->xScrollCmd != NULL) {
	    if (col < tablePtr->titleCols) {
		first = 0;
		last  = 1;
	    } else {
		diff = tablePtr->colStarts[tablePtr->titleCols];
		last = (double) (tablePtr->colStarts[tablePtr->cols]-diff);
		if (last <= 0.0) {
		    first = 0;
		    last  = 1;
		} else {
		    first = (tablePtr->colStarts[leftCol]-diff) / last;
		    last  = (width+tablePtr->colStarts[col]-diff) / last;
		}
	    }
	    sprintf(buf, " %g %g", first, last);
	    if (Tcl_VarEval(interp, tablePtr->xScrollCmd,
		    buf, (char *)NULL) != TCL_OK) {
		Tcl_AddErrorInfo(interp,
			"\n\t(horizontal scrolling command executed by table)");
		Tcl_BackgroundError(interp);
	    }
	}

	Tcl_Release((ClientData) interp);
    }

    /*
     * Adjust the last row/col to fill empty space if it is visible.
     * Do this after setting the scrollbars to not upset its calculations.
     */
    if (row == tablePtr->rows-1 && tablePtr->rowStretch != STRETCH_MODE_NONE) {
	diff = h-(y+height);
	if (diff > 0) {
	    tablePtr->rowPixels[tablePtr->rows-1] += diff;
	    tablePtr->rowStarts[tablePtr->rows] += diff;
	}
    }
    if (col == tablePtr->cols-1 && tablePtr->colStretch != STRETCH_MODE_NONE) {
	diff = w-(x+width);
	if (diff > 0) {
	    tablePtr->colPixels[tablePtr->cols-1] += diff;
	    tablePtr->colStarts[tablePtr->cols] += diff;
	}
    }

    TableAdjustActive(tablePtr);

    /*
     * now check the new value of topleft cell against the originals,
     * If they changed, invalidate the area, else leave it alone
     */
    if (tablePtr->topRow != tablePtr->oldTopRow ||
	tablePtr->leftCol != tablePtr->oldLeftCol) {
	/* set the old top row/col for the next time this function is called */
	tablePtr->oldTopRow = tablePtr->topRow;
	tablePtr->oldLeftCol = tablePtr->leftCol;
	/* only the upper corner title cells wouldn't change */
	TableInvalidateAll(tablePtr, 0);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TableCursorEvent --
 *	Toggle the cursor status.  Equivalent to EntryBlinkProc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor will be switched off/on.
 *
 *----------------------------------------------------------------------
 */
static void
TableCursorEvent(ClientData clientData)
{
    register Table *tablePtr = (Table *) clientData;

    if (!(tablePtr->flags & HAS_FOCUS) || (tablePtr->insertOffTime == 0)
	    || (tablePtr->flags & ACTIVE_DISABLED)
	    || (tablePtr->state != STATE_NORMAL)) {
	return;
    }

    if (tablePtr->cursorTimer != NULL) {
	Tcl_DeleteTimerHandler(tablePtr->cursorTimer);
    }

    tablePtr->cursorTimer =
	Tcl_CreateTimerHandler((tablePtr->flags & CURSOR_ON) ?
		tablePtr->insertOffTime : tablePtr->insertOnTime,
		TableCursorEvent, (ClientData) tablePtr);

    /* Toggle the cursor */
    tablePtr->flags ^= CURSOR_ON;

    /* invalidate the cell */
    TableRefresh(tablePtr, tablePtr->activeRow, tablePtr->activeCol, CELL);
}

/*
 *----------------------------------------------------------------------
 *
 * TableConfigCursor --
 *	Configures the timer depending on the state of the table.
 *	Equivalent to EntryFocusProc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor will be switched off/on.
 *
 *----------------------------------------------------------------------
 */
void
TableConfigCursor(register Table *tablePtr)
{
    /*
     * To have a cursor, we have to have focus and allow edits
     */
    if ((tablePtr->flags & HAS_FOCUS) && (tablePtr->state == STATE_NORMAL) &&
	!(tablePtr->flags & ACTIVE_DISABLED)) {
	/*
	 * Turn the cursor ON
	 */
	if (!(tablePtr->flags & CURSOR_ON)) {
	    tablePtr->flags |= CURSOR_ON;
	    /*
	     * Only refresh when we toggled cursor
	     */
	    TableRefresh(tablePtr, tablePtr->activeRow, tablePtr->activeCol,
		    CELL);
	}

	/* set up the first timer */
	if (tablePtr->insertOffTime != 0) {
	    /* make sure nothing existed */
	    Tcl_DeleteTimerHandler(tablePtr->cursorTimer);
	    tablePtr->cursorTimer =
		Tcl_CreateTimerHandler(tablePtr->insertOnTime,
			TableCursorEvent, (ClientData) tablePtr);
	}
    } else {
	/*
	 * Turn the cursor OFF
	 */
	if ((tablePtr->flags & CURSOR_ON)) {
	    tablePtr->flags &= ~CURSOR_ON;
	    TableRefresh(tablePtr, tablePtr->activeRow, tablePtr->activeCol,
		    CELL);
	}

	/* and disable the timer */
	if (tablePtr->cursorTimer != NULL) {
	    Tcl_DeleteTimerHandler(tablePtr->cursorTimer);
	}
	tablePtr->cursorTimer = NULL;
    }

}

/*
 *----------------------------------------------------------------------
 *
 * TableFetchSelection --
 *	This procedure is called back by Tk when the selection is
 *	requested by someone.  It returns part or all of the selection
 *	in a buffer provided by the caller.
 *
 * Results:
 *	The return value is the number of non-NULL bytes stored
 *	at buffer.  Buffer is filled (or partially filled) with a
 *	NULL-terminated string containing part or all of the selection,
 *	as given by offset and maxBytes.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static int
TableFetchSelection(clientData, offset, buffer, maxBytes)
     ClientData clientData;	/* Information about table widget. */
     int offset;		/* Offset within selection of first
				 * character to be returned. */
     char *buffer;		/* Location in which to place selection. */
     int maxBytes;		/* Maximum number of bytes to place at buffer,
				 * not including terminating NULL. */
{
    register Table *tablePtr = (Table *) clientData;
    Tcl_Interp *interp = tablePtr->interp;
    char *value, *data, *rowsep = tablePtr->rowSep, *colsep = tablePtr->colSep;
    Tcl_DString selection;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    int length, count, lastrow=0, needcs=0, r, c, listArgc, rslen=0, cslen=0;
    int numcols, numrows;
    CONST84 char **listArgv;

    /* if we are not exporting the selection ||
     * we have no data source, return */
    if (!tablePtr->exportSelection ||
	(tablePtr->dataSource == DATA_NONE)) {
	return -1;
    }

    /* First get a sorted list of the selected elements */
    Tcl_DStringInit(&selection);
    for (entryPtr = Tcl_FirstHashEntry(tablePtr->selCells, &search);
	 entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&search)) {
	Tcl_DStringAppendElement(&selection,
				 Tcl_GetHashKey(tablePtr->selCells, entryPtr));
    }
    value = TableCellSort(tablePtr, Tcl_DStringValue(&selection));
    Tcl_DStringFree(&selection);

    if (value == NULL ||
	Tcl_SplitList(interp, value, &listArgc, &listArgv) != TCL_OK) {
	return -1;
    }
    Tcl_Free(value);

    Tcl_DStringInit(&selection);
    rslen = (rowsep?(strlen(rowsep)):0);
    cslen = (colsep?(strlen(colsep)):0);
    numrows = numcols = 0;
    for (count = 0; count < listArgc; count++) {
	TableParseArrayIndex(&r, &c, listArgv[count]);
	if (count) {
	    if (lastrow != r) {
		lastrow = r;
		needcs = 0;
		if (rslen) {
		    Tcl_DStringAppend(&selection, rowsep, rslen);
		} else {
		    Tcl_DStringEndSublist(&selection);
		    Tcl_DStringStartSublist(&selection);
		}
		++numrows;
	    } else {
		if (++needcs > numcols)
		    numcols = needcs;
	    }
	} else {
	    lastrow = r;
	    needcs = 0;
	    if (!rslen) {
		Tcl_DStringStartSublist(&selection);
	    }
	}
	data = TableGetCellValue(tablePtr, r, c);
	if (cslen) {
	    if (needcs) {
		Tcl_DStringAppend(&selection, colsep, cslen);
	    }
	    Tcl_DStringAppend(&selection, data, -1);
	} else {
	    Tcl_DStringAppendElement(&selection, data);
	}
    }
    if (!rslen && count) {
	Tcl_DStringEndSublist(&selection);
    }
    Tcl_Free((char *) listArgv);

    if (tablePtr->selCmd != NULL) {
	Tcl_DString script;
	Tcl_DStringInit(&script);
	ExpandPercents(tablePtr, tablePtr->selCmd, numrows+1, numcols+1,
		       Tcl_DStringValue(&selection), (char *)NULL,
		       listArgc, &script, CMD_ACTIVATE);
	if (Tcl_GlobalEval(interp, Tcl_DStringValue(&script)) == TCL_ERROR) {
	    Tcl_AddErrorInfo(interp,
			     "\n    (error in table selection command)");
	    Tcl_BackgroundError(interp);
	    Tcl_DStringFree(&script);
	    Tcl_DStringFree(&selection);
	    return -1;
	} else {
	    Tcl_DStringGetResult(interp, &selection);
	}
	Tcl_DStringFree(&script);
    }

    length = Tcl_DStringLength(&selection);

    if (length == 0)
	return -1;

    /* Copy the requested portion of the selection to the buffer. */
    count = length - offset;
    if (count <= 0) {
	count = 0;
    } else {
	if (count > maxBytes) {
	    count = maxBytes;
	}
	memcpy((VOID *) buffer,
	       (VOID *) (Tcl_DStringValue(&selection) + offset),
	       (size_t) count);
    }
    buffer[count] = '\0';
    Tcl_DStringFree(&selection);
    return count;
}

/*
 *----------------------------------------------------------------------
 *
 * TableLostSelection --
 *	This procedure is called back by Tk when the selection is
 *	grabbed away from a table widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The existing selection is unhighlighted, and the window is
 *	marked as not containing a selection.
 *
 *----------------------------------------------------------------------
 */
void
TableLostSelection(clientData)
     ClientData clientData;	/* Information about table widget. */
{
    register Table *tablePtr = (Table *) clientData;

    if (tablePtr->exportSelection) {
	Tcl_HashEntry *entryPtr;
	Tcl_HashSearch search;
	int row, col;

	/* Same as SEL CLEAR ALL */
	for (entryPtr = Tcl_FirstHashEntry(tablePtr->selCells, &search);
	     entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&search)) {
	    TableParseArrayIndex(&row, &col,
				 Tcl_GetHashKey(tablePtr->selCells,entryPtr));
	    Tcl_DeleteHashEntry(entryPtr);
	    TableRefresh(tablePtr, row-tablePtr->rowOffset,
			 col-tablePtr->colOffset, CELL);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TableRestrictProc --
 *	A Tk_RestrictProc used by TableValidateChange to eliminate any
 *	extra key input events in the event queue that
 *	have a serial number no less than a given value.
 *
 * Results:
 *	Returns either TK_DISCARD_EVENT or TK_DEFER_EVENT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static Tk_RestrictAction
TableRestrictProc(serial, eventPtr)
     ClientData serial;
     XEvent *eventPtr;
{
    if ((eventPtr->type == KeyRelease || eventPtr->type == KeyPress) &&
	((eventPtr->xany.serial-(unsigned int)serial) > 0)) {
	return TK_DEFER_EVENT;
    } else {
	return TK_PROCESS_EVENT;
    }
}

/*
 *--------------------------------------------------------------
 *
 * TableValidateChange --
 *	This procedure is invoked when any character is added or
 *	removed from the table widget, or a set has triggered validation.
 *
 * Results:
 *	TCL_OK    if the validatecommand accepts the new string,
 *	TCL_BREAK if the validatecommand rejects the new string,
 *      TCL_ERROR if any problems occured with validatecommand.
 *
 * Side effects:
 *      The insertion/deletion may be aborted, and the
 *      validatecommand might turn itself off (if an error
 *      or loop condition arises).
 *
 *--------------------------------------------------------------
 */
int
TableValidateChange(tablePtr, r, c, old, new, index)
     register Table *tablePtr;	/* Table that needs validation. */
     int r, c;			/* row,col index of cell in user coords */
     char *old;			/* current value of cell */
     char *new;			/* potential new value of cell */
     int index;			/* index of insert/delete, -1 otherwise */
{
    register Tcl_Interp *interp = tablePtr->interp;
    int code, bool;
    Tk_RestrictProc *rstrct;
    ClientData cdata;
    Tcl_DString script;
    
    if (tablePtr->valCmd == NULL || tablePtr->validate == 0) {
	return TCL_OK;
    }

    /* Magic code to make this bit of code UI synchronous in the face of
     * possible new key events */
    XSync(tablePtr->display, False);
    rstrct = Tk_RestrictEvents(TableRestrictProc, (ClientData)
				 NextRequest(tablePtr->display), &cdata);

    /*
     * If we're already validating, then we're hitting a loop condition
     * Return and set validate to 0 to disallow further validations
     * and prevent current validation from finishing
     */
    if (tablePtr->flags & VALIDATING) {
	tablePtr->validate = 0;
	return TCL_OK;
    }
    tablePtr->flags |= VALIDATING;

    /* Now form command string and run through the -validatecommand */
    Tcl_DStringInit(&script);
    ExpandPercents(tablePtr, tablePtr->valCmd, r, c, old, new, index, &script,
		   CMD_VALIDATE);
    code = Tcl_GlobalEval(tablePtr->interp, Tcl_DStringValue(&script));
    Tcl_DStringFree(&script);

    if (code != TCL_OK && code != TCL_RETURN) {
	Tcl_AddErrorInfo(interp,
			 "\n\t(in validation command executed by table)");
	Tcl_BackgroundError(interp);
	code = TCL_ERROR;
    } else if (Tcl_GetBooleanFromObj(interp, Tcl_GetObjResult(interp),
				     &bool) != TCL_OK) {
	Tcl_AddErrorInfo(interp,
			 "\n\tboolean not returned by validation command");
	Tcl_BackgroundError(interp);
	code = TCL_ERROR;
    } else {
	code = (bool) ? TCL_OK : TCL_BREAK;
    }
    Tcl_SetObjResult(interp, Tcl_NewObj());

    /*
     * If ->validate has become VALIDATE_NONE during the validation,
     * it means that a loop condition almost occured.  Do not allow
     * this validation result to finish.
     */
    if (tablePtr->validate == 0) {
	code = TCL_ERROR;
    }

    /* If validate will return ERROR, then disallow further validations */
    if (code == TCL_ERROR) {
	tablePtr->validate = 0;
    }

    Tk_RestrictEvents(rstrct, cdata, &cdata);
    tablePtr->flags &= ~VALIDATING;

    return code;
}

/*
 *--------------------------------------------------------------
 *
 * ExpandPercents --
 *	Given a command and an event, produce a new command
 *	by replacing % constructs in the original command
 *	with information from the X event.
 *
 * Results:
 *	The new expanded command is appended to the dynamic string
 *	given by dsPtr.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */
void
ExpandPercents(tablePtr, before, r, c, old, new, index, dsPtr, cmdType)
     Table *tablePtr;		/* Table that needs validation. */
     char *before;		/* Command containing percent
				 * expressions to be replaced. */
     int r, c;			/* row,col index of cell */
     char *old;                 /* current value of cell */
     char *new;                 /* potential new value of cell */
     int index;                 /* index of insert/delete */
     Tcl_DString *dsPtr;        /* Dynamic string in which to append
				 * new command. */
     int cmdType;		/* type of command to make %-subs for */
{
    int length, spaceNeeded, cvtFlags;
#ifdef TCL_UTF_MAX
    Tcl_UniChar ch;
#else
    char ch;
#endif
    char *string, buf[INDEX_BUFSIZE];

    /* This returns the static value of the string as set in the array */
    if (old == NULL && cmdType == CMD_VALIDATE) {
	old = TableGetCellValue(tablePtr, r, c);
    }

    while (1) {
	if (*before == '\0') {
	    break;
	}
	/*
	 * Find everything up to the next % character and append it
	 * to the result string.
	 */

	string = before;
#ifdef TCL_UTF_MAX
	/* No need to convert '%', as it is in ascii range */
	string = (char *) Tcl_UtfFindFirst(before, '%');
#else
	string = strchr(before, '%');
#endif
	if (string == (char *) NULL) {
	    Tcl_DStringAppend(dsPtr, before, -1);
	    break;
	} else if (string != before) {
	    Tcl_DStringAppend(dsPtr, before, string-before);
	    before = string;
	}

	/*
	 * There's a percent sequence here.  Process it.
	 */

	before++; /* skip over % */
	if (*before != '\0') {
#ifdef TCL_UTF_MAX
	    before += Tcl_UtfToUniChar(before, &ch);
#else
	    ch = before[0];
	    before++;
#endif
	} else {
	    ch = '%';
	}
	switch (ch) {
	case 'c':
	    sprintf(buf, "%d", c);
	    string = buf;
	    break;
	case 'C': /* index of cell */
	    TableMakeArrayIndex(r, c, buf);
	    string = buf;
	    break;
	case 'r':
	    sprintf(buf, "%d", r);
	    string = buf;
	    break;
	case 'i': /* index of cursor OR |number| of cells selected */
	    sprintf(buf, "%d", index);
	    string = buf;
	    break;
	case 's': /* Current cell value */
	    string = old;
	    break;
	case 'S': /* Potential new value of cell */
	    string = (new?new:old);
	    break;
	case 'W': /* widget name */
	    string = Tk_PathName(tablePtr->tkwin);
	    break;
	default:
#ifdef TCL_UTF_MAX
	    length = Tcl_UniCharToUtf(ch, buf);
#else
	    buf[0] = ch;
	    length = 1;
#endif
	    buf[length] = '\0';
	    string = buf;
	    break;
	}

	spaceNeeded = Tcl_ScanElement(string, &cvtFlags);
	length = Tcl_DStringLength(dsPtr);
	Tcl_DStringSetLength(dsPtr, length + spaceNeeded);
	spaceNeeded = Tcl_ConvertElement(string,
					 Tcl_DStringValue(dsPtr) + length,
					 cvtFlags | TCL_DONT_USE_BRACES);
	Tcl_DStringSetLength(dsPtr, length + spaceNeeded);
    }
    Tcl_DStringAppend(dsPtr, "", 1);
}

/* Function to call on loading the Table module */

#ifdef BUILD_Tktable
#   undef TCL_STORAGE_CLASS
#   define TCL_STORAGE_CLASS DLLEXPORT
#endif
#ifdef MAC_TCL
#pragma export on
#endif
EXTERN int
Tktable_Init(interp)
     Tcl_Interp *interp;
{
    /* This defines the static chars tkTable(Safe)InitScript */
#include "tkTableInitScript.h"

    if (
#ifdef USE_TCL_STUBS
	Tcl_InitStubs(interp, "8.0", 0)
#else
	Tcl_PkgRequire(interp, "Tcl", "8.0", 0)
#endif
	== NULL) {
	return TCL_ERROR;
    }
    if (
#ifdef USE_TK_STUBS
	Tk_InitStubs(interp, "8.0", 0)
#else
#    if (TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION == 0)
	/* We require 8.0 exact because of the Unicode in 8.1+ */
	Tcl_PkgRequire(interp, "Tk", "8.0", 1)
#    else
	Tcl_PkgRequire(interp, "Tk", "8.0", 0)
#    endif
#endif
	== NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgProvide(interp, "Tktable", PACKAGE_VERSION) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_CreateObjCommand(interp, TBL_COMMAND, Tk_TableObjCmd,
			 (ClientData) Tk_MainWindow(interp),
			 (Tcl_CmdDeleteProc *) NULL);

    /*
     * The init script can't make certain calls in a safe interpreter,
     * so we always have to use the embedded runtime for it
     */
    return Tcl_Eval(interp, Tcl_IsSafe(interp) ?
	    tkTableSafeInitScript : tkTableInitScript);
}

EXTERN int
Tktable_SafeInit(interp)
     Tcl_Interp *interp;
{
    return Tktable_Init(interp);
}
#ifdef MAC_TCL
#pragma export reset
#endif

#ifdef WIN32
/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This wrapper function is used by Windows to invoke the
 *	initialization code for the DLL.  If we are compiling
 *	with Visual C++, this routine will be renamed to DllMain.
 *	routine.
 *
 * Results:
 *	Returns TRUE;
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY
DllEntryPoint(hInst, reason, reserved)
     HINSTANCE hInst;		/* Library instance handle. */
     DWORD reason;		/* Reason this function is being called. */
     LPVOID reserved;		/* Not used. */
{
    return TRUE;
}
#endif
