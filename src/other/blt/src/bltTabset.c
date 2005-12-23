/*
 * bltTabset.c --
 *
 *	This module implements a tabset widget for the BLT toolkit.
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
 *	Tabset widget created by George A. Howlett (gah@bell-labs.com)
 *
 */

#include "bltInt.h"

#ifndef NO_TABSET
#include "bltBind.h"
#include "bltChain.h"
#include "bltHash.h"
#include "bltTile.h"

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
   (((t)->attr != NULL) ? (t)->attr : (t)->setPtr->defTabStyle.attr)

/*
 * ----------------------------------------------------------------------------
 *
 *  Internal widget flags:
 *
 *	TABSET_LAYOUT		The layout of the widget needs to be
 *				recomputed.
 *
 *	TABSET_REDRAW		A redraw request is pending for the widget.
 *
 *	TABSET_SCROLL		A scroll request is pending.
 *
 *	TABSET_FOCUS		The widget is receiving keyboard events.
 *				Draw the focus highlight border around the
 *				widget.
 *
 *	TABSET_MULTIPLE_TIER	Tabset is using multiple tiers.
 *
 *	TABSET_STATIC		Tabset does not scroll.
 *
 * ---------------------------------------------------------------------------
 */
#define TABSET_LAYOUT		(1<<0)
#define TABSET_REDRAW		(1<<1)
#define TABSET_SCROLL		(1<<2)
#define TABSET_FOCUS		(1<<4)

#define TABSET_STATIC		(1<<8)
#define TABSET_MULTIPLE_TIER	(1<<9)

#define PERFORATION_ACTIVE	(1<<10)

#define SIDE_TOP		(1<<0)
#define SIDE_RIGHT		(1<<1)
#define SIDE_LEFT		(1<<2)
#define SIDE_BOTTOM		(1<<3)

#define SIDE_VERTICAL	(SIDE_LEFT | SIDE_RIGHT)
#define SIDE_HORIZONTAL (SIDE_TOP | SIDE_BOTTOM)

#define TAB_LABEL		(ClientData)0
#define TAB_PERFORATION		(ClientData)1

#define DEF_TABSET_ACTIVE_BACKGROUND	RGB_GREY90
#define DEF_TABSET_ACTIVE_BG_MONO	STD_ACTIVE_BG_MONO
#define DEF_TABSET_ACTIVE_FOREGROUND	STD_ACTIVE_FOREGROUND
#define DEF_TABSET_ACTIVE_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_TABSET_BG_MONO		STD_NORMAL_BG_MONO
#define DEF_TABSET_BACKGROUND		STD_NORMAL_BACKGROUND
#define DEF_TABSET_BORDERWIDTH		"1"
#define DEF_TABSET_COMMAND		(char *)NULL
#define DEF_TABSET_CURSOR		(char *)NULL
#define DEF_TABSET_DASHES		"1"
#define DEF_TABSET_FOREGROUND		STD_NORMAL_FOREGROUND
#define DEF_TABSET_FG_MONO		STD_NORMAL_FG_MONO
#define DEF_TABSET_FONT			STD_FONT
#define DEF_TABSET_GAP			"3"
#define DEF_TABSET_HEIGHT		"0"
#define DEF_TABSET_HIGHLIGHT_BACKGROUND	STD_NORMAL_BACKGROUND
#define DEF_TABSET_HIGHLIGHT_BG_MONO	STD_NORMAL_BG_MONO
#define DEF_TABSET_HIGHLIGHT_COLOR	RGB_BLACK
#define DEF_TABSET_HIGHLIGHT_WIDTH	"2"
#define DEF_TABSET_NORMAL_BACKGROUND 	STD_NORMAL_BACKGROUND
#define DEF_TABSET_NORMAL_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_TABSET_OUTER_PAD		"3"
#define DEF_TABSET_RELIEF		"sunken"
#define DEF_TABSET_ROTATE		"0.0"
#define DEF_TABSET_SCROLL_INCREMENT 	"0"
#define DEF_TABSET_SELECT_BACKGROUND 	STD_NORMAL_BACKGROUND
#define DEF_TABSET_SELECT_BG_MONO  	STD_SELECT_BG_MONO
#define DEF_TABSET_SELECT_BORDERWIDTH 	"1"
#define DEF_TABSET_SELECT_CMD		(char *)NULL
#define DEF_TABSET_SELECT_FOREGROUND 	STD_SELECT_FOREGROUND
#define DEF_TABSET_SELECT_FG_MONO  	STD_SELECT_FG_MONO
#define DEF_TABSET_SELECT_MODE		"multiple"
#define DEF_TABSET_SELECT_RELIEF	"raised"
#define DEF_TABSET_SELECT_PAD		"5"
#define DEF_TABSET_SHADOW_COLOR		RGB_BLACK
#define DEF_TABSET_SIDE			"top"
#define DEF_TABSET_SLANT		"none"
#define DEF_TABSET_TAB_BACKGROUND		RGB_GREY82
#define DEF_TABSET_TAB_BG_MONO		STD_SELECT_BG_MONO
#define DEF_TABSET_TAB_RELIEF		"raised"
#define DEF_TABSET_TAKE_FOCUS		"1"
#define DEF_TABSET_TEXT_COLOR		STD_NORMAL_FOREGROUND
#define DEF_TABSET_TEXT_MONO		STD_NORMAL_FG_MONO
#define DEF_TABSET_TEXT_SIDE		"left"
#define DEF_TABSET_TIERS		"1"
#define DEF_TABSET_TILE			(char *)NULL
#define DEF_TABSET_WIDTH		"0"
#define DEF_TABSET_SAME_WIDTH		"yes"
#define DEF_TABSET_TEAROFF		"yes"
#define DEF_TABSET_PAGE_WIDTH		"0"
#define DEF_TABSET_PAGE_HEIGHT		"0"

#define DEF_TAB_ACTIVE_BG		(char *)NULL
#define DEF_TAB_ACTIVE_FG		(char *)NULL
#define DEF_TAB_ANCHOR			"center"
#define DEF_TAB_BG			(char *)NULL
#define DEF_TAB_COMMAND			(char *)NULL
#define DEF_TAB_DATA			(char *)NULL
#define DEF_TAB_FG			(char *)NULL
#define DEF_TAB_FILL			"none"
#define DEF_TAB_FONT			(char *)NULL
#define DEF_TAB_HEIGHT			"0"
#define DEF_TAB_IMAGE			(char *)NULL
#define DEF_TAB_IPAD			"0"
#define DEF_TAB_PAD			"3"
#define DEF_TAB_PERF_COMMAND		(char *)NULL
#define DEF_TAB_SELECT_BG		(char *)NULL
#define DEF_TAB_SELECT_BORDERWIDTH 	"1"
#define DEF_TAB_SELECT_CMD		(char *)NULL
#define DEF_TAB_SELECT_FG	 	(char *)NULL
#define DEF_TAB_SHADOW			(char *)NULL
#define DEF_TAB_STATE			"normal"
#define DEF_TAB_STIPPLE			"BLT"
#define DEF_TAB_BIND_TAGS		"all"
#define DEF_TAB_TEXT			(char *)NULL
#define DEF_TAB_VISUAL			(char *)NULL
#define DEF_TAB_WIDTH			"0"
#define DEF_TAB_WINDOW			(char *)NULL

typedef struct TabsetStruct Tabset;

static void EmbeddedWidgetGeometryProc _ANSI_ARGS_((ClientData, Tk_Window));
static void EmbeddedWidgetCustodyProc _ANSI_ARGS_((ClientData, Tk_Window));

static Tk_GeomMgr tabMgrInfo =
{
    "tabset",			/* Name of geometry manager used by winfo */
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
static Tabset *tabSet;

static Tk_CustomOption imageOption =
{
    StringToImage, ImageToString, (ClientData)&tabSet,
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

    Tabset *setPtr;		/* Tabset that includes this
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

struct TabsetStruct {
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

    int pageTop;		/* Offset from top of tabset to the
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


    int side;			/* How tabset is oriented: either SIDE_LEFT,
				 * SIDE_RIGHT, SIDE_TOP, or SIDE_BOTTOM. */

    int slant;
    int overlap;
    int gap;
    int tabWidth, tabHeight;
    int xSelectPad, ySelectPad;	/* Padding around label of the selected tab. */
    int outerPad;		/* Padding around the exterior of the tabset
				 * and folder. */

    TabAttributes defTabStyle;	/* Global attribute information specific to
				 * tabs. */
    Blt_Tile tile;

    int reqWidth, reqHeight;	/* Requested dimensions of the tabset
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
	DEF_TABSET_ACTIVE_BACKGROUND, Tk_Offset(Tabset, defTabStyle.activeBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground",
	"activeBackground",
	DEF_TABSET_ACTIVE_BG_MONO, Tk_Offset(Tabset, defTabStyle.activeBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground",
	"activeForeground", DEF_TABSET_ACTIVE_FOREGROUND, 
	Tk_Offset(Tabset, defTabStyle.activeFgColor), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground",
	"activeForeground", DEF_TABSET_ACTIVE_FG_MONO, 
	Tk_Offset(Tabset, defTabStyle.activeFgColor), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_TABSET_BG_MONO, Tk_Offset(Tabset, border), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_TABSET_BACKGROUND, Tk_Offset(Tabset, border), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_TABSET_CURSOR, Tk_Offset(Tabset, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_TABSET_BORDERWIDTH, Tk_Offset(Tabset, borderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-dashes", "dashes", "Dashes",
	DEF_TABSET_DASHES, Tk_Offset(Tabset, defTabStyle.dashes),
	TK_CONFIG_NULL_OK, &bltDashesOption},
    {TK_CONFIG_SYNONYM, "-fg", "tabForeground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_TABSET_FONT, Tk_Offset(Tabset, defTabStyle.font), 0},
    {TK_CONFIG_SYNONYM, "-foreground", "tabForeground", (char *)NULL, 
	(char *)NULL, 0, 0},
    {TK_CONFIG_PIXELS, "-gap", "gap", "Gap",
	DEF_TABSET_GAP, Tk_Offset(Tabset, gap),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-height", "height", "Height",
	DEF_TABSET_HEIGHT, Tk_Offset(Tabset, reqHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground",
	DEF_TABSET_HIGHLIGHT_BACKGROUND, Tk_Offset(Tabset, highlightBgColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground",
	DEF_TABSET_HIGHLIGHT_BG_MONO, Tk_Offset(Tabset, highlightBgColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_TABSET_HIGHLIGHT_COLOR, Tk_Offset(Tabset, highlightColor), 0},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_TABSET_HIGHLIGHT_WIDTH, Tk_Offset(Tabset, highlightWidth),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-outerpad", "outerPad", "OuterPad",
	DEF_TABSET_OUTER_PAD, Tk_Offset(Tabset, outerPad),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-pageheight", "pageHeight", "PageHeight",
	DEF_TABSET_PAGE_HEIGHT, Tk_Offset(Tabset, reqPageHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-pagewidth", "pageWidth", "PageWidth",
	DEF_TABSET_PAGE_WIDTH, Tk_Offset(Tabset, reqPageWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_STRING, "-perforationcommand", "perforationcommand", 
	"PerforationCommand",
	DEF_TAB_PERF_COMMAND, Tk_Offset(Tabset, defTabStyle.perfCommand),
	TK_CONFIG_NULL_OK, &bltUidOption},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_TABSET_RELIEF, Tk_Offset(Tabset, relief), 0},
    {TK_CONFIG_DOUBLE, "-rotate", "rotate", "Rotate",
	DEF_TABSET_ROTATE, Tk_Offset(Tabset, defTabStyle.rotate),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, "-samewidth", "sameWidth", "SameWidth",
	DEF_TABSET_SAME_WIDTH, Tk_Offset(Tabset, defTabStyle.constWidth),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_STRING, "-scrollcommand", "scrollCommand", "ScrollCommand",
	(char *)NULL, Tk_Offset(Tabset, scrollCmdPrefix), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-scrollincrement", "scrollIncrement",
	"ScrollIncrement",
	DEF_TABSET_SCROLL_INCREMENT, Tk_Offset(Tabset, scrollUnits),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPositiveDistanceOption},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Foreground",
	DEF_TABSET_SELECT_BG_MONO, Tk_Offset(Tabset, defTabStyle.selBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Foreground",
	DEF_TABSET_SELECT_BACKGROUND, Tk_Offset(Tabset, defTabStyle.selBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_STRING, "-selectcommand", "selectCommand", "SelectCommand",
	DEF_TABSET_SELECT_CMD, Tk_Offset(Tabset, defTabStyle.command),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_TABSET_SELECT_FG_MONO, Tk_Offset(Tabset, defTabStyle.selColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_TABSET_SELECT_FOREGROUND, Tk_Offset(Tabset, defTabStyle.selColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_CUSTOM, "-selectpad", "selectPad", "SelectPad",
	DEF_TABSET_SELECT_PAD, Tk_Offset(Tabset, xSelectPad),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_COLOR, "-shadowcolor", "shadowColor", "ShadowColor",
	DEF_TABSET_SHADOW_COLOR, Tk_Offset(Tabset, shadowColor), 0},
    {TK_CONFIG_CUSTOM, "-side", "side", "side",
	DEF_TABSET_SIDE, Tk_Offset(Tabset, side),
	TK_CONFIG_DONT_SET_DEFAULT, &sideOption},
    {TK_CONFIG_CUSTOM, "-slant", "slant", "Slant",
	DEF_TABSET_SLANT, Tk_Offset(Tabset, slant),
	TK_CONFIG_DONT_SET_DEFAULT, &slantOption},
    {TK_CONFIG_BORDER, "-tabbackground", "tabBackground", "Background",
	DEF_TABSET_TAB_BG_MONO, Tk_Offset(Tabset, defTabStyle.border),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-tabbackground", "tabBackground", "Background",
	DEF_TABSET_TAB_BACKGROUND, Tk_Offset(Tabset, defTabStyle.border),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_CUSTOM, "-tabborderwidth", "tabBorderWidth", "BorderWidth",
	DEF_TABSET_BORDERWIDTH, Tk_Offset(Tabset, defTabStyle.borderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_COLOR, "-tabforeground", "tabForeground", "Foreground",
	DEF_TABSET_TEXT_COLOR, Tk_Offset(Tabset, defTabStyle.textColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-tabforeground", "tabForeground", "Foreground",
	DEF_TABSET_TEXT_MONO, Tk_Offset(Tabset, defTabStyle.textColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_RELIEF, "-tabrelief", "tabRelief", "TabRelief",
	DEF_TABSET_TAB_RELIEF, Tk_Offset(Tabset, defTabStyle.relief), 0},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_TABSET_TAKE_FOCUS, Tk_Offset(Tabset, takeFocus), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-tearoff", "tearoff", "Tearoff",
	DEF_TABSET_TEAROFF, Tk_Offset(Tabset, tearoff),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-textside", "textSide", "TextSide",
	DEF_TABSET_TEXT_SIDE, Tk_Offset(Tabset, defTabStyle.textSide),
	TK_CONFIG_DONT_SET_DEFAULT, &sideOption},
    {TK_CONFIG_CUSTOM, "-tiers", "tiers", "Tiers",
	DEF_TABSET_TIERS, Tk_Offset(Tabset, reqTiers),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPositiveCountOption},
    {TK_CONFIG_CUSTOM, "-tile", "tile", "Tile",
	(char *)NULL, Tk_Offset(Tabset, tile), TK_CONFIG_NULL_OK,
	&bltTileOption},
    {TK_CONFIG_CUSTOM, "-width", "width", "Width",
	DEF_TABSET_WIDTH, Tk_Offset(Tabset, reqWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

/* Forward Declarations */
static void DestroyTabset _ANSI_ARGS_((DestroyData dataPtr));
static void DestroyTearoff _ANSI_ARGS_((DestroyData dataPtr));
static void EmbeddedWidgetEventProc _ANSI_ARGS_((ClientData clientdata,
	XEvent *eventPtr));
static void TearoffEventProc _ANSI_ARGS_((ClientData clientdata,
	XEvent *eventPtr));
static void TabsetEventProc _ANSI_ARGS_((ClientData clientdata,
	XEvent *eventPtr));
static void DrawLabel _ANSI_ARGS_((Tabset *setPtr, Tab *tabPtr,
	Drawable drawable));
static void DrawFolder _ANSI_ARGS_((Tabset *setPtr, Tab *tabPtr,
	Drawable drawable));
static void DisplayTabset _ANSI_ARGS_((ClientData clientData));
static void DisplayTearoff _ANSI_ARGS_((ClientData clientData));
static void TabsetInstDeletedCmd _ANSI_ARGS_((ClientData clientdata));
static int TabsetInstCmd _ANSI_ARGS_((ClientData clientdata,
	Tcl_Interp *interp, int argc, char **argv));
static void GetWindowRectangle _ANSI_ARGS_((Tab *tabPtr, Tk_Window parent,
	int tearOff, XRectangle *rectPtr));
static void ArrangeWindow _ANSI_ARGS_((Tk_Window tkwin, XRectangle *rectPtr,
	int force));
static void EventuallyRedraw _ANSI_ARGS_((Tabset *setPtr));
static void EventuallyRedrawTearoff _ANSI_ARGS_((Tab *tabPtr));
static void ComputeLayout _ANSI_ARGS_((Tabset *setPtr));
static void DrawOuterBorders _ANSI_ARGS_((Tabset *setPtr, Drawable drawable));

static Tk_ImageChangedProc ImageChangedProc;
static Blt_TileChangedProc TileChangedProc;
static Blt_BindTagProc GetTags;
static Blt_BindPickProc PickTab;
static Tcl_IdleProc AdoptWindow;
static Tcl_CmdProc TabsetCmd;

static ClientData
MakeTag(setPtr, tagName)
    Tabset *setPtr;
    char *tagName;
{
    Blt_HashEntry *hPtr;
    int isNew;

    hPtr = Blt_CreateHashEntry(&(setPtr->tagTable), tagName, &isNew);
    assert(hPtr);
    return Blt_GetHashKey(&(setPtr->tagTable), hPtr);
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
WorldToScreen(setPtr, x, y, xScreenPtr, yScreenPtr)
    Tabset *setPtr;
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
    x += (setPtr->inset + setPtr->xSelectPad - setPtr->scrollOffset);
    y += setPtr->inset + setPtr->yPad;

    switch (setPtr->side) {
    case SIDE_TOP:
	sx = x, sy = y;		/* Do nothing */
	break;
    case SIDE_RIGHT:
	sx = Tk_Width(setPtr->tkwin) - y;
	sy = x;
	break;
    case SIDE_LEFT:
	sx = y, sy = x;		/* Flip coordinates */
	break;
    case SIDE_BOTTOM:
	sx = x;
	sy = Tk_Height(setPtr->tkwin) - y;
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
EventuallyRedraw(setPtr)
    Tabset *setPtr;
{
    if ((setPtr->tkwin != NULL) && !(setPtr->flags & TABSET_REDRAW)) {
	setPtr->flags |= TABSET_REDRAW;
	Tcl_DoWhenIdle(DisplayTabset, setPtr);
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
 *	and queue a request to re-layout and redraw the entire tabset.
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
    Tabset *setPtr = clientData;

    setPtr->flags |= (TABSET_LAYOUT | TABSET_SCROLL);
    EventuallyRedraw(setPtr);
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
GetImage(setPtr, interp, tkwin, name)
    Tabset *setPtr;
    Tcl_Interp *interp;
    Tk_Window tkwin;
    char *name;
{
    struct TabImageStruct *imagePtr;
    int isNew;
    Blt_HashEntry *hPtr;

    hPtr = Blt_CreateHashEntry(&(setPtr->imageTable), (char *)name, &isNew);
    if (isNew) {
	Tk_Image tkImage;
	int width, height;

	tkImage = Tk_GetImage(interp, tkwin, name, ImageChangedProc, setPtr);
	if (tkImage == NULL) {
	    Blt_DeleteHashEntry(&(setPtr->imageTable), hPtr);
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
	imagePtr = Blt_GetHashValue(hPtr);
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
FreeImage(setPtr, imagePtr)
    Tabset *setPtr;
    struct TabImageStruct *imagePtr;
{
    imagePtr->refCount--;
    if (imagePtr->refCount == 0) {
	Blt_DeleteHashEntry(&(setPtr->imageTable), imagePtr->hashPtr);
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
    ClientData clientData;	/* Contains a pointer to the tabset containing
				 * this image. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window associated with the tabset. */
    char *string;		/* String representation */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset to field in structure */
{
    Tabset *setPtr = *(Tabset **)clientData;
    TabImage *imagePtr = (TabImage *) (widgRec + offset);
    TabImage image;

    image = NULL;
    if ((string != NULL) && (*string != '\0')) {
	image = GetImage(setPtr, interp, tkwin, string);
	if (image == NULL) {
	    return TCL_ERROR;
	}
    }
    if (*imagePtr != NULL) {
	FreeImage(setPtr, *imagePtr);
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
    ClientData clientData;	/* Pointer to tabset containing image. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of field in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    Tabset *setPtr = *(Tabset **)clientData;
    TabImage *imagePtr = (TabImage *) (widgRec + offset);

    if (*imagePtr == NULL) {
	return "";
    }
    return Blt_GetHashKey(&(setPtr->imageTable), (*imagePtr)->hashPtr);
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
    Tabset *setPtr;

    old = *tkwinPtr;
    tkwin = NULL;
    setPtr = tabPtr->setPtr;
    if ((string != NULL) && (*string != '\0')) {
	tkwin = Tk_NameToWindow(interp, string, parent);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	if (tkwin == old) {
	    return TCL_OK;
	}
	/*
	 * Allow only widgets that are children of the tabset to be
	 * embedded into the page.  This way we can make assumptions about
	 * the window based upon its parent; either it's the tabset window
	 * or it has been torn off.
	 */
	parent = Tk_Parent(tkwin);
	if (parent != setPtr->tkwin) {
	    Tcl_AppendResult(interp, "can't manage \"", Tk_PathName(tkwin),
		"\" in tabset \"", Tk_PathName(setPtr->tkwin), "\"",
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
 *	designating the side of the tabset which to display tabs.
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

    tier = tabPtr->setPtr->nTiers - tabPtr->tier;
    return tier * tabPtr->setPtr->tabHeight;
}

static int
TabIndex(setPtr, tabPtr) 
    Tabset *setPtr;
    Tab *tabPtr;
{
    Tab *t2Ptr;
    int count;
    Blt_ChainLink *linkPtr;
    
    count = 0;
    for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
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
RenumberTiers(setPtr, tabPtr)
    Tabset *setPtr;
    Tab *tabPtr;
{
    int tier;
    Tab *prevPtr;
    Blt_ChainLink *linkPtr, *lastPtr;

    setPtr->focusPtr = setPtr->selectPtr = tabPtr;
    Blt_SetFocusItem(setPtr->bindTable, setPtr->focusPtr, NULL);

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
    setPtr->startPtr = tabPtr;
    for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	tabPtr = Blt_ChainGetValue(linkPtr);
	tabPtr->tier = (tabPtr->tier - tier + 1);
	if (tabPtr->tier < 1) {
	    tabPtr->tier += setPtr->nTiers;
	}
	tabPtr->worldY = WorldY(tabPtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * TabExists --
 *
 *	Searches for a tab based upon its name.
 *
 * Results:
 *	Returns TRUE if the tab exists, FALSE otherwise.
 *
 * ----------------------------------------------------------------------
 */
static int
TabExists(setPtr, string)
    Tabset *setPtr;
    char *string;
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&(setPtr->tabTable), string);
    return (hPtr != NULL);
}

/*
 * ----------------------------------------------------------------------
 *
 * GetTabByName --
 *
 *	Searches for a tab based upon its name.
 *
 * Results:
 *	A standard Tcl result.  An error message is generated if
 *	the tab can't be found.
 *
 * Side Effects:
 *	If the tab is found, *tabPtrPtr will contain the pointer to the
 *	tab structure.
 *
 * ----------------------------------------------------------------------
 */
static int
GetTabByName(setPtr, string, tabPtrPtr)
    Tabset *setPtr;
    char *string;
    Tab **tabPtrPtr;
{
    Blt_HashEntry *hPtr;
    *tabPtrPtr = NULL;

    hPtr = Blt_FindHashEntry(&(setPtr->tabTable), string);
    if (hPtr != NULL) {
	*tabPtrPtr = (Tab *)Blt_GetHashValue(hPtr);
	return TCL_OK;
    }
    Tcl_AppendResult(setPtr->interp, "can't find tab named \"", string,
     "\" in \"", Tk_PathName(setPtr->tkwin), "\"", (char *)NULL);
    return TCL_ERROR;
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
static ClientData
PickTab(clientData, x, y, contextPtr)
    ClientData clientData;
    int x, y;			/* Screen coordinates to test. */
    ClientData *contextPtr;	/* (out) If non-NULL, will contain the
				 * context of the tab selected.  */
{
    Tabset *setPtr = clientData;	/* Tabset widget record. */
    Tab *tabPtr;
    Blt_ChainLink *linkPtr;

    tabPtr = setPtr->selectPtr;
    if ((setPtr->tearoff) && (tabPtr != NULL) && 
	(tabPtr->container == NULL) && (tabPtr->tkwin != NULL)) {
	int top, bottom, left, right;
	int sx, sy;

	/* Check first for perforation on the selected tab. */
	WorldToScreen(setPtr, tabPtr->worldX + 2, 
	      tabPtr->worldY + tabPtr->worldHeight + 4, &sx, &sy);
	if (setPtr->side & SIDE_HORIZONTAL) {
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
	    return tabPtr;
	}
    } 
    for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
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
    if (tabPtr != NULL) {
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
    }
    return tabPtr;
}

static Tab *
TabRight(tabPtr)
    Tab *tabPtr;
{
    if (tabPtr != NULL) {
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
    }
    return tabPtr;
}

static Tab *
TabUp(tabPtr)
    Tab *tabPtr;
{
    if (tabPtr != NULL) {
	Tabset *setPtr;
	int x, y;
	int worldX, worldY;

	setPtr = tabPtr->setPtr;
	worldX = tabPtr->worldX + (tabPtr->worldWidth / 2);
	worldY = tabPtr->worldY - (setPtr->tabHeight / 2);
	WorldToScreen(setPtr, worldX, worldY, &x, &y);
	
	tabPtr = PickTab(setPtr, x, y, NULL);
	if (tabPtr == NULL) {
	    /*
	     * We might have inadvertly picked the gap between two tabs,
	     * so if the first pick fails, try again a little to the left.
	     */
	    WorldToScreen(setPtr, worldX + setPtr->gap, worldY, &x, &y);
	    tabPtr = PickTab(setPtr, x, y, NULL);
	}
	if ((tabPtr == NULL) &&
	    (setPtr->focusPtr->tier < (setPtr->nTiers - 1))) {
	    WorldToScreen(setPtr, worldX, worldY - setPtr->tabHeight, &x, &y);
	    tabPtr = PickTab(setPtr, x, y, NULL);
	}
	if (tabPtr == NULL) {
	    tabPtr = setPtr->focusPtr;
	}
    }
    return tabPtr;
}

static Tab *
TabDown(tabPtr)
    Tab *tabPtr;
{
    if (tabPtr != NULL) {
	Tabset *setPtr;
	int x, y;
	int worldX, worldY;

	setPtr = tabPtr->setPtr;
	worldX = tabPtr->worldX + (tabPtr->worldWidth / 2);
	worldY = tabPtr->worldY + (3 * setPtr->tabHeight) / 2;
	WorldToScreen(setPtr, worldX, worldY, &x, &y);
	tabPtr = PickTab(setPtr, x, y, NULL);
	if (tabPtr == NULL) {
	    /*
	     * We might have inadvertly picked the gap between two tabs,
	     * so if the first pick fails, try again a little to the left.
	     */
	    WorldToScreen(setPtr, worldX - setPtr->gap, worldY, &x, &y);
	    tabPtr = PickTab(setPtr, x, y, NULL);
	}
	if ((tabPtr == NULL) && (setPtr->focusPtr->tier > 2)) {
	    WorldToScreen(setPtr, worldX, worldY + setPtr->tabHeight, &x, &y);
	    tabPtr = PickTab(setPtr, x, y, NULL);
	}
	if (tabPtr == NULL) {
	    tabPtr = setPtr->focusPtr;
	}
    }
    return tabPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * GetTabByIndex --
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
GetTabByIndex(setPtr, string, tabPtrPtr, allowNull)
    Tabset *setPtr;
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
    if (setPtr->focusPtr == NULL) {
	setPtr->focusPtr = setPtr->selectPtr;
	Blt_SetFocusItem(setPtr->bindTable, setPtr->focusPtr, NULL);
    }
    if ((isdigit(UCHAR(c))) &&
	(Tcl_GetInt(setPtr->interp, string, &position) == TCL_OK)) {
	linkPtr = Blt_ChainGetNthLink(setPtr->chainPtr, position);
	if (linkPtr == NULL) {
	    Tcl_AppendResult(setPtr->interp, "can't find tab \"", string,
		"\" in \"", Tk_PathName(setPtr->tkwin), "\": no such index", 
		(char *)NULL);
	    return TCL_ERROR;
	}
	tabPtr = Blt_ChainGetValue(linkPtr);
    } else if ((c == 'a') && (strcmp(string, "active") == 0)) {
	tabPtr = setPtr->activePtr;
    } else if ((c == 'c') && (strcmp(string, "current") == 0)) {
	tabPtr = (Tab *)Blt_GetCurrentItem(setPtr->bindTable);
    } else if ((c == 's') && (strcmp(string, "select") == 0)) {
	tabPtr = setPtr->selectPtr;
    } else if ((c == 'f') && (strcmp(string, "focus") == 0)) {
	tabPtr = setPtr->focusPtr;
    } else if ((c == 'u') && (strcmp(string, "up") == 0)) {
	switch (setPtr->side) {
	case SIDE_LEFT:
	case SIDE_RIGHT:
	    tabPtr = TabLeft(setPtr->focusPtr);
	    break;
	    
	case SIDE_BOTTOM:
	    tabPtr = TabDown(setPtr->focusPtr);
	    break;
	    
	case SIDE_TOP:
	    tabPtr = TabUp(setPtr->focusPtr);
	    break;
	}
    } else if ((c == 'd') && (strcmp(string, "down") == 0)) {
	switch (setPtr->side) {
	case SIDE_LEFT:
	case SIDE_RIGHT:
	    tabPtr = TabRight(setPtr->focusPtr);
	    break;
	    
	case SIDE_BOTTOM:
	    tabPtr = TabUp(setPtr->focusPtr);
	    break;
	    
	case SIDE_TOP:
	    tabPtr = TabDown(setPtr->focusPtr);
	    break;
	}
    } else if ((c == 'l') && (strcmp(string, "left") == 0)) {
	switch (setPtr->side) {
	case SIDE_LEFT:
	    tabPtr = TabUp(setPtr->focusPtr);
	    break;
	    
	case SIDE_RIGHT:
	    tabPtr = TabDown(setPtr->focusPtr);
	    break;
	    
	case SIDE_BOTTOM:
	case SIDE_TOP:
	    tabPtr = TabLeft(setPtr->focusPtr);
	    break;
	}
    } else if ((c == 'r') && (strcmp(string, "right") == 0)) {
	switch (setPtr->side) {
	case SIDE_LEFT:
	    tabPtr = TabDown(setPtr->focusPtr);
	    break;
	    
	case SIDE_RIGHT:
	    tabPtr = TabUp(setPtr->focusPtr);
	    break;
	    
	case SIDE_BOTTOM:
	case SIDE_TOP:
	    tabPtr = TabRight(setPtr->focusPtr);
	    break;
	}
    } else if ((c == 'e') && (strcmp(string, "end") == 0)) {
	linkPtr = Blt_ChainLastLink(setPtr->chainPtr);
	if (linkPtr != NULL) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	}
    } else if (c == '@') {
	int x, y;

	if (Blt_GetXY(setPtr->interp, setPtr->tkwin, string, &x, &y) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	tabPtr = PickTab(setPtr, x, y, NULL);
    } else {
	Tcl_AppendResult(setPtr->interp, "can't find tab \"", string,
	    "\" in \"", Tk_PathName(setPtr->tkwin), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    *tabPtrPtr = tabPtr;
    Tcl_ResetResult(setPtr->interp);

    if ((!allowNull) && (tabPtr == NULL)) {
	Tcl_AppendResult(setPtr->interp, "can't find tab \"", string,
	    "\" in \"", Tk_PathName(setPtr->tkwin), "\"", (char *)NULL);
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
 * 	events on embedded widgets contained in the tabset.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When an embedded widget gets deleted, internal structures get
 *	cleaned up.  When it gets resized, the tabset is redisplayed.
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
	    (tabPtr->setPtr->selectPtr == tabPtr)) {
	    EventuallyRedraw(tabPtr->setPtr);
	}
	break;

    case DestroyNotify:
	/*
	 * Mark the tab as deleted by dereferencing the Tk window
	 * pointer. Redraw the window only if the tab is currently
	 * visible.
	 */
	if ((Tk_IsMapped(tabPtr->tkwin)) &&
	    (tabPtr->setPtr->selectPtr == tabPtr)) {
	    EventuallyRedraw(tabPtr->setPtr);
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
    Tabset *setPtr;

    if ((tabPtr == NULL) || (tabPtr->tkwin == NULL)) {
	return;
    }
    setPtr = tabPtr->setPtr;
    if (tabPtr->container != NULL) {
	Tcl_EventuallyFree(tabPtr, DestroyTearoff);
    }
    /*
     * Mark the tab as deleted by dereferencing the Tk window
     * pointer. Redraw the window only if the tab is currently
     * visible.
     */
    if (tabPtr->tkwin != NULL) {
	if (Tk_IsMapped(tabPtr->tkwin) && (setPtr->selectPtr == tabPtr)) {
	    setPtr->flags |= (TABSET_LAYOUT | TABSET_SCROLL);
	    EventuallyRedraw(setPtr);
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
    tabPtr->setPtr->flags |= (TABSET_LAYOUT | TABSET_SCROLL);
    EventuallyRedraw(tabPtr->setPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyTab --
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyTab(setPtr, tabPtr)
    Tabset *setPtr;
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
    if (tabPtr == setPtr->activePtr) {
	setPtr->activePtr = NULL;
    }
    if (tabPtr == setPtr->selectPtr) {
	setPtr->selectPtr = NextOrLastTab(tabPtr);
    }
    if (tabPtr == setPtr->focusPtr) {
	setPtr->focusPtr = setPtr->selectPtr;
	Blt_SetFocusItem(setPtr->bindTable, setPtr->focusPtr, NULL);
    }
    if (tabPtr == setPtr->startPtr) {
	setPtr->startPtr = NULL;
    }
    Tk_FreeOptions(tabConfigSpecs, (char *)tabPtr, setPtr->display, 0);
    if (tabPtr->text != NULL) {
	Blt_FreeUid(tabPtr->text);
    }
    hPtr = Blt_FindHashEntry(&(setPtr->tabTable), tabPtr->name);
    assert(hPtr);
    Blt_DeleteHashEntry(&(setPtr->tabTable), hPtr);

    if (tabPtr->image != NULL) {
	FreeImage(setPtr, tabPtr->image);
    }
    if (tabPtr->name != NULL) {
	Blt_Free(tabPtr->name);
    }
    if (tabPtr->textGC != NULL) {
	Tk_FreeGC(setPtr->display, tabPtr->textGC);
    }
    if (tabPtr->backGC != NULL) {
	Tk_FreeGC(setPtr->display, tabPtr->backGC);
    }
    if (tabPtr->command != NULL) {
	Blt_FreeUid(tabPtr->command);
    }
    if (tabPtr->linkPtr != NULL) {
	Blt_ChainDeleteLink(setPtr->chainPtr, tabPtr->linkPtr);
    }
    if (tabPtr->tags != NULL) {
	Blt_FreeUid(tabPtr->tags);
    }
    Blt_DeleteBindings(setPtr->bindTable, tabPtr);
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
CreateTab(setPtr, name)
    Tabset *setPtr;
    char *name;
{
    Tab *tabPtr;
    Blt_HashEntry *hPtr;
    int isNew;

    tabPtr = Blt_Calloc(1, sizeof(Tab));
    assert(tabPtr);
    tabPtr->setPtr = setPtr;
    tabPtr->name = Blt_Strdup(name);
    tabPtr->text = Blt_GetUid(name);
    tabPtr->fill = FILL_NONE;
    tabPtr->anchor = TK_ANCHOR_CENTER;
    tabPtr->container = NULL;
    tabPtr->state = STATE_NORMAL;
    hPtr = Blt_CreateHashEntry(&(setPtr->tabTable), name, &isNew);
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
    Tabset *setPtr = clientData;

    if (setPtr->tkwin != NULL) {
	EventuallyRedraw(setPtr);
    }
}

static int
ConfigureTab(setPtr, tabPtr)
    Tabset *setPtr;
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
	Blt_GetBoundingBox(labelWidth, labelHeight, setPtr->defTabStyle.rotate,
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
	if (setPtr->defTabStyle.textSide & SIDE_VERTICAL) {
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
	newGC = Tk_GetGC(setPtr->tkwin, gcMask, &gcValues);
    }
    if (tabPtr->textGC != NULL) {
	Tk_FreeGC(setPtr->display, tabPtr->textGC);
    }
    tabPtr->textGC = newGC;

    gcMask = GCForeground | GCStipple | GCFillStyle;
    gcValues.fill_style = FillStippled;
    border = GETATTR(tabPtr, border);
    gcValues.foreground = Tk_3DBorderColor(border)->pixel;
    gcValues.stipple = tabPtr->stipple;
    newGC = Tk_GetGC(setPtr->tkwin, gcMask, &gcValues);
    if (tabPtr->backGC != NULL) {
	Tk_FreeGC(setPtr->display, tabPtr->backGC);
    }
    tabPtr->backGC = newGC;
    /*
     * GC for tiled background.
     */
    if (tabPtr->tile != NULL) {
	Blt_SetTileChangedProc(tabPtr->tile, TileChangedProc, setPtr);
    }
    if (tabPtr->flags & TAB_VISIBLE) {
	EventuallyRedraw(setPtr);
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
    Tabset *setPtr;
    int cavityWidth, cavityHeight;
    int width, height;
    int dx, dy;
    int x, y;

    setPtr = tabPtr->setPtr;
    pad = setPtr->inset + setPtr->inset2;

    if (!tearoff) {
	switch (setPtr->side) {
	case SIDE_RIGHT:
	case SIDE_BOTTOM:
	    x = setPtr->inset + setPtr->inset2;
	    y = setPtr->inset + setPtr->inset2;
	    break;

	case SIDE_LEFT:
	    x = setPtr->pageTop;
	    y = setPtr->inset + setPtr->inset2;
	    break;

	case SIDE_TOP:
	    x = setPtr->inset + setPtr->inset2;
	    y = setPtr->pageTop;
	    break;
	}

	if (setPtr->side & SIDE_VERTICAL) {
	    cavityWidth = Tk_Width(setPtr->tkwin) - (setPtr->pageTop + pad);
	    cavityHeight = Tk_Height(setPtr->tkwin) - (2 * pad);
	} else {
	    cavityWidth = Tk_Width(setPtr->tkwin) - (2 * pad);
	    cavityHeight = Tk_Height(setPtr->tkwin) - (setPtr->pageTop + pad);
	}

    } else {
	x = setPtr->inset + setPtr->inset2;
#define TEAR_OFF_TAB_SIZE	5
	y = setPtr->inset + setPtr->inset2 + setPtr->yPad + setPtr->outerPad +
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
	Tk_MoveResizeWindow(tkwin, rectPtr->x, rectPtr->y, rectPtr->width,
	    rectPtr->height);
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
    ClientData context;		/* Not used. */
    Blt_List list;
{
    Tab *tabPtr = (Tab *)object;
    Tabset *setPtr;

    setPtr = (Tabset *)table->clientData;
    if (context == TAB_PERFORATION) {
	Blt_ListAppend(list, MakeTag(setPtr, "Perforation"), 0);
    } else if (context == TAB_LABEL) {
	Blt_ListAppend(list, MakeTag(setPtr, tabPtr->name), 0);
	if (tabPtr->tags != NULL) {
	    int nNames;
	    char **names;
	    register char **p;
	    
	    /* 
	     * This is a space/time trade-off in favor of space.  The tags
	     * are stored as character strings in a hash table.  That way,
	     * tabs can share the strings. It's likely that they will.  The
	     * down side is that the same string is split over an over again. 
	     */
	    if (Tcl_SplitList((Tcl_Interp *)NULL, tabPtr->tags, &nNames, 
		      &names) == TCL_OK) {
		for (p = names; *p != NULL; p++) {
		    Blt_ListAppend(list, MakeTag(setPtr, *p), 0);
		}
		Blt_Free(names);
	    }
	}
    }
}

/*
 * --------------------------------------------------------------
 *
 * TabsetEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on tabset widgets.
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
TabsetEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Tabset *setPtr = clientData;

    switch (eventPtr->type) {
    case Expose:
	if (eventPtr->xexpose.count == 0) {
	    EventuallyRedraw(setPtr);
	}
	break;

    case ConfigureNotify:
	setPtr->flags |= (TABSET_LAYOUT | TABSET_SCROLL);
	EventuallyRedraw(setPtr);
	break;

    case FocusIn:
    case FocusOut:
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    if (eventPtr->type == FocusIn) {
		setPtr->flags |= TABSET_FOCUS;
	    } else {
		setPtr->flags &= ~TABSET_FOCUS;
	    }
	    EventuallyRedraw(setPtr);
	}
	break;

    case DestroyNotify:
	if (setPtr->tkwin != NULL) {
	    setPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(setPtr->interp, setPtr->cmdToken);
	}
	if (setPtr->flags & TABSET_REDRAW) {
	    Tcl_CancelIdleCall(DisplayTabset, setPtr);
	}
	Tcl_EventuallyFree(setPtr, DestroyTabset);
	break;

    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyTabset --
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
DestroyTabset(dataPtr)
    DestroyData dataPtr;	/* Pointer to the widget record. */
{
    Tabset *setPtr = (Tabset *)dataPtr;
    Tab *tabPtr;
    Blt_ChainLink *linkPtr;

    if (setPtr->highlightGC != NULL) {
	Tk_FreeGC(setPtr->display, setPtr->highlightGC);
    }
    if (setPtr->tile != NULL) {
	Blt_FreeTile(setPtr->tile);
    }
    if (setPtr->defTabStyle.activeGC != NULL) {
	Blt_FreePrivateGC(setPtr->display, setPtr->defTabStyle.activeGC);
    }
    for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	tabPtr = Blt_ChainGetValue(linkPtr);
	tabPtr->linkPtr = NULL;
	DestroyTab(setPtr, tabPtr);
    }
    Blt_ChainDestroy(setPtr->chainPtr);
    Blt_DestroyBindingTable(setPtr->bindTable);
    Blt_DeleteHashTable(&(setPtr->tabTable));
    Blt_DeleteHashTable(&(setPtr->tagTable));
    Tk_FreeOptions(configSpecs, (char *)setPtr, setPtr->display, 0);
    Blt_Free(setPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * CreateTabset --
 *
 * ----------------------------------------------------------------------
 */
static Tabset *
CreateTabset(interp, tkwin)
    Tcl_Interp *interp;
    Tk_Window tkwin;
{
    Tabset *setPtr;

    setPtr = Blt_Calloc(1, sizeof(Tabset));
    assert(setPtr);

    Tk_SetClass(tkwin, "Tabset");
    setPtr->tkwin = tkwin;
    setPtr->display = Tk_Display(tkwin);
    setPtr->interp = interp;

    setPtr->flags |= (TABSET_LAYOUT | TABSET_SCROLL);
    setPtr->side = SIDE_TOP;
    setPtr->borderWidth = setPtr->highlightWidth = 2;
    setPtr->ySelectPad = SELECT_PADY;
    setPtr->xSelectPad = SELECT_PADX;
    setPtr->relief = TK_RELIEF_SUNKEN;
    setPtr->defTabStyle.relief = TK_RELIEF_RAISED;
    setPtr->defTabStyle.borderWidth = 1;
    setPtr->defTabStyle.constWidth = TRUE;
    setPtr->defTabStyle.textSide = SIDE_LEFT;
    setPtr->scrollUnits = 2;
    setPtr->corner = CORNER_OFFSET;
    setPtr->gap = GAP;
    setPtr->outerPad = OUTER_PAD;
    setPtr->slant = SLANT_NONE;
    setPtr->overlap = 0;
    setPtr->tearoff = TRUE;
    setPtr->bindTable = Blt_CreateBindingTable(interp, tkwin, setPtr, PickTab, 
	GetTags);
    setPtr->chainPtr = Blt_ChainCreate();
    Blt_InitHashTable(&(setPtr->tabTable), BLT_STRING_KEYS);
    Blt_InitHashTable(&(setPtr->imageTable), BLT_STRING_KEYS);
    Blt_InitHashTable(&(setPtr->tagTable), BLT_STRING_KEYS);
#if (TK_MAJOR_VERSION > 4)
    Blt_SetWindowInstanceData(tkwin, setPtr);
#endif
    return setPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureTabset --
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
 *	etc. get set for setPtr; old resources get freed, if there
 *	were any.  The widget is redisplayed.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureTabset(interp, setPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Interpreter to report errors. */
    Tabset *setPtr;		/* Information about widget; may or
			         * may not already have values for
			         * some fields. */
    int argc;
    char **argv;
    int flags;
{
    XGCValues gcValues;
    unsigned long gcMask;
    GC newGC;

    tabSet = setPtr;
    if (Tk_ConfigureWidget(interp, setPtr->tkwin, configSpecs, argc, argv,
	    (char *)setPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_ConfigModified(configSpecs, "-width", "-height", "-side", "-gap",
	    "-slant", (char *)NULL)) {
	setPtr->flags |= (TABSET_LAYOUT | TABSET_SCROLL);
    }
    if ((setPtr->reqHeight > 0) && (setPtr->reqWidth > 0)) {
	Tk_GeometryRequest(setPtr->tkwin, setPtr->reqWidth, setPtr->reqHeight);
    }
    /*
     * GC for focus highlight.
     */
    gcMask = GCForeground;
    gcValues.foreground = setPtr->highlightColor->pixel;
    newGC = Tk_GetGC(setPtr->tkwin, gcMask, &gcValues);
    if (setPtr->highlightGC != NULL) {
	Tk_FreeGC(setPtr->display, setPtr->highlightGC);
    }
    setPtr->highlightGC = newGC;

    /*
     * GC for tiled background.
     */
    if (setPtr->tile != NULL) {
	Blt_SetTileChangedProc(setPtr->tile, TileChangedProc, setPtr);
    }
    /*
     * GC for active line.
     */
    gcMask = GCForeground | GCLineWidth | GCLineStyle | GCCapStyle;
    gcValues.foreground = setPtr->defTabStyle.activeFgColor->pixel;
    gcValues.line_width = 0;
    gcValues.cap_style = CapProjecting;
    gcValues.line_style = (LineIsDashed(setPtr->defTabStyle.dashes))
	? LineOnOffDash : LineSolid;

    newGC = Blt_GetPrivateGC(setPtr->tkwin, gcMask, &gcValues);
    if (LineIsDashed(setPtr->defTabStyle.dashes)) {
	setPtr->defTabStyle.dashes.offset = 2;
	Blt_SetDashes(setPtr->display, newGC, &(setPtr->defTabStyle.dashes));
    }
    if (setPtr->defTabStyle.activeGC != NULL) {
	Blt_FreePrivateGC(setPtr->display, setPtr->defTabStyle.activeGC);
    }
    setPtr->defTabStyle.activeGC = newGC;

    setPtr->defTabStyle.rotate = FMOD(setPtr->defTabStyle.rotate, 360.0);
    if (setPtr->defTabStyle.rotate < 0.0) {
	setPtr->defTabStyle.rotate += 360.0;
    }
    setPtr->inset = setPtr->highlightWidth + setPtr->borderWidth +
	setPtr->outerPad;
    if (Blt_ConfigModified(configSpecs, "-font", "-*foreground", "-rotate",
	    "-*background", "-side", (char *)NULL)) {
	Blt_ChainLink *linkPtr;
	Tab *tabPtr;

	for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    ConfigureTab(setPtr, tabPtr);
	}
	setPtr->flags |= (TABSET_LAYOUT | TABSET_SCROLL);
    }
    setPtr->inset2 = setPtr->defTabStyle.borderWidth + setPtr->corner;
    EventuallyRedraw(setPtr);
    return TCL_OK;
}

/*
 * --------------------------------------------------------------
 *
 * Tabset operations
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
ActivateOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr, *oldPtr, *selPtr;
    Drawable drawable;

    if (argv[2][0] == '\0') {
	tabPtr = NULL;
    } else if (GetTabByIndex(setPtr, argv[2], &tabPtr, INVALID_OK) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((tabPtr != NULL) && (tabPtr->state == STATE_DISABLED)) {
	tabPtr = NULL;
    }
    oldPtr = setPtr->activePtr;

    setPtr->activePtr = tabPtr;
    drawable = Tk_WindowId(setPtr->tkwin);
    if (tabPtr != oldPtr) {
	int redraw;

	selPtr = setPtr->selectPtr;
	redraw = FALSE;
	if (oldPtr != NULL) {
	    if ((selPtr != NULL) && 
		((oldPtr == TabLeft(selPtr)) || (oldPtr == TabRight(selPtr)))) {
		redraw = TRUE;
	    }
	    if ((selPtr != NULL) && (oldPtr->tier == 2) &&
		(oldPtr->worldX + oldPtr->worldWidth) >= (selPtr->worldX) &&
		(oldPtr->worldX < (selPtr->worldX + selPtr->worldWidth))) {
		redraw = TRUE;
	    } else {
		DrawLabel(setPtr, oldPtr, drawable);
	    }
	}
	if ((tabPtr != NULL) && (!redraw)) {
	    if ((selPtr != NULL) && 
		((tabPtr == TabLeft(selPtr)) || (tabPtr == TabRight(selPtr)))) {
		redraw = TRUE;
	    }
	    if ((selPtr != NULL) && (tabPtr->tier == 2) &&
		(tabPtr->worldX + tabPtr->worldWidth) >= (selPtr->worldX) &&
		(tabPtr->worldX < (selPtr->worldX + selPtr->worldWidth))) {
		redraw = TRUE;
	    } else {
		DrawLabel(setPtr, tabPtr, drawable);
	    }
	}
	DrawOuterBorders(setPtr, drawable);
	if (redraw) {
	    EventuallyRedraw(setPtr);
	}
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
BindOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    if (argc == 2) {
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;
	char *tagName;

	for (hPtr = Blt_FirstHashEntry(&(setPtr->tagTable), &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    tagName = Blt_GetHashKey(&(setPtr->tagTable), hPtr);
	    Tcl_AppendElement(interp, tagName);
	}
	return TCL_OK;
    }
    return Blt_ConfigureBindings(interp, setPtr->bindTable,
	MakeTag(setPtr, argv[2]), argc - 3, argv + 3);
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
CgetOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    tabSet = setPtr;
    return Tk_ConfigureValue(interp, setPtr->tkwin, configSpecs,
	(char *)setPtr, argv[2], 0);
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
 *	etc. get set for setPtr; old resources get freed, if there
 *	were any.  The widget is redisplayed.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    tabSet = setPtr;
    if (argc == 2) {
	return Tk_ConfigureInfo(interp, setPtr->tkwin, configSpecs,
	    (char *)setPtr, (char *)NULL, 0);
    } else if (argc == 3) {
	return Tk_ConfigureInfo(interp, setPtr->tkwin, configSpecs,
	    (char *)setPtr, argv[2], 0);
    }
    if (ConfigureTabset(interp, setPtr, argc - 2, argv + 2,
	    TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    EventuallyRedraw(setPtr);
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
DeleteOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *firstPtr, *lastPtr;

    lastPtr = NULL;
    if (GetTabByIndex(setPtr, argv[2], &firstPtr, INVALID_FAIL) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((argc == 4) && 
	(GetTabByIndex(setPtr, argv[3], &lastPtr, INVALID_FAIL) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (lastPtr == NULL) {
	DestroyTab(setPtr, firstPtr);
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
	    DestroyTab(setPtr, tabPtr);
	    linkPtr = nextLinkPtr;
	    if (tabPtr == lastPtr) {
		break;
	    }
	}
    }
    setPtr->flags |= (TABSET_LAYOUT | TABSET_SCROLL);
    EventuallyRedraw(setPtr);
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
FocusOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr;

    if (GetTabByIndex(setPtr, argv[2], &tabPtr, INVALID_FAIL) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tabPtr != NULL) {
	setPtr->focusPtr = tabPtr;
	Blt_SetFocusItem(setPtr->bindTable, setPtr->focusPtr, NULL);
	EventuallyRedraw(setPtr);
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
IndexOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			
    char **argv;
{
    Tab *tabPtr;
    int search;

#define SEARCH_NAMES	1
#define SEARCH_INDICES	2
    search = SEARCH_INDICES;
    if (argc == 4) {
	if (strcmp(argv[2], "-index") == 0) {
	    search = SEARCH_INDICES;
	} else if (strcmp(argv[2], "-name") == 0) {
	    search = SEARCH_NAMES;
	} else {
	    Tcl_AppendResult(interp, "bad switch \"", argv[2], 
		     "\": should be \"-index\" or \"-name\"", (char *)NULL);
	    return TCL_ERROR;
	}
	argv++;
    }
    if (search == SEARCH_INDICES) {
	if (GetTabByIndex(setPtr, argv[2], &tabPtr, INVALID_OK) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	if (GetTabByName(setPtr, argv[2], &tabPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (tabPtr == NULL) {
	Tcl_SetResult(interp, "", TCL_STATIC);
    } else {
	Tcl_SetResult(interp, Blt_Itoa(TabIndex(setPtr, tabPtr)), 
		TCL_VOLATILE);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetOp --
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
GetOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr;

    if (GetTabByIndex(setPtr, argv[2], &tabPtr, INVALID_OK) != TCL_OK) {
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
InsertOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr;
    register int i;
    char **options;
    Blt_ChainLink *linkPtr, *beforeLinkPtr;
    int start, count;
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
	    beforeLinkPtr = Blt_ChainFirstLink(setPtr->chainPtr);
	} else if (position > Blt_ChainGetLength(setPtr->chainPtr)) {
	    beforeLinkPtr = NULL;
	} else {
	    beforeLinkPtr = Blt_ChainGetNthLink(setPtr->chainPtr, position);
	}
    } else {
	Tab *beforePtr;

	if (GetTabByIndex(setPtr, argv[2], &beforePtr, INVALID_FAIL) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	beforeLinkPtr = beforePtr->linkPtr;
    }
    tabSet = setPtr;
    setPtr->flags |= (TABSET_LAYOUT | TABSET_SCROLL);
    EventuallyRedraw(setPtr);
    for (i = 3; i < argc; /*empty*/ ) {
	if (TabExists(setPtr, argv[i])) {
	    Tcl_AppendResult(setPtr->interp, "tab \"", argv[i],
		"\" already exists in \"", Tk_PathName(setPtr->tkwin), "\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
	tabPtr = CreateTab(setPtr, argv[i]);
	if (tabPtr == NULL) {
	    return TCL_ERROR;
	}
	/*
	 * Count the option-value pairs that follow.  Count until we
	 * spot one that doesn't look like a configuration option (i.e.
	 * doesn't start with a minus "-").
	 */
	i++;
	start = i;
	for ( /*empty*/ ; i < argc; i += 2) {
	    if (argv[i][0] != '-') {
		break;
	    }
	}
	count = i - start;
	options = argv + start;
	if (Blt_ConfigureWidgetComponent(interp, setPtr->tkwin, tabPtr->name,
		"Tab", tabConfigSpecs, count, options, (char *)tabPtr, 0)
	    != TCL_OK) {
	    DestroyTab(setPtr, tabPtr);
	    return TCL_ERROR;
	}
	if (ConfigureTab(setPtr, tabPtr) != TCL_OK) {
	    DestroyTab(setPtr, tabPtr);
	    return TCL_ERROR;
	}
	linkPtr = Blt_ChainNewLink();
	if (beforeLinkPtr == NULL) {
	    Blt_ChainAppendLink(setPtr->chainPtr, linkPtr);
	} else {
	    Blt_ChainLinkBefore(setPtr->chainPtr, linkPtr, beforeLinkPtr);
	}
	tabPtr->linkPtr = linkPtr;
	Blt_ChainSetValue(linkPtr, tabPtr);
    }
    return TCL_OK;

}

/*
 * Preprocess the command string for percent substitution.
 */
static void
PercentSubst(setPtr, tabPtr, command, resultPtr)
    Tabset *setPtr;
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
		string = Tk_PathName(setPtr->tkwin);
		break;
	    case 'i':		/* Tab Index */
		string = Blt_Itoa(TabIndex(setPtr, tabPtr));
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
InvokeOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Tab *tabPtr;
    char *command;

    if (GetTabByIndex(setPtr, argv[2], &tabPtr, INVALID_FAIL) != TCL_OK) {
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

	PercentSubst(setPtr, tabPtr, command, &dString);
	result = Tcl_GlobalEval(setPtr->interp, Tcl_DStringValue(&dString));
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
MoveOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr, *linkPtr;
    int before;

    if (GetTabByIndex(setPtr, argv[2], &tabPtr, INVALID_FAIL) != TCL_OK) {
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
    if (GetTabByIndex(setPtr, argv[4], &linkPtr, INVALID_FAIL) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tabPtr == linkPtr) {
	return TCL_OK;
    }
    Blt_ChainUnlinkLink(setPtr->chainPtr, tabPtr->linkPtr);
    if (before) {
	Blt_ChainLinkBefore(setPtr->chainPtr, tabPtr->linkPtr, linkPtr->linkPtr);
    } else {
	Blt_ChainLinkAfter(setPtr->chainPtr, tabPtr->linkPtr, linkPtr->linkPtr);
    }
    setPtr->flags |= (TABSET_LAYOUT | TABSET_SCROLL);
    EventuallyRedraw(setPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
NearestOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    int x, y;			/* Screen coordinates of the test point. */
    Tab *tabPtr;

    if ((Tk_GetPixels(interp, setPtr->tkwin, argv[2], &x) != TCL_OK) ||
	(Tk_GetPixels(interp, setPtr->tkwin, argv[3], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (setPtr->nVisible > 0) {
	tabPtr = PickTab(setPtr, x, y, NULL);
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
SelectOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Tab *tabPtr;

    if (GetTabByIndex(setPtr, argv[2], &tabPtr, INVALID_OK) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((tabPtr == NULL) || (tabPtr->state == STATE_DISABLED)) {
	return TCL_OK;
    }
    if ((setPtr->selectPtr != NULL) && (setPtr->selectPtr != tabPtr) &&
	(setPtr->selectPtr->tkwin != NULL)) {
	if (setPtr->selectPtr->container == NULL) {
	    if (Tk_IsMapped(setPtr->selectPtr->tkwin)) {
		Tk_UnmapWindow(setPtr->selectPtr->tkwin);
	    }
	} else {
	    /* Redraw now unselected container. */
	    EventuallyRedrawTearoff(setPtr->selectPtr);
	}
    }
    setPtr->selectPtr = tabPtr;
    if ((setPtr->nTiers > 1) && (tabPtr->tier != setPtr->startPtr->tier)) {
	RenumberTiers(setPtr, tabPtr);
	Blt_PickCurrentItem(setPtr->bindTable);
    }
    setPtr->flags |= (TABSET_SCROLL);
    if (tabPtr->container != NULL) {
	EventuallyRedrawTearoff(tabPtr);
    }
    EventuallyRedraw(setPtr);
    return TCL_OK;
}

static int
ViewOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int width;

    width = VPORTWIDTH(setPtr);
    if (argc == 2) {
	double fract;

	/*
	 * Note: we are bounding the fractions between 0.0 and 1.0 to
	 * support the "canvas"-style of scrolling.
	 */

	fract = (double)setPtr->scrollOffset / setPtr->worldWidth;
	Tcl_AppendElement(interp, Blt_Dtoa(interp, CLAMP(fract, 0.0, 1.0)));
	fract = (double)(setPtr->scrollOffset + width) / setPtr->worldWidth;
	Tcl_AppendElement(interp, Blt_Dtoa(interp, CLAMP(fract, 0.0, 1.0)));
	return TCL_OK;
    }
    if (Blt_GetScrollInfo(interp, argc - 2, argv + 2, &(setPtr->scrollOffset),
	    setPtr->worldWidth, width, setPtr->scrollUnits, 
	    BLT_SCROLL_MODE_CANVAS) != TCL_OK) {
	return TCL_ERROR;
    }
    setPtr->flags |= TABSET_SCROLL;
    EventuallyRedraw(setPtr);
    return TCL_OK;
}


static void
AdoptWindow(clientData)
    ClientData clientData;
{
    Tab *tabPtr = clientData;
    int x, y;
    Tabset *setPtr = tabPtr->setPtr;

    x = setPtr->inset + setPtr->inset2 + tabPtr->padLeft;
#define TEAR_OFF_TAB_SIZE	5
    y = setPtr->inset + setPtr->inset2 + setPtr->yPad +
	setPtr->outerPad + TEAR_OFF_TAB_SIZE + tabPtr->padTop;
    Blt_RelinkWindow(tabPtr->tkwin, tabPtr->container, x, y);
    Tk_MapWindow(tabPtr->tkwin);
}

static void
DestroyTearoff(dataPtr)
    DestroyData dataPtr;
{
    Tab *tabPtr = (Tab *)dataPtr;

    if (tabPtr->container != NULL) {
	Tabset *setPtr;
	Tk_Window tkwin;
	setPtr = tabPtr->setPtr;

	tkwin = tabPtr->container;
	if (tabPtr->flags & TAB_REDRAW) {
	    Tcl_CancelIdleCall(DisplayTearoff, tabPtr);
	}
	Tk_DeleteEventHandler(tkwin, StructureNotifyMask, TearoffEventProc,
	    tabPtr);
	if (tabPtr->tkwin != NULL) {
	    XRectangle rect;

	    GetWindowRectangle(tabPtr, setPtr->tkwin, FALSE, &rect);
	    Blt_RelinkWindow(tabPtr->tkwin, setPtr->tkwin, rect.x, rect.y);
	    if (tabPtr == setPtr->selectPtr) {
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
CreateTearoff(setPtr, name, tabPtr)
    Tabset *setPtr;
    char *name;
    Tab *tabPtr;
{
    Tk_Window tkwin;
    int width, height;

    tkwin = Tk_CreateWindowFromPath(setPtr->interp, setPtr->tkwin, name,
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
    width += 2 * (setPtr->inset2 + setPtr->inset);
#define TEAR_OFF_TAB_SIZE	5
    height = Tk_Height(tabPtr->tkwin);
    if (height < 2) {
	height = (tabPtr->reqHeight > 0)
	    ? tabPtr->reqHeight : Tk_ReqHeight(tabPtr->tkwin);
    }
    height += PADDING(tabPtr->padY) +
	2 * Tk_Changes(tabPtr->tkwin)->border_width;
    height += setPtr->inset + setPtr->inset2 + setPtr->yPad +
	TEAR_OFF_TAB_SIZE + setPtr->outerPad;
    Tk_GeometryRequest(tkwin, width, height);
    Tk_UnmapWindow(tabPtr->tkwin);
    /* Tk_MoveWindow(tabPtr->tkwin, 0, 0); */
    Tcl_SetResult(setPtr->interp, Tk_PathName(tkwin), TCL_VOLATILE);
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
TabCgetOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr;

    if (GetTabByName(setPtr, argv[3], &tabPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    tabSet = setPtr;
    return Tk_ConfigureValue(interp, setPtr->tkwin, tabConfigSpecs,
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
TabConfigureOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
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
	if (GetTabByName(setPtr, argv[i], &tabPtr) != TCL_OK) {
	    return TCL_ERROR;	/* Can't find node. */
	}
    }
    nTabs = i;			/* Number of tab indices specified */
    nOpts = argc - i;		/* Number of options specified */
    options = argv + i;		/* Start of options in argv  */

    for (i = 0; i < nTabs; i++) {
	GetTabByName(setPtr, argv[i], &tabPtr);
	if (argc == 1) {
	    return Tk_ConfigureInfo(interp, setPtr->tkwin, tabConfigSpecs,
		(char *)tabPtr, (char *)NULL, 0);
	} else if (argc == 2) {
	    return Tk_ConfigureInfo(interp, setPtr->tkwin, tabConfigSpecs,
		(char *)tabPtr, argv[2], 0);
	}
	tabSet = setPtr;
	Tcl_Preserve(tabPtr);
	result = Tk_ConfigureWidget(interp, setPtr->tkwin, tabConfigSpecs,
	    nOpts, options, (char *)tabPtr, TK_CONFIG_ARGV_ONLY);
	if (result == TCL_OK) {
	    result = ConfigureTab(setPtr, tabPtr);
	}
	Tcl_Release(tabPtr);
	if (result == TCL_ERROR) {
	    return TCL_ERROR;
	}
	if (tabPtr->flags & TAB_VISIBLE) {
	    setPtr->flags |= (TABSET_LAYOUT | TABSET_SCROLL);
	    EventuallyRedraw(setPtr);
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
TabDockallOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;		/* Not used. */
{
    Tab *tabPtr;
    Blt_ChainLink *linkPtr;

    for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
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
 * TabPageHeight --
 *
 *	  .h tab pageheight
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TabPageHeight(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;		/* Not used. */
{
    Tcl_SetResult(interp, Blt_Itoa(VPORTHEIGHT(setPtr)), TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TabPageWidth --
 *
 *	  .h tab pagewidth
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TabPageWidth(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;		/* Not used. */
{
    Tcl_SetResult(interp, Blt_Itoa(VPORTWIDTH(setPtr)), TCL_VOLATILE);
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
TabNamesOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;		/* Not used. */
{
    Tab *tabPtr;
    Blt_ChainLink *linkPtr;

    if (argc == 3) {
	for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    Tcl_AppendElement(interp, tabPtr->name);
	}
    } else {
	register int i;

	for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
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
TabTearoffOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tab *tabPtr;
    int result;
    Tk_Window tkwin;

    if (GetTabByIndex(setPtr, argv[3], &tabPtr, INVALID_OK) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((tabPtr == NULL) || (tabPtr->tkwin == NULL) ||
	(tabPtr->state == STATE_DISABLED)) {
	return TCL_OK;		/* No-op */
    }
    if (argc == 4) {
	Tk_Window parent;

	parent = (tabPtr->container == NULL)
	    ? setPtr->tkwin : tabPtr->container;
	Tcl_SetResult(setPtr->interp, Tk_PathName(parent), TCL_VOLATILE);
	return TCL_OK;
    }
    Tcl_Preserve(tabPtr);
    result = TCL_OK;

    tkwin = Tk_NameToWindow(interp, argv[4], setPtr->tkwin);
    Tcl_ResetResult(interp);

    if (tabPtr->container != NULL) {
	Tcl_EventuallyFree(tabPtr, DestroyTearoff);
    }
    if ((tkwin != setPtr->tkwin) && (tabPtr->container == NULL)) {
	result = CreateTearoff(setPtr, argv[4], tabPtr);
    }
    Tcl_Release(tabPtr);
    EventuallyRedraw(setPtr);
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
    {"pageheight", 5, (Blt_Op)TabPageHeight, 3, 3, "", },
    {"pagewidth", 5, (Blt_Op)TabPageWidth, 3, 3, "", },
    {"tearoff", 1, (Blt_Op)TabTearoffOp, 4, 5, "index ?parent?",},
};

static int nTabOps = sizeof(tabOps) / sizeof(Blt_OpSpec);

static int
TabOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
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
    result = (*proc) (setPtr, interp, argc, argv);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * PerforationActivateOp --
 *
 * 	This procedure is called to activate (highlight) the
 * 	perforation.
 *
 *	  .h perforation activate boolean
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PerforationActivateOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    int bool;

    if (Tcl_GetBoolean(interp, argv[3], &bool) != TCL_OK) {
	return TCL_ERROR;
    }
    if (bool) {
	setPtr->flags |= PERFORATION_ACTIVE;
    } else {
	setPtr->flags &= ~PERFORATION_ACTIVE;
    }
    EventuallyRedraw(setPtr);
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
PerforationInvokeOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{

    if (setPtr->selectPtr != NULL) {
	char *cmd;
	
	cmd = GETATTR(setPtr->selectPtr, perfCommand);
	if (cmd != NULL) {
	    Tcl_DString dString;
	    int result;
	    
	    PercentSubst(setPtr, setPtr->selectPtr, cmd, &dString);
	    Tcl_Preserve(setPtr);
	    result = Tcl_GlobalEval(interp, Tcl_DStringValue(&dString));
	    Tcl_Release(setPtr);
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
PerforationOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
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
    result = (*proc) (setPtr, interp, argc, argv);
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
ScanOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
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
    if ((Tk_GetPixels(interp, setPtr->tkwin, argv[3], &x) != TCL_OK) ||
	(Tk_GetPixels(interp, setPtr->tkwin, argv[4], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (oper == SCAN_MARK) {
	if (setPtr->side & SIDE_VERTICAL) {
	    setPtr->scanAnchor = y;
	} else {
	    setPtr->scanAnchor = x;
	}
	setPtr->scanOffset = setPtr->scrollOffset;
    } else {
	int offset, delta;

	if (setPtr->side & SIDE_VERTICAL) {
	    delta = setPtr->scanAnchor - y;
	} else {
	    delta = setPtr->scanAnchor - x;
	}
	offset = setPtr->scanOffset + (10 * delta);
	offset = Blt_AdjustViewport(offset, setPtr->worldWidth,
	    VPORTWIDTH(setPtr), setPtr->scrollUnits, BLT_SCROLL_MODE_CANVAS);
	setPtr->scrollOffset = offset;
	setPtr->flags |= TABSET_SCROLL;
	EventuallyRedraw(setPtr);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
SeeOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Tab *tabPtr;

    if (GetTabByIndex(setPtr, argv[2], &tabPtr, INVALID_OK) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tabPtr != NULL) {
	int left, right, width;

	width = VPORTWIDTH(setPtr);
	left = setPtr->scrollOffset + setPtr->xSelectPad;
	right = setPtr->scrollOffset + width - setPtr->xSelectPad;

	/* If the tab is partially obscured, scroll so that it's
	 * entirely in view. */
	if (tabPtr->worldX < left) {
	    setPtr->scrollOffset = tabPtr->worldX - TAB_SCROLL_OFFSET;
	} else if ((tabPtr->worldX + tabPtr->worldWidth) >= right) {
	    Blt_ChainLink *linkPtr;

	    setPtr->scrollOffset = tabPtr->worldX + tabPtr->worldWidth -
		(width - 2 * setPtr->xSelectPad);
	    linkPtr = Blt_ChainNextLink(tabPtr->linkPtr); 
	    if (linkPtr != NULL) {
		Tab *nextPtr;

		nextPtr = Blt_ChainGetValue(linkPtr);
		if (nextPtr->tier == tabPtr->tier) {
		    setPtr->scrollOffset += TAB_SCROLL_OFFSET;
		}
	    }
	}
	setPtr->flags |= TABSET_SCROLL;
	EventuallyRedraw(setPtr);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
SizeOp(setPtr, interp, argc, argv)
    Tabset *setPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;		/* Not used. */
{
    Tcl_SetResult(interp, Blt_Itoa(Blt_ChainGetLength(setPtr->chainPtr)),
	TCL_VOLATILE);
    return TCL_OK;
}


static int
CountTabs(setPtr)
    Tabset *setPtr;
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
    for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
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

    setPtr->overlap = 0;

    /*
     * Pass 2:	Set the individual sizes of each tab.  This is different
     *		for constant and variable width tabs.  Add the extra space
     *		needed for slanted tabs, now that we know maximum tab
     *		height.
     */
    if (setPtr->defTabStyle.constWidth) {
	int slant;

	tabWidth = 2 * setPtr->inset2;
	tabHeight = setPtr->inset2 /* + 4 */;

	if (setPtr->side & SIDE_VERTICAL) {
	    tabWidth += labelHeight;
	    tabHeight += labelWidth;
	    slant = labelWidth;
	} else {
	    tabWidth += labelWidth;
	    tabHeight += labelHeight;
	    slant = labelHeight;
	}
	if (setPtr->slant & SLANT_LEFT) {
	    tabWidth += slant;
	    setPtr->overlap += tabHeight / 2;
	}
	if (setPtr->slant & SLANT_RIGHT) {
	    tabWidth += slant;
	    setPtr->overlap += tabHeight / 2;
	}
	for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    tabPtr->worldWidth = tabWidth;
	    tabPtr->worldHeight = tabHeight;
	}
    } else {
	int slant;

	tabWidth = tabHeight = 0;
	for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);

	    width = 2 * setPtr->inset2;
	    height = setPtr->inset2 /* + 4 */;
	    if (setPtr->side & SIDE_VERTICAL) {
		width += tabPtr->labelHeight;
		height += labelWidth;
		slant = labelWidth;
	    } else {
		width += tabPtr->labelWidth;
		height += labelHeight;
		slant = labelHeight;
	    }
	    width += (setPtr->slant & SLANT_LEFT) ? slant : setPtr->corner;
	    width += (setPtr->slant & SLANT_RIGHT) ? slant : setPtr->corner;

	    tabPtr->worldWidth = width; /* + 2 * (setPtr->corner + setPtr->xSelectPad) */ ;
	    tabPtr->worldHeight = height;

	    if (tabWidth < width) {
		tabWidth = width;
	    }
	    if (tabHeight < height) {
		tabHeight = height;
	    }
	}
	if (setPtr->slant & SLANT_LEFT) {
	    setPtr->overlap += tabHeight / 2;
	}
	if (setPtr->slant & SLANT_RIGHT) {
	    setPtr->overlap += tabHeight / 2;
	}
    }

    setPtr->tabWidth = tabWidth;
    setPtr->tabHeight = tabHeight;

    /*
     * Let the user override any page dimension.
     */
    setPtr->pageWidth = pageWidth;
    setPtr->pageHeight = pageHeight;
    if (setPtr->reqPageWidth > 0) {
	setPtr->pageWidth = setPtr->reqPageWidth;
    }
    if (setPtr->reqPageHeight > 0) {
	setPtr->pageHeight = setPtr->reqPageHeight;
    }
    return count;
}


static void
WidenTabs(setPtr, startPtr, nTabs, adjustment)
    Tabset *setPtr;
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
	x += tabPtr->worldWidth + setPtr->gap - setPtr->overlap;
	linkPtr = Blt_ChainNextLink(linkPtr);
    }
}


static void
AdjustTabSizes(setPtr, nTabs)
    Tabset *setPtr;
    int nTabs;
{
    int tabsPerTier;
    int total, count, extra;
    Tab *startPtr, *nextPtr;
    Blt_ChainLink *linkPtr;
    register Tab *tabPtr;
    int x, maxWidth;

    tabsPerTier = (nTabs + (setPtr->nTiers - 1)) / setPtr->nTiers;
    x = 0;
    maxWidth = 0;
    if (setPtr->defTabStyle.constWidth) {
	register int i;

	linkPtr = Blt_ChainFirstLink(setPtr->chainPtr);
	count = 1;
	while (linkPtr != NULL) {
	    for (i = 0; i < tabsPerTier; i++) {
		tabPtr = Blt_ChainGetValue(linkPtr);
		tabPtr->tier = count;
		tabPtr->worldX = x;
		x += tabPtr->worldWidth + setPtr->gap - setPtr->overlap;
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
    if (((nTabs % tabsPerTier) != 0) && (setPtr->defTabStyle.constWidth)) {
	return;
    }
    startPtr = NULL;
    count = total = 0;
    for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
	/*empty*/ ) {
	tabPtr = Blt_ChainGetValue(linkPtr);
	if (startPtr == NULL) {
	    startPtr = tabPtr;
	}
	count++;
	total += tabPtr->worldWidth + setPtr->gap - setPtr->overlap;
	linkPtr = Blt_ChainNextLink(linkPtr);
	if (linkPtr != NULL) {
	    nextPtr = Blt_ChainGetValue(linkPtr);
	    if (tabPtr->tier == nextPtr->tier) {
		continue;
	    }
	}
	total += setPtr->overlap;
	extra = setPtr->worldWidth - total;
	assert(count > 0);
	if (extra > 0) {
	    WidenTabs(setPtr, startPtr, count, extra);
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
ComputeLayout(setPtr)
    Tabset *setPtr;
{
    int width;
    Blt_ChainLink *linkPtr;
    Tab *tabPtr;
    int x, extra;
    int nTiers, nTabs;

    setPtr->nTiers = 0;
    setPtr->pageTop = 0;
    setPtr->worldWidth = 1;
    setPtr->yPad = 0;

    nTabs = CountTabs(setPtr);
    if (nTabs == 0) {
	return;
    }
    /* Reset the pointers to the selected and starting tab. */
    if (setPtr->selectPtr == NULL) {
	linkPtr = Blt_ChainFirstLink(setPtr->chainPtr);
	if (linkPtr != NULL) {
	    setPtr->selectPtr = Blt_ChainGetValue(linkPtr);
	}
    }
    if (setPtr->startPtr == NULL) {
	setPtr->startPtr = setPtr->selectPtr;
    }
    if (setPtr->focusPtr == NULL) {
	setPtr->focusPtr = setPtr->selectPtr;
	Blt_SetFocusItem(setPtr->bindTable, setPtr->focusPtr, NULL);
    }

    if (setPtr->side & SIDE_VERTICAL) {
        width = Tk_Height(setPtr->tkwin) - 
		2 * (setPtr->corner + setPtr->xSelectPad);
    } else {
        width = Tk_Width(setPtr->tkwin) - (2 * setPtr->inset) -
		setPtr->xSelectPad - setPtr->corner;
    }
    setPtr->flags |= TABSET_STATIC;
    if (setPtr->reqTiers > 1) {
	int total, maxWidth;

	/* Static multiple tier mode. */

	/* Sum tab widths and determine the number of tiers needed. */
	nTiers = 1;
	total = x = 0;
	for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    if ((x + tabPtr->worldWidth) > width) {
		nTiers++;
		x = 0;
	    }
	    tabPtr->worldX = x;
	    tabPtr->tier = nTiers;
	    extra = tabPtr->worldWidth + setPtr->gap - setPtr->overlap;
	    total += extra, x += extra;
	}
	maxWidth = width;

	if (nTiers > setPtr->reqTiers) {
	    /*
	     * The tabs do not fit into the requested number of tiers.
             * Go into scrolling mode.
	     */
	    width = ((total + setPtr->tabWidth) / setPtr->reqTiers);
	    x = 0;
	    nTiers = 1;
	    for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr);
		linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
		tabPtr = Blt_ChainGetValue(linkPtr);
		tabPtr->tier = nTiers;
		/*
		 * Keep adding tabs to a tier until we overfill it.
		 */
		tabPtr->worldX = x;
		x += tabPtr->worldWidth + setPtr->gap - setPtr->overlap;
		if (x > width) {
		    nTiers++;
		    if (x > maxWidth) {
			maxWidth = x;
		    }
		    x = 0;
		}
	    }
	    setPtr->flags &= ~TABSET_STATIC;
	}
	setPtr->worldWidth = maxWidth;
	setPtr->nTiers = nTiers;

	if (nTiers > 1) {
	    AdjustTabSizes(setPtr, nTabs);
	}
	if (setPtr->flags & TABSET_STATIC) {
	    setPtr->worldWidth = VPORTWIDTH(setPtr);
	} else {
	    /* Do you add an offset ? */
	    setPtr->worldWidth += (setPtr->xSelectPad + setPtr->corner);
	}
	setPtr->worldWidth += setPtr->overlap;
	if (setPtr->selectPtr != NULL) {
	    RenumberTiers(setPtr, setPtr->selectPtr);
	}
    } else {
	/*
	 * Scrollable single tier mode.
	 */
	nTiers = 1;
	x = 0;
	for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    tabPtr->tier = nTiers;
	    tabPtr->worldX = x;
	    tabPtr->worldY = 0;
	    x += tabPtr->worldWidth + setPtr->gap - setPtr->overlap;
	}
	setPtr->worldWidth = x + setPtr->corner - setPtr->gap +
	    setPtr->xSelectPad + setPtr->overlap;
	setPtr->flags &= ~TABSET_STATIC;
    }
    if (nTiers == 1) {
	setPtr->yPad = setPtr->ySelectPad;
    }
    setPtr->nTiers = nTiers;
    setPtr->pageTop = setPtr->inset + setPtr->yPad /* + 4 */ +
	(setPtr->nTiers * setPtr->tabHeight) + setPtr->inset2;

    if (setPtr->side & SIDE_VERTICAL) {
	for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    tabPtr->screenWidth = (short int)setPtr->tabHeight;
	    tabPtr->screenHeight = (short int)tabPtr->worldWidth;
	}
    } else {
	for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    tabPtr->screenWidth = (short int)tabPtr->worldWidth;
	    tabPtr->screenHeight = (short int)setPtr->tabHeight;
	}
    }
}

static void
ComputeVisibleTabs(setPtr)
    Tabset *setPtr;
{
    int nVisibleTabs;
    register Tab *tabPtr;
    Blt_ChainLink *linkPtr;

    setPtr->nVisible = 0;
    if (Blt_ChainGetLength(setPtr->chainPtr) == 0) {
	return;
    }
    nVisibleTabs = 0;
    if (setPtr->flags & TABSET_STATIC) {

	/* Static multiple tier mode. */

	for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
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
	offset = setPtr->scrollOffset - (setPtr->outerPad + setPtr->xSelectPad);
	width = VPORTWIDTH(setPtr) + setPtr->scrollOffset +
	    2 * setPtr->outerPad;
	for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
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
    for (linkPtr = Blt_ChainFirstLink(setPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	tabPtr = Blt_ChainGetValue(linkPtr);
	tabPtr->screenX = tabPtr->screenY = -1000;
	if (tabPtr->flags & TAB_VISIBLE) {
	    WorldToScreen(setPtr, tabPtr->worldX, tabPtr->worldY,
		&(tabPtr->screenX), &(tabPtr->screenY));
	    switch (setPtr->side) {
	    case SIDE_RIGHT:
		tabPtr->screenX -= setPtr->tabHeight;
		break;

	    case SIDE_BOTTOM:
		tabPtr->screenY -= setPtr->tabHeight;
		break;
	    }
	}
    }
    setPtr->nVisible = nVisibleTabs;
    Blt_PickCurrentItem(setPtr->bindTable);
}


static void
Draw3DFolder(setPtr, tabPtr, drawable, side, pointArr, nPoints)
    Tabset *setPtr;
    Tab *tabPtr;
    Drawable drawable;
    int side;
    XPoint pointArr[];
    int nPoints;
{
    GC gc;
    int relief, borderWidth;
    Tk_3DBorder border;

    if (tabPtr == setPtr->selectPtr) {
	border = GETATTR(tabPtr, selBorder);
    } else {
	border = tabPtr->border;
	if (border == NULL) {
	    border = setPtr->defTabStyle.border;
	}
    }
    relief = setPtr->defTabStyle.relief;
    if ((side == SIDE_RIGHT) || (side == SIDE_TOP)) {
	borderWidth = -setPtr->defTabStyle.borderWidth;
	if (relief == TK_RELIEF_SUNKEN) {
	    relief = TK_RELIEF_RAISED;
	} else if (relief == TK_RELIEF_RAISED) {
	    relief = TK_RELIEF_SUNKEN;
	}
    } else {
	borderWidth = setPtr->defTabStyle.borderWidth;
    }
    /* Draw the outline of the folder. */
    gc = Tk_GCForColor(setPtr->shadowColor, drawable);
    XDrawLines(setPtr->display, drawable, gc, pointArr, nPoints, CoordModeOrigin);
    /* And the folder itself. */
    if (tabPtr->tile != NULL) {
	Blt_TilePolygon(setPtr->tkwin, drawable, tabPtr->tile, pointArr,
	    nPoints);
	Tk_Draw3DPolygon(setPtr->tkwin, drawable, border, pointArr, nPoints,
	    borderWidth, relief);
    } else {
	Tk_Fill3DPolygon(setPtr->tkwin, drawable, border, pointArr, nPoints,
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
DrawLabel(setPtr, tabPtr, drawable)
    Tabset *setPtr;
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

    active = (setPtr->activePtr == tabPtr);
    selected = (setPtr->selectPtr == tabPtr);

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
     * screen coordinate space, and not according to the orientation
     * of the tabs themselves.  That's why we have to consider the
     * side when correcting for left/right slants.
     */
    switch (setPtr->side) {
    case SIDE_TOP:
    case SIDE_BOTTOM:
	if (setPtr->slant == SLANT_LEFT) {
	    x += setPtr->overlap;
	} else if (setPtr->slant == SLANT_RIGHT) {
	    x -= setPtr->overlap;
	}
	break;
    case SIDE_LEFT:
    case SIDE_RIGHT:
	if (setPtr->slant == SLANT_LEFT) {
	    y += setPtr->overlap;
	} else if (setPtr->slant == SLANT_RIGHT) {
	    y -= setPtr->overlap;
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
    XFillRectangle(setPtr->display, drawable, gc, x + dx, y + dy,
	tabPtr->labelWidth, tabPtr->labelHeight);
    if ((setPtr->flags & TABSET_FOCUS) && (setPtr->focusPtr == tabPtr)) {
	XDrawRectangle(setPtr->display, drawable, setPtr->defTabStyle.activeGC,
	    x + dx, y + dy, tabPtr->labelWidth - 1, tabPtr->labelHeight - 1);
    }
    tx = ty = ix = iy = 0;	/* Suppress compiler warning. */

    imgWidth = imgHeight = 0;
    if (tabPtr->image != NULL) {
	imgWidth = ImageWidth(tabPtr->image);
	imgHeight = ImageHeight(tabPtr->image);
    }
    switch (setPtr->defTabStyle.textSide) {
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
	    setPtr->defTabStyle.rotate, TK_ANCHOR_NW, TK_JUSTIFY_LEFT,
	    0, tabPtr->shadow.offset);
	ts.state = tabPtr->state;
	ts.border = border;
	ts.padX.side1 = ts.padX.side2 = 2;
	if ((selected) || (active)) {
	    ts.state |= STATE_ACTIVE;
	}
	Blt_DrawText(setPtr->tkwin, drawable, tabPtr->text, &ts, tx, ty);
    }
}


static void
DrawPerforation(setPtr, tabPtr, drawable)
    Tabset *setPtr;
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
    WorldToScreen(setPtr, tabPtr->worldX + 2, 
	  tabPtr->worldY + tabPtr->worldHeight + 2, &x, &y);
    border = GETATTR(tabPtr, selBorder);
    segmentWidth = 3;
    if (setPtr->flags & PERFORATION_ACTIVE) {
	perfBorder = GETATTR(tabPtr, activeBorder);
    } else {
	perfBorder = GETATTR(tabPtr, selBorder);
    }	
    if (setPtr->side & SIDE_HORIZONTAL) {
	pointArr[0].x = x;
	pointArr[0].y = pointArr[1].y = y;
	max = tabPtr->screenX + tabPtr->screenWidth - 2;
	Blt_Fill3DRectangle(setPtr->tkwin, drawable, perfBorder,
	       x - 2, y - 4, tabPtr->screenWidth, 8, 0, TK_RELIEF_FLAT);
	while (pointArr[0].x < max) {
	    pointArr[1].x = pointArr[0].x + segmentWidth;
	    if (pointArr[1].x > max) {
		pointArr[1].x = max;
	    }
	    Tk_Draw3DPolygon(setPtr->tkwin, drawable, border, pointArr, 2, 1,
			     TK_RELIEF_RAISED);
	    pointArr[0].x += 2 * segmentWidth;
	}
    } else {
	pointArr[0].x = pointArr[1].x = x;
	pointArr[0].y = y;
	max  = tabPtr->screenY + tabPtr->screenHeight - 2;
	Blt_Fill3DRectangle(setPtr->tkwin, drawable, perfBorder,
	       x - 4, y - 2, 8, tabPtr->screenHeight, 0, TK_RELIEF_FLAT);
	while (pointArr[0].y < max) {
	    pointArr[1].y = pointArr[0].y + segmentWidth;
	    if (pointArr[1].y > max) {
		pointArr[1].y = max;
	    }
	    Tk_Draw3DPolygon(setPtr->tkwin, drawable, border, pointArr, 2, 1,
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
	NextPoint((px) + setPtr->corner, (py)), \
	NextPoint((px), (py) - setPtr->corner)

#define TopLeft(px, py) \
	NextPoint((px), (py) + setPtr->corner), \
	NextPoint((px) + setPtr->corner, (py))

#define TopRight(px, py) \
	NextPoint((px) - setPtr->corner, (py)), \
	NextPoint((px), (py) + setPtr->corner)

#define BottomRight(px, py) \
	NextPoint((px), (py) - setPtr->corner), \
	NextPoint((px) - setPtr->corner, (py))


/*
 * From the left edge:
 *
 *   |a|b|c|d|e| f |d|e|g|h| i |h|g|e|d|f|    j    |e|d|c|b|a|
 *
 *	a. highlight ring
 *	b. tabset 3D border
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
DrawFolder(setPtr, tabPtr, drawable)
    Tabset *setPtr;
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

    width = VPORTWIDTH(setPtr);
    height = VPORTHEIGHT(setPtr);

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
    left = setPtr->scrollOffset - setPtr->xSelectPad;
    right = left + width;
    yTop = y + tabPtr->worldHeight;
    yBot = setPtr->pageTop - (setPtr->inset + setPtr->yPad);
    top = yBot - setPtr->inset2 /* - 4 */;

    if (setPtr->pageHeight == 0) {
	bottom = yBot + 2 * setPtr->corner;
    } else {
	bottom = height - setPtr->yPad - 1;
    }
    if (tabPtr != setPtr->selectPtr) {

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

	if (setPtr->slant & SLANT_LEFT) {
	    NextPoint(x, yBot);
	    NextPoint(x, yTop);
	    NextPoint(x + setPtr->tabHeight, y);
	} else {
	    BottomLeft(x, yBot);
	    TopLeft(x, y);
	}
	x += tabPtr->worldWidth;
	if (setPtr->slant & SLANT_RIGHT) {
	    NextPoint(x - setPtr->tabHeight, y);
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

	x -= setPtr->xSelectPad;
	y -= setPtr->yPad;
	tabWidth = tabPtr->worldWidth + 2 * setPtr->xSelectPad;

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
	     *   1+. . .2+         +7 . . . .+8
	     * 0+                              +9
	     *  .                              .
	     *  .                              .
	     *13+                              +10
	     *  12+-------------------------+11
	     */

	    if (x < (left + setPtr->corner)) {
		NextPoint(left, top);
	    } else {
		TopLeft(left, top);
	    }
	    if (setPtr->slant & SLANT_LEFT) {
		NextPoint(x, yTop);
		NextPoint(x + setPtr->tabHeight + setPtr->yPad, y);
	    } else {
		NextPoint(x, top);
		TopLeft(x, y);
	    }
	    x += tabWidth;
	    if (setPtr->slant & SLANT_RIGHT) {
		NextPoint(x - setPtr->tabHeight - setPtr->yPad, y);
		NextPoint(x, yTop);
	    } else {
		TopRight(x, y);
		NextPoint(x, top);
	    }
	    if (x > (right - setPtr->corner)) {
		NextPoint(right, top + setPtr->corner);
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
	     *          2+         +7 . . . .+8
	     *            1+ . . . +0          +9
	     *                     .           .
	     *                     .           .
	     *                   13+           +10
	     *                     12+--------+11
	     */

	    NextPoint(left, yBot);
	    if (setPtr->slant & SLANT_LEFT) {
		NextPoint(x, yBot);
		NextPoint(x, yTop);
		NextPoint(x + setPtr->tabHeight + setPtr->yPad, y);
	    } else {
		BottomLeft(x, yBot);
		TopLeft(x, y);
	    }

	    x += tabWidth;
	    if (setPtr->slant & SLANT_RIGHT) {
		NextPoint(x - setPtr->tabHeight - setPtr->yPad, y);
		NextPoint(x, yTop);
		NextPoint(x, top);
	    } else {
		TopRight(x, y);
		NextPoint(x, top);
	    }
	    if (x > (right - setPtr->corner)) {
		NextPoint(right, top + setPtr->corner);
	    } else {
		TopRight(right, top);
	    }
	    /* TopRight(right, top); */
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
	     *           6+ . . . .7+         +12
	     *         5+          0+ . . . +13
	     *          .           .
	     *          .           .
	     *         4+           +1
	     *           3+-------+2
	     */

	    NextPoint(right, yBot);
	    BottomRight(right, bottom);
	    BottomLeft(left, bottom);
	    /* TopLeft(left, top); */
	    if (x < (left + setPtr->corner)) {
		NextPoint(left, top);
	    } else {
		TopLeft(left, top);
	    }
	    NextPoint(x, top);

	    if (setPtr->slant & SLANT_LEFT) {
		NextPoint(x, yTop);
		NextPoint(x + setPtr->tabHeight + setPtr->yPad, y);
	    } else {
		TopLeft(x, y);
	    }
	    x += tabWidth;
	    if (setPtr->slant & SLANT_RIGHT) {
		NextPoint(x - setPtr->tabHeight - setPtr->yPad, y);
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
	     *     1+                     +7
	     *       2+ 0+          +9 .+8
	     *           .          .
	     *           .          .
	     *         13+          +10
	     *          12+-------+11
	     */

	    NextPoint(left, yBot);
	    if (setPtr->slant & SLANT_LEFT) {
		NextPoint(x, yBot);
		NextPoint(x, yTop);
		NextPoint(x + setPtr->tabHeight + setPtr->yPad, y);
	    } else {
		BottomLeft(x, yBot);
		TopLeft(x, y);
	    }
	    x += tabPtr->worldWidth;
	    if (setPtr->slant & SLANT_RIGHT) {
		NextPoint(x - setPtr->tabHeight - setPtr->yPad, y);
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
	WorldToScreen(setPtr, pointArr[i].x, pointArr[i].y, &x, &y);
	pointArr[i].x = x;
	pointArr[i].y = y;
    }
    Draw3DFolder(setPtr, tabPtr, drawable, setPtr->side, pointArr, nPoints);
    DrawLabel(setPtr, tabPtr, drawable);
    if (tabPtr->container != NULL) {
	XRectangle rect;

	/* Draw a rectangle covering the spot representing the window  */
	GetWindowRectangle(tabPtr, setPtr->tkwin, FALSE, &rect);
	XFillRectangles(setPtr->display, drawable, tabPtr->backGC,
	    &rect, 1);
    }
}

static void
DrawOuterBorders(setPtr, drawable)
    Tabset *setPtr;
    Drawable drawable;
{
    /*
     * Draw 3D border just inside of the focus highlight ring.  We
     * draw the border even if the relief is flat so that any tabs
     * that hang over the edge will be clipped.
     */
    if (setPtr->borderWidth > 0) {
	Blt_Draw3DRectangle(setPtr->tkwin, drawable, setPtr->border,
	    setPtr->highlightWidth, setPtr->highlightWidth,
	    Tk_Width(setPtr->tkwin) - 2 * setPtr->highlightWidth,
	    Tk_Height(setPtr->tkwin) - 2 * setPtr->highlightWidth,
	    setPtr->borderWidth, setPtr->relief);
    }
    /* Draw focus highlight ring. */
    if (setPtr->highlightWidth > 0) {
	XColor *color;
	GC gc;

	color = (setPtr->flags & TABSET_FOCUS)
	    ? setPtr->highlightColor : setPtr->highlightBgColor;
	gc = Tk_GCForColor(color, drawable);
	Tk_DrawFocusHighlight(setPtr->tkwin, gc, setPtr->highlightWidth, 
	      drawable);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DisplayTabset --
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
DisplayTabset(clientData)
    ClientData clientData;	/* Information about widget. */
{
    Tabset *setPtr = clientData;
    Pixmap drawable;
    int width, height;

    setPtr->flags &= ~TABSET_REDRAW;
    if (setPtr->tkwin == NULL) {
	return;			/* Window has been destroyed. */
    }
    if (setPtr->flags & TABSET_LAYOUT) {
	ComputeLayout(setPtr);
	setPtr->flags &= ~TABSET_LAYOUT;
    }
    if ((setPtr->reqHeight == 0) || (setPtr->reqWidth == 0)) {
	width = height = 0;
	if (setPtr->side & SIDE_VERTICAL) {
	    height = setPtr->worldWidth;
	} else {
	    width = setPtr->worldWidth;
	}
	if (setPtr->reqWidth > 0) {
	    width = setPtr->reqWidth;
	} else if (setPtr->pageWidth > 0) {
	    width = setPtr->pageWidth;
	}
	if (setPtr->reqHeight > 0) {
	    height = setPtr->reqHeight;
	} else if (setPtr->pageHeight > 0) {
	    height = setPtr->pageHeight;
	}
	if (setPtr->side & SIDE_VERTICAL) {
	    width += setPtr->pageTop + setPtr->inset + setPtr->inset2;
	    height += (setPtr->inset + setPtr->inset2);
	} else {
	    height += setPtr->pageTop + setPtr->inset + setPtr->inset2;
	    width += (setPtr->inset + setPtr->inset2);
	}
	if ((Tk_ReqWidth(setPtr->tkwin) != width) ||
	    (Tk_ReqHeight(setPtr->tkwin) != height)) {
	    Tk_GeometryRequest(setPtr->tkwin, width, height);
	}
    }
    if (setPtr->flags & TABSET_SCROLL) {
	width = VPORTWIDTH(setPtr);
	setPtr->scrollOffset = Blt_AdjustViewport(setPtr->scrollOffset,
	    setPtr->worldWidth, width, setPtr->scrollUnits,
	    BLT_SCROLL_MODE_CANVAS);
	if (setPtr->scrollCmdPrefix != NULL) {
	    Blt_UpdateScrollbar(setPtr->interp, setPtr->scrollCmdPrefix,
		(double)setPtr->scrollOffset / setPtr->worldWidth,
		(double)(setPtr->scrollOffset + width) / setPtr->worldWidth);
	}
	ComputeVisibleTabs(setPtr);
	setPtr->flags &= ~TABSET_SCROLL;
    }
    if (!Tk_IsMapped(setPtr->tkwin)) {
	return;
    }
    height = Tk_Height(setPtr->tkwin);
    drawable = Tk_GetPixmap(setPtr->display, Tk_WindowId(setPtr->tkwin),
	Tk_Width(setPtr->tkwin), Tk_Height(setPtr->tkwin),
	Tk_Depth(setPtr->tkwin));

    /*
     * Clear the background either by tiling a pixmap or filling with
     * a solid color. Tiling takes precedence.
     */
    if (setPtr->tile != NULL) {
	Blt_SetTileOrigin(setPtr->tkwin, setPtr->tile, 0, 0);
	Blt_TileRectangle(setPtr->tkwin, drawable, setPtr->tile, 0, 0,
	    Tk_Width(setPtr->tkwin), height);
    } else {
	Blt_Fill3DRectangle(setPtr->tkwin, drawable, setPtr->border, 0, 0,
	    Tk_Width(setPtr->tkwin), height, 0, TK_RELIEF_FLAT);
    }

    if (setPtr->nVisible > 0) {
	register int i;
	register Tab *tabPtr;
	Blt_ChainLink *linkPtr;

	linkPtr = setPtr->startPtr->linkPtr;
	for (i = 0; i < Blt_ChainGetLength(setPtr->chainPtr); i++) {
	    linkPtr = Blt_ChainPrevLink(linkPtr);
	    if (linkPtr == NULL) {
		linkPtr = Blt_ChainLastLink(setPtr->chainPtr);
	    }
	    tabPtr = Blt_ChainGetValue(linkPtr);
	    if ((tabPtr != setPtr->selectPtr) &&
		(tabPtr->flags & TAB_VISIBLE)) {
		DrawFolder(setPtr, tabPtr, drawable);
	    }
	}
	DrawFolder(setPtr, setPtr->selectPtr, drawable);
	if (setPtr->tearoff) {
	    DrawPerforation(setPtr, setPtr->selectPtr, drawable);
	}

	if ((setPtr->selectPtr->tkwin != NULL) &&
	    (setPtr->selectPtr->container == NULL)) {
	    XRectangle rect;

	    GetWindowRectangle(setPtr->selectPtr, setPtr->tkwin, FALSE, &rect);
	    ArrangeWindow(setPtr->selectPtr->tkwin, &rect, 0);
	}
    }
    DrawOuterBorders(setPtr, drawable);
    XCopyArea(setPtr->display, drawable, Tk_WindowId(setPtr->tkwin),
	setPtr->highlightGC, 0, 0, Tk_Width(setPtr->tkwin), height, 0, 0);
    Tk_FreePixmap(setPtr->display, drawable);
}

/*
 * From the left edge:
 *
 *   |a|b|c|d|e| f |d|e|g|h| i |h|g|e|d|f|    j    |e|d|c|b|a|
 *
 *	a. highlight ring
 *	b. tabset 3D border
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
    Tabset *setPtr;
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
    setPtr = tabPtr->setPtr;
    if (setPtr->tkwin == NULL) {
	return;
    }
    tkwin = tabPtr->container;
    drawable = Tk_WindowId(tkwin);

    /*
     * Clear the background either by tiling a pixmap or filling with
     * a solid color. Tiling takes precedence.
     */
    if (setPtr->tile != NULL) {
	Blt_SetTileOrigin(tkwin, setPtr->tile, 0, 0);
	Blt_TileRectangle(tkwin, drawable, setPtr->tile, 0, 0,
	    Tk_Width(tkwin), Tk_Height(tkwin));
    } else {
	Blt_Fill3DRectangle(tkwin, drawable, setPtr->border, 0, 0,
	    Tk_Width(tkwin), Tk_Height(tkwin), 0, TK_RELIEF_FLAT);
    }

    width = Tk_Width(tkwin) - 2 * setPtr->inset;
    height = Tk_Height(tkwin) - 2 * setPtr->inset;
    x = setPtr->inset + setPtr->gap + setPtr->corner;
    y = setPtr->inset;

    left = setPtr->inset;
    right = setPtr->inset + width;
    top = setPtr->inset + setPtr->corner + setPtr->xSelectPad;
    bottom = setPtr->inset + height;

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
    Draw3DFolder(setPtr, tabPtr, drawable, SIDE_TOP, pointArr, nPoints);

    parent = (tabPtr->container == NULL) ? setPtr->tkwin : tabPtr->container;
    GetWindowRectangle(tabPtr, parent, TRUE, &rect);
    ArrangeWindow(tabPtr->tkwin, &rect, TRUE);

    /* Draw 3D border. */
    if ((setPtr->borderWidth > 0) && (setPtr->relief != TK_RELIEF_FLAT)) {
	Blt_Draw3DRectangle(tkwin, drawable, setPtr->border, 0, 0,
	    Tk_Width(tkwin), Tk_Height(tkwin), setPtr->borderWidth,
	    setPtr->relief);
    }
}

/*
 * --------------------------------------------------------------
 *
 * TabsetCmd --
 *
 * 	This procedure is invoked to process the "tabset" command.
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
static Blt_OpSpec tabsetOps[] =
{
    {"activate", 1, (Blt_Op)ActivateOp, 3, 3, "index",},
    {"bind", 1, (Blt_Op)BindOp, 2, 5, "index ?sequence command?",},
    {"cget", 2, (Blt_Op)CgetOp, 3, 3, "option",},
    {"configure", 2, (Blt_Op)ConfigureOp, 2, 0, "?option value?...",},
    {"delete", 1, (Blt_Op)DeleteOp, 2, 0, "first ?last?",},
    {"focus", 1, (Blt_Op)FocusOp, 3, 3, "index",},
    {"get", 1, (Blt_Op)GetOp, 3, 3, "index",},
    {"highlight", 1, (Blt_Op)ActivateOp, 3, 3, "index",},
    {"index", 3, (Blt_Op)IndexOp, 3, 5, "string",},
    {"insert", 3, (Blt_Op)InsertOp, 3, 0,
	"index name ?name...? ?option value?",},
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

static int nTabsetOps = sizeof(tabsetOps) / sizeof(Blt_OpSpec);

static int
TabsetInstCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about the widget. */
    Tcl_Interp *interp;		/* Interpreter to report errors back to. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Vector of argument strings. */
{
    Blt_Op proc;
    Tabset *setPtr = clientData;
    int result;

    proc = Blt_GetOp(interp, nTabsetOps, tabsetOps, BLT_OP_ARG1, 
	argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve(setPtr);
    result = (*proc) (setPtr, interp, argc, argv);
    Tcl_Release(setPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TabsetInstDeletedCmd --
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
TabsetInstDeletedCmd(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Tabset *setPtr = clientData;

    if (setPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = setPtr->tkwin;
	setPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
#ifdef ITCL_NAMESPACES
	Itk_SetWidgetCommand(tkwin, (Tcl_Command) NULL);
#endif /* ITCL_NAMESPACES */
    }
}

/*
 * ------------------------------------------------------------------------
 *
 * TabsetCmd --
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
TabsetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tabset *setPtr;
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
    setPtr = CreateTabset(interp, tkwin);
    if (ConfigureTabset(interp, setPtr, argc - 2, argv + 2, 0) != TCL_OK) {
	Tk_DestroyWindow(setPtr->tkwin);
	return TCL_ERROR;
    }
    mask = (ExposureMask | StructureNotifyMask | FocusChangeMask);
    Tk_CreateEventHandler(tkwin, mask, TabsetEventProc, setPtr);
    setPtr->cmdToken = Tcl_CreateCommand(interp, argv[1], TabsetInstCmd,
	setPtr, TabsetInstDeletedCmd);
#ifdef ITCL_NAMESPACES
    Itk_SetWidgetCommand(setPtr->tkwin, setPtr->cmdToken);
#endif

    /*
     * Try to invoke a procedure to initialize various bindings on
     * tabs.  Source the file containing the procedure now if the
     * procedure isn't currently defined.  We deferred this to now so
     * that the user could set the variable "blt_library" within the
     * script.
     */
    if (!Tcl_GetCommandInfo(interp, "blt::TabsetInit", &cmdInfo)) {
	static char initCmd[] = "source [file join $blt_library tabset.tcl]";

	if (Tcl_GlobalEval(interp, initCmd) != TCL_OK) {
	    char info[200];

	    sprintf(info, "\n    (while loading bindings for %s)", argv[0]);
	    Tcl_AddErrorInfo(interp, info);
	    Tk_DestroyWindow(setPtr->tkwin);
	    return TCL_ERROR;
	}
    }
    if (Tcl_VarEval(interp, "blt::TabsetInit ", argv[1], (char *)NULL)
	!= TCL_OK) {
	Tk_DestroyWindow(setPtr->tkwin);
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, Tk_PathName(setPtr->tkwin), TCL_VOLATILE);
    return TCL_OK;
}

int
Blt_TabsetInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec =
    {
	"tabset", TabsetCmd,
    };

    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#endif /* NO_TABSET */
