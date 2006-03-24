
/*
 * bltHierbox.c --
 *
 *	This module implements an hierarchy widget for the BLT toolkit.
 *
 * Copyright -1998 Lucent Technologies, Inc.
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
 *	The "hierbox" widget was created by George A. Howlett.
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

#include "bltInt.h"

#ifndef NO_HIERBOX
#include "bltBind.h"
#include "bltImage.h"
#include "bltHash.h"
#include "bltChain.h"
#include "bltList.h"
#include "bltTile.h"
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <ctype.h>

#if HAVE_UTF
#else
#define Tcl_NumUtfChars(s,n)	(((n) == -1) ? strlen((s)) : (n))
#define Tcl_UtfAtIndex(s,i)	((s) + (i))
#endif

#define SEPARATOR_NONE		((char *)-1)
#define SEPARATOR_LIST		((char *)NULL)
#define APPEND			(-1)

/*
 * The macro below is used to modify a "char" value (e.g. by casting
 * it to an unsigned character) so that it can be used safely with
 * macros such as isspace.
 */
#define UCHAR(c) ((unsigned char) (c))

#define APPLY_BEFORE		(1<<0)
#define APPLY_OPEN_ONLY		(1<<1)
#define APPLY_RECURSE		(1<<2)

#define BUTTON_IPAD		1
#define BUTTON_SIZE		7
#define INSET_PAD		2
#define ICON_PADX		2
#define ICON_PADY		1
#define LABEL_PADX		3
#define LABEL_PADY		0
#define FOCUS_WIDTH		1

#define CLAMP(val,low,hi)	\
	(((val) < (low)) ? (low) : ((val) > (hi)) ? (hi) : (val))
#define ODD(x)			((x) | 0x01)
#define TOGGLE(x, mask)		\
	(((x) & (mask)) ? ((x) & ~(mask)) : ((x) | (mask)))

#define VPORTWIDTH(h)		(Tk_Width((h)->tkwin) - 2 * (h)->inset)
#define VPORTHEIGHT(h)		(Tk_Height((h)->tkwin) - 2 * (h)->inset)

#define WORLDX(h, sx)		((sx) - (h)->inset + (h)->xOffset)
#define WORLDY(h, sy)		((sy) - (h)->inset + (h)->yOffset)

#define SCREENX(h, wx)		((wx) - (h)->xOffset + (h)->inset)
#define SCREENY(h, wy)		((wy) - (h)->yOffset + (h)->inset)

#define LEVELWIDTH(d)		(hboxPtr->levelInfo[(d)].width)
#define LEVELX(d)		(hboxPtr->levelInfo[(d)].x)

#define GETFONT(h, f)		(((f) == NULL) ? (h)->defFont : (f))
#define GETCOLOR(h, c)		(((c) == NULL) ? (h)->defColor : (c))

/*
 * ----------------------------------------------------------------------------
 *
 *  Internal hierarchy widget flags:
 *
 *	HIERBOX_LAYOUT		The layout of the hierarchy needs to be
 *				recomputed.
 *
 *	HIERBOX_REDRAW		A redraw request is pending for the widget.
 *
 *	HIERBOX_XSCROLL		X-scroll request is pending.
 *	HIERBOX_YSCROLL		Y-scroll request is pending.
 *	HIERBOX_SCROLL		Both X-scroll and  Y-scroll requests are
 *				pending.
 *
 *	HIERBOX_FOCUS		The widget is receiving keyboard events.
 *				Draw the focus highlight border around the
 *				widget.
 *
 *	HIERBOX_DIRTY		The hierarchy has changed, possibly invalidating
 *				locations and pointers to entries.  This widget
 *				need to recompute its layout.
 *
 *	HIERBOX_BORDERS		The borders of the widget (highlight ring and
 *				3-D border) need to be redrawn.
 *
 *
 *  Selection related flags:
 *
 *	SELECTION_EXPORT	Export the selection to X.
 *
 *	SELECTION_PENDING	A selection command idle task is pending.
 *
 *	SELECTION_CLEAR		Entry's selection flag is to be cleared.
 *
 *	SELECTION_SET		Entry's selection flag is to be set.
 *
 *	SELECTION_TOGGLE	Entry's selection flag is to be toggled.
 *
 *	SELECTION_MASK		Mask of selection set/clear/toggle flags.
 *
 * ---------------------------------------------------------------------------
 */
#define HIERBOX_LAYOUT		(1<<0)
#define HIERBOX_REDRAW		(1<<1)
#define HIERBOX_XSCROLL		(1<<2)
#define HIERBOX_YSCROLL		(1<<3)
#define HIERBOX_SCROLL		(HIERBOX_XSCROLL | HIERBOX_YSCROLL)
#define HIERBOX_FOCUS		(1<<4)
#define HIERBOX_DIRTY		(1<<5)
#define HIERBOX_BORDERS		(1<<6)

#define SELECTION_PENDING	(1<<15)
#define SELECTION_EXPORT	(1<<16)
#define SELECTION_CLEAR		(1<<17)
#define SELECTION_SET		(1<<18)
#define SELECTION_TOGGLE	(SELECTION_SET | SELECTION_CLEAR)
#define SELECTION_MASK		(SELECTION_SET | SELECTION_CLEAR)

/*
 * -------------------------------------------------------------------------
 *
 *  Internal entry flags:
 *
 *	ENTRY_BUTTON		Indicates that a button needs to be
 *				drawn for this entry.
 *
 *	ENTRY_OPEN		Indicates that the entry is open and
 *				its subentries should also be displayed.
 *
 *	ENTRY_MAPPED		Indicates that the entry is mapped (i.e.
 *				can be viewed by opening or scrolling.
 *
 *	BUTTON_AUTO
 *	BUTTON_SHOW
 *	BUTTON_MASK
 *
 * -------------------------------------------------------------------------
 */
#define ENTRY_BUTTON	(1<<0)
#define ENTRY_OPEN	(1<<2)
#define ENTRY_MAPPED	(1<<3)
#define BUTTON_AUTO	(1<<8)
#define BUTTON_SHOW	(1<<9)
#define BUTTON_MASK	(BUTTON_AUTO | BUTTON_SHOW)

#define DEF_ENTRY_BACKGROUND		(char *)NULL
#define DEF_ENTRY_BG_MONO		(char *)NULL
#define DEF_ENTRY_BIND_TAGS		"Entry all"
#define DEF_ENTRY_BUTTON		"auto"
#define DEF_ENTRY_COMMAND		(char *)NULL
#define DEF_ENTRY_DATA			(char *)NULL
#define DEF_ENTRY_FOREGROUND		(char *)NULL
#define DEF_ENTRY_FG_MONO		(char *)NULL
#define DEF_ENTRY_FONT			(char *)NULL
#define DEF_ENTRY_ICONS			(char *)NULL
#define DEF_ENTRY_ACTIVE_ICONS		(char *)NULL
#define DEF_ENTRY_IMAGES		(char *)NULL
#define DEF_ENTRY_LABEL			(char *)NULL
#define DEF_ENTRY_SHADOW_COLOR		(char *)NULL
#define DEF_ENTRY_SHADOW_MONO		(char *)NULL
#define DEF_ENTRY_TEXT			(char *)NULL

#define DEF_BUTTON_ACTIVE_BACKGROUND	RGB_WHITE
#define DEF_BUTTON_ACTIVE_BG_MONO	STD_ACTIVE_BG_MONO
#define DEF_BUTTON_ACTIVE_FOREGROUND	STD_ACTIVE_FOREGROUND
#define DEF_BUTTON_ACTIVE_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_BUTTON_BORDERWIDTH		"1"
#if (TK_MAJOR_VERSION == 4) 
#define DEF_BUTTON_CLOSE_RELIEF		"flat"
#define DEF_BUTTON_OPEN_RELIEF		"flat"
#else
#define DEF_BUTTON_CLOSE_RELIEF		"solid"
#define DEF_BUTTON_OPEN_RELIEF		"solid"
#endif
#define DEF_BUTTON_IMAGES		(char *)NULL
#define DEF_BUTTON_NORMAL_BACKGROUND	RGB_WHITE
#define DEF_BUTTON_NORMAL_BG_MONO	STD_NORMAL_BG_MONO
#define DEF_BUTTON_NORMAL_FOREGROUND	STD_NORMAL_FOREGROUND
#define DEF_BUTTON_NORMAL_FG_MONO	STD_NORMAL_FG_MONO
#define DEF_BUTTON_SIZE			"7"

#define DEF_HIERBOX_ACTIVE_BACKGROUND	RGB_LIGHTBLUE0
#define DEF_HIERBOX_ACTIVE_SELECT_BACKGROUND	RGB_LIGHTBLUE1
#define DEF_HIERBOX_ACTIVE_BG_MONO	STD_ACTIVE_BG_MONO
#define DEF_HIERBOX_ACTIVE_FOREGROUND	RGB_BLACK
#define DEF_HIERBOX_ACTIVE_RELIEF	"flat"
#define DEF_HIERBOX_ACTIVE_STIPPLE	"gray25"
#define DEF_HIERBOX_ALLOW_DUPLICATES	"yes"
#define DEF_HIERBOX_BACKGROUND		RGB_WHITE
#define DEF_HIERBOX_BORDERWIDTH	STD_BORDERWIDTH
#define DEF_HIERBOX_COMMAND		(char *)NULL
#define DEF_HIERBOX_CURSOR		(char *)NULL
#define DEF_HIERBOX_DASHES		"dot"
#define DEF_HIERBOX_EXPORT_SELECTION	"no"
#define DEF_HIERBOX_FOREGROUND		STD_NORMAL_FOREGROUND
#define DEF_HIERBOX_FG_MONO		STD_NORMAL_FG_MONO
#define DEF_HIERBOX_FOCUS_DASHES	"dot"
#define DEF_HIERBOX_FOCUS_EDIT		"no"
#define DEF_HIERBOX_FOCUS_FOREGROUND	STD_ACTIVE_FOREGROUND
#define DEF_HIERBOX_FOCUS_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_HIERBOX_FONT		STD_FONT
#define DEF_HIERBOX_HEIGHT		"400"
#define DEF_HIERBOX_HIDE_ROOT		"no"
#define DEF_HIERBOX_HIGHLIGHT_BACKGROUND	STD_NORMAL_BACKGROUND
#define DEF_HIERBOX_HIGHLIGHT_BG_MONO	STD_NORMAL_BG_MONO
#define DEF_HIERBOX_HIGHLIGHT_COLOR	RGB_BLACK
#define DEF_HIERBOX_HIGHLIGHT_WIDTH	"2"
#define DEF_HIERBOX_LINE_COLOR		RGB_GREY50
#define DEF_HIERBOX_LINE_MONO		STD_NORMAL_FG_MONO
#define DEF_HIERBOX_LINE_SPACING	"0"
#define DEF_HIERBOX_LINE_WIDTH		"1"
#define DEF_HIERBOX_MAKE_PATH		"no"
#define DEF_HIERBOX_NORMAL_BACKGROUND 	STD_NORMAL_BACKGROUND
#define DEF_HIERBOX_NORMAL_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_HIERBOX_RELIEF		"sunken"
#define DEF_HIERBOX_SCROLL_INCREMENT 	"0"
#define DEF_HIERBOX_SCROLL_MODE		"hierbox"
#define DEF_HIERBOX_SCROLL_TILE		"yes"
#define DEF_HIERBOX_SELECT_BACKGROUND 	RGB_LIGHTSKYBLUE1
#define DEF_HIERBOX_SELECT_BG_MONO  	STD_SELECT_BG_MONO
#define DEF_HIERBOX_SELECT_BORDERWIDTH "1"
#define DEF_HIERBOX_SELECT_CMD		(char *)NULL
#define DEF_HIERBOX_SELECT_FOREGROUND 	STD_SELECT_FOREGROUND
#define DEF_HIERBOX_SELECT_FG_MONO  	STD_SELECT_FG_MONO
#define DEF_HIERBOX_SELECT_MODE		"single"
#define DEF_HIERBOX_SELECT_RELIEF	"flat"
#define DEF_HIERBOX_SEPARATOR		(char *)NULL
#define DEF_HIERBOX_SHOW_ROOT		"yes"
#define DEF_HIERBOX_SORT_SELECTION	"no"
#define DEF_HIERBOX_TAKE_FOCUS		"1"
#define DEF_HIERBOX_TEXT_COLOR		STD_NORMAL_FOREGROUND
#define DEF_HIERBOX_TEXT_MONO		STD_NORMAL_FG_MONO
#define DEF_HIERBOX_TILE		(char *)NULL
#define DEF_HIERBOX_TRIMLEFT		""
#define DEF_HIERBOX_WIDTH		"200"

typedef struct HierboxStruct Hierbox;
typedef struct EntryStruct Entry;
typedef struct TreeStruct Tree;

typedef int (CompareProc) _ANSI_ARGS_((Tcl_Interp *interp, char *name,
	char *pattern));
typedef int (ApplyProc) _ANSI_ARGS_((Hierbox *hboxPtr, Tree * treePtr));
typedef Tree *(IterProc) _ANSI_ARGS_((Tree * treePtr, unsigned int mask));

extern Tk_CustomOption bltDashesOption;
extern Tk_CustomOption bltDistanceOption;
extern Tk_CustomOption bltShadowOption;
extern Tk_CustomOption bltTileOption;
extern Tk_CustomOption bltUidOption;

static Tk_OptionParseProc StringToButton;
static Tk_OptionPrintProc ButtonToString;
static Tk_OptionParseProc StringToImages;
static Tk_OptionPrintProc ImagesToString;
static Tk_OptionParseProc StringToScrollMode;
static Tk_OptionPrintProc ScrollModeToString;
static Tk_OptionParseProc StringToSeparator;
static Tk_OptionPrintProc SeparatorToString;
/*
 * Contains a pointer to the widget that's currently being configured.
 * This is used in the custom configuration parse routine for images.
 */
static Hierbox *hierBox;

static Tk_CustomOption imagesOption =
{
    StringToImages, ImagesToString, (ClientData)&hierBox,
};
static Tk_CustomOption buttonOption =
{
    StringToButton, ButtonToString, (ClientData)0,
};
static Tk_CustomOption scrollModeOption =
{
    StringToScrollMode, ScrollModeToString, (ClientData)0,
};
static Tk_CustomOption separatorOption = 
{
    StringToSeparator, SeparatorToString, (ClientData)0,
};
/*
 * CachedImage --
 *
 *	Since instances of the same Tk image can be displayed in
 *	different windows with possibly different color palettes, Tk
 *	internally stores each instance in a linked list.  But if
 *	the instances are used in the same widget and therefore use
 *	the same color palette, this adds a lot of overhead,
 *	especially when deleting instances from the linked list.
 *
 *	For the hierbox widget, we never need more than a single
 *	instance of an image, regardless of how many times it's used.
 *	So one solution is to cache the image, maintaining a reference
 *	count for each image used in the widget.  It's likely that the
 *	hierarchy widget will use many instances of the same image
 *	(for example the open/close icons).
 */

typedef struct CachedImageStruct {
    Tk_Image tkImage;		/* The Tk image being cached. */
    int refCount;		/* Reference count for this image. */
    short int width, height;	/* Dimensions of the cached image. */
    Blt_HashEntry *hashPtr;	/* Hash table pointer to the image. */
} *CachedImage;

#define ImageHeight(image)		((image)->height)
#define ImageWidth(image)		((image)->width)
#define ImageBits(image)		((image)->tkImage)

/*
 * Tree --
 *
 *	Structure representing a general order tree.
 *
 */
struct TreeStruct {
    Blt_Uid nameId;		/* String identifying the node within this
				 * hierarchy.  Does not necessarily have to
				 * be unique. */

    Entry *entryPtr;		/* Points to the entry structure at this
				 * node. */

    Tree *parentPtr;		/* Pointer to the parent node.
				 * If NULL, this is the root node. */

    Blt_Chain *chainPtr;	/* Pointer to list of child nodes. The list
				 * isn't allocated until the child nodes are
				 * to be inserted. */

    Blt_ChainLink *linkPtr;	/* List link containing this node in parent.
				 * This is used to remove the node from
				 * its parent when destroying the node. */

    short int level;		/* The level of this node in the tree. */
};

/*
 * Entry --
 *
 *	Contains data-specific information how to represent the data
 *	of a node of the hierarchy.
 *
 */
struct EntryStruct {
    int worldX, worldY;		/* X-Y position in world coordinates
				 * where the entry is positioned. */

    short int width, height;	/* Dimensions of the entry. */

    int lineHeight;		/* Length of the vertical line segment. */

    unsigned int flags;		/* Flags for this entry. For the
				 * definitions of the various bit
				 * fields see below. */

    Blt_Uid dataUid;		/* This value isn't used in C code.
				 * It may be used in Tcl bindings to
				 * associate extra data (other than
				 * the label, image, or text) with the
				 * entry. */

    Blt_Uid tags;		/* List of binding tags for this
				 * entry.  Blt_Uid, not a string,
				 * because in the typical case most
				 * entries will have the same
				 * bintags. */
    Blt_HashEntry *hashPtr;	/* Points the hash entry containing this
				 * entry.  This is how the node index is
				 * stored--as the key of the hash entry. */
    Hierbox *hboxPtr;

    Blt_Uid openCmd, closeCmd;	/* Tcl commands to invoke when entries
				 * are opened or closed. They override
				 * those specified globally. */
    /*
     * Button information:
     */
    short int buttonX, buttonY;	/* X-Y coordinate offsets from to
				 * upper left corner of the entry to
				 * the upper-left corner of the
				 * button.  Used to pick the
				 * button quickly */

    CachedImage *icons;		/* Tk images displayed for the entry.
				 * The first image is the icon
				 * displayed to the left of the
				 * entry's label. The second is icon
				 * displayed when entry is "open". */

    GC iconGC;			/* GC for drawing default bitmaps. */

    short int iconWidth, iconHeight;
				/* Maximum dimensions for icons and
				 * buttons for this entry. This is
				 * used to align the button, icon, and
				 * text. */

    CachedImage *activeIcons;	/* Tk images displayed for the entry.
				 * The first image is the icon
				 * displayed to the left of the
				 * entry's label. The second is icon
				 * displayed when entry is "open". */

    short int levelX;		/* X-coordinate offset of data image
				 * or text for children nodes. */
    /*
     * Label information:
     */
    short int labelWidth, labelHeight;
    char *labelText;		/* Text displayed right of the icon. */


    Tk_Font labelFont;		/* Font of label. Overrides global font
				 * specification. */

    XColor *labelColor;		/* Color of label. Overrides default text color
				 * specification. */

    GC labelGC;

    Shadow labelShadow;

    /*
     * Data (text or image) information:
     */
    Blt_Uid dataText;

    Tk_Font dataFont;

    XColor *dataColor;

    Shadow dataShadow;

    GC dataGC;

    CachedImage *images;

};


static Tk_ConfigSpec entryConfigSpecs[] =
{
    {TK_CONFIG_CUSTOM, "-activeicons", "activeIcons", "Icons",
	DEF_ENTRY_ICONS, Tk_Offset(Entry, activeIcons),
	TK_CONFIG_NULL_OK, &imagesOption},
    {TK_CONFIG_CUSTOM, "-bindtags", "bindTags", "BindTags",
	DEF_ENTRY_BIND_TAGS, Tk_Offset(Entry, tags),
	TK_CONFIG_NULL_OK, &bltUidOption},
    {TK_CONFIG_CUSTOM, "-closecommand", "entryCloseCommand",
	"EntryCloseCommand",
	DEF_ENTRY_COMMAND, Tk_Offset(Entry, closeCmd),
	TK_CONFIG_NULL_OK, &bltUidOption},
    {TK_CONFIG_CUSTOM, "-data", "data", "data",
	DEF_ENTRY_DATA, Tk_Offset(Entry, dataUid),
	TK_CONFIG_NULL_OK, &bltUidOption},
    {TK_CONFIG_CUSTOM, "-button", "button", "Button",
	DEF_ENTRY_BUTTON, Tk_Offset(Entry, flags),
	TK_CONFIG_DONT_SET_DEFAULT, &buttonOption},
    {TK_CONFIG_CUSTOM, "-icons", "icons", "Icons",
	DEF_ENTRY_ICONS, Tk_Offset(Entry, icons),
	TK_CONFIG_NULL_OK, &imagesOption},
    {TK_CONFIG_CUSTOM, "-images", "images", "Images",
	DEF_ENTRY_IMAGES, Tk_Offset(Entry, images),
	TK_CONFIG_NULL_OK, &imagesOption},
    {TK_CONFIG_STRING, "-label", "label", "Label",
	DEF_ENTRY_LABEL, Tk_Offset(Entry, labelText), 0},
    {TK_CONFIG_COLOR, "-labelcolor", "labelColor", "LabelColor",
	DEF_ENTRY_FOREGROUND, Tk_Offset(Entry, labelColor),
	TK_CONFIG_NULL_OK | TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-labelcolor", "labelColor", "LabelColor",
	DEF_ENTRY_FG_MONO, Tk_Offset(Entry, labelColor),
	TK_CONFIG_NULL_OK | TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_FONT, "-labelfont", "labelFont", "LabelFont",
	DEF_ENTRY_FONT, Tk_Offset(Entry, labelFont), 0},
    {TK_CONFIG_CUSTOM, "-labelshadow", "labelShadow", "LabelShadow",
	DEF_ENTRY_SHADOW_COLOR, Tk_Offset(Entry, labelShadow),
	TK_CONFIG_NULL_OK | TK_CONFIG_COLOR_ONLY, &bltShadowOption},
    {TK_CONFIG_CUSTOM, "-labelshadow", "labelShadow", "LabelShadow",
	DEF_ENTRY_SHADOW_MONO, Tk_Offset(Entry, labelShadow),
	TK_CONFIG_NULL_OK | TK_CONFIG_MONO_ONLY, &bltShadowOption},
    {TK_CONFIG_CUSTOM, "-opencommand", "entryOpenCommand", "EntryOpenCommand",
	DEF_ENTRY_COMMAND, Tk_Offset(Entry, openCmd),
	TK_CONFIG_NULL_OK, &bltUidOption},
    {TK_CONFIG_CUSTOM, "-text", "text", "Text",
	DEF_ENTRY_LABEL, Tk_Offset(Entry, dataText),
	TK_CONFIG_NULL_OK, &bltUidOption},
    {TK_CONFIG_COLOR, "-textcolor", "textColor", "TextColor",
	DEF_ENTRY_FOREGROUND, Tk_Offset(Entry, dataColor),
	TK_CONFIG_NULL_OK | TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_FONT, "-textfont", "textFont", "TextFont",
	DEF_ENTRY_FONT, Tk_Offset(Entry, dataFont), 0},
    {TK_CONFIG_CUSTOM, "-textshadow", "textShadow", "Shadow",
	DEF_ENTRY_SHADOW_COLOR, Tk_Offset(Entry, dataShadow),
	TK_CONFIG_NULL_OK | TK_CONFIG_COLOR_ONLY, &bltShadowOption},
    {TK_CONFIG_CUSTOM, "-textShadow", "textShadow", "Shadow",
	DEF_ENTRY_SHADOW_MONO, Tk_Offset(Entry, dataShadow),
	TK_CONFIG_NULL_OK | TK_CONFIG_MONO_ONLY, &bltShadowOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

/*
 * ButtonAttributes --
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
    GC lineGC, normalGC, activeGC;
    int reqSize;
    int borderWidth;
    int openRelief, closeRelief;
    int width, height;
    CachedImage *images;

} ButtonAttributes;

/*
 * TextEdit --
 *
 *	This structure is shared by entries when their labels are
 *	edited via the keyboard.  It maintains the location of the
 *	insertion cursor and the text selection for the editted entry.
 *	The structure is shared since we need only one.  The "focus"
 *	entry should be the only entry receiving KeyPress/KeyRelease
 *	events at any time.  Information from the previously editted
 *	entry is overwritten.
 *
 *	Note that all the indices internally are in terms of bytes,
 *	not characters.  This is because UTF-8 strings may encode a
 *	single character into a multi-byte sequence.  To find the
 *	respective character position
 *
 *		n = Tcl_NumUtfChars(string, index);
 *
 *	where n is the resulting character number.
 */
typedef struct {
    int active;
    int insertPos;		/* Position of the cursor within the
				 * array of bytes of the entry's label. */
    int x, y;			/* Offsets of the insertion cursolsr from the
				 * start of the label.  Used to draw the
				 * the cursor relative to the entry. */
    int width, height;		/* Size of the insertion cursor symbol. */

    int selAnchor;		/* Fixed end of selection. Used to extend
				 * the selection while maintaining the
				 * other end of the selection. */
    int selFirst;		/* Position of the first character in the
				 * selection. */
    int selLast;		/* Position of the last character in the
				 * selection. */

    int cursorOn;		/* Indicates if the cursor is displayed. */
    int onTime, offTime;	/* Time in milliseconds to wait before
				 * changing the cursor from off-to-on
				 * and on-to-off. Setting offTime to 0 makes
				 * the cursor steady. */
    Tcl_TimerToken timerToken;	/* Handle for a timer event called periodically
				 * to blink the cursor. */

} TextEdit;

/*
 * LevelInfo --
 *
 */

typedef struct {
    int x;
    int width;
} LevelInfo;

/*
 * Hierbox --
 *
 *	Represents the hierarchy widget that contains one or more
 *	entries.
 *
 *	Entries are positioned in "world" coordinates, refering to
 *	the virtual hierbox space.  Coordinate 0,0 is the upper-left
 *	of the hierarchy box and the bottom is the end of the last
 *	entry.  The widget's Tk window acts as view port into this
 *	virtual space. The hierbox's xOffset and yOffset fields specify
 *	the location of the view port in the virtual world.  Scrolling
 *	the viewport is therefore simply changing the xOffset and/or
 *	yOffset fields and redrawing.
 *
 *	Note that world coordinates are integers, not signed short
 *	integers like X11 screen coordinates.  It's very easy to
 *	create a hierarchy that is more than 0x7FFF pixels high.
 *
 */
struct HierboxStruct {
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

    int flags;			/* For bitfield definitions, see below */

    int allowDuplicates;	/* Allow duplicate entries. */

    int autoCreate;		/* Automatically create path entries
				 * as needed. */
    int exportSelection;	/* Export the selection to X. */

    int hideRoot;		/* Don't display the root entry. */

    int scrollTile;		/* Adjust the tile along with viewport
				 * offsets as the widget is
				 * scrolled. */

    int inset;			/* Total width of all borders,
				 * including traversal highlight and
				 * 3-D border.  Indicates how much
				 * interior stuff must be offset from
				 * outside edges to leave room for
				 * borders. */

    Tk_3DBorder border;		/* 3D border surrounding the window
				 * (viewport). */
    int borderWidth;		/* Width of 3D border. */
    int relief;			/* 3D border relief. */


    int highlightWidth;		/* Width in pixels of highlight to draw
				 * around widget when it has the focus.
				 * <= 0 means don't draw a highlight. */
    XColor *highlightBgColor;	/* Color for drawing traversal highlight
				 * area when highlight is off. */
    XColor *highlightColor;	/* Color for drawing traversal highlight. */

    Blt_Tile tile;		/* Tiled background */

    char *separator;		/* Pathname separators */
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
     * colored box with optionally an border. It has both "active"
     * and "normal" colors.
     */
    ButtonAttributes button;


    /*
     * Selection Information:
     *
     * The selection is the rectangle that contains a selected entry.
     * There may be many selected entries.  It is displayed as a
     * solid colored box with optionally a 3D border.
     */
    Tk_3DBorder selBorder;	/* Background color of an highlighted entry.*/

    int selRelief;		/* Relief of selected items. Currently
				 * is always raised. */

    int selBorderWidth;		/* Border width of a selected entry.*/

    XColor *selFgColor;		/* Text color of a selected entry. */

    Tree *selAnchorPtr;		/* Fixed end of selection (i.e. node
				 * at which selection was started.) */

    Blt_HashTable selectTable;
    Blt_Chain selectChain;

    int sortSelection;		/* Indicates if the entries in the
				 * selection should be sorted or
				 * displayed in the order they were
				 * selected. */

    char *selectMode;		/* Selection style.  This value isn't
				 * used in C code (it's just a place
				 * holder), but for the widget's Tcl
				 * bindings. */

    char *selectCmd;		/* Tcl script that's invoked whenever
				 * the selection changes. */

    int leader;			/* Number of pixels padding between entries */

    Tk_Cursor cursor;		/* X Cursor */

    int reqWidth, reqHeight;	/* Requested dimensions of the widget's
				 * window. */

    GC lineGC;			/* GC for drawing dotted line between
				 * entries. */

    XColor *activeFgColor;
    Tk_3DBorder activeBorder;

    XColor *focusColor;
    Blt_Dashes focusDashes;	/* Dash on-off value. */
    GC focusGC;			/* Graphics context for the active label */
    int focusEdit;		/* Indicates if the label of the "focus" entry
				 * can be editted. */
    TextEdit labelEdit;

    Tree *activePtr;
    Tree *focusPtr;		/* Pointer to node that's currently active. */
    Tree *activeButtonPtr;	/* Pointer to last active button */

    /* Number of pixels to move for 1 scroll unit. */
    int reqScrollX, reqScrollY;
    int xScrollUnits, yScrollUnits;

    /* Command strings to control horizontal and vertical scrollbars. */
    char *xScrollCmdPrefix, *yScrollCmdPrefix;

    int scrollMode;		/* Selects mode of scrolling: either
				 * BLT_SCROLL_MODE_HIERBOX, 
				 * BLT_SCROLL_MODE_LISTBOX, or
				 * BLT_SCROLL_MODE_CANVAS. */

    /*
     * Total size of all "open" entries. This represents the range of
     * world coordinates.
     */
    int worldWidth, worldHeight;
    int xOffset, yOffset;	/* Translation between view port and
				 * world origin. */
    int minHeight;		/* Minimum entry height. Used to
				 * to compute what the y-scroll unit
				 * should be. */
    LevelInfo *levelInfo;
    /*
     * Scanning information:
     */
    int scanAnchorX, scanAnchorY;
    /* Scan anchor in screen coordinates. */
    int scanX, scanY;		/* X-Y world coordinate where the scan
				 * started. */


    Blt_HashTable nodeTable;	/* Table of node identifiers */
    Blt_HashTable imageTable;	/* Table of Tk images */
    Tree *rootPtr;		/* Root of hierarchy */
    int depth;			/* Maximum depth of the hierarchy. */

    Tree **visibleArr;		/* Array of visible entries */
    int nVisible;		/* Number of entries in the above array */

    int nextSerial;

    char *openCmd, *closeCmd;	/* Tcl commands to invoke when entries
				 * are opened or closed. */

    Tk_Font defFont;		/* Specifies a default font for labels and
				 * text data.  It may be overridden for a
				 * single entry by its -*font option. */

    XColor *defColor;		/* Global text color for labels. This
				 * may be overridden by the -color
				 * option for an individual entry. */

    Pixmap iconBitmap, iconMask;/* Default icon bitmaps */
    XColor *iconColor;

    char *takeFocus;
    char *sortCmd;		/* Tcl command to invoke to sort entries */

    ClientData clientData;
    Blt_BindTable bindTable;	/* Binding information for entries. */
    Blt_BindTable buttonBindTable; /* Binding information for buttons. */
};

static Tk_ConfigSpec buttonConfigSpecs[] =
{
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Background",
	DEF_BUTTON_ACTIVE_BG_MONO, Tk_Offset(Hierbox, button.activeBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Background",
	DEF_BUTTON_ACTIVE_BACKGROUND, Tk_Offset(Hierbox, button.activeBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Foreground",
	DEF_BUTTON_ACTIVE_FOREGROUND, Tk_Offset(Hierbox, button.activeFgColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Foreground",
	DEF_BUTTON_ACTIVE_FG_MONO, Tk_Offset(Hierbox, button.activeFgColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_BUTTON_NORMAL_BACKGROUND, Tk_Offset(Hierbox, button.border),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_BUTTON_NORMAL_BG_MONO, Tk_Offset(Hierbox, button.border),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_BUTTON_BORDERWIDTH, Tk_Offset(Hierbox, button.borderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_RELIEF, "-closerelief", "closeRelief", "Relief",
	DEF_BUTTON_CLOSE_RELIEF, Tk_Offset(Hierbox, button.closeRelief),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_BUTTON_NORMAL_FOREGROUND, Tk_Offset(Hierbox, button.fgColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_BUTTON_NORMAL_FG_MONO, Tk_Offset(Hierbox, button.fgColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-images", "images", "Images",
	DEF_BUTTON_IMAGES, Tk_Offset(Hierbox, button.images),
	TK_CONFIG_NULL_OK, &imagesOption},
    {TK_CONFIG_RELIEF, "-openrelief", "openRelief", "Relief",
	DEF_BUTTON_OPEN_RELIEF, Tk_Offset(Hierbox, button.openRelief),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-size", "size", "Size", DEF_BUTTON_SIZE, 
	Tk_Offset(Hierbox, button.reqSize), 0, &bltDistanceOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground",
	"ActiveBackground",
	DEF_HIERBOX_ACTIVE_BACKGROUND, Tk_Offset(Hierbox, activeBorder),
	TK_CONFIG_COLOR_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground",
	"ActiveBackground",
	DEF_HIERBOX_ACTIVE_BG_MONO, Tk_Offset(Hierbox, activeBorder),
	TK_CONFIG_MONO_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Foreground",
	DEF_HIERBOX_ACTIVE_FOREGROUND, Tk_Offset(Hierbox, activeFgColor), 0},
    {TK_CONFIG_BOOLEAN, "-allowduplicates", "allowDuplicates",
	"AllowDuplicates",
	DEF_HIERBOX_ALLOW_DUPLICATES, Tk_Offset(Hierbox, allowDuplicates),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, "-autocreate", "autoCreate", "AutoCreate",
	DEF_HIERBOX_MAKE_PATH, Tk_Offset(Hierbox, autoCreate),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_HIERBOX_BACKGROUND, Tk_Offset(Hierbox, border), 0},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_HIERBOX_CURSOR, Tk_Offset(Hierbox, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_HIERBOX_BORDERWIDTH, Tk_Offset(Hierbox, borderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_STRING, "-closecommand", "closeCommand", "CloseCommand",
	DEF_HIERBOX_COMMAND, Tk_Offset(Hierbox, closeCmd), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-dashes", "dashes", "Dashes",
	DEF_HIERBOX_DASHES, Tk_Offset(Hierbox, dashes),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_BOOLEAN, "-exportselection", "exportSelection",
	"ExportSelection",
	DEF_HIERBOX_EXPORT_SELECTION, Tk_Offset(Hierbox, exportSelection),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, "-focusdashes", "focusDashes", "FocusDashes",
	DEF_HIERBOX_FOCUS_DASHES, Tk_Offset(Hierbox, focusDashes),
	TK_CONFIG_NULL_OK, &bltDashesOption},
    {TK_CONFIG_BOOLEAN, "-focusedit", "focusEdit", "FocusEdit",
	DEF_HIERBOX_FOCUS_EDIT, Tk_Offset(Hierbox, focusEdit), 0},
    {TK_CONFIG_COLOR, "-focusforeground", "focusForeground",
	"FocusForeground",
	DEF_HIERBOX_FOCUS_FOREGROUND, Tk_Offset(Hierbox, focusColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-focusforeground", "focusForeground",
	"FocusForeground",
	DEF_HIERBOX_FOCUS_FG_MONO, Tk_Offset(Hierbox, focusColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_HIERBOX_FONT, Tk_Offset(Hierbox, defFont), 0},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_HIERBOX_TEXT_COLOR, Tk_Offset(Hierbox, defColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_HIERBOX_TEXT_MONO, Tk_Offset(Hierbox, defColor), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-height", "height", "Height",
	DEF_HIERBOX_HEIGHT, Tk_Offset(Hierbox, reqHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_BOOLEAN, "-hideroot", "hideRoot", "HideRoot",
	DEF_HIERBOX_HIDE_ROOT, Tk_Offset(Hierbox, hideRoot),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground",
	DEF_HIERBOX_HIGHLIGHT_BACKGROUND, Tk_Offset(Hierbox, highlightBgColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground",
	DEF_HIERBOX_HIGHLIGHT_BG_MONO, Tk_Offset(Hierbox, highlightBgColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_HIERBOX_HIGHLIGHT_COLOR, Tk_Offset(Hierbox, highlightColor), 0},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_HIERBOX_HIGHLIGHT_WIDTH, Tk_Offset(Hierbox, highlightWidth),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_COLOR, "-linecolor", "lineColor", "LineColor",
	DEF_HIERBOX_LINE_COLOR, Tk_Offset(Hierbox, lineColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-linecolor", "lineColor", "LineColor",
	DEF_HIERBOX_LINE_MONO, Tk_Offset(Hierbox, lineColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-linespacing", "lineSpacing", "LineSpacing",
	DEF_HIERBOX_LINE_SPACING, Tk_Offset(Hierbox, leader),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-linewidth", "lineWidth", "LineWidth",
	DEF_HIERBOX_LINE_WIDTH, Tk_Offset(Hierbox, lineWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_STRING, "-opencommand", "openCommand", "OpenCommand",
	DEF_HIERBOX_COMMAND, Tk_Offset(Hierbox, openCmd), TK_CONFIG_NULL_OK},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_HIERBOX_RELIEF, Tk_Offset(Hierbox, relief), 0},
    {TK_CONFIG_CUSTOM, "-scrollmode", "scrollMode", "ScrollMode",
	DEF_HIERBOX_SCROLL_MODE, Tk_Offset(Hierbox, scrollMode),
	TK_CONFIG_DONT_SET_DEFAULT, &scrollModeOption},
    {TK_CONFIG_BOOLEAN, "-scrolltile", "scrollTile", "ScrollTile",
	DEF_HIERBOX_SCROLL_TILE, Tk_Offset(Hierbox, scrollTile),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Foreground",
	DEF_HIERBOX_SELECT_BG_MONO, Tk_Offset(Hierbox, selBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Foreground",
	DEF_HIERBOX_SELECT_BACKGROUND, Tk_Offset(Hierbox, selBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_CUSTOM, "-selectborderwidth", "selectBorderWidth", "BorderWidth",
	DEF_HIERBOX_SELECT_BORDERWIDTH, Tk_Offset(Hierbox, selBorderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_STRING, "-selectcommand", "selectCommand", "SelectCommand",
	DEF_HIERBOX_SELECT_CMD, Tk_Offset(Hierbox, selectCmd),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_HIERBOX_SELECT_FG_MONO, Tk_Offset(Hierbox, selFgColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_HIERBOX_SELECT_FOREGROUND, Tk_Offset(Hierbox, selFgColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_STRING, "-selectmode", "selectMode", "SelectMode",
	DEF_HIERBOX_SELECT_MODE, Tk_Offset(Hierbox, selectMode),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_RELIEF, "-selectrelief", "selectRelief", "Relief",
	DEF_HIERBOX_SELECT_RELIEF, Tk_Offset(Hierbox, selRelief),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-separator", "separator", "Separator",
	DEF_HIERBOX_SEPARATOR, Tk_Offset(Hierbox, separator), 
	TK_CONFIG_NULL_OK, &separatorOption},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_HIERBOX_TAKE_FOCUS, Tk_Offset(Hierbox, takeFocus), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-tile", "tile", "Tile",
	(char *)NULL, Tk_Offset(Hierbox, tile), TK_CONFIG_NULL_OK,
	&bltTileOption},
    {TK_CONFIG_STRING, "-trimleft", "trimLeft", "Trim",
	DEF_HIERBOX_TRIMLEFT, Tk_Offset(Hierbox, trimLeft), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-width", "width", "Width",
	DEF_HIERBOX_WIDTH, Tk_Offset(Hierbox, reqWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
	(char *)NULL, Tk_Offset(Hierbox, xScrollCmdPrefix), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-xscrollincrement", "xScrollIncrement",
	"ScrollIncrement",
	DEF_HIERBOX_SCROLL_INCREMENT, Tk_Offset(Hierbox, reqScrollX),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_STRING, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
	(char *)NULL, Tk_Offset(Hierbox, yScrollCmdPrefix), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-yscrollincrement", "yScrollIncrement",
	"ScrollIncrement",
	DEF_HIERBOX_SCROLL_INCREMENT, Tk_Offset(Hierbox, reqScrollY),
	TK_CONFIG_DONT_SET_DEFAULT, &bltDistanceOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

typedef struct {
    int x, y;			/* Tracks the current world
				 * coordinates as we traverse through
				 * the tree. After a full-tree
				 * traversal, the y-coordinate will be
				 * the height of the virtual
				 * hierarchy. */
    int maxWidth;		/* Maximum entry width. This is the
				 * width of the virtual hierarchy. */
    int labelOffset;
    int minHeight;		/* Minimum entry height. Used to
				 * to compute what the y-scroll unit
				 * should be. */
    int maxIconWidth;
    int level, depth;
} LayoutInfo;

#define DEF_ICON_WIDTH 16
#define DEF_ICON_HEIGHT 16
static unsigned char folderBits[] =
{
    0x00, 0x00, 0x7c, 0x00, 0x82, 0x00, 0x81, 0x3f, 0x41, 0x40, 0x3f, 0xc0,
    0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0,
    0x01, 0xc0, 0xff, 0xff, 0xfe, 0xff, 0x00, 0x00};

static unsigned char folderMaskBits[] =
{
    0x00, 0x00, 0x7c, 0x00, 0xfe, 0x00, 0xff, 0x3f, 0xff, 0x7f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0x00, 0x00};


/* Forward Declarations */
static void DestroyHierbox _ANSI_ARGS_((DestroyData dataPtr));
static void HierboxEventProc _ANSI_ARGS_((ClientData clientdata,
	XEvent *eventPtr));
static void DrawButton _ANSI_ARGS_((Hierbox *hboxPtr, Tree * treePtr,
	Drawable drawable));
static void DisplayHierbox _ANSI_ARGS_((ClientData clientData));
static void HierboxInstCmdDeleteProc _ANSI_ARGS_((ClientData clientdata));
static int HierboxInstCmd _ANSI_ARGS_((ClientData clientdata,
	Tcl_Interp *interp, int argc, char **argv));
static void EventuallyRedraw _ANSI_ARGS_((Hierbox *hboxPtr));
static void SelectCmdProc _ANSI_ARGS_((ClientData clientData));
static void EventuallyInvokeSelectCmd _ANSI_ARGS_((Hierbox *hboxPtr));
static int ComputeVisibleEntries _ANSI_ARGS_((Hierbox *hboxPtr));
static int ConfigureEntry _ANSI_ARGS_((Hierbox *hboxPtr, Entry * entryPtr,
	int argc, char **argv, int flags));
static void ComputeLayout _ANSI_ARGS_((Hierbox *hboxPtr));

static CompareProc ExactCompare, GlobCompare, RegexpCompare;
static ApplyProc SelectNode, GetSelectedLabels, CloseNode, SizeOfNode, 
    IsSelectedNode, MapAncestors, FixUnmappedSelections, UnmapNode, 
    SortNode, OpenNode;
static IterProc NextNode, LastNode;
static Tk_ImageChangedProc ImageChangedProc;
static Blt_ChainCompareProc CompareNodesByTclCmd, CompareNodesByName;
static Blt_BindPickProc PickButton, PickEntry;
static Blt_BindTagProc GetTags;
static Tk_SelectionProc SelectionProc;
static Tk_LostSelProc LostSelection;
static Tcl_CmdProc HierboxCmd;
static Blt_TileChangedProc TileChangedProc;
static Tcl_TimerProc LabelBlinkProc;
static Tcl_FreeProc DestroyNode;


/*
 *----------------------------------------------------------------------
 *
 * StringToScrollMode --
 *
 *	Convert the string reprsenting a scroll mode, to its numeric
 *	form.
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
StringToScrollMode(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* New legend position string */
    char *widgRec;		/* Widget record */
    int offset;			/* offset to XPoint structure */
{
    int *modePtr = (int *)(widgRec + offset);

    if ((string[0] == 'l') && (strcmp(string, "listbox") == 0)) {
	*modePtr = BLT_SCROLL_MODE_LISTBOX;
    } else if ((string[0] == 'h') && (strcmp(string, "hierbox") == 0)) {
	*modePtr = BLT_SCROLL_MODE_HIERBOX;
    } else if ((string[0] == 'c') && (strcmp(string, "canvas") == 0)) {
	*modePtr = BLT_SCROLL_MODE_CANVAS;
    } else {
	Tcl_AppendResult(interp, "bad scroll mode \"", string,
	    "\": should be \"hierbox\", \"listbox\", or \"canvas\"", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ScrollModeToString --
 *
 * Results:
 *	The string representation of the button boolean is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
ScrollModeToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* offset of flags field in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    int mode = *(int *)(widgRec + offset);

    switch (mode) {
    case BLT_SCROLL_MODE_LISTBOX:
	return "listbox";
    case BLT_SCROLL_MODE_HIERBOX:
	return "hierbox";
    case BLT_SCROLL_MODE_CANVAS:
	return "canvas";
    default:
	return "unknown scroll mode";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * StringToButton --
 *
 *	Convert a string to one of three values.
 *		0 - false, no, off
 *		1 - true, yes, on
 *		2 - auto
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left in
 *	interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToButton(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* New legend position string */
    char *widgRec;		/* Widget record */
    int offset;			/* offset to XPoint structure */
{
    int *flags = (int *)(widgRec + offset);

    *flags &= ~BUTTON_MASK;
    if ((string[0] == 'a') && (strcmp(string, "auto") == 0)) {
	*flags |= BUTTON_AUTO;
    } else {
	int value;

	if (Tcl_GetBoolean(interp, string, &value) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (value) {
	    *flags |= BUTTON_SHOW;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonToString --
 *
 * Results:
 *	The string representation of the button boolean is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
ButtonToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* offset of flags field in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    unsigned int flags = *(int *)(widgRec + offset);

    switch (flags & BUTTON_MASK) {
    case 0:
	return "0";
    case BUTTON_SHOW:
	return "1";
    case BUTTON_AUTO:
	return "auto";
    default:
	return "unknown button value";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ImageChangedProc
 *
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
    Hierbox *hboxPtr = clientData;

    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
    EventuallyRedraw(hboxPtr);
}

static CachedImage
GetCachedImage(hboxPtr, interp, tkwin, name)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    Tk_Window tkwin;
    char *name;
{
    struct CachedImageStruct *imagePtr;
    int isNew;
    Blt_HashEntry *hPtr;

    hPtr = Blt_CreateHashEntry(&(hboxPtr->imageTable), (char *)name, &isNew);
    if (isNew) {
	Tk_Image tkImage;
	int width, height;

	tkImage = Tk_GetImage(interp, tkwin, name, ImageChangedProc, hboxPtr);
	if (tkImage == NULL) {
	    Blt_DeleteHashEntry(&(hboxPtr->imageTable), hPtr);
	    return NULL;
	}
	Tk_SizeOfImage(tkImage, &width, &height);
	imagePtr = Blt_Malloc(sizeof(struct CachedImageStruct));
	imagePtr->tkImage = tkImage;
	imagePtr->hashPtr = hPtr;
	imagePtr->refCount = 1;
	imagePtr->width = width, imagePtr->height = height;
	Blt_SetHashValue(hPtr, imagePtr);
    } else {
	imagePtr = (struct CachedImageStruct *)Blt_GetHashValue(hPtr);
	imagePtr->refCount++;
    }
    return imagePtr;
}

static void
FreeCachedImage(hboxPtr, imagePtr)
    Hierbox *hboxPtr;
    struct CachedImageStruct *imagePtr;
{
    imagePtr->refCount--;
    if (imagePtr->refCount == 0) {
	Blt_DeleteHashEntry(&(hboxPtr->imageTable), imagePtr->hashPtr);
	Tk_FreeImage(imagePtr->tkImage);
	Blt_Free(imagePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * StringToImages --
 *
 *	Convert a list of image names into Tk images.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left in
 *	interpreter's result field.
 *
 *----------------------------------------------------------------------
 */
static int
StringToImages(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* New legend position string */
    char *widgRec;		/* Widget record */
    int offset;			/* offset to field in structure */
{
    Hierbox *hboxPtr = *(Hierbox **)clientData;
    CachedImage **imagePtrPtr = (CachedImage **) (widgRec + offset);
    CachedImage *imageArr;
    int result;

    result = TCL_OK;
    imageArr = NULL;
    if ((string != NULL) && (*string != '\0')) {
	int nNames;
	char **nameArr;

	if (Tcl_SplitList(interp, string, &nNames, &nameArr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (nNames > 0) {
	    register int i;

	    imageArr = Blt_Malloc(sizeof(CachedImage *) * (nNames + 1));
	    assert(imageArr);
	    for (i = 0; i < nNames; i++) {
		imageArr[i] = GetCachedImage(hboxPtr, interp, tkwin, nameArr[i]);
		if (imageArr[i] == NULL) {
		    result = TCL_ERROR;
		    break;
		}
	    }
	    Blt_Free(nameArr);
	    imageArr[nNames] = NULL;
	}
    }
    if (*imagePtrPtr != NULL) {
	register CachedImage *imagePtr;

	for (imagePtr = *imagePtrPtr; *imagePtr != NULL; imagePtr++) {
	    FreeCachedImage(hboxPtr, *imagePtr);
	}
	Blt_Free(*imagePtrPtr);
    }
    *imagePtrPtr = imageArr;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ImagesToString --
 *
 *	Converts the image into its string representation (its name).
 *
 * Results:
 *	The name of the image is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
ImagesToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* offset of images array in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    CachedImage **imagePtrPtr = (CachedImage **) (widgRec + offset);
    Tcl_DString dString;
    char *result;

    Tcl_DStringInit(&dString);
    if (*imagePtrPtr != NULL) {
	register CachedImage *imagePtr;

	for (imagePtr = *imagePtrPtr; *imagePtr != NULL; imagePtr++) {
	    Tcl_DStringAppendElement(&dString, 
			     Blt_NameOfImage((*imagePtr)->tkImage));
	}
    }
    result = Blt_Strdup(Tcl_DStringValue(&dString));
    Tcl_DStringFree(&dString);
    *freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * StringToScrollmode --
 *
 *	Convert the string reprsenting a scroll mode, to its numeric
 *	form.
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
StringToSeparator(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* String representing new value */
    char *widgRec;		/* Widget record */
    int offset;			/* offset in structure */
{
    char **sepPtr = (char **)(widgRec + offset);

    if ((*sepPtr != SEPARATOR_LIST) && (*sepPtr != SEPARATOR_NONE)) {
	Blt_Free(*sepPtr);
    }
    if ((string == NULL) || (*string == '\0')) {
	*sepPtr = SEPARATOR_LIST;
    } else if (strcmp(string, "none") == 0) {
	*sepPtr = SEPARATOR_NONE;
    } else {
	*sepPtr = Blt_Strdup(string);
    } 
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SeparatorToString --
 *
 * Results:
 *	The string representation of the separator is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
SeparatorToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* offset of flags field in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    char *separator = *(char **)(widgRec + offset);

    if (separator == SEPARATOR_NONE) {
	return "";
    } else if (separator == SEPARATOR_LIST) {
	return "list";
    } 
    return separator;
}

static int
ApplyToTree(hboxPtr, rootPtr, proc, flags)
    Hierbox *hboxPtr;
    Tree *rootPtr;
    ApplyProc *proc;
    unsigned int flags;
{
    if (flags & APPLY_BEFORE) {
	if ((*proc) (hboxPtr, rootPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (flags & APPLY_RECURSE) {
	if (!(flags & APPLY_OPEN_ONLY) ||
	    (rootPtr->entryPtr->flags & ENTRY_OPEN)) {
	    Blt_ChainLink *linkPtr, *nextPtr;
	    Tree *treePtr;

	    for (linkPtr = Blt_ChainFirstLink(rootPtr->chainPtr); 
		linkPtr != NULL;  linkPtr = nextPtr) {
		/* Get the next link in the chain before calling
		 * ApplyToTree.  This is because ApplyToTree may
		 * delete the node and its link.  */
		nextPtr = Blt_ChainNextLink(linkPtr);
		treePtr = Blt_ChainGetValue(linkPtr);
		if (ApplyToTree(hboxPtr, treePtr, proc, flags) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	}
    }
    if ((flags & APPLY_BEFORE) == 0) {
	if ((*proc) (hboxPtr, rootPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

static void
ConfigureButtons(hboxPtr)
    Hierbox *hboxPtr;
{
    ButtonAttributes *buttonPtr = &(hboxPtr->button);
    unsigned long gcMask;
    XGCValues gcValues;
    GC newGC;

    gcMask = GCForeground;
    gcValues.foreground = buttonPtr->fgColor->pixel;
    newGC = Tk_GetGC(hboxPtr->tkwin, gcMask, &gcValues);
    if (buttonPtr->normalGC != NULL) {
	Tk_FreeGC(hboxPtr->display, buttonPtr->normalGC);
    }
    buttonPtr->normalGC = newGC;

    gcMask = (GCForeground | GCLineWidth);
    gcValues.foreground = hboxPtr->lineColor->pixel;
    gcValues.line_width = hboxPtr->lineWidth;
    newGC = Tk_GetGC(hboxPtr->tkwin, gcMask, &gcValues);
    if (buttonPtr->lineGC != NULL) {
	Tk_FreeGC(hboxPtr->display, buttonPtr->lineGC);
    }
    buttonPtr->lineGC = newGC;

    gcMask = GCForeground;
    gcValues.foreground = buttonPtr->activeFgColor->pixel;
    newGC = Tk_GetGC(hboxPtr->tkwin, gcMask, &gcValues);
    if (buttonPtr->activeGC != NULL) {
	Tk_FreeGC(hboxPtr->display, buttonPtr->activeGC);
    }
    buttonPtr->activeGC = newGC;

    buttonPtr->width = buttonPtr->height = ODD(buttonPtr->reqSize);
    if (buttonPtr->images != NULL) {
	register int i;

	for (i = 0; i < 2; i++) {
	    if (buttonPtr->images[i] == NULL) {
		break;
	    }
	    if (buttonPtr->width < ImageWidth(buttonPtr->images[i])) {
		buttonPtr->width = ImageWidth(buttonPtr->images[i]);
	    }
	    if (buttonPtr->height < ImageHeight(buttonPtr->images[i])) {
		buttonPtr->height = ImageHeight(buttonPtr->images[i]);
	    }
	}
    }
    buttonPtr->width += 2 * buttonPtr->borderWidth;
    buttonPtr->height += 2 * buttonPtr->borderWidth;
}

static void
DestroyEntry(entryPtr)
    Entry *entryPtr;
{
    Hierbox *hboxPtr = entryPtr->hboxPtr;
    register CachedImage *imagePtr;

    Tk_FreeOptions(entryConfigSpecs, (char *)entryPtr, hboxPtr->display, 0);
    if (entryPtr->labelGC != NULL) {
	Tk_FreeGC(hboxPtr->display, entryPtr->labelGC);
    }
    if (entryPtr->dataGC != NULL) {
	Tk_FreeGC(hboxPtr->display, entryPtr->dataGC);
    }
    if (entryPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(&(hboxPtr->nodeTable), entryPtr->hashPtr);
    }
    if (entryPtr->dataShadow.color != NULL) {
	Tk_FreeColor(entryPtr->dataShadow.color);
    }
    if (entryPtr->labelShadow.color != NULL) {
	Tk_FreeColor(entryPtr->labelShadow.color);
    }
    if (entryPtr->iconGC != NULL) {
	Tk_FreeGC(hboxPtr->display, entryPtr->iconGC);
    }
    if (entryPtr->openCmd != NULL) {
	Blt_FreeUid(entryPtr->openCmd);
    }
    if (entryPtr->closeCmd != NULL) {
	Blt_FreeUid(entryPtr->closeCmd);
    }
    if (entryPtr->dataUid != NULL) {
	Blt_FreeUid(entryPtr->dataUid);
    }
    if (entryPtr->dataText != NULL) {
	Blt_FreeUid(entryPtr->dataText);
    }
    if (entryPtr->tags != NULL) {
	Blt_FreeUid(entryPtr->tags);
    }
    if (entryPtr->icons != NULL) {
	for (imagePtr = entryPtr->icons; *imagePtr != NULL; imagePtr++) {
	    FreeCachedImage(hboxPtr, *imagePtr);
	}
	Blt_Free(entryPtr->icons);
    }
    if (entryPtr->activeIcons != NULL) {
	for (imagePtr = entryPtr->activeIcons; *imagePtr != NULL; imagePtr++) {
	    FreeCachedImage(hboxPtr, *imagePtr);
	}
	Blt_Free(entryPtr->activeIcons);
    }
    if (entryPtr->images != NULL) {
	for (imagePtr = entryPtr->images; *imagePtr != NULL; imagePtr++) {
	    FreeCachedImage(hboxPtr, *imagePtr);
	}
	Blt_Free(entryPtr->images);
    }
    Blt_Free(entryPtr);
}

/*ARGSUSED*/
static Tree *
LastNode(treePtr, mask)
    register Tree *treePtr;
    unsigned int mask;
{
    Blt_ChainLink *linkPtr;

    if (treePtr->parentPtr == NULL) {
	return NULL;		/* The root is the first node. */
    }
    linkPtr = Blt_ChainPrevLink(treePtr->linkPtr);
    if (linkPtr == NULL) {
	/* There are no siblings previous to this one, so pick the parent. */
	return treePtr->parentPtr;
    }
    /*
     * Traverse down the right-most thread, in order to select the
     * next entry.  Stop if we find a "closed" entry or reach a leaf.
     */
    treePtr = Blt_ChainGetValue(linkPtr);
    while ((treePtr->entryPtr->flags & mask) == mask) {
	linkPtr = Blt_ChainLastLink(treePtr->chainPtr);
	if (linkPtr == NULL) {
	    break;		/* Found a leaf. */
	}
	treePtr = Blt_ChainGetValue(linkPtr);
    }
    return treePtr;
}


static Tree *
NextNode(treePtr, mask)
    Tree *treePtr;
    unsigned int mask;
{
    Blt_ChainLink *linkPtr;

    if ((treePtr->entryPtr->flags & mask) == mask) {
	/* Pick the first sub-node. */
	linkPtr = Blt_ChainFirstLink(treePtr->chainPtr);
	if (linkPtr != NULL) {
	    return Blt_ChainGetValue(linkPtr);
	}
    }
    /*
     * Back up until we can find a level where we can pick a "next" entry.
     * For the last entry we'll thread our way back to the root.
     */
    while (treePtr->parentPtr != NULL) {
	linkPtr = Blt_ChainNextLink(treePtr->linkPtr);
	if (linkPtr != NULL) {
	    return Blt_ChainGetValue(linkPtr);
	}
	treePtr = treePtr->parentPtr;
    }
    return NULL;		/* At root, no next node. */
}

/*ARGSUSED*/
static Tree *
EndNode(treePtr, mask)
    Tree *treePtr;
    unsigned int mask;
{
    Blt_ChainLink *linkPtr;

    linkPtr = Blt_ChainLastLink(treePtr->chainPtr);
    while (linkPtr != NULL) {
	treePtr = Blt_ChainGetValue(linkPtr);
	if ((treePtr->entryPtr->flags & mask) != mask) {
	    break;
	}
	linkPtr = Blt_ChainLastLink(treePtr->chainPtr);
    }
    return treePtr;
}

static void
ExposeAncestors(treePtr)
    register Tree *treePtr;
{
    treePtr = treePtr->parentPtr;
    while (treePtr != NULL) {
	treePtr->entryPtr->flags |= (ENTRY_OPEN | ENTRY_MAPPED);
	treePtr = treePtr->parentPtr;
    }
}

static int
IsBefore(t1Ptr, t2Ptr)
    register Tree *t1Ptr, *t2Ptr;
{
    int depth;
    register int i;
    Blt_ChainLink *linkPtr;
    Tree *treePtr;

    depth = MIN(t1Ptr->level, t2Ptr->level);

    if (depth == 0) {		/* One of the nodes is root. */
	if (t1Ptr->parentPtr == NULL) {
	    return 1;
	}
	return 0;
    }
    /*
     * Traverse back from the deepest node, until the both nodes are at the
     * same depth.  Check if the ancestor node found is the other node.
     */
    for (i = t1Ptr->level; i > depth; i--) {
	t1Ptr = t1Ptr->parentPtr;
    }
    if (t1Ptr == t2Ptr) {
	return 0;
    }
    for (i = t2Ptr->level; i > depth; i--) {
	t2Ptr = t2Ptr->parentPtr;
    }
    if (t2Ptr == t1Ptr) {
	return 1;
    }
    /*
     * First find the mutual ancestor of both nodes.  Look at each
     * preceding ancestor level-by-level for both nodes.  Eventually
     * we'll find a node that's the parent of both ancestors.  Then
     * find the first ancestor in the parent's list of subnodes.
     */
    for (i = depth; i > 0; i--) {
	if (t1Ptr->parentPtr == t2Ptr->parentPtr) {
	    break;
	}
	t1Ptr = t1Ptr->parentPtr;
	t2Ptr = t2Ptr->parentPtr;
    }
    for (linkPtr = Blt_ChainFirstLink(t1Ptr->parentPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	treePtr = Blt_ChainGetValue(linkPtr);
	if (treePtr == t1Ptr) {
	    return 1;
	} else if (treePtr == t2Ptr) {
	    return 0;
	}
    }
    assert(linkPtr != NULL);
    return 0;
}

static int
IsAncestor(rootPtr, treePtr)
    Tree *rootPtr, *treePtr;
{
    if (treePtr != NULL) {
	treePtr = treePtr->parentPtr;
	while (treePtr != NULL) {
	    if (treePtr == rootPtr) {
		return 1;
	    }
	    treePtr = treePtr->parentPtr;
	}
    }
    return 0;
}

static int
IsHidden(treePtr)
    register Tree *treePtr;
{
    if (treePtr != NULL) {
	unsigned int mask;

	if (!(treePtr->entryPtr->flags & ENTRY_MAPPED)) {
	    return TRUE;
	}
	treePtr = treePtr->parentPtr;
	mask = (ENTRY_OPEN | ENTRY_MAPPED);
	while (treePtr != NULL) {
	    if ((treePtr->entryPtr->flags & mask) != mask) {
		return TRUE;
	    }
	    treePtr = treePtr->parentPtr;
	}
    }
    return FALSE;
}

static void
SelectEntry(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    int isNew;
    Blt_HashEntry *hPtr;

    hPtr = Blt_CreateHashEntry(&(hboxPtr->selectTable), (char *)treePtr, 
       &isNew);
    if (isNew) {
	Blt_ChainLink *linkPtr;

	linkPtr = Blt_ChainAppend(&(hboxPtr->selectChain), treePtr);
	Blt_SetHashValue(hPtr, linkPtr);
    }
}

static void
DeselectEntry(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&(hboxPtr->selectTable), (char *)treePtr);
    if (hPtr != NULL) {
	Blt_ChainLink *linkPtr;

	linkPtr = (Blt_ChainLink *)Blt_GetHashValue(hPtr);
	Blt_ChainDeleteLink(&(hboxPtr->selectChain), linkPtr);
	Blt_DeleteHashEntry(&(hboxPtr->selectTable), hPtr);
    }
}

static void
ClearSelection(hboxPtr)
    Hierbox *hboxPtr;
{
    Blt_DeleteHashTable(&(hboxPtr->selectTable));
    Blt_InitHashTable(&(hboxPtr->selectTable), BLT_ONE_WORD_KEYS);
    Blt_ChainReset(&(hboxPtr->selectChain));
    EventuallyRedraw(hboxPtr);
    if (hboxPtr->selectCmd != NULL) {
	EventuallyInvokeSelectCmd(hboxPtr);
    }
}

static void
PruneSelection(hboxPtr, rootPtr)
    Hierbox *hboxPtr;
    Tree *rootPtr;
{
    Blt_ChainLink *linkPtr, *nextPtr;
    Tree *treePtr;
    int selectionChanged;

    selectionChanged = FALSE;
    for (linkPtr = Blt_ChainFirstLink(&(hboxPtr->selectChain)); 
	 linkPtr != NULL; linkPtr = nextPtr) {
	nextPtr = Blt_ChainNextLink(linkPtr);
	treePtr = Blt_ChainGetValue(linkPtr);
	if (IsAncestor(rootPtr, treePtr)) {
	    DeselectEntry(hboxPtr, treePtr);
	    selectionChanged = TRUE;
	}
    }
    if (selectionChanged) {
	EventuallyRedraw(hboxPtr);
	if (hboxPtr->selectCmd != NULL) {
	    EventuallyInvokeSelectCmd(hboxPtr);
	}
    }
}

static int
IsSelected(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&(hboxPtr->selectTable), (char *)treePtr);
    return (hPtr != NULL);
}


static void
GetFullPath(treePtr, separator, resultPtr)
    Tree *treePtr;
    char *separator;
    Tcl_DString *resultPtr;
{
    char **nameArr;		/* Used the stack the component names. */
    register int i;
    int level;

    level = treePtr->level;
    nameArr = Blt_Malloc((level + 1) * sizeof(char *));
    assert(nameArr);
    for (i = level; i >= 0; i--) {
	/* Save the name of each ancestor in the name array. */
	nameArr[i] = treePtr->nameId;
	treePtr = treePtr->parentPtr;
    }
    Tcl_DStringInit(resultPtr);
    if ((separator == SEPARATOR_LIST) || (separator == SEPARATOR_NONE)) {
	for (i = 0; i <= level; i++) {
	    Tcl_DStringAppendElement(resultPtr, nameArr[i]);
	}
    } else {
	Tcl_DStringAppend(resultPtr, nameArr[0], -1);
	if (strcmp(nameArr[0], separator) != 0) {
	    Tcl_DStringAppend(resultPtr, separator, -1);
	}
	if (level > 0) {
	    for (i = 1; i < level; i++) {
		Tcl_DStringAppend(resultPtr, nameArr[i], -1);
		Tcl_DStringAppend(resultPtr, separator, -1);
	    }
	    Tcl_DStringAppend(resultPtr, nameArr[i], -1);
	}
    }
    Blt_Free(nameArr);
}

static void
InsertNode(parentPtr, position, nodePtr)
    Tree *parentPtr;		/* Parent node where the new node will
				 * be added. */
    int position;		/* Position in the parent of the new node. */
    Tree *nodePtr;		/* The node to be inserted. */
{
    Blt_ChainLink *linkPtr;

    /*
     * Create lists to contain subnodes as needed.  We don't want to
     * unnecessarily allocate storage for leaves (of which there may
     * be many).
     */
    if (parentPtr->chainPtr == NULL) {
	parentPtr->chainPtr = Blt_ChainCreate();
    }
    linkPtr = Blt_ChainNewLink();
    if (position == APPEND) {
	Blt_ChainAppendLink(parentPtr->chainPtr, linkPtr);
    } else {
	Blt_ChainLink *beforePtr;

	beforePtr = Blt_ChainGetNthLink(parentPtr->chainPtr, position);
	Blt_ChainLinkBefore(parentPtr->chainPtr, linkPtr, beforePtr);
    }
    nodePtr->level = parentPtr->level + 1;
    nodePtr->parentPtr = parentPtr;
    nodePtr->linkPtr = linkPtr;
    Blt_ChainSetValue(linkPtr, nodePtr);
}

static void
DestroyNode(data)
    DestroyData data;
{
    Tree *treePtr = (Tree *)data;

    if (treePtr->nameId != NULL) {
	Blt_FreeUid(treePtr->nameId);
    }
    if (treePtr->chainPtr != NULL) {
	Blt_ChainDestroy(treePtr->chainPtr);
    }
    if (treePtr->entryPtr != NULL) {
	DestroyEntry(treePtr->entryPtr);
    }
    treePtr->entryPtr = NULL;
    Blt_Free(treePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CreateNode --
 *
 *	Creates and inserts a new node into the given tree at the
 *	specified position.
 *
 * Results:
 *	Returns a pointer to the newly created node.  If an error
 *	occurred, such as the entry could not be configured, NULL
 *	is returned.
 *
 *----------------------------------------------------------------------
 */
static Tree *
CreateNode(hboxPtr, parentPtr, position, name)
    Hierbox *hboxPtr;		/* Hierarchy widget record */
    Tree *parentPtr;		/* Pointer to parent node. */
    int position;		/* Position in node list to insert node. */
    char *name;			/* Name identifier for the new node. */
{
    Entry *entryPtr;
    Tree *treePtr;
    Blt_HashEntry *hPtr;
    int isNew;
    int serial;

    /* Create the entry structure */
    entryPtr = Blt_Calloc(1, sizeof(Entry));
    assert(entryPtr);
    entryPtr->flags |= (BUTTON_AUTO | ENTRY_MAPPED);
    entryPtr->hboxPtr = hboxPtr;

    if (name == NULL) {
	name = "";
    }
    entryPtr->labelText = Blt_Strdup(name);

    if (ConfigureEntry(hboxPtr, entryPtr, 0, (char **)NULL, 0) != TCL_OK) {
	DestroyEntry(entryPtr);
	return NULL;
    }
    /* Create the container structure too */
    treePtr = Blt_Calloc(1, sizeof(Tree));
    assert(treePtr);
    treePtr->nameId = Blt_GetUid(name);
    treePtr->entryPtr = entryPtr;

    /* Generate a unique node serial number. */
    do {
	serial = hboxPtr->nextSerial++;
	hPtr = Blt_CreateHashEntry(&(hboxPtr->nodeTable), (char *)serial,
	    &isNew);
    } while (!isNew);
    Blt_SetHashValue(hPtr, treePtr);
    entryPtr->hashPtr = hPtr;

    if (parentPtr != NULL) {
	InsertNode(parentPtr, position, treePtr);
    }
    return treePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FindComponent --
 *
 *	Searches for the given child node.  The node is designated
 *	by its node identifier string.
 *
 * Results:
 *	If found, returns a node pointer. Otherwise NULL.
 *
 *----------------------------------------------------------------------
 */
static Tree *
FindComponent(parentPtr, name)
    Tree *parentPtr;
    char *name;
{
    Blt_Uid nameId;

    nameId = Blt_FindUid(name);
    if (nameId != NULL) {
	register Tree *treePtr;
	Blt_ChainLink *linkPtr;

	/* The component identifier must already exist if the node exists. */
	for (linkPtr = Blt_ChainFirstLink(parentPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    treePtr = Blt_ChainGetValue(linkPtr);
	    if (nameId == treePtr->nameId) {
		return treePtr;
	    }
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * SkipSeparators --
 *
 *	Moves the character pointer past one of more separators.
 *
 * Results:
 *	Returns the updates character pointer.
 *
 *----------------------------------------------------------------------
 */
static char *
SkipSeparators(path, separator, length)
    char *path, *separator;
    int length;
{
    while ((*path == separator[0]) && (strncmp(path, separator, length) == 0)) {
	path += length;
    }
    return path;
}

/*
 *----------------------------------------------------------------------
 *
 * SplitPath --
 *
 *	Returns the trailing component of the given path.  Trailing
 *	separators are ignored.
 *
 * Results:
 *	Returns the string of the tail component.
 *
 *----------------------------------------------------------------------
 */
static int
SplitPath(hboxPtr, path, levelPtr, compPtrPtr)
    Hierbox *hboxPtr;
    char *path;
    int *levelPtr;
    char ***compPtrPtr;
{
    int skipLen, pathLen;
    char *sep;
    int level, listSize;
    char **components;
    register char *p;

    skipLen = strlen(hboxPtr->separator);
    path = SkipSeparators(path, hboxPtr->separator, skipLen);
    pathLen = strlen(path);

    level = pathLen / skipLen;
    listSize = (level + 1) * sizeof(char *);
    components = Blt_Malloc(listSize + (pathLen + 1));
    assert(components);
    p = (char *)components + listSize;
    strcpy(p, path);

    sep = strstr(p, hboxPtr->separator);
    level = 0;
    while ((*p != '\0') && (sep != NULL)) {
	*sep = '\0';
	components[level++] = p;
	p = SkipSeparators(sep + skipLen, hboxPtr->separator, skipLen);
	sep = strstr(p, hboxPtr->separator);
    }
    if (*p != '\0') {
	components[level++] = p;
    }
    components[level] = NULL;
    *levelPtr = level;
    *compPtrPtr = components;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FindPath --
 *
 *	Finds the node designated by the given path.  Each path
 *	component is searched for as the tree is traversed.
 *
 *	A leading character string is trimmed off the path if it
 *	matches the one designated (see the -trimleft option).
 *
 *	If no separator is designated (see the -separator
 *	configuration option), the path is considered a Tcl list.
 *	Otherwise the each component of the path is separated by a
 *	character string.  Leading and trailing separators are
 *	ignored.  Multiple separators are treated as one.
 *
 * Results:
 *	Returns the pointer to the designated node.  If any component
 *	can't be found, NULL is returned.
 *
 *----------------------------------------------------------------------
 */
static Tree *
FindPath(hboxPtr, rootPtr, path)
    Hierbox *hboxPtr;
    Tree *rootPtr;
    char *path;
{
    register char *p;
    int skip;
    char *sep;
    Tree *treePtr;
    char save;

    /* Trim off characters that we don't want */
    if (hboxPtr->trimLeft != NULL) {
	register char *s;

	/* Trim off leading character string if one exists. */
	for (p = path, s = hboxPtr->trimLeft; *s != '\0'; s++, p++) {
	    if (*p != *s) {
		break;
	    }
	}
	if (*s == '\0') {
	    path = p;
	}
    }
    if (*path == '\0') {
	return rootPtr;
    }
    if (hboxPtr->separator == SEPARATOR_NONE) { 
	return FindComponent(rootPtr, path);
    } 
    if (hboxPtr->separator == SEPARATOR_LIST) {
	char **nameArr;
	int nComp;
	register int i;

	/*
	 * No separator, so this must be a Tcl list of components.
	 */
	if (Tcl_SplitList(hboxPtr->interp, path, &nComp, &nameArr)
	    != TCL_OK) {
	    return NULL;
	}
	for (i = 0; i < nComp; i++) {
	    treePtr = FindComponent(rootPtr, nameArr[i]);
	    if (treePtr == NULL) {
		Blt_Free(nameArr);
		return NULL;
	    }
	    rootPtr = treePtr;
	}
	Blt_Free(nameArr);
	return rootPtr;
    }
    skip = strlen(hboxPtr->separator);
    path = SkipSeparators(path, hboxPtr->separator, skip);
    sep = strstr(path, hboxPtr->separator);
    p = path;
    treePtr = rootPtr;
    while ((*p != '\0') && (sep != NULL)) {
	save = *sep, *sep = '\0';
	treePtr = FindComponent(treePtr, p);
	*sep = save;
	if (treePtr == NULL) {
	    return NULL;	/* Bad component name. */
	}
	p = SkipSeparators(sep + skip, hboxPtr->separator, skip);
	sep = strstr(p, hboxPtr->separator);
    }
    if (*p != '\0') {
	treePtr = FindComponent(treePtr, p);
	if (treePtr == NULL) {
	    return NULL;
	}
    }
    return treePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * NearestNode --
 *
 *	Finds the entry closest to the given screen X-Y coordinates
 *	in the viewport.
 *
 * Results:
 *	Returns the pointer to the closest node.  If no node is
 *	visible (nodes may be hidden), NULL is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tree *
NearestNode(hboxPtr, x, y, selectOne)
    Hierbox *hboxPtr;
    int x, y;
    int selectOne;
{
    register Tree *lastPtr, **treePtrPtr;
    register Entry *entryPtr;

    /*
     * We implicitly can pick only visible entries.  So make sure that
     * the tree exists.
     */
    if (hboxPtr->nVisible == 0) {
	return NULL;
    }
    /*
     * Since the entry positions were previously computed in world
     * coordinates, convert Y-coordinate from screen to world
     * coordinates too.
     */
    y = WORLDY(hboxPtr, y);
    treePtrPtr = hboxPtr->visibleArr;
    lastPtr = *treePtrPtr;
    for ( /* empty */ ; *treePtrPtr != NULL; treePtrPtr++) {
	entryPtr = (*treePtrPtr)->entryPtr;
	/*
	 * If the start of the next entry starts beyond the point,
	 * use the last entry.
	 */
	if (entryPtr->worldY > y) {
	    return (selectOne) ? lastPtr : NULL;
	}
	if (y < (entryPtr->worldY + entryPtr->height)) {
	    return *treePtrPtr;	/* Found it. */
	}
	lastPtr = *treePtrPtr;
    }
    return (selectOne) ? lastPtr : NULL;
}

static Tree *
GetNodeByIndex(hboxPtr, string)
    Hierbox *hboxPtr;
    char *string;
{
    if (isdigit(UCHAR(string[0]))) {
	int serial;

	if (Tcl_GetInt(NULL, string, &serial) == TCL_OK) {
	    Blt_HashEntry *hPtr;

	    hPtr = Blt_FindHashEntry(&(hboxPtr->nodeTable), (char *)serial);
	    if (hPtr != NULL) {
		return (Tree *) Blt_GetHashValue(hPtr);
	    }
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * NodeToString --
 *
 *	Converts a node pointer in its string representation.  The
 *	string is the node's identifier number.
 *
 * Results:
 *	The string representation of the node is returned.  Note that
 *	the string is stored statically, so that callers must save the
 *	string before the next call to this routine overwrites the
 *	static array again.
 *
 *----------------------------------------------------------------------
 */
static char *
NodeToString(hboxPtr, nodePtr)
    Hierbox *hboxPtr;
    Tree *nodePtr;
{
    static char string[200];
    int serial;

    /* Node table keys are integers.  Convert them to strings. */
    serial = (int)Blt_GetHashKey(&(hboxPtr->nodeTable),
	nodePtr->entryPtr->hashPtr);
    sprintf(string, "%d", serial);

    return string;
}

/*
 *----------------------------------------------------------------------
 *
 * GetNode --
 *
 *	Converts a string into node pointer.  The string may be in one
 *	of the following forms:
 *
 *	    @x,y		- Closest node to the specified X-Y position.
 *	    NNN			- inode.
 *	    "active"		- Currently active node.
 *	    "anchor"		- anchor of selected region.
 *	    "mark"		- mark of selected region.
 *	    "current"		- Currently picked node in bindtable.
 *	    "focus"		- The node currently with focus.
 *	    "root"		- Root node.
 *	    "end"		- Last open node in the entire hierarchy.
 *	    "next"		- Next open node from the currently active
 *				  node. Wraps around back to top.
 *	    "last"		- Previous open node from the currently active
 *				  node. Wraps around back to bottom.
 *	    "up"		- Next open node from the currently active
 *				  node. Does not wrap around.
 *	    "down"		- Previous open node from the currently active
 *				  node. Does not wrap around.
 *	    "nextsibling"	- Next sibling of the current node.
 *	    "prevsibling"	- Previous sibling of the current node.
 *	    "parent"		- Parent of the current node.
 *	    "view.top"		- Top of viewport.
 *	    "view.bottom"	- Bottom of viewport.
 *	    @path		- Absolute path to a node.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	The pointer to the node is returned via treePtrPtr.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */

static int
GetNode(hboxPtr, string, treePtrPtr)
    Hierbox *hboxPtr;
    char *string;
    Tree **treePtrPtr;
{
    Tree *nodePtr;
    char c;
    Tree *fromPtr;

    c = string[0];
    fromPtr = *treePtrPtr;
    nodePtr = NULL;
    if (isdigit(UCHAR(string[0]))) {
	nodePtr = GetNodeByIndex(hboxPtr, string);
    } else if ((c == 'e') && (strcmp(string, "end") == 0)) {
	nodePtr = EndNode(hboxPtr->rootPtr, ENTRY_OPEN | ENTRY_MAPPED);
    } else if ((c == 'a') && (strcmp(string, "anchor") == 0)) {
	nodePtr = hboxPtr->selAnchorPtr;
    } else if ((c == 'f') && (strcmp(string, "focus") == 0)) {
	nodePtr = hboxPtr->focusPtr;
    } else if ((c == 'r') && (strcmp(string, "root") == 0)) {
	nodePtr = hboxPtr->rootPtr;
    } else if ((c == 'p') && (strcmp(string, "parent") == 0)) {
	nodePtr = fromPtr;
	if (nodePtr->parentPtr != NULL) {
	    nodePtr = nodePtr->parentPtr;
	}
    } else if ((c == 'c') && (strcmp(string, "current") == 0)) {
	/* Can't trust picked item, if entries have been added or deleted. */
	if (!(hboxPtr->flags & HIERBOX_DIRTY)) {
	    nodePtr = (Tree *) Blt_GetCurrentItem(hboxPtr->bindTable);
	    if (nodePtr == NULL) {
		nodePtr = (Tree *)Blt_GetCurrentItem(hboxPtr->buttonBindTable);
	    }
	}
    } else if ((c == 'u') && (strcmp(string, "up") == 0)) {
	nodePtr = LastNode(fromPtr, ENTRY_OPEN | ENTRY_MAPPED);
	if (nodePtr == NULL) {
	    nodePtr = fromPtr;
	}
	if ((nodePtr == hboxPtr->rootPtr) && (hboxPtr->hideRoot)) {
	    nodePtr = NextNode(hboxPtr->rootPtr, ENTRY_OPEN | ENTRY_MAPPED);
	}
    } else if ((c == 'd') && (strcmp(string, "down") == 0)) {
	nodePtr = NextNode(fromPtr, ENTRY_OPEN | ENTRY_MAPPED);
	if (nodePtr == NULL) {
	    nodePtr = fromPtr;
	}
	if ((nodePtr == hboxPtr->rootPtr) && (hboxPtr->hideRoot)) {
	    nodePtr = NextNode(hboxPtr->rootPtr, ENTRY_OPEN | ENTRY_MAPPED);
	}
    } else if (((c == 'l') && (strcmp(string, "last") == 0)) ||
	((c == 'p') && (strcmp(string, "prev") == 0))) {
	nodePtr = LastNode(fromPtr, ENTRY_OPEN | ENTRY_MAPPED);
	if (nodePtr == NULL) {
	    nodePtr = EndNode(hboxPtr->rootPtr, ENTRY_OPEN | ENTRY_MAPPED);
	}
	if ((nodePtr == hboxPtr->rootPtr) && (hboxPtr->hideRoot)) {
	    nodePtr = NextNode(hboxPtr->rootPtr, ENTRY_OPEN | ENTRY_MAPPED);
	}
    } else if ((c == 'n') && (strcmp(string, "next") == 0)) {
	nodePtr = NextNode(fromPtr, ENTRY_OPEN | ENTRY_MAPPED);
	if (nodePtr == NULL) {
	    if (hboxPtr->hideRoot) {
		nodePtr = NextNode(hboxPtr->rootPtr, ENTRY_OPEN | ENTRY_MAPPED);
	    } else {
		nodePtr = hboxPtr->rootPtr;
	    }
	}
    } else if ((c == 'n') && (strcmp(string, "nextsibling") == 0)) {
	if (fromPtr->linkPtr != NULL) {
	    Blt_ChainLink *linkPtr;

	    linkPtr = Blt_ChainNextLink(fromPtr->linkPtr);
	    if (linkPtr != NULL) {
		nodePtr = Blt_ChainGetValue(linkPtr);
	    }
	}
    } else if ((c == 'p') && (strcmp(string, "prevsibling") == 0)) {
	if (fromPtr->linkPtr != NULL) {
	    Blt_ChainLink *linkPtr;

	    linkPtr = Blt_ChainPrevLink(fromPtr->linkPtr);
	    if (linkPtr != NULL) {
		nodePtr = Blt_ChainGetValue(linkPtr);
	    }
	}
    } else if ((c == 'v') && (strcmp(string, "view.top") == 0)) {
	if (hboxPtr->nVisible > 0) {
	    nodePtr = hboxPtr->visibleArr[0];
	}
    } else if ((c == 'v') && (strcmp(string, "view.bottom") == 0)) {
	if (hboxPtr->nVisible > 0) {
	    nodePtr = hboxPtr->visibleArr[hboxPtr->nVisible - 1];
	}
    } else if (c == '@') {
	int x, y;

	if (Blt_GetXY(hboxPtr->interp, hboxPtr->tkwin, string, &x, &y)
	    == TCL_OK) {
	    nodePtr = NearestNode(hboxPtr, x, y, TRUE);
	} else {
	    nodePtr = FindPath(hboxPtr, hboxPtr->rootPtr, string + 1);
	}
	if (nodePtr == NULL) {
	    Tcl_ResetResult(hboxPtr->interp);
	    Tcl_AppendResult(hboxPtr->interp, "can't find node entry \"", string,
		"\" in \"", Tk_PathName(hboxPtr->tkwin), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	nodePtr = FindPath(hboxPtr, hboxPtr->rootPtr, string);
	if (nodePtr == NULL) {
	    Tcl_ResetResult(hboxPtr->interp);
	    Tcl_AppendResult(hboxPtr->interp, "can't find node entry \"",
		string, "\" in \"", Tk_PathName(hboxPtr->tkwin), "\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
    }
    *treePtrPtr = nodePtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToNode --
 *
 *	Like GetNode but also finds nodes by serial number.
 *	If the string starts with a digit, it's converted into a
 *	number and then looked-up in a hash table.  This means that
 *	serial identifiers take precedence over node names with
 *	the contain only numbers.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	The pointer to the node is returned via treePtrPtr.
 *	Otherwise, TCL_ERROR is returned and an error message is left
 *	in interpreter's result field.
 *
 *----------------------------------------------------------------------
 */

static int
StringToNode(hboxPtr, string, treePtrPtr)
    Hierbox *hboxPtr;
    char *string;
    Tree **treePtrPtr;
{
    *treePtrPtr = hboxPtr->focusPtr;
    if (GetNode(hboxPtr, string, treePtrPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (*treePtrPtr == NULL) {
	Tcl_ResetResult(hboxPtr->interp);
	Tcl_AppendResult(hboxPtr->interp, "can't find node entry \"", string,
	    "\" in \"", Tk_PathName(hboxPtr->tkwin), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * Preprocess the command string for percent substitution.
 */
static void
PercentSubst(hboxPtr, treePtr, command, resultPtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
    char *command;
    Tcl_DString *resultPtr;
{
    Tcl_DString dString;
    register char *last, *p;

    /*
     * Get the full path name of the node, in case we need to
     * substitute for it.
     */
    GetFullPath(treePtr, hboxPtr->separator, &dString);
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
		string = Tk_PathName(hboxPtr->tkwin);
		break;
	    case 'P':		/* Full pathname */
		string = Tcl_DStringValue(&dString);
		break;
	    case 'p':		/* Name of the node */
		string = treePtr->nameId;
		break;
	    case 'n':		/* Node identifier */
		string = NodeToString(hboxPtr, treePtr);
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
    Tcl_DStringFree(&dString);
}

/*
 *----------------------------------------------------------------------
 *
 * CloseNode --
 *
 *	Closes the node.  If a close Tcl command is specified (see the
 *	"-close" option), it's invoked after the node is closed.
 *
 * Results:
 *	If an error occurred when executing the Tcl proc, TCL_ERROR is
 *	returned.  Otherwise TCL_OK is returned.
 *
 *----------------------------------------------------------------------
 */
static int
CloseNode(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    Entry *entryPtr = treePtr->entryPtr;
    char *command;
    int result;

    /*   
     * The tricky part here is that the "close" command can delete 
     * this node.  So we'll preserve it until after we mark it closed.
     */
    Tcl_Preserve(treePtr);

    /*
     * Invoke the entry's "close" command, if there is one. Otherwise
     * try the hierbox's global "close" command.
     */
    command = (entryPtr->closeCmd != NULL) ? entryPtr->closeCmd :
	hboxPtr->closeCmd;
    result = TCL_OK;
    if ((entryPtr->flags & ENTRY_OPEN) && (command != NULL)) {
	Tcl_DString cmdString;

	PercentSubst(hboxPtr, treePtr, command, &cmdString);
	result = Tcl_GlobalEval(hboxPtr->interp, Tcl_DStringValue(&cmdString));
	Tcl_DStringFree(&cmdString);
    }
    /*
     * Mark the entry closed, only after the Tcl proc callback.
     */
    entryPtr->flags &= ~ENTRY_OPEN;

    Tcl_Release(treePtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * OpenNode --
 *
 *	Opens the node.  If an open Tcl command is specified (see the
 *	"-open" option), it's invoked after the node is opened.
 *
 * Results:
 *	If an error occurred when executing the Tcl proc, TCL_ERROR is
 *	returned.  Otherwise TCL_OK is returned.
 *
 *----------------------------------------------------------------------
 */
static int
OpenNode(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    Entry *entryPtr = treePtr->entryPtr;
    char *string;
    int result;

    /*   
     * The tricky part here is that the "open" command can delete 
     * this node.  So we'll preserve it until after we mark it closed.
     */
    Tcl_Preserve(treePtr);

    /*
     * Invoke the entry's "open" command, if there is one. Otherwise
     * try the hierbox's global "open" command.
     */
    result = TCL_OK;
    string = (entryPtr->openCmd != NULL) ? entryPtr->openCmd : hboxPtr->openCmd;
    if (!(entryPtr->flags & ENTRY_OPEN) && (string != NULL)) {
	Tcl_DString dString;

	PercentSubst(hboxPtr, treePtr, string, &dString);
	result = Tcl_GlobalEval(hboxPtr->interp, Tcl_DStringValue(&dString));
	Tcl_DStringFree(&dString);
    }
    /* 
     * Mark the entry open, only after the Tcl proc callback has run.
     */
    entryPtr->flags |= ENTRY_OPEN;

    Tcl_Release(treePtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectNode --
 *
 *	Sets the selection flag for a node.  The selection flag is
 *	set/cleared/toggled based upon the flag set in the hierarchy
 *	widget.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
static int
SelectNode(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    switch (hboxPtr->flags & SELECTION_MASK) {
    case SELECTION_CLEAR:
	DeselectEntry(hboxPtr, treePtr);
	break;

    case SELECTION_SET:
	SelectEntry(hboxPtr, treePtr);
	break;

    case SELECTION_TOGGLE:
	if (IsSelected(hboxPtr, treePtr)) {
	    DeselectEntry(hboxPtr, treePtr);
	} else {
	    SelectEntry(hboxPtr, treePtr);
	}
	break;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectRange --
 *
 *	Sets the selection flag for a range of nodes.  The range is
 *	determined by two pointers which designate the first/last
 *	nodes of the range.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
static int
SelectRange(hboxPtr, fromPtr, toPtr)
    Hierbox *hboxPtr;
    Tree *fromPtr, *toPtr;
{
    register Tree *treePtr;
    IterProc *proc;

    /* From the range determine the direction to select entries. */
    proc = (IsBefore(toPtr, fromPtr)) ? LastNode : NextNode;
    for (treePtr = fromPtr; treePtr != NULL;
	 treePtr = (*proc)(treePtr, ENTRY_OPEN | ENTRY_MAPPED)) {
	SelectNode(hboxPtr, treePtr);
	if (treePtr == toPtr) {
	    break;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IsSelectedNode --
 *
 *	Adds the name of the node the interpreter result if it is
 *	currently selected.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
static int
IsSelectedNode(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    if (IsSelected(hboxPtr, treePtr)) {
	Tcl_AppendElement(hboxPtr->interp, NodeToString(hboxPtr, treePtr));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetSelectedLabels --
 *
 *	This routine is invoked when the selection is exported.  Each
 *	selected entry is appended line-by-line to a dynamic string
 *	passed via the clientData field of the hierarchy widget.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
static int
GetSelectedLabels(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    if (IsSelected(hboxPtr, treePtr)) {
	Tcl_DString *resultPtr = (Tcl_DString *) hboxPtr->clientData;
	Entry *entryPtr = treePtr->entryPtr;

	Tcl_DStringAppend(resultPtr, entryPtr->labelText, -1);
	Tcl_DStringAppend(resultPtr, "\n", -1);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SizeOfNode --
 *
 *	Returns the number of children at the given node. The sum
 *	is passed via the clientData field of the hierarchy widget.
 *
 * Results:
 *	Always TCL_OK.
 *
 *----------------------------------------------------------------------
 */
static int
SizeOfNode(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    int *sumPtr = (int *)&(hboxPtr->clientData);

    *sumPtr += Blt_ChainGetLength(treePtr->chainPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CompareNodesByName --
 *
 *	Comparison routine (used by qsort) to sort a chain of subnodes.
 *	A simple string comparison is performed on each node name.
 *
 * Results:
 *	1 is the first is greater, -1 is the second is greater, 0
 *	if equal.
 *
 *----------------------------------------------------------------------
 */
static int
CompareNodesByName(link1PtrPtr, link2PtrPtr)
    Blt_ChainLink **link1PtrPtr, **link2PtrPtr;
{
    Tree *t1Ptr, *t2Ptr;

    t1Ptr = Blt_ChainGetValue(*link1PtrPtr);
    t2Ptr = Blt_ChainGetValue(*link2PtrPtr);
    return strcmp(t1Ptr->nameId, t2Ptr->nameId);
}

/*
 *----------------------------------------------------------------------
 *
 * CompareNodesByTclCmd --
 *
 *	Comparison routine (used by qsort) to sort a list of subnodes.
 *	A specified Tcl proc is invoked to compare the nodes.
 *
 * Results:
 *	1 is the first is greater, -1 is the second is greater, 0
 *	if equal.
 *
 *----------------------------------------------------------------------
 */
static int
CompareNodesByTclCmd(link1PtrPtr, link2PtrPtr)
    Blt_ChainLink **link1PtrPtr, **link2PtrPtr;
{
    int result;
    Tree *t1Ptr, *t2Ptr;
    Hierbox *hboxPtr = hierBox;
    Tcl_Interp *interp = hboxPtr->interp;

    t1Ptr = Blt_ChainGetValue(*link1PtrPtr);
    t2Ptr = Blt_ChainGetValue(*link2PtrPtr);
    result = 0;			/* Hopefully this will be Ok even if the
				 * Tcl command fails to return the correct
				 * result. */
    if ((Tcl_VarEval(interp, hboxPtr->sortCmd, " ",
	Tk_PathName(hboxPtr->tkwin), " ", NodeToString(hboxPtr, t1Ptr), " ",
	NodeToString(hboxPtr, t2Ptr)) != TCL_OK) ||
	(Tcl_GetInt(interp, Tcl_GetStringResult(interp), &result) != TCL_OK)) {
	Tcl_BackgroundError(interp);
    }
    Tcl_ResetResult(interp);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * SortNode --
 *
 *	Sorts the subnodes at a given node.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
static int
SortNode(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    if (treePtr->chainPtr != NULL) {
	if (hboxPtr->sortCmd != NULL) {
	    hierBox = hboxPtr;
	    Blt_ChainSort(treePtr->chainPtr, CompareNodesByTclCmd);
	} else {
	    Blt_ChainSort(treePtr->chainPtr, CompareNodesByName);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UnmapNode --
 *
 *	Unmaps the given node.  The node will not be drawn.  Ignore
 *	unmapping of the root node (it makes no sense).
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
static int
UnmapNode(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    if (treePtr != hboxPtr->rootPtr) {
	treePtr->entryPtr->flags &= ~ENTRY_MAPPED;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MapAncestors --
 *
 *	If a node in mapped, then all its ancestors must be mapped also.
 *	This routine traverses upwards and maps each unmapped ancestor.
 *	It's assumed that for any mapped ancestor, all it's ancestors
 *	will already be mapped too.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
MapAncestors(hboxPtr, treePtr)
    Hierbox *hboxPtr;		/* Not used. */
    Tree *treePtr;
{
    /*
     * Make sure that all the ancestors of this node are mapped too.
     */
    treePtr = treePtr->parentPtr;
    while (treePtr != NULL) {
	if (treePtr->entryPtr->flags & ENTRY_MAPPED) {
	    break;		/* Assume ancestors are also mapped. */
	}
	treePtr->entryPtr->flags |= ENTRY_MAPPED;
	treePtr = treePtr->parentPtr;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MapNode --
 *
 *	Maps the given node.  Only mapped nodes are drawn.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *----------------------------------------------------------------------
 */
static int
MapNode(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    treePtr->entryPtr->flags |= ENTRY_MAPPED;
    MapAncestors(hboxPtr, treePtr);
    return TCL_OK;
}

static int
FixUnmappedSelections(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    if (!(treePtr->entryPtr->flags & ENTRY_MAPPED)) {
	DeselectEntry(hboxPtr, treePtr);
	PruneSelection(hboxPtr, treePtr);
	if (IsAncestor(treePtr, hboxPtr->focusPtr)) {
	    hboxPtr->focusPtr = treePtr->parentPtr;
	    if (hboxPtr->focusPtr == NULL) {
		hboxPtr->focusPtr = hboxPtr->rootPtr;
	    }
	    Blt_SetFocusItem(hboxPtr->bindTable, hboxPtr->focusPtr, NULL);
	}
    }
    return TCL_OK;
}

static int
DeleteNode(hboxPtr, treePtr)
    Hierbox *hboxPtr;		
    Tree *treePtr;
{
    /*
     * Indicate that the screen layout of the hierarchy may have changed
     * because the node was deleted.  We don't want to access the
     * hboxPtr->visibleArr array if one of the nodes is bogus.
     */
    hboxPtr->flags |= HIERBOX_DIRTY;
    if (treePtr == hboxPtr->activePtr) {
	hboxPtr->activePtr = treePtr->parentPtr;
    }
    if (treePtr == hboxPtr->activeButtonPtr) {
	hboxPtr->activeButtonPtr = NULL;
    }
    if (treePtr == hboxPtr->focusPtr) {
	hboxPtr->focusPtr = treePtr->parentPtr;
	Blt_SetFocusItem(hboxPtr->bindTable, hboxPtr->focusPtr, NULL);
    }
    if (treePtr == hboxPtr->selAnchorPtr) {
	hboxPtr->selAnchorPtr = NULL;
    }
    DeselectEntry(hboxPtr, treePtr);
    PruneSelection(hboxPtr, treePtr);
    if (treePtr->linkPtr != NULL) { /* Remove from parent's list */
	Blt_ChainDeleteLink(treePtr->parentPtr->chainPtr, treePtr->linkPtr);
	treePtr->linkPtr = NULL;
    }
    /* 
     * Node may still be in use, so we can't free it right now.  We'll
     * mark the parent as NULL and remove it from the parent's list of
     * children. 
     */
    treePtr->parentPtr = NULL;	
    Blt_DeleteBindings(hboxPtr->bindTable, treePtr);
    Blt_DeleteBindings(hboxPtr->buttonBindTable, treePtr);
    Tcl_EventuallyFree(treePtr, DestroyNode);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * DestroyTree --
 *
 *	Recursively deletes the given node and all its subnodes.
 *
 * Results:
 *	If successful, returns TCL_OK.  Otherwise TCL_ERROR is
 *	returned.
 *
 *----------------------------------------------------------------------
 */
static int
DestroyTree(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    return ApplyToTree(hboxPtr, treePtr, DeleteNode, APPLY_RECURSE);
}

/*ARGSUSED*/
static void
GetTags(table, object, context, list)
    Blt_BindTable table;	/* Not used. */
    ClientData object;
    ClientData context;		/* Not used. */
    Blt_List list;
{
    Tree *treePtr;

    Blt_ListAppend(list, (char *)object, 0);
    treePtr = (Tree *) object;
    if (treePtr->entryPtr->tags != NULL) {
	int nNames;
	char **names;
	register char **p;

	if (Tcl_SplitList((Tcl_Interp *)NULL, treePtr->entryPtr->tags, &nNames,
		&names) == TCL_OK) {
	    for (p = names; *p != NULL; p++) {
		Blt_ListAppend(list, Tk_GetUid(*p), 0);
	    }
	    Blt_Free(names);
	}
    }
}

/*ARGSUSED*/
static ClientData
PickButton(clientData, x, y, contextPtr)
    ClientData clientData;
    int x, y;			/* Screen coordinates of the test point. */
    ClientData *contextPtr;	/* Not used. */
{
    Hierbox *hboxPtr = clientData;
    Entry *entryPtr;
    Tree *treePtr;

    if (hboxPtr->flags & HIERBOX_DIRTY) {
	/* Can't trust selected entry, if entries have been added or deleted. */
	if (hboxPtr->flags & HIERBOX_LAYOUT) {
	    ComputeLayout(hboxPtr);
	}
	ComputeVisibleEntries(hboxPtr);
    }
    if (hboxPtr->nVisible == 0) {
	return (ClientData) 0;
    }
    treePtr = NearestNode(hboxPtr, x, y, FALSE);
    if (treePtr == NULL) {
	return (ClientData) 0;
    }
    entryPtr = treePtr->entryPtr;
    if (entryPtr->flags & ENTRY_BUTTON) {
	ButtonAttributes *buttonPtr = &(hboxPtr->button);
	int left, right, top, bottom;
#define BUTTON_PAD 2
	left = entryPtr->worldX + entryPtr->buttonX - BUTTON_PAD;
	right = left + buttonPtr->width + 2 * BUTTON_PAD;
	top = entryPtr->worldY + entryPtr->buttonY - BUTTON_PAD;
	bottom = top + buttonPtr->height + 2 * BUTTON_PAD;
	x = WORLDX(hboxPtr, x);
	y = WORLDY(hboxPtr, y);
	if ((x >= left) && (x < right) && (y >= top) && (y < bottom)) {
	    return treePtr;
	}
    }
    return (ClientData) 0;
}

/*ARGSUSED*/
static ClientData
PickEntry(clientData, x, y, contextPtr)
    ClientData clientData;
    int x, y;			/* Screen coordinates of the test point. */
    ClientData *contextPtr;	/* Not used. */
{
    Hierbox *hboxPtr = clientData;
    Entry *entryPtr;
    Tree *treePtr;

    if (hboxPtr->flags & HIERBOX_DIRTY) {
	/* Can't trust selected entry, if entries have been added or deleted. */
	if (hboxPtr->flags & HIERBOX_LAYOUT) {
	    ComputeLayout(hboxPtr);
	}
	ComputeVisibleEntries(hboxPtr);
    }
    if (hboxPtr->nVisible == 0) {
	return (ClientData) 0;
    }
    treePtr = NearestNode(hboxPtr, x, y, FALSE);
    if (treePtr == NULL) {
	return (ClientData) 0;
    }
    entryPtr = treePtr->entryPtr;
    if (entryPtr->flags & ENTRY_BUTTON) {
	ButtonAttributes *buttonPtr = &(hboxPtr->button);
	int left, right, top, bottom;
#define BUTTON_PAD 2
	left = entryPtr->worldX + entryPtr->buttonX - BUTTON_PAD;
	right = left + buttonPtr->width + 2 * BUTTON_PAD;
	top = entryPtr->worldY + entryPtr->buttonY - BUTTON_PAD;
	bottom = top + buttonPtr->height + 2 * BUTTON_PAD;
	x = WORLDX(hboxPtr, x);
	y = WORLDY(hboxPtr, y);
	if ((x >= left) && (x < right) && (y >= top) && (y < bottom)) {
	    return NULL;
	}
    }
    return treePtr;
}

static int
ConfigureEntry(hboxPtr, entryPtr, argc, argv, flags)
    Hierbox *hboxPtr;
    Entry *entryPtr;
    int argc;
    char **argv;
    int flags;
{
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;
    int entryWidth, entryHeight;
    int width, height;
    Tk_Font font;
    XColor *colorPtr;

    hierBox = hboxPtr;
    if (Tk_ConfigureWidget(hboxPtr->interp, hboxPtr->tkwin, entryConfigSpecs,
	    argc, argv, (char *)entryPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    entryPtr->iconWidth = entryPtr->iconHeight = 0;
    if (entryPtr->icons != NULL) {
	register int i;

	for (i = 0; i < 2; i++) {
	    if (entryPtr->icons[i] == NULL) {
		break;
	    }
	    if (entryPtr->iconWidth < ImageWidth(entryPtr->icons[i])) {
		entryPtr->iconWidth = ImageWidth(entryPtr->icons[i]);
	    }
	    if (entryPtr->iconHeight < ImageHeight(entryPtr->icons[i])) {
		entryPtr->iconHeight = ImageHeight(entryPtr->icons[i]);
	    }
	}
    }
    newGC = NULL;
    if ((entryPtr->icons == NULL) || (entryPtr->icons[0] == NULL)) {
	gcMask = GCClipMask | GCBackground;
	gcValues.clip_mask = hboxPtr->iconMask;
	gcValues.background = hboxPtr->iconColor->pixel;
	newGC = Tk_GetGC(hboxPtr->tkwin, gcMask, &gcValues);
	entryPtr->iconWidth = DEF_ICON_WIDTH;
	entryPtr->iconHeight = DEF_ICON_HEIGHT;
    }
    entryPtr->iconWidth += 2 * ICON_PADX;
    entryPtr->iconHeight += 2 * ICON_PADY;
    if (entryPtr->iconGC != NULL) {
	Tk_FreeGC(hboxPtr->display, entryPtr->iconGC);
    }
    entryPtr->iconGC = newGC;

    entryHeight = MAX(entryPtr->iconHeight, hboxPtr->button.height);
    entryWidth = 0;

    gcMask = GCForeground | GCFont;
    colorPtr = GETCOLOR(hboxPtr, entryPtr->labelColor);
    gcValues.foreground = colorPtr->pixel;
    font = GETFONT(hboxPtr, entryPtr->labelFont);
    gcValues.font = Tk_FontId(font);
    newGC = Tk_GetGC(hboxPtr->tkwin, gcMask, &gcValues);
    if (entryPtr->labelGC != NULL) {
	Tk_FreeGC(hboxPtr->display, entryPtr->labelGC);
    }
    entryPtr->labelGC = newGC;

    if (*entryPtr->labelText == '\0') {
	Tk_FontMetrics fontMetrics;

	Tk_GetFontMetrics(font, &fontMetrics);
	width = height = fontMetrics.linespace;
    } else {
	TextStyle ts;

	Blt_InitTextStyle(&ts);
	ts.shadow.offset = entryPtr->labelShadow.offset;
	ts.font = font;
	Blt_GetTextExtents(&ts, entryPtr->labelText, &width, &height);
    }
    width += 2 * (FOCUS_WIDTH + LABEL_PADX + hboxPtr->selBorderWidth);
    height += 2 * (FOCUS_WIDTH + LABEL_PADY + hboxPtr->selBorderWidth);
    width = ODD(width);
    height = ODD(height);
    entryWidth += width;
    if (entryHeight < height) {
	entryHeight = height;
    }
    entryPtr->labelWidth = width;
    entryPtr->labelHeight = height;
    width = height = 0;
    if (entryPtr->images != NULL) {
	register CachedImage *imagePtr;

	for (imagePtr = entryPtr->images; *imagePtr != NULL; imagePtr++) {
	    width += ImageWidth(*imagePtr);
	    if (height < ImageHeight(*imagePtr)) {
		height = ImageHeight(*imagePtr);
	    }
	}
    } else if (entryPtr->dataText != NULL) {
	TextStyle ts;

	gcMask = GCForeground | GCFont;
	colorPtr = GETCOLOR(hboxPtr, entryPtr->dataColor);
	gcValues.foreground = colorPtr->pixel;
	font = GETFONT(hboxPtr, entryPtr->dataFont);
	gcValues.font = Tk_FontId(font);
	newGC = Tk_GetGC(hboxPtr->tkwin, gcMask, &gcValues);
	if (entryPtr->dataGC != NULL) {
	    Tk_FreeGC(hboxPtr->display, entryPtr->dataGC);
	}
	entryPtr->dataGC = newGC;

	Blt_InitTextStyle(&ts);
	ts.font = font;
	ts.shadow.offset = entryPtr->dataShadow.offset;
	Blt_GetTextExtents(&ts, entryPtr->dataText, &width, &height);
	width += 2 * LABEL_PADX;
	height += 2 * LABEL_PADY;
    }
    entryWidth += width;
    if (entryHeight < height) {
	entryHeight = height;
    }
    entryPtr->width = entryWidth + 4;
    entryPtr->height = entryHeight + hboxPtr->leader;

    /*
     * Force the height of the entry to an even number. This is to
     * make the dots or the vertical line segments coincide with the
     * start of the horizontal lines.
     */
    if (entryPtr->height & 0x01) {
	entryPtr->height++;
    }
    hboxPtr->flags |= HIERBOX_LAYOUT;
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*
 * Hierbox Procedures
 */
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
EventuallyRedraw(hboxPtr)
    Hierbox *hboxPtr;
{
    if ((hboxPtr->tkwin != NULL) && !(hboxPtr->flags & HIERBOX_REDRAW)) {
	hboxPtr->flags |= HIERBOX_REDRAW;
	Tcl_DoWhenIdle(DisplayHierbox, hboxPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * LabelBlinkProc --
 *
 *	This procedure is called as a timer handler to blink the
 *	insertion cursor off and on.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor gets turned on or off, redisplay gets invoked,
 *	and this procedure reschedules itself.
 *
 *----------------------------------------------------------------------
 */

static void
LabelBlinkProc(clientData)
    ClientData clientData;	/* Pointer to record describing entry. */
{
    Hierbox *hboxPtr = clientData;
    TextEdit *editPtr = &(hboxPtr->labelEdit);
    int interval;

    if (!(hboxPtr->flags & HIERBOX_FOCUS) || (editPtr->offTime == 0)) {
	return;
    }
    if (editPtr->active) {
	editPtr->cursorOn ^= 1;
	interval = (editPtr->cursorOn) ? editPtr->onTime : editPtr->offTime;
	editPtr->timerToken = Tcl_CreateTimerHandler(interval, LabelBlinkProc,
	     hboxPtr);
	EventuallyRedraw(hboxPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * EventuallyInvokeSelectCmd --
 *
 *      Queues a request to execute the -selectcommand code associated
 *      with the widget at the next idle point.  Invoked whenever the
 *      selection changes.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Tcl code gets executed for some application-specific task.
 *
 *----------------------------------------------------------------------
 */
static void
EventuallyInvokeSelectCmd(hboxPtr)
    Hierbox *hboxPtr;
{
    if (!(hboxPtr->flags & SELECTION_PENDING)) {
	hboxPtr->flags |= SELECTION_PENDING;
	Tcl_DoWhenIdle(SelectCmdProc, hboxPtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * CreateHierbox --
 *
 * ----------------------------------------------------------------------
 */
static Hierbox *
CreateHierbox(interp, tkwin)
    Tcl_Interp *interp;
    Tk_Window tkwin;
{
    Hierbox *hboxPtr;

    hboxPtr = Blt_Calloc(1, sizeof(Hierbox));
    assert(hboxPtr);

    Tk_SetClass(tkwin, "Hierbox");
    hboxPtr->tkwin = tkwin;
    hboxPtr->display = Tk_Display(tkwin);
    hboxPtr->interp = interp;

    hboxPtr->leader = 0;
    hboxPtr->dashes = 1;
    hboxPtr->highlightWidth = 2;
    hboxPtr->selBorderWidth = 1;
    hboxPtr->borderWidth = 2;
    hboxPtr->relief = TK_RELIEF_SUNKEN;
    hboxPtr->selRelief = TK_RELIEF_FLAT;
    hboxPtr->scrollMode = BLT_SCROLL_MODE_HIERBOX;
    hboxPtr->button.closeRelief = hboxPtr->button.openRelief = TK_RELIEF_SOLID;
    hboxPtr->reqWidth = 200;
    hboxPtr->reqHeight = 400;
    hboxPtr->lineWidth = 1;
    hboxPtr->button.borderWidth = 1;
    hboxPtr->labelEdit.selAnchor = -1;
    hboxPtr->labelEdit.selFirst = hboxPtr->labelEdit.selLast = -1;
    hboxPtr->labelEdit.onTime = 600, hboxPtr->labelEdit.offTime = 300;
    Blt_ChainInit(&(hboxPtr->selectChain));
    Blt_InitHashTable(&(hboxPtr->selectTable), BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&(hboxPtr->nodeTable), BLT_ONE_WORD_KEYS);
    Blt_InitHashTable(&(hboxPtr->imageTable), BLT_STRING_KEYS);
    hboxPtr->bindTable = Blt_CreateBindingTable(interp, tkwin, hboxPtr, 
	PickEntry, GetTags);
    hboxPtr->buttonBindTable = Blt_CreateBindingTable(interp, tkwin, hboxPtr, 
	PickButton, GetTags);
#if (TK_MAJOR_VERSION > 4)
    Blt_SetWindowInstanceData(tkwin, hboxPtr);
#endif
    return hboxPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyHierbox --
 *
 * 	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a Hierbox at a safe time
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
DestroyHierbox(dataPtr)
    DestroyData dataPtr;	/* Pointer to the widget record. */
{
    Hierbox *hboxPtr = (Hierbox *)dataPtr;
    ButtonAttributes *buttonPtr = &(hboxPtr->button);

    Tk_FreeOptions(configSpecs, (char *)hboxPtr, hboxPtr->display, 0);
    if (hboxPtr->tkwin != NULL) {
	Tk_DeleteSelHandler(hboxPtr->tkwin, XA_PRIMARY, XA_STRING);
    }
    if (hboxPtr->lineGC != NULL) {
	Tk_FreeGC(hboxPtr->display, hboxPtr->lineGC);
    }
    if (hboxPtr->focusGC != NULL) {
	Blt_FreePrivateGC(hboxPtr->display, hboxPtr->focusGC);
    }
    if (hboxPtr->tile != NULL) {
	Blt_FreeTile(hboxPtr->tile);
    }
    if (hboxPtr->visibleArr != NULL) {
	Blt_Free(hboxPtr->visibleArr);
    }
    if (hboxPtr->levelInfo != NULL) {
	Blt_Free(hboxPtr->levelInfo);
    }
    if (hboxPtr->iconBitmap != None) {
	Tk_FreeBitmap(hboxPtr->display, hboxPtr->iconBitmap);
    }
    if (hboxPtr->iconMask != None) {
	Tk_FreeBitmap(hboxPtr->display, hboxPtr->iconMask);
    }
    if (hboxPtr->iconColor != NULL) {
	Tk_FreeColor(hboxPtr->iconColor);
    }
    if (buttonPtr->images != NULL) {
	register CachedImage *imagePtr;

	for (imagePtr = buttonPtr->images; *imagePtr != NULL; imagePtr++) {
	    FreeCachedImage(hboxPtr, *imagePtr);
	}
	Blt_Free(buttonPtr->images);
    }
    if (buttonPtr->activeGC != NULL) {
	Tk_FreeGC(hboxPtr->display, buttonPtr->activeGC);
    }
    if (buttonPtr->normalGC != NULL) {
	Tk_FreeGC(hboxPtr->display, buttonPtr->normalGC);
    }
    if (buttonPtr->lineGC != NULL) {
	Tk_FreeGC(hboxPtr->display, buttonPtr->lineGC);
    }
    DestroyTree(hboxPtr, hboxPtr->rootPtr);
    Blt_DeleteHashTable(&(hboxPtr->nodeTable));
    Blt_ChainReset(&(hboxPtr->selectChain));
    Blt_DeleteHashTable(&(hboxPtr->selectTable));
    Blt_DestroyBindingTable(hboxPtr->bindTable);
    Blt_DestroyBindingTable(hboxPtr->buttonBindTable);
    Blt_Free(hboxPtr);
}

/*
 * --------------------------------------------------------------
 *
 * HierboxEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on hierarchy widgets.
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
HierboxEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Hierbox *hboxPtr = clientData;

    if (eventPtr->type == Expose) {
	if (eventPtr->xexpose.count == 0) {
	    EventuallyRedraw(hboxPtr);
	}
    } else if (eventPtr->type == ConfigureNotify) {
	hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
	EventuallyRedraw(hboxPtr);
    } else if ((eventPtr->type == FocusIn) || (eventPtr->type == FocusOut)) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    TextEdit *editPtr = &(hboxPtr->labelEdit);

	    if (eventPtr->type == FocusIn) {
		hboxPtr->flags |= HIERBOX_FOCUS;
	    } else {
		hboxPtr->flags &= ~HIERBOX_FOCUS;
	    }
	    Tcl_DeleteTimerHandler(editPtr->timerToken);
	    if ((editPtr->active) && (hboxPtr->flags & HIERBOX_FOCUS)) {
		editPtr->cursorOn = TRUE;
		if (editPtr->offTime != 0) {
		    editPtr->timerToken = 
			Tcl_CreateTimerHandler(editPtr->onTime,
			LabelBlinkProc, clientData);
		}
	    } else {
		editPtr->cursorOn = FALSE;
		editPtr->timerToken = (Tcl_TimerToken) NULL;
	    }
	    EventuallyRedraw(hboxPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (hboxPtr->tkwin != NULL) {
	    hboxPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(hboxPtr->interp, hboxPtr->cmdToken);
	}
	if (hboxPtr->flags & HIERBOX_REDRAW) {
	    Tcl_CancelIdleCall(DisplayHierbox, hboxPtr);
	}
	if (hboxPtr->flags & SELECTION_PENDING) {
	    Tcl_CancelIdleCall(SelectCmdProc, hboxPtr);
	}
	Tcl_EventuallyFree(hboxPtr, DestroyHierbox);
    }
}

/* Selection Procedures */
/*
 *----------------------------------------------------------------------
 *
 * SelectionProc --
 *
 *	This procedure is called back by Tk when the selection is
 *	requested by someone.  It returns part or all of the selection
 *	in a buffer provided by the caller.
 *
 * Results:
 *	The return value is the number of non-NULL bytes stored at
 *	buffer.  Buffer is filled (or partially filled) with a
 *	NUL-terminated string containing part or all of the
 *	selection, as given by offset and maxBytes.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static int
SelectionProc(clientData, offset, buffer, maxBytes)
    ClientData clientData;	/* Information about the widget. */
    int offset;			/* Offset within selection of first
				 * character to be returned. */
    char *buffer;		/* Location in which to place
				 * selection. */
    int maxBytes;		/* Maximum number of bytes to place
				 * at buffer, not including terminating
				 * NULL character. */
{
    Hierbox *hboxPtr = clientData;
    int size;
    Tcl_DString dString;

    if (!hboxPtr->exportSelection) {
	return -1;
    }
    /*
     * Retrieve the names of the selected entries.
     */
    Tcl_DStringInit(&dString);
    if (hboxPtr->sortSelection) {
	hboxPtr->clientData = &dString;
	ApplyToTree(hboxPtr, hboxPtr->rootPtr, GetSelectedLabels,
		    APPLY_RECURSE | APPLY_BEFORE | APPLY_OPEN_ONLY);
    } else {
	Blt_ChainLink *linkPtr;
	Tree *treePtr;

	for (linkPtr = Blt_ChainFirstLink(&(hboxPtr->selectChain)); 
	     linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	    treePtr = Blt_ChainGetValue(linkPtr);
	    Tcl_DStringAppend(&dString, treePtr->entryPtr->labelText, -1);
	    Tcl_DStringAppend(&dString, "\n", -1);
	}
    }
    size = Tcl_DStringLength(&dString) - offset;
    strncpy(buffer, Tcl_DStringValue(&dString) + offset, maxBytes);
    Tcl_DStringFree(&dString);
    buffer[maxBytes] = '\0';
    return (size > maxBytes) ? maxBytes : size;
}

/*
 *----------------------------------------------------------------------
 *
 * LostSelection --
 *
 *	This procedure is called back by Tk when the selection is grabbed
 *	away.
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
LostSelection(clientData)
    ClientData clientData;	/* Information about the widget. */
{
    Hierbox *hboxPtr = clientData;

    if ((hboxPtr->selAnchorPtr != NULL) && (hboxPtr->exportSelection)) {
	ClearSelection(hboxPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * HierboxInstCmdDeleteProc --
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
HierboxInstCmdDeleteProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Hierbox *hboxPtr = clientData;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this
     * procedure destroys the widget.
     */
    if (hboxPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = hboxPtr->tkwin;
	hboxPtr->tkwin = NULL;
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
    Hierbox *hboxPtr = clientData;

    if (hboxPtr->tkwin != NULL) {
	EventuallyRedraw(hboxPtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureHierbox --
 *
 * 	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	the widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 * 	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for hboxPtr; old resources get freed, if there
 *	were any.  The widget is redisplayed.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureHierbox(interp, hboxPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Interpreter to report errors back to. */
    Hierbox *hboxPtr;		/* Information about widget; may or may not
			         * already have values for some fields. */
    int argc;
    char **argv;
    int flags;
{
    XGCValues gcValues;
    unsigned long gcMask;
    GC newGC;
    Tk_Uid nameId;
    Pixmap bitmap;
    hierBox = hboxPtr;
    if (Tk_ConfigureWidget(interp, hboxPtr->tkwin, configSpecs, argc, argv,
	    (char *)hboxPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_ConfigModified(configSpecs, "-font", "-linespacing", "-width",
	    "-height", "-hideroot", (char *)NULL)) {
	/*
	 * These options change the layout of the box.  Mark for
	 * update.
	 */
	hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
    }
    if ((hboxPtr->reqHeight != Tk_ReqHeight(hboxPtr->tkwin)) ||
	(hboxPtr->reqWidth != Tk_ReqWidth(hboxPtr->tkwin))) {
	Tk_GeometryRequest(hboxPtr->tkwin, hboxPtr->reqWidth,
	    hboxPtr->reqHeight);
    }
    gcMask = (GCForeground | GCLineWidth);
    gcValues.foreground = hboxPtr->lineColor->pixel;
    gcValues.line_width = hboxPtr->lineWidth;
    if (hboxPtr->dashes > 0) {
	gcMask |= (GCLineStyle | GCDashList);
	gcValues.line_style = LineOnOffDash;
	gcValues.dashes = hboxPtr->dashes;
    }
    newGC = Tk_GetGC(hboxPtr->tkwin, gcMask, &gcValues);
    if (hboxPtr->lineGC != NULL) {
	Tk_FreeGC(hboxPtr->display, hboxPtr->lineGC);
    }
    hboxPtr->lineGC = newGC;

    /*
     * GC for active label. Dashed outline.
     */
    gcMask = GCForeground | GCLineStyle;
    gcValues.foreground = hboxPtr->focusColor->pixel;
    gcValues.line_style = (LineIsDashed(hboxPtr->focusDashes))
	? LineOnOffDash : LineSolid;
    newGC = Blt_GetPrivateGC(hboxPtr->tkwin, gcMask, &gcValues);
    if (LineIsDashed(hboxPtr->focusDashes)) {
	hboxPtr->focusDashes.offset = 2;
	Blt_SetDashes(hboxPtr->display, newGC, &(hboxPtr->focusDashes));
    }
    if (hboxPtr->focusGC != NULL) {
	Blt_FreePrivateGC(hboxPtr->display, hboxPtr->focusGC);
    }
    hboxPtr->focusGC = newGC;

    if (hboxPtr->iconBitmap == None) {
	nameId = Tk_GetUid("HierboxFolder");
	bitmap = Tk_GetBitmap(interp, hboxPtr->tkwin, nameId);
	if (bitmap == None) {
	    if (Tk_DefineBitmap(interp, nameId, (char *)folderBits,
		    DEF_ICON_WIDTH, DEF_ICON_HEIGHT) != TCL_OK) {
		return TCL_ERROR;
	    }
	    bitmap = Tk_GetBitmap(interp, hboxPtr->tkwin, nameId);
	    if (bitmap == None) {
		return TCL_ERROR;
	    }
	}
	hboxPtr->iconBitmap = bitmap;
	Tcl_ResetResult(interp);
    }
    if (hboxPtr->iconMask == None) {
	nameId = Tk_GetUid("HierboxFolderMask");
	bitmap = Tk_GetBitmap(interp, hboxPtr->tkwin, nameId);
	if (bitmap == None) {
	    if (Tk_DefineBitmap(interp, nameId, (char *)folderMaskBits,
		    DEF_ICON_WIDTH, DEF_ICON_HEIGHT) != TCL_OK) {
		return TCL_ERROR;
	    }
	    bitmap = Tk_GetBitmap(interp, hboxPtr->tkwin, nameId);
	    if (bitmap == None) {
		return TCL_ERROR;
	    }
	}
	hboxPtr->iconMask = bitmap;
	Tcl_ResetResult(interp);
    }
    if (hboxPtr->iconColor == NULL) {
	hboxPtr->iconColor = Tk_GetColor(interp, hboxPtr->tkwin,
	    Tk_GetUid("yellow"));
	if (hboxPtr->iconColor == NULL) {
	    return TCL_ERROR;
	}
    }
    if (hboxPtr->tile != NULL) {
	Blt_SetTileChangedProc(hboxPtr->tile, TileChangedProc, hboxPtr);
    }
    ConfigureButtons(hboxPtr);

    hboxPtr->inset = hboxPtr->highlightWidth + hboxPtr->borderWidth + INSET_PAD;
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ResetCoordinates --
 *
 *	Determines the maximum height of all visible entries.
 *
 * Results:
 *	Returns 1 if beyond the last visible entry, 0 otherwise.
 *
 * Side effects:
 *	The array of visible nodes is filled.
 *
 * ----------------------------------------------------------------------
 */
static void
ResetCoordinates(hboxPtr, treePtr, infoPtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
    LayoutInfo *infoPtr;
{
    Entry *entryPtr = treePtr->entryPtr;
    int width;

    /*
     * If the entry is hidden, then do nothing.
     * Otherwise, include it in the layout.
     */
    entryPtr->worldY = infoPtr->y;
    if (!(entryPtr->flags & ENTRY_MAPPED)) {
	return;
    }
    treePtr->level = infoPtr->level;
    if (infoPtr->depth < infoPtr->level) {
	infoPtr->depth = infoPtr->level;
    }
    if ((entryPtr->flags & BUTTON_SHOW) || ((entryPtr->flags & BUTTON_AUTO) &&
	    (Blt_ChainGetLength(treePtr->chainPtr) > 0))) {
	entryPtr->flags |= ENTRY_BUTTON;
    } else {
	entryPtr->flags &= ~ENTRY_BUTTON;
    }
    if (entryPtr->height < infoPtr->minHeight) {
	infoPtr->minHeight = entryPtr->height;
    }
    /*
     * Note: The maximum entry width below does not take into account
     *       the space for the icon (level offset).  This has to be
     *       deferred because it's dependent upon the maximum icon
     *       size.
     */
    width = infoPtr->x + entryPtr->width;
    if (width > infoPtr->maxWidth) {
	infoPtr->maxWidth = width;
    }
    if (infoPtr->maxIconWidth < entryPtr->iconWidth) {
	infoPtr->maxIconWidth = entryPtr->iconWidth;
    }
    entryPtr->lineHeight = -(infoPtr->y);
    infoPtr->y += entryPtr->height;
    if (entryPtr->flags & ENTRY_OPEN) {
	Blt_ChainLink *linkPtr;
	int labelOffset;
	Tree *bottomPtr;

	infoPtr->level++;
	labelOffset = infoPtr->labelOffset;
	infoPtr->labelOffset = 0;
	bottomPtr = treePtr;
	for (linkPtr = Blt_ChainFirstLink(treePtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    treePtr = Blt_ChainGetValue(linkPtr);
	    if (treePtr->entryPtr->flags & ENTRY_MAPPED) {
		ResetCoordinates(hboxPtr, treePtr, infoPtr);
		bottomPtr = treePtr;
	    }
	}
	infoPtr->level--;
	entryPtr->lineHeight += bottomPtr->entryPtr->worldY;
	entryPtr->levelX = infoPtr->labelOffset;
	infoPtr->labelOffset = labelOffset;
    }
    if (infoPtr->labelOffset < entryPtr->labelWidth) {
	infoPtr->labelOffset = entryPtr->labelWidth;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ComputeWidths --
 *
 *	Determines the maximum height of all visible entries.
 *
 * Results:
 *	Returns 1 if beyond the last visible entry, 0 otherwise.
 *
 * Side effects:
 *	The array of visible nodes is filled.
 *
 * ----------------------------------------------------------------------
 */
static void
ComputeWidths(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    Entry *entryPtr = treePtr->entryPtr;

    if (!(entryPtr->flags & ENTRY_MAPPED)) {
	return;
    }
    if (entryPtr->iconWidth > LEVELWIDTH(treePtr->level + 1)) {
	LEVELWIDTH(treePtr->level + 1) = entryPtr->iconWidth;
    }
    if (entryPtr->flags & ENTRY_OPEN) {
	Blt_ChainLink *linkPtr;

	for (linkPtr = Blt_ChainFirstLink(treePtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    treePtr = Blt_ChainGetValue(linkPtr);
	    if (treePtr->entryPtr->flags & ENTRY_MAPPED) {
		ComputeWidths(hboxPtr, treePtr);
	    }
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ComputeLayout --
 *
 *	Recompute the layout when entries are opened/closed,
 *	inserted/deleted, or when text attributes change (such as
 *	font, linespacing).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The world coordinates are set for all the opened entries.
 *
 * ----------------------------------------------------------------------
 */
static void
ComputeLayout(hboxPtr)
    Hierbox *hboxPtr;
{
    LayoutInfo info;

    info.level = info.depth = 0;
    info.x = info.y = 0;
    info.maxWidth = hboxPtr->button.width;
    info.maxIconWidth = hboxPtr->button.width;
    info.minHeight = INT_MAX;
    info.labelOffset = 0;

    if (hboxPtr->hideRoot) {
	info.y = -(hboxPtr->rootPtr->entryPtr->height);
    }
    ResetCoordinates(hboxPtr, hboxPtr->rootPtr, &info);

    hboxPtr->xScrollUnits = info.maxIconWidth;
    hboxPtr->minHeight = hboxPtr->yScrollUnits = info.minHeight;
    if (hboxPtr->reqScrollX > 0) {
	hboxPtr->xScrollUnits = hboxPtr->reqScrollX;
    }
    if (hboxPtr->reqScrollY > 0) {
	hboxPtr->yScrollUnits = hboxPtr->reqScrollY;
    }
    hboxPtr->depth = info.depth + 1;
    hboxPtr->worldWidth = info.maxWidth + (hboxPtr->depth * info.maxIconWidth);
    if (hboxPtr->worldWidth < 1) {
	hboxPtr->worldWidth = 1;
    }
    hboxPtr->worldHeight = info.y;
    if (hboxPtr->worldHeight < 1) {
	hboxPtr->worldHeight = 1;
    }
    if (hboxPtr->yScrollUnits < 1) {
	hboxPtr->yScrollUnits = 1;
    }
    if (hboxPtr->xScrollUnits < 1) {
	hboxPtr->xScrollUnits = 1;
    }
    if (hboxPtr->levelInfo != NULL) {
	Blt_Free(hboxPtr->levelInfo);
    }
    hboxPtr->levelInfo = Blt_Calloc(hboxPtr->depth + 2, sizeof(LevelInfo));
    assert(hboxPtr->levelInfo);
    ComputeWidths(hboxPtr, hboxPtr->rootPtr);
    {
	int sum, width;
	register int i;

	sum = 0;
	for (i = 0; i <= hboxPtr->depth; i++) {
	    width = hboxPtr->levelInfo[i].width;
	    width |= 0x01;
	    hboxPtr->levelInfo[i].width = width;
	    sum += width;
	    hboxPtr->levelInfo[i + 1].x = sum;
	}
    }
    hboxPtr->flags &= ~HIERBOX_LAYOUT;
}

/*
 * ----------------------------------------------------------------------
 *
 * ComputeVisibleEntries --
 *
 *	The entries visible in the viewport (the widget's window) are
 *	inserted into the array of visible nodes.
 *
 * Results:
 *	Returns 1 if beyond the last visible entry, 0 otherwise.
 *
 * Side effects:
 *	The array of visible nodes is filled.
 *
 * ----------------------------------------------------------------------
 */
static int
ComputeVisibleEntries(hboxPtr)
    Hierbox *hboxPtr;
{
    Entry *entryPtr;
    int height;
    Blt_ChainLink *linkPtr;
    register Tree *treePtr;
    int x, maxX;
    int nSlots;

    hboxPtr->xOffset = Blt_AdjustViewport(hboxPtr->xOffset, hboxPtr->worldWidth,
	VPORTWIDTH(hboxPtr), hboxPtr->xScrollUnits, hboxPtr->scrollMode);
    hboxPtr->yOffset = Blt_AdjustViewport(hboxPtr->yOffset,
	hboxPtr->worldHeight, VPORTHEIGHT(hboxPtr), hboxPtr->yScrollUnits,
	hboxPtr->scrollMode);

    height = VPORTHEIGHT(hboxPtr);

    /* Allocate worst case number of slots for entry array. */
    nSlots = (height / hboxPtr->minHeight) + 3;
    if ((nSlots != hboxPtr->nVisible) && (hboxPtr->visibleArr != NULL)) {
	Blt_Free(hboxPtr->visibleArr);
    }
    hboxPtr->visibleArr = Blt_Calloc(nSlots, sizeof(Tree *));
    assert(hboxPtr->visibleArr);
    hboxPtr->nVisible = 0;

    /* Find the node where the view port starts. */
    treePtr = hboxPtr->rootPtr;
    entryPtr = treePtr->entryPtr;
    while ((entryPtr->worldY + entryPtr->height) <= hboxPtr->yOffset) {
	for (linkPtr = Blt_ChainLastLink(treePtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainPrevLink(linkPtr)) {
	    treePtr = Blt_ChainGetValue(linkPtr);
	    if (IsHidden(treePtr)) {
		continue;	/* Ignore hidden entries.  */
	    }
	    entryPtr = treePtr->entryPtr;
	    if (entryPtr->worldY <= hboxPtr->yOffset) {
		break;
	    }
	}

	/*
         * If we can't find the starting node, then the view must be
	 * scrolled down, but some nodes were deleted.  Reset the view
         * back to the top and try again.
         */
	if (linkPtr == NULL) {
	    if (hboxPtr->yOffset == 0) {
		return TCL_OK;	/* All entries are hidden. */
	    }
	    hboxPtr->yOffset = 0;
	    continue;
	}
    }

    height += hboxPtr->yOffset;
    maxX = 0;
    while (treePtr != NULL) {
	if (!IsHidden(treePtr)) {
	    entryPtr = treePtr->entryPtr;
	    /*
	     * Compute and save the entry's X-coordinate now that we know
	     * what the maximum level offset for the entire Hierbox is.
	     */
	    entryPtr->worldX = LEVELX(treePtr->level);
	    x = entryPtr->worldX + LEVELWIDTH(treePtr->level) +
		LEVELWIDTH(treePtr->level + 1) + entryPtr->width;
	    if (x > maxX) {
		maxX = x;
	    }
	    if (entryPtr->worldY >= height) {
		break;
	    }
	    hboxPtr->visibleArr[hboxPtr->nVisible] = treePtr;
	    hboxPtr->nVisible++;
	}
	treePtr = NextNode(treePtr, ENTRY_OPEN | ENTRY_MAPPED);
    }
    hboxPtr->worldWidth = maxX;

    /*
     * -------------------------------------------------------------------
     *
     * Note:	It's assumed that the view port always starts at or
     *		over an entry.  Check that a change in the hierarchy
     *		(e.g. closing a node) hasn't left the viewport beyond
     *		the last entry.  If so, adjust the viewport to start
     *		on the last entry.
     *
     * -------------------------------------------------------------------
     */
    if (hboxPtr->xOffset > (hboxPtr->worldWidth - hboxPtr->xScrollUnits)) {
	hboxPtr->xOffset = hboxPtr->worldWidth - hboxPtr->xScrollUnits;
    }
    if (hboxPtr->yOffset > (hboxPtr->worldHeight - hboxPtr->yScrollUnits)) {
	hboxPtr->yOffset = hboxPtr->worldHeight - hboxPtr->yScrollUnits;
    }
    hboxPtr->xOffset = Blt_AdjustViewport(hboxPtr->xOffset, hboxPtr->worldWidth,
	VPORTWIDTH(hboxPtr), hboxPtr->xScrollUnits, hboxPtr->scrollMode);
    hboxPtr->yOffset = Blt_AdjustViewport(hboxPtr->yOffset,
	hboxPtr->worldHeight, VPORTHEIGHT(hboxPtr), hboxPtr->yScrollUnits,
	hboxPtr->scrollMode);
    hboxPtr->flags &= ~HIERBOX_DIRTY;
    return TCL_OK;
}

static int
GetCursorLocation(hboxPtr, treePtr)
    Hierbox *hboxPtr;
    Tree *treePtr;
{
    TextEdit *editPtr = &(hboxPtr->labelEdit);
    int x, y;
    int maxLines;
    Tk_Font font;
    TextStyle ts;
    TextLayout *textPtr;
    Tk_FontMetrics fontMetrics;
    int nBytes;
    int sum;
    Entry *entryPtr;
    TextFragment *fragPtr;
    register int i;

    entryPtr = treePtr->entryPtr;
    font = GETFONT(hboxPtr, entryPtr->labelFont);
    memset(&ts, 0, sizeof(TextStyle));
    ts.font = font;
    ts.justify = TK_JUSTIFY_LEFT;
    ts.shadow.offset = entryPtr->labelShadow.offset;
    textPtr = Blt_GetTextLayout(entryPtr->labelText, &ts);

    Tk_GetFontMetrics(font, &fontMetrics);
    maxLines = (textPtr->height / fontMetrics.linespace) - 1;

    sum = 0;
    x = y = 0;

    fragPtr = textPtr->fragArr;
    for (i = 0; i <= maxLines; i++) {
	/* Total the number of bytes on each line.  Include newlines. */
	nBytes = fragPtr->count + 1;
	if ((sum + nBytes) > editPtr->insertPos) {
	    x += Tk_TextWidth(font, fragPtr->text, editPtr->insertPos - sum);
	    break;
	}
	y += fontMetrics.linespace;
	sum += nBytes;
	fragPtr++;
    }
    editPtr->x = x;
    editPtr->y = y;
    editPtr->height = fontMetrics.linespace;
    editPtr->width = 3;
    Blt_Free(textPtr);
    return TCL_OK;
}

/*
 * ---------------------------------------------------------------------------
 *
 * DrawVerticals --
 *
 * 	Draws vertical lines for the ancestor nodes.  While the entry
 *	of the ancestor may not be visible, its vertical line segment
 *	does extent into the viewport.  So walk back up the hierarchy
 *	drawing lines until we get to the root.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Vertical lines are drawn for the ancestor nodes.
 *
 * ---------------------------------------------------------------------------
 */
static void
DrawVerticals(hboxPtr, treePtr, drawable)
    Hierbox *hboxPtr;		/* Widget record containing the attribute
				 * information for buttons. */
    Tree *treePtr;		/* Entry to be drawn. */
    Drawable drawable;		/* Pixmap or window to draw into. */
{
    Entry *entryPtr;		/* Entry to be drawn. */
    int x1, y1, x2, y2;
    int height;
    int x, y;

    while (treePtr->parentPtr != NULL) {
	treePtr = treePtr->parentPtr;
	entryPtr = treePtr->entryPtr;

	/*
	 * World X-coordinates are computed only for entries that are in
	 * the current view port.  So for each of the off-screen ancestor
	 * nodes we must compute it here too.
	 */
	entryPtr->worldX = LEVELX(treePtr->level);
	x = SCREENX(hboxPtr, entryPtr->worldX);
	y = SCREENY(hboxPtr, entryPtr->worldY);
	height = MAX(entryPtr->iconHeight, hboxPtr->button.height);
	y += (height - hboxPtr->button.height) / 2;
	x1 = x2 = x + LEVELWIDTH(treePtr->level) +
	    LEVELWIDTH(treePtr->level + 1) / 2;
	y1 = y + hboxPtr->button.height / 2;
	y2 = y1 + entryPtr->lineHeight;
	if ((treePtr == hboxPtr->rootPtr) && (hboxPtr->hideRoot)) {
	    y1 += entryPtr->height;
	}
	/*
	 * Clip the line's Y-coordinates at the window border.
	 */
	if (y1 < 0) {
	    y1 = 0;
	}
	if (y2 > Tk_Height(hboxPtr->tkwin)) {
	    y2 = Tk_Height(hboxPtr->tkwin);
	}
	if ((y1 < Tk_Height(hboxPtr->tkwin)) && (y2 > 0)) {
	    XDrawLine(hboxPtr->display, drawable, hboxPtr->lineGC, x1, y1, 
		      x2, y2);
	}
    }
}

/*
 * ---------------------------------------------------------------------------
 *
 * DrawButton --
 *
 * 	Draws a button for the given entry. The button is drawn
 * 	centered in the region immediately to the left of the origin
 * 	of the entry (computed in the layout routines). The height
 * 	and width of the button were previously calculated from the
 * 	average row height.
 *
 *		button height = entry height - (2 * some arbitrary padding).
 *		button width = button height.
 *
 *	The button may have a border.  The symbol (either a plus or
 *	minus) is slight smaller than the width or height minus the
 *	border.
 *
 *	    x,y origin of entry
 *
 *              +---+
 *              | + | icon label
 *              +---+
 *             closed
 *
 *           |----|----| horizontal offset
 *
 *              +---+
 *              | - | icon label
 *              +---+
 *              open
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A button is drawn for the entry.
 *
 * ---------------------------------------------------------------------------
 */
static void
DrawButton(hboxPtr, treePtr, drawable)
    Hierbox *hboxPtr;		/* Widget record containing the attribute
				 * information for buttons. */
    Tree *treePtr;		/* Node of entry. */
    Drawable drawable;		/* Pixmap or window to draw into. */
{
    ButtonAttributes *buttonPtr = &(hboxPtr->button);
    Entry *entryPtr;
    int relief;
    Tk_3DBorder border;
    GC gc, lineGC;
    int x, y;
    CachedImage image;
    int width, height;

    entryPtr = treePtr->entryPtr;

    width = LEVELWIDTH(treePtr->level);
    height = MAX(entryPtr->iconHeight, buttonPtr->height);
    entryPtr->buttonX = (width - buttonPtr->width) / 2;
    entryPtr->buttonY = (height - buttonPtr->height) / 2;

    x = SCREENX(hboxPtr, entryPtr->worldX) + entryPtr->buttonX;
    y = SCREENY(hboxPtr, entryPtr->worldY) + entryPtr->buttonY;

    if (treePtr == hboxPtr->activeButtonPtr) {
	border = buttonPtr->activeBorder;
	lineGC = buttonPtr->activeGC;
    } else {
	border = buttonPtr->border;
	lineGC = buttonPtr->lineGC;
    }
    relief = (entryPtr->flags & ENTRY_OPEN)
	? buttonPtr->openRelief : buttonPtr->closeRelief;
    /*
     * FIXME: Reliefs "flat" and "solid" the same, since there's no 
     * "solid" in pre-8.0 releases.  Should change this when we go to a
     * pure 8.x release.
     */
    if (relief == TK_RELIEF_SOLID) {
	relief = TK_RELIEF_FLAT;
    }
    Blt_Fill3DRectangle(hboxPtr->tkwin, drawable, border, x, y,
	buttonPtr->width, buttonPtr->height, buttonPtr->borderWidth, relief);
    if (relief == TK_RELIEF_FLAT) {
	XDrawRectangle(hboxPtr->display, drawable, lineGC, x, y,
	    buttonPtr->width - 1, buttonPtr->height - 1);
    }
    x += buttonPtr->borderWidth;
    y += buttonPtr->borderWidth;
    width = buttonPtr->width - (2 * buttonPtr->borderWidth);
    height = buttonPtr->height - (2 * buttonPtr->borderWidth);

    image = NULL;
    if (buttonPtr->images != NULL) {
	/* Open or close button image? */
	image = buttonPtr->images[0];
	if ((entryPtr->flags & ENTRY_OPEN) && (buttonPtr->images[1] != NULL)) {
	    image = buttonPtr->images[1];
	}
    }
    /* Image or rectangle? */
    if (image != NULL) {
	Tk_RedrawImage(ImageBits(image), 0, 0, width, height, drawable, x, y);
    } else {
	XSegment segArr[2];
	int count;

	gc = (treePtr == hboxPtr->activeButtonPtr)
	    ? buttonPtr->activeGC : buttonPtr->normalGC;
	count = 1;
	segArr[0].y1 = segArr[0].y2 = y + height / 2;
	segArr[0].x1 = x + BUTTON_IPAD;
#ifdef WIN32
	segArr[0].x2 = x + width - BUTTON_IPAD;
#else
	segArr[0].x2 = x + width - BUTTON_IPAD - 1;
#endif
	if (!(entryPtr->flags & ENTRY_OPEN)) {
	    segArr[1].x1 = segArr[1].x2 = x + width / 2;
	    segArr[1].y1 = y + BUTTON_IPAD;
#ifdef WIN32
	    segArr[1].y2 = y + height - BUTTON_IPAD;
#else
	    segArr[1].y2 = y + height - BUTTON_IPAD - 1;
#endif
	    count++;
	}
	XDrawSegments(hboxPtr->display, drawable, gc, segArr, count);
    }
}


static void
DisplayIcon(hboxPtr, treePtr, x, y, drawable)
    Hierbox *hboxPtr;		/* Widget record containing the attribute
				 * information for buttons. */
    Tree *treePtr;		/* Node of entry. */
    int x, y;
    Drawable drawable;		/* Pixmap or window to draw into. */
{
    Entry *entryPtr = treePtr->entryPtr;
    int entryHeight;
    CachedImage image;
    int isActive;

    entryHeight = MAX(entryPtr->iconHeight, hboxPtr->button.height);
    isActive = (treePtr == hboxPtr->activePtr);
    image = NULL;
    if ((isActive) && (entryPtr->activeIcons != NULL)) {
	image = entryPtr->activeIcons[0];
	if ((treePtr == hboxPtr->focusPtr) && 
	    (entryPtr->activeIcons[1] != NULL)) {
	    image = entryPtr->activeIcons[1];
	}
    } else if (entryPtr->icons != NULL) {	/* Selected or normal icon? */
	image = entryPtr->icons[0];
	if ((treePtr == hboxPtr->focusPtr) && (entryPtr->icons[1] != NULL)) {
	    image = entryPtr->icons[1];
	}
    }
    if (image != NULL) {	/* Image or default icon bitmap? */
	int width, height;
	int top, bottom;
	int inset;
	int maxY;

	height = ImageHeight(image);
	width = ImageWidth(image);
	x += (LEVELWIDTH(treePtr->level + 1) - width) / 2;
	y += (entryHeight - height) / 2;
	inset = hboxPtr->inset - INSET_PAD;
	maxY = Tk_Height(hboxPtr->tkwin) - inset;
	top = 0;
	bottom = y + height;
	if (y < inset) {
	    height += y - inset;
	    top = -y + inset;
	    y = inset;
	} else if (bottom >= maxY) {
	    height = maxY - y;
	}
	Tk_RedrawImage(ImageBits(image), 0, top, width, height, drawable, x, y);
    } else {
	x += (LEVELWIDTH(treePtr->level + 1) - DEF_ICON_WIDTH) / 2;
	y += (entryHeight - DEF_ICON_HEIGHT) / 2;
	XSetClipOrigin(hboxPtr->display, entryPtr->iconGC, x, y);
	XCopyPlane(hboxPtr->display, hboxPtr->iconBitmap, drawable,
	    entryPtr->iconGC, 0, 0, DEF_ICON_WIDTH, DEF_ICON_HEIGHT, x, y, 1);
    }
}

static void
DrawData(hboxPtr, treePtr, x, y, entryHeight, drawable)
    Hierbox *hboxPtr;		/* Widget record containing the attribute
				 * information for buttons. */
    Tree *treePtr;		/* Node of entry. */
    int x, y;
    int entryHeight;
    Drawable drawable;		/* Pixmap or window to draw into. */
{
    Entry *entryPtr = treePtr->entryPtr;
    /*
     * Auxillary data: text string or images.
     */
    if (entryPtr->images != NULL) {
	register CachedImage *imagePtr;
	int imageY;

	for (imagePtr = entryPtr->images; *imagePtr != NULL; imagePtr++) {
	    imageY = y;
	    if (ImageHeight(*imagePtr) < entryHeight) {
		imageY += (entryHeight - ImageHeight(*imagePtr)) / 2;
	    }
	    Tk_RedrawImage(ImageBits(*imagePtr), 0, 0, ImageWidth(*imagePtr),
		ImageHeight(*imagePtr), drawable, x, imageY);
	    x += ImageWidth(*imagePtr);
	}
    } else if (entryPtr->dataText != NULL) {
	TextStyle ts;
	Tk_Font font;
	XColor *colorPtr;
	int width, height;

	font = GETFONT(hboxPtr, entryPtr->dataFont);
	colorPtr = GETCOLOR(hboxPtr, entryPtr->dataColor);
	y += hboxPtr->selBorderWidth + LABEL_PADY;

	Blt_SetDrawTextStyle(&ts, font, entryPtr->dataGC, colorPtr,
	    hboxPtr->selFgColor, entryPtr->dataShadow.color, 0.0,
	    TK_ANCHOR_NW, TK_JUSTIFY_LEFT, 0, entryPtr->dataShadow.offset);
	Blt_GetTextExtents(&ts, entryPtr->dataText, &width, &height);
	if (height < entryHeight) {
	    y += (entryHeight - height) / 2;
	}
	Blt_DrawText(hboxPtr->tkwin, drawable, entryPtr->dataText, &ts, x, y);
    }
}


static int
DrawLabel(hboxPtr, treePtr, x, y, drawable)
    Hierbox *hboxPtr;		/* Widget record containing the attribute
				 * information for buttons. */
    Tree *treePtr;		/* Node of entry. */
    int x, y;
    Drawable drawable;		/* Pixmap or window to draw into. */
{
    Entry *entryPtr = treePtr->entryPtr;
    TextEdit *editPtr = &(hboxPtr->labelEdit);
    TextStyle ts;
    int width, height;		/* Width and height of label. */
    Tk_Font font;
    int isSelected, isFocused;
    int entryHeight;

    entryHeight = MAX(entryPtr->iconHeight, hboxPtr->button.height);
    font = GETFONT(hboxPtr, entryPtr->labelFont);
    isFocused = ((treePtr == hboxPtr->focusPtr) &&
	(hboxPtr->flags & HIERBOX_FOCUS));
    isSelected = IsSelected(hboxPtr, treePtr);

    /* Includes padding, selection 3-D border, and focus outline. */
    width = entryPtr->labelWidth;
    height = entryPtr->labelHeight;

    /* Center the label, if necessary, vertically along the entry row. */
    if (height < entryHeight) {
	y += (entryHeight - height) / 2;
    }

#ifdef notdef
    /* Normal background color */
    Blt_Fill3DRectangle(hboxPtr->tkwin, drawable, hboxPtr->border, x, y, width, 
	       height, 0, TK_RELIEF_FLAT);
#endif
    if (isFocused) {		/* Focus outline */
	XDrawRectangle(hboxPtr->display, drawable, hboxPtr->focusGC,
	    x, y, width - 1, height - 1);
    }
    x += FOCUS_WIDTH;
    y += FOCUS_WIDTH;
    if (isSelected) {
	Blt_Fill3DRectangle(hboxPtr->tkwin, drawable, hboxPtr->selBorder,
	    x, y, width - 2 * FOCUS_WIDTH, height - 2 * FOCUS_WIDTH,
	    hboxPtr->selBorderWidth, hboxPtr->selRelief);
    }
    x += LABEL_PADX + hboxPtr->selBorderWidth;
    y += LABEL_PADY + hboxPtr->selBorderWidth;

    if (*entryPtr->labelText != '\0') {
	XColor *normalColor;

	normalColor = GETCOLOR(hboxPtr, entryPtr->labelColor);
	Blt_SetDrawTextStyle(&ts, font, entryPtr->labelGC, normalColor,
	    hboxPtr->selFgColor, entryPtr->labelShadow.color, 0.0, TK_ANCHOR_NW,
	    TK_JUSTIFY_LEFT, 0, entryPtr->labelShadow.offset);
	ts.state = (isSelected) ? STATE_ACTIVE : 0;
	Blt_DrawText(hboxPtr->tkwin, drawable, entryPtr->labelText, &ts,
	    x, y);
    }
    if ((isFocused) && (hboxPtr->focusEdit) && (editPtr->cursorOn)) {
	int x1, y1, x2, y2;

	GetCursorLocation(hboxPtr, treePtr);
	x1 = x + editPtr->x;
	x2 = x1 + 3;
	y1 = y + editPtr->y - 1;
	y2 = y1 + editPtr->height - 1;
	XDrawLine(hboxPtr->display, drawable, entryPtr->labelGC,
	    x1, y1, x1, y2);
	XDrawLine(hboxPtr->display, drawable, entryPtr->labelGC,
	    x1 - 2, y1, x2, y1);
	XDrawLine(hboxPtr->display, drawable, entryPtr->labelGC,
	    x1 - 2, y2, x2, y2);
    }
    return entryHeight;
}

/*
 * ---------------------------------------------------------------------------
 *
 * DrawEntry --
 *
 * 	Draws a button for the given entry.  Note that buttons should only
 *	be drawn if the entry has sub-entries to be opened or closed.  It's
 *	the responsibility of the calling routine to ensure this.
 *
 *	The button is drawn centered in the region immediately to the left
 *	of the origin of the entry (computed in the layout routines). The
 *	height and width of the button were previously calculated from the
 *	average row height.
 *
 *		button height = entry height - (2 * some arbitrary padding).
 *		button width = button height.
 *
 *	The button has a border.  The symbol (either a plus or minus) is
 *	slight smaller than the width or height minus the border.
 *
 *	    x,y origin of entry
 *
 *              +---+
 *              | + | icon label
 *              +---+
 *             closed
 *
 *           |----|----| horizontal offset
 *
 *              +---+
 *              | - | icon label
 *              +---+
 *              open
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A button is drawn for the entry.
 *
 * ---------------------------------------------------------------------------
 */
static void
DrawEntry(hboxPtr, treePtr, drawable)
    Hierbox *hboxPtr;		/* Widget record containing the attribute
				 * information for buttons. */
    Tree *treePtr;		/* Node of entry to be drawn. */
    Drawable drawable;		/* Pixmap or window to draw into. */
{
    ButtonAttributes *buttonPtr = &(hboxPtr->button);
    int x, y;
    int width, height;
    int entryHeight;
    int buttonY;
    int x1, y1, x2, y2;
    Entry *entryPtr;

    entryPtr = treePtr->entryPtr;
    x = SCREENX(hboxPtr, entryPtr->worldX);
    y = SCREENY(hboxPtr, entryPtr->worldY);

    width = LEVELWIDTH(treePtr->level);
    height = MAX(entryPtr->iconHeight, buttonPtr->height);

    entryPtr->buttonX = (width - buttonPtr->width) / 2;
    entryPtr->buttonY = (height - buttonPtr->height) / 2;

    buttonY = y + entryPtr->buttonY;

    x1 = x + (width / 2);
    y1 = y2 = buttonY + (buttonPtr->height / 2);
    x2 = x1 + (LEVELWIDTH(treePtr->level) + LEVELWIDTH(treePtr->level + 1)) / 2;

    if ((treePtr->parentPtr != NULL) && (hboxPtr->lineWidth > 0)) {
	/*
	 * For every node except root, draw a horizontal line from
	 * the vertical bar to the middle of the icon.
	 */
	XDrawLine(hboxPtr->display, drawable, hboxPtr->lineGC, x1, y1, x2, y2);
    }
    if ((entryPtr->flags & ENTRY_OPEN) && (hboxPtr->lineWidth > 0)) {
	/*
	 * Entry is open, draw vertical line.
	 */
	y2 = y1 + entryPtr->lineHeight;
	if (y2 > Tk_Height(hboxPtr->tkwin)) {
	    y2 = Tk_Height(hboxPtr->tkwin);	/* Clip line at window border. */
	}
	XDrawLine(hboxPtr->display, drawable, hboxPtr->lineGC, x2, y1, x2, y2);
    }
    if ((entryPtr->flags & ENTRY_BUTTON) && (treePtr->parentPtr != NULL)) {
	/*
	 * Except for root, draw a button for every entry that needs
	 * one.  The displayed button can be either a Tk image or a
	 * rectangle with plus or minus sign.
	 */
	DrawButton(hboxPtr, treePtr, drawable);
    }
    x += LEVELWIDTH(treePtr->level);
    DisplayIcon(hboxPtr, treePtr, x, y, drawable);

    x += LEVELWIDTH(treePtr->level + 1) + 4;

    /* Entry label. */
    entryHeight = DrawLabel(hboxPtr, treePtr, x, y, drawable);
    if (treePtr->parentPtr != NULL) {
	x += treePtr->parentPtr->entryPtr->levelX + LABEL_PADX;
    } else {
	x += width + entryPtr->labelWidth + LABEL_PADX;
    }
    /* Auxilary data */
    DrawData(hboxPtr, treePtr, x, y, entryHeight, drawable);

}

static void
DrawOuterBorders(hboxPtr, drawable)
    Hierbox *hboxPtr;
    Drawable drawable;
{
    /* Draw 3D border just inside of the focus highlight ring. */
    if ((hboxPtr->borderWidth > 0) && (hboxPtr->relief != TK_RELIEF_FLAT)) {
	Blt_Draw3DRectangle(hboxPtr->tkwin, drawable, hboxPtr->border,
	    hboxPtr->highlightWidth, hboxPtr->highlightWidth,
	    Tk_Width(hboxPtr->tkwin) - 2 * hboxPtr->highlightWidth,
	    Tk_Height(hboxPtr->tkwin) - 2 * hboxPtr->highlightWidth,
	    hboxPtr->borderWidth, hboxPtr->relief);
    }
    /* Draw focus highlight ring. */
    if (hboxPtr->highlightWidth > 0) {
	XColor *color;
	GC gc;

	color = (hboxPtr->flags & HIERBOX_FOCUS)
	    ? hboxPtr->highlightColor : hboxPtr->highlightBgColor;
	gc = Tk_GCForColor(color, drawable);
	Tk_DrawFocusHighlight(hboxPtr->tkwin, gc, hboxPtr->highlightWidth,
	    drawable);
    }
    hboxPtr->flags &= ~HIERBOX_BORDERS;
}

/*
 * ----------------------------------------------------------------------
 *
 * DisplayHierbox --
 *
 * 	This procedure is invoked to display the widget.
 *
 *      Recompute the layout of the text if necessary. This is
 *	necessary if the world coordinate system has changed.
 *	Specifically, the following may have occurred:
 *
 *	  1.  a text attribute has changed (font, linespacing, etc.).
 *	  2.  an entry's option changed, possibly resizing the entry.
 *
 *      This is deferred to the display routine since potentially
 *      many of these may occur.
 *
 *	Set the vertical and horizontal scrollbars.  This is done
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
DisplayHierbox(clientData)
    ClientData clientData;	/* Information about widget. */
{
    Hierbox *hboxPtr = clientData;
    Pixmap drawable;

    hboxPtr->flags &= ~HIERBOX_REDRAW;
    if (hboxPtr->tkwin == NULL) {
	return;			/* Window has been destroyed. */
    }
    if (hboxPtr->flags & HIERBOX_LAYOUT) {
	/*
	 * Recompute the layout when entries are opened/closed,
	 * inserted/deleted, or when text attributes change
	 * (such as font, linespacing).
	 */
	ComputeLayout(hboxPtr);
    }
    if (hboxPtr->flags & HIERBOX_SCROLL) {
	int width, height;

	/* Scrolling means that the view port has changed and that the
	 * visible entries need to be recomputed.  */
	ComputeVisibleEntries(hboxPtr);
	Blt_PickCurrentItem(hboxPtr->bindTable);
	Blt_PickCurrentItem(hboxPtr->buttonBindTable);

	width = VPORTWIDTH(hboxPtr);
	height = VPORTHEIGHT(hboxPtr);
	if (hboxPtr->flags & HIERBOX_XSCROLL) {
	    if (hboxPtr->xScrollCmdPrefix != NULL) {
		Blt_UpdateScrollbar(hboxPtr->interp, hboxPtr->xScrollCmdPrefix,
		    (double)hboxPtr->xOffset / hboxPtr->worldWidth,
		    (double)(hboxPtr->xOffset + width) / hboxPtr->worldWidth);
	    }
	}
	if (hboxPtr->flags & HIERBOX_YSCROLL) {
	    if (hboxPtr->yScrollCmdPrefix != NULL) {
		Blt_UpdateScrollbar(hboxPtr->interp, hboxPtr->yScrollCmdPrefix,
		    (double)hboxPtr->yOffset / hboxPtr->worldHeight,
		    (double)(hboxPtr->yOffset + height) / hboxPtr->worldHeight);
	    }
	}
	hboxPtr->flags &= ~HIERBOX_SCROLL;
    }
    if (!Tk_IsMapped(hboxPtr->tkwin)) {
	return;
    }
    drawable = Tk_GetPixmap(hboxPtr->display, Tk_WindowId(hboxPtr->tkwin),
	Tk_Width(hboxPtr->tkwin), Tk_Height(hboxPtr->tkwin),
	Tk_Depth(hboxPtr->tkwin));

    /*
     * Clear the background either by tiling a pixmap or filling with
     * a solid color. Tiling takes precedence.
     */
    if (hboxPtr->tile != NULL) {
	if (hboxPtr->scrollTile) {
	    Blt_SetTSOrigin(hboxPtr->tkwin, hboxPtr->tile, -hboxPtr->xOffset,
		-hboxPtr->yOffset);
	} else {
	    Blt_SetTileOrigin(hboxPtr->tkwin, hboxPtr->tile, 0, 0);
	}
	Blt_TileRectangle(hboxPtr->tkwin, drawable, hboxPtr->tile, 0, 0,
	    Tk_Width(hboxPtr->tkwin), Tk_Height(hboxPtr->tkwin));
    } else {
	Blt_Fill3DRectangle(hboxPtr->tkwin, drawable, hboxPtr->border, 0, 0,
	    Tk_Width(hboxPtr->tkwin), Tk_Height(hboxPtr->tkwin), 0,
	    TK_RELIEF_FLAT);
    }

    if (hboxPtr->nVisible > 0) {
	register Tree **treePtrPtr;

	if (hboxPtr->activePtr != NULL) {
	    int y, height;

	    y = SCREENY(hboxPtr, hboxPtr->activePtr->entryPtr->worldY);
	    height = MAX(hboxPtr->activePtr->entryPtr->iconHeight, 
		hboxPtr->activePtr->entryPtr->labelHeight);
	    Blt_Fill3DRectangle(hboxPtr->tkwin, drawable, hboxPtr->activeBorder, 
	       0, y, Tk_Width(hboxPtr->tkwin), height, 0, TK_RELIEF_FLAT);
	}
	if (hboxPtr->lineWidth > 0) {
	    DrawVerticals(hboxPtr, hboxPtr->visibleArr[0], drawable);
	}
	for (treePtrPtr = hboxPtr->visibleArr; *treePtrPtr != NULL;
	    treePtrPtr++) {
	    DrawEntry(hboxPtr, *treePtrPtr, drawable);
	}
    }
    DrawOuterBorders(hboxPtr, drawable);
    /* Now copy the new view to the window. */
    XCopyArea(hboxPtr->display, drawable, Tk_WindowId(hboxPtr->tkwin),
	hboxPtr->lineGC, 0, 0, Tk_Width(hboxPtr->tkwin),
	Tk_Height(hboxPtr->tkwin), 0, 0);
    Tk_FreePixmap(hboxPtr->display, drawable);
}

/*
 *----------------------------------------------------------------------
 *
 * SelectCmdProc --
 *
 *      Invoked at the next idle point whenever the current
 *      selection changes.  Executes some application-specific code
 *      in the -selectcommand option.  This provides a way for
 *      applications to handle selection changes.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Tcl code gets executed for some application-specific task.
 *
 *----------------------------------------------------------------------
 */
static void
SelectCmdProc(clientData)
    ClientData clientData;	/* Information about widget. */
{
    Hierbox *hboxPtr = clientData;

    /*
     * Preserve the widget structure in case the "select" callback
     * destroys it.
     */
    Tcl_Preserve(hboxPtr);
    if (hboxPtr->selectCmd != NULL) {
	hboxPtr->flags &= ~SELECTION_PENDING;
	if (Tcl_GlobalEval(hboxPtr->interp, hboxPtr->selectCmd) != TCL_OK) {
	    Tcl_BackgroundError(hboxPtr->interp);
	}
    }
    Tcl_Release(hboxPtr);
}

/*
 * --------------------------------------------------------------
 *
 * HierboxCmd --
 *
 * 	This procedure is invoked to process the Tcl command that
 * 	corresponds to a widget managed by this module. See the user
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
/* ARGSUSED */
static int
HierboxCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Hierbox *hboxPtr;
    Tk_Window tkwin;
    Tree *treePtr;
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
    hboxPtr = CreateHierbox(interp, tkwin);

    if (Blt_ConfigureWidgetComponent(interp, tkwin, "button", "Button",
	    buttonConfigSpecs, 0, (char **)NULL, (char *)hboxPtr, 0) != TCL_OK) {
	goto error;
    }
    if (ConfigureHierbox(interp, hboxPtr, argc - 2, argv + 2, 0) != TCL_OK) {
	goto error;
    }
    treePtr = CreateNode(hboxPtr, (Tree *) NULL, APPEND, hboxPtr->separator);
    if (treePtr == NULL) {
	goto error;
    }
    hboxPtr->rootPtr = hboxPtr->focusPtr = treePtr;
    hboxPtr->selAnchorPtr = NULL;
    Blt_SetFocusItem(hboxPtr->bindTable, hboxPtr->focusPtr, NULL);

    Tk_CreateSelHandler(tkwin, XA_PRIMARY, XA_STRING, SelectionProc, hboxPtr, 
	XA_STRING);
    Tk_CreateEventHandler(tkwin, ExposureMask | StructureNotifyMask |
	FocusChangeMask, HierboxEventProc, hboxPtr);

    hboxPtr->cmdToken = Tcl_CreateCommand(interp, argv[1], HierboxInstCmd,
	  hboxPtr, HierboxInstCmdDeleteProc);
#ifdef ITCL_NAMESPACES
    Itk_SetWidgetCommand(hboxPtr->tkwin, hboxPtr->cmdToken);
#endif

    /*
     * Invoke a procedure to initialize various bindings on hierbox
     * entries.  If the procedure doesn't already exist, source it
     * from "$blt_library/bltHierbox.tcl".  We deferred sourcing the file
     * until now so that the variable $blt_library could be set within a
     * script.
     */
    if (!Tcl_GetCommandInfo(interp, "blt::Hierbox::Init", &cmdInfo)) {
	static char initCmd[] =
	{
	    "source [file join $blt_library hierbox.tcl]"
	};
	if (Tcl_GlobalEval(interp, initCmd) != TCL_OK) {
	    char info[200];

	    sprintf(info, "\n    (while loading bindings for %s)", argv[0]);
	    Tcl_AddErrorInfo(interp, info);
	    goto error;
	}
    }
    if (Tcl_VarEval(interp, "blt::Hierbox::Init ", argv[1], (char *)NULL)
	!= TCL_OK) {
	goto error;
    }
    treePtr->entryPtr->flags = (ENTRY_MAPPED);
    if (OpenNode(hboxPtr, treePtr) != TCL_OK) {
	goto error;
    }
    Tcl_SetResult(interp, Tk_PathName(hboxPtr->tkwin), TCL_VOLATILE);
    return TCL_OK;
  error:
    Tk_DestroyWindow(tkwin);
    return TCL_ERROR;
}

/*
 * --------------------------------------------------------------
 *
 * Hierbox operations
 *
 * --------------------------------------------------------------
 */

/*ARGSUSED*/
static int
FocusOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (argc == 3) {
	Tree *treePtr;

	treePtr = hboxPtr->focusPtr;
	if (GetNode(hboxPtr, argv[2], &treePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((treePtr != NULL) && (treePtr != hboxPtr->focusPtr)) {
	    if (IsHidden(treePtr)) {
		/* Doesn't make sense to set focus to a node you can't see. */
		ExposeAncestors(treePtr);
	    }
	    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
	    hboxPtr->focusPtr = treePtr;
	    hboxPtr->labelEdit.insertPos = strlen(treePtr->entryPtr->labelText);
	}
	EventuallyRedraw(hboxPtr);
    }
    Blt_SetFocusItem(hboxPtr->bindTable, hboxPtr->focusPtr, NULL);
    if (hboxPtr->focusPtr != NULL) {
	Tcl_SetResult(interp, NodeToString(hboxPtr, hboxPtr->focusPtr),
	    TCL_VOLATILE);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * BboxOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BboxOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *treePtr;
    register int i;
    Entry *entryPtr;
    char string[200];
    int width, height, yBot;
    int left, top, right, bottom;
    int screen;

    if (hboxPtr->flags & HIERBOX_LAYOUT) {
	/*
	 * The layout is dirty.  Recompute it now, before we use the
	 * world dimensions.  But remember, the "bbox" operation isn't
	 * valid for hidden entries (since they're not visible, they
	 * don't have world coordinates).
	 */
	ComputeLayout(hboxPtr);
    }
    left = hboxPtr->worldWidth;
    top = hboxPtr->worldHeight;
    right = bottom = 0;

    screen = FALSE;
    if ((argc > 2) && (argv[2][0] == '-') && 
	(strcmp(argv[2], "-screen") == 0)) {
	screen = TRUE;
	argc--, argv++;
    }
    for (i = 2; i < argc; i++) {
	if ((argv[i][0] == 'a') && (strcmp(argv[i], "all") == 0)) {
	    left = top = 0;
	    right = hboxPtr->worldWidth;
	    bottom = hboxPtr->worldHeight;
	    break;
	}
	treePtr = hboxPtr->focusPtr;
	if (GetNode(hboxPtr, argv[i], &treePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((treePtr == NULL) || (IsHidden(treePtr))) {
	    continue;
	}
	entryPtr = treePtr->entryPtr;
	yBot = entryPtr->worldY + entryPtr->height;
	height = VPORTHEIGHT(hboxPtr);
	if ((yBot <= hboxPtr->yOffset) &&
	    (entryPtr->worldY >= (hboxPtr->yOffset + height))) {
	    continue;
	}
	if (bottom < yBot) {
	    bottom = yBot;
	}
	if (top > entryPtr->worldY) {
	    top = entryPtr->worldY;
	}
	if (right <
	    (entryPtr->worldX + entryPtr->width + LEVELWIDTH(treePtr->level))) {
	    right = (entryPtr->worldX + entryPtr->width +
		LEVELWIDTH(treePtr->level));
	}
	if (left > entryPtr->worldX) {
	    left = entryPtr->worldX;
	}
    }

    if (screen) {
	width = VPORTWIDTH(hboxPtr);
	height = VPORTHEIGHT(hboxPtr);

	/*
	 * Do a min-max text for the intersection of the viewport and
	 * the computed bounding box.  If there is no intersection, return
	 * the empty string.
	 */
	if ((right < hboxPtr->xOffset) || (bottom < hboxPtr->yOffset) ||
	    (left >= (hboxPtr->xOffset + width)) ||
	    (top >= (hboxPtr->yOffset + height))) {
	    return TCL_OK;
	}
	/* Otherwise clip the coordinates at the view port boundaries. */
	if (left < hboxPtr->xOffset) {
	    left = hboxPtr->xOffset;
	} else if (right > (hboxPtr->xOffset + width)) {
	    right = hboxPtr->xOffset + width;
	}
	if (top < hboxPtr->yOffset) {
	    top = hboxPtr->yOffset;
	} else if (bottom > (hboxPtr->yOffset + height)) {
	    bottom = hboxPtr->yOffset + height;
	}
	left = SCREENX(hboxPtr, left), top = SCREENY(hboxPtr, top);
	right = SCREENX(hboxPtr, right), bottom = SCREENY(hboxPtr, bottom);
    }
    if ((left < right) && (top < bottom)) {
	sprintf(string, "%d %d %d %d", left, top, right - left, bottom - top);
	Tcl_SetResult(interp, string, TCL_VOLATILE);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonActivateOp --
 *
 *	Selects the button to appear active.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ButtonActivateOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *treePtr, *oldPtr;

    treePtr = hboxPtr->focusPtr;
    if (argv[3][0] == '\0') {
	treePtr = NULL;
    } else if (GetNode(hboxPtr, argv[3], &treePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    oldPtr = hboxPtr->activeButtonPtr;
    hboxPtr->activeButtonPtr = treePtr;
    if (treePtr != oldPtr) {
	Drawable drawable;

	drawable = Tk_WindowId(hboxPtr->tkwin);
	if (oldPtr != NULL) {
	    DrawButton(hboxPtr, oldPtr, drawable);
	}
	if (treePtr != NULL) {
	    DrawButton(hboxPtr, treePtr, drawable);
	}
	DrawOuterBorders(hboxPtr, drawable);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonBindOp --
 *
 *	  .t bind tag sequence command
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ButtonBindOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    ClientData object;
    /*
     * Individual entries are selected by inode only.  All other
     * strings are interpreted as a binding tag.  For example, if one
     * binds to "focus", it is assumed that this refers to a bind tag,
     * not the entry with focus.
     */
    object = GetNodeByIndex(hboxPtr, argv[3]);
    if (object == 0) {
	object = (ClientData)Tk_GetUid(argv[3]);
    }
    return Blt_ConfigureBindings(interp, hboxPtr->buttonBindTable, object,
	argc - 4, argv + 4);
}

/*
 *----------------------------------------------------------------------
 *
 * ButCgetOpOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ButtonCgetOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    return Tk_ConfigureValue(interp, hboxPtr->tkwin, buttonConfigSpecs,
	(char *)hboxPtr, argv[3], 0);
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonConfigureOp --
 *
 * 	This procedure is called to process a list of configuration
 *	options database, in order to reconfigure the one of more
 *	entries in the widget.
 *
 *	  .h entryconfigure node node node node option value
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for hboxPtr; old resources get freed, if there
 *	were any.  The hypertext is redisplayed.
 *
 *----------------------------------------------------------------------
 */
static int
ButtonConfigureOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    /* Figure out where the option value pairs begin */
    argc -= 3;
    argv += 3;

    if (argc == 0) {
	return Tk_ConfigureInfo(interp, hboxPtr->tkwin, buttonConfigSpecs,
	    (char *)hboxPtr, (char *)NULL, 0);
    } else if (argc == 1) {
	return Tk_ConfigureInfo(interp, hboxPtr->tkwin, buttonConfigSpecs,
	    (char *)hboxPtr, argv[0], 0);
    }
    if (Tk_ConfigureWidget(hboxPtr->interp, hboxPtr->tkwin, buttonConfigSpecs,
	    argc, argv, (char *)hboxPtr, TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    ConfigureButtons(hboxPtr);
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonOp --
 *
 *	This procedure handles button operations.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static Blt_OpSpec buttonOperSpecs[] =
{
    {"activate", 1, (Blt_Op)ButtonActivateOp, 4, 4, "node",},
    {"bind", 1, (Blt_Op)ButtonBindOp, 4, 6,
	"tagName ?sequence command?",},
    {"cget", 2, (Blt_Op)ButtonCgetOp, 4, 4, "option",},
    {"configure", 2, (Blt_Op)ButtonConfigureOp, 3, 0,
	"?option value?...",},
    {"highlight", 1, (Blt_Op)ButtonActivateOp, 4, 4, "node",},
};

static int nButtonSpecs = sizeof(buttonOperSpecs) / sizeof(Blt_OpSpec);

static int
ButtonOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nButtonSpecs, buttonOperSpecs, BLT_OP_ARG2, argc,
	 argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (hboxPtr, interp, argc, argv);
    return result;
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
CgetOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    return Tk_ConfigureValue(interp, hboxPtr->tkwin, configSpecs,
	(char *)hboxPtr, argv[2], 0);
}

/*ARGSUSED*/
static int
CloseOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Tree *rootPtr;
    unsigned int flags;
    register int i;

    flags = 0;
    if (argc > 2) {
	int length;

	length = strlen(argv[2]);
	if ((argv[2][0] == '-') && (length > 1) &&
	    (strncmp(argv[2], "-recurse", length) == 0)) {
	    argv++, argc--;
	    flags |= APPLY_RECURSE;
	}
    }
    for (i = 2; i < argc; i++) {
	rootPtr = hboxPtr->focusPtr;
	if (GetNode(hboxPtr, argv[i], &rootPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (rootPtr == NULL) {
	    continue;
	}
	/*
	 * Clear any selected entries that may become hidden by
	 * closing the node.
	 */
	PruneSelection(hboxPtr, rootPtr);

	/*
	 * -----------------------------------------------------------
	 *
	 *  Check if either the "focus" entry or selection anchor
	 *  is in this hierarchy.  Must move it or disable it before
	 *  we close the node.  Otherwise it may be deleted by a Tcl
	 *  "close" script, and we'll be left pointing to a bogus
	 *  memory location.
	 *
	 * -----------------------------------------------------------
	 */
	if (IsAncestor(rootPtr, hboxPtr->focusPtr)) {
	    hboxPtr->focusPtr = rootPtr;
	    Blt_SetFocusItem(hboxPtr->bindTable, hboxPtr->focusPtr, NULL);
	}
	if (IsAncestor(rootPtr, hboxPtr->selAnchorPtr)) {
	    hboxPtr->selAnchorPtr = NULL;
	}
	if (IsAncestor(rootPtr, hboxPtr->activePtr)) {
	    hboxPtr->activePtr = rootPtr;
	}
	if (ApplyToTree(hboxPtr, rootPtr, CloseNode, flags) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
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
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for hboxPtr; old resources get freed, if there
 *	were any.  The widget is redisplayed.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (argc == 2) {
	return Tk_ConfigureInfo(interp, hboxPtr->tkwin, configSpecs,
	    (char *)hboxPtr, (char *)NULL, 0);
    } else if (argc == 3) {
	return Tk_ConfigureInfo(interp, hboxPtr->tkwin, configSpecs,
	    (char *)hboxPtr, argv[2], 0);
    }
    if (ConfigureHierbox(interp, hboxPtr, argc - 2, argv + 2,
	    TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
CurselectionOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;			/* Not used. */
    char **argv;		/* Not used. */
{
    if (hboxPtr->sortSelection) {
	ApplyToTree(hboxPtr, hboxPtr->rootPtr, IsSelectedNode,
		    APPLY_RECURSE | APPLY_OPEN_ONLY | APPLY_BEFORE);
    } else {
	Blt_ChainLink *linkPtr;
	Tree *treePtr;

	for (linkPtr = Blt_ChainFirstLink(&(hboxPtr->selectChain)); 
	     linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	    treePtr = Blt_ChainGetValue(linkPtr);
	    Tcl_AppendElement(interp, NodeToString(hboxPtr, treePtr));
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ActivateOpOp --
 *
 *	Selects the tab to appear active.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ActivateOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *treePtr, *oldPtr;

    treePtr = hboxPtr->focusPtr;
    if (argv[3][0] == '\0') {
	treePtr = NULL;
    } else if (GetNode(hboxPtr, argv[3], &treePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    oldPtr = hboxPtr->activePtr;
    hboxPtr->activePtr = treePtr;
    if (treePtr != oldPtr) {
	EventuallyRedraw(hboxPtr);
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
BindOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    ClientData item;

    /*
     * Individual entries are selected by inode only.  All other strings
     * are interpreted as a binding tag.
     */
    item = GetNodeByIndex(hboxPtr, argv[2]);
    if (item == 0) {
	item = (ClientData)Tk_GetUid(argv[2]);
    }
    return Blt_ConfigureBindings(interp, hboxPtr->bindTable, item, argc - 3, 
	argv + 3);
}

/*
 *----------------------------------------------------------------------
 *
 * CgetOpOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *treePtr;

    if (StringToNode(hboxPtr, argv[3], &treePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return Tk_ConfigureValue(interp, hboxPtr->tkwin, entryConfigSpecs,
	(char *)treePtr->entryPtr, argv[4], 0);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOpOp --
 *
 * 	This procedure is called to process a list of configuration
 *	options database, in order to reconfigure the one of more
 *	entries in the widget.
 *
 *	  .h entryconfigure node node node node option value
 *
 * Results:
 *	A standard Tcl result.  If TCL_ERROR is returned, then
 *	interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for hboxPtr; old resources get freed, if there
 *	were any.  The hypertext is redisplayed.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int nIds, nOpts;
    char **options;
    register int i;
    Tree *treePtr;


    /* Figure out where the option value pairs begin */
    argc -= 3;
    argv += 3;
    for (i = 0; i < argc; i++) {
	if (argv[i][0] == '-') {
	    break;
	}
	if (StringToNode(hboxPtr, argv[i], &treePtr) != TCL_OK) {
	    return TCL_ERROR;	/* Can't find node. */
	}
    }
    nIds = i;			/* Number of element names specified */
    nOpts = argc - i;		/* Number of options specified */
    options = argv + i;		/* Start of options in argv  */

    for (i = 0; i < nIds; i++) {
	StringToNode(hboxPtr, argv[i], &treePtr);
	if (argc == 1) {
	    return Tk_ConfigureInfo(interp, hboxPtr->tkwin, entryConfigSpecs,
		(char *)treePtr->entryPtr, (char *)NULL, 0);
	} else if (argc == 2) {
	    return Tk_ConfigureInfo(interp, hboxPtr->tkwin, entryConfigSpecs,
		(char *)treePtr->entryPtr, argv[2], 0);
	}
	if (ConfigureEntry(hboxPtr, treePtr->entryPtr, nOpts, options,
		TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IsHiddenOpOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
IsHiddenOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *treePtr;

    if (StringToNode(hboxPtr, argv[3], &treePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Blt_SetBooleanResult(interp, IsHidden(treePtr));
    return TCL_OK;
}

/*ARGSUSED*/
static int
IsBeforeOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *n1Ptr, *n2Ptr;

    if ((StringToNode(hboxPtr, argv[3], &n1Ptr) != TCL_OK) ||
	(StringToNode(hboxPtr, argv[4], &n2Ptr) != TCL_OK)) {
	return TCL_ERROR;
    }
    Blt_SetBooleanResult(interp, IsBefore(n1Ptr, n2Ptr));
    return TCL_OK;
}


static int
ScreenToIndex(hboxPtr, x, y)
    Hierbox *hboxPtr;
    int x, y;
{
    Tk_Font font;
    TextStyle ts;
    TextLayout *textPtr;
    Tk_FontMetrics fontMetrics;
    TextFragment *fragPtr;
    int nBytes;
    Entry *entryPtr;
    Tree *treePtr;
    int lineNum;
    register int i;

    treePtr = hboxPtr->focusPtr;
    entryPtr = treePtr->entryPtr;

    if (*entryPtr->labelText == '\0') {
	return 0;
    }
    /*
     * Compute the X-Y offsets of the screen point from the start of
     * label.  Force the offsets to point within the label.
     */
    x -= SCREENX(hboxPtr, entryPtr->worldX) + LABEL_PADX +
	hboxPtr->selBorderWidth;
    y -= SCREENY(hboxPtr, entryPtr->worldY) + LABEL_PADY +
	hboxPtr->selBorderWidth;
    x -= LEVELWIDTH(treePtr->level) + LEVELWIDTH(treePtr->level + 1) + 4;

    font = GETFONT(hboxPtr, entryPtr->labelFont);
    memset(&ts, 0, sizeof(TextStyle));
    ts.font = font;
    ts.justify = TK_JUSTIFY_LEFT;
    ts.shadow.offset = entryPtr->labelShadow.offset;
    textPtr = Blt_GetTextLayout(entryPtr->labelText, &ts);

    if (y < 0) {
	y = 0;
    } else if (y >= textPtr->height) {
	y = textPtr->height - 1;
    }
    Tk_GetFontMetrics(font, &fontMetrics);
    lineNum = y / fontMetrics.linespace;
    fragPtr = textPtr->fragArr + lineNum;

    if (x < 0) {
	nBytes = 0;
    } else if (x >= textPtr->width) {
	nBytes = fragPtr->count;
    } else {
	int newX;
	/* Find the character underneath the pointer. */
	nBytes = Tk_MeasureChars(font, fragPtr->text, fragPtr->count, x, 0,
	    &newX);
	if ((newX < x) && (nBytes < fragPtr->count)) {
	    double fract;
	    int length, charSize;
	    char *next;

	    next = fragPtr->text + nBytes;
#if HAVE_UTF
	    {
		Tcl_UniChar dummy;

		length = Tcl_UtfToUniChar(next, &dummy);
	    }
#else
	    length = 1;
#endif
	    charSize = Tk_TextWidth(font, next, length);
	    fract = ((double)(x - newX) / (double)charSize);
	    if (ROUND(fract)) {
		nBytes += length;
	    }
	}
    }
    for (i = lineNum - 1; i >= 0; i--) {
	fragPtr--;
	nBytes += fragPtr->count + 1;
    }
    Blt_Free(textPtr);
    return nBytes;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetLabelIndex --
 *
 *	Parse an index into an entry and return either its value
 *	or an error.
 *
 * Results:
 *	A standard Tcl result.  If all went well, then *indexPtr is
 *	filled in with the character index (into entryPtr) corresponding to
 *	string.  The index value is guaranteed to lie between 0 and
 *	the number of characters in the string, inclusive.  If an
 *	error occurs then an error message is left in the interp's result.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static int
GetLabelIndex(hboxPtr, entryPtr, string, indexPtr)
    Hierbox *hboxPtr;
    Entry *entryPtr;
    char *string;
    int *indexPtr;
{
    Tcl_Interp *interp = hboxPtr->interp;
    TextEdit *editPtr = &(hboxPtr->labelEdit);
    char c;

    c = string[0];
    if ((c == 'a') && (strcmp(string, "anchor") == 0)) {
	*indexPtr = editPtr->selAnchor;
    } else if ((c == 'e') && (strcmp(string, "end") == 0)) {
	*indexPtr = strlen(entryPtr->labelText);
    } else if ((c == 'i') && (strcmp(string, "insert") == 0)) {
	*indexPtr = editPtr->insertPos;
    } else if ((c == 's') && (strcmp(string, "sel.first") == 0)) {
	if (editPtr->selFirst < 0) {
	    Tcl_AppendResult(interp, "nothing is selected", (char *)NULL);
	    return TCL_ERROR;
	}
	*indexPtr = editPtr->selFirst;
    } else if ((c == 's') && (strcmp(string, "sel.last") == 0)) {
	if (editPtr->selLast < 0) {
	    Tcl_AppendResult(interp, "nothing is selected", (char *)NULL);
	    return TCL_ERROR;
	}
	*indexPtr = editPtr->selLast;
    } else if (c == '@') {
	int x, y;

	if (Blt_GetXY(interp, hboxPtr->tkwin, string, &x, &y) != TCL_OK) {
	    return TCL_ERROR;
	}
	*indexPtr = ScreenToIndex(hboxPtr, x, y);
    } else if (isdigit((int)c)) {
	int number;
	int maxChars;

	if (Tcl_GetInt(interp, string, &number) != TCL_OK) {
	    return TCL_ERROR;
	}
	/* Don't allow the index to point outside the label. */
	maxChars = Tcl_NumUtfChars(entryPtr->labelText, -1);
	if (number < 0) {
	    *indexPtr = 0;
	} else if (number > maxChars) {
	    *indexPtr = strlen(entryPtr->labelText);
	} else {
	    *indexPtr = Tcl_UtfAtIndex(entryPtr->labelText, number) -
		entryPtr->labelText;
	}
    } else {
	Tcl_AppendResult(interp, "bad label index \"", string, "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IndexOpOp --
 *
 *	Returns the numeric index of the given string. Indices can be
 *	one of the following:
 *
 *	"anchor"	Selection anchor.
 *	"end"		End of the label.
 *	"insert"	Insertion cursor.
 *	"sel.first"	First character selected.
 *	"sel.last"	Last character selected.
 *	@x,y		Index at X-Y screen coordinate.
 *	number		Returns the same number.
 *
 * Results:
 *	A standard Tcl result.  If the argument does not represent a
 *	valid label index, then TCL_ERROR is returned and the interpreter
 *	result will contain an error message.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
IndexOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Entry *entryPtr;
    int nBytes, nChars;

    entryPtr = hboxPtr->focusPtr->entryPtr;
    if (GetLabelIndex(hboxPtr, entryPtr, argv[3], &nBytes) != TCL_OK) {
	return TCL_ERROR;
    }
    nChars = Tcl_NumUtfChars(entryPtr->labelText, nBytes);
    Tcl_SetResult(interp, Blt_Itoa(nChars), TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InsertOpOp --
 *
 *	Add new characters to the label of an entry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	New information gets added to editPtr;  it will be redisplayed
 *	soon, but not necessarily immediately.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InsertOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Tree *treePtr;
    Entry *entryPtr;
    TextEdit *editPtr = &(hboxPtr->labelEdit);
    int oldSize, newSize;
    char *string;
    int extra;
    int insertPos;

    if (!hboxPtr->focusEdit) {
	return TCL_OK;		/* Not in edit mode. */
    }
    if (StringToNode(hboxPtr, argv[3], &treePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (treePtr == NULL) {
	return TCL_OK;		/* Not in edit mode. */
    }
    entryPtr = treePtr->entryPtr;
    if (hboxPtr->focusPtr != treePtr) {
	hboxPtr->focusPtr = treePtr;
	editPtr->insertPos = strlen(entryPtr->labelText);
	editPtr->selAnchor = editPtr->selFirst = editPtr->selLast = -1;
    }
    if (GetLabelIndex(hboxPtr, entryPtr, argv[4], &insertPos) != TCL_OK) {
	return TCL_ERROR;
    }
    extra = strlen(argv[5]);
    if (extra == 0) {
	/* Nothing to insert. Move the cursor anyways. */
	editPtr->insertPos = insertPos;
	EventuallyRedraw(hboxPtr);
	return TCL_OK;
    }
    oldSize = strlen(entryPtr->labelText);
    newSize = oldSize + extra;
    string = Blt_Malloc(sizeof(char) * (newSize + 1));

    if (insertPos == oldSize) {	/* Append */
	strcpy(string, entryPtr->labelText);
	strcat(string, argv[5]);
    } else if (insertPos == 0) {/* Prepend */
	strcpy(string, argv[5]);
	strcat(string, entryPtr->labelText);
    } else {			/* Insert into existing. */
	char *left, *right;
	char *p;

	left = entryPtr->labelText;
	right = left + insertPos;
	p = string;
	strncpy(p, left, insertPos);
	p += insertPos;
	strcpy(p, argv[5]);
	p += extra;
	strcpy(p, right);
    }
    /*
     * All indices from the start of the insertion to the end of the
     * string need to be updated.  Simply move the indices down by the
     * number of characters added.
     */
    if (editPtr->selFirst >= insertPos) {
	editPtr->selFirst += extra;
    }
    if (editPtr->selLast > insertPos) {
	editPtr->selLast += extra;
    }
    if ((editPtr->selAnchor > insertPos) || (editPtr->selFirst >= insertPos)) {
	editPtr->selAnchor += extra;
    }
    Blt_Free(entryPtr->labelText);
    entryPtr->labelText = string;

    editPtr->insertPos = insertPos + extra;
    GetCursorLocation(hboxPtr, treePtr);
    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteOpOp --
 *
 *	Remove one or more characters from the label of an entry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets freed, the entry gets modified and (eventually)
 *	redisplayed.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeleteOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    TextEdit *editPtr = &(hboxPtr->labelEdit);
    Tree *treePtr;
    Entry *entryPtr;
    int oldSize, newSize;
    int nDeleted;
    int first, last;
    char *string;
    char *p;

    if (!hboxPtr->focusEdit) {
	return TCL_OK;		/* Not in edit mode. */
    }
    if (StringToNode(hboxPtr, argv[3], &treePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (treePtr == NULL) {
	return TCL_OK;		/* Not in edit mode. */
    }
    entryPtr = treePtr->entryPtr;
    if (hboxPtr->focusPtr != treePtr) {
	hboxPtr->focusPtr = treePtr;
	editPtr->insertPos = strlen(entryPtr->labelText);
	editPtr->selAnchor = editPtr->selFirst = editPtr->selLast = -1;
    }
    if ((GetLabelIndex(hboxPtr, entryPtr, argv[4], &first) != TCL_OK) ||
	(GetLabelIndex(hboxPtr, entryPtr, argv[5], &last) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (first >= last) {
	return TCL_OK;
    }
    if ((!hboxPtr->focusEdit) || (entryPtr == NULL)) {
	return TCL_OK;		/* Not in edit mode. */
    }
    oldSize = strlen(entryPtr->labelText);
    newSize = oldSize - (last - first);
    p = string = Blt_Malloc(sizeof(char) * (newSize + 1));
    strncpy(p, entryPtr->labelText, first);
    p += first;
    strcpy(p, entryPtr->labelText + last);

    Blt_Free(entryPtr->labelText);
    entryPtr->labelText = string;
    nDeleted = last - first + 1;

    /*
     * Since deleting characters compacts the character array, we need to
     * update the various character indices according.  It depends where
     * the index occurs in relation to range of deleted characters:
     *
     *		before		Ignore.
     *		within		Move the index back to the start of the deletion.
     *		after		Subtract off the deleted number of characters,
     *				since the array has been compressed by that
     *				many characters.
     *
     */
    if (editPtr->selFirst >= first) {
	if (editPtr->selFirst >= last) {
	    editPtr->selFirst -= nDeleted;
	} else {
	    editPtr->selFirst = first;
	}
    }
    if (editPtr->selLast >= first) {
	if (editPtr->selLast >= last) {
	    editPtr->selLast -= nDeleted;
	} else {
	    editPtr->selLast = first;
	}
    }
    if (editPtr->selLast <= editPtr->selFirst) {
	editPtr->selFirst = editPtr->selLast = -1;	/* Cut away the entire
						      * selection. */
    }
    if (editPtr->selAnchor >= first) {
	if (editPtr->selAnchor >= last) {
	    editPtr->selAnchor -= nDeleted;
	} else {
	    editPtr->selAnchor = first;
	}
    }
    if (editPtr->insertPos >= first) {
	if (editPtr->insertPos >= last) {
	    editPtr->insertPos -= nDeleted;
	} else {
	    editPtr->insertPos = first;
	}
    }
    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IsOpenOpOp --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
IsOpenOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *treePtr;

    if (StringToNode(hboxPtr, argv[3], &treePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Blt_SetBooleanResult(interp, (treePtr->entryPtr->flags & ENTRY_OPEN));
    return TCL_OK;
}

/*ARGSUSED*/
static int
ChildrenOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Tree *parentPtr, *nodePtr;

    if (StringToNode(hboxPtr, argv[3], &parentPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (argc == 4) {
	Blt_ChainLink *linkPtr;

	for (linkPtr = Blt_ChainFirstLink(parentPtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    nodePtr = Blt_ChainGetValue(linkPtr);
	    Tcl_AppendElement(interp, NodeToString(hboxPtr, nodePtr));
	}
    } else if (argc == 6) {
	Blt_ChainLink *firstPtr, *lastPtr;
	int first, last;
	int nNodes;

	if ((Blt_GetPosition(interp, argv[4], &first) != TCL_OK) ||
	    (Blt_GetPosition(interp, argv[5], &last) != TCL_OK)) {
	    return TCL_ERROR;
	}
	nNodes = Blt_ChainGetLength(parentPtr->chainPtr);
	if (nNodes == 0) {
	    return TCL_OK;
	}
	if ((last == -1) || (last >= nNodes)) {
	    last = nNodes - 1;
	}
	if ((first == -1) || (first >= nNodes)) {
	    first = nNodes - 1;
	} 
	firstPtr = Blt_ChainGetNthLink(parentPtr->chainPtr, first);
	lastPtr = Blt_ChainGetNthLink(parentPtr->chainPtr, last);
	if (first > last) {
	    for ( /*empty*/ ; lastPtr != NULL;
		lastPtr = Blt_ChainPrevLink(lastPtr)) {
		nodePtr = Blt_ChainGetValue(lastPtr);
		Tcl_AppendElement(interp, NodeToString(hboxPtr, nodePtr));
		if (lastPtr == firstPtr) {
		    break;
		}
	    }
	} else {
	    for ( /*empty*/ ; firstPtr != NULL;
		firstPtr = Blt_ChainNextLink(firstPtr)) {
		nodePtr = Blt_ChainGetValue(firstPtr);
		Tcl_AppendElement(interp, NodeToString(hboxPtr, nodePtr));
		if (firstPtr == lastPtr) {
		    break;
		}
	    }
	}
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0], " ",
	    argv[1], " ", argv[2], " index ?first last?", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SizeOpOp --
 *
 *	Counts the number of entries at this node.
 *
 * Results:
 *	A standard Tcl result.  If an error occurred TCL_ERROR is
 *	returned and interp->result will contain an error message.
 *	Otherwise, TCL_OK is returned and interp->result contains
 *	the number of entries.
 *
 *----------------------------------------------------------------------
 */
static int
SizeOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int length;
    Tree *rootPtr;
    int *sumPtr = (int *)&(hboxPtr->clientData);

    length = strlen(argv[3]);
    if ((argv[3][0] == '-') && (length > 1) &&
	(strncmp(argv[3], "-recurse", length) == 0)) {
	argv++, argc--;
    }
    if (argc == 3) {
	Tcl_AppendResult(interp, "missing node argument: should be \"",
	    argv[0], " entry open node\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (StringToNode(hboxPtr, argv[3], &rootPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    *sumPtr = 0;
    if (ApplyToTree(hboxPtr, rootPtr, SizeOfNode, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, Blt_Itoa(*sumPtr), TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * EntryOp --
 *
 *	This procedure handles entry operations.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */

static Blt_OpSpec entryOperSpecs[] =
{
    {"activate", 1, (Blt_Op)ActivateOpOp, 4, 4, "node",},
    {"cget", 2, (Blt_Op)CgetOpOp, 5, 5, "node option",},
    {"children", 2, (Blt_Op)ChildrenOpOp, 4, 6, "node first last",},
    {"configure", 2, (Blt_Op)ConfigureOpOp, 4, 0,
	"node ?node...? ?option value?...",},
    {"delete", 1, (Blt_Op)DeleteOpOp, 6, 6, "node first last"},
    {"highlight", 1, (Blt_Op)ActivateOpOp, 4, 4, "node",},
    {"index", 3, (Blt_Op)IndexOpOp, 6, 6, "index"},
    {"insert", 3, (Blt_Op)InsertOpOp, 6, 6, "node index string"},
    {"isbefore", 3, (Blt_Op)IsBeforeOpOp, 5, 5, "node node",},
    {"ishidden", 3, (Blt_Op)IsHiddenOpOp, 4, 4, "node",},
    {"isopen", 3, (Blt_Op)IsOpenOpOp, 4, 4, "node",},
    {"size", 1, (Blt_Op)SizeOpOp, 4, 5, "?-recurse? node",},
};
static int nEntrySpecs = sizeof(entryOperSpecs) / sizeof(Blt_OpSpec);

static int
EntryOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nEntrySpecs, entryOperSpecs, BLT_OP_ARG2, argc, 
	argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (hboxPtr, interp, argc, argv);
    return result;
}

/*ARGSUSED*/
static int
ExactCompare(interp, name, pattern)
    Tcl_Interp *interp;		/* Not used. */
    char *name;
    char *pattern;
{
    return (strcmp(name, pattern) == 0);
}

/*ARGSUSED*/
static int
GlobCompare(interp, name, pattern)
    Tcl_Interp *interp;		/* Not used. */
    char *name;
    char *pattern;
{
    return Tcl_StringMatch(name, pattern);
}

static int
RegexpCompare(interp, name, pattern)
    Tcl_Interp *interp;
    char *name;
    char *pattern;
{
    return Tcl_RegExpMatch(interp, name, pattern);
}

/*
 *----------------------------------------------------------------------
 *
 * FindOp --
 *
 *	Find one or more nodes based upon the pattern provided.
 *
 * Results:
 *	A standard Tcl result.  The interpreter result will contain a
 *	list of the node serial identifiers.
 *
 *----------------------------------------------------------------------
 */
static int
FindOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register Tree *treePtr;
    Tree *firstPtr, *lastPtr;
    int nMatches, maxMatches;
    char c;
    int length;
    CompareProc *compareProc;
    IterProc *nextProc;
    int invertMatch;		/* normal search mode (matching entries) */
    char *namePattern, *fullPattern;
    char *execCmd;
    register int i;
    int result;
    char *pattern;
    char *option;
    char *value;
    Tcl_DString dString, pathString;
    Blt_List optionList;
    register Blt_ListNode node;

    invertMatch = FALSE;
    maxMatches = 0;
    execCmd = namePattern = fullPattern = NULL;
    compareProc = ExactCompare;
    nextProc = NextNode;
    optionList = Blt_ListCreate(BLT_STRING_KEYS);

    /*
     * Step 1:  Process flags for find operation.
     */
    for (i = 2; i < argc; i++) {
	if (argv[i][0] != '-') {
	    break;
	}
	option = argv[i] + 1;
	length = strlen(option);
	c = option[0];
	if ((c == 'e') && (length > 2) &&
	    (strncmp(option, "exact", length) == 0)) {
	    compareProc = ExactCompare;
	} else if ((c == 'g') && (strncmp(option, "glob", length) == 0)) {
	    compareProc = GlobCompare;
	} else if ((c == 'r') && (strncmp(option, "regexp", length) == 0)) {
	    compareProc = RegexpCompare;
	} else if ((c == 'n') && (length > 1) &&
	    (strncmp(option, "nonmatching", length) == 0)) {
	    invertMatch = TRUE;
	} else if ((c == 'n') && (length > 1) &&
	    (strncmp(option, "name", length) == 0)) {
	    if ((i + 1) == argc) {
		goto missingArg;
	    }
	    i++;
	    namePattern = argv[i];
	} else if ((c == 'f') && (strncmp(option, "full", length) == 0)) {
	    if ((i + 1) == argc) {
		goto missingArg;
	    }
	    i++;
	    fullPattern = argv[i];
	} else if ((c == 'e') && (length > 2) &&
	    (strncmp(option, "exec", length) == 0)) {
	    if ((i + 1) == argc) {
		goto missingArg;
	    }
	    i++;
	    execCmd = argv[i];
	} else if ((c == 'c') && (strncmp(option, "count", length) == 0)) {
	    if ((i + 1) == argc) {
		goto missingArg;
	    }
	    i++;
	    if (Tcl_GetInt(interp, argv[i], &maxMatches) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (maxMatches < 0) {
		Tcl_AppendResult(interp, "bad match count \"", argv[i],
		    "\": should be a positive number", (char *)NULL);
		Blt_ListDestroy(optionList);
		return TCL_ERROR;
	    }
	} else if ((option[0] == '-') && (option[1] == '\0')) {
	    break;
	} else {
	    /*
	     * Verify that the switch is actually an node configuration
	     * option.
	     */
	    if (Tk_ConfigureValue(interp, hboxPtr->tkwin, entryConfigSpecs,
		    (char *)hboxPtr->rootPtr->entryPtr, argv[i], 0) != TCL_OK) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "bad find switch \"", argv[i], "\"",
		    (char *)NULL);
		Blt_ListDestroy(optionList);
		return TCL_ERROR;
	    }
	    if ((i + 1) == argc) {
		goto missingArg;
	    }
	    /* Save the option in the list of configuration options */
	    node = Blt_ListGetNode(optionList, argv[i]);
	    if (node == NULL) {
		node = Blt_ListCreateNode(optionList, argv[i]);
		Blt_ListAppendNode(optionList, node);
	    }
	    Blt_ListSetValue(node, argv[i + 1]);
	    i++;
	}
    }

    if ((argc - i) > 2) {
	Blt_ListDestroy(optionList);
	Tcl_AppendResult(interp, "too many args", (char *)NULL);
	return TCL_ERROR;
    }
    /*
     * Step 2:  Find the range of the search.  Check the order of two
     *		nodes and arrange the search accordingly.
     *
     *	Note:	Be careful to treat "end" as the end of all nodes, instead
     *		of the end of visible nodes.  That way, we can search the
     *		entire tree, even if the last folder is closed.
     */
    firstPtr = hboxPtr->rootPtr;/* Default to root node */
    lastPtr = EndNode(firstPtr, 0);

    if (i < argc) {
	if ((argv[i][0] == 'e') && (strcmp(argv[i], "end") == 0)) {
	    firstPtr = EndNode(hboxPtr->rootPtr, 0);
	} else if (StringToNode(hboxPtr, argv[i], &firstPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	i++;
    }
    if (i < argc) {
	if ((argv[i][0] == 'e') && (strcmp(argv[i], "end") == 0)) {
	    lastPtr = EndNode(hboxPtr->rootPtr, 0);
	} else if (StringToNode(hboxPtr, argv[i], &lastPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (IsBefore(lastPtr, firstPtr)) {
	nextProc = LastNode;
    }
    nMatches = 0;

    /*
     * Step 3:	Search through the tree and look for nodes that match the
     *		current pattern specifications.  Save the name of each of
     *		the matching nodes.
     */
    Tcl_DStringInit(&dString);
    for (treePtr = firstPtr; treePtr != NULL;
	treePtr = (*nextProc) (treePtr, 0)) {
	if (namePattern != NULL) {
	    result = (*compareProc) (interp, treePtr->nameId, namePattern);
	    if (result == invertMatch) {
		goto nextNode;	/* Failed to match */
	    }
	}
	if (fullPattern != NULL) {
	    GetFullPath(treePtr, hboxPtr->separator, &pathString);
	    result = (*compareProc) (interp, Tcl_DStringValue(&pathString),
		fullPattern);
	    Tcl_DStringFree(&pathString);
	    if (result == invertMatch) {
		goto nextNode;	/* Failed to match */
	    }
	}
	for (node = Blt_ListFirstNode(optionList); node != NULL;
	    node = Blt_ListNextNode(node)) {
	    option = (char *)Blt_ListGetKey(node);
	    Tcl_ResetResult(interp);
	    if (Tk_ConfigureValue(interp, hboxPtr->tkwin, entryConfigSpecs,
		    (char *)treePtr->entryPtr, option, 0) != TCL_OK) {
		goto error;	/* This shouldn't happen. */
	    }
	    pattern = (char *)Blt_ListGetValue(node);
	    value = (char *)Tcl_GetStringResult(interp);
	    result = (*compareProc) (interp, value, pattern);
	    if (result == invertMatch) {
		goto nextNode;	/* Failed to match */
	    }
	}
	/*
	 * As unlikely as it sounds, the "exec" callback may delete this
	 * node. We need to preverse it until we're not using it anymore
	 * (i.e. no longer accessing its fields).
	 */
	Tcl_Preserve(treePtr);
	if (execCmd != NULL) {
	    Tcl_DString cmdString;

	    PercentSubst(hboxPtr, treePtr, execCmd, &cmdString);
	    result = Tcl_GlobalEval(interp, Tcl_DStringValue(&cmdString));
	    Tcl_DStringFree(&cmdString);
	    if (result != TCL_OK) {
		Tcl_Release(treePtr);
		goto error;
	    }
	}
	/* Finally, save the matching node name. */
	Tcl_DStringAppendElement(&dString, NodeToString(hboxPtr, treePtr));
	Tcl_Release(treePtr);
	nMatches++;
	if ((nMatches == maxMatches) && (maxMatches > 0)) {
	    break;
	}
      nextNode:
	if (treePtr == lastPtr) {
	    break;
	}
    }
    Tcl_ResetResult(interp);
    Blt_ListDestroy(optionList);
    Tcl_DStringResult(interp, &dString);
    return TCL_OK;

  missingArg:
    Tcl_AppendResult(interp, "missing argument for find option \"", argv[i],
	"\"", (char *)NULL);
  error:
    Tcl_DStringFree(&dString);
    Blt_ListDestroy(optionList);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * GetOp --
 *
 *	Converts one or more node identifiers to its path component.
 *	The path may be either the single entry name or the full path
 *	of the entry.
 *
 * Results:
 *	A standard Tcl result.  The interpreter result will contain a
 *	list of the convert names.
 *
 *----------------------------------------------------------------------
 */
static int
GetOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int fullName;
    Tree *treePtr;
    Tcl_DString dString;
    Tcl_DString pathString;
    register int i;

    fullName = FALSE;
    if ((argc > 2) && (argv[2][0] == '-') && (strcmp(argv[2], "-full") == 0)) {
	fullName = TRUE;
	argv++, argc--;
    }
    Tcl_DStringInit(&dString);
    Tcl_DStringInit(&pathString);
    for (i = 2; i < argc; i++) {
	treePtr = hboxPtr->focusPtr;
	if (GetNode(hboxPtr, argv[i], &treePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (treePtr == NULL) {
	    Tcl_DStringAppendElement(&dString, "");
	    continue;
	}
	if (fullName) {
	    GetFullPath(treePtr, hboxPtr->separator, &pathString);
	    Tcl_DStringAppendElement(&dString, Tcl_DStringValue(&pathString));
	} else {
	    Tcl_DStringAppendElement(&dString, treePtr->nameId);
	}
    }
    Tcl_DStringFree(&pathString);
    Tcl_DStringResult(interp, &dString);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SearchAndApplyToTree --
 *
 *	Searches through the current tree and applies a procedure
 *	to matching nodes.  The search specification is taken from
 *	the following command-line arguments:
 *
 *      ?-exact? ?-glob? ?-regexp? ?-nonmatching?
 *      ?-data string?
 *      ?-name string?
 *      ?-full string?
 *      ?--?
 *      ?inode...?
 *
 * Results:
 *	A standard Tcl result.  If the result is valid, and if the
 *      nonmatchPtr is specified, it returns a boolean value
 *      indicating whether or not the search was inverted.  This
 *      is needed to fix things properly for the "hide nonmatching"
 *      case.
 *
 *----------------------------------------------------------------------
 */
static int
SearchAndApplyToTree(hboxPtr, interp, argc, argv, proc, nonMatchPtr)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
    ApplyProc *proc;
    int *nonMatchPtr;		/* returns: inverted search indicator */
{
    CompareProc *compareProc;
    int invertMatch;		/* normal search mode (matching entries) */
    char *namePattern, *fullPattern;
    register int i;
    int length;
    int result;
    char *option, *pattern;
    char *value;
    Tree *treePtr;
    char c;
    Blt_List optionList;
    register Blt_ListNode node;

    optionList = Blt_ListCreate(BLT_STRING_KEYS);
    invertMatch = FALSE;
    namePattern = fullPattern = NULL;
    compareProc = ExactCompare;
    for (i = 2; i < argc; i++) {
	if (argv[i][0] != '-') {
	    break;
	}
	option = argv[i] + 1;
	length = strlen(option);
	c = option[0];
	if ((c == 'e') && (strncmp(option, "exact", length) == 0)) {
	    compareProc = ExactCompare;
	} else if ((c == 'g') && (strncmp(option, "glob", length) == 0)) {
	    compareProc = GlobCompare;
	} else if ((c == 'r') && (strncmp(option, "regexp", length) == 0)) {
	    compareProc = RegexpCompare;
	} else if ((c == 'n') && (length > 1) &&
	    (strncmp(option, "nonmatching", length) == 0)) {
	    invertMatch = TRUE;
	} else if ((c == 'f') && (strncmp(option, "full", length) == 0)) {
	    if ((i + 1) == argc) {
		goto missingArg;
	    }
	    i++;
	    fullPattern = argv[i];
	} else if ((c == 'n') && (length > 1) &&
	    (strncmp(option, "name", length) == 0)) {
	    if ((i + 1) == argc) {
		goto missingArg;
	    }
	    i++;
	    namePattern = argv[i];
	} else if ((option[0] == '-') && (option[1] == '\0')) {
	    break;
	} else {
	    /*
	     * Verify that the switch is actually an entry configuration option.
	     */
	    if (Tk_ConfigureValue(interp, hboxPtr->tkwin, entryConfigSpecs,
		    (char *)hboxPtr->rootPtr->entryPtr, argv[i], 0) != TCL_OK) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "bad switch \"", argv[i],
		    "\": must be -exact, -glob, -regexp, -name, -full, or -nonmatching",
		    (char *)NULL);
		return TCL_ERROR;
	    }
	    if ((i + 1) == argc) {
		goto missingArg;
	    }
	    /* Save the option in the list of configuration options */
	    node = Blt_ListGetNode(optionList, argv[i]);
	    if (node == NULL) {
		node = Blt_ListCreateNode(optionList, argv[i]);
		Blt_ListAppendNode(optionList, node);
	    }
	    Blt_ListSetValue(node, argv[i + 1]);
	}
    }

    if ((namePattern != NULL) || (fullPattern != NULL) ||
	(Blt_ListGetLength(optionList) > 0)) {
	/*
	 * Search through the tree and look for nodes that match the
	 * current spec.  Apply the input procedure to each of the
	 * matching nodes.
	 */
	for (treePtr = hboxPtr->rootPtr; treePtr != NULL;
	    treePtr = NextNode(treePtr, 0)) {

	    if (namePattern != NULL) {
		result = (*compareProc) (interp, treePtr->nameId, namePattern);
		if (result == invertMatch) {
		    continue;	/* Failed to match */
		}
	    }
	    if (fullPattern != NULL) {
		Tcl_DString dString;

		GetFullPath(treePtr, hboxPtr->separator, &dString);
		result = (*compareProc) (interp, Tcl_DStringValue(&dString),
		    fullPattern);
		Tcl_DStringFree(&dString);
		if (result == invertMatch) {
		    continue;	/* Failed to match */
		}
	    }
	    for (node = Blt_ListFirstNode(optionList); node != NULL;
		node = Blt_ListNextNode(node)) {
		option = (char *)Blt_ListGetKey(node);
		Tcl_ResetResult(interp);
		if (Tk_ConfigureValue(interp, hboxPtr->tkwin, entryConfigSpecs,
			(char *)treePtr->entryPtr, option, 0) != TCL_OK) {
		    return TCL_ERROR;	/* This shouldn't happen. */
		}
		pattern = (char *)Blt_ListGetValue(node);
		value = (char *)Tcl_GetStringResult(interp);
		result = (*compareProc) (interp, value, pattern);
		if (result == invertMatch) {
		    continue;	/* Failed to match */
		}
	    }
	    /* Finally, apply the procedure to the node */
	    (*proc) (hboxPtr, treePtr);
	}
	Tcl_ResetResult(interp);
	Blt_ListDestroy(optionList);
    }
    /*
     * Apply the procedure to nodes that have been specified
     * individually.
     */
    for ( /*empty*/ ; i < argc; i++) {
	if ((argv[i][0] == 'a') && (strcmp(argv[i], "all") == 0)) {
	    return ApplyToTree(hboxPtr, hboxPtr->rootPtr, proc, APPLY_RECURSE);
	}
	if (StringToNode(hboxPtr, argv[i], &treePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((*proc) (hboxPtr, treePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    if (nonMatchPtr != NULL) {
	*nonMatchPtr = invertMatch;	/* return "inverted search" status */
    }
    return TCL_OK;

  missingArg:
    Blt_ListDestroy(optionList);
    Tcl_AppendResult(interp, "missing pattern for search option \"", argv[i],
	"\"", (char *)NULL);
    return TCL_ERROR;

}

/*
 *----------------------------------------------------------------------
 *
 * HideOp --
 *
 *	Hides one or more nodes.  Nodes can be specified by their
 *      inode, or by matching a name or data value pattern.  By
 *      default, the patterns are matched exactly.  They can also
 *      be matched using glob-style and regular expression rules.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static int
HideOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int status, nonmatching;

    status = SearchAndApplyToTree(hboxPtr, interp, argc, argv, UnmapNode,
	&nonmatching);

    if (status != TCL_OK) {
	return TCL_ERROR;
    }
    /*
     * If this was an inverted search, scan back through the
     * tree and make sure that the parents for all visible
     * nodes are also visible.  After all, if a node is supposed
     * to be visible, its parent can't be hidden.
     */
    if (nonmatching) {
	ApplyToTree(hboxPtr, hboxPtr->rootPtr, MapAncestors, APPLY_RECURSE);
    }
    /*
     * Make sure that selections arme cleared from any hidden
     * nodes.  This wasn't done earlier--we had to delay it until
     * we fixed the visibility status for the parents.
     */
    ApplyToTree(hboxPtr, hboxPtr->rootPtr, FixUnmappedSelections,
	APPLY_RECURSE);

    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ShowOp --
 *
 *	Mark one or more nodes to be exposed.  Nodes can be specified
 *	by their inode, or by matching a name or data value pattern.  By
 *      default, the patterns are matched exactly.  They can also
 *      be matched using glob-style and regular expression rules.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static int
ShowOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (SearchAndApplyToTree(hboxPtr, interp, argc, argv, MapNode,
	    (int *)NULL) != TCL_OK) {
	return TCL_ERROR;
    }
    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IndexOp --
 *
 *	Converts one of more words representing indices of the entries
 *	in the hierarchy widget to their respective serial identifiers.
 *
 * Results:
 *	A standard Tcl result.  Interp->result will contain the
 *	identifier of each inode found. If an inode could not be found,
 *	then the serial identifier will be the empty string.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
IndexOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *nodePtr, *rootPtr;

    rootPtr = hboxPtr->focusPtr;
    if ((argv[2][0] == '-') && (strcmp(argv[2], "-at") == 0)) {
	if (StringToNode(hboxPtr, argv[3], &rootPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	argv += 2, argc -= 2;
    }
    if (argc > 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " index ?-at index? index\"", (char *)NULL);
	return TCL_ERROR;
    }
    nodePtr = rootPtr;
    if ((GetNode(hboxPtr, argv[2], &nodePtr) == TCL_OK) && (nodePtr != NULL)) {
	Tcl_SetResult(interp, NodeToString(hboxPtr, nodePtr), TCL_VOLATILE);
    } else {
	Tcl_SetResult(interp, "", TCL_STATIC);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InsertOp --
 *
 *	Add new entries into a hierarchy.  If no node is specified,
 *	new entries will be added to the root of the hierarchy.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InsertOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *rootPtr, *nodePtr, *parentPtr;
    int position;
    int level, count;
    char *path;
    Tcl_DString dString;
    register int i, l;
    int nOpts;
    char **options;
    char **nameArr;

    rootPtr = hboxPtr->rootPtr;
    if ((argv[2][0] == '-') && (strcmp(argv[2], "-at") == 0)) {
	if (StringToNode(hboxPtr, argv[3], &rootPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	argv += 2, argc -= 2;
    }
    if (Blt_GetPosition(hboxPtr->interp, argv[2], &position) != TCL_OK) {
	return TCL_ERROR;
    }
    argc -= 3, argv += 3;

    /*
     * Count the pathnames that follow.  Count the arguments until we
     * spot one that looks like a configuration option (i.e. starts
     * with a minus ("-")).
     */
    for (count = 0; count < argc; count++) {
	if (argv[count][0] == '-') {
	    break;
	}
    }
    nOpts = argc - count;
    options = argv + count;

    Tcl_DStringInit(&dString);
    for (i = 0; i < count; i++) {
	path = argv[i];
	if (hboxPtr->trimLeft != NULL) {
	    register char *p, *s;

	    /* Trim off leading character string if one exists. */
	    for (p = path, s = hboxPtr->trimLeft; *s != '\0'; s++, p++) {
		if (*p != *s) {
		    break;
		}
	    }
	    if (*s == '\0') {
		path = p;
	    }
	}
	/*
	 * Split the path and find the parent node of the path.
	 */
	nameArr = &path;
	level = 1;
	if (hboxPtr->separator == SEPARATOR_LIST) {
	    if (Tcl_SplitList(interp, path, &level, &nameArr) != TCL_OK) {
		goto error;
	    }
	} else if (hboxPtr->separator != SEPARATOR_NONE) {
	    if (SplitPath(hboxPtr, path, &level, &nameArr) != TCL_OK) {
		goto error;
	    }
	}
	if (level == 0) {
	    continue;		/* Root already exists. */
	}
	parentPtr = rootPtr;
	level--;
	for (l = 0; l < level; l++) {
	    nodePtr = FindComponent(parentPtr, nameArr[l]);
	    if (nodePtr == NULL) {
		if (!hboxPtr->autoCreate) {
		    Tcl_AppendResult(interp, "can't find path component \"",
			nameArr[l], "\" in \"", path, "\"", (char *)NULL);
		    goto error;
		}
		nodePtr = CreateNode(hboxPtr, parentPtr, APPEND, nameArr[l]);
	    }
	    parentPtr = nodePtr;
	}
	nodePtr = NULL;
	if (!hboxPtr->allowDuplicates) {
	    nodePtr = FindComponent(parentPtr, nameArr[level]);
	}
	if (nodePtr == NULL) {
	    nodePtr = CreateNode(hboxPtr, parentPtr, position, nameArr[level]);
	    if (nodePtr == NULL) {
		goto error;
	    }
	    if (ConfigureEntry(hboxPtr, nodePtr->entryPtr, nOpts, options,
		    TK_CONFIG_ARGV_ONLY) != TCL_OK) {
		DeleteNode(hboxPtr, nodePtr);
		goto error;
	    }
	    Tcl_DStringAppendElement(&dString, NodeToString(hboxPtr, nodePtr));
	} else {
	    if (ConfigureEntry(hboxPtr, nodePtr->entryPtr, nOpts, options,
		    0) != TCL_OK) {
		goto error;
	    }
	}
	if (nameArr != &path) {
	    Blt_Free(nameArr);
	}
    }
    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL | HIERBOX_DIRTY);
    EventuallyRedraw(hboxPtr);
    Tcl_DStringResult(hboxPtr->interp, &dString);
    return TCL_OK;
  error:
    if (nameArr != &path) {
	Blt_Free(nameArr);
    }
    Tcl_DStringFree(&dString);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteOp --
 *
 *	Deletes nodes from the hierarchy. Deletes either a range of
 *	entries from a hierarchy or a single node (except root).
 *	In all cases, nodes are removed recursively.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeleteOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *treePtr;
    Blt_ChainLink *linkPtr;
    Blt_ChainLink *firstPtr, *lastPtr, *nextPtr;

    if (argc == 2) {
	return TCL_OK;
    }
    if (StringToNode(hboxPtr, argv[2], &treePtr) != TCL_OK) {
	return TCL_ERROR;	/* Node or path doesn't already exist */
    }
    firstPtr = lastPtr = NULL;
    switch (argc) {
    case 3:
	/*
	 * Delete a single hierarchy. If the node specified is root,
	 * delete only the children.
	 */
	if (treePtr != hboxPtr->rootPtr) {
	    DestroyTree(hboxPtr, treePtr);	/* Don't delete root */
	    goto done;
	}
	firstPtr = Blt_ChainFirstLink(treePtr->chainPtr);
	lastPtr = Blt_ChainLastLink(treePtr->chainPtr);
	break;

    case 4:
	/*
	 * Delete a single node from hierarchy specified by its
	 * numeric position.
	 */
	{
	    int position;

	    if (Blt_GetPosition(interp, argv[3], &position) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (position >= Blt_ChainGetLength(treePtr->chainPtr)) {
		return TCL_OK;	/* Bad first index */
	    }
	    if (position == APPEND) {
		linkPtr = Blt_ChainLastLink(treePtr->chainPtr);
	    } else {
		linkPtr = Blt_ChainGetNthLink(treePtr->chainPtr, position);
	    }
	    firstPtr = lastPtr = linkPtr;
	}
	break;

    case 5:
	/*
	 * Delete range of nodes in hierarchy specified by first/last
	 * positions.
	 */
	{
	    int first, last;
	    int nEntries;

	    if ((Blt_GetPosition(interp, argv[3], &first) != TCL_OK) ||
		(Blt_GetPosition(interp, argv[4], &last) != TCL_OK)) {
		return TCL_ERROR;
	    }
	    nEntries = Blt_ChainGetLength(treePtr->chainPtr);
	    if (nEntries == 0) {
		return TCL_OK;
	    }
	    if (first == APPEND) {
		first = nEntries - 1;
	    }
	    if (first >= nEntries) {
		Tcl_AppendResult(interp, "first position \"", argv[3],
		    " is out of range", (char *)NULL);
		return TCL_ERROR;
	    }
	    if ((last == APPEND) || (last >= nEntries)) {
		last = nEntries - 1;
	    }
	    if (first > last) {
		Tcl_AppendResult(interp, "bad range: \"", argv[3],
		    " > ", argv[4], "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    firstPtr = Blt_ChainGetNthLink(treePtr->chainPtr, first);
	    lastPtr = Blt_ChainGetNthLink(treePtr->chainPtr, last);
	}
	break;
    }
    for (linkPtr = firstPtr; linkPtr != NULL; linkPtr = nextPtr) {
	nextPtr = Blt_ChainNextLink(linkPtr);
	treePtr = Blt_ChainGetValue(linkPtr);
	DestroyTree(hboxPtr, treePtr);
	if (linkPtr == lastPtr) {
	    break;
	}
    }
  done:
    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MoveOp --
 *
 *	Move an entry into a new location in the hierarchy.
 *
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
MoveOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *srcPtr, *destPtr, *parentPtr;
    char c;
    int action;

#define MOVE_INTO	(1<<0)
#define MOVE_BEFORE	(1<<1)
#define MOVE_AFTER	(1<<2)
    if (StringToNode(hboxPtr, argv[2], &srcPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    c = argv[3][0];
    if ((c == 'i') && (strcmp(argv[3], "into") == 0)) {
	action = MOVE_INTO;
    } else if ((c == 'b') && (strcmp(argv[3], "before") == 0)) {
	action = MOVE_BEFORE;
    } else if ((c == 'a') && (strcmp(argv[3], "after") == 0)) {
	action = MOVE_AFTER;
    } else {
	Tcl_AppendResult(interp, "bad position \"", argv[3],
	    "\": should be into, before, or after", (char *)NULL);
	return TCL_ERROR;
    }
    if (StringToNode(hboxPtr, argv[4], &destPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Verify they aren't ancestors. */
    if (IsAncestor(srcPtr, destPtr)) {
	Tcl_AppendResult(interp, "can't move node: \"", argv[2],
	    "\" is an ancestor of \"", argv[4], "\"", (char *)NULL);
	return TCL_ERROR;
    }
    parentPtr = destPtr->parentPtr;
    if (parentPtr == NULL) {
	action = MOVE_INTO;
    }
    Blt_ChainUnlinkLink(srcPtr->parentPtr->chainPtr, srcPtr->linkPtr);
    switch (action) {
    case MOVE_INTO:
	Blt_ChainLinkBefore(destPtr->chainPtr, srcPtr->linkPtr,
	    (Blt_ChainLink *) NULL);
	parentPtr = destPtr;
	break;

    case MOVE_BEFORE:
	Blt_ChainLinkBefore(parentPtr->chainPtr, srcPtr->linkPtr,
	    destPtr->linkPtr);
	break;

    case MOVE_AFTER:
	Blt_ChainLinkAfter(parentPtr->chainPtr, srcPtr->linkPtr,
	    destPtr->linkPtr);
	break;
    }
    srcPtr->parentPtr = parentPtr;
    srcPtr->level = parentPtr->level + 1;
    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL | HIERBOX_DIRTY);
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
NearestOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    ButtonAttributes *buttonPtr = &(hboxPtr->button);
    int x, y;			/* Screen coordinates of the test point. */
    register Entry *entryPtr;
    register Tree *treePtr;

    if ((Tk_GetPixels(interp, hboxPtr->tkwin, argv[2], &x) != TCL_OK) ||
	(Tk_GetPixels(interp, hboxPtr->tkwin, argv[3], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (hboxPtr->nVisible == 0) {
	return TCL_OK;
    }
    treePtr = NearestNode(hboxPtr, x, y, TRUE);
    if (treePtr == NULL) {
	return TCL_OK;
    }
    x = WORLDX(hboxPtr, x);
    y = WORLDY(hboxPtr, y);
    entryPtr = treePtr->entryPtr;
    if (argc > 4) {
	char *where;
	int labelX;

	where = "";
	if (entryPtr->flags & ENTRY_BUTTON) {
	    int buttonX, buttonY;

	    buttonX = entryPtr->worldX + entryPtr->buttonX;
	    buttonY = entryPtr->worldY + entryPtr->buttonY;
	    if ((x >= buttonX) && (x < (buttonX + buttonPtr->width)) &&
		(y >= buttonY) && (y < (buttonY + buttonPtr->height))) {
		where = "gadget";
	    }
	}
	labelX = entryPtr->worldX + LEVELWIDTH(treePtr->level);
	if ((x >= labelX) &&
	    (x < (labelX + LEVELWIDTH(treePtr->level + 1) + entryPtr->width))) {
	    where = "select";
	}
	if (Tcl_SetVar(interp, argv[4], where, TCL_LEAVE_ERR_MSG) == NULL) {
	    return TCL_ERROR;
	}
    }
    Tcl_SetResult(interp, NodeToString(hboxPtr, treePtr), TCL_VOLATILE);
    return TCL_OK;
}

/*ARGSUSED*/
static int
OpenOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Tree *rootPtr;
    unsigned int flags;
    register int i;

    flags = 0;
    if (argc > 2) {
	int length;

	length = strlen(argv[2]);
	if ((argv[2][0] == '-') && (length > 1) &&
	    (strncmp(argv[2], "-recurse", length) == 0)) {
	    argv++, argc--;
	    flags |= APPLY_RECURSE;
	}
    }
    for (i = 2; i < argc; i++) {
	rootPtr = hboxPtr->focusPtr;
	if (GetNode(hboxPtr, argv[i], &rootPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (rootPtr == NULL) {
	    continue;
	}
	ExposeAncestors(rootPtr);	/* Also make sure that all the ancestors
					 * of this node are also not hidden. */
	if (ApplyToTree(hboxPtr, rootPtr, OpenNode, flags) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RangeOp --
 *
 *	Returns the node identifiers in a given range.
 *
 *----------------------------------------------------------------------
 */
static int
RangeOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Tree *firstPtr, *lastPtr;
    register Tree *treePtr;
    unsigned int flags;
    int length;

    flags = 0;
    length = strlen(argv[2]);
    if ((argv[2][0] == '-') && (length > 1) &&
	(strncmp(argv[2], "-open", length) == 0)) {
	argv++, argc--;
	flags |= ENTRY_OPEN;
    }
    if (StringToNode(hboxPtr, argv[2], &firstPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    lastPtr = EndNode(firstPtr, flags);
    if (argc > 3) {
	if (StringToNode(hboxPtr, argv[3], &lastPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (flags & ENTRY_OPEN) {
	if (IsHidden(firstPtr)) {
	    Tcl_AppendResult(interp, "first node \"", argv[2], "\" is hidden.",
		(char *)NULL);
	    return TCL_ERROR;
	}
	if (IsHidden(lastPtr)) {
	    Tcl_AppendResult(interp, "last node \"", argv[3], "\" is hidden.",
		(char *)NULL);
	    return TCL_ERROR;
	}
    }
    /*
     * The relative order of the first/last markers determines the
     * direction.
     */
    if (IsBefore(lastPtr, firstPtr)) {
	for (treePtr = lastPtr; treePtr != NULL;
	    treePtr = LastNode(treePtr, flags)) {
	    Tcl_AppendElement(interp, NodeToString(hboxPtr, treePtr));
	    if (treePtr == firstPtr) {
		break;
	    }
	}
    } else {
	for (treePtr = firstPtr; treePtr != NULL;
	    treePtr = NextNode(treePtr, flags)) {
	    Tcl_AppendElement(interp, NodeToString(hboxPtr, treePtr));
	    if (treePtr == lastPtr) {
		break;
	    }
	}
    }
    return TCL_OK;
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
ScanOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    int x, y;
    char c;
    unsigned int length;
    int oper;

#define SCAN_MARK		1
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
    if ((Tk_GetPixels(interp, hboxPtr->tkwin, argv[3], &x) != TCL_OK) ||
	(Tk_GetPixels(interp, hboxPtr->tkwin, argv[4], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (oper == SCAN_MARK) {
	hboxPtr->scanAnchorX = x;
	hboxPtr->scanAnchorY = y;
	hboxPtr->scanX = hboxPtr->xOffset;
	hboxPtr->scanY = hboxPtr->yOffset;
    } else {
	int worldX, worldY;
	int dx, dy;

	dx = hboxPtr->scanAnchorX - x;
	dy = hboxPtr->scanAnchorY - y;
	worldX = hboxPtr->scanX + (10 * dx);
	worldY = hboxPtr->scanY + (10 * dy);

	if (worldX < 0) {
	    worldX = 0;
	} else if (worldX >= hboxPtr->worldWidth) {
	    worldX = hboxPtr->worldWidth - hboxPtr->xScrollUnits;
	}
	if (worldY < 0) {
	    worldY = 0;
	} else if (worldY >= hboxPtr->worldHeight) {
	    worldY = hboxPtr->worldHeight - hboxPtr->yScrollUnits;
	}
	hboxPtr->xOffset = worldX;
	hboxPtr->yOffset = worldY;
	hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
	EventuallyRedraw(hboxPtr);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
SeeOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Entry *entryPtr;
    int width, height;
    int x, y;
    Tree *treePtr;
    Tk_Anchor anchor;
    int left, right, top, bottom;

    anchor = TK_ANCHOR_W;	/* Default anchor is West */
    if ((argv[2][0] == '-') && (strcmp(argv[2], "-anchor") == 0)) {
	if (argc == 3) {
	    Tcl_AppendResult(interp, "missing \"-anchor\" argument",
		(char *)NULL);
	    return TCL_ERROR;
	}
	if (Tk_GetAnchor(interp, argv[3], &anchor) != TCL_OK) {
	    return TCL_ERROR;
	}
	argc -= 2, argv += 2;
    }
    if (argc == 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    "see ?-anchor anchor? index\"", (char *)NULL);
	return TCL_ERROR;
    }
    treePtr = hboxPtr->focusPtr;
    if (GetNode(hboxPtr, argv[2], &treePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (treePtr == NULL) {
	return TCL_OK;
    }
    if (IsHidden(treePtr)) {
	ExposeAncestors(treePtr);
	hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
	/*
	 * If the entry wasn't previously exposed, its world coordinates
	 * aren't likely to be valid.  So re-compute the layout before
	 * we try to see the viewport to the entry's location.
	 */
	ComputeLayout(hboxPtr);
    }
    entryPtr = treePtr->entryPtr;
    width = VPORTWIDTH(hboxPtr);
    height = VPORTHEIGHT(hboxPtr);

    /*
     * XVIEW:	If the entry is left or right of the current view, adjust
     *		the offset.  If the entry is nearby, adjust the view just
     *		a bit.  Otherwise, center the entry.
     */
    left = hboxPtr->xOffset;
    right = hboxPtr->xOffset + width;

    switch (anchor) {
    case TK_ANCHOR_W:
    case TK_ANCHOR_NW:
    case TK_ANCHOR_SW:
	x = 0;
	break;
    case TK_ANCHOR_E:
    case TK_ANCHOR_NE:
    case TK_ANCHOR_SE:
	x = entryPtr->worldX + entryPtr->width + LEVELWIDTH(treePtr->level) -
	    width;
	break;
    default:
	if (entryPtr->worldX < left) {
	    x = entryPtr->worldX;
	} else if ((entryPtr->worldX + entryPtr->width) > right) {
	    x = entryPtr->worldX + entryPtr->width - width;
	} else {
	    x = hboxPtr->xOffset;
	}
	break;
    }
    /*
     * YVIEW:	If the entry is above or below the current view, adjust
     *		the offset.  If the entry is nearby, adjust the view just
     *		a bit.  Otherwise, center the entry.
     */
    top = hboxPtr->yOffset;
    bottom = hboxPtr->yOffset + height;

    switch (anchor) {
    case TK_ANCHOR_N:
	y = hboxPtr->yOffset;
	break;
    case TK_ANCHOR_NE:
    case TK_ANCHOR_NW:
	y = entryPtr->worldY - (height / 2);
	break;
    case TK_ANCHOR_S:
    case TK_ANCHOR_SE:
    case TK_ANCHOR_SW:
	y = entryPtr->worldY + entryPtr->height - height;
	break;
    default:
	if (entryPtr->worldY < top) {
	    y = entryPtr->worldY;
	} else if ((entryPtr->worldY + entryPtr->height) > bottom) {
	    y = entryPtr->worldY + entryPtr->height - height;
	} else {
	    y = hboxPtr->yOffset;
	}
	break;
    }
    if ((y != hboxPtr->yOffset) || (x != hboxPtr->xOffset)) {
	hboxPtr->xOffset = x;
	hboxPtr->yOffset = y;
	hboxPtr->flags |= (HIERBOX_SCROLL | HIERBOX_LAYOUT);
    }
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AnchorOpOp --
 *
 *	Sets the selection anchor to the element given by a index.
 *	The selection anchor is the end of the selection that is fixed
 *	while dragging out a selection with the mouse.  The index
 *	"anchor" may be used to refer to the anchor element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
AnchorOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;			/* Not used. */
    char **argv;
{
    Tree *nodePtr;

    nodePtr = hboxPtr->focusPtr;
    if (GetNode(hboxPtr, argv[3], &nodePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    hboxPtr->selAnchorPtr = nodePtr;
    if (nodePtr != NULL) {
	Tcl_SetResult(interp, NodeToString(hboxPtr, nodePtr), TCL_VOLATILE);
    }
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}



/*
 *----------------------------------------------------------------------
 *
 * ClearallOpOp
 *
 *	Clears the entire selection.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ClearallOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    ClearSelection(hboxPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IncludesOpOp
 *
 *	Returns 1 if the element indicated by index is currently
 *	selected, 0 if it isn't.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
IncludesOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *treePtr;

    treePtr = hboxPtr->focusPtr;
    if (GetNode(hboxPtr, argv[3], &treePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (treePtr != NULL) {
	int bool;

	bool = IsSelected(hboxPtr, treePtr);
	Blt_SetBooleanResult(interp, bool);
    }
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * MarkOpOp --
 *
 *	Sets the selection mark to the element given by a index.  The
 *	selection mark is the end of the selection that is not fixed
 *	while dragging out a selection with the mouse.  The index
 *	"mark" may be used to refer to the anchor element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *---------------------------------------------------------------------- 
 */
/*ARGSUSED*/
static int
MarkOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;			/* Not used. */
    char **argv;
{
    Tree *nodePtr;
    Blt_ChainLink *linkPtr, *nextPtr;
    Tree *selectPtr;

    nodePtr = hboxPtr->focusPtr;
    if (GetNode(hboxPtr, argv[3], &nodePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (hboxPtr->selAnchorPtr == NULL) {
	Tcl_AppendResult(interp, "selection anchor must be set first", 
		 (char *)NULL);
	return TCL_ERROR;
    }

    /* Deselect entry from the list all the way back to the anchor. */
    for (linkPtr = Blt_ChainLastLink(&(hboxPtr->selectChain)); 
	 linkPtr != NULL; linkPtr = nextPtr) {
	nextPtr = Blt_ChainPrevLink(linkPtr);
	selectPtr = Blt_ChainGetValue(linkPtr);
	if (selectPtr == hboxPtr->selAnchorPtr) {
	    break;
	}
	DeselectEntry(hboxPtr, selectPtr);
    }
    if (nodePtr != NULL) {
	hboxPtr->flags &= ~SELECTION_MASK;
	hboxPtr->flags |= SELECTION_SET;
	SelectRange(hboxPtr, hboxPtr->selAnchorPtr, nodePtr);
	hboxPtr->flags &= ~SELECTION_MASK;
	Tcl_SetResult(interp, NodeToString(hboxPtr, nodePtr), TCL_VOLATILE);
    }
    EventuallyRedraw(hboxPtr);
    if (hboxPtr->selectCmd != NULL) {
	EventuallyInvokeSelectCmd(hboxPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PresentOpOp
 *
 *	Indicates if there is a selection present.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PresentOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    int bool;

    bool = (Blt_ChainGetLength(&(hboxPtr->selectChain)) > 0);
    Blt_SetBooleanResult(interp, bool);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectOpOp
 *
 *	Selects, deselects, or toggles all of the elements in the
 *	range between first and last, inclusive, without affecting the
 *	selection state of elements outside that range.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SelectOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tree *firstPtr, *lastPtr;

    hboxPtr->flags &= ~SELECTION_MASK;
    switch (argv[2][0]) {
    case 's':
	hboxPtr->flags |= SELECTION_SET;
	break;
    case 'c':
	hboxPtr->flags |= SELECTION_CLEAR;
	break;
    case 't':
	hboxPtr->flags |= SELECTION_TOGGLE;
	break;
    }

    if (StringToNode(hboxPtr, argv[3], &firstPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((IsHidden(firstPtr)) && !(hboxPtr->flags & SELECTION_CLEAR)) {
	Tcl_AppendResult(interp, "can't select hidden node \"", argv[3], "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    lastPtr = firstPtr;
    if (argc > 4) {
	if (StringToNode(hboxPtr, argv[4], &lastPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((IsHidden(lastPtr)) && !(hboxPtr->flags & SELECTION_CLEAR)) {
	    Tcl_AppendResult(interp, "can't select hidden node \"", argv[4],
		"\"", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    if (firstPtr == lastPtr) {
	SelectNode(hboxPtr, firstPtr);
    } else {
	SelectRange(hboxPtr, firstPtr, lastPtr);
    }
    hboxPtr->flags &= ~SELECTION_MASK;
    if (hboxPtr->flags & SELECTION_EXPORT) {
	Tk_OwnSelection(hboxPtr->tkwin, XA_PRIMARY, LostSelection, hboxPtr);
    }
    EventuallyRedraw(hboxPtr);
    if (hboxPtr->selectCmd != NULL) {
	EventuallyInvokeSelectCmd(hboxPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectionOp --
 *
 *	This procedure handles the individual options for text
 *	selections.  The selected text is designated by start and end
 *	indices into the text pool.  The selected segment has both a
 *	anchored and unanchored ends.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
static Blt_OpSpec selectionOperSpecs[] =
{
    {"anchor", 1, (Blt_Op)AnchorOpOp, 4, 4, "index",},
    {"clear", 5, (Blt_Op)SelectOpOp, 4, 5, "firstIndex ?lastIndex?",},
    {"clearall", 6, (Blt_Op)ClearallOpOp, 3, 3, "",},
    {"includes", 2, (Blt_Op)IncludesOpOp, 4, 4, "index",},
    {"mark", 1, (Blt_Op)MarkOpOp, 4, 4, "index",},
    {"present", 1, (Blt_Op)PresentOpOp, 3, 3, "",},
    {"set", 1, (Blt_Op)SelectOpOp, 4, 5, "firstIndex ?lastIndex?",},
    {"toggle", 1, (Blt_Op)SelectOpOp, 4, 5, "firstIndex ?lastIndex?",},
};
static int nSelectionSpecs = sizeof(selectionOperSpecs) / sizeof(Blt_OpSpec);

static int
SelectionOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nSelectionSpecs, selectionOperSpecs, BLT_OP_ARG2,
	argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (hboxPtr, interp, argc, argv);
    return result;
}

/*ARGSUSED*/
static int
SortOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    int length;
    Tree *rootPtr;
    register int i;
    unsigned int flags;

    flags = 0;
    hboxPtr->sortCmd = NULL;
    for (i = 2; i < argc; i++) {
	if (argv[i][0] != '-') {
	    break;		/* Found start of indices */
	}
	length = strlen(argv[i]);
	if ((length > 1) && (strncmp(argv[i], "-recurse", length) == 0)) {
	    flags |= APPLY_RECURSE;
	} else if ((length > 1) && (strncmp(argv[i], "-command", length) ==0)) {
	    if ((i + 1) == argc) {
		Tcl_AppendResult(interp, "\"-command\" must be",
		    " followed by comparison command", (char *)NULL);
		return TCL_ERROR;
	    }
	    i++;
	    hboxPtr->sortCmd = argv[i];
	} else if ((argv[i][1] == '-') && (argv[i][2] == '\0')) {
	    break;		/* Allow first index to start with a '-' */
	} else {
	    Tcl_AppendResult(interp, "bad switch \"", argv[i],
		"\": must be -command or -recurse", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    for ( /*empty*/ ; i < argc; i++) {
	if (StringToNode(hboxPtr, argv[i], &rootPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (ApplyToTree(hboxPtr, rootPtr, SortNode, flags) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    hboxPtr->flags |= HIERBOX_LAYOUT;
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
ToggleOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;		/* Not used. */
    int argc;
    char **argv;
{
    Tree *rootPtr;
    int result;

    rootPtr = hboxPtr->focusPtr;
    if (GetNode(hboxPtr, argv[2], &rootPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (rootPtr == NULL) {
	return TCL_OK;
    }
    if (rootPtr->entryPtr->flags & ENTRY_OPEN) {
	PruneSelection(hboxPtr, rootPtr);
	if (IsAncestor(rootPtr, hboxPtr->focusPtr)) {
	    hboxPtr->focusPtr = rootPtr;
	    Blt_SetFocusItem(hboxPtr->bindTable, hboxPtr->focusPtr, NULL);
	}
	if (IsAncestor(rootPtr, hboxPtr->selAnchorPtr)) {
	    hboxPtr->selAnchorPtr = NULL;
	}
	result = CloseNode(hboxPtr, rootPtr);
    } else {
	result = OpenNode(hboxPtr, rootPtr);
    }
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    hboxPtr->flags |= (HIERBOX_LAYOUT | HIERBOX_SCROLL);
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

static int
XViewOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int width, worldWidth;

    width = VPORTWIDTH(hboxPtr);
    worldWidth = hboxPtr->worldWidth;
    if (argc == 2) {
	double fract;

	/*
	 * Note that we are bounding the fractions between 0.0 and 1.0
	 * to support the "canvas"-style of scrolling.
	 */
	fract = (double)hboxPtr->xOffset / worldWidth;
	Tcl_AppendElement(interp, Blt_Dtoa(interp, CLAMP(fract, 0.0, 1.0)));
	fract = (double)(hboxPtr->xOffset + width) / worldWidth;
	Tcl_AppendElement(interp, Blt_Dtoa(interp, CLAMP(fract, 0.0, 1.0)));
	return TCL_OK;
    }
    if (Blt_GetScrollInfo(interp, argc - 2, argv + 2, &(hboxPtr->xOffset),
	    worldWidth, width, hboxPtr->xScrollUnits, hboxPtr->scrollMode) 
	    != TCL_OK) {
	return TCL_ERROR;
    }
    hboxPtr->flags |= HIERBOX_XSCROLL;
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

static int
YViewOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int height, worldHeight;

    height = VPORTHEIGHT(hboxPtr);
    worldHeight = hboxPtr->worldHeight;
    if (argc == 2) {
	double fract;

	/* Report first and last fractions */
	fract = (double)hboxPtr->yOffset / worldHeight;
	Tcl_AppendElement(interp, Blt_Dtoa(interp, CLAMP(fract, 0.0, 1.0)));
	fract = (double)(hboxPtr->yOffset + height) / worldHeight;
	Tcl_AppendElement(interp, Blt_Dtoa(interp, CLAMP(fract, 0.0, 1.0)));
	return TCL_OK;
    }
    if (Blt_GetScrollInfo(interp, argc - 2, argv + 2, &(hboxPtr->yOffset),
	    worldHeight, height, hboxPtr->yScrollUnits, hboxPtr->scrollMode)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    hboxPtr->flags |= HIERBOX_SCROLL;
    EventuallyRedraw(hboxPtr);
    return TCL_OK;
}

/*
 * --------------------------------------------------------------
 *
 * HierboxInstCmd --
 *
 * 	This procedure is invoked to process the "hierbox" command.
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
static Blt_OpSpec operSpecs[] =
{
    {"bbox", 2, (Blt_Op)BboxOp, 2, 0, "index...",},
    {"bind", 2, (Blt_Op)BindOp, 3, 5, "tagName ?sequence command?",},
    {"button", 2, (Blt_Op)ButtonOp, 2, 0, "args",},
    {"cget", 2, (Blt_Op)CgetOp, 3, 3, "option",},
    {"close", 2, (Blt_Op)CloseOp, 2, 0, "?-recurse? index...",},
    {"configure", 2, (Blt_Op)ConfigureOp, 2, 0, "?option value?...",},
    {"curselection", 2, (Blt_Op)CurselectionOp, 2, 2, "",},
    {"delete", 1, (Blt_Op)DeleteOp, 2, 0, "?-recurse? index ?index...?",},
    {"entry", 1, (Blt_Op)EntryOp, 2, 0, "oper args",},
    {"find", 2, (Blt_Op)FindOp, 2, 0, "?flags...? ?firstIndex lastIndex?",},
    {"focus", 2, (Blt_Op)FocusOp, 3, 3, "index",},
    {"get", 1, (Blt_Op)GetOp, 2, 0, "?-full? index ?index...?",},
    {"hide", 1, (Blt_Op)HideOp, 2, 0, "?-exact? ?-glob? ?-regexp? ?-nonmatching? ?-name string? ?-full string? ?-data string? ?--? ?index...?",},
    {"index", 3, (Blt_Op)IndexOp, 3, 5, "?-at index? string",},
    {"insert", 3, (Blt_Op)InsertOp, 3, 0, "?-at index? position label ?label...? ?option value?",},
    {"move", 1, (Blt_Op)MoveOp, 5, 5, "index into|before|after index",},
    {"nearest", 1, (Blt_Op)NearestOp, 4, 5, "x y ?varName?",},
    {"open", 1, (Blt_Op)OpenOp, 2, 0, "?-recurse? index...",},
    {"range", 1, (Blt_Op)RangeOp, 4, 5, "?-open? firstIndex lastIndex",},
    {"scan", 2, (Blt_Op)ScanOp, 5, 5, "dragto|mark x y",},
    {"see", 3, (Blt_Op)SeeOp, 3, 0, "?-anchor anchor? index",},
    {"selection", 3, (Blt_Op)SelectionOp, 2, 0, "oper args",},
    {"show", 2, (Blt_Op)ShowOp, 2, 0, "?-exact? ?-glob? ?-regexp? ?-nonmatching? ?-name string? ?-full string? ?-data string? ?--? ?index...?",},
    {"sort", 2, (Blt_Op)SortOp, 2, 0, "?-recurse? ?-command string? index...",},
    {"toggle", 1, (Blt_Op)ToggleOp, 3, 3, "index",},
    {"xview", 1, (Blt_Op)XViewOp, 2, 5, "?moveto fract? ?scroll number what?",},
    {"yview", 1, (Blt_Op)YViewOp, 2, 5, "?moveto fract? ?scroll number what?",},
};

static int nSpecs = sizeof(operSpecs) / sizeof(Blt_OpSpec);

static int
HierboxInstCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about the widget. */
    Tcl_Interp *interp;		/* Interpreter to report errors back to. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Vector of argument strings. */
{
    Blt_Op proc;
    Hierbox *hboxPtr = clientData;
    int result;

    proc = Blt_GetOp(interp, nSpecs, operSpecs, BLT_OP_ARG1, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve(hboxPtr);
    result = (*proc) (hboxPtr, interp, argc, argv);
    Tcl_Release(hboxPtr);
    return result;
}

int
Blt_HierboxInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec =
    {
	"hierbox", HierboxCmd,
    };

    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#ifdef notdef

/* Selection Procedures */
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
SelectTextBlock(hboxPtr, to)
    Hierbox *hboxPtr;		/* Information about widget. */
    int to;			/* Index of element that is to
				 * become the "other" end of the
				 * selection. */
{
    TextEdit *editPtr = &(hboxPtr->labelEdit);
    int first, last;

    /*  If the anchor hasn't been set yet, assume the beginning of the label. */
    if (editPtr->selAnchor < 0) {
	editPtr->selAnchor = 0;
    }
    if (editPtr->selAnchor <= to) {
	first = editPtr->selAnchor;
	last = to;
    } else {
	first = to;
	last = editPtr->selAnchor;
    }
    if ((editPtr->selFirst != first) || (editPtr->selLast != last)) {
	editPtr->selFirst = first, editPtr->selLast = last;
	EventuallyRedraw(hboxPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ClearOpOp --
 *
 *		Clears the selection from the label.
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
ClearOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    TextEdit *editPtr = &(hboxPtr->labelEdit);

    if (editPtr->selFirst != -1) {
	editPtr->selFirst = editPtr->selLast = -1;
	EventuallyRedraw(hboxPtr);
    }
    return TCL_OK;
}

static int
FromOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    TextEdit *editPtr = &(hboxPtr->labelEdit);
    int from;

    if (GetLabelIndex(hboxPtr, argv[3], &from) != TCL_OK) {
	return TCL_ERROR;
    }
    editPtr->selAnchor = from;
    return TCL_OK;
}

static int
PresentOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    TextEdit *editPtr = &(hboxPtr->labelEdit);
    Tcl_AppendResult(interp, (editPtr->selFirst != -1) ? "0" : "1", (char *)NULL);
    return TCL_OK;
}

static int
AdjustOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    TextEdit *editPtr = &(hboxPtr->labelEdit);
    int adjust;

    if (GetLabelIndex(hboxPtr, argv[3], &adjust) != TCL_OK) {
	return TCL_ERROR;
    }
    half1 = (editPtr->selFirst + editPtr->selLast) / 2;
    half2 = (editPtr->selFirst + editPtr->selLast + 1) / 2;
    if (adjust < half1) {
	editPtr->selAnchor = editPtr->selLast;
    } else if (adjust > half2) {
	editPtr->selAnchor = editPtr->selFirst;
    }
    result = SelectTextBlock(hboxPtr, adjust);
    return TCL_OK;
}

static int
ToOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int to;

    if (GetLabelIndex(hboxPtr, argv[3], &to) != TCL_OK) {
	return TCL_ERROR;
    }
    SelectTextBlock(hboxPtr, to);
    return TCL_OK;
}

static int
RangeOpOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    TextEdit *editPtr = &(hboxPtr->labelEdit);
    int first, last;

    if (GetLabelIndex(hboxPtr, argv[4], &first) != TCL_OK) {
	return TCL_ERROR;
    }
    if (GetLabelIndex(hboxPtr, argv[5], &last) != TCL_OK) {
	return TCL_ERROR;
    }
    editPtr->selAnchor = first;
    return SelectTextBlock(hboxPtr, last);
}

/*
 *----------------------------------------------------------------------
 *
 * SelectionOp --
 *
 *	This procedure handles the individual options for text
 *	selections.  The selected text is designated by a starting
 *	and terminating index that points into the text.  The selected
 *	segment has both a anchored and unanchored ends.  The following
 *	selection operations are implemented:
 *
 *	  adjust index		Resets either the first or last index
 *				of the selection.
 *	  clear			Clears the selection. Sets first/last
 *				indices to -1.
 *	  from index		Sets the index of the selection anchor.
 *	  present		Return "1" if a selection is available,
 *				"0" otherwise.
 *	  range first last	Sets the selection.
 *	  to index		Sets the index of the un-anchored end.

 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
static Blt_OpSpec entrySelectionOperSpecs[] =
{
    {"adjust", 2, (Blt_Op)AdjustOpOp, 5, 5, "index",},
    {"clear", 2, (Blt_Op)ClearOpOp, 4, 4, "",},
    {"from", 1, (Blt_Op)FromOpOp, 5, 5, "index",},
    {"present", 1, (Blt_Op)PresentOpOp, 4, 4, "",},
    {"range", 1, (Blt_Op)RangeOpOp, 4, 5, "firstIndex lastIndex",},
    {"to", 1, (Blt_Op)ToOpOp, 5, 5, "index",},
};
static int nEntrySelectionSpecs =
sizeof(entrySelectionOperSpecs) / sizeof(Blt_OpSpec);

static int
EntrySelectionOp(hboxPtr, interp, argc, argv)
    Hierbox *hboxPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nSelectionSpecs, selectionOperSpecs, BLT_OP_ARG2,
	argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (hboxPtr, interp, argc, argv);
    return result;
}

#endif

#endif /* NO_HIERBOX */
