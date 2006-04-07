/*
 * bltTabnotebook.c --
 *
 *	This module implements a tab notebook widget for the BLT toolkit.
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
 *	Tabnotebook widget created by George A. Howlett (gah@bell-labs.com)
 *
 */

#include "bltInt.h"

#ifndef NO_TABNOTEBOOK
#include "bltBind.h"
#include "bltChain.h"
#include "bltHash.h"
#include "bltTile.h"
#include <ctype.h>

#if (TK_MAJOR_VERSION == 4)
#define TK_REPARENTED           0x2000
#endif

#define INVALID_FAIL	0
#define INVALID_OK	1

/*
 * The macro below is used to modify a "char" value (e.g. by casting
 * it to an unsigned character) so that it can be used safely with
 * macros such as isspace.
 */
#define CLAMP(val,low,hi)	\
	(((val) < (low)) ? (low) : ((val) > (hi)) ? (hi) : (val))

#define GAP			3
#define SELECT_PADX		4
#define SELECT_PADY		2
#define OUTER_PAD		2
#define LABEL_PAD		1
#define LABEL_PADX		2
#define LABEL_PADY		2
#define IMAGE_PAD		1
#define CORNER_OFFSET		3

#define TAB_SCROLL_OFFSET	10

#define SLANT_NONE		0
#define SLANT_LEFT		1
#define SLANT_RIGHT		2
#define SLANT_BOTH		(SLANT_LEFT | SLANT_RIGHT)

#define END			(-1)
#define ODD(x)			((x) | 0x01)

#define TABWIDTH(s, t)		\
  ((s)->side & SIDE_VERTICAL) ? (t)->height : (t)->width)
#define TABHEIGHT(s, t)		\
  ((s)->side & SIDE_VERTICAL) ? (t)->height : (t)->width)

#define VPORTWIDTH(s)		 \
  (((s)->side & SIDE_HORIZONTAL) ? (Tk_Width((s)->tkwin) - 2 * (s)->inset) : \
   (Tk_Height((s)->tkwin) - 2 * (s)->inset))

#define VPORTHEIGHT(s)		 \
  (((s)->side & SIDE_VERTICAL) ? (Tk_Width((s)->tkwin) - 2 * (s)->inset) : \
   (Tk_Height((s)->tkwin) - 2 * (s)->inset))

#define GETATTR(t,attr)		\
   (((t)->attr != NULL) ? (t)->attr : (t)->nbPtr->defTabStyle.attr)

/*
 * ----------------------------------------------------------------------------
 *
 *  Internal widget flags:
 *
 *	TNB_LAYOUT		The layout of the widget needs to be
 *				recomputed.
 *
 *	TNB_REDRAW		A redraw request is pending for the widget.
 *
 *	TNB_SCROLL		A scroll request is pending.
 *
 *	TNB_FOCUS		The widget is receiving keyboard events.
 *				Draw the focus highlight border around the
 *				widget.
 *
 *	TNB_MULTIPLE_TIER	Notebook is using multiple tiers.
 *
 *	TNB_STATIC		Notebook does not scroll.
 *
 * ---------------------------------------------------------------------------
 */
#define TNB_LAYOUT		(1<<0)
#define TNB_REDRAW		(1<<1)
#define TNB_SCROLL		(1<<2)
#define TNB_FOCUS		(1<<4)

#define TNB_STATIC		(1<<8)
#define TNB_MULTIPLE_TIER	(1<<9)

#define PERFORATION_ACTIVE	(1<<10)

#define SIDE_TOP		(1<<0)
#define SIDE_RIGHT		(1<<1)
#define SIDE_LEFT		(1<<2)
#define SIDE_BOTTOM		(1<<3)

#define SIDE_VERTICAL	(SIDE_LEFT | SIDE_RIGHT)
#define SIDE_HORIZONTAL (SIDE_TOP | SIDE_BOTTOM)

#define TAB_LABEL		(ClientData)0
#define TAB_PERFORATION		(ClientData)1

#define DEF_TNB_ACTIVE_BACKGROUND	RGB_GREY90
#define DEF_TNB_ACTIVE_BG_MONO	STD_ACTIVE_BG_MONO
#define DEF_TNB_ACTIVE_FOREGROUND	STD_ACTIVE_FOREGROUND
#define DEF_TNB_ACTIVE_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_TNB_BG_MONO		STD_NORMAL_BG_MONO
#define DEF_TNB_BACKGROUND	STD_NORMAL_BACKGROUND
#define DEF_TNB_BORDERWIDTH	"1"
#define DEF_TNB_COMMAND		(char *)NULL
#define DEF_TNB_CURSOR		(char *)NULL
#define DEF_TNB_DASHES		"1"
#define DEF_TNB_FOREGROUND	STD_NORMAL_FOREGROUND
#define DEF_TNB_FG_MONO		STD_NORMAL_FG_MONO
#define DEF_TNB_FONT		STD_FONT
#define DEF_TNB_GAP		"3"
#define DEF_TNB_HEIGHT		"0"
#define DEF_TNB_HIGHLIGHT_BACKGROUND	STD_NORMAL_BACKGROUND
#define DEF_TNB_HIGHLIGHT_BG_MONO	STD_NORMAL_BG_MONO
#define DEF_TNB_HIGHLIGHT_COLOR	RGB_BLACK
#define DEF_TNB_HIGHLIGHT_WIDTH	"2"
#define DEF_TNB_NORMAL_BACKGROUND STD_NORMAL_BACKGROUND
#define DEF_TNB_NORMAL_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_TNB_OUTER_PAD	"3"
#define DEF_TNB_RELIEF		"sunken"
#define DEF_TNB_ROTATE		"0.0"
#define DEF_TNB_SCROLL_INCREMENT "0"
#define DEF_TNB_SELECT_BACKGROUND	STD_NORMAL_BACKGROUND
#define DEF_TNB_SELECT_BG_MONO 	STD_SELECT_BG_MONO
#define DEF_TNB_SELECT_BORDERWIDTH 	"1"
#define DEF_TNB_SELECT_CMD	(char *)NULL
#define DEF_TNB_SELECT_FOREGROUND STD_SELECT_FOREGROUND
#define DEF_TNB_SELECT_FG_MONO  STD_SELECT_FG_MONO
#define DEF_TNB_SELECT_MODE	"multiple"
#define DEF_TNB_SELECT_RELIEF	"raised"
#define DEF_TNB_SELECT_PAD	"5"
#define DEF_TNB_SHADOW_COLOR	RGB_BLACK
#define DEF_TNB_SIDE		"top"
#define DEF_TNB_SLANT		"none"
#define DEF_TNB_TAB_BACKGROUND	RGB_GREY82
#define DEF_TNB_TAB_BG_MONO	STD_SELECT_BG_MONO
#define DEF_TNB_TAB_RELIEF	"raised"
#define DEF_TNB_TAKE_FOCUS	"1"
#define DEF_TNB_TEXT_COLOR	STD_NORMAL_FOREGROUND
#define DEF_TNB_TEXT_MONO	STD_NORMAL_FG_MONO
#define DEF_TNB_TEXT_SIDE	"left"
#define DEF_TNB_TIERS		"1"
#define DEF_TNB_TILE		(char *)NULL
#define DEF_TNB_WIDTH		"0"
#define DEF_TNB_SAME_WIDTH	"yes"
#define DEF_TNB_TEAROFF		"yes"
#define DEF_TNB_PAGE_WIDTH	"0"
#define DEF_TNB_PAGE_HEIGHT	"0"

#define DEF_TAB_ACTIVE_BG	(char *)NULL
#define DEF_TAB_ACTIVE_FG	(char *)NULL
#define DEF_TAB_ANCHOR		"center"
#define DEF_TAB_BG		(char *)NULL
#define DEF_TAB_COMMAND		(char *)NULL
#define DEF_TAB_DATA		(char *)NULL
#define DEF_TAB_FG		(char *)NULL
#define DEF_TAB_FILL		"none"
#define DEF_TAB_FONT		(char *)NULL
#define DEF_TAB_HEIGHT		"0"
#define DEF_TAB_IMAGE		(char *)NULL
#define DEF_TAB_IPAD		"0"
#define DEF_TAB_PAD		"3"
#define DEF_TAB_PERF_COMMAND	(char *)NULL
#define DEF_TAB_SELECT_BG	(char *)NULL
#define DEF_TAB_SELECT_BORDERWIDTH 	"1"
#define DEF_TAB_SELECT_CMD	(char *)NULL
#define DEF_TAB_SELECT_FG	(char *)NULL
#define DEF_TAB_SHADOW		(char *)NULL
#define DEF_TAB_STATE		"normal"
#define DEF_TAB_STIPPLE		"BLT"
#define DEF_TAB_BIND_TAGS	"all"
#define DEF_TAB_TEXT		(char *)NULL
#define DEF_TAB_VISUAL		(char *)NULL
#define DEF_TAB_WIDTH		"0"
#define DEF_TAB_WINDOW		(char *)NULL

typedef struct NotebookStruct Notebook;

static void EmbeddedWidgetGeometryProc _ANSI_ARGS_((ClientData, Tk_Window));
static void EmbeddedWidgetCustodyProc _ANSI_ARGS_((ClientData, Tk_Window));

static Tk_GeomMgr tabMgrInfo =
{
    "notebook",			/* Name of geometry manager used by winfo */
    EmbeddedWidgetGeometryProc,	/* Procedure to for new geometry requests */
    EmbeddedWidgetCustodyProc,	/* Procedure when window is taken away */
};

extern Tk_CustomOption bltDashesOption;
extern Tk_CustomOption bltFillOption;
extern Tk_CustomOption bltDistanceOption;
extern Tk_CustomOption bltPositiveDistanceOption;
extern Tk_CustomOption bltPositiveCountOption;
extern Tk_CustomOption bltListOption;
extern Tk_CustomOption bltPadOption;
extern Tk_CustomOption bltShadowOption;
extern Tk_CustomOption bltStateOption;
extern Tk_CustomOption bltTileOption;
extern Tk_CustomOption bltUidOption;

static int StringToImage _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int offset));
static char *ImageToString _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtrPtr));

static int StringToWindow _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int offset));
static char *WindowToString _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtrPtr));

static int StringToSide _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int offset));
static char *SideToString _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtrPtr));

static int StringToSlant _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int offset));
static char *SlantToString _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtrPtr));

/*
 * Contains a pointer to the widget that's currently being configured.
 * This is used in the custom configuration parse routine for images.
 */
static Notebook *lastNotebookInstance;

static Tk_CustomOption imageOption =
{
    StringToImage, ImageToString, (ClientData)&lastNotebookInstance,
};

static Tk_CustomOption sideOption =
{
    StringToSide, SideToString, (ClientData)0,
};

static Tk_CustomOption windowOption =
{
    StringToWindow, WindowToString, (ClientData)0,
};

static Tk_CustomOption slantOption =
{
    StringToSlant, SlantToString, (ClientData)0,
};

/*
 * TabImage --
 *
 *	When multiple instances of an image are displayed in the
 *	same widget, this can be inefficient in terms of both memory
 *	and time.  We only need one instance of each image, regardless
 *	of number of times we use it.  And searching/deleting instances
 *	can be very slow as the list gets large.
 *
 *	The workaround, employed below, is to maintain a hash table of
 *	images that maintains a reference count for each image.
 */

typedef struct TabImageStruct {
    int refCount;		/* Reference counter for this image. */
    Tk_Image tkImage;		/* The Tk image being cached. */
    int width, height;		/* Dimensions of the cached image. */
    Blt_HashEntry *hashPtr;	/* Hash table pointer to the image. */

} *TabImage;

#define ImageHeight(image)	((image)->height)
#define ImageWidth(image)	((image)->width)
#define ImageBits(image)	((image)->tkImage)

#define TAB_VISIBLE	(1<<0)
#define TAB_REDRAW	(1<<2)

typedef struct {
    char *name;			/* Identifier for tab entry */
    int state;			/* State of the tab: Disabled, active, or
				 * normal. */
    unsigned int flags;

    int tier;			/* Index of tier [1..numTiers] containing
				 * this tab. */

    int worldX, worldY;		/* Position of the tab in world coordinates. */
    int worldWidth, worldHeight;/* Dimensions of the tab, corrected for
				 * orientation (-side).  It includes the
				 * border, padding, label, etc. */
    int screenX, screenY;
    short int screenWidth, screenHeight;	/*  */

    Notebook *nbPtr;		/* Notebook that includes this
				 * tab. Needed for callbacks can pass
				 * only a tab pointer.  */
    Blt_Uid tags;

    /*
     * Tab label:
     */
    Blt_Uid text;		/* String displayed as the tab's label. */
    TabImage image;		/* Image displayed as the label. */

    short int textWidth, textHeight;
    short int labelWidth, labelHeight;
    Blt_Pad iPadX, iPadY;	/* Internal padding around the text */

    Tk_Font font;

    /*
     * Normal:
     */
    XColor *textColor;		/* Text color */
    Tk_3DBorder border;		/* Background color and border for tab.*/

    /*
     * Selected: Tab is currently selected.
     */
    XColor *selColor;		/* Selected text color */
    Tk_3DBorder selBorder;	/* 3D border of selected folder. */

    /*
     * Active: Mouse passes over the tab.
     */
    Tk_3DBorder activeBorder;	/* Active background color. */
    XColor *activeFgColor;	/* Active text color */

    Shadow shadow;
    Pixmap stipple;		/* Stipple for outline of embedded window
				 * when torn off. */
    /*
     * Embedded widget information:
     */
    Tk_Window tkwin;		/* Widget to be mapped when the tab is
				 * selected.  If NULL, don't make
				 * space for the page. */

    int reqWidth, reqHeight;	/* If non-zero, overrides the
				 * requested dimensions of the
				 * embedded widget. */

    Tk_Window container;	/* The window containing the embedded
				 * widget.  Does not necessarily have
				 * to be the parent. */

    Tk_Anchor anchor;		/* Anchor: indicates how the embedded
				 * widget is positioned within the
				 * extra space on the page. */

    Blt_Pad padX, padY;		/* Padding around embedded widget */

    int fill;			/* Indicates how the window should
				 * fill the page. */

    /*
     * Auxillary information:
     */
    Blt_Uid command;		/* Command (malloc-ed) invoked when the tab
				 * is selected */
    Blt_Uid data;		/* This value isn't used in C code.
				 * It may be used by clients in Tcl bindings
				 * to associate extra data (other than the
				 * label or name) with the tab. */

    Blt_ChainLink *linkPtr;	/* Pointer to where the tab resides in the
				 * list of tabs. */
    Blt_Uid perfCommand;		/* Command (malloc-ed) invoked when the tab
				 * is selected */
    GC textGC;
    GC backGC;

    Blt_Tile tile;

} Tab;

static Tk_ConfigSpec tabConfigSpecs[] =
{
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground",
	"ActiveBackground", DEF_TAB_ACTIVE_BG, 
	Tk_Offset(Tab, activeBorder), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground",
	"ActiveForeground", DEF_TAB_ACTIVE_FG, 
	Tk_Offset(Tab, activeFgColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_TAB_ANCHOR, Tk_Offset(Tab, anchor), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_TAB_BG, Tk_Offset(Tab, border), TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, "-bindtags", "bindTags", "BindTags",
	DEF_TAB_BIND_TAGS, Tk_Offset(Tab, tags),
	TK_CONFIG_NULL_OK, &bltUidOption},
    {TK_CONFIG_CUSTOM, "-command", "command", "Command",
	DEF_TAB_COMMAND, Tk_Offset(Tab, command),
	TK_CONFIG_NULL_OK, &bltUidOption},
    {TK_CONFIG_CUSTOM, "-data", "data", "data",
	DEF_TAB_DATA, Tk_Offset(Tab, data),
	TK_CONFIG_NULL_OK, &bltUidOption},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, "-fill", "fill", "Fill",
	DEF_TAB_FILL, Tk_Offset(Tab, fill),
	TK_CONFIG_DONT_SET_DEFAULT, &bltFillOption},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_TAB_FG, Tk_Offset(Tab, textColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_TAB_FONT, Tk_Offset(Tab, font), 0},
    {TK_CONFIG_CUSTOM, "-image", "image", "image",
	DEF_TAB_IMAGE, Tk_Offset(Tab, image),
	TK_CONFIG_NULL_OK, &imageOption},
    {TK_CONFIG_CUSTOM, "-ipadx", "iPadX", "PadX",
	DEF_TAB_IPAD, Tk_Offset(Tab, iPadX),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-ipady", "iPadY", "PadY",
	DEF_TAB_IPAD, Tk_Offset(Tab, iPadY),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-padx", "padX", "PadX",
	DEF_TAB_PAD, Tk_Offset(Tab, padX), 0, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-pady", "padY", "PadY",
	DEF_TAB_PAD, Tk_Offset(Tab, padY), 0, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-perforationcommand", "perforationcommand", 
	"PerforationCommand",
	DEF_TAB_PERF_COMMAND, Tk_Offset(Tab, perfCommand),
	TK_CONFIG_NULL_OK, &bltUidOption},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Background",
	DEF_TAB_SELECT_BG, Tk_Offset(Tab, selBorder), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Foreground",
	DEF_TAB_SELECT_FG, Tk_Offset(Tab, selColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-shadow", "shadow", "Shadow",
	DEF_TAB_SHADOW, Tk_Offset(Tab, shadow),
	TK_CONFIG_NULL_OK, &bltShadowOption},
    {TK_CONFIG_CUSTOM, "-state", "state", "State",
	DEF_TAB_STATE, Tk_Offset(Tab, state), 
	TK_CONFIG_DONT_SET_DEFAULT, &bltStateOption},
    {TK_CONFIG_BITMAP, "-stipple", "stipple", "Stipple",
	DEF_TAB_STIPPLE, Tk_Offset(Tab, stipple), 0},
    {TK_CONFIG_CUSTOM, "-tile", "tile", "Tile",
	(char *)NULL, Tk_Offset(Tab, tile), TK_CONFIG_NULL_OK,
	&bltTileOption},
    {TK_CONFIG_CUSTOM, "-text", "Text", "Text",
	DEF_TAB_TEXT, Tk_Offset(Tab, text),
	TK_CONFIG_NULL_OK, &bltUidOption},
    {TK_CONFIG_CUSTOM, "-window", "window", "Window",
	DEF_TAB_WINDOW, Tk_Offset(Tab, tkwin),
	TK_CONFIG_NULL_OK, &windowOption},
    {TK_CONFIG_CUSTOM, "-windowheight", "windowHeight", "WindowHeight",
	DEF_TAB_HEIGHT, Tk_Offset(Tab, reqHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-windowwidth", "windowWidth", "WindowWidth",
	DEF_TAB_WIDTH, Tk_Offset(Tab, reqWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

/*
 * TabAttributes --
 */
typedef struct {
    Tk_Window tkwin;		/* Default window to map pages. */

    int reqWidth, reqHeight;	/* Requested tab size. */
    int constWidth;
    int borderWidth;		/* Width of 3D border around the tab's
				 * label. */
    int pad;			/* Extra padding of a tab entry */

    XColor *activeFgColor;	/* Active foreground. */
    Tk_3DBorder activeBorder;	/* Active background. */
    XColor *selColor;		/* Selected foreground. */
    Tk_Font font;
    XColor *textColor;

    Tk_3DBorder border;		/* Normal background. */
    Tk_3DBorder selBorder;	/* Selected background. */

    Blt_Dashes dashes;
    GC normalGC, activeGC;
    int relief;
    char *command;
    char *perfCommand;		/* Command (malloc-ed) invoked when the tab
				 * is selected */
    double rotate;
    int textSide;

} TabAttributes;

struct NotebookStruct {
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

    unsigned int flags;		/* For bitfield definitions, see below */

    int inset;			/* Total width of all borders, including
				 * traversal highlight and 3-D border.
				 * Indicates how much interior stuff must
				 * be offset from outside edges to leave
				 * room for borders. */

    int inset2;			/* Total width of 3-D folder border + corner,
				 * Indicates how much interior stuff must
				 * be offset from outside edges of folder.*/

    int yPad;			/* Extra offset for selected tab. Only
				 * for single tiers. */

    int pageTop;		/* Offset from top of notebook to the
				 * start of the page. */

    Tk_Cursor cursor;		/* X Cursor */

    Tk_3DBorder border;		/* 3D border surrounding the window. */
    int borderWidth;		/* Width of 3D border. */
    int relief;			/* 3D border relief. */

    XColor *shadowColor;	/* Shadow color around folder. */
    /*
     * Focus highlight ring
     */
    int highlightWidth;		/* Width in pixels of highlight to draw
				 * around widget when it has the focus.
				 * <= 0 means don't draw a highlight. */
    XColor *highlightBgColor;	/* Color for drawing traversal highlight
				 * area when highlight is off. */
    XColor *highlightColor;	/* Color for drawing traversal highlight. */

    GC highlightGC;		/* GC for focus highlight. */

    char *takeFocus;		/* Says whether to select this widget during
				 * tab traveral operations.  This value isn't
				 * used in C code, but for the widget's Tcl
				 * bindings. */


    int side;			/* Orientation of the notebook: either
				 * SIDE_LEFT, SIDE_RIGHT, SIDE_TOP, or
				 * SIDE_BOTTOM. */

    int slant;
    int overlap;
    int gap;
    int tabWidth, tabHeight;
    int xSelectPad, ySelectPad;	/* Padding around label of the selected tab. */
    int outerPad;		/* Padding around the exterior of the notebook
				 * and folder. */

    TabAttributes defTabStyle;	/* Global attribute information specific to
				 * tabs. */
    Blt_Tile tile;

    int reqWidth, reqHeight;	/* Requested dimensions of the notebook
				 * window. */
    int pageWidth, pageHeight;	/* Dimensions of a page in the folder. */
    int reqPageWidth, reqPageHeight;	/* Requested dimensions of a page. */

    int lastX, lastY;
    /*
     * Scrolling information:
     */
    int worldWidth;
    int scrollOffset;		/* Offset of viewport in world coordinates. */
    char *scrollCmdPrefix;	/* Command strings to control scrollbar.*/

    int scrollUnits;		/* Smallest unit of scrolling for tabs. */

    /*
     * Scanning information:
     */
    int scanAnchor;		/* Scan anchor in screen coordinates. */
    int scanOffset;		/* Offset of the start of the scan in world
				 * coordinates.*/


    int corner;			/* Number of pixels to offset next point
				 * when drawing corners of the folder. */
    int reqTiers;		/* Requested number of tiers. Zero means to
				 * dynamically scroll if there are too many
				 * tabs to be display on a single tier. */
    int nTiers;			/* Actual number of tiers. */

    Blt_HashTable imageTable;


    Tab *selectPtr;		/* The currently selected tab.
				 * (i.e. its page is displayed). */

    Tab *activePtr;		/* Tab last located under the pointer.
				 * It is displayed with its active
				 * foreground/background colors.  */

    Tab *focusPtr;		/* Tab currently receiving focus. */

    Tab *startPtr;		/* The first tab on the first tier. */

    Blt_Chain *chainPtr;	/* List of tab entries. Used to
				 * arrange placement of tabs. */

    Blt_HashTable tabTable;	/* Hash table of tab entries. Used for
				 * lookups of tabs by name. */
    int nextId;

    int nVisible;		/* Number of tabs that are currently visible
				 * in the view port. */

    Blt_BindTable bindTable;	/* Tab binding information */
    Blt_HashTable tagTable;	/* Table of bind tags. */

    int tearoff;
};

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground",
	"activeBackground",
	DEF_TNB_ACTIVE_BACKGROUND, Tk_Offset(Notebook, defTabStyle.activeBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground",
	"activeBackground",
	DEF_TNB_ACTIVE_BG_MONO, Tk_Offset(Notebook, defTabStyle.activeBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground",
	"activeForeground", DEF_TNB_ACTIVE_FOREGROUND, 
	Tk_Offset(Notebook, defTabStyle.activeFgColor), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground",
	"activeForeground", DEF_TNB_ACTIVE_FG_MONO, 
	Tk_Offset(Notebook, defTabStyle.activeFgColor), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_TNB_BG_MONO, Tk_Offset(Notebook, border), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_TNB_BACKGROUND, Tk_Offset(Notebook, border), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_TNB_CURSOR, Tk_Offset(Notebook, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_TNB_BORDERWIDTH, Tk_Offset(Notebook, borderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-dashes", "dashes", "Dashes",
	DEF_TNB_DASHES, Tk_Offset(Notebook, defTabStyle.dashes),
	TK_CONFIG_NULL_OK, &bltDashesOption},
    {TK_CONFIG_SYNONYM, "-fg", "tabForeground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_TNB_FONT, Tk_Offset(Notebook, defTabStyle.font), 0},
    {TK_CONFIG_SYNONYM, "-foreground", "tabForeground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {TK_CONFIG_PIXELS, "-gap", "gap", "Gap",
	DEF_TNB_GAP, Tk_Offset(Notebook, gap),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-height", "height", "Height",
	DEF_TNB_HEIGHT, Tk_Offset(Notebook, reqHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground",
	DEF_TNB_HIGHLIGHT_BACKGROUND, Tk_Offset(Notebook, highlightBgColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground",
	DEF_TNB_HIGHLIGHT_BG_MONO, Tk_Offset(Notebook, highlightBgColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_TNB_HIGHLIGHT_COLOR, Tk_Offset(Notebook, highlightColor), 0},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_TNB_HIGHLIGHT_WIDTH, Tk_Offset(Notebook, highlightWidth),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-outerpad", "outerPad", "OuterPad",
	DEF_TNB_OUTER_PAD, Tk_Offset(Notebook, outerPad),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-pageheight", "pageHeight", "PageHeight",
	DEF_TNB_PAGE_HEIGHT, Tk_Offset(Notebook, reqPageHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-pagewidth", "pageWidth", "PageWidth",
	DEF_TNB_PAGE_WIDTH, Tk_Offset(Notebook, reqPageWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_STRING, "-perforationcommand", "perforationcommand", 
	"PerforationCommand",
	DEF_TAB_PERF_COMMAND, Tk_Offset(Notebook, defTabStyle.perfCommand),
	TK_CONFIG_NULL_OK, &bltUidOption},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_TNB_RELIEF, Tk_Offset(Notebook, relief), 0},
    {TK_CONFIG_DOUBLE, "-rotate", "rotate", "Rotate",
	DEF_TNB_ROTATE, Tk_Offset(Notebook, defTabStyle.rotate),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, "-samewidth", "sameWidth", "SameWidth",
	DEF_TNB_SAME_WIDTH, Tk_Offset(Notebook, defTabStyle.constWidth),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_STRING, "-scrollcommand", "scrollCommand", "ScrollCommand",
	(char *)NULL, Tk_Offset(Notebook, scrollCmdPrefix), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-scrollincrement", "scrollIncrement",
	"ScrollIncrement",
	DEF_TNB_SCROLL_INCREMENT, Tk_Offset(Notebook, scrollUnits),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPositiveDistanceOption},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Foreground",
	DEF_TNB_SELECT_BG_MONO, Tk_Offset(Notebook, defTabStyle.selBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Foreground",
	DEF_TNB_SELECT_BACKGROUND, Tk_Offset(Notebook, defTabStyle.selBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_STRING, "-selectcommand", "selectCommand", "SelectCommand",
	DEF_TNB_SELECT_CMD, Tk_Offset(Notebook, defTabStyle.command),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_TNB_SELECT_FG_MONO, Tk_Offset(Notebook, defTabStyle.selColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_TNB_SELECT_FOREGROUND, Tk_Offset(Notebook, defTabStyle.selColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_CUSTOM, "-selectpad", "selectPad", "SelectPad",
	DEF_TNB_SELECT_PAD, Tk_Offset(Notebook, xSelectPad),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_COLOR, "-shadowcolor", "shadowColor", "ShadowColor",
	DEF_TNB_SHADOW_COLOR, Tk_Offset(Notebook, shadowColor), 0},
    {TK_CONFIG_CUSTOM, "-side", "side", "side",
	DEF_TNB_SIDE, Tk_Offset(Notebook, side),
	TK_CONFIG_DONT_SET_DEFAULT, &sideOption},
    {TK_CONFIG_CUSTOM, "-slant", "slant", "Slant",
	DEF_TNB_SLANT, Tk_Offset(Notebook, slant),
	TK_CONFIG_DONT_SET_DEFAULT, &slantOption},
    {TK_CONFIG_BORDER, "-tabbackground", "tabBackground", "Background",
	DEF_TNB_TAB_BG_MONO, Tk_Offset(Notebook, defTabStyle.border),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-tabbackground", "tabBackground", "Background",
	DEF_TNB_TAB_BACKGROUND, Tk_Offset(Notebook, defTabStyle.border),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_CUSTOM, "-tabborderwidth", "tabBorderWidth", "BorderWidth",
	DEF_TNB_BORDERWIDTH, Tk_Offset(Notebook, defTabStyle.borderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_COLOR, "-tabforeground", "tabForeground", "Foreground",
	DEF_TNB_TEXT_COLOR, Tk_Offset(Notebook, defTabStyle.textColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-tabforeground", "tabForeground", "Foreground",
	DEF_TNB_TEXT_MONO, Tk_Offset(Notebook, defTabStyle.textColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_RELIEF, "-tabrelief", "tabRelief", "TabRelief",
	DEF_TNB_TAB_RELIEF, Tk_Offset(Notebook, defTabStyle.relief), 0},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_TNB_TAKE_FOCUS, Tk_Offset(Notebook, takeFocus), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-tearoff", "tearoff", "Tearoff",
	DEF_TNB_TEAROFF, Tk_Offset(Notebook, tearoff),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-textside", "textSide", "TextSide",
	DEF_TNB_TEXT_SIDE, Tk_Offset(Notebook, defTabStyle.textSide),
	TK_CONFIG_DONT_SET_DEFAULT, &sideOption},
    {TK_CONFIG_CUSTOM, "-tiers", "tiers", "Tiers",
	DEF_TNB_TIERS, Tk_Offset(Notebook, reqTiers),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPositiveCountOption},
    {TK_CONFIG_CUSTOM, "-tile", "tile", "Tile",
	(char *)NULL, Tk_Offset(Notebook, tile), TK_CONFIG_NULL_OK,
	&bltTileOption},
    {TK_CONFIG_CUSTOM, "-width", "width", "Width",
	DEF_TNB_WIDTH, Tk_Offset(Notebook, reqWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

/* Forward Declarations */
static void DestroyNotebook _ANSI_ARGS_((DestroyData dataPtr));
static void DestroyTearoff _ANSI_ARGS_((DestroyData dataPtr));
static void EmbeddedWidgetEventProc _ANSI_ARGS_((ClientData clientdata,
	XEvent *eventPtr));
static void TearoffEventProc _ANSI_ARGS_((ClientData clientdata,
	XEvent *eventPtr));
static void NotebookEventProc _ANSI_ARGS_((ClientData clientdata,
	XEvent *eventPtr));
static void DrawLabel _ANSI_ARGS_((Notebook *nbPtr, Tab *tabPtr,
	Drawable drawable));
static void DrawFolder _ANSI_ARGS_((Notebook *nbPtr, Tab *tabPtr,
	Drawable drawable));
static void DisplayNotebook _ANSI_ARGS_((ClientData clientData));
static void DisplayTearoff _ANSI_ARGS_((ClientData clientData));
static void NotebookInstDeletedCmd _ANSI_ARGS_((ClientData clientdata));
static int NotebookInstCmd _ANSI_ARGS_((ClientData clientdata,
	Tcl_Interp *interp, int argc, char **argv));
static void GetWindowRectangle _ANSI_ARGS_((Tab *tabPtr, Tk_Window parent,
	int tearOff, XRectangle *rectPtr));
static void ArrangeWindow _ANSI_ARGS_((Tk_Window tkwin, XRectangle *rectPtr,
	int force));
static void EventuallyRedraw _ANSI_ARGS_((Notebook *nbPtr));
static void EventuallyRedrawTearoff _ANSI_ARGS_((Tab *tabPtr));
static void ComputeLayout _ANSI_ARGS_((Notebook *nbPtr));
static void DrawOuterBorders _ANSI_ARGS_((Notebook *nbPtr, 
	Drawable drawable));

static Tk_ImageChangedProc ImageChangedProc;
static Blt_TileChangedProc TileChangedProc;
static Blt_BindTagProc GetTags;
static Blt_BindPickProc PickTab;
static Tcl_IdleProc AdoptWindow;
static Tcl_CmdProc NotebookCmd;

static ClientData
MakeTag(nbPtr, tagName)
    Notebook *nbPtr;
    char *tagName;
{
    Blt_HashEntry *hPtr;
    int isNew;

    hPtr = Blt_CreateHashEntry(&(nbPtr->tagTable), tagName, &isNew);
    assert(hPtr);
    return Blt_GetHashKey(&(nbPtr->tagTable), hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * WorldToScreen --
 *
 *	Converts world coordinates to screen coordinates. Note that
 *	the world view is always tabs up.
 *
 * Results:
 *	The screen coordinates are returned via *xScreenPtr and *yScreenPtr.
 *
 *----------------------------------------------------------------------
 */
static void
WorldToScreen(nbPtr, x, y, xScreenPtr, yScreenPtr)
    Notebook *nbPtr;
    int x, y;
    int *xScreenPtr, *yScreenPtr;
{
    int sx, sy;

    sx = sy = 0;		/* Suppress compiler warning. */

    /* Translate world X-Y to screen coordinates */
    /*
     * Note that the world X-coordinate is translated by the selected label's
     * X padding. This is done only to keep the scroll range is between
     * 0.0 and 1.0, rather adding/subtracting the pad in various locations.
     * It may be changed back in the future.
     */
    x += (nbPtr->inset + 
	  nbPtr->xSelectPad - 
	  nbPtr->scrollOffset);
    y += nbPtr->inset + nbPtr->yPad;

    switch (nbPtr->side) {
    case SIDE_TOP:
	sx = x, sy = y;		/* Do nothing */
	break;
    case SIDE_RIGHT:
	sx = Tk_Width(nbPtr->tkwin) - y;
	sy = x;
	break;
    case SIDE_LEFT:
	sx = y, sy = x;		/* Flip coordinates */
	break;
    case SIDE_BOTTOM:
	sx = x;
	sy = Tk_Height(nbPtr->tkwin) - y;
	break;
    }
    *xScreenPtr = sx;
    *yScreenPtr = sy;
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
EventuallyRedraw(nbPtr)
    Notebook *nbPtr;
{
    if ((nbPtr->tkwin != NULL) && !(nbPtr->flags & TNB_REDRAW)) {
	nbPtr->flags |= TNB_REDRAW;
	Tcl_DoWhenIdle(DisplayNotebook, nbPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * EventuallyRedrawTearoff --
 *
 *	Queues a request to redraw the tearoff at the next idle point.
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
EventuallyRedrawTearoff(tabPtr)
    Tab *tabPtr;
{
    if ((tabPtr->tkwin != NULL) && !(tabPtr->flags & TAB_REDRAW)) {
	tabPtr->flags |= TAB_REDRAW;
	Tcl_DoWhenIdle(DisplayTearoff, tabPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ImageChangedProc
 *
 *	This routine is called whenever an image displayed in a tab
 *	changes.  In this case, we assume that everything will change
 *	and queue a request to re-layout and redraw the entire notebook.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static void
ImageChangedProc(clientData, x, y, width, height, imageWidth, imageHeight)
    ClientData clientData;
    int x, y, width, height;	/* Not used. */
    int imageWidth, imageHeight;/* Not used. */
{
    Notebook *nbPtr = clientData;

    nbPtr->flags |= (TNB_LAYOUT | TNB_SCROLL);
    EventuallyRedraw(nbPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * GetImage --
 *
 *	This is a wrapper procedure for Tk_GetImage. The problem is
 *	that if the same image is used repeatedly in the same widget,
 *	the separate instances are saved in a linked list.  This makes
 *	it especially slow to destroy the widget.  As a workaround,
 *	this routine hashes the image and maintains a reference count
 *	for it.
 *
 * Results:
 *	Returns a pointer to the new image.
 *
 *----------------------------------------------------------------------
 */
static TabImage
GetImage(nbPtr, interp, tkwin, name)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    Tk_Window tkwin;
    char *name;
{
    struct TabImageStruct *imagePtr;
    int isNew;
    Blt_HashEntry *hPtr;

    hPtr = Blt_CreateHashEntry(&(nbPtr->imageTable), name, &isNew);
    if (isNew) {
	Tk_Image tkImage;
	int width, height;

	tkImage = Tk_GetImage(interp, tkwin, name, ImageChangedProc, nbPtr);
	if (tkImage == NULL) {
	    Blt_DeleteHashEntry(&(nbPtr->imageTable), hPtr);
	    return NULL;
	}
	Tk_SizeOfImage(tkImage, &width, &height);
	imagePtr = Blt_Malloc(sizeof(struct TabImageStruct));
	imagePtr->tkImage = tkImage;
	imagePtr->hashPtr = hPtr;
	imagePtr->refCount = 1;
	imagePtr->width = width;
	imagePtr->height = height;
	Blt_SetHashValue(hPtr, imagePtr);
    } else {
	imagePtr = (struct TabImageStruct *)Blt_GetHashValue(hPtr);
	imagePtr->refCount++;
    }
    return imagePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeImage --
 *
 *	Releases the image if it's not being used anymore by this
 *	widget.  Note there may be several uses of the same image
 *	by many tabs.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The reference count is decremented and the image is freed
 *	is it's not being used anymore.
 *
 *----------------------------------------------------------------------
 */
static void
FreeImage(nbPtr, imagePtr)
    Notebook *nbPtr;
    struct TabImageStruct *imagePtr;
{
    imagePtr->refCount--;
    if (imagePtr->refCount == 0) {
	Blt_DeleteHashEntry(&(nbPtr->imageTable), imagePtr->hashPtr);
	Tk_FreeImage(imagePtr->tkImage);
	Blt_Free(imagePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * StringToImage --
 *
 *	Converts an image name into a Tk image token.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToImage(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Contains a pointer to the notebook containing
				 * this image. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window associated with the notebook. */
    char *string;		/* String representation */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset to field in structure */
{
    Notebook *nbPtr = *(Notebook **)clientData;
    TabImage *imagePtr = (TabImage *) (widgRec + offset);
    TabImage image;

    image = NULL;
    if ((string != NULL) && (*string != '\0')) {
	image = GetImage(nbPtr, interp, tkwin, string);
	if (image == NULL) {
	    return TCL_ERROR;
	}
    }
    if (*imagePtr != NULL) {
	FreeImage(nbPtr, *imagePtr);
    }
    *imagePtr = image;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageToString --
 *
 *	Converts the Tk image back to its string representation (i.e.
 *	its name).
 *
 * Results:
 *	The name of the image is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
ImageToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Pointer to notebook containing image. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of field in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    Notebook *nbPtr = *(Notebook **)clientData;
    TabImage *imagePtr = (TabImage *) (widgRec + offset);

    if (*imagePtr == NULL) {
	return "";
    }
    return Blt_GetHashKey(&(nbPtr->imageTable), (*imagePtr)->hashPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * StringToWindow --
 *
 *	Converts a window name into Tk window.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToWindow(clientData, interp, parent, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window parent;		/* Parent window */
    char *string;		/* String representation. */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset to field in structure */
{
    Tab *tabPtr = (Tab *)widgRec;
    Tk_Window *tkwinPtr = (Tk_Window *)(widgRec + offset);
    Tk_Window old, tkwin;
    Notebook *nbPtr;

    old = *tkwinPtr;
    tkwin = NULL;
    nbPtr = tabPtr->nbPtr;
    if ((string != NULL) && (*string != '\0')) {
	tkwin = Tk_NameToWindow(interp, string, parent);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	if (tkwin == old) {
	    return TCL_OK;
	}
	/*
	 * Allow only widgets that are children of the notebook to be
	 * embedded into the page.  This way we can make assumptions about
	 * the window based upon its parent; either it's the notebook window
	 * or it has been torn off.
	 */
	parent = Tk_Parent(tkwin);
	if (parent != nbPtr->tkwin) {
	    Tcl_AppendResult(interp, "can't manage \"", Tk_PathName(tkwin),
		"\" in notebook \"", Tk_PathName(nbPtr->tkwin), "\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
	Tk_ManageGeometry(tkwin, &tabMgrInfo, tabPtr);
	Tk_CreateEventHandler(tkwin, StructureNotifyMask, 
		EmbeddedWidgetEventProc, tabPtr);

	/*
	 * We need to make the window to exist immediately.  If the
	 * window is torn off (placed into another container window),
	 * the timing between the container and the its new child
	 * (this window) gets tricky.  This should work for Tk 4.2.
	 */
	Tk_MakeWindowExist(tkwin);
    }
    if (old != NULL) {
	if (tabPtr->container != NULL) {
	    Tcl_EventuallyFree(tabPtr, DestroyTearoff);
	}
	Tk_DeleteEventHandler(old, StructureNotifyMask, 
	      EmbeddedWidgetEventProc, tabPtr);
	Tk_ManageGeometry(old, (Tk_GeomMgr *) NULL, tabPtr);
	Tk_UnmapWindow(old);
    }
    *tkwinPtr = tkwin;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WindowToString --
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
WindowToString(clientData, parent, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window parent;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of field in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    Tk_Window tkwin = *(Tk_Window *)(widgRec + offset);

    if (tkwin == NULL) {
	return "";
    }
    return Tk_PathName(tkwin);
}

/*
 *----------------------------------------------------------------------
 *
 * StringToSide --
 *
 *	Converts "left", "right", "top", "bottom", into a numeric token
 *	designating the side of the notebook which to display tabs.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED */
static int
StringToSide(clientData, interp, parent, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window parent;		/* Parent window */
    char *string;		/* Option value string */
    char *widgRec;		/* Widget record */
    int offset;			/* offset to field in structure */
{
    int *sidePtr = (int *)(widgRec + offset);
    char c;
    unsigned int length;

    c = string[0];
    length = strlen(string);
    if ((c == 'l') && (strncmp(string, "left", length) == 0)) {
	*sidePtr = SIDE_LEFT;
    } else if ((c == 'r') && (strncmp(string, "right", length) == 0)) {
	*sidePtr = SIDE_RIGHT;
    } else if ((c == 't') && (strncmp(string, "top", length) == 0)) {
	*sidePtr = SIDE_TOP;
    } else if ((c == 'b') && (strncmp(string, "bottom", length) == 0)) {
	*sidePtr = SIDE_BOTTOM;
    } else {
	Tcl_AppendResult(interp, "bad side \"", string,
	    "\": should be left, right, top, or bottom", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SideToString --
 *
 *	Converts the window into its string representation (its name).
 *
 * Results:
 *	The name of the window is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
SideToString(clientData, parent, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window parent;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* offset of windows array in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    int side = *(int *)(widgRec + offset);

    switch (side) {
    case SIDE_LEFT:
	return "left";
    case SIDE_RIGHT:
	return "right";
    case SIDE_BOTTOM:
	return "bottom";
    case SIDE_TOP:
	return "top";
    }
    return "unknown side value";
}

/*
 *----------------------------------------------------------------------
 *
 * StringToSlant --
 *
 *	Converts the slant style string into its numeric representation.
 *
 *	Valid style strings are:
 *
 *	  "none"   Both sides are straight.
 * 	  "left"   Left side is slanted.
 *	  "right"  Right side is slanted.
 *	  "both"   Both sides are slanted.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToSlant(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* String representation of attribute. */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of field in widget record. */
{
    int *slantPtr = (int *)(widgRec + offset);
    unsigned int length;
    char c;

    c = string[0];
    length = strlen(string);
    if ((c == 'n') && (strncmp(string, "none", length) == 0)) {
	*slantPtr = SLANT_NONE;
    } else if ((c == 'l') && (strncmp(string, "left", length) == 0)) {
	*slantPtr = SLANT_LEFT;
    } else if ((c == 'r') && (strncmp(string, "right", length) == 0)) {
	*slantPtr = SLANT_RIGHT;
    } else if ((c == 'b') && (strncmp(string, "both", length) == 0)) {
	*slantPtr = SLANT_BOTH;
    } else {
	Tcl_AppendResult(interp, "bad argument \"", string,
	    "\": should be \"none\", \"left\", \"right\", or \"both\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SlantToString --
 *
 *	Returns the slant style string based upon the slant flags.
 *
 * Results:
 *	The slant style string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
SlantToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget structure record. */
    int offset;			/* Offset of field in widget record. */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    int slant = *(int *)(widgRec + offset);

    switch (slant) {
    case SLANT_LEFT:
	return "left";
    case SLANT_RIGHT:
	return "right";
    case SLANT_NONE:
	return "none";
    case SLANT_BOTH:
	return "both";
    default:
	return "unknown value";
    }
}


static int
WorldY(tabPtr)
    Tab *tabPtr;
{
    int tier;

    tier = tabPtr->nbPtr->nTiers - tabPtr->tier;
    return tier * tabPtr->nbPtr->tabHeight;
}

static int
TabIndex(nbPtr, tabPtr) 
    Notebook *nbPtr;
    Tab *tabPtr;
{
    Tab *t2Ptr;
    int count;
    Blt_ChainLink *linkPtr;
    
    count = 0;
    for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	t2Ptr = Blt_ChainGetValue(linkPtr);
	if (t2Ptr == tabPtr) {
	    return count;
	}
	count++;
    }
    return -1;
}

/*
 * ----------------------------------------------------------------------
 *
 * RenumberTiers --
 *
 *	In multi-tier mode, we need to find the start of the tier
 *	containing the newly selected tab.
 *
 *	Tiers are draw from the last tier to the first, so that
 *	the the lower-tiered tabs will partially cover the bottoms
 *	of tab directly above it.  This simplifies the drawing of
 *	tabs because we don't worry how tabs are clipped by their
 *	neighbors.
 *
 *	In addition, tabs are re-marked with the correct tier number.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Renumbering the tab's tier will change the vertical placement
 *	of the tab (i.e. shift tiers).
 *
 * ----------------------------------------------------------------------
 */
static void
RenumberTiers(nbPtr, tabPtr)
    Notebook *nbPtr;
    Tab *tabPtr;
{
    int tier;
    Tab *prevPtr;
    Blt_ChainLink *linkPtr, *lastPtr;

    nbPtr->focusPtr = nbPtr->selectPtr = tabPtr;
    Blt_SetFocusItem(nbPtr->bindTable, nbPtr->focusPtr, NULL);

    tier = tabPtr->tier;
    for (linkPtr = Blt_ChainPrevLink(tabPtr->linkPtr); linkPtr != NULL;
	linkPtr = lastPtr) {
	lastPtr = Blt_ChainPrevLink(linkPtr);
	prevPtr = Blt_ChainGetValue(linkPtr);
	if ((prevPtr == NULL) || (prevPtr->tier != tier)) {
	    break;
	}
	tabPtr = prevPtr;
    }
    nbPtr->startPtr = tabPtr;
    for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	tabPtr = Blt_ChainGetValue(linkPtr);
	tabPtr->tier = (tabPtr->tier - tier + 1);
	if (tabPtr->tier < 1) {
	    tabPtr->tier += nbPtr->nTiers;
	}
	tabPtr->worldY = WorldY(tabPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PickTab --
 *
 *	Searches the tab located within the given screen X-Y coordinates
 *	in the viewport.  Note that tabs overlap slightly, so that its
 *	important to search from the innermost tier out.
 *
 * Results:
 *	Returns the pointer to the tab.  If the pointer isn't contained
 *	by any tab, NULL is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static ClientData
PickTab(clientData, x, y, contextPtr)
    ClientData clientData;
    int x, y;			/* Screen coordinates to test. */
    ClientData *contextPtr;	
{
    Notebook *nbPtr = clientData;
    Tab *tabPtr;
    Blt_ChainLink *linkPtr;

    if (contextPtr != NULL) {
	*contextPtr = NULL;
    }
    tabPtr = nbPtr->selectPtr;
    if ((nbPtr->tearoff) && (tabPtr != NULL) && 
	(tabPtr->container == NULL) && (tabPtr->tkwin != NULL)) {
	int top, bottom, left, right;
	int sx, sy;

	/* Check first for perforation on the selected tab. */
	WorldToScreen(nbPtr, tabPtr->worldX + 2, 
	      tabPtr->worldY + tabPtr->worldHeight + 4, &sx, &sy);
	if (nbPtr->side & SIDE_HORIZONTAL) {
	    left = sx - 2;
	    right = left + tabPtr->screenWidth;
	    top = sy - 4;
	    bottom = sy + 4;
	} else {
	    left = sx - 4;
	    right = sx + 4;
	    top = sy - 2;
	    bottom = top + tabPtr->screenHeight;
	}
	if ((x >= left) && (y >= top) && (x < right) && (y < bottom)) {
	    if (contextPtr != NULL) {
		*contextPtr = TAB_PERFORATION;
	    }
	    return nbPtr->selectPtr;
	}
    } 
    for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	tabPtr = Blt_ChainGetValue(linkPtr);
	if (!(tabPtr->flags & TAB_VISIBLE)) {
	    continue;
	}
	if ((x >= tabPtr->screenX) && (y >= tabPtr->screenY) &&
	    (x <= (tabPtr->screenX + tabPtr->screenWidth)) &&
	    (y < (tabPtr->screenY + tabPtr->screenHeight))) {
	    if (contextPtr != NULL) {
		*contextPtr = TAB_LABEL;
	    }
	    return tabPtr;
	}
    }
    return NULL;
}

static Tab *
TabLeft(tabPtr)
    Tab *tabPtr;
{
    Blt_ChainLink *linkPtr;

    linkPtr = Blt_ChainPrevLink(tabPtr->linkPtr);
    if (linkPtr != NULL) {
	Tab *newPtr;

	newPtr = Blt_ChainGetValue(linkPtr);
	/* Move only if the next tab is on another tier. */
	if (newPtr->tier == tabPtr->tier) {
	    tabPtr = newPtr;
	}
    }
    return tabPtr;
}

static Tab *
TabRight(tabPtr)
    Tab *tabPtr;
{
    Blt_ChainLink *linkPtr;

    linkPtr = Blt_ChainNextLink(tabPtr->linkPtr);
    if (linkPtr != NULL) {
	Tab *newPtr;

	newPtr = Blt_ChainGetValue(linkPtr);
	/* Move only if the next tab is on another tier. */
	if (newPtr->tier == tabPtr->tier) {
	    tabPtr = newPtr;
	}
    }
    return tabPtr;
}

static Tab *
TabUp(tabPtr)
    Tab *tabPtr;
{
    if (tabPtr != NULL) {
	Notebook *nbPtr;
	int x, y;
	int worldX, worldY;
	
	nbPtr = tabPtr->nbPtr;
	worldX = tabPtr->worldX + (tabPtr->worldWidth / 2);
	worldY = tabPtr->worldY - (nbPtr->tabHeight / 2);
	WorldToScreen(nbPtr, worldX, worldY, &x, &y);
	
	tabPtr = (Tab *)PickTab(nbPtr, x, y, NULL);
	if (tabPtr == NULL) {
	    /*
	     * We might have inadvertly picked the gap between two tabs,
	     * so if the first pick fails, try again a little to the left.
	     */
	    WorldToScreen(nbPtr, worldX + nbPtr->gap, worldY, &x, &y);
	    tabPtr = (Tab *)PickTab(nbPtr, x, y, NULL);
	}
	if ((tabPtr == NULL) &&
	    (nbPtr->focusPtr->tier < (nbPtr->nTiers - 1))) {
	    worldY -= nbPtr->tabHeight;
	    WorldToScreen(nbPtr, worldX, worldY, &x, &y);
	    tabPtr = (Tab *)PickTab(nbPtr, x, y, NULL);
	}
	if (tabPtr == NULL) {
	    tabPtr = nbPtr->focusPtr;
	}
    }
    return tabPtr;
}

static Tab *
TabDown(tabPtr)
    Tab *tabPtr;
{
    if (tabPtr != NULL) {
	Notebook *nbPtr;
	int x, y;
	int worldX, worldY;

	nbPtr = tabPtr->nbPtr;
	worldX = tabPtr->worldX + (tabPtr->worldWidth / 2);
	worldY = tabPtr->worldY + (3 * nbPtr->tabHeight) / 2;
	WorldToScreen(nbPtr, worldX, worldY, &x, &y);
	tabPtr = (Tab *)PickTab(nbPtr, x, y, NULL);
	if (tabPtr == NULL) {
	    /*
	     * We might have inadvertly picked the gap between two tabs,
	     * so if the first pick fails, try again a little to the left.
	     */
	    WorldToScreen(nbPtr, worldX - nbPtr->gap, worldY, &x, &y);
	    tabPtr = (Tab *)PickTab(nbPtr, x, y, NULL);
	}
	if ((tabPtr == NULL) && (nbPtr->focusPtr->tier > 2)) {
	    worldY += nbPtr->tabHeight;
	    WorldToScreen(nbPtr, worldX, worldY, &x, &y);
	    tabPtr = (Tab *)PickTab(nbPtr, x, y, NULL);
	}
	if (tabPtr == NULL) {
	    tabPtr = nbPtr->focusPtr;
	}
    }
    return tabPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * GetTab --
 *
 *	Converts a string representing a tab index into a tab pointer.
 *	The index may be in one of the following forms:
 *
 *	 number		Tab at position in the list of tabs.
 *	 @x,y		Tab closest to the specified X-Y screen coordinates.
 *	 "active"	Tab mouse is located over.
 *	 "focus"	Tab is the widget's focus.
 *	 "select"	Currently selected tab.
 *	 "right"	Next tab from the focus tab.
 *	 "left"		Previous tab from the focus tab.
 *	 "up"		Next tab from the focus tab.
 *	 "down"		Previous tab from the focus tab.
 *	 "end"		Last tab in list.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	The pointer to the node is returned via tabPtrPtr.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
static int
GetTab(nbPtr, string, tabPtrPtr, allowNull)
    Notebook *nbPtr;
    char *string;
    Tab **tabPtrPtr;
    int allowNull;	/* Allow NULL tabPtr */
{
    Tab *tabPtr;
    Blt_ChainLink *linkPtr;
    int position;
    char c;

    c = string[0];
    tabPtr = NULL;
    if (nbPtr->focusPtr == NULL) {
	nbPtr->focusPtr = nbPtr->selectPtr;
	Blt_SetFocusItem(nbPtr->bindTable, nbPtr->focusPtr, NULL);
    }
    if ((isdigit(UCHAR(c))) &&
	(Tcl_GetInt(nbPtr->interp, string, &position) == TCL_OK)) {
	linkPtr = Blt_ChainGetNthLink(nbPtr->chainPtr, position);
	if (linkPtr == NULL) {
	    Tcl_AppendResult(nbPtr->interp, "can't find tab \"", string,
		"\" in \"", Tk_PathName(nbPtr->tkwin), 
		"\": no such index", (char *)NULL);
	    return TCL_ERROR;
	}
	tabPtr = Blt_ChainGetValue(linkPtr);
    } else if ((c == 'a') && (strcmp(string, "active") == 0)) {
	tabPtr = nbPtr->activePtr;
    } else if ((c == 'c') && (strcmp(string, "current") == 0)) {
	tabPtr = (Tab *)Blt_GetCurrentItem(nbPtr->bindTable);
    } else if ((c == 's') && (strcmp(string, "select") == 0)) {
	tabPtr = nbPtr->selectPtr;
    } else if ((c == 'f') && (strcmp(string, "focus") == 0)) {
	tabPtr = nbPtr->focusPtr;
    } else if ((c == 'u') && (strcmp(string, "up") == 0)) {
	switch (nbPtr->side) {
	case SIDE_LEFT:
	case SIDE_RIGHT:
	    tabPtr = TabLeft(nbPtr->focusPtr);
	    break;
	    
	case SIDE_BOTTOM:
	    tabPtr = TabDown(nbPtr->focusPtr);
	    break;
	    
	case SIDE_TOP:
	    tabPtr = TabUp(nbPtr->focusPtr);
	    break;
	}
    } else if ((c == 'd') && (strcmp(string, "down") == 0)) {
	switch (nbPtr->side) {
	case SIDE_LEFT:
	case SIDE_RIGHT:
	    tabPtr = TabRight(nbPtr->focusPtr);
	    break;
	    
	case SIDE_BOTTOM:
	    tabPtr = TabUp(nbPtr->focusPtr);
	    break;
	    
	case SIDE_TOP:
	    tabPtr = TabDown(nbPtr->focusPtr);
	    break;
	}
    } else if ((c == 'l') && (strcmp(string, "left") == 0)) {
	switch (nbPtr->side) {
	case SIDE_LEFT:
	    tabPtr = TabUp(nbPtr->focusPtr);
	    break;
	    
	case SIDE_RIGHT:
	    tabPtr = TabDown(nbPtr->focusPtr);
	    break;
	    
	case SIDE_BOTTOM:
	case SIDE_TOP:
	    tabPtr = TabLeft(nbPtr->focusPtr);
	    break;
	}
    } else if ((c == 'r') && (strcmp(string, "right") == 0)) {
	switch (nbPtr->side) {
	case SIDE_LEFT:
	    tabPtr = TabDown(nbPtr->focusPtr);
	    break;
	    
	case SIDE_RIGHT:
	    tabPtr = TabUp(nbPtr->focusPtr);
	    break;
	    
	case SIDE_BOTTOM:
	case SIDE_TOP:
	    tabPtr = TabRight(nbPtr->focusPtr);
	    break;
	}
    } else if ((c == 'e') && (strcmp(string, "end") == 0)) {
	linkPtr = Blt_ChainLastLink(nbPtr->chainPtr);
	if (linkPtr != NULL) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	}
    } else if (c == '@') {
	int x, y;

	if (Blt_GetXY(nbPtr->interp, nbPtr->tkwin, string, &x, &y) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	tabPtr = (Tab *)PickTab(nbPtr, x, y, NULL);
    } else {
	Blt_HashEntry *hPtr;

	hPtr = Blt_FindHashEntry(&(nbPtr->tabTable), string);
	if (hPtr != NULL) {
	    tabPtr = (Tab *)Blt_GetHashValue(hPtr);
	}
    }
    *tabPtrPtr = tabPtr;
    Tcl_ResetResult(nbPtr->interp);

    if ((!allowNull) && (tabPtr == NULL)) {
	Tcl_AppendResult(nbPtr->interp, "can't find tab \"", string,
	    "\" in \"", Tk_PathName(nbPtr->tkwin), "\"", (char *)NULL);
	return TCL_ERROR;
    }	
    return TCL_OK;
}

static Tab *
NextOrLastTab(tabPtr)
    Tab *tabPtr;
{
    if (tabPtr->linkPtr != NULL) {
	Blt_ChainLink *linkPtr;

	linkPtr = Blt_ChainNextLink(tabPtr->linkPtr);
	if (linkPtr == NULL) {
	    linkPtr = Blt_ChainPrevLink(tabPtr->linkPtr);
	}
	if (linkPtr != NULL) {
	    return Blt_ChainGetValue(linkPtr);
	}
    }
    return NULL;
}

/*
 * --------------------------------------------------------------
 *
 * EmbeddedWidgetEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on embedded widgets contained in the notebook.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When an embedded widget gets deleted, internal structures get
 *	cleaned up.  When it gets resized, the notebook is redisplayed.
 *
 * --------------------------------------------------------------
 */
static void
EmbeddedWidgetEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about the tab window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Tab *tabPtr = clientData;

    if ((tabPtr == NULL) || (tabPtr->tkwin == NULL)) {
	return;
    }
    switch (eventPtr->type) {
    case ConfigureNotify:
	/*
	 * If the window's requested size changes, redraw the window.
	 * But only if it's currently the selected page.
	 */
	if ((tabPtr->container == NULL) && (Tk_IsMapped(tabPtr->tkwin)) &&
	    (tabPtr->nbPtr->selectPtr == tabPtr)) {
	    EventuallyRedraw(tabPtr->nbPtr);
	}
	break;

    case DestroyNotify:
	/*
	 * Mark the tab as deleted by dereferencing the Tk window
	 * pointer. Redraw the window only if the tab is currently
	 * visible.
	 */
	if ((Tk_IsMapped(tabPtr->tkwin)) &&
	    (tabPtr->nbPtr->selectPtr == tabPtr)) {
	    EventuallyRedraw(tabPtr->nbPtr);
	}
	Tk_DeleteEventHandler(tabPtr->tkwin, StructureNotifyMask,
	    EmbeddedWidgetEventProc, tabPtr);
	tabPtr->tkwin = NULL;
	break;

    }
}

/*
 * ----------------------------------------------------------------------
 *
 * EmbeddedWidgetCustodyProc --
 *
 *	This procedure is invoked when a tab window has been
 *	stolen by another geometry manager.  The information and
 *	memory associated with the tab window is released.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the widget formerly associated with the tab
 *	window to have its layout re-computed and arranged at the
 *	next idle point.
 *
 * ---------------------------------------------------------------------
 */
 /* ARGSUSED */
static void
EmbeddedWidgetCustodyProc(clientData, tkwin)
    ClientData clientData;	/* Information about the former tab window. */
    Tk_Window tkwin;		/* Not used. */
{
    Tab *tabPtr = clientData;
    Notebook *nbPtr;

    if ((tabPtr == NULL) || (tabPtr->tkwin == NULL)) {
	return;
    }
    nbPtr = tabPtr->nbPtr;
    if (tabPtr->container != NULL) {
	Tcl_EventuallyFree(tabPtr, DestroyTearoff);
    }
    /*
     * Mark the tab as deleted by dereferencing the Tk window
     * pointer. Redraw the window only if the tab is currently
     * visible.
     */
    if (tabPtr->tkwin != NULL) {
	if (Tk_IsMapped(tabPtr->tkwin) && (nbPtr->selectPtr == tabPtr)) {
	    nbPtr->flags |= (TNB_LAYOUT | TNB_SCROLL);
	    EventuallyRedraw(nbPtr);
	}
	Tk_DeleteEventHandler(tabPtr->tkwin, StructureNotifyMask,
	    EmbeddedWidgetEventProc, tabPtr);
	tabPtr->tkwin = NULL;
    }
}

/*
 * -------------------------------------------------------------------------
 *
 * EmbeddedWidgetGeometryProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for tab
 *	windows managed by the widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for tkwin, and all its managed siblings, to be
 *	repacked and drawn at the next idle point.
 *
 * ------------------------------------------------------------------------
 */
 /* ARGSUSED */
static void
EmbeddedWidgetGeometryProc(clientData, tkwin)
    ClientData clientData;	/* Information about window that got new
			         * preferred geometry.  */
    Tk_Window tkwin;		/* Other Tk-related information about the
			         * window. */
{
    Tab *tabPtr = clientData;

    if ((tabPtr == NULL) || (tabPtr->tkwin == NULL)) {
	fprintf(stderr, "%s: line %d \"tkwin is null\"", __FILE__, __LINE__);
	return;
    }
    tabPtr->nbPtr->flags |= (TNB_LAYOUT | TNB_SCROLL);
    EventuallyRedraw(tabPtr->nbPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyTab --
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyTab(nbPtr, tabPtr)
    Notebook *nbPtr;
    Tab *tabPtr;
{
    Blt_HashEntry *hPtr;

    if (tabPtr->flags & TAB_REDRAW) {
	Tcl_CancelIdleCall(DisplayTearoff, tabPtr);
    }
    if (tabPtr->container != NULL) {
	Tk_DestroyWindow(tabPtr->container);
    }
    if (tabPtr->tkwin != NULL) {
	Tk_ManageGeometry(tabPtr->tkwin, (Tk_GeomMgr *)NULL, tabPtr);
	Tk_DeleteEventHandler(tabPtr->tkwin, StructureNotifyMask, 
		EmbeddedWidgetEventProc, tabPtr);
	if (Tk_IsMapped(tabPtr->tkwin)) {
	    Tk_UnmapWindow(tabPtr->tkwin);
	}
    }
    if (tabPtr == nbPtr->activePtr) {
	nbPtr->activePtr = NULL;
    }
    if (tabPtr == nbPtr->selectPtr) {
	nbPtr->selectPtr = NextOrLastTab(tabPtr);
    }
    if (tabPtr == nbPtr->focusPtr) {
	nbPtr->focusPtr = nbPtr->selectPtr;
	Blt_SetFocusItem(nbPtr->bindTable, nbPtr->focusPtr, NULL);
    }
    if (tabPtr == nbPtr->startPtr) {
	nbPtr->startPtr = NULL;
    }
    Tk_FreeOptions(tabConfigSpecs, (char *)tabPtr, nbPtr->display, 0);
    if (tabPtr->text != NULL) {
	Blt_FreeUid(tabPtr->text);
    }
    hPtr = Blt_FindHashEntry(&(nbPtr->tabTable), tabPtr->name);
    assert(hPtr);
    Blt_DeleteHashEntry(&(nbPtr->tabTable), hPtr);

    if (tabPtr->image != NULL) {
	FreeImage(nbPtr, tabPtr->image);
    }
    if (tabPtr->name != NULL) {
	Blt_Free(tabPtr->name);
    }
    if (tabPtr->textGC != NULL) {
	Tk_FreeGC(nbPtr->display, tabPtr->textGC);
    }
    if (tabPtr->backGC != NULL) {
	Tk_FreeGC(nbPtr->display, tabPtr->backGC);
    }
    if (tabPtr->command != NULL) {
	Blt_FreeUid(tabPtr->command);
    }
    if (tabPtr->linkPtr != NULL) {
	Blt_ChainDeleteLink(nbPtr->chainPtr, tabPtr->linkPtr);
    }
    if (tabPtr->tags != NULL) {
	Blt_FreeUid(tabPtr->tags);
    }
    Blt_DeleteBindings(nbPtr->bindTable, tabPtr);
    Blt_Free(tabPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * CreateTab --
 *
 *	Creates a new tab structure.  A tab contains information about
 *	the state of the tab and its embedded window.
 *
 * Results:
 *	Returns a pointer to the new tab structure.
 *
 * ----------------------------------------------------------------------
 */
static Tab *
CreateTab(nbPtr)
    Notebook *nbPtr;
{
    Tab *tabPtr;
    Blt_HashEntry *hPtr;
    int isNew;
    char string[200];

    tabPtr = Blt_Calloc(1, sizeof(Tab));
    assert(tabPtr);
    tabPtr->nbPtr = nbPtr;
    sprintf(string, "tab%d", nbPtr->nextId++);
    tabPtr->name = Blt_Strdup(string);
    tabPtr->text = Blt_GetUid(string);
    tabPtr->fill = FILL_NONE;
    tabPtr->anchor = TK_ANCHOR_CENTER;
    tabPtr->container = NULL;
    tabPtr->state = STATE_NORMAL;
    hPtr = Blt_CreateHashEntry(&(nbPtr->tabTable), string, &isNew);
    Blt_SetHashValue(hPtr, tabPtr);
    return tabPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TileChangedProc
 *
 *	Stub for image change notifications.  Since we immediately draw
 *	the image into a pixmap, we don't really care about image changes.
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
    Notebook *nbPtr = clientData;

    if (nbPtr->tkwin != NULL) {
	EventuallyRedraw(nbPtr);
    }
}

static int
ConfigureTab(nbPtr, tabPtr)
    Notebook *nbPtr;
    Tab *tabPtr;
{
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;
    int labelWidth, labelHeight;
    Tk_Font font;
    Tk_3DBorder border;

    font = GETATTR(tabPtr, font);
    labelWidth = labelHeight = 0;
    if (tabPtr->text != NULL) {
	TextStyle ts;
	double rotWidth, rotHeight;

	Blt_InitTextStyle(&ts);
	ts.font = font;
	ts.shadow.offset = tabPtr->shadow.offset;
	ts.padX.side1 = ts.padX.side2 = 2;
	Blt_GetTextExtents(&ts, tabPtr->text, &labelWidth, &labelHeight);
	Blt_GetBoundingBox(labelWidth, labelHeight, nbPtr->defTabStyle.rotate,
	    &rotWidth, &rotHeight, (Point2D *)NULL);
	labelWidth = ROUND(rotWidth);
	labelHeight = ROUND(rotHeight);
    }
    tabPtr->textWidth = (short int)labelWidth;
    tabPtr->textHeight = (short int)labelHeight;
    if (tabPtr->image != NULL) {
	int width, height;

	width = ImageWidth(tabPtr->image) + 2 * IMAGE_PAD;
	height = ImageHeight(tabPtr->image) + 2 * IMAGE_PAD;
	if (nbPtr->defTabStyle.textSide & SIDE_VERTICAL) {
	    labelWidth += width;
	    labelHeight = MAX(labelHeight, height);
	} else {
	    labelHeight += height;
	    labelWidth = MAX(labelWidth, width);
	}
    }
    labelWidth += PADDING(tabPtr->iPadX);
    labelHeight += PADDING(tabPtr->iPadY);

    tabPtr->labelWidth = ODD(labelWidth);
    tabPtr->labelHeight = ODD(labelHeight);

    newGC = NULL;
    if (tabPtr->text != NULL) {
	XColor *colorPtr;

	gcMask = GCForeground | GCFont;
	colorPtr = GETATTR(tabPtr, textColor);
	gcValues.foreground = colorPtr->pixel;
	gcValues.font = Tk_FontId(font);
	newGC = Tk_GetGC(nbPtr->tkwin, gcMask, &gcValues);
    }
    if (tabPtr->textGC != NULL) {
	Tk_FreeGC(nbPtr->display, tabPtr->textGC);
    }
    tabPtr->textGC = newGC;

    gcMask = GCForeground | GCStipple | GCFillStyle;
    gcValues.fill_style = FillStippled;
    border = GETATTR(tabPtr, border);
    gcValues.foreground = Tk_3DBorderColor(border)->pixel;
    gcValues.stipple = tabPtr->stipple;
    newGC = Tk_GetGC(nbPtr->tkwin, gcMask, &gcValues);
    if (tabPtr->backGC != NULL) {
	Tk_FreeGC(nbPtr->display, tabPtr->backGC);
    }
    tabPtr->backGC = newGC;
    /*
     * GC for tiled background.
     */
    if (tabPtr->tile != NULL) {
	Blt_SetTileChangedProc(tabPtr->tile, TileChangedProc, nbPtr);
    }
    if (tabPtr->flags & TAB_VISIBLE) {
	EventuallyRedraw(nbPtr);
    }
    return TCL_OK;
}

/*
 * --------------------------------------------------------------
 *
 * TearoffEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on the tearoff widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the tearoff gets deleted, internal structures get
 *	cleaned up.  When it gets resized or exposed, it's redisplayed.
 *
 * --------------------------------------------------------------
 */
static void
TearoffEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about the tab window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Tab *tabPtr = clientData;

    if ((tabPtr == NULL) || (tabPtr->tkwin == NULL) ||
	(tabPtr->container == NULL)) {
	return;
    }
    switch (eventPtr->type) {
    case Expose:
	if (eventPtr->xexpose.count == 0) {
	    EventuallyRedrawTearoff(tabPtr);
	}
	break;

    case ConfigureNotify:
	EventuallyRedrawTearoff(tabPtr);
	break;

    case DestroyNotify:
	if (tabPtr->flags & TAB_REDRAW) {
	    tabPtr->flags &= ~TAB_REDRAW;
	    Tcl_CancelIdleCall(DisplayTearoff, clientData);
	}
	Tk_DestroyWindow(tabPtr->container);
	tabPtr->container = NULL;
	break;

    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetReqWidth --
 *
 *	Returns the width requested by the embedded tab window and
 *	any requested padding around it. This represents the requested
 *	width of the page.
 *
 * Results:
 *	Returns the requested width of the page.
 *
 * ----------------------------------------------------------------------------
 */
static int
GetReqWidth(tabPtr)
    Tab *tabPtr;
{
    int width;

    if (tabPtr->reqWidth > 0) {
	width = tabPtr->reqWidth;
    } else {
	width = Tk_ReqWidth(tabPtr->tkwin);
    }
    width += PADDING(tabPtr->padX) +
	2 * Tk_Changes(tabPtr->tkwin)->border_width;
    if (width < 1) {
	width = 1;
    }
    return width;
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetReqHeight --
 *
 *	Returns the height requested by the window and padding around
 *	the window. This represents the requested height of the page.
 *
 * Results:
 *	Returns the requested height of the page.
 *
 * ----------------------------------------------------------------------------
 */
static int
GetReqHeight(tabPtr)
    Tab *tabPtr;
{
    int height;

    if (tabPtr->reqHeight > 0) {
	height = tabPtr->reqHeight;
    } else {
	height = Tk_ReqHeight(tabPtr->tkwin);
    }
    height += PADDING(tabPtr->padY) +
	2 * Tk_Changes(tabPtr->tkwin)->border_width;
    if (height < 1) {
	height = 1;
    }
    return height;
}

/*
 * ----------------------------------------------------------------------------
 *
 * TranslateAnchor --
 *
 * 	Translate the coordinates of a given bounding box based upon the
 * 	anchor specified.  The anchor indicates where the given xy position
 * 	is in relation to the bounding box.
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
 * ----------------------------------------------------------------------------
 */
static void
TranslateAnchor(dx, dy, anchor, xPtr, yPtr)
    int dx, dy;			/* Difference between outer and inner regions
				 */
    Tk_Anchor anchor;		/* Direction of the anchor */
    int *xPtr, *yPtr;
{
    int x, y;

    x = y = 0;
    switch (anchor) {
    case TK_ANCHOR_NW:		/* Upper left corner */
	break;
    case TK_ANCHOR_W:		/* Left center */
	y = (dy / 2);
	break;
    case TK_ANCHOR_SW:		/* Lower left corner */
	y = dy;
	break;
    case TK_ANCHOR_N:		/* Top center */
	x = (dx / 2);
	break;
    case TK_ANCHOR_CENTER:	/* Centered */
	x = (dx / 2);
	y = (dy / 2);
	break;
    case TK_ANCHOR_S:		/* Bottom center */
	x = (dx / 2);
	y = dy;
	break;
    case TK_ANCHOR_NE:		/* Upper right corner */
	x = dx;
	break;
    case TK_ANCHOR_E:		/* Right center */
	x = dx;
	y = (dy / 2);
	break;
    case TK_ANCHOR_SE:		/* Lower right corner */
	x = dx;
	y = dy;
	break;
    }
    *xPtr = (*xPtr) + x;
    *yPtr = (*yPtr) + y;
}


static void
GetWindowRectangle(tabPtr, parent, tearoff, rectPtr)
    Tab *tabPtr;
    Tk_Window parent;
    int tearoff;
    XRectangle *rectPtr;
{
    int pad;
    Notebook *nbPtr;
    int cavityWidth, cavityHeight;
    int width, height;
    int dx, dy;
    int x, y;

    nbPtr = tabPtr->nbPtr;
    pad = nbPtr->inset + nbPtr->inset2;

    if (!tearoff) {
	switch (nbPtr->side) {
	case SIDE_RIGHT:
	case SIDE_BOTTOM:
	    x = nbPtr->inset + nbPtr->inset2;
	    y = nbPtr->inset + nbPtr->inset2;
	    break;

	case SIDE_LEFT:
	    x = nbPtr->pageTop;
	    y = nbPtr->inset + nbPtr->inset2;
	    break;

	case SIDE_TOP:
	    x = nbPtr->inset + nbPtr->inset2;
	    y = nbPtr->pageTop;
	    break;
	}

	if (nbPtr->side & SIDE_VERTICAL) {
	    cavityWidth = Tk_Width(nbPtr->tkwin) - (nbPtr->pageTop + pad);
	    cavityHeight = Tk_Height(nbPtr->tkwin) - (2 * pad);
	} else {
	    cavityWidth = Tk_Width(nbPtr->tkwin) - (2 * pad);
	    cavityHeight = Tk_Height(nbPtr->tkwin) - (nbPtr->pageTop + pad);
	}

    } else {
	x = nbPtr->inset + nbPtr->inset2;
#define TEAR_OFF_TAB_SIZE	5
	y = nbPtr->inset + nbPtr->inset2 + nbPtr->yPad + nbPtr->outerPad +
	    TEAR_OFF_TAB_SIZE;
	cavityWidth = Tk_Width(parent) - (2 * pad);
	cavityHeight = Tk_Height(parent) - (y + pad);
    }
    cavityWidth -= PADDING(tabPtr->padX);
    cavityHeight -= PADDING(tabPtr->padY);
    if (cavityWidth < 1) {
	cavityWidth = 1;
    }
    if (cavityHeight < 1) {
	cavityHeight = 1;
    }
    width = GetReqWidth(tabPtr);
    height = GetReqHeight(tabPtr);

    /*
     * Resize the embedded window is of the following is true:
     *
     *	1) It's been torn off.
     *  2) The -fill option (horizontal or vertical) is set.
     *  3) the window is bigger than the cavity.
     */
    if ((tearoff) || (cavityWidth < width) || (tabPtr->fill & FILL_X)) {
	width = cavityWidth;
    }
    if ((tearoff) || (cavityHeight < height) || (tabPtr->fill & FILL_Y)) {
	height = cavityHeight;
    }
    dx = (cavityWidth - width);
    dy = (cavityHeight - height);
    if ((dx > 0) || (dy > 0)) {
	TranslateAnchor(dx, dy, tabPtr->anchor, &x, &y);
    }
    /* Remember that X11 windows must be at least 1 pixel. */
    if (width < 1) {
	width = 1;
    }
    if (height < 1) {
	height = 1;
    }
    rectPtr->x = (short)(x + tabPtr->padLeft);
    rectPtr->y = (short)(y + tabPtr->padTop);
    rectPtr->width = (short)width;
    rectPtr->height = (short)height;
}

static void
ArrangeWindow(tkwin, rectPtr, force)
    Tk_Window tkwin;
    XRectangle *rectPtr;
    int force;
{
    if ((force) ||
	(rectPtr->x != Tk_X(tkwin)) || 
	(rectPtr->y != Tk_Y(tkwin)) ||
	(rectPtr->width != Tk_Width(tkwin)) ||
	(rectPtr->height != Tk_Height(tkwin))) {
	Tk_MoveResizeWindow(tkwin, rectPtr->x, rectPtr->y, 
			    rectPtr->width, rectPtr->height);
    }
    if (!Tk_IsMapped(tkwin)) {
	Tk_MapWindow(tkwin);
    }
}


/*ARGSUSED*/
static void
GetTags(table, object, context, list)
    Blt_BindTable table;
    ClientData object;
    ClientData context;
    Blt_List list;
{
    Tab *tabPtr = (Tab *)object;
    Notebook *nbPtr;

    nbPtr = (Notebook *)table->clientData;
    if (context == TAB_PERFORATION) {
	Blt_ListAppend(list, MakeTag(nbPtr, "Perforation"), 0);
    } else if (context == TAB_LABEL) {
	Blt_ListAppend(list, MakeTag(nbPtr, tabPtr->name), 0);
	if (tabPtr->tags != NULL) {
	    int nNames;
	    char **names;
	    register char **p;
	    
	    /* 
	     * This is a space/time trade-off in favor of space.  The tags
	     * are stored as character strings in a hash table.  That way,
	     * tabs can share the strings. It's likely that they will.  The
	     * down side is that the same string is split over and over again. 
	     */
	    if (Tcl_SplitList((Tcl_Interp *)NULL, tabPtr->tags, &nNames, 
		&names) == TCL_OK) {
		for (p = names; *p != NULL; p++) {
		    Blt_ListAppend(list, MakeTag(nbPtr, *p), 0);
		}
		Blt_Free(names);
	    }
	}
    }
}

/*
 * --------------------------------------------------------------
 *
 * NotebookEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on notebook widgets.
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
NotebookEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Notebook *nbPtr = clientData;

    switch (eventPtr->type) {
    case Expose:
	if (eventPtr->xexpose.count == 0) {
	    EventuallyRedraw(nbPtr);
	}
	break;

    case ConfigureNotify:
	nbPtr->flags |= (TNB_LAYOUT | TNB_SCROLL);
	EventuallyRedraw(nbPtr);
	break;

    case FocusIn:
    case FocusOut:
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    if (eventPtr->type == FocusIn) {
		nbPtr->flags |= TNB_FOCUS;
	    } else {
		nbPtr->flags &= ~TNB_FOCUS;
	    }
	    EventuallyRedraw(nbPtr);
	}
	break;

    case DestroyNotify:
	if (nbPtr->tkwin != NULL) {
	    nbPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(nbPtr->interp, nbPtr->cmdToken);
	}
	if (nbPtr->flags & TNB_REDRAW) {
	    Tcl_CancelIdleCall(DisplayNotebook, nbPtr);
	}
	Tcl_EventuallyFree(nbPtr, DestroyNotebook);
	break;

    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyNotebook --
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
DestroyNotebook(dataPtr)
    DestroyData dataPtr;	/* Pointer to the widget record. */
{
    Notebook *nbPtr = (Notebook *)dataPtr;
    Tab *tabPtr;
    Blt_ChainLink *linkPtr;

    if (nbPtr->highlightGC != NULL) {
	Tk_FreeGC(nbPtr->display, nbPtr->highlightGC);
    }
    if (nbPtr->tile != NULL) {
	Blt_FreeTile(nbPtr->tile);
    }
    if (nbPtr->defTabStyle.activeGC != NULL) {
	Blt_FreePrivateGC(nbPtr->display, nbPtr->defTabStyle.activeGC);
    }
    for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	tabPtr = Blt_ChainGetValue(linkPtr);
	tabPtr->linkPtr = NULL;
	DestroyTab(nbPtr, tabPtr);
    }
    Blt_ChainDestroy(nbPtr->chainPtr);
    Blt_DestroyBindingTable(nbPtr->bindTable);
    Blt_DeleteHashTable(&(nbPtr->tabTable));
    Blt_DeleteHashTable(&(nbPtr->tagTable));
    Tk_FreeOptions(configSpecs, (char *)nbPtr, nbPtr->display, 0);
    Blt_Free(nbPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * CreateNotebook --
 *
 * ----------------------------------------------------------------------
 */
static Notebook *
CreateNotebook(interp, tkwin)
    Tcl_Interp *interp;
    Tk_Window tkwin;
{
    Notebook *nbPtr;

    nbPtr = Blt_Calloc(1, sizeof(Notebook));
    assert(nbPtr);

    Tk_SetClass(tkwin, "Tabnotebook");
    nbPtr->tkwin = tkwin;
    nbPtr->display = Tk_Display(tkwin);
    nbPtr->interp = interp;

    nbPtr->flags |= (TNB_LAYOUT | TNB_SCROLL);
    nbPtr->side = SIDE_TOP;
    nbPtr->borderWidth = nbPtr->highlightWidth = 2;
    nbPtr->ySelectPad = SELECT_PADY;
    nbPtr->xSelectPad = SELECT_PADX;
    nbPtr->relief = TK_RELIEF_SUNKEN;
    nbPtr->defTabStyle.relief = TK_RELIEF_RAISED;
    nbPtr->defTabStyle.borderWidth = 1;
    nbPtr->defTabStyle.constWidth = TRUE;
    nbPtr->defTabStyle.textSide = SIDE_LEFT;
    nbPtr->scrollUnits = 2;
    nbPtr->corner = CORNER_OFFSET;
    nbPtr->gap = GAP;
    nbPtr->outerPad = OUTER_PAD;
    nbPtr->slant = SLANT_NONE;
    nbPtr->overlap = 0;
    nbPtr->tearoff = TRUE;
    nbPtr->bindTable = Blt_CreateBindingTable(interp, tkwin, nbPtr, PickTab, 
	GetTags);
    nbPtr->chainPtr = Blt_ChainCreate();
    Blt_InitHashTable(&(nbPtr->tabTable), BLT_STRING_KEYS);
    Blt_InitHashTable(&(nbPtr->imageTable), BLT_STRING_KEYS);
    Blt_InitHashTable(&(nbPtr->tagTable), BLT_STRING_KEYS);
#if (TK_MAJOR_VERSION > 4)
    Blt_SetWindowInstanceData(tkwin, nbPtr);
#endif
    return nbPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureNotebook --
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
 *	etc. get set for nbPtr; old resources get freed, if there
 *	were any.  The widget is redisplayed.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureNotebook(interp, nbPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Interpreter to report errors. */
    Notebook *nbPtr;		/* Information about widget; may or
			         * may not already have values for
			         * some fields. */
    int argc;
    char **argv;
    int flags;
{
    XGCValues gcValues;
    unsigned long gcMask;
    GC newGC;

    lastNotebookInstance = nbPtr;
    if (Tk_ConfigureWidget(interp, nbPtr->tkwin, configSpecs, argc, argv,
	    (char *)nbPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_ConfigModified(configSpecs, "-width", "-height", "-side", "-gap",
	    "-slant", (char *)NULL)) {
	nbPtr->flags |= (TNB_LAYOUT | TNB_SCROLL);
    }
    if ((nbPtr->reqHeight > 0) && (nbPtr->reqWidth > 0)) {
	Tk_GeometryRequest(nbPtr->tkwin, nbPtr->reqWidth, 
		nbPtr->reqHeight);
    }
    /*
     * GC for focus highlight.
     */
    gcMask = GCForeground;
    gcValues.foreground = nbPtr->highlightColor->pixel;
    newGC = Tk_GetGC(nbPtr->tkwin, gcMask, &gcValues);
    if (nbPtr->highlightGC != NULL) {
	Tk_FreeGC(nbPtr->display, nbPtr->highlightGC);
    }
    nbPtr->highlightGC = newGC;

    /*
     * GC for tiled background.
     */
    if (nbPtr->tile != NULL) {
	Blt_SetTileChangedProc(nbPtr->tile, TileChangedProc, nbPtr);
    }
    /*
     * GC for active line.
     */
    gcMask = GCForeground | GCLineWidth | GCLineStyle | GCCapStyle;
    gcValues.foreground = nbPtr->defTabStyle.activeFgColor->pixel;
    gcValues.line_width = 0;
    gcValues.cap_style = CapProjecting;
    gcValues.line_style = (LineIsDashed(nbPtr->defTabStyle.dashes))
	? LineOnOffDash : LineSolid;

    newGC = Blt_GetPrivateGC(nbPtr->tkwin, gcMask, &gcValues);
    if (LineIsDashed(nbPtr->defTabStyle.dashes)) {
	nbPtr->defTabStyle.dashes.offset = 2;
	Blt_SetDashes(nbPtr->display, newGC, &(nbPtr->defTabStyle.dashes));
    }
    if (nbPtr->defTabStyle.activeGC != NULL) {
	Blt_FreePrivateGC(nbPtr->display, 
			  nbPtr->defTabStyle.activeGC);
    }
    nbPtr->defTabStyle.activeGC = newGC;

    nbPtr->defTabStyle.rotate = FMOD(nbPtr->defTabStyle.rotate, 360.0);
    if (nbPtr->defTabStyle.rotate < 0.0) {
	nbPtr->defTabStyle.rotate += 360.0;
    }
    nbPtr->inset = nbPtr->highlightWidth + nbPtr->borderWidth + nbPtr->outerPad;
    if (Blt_ConfigModified(configSpecs, "-font", "-*foreground", "-rotate",
	    "-*background", "-side", (char *)NULL)) {
	Blt_ChainLink *linkPtr;
	Tab *tabPtr;

	for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); 
	     linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    ConfigureTab(nbPtr, tabPtr);
	}
	nbPtr->flags |= (TNB_LAYOUT | TNB_SCROLL);
    }
    nbPtr->inset2 = nbPtr->defTabStyle.borderWidth + nbPtr->corner;
    EventuallyRedraw(nbPtr);
    return TCL_OK;
}

/*
 * --------------------------------------------------------------
 *
 * Notebook operations
 *
 * --------------------------------------------------------------
 */
/*
 *----------------------------------------------------------------------
 *
 * ActivateOp --
 *
 *	Selects the tab to appear active.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ActivateOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr;

    if (argv[2][0] == '\0') {
	tabPtr = NULL;
    } else if (GetTab(nbPtr, argv[2], &tabPtr, INVALID_OK) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((tabPtr != NULL) && (tabPtr->state == STATE_DISABLED)) {
	tabPtr = NULL;
    }
    if (tabPtr != nbPtr->activePtr) {
	nbPtr->activePtr = tabPtr;
	EventuallyRedraw(nbPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * BindOp --
 *
 *	  .t bind index sequence command
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BindOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    if (argc == 2) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;
	char *tagName;

	for (hPtr = Blt_FirstHashEntry(&(nbPtr->tagTable), &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    tagName = Blt_GetHashKey(&(nbPtr->tagTable), hPtr);
	    Tcl_AppendElement(interp, tagName);
	}
	return TCL_OK;
    }
    return Blt_ConfigureBindings(interp, nbPtr->bindTable,
	MakeTag(nbPtr, argv[2]), argc - 3, argv + 3);
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
CgetOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    lastNotebookInstance = nbPtr;
    return Tk_ConfigureValue(interp, nbPtr->tkwin, configSpecs,
	(char *)nbPtr, argv[2], 0);
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
 *	etc. get set for nbPtr; old resources get freed, if there
 *	were any.  The widget is redisplayed.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{

    lastNotebookInstance = nbPtr;
    if (argc == 2) {
	return Tk_ConfigureInfo(interp, nbPtr->tkwin, configSpecs,
	    (char *)nbPtr, (char *)NULL, 0);
    } else if (argc == 3) {
	return Tk_ConfigureInfo(interp, nbPtr->tkwin, configSpecs,
	    (char *)nbPtr, argv[2], 0);
    }
    if (ConfigureNotebook(interp, nbPtr, argc - 2, argv + 2,
	    TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    EventuallyRedraw(nbPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteOp --
 *
 *	Deletes tab from the set. Deletes either a range of
 *	tabs or a single node.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeleteOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *firstPtr, *lastPtr;

    lastPtr = NULL;
    if (GetTab(nbPtr, argv[2], &firstPtr, INVALID_FAIL) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((argc == 4) && 
	(GetTab(nbPtr, argv[3], &lastPtr, INVALID_FAIL) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (lastPtr == NULL) {
	DestroyTab(nbPtr, firstPtr);
    } else {
	Tab *tabPtr;
	Blt_ChainLink *linkPtr, *nextLinkPtr;

	tabPtr = NULL;		/* Suppress compiler warning. */

	/* Make sure that the first tab is before the last. */
	for (linkPtr = firstPtr->linkPtr; linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    if (tabPtr == lastPtr) {
		break;
	    }
	}
	if (tabPtr != lastPtr) {
	    return TCL_OK;
	}
	linkPtr = firstPtr->linkPtr;
	while (linkPtr != NULL) {
	    nextLinkPtr = Blt_ChainNextLink(linkPtr);
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    DestroyTab(nbPtr, tabPtr);
	    linkPtr = nextLinkPtr;
	    if (tabPtr == lastPtr) {
		break;
	    }
	}
    }
    nbPtr->flags |= (TNB_LAYOUT | TNB_SCROLL);
    EventuallyRedraw(nbPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FocusOp --
 *
 *	Selects the tab to get focus.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
FocusOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr;

    if (GetTab(nbPtr, argv[2], &tabPtr, INVALID_FAIL) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tabPtr != NULL) {
	nbPtr->focusPtr = tabPtr;
	Blt_SetFocusItem(nbPtr->bindTable, nbPtr->focusPtr, NULL);
	EventuallyRedraw(nbPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IndexOp --
 *
 *	Converts a string representing a tab index.
 *
 * Results:
 *	A standard Tcl result.  Interp->result will contain the
 *	identifier of each index found. If an index could not be found,
 *	then the serial identifier will be the empty string.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
IndexOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			
    char **argv;
{
    Tab *tabPtr;

    if (GetTab(nbPtr, argv[2], &tabPtr, INVALID_OK) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tabPtr != NULL) {
	Tcl_SetResult(interp, Blt_Itoa(TabIndex(nbPtr, tabPtr)), 
		TCL_VOLATILE);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IdOp --
 *
 *	Converts a tab index into the tab identifier.
 *
 * Results:
 *	A standard Tcl result.  Interp->result will contain the
 *	identifier of each index found. If an index could not be found,
 *	then the serial identifier will be the empty string.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
IdOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr;

    if (GetTab(nbPtr, argv[2], &tabPtr, INVALID_OK) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tabPtr == NULL) {
	Tcl_SetResult(interp, "", TCL_STATIC);
    } else {
	Tcl_SetResult(interp, tabPtr->name, TCL_VOLATILE);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InsertOp --
 *
 *	Add new entries into a tab set.
 *
 *	.t insert end label option-value label option-value...
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InsertOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr;
    Blt_ChainLink *linkPtr, *beforeLinkPtr;
    char c;

    c = argv[2][0];
    if ((c == 'e') && (strcmp(argv[2], "end") == 0)) {
	beforeLinkPtr = NULL;
    } else if (isdigit(UCHAR(c))) {
	int position;

	if (Tcl_GetInt(interp, argv[2], &position) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (position < 0) {
	    beforeLinkPtr = Blt_ChainFirstLink(nbPtr->chainPtr);
	} else if (position > Blt_ChainGetLength(nbPtr->chainPtr)) {
	    beforeLinkPtr = NULL;
	} else {
	    beforeLinkPtr = Blt_ChainGetNthLink(nbPtr->chainPtr, position);
	}
    } else {
	Tab *beforePtr;

	if (GetTab(nbPtr, argv[2], &beforePtr, INVALID_FAIL) != TCL_OK) {
	    return TCL_ERROR;
	}
	beforeLinkPtr = beforePtr->linkPtr;
    }
    nbPtr->flags |= (TNB_LAYOUT | TNB_SCROLL);
    EventuallyRedraw(nbPtr);
    tabPtr = CreateTab(nbPtr);
    if (tabPtr == NULL) {
	return TCL_ERROR;
    }
    lastNotebookInstance = nbPtr;
    if (Blt_ConfigureWidgetComponent(interp, nbPtr->tkwin, tabPtr->name,
	"Tab", tabConfigSpecs, argc - 3, argv + 3, (char *)tabPtr, 0) 
	!= TCL_OK) {
	DestroyTab(nbPtr, tabPtr);
	return TCL_ERROR;
    }
    if (ConfigureTab(nbPtr, tabPtr) != TCL_OK) {
	DestroyTab(nbPtr, tabPtr);
	return TCL_ERROR;
    }
    linkPtr = Blt_ChainNewLink();
    if (beforeLinkPtr == NULL) {
	Blt_ChainAppendLink(nbPtr->chainPtr, linkPtr);
    } else {
	Blt_ChainLinkBefore(nbPtr->chainPtr, linkPtr, beforeLinkPtr);
    }
    tabPtr->linkPtr = linkPtr;
    Blt_ChainSetValue(linkPtr, tabPtr);
    Tcl_SetResult(interp, tabPtr->name, TCL_VOLATILE);
    return TCL_OK;

}

/*
 * Preprocess the command string for percent substitution.
 */
static void
PercentSubst(nbPtr, tabPtr, command, resultPtr)
    Notebook *nbPtr;
    Tab *tabPtr;
    char *command;
    Tcl_DString *resultPtr;
{
    register char *last, *p;
    /*
     * Get the full path name of the node, in case we need to
     * substitute for it.
     */
    Tcl_DStringInit(resultPtr);
    for (last = p = command; *p != '\0'; p++) {
	if (*p == '%') {
	    char *string;
	    char buf[3];

	    if (p > last) {
		*p = '\0';
		Tcl_DStringAppend(resultPtr, last, -1);
		*p = '%';
	    }
	    switch (*(p + 1)) {
	    case '%':		/* Percent sign */
		string = "%";
		break;
	    case 'W':		/* Widget name */
		string = Tk_PathName(nbPtr->tkwin);
		break;
	    case 'i':		/* Tab Index */
		string = Blt_Itoa(TabIndex(nbPtr, tabPtr));
		break;
	    case 'n':		/* Tab name */
		string = tabPtr->name;
		break;
	    default:
		if (*(p + 1) == '\0') {
		    p--;
		}
		buf[0] = *p, buf[1] = *(p + 1), buf[2] = '\0';
		string = buf;
		break;
	    }
	    Tcl_DStringAppend(resultPtr, string, -1);
	    p++;
	    last = p + 1;
	}
    }
    if (p > last) {
	*p = '\0';
	Tcl_DStringAppend(resultPtr, last, -1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InvokeOp --
 *
 * 	This procedure is called to invoke a selection command.
 *
 *	  .h invoke index
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side Effects:
 *	Configuration information, such as text string, colors, font,
 * 	etc. get set;  old resources get freed, if there were any.
 * 	The widget is redisplayed if needed.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InvokeOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Tab *tabPtr;
    char *command;

    if (GetTab(nbPtr, argv[2], &tabPtr, INVALID_OK) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((tabPtr == NULL) || (tabPtr->state == STATE_DISABLED)) {
	return TCL_OK;
    }
    Tcl_Preserve(tabPtr);
    command = GETATTR(tabPtr, command);
    if (command != NULL) {
	Tcl_DString dString;
	int result;

	PercentSubst(nbPtr, tabPtr, command, &dString);
	result = Tcl_GlobalEval(nbPtr->interp, 
		Tcl_DStringValue(&dString));
	Tcl_DStringFree(&dString);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    Tcl_Release(tabPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MoveOp --
 *
 *	Moves a tab to a new location.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
MoveOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr, *linkPtr;
    int before;

    if (GetTab(nbPtr, argv[2], &tabPtr, INVALID_OK) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((tabPtr == NULL) || (tabPtr->state == STATE_DISABLED)) {
	return TCL_OK;
    }
    if ((argv[3][0] == 'b') && (strcmp(argv[3], "before") == 0)) {
	before = 1;
    } else if ((argv[3][0] == 'a') && (strcmp(argv[3], "after") == 0)) {
	before = 0;
    } else {
	Tcl_AppendResult(interp, "bad key word \"", argv[3],
	    "\": should be \"after\" or \"before\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (GetTab(nbPtr, argv[4], &linkPtr, INVALID_FAIL) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tabPtr == linkPtr) {
	return TCL_OK;
    }
    Blt_ChainUnlinkLink(nbPtr->chainPtr, tabPtr->linkPtr);
    if (before) {
	Blt_ChainLinkBefore(nbPtr->chainPtr, tabPtr->linkPtr, 
	    linkPtr->linkPtr);
    } else {
	Blt_ChainLinkAfter(nbPtr->chainPtr, tabPtr->linkPtr, 
	    linkPtr->linkPtr);
    }
    nbPtr->flags |= (TNB_LAYOUT | TNB_SCROLL);
    EventuallyRedraw(nbPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
NearestOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    int x, y;			/* Screen coordinates of the test point. */
    Tab *tabPtr;

    if ((Tk_GetPixels(interp, nbPtr->tkwin, argv[2], &x) != TCL_OK) ||
	(Tk_GetPixels(interp, nbPtr->tkwin, argv[3], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (nbPtr->nVisible > 0) {
	tabPtr = (Tab *)PickTab(nbPtr, x, y, NULL);
	if (tabPtr != NULL) {
	    Tcl_SetResult(interp, tabPtr->name, TCL_VOLATILE);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectOp --
 *
 * 	This procedure is called to selecta tab.
 *
 *	  .h select index
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side Effects:
 *	Configuration information, such as text string, colors, font,
 * 	etc. get set;  old resources get freed, if there were any.
 * 	The widget is redisplayed if needed.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SelectOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Tab *tabPtr;

    if (GetTab(nbPtr, argv[2], &tabPtr, INVALID_OK) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((tabPtr == NULL) || (tabPtr->state == STATE_DISABLED)) {
	return TCL_OK;
    }
    if ((nbPtr->selectPtr != NULL) && (nbPtr->selectPtr != tabPtr) &&
	(nbPtr->selectPtr->tkwin != NULL)) {
	if (nbPtr->selectPtr->container == NULL) {
	    if (Tk_IsMapped(nbPtr->selectPtr->tkwin)) {
		Tk_UnmapWindow(nbPtr->selectPtr->tkwin);
	    }
	} else {
	    /* Redraw now unselected container. */
	    EventuallyRedrawTearoff(nbPtr->selectPtr);
	}
    }
    nbPtr->selectPtr = tabPtr;
    if ((nbPtr->nTiers > 1) && (tabPtr->tier != nbPtr->startPtr->tier)) {
	RenumberTiers(nbPtr, tabPtr);
	Blt_PickCurrentItem(nbPtr->bindTable);
    }
    nbPtr->flags |= (TNB_SCROLL);
    if (tabPtr->container != NULL) {
	EventuallyRedrawTearoff(tabPtr);
    }
    EventuallyRedraw(nbPtr);
    return TCL_OK;
}

static int
ViewOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int width;

    width = VPORTWIDTH(nbPtr);
    if (argc == 2) {
	double fract;

	/*
	 * Note: we are bounding the fractions between 0.0 and 1.0 to
	 * support the "canvas"-style of scrolling.
	 */

	fract = (double)nbPtr->scrollOffset / nbPtr->worldWidth;
	Tcl_AppendElement(interp, Blt_Dtoa(interp, CLAMP(fract, 0.0, 1.0)));
	fract = (double)(nbPtr->scrollOffset + width) / nbPtr->worldWidth;
	Tcl_AppendElement(interp, Blt_Dtoa(interp, CLAMP(fract, 0.0, 1.0)));
	return TCL_OK;
    }
    if (Blt_GetScrollInfo(interp, argc - 2, argv + 2, &(nbPtr->scrollOffset),
	    nbPtr->worldWidth, width, nbPtr->scrollUnits, 
	    BLT_SCROLL_MODE_CANVAS) != TCL_OK) {
	return TCL_ERROR;
    }
    nbPtr->flags |= TNB_SCROLL;
    EventuallyRedraw(nbPtr);
    return TCL_OK;
}


static void
AdoptWindow(clientData)
    ClientData clientData;
{
    Tab *tabPtr = clientData;
    int x, y;
    Notebook *nbPtr = tabPtr->nbPtr;

    x = nbPtr->inset + nbPtr->inset2 + tabPtr->padLeft;
#define TEAR_OFF_TAB_SIZE	5
    y = nbPtr->inset + nbPtr->inset2 + nbPtr->yPad +
	nbPtr->outerPad + TEAR_OFF_TAB_SIZE + tabPtr->padTop;
    Blt_RelinkWindow(tabPtr->tkwin, tabPtr->container, x, y);
    Tk_MapWindow(tabPtr->tkwin);
}

static void
DestroyTearoff(dataPtr)
    DestroyData dataPtr;
{
    Tab *tabPtr = (Tab *)dataPtr;

    if (tabPtr->container != NULL) {
	Notebook *nbPtr;
	Tk_Window tkwin;
	nbPtr = tabPtr->nbPtr;

	tkwin = tabPtr->container;
	if (tabPtr->flags & TAB_REDRAW) {
	    Tcl_CancelIdleCall(DisplayTearoff, tabPtr);
	}
	Tk_DeleteEventHandler(tkwin, StructureNotifyMask, TearoffEventProc,
	    tabPtr);
	if (tabPtr->tkwin != NULL) {
	    XRectangle rect;

	    GetWindowRectangle(tabPtr, nbPtr->tkwin, FALSE, &rect);
	    Blt_RelinkWindow(tabPtr->tkwin, nbPtr->tkwin, rect.x, rect.y);
	    if (tabPtr == nbPtr->selectPtr) {
		ArrangeWindow(tabPtr->tkwin, &rect, TRUE);
	    } else {
		Tk_UnmapWindow(tabPtr->tkwin);
	    }
	}
	Tk_DestroyWindow(tkwin);
	tabPtr->container = NULL;
    }
}

static int
CreateTearoff(nbPtr, name, tabPtr)
    Notebook *nbPtr;
    char *name;
    Tab *tabPtr;
{
    Tk_Window tkwin;
    int width, height;

    tkwin = Tk_CreateWindowFromPath(nbPtr->interp, nbPtr->tkwin, name,
	(char *)NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    tabPtr->container = tkwin;
    if (Tk_WindowId(tkwin) == None) {
	Tk_MakeWindowExist(tkwin);
    }
    Tk_SetClass(tkwin, "Tearoff");
    Tk_CreateEventHandler(tkwin, (ExposureMask | StructureNotifyMask),
	TearoffEventProc, tabPtr);
    if (Tk_WindowId(tabPtr->tkwin) == None) {
	Tk_MakeWindowExist(tabPtr->tkwin);
    }
    width = Tk_Width(tabPtr->tkwin);
    if (width < 2) {
	width = (tabPtr->reqWidth > 0)
	    ? tabPtr->reqWidth : Tk_ReqWidth(tabPtr->tkwin);
    }
    width += PADDING(tabPtr->padX) + 2 *
	Tk_Changes(tabPtr->tkwin)->border_width;
    width += 2 * (nbPtr->inset2 + nbPtr->inset);
#define TEAR_OFF_TAB_SIZE	5
    height = Tk_Height(tabPtr->tkwin);
    if (height < 2) {
	height = (tabPtr->reqHeight > 0)
	    ? tabPtr->reqHeight : Tk_ReqHeight(tabPtr->tkwin);
    }
    height += PADDING(tabPtr->padY) +
	2 * Tk_Changes(tabPtr->tkwin)->border_width;
    height += nbPtr->inset + nbPtr->inset2 + nbPtr->yPad +
	TEAR_OFF_TAB_SIZE + nbPtr->outerPad;
    Tk_GeometryRequest(tkwin, width, height);
    Tk_UnmapWindow(tabPtr->tkwin);
    /* Tk_MoveWindow(tabPtr->tkwin, 0, 0); */
    Tcl_SetResult(nbPtr->interp, Tk_PathName(tkwin), TCL_VOLATILE);
#ifdef WIN32
    AdoptWindow(tabPtr);
#else
    Tcl_DoWhenIdle(AdoptWindow, tabPtr);
#endif
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TabCgetOp --
 *
 *	  .h tab cget index option
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TabCgetOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr;

    if (GetTab(nbPtr, argv[3], &tabPtr, INVALID_FAIL) != TCL_OK) {
	return TCL_ERROR;
    }
    lastNotebookInstance = nbPtr;
    return Tk_ConfigureValue(interp, nbPtr->tkwin, tabConfigSpecs,
	(char *)tabPtr, argv[4], 0);
}

/*
 *----------------------------------------------------------------------
 *
 * TabConfigureOp --
 *
 * 	This procedure is called to process a list of configuration
 *	options database, in order to reconfigure the options for
 *	one or more tabs in the widget.
 *
 *	  .h tab configure index ?index...? ?option value?...
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side Effects:
 *	Configuration information, such as text string, colors, font,
 * 	etc. get set;  old resources get freed, if there were any.
 * 	The widget is redisplayed if needed.
 *
 *----------------------------------------------------------------------
 */
static int
TabConfigureOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int nTabs, nOpts, result;
    char **options;
    register int i;
    Tab *tabPtr;

    /* Figure out where the option value pairs begin */
    argc -= 3;
    argv += 3;
    for (i = 0; i < argc; i++) {
	if (argv[i][0] == '-') {
	    break;
	}
	if (GetTab(nbPtr, argv[i], &tabPtr, INVALID_FAIL) != TCL_OK) {
	    return TCL_ERROR;	/* Can't find node. */
	}
    }
    nTabs = i;			/* Number of tab indices specified */
    nOpts = argc - i;		/* Number of options specified */
    options = argv + i;		/* Start of options in argv  */

    for (i = 0; i < nTabs; i++) {
	GetTab(nbPtr, argv[i], &tabPtr, INVALID_FAIL);
	if (argc == 1) {
	    return Tk_ConfigureInfo(interp, nbPtr->tkwin, tabConfigSpecs,
		(char *)tabPtr, (char *)NULL, 0);
	} else if (argc == 2) {
	    return Tk_ConfigureInfo(interp, nbPtr->tkwin, tabConfigSpecs,
		(char *)tabPtr, argv[2], 0);
	}
	Tcl_Preserve(tabPtr);
	lastNotebookInstance = nbPtr;
	result = Tk_ConfigureWidget(interp, nbPtr->tkwin, tabConfigSpecs,
	    nOpts, options, (char *)tabPtr, TK_CONFIG_ARGV_ONLY);
	if (result == TCL_OK) {
	    result = ConfigureTab(nbPtr, tabPtr);
	}
	Tcl_Release(tabPtr);
	if (result == TCL_ERROR) {
	    return TCL_ERROR;
	}
	if (tabPtr->flags & TAB_VISIBLE) {
	    nbPtr->flags |= (TNB_LAYOUT | TNB_SCROLL);
	    EventuallyRedraw(nbPtr);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TabDockallOp --
 *
 *	  .h tab dockall
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TabDockallOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;		/* Not used. */
{
    Tab *tabPtr;
    Blt_ChainLink *linkPtr;

    for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	 linkPtr = Blt_ChainNextLink(linkPtr)) {
	tabPtr = Blt_ChainGetValue(linkPtr);
	if (tabPtr->container != NULL) {
	    Tcl_EventuallyFree(tabPtr, DestroyTearoff);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TabNamesOp --
 *
 *	  .h tab names pattern
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TabNamesOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;		/* Not used. */
{
    Tab *tabPtr;
    Blt_ChainLink *linkPtr;

    if (argc == 3) {
	for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    Tcl_AppendElement(interp, tabPtr->name);
	}
    } else {
	register int i;

	for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    for (i = 3; i < argc; i++) {
		if (Tcl_StringMatch(tabPtr->name, argv[i])) {
		    Tcl_AppendElement(interp, tabPtr->name);
		    break;
		}
	    }
	}
    }
    return TCL_OK;
}
/*
 *----------------------------------------------------------------------
 *
 * TabTearoffOp --
 *
 *	  .h tab tearoff index ?title?
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TabTearoffOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr;
    int result;
    Tk_Window tkwin;

    if (GetTab(nbPtr, argv[3], &tabPtr, INVALID_OK) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((tabPtr == NULL) || (tabPtr->tkwin == NULL) ||
	(tabPtr->state == STATE_DISABLED)) {
	return TCL_OK;		/* No-op */
    }
    if (argc == 4) {
	Tk_Window parent;

	parent = (tabPtr->container == NULL)
	    ? nbPtr->tkwin : tabPtr->container;
	Tcl_SetResult(nbPtr->interp, Tk_PathName(parent), TCL_VOLATILE);
	return TCL_OK;
    }
    Tcl_Preserve(tabPtr);
    result = TCL_OK;

    tkwin = Tk_NameToWindow(interp, argv[4], nbPtr->tkwin);
    Tcl_ResetResult(interp);

    if (tabPtr->container != NULL) {
	Tcl_EventuallyFree(tabPtr, DestroyTearoff);
    }
    if ((tkwin != nbPtr->tkwin) && (tabPtr->container == NULL)) {
	result = CreateTearoff(nbPtr, argv[4], tabPtr);
    }
    Tcl_Release(tabPtr);
    EventuallyRedraw(nbPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TabOp --
 *
 *	This procedure handles tab operations.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static Blt_OpSpec tabOps[] =
{
    {"cget", 2, (Blt_Op)TabCgetOp, 5, 5, "nameOrIndex option",},
    {"configure", 2, (Blt_Op)TabConfigureOp, 4, 0,
	"nameOrIndex ?option value?...",},
    {"dockall", 1, (Blt_Op)TabDockallOp, 3, 3, "" }, 
    {"names", 1, (Blt_Op)TabNamesOp, 3, 0, "?pattern...?",},
    {"tearoff", 1, (Blt_Op)TabTearoffOp, 4, 5, "index ?parent?",},
};

static int nTabOps = sizeof(tabOps) / sizeof(Blt_OpSpec);

static int
TabOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nTabOps, tabOps, BLT_OP_ARG2, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (nbPtr, interp, argc, argv);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * PerforationActivateOp --
 *
 * 	This procedure is called to highlight the perforation.
 *
 *	  .h perforation highlight boolean
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PerforationActivateOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    int bool;

    if (Tcl_GetBoolean(interp, argv[3], &bool) != TCL_OK) {
	return TCL_ERROR;
    }
    if (bool) {
	nbPtr->flags |= PERFORATION_ACTIVE;
    } else {
	nbPtr->flags &= ~PERFORATION_ACTIVE;
    }
    EventuallyRedraw(nbPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PerforationInvokeOp --
 *
 * 	This procedure is called to invoke a perforation command.
 *
 *	  .t perforation invoke
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PerforationInvokeOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{

    if (nbPtr->selectPtr != NULL) {
	char *cmd;
	
	cmd = GETATTR(nbPtr->selectPtr, perfCommand);
	if (cmd != NULL) {
	    Tcl_DString dString;
	    int result;
	    
	    PercentSubst(nbPtr, nbPtr->selectPtr, cmd, &dString);
	    Tcl_Preserve(nbPtr);
	    result = Tcl_GlobalEval(interp, Tcl_DStringValue(&dString));
	    Tcl_Release(nbPtr);
	    Tcl_DStringFree(&dString);
	    if (result != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PerforationOp --
 *
 *	This procedure handles tab operations.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static Blt_OpSpec perforationOps[] =
{
    {"activate", 1, (Blt_Op)PerforationActivateOp, 4, 4, "boolean" }, 
    {"invoke", 1, (Blt_Op)PerforationInvokeOp, 3, 3, "",},
};

static int nPerforationOps = sizeof(perforationOps) / sizeof(Blt_OpSpec);

static int
PerforationOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nPerforationOps, perforationOps, BLT_OP_ARG2, 
		     argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (nbPtr, interp, argc, argv);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ScanOp --
 *
 *	Implements the quick scan.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ScanOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    int x, y;
    char c;
    unsigned int length;
    int oper;

#define SCAN_MARK	1
#define SCAN_DRAGTO	2
    c = argv[2][0];
    length = strlen(argv[2]);
    if ((c == 'm') && (strncmp(argv[2], "mark", length) == 0)) {
	oper = SCAN_MARK;
    } else if ((c == 'd') && (strncmp(argv[2], "dragto", length) == 0)) {
	oper = SCAN_DRAGTO;
    } else {
	Tcl_AppendResult(interp, "bad scan operation \"", argv[2],
	    "\": should be either \"mark\" or \"dragto\"", (char *)NULL);
	return TCL_ERROR;
    }
    if ((Tk_GetPixels(interp, nbPtr->tkwin, argv[3], &x) != TCL_OK) ||
	(Tk_GetPixels(interp, nbPtr->tkwin, argv[4], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (oper == SCAN_MARK) {
	if (nbPtr->side & SIDE_VERTICAL) {
	    nbPtr->scanAnchor = y;
	} else {
	    nbPtr->scanAnchor = x;
	}
	nbPtr->scanOffset = nbPtr->scrollOffset;
    } else {
	int offset, delta;

	if (nbPtr->side & SIDE_VERTICAL) {
	    delta = nbPtr->scanAnchor - y;
	} else {
	    delta = nbPtr->scanAnchor - x;
	}
	offset = nbPtr->scanOffset + (10 * delta);
	offset = Blt_AdjustViewport(offset, nbPtr->worldWidth,
	    VPORTWIDTH(nbPtr), nbPtr->scrollUnits, BLT_SCROLL_MODE_CANVAS);
	nbPtr->scrollOffset = offset;
	nbPtr->flags |= TNB_SCROLL;
	EventuallyRedraw(nbPtr);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
SeeOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Tab *tabPtr;

    if (GetTab(nbPtr, argv[2], &tabPtr, INVALID_OK) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tabPtr != NULL) {
	int left, right, width;

	width = VPORTWIDTH(nbPtr);
	left = nbPtr->scrollOffset + nbPtr->xSelectPad;
	right = nbPtr->scrollOffset + width - nbPtr->xSelectPad;

	/* If the tab is partially obscured, scroll so that it's
	 * entirely in view. */
	if (tabPtr->worldX < left) {
	    nbPtr->scrollOffset = tabPtr->worldX;
	    if (TabIndex(nbPtr, tabPtr) > 0) {
		nbPtr->scrollOffset -= TAB_SCROLL_OFFSET;
	    }
	} else if ((tabPtr->worldX + tabPtr->worldWidth) >= right) {
	    Blt_ChainLink *linkPtr;

	    nbPtr->scrollOffset = tabPtr->worldX + tabPtr->worldWidth -
		(width - 2 * nbPtr->xSelectPad);
	    linkPtr = Blt_ChainNextLink(tabPtr->linkPtr); 
	    if (linkPtr != NULL) {
		Tab *nextPtr;

		nextPtr = Blt_ChainGetValue(linkPtr);
		if (nextPtr->tier == tabPtr->tier) {
		    nbPtr->scrollOffset += TAB_SCROLL_OFFSET;
		}
	    }
	}
	nbPtr->flags |= TNB_SCROLL;
	EventuallyRedraw(nbPtr);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
SizeOp(nbPtr, interp, argc, argv)
    Notebook *nbPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;		/* Not used. */
{
    Tcl_SetResult(interp, Blt_Itoa(Blt_ChainGetLength(nbPtr->chainPtr)),
	TCL_VOLATILE);
    return TCL_OK;
}


static int
CountTabs(nbPtr)
    Notebook *nbPtr;
{
    int count;
    int width, height;
    Blt_ChainLink *linkPtr;
    register Tab *tabPtr;
    register int pageWidth, pageHeight;
    int labelWidth, labelHeight;
    int tabWidth, tabHeight;

    pageWidth = pageHeight = 0;
    count = 0;

    labelWidth = labelHeight = 0;

    /*
     * Pass 1:  Figure out the maximum area needed for a label and a
     *		page.  Both the label and page dimensions are adjusted
     *		for orientation.  In addition, reset the visibility
     *		flags and reorder the tabs.
     */
    for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	tabPtr = Blt_ChainGetValue(linkPtr);

	/* Reset visibility flag and order of tabs. */

	tabPtr->flags &= ~TAB_VISIBLE;
	count++;

	if (tabPtr->tkwin != NULL) {
	    width = GetReqWidth(tabPtr);
	    if (pageWidth < width) {
		pageWidth = width;
	    }
	    height = GetReqHeight(tabPtr);
	    if (pageHeight < height) {
		pageHeight = height;
	    }
	}
	if (labelWidth < tabPtr->labelWidth) {
	    labelWidth = tabPtr->labelWidth;
	}
	if (labelHeight < tabPtr->labelHeight) {
	    labelHeight = tabPtr->labelHeight;
	}
    }

    nbPtr->overlap = 0;

    /*
     * Pass 2:	Set the individual sizes of each tab.  This is different
     *		for constant and variable width tabs.  Add the extra space
     *		needed for slanted tabs, now that we know maximum tab
     *		height.
     */
    if (nbPtr->defTabStyle.constWidth) {
	int slant;

	tabWidth = 2 * nbPtr->inset2;
	tabHeight = nbPtr->inset2 /* + 4 */;

	if (nbPtr->side & SIDE_VERTICAL) {
	    tabWidth += labelHeight;
	    tabHeight += labelWidth;
	    slant = labelWidth;
	} else {
	    tabWidth += labelWidth;
	    tabHeight += labelHeight;
	    slant = labelHeight;
	}
	if (nbPtr->slant & SLANT_LEFT) {
	    tabWidth += slant;
	    nbPtr->overlap += tabHeight / 2;
	}
	if (nbPtr->slant & SLANT_RIGHT) {
	    tabWidth += slant;
	    nbPtr->overlap += tabHeight / 2;
	}
	for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    tabPtr->worldWidth = tabWidth;
	    tabPtr->worldHeight = tabHeight;
	}
    } else {
	int slant;

	tabWidth = tabHeight = 0;
	for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);

	    width = 2 * nbPtr->inset2;
	    height = nbPtr->inset2 /* + 4 */;
	    if (nbPtr->side & SIDE_VERTICAL) {
		width += tabPtr->labelHeight;
		height += labelWidth;
		slant = labelWidth;
	    } else {
		width += tabPtr->labelWidth;
		height += labelHeight;
		slant = labelHeight;
	    }
	    width += (nbPtr->slant & SLANT_LEFT) ? slant : nbPtr->corner;
	    width += (nbPtr->slant & SLANT_RIGHT) ? slant : nbPtr->corner;

	    tabPtr->worldWidth = width; /* + 2 * (nbPtr->corner + nbPtr->xSelectPad) */ ;
	    tabPtr->worldHeight = height;

	    if (tabWidth < width) {
		tabWidth = width;
	    }
	    if (tabHeight < height) {
		tabHeight = height;
	    }
	}
	if (nbPtr->slant & SLANT_LEFT) {
	    nbPtr->overlap += tabHeight / 2;
	}
	if (nbPtr->slant & SLANT_RIGHT) {
	    nbPtr->overlap += tabHeight / 2;
	}
    }

    nbPtr->tabWidth = tabWidth;
    nbPtr->tabHeight = tabHeight;

    /*
     * Let the user override any page dimension.
     */
    nbPtr->pageWidth = pageWidth;
    nbPtr->pageHeight = pageHeight;
    if (nbPtr->reqPageWidth > 0) {
	nbPtr->pageWidth = nbPtr->reqPageWidth;
    }
    if (nbPtr->reqPageHeight > 0) {
	nbPtr->pageHeight = nbPtr->reqPageHeight;
    }
    return count;
}


static void
WidenTabs(nbPtr, startPtr, nTabs, adjustment)
    Notebook *nbPtr;
    Tab *startPtr;
    int nTabs;
    int adjustment;
{
    register Tab *tabPtr;
    register int i;
    int ration;
    Blt_ChainLink *linkPtr;
    int x;

    x = startPtr->tier;
    while (adjustment > 0) {
	ration = adjustment / nTabs;
	if (ration == 0) {
	    ration = 1;
	}
	linkPtr = startPtr->linkPtr;
	for (i = 0; (linkPtr != NULL) && (i < nTabs) && (adjustment > 0); i++) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    adjustment -= ration;
	    tabPtr->worldWidth += ration;
	    assert(x == tabPtr->tier);
	    linkPtr = Blt_ChainNextLink(linkPtr);
	}
    }
    /*
     * Go back and reset the world X-coordinates of the tabs,
     * now that their widths have changed.
     */
    x = 0;
    linkPtr = startPtr->linkPtr;
    for (i = 0; (i < nTabs) && (linkPtr != NULL); i++) {
	tabPtr = Blt_ChainGetValue(linkPtr);
	tabPtr->worldX = x;
	x += tabPtr->worldWidth + nbPtr->gap - nbPtr->overlap;
	linkPtr = Blt_ChainNextLink(linkPtr);
    }
}


static void
AdjustTabSizes(nbPtr, nTabs)
    Notebook *nbPtr;
    int nTabs;
{
    int tabsPerTier;
    int total, count, extra;
    Tab *startPtr, *nextPtr;
    Blt_ChainLink *linkPtr;
    register Tab *tabPtr;
    int x, maxWidth;

    tabsPerTier = (nTabs + (nbPtr->nTiers - 1)) / nbPtr->nTiers;
    x = 0;
    maxWidth = 0;
    if (nbPtr->defTabStyle.constWidth) {
	register int i;

	linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr);
	count = 1;
	while (linkPtr != NULL) {
	    for (i = 0; i < tabsPerTier; i++) {
		tabPtr = Blt_ChainGetValue(linkPtr);
		tabPtr->tier = count;
		tabPtr->worldX = x;
		x += tabPtr->worldWidth + nbPtr->gap - nbPtr->overlap;
		linkPtr = Blt_ChainNextLink(linkPtr);
		if (x > maxWidth) {
		    maxWidth = x;
		}
		if (linkPtr == NULL) {
		    goto done;
		}
	    }
	    count++;
	    x = 0;
	}
    }
  done:
    /* Add to tab widths to fill out row. */
    if (((nTabs % tabsPerTier) != 0) && (nbPtr->defTabStyle.constWidth)) {
	return;
    }
    startPtr = NULL;
    count = total = 0;
    for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	/*empty*/ ) {
	tabPtr = Blt_ChainGetValue(linkPtr);
	if (startPtr == NULL) {
	    startPtr = tabPtr;
	}
	count++;
	total += tabPtr->worldWidth + nbPtr->gap - nbPtr->overlap;
	linkPtr = Blt_ChainNextLink(linkPtr);
	if (linkPtr != NULL) {
	    nextPtr = Blt_ChainGetValue(linkPtr);
	    if (tabPtr->tier == nextPtr->tier) {
		continue;
	    }
	}
	total += nbPtr->overlap;
	extra = nbPtr->worldWidth - total;
	assert(count > 0);
	if (extra > 0) {
	    WidenTabs(nbPtr, startPtr, count, extra);
	}
	count = total = 0;
	startPtr = NULL;
    }
}

/*
 *
 * tabWidth = textWidth + gap + (2 * (pad + outerBW));
 *
 * tabHeight = textHeight + 2 * (pad + outerBW) + topMargin;
 *
 */
static void
ComputeLayout(nbPtr)
    Notebook *nbPtr;
{
    int width;
    Blt_ChainLink *linkPtr;
    Tab *tabPtr;
    int x, extra;
    int nTiers, nTabs;

    nbPtr->nTiers = 0;
    nbPtr->pageTop = 0;
    nbPtr->worldWidth = 1;
    nbPtr->yPad = 0;

    nTabs = CountTabs(nbPtr);
    if (nTabs == 0) {
	return;
    }
    /* Reset the pointers to the selected and starting tab. */
    if (nbPtr->selectPtr == NULL) {
	linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr);
	if (linkPtr != NULL) {
	    nbPtr->selectPtr = Blt_ChainGetValue(linkPtr);
	}
    }
    if (nbPtr->startPtr == NULL) {
	nbPtr->startPtr = nbPtr->selectPtr;
    }
    if (nbPtr->focusPtr == NULL) {
	nbPtr->focusPtr = nbPtr->selectPtr;
	Blt_SetFocusItem(nbPtr->bindTable, nbPtr->focusPtr, NULL);
    }

    if (nbPtr->side & SIDE_VERTICAL) {
        width = Tk_Height(nbPtr->tkwin) - 2 * 
	    (nbPtr->corner + nbPtr->xSelectPad);
    } else {
        width = Tk_Width(nbPtr->tkwin) - (2 * nbPtr->inset) -
		nbPtr->xSelectPad - nbPtr->corner;
    }
    nbPtr->flags |= TNB_STATIC;
    if (nbPtr->reqTiers > 1) {
	int total, maxWidth;

	/* Static multiple tier mode. */

	/* Sum tab widths and determine the number of tiers needed. */
	nTiers = 1;
	total = x = 0;
	for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    if ((x + tabPtr->worldWidth) > width) {
		nTiers++;
		x = 0;
	    }
	    tabPtr->worldX = x;
	    tabPtr->tier = nTiers;
	    extra = tabPtr->worldWidth + nbPtr->gap - nbPtr->overlap;
	    total += extra, x += extra;
	}
	maxWidth = width;

	if (nTiers > nbPtr->reqTiers) {
	    /*
	     * The tabs do not fit into the requested number of tiers.
             * Go into scrolling mode.
	     */
	    width = ((total + nbPtr->tabWidth) / nbPtr->reqTiers);
	    x = 0;
	    nTiers = 1;
	    for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr);
		linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
		tabPtr = Blt_ChainGetValue(linkPtr);
		tabPtr->tier = nTiers;
		/*
		 * Keep adding tabs to a tier until we overfill it.
		 */
		tabPtr->worldX = x;
		x += tabPtr->worldWidth + nbPtr->gap - nbPtr->overlap;
		if (x > width) {
		    nTiers++;
		    if (x > maxWidth) {
			maxWidth = x;
		    }
		    x = 0;
		}
	    }
	    nbPtr->flags &= ~TNB_STATIC;
	}
	nbPtr->worldWidth = maxWidth;
	nbPtr->nTiers = nTiers;

	if (nTiers > 1) {
	    AdjustTabSizes(nbPtr, nTabs);
	}
	if (nbPtr->flags & TNB_STATIC) {
	    nbPtr->worldWidth = VPORTWIDTH(nbPtr);
	} else {
	    /* Do you add an offset ? */
	    nbPtr->worldWidth += (nbPtr->xSelectPad + nbPtr->corner);
	}
	nbPtr->worldWidth += nbPtr->overlap;
	if (nbPtr->selectPtr != NULL) {
	    RenumberTiers(nbPtr, nbPtr->selectPtr);
	}
    } else {
	/*
	 * Scrollable single tier mode.
	 */
	nTiers = 1;
	x = 0;
	for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    tabPtr->tier = nTiers;
	    tabPtr->worldX = x;
	    tabPtr->worldY = 0;
	    x += tabPtr->worldWidth + nbPtr->gap - nbPtr->overlap;
	}
	nbPtr->worldWidth = x + nbPtr->corner - nbPtr->gap +
	    nbPtr->xSelectPad + nbPtr->overlap;
	nbPtr->flags &= ~TNB_STATIC;
    }
    if (nTiers == 1) {
	nbPtr->yPad = nbPtr->ySelectPad;
    }
    nbPtr->nTiers = nTiers;
    nbPtr->pageTop = nbPtr->inset + nbPtr->yPad /* + 4 */ +
	(nbPtr->nTiers * nbPtr->tabHeight) + nbPtr->inset2;

    if (nbPtr->side & SIDE_VERTICAL) {
	for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    tabPtr->screenWidth = (short int)nbPtr->tabHeight;
	    tabPtr->screenHeight = (short int)tabPtr->worldWidth;
	}
    } else {
	for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    tabPtr->screenWidth = (short int)tabPtr->worldWidth;
	    tabPtr->screenHeight = (short int)nbPtr->tabHeight;
	}
    }
}

static void
ComputeVisibleTabs(nbPtr)
    Notebook *nbPtr;
{
    int nVisibleTabs;
    register Tab *tabPtr;
    Blt_ChainLink *linkPtr;

    nbPtr->nVisible = 0;
    if (Blt_ChainGetLength(nbPtr->chainPtr) == 0) {
	return;
    }
    nVisibleTabs = 0;
    if (nbPtr->flags & TNB_STATIC) {

	/* Static multiple tier mode. */

	for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    tabPtr->flags |= TAB_VISIBLE;
	    nVisibleTabs++;
	}
    } else {
	int width, offset;
	/*
	 * Scrollable (single or multiple) tier mode.
	 */
	offset = nbPtr->scrollOffset - (nbPtr->outerPad + nbPtr->xSelectPad);
	width = VPORTWIDTH(nbPtr) + nbPtr->scrollOffset +
	    2 * nbPtr->outerPad;
	for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    if ((tabPtr->worldX >= width) ||
		((tabPtr->worldX + tabPtr->worldWidth) < offset)) {
		tabPtr->flags &= ~TAB_VISIBLE;
	    } else {
		tabPtr->flags |= TAB_VISIBLE;
		nVisibleTabs++;
	    }
	}
    }
    for (linkPtr = Blt_ChainFirstLink(nbPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	tabPtr = Blt_ChainGetValue(linkPtr);
	tabPtr->screenX = tabPtr->screenY = -1000;
	if (tabPtr->flags & TAB_VISIBLE) {
	    WorldToScreen(nbPtr, tabPtr->worldX, tabPtr->worldY,
		&(tabPtr->screenX), &(tabPtr->screenY));
	    switch (nbPtr->side) {
	    case SIDE_RIGHT:
		tabPtr->screenX -= nbPtr->tabHeight;
		break;

	    case SIDE_BOTTOM:
		tabPtr->screenY -= nbPtr->tabHeight;
		break;
	    }
	}
    }
    nbPtr->nVisible = nVisibleTabs;
    Blt_PickCurrentItem(nbPtr->bindTable);
}


static void
Draw3DFolder(nbPtr, tabPtr, drawable, side, pointArr, nPoints)
    Notebook *nbPtr;
    Tab *tabPtr;
    Drawable drawable;
    int side;
    XPoint pointArr[];
    int nPoints;
{
    GC gc;
    int relief, borderWidth;
    Tk_3DBorder border;

    if (tabPtr == nbPtr->selectPtr) {
	border = GETATTR(tabPtr, selBorder);
    } else if (tabPtr->border != NULL) {
	border = tabPtr->border;
    } else {
	border = nbPtr->defTabStyle.border;
    }
    relief = nbPtr->defTabStyle.relief;
    if ((side == SIDE_RIGHT) || (side == SIDE_TOP)) {
	borderWidth = -nbPtr->defTabStyle.borderWidth;
	if (relief == TK_RELIEF_SUNKEN) {
	    relief = TK_RELIEF_RAISED;
	} else if (relief == TK_RELIEF_RAISED) {
	    relief = TK_RELIEF_SUNKEN;
	}
    } else {
	borderWidth = nbPtr->defTabStyle.borderWidth;
    }
    {
	int i;

#ifndef notdef
	int dx, dy;
	int oldType, newType;
	int start;

	dx = pointArr[0].x - pointArr[1].x;
	dy = pointArr[0].y - pointArr[1].y;
	oldType = ((dy < 0) || (dx > 0));
	start = 0;
	for (i = 1; i < nPoints; i++) {
	    dx = pointArr[i - 1].x - pointArr[i].x;
	    dy = pointArr[i - 1].y - pointArr[i].y;
	    newType = ((dy < 0) || (dx > 0));
	    if (newType != oldType) {
		if (oldType) {
		    gc = Tk_GCForColor(nbPtr->shadowColor, drawable);
		}  else {
		    gc = Tk_3DBorderGC(nbPtr->tkwin, border, TK_3D_FLAT_GC);
		}		    
		XDrawLines(nbPtr->display, drawable, gc, pointArr + start, 
			   i - start, CoordModeOrigin);
		start = i - 1;
		oldType = newType;
	    }
	}
	if (start != i) {
	    if (oldType) {
		gc = Tk_GCForColor(nbPtr->shadowColor, drawable);
	    }  else {
		gc = Tk_3DBorderGC(nbPtr->tkwin, border, TK_3D_FLAT_GC);
	    }		    
	    XDrawLines(nbPtr->display, drawable, gc, pointArr + start, 
		       i - start, CoordModeOrigin);
	}
#else
	/* Draw the outline of the folder. */
	gc = Tk_GCForColor(nbPtr->shadowColor, drawable);
	XDrawLines(nbPtr->display, drawable, gc, pointArr, nPoints, 
		   CoordModeOrigin);
#endif
    }
    /* And the folder itself. */
    if (tabPtr->tile != NULL) {
#ifdef notdef
	Tk_Fill3DPolygon(nbPtr->tkwin, drawable, border, pointArr, nPoints,
	    borderWidth, relief);
#endif
	Blt_TilePolygon(nbPtr->tkwin, drawable, tabPtr->tile, pointArr, 
			nPoints);
#ifdef notdef
	Tk_Draw3DPolygon(nbPtr->tkwin, drawable, border, pointArr, nPoints,
	    borderWidth, relief);
#endif
    } else {
	Tk_Fill3DPolygon(nbPtr->tkwin, drawable, border, pointArr, nPoints,
	    borderWidth, relief);
    }
}

/*
 *   x,y
 *    |1|2|3|   4    |3|2|1|
 *
 *   1. tab border width
 *   2. corner offset
 *   3. label pad
 *   4. label width
 *
 *
 */
static void
DrawLabel(nbPtr, tabPtr, drawable)
    Notebook *nbPtr;
    Tab *tabPtr;
    Drawable drawable;
{
    int x, y, dx, dy;
    int tx, ty, ix, iy;
    int imgWidth, imgHeight;
    int active, selected;
    XColor *fgColor, *bgColor;
    Tk_3DBorder border;
    GC gc;

    if (!(tabPtr->flags & TAB_VISIBLE)) {
	return;
    }
    x = tabPtr->screenX;
    y = tabPtr->screenY;

    active = (nbPtr->activePtr == tabPtr);
    selected = (nbPtr->selectPtr == tabPtr);

    fgColor = GETATTR(tabPtr, textColor);
    border = GETATTR(tabPtr, border);
    if (selected) {
	border = GETATTR(tabPtr, selBorder);
    }
    bgColor = Tk_3DBorderColor(border);
    if (active) {
	Tk_3DBorder activeBorder;

	activeBorder = GETATTR(tabPtr, activeBorder);
	bgColor = Tk_3DBorderColor(activeBorder);
    }
    dx = (tabPtr->screenWidth - tabPtr->labelWidth) / 2;
    dy = (tabPtr->screenHeight - tabPtr->labelHeight) / 2;


    /*
     * The label position is computed with screen coordinates.  This
     * is because both text and image components are oriented in
     * screen space, and not according to the orientation of the tabs
     * themselves.  That's why we have to consider the side when
     * correcting for left/right slants.
     */
    switch (nbPtr->side) {
    case SIDE_TOP:
    case SIDE_BOTTOM:
	if (nbPtr->slant == SLANT_LEFT) {
	    x += nbPtr->overlap;
	} else if (nbPtr->slant == SLANT_RIGHT) {
	    x -= nbPtr->overlap;
	}
	break;
    case SIDE_LEFT:
    case SIDE_RIGHT:
	if (nbPtr->slant == SLANT_LEFT) {
	    y += nbPtr->overlap;
	} else if (nbPtr->slant == SLANT_RIGHT) {
	    y -= nbPtr->overlap;
	}
	break;
    }

    /*
     * Draw the active or normal background color over the entire
     * label area.  This includes both the tab's text and image.
     * The rectangle should be 2 pixels wider/taller than this
     * area. So if the label consists of just an image, we get an
     * halo around the image when the tab is active.
     */
    gc = Tk_GCForColor(bgColor, drawable);
    XFillRectangle(nbPtr->display, drawable, gc, x + dx, y + dy,
	tabPtr->labelWidth, tabPtr->labelHeight);

    if ((nbPtr->flags & TNB_FOCUS) && (nbPtr->focusPtr == tabPtr)) {
	XDrawRectangle(nbPtr->display, drawable, nbPtr->defTabStyle.activeGC,
	    x + dx, y + dy, tabPtr->labelWidth - 1, tabPtr->labelHeight - 1);
    }
    tx = ty = ix = iy = 0;	/* Suppress compiler warning. */

    imgWidth = imgHeight = 0;
    if (tabPtr->image != NULL) {
	imgWidth = ImageWidth(tabPtr->image);
	imgHeight = ImageHeight(tabPtr->image);
    }
    switch (nbPtr->defTabStyle.textSide) {
    case SIDE_LEFT:
	tx = x + dx + tabPtr->iPadX.side1;
	ty = y + (tabPtr->screenHeight - tabPtr->textHeight) / 2;
	ix = tx + tabPtr->textWidth + IMAGE_PAD;
	iy = y + (tabPtr->screenHeight - imgHeight) / 2;
	break;
    case SIDE_RIGHT:
	ix = x + dx + tabPtr->iPadX.side1 + IMAGE_PAD;
	iy = y + (tabPtr->screenHeight - imgHeight) / 2;
	tx = ix + imgWidth;
	ty = y + (tabPtr->screenHeight - tabPtr->textHeight) / 2;
	break;
    case SIDE_BOTTOM:
	iy = y + dy + tabPtr->iPadY.side1 + IMAGE_PAD;
	ix = x + (tabPtr->screenWidth - imgWidth) / 2;
	ty = iy + imgHeight;
	tx = x + (tabPtr->screenWidth - tabPtr->textWidth) / 2;
	break;
    case SIDE_TOP:
	tx = x + (tabPtr->screenWidth - tabPtr->textWidth) / 2;
	ty = y + dy + tabPtr->iPadY.side1 + IMAGE_PAD;
	ix = x + (tabPtr->screenWidth - imgWidth) / 2;
	iy = ty + tabPtr->textHeight;
	break;
    }
    if (tabPtr->image != NULL) {
	Tk_RedrawImage(ImageBits(tabPtr->image), 0, 0, imgWidth, imgHeight,
	    drawable, ix, iy);
    }
    if (tabPtr->text != NULL) {
	TextStyle ts;
	XColor *activeColor;

	activeColor = fgColor;
	if (selected) {
	    activeColor = GETATTR(tabPtr, selColor);
	} else if (active) {
	    activeColor = GETATTR(tabPtr, activeFgColor);
	}
	Blt_SetDrawTextStyle(&ts, GETATTR(tabPtr, font), tabPtr->textGC,
	    fgColor, activeColor, tabPtr->shadow.color,
	    nbPtr->defTabStyle.rotate, TK_ANCHOR_NW, TK_JUSTIFY_LEFT,
	    0, tabPtr->shadow.offset);
	ts.state = tabPtr->state;
	ts.border = border;
	ts.padX.side1 = ts.padX.side2 = 2;
	if (selected || active) {
	    ts.state |= STATE_ACTIVE;
	}
	Blt_DrawText(nbPtr->tkwin, drawable, tabPtr->text, &ts, tx, ty);
    }
}


static void
DrawPerforation(nbPtr, tabPtr, drawable)
    Notebook *nbPtr;
    Tab *tabPtr;
    Drawable drawable;
{
    XPoint pointArr[2];
    int x, y;
    int segmentWidth, max;
    Tk_3DBorder border, perfBorder;

    if ((tabPtr->container != NULL) || (tabPtr->tkwin == NULL)) {
	return;
    }
    WorldToScreen(nbPtr, tabPtr->worldX + 2, 
	  tabPtr->worldY + tabPtr->worldHeight + 2, &x, &y);
    border = GETATTR(tabPtr, selBorder);
    segmentWidth = 3;
    if (nbPtr->flags & PERFORATION_ACTIVE) {
	perfBorder = GETATTR(tabPtr, activeBorder);
    } else {
	perfBorder = GETATTR(tabPtr, selBorder);
    }	
    if (nbPtr->side & SIDE_HORIZONTAL) {
	pointArr[0].x = x;
	pointArr[0].y = pointArr[1].y = y;
	max = tabPtr->screenX + tabPtr->screenWidth - 2;
	Blt_Fill3DRectangle(nbPtr->tkwin, drawable, perfBorder,
	       x - 2, y - 4, tabPtr->screenWidth, 8, 0, TK_RELIEF_FLAT);
	while (pointArr[0].x < max) {
	    pointArr[1].x = pointArr[0].x + segmentWidth;
	    if (pointArr[1].x > max) {
		pointArr[1].x = max;
	    }
	    Tk_Draw3DPolygon(nbPtr->tkwin, drawable, border, pointArr, 2, 1,
			     TK_RELIEF_RAISED);
	    pointArr[0].x += 2 * segmentWidth;
	}
    } else {
	pointArr[0].x = pointArr[1].x = x;
	pointArr[0].y = y;
	max  = tabPtr->screenY + tabPtr->screenHeight - 2;
	Blt_Fill3DRectangle(nbPtr->tkwin, drawable, perfBorder,
	       x - 4, y - 2, 8, tabPtr->screenHeight, 0, TK_RELIEF_FLAT);
	while (pointArr[0].y < max) {
	    pointArr[1].y = pointArr[0].y + segmentWidth;
	    if (pointArr[1].y > max) {
		pointArr[1].y = max;
	    }
	    Tk_Draw3DPolygon(nbPtr->tkwin, drawable, border, pointArr, 2, 1,
			     TK_RELIEF_RAISED);
	    pointArr[0].y += 2 * segmentWidth;
	}
    }
}

#define NextPoint(px, py) \
	pointPtr->x = (px), pointPtr->y = (py), pointPtr++, nPoints++
#define EndPoint(px, py) \
	pointPtr->x = (px), pointPtr->y = (py), nPoints++

#define BottomLeft(px, py) \
	NextPoint((px) + nbPtr->corner, (py)), \
	NextPoint((px), (py) - nbPtr->corner)

#define TopLeft(px, py) \
	NextPoint((px), (py) + nbPtr->corner), \
	NextPoint((px) + nbPtr->corner, (py))

#define TopRight(px, py) \
	NextPoint((px) - nbPtr->corner, (py)), \
	NextPoint((px), (py) + nbPtr->corner)

#define BottomRight(px, py) \
	NextPoint((px), (py) - nbPtr->corner), \
	NextPoint((px) - nbPtr->corner, (py))


/*
 * From the left edge:
 *
 *   |a|b|c|d|e| f |d|e|g|h| i |h|g|e|d|f|    j    |e|d|c|b|a|
 *
 *	a. highlight ring
 *	b. notebook 3D border
 *	c. outer gap
 *      d. page border
 *	e. page corner
 *	f. gap + select pad
 *	g. label pad x (worldX)
 *	h. internal pad x
 *	i. label width
 *	j. rest of page width
 *
 *  worldX, worldY
 *          |
 *          |
 *          * 4+ . . +5
 *          3+         +6
 *           .         .
 *           .         .
 *   1+. . .2+         +7 . . . .+8
 * 0+                              +9
 *  .                              .
 *  .                              .
 *13+                              +10
 *  12+-------------------------+11
 *
 */
static void
DrawFolder(nbPtr, tabPtr, drawable)
    Notebook *nbPtr;
    Tab *tabPtr;
    Drawable drawable;
{
    XPoint pointArr[16];
    XPoint *pointPtr;
    int width, height;
    int left, bottom, right, top, yBot, yTop;
    int x, y;
    register int i;
    int nPoints;

    width = VPORTWIDTH(nbPtr);
    height = VPORTHEIGHT(nbPtr);

    x = tabPtr->worldX;
    y = tabPtr->worldY;

    nPoints = 0;
    pointPtr = pointArr;

    /* Remember these are all world coordinates. */
    /*
     * x	Left side of tab.
     * y	Top of tab.
     * yTop	Top of folder.
     * yBot	Bottom of the tab.
     * left	Left side of the folder.
     * right	Right side of the folder.
     * top	Top of folder.
     * bottom	Bottom of folder.
     */
    left = nbPtr->scrollOffset - nbPtr->xSelectPad;
    right = left + width;
    yTop = y + tabPtr->worldHeight;
    yBot = nbPtr->pageTop - (nbPtr->inset + nbPtr->yPad);
    top = yBot - nbPtr->inset2 /* - 4 */;

    if (nbPtr->pageHeight == 0) {
	bottom = yBot + 2 * nbPtr->corner;
    } else {
	bottom = height - nbPtr->yPad - 1;
    }
    if (tabPtr != nbPtr->selectPtr) {

	/*
	 * Case 1: Unselected tab
	 *
	 * * 3+ . . +4
	 * 2+         +5
	 *  .         .
	 * 1+         +6
	 *   0+ . . +7
	 *
	 */
	
	if (nbPtr->slant & SLANT_LEFT) {
	    NextPoint(x, yBot);
	    NextPoint(x, yTop);
	    NextPoint(x + nbPtr->tabHeight, y);
	} else {
	    BottomLeft(x, yBot);
	    TopLeft(x, y);
	}
	x += tabPtr->worldWidth;
	if (nbPtr->slant & SLANT_RIGHT) {
	    NextPoint(x - nbPtr->tabHeight, y);
	    NextPoint(x, yTop);
	    NextPoint(x, yBot);
	} else {
	    TopRight(x, y);
	    BottomRight(x, yBot);
	}
    } else if (!(tabPtr->flags & TAB_VISIBLE)) {

	/*
	 * Case 2: Selected tab not visible in viewport.  Draw folder only.
	 *
	 * * 3+ . . +4
	 * 2+         +5
	 *  .         .
	 * 1+         +6
	 *   0+------+7
	 *
	 */

	TopLeft(left, top);
	TopRight(right, top);
	BottomRight(right, bottom);
	BottomLeft(left, bottom);
    } else {
	int flags;
	int tabWidth;

	x -= nbPtr->xSelectPad;
	y -= nbPtr->yPad;
	tabWidth = tabPtr->worldWidth + 2 * nbPtr->xSelectPad;

#define CLIP_NONE	0
#define CLIP_LEFT	(1<<0)
#define CLIP_RIGHT	(1<<1)
	flags = 0;
	if (x < left) {
	    flags |= CLIP_LEFT;
	}
	if ((x + tabWidth) > right) {
	    flags |= CLIP_RIGHT;
	}
	switch (flags) {
	case CLIP_NONE:

	    /*
	     *  worldX, worldY
	     *          |
	     *          * 4+ . . +5
	     *          3+         +6
	     *           .         .
	     *           .         .
	     *   1+. . .2+---------+7 . . . .+8
	     * 0+                              +9
	     *  .                              .
	     *  .                              .
	     *13+                              +10
	     *  12+ . . . . . . . . . . . . +11
	     */

	    if (x < (left + nbPtr->corner)) {
		NextPoint(left, top);
	    } else {
		TopLeft(left, top);
	    }
	    if (nbPtr->slant & SLANT_LEFT) {
		NextPoint(x, yTop);
		NextPoint(x + nbPtr->tabHeight + nbPtr->yPad, y);
	    } else {
		NextPoint(x, top);
		TopLeft(x, y);
	    }
	    x += tabWidth;
	    if (nbPtr->slant & SLANT_RIGHT) {
		NextPoint(x - nbPtr->tabHeight - nbPtr->yPad, y);
		NextPoint(x, yTop);
	    } else {
		TopRight(x, y);
		NextPoint(x, top);
	    }
	    if (x > (right - nbPtr->corner)) {
		NextPoint(right, top + nbPtr->corner);
	    } else {
		TopRight(right, top);
	    }
	    BottomRight(right, bottom);
	    BottomLeft(left, bottom);
	    break;

	case CLIP_LEFT:

	    /*
	     *  worldX, worldY
	     *          |
	     *          * 4+ . . +5
	     *          3+         +6
	     *           .         .
	     *           .         .
	     *          2+--------+7 . . . .+8
	     *            1+ . . . +0          +9
	     *                     .           .
	     *                     .           .
	     *                   13+           +10
	     *                     12+ . . . .+11
	     */

	    NextPoint(left, yBot);
	    if (nbPtr->slant & SLANT_LEFT) {
		NextPoint(x, yBot);
		NextPoint(x, yTop);
		NextPoint(x + nbPtr->tabHeight + nbPtr->yPad, y);
	    } else {
		BottomLeft(x, yBot);
		TopLeft(x, y);
	    }

	    x += tabWidth;
	    if (nbPtr->slant & SLANT_RIGHT) {
		NextPoint(x - nbPtr->tabHeight - nbPtr->yPad, y);
		NextPoint(x, yTop);
		NextPoint(x, top);
	    } else {
		TopRight(x, y);
		NextPoint(x, top);
	    }
	    if (x > (right - nbPtr->corner)) {
		NextPoint(right, top + nbPtr->corner);
	    } else {
		TopRight(right, top);
	    }
	    BottomRight(right, bottom);
	    BottomLeft(left, bottom);
	    break;

	case CLIP_RIGHT:

	    /*
	     *              worldX, worldY
	     *                     |
	     *                     * 9+ . . +10
	     *                     8+         +11
	     *                      .         .
	     *                      .         .
	     *           6+ . . . .7+---------+12
	     *         5+          0+ . . . +13
	     *          .           .
	     *          .           .
	     *         4+           +1
	     *           3+ . . . +2
	     */

	    NextPoint(right, yBot);
	    BottomRight(right, bottom);
	    BottomLeft(left, bottom);
	    if (x < (left + nbPtr->corner)) {
		NextPoint(left, top);
	    } else {
		TopLeft(left, top);
	    }
	    NextPoint(x, top);

	    if (nbPtr->slant & SLANT_LEFT) {
		NextPoint(x, yTop);
		NextPoint(x + nbPtr->tabHeight + nbPtr->yPad, y);
	    } else {
		TopLeft(x, y);
	    }
	    x += tabWidth;
	    if (nbPtr->slant & SLANT_RIGHT) {
		NextPoint(x - nbPtr->tabHeight - nbPtr->yPad, y);
		NextPoint(x, yTop);
		NextPoint(x, yBot);
	    } else {
		TopRight(x, y);
		BottomRight(x, yBot);
	    }
	    break;

	case (CLIP_LEFT | CLIP_RIGHT):

	    /*
	     *  worldX, worldY
	     *     |
	     *     * 4+ . . . . . . . . +5
	     *     3+                     +6
	     *      .                     .
	     *      .                     .
	     *     1+---------------------+7
	     *       2+ 0+          +9 .+8
	     *           .          .
	     *           .          .
	     *         13+          +10
	     *          12+ . . . +11
	     */

	    NextPoint(left, yBot);
	    if (nbPtr->slant & SLANT_LEFT) {
		NextPoint(x, yBot);
		NextPoint(x, yTop);
		NextPoint(x + nbPtr->tabHeight + nbPtr->yPad, y);
	    } else {
		BottomLeft(x, yBot);
		TopLeft(x, y);
	    }
	    x += tabPtr->worldWidth;
	    if (nbPtr->slant & SLANT_RIGHT) {
		NextPoint(x - nbPtr->tabHeight - nbPtr->yPad, y);
		NextPoint(x, yTop);
		NextPoint(x, yBot);
	    } else {
		TopRight(x, y);
		BottomRight(x, yBot);
	    }
	    NextPoint(right, yBot);
	    BottomRight(right, bottom);
	    BottomLeft(left, bottom);
	    break;
	}
    }
    EndPoint(pointArr[0].x, pointArr[0].y);
    for (i = 0; i < nPoints; i++) {
	WorldToScreen(nbPtr, pointArr[i].x, pointArr[i].y, &x, &y);
	pointArr[i].x = x;
	pointArr[i].y = y;
    }
    Draw3DFolder(nbPtr, tabPtr, drawable, nbPtr->side, pointArr, nPoints);
    DrawLabel(nbPtr, tabPtr, drawable);
    if (tabPtr->container != NULL) {
	XRectangle rect;

	/* Draw a rectangle covering the spot representing the window  */
	GetWindowRectangle(tabPtr, nbPtr->tkwin, FALSE, &rect);
	XFillRectangles(nbPtr->display, drawable, tabPtr->backGC,
	    &rect, 1);
    }
}

static void
DrawOuterBorders(nbPtr, drawable)
    Notebook *nbPtr;
    Drawable drawable;
{
    /*
     * Draw 3D border just inside of the focus highlight ring.  We
     * draw the border even if the relief is flat so that any tabs
     * that hang over the edge will be clipped.
     */
    if (nbPtr->borderWidth > 0) {
	Blt_Draw3DRectangle(nbPtr->tkwin, drawable, nbPtr->border,
	    nbPtr->highlightWidth, nbPtr->highlightWidth,
	    Tk_Width(nbPtr->tkwin) - 2 * nbPtr->highlightWidth,
	    Tk_Height(nbPtr->tkwin) - 2 * nbPtr->highlightWidth,
	    nbPtr->borderWidth, nbPtr->relief);
    }
    /* Draw focus highlight ring. */
    if (nbPtr->highlightWidth > 0) {
	XColor *color;
	GC gc;

	color = (nbPtr->flags & TNB_FOCUS)
	    ? nbPtr->highlightColor : nbPtr->highlightBgColor;
	gc = Tk_GCForColor(color, drawable);
	Tk_DrawFocusHighlight(nbPtr->tkwin, gc, nbPtr->highlightWidth, 
	      drawable);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DisplayNotebook --
 *
 * 	This procedure is invoked to display the widget.
 *
 *      Recomputes the layout of the widget if necessary. This is
 *	necessary if the world coordinate system has changed.
 *	Sets the vertical and horizontal scrollbars.  This is done
 *	here since the window width and height are needed for the
 *	scrollbar calculations.
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
DisplayNotebook(clientData)
    ClientData clientData;	/* Information about widget. */
{
    Notebook *nbPtr = clientData;
    Pixmap drawable;
    int width, height;

    nbPtr->flags &= ~TNB_REDRAW;
    if (nbPtr->tkwin == NULL) {
	return;			/* Window has been destroyed. */
    }
    if (nbPtr->flags & TNB_LAYOUT) {
	ComputeLayout(nbPtr);
	nbPtr->flags &= ~TNB_LAYOUT;
    }
    if ((nbPtr->reqHeight == 0) || (nbPtr->reqWidth == 0)) {
	width = height = 0;
	if (nbPtr->side & SIDE_VERTICAL) {
	    height = nbPtr->worldWidth;
	} else {
	    width = nbPtr->worldWidth;
	}
	if (nbPtr->reqWidth > 0) {
	    width = nbPtr->reqWidth;
	} else if (nbPtr->pageWidth > 0) {
	    width = nbPtr->pageWidth;
	}
	if (nbPtr->reqHeight > 0) {
	    height = nbPtr->reqHeight;
	} else if (nbPtr->pageHeight > 0) {
	    height = nbPtr->pageHeight;
	}
	if (nbPtr->side & SIDE_VERTICAL) {
	    width += nbPtr->pageTop + nbPtr->inset + nbPtr->inset2;
	    height += nbPtr->inset + nbPtr->inset2;
	} else {
	    height += nbPtr->pageTop + nbPtr->inset + nbPtr->inset2;
	    width += nbPtr->inset + nbPtr->inset2;
	}
	if ((Tk_ReqWidth(nbPtr->tkwin) != width) ||
	    (Tk_ReqHeight(nbPtr->tkwin) != height)) {
	    Tk_GeometryRequest(nbPtr->tkwin, width, height);
	}
    }
    if (nbPtr->flags & TNB_SCROLL) {
	width = VPORTWIDTH(nbPtr);
	nbPtr->scrollOffset = Blt_AdjustViewport(nbPtr->scrollOffset,
	    nbPtr->worldWidth, width, nbPtr->scrollUnits, 
	    BLT_SCROLL_MODE_CANVAS);
	if (nbPtr->scrollCmdPrefix != NULL) {
	    Blt_UpdateScrollbar(nbPtr->interp, nbPtr->scrollCmdPrefix,
		(double)nbPtr->scrollOffset / nbPtr->worldWidth,
		(double)(nbPtr->scrollOffset + width) / nbPtr->worldWidth);
	}
	ComputeVisibleTabs(nbPtr);
	nbPtr->flags &= ~TNB_SCROLL;
    }
    if (!Tk_IsMapped(nbPtr->tkwin)) {
	return;
    }
    height = Tk_Height(nbPtr->tkwin);
    drawable = Tk_GetPixmap(nbPtr->display, Tk_WindowId(nbPtr->tkwin),
	Tk_Width(nbPtr->tkwin), Tk_Height(nbPtr->tkwin),
	Tk_Depth(nbPtr->tkwin));

    /*
     * Clear the background either by tiling a pixmap or filling with
     * a solid color. Tiling takes precedence.
     */
    if (nbPtr->tile != NULL) {
	Blt_SetTileOrigin(nbPtr->tkwin, nbPtr->tile, 0, 0);
	Blt_TileRectangle(nbPtr->tkwin, drawable, nbPtr->tile, 0, 0,
	    Tk_Width(nbPtr->tkwin), height);
    } else {
	Blt_Fill3DRectangle(nbPtr->tkwin, drawable, nbPtr->border, 0, 0,
	    Tk_Width(nbPtr->tkwin), height, 0, TK_RELIEF_FLAT);
    }

    if (nbPtr->nVisible > 0) {
	register int i;
	register Tab *tabPtr;
	Blt_ChainLink *linkPtr;

	linkPtr = nbPtr->startPtr->linkPtr;
	for (i = 0; i < Blt_ChainGetLength(nbPtr->chainPtr); i++) {
	    linkPtr = Blt_ChainPrevLink(linkPtr);
	    if (linkPtr == NULL) {
		linkPtr = Blt_ChainLastLink(nbPtr->chainPtr);
	    }
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    if ((tabPtr != nbPtr->selectPtr) &&
		(tabPtr->flags & TAB_VISIBLE)) {
		DrawFolder(nbPtr, tabPtr, drawable);
	    }
	}
	DrawFolder(nbPtr, nbPtr->selectPtr, drawable);
	if (nbPtr->tearoff) {
	    DrawPerforation(nbPtr, nbPtr->selectPtr, drawable);
	}

	if ((nbPtr->selectPtr->tkwin != NULL) &&
	    (nbPtr->selectPtr->container == NULL)) {
	    XRectangle rect;

	    GetWindowRectangle(nbPtr->selectPtr, nbPtr->tkwin, FALSE, &rect);
	    ArrangeWindow(nbPtr->selectPtr->tkwin, &rect, 0);
	}
    }
    DrawOuterBorders(nbPtr, drawable);
    XCopyArea(nbPtr->display, drawable, Tk_WindowId(nbPtr->tkwin),
	nbPtr->highlightGC, 0, 0, Tk_Width(nbPtr->tkwin), height, 0, 0);
    Tk_FreePixmap(nbPtr->display, drawable);
}

/*
 * From the left edge:
 *
 *   |a|b|c|d|e| f |d|e|g|h| i |h|g|e|d|f|    j    |e|d|c|b|a|
 *
 *	a. highlight ring
 *	b. notebook 3D border
 *	c. outer gap
 *      d. page border
 *	e. page corner
 *	f. gap + select pad
 *	g. label pad x (worldX)
 *	h. internal pad x
 *	i. label width
 *	j. rest of page width
 *
 *  worldX, worldY
 *          |
 *          |
 *          * 4+ . . +5
 *          3+         +6
 *           .         .
 *           .         .
 *   1+. . .2+         +7 . . . .+8
 * 0+                              +9
 *  .                              .
 *  .                              .
 *13+                              +10
 *  12+-------------------------+11
 *
 */
static void
DisplayTearoff(clientData)
    ClientData clientData;
{
    Notebook *nbPtr;
    Tab *tabPtr;
    Drawable drawable;
    XPoint pointArr[16];
    XPoint *pointPtr;
    int width, height;
    int left, bottom, right, top;
    int x, y;
    int nPoints;
    Tk_Window tkwin;
    Tk_Window parent;
    XRectangle rect;

    tabPtr = clientData;
    if (tabPtr == NULL) {
	return;
    }
    tabPtr->flags &= ~TAB_REDRAW;
    nbPtr = tabPtr->nbPtr;
    if (nbPtr->tkwin == NULL) {
	return;
    }
    tkwin = tabPtr->container;
    drawable = Tk_WindowId(tkwin);

    /*
     * Clear the background either by tiling a pixmap or filling with
     * a solid color. Tiling takes precedence.
     */
    if (nbPtr->tile != NULL) {
	Blt_SetTileOrigin(tkwin, nbPtr->tile, 0, 0);
	Blt_TileRectangle(tkwin, drawable, nbPtr->tile, 0, 0, Tk_Width(tkwin), 
		Tk_Height(tkwin));
    } else {
	Blt_Fill3DRectangle(tkwin, drawable, nbPtr->border, 0, 0, 
		Tk_Width(tkwin), Tk_Height(tkwin), 0, TK_RELIEF_FLAT);
    }

    width = Tk_Width(tkwin) - 2 * nbPtr->inset;
    height = Tk_Height(tkwin) - 2 * nbPtr->inset;
    x = nbPtr->inset + nbPtr->gap + nbPtr->corner;
    y = nbPtr->inset;

    left = nbPtr->inset;
    right = nbPtr->inset + width;
    top = nbPtr->inset + nbPtr->corner + nbPtr->xSelectPad;
    bottom = nbPtr->inset + height;

    /*
     *  worldX, worldY
     *          |
     *          * 4+ . . +5
     *          3+         +6
     *           .         .
     *           .         .
     *   1+. . .2+         +7 . . . .+8
     * 0+                              +9
     *  .                              .
     *  .                              .
     *13+                              +10
     *  12+-------------------------+11
     */

    nPoints = 0;
    pointPtr = pointArr;

    TopLeft(left, top);
    NextPoint(x, top);
    TopLeft(x, y);
    x += tabPtr->worldWidth;
    TopRight(x, y);
    NextPoint(x, top);
    TopRight(right, top);
    BottomRight(right, bottom);
    BottomLeft(left, bottom);
    EndPoint(pointArr[0].x, pointArr[0].y);
    Draw3DFolder(nbPtr, tabPtr, drawable, SIDE_TOP, pointArr, nPoints);

    parent = (tabPtr->container == NULL) ? nbPtr->tkwin : tabPtr->container;
    GetWindowRectangle(tabPtr, parent, TRUE, &rect);
    ArrangeWindow(tabPtr->tkwin, &rect, TRUE);

    /* Draw 3D border. */
    if ((nbPtr->borderWidth > 0) && (nbPtr->relief != TK_RELIEF_FLAT)) {
	Blt_Draw3DRectangle(tkwin, drawable, nbPtr->border, 0, 0,
	    Tk_Width(tkwin), Tk_Height(tkwin), nbPtr->borderWidth,
	    nbPtr->relief);
    }
}

/*
 * --------------------------------------------------------------
 *
 * NotebookCmd --
 *
 * 	This procedure is invoked to process the "notebook" command.
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
static Blt_OpSpec notebookOps[] =
{
    {"activate", 1, (Blt_Op)ActivateOp, 3, 3, "index",},
    {"bind", 1, (Blt_Op)BindOp, 2, 5, "index ?sequence command?",},
    {"cget", 2, (Blt_Op)CgetOp, 3, 3, "option",},
    {"configure", 2, (Blt_Op)ConfigureOp, 2, 0, "?option value?...",},
    {"delete", 1, (Blt_Op)DeleteOp, 2, 0, "first ?last?",},
    {"focus", 1, (Blt_Op)FocusOp, 3, 3, "index",},
    {"highlight", 1, (Blt_Op)ActivateOp, 3, 3, "index",},
    {"id", 2, (Blt_Op)IdOp, 3, 3, "index",},
    {"index", 3, (Blt_Op)IndexOp, 3, 5, "string",},
    {"insert", 3, (Blt_Op)InsertOp, 3, 0, "index ?option value?",},
    {"invoke", 3, (Blt_Op)InvokeOp, 3, 3, "index",},
    {"move", 1, (Blt_Op)MoveOp, 5, 5, "name after|before index",},
    {"nearest", 1, (Blt_Op)NearestOp, 4, 4, "x y",},
    {"perforation", 1, (Blt_Op)PerforationOp, 2, 0, "args",},
    {"scan", 2, (Blt_Op)ScanOp, 5, 5, "dragto|mark x y",},
    {"see", 3, (Blt_Op)SeeOp, 3, 3, "index",},
    {"select", 3, (Blt_Op)SelectOp, 3, 3, "index",},
    {"size", 2, (Blt_Op)SizeOp, 2, 2, "",},
    {"tab", 1, (Blt_Op)TabOp, 2, 0, "oper args",},
    {"view", 1, (Blt_Op)ViewOp, 2, 5,
	"?moveto fract? ?scroll number what?",},
};

static int nNotebookOps = sizeof(notebookOps) / sizeof(Blt_OpSpec);

static int
NotebookInstCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about the widget. */
    Tcl_Interp *interp;		/* Interpreter to report errors back to. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Vector of argument strings. */
{
    Blt_Op proc;
    Notebook *nbPtr = clientData;
    int result;

    proc = Blt_GetOp(interp, nNotebookOps, notebookOps, BLT_OP_ARG1, argc, 
	argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve(nbPtr);
    result = (*proc) (nbPtr, interp, argc, argv);
    Tcl_Release(nbPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * NotebookInstDeletedCmd --
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
NotebookInstDeletedCmd(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Notebook *nbPtr = clientData;

    if (nbPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = nbPtr->tkwin;
	nbPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
#ifdef ITCL_NAMESPACES
	Itk_SetWidgetCommand(tkwin, (Tcl_Command) NULL);
#endif /* ITCL_NAMESPACES */
    }
}

/*
 * ------------------------------------------------------------------------
 *
 * NotebookCmd --
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
NotebookCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Notebook *nbPtr;
    Tk_Window tkwin;
    unsigned int mask;
    Tcl_CmdInfo cmdInfo;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " pathName ?option value?...\"", (char *)NULL);
	return TCL_ERROR;
    }
    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp), argv[1],
	(char *)NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    nbPtr = CreateNotebook(interp, tkwin);
    if (ConfigureNotebook(interp, nbPtr, argc - 2, argv + 2, 0) != TCL_OK) {
	Tk_DestroyWindow(nbPtr->tkwin);
	return TCL_ERROR;
    }
    mask = (ExposureMask | StructureNotifyMask | FocusChangeMask);
    Tk_CreateEventHandler(tkwin, mask, NotebookEventProc, nbPtr);
    nbPtr->cmdToken = Tcl_CreateCommand(interp, argv[1], NotebookInstCmd,
	nbPtr, NotebookInstDeletedCmd);
#ifdef ITCL_NAMESPACES
    Itk_SetWidgetCommand(nbPtr->tkwin, nbPtr->cmdToken);
#endif

    /*
     * Try to invoke a procedure to initialize various bindings on
     * tabs.  Source the file containing the procedure now if the
     * procedure isn't currently defined.  We deferred this to now so
     * that the user could set the variable "blt_library" within the
     * script.
     */
    if (!Tcl_GetCommandInfo(interp, "blt::TabnotebookInit", &cmdInfo)) {
	static char initCmd[] = 
	    "source [file join $blt_library tabnotebook.tcl]";

	if (Tcl_GlobalEval(interp, initCmd) != TCL_OK) {
	    char info[200];

	    sprintf(info, "\n    (while loading bindings for %s)", argv[0]);
	    Tcl_AddErrorInfo(interp, info);
	    Tk_DestroyWindow(nbPtr->tkwin);
	    return TCL_ERROR;
	}
    }
    if (Tcl_VarEval(interp, "blt::TabnotebookInit ", argv[1], (char *)NULL)
	!= TCL_OK) {
	Tk_DestroyWindow(nbPtr->tkwin);
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, Tk_PathName(nbPtr->tkwin), TCL_VOLATILE);
    return TCL_OK;
}

int
Blt_TabnotebookInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec =
    {
	"tabnotebook", NotebookCmd,
    };

    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#endif /* NO_TABNOTEBOOK */
