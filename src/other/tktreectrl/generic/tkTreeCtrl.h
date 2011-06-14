/* 
 * tkTreeCtrl.h --
 *
 *	This module is the header for treectrl widgets for the Tk toolkit.
 *
 * Copyright (c) 2002-2010 Tim Baker
 * Copyright (c) 2002-2003 Christian Krone
 * Copyright (c) 2003 ActiveState Corporation
 *
 * RCS: @(#) $Id$
 */

#include "tkPort.h"
#include "default.h"
#include "tktreectrl_config.h"
#include "tclInt.h"
#include "tkInt.h"
#include "qebind.h"


/*
 * Used to tag functions that are only to be visible within the module being
 * built and not outside it (where this is supported by the linker).
 */

#ifndef MODULE_SCOPE
#   ifdef __cplusplus
#	define MODULE_SCOPE extern "C"
#   else
#	define MODULE_SCOPE extern
#   endif
#endif

/*
 * Macros used to cast between pointers and integers (e.g. when storing an int
 * in ClientData), on 64-bit architectures they avoid gcc warning about "cast
 * to/from pointer from/to integer of different size".
 */

#if !defined(INT2PTR) && !defined(PTR2INT)
#   if defined(HAVE_INTPTR_T) || defined(intptr_t)
#	define INT2PTR(p) ((void *)(intptr_t)(p))
#	define PTR2INT(p) ((int)(intptr_t)(p))
#   else
#	define INT2PTR(p) ((void *)(p))
#	define PTR2INT(p) ((int)(p))
#   endif
#endif

#ifdef PLATFORM_SDL
#undef WIN32
#endif

#define dbwin TreeCtrl_dbwin
#define dbwin_add_interp TreeCtrl_dbwin_add_interp
MODULE_SCOPE void dbwin(char *fmt, ...);
MODULE_SCOPE void dbwin_add_interp(Tcl_Interp *interp);

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifdef WIN32
#define vsnprintf _vsnprintf
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define SELECTION_VISIBLE
#define ALLOC_HAX
#define DEPRECATED
/* #define DRAG_PIXMAP */
/* #define DRAGIMAGE_STYLE */ /* Use an item style as the dragimage instead of XOR rectangles. */
#define USE_ITEM_PIXMAP 1
#define COLUMNGRID 1

typedef struct TreeCtrl TreeCtrl;
typedef struct TreeColumn_ *TreeColumn;
typedef struct TreeColumnDInfo_ *TreeColumnDInfo;
typedef struct TreeDInfo_ *TreeDInfo;
typedef struct TreeDragImage_ *TreeDragImage;
typedef struct TreeItem_ *TreeItem;
typedef struct TreeItemColumn_ *TreeItemColumn;
typedef struct TreeItemDInfo_ *TreeItemDInfo;
typedef struct TreeMarquee_ *TreeMarquee;
typedef struct TreeItemRInfo_ *TreeItemRInfo;
typedef struct TreeStyle_ *TreeStyle;
typedef struct TreeElement_ *TreeElement;
typedef struct TreeThemeData_ *TreeThemeData;
typedef struct TreeGradient_ *TreeGradient;

/*****/

typedef struct PerStateInfo PerStateInfo;
typedef struct PerStateData PerStateData;
typedef struct PerStateType PerStateType;

/* There is one of these for each XColor, Tk_Font, Tk_Image etc */
struct PerStateData
{
    int stateOff;
    int stateOn;
    /* Type-specific fields go here */
};

struct PerStateInfo
{
#ifdef TREECTRL_DEBUG
    PerStateType *type;
#endif
    Tcl_Obj *obj;
    int count;
    PerStateData *data;
};

typedef int (*PerStateType_FromObjProc)(TreeCtrl *, Tcl_Obj *, PerStateData *);
typedef void (*PerStateType_FreeProc)(TreeCtrl *, PerStateData *);

struct PerStateType
{
    CONST char *name;
    int size;
    PerStateType_FromObjProc fromObjProc;
    PerStateType_FreeProc freeProc;
};

/*****/

typedef struct
{
    XColor *color;
#if 0
    double opacity;
#endif
    TreeGradient gradient;
} TreeColor;

#define TREECOLOR_CMP2(a,b) (((a)->color!=(b)->color)||((a)->gradient!=(b)->gradient))
#define TREECOLOR_CMP(a,b) ((!(a)!=!(b))||(((a)&&(b))&&TREECOLOR_CMP2(a,b)))

typedef struct
{
    Drawable drawable;
    int width;
    int height;
} TreeDrawable;

typedef struct
{
    int x, y;
    int width, height;
} TreeRectangle;

#define TreeRect_Left(tr) ((tr).x)
#define TreeRect_Top(tr) ((tr).y)
#define TreeRect_Right(tr) ((tr).x + (tr).width)
#define TreeRect_Bottom(tr) ((tr).y + (tr).height)
#define TreeRect_Width(tr) ((tr).width)
#define TreeRect_Height(tr) ((tr).height)
#define TreeRect_XYWH(tr,X,Y,W,H) ((*X)=TreeRect_Left(tr),(*Y)=TreeRect_Top(tr),\
	    (*W)=TreeRect_Width(tr),(*H)=TreeRect_Height(tr))
#define TreeRect_SetXYWH(tr,X,Y,W,H) ((tr).x=(X),(tr).y=(Y),\
	    (tr).width=(W),(tr).height=(H))
#define TreeRect_XYXY(tr,X1,Y1,X2,Y2) ((*X1)=TreeRect_Left(tr),(*Y1)=TreeRect_Top(tr),\
	    (*X2)=TreeRect_Right(tr),(*Y2)=TreeRect_Bottom(tr))
#define TreeRect_SetXYXY(tr,X1,Y1,X2,Y2) ((tr).x=(X1),(tr).y=(Y1),\
	    (tr).width=(X2)-(X1),(tr).height=(Y2)-(Y1))

#define TreeRect_ClipPoint(tr,XP,YP) \
    do { \
	if ((*XP) < (tr).x) (*XP)=(tr).x; \
	if ((*XP) >= TreeRect_Right(tr)) (*XP) = TreeRect_Right(tr) - 1; \
	if ((*YP) < (tr).y) (*YP)=(tr).y; \
	if ((*YP) >= TreeRect_Bottom(tr)) (*YP) = TreeRect_Bottom(tr) - 1; \
    } while (0)

#define TreeRect_FromXRect(xr,trp) ((trp)->x=(xr).x, (trp)->y=(xr).y, \
	    (trp)->width=(xr).width, (trp)->height=(xr).height)
#define TreeRect_ToXRect(tr,xrp) ((xrp)->x=(tr).x, (xrp)->y=(tr).y, \
	    (xrp)->width=(tr).width, (xrp)->height=(tr).height)

typedef struct GCCache GCCache;
struct GCCache
{
    unsigned long mask;
    XGCValues gcValues;
    GC gc;
    GCCache *next;
};

/*
 * A TreePtrList is used for dynamically-growing lists of ClientData pointers.
 * A TreePtrList is NULL-terminated.
 * Based on Tcl_DString code.
 */
#define TIL_STATIC_SPACE 128
typedef struct TreePtrList TreePtrList;
typedef TreePtrList TreeItemList;
typedef TreePtrList TreeColumnList;
struct TreePtrList {
#ifdef TREECTRL_DEBUG
    char magic[4];
#endif
    TreeCtrl *tree;
    ClientData *pointers;	/* NULL-terminated list of pointers. */
    int count;			/* Number of items. */
    int space;			/* Number of slots pointed to by pointers[]. */
    ClientData pointerSpace[TIL_STATIC_SPACE]; /* Space to use in common case
				 * where the list is small. */
};

enum { LEFT, TOP, RIGHT, BOTTOM };

struct TreeCtrlDebug
{
    Tk_OptionTable optionTable;
    int enable;			/* Turn all debugging on/off */
    int data;			/* Debug data structures */
    int display;		/* Debug display routines */
    int span;			/* Debug column spanning */
    int textLayout;		/* Debug text layout */
    int displayDelay;		/* Delay between copy/draw operations */
    XColor *eraseColor;		/* Erase "invalidated" areas */
    GC gcErase;			/* for eraseColor */
    XColor *drawColor;		/* Erase about-to-be-drawn areas */
    GC gcDraw;			/* for eraseColor */
};

struct TreeCtrlColumnDrag
{
    Tk_OptionTable optionTable;
    int enable;			/* -enable */
    TreeColumn column;		/* -imagecolumn */
    Tcl_Obj *offsetObj;		/* -imageoffset */
    int offset;			/* -imageoffset */
    XColor *color;		/* -imagecolor */
    int alpha;			/* -imagealpha */
    TreeColumn indColumn;	/* -indicatorcolumn */
    XColor *indColor;		/* -indicatorcolor */
    int indSide;		/* -indicatorside */
};

struct TreeCtrl
{
    /* Standard stuff */
    Tk_Window tkwin;
    Display *display;
    Tcl_Interp *interp;
    Tcl_Command widgetCmd;
    Tk_OptionTable optionTable;

    /* Configuration options */
    Tcl_Obj *fgObj;		/* -foreground */
    XColor *fgColorPtr;		/* -foreground */
    Tcl_Obj *borderWidthObj;	/* -borderwidth */
    int borderWidth;		/* -borderwidth */
    Tk_3DBorder border;		/* -background */
    Tk_Cursor cursor;		/* Current cursor for window, or None. */
    int relief;			/* -relief */
    Tcl_Obj *highlightWidthObj;	/* -highlightthickness */
    int highlightWidth;		/* -highlightthickness */
    XColor *highlightBgColorPtr; /* -highlightbackground */
    XColor *highlightColorPtr;	/* -highlightcolor */
    char *xScrollCmd;		/* -xscrollcommand */
    char *yScrollCmd;		/* -yscrollcommand */
    Tcl_Obj *xScrollDelay;	/* -xscrolldelay: used by scripts */
    Tcl_Obj *yScrollDelay;	/* -yscrolldelay: used by scripts */
    int xScrollIncrement;	/* -xscrollincrement */
    int yScrollIncrement;	/* -yscrollincrement */
    int xScrollSmoothing;	/* -xscrollsmoothing */
    int yScrollSmoothing;	/* -yscrollsmoothing */
#define SMOOTHING_X 0x01
#define SMOOTHING_Y 0x02
    int scrollSmoothing;	/* */
    Tcl_Obj *scrollMargin;	/* -scrollmargin: used by scripts */
    char *takeFocus;		/* -takfocus */
    Tcl_Obj *fontObj;		/* -font */
    Tk_Font tkfont;		/* -font */
    int showButtons;		/* boolean: Draw expand/collapse buttons */
    int showLines;		/* boolean: Draw lines connecting parent to
				 * child */
    int showRootLines;		/* boolean: Draw lines connecting children
				 * of the root item */
    int showRoot;		/* boolean: Draw the unique root item */
    int showRootButton;		/* boolean: Draw expand/collapse button for
				 * root item */
    int showRootChildButtons;	/* boolean: Draw expand/collapse buttons for
				 * children of the root item */
    int showHeader;		/* boolean: show column titles */
    Tcl_Obj *indentObj;		/* pixels: offset of child relative to
				 * parent */
    int indent;			/* pixels: offset of child relative to
				 * parent */
    char *selectMode;		/* -selectmode: used by scripts only */
    Tcl_Obj *itemHeightObj;	/* -itemheight: Fixed height for all items
                                    (unless overridden) */
    int itemHeight;		/* -itemheight */
    Tcl_Obj *minItemHeightObj;	/* -minitemheight: Minimum height for all items */
    int minItemHeight;		/* -minitemheight */
    Tcl_Obj *itemWidthObj;	/* -itemwidth */
    int itemWidth;		/* -itemwidth */
    int itemWidthEqual;		/* -itemwidthequal */
    Tcl_Obj *itemWidMultObj;	/* -itemwidthmultiple */
    int itemWidMult;		/* -itemwidthmultiple */
    Tcl_Obj *widthObj;		/* -width */
    int width;			/* -width */
    Tcl_Obj *heightObj;		/* -height */
    int height;			/* -height */
    TreeColumn columnTree;	/* column where buttons/lines are drawn */
#define DOUBLEBUFFER_NONE 0
#define DOUBLEBUFFER_ITEM 1
#define DOUBLEBUFFER_WINDOW 2
    int doubleBuffer;		/* -doublebuffer */
    XColor *buttonColor;	/* -buttoncolor */
    Tcl_Obj *buttonSizeObj;	/* -buttonSize */
    int buttonSize;		/* -buttonsize */
    Tcl_Obj *buttonThicknessObj;/* -buttonthickness */
    int buttonThickness;	/* -buttonthickness */
    int buttonTracking;		/* -buttontracking */
    XColor *lineColor;		/* -linecolor */
    Tcl_Obj *lineThicknessObj;	/* -linethickness */
    int lineThickness;		/* -linethickness */
#define LINE_STYLE_DOT 0
#define LINE_STYLE_SOLID 1
    int lineStyle;		/* -linestyle */
    int vertical;		/* -orient */
    Tcl_Obj *wrapObj;		/* -wrap */
    PerStateInfo buttonImage;	/* -buttonimage */
    PerStateInfo buttonBitmap;	/* -buttonbitmap */
    char *backgroundImageString; /* -backgroundimage */
    Tk_Anchor bgImageAnchor;	/* -bgimageanchor */
    int bgImageOpaque;		/* -bgimageopaque */
#define BGIMG_SCROLL_X 0x01
#define BGIMG_SCROLL_Y 0x02
    Tcl_Obj *bgImageScrollObj;	/* -bgimagescroll */
    int bgImageScroll;		/* -bgimagescroll */
#define BGIMG_TILE_X 0x01
#define BGIMG_TILE_Y 0x02
    Tcl_Obj *bgImageTileObj;	/* -bgimagetile */
    int bgImageTile;		/* -bgimagetile */
    int useIndent;		/* MAX of open/closed button width and
				 * indent */
    int buttonHeightMax;	/* Maximum height of a button in any state. */
#define BG_MODE_COLUMN 0
#define BG_MODE_ORDER 1
#define BG_MODE_ORDERVIS 2
#define BG_MODE_ROW 3
#ifdef DEPRECATED
#define BG_MODE_INDEX 4		/* compatibility */
#define BG_MODE_VISINDEX 5	/* compatibility */
#endif /* DEPRECATED */
    int backgroundMode;		/* -backgroundmode */
    int columnResizeMode;	/* -columnresizemode */

    int *canvasPadX;		/* -canvaspadx */
    Tcl_Obj *canvasPadXObj;	/* -canvaspadx */
    int *canvasPadY;		/* -canvaspady */
    Tcl_Obj *canvasPadYObj;	/* -canvaspady */

    int itemGapX;		/* -itemgapx */
    Tcl_Obj *itemGapXObj;	/* -itemgapx */
    int itemGapY;		/* -itemgapy */
    Tcl_Obj *itemGapYObj;	/* -itemgapy */

    struct TreeCtrlDebug debug;
    struct TreeCtrlColumnDrag columnDrag;

    /* Other stuff */
    int gotFocus;		/* flag */
    int deleted;		/* flag */
    int updateIndex;		/* flag */
    int isActive;		/* flag: mac & win "active" toplevel */
    struct {
	int left;
	int top;
	int right;
	int bottom;
    } inset;			/* borderWidth + highlightWidth */
    int xOrigin;		/* offset from content x=0 to window x=0 */
    int yOrigin;		/* offset from content y=0 to window y=0 */
    GC copyGC;
    GC textGC;
    GC buttonGC;
    GC lineGC[2];
    Tk_Image backgroundImage;	/* -backgroundimage */
    int useTheme;		/* -usetheme */
    char *itemPrefix;		/* -itemprefix */
    char *columnPrefix;		/* -columnprefix */

    int prevWidth;
    int prevHeight;
    int drawableXOrigin;
    int drawableYOrigin;

    TreeColumn columns;		/* List of columns */
    TreeColumn columnLast;	/* Last in list of columns */
    TreeColumn columnTail;	/* Last infinitely-wide column */
    TreeColumn columnVis;	/* First visible non-locked column */
    int columnCount;		/* Number of columns */
    int columnCountVis;		/* Number of visible columns */
    int headerHeight;		/* Height of column titles */
    int widthOfColumns;		/* Sum of column widths */
    int columnTreeLeft;		/* left of column where buttons/lines are
				 * drawn */
    int columnTreeVis;		/* TRUE if columnTree is visible */
    int columnBgCnt;		/* Max -itembackground colors */
#if COLUMNGRID == 1
    int columnsWithGridLines;	/* # visible columns with grid lines. */
#endif

#define COLUMN_LOCK_LEFT 0
#define COLUMN_LOCK_NONE 1
#define COLUMN_LOCK_RIGHT 2
    TreeColumn columnLockLeft;	/* First left-locked column */
    TreeColumn columnLockNone;	/* First unlocked column */
    TreeColumn columnLockRight;	/* First right-locked column */
    int widthOfColumnsLeft;	/* Sum of left-locked column widths */
    int widthOfColumnsRight;	/* Sum of right-locked column widths */
    int columnCountVisLeft;	/* Number of visible left-locked columns */
    int columnCountVisRight;	/* Number of visible right-locked columns */

#define UNIFORM_GROUP
#ifdef UNIFORM_GROUP
    Tcl_HashTable uniformGroupHash;	/* -uniform -> UniformGroup */
#endif

    XColor *defColumnTextColor;	/* Default column header text color when
    				 * the column's -textcolor option
    				 * is not specified and the system theme
    				 * doesn't specify a color. */

    TreeItem root;
    TreeItem activeItem;
    TreeItem anchorItem;
    int nextItemId;
    int nextColumnId;
    Tcl_HashTable itemHash;	/* TreeItem.id -> TreeItem */
    Tcl_HashTable itemSpansHash; /* TreeItem -> nothing */
    Tcl_HashTable elementHash;	/* Element.name -> Element */
    Tcl_HashTable styleHash;	/* Style.name -> Style */
    Tcl_HashTable imageNameHash;  /* image name -> TreeImageRef */
    Tcl_HashTable imageTokenHash; /* Tk_Image -> TreeImageRef */
    int depth;			/* max depth of items under root */
    int itemCount;		/* Total number of items */
    int itemVisCount;		/* Total number of ReallyVisible() items */
    int itemWrapCount;		/* ReallyVisible() items with -wrap=true */
    QE_BindingTable bindingTable;
    TreeDragImage dragImage;
    TreeMarquee marquee;
    TreeDInfo dInfo;
    int selectCount;		/* Number of selected items */
    Tcl_HashTable selection;	/* Selected items */

#define TREE_WRAP_NONE 0
#define TREE_WRAP_ITEMS 1
#define TREE_WRAP_PIXELS 2
#define TREE_WRAP_WINDOW 3
    int wrapMode;		/* TREE_WRAP_xxx */
    int wrapArg;		/* Number of items, number of pixels */

    int totalWidth;		/* Max/Sum of all TreeRanges */
    int totalHeight;		/* Max/Sum of all TreeRanges */

    struct {
	Tcl_Obj *xObj;
	int x;			/* Window coords */
	int sx;			/* Window coords */
	int onScreen;
    } columnProxy;

    char *stateNames[32];	/* Names of static and dynamic item states */

    int scanX;			/* [scan mark] and [scan dragto] */
    int scanY;
    int scanXOrigin;
    int scanYOrigin;

    Tk_OptionTable styleOptionTable;
#ifdef DEPRECATED
    struct {
	Tcl_Obj *stylesObj;
	TreeStyle *styles;
	int numStyles;
    } defaultStyle;
#endif /* DEPRECATED */
    Tk_OptionTable itemOptionTable;
    int itemPrefixLen;		/* -itemprefix */
    int columnPrefixLen;	/* -columnprefix */
#ifdef ALLOC_HAX
    ClientData allocData;
#endif
    int preserveItemRefCnt;	/* Ref count so items-in-use aren't freed. */
    TreeItemList preserveItemList;	/* List of items to be deleted when
				 * preserveItemRefCnt==0. */

    struct {
	Tcl_Obj *yObj;
	int y;			/* Window coords */
	int sy;			/* Window coords */
	int onScreen;
    } rowProxy;

    char *optionHax[64];	/* Used by OptionHax_xxx */
    int optionHaxCnt;		/* Used by OptionHax_xxx */

    TreeThemeData themeData;
    GCCache *gcCache;		/* Graphics contexts for elements. */

    TkRegion regionStack[8];	/* Temp region stack. */
    int regionStackLen;		/* Number of unused regions in regionStack. */

    int itemTagExpr;		/* Enable/disable operators in item tags */
    int columnTagExpr;		/* Enable/disable operators in column tags */

    Tk_OptionTable gradientOptionTable;
    Tcl_HashTable gradientHash;	/* TreeGradient.name -> TreeGradient */
    int nativeGradients;	/* Preference, not availability. */
};

#define TREE_CONF_FONT 0x0001
#define TREE_CONF_ITEMSIZE 0x0002
#define TREE_CONF_INDENT 0x0004
#define TREE_CONF_WRAP 0x0008
#define TREE_CONF_BUTIMG 0x0010
#define TREE_CONF_BUTBMP 0x0020
#define TREE_CONF_BORDERS 0x0040
#define TREE_CONF_BGIMGOPT 0x0080
#define TREE_CONF_RELAYOUT 0x0100
#define TREE_CONF_REDISPLAY 0x0200
#define TREE_CONF_FG 0x0400
#define TREE_CONF_PROXY 0x0800
#define TREE_CONF_BUTTON 0x1000
#define TREE_CONF_LINE 0x2000
#define TREE_CONF_DEFSTYLE 0x4000
#define TREE_CONF_BG_IMAGE 0x8000
#define TREE_CONF_THEME 0x00010000

MODULE_SCOPE void Tree_AddItem(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void Tree_RemoveItem(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE Tk_Image Tree_GetImage(TreeCtrl *tree, char *imageName);
MODULE_SCOPE void Tree_FreeImage(TreeCtrl *tree, Tk_Image image);
MODULE_SCOPE void Tree_UpdateScrollbarX(TreeCtrl *tree);
MODULE_SCOPE void Tree_UpdateScrollbarY(TreeCtrl *tree);
MODULE_SCOPE void Tree_AddToSelection(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void Tree_RemoveFromSelection(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void Tree_PreserveItems(TreeCtrl *tree);
MODULE_SCOPE void Tree_ReleaseItems(TreeCtrl *tree);

MODULE_SCOPE int TreeArea_FromObj(TreeCtrl *tree, Tcl_Obj *objPtr,
    int *areaPtr);

#define STATE_OP_ON	0
#define STATE_OP_OFF	1
#define STATE_OP_TOGGLE	2
#define SFO_NOT_OFF	0x0001
#define SFO_NOT_TOGGLE	0x0002
#define SFO_NOT_STATIC	0x0004
MODULE_SCOPE int Tree_StateFromObj(TreeCtrl *tree, Tcl_Obj *obj, int states[3], int *indexPtr, int flags);
MODULE_SCOPE int Tree_StateFromListObj(TreeCtrl *tree, Tcl_Obj *obj, int states[3], int flags);

#define Tree_BorderLeft(tree) \
    tree->inset.left
#define Tree_BorderTop(tree) \
    tree->inset.top
#define Tree_BorderRight(tree) \
    (Tk_Width(tree->tkwin) - tree->inset.right)
#define Tree_BorderBottom(tree) \
    (Tk_Height(tree->tkwin) - tree->inset.bottom)

#define Tree_HeaderLeft(tree) \
    Tree_BorderLeft(tree)
#define Tree_HeaderTop(tree) \
    Tree_BorderTop(tree)
#define Tree_HeaderRight(tree) \
    Tree_BorderRight(tree)
#define Tree_HeaderBottom(tree) \
    (Tree_BorderTop(tree) + Tree_HeaderHeight(tree))
#define Tree_HeaderWidth(tree) \
    (Tree_HeaderRight(tree) - Tree_HeaderLeft(tree))

#define Tree_ContentLeft(tree) \
    (Tree_BorderLeft(tree) + Tree_WidthOfLeftColumns(tree))
#define Tree_ContentTop(tree) \
    (Tree_BorderTop(tree) + Tree_HeaderHeight(tree))
#define Tree_ContentRight(tree) \
    (Tree_BorderRight(tree) - Tree_WidthOfRightColumns(tree))
#define Tree_ContentBottom(tree) \
    Tree_BorderBottom(tree)

#define Tree_ContentWidth(tree) \
    (Tree_ContentRight(tree) - Tree_ContentLeft(tree))
#define Tree_ContentHeight(tree) \
    (Tree_ContentBottom(tree) - Tree_ContentTop(tree))

/* tkTreeItem.c */

#define ITEM_ALL ((TreeItem) -1)
#define IFO_NOT_MANY	0x0001	/* ItemFromObj flag: > 1 item is not ok */
#define IFO_NOT_NULL	0x0002	/* ItemFromObj flag: can't be NULL */
#define IFO_NOT_ROOT	0x0004	/* ItemFromObj flag: "root" is forbidden */
#define IFO_NOT_ORPHAN	0x0008	/* ItemFromObj flag: item must have a parent */
#define IFO_LIST_ALL	0x0010	/* ItemFromObj flag: return "all" as list */
MODULE_SCOPE int TreeItemList_FromObj(TreeCtrl *tree, Tcl_Obj *objPtr, TreeItemList *items, int flags);
MODULE_SCOPE int TreeItem_FromObj(TreeCtrl *tree, Tcl_Obj *objPtr, TreeItem *itemPtr, int flags);

typedef struct ItemForEach ItemForEach;
struct ItemForEach {
    TreeCtrl *tree;
    int error;
    int all;
    Tcl_HashSearch search;
    TreeItem last;
    TreeItem item;
    TreeItemList *items;
    int index;
};
MODULE_SCOPE TreeItem TreeItemForEach_Start(TreeItemList *items, TreeItemList *item2s,
    ItemForEach *iter);
MODULE_SCOPE TreeItem TreeItemForEach_Next(ItemForEach *iter);
#define ITEM_FOR_EACH(item, items, item2s, iter) \
    for (item = TreeItemForEach_Start(items, item2s, iter); \
	 item != NULL; \
	 item = TreeItemForEach_Next(iter))

#define FormatResult TreeCtrl_FormatResult
MODULE_SCOPE void FormatResult(Tcl_Interp *interp, char *fmt, ...);
#define DStringAppendf TreeCtrl_DStringAppendf
MODULE_SCOPE void DStringAppendf(Tcl_DString *dString, char *fmt, ...);
MODULE_SCOPE void Tree_Debug(TreeCtrl *tree);

MODULE_SCOPE int TreeItem_Init(TreeCtrl *tree);
MODULE_SCOPE int TreeItem_Debug(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void TreeItem_OpenClose(TreeCtrl *tree, TreeItem item, int mode);
MODULE_SCOPE void TreeItem_Delete(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE int TreeItem_Deleted(TreeCtrl *tree, TreeItem item);

#define STATE_OPEN	0x0001
#define STATE_SELECTED	0x0002
#define STATE_ENABLED	0x0004
#define STATE_ACTIVE	0x0008
#define STATE_FOCUS	0x0010
#define STATE_USER	6		/* first user bit */
MODULE_SCOPE int TreeItem_GetState(TreeCtrl *tree, TreeItem item_);

/* State flags for button state, needed by themes */
/* FIXME: These may conflict with [state define] states */
#define BUTTON_STATE_ACTIVE (1<<30)
#define BUTTON_STATE_PRESSED (1<<31)

#define CS_DISPLAY 0x01
#define CS_LAYOUT 0x02
MODULE_SCOPE int TreeItem_ChangeState(TreeCtrl *tree, TreeItem item_, int stateOff, int stateOn);

MODULE_SCOPE void TreeItem_UndefineState(TreeCtrl *tree, TreeItem item_, int state);

MODULE_SCOPE int TreeItem_HasButton(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE int TreeItem_IsPointInButton(TreeCtrl *tree, TreeItem item_, int x, int y);
MODULE_SCOPE int TreeItem_GetDepth(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE int TreeItem_GetID(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE int TreeItem_SetID(TreeCtrl *tree, TreeItem item_, int id);
MODULE_SCOPE int TreeItem_GetEnabled(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE int TreeItem_GetSelected(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE int TreeItem_CanAddToSelection(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE int TreeItem_GetWrap(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE TreeItem TreeItem_GetParent(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem TreeItem_GetNextSibling(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem TreeItem_NextSiblingVisible(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void TreeItem_SetDInfo(TreeCtrl *tree, TreeItem item, TreeItemDInfo dInfo);
MODULE_SCOPE TreeItemDInfo TreeItem_GetDInfo(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void TreeItem_SetRInfo(TreeCtrl *tree, TreeItem item, TreeItemRInfo rInfo);
MODULE_SCOPE TreeItemRInfo TreeItem_GetRInfo(TreeCtrl *tree, TreeItem item);

MODULE_SCOPE void TreeItem_AppendChild(TreeCtrl *tree, TreeItem self, TreeItem child);
MODULE_SCOPE void TreeItem_RemoveFromParent(TreeCtrl *tree, TreeItem self);
MODULE_SCOPE int TreeItem_FirstAndLast(TreeCtrl *tree, TreeItem *first, TreeItem *last);
MODULE_SCOPE void TreeItem_ListDescendants(TreeCtrl *tree, TreeItem item_, TreeItemList *items);
MODULE_SCOPE void TreeItem_UpdateDepth(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void TreeItem_AddToParent(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE int TreeItem_Height(TreeCtrl *tree, TreeItem self);
MODULE_SCOPE int TreeItem_TotalHeight(TreeCtrl *tree, TreeItem self);
MODULE_SCOPE void TreeItem_InvalidateHeight(TreeCtrl *tree, TreeItem self);
MODULE_SCOPE void TreeItem_SpansInvalidate(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE int TreeItem_SpansRedo(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE void TreeItem_SpansRedoIfNeeded(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE int *TreeItem_GetSpans(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE void TreeItem_Draw(TreeCtrl *tree, TreeItem self, int lock, int x, int y, int width, int height, TreeDrawable td, int minX, int maxX, int index);
MODULE_SCOPE void TreeItem_DrawLines(TreeCtrl *tree, TreeItem self, int x, int y, int width, int height, TreeDrawable td, TreeStyle style);
MODULE_SCOPE void TreeItem_DrawButton(TreeCtrl *tree, TreeItem self, int x, int y, int width, int height, TreeDrawable td, TreeStyle style);
MODULE_SCOPE int TreeItem_ReallyVisible(TreeCtrl *tree, TreeItem self);
MODULE_SCOPE void TreeItem_FreeResources(TreeCtrl *tree, TreeItem self);
MODULE_SCOPE void TreeItem_Release(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem TreeItem_RootAncestor(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE int TreeItem_IsAncestor(TreeCtrl *tree, TreeItem item1, TreeItem item2);
MODULE_SCOPE Tcl_Obj *TreeItem_ToObj(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void TreeItem_ToIndex(TreeCtrl *tree, TreeItem item, int *absolute, int *visible);
MODULE_SCOPE TreeItem TreeItem_Next(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem TreeItem_NextVisible(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem TreeItem_Prev(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem TreeItem_PrevVisible(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void TreeItem_Identify(TreeCtrl *tree, TreeItem item_, int lock, int x, int y, char *buf);
MODULE_SCOPE void TreeItem_Identify2(TreeCtrl *tree, TreeItem item_,
	int x1, int y1, int x2, int y2, Tcl_Obj *listObj);
MODULE_SCOPE int TreeItem_GetRects(TreeCtrl *tree, TreeItem item_,
    TreeColumn treeColumn, int objc, Tcl_Obj *CONST objv[], TreeRectangle rects[]);
MODULE_SCOPE int TreeItem_Indent(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE void Tree_UpdateItemIndex(TreeCtrl *tree);
MODULE_SCOPE void Tree_DeselectHidden(TreeCtrl *tree);
MODULE_SCOPE int TreeItemCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
MODULE_SCOPE void TreeItem_UpdateWindowPositions(TreeCtrl *tree, TreeItem item_,
    int lock, int x, int y, int width, int height);
MODULE_SCOPE void TreeItem_OnScreen(TreeCtrl *tree, TreeItem item_, int onScreen);

MODULE_SCOPE TreeItemColumn TreeItem_GetFirstColumn(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItemColumn TreeItemColumn_GetNext(TreeCtrl *tree, TreeItemColumn column);
MODULE_SCOPE void TreeItemColumn_InvalidateSize(TreeCtrl *tree, TreeItemColumn column);
MODULE_SCOPE TreeStyle TreeItemColumn_GetStyle(TreeCtrl *tree, TreeItemColumn column);
MODULE_SCOPE int TreeItemColumn_Index(TreeCtrl *tree, TreeItem item_, TreeItemColumn column_);
MODULE_SCOPE void TreeItemColumn_ForgetStyle(TreeCtrl *tree, TreeItemColumn column_);
MODULE_SCOPE int TreeItemColumn_NeededWidth(TreeCtrl *tree, TreeItem item_, TreeItemColumn column_);
MODULE_SCOPE TreeItemColumn TreeItem_FindColumn(TreeCtrl *tree, TreeItem item, int columnIndex);
MODULE_SCOPE int TreeItem_ColumnFromObj(TreeCtrl *tree, TreeItem item, Tcl_Obj *obj, TreeItemColumn *columnPtr, int *indexPtr);
MODULE_SCOPE void TreeItem_RemoveColumns(TreeCtrl *tree, TreeItem item_, int first, int last);
MODULE_SCOPE void TreeItem_RemoveAllColumns(TreeCtrl *tree, TreeItem item_);
MODULE_SCOPE void TreeItem_MoveColumn(TreeCtrl *tree, TreeItem item_, int columnIndex, int beforeIndex);

/* tkTreeElem.c */
MODULE_SCOPE int TreeElement_Init(Tcl_Interp *interp);
MODULE_SCOPE int TreeStateFromObj(TreeCtrl *tree, Tcl_Obj *obj, int *stateOff, int *stateOn);
MODULE_SCOPE int StringTableCO_Init(Tk_OptionSpec *optionTable, CONST char *optionName, CONST char **tablePtr);

typedef struct StyleDrawArgs StyleDrawArgs;
struct StyleDrawArgs
{
    TreeCtrl *tree;
    TreeColumn column; /* needed for gradients */
    TreeItem item; /* needed for gradients */
    TreeStyle style;
    int indent;
    int x;
    int y;
    int width;
    int height;
    TreeDrawable td;
    int state;		/* STATE_xxx */
    Tk_Justify justify;
    TreeRectangle bounds;
};

/* tkTreeStyle.c */
MODULE_SCOPE int TreeStyle_Init(TreeCtrl *tree);
MODULE_SCOPE int TreeStyle_NeededWidth(TreeCtrl *tree, TreeStyle style_, int state);
MODULE_SCOPE int TreeStyle_NeededHeight(TreeCtrl *tree, TreeStyle style_, int state);
MODULE_SCOPE int TreeStyle_UseHeight(StyleDrawArgs *drawArgs);
MODULE_SCOPE void TreeStyle_Draw(StyleDrawArgs *args);
MODULE_SCOPE void TreeStyle_FreeResources(TreeCtrl *tree, TreeStyle style_);
MODULE_SCOPE void TreeStyle_Free(TreeCtrl *tree);
MODULE_SCOPE int TreeElement_FromObj(TreeCtrl *tree, Tcl_Obj *obj, TreeElement *elemPtr);
MODULE_SCOPE int TreeElement_IsType(TreeCtrl *tree, TreeElement elem, CONST char *type);
MODULE_SCOPE int TreeStyle_FromObj(TreeCtrl *tree, Tcl_Obj *obj, TreeStyle *stylePtr);
MODULE_SCOPE Tcl_Obj *TreeStyle_ToObj(TreeStyle style_);
MODULE_SCOPE Tcl_Obj *TreeStyle_GetImage(TreeCtrl *tree, TreeStyle style_);
MODULE_SCOPE Tcl_Obj *TreeStyle_GetText(TreeCtrl *tree, TreeStyle style_);
MODULE_SCOPE int TreeStyle_SetImage(TreeCtrl *tree, TreeItem item, TreeItemColumn column, TreeStyle style_, Tcl_Obj *textObj);
MODULE_SCOPE int TreeStyle_SetText(TreeCtrl *tree, TreeItem item, TreeItemColumn column, TreeStyle style_, Tcl_Obj *textObj);
MODULE_SCOPE int TreeStyle_FindElement(TreeCtrl *tree, TreeStyle style_, TreeElement elem, int *index);
MODULE_SCOPE TreeStyle TreeStyle_NewInstance(TreeCtrl *tree, TreeStyle master);
MODULE_SCOPE int TreeStyle_ElementActual(TreeCtrl *tree, TreeStyle style_, int state, Tcl_Obj *elemObj, Tcl_Obj *obj);
MODULE_SCOPE int TreeStyle_ElementCget(TreeCtrl *tree, TreeItem item, TreeItemColumn column, TreeStyle style_, Tcl_Obj *elemObj, Tcl_Obj *obj);
MODULE_SCOPE int TreeStyle_ElementConfigure(TreeCtrl *tree, TreeItem item, TreeItemColumn column, TreeStyle style_, Tcl_Obj *elemObj, int objc, Tcl_Obj **objv, int *eMask);
MODULE_SCOPE void TreeStyle_ListElements(TreeCtrl *tree, TreeStyle style_);
MODULE_SCOPE int TreeStyle_GetButtonY(TreeCtrl *tree, TreeStyle style_);
MODULE_SCOPE TreeStyle TreeStyle_GetMaster(TreeCtrl *tree, TreeStyle style_);
MODULE_SCOPE char *TreeStyle_Identify(StyleDrawArgs *drawArgs, int x, int y);
MODULE_SCOPE void TreeStyle_Identify2(StyleDrawArgs *drawArgs,
	int x1, int y1, int x2, int y2, Tcl_Obj *listObj);
MODULE_SCOPE int TreeStyle_Remap(TreeCtrl *tree, TreeStyle styleFrom_, TreeStyle styleTo_, int objc, Tcl_Obj *CONST objv[]);
MODULE_SCOPE void TreeStyle_TreeChanged(TreeCtrl *tree, int flagT);
#define SORT_ASCII 0
#define SORT_DICT  1
#define SORT_DOUBLE 2
#define SORT_LONG 3
#define SORT_COMMAND 4
MODULE_SCOPE int TreeStyle_GetSortData(TreeCtrl *tree, TreeStyle style_, int elemIndex, int type, long *lv, double *dv, char **sv);
#if 0
MODULE_SCOPE int TreeStyle_ValidateElements(TreeCtrl *tree, TreeStyle style_, int objc, Tcl_Obj *CONST objv[]);
#endif
MODULE_SCOPE int TreeStyle_GetElemRects(StyleDrawArgs *drawArgs, int objc, Tcl_Obj *CONST objv[], TreeRectangle rects[]);
MODULE_SCOPE int TreeElementCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
MODULE_SCOPE int TreeStyleCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
MODULE_SCOPE int TreeStyle_ChangeState(TreeCtrl *tree, TreeStyle style_, int state1, int state2);
MODULE_SCOPE void Tree_UndefineState(TreeCtrl *tree, int state);
MODULE_SCOPE int TreeStyle_NumElements(TreeCtrl *tree, TreeStyle style_);
MODULE_SCOPE void TreeStyle_UpdateWindowPositions(StyleDrawArgs *drawArgs);
MODULE_SCOPE void TreeStyle_OnScreen(TreeCtrl *tree, TreeStyle style_, int onScreen);

MODULE_SCOPE void Tree_ButtonMaxSize(TreeCtrl *tree, int *maxWidth, int *maxHeight);
MODULE_SCOPE int Tree_ButtonHeight(TreeCtrl *tree, int state);

/* tkTreeNotify.c */
MODULE_SCOPE int TreeNotify_Init(TreeCtrl *tree);
MODULE_SCOPE void TreeNotify_OpenClose(TreeCtrl *tree, TreeItem item, int isOpen, int before);
MODULE_SCOPE void TreeNotify_Selection(TreeCtrl *tree, TreeItemList *select, TreeItemList *deselect);
MODULE_SCOPE int TreeNotifyCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
MODULE_SCOPE void TreeNotify_ActiveItem(TreeCtrl *tree, TreeItem itemOld, TreeItem itemNew);
MODULE_SCOPE void TreeNotify_Scroll(TreeCtrl *tree, double fractions[2], int vertical);
MODULE_SCOPE void TreeNotify_ItemDeleted(TreeCtrl *tree, TreeItemList *items);
MODULE_SCOPE void TreeNotify_ItemVisibility(TreeCtrl *tree, TreeItemList *v, TreeItemList *h);

/* tkTreeColumn.c */
MODULE_SCOPE Tk_ObjCustomOption TreeCtrlCO_column;
MODULE_SCOPE Tk_ObjCustomOption TreeCtrlCO_column_NOT_TAIL;
MODULE_SCOPE int TreeColumn_InitInterp(Tcl_Interp *interp);
MODULE_SCOPE void Tree_InitColumns(TreeCtrl *tree);
MODULE_SCOPE TreeColumn Tree_FindColumn(TreeCtrl *tree, int columnIndex);
MODULE_SCOPE int TreeColumn_FirstAndLast(TreeColumn *first, TreeColumn *last);

#define COLUMN_ALL ((TreeColumn) -1)	/* Every column. */
#define COLUMN_NTAIL ((TreeColumn) -2)	/* Every column but the tail. */
#define CFO_NOT_MANY 0x01
#define CFO_NOT_NULL 0x02
#define CFO_NOT_TAIL 0x04
#define CFO_LIST_ALL 0x08
MODULE_SCOPE int TreeColumnList_FromObj(TreeCtrl *tree, Tcl_Obj *objPtr, TreeColumnList *columns, int flags);
MODULE_SCOPE int TreeColumn_FromObj(TreeCtrl *tree, Tcl_Obj *objPtr, TreeColumn *columnPtr, int flags);

/* Values for TreeColumn -arrow option, needed by themes */
#define COLUMN_ARROW_NONE 0
#define COLUMN_ARROW_UP 1
#define COLUMN_ARROW_DOWN 2

/* Values for TreeColumn -state option, needed by themes */
#define COLUMN_STATE_NORMAL 0
#define COLUMN_STATE_ACTIVE 1
#define COLUMN_STATE_PRESSED 2

typedef struct ColumnForEach ColumnForEach;
struct ColumnForEach {
    TreeCtrl *tree;
    int error;
    int all;
    int ntail;
    TreeColumn current;
    TreeColumn next;
    TreeColumn last;
    TreeColumnList *list;
    int index;
};
MODULE_SCOPE TreeColumn TreeColumnForEach_Start(TreeColumnList *columns,
    TreeColumnList *column2s, ColumnForEach *iter);
MODULE_SCOPE TreeColumn TreeColumnForEach_Next(ColumnForEach *iter);
#define COLUMN_FOR_EACH(column, columns, column2s, iter) \
    for (column = TreeColumnForEach_Start(columns, column2s, iter); \
	 column != NULL; \
	 column = TreeColumnForEach_Next(iter))
    
MODULE_SCOPE Tcl_Obj *TreeColumn_ToObj(TreeCtrl *tree, TreeColumn column_);
MODULE_SCOPE int TreeColumnCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
MODULE_SCOPE int TreeColumn_GetID(TreeColumn column_);
MODULE_SCOPE int TreeColumn_Index(TreeColumn column_);
MODULE_SCOPE TreeColumn TreeColumn_Next(TreeColumn column_);
MODULE_SCOPE TreeColumn TreeColumn_Prev(TreeColumn column_);
MODULE_SCOPE int TreeColumn_FixedWidth(TreeColumn column_);
MODULE_SCOPE int TreeColumn_MinWidth(TreeColumn column_);
MODULE_SCOPE int TreeColumn_MaxWidth(TreeColumn column_);
MODULE_SCOPE int TreeColumn_NeededWidth(TreeColumn column_);
MODULE_SCOPE int TreeColumn_NeededHeight(TreeColumn column_);
MODULE_SCOPE int TreeColumn_UseWidth(TreeColumn column_);
MODULE_SCOPE int TreeColumn_Offset(TreeColumn column_);
MODULE_SCOPE Tk_Justify TreeColumn_ItemJustify(TreeColumn column_);
#ifdef DEPRECATED
MODULE_SCOPE int TreeColumn_WidthHack(TreeColumn column_);
MODULE_SCOPE int TreeColumn_StepWidth(TreeColumn column_);
#endif
MODULE_SCOPE TreeStyle TreeColumn_ItemStyle(TreeColumn column_);
MODULE_SCOPE void TreeColumn_StyleDeleted(TreeColumn column_, TreeStyle style);
MODULE_SCOPE int TreeColumn_Visible(TreeColumn column_);
MODULE_SCOPE int TreeColumn_Squeeze(TreeColumn column_);
MODULE_SCOPE int TreeColumn_BackgroundCount(TreeColumn column_);
MODULE_SCOPE TreeColor *TreeColumn_BackgroundColor(TreeColumn column_, int which);
#if COLUMNGRID==1
MODULE_SCOPE int TreeColumn_GridColors(TreeColumn column, TreeColor **leftColorPtr,
    TreeColor **rightColorPtr, int *leftWidthPtr, int *rightWidthPtr);
#endif
MODULE_SCOPE void Tree_DrawHeader(TreeCtrl *tree, TreeDrawable td, int x, int y);
MODULE_SCOPE int TreeColumn_WidthOfItems(TreeColumn column_);
MODULE_SCOPE void TreeColumn_InvalidateWidth(TreeColumn column_);
MODULE_SCOPE void TreeColumn_Init(TreeCtrl *tree);
MODULE_SCOPE void Tree_FreeColumns(TreeCtrl *tree);
MODULE_SCOPE void Tree_InvalidateColumnWidth(TreeCtrl *tree, TreeColumn column);
MODULE_SCOPE void Tree_InvalidateColumnHeight(TreeCtrl *tree, TreeColumn column);
MODULE_SCOPE int Tree_HeaderHeight(TreeCtrl *tree);
MODULE_SCOPE int TreeColumn_Bbox(TreeColumn column, int *x, int *y, int *w, int *h);
MODULE_SCOPE TreeColumn Tree_HeaderUnderPoint(TreeCtrl *tree, int *x_, int *y_, int *w, int *h, int nearest);
MODULE_SCOPE int TreeColumn_Lock(TreeColumn column_);
MODULE_SCOPE int Tree_WidthOfColumns(TreeCtrl *tree);
MODULE_SCOPE int Tree_WidthOfLeftColumns(TreeCtrl *tree);
MODULE_SCOPE int Tree_WidthOfRightColumns(TreeCtrl *tree);
MODULE_SCOPE void TreeColumn_TreeChanged(TreeCtrl *tree, int flagT);
MODULE_SCOPE void TreeColumn_SetDInfo(TreeColumn column, TreeColumnDInfo dInfo);
MODULE_SCOPE TreeColumnDInfo TreeColumn_GetDInfo(TreeColumn column);

/* tkTreeDrag.c */
MODULE_SCOPE int TreeDragImage_Init(TreeCtrl *tree);
MODULE_SCOPE void TreeDragImage_Free(TreeDragImage dragImage_);
MODULE_SCOPE int TreeDragImage_IsXOR(TreeDragImage dragImage_);
MODULE_SCOPE int TreeDragImage_IsVisible(TreeDragImage dragImage_);
MODULE_SCOPE void TreeDragImage_Display(TreeDragImage dragImage_);
MODULE_SCOPE void TreeDragImage_Undisplay(TreeDragImage dragImage_);
MODULE_SCOPE void TreeDragImage_DrawXOR(TreeDragImage dragImage_, Drawable drawable, int x, int y);
MODULE_SCOPE void TreeDragImage_Draw(TreeDragImage dragImage, TreeDrawable td);
MODULE_SCOPE int TreeDragImageCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);

/* tkTreeMarquee.c */
MODULE_SCOPE int TreeMarquee_Init(TreeCtrl *tree);
MODULE_SCOPE void TreeMarquee_Free(TreeMarquee marquee_);
MODULE_SCOPE int TreeMarquee_IsXOR(TreeMarquee marquee_);
MODULE_SCOPE int TreeMarquee_IsVisible(TreeMarquee marquee_);
MODULE_SCOPE void TreeMarquee_DrawXOR(TreeMarquee marquee_, Drawable drawable, int x, int y);
MODULE_SCOPE void TreeMarquee_Display(TreeMarquee marquee_);
MODULE_SCOPE void TreeMarquee_Undisplay(TreeMarquee marquee_);
MODULE_SCOPE void TreeMarquee_Draw(TreeMarquee marquee_, TreeDrawable td);
MODULE_SCOPE int TreeMarqueeCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);

/* tkTreeDisplay.c */
MODULE_SCOPE int Tree_CanvasWidth(TreeCtrl *tree);
MODULE_SCOPE int Tree_CanvasHeight(TreeCtrl *tree);
MODULE_SCOPE TreeItem Tree_ItemUnderPoint(TreeCtrl *tree, int *x, int *y, int nearest);
MODULE_SCOPE void Tree_FreeItemRInfo(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE int Tree_ItemBbox(TreeCtrl *tree, TreeItem item, int lock, TreeRectangle *tr);
MODULE_SCOPE TreeItem Tree_ItemLARB(TreeCtrl *tree, TreeItem item, int vertical, int prev);
MODULE_SCOPE TreeItem Tree_ItemAbove(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem Tree_ItemBelow(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem Tree_ItemLeft(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem Tree_ItemRight(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem Tree_ItemFL(TreeCtrl *tree, TreeItem item, int vertical, int first);
MODULE_SCOPE TreeItem Tree_ItemTop(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem Tree_ItemBottom(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem Tree_ItemLeftMost(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE TreeItem Tree_ItemRightMost(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE int Tree_ItemToRNC(TreeCtrl *tree, TreeItem item, int *row, int *col);
MODULE_SCOPE TreeItem Tree_RNCToItem(TreeCtrl *tree, int row, int col);
MODULE_SCOPE int Tree_AreaBbox(TreeCtrl *tree, int area, TreeRectangle *tr);

enum {
TREE_AREA_NONE = 0,
TREE_AREA_HEADER,
TREE_AREA_CONTENT,
TREE_AREA_LEFT,
TREE_AREA_RIGHT
};
MODULE_SCOPE int Tree_HitTest(TreeCtrl *tree, int x, int y);

MODULE_SCOPE void TreeDInfo_Init(TreeCtrl *tree);
MODULE_SCOPE void TreeDInfo_Free(TreeCtrl *tree);
MODULE_SCOPE void Tree_EventuallyRedraw(TreeCtrl *tree);
MODULE_SCOPE void Tree_GetScrollFractionsX(TreeCtrl *tree, double fractions[2]);
MODULE_SCOPE void Tree_GetScrollFractionsY(TreeCtrl *tree, double fractions[2]);
MODULE_SCOPE int Tree_FakeCanvasWidth(TreeCtrl *tree);
MODULE_SCOPE int Tree_FakeCanvasHeight(TreeCtrl *tree);
MODULE_SCOPE void Tree_SetScrollSmoothingX(TreeCtrl *tree, int smooth);
MODULE_SCOPE void Tree_SetScrollSmoothingY(TreeCtrl *tree, int smooth);
MODULE_SCOPE int Increment_FindX(TreeCtrl *tree, int offset);
MODULE_SCOPE int Increment_FindY(TreeCtrl *tree, int offset);
MODULE_SCOPE int Increment_ToOffsetX(TreeCtrl *tree, int index);
MODULE_SCOPE int Increment_ToOffsetY(TreeCtrl *tree, int index);
MODULE_SCOPE int TreeXviewCmd(TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[]);
MODULE_SCOPE int TreeYviewCmd(TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[]);
MODULE_SCOPE void Tree_SetOriginX(TreeCtrl *tree, int xOrigin);
MODULE_SCOPE void Tree_SetOriginY(TreeCtrl *tree, int yOrigin);
MODULE_SCOPE int Tree_GetOriginX(TreeCtrl *tree);
MODULE_SCOPE int Tree_GetOriginY(TreeCtrl *tree);
MODULE_SCOPE void Tree_RelayoutWindow(TreeCtrl *tree);
MODULE_SCOPE void Tree_FreeItemDInfo(TreeCtrl *tree, TreeItem item1, TreeItem item2);
MODULE_SCOPE void Tree_InvalidateItemDInfo(TreeCtrl *tree, TreeColumn column, TreeItem item1, TreeItem item2);
MODULE_SCOPE void TreeDisplay_ItemDeleted(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void TreeDisplay_ColumnDeleted(TreeCtrl *tree, TreeColumn column);
MODULE_SCOPE void TreeDisplay_FreeColumnDInfo(TreeCtrl *tree, TreeColumn column);
MODULE_SCOPE int Tree_ShouldDisplayLockedColumns(TreeCtrl *tree);
MODULE_SCOPE void TreeDisplay_GetReadyForTrouble(TreeCtrl *tree, int *requestsPtr);
MODULE_SCOPE int TreeDisplay_WasThereTrouble(TreeCtrl *tree, int requests);
MODULE_SCOPE void Tree_InvalidateArea(TreeCtrl *tree, int x1, int y1, int x2, int y2);
MODULE_SCOPE void Tree_InvalidateItemArea(TreeCtrl *tree, int x1, int y1, int x2, int y2);
MODULE_SCOPE void Tree_InvalidateItemOnScrollX(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void Tree_InvalidateItemOnScrollY(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void Tree_InvalidateRegion(TreeCtrl *tree, TkRegion region);
MODULE_SCOPE void Tree_RedrawArea(TreeCtrl *tree, int x1, int y1, int x2, int y2);
MODULE_SCOPE void Tree_ExposeArea(TreeCtrl *tree, int x1, int y1, int x2, int y2);
MODULE_SCOPE void Tree_FocusChanged(TreeCtrl *tree, int gotFocus);
MODULE_SCOPE void Tree_Activate(TreeCtrl *tree, int isActive);
MODULE_SCOPE void Tree_ItemsInArea(TreeCtrl *tree, TreeItemList *items, int minX, int minY, int maxX, int maxY);
MODULE_SCOPE void TreeColumnProxy_Undisplay(TreeCtrl *tree);
MODULE_SCOPE void TreeColumnProxy_Display(TreeCtrl *tree);
MODULE_SCOPE void TreeRowProxy_Undisplay(TreeCtrl *tree);
MODULE_SCOPE void TreeRowProxy_Display(TreeCtrl *tree);
MODULE_SCOPE int Tree_DrawTiledImage(TreeCtrl *tree, TreeDrawable td,
    Tk_Image image, TreeRectangle tr, int xOffset, int yOffset,
    int tileX, int tileY);
MODULE_SCOPE int Tree_IsBgImageOpaque(TreeCtrl *tree);
MODULE_SCOPE int Tree_DrawBgImage(TreeCtrl *tree, TreeDrawable td,
    TreeRectangle tr, int xOrigin, int yOrigin);

MODULE_SCOPE int TreeRect_Intersect(TreeRectangle *resultPtr,
    CONST TreeRectangle *r1, CONST TreeRectangle *r2);

#define DINFO_OUT_OF_DATE 0x0001
#define DINFO_CHECK_COLUMN_WIDTH 0x0002
#define DINFO_DRAW_HEADER 0x0004
#define DINFO_SET_ORIGIN_X 0x0008
#define DINFO_UPDATE_SCROLLBAR_X 0x0010
#define DINFO_REDRAW_PENDING 0x00020
#define DINFO_INVALIDATE 0x0040
#define DINFO_DRAW_HIGHLIGHT 0x0080
#define DINFO_DRAW_BORDER 0x0100
#define DINFO_REDO_RANGES 0x0200
#define DINFO_SET_ORIGIN_Y 0x0400
#define DINFO_UPDATE_SCROLLBAR_Y 0x0800
#define DINFO_REDO_INCREMENTS 0x1000
#define DINFO_REDO_COLUMN_WIDTH 0x2000
#define DINFO_REDO_SELECTION 0x4000
#define DINFO_DRAW_WHITESPACE 0x8000
MODULE_SCOPE void Tree_DInfoChanged(TreeCtrl *tree, int flags);

MODULE_SCOPE void Tree_TheWorldHasChanged(Tcl_Interp *interp);
MODULE_SCOPE int Tree_DumpDInfo(TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[]);

/* tkTreeTheme.c */
MODULE_SCOPE int TreeTheme_InitInterp(Tcl_Interp *interp);
MODULE_SCOPE void TreeTheme_ThemeChanged(TreeCtrl *tree);
MODULE_SCOPE int TreeTheme_Init(TreeCtrl *tree);
MODULE_SCOPE int TreeTheme_Free(TreeCtrl *tree);
MODULE_SCOPE int TreeTheme_DrawHeaderItem(TreeCtrl *tree, TreeDrawable td,
    int state, int arrow, int visIndex, int x, int y, int width, int height);
MODULE_SCOPE int TreeTheme_GetHeaderFixedHeight(TreeCtrl *tree, int *heightPtr);
MODULE_SCOPE int TreeTheme_GetHeaderContentMargins(TreeCtrl *tree, int state, int arrow, int bounds[4]);
MODULE_SCOPE int TreeTheme_DrawHeaderArrow(TreeCtrl *tree, TreeDrawable td, int state, int up, int x, int y, int width, int height);
MODULE_SCOPE int TreeTheme_DrawButton(TreeCtrl *tree, TreeDrawable td, TreeItem item, int state, int x, int y, int width, int height);
MODULE_SCOPE int TreeTheme_GetButtonSize(TreeCtrl *tree, Drawable drawable, int open, int *widthPtr, int *heightPtr);
MODULE_SCOPE int TreeTheme_GetArrowSize(TreeCtrl *tree, Drawable drawable, int up, int *widthPtr, int *heightPtr);
MODULE_SCOPE int TreeTheme_SetBorders(TreeCtrl *tree);
MODULE_SCOPE int TreeTheme_DrawBorders(TreeCtrl *tree, Drawable drawable);
MODULE_SCOPE int TreeTheme_GetColumnTextColor(TreeCtrl *tree, int state, XColor **xColorPtr);
MODULE_SCOPE int TreeTheme_AnimateButtonStart(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE int TreeTheme_ItemDeleted(TreeCtrl *tree, TreeItem item);
MODULE_SCOPE void TreeTheme_Relayout(TreeCtrl *tree);
MODULE_SCOPE int TreeTheme_IsDesktopComposited(TreeCtrl *tree);
MODULE_SCOPE int TreeThemeCmd(TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[]);
MODULE_SCOPE void TreeTheme_SetOptionDefault(Tk_OptionSpec *specPtr);

/* tkTreeUtils.c */
#ifdef TREECTRL_DEBUG
#define WIPE(p,s) memset((char *) p, 0xAA, s)
#else
#define WIPE(p,s)
#endif
#define CWIPE(p,t,c) WIPE(p, sizeof(t) * (c))
#define WIPEFREE(p,s) { WIPE(p, s); ckfree((char *) p); }
#define WFREE(p,t) WIPEFREE(p, sizeof(t))
#define WCFREE(p,t,c) WIPEFREE(p, sizeof(t) * (c))

MODULE_SCOPE int Tree_Ellipsis(Tk_Font tkfont, char *string, int numBytes, int *maxPixels, char *ellipsis, int force);
MODULE_SCOPE void Tree_HDotLine(TreeCtrl *tree, Drawable drawable, int x1, int y1, int x2);
MODULE_SCOPE void Tree_VDotLine(TreeCtrl *tree, Drawable drawable, int x1, int y1, int y2);
MODULE_SCOPE void Tree_DrawActiveOutline(TreeCtrl *tree, Drawable drawable, int x, int y, int width, int height, int open);
typedef struct DotState
{
    void *stuff[10];
} DotState;
MODULE_SCOPE void TreeDotRect_Setup(TreeCtrl *tree, Drawable drawable, DotState *dotState);
MODULE_SCOPE void TreeDotRect_Draw(DotState *dotState, int x, int y, int width, int height);
MODULE_SCOPE void TreeDotRect_Restore(DotState *dotState);
typedef struct TextLayout_ *TextLayout;
MODULE_SCOPE TextLayout TextLayout_Compute(Tk_Font tkfont, CONST char *string,
	int numChars, int wrapLength, Tk_Justify justify, int maxLines,
	int lMargin1, int lMargin2, int flags);
MODULE_SCOPE void TextLayout_Free(TextLayout textLayout);
MODULE_SCOPE void TextLayout_Size(TextLayout textLayout, int *widthPtr, int *heightPtr);
MODULE_SCOPE int TextLayout_TotalWidth(TextLayout textLayout);
MODULE_SCOPE void TextLayout_Draw(Display *display, Drawable drawable, GC gc,
	TextLayout layout, int x, int y, int firstChar, int lastChar,
	int underline);
MODULE_SCOPE void Tree_RedrawImage(Tk_Image image, int imageX, int imageY,
	int width, int height, TreeDrawable td, int drawableX, int drawableY);
MODULE_SCOPE void Tree_DrawBitmapWithGC(TreeCtrl *tree, Pixmap bitmap, Drawable drawable,
	GC gc, int src_x, int src_y, int width, int height, int dest_x, int dest_y);
MODULE_SCOPE void Tree_DrawBitmap(TreeCtrl *tree, Pixmap bitmap, Drawable drawable,
	XColor *fg, XColor *bg,
	int src_x, int src_y, int width, int height, int dest_x, int dest_y);
MODULE_SCOPE TkRegion Tree_GetRegion(TreeCtrl *tree);
MODULE_SCOPE void Tree_FreeRegion(TreeCtrl *tree, TkRegion region);
MODULE_SCOPE void Tree_FillRegion(Display *display, Drawable drawable, GC gc, TkRegion rgn);
MODULE_SCOPE void Tree_OffsetRegion(TkRegion region, int xOffset, int yOffset);
MODULE_SCOPE void Tree_SetEmptyRegion(TkRegion region);
MODULE_SCOPE TkRegion Tree_GetRectRegion(TreeCtrl *tree, const TreeRectangle *rect);
MODULE_SCOPE void Tree_SetRectRegion(TkRegion region, const TreeRectangle *rect);
MODULE_SCOPE void Tree_GetRegionBounds(TkRegion region, TreeRectangle *rect);
MODULE_SCOPE void Tree_UnionRegion(TkRegion rgnA, TkRegion rgnB, TkRegion rgnOut);
MODULE_SCOPE int Tree_ScrollWindow(TreeCtrl *tree, GC gc, int x, int y,
	int width, int height, int dx, int dy, TkRegion damageRgn);
MODULE_SCOPE void Tree_UnsetClipMask(TreeCtrl *tree, Drawable drawable, GC gc);
MODULE_SCOPE void Tree_XImage2Photo(Tcl_Interp *interp, Tk_PhotoHandle photoH,
    XImage *ximage, unsigned long trans, int alpha);

#define PAD_TOP_LEFT     0
#define PAD_BOTTOM_RIGHT 1
MODULE_SCOPE Tk_ObjCustomOption TreeCtrlCO_pad;

MODULE_SCOPE int TreeCtrl_GetPadAmountFromObj(Tcl_Interp *interp,
	Tk_Window tkwin, Tcl_Obj *padObj,
	int *topLeftPtr, int *bottomRightPtr);
MODULE_SCOPE Tcl_Obj *TreeCtrl_NewPadAmountObj(int *padAmounts);

/*****/

#define ObjectIsEmpty TreeCtrl_ObjectIsEmpty
MODULE_SCOPE int ObjectIsEmpty(Tcl_Obj *obj);

#define pstBitmap TreeCtrl_pstBitmap
#define pstBoolean TreeCtrl_pstBoolean
#define pstBorder TreeCtrl_pstBorder
#define pstColor TreeCtrl_pstColor
#define pstFlags TreeCtrl_pstFlags
#define pstFont TreeCtrl_pstFont
#define pstImage TreeCtrl_pstImage
#define pstRelief TreeCtrl_pstRelief
MODULE_SCOPE PerStateType pstBitmap;
MODULE_SCOPE PerStateType pstBoolean;
MODULE_SCOPE PerStateType pstBorder;
MODULE_SCOPE PerStateType pstColor;
MODULE_SCOPE PerStateType pstFlags;
MODULE_SCOPE PerStateType pstFont;
MODULE_SCOPE PerStateType pstImage;
MODULE_SCOPE PerStateType pstRelief;

#define MATCH_NONE	0
#define MATCH_ANY	1
#define MATCH_PARTIAL	2
#define MATCH_EXACT	3

MODULE_SCOPE void PerStateInfo_Free(TreeCtrl *tree, PerStateType *typePtr,
    PerStateInfo *pInfo);
typedef int (*StateFromObjProc)(TreeCtrl *tree, Tcl_Obj *obj, int *stateOff, int *stateOn);
MODULE_SCOPE int PerStateInfo_FromObj(TreeCtrl *tree, StateFromObjProc proc,
    PerStateType *typePtr, PerStateInfo *pInfo);
MODULE_SCOPE PerStateData *PerStateInfo_ForState(TreeCtrl *tree,
    PerStateType *typePtr, PerStateInfo *pInfo, int state, int *match);
MODULE_SCOPE Tcl_Obj *PerStateInfo_ObjForState(TreeCtrl *tree, PerStateType *typePtr,
    PerStateInfo *pInfo, int state, int *match);
MODULE_SCOPE int PerStateInfo_Undefine(TreeCtrl *tree, PerStateType *typePtr,
    PerStateInfo *pInfo, int state);

MODULE_SCOPE GC Tree_GetGC(TreeCtrl *tree, unsigned long mask, XGCValues *gcValues);
MODULE_SCOPE void Tree_FreeAllGC(TreeCtrl *tree);

MODULE_SCOPE Pixmap PerStateBitmap_ForState(TreeCtrl *tree, PerStateInfo *pInfo,
    int state, int *match);
MODULE_SCOPE void PerStateBitmap_MaxSize(TreeCtrl *tree, PerStateInfo *pInfo,
    int *widthPtr, int *heightPtr);
MODULE_SCOPE int PerStateBoolean_ForState(TreeCtrl *tree, PerStateInfo *pInfo,
    int state, int *match);
MODULE_SCOPE Tk_3DBorder PerStateBorder_ForState(TreeCtrl *tree, PerStateInfo *pInfo,
    int state, int *match);
MODULE_SCOPE TreeColor *PerStateColor_ForState(TreeCtrl *tree, PerStateInfo *pInfo,
    int state, int *match);
MODULE_SCOPE int PerStateFlags_ForState(TreeCtrl *tree, PerStateInfo *pInfo,
    int state, int *match);
MODULE_SCOPE Tk_Font PerStateFont_ForState(TreeCtrl *tree, PerStateInfo *pInfo,
    int state, int *match);
MODULE_SCOPE Tk_Image PerStateImage_ForState(TreeCtrl *tree, PerStateInfo *pInfo,
    int state, int *match);
MODULE_SCOPE void PerStateImage_MaxSize(TreeCtrl *tree, PerStateInfo *pInfo,
    int *widthPtr, int *heightPtr);
MODULE_SCOPE int PerStateRelief_ForState(TreeCtrl *tree, PerStateInfo *pInfo,
    int state, int *match);

MODULE_SCOPE void PSTSave(PerStateInfo *pInfo, PerStateInfo *pSave);
MODULE_SCOPE void PSTRestore(TreeCtrl *tree, PerStateType *typePtr,
    PerStateInfo *pInfo, PerStateInfo *pSave);

typedef struct CharFlag {
    char flagChar;
    int flagBit;
} CharFlag;

MODULE_SCOPE int Tree_GetFlagsFromString(TreeCtrl *tree, const char *string,
    int length, const char *typeStr, const CharFlag flags[], int *flagsPtr);
MODULE_SCOPE int Tree_GetFlagsFromObj(TreeCtrl *tree, Tcl_Obj *obj,
    const char *typeStr, const CharFlag flags[], int *flagsPtr);
    
#ifdef ALLOC_HAX
MODULE_SCOPE ClientData TreeAlloc_Init(void);
MODULE_SCOPE void TreeAlloc_Finalize(ClientData data);
MODULE_SCOPE char *TreeAlloc_Alloc(ClientData data, Tk_Uid id, int size);
MODULE_SCOPE char *TreeAlloc_CAlloc(ClientData data, Tk_Uid id, int size, int count, int roundUp);
MODULE_SCOPE char *TreeAlloc_Realloc(ClientData data, Tk_Uid id, char *ptr, int size1, int size2);
MODULE_SCOPE void TreeAlloc_Free(ClientData data, Tk_Uid id, char *ptr, int size);
MODULE_SCOPE void TreeAlloc_CFree(ClientData data, Tk_Uid id, char *ptr, int size, int count, int roundUp);
MODULE_SCOPE void TreeAlloc_Stats(Tcl_Interp *interp, ClientData data);
#endif

/*****/

MODULE_SCOPE void TreePtrList_Init(TreeCtrl *tree, TreePtrList *tilPtr, int count);
MODULE_SCOPE void TreePtrList_Grow(TreePtrList *tilPtr, int count);
MODULE_SCOPE ClientData *TreePtrList_Append(TreePtrList *tilPtr, ClientData ptr);
MODULE_SCOPE ClientData *TreePtrList_Concat(TreePtrList *tilPtr, TreePtrList *til2Ptr);
MODULE_SCOPE void TreePtrList_Free(TreePtrList *tilPtr);

#define TreeItemList_Init TreePtrList_Init
#define TreeItemList_Append TreePtrList_Append
#define TreeItemList_Concat TreePtrList_Concat
#define TreeItemList_Free TreePtrList_Free
#define TreeItemList_Items(L) ((TreeItem *) (L)->pointers)
#define TreeItemList_Nth(L,n) ((TreeItem) (L)->pointers[n])
#define TreeItemList_Count(L) ((L)->count)
MODULE_SCOPE void TreeItemList_Sort(TreeItemList *items);

#define TreeColumnList_Init TreePtrList_Init
#define TreeColumnList_Append TreePtrList_Append
#define TreeColumnList_Concat TreePtrList_Concat
#define TreeColumnList_Free TreePtrList_Free
#define TreeColumnList_Nth(L,n) ((TreeColumn) (L)->pointers[n])
#define TreeColumnList_Count(L) ((L)->count)

/*****/

/*
 * This structure holds a list of tags.
 */
typedef struct TagInfo TagInfo;
struct TagInfo {
    int numTags;		/* Number of tag slots actually used
				    * at tagPtr. */
    int tagSpace;		/* Total amount of tag space available
				    * at tagPtr. */
#define TREE_TAG_SPACE 3
    Tk_Uid tagPtr[TREE_TAG_SPACE]; /* Array of tags. The actual size will
				    * be tagSpace. THIS FIELD MUST BE THE
				    * LAST IN THE STRUCTURE. */
};

MODULE_SCOPE TagInfo *TagInfo_Add(TreeCtrl *tree, TagInfo *tagInfo, Tk_Uid tags[], int numTags);
MODULE_SCOPE TagInfo *TagInfo_Remove(TreeCtrl *tree, TagInfo *tagInfo, Tk_Uid tags[], int numTags);
MODULE_SCOPE Tk_Uid *TagInfo_Names(TreeCtrl *tree, TagInfo *tagInfo, Tk_Uid *tags, int *numTagsPtr, int *tagSpacePtr);
MODULE_SCOPE TagInfo *TagInfo_Copy(TreeCtrl *tree, TagInfo *tagInfo);
MODULE_SCOPE void TagInfo_Free(TreeCtrl *tree, TagInfo *tagInfo);
MODULE_SCOPE int TagInfo_FromObj(TreeCtrl *tree, Tcl_Obj *objPtr, TagInfo **tagInfoPtr);
MODULE_SCOPE Tk_ObjCustomOption TreeCtrlCO_tagInfo;

/*
 * This struct holds information about a tag expression.
 */
typedef struct TagExpr {
    TreeCtrl *tree;

    Tk_Uid *uids;		/* expresion compiled to an array of uids */
    Tk_Uid staticUids[15];
    int allocated;		/* available space for array of uids */
    int length;			/* number of uids */
    int index;			/* current position in expression evaluation */

    int simple;			/* TRUE if expr is single tag */
    Tk_Uid uid;			/* single tag if 'simple' is TRUE */

    char *string;		/* tag expression string */
    int stringIndex;		/* current position in string scan */
    int stringLength;		/* length of tag expression string */

    char *rewritebuffer;	/* tag string (after removing escapes) */
    char staticRWB[100];
} TagExpr;

MODULE_SCOPE int TagExpr_Init(TreeCtrl *tree, Tcl_Obj *exprObj, TagExpr *expr);
MODULE_SCOPE int TagExpr_Scan(TagExpr *expr);
MODULE_SCOPE int TagExpr_Eval(TagExpr *expr, TagInfo *tags);
MODULE_SCOPE void TagExpr_Free(TagExpr *expr);

MODULE_SCOPE Tk_OptionSpec *Tree_FindOptionSpec(Tk_OptionSpec *optionTable, CONST char *optionName);
    
MODULE_SCOPE Tk_ObjCustomOption *PerStateCO_Alloc(CONST char *optionName,
    PerStateType *typePtr, StateFromObjProc proc);
MODULE_SCOPE int PerStateCO_Init(Tk_OptionSpec *optionTable, CONST char *optionName,
    PerStateType *typePtr, StateFromObjProc proc);

/*****/

typedef struct DynamicOptionSpec DynamicOptionSpec;
typedef struct DynamicOption DynamicOption;

struct DynamicOption
{
    int id;			/* Unique id. */
    DynamicOption *next;	/* Linked list. */
    char data[1];		/* Actual size will be > 1 */
};

typedef void (DynamicOptionInitProc)(void *data);

MODULE_SCOPE DynamicOption *DynamicOption_AllocIfNeeded(TreeCtrl *tree,
    DynamicOption **firstPtr, int id, int size, DynamicOptionInitProc *init);
MODULE_SCOPE void *DynamicOption_FindData(DynamicOption *first, int id);
MODULE_SCOPE void DynamicOption_Free(TreeCtrl *tree, DynamicOption *first,
    Tk_OptionSpec *optionTable);
MODULE_SCOPE void DynamicOption_Free1(TreeCtrl *tree, DynamicOption **firstPtr,
    int id, int size);
MODULE_SCOPE int DynamicCO_Init(Tk_OptionSpec *optionTable, CONST char *optionName,
    int id, int size, int objOffset, int internalOffset,
    Tk_ObjCustomOption *custom, DynamicOptionInitProc *init);

MODULE_SCOPE int BooleanFlagCO_Init(Tk_OptionSpec *optionTable, CONST char *optionName,
    int theFlag);
MODULE_SCOPE int ItemButtonCO_Init(Tk_OptionSpec *optionTable, CONST char *optionName,
    int flag1, int flag2);

MODULE_SCOPE int Tree_GetIntForIndex(TreeCtrl *tree, Tcl_Obj *objPtr, int *indexPtr,
    int *endRelativePtr);

MODULE_SCOPE Tk_ObjCustomOption TreeCtrlCO_pixels;
MODULE_SCOPE Tk_ObjCustomOption TreeCtrlCO_string;
MODULE_SCOPE Tk_ObjCustomOption TreeCtrlCO_style;
MODULE_SCOPE Tk_ObjCustomOption TreeCtrlCO_treecolor;

/*****/

#define STATIC_SIZE 20
#define STATIC_ALLOC(P,T,C) \
    if (C > STATIC_SIZE) \
	P = (T *) ckalloc(sizeof(T) * (C))
#define STATIC_FREE(P,T,C) \
    CWIPE(P, T, C); \
    if (C > STATIC_SIZE) \
	ckfree((char *) P)
#define STATIC_FREE2(P,P2) \
    if (P != P2) \
	ckfree((char *) P)

MODULE_SCOPE TreeColor *Tree_AllocColorFromObj(TreeCtrl *tree, Tcl_Obj *obj);
MODULE_SCOPE void Tree_FreeColor(TreeCtrl *tree, TreeColor *tc);
MODULE_SCOPE Tcl_Obj *TreeColor_ToObj(TreeCtrl *tree, TreeColor *tc);
MODULE_SCOPE int TreeColor_IsOpaque(TreeCtrl *tree, TreeColor *tc);

#define pstGradient TreeCtrl_pstGradient
MODULE_SCOPE PerStateType pstGradient;
MODULE_SCOPE TreeGradient PerStateGradient_ForState(TreeCtrl *tree, PerStateInfo *pInfo,
    int state, int *match);

/*****/

typedef struct GradientCoord GradientCoord;

/*
 * Records for gradient fills.
 * We need a separate GradientStopArray to simplify option parsing.
 */
 
typedef struct GradientStop {
    double offset;
    XColor *color;
    double opacity;
} GradientStop;

typedef struct GradientStopArray {
    int nstops;
    GradientStop **stops;	/* Array of pointers to GradientStop. */
} GradientStopArray;

typedef struct TreeGradient_
{
    int refCount;		/* Number of users of gradient. */
    int deletePending;		/* Non-zero if gradient should be deleted when
				 * last reference goes away. */
    Tk_Uid name;
    Tcl_Obj *stopsObj;		/* -stops */
    GradientStopArray *stopArrPtr; /* -stops */
    int vertical;		/* -orient */

    int steps;			/* -steps */
    int nStepColors;		/* length of stepColors */
    XColor **stepColors;	/* calculated from color1,color2,steps */

    GradientCoord *left, *right, *top, *bottom;
    Tcl_Obj *leftObj, *rightObj, *topObj, *bottomObj;
} TreeGradient_;

MODULE_SCOPE void TreeGradient_Init(TreeCtrl *tree);
MODULE_SCOPE void TreeGradient_Free(TreeCtrl *tree);
MODULE_SCOPE int TreeGradientCmd(ClientData clientData, Tcl_Interp *interp,
    int objc, Tcl_Obj *CONST objv[]);
MODULE_SCOPE int TreeGradient_FromObj(TreeCtrl *tree, Tcl_Obj *objPtr,
    TreeGradient *gradientPtr);
MODULE_SCOPE void TreeGradient_Release(TreeCtrl *tree, TreeGradient gradient);
MODULE_SCOPE int TreeGradient_IsOpaque(TreeCtrl *tree, TreeGradient gradient);
MODULE_SCOPE int Tree_HasNativeGradients(TreeCtrl *tree);

MODULE_SCOPE int TreeGradient_GetBrushBounds(TreeCtrl *tree,
    TreeGradient gradient, const TreeRectangle *trPaint,
    TreeRectangle *trBrush, TreeColumn column, TreeItem item);
MODULE_SCOPE void TreeGradient_IsRelativeToCanvas(TreeGradient gradient,
    int *relX, int *relY);
MODULE_SCOPE void TreeColor_GetBrushBounds(TreeCtrl *tree, TreeColor *tc,
    TreeRectangle trPaint, int xOrigin, int yOrigin, TreeColumn column,
    TreeItem item, TreeRectangle *trBrush);

MODULE_SCOPE void TreeGradient_ColumnDeleted(TreeCtrl *tree,
    TreeColumn column);
MODULE_SCOPE void TreeGradient_ItemDeleted(TreeCtrl *tree, TreeItem item);

/*****/

typedef enum  {
    TREE_CLIP_REGION,
    TREE_CLIP_RECT,
    TREE_CLIP_AREA
} TreeClipType;

typedef struct {
    TreeClipType type;
    TkRegion region;
    TreeRectangle tr;
    int area;
} TreeClip;

#define RECT_OPEN_W 0x01
#define RECT_OPEN_N 0x02
#define RECT_OPEN_E 0x04
#define RECT_OPEN_S 0x08
#define RECT_OPEN_WNES (RECT_OPEN_W|RECT_OPEN_N|RECT_OPEN_E|RECT_OPEN_S)

MODULE_SCOPE void Tree_FillRectangle(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, GC gc, TreeRectangle tr);


#if USE_ITEM_PIXMAP == 0
MODULE_SCOPE void Tree_DrawArc(TreeCtrl *tree, TreeDrawable td, TreeClip *clip,
    GC gc, int x, int y, unsigned int width, unsigned int height,
    int start, int extent);
MODULE_SCOPE void Tree_FillArc(TreeCtrl *tree, TreeDrawable td, TreeClip *clip,
    GC gc, int x, int y, unsigned int width, unsigned int height,
    int start, int extent);

MODULE_SCOPE void Tree_Draw3DRectangle(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, Tk_3DBorder border, int x, int y, int width, int height,
    int borderWidth, int relief);
MODULE_SCOPE void Tree_Fill3DRectangle(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, Tk_3DBorder border, int x, int y, int width, int height,
    int borderWidth, int relief);
#else /* USE_ITEM_PIXMAP == 1 */
#define Tree_DrawArc(tree,td,clip,gc,x,y,width,height,start,extent) \
    XDrawArc(tree->display,td.drawable,gc,x,y,width,height,start,extent);
#define Tree_FillArc(tree,td,clip,gc,x,y,width,height,start,extent) \
    XFillArc(tree->display,td.drawable,gc,x,y,width,height,start,extent);
#endif /* USE_ITEM_PIXMAP == 1 */

MODULE_SCOPE void Tree_DrawRoundRectX11(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, GC gc, TreeRectangle tr, int outlineWidth,
    int rx, int ry, int open);
MODULE_SCOPE void Tree_FillRoundRectX11(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, GC gc, TreeRectangle tr, int rx, int ry, int open);
MODULE_SCOPE void TreeGradient_DrawRectX11(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, TreeGradient gradient, TreeRectangle trBrush, TreeRectangle tr,
    int outlineWidth, int open);
MODULE_SCOPE void TreeGradient_FillRectX11(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, TreeGradient gradient, TreeRectangle trBrush,
    TreeRectangle tr);
MODULE_SCOPE void TreeGradient_FillRoundRectX11(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, TreeGradient gradient, TreeRectangle trBrush,
    TreeRectangle tr, int rx, int ry, int open);

MODULE_SCOPE void Tree_DrawRoundRect(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, XColor *xcolor, TreeRectangle tr, int outlineWidth,
    int rx, int ry, int open);
MODULE_SCOPE void Tree_FillRoundRect(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, XColor *xcolor, TreeRectangle tr, int rx, int ry, int open);

MODULE_SCOPE void TreeGradient_DrawRect(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, TreeGradient gradient, TreeRectangle trBrush,
    TreeRectangle tr, int outlineWidth, int open);
MODULE_SCOPE void TreeGradient_DrawRoundRect(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, TreeGradient gradient, TreeRectangle trBrush,
    TreeRectangle tr, int outlineWidth, int rx, int ry, int open);
MODULE_SCOPE void TreeGradient_FillRect(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, TreeGradient gradient, TreeRectangle trBrush,
    TreeRectangle tr);
MODULE_SCOPE void TreeGradient_FillRoundRect(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, TreeGradient gradient, TreeRectangle trBrush,
    TreeRectangle tr, int rx, int ry, int open);

MODULE_SCOPE void TreeColor_DrawRect(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, TreeColor *tc, TreeRectangle trBrush, TreeRectangle tr,
    int outlineWidth, int open);
MODULE_SCOPE void TreeColor_FillRect(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, TreeColor *tc, TreeRectangle trBrush, TreeRectangle tr);
MODULE_SCOPE void TreeColor_DrawRoundRect(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, TreeColor *tc, TreeRectangle trBrush, TreeRectangle tr,
    int outlineWidth, int rx, int ry, int open);
MODULE_SCOPE void TreeColor_FillRoundRect(TreeCtrl *tree, TreeDrawable td,
    TreeClip *clip, TreeColor *tc, TreeRectangle trBrush, TreeRectangle tr,
    int rx, int ry, int open);

MODULE_SCOPE int TreeDraw_InitInterp(Tcl_Interp *interp);
