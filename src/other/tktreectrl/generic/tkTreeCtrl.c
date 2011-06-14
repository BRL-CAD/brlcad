/* 
 * tkTreeCtrl.c --
 *
 *	This module implements treectrl widgets for the Tk toolkit.
 *
 * Copyright (c) 2002-2010 Tim Baker
 * Copyright (c) 2002-2003 Christian Krone
 * Copyright (c) 2003-2005 ActiveState, a division of Sophos
 *
 * RCS: @(#) $Id$
 */

#include "tkTreeCtrl.h"

#ifdef WIN32
#include <windows.h>
#endif
#if defined(MAC_TK_CARBON)
#include <Carbon/Carbon.h>
#endif
#if defined(MAC_TK_COCOA)
#import <Cocoa/Cocoa.h>
#endif

/*
 * TIP #116 altered Tk_PhotoPutBlock API to add interp arg.
 * We need to remove that for compiling with 8.4.
 */
#if (TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION < 5)
#define TK_PHOTOPUTBLOCK(interp, hdl, blk, x, y, w, h, cr) \
		Tk_PhotoPutBlock(hdl, blk, x, y, w, h, cr)
#define TK_PHOTOPUTZOOMEDBLOCK(interp, hdl, blk, x, y, w, h, \
				zx, zy, sx, sy, cr) \
		Tk_PhotoPutZoomedBlock(hdl, blk, x, y, w, h, \
				zx, zy, sx, sy, cr)
#else
#define TK_PHOTOPUTBLOCK	Tk_PhotoPutBlock
#define TK_PHOTOPUTZOOMEDBLOCK	Tk_PhotoPutZoomedBlock
#endif

/* This structure is used for reference-counted images. */
typedef struct TreeImageRef {
    int count;			/* Reference count. */
    Tk_Image image;		/* Image token. */
    Tcl_HashEntry *hPtr;	/* Entry in tree->imageNameHash. */
} TreeImageRef;

static CONST char *bgModeST[] = {
    "column", "order", "ordervisible", "row",
#ifdef DEPRECATED
    "index", "visindex",
#endif
    (char *) NULL
};
static CONST char *columnResizeModeST[] = {
    "proxy", "realtime", (char *) NULL
};
static CONST char *doubleBufferST[] = {
    "none", "item", "window", (char *) NULL
};
static CONST char *lineStyleST[] = {
    "dot", "solid", (char *) NULL
};
static CONST char *orientStringTable[] = {
    "horizontal", "vertical", (char *) NULL
};

static Tk_OptionSpec optionSpecs[] = {
    {TK_OPTION_BORDER, "-background", "background", "Background",
     "white", -1, Tk_Offset(TreeCtrl, border), 0, 
     (ClientData) "white", TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING, "-backgroundimage", "backgroundImage", "BackgroundImage",
      (char *) NULL, -1, Tk_Offset(TreeCtrl, backgroundImageString),
      TK_OPTION_NULL_OK, (ClientData) NULL,
      TREE_CONF_BG_IMAGE | TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING_TABLE, "-backgroundmode",
     "backgroundMode", "BackgroundMode",
     "row", -1, Tk_Offset(TreeCtrl, backgroundMode),
     0, (ClientData) bgModeST, TREE_CONF_REDISPLAY},
    {TK_OPTION_SYNONYM, "-bd", (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) "-borderwidth"},
    {TK_OPTION_SYNONYM, "-bg", (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) "-background"},
    {TK_OPTION_SYNONYM, "-bgimage", (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) "-backgroundimage"},
    {TK_OPTION_ANCHOR, "-bgimageanchor", "bgImageAnchor", "BgImageAnchor",
     "nw", -1, Tk_Offset(TreeCtrl, bgImageAnchor),
     0, (ClientData) NULL, TREE_CONF_BGIMGOPT | TREE_CONF_REDISPLAY},
    {TK_OPTION_BOOLEAN, "-bgimageopaque", "bgImageOpaque", "BgImageOpaque",
     "1", -1, Tk_Offset(TreeCtrl, bgImageOpaque), 0, (ClientData) NULL,
     TREE_CONF_BGIMGOPT | TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING, "-bgimagescroll", "bgImageScroll", "BgImageScroll",
     "xy", Tk_Offset(TreeCtrl, bgImageScrollObj), -1,
     0, (ClientData) NULL, TREE_CONF_BGIMGOPT | TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING, "-bgimagetile", "bgImageTile", "BgImageTile",
     "xy", Tk_Offset(TreeCtrl, bgImageTileObj), -1,
     0, (ClientData) NULL, TREE_CONF_BGIMGOPT | TREE_CONF_REDISPLAY},
    {TK_OPTION_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
     DEF_LISTBOX_BORDER_WIDTH, Tk_Offset(TreeCtrl, borderWidthObj),
     Tk_Offset(TreeCtrl, borderWidth),
     0, (ClientData) NULL, TREE_CONF_BORDERS | TREE_CONF_RELAYOUT},
    {TK_OPTION_CUSTOM, "-buttonbitmap", "buttonBitmap", "ButtonBitmap",
     (char *) NULL, /* DEFAULT VALUE IS INITIALIZED LATER */
     Tk_Offset(TreeCtrl, buttonBitmap.obj), Tk_Offset(TreeCtrl, buttonBitmap),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     TREE_CONF_BUTTON | TREE_CONF_BUTBMP | TREE_CONF_RELAYOUT},
    {TK_OPTION_COLOR, "-buttoncolor", "buttonColor", "ButtonColor",
     "#808080", -1, Tk_Offset(TreeCtrl, buttonColor),
     0, (ClientData) NULL, TREE_CONF_BUTTON | TREE_CONF_REDISPLAY},
    {TK_OPTION_CUSTOM, "-buttonimage", "buttonImage", "ButtonImage",
     (char *) NULL, /* DEFAULT VALUE IS INITIALIZED LATER */
     Tk_Offset(TreeCtrl, buttonImage.obj), Tk_Offset(TreeCtrl, buttonImage),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     TREE_CONF_BUTTON | TREE_CONF_BUTIMG | TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-buttonsize", "buttonSize", "ButtonSize",
     "9", Tk_Offset(TreeCtrl, buttonSizeObj),
     Tk_Offset(TreeCtrl, buttonSize),
     0, (ClientData) NULL, TREE_CONF_BUTTON | TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-buttonthickness",
     "buttonThickness", "ButtonThickness",
     "1", Tk_Offset(TreeCtrl, buttonThicknessObj),
     Tk_Offset(TreeCtrl, buttonThickness),
     0, (ClientData) NULL, TREE_CONF_BUTTON | TREE_CONF_REDISPLAY},
    {TK_OPTION_BOOLEAN, "-buttontracking", "buttonTracking", "ButtonTracking",
     (char *) NULL, /* DEFAULT VALUE IS INITIALIZED LATER */
     -1, Tk_Offset(TreeCtrl, buttonTracking), 0, (ClientData) NULL, 0},
    {TK_OPTION_CUSTOM, "-canvaspadx", "canvasPadX", "CanvasPadX",
     "0",
     Tk_Offset(TreeCtrl, canvasPadXObj),
     Tk_Offset(TreeCtrl, canvasPadX),
     0, (ClientData) &TreeCtrlCO_pad, TREE_CONF_RELAYOUT},
    {TK_OPTION_CUSTOM, "-canvaspady", "canvasPadY", "CanvasPadY",
     "0",
     Tk_Offset(TreeCtrl, canvasPadYObj),
     Tk_Offset(TreeCtrl, canvasPadY),
     0, (ClientData) &TreeCtrlCO_pad, TREE_CONF_RELAYOUT},
    {TK_OPTION_STRING, "-columnprefix", "columnPrefix", "ColumnPrefix",
     "", -1, Tk_Offset(TreeCtrl, columnPrefix), 0, (ClientData) NULL, 0},
    {TK_OPTION_PIXELS, "-columnproxy", "columnProxy", "ColumnProxy",
     (char *) NULL, Tk_Offset(TreeCtrl, columnProxy.xObj),
     Tk_Offset(TreeCtrl, columnProxy.x),
     TK_OPTION_NULL_OK, (ClientData) NULL, TREE_CONF_PROXY},
    {TK_OPTION_STRING_TABLE, "-columnresizemode",
     "columnResizeMode", "ColumnResizeMode",
     "realtime", -1, Tk_Offset(TreeCtrl, columnResizeMode),
     0, (ClientData) columnResizeModeST, 0},
    {TK_OPTION_BOOLEAN, "-columntagexpr", "columnTagExpr", "ColumnTagExpr",
     "1", -1, Tk_Offset(TreeCtrl, columnTagExpr),
     0, (ClientData) NULL, 0},
    {TK_OPTION_CURSOR, "-cursor", "cursor", "Cursor",
     (char *) NULL, -1, Tk_Offset(TreeCtrl, cursor),
     TK_OPTION_NULL_OK, (ClientData) NULL, 0},
#ifdef DEPRECATED
    {TK_OPTION_STRING, "-defaultstyle", "defaultStyle", "DefaultStyle",
     (char *) NULL, Tk_Offset(TreeCtrl, defaultStyle.stylesObj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, TREE_CONF_DEFSTYLE},
    {TK_OPTION_STRING_TABLE, "-doublebuffer", "doubleBuffer", "DoubleBuffer",
     "item", -1, Tk_Offset(TreeCtrl, doubleBuffer),
     0, (ClientData) doubleBufferST, TREE_CONF_REDISPLAY},
#endif /* DEPRECATED */
    {TK_OPTION_SYNONYM, "-fg", (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) "-foreground"},
    {TK_OPTION_FONT, "-font", "font", "Font",
     DEF_LISTBOX_FONT, Tk_Offset(TreeCtrl, fontObj),
     Tk_Offset(TreeCtrl, tkfont),
     0, (ClientData) NULL, TREE_CONF_FONT | TREE_CONF_RELAYOUT},
    {TK_OPTION_COLOR, "-foreground", "foreground", "Foreground",
     DEF_LISTBOX_FG, Tk_Offset(TreeCtrl, fgObj), Tk_Offset(TreeCtrl, fgColorPtr),
     0, (ClientData) NULL, TREE_CONF_FG | TREE_CONF_REDISPLAY},
    {TK_OPTION_PIXELS, "-height", "height", "Height",
     "200", Tk_Offset(TreeCtrl, heightObj), Tk_Offset(TreeCtrl, height),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_COLOR, "-highlightbackground", "highlightBackground",
     "HighlightBackground", DEF_LISTBOX_HIGHLIGHT_BG, -1, 
     Tk_Offset(TreeCtrl, highlightBgColorPtr),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
     DEF_LISTBOX_HIGHLIGHT, -1, Tk_Offset(TreeCtrl, highlightColorPtr),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_PIXELS, "-highlightthickness", "highlightThickness",
     "HighlightThickness", DEF_LISTBOX_HIGHLIGHT_WIDTH,
     Tk_Offset(TreeCtrl, highlightWidthObj),
     Tk_Offset(TreeCtrl, highlightWidth),
     0, (ClientData) NULL, TREE_CONF_BORDERS | TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-indent", "indent", "Indent",
     "19", Tk_Offset(TreeCtrl, indentObj),
     Tk_Offset(TreeCtrl, indent),
     0, (ClientData) NULL, TREE_CONF_INDENT | TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-itemgapx", "itemGapX", "ItemGapX",
     "0",
     Tk_Offset(TreeCtrl, itemGapXObj),
     Tk_Offset(TreeCtrl, itemGapX),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-itemgapy", "itemGapY", "ItemGapY",
     "0",
     Tk_Offset(TreeCtrl, itemGapYObj),
     Tk_Offset(TreeCtrl, itemGapY),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-itemheight", "itemHeight", "ItemHeight",
     "0", Tk_Offset(TreeCtrl, itemHeightObj),
     Tk_Offset(TreeCtrl, itemHeight),
     0, (ClientData) NULL, TREE_CONF_ITEMSIZE | TREE_CONF_RELAYOUT},
    {TK_OPTION_STRING, "-itemprefix", "itemPrefix", "ItemPrefix",
     "", -1, Tk_Offset(TreeCtrl, itemPrefix), 0, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-itemtagexpr", "itemTagExpr", "ItemTagExpr",
     "1", -1, Tk_Offset(TreeCtrl, itemTagExpr),
     0, (ClientData) NULL, 0},
    {TK_OPTION_PIXELS, "-itemwidth", "itemWidth", "ItemWidth",
     "", Tk_Offset(TreeCtrl, itemWidthObj), Tk_Offset(TreeCtrl, itemWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, TREE_CONF_ITEMSIZE | TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-itemwidthequal", "itemWidthEqual", "ItemWidthEqual",
     "0", -1, Tk_Offset(TreeCtrl, itemWidthEqual),
     TK_OPTION_NULL_OK, (ClientData) NULL, TREE_CONF_ITEMSIZE | TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-itemwidthmultiple", "itemWidthMultiple", "ItemWidthMultiple",
     "", Tk_Offset(TreeCtrl, itemWidMultObj), Tk_Offset(TreeCtrl, itemWidMult),
     TK_OPTION_NULL_OK, (ClientData) NULL, TREE_CONF_ITEMSIZE | TREE_CONF_RELAYOUT},
    {TK_OPTION_COLOR, "-linecolor", "lineColor", "LineColor",
     "#808080", -1, Tk_Offset(TreeCtrl, lineColor),
     0, (ClientData) NULL, TREE_CONF_LINE | TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING_TABLE, "-linestyle", "lineStyle", "LineStyle",
     "dot", -1, Tk_Offset(TreeCtrl, lineStyle),
     0, (ClientData) lineStyleST, TREE_CONF_LINE | TREE_CONF_REDISPLAY},
    {TK_OPTION_PIXELS, "-linethickness", "lineThickness", "LineThickness",
     "1", Tk_Offset(TreeCtrl, lineThicknessObj),
     Tk_Offset(TreeCtrl, lineThickness),
     0, (ClientData) NULL, TREE_CONF_LINE | TREE_CONF_REDISPLAY},
    {TK_OPTION_PIXELS, "-minitemheight", "minItemHeight", "MinItemHeight",
     "0", Tk_Offset(TreeCtrl, minItemHeightObj),
     Tk_Offset(TreeCtrl, minItemHeight),
     0, (ClientData) NULL, TREE_CONF_ITEMSIZE | TREE_CONF_RELAYOUT},
    {TK_OPTION_STRING_TABLE, "-orient", "orient", "Orient",
     "vertical", -1, Tk_Offset(TreeCtrl, vertical),
     0, (ClientData) orientStringTable, TREE_CONF_RELAYOUT},
    {TK_OPTION_RELIEF, "-relief", "relief", "Relief",
     "sunken", -1, Tk_Offset(TreeCtrl, relief),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_PIXELS, "-rowproxy", "rowProxy", "RowProxy",
     (char *) NULL, Tk_Offset(TreeCtrl, rowProxy.yObj),
     Tk_Offset(TreeCtrl, rowProxy.y),
     TK_OPTION_NULL_OK, (ClientData) NULL, TREE_CONF_PROXY},
    {TK_OPTION_STRING, "-scrollmargin", "scrollMargin", "ScrollMargin",
     "0", Tk_Offset(TreeCtrl, scrollMargin), -1,
     0, (ClientData) NULL, 0},
    {TK_OPTION_STRING, "-selectmode", "selectMode", "SelectMode",
     DEF_LISTBOX_SELECT_MODE, -1, Tk_Offset(TreeCtrl, selectMode),
     TK_OPTION_NULL_OK, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-showbuttons", "showButtons",
     "ShowButtons", "1", -1, Tk_Offset(TreeCtrl, showButtons),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-showheader", "showHeader", "ShowHeader",
     "1", -1, Tk_Offset(TreeCtrl, showHeader),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-showlines", "showLines", "ShowLines",
     (char *) NULL, /* DEFAULT VALUE IS INITIALIZED LATER */
     -1, Tk_Offset(TreeCtrl, showLines),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-showroot", "showRoot",
     "ShowRoot", "1", -1, Tk_Offset(TreeCtrl, showRoot),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-showrootbutton", "showRootButton",
     "ShowRootButton", "0", -1, Tk_Offset(TreeCtrl, showRootButton),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-showrootchildbuttons", "showRootChildButtons",
     "ShowRootChildButtons", "1", -1, Tk_Offset(TreeCtrl, showRootChildButtons),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-showrootlines", "showRootLines",
     "ShowRootLines", "1", -1, Tk_Offset(TreeCtrl, showRootLines),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_STRING, "-takefocus", "takeFocus", "TakeFocus",
     DEF_LISTBOX_TAKE_FOCUS, -1, Tk_Offset(TreeCtrl, takeFocus),
     TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_CUSTOM, "-treecolumn", "treeColumn", "TreeColumn",
     (char *) NULL, -1, Tk_Offset(TreeCtrl, columnTree),
     TK_OPTION_NULL_OK, (ClientData) &TreeCtrlCO_column_NOT_TAIL,
     TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-usetheme", "useTheme",
     "UseTheme", "1", -1, Tk_Offset(TreeCtrl, useTheme),
     0, (ClientData) NULL, TREE_CONF_THEME | TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-width", "width", "Width",
     "200", Tk_Offset(TreeCtrl, widthObj), Tk_Offset(TreeCtrl, width),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_STRING, "-wrap", "wrap", "Wrap",
     (char *) NULL, Tk_Offset(TreeCtrl, wrapObj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL,
     TREE_CONF_WRAP | TREE_CONF_RELAYOUT},
    {TK_OPTION_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
     (char *) NULL, -1, Tk_Offset(TreeCtrl, xScrollCmd),
     TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_STRING, "-xscrolldelay", "xScrollDelay", "ScrollDelay",
     "50", Tk_Offset(TreeCtrl, xScrollDelay), -1,
     TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_PIXELS, "-xscrollincrement", "xScrollIncrement", "ScrollIncrement",
     "0", -1, Tk_Offset(TreeCtrl, xScrollIncrement),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_BOOLEAN, "-xscrollsmoothing", "xScrollSmoothing", "ScrollSmoothing",
     "0", -1, Tk_Offset(TreeCtrl, xScrollSmoothing),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
     (char *) NULL, -1, Tk_Offset(TreeCtrl, yScrollCmd),
     TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_STRING, "-yscrolldelay", "yScrollDelay", "ScrollDelay",
     "50", Tk_Offset(TreeCtrl, yScrollDelay), -1,
     TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_PIXELS, "-yscrollincrement", "yScrollIncrement", "ScrollIncrement",
     "0", -1, Tk_Offset(TreeCtrl, yScrollIncrement),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_BOOLEAN, "-yscrollsmoothing", "yScrollSmoothing", "ScrollSmoothing",
     "0", -1, Tk_Offset(TreeCtrl, yScrollSmoothing),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},

    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, -1, -1, 0, (ClientData) NULL, 0}
};

static Tk_OptionSpec debugSpecs[] = {
    {TK_OPTION_INT, "-displaydelay", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeCtrl, debug.displayDelay),
     0, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-data", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeCtrl, debug.data),
     0, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-display", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeCtrl, debug.display),
     0, (ClientData) NULL, 0},
    {TK_OPTION_COLOR, "-drawcolor", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeCtrl, debug.drawColor),
     TK_OPTION_NULL_OK, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-enable", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeCtrl, debug.enable),
     0, (ClientData) NULL, 0},
    {TK_OPTION_COLOR, "-erasecolor", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeCtrl, debug.eraseColor),
     TK_OPTION_NULL_OK, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-span", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeCtrl, debug.span),
     0, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-textlayout", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeCtrl, debug.textLayout),
     0, (ClientData) NULL, 0},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, -1, -1, 0, (ClientData) NULL, 0}
};

static int TreeWidgetCmd(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *CONST objv[]);
static int TreeConfigure(Tcl_Interp *interp, TreeCtrl *tree, int objc,
    Tcl_Obj *CONST objv[], int createFlag);
static void TreeEventProc(ClientData clientData, XEvent * eventPtr);
static void TreeDestroy(char *memPtr);
static void TreeCmdDeletedProc(ClientData clientData);
static void TreeWorldChanged(ClientData instanceData);
static void TreeComputeGeometry(TreeCtrl *tree);
static int TreeSeeCmd(TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[]);
static int TreeStateCmd(TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[]);
static int TreeSelectionCmd(Tcl_Interp *interp, TreeCtrl *tree, int objc,
    Tcl_Obj *CONST objv[]);
static int TreeDebugCmd(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *CONST objv[]);

static Tk_ClassProcs treectrlClass = {
    sizeof(Tk_ClassProcs),	/* size */
    TreeWorldChanged,		/* worldChangedProc. */
    NULL,			/* createProc. */
    NULL			/* modalProc. */
};

/*
 *--------------------------------------------------------------
 *
 * TreeObjCmd --
 *
 *	This procedure is invoked to process the [treectrl] Tcl
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
TreeObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree;
    Tk_Window tkwin;
    Tk_OptionTable optionTable;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "pathName ?options?");
	return TCL_ERROR;
    }

    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp), 
	    Tcl_GetStringFromObj(objv[1], NULL), (char *) NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }

    optionTable = Tk_CreateOptionTable(interp, optionSpecs);

    tree = (TreeCtrl *) ckalloc(sizeof(TreeCtrl));
    memset(tree, '\0', sizeof(TreeCtrl));
    tree->tkwin		= tkwin;
    tree->display	= Tk_Display(tkwin);
    tree->interp	= interp;
    tree->widgetCmd	= Tcl_CreateObjCommand(interp,
				Tk_PathName(tree->tkwin), TreeWidgetCmd,
				(ClientData) tree, TreeCmdDeletedProc);
    tree->optionTable	= optionTable;
    tree->relief	= TK_RELIEF_SUNKEN;
    tree->prevWidth	= Tk_Width(tkwin);
    tree->prevHeight	= Tk_Height(tkwin);
    tree->updateIndex	= 1;

    tree->stateNames[0]	= "open";
    tree->stateNames[1]	= "selected";
    tree->stateNames[2]	= "enabled";
    tree->stateNames[3]	= "active";
    tree->stateNames[4]	= "focus";

    Tcl_InitHashTable(&tree->selection, TCL_ONE_WORD_KEYS);

    /* Do this before Tree_InitColumns() which does Tk_InitOptions(), which
     * calls Tk_GetOption() which relies on the window class */
    Tk_SetClass(tkwin, "TreeCtrl");
    Tk_SetClassProcs(tkwin, &treectrlClass, (ClientData) tree);

    tree->debug.optionTable = Tk_CreateOptionTable(interp, debugSpecs);
    (void) Tk_InitOptions(interp, (char *) tree, tree->debug.optionTable,
	    tkwin);

    Tcl_InitHashTable(&tree->itemHash, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&tree->itemSpansHash, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&tree->elementHash, TCL_STRING_KEYS);
    Tcl_InitHashTable(&tree->styleHash, TCL_STRING_KEYS);
    Tcl_InitHashTable(&tree->imageNameHash, TCL_STRING_KEYS);
    Tcl_InitHashTable(&tree->imageTokenHash, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&tree->gradientHash, TCL_STRING_KEYS);

    TreeItemList_Init(tree, &tree->preserveItemList, 0);

#ifdef ALLOC_HAX
    tree->allocData = TreeAlloc_Init();
#endif

    Tree_InitColumns(tree);
    TreeItem_Init(tree);
    TreeNotify_Init(tree);
    (void) TreeStyle_Init(tree);
    TreeMarquee_Init(tree);
    TreeDragImage_Init(tree);
    TreeDInfo_Init(tree);
    TreeGradient_Init(tree);

    Tk_CreateEventHandler(tree->tkwin,
#ifdef USE_TTK
	    ExposureMask|StructureNotifyMask|FocusChangeMask|ActivateMask|VirtualEventMask,
#else
	    ExposureMask|StructureNotifyMask|FocusChangeMask|ActivateMask,
#endif
	    TreeEventProc, (ClientData) tree);

    /* Must do this on Unix because Tk_GCForColor() uses
     * Tk_WindowId(tree->tkwin) */
    Tk_MakeWindowExist(tree->tkwin);

    /* Window must exist on Win32. */
    TreeTheme_Init(tree);

    /*
     * Keep a hold of the associated tkwin until we destroy the listbox,
     * otherwise Tk might free it while we still need it.
     */
    Tcl_Preserve((ClientData) tkwin);

    if (Tk_InitOptions(interp, (char *) tree, optionTable, tkwin) != TCL_OK) {
	Tk_DestroyWindow(tree->tkwin);
	return TCL_ERROR;
    }

    if (TreeConfigure(interp, tree, objc - 2, objv + 2, TRUE) != TCL_OK) {
	Tk_DestroyWindow(tree->tkwin);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(Tk_PathName(tree->tkwin), -1));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeArea_FromObj --
 *
 *	Get a TREE_AREA_xxx constant from a Tcl_Obj string rep.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeArea_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,		/* Object whose string rep is used. */
    int *areaPtr		/* Returned TREE_AREA_xxx constant. */
    )
{
    static CONST char *areaName[] = { "content", "header", "left",
	    "right", (char *) NULL };
    static CONST int area[4] = { TREE_AREA_CONTENT, TREE_AREA_HEADER,
	    TREE_AREA_LEFT, TREE_AREA_RIGHT };
    int index;

    if (Tcl_GetIndexFromObj(tree->interp, objPtr, areaName, "area", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }
    (*areaPtr) = area[index];
    return TCL_OK;
}

#define W2Cx(x) ((x) + tree->xOrigin)
#define C2Wx(x) ((x) - tree->xOrigin)
#define C2Ox(x) ((x) - Tree_ContentLeft(tree))

#define W2Cy(y) ((y) + tree->yOrigin)
#define C2Wy(y) ((y) - tree->yOrigin)
#define C2Oy(y) ((y) - Tree_ContentTop(tree))

/*
 *--------------------------------------------------------------
 *
 * TreeWidgetCmd --
 *
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

static int TreeWidgetCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    int result = TCL_OK;
    static CONST char *commandName[] = {
	"activate", "bbox", "canvasx", "canvasy", "cget",
#ifdef DEPRECATED
	"collapse",
#endif
	"column",
#ifdef DEPRECATED
	"compare",
#endif
	"configure", "contentbox", "debug", "depth", "dragimage", "element",
#ifdef DEPRECATED
	"expand",
#endif
	"gradient",
	"identify", "index", "item", "marquee", "notify",
#ifdef DEPRECATED
	"numcolumns", "numitems",
#endif
	"orphans",
#ifdef DEPRECATED
	"range",
#endif
	"scan", "see", "selection", "state", "style", "theme",
#ifdef DEPRECATED
	"toggle",
#endif
	"xview", "yview", (char *) NULL
    };
    enum {
	COMMAND_ACTIVATE, COMMAND_BBOX, COMMAND_CANVASX, COMMAND_CANVASY,
	COMMAND_CGET,
#ifdef DEPRECATED
	COMMAND_COLLAPSE,
#endif
	COMMAND_COLUMN,
#ifdef DEPRECATED
	COMMAND_COMPARE,
#endif
	COMMAND_CONFIGURE, COMMAND_CONTENTBOX, COMMAND_DEBUG, COMMAND_DEPTH,
	COMMAND_DRAGIMAGE, COMMAND_ELEMENT,
#ifdef DEPRECATED
	COMMAND_EXPAND,
#endif
	COMMAND_GRADIENT,
	COMMAND_IDENTIFY, COMMAND_INDEX, COMMAND_ITEM, COMMAND_MARQUEE,
	COMMAND_NOTIFY,
#ifdef DEPRECATED
	COMMAND_NUMCOLUMNS, COMMAND_NUMITEMS,
#endif
	COMMAND_ORPHANS,
#ifdef DEPRECATED
	COMMAND_RANGE,
#endif
	COMMAND_SCAN, COMMAND_SEE, COMMAND_SELECTION, COMMAND_STATE,
	COMMAND_STYLE, COMMAND_THEME,
#ifdef DEPRECATED
	COMMAND_TOGGLE,
#endif
	COMMAND_XVIEW, COMMAND_YVIEW
    };
    Tcl_Obj *resultObjPtr;
    int index;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], commandName, "command", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    Tcl_Preserve((ClientData) tree);
    Tree_PreserveItems(tree);

    switch (index) {
	case COMMAND_ACTIVATE: {
	    TreeItem active, item;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "item");
		goto error;
	    }
	    if (TreeItem_FromObj(tree, objv[2], &item, IFO_NOT_NULL) != TCL_OK) {
		goto error;
	    }
	    if (item != tree->activeItem) {
		TreeRectangle tr;

		active = tree->activeItem;
		TreeItem_ChangeState(tree, active, STATE_ACTIVE, 0);
		tree->activeItem = item;
		TreeItem_ChangeState(tree, tree->activeItem, 0, STATE_ACTIVE);

		/* FIXME: is it onscreen? */
		/* FIXME: what if only lock columns displayed? */
		if (Tree_ItemBbox(tree, item, COLUMN_LOCK_NONE, &tr) >= 0) {
		    Tk_SetCaretPos(tree->tkwin, tr.x - tree->xOrigin,
			    tr.y - tree->yOrigin, tr.height);
		}
		TreeNotify_ActiveItem(tree, active, item);
	    }
	    break;
	}

	/* .t bbox ?area? */
	case COMMAND_BBOX: {
	    TreeRectangle tr;

	    if (objc > 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "?area?");
		goto error;
	    }
	    if (objc == 3) {
		int area;
		if (TreeArea_FromObj(tree, objv[2], &area) != TCL_OK) {
		    goto error;
		}
		if (!Tree_AreaBbox(tree, area, &tr))
		    break;
	    } else {
		TreeRect_SetXYWH(tr, 0, 0, Tk_Width(tree->tkwin),
			Tk_Height(tree->tkwin));
	    }
	    FormatResult(interp, "%d %d %d %d",
		    TreeRect_Left(tr), TreeRect_Top(tr),
		    TreeRect_Right(tr), TreeRect_Bottom(tr));
	    break;
	}

	case COMMAND_CANVASX: {
	    int x;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "x");
		goto error;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK)
		goto error;
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(x + tree->xOrigin));
	    break;
	}

	case COMMAND_CANVASY: {
	    int y;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "y");
		goto error;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[2], &y) != TCL_OK)
		goto error;
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(y + tree->yOrigin));
	    break;
	}

	case COMMAND_CGET: {
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "option");
		goto error;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) tree,
		    tree->optionTable, objv[2], tree->tkwin);
	    if (resultObjPtr == NULL) {
		result = TCL_ERROR;
	    } else {
		Tcl_SetObjResult(interp, resultObjPtr);
	    }
	    break;
	}

	case COMMAND_CONFIGURE: {
	    resultObjPtr = NULL;
	    if (objc <= 3) {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) tree,
			tree->optionTable,
			(objc == 2) ? (Tcl_Obj *) NULL : objv[2],
			tree->tkwin);
		if (resultObjPtr == NULL) {
		    result = TCL_ERROR;
		} else {
		    Tcl_SetObjResult(interp, resultObjPtr);
		}
	    } else {
		result = TreeConfigure(interp, tree, objc - 2, objv + 2, FALSE);
	    }
	    break;
	}

	case COMMAND_CONTENTBOX: {
	    TreeRectangle tr;

	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
		goto error;
	    }
	    if (Tree_AreaBbox(tree, TREE_AREA_CONTENT, &tr)) {
		FormatResult(interp, "%d %d %d %d",
		    TreeRect_Left(tr), TreeRect_Top(tr),
		    TreeRect_Right(tr), TreeRect_Bottom(tr));
	    }
	    break;
	}

#ifdef DEPRECATED
	/* T expand ?-recurse? I ... */
	case COMMAND_COLLAPSE:
	case COMMAND_EXPAND:
	case COMMAND_TOGGLE: {
	    char *s;
	    int recurse = 0;
	    int mode = 0; /* lint */
	    int i, j, count, len;
	    TreeItemList items, item2s;
	    TreeItem _item;
	    ItemForEach iter;

	    if (objc == 2)
		break;
	    s = Tcl_GetStringFromObj(objv[2], &len);
	    if (s[0] == '-') {
		if (strncmp(s, "-recurse", len)) {
		    FormatResult(interp, "bad option \"%s\": must be -recurse",
			    s);
		    goto error;
		}
		if (objc == 3)
		    break;
		recurse = 1;
	    }
	    switch (index) {
		case COMMAND_COLLAPSE:
		    mode = 0;
		    break;
		case COMMAND_EXPAND:
		    mode = 1;
		    break;
		case COMMAND_TOGGLE:
		    mode = -1;
		    break;
	    }
	    for (i = 2 + recurse; i < objc; i++) {
		if (TreeItemList_FromObj(tree, objv[i], &items,
			IFO_NOT_NULL) != TCL_OK) {
		    goto error;
		}
		TreeItemList_Init(tree, &item2s, 0);
		ITEM_FOR_EACH(_item, &items, NULL, &iter) {
		    TreeItemList_Append(&item2s, _item);
		    if (!iter.all && recurse) {
			TreeItem_ListDescendants(tree, _item, &item2s);
		    }
		}
		count = TreeItemList_Count(&item2s);
		for (j = 0; j < count; j++) {
		    _item = TreeItemList_Nth(&item2s, j);
		    TreeItem_OpenClose(tree, _item, mode);
		}
		TreeItemList_Free(&items);
		TreeItemList_Free(&item2s);
	    }
#ifdef SELECTION_VISIBLE
	    Tree_DeselectHidden(tree);
#endif
	    break;
	}
#endif /* DEPRECATED */

	case COMMAND_COLUMN: {
	    result = TreeColumnCmd(clientData, interp, objc, objv);
	    break;
	}

#ifdef DEPRECATED
	case COMMAND_COMPARE: {
	    TreeItem item1, item2;
	    static CONST char *opName[] = { "<", "<=", "==", ">=", ">", "!=", NULL };
	    int op, compare = 0, index1, index2;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 2, objv, "item1 op item2");
		goto error;
	    }
	    if (TreeItem_FromObj(tree, objv[2], &item1, IFO_NOT_NULL) != TCL_OK)
		goto error;
	    if (Tcl_GetIndexFromObj(interp, objv[3], opName, "comparison operator", 0,
		    &op) != TCL_OK)
		goto error;
	    if (TreeItem_FromObj(tree, objv[4], &item2, IFO_NOT_NULL) != TCL_OK)
		goto error;
	    if (TreeItem_RootAncestor(tree, item1) !=
		    TreeItem_RootAncestor(tree, item2)) {
		FormatResult(interp,
			"item %s%d and item %s%d don't share a common ancestor",
			tree->itemPrefix, TreeItem_GetID(tree, item1),
			tree->itemPrefix, TreeItem_GetID(tree, item2));
		goto error;
	    }
	    TreeItem_ToIndex(tree, item1, &index1, NULL);
	    TreeItem_ToIndex(tree, item2, &index2, NULL);
	    switch (op) {
		case 0: compare = index1 < index2; break;
		case 1: compare = index1 <= index2; break;
		case 2: compare = index1 == index2; break;
		case 3: compare = index1 >= index2; break;
		case 4: compare = index1 > index2; break;
		case 5: compare = index1 != index2; break;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(compare));
	    break;
	}
#endif /* DEPRECATED */

	case COMMAND_DEBUG: {
	    result = TreeDebugCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_DEPTH: {
	    TreeItem item;
	    int depth;

	    if (objc < 2 || objc > 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "?item?");
		goto error;
	    }
	    if (objc == 3) {
		if (TreeItem_FromObj(tree, objv[2], &item, IFO_NOT_NULL) != TCL_OK)
		    goto error;
		depth = TreeItem_GetDepth(tree, item);
		if (TreeItem_RootAncestor(tree, item) == tree->root)
		    depth++;
		Tcl_SetObjResult(interp, Tcl_NewIntObj(depth));
		break;
	    }
	    Tree_UpdateItemIndex(tree);
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->depth + 1));
	    break;
	}

	case COMMAND_DRAGIMAGE: {
	    result = TreeDragImageCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_ELEMENT: {
	    result = TreeElementCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_GRADIENT: {
	    result = TreeGradientCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_IDENTIFY: {
	    int x, y, width, height, depth;
	    TreeColumn treeColumn;
	    TreeItem item;
	    char buf[64];
	    int hit;
	    int lock;
	    int columnTreeLeft;
/*
  set id [$tree identify $x $y]
  "item I column C" : mouse is in column C of item I
  "item I column C elem E" : mouse is in element E in column C of item I
  "item I button" : mouse is in button-area of item I
  "item I line J" : mouse is near line coming from item J
  "header C ?left|right?" : mouse is in header column C
  "" : mouse is not in any item
*/
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "x y");
		goto error;
	    }
	    if (Tk_GetPixelsFromObj(interp, tree->tkwin, objv[2], &x) != TCL_OK)
		goto error;
	    if (Tk_GetPixelsFromObj(interp, tree->tkwin, objv[3], &y) != TCL_OK)
		goto error;

	    hit = Tree_HitTest(tree, x, y);

	    /* Require point inside borders */
	    if (hit == TREE_AREA_NONE)
		break;

	    if (hit == TREE_AREA_HEADER) {
		treeColumn = Tree_HeaderUnderPoint(tree, &x, &y, &width, &height,
			FALSE);
		if (treeColumn == tree->columnTail) {
		    strcpy(buf, "header tail");
		    if (x < 4)
			sprintf(buf + strlen(buf), " left");
		    Tcl_SetResult(interp, buf, TCL_VOLATILE);
		    break;
		} else if (treeColumn != NULL) {
		    sprintf(buf, "header %s%d", tree->columnPrefix,
			TreeColumn_GetID(treeColumn));
		    if (x < 4)
			sprintf(buf + strlen(buf), " left");
		    else if (x >= width - 4)
			sprintf(buf + strlen(buf), " right");
		    Tcl_SetResult(interp, buf, TCL_VOLATILE);
		    break;
		}
	    }

	    item = Tree_ItemUnderPoint(tree, &x, &y, FALSE);
	    if (item == NULL)
		break;

	    sprintf(buf, "item %s%d", tree->itemPrefix, TreeItem_GetID(tree, item)); /* TreeItem_ToObj() */
	    depth = TreeItem_GetDepth(tree, item);
	    if (item == tree->root)
		depth = (tree->showButtons && tree->showRootButton) ? 1 : 0;
	    else if (tree->showRoot)
	    {
		depth++;
		if (tree->showButtons && tree->showRootButton)
		    depth++;
	    }
	    else if (tree->showButtons && tree->showRootChildButtons)
		depth += 1;
	    else if (tree->showLines && tree->showRootLines)
		depth += 1;

	    lock = (hit == TREE_AREA_LEFT) ? COLUMN_LOCK_LEFT :
		(hit == TREE_AREA_RIGHT) ? COLUMN_LOCK_RIGHT :
		COLUMN_LOCK_NONE;

	    columnTreeLeft = tree->columnTreeLeft; /* canvas coords */
	    if (hit == TREE_AREA_CONTENT)
		columnTreeLeft -= tree->canvasPadX[PAD_TOP_LEFT]; /* item coords */

	    /* Point is in a line or button */
	    if (tree->columnTreeVis &&
		    (TreeColumn_Lock(tree->columnTree) == lock) &&
		    (x >= columnTreeLeft) &&
		    (x < columnTreeLeft + TreeColumn_UseWidth(tree->columnTree)) &&
		    (x < columnTreeLeft + depth * tree->useIndent)) {
		int column = (x - columnTreeLeft) / tree->useIndent + 1;
		if (column == depth) {
		    if (TreeItem_IsPointInButton(tree, item, x, y))
			sprintf(buf + strlen(buf), " button");
		} else if (tree->showLines) {
		    TreeItem sibling;
		    do {
			item = TreeItem_GetParent(tree, item);
		    } while (++column < depth);
		    sibling = TreeItem_NextSiblingVisible(tree, item);
		    if ((sibling != NULL) &&
			((TreeItem_GetParent(tree, sibling) != tree->root) ||
			tree->showRootLines))
			sprintf(buf + strlen(buf), " line %s%d", tree->itemPrefix,
				TreeItem_GetID(tree, item)); /* TreeItem_ToObj() */
		}
	    } else {
		TreeItem_Identify(tree, item, lock, x, y, buf);
	    }
	    Tcl_SetResult(interp, buf, TCL_VOLATILE);
	    break;
	}

#ifdef DEPRECATED
	case COMMAND_INDEX: {
	    TreeItem item;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "item");
		goto error;
	    }
	    if (TreeItem_FromObj(tree, objv[2], &item, 0) != TCL_OK) {
		goto error;
	    }
	    if (item != NULL)
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, item));
	    break;
	}
#endif /* DEPRECATED */

	case COMMAND_ITEM: {
	    result = TreeItemCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_MARQUEE: {
	    result = TreeMarqueeCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_NOTIFY: {
	    result = TreeNotifyCmd(clientData, interp, objc, objv);
	    break;
	}

#ifdef DEPRECATED
	case COMMAND_NUMCOLUMNS: {
	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
		goto error;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->columnCount));
	    break;
	}

	case COMMAND_NUMITEMS: {
	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
		goto error;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->itemCount));
	    break;
	}
#endif /* DEPRECATED */

	case COMMAND_ORPHANS: {
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;
	    Tcl_Obj *listObj;
	    TreeItem item;

	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
		goto error;
	    }

	    /* Pretty slow. Could keep a hash table of orphans */
	    listObj = Tcl_NewListObj(0, NULL);
	    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		if ((item != tree->root) &&
			(TreeItem_GetParent(tree, item) == NULL)) {
		    Tcl_ListObjAppendElement(interp, listObj,
			    TreeItem_ToObj(tree, item));
		}
		hPtr = Tcl_NextHashEntry(&search);
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

#ifdef DEPRECATED
	case COMMAND_RANGE: {
	    TreeItem item, itemFirst, itemLast;
	    Tcl_Obj *listObj;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "first last");
		goto error;
	    }
	    if (TreeItem_FromObj(tree, objv[2], &itemFirst, IFO_NOT_NULL) != TCL_OK)
		goto error;
	    if (TreeItem_FromObj(tree, objv[3], &itemLast, IFO_NOT_NULL) != TCL_OK)
		goto error;
	    if (TreeItem_FirstAndLast(tree, &itemFirst, &itemLast) == 0)
		goto error;
	    listObj = Tcl_NewListObj(0, NULL);
	    item = itemFirst;
	    while (item != NULL) {
		Tcl_ListObjAppendElement(interp, listObj,
			TreeItem_ToObj(tree, item));
		if (item == itemLast)
		    break;
		item = TreeItem_Next(tree, item);
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}
#endif /* DEPRECATED */

	case COMMAND_SCAN: {
	    static CONST char *optionName[] = { "dragto", "mark",
						(char *) NULL };
	    int x, y, gain = 10, xOrigin, yOrigin;

	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "option ?arg arg ...?");
		goto error;
	    }
	    if (Tcl_GetIndexFromObj(interp, objv[2], optionName, "option",
		    2, &index) != TCL_OK)
		goto error;
	    switch (index) {
		/* T scan dragto x y ?gain? */
		case 0:
		    if ((objc < 5) || (objc > 6)) {
			Tcl_WrongNumArgs(interp, 3, objv, "x y ?gain?");
			goto error;
		    }
		    if ((Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK) ||
			    (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK))
			goto error;
		    if (objc == 6) {
			if (Tcl_GetIntFromObj(interp, objv[5], &gain) != TCL_OK)
			    goto error;
		    }
		    xOrigin = tree->scanXOrigin - gain * (x - tree->scanX);
		    yOrigin = tree->scanYOrigin - gain * (y - tree->scanY);

		    Tree_SetScrollSmoothingX(tree, TRUE);
		    Tree_SetScrollSmoothingY(tree, TRUE);

		    Tree_SetOriginX(tree, xOrigin);
		    Tree_SetOriginY(tree, yOrigin);
		    break;

		/* T scan mark x y ?gain? */
		case 1:
		    if (objc != 5) {
			Tcl_WrongNumArgs(interp, 3, objv, "x y");
			goto error;
		    }
		    if ((Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK) ||
			    (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK))
			goto error;
		    tree->scanX = x;
		    tree->scanY = y;
		    tree->scanXOrigin = tree->xOrigin;
		    tree->scanYOrigin = tree->yOrigin;
		    break;
	    }
	    break;
	}

	case COMMAND_SEE: {
	    result = TreeSeeCmd(tree, objc, objv);
	    break;
	}

	case COMMAND_SELECTION: {
	    result = TreeSelectionCmd(interp, tree, objc, objv);
	    break;
	}

	case COMMAND_STATE: {
	    result = TreeStateCmd(tree, objc, objv);
	    break;
	}

	case COMMAND_STYLE: {
	    result = TreeStyleCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_THEME: {
	    result = TreeThemeCmd(tree, objc, objv);
	    break;
	}
	case COMMAND_XVIEW: {
	    result = TreeXviewCmd(tree, objc, objv);
	    break;
	}

	case COMMAND_YVIEW: {
	    result = TreeYviewCmd(tree, objc, objv);
	    break;
	}
    }
    Tree_ReleaseItems(tree);
    Tcl_Release((ClientData) tree);
    return result;

error:
    Tree_ReleaseItems(tree);
    Tcl_Release((ClientData) tree);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * TreeConfigure --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	a treectrl widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then the interp's result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for tree;  old resources get freed,
 *	if there were any.
 *
 *--------------------------------------------------------------
 */

static int
TreeConfigure(
    Tcl_Interp *interp,		/* Current interpreter. */
    TreeCtrl *tree,		/* Widget info. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[],	/* Argument values. */
    int createFlag		/* TRUE if the widget is being created. */
    )
{
    int error;
    Tcl_Obj *errorResult = NULL;
    TreeCtrl saved;
    Tk_SavedOptions savedOptions;
    int oldShowRoot = tree->showRoot;
    int buttonWidth, buttonHeight;
    int mask, maskFree = 0;
    XGCValues gcValues;
    unsigned long gcMask;

    /* Init these to prevent compiler warnings */
    saved.backgroundImage = NULL;
#ifdef DEPRECATED
    saved.defaultStyle.styles = NULL;
    saved.defaultStyle.numStyles = 0;
#endif
    saved.wrapMode = TREE_WRAP_NONE;
    saved.wrapArg = 0;
    saved.bgImageScroll = tree->bgImageScroll;
    saved.bgImageTile = tree->bgImageTile;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(interp, (char *) tree, tree->optionTable, objc,
		    objv, tree->tkwin, &savedOptions, &mask) != TCL_OK) {
		mask = 0;
		continue;
	    }

	    /* Wouldn't have to do this if Tk_InitOptions() would return
	    * a mask of configured options like Tk_SetOptions() does. */
	    if (createFlag) {
		if (tree->backgroundImageString != NULL)
		    mask |= TREE_CONF_BG_IMAGE;
		if (tree->buttonBitmap.obj != NULL)
		    mask |= TREE_CONF_BUTBMP;
		if (tree->buttonImage.obj != NULL)
		    mask |= TREE_CONF_BUTIMG;
#ifdef DEPRECATED
		if (tree->defaultStyle.stylesObj != NULL)
		    mask |= TREE_CONF_DEFSTYLE;
#endif
		if (tree->wrapObj != NULL)
		    mask |= TREE_CONF_WRAP;
		if (!ObjectIsEmpty(tree->itemWidthObj))
		    mask |= TREE_CONF_ITEMSIZE;
		if (!ObjectIsEmpty(tree->itemWidMultObj))
		    mask |= TREE_CONF_ITEMSIZE;
		mask |= TREE_CONF_BGIMGOPT;
	    }

	    /*
	     * Step 1: Save old values
	     */

	    if (mask & TREE_CONF_BG_IMAGE)
		saved.backgroundImage = tree->backgroundImage;
#ifdef DEPRECATED
	    if (mask & TREE_CONF_DEFSTYLE) {
		saved.defaultStyle.styles = tree->defaultStyle.styles;
		saved.defaultStyle.numStyles = tree->defaultStyle.numStyles;
	    }
#endif
	    if (mask & TREE_CONF_WRAP) {
		saved.wrapMode = tree->wrapMode;
		saved.wrapArg = tree->wrapArg;
	    }

	    /*
	     * Step 2: Process new values
	     */

	    if (mask & TREE_CONF_BG_IMAGE) {
		if (tree->backgroundImageString == NULL) {
		    tree->backgroundImage = NULL;
		} else {
		    Tk_Image image = Tree_GetImage(tree, tree->backgroundImageString);
		    if (image == NULL)
			continue;
		    tree->backgroundImage = image;
		    maskFree |= TREE_CONF_BG_IMAGE;
		}
	    }

	    if (mask & TREE_CONF_BGIMGOPT) {
		static const CharFlag scrollFlags[] = {
		    { 'x', BGIMG_SCROLL_X },
		    { 'y', BGIMG_SCROLL_Y },
		    { 0, 0 }
		};
		if (Tree_GetFlagsFromObj(tree, tree->bgImageScrollObj,
			"scroll value", scrollFlags, &tree->bgImageScroll)
			!= TCL_OK) {
		    continue;
		}
	    }

	    if (mask & TREE_CONF_BGIMGOPT) {
		static const CharFlag tileFlags[] = {
		    { 'x', BGIMG_TILE_X },
		    { 'y', BGIMG_TILE_Y },
		    { 0, 0 }
		};
		if (Tree_GetFlagsFromObj(tree, tree->bgImageTileObj,
			"tile value", tileFlags, &tree->bgImageTile)
			!= TCL_OK) {
		    continue;
		}
	    }

#ifdef DEPRECATED
	    if (mask & TREE_CONF_DEFSTYLE) {
		if (tree->defaultStyle.stylesObj == NULL) {
		    tree->defaultStyle.styles = NULL;
		    tree->defaultStyle.numStyles = 0;
		} else {
		    int i, listObjc;
		    Tcl_Obj **listObjv;
		    TreeStyle style;

		    if ((Tcl_ListObjGetElements(interp,
			tree->defaultStyle.stylesObj, &listObjc, &listObjv)
			!= TCL_OK)) continue;
		    tree->defaultStyle.styles =
			(TreeStyle *) ckalloc(sizeof(TreeStyle) * listObjc);
		    tree->defaultStyle.numStyles = listObjc;
		    for (i = 0; i < listObjc; i++) {
			if (ObjectIsEmpty(listObjv[i])) {
			    style = NULL;
			} else {
			    if (TreeStyle_FromObj(tree, listObjv[i], &style) != TCL_OK) {
				ckfree((char *) tree->defaultStyle.styles);
				break;
			    }
			}
			tree->defaultStyle.styles[i] = style;
		    }
		    if (i < listObjc)
			continue;
		    maskFree |= TREE_CONF_DEFSTYLE;
		}
	    }
#endif /* DEPRECATED */

	    /* Parse -wrap string into wrapMode and wrapArg */
	    if (mask & TREE_CONF_WRAP) {
		int listObjc;
		Tcl_Obj **listObjv;

		if (tree->wrapObj == NULL) {
		    tree->wrapMode = TREE_WRAP_NONE;
		    tree->wrapArg = 0;
		} else {
		    int len0, len1;
		    char *s0, *s1, ch0, ch1;

		    if ((Tcl_ListObjGetElements(interp, tree->wrapObj, &listObjc,
			    &listObjv) != TCL_OK) || (listObjc > 2)) {
badWrap:
			FormatResult(interp, "bad wrap \"%s\"",
				Tcl_GetString(tree->wrapObj));
			continue;
		    }
		    if (listObjc == 1) {
			s0 = Tcl_GetStringFromObj(listObjv[0], &len0);
			ch0 = s0[0];
			if ((ch0 == 'w') && !strncmp(s0, "window", len0)) {
			    tree->wrapMode = TREE_WRAP_WINDOW;
			    tree->wrapArg = 0;
			} else
			    goto badWrap;
		    } else {
			s1 = Tcl_GetStringFromObj(listObjv[1], &len1);
			ch1 = s1[0];
			if ((ch1 == 'i') && !strncmp(s1, "items", len1)) {
			    int n;
			    if ((Tcl_GetIntFromObj(interp, listObjv[0], &n) != TCL_OK) ||
				    (n < 0)) {
				goto badWrap;
			    }
			    tree->wrapMode = TREE_WRAP_ITEMS;
			    tree->wrapArg = n;
			}
			else if ((ch1 == 'p') && !strncmp(s1, "pixels", len1)) {
			    int n;
			    if (Tk_GetPixelsFromObj(interp, tree->tkwin, listObjv[0], &n)
				    != TCL_OK) {
				goto badWrap;
			    }
			    tree->wrapMode = TREE_WRAP_PIXELS;
			    tree->wrapArg = n;
			} else
			    goto badWrap;
		    }
		}
	    }

	    /*
	     * Step 3: Free saved values
	     */

	    if (mask & TREE_CONF_BG_IMAGE) {
		if (saved.backgroundImage != NULL)
		    Tree_FreeImage(tree, saved.backgroundImage);
	    }
#ifdef DEPRECATED
	    if (mask & TREE_CONF_DEFSTYLE) {
		if (saved.defaultStyle.styles != NULL)
		    ckfree((char *) saved.defaultStyle.styles);
	    }
#endif
	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    /*
	     * Free new values.
	     */
	    if (maskFree & TREE_CONF_BG_IMAGE)
		Tree_FreeImage(tree, tree->backgroundImage);
#ifdef DEPRECATED
	    if (maskFree & TREE_CONF_DEFSTYLE)
		ckfree((char *) tree->defaultStyle.styles);
#endif
	    /*
	     * Restore old values.
	     */
	    if (mask & TREE_CONF_BG_IMAGE) {
		tree->backgroundImage = saved.backgroundImage;
	    }
#ifdef DEPRECATED
	    if (mask & TREE_CONF_DEFSTYLE) {
		tree->defaultStyle.styles = saved.defaultStyle.styles;
		tree->defaultStyle.numStyles = saved.defaultStyle.numStyles;
	    }
#endif
	    if (mask & TREE_CONF_WRAP) {
		tree->wrapMode = saved.wrapMode;
		tree->wrapArg = saved.wrapArg;
	    }
	    if (mask & TREE_CONF_BGIMGOPT) {
		tree->bgImageScroll = saved.bgImageScroll;
		tree->bgImageTile = saved.bgImageTile;
	    }

	    Tcl_SetObjResult(interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    tree->itemPrefixLen = (int) strlen(tree->itemPrefix);
    tree->columnPrefixLen = (int) strlen(tree->columnPrefix);

    Tk_SetWindowBackground(tree->tkwin,
	    Tk_3DBorderColor(tree->border)->pixel);

    if (createFlag)
	mask |= TREE_CONF_FONT | TREE_CONF_RELAYOUT;

    if (mask & (TREE_CONF_FONT | TREE_CONF_FG)) {
	/*
	 * Should be blended into TreeWorldChanged.
	 */
	gcValues.font = Tk_FontId(tree->tkfont);
	gcValues.foreground = tree->fgColorPtr->pixel;
	gcValues.graphics_exposures = False;
	gcMask = GCForeground | GCFont | GCGraphicsExposures;
	if (tree->textGC != None)
	    Tk_FreeGC(tree->display, tree->textGC);
	tree->textGC = Tk_GetGC(tree->tkwin, gcMask, &gcValues);
    }

    if (tree->copyGC == None) {
	gcValues.function = GXcopy;
	gcValues.graphics_exposures = False;
	gcMask = GCFunction | GCGraphicsExposures;
	tree->copyGC = Tk_GetGC(tree->tkwin, gcMask, &gcValues);
    }

    if (createFlag)
	mask |= TREE_CONF_BUTTON;

    if (mask & TREE_CONF_BUTTON) {
	if (tree->buttonGC != None)
	    Tk_FreeGC(tree->display, tree->buttonGC);
	gcValues.foreground = tree->buttonColor->pixel;
	gcValues.line_width = tree->buttonThickness;
	gcMask = GCForeground | GCLineWidth;
	tree->buttonGC = Tk_GetGC(tree->tkwin, gcMask, &gcValues);
    }

    if (createFlag)
	mask |= TREE_CONF_LINE;

    if (mask & TREE_CONF_LINE) {
	if (tree->lineGC[0] != None)
	    Tk_FreeGC(tree->display, tree->lineGC[0]);
	if (tree->lineGC[1] != None)
	    Tk_FreeGC(tree->display, tree->lineGC[1]);
	if (tree->lineStyle == LINE_STYLE_DOT) {
	    gcValues.foreground = tree->lineColor->pixel;
	    gcValues.line_style = LineOnOffDash;
	    gcValues.line_width = 1;
	    gcValues.dash_offset = 0;
	    gcValues.dashes = 1;
	    gcMask = GCForeground | GCLineWidth | GCLineStyle | GCDashList | GCDashOffset;
	    tree->lineGC[0] = Tk_GetGC(tree->tkwin, gcMask, &gcValues);

	    gcValues.dash_offset = 1;
	    tree->lineGC[1] = Tk_GetGC(tree->tkwin, gcMask, &gcValues);
	} else {
	    gcValues.foreground = tree->lineColor->pixel;
	    gcValues.line_width = tree->lineThickness;
	    gcMask = GCForeground | GCLineWidth;
	    tree->lineGC[0] = Tk_GetGC(tree->tkwin, gcMask, &gcValues);
	    tree->lineGC[1] = None;
	}
    }

    if (mask & TREE_CONF_PROXY) {
	TreeColumnProxy_Undisplay(tree);
	TreeColumnProxy_Display(tree);
	TreeRowProxy_Undisplay(tree);
	TreeRowProxy_Display(tree);
    }

    Tree_ButtonMaxSize(tree, &buttonWidth, &buttonHeight);
    tree->useIndent = MAX(tree->indent, buttonWidth);
    tree->buttonHeightMax = buttonHeight;

    if (createFlag)
	mask |= TREE_CONF_BORDERS;

    if (mask & TREE_CONF_BORDERS) {
	if (tree->highlightWidth < 0)
	    tree->highlightWidth = 0;
	if (tree->useTheme && TreeTheme_SetBorders(tree) == TCL_OK) {
	    /* nothing */
	} else {
	    tree->inset.left = tree->inset.top =
	    tree->inset.right = tree->inset.bottom =
		    tree->highlightWidth + tree->borderWidth;
	}
    }

    if (oldShowRoot != tree->showRoot) {
	TreeItem_InvalidateHeight(tree, tree->root);
	tree->updateIndex = 1;
    }

    TreeStyle_TreeChanged(tree, mask);
    TreeColumn_TreeChanged(tree, mask);

    if ((tree->scrollSmoothing & SMOOTHING_X) && !tree->xScrollSmoothing)
	Tree_SetScrollSmoothingX(tree, FALSE);
    if ((tree->scrollSmoothing & SMOOTHING_Y) && !tree->yScrollSmoothing)
	Tree_SetScrollSmoothingY(tree, FALSE);

    if (mask & TREE_CONF_RELAYOUT) {
	TreeComputeGeometry(tree);
	Tree_InvalidateColumnWidth(tree, NULL);
	Tree_InvalidateColumnHeight(tree, NULL); /* In case -usetheme changes */
	Tree_RelayoutWindow(tree);
    } else if (mask & TREE_CONF_REDISPLAY) {
	Tree_RelayoutWindow(tree);
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TreeWorldChanged --
 *
 *	This procedure is called when the world has changed in some
 *	way and the widget needs to recompute all its graphics contexts
 *	and determine its new geometry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Widget will be relayed out and redisplayed.
 *
 *---------------------------------------------------------------------------
 */

static void
TreeWorldChanged(
    ClientData instanceData	/* Widget info. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) instanceData;
    XGCValues gcValues;
    unsigned long gcMask;

    gcValues.font = Tk_FontId(tree->tkfont);
    gcValues.foreground = tree->fgColorPtr->pixel;
    gcValues.graphics_exposures = False;
    gcMask = GCForeground | GCFont | GCGraphicsExposures;
    if (tree->textGC != None)
	Tk_FreeGC(tree->display, tree->textGC);
    tree->textGC = Tk_GetGC(tree->tkwin, gcMask, &gcValues);

    TreeStyle_TreeChanged(tree, TREE_CONF_FONT | TREE_CONF_RELAYOUT);
    TreeColumn_TreeChanged(tree, TREE_CONF_FONT | TREE_CONF_RELAYOUT);

    TreeComputeGeometry(tree);
    Tree_InvalidateColumnWidth(tree, NULL);
    Tree_RelayoutWindow(tree);
}

/*
 *--------------------------------------------------------------
 *
 * TreeEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on the widget.
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
TreeEventProc(
    ClientData clientData,	/* Widget info. */
    XEvent *eventPtr		/* Event info. */
    )
{
    TreeCtrl *tree = clientData;

    switch (eventPtr->type) {
	case Expose: {
	    int x = eventPtr->xexpose.x;
	    int y = eventPtr->xexpose.y;
	    Tree_ExposeArea(tree, x, y,
		    x + eventPtr->xexpose.width,
		    y + eventPtr->xexpose.height);
	    break;
	}
	case ConfigureNotify: {
	    if ((tree->prevWidth != Tk_Width(tree->tkwin)) ||
		    (tree->prevHeight != Tk_Height(tree->tkwin))) {
		tree->widthOfColumns = -1;
		tree->widthOfColumnsLeft = tree->widthOfColumnsRight = -1;
		Tree_RelayoutWindow(tree);
		tree->prevWidth = Tk_Width(tree->tkwin);
		tree->prevHeight = Tk_Height(tree->tkwin);
	    }
	    break;
	}
	case FocusIn:
	    /* Handle focus as Tile does */
	    if (   eventPtr->xfocus.detail == NotifyInferior
		|| eventPtr->xfocus.detail == NotifyAncestor
		|| eventPtr->xfocus.detail == NotifyNonlinear) {
		Tree_FocusChanged(tree, 1);
	    }
	    break;
	case FocusOut:
	    /* Handle focus as Tile does */
	    if (   eventPtr->xfocus.detail == NotifyInferior
		|| eventPtr->xfocus.detail == NotifyAncestor
		|| eventPtr->xfocus.detail == NotifyNonlinear) {
		Tree_FocusChanged(tree, 0);
	    }
	    break;
	case ActivateNotify:
	    Tree_Activate(tree, 1);
	    break;
	case DeactivateNotify:
	    Tree_Activate(tree, 0);
	    break;
	case DestroyNotify:
	    if (!tree->deleted) {
		tree->deleted = 1;
		Tcl_DeleteCommandFromToken(tree->interp, tree->widgetCmd);
		Tcl_EventuallyFree((ClientData) tree, TreeDestroy);
	    }
	    break;
#ifdef USE_TTK
	case VirtualEvent:
	    if (!strcmp("ThemeChanged", ((XVirtualEvent *)(eventPtr))->name)) {
		TreeTheme_ThemeChanged(tree);
		tree->widthOfColumns = -1;
		tree->widthOfColumnsLeft = tree->widthOfColumnsRight = -1;
		Tree_RelayoutWindow(tree);
	    }
	    break;
#endif
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeCmdDeletedProc --
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
TreeCmdDeletedProc(
    ClientData clientData	/* Widget info. */
    )
{
    TreeCtrl *tree = clientData;

    if (!tree->deleted) {
	Tk_DestroyWindow(tree->tkwin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDestroy --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a widget at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the widget is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
TreeDestroy(
    char *memPtr		/* Widget info. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) memPtr;
    TreeItem item;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    int i, count;

    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
    while (hPtr != NULL) {
	item = (TreeItem) Tcl_GetHashValue(hPtr);
	TreeItem_FreeResources(tree, item);
	hPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&tree->itemHash);

    Tcl_DeleteHashTable(&tree->itemSpansHash);

    count = TreeItemList_Count(&tree->preserveItemList);
    for (i = 0; i < count; i++) {
	item = TreeItemList_Nth(&tree->preserveItemList, i);
	TreeItem_Release(tree, item);
    }
    TreeItemList_Free(&tree->preserveItemList);

    TreeStyle_Free(tree);

    TreeDragImage_Free(tree->dragImage);
    TreeMarquee_Free(tree->marquee);
    TreeDInfo_Free(tree);
    TreeTheme_Free(tree);

    if (tree->copyGC != None)
	Tk_FreeGC(tree->display, tree->copyGC);
    if (tree->textGC != None)
	Tk_FreeGC(tree->display, tree->textGC);
    if (tree->buttonGC != None)
	Tk_FreeGC(tree->display, tree->buttonGC);
    if (tree->lineGC[0] != None)
	Tk_FreeGC(tree->display, tree->lineGC[0]);
    if (tree->lineGC[1] != None)
	Tk_FreeGC(tree->display, tree->lineGC[1]);
    Tree_FreeAllGC(tree);

    Tree_FreeColumns(tree);

    while (tree->regionStackLen > 0)
	TkDestroyRegion(tree->regionStack[--tree->regionStackLen]);

    QE_DeleteBindingTable(tree->bindingTable);

    for (i = STATE_USER - 1; i < 32; i++)
	if (tree->stateNames[i] != NULL)
	    ckfree(tree->stateNames[i]);

    Tk_FreeConfigOptions((char *) tree, tree->debug.optionTable,
	    tree->tkwin);

    Tk_FreeConfigOptions((char *) tree, tree->optionTable, tree->tkwin);

    hPtr = Tcl_FirstHashEntry(&tree->imageNameHash, &search);
    while (hPtr != NULL) {
	TreeImageRef *ref = (TreeImageRef *) Tcl_GetHashValue(hPtr);
	Tk_FreeImage(ref->image);
	ckfree((char *) ref);
	hPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&tree->imageNameHash);
    Tcl_DeleteHashTable(&tree->imageTokenHash);

    Tcl_DeleteHashTable(&tree->selection);

    /* Must be done after all gradient users are freed */ 
    TreeGradient_Free(tree);

#ifdef DEPRECATED
    if (tree->defaultStyle.styles != NULL)
	ckfree((char *) tree->defaultStyle.styles);
#endif
#ifdef ALLOC_HAX
    TreeAlloc_Finalize(tree->allocData);
#endif

    Tcl_Release(tree->tkwin);
    WFREE(tree, TreeCtrl);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_UpdateScrollbarX --
 *
 *	This procedure is invoked whenever information has changed in
 *	a widget in a way that would invalidate a scrollbar display.
 *
 *	A <Scroll-x> event is generated.
 *
 *	If there is an associated scrollbar, then this procedure updates
 *	it by invoking a Tcl command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A Tcl command is invoked, and an additional command may be
 *	invoked to process errors in the command.
 *
 *----------------------------------------------------------------------
 */

void
Tree_UpdateScrollbarX(
    TreeCtrl *tree		/* Widget info. */
    )
{
    Tcl_Interp *interp = tree->interp;
    int result;
    double fractions[2];
    char buf1[TCL_DOUBLE_SPACE+1];
    char buf2[TCL_DOUBLE_SPACE+1];
    char *xScrollCmd;

    Tree_GetScrollFractionsX(tree, fractions);
    TreeNotify_Scroll(tree, fractions, FALSE);

    if (tree->xScrollCmd == NULL)
	return;

    Tcl_Preserve((ClientData) interp);
    Tcl_Preserve((ClientData) tree);

    xScrollCmd = tree->xScrollCmd;
    Tcl_Preserve((ClientData) xScrollCmd);
    buf1[0] = buf2[0] = ' ';
    Tcl_PrintDouble(NULL, fractions[0], buf1+1);
    Tcl_PrintDouble(NULL, fractions[1], buf2+1);
    result = Tcl_VarEval(interp, xScrollCmd, buf1, buf2, (char *) NULL);
    if (result != TCL_OK)
	Tcl_BackgroundError(interp);
    Tcl_ResetResult(interp);
    Tcl_Release((ClientData) xScrollCmd);

    Tcl_Release((ClientData) tree);
    Tcl_Release((ClientData) interp);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_UpdateScrollbarY --
 *
 *	This procedure is invoked whenever information has changed in
 *	a widget in a way that would invalidate a scrollbar display.
 *
 *	A <Scroll-y> event is generated.
 *
 *	If there is an associated scrollbar, then this procedure updates
 *	it by invoking a Tcl command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A Tcl command is invoked, and an additional command may be
 *	invoked to process errors in the command.
 *
 *----------------------------------------------------------------------
 */

void
Tree_UpdateScrollbarY(
    TreeCtrl *tree		/* Widget info. */
    )
{
    Tcl_Interp *interp = tree->interp;
    int result;
    double fractions[2];
    char buf1[TCL_DOUBLE_SPACE+1];
    char buf2[TCL_DOUBLE_SPACE+1];
    char *yScrollCmd;

    Tree_GetScrollFractionsY(tree, fractions);
    TreeNotify_Scroll(tree, fractions, TRUE);

    if (tree->yScrollCmd == NULL)
	return;

    Tcl_Preserve((ClientData) interp);
    Tcl_Preserve((ClientData) tree);

    yScrollCmd = tree->yScrollCmd;
    Tcl_Preserve((ClientData) yScrollCmd);
    buf1[0] = buf2[0] = ' ';
    Tcl_PrintDouble(NULL, fractions[0], buf1+1);
    Tcl_PrintDouble(NULL, fractions[1], buf2+1);
    result = Tcl_VarEval(interp, yScrollCmd, buf1, buf2, (char *) NULL);
    if (result != TCL_OK)
	Tcl_BackgroundError(interp);
    Tcl_ResetResult(interp);
    Tcl_Release((ClientData) yScrollCmd);

    Tcl_Release((ClientData) tree);
    Tcl_Release((ClientData) interp);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeComputeGeometry --
 *
 *	This procedure is invoked to compute the requested size for the
 *	window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tk_GeometryRequest is called to register the desired dimensions
 *	for the window.
 *
 *----------------------------------------------------------------------
 */

static void
TreeComputeGeometry(
    TreeCtrl *tree		/* Widget info. */
    )
{
    Tk_SetInternalBorderEx(tree->tkwin,
	    tree->inset.left, tree->inset.right,
	    tree->inset.top, tree->inset.bottom);
    Tk_GeometryRequest(tree->tkwin,
	    tree->width + tree->inset.left + tree->inset.right,
	    tree->height + tree->inset.top + tree->inset.bottom);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_AddItem --
 *
 *	Add an item to the hash table of items. Also set the unique item
 *	id and increment the number of items.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tree_AddItem(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item that was created. */
    )
{
    Tcl_HashEntry *hPtr;
    int id, isNew;

    id = TreeItem_SetID(tree, item, tree->nextItemId++);
    hPtr = Tcl_CreateHashEntry(&tree->itemHash, (char *) INT2PTR(id), &isNew);
    Tcl_SetHashValue(hPtr, item);
    tree->itemCount++;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_RemoveItem --
 *
 *	Remove an item from the selection, if selected.
 *	Remove an item from the hash table of items.
 *	Decrement the number of items.
 *	Reset the unique item id allocator if the last item is removed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tree_RemoveItem(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item to remove. */
    )
{
    Tcl_HashEntry *hPtr;

    if (TreeItem_GetSelected(tree, item))
	Tree_RemoveFromSelection(tree, item);

    hPtr = Tcl_FindHashEntry(&tree->itemSpansHash, (char *) item);
    if (hPtr != NULL)
	Tcl_DeleteHashEntry(hPtr);

    hPtr = Tcl_FindHashEntry(&tree->itemHash,
	    (char *) INT2PTR(TreeItem_GetID(tree, item)));
    Tcl_DeleteHashEntry(hPtr);
    tree->itemCount--;
    if (tree->itemCount == 1)
	tree->nextItemId = TreeItem_GetID(tree, tree->root) + 1;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageChangedProc --
 *
 *	This procedure is invoked by the image code whenever the manager
 *	for an image does something that affects the image's size or
 *	how it is displayed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the widget to get redisplayed.
 *
 *----------------------------------------------------------------------
 */

static void
ImageChangedProc(
    ClientData clientData,		/* Widget info. */
    int x, int y,			/* Upper left pixel (within image)
					 * that must be redisplayed. */
    int width, int height,		/* Dimensions of area to redisplay
					 * (may be <= 0). */
    int imageWidth, int imageHeight	/* New dimensions of image. */
    )
{
    /* I would like to know the image was deleted... */
    TreeCtrl *tree = clientData;

    /* FIXME: any image elements need to have their size invalidated
     * and items relayout'd accordingly. */

    /* FIXME: this is used for the background image, but whitespace
     * is not redrawn if the background image is modified. */

    Tree_DInfoChanged(tree, DINFO_INVALIDATE | DINFO_OUT_OF_DATE);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_GetImage --
 *
 *	Wrapper around Tk_GetImage(). If the requested image does not yet
 *	exist it is created. Otherwise an existing instance is returned.
 *
 *	The purpose of this procedure is to save memory. We may expect
 *	the same image to be used hundreds of times (a folder image for
 *	example) and want to avoid allocating an instance for every usage.
 *
 *	For each call to this function, there must be a matching call
 *	to Tree_FreeImage.
 *
 * Results:
 *	Token for the image instance. If an error occurs the result is
 *	NULL and a message is left in the interpreter's result.
 *
 * Side effects:
 *	A new image instance may be created.
 *
 *----------------------------------------------------------------------
 */

Tk_Image
Tree_GetImage(
    TreeCtrl *tree,		/* Widget info. */
    char *imageName		/* Name of an existing image. */
    )
{
    Tcl_HashEntry *hPtr, *h2Ptr;
    TreeImageRef *ref;
    Tk_Image image;
    int isNew;

    hPtr = Tcl_CreateHashEntry(&tree->imageNameHash, imageName, &isNew);
    if (isNew) {
	image = Tk_GetImage(tree->interp, tree->tkwin, imageName,
		ImageChangedProc, (ClientData) tree);
	if (image == NULL) {
	    Tcl_DeleteHashEntry(hPtr);
	    return NULL;
	}
	ref = (TreeImageRef *) ckalloc(sizeof(TreeImageRef));
	ref->count = 0;
	ref->image = image;
	ref->hPtr = hPtr;
	Tcl_SetHashValue(hPtr, ref);

	h2Ptr = Tcl_CreateHashEntry(&tree->imageTokenHash, (char *) image,
		&isNew);
	Tcl_SetHashValue(h2Ptr, ref);
    }
    ref = (TreeImageRef *) Tcl_GetHashValue(hPtr);
    ref->count++;
    return ref->image;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_FreeImage --
 *
 *	Decrement the reference count on an image.
 *
 * Results:
 *	If the reference count hits zero, frees the image instance and
 *	hash table entries.
 *
 * Side effects:
 *	Memory may be freed.
 *
 *----------------------------------------------------------------------
 */

void
Tree_FreeImage(
    TreeCtrl *tree,		/* Widget info. */
    Tk_Image image		/* Image token. */
    )
{
    Tcl_HashEntry *hPtr;
    TreeImageRef *ref;

    hPtr = Tcl_FindHashEntry(&tree->imageTokenHash, (char *) image);
    if (hPtr != NULL) {
	ref = (TreeImageRef *) Tcl_GetHashValue(hPtr);
	if (--ref->count == 0) {
	    Tcl_DeleteHashEntry(ref->hPtr); /* imageNameHash */
	    Tcl_DeleteHashEntry(hPtr);
	    Tk_FreeImage(ref->image);
	    ckfree((char *) ref);
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * TreeSeeCmd --
 *
 *	This procedure is invoked to process the [see] widget
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
TreeSeeCmd(
    TreeCtrl *tree,		/* Widget info. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    Tcl_Interp *interp = tree->interp;
    TreeItem item;
    TreeColumn treeColumn = NULL;
    TreeRectangle tr;
    int x, y, w, h;
    int visWidth = Tree_ContentWidth(tree);
    int visHeight = Tree_ContentHeight(tree);
    int xOrigin = Tree_GetOriginX(tree);
    int yOrigin = Tree_GetOriginY(tree);
    int minX = Tree_ContentLeft(tree);
    int minY = Tree_ContentTop(tree);
    int maxX = Tree_ContentRight(tree);
    int maxY = Tree_ContentBottom(tree);
    int index, offset;
    int centerX = 0, centerY = 0;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "item ?column? ?option value ...?");
	return TCL_ERROR;
    }
    if (TreeItem_FromObj(tree, objv[2], &item, IFO_NOT_NULL) != TCL_OK)
	return TCL_ERROR;

    if (objc > 3) {
	int i, len, firstOption = 3;
	char *s = Tcl_GetStringFromObj(objv[3], &len);
	if (s[0] != '-') {
	    if (TreeColumn_FromObj(tree, objv[3], &treeColumn,
		    CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
		return TCL_ERROR;
	    firstOption = 4;
	}

	for (i = firstOption; i < objc; i += 2)
	{
	    static CONST char *optionNames[] = {
		"-center", (char *) NULL
	    };
	    if (Tcl_GetIndexFromObj(interp, objv[i], optionNames,
		    "option", 0, &index) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (i + 1 == objc) {
		FormatResult(interp, "missing value for \"%s\" option",
			optionNames[index]);
		return TCL_ERROR;
	    }
	    switch (index)
	    {
		case 0: { /* -center */
		    static const CharFlag centerFlags[] = {
			{ 'x', 0x01 },
			{ 'y', 0x02 },
			{ 0, 0 }
		    };
		    int flags = 0;
		    if (Tree_GetFlagsFromObj(tree, objv[i+1], "-center value",
			    centerFlags, &flags) != TCL_OK) {
			return TCL_ERROR;
		    }
		    centerX = (flags & 0x01) != 0;
		    centerY = (flags & 0x02) != 0;
		    break;
		}
	    }
	}
    }

    /* Get the item bounds in canvas coords. */
    if (Tree_ItemBbox(tree, item, COLUMN_LOCK_NONE, &tr) < 0)
	return TCL_OK;

    TreeRect_XYWH(tr, &x, &y, &w, &h);

    if (treeColumn != NULL) {
	x += TreeColumn_Offset(treeColumn);
	w = TreeColumn_UseWidth(treeColumn);
    }

    Tree_SetScrollSmoothingX(tree, TRUE);
    Tree_SetScrollSmoothingY(tree, TRUE);

    /* No horizontal scrolling for locked columns. */
    if ((treeColumn != NULL) &&
	    (TreeColumn_Lock(treeColumn) != COLUMN_LOCK_NONE)) {
	/* nothing */

    /* Center the item or column horizontally. */
    } else if (centerX) {
	index = Increment_FindX(tree, x + w/2 - visWidth/2);
	offset = Increment_ToOffsetX(tree, index);
	if (offset < x + w/2 - visWidth/2) {
	    index++;
	    offset = Increment_ToOffsetX(tree, index);
	}
	xOrigin = C2Ox(offset);

    /* Scroll horizontally a minimal amount. */
    } else if ((C2Wx(x) > maxX) || (C2Wx(x + w) <= minX) || (w <= visWidth)) {
	if ((C2Wx(x) < minX) || (w > visWidth)) {
	    index = Increment_FindX(tree, x);
	    offset = Increment_ToOffsetX(tree, index);
	    xOrigin = C2Ox(offset);
	}
	else if (C2Wx(x + w) > maxX) {
	    index = Increment_FindX(tree, x + w - visWidth);
	    offset = Increment_ToOffsetX(tree, index);
	    if (offset < x + w - visWidth) {
		index++;
		offset = Increment_ToOffsetX(tree, index);
	    }
	    xOrigin = C2Ox(offset);
	}
    }

    /* Center the item or column vertically. */
    if (centerY) {
	index = Increment_FindY(tree, y + h/2 - visHeight/2);
	offset = Increment_ToOffsetY(tree, index);
	if (offset < y + h/2 - visHeight/2) {
	    index++;
	    offset = Increment_ToOffsetY(tree, index);
	}
	yOrigin = C2Oy(offset);

    /* Scroll vertically a minimal amount. */
    } else if ((C2Wy(y) > maxY) || (C2Wy(y + h) <= minY) || (h <= visHeight)) {
	if ((C2Wy(y) < minY) || (h > visHeight)) {
	    index = Increment_FindY(tree, y);
	    offset = Increment_ToOffsetY(tree, index);
	    yOrigin = C2Oy(offset);
	}
	else if (C2Wy(y + h) > maxY) {
	    index = Increment_FindY(tree, y + h - visHeight);
	    offset = Increment_ToOffsetY(tree, index);
	    if (offset < y + h - visHeight) {
		index++;
		offset = Increment_ToOffsetY(tree, index);
	    }
	    yOrigin = C2Oy(offset);
	}
    }

    Tree_SetOriginX(tree, xOrigin);
    Tree_SetOriginY(tree, yOrigin);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_StateFromObj --
 *
 *	Parse a Tcl_Obj containing a state name (with optional modifers)
 *	into a STATE_xxx flag, and modify an existing array of state
 *	flags accordingly.
 *
 *	If the object contains "foo", then the state "foo" is set on.
 *	If the object contains "!foo", then the state "foo" is set off.
 *	If the object contains "~foo", then the state "foo" is toggled.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tree_StateFromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *obj,		/* String rep of the state. */
    int states[3],		/* Initialized state flags, indexed by the
				 * STATE_OP_xxx contants. A single flag
				 * may be turned on or off in each value. */
    int *indexPtr,		/* Returned index of the STATE_xxx flag.
				 * May be NULL. */
    int flags			/* SFO_xxx flags. */
    )
{
    Tcl_Interp *interp = tree->interp;
    int i, op = STATE_OP_ON, op2, op3, length, state = 0;
    char ch0, *string;

    string = Tcl_GetStringFromObj(obj, &length);
    if (length == 0)
	goto unknown;
    ch0 = string[0];
    if (ch0 == '!') {
	if (flags & SFO_NOT_OFF) {
	    FormatResult(interp, "can't specify '!' for this command");
	    return TCL_ERROR;
	}
	op = STATE_OP_OFF;
	++string;
	ch0 = string[0];
    } else if (ch0 == '~') {
	if (flags & SFO_NOT_TOGGLE) {
	    FormatResult(interp, "can't specify '~' for this command");
	    return TCL_ERROR;
	}
	op = STATE_OP_TOGGLE;
	++string;
	ch0 = string[0];
    }
    for (i = 0; i < 32; i++) {
	if (tree->stateNames[i] == NULL)
	    continue;
	if ((ch0 == tree->stateNames[i][0]) &&
		(strcmp(string, tree->stateNames[i]) == 0)) {
	    if ((i < STATE_USER - 1) && (flags & SFO_NOT_STATIC)) {
		FormatResult(interp,
			"can't specify state \"%s\" for this command",
			tree->stateNames[i]);
		return TCL_ERROR;
	    }
	    state = 1L << i;
	    break;
	}
    }
    if (state == 0)
	goto unknown;

    if (states != NULL) {
	if (op == STATE_OP_ON) {
	    op2 = STATE_OP_OFF;
	    op3 = STATE_OP_TOGGLE;
	}
	else if (op == STATE_OP_OFF) {
	    op2 = STATE_OP_ON;
	    op3 = STATE_OP_TOGGLE;
	} else {
	    op2 = STATE_OP_ON;
	    op3 = STATE_OP_OFF;
	}
	states[op2] &= ~state;
	states[op3] &= ~state;
	states[op] |= state;
    }
    if (indexPtr != NULL) (*indexPtr) = i;
    return TCL_OK;

unknown:
    FormatResult(interp, "unknown state \"%s\"", string);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_StateFromListObj --
 *
 *	Call Tree_StateFromObj for a Tcl_Obj list object.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tree_StateFromListObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *obj,		/* List of states. */
    int states[3],		/* Uninitialized state flags, indexed by the
				 * STATE_OP_xxx contants. A single flag
				 * may be turned on or off in each value. */
    int flags			/* SFO_xxx flags. */
    )
{
    Tcl_Interp *interp = tree->interp;
    int i, listObjc;
    Tcl_Obj **listObjv;

    states[0] = states[1] = states[2] = 0;
    if (Tcl_ListObjGetElements(interp, obj, &listObjc, &listObjv) != TCL_OK)
	return TCL_ERROR;
    for (i = 0; i < listObjc; i++) {
	if (Tree_StateFromObj(tree, listObjv[i], states, NULL, flags) != TCL_OK)
	    return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TreeStateCmd --
 *
 *	This procedure is invoked to process the [state] widget
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
TreeStateCmd(
    TreeCtrl *tree,		/* Widget info. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    Tcl_Interp *interp = tree->interp;
    static CONST char *commandName[] = {
	"define", "linkage", "names",  "undefine", (char *) NULL
    };
    enum {
	COMMAND_DEFINE, COMMAND_LINKAGE, COMMAND_NAMES, COMMAND_UNDEFINE
    };
    int index;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandName, "command", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
	case COMMAND_DEFINE: {
	    char *string;
	    int i, length, slot = -1;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "stateName");
		return TCL_ERROR;
	    }
	    string = Tcl_GetStringFromObj(objv[3], &length);
	    if (!length || (*string == '~') || (*string == '!')) {
		FormatResult(interp, "invalid state name \"%s\"", string);
		return TCL_ERROR;
	    }
	    for (i = 0; i < 32; i++) {
		if (tree->stateNames[i] == NULL) {
		    if (slot == -1)
			slot = i;
		    continue;
		}
		if (strcmp(tree->stateNames[i], string) == 0) {
		    FormatResult(interp, "state \"%s\" already defined", string);
		    return TCL_ERROR;
		}
	    }
	    if (slot == -1) {
		FormatResult(interp, "cannot define any more states");
		return TCL_ERROR;
	    }
	    tree->stateNames[slot] = ckalloc(length + 1);
	    strcpy(tree->stateNames[slot], string);
	    break;
	}

	case COMMAND_LINKAGE: {
	    int index;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "state");
		return TCL_ERROR;
	    }
	    if (Tree_StateFromObj(tree, objv[3], NULL, &index,
		    SFO_NOT_OFF | SFO_NOT_TOGGLE) != TCL_OK)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		(index < STATE_USER - 1) ? "static" : "dynamic", -1));
	    break;
	}

	case COMMAND_NAMES: {
	    Tcl_Obj *listObj;
	    int i;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
		return TCL_ERROR;
	    }
	    listObj = Tcl_NewListObj(0, NULL);
	    for (i = STATE_USER - 1; i < 32; i++) {
		if (tree->stateNames[i] != NULL)
		    Tcl_ListObjAppendElement(interp, listObj,
			    Tcl_NewStringObj(tree->stateNames[i], -1));
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	case COMMAND_UNDEFINE: {
	    int i, index;

	    for (i = 3; i < objc; i++) {
		if (Tree_StateFromObj(tree, objv[i], NULL, &index,
			SFO_NOT_STATIC | SFO_NOT_OFF | SFO_NOT_TOGGLE) != TCL_OK)
		    return TCL_ERROR;
		Tree_UndefineState(tree, 1L << index);
		PerStateInfo_Undefine(tree, &pstBitmap, &tree->buttonBitmap,
			1L << index);
		PerStateInfo_Undefine(tree, &pstImage, &tree->buttonImage,
			1L << index);
		ckfree(tree->stateNames[index]);
		tree->stateNames[index] = NULL;
	    }
	    break;
	}
    }

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_AddToSelection --
 *
 *	Add an item to the hash table of selected items. Turn on the
 *	STATE_SELECTED state for the item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget may be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_AddToSelection(
    TreeCtrl *tree,		/* Widget info */
    TreeItem item		/* Item to add to the selection. */
    )
{
    Tcl_HashEntry *hPtr;
    int isNew;

#ifdef SELECTION_VISIBLE
    if (!TreeItem_ReallyVisible(tree, item))
	panic("Tree_AddToSelection: item %d not ReallyVisible",
		TreeItem_GetID(tree, item));
#endif
    if (TreeItem_GetSelected(tree, item))
	panic("Tree_AddToSelection: item %d already selected",
		TreeItem_GetID(tree, item));
    if (!TreeItem_GetEnabled(tree, item))
	panic("Tree_AddToSelection: item %d not enabled",
		TreeItem_GetID(tree, item));
    TreeItem_ChangeState(tree, item, 0, STATE_SELECTED);
    hPtr = Tcl_CreateHashEntry(&tree->selection, (char *) item, &isNew);
    if (!isNew)
	panic("Tree_AddToSelection: item %d already in selection hash table",
		TreeItem_GetID(tree, item));
    tree->selectCount++;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_RemoveFromSelection --
 *
 *	Remove an item from the hash table of selected items. Turn off the
 *	STATE_SELECTED state for the item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget may be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_RemoveFromSelection(
    TreeCtrl *tree,		/* Widget info */
    TreeItem item		/* Item to remove from the selection. */
    )
{
    Tcl_HashEntry *hPtr;

    if (!TreeItem_GetSelected(tree, item))
	panic("Tree_RemoveFromSelection: item %d isn't selected",
		TreeItem_GetID(tree, item));
    TreeItem_ChangeState(tree, item, STATE_SELECTED, 0);
    hPtr = Tcl_FindHashEntry(&tree->selection, (char *) item);
    if (hPtr == NULL)
	panic("Tree_RemoveFromSelection: item %d not found in selection hash table",
		TreeItem_GetID(tree, item));
    Tcl_DeleteHashEntry(hPtr);
    tree->selectCount--;
}

/*
 *--------------------------------------------------------------
 *
 * TreeSelectionCmd --
 *
 *	This procedure is invoked to process the [selection] widget
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
TreeSelectionCmd(
    Tcl_Interp *interp,		/* Current interpreter. */
    TreeCtrl *tree,		/* Widget info. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    static CONST char *commandName[] = {
	"add", "anchor", "clear", "count", "get", "includes", "modify", NULL
    };
    enum {
	COMMAND_ADD, COMMAND_ANCHOR, COMMAND_CLEAR, COMMAND_COUNT,
	COMMAND_GET, COMMAND_INCLUDES, COMMAND_MODIFY
    };
    int index;
    TreeItemList itemsFirst, itemsLast;
    TreeItem item, itemFirst, itemLast;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandName, "command", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
	case COMMAND_ADD: {
	    int i, count;
	    TreeItemList items;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;

	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "first ?last?");
		return TCL_ERROR;
	    }
	    if (TreeItemList_FromObj(tree, objv[3], &itemsFirst, IFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    itemFirst = TreeItemList_Nth(&itemsFirst, 0);
	    itemLast = NULL;
	    if (objc == 5) {
		if (TreeItemList_FromObj(tree, objv[4], &itemsLast, IFO_NOT_NULL) != TCL_OK) {
		    TreeItemList_Free(&itemsFirst);
		    return TCL_ERROR;
		}
		itemLast = TreeItemList_Nth(&itemsLast, 0);
	    }
	    if ((itemFirst == ITEM_ALL) || (itemLast == ITEM_ALL)) {
		if (objc == 5) TreeItemList_Free(&itemsLast);
		TreeItemList_Init(tree, &items,
			tree->itemCount - tree->selectCount);

		/* Include orphans. */
		hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		while (hPtr != NULL) {
		    item = (TreeItem) Tcl_GetHashValue(hPtr);
		    if (TreeItem_CanAddToSelection(tree, item)) {
			Tree_AddToSelection(tree, item);
			TreeItemList_Append(&items, item);
		    }
		    hPtr = Tcl_NextHashEntry(&search);
		}
		goto doneADD;
	    }
	    if (objc == 5) {
		TreeItemList_Free(&itemsFirst);
		TreeItemList_Free(&itemsLast);
		count = TreeItem_FirstAndLast(tree, &itemFirst, &itemLast);
		if (count == 0)
		    return TCL_ERROR;
		TreeItemList_Init(tree, &items, count);
		while (1) {
		    if (TreeItem_CanAddToSelection(tree, itemFirst)) {
			Tree_AddToSelection(tree, itemFirst);
			TreeItemList_Append(&items, itemFirst);
		    }
		    if (itemFirst == itemLast)
			break;
		    itemFirst = TreeItem_Next(tree, itemFirst);
		}
		goto doneADD;
	    }
	    count = TreeItemList_Count(&itemsFirst);
	    TreeItemList_Init(tree, &items, count);
	    for (i = 0; i < count; i++) {
		item = TreeItemList_Nth(&itemsFirst, i);
		if (TreeItem_CanAddToSelection(tree, item)) {
		    Tree_AddToSelection(tree, item);
		    TreeItemList_Append(&items, item);
		}
	    }
doneADD:
	    if (TreeItemList_Count(&items)) {
		TreeNotify_Selection(tree, &items, NULL);
	    }
	    TreeItemList_Free(&items);
	    TreeItemList_Free(&itemsFirst);
	    break;
	}

	case COMMAND_ANCHOR: {
	    if (objc != 3 && objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "?item?");
		return TCL_ERROR;
	    }
	    if (objc == 4) {
		if (TreeItem_FromObj(tree, objv[3], &item, IFO_NOT_NULL) != TCL_OK) {
		    return TCL_ERROR;
		}
		tree->anchorItem = item;
	    }
	    Tcl_SetObjResult(interp, TreeItem_ToObj(tree, tree->anchorItem));
	    break;
	}

	case COMMAND_CLEAR: {
	    int i, count;
	    TreeItemList items;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;

	    if (objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "?first? ?last?");
		return TCL_ERROR;
	    }
	    itemFirst = itemLast = NULL;
	    if (objc >= 4) {
		if (TreeItemList_FromObj(tree, objv[3], &itemsFirst, IFO_NOT_NULL) != TCL_OK)
		    return TCL_ERROR;
		itemFirst = TreeItemList_Nth(&itemsFirst, 0);
	    }
	    if (objc == 5) {
		if (TreeItemList_FromObj(tree, objv[4], &itemsLast, IFO_NOT_NULL) != TCL_OK) {
		    TreeItemList_Free(&itemsFirst);
		    return TCL_ERROR;
		}
		itemLast = TreeItemList_Nth(&itemsLast, 0);
	    }
	    if (tree->selectCount < 1) {
		if (objc >= 4) TreeItemList_Free(&itemsFirst);
		if (objc == 5) TreeItemList_Free(&itemsLast);
		break;
	    }
	    if ((objc == 3) || (itemFirst == ITEM_ALL) || (itemLast == ITEM_ALL)) {
		if (objc >= 4) TreeItemList_Free(&itemsFirst);
		if (objc == 5) TreeItemList_Free(&itemsLast);
		TreeItemList_Init(tree, &items, tree->selectCount);
		hPtr = Tcl_FirstHashEntry(&tree->selection, &search);
		while (hPtr != NULL) {
		    item = (TreeItem) Tcl_GetHashKey(&tree->selection, hPtr);
		    TreeItemList_Append(&items, item);
		    hPtr = Tcl_NextHashEntry(&search);
		}
		count = TreeItemList_Count(&items);
		for (i = 0; i < count; i++)
		    Tree_RemoveFromSelection(tree, TreeItemList_Nth(&items, i));
		goto doneCLEAR;
	    }
	    if (objc == 5) {
		TreeItemList_Free(&itemsFirst);
		TreeItemList_Free(&itemsLast);
		count = TreeItem_FirstAndLast(tree, &itemFirst, &itemLast);
		if (count == 0)
		    return TCL_ERROR;
		TreeItemList_Init(tree, &items, count);
		while (1) {
		    if (TreeItem_GetSelected(tree, itemFirst)) {
			Tree_RemoveFromSelection(tree, itemFirst);
			TreeItemList_Append(&items, itemFirst);
		    }
		    if (itemFirst == itemLast)
			break;
		    itemFirst = TreeItem_Next(tree, itemFirst);
		}
		goto doneCLEAR;
	    }
	    count = TreeItemList_Count(&itemsFirst);
	    TreeItemList_Init(tree, &items, count);
	    for (i = 0; i < count; i++) {
		item = TreeItemList_Nth(&itemsFirst, i);
		if (TreeItem_GetSelected(tree, item)) {
		    Tree_RemoveFromSelection(tree, item);
		    TreeItemList_Append(&items, item);
		}
	    }
	    TreeItemList_Free(&itemsFirst);
doneCLEAR:
	    if (TreeItemList_Count(&items)) {
		TreeNotify_Selection(tree, NULL, &items);
	    }
	    TreeItemList_Free(&items);
	    break;
	}

	case COMMAND_COUNT: {
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->selectCount));
	    break;
	}

	case COMMAND_GET: {
	    TreeItem item;
	    Tcl_Obj *listObj;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;

#ifdef SELECTION_VISIBLE
	    if (objc < 3 || objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "?first? ?last?");
		return TCL_ERROR;
	    }
	    if (objc > 3) {
		int first, last;
		TreeItemList items;

		if (TclGetIntForIndex(interp, objv[3], tree->selectCount - 1,
			&first) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (first < 0)
		    first = 0;
		last = first;
		if (objc == 5) {
		    if (TclGetIntForIndex(interp, objv[4], tree->selectCount - 1,
			    &last) != TCL_OK) {
			return TCL_ERROR;
		    }
		}
		if (last >= tree->selectCount)
		    last = tree->selectCount - 1;
		if (first > last)
		    break;

		/* Build a list of selected items. */
		TreeItemList_Init(tree, &items, tree->selectCount);
		hPtr = Tcl_FirstHashEntry(&tree->selection, &search);
		while (hPtr != NULL) {
		    item = (TreeItem) Tcl_GetHashKey(&tree->selection, hPtr);
		    TreeItemList_Append(&items, item);
		    hPtr = Tcl_NextHashEntry(&search);
		}

		/* Sort it. */
		TreeItemList_Sort(&items);

		if (first == last) {
		    item = TreeItemList_Nth(&items, first);
		    Tcl_SetObjResult(interp, TreeItem_ToObj(tree, item));
		} else {
		    listObj = Tcl_NewListObj(0, NULL);
		    for (index = first; index <= last; index++) {
			item = TreeItemList_Nth(&items, index);
			Tcl_ListObjAppendElement(interp, listObj,
				TreeItem_ToObj(tree, item));
		    }
		    Tcl_SetObjResult(interp, listObj);
		}

		TreeItemList_Free(&items);
		break;
	    }
#else /* SELECTION_VISIBLE */
	    /* If any item may be selected, including orphans, then getting
	     * a sorted list of selected items is impossible. */
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
		return TCL_ERROR;
	    }
#endif /* SELECTION_VISIBLE */

	    if (tree->selectCount < 1)
		break;
	    listObj = Tcl_NewListObj(0, NULL);
	    hPtr = Tcl_FirstHashEntry(&tree->selection, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashKey(&tree->selection, hPtr);
		Tcl_ListObjAppendElement(interp, listObj,
			TreeItem_ToObj(tree, item));
		hPtr = Tcl_NextHashEntry(&search);
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	case COMMAND_INCLUDES: {
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "item");
		return TCL_ERROR;
	    }
	    if (TreeItem_FromObj(tree, objv[3], &item, IFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(
		TreeItem_GetSelected(tree, item)));
	    break;
	}

	case COMMAND_MODIFY: {
	    int i, j, k, objcS, objcD;
	    Tcl_Obj **objvS, **objvD;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;
	    TreeItem item;
	    TreeItemList items;
	    TreeItemList itemS, itemD, newS, newD;
	    int allS = FALSE, allD = FALSE;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "select deselect");
		return TCL_ERROR;
	    }
	    if (Tcl_ListObjGetElements(interp, objv[3], &objcS, &objvS) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_ListObjGetElements(interp, objv[4], &objcD, &objvD) != TCL_OK)
		return TCL_ERROR;

	    /* No change */
	    if (!objcS && !objcD)
		break;

	    /* Some of these may get double-initialized. */
	    TreeItemList_Init(tree, &itemS, 0);
	    TreeItemList_Init(tree, &itemD, 0);
	    TreeItemList_Init(tree, &newS, 0);
	    TreeItemList_Init(tree, &newD, 0);

	    /* List of items to select */
	    for (i = 0; i < objcS; i++) {
		if (TreeItemList_FromObj(tree, objvS[i], &items, IFO_NOT_NULL) != TCL_OK) {
		    TreeItemList_Free(&itemS);
		    return TCL_ERROR;
		}

		/* Add unique items to itemS */
		for (k = 0; k < TreeItemList_Count(&items); k++) {
		    item = TreeItemList_Nth(&items, k);
		    if (item == ITEM_ALL) {
			allS = TRUE;
			break;
		    }
		    for (j = 0; j < TreeItemList_Count(&itemS); j++) {
			if (TreeItemList_Nth(&itemS, j) == item)
			    break;
		    }
		    if (j == TreeItemList_Count(&itemS)) {
			TreeItemList_Append(&itemS, item);
		    }
		}
		TreeItemList_Free(&items);
		if (allS) break;
	    }

	    /* List of items to deselect */
	    for (i = 0; i < objcD; i++) {
		if (TreeItemList_FromObj(tree, objvD[i], &items, IFO_NOT_NULL) != TCL_OK) {
		    TreeItemList_Free(&itemS);
		    TreeItemList_Free(&itemD);
		    return TCL_ERROR;
		}

		/* Add unique items to itemD */
		for (k = 0; k < TreeItemList_Count(&items); k++) {
		    item = TreeItemList_Nth(&items, k);
		    if (item == ITEM_ALL) {
			allD = TRUE;
			break;
		    }
		    for (j = 0; j < TreeItemList_Count(&itemD); j++) {
			if (TreeItemList_Nth(&itemD, j) == item)
			    break;
		    }
		    if (j == TreeItemList_Count(&itemD)) {
			TreeItemList_Append(&itemD, item);
		    }
		}
		TreeItemList_Free(&items);
		if (allD) break;
	    }

	    /* Select all */
	    if (allS) {
		TreeItemList_Init(tree, &newS, tree->itemCount - tree->selectCount);
#ifdef SELECTION_VISIBLE
		item = tree->root;
		if (!TreeItem_ReallyVisible(tree, item))
		    item = TreeItem_NextVisible(tree, item);
		while (item != NULL) {
		    if (TreeItem_CanAddToSelection(tree, item)) {
			TreeItemList_Append(&newS, item);
		    }
		    item = TreeItem_NextVisible(tree, item);
		}
#else
		/* Include detached items */
		hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		while (hPtr != NULL) {
		    item = (TreeItem) Tcl_GetHashValue(hPtr);
		    if (TreeItem_CanAddToSelection(tree, item)) {
			TreeItemList_Append(&newS, item);
		    }
		    hPtr = Tcl_NextHashEntry(&search);
		}
#endif
		/* Ignore the deselect list. */
		goto modifyDONE;
	    }

	    /* Select some */
	    if (objcS > 0) {
		TreeItemList_Init(tree, &newS, objcS);
		for (i = 0; i < TreeItemList_Count(&itemS); i++) {
		    item = TreeItemList_Nth(&itemS, i);
		    if (TreeItem_CanAddToSelection(tree, item)) {
			TreeItemList_Append(&newS, item);
		    }
		}
	    }

	    /* Deselect all */
	    if (allD) {
		TreeItemList_Init(tree, &newD, tree->selectCount);
		hPtr = Tcl_FirstHashEntry(&tree->selection, &search);
		while (hPtr != NULL) {
		    item = (TreeItem) Tcl_GetHashKey(&tree->selection, hPtr);
		    /* Don't deselect an item in the select list */
		    for (j = 0; j < TreeItemList_Count(&itemS); j++) {
			if (item == TreeItemList_Nth(&itemS, j))
			    break;
		    }
		    if (j == TreeItemList_Count(&itemS)) {
			TreeItemList_Append(&newD, item);
		    }
		    hPtr = Tcl_NextHashEntry(&search);
		}
	    }

	    /* Deselect some */
	    if ((objcD > 0) && !allD) {
		TreeItemList_Init(tree, &newD, objcD);
		for (i = 0; i < TreeItemList_Count(&itemD); i++) {
		    item = TreeItemList_Nth(&itemD, i);
		    if (!TreeItem_GetSelected(tree, item))
			continue;
		    /* Don't deselect an item in the select list */
		    for (j = 0; j < TreeItemList_Count(&itemS); j++) {
			if (item == TreeItemList_Nth(&itemS, j))
			    break;
		    }
		    if (j == TreeItemList_Count(&itemS)) {
			TreeItemList_Append(&newD, item);
		    }
		}
	    }
modifyDONE:
	    for (i = 0; i < TreeItemList_Count(&newS); i++)
		Tree_AddToSelection(tree, TreeItemList_Nth(&newS, i));
	    for (i = 0; i < TreeItemList_Count(&newD); i++)
		Tree_RemoveFromSelection(tree, TreeItemList_Nth(&newD, i));
	    if (TreeItemList_Count(&newS) || TreeItemList_Count(&newD)) {
		TreeNotify_Selection(tree, &newS, &newD);
	    }
	    TreeItemList_Free(&newS);
	    TreeItemList_Free(&itemS);
	    TreeItemList_Free(&newD);
	    TreeItemList_Free(&itemD);
	    break;
	}
    }

    return TCL_OK;
}

void
Tree_Debug(
    TreeCtrl *tree		/* Widget info. */
    )
{
    if (TreeItem_Debug(tree, tree->root) != TCL_OK) {
	dbwin("Tree_Debug: %s\n", Tcl_GetStringResult(tree->interp));
	Tcl_BackgroundError(tree->interp);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TreeDebugCmd --
 *
 *	This procedure is invoked to process the [debug] widget
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
TreeDebugCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    static CONST char *commandNames[] = {
	"alloc", "cget", "configure", "dinfo", "expose", (char *) NULL
    };
    enum { COMMAND_ALLOC, COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_DINFO,
	COMMAND_EXPOSE };
    int index;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandNames, "command", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
	/* T debug alloc */
	case COMMAND_ALLOC: {
#ifdef ALLOC_HAX
#ifdef TREECTRL_DEBUG
	    TreeAlloc_Stats(interp, tree->allocData);
#else
	    FormatResult(interp, "TREECTRL_DEBUG is not defined");
#endif
#else
	    FormatResult(interp, "ALLOC_HAX is not defined");
#endif
	    break;
	}

	/* T debug cget option */
	case COMMAND_CGET: {
	    Tcl_Obj *resultObjPtr;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "option");
		return TCL_ERROR;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) tree,
		    tree->debug.optionTable, objv[3], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T debug configure ?option? ?value? ?option value ...? */
	case COMMAND_CONFIGURE: {
	    Tcl_Obj *resultObjPtr;
	    Tk_SavedOptions savedOptions;
	    int mask, result;

	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 3, objv, "?option? ?value?");
		return TCL_ERROR;
	    }
	    if (objc <= 4) {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) tree,
			tree->debug.optionTable,
			(objc == 3) ? (Tcl_Obj *) NULL : objv[3],
			tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    result = Tk_SetOptions(interp, (char *) tree,
		    tree->debug.optionTable, objc - 3, objv + 3, tree->tkwin,
		    &savedOptions, &mask);
	    if (result != TCL_OK) {
		Tk_RestoreSavedOptions(&savedOptions);
		return TCL_ERROR;
	    }
	    Tk_FreeSavedOptions(&savedOptions);
	    if (tree->debug.eraseColor != NULL) {
		tree->debug.gcErase = Tk_GCForColor(tree->debug.eraseColor,
			Tk_WindowId(tree->tkwin));
	    }
	    if (tree->debug.drawColor != NULL) {
		tree->debug.gcDraw = Tk_GCForColor(tree->debug.drawColor,
			Tk_WindowId(tree->tkwin));
	    }
	    break;
	}

	case COMMAND_DINFO: {
	    return Tree_DumpDInfo(tree, objc, objv);
	}

	/* T debug expose x1 y1 x2 y2 */
	case COMMAND_EXPOSE: {
	    int x1, y1, x2, y2;

	    if (objc != 7) {
		Tcl_WrongNumArgs(interp, 3, objv, "x1 y1 x2 y2");
		return TCL_ERROR;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[3], &x1) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[4], &y1) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[5], &x2) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[6], &y2) != TCL_OK)
		return TCL_ERROR;
	    Tree_RedrawArea(tree, MIN(x1, x2), MIN(y1, y2),
		    MAX(x1, x2), MAX(y1, y2));
	    break;
	}
    }

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_PreserveItems --
 *
 *	Increment tree->preserveItemRefCnt.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void 
Tree_PreserveItems(
    TreeCtrl *tree
    )
{
    tree->preserveItemRefCnt++;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ReleaseItems --
 *
 *	Decrement tree->preserveItemRefCnt. If it reaches zero,
 *	release the storage of items marked as deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_ReleaseItems(
    TreeCtrl *tree
    )
{
    int i, count;
    TreeItem item;

    if (tree->preserveItemRefCnt == 0)
	panic("mismatched calls to Tree_PreserveItems/Tree_ReleaseItems");

    if (--tree->preserveItemRefCnt > 0)
	return;

    count = TreeItemList_Count(&tree->preserveItemList);
    for (i = 0; i < count; i++) {
	item = TreeItemList_Nth(&tree->preserveItemList, i);
	TreeItem_Release(tree, item);
    }

    TreeItemList_Free(&tree->preserveItemList);
}

/*
 *--------------------------------------------------------------
 *
 * TextLayoutCmd --
 *
 *	This procedure is invoked to process the [textlayout] Tcl
 *	command. The command is used by the library scripts to place
 *	the text-edit Entry or Text widget.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

/*
textlayout $font $text
	-width pixels
	-wrap word|char
	-justify left|center|right
	-ignoretabs boolean
	-ignorenewlines boolean
*/
static int
TextLayoutCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    Tk_Font tkfont;
    Tk_Window tkwin = Tk_MainWindow(interp);
    char *text;
    int flags = 0;
    Tk_Justify justify = TK_JUSTIFY_LEFT;
    Tk_TextLayout layout;
    int width = 0, height;
    int result = TCL_OK;
    int i;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "font text ?options ...?");
	return TCL_ERROR;
    }

    tkfont = Tk_AllocFontFromObj(interp, tkwin, objv[1]);
    if (tkfont == NULL)
	return TCL_ERROR;
    text = Tcl_GetString(objv[2]);

    for (i = 3; i < objc; i += 2) {
	static CONST char *optionNames[] = {
	    "-ignoretabs", "-ignorenewlines",
	    "-justify", "-width", (char *) NULL
	};
	enum { OPT_IGNORETABS, OPT_IGNORENEWLINES, OPT_JUSTIFY, OPT_WIDTH };
	int index;

	if (Tcl_GetIndexFromObj(interp, objv[i], optionNames, "option", 0,
		&index) != TCL_OK) {
	    result = TCL_ERROR;
	    goto done;
	}

	if (i + 1 == objc) {
	    FormatResult(interp, "missing value for \"%s\" option",
		    optionNames[index]);
	    goto done;
	}

	switch (index) {
	    case OPT_IGNORENEWLINES: {
		int v;
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &v) != TCL_OK) {
		    result = TCL_ERROR;
		    goto done;
		}
		if (v)
		    flags |= TK_IGNORE_NEWLINES;
		else
		    flags &= ~TK_IGNORE_NEWLINES;
		break;
	    }
	    case OPT_IGNORETABS: {
		int v;
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &v) != TCL_OK) {
		    result = TCL_ERROR;
		    goto done;
		}
		if (v)
		    flags |= TK_IGNORE_TABS;
		else
		    flags &= ~TK_IGNORE_TABS;
		break;
	    }
	    case OPT_JUSTIFY: {
		if (Tk_GetJustifyFromObj(interp, objv[i + 1], &justify) != TCL_OK) {
		    result = TCL_ERROR;
		    goto done;
		}
		break;
	    }
	    case OPT_WIDTH: {
		if (Tk_GetPixelsFromObj(interp, tkwin, objv[i + 1], &width) != TCL_OK) {
		    result = TCL_ERROR;
		    goto done;
		}
		break;
	    }
	}
    }

    layout = Tk_ComputeTextLayout(tkfont, text, -1, width, justify, flags,
	    &width, &height);
    FormatResult(interp, "%d %d", width, height);
    Tk_FreeTextLayout(layout);

done:
    Tk_FreeFont(tkfont);
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * ImageTintCmd --
 *
 *	This procedure is invoked to process the [imagetint] Tcl
 *	command. The command may be used to apply a highlight to an
 *	existing photo image. It is used by the demos to produce a
 *	selected version of an image.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A photo image is modified.
 *
 *--------------------------------------------------------------
 */

static int
ImageTintCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    char *imageName;
    Tk_PhotoHandle photoH;
    Tk_PhotoImageBlock photoBlock;
    XColor *xColor;
    unsigned char *pixelPtr, *photoPix;
    int x, y, alpha, imgW, imgH, pitch;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "imageName color alpha");
	return TCL_ERROR;
    }

    imageName = Tcl_GetStringFromObj(objv[1], NULL);
    photoH = Tk_FindPhoto(interp, imageName);
    if (photoH == NULL) {
	Tcl_AppendResult(interp, "image \"", imageName,
		"\" doesn't exist or is not a photo image",
		(char *) NULL);
	return TCL_ERROR;
    }

    xColor = Tk_AllocColorFromObj(interp, Tk_MainWindow(interp), objv[2]);
    if (xColor == NULL)
	return TCL_ERROR;

    if (Tcl_GetIntFromObj(interp, objv[3], &alpha) != TCL_OK)
	return TCL_ERROR;
    if (alpha < 0)
	alpha = 0;
    if (alpha > 255)
	alpha = 255;

    Tk_PhotoGetImage(photoH, &photoBlock);
    photoPix = photoBlock.pixelPtr;
    imgW = photoBlock.width;
    imgH = photoBlock.height;
    pitch = photoBlock.pitch;

    pixelPtr = (unsigned char *) Tcl_Alloc(imgW * 4);
    photoBlock.pixelPtr = pixelPtr;
    photoBlock.width = imgW;
    photoBlock.height = 1;
    photoBlock.pitch = imgW * 4;
    photoBlock.pixelSize = 4;
    photoBlock.offset[0] = 0;
    photoBlock.offset[1] = 1;
    photoBlock.offset[2] = 2;
    photoBlock.offset[3] = 3;

    for (x = 0; x < imgW; x++) {
	pixelPtr[x*4 + 0] = UCHAR(((double) xColor->red / USHRT_MAX) * 255);
	pixelPtr[x*4 + 1] = UCHAR(((double) xColor->green / USHRT_MAX) * 255);
	pixelPtr[x*4 + 2] = UCHAR(((double) xColor->blue / USHRT_MAX) * 255);
    }
    for (y = 0; y < imgH; y++) {
	for (x = 0; x < imgW; x++) {
	    if (photoPix[x * 4 + 3]) {
		pixelPtr[x * 4 + 3] = alpha;
	    } else {
		pixelPtr[x * 4 + 3] = 0;
	    }
	}
	TK_PHOTOPUTBLOCK(interp, photoH, &photoBlock, 0, y,
		imgW, 1, TK_PHOTO_COMPOSITE_OVERLAY);
	photoPix += pitch;
    }
    Tcl_Free((char *) photoBlock.pixelPtr);

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * LoupeCmd --
 *
 *	This procedure is invoked to process the [loupe] Tcl
 *	command. The command is used to perform a screen grab on the
 *	root window and place a magnified version of the screen grab
 *	into an existing photo image. The command is used to check those
 *	dotted lines and make sure they line up properly.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A photo image is modified.
 *
 *--------------------------------------------------------------
 */

static int
LoupeCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    Tk_Window tkwin = Tk_MainWindow(interp);
    Display *display = Tk_Display(tkwin);
    int screenNum = Tk_ScreenNumber(tkwin);
    int displayW = DisplayWidth(display, screenNum);
    int displayH = DisplayHeight(display, screenNum);
    char *imageName;
    Tk_PhotoHandle photoH;
    Tk_PhotoImageBlock photoBlock;
    unsigned char *pixelPtr;
    int x, y, w, h, zoom;
    int grabX, grabY, grabW, grabH;
    int minx = 0, miny = 0;
#ifdef WIN32
    int xx, yy;
    HWND hwnd;
    HDC hdc;
#define WIN7
#ifdef WIN7
    HDC hdcCopy;
    HBITMAP hBitmap, hBitmapSave;
#endif /* WIN7 */
#elif defined(MAC_OSX_TK)
#else
    Visual *visual = Tk_Visual(tkwin);
    Window rootWindow = RootWindow(display, screenNum);
    XImage *ximage;
    XColor *xcolors;
    unsigned long red_shift, green_shift, blue_shift;
    int i, ncolors;
    int separated = 0;
#endif

    /*
     * x && y are points on screen to snap from
     * w && h are size of image to grab (default to image size)
     * zoom is the integer zoom factor to grab
     */
    if ((objc != 4) && (objc != 6) && (objc != 7)) {
	Tcl_WrongNumArgs(interp, 1, objv, "imageName x y ?w h? ?zoom?");
	return TCL_ERROR;
    }

    imageName = Tcl_GetStringFromObj(objv[1], NULL);
    photoH = Tk_FindPhoto(interp, imageName);
    if (photoH == NULL) {
	Tcl_AppendResult(interp, "image \"", imageName,
		"\" doesn't exist or is not a photo image",
		(char *) NULL);
	return TCL_ERROR;
    }

    if ((Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK)
	    || (Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (objc >= 6) {
	if ((Tcl_GetIntFromObj(interp, objv[4], &w) != TCL_OK)
		|| (Tcl_GetIntFromObj(interp, objv[5], &h) != TCL_OK)) {
	    return TCL_ERROR;
	}
    } else {
	/*
	 * Get dimensions from image
	 */
	Tk_PhotoGetSize(photoH, &w, &h);
    }
    if (objc == 7) {
	if (Tcl_GetIntFromObj(interp, objv[6], &zoom) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	zoom = 1;
    }

#ifdef WIN32
    /*
     * Windows multiple monitors can have negative coords
     */
    minx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    miny = GetSystemMetrics(SM_YVIRTUALSCREEN);
    displayW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    displayH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
#elif defined(MAC_OSX_TK)
    /*
     * OS X multiple monitors can have negative coords
     * FIX: must be implemented
     * Probably with CGDisplayPixelsWide & CGDisplayPixelsHigh,
     * may need to iterate existing displays
     */
#else
    /*
     * Does X11 allow for negative screen coords?
     */
#endif
    grabX = x - (w / zoom / 2);
    grabY = y - (h / zoom / 2);
    grabW = w / zoom;
    grabH = h / zoom;
    if (grabW * zoom < w)		++grabW;
    if (grabH * zoom < h)		++grabH;
    if (grabW > displayW)		grabW = displayW;
    if (grabH > displayH)		grabH = displayH;
    if (grabX < minx)			grabX = minx;
    if (grabY < miny)			grabY = miny;
    if (grabX + grabW > displayW)	grabX = displayW - grabW;
    if (grabY + grabH > displayH)	grabY = displayH - grabH;

    if ((grabW <= 0) || (grabH <= 0)) {
	return TCL_OK;
    }

#ifdef WIN32
    hwnd = GetDesktopWindow();
    hdc = GetWindowDC(hwnd);

#ifdef WIN7
    /* Doing GetPixel() on the desktop DC under Windows 7 (Aero) is buggy
     * and *very* slow.  So BitBlt() from the desktop DC to an in-memory
     * bitmap and run GetPixel() on that. */
    hdcCopy = CreateCompatibleDC(hdc);
    hBitmap = CreateCompatibleBitmap(hdc, grabW, grabH);
    hBitmapSave = SelectObject(hdcCopy, hBitmap);
    BitBlt(hdcCopy, 0, 0, grabW, grabH, hdc, grabX, grabY,
	SRCCOPY | CAPTUREBLT);
#endif /* WIN7 */

    /* XImage -> Tk_Image */
    pixelPtr = (unsigned char *) Tcl_Alloc(grabW * grabH * 4);
    memset(pixelPtr, 0, (grabW * grabH * 4));
    photoBlock.pixelPtr  = pixelPtr;
    photoBlock.width     = grabW;
    photoBlock.height    = grabH;
    photoBlock.pitch     = grabW * 4;
    photoBlock.pixelSize = 4;
    photoBlock.offset[0] = 0;
    photoBlock.offset[1] = 1;
    photoBlock.offset[2] = 2;
    photoBlock.offset[3] = 3;

    /*
     * We could do a BitBlt for bulk copying, but then we'd have to
     * do screen size consistency checks and possibly pixel conversion.
     */
    for (yy = 0; yy < grabH; yy++) {
	COLORREF pixel;
	unsigned long stepDest = yy * photoBlock.pitch;
	for (xx = 0; xx < grabW; xx++) {
#ifdef WIN7
	    pixel = GetPixel(hdcCopy, xx, yy);
#else /* WIN7 */
	    pixel = GetPixel(hdc, grabX + xx, grabY + yy);
#endif /* WIN7 */
	    if (pixel == CLR_INVALID) {
		/*
		 * Skip just this pixel, as others will be valid depending on
		 * what corner we are in.
		 */
		continue;
	    }
	    pixelPtr[stepDest + xx * 4 + 0] = GetRValue(pixel);
	    pixelPtr[stepDest + xx * 4 + 1] = GetGValue(pixel);
	    pixelPtr[stepDest + xx * 4 + 2] = GetBValue(pixel);
	    pixelPtr[stepDest + xx * 4 + 3] = 255;
	}
    }
#ifdef WIN7
    SelectObject(hdcCopy, hBitmapSave);
    DeleteObject(hBitmap);
    DeleteDC(hdcCopy);
#endif /* WIN7 */
    ReleaseDC(hwnd, hdc);
#elif defined(MAC_OSX_TK)
    /*
     * Adapted from John Anon's ScreenController demo code.
     */
    {
    int xx, yy;
    unsigned char *screenBytes;
    int bPerPixel, byPerRow, byPerPixel;

    /* Gets all the screen info: */
    CGDisplayHideCursor(kCGDirectMainDisplay);
    bPerPixel  = CGDisplayBitsPerPixel(kCGDirectMainDisplay);
    byPerRow   = CGDisplayBytesPerRow(kCGDirectMainDisplay);
    byPerPixel = bPerPixel / 8;

    screenBytes = (unsigned char *)CGDisplayBaseAddress(kCGDirectMainDisplay);

    pixelPtr = (unsigned char *) Tcl_Alloc(grabW * grabH * 4);
    memset(pixelPtr, 0, (grabW * grabH * 4));

    photoBlock.pixelPtr  = pixelPtr;
    photoBlock.width     = grabW;
    photoBlock.height    = grabH;
    photoBlock.pitch     = grabW * 4;
    photoBlock.pixelSize = 4;
    photoBlock.offset[0] = 0;
    photoBlock.offset[1] = 1;
    photoBlock.offset[2] = 2;
    photoBlock.offset[3] = 3;

    for (yy = 0; yy < grabH; yy++) {
	unsigned long newPixel = 0;
	unsigned long stepSrc = (grabY + yy) * byPerRow;
	unsigned long stepDest = yy * photoBlock.pitch;

	for (xx = 0; xx < grabW; xx++) {
	    if (bPerPixel == 16) {
		unsigned short thisPixel;

		thisPixel = *((unsigned short*)(screenBytes + stepSrc
				      + ((grabX + xx) * byPerPixel)));
#ifdef WORDS_BIGENDIAN
		/* Transform from 0xARGB (1555) to 0xR0G0B0A0 (4444) */
		newPixel = (((thisPixel & 0x8000) >> 15) * 0xF8) | /* A */
		    ((thisPixel & 0x7C00) << 17) | /* R */
		    ((thisPixel & 0x03E0) << 14) | /* G */
		    ((thisPixel & 0x001F) << 11);  /* B */
#else
		/* Transform from 0xARGB (1555) to 0xB0G0R0A0 (4444) */
		newPixel = (((thisPixel & 0x8000) >> 15) * 0xF8) | /* A */
		    ((thisPixel & 0x7C00) << 11) | /* R */
		    ((thisPixel & 0x03E0) << 14) | /* G */
		    ((thisPixel & 0x001F) << 17);  /* B */
#endif
	    } else if (bPerPixel == 32) {
		unsigned long thisPixel;

		thisPixel = *((unsigned long*)(screenBytes + stepSrc
				      + ((grabX + xx) * byPerPixel)));

#ifdef WORDS_BIGENDIAN
		/* Transformation is from 0xAARRGGBB to 0xRRGGBBAA */
		newPixel = ((thisPixel & 0xFF000000) >> 24) |
		    ((thisPixel & 0x00FFFFFF) << 8);
#else
		/* Transformation is from 0xAARRGGBB to 0xBBGGRRAA */
		newPixel = (thisPixel & 0xFF00FF00) |
		    ((thisPixel & 0x00FF0000) >> 16) |
		    ((thisPixel & 0x000000FF) << 16);
#endif
	    }
	    *((unsigned int *)(pixelPtr + stepDest + xx * 4)) = newPixel;
	}
    }
    CGDisplayShowCursor(kCGDirectMainDisplay);
    }
#else
    ximage = XGetImage(display, rootWindow,
	    grabX, grabY, grabW, grabH, AllPlanes, ZPixmap);
    if (ximage == NULL) {
	FormatResult(interp, "XGetImage() failed");
	return TCL_ERROR;
    }

    /* See TkPostscriptImage */

    ncolors = visual->map_entries;
    xcolors = (XColor *) ckalloc(sizeof(XColor) * ncolors);

    if ((visual->class == DirectColor) || (visual->class == TrueColor)) {
	separated = 1;
	red_shift = green_shift = blue_shift = 0;
	while ((0x0001 & (ximage->red_mask >> red_shift)) == 0)
	    red_shift++;
	while ((0x0001 & (ximage->green_mask >> green_shift)) == 0)
	    green_shift++;
	while ((0x0001 & (ximage->blue_mask >> blue_shift)) == 0)
	    blue_shift++;
	for (i = 0; i < ncolors; i++) {
	    xcolors[i].pixel =
		((i << red_shift) & ximage->red_mask) |
		((i << green_shift) & ximage->green_mask) |
		((i << blue_shift) & ximage->blue_mask);
	}
    } else {
	for (i = 0; i < ncolors; i++)
	    xcolors[i].pixel = i;
    	red_shift = green_shift = blue_shift = 0; /* compiler warning */
    }

    XQueryColors(display, Tk_Colormap(tkwin), xcolors, ncolors);

    /* XImage -> Tk_Image */
    pixelPtr = (unsigned char *) Tcl_Alloc(ximage->width * ximage->height * 4);
    photoBlock.pixelPtr  = pixelPtr;
    photoBlock.width     = ximage->width;
    photoBlock.height    = ximage->height;
    photoBlock.pitch     = ximage->width * 4;
    photoBlock.pixelSize = 4;
    photoBlock.offset[0] = 0;
    photoBlock.offset[1] = 1;
    photoBlock.offset[2] = 2;
    photoBlock.offset[3] = 3;

    for (y = 0; y < ximage->height; y++) {
	for (x = 0; x < ximage->width; x++) {
	    int r, g, b;
	    unsigned long pixel;

	    pixel = XGetPixel(ximage, x, y);
	    if (separated) {
		r = (pixel & ximage->red_mask) >> red_shift;
		g = (pixel & ximage->green_mask) >> green_shift;
		b = (pixel & ximage->blue_mask) >> blue_shift;
		r = ((double) xcolors[r].red / USHRT_MAX) * 255;
		g = ((double) xcolors[g].green / USHRT_MAX) * 255;
		b = ((double) xcolors[b].blue / USHRT_MAX) * 255;
	    } else {
		r = ((double) xcolors[pixel].red / USHRT_MAX) * 255;
		g = ((double) xcolors[pixel].green / USHRT_MAX) * 255;
		b = ((double) xcolors[pixel].blue / USHRT_MAX) * 255;
	    }
	    pixelPtr[y * photoBlock.pitch + x * 4 + 0] = r;
	    pixelPtr[y * photoBlock.pitch + x * 4 + 1] = g;
	    pixelPtr[y * photoBlock.pitch + x * 4 + 2] = b;
	    pixelPtr[y * photoBlock.pitch + x * 4 + 3] = 255;
	}
    }
#endif

    TK_PHOTOPUTZOOMEDBLOCK(interp, photoH, &photoBlock, 0, 0, w, h,
	    zoom, zoom, 1, 1, TK_PHOTO_COMPOSITE_SET);

    Tcl_Free((char *) pixelPtr);
#if !defined(WIN32) && !defined(MAC_OSX_TK)
    ckfree((char *) xcolors);
    XDestroyImage(ximage);
#endif

    return TCL_OK;
}

#ifndef USE_TTK

/*
 *--------------------------------------------------------------
 *
 * RecomputeWidgets --
 *
 *	This procedure is called when the system theme changes on platforms
 *	that support theming. The worldChangedProc of all treectrl widgets
 *	is called to relayout and redisplay the widgets. 
 *
 *	Taken from tkFont.c.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All treectrl widgets will be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

static void
RecomputeWidgets(
    TkWindow *winPtr		/* Window info. */
    )
{
    Tk_ClassWorldChangedProc *proc;

    /* Clomp! Stomp! All over the internals */
    proc = Tk_GetClassProc(winPtr->classProcsPtr, worldChangedProc);
    if (proc == TreeWorldChanged) {
	TreeTheme_ThemeChanged((TreeCtrl *) winPtr->instanceData);
	TreeWorldChanged(winPtr->instanceData);
    }

    for (winPtr = winPtr->childList; winPtr != NULL; winPtr = winPtr->nextPtr) {
	RecomputeWidgets(winPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tree_TheWorldHasChanged --
 *
 *	This procedure is called when the system theme changes on platforms
 *	that support theming. The worldChangedProc of all treectrl widgets
 *	is called to relayout and redisplay the widgets. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All treectrl widgets will be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_TheWorldHasChanged(
    Tcl_Interp *interp		/* Current interpreter. */
    )
{
    /* Could send a <<ThemeChanged>> event to every window like Tile does. */
    /* Could keep a list of treectrl widgets. */
    TkWindow *winPtr = (TkWindow *) Tk_MainWindow(interp);
    RecomputeWidgets(winPtr);
}

#endif /* !USE_TTK */

/*
 * In order to find treectrl.tcl during initialization, the following script
 * is invoked.
 */
static char initScript[] = "if {![llength [info proc ::TreeCtrl::Init]]} {\n\
  namespace eval ::TreeCtrl {}\n\
  proc ::TreeCtrl::Init {} {\n\
    global treectrl_library\n\
    tcl_findLibrary treectrl " PACKAGE_PATCHLEVEL " " PACKAGE_PATCHLEVEL " treectrl.tcl TREECTRL_LIBRARY treectrl_library\n\
  }\n\
}\n\
::TreeCtrl::Init";

/*
 *--------------------------------------------------------------
 *
 * Treectrl_Init --
 *
 *	This procedure initializes the TreeCtrl package and related
 *	commands.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Memory is allocated. New Tcl commands are created.
 *
 *--------------------------------------------------------------
 */

DLLEXPORT int
Treectrl_Init(
    Tcl_Interp *interp		/* Interpreter the package is loading into. */
    )
{
#ifdef USE_TTK
    static CONST char *tcl_version = "8.5";
#else
    static CONST char *tcl_version = "8.4";
#endif
#ifdef TREECTRL_DEBUG
    Tk_OptionSpec *specPtr, *prevSpecPtr = NULL;
#endif

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, tcl_version, 0) == NULL) {
	return TCL_ERROR;
    }
#endif
#ifdef USE_TK_STUBS
#if (TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION < 5)
    if (Tk_InitStubs(interp, (char*)tcl_version, 0) == NULL) {
#else
    if (Tk_InitStubs(interp, tcl_version, 0) == NULL) {
#endif
	return TCL_ERROR;
    }
#endif

    dbwin_add_interp(interp);

#ifdef TREECTRL_DEBUG
    for (specPtr = optionSpecs; specPtr->type != TK_OPTION_END; specPtr++) {
	if (prevSpecPtr != NULL &&
		strcmp(prevSpecPtr->optionName, specPtr->optionName) >= 0) {
	    panic("Treectrl_Init option table ordering %s >= %s",
		    prevSpecPtr->optionName, specPtr->optionName);
	}
	prevSpecPtr = specPtr;
    }
#endif

    PerStateCO_Init(optionSpecs, "-buttonbitmap", &pstBitmap, TreeStateFromObj);
    PerStateCO_Init(optionSpecs, "-buttonimage", &pstImage, TreeStateFromObj);

    if (TreeElement_Init(interp) != TCL_OK) {
	return TCL_ERROR;
    }

    (void) TreeDraw_InitInterp(interp);

    /* We don't care if this fails. */
    (void) TreeTheme_InitInterp(interp);

    if (TreeColumn_InitInterp(interp) != TCL_OK)
	return TCL_ERROR;

    /* Set the default value of some options. */
    /* Do this *after* the system theme is initialized. */
    TreeTheme_SetOptionDefault(
	Tree_FindOptionSpec(optionSpecs, "-buttontracking"));
    TreeTheme_SetOptionDefault(
	Tree_FindOptionSpec(optionSpecs, "-showlines"));

    /* Hack for editing a text Element. */
    Tcl_CreateObjCommand(interp, "textlayout", TextLayoutCmd, NULL, NULL);

    /* Hack for colorizing an image (like Win98 explorer). */
    Tcl_CreateObjCommand(interp, "imagetint", ImageTintCmd, NULL, NULL);

    /* Screen magnifier to check those dotted lines. */
    Tcl_CreateObjCommand(interp, "loupe", LoupeCmd, NULL, NULL);

    Tcl_CreateObjCommand(interp, "treectrl", TreeObjCmd, NULL, NULL);

    if (Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_PATCHLEVEL) != TCL_OK) {
	return TCL_ERROR;
    }
    return Tcl_EvalEx(interp, initScript, -1, TCL_EVAL_GLOBAL);
}

/*
 *--------------------------------------------------------------
 *
 * Treectrl_SafeInit --
 *
 *	This procedure initializes the TreeCtrl package and related
 *	commands.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Memory is allocated. New Tcl commands are created.
 *
 *--------------------------------------------------------------
 */

DLLEXPORT int
Treectrl_SafeInit(
    Tcl_Interp *interp		/* Interpreter the package is loading into. */
    )
{
    return Treectrl_Init(interp);
}

