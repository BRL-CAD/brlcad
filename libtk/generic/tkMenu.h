/*
 * tkMenu.h --
 *
 *	Declarations shared among all of the files that implement menu widgets.
 *
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMenu.h 1.60 97/06/20 14:43:21
 */

#ifndef _TKMENU
#define _TKMENU

#ifndef _TK
#include "tk.h"
#endif

#ifndef _TKINT
#include "tkInt.h"
#endif

#ifndef _DEFAULT
#include "default.h"
#endif

/*
 * Dummy types used by the platform menu code.
 */

typedef struct TkMenuPlatformData_ *TkMenuPlatformData;
typedef struct TkMenuPlatformEntryData_ *TkMenuPlatformEntryData;

/*
 * One of the following data structures is kept for each entry of each
 * menu managed by this file:
 */

typedef struct TkMenuEntry {
    int type;			/* Type of menu entry;  see below for
				 * valid types. */
    struct TkMenu *menuPtr;	/* Menu with which this entry is associated. */
    char *label;		/* Main text label displayed in entry (NULL
				 * if no label).  Malloc'ed. */
    int labelLength;		/* Number of non-NULL characters in label. */
    Tk_Uid state;		/* State of button for display purposes:
				 * normal, active, or disabled. */
    int underline;		/* Index of character to underline. */
    Pixmap bitmap;		/* Bitmap to display in menu entry, or None.
				 * If not None then label is ignored. */
    char *imageString;		/* Name of image to display (malloc'ed), or
				 * NULL.  If non-NULL, bitmap, text, and
				 * textVarName are ignored. */
    Tk_Image image;		/* Image to display in menu entry, or NULL if
				 * none. */
    char *selectImageString;	/* Name of image to display when selected
				 * (malloc'ed), or NULL. */
    Tk_Image selectImage;	/* Image to display in entry when selected,
				 * or NULL if none.  Ignored if image is
				 * NULL. */
    char *accel;		/* Accelerator string displayed at right
				 * of menu entry.  NULL means no such
				 * accelerator.  Malloc'ed. */
    int accelLength;		/* Number of non-NULL characters in
				 * accelerator. */
    int indicatorOn;		/* True means draw indicator, false means
				 * don't draw it. */
    /*
     * Display attributes
     */

    Tk_3DBorder border;		/* Structure used to draw background for
				 * entry.  NULL means use overall border
				 * for menu. */
    XColor *fg;			/* Foreground color to use for entry.  NULL
				 * means use foreground color from menu. */
    Tk_3DBorder activeBorder;	/* Used to draw background and border when
				 * element is active.  NULL means use
				 * activeBorder from menu. */
    XColor *activeFg;		/* Foreground color to use when entry is
				 * active.  NULL means use active foreground
				 * from menu. */
    XColor *indicatorFg;	/* Color for indicators in radio and check
				 * button entries.  NULL means use indicatorFg
				 * GC from menu. */
    Tk_Font tkfont;		/* Text font for menu entries.  NULL means
				 * use overall font for menu. */
    int columnBreak;		/* If this is 0, this item appears below
				 * the item in front of it. If this is
				 * 1, this item starts a new column. */
    int hideMargin;		/* If this is 0, then the item has enough
    				 * margin to accomodate a standard check
    				 * mark and a default right margin. If this 
    				 * is 1, then the item has no such margins.
    				 * and checkbuttons and radiobuttons with
    				 * this set will have a rectangle drawn
    				 * in the indicator around the item if
    				 * the item is checked.
    				 * This is useful palette menus.*/ 
    int indicatorSpace;		/* The width of the indicator space for this
				 * entry.
				 */
    int labelWidth;		/* Number of pixels to allow for displaying
				 * labels in menu entries. */

    /*
     * Information used to implement this entry's action:
     */

    char *command;		/* Command to invoke when entry is invoked.
				 * Malloc'ed. */
    char *name;			/* Name of variable (for check buttons and
				 * radio buttons) or menu (for cascade
				 * entries).  Malloc'ed.*/
    char *onValue;		/* Value to store in variable when selected
				 * (only for radio and check buttons).
				 * Malloc'ed. */
    char *offValue;		/* Value to store in variable when not
				 * selected (only for check buttons).
				 * Malloc'ed. */
    
    /*
     * Information used for drawing this menu entry.
     */
     
    int width;			/* Number of pixels occupied by entry in
				 * horizontal dimension. Not used except
				 * in menubars. The width of norma menus
				 * is dependent on the rest of the menu. */
    int x;			/* X-coordinate of leftmost pixel in entry */
    int height;			/* Number of pixels occupied by entry in
				 * vertical dimension, including raised
				 * border drawn around entry when active. */
    int y;			/* Y-coordinate of topmost pixel in entry. */
    GC textGC;			/* GC for drawing text in entry.  NULL means
				 * use overall textGC for menu. */
    GC activeGC;		/* GC for drawing text in entry when active.
				 * NULL means use overall activeGC for
				 * menu. */
    GC disabledGC;		/* Used to produce disabled effect for entry.
				 * NULL means use overall disabledGC from
				 * menu structure.  See comments for
				 * disabledFg in menu structure for more
				 * information. */
    GC indicatorGC;		/* For drawing indicators.  None means use
				 * GC from menu. */

    /*
     * Miscellaneous fields.
     */
 
    int entryFlags;		/* Various flags.  See below for
				   definitions. */
    int index;			/* Need to know which index we are. This
    				 * is zero-based. This is the top-left entry
    				 * of the menu. */
				 
    /*
     * Bookeeping for master menus and cascade menus.
     */
     
    struct TkMenuReferences *childMenuRefPtr;
    				/* A pointer to the hash table entry for
    				 * the child menu. Stored here when the menu
    				 * entry is configured so that a hash lookup
    				 * is not necessary later.*/
    struct TkMenuEntry *nextCascadePtr;
    				/* The next cascade entry that is a parent of
    				 * this entry's child cascade menu. NULL
    				 * end of list, this is not a cascade entry,
    				 * or the menu that this entry point to
    				 * does not yet exist. */
    TkMenuPlatformEntryData platformEntryData;
    				/* The data for the specific type of menu.
  				 * Depends on platform and menu type what
  				 * kind of options are in this structure.
  				 */
} TkMenuEntry;

/*
 * Flag values defined for menu entries:
 *
 * ENTRY_SELECTED:		Non-zero means this is a radio or check
 *				button and that it should be drawn in
 *				the "selected" state.
 * ENTRY_NEEDS_REDISPLAY:	Non-zero means the entry should be redisplayed.
 * ENTRY_LAST_COLUMN:		Used by the drawing code. If the entry is in the
 *				last column, the space to its right needs to
 *				be filled.
 * ENTRY_PLATFORM_FLAG1 - 4	These flags are reserved for use by the
 *				platform-dependent implementation of menus
 *				and should not be used by anything else.
 */

#define ENTRY_SELECTED		1
#define ENTRY_NEEDS_REDISPLAY	2
#define ENTRY_LAST_COLUMN	4
#define ENTRY_PLATFORM_FLAG1	(1 << 30)
#define ENTRY_PLATFORM_FLAG2	(1 << 29)
#define ENTRY_PLATFORM_FLAG3	(1 << 28)
#define ENTRY_PLATFORM_FLAG4	(1 << 27)

/*
 * Types defined for MenuEntries:
 */

#define COMMAND_ENTRY		0
#define SEPARATOR_ENTRY		1
#define CHECK_BUTTON_ENTRY	2
#define RADIO_BUTTON_ENTRY	3
#define CASCADE_ENTRY		4
#define TEAROFF_ENTRY		5

/*
 * Mask bits for above types:
 */

#define COMMAND_MASK		TK_CONFIG_USER_BIT
#define SEPARATOR_MASK		(TK_CONFIG_USER_BIT << 1)
#define CHECK_BUTTON_MASK	(TK_CONFIG_USER_BIT << 2)
#define RADIO_BUTTON_MASK	(TK_CONFIG_USER_BIT << 3)
#define CASCADE_MASK		(TK_CONFIG_USER_BIT << 4)
#define TEAROFF_MASK		(TK_CONFIG_USER_BIT << 5)
#define ALL_MASK		(COMMAND_MASK | SEPARATOR_MASK \
	| CHECK_BUTTON_MASK | RADIO_BUTTON_MASK | CASCADE_MASK | TEAROFF_MASK)

/*
 * A data structure of the following type is kept for each
 * menu widget:
 */

typedef struct TkMenu {
    Tk_Window tkwin;		/* Window that embodies the pane.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Display *display;		/* Display containing widget.  Needed, among
				 * other things, so that resources can be
				 * freed up even after tkwin has gone away. */
    Tcl_Interp *interp;		/* Interpreter associated with menu. */
    Tcl_Command widgetCmd;	/* Token for menu's widget command. */
    TkMenuEntry **entries;	/* Array of pointers to all the entries
				 * in the menu.  NULL means no entries. */
    int numEntries;		/* Number of elements in entries. */
    int active;			/* Index of active entry.  -1 means
				 * nothing active. */
    int menuType;		/* MASTER_MENU, TEAROFF_MENU, or MENUBAR.
    				 * See below for definitions. */
    char *menuTypeName;		/* Used to control whether created tkwin
				 * is a toplevel or not. "normal", "menubar",
				 * or "toplevel" */

    /*
     * Information used when displaying widget:
     */

    Tk_3DBorder border;		/* Structure used to draw 3-D
				 * border and background for menu. */
    int borderWidth;		/* Width of border around whole menu. */
    Tk_3DBorder activeBorder;	/* Used to draw background and border for
				 * active element (if any). */
    int activeBorderWidth;	/* Width of border around active element. */
    int relief;			/* 3-d effect: TK_RELIEF_RAISED, etc. */
    Tk_Font tkfont;		/* Text font for menu entries. */
    XColor *fg;			/* Foreground color for entries. */
    XColor *disabledFg;		/* Foreground color when disabled.  NULL
				 * means use normalFg with a 50% stipple
				 * instead. */
    XColor *activeFg;		/* Foreground color for active entry. */
    XColor *indicatorFg;	/* Color for indicators in radio and check
				 * button entries. */
    Pixmap gray;		/* Bitmap for drawing disabled entries in
				 * a stippled fashion.  None means not
				 * allocated yet. */
    GC textGC;			/* GC for drawing text and other features
				 * of menu entries. */
    GC disabledGC;		/* Used to produce disabled effect.  If
				 * disabledFg isn't NULL, this GC is used to
				 * draw text and icons for disabled entries.
				 * Otherwise text and icons are drawn with
				 * normalGC and this GC is used to stipple
				 * background across them. */
    GC activeGC;		/* GC for drawing active entry. */
    GC indicatorGC;		/* For drawing indicators. */
    GC disabledImageGC;		/* Used for drawing disabled images. They
				 * have to be stippled. This is created
				 * when the image is about to be drawn the
				 * first time. */

    /*
     * Information about geometry of menu.
     */
    
    int totalWidth;		/* Width of entire menu */
    int totalHeight;		/* Height of entire menu */
   
    /*
     * Miscellaneous information:
     */

    int tearOff;		/* 1 means this menu can be torn off. On some
    				 * platforms, the user can drag an outline
    				 * of the menu by just dragging outside of
    				 * the menu, and the tearoff is created where
    				 * the mouse is released. On others, an
				 * indicator (such as a dashed stripe) is
				 * drawn, and when the menu is selected, the
				 * tearoff is created. */
    char *title;		/* The title to use when this menu is torn
    				 * off. If this is NULL, a default scheme
    				 * will be used to generate a title for
    				 * tearoff. */
    char *tearOffCommand;	/* If non-NULL, points to a command to
				 * run whenever the menu is torn-off. */
    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    Tk_Cursor cursor;		/* Current cursor for window, or None. */
    char *postCommand;		/* Used to detect cycles in cascade hierarchy
    				 * trees when preprocessing postcommands
    				 * on some platforms. See PostMenu for
    				 * more details. */
    int postCommandGeneration;	/* Need to do pre-invocation post command
				 * traversal */
    int menuFlags;		/* Flags for use by X; see below for
				   definition */
    TkMenuEntry *postedCascade;	/* Points to menu entry for cascaded submenu
				 * that is currently posted or NULL if no
				 * submenu posted. */
    struct TkMenu *nextInstancePtr;	
    				/* The next instance of this menu in the
    				 * chain. */
    struct TkMenu *masterMenuPtr;
    				/* A pointer to the original menu for this
    				 * clone chain. Points back to this structure
    				 * if this menu is a master menu. */
    Tk_Window parentTopLevelPtr;/* If this menu is a menubar, this is the
    				 * toplevel that owns the menu. Only applicable
    				 * for menubar clones.
    				 */
    struct TkMenuReferences *menuRefPtr;	
    				/* Each menu is hashed into a table with the
    				 * name of the menu's window as the key.
    				 * The information in this hash table includes
    				 * a pointer to the menu (so that cascades
    				 * can find this menu), a pointer to the
    				 * list of toplevel widgets that have this
    				 * menu as its menubar, and a list of menu
    				 * entries that have this menu specified
    				 * as a cascade. */    
    TkMenuPlatformData platformData;
				/* The data for the specific type of menu.
  				 * Depends on platform and menu type what
  				 * kind of options are in this structure.
  				 */
} TkMenu;

/*
 * When the toplevel configure -menu command is executed, the menu may not
 * exist yet. We need to keep a linked list of windows that reference
 * a particular menu.
 */

typedef struct TkMenuTopLevelList {
    struct TkMenuTopLevelList *nextPtr;
    				/* The next window in the list */
    Tk_Window tkwin;		/* The window that has this menu as its
				 * menubar. */
} TkMenuTopLevelList;

/*
 * The following structure is used to keep track of things which
 * reference a menu. It is created when:
 * - a menu is created.
 * - a cascade entry is added to a menu with a non-null name
 * - the "-menu" configuration option is used on a toplevel widget
 * with a non-null parameter.
 *
 * One of these three fields must be non-NULL, but any of the fields may
 * be NULL. This structure makes it easy to determine whether or not
 * anything like recalculating platform data or geometry is necessary
 * when one of the three actions above is performed.
 */

typedef struct TkMenuReferences {
    struct TkMenu *menuPtr;	/* The menu data structure. This is NULL
    				 * if the menu does not exist. */
    TkMenuTopLevelList *topLevelListPtr;
    				/* First in the list of all toplevels that 
    				 * have this menu as its menubar. NULL if no 
    				 * toplevel widgets have this menu as its
    				 * menubar. */
    TkMenuEntry *parentEntryPtr;/* First in the list of all cascade menu 
    				 * entries that have this menu as their child.
    				 * NULL means no cascade entries. */
    Tcl_HashEntry *hashEntryPtr;/* This is needed because the pathname of the
    				 * window (which is what we hash on) may not
    				 * be around when we are deleting.
    				 */
} TkMenuReferences;

/*
 * Flag bits for menus:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 * RESIZE_PENDING:		Non-zero means a call to ComputeMenuGeometry
 *				has already been scheduled.
 * MENU_DELETION_PENDING	Non-zero means that we are currently destroying
 *				this menu. This is useful when we are in the 
 *				middle of cleaning this master menu's chain of 
 *				menus up when TkDestroyMenu was called again on
 *				this menu (via a destroy binding or somesuch).
 * MENU_PLATFORM_FLAG1...	Reserved for use by the platform-specific menu
 *				code.
 */

#define REDRAW_PENDING		1
#define RESIZE_PENDING		2
#define MENU_DELETION_PENDING	4
#define MENU_PLATFORM_FLAG1	(1 << 30)
#define MENU_PLATFORM_FLAG2	(1 << 29)
#define MENU_PLATFORM_FLAG3	(1 << 28)

/*
 * Each menu created by the user is a MASTER_MENU. When a menu is torn off,
 * a TEAROFF_MENU instance is created. When a menu is assigned to a toplevel
 * as a menu bar, a MENUBAR instance is created. All instances have the same
 * configuration information. If the master instance is deleted, all instances
 * are deleted. If one of the other instances is deleted, only that instance
 * is deleted.
 */
 
#define UNKNOWN_TYPE		-1
#define MASTER_MENU 		0
#define TEAROFF_MENU 		1
#define MENUBAR 		2

/*
 * Various geometry definitions:
 */

#define CASCADE_ARROW_HEIGHT 10
#define CASCADE_ARROW_WIDTH 8
#define DECORATION_BORDER_WIDTH 2

/*
 * Configuration specs. Needed for platform-specific default initializations.
 */

EXTERN Tk_ConfigSpec tkMenuEntryConfigSpecs[];
EXTERN Tk_ConfigSpec tkMenuConfigSpecs[];

/*
 * Menu-related procedures that are shared among Tk modules but not exported
 * to the outside world:
 */

EXTERN int		TkActivateMenuEntry _ANSI_ARGS_((TkMenu *menuPtr,
			    int index));
EXTERN void		TkBindMenu _ANSI_ARGS_((
			    Tk_Window tkwin, TkMenu *menuPtr));
EXTERN TkMenuReferences *
			TkCreateMenuReferences _ANSI_ARGS_((Tcl_Interp *interp,
			    char *pathName));
EXTERN void		TkDestroyMenu _ANSI_ARGS_((TkMenu *menuPtr));
EXTERN void             TkEventuallyRecomputeMenu _ANSI_ARGS_((TkMenu *menuPtr));
EXTERN void		TkEventuallyRedrawMenu _ANSI_ARGS_((
    			    TkMenu *menuPtr, TkMenuEntry *mePtr));
EXTERN TkMenuReferences *
			TkFindMenuReferences _ANSI_ARGS_((Tcl_Interp *interp,
			    char *pathName));
EXTERN void		TkFreeMenuReferences _ANSI_ARGS_((
			    TkMenuReferences *menuRefPtr));
EXTERN Tcl_HashTable *	TkGetMenuHashTable _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int		TkGetMenuIndex _ANSI_ARGS_((Tcl_Interp *interp,
			    TkMenu *menuPtr, char *string, int lastOK,
			    int *indexPtr));
EXTERN void		TkMenuInitializeDrawingFields _ANSI_ARGS_((TkMenu *menuPtr));
EXTERN void		TkMenuInitializeEntryDrawingFields _ANSI_ARGS_((
			    TkMenuEntry *mePtr));
EXTERN int		TkInvokeMenu _ANSI_ARGS_((Tcl_Interp *interp,
			    TkMenu *menuPtr, int index));
EXTERN void		TkMenuConfigureDrawOptions _ANSI_ARGS_((
			    TkMenu *menuPtr));
EXTERN int		TkMenuConfigureEntryDrawOptions _ANSI_ARGS_((
			    TkMenuEntry *mePtr, int index));
EXTERN void		TkMenuFreeDrawOptions _ANSI_ARGS_((TkMenu *menuPtr));
EXTERN void		TkMenuEntryFreeDrawOptions _ANSI_ARGS_((
			    TkMenuEntry *mePtr));
EXTERN void		TkMenuEventProc _ANSI_ARGS_((ClientData clientData,
    			    XEvent *eventPtr));
EXTERN void		TkMenuImageProc _ANSI_ARGS_((
    			    ClientData clientData, int x, int y, int width,
			    int height, int imgWidth, int imgHeight));
EXTERN void		TkMenuInit _ANSI_ARGS_((void));
EXTERN void		TkMenuSelectImageProc _ANSI_ARGS_
			    ((ClientData clientData, int x, int y,
			    int width, int height, int imgWidth,
			    int imgHeight));
EXTERN char *		TkNewMenuName _ANSI_ARGS_((Tcl_Interp *interp, 
			    char *parentName, TkMenu *menuPtr));
EXTERN int		TkPostCommand _ANSI_ARGS_((TkMenu *menuPtr));
EXTERN int		TkPostSubmenu _ANSI_ARGS_((Tcl_Interp *interp,
			    TkMenu *menuPtr, TkMenuEntry *mePtr));
EXTERN int		TkPostTearoffMenu _ANSI_ARGS_((Tcl_Interp *interp,
			    TkMenu *menuPtr, int x, int y));
EXTERN int		TkPreprocessMenu _ANSI_ARGS_((TkMenu *menuPtr));
EXTERN void             TkRecomputeMenu _ANSI_ARGS_((TkMenu *menuPtr));

/*
 * These routines are the platform-dependent routines called by the
 * common code.
 */

EXTERN void		TkpComputeMenubarGeometry _ANSI_ARGS_((TkMenu *menuPtr));
EXTERN void		TkpComputeStandardMenuGeometry _ANSI_ARGS_
			    ((TkMenu *menuPtr));
EXTERN int		TkpConfigureMenuEntry
                            _ANSI_ARGS_((TkMenuEntry *mePtr));
EXTERN void		TkpDestroyMenu _ANSI_ARGS_((TkMenu *menuPtr));
EXTERN void		TkpDestroyMenuEntry
			    _ANSI_ARGS_((TkMenuEntry *mEntryPtr));
EXTERN void		TkpDrawMenuEntry _ANSI_ARGS_((TkMenuEntry *mePtr,
			    Drawable d, Tk_Font tkfont, 
			    CONST Tk_FontMetrics *menuMetricsPtr, int x,
			    int y, int width, int height, int strictMotif,
			    int drawArrow));
EXTERN void		TkpMenuInit _ANSI_ARGS_((void));
EXTERN int		TkpMenuNewEntry _ANSI_ARGS_((TkMenuEntry *mePtr));
EXTERN int		TkpNewMenu _ANSI_ARGS_((TkMenu *menuPtr));
EXTERN int		TkpPostMenu _ANSI_ARGS_((Tcl_Interp *interp,
			    TkMenu *menuPtr, int x, int y));
EXTERN void		TkpSetWindowMenuBar _ANSI_ARGS_((Tk_Window tkwin,
			    TkMenu *menuPtr));

#endif /* _TKMENU */

