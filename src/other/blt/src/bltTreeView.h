
/*
 * bltTreeView.h --
 *
 *	This module implements an hierarchy widget for the BLT toolkit.
 *
 * Copyright 1998-1999 Lucent Technologies, Inc.
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
 *	The "treeview" widget was created by George A. Howlett.
 */

/*
 * TODO:
 *
 * BUGS:
 *   1.  "open" operation should change scroll offset so that as many
 *	 new entries (up to half a screen) can be seen.
 *   2.  "open" needs to adjust the scrolloffset so that the same entry
 *	 is seen at the same place.
 */

#ifndef BLT_TREEVIEW_H
#define BLT_TREEVIEW_H

#include "bltImage.h"
#include "bltHash.h"
#include "bltChain.h"
#include "bltTree.h"
#include "bltTile.h"
#include "bltBind.h"
#include "bltObjConfig.h"

#define ITEM_ENTRY		(ClientData)0
#define ITEM_ENTRY_BUTTON	(ClientData)1
#define ITEM_COLUMN_TITLE	(ClientData)2
#define ITEM_COLUMN_RULE	(ClientData)3
#define ITEM_STYLE		(ClientData)0x10004

#if HAVE_UTF
#else
#define Tcl_NumUtfChars(s,n)	(((n) == -1) ? strlen((s)) : (n))
#define Tcl_UtfAtIndex(s,i)	((s) + (i))
#endif

#define ODD(x)			((x) | 0x01)

#define END			(-1)
#define SEPARATOR_LIST		((char *)NULL)
#define SEPARATOR_NONE		((char *)-1)

#define SEARCH_Y		1

typedef char *UID;

/*
 * The macro below is used to modify a "char" value (e.g. by casting
 * it to an unsigned character) so that it can be used safely with
 * macros such as isspace.
 */
#define UCHAR(c)	((unsigned char) (c))

#define TOGGLE(x, mask) (((x) & (mask)) ? ((x) & ~(mask)) : ((x) | (mask)))


#define SCREENX(h, wx)	((wx) - (h)->xOffset + (h)->inset)
#define SCREENY(h, wy)	((wy) - (h)->yOffset + (h)->inset + (h)->titleHeight)

#define WORLDX(h, sx)	((sx) - (h)->inset + (h)->xOffset)
#define WORLDY(h, sy)	((sy) - ((h)->inset + (h)->titleHeight) + (h)->yOffset)

#define VPORTWIDTH(h)	(Tk_Width((h)->tkwin) - 2 * (h)->inset)
#define VPORTHEIGHT(h) \
	(Tk_Height((h)->tkwin) - (h)->titleHeight - 2 * (h)->inset)

#define ICONWIDTH(d)	(tvPtr->levelInfo[(d)].iconWidth)
#define LEVELX(d)	(tvPtr->levelInfo[(d)].x)

#define DEPTH(h, n)	\
		(((h)->flatView) ? 0 : Blt_TreeNodeDepth((h)->tree, (n)))

#define SELECT_FG(t)	\
   (((((t)->flags & TV_FOCUS)) || ((t)->selOutFocusFgColor == NULL)) \
	? (t)->selInFocusFgColor : (t)->selOutFocusFgColor)
#define SELECT_BORDER(t)	\
   (((((t)->flags & TV_FOCUS)) || ((t)->selOutFocusBorder == NULL)) \
	? (t)->selInFocusBorder : (t)->selOutFocusBorder)

#define SELECT_MODE_SINGLE	(1<<0)
#define SELECT_MODE_MULTIPLE	(1<<1)

/*
 * ----------------------------------------------------------------------------
 *
 *  Internal treeview widget flags:
 *
 *	TV_LAYOUT	The layout of the hierarchy needs to be recomputed.
 *
 *	TV_REDRAW	A redraw request is pending for the widget.
 *
 *	TV_XSCROLL	X-scroll request is pending.
 *
 *	TV_YSCROLL	Y-scroll request is pending.
 *
 *	TV_SCROLL	Both X-scroll and  Y-scroll requests are pending.
 *
 *	TV_FOCUS	The widget is receiving keyboard events.
 *			Draw the focus highlight border around the widget.
 *
 *	TV_DIRTY	The hierarchy has changed. It may invalidate
 *			the locations and pointers to entries.  The widget 
 *			will need to recompute its layout.
 *
 *	TV_RESORT	The tree has changed such that the view needs to
 *			be resorted.  This can happen when an entry is
 *			open or closed, it's label changes, a column value
 *			changes, etc.
 *
 *	TV_BORDERS      The borders of the widget (highlight ring and
 *			3-D border) need to be redrawn.
 *
 *	TV_VIEWPORT     Indicates that the viewport has changed in some
 *			way: the size of the viewport, the location of 
 *			the viewport, or the contents of the viewport.
 *
 */

#define TV_LAYOUT	(1<<0)
#define TV_REDRAW	(1<<1)
#define TV_XSCROLL	(1<<2)
#define TV_YSCROLL	(1<<3)
#define TV_SCROLL	(TV_XSCROLL | TV_YSCROLL)
#define TV_FOCUS	(1<<4)
#define TV_DIRTY	(1<<5)
#define TV_UPDATE	(1<<6)
#define TV_RESORT	(1<<7)
#define TV_SORTED	(1<<8)
#define TV_SORT_PENDING (1<<9)
#define TV_BORDERS	(1<<10)
#define TV_VIEWPORT	(1<<11)

/*
 *  Rule related flags: Rules are XOR-ed lines. We need to track whether
 *			they have been drawn or not. 
 *
 *	TV_RULE_ACTIVE  Indicates that a rule is currently being drawn
 *			for a column.
 *			
 *
 *	TV_RULE_NEEDED  Indicates that a rule is needed (but not yet
 *			drawn) for a column.
 */

#define TV_RULE_ACTIVE	(1<<15)
#define TV_RULE_NEEDED	(1<<16)

/*
 *  Selection related flags:
 *
 *	TV_SELECT_EXPORT	Export the selection to X11.
 *
 *	TV_SELECT_PENDING	A "selection" command idle task is pending.
 *
 *	TV_SELECT_CLEAR		Clear selection flag of entry.
 *
 *	TV_SELECT_SET		Set selection flag of entry.
 *
 *	TV_SELECT_TOGGLE	Toggle selection flag of entry.
 *
 *	TV_SELECT_MASK		Mask of selection set/clear/toggle flags.
 *
 *	TV_SELECT_SORTED	Indicates if the entries in the selection 
 *				should be sorted or displayed in the order 
 *				they were selected.
 *
 */
#define TV_SELECT_CLEAR		(1<<16)
#define TV_SELECT_EXPORT	(1<<17) 
#define TV_SELECT_PENDING	(1<<18)
#define TV_SELECT_SET		(1<<19)
#define TV_SELECT_TOGGLE	(TV_SELECT_SET | TV_SELECT_CLEAR)
#define TV_SELECT_MASK		(TV_SELECT_SET | TV_SELECT_CLEAR)
#define TV_SELECT_SORTED	(1<<20)

/*
 *  Miscellaneous flags:
 *
 *	TV_ALLOW_DUPLICATES	When inserting new entries, create 
 *			        duplicate entries.
 *
 *	TV_FILL_ANCESTORS	Automatically create ancestor entries 
 *				as needed when inserting a new entry.
 *
 *	TV_HIDE_ROOT		Don't display the root entry.
 *
 *	TV_HIDE_LEAVES		Don't display entries that are leaves.
 *
 *	TV_SHOW_COLUMN_TITLES	Indicates whether to draw titles over each
 *				column.
 *
 */
#define TV_ALLOW_DUPLICATES	(1<<21)
#define TV_FILL_ANCESTORS	(1<<22)
#define TV_HIDE_ROOT		(1<<23) 
#define TV_HIDE_LEAVES		(1<<24) 
#define TV_SHOW_COLUMN_TITLES	(1<<25)
#define TV_SORT_AUTO		(1<<26)
#define TV_NEW_TAGS		(1<<27)
#define TV_HIGHLIGHT_CELLS	(1<<28)

#define TV_ITEM_COLUMN	1
#define TV_ITEM_RULE	2

/*
 * -------------------------------------------------------------------------
 *
 *  Internal entry flags:
 *
 *	ENTRY_HAS_BUTTON	Indicates that a button needs to be
 *				drawn for this entry.
 *
 *	ENTRY_CLOSED		Indicates that the entry is closed and
 *				its subentries are not displayed.
 *
 *	ENTRY_HIDDEN		Indicates that the entry is hidden (i.e.
 *				can not be viewed by opening or scrolling).
 *
 *	BUTTON_AUTO
 *	BUTTON_SHOW
 *	BUTTON_MASK
 *
 * -------------------------------------------------------------------------
 */
#define ENTRY_CLOSED		(1<<0)
#define ENTRY_HIDDEN		(1<<1)
#define ENTRY_NOT_LEAF		(1<<2)
#define ENTRY_MASK		(ENTRY_CLOSED | ENTRY_HIDDEN)

#define ENTRY_HAS_BUTTON	(1<<3)
#define ENTRY_ICON		(1<<4)
#define ENTRY_REDRAW		(1<<5)
#define ENTRY_LAYOUT_PENDING	(1<<6)
#define ENTRY_DATA_CHANGED	(1<<7)
#define ENTRY_DIRTY		(ENTRY_DATA_CHANGED | ENTRY_LAYOUT_PENDING)

#define BUTTON_AUTO		(1<<8)
#define BUTTON_SHOW		(1<<9)
#define BUTTON_MASK		(BUTTON_AUTO | BUTTON_SHOW)

#define COLUMN_RULE_PICKED	(1<<1)
#define COLUMN_DIRTY		(1<<2)

#define STYLE_TEXTBOX		(0)
#define STYLE_COMBOBOX		(1)
#define STYLE_CHECKBOX		(2)
#define STYLE_TYPE		0x3

#define STYLE_LAYOUT		(1<<3)
#define STYLE_DIRTY		(1<<4)
#define STYLE_HIGHLIGHT		(1<<5)
#define STYLE_USER		(1<<6)

typedef struct TreeViewColumnStruct TreeViewColumn;
typedef struct TreeViewComboboxStruct TreeViewCombobox;
typedef struct TreeViewEntryStruct TreeViewEntry;
typedef struct TreeViewStruct TreeView;
typedef struct TreeViewStyleClassStruct TreeViewStyleClass;
typedef struct TreeViewStyleStruct TreeViewStyle;

typedef int (TreeViewCompareProc) _ANSI_ARGS_((Tcl_Interp *interp, char *name,
	char *pattern));

typedef TreeViewEntry *(TreeViewIterProc) _ANSI_ARGS_((TreeViewEntry *entryPtr,
	unsigned int mask));

typedef struct {
    int tagType;
    TreeView *tvPtr;
    Blt_HashSearch cursor;
    TreeViewEntry *entryPtr;
} TreeViewTagInfo;

/*
 * TreeViewIcon --
 *
 *	Since instances of the same Tk image can be displayed in
 *	different windows with possibly different color palettes, Tk
 *	internally stores each instance in a linked list.  But if
 *	the instances are used in the same widget and therefore use
 *	the same color palette, this adds a lot of overhead,
 *	especially when deleting instances from the linked list.
 *
 *	For the treeview widget, we never need more than a single
 *	instance of an image, regardless of how many times it's used.
 *	Cache the image, maintaining a reference count for each
 *	image used in the widget.  It's likely that the treeview
 *	widget will use many instances of the same image (for example
 *	the open/close icons).
 */

typedef struct TreeViewIconStruct {
    Tk_Image tkImage;		/* The Tk image being cached. */

    int refCount;		/* Reference count for this image. */

    short int width, height;	/* Dimensions of the cached image. */

    Blt_HashEntry *hashPtr;	/* Hash table pointer to the image. */

} *TreeViewIcon;

#define TreeViewIconHeight(icon)	((icon)->height)
#define TreeViewIconWidth(icon)	((icon)->width)
#define TreeViewIconBits(icon)	((icon)->tkImage)

/*
 * TreeViewColumn --
 *
 *	A column describes how to display a field of data in the tree.
 *	It may display a title that you can bind to. It may display a
 *	rule for resizing the column.  Columns may be hidden, and have
 *	attributes (foreground color, background color, font, etc)
 *	that override those designated globally for the treeview
 *	widget.
 */
struct TreeViewColumnStruct {
    int type;			/* Always TV_COLUMN */
    Blt_TreeKey key;		/* Data cell identifier for current tree. */
    int position;		/* Position of column in list.  Used
				 * to indicate the first and last
				 * columns. */
    UID tagsUid;		/* List of binding tags for this
				 * entry.  UID, not a string, because
				 * in the typical case most columns
				 * will have the same bindtags. */

    TreeView *tvPtr;
    unsigned int flags;

    /* Title-related information */
    char *title;		/* Text displayed in column heading as its
				 * title. By default, this is the same as 
				 * the data cell name. */
    Tk_Font titleFont;		/* Font to draw title in. */
    Shadow titleShadow;

    XColor *titleFgColor;	/* Foreground color of text displayed in 
				 * the heading */
    Tk_3DBorder titleBorder;	/* Background color of the column's heading. */

    GC titleGC;

    XColor *activeTitleFgColor;	/* Foreground color of text heading when 
				 * the column is activated.*/
    Tk_3DBorder activeTitleBorder;	

    int titleBorderWidth;
    int titleRelief;

    GC activeTitleGC;

    TextLayout *titleTextPtr;
    short int titleWidth, titleHeight;

    TreeViewIcon titleIcon;	/* Icon displayed in column heading */
    char *titleCmd;		/* Tcl script to be executed by the 
				 * column's "invoke" operation. */

    char *sortCmd;		/* Tcl script used to compare two
				 * columns. */

    /* General information. */
    int hidden;			/* Indicates if the column is
				 * displayed */
    int state;			/* Indicates if column title can
				 * invoked. */
    int editable;		/* Indicates if column can be
				 * edited. */

    int max;			/* Maximum space allowed for column. */
    int reqMin, reqMax;		/* Requested bounds on the width of
				 * column.  Does not include any
				 * padding or the borderwidth of
				 * column.  If non-zero, overrides the
				 * computed width of the column. */

    int reqWidth;		/* User-requested width of
				 * column. Does not include any
				 * padding or the borderwidth of
				 * column.  If non-zero, overrides the
				 * computed column width. */

    int maxWidth;		/* Width of the widest entry in the
				 * column. */

    int worldX;			/* Starting world x-coordinate of the
				 * column. */

    double weight;		/* Growth factor for column.  Zero
				 * indicates that the column can not
				 * be resized. */

    int width;			/* Computed width of column. */

    TreeViewStyle *stylePtr;	/* Default style for column. */

    Tk_3DBorder border;		/* Background color of column. */
    int borderWidth;		/* Border width of the column. */
    int relief;			/* Relief of the column. */
    Blt_Pad pad;		/* Horizontal padding on either side
				 * of the column. */

    Tk_Justify justify;		/* Indicates how the text or icon is
				 * justified within the column. */

    Blt_ChainLink *linkPtr;
    
    int ruleLineWidth;
    Blt_Dashes ruleDashes;
    GC ruleGC;
};


struct TreeViewStyleStruct {
    int refCount;		/* Usage reference count.  A reference 
				 * count of zero indicates that the 
				 * style may be freed. */
    unsigned int flags;		/* Bit field containing both the style
				 * type and various flags. */
    char *name;			/* Instance name. */
    TreeViewStyleClass *classPtr; 
				/* Contains class-specific information such
				 * as configuration specifications and 
				 * configure, draw, etc. routines. */
    Blt_HashEntry *hashPtr;	/* If non-NULL, points to the hash
				 * table entry for the style.  A style
				 * that's been deleted, but still in
				 * use (non-zero reference count) will
				 * have no hash table entry.
				 */
    /* General style fields. */
    Tk_Cursor cursor;		/* X Cursor */

    TreeViewIcon icon;		/* If non-NULL, is a Tk_Image to be drawn
				 * in the cell. */
    int gap;			/* # pixels gap between icon and text. */
    Tk_Font font;
    XColor *fgColor;		/* Normal foreground color of cell. */
    Tk_3DBorder border;		/* Normal background color of cell. */
    XColor *highlightFgColor;	/* Foreground color of cell when
				 * highlighted. */
    Tk_3DBorder highlightBorder;/* Background color of cell when
				 * highlighted. */
    XColor *activeFgColor;	/* Foreground color of cell when active. */
    Tk_3DBorder activeBorder;	/* Background color of cell when active. */

};

typedef struct TreeViewValueStruct {
    TreeViewColumn *columnPtr;	/* Column in which the value is located. */
    short int width, height;	/* Dimensions of value. */
    TreeViewStyle *stylePtr;	/* Style information for cell
				 * displaying value. */
    char *string;		/* Raw text string. */
    TextLayout *textPtr;	/* Processes string to be displayed .*/
    struct TreeViewValueStruct *nextPtr;
} TreeViewValue;
    
typedef void (StyleConfigProc) _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewStyle *stylePtr));
typedef void (StyleDrawProc) _ANSI_ARGS_((TreeView *tvPtr, Drawable drawable, 
	TreeViewEntry *entryPtr, TreeViewValue *valuePtr, 
	TreeViewStyle *stylePtr, int x, int y));
typedef int (StyleEditProc) _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr, TreeViewValue *valuePtr, 
	TreeViewStyle *stylePtr));
typedef void (StyleFreeProc) _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewStyle *stylePtr));
typedef void (StyleMeasureProc) _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewStyle *stylePtr, TreeViewValue *valuePtr));
typedef int (StylePickProc) _ANSI_ARGS_((TreeViewEntry *entryPtr, 
	TreeViewValue *valuePtr, TreeViewStyle *stylePtr, int worldX, 
	int worldY));

struct TreeViewStyleClassStruct {
    char *className;		/* Class name of the style */
    Blt_ConfigSpec *specsPtr;	/* Style configuration specifications */
    StyleConfigProc *configProc;/* Sets the GCs for style. */
    StyleMeasureProc *measProc;	/* Measures the area needed for the value
				 * with this style. */
    StyleDrawProc *drawProc;	/* Draw the value in it's style. */
    StylePickProc *pickProc;	/* Routine to pick the style's button. 
				 * Indicates if the mouse pointer is over
				 * the style's button (if it has one). */
    StyleEditProc *editProc;	/* Routine to edit the style's value. */
    StyleFreeProc *freeProc;	/* Routine to free the style's resources. */
};

/*
 * TreeViewEntry --
 *
 *	Contains data-specific information how to represent the data
 *	of a node of the hierarchy.
 *
 */
struct TreeViewEntryStruct {
    Blt_TreeNode node;		/* Node containing entry */
    int worldX, worldY;		/* X-Y position in world coordinates
				 * where the entry is positioned. */

    short int width, height;	/* Dimensions of the entry. This
				 * includes the size of its
				 * columns. */

    int reqHeight;		/* Requested height of the entry. 
				 * Overrides computed height. */

    int vertLineLength;		/* Length of the vertical line
				 * segment. */

    int lineHeight;		/* Height of first line of text. */
    unsigned int flags;		/* Flags for this entry. For the
				 * definitions of the various bit
				 * fields see below. */

    UID tagsUid;		/* List of binding tags for this
				 * entry.  UID, not a string, because
				 * in the typical case most entries
				 * will have the same bindtags. */
    TreeView *tvPtr;

    UID openCmd, closeCmd;	/* Tcl commands to invoke when entries
				 * are opened or closed. They override
				 * those specified globally. */
    /*
     * Button information:
     */
    short int buttonX, buttonY; /* X-Y coordinate offsets from to
				 * upper left corner of the entry to
				 * the upper-left corner of the
				 * button.  Used to pick the
				 * button quickly */

    TreeViewIcon *icons;	/* Tk images displayed for the entry.
				 * The first image is the icon
				 * displayed to the left of the
				 * entry's label. The second is icon
				 * displayed when entry is "open". */

    TreeViewIcon *activeIcons;	/* Tk images displayed for the entry.
				 * The first image is the icon
				 * displayed to the left of the
				 * entry's label. The second is icon
				 * displayed when entry is "open". */

    short int iconWidth;
    short int iconHeight;	/* Maximum dimensions for icons and
				 * buttons for this entry. This is
				 * used to align the button, icon, and
				 * text. */
    /*
     * Label information:
     */
    TextLayout *textPtr;

    short int labelWidth;
    short int labelHeight;

    UID labelUid;		/* Text displayed right of the icon. */

    Tk_Font font;		/* Font of label. Overrides global
				 * font specification. */
    char *fullName;

    int flatIndex;

    Tcl_Obj *dataObjPtr;	/* pre-fetched data for sorting */

    XColor *color;		/* Color of label. Overrides default
				 * text color specification. */
    GC gc;

    Shadow shadow;

    TreeViewValue *values;	/* List of column-related information
				 * for each data value in the node.
				 * Non-NULL only if there are value
				 * entries. */
};

/*
 * TreeViewButton --
 *
 *	A button is the open/close indicator at the far left of the
 *	entry.  It is displayed as a plus or minus in a solid
 *	colored box with optionally an border. It has both "active"
 *	and "normal" colors.
 */
typedef struct {
    XColor *fgColor;		/* Foreground color. */

    Tk_3DBorder border;		/* Background color. */

    XColor *activeFgColor;	/* Active foreground color. */

    Tk_3DBorder activeBorder;	/* Active background color. */

    GC normalGC;
    GC activeGC;

    int reqSize;

    int borderWidth;

    int openRelief, closeRelief;

    int width, height;

    TreeViewIcon *icons;

} TreeViewButton;

/*
 * LevelInfo --
 *
 */
typedef struct {
    int x;
    int iconWidth;
    int labelWidth;
} LevelInfo;

/*
 * TreeView --
 *
 *	A TreeView is a widget that displays an hierarchical table 
 *	of one or more entries.
 *
 *	Entries are positioned in "world" coordinates, referring to
 *	the virtual treeview.  Coordinate 0,0 is the upper-left corner
 *	of the root entry and the bottom is the end of the last entry.
 *	The widget's Tk window acts as view port into this virtual
 *	space. The treeview's xOffset and yOffset fields specify the
 *	location of the view port in the virtual world.  Scrolling the
 *	viewport is therefore simply changing the xOffset and/or
 *	yOffset fields and redrawing.
 *
 *	Note that world coordinates are integers, not signed short
 *	integers like X11 screen coordinates.  It's very easy to
 *	create a hierarchy taller than 0x7FFF pixels.
 */
struct TreeViewStruct {
    Tcl_Interp *interp;

    Tcl_Command cmdToken;	/* Token for widget's Tcl command. */

    Blt_Tree tree;		/* Token holding internal tree. */

    Blt_HashEntry *hashPtr;

    /* TreeView specific fields. */ 

    Tk_Window tkwin;		/* Window that embodies the widget.
                                 * NULL means that the window has been
                                 * destroyed but the data structures
                                 * haven't yet been cleaned up.*/

    Display *display;		/* Display containing widget; needed,
                                 * among other things, to release
                                 * resources after tkwin has already
                                 * gone away. */

    Blt_HashTable entryTable;	/* Table of entry information, keyed by
				 * the node pointer. */

    Blt_HashTable columnTable;	/* Table of column information. */
    Blt_Chain *colChainPtr;	/* Chain of columns. Same as the hash
				 * table above but maintains the order
				 * in which columns are displayed. */

    unsigned int flags;		/* For bitfield definitions, see below */

    int inset;			/* Total width of all borders,
				 * including traversal highlight and
				 * 3-D border.  Indicates how much
				 * interior stuff must be offset from
				 * outside edges to leave room for
				 * borders. */

    Tk_Font font;
    XColor *fgColor;

    Tk_3DBorder border;		/* 3D border surrounding the window
				 * (viewport). */

    int borderWidth;		/* Width of 3D border. */

    int relief;			/* 3D border relief. */


    int highlightWidth;		/* Width in pixels of highlight to
				 * draw around widget when it has the
				 * focus.  <= 0 means don't draw a
				 * highlight. */

    XColor *highlightBgColor;	/* Color for drawing traversal
				 * highlight area when highlight is
				 * off. */

    XColor *highlightColor;	/* Color for drawing traversal highlight. */

    char *pathSep;		/* Pathname separators */

    char *trimLeft;		/* Leading characters to trim from
				 * pathnames */

    /*
     * Entries are connected by horizontal and vertical lines. They
     * may be drawn dashed or solid.
     */
    int lineWidth;		/* Width of lines connecting entries */

    int dashes;			/* Dash on-off value. */

    XColor *lineColor;		/* Color of connecting lines. */

    /*
     * Button Information:
     *
     * The button is the open/close indicator at the far left of the
     * entry.  It is usually displayed as a plus or minus in a solid
     * colored box with optionally an border. It has both "active" and
     * "normal" colors.
     */
    TreeViewButton button;

    /*
     * Selection Information:
     *
     * The selection is the rectangle that contains a selected entry.
     * There may be many selected entries.  It is displayed as a solid
     * colored box with optionally a 3D border.
     */
    int selRelief;		/* Relief of selected items. Currently
				 * is always raised. */

    int selBorderWidth;		/* Border width of a selected entry.*/

    XColor *selInFocusFgColor;	/* Text color of a selected entry. */
    XColor *selOutFocusFgColor;

    Tk_3DBorder selInFocusBorder;
    Tk_3DBorder selOutFocusBorder;


    TreeViewEntry *selAnchorPtr; /* Fixed end of selection (i.e. entry
				  * at which selection was started.) */
    TreeViewEntry *selMarkPtr;
    
    int	selectMode;		/* Selection style: "single" or
				 * "multiple".  */

    char *selectCmd;		/* Tcl script that's invoked whenever
				 * the selection changes. */

    Blt_HashTable selectTable;	/* Hash table of currently selected
				 * entries. */

    Blt_Chain *selChainPtr;	/* Chain of currently selected
				 * entries.  Contains the same
				 * information as the above hash
				 * table, but maintains the order in
				 * which entries are selected.
				 */

    int leader;			/* Number of pixels padding between
				 * entries. */

    Tk_Cursor cursor;		/* X Cursor */

    Tk_Cursor resizeCursor;	/* Resize Cursor */

    int reqWidth, reqHeight;	/* Requested dimensions of the
				 * treeview widget's window. */

    GC lineGC;			/* GC for drawing dotted line between
				 * entries. */

    XColor *focusColor;

    Blt_Dashes focusDashes;	/* Dash on-off value. */

    GC focusGC;			/* Graphics context for the active
				 * label. */

    Tk_Window comboWin;		

    TreeViewEntry *activePtr;	/* Last active entry. */ 

    TreeViewEntry *focusPtr;	/* Entry that currently has focus. */

    TreeViewEntry *activeButtonPtr; /* Pointer to last active button */

    TreeViewEntry *fromPtr;

    TreeViewValue *activeValuePtr;/* Last active value. */ 

    int xScrollUnits, yScrollUnits; /* # of pixels per scroll unit. */

    /* Command strings to control horizontal and vertical
     * scrollbars. */
    char *xScrollCmdPrefix, *yScrollCmdPrefix;

    int scrollMode;		/* Selects mode of scrolling: either
				 * BLT_SCROLL_MODE_HIERBOX, 
				 * BLT_SCROLL_MODE_LISTBOX, 
				 * or BLT_SCROLL_MODE_CANVAS. */
    /*
     * Total size of all "open" entries. This represents the range of
     * world coordinates.
     */
    int worldWidth, worldHeight;

    int xOffset, yOffset;	/* Translation between view port and
				 * world origin. */

    short int minHeight;	/* Minimum entry height. Used to to
				 * compute what the y-scroll unit
				 * should be. */
    short int titleHeight;	/* Height of column titles. */

    LevelInfo *levelInfo;

    /*
     * Scanning information:
     */
    int scanAnchorX, scanAnchorY;
    /* Scan anchor in screen coordinates. */
    int scanX, scanY;		/* X-Y world coordinate where the scan
				 * started. */

    Blt_HashTable iconTable;	/* Table of Tk images */

    Blt_HashTable uidTable;	/* Table of strings. */

    Blt_HashTable styleTable;	/* Table of cell styles. */

    TreeViewEntry *rootPtr;	/* Root entry of tree. */

    TreeViewEntry **visibleArr;	/* Array of visible entries */

    int nVisible;		/* Number of entries in the above array */

    int nEntries;		/* Number of entries in tree. */
    int treeWidth;		/* Computed width of the tree. */

    int buttonFlags;		/* Global button indicator for all
				 * entries.  This may be overridden by
				 * the entry's -button option. */

    char *openCmd, *closeCmd;	/* Tcl commands to invoke when entries
				 * are opened or closed. */

    TreeViewIcon *icons;	/* Tk images displayed for the entry.
				 * The first image is the icon
				 * displayed to the left of the
				 * entry's label. The second is icon
				 * displayed when entry is "open". */
    TreeViewIcon *activeIcons;	/* Tk images displayed for the entry.
				 * The first image is the icon
				 * displayed to the left of the
				 * entry's label. The second is icon
				 * displayed when entry is "open". */
    char *takeFocus;

    ClientData clientData;

    Blt_BindTable bindTable;	/* Binding information for entries. */

    Blt_HashTable entryTagTable;
    Blt_HashTable buttonTagTable;
    Blt_HashTable columnTagTable;
    Blt_HashTable styleTagTable;

    TreeViewStyle *stylePtr;	/* Default style for text cells */

    TreeViewColumn treeColumn;
    
    TreeViewColumn *activeColumnPtr; 
    TreeViewColumn *activeTitleColumnPtr; 
				/* Column title currently active. */

    TreeViewColumn *resizeColumnPtr; 
				/* Column that is being resized. */

    int depth;

    int flatView;		/* Indicates if the view of the tree
				 * has been flattened. */

    TreeViewEntry **flatArr;	/* Flattened array of entries. */

    char *sortField;		/* Field to be sorted. */

    int sortType;		/* Type of sorting to be performed. See
				 * below for valid values. */

    char *sortCmd;		/* Sort command. */

    int sortDecreasing;		/* Indicates entries should be sorted
				 * in decreasing order. */

    int viewIsDecreasing;	/* Current sorting direction */

    TreeViewColumn *sortColumnPtr;/* Column to use for sorting criteria. */

#ifdef notdef
    Pixmap drawable;		/* Pixmap used to cache the entries
				 * displayed.  The pixmap is saved so
				 * that only selected elements can be
				 * drawn quicky. */

    short int drawWidth, drawHeight;
#endif
    short int ruleAnchor, ruleMark;

    Blt_Pool entryPool;
    Blt_Pool valuePool;
};


extern UID Blt_TreeViewGetUid _ANSI_ARGS_((TreeView *tvPtr, 
	CONST char *string));
extern void Blt_TreeViewFreeUid _ANSI_ARGS_((TreeView *tvPtr, UID uid));

extern void Blt_TreeViewEventuallyRedraw _ANSI_ARGS_((TreeView *tvPtr));
extern Tcl_ObjCmdProc Blt_TreeViewWidgetInstCmd;
extern TreeViewEntry *Blt_TreeViewNearestEntry _ANSI_ARGS_((TreeView *tvPtr,
	int x, int y, int flags));
extern char *Blt_TreeViewGetFullName _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr, int checkEntryLabel, Tcl_DString *dsPtr));
extern void Blt_TreeViewSelectCmdProc _ANSI_ARGS_((ClientData clientData));
extern void Blt_TreeViewInsertText _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr, char *string, int extra, int insertPos));
extern void Blt_TreeViewComputeLayout _ANSI_ARGS_((TreeView *tvPtr));
extern void Blt_TreeViewPercentSubst _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr, char *command, Tcl_DString *resultPtr));
extern void Blt_TreeViewDrawButton _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr, Drawable drawable, int x, int y));
extern void Blt_TreeViewDrawValue _ANSI_ARGS_((TreeView *tvPtr,
    TreeViewEntry *entryPtr, TreeViewValue *valuePtr, Drawable drawable,
    int x, int y));
extern void Blt_TreeViewDrawOuterBorders _ANSI_ARGS_((TreeView *tvPtr, 
	Drawable drawable));
extern int Blt_TreeViewDrawIcon _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr, Drawable drawable, int x, int y));
extern void Blt_TreeViewDrawHeadings _ANSI_ARGS_((TreeView *tvPtr, 
	Drawable drawable));
extern void Blt_TreeViewDrawRule _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewColumn *columnPtr, Drawable drawable));

extern void Blt_TreeViewConfigureButtons _ANSI_ARGS_((TreeView *tvPtr));
extern int Blt_TreeViewUpdateWidget _ANSI_ARGS_((Tcl_Interp *interp, 
	TreeView *tvPtr));
extern int Blt_TreeViewScreenToIndex _ANSI_ARGS_((TreeView *tvPtr, 
	int x, int y));

extern void Blt_TreeViewFreeIcon _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewIcon icon));
extern TreeViewIcon Blt_TreeViewGetIcon _ANSI_ARGS_((TreeView *tvPtr,
	CONST char *iconName));
extern void Blt_TreeViewAddValue _ANSI_ARGS_((TreeViewEntry *entryPtr, 
	TreeViewColumn *columnPtr));
extern int Blt_TreeViewCreateColumn _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewColumn *columnPtr, char *name, char *defaultLabel));
extern void Blt_TreeViewDestroyValue _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewValue *valuePtr));
extern TreeViewValue *Blt_TreeViewFindValue _ANSI_ARGS_((
	TreeViewEntry *entryPtr, TreeViewColumn *columnPtr));
extern void Blt_TreeViewDestroyColumns _ANSI_ARGS_((TreeView *tvPtr));
extern void Blt_TreeViewAllocateColumnUids _ANSI_ARGS_((TreeView *tvPtr));
extern void Blt_TreeViewFreeColumnUids _ANSI_ARGS_((TreeView *tvPtr));
extern void Blt_TreeViewUpdateColumnGCs _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewColumn *columnPtr));
extern TreeViewColumn *Blt_TreeViewNearestColumn _ANSI_ARGS_((TreeView *tvPtr,
	int x, int y, ClientData *contextPtr));

extern void Blt_TreeViewDrawRule _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewColumn *columnPtr, Drawable drawable));
extern int Blt_TreeViewTextOp _ANSI_ARGS_((TreeView *tvPtr, Tcl_Interp *interp,
	int objc, Tcl_Obj *CONST *objv));
extern int Blt_TreeViewCombobox _ANSI_ARGS_((TreeView *tvPtr,
	TreeViewEntry *entryPtr, TreeViewColumn *columnPtr));
extern int Blt_TreeViewCreateEntry _ANSI_ARGS_((TreeView *tvPtr, 
	Blt_TreeNode node, int objc, Tcl_Obj *CONST *objv, int flags));
extern int Blt_TreeViewConfigureEntry _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr, int objc, Tcl_Obj *CONST *objv, int flags));
extern int Blt_TreeViewOpenEntry _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr));
extern int Blt_TreeViewCloseEntry _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr));
extern TreeViewEntry *Blt_TreeViewNextEntry _ANSI_ARGS_((
	TreeViewEntry *entryPtr, unsigned int mask));
extern TreeViewEntry *Blt_TreeViewPrevEntry _ANSI_ARGS_((
	TreeViewEntry *entryPtr, unsigned int mask));
extern int Blt_TreeViewGetEntry _ANSI_ARGS_((TreeView *tvPtr, Tcl_Obj *objPtr, 
	TreeViewEntry **entryPtrPtr));
extern int Blt_TreeViewEntryIsHidden _ANSI_ARGS_((TreeViewEntry *entryPtr));
extern TreeViewEntry *Blt_TreeViewNextSibling _ANSI_ARGS_((
	TreeViewEntry *entryPtr, unsigned int mask));
extern TreeViewEntry *Blt_TreeViewPrevSibling _ANSI_ARGS_((
	TreeViewEntry *entryPtr, unsigned int mask));
extern TreeViewEntry *Blt_TreeViewFirstChild _ANSI_ARGS_((
	TreeViewEntry *parentPtr, unsigned int mask));
extern TreeViewEntry *Blt_TreeViewLastChild _ANSI_ARGS_((
	TreeViewEntry *entryPtr, unsigned int mask));
extern TreeViewEntry *Blt_TreeViewParentEntry _ANSI_ARGS_((
	TreeViewEntry *entryPtr));

typedef int (TreeViewApplyProc) _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr));

extern int Blt_TreeViewApply _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr, TreeViewApplyProc *proc, unsigned int mask));

extern int Blt_TreeViewColumnOp _ANSI_ARGS_((TreeView *tvPtr, 
	Tcl_Interp *interp, int objc, Tcl_Obj *CONST *objv));
extern int Blt_TreeViewSortOp _ANSI_ARGS_((TreeView *tvPtr, Tcl_Interp *interp,
	int objc, Tcl_Obj *CONST *objv));
extern int Blt_TreeViewGetColumn _ANSI_ARGS_((Tcl_Interp *interp, 
	TreeView *tvPtr, Tcl_Obj *objPtr, TreeViewColumn **columnPtrPtr));

extern void Blt_TreeViewSortFlatView _ANSI_ARGS_((TreeView *tvPtr));
extern void Blt_TreeViewSortTreeView _ANSI_ARGS_((TreeView *tvPtr));

extern int Blt_TreeViewEntryIsSelected _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr));
extern void Blt_TreeViewSelectEntry _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr));
extern void Blt_TreeViewDeselectEntry _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr));
extern void Blt_TreeViewPruneSelection _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr));
extern void Blt_TreeViewClearSelection _ANSI_ARGS_((TreeView *tvPtr));
extern void Blt_TreeViewClearTags _ANSI_ARGS_((TreeView *tvPtr,
	TreeViewEntry *entryPtr));
extern int Blt_TreeViewFindTaggedEntries _ANSI_ARGS_((TreeView *tvPtr, 
	Tcl_Obj *objPtr, TreeViewTagInfo *infoPtr));
extern TreeViewEntry *Blt_TreeViewFirstTaggedEntry _ANSI_ARGS_((
	TreeViewTagInfo *infoPtr)); 
extern TreeViewEntry *Blt_TreeViewNextTaggedEntry _ANSI_ARGS_((
	TreeViewTagInfo *infoPtr)); 
extern ClientData Blt_TreeViewButtonTag _ANSI_ARGS_((TreeView *tvPtr, 
    CONST char *string));
extern ClientData Blt_TreeViewEntryTag _ANSI_ARGS_((TreeView *tvPtr, 
    CONST char *string));
extern ClientData Blt_TreeViewColumnTag _ANSI_ARGS_((TreeView *tvPtr, 
    CONST char *string));
extern void Blt_TreeViewGetTags _ANSI_ARGS_((Tcl_Interp *interp, 
	TreeView *tvPtr, TreeViewEntry *entryPtr, Blt_List list));
extern void Blt_TreeViewTraceColumn _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewColumn *columnPtr));
extern TreeViewIcon Blt_TreeViewGetEntryIcon _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewEntry *entryPtr));
extern void Blt_TreeViewSetStyleIcon _ANSI_ARGS_((TreeView *tvPtr,
	TreeViewStyle *stylePtr, TreeViewIcon icon));
extern int Blt_TreeViewGetStyle _ANSI_ARGS_((Tcl_Interp *interp, 
	TreeView *tvPtr, char *styleName, TreeViewStyle **stylePtrPtr));
extern void Blt_TreeViewFreeStyle _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewStyle *stylePtr));
extern TreeViewStyle *Blt_TreeViewCreateStyle _ANSI_ARGS_((Tcl_Interp *interp,
	TreeView *tvPtr, int type, char *styleName));
extern void Blt_TreeViewUpdateStyleGCs _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewStyle *stylePtr));
extern Tk_3DBorder Blt_TreeViewGetStyleBorder _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewStyle *stylePtr));
extern GC Blt_TreeViewGetStyleGC _ANSI_ARGS_((TreeViewStyle *stylePtr));
extern Tk_Font Blt_TreeViewGetStyleFont _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewStyle *stylePtr));
extern XColor *Blt_TreeViewGetStyleFg _ANSI_ARGS_((TreeView *tvPtr, 
	TreeViewStyle *stylePtr));
extern TreeViewEntry *Blt_NodeToEntry _ANSI_ARGS_((TreeView *tvPtr, 
	Blt_TreeNode node));
extern int Blt_TreeViewStyleOp _ANSI_ARGS_((TreeView *tvPtr, Tcl_Interp *interp,
	int objc, Tcl_Obj *CONST *objv));

#define CHOOSE(default, override)	\
	(((override) == NULL) ? (default) : (override))

#define GETLABEL(e)		\
	(((e)->labelUid != NULL) ? (e)->labelUid : Blt_TreeNodeLabel((e)->node))

#define Blt_TreeViewGetData(entryPtr, key, objPtrPtr) \
	Blt_TreeGetValueByKey((Tcl_Interp *)NULL, (entryPtr)->tvPtr->tree, \
	      (entryPtr)->node, key, objPtrPtr)

#endif /* BLT_TREEVIEW_H */
