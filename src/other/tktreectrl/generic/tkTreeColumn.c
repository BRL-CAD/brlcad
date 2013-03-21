/*
 * tkTreeColumn.c --
 *
 *	This module implements treectrl widget's columns.
 *
 * Copyright (c) 2002-2011 Tim Baker
 * Copyright (c) 2002-2003 Christian Krone
 * Copyright (c) 2003 ActiveState Corporation
 */

#include "tkTreeCtrl.h"

typedef struct TreeColumn_ TreeColumn_;

typedef struct ColumnSpan ColumnSpan;
typedef struct SpanArray SpanArray;
struct SpanArray
{
    ColumnSpan **spans;
    int count;			/* Number of useful elements in spans[]. */
    int alloc;			/* Number of elements allocated in spans[]. */
};

/* A structure of the following type is kept for each span in an item or
 * header that covers a unique range of columns. */
struct ColumnSpan
{
    TreeColumn start;		/* First column in the span. */
    TreeColumn end;		/* Last column in the span. */
    int maxNeededWidth;		/* Width of the widest style in any item
				 * or header. */
    int widthOfColumns;		/* Sum of the calculated display widths of
				 * the columns. */
    SpanArray spansToRight;	/* List of spans following this one. */
    ColumnSpan *next;		/* Head is TreeColumnPriv_.spans. */
    ColumnSpan *nextCur;	/* Head is TreeColumnPriv_.spansCur. */
    int sumOfSpans;
};

/* A structure of the following type is kept for each TreeColumn.
 * This is used when calculating the requested width of styles. */
typedef struct ColumnReqData ColumnReqData;
struct ColumnReqData
{
    int vis;		/* TRUE if the column is visible, otherwise FALSE. */
    int min;		/* -minwidth or -1, no greater than -maxwidth. */
    int fixed;		/* -width, or -1. */
    int max;		/* -maxwidth or -1. */
    int req;		/* The width requested by a all or part of a style
			 * in a single item in this column. */
    int maxSingleSpanWidth; /* The widest span of 1. */
    int maxSingleItemWidth; /* The widest span of 1 in items. */
    int maxSingleHeaderWidth; /* The widest span of 1 in headers. */
    SpanArray spans;	/* Array of span pointers touching this column.*/
    TreeColumn spanMin;		/* Any span that includes this column */
    TreeColumn spanMax;		/* begins on or after spanMin and ends on */
				/* or before spanMax.  This includes spans */
				/* in headers and items. */
    int fat;		/* TRUE when every spans[].widthOfColumns is greater
			 * than spans[].maxNeededWidth, indicating the column
			 * is wider than needed. */
};

/* A structure of the following type is kept for each TreeCtrl to hold
 * private data used by this file. */
struct TreeColumnPriv_
{
    int spansInvalid;		/* TRUE if the TreeColumn.spanMin and
				 * TreeColumn.spanMax fields are
				 * out-of-date, otherwise FALSE. */
    int reqInvalid;		/* TRUE if TreeColumn.reqData is out-of-date,
				 * otherwise FALSE. */
    ColumnSpan *spans;		/* All the spans for the widget. */
    ColumnSpan *freeSpans;	/* Unused spans. */
    ColumnSpan *spansCur;	/* The subset of spans[] for the current
				 * update. */
    int allSpansAreOne;		/* TRUE if all spans cover exactly one column,
				 * otherwise FALSE. */
};

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
    Tk_Justify justify;		/* -justify */
    int itemJustify;		/* -itemjustify */
    int expand;			/* -expand */
    int squeeze;		/* -squeeze */
    int visible;		/* -visible */
    int resize;			/* -resize */
    TagInfo *tagInfo;		/* -tags */
    Tcl_Obj *itemBgObj;		/* -itembackground */
    TreeStyle itemStyle;	/* -itemstyle */
    int lock;			/* -lock */

    TreeCtrl *tree;
    Tk_OptionTable optionTable;
    int id;			/* unique column identifier */
    int index;			/* order in list of columns */
    int offset;			/* Total width of preceding columns */
    int useWidth;		/* -width, -minwidth, or required+expansion */
    int widthOfItems;		/* width of all TreeItemColumns */
    int itemBgCount;		/* -itembackground colors */
    TreeColor **itemBgColor;	/* -itembackground colors */
    TreeColumn prev;
    TreeColumn next;
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
    ColumnReqData reqData;
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

static CONST char *arrowSideST[] = { "left", "right", (char *) NULL };
static CONST char *lockST[] = { "left", "none", "right", (char *) NULL };
static CONST char *justifyStrings[] = {
    "left", "right", "center", (char *) NULL
};

#define COLU_CONF_TWIDTH	0x0008	/* totalWidth */
#define COLU_CONF_ITEMBG	0x0010
#define COLU_CONF_DISPLAY	0x0040
#define COLU_CONF_JUSTIFY	0x0080
#define COLU_CONF_TAGS		0x0100
#ifdef DEPRECATED
#define COLU_CONF_RANGES	0x0800
#endif
#if COLUMNGRID == 1
#define COLU_CONF_GRIDLINES	0x1000
#endif

static Tk_OptionSpec columnSpecs[] = {
    {TK_OPTION_BOOLEAN, "-expand", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeColumn_, expand),
     0, (ClientData) NULL, COLU_CONF_TWIDTH},
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
#ifdef DEPRECATED
    {TK_OPTION_PIXELS, "-stepwidth", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(TreeColumn_, stepWidthObj),
     Tk_Offset(TreeColumn_, stepWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_RANGES},
#endif /* DEPRECATED */
    {TK_OPTION_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeColumn_, tagInfo),
     TK_OPTION_NULL_OK, (ClientData) &TreeCtrlCO_tagInfo, COLU_CONF_TAGS},
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
    (ClientData) INT2PTR(CFO_NOT_NULL)
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
    (ClientData) INT2PTR(CFO_NOT_NULL | CFO_NOT_TAIL)
};

static Tk_OptionSpec dragSpecs[] = {
    {TK_OPTION_BOOLEAN, "-enable", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeCtrl, columnDrag.enable),
     0, (ClientData) NULL, 0},
    {TK_OPTION_INT, "-imagealpha", (char *) NULL, (char *) NULL,
     "200", -1, Tk_Offset(TreeCtrl, columnDrag.alpha),
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
    {TK_OPTION_INT, "-imagespan", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeCtrl, columnDrag.span),
     0, (ClientData) NULL, 0},
    {TK_OPTION_COLOR, "-indicatorcolor", (char *) NULL, (char *) NULL,
     "Black", -1, Tk_Offset(TreeCtrl, columnDrag.indColor),
     0, (ClientData) NULL, 0},
    {TK_OPTION_CUSTOM, "-indicatorcolumn", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeCtrl, columnDrag.indColumn),
     TK_OPTION_NULL_OK, (ClientData) &TreeCtrlCO_column, 0},
    {TK_OPTION_STRING_TABLE, "-indicatorside", (char *) NULL, (char *) NULL,
     "left", -1, Tk_Offset(TreeCtrl, columnDrag.indSide),
     0, (ClientData) arrowSideST, 0},
    {TK_OPTION_INT, "-indicatorspan", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeCtrl, columnDrag.indSpan),
     0, (ClientData) NULL, 0},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

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
 *	Helper routine for TreeColumnList_FromObj.
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
 *	Helper routine for TreeColumnList_FromObj.
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
	"lock", "tag", "visible", "!tail", "!visible", NULL
    };
    enum qualEnum {
	QUAL_LOCK, QUAL_TAG, QUAL_VISIBLE, QUAL_NOT_TAIL, QUAL_NOT_VISIBLE
    };
    /* Number of arguments used by qualifiers[]. */
    static int qualArgs[] = {
	2, 2, 1, 1, 1
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
 *	Helper routine for TreeColumnList_FromObj.
 *
 * Results:
 *	Returns TRUE if the column meets the given criteria.
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
 *	Helper routine for TreeColumnList_FromObj.
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
 * span N QUALIFIERS
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
	"next", "prev", "span", (char *) NULL
    };
    enum modEnum {
	TMOD_NEXT, TMOD_PREV, TMOD_SPAN
    };
    /* Number of arguments used by modifiers[]. */
    static int modArgs[] = {
	1, 1, 2
    };
    /* Boolean: can modifiers[] be followed by 1 or more qualifiers. */
    static int modQual[] = {
	1, 1, 1
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
	    case TMOD_SPAN: {
		int span, lock;
		TreeColumn match = NULL;

		if (Tcl_GetIntFromObj(NULL, objv[listIndex + 1], &span) != TCL_OK)
		    goto errorExit;
		lock = column->lock;
		while (span-- > 0 && column != NULL && column->lock == lock) {
		    if (!Qualifies(&q, column))
			break;
		    match = column;
		    column = column->next;
		}
		column = match;
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

if (columnIndex == tree->columnTail->index) return tree->columnTail;

    while (column != NULL) {
	if (column->index == columnIndex)
	    break;
	column = column->next;
    }
    return column;
}

TreeColumn
Tree_FirstColumn(
    TreeCtrl *tree,
    int lock,
    int tailOK
    )
{
    TreeColumn column = NULL;

    switch (lock) {
	case COLUMN_LOCK_LEFT:
	    column = tree->columnLockLeft;
	    break;
	case COLUMN_LOCK_NONE:
	    column = tree->columnLockNone;
	    if (column == NULL && tailOK)
		column = tree->columnTail;
	    break;
	case COLUMN_LOCK_RIGHT:
	    column = tree->columnLockRight;
	    break;
	default:
	    column = tree->columns;
	    if (column == NULL && tailOK)
		column = tree->columnTail;
	    break;
    }
    return column;
}

TreeColumn
Tree_ColumnToTheRight(
    TreeColumn column,		/* Column token. */
    int displayOrder,
    int tailOK
    )
{
    TreeCtrl *tree = column->tree;
    TreeColumn next = column->next;

    if (column == tree->columnTail)
	tailOK = FALSE;
    if (displayOrder && next == tree->columnLockRight) {
	return tailOK ? tree->columnTail : NULL;
    }
    if (next == NULL && tailOK)
	return tree->columnTail;
    return next;
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

    /* Move the column in every header */
    item = tree->headerItems;
    while (item != NULL) {
	TreeItem_MoveColumn(tree, item, move->index, before->index);
	item = TreeItem_GetNextSibling(tree, item);
    }

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
	TreeColumns_InvalidateWidth(tree);
	TreeColumns_InvalidateCounts(tree);
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
    int visible = column->visible;
    int lock = column->lock;
#if COLUMNGRID == 1
    int gridLines, prevGridLines = visible &&
	    (column->gridLeftColor != NULL ||
	    column->gridRightColor != NULL);
#endif

    int objC = 0, hObjC = 0;
    Tcl_Obj *staticObjV[STATIC_SIZE], **objV = staticObjV;
    Tcl_Obj *staticHObjV[STATIC_SIZE], **hObjV = staticHObjV;
    int i;

    /* Hack -- Pass some options to the underlying header-column */
    STATIC_ALLOC(objV, Tcl_Obj *, objc);
    STATIC_ALLOC(hObjV, Tcl_Obj *, objc);
    for (i = 0; i < objc; i += 2) {
	Tk_OptionSpec *specPtr = columnSpecs;
	int length;
	CONST char *optionName = Tcl_GetStringFromObj(objv[i], &length);
	while (specPtr->type != TK_OPTION_END) {
	    if (strncmp(specPtr->optionName, optionName, length) == 0) {
		objV[objC++] = objv[i];
		if (i + 1 < objc)
		    objV[objC++] = objv[i + 1];
		/* Pass -justify to the default header as well */
		if (strcmp(specPtr->optionName, "-justify") == 0) {
		    hObjV[hObjC++] = objv[i];
		    if (i + 1 < objc)
			hObjV[hObjC++] = objv[i + 1];
		}
		break;
	    }
	    specPtr++;
	}
	if (specPtr->type == TK_OPTION_END) {
	    hObjV[hObjC++] = objv[i];
	    if (i + 1 < objc)
		hObjV[hObjC++] = objv[i + 1];
	}
    }
    if (TreeHeader_ConsumeColumnConfig(tree, column, hObjC, hObjV, createFlag) != TCL_OK) {
	STATIC_FREE(objV, Tcl_Obj *, objc);
	STATIC_FREE(hObjV, Tcl_Obj *, objc);
	return TCL_ERROR;
    }

    /* Init these to prevent compiler warnings */
    saved.itemBgCount = 0;
    saved.itemBgColor = NULL;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) column,
			column->optionTable, objC, objV, tree->tkwin,
			&savedOptions, &mask) != TCL_OK) {
		mask = 0;
		continue;
	    }

	    /* Wouldn't have to do this if Tk_InitOptions() would return
	     * a mask of configured options like Tk_SetOptions() does. */
	    if (createFlag) {
		if (column->itemBgObj != NULL)
		    mask |= COLU_CONF_ITEMBG;
	    }

	    /*
	     * Step 1: Save old values
	     */

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

	    if (mask & COLU_CONF_ITEMBG)
		Column_FreeColors(column, saved.itemBgColor, saved.itemBgCount);
	    Tk_FreeSavedOptions(&savedOptions);
	    STATIC_FREE(objV, Tcl_Obj *, objc);
	    STATIC_FREE(hObjV, Tcl_Obj *, objc);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    /*
	     * Free new values.
	     */
	    if (maskFree & COLU_CONF_ITEMBG)
		Column_FreeColors(column, column->itemBgColor, column->itemBgCount);

	    /*
	     * Restore old values.
	     */
	    if (mask & COLU_CONF_ITEMBG) {
		column->itemBgColor = saved.itemBgColor;
		column->itemBgCount = saved.itemBgCount;
	    }

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    STATIC_FREE(objV, Tcl_Obj *, objc);
	    STATIC_FREE(hObjV, Tcl_Obj *, objc);
	    return TCL_ERROR;
	}
    }

    /* Indicate that all items must recalculate their list of spans. */
    if (visible != column->visible || lock != column->lock)
	TreeItem_SpansInvalidate(tree, NULL);

    if (visible != column->visible || lock != column->lock)
	TreeColumns_InvalidateCounts(tree);

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

    /* If there are no more visible columns, the header isn't shown. */
    /* Header height may change with the width of columns (due to text wrapping). */
    if (mask & COLU_CONF_TWIDTH) {
	tree->headerHeight = -1;
    }

    /* FIXME: only this column needs to be redisplayed. */
    if (mask & COLU_CONF_JUSTIFY)
	Tree_DInfoChanged(tree, DINFO_INVALIDATE);

#ifdef DEPRECATED
    /* -stepwidth and -widthhack */
    if (mask & COLU_CONF_RANGES)
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
#endif

    /* Redraw everything */
    if (mask & COLU_CONF_TWIDTH) {
	if ((column->reqData.spanMin != NULL) && /* if it's NULL, then it's hidden or tree->spansInvalid=TRUE */
		(column->reqData.spanMin != column->reqData.spanMax)) { /* spans of 1 don't require item-width recalc */
	    TreeColumns_InvalidateWidthOfItems(tree, column);
	}
	TreeColumns_InvalidateWidth(tree);
	Tree_DInfoChanged(tree, DINFO_DRAW_HEADER);
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
    tree->headerHeight = -1;
    /* Don't call TreeColumns_InvalidateWidth(), it will fail during
     * Tree_InitColumns(). */
    tree->widthOfColumns = -1;
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
    TreeDisplay_FreeColumnDInfo(tree, column);
    Tk_FreeConfigOptions((char *) column, column->optionTable, tree->tkwin);
    if (column->reqData.spans.spans != NULL)
	ckfree((char *) column->reqData.spans.spans);
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
 * TreeColumn_VisIndex --
 *
 *	Return the 0-based index for a column.
 *
 * Results:
 *	Position of the column in the list of visible columns.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_VisIndex(
    TreeColumn column		/* Column token. */
    )
{
    TreeCtrl *tree = column->tree;
    TreeColumn walk = Tree_FirstColumn(tree, column->lock, TRUE);
    int index = 0;

    /* FIXME: slow, this is only used by headers in TreeItem_Indent. */
    /* I can't calculate this in LayoutColumns because TreeItem_Indent is
     * used during that procedure. */

    if (!column->visible)
	return -1;

    while (walk != column) {
	if (walk->visible) {
	    /* I only care about the first non-locked visible column (index==0).*/
	    return 1;
	    index++;
	}
	walk = Tree_ColumnToTheRight(walk, TRUE, TRUE);
    }
    return index;
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
	    int i, tagSpace = 0, numTags = 0;

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
	    {
		Tk_OptionSpec *specPtr = columnSpecs;
		int length;
		CONST char *optionName = Tcl_GetStringFromObj(objv[4], &length);
		while (specPtr->type != TK_OPTION_END) {
		    if (strncmp(specPtr->optionName, optionName, length) == 0) {
			break;
		    }
		    specPtr++;
		}
		if (specPtr->type == TK_OPTION_END) {
		    return TreeHeader_ConsumeColumnCget(tree, column, objv[4]);
		}
	    }
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
		if (objc == 5) {
		    Tk_OptionSpec *specPtr = columnSpecs;
		    int length;
		    CONST char *optionName = Tcl_GetStringFromObj(objv[4], &length);
		    while (specPtr->type != TK_OPTION_END) {
			if (strncmp(specPtr->optionName, optionName, length) == 0) {
			    break;
			}
			specPtr++;
		    }
		    if (specPtr->type == TK_OPTION_END) {
			resultObjPtr = TreeHeader_ConsumeColumnOptionInfo(tree, column, objv[4]);
			if (resultObjPtr == NULL)
			    goto errorExit;
			Tcl_SetObjResult(interp, resultObjPtr);
			break;
		    }
		    resultObjPtr = Tk_GetOptionInfo(interp, (char *) column,
			    column->optionTable,
			    (objc == 4) ? (Tcl_Obj *) NULL : objv[4],
			    tree->tkwin);
		    if (resultObjPtr == NULL)
			goto errorExit;
		    Tcl_SetObjResult(interp, resultObjPtr);
		} else {
		    /* Return the combined [configure] output of the column and header-column */
		    Tcl_Obj *objPtr = Tk_GetOptionInfo(interp, (char *) column,
			column->optionTable, (Tcl_Obj *) NULL,  tree->tkwin);
		    resultObjPtr = TreeHeader_ConsumeColumnOptionInfo(tree, column, NULL);
		    Tcl_ListObjAppendList(interp, resultObjPtr, objPtr);
		    Tcl_SetObjResult(interp, resultObjPtr);
		    break;
		}
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
	    TreeItem item;

	    /* FIXME: -count N -tags $tags */
	    column = Column_Alloc(tree);

	    column->index = TreeColumn_Index(tree->columnTail) + 1; /* after the tail column */

	    /* Create the item-column and header-column in every header */
	    item = tree->headerItems;
	    while (item != NULL) {
		(void) TreeItem_MakeColumnExist(tree, item, column->index);
		item = TreeItem_GetNextSibling(tree, item);
	    }
	    column->index = TreeColumn_Index(tree->columnTail);

	    if (Column_Config(column, objc - 3, objv + 3, TRUE) != TCL_OK) {

		/* Delete the item-column and header-column in every header */
		item = tree->headerItems;
		while (item != NULL) {
		    TreeItem_RemoveColumns(tree, item, column->index, column->index);
		    item = TreeItem_GetNextSibling(tree, item);
		}

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

	    item = tree->headerItems;
	    while (item != NULL) {
		TreeItemColumn itemColumn = TreeItem_FindColumn(tree, item,
		    column->index);
		TreeHeaderColumn_EnsureStyleExists(TreeItem_GetHeader(tree,
		    item), TreeItemColumn_GetHeaderColumn(tree, itemColumn),
		    column);
		item = TreeItem_GetNextSibling(tree, item);
	    }

	    /* Indicate that all items must recalculate their list of spans. */
	    TreeItem_SpansInvalidate(tree, NULL);

	    TreeColumns_InvalidateCounts(tree);
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
			TreeHeader_ColumnDeleted(tree, column);
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
		    item = tree->headerItems;
		    while (item != NULL) {
			TreeItem_RemoveAllColumns(tree, item);
			item = TreeItem_GetNextSibling(tree, item);
		    }

		    /* Delete all TreeItemColumns */
		    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		    while (hPtr != NULL) {
			item = (TreeItem) Tcl_GetHashValue(hPtr);
			TreeItem_RemoveAllColumns(tree, item);
			hPtr = Tcl_NextHashEntry(&search);
		    }

		    tree->columnTree = NULL;
		    goto doneDELETE;
		}

		/* Delete all TreeItemColumns */
		item = tree->headerItems;
		while (item != NULL) {
		    TreeItem_RemoveColumns(tree, item, column->index,
			column->index);
		    item = TreeItem_GetNextSibling(tree, item);
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
		TreeHeader_ColumnDeleted(tree, column);
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

doneDELETE:
	    TreeColumns_InvalidateCounts(tree);
	    tree->widthOfColumns = tree->headerHeight = -1;
	    tree->widthOfColumnsLeft = tree->widthOfColumnsRight = -1;
	    tree->columnPriv->reqInvalid = TRUE;
	    Tree_DInfoChanged(tree, DINFO_REDO_COLUMN_WIDTH);

	    /* Indicate that all items must recalculate their list of spans. */
	    TreeItem_SpansInvalidate(tree, NULL);

	    if (objc == 5)
		TreeColumnList_Free(&column2s);
	    break;
	}

	/* T column dragcget option */
	case COMMAND_DRAGCGET: {
	    return TreeHeaderCmd(clientData, interp, objc, objv);
	}

	/* T column dragconfigure ?option? ?value? ?option value ...? */
	case COMMAND_DRAGCONF: {
	    return TreeHeaderCmd(clientData, interp, objc, objv);
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

	    Tcl_SetObjResult(interp, Tcl_NewIntObj(TreeColumn_UseWidth(column)));
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
		if (before == tree->columnTail) {
		    FormatResult(tree->interp,
			"can't move column %s%d before tail: -lock options conflict",
			tree->columnPrefix, move->id);
		} else {
		    FormatResult(tree->interp,
			"can't move column %s%d before column %s%d: -lock options conflict",
			tree->columnPrefix, move->id,
			tree->columnPrefix, before->id);
		}
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
	    width = MAX(width, TreeColumn_WidthOfHeaders(column));
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
 * InitColumnReqData --
 *
 *	Initialize the ColumnReqData structure in every column.
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
InitColumnReqData(
    TreeCtrl *tree
    )
{
    TreeColumnPriv priv = tree->columnPriv;
    ColumnReqData *cd;
    TreeColumn column;

    if (!priv->reqInvalid || tree->columnCount == 0)
	return;

/*dbwin("InitColumnReqData %s\n", Tk_PathName(tree->tkwin));*/

    for (column = tree->columns;
	    column != NULL;
	    column = column->next) {
	cd = &column->reqData;
	cd->vis = TreeColumn_Visible(column);
	cd->min = TreeColumn_MinWidth(column);
	cd->fixed = TreeColumn_FixedWidth(column);
	cd->max = TreeColumn_MaxWidth(column);
/*	cd->req = 0;*/
	if ((cd->max >= 0) && (cd->min > cd->max))
	    cd->min = cd->max;
    }

    priv->reqInvalid = FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 * SpanArray_Add --
 *
 *	Adds a ColumnSpan pointer to an array if it isn't already in
 *	the array.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
SpanArray_Add(
    SpanArray *sa,
    ColumnSpan *cs
    )
{
    int i;

    for (i = 0; i < sa->count; i++) {
	if (sa->spans[i] == cs)
	    return;
    }
    if (sa->alloc < sa->count + 1) {
	sa->spans = (ColumnSpan **) ckrealloc((char *) sa->spans,
	    sizeof(ColumnSpan *) * (sa->count + 10));
	sa->alloc = sa->count + 10;
    }
    sa->spans[sa->count++] = cs;
}

/*
 *----------------------------------------------------------------------
 *
 * AddColumnSpan --
 *
 *	Adds or updates a span record for a range of columns
 *	covered by a span.  For every unique range of columns covered
 *	by a span, there exists exactly one ColumnSpan record.
 *
 * Results:
 *	Pointer to a new or updated ColumnSpan.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static ColumnSpan *
AddColumnSpan(
    ColumnSpan *spanPrev,	/* The span to the left.  The span returned
				 * by this function will be added to the
				 * spanToTheRight array of spanPrev if it
				 * wasn't already. */
    TreeColumn spanMin,		/* First column in the span. */
    TreeColumn spanMax,		/* Last column in the span. */
    int neededWidth,		/* Width needed by the span. */
    int doHeaders		/* TRUE if this span is in a header, FALSE
				 * if the span is in an item. */
    )
{
    TreeCtrl *tree = spanMin->tree;
    TreeColumnPriv priv = tree->columnPriv;
    ColumnSpan *cs = priv->spans;
    ColumnReqData *cd = &spanMin->reqData;
    TreeColumn column;
    int i;

    /* See if a span record exists by checking the list of spans touching
     * the first column. */
    for (i = 0; i < cd->spans.count; i++) {
	cs = cd->spans.spans[i];
	if ((cs->start == spanMin) && (cs->end == spanMax))
	    break;
    }
    if (i < cd->spans.count) {
	/* Add this span to the list of spans following spanPrev. */
	if (spanPrev != NULL && priv->spansInvalid == TRUE)
	    SpanArray_Add(&spanPrev->spansToRight, cs);

	cs->maxNeededWidth = MAX(cs->maxNeededWidth, neededWidth);

	/* Remember the widest span of 1 in this column. */
	if (spanMin == spanMax) {
	    cd->maxSingleSpanWidth = MAX(cd->maxSingleSpanWidth, neededWidth);
	    if (doHeaders)
		cd->maxSingleHeaderWidth = MAX(cd->maxSingleHeaderWidth, neededWidth);
	    else
		cd->maxSingleItemWidth = MAX(cd->maxSingleItemWidth, neededWidth);
	}
	return cs;
    }

#ifdef TREECTRL_DEBUG
    if (priv->spansInvalid == FALSE) BreakIntoDebugger();
#endif

    if (priv->freeSpans == NULL) {
	cs = (ColumnSpan *) ckalloc(sizeof(ColumnSpan));
	cs->spansToRight.alloc = 0;
	cs->spansToRight.spans = NULL;
    } else {
	cs = priv->freeSpans;
	priv->freeSpans = cs->next;
    }
    cs->start = spanMin;
    cs->end = spanMax;
    cs->maxNeededWidth = neededWidth;
    cs->spansToRight.count = 0;

    cs->next = priv->spans;
    priv->spans = cs;

    cs->nextCur = priv->spansCur;
    priv->spansCur = cs;

    /* Add this span to the list of spans following spanPrev. */
    if (spanPrev != NULL)
	SpanArray_Add(&spanPrev->spansToRight, cs);

    for (column = spanMin; column != spanMax->next; column = column->next) {
	cd = &column->reqData;
	/* Add this new span record to the list of span records touching
	 *  this column. */
	SpanArray_Add(&cd->spans, cs);

	/* Track the minimum and maximum columns of any span touching this
	 * column.*/
	if (priv->spansInvalid == FALSE) {
#ifdef TREECTRL_DEBUG
	    if (spanMin->index < cd->spanMin->index) BreakIntoDebugger();
	    if (spanMax->index > cd->spanMax->index) BreakIntoDebugger();
#endif
	} else {
	    if (spanMin->index < cd->spanMin->index)
		cd->spanMin = spanMin;
	    if (spanMax->index > cd->spanMax->index)
		cd->spanMax = spanMax;
	}

	/* Remember the widest span of 1 in this column. */
	if (spanMin == spanMax) {
	    cd->maxSingleSpanWidth = MAX(cd->maxSingleSpanWidth, neededWidth);
	    if (doHeaders)
		cd->maxSingleHeaderWidth = MAX(cd->maxSingleHeaderWidth, neededWidth);
	    else
		cd->maxSingleItemWidth = MAX(cd->maxSingleItemWidth, neededWidth);
	} else
	    priv->allSpansAreOne = FALSE;
    }
    return cs;
}
/*
s1 -> s2 -> s3
   |_ s4 -> s5
s6 -> s7 -> s8
*/
static int
SumSpanWidths(
    int *sum,			/* The current total span width. */
    SpanArray *sa,		/* For each span in this array, the
				 * maximum width of each chain of spans
				 * starting with that span is found and
				 * added to *sum. */
    TreeColumn end		/* The column up to and including which the
				 * sum of spans should be found. */
    )
{
    int i, max = 0;
    int visited = 0;

    for (i = 0; i < sa->count; i++) {
	ColumnSpan *cs = sa->spans[i];
	if (cs->end->index <= end->index) {
	    visited++;
	    if (cs->sumOfSpans == -1) {
		cs->sumOfSpans = cs->maxNeededWidth;
		visited += SumSpanWidths(&cs->sumOfSpans, &cs->spansToRight, end);
	    }
	    max = MAX(max, cs->sumOfSpans);
	}
    }
    *sum += max;
    return visited;
}

/*
 *----------------------------------------------------------------------
 *
 * TrimTheFat --
 *
 *	Removes excess width from columns that may have been added by
 *	DistributeSpanWidthToColumns(). In the scenario below, where
 *	two items and the width needed by each of 3 spans is shown,
 *	Column0 will have a requested width of 75, and Column1 will
 *	have a requested width of 100/2=50, for a total width of
 *	75+50=125, which is wider than any item needs.  At the end of
 *	this procedure, the requested width of Column1 will be 25.
 *
 *	       Column0 Column1
 *	item1: 75----- 25----
 *	item2: 100-----------
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
TrimTheFatAux(
    TreeColumn start,
    TreeColumn end
    )
{
#ifdef TREECTRL_DEBUG
    TreeCtrl *tree = start->tree;
#endif
    TreeColumn column;
    int sumOfSpanWidths = 0;
    int sumOfColumnWidths = 0, fat, fatPerCol;
    int numColsThatCanShrink = 0, csn;
    ColumnReqData *cd;
    ColumnSpan *cs;
#ifdef TREECTRL_DEBUG
    int visited;

    if (start->prev != NULL &&
	start->prev->reqData.spanMax->index >= start->index) BreakIntoDebugger();
    if (end->next != NULL &&
	end->next->reqData.spanMin->index <= end->index) BreakIntoDebugger();
#endif

    /* Sum the widths of spans across the range of columns. */
    sumOfSpanWidths = 0;
#ifdef TREECTRL_DEBUG
    visited = 
#endif
    SumSpanWidths(&sumOfSpanWidths, &start->reqData.spans, end);
    if (sumOfSpanWidths <= 0)
	return;

    for (column = start; column != end->next; column = column->next) {
	cd = &column->reqData;
	if (!cd->vis) continue;
	sumOfColumnWidths += (cd->fixed >= 0) ? cd->fixed : column->widthOfItems;
	if (cd->fat &&
		(cd->fixed < 0) &&
		(column->widthOfItems > MAX(cd->maxSingleSpanWidth, cd->min)))
	    numColsThatCanShrink++;
    }

#ifdef TREECTRL_DEBUG
    if (tree->debug.enable && tree->debug.span)
	dbwin("%d-%d sumOfColumnWidths %d sumOfSpanWidths %d (visited %d)\n", start->index, end->index, sumOfColumnWidths, sumOfSpanWidths, visited);
#endif

    if (!numColsThatCanShrink)
	return;
    fat = sumOfColumnWidths - sumOfSpanWidths;
    if (fat <= 0)
	return;

    while (fat > 0) {
	int origFat = fat;
	/* Subtract 1 to smooth out the distribution. May result in more looping. */
	fatPerCol = MAX(1, fat / numColsThatCanShrink - 1);
	for (column = start; column != end->next; column = column->next) {
	    int minSpanFat = -1;
	    cd = &column->reqData;
	    if (!cd->vis || !cd->fat) continue;
	    if ((cd->fixed >= 0) || (column->widthOfItems <= cd->min) ||
		    (column->widthOfItems <= cd->maxSingleSpanWidth))
		continue;
	    for (csn = 0; csn < cd->spans.count; csn++) {
		cs = cd->spans.spans[csn];
		if (cs->widthOfColumns > cs->maxNeededWidth) {
		    if (minSpanFat == -1)
			minSpanFat = cs->widthOfColumns - cs->maxNeededWidth;
		    else
			minSpanFat = MIN(minSpanFat, cs->widthOfColumns - cs->maxNeededWidth);
		} else {
		    minSpanFat = 0; /* no span touching this column can get smaller */
		    cd->fat = FALSE; /* FIXME: mark all in this span */
		    --numColsThatCanShrink;
		    break;
		}
	    }
	    if (minSpanFat > 0) {
		int trim = MIN(minSpanFat, fatPerCol);
		if (column->widthOfItems - trim < cd->min)
		    trim = column->widthOfItems - cd->min;
		if (column->widthOfItems - trim < cd->maxSingleSpanWidth)
		    trim = column->widthOfItems - cd->maxSingleSpanWidth;
#ifdef TREECTRL_DEBUG
		if (tree->debug.enable && tree->debug.span)
		    dbwin("trimmed %d from %d (numColsThatCanShrink=%d fat=%d minSpanFat=%d)\n", trim, column->index, numColsThatCanShrink, fat, minSpanFat);
#endif
		column->widthOfItems -= trim;
		fat -= trim;
		if (fat <= 0) break;
		for (csn = 0; csn < cd->spans.count; csn++) {
		    cs = cd->spans.spans[csn];
		    cs->widthOfColumns -= trim;
		}
		if (column->widthOfItems <= MAX(cd->maxSingleSpanWidth, cd->min))
		    --numColsThatCanShrink;
	    }
	    if (numColsThatCanShrink == 0)
		break;
	}
	if (fat == origFat || numColsThatCanShrink == 0)
	    break;
    }
}

static void
TrimTheFat(
    TreeColumn start,
    TreeColumn end
    )
{
    TreeColumnPriv priv = start->tree->columnPriv;
    TreeColumn column;
    ColumnReqData *cd;
    ColumnSpan *cs;

    if (priv->allSpansAreOne)
	return;

#ifdef TREECTRL_DEBUG
    if (start->prev != NULL &&
	start->prev->reqData.spanMax->index >= start->index) BreakIntoDebugger();
    if (end->next != NULL &&
	end->next->reqData.spanMin->index <= end->index) BreakIntoDebugger();
#endif

    for (cs = priv->spansCur; cs != NULL; cs = cs->nextCur) {
	/* Sum the current widths of columns in each span record. */
	cs->widthOfColumns = 0;
	for (column = cs->start; column != cs->end->next; column = column->next) {
	    cd = &column->reqData;
	    if (!cd->vis) continue;
	    cs->widthOfColumns += (cd->fixed >= 0) ? cd->fixed : column->widthOfItems;
	}
	/* Mark columns that cannot get smaller. */
	if (cs->widthOfColumns <= cs->maxNeededWidth) {
	    for (column = cs->start; column != cs->end->next; column = column->next) {
		cd = &column->reqData;
		cd->fat = FALSE;
	    }
#if 0
	} else {
	    priv->minPositiveFatOfAllSpans = MIN(priv->minPositiveFatOfAllSpans, cs->widthOfColumns - cs->maxNeededWidth);
#endif
	}
	cs->sumOfSpans = -1;
    }

    column = start;
    while (column != end->next) {
	TreeColumn columnMin, columnMax;
	/* Operate on the narrowest range of columns whose spans do not
	 * overlap another such range of columns. */
	columnMin = column->reqData.spanMin;
	columnMax = column->reqData.spanMax;
	while ((columnMax->next != NULL) &&
		(columnMax->next->reqData.spanMin->index <= columnMax->index)) {
	    columnMax = columnMax->next->reqData.spanMax;
	}
	TrimTheFatAux(columnMin, columnMax);
	column = columnMax->next;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_RequestWidthInColumns --
 *
 *	Calculates the width needed by styles in a range of columns
 *	for a single header or item.
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
TreeItem_RequestWidthInColumns(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,
    TreeColumn columnMin,
    TreeColumn columnMax
    )
{
#ifdef TREECTRL_DEBUG
    TreeColumnPriv priv = tree->columnPriv;
#endif
    int doHeaders = TreeItem_GetHeader(tree, item) != NULL;
    int columnIndexMin = TreeColumn_Index(columnMin);
    int columnIndexMax = TreeColumn_Index(columnMax);
    int *spans = TreeItem_GetSpans(tree, item);
    TreeItemColumn itemColumn;
    TreeColumn treeColumn;
    int columnIndex, width, indent;
    ColumnSpan *csPrev = NULL;

#ifdef TREECTRL_DEBUG
    if (columnMax == tree->columnTail) BreakIntoDebugger();
    if (priv->reqInvalid) BreakIntoDebugger();
    if (columnMin->prev != NULL &&
	columnMin->prev->reqData.spanMax->index >= columnMin->index) BreakIntoDebugger();
    if (columnMax->next != NULL &&
	columnMax->next->reqData.spanMin->index <= columnMax->index) BreakIntoDebugger();
#endif

    treeColumn = columnMin;
    itemColumn = TreeItem_FindColumn(tree, item, columnIndexMin);
    if (spans == NULL) {
	for (columnIndex = columnIndexMin;
		columnIndex <= columnIndexMax;
		++columnIndex) {
	    ColumnReqData *cd = &treeColumn->reqData;
	    if (cd->vis) {
		width = (itemColumn != NULL) ?
		    TreeItemColumn_NeededWidth(tree, item, itemColumn) : 0;
		/* FOR COMPATIBILITY ONLY. Don't request width from -indent
		 * if there is no item column. */
		if (itemColumn != NULL) {
		    indent = doHeaders ? 0 : TreeItem_Indent(tree, treeColumn, item);
		    width += indent;
		}
		csPrev = AddColumnSpan(csPrev, treeColumn, treeColumn, width, doHeaders);
	    }
	    treeColumn = TreeColumn_Next(treeColumn);
	    if (itemColumn != NULL)
		itemColumn = TreeItemColumn_GetNext(tree, itemColumn);
	}
	return;
    }

#if defined(TREECTRL_DEBUG)
    /* It must be true that a span starts at columnMin. */
    if (spans[columnIndexMin] != columnIndexMin) BreakIntoDebugger();
#endif

    for (columnIndex = columnIndexMin;
	    columnIndex <= columnIndexMax;
	    /*++columnIndex*/) {
	ColumnReqData *cd = &treeColumn->reqData;
	int columnIndex2 = columnIndex;
	TreeColumn treeColumn2 = treeColumn;
	TreeColumn lastColumnInSpan = treeColumn;
#if defined(TREECTRL_DEBUG)
	if (TreeColumn_Index(treeColumn) != columnIndex) BreakIntoDebugger();
	if (itemColumn != NULL && TreeItemColumn_Index(tree, item, itemColumn) != columnIndex) BreakIntoDebugger();
	if (spans[columnIndex] != columnIndex) BreakIntoDebugger();
#endif
	while ((columnIndex2 <= columnIndexMax) &&
		(spans[columnIndex2] == columnIndex)) {
	    lastColumnInSpan = treeColumn2;
	    treeColumn2 = TreeColumn_Next(treeColumn2);
	    columnIndex2++;
	}

	if (cd->vis) {
	    width = (itemColumn != NULL) ?
		TreeItemColumn_NeededWidth(tree, item, itemColumn) : 0;

	    /* Indentation is spread amongst the visible columns as well, and
	     * is only used if the -treecolumn is the first column in the span. */
	    /* FOR COMPATIBILITY ONLY. Don't request width from -indent
	     * if there is no item column. */
	    if (itemColumn != NULL) {
		indent = doHeaders ? 0 : TreeItem_Indent(tree, treeColumn, item);
		width += indent;
	    }
	    csPrev = AddColumnSpan(csPrev, treeColumn, lastColumnInSpan, width, doHeaders);
	}

	treeColumn = TreeColumn_Next(lastColumnInSpan);
	if (treeColumn == NULL)
	    break;
	while (/*(itemColumn != NULL) && */(columnIndex < TreeColumn_Index(treeColumn))) {
	    if (itemColumn != NULL)
	    itemColumn = TreeItemColumn_GetNext(tree, itemColumn);
	    ++columnIndex;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DistributeSpanWidthToColumns --
 *
 *	Ensures that each range of columns covered by a span is wide
 *	enough to contain that span, without violating the -width and
 *	-maxwidth column options.
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
DistributeSpanWidthToColumns(
    TreeColumn start,
    TreeColumn end
    )
{
    TreeCtrl *tree = start->tree;
    TreeColumnPriv priv = tree->columnPriv;
    ColumnSpan *cs;
    ColumnReqData *cd;
    TreeColumn column;

    if (priv->allSpansAreOne) {
	for (column = start; column != end->next; column = column->next)
	    column->widthOfItems = column->reqData.maxSingleSpanWidth;
	return;
    }

    for (cs = priv->spansCur; cs != NULL; cs = cs->nextCur) {
	int numVisibleColumns = 0, spaceRemaining = cs->maxNeededWidth;
	int numMinWidth = 0, minMinWidth = -1, varWidth = 0;

	if (cs->maxNeededWidth <= 0)
	    continue;

	for (column = cs->start;
		column != cs->end->next;
		column = column->next) {
	    cd = &column->reqData;
#if defined(TREECTRL_DEBUG)
	    if (cd->vis != TreeColumn_Visible(column)) BreakIntoDebugger();
	    if (TreeColumn_MaxWidth(column) >= 0 && cd->min > TreeColumn_MaxWidth(column)) BreakIntoDebugger();
	    if (TreeColumn_MaxWidth(column) < 0 && cd->min != TreeColumn_MinWidth(column)) BreakIntoDebugger();
	    if (cd->max != TreeColumn_MaxWidth(column)) BreakIntoDebugger();
	    if (cd->fixed != TreeColumn_FixedWidth(column)) BreakIntoDebugger();
#endif
	    cd->req = 0;
	    if (cd->vis)
		numVisibleColumns++;
	}

	/* Dump space into fixed-width and minwidth */
	for (column = cs->start;
		(spaceRemaining > 0) &&
		(column != cs->end->next);
		column = column->next) {
	    int spaceUsed;
	    cd = &column->reqData;
	    if (!cd->vis) continue;
	    if (cd->fixed >= 0) {
		spaceUsed = cd->fixed;
		numVisibleColumns--;
	    } else if (cd->min >= 0) {
		spaceUsed = cd->min;
		++numMinWidth;
		if (minMinWidth == -1)
		    minMinWidth = cd->min;
		else
		    minMinWidth = MIN(minMinWidth, cd->min);
	    } else
		continue;
	    spaceUsed = MIN(spaceUsed, spaceRemaining);
	    cd->req += spaceUsed;
	    spaceRemaining -= spaceUsed;
	}

	/* Distribute width to visible columns in the span. */
	while ((spaceRemaining > 0) && (numVisibleColumns > 0)) {
	    int each;
	    int origSpaceRemaining = spaceRemaining;

	    /* This is the amount to give to each column. Some columns
	     * may get one extra pixel (starting from the leftmost column). */
	    each = MAX(1, spaceRemaining / numVisibleColumns);
	    varWidth += each;

	    /* If all the columns have -minwidth, there will be a lot of
	     * useless looping until varWidth exceeds the smallest -minwidth. */
	    if (numMinWidth == numVisibleColumns)
		while (varWidth <= minMinWidth)
		    varWidth += each;

	    for (column = cs->start;
		    (spaceRemaining > 0) &&
		    (column != cs->end->next);
		    column = column->next) {
		int spaceUsed = each;
		cd = &column->reqData;
		if (!cd->vis) continue;
		if (cd->fixed >= 0)
		    continue;
		if ((cd->max >= 0) && (cd->req >= cd->max))
		    continue;
		/* Don't grow minwidth columns until the space of variable-width
		 * columns exceeds that of minwidth. */
		if ((cd->min >= 0) && (cd->req >= varWidth))
		    continue;
		if (cd->min >= 0) {
		    spaceUsed = MIN(varWidth - cd->req, spaceUsed);
		}
		if (cd->max >= 0) {
		    spaceUsed = MIN(cd->max - cd->req, spaceUsed);
		    if (cd->req + MIN(spaceUsed, spaceRemaining) >= cd->max) {
			spaceUsed = cd->max - cd->req;
			numVisibleColumns--;
		    }
		}
		spaceUsed = MIN(spaceUsed, spaceRemaining);
		cd->req += spaceUsed;
#if defined(TREECTRL_DEBUG)
		if (cd->fixed >= 0 && cd->req > cd->fixed) BreakIntoDebugger();
		if (cd->fixed < 0 && cd->max >= 0 && cd->req > cd->max) BreakIntoDebugger();
#endif
		spaceRemaining -= spaceUsed;
	    }

	    if (spaceRemaining == origSpaceRemaining)
		break;
	}
	for (column = cs->start;
		(column != cs->end->next);
		column = column->next) {
	    cd = &column->reqData;
	    if (!cd->vis) continue;
	    column->widthOfItems = MAX(column->widthOfItems, cd->req);
	}
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
    TreeColumnPriv priv = tree->columnPriv;
    TreeColumn columnMin = NULL, columnMax = NULL;

    if (IS_TAIL(column))
	return 0;

    if (priv->spansInvalid) {
	columnMin = tree->columns;
	columnMax = tree->columnLast;
	if (priv->freeSpans != NULL)
	    priv->freeSpans->next = priv->spans;
	else
	    priv->freeSpans = priv->spans;
	priv->spans = NULL;
	priv->spansCur = NULL;
	priv->allSpansAreOne = TRUE;
    } else if (column->reqData.spanMin->widthOfItems < 0) {
	ColumnSpan *cs;

	columnMin = column->reqData.spanMin;
	columnMax = column->reqData.spanMax;
	/* Gather all adjacent out-of-date columns into one group. */
	/* Must also get any spans that overlap any spans that overlap this
	 * column. */
	while ((columnMin->prev != NULL) &&
		((columnMin->prev->reqData.spanMax->index >= columnMin->index) ||
		(columnMin->prev->reqData.spanMin->widthOfItems < 0))) {
	    columnMin = columnMin->prev->reqData.spanMin;
	}
	while ((columnMax->next != NULL) &&
		((columnMax->next->reqData.spanMin->index <= columnMax->index) ||
		(columnMax->next->widthOfItems < 0))) {
	    columnMax = columnMax->next->reqData.spanMax;
	}
	/* Build the list of span records touching the range of columns. */
	priv->spansCur = NULL;
	for (cs = priv->spans; cs != NULL; cs = cs->next) {
	    if (cs->start->index < columnMin->index ||
		    cs->end->index > columnMax->index) {
		continue;
	    }
	    cs->maxNeededWidth = 0;
	    cs->nextCur = priv->spansCur;
	    priv->spansCur = cs;
	}
    }
    if (columnMin != NULL) {
	TreeColumn column2;
	for (column2 = columnMin;
		column2 != columnMax->next;
		column2 = column2->next) {
	    column2->widthOfItems = 0;
	    column2->reqData.maxSingleSpanWidth = 0;
	    column2->reqData.maxSingleHeaderWidth = 0;
	    column2->reqData.maxSingleItemWidth = 0;
	    column2->reqData.fat = TRUE;
	    if (priv->spansInvalid) {
		column2->reqData.spanMin = column2->reqData.spanMax = column2;
		column2->reqData.spans.count = 0;
	    }
	}
#ifdef TREECTRL_DEBUG
	if (tree->debug.enable && tree->debug.span)
	    dbwin("RequestWidthInColumns %s %d-%d allSpansAreOne=%d\n",
		Tk_PathName(tree->tkwin), columnMin->index, columnMax->index,
		priv->allSpansAreOne);
#endif
	InitColumnReqData(tree);
	TreeHeaders_RequestWidthInColumns(tree, columnMin, columnMax);
	TreeItems_RequestWidthInColumns(tree, columnMin, columnMax);
	priv->spansInvalid = FALSE; /* Clear this *after* the above call. */
	DistributeSpanWidthToColumns(columnMin, columnMax);
	TrimTheFat(columnMin, columnMax);
    }

    /* FOR COMPATIBILITY ONLY */
    /* See the use of TreeColumn_WidthOfItems() by tkTreeDisplay.c. */
    TreeColumns_UpdateCounts(tree);
    if (tree->columnCountVis == 1 && tree->columnVis == column)
	return column->reqData.maxSingleItemWidth;

    return column->widthOfItems;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_WidthOfHeaders --
 *
 *	Return the requested width of styles displayed in every visible
 *	header for a single column.
 *
 * Results:
 *	Pixel width.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_WidthOfHeaders(
    TreeColumn column		/* Column token. */
    )
{
    TreeCtrl *tree = column->tree;
    int width = TreeColumn_WidthOfItems(column);

    /* FOR COMPATIBILITY ONLY */
    /* See the use of TreeColumn_WidthOfItems() by tkTreeDisplay.c. */
    if (tree->columnCountVis == 1 && tree->columnVis == column)
	return column->reqData.maxSingleHeaderWidth;

    return width;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumns_InvalidateSpans --
 *
 *	Marks the spanMin & spanMax fields of every column as
 *	out-of-date.
 *	Called when anything affects the spans of items or headers,
 *	such as:
 *	a) header and item deletion
 *	b) header and item visibility changes, including expanding,
 *	   collapsing, or reparenting
 *	d) [item span] or [header span] changed spans
 *	e) column creation, deletion, reordering, or visibility
 *	   changes (handled by TreeItem_SpansInvalidate).
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
TreeColumns_InvalidateSpans(
    TreeCtrl *tree		/* Widget info. */
    )
{
    tree->columnPriv->spansInvalid = TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumns_InvalidateWidthOfItems --
 *
 *	Marks the width requested by items in zero or more columns
 *	as out-of-date.
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
TreeColumns_InvalidateWidthOfItems(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn column		/* Column to modify. NULL means
				 * modify every column. */
    )
{
    TreeColumnPriv priv = tree->columnPriv;

    /* FIXME: This gets called for both items and headers.  If invalidating
     * header width, there is no need to invalidate widthOfItems unless the
     * column is covered by a span > 1 in one or more items. */

    if (column == NULL) {
	column = tree->columns;
	while (column != NULL) {
	    column->widthOfItems = -1;
	    column = column->next;
	}
    } else if (!priv->spansInvalid &&
	    column->reqData.spanMin != NULL) { /* spanMin/Max can be NULL during creation when spansInvalid hasn't been set TRUE yet */
	TreeColumn columnMin = column->reqData.spanMin;
	TreeColumn columnMax = column->reqData.spanMax;
	columnMin->widthOfItems = -1;

	/* Must recalculate the width of items in every span that overlaps
	 * any of the spans that include this column. */
	while ((columnMin->prev != NULL) &&
		(columnMin->prev->reqData.spanMax->index >= columnMin->index)) {
	    columnMin = columnMin->prev->reqData.spanMin;
	    columnMin->widthOfItems = -1;
	}
	while ((columnMax->next != NULL) &&
		(columnMax->next->reqData.spanMin->index <= columnMax->index)) {
	    columnMax = columnMax->next->reqData.spanMax;
	    columnMax->reqData.spanMin->widthOfItems = -1;
	}

    }
    TreeColumns_InvalidateWidth(tree);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumns_InvalidateWidth --
 *
 *	Marks the width of columns as out-of-date.
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
TreeColumns_InvalidateWidth(
    TreeCtrl *tree		/* Widget info. */
    )
{
    tree->widthOfColumns = -1;
    tree->widthOfColumnsLeft = -1;
    tree->widthOfColumnsRight = -1;
    tree->columnPriv->reqInvalid = TRUE;
    Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH);
}

/*
 *--------------------------------------------------------------
 *
 * TreeColumn_Bbox --
 *
 *	Return the bounding box for a column.
 *	This used to be the function to call to get the bounding
 *	box of a column header.
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
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn first		/* First column to update. All columns
				 * with the same -lock value are updated. */
    )
{
    TreeColumn column;
    int width, visWidth, totalWidth = 0;
    int numExpand = 0, numSqueeze = 0;
#ifdef UNIFORM_GROUP
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    UniformGroup *uniform;
    int uniformCount = 0;
#endif

#ifdef TREECTRL_DEBUG
    if (tree->debugCheck.inLayoutColumns)
	panic("recursive call to LayoutColumns");
#endif

    if (first == NULL)
	return 0;

    tree = first->tree;

#ifdef TREECTRL_DEBUG
    tree->debugCheck.inLayoutColumns = TRUE;
#endif

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
		width = MAX(width, TreeColumn_WidthOfHeaders(column));
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

#ifdef TREECTRL_DEBUG
    tree->debugCheck.inLayoutColumns = FALSE;
#endif

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
    /* Tree_WidthOfColumns used to update columnCountVis, but that is now
     * done in a separate function TreeColumns_UpdateCounts, because
     * TreeItem_ReallyVisible needs up-to-date counts (for header items)
     * and LayoutColumns may call TreeItem_ReallyVisible. */
    TreeColumns_UpdateCounts(tree);

    /* This gets called when the layout of all columns needs to be current.
     * So update the layout of the left- and right-locked columns too. */
    (void) Tree_WidthOfLeftColumns(tree);
    (void) Tree_WidthOfRightColumns(tree);

    if (tree->widthOfColumns >= 0)
	return tree->widthOfColumns;

    tree->widthOfColumns = LayoutColumns(tree, tree->columnLockNone);

    if (tree->columnTree != NULL && TreeColumn_Visible(tree->columnTree)) {
	tree->columnTreeLeft = tree->columnTree->offset;
	tree->columnTreeVis = TRUE;
    } else {
	tree->columnTreeLeft = 0;
	tree->columnTreeVis = FALSE;
    }

    /* I can't calculate the width of the tail column here because it
     * depends on Tree_FakeCanvasWidth which calls this function. */
    tree->columnTail->offset = tree->canvasPadX[PAD_TOP_LEFT] +
	tree->widthOfColumns;
    tree->columnTail->useWidth = 0; /* hack */

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

    tree->widthOfColumnsLeft = LayoutColumns(tree, tree->columnLockLeft);

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

    tree->widthOfColumnsRight = LayoutColumns(tree, tree->columnLockRight);

    return tree->widthOfColumnsRight;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateColumnCounts --
 *
 *	Calculates the number of visible columns with the same -lock
 *	value, and optionally returns the first visible column in the
 *	group.
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
UpdateColumnCounts(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn first,		/* First column to check. All columns
				 * with the same -lock value are checked. */
    TreeColumn *visPtr,		/* Out: first visible column. */
    int *countVisPtr		/* Out: number of visible columns. */
    )
{
    TreeColumn column;

    if (visPtr != NULL)
	(*visPtr) = NULL;
    (*countVisPtr) = 0;

    for (column = first;
	    column != NULL && column->lock == first->lock;
	    column = column->next) {
	if (column->visible) {
	    if (visPtr != NULL && (*visPtr) == NULL)
		(*visPtr) = column;
	    (*countVisPtr)++;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumns_UpdateCounts --
 *
 *	Recalculates the number of visible columns and the first
 *	visible non-locked column if needed.
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
TreeColumns_UpdateCounts(
    TreeCtrl *tree		/* Widget info. */
    )
{
    int displayLockedColumns = Tree_ShouldDisplayLockedColumns(tree);

    if (tree->displayLockedColumns != displayLockedColumns) {
	tree->columnCountVis = -1;
	tree->displayLockedColumns = displayLockedColumns;
    }

    if (tree->columnCountVis >= 0)
	return;

    UpdateColumnCounts(tree, tree->columnLockNone,
	&tree->columnVis, &tree->columnCountVis);

    if (displayLockedColumns) {
	UpdateColumnCounts(tree, tree->columnLockLeft,
	    NULL, &tree->columnCountVisLeft);
	UpdateColumnCounts(tree, tree->columnLockRight,
	    NULL, &tree->columnCountVisRight);
    } else {
	tree->columnCountVisLeft = 0;
	tree->columnCountVisRight = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumns_InvalidateCounts --
 *
 *	Marks the number of visible columns as out-of-date.
 *	The number of visible columns changes when:
 *	1) creating a column
 *	2) deleting a column
 *	3) moving a column (affects tree->columnVis anyway)
 *	4) column option -visible changes
 *	5) column option -lock changes
 *	6) Tree_ShouldDisplayLockedColumns() changes
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
TreeColumns_InvalidateCounts(
    TreeCtrl *tree		/* Widget info. */
    )
{
    tree->columnCountVis = -1;
    tree->columnCountVisLeft = -1;
    tree->columnCountVisRight = -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_InitWidget --
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
TreeColumn_InitWidget(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeColumn column;

    column = Column_Alloc(tree);
    column->id = -1;
    column->reqData.spanMin = column->reqData.spanMax = column;
    tree->columnTail = column;
    tree->nextColumnId = 0;
    tree->columnCount = 0;
    Column_Config(column, 0, NULL, TRUE);

    tree->columnDrag.optionTable = Tk_CreateOptionTable(tree->interp,
	dragSpecs);
    (void) Tk_InitOptions(tree->interp, (char *) tree,
	tree->columnDrag.optionTable, tree->tkwin);

#ifdef UNIFORM_GROUP
    Tcl_InitHashTable(&tree->uniformGroupHash, TCL_STRING_KEYS);
#endif

    tree->columnPriv = (TreeColumnPriv) ckalloc(sizeof(struct TreeColumnPriv_));
    memset((char *) tree->columnPriv, 0, sizeof(struct TreeColumnPriv_));
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_FreeWidget --
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

void
TreeColumn_FreeWidget(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeColumn column = tree->columns;
    struct TreeColumnPriv_ *priv = tree->columnPriv;

    while (column != NULL) {
	column = Column_Free(column);
    }

    Column_Free(tree->columnTail);
    tree->columnCount = 0;

#ifdef UNIFORM_GROUP
    Tcl_DeleteHashTable(&tree->uniformGroupHash);
#endif

    while (priv->spans != NULL) {
	ColumnSpan *cs = priv->spans;
	priv->spans = cs->next;
	if (cs->spansToRight.spans != NULL)
	    ckfree((char *) cs->spansToRight.spans);
	ckfree((char *) cs);
    }
    while (priv->freeSpans != NULL) {
	ColumnSpan *cs = priv->freeSpans;
	priv->freeSpans = cs->next;
	if (cs->spansToRight.spans != NULL)
	    ckfree((char *) cs->spansToRight.spans);
	ckfree((char *) cs);
    }

    ckfree((char *) priv);
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
    StringTableCO_Init(columnSpecs, "-itemjustify", justifyStrings);
    TreeStyleCO_Init(columnSpecs, "-itemstyle", STATE_DOMAIN_ITEM);

    return TCL_OK;
}
