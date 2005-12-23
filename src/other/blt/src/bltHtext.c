/*
 * bltHtext.c --
 *
 *	This module implements a hypertext widget for the BLT toolkit.
 *
 * Copyright 1991-1998 Lucent Technologies, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
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
 * The "htext" widget was created by George Howlett.
 */

/*
 * To do:
 *
 * 1) Fix scroll unit round off errors.
 *
 * 2) Better error checking.
 *
 * 3) Use html format.
 *
 * 4) The dimension of cavities using -relwidth and -relheight
 *    should be 0 when computing initial estimates for the size
 *    of the virtual text.
 */

#include "bltInt.h"

#ifndef NO_HTEXT
#include <bltChain.h>
#include <bltHash.h>
#include "bltTile.h"
 
#include <sys/stat.h>
#include <X11/Xatom.h>

#define DEF_LINES_ALLOC 512	/* Default block of lines allocated */
#define CLAMP(val,low,hi)	\
	(((val) < (low)) ? (low) : ((val) > (hi)) ? (hi) : (val))

/*
 * Justify option values
 */
typedef enum {
    JUSTIFY_CENTER, JUSTIFY_TOP, JUSTIFY_BOTTOM
} Justify;

extern Tk_CustomOption bltFillOption;
extern Tk_CustomOption bltPadOption;
extern Tk_CustomOption bltDistanceOption;
extern Tk_CustomOption bltTileOption;

static int StringToWidth _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int flags));
static int StringToHeight _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int flags));
static char *WidthHeightToString _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset, Tcl_FreeProc **freeProc));

static Tk_CustomOption widthOption =
{
    StringToWidth, WidthHeightToString, (ClientData)0
};

static Tk_CustomOption heightOption =
{
    StringToHeight, WidthHeightToString, (ClientData)0
};

static int StringToJustify _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int offset));
static char *JustifyToString _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset, Tcl_FreeProc **freeProcPtr));

static Tk_CustomOption justifyOption =
{
    StringToJustify, JustifyToString, (ClientData)0
};


static void EmbeddedWidgetGeometryProc _ANSI_ARGS_((ClientData, Tk_Window));
static void EmbeddedWidgetCustodyProc _ANSI_ARGS_((ClientData, Tk_Window));

static Tk_GeomMgr htextMgrInfo =
{
    "htext",			/* Name of geometry manager used by winfo */
    EmbeddedWidgetGeometryProc,	/* Procedure to for new geometry requests */
    EmbeddedWidgetCustodyProc,	/* Procedure when window is taken away */
};


/*
 * Line --
 *
 *	Structure to contain the contents of a single line of text and
 *	the widgets on that line.
 *
 * 	Individual lines are not configurable, although changes to the
 * 	size of widgets do effect its values.
 */
typedef struct {
    int offset;			/* Offset of line from y-origin (0) in
				 * world coordinates */
    int baseline;		/* Baseline y-coordinate of the text */
    short int width, height;	/* Dimensions of the line */
    int textStart, textEnd;	/* Start and end indices of characters
				 * forming the line in the text array */
    Blt_Chain *chainPtr;	/* Chain of embedded widgets on the line of 
				 * text */
} Line;

typedef struct {
    int textStart;
    int textEnd;
} Segment;

typedef struct {
    int x, y;
} Position;

/*
 * Hypertext widget.
 */
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

    Tcl_Command cmdToken;	/* Token for htext's widget command. */
    int flags;

    /* User-configurable fields */

    XColor *normalFg, *normalBg;
    Tk_Font font;		/* Font for normal text. May affect the size
				 * of the viewport if the width/height is
				 * specified in columns/rows */
    GC drawGC;			/* Graphics context for normal text */
    Blt_Tile tile;
    int tileOffsetPage;		/* Set tile offset to top of page instead
				 * of toplevel window */
    GC fillGC;			/* GC for clearing the window in the
				 * designated background color. The
				 * background color is the foreground
				 * attribute in GC.  */

    int nRows, nColumns;	/* # of characters of the current font
				 * for a row or column of the viewport.
				 * Used to determine the width and height
				 * of the text window (i.e. viewport) */
    int reqWidth, reqHeight;	/* Requested dimensions of the viewport */
    int maxWidth, maxHeight;	/* Maximum dimensions allowed for the viewport,
				 * regardless of the size of the text */

    Tk_Cursor cursor;		/* X Cursor */

    char *fileName;		/* If non-NULL, indicates the name of a
				 * hypertext file to be read into the widget.
				 * If NULL, the *text* field is considered
				 * instead */
    char *text;			/* Hypertext to be loaded into the widget. This
				 * value is ignored if *fileName* is non-NULL */
    int specChar;		/* Special character designating a TCL
			         * command block in a hypertext file. */
    int leader;			/* # of pixels between lines */

    char *yScrollCmdPrefix;	/* Name of vertical scrollbar to invoke */
    int yScrollUnits;		/* # of pixels per vertical scroll */
    char *xScrollCmdPrefix;	/* Name of horizontal scroll bar to invoke */
    int xScrollUnits;		/* # of pixels per horizontal scroll */

    int reqLineNum;		/* Line requested by "goto" command */

    /*
     * The view port is the width and height of the window and the
     * origin of the viewport (upper left corner) in world coordinates.
     */
    int worldWidth, worldHeight;/* Size of view text in world coordinates */
    int xOffset, yOffset;	/* Position of viewport in world coordinates */

    int pendingX, pendingY;	/* New upper-left corner (origin) of
				 * the viewport (not yet posted) */

    int first, last;		/* Range of lines displayed */

    int lastWidth, lastHeight;
    /* Last known size of the window: saved to
				 * recognize when the viewport is resized. */

    Blt_HashTable widgetTable;	/* Table of embedded widgets. */

    /*
     * Selection display information:
     */
    Tk_3DBorder selBorder;	/* Border and background color */
    int selBorderWidth;		/* Border width */
    XColor *selFgColor;		/* Text foreground color */
    GC selectGC;		/* GC for drawing selected text */
    int selAnchor;		/* Fixed end of selection
			         * (i.e. "selection to" operation will
			         * use this as one end of the selection).*/
    int selFirst;		/* The index of first character in the
				 * text array selected */
    int selLast;		/* The index of the last character selected */
    int exportSelection;	/* Non-zero means to export the internal text
				 * selection to the X server. */
    char *takeFocus;

    /*
     * Scanning information:
     */
    XPoint scanMark;		/* Anchor position of scan */
    XPoint scanPt;		/* x,y position where the scan started. */

    char *charArr;		/* Pool of characters representing the text
				 * to be displayed */
    int nChars;			/* Length of the text pool */

    Line *lineArr;		/* Array of pointers to text lines */
    int nLines;			/* # of line entered into array. */
    int arraySize;		/* Size of array allocated. */

} HText;

/*
 * Bit flags for the hypertext widget:
 */
#define REDRAW_PENDING	 (1<<0)	/* A DoWhenIdle handler has already
				 * been queued to redraw the window */
#define IGNORE_EXPOSURES (1<<1)	/* Ignore exposure events in the text
				 * window.  Potentially many expose
				 * events can occur while rearranging
				 * embedded widgets during a single call to
				 * the DisplayText.  */

#define REQUEST_LAYOUT 	(1<<4)	/* Something has happened which
				 * requires the layout of text and
				 * embedded widget positions to be
				 * recalculated.  The following
				 * actions may cause this:
				 *
				 * 1) the contents of the hypertext
				 *    has changed by either the -file or
				 *    -text options.
				 *
				 * 2) a text attribute has changed
				 *    (line spacing, font, etc)
				 *
				 * 3) a embedded widget has been resized or
				 *    moved.
				 *
				 * 4) a widget configuration option has
				 *    changed.
				 */
#define TEXT_DIRTY 	(1<<5)	/* The layout was recalculated and the
				 * size of the world (text layout) has
				 * changed. */
#define GOTO_PENDING 	(1<<6)	/* Indicates the starting text line
				 * number has changed. To be reflected
				 * the next time the widget is redrawn. */
#define WIDGET_APPENDED	(1<<7)	/* Indicates a embedded widget has just
				 * been appended to the text.  This is
				 * used to determine when to add a
				 * space to the text array */

#define DEF_HTEXT_BACKGROUND		STD_NORMAL_BACKGROUND
#define DEF_HTEXT_BG_MONO		STD_NORMAL_BG_MONO
#define DEF_HTEXT_CURSOR		"arrow"
#define DEF_HTEXT_EXPORT_SELECTION	"1"

#define DEF_HTEXT_FOREGROUND		STD_NORMAL_FOREGROUND
#define DEF_HTEXT_FG_MONO		STD_NORMAL_FG_MONO
#define DEF_HTEXT_FILE_NAME		(char *)NULL
#define DEF_HTEXT_FONT			STD_FONT
#define DEF_HTEXT_HEIGHT		"0"
#define DEF_HTEXT_LINE_SPACING		"1"
#define DEF_HTEXT_MAX_HEIGHT		(char *)NULL
#define DEF_HTEXT_MAX_WIDTH 		(char *)NULL
#define DEF_HTEXT_SCROLL_UNITS		"10"
#define DEF_HTEXT_SPEC_CHAR		"0x25"
#define DEF_HTEXT_SELECT_BORDERWIDTH 	STD_SELECT_BORDERWIDTH
#define DEF_HTEXT_SELECT_BACKGROUND 	STD_SELECT_BACKGROUND
#define DEF_HTEXT_SELECT_BG_MONO  	STD_SELECT_BG_MONO
#define DEF_HTEXT_SELECT_FOREGROUND 	STD_SELECT_FOREGROUND
#define DEF_HTEXT_SELECT_FG_MONO  	STD_SELECT_FG_MONO
#define DEF_HTEXT_TAKE_FOCUS		"1"
#define DEF_HTEXT_TEXT			(char *)NULL
#define DEF_HTEXT_TILE_OFFSET		"1"
#define DEF_HTEXT_WIDTH			"0"

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_HTEXT_BACKGROUND, Tk_Offset(HText, normalBg), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_HTEXT_BG_MONO, Tk_Offset(HText, normalBg), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_HTEXT_CURSOR, Tk_Offset(HText, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-exportselection", "exportSelection", "ExportSelection",
	DEF_HTEXT_EXPORT_SELECTION, Tk_Offset(HText, exportSelection), 0},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_STRING, "-file", "file", "File",
	DEF_HTEXT_FILE_NAME, Tk_Offset(HText, fileName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_HTEXT_FONT, Tk_Offset(HText, font), 0},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_HTEXT_FOREGROUND, Tk_Offset(HText, normalFg), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_HTEXT_FG_MONO, Tk_Offset(HText, normalFg), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-height", "height", "Height",
	DEF_HTEXT_HEIGHT, Tk_Offset(HText, reqHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &heightOption},
    {TK_CONFIG_CUSTOM, "-linespacing", "lineSpacing", "LineSpacing",
	DEF_HTEXT_LINE_SPACING, Tk_Offset(HText, leader),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-maxheight", "maxHeight", "MaxHeight",
	DEF_HTEXT_MAX_HEIGHT, Tk_Offset(HText, maxHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-maxwidth", "maxWidth", "MaxWidth",
	DEF_HTEXT_MAX_WIDTH, Tk_Offset(HText, maxWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Background",
	DEF_HTEXT_SELECT_BG_MONO, Tk_Offset(HText, selBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Background",
	DEF_HTEXT_SELECT_BACKGROUND, Tk_Offset(HText, selBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_CUSTOM, "-selectborderwidth", "selectBorderWidth", "BorderWidth",
	DEF_HTEXT_SELECT_BORDERWIDTH, Tk_Offset(HText, selBorderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Foreground",
	DEF_HTEXT_SELECT_FG_MONO, Tk_Offset(HText, selFgColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Foreground",
	DEF_HTEXT_SELECT_FOREGROUND, Tk_Offset(HText, selFgColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_INT, "-specialchar", "specialChar", "SpecialChar",
	DEF_HTEXT_SPEC_CHAR, Tk_Offset(HText, specChar), 0},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_HTEXT_TAKE_FOCUS, Tk_Offset(HText, takeFocus),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-tile", "tile", "Tile",
	(char *)NULL, Tk_Offset(HText, tile), TK_CONFIG_NULL_OK,
	&bltTileOption},
    {TK_CONFIG_BOOLEAN, "-tileoffset", "tileOffset", "TileOffset",
	DEF_HTEXT_TILE_OFFSET, Tk_Offset(HText, tileOffsetPage), 0},
    {TK_CONFIG_STRING, "-text", "text", "Text",
	DEF_HTEXT_TEXT, Tk_Offset(HText, text), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-width", "width", "Width",
	DEF_HTEXT_WIDTH, Tk_Offset(HText, reqWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &widthOption},
    {TK_CONFIG_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
	(char *)NULL, Tk_Offset(HText, xScrollCmdPrefix), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-xscrollunits", "xScrollUnits", "ScrollUnits",
	DEF_HTEXT_SCROLL_UNITS, Tk_Offset(HText, xScrollUnits),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_STRING, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
	(char *)NULL, Tk_Offset(HText, yScrollCmdPrefix), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-yscrollunits", "yScrollUnits", "yScrollUnits",
	DEF_HTEXT_SCROLL_UNITS, Tk_Offset(HText, yScrollUnits),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

typedef struct {
    HText *htPtr;		/* Pointer to parent's Htext structure */
    Tk_Window tkwin;		/* Widget window */
    int flags;

    int x, y;			/* Origin of embedded widget in text */

    int cavityWidth, cavityHeight; /* Dimensions of the cavity
				    * surrounding the embedded widget */
    /*
     *  Dimensions of the embedded widget.  Compared against actual
     *	embedded widget sizes when checking for resizing.
     */
    int winWidth, winHeight;

    int precedingTextEnd;	/* Index (in charArr) of the the last
				 * character immediatedly preceding
				 * the embedded widget */
    int precedingTextWidth;	/* Width of normal text preceding widget. */

    Tk_Anchor anchor;
    Justify justify;		/* Justification of region wrt to line */

    /*
     * Requested dimensions of the cavity (includes padding). If non-zero,
     * it overrides the calculated dimension of the cavity.
     */
    int reqCavityWidth, reqCavityHeight;

    /*
     * Relative dimensions of cavity wrt the size of the viewport. If
     * greater than 0.0.
     */
    double relCavityWidth, relCavityHeight;

    int reqWidth, reqHeight;	/* If non-zero, overrides the requested
				 * dimension of the embedded widget */

    double relWidth, relHeight;	/* Relative dimensions of embedded
				 * widget wrt the size of the viewport */

    Blt_Pad padX, padY;		/* Extra padding to frame around */

    int ipadX, ipadY;		/* internal padding for window */

    int fill;			/* Fill style flag */

} EmbeddedWidget;

/*
 * Flag bits embedded widgets:
 */
#define WIDGET_VISIBLE	(1<<2)	/* Widget is currently visible in the
				 * viewport. */
#define WIDGET_NOT_CHILD (1<<3) /* Widget is not a child of hypertext. */
/*
 * Defaults for embedded widgets:
 */
#define DEF_WIDGET_ANCHOR        "center"
#define DEF_WIDGET_FILL		"none"
#define DEF_WIDGET_HEIGHT	"0"
#define DEF_WIDGET_JUSTIFY	"center"
#define DEF_WIDGET_PAD_X		"0"
#define DEF_WIDGET_PAD_Y		"0"
#define DEF_WIDGET_REL_HEIGHT	"0.0"
#define DEF_WIDGET_REL_WIDTH  	"0.0"
#define DEF_WIDGET_WIDTH  	"0"

static Tk_ConfigSpec widgetConfigSpecs[] =
{
    {TK_CONFIG_ANCHOR, "-anchor", (char *)NULL, (char *)NULL,
	DEF_WIDGET_ANCHOR, Tk_Offset(EmbeddedWidget, anchor),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-fill", (char *)NULL, (char *)NULL,
	DEF_WIDGET_FILL, Tk_Offset(EmbeddedWidget, fill),
	TK_CONFIG_DONT_SET_DEFAULT, &bltFillOption},
    {TK_CONFIG_CUSTOM, "-cavityheight", (char *)NULL, (char *)NULL,
	DEF_WIDGET_HEIGHT, Tk_Offset(EmbeddedWidget, reqCavityHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-cavitywidth", (char *)NULL, (char *)NULL,
	DEF_WIDGET_WIDTH, Tk_Offset(EmbeddedWidget, reqCavityWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-height", (char *)NULL, (char *)NULL,
	DEF_WIDGET_HEIGHT, Tk_Offset(EmbeddedWidget, reqHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-justify", (char *)NULL, (char *)NULL,
	DEF_WIDGET_JUSTIFY, Tk_Offset(EmbeddedWidget, justify),
	TK_CONFIG_DONT_SET_DEFAULT, &justifyOption},
    {TK_CONFIG_CUSTOM, "-padx", (char *)NULL, (char *)NULL,
	DEF_WIDGET_PAD_X, Tk_Offset(EmbeddedWidget, padX),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-pady", (char *)NULL, (char *)NULL,
	DEF_WIDGET_PAD_Y, Tk_Offset(EmbeddedWidget, padY),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_DOUBLE, "-relcavityheight", (char *)NULL, (char *)NULL,
	DEF_WIDGET_REL_HEIGHT, Tk_Offset(EmbeddedWidget, relCavityHeight),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_DOUBLE, "-relcavitywidth", (char *)NULL, (char *)NULL,
	DEF_WIDGET_REL_WIDTH, Tk_Offset(EmbeddedWidget, relCavityWidth),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_DOUBLE, "-relheight", (char *)NULL, (char *)NULL,
	DEF_WIDGET_REL_HEIGHT, Tk_Offset(EmbeddedWidget, relHeight),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_DOUBLE, "-relwidth", (char *)NULL, (char *)NULL,
	DEF_WIDGET_REL_WIDTH, Tk_Offset(EmbeddedWidget, relWidth),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-width", (char *)NULL, (char *)NULL,
	DEF_WIDGET_WIDTH, Tk_Offset(EmbeddedWidget, reqWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};


/* Forward Declarations */
static void DestroyText _ANSI_ARGS_((DestroyData dataPtr));
static void EmbeddedWidgetEventProc _ANSI_ARGS_((ClientData clientdata, 
	XEvent *eventPtr));
static void DisplayText _ANSI_ARGS_((ClientData clientData));
static void TextDeleteCmdProc _ANSI_ARGS_((ClientData clientdata));

static Tcl_VarTraceProc TextVarProc;
static Blt_TileChangedProc TileChangedProc;
static Tk_LostSelProc TextLostSelection;
static Tk_SelectionProc TextSelectionProc;
static Tk_EventProc TextEventProc;
static Tcl_CmdProc TextWidgetCmd;
static Tcl_CmdProc TextCmd;
/* end of Forward Declarations */


 /* Custom options */
/*
 *----------------------------------------------------------------------
 *
 * StringToJustify --
 *
 * 	Converts the justification string into its numeric
 * 	representation. This configuration option affects how the
 *	embedded widget is positioned with respect to the line on which
 *	it sits.
 *
 *	Valid style strings are:
 *
 *	"top"      Uppermost point of region is top of the line's
 *		   text
 * 	"center"   Center point of region is line's baseline.
 *	"bottom"   Lowermost point of region is bottom of the
 *		   line's text
 *
 * Returns:
 *	A standard Tcl result.  If the value was not valid
 *
 *---------------------------------------------------------------------- */
/*ARGSUSED*/
static int
StringToJustify(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* Justification string */
    char *widgRec;		/* Structure record */
    int offset;			/* Offset of justify in record */
{
    Justify *justPtr = (Justify *)(widgRec + offset);
    unsigned int length;
    char c;

    c = string[0];
    length = strlen(string);
    if ((c == 'c') && (strncmp(string, "center", length) == 0)) {
	*justPtr = JUSTIFY_CENTER;
    } else if ((c == 't') && (strncmp(string, "top", length) == 0)) {
	*justPtr = JUSTIFY_TOP;
    } else if ((c == 'b') && (strncmp(string, "bottom", length) == 0)) {
	*justPtr = JUSTIFY_BOTTOM;
    } else {
	Tcl_AppendResult(interp, "bad justification argument \"", string,
	    "\": should be \"center\", \"top\", or \"bottom\"", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NameOfJustify --
 *
 *	Returns the justification style string based upon the value.
 *
 * Results:
 *	The static justification style string is returned.
 *
 *----------------------------------------------------------------------
 */
static char *
NameOfJustify(justify)
    Justify justify;
{
    switch (justify) {
    case JUSTIFY_CENTER:
	return "center";
    case JUSTIFY_TOP:
	return "top";
    case JUSTIFY_BOTTOM:
	return "bottom";
    default:
	return "unknown justification value";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * JustifyToString --
 *
 *	Returns the justification style string based upon the value.
 *
 * Results:
 *	The justification style string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
JustifyToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Structure record */
    int offset;			/* Offset of justify record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    Justify justify = *(Justify *)(widgRec + offset);

    return NameOfJustify(justify);
}

/*
 *----------------------------------------------------------------------
 *
 * GetScreenDistance --
 *
 *	Converts the given string into the screen distance or number
 *	of characters.  The valid formats are
 *
 *	    N	- pixels	Nm - millimeters
 *	    Ni  - inches        Np - pica
 *          Nc  - centimeters   N# - number of characters
 *
 *	where N is a non-negative decimal number.
 *
 * Results:
 *	A standard Tcl result.  The screen distance and the number of
 *	characters are returned.  If the string can't be converted,
 *	TCL_ERROR is returned and interp->result will contain an error
 *	message.
 *
 *----------------------------------------------------------------------
 */
static int
GetScreenDistance(interp, tkwin, string, sizePtr, countPtr)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    char *string;
    int *sizePtr;
    int *countPtr;
{
    int nPixels, nChars;
    char *endPtr;		/* Pointer to last character scanned */
    double value;
    int rounded;

    value = strtod(string, &endPtr);
    if (endPtr == string) {
	Tcl_AppendResult(interp, "bad screen distance \"", string, "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    if (value < 0.0) {
	Tcl_AppendResult(interp, "screen distance \"", string,
	    "\" must be non-negative value", (char *)NULL);
	return TCL_ERROR;
    }
    while (isspace(UCHAR(*endPtr))) {
	if (*endPtr == '\0') {
	    break;
	}
	endPtr++;
    }
    nPixels = nChars = 0;
    rounded = ROUND(value);
    switch (*endPtr) {
    case '\0':			/* Distance in pixels */
	nPixels = rounded;
	break;
    case '#':			/* Number of characters */
	nChars = rounded;
	break;
    default:			/* cm, mm, pica, inches */
	if (Tk_GetPixels(interp, tkwin, string, &rounded) != TCL_OK) {
	    return TCL_ERROR;
	}
	nPixels = rounded;
	break;
    }
    *sizePtr = nPixels;
    *countPtr = nChars;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToHeight --
 *
 *	Like TK_CONFIG_PIXELS, but adds an extra check for negative
 *	values.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToHeight(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window */
    char *string;		/* Pixel value string */
    char *widgRec;		/* Widget record */
    int offset;			/* Not used. */
{
    HText *htPtr = (HText *)widgRec;
    int height, nRows;

    if (GetScreenDistance(interp, tkwin, string, &height, &nRows) != TCL_OK) {
	return TCL_ERROR;
    }
    htPtr->nRows = nRows;
    htPtr->reqHeight = height;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToWidth --
 *
 *	Like TK_CONFIG_PIXELS, but adds an extra check for negative
 *	values.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToWidth(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window */
    char *string;		/* Pixel value string */
    char *widgRec;		/* Widget record */
    int offset;			/* Not used. */
{
    HText *htPtr = (HText *)widgRec;
    int width, nColumns;

    if (GetScreenDistance(interp, tkwin, string, &width,
	    &nColumns) != TCL_OK) {
	return TCL_ERROR;
    }
    htPtr->nColumns = nColumns;
    htPtr->reqWidth = width;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WidthHeightToString --
 *
 *	Returns the string representing the positive pixel size.
 *
 * Results:
 *	The pixel size string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
WidthHeightToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Row/column structure record */
    int offset;			/* Offset of fill in Partition record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    int pixels = *(int *)(widgRec + offset);
    char *result;
    char string[200];

    sprintf(string, "%d", pixels);
    result = Blt_Strdup(string);
    if (result == NULL) {
	return "out of memory";
    }
    *freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    return result;
}

/* General routines */
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
EventuallyRedraw(htPtr)
    HText *htPtr;		/* Information about widget. */
{
    if ((htPtr->tkwin != NULL) && !(htPtr->flags & REDRAW_PENDING)) {
	htPtr->flags |= REDRAW_PENDING;
	Tcl_DoWhenIdle(DisplayText, htPtr);
    }
}

/*
 * --------------------------------------------------------------------
 *
 * ResizeArray --
 *
 *	Reallocates memory to the new size given.  New memory
 *	is also cleared (zeros).
 *
 * Results:
 *	Returns a pointer to the new object or NULL if an error occurred.
 *
 * Side Effects:
 *	Memory is re/allocated.
 *
 * --------------------------------------------------------------------
 */
static int
ResizeArray(arrayPtr, elemSize, newSize, prevSize)
    char **arrayPtr;
    int elemSize;
    int newSize;
    int prevSize;
{
    char *newPtr;

    if (newSize == prevSize) {
	return TCL_OK;
    }
    if (newSize == 0) {		/* Free entire array */
	Blt_Free(*arrayPtr);
	*arrayPtr = NULL;
	return TCL_OK;
    }
    newPtr = Blt_Calloc(elemSize, newSize);
    if (newPtr == NULL) {
	return TCL_ERROR;
    }
    if ((prevSize > 0) && (*arrayPtr != NULL)) {
	int size;

	size = MIN(prevSize, newSize) * elemSize;
	if (size > 0) {
	    memcpy(newPtr, *arrayPtr, size);
	}
	Blt_Free(*arrayPtr);
    }
    *arrayPtr = newPtr;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * LineSearch --
 *
 * 	Performs a binary search for the line of text located at some
 * 	world y-coordinate (not screen y-coordinate). The search is
 * 	inclusive of those lines from low to high.
 *
 * Results:
 *	Returns the array index of the line found at the given
 *	y-coordinate.  If the y-coordinate is outside of the given range
 *	of lines, -1 is returned.
 *
 * ----------------------------------------------------------------------
 */
static int
LineSearch(htPtr, yCoord, low, high)
    HText *htPtr;		/* HText widget */
    int yCoord;			/* Search y-coordinate  */
    int low, high;		/* Range of lines to search */
{
    int median;
    Line *linePtr;

    while (low <= high) {
	median = (low + high) >> 1;
	linePtr = htPtr->lineArr + median;
	if (yCoord < linePtr->offset) {
	    high = median - 1;
	} else if (yCoord >= (linePtr->offset + linePtr->height)) {
	    low = median + 1;
	} else {
	    return median;
	}
    }
    return -1;
}

/*
 * ----------------------------------------------------------------------
 *
 * IndexSearch --
 *
 *	Try to find what line contains a given text index. Performs
 *	a binary search for the text line which contains the given index.
 *	The search is inclusive of those lines from low and high.
 *
 * Results:
 *	Returns the line number containing the given index. If the index
 *	is outside the range of lines, -1 is returned.
 *
 * ----------------------------------------------------------------------
 */
static int
IndexSearch(htPtr, key, low, high)
    HText *htPtr;		/* HText widget */
    int key;			/* Search index */
    int low, high;		/* Range of lines to search */
{
    int median;
    Line *linePtr;

    while (low <= high) {
	median = (low + high) >> 1;
	linePtr = htPtr->lineArr + median;
	if (key < linePtr->textStart) {
	    high = median - 1;
	} else if (key > linePtr->textEnd) {
	    low = median + 1;
	} else {
	    return median;
	}
    }
    return -1;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetXYPosIndex --
 *
 * 	Converts a string in the form "@x,y", where x and y are
 *	window coordinates, to a text index.
 *
 *	Window coordinates are first translated into world coordinates.
 *	Any coordinate outside of the bounds of the virtual text is
 *	silently set the nearest boundary.
 *
 * Results:
 *	A standard Tcl result.  If "string" is a valid index, then
 *	*indexPtr is filled with the numeric index corresponding.
 *	Otherwise an error message is left in interp->result.
 *
 * ----------------------------------------------------------------------
 */
static int
GetXYPosIndex(htPtr, string, indexPtr)
    HText *htPtr;
    char *string;
    int *indexPtr;
{
    int x, y, curX, dummy;
    int textLength, textStart;
    int cindex, lindex;
    Line *linePtr;

    if (Blt_GetXY(htPtr->interp, htPtr->tkwin, string, &x, &y) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Locate the line corresponding to the window y-coordinate position */

    y += htPtr->yOffset;
    if (y < 0) {
	lindex = htPtr->first;
    } else if (y >= htPtr->worldHeight) {
	lindex = htPtr->last;
    } else {
	lindex = LineSearch(htPtr, y, 0, htPtr->nLines - 1);
    }
    if (lindex < 0) {
	Tcl_AppendResult(htPtr->interp, "can't find line at \"", string, "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    x += htPtr->xOffset;
    if (x < 0) {
	x = 0;
    } else if (x > htPtr->worldWidth) {
	x = htPtr->worldWidth;
    }
    linePtr = htPtr->lineArr + lindex;
    curX = 0;
    textStart = linePtr->textStart;
    textLength = linePtr->textEnd - linePtr->textStart;
    if (Blt_ChainGetLength(linePtr->chainPtr) > 0) {
	Blt_ChainLink *linkPtr;
	int deltaX;
	EmbeddedWidget *winPtr;

	for (linkPtr = Blt_ChainFirstLink(linePtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    winPtr = Blt_ChainGetValue(linkPtr);
	    deltaX = winPtr->precedingTextWidth + winPtr->cavityWidth;
	    if ((curX + deltaX) > x) {
		textLength = (winPtr->precedingTextEnd - textStart);
		break;
	    }
	    curX += deltaX;
	    /*
	     * Skip over the trailing space. It designates the position of
	     * a embedded widget in the text
	     */
	    textStart = winPtr->precedingTextEnd + 1;
	}
    }
    cindex = Tk_MeasureChars(htPtr->font, htPtr->charArr + textStart,
	textLength, 10000, DEF_TEXT_FLAGS, &dummy);
    *indexPtr = textStart + cindex;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ParseIndex --
 *
 *	Parse a string representing a text index into numeric
 *	value.  A text index can be in one of the following forms.
 *
 *	  "anchor"	- anchor position of the selection.
 *	  "sel.first"   - index of the first character in the selection.
 *	  "sel.last"	- index of the last character in the selection.
 *	  "page.top"  	- index of the first character on the page.
 *	  "page.bottom"	- index of the last character on the page.
 *	  "@x,y"	- x and y are window coordinates.
 * 	  "number	- raw index of text
 *	  "line.char"	- line number and character position
 *
 * Results:
 *	A standard Tcl result.  If "string" is a valid index, then
 *	*indexPtr is filled with the corresponding numeric index.
 *	Otherwise an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */
static int
ParseIndex(htPtr, string, indexPtr)
    HText *htPtr;		/* Text for which the index is being
				 * specified. */
    char *string;		/* Numerical index into htPtr's element
				 * list, or "end" to refer to last element. */
    int *indexPtr;		/* Where to store converted relief. */
{
    unsigned int length;
    char c;
    Tcl_Interp *interp = htPtr->interp;

    length = strlen(string);
    c = string[0];

    if ((c == 'a') && (strncmp(string, "anchor", length) == 0)) {
	*indexPtr = htPtr->selAnchor;
    } else if ((c == 's') && (length > 4)) {
	if (strncmp(string, "sel.first", length) == 0) {
	    *indexPtr = htPtr->selFirst;
	} else if (strncmp(string, "sel.last", length) == 0) {
	    *indexPtr = htPtr->selLast;
	} else {
	    goto badIndex;	/* Not a valid index */
	}
	if (*indexPtr < 0) {
	    Tcl_AppendResult(interp, "bad index \"", string,
		"\": nothing selected in \"",
		Tk_PathName(htPtr->tkwin), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
    } else if ((c == 'p') && (length > 5) &&
	(strncmp(string, "page.top", length) == 0)) {
	int first;

	first = htPtr->first;
	if (first < 0) {
	    first = 0;
	}
	*indexPtr = htPtr->lineArr[first].textStart;
    } else if ((c == 'p') && (length > 5) &&
	(strncmp(string, "page.bottom", length) == 0)) {
	*indexPtr = htPtr->lineArr[htPtr->last].textEnd;
    } else if (c == '@') {	/* Screen position */
	if (GetXYPosIndex(htPtr, string, indexPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	char *period;

	period = strchr(string, '.');
	if (period == NULL) {	/* Raw index */
	    int tindex;

	    if ((string[0] == 'e') && (strcmp(string, "end") == 0)) {
		tindex = htPtr->nChars - 1;
	    } else if (Tcl_GetInt(interp, string, &tindex) != TCL_OK) {
		goto badIndex;
	    }
	    if (tindex < 0) {
		tindex = 0;
	    } else if (tindex > (htPtr->nChars - 1)) {
		tindex = htPtr->nChars - 1;
	    }
	    *indexPtr = tindex;
	} else {
	    int lindex, cindex, offset;
	    Line *linePtr;
	    int result;

	    *period = '\0';
	    result = TCL_OK;
	    if ((string[0] == 'e') && (strcmp(string, "end") == 0)) {
		lindex = htPtr->nLines - 1;
	    } else {
		result = Tcl_GetInt(interp, string, &lindex);
	    }
	    *period = '.';	/* Repair index string before returning */
	    if (result != TCL_OK) {
		goto badIndex;	/* Bad line number */
	    }
	    if (lindex < 0) {
		lindex = 0;	/* Silently repair bad line numbers */
	    }
	    if (htPtr->nChars == 0) {
		*indexPtr = 0;
		return TCL_OK;
	    }
	    if (lindex >= htPtr->nLines) {
		lindex = htPtr->nLines - 1;
	    }
	    linePtr = htPtr->lineArr + lindex;
	    cindex = 0;
	    if ((*(period + 1) != '\0')) {
		string = period + 1;
		if ((string[0] == 'e') && (strcmp(string, "end") == 0)) {
		    cindex = linePtr->textEnd - linePtr->textStart;
		} else if (Tcl_GetInt(interp, string, &cindex) != TCL_OK) {
		    goto badIndex;
		}
	    }
	    if (cindex < 0) {
		cindex = 0;	/* Silently fix bogus indices */
	    }
	    offset = 0;
	    if (htPtr->nChars > 0) {
		offset = linePtr->textStart + cindex;
		if (offset > linePtr->textEnd) {
		    offset = linePtr->textEnd;
		}
	    }
	    *indexPtr = offset;
	}
    }
    if (htPtr->nChars == 0) {
	*indexPtr = 0;
    }
    return TCL_OK;

  badIndex:

    /*
     * Some of the paths here leave messages in interp->result, so we
     * have to clear it out before storing our own message.
     */
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "bad index \"", string, "\": \
should be one of the following: anchor, sel.first, sel.last, page.bottom, \
page.top, @x,y, index, line.char", (char *)NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * GetIndex --
 *
 *	Get the index from a string representing a text index.
 *
 *
 * Results:
 *	A standard Tcl result.  If "string" is a valid index, then
 *	*indexPtr is filled with the numeric index corresponding.
 *	Otherwise an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */
static int
GetIndex(htPtr, string, indexPtr)
    HText *htPtr;		/* Text for which the index is being
				 * specified. */
    char *string;		/* Numerical index into htPtr's element
				 * list, or "end" to refer to last element. */
    int *indexPtr;		/* Where to store converted relief. */
{
    int tindex;

    if (ParseIndex(htPtr, string, &tindex) != TCL_OK) {
	return TCL_ERROR;
    }
    *indexPtr = tindex;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetTextPosition --
 *
 * 	Performs a binary search for the index located on line in
 *	the text. The search is limited to those lines between
 *	low and high inclusive.
 *
 * Results:
 *	Returns the line number at the given Y coordinate. If position
 *	does not correspond to any of the lines in the given the set,
 *	-1 is returned.
 *
 * ----------------------------------------------------------------------
 */
static int
GetTextPosition(htPtr, tindex, lindexPtr, cindexPtr)
    HText *htPtr;
    int tindex;
    int *lindexPtr;
    int *cindexPtr;
{
    int lindex, cindex;

    lindex = cindex = 0;
    if (htPtr->nChars > 0) {
	Line *linePtr;

	lindex = IndexSearch(htPtr, tindex, 0, htPtr->nLines - 1);
	if (lindex < 0) {
	    char string[200];

	    sprintf(string, "can't determine line number from index \"%d\"",
		tindex);
	    Tcl_AppendResult(htPtr->interp, string, (char *)NULL);
	    return TCL_ERROR;
	}
	linePtr = htPtr->lineArr + lindex;
	if (tindex > linePtr->textEnd) {
	    tindex = linePtr->textEnd;
	}
	cindex = tindex - linePtr->textStart;
    }
    *lindexPtr = lindex;
    *cindexPtr = cindex;
    return TCL_OK;
}

/* EmbeddedWidget Procedures */
/*
 *----------------------------------------------------------------------
 *
 * GetEmbeddedWidgetWidth --
 *
 *	Returns the width requested by the embedded widget. The requested
 *	space also includes any internal padding which has been designated
 *	for this window.
 *
 * Results:
 *	Returns the requested width of the embedded widget.
 *
 *----------------------------------------------------------------------
 */
static int
GetEmbeddedWidgetWidth(winPtr)
    EmbeddedWidget *winPtr;
{
    int width;

    if (winPtr->reqWidth > 0) {
	width = winPtr->reqWidth;
    } else if (winPtr->relWidth > 0.0) {
	width = (int)
	    ((double)Tk_Width(winPtr->htPtr->tkwin) * winPtr->relWidth + 0.5);
    } else {
	width = Tk_ReqWidth(winPtr->tkwin);
    }
    width += (2 * winPtr->ipadX);
    return width;
}

/*
 *----------------------------------------------------------------------
 *
 * GetEmbeddedWidgetHeight --
 *
 *	Returns the height requested by the embedded widget. The requested
 *	space also includes any internal padding which has been designated
 *	for this window.
 *
 * Results:
 *	Returns the requested height of the embedded widget.
 *
 *----------------------------------------------------------------------
 */
static int
GetEmbeddedWidgetHeight(winPtr)
    EmbeddedWidget *winPtr;
{
    int height;

    if (winPtr->reqHeight > 0) {
	height = winPtr->reqHeight;
    } else if (winPtr->relHeight > 0.0) {
	height = (int)((double)Tk_Height(winPtr->htPtr->tkwin) *
	    winPtr->relHeight + 0.5);
    } else {
	height = Tk_ReqHeight(winPtr->tkwin);
    }
    height += (2 * winPtr->ipadY);
    return height;
}

/*
 * --------------------------------------------------------------
 *
 * EmbeddedWidgetEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on hypertext widgets.
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
EmbeddedWidgetEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about the embedded widget. */
    XEvent *eventPtr;		/* Information about event. */
{
    EmbeddedWidget *winPtr = clientData;
    HText *htPtr;

    if ((winPtr == NULL) || (winPtr->tkwin == NULL)) {
	return;
    }
    htPtr = winPtr->htPtr;

    if (eventPtr->type == DestroyNotify) {
	Blt_HashEntry *hPtr;
	/*
	 * Mark the widget as deleted by dereferencing the Tk window
	 * pointer.  Zero out the height and width to collapse the area
	 * used by the widget.  Redraw the window only if the widget is
	 * currently visible.
	 */
	winPtr->htPtr->flags |= REQUEST_LAYOUT;
	if (Tk_IsMapped(winPtr->tkwin) && (winPtr->flags & WIDGET_VISIBLE)) {
	    EventuallyRedraw(htPtr);
	}
	Tk_DeleteEventHandler(winPtr->tkwin, StructureNotifyMask,
	    EmbeddedWidgetEventProc, winPtr);
	hPtr = Blt_FindHashEntry(&(htPtr->widgetTable), (char *)winPtr->tkwin);
	Blt_DeleteHashEntry(&(htPtr->widgetTable), hPtr);
	winPtr->cavityWidth = winPtr->cavityHeight = 0;
	winPtr->tkwin = NULL;

    } else if (eventPtr->type == ConfigureNotify) {
	/*
	 * EmbeddedWidgets can't request new positions. Worry only about resizing.
	 */
	if (winPtr->winWidth != Tk_Width(winPtr->tkwin) ||
	    winPtr->winHeight != Tk_Height(winPtr->tkwin)) {
	    EventuallyRedraw(htPtr);
	    htPtr->flags |= REQUEST_LAYOUT;
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * EmbeddedWidgetCustodyProc --
 *
 *	This procedure is invoked when a embedded widget has been
 *	stolen by another geometry manager.  The information and
 *	memory associated with the embedded widget is released.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the widget formerly associated with the widget
 *	to have its layout re-computed and arranged at the
 *	next idle point.
 *
 *--------------------------------------------------------------
 */
 /* ARGSUSED */
static void
EmbeddedWidgetCustodyProc(clientData, tkwin)
    ClientData clientData;	/* Information about the former embedded widget. */
    Tk_Window tkwin;		/* Not used. */
{
    Blt_HashEntry *hPtr;
    EmbeddedWidget *winPtr = clientData;
    /*
     * Mark the widget as deleted by dereferencing the Tk window
     * pointer.  Zero out the height and width to collapse the area
     * used by the widget.  Redraw the window only if the widget is
     * currently visible.
     */
    winPtr->htPtr->flags |= REQUEST_LAYOUT;
    if (Tk_IsMapped(winPtr->tkwin) && (winPtr->flags & WIDGET_VISIBLE)) {
	EventuallyRedraw(winPtr->htPtr);
    }
    Tk_DeleteEventHandler(winPtr->tkwin, StructureNotifyMask,
	EmbeddedWidgetEventProc, winPtr);
    hPtr = Blt_FindHashEntry(&(winPtr->htPtr->widgetTable), 
			     (char *)winPtr->tkwin);
    Blt_DeleteHashEntry(&(winPtr->htPtr->widgetTable), hPtr);
    winPtr->cavityWidth = winPtr->cavityHeight = 0;
    winPtr->tkwin = NULL;
}

/*
 *--------------------------------------------------------------
 *
 * EmbeddedWidgetGeometryProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for
 *	embedded widgets managed by the hypertext widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for tkwin, and all its managed siblings, to
 *	be repacked and drawn at the next idle point.
 *
 *--------------------------------------------------------------
 */
 /* ARGSUSED */
static void
EmbeddedWidgetGeometryProc(clientData, tkwin)
    ClientData clientData;	/* Information about window that got new
			         * preferred geometry.  */
    Tk_Window tkwin;		/* Other Tk-related information about the
			         * window. */
{
    EmbeddedWidget *winPtr = clientData;

    winPtr->htPtr->flags |= REQUEST_LAYOUT;
    EventuallyRedraw(winPtr->htPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * FindEmbeddedWidget --
 *
 *	Searches for a widget matching the path name given
 *	If found, the pointer to the widget structure is returned,
 *	otherwise NULL.
 *
 * Results:
 *	The pointer to the widget structure. If not found, NULL.
 *
 * ----------------------------------------------------------------------
 */
static EmbeddedWidget *
FindEmbeddedWidget(htPtr, tkwin)
    HText *htPtr;		/* Hypertext widget structure */
    Tk_Window tkwin;		/* Path name of embedded widget  */
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&(htPtr->widgetTable), (char *)tkwin);
    if (hPtr != NULL) {
	return (EmbeddedWidget *) Blt_GetHashValue(hPtr);
    }
    return NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * CreateEmbeddedWidget --
 *
 * 	This procedure creates and initializes a new embedded widget
 *	in the hyper text widget.
 *
 * Results:
 *	The return value is a pointer to a structure describing the
 *	new embedded widget.  If an error occurred, then the return 
 *	value is NULL and an error message is left in interp->result.
 *
 * Side effects:
 *	Memory is allocated. EmbeddedWidget window is mapped. 
 *	Callbacks are set up for embedded widget resizes and geometry 
 *	requests.
 *
 * ----------------------------------------------------------------------
 */
static EmbeddedWidget *
CreateEmbeddedWidget(htPtr, name)
    HText *htPtr;		/* Hypertext widget */
    char *name;			/* Name of embedded widget */
{
    EmbeddedWidget *winPtr;
    Tk_Window tkwin;
    Blt_HashEntry *hPtr;
    int isNew;

    tkwin = Tk_NameToWindow(htPtr->interp, name, htPtr->tkwin);
    if (tkwin == NULL) {
	return NULL;
    }
    if (Tk_Parent(tkwin) != htPtr->tkwin) {
	Tcl_AppendResult(htPtr->interp, "parent window of \"", name,
	    "\" must be \"", Tk_PathName(htPtr->tkwin), "\"", (char *)NULL);
	return NULL;
    }
    hPtr = Blt_CreateHashEntry(&(htPtr->widgetTable), (char *)tkwin, &isNew);
    /* Check is the widget is already embedded into this widget */
    if (!isNew) {
	Tcl_AppendResult(htPtr->interp, "\"", name,
	    "\" is already appended to ", Tk_PathName(htPtr->tkwin),
	    (char *)NULL);
	return NULL;
    }
    winPtr = Blt_Calloc(1, sizeof(EmbeddedWidget));
    assert(winPtr);
    winPtr->flags = 0;
    winPtr->tkwin = tkwin;
    winPtr->htPtr = htPtr;
    winPtr->x = winPtr->y = 0;
    winPtr->fill = FILL_NONE;
    winPtr->justify = JUSTIFY_CENTER;
    winPtr->anchor = TK_ANCHOR_CENTER;
    Blt_SetHashValue(hPtr, winPtr);

    Tk_ManageGeometry(tkwin, &htextMgrInfo, winPtr);
    Tk_CreateEventHandler(tkwin, StructureNotifyMask, EmbeddedWidgetEventProc,
	  winPtr);
    return winPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyEmbeddedWidget --
 *
 * 	This procedure is invoked by DestroyLine to clean up the
 * 	internal structure of a widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the widget is freed up.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyEmbeddedWidget(winPtr)
    EmbeddedWidget *winPtr;
{
    /* Destroy the embedded widget if it still exists */
    if (winPtr->tkwin != NULL) {
	Blt_HashEntry *hPtr;

	Tk_DeleteEventHandler(winPtr->tkwin, StructureNotifyMask,
	    EmbeddedWidgetEventProc, winPtr);
	hPtr = Blt_FindHashEntry(&(winPtr->htPtr->widgetTable),
	    (char *)winPtr->tkwin);
	Blt_DeleteHashEntry(&(winPtr->htPtr->widgetTable), hPtr);
	Tk_DestroyWindow(winPtr->tkwin);
    }
    Blt_Free(winPtr);
}

/* Line Procedures */
/*
 * ----------------------------------------------------------------------
 *
 * CreateLine --
 *
 * 	This procedure creates and initializes a new line of text.
 *
 * Results:
 *	The return value is a pointer to a structure describing the new
 * 	line of text.  If an error occurred, then the return value is NULL
 *	and an error message is left in interp->result.
 *
 * Side effects:
 *	Memory is allocated.
 *
 * ----------------------------------------------------------------------
 */
static Line *
CreateLine(htPtr)
    HText *htPtr;
{
    Line *linePtr;

    if (htPtr->nLines >= htPtr->arraySize) {
	if (htPtr->arraySize == 0) {
	    htPtr->arraySize = DEF_LINES_ALLOC;
	} else {
	    htPtr->arraySize += htPtr->arraySize;
	}
	if (ResizeArray((char **)&(htPtr->lineArr), sizeof(Line),
		htPtr->arraySize, htPtr->nLines) != TCL_OK) {
	    return NULL;
	}
    }
    /* Initialize values in the new entry */

    linePtr = htPtr->lineArr + htPtr->nLines;
    linePtr->offset = 0;
    linePtr->height = linePtr->width = 0;
    linePtr->textStart = 0;
    linePtr->textEnd = -1;
    linePtr->baseline = 0;
    linePtr->chainPtr = Blt_ChainCreate();

    htPtr->nLines++;
    return linePtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyLine --
 *
 * 	This procedure is invoked to clean up the internal structure
 *	of a line.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the line (text and widgets) is
 *	freed up.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyLine(linePtr)
    Line *linePtr;
{
    Blt_ChainLink *linkPtr;
    EmbeddedWidget *winPtr;

    /* Free the list of embedded widget structures */
    for (linkPtr = Blt_ChainFirstLink(linePtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	winPtr = Blt_ChainGetValue(linkPtr);
	DestroyEmbeddedWidget(winPtr);
    }
    Blt_ChainDestroy(linePtr->chainPtr);
}

static void
FreeText(htPtr)
    HText *htPtr;
{
    int i;

    for (i = 0; i < htPtr->nLines; i++) {
	DestroyLine(htPtr->lineArr + i);
    }
    htPtr->nLines = 0;
    htPtr->nChars = 0;
    if (htPtr->charArr != NULL) {
	Blt_Free(htPtr->charArr);
	htPtr->charArr = NULL;
    }
}

/* Text Procedures */
/*
 * ----------------------------------------------------------------------
 *
 * DestroyText --
 *
 * 	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a HText at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the widget is freed up.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyText(dataPtr)
    DestroyData dataPtr;	/* Info about hypertext widget. */
{
    HText *htPtr = (HText *)dataPtr;

    Tk_FreeOptions(configSpecs, (char *)htPtr, htPtr->display, 0);
    if (htPtr->drawGC != NULL) {
	Tk_FreeGC(htPtr->display, htPtr->drawGC);
    }
    if (htPtr->fillGC != NULL) {
	Tk_FreeGC(htPtr->display, htPtr->fillGC);
    }
    if (htPtr->tile != NULL) {
	Blt_FreeTile(htPtr->tile);
    }
    if (htPtr->selectGC != NULL) {
	Tk_FreeGC(htPtr->display, htPtr->selectGC);
    }
    FreeText(htPtr);
    if (htPtr->lineArr != NULL) {
	Blt_Free(htPtr->lineArr);
    }
    Blt_DeleteHashTable(&(htPtr->widgetTable));
    Blt_Free(htPtr);
}

/*
 * --------------------------------------------------------------
 *
 * TextEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on hypertext widgets.
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
TextEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    HText *htPtr = clientData;

    if (eventPtr->type == ConfigureNotify) {
	if ((htPtr->lastWidth != Tk_Width(htPtr->tkwin)) ||
	    (htPtr->lastHeight != Tk_Height(htPtr->tkwin))) {
	    htPtr->flags |= (REQUEST_LAYOUT | TEXT_DIRTY);
	    EventuallyRedraw(htPtr);
	}
    } else if (eventPtr->type == Expose) {

	/*
	 * If the Expose event was synthetic (i.e. we manufactured it
	 * ourselves during a redraw operation), toggle the bit flag
	 * which controls redraws.
	 */

	if (eventPtr->xexpose.send_event) {
	    htPtr->flags ^= IGNORE_EXPOSURES;
	    return;
	}
	if ((eventPtr->xexpose.count == 0) &&
	    !(htPtr->flags & IGNORE_EXPOSURES)) {
	    htPtr->flags |= TEXT_DIRTY;
	    EventuallyRedraw(htPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (htPtr->tkwin != NULL) {
	    htPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(htPtr->interp, htPtr->cmdToken);
	}
	if (htPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayText, htPtr);
	}
	Tcl_EventuallyFree(htPtr, DestroyText);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TextDeleteCmdProc --
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
TextDeleteCmdProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    HText *htPtr = clientData;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (htPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = htPtr->tkwin;
	htPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
#ifdef ITCL_NAMESPACES
	Itk_SetWidgetCommand(tkwin, (Tcl_Command) NULL);
#endif /* ITCL_NAMESPACES */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TileChangedProc
 *
 *	Stub for image change notifications.  Since we immediately draw
 *	the image into a pixmap, we don't care about image changes.
 *
 *	It would be better if Tk checked for NULL proc pointers.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
TileChangedProc(clientData, tile)
    ClientData clientData;
    Blt_Tile tile;		/* Not used. */
{
    HText *htPtr = clientData;

    if (htPtr->tkwin != NULL) {
	EventuallyRedraw(htPtr);
    }
}

/* Configuration Procedures */
static void
ResetTextInfo(htPtr)
    HText *htPtr;
{
    htPtr->first = 0;
    htPtr->last = htPtr->nLines - 1;
    htPtr->selFirst = htPtr->selLast = -1;
    htPtr->selAnchor = 0;
    htPtr->pendingX = htPtr->pendingY = 0;
    htPtr->worldWidth = htPtr->worldHeight = 0;
    htPtr->xOffset = htPtr->yOffset = 0;
}

static Line *
GetLastLine(htPtr)
    HText *htPtr;
{
    if (htPtr->nLines == 0) {
	return CreateLine(htPtr);
    }
    return (htPtr->lineArr + (htPtr->nLines - 1));
}

/*
 * ----------------------------------------------------------------------
 *
 * ReadNamedFile --
 *
 * 	Read the named file into a newly allocated buffer.
 *
 * Results:
 *	Returns the size of the allocated buffer if the file was
 *	read correctly.  Otherwise -1 is returned and "interp->result"
 *	will contain an error message.
 *
 * Side Effects:
 *	If successful, the contents of "bufferPtr" will point
 *	to the allocated buffer.
 *
 * ----------------------------------------------------------------------
 */
static int
ReadNamedFile(interp, fileName, bufferPtr)
    Tcl_Interp *interp;
    char *fileName;
    char **bufferPtr;
{
    FILE *f;
    int nRead, fileSize;
    int count, bytesLeft;
    char *buffer;
#if defined(_MSC_VER) || defined(__BORLANDC__)
#define fstat	 _fstat
#define stat	 _stat
#ifdef _MSC_VER
#define fileno	 _fileno
#endif
#endif /* _MSC_VER || __BORLANDC__ */

    struct stat fileInfo;

    f = fopen(fileName, "r");
    if (f == NULL) {
	Tcl_AppendResult(interp, "can't open \"", fileName,
	    "\" for reading: ", Tcl_PosixError(interp), (char *)NULL);
	return -1;
    }
    if (fstat(fileno(f), &fileInfo) < 0) {
	Tcl_AppendResult(interp, "can't stat \"", fileName, "\": ",
	    Tcl_PosixError(interp), (char *)NULL);
	fclose(f);
	return -1;
    }
    fileSize = fileInfo.st_size + 1;
    buffer = Blt_Malloc(sizeof(char) * fileSize);
    if (buffer == NULL) {
	fclose(f);
	return -1;		/* Can't allocate memory for file buffer */
    }
    count = 0;
    for (bytesLeft = fileInfo.st_size; bytesLeft > 0; bytesLeft -= nRead) {
	nRead = fread(buffer + count, sizeof(char), bytesLeft, f);
	if (nRead < 0) {
	    Tcl_AppendResult(interp, "error reading \"", fileName, "\": ",
		Tcl_PosixError(interp), (char *)NULL);
	    fclose(f);
	    Blt_Free(buffer);
	    return -1;
	} else if (nRead == 0) {
	    break;
	}
	count += nRead;
    }
    fclose(f);
    buffer[count] = '\0';
    *bufferPtr = buffer;
    return count;
}

/*
 * ----------------------------------------------------------------------
 *
 * CollectCommand --
 *
 * 	Collect the characters representing a Tcl command into a
 *	given buffer.
 *
 * Results:
 *	Returns the number of bytes examined.  If an error occurred,
 *	-1 is returned and "interp->result" will contain an error
 *	message.
 *
 * Side Effects:
 *	If successful, the "cmdArr" will be filled with the string
 *	representing the Tcl command.
 *
 * ----------------------------------------------------------------------
 */

static int
CollectCommand(htPtr, inputArr, maxBytes, cmdArr)
    HText *htPtr;		/* Widget record */
    char inputArr[];		/* Array of bytes representing the htext input */
    int maxBytes;		/* Maximum number of bytes left in input */
    char cmdArr[];		/* Output buffer to be filled with the Tcl
				 * command */
{
    int c;
    int i;
    int state, count;

    /* Simply collect the all the characters until %% into a buffer */

    state = count = 0;
    for (i = 0; i < maxBytes; i++) {
	c = inputArr[i];
	if (c == htPtr->specChar) {
	    state++;
	} else if ((state == 0) && (c == '\\')) {
	    state = 3;
	} else {
	    state = 0;
	}
	switch (state) {
	case 2:		/* End of command block found */
	    cmdArr[count - 1] = '\0';
	    return i;

	case 4:		/* Escaped block designator */
	    cmdArr[count] = c;
	    state = 0;
	    break;

	default:		/* Add to command buffer */
	    cmdArr[count++] = c;
	    break;
	}
    }
    Tcl_AppendResult(htPtr->interp, "premature end of TCL command block",
	(char *)NULL);
    return -1;
}

/*
 * ----------------------------------------------------------------------
 *
 * ParseInput --
 *
 * 	Parse the input to the HText structure into an array of lines.
 *	Each entry contains the beginning index and end index of the
 *	characters in the text array which comprise the line.
 *
 *	|*|*|*|\n|T|h|i|s| |a| |l|i|n|e| |o|f| |t|e|x|t|.|\n|*|*|*|
 *                ^					  ^
 *	          textStart				  textEnd
 *
 *	Note that the end index contains the '\n'.
 *
 * Results:
 *	Returns TCL_OK or error depending if the file was read correctly.
 *
 * ----------------------------------------------------------------------
 */
static int
ParseInput(interp, htPtr, input, nBytes)
    Tcl_Interp *interp;
    HText *htPtr;
    char input[];
    int nBytes;
{
    int c;
    int i;
    char *textArr;
    char *cmdArr;
    int count, nLines;
    int length;
    int state;
    Line *linePtr;

    linePtr = CreateLine(htPtr);
    if (linePtr == NULL) {
	return TCL_ERROR;	/* Error allocating the line structure */
    }
    /*  Right now, we replace the text array instead of appending to it */

    linePtr->textStart = 0;

    /* In the worst case, assume the entire input could be Tcl commands */
    cmdArr = Blt_Malloc(sizeof(char) * (nBytes + 1));

    textArr = Blt_Malloc(sizeof(char) * (nBytes + 1));
    if (htPtr->charArr != NULL) {
	Blt_Free(htPtr->charArr);
    }
    htPtr->charArr = textArr;
    htPtr->nChars = 0;

    nLines = count = state = 0;
    htPtr->flags &= ~WIDGET_APPENDED;

    for (i = 0; i < nBytes; i++) {
	c = input[i];
	if (c == htPtr->specChar) {
	    state++;
	} else if (c == '\n') {
	    state = -1;
	} else if ((state == 0) && (c == '\\')) {
	    state = 3;
	} else {
	    state = 0;
	}
	switch (state) {
	case 2:		/* Block of Tcl commands found */
	    count--, i++;
	    length = CollectCommand(htPtr, input + i, nBytes - i, cmdArr);
	    if (length < 0) {
		goto error;
	    }
	    i += length;
	    linePtr->textEnd = count;
	    htPtr->nChars = count + 1;
	    if (Tcl_Eval(interp, cmdArr) != TCL_OK) {
		goto error;
	    }
	    if (htPtr->flags & WIDGET_APPENDED) {
		/* Indicates the location a embedded widget in the text array */
		textArr[count++] = ' ';
		htPtr->flags &= ~WIDGET_APPENDED;
	    }
	    state = 0;
	    break;

	case 4:		/* Escaped block designator */
	    textArr[count - 1] = c;
	    state = 0;
	    break;

	case -1:		/* End of line or input */
	    linePtr->textEnd = count;
	    textArr[count++] = '\n';
	    nLines++;
	    linePtr = CreateLine(htPtr);
	    if (linePtr == NULL) {
		goto error;
	    }
	    linePtr->textStart = count;
	    state = 0;
	    break;

	default:		/* Default action, add to text buffer */
	    textArr[count++] = c;
	    break;
	}
    }
    if (count > linePtr->textStart) {
	linePtr->textEnd = count;
	textArr[count++] = '\n';/* Every line must end with a '\n' */
	nLines++;
    }
    Blt_Free(cmdArr);
    /* Reset number of lines allocated */
    if (ResizeArray((char **)&(htPtr->lineArr), sizeof(Line), nLines,
	    htPtr->arraySize) != TCL_OK) {
	Tcl_AppendResult(interp, "can't reallocate array of lines", (char *)NULL);
	return TCL_ERROR;
    }
    htPtr->nLines = htPtr->arraySize = nLines;
    /*  and the size of the character array */
    if (ResizeArray(&(htPtr->charArr), sizeof(char), count,
	    nBytes) != TCL_OK) {
	Tcl_AppendResult(interp, "can't reallocate text character buffer",
	    (char *)NULL);
	return TCL_ERROR;
    }
    htPtr->nChars = count;
    return TCL_OK;
  error:
    Blt_Free(cmdArr);
    return TCL_ERROR;
}

static int
IncludeText(interp, htPtr, fileName)
    Tcl_Interp *interp;
    HText *htPtr;
    char *fileName;
{
    char *buffer;
    int result;
    int nBytes;

    if ((htPtr->text == NULL) && (fileName == NULL)) {
	return TCL_OK;		/* Empty text string */
    }
    if (fileName != NULL) {
	nBytes = ReadNamedFile(interp, fileName, &buffer);
	if (nBytes < 0) {
	    return TCL_ERROR;
	}
    } else {
	buffer = htPtr->text;
	nBytes = strlen(htPtr->text);
    }
    result = ParseInput(interp, htPtr, buffer, nBytes);
    if (fileName != NULL) {
	Blt_Free(buffer);
    }
    return result;
}

/* ARGSUSED */
static char *
TextVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about widget. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    HText *htPtr = clientData;
    HText *lasthtPtr;

    /* Check to see of this is the most recent trace */
    lasthtPtr = (HText *)Tcl_VarTraceInfo2(interp, name1, name2, flags,
	TextVarProc, NULL);
    if (lasthtPtr != htPtr) {
	return NULL;		/* Ignore all but most current trace */
    }
    if (flags & TCL_TRACE_READS) {
	char c;

	c = name2[0];
	if ((c == 'w') && (strcmp(name2, "widget") == 0)) {
	    Tcl_SetVar2(interp, name1, name2, Tk_PathName(htPtr->tkwin),
		flags);
	} else if ((c == 'l') && (strcmp(name2, "line") == 0)) {
	    char buf[80];
	    int lineNum;

	    lineNum = htPtr->nLines - 1;
	    if (lineNum < 0) {
		lineNum = 0;
	    }
	    sprintf(buf, "%d", lineNum);
	    Tcl_SetVar2(interp, name1, name2, buf, flags);
	} else if ((c == 'i') && (strcmp(name2, "index") == 0)) {
	    char buf[80];

	    sprintf(buf, "%d", htPtr->nChars - 1);
	    Tcl_SetVar2(interp, name1, name2, buf, flags);
	} else if ((c == 'f') && (strcmp(name2, "file") == 0)) {
	    char *fileName;

	    fileName = htPtr->fileName;
	    if (fileName == NULL) {
		fileName = "";
	    }
	    Tcl_SetVar2(interp, name1, name2, fileName, flags);
	} else {
	    return "?unknown?";
	}
    }
    return NULL;
}

static char *varNames[] =
{
    "widget", "line", "file", "index", (char *)NULL
};

static void
CreateTraces(htPtr)
    HText *htPtr;
{
    char **ptr;
    static char globalCmd[] = "global htext";

    /*
     * Make the traced variables global to the widget
     */
    Tcl_Eval(htPtr->interp, globalCmd);
    for (ptr = varNames; *ptr != NULL; ptr++) {
	Tcl_TraceVar2(htPtr->interp, "htext", *ptr,
	    (TCL_GLOBAL_ONLY | TCL_TRACE_READS), TextVarProc, htPtr);
    }
}

static void
DeleteTraces(htPtr)
    HText *htPtr;
{
    char **ptr;

    for (ptr = varNames; *ptr != NULL; ptr++) {
	Tcl_UntraceVar2(htPtr->interp, "htext", *ptr,
	    (TCL_GLOBAL_ONLY | TCL_TRACE_READS), TextVarProc, htPtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureText --
 *
 * 	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	a hypertext widget.
 *
 * 	The layout of the text must be calculated (by ComputeLayout)
 *	whenever particular options change; -font, -file, -linespacing
 *	and -text options. If the user has changes one of these options,
 *	it must be detected so that the layout can be recomputed. Since the
 *	coordinates of the layout are virtual, there is no need to adjust
 *	them if physical window attributes (window size, etc.)
 *	change.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 * 	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 * 	etc. get set for htPtr;  old resources get freed, if there were any.
 * 	The hypertext is redisplayed.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureText(interp, htPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    HText *htPtr;		/* Information about widget; may or may not
			         * already have values for some fields. */
{
    XGCValues gcValues;
    unsigned long gcMask;
    GC newGC;

    if (Blt_ConfigModified(configSpecs, "-font", "-linespacing", "-file",
	    "-text", "-width", "-height", (char *)NULL)) {
	/*
	 * These options change the layout of the text.  Width/height
	 * and rows/columns may change a relatively sized window or cavity.
	 */
	htPtr->flags |= (REQUEST_LAYOUT | TEXT_DIRTY);	/* Mark for update */
    }
    gcMask = GCForeground | GCFont;
    gcValues.font = Tk_FontId(htPtr->font);
    gcValues.foreground = htPtr->normalFg->pixel;
    newGC = Tk_GetGC(htPtr->tkwin, gcMask, &gcValues);
    if (htPtr->drawGC != NULL) {
	Tk_FreeGC(htPtr->display, htPtr->drawGC);
    }
    htPtr->drawGC = newGC;

    gcValues.foreground = htPtr->selFgColor->pixel;
    newGC = Tk_GetGC(htPtr->tkwin, gcMask, &gcValues);
    if (htPtr->selectGC != NULL) {
	Tk_FreeGC(htPtr->display, htPtr->selectGC);
    }
    htPtr->selectGC = newGC;

    if (htPtr->xScrollUnits < 1) {
	htPtr->xScrollUnits = 1;
    }
    if (htPtr->yScrollUnits < 1) {
	htPtr->yScrollUnits = 1;
    }
    if (htPtr->tile != NULL) {
	Blt_SetTileChangedProc(htPtr->tile, TileChangedProc, htPtr);
    }
    gcValues.foreground = htPtr->normalBg->pixel;
    newGC = Tk_GetGC(htPtr->tkwin, gcMask, &gcValues);
    if (htPtr->fillGC != NULL) {
	Tk_FreeGC(htPtr->display, htPtr->fillGC);
    }
    htPtr->fillGC = newGC;

    if (htPtr->nColumns > 0) {
	htPtr->reqWidth =
	    htPtr->nColumns * Tk_TextWidth(htPtr->font, "0", 1);
    }
    if (htPtr->nRows > 0) {
	Tk_FontMetrics fontMetrics;

	Tk_GetFontMetrics(htPtr->font, &fontMetrics);
	htPtr->reqHeight = htPtr->nRows * fontMetrics.linespace;
    }
    /*
     * If the either the -text or -file option changed, read in the
     * new text.  The -text option supersedes any -file option.
     */
    if (Blt_ConfigModified(configSpecs, "-file", "-text", (char *)NULL)) {
	int result;

	FreeText(htPtr);
	CreateTraces(htPtr);	/* Create variable traces */

	result = IncludeText(interp, htPtr, htPtr->fileName);

	DeleteTraces(htPtr);
	if (result == TCL_ERROR) {
	    FreeText(htPtr);
	    return TCL_ERROR;
	}
	ResetTextInfo(htPtr);
    }
    EventuallyRedraw(htPtr);
    return TCL_OK;
}

/* Layout Procedures */
/*
 * -----------------------------------------------------------------
 *
 * TranslateAnchor --
 *
 * 	Translate the coordinates of a given bounding box based
 *	upon the anchor specified.  The anchor indicates where
 *	the given xy position is in relation to the bounding box.
 *
 *  		nw --- n --- ne
 *  		|            |     x,y ---+
 *  		w   center   e      |     |
 *  		|            |      +-----+
 *  		sw --- s --- se
 *
 * Results:
 *	The translated coordinates of the bounding box are returned.
 *
 * -----------------------------------------------------------------
 */
static XPoint
TranslateAnchor(deltaX, deltaY, anchor)
    int deltaX, deltaY;		/* Difference between outer and inner regions
				 */
    Tk_Anchor anchor;		/* Direction of the anchor */
{
    XPoint point;

    point.x = point.y = 0;
    switch (anchor) {
    case TK_ANCHOR_NW:		/* Upper left corner */
	break;
    case TK_ANCHOR_W:		/* Left center */
	point.y = (deltaY / 2);
	break;
    case TK_ANCHOR_SW:		/* Lower left corner */
	point.y = deltaY;
	break;
    case TK_ANCHOR_N:		/* Top center */
	point.x = (deltaX / 2);
	break;
    case TK_ANCHOR_CENTER:	/* Centered */
	point.x = (deltaX / 2);
	point.y = (deltaY / 2);
	break;
    case TK_ANCHOR_S:		/* Bottom center */
	point.x = (deltaX / 2);
	point.y = deltaY;
	break;
    case TK_ANCHOR_NE:		/* Upper right corner */
	point.x = deltaX;
	break;
    case TK_ANCHOR_E:		/* Right center */
	point.x = deltaX;
	point.y = (deltaY / 2);
	break;
    case TK_ANCHOR_SE:		/* Lower right corner */
	point.x = deltaX;
	point.y = deltaY;
	break;
    }
    return point;
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeCavitySize --
 *
 *	Sets the width and height of the cavity based upon the
 *	requested size of the embedded widget.  The requested space also
 *	includes any external padding which has been designated for
 *	this window.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The size of the cavity is set in the embedded widget information
 *	structure.  These values can effect how the embedded widget is
 *	packed into the master window.
 *
 *----------------------------------------------------------------------
 */
static void
ComputeCavitySize(winPtr)
    EmbeddedWidget *winPtr;
{
    int width, height;
    int twiceBW;

    twiceBW = 2 * Tk_Changes(winPtr->tkwin)->border_width;
    if (winPtr->reqCavityWidth > 0) {
	width = winPtr->reqCavityWidth;
    } else if (winPtr->relCavityWidth > 0.0) {
	width = (int)((double)Tk_Width(winPtr->htPtr->tkwin) *
	    winPtr->relCavityWidth + 0.5);
    } else {
	width = GetEmbeddedWidgetWidth(winPtr) + PADDING(winPtr->padX) + 
	    twiceBW;
    }
    winPtr->cavityWidth = width;

    if (winPtr->reqCavityHeight > 0) {
	height = winPtr->reqCavityHeight;
    } else if (winPtr->relCavityHeight > 0.0) {
	height = (int)((double)Tk_Height(winPtr->htPtr->tkwin) *
	    winPtr->relCavityHeight + 0.5);
    } else {
	height = GetEmbeddedWidgetHeight(winPtr) + PADDING(winPtr->padY) + 
	    twiceBW;
    }
    winPtr->cavityHeight = height;
}

/*
 *----------------------------------------------------------------------
 *
 * LayoutLine --
 *
 *	This procedure computes the total width and height needed
 *      to contain the text and widgets for a particular line.
 *      It also calculates the baseline of the text on the line with
 *	respect to the other widgets on the line.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
LayoutLine(htPtr, linePtr)
    HText *htPtr;
    Line *linePtr;
{
    EmbeddedWidget *winPtr;
    int textStart, textLength;
    int maxAscent, maxDescent, maxHeight;
    int ascent, descent;
    int median;			/* Difference of font ascent/descent values */
    Blt_ChainLink *linkPtr;
    int x, y;
    int newX;
    Tk_FontMetrics fontMetrics;

    /* Initialize line defaults */
    Tk_GetFontMetrics(htPtr->font, &fontMetrics);
    maxAscent = fontMetrics.ascent;
    maxDescent = fontMetrics.descent;
    median = fontMetrics.ascent - fontMetrics.descent;
    ascent = descent = 0;	/* Suppress compiler warnings */

    /*
     * Pass 1: Determine the maximum ascent (baseline) and descent
     * needed for the line.  We'll need this for figuring the top,
     * bottom, and center anchors.
     */
    for (linkPtr = Blt_ChainFirstLink(linePtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	winPtr = Blt_ChainGetValue(linkPtr);
	if (winPtr->tkwin == NULL) {
	    continue;
	}
	ComputeCavitySize(winPtr);

	switch (winPtr->justify) {
	case JUSTIFY_TOP:
	    ascent = fontMetrics.ascent + winPtr->padTop;
	    descent = winPtr->cavityHeight - fontMetrics.ascent;
	    break;
	case JUSTIFY_CENTER:
	    ascent = (winPtr->cavityHeight + median) / 2;
	    descent = (winPtr->cavityHeight - median) / 2;
	    break;
	case JUSTIFY_BOTTOM:
	    ascent = winPtr->cavityHeight - fontMetrics.descent;
	    descent = fontMetrics.descent;
	    break;
	}
	if (descent > maxDescent) {
	    maxDescent = descent;
	}
	if (ascent > maxAscent) {
	    maxAscent = ascent;
	}
    }

    maxHeight = maxAscent + maxDescent + htPtr->leader;
    x = 0;			/* Always starts from x=0 */
    y = 0;			/* Suppress compiler warning */
    textStart = linePtr->textStart;

    /*
     * Pass 2: Find the placements of the text and widgets along each
     * line.
     */
    for (linkPtr = Blt_ChainFirstLink(linePtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	winPtr = Blt_ChainGetValue(linkPtr);
	if (winPtr->tkwin == NULL) {
	    continue;
	}
	/* Get the width of the text leading to the widget. */
	textLength = (winPtr->precedingTextEnd - textStart);
	if (textLength > 0) {
	    Tk_MeasureChars(htPtr->font, htPtr->charArr + textStart,
		textLength, 10000, TK_AT_LEAST_ONE, &newX);
	    winPtr->precedingTextWidth = newX;
	    x += newX;
	}
	switch (winPtr->justify) {
	case JUSTIFY_TOP:
	    y = maxAscent - fontMetrics.ascent;
	    break;
	case JUSTIFY_CENTER:
	    y = maxAscent - (winPtr->cavityHeight + median) / 2;
	    break;
	case JUSTIFY_BOTTOM:
	    y = maxAscent + fontMetrics.descent - winPtr->cavityHeight;
	    break;
	}
	winPtr->x = x, winPtr->y = y;

	/* Skip over trailing space */
	textStart = winPtr->precedingTextEnd + 1;

	x += winPtr->cavityWidth;
    }

    /*
     * This can be either the trailing piece of a line after the last widget
     * or the entire line if no widgets are embedded in it.
     */
    textLength = (linePtr->textEnd - textStart) + 1;
    if (textLength > 0) {
	Tk_MeasureChars(htPtr->font, htPtr->charArr + textStart,
	    textLength, 10000, DEF_TEXT_FLAGS, &newX);
	x += newX;
    }
    /* Update line parameters */
    if ((linePtr->width != x) || (linePtr->height != maxHeight) ||
	(linePtr->baseline != maxAscent)) {
	htPtr->flags |= TEXT_DIRTY;
    }
    linePtr->width = x;
    linePtr->height = maxHeight;
    linePtr->baseline = maxAscent;
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeLayout --
 *
 *	This procedure computes the total width and height needed
 *      to contain the text and widgets from all the lines of text.
 *      It merely sums the heights and finds the maximum width of
 *	all the lines.  The width and height are needed for scrolling.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
ComputeLayout(htPtr)
    HText *htPtr;
{
    int count;
    Line *linePtr;
    int height, width;

    width = height = 0;
    for (count = 0; count < htPtr->nLines; count++) {
	linePtr = htPtr->lineArr + count;

	linePtr->offset = height;
	LayoutLine(htPtr, linePtr);
	height += linePtr->height;
	if (linePtr->width > width) {
	    width = linePtr->width;
	}
    }
    /*
     * Set changed flag if new layout changed size of virtual text.
     */
    if ((height != htPtr->worldHeight) || (width != htPtr->worldWidth)) {
	htPtr->worldHeight = height, htPtr->worldWidth = width;
	htPtr->flags |= TEXT_DIRTY;
    }
}

/* Display Procedures */
/*
 * ----------------------------------------------------------------------
 *
 * GetVisibleLines --
 *
 * 	Calculates which lines are visible using the height
 *      of the viewport and y offset from the top of the text.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Only those line between first and last inclusive are
 * 	redrawn.
 *
 * ----------------------------------------------------------------------
 */
static int
GetVisibleLines(htPtr)
    HText *htPtr;
{
    int topLine, bottomLine;
    int firstY, lastY;
    int lastLine;

    if (htPtr->nLines == 0) {
	htPtr->first = 0;
	htPtr->last = -1;
	return TCL_OK;
    }
    firstY = htPtr->pendingY;
    lastLine = htPtr->nLines - 1;

    /* First line */
    topLine = LineSearch(htPtr, firstY, 0, lastLine);
    if (topLine < 0) {
	/*
	 * This can't be. The y-coordinate offset must be corrupted.
	 */
	fprintf(stderr, "internal error: First position not found `%d'\n",
	    firstY);
	return TCL_ERROR;
    }
    htPtr->first = topLine;

    /*
     * If there is less text than window space, the bottom line is the
     * last line of text.  Otherwise search for the line at the bottom
     * of the window.
     */
    lastY = firstY + Tk_Height(htPtr->tkwin) - 1;
    if (lastY < htPtr->worldHeight) {
	bottomLine = LineSearch(htPtr, lastY, topLine, lastLine);
    } else {
	bottomLine = lastLine;
    }
    if (bottomLine < 0) {
	/*
	 * This can't be. The newY offset must be corrupted.
	 */
	fprintf(stderr, "internal error: Last position not found `%d'\n",
	    lastY);
#ifdef notdef
	fprintf(stderr, "worldHeight=%d,height=%d,top=%d,first=%d,last=%d\n",
	    htPtr->worldHeight, Tk_Height(htPtr->tkwin), firstY,
	    htPtr->lineArr[topLine].offset, htPtr->lineArr[lastLine].offset);
#endif
	return TCL_ERROR;
    }
    htPtr->last = bottomLine;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * DrawSegment --
 *
 * 	Draws a line segment, designated by the segment structure.
 *	This routine handles the display of selected text by drawing
 *	a raised 3D border underneath the selected text.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The line segment is drawn on *draw*.
 *
 * ----------------------------------------------------------------------
 */
static void
DrawSegment(htPtr, draw, linePtr, x, y, segPtr)
    HText *htPtr;
    Drawable draw;
    Line *linePtr;
    int x, y;
    Segment *segPtr;
{
    int lastX, curPos, nChars;
    int textLength;
    int selStart, selEnd, selLength;
    Tk_FontMetrics fontMetrics;

#ifdef notdef
    fprintf(stderr, "DS select: first=%d,last=%d text: first=%d,last=%d\n",
	htPtr->selFirst, htPtr->selLast, segPtr->textStart, segPtr->textEnd);
#endif
    textLength = (segPtr->textEnd - segPtr->textStart) + 1;
    if (textLength < 1) {
	return;
    }
    Tk_GetFontMetrics(htPtr->font, &fontMetrics);
    if ((segPtr->textEnd < htPtr->selFirst) ||
	(segPtr->textStart > htPtr->selLast)) {	/* No selected text */
	Tk_DrawChars(htPtr->display, draw, htPtr->drawGC, htPtr->font,
	    htPtr->charArr + segPtr->textStart, textLength - 1,
	    x, y + linePtr->baseline);
	return;
    }
    /*
     *	Text in a segment (with selected text) may have
     *	up to three regions:
     *
     *	1) the text before the start the selection
     *	2) the selected text itself (drawn in a raised border)
     *	3) the text following the selection.
     */

    selStart = segPtr->textStart;
    selEnd = segPtr->textEnd;
    if (htPtr->selFirst > segPtr->textStart) {
	selStart = htPtr->selFirst;
    }
    if (htPtr->selLast < segPtr->textEnd) {
	selEnd = htPtr->selLast;
    }
    selLength = (selEnd - selStart) + 1;
    lastX = x;
    curPos = segPtr->textStart;

    if (selStart > segPtr->textStart) {	/* Text preceding selection */
	nChars = (selStart - segPtr->textStart);
	Tk_MeasureChars(htPtr->font, htPtr->charArr + segPtr->textStart,
	    nChars, 10000, DEF_TEXT_FLAGS, &lastX);
	lastX += x;
	Tk_DrawChars(htPtr->display, draw, htPtr->drawGC, htPtr->font,
	    htPtr->charArr + segPtr->textStart, nChars, x,
	    y + linePtr->baseline);
	curPos = selStart;
    }
    if (selLength > 0) {	/* The selection itself */
	int width, nextX;

	Tk_MeasureChars(htPtr->font, htPtr->charArr + selStart,
	    selLength, 10000, DEF_TEXT_FLAGS, &nextX);
	nextX += x;
	width = (selEnd == linePtr->textEnd)
	    ? htPtr->worldWidth - htPtr->xOffset - lastX :
	    nextX - lastX;
	Blt_Fill3DRectangle(htPtr->tkwin, draw, htPtr->selBorder,
	    lastX, y + linePtr->baseline - fontMetrics.ascent,
	    width, fontMetrics.linespace, htPtr->selBorderWidth,
	    TK_RELIEF_RAISED);
	Tk_DrawChars(htPtr->display, draw, htPtr->selectGC,
	    htPtr->font, htPtr->charArr + selStart, selLength,
	    lastX, y + linePtr->baseline);
	lastX = nextX;
	curPos = selStart + selLength;
    }
    nChars = segPtr->textEnd - curPos;
    if (nChars > 0) {		/* Text following the selection */
	Tk_DrawChars(htPtr->display, draw, htPtr->drawGC, htPtr->font,
	    htPtr->charArr + curPos, nChars - 1, lastX, y + linePtr->baseline);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * MoveEmbeddedWidget --
 *
 * 	Move a embedded widget to a new location in the hypertext
 *	parent window.  If the window has no geometry (i.e. width,
 *	or height is 0), simply unmap to window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Each embedded widget is moved to its new location, generating
 *      Expose events in the parent for each embedded widget moved.
 *
 * ----------------------------------------------------------------------
 */
static void
MoveEmbeddedWidget(winPtr, offset)
    EmbeddedWidget *winPtr;
    int offset;
{
    int winWidth, winHeight;
    int width, height;
    int deltaX, deltaY;
    int x, y;
    int intBW;

    winWidth = GetEmbeddedWidgetWidth(winPtr);
    winHeight = GetEmbeddedWidgetHeight(winPtr);
    if ((winWidth < 1) || (winHeight < 1)) {
	if (Tk_IsMapped(winPtr->tkwin)) {
	    Tk_UnmapWindow(winPtr->tkwin);
	}
	return;
    }
    intBW = Tk_Changes(winPtr->tkwin)->border_width;
    x = (winPtr->x + intBW + winPtr->padLeft) -
	winPtr->htPtr->xOffset;
    y = offset + (winPtr->y + intBW + winPtr->padTop) -
	winPtr->htPtr->yOffset;

    width = winPtr->cavityWidth - (2 * intBW + PADDING(winPtr->padX));
    if (width < 0) {
	width = 0;
    }
    if ((width < winWidth) || (winPtr->fill & FILL_X)) {
	winWidth = width;
    }
    deltaX = width - winWidth;

    height = winPtr->cavityHeight - (2 * intBW + PADDING(winPtr->padY));
    if (height < 0) {
	height = 0;
    }
    if ((height < winHeight) || (winPtr->fill & FILL_Y)) {
	winHeight = height;
    }
    deltaY = height - winHeight;

    if ((deltaX > 0) || (deltaY > 0)) {
	XPoint point;

	point = TranslateAnchor(deltaX, deltaY, winPtr->anchor);
	x += point.x, y += point.y;
    }
    winPtr->winWidth = winWidth;
    winPtr->winHeight = winHeight;

    if ((x != Tk_X(winPtr->tkwin)) || (y != Tk_Y(winPtr->tkwin)) ||
	(winWidth != Tk_Width(winPtr->tkwin)) ||
	(winHeight != Tk_Height(winPtr->tkwin))) {
	Tk_MoveResizeWindow(winPtr->tkwin, x, y, winWidth, winHeight);
    }
    if (!Tk_IsMapped(winPtr->tkwin)) {
	Tk_MapWindow(winPtr->tkwin);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DrawPage --
 *
 * 	This procedure displays the lines of text and moves the widgets
 *      to their new positions.  It draws lines with regard to
 *	the direction of the scrolling.  The idea here is to make the
 *	text and buttons appear to move together. Otherwise you will
 *	get a "jiggling" effect where the windows appear to bump into
 *	the next line before that line is moved.  In the worst case, where
 *	every line has at least one widget, you can get an aquarium effect
 *      (lines appear to ripple up).
 *
 * 	The text area may start between line boundaries (to accommodate
 *	both variable height lines and constant scrolling). Subtract the
 *	difference of the page offset and the line offset from the starting
 *	coordinates. For horizontal scrolling, simply subtract the offset
 *	of the viewport. The window will clip the top of the first line,
 *	the bottom of the last line, whatever text extends to the left
 *	or right of the viewport on any line.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the line in its current
 * 	mode.
 *
 * ----------------------------------------------------------------------
 */
static void
DrawPage(htPtr, deltaY)
    HText *htPtr;
    int deltaY;			/* Change from previous Y coordinate */
{
    Line *linePtr;
    EmbeddedWidget *winPtr;
    Tk_Window tkwin = htPtr->tkwin;
    Segment sgmt;
    Pixmap pixmap;
    int forceCopy;
    int i;
    int lineNum;
    int x, y, lastY;
    Blt_ChainLink *linkPtr;
    int width, height;
    Display *display;

    display = htPtr->display;
    width = Tk_Width(tkwin);
    height = Tk_Height(tkwin);

    /* Create an off-screen pixmap for semi-smooth scrolling. */
    pixmap = Tk_GetPixmap(display, Tk_WindowId(tkwin), width, height,
	  Tk_Depth(tkwin));

    x = -(htPtr->xOffset);
    y = -(htPtr->yOffset);

    if (htPtr->tile != NULL) {
	if (htPtr->tileOffsetPage) {
	    Blt_SetTSOrigin(htPtr->tkwin, htPtr->tile, x, y);
	} else {
	    Blt_SetTileOrigin(htPtr->tkwin, htPtr->tile, 0, 0);
	}
	Blt_TileRectangle(htPtr->tkwin, pixmap, htPtr->tile, 0, 0, width, 
		height);
    } else {
	XFillRectangle(display, pixmap, htPtr->fillGC, 0, 0, width, height);
    }


    if (deltaY >= 0) {
	y += htPtr->lineArr[htPtr->first].offset;
	lineNum = htPtr->first;
	lastY = 0;
    } else {
	y += htPtr->lineArr[htPtr->last].offset;
	lineNum = htPtr->last;
	lastY = height;
    }
    forceCopy = 0;

    /* Draw each line */
    for (i = htPtr->first; i <= htPtr->last; i++) {

	/* Initialize character position in text buffer to start */
	linePtr = htPtr->lineArr + lineNum;
	sgmt.textStart = linePtr->textStart;
	sgmt.textEnd = linePtr->textEnd;

	/* Initialize X position */
	x = -(htPtr->xOffset);
	for (linkPtr = Blt_ChainFirstLink(linePtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    winPtr = Blt_ChainGetValue(linkPtr);

	    if (winPtr->tkwin != NULL) {
		winPtr->flags |= WIDGET_VISIBLE;
		MoveEmbeddedWidget(winPtr, linePtr->offset);
	    }
	    sgmt.textEnd = winPtr->precedingTextEnd - 1;
	    if (sgmt.textEnd >= sgmt.textStart) {
		DrawSegment(htPtr, pixmap, linePtr, x, y, &sgmt);
		x += winPtr->precedingTextWidth;
	    }
	    /* Skip over the extra trailing space which designates the widget */
	    sgmt.textStart = winPtr->precedingTextEnd + 1;
	    x += winPtr->cavityWidth;
	    forceCopy++;
	}

	/*
	 * This may be the text trailing the last widget or the entire
	 * line if no widgets occur on it.
	 */
	sgmt.textEnd = linePtr->textEnd;
	if (sgmt.textEnd >= sgmt.textStart) {
	    DrawSegment(htPtr, pixmap, linePtr, x, y, &sgmt);
	}
	/* Go to the top of the next line */
	if (deltaY >= 0) {
	    y += htPtr->lineArr[lineNum].height;
	    lineNum++;
	}
	if ((forceCopy > 0) && !(htPtr->flags & TEXT_DIRTY)) {
	    if (deltaY >= 0) {
		XCopyArea(display, pixmap, Tk_WindowId(tkwin), htPtr->drawGC,
			  0, lastY, width, y - lastY, 0, lastY);
	    } else {
		XCopyArea(display, pixmap, Tk_WindowId(tkwin), htPtr->drawGC,
			  0, y, width, lastY - y, 0, y);
	    }
	    forceCopy = 0;	/* Reset drawing flag */
	    lastY = y;		/* Record last Y position */
	}
	if ((deltaY < 0) && (lineNum > 0)) {
	    --lineNum;
	    y -= htPtr->lineArr[lineNum].height;
	}
    }
    /*
     * If the viewport was resized, draw the page in one operation.
     * Otherwise draw any left-over block of text (either at the top
     * or bottom of the page)
     */
    if (htPtr->flags & TEXT_DIRTY) {
	XCopyArea(display, pixmap, Tk_WindowId(tkwin),
	    htPtr->drawGC, 0, 0, width, height, 0, 0);
    } else if (lastY != y) {
	if (deltaY >= 0) {
	    height -= lastY;
	    XCopyArea(display, pixmap, Tk_WindowId(tkwin),
		htPtr->drawGC, 0, lastY, width, height, 0, lastY);
	} else {
	    height = lastY;
	    XCopyArea(display, pixmap, Tk_WindowId(tkwin),
		htPtr->drawGC, 0, 0, width, height, 0, 0);
	}
    }
    Tk_FreePixmap(display, pixmap);
}


static void
SendBogusEvent(tkwin)
    Tk_Window tkwin;
{
#define DONTPROPAGATE 0
    XEvent event;

    event.type = event.xexpose.type = Expose;
    event.xexpose.window = Tk_WindowId(tkwin);
    event.xexpose.display = Tk_Display(tkwin);
    event.xexpose.count = 0;
    event.xexpose.x = event.xexpose.y = 0;
    event.xexpose.width = Tk_Width(tkwin);
    event.xexpose.height = Tk_Height(tkwin);

    XSendEvent(Tk_Display(tkwin), Tk_WindowId(tkwin), DONTPROPAGATE,
	ExposureMask, &event);
}

/*
 * ----------------------------------------------------------------------
 *
 * DisplayText --
 *
 * 	This procedure is invoked to display a hypertext widget.
 *	Many of the operations which might ordinarily be performed
 *	elsewhere (e.g. in a configuration routine) are done here
 *	because of the somewhat unusual interactions occurring between
 *	the parent and embedded widgets.
 *
 *      Recompute the layout of the text if necessary. This is
 *	necessary if the world coordinate system has changed.
 *	Specifically, the following may have occurred:
 *
 *	  1.  a text attribute has changed (font, linespacing, etc.).
 *	  2.  widget option changed (anchor, width, height).
 *        3.  actual embedded widget was resized.
 *	  4.  new text string or file.
 *
 *      This is deferred to the display routine since potentially
 *      many of these may occur (especially embedded widget changes).
 *
 *	Set the vertical and horizontal scrollbars (if they are
 *	designated) by issuing a Tcl command.  Done here since
 *	the text window width and height are needed.
 *
 *	If the viewport position or contents have changed in the
 *	vertical direction,  the now out-of-view embedded widgets
 *	must be moved off the viewport.  Since embedded widgets will
 *	obscure the text window, it is imperative that the widgets
 *	are moved off before we try to redraw text in the same area.
 *      This is necessary only for vertical movements.  Horizontal
 *	embedded widget movements are handled automatically in the
 *	page drawing routine.
 *
 *      Get the new first and last line numbers for the viewport.
 *      These line numbers may have changed because either a)
 *      the viewport changed size or position, or b) the text
 *	(embedded widget sizes or text attributes) have changed.
 *
 *	If the viewport has changed vertically (i.e. the first or
 *      last line numbers have changed), move the now out-of-view
 *	embedded widgets off the viewport.
 *
 *      Potentially many expose events may be generated when the
 *	the individual embedded widgets are moved and/or resized.
 *	These events need to be ignored.  Since (I think) expose
 * 	events are guaranteed to happen in order, we can bracket
 *	them by sending phony events (via XSendEvent). The phony
 *      events turn on and off flags indicating which events
*	should be ignored.
 *
 *	Finally, the page drawing routine is called.
 *
 * Results:
 *	None.
 *
 * Side effects:
 * 	Commands are output to X to display the hypertext in its
 *	current mode.
 *
 * ----------------------------------------------------------------------
 */
static void
DisplayText(clientData)
    ClientData clientData;	/* Information about widget. */
{
    HText *htPtr = clientData;
    Tk_Window tkwin = htPtr->tkwin;
    int oldFirst;		/* First line of old viewport */
    int oldLast;		/* Last line of old viewport */
    int deltaY;			/* Change in viewport in Y direction */
    int reqWidth, reqHeight;

#ifdef notdef
    fprintf(stderr, "calling DisplayText(%s)\n", Tk_PathName(htPtr->tkwin));
#endif
    htPtr->flags &= ~REDRAW_PENDING;
    if (tkwin == NULL) {
	return;			/* Window has been destroyed */
    }
    if (htPtr->flags & REQUEST_LAYOUT) {
	/*
	 * Recompute the layout when widgets are created, deleted,
	 * moved, or resized.  Also when text attributes (such as
	 * font, linespacing) have changed.
	 */
	ComputeLayout(htPtr);
    }
    htPtr->lastWidth = Tk_Width(tkwin);
    htPtr->lastHeight = Tk_Height(tkwin);

    /*
     * Check the requested width and height.  We allow two modes:
     * 	1) If the user requested value is greater than zero, use it.
     *  2) Otherwise, let the window be as big as the virtual text.
     *	   This could be too large to display, so constrain it by
     *	   the maxWidth and maxHeight values.
     *
     * In any event, we need to calculate the size of the virtual
     * text and then make a geometry request.  This is so that widgets
     * whose size is relative to the master, will be set once.
     */
    if (htPtr->reqWidth > 0) {
	reqWidth = htPtr->reqWidth;
    } else {
	reqWidth = MIN(htPtr->worldWidth, htPtr->maxWidth);
	if (reqWidth < 1) {
	    reqWidth = 1;
	}
    }
    if (htPtr->reqHeight > 0) {
	reqHeight = htPtr->reqHeight;
    } else {
	reqHeight = MIN(htPtr->worldHeight, htPtr->maxHeight);
	if (reqHeight < 1) {
	    reqHeight = 1;
	}
    }
    if ((reqWidth != Tk_ReqWidth(tkwin)) || (reqHeight != Tk_ReqHeight(tkwin))) {
	Tk_GeometryRequest(tkwin, reqWidth, reqHeight);

	EventuallyRedraw(htPtr);
	return;			/* Try again with new geometry */
    }
    if (!Tk_IsMapped(tkwin)) {
	return;
    }
    /*
     * Turn off layout requests here, after the text window has been
     * mapped.  Otherwise, relative embedded widget size requests wrt
     * to the size of parent text window will be wrong.
     */
    htPtr->flags &= ~REQUEST_LAYOUT;

    /* Is there a pending goto request? */
    if (htPtr->flags & GOTO_PENDING) {
	htPtr->pendingY = htPtr->lineArr[htPtr->reqLineNum].offset;
	htPtr->flags &= ~GOTO_PENDING;
    }
    deltaY = htPtr->pendingY - htPtr->yOffset;
    oldFirst = htPtr->first, oldLast = htPtr->last;

    /*
     * If the viewport has changed size or position, or the text
     * and/or embedded widgets have changed, adjust the scrollbars to
     * new positions.
     */
    if (htPtr->flags & TEXT_DIRTY) {
	int width, height;

	width = Tk_Width(htPtr->tkwin);
	height = Tk_Height(htPtr->tkwin);

	/* Reset viewport origin and world extents */
	htPtr->xOffset = Blt_AdjustViewport(htPtr->pendingX,
	    htPtr->worldWidth, width,
	    htPtr->xScrollUnits, BLT_SCROLL_MODE_LISTBOX);
	htPtr->yOffset = Blt_AdjustViewport(htPtr->pendingY,
	    htPtr->worldHeight, height,
	    htPtr->yScrollUnits, BLT_SCROLL_MODE_LISTBOX);
	if (htPtr->xScrollCmdPrefix != NULL) {
	    Blt_UpdateScrollbar(htPtr->interp, htPtr->xScrollCmdPrefix,
		(double)htPtr->xOffset / htPtr->worldWidth,
		(double)(htPtr->xOffset + width) / htPtr->worldWidth);
	}
	if (htPtr->yScrollCmdPrefix != NULL) {
	    Blt_UpdateScrollbar(htPtr->interp, htPtr->yScrollCmdPrefix,
		(double)htPtr->yOffset / htPtr->worldHeight,
		(double)(htPtr->yOffset + height) / htPtr->worldHeight);
	}
	/*
	 * Given a new viewport or text height, find the first and
	 * last line numbers of the new viewport.
	 */
	if (GetVisibleLines(htPtr) != TCL_OK) {
	    return;
	}
    }
    /*
     * 	This is a kludge: Send an expose event before and after
     * 	drawing the page of text.  Since moving and resizing of the
     * 	embedded widgets will cause redundant expose events in the parent
     * 	window, the phony events will bracket them indicating no
     * 	action should be taken.
     */
    SendBogusEvent(tkwin);

    /*
     * If either the position of the viewport has changed or the size
     * of width or height of the entire text have changed, move the
     * widgets from the previous viewport out of the current
     * viewport. Worry only about the vertical embedded widget movements.
     * The page is always draw at full width and the viewport will clip
     * the text.
     */
    if ((htPtr->first != oldFirst) || (htPtr->last != oldLast)) {
	int offset;
	int i;
	int first, last;
	Blt_ChainLink *linkPtr;
	EmbeddedWidget *winPtr;

	/* Figure out which lines are now out of the viewport */

	if ((htPtr->first > oldFirst) && (htPtr->first <= oldLast)) {
	    first = oldFirst, last = htPtr->first;
	} else if ((htPtr->last < oldLast) && (htPtr->last >= oldFirst)) {
	    first = htPtr->last, last = oldLast;
	} else {
	    first = oldFirst, last = oldLast;
	}

	for (i = first; i <= last; i++) {
	    offset = htPtr->lineArr[i].offset;
	    for (linkPtr = Blt_ChainFirstLink(htPtr->lineArr[i].chainPtr);
		linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
		winPtr = Blt_ChainGetValue(linkPtr);
		if (winPtr->tkwin != NULL) {
		    MoveEmbeddedWidget(winPtr, offset);
		    winPtr->flags &= ~WIDGET_VISIBLE;
		}
	    }
	}
    }
    DrawPage(htPtr, deltaY);
    SendBogusEvent(tkwin);

    /* Reset flags */
    htPtr->flags &= ~TEXT_DIRTY;
}

/* Selection Procedures */
/*
 *----------------------------------------------------------------------
 *
 * TextSelectionProc --
 *
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
TextSelectionProc(clientData, offset, buffer, maxBytes)
    ClientData clientData;	/* Information about Text widget. */
    int offset;			/* Offset within selection of first
				 * character to be returned. */
    char *buffer;		/* Location in which to place
				 * selection. */
    int maxBytes;		/* Maximum number of bytes to place
				 * at buffer, not including terminating
				 * NULL character. */
{
    HText *htPtr = clientData;
    int size;

    if ((htPtr->selFirst < 0) || (!htPtr->exportSelection)) {
	return -1;
    }
    size = (htPtr->selLast - htPtr->selFirst) + 1 - offset;
    if (size > maxBytes) {
	size = maxBytes;
    }
    if (size <= 0) {
	return 0;		/* huh? */
    }
    strncpy(buffer, htPtr->charArr + htPtr->selFirst + offset, size);
    buffer[size] = '\0';
    return size;
}

/*
 *----------------------------------------------------------------------
 *
 * TextLostSelection --
 *
 *	This procedure is called back by Tk when the selection is
 *	grabbed away from a Text widget.
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
static void
TextLostSelection(clientData)
    ClientData clientData;	/* Information about Text widget. */
{
    HText *htPtr = clientData;

    if ((htPtr->selFirst >= 0) && (htPtr->exportSelection)) {
	htPtr->selFirst = htPtr->selLast = -1;
	EventuallyRedraw(htPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SelectLine --
 *
 *	Modify the selection by moving both its anchored and un-anchored
 *	ends.  This could make the selection either larger or smaller.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
static int
SelectLine(htPtr, tindex)
    HText *htPtr;		/* Information about widget. */
    int tindex;			/* Index of element that is to
				 * become the "other" end of the
				 * selection. */
{
    int selFirst, selLast;
    int lineNum;
    Line *linePtr;

    lineNum = IndexSearch(htPtr, tindex, 0, htPtr->nLines - 1);
    if (lineNum < 0) {
	char string[200];

	sprintf(string, "can't determine line number from index \"%d\"",
	    tindex);
	Tcl_AppendResult(htPtr->interp, string, (char *)NULL);
	return TCL_ERROR;
    }
    linePtr = htPtr->lineArr + lineNum;
    /*
     * Grab the selection if we don't own it already.
     */
    if ((htPtr->exportSelection) && (htPtr->selFirst == -1)) {
	Tk_OwnSelection(htPtr->tkwin, XA_PRIMARY, TextLostSelection, htPtr);
    }
    selFirst = linePtr->textStart;
    selLast = linePtr->textEnd;
    htPtr->selAnchor = tindex;
    if ((htPtr->selFirst != selFirst) ||
	(htPtr->selLast != selLast)) {
	htPtr->selFirst = selFirst;
	htPtr->selLast = selLast;
	EventuallyRedraw(htPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectWord --
 *
 *	Modify the selection by moving both its anchored and un-anchored
 *	ends.  This could make the selection either larger or smaller.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
static int
SelectWord(htPtr, tindex)
    HText *htPtr;		/* Information about widget. */
    int tindex;			/* Index of element that is to
				 * become the "other" end of the
				 * selection. */
{
    int selFirst, selLast;
    int i;

    for (i = tindex; i < htPtr->nChars; i++) {
	if (isspace(UCHAR(htPtr->charArr[i]))) {
	    break;
	}
    }
    selLast = i - 1;
    for (i = tindex; i >= 0; i--) {
	if (isspace(UCHAR(htPtr->charArr[i]))) {
	    break;
	}
    }
    selFirst = i + 1;
    if (selFirst > selLast) {
	selFirst = selLast = tindex;
    }
    /*
     * Grab the selection if we don't own it already.
     */
    if ((htPtr->exportSelection) && (htPtr->selFirst == -1)) {
	Tk_OwnSelection(htPtr->tkwin, XA_PRIMARY, TextLostSelection, htPtr);
    }
    htPtr->selAnchor = tindex;
    if ((htPtr->selFirst != selFirst) || (htPtr->selLast != selLast)) {
	htPtr->selFirst = selFirst, htPtr->selLast = selLast;
	EventuallyRedraw(htPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectTextBlock --
 *
 *	Modify the selection by moving its un-anchored end.  This
 *	could make the selection either larger or smaller.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
static int
SelectTextBlock(htPtr, tindex)
    HText *htPtr;		/* Information about widget. */
    int tindex;			/* Index of element that is to
				 * become the "other" end of the
				 * selection. */
{
    int selFirst, selLast;

    /*
     * Grab the selection if we don't own it already.
     */

    if ((htPtr->exportSelection) && (htPtr->selFirst == -1)) {
	Tk_OwnSelection(htPtr->tkwin, XA_PRIMARY, TextLostSelection, htPtr);
    }
    /*  If the anchor hasn't been set yet, assume the beginning of the text*/
    if (htPtr->selAnchor < 0) {
	htPtr->selAnchor = 0;
    }
    if (htPtr->selAnchor <= tindex) {
	selFirst = htPtr->selAnchor;
	selLast = tindex;
    } else {
	selFirst = tindex;
	selLast = htPtr->selAnchor;
    }
    if ((htPtr->selFirst != selFirst) || (htPtr->selLast != selLast)) {
	htPtr->selFirst = selFirst, htPtr->selLast = selLast;
	EventuallyRedraw(htPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectOp --
 *
 *	This procedure handles the individual options for text
 *	selections.  The selected text is designated by start and end
 *	indices into the text pool.  The selected segment has both a
 *	anchored and unanchored ends.  The following selection
 *	operations are implemented:
 *
 *	  "adjust"	- resets either the first or last index
 *			  of the selection.
 *	  "clear"	- clears the selection. Sets first/last
 *			  indices to -1.
 *	  "from"	- sets the index of the selection anchor.
 *	  "line"	- sets the first of last indices to the
 *			  start and end of the line at the
 *			  designated point.
 *	  "present"	- return "1" if a selection is available,
 *			  "0" otherwise.
 *	  "range"	- sets the first and last indices.
 *	  "to"		- sets the index of the un-anchored end.
 *	  "word"	- sets the first of last indices to the
 *			  start and end of the word at the
 *			  designated point.
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
static int
SelectOp(htPtr, interp, argc, argv)
    HText *htPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int iselection;
    unsigned int length;
    int result = TCL_OK;
    char c;

    length = strlen(argv[2]);
    c = argv[2][0];
    if ((c == 'c') && (strncmp(argv[2], "clear", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" selection clear\"", (char *)NULL);
	    return TCL_ERROR;
	}
	if (htPtr->selFirst != -1) {
	    htPtr->selFirst = htPtr->selLast = -1;
	    EventuallyRedraw(htPtr);
	}
	return TCL_OK;
    } else if ((c == 'p') && (strncmp(argv[2], "present", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" selection present\"", (char *)NULL);
	    return TCL_ERROR;
	}
	Tcl_AppendResult(interp, (htPtr->selFirst != -1) ? "0" : "1",
	    (char *)NULL);
	return TCL_OK;
    } else if ((c == 'r') && (strncmp(argv[2], "range", length) == 0)) {
	int selFirst, selLast;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" selection range first last\"", (char *)NULL);
	    return TCL_ERROR;
	}
	if (GetIndex(htPtr, argv[3], &selFirst) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (GetIndex(htPtr, argv[4], &selLast) != TCL_OK) {
	    return TCL_ERROR;
	}
	htPtr->selAnchor = selFirst;
	SelectTextBlock(htPtr, selLast);
	return TCL_OK;
    }
    if (argc != 4) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " selection ", argv[2], " index\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (GetIndex(htPtr, argv[3], &iselection) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((c == 'f') && (strncmp(argv[2], "from", length) == 0)) {
	htPtr->selAnchor = iselection;
    } else if ((c == 'a') && (strncmp(argv[2], "adjust", length) == 0)) {
	int half1, half2;

	half1 = (htPtr->selFirst + htPtr->selLast) / 2;
	half2 = (htPtr->selFirst + htPtr->selLast + 1) / 2;
	if (iselection < half1) {
	    htPtr->selAnchor = htPtr->selLast;
	} else if (iselection > half2) {
	    htPtr->selAnchor = htPtr->selFirst;
	}
	result = SelectTextBlock(htPtr, iselection);
    } else if ((c == 't') && (strncmp(argv[2], "to", length) == 0)) {
	result = SelectTextBlock(htPtr, iselection);
    } else if ((c == 'w') && (strncmp(argv[2], "word", length) == 0)) {
	result = SelectWord(htPtr, iselection);
    } else if ((c == 'l') && (strncmp(argv[2], "line", length) == 0)) {
	result = SelectLine(htPtr, iselection);
    } else {
	Tcl_AppendResult(interp, "bad selection operation \"", argv[2],
	    "\": should be \"adjust\", \"clear\", \"from\", \"line\", \
\"present\", \"range\", \"to\", or \"word\"", (char *)NULL);
	return TCL_ERROR;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GotoOp --
 *
 *	Move the top line of the viewport to the new location based
 *	upon the given line number.  Force out-of-range requests to the
 *	top or bottom of text.
 *
 * Results:
 *	A standard Tcl result. If TCL_OK, interp->result contains the
 *	current line number.
 *
 * Side effects:
 *	At the next idle point, the text viewport will be move to the
 *	new line.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
GotoOp(htPtr, interp, argc, argv)
    HText *htPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    int line;

    line = htPtr->first;
    if (argc == 3) {
	int tindex;

	if (GetIndex(htPtr, argv[2], &tindex) != TCL_OK) {
	    return TCL_ERROR;
	}
	line = IndexSearch(htPtr, tindex, 0, htPtr->nLines - 1);
	if (line < 0) {
	    char string[200];

	    sprintf(string, "can't determine line number from index \"%d\"",
		tindex);
	    Tcl_AppendResult(htPtr->interp, string, (char *)NULL);
	    return TCL_ERROR;
	}
	htPtr->reqLineNum = line;
	htPtr->flags |= TEXT_DIRTY;

	/*
	 * Make only a request for a change in the viewport.  Defer
	 * the actual scrolling until the text layout is adjusted at
	 * the next idle point.
	 */
	if (line != htPtr->first) {
	    htPtr->flags |= GOTO_PENDING;
	    EventuallyRedraw(htPtr);
	}
    }
    Tcl_SetResult(htPtr->interp, Blt_Itoa(line), TCL_VOLATILE);
    return TCL_OK;
}


static int
XViewOp(htPtr, interp, argc, argv)
    HText *htPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int width, worldWidth;

    width = Tk_Width(htPtr->tkwin);
    worldWidth = htPtr->worldWidth;
    if (argc == 2) {
	double fract;

	/* Report first and last fractions */
	fract = (double)htPtr->xOffset / worldWidth;
	Tcl_AppendElement(interp, Blt_Dtoa(interp, CLAMP(fract, 0.0, 1.0)));
	fract = (double)(htPtr->xOffset + width) / worldWidth;
	Tcl_AppendElement(interp, Blt_Dtoa(interp, CLAMP(fract, 0.0, 1.0)));
	return TCL_OK;
    }
    htPtr->pendingX = htPtr->xOffset;
    if (Blt_GetScrollInfo(interp, argc - 2, argv + 2, &(htPtr->pendingX),
	    worldWidth, width, htPtr->xScrollUnits, BLT_SCROLL_MODE_LISTBOX) 
	!= TCL_OK) {
	return TCL_ERROR;
    }
    htPtr->flags |= TEXT_DIRTY;
    EventuallyRedraw(htPtr);
    return TCL_OK;
}

static int
YViewOp(htPtr, interp, argc, argv)
    HText *htPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int height, worldHeight;

    height = Tk_Height(htPtr->tkwin);
    worldHeight = htPtr->worldHeight;
    if (argc == 2) {
	double fract;

	/* Report first and last fractions */
	fract = (double)htPtr->yOffset / worldHeight;
	Tcl_AppendElement(interp, Blt_Dtoa(interp, CLAMP(fract, 0.0, 1.0)));
	fract = (double)(htPtr->yOffset + height) / worldHeight;
	Tcl_AppendElement(interp, Blt_Dtoa(interp, CLAMP(fract, 0.0, 1.0)));
	return TCL_OK;
    }
    htPtr->pendingY = htPtr->yOffset;
    if (Blt_GetScrollInfo(interp, argc - 2, argv + 2, &(htPtr->pendingY),
	    worldHeight, height, htPtr->yScrollUnits, BLT_SCROLL_MODE_LISTBOX)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    htPtr->flags |= TEXT_DIRTY;
    EventuallyRedraw(htPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * AppendOp --
 *
 * 	This procedure embeds a Tk widget into the hypertext.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Memory is allocated.  EmbeddedWidget gets configured.
 *
 * ----------------------------------------------------------------------
 */
static int
AppendOp(htPtr, interp, argc, argv)
    HText *htPtr;		/* Hypertext widget */
    Tcl_Interp *interp;		/* Interpreter associated with widget */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Line *linePtr;
    EmbeddedWidget *winPtr;

    winPtr = CreateEmbeddedWidget(htPtr, argv[2]);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }
    if (Tk_ConfigureWidget(interp, htPtr->tkwin, widgetConfigSpecs,
	    argc - 3, argv + 3, (char *)winPtr, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    /*
     * Append widget to list of embedded widgets of the last line.
     */
    linePtr = GetLastLine(htPtr);
    if (linePtr == NULL) {
	Tcl_AppendResult(htPtr->interp, "can't allocate line structure",
	    (char *)NULL);
	return TCL_ERROR;
    }
    Blt_ChainAppend(linePtr->chainPtr, winPtr);
    linePtr->width += winPtr->cavityWidth;
    winPtr->precedingTextEnd = linePtr->textEnd;

    htPtr->flags |= (WIDGET_APPENDED | REQUEST_LAYOUT);
    EventuallyRedraw(htPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WindowsOp --
 *
 *	Returns a list of all the pathNames of embedded widgets of the
 *	HText widget.  If a pattern argument is given, only the names
 *	of windows matching it will be placed into the list.
 *
 * Results:
 *	Standard Tcl result.  If TCL_OK, interp->result will contain
 *	the list of the embedded widget pathnames.  Otherwise it will
 *	contain an error message.
 *
 *----------------------------------------------------------------------
 */
static int
WindowsOp(htPtr, interp, argc, argv)
    HText *htPtr;		/* Hypertext widget record */
    Tcl_Interp *interp;		/* Interpreter associated with widget */
    int argc;
    char **argv;
{
    EmbeddedWidget *winPtr;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    char *name;

    for (hPtr = Blt_FirstHashEntry(&(htPtr->widgetTable), &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	winPtr = (EmbeddedWidget *)Blt_GetHashValue(hPtr);
	if (winPtr->tkwin == NULL) {
	    fprintf(stderr, "window `%s' is null\n",
		Tk_PathName(Blt_GetHashKey(&(htPtr->widgetTable), hPtr)));
	    continue;
	}
	name = Tk_PathName(winPtr->tkwin);
	if ((argc == 2) || (Tcl_StringMatch(name, argv[2]))) {
	    Tcl_AppendElement(interp, name);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CgetOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOp(htPtr, interp, argc, argv)
    HText *htPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    char *itemPtr;
    Tk_ConfigSpec *specsPtr;

    if ((argc > 2) && (argv[2][0] == '.')) {
	Tk_Window tkwin;
	EmbeddedWidget *winPtr;

	/* EmbeddedWidget window to be configured */
	tkwin = Tk_NameToWindow(interp, argv[2], htPtr->tkwin);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	winPtr = FindEmbeddedWidget(htPtr, tkwin);
	if (winPtr == NULL) {
	    Tcl_AppendResult(interp, "window \"", argv[2],
		"\" is not managed by \"", argv[0], "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	specsPtr = widgetConfigSpecs;
	itemPtr = (char *)winPtr;
	argv++;
    } else {
	specsPtr = configSpecs;
	itemPtr = (char *)htPtr;
    }
    return Tk_ConfigureValue(interp, htPtr->tkwin, specsPtr, itemPtr,
	    argv[2], 0);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 * 	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	a hypertext widget.
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 * 	etc. get set for htPtr;  old resources get freed, if there were any.
 * 	The hypertext is redisplayed.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOp(htPtr, interp, argc, argv)
    HText *htPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    char *itemPtr;
    Tk_ConfigSpec *specsPtr;

    if ((argc > 2) && (argv[2][0] == '.')) {
	Tk_Window tkwin;
	EmbeddedWidget *winPtr;

	/* EmbeddedWidget window to be configured */
	tkwin = Tk_NameToWindow(interp, argv[2], htPtr->tkwin);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	winPtr = FindEmbeddedWidget(htPtr, tkwin);
	if (winPtr == NULL) {
	    Tcl_AppendResult(interp, "window \"", argv[2],
		"\" is not managed by \"", argv[0], "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	specsPtr = widgetConfigSpecs;
	itemPtr = (char *)winPtr;
	argv++;
	argc--;
    } else {
	specsPtr = configSpecs;
	itemPtr = (char *)htPtr;
    }
    if (argc == 2) {
	return Tk_ConfigureInfo(interp, htPtr->tkwin, specsPtr, itemPtr,
		(char *)NULL, 0);
    } else if (argc == 3) {
	return Tk_ConfigureInfo(interp, htPtr->tkwin, specsPtr, itemPtr,
		argv[2], 0);
    }
    if (Tk_ConfigureWidget(interp, htPtr->tkwin, specsPtr, argc - 2,
	    argv + 2, itemPtr, TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == (char *)htPtr) {
	/* Reconfigure the master */
	if (ConfigureText(interp, htPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	htPtr->flags |= REQUEST_LAYOUT;
    }
    EventuallyRedraw(htPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ScanOp --
 *
 *	Implements the quick scan for hypertext widgets.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ScanOp(htPtr, interp, argc, argv)
    HText *htPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    int x, y;
    char c;
    unsigned int length;


    if (Blt_GetXY(interp, htPtr->tkwin, argv[3], &x, &y) != TCL_OK) {
	return TCL_ERROR;
    }
    c = argv[2][0];
    length = strlen(argv[2]);
    if ((c == 'm') && (strncmp(argv[2], "mark", length) == 0)) {
	htPtr->scanMark.x = x, htPtr->scanMark.y = y;
	htPtr->scanPt.x = htPtr->xOffset;
	htPtr->scanPt.y = htPtr->yOffset;

    } else if ((c == 'd') && (strncmp(argv[2], "dragto", length) == 0)) {
	int px, py;

	px = htPtr->scanPt.x - (10 * (x - htPtr->scanMark.x));
	py = htPtr->scanPt.y - (10 * (y - htPtr->scanMark.y));

	if (px < 0) {
	    px = htPtr->scanPt.x = 0;
	    htPtr->scanMark.x = x;
	} else if (px >= htPtr->worldWidth) {
	    px = htPtr->scanPt.x = htPtr->worldWidth - htPtr->xScrollUnits;
	    htPtr->scanMark.x = x;
	}
	if (py < 0) {
	    py = htPtr->scanPt.y = 0;
	    htPtr->scanMark.y = y;
	} else if (py >= htPtr->worldHeight) {
	    py = htPtr->scanPt.y = htPtr->worldHeight - htPtr->yScrollUnits;
	    htPtr->scanMark.y = y;
	}
	if ((py != htPtr->pendingY) || (px != htPtr->pendingX)) {
	    htPtr->pendingX = px, htPtr->pendingY = py;
	    htPtr->flags |= TEXT_DIRTY;
	    EventuallyRedraw(htPtr);
	}
    } else {
	Tcl_AppendResult(interp, "bad scan operation \"", argv[2],
	    "\": should be either \"mark\" or \"dragto\"", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SearchOp --
 *
 *	Returns the linenumber of the next line matching the given
 *	pattern within the range of lines provided.  If the first
 *	line number is greater than the last, the search is done in
 *	reverse.
 *
 *----------------------------------------------------------------------
 */
static int
SearchOp(htPtr, interp, argc, argv)
    HText *htPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    char *startPtr, *endPtr;
    char saved;
    Tcl_RegExp regExpToken;
    int iFirst, iLast;
    int matchStart, matchEnd;
    int match;

    regExpToken = Tcl_RegExpCompile(interp, argv[2]);
    if (regExpToken == NULL) {
	return TCL_ERROR;
    }
    iFirst = 0;
    iLast = htPtr->nChars;
    if (argc > 3) {
	if (GetIndex(htPtr, argv[3], &iFirst) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (argc == 4) {
	if (GetIndex(htPtr, argv[4], &iLast) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (iLast < iFirst) {
	return TCL_ERROR;
    }
    matchStart = matchEnd = -1;
    startPtr = htPtr->charArr + iFirst;
    endPtr = htPtr->charArr + (iLast + 1);
    saved = *endPtr;
    *endPtr = '\0';		/* Make the line a string by changing the
				 * '\n' into a NUL byte before searching */
    match = Tcl_RegExpExec(interp, regExpToken, startPtr, startPtr);
    *endPtr = saved;
    if (match < 0) {
	return TCL_ERROR;
    } else if (match > 0) {
	Tcl_RegExpRange(regExpToken, 0, &startPtr, &endPtr);
	if ((startPtr != NULL) || (endPtr != NULL)) {
	    matchStart = startPtr - htPtr->charArr;
	    matchEnd = endPtr - htPtr->charArr - 1;
	}
    }
    if (match > 0) {
	Tcl_AppendElement(interp, Blt_Itoa(matchStart));
	Tcl_AppendElement(interp, Blt_Itoa(matchEnd));
    } else {
	Tcl_ResetResult(interp);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RangeOp --
 *
 *	Returns the characters designated by the range of elements.
 *
 *----------------------------------------------------------------------
 */
static int
RangeOp(htPtr, interp, argc, argv)
    HText *htPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    char *startPtr, *endPtr;
    char saved;
    int textFirst, textLast;

    textFirst = htPtr->selFirst;
    textLast = htPtr->selLast;
    if (textFirst < 0) {
	textFirst = 0;
	textLast = htPtr->nChars - 1;
    }
    if (argc > 2) {
	if (GetIndex(htPtr, argv[2], &textFirst) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (argc == 4) {
	if (GetIndex(htPtr, argv[3], &textLast) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (textLast < textFirst) {
	Tcl_AppendResult(interp, "first index is greater than last", (char *)NULL);
	return TCL_ERROR;
    }
    startPtr = htPtr->charArr + textFirst;
    endPtr = htPtr->charArr + (textLast + 1);
    saved = *endPtr;
    *endPtr = '\0';		/* Make the line into a string by
				 * changing the * '\n' into a '\0'
				 * before copying */
    Tcl_SetResult(interp, startPtr, TCL_VOLATILE);
    *endPtr = saved;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IndexOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
IndexOp(htPtr, interp, argc, argv)
    HText *htPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    int tindex;

    if (GetIndex(htPtr, argv[2], &tindex) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, Blt_Itoa(tindex), TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LinePosOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
LinePosOp(htPtr, interp, argc, argv)
    HText *htPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    int line, cpos, tindex;
    char string[200];

    if (GetIndex(htPtr, argv[2], &tindex) != TCL_OK) {
	return TCL_ERROR;
    }
    if (GetTextPosition(htPtr, tindex, &line, &cpos) != TCL_OK) {
	return TCL_ERROR;
    }
    sprintf(string, "%d.%d", line, cpos);
    Tcl_SetResult(interp, string, TCL_VOLATILE);
    return TCL_OK;
}

/*
 * --------------------------------------------------------------
 *
 * TextWidgetCmd --
 *
 * 	This procedure is invoked to process the Tcl command that
 *	corresponds to a widget managed by this module. See the user
 * 	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 * --------------------------------------------------------------
 */

static Blt_OpSpec textOps[] =
{
    {"append", 1, (Blt_Op)AppendOp, 3, 0, "window ?option value?...",},
    {"cget", 2, (Blt_Op)CgetOp, 3, 3, "?window? option",},
    {"configure", 2, (Blt_Op)ConfigureOp, 2, 0,
	"?window? ?option value?...",},
    {"gotoline", 2, (Blt_Op)GotoOp, 2, 3, "?line?",},
    {"index", 1, (Blt_Op)IndexOp, 3, 3, "string",},
    {"linepos", 1, (Blt_Op)LinePosOp, 3, 3, "string",},
    {"range", 2, (Blt_Op)RangeOp, 2, 4, "?from? ?to?",},
    {"scan", 2, (Blt_Op)ScanOp, 4, 4, "oper @x,y",},
    {"search", 3, (Blt_Op)SearchOp, 3, 5, "pattern ?from? ?to?",},
    {"selection", 3, (Blt_Op)SelectOp, 3, 5, "oper ?index?",},
    {"windows", 6, (Blt_Op)WindowsOp, 2, 3, "?pattern?",},
    {"xview", 1, (Blt_Op)XViewOp, 2, 5,
	"?moveto fract? ?scroll number what?",},
    {"yview", 1, (Blt_Op)YViewOp, 2, 5,
	"?moveto fract? ?scroll number what?",},
};
static int nTextOps = sizeof(textOps) / sizeof(Blt_OpSpec);

static int
TextWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about hypertext widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Blt_Op proc;
    int result;
    HText *htPtr = clientData;

    proc = Blt_GetOp(interp, nTextOps, textOps, BLT_OP_ARG1, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve(htPtr);
    result = (*proc) (htPtr, interp, argc, argv);
    Tcl_Release(htPtr);
    return result;
}

/*
 * --------------------------------------------------------------
 *
 * TextCmd --
 *
 * 	This procedure is invoked to process the "htext" Tcl command.
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
/* ARGSUSED */
static int
TextCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    HText *htPtr;
    Screen *screenPtr;
    Tk_Window tkwin;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " pathName ?option value?...\"", (char *)NULL);
	return TCL_ERROR;
    }
    htPtr = Blt_Calloc(1, sizeof(HText));
    assert(htPtr);
    tkwin = Tk_MainWindow(interp);
    tkwin = Tk_CreateWindowFromPath(interp, tkwin, argv[1], (char *)NULL);
    if (tkwin == NULL) {
	Blt_Free(htPtr);
	return TCL_ERROR;
    }
    /* Initialize the new hypertext widget */

    Tk_SetClass(tkwin, "Htext");
    htPtr->tkwin = tkwin;
    htPtr->display = Tk_Display(tkwin);
    htPtr->interp = interp;
    htPtr->nLines = htPtr->arraySize = 0;
    htPtr->leader = 1;
    htPtr->xScrollUnits = htPtr->yScrollUnits = 10;
    htPtr->nRows = htPtr->nColumns = 0;
    htPtr->selFirst = htPtr->selLast = -1;
    htPtr->selAnchor = 0;
    htPtr->exportSelection = TRUE;
    htPtr->selBorderWidth = 2;
    screenPtr = Tk_Screen(htPtr->tkwin);
    htPtr->maxWidth = WidthOfScreen(screenPtr);
    htPtr->maxHeight = HeightOfScreen(screenPtr);
    Blt_InitHashTable(&(htPtr->widgetTable), BLT_ONE_WORD_KEYS);

    Tk_CreateSelHandler(tkwin, XA_PRIMARY, XA_STRING, TextSelectionProc,
	htPtr, XA_STRING);
    Tk_CreateEventHandler(tkwin, ExposureMask | StructureNotifyMask,
	TextEventProc, htPtr);
#if (TK_MAJOR_VERSION > 4)
    Blt_SetWindowInstanceData(tkwin, htPtr);
#endif
    /*
     * -----------------------------------------------------------------
     *
     *  Create the widget command before configuring the widget. This
     *  is because the "-file" and "-text" options may have embedded
     *  commands that self-reference the widget through the
     *  "$blt_htext(widget)" variable.
     *
     * ------------------------------------------------------------------
     */
    htPtr->cmdToken = Tcl_CreateCommand(interp, argv[1], TextWidgetCmd, htPtr, 
	TextDeleteCmdProc);
#ifdef ITCL_NAMESPACES
    Itk_SetWidgetCommand(htPtr->tkwin, htPtr->cmdToken);
#endif
    if ((Tk_ConfigureWidget(interp, htPtr->tkwin, configSpecs, argc - 2,
		argv + 2, (char *)htPtr, 0) != TCL_OK) ||
	(ConfigureText(interp, htPtr) != TCL_OK)) {
	Tk_DestroyWindow(htPtr->tkwin);
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, Tk_PathName(htPtr->tkwin), TCL_VOLATILE);
    return TCL_OK;
}

int
Blt_HtextInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec =
    {"htext", TextCmd,};

    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#endif /* NO_HTEXT */
