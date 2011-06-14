/* 
 * tkTreeColumn.c --
 *
 *	This module implements treectrl widget's columns.
 *
 * Copyright (c) 2002-2010 Tim Baker
 * Copyright (c) 2002-2003 Christian Krone
 * Copyright (c) 2003 ActiveState Corporation
 *
 * RCS: @(#) $Id$
 */

#include "tkTreeCtrl.h"

typedef struct TreeColumn_ TreeColumn_;

#ifdef UNIFORM_GROUP
typedef struct UniformGroup {
    Tcl_HashEntry *hPtr;	/* Entry in TreeCtrl.uniformGroupHash */
    int refCount;		/* Number of columns in this group. */
    int minSize;		/* Used during layout. */
} UniformGroup;
#endif

/*
 * The following structure holds information about a single
 * column in a TreeCtrl.
 */
struct TreeColumn_
{
    Tcl_Obj *textObj;		/* -text */
    char *text;			/* -text */
    int width;			/* -width */
    Tcl_Obj *widthObj;		/* -width */
    int minWidth;		/* -minwidth */
    Tcl_Obj *minWidthObj;	/* -minwidth */
    int maxWidth;		/* -maxwidth */
    Tcl_Obj *maxWidthObj;	/* -maxwidth */
#ifdef DEPRECATED
    int stepWidth;		/* -stepwidth */
    Tcl_Obj *stepWidthObj;	/* -stepwidth */
    int widthHack;		/* -widthhack */
#endif /* DEPRECATED */
    Tk_Font tkfont;		/* -font */
    Tk_Justify justify;		/* -justify */
    int itemJustify;		/* -itemjustify */
    PerStateInfo border;	/* -background */
    Tcl_Obj *borderWidthObj;	/* -borderwidth */
    int borderWidth;		/* -borderwidth */
    PerStateInfo textColor;	/* -textcolor */
    int expand;			/* -expand */
    int squeeze;		/* -squeeze */
    int visible;		/* -visible */
    int resize;			/* -resize */
    TagInfo *tagInfo;		/* -tags */
    char *imageString;		/* -image */
    PerStateInfo arrowBitmap;	/* -arrowbitmap */
    PerStateInfo arrowImage;	/* -arrowimage */
    Pixmap bitmap;		/* -bitmap */
    Tcl_Obj *itemBgObj;		/* -itembackground */
    TreeStyle itemStyle;	/* -itemstyle */
    int button;			/* -button */
    Tcl_Obj *textPadXObj;	/* -textpadx */
    int *textPadX;		/* -textpadx */
    Tcl_Obj *textPadYObj;	/* -textpady */
    int *textPadY;		/* -textpady */
    Tcl_Obj *imagePadXObj;	/* -imagepadx */
    int *imagePadX;		/* -imagepadx */
    Tcl_Obj *imagePadYObj;	/* -imagepady */
    int *imagePadY;		/* -imagepady */
    Tcl_Obj *arrowPadXObj;	/* -arrowpadx */
    int *arrowPadX;		/* -arrowpadx */
    Tcl_Obj *arrowPadYObj;	/* -arrowpady */
    int *arrowPadY;		/* -arrowpady */

    int arrow;			/* -arrow */

#define SIDE_LEFT 0
#define SIDE_RIGHT 1
    int arrowSide;		/* -arrowside */
    int arrowGravity;		/* -arrowgravity */

    int state;			/* -state */

    int lock;			/* -lock */

    TreeCtrl *tree;
    Tk_OptionTable optionTable;
    int id;			/* unique column identifier */
    int index;			/* order in list of columns */
    int textLen;
    int textWidth;
    Tk_Image image;
    int neededWidth;		/* calculated from borders + image/bitmap +
				 * text + arrow */
    int neededHeight;		/* calculated from borders + image/bitmap +
				 * text */
    int offset;			/* Total width of preceding columns */
    int useWidth;		/* -width, -minwidth, or required+expansion */
    int widthOfItems;		/* width of all TreeItemColumns */
    int itemBgCount;		/* -itembackground colors */
    TreeColor **itemBgColor;	/* -itembackground colors */
    GC bitmapGC;
    TreeColumn prev;
    TreeColumn next;
    TextLayout textLayout;	/* multi-line titles */
    int textLayoutWidth;	/* width passed to TextLayout_Compute */
    int textLayoutInvalid;
#define TEXT_WRAP_NULL -1
#define TEXT_WRAP_CHAR 0
#define TEXT_WRAP_WORD 1
    int textWrap;		/* -textwrap */
    int textLines;		/* -textlines */
#ifdef UNIFORM_GROUP
    UniformGroup *uniform;	/* -uniform */
    int weight;			/* -weight */
#endif
    TreeColumnDInfo dInfo;	/* Display info. */
#if COLUMNGRID == 1
    Tcl_Obj *gridLeftColorObj;	/* -gridleftcolor */
    Tcl_Obj *gridRightColorObj;	/* -gridrightcolor */
    TreeColor *gridLeftColor;	/* -gridleftcolor */
    TreeColor *gridRightColor;	/* -gridrightcolor */
#endif
};

#ifdef UNIFORM_GROUP
/*
 *----------------------------------------------------------------------
 *
 * UniformGroupCO_Set --
 * UniformGroupCO_Get --
 * UniformGroupCO_Restore --
 * UniformGroupCO_Free --
 *
 *	These procedures implement a TK_OPTION_CUSTOM where the custom
 *	option is a UniformGroup.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
UniformGroupCO_Set(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **valuePtr,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags
    )
{
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    int objEmpty;
    UniformGroup **internalPtr, *new;

    if (internalOffset >= 0)
	internalPtr = (UniformGroup **) (recordPtr + internalOffset);
    else
	internalPtr = NULL;

    objEmpty = ObjectIsEmpty((*valuePtr));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty)
	(*valuePtr) = NULL;

    if (internalPtr != NULL) {
	if (*valuePtr != NULL) {
	    int isNew;
	    Tcl_HashEntry *hPtr = Tcl_CreateHashEntry(&tree->uniformGroupHash,
		    Tcl_GetString(*valuePtr), &isNew);
	    if (isNew) {
		new = (UniformGroup *) ckalloc(sizeof(UniformGroup));
		new->refCount = 0;
		new->hPtr = hPtr;
		Tcl_SetHashValue(hPtr, (ClientData) new);
	    } else {
		new = (UniformGroup *) Tcl_GetHashValue(hPtr);
	    }
	    new->refCount++;
#ifdef TREECTRL_DEBUG
	    if (tree->debug.enable)
		dbwin("UniformGroupCO_Set: %s refCount=%d\n", Tcl_GetString(*valuePtr), new->refCount);
#endif
	} else {
	    new = NULL;
	}
	*((UniformGroup **) saveInternalPtr) = *internalPtr;
	*internalPtr = new;
    }

    return TCL_OK;
}

static Tcl_Obj *
UniformGroupCO_Get(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,
    int internalOffset
    )
{
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    UniformGroup *uniform = *(UniformGroup **) (recordPtr + internalOffset);

    if (uniform == NULL)
	return NULL;
    return Tcl_NewStringObj(Tcl_GetHashKey(&tree->uniformGroupHash,
	    uniform->hPtr), -1);
}

static void
UniformGroupCO_Restore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,
    char *saveInternalPtr
    )
{
    *(UniformGroup **) internalPtr = *(UniformGroup **) saveInternalPtr;
}

static void
UniformGroupCO_Free(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr
    )
{
    UniformGroup *uniform = *(UniformGroup **) internalPtr;

#ifdef TREECTRL_DEBUG
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    if (tree->debug.enable && uniform != NULL) {
	dbwin("UniformGroupCO_Free: %s refCount=%d\n", Tcl_GetHashKey(&tree->uniformGroupHash, uniform->hPtr), uniform->refCount - 1);
    }
#endif
    if ((uniform != NULL) && (--uniform->refCount <= 0)) {
	Tcl_DeleteHashEntry(uniform->hPtr);
	ckfree((char *) uniform);
	*((UniformGroup **) internalPtr) = NULL;
    }
}

static Tk_ObjCustomOption uniformGroupCO =
{
    "uniform group",
    UniformGroupCO_Set,
    UniformGroupCO_Get,
    UniformGroupCO_Restore,
    UniformGroupCO_Free,
    (ClientData) NULL
};
#endif /* UNIFORM_GROUP */

static CONST char *arrowST[] = { "none", "up", "down", (char *) NULL };
static CONST char *arrowSideST[] = { "left", "right", (char *) NULL };
static CONST char *stateST[] = { "normal", "active", "pressed", (char *) NULL };
static CONST char *lockST[] = { "left", "none", "right", (char *) NULL };
static CONST char *justifyStrings[] = {
    "left", "right", "center", (char *) NULL
};

#define COLU_CONF_IMAGE		0x0001
#define COLU_CONF_NWIDTH	0x0002	/* neededWidth */
#define COLU_CONF_NHEIGHT	0x0004	/* neededHeight */
#define COLU_CONF_TWIDTH	0x0008	/* totalWidth */
#define COLU_CONF_ITEMBG	0x0010
#define COLU_CONF_DISPLAY	0x0040
#define COLU_CONF_JUSTIFY	0x0080
#define COLU_CONF_TAGS		0x0100
#define COLU_CONF_TEXT		0x0200
#define COLU_CONF_BITMAP	0x0400
#define COLU_CONF_RANGES	0x0800
#if COLUMNGRID == 1
#define COLU_CONF_GRIDLINES	0x1000
#endif

static Tk_OptionSpec columnSpecs[] = {
    {TK_OPTION_STRING_TABLE, "-arrow", (char *) NULL, (char *) NULL,
     "none", -1, Tk_Offset(TreeColumn_, arrow),
     0, (ClientData) arrowST, COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-arrowbitmap", (char *) NULL, (char *) NULL,
     (char *) NULL,
     Tk_Offset(TreeColumn_, arrowBitmap.obj), Tk_Offset(TreeColumn_, arrowBitmap),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_STRING_TABLE, "-arrowgravity", (char *) NULL, (char *) NULL,
     "left", -1, Tk_Offset(TreeColumn_, arrowGravity),
     0, (ClientData) arrowSideST, COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-arrowimage", (char *) NULL, (char *) NULL,
     (char *) NULL,
     Tk_Offset(TreeColumn_, arrowImage.obj), Tk_Offset(TreeColumn_, arrowImage),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-arrowpadx", (char *) NULL, (char *) NULL,
     "6", Tk_Offset(TreeColumn_, arrowPadXObj), Tk_Offset(TreeColumn_, arrowPadX),
     0, (ClientData) &TreeCtrlCO_pad, COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-arrowpady", (char *) NULL, (char *) NULL,
     "0", Tk_Offset(TreeColumn_, arrowPadYObj), Tk_Offset(TreeColumn_, arrowPadY),
     0, (ClientData) &TreeCtrlCO_pad, COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_STRING_TABLE, "-arrowside", (char *) NULL, (char *) NULL,
     "right", -1, Tk_Offset(TreeColumn_, arrowSide),
     0, (ClientData) arrowSideST, COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
     /* NOTE: -background is a per-state option, so DEF_BUTTON_BG_COLOR
      * must be a list of one element */
    {TK_OPTION_CUSTOM, "-background", (char *) NULL, (char *) NULL,
     (char *) NULL /* initialized later */,
     Tk_Offset(TreeColumn_, border.obj), Tk_Offset(TreeColumn_, border),
     0, (ClientData) NULL, COLU_CONF_DISPLAY},
    {TK_OPTION_BITMAP, "-bitmap", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeColumn_, bitmap),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_BITMAP | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_PIXELS, "-borderwidth", (char *) NULL, (char *) NULL,
     "2", Tk_Offset(TreeColumn_, borderWidthObj), Tk_Offset(TreeColumn_, borderWidth),
     0, (ClientData) NULL, COLU_CONF_TWIDTH | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_BOOLEAN, "-button", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeColumn_, button),
     0, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-expand", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeColumn_, expand),
     0, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_FONT, "-font", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeColumn_, tkfont),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_NWIDTH |
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY | COLU_CONF_TEXT},
#if COLUMNGRID==1
    {TK_OPTION_CUSTOM, "-gridleftcolor", (char *) NULL, (char *) NULL, (char *) NULL,
	Tk_Offset(TreeColumn_, gridLeftColorObj),
	Tk_Offset(TreeColumn_, gridLeftColor),
	TK_OPTION_NULL_OK, (ClientData) &TreeCtrlCO_treecolor,
	COLU_CONF_GRIDLINES},
    {TK_OPTION_CUSTOM, "-gridrightcolor", (char *) NULL, (char *) NULL, (char *) NULL,
	Tk_Offset(TreeColumn_, gridRightColorObj),
	Tk_Offset(TreeColumn_, gridRightColor),
	TK_OPTION_NULL_OK, (ClientData) &TreeCtrlCO_treecolor,
	COLU_CONF_GRIDLINES},
#endif
    {TK_OPTION_STRING, "-image", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeColumn_, imageString),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_IMAGE | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-imagepadx", (char *) NULL, (char *) NULL,
     "6", Tk_Offset(TreeColumn_, imagePadXObj),
     Tk_Offset(TreeColumn_, imagePadX), 0, (ClientData) &TreeCtrlCO_pad,
     COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-imagepady", (char *) NULL, (char *) NULL,
     "0", Tk_Offset(TreeColumn_, imagePadYObj),
     Tk_Offset(TreeColumn_, imagePadY), 0, (ClientData) &TreeCtrlCO_pad,
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_STRING, "-itembackground", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(TreeColumn_, itemBgObj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_ITEMBG},
    {TK_OPTION_CUSTOM, "-itemjustify", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeColumn_, itemJustify),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_JUSTIFY},
    {TK_OPTION_CUSTOM, "-itemstyle", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeColumn_, itemStyle),
     TK_OPTION_NULL_OK, (ClientData) &TreeCtrlCO_style, 0},
    {TK_OPTION_JUSTIFY, "-justify", (char *) NULL, (char *) NULL,
     "left", -1, Tk_Offset(TreeColumn_, justify),
     0, (ClientData) NULL, COLU_CONF_DISPLAY | COLU_CONF_JUSTIFY},
    {TK_OPTION_STRING_TABLE, "-lock", (char *) NULL, (char *) NULL,
     "none", -1, Tk_Offset(TreeColumn_, lock), 0, (ClientData) lockST, 0},
    {TK_OPTION_PIXELS, "-maxwidth", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(TreeColumn_, maxWidthObj),
     Tk_Offset(TreeColumn_, maxWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_PIXELS, "-minwidth", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(TreeColumn_, minWidthObj),
     Tk_Offset(TreeColumn_, minWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_BOOLEAN, "-resize", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeColumn_, resize), 0, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-squeeze", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeColumn_, squeeze),
     0, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_STRING_TABLE, "-state", (char *) NULL, (char *) NULL,
     "normal", -1, Tk_Offset(TreeColumn_, state), 0, (ClientData) stateST,
     COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
#ifdef DEPRECATED
    {TK_OPTION_PIXELS, "-stepwidth", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(TreeColumn_, stepWidthObj),
     Tk_Offset(TreeColumn_, stepWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_RANGES},
#endif /* DEPRECATED */
    {TK_OPTION_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeColumn_, tagInfo),
     TK_OPTION_NULL_OK, (ClientData) &TreeCtrlCO_tagInfo, COLU_CONF_TAGS},
    {TK_OPTION_STRING, "-text", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(TreeColumn_, textObj), Tk_Offset(TreeColumn_, text),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_TEXT | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    /* -textcolor initialized by TreeColumn_InitInterp() */
    {TK_OPTION_CUSTOM, "-textcolor", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(TreeColumn_, textColor.obj),
     Tk_Offset(TreeColumn_, textColor), 0, (ClientData) NULL, COLU_CONF_DISPLAY},
    {TK_OPTION_INT, "-textlines", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeColumn_, textLines),
     0, (ClientData) NULL, COLU_CONF_TEXT | COLU_CONF_NWIDTH |
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-textpadx", (char *) NULL, (char *) NULL,
     "6", Tk_Offset(TreeColumn_, textPadXObj),
     Tk_Offset(TreeColumn_, textPadX), 0, (ClientData) &TreeCtrlCO_pad,
     COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-textpady", (char *) NULL, (char *) NULL,
     "0", Tk_Offset(TreeColumn_, textPadYObj),
     Tk_Offset(TreeColumn_, textPadY), 0, (ClientData) &TreeCtrlCO_pad,
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
#ifdef UNIFORM_GROUP
    {TK_OPTION_CUSTOM, "-uniform", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeColumn_, uniform), TK_OPTION_NULL_OK,
     (ClientData) &uniformGroupCO, COLU_CONF_TWIDTH},
    {TK_OPTION_INT, "-weight", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeColumn_, weight),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_TWIDTH},
#endif
    {TK_OPTION_PIXELS, "-width", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(TreeColumn_, widthObj), Tk_Offset(TreeColumn_, width),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_BOOLEAN, "-visible", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeColumn_, visible),
     0, (ClientData) NULL, COLU_CONF_TWIDTH | COLU_CONF_DISPLAY},
#ifdef DEPRECATED
    {TK_OPTION_BOOLEAN, "-widthhack", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeColumn_, widthHack),
     0, (ClientData) NULL, COLU_CONF_RANGES},
#endif /* DEPRECATED */
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

#define IS_TAIL(C) ((C) == tree->columnTail)
#define IS_ALL(C) (((C) == COLUMN_ALL) || ((C) == COLUMN_NTAIL))

/*
 *----------------------------------------------------------------------
 *
 * ColumnCO_Set --
 *
 *	Tk_ObjCustomOption.setProc(). Converts a Tcl_Obj holding a
 *	column description into a pointer to a Column.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May store a TreeColumn pointer into the internal representation
 *	pointer.  May change the pointer to the Tcl_Obj to NULL to indicate
 *	that the specified string was empty and that is acceptable.
 *
 *----------------------------------------------------------------------
 */

static int
ColumnCO_Set(
    ClientData clientData,	/* CFO_xxx flags to control the conversion. */
    Tcl_Interp *interp,		/* Current interpreter. */
    Tk_Window tkwin,		/* Window for which option is being set. */
    Tcl_Obj **value,		/* Pointer to the pointer to the value object.
				 * We use a pointer to the pointer because
				 * we may need to return a value (NULL). */
    char *recordPtr,		/* Pointer to storage for the widget record. */
    int internalOffset,		/* Offset within *recordPtr at which the
				 * internal value is to be stored. */
    char *saveInternalPtr,	/* Pointer to storage for the old value. */
    int flags			/* Flags for the option, set Tk_SetOptions. */
    )
{
    int cfoFlags = PTR2INT(clientData);
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    int objEmpty;
    TreeColumn new, *internalPtr;

    if (internalOffset >= 0)
	internalPtr = (TreeColumn *) (recordPtr + internalOffset);
    else
	internalPtr = NULL;

    objEmpty = ObjectIsEmpty((*value));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty)
	(*value) = NULL;
    else {
	if (TreeColumn_FromObj(tree, (*value), &new, cfoFlags) != TCL_OK)
	    return TCL_ERROR;
    }
    if (internalPtr != NULL) {
	if ((*value) == NULL)
	    new = NULL;
	*((TreeColumn *) saveInternalPtr) = *internalPtr;
	*internalPtr = new;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ColumnCO_Get --
 *
 *	Tk_ObjCustomOption.getProc(). Converts a TreeColumn into a
 *	Tcl_Obj string representation.
 *
 * Results:
 *	Tcl_Obj containing the string representation of the column.
 *	Returns NULL if the TreeColumn is NULL.
 *
 * Side effects:
 *	May create a new Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
ColumnCO_Get(
    ClientData clientData,	/* Not used. */
    Tk_Window tkwin,		/* Window for which option is being set. */
    char *recordPtr,		/* Pointer to widget record. */
    int internalOffset		/* Offset within *recordPtr containing the
				 * sticky value. */
    )
{
    TreeColumn value = *(TreeColumn *) (recordPtr + internalOffset);
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    if (value == NULL)
	return NULL;
#if 0
    if (value == COLUMN_ALL)
	return Tcl_NewStringObj("all", -1);
#endif
    return TreeColumn_ToObj(tree, value);
}

/*
 *----------------------------------------------------------------------
 *
 * ColumnCO_Restore --
 *
 *	Tk_ObjCustomOption.restoreProc(). Restores a TreeColumn value
 *	from a saved value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Restores the old value.
 *
 *----------------------------------------------------------------------
 */

static void
ColumnCO_Restore(
    ClientData clientData,	/* Not used. */
    Tk_Window tkwin,		/* Not used. */
    char *internalPtr,		/* Where to store old value. */
    char *saveInternalPtr)	/* Pointer to old value. */
{
    *(TreeColumn *) internalPtr = *(TreeColumn *) saveInternalPtr;
}

/*
 * The following structure contains pointers to functions used for processing
 * a custom config option that handles Tcl_Obj<->TreeColumn conversion.
 * A column description must refer to a single column.
 */
Tk_ObjCustomOption TreeCtrlCO_column =
{
    "column",
    ColumnCO_Set,
    ColumnCO_Get,
    ColumnCO_Restore,
    NULL,
    (ClientData) (CFO_NOT_NULL)
};

/*
 * The following structure contains pointers to functions used for processing
 * a custom config option that handles Tcl_Obj<->TreeColumn conversion.
 * A column description must refer to a single column.
 * "tail" is not allowed.
 */
Tk_ObjCustomOption TreeCtrlCO_column_NOT_TAIL =
{
    "column",
    ColumnCO_Set,
    ColumnCO_Get,
    ColumnCO_Restore,
    NULL,
    (ClientData) (CFO_NOT_NULL | CFO_NOT_TAIL)
};

static Tk_OptionSpec dragSpecs[] = {
    {TK_OPTION_BOOLEAN, "-enable", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeCtrl, columnDrag.enable),
     0, (ClientData) NULL, 0},
    {TK_OPTION_INT, "-imagealpha", (char *) NULL, (char *) NULL,
     "128", -1, Tk_Offset(TreeCtrl, columnDrag.alpha),
     0, (ClientData) NULL, 0},
    {TK_OPTION_COLOR, "-imagecolor", (char *) NULL, (char *) NULL,
     "gray75", -1, Tk_Offset(TreeCtrl, columnDrag.color),
     0, (ClientData) NULL, 0},
    {TK_OPTION_CUSTOM, "-imagecolumn", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeCtrl, columnDrag.column),
     TK_OPTION_NULL_OK, (ClientData) &TreeCtrlCO_column_NOT_TAIL, 0},
    {TK_OPTION_PIXELS, "-imageoffset", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(TreeCtrl, columnDrag.offsetObj),
     Tk_Offset(TreeCtrl, columnDrag.offset), 0, (ClientData) NULL, 0},
    {TK_OPTION_COLOR, "-indicatorcolor", (char *) NULL, (char *) NULL,
     "Black", -1, Tk_Offset(TreeCtrl, columnDrag.indColor),
     0, (ClientData) NULL, 0},
    {TK_OPTION_CUSTOM, "-indicatorcolumn", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeCtrl, columnDrag.indColumn),
     TK_OPTION_NULL_OK, (ClientData) &TreeCtrlCO_column, 0},
    {TK_OPTION_STRING_TABLE, "-indicatorside", (char *) NULL, (char *) NULL,
     "left", -1, Tk_Offset(TreeCtrl, columnDrag.indSide),
     0, (ClientData) arrowSideST, 0},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

/*
 *----------------------------------------------------------------------
 *
 * ImageChangedProc --
 *
 *	This procedure is invoked by the image code whenever the manager
 *	for an image does something that affects the size or contents
 *	of an image displayed in a column header.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Invalidates the size of the column and schedules a redisplay.
 *
 *----------------------------------------------------------------------
 */

static void
ImageChangedProc(
    ClientData clientData,		/* Pointer to Column record. */
    int x, int y,			/* Upper left pixel (within image)
					 * that must be redisplayed. */
    int width, int height,		/* Dimensions of area to redisplay
					 * (may be <= 0). */
    int imageWidth, int imageHeight	/* New dimensions of image. */
    )
{
    /* I would like to know the image was deleted... */
    TreeColumn column = clientData;
    TreeCtrl *tree = column->tree;

    /* Duplicate the effects of configuring the -image option. */
    column->neededWidth = -1;
    column->neededHeight = -1;
    tree->headerHeight = -1;
    tree->widthOfColumns = -1;
    tree->widthOfColumnsLeft = tree->widthOfColumnsRight = -1;
    Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH | DINFO_DRAW_HEADER);
}

/*
 *----------------------------------------------------------------------
 *
 * ColumnStateFromObj --
 *
 *	Parses a string object containing "state" or "!state" to a
 *	state bit flag.
 *	This function is passed to PerStateInfo_FromObj().
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ColumnStateFromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *obj,		/* String object to parse. */
    int *stateOff,		/* OR'd with state bit if "!state" is
				 * specified. Caller must initialize. */
    int *stateOn		/* OR'd with state bit if "state" is
				 * specified. Caller must initialize. */
    )
{
    Tcl_Interp *interp = tree->interp;
    int i, op = STATE_OP_ON, op2, op3, length, state = 0;
    char ch0, *string;
    CONST char *stateNames[4] = { "normal", "active", "pressed", "up" };
    int states[3];

    states[STATE_OP_ON] = 0;
    states[STATE_OP_OFF] = 0;
    states[STATE_OP_TOGGLE] = 0;

    string = Tcl_GetStringFromObj(obj, &length);
    if (length == 0)
	goto unknown;
    ch0 = string[0];
    if (ch0 == '!') {
	op = STATE_OP_OFF;
	++string;
	ch0 = string[0];
    } else if (ch0 == '~') {
	if (1) {
	    FormatResult(interp, "can't specify '~' for this command");
	    return TCL_ERROR;
	}
	op = STATE_OP_TOGGLE;
	++string;
	ch0 = string[0];
    }
    for (i = 0; i < 4; i++) {
	if ((ch0 == stateNames[i][0]) && !strcmp(string, stateNames[i])) {
	    state = 1L << i;
	    break;
	}
    }
    if (state == 0)
	goto unknown;

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

    *stateOn |= states[STATE_OP_ON];
    *stateOff |= states[STATE_OP_OFF];

    return TCL_OK;

unknown:
    FormatResult(interp, "unknown state \"%s\"", string);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_MakeState --
 *
 *	Return a bit mask suitable for passing to the PerState_xxx
 *	functions.
 *
 * Results:
 *	State flags for the column's current state.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Column_MakeState(
    TreeColumn column		/* Column record. */
    )
{
    int state = 0;
    if (column->state == COLUMN_STATE_NORMAL)
	state |= 1L << 0;
    else if (column->state == COLUMN_STATE_ACTIVE)
	state |= 1L << 1;
    else if (column->state == COLUMN_STATE_PRESSED)
	state |= 1L << 2;
    if (column->arrow == COLUMN_ARROW_UP)
	state |= 1L << 3;
    return state;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_FirstAndLast --
 *
 *	Determine the order of two columns and swap them if needed.
 *
 * Results:
 *	The return value is the number of columns in the range between
 *	first and last.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_FirstAndLast(
    TreeColumn *first,		/* Column token. */
    TreeColumn *last		/* Column token. */
    )
{
    int indexFirst, indexLast, index;

    indexFirst = TreeColumn_Index(*first);
    indexLast = TreeColumn_Index(*last);
    if (indexFirst > indexLast) {
	TreeColumn column = *first;
	*first = *last;
	*last = column;

	index = indexFirst;
	indexFirst = indexLast;
	indexLast = index;
    }
    return indexLast - indexFirst + 1;
}

/*
 *----------------------------------------------------------------------
 *
 * ColumnHasTag --
 *
 *	Checks whether a column has a certain tag.
 *
 * Results:
 *	Returns TRUE if the column has the given tag.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ColumnHasTag(
    TreeColumn column,		/* The column to test. */
    Tk_Uid tag			/* Tag to look for. */
    )
{
    TagInfo *tagInfo = column->tagInfo;
    Tk_Uid *tagPtr;
    int count;

    if (tagInfo == NULL)
	return 0;

    for (tagPtr = tagInfo->tagPtr, count = tagInfo->numTags;
	count > 0; tagPtr++, count--) {
	if (*tagPtr == tag) {
	    return 1;
	}
    }
    return 0;
}

typedef struct Qualifiers {
    TreeCtrl *tree;
    int visible;		/* 1 for -visible TRUE,
				   0 for -visible FALSE,
				   -1 for unspecified. */
    int states[3];		/* States that must be on or off. */
    TagExpr expr;		/* Tag expression. */
    int exprOK;			/* TRUE if expr is valid. */
    int lock;			/* COLUMN_LOCK_xxx or -1 */
    int ntail;			/* 1 for !tail,
				 * 0 for unspecified. */
    Tk_Uid tag;			/* Tag (without operators) or NULL. */
} Qualifiers;

/*
 *----------------------------------------------------------------------
 *
 * Qualifiers_Init --
 *
 *	Helper routine for TreeItem_FromObj.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Qualifiers_Init(
    TreeCtrl *tree,		/* Widget info. */
    Qualifiers *q		/* Out: Initialized qualifiers. */
    )
{
    q->tree = tree;
    q->visible = -1;
    q->states[0] = q->states[1] = q->states[2] = 0;
    q->exprOK = FALSE;
    q->lock = -1;
    q->ntail = 0;
    q->tag = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Qualifiers_Scan --
 *
 *	Helper routine for TreeItem_FromObj.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Qualifiers_Scan(
    Qualifiers *q,		/* Must call Qualifiers_Init first,
				 * and Qualifiers_Free if result is TCL_OK. */
    int objc,			/* Number of arguments. */
    Tcl_Obj **objv,		/* Argument values. */
    int startIndex,		/* First objv[] index to look at. */
    int *argsUsed		/* Out: number of objv[] used. */
    )
{
    TreeCtrl *tree = q->tree;
    Tcl_Interp *interp = tree->interp;
    int qual, j = startIndex;

    static CONST char *qualifiers[] = {
	"lock", "state", "tag", "visible", "!tail", "!visible", NULL
    };
    enum qualEnum {
	QUAL_LOCK, QUAL_STATE, QUAL_TAG, QUAL_VISIBLE, QUAL_NOT_TAIL,
	QUAL_NOT_VISIBLE
    };
    /* Number of arguments used by qualifiers[]. */
    static int qualArgs[] = {
	2, 2, 2, 1, 1, 1
    };

    *argsUsed = 0;

    for (; j < objc; ) {
	if (Tcl_GetIndexFromObj(NULL, objv[j], qualifiers, NULL, 0,
		&qual) != TCL_OK)
	    break;
	if (objc - j < qualArgs[qual]) {
	    Tcl_AppendResult(interp, "missing arguments to \"",
		    Tcl_GetString(objv[j]), "\" qualifier", NULL);
	    goto errorExit;
	}
	switch ((enum qualEnum) qual) {
	    case QUAL_LOCK: {
		if (Tcl_GetIndexFromObj(interp, objv[j + 1], lockST,
			"lock", 0, &q->lock) != TCL_OK)
		    goto errorExit;
		break;
	    }
	    case QUAL_STATE: {
		int i, listObjc;
		Tcl_Obj **listObjv;

		if (Tcl_ListObjGetElements(interp, objv[j + 1],
			&listObjc, &listObjv) != TCL_OK)
		    goto errorExit;
		q->states[STATE_OP_OFF] = q->states[STATE_OP_ON] = 0;
		for (i = 0; i < listObjc; i++) {
		    if (ColumnStateFromObj(tree, listObjv[i],
			    &q->states[STATE_OP_OFF],
			    &q->states[STATE_OP_ON]) != TCL_OK)
			goto errorExit;
		}
		break;
	    }
	    case QUAL_TAG: {
		if (tree->columnTagExpr) {
		    if (q->exprOK)
			TagExpr_Free(&q->expr);
		    if (TagExpr_Init(tree, objv[j + 1], &q->expr) != TCL_OK)
			return TCL_ERROR;
		    q->exprOK = TRUE;
		} else {
		    q->tag = Tk_GetUid(Tcl_GetString(objv[j + 1]));
		}
		break;
	    }
	    case QUAL_VISIBLE: {
		q->visible = 1;
		break;
	    }
	    case QUAL_NOT_TAIL: {
		q->ntail = 1;
		break;
	    }
	    case QUAL_NOT_VISIBLE: {
		q->visible = 0;
		break;
	    }
	}
	*argsUsed += qualArgs[qual];
	j += qualArgs[qual];
    }
    return TCL_OK;
errorExit:
    if (q->exprOK)
	TagExpr_Free(&q->expr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Qualifies --
 *
 *	Helper routine for TreeItem_FromObj.
 *
 * Results:
 *	Returns TRUE if the item meets the given criteria.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Qualifies(
    Qualifiers *q,		/* Qualifiers to check. */
    TreeColumn column		/* The column to test. May be NULL. */
    )
{
    /* Note: if the column is NULL it is a "match" because we have run
     * out of columns to check. */
    if (column == NULL)
	return 1;
    if ((q->ntail == 1) && (column == column->tree->columnTail))
	return 0;
    if ((q->visible == 1) && !column->visible)
	return 0;
    else if ((q->visible == 0) && column->visible)
	return 0;
    if (q->states[STATE_OP_OFF] & Column_MakeState(column))
	return 0;
    if ((q->states[STATE_OP_ON] & Column_MakeState(column)) != q->states[STATE_OP_ON])
	return 0;
    if (q->exprOK && !TagExpr_Eval(&q->expr, column->tagInfo))
	return 0;
    if ((q->lock != -1) && (column->lock != q->lock))
	return 0;
    if ((q->tag != NULL) && !ColumnHasTag(column, q->tag))
	return 0;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Qualifiers_Free --
 *
 *	Helper routine for TreeItem_FromObj.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Qualifiers_Free(
    Qualifiers *q		/* Out: Initialized qualifiers. */
    )
{
    if (q->exprOK)
	TagExpr_Free(&q->expr);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumnList_FromObj --
 *
 *	Parse a Tcl_Obj column description to get a list of columns.
 *
 * -- returning a single column --
 * ID MODIFIERS
 * first QUALIFIERS MODIFIERS
 * end|last QUALIFIERS MODIFIERS
 * order N QUALIFIERS MODIFIERS
 * tail
 * tree
 * -- returning multiple columns --
 * all QUALIFIERS
 * QUALIFIERS (like "all QUALIFIERS")
 * list listOfDescs
 * range first last QUALIFIERS
 * tag tagExpr QUALIFIERS
 * TAG-EXPR QUALIFIERS MODIFIERS
 *
 * MODIFIERS:
 * -- returning a single column --
 * next QUALIFIERS
 * prev QUALIFIERS
 *
 * QUALIFIERS:
 * state stateList
 * tag tagExpr
 * visible
 * !visible
 * !tail
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
TreeColumnList_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,		/* Column description. */
    TreeColumnList *columns,	/* Uninitialized list. Caller must free
				 * it with TreeColumnList_Free unless the
				 * result of this function is TCL_ERROR. */
    int flags			/* CFO_xxx flags. */
    )
{
    Tcl_Interp *interp = tree->interp;
    int i, objc, index, listIndex;
    Tcl_Obj **objv, *elemPtr;
    TreeColumn column = NULL;
    Qualifiers q;
    int qualArgsTotal;

    static CONST char *indexName[] = {
	"all", "end", "first", "last", "list", "order", "range", "tail",
	"tree", (char *) NULL
    };
    enum indexEnum {
	INDEX_ALL, INDEX_END, INDEX_FIRST, INDEX_LAST, INDEX_LIST, INDEX_ORDER,
	INDEX_RANGE, INDEX_TAIL, INDEX_TREE
    } ;
    /* Number of arguments used by indexName[]. */
    static int indexArgs[] = {
	1, 1, 1, 1, 2, 2, 3, 1, 1
    };
    /* Boolean: can indexName[] be followed by 1 or more qualifiers. */
    static int indexQual[] = {
	1, 0, 1, 1, 0, 1, 1, 0, 0
    };

    static CONST char *modifiers[] = {
	"next", "prev", (char *) NULL
    };
    enum modEnum {
	TMOD_NEXT, TMOD_PREV
    };
    /* Number of arguments used by modifiers[]. */
    static int modArgs[] = {
	1, 1
    };
    /* Boolean: can modifiers[] be followed by 1 or more qualifiers. */
    static int modQual[] = {
	1, 1
    };

    TreeColumnList_Init(tree, columns, 0);
    Qualifiers_Init(tree, &q);

    if (Tcl_ListObjGetElements(NULL, objPtr, &objc, &objv) != TCL_OK)
	goto badDesc;
    if (objc == 0)
	goto badDesc;

    listIndex = 0;
    elemPtr = objv[listIndex];
    if (Tcl_GetIndexFromObj(NULL, elemPtr, indexName, NULL, 0, &index)
	    == TCL_OK) {

	if (objc - listIndex < indexArgs[index]) {
	    Tcl_AppendResult(interp, "missing arguments to \"",
		    Tcl_GetString(elemPtr), "\" keyword", NULL);
	    goto errorExit;
	}

	qualArgsTotal = 0;
	if (indexQual[index]) {
	    if (Qualifiers_Scan(&q, objc, objv, listIndex + indexArgs[index],
		    &qualArgsTotal) != TCL_OK) {
		goto errorExit;
	    }
	}

	switch ((enum indexEnum) index) {
	    case INDEX_ALL: {
		if (qualArgsTotal) {
		    column = tree->columns;
		    while (column != NULL) {
			if (Qualifies(&q, column)) {
			    TreeColumnList_Append(columns, column);
			}
			column = column->next;
		    }
		    if (!(flags & CFO_NOT_TAIL) &&
			    Qualifies(&q, tree->columnTail)) {
			TreeColumnList_Append(columns, tree->columnTail);
		    }
		    column = NULL;
		} else if (flags & CFO_LIST_ALL) {
		    column = tree->columns;
		    while (column != NULL) {
			TreeColumnList_Append(columns, column);
			column = column->next;
		    }
		    if (!(flags & CFO_NOT_TAIL))
			TreeColumnList_Append(columns, tree->columnTail);
		    column = NULL;
		} else if (flags & CFO_NOT_TAIL) {
		    column = COLUMN_NTAIL;
		} else {
		    column = COLUMN_ALL;
		}
		break;
	    }
	    case INDEX_FIRST: {
		column = tree->columns;
		while (!Qualifies(&q, column))
		    column = column->next;
		break;
	    }
	    case INDEX_END:
	    case INDEX_LAST: {
		column = tree->columnLast;
		while (!Qualifies(&q, column)) {
		    column = column->prev;
		}
		break;
	    }
	    case INDEX_LIST: {
		int listObjc;
		Tcl_Obj **listObjv;
		int count;

		if (Tcl_ListObjGetElements(interp, objv[listIndex + 1],
			&listObjc, &listObjv) != TCL_OK)
		    goto errorExit;
		for (i = 0; i < listObjc; i++) {
		    TreeColumnList column2s;
		    if (TreeColumnList_FromObj(tree, listObjv[i], &column2s,
			    flags) != TCL_OK)
			goto errorExit;
		    TreeColumnList_Concat(columns, &column2s);
		    TreeColumnList_Free(&column2s);
		}
		/* If any of the column descriptions in the list is "all", then
		 * clear the list of columns and use "all". */
		count = TreeColumnList_Count(columns);
		for (i = 0; i < count; i++) {
		    TreeColumn column = TreeColumnList_Nth(columns, i);
		    if (IS_ALL(column))
			break;
		}
		if (i < count) {
		    TreeColumnList_Free(columns);
		    if (flags & CFO_NOT_TAIL)
			column = COLUMN_NTAIL;
		    else
			column = COLUMN_ALL;
		} else
		    column = NULL;
		break;
	    }
	    case INDEX_ORDER: {
		int order;

		if (Tcl_GetIntFromObj(NULL, objv[listIndex + 1], &order) != TCL_OK)
		    goto errorExit;
		column = tree->columns;
		while (column != NULL) {
		    if (Qualifies(&q, column))
			if (order-- <= 0)
			    break;
		    column = column->next;
		}
		break;
	    }
	    case INDEX_RANGE: {
		TreeColumn _first, _last;

		if (TreeColumn_FromObj(tree, objv[listIndex + 1],
			&_first, CFO_NOT_NULL) != TCL_OK)
		    goto errorExit;
		if (TreeColumn_FromObj(tree, objv[listIndex + 2],
			&_last, CFO_NOT_NULL) != TCL_OK)
		    goto errorExit;
		(void) TreeColumn_FirstAndLast(&_first, &_last);
		column = _first;
		while (1) {
		    if (Qualifies(&q, column)) {
			TreeColumnList_Append(columns, column);
		    }
		    if (column == _last)
			break;
		    column = column->next;
		    if (column == NULL)
			column = tree->columnTail;
		}
		column = NULL;
		break;
	    }
	    case INDEX_TAIL: {
		column = tree->columnTail;
		break;
	    }
	    case INDEX_TREE: {
		column = tree->columnTree;
		break;
	    }
	}
	listIndex += indexArgs[index] + qualArgsTotal;

    /* No indexName[] was found. */
    } else {
	int gotId = FALSE, id;
	TagExpr expr;

	if (tree->columnPrefixLen) {
	    char *end, *t = Tcl_GetString(elemPtr);
	    if (strncmp(t, tree->columnPrefix, tree->columnPrefixLen) == 0) {
		t += tree->columnPrefixLen;
		id = strtoul(t, &end, 10);
		if ((end != t) && (*end == '\0'))
		    gotId = TRUE;
	    }

	} else if (Tcl_GetIntFromObj(NULL, elemPtr, &id) == TCL_OK) {
	    gotId = TRUE;
	}
	if (gotId) {
	    column = tree->columns;
	    while (column) {
		if (column->id == id)
		    break;
		column = column->next;
	    }
	    listIndex++;
	    goto gotFirstPart;
	}

	/* Try a list of qualifiers. This has the same effect as
	 * "all QUALIFIERS". */
	if (Qualifiers_Scan(&q, objc, objv, listIndex, &qualArgsTotal)
		!= TCL_OK) {
	    goto errorExit;
	}
	if (qualArgsTotal) {
	    column = tree->columns;
	    while (column != NULL) {
		if (Qualifies(&q, column)) {
		    TreeColumnList_Append(columns, column);
		}
		column = column->next;
	    }
	    if (!(flags & CFO_NOT_TAIL) &&
		    Qualifies(&q, tree->columnTail)) {
		TreeColumnList_Append(columns, tree->columnTail);
	    }
	    column = NULL;
	    listIndex += qualArgsTotal;
	    goto gotFirstPart;
	}

	/* Try a tag or tag expression followed by qualifiers. */
	if (objc > 1) {
	    if (Qualifiers_Scan(&q, objc, objv, listIndex + 1,
		    &qualArgsTotal) != TCL_OK) {
		goto errorExit;
	    }
	}
	if (tree->columnTagExpr) {
	    if (TagExpr_Init(tree, elemPtr, &expr) != TCL_OK)
		goto errorExit;
	    column = tree->columns;
	    while (column != NULL) {
		if (TagExpr_Eval(&expr, column->tagInfo) && Qualifies(&q, column)) {
		    TreeColumnList_Append(columns, column);
		}
		column = column->next;
	    }
	    if (!(flags & CFO_NOT_TAIL) &&
		    TagExpr_Eval(&expr, tree->columnTail->tagInfo) &&
		    Qualifies(&q, tree->columnTail)) {
		TreeColumnList_Append(columns, tree->columnTail);
	    }
	    TagExpr_Free(&expr);
	} else {
	    Tk_Uid tag = Tk_GetUid(Tcl_GetString(elemPtr));
	    column = tree->columns;
	    while (column != NULL) {
		if (ColumnHasTag(column, tag) && Qualifies(&q, column)) {
		    TreeColumnList_Append(columns, column);
		}
		column = column->next;
	    }
	    if (!(flags & CFO_NOT_TAIL) &&
		ColumnHasTag(tree->columnTail, tag) &&
		Qualifies(&q, tree->columnTail)) {
		TreeColumnList_Append(columns, tree->columnTail);
	    }
	}
	column = NULL;
	listIndex += 1 + qualArgsTotal;
    }

gotFirstPart:

    /* If 1 column, use it and clear the list. */
    if (TreeColumnList_Count(columns) == 1) {
	column = TreeColumnList_Nth(columns, 0);
	columns->count = 0;
    }

    /* If "all" but only tail column exists, use it. */
    if (IS_ALL(column) && (tree->columns == NULL) && !(flags & CFO_NOT_TAIL))
	column = tree->columnTail;

    /* If > 1 column, no modifiers may follow. */
    if ((TreeColumnList_Count(columns) > 1) || IS_ALL(column)) {
	if (listIndex  < objc) {
	    Tcl_AppendResult(interp, "unexpected arguments after \"",
		(char *) NULL);
	    for (i = 0; i < listIndex; i++) {
		Tcl_AppendResult(interp, Tcl_GetString(objv[i]), (char *) NULL);
		if (i != listIndex - 1)
		    Tcl_AppendResult(interp, " ", (char *) NULL);
	    }
	    Tcl_AppendResult(interp, "\"", (char *) NULL);
	    goto errorExit;
	}
    }

    /* This means a valid specification was given, but there is no such column */
    if ((TreeColumnList_Count(columns) == 0) && (column == NULL)) {
	if (flags & CFO_NOT_NULL)
	    goto notNull;
	/* Empty list returned */
	goto goodExit;
    }

    /* Process any modifiers following the column we matched above. */
    for (; listIndex < objc; /* nothing */) {
	int qualArgsTotal = 0;

	elemPtr = objv[listIndex];
	if (Tcl_GetIndexFromObj(interp, elemPtr, modifiers, "modifier", 0,
		    &index) != TCL_OK) {
	    goto errorExit;
	}
	if (objc - listIndex < modArgs[index]) {
	    Tcl_AppendResult(interp, "missing arguments to \"",
		    Tcl_GetString(elemPtr), "\" modifier", NULL);
	    goto errorExit;
	}
	if (modQual[index]) {
	    Qualifiers_Free(&q);
	    Qualifiers_Init(tree, &q);
	    if (Qualifiers_Scan(&q, objc, objv, listIndex + modArgs[index],
		    &qualArgsTotal) != TCL_OK) {
		goto errorExit;
	    }
	}
	switch ((enum modEnum) index) {
	    case TMOD_NEXT: {
		int isTail = IS_TAIL(column);
		if (isTail) {
		    column = NULL;
		    break;
		}
		column = column->next;
		while (!Qualifies(&q, column))
		    column = column->next;
		if (column == NULL) {
		    column = tree->columnTail;
		    if (!Qualifies(&q, column))
			column = NULL;
		}
		break;
	    }
	    case TMOD_PREV: {
		int isTail = IS_TAIL(column);
		if (isTail)
		    column = tree->columnLast;
		else
		    column = column->prev;
		while (!Qualifies(&q, column))
		    column = column->prev;
		break;
	    }
	}
	if ((TreeColumnList_Count(columns) == 0) && (column == NULL)) {
	    if (flags & CFO_NOT_NULL)
		goto notNull;
	    /* Empty list returned. */
	    goto goodExit;
	}
	listIndex += modArgs[index] + qualArgsTotal;
    }
    if ((flags & CFO_NOT_MANY) && (IS_ALL(column) ||
	    (TreeColumnList_Count(columns) > 1))) {
	FormatResult(interp, "can't specify > 1 column for this command");
	goto errorExit;
    }
    if ((flags & CFO_NOT_NULL) && (TreeColumnList_Count(columns) == 0) &&
	    (column == NULL)) {
notNull:
	FormatResult(interp, "column \"%s\" doesn't exist", Tcl_GetString(objPtr));
	goto errorExit;
    }
    if (TreeColumnList_Count(columns)) {
	if (flags & (CFO_NOT_TAIL)) {
	    int i;
	    for (i = 0; i < TreeColumnList_Count(columns); i++) {
		column = TreeColumnList_Nth(columns, i);
		if ((flags & CFO_NOT_TAIL) && IS_TAIL(column))
		    goto notTail;
	    }
	}
    } else if (IS_ALL(column)) {
	TreeColumnList_Append(columns, column);
    } else {
	if ((flags & CFO_NOT_TAIL) && IS_TAIL(column)) {
notTail:
	    FormatResult(interp, "can't specify \"tail\" for this command");
	    goto errorExit;
	}
	TreeColumnList_Append(columns, column);
    }
goodExit:
    Qualifiers_Free(&q);
    return TCL_OK;

badDesc:
    FormatResult(interp, "bad column description \"%s\"", Tcl_GetString(objPtr));
    goto errorExit;

errorExit:
    Qualifiers_Free(&q);
    TreeColumnList_Free(columns);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_FromObj --
 *
 *	Parse a Tcl_Obj column description to get a single column.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,		/* Object to parse to a column. */
    TreeColumn *columnPtr,	/* Returned column. */
    int flags			/* CFO_xxx flags */
    )
{
    TreeColumnList columns;

    if (TreeColumnList_FromObj(tree, objPtr, &columns, flags | CFO_NOT_MANY) != TCL_OK)
	return TCL_ERROR;
    /* May be NULL. */
    (*columnPtr) = TreeColumnList_Nth(&columns, 0);
    TreeColumnList_Free(&columns);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumnForEach_Start --
 *
 *	Begin iterating over items. A command might accept two column
 *	descriptions for a range of column, or a single column description
 *	which may itself refer to multiple column. Either column
 *	description could be "all".
 *
 * Results:
 *	Returns the first column to iterate over. If an error occurs
 *	then ColumnForEach.error is set to 1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeColumn
TreeColumnForEach_Start(
    TreeColumnList *columns,	/* List of columns. */
    TreeColumnList *column2s,	/* List of columns or NULL. */
    ColumnForEach *iter		/* Returned info, pass to
				   TreeColumnForEach_Next. */
    )
{
    TreeCtrl *tree = columns->tree;
    TreeColumn column, column2 = NULL;

    column = TreeColumnList_Nth(columns, 0);
    if (column2s)
	column2 = TreeColumnList_Nth(column2s, 0);

    iter->tree = tree;
    iter->all = FALSE;
    iter->ntail = FALSE;
    iter->error = 0;
    iter->list = NULL;

    if (IS_ALL(column) || IS_ALL(column2)) {
	iter->all = TRUE;
	iter->ntail = (column == COLUMN_NTAIL) || (column2 == COLUMN_NTAIL);
	if (tree->columns == NULL)
	    return iter->current = iter->ntail ? NULL : tree->columnTail;
	iter->next = TreeColumn_Next(tree->columns);
	return iter->current = tree->columns;
    }

    if (column2 != NULL) {
	if (TreeColumn_FirstAndLast(&column, &column2) == 0) {
	    iter->error = 1;
	    return NULL;
	}
	iter->next = TreeColumn_Next(column);
	iter->last = column2;
	return iter->current = column;
    }

    iter->list = columns;
    iter->index = 0;
    return iter->current = column;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumnForEach_Next --
 *
 *	Returns the next column to iterate over. Keep calling this until
 *	the result is NULL.
 *
 * Results:
 *	Returns the next column to iterate over or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeColumn
TreeColumnForEach_Next(
    ColumnForEach *iter		/* Initialized by TreeColumnForEach_Start. */
    )
{
    TreeCtrl *tree = iter->tree;
    TreeColumn column;

    if (iter->all) {
	if (iter->current == tree->columnTail)
	    return iter->current = NULL;
	column = iter->next;
	if (column == NULL)
	    return iter->current = iter->ntail ? NULL : tree->columnTail;
	iter->next = TreeColumn_Next(column);
	return iter->current = column;
    }

    if (iter->list != NULL) {
	if (iter->index >= TreeColumnList_Count(iter->list))
	    return iter->current = NULL;
	return iter->current = TreeColumnList_Nth(iter->list, ++iter->index);
    }

    if (iter->current == iter->last)
	return iter->current = NULL;
    column = iter->next;
    iter->next = TreeColumn_Next(column);
    return iter->current = column;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_ToObj --
 *
 *	Return a Tcl_Obj representing a column.
 *
 * Results:
 *	A Tcl_Obj.
 *
 * Side effects:
 *	Allocates a Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TreeColumn_ToObj(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn column		/* Column token to get Tcl_Obj for. */
    )
{
    if (column == tree->columnTail)
	return Tcl_NewStringObj("tail", -1);
    if (tree->columnPrefixLen) {
	char buf[100 + TCL_INTEGER_SPACE];
	(void) sprintf(buf, "%s%d", tree->columnPrefix, column->id);
	return Tcl_NewStringObj(buf, -1);
    }
    return Tcl_NewIntObj(column->id);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_FindColumn --
 *
 *	Get the N'th column in a TreeCtrl.
 *
 * Results:
 *	Token for the N'th column.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeColumn
Tree_FindColumn(
    TreeCtrl *tree,		/* Widget info. */
    int columnIndex		/* 0-based index of the column to return. */
    )
{
    TreeColumn column = tree->columns;

    while (column != NULL) {
	if (column->index == columnIndex)
	    break;
	column = column->next;
    }
    return column;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_Next --
 *
 *	Return the column to the right of the given one.
 *
 * Results:
 *	Token for the next column.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeColumn
TreeColumn_Next(
    TreeColumn column		/* Column token. */
    )
{
    return column->next;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_Prev --
 *
 *	Return the column to the left of the given one.
 *
 * Results:
 *	Token for the previous column.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeColumn
TreeColumn_Prev(
    TreeColumn column		/* Column token. */
    )
{
    return column->prev;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_FreeColors --
 *
 *	Frees an array of XColors. This is used to free the -itembackground
 *	array of colors.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated, colors are freed.
 *
 *----------------------------------------------------------------------
 */

static void
Column_FreeColors(
    TreeColumn column,		/* Column token. */
    TreeColor **colors,		/* Array of colors. May be NULL. */
    int count			/* Number of colors. */
    )
{
    int i;

    if (colors == NULL) {
	return;
    }
    for (i = 0; i < count; i++) {
	if (colors[i] != NULL) {
	    Tree_FreeColor(column->tree, colors[i]);
	}
    }
    WCFREE(colors, XColor *, count);
}

/*
 *----------------------------------------------------------------------
 *
 * Column_Move --
 *
 *	Move a column before another.
 *
 * Results:
 *	If the column is moved, then the list of item-columns for every item
 *	is rearranged and the treectrl option -defaultstyles is rearranged.
 *	Whether the column is moved or not, the .index field of every
 *	column is recalculated.
 *
 * Side effects:
 *	A redisplay is scheduled if the moved column is visible.
 *
 *----------------------------------------------------------------------
 */

static void
Column_Move(
    TreeColumn move,		/* Column to move. */
    TreeColumn before		/* Column to place 'move' in front of.
				 * May be the same as 'move'. */
    )
{
    TreeCtrl *tree = move->tree;
    TreeColumn column, prev, next, last;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    TreeItem item;
    int index;
#ifdef DEPRECATED
    int numStyles;
#endif

    if (move == before)
	goto renumber;
    if (move->index == before->index - 1)
	goto renumber;

    /* Move the column in every item */
    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
    while (hPtr != NULL) {
	item = (TreeItem) Tcl_GetHashValue(hPtr);
	TreeItem_MoveColumn(tree, item, move->index, before->index);
	hPtr = Tcl_NextHashEntry(&search);
    }

    /* Indicate that all items must recalculate their list of spans. */
    TreeItem_SpansInvalidate(tree, NULL);

#ifdef DEPRECATED
    /* Re-order -defaultstyle */
    numStyles = tree->defaultStyle.numStyles;
    if ((numStyles > 0) && ((before->index < numStyles) ||
	    (move->index < numStyles))) {
	TreeStyle style, *styles;
	int i, j;
	Tcl_Obj *staticObjv[STATIC_SIZE], **objv = staticObjv;

	/* Case 1: move existing */
	if ((before->index <= numStyles) && (move->index < numStyles)) {
	    styles = tree->defaultStyle.styles;
	    style = styles[move->index];
	    for (i = move->index; i < numStyles - 1; i++)
		styles[i] = styles[i + 1];
	    j = before->index;
	    if (move->index < before->index)
		j--;
	    for (i = numStyles - 1; i > j; i--)
		styles[i] = styles[i - 1];
	    styles[j] = style;

	/* Case 2: insert empty between existing */
	} else if (before->index < numStyles) {
	    numStyles++;
	    styles = (TreeStyle *) ckalloc(numStyles * sizeof(TreeStyle));
	    for (i = 0; i < before->index; i++)
		styles[i] = tree->defaultStyle.styles[i];
	    styles[i++] = NULL;
	    for (; i < numStyles; i++)
		styles[i] = tree->defaultStyle.styles[i - 1];

	/* Case 3: move existing past end */
	} else {
	    numStyles += before->index - numStyles;
	    styles = (TreeStyle *) ckalloc(numStyles * sizeof(TreeStyle));
	    style = tree->defaultStyle.styles[move->index];
	    for (i = 0; i < move->index; i++)
		styles[i] = tree->defaultStyle.styles[i];
	    for (; i < tree->defaultStyle.numStyles - 1; i++)
		styles[i] = tree->defaultStyle.styles[i + 1];
	    for (; i < numStyles - 1; i++)
		styles[i] = NULL;
	    styles[i] = style;
	}
	Tcl_DecrRefCount(tree->defaultStyle.stylesObj);
	STATIC_ALLOC(objv, Tcl_Obj *, numStyles);
	for (i = 0; i < numStyles; i++) {
	    if (styles[i] != NULL)
		objv[i] = TreeStyle_ToObj(styles[i]);
	    else
		objv[i] = Tcl_NewObj();
	}
	tree->defaultStyle.stylesObj = Tcl_NewListObj(numStyles, objv);
	Tcl_IncrRefCount(tree->defaultStyle.stylesObj);
	STATIC_FREE(objv, Tcl_Obj *, numStyles);
	if (styles != tree->defaultStyle.styles) {
	    ckfree((char *) tree->defaultStyle.styles);
	    tree->defaultStyle.styles = styles;
	    tree->defaultStyle.numStyles = numStyles;
	}
    }
#endif /* DEPRECATED */

    /* Unlink. */
    prev = move->prev;
    next = move->next;
    if (prev == NULL)
	tree->columns = next;
    else
	prev->next = next;
    if (next == NULL)
	tree->columnLast = prev;
    else
	next->prev = prev;

    /* Link. */
    if (before == tree->columnTail) {
	last = tree->columnLast;
	last->next = move;
	move->prev = last;
	move->next = NULL;
	tree->columnLast = move;
    } else {
	prev = before->prev;
	if (prev == NULL)
	    tree->columns = move;
	else
	    prev->next = move;
	before->prev = move;
	move->prev = prev;
	move->next = before;
    }

    /* Renumber columns */
renumber:
    tree->columnLockLeft = NULL;
    tree->columnLockNone = NULL;
    tree->columnLockRight = NULL;

    index = 0;
    column = tree->columns;
    while (column != NULL) {
	column->index = index++;
	if (column->lock == COLUMN_LOCK_LEFT && tree->columnLockLeft == NULL)
	    tree->columnLockLeft = column;
	if (column->lock == COLUMN_LOCK_NONE && tree->columnLockNone == NULL)
	    tree->columnLockNone = column;
	if (column->lock == COLUMN_LOCK_RIGHT && tree->columnLockRight == NULL)
	    tree->columnLockRight = column;
	column = column->next;
    }

    if (move->visible) {
	/* Must update column widths because of expansion. */
	/* Also update columnTreeLeft. */
	tree->widthOfColumns = -1;
	tree->widthOfColumnsLeft = tree->widthOfColumnsRight = -1;
	Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Column_Config --
 *
 *	This procedure is called to process an objc/objv list to set
 *	configuration options for a Column.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then an error message is left in interp's result.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for column;  old resources get freed, if there
 *	were any.  Display changes may occur.
 *
 *----------------------------------------------------------------------
 */

static int
Column_Config(
    TreeColumn column,		/* Column record. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[],	/* Argument values. */
    int createFlag		/* TRUE if the Column is being created. */
    )
{
    TreeCtrl *tree = column->tree;
    TreeColumn_ saved;
    TreeColumn walk;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;
    int mask, maskFree = 0;
    XGCValues gcValues;
    unsigned long gcMask;
    int visible = column->visible;
    int lock = column->lock;
#if COLUMNGRID == 1
    int gridLines, prevGridLines = visible &&
	    (column->gridLeftColor != NULL ||
	    column->gridRightColor != NULL);
	    
#endif

    /* Init these to prevent compiler warnings */
    saved.image = NULL;
    saved.itemBgCount = 0;
    saved.itemBgColor = NULL;


    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) column,
			column->optionTable, objc, objv, tree->tkwin,
			&savedOptions, &mask) != TCL_OK) {
		mask = 0;
		continue;
	    }

	    /* Wouldn't have to do this if Tk_InitOptions() would return
	     * a mask of configured options like Tk_SetOptions() does. */
	    if (createFlag) {
		if (column->imageString != NULL)
		    mask |= COLU_CONF_IMAGE;
		if (column->itemBgObj != NULL)
		    mask |= COLU_CONF_ITEMBG;
	    }

	    /*
	     * Step 1: Save old values
	     */

	    if (mask & COLU_CONF_IMAGE)
		saved.image = column->image;
	    if (mask & COLU_CONF_ITEMBG) {
		saved.itemBgColor = column->itemBgColor;
		saved.itemBgCount = column->itemBgCount;
	    }

	    if (column == tree->columnTail) {
		if (column->itemStyle != NULL) {
		    FormatResult(tree->interp,
			    "can't change the -itemstyle option of the tail column");
		    continue;
		}
		if (column->lock != COLUMN_LOCK_NONE) {
		    FormatResult(tree->interp,
			    "can't change the -lock option of the tail column");
		    continue;
		}
	    }

	    /*
	     * Step 2: Process new values
	     */

	    if (mask & COLU_CONF_IMAGE) {
		if (column->imageString == NULL) {
		    column->image = NULL;
		} else {
		    column->image = Tk_GetImage(tree->interp, tree->tkwin,
			    column->imageString, ImageChangedProc,
			    (ClientData) column);
		    if (column->image == NULL)
			continue;
		    maskFree |= COLU_CONF_IMAGE;
		}
	    }

	    if (mask & COLU_CONF_ITEMBG) {
		if (column->itemBgObj == NULL) {
		    column->itemBgColor = NULL;
		    column->itemBgCount = 0;
		} else {
		    int i, length, listObjc;
		    Tcl_Obj **listObjv;
		    TreeColor **colors;

		    if (Tcl_ListObjGetElements(tree->interp, column->itemBgObj,
				&listObjc, &listObjv) != TCL_OK)
			continue;
		    colors = (TreeColor **) ckalloc(sizeof(TreeColor *) * listObjc);
		    for (i = 0; i < listObjc; i++)
			colors[i] = NULL;
		    for (i = 0; i < listObjc; i++) {
			/* Can specify "" for tree background */
			(void) Tcl_GetStringFromObj(listObjv[i], &length);
			if (length != 0) {
			    colors[i] = Tree_AllocColorFromObj(tree, listObjv[i]);
			    if (colors[i] == NULL)
				break;
			}
		    }
		    if (i < listObjc) {
			Column_FreeColors(column, colors, listObjc);
			continue;
		    }
		    column->itemBgColor = colors;
		    column->itemBgCount = listObjc;
		    maskFree |= COLU_CONF_ITEMBG;
		}
	    }

	    /*
	     * Step 3: Free saved values
	     */

	    if (mask & COLU_CONF_IMAGE) {
		if (saved.image != NULL)
		    Tk_FreeImage(saved.image);
	    }
	    if (mask & COLU_CONF_ITEMBG)
		Column_FreeColors(column, saved.itemBgColor, saved.itemBgCount);
	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    /*
	     * Free new values.
	     */
	    if (maskFree & COLU_CONF_IMAGE)
		Tk_FreeImage(column->image);
	    if (maskFree & COLU_CONF_ITEMBG)
		Column_FreeColors(column, column->itemBgColor, column->itemBgCount);

	    /*
	     * Restore old values.
	     */
	    if (mask & COLU_CONF_IMAGE)
		column->image = saved.image;
	    if (mask & COLU_CONF_ITEMBG) {
		column->itemBgColor = saved.itemBgColor;
		column->itemBgCount = saved.itemBgCount;
	    }

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    /* Indicate that all items must recalculate their list of spans. */
    if (visible != column->visible || lock != column->lock)
	TreeItem_SpansInvalidate(tree, NULL);

    /* Wouldn't have to do this if Tk_InitOptions() would return
    * a mask of configured options like Tk_SetOptions() does. */
    if (createFlag) {
	if (column->textObj != NULL)
	    mask |= COLU_CONF_TEXT;
	if (column->bitmap != None)
	    mask |= COLU_CONF_BITMAP;
    }

    if (mask & COLU_CONF_TEXT) {
	if (column->textObj != NULL)
	    (void) Tcl_GetStringFromObj(column->textObj, &column->textLen);
	else
	    column->textLen = 0;
	if (column->textLen) {
	    Tk_Font tkfont = column->tkfont ? column->tkfont : tree->tkfont;
	    column->textWidth = Tk_TextWidth(tkfont, column->text, column->textLen);
	} else
	    column->textWidth = 0;
    }

    if (mask & COLU_CONF_BITMAP) {
	if (column->bitmapGC != None) {
	    Tk_FreeGC(tree->display, column->bitmapGC);
	    column->bitmapGC = None;
	}
	if (column->bitmap != None) {
	    gcValues.clip_mask = column->bitmap;
	    gcValues.graphics_exposures = False;
	    gcMask = GCClipMask | GCGraphicsExposures;
	    column->bitmapGC = Tk_GetGC(tree->tkwin, gcMask, &gcValues);
	}
    }

    if (mask & COLU_CONF_ITEMBG) {
	if (!createFlag) {
	    /* Set max -itembackground */
	    tree->columnBgCnt = 0;
	    walk = tree->columns;
	    while (walk != NULL) {
		if (walk->visible) {
		    if (walk->itemBgCount > tree->columnBgCnt)
			tree->columnBgCnt = walk->itemBgCount;
		}
		walk = walk->next;
	    }
	}
	Tree_DInfoChanged(tree, DINFO_INVALIDATE); /* implicit DINFO_DRAW_WHITESPACE */
    }

    if (!createFlag && (column->lock != lock)) {
	TreeColumn before = NULL;
	switch (column->lock) {
	    case COLUMN_LOCK_LEFT:
		before = tree->columnLockNone;
		if (before == NULL)
		    before = tree->columnLockRight;
		break;
	    case COLUMN_LOCK_NONE:
		if (lock == COLUMN_LOCK_LEFT) {
		    before = tree->columnLockNone;
		    if (before == NULL)
			before = tree->columnLockRight;
		} else
		    before = tree->columnLockRight;
		break;
	    case COLUMN_LOCK_RIGHT:
		before = NULL;
		break;
	}
	if (before == NULL)
	    before = tree->columnTail;
	Column_Move(column, before);
	Tree_DInfoChanged(tree, DINFO_REDO_COLUMN_WIDTH);
    }

    if (mask & (COLU_CONF_NWIDTH | COLU_CONF_TWIDTH))
	mask |= COLU_CONF_NHEIGHT;
    if (mask & (COLU_CONF_JUSTIFY | COLU_CONF_TEXT))
	column->textLayoutInvalid = TRUE;

    if (mask & COLU_CONF_NWIDTH)
	column->neededWidth = -1;
    if (mask & COLU_CONF_NHEIGHT) {
	column->neededHeight = -1;
	tree->headerHeight = -1;
    }

    /* FIXME: only this column needs to be redisplayed. */
    if (mask & COLU_CONF_JUSTIFY)
	Tree_DInfoChanged(tree, DINFO_INVALIDATE);

    /* -stepwidth and -widthhack */
    if (mask & COLU_CONF_RANGES)
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

    /* Redraw everything */
    if (mask & (COLU_CONF_TWIDTH | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT)) {
	tree->widthOfColumns = -1;
	tree->widthOfColumnsLeft = tree->widthOfColumnsRight = -1;
	Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH | DINFO_DRAW_HEADER);
    }

    /* Redraw header only */
    else if (mask & COLU_CONF_DISPLAY) {
	Tree_DInfoChanged(tree, DINFO_DRAW_HEADER);
    }

#if COLUMNGRID == 1
    if (mask & COLU_CONF_GRIDLINES) {
	Tree_DInfoChanged(tree, DINFO_INVALIDATE | DINFO_DRAW_WHITESPACE);
    }

    gridLines = column->visible &&
	    (column->gridLeftColor != NULL ||
	    column->gridRightColor != NULL);
    if (gridLines != prevGridLines) {
	tree->columnsWithGridLines += gridLines ? 1 : -1;
/*dbwin("tree->columnsWithGridLines is now %d", tree->columnsWithGridLines);*/
    }
#endif

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_Alloc --
 *
 *	Allocate and initialize a new Column record.
 *
 * Results:
 *	Pointer to the new Column, or NULL if errors occurred.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static TreeColumn
Column_Alloc(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeColumn column;

    column = (TreeColumn) ckalloc(sizeof(TreeColumn_));
    memset(column, '\0', sizeof(TreeColumn_));
    column->tree = tree;
    column->optionTable = Tk_CreateOptionTable(tree->interp, columnSpecs);
    column->itemJustify = -1;
    if (Tk_InitOptions(tree->interp, (char *) column, column->optionTable,
		tree->tkwin) != TCL_OK) {
	WFREE(column, TreeColumn_);
	return NULL;
    }
#if 0
    if (Tk_SetOptions(header->tree->interp, (char *) column,
		column->optionTable, 0,
		NULL, header->tree->tkwin, &savedOptions,
		(int *) NULL) != TCL_OK) {
	WFREE(column, TreeColumn_);
	return NULL;
    }
#endif
    column->neededWidth = column->neededHeight = -1;
    tree->headerHeight = tree->widthOfColumns = -1;
    tree->widthOfColumnsLeft = tree->widthOfColumnsRight = -1;
    column->id = tree->nextColumnId++;
    tree->columnCount++;
    return column;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_Free --
 *
 *	Free a Column.
 *
 * Results:
 *	Pointer to the next column.
 *
 * Side effects:
 *	Memory is deallocated. If this is the last column being
 *	deleted, the TreeCtrl.nextColumnId field is reset to zero.
 *
 *----------------------------------------------------------------------
 */

static TreeColumn
Column_Free(
    TreeColumn column		/* Column record. */
    )
{
    TreeCtrl *tree = column->tree;
    TreeColumn next = column->next;

    Column_FreeColors(column, column->itemBgColor, column->itemBgCount);
    if (column->bitmapGC != None)
	Tk_FreeGC(tree->display, column->bitmapGC);
    if (column->image != NULL)
	Tk_FreeImage(column->image);
    if (column->textLayout != NULL)
	TextLayout_Free(column->textLayout);
    TreeDisplay_FreeColumnDInfo(tree, column);
    Tk_FreeConfigOptions((char *) column, column->optionTable, tree->tkwin);
    WFREE(column, TreeColumn_);
    tree->columnCount--;
    if (tree->columnCount == 0)
	tree->nextColumnId = 0;
    return next;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_SetDInfo --
 *
 *	Store a display-info token in a column. Called by the display
 *	code.
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
TreeColumn_SetDInfo(
    TreeColumn column,		/* Column record. */
    TreeColumnDInfo dInfo	/* Display info token. */
    )
{
    column->dInfo = dInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_GetDInfo --
 *
 *	Return the display-info token of a column. Called by the display
 *	code.
 *
 * Results:
 *	The display-info token or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeColumnDInfo
TreeColumn_GetDInfo(
    TreeColumn column		/* Column record. */
    )
{
    return column->dInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_FixedWidth --
 *
 *	Return the value of the -width option.
 *
 * Results:
 *	The pixel width or -1 if the -width option is unspecified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_FixedWidth(
    TreeColumn column		/* Column token. */
    )
{
    return column->widthObj ? column->width : -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_MinWidth --
 *
 *	Return the value of the -minwidth option.
 *
 * Results:
 *	The pixel width or -1 if the -minwidth option is unspecified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_MinWidth(
    TreeColumn column		/* Column token. */
    )
{
    return column->minWidthObj ? column->minWidth : -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_MaxWidth --
 *
 *	Return the value of the -maxwidth option.
 *
 * Results:
 *	The pixel width or -1 if the -maxwidth option is unspecified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_MaxWidth(
    TreeColumn column		/* Column token. */
    )
{
    return column->maxWidthObj ? column->maxWidth : -1;
}

#ifdef DEPRECATED
/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_StepWidth --
 *
 *	Return the value of the -stepwidth option.
 *	NOTE: -stepwidth is deprecated.
 *
 * Results:
 *	The pixel width or -1 if the -stepwidth option is unspecified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_StepWidth(
    TreeColumn column		/* Column token. */
    )
{
    return column->stepWidthObj ? column->stepWidth : -1;
}
#endif /* DEPRECATED */

/*
 *----------------------------------------------------------------------
 *
 * Column_UpdateTextLayout --
 *
 *	Recalculate the TextLayout for the text displayed in the
 *	column header. The old TextLayout (if any) is freed. If
 *	there is no text or if it is only one line then no TextLayout
 *	is created.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory may be allocated/deallocated.
 *
 *----------------------------------------------------------------------
 */

static void
Column_UpdateTextLayout(
    TreeColumn column,		/* Column record. */
    int width			/* Maximum line length. Zero means there
				 * is no limit. */
    )
{
    Tk_Font tkfont;
    char *text = column->text;
    int textLen = column->textLen;
    int justify = column->justify;
    int maxLines = MAX(column->textLines, 0); /* -textlines */
    int wrap = TEXT_WRAP_WORD; /* -textwrap */
    int flags = 0;
    int i, multiLine = FALSE;

    if (column->textLayout != NULL) {
	TextLayout_Free(column->textLayout);
	column->textLayout = NULL;
    }

    if ((text == NULL) || (textLen == 0))
	return;

    for (i = 0; i < textLen; i++) {
	if ((text[i] == '\n') || (text[i] == '\r')) {
	    multiLine = TRUE;
	    break;
	}
    }

#ifdef MAC_OSX_TK
    /* The height of the header is fixed on Aqua. There is only room for
     * a single line of text. */
    if (column->tree->useTheme)
	maxLines = 1;
#endif

    if (!multiLine && ((maxLines == 1) || (!width || (width >= column->textWidth))))
	return;

    tkfont = column->tkfont ? column->tkfont : column->tree->tkfont;

    if (wrap == TEXT_WRAP_WORD)
	flags |= TK_WHOLE_WORDS;

    column->textLayout = TextLayout_Compute(tkfont, text,
	    Tcl_NumUtfChars(text, textLen), width, justify, maxLines,
	    0, 0, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * Column_GetArrowSize --
 *
 *	Return the size of the sort arrow displayed in the column header
 *	for the column's current state.
 *
 * Results:
 *	Height and width of the arrow.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Column_GetArrowSize(
    TreeColumn column,		/* Column record. */
    int *widthPtr,		/* Returned width. */
    int *heightPtr		/* Returned height. */
    )
{
    TreeCtrl *tree = column->tree;
    int state = Column_MakeState(column);
    int arrowWidth = -1, arrowHeight;
    Tk_Image image;
    Pixmap bitmap;

    /* image > bitmap > theme > draw */
    image = PerStateImage_ForState(tree, &column->arrowImage,
	state, NULL);
    if (image != NULL) {
	Tk_SizeOfImage(image, &arrowWidth, &arrowHeight);
    }
    if (arrowWidth == -1) {
	bitmap = PerStateBitmap_ForState(tree, &column->arrowBitmap,
	    state, NULL);
	if (bitmap != None) {
	    Tk_SizeOfBitmap(tree->display, bitmap, &arrowWidth, &arrowHeight);
	}
    }
    if ((arrowWidth == -1) && tree->useTheme &&
	TreeTheme_GetArrowSize(tree, Tk_WindowId(tree->tkwin),
	column->arrow == COLUMN_ARROW_UP, &arrowWidth, &arrowHeight) == TCL_OK) {
    }
    if (arrowWidth == -1) {
	Tk_Font tkfont = column->tkfont ? column->tkfont : tree->tkfont;
	Tk_FontMetrics fm;
	Tk_GetFontMetrics(tkfont, &fm);
	arrowWidth = (fm.linespace + column->textPadY[PAD_TOP_LEFT] +
	    column->textPadY[PAD_BOTTOM_RIGHT] + column->borderWidth * 2) / 2;
	if (!(arrowWidth & 1))
	    arrowWidth--;
	arrowHeight = arrowWidth;
    }

    (*widthPtr) = arrowWidth;
    (*heightPtr) = arrowHeight;
}

/*
 * The following structure holds size/position info for all the graphical
 * elements of a column header.
 */
struct Layout
{
    Tk_Font tkfont;
    Tk_FontMetrics fm;
    int width; /* Provided by caller */
    int height; /* Provided by caller */
    int textLeft;
    int textWidth;
    int bytesThatFit;
    int imageLeft;
    int imageWidth;
    int arrowLeft;
    int arrowWidth;
    int arrowHeight;
};

/*
 * The following structure is used by the Column_DoLayout() procedure to
 * hold size/position info for each graphical element displayed in the
 * header.
 */
struct LayoutPart
{
    int padX[2];
    int padY[2];
    int width;
    int height;
    int left;
    int top;
};

/*
 *----------------------------------------------------------------------
 *
 * Column_DoLayout --
 *
 *	Arrange all the graphical elements making up a column header.
 *
 * Results:
 *	Layout info is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Column_DoLayout(
    TreeColumn column,		/* Column record. */
    struct Layout *layout	/* Returned layout info. The width and
				 * height fields must be initialized. */
    )
{
#if defined(MAC_OSX_TK)
    TreeCtrl *tree = column->tree;
#endif
    struct LayoutPart *parts[3];
    struct LayoutPart partArrow, partImage, partText;
    int i, padList[4], widthList[3], n = 0;
    int iArrow = -1, iImage = -1, iText = -1;
    int left, right;
    int widthForText = 0;
    int arrowSide = column->arrowSide;
    int arrowGravity = column->arrowGravity;
#if defined(MAC_OSX_TK)
    int margins[4];
#endif

#if defined(MAC_OSX_TK)
    /* Under Aqua, we let the Appearance Manager draw the sort arrow. */
    if (tree->useTheme) {
	arrowSide = SIDE_RIGHT;
	arrowGravity = SIDE_RIGHT;
    }
#endif

    padList[0] = 0;

#if defined(MAC_OSX_TK)
    if (tree->useTheme && (column->arrow != COLUMN_ARROW_NONE)) {
	if (TreeTheme_GetHeaderContentMargins(tree, column->state,
		column->arrow, margins) == TCL_OK) {
	    partArrow.width = margins[2];
	} else {
	    partArrow.width = 12;
	}
	/* NOTE: -arrowpadx[1] and -arrowpady ignored. */
	partArrow.padX[PAD_TOP_LEFT] = column->arrowPadX[PAD_TOP_LEFT];
	partArrow.padX[PAD_BOTTOM_RIGHT] = 0;
	partArrow.padY[PAD_TOP_LEFT] = partArrow.padY[PAD_BOTTOM_RIGHT] = 0;
	partArrow.height = 1; /* bogus value */
    }
    else
#endif
    if (column->arrow != COLUMN_ARROW_NONE) {
	Column_GetArrowSize(column, &partArrow.width, &partArrow.height);
	partArrow.padX[PAD_TOP_LEFT] = column->arrowPadX[PAD_TOP_LEFT];
	partArrow.padX[PAD_BOTTOM_RIGHT] = column->arrowPadX[PAD_BOTTOM_RIGHT];
	partArrow.padY[PAD_TOP_LEFT] = column->arrowPadY[PAD_TOP_LEFT];
	partArrow.padY[PAD_BOTTOM_RIGHT] = column->arrowPadY[PAD_BOTTOM_RIGHT];
    }
    if ((column->arrow != COLUMN_ARROW_NONE) && (arrowSide == SIDE_LEFT)) {
	parts[n] = &partArrow;
	padList[n] = partArrow.padX[PAD_TOP_LEFT];
	padList[n + 1] = partArrow.padX[PAD_BOTTOM_RIGHT];
	iArrow = n++;
    }
    if ((column->image != NULL) || (column->bitmap != None)) {
	if (column->image != NULL)
	    Tk_SizeOfImage(column->image, &partImage.width, &partImage.height);
	else
	    Tk_SizeOfBitmap(column->tree->display, column->bitmap, &partImage.width, &partImage.height);
	partImage.padX[PAD_TOP_LEFT] = column->imagePadX[PAD_TOP_LEFT];
	partImage.padX[PAD_BOTTOM_RIGHT] = column->imagePadX[PAD_BOTTOM_RIGHT];
	partImage.padY[PAD_TOP_LEFT] = column->imagePadY[PAD_TOP_LEFT];
	partImage.padY[PAD_BOTTOM_RIGHT] = column->imagePadY[PAD_BOTTOM_RIGHT];
	parts[n] = &partImage;
	padList[n] = MAX(partImage.padX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = partImage.padX[PAD_BOTTOM_RIGHT];
	iImage = n++;
    }
    if (column->textLen > 0) {
	struct LayoutPart *parts2[3];
	int n2 = 0;

	partText.padX[PAD_TOP_LEFT] = column->textPadX[PAD_TOP_LEFT];
	partText.padX[PAD_BOTTOM_RIGHT] = column->textPadX[PAD_BOTTOM_RIGHT];
	partText.padY[PAD_TOP_LEFT] = column->textPadY[PAD_TOP_LEFT];
	partText.padY[PAD_BOTTOM_RIGHT] = column->textPadY[PAD_BOTTOM_RIGHT];

	/* Calculate space for the text */
	if (iArrow != -1)
	    parts2[n2++] = &partArrow;
	if (iImage != -1)
	    parts2[n2++] = &partImage;
	parts2[n2++] = &partText;
	if ((column->arrow != COLUMN_ARROW_NONE) && (arrowSide == SIDE_RIGHT))
	    parts2[n2++] = &partArrow;
	widthForText = layout->width;
	for (i = 0; i < n2; i++) {
	    if (i)
		widthForText -= MAX(parts2[i]->padX[0], parts2[i-1]->padX[1]);
	    else
		widthForText -= parts2[i]->padX[0];
	    if (parts2[i] != &partText)
		widthForText -= parts2[i]->width;
	}
	widthForText -= parts2[n2-1]->padX[1];
    }
    layout->bytesThatFit = 0;
    if (widthForText > 0) {
	if (column->textLayoutInvalid || (column->textLayoutWidth != widthForText)) {
	    Column_UpdateTextLayout(column, widthForText);
	    column->textLayoutInvalid = FALSE;
	    column->textLayoutWidth = widthForText;
	}
	if (column->textLayout != NULL) {
	    TextLayout_Size(column->textLayout, &partText.width, &partText.height);
	    parts[n] = &partText;
	    padList[n] = MAX(partText.padX[PAD_TOP_LEFT], padList[n]);
	    padList[n + 1] = partText.padX[PAD_BOTTOM_RIGHT];
	    iText = n++;
	} else {
	    layout->tkfont = column->tkfont ? column->tkfont : column->tree->tkfont;
	    Tk_GetFontMetrics(layout->tkfont, &layout->fm);
	    if (widthForText >= column->textWidth) {
		partText.width = column->textWidth;
		partText.height = layout->fm.linespace;
		layout->bytesThatFit = column->textLen;
	    } else {
		partText.width = widthForText;
		partText.height = layout->fm.linespace;
		layout->bytesThatFit = Tree_Ellipsis(layout->tkfont,
			column->text, column->textLen, &widthForText,
			"...", FALSE);
	    }
	    parts[n] = &partText;
	    padList[n] = MAX(partText.padX[PAD_TOP_LEFT], padList[n]);
	    padList[n + 1] = partText.padX[PAD_BOTTOM_RIGHT];
	    iText = n++;
	}
    }
    if ((column->arrow != COLUMN_ARROW_NONE) && (arrowSide == SIDE_RIGHT)) {
	parts[n] = &partArrow;
	padList[n] = MAX(partArrow.padX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = partArrow.padX[PAD_BOTTOM_RIGHT];
	iArrow = n++;
    }

    if (n == 0)
	return;

    for (i = 0; i < n; i++) {
	padList[i] = parts[i]->padX[0];
	if (i)
	    padList[i] = MAX(padList[i], parts[i-1]->padX[1]);
	padList[i + 1] = parts[i]->padX[1];
	widthList[i] = parts[i]->width;
    }
    if (iText != -1) {
	switch (column->justify) {
	    case TK_JUSTIFY_LEFT:
		partText.left = 0;
		break;
	    case TK_JUSTIFY_RIGHT:
		partText.left = layout->width;
		break;
	    case TK_JUSTIFY_CENTER:
		if (iImage == -1)
		    partText.left = (layout->width - partText.width) / 2;
		else
		    partText.left = (layout->width - partImage.width -
			    padList[iText] - partText.width) / 2 + partImage.width +
			padList[iText];
		break;
	}
    }

    if (iImage != -1) {
	switch (column->justify) {
	    case TK_JUSTIFY_LEFT:
		partImage.left = 0;
		break;
	    case TK_JUSTIFY_RIGHT:
		partImage.left = layout->width;
		break;
	    case TK_JUSTIFY_CENTER:
		if (iText == -1)
		    partImage.left = (layout->width - partImage.width) / 2;
		else
		    partImage.left = (layout->width - partImage.width -
			    padList[iText] - partText.width) / 2;
		break;
	}
    }

    if (iArrow == -1)
	goto finish;

    switch (column->justify) {
	case TK_JUSTIFY_LEFT:
	    switch (arrowSide) {
		case SIDE_LEFT:
		    partArrow.left = 0;
		    break;
		case SIDE_RIGHT:
		    switch (arrowGravity) {
			case SIDE_LEFT:
			    partArrow.left = 0;
			    break;
			case SIDE_RIGHT:
			    partArrow.left = layout->width;
			    break;
		    }
		    break;
	    }
	    break;
	case TK_JUSTIFY_RIGHT:
	    switch (arrowSide) {
		case SIDE_LEFT:
		    switch (arrowGravity) {
			case SIDE_LEFT:
			    partArrow.left = 0;
			    break;
			case SIDE_RIGHT:
			    partArrow.left = layout->width;
			    break;
		    }
		    break;
		case SIDE_RIGHT:
		    partArrow.left = layout->width;
		    break;
	    }
	    break;
	case TK_JUSTIFY_CENTER:
	    switch (arrowSide) {
		case SIDE_LEFT:
		    switch (arrowGravity) {
			case SIDE_LEFT:
			    partArrow.left = 0;
			    break;
			case SIDE_RIGHT:
			    if (n == 3)
				partArrow.left =
				    (layout->width - widthList[1] - padList[2] -
					    widthList[2]) / 2 - padList[1] - widthList[0];
			    else if (n == 2)
				partArrow.left =
				    (layout->width - widthList[1]) / 2 -
				    padList[1] - widthList[0];
			    else
				partArrow.left = layout->width;
			    break;
		    }
		    break;
		case SIDE_RIGHT:
		    switch (arrowGravity) {
			case SIDE_LEFT:
			    if (n == 3)
				partArrow.left =
				    (layout->width - widthList[0] - padList[1] -
					    widthList[1]) / 2 + widthList[0] + padList[1] +
				    widthList[1] + padList[2];
			    else if (n == 2)
				partArrow.left =
				    (layout->width - widthList[0]) / 2 +
				    widthList[0] + padList[1];
			    else
				partArrow.left = 0;
			    break;
			case SIDE_RIGHT:
			    partArrow.left = layout->width;
			    break;
		    }
		    break;
	    }
	    break;
    }

finish:
    right = layout->width - padList[n];
    for (i = n - 1; i >= 0; i--) {
	if (parts[i]->left + parts[i]->width > right)
	    parts[i]->left = right - parts[i]->width;
	right -= parts[i]->width + padList[i];
    }
    left = padList[0];
    for (i = 0; i < n; i++) {
	if (parts[i]->left < left)
	    parts[i]->left = left;
	left += parts[i]->width + padList[i + 1];
    }

    if (iArrow != -1) {
	layout->arrowLeft = partArrow.left;
	layout->arrowWidth = partArrow.width;
	layout->arrowHeight = partArrow.height;
    }
    if (iImage != -1) {
	layout->imageLeft = partImage.left;
	layout->imageWidth = partImage.width;
    }
    if (iText != -1) {
	layout->textLeft = partText.left;
	layout->textWidth = partText.width;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_NeededWidth --
 *
 *	Return the total width requested by all the graphical elements
 *	that make up a column header.  The width is recalculated if it
 *	is marked out-of-date.
 *
 * Results:
 *	The width needed by the current arrangement of the
 *	bitmap/image/text/arrow.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_NeededWidth(
    TreeColumn column		/* Column token. */
    )
{
    TreeCtrl *tree = column->tree;
    int i, widthList[3], padList[4], n = 0;
    int arrowWidth, arrowHeight, arrowPadX[2];
    int arrowSide = column->arrowSide;
#if defined(MAC_OSX_TK)
    int margins[4];
#endif

    if (!tree->showHeader)
	return 0;

    if (column->neededWidth >= 0)
	return column->neededWidth;

    for (i = 0; i < 3; i++) widthList[i] = 0;
    for (i = 0; i < 4; i++) padList[i] = 0;

    for (i = 0; i < 2; i++) arrowPadX[i] = column->arrowPadX[i];

#if defined(MAC_OSX_TK)
    /* Under OSX we let the Appearance Manager draw the sort arrow. This code
     * assumes the arrow is on the right. */
    if (tree->useTheme && (column->arrow != COLUMN_ARROW_NONE)) {
	if (TreeTheme_GetHeaderContentMargins(tree, column->state,
		column->arrow, margins) == TCL_OK) {
	    arrowWidth = margins[2];
	} else {
	    arrowWidth = 12;
	}
	arrowHeight = 1; /* bogus value */
	arrowSide = SIDE_RIGHT;
	arrowPadX[PAD_BOTTOM_RIGHT] = 0;
    }
    else
#endif
    if (column->arrow != COLUMN_ARROW_NONE)
	Column_GetArrowSize(column, &arrowWidth, &arrowHeight);
    if ((column->arrow != COLUMN_ARROW_NONE) && (arrowSide == SIDE_LEFT)) {
	widthList[n] = arrowWidth;
	padList[n] = arrowPadX[PAD_TOP_LEFT];
	padList[n + 1] = arrowPadX[PAD_BOTTOM_RIGHT];
	n++;
    }
    if ((column->image != NULL) || (column->bitmap != None)) {
	int imgWidth, imgHeight;
	if (column->image != NULL)
	    Tk_SizeOfImage(column->image, &imgWidth, &imgHeight);
	else
	    Tk_SizeOfBitmap(tree->display, column->bitmap, &imgWidth, &imgHeight);
	padList[n] = MAX(column->imagePadX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = column->imagePadX[PAD_BOTTOM_RIGHT];
	widthList[n] = imgWidth;
	n++;
    }
    if (column->textLen > 0) {
	padList[n] = MAX(column->textPadX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = column->textPadX[PAD_BOTTOM_RIGHT];
	if (column->textLayoutInvalid || (column->textLayoutWidth != 0)) {
	    Column_UpdateTextLayout(column, 0);
	    column->textLayoutInvalid = FALSE;
	    column->textLayoutWidth = 0;
	}
	if (column->textLayout != NULL)
	    TextLayout_Size(column->textLayout, &widthList[n], NULL);
	else
	    widthList[n] = column->textWidth;
	n++;
    }
    if ((column->arrow != COLUMN_ARROW_NONE) && (arrowSide == SIDE_RIGHT)) {
	widthList[n] = arrowWidth;
	padList[n] = MAX(arrowPadX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = arrowPadX[PAD_BOTTOM_RIGHT];
	n++;
    }

    column->neededWidth = 0;
    for (i = 0; i < n; i++)
	column->neededWidth += widthList[i] + padList[i];
    column->neededWidth += padList[n];

    /* Notice I'm not considering column->borderWidth. */

    return column->neededWidth;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_NeededHeight --
 *
 *	Return the total height requested by all the graphical elements
 *	that make up a column header.  The height is recalculated if it
 *	is marked out-of-date.
 *
 * Results:
 *	The height needed by the current arrangement of the
 *	bitmap/image/text/arrow.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_NeededHeight(
    TreeColumn column		/* Column token. */
    )
{
    TreeCtrl *tree = column->tree;
    int margins[4];

    if (column->neededHeight >= 0)
	return column->neededHeight;

#if defined(MAC_OSX_TK)
    /* List headers are a fixed height on Aqua */
    if (tree->useTheme &&
	(TreeTheme_GetHeaderFixedHeight(tree, &column->neededHeight) == TCL_OK)) {
	return column->neededHeight;
    }
#endif

    column->neededHeight = 0;
    if (column->arrow != COLUMN_ARROW_NONE) {
	int arrowWidth, arrowHeight;
	Column_GetArrowSize(column, &arrowWidth, &arrowHeight);
	arrowHeight += column->arrowPadY[PAD_TOP_LEFT]
	    + column->arrowPadY[PAD_BOTTOM_RIGHT];
	column->neededHeight = MAX(column->neededHeight, arrowHeight);
    }
    if ((column->image != NULL) || (column->bitmap != None)) {
	int imgWidth, imgHeight;
	if (column->image != NULL)
	    Tk_SizeOfImage(column->image, &imgWidth, &imgHeight);
	else
	    Tk_SizeOfBitmap(tree->display, column->bitmap, &imgWidth, &imgHeight);
	imgHeight += column->imagePadY[PAD_TOP_LEFT]
	    + column->imagePadY[PAD_BOTTOM_RIGHT];
	column->neededHeight = MAX(column->neededHeight, imgHeight);
    }
    if (column->text != NULL) {
	struct Layout layout;
	layout.width = TreeColumn_UseWidth(column);
	layout.height = -1;
	Column_DoLayout(column, &layout);
	if (column->textLayout != NULL) {
	    int height;
	    TextLayout_Size(column->textLayout, NULL, &height);
	    height += column->textPadY[PAD_TOP_LEFT]
		+ column->textPadY[PAD_BOTTOM_RIGHT];
	    column->neededHeight = MAX(column->neededHeight, height);
	} else {
	    Tk_Font tkfont = column->tkfont ? column->tkfont : column->tree->tkfont;
	    Tk_FontMetrics fm;
	    Tk_GetFontMetrics(tkfont, &fm);
	    fm.linespace += column->textPadY[PAD_TOP_LEFT]
		+ column->textPadY[PAD_BOTTOM_RIGHT];
	    column->neededHeight = MAX(column->neededHeight, fm.linespace);
	}
    }
    if (column->tree->useTheme &&
	(TreeTheme_GetHeaderContentMargins(tree, column->state,
		column->arrow, margins) == TCL_OK)) {
#ifdef WIN32
	/* I'm hacking these margins since the default XP theme does not give
	 * reasonable ContentMargins for HP_HEADERITEM */
	int bw = MAX(column->borderWidth, 3);
	margins[1] = MAX(margins[1], bw);
	margins[3] = MAX(margins[3], bw);
#endif /* WIN32 */
	column->neededHeight += margins[1] + margins[3];
    } else {
	column->neededHeight += column->borderWidth * 2;
    }

    return column->neededHeight;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_UseWidth --
 *
 *	Return the actual display width of a column.
 *
 * Results:
 *	Pixel width.
 *
 * Side effects:
 *	The size of any column that is marked out-of-date is
 *	recalculated. This could involve recalculating the size of
 *	every element and style in the column in all items.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_UseWidth(
    TreeColumn column		/* Column token. */
    )
{
    /* Update layout if needed */
    (void) Tree_WidthOfColumns(column->tree);

    return column->useWidth;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_Offset --
 *
 *	Return the x-offset of a column.
 *
 * Results:
 *	Pixel offset.
 *
 * Side effects:
 *	Column layout is updated if needed.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_Offset(
    TreeColumn column		/* Column token. */
    )
{
    /* Update layout if needed */
    (void) Tree_WidthOfColumns(column->tree);

    return column->offset;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_ItemJustify --
 *
 *	Return the value of the -itemjustify config option for a column.
 *	If -itemjustify is unspecified, then return the value of the
 *	-justify option.
 *
 * Results:
 *	TK_JUSTIFY_xxx constant.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Justify
TreeColumn_ItemJustify(
    TreeColumn column		/* Column token. */
    )
{
    return (column->itemJustify != -1) ? column->itemJustify : column->justify;
}

#ifdef DEPRECATED
/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_WidthHack --
 *
 *	Return the value of the -widthhack config option for a column.
 *	NOTE: -widthhack is deprecated.
 *
 * Results:
 *	Boolean value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_WidthHack(
    TreeColumn column		/* Column token. */
    )
{
    return column->widthHack;
}
#endif /* DEPRECATED */

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_Squeeze --
 *
 *	Return the value of the -squeeze config option for a column.
 *
 * Results:
 *	Boolean value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_Squeeze(
    TreeColumn column		/* Column token. */
    )
{
    return column->squeeze;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_BackgroundCount --
 *
 *	Return the number of -itembackground colors for a column.
 *
 * Results:
 *	column->itemBgCount.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_BackgroundCount(
    TreeColumn column		/* Column token. */
    )
{
    return column->itemBgCount;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_BackgroundColor --
 *
 *	Return a TreeColor for one color of the -itembackground
 *	config option for a column.
 *
 * Results:
 *	A TreeColor, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeColor *
TreeColumn_BackgroundColor(
    TreeColumn column,		/* Column token. */
    int index			/* This number is determined by the display
				 * code. */
    )
{
    if ((index < 0) || (column->itemBgCount == 0))
	return NULL;
    return column->itemBgColor[index % column->itemBgCount];
}

#if COLUMNGRID == 1
/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_GridColors --
 *
 *	Returns the color and width for this column's gridlines.
 *
 * Results:
 *	Returns the left and right colors and their widths.  Either
 *	color may be NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_GridColors(
    TreeColumn column,		/* Column token. */
    TreeColor **leftColorPtr,	/* Returned left color. */
    TreeColor **rightColorPtr,	/* Returned right color. */
    int *leftWidthPtr,
    int *rightWidthPtr
    )
{
    (*leftColorPtr) = column->gridLeftColor;
    (*rightColorPtr) = column->gridRightColor;
    (*leftWidthPtr) = 1;
    (*rightWidthPtr) = 1;
    return (*leftColorPtr != NULL && *leftWidthPtr > 0) ||
	    (*rightColorPtr != NULL && *rightWidthPtr > 0);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_ItemStyle --
 *
 *	Return the value of the -itemstyle config option for a column.
 *
 * Results:
 *	TreeStyle or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeStyle
TreeColumn_ItemStyle(
    TreeColumn column		/* Column token. */
    )
{
    return column->itemStyle;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_StyleDeleted --
 *
 *	Called when a master style is deleted.
 *
 * Results:
 *	Clear the column's -itemstyle option if it is the style being
 *	deleted.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeColumn_StyleDeleted(
    TreeColumn column,		/* Column token. */
    TreeStyle style		/* Style that was deleted. */
    )
{
    if (column->itemStyle == style)
	column->itemStyle = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_Visible --
 *
 *	Return the value of the -visible config option for a column.
 *
 * Results:
 *	Boolean value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_Visible(
    TreeColumn column		/* Column token. */
    )
{
    return column->visible;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_GetID --
 *
 *	Return the unique identifier for a column.
 *
 * Results:
 *	Unique integer id.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int TreeColumn_GetID(
    TreeColumn column		/* Column token. */
    )
{
    return column->id;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_Lock --
 *
 *	Return the value of the -lock option for a column.
 *
 * Results:
 *	One of the COLUMN_LOCK_xxx constants.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int TreeColumn_Lock(
    TreeColumn column		/* Column token. */
    )
{
    return column->lock;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_Index --
 *
 *	Return the 0-based index for a column.
 *
 * Results:
 *	Position of the column in the list of columns.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_Index(
    TreeColumn column		/* Column token. */
    )
{
    return column->index;
}

/*
 *----------------------------------------------------------------------
 *
 * ColumnTagCmd --
 *
 *	This procedure is invoked to process the [column tag] widget
 *	command.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ColumnTagCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    static CONST char *commandNames[] = {
	"add", "expr", "names", "remove", (char *) NULL
    };
    enum {
	COMMAND_ADD, COMMAND_EXPR, COMMAND_NAMES, COMMAND_REMOVE
    };
    int index;
    ColumnForEach iter;
    TreeColumnList columns;
    TreeColumn column;
    int result = TCL_OK;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 3, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
	&index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
	/* T column tag add C tagList */
	case COMMAND_ADD: {
	    int i, numTags;
	    Tcl_Obj **listObjv;
	    Tk_Uid staticTags[STATIC_SIZE], *tags = staticTags;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 4, objv, "column tagList");
		return TCL_ERROR;
	    }
	    if (TreeColumnList_FromObj(tree, objv[4], &columns, 0) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (Tcl_ListObjGetElements(interp, objv[5], &numTags, &listObjv) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    STATIC_ALLOC(tags, Tk_Uid, numTags);
	    for (i = 0; i < numTags; i++) {
		tags[i] = Tk_GetUid(Tcl_GetString(listObjv[i]));
	    }
	    COLUMN_FOR_EACH(column, &columns, NULL, &iter) {
		column->tagInfo = TagInfo_Add(tree, column->tagInfo, tags, numTags);
	    }
	    STATIC_FREE(tags, Tk_Uid, numTags);
	    break;
	}

	/* T column tag expr C tagExpr */
	case COMMAND_EXPR: {
	    TagExpr expr;
	    int ok = TRUE;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 4, objv, "column tagExpr");
		return TCL_ERROR;
	    }
	    if (TreeColumnList_FromObj(tree, objv[4], &columns, 0) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (TagExpr_Init(tree, objv[5], &expr) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    COLUMN_FOR_EACH(column, &columns, NULL, &iter) {
		if (!TagExpr_Eval(&expr, column->tagInfo)) {
		    ok = FALSE;
		    break;
		}
	    }
	    TagExpr_Free(&expr);
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(ok));
	    break;
	}

	/* T column tag names C */
	case COMMAND_NAMES: {
	    Tcl_Obj *listObj;
	    Tk_Uid *tags = NULL;
	    int i, tagSpace, numTags = 0;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 4, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumnList_FromObj(tree, objv[4], &columns, 0) != TCL_OK) {
		return TCL_ERROR;
	    }
	    COLUMN_FOR_EACH(column, &columns, NULL, &iter) {
		tags = TagInfo_Names(tree, column->tagInfo, tags, &numTags, &tagSpace);
	    }
	    if (numTags) {
		listObj = Tcl_NewListObj(0, NULL);
		for (i = 0; i < numTags; i++) {
		    Tcl_ListObjAppendElement(NULL, listObj,
			    Tcl_NewStringObj((char *) tags[i], -1));
		}
		Tcl_SetObjResult(interp, listObj);
		ckfree((char *) tags);
	    }
	    break;
	}

	/* T column tag remove C tagList */
	case COMMAND_REMOVE: {
	    int i, numTags;
	    Tcl_Obj **listObjv;
	    Tk_Uid staticTags[STATIC_SIZE], *tags = staticTags;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 4, objv, "column tagList");
		return TCL_ERROR;
	    }
	    if (TreeColumnList_FromObj(tree, objv[4], &columns, 0) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (Tcl_ListObjGetElements(interp, objv[5], &numTags, &listObjv) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    STATIC_ALLOC(tags, Tk_Uid, numTags);
	    for (i = 0; i < numTags; i++) {
		tags[i] = Tk_GetUid(Tcl_GetString(listObjv[i]));
	    }
	    COLUMN_FOR_EACH(column, &columns, NULL, &iter) {
		column->tagInfo = TagInfo_Remove(tree, column->tagInfo, tags, numTags);
	    }
	    STATIC_FREE(tags, Tk_Uid, numTags);
	    break;
	}
    }

    TreeColumnList_Free(&columns);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumnCmd --
 *
 *	This procedure is invoked to process the [column] widget
 *	command.  See the user documentation for details on what it
 *	does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumnCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    static CONST char *commandNames[] = {
	"bbox", "cget", "compare", "configure", "count", "create", "delete",
	"dragcget", "dragconfigure", "id",
#ifdef DEPRECATED
	"index",
#endif
	"list", "move", "neededwidth", "order", "tag", "width", (char *) NULL
    };
    enum {
	COMMAND_BBOX, COMMAND_CGET, COMMAND_COMPARE, COMMAND_CONFIGURE,
	COMMAND_COUNT, COMMAND_CREATE, COMMAND_DELETE, COMMAND_DRAGCGET,
	COMMAND_DRAGCONF, COMMAND_ID,
#ifdef DEPRECATED
	COMMAND_INDEX,
#endif
	COMMAND_LIST, COMMAND_MOVE, COMMAND_NEEDEDWIDTH, COMMAND_ORDER,
	COMMAND_TAG, COMMAND_WIDTH
    };
    int index;
    TreeColumnList columns;
    TreeColumn column;
    ColumnForEach citer;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandNames, "command", 0,
		&index) != TCL_OK) {
	return TCL_ERROR;
    }

    TreeColumnList_Init(tree, &columns, 0);

    switch (index) {
	case COMMAND_BBOX: {
	    int left, top, width, height;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &column,
			CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
		return TCL_ERROR;
	    if (TreeColumn_Bbox(column, &left, &top, &width, &height) < 0)
		break;
	    FormatResult(interp, "%d %d %d %d",
		    left, top, left + width, top + height);
	    break;
	}

	case COMMAND_CGET: {
	    TreeColumn column;
	    Tcl_Obj *resultObjPtr;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "column option");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &column,
			CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) column,
		    column->optionTable, objv[4], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T column compare C op C */
	case COMMAND_COMPARE: {
	    TreeColumn column1, column2;
	    static CONST char *opName[] = { "<", "<=", "==", ">=", ">", "!=", NULL };
	    int op, compare = 0, index1, index2;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 3, objv, "column1 op column2");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &column1,
		    CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIndexFromObj(interp, objv[4], opName,
		    "comparison operator", 0, &op) != TCL_OK)
		return TCL_ERROR;
	    if (TreeColumn_FromObj(tree, objv[5], &column2,
		    CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    index1 = TreeColumn_Index(column1);
	    index2 = TreeColumn_Index(column2);
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

	case COMMAND_CONFIGURE: {
	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column ?option? ?value? ?option value ...?");
		return TCL_ERROR;
	    }
	    if (objc <= 5) {
		Tcl_Obj *resultObjPtr;
		if (TreeColumn_FromObj(tree, objv[3], &column,
			CFO_NOT_NULL) != TCL_OK)
		    return TCL_ERROR;
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) column,
			column->optionTable,
			(objc == 4) ? (Tcl_Obj *) NULL : objv[4],
			tree->tkwin);
		if (resultObjPtr == NULL)
		    goto errorExit;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    /* If "all" is specified, get a list of columns instead of
	     * COLUMN_ALL, since changing the -lock option of a column
	     * may reorder columns. */
	    if (TreeColumnList_FromObj(tree, objv[3], &columns,
		    CFO_LIST_ALL | CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    COLUMN_FOR_EACH(column, &columns, NULL, &citer) {
		if (Column_Config(column, objc - 4, objv + 4, FALSE) != TCL_OK)
		    goto errorExit;
	    }
	    break;
	}

	case COMMAND_CREATE: {
	    TreeColumn column, last = tree->columnLast;

	    /* FIXME: -count N -tags $tags */
	    column = Column_Alloc(tree);
	    if (Column_Config(column, objc - 3, objv + 3, TRUE) != TCL_OK) {
		Column_Free(column);
		return TCL_ERROR;
	    }

	    if (tree->columns == NULL) {
		column->index = 0;
		tree->columns = column;
	    } else {
		last->next = column;
		column->prev = last;
		column->index = last->index + 1;
	    }
	    tree->columnLast = column;
	    tree->columnTail->index++;

	    {
		TreeColumn before = NULL;
		switch (column->lock) {
		    case COLUMN_LOCK_LEFT:
			before = tree->columnLockNone;
			if (before == NULL)
			    before = tree->columnLockRight;
			break;
		    case COLUMN_LOCK_NONE:
			before = tree->columnLockRight;
			break;
		    case COLUMN_LOCK_RIGHT:
			before = NULL;
			break;
		}
		if (before == NULL)
		    before = tree->columnTail;
		Column_Move(column, before);
	    }

	    /* Indicate that all items must recalculate their list of spans. */
	    TreeItem_SpansInvalidate(tree, NULL);

	    Tree_DInfoChanged(tree, DINFO_REDO_COLUMN_WIDTH);
	    Tcl_SetObjResult(interp, TreeColumn_ToObj(tree, column));
	    break;
	}

	/* T column delete first ?last? */
	case COMMAND_DELETE: {
	    TreeColumnList column2s;
	    TreeColumn prev, next;
	    int flags = CFO_NOT_NULL | CFO_NOT_TAIL;
	    TreeItem item;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;
	    int index;

	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "first ?last?");
		return TCL_ERROR;
	    }
	    if (objc == 5)
		flags |= CFO_NOT_MANY;
	    if (TreeColumnList_FromObj(tree, objv[3], &columns,
		    flags) != TCL_OK)
		goto errorExit;
	    if (objc == 5) {
		if (TreeColumnList_FromObj(tree, objv[4], &column2s,
			CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
		    goto errorExit;
	    }
	    COLUMN_FOR_EACH(column, &columns, (objc == 5) ? &column2s : NULL,
		    &citer) {
		/* T column delete "all" */
		if (citer.all) {
		    column = tree->columns;
		    while (column != NULL) {
			TreeDisplay_ColumnDeleted(tree, column);
			TreeGradient_ColumnDeleted(tree, column);
#if COLUMNGRID == 1
			if (column->visible &&
				(column->gridLeftColor != NULL ||
				column->gridRightColor != NULL)) {
			    tree->columnsWithGridLines -= 1;
			}
#endif
			column = Column_Free(column);
		    }
		    tree->columnTail->index = 0;
		    tree->columns = NULL;
		    tree->columnLast = NULL;
		    tree->columnLockLeft = NULL;
		    tree->columnLockNone = NULL;
		    tree->columnLockRight = NULL;

		    /* Delete all TreeItemColumns */
		    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		    while (hPtr != NULL) {
			item = (TreeItem) Tcl_GetHashValue(hPtr);
			TreeItem_RemoveAllColumns(tree, item);
			hPtr = Tcl_NextHashEntry(&search);
		    }

		    tree->columnTree = NULL;
		    tree->columnDrag.column = tree->columnDrag.indColumn = NULL;
		    tree->widthOfColumns = tree->headerHeight = -1;
		    tree->widthOfColumnsLeft = tree->widthOfColumnsRight = -1;
		    Tree_DInfoChanged(tree, DINFO_REDO_COLUMN_WIDTH);
		    goto doneDELETE;
		}

		/* Delete all TreeItemColumns */
		hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		while (hPtr != NULL) {
		    item = (TreeItem) Tcl_GetHashValue(hPtr);
		    TreeItem_RemoveColumns(tree, item, column->index,
			    column->index);
		    hPtr = Tcl_NextHashEntry(&search);
		}

		TreeDisplay_ColumnDeleted(tree, column);
		TreeGradient_ColumnDeleted(tree, column);
#if COLUMNGRID == 1
		if (column->visible &&
			(column->gridLeftColor != NULL ||
			column->gridRightColor != NULL)) {
		    tree->columnsWithGridLines -= 1;
/*dbwin("tree->columnsWithGridLines is now %d", tree->columnsWithGridLines);*/
		}
#endif

		/* Unlink. */
		prev = column->prev;
		next = column->next;
		if (prev == NULL)
		    tree->columns = next;
		else
		    prev->next = next;
		if (next == NULL)
		    tree->columnLast = prev;
		else
		    next->prev = prev;

		if (column == tree->columnTree)
		    tree->columnTree = NULL;
		if (column == tree->columnDrag.column)
		    tree->columnDrag.column = NULL;
		if (column == tree->columnDrag.indColumn)
		    tree->columnDrag.indColumn = NULL;

		(void) Column_Free(column);

		/* Renumber trailing columns */
		column = next;
		while (column != NULL) {
		    column->index--;
		    column = column->next;
		}
	    }

	    tree->columnLockLeft = NULL;
	    tree->columnLockNone = NULL;
	    tree->columnLockRight = NULL;

	    index = 0;
	    column = tree->columns;
	    while (column != NULL) {
		column->index = index++;
		if (column->lock == COLUMN_LOCK_LEFT && tree->columnLockLeft == NULL)
		    tree->columnLockLeft = column;
		if (column->lock == COLUMN_LOCK_NONE && tree->columnLockNone == NULL)
		    tree->columnLockNone = column;
		if (column->lock == COLUMN_LOCK_RIGHT && tree->columnLockRight == NULL)
		    tree->columnLockRight = column;
		column = column->next;
	    }
	    tree->columnTail->index = index;

	    tree->widthOfColumns = tree->headerHeight = -1;
	    tree->widthOfColumnsLeft = tree->widthOfColumnsRight = -1;
	    Tree_DInfoChanged(tree, DINFO_REDO_COLUMN_WIDTH);

doneDELETE:
	    /* Indicate that all items must recalculate their list of spans. */
	    TreeItem_SpansInvalidate(tree, NULL);

	    if (objc == 5)
		TreeColumnList_Free(&column2s);
	    break;
	}

	/* T column dragcget option */
	case COMMAND_DRAGCGET: {
	    Tcl_Obj *resultObjPtr;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "option");
		return TCL_ERROR;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) tree,
		    tree->columnDrag.optionTable, objv[3], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T column dragconfigure ?option? ?value? ?option value ...? */
	case COMMAND_DRAGCONF: {
	    Tcl_Obj *resultObjPtr;
	    Tk_SavedOptions savedOptions;
	    int mask, result;

	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 3, objv, "?option? ?value?");
		return TCL_ERROR;
	    }
	    if (objc <= 4) {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) tree,
			tree->columnDrag.optionTable,
			(objc == 3) ? (Tcl_Obj *) NULL : objv[3],
			tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    result = Tk_SetOptions(interp, (char *) tree,
		    tree->columnDrag.optionTable, objc - 3, objv + 3, tree->tkwin,
		    &savedOptions, &mask);
	    if (result != TCL_OK) {
		Tk_RestoreSavedOptions(&savedOptions);
		return TCL_ERROR;
	    }
	    Tk_FreeSavedOptions(&savedOptions);

	    if (tree->columnDrag.alpha < 0)
		tree->columnDrag.alpha = 0;
	    if (tree->columnDrag.alpha > 255)
		tree->columnDrag.alpha = 255;

	    Tree_DInfoChanged(tree, DINFO_DRAW_HEADER);
	    break;
	}

	case COMMAND_COUNT: {
	    int count = tree->columnCount;

	    if (objc < 3 || objc > 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "?columnDesc?");
		return TCL_ERROR;
	    }
	    if (objc == 4) {
		if (TreeColumnList_FromObj(tree, objv[3], &columns, 0)
			!= TCL_OK)
		    return TCL_ERROR;
		count = 0;
		COLUMN_FOR_EACH(column, &columns, NULL, &citer) {
		    count++;
		}
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(count));
	    break;
	}

	case COMMAND_WIDTH: {
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &column,
			CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;

	    /* Update layout if needed */
	    (void) Tree_WidthOfColumns(tree);
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(column->useWidth));
	    break;
	}

	case COMMAND_ID:
#ifdef DEPRECATED
	case COMMAND_INDEX:
#endif
	{
	    Tcl_Obj *listObj;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumnList_FromObj(tree, objv[3], &columns, 0) != TCL_OK)
		return TCL_ERROR;
	    listObj = Tcl_NewListObj(0, NULL);
	    COLUMN_FOR_EACH(column, &columns, NULL, &citer) {
		Tcl_ListObjAppendElement(interp, listObj,
			TreeColumn_ToObj(tree, column));
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	/* T column list ?-visible? */
	case COMMAND_LIST: {
	    TreeColumn column = tree->columns;
	    Tcl_Obj *listObj;
	    int visible = FALSE;

	    if (objc > 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "?-visible?");
		return TCL_ERROR;
	    }
	    if (objc == 4) {
		int len;
		char *s = Tcl_GetStringFromObj(objv[3], &len);
		if ((s[0] == '-') && (strncmp(s, "-visible", len) == 0))
		    visible = TRUE;
		else {
		    FormatResult(interp, "bad switch \"%s\": must be -visible",
			s);
		    return TCL_ERROR;
		}
	    }
	    listObj = Tcl_NewListObj(0, NULL);
	    while (column != NULL) {
		if (!visible || column->visible)
		    Tcl_ListObjAppendElement(interp, listObj,
				TreeColumn_ToObj(tree, column));
		column = column->next;
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	/* T column move C before */
	case COMMAND_MOVE: {
	    TreeColumn move, before;
	    TreeColumn first = NULL, last = tree->columnTail;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "column before");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &move,
		    CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
		return TCL_ERROR;
	    if (TreeColumn_FromObj(tree, objv[4], &before,
		    CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;

	    if ((move == before) || (move->index == before->index - 1))
		break;
	    switch (move->lock) {
		case COLUMN_LOCK_LEFT:
		    first =  tree->columnLockLeft;
		    if (tree->columnLockNone != NULL)
			last = tree->columnLockNone;
		    else if (tree->columnLockRight != NULL)
			last = tree->columnLockRight;
		    break;
		case COLUMN_LOCK_NONE:
		    first = tree->columnLockNone;
		    if (tree->columnLockRight != NULL)
			last = tree->columnLockRight;
		    break;
		case COLUMN_LOCK_RIGHT:
		    first = tree->columnLockRight;
		    break;
	    }
	    if (before->index < first->index || before->index > last->index) {
		FormatResult(tree->interp,
		    "column %s%d and column %s%d -lock options conflict",
		    tree->columnPrefix, move->id,
		    tree->columnPrefix, before->id);
		return TCL_ERROR;
	    }
	    Column_Move(move, before);

	    /* Indicate that all items must recalculate their list of spans. */
	    TreeItem_SpansInvalidate(tree, NULL);
	    break;
	}

	case COMMAND_NEEDEDWIDTH: {
	    TreeColumn column;
	    int width;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &column,
			CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;

	    /* Update layout if needed */
	    (void) Tree_CanvasWidth(tree);
	    width = TreeColumn_WidthOfItems(column);
	    width = MAX(width, TreeColumn_NeededWidth(column));
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(width));
	    break;
	}

	/* T column order C ?-visible? */
	case COMMAND_ORDER: {
	    TreeColumn column;
	    int visible = FALSE;
	    int index = 0;

	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "column ?-visible?");
		return TCL_ERROR;
	    }
	    if (objc == 5) {
		int len;
		char *s = Tcl_GetStringFromObj(objv[4], &len);
		if ((s[0] == '-') && (strncmp(s, "-visible", len) == 0))
		    visible = TRUE;
		else {
		    FormatResult(interp, "bad switch \"%s\": must be -visible",
			s);
		    return TCL_ERROR;
		}
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &column,
		    CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    if (visible) {
		TreeColumn walk = tree->columns;
		while (walk != NULL) {
		    if (walk == column)
			break;
		    if (walk->visible)
			index++;
		    walk = walk->next;
		}
		if (!column->visible)
		    index = -1;
	    } else {
		index = column->index;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(index));
	    break;
	}

	case COMMAND_TAG: {
	    return ColumnTagCmd(clientData, interp, objc, objv);
	}
    }

    TreeColumnList_Free(&columns);
    return TCL_OK;

errorExit:
    TreeColumnList_Free(&columns);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_DrawArrow --
 *
 *	Draw the sort arrow for a column.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in a drawable.
 *
 *----------------------------------------------------------------------
 */

static void
Column_DrawArrow(
    TreeColumn column,		/* Column record. */
    TreeDrawable td,		/* Where to draw. */
    int x, int y,		/* Top-left corner of the column's header. */
    struct Layout layout	/* Size/position info. */
    )
{
    TreeCtrl *tree = column->tree;
    int height = tree->headerHeight;
    int sunken = column->state == COLUMN_STATE_PRESSED;
    Tk_Image image = NULL;
    Pixmap bitmap;
    Tk_3DBorder border;
    int state = Column_MakeState(column);
    int arrowPadTop = column->arrowPadY[PAD_TOP_LEFT];
    int arrowPadY = arrowPadTop + column->arrowPadY[PAD_BOTTOM_RIGHT];
    int arrowTop = y + (height - (layout.arrowHeight + arrowPadY)) / 2
	+ arrowPadTop;

    if (column->arrow == COLUMN_ARROW_NONE)
	return;

    image = PerStateImage_ForState(tree, &column->arrowImage, state, NULL);
    if (image != NULL) {
	Tree_RedrawImage(image, 0, 0, layout.arrowWidth, layout.arrowHeight,
	    td,
	    x + layout.arrowLeft + sunken,
	    arrowTop + sunken);
	return;
    }

    bitmap = PerStateBitmap_ForState(tree, &column->arrowBitmap, state, NULL);
    if (bitmap != None) {
	int bx, by;
	bx = x + layout.arrowLeft + sunken;
	by = arrowTop + sunken;
	Tree_DrawBitmap(tree, bitmap, td.drawable, NULL, NULL,
		0, 0,
		(unsigned int) layout.arrowWidth, (unsigned int) layout.arrowHeight,
		bx, by);
	return;
    }

    if (tree->useTheme) {
	if (TreeTheme_DrawHeaderArrow(tree, td, column->state,
	    column->arrow == COLUMN_ARROW_UP, x + layout.arrowLeft + sunken,
	    arrowTop + sunken,
	    layout.arrowWidth, layout.arrowHeight) == TCL_OK)
	    return;
    }

    if (1) {
	int arrowWidth = layout.arrowWidth;
	int arrowHeight = layout.arrowHeight;
	int arrowBottom = arrowTop + arrowHeight;
	XPoint points[5];
	int color1 = 0, color2 = 0;
	int i;

	switch (column->arrow) {
	    case COLUMN_ARROW_UP:
		points[0].x = x + layout.arrowLeft;
		points[0].y = arrowBottom - 1;
		points[1].x = x + layout.arrowLeft + arrowWidth / 2;
		points[1].y = arrowTop - 1;
		color1 = TK_3D_DARK_GC;
		points[4].x = x + layout.arrowLeft + arrowWidth / 2;
		points[4].y = arrowTop - 1;
		points[3].x = x + layout.arrowLeft + arrowWidth - 1;
		points[3].y = arrowBottom - 1;
		points[2].x = x + layout.arrowLeft;
		points[2].y = arrowBottom - 1;
		color2 = TK_3D_LIGHT_GC;
		break;
	    case COLUMN_ARROW_DOWN:
		points[0].x = x + layout.arrowLeft + arrowWidth - 1;
		points[0].y = arrowTop;
		points[1].x = x + layout.arrowLeft + arrowWidth / 2;
		points[1].y = arrowBottom;
		color1 = TK_3D_LIGHT_GC;
		points[2].x = x + layout.arrowLeft + arrowWidth - 1;
		points[2].y = arrowTop;
		points[3].x = x + layout.arrowLeft;
		points[3].y = arrowTop;
		points[4].x = x + layout.arrowLeft + arrowWidth / 2;
		points[4].y = arrowBottom;
		color2 = TK_3D_DARK_GC;
		break;
	}
	for (i = 0; i < 5; i++) {
	    points[i].x += sunken;
	    points[i].y += sunken;
	}

	border = PerStateBorder_ForState(tree, &column->border, state, NULL);
	if (border == NULL)
	    border = tree->border;
	XDrawLines(tree->display, td.drawable,
		Tk_3DBorderGC(tree->tkwin, border, color2),
		points + 2, 3, CoordModeOrigin);
	XDrawLines(tree->display, td.drawable,
		Tk_3DBorderGC(tree->tkwin, border, color1),
		points, 2, CoordModeOrigin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Column_Draw --
 *
 *	Draw the header for a column.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in a drawable.
 *
 *----------------------------------------------------------------------
 */

static void
Column_Draw(
    TreeColumn column,		/* Column record. */
    TreeDrawable td,		/* Where to draw. */
    int x, int y,		/* Top-left corner of the column's header. */
    int visIndex,		/* 0-based index in the list of visible
				 * columns. */
    int dragImage		/* TRUE if we are creating a transparent
				 * drag image for this header. */
    )
{
    TreeCtrl *tree = column->tree;
    int height = tree->headerHeight;
    struct Layout layout;
    int width = column->useWidth;
    int sunken = column->state == COLUMN_STATE_PRESSED;
    int relief = sunken ? TK_RELIEF_SUNKEN : TK_RELIEF_RAISED;
    Tk_3DBorder border;
    int theme = TCL_ERROR;
    GC gc = None;
    TkRegion clipRgn = NULL;

    layout.width = width;
    layout.height = height;
    Column_DoLayout(column, &layout);

    border = PerStateBorder_ForState(tree, &column->border,
	Column_MakeState(column), NULL);
    if (border == NULL)
	border = tree->border;

    if (dragImage) {
	GC gc = Tk_GCForColor(tree->columnDrag.color, Tk_WindowId(tree->tkwin));
	XFillRectangle(tree->display, td.drawable, gc, x, y, width, height);
    } else {
	/* Hack on */
	if (visIndex == 0 && column->lock == COLUMN_LOCK_NONE) {
	    x -= tree->canvasPadX[PAD_TOP_LEFT];
	    width += tree->canvasPadX[PAD_TOP_LEFT];
	}
	if (tree->useTheme) {
	    theme = TreeTheme_DrawHeaderItem(tree, td, column->state,
		    column->arrow, visIndex, x, y, width, height);
	}
	if (theme != TCL_OK) {
	    Tk_Fill3DRectangle(tree->tkwin, td.drawable, border,
		    x, y, width, height, 0, TK_RELIEF_FLAT);
	}
	/* Hack off */
	if (visIndex == 0 && column->lock == COLUMN_LOCK_NONE) {
	    x += tree->canvasPadX[PAD_TOP_LEFT];
	    width -= tree->canvasPadX[PAD_TOP_LEFT];
	}
    }

    if (column->image != NULL) {
	int imgW, imgH, ix, iy, h;
	Tk_SizeOfImage(column->image, &imgW, &imgH);
	ix = x + layout.imageLeft + sunken;
	h = column->imagePadY[PAD_TOP_LEFT] + imgH
	    + column->imagePadY[PAD_BOTTOM_RIGHT];
	iy = y + (height - h) / 2 + sunken;
	iy += column->imagePadY[PAD_TOP_LEFT];
	Tree_RedrawImage(column->image, 0, 0, imgW, imgH, td, ix, iy);
    } else if (column->bitmap != None) {
	int imgW, imgH, bx, by, h;

	Tk_SizeOfBitmap(tree->display, column->bitmap, &imgW, &imgH);
	bx = x + layout.imageLeft + sunken;
	h = column->imagePadY[PAD_TOP_LEFT] + imgH
	    + column->imagePadY[PAD_BOTTOM_RIGHT];
	by = y + (height - h) / 2 + sunken;
	by += column->imagePadY[PAD_TOP_LEFT];
	Tree_DrawBitmapWithGC(tree, column->bitmap, td.drawable, column->bitmapGC,
		0, 0, (unsigned int) imgW, (unsigned int) imgH,
		bx, by);
    }
    
    /* Get a graphics context for drawing the text */
    if ((column->text != NULL) && ((column->textLayout != NULL) || (layout.bytesThatFit != 0))) {
	TreeColor *tc;
	XColor *textColor = tree->defColumnTextColor;
	XGCValues gcValues;
	unsigned long mask;
	TreeRectangle trClip;

	tc = PerStateColor_ForState(tree, &column->textColor,
	    Column_MakeState(column), NULL);
	if (tc == NULL || tc->color == NULL) {
	    if (tree->useTheme && TreeTheme_GetColumnTextColor(tree, column->state, &textColor)
		    != TCL_OK) {
		/*textColor = tree->fgColorPtr*/;
	    }
	} else {
	    textColor = tc->color;
	}
	gcValues.font = Tk_FontId(column->tkfont ? column->tkfont : tree->tkfont); /* layout.tkfont */
	gcValues.foreground = textColor->pixel;
	gcValues.graphics_exposures = False;
	mask = GCFont | GCForeground | GCGraphicsExposures;
	gc = Tree_GetGC(tree, mask, &gcValues);

	TreeRect_SetXYWH(trClip, x + layout.textLeft + sunken, y,
		MIN(layout.textWidth, td.width), MIN(height, td.height));
	clipRgn = Tree_GetRectRegion(tree, &trClip);
	TkSetRegion(tree->display, gc, clipRgn);
    }

    if ((column->text != NULL) && (column->textLayout != NULL)) {
	int h;
	TextLayout_Size(column->textLayout, NULL, &h);
	h += column->textPadY[PAD_TOP_LEFT] + column->textPadY[PAD_BOTTOM_RIGHT];
	TextLayout_Draw(tree->display, td.drawable, gc,
		column->textLayout,
		x + layout.textLeft + sunken,
		y + (height - h) / 2 + column->textPadY[PAD_TOP_LEFT] + sunken,
		0, -1, -1);
    } else if ((column->text != NULL) && (layout.bytesThatFit != 0)) {
	char staticStr[256], *text = staticStr;
	int textLen = column->textLen;
	char *ellipsis = "...";
	int ellipsisLen = (int) strlen(ellipsis);
	int tx, ty, h;

	if (textLen + ellipsisLen > sizeof(staticStr))
	    text = ckalloc(textLen + ellipsisLen);
	memcpy(text, column->text, textLen);
	if (layout.bytesThatFit != textLen) {
	    textLen = abs(layout.bytesThatFit);
	    if (layout.bytesThatFit > 0) {
		memcpy(text + layout.bytesThatFit, ellipsis, ellipsisLen);
		textLen += ellipsisLen;
	    }
	}

	tx = x + layout.textLeft + sunken;
	h = column->textPadY[PAD_TOP_LEFT] + layout.fm.linespace
	    + column->textPadY[PAD_BOTTOM_RIGHT];
	ty = y + (height - h) / 2 + layout.fm.ascent + sunken;
	ty += column->textPadY[PAD_TOP_LEFT];
	Tk_DrawChars(tree->display, td.drawable, gc,
		layout.tkfont, text, textLen, tx, ty);
	if (text != staticStr)
	    ckfree(text);
    }

    if (clipRgn != NULL) {
	Tree_UnsetClipMask(tree, td.drawable, gc);
	Tree_FreeRegion(tree, clipRgn);
    }

    if (dragImage)
	return;

#if defined(MAC_OSX_TK)
    /* Under Aqua, we let the Appearance Manager draw the sort arrow */
    if (theme != TCL_OK)
#endif
    Column_DrawArrow(column, td, x, y, layout);

    if (theme != TCL_OK) {
	/* Hack */
	if (visIndex == 0 && column->lock == COLUMN_LOCK_NONE) {
	    x -= tree->canvasPadX[PAD_TOP_LEFT];
	    width += tree->canvasPadX[PAD_TOP_LEFT];
	}
	Tk_Draw3DRectangle(tree->tkwin, td.drawable, border,
		x, y, width, height, column->borderWidth, relief);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SetImageForColumn --
 *
 *	Set a photo image containing a simplified picture of the header
 *	of a column. This image is used when dragging and dropping a column
 *	header.
 *
 * Results:
 *	Token for a photo image, or NULL if the image could not be
 *	created.
 *
 * Side effects:
 *	A photo image called "::TreeCtrl::ImageColumn" will be created if
 *	it doesn't exist. The image is set to contain a picture of the
 *	column header.
 *
 *----------------------------------------------------------------------
 */

static Tk_Image
SetImageForColumn(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn column		/* Column record. */
    )
{
    Tk_PhotoHandle photoH;
    TreeDrawable td;
    int width = column->useWidth; /* the entire column, not just what is visible */
    int height = tree->headerHeight;
    XImage *ximage;

    photoH = Tk_FindPhoto(tree->interp, "::TreeCtrl::ImageColumn");
    if (photoH == NULL) {
	Tcl_GlobalEval(tree->interp, "image create photo ::TreeCtrl::ImageColumn");
	photoH = Tk_FindPhoto(tree->interp, "::TreeCtrl::ImageColumn");
	if (photoH == NULL)
	    return NULL;
    }

    td.width = width;
    td.height = height;
    td.drawable = Tk_GetPixmap(tree->display, Tk_WindowId(tree->tkwin),
	    width, height, Tk_Depth(tree->tkwin));

    Column_Draw(column, td, 0, 0, 0, TRUE);

    /* Pixmap -> XImage */
    ximage = XGetImage(tree->display, td.drawable, 0, 0,
	    (unsigned int)width, (unsigned int)height, AllPlanes, ZPixmap);
    if (ximage == NULL)
	panic("tkTreeColumn.c:SetImageForColumn() ximage is NULL");

    /* XImage -> Tk_Image */
    Tree_XImage2Photo(tree->interp, photoH, ximage, 0, tree->columnDrag.alpha);

    XDestroyImage(ximage);
    Tk_FreePixmap(tree->display, td.drawable);

    return Tk_GetImage(tree->interp, tree->tkwin, "::TreeCtrl::ImageColumn",
	NULL, (ClientData) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * DrawDragIndicator --
 *
 *	Draws the marker between column headers that shows where a
 *	dragged column will end up when dropped.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Drawing.
 *
 *----------------------------------------------------------------------
 */

static void
DrawDragIndicator(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    int lock
    )
{
    TreeColumn column = tree->columnDrag.indColumn;
    int x, y, w, h;
    int minX = 0, maxX = 0;
    GC gc;

    if ((column == NULL) || (column->lock != lock))
	return;

    switch (lock) {
	case COLUMN_LOCK_LEFT:
	    minX = Tree_HeaderLeft(tree);
	    maxX = Tree_ContentLeft(tree);
	    break;
	case COLUMN_LOCK_NONE:
	    minX = Tree_ContentLeft(tree);
	    maxX = Tree_ContentRight(tree);
	    break;
	case COLUMN_LOCK_RIGHT:
	    minX = Tree_ContentRight(tree);
	    maxX = Tree_HeaderRight(tree);
	    break;
    }

    if (TreeColumn_Bbox(column, &x, &y, &w, &h) == 0) {
	if (column == tree->columnVis) {
	    x -= tree->canvasPadX[PAD_TOP_LEFT];
	    w += tree->canvasPadX[PAD_TOP_LEFT];
	}
	if (tree->columnDrag.indSide == SIDE_LEFT) {
	    x -= 1;
	    if (x == minX - 1)
		x += 1;
	} else {
	    x += w - 1;
	    if (x == maxX - 1)
		x -= 1;
	}
	gc = Tk_GCForColor(tree->columnDrag.indColor, Tk_WindowId(tree->tkwin));
	XFillRectangle(tree->display, drawable, gc,
	    x, y, 2, tree->headerHeight);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DrawHeaderLeft --
 *
 *	Draws the column headers for the left-locked columns.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Drawing.
 *
 *----------------------------------------------------------------------
 */

static void
DrawHeaderLeft(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td		/* Where to draw. */
    )
{
    TreeColumn column = tree->columnLockLeft;
    Tk_Window tkwin = tree->tkwin;
    int x = Tree_HeaderLeft(tree), y = Tree_HeaderTop(tree);
    int height = tree->headerHeight;
    TreeDrawable td2;
    int visIndex = 0;

    td2.width = Tk_Width(tkwin);
    td2.height = Tree_HeaderBottom(tree);
    td2.drawable = Tk_GetPixmap(tree->display, Tk_WindowId(tkwin),
	    td2.width, td2.height, Tk_Depth(tkwin));

    while (column != NULL && column->lock == COLUMN_LOCK_LEFT) {
	if (column->visible) {
	    Column_Draw(column, td2, x, y, visIndex++, FALSE);
	    x += column->useWidth;
	}
	column = column->next;
    }

    DrawDragIndicator(tree, td2.drawable, COLUMN_LOCK_LEFT);

    height = MIN(height, Tree_BorderBottom(tree) - Tree_BorderTop(tree));
    XCopyArea(tree->display, td2.drawable, td.drawable,
	    tree->copyGC, Tree_HeaderLeft(tree), y,
	    x - Tree_HeaderLeft(tree), height,
	    Tree_HeaderLeft(tree), y);

    Tk_FreePixmap(tree->display, td2.drawable);
}

/*
 *----------------------------------------------------------------------
 *
 * DrawHeaderRight --
 *
 *	Draws the column headers for the right-locked columns.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Drawing.
 *
 *----------------------------------------------------------------------
 */

static void
DrawHeaderRight(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td		/* Where to draw. */
    )
{
    TreeColumn column = tree->columnLockRight;
    Tk_Window tkwin = tree->tkwin;
    int x = Tree_ContentRight(tree), y = Tree_HeaderTop(tree);
    int height = tree->headerHeight;
    TreeDrawable td2;
    int visIndex = 0;

    td2.width = Tk_Width(tkwin);
    td2.height = Tree_HeaderBottom(tree);
    td2.drawable = Tk_GetPixmap(tree->display, Tk_WindowId(tkwin),
	    td2.width, td2.height, Tk_Depth(tkwin));

    while (column != NULL && column->lock == COLUMN_LOCK_RIGHT) {
	if (column->visible) {
	    Column_Draw(column, td2, x, y, visIndex++, FALSE);
	    x += column->useWidth;
	}
	column = column->next;
    }

    DrawDragIndicator(tree, td2.drawable, COLUMN_LOCK_RIGHT);

    height = MIN(height, Tree_BorderBottom(tree) - Tree_BorderTop(tree));
    XCopyArea(tree->display, td2.drawable, td.drawable,
	    tree->copyGC, Tree_ContentRight(tree), y,
	    x - Tree_ContentRight(tree), height,
	    Tree_ContentRight(tree), y);

    Tk_FreePixmap(tree->display, td2.drawable);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_DrawHeader --
 *
 *	Draw the header of every column.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in a drawable.
 *
 *----------------------------------------------------------------------
 */

void
Tree_DrawHeader(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    int x, int y		/* Top-left corner of the header in
				 * window coordinates. */
    )
{
    TreeColumn column = tree->columns;
    Tk_Window tkwin = tree->tkwin;
    int minX, maxX, width, height;
    Drawable drawable = td.drawable;
    TreeDrawable tp;
    Drawable pixmap;
    int visIndex = 0;

    /* Update layout if needed */
    (void) Tree_HeaderHeight(tree);
    (void) Tree_WidthOfColumns(tree);

    minX = Tree_ContentLeft(tree);
    maxX = Tree_ContentRight(tree);

    if (tree->doubleBuffer == DOUBLEBUFFER_ITEM) {
	tp.width = Tk_Width(tkwin);
	tp.height = Tree_HeaderBottom(tree);
	tp.drawable = Tk_GetPixmap(tree->display, Tk_WindowId(tkwin),
		tp.width, tp.height, Tk_Depth(tkwin));
    } else {
	tp = td;
    }
    pixmap = tp.drawable;

    /* Hack */
    x += tree->canvasPadX[PAD_TOP_LEFT];

    column = tree->columnLockNone;
    while (column != NULL && column->lock == COLUMN_LOCK_NONE) {
	if (column->visible) {
	    /* Hack */
	    if (column == tree->columnVis && tree->canvasPadX[PAD_TOP_LEFT] > 0)
		Column_Draw(column, tp, x, y, visIndex++, FALSE);
	    else
	    if ((x < maxX) && (x + column->useWidth > minX))
		Column_Draw(column, tp, x, y, visIndex++, FALSE);
	    x += column->useWidth;
	}
	if (x >= maxX)
	    break;
	column = column->next;
    }

    /* Draw "tail" column */
    if (x < maxX) {
	column = tree->columnTail;

	/* Hack */
	if (tree->columnCountVis == 0)
	    x -= tree->canvasPadX[PAD_TOP_LEFT];

	width = maxX - x + column->borderWidth;
	height = tree->headerHeight;
	if (!column->visible) {
	    Tk_Fill3DRectangle(tkwin, pixmap, tree->border,
		    x, y, width, height, 0, TK_RELIEF_FLAT);
	} else if (tree->useTheme &&
	    (TreeTheme_DrawHeaderItem(tree, tp, 0, 0, tree->columnCountVis,
		x, y, width, height) == TCL_OK)) {
	} else {
	    Tk_3DBorder border;
	    border = PerStateBorder_ForState(tree, &column->border,
		Column_MakeState(column), NULL);
	    if (border == NULL)
		border = tree->border;
	    Tk_Fill3DRectangle(tkwin, pixmap, border,
		    x, y, width, height, column->borderWidth, TK_RELIEF_RAISED);
	}
    }

    if (minX < maxX)
	DrawDragIndicator(tree, pixmap, COLUMN_LOCK_NONE);

    if (Tree_WidthOfLeftColumns(tree) > 0)
	DrawHeaderLeft(tree, tp);
    if (Tree_WidthOfRightColumns(tree) > 0)
	DrawHeaderRight(tree, tp);

    if (tree->columnDrag.column != NULL) {
	Tk_Image image;
	int x, y, w, h;

	if (TreeColumn_Bbox(tree->columnDrag.column, &x, &y, &w, &h) == 0) {
	    int ix = 0, iy = 0, iw = w, ih = tree->headerHeight;

	    image = SetImageForColumn(tree, tree->columnDrag.column);
	    x += tree->columnDrag.offset;
	    Tree_RedrawImage(image, ix, iy, iw, ih, tp, x, y);
	    Tk_FreeImage(image);
	}
    }

    if (tree->doubleBuffer == DOUBLEBUFFER_ITEM) {
	height = MIN(tree->headerHeight, Tree_BorderBottom(tree) - Tree_BorderTop(tree));
	XCopyArea(tree->display, pixmap, drawable,
		tree->copyGC, Tree_HeaderLeft(tree), y,
		Tree_HeaderWidth(tree), height,
		Tree_HeaderLeft(tree), y);

	Tk_FreePixmap(tree->display, pixmap);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_WidthOfItems --
 *
 *	Calculate the maximum needed width of the styles in every
 *	ReallyVisible() item for a particular column. The width will
 *	only be recalculated if it is marked out-of-date.
 *
 * Results:
 *	Pixel width.
 *
 * Side effects:
 *	The size of elements and styles will be updated if they are
 *	marked out-of-date.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_WidthOfItems(
    TreeColumn column		/* Column token. */
    )
{
    TreeCtrl *tree = column->tree;
    TreeItem item;
    TreeItemColumn itemColumn;
    int width;

    if (column->widthOfItems >= 0)
	return column->widthOfItems;

    column->widthOfItems = 0;
    item = tree->root;
    if (!TreeItem_ReallyVisible(tree, item))
	item = TreeItem_NextVisible(tree, item);
    while (item != NULL) {
#ifdef EXPENSIVE_SPAN_WIDTH /* NOT USED */
	width = TreeItem_NeededWidthOfColumn(tree, item, column->index);
	if (column == tree->columnTree)
	    width += TreeItem_Indent(tree, item);
	column->widthOfItems = MAX(column->widthOfItems, width);
#else
	itemColumn = TreeItem_FindColumn(tree, item, column->index);
	if (itemColumn != NULL) {
	    width = TreeItemColumn_NeededWidth(tree, item, itemColumn);
	    if (column == tree->columnTree)
		width += TreeItem_Indent(tree, item);
	    column->widthOfItems = MAX(column->widthOfItems, width);
	}
#endif
	item = TreeItem_NextVisible(tree, item);
    }

    return column->widthOfItems;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_InvalidateColumnWidth --
 *
 *	Marks the width of zero or more columns as out-of-date.
 *	Schedules a redisplay to check the widths of columns which
 *	will perform any relayout necessary.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Idle task may be scheduled.
 *
 *----------------------------------------------------------------------
 */

void
Tree_InvalidateColumnWidth(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn column		/* Column to modify. NULL means
				 * modify every column. */
    )
{
#ifdef COLUMN_SPANxxx
    /* It may be necessary to recalculate the width of other columns as
     * well when column-spanning is in effect. */
    column = NULL;
#endif

    if (column == NULL) {
	column = tree->columns;
	while (column != NULL) {
	    column->widthOfItems = -1;
	    column = column->next;
	}
    } else {
	column->widthOfItems = -1;
    }
    tree->widthOfColumns = -1;
    tree->widthOfColumnsLeft = tree->widthOfColumnsRight = -1;
    Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_InvalidateColumnHeight --
 *
 *	Marks the height of zero or more column headers as out-of-date.
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
Tree_InvalidateColumnHeight(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn column		/* Column to modify. NULL means
				 * modify every column. */
    )
{
    if (column == NULL) {
	column = tree->columns;
	while (column != NULL) {
	    column->neededHeight = -1;
	    column = column->next;
	}
    } else {
	column->neededHeight = -1;
    }
    tree->headerHeight = -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_TreeChanged --
 *
 *	Called when a TreeCtrl is configured. Performs any relayout
 *	necessary on column headers.
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
TreeColumn_TreeChanged(
    TreeCtrl *tree,		/* Widget info. */
    int flagT			/* TREE_CONF_xxx flags. */
    )
{
    TreeColumn column;

    /* Column widths are invalidated elsewhere */
    if (flagT & TREE_CONF_FONT) {
	column = tree->columns;
	while (column != NULL) {
	    if ((column->tkfont == NULL) && (column->textLen > 0)) {
		column->textWidth = Tk_TextWidth(tree->tkfont, column->text,
		    column->textLen);
		column->neededWidth = column->neededHeight = -1;
		column->textLayoutInvalid = TRUE;
	    }
	    column = column->next;
	}
	tree->headerHeight = -1;
    }

    /* Need to do this if the -usetheme option changes or the system
     * theme changes. */
    if (flagT & TREE_CONF_RELAYOUT) {
	column = tree->columns;
	while (column != NULL) {
	    column->neededWidth = column->neededHeight = -1;
	    column = column->next;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_HeaderHeight --
 *
 *	Return the total height of the column header area. The height
 *	is only recalculated if it is marked out-of-date.
 *
 * Results:
 *	Pixel height. Will be zero if the -showheader option is FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tree_HeaderHeight(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeColumn column;
    int height;

    if (!tree->showHeader)
	return 0;

    if (tree->headerHeight >= 0)
	return tree->headerHeight;

    height = 0;
    column = tree->columns;
    while (column != NULL) {
	if (column->visible)
	    height = MAX(height, TreeColumn_NeededHeight(column));
	column = column->next;
    }
    return tree->headerHeight = height;
}

/*
 *--------------------------------------------------------------
 *
 * TreeColumn_Bbox --
 *
 *	Return the bounding box for a column header.
 *
 * Results:
 *	Return value is -1 if the column is not visible.
 *
 * Side effects:
 *	Column layout will be updated if needed.
 *
 *--------------------------------------------------------------
 */

int
TreeColumn_Bbox(
    TreeColumn column,		/* Column token. */
    int *x, int *y,		/* Out: window coordinates. */
    int *w, int *h		/* Out: width and height. */
    )
{
    TreeCtrl *tree = column->tree;
    int left = 0;

    if (!tree->showHeader || !TreeColumn_Visible(column))
	return -1;

    *y = Tree_HeaderTop(tree);
    *h = Tree_HeaderHeight(tree);

    if (column == tree->columnTail) {
	*x = Tree_WidthOfColumns(tree) - tree->xOrigin; /* Canvas -> Window */
	*w = 1; /* xxx */
	return 0;
    }

    /* Get width (and update column layout) */
    *w = TreeColumn_UseWidth(column);

    switch (TreeColumn_Lock(column)) {
	case COLUMN_LOCK_LEFT:
	    left = Tree_BorderLeft(tree);
	    break;
	case COLUMN_LOCK_NONE:
	    left = 0 - Tree_GetOriginX(tree); /* Canvas -> Window */
	    break;
	case COLUMN_LOCK_RIGHT:
	    left = Tree_ContentRight(tree);
	    break;
    }

    *x = left + TreeColumn_Offset(column);
    return 0;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_HeaderUnderPoint --
 *
 *	Return a TreeColumn whose header contains the given coordinates.
 *
 * Results:
 *	TreeColumn token or NULL if no column contains the point.
 *
 * Side effects:
 *	Column layout will be updated if needed.
 *
 *--------------------------------------------------------------
 */

TreeColumn
Tree_HeaderUnderPoint(
    TreeCtrl *tree,		/* Widget info. */
    int *x_, int *y_,		/* In: window coordinates.
				 * Out: coordinates relative to top-left
				 * corner of the returned column. */
    int *w, int *h,		/* Returned width and height. */
    int nearest			/* TRUE if the column nearest the coordinates
				 * should be returned. */
    )
{
    Tk_Window tkwin = tree->tkwin;
    int x = *x_, y = *y_;
    int left, top, width, height;
    TreeColumn column = tree->columns;

    if (!nearest && (Tree_HitTest(tree, x, y) != TREE_AREA_HEADER))
	return NULL;

    if (nearest) {
	if (x < Tree_BorderLeft(tree))
	    x = Tree_BorderLeft(tree);
	if (x >= Tree_BorderRight(tree))
	    x = Tree_BorderRight(tree) - 1;
	if (y < Tree_BorderTop(tree))
	    y = Tree_BorderTop(tree);
	if (y >= Tree_ContentTop(tree))
	    y = Tree_ContentTop(tree) - 1;
    }

    /* Test the columns in reverse of drawing order. */
    column = tree->columnLockRight;
    while ((column != NULL) && (TreeColumn_Lock(column) == COLUMN_LOCK_RIGHT)) {
	if (TreeColumn_Bbox(column, &left, &top, &width, &height) == 0) {
	    if ((x >= left) && (x < left + width)) {
		goto done;
	    }
	}
	column = TreeColumn_Next(column);
    }

    column = tree->columnLockLeft;
    while ((column != NULL) && (TreeColumn_Lock(column) == COLUMN_LOCK_LEFT)) {
	if (TreeColumn_Bbox(column, &left, &top, &width, &height) == 0) {
	    if ((x >= left) && (x < left + width)) {
		goto done;
	    }
	}
	column = TreeColumn_Next(column);
    }

    column = tree->columnLockNone;
    while ((column != NULL) && (TreeColumn_Lock(column) == COLUMN_LOCK_NONE)) {
	if (TreeColumn_Bbox(column, &left, &top, &width, &height) == 0) {
	    /* Hack */
	    if (x + tree->xOrigin < tree->canvasPadX[PAD_TOP_LEFT])
		goto done;
	    if ((x >= left) && (x < left + width)) {
		goto done;
	    }
	}
	column = TreeColumn_Next(column);
    }

    column = tree->columnTail;
    left = Tree_WidthOfColumns(tree) - tree->xOrigin;
    width = Tk_Width(tkwin) - left;
done:
    (*x_) = x - left;
    (*y_) = y - Tree_HeaderTop(tree);
    (*w) = width;
    (*h) = Tree_HeaderHeight(tree);
    return column;
}

/*
 *----------------------------------------------------------------------
 *
 * LayoutColumns --
 *
 *	Calculates the display width and horizontal offset of a range
 *	of columns.
 *
 * Results:
 *	The .useWidth and .offset fields of every column in the range
 *	are updated.
 *	The result is the sum of the widths of all visible columns in the
 *	range.
 *
 * Side effects:
 *	The size of elements and styles may be updated if they are
 *	marked out-of-date.
 *
 *----------------------------------------------------------------------
 */

static int
LayoutColumns(
    TreeColumn first,		/* First column to update. All columns
				 * with the same -lock value are updated. */
    TreeColumn *visPtr,		/* Out: first visible column. */
    int *countVisPtr		/* Out: number of visible columns. */
    )
{
    TreeCtrl *tree;
    TreeColumn column;
    int width, visWidth, totalWidth = 0;
    int numExpand = 0, numSqueeze = 0;
#ifdef UNIFORM_GROUP
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    UniformGroup *uniform;
    int uniformCount = 0;
#endif

    if (visPtr != NULL)
	(*visPtr) = NULL;
    (*countVisPtr) = 0;

    if (first == NULL)
	return 0;

    tree = first->tree;

#ifdef UNIFORM_GROUP
    /* Initialize the .minSize field of every uniform group. */
    hPtr = Tcl_FirstHashEntry(&tree->uniformGroupHash, &search);
    while (hPtr != NULL) {
	uniform = (UniformGroup *) Tcl_GetHashValue(hPtr);
	uniform->minSize = 0;
	hPtr = Tcl_NextHashEntry(&search);
    }
#endif

    /*
     * Determine the initial display width of each column. This will be:
     *   a) the column's -width option (a fixed width), or
     *   b) the maximum of:
     *    1) the width requested by the column's header
     *    2) the width requested by each item style in that column
     * For b) the width is clipped to -minwidth and -maxwidth.
     */
    column = first;
    while (column != NULL && column->lock == first->lock) {
	if (column->visible) {
	    if (column->widthObj != NULL)
		width = column->width;
	    else {
		width = TreeColumn_WidthOfItems(column);
		width = MAX(width, TreeColumn_NeededWidth(column));
		width = MAX(width, TreeColumn_MinWidth(column));
		if (TreeColumn_MaxWidth(column) != -1)
		    width = MIN(width, TreeColumn_MaxWidth(column));
#ifdef UNIFORM_GROUP
		/* Track the maximum requested width of every column in this
		 * column's uniform group considering -weight. */
		if (column->uniform != NULL) {
		    int weight = MAX(column->weight, 1);
		    int minSize = (width + weight - 1) / weight;
		    if (minSize > column->uniform->minSize)
			column->uniform->minSize = minSize;
		    uniformCount++;
		}
		if (column->expand)
		    numExpand += MAX(column->weight, 0);
		if (column->squeeze)
		    numSqueeze += MAX(column->weight, 0);
#else
		if (column->expand)
		    numExpand++;
		if (column->squeeze)
		    numSqueeze++;
#endif
	    }
	    if (visPtr != NULL && (*visPtr) == NULL)
		(*visPtr) = column;
	    (*countVisPtr)++;
	} else
	    width = 0;
	column->useWidth = width;
	totalWidth += width;
	column = column->next;
    }

#ifdef UNIFORM_GROUP
    /* Apply the -uniform and -weight options. */
    if (uniformCount > 0) {
	column = first;
	while (column != NULL && column->lock == first->lock) {
	    if (column->visible &&
		    column->widthObj == NULL &&
		    column->uniform != NULL) {
		int weight = MAX(column->weight, 1);
		width = column->uniform->minSize * weight;
		if (column->maxWidthObj != NULL)
		    width = MIN(width, column->maxWidth);
		totalWidth -= column->useWidth;
		column->useWidth = width;
		totalWidth += width;
	    }
	    column = column->next;
	}
    }
#endif /* UNIFORM_GROUP */

    /* Locked columns don't squeeze or expand. */
    if (first->lock != COLUMN_LOCK_NONE)
	goto doOffsets;

    visWidth = Tree_ContentWidth(tree);

    /* Hack */
    visWidth -= tree->canvasPadX[PAD_TOP_LEFT] + tree->canvasPadX[PAD_BOTTOM_RIGHT];

    if (visWidth <= 0)
	goto doOffsets;

    /* Squeeze columns */
    if ((visWidth < totalWidth) && (numSqueeze > 0)) {
	int spaceRemaining = totalWidth - visWidth;
	while ((spaceRemaining > 0) && (numSqueeze > 0)) {
	    int each = (spaceRemaining >= numSqueeze) ?
		spaceRemaining / numSqueeze : 1;
	    numSqueeze = 0;
	    column = first;
	    while (column != NULL && column->lock == first->lock) {
		if (column->visible &&
			column->squeeze &&
			(column->widthObj == NULL)) {
		    int min = MAX(0, TreeColumn_MinWidth(column));
		    if (column->useWidth > min) {
			int sub = MIN(each, column->useWidth - min);
			column->useWidth -= sub;
			spaceRemaining -= sub;
			if (!spaceRemaining) break;
			if (column->useWidth > min)
			    numSqueeze++;
		    }
		}
		column = column->next;
	    }
	}
    }

    /* Expand columns */
    if ((visWidth > totalWidth) && (numExpand > 0)) {
	int spaceRemaining = visWidth - totalWidth;
	while ((spaceRemaining > 0) && (numExpand > 0)) {
	    int each = (spaceRemaining >= numExpand) ?
		spaceRemaining / numExpand : 1;
	    numExpand = 0;
	    column = first;
	    while (column != NULL && column->lock == first->lock) {
#ifdef UNIFORM_GROUP
		int weight = MAX(column->weight, 0);
		if (column->visible &&
			column->expand && weight &&
			(column->widthObj == NULL)) {
		    int max = TreeColumn_MaxWidth(column);
		    if ((max == -1) || (column->useWidth < max)) {
			int eachW = MIN(each * weight, spaceRemaining);
			int add = (max == -1) ? eachW : MIN(eachW, max - column->useWidth);
			column->useWidth += add;
			spaceRemaining -= add;
			if (!spaceRemaining) break;
			if ((max == -1) || (column->useWidth < max))
			    numExpand += weight;
#else
		if (column->visible &&
			column->expand &&
			(column->widthObj == NULL)) {
		    int max = TreeColumn_MaxWidth(column);
		    if ((max == -1) || (column->useWidth < max)) {
			int add = (max == -1) ? each : MIN(each, max - column->useWidth);
			column->useWidth += add;
			spaceRemaining -= add;
			if (!spaceRemaining) break;
			if ((max == -1) || (column->useWidth < max))
			    numExpand++;
#endif
		    }
		}
		column = column->next;
	    }
	}
    }

doOffsets:

    /* Calculate the horizontal offset of each column in the range.
     * The total width is recalculated as well (needed anyway if any
     * columns were expanded or squeezed). */
    totalWidth = 0;
    column = first;
    while (column != NULL && column->lock == first->lock) {
	column->offset = totalWidth;

	/* Hack */
	if (column->lock == COLUMN_LOCK_NONE)
	    column->offset += tree->canvasPadX[PAD_TOP_LEFT];

	totalWidth += column->useWidth;
	column = column->next;
    }
    return totalWidth;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_WidthOfColumns --
 *
 *	Return the total display width of all non-locked columns (except
 *	the tail).
 *	The width is only recalculated if it is marked out-of-date.
 *	Other fields of the TreeCtrl are updated to reflect the current
 *	arrangement of columns.
 *
 * Results:
 *	Pixel width.
 *
 * Side effects:
 *	The size of elements and styles may be updated if they are
 *	marked out-of-date.
 *
 *----------------------------------------------------------------------
 */

int
Tree_WidthOfColumns(
    TreeCtrl *tree		/* Widget info. */
    )
{
    /* This gets called when the layout of all columns needs to be current.
     * So update the layout of the left- and right-locked columns too. */
    (void) Tree_WidthOfLeftColumns(tree);
    (void) Tree_WidthOfRightColumns(tree);

    if (tree->widthOfColumns >= 0)
	return tree->widthOfColumns;

    tree->widthOfColumns = LayoutColumns(
	tree->columnLockNone,
	&tree->columnVis,
	&tree->columnCountVis);

    if (tree->columnTree != NULL && TreeColumn_Visible(tree->columnTree)) {
	tree->columnTreeLeft = tree->columnTree->offset;
	tree->columnTreeVis = TRUE;
    } else {
	tree->columnTreeLeft = 0;
	tree->columnTreeVis = FALSE;
    }

    return tree->widthOfColumns;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_WidthOfLeftColumns --
 *
 *	Return the total display width of all left-locked columns.
 *	The width is only recalculated if it is marked out-of-date.
 *	Other fields of the TreeCtrl are updated to reflect the current
 *	arrangement of columns.
 *
 * Results:
 *	Pixel width.
 *
 * Side effects:
 *	The size of elements and styles may be updated if they are
 *	marked out-of-date.
 *
 *----------------------------------------------------------------------
 */

int
Tree_WidthOfLeftColumns(
    TreeCtrl *tree		/* Widget info. */
    )
{
    if (tree->widthOfColumnsLeft >= 0)
	return tree->widthOfColumnsLeft;

    if (!Tree_ShouldDisplayLockedColumns(tree)) {
	TreeColumn column = tree->columnLockLeft;
	while (column != NULL && column->lock == COLUMN_LOCK_LEFT) {
	    column->useWidth = 0;
	    column = column->next;
	}
	tree->columnCountVisLeft = 0;
	tree->widthOfColumnsLeft = 0;
	return 0;
    }

    tree->widthOfColumnsLeft = LayoutColumns(
	tree->columnLockLeft,
	NULL,
	&tree->columnCountVisLeft);

    return tree->widthOfColumnsLeft;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_WidthOfRightColumns --
 *
 *	Return the total display width of all right-locked columns.
 *	The width is only recalculated if it is marked out-of-date.
 *	Other fields of the TreeCtrl are updated to reflect the current
 *	arrangement of columns.
 *
 * Results:
 *	Pixel width.
 *
 * Side effects:
 *	The size of elements and styles may be updated if they are
 *	marked out-of-date.
 *
 *----------------------------------------------------------------------
 */

int
Tree_WidthOfRightColumns(
    TreeCtrl *tree		/* Widget info. */
    )
{
    if (tree->widthOfColumnsRight >= 0)
	return tree->widthOfColumnsRight;

    if (!Tree_ShouldDisplayLockedColumns(tree)) {
	TreeColumn column = tree->columnLockRight;
	while (column != NULL && column->lock == COLUMN_LOCK_RIGHT) {
	    column->useWidth = 0;
	    column = column->next;
	}
	tree->columnCountVisRight = 0;
	tree->widthOfColumnsRight = 0;
	return 0;
    }

    tree->widthOfColumnsRight = LayoutColumns(
	tree->columnLockRight,
	NULL,
	&tree->columnCountVisRight);

    return tree->widthOfColumnsRight;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_InitColumns --
 *
 *	Perform column-related initialization when a new TreeCtrl is
 *	created.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

void
Tree_InitColumns(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeColumn column;

    column = Column_Alloc(tree);
    column->id = -1;
    tree->columnTail = column;
    tree->nextColumnId = 0;
    tree->columnCount = 0;
    Column_Config(column, 0, NULL, TRUE);

    tree->columnDrag.optionTable = Tk_CreateOptionTable(tree->interp, dragSpecs);
    (void) Tk_InitOptions(tree->interp, (char *) tree,
	    tree->columnDrag.optionTable, tree->tkwin);

#ifdef UNIFORM_GROUP
    Tcl_InitHashTable(&tree->uniformGroupHash, TCL_STRING_KEYS);
#endif

    tree->defColumnTextColor = Tk_GetColor(tree->interp, tree->tkwin, DEF_BUTTON_FG);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_FreeColumns --
 *
 *	Free column-related resources for a deleted TreeCtrl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

void Tree_FreeColumns(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeColumn column = tree->columns;

    while (column != NULL) {
	column = Column_Free(column);
    }

    Column_Free(tree->columnTail);
    tree->columnCount = 0;

#ifdef UNIFORM_GROUP
    Tcl_DeleteHashTable(&tree->uniformGroupHash);
#endif

    Tk_FreeColor(tree->defColumnTextColor);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_InitInterp --
 *
 *	Performs column-related initialization when the TkTreeCtrl
 *	package is loaded into an interpreter.
 *
 * Results:
 *	TCL_OK.
 *
 * Side effects:
 *	Messes with the columnSpecs[] option array.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_InitInterp(
    Tcl_Interp *interp		/* Current interpreter. */
    )
{
    Tk_OptionSpec *specPtr;
    Tcl_DString dString;

    specPtr = Tree_FindOptionSpec(columnSpecs, "-background");
    if (specPtr->defValue == NULL) {
	Tcl_DStringInit(&dString);
	Tcl_DStringAppendElement(&dString, DEF_BUTTON_BG_COLOR);
	Tcl_DStringAppendElement(&dString, "normal");
	Tcl_DStringAppendElement(&dString, DEF_BUTTON_ACTIVE_BG_COLOR);
	Tcl_DStringAppendElement(&dString, "");
	specPtr->defValue = ckalloc(Tcl_DStringLength(&dString) + 1);
	strcpy((char *)specPtr->defValue, Tcl_DStringValue(&dString));
	Tcl_DStringFree(&dString);
    }
    
    PerStateCO_Init(columnSpecs, "-arrowbitmap", &pstBitmap, ColumnStateFromObj);
    PerStateCO_Init(columnSpecs, "-arrowimage", &pstImage, ColumnStateFromObj);
    PerStateCO_Init(columnSpecs, "-background", &pstBorder, ColumnStateFromObj);
    PerStateCO_Init(columnSpecs, "-textcolor", &pstColor, ColumnStateFromObj);
    StringTableCO_Init(columnSpecs, "-itemjustify", justifyStrings);

    return TCL_OK;
}
