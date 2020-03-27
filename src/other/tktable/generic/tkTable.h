/* 
 * tkTable.h --
 *
 *	This is the header file for the module that implements
 *	table widgets for the Tk toolkit.
 *
 * Copyright (c) 1997-2002 Jeffrey Hobbs
 *
 * See the file "license.txt" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifndef _TKTABLE_H_
#define _TKTABLE_H_

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <tk.h>
#ifdef MAC_TCL
# include <Xatom.h>
#else
# include <X11/Xatom.h>
#endif /* MAC_TCL */

#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION == 0) /* Tcl8.0 stuff */
#define Tcl_GetString(objPtr)	Tcl_GetStringFromObj(objPtr, (int *)NULL)
#endif

#if (TCL_MAJOR_VERSION > 8) || ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4))
#   define HAVE_TCL84
#endif

/*
 * Tcl/Tk 8.4 introduced better CONST-ness in the APIs, but we use CONST84 in
 * some cases for compatibility with earlier Tcl headers to prevent warnings.
 */
#ifndef CONST84
#  define CONST84
#endif

/* This EXTERN declaration is needed for Tcl < 8.0.3 */
#ifndef EXTERN
# ifdef __cplusplus
#  define EXTERN extern "C"
# else
#  define EXTERN extern
# endif
#endif

#ifdef TCL_STORAGE_CLASS
# undef TCL_STORAGE_CLASS
#endif
#ifdef BUILD_Tktable
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# define TCL_STORAGE_CLASS DLLIMPORT
#endif

#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   undef WIN32_LEAN_AND_MEAN
/* VC++ has an entry point called DllMain instead of DllEntryPoint */
#   if defined(_MSC_VER)
#	define DllEntryPoint DllMain
#   endif
#endif

#if defined(WIN32) || defined(MAC_TCL) || defined(MAC_OSX_TK)
/* XSync call defined in the internals for some reason */
#   ifndef XSync
#	define XSync(display, bool) {display->request++;}
#   endif
#endif /* defn of XSync */

#ifndef NORMAL_BG
#   ifdef WIN32
#	define NORMAL_BG	"SystemButtonFace"
#	define ACTIVE_BG	NORMAL_BG
#	define SELECT_BG	"SystemHighlight"
#	define SELECT_FG	"SystemHighlightText"
#	define DISABLED		"SystemDisabledText"
#	define HIGHLIGHT	"SystemWindowFrame"
#	define DEF_TABLE_FONT	"{MS Sans Serif} 8"
#   elif defined(MAC_TCL) || defined(MAC_OSX_TK)
#	define NORMAL_BG	"systemWindowBody"
#	define ACTIVE_BG	"#ececec"
#	define SELECT_BG	"systemHighlight"
#	define SELECT_FG	"systemHighlightText"
#	define DISABLED		"#a3a3a3"
#	define HIGHLIGHT	"Black"
#	define DEF_TABLE_FONT	"Helvetica 12"
#   else
#	define NORMAL_BG	"#d9d9d9"
#	define ACTIVE_BG	"#fcfcfc"
#	define SELECT_BG	"#c3c3c3"
#	define SELECT_FG	"Black"
#	define DISABLED		"#a3a3a3"
#	define HIGHLIGHT	"Black"
#	define DEF_TABLE_FONT	"Helvetica -12"
#   endif
#endif /* NORMAL_BG */

#define MAX(A,B)	(((A)>(B))?(A):(B))
#define MIN(A,B)	(((A)>(B))?(B):(A))
#define BETWEEN(val,min,max)	( ((val)<(min)) ? (min) : \
				( ((val)>(max)) ? (max) : (val) ) )
#define CONSTRAIN(val,min,max)	if ((val) < (min)) { (val) = (min); } \
				else if ((val) > (max)) { (val) = (max); }
#define STREQ(s1, s2)	(strcmp((s1), (s2)) == 0)
#define ARSIZE(A)	(sizeof(A)/sizeof(*A))
#define INDEX_BUFSIZE	32		/* max size of buffer for indices */
#define TEST_KEY	"#TEST KEY#"	/* index for testing array existence */

/*
 * Assigned bits of "flags" fields of Table structures, and what those
 * bits mean:
 *
 * REDRAW_PENDING:	Non-zero means a DoWhenIdle handler has
 *			already been queued to redisplay the table.
 * REDRAW_BORDER:	Non-zero means 3-D border must be redrawn
 *			around window during redisplay.	 Normally
 *			only text portion needs to be redrawn.
 * CURSOR_ON:		Non-zero means insert cursor is displayed at
 *			present.  0 means it isn't displayed.
 * TEXT_CHANGED:	Non-zero means the active cell text is being edited.
 * HAS_FOCUS:		Non-zero means this window has the input focus.
 * HAS_ACTIVE:		Non-zero means the active cell is set.
 * HAS_ANCHOR:		Non-zero means the anchor cell is set.
 * BROWSE_CMD:		Non-zero means we're evaluating the -browsecommand.
 * VALIDATING:		Non-zero means we are in a valCmd
 * SET_ACTIVE:		About to set the active array element internally
 * ACTIVE_DISABLED:	Non-zero means the active cell is -state disabled
 * OVER_BORDER:		Non-zero means we are over a table cell border
 * REDRAW_ON_MAP:	Forces a redraw on the unmap
 * AVOID_SPANS:		prevent cell spans from being used
 *
 * FIX - consider adding UPDATE_SCROLLBAR a la entry
 */
#define REDRAW_PENDING		(1L<<0)
#define CURSOR_ON		(1L<<1)
#define	HAS_FOCUS		(1L<<2)
#define TEXT_CHANGED		(1L<<3)
#define HAS_ACTIVE		(1L<<4)
#define HAS_ANCHOR		(1L<<5)
#define BROWSE_CMD		(1L<<6)
#define REDRAW_BORDER		(1L<<7)
#define VALIDATING		(1L<<8)
#define SET_ACTIVE		(1L<<9)
#define ACTIVE_DISABLED		(1L<<10)
#define OVER_BORDER		(1L<<11)
#define REDRAW_ON_MAP		(1L<<12)
#define AVOID_SPANS		(1L<<13)

/* Flags for TableInvalidate && TableRedraw */
#define ROW		(1L<<0)
#define COL		(1L<<1)
#define CELL		(1L<<2)

#define CELL_BAD	(1<<0)
#define CELL_OK		(1<<1)
#define CELL_SPAN	(1<<2)
#define CELL_HIDDEN	(1<<3)
#define CELL_VIEWABLE	(CELL_OK|CELL_SPAN)

#define INV_FILL	(1L<<3)	/* use for Redraw when the affected
				 * row/col will affect neighbors */
#define INV_FORCE	(1L<<4)
#define INV_HIGHLIGHT	(1L<<5)
#define INV_NO_ERR_MSG	(1L<<5) /* Don't leave an error message */

/* These alter how the selection set/clear commands behave */
#define SEL_ROW		(1<<0)
#define SEL_COL		(1<<1)
#define SEL_BOTH	(1<<2)
#define SEL_CELL	(1<<3)
#define SEL_NONE	(1<<4)

/*
 * Definitions for tablePtr->dataSource, by bit
 */
#define DATA_NONE	0
#define DATA_CACHE	(1<<1)
#define	DATA_ARRAY	(1<<2)
#define DATA_COMMAND	(1<<3)

/*
 * Definitions for configuring -borderwidth
 */
#define BD_TABLE	0
#define BD_TABLE_TAG	(1<<1)
#define BD_TABLE_WIN	(1<<2)

/*
 * Possible state values for tags
 */
typedef enum {
    STATE_UNUSED, STATE_UNKNOWN, STATE_HIDDEN,
    STATE_NORMAL, STATE_DISABLED, STATE_ACTIVE, STATE_LAST
} TableState;

/*
 * Structure for use in parsing table commands/values.
 * Accessor functions defined in tkTableUtil.c
 */
typedef struct {
  char *name;		/* name of the command/value */
  int value;		/* >0 because 0 represents an error or proc */
} Cmd_Struct;

/*
 * The tag structure
 */
typedef struct {
    Tk_3DBorder	bg;		/* background color */
    Tk_3DBorder	fg;		/* foreground color */

    char *	borderStr;	/* border style */
    int		borders;	/* number of borders specified (1, 2 or 4) */
    int		bd[4];		/* cell border width */

    int		relief;		/* relief type */
    Tk_Font	tkfont;		/* Information about text font, or NULL. */
    Tk_Anchor	anchor;		/* default anchor point */
    char *	imageStr;	/* name of image */
    Tk_Image	image;		/* actual pointer to image, if any */
    TableState	state;		/* state of the cell */
    Tk_Justify	justify;	/* justification of text in the cell */
    int		multiline;	/* wrapping style of multiline text */
    int		wrap;		/* wrapping style of multiline text */
    int		showtext;	/* whether to display text over image */
    char *	ellipsis;	/* ellipsis to display on clipped text */
} TableTag;

/*  The widget structure for the table Widget */

typedef struct {
    /* basic information about the window and the interpreter */
    Tk_Window tkwin;
    Display *display;
    Tcl_Interp *interp;
    Tcl_Command widgetCmd;	/* Token for entry's widget command. */

    /*
     * Configurable Options
     */
    int autoClear;
    char *selectMode;		/* single, browse, multiple, or extended */
    int selectType;		/* row, col, both, or cell */
    int selectTitles;		/* whether to do automatic title selection */
    int rows, cols;		/* number of rows and columns */
    int defRowHeight;		/* default row height in chars (positive)
				 * or pixels (negative) */
    int defColWidth;		/* default column width in chars (positive)
				 * or pixels (negative) */
    int maxReqCols;		/* the requested # cols to display */
    int maxReqRows;		/* the requested # rows to display */
    int maxReqWidth;		/* the maximum requested width in pixels */
    int maxReqHeight;		/* the maximum requested height in pixels */
    char *arrayVar;		/* name of traced array variable */
    char *rowSep;		/* separator string to place between
				 * rows when getting selection */
    char *colSep;		/* separator string to place between
				 * cols when getting selection */
    TableTag defaultTag;	/* the default tag colors/fonts etc */
    char *yScrollCmd;		/* the y-scroll command */
    char *xScrollCmd;		/* the x-scroll command */
    char *browseCmd;		/* the command that is called when the
				 * active cell changes */
    int caching;		/* whether to cache values of table */
    char *command;		/* A command to eval when get/set occurs
				 * for table values */
    int useCmd;			/* Signals whether to use command or the
				 * array variable, will be 0 if command errs */
    char *selCmd;		/* the command that is called to when a
				 * [selection get] call occurs for a table */
    char *valCmd;		/* Command prefix to use when invoking
				 * validate command.  NULL means don't
				 * invoke commands.  Malloc'ed. */
    int validate;		/* Non-zero means try to validate */
    Tk_3DBorder insertBg;	/* the cursor color */
    Tk_Cursor cursor;		/* the regular mouse pointer */
    Tk_Cursor bdcursor;		/* the mouse pointer when over borders */
#ifdef TITLE_CURSOR
    Tk_Cursor titleCursor;	/* the mouse pointer when over titles */
#endif
    int exportSelection;	/* Non-zero means tie internal table
				 * to X selection. */
    TableState state;		/* Normal or disabled.	Table is read-only
				 * when disabled. */
    int insertWidth;		/* Total width of insert cursor. */
    int insertBorderWidth;	/* Width of 3-D border around insert cursor. */
    int insertOnTime;		/* Number of milliseconds cursor should spend
				 * in "on" state for each blink. */
    int insertOffTime;		/* Number of milliseconds cursor should spend
				 * in "off" state for each blink. */
    int invertSelected;		/* Whether to draw selected cells swapping
				 * foreground and background */
    int colStretch;		/* The way to stretch columns if the window
				 * is too large */
    int rowStretch;		/* The way to stretch rows if the window is
				 * too large */
    int colOffset;		/* X index of leftmost col in the display */
    int rowOffset;		/* Y index of topmost row in the display */
    int drawMode;		/* The mode to use when redrawing */
    int flashMode;		/* Specifies whether flashing is enabled */
    int flashTime;		/* The number of ms to flash a cell for */
    int resize;			/* -resizeborders option for interactive
				 * resizing of borders */
    int sparse;			/* Whether to use "sparse" arrays by
				 * deleting empty array elements (default) */
    char *rowTagCmd, *colTagCmd;/* script to eval for getting row/tag cmd */
    int highlightWidth;		/* Width in pixels of highlight to draw
				 * around widget when it has the focus.
				 * <= 0 means don't draw a highlight. */
    XColor *highlightBgColorPtr;/* Color for drawing traversal highlight
				 * area when highlight is off. */
    XColor *highlightColorPtr;	/* Color for drawing traversal highlight. */
    char *takeFocus;		/* Used only in Tcl to check if this
				 * widget will accept focus */
    int padX, padY;		/* Extra space around text (pixels to leave
				 * on each side).  Ignored for bitmaps and
				 * images. */
    int ipadX, ipadY;		/* Space to leave empty around cell borders.
				 * This differs from pad* in that it is always
				 * present for the cell (except windows). */

    /*
     * Cached Information
     */
#ifdef TITLE_CURSOR
    Tk_Cursor *lastCursorPtr;	/* pointer to last cursor defined. */
#endif
    int titleRows, titleCols;	/* the number of rows|cols to use as a title */
    /* these are kept in real coords */
    int topRow, leftCol;	/* The topleft cell to display excluding the
				 * fixed title rows.  This is just the
				 * config request.  The actual cell used may
				 * be different to keep the screen full */
    int anchorRow, anchorCol;	/* the row,col of the anchor cell */
    int activeRow, activeCol;	/* the row,col of the active cell */
    int oldTopRow, oldLeftCol;	/* cached by TableAdjustParams */
    int oldActRow, oldActCol;	/* cached by TableAdjustParams */
    int icursor;		/* The index of the insertion cursor in the
				 * active cell */
    int flags;			/* An or'ed combination of flags concerning
				 * redraw/cursor etc. */
    int dataSource;		/* where our data comes from:
				 * DATA_{NONE,CACHE,ARRAY,COMMAND} */
    int maxWidth, maxHeight;	/* max width|height required in pixels */
    int charWidth, charHeight;	/* size of a character in the default font */
    int *colPixels, *rowPixels;	/* Array of the pixel widths/heights */
    int *colStarts, *rowStarts;	/* Array of start pixels for rows|columns */
    int scanMarkX, scanMarkY;	/* Used by "scan" and "border" to mark */
    int scanMarkRow, scanMarkCol;/* necessary information for dragto */
    /* values in these are kept in user coords */
    Tcl_HashTable *cache;	/* value cache */

    /*
     * colWidths and rowHeights are indexed from 0, so always adjust numbers
     * by the appropriate *Offset factor
     */
    Tcl_HashTable *colWidths;	/* hash table of non default column widths */
    Tcl_HashTable *rowHeights;	/* hash table of non default row heights */
    Tcl_HashTable *spanTbl;	/* table for spans */
    Tcl_HashTable *spanAffTbl;	/* table for cells affected by spans */
    Tcl_HashTable *tagTable;	/* table for style tags */
    Tcl_HashTable *winTable;	/* table for embedded windows */
    Tcl_HashTable *rowStyles;	/* table for row styles */
    Tcl_HashTable *colStyles;	/* table for col styles */
    Tcl_HashTable *cellStyles;	/* table for cell styles */
    Tcl_HashTable *flashCells;	/* table of flashing cells */
    Tcl_HashTable *selCells;	/* table of selected cells */
    Tcl_TimerToken cursorTimer;	/* timer token for the cursor blinking */
    Tcl_TimerToken flashTimer;	/* timer token for the cell flashing */
    char *activeBuf;		/* buffer where the selection is kept
				 * for editing the active cell */
    char **tagPrioNames;	/* list of tag names in priority order */
    TableTag **tagPrios;	/* list of tag pointers in priority order */
    TableTag *activeTagPtr;	/* cache of active composite tag */
    int activeX, activeY;	/* cache offset of active layout in cell */
    int tagPrioSize;		/* size of tagPrios list */
    int tagPrioMax;		/* max allocated size of tagPrios list */

    /* The invalid rectangle if there is an update pending */
    int invalidX, invalidY, invalidWidth, invalidHeight;
    int seen[4];			/* see TableUndisplay */

#ifdef POSTSCRIPT
    /* Pointer to information used for generating Postscript for the canvas.
     * NULL means no Postscript is currently being generated. */
    struct TkPostscriptInfo *psInfoPtr;
#endif

#ifdef PROCS
    Tcl_HashTable *inProc;	/* cells where proc is being evaled */
    int showProcs;		/* whether to show embedded proc (1) or
				 * its calculated value (0) */
    int hasProcs;		/* whether table has embedded procs or not */
#endif
} Table;

/*
 * HEADERS FOR EMBEDDED WINDOWS
 */

/*
 * A structure of the following type holds information for each window
 * embedded in a table widget.
 */

typedef struct TableEmbWindow {
    Table *tablePtr;		/* Information about the overall table
				 * widget. */
    Tk_Window tkwin;		/* Window for this segment.  NULL means that
				 * the window hasn't been created yet. */
    Tcl_HashEntry *hPtr;	/* entry into winTable */
    char *create;		/* Script to create window on-demand.
				 * NULL means no such script.
				 * Malloc-ed. */
    Tk_3DBorder bg;		/* background color */

    char *borderStr;		/* border style */
    int borders;		/* number of borders specified (1, 2 or 4) */
    int bd[4];			/* border width for cell around window */

    int relief;			/* relief type */
    int sticky;			/* How to align window in space */
    int padX, padY;		/* Padding to leave around each side
				 * of window, in pixels. */
    int displayed;		/* Non-zero means that the window has been
				 * displayed on the screen recently. */
} TableEmbWindow;

extern Tk_ConfigSpec tableSpecs[];

extern void	EmbWinDisplay(Table *tablePtr, Drawable window,
			TableEmbWindow *ewPtr, TableTag *tagPtr,
			int x, int y, int width, int height);
extern void	EmbWinUnmap(register Table *tablePtr,
			int rlo, int rhi, int clo, int chi);
extern void	EmbWinDelete(register Table *tablePtr, TableEmbWindow *ewPtr);
extern int	Table_WinMove(register Table *tablePtr,
			char *CONST srcPtr, char *CONST destPtr, int flags);
extern int	Table_WinDelete(register Table *tablePtr, char *CONST idxPtr);
extern int	Table_WindowCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	TableValidateChange(Table *tablePtr, int r,
			int c, char *oldVal, char *newVal, int idx);
extern void	TableLostSelection(ClientData clientData);
extern void	TableSetActiveIndex(register Table *tablePtr);

/*
 * HEADERS IN tkTableCmds.c
 */

extern int	Table_ActivateCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_AdjustCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_BboxCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_BorderCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_ClearCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_CurselectionCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_CurvalueCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_GetCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_ScanCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_SeeCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_SelAnchorCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_SelClearCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_SelIncludesCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_SelSetCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_ViewCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);

/*
 * HEADERS IN tkTableEdit.c
 */

extern int	Table_EditCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern void	TableDeleteChars(register Table *tablePtr,
			int idx, int count);
extern void	TableInsertChars(register Table *tablePtr,
			int idx, char *string);

/*
 * HEADERS IN tkTableTag.c
 */

extern TableTag *TableNewTag(Table *tablePtr);
extern void	TableResetTag(Table *tablePtr, TableTag *tagPtr);
extern void	TableMergeTag(Table *tablePtr, TableTag *baseTag,
			TableTag *addTag);
extern void	TableInvertTag(TableTag *baseTag);
extern int	TableGetTagBorders(TableTag *tagPtr,
			int *left, int *right, int *top, int *bottom);
extern void	TableInitTags(Table *tablePtr);
extern TableTag *FindRowColTag(Table *tablePtr,
			int cell, int type);
extern void	TableCleanupTag(Table *tablePtr,
			TableTag *tagPtr);
extern int	Table_TagCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);

/*
 * HEADERS IN tkTableUtil.c
 */

extern void	Table_ClearHashTable(Tcl_HashTable *hashTblPtr);
extern int	TableOptionBdSet(ClientData clientData,
			Tcl_Interp *interp, Tk_Window tkwin,
			CONST84 char *value, char *widgRec, int offset);
extern char *	TableOptionBdGet(ClientData clientData,
			Tk_Window tkwin, char *widgRec, int offset,
			Tcl_FreeProc **freeProcPtr);
extern int	TableTagConfigureBd(Table *tablePtr,
			TableTag *tagPtr, char *oldValue, int nullOK);
extern int	Cmd_OptionSet(ClientData clientData,
			Tcl_Interp *interp,
			Tk_Window unused, CONST84 char *value,
			char *widgRec, int offset);
extern char *	Cmd_OptionGet(ClientData clientData,
			Tk_Window unused, char *widgRec,
			int offset, Tcl_FreeProc **freeProcPtr);

/*
 * HEADERS IN tkTableCell.c
 */

extern int	TableTrueCell(Table *tablePtr, int row, int col,
					   int *trow, int *tcol);
extern int	TableCellCoords(Table *tablePtr, int row,
			int col, int *rx, int *ry, int *rw, int *rh);
extern int	TableCellVCoords(Table *tablePtr, int row,
			int col, int *rx, int *ry,
			int *rw, int *rh, int full);
extern void	TableWhatCell(register Table *tablePtr,
			int x, int y, int *row, int *col);
extern int	TableAtBorder(Table *tablePtr, int x, int y,
			int *row, int *col);
extern char *	TableGetCellValue(Table *tablePtr, int r, int c);
extern int	TableSetCellValue(Table *tablePtr, int r, int c,
			char *value);
extern int    TableMoveCellValue(Table *tablePtr,
			int fromr, int fromc, char *frombuf,
			int tor, int toc, char *tobuf, int outOfBounds);

extern int	TableGetIcursor(Table *tablePtr, char *arg,
			int *posn);
#define TableGetIcursorObj(tablePtr, objPtr, posnPtr) \
	TableGetIcursor(tablePtr, Tcl_GetString(objPtr), posnPtr)
extern int	TableGetIndex(register Table *tablePtr,
			char *str, int *row_p, int *col_p);
#define TableGetIndexObj(tablePtr, objPtr, rowPtr, colPtr) \
	TableGetIndex(tablePtr, Tcl_GetString(objPtr), rowPtr, colPtr)
extern int	Table_SetCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_HiddenCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int	Table_SpanCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern void	TableSpanSanCheck(register Table *tablePtr);

/*
 * HEADERS IN TKTABLECELLSORT
 */
/*
 * We keep the old CellSort true because it is used for grabbing
 * the selection, so we really want them ordered
 */
extern char *	TableCellSort(Table *tablePtr, char *str);
#ifdef NO_SORT_CELLS
#  define TableCellSortObj(interp, objPtr) (objPtr)
#else
extern Tcl_Obj*	TableCellSortObj(Tcl_Interp *interp, Tcl_Obj *listObjPtr);
#endif

/*
 * HEADERS IN TKTABLEPS
 */

#ifdef POSTSCRIPT
extern int	Table_PostscriptCmd(ClientData clientData,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern void	Tcl_DStringAppendAllTCL_VARARGS(Tcl_DString *, arg1);
#endif

/*
 * HEADERS IN TKTABLE
 */

EXTERN int Tktable_Init(Tcl_Interp *interp);
EXTERN int Tktable_SafeInit(Tcl_Interp *interp);

extern void	TableGetActiveBuf(register Table *tablePtr);
extern void	ExpandPercents(Table *tablePtr, char *before,
			int r, int c, char *oldVal, char *newVal, int idx,
			Tcl_DString *dsPtr, int cmdType);
extern void	TableInvalidate(Table *tablePtr, int x, int y,
			int width, int height, int force);
extern void	TableRefresh(register Table *tablePtr,
			int arg1, int arg2, int mode);
extern void	TableGeometryRequest(Table *tablePtr);
extern void	TableAdjustActive(register Table *tablePtr);
extern void	TableAdjustParams(register Table *tablePtr);
extern void	TableConfigCursor(register Table *tablePtr);
extern void	TableAddFlash(Table *tablePtr, int row, int col);


#define TableInvalidateAll(tablePtr, flags) \
	TableInvalidate((tablePtr), 0, 0, Tk_Width((tablePtr)->tkwin),\
		Tk_Height((tablePtr)->tkwin), (flags))

     /*
      * Turn row/col into an index into the table
      */
#define TableMakeArrayIndex(r, c, i)	sprintf((i), "%d,%d", (r), (c))

     /*
      * Turn array index back into row/col
      * return the number of args parsed (should be two)
      */
#define TableParseArrayIndex(r, c, i)	sscanf((i), "%d,%d", (r), (c))

     /*
      * Macro for finding the last cell of the table
      */
#define TableGetLastCell(tablePtr, rowPtr, colPtr) \
	TableWhatCell((tablePtr),\
		Tk_Width((tablePtr)->tkwin)-(tablePtr)->highlightWidth-1,\
		Tk_Height((tablePtr)->tkwin)-(tablePtr)->highlightWidth-1,\
		(rowPtr), (colPtr))

/*
 * end of header
 * reset TCL_STORAGE_CLASS to DLLIMPORT.
 */
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _TKTABLE_H_ */

