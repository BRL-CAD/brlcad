/* 
 * tkTreeItem.c --
 *
 *	This module implements items for treectrl widgets.
 *
 * Copyright (c) 2002-2010 Tim Baker
 *
 * RCS: @(#) $Id$
 */

#include "tkTreeCtrl.h"

typedef struct TreeItem_ TreeItem_;
typedef struct Column Column;

/*
 * A data structure of the following type is kept for a single column in a
 * single item.
 */
struct Column {
    int cstate;		/* STATE_xxx flags manipulated with the
			 * [item state forcolumn] command */
    int span;		/* Number of tree-columns this column covers */
    TreeStyle style;	/* Instance style. */
    Column *next;	/* Column to the right of this one */
};

/*
 * A data structure of the following type is kept for each item.
 */
struct TreeItem_ {
    int id;		/* unique id */
    int depth;		/* tree depth (-1 for the unique root item) */
    int fixedHeight;	/* -height: desired height of this item (0 for
			 * no-such-value) */
    int numChildren;
    int index;		/* "row" in flattened tree */
    int indexVis;	/* visible "row" in flattened tree, -1 if hidden */
    int state;		/* STATE_xxx flags */
    TreeItem parent;
    TreeItem firstChild;
    TreeItem lastChild;
    TreeItem prevSibling;
    TreeItem nextSibling;
    TreeItemDInfo dInfo; /* display info, or NULL */
    TreeItemRInfo rInfo; /* range info, or NULL */
    Column *columns;
    int *spans;		/* 1 per tree-column. spans[N] is the column index of
			 * the item-column displayed in column N. If this
			 * item's columns all have a span of 1, this field
			 * is NULL (unless it was previously allocated
			 * because some spans were > 1). */
    int spanAlloc;	/* Size of spans[]. */
#define ITEM_FLAG_DELETED	0x0001 /* Item is being deleted */
#define ITEM_FLAG_SPANS_SIMPLE	0x0002 /* All spans are 1 */
#define ITEM_FLAG_SPANS_VALID	0x0004 /* Some spans are > 1, but we don't
					* need to redo them. Also indicates
					* we have an entry in
					* TreeCtrl.itemSpansHash. */
#define ITEM_FLAG_BUTTON	0x0008 /* -button true */
#define ITEM_FLAG_BUTTON_AUTO	0x0010 /* -button auto */
#define ITEM_FLAG_VISIBLE	0x0020 /* -visible */
#define ITEM_FLAG_WRAP		0x0040 /* -wrap */

#define ITEM_FLAG_BUTTONSTATE_ACTIVE	0x0080 /* buttonstate "active" */
#define ITEM_FLAG_BUTTONSTATE_PRESSED	0x0100 /* buttonstate "pressed" */
    int flags;
    TagInfo *tagInfo;	/* Tags. May be NULL. */
};

#define ITEM_FLAGS_BUTTONSTATE (ITEM_FLAG_BUTTONSTATE_ACTIVE | \
    ITEM_FLAG_BUTTONSTATE_PRESSED)

static CONST char *ItemUid = "Item", *ItemColumnUid = "ItemColumn";

/*
 * Macro to test whether an item is the unique root item
 */
#define IS_ROOT(i) ((i)->depth == -1)

#define IS_ALL(i) ((i) == ITEM_ALL)

#define IS_DELETED(i) (((i)->flags & ITEM_FLAG_DELETED) != 0)
#define IS_VISIBLE(i) (((i)->flags & ITEM_FLAG_VISIBLE) != 0)
#define IS_WRAP(i) (((i)->flags & ITEM_FLAG_WRAP) != 0)

/*
 * Flags returned by Tk_SetOptions() (see itemOptionSpecs below).
 */
#define ITEM_CONF_BUTTON		0x0001
#define ITEM_CONF_SIZE			0x0002
#define ITEM_CONF_VISIBLE		0x0004
#define ITEM_CONF_WRAP			0x0008

/*
 * Information used for Item objv parsing.
 */
static Tk_OptionSpec itemOptionSpecs[] = {
    {TK_OPTION_CUSTOM, "-button", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeItem_, flags),
     0, (ClientData) NULL, ITEM_CONF_BUTTON},
    {TK_OPTION_PIXELS, "-height", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeItem_, fixedHeight),
     TK_OPTION_NULL_OK, (ClientData) NULL, ITEM_CONF_SIZE},
    {TK_OPTION_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeItem_, tagInfo),
     TK_OPTION_NULL_OK, (ClientData) &TreeCtrlCO_tagInfo, 0},
    {TK_OPTION_CUSTOM, "-visible", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeItem_, flags),
     0, (ClientData) NULL, ITEM_CONF_VISIBLE},
    {TK_OPTION_CUSTOM, "-wrap", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeItem_, flags),
     0, (ClientData) NULL, ITEM_CONF_WRAP},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

/*
 *----------------------------------------------------------------------
 *
 * Column_Alloc --
 *
 *	Allocate and initialize a new Column record.
 *
 * Results:
 *	Pointer to allocated Column.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static Column *
Column_Alloc(
    TreeCtrl *tree		/* Widget info. */
    )
{
#ifdef ALLOC_HAX
    Column *column = (Column *) TreeAlloc_Alloc(tree->allocData, ItemColumnUid,
	    sizeof(Column));
#else
    Column *column = (Column *) ckalloc(sizeof(Column));
#endif
    memset(column, '\0', sizeof(Column));
    column->span = 1;
    return column;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemColumn_InvalidateSize --
 *
 *	Marks the needed height and width of the column as out-of-date.
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
TreeItemColumn_InvalidateSize(
    TreeCtrl *tree,		/* Widget info. */
    TreeItemColumn column_	/* Column token. */
    )
{
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemColumn_NeededWidth --
 *
 *	Returns the requested width of a Column.
 *
 * Results:
 *	If the Column has a style, the requested width of the style
 *	is returned (a positive pixel value). Otherwise 0 is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItemColumn_NeededWidth(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    TreeItemColumn column_	/* Column token. */
    )
{
    Column *self = (Column *) column_;

    if (self->style != NULL)
	return TreeStyle_NeededWidth(tree, self->style,
		item->state | self->cstate);
    return 0;
}

#ifdef EXPENSIVE_SPAN_WIDTH /* NOT USED */

/*
 * When a style spans 2 or more columns, all of the requested width goes
 * to the first column in the span. Ideally the width needed by the style
 * would be distributed over each column in the span. To be done properly,
 * the visibility and fixed-width of each column needs to be considered.
 *
 * This routine does an okay job of calculating the requested width of a
 * column in an item when spans are involved; however there is a lot of
 * processing involved, and this routine is called for each column in
 * a span separately (it would be better to calculate the width of all
 * the columns in a span at the same time).
 */

int
TreeItem_NeededWidthOfColumn(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int _columnIndex		/* Column index. */
    )
{
    TreeColumn treeColumn = tree->columns;
    Column *column = item->columns;
    int columnIndex = 0, itemColumnIndex = 0, span = 1;
    int selfIndex = _columnIndex, fixedWidth = 0, width = 0;
    int spanStart, spanEnd;
    struct span {
	Column *itemColumn;
	int itemColumnIndex;
	int fixedWidth;
	int spanWidth;
	int visible;
    } staticSpans[STATIC_SIZE], *spans = staticSpans;

    STATIC_ALLOC(spans, struct span, tree->columnCount);

    while (treeColumn != NULL) {
	spans[columnIndex].visible = TreeColumn_Visible(treeColumn);
	if (--span == 0) {
	    if (spans[columnIndex].visible)
		span = column ? column->span : 1;
	    else
		span = 1;
	    itemColumnIndex = columnIndex;
	}
	spans[columnIndex].itemColumn = column;
	spans[columnIndex].itemColumnIndex = itemColumnIndex;
	spans[columnIndex].spanWidth = 0;
	spans[columnIndex].fixedWidth = TreeColumn_FixedWidth(treeColumn);
	++columnIndex;
	treeColumn = TreeColumn_Next(treeColumn);
	if (column != NULL)
	    column = column->next;
    }

    span = 0;
    columnIndex = spanStart = spanEnd = spans[selfIndex].itemColumnIndex;
    while (spans[columnIndex].itemColumnIndex == spanStart) {
	if (spans[columnIndex].visible) {
	    if (spans[columnIndex].fixedWidth >= 0) {
		fixedWidth += spans[columnIndex].fixedWidth;
		spans[columnIndex].spanWidth = spans[columnIndex].fixedWidth;
	    } else
		++span; /* number of visible columns in the span with no
			 * fixed width */
	}
	spanEnd = columnIndex;
	++columnIndex;
	if (columnIndex == tree->columnCount)
	    break;
    }

    column = spans[spanStart].itemColumn;
    if (column != NULL && column->style != NULL) {
	width = TreeStyle_NeededWidth(tree, column->style,
		item->state | column->cstate);
	if (width > fixedWidth && span > 0) {
	    width -= fixedWidth;
	    while (width > 0) {
		int each = (width > span) ? width / span : 1;
		columnIndex = spanStart;
		while (columnIndex <= spanEnd) {
		    if (spans[columnIndex].visible) {
			if (spans[columnIndex].fixedWidth < 0) {
			    spans[columnIndex].spanWidth += each;
			    width -= each;
			    if (width <= 0)
				break;
			}
		    }
		    ++columnIndex;
		}
	    }
	};
    }

if (0)
{
    int i;
    for (i = 0; i < tree->columnCount; i++) {
	dbwin("%d,%d: itemColumn %p itemColumnIndex %d fixedWidth %d spanWidth %d\n",
	    item->id, i, spans[i].itemColumn, spans[i].itemColumnIndex, spans[i].fixedWidth,
	    spans[i].spanWidth);
    }
}
    width = spans[selfIndex].spanWidth;
    STATIC_FREE(spans, struct span, tree->columnCount);

    return width;
}

#endif /* EXPENSIVE_SPAN_WIDTH */

/*
 *----------------------------------------------------------------------
 *
 * TreeItemColumn_GetStyle --
 *
 *	Returns the style assigned to a Column.
 *
 * Results:
 *	Returns the style, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeStyle
TreeItemColumn_GetStyle(
    TreeCtrl *tree,		/* Widget info. */
    TreeItemColumn column	/* Column token. */
    )
{
    return ((Column *) column)->style;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemColumn_Index --
 *
 *	Return the 0-based index of a Column in an Item's linked list of
 *	Columns.
 *
 * Results:
 *	Integer index of the Column.
 *
 * Side effects:
 *	Tcl_Panic() if the Column isn't found.
 *
 *----------------------------------------------------------------------
 */

int
TreeItemColumn_Index(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    TreeItemColumn column_	/* Column token. */
    )
{
    Column *column;
    int i = 0;

    column = item->columns;
    while ((column != NULL) && (column != (Column *) column_)) {
	i++;
	column = column->next;
    }
    if (column == NULL)
	panic("TreeItemColumn_Index: couldn't find the column\n");
    return i;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemColumn_ForgetStyle --
 *
 *	Free the style assigned to a Column.
 *
 * Results:
 *	Column has no style assigned anymore.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

void
TreeItemColumn_ForgetStyle(
    TreeCtrl *tree,		/* Widget info. */
    TreeItemColumn column_	/* Column token. */
    )
{
    Column *self = (Column *) column_;

    if (self->style != NULL) {
	TreeStyle_FreeResources(tree, self->style);
	self->style = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemColumn_GetNext --
 *
 *	Return the Column to the right of this one.
 *
 * Results:
 *	The next Column in the linked list, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItemColumn
TreeItemColumn_GetNext(
    TreeCtrl *tree,		/* Widget info. */
    TreeItemColumn column	/* Column token. */
    )
{
    return (TreeItemColumn) ((Column *) column)->next;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_FreeResources --
 *
 *	Free the style and memory associated with the given Column.
 *
 * Results:
 *	The next Column in the linked list, or NULL.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

static Column *
Column_FreeResources(
    TreeCtrl *tree,		/* Widget info. */
    Column *self		/* Column to free. */
    )
{
    Column *next = self->next;

    if (self->style != NULL)
	TreeStyle_FreeResources(tree, self->style);
#ifdef ALLOC_HAX
    TreeAlloc_Free(tree->allocData, ItemColumnUid, (char *) self, sizeof(Column));
#else
    WFREE(self, Column);
#endif
    return next;
}

/*
 *----------------------------------------------------------------------
 *
 * Item_UpdateIndex --
 *
 *	Set the Item.depth, Item.index and Item.indexVis fields of the
 *	given Item and all its descendants.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The TreeCtrl.depth field may be updated to track the maximum
 *	depth of all items.
 *
 *----------------------------------------------------------------------
 */

static void
Item_UpdateIndex(TreeCtrl *tree,
    TreeItem item,		/* Item to update. */
    int *index,			/* New Item.index value for the item.
				 * Value is incremented. */
    int *indexVis		/* New Item.indexVis value for the item if
				 * the item is ReallyVisible().
				 * Value is incremented if the item is
				 * ReallyVisible(). */
    )
{
    TreeItem child, parent = item->parent;
    int parentVis, parentOpen;

    /* Also track max depth */
    if (parent != NULL)
	item->depth = parent->depth + 1;
    else
	item->depth = 0;
    if (item->depth > tree->depth)
	tree->depth = item->depth;

    item->index = (*index)++;
    item->indexVis = -1;
    if (parent != NULL) {
	parentOpen = (parent->state & STATE_OPEN) != 0;
	parentVis = parent->indexVis != -1;
	if (IS_ROOT(parent) && !tree->showRoot) {
	    parentOpen = TRUE;
	    parentVis = IS_VISIBLE(parent);
	}
	if (parentVis && parentOpen && IS_VISIBLE(item)) {
	    item->indexVis = (*indexVis)++;
	    if (IS_WRAP(item))
		tree->itemWrapCount++;
	}
	
    }
    child = item->firstChild;
    while (child != NULL) {
	Item_UpdateIndex(tree, child, index, indexVis);
	child = child->nextSibling;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_UpdateItemIndex --
 *
 *	Set the Item.depth, Item.index and Item.indexVis fields of the
 *	every Item. Set TreeCtrl.depth to the maximum depth of all
 *	Items. Set TreeCtrl.itemVisCount to the count of all visible
 *	items.
 *
 *	Because this is slow we try not to do it until necessary.
 *	The tree->updateIndex flags indicates when this is needed.
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
Tree_UpdateItemIndex(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeItem item = tree->root;
    int index = 1, indexVis = 0;

    if (!tree->updateIndex)
	return;

    if (tree->debug.enable && tree->debug.data)
	dbwin("Tree_UpdateItemIndex %s\n", Tk_PathName(tree->tkwin));

    /* Also track max depth */
    tree->depth = -1;

    /* Count visible items with -wrap=true */
    tree->itemWrapCount = 0;

    item->index = 0;
    item->indexVis = -1;
    if (tree->showRoot && IS_VISIBLE(item)) {
	item->indexVis = indexVis++;
	if (IS_WRAP(item))
	    tree->itemWrapCount++;
    }
    item = item->firstChild;
    while (item != NULL) {
	Item_UpdateIndex(tree, item, &index, &indexVis);
	item = item->nextSibling;
    }
    tree->itemVisCount = indexVis;
    tree->updateIndex = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Item_Alloc --
 *
 *	Allocate an initialize a new Item record.
 *
 * Results:
 *	Pointer to the allocated Item record.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static TreeItem
Item_Alloc(
    TreeCtrl *tree		/* Widget info. */
    )
{
#ifdef ALLOC_HAX
    TreeItem item = (TreeItem) TreeAlloc_Alloc(tree->allocData, ItemUid, sizeof(TreeItem_));
#else
    TreeItem item = (TreeItem) ckalloc(sizeof(TreeItem_));
#endif
    memset(item, '\0', sizeof(TreeItem_));
    if (Tk_InitOptions(tree->interp, (char *) item,
		tree->itemOptionTable, tree->tkwin) != TCL_OK)
	panic("Tk_InitOptions() failed in Item_Alloc()");
    item->state =
	STATE_OPEN |
	STATE_ENABLED;
    if (tree->gotFocus)
	item->state |= STATE_FOCUS;
    item->indexVis = -1;
    /* In the typical case all spans are 1. */
    item->flags |= ITEM_FLAG_SPANS_SIMPLE;
    Tree_AddItem(tree, item);
    return item;
}

/*
 *----------------------------------------------------------------------
 *
 * Item_AllocRoot --
 *
 *	Allocate and initialize a new Item record for the root item.
 *
 * Results:
 *	Pointer to the allocated Item record.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static TreeItem
Item_AllocRoot(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeItem item;

    item = Item_Alloc(tree);
    item->depth = -1;
    item->state |= STATE_ACTIVE;
    return item;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetFirstColumn --
 *
 *	Return the first Column record for an Item.
 *
 * Results:
 *	Token for the column, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItemColumn
TreeItem_GetFirstColumn(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    return (TreeItemColumn) item->columns;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetState --
 *
 *	Return the state flags for an Item.
 *
 * Results:
 *	Bit mask of STATE_xxx flags.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetState(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    return item->state;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_ChangeState --
 *
 *	Toggles zero or more STATE_xxx flags for a Column. If the
 *	Column has a style assigned, its state may be changed.
 *
 * Results:
 *	Bit mask of CS_LAYOUT and CS_DISPLAY flags, or zero if no
 *	changes occurred.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

static int
Column_ChangeState(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item containing the column. */
    Column *column,		/* Column to modify the state of. */
    TreeColumn treeColumn,	/* Tree column. */
    int stateOff,		/* STATE_xxx flags to turn off. */
    int stateOn			/* STATE_xxx flags to turn on. */
    )
{
    int cstate, state;
    int sMask, iMask = 0;

    cstate = column->cstate;
    cstate &= ~stateOff;
    cstate |= stateOn;

    if (cstate == column->cstate)
	return 0;

    state = item->state | column->cstate;
    state &= ~stateOff;
    state |= stateOn;

    if (column->style != NULL) {
	sMask = TreeStyle_ChangeState(tree, column->style,
		item->state | column->cstate, state);
	if (sMask) {
	    if (sMask & CS_LAYOUT)
		Tree_InvalidateColumnWidth(tree, treeColumn);
	    iMask |= sMask;
	}

	if (iMask & CS_LAYOUT) {
	    TreeItem_InvalidateHeight(tree, item);
	    TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
	    Tree_FreeItemDInfo(tree, item, NULL);
	    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	} else if (iMask & CS_DISPLAY) {
	    Tree_InvalidateItemDInfo(tree, treeColumn, item, NULL);
	}
    }

    column->cstate = cstate;

    return iMask;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_ChangeState --
 *
 *	Toggles zero or more STATE_xxx flags for an Item. If the
 *	Column has a style assigned, its state may be changed.
 *
 * Results:
 *	Bit mask of CS_LAYOUT and CS_DISPLAY flags, or zero if no
 *	changes occurred.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_ChangeState(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int stateOff,		/* STATE_xxx flags to turn off. */
    int stateOn			/* STATE_xxx flags to turn on. */
    )
{
    Column *column;
    TreeColumn treeColumn;
    int columnIndex = 0, state, cstate;
    int sMask, iMask = 0;

    state = item->state;
    state &= ~stateOff;
    state |= stateOn;

    if (state == item->state)
	return 0;

    treeColumn = tree->columns;
    column = item->columns;
    while (column != NULL) {
	if (column->style != NULL) {
	    cstate = item->state | column->cstate;
	    cstate &= ~stateOff;
	    cstate |= stateOn;
	    sMask = TreeStyle_ChangeState(tree, column->style,
		    item->state | column->cstate, cstate);
	    if (sMask) {
		if (sMask & CS_LAYOUT) {
		    Tree_InvalidateColumnWidth(tree, treeColumn);
		    TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
		} else if (sMask & CS_DISPLAY) {
		    Tree_InvalidateItemDInfo(tree, treeColumn, item, NULL);
		}
		iMask |= sMask;
	    }
	}
	columnIndex++;
	column = column->next;
	treeColumn = TreeColumn_Next(treeColumn);
    }

    /* This item has a button */
    if (TreeItem_HasButton(tree, item)) {

	Tk_Image image1, image2;
	Pixmap bitmap1, bitmap2;
	/* NOTE: These next 2 lines must have 'static' to work around a
	 * Microsoft compiler optimization bug. */
	static int butOpen, butClosed;
	static int themeOpen, themeClosed;
	int w1, h1, w2, h2;
	void *ptr1 = NULL, *ptr2 = NULL;

	/*
	 * Compare the image/bitmap/theme/xlib button for the old state
	 * to the image/bitmap/theme/xlib button for the new state. Figure
	 * out if the size or appearance has changed.
	 */

	/* image > bitmap > theme > draw */
	image1 = PerStateImage_ForState(tree, &tree->buttonImage, item->state, NULL);
	if (image1 != NULL) {
	    Tk_SizeOfImage(image1, &w1, &h1);
	    ptr1 = image1;
	}
	if (ptr1 == NULL) {
	    bitmap1 = PerStateBitmap_ForState(tree, &tree->buttonBitmap, item->state, NULL);
	    if (bitmap1 != None) {
		Tk_SizeOfBitmap(tree->display, bitmap1, &w1, &h1);
		ptr1 = (void *) bitmap1;
	    }
	}
	if (ptr1 == NULL) {
	    if (tree->useTheme &&
		TreeTheme_GetButtonSize(tree, Tk_WindowId(tree->tkwin),
		(item->state & STATE_OPEN) != 0, &w1, &h1) == TCL_OK) {
		ptr1 = (item->state & STATE_OPEN) ? &themeOpen : &themeClosed;
	    }
	}
	if (ptr1 == NULL) {
	    w1 = h1 = tree->buttonSize;
	    ptr1 = (item->state & STATE_OPEN) ? &butOpen : &butClosed;
	}

	/* image > bitmap > theme > draw */
	image2 = PerStateImage_ForState(tree, &tree->buttonImage, state, NULL);
	if (image2 != NULL) {
	    Tk_SizeOfImage(image2, &w2, &h2);
	    ptr2 = image2;
	}
	if (ptr2 == NULL) {
	    bitmap2 = PerStateBitmap_ForState(tree, &tree->buttonBitmap, state, NULL);
	    if (bitmap2 != None) {
		Tk_SizeOfBitmap(tree->display, bitmap2, &w2, &h2);
		ptr2 = (void *) bitmap2;
	    }
	}
	if (ptr2 == NULL) {
	    if (tree->useTheme &&
		TreeTheme_GetButtonSize(tree, Tk_WindowId(tree->tkwin),
		(state & STATE_OPEN) != 0, &w2, &h2) == TCL_OK) {
		ptr2 = (state & STATE_OPEN) ? &themeOpen : &themeClosed;
	    }
	}
	if (ptr2 == NULL) {
	    w2 = h2 = tree->buttonSize;
	    ptr2 = (state & STATE_OPEN) ? &butOpen : &butClosed;
	}

	if ((w1 != w2) || (h1 != h2)) {
	    iMask |= CS_LAYOUT | CS_DISPLAY;
	} else if (ptr1 != ptr2) {
	    iMask |= CS_DISPLAY;
	    if (tree->columnTree != NULL)
		Tree_InvalidateItemDInfo(tree, tree->columnTree, item, NULL);
	}
    }

    if (iMask & CS_LAYOUT) {
	TreeItem_InvalidateHeight(tree, item);
	Tree_FreeItemDInfo(tree, item, NULL);
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
    }

    item->state = state;

    return iMask;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_UndefineState --
 *
 *	Clear a STATE_xxx flag in an Item and its Columns. This is
 *	called when a user-defined state is undefined via the
 *	[state undefine] widget command.
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
TreeItem_UndefineState(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int state			/* STATE_xxx flag that is undefined. */
    )
{
    Column *column = item->columns;

    while (column != NULL) {
	column->cstate &= ~state;
	column = column->next;
    }

    item->state &= ~state;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_HasButton --
 *
 *	Determine whether an item should have a button displayed next to
 *	it. This considers the value of the item option -button as well
 *	as the treectrl options -showbuttons, -showrootchildbuttons and
 *	-showrootbutton.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_HasButton(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    if (!tree->showButtons || (IS_ROOT(item) && !tree->showRootButton))
	return 0;
    if (item->parent == tree->root && !tree->showRootChildButtons)
	return 0;
    if (item->flags & ITEM_FLAG_BUTTON)
	return 1;
    if (item->flags & ITEM_FLAG_BUTTON_AUTO) {
	TreeItem child = item->firstChild;
	while (child != NULL) {
	    if (IS_VISIBLE(child))
		return 1;
	    child = child->nextSibling;
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetButtonBbox --
 *
 *	Determine the bounding box of the expand/collapse button
 *	next to an item.
 *
 * Results:
 *	If the -treecolumn is not visible, or the item is not visible,
 *	or the item has no button, or the bounding box for the item
 *	in the -treecolumn has zero size, then 0 is returned.
 *
 *	Otherwise, the bounding box of the biggest possible button
 *	(considering -buttonbitmap, -buttonimage, -buttonsize and
 *	any themed button) is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetButtonBbox(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    TreeRectangle *tr		/* Returned bounds of the button in
				 * item coordinates. */
    )
{
    TreeItemColumn itemColumn;
    TreeStyle style = NULL;
    int indent, buttonY = -1;

    if (!tree->columnTreeVis)
	return 0;

    if (!TreeItem_HasButton(tree, item))
	return 0;

    /* Get the bounding box in canvas coords of the tree-column for this
     * item. */
    if (TreeItem_GetRects(tree, item, tree->columnTree, 0, NULL, tr) == 0)
	return 0;

    itemColumn = TreeItem_FindColumn(tree, item,
	TreeColumn_Index(tree->columnTree));
    if (itemColumn != NULL)
	style = TreeItemColumn_GetStyle(tree, itemColumn);

    indent = TreeItem_Indent(tree, item);

    if (style != NULL)
	buttonY = TreeStyle_GetButtonY(tree, style);

    /* FIXME? The button is as wide as the -indent option. */
    tr->x = indent - tree->useIndent;
    tr->width = tree->useIndent;

    if (buttonY < 0)
	tr->y = (tr->height - tree->buttonHeightMax) / 2;
    else
	tr->y = buttonY;
    tr->height = tree->buttonHeightMax;

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_IsPointInButton --
 *
 *	Determine if the given point is over the expand/collapse
 *	button next to an item.
 *
 * Results:
 *	1 if the point is over the button, 0 otherwise.
 *
 *	For the purposes of hit-testing, the button is considered to
 *	be larger than what is displayed to make it easier to click.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_IsPointInButton(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int x,			/* Item coords. */
    int y			/* Item coords. */
    )
{
    TreeRectangle tr;
#define BUTTON_HITTEST_SLOP 11
    int centerY, slop = MAX(tree->buttonHeightMax / 2, BUTTON_HITTEST_SLOP);

    if (!TreeItem_GetButtonBbox(tree, item, &tr))
	return 0;

    centerY = TreeRect_Top(tr) + TreeRect_Height(tr) / 2;
    if ((y < centerY - slop) ||
	    (y >= centerY + slop + (tree->buttonHeightMax % 2)))
	return 0;

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetDepth --
 *
 *	Return the depth of an Item.
 *
 * Results:
 *	Integer >= -1. (-1 for the root)
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetDepth(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
#if 0
    Tree_UpdateItemIndex(tree);
#endif
    return item->depth;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetID --
 *
 *	Return the unique ID of an Item.
 *
 * Results:
 *	Integer >= 0. (0 for the root)
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetID(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    return item->id;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_SetID --
 *
 *	Set the unique ID of an Item. This is called when the item
 *	is created.
 *
 * Results:
 *	The given ID.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_SetID(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int id			/* Unique ID for the item. */
    )
{
    return item->id = id;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetEnabled --
 *
 *	Return whether an Item is enabled or not.
 *
 * Results:
 *	TRUE if the item is enabled, FALSE otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetEnabled(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    return (item->state & STATE_ENABLED) != 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetSelected --
 *
 *	Return whether an Item is selected or not.
 *
 * Results:
 *	TRUE if the item is part of the selection, FALSE otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetSelected(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    return (item->state & STATE_SELECTED) != 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_CanAddToSelection --
 *
 *	Return whether an Item is selectable or not.
 *
 * Results:
 *	TRUE if the item can be added to the selection, FALSE otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_CanAddToSelection(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    if (TreeItem_GetSelected(tree, item))
	return FALSE;

    if (!TreeItem_GetEnabled(tree, item))
	return FALSE;

#ifdef SELECTION_VISIBLE
    if (!TreeItem_ReallyVisible(tree, item))
	return FALSE;
#endif

    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetParent --
 *
 *	Return the parent of an Item.
 *
 * Results:
 *	Token for parent Item, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_GetParent(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    return item->parent;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetWrap --
 *
 *	Return whether an Item -wrap is TRUE or FALSE.
 *
 * Results:
 *	TRUE if the item should wrap, FALSE otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetWrap(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    return (item->flags & ITEM_FLAG_WRAP) != 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetNextSibling --
 *
 *	Return the next sibling of an Item.
 *
 * Results:
 *	Token for next sibling Item, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_GetNextSibling(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    return item->nextSibling;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_NextSiblingVisible --
 *
 *	Find a following sibling that is ReallyVisible().
 *
 * Results:
 *	Token for a sibling Item, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_NextSiblingVisible(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    item = TreeItem_GetNextSibling(tree, item);
    while (item != NULL) {
	if (TreeItem_ReallyVisible(tree, item))
	    return item;
	item = TreeItem_GetNextSibling(tree, item);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_SetDInfo --
 *
 *	Store a display-info token in an Item. Called by the display
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
TreeItem_SetDInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    TreeItemDInfo dInfo		/* Display-info token. */
    )
{
    item->dInfo = dInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetDInfo --
 *
 *	Return the display-info token of an Item. Called by the display
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

TreeItemDInfo
TreeItem_GetDInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    return item->dInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_SetRInfo --
 *
 *	Store a range-info token in an Item. Called by the display
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
TreeItem_SetRInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    TreeItemRInfo rInfo		/* Range-info token */
    )
{
    item->rInfo = rInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetRInfo --
 *
 *	Return the range-info token of an Item. Called by the display
 *	code.
 *
 * Results:
 *	The range-info token or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItemRInfo
TreeItem_GetRInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    return item->rInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Next --
 *
 *	Return the Item after the given one.
 *
 * Results:
 *	Result will be:
 *	  a) the first child, or
 *	  b) the next sibling, or
 *	  c) the next sibling of an ancestor, or
 *	  d) NULL if no item follows this one
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_Next(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    if (item->firstChild != NULL)
	return item->firstChild;
    if (item->nextSibling != NULL)
	return item->nextSibling;
    while (1) {
	item = item->parent;
	if (item == NULL)
	    break;
	if (item->nextSibling != NULL)
	    return item->nextSibling;
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_NextVisible --
 *
 *	Return the ReallyVisible() Item after the given one.
 *
 * Results:
 *	Result will be:
 *	  a) the first ReallyVisible() child, or
 *	  b) the next ReallyVisible() sibling, or
 *	  c) the next ReallyVisible() sibling of an ancestor, or
 *	  d) NULL if no ReallyVisible() item follows this one
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_NextVisible(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    item = TreeItem_Next(tree, item);
    while (item != NULL) {
	if (TreeItem_ReallyVisible(tree, item))
	    return item;
	item = TreeItem_Next(tree, item);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Prev --
 *
 *	Return the Item before the given one.
 *
 * Results:
 *	Result will be:
 *	  a) the last descendant of the previous sibling, or
 *	  b) the parent, or
 *	  c) NULL if the given Item is the root
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_Prev(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    TreeItem walk;

    if (item->parent == NULL) /* root */
	return NULL;
    walk = item->parent;
    if (item->prevSibling) {
	walk = item->prevSibling;
	while (walk->lastChild != NULL)
	    walk = walk->lastChild;
    }
    return walk;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_PrevVisible --
 *
 *	Return the ReallyVisible() Item before the given one.
 *
 * Results:
 *	Result will be:
 *	  a) the last descendant of the previous sibling, or
 *	  b) the parent, or
 *	  c) NULL if the given Item is the root
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_PrevVisible(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    item = TreeItem_Prev(tree, item);
    while (item != NULL) {
	if (TreeItem_ReallyVisible(tree, item))
	    return item;
	item = TreeItem_Prev(tree, item);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_ToIndex --
 *
 *	Return Item.index and Item.indexVis.
 *
 * Results:
 *	The zero-based indexes of the Item.
 *
 * Side effects:
 *	Will recalculate the indexes of every item if needed.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_ToIndex(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int *index,			/* Returned Item.index, may be NULL */
    int *indexVis		/* Returned Item.indexVis, may be NULL */
    )
{
    Tree_UpdateItemIndex(tree);
    if (index != NULL) (*index) = item->index;
    if (indexVis != NULL) (*indexVis) = item->indexVis;
}

/*
 *----------------------------------------------------------------------
 *
 * ItemHasTag --
 *
 *	Checks whether an item has a certain tag.
 *
 * Results:
 *	Returns TRUE if the item has the given tag.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ItemHasTag(
    TreeItem item,		/* The item to test. */
    Tk_Uid tag			/* Tag to look for. */
    )
{
    TagInfo *tagInfo = item->tagInfo;
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
    int visible;		/* 1 if the item must be ReallyVisible(),
				   0 if the item must not be ReallyVisible(),
				   -1 for unspecified. */
    int states[3];		/* Item states that must be on or off. */
    TagExpr expr;		/* Tag expression. */
    int exprOK;			/* TRUE if expr is valid. */
    int depth;			/* >= 0 for depth, -1 for unspecified */
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
    q->depth = -1;
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
	"depth", "state", "tag", "visible", "!visible", NULL
    };
    enum qualEnum {
	QUAL_DEPTH, QUAL_STATE, QUAL_TAG, QUAL_VISIBLE, QUAL_NOT_VISIBLE
    };
    /* Number of arguments used by qualifiers[]. */
    static int qualArgs[] = {
	2, 2, 2, 1, 1
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
	    case QUAL_DEPTH: {
		if (Tcl_GetIntFromObj(interp, objv[j + 1], &q->depth) != TCL_OK)
		    goto errorExit;
		break;
	    }
	    case QUAL_STATE: {
		if (Tree_StateFromListObj(tree, objv[j + 1], q->states,
			SFO_NOT_TOGGLE) != TCL_OK)
		    goto errorExit;
		break;
	    }
	    case QUAL_TAG: {
		if (tree->itemTagExpr) {
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
    TreeItem item		/* The item to test. May be NULL. */
    )
{
    TreeCtrl *tree = q->tree;

    /* Note: if the item is NULL it is a "match" because we have run
     * out of items to check. */
    if (item == NULL)
	return 1;
    if ((q->visible == 1) && !TreeItem_ReallyVisible(tree, item))
	return 0;
    else if ((q->visible == 0) && TreeItem_ReallyVisible(tree, (TreeItem) item))
	return 0;
    if (q->states[STATE_OP_OFF] & item->state)
	return 0;
    if ((q->states[STATE_OP_ON] & item->state) != q->states[STATE_OP_ON])
	return 0;
    if (q->exprOK && !TagExpr_Eval(&q->expr, item->tagInfo))
	return 0;
    if ((q->depth >= 0) && (item->depth + 1 != q->depth))
	return 0;
    if ((q->tag != NULL) && !ItemHasTag(item, q->tag))
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
 * TreeItemList_FromObj --
 *
 *	Parse a Tcl_Obj item description to get a list of items.
 *
 *   -- returning a single item --
 *   "active MODIFIERS"
 *   "anchor MODIFIERS"
 *   "nearest x y MODIFIERS"
 *   "root MODIFIERS"
 *   "first QUALIFIERS MODIFIERS"
 *   "last QUALIFIERS MODIFIERS"
 *   "end QUALIFIERS MODIFIERS"
 *   "rnc row col MODIFIERS"
 *   "ID MODIFIERS"
 *   -- returning multiple items --
 *   all QUALIFIERS
 *   QUALIFIERS (like "all QUALIFIERS")
 *   list listOfItemDescs
 *   range QUALIFIERS
 *   tag tagExpr QUALIFIERS
 *   "TAG-EXPR QUALFIERS"
 *
 *   MODIFIERS:
 *   -- returning a single item --
 *   above
 *   below
 *   left
 *   right
 *   top
 *   bottom
 *   leftmost
 *   rightmost
 *   next QUALIFIERS
 *   prev QUALIFIERS
 *   parent
 *   firstchild QUALIFIERS
 *   lastchild QUALIFIERS
 *   child N|end?-offset? QUALIFIERS
 *   nextsibling QUALIFIERS
 *   prevsibling QUALIFIERS
 *   sibling N|end?-offset? QUALIFIERS
 *   -- returning multiple items --
 *   ancestors QUALIFIERS
 *   children QUALIFIERS
 *   descendants QUALIFIERS
 *
 *   QUALIFIERS:
 *   depth integer
 *   state stateList
 *   tag tagExpr
 *   visible
 *   !visible
 *
 *   Examples:
 *   $T item id "first visible firstchild"
 *   $T item id "first visible firstchild visible"
 *   $T item id "nearest x y nextsibling visible"
 *   $T item id "last visible state enabled" 
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
TreeItemList_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,		/* Object to parse to a list of items. */
    TreeItemList *items,	/* Uninitialized item list. Caller must free
				 * it with TreeItemList_Free unless the
				 * result of this function is TCL_ERROR. */
    int flags			/* IFO_xxx flags */
    )
{
    Tcl_Interp *interp = tree->interp;
    int i, objc, index, listIndex, id;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_Obj **objv, *elemPtr;
    TreeItem item = NULL;
    Qualifiers q;
    int qualArgsTotal;

    static CONST char *indexName[] = {
	"active", "all", "anchor", "end", "first", "last", "list",
	"nearest", "range", "rnc", "root", (char *) NULL
    };
    enum indexEnum {
	INDEX_ACTIVE, INDEX_ALL, INDEX_ANCHOR, INDEX_END, INDEX_FIRST,
	INDEX_LAST, INDEX_LIST, INDEX_NEAREST, INDEX_RANGE, INDEX_RNC,
	INDEX_ROOT
    };
    /* Number of arguments used by indexName[]. */
    static int indexArgs[] = {
	1, 1, 1, 1, 1, 1, 2, 3, 3, 3, 1
    };
    /* Boolean: can indexName[] be followed by 1 or more qualifiers. */
    static int indexQual[] = {
	0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1
    };

    static CONST char *modifiers[] = {
	"above", "ancestors", "below", "bottom", "child", "children",
	"descendants", "firstchild", "lastchild", "left", "leftmost", "next",
	"nextsibling", "parent", "prev", "prevsibling", "right", "rightmost",
	"sibling", "top", (char *) NULL
    };
    enum modEnum {
	TMOD_ABOVE, TMOD_ANCESTORS, TMOD_BELOW, TMOD_BOTTOM, TMOD_CHILD,
	TMOD_CHILDREN, TMOD_DESCENDANTS, TMOD_FIRSTCHILD, TMOD_LASTCHILD,
	TMOD_LEFT, TMOD_LEFTMOST, TMOD_NEXT, TMOD_NEXTSIBLING, TMOD_PARENT,
	TMOD_PREV, TMOD_PREVSIBLING, TMOD_RIGHT, TMOD_RIGHTMOST, TMOD_SIBLING,
	TMOD_TOP
    };
    /* Number of arguments used by modifiers[]. */
    static int modArgs[] = {
	1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1
    };
    /* Boolean: can modifiers[] be followed by 1 or more qualifiers. */
    static int modQual[] = {
	0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0
    };

    TreeItemList_Init(tree, items, 0);
    Qualifiers_Init(tree, &q);

    if (Tcl_ListObjGetElements(NULL, objPtr, &objc, &objv) != TCL_OK)
	goto baditem;
    if (objc == 0)
	goto baditem;

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
	    case INDEX_ACTIVE: {
		item = tree->activeItem;
		break;
	    }
	    case INDEX_ALL: {
		if (qualArgsTotal) {
		    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		    while (hPtr != NULL) {
			item = (TreeItem) Tcl_GetHashValue(hPtr);
			if (Qualifies(&q, item)) {
			    TreeItemList_Append(items, (TreeItem) item);
			}
			hPtr = Tcl_NextHashEntry(&search);
		    }
		    item = NULL;
		} else if (flags & IFO_LIST_ALL) {
		    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		    while (hPtr != NULL) {
			item = (TreeItem) Tcl_GetHashValue(hPtr);
			TreeItemList_Append(items, item);
			hPtr = Tcl_NextHashEntry(&search);
		    }
		    item = NULL;
		} else {
		    item = ITEM_ALL;
		}
		break;
	    }
	    case INDEX_ANCHOR: {
		item = tree->anchorItem;
		break;
	    }
	    case INDEX_FIRST: {
		item = tree->root;
		while (!Qualifies(&q, item))
		    item = TreeItem_Next(tree, item);
		break;
	    }
	    case INDEX_END:
	    case INDEX_LAST: {
		item = tree->root;
		while (item->lastChild) {
		    item = item->lastChild;
		}
		while (!Qualifies(&q, item))
		    item = TreeItem_Prev(tree, item);
		break;
	    }
	    case INDEX_LIST: {
		int listObjc;
		Tcl_Obj **listObjv;
		int count;

		if (Tcl_ListObjGetElements(interp, objv[listIndex + 1],
			&listObjc, &listObjv) != TCL_OK) {
		    goto errorExit;
		}
		for (i = 0; i < listObjc; i++) {
		    TreeItemList item2s;
		    if (TreeItemList_FromObj(tree, listObjv[i], &item2s, flags)
			    != TCL_OK)
			goto errorExit;
		    TreeItemList_Concat(items, &item2s);
		    TreeItemList_Free(&item2s);
		}
		/* If any of the item descriptions in the list is "all", then
		 * clear the list of items and use "all". */
		count = TreeItemList_Count(items);
		for (i = 0; i < count; i++) {
		    TreeItem item2 = TreeItemList_Nth(items, i);
		    if (IS_ALL(item2))
			break;
		}
		if (i < count) {
		    TreeItemList_Free(items);
		    item = ITEM_ALL;
		} else
		    item = NULL;
		break;
	    }
	    case INDEX_NEAREST: {
		int x, y;

		if (Tk_GetPixelsFromObj(interp, tree->tkwin,
			objv[listIndex + 1], &x) != TCL_OK) {
		    goto errorExit;
		}
		if (Tk_GetPixelsFromObj(interp, tree->tkwin,
			objv[listIndex + 2], &y) != TCL_OK) {
		    goto errorExit;
		}
		item = Tree_ItemUnderPoint(tree, &x, &y, TRUE);
		break;
	    }
	    case INDEX_RANGE: {
		TreeItem itemFirst, itemLast;

		if (TreeItem_FromObj(tree, objv[listIndex + 1], &itemFirst,
			IFO_NOT_NULL) != TCL_OK)
		    goto errorExit;
		if (TreeItem_FromObj(tree, objv[listIndex + 2], &itemLast,
			IFO_NOT_NULL) != TCL_OK)
		    goto errorExit;
		if (TreeItem_FirstAndLast(tree, &itemFirst, &itemLast) == 0)
		    goto errorExit;
		while (1) {
		    if (Qualifies(&q, itemFirst)) {
			TreeItemList_Append(items, itemFirst);
		    }
		    if (itemFirst == itemLast)
			break;
		    itemFirst = TreeItem_Next(tree, itemFirst);
		}
		item = NULL;
		break;
	    }
	    case INDEX_RNC: {
		int row, col;

		if (Tcl_GetIntFromObj(interp, objv[listIndex + 1], &row) != TCL_OK)
		    goto errorExit;
		if (Tcl_GetIntFromObj(interp, objv[listIndex + 2], &col) != TCL_OK)
		    goto errorExit;
		item = Tree_RNCToItem(tree, row, col);
		break;
	    }
	    case INDEX_ROOT: {
		item = tree->root;
		break;
	    }
	}

	listIndex += indexArgs[index] + qualArgsTotal;

    /* No indexName[] was found. */
    } else {
	int gotId = FALSE;
	TagExpr expr;

	/* Try an itemPrefix + item ID. */
	if (tree->itemPrefixLen) {
	    char *end, *t = Tcl_GetString(elemPtr);
	    if (strncmp(t, tree->itemPrefix, tree->itemPrefixLen) == 0) {
		t += tree->itemPrefixLen;
		id = strtoul(t, &end, 10);
		if ((end != t) && (*end == '\0'))
		    gotId = TRUE;
	    }

	/* Try an item ID. */
	} else if (Tcl_GetIntFromObj(NULL, elemPtr, &id) == TCL_OK) {
	    gotId = TRUE;
	}
	if (gotId) {
	    hPtr = Tcl_FindHashEntry(&tree->itemHash, (char *) INT2PTR(id));
	    if (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashValue(hPtr);
	    } else {
		item = NULL;
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
	    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		if (Qualifies(&q, item)) {
		    TreeItemList_Append(items, item);
		}
		hPtr = Tcl_NextHashEntry(&search);
	    }
	    item = NULL;
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
	if (tree->itemTagExpr) {
	    if (TagExpr_Init(tree, elemPtr, &expr) != TCL_OK)
		goto errorExit;
	    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		if (TagExpr_Eval(&expr, item->tagInfo) && Qualifies(&q, item)) {
		    TreeItemList_Append(items, item);
		}
		hPtr = Tcl_NextHashEntry(&search);
	    }
	    TagExpr_Free(&expr);
	} else {
	    Tk_Uid tag = Tk_GetUid(Tcl_GetString(elemPtr));
	    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		if (ItemHasTag(item, tag) && Qualifies(&q, item)) {
		    TreeItemList_Append(items, item);
		}
		hPtr = Tcl_NextHashEntry(&search);
	    }
	}
	item = NULL;
	listIndex += 1 + qualArgsTotal;
    }

gotFirstPart:
    /* If 1 item, use it and clear the list. */
    if (TreeItemList_Count(items) == 1) {
	item = TreeItemList_Nth(items, 0);
	items->count = 0;
    }

    /* If "all" but only root exists, use it. */
    if (IS_ALL(item) && (tree->itemCount == 1) && !(flags & IFO_NOT_ROOT)) {
	item = tree->root;
    }

    /* If > 1 item, no modifiers may follow. */
    if ((TreeItemList_Count(items) > 1) || IS_ALL(item)) {
	if (listIndex < objc) {
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

    /* This means a valid specification was given, but there is no such item */
    if ((TreeItemList_Count(items) == 0) && (item == NULL)) {
	if (flags & IFO_NOT_NULL)
	    goto noitem;
	/* Empty list returned */
	goto goodExit;
    }

    /* Process any modifiers following the item we matched above. */
    for (; listIndex < objc; /* nothing */) {

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

	qualArgsTotal = 0;
	if (modQual[index]) {
	    Qualifiers_Free(&q);
	    Qualifiers_Init(tree, &q);
	    if (Qualifiers_Scan(&q, objc, objv, listIndex + modArgs[index],
		    &qualArgsTotal) != TCL_OK) {
		goto errorExit;
	    }
	}

	switch ((enum modEnum) index) {
	    case TMOD_ABOVE: {
		item = Tree_ItemAbove(tree, item);
		break;
	    }
	    case TMOD_ANCESTORS: {
		item = item->parent;
		while (item != NULL) {
		    if (Qualifies(&q, item)) {
			TreeItemList_Append(items, item);
		    }
		    item = item->parent;
		}
		item = NULL;
		break;
	    }
	    case TMOD_BELOW: {
		item = Tree_ItemBelow(tree, item);
		break;
	    }
	    case TMOD_BOTTOM: {
		item = Tree_ItemBottom(tree, item);
		break;
	    }
	    case TMOD_CHILD: {
		int n, endRelative;

		if (Tree_GetIntForIndex(tree, objv[listIndex + 1], &n,
			&endRelative) != TCL_OK) {
		    goto errorExit;
		}
		if (endRelative) {
		    item = item->lastChild;
		    while (item != NULL) {
			if (Qualifies(&q, item))
			    if (n-- <= 0)
				break;
			item = item->prevSibling;
		    }
		} else {
		    item = item->firstChild;
		    while (item != NULL) {
			if (Qualifies(&q, item))
			    if (n-- <= 0)
				break;
			item = item->nextSibling;
		    }
		}
		break;
	    }
	    case TMOD_CHILDREN: {
		item = item->firstChild;
		while (item != NULL) {
		    if (Qualifies(&q, item)) {
			TreeItemList_Append(items, item);
		    }
		    item = item->nextSibling;
		}
		item = NULL;
		break;
	    }
	    case TMOD_DESCENDANTS: {
		TreeItem last = item;

		while (last->lastChild != NULL)
		    last = last->lastChild;
		item = item->firstChild;
		while (item != NULL) {
		    if (Qualifies(&q, item)) {
			TreeItemList_Append(items, item);
		    }
		    if (item == last)
			break;
		    item = TreeItem_Next(tree, item);
		}
		item = NULL;
		break;
	    }
	    case TMOD_FIRSTCHILD: {
		item = item->firstChild;
		while (!Qualifies(&q, item))
		    item = item->nextSibling;
		break;
	    }
	    case TMOD_LASTCHILD: {
		item = item->lastChild;
		while (!Qualifies(&q, item))
		    item = item->prevSibling;
		break;
	    }
	    case TMOD_LEFT: {
		item = Tree_ItemLeft(tree, item);
		break;
	    }
	    case TMOD_LEFTMOST: {
		item = Tree_ItemLeftMost(tree, item);
		break;
	    }
	    case TMOD_NEXT: {
		item = TreeItem_Next(tree, item);
		while (!Qualifies(&q, item))
		    item = TreeItem_Next(tree, item);
		break;
	    }
	    case TMOD_NEXTSIBLING: {
		item = item->nextSibling;
		while (!Qualifies(&q, item))
		    item = item->nextSibling;
		break;
	    }
	    case TMOD_PARENT: {
		item = item->parent;
		break;
	    }
	    case TMOD_PREV: {
		item = TreeItem_Prev(tree, item);
		while (!Qualifies(&q, item))
		    item = TreeItem_Prev(tree, item);
		break;
	    }
	    case TMOD_PREVSIBLING: {
		item = item->prevSibling;
		while (!Qualifies(&q, item))
		    item = item->prevSibling;
		break;
	    }
	    case TMOD_RIGHT: {
		item = Tree_ItemRight(tree, item);
		break;
	    }
	    case TMOD_RIGHTMOST: {
		item = Tree_ItemRightMost(tree, item);
		break;
	    }
	    case TMOD_SIBLING: {
		int n, endRelative;

		if (Tree_GetIntForIndex(tree, objv[listIndex + 1], &n,
			&endRelative) != TCL_OK) {
		    goto errorExit;
		}
		item = item->parent;
		if (item == NULL)
		    break;
		if (endRelative) {
		    item = item->lastChild;
		    while (item != NULL) {
			if (Qualifies(&q, item))
			    if (n-- <= 0)
				break;
			item = item->prevSibling;
		    }
		} else {
		    item = item->firstChild;
		    while (item != NULL) {
			if (Qualifies(&q, item))
			    if (n-- <= 0)
				break;
			item = item->nextSibling;
		    }
		}
		break;
	    }
	    case TMOD_TOP: {
		item = Tree_ItemTop(tree, item);
		break;
	    }
	}
	if ((TreeItemList_Count(items) > 1) || IS_ALL(item)) {
	    int end = listIndex + modArgs[index] + qualArgsTotal;
	    if (end < objc) {
		Tcl_AppendResult(interp, "unexpected arguments after \"",
		    (char *) NULL);
		for (i = 0; i < end; i++) {
		    Tcl_AppendResult(interp, Tcl_GetString(objv[i]), (char *) NULL);
		    if (i != end - 1)
			Tcl_AppendResult(interp, " ", (char *) NULL);
		}
		Tcl_AppendResult(interp, "\"", (char *) NULL);
		goto errorExit;
	    }
	}
	if ((TreeItemList_Count(items) == 0) && (item == NULL)) {
	    if (flags & IFO_NOT_NULL)
		goto noitem;
	    /* Empty list returned. */
	    goto goodExit;
	}
	listIndex += modArgs[index] + qualArgsTotal;
    }
    if ((flags & IFO_NOT_MANY) && (IS_ALL(item) ||
	    (TreeItemList_Count(items) > 1))) {
	FormatResult(interp, "can't specify > 1 item for this command");
	goto errorExit;
    }
    if (TreeItemList_Count(items)) {
	if (flags & (IFO_NOT_ROOT | IFO_NOT_ORPHAN)) {
	    int i;
	    for (i = 0; i < TreeItemList_Count(items); i++) {
		item = TreeItemList_Nth(items, i);
		if (IS_ROOT(item) && (flags & IFO_NOT_ROOT))
		    goto notRoot;
		if ((item->parent == NULL) && (flags & IFO_NOT_ORPHAN))
		    goto notOrphan;
	    }
	}
    } else if (IS_ALL(item)) {
	TreeItemList_Append(items, ITEM_ALL);
    } else {
	if (IS_ROOT(item) && (flags & IFO_NOT_ROOT)) {
notRoot:
	    FormatResult(interp, "can't specify \"root\" for this command");
	    goto errorExit;
	}
	if ((item->parent == NULL) && (flags & IFO_NOT_ORPHAN)) {
notOrphan:
	    FormatResult(interp, "item \"%s\" has no parent",
		    Tcl_GetString(objPtr));
	    goto errorExit;
	}
	TreeItemList_Append(items, item);
    }
goodExit:
    Qualifiers_Free(&q);
    return TCL_OK;

baditem:
    Tcl_AppendResult(interp, "bad item description \"", Tcl_GetString(objPtr),
	    "\"", NULL);
    goto errorExit;

noitem:
    Tcl_AppendResult(interp, "item \"", Tcl_GetString(objPtr),
	    "\" doesn't exist", NULL);

errorExit:
    Qualifiers_Free(&q);
    TreeItemList_Free(items);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_FromObj --
 *
 *	Parse a Tcl_Obj item description to get a single item.
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
TreeItem_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,		/* Object to parse to an item. */
    TreeItem *itemPtr,		/* Returned item. */
    int flags			/* IFO_xxx flags */
    )
{
    TreeItemList items;

    if (TreeItemList_FromObj(tree, objPtr, &items, flags | IFO_NOT_MANY) != TCL_OK)
	return TCL_ERROR;
    /* May be NULL. */
    (*itemPtr) = TreeItemList_Nth(&items, 0);
    TreeItemList_Free(&items);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemForEach_Start --
 *
 *	Begin iterating over items. A command might accept two item
 *	descriptions for a range of items, or a single item description
 *	which may itself refer to multiple items. Either item
 *	description could be ITEM_ALL.
 *
 * Results:
 *	Returns the first item to iterate over. If an error occurs
 *	then ItemForEach.error is set to 1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItemForEach_Start(
    TreeItemList *items,	/* List of items. */
    TreeItemList *item2s,	/* List of items or NULL. */
    ItemForEach *iter		/* Returned info, pass to
				   TreeItemForEach_Next. */
    )
{
    TreeCtrl *tree = items->tree;
    TreeItem item, item2 = NULL;

    item = TreeItemList_Nth(items, 0);
    if (item2s)
	item2 = TreeItemList_Nth(item2s, 0);

    iter->tree = tree;
    iter->all = FALSE;
    iter->error = 0;
    iter->items = NULL;

    if (IS_ALL(item) || IS_ALL(item2)) {
	Tcl_HashEntry *hPtr = Tcl_FirstHashEntry(&tree->itemHash, &iter->search);
	iter->all = TRUE;
	return iter->item = (TreeItem) Tcl_GetHashValue(hPtr);
    }

    if (item2 != NULL) {
	if (TreeItem_FirstAndLast(tree, &item, &item2) == 0) {
	    iter->error = 1;
	    return NULL;
	}
	iter->last = item2;
	return iter->item = item;
    }

    iter->items = items;
    iter->index = 0;
    return iter->item = item;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemForEach_Next --
 *
 *	Returns the next item to iterate over. Keep calling this until
 *	the result is NULL.
 *
 * Results:
 *	Returns the next item to iterate over or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItemForEach_Next(
    ItemForEach *iter		/* Initialized by TreeItemForEach_Start. */
    )
{
    TreeCtrl *tree = iter->tree;

    if (iter->all) {
	Tcl_HashEntry *hPtr = Tcl_NextHashEntry(&iter->search);
	if (hPtr == NULL)
	    return iter->item = NULL;
	return iter->item = (TreeItem) Tcl_GetHashValue(hPtr);
    }

    if (iter->items != NULL) {
	if (iter->index >= TreeItemList_Count(iter->items))
	    return iter->item = NULL;
	return iter->item = TreeItemList_Nth(iter->items, ++iter->index);
    }

    if (iter->item == iter->last)
	return iter->item = NULL;
    return iter->item = TreeItem_Next(tree, iter->item);
}

/*
 *----------------------------------------------------------------------
 *
 * Item_ToggleOpen --
 *
 *	Inverts the STATE_OPEN flag of an Item.
 *
 * Results:
 *	Items may be displayed/undisplayed.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

static void
Item_ToggleOpen(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item record. */
    int stateOff,		/* STATE_OPEN or 0 */
    int stateOn			/* STATE_OPEN or 0 */
    )
{
    int mask;

    mask = TreeItem_ChangeState(tree, item, stateOff, stateOn);

    if (IS_ROOT(item) && !tree->showRoot)
	return;

#if 0
    /* Don't affect display if we weren't visible */
    if (!TreeItem_ReallyVisible(tree, item))
	return;

    /* Invalidate display info for this item, so it is redrawn later. */
    Tree_InvalidateItemDInfo(tree, item, NULL);
#endif

    if (item->numChildren > 0) {
	/* indexVis needs updating for all items after this one, if we
	 * have any visible children */
	tree->updateIndex = 1;
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

	/* Hiding/showing children may change the width of any column */
	Tree_InvalidateColumnWidth(tree, NULL);
    }

    /* If this item was previously onscreen, this call is repetitive. */
    Tree_EventuallyRedraw(tree);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_OpenClose --
 *
 *	Inverts the STATE_OPEN flag of an Item.
 *
 * Results:
 *	Items may be displayed/undisplayed.
 *
 * Side effects:
 *	Display changes. <Expand> and <Collapse> events may be
 *	generated.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_OpenClose(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int mode			/* -1: toggle
				 * 0: close
				 * 1: open */
    )
{
    int stateOff = 0, stateOn = 0;

    /* When processing a list of items, any <Expand> or <Collapse> event
     * may result in items being deleted. */
    if (IS_DELETED(item)) return;

    if (mode == -1) {
	if (item->state & STATE_OPEN)
	    stateOff = STATE_OPEN;
	else
	    stateOn = STATE_OPEN;
    } else if (!mode && (item->state & STATE_OPEN))
	stateOff = STATE_OPEN;
    else if (mode && !(item->state & STATE_OPEN))
	stateOn = STATE_OPEN;

    if (stateOff != stateOn) {
	TreeNotify_OpenClose(tree, item, stateOn, TRUE);
	if (IS_DELETED(item)) return;
	Item_ToggleOpen(tree, item, stateOff, stateOn);
	TreeNotify_OpenClose(tree, item, stateOn, FALSE);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Delete --
 *
 *	Recursively frees resources associated with an Item and its
 *	descendants.
 *
 * Results:
 *	Items are removed from their parent and freed.
 *
 * Side effects:
 *	Memory is freed. If the active item or selection-anchor item
 *	is deleted, the root becomes the active/anchor item.
 *	Display changes may occur.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_Delete(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    if (TreeItem_ReallyVisible(tree, item))
	Tree_InvalidateColumnWidth(tree, NULL);

    while (item->numChildren > 0)
	TreeItem_Delete(tree, item->firstChild);

    TreeItem_RemoveFromParent(tree, item);
    TreeDisplay_ItemDeleted(tree, item);
    TreeGradient_ItemDeleted(tree, item);
    TreeTheme_ItemDeleted(tree, item);
    Tree_RemoveItem(tree, item);
    TreeItem_FreeResources(tree, item);
    if (tree->activeItem == item) {
	tree->activeItem = tree->root;
	TreeItem_ChangeState(tree, tree->activeItem, 0, STATE_ACTIVE);
    }
    if (tree->anchorItem == item)
	tree->anchorItem = tree->root;
    if (tree->debug.enable && tree->debug.data)
	Tree_Debug(tree);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Deleted --
 *
 *	Return 1 if the given item is deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_Deleted(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    return IS_DELETED(item);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_FirstAndLast --
 *
 *	Determine the order of two items and swap them if needed.
 *
 * Results:
 *	If the items do not share a common ancestor, 0 is returned and
 *	an error message is left in the interpreter result.
 *	Otherwise the return value is the number of items in the
 *	range between first and last.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_FirstAndLast(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem *first,		/* Item token. */
    TreeItem *last		/* Item token. */
    )
{
    int indexFirst, indexLast, index;

    if (TreeItem_RootAncestor(tree, *first) !=
	    TreeItem_RootAncestor(tree, *last)) {
	FormatResult(tree->interp,
		"item %s%d and item %s%d don't share a common ancestor",
		tree->itemPrefix, TreeItem_GetID(tree, *first),
		tree->itemPrefix, TreeItem_GetID(tree, *last));
	return 0;
    }
    TreeItem_ToIndex(tree, *first, &indexFirst, NULL);
    TreeItem_ToIndex(tree, *last, &indexLast, NULL);
    if (indexFirst > indexLast) {
	TreeItem item = *first;
	*first = *last;
	*last = item;

	index = indexFirst;
	indexFirst = indexLast;
	indexLast = index;
    }
    return indexLast - indexFirst + 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_ListDescendants --
 *
 *	Appends descendants of an item to a list of items.
 *
 * Results:
 *	List of items may grow.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_ListDescendants(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    TreeItemList *items		/* List of items to append descendants to. */
    )
{
    TreeItem last;

    if (item->firstChild == NULL)
	return;
    last = item;
    while (last->lastChild != NULL)
	last = last->lastChild;
    item = item->firstChild;
    while (1) {
	TreeItemList_Append(items, item);
	if (item == last)
	    break;
	item = TreeItem_Next(tree, item);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_UpdateDepth --
 *
 *	Recursively updates Item.depth of an Item and its
 *	descendants.
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
TreeItem_UpdateDepth(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    TreeItem child;

    if (IS_ROOT(item))
	return;
    if (item->parent != NULL)
	item->depth = item->parent->depth + 1;
    else
	item->depth = 0;
    child = item->firstChild;
    while (child != NULL) {
	TreeItem_UpdateDepth(tree, child);
	child = child->nextSibling;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_AddToParent --
 *
 *	Called *after* an Item is added as a child to another Item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_AddToParent(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    TreeItem last, parent = item->parent;

    /* If this is the new last child, redraw the lines of the previous
     * sibling and all of its descendants so the line from the previous
     * sibling reaches this item */
    if ((item->prevSibling != NULL) &&
	    (item->nextSibling == NULL) &&
	    tree->showLines && (tree->columnTree != NULL)) {
	last = item->prevSibling;
	while (last->lastChild != NULL)
	    last = last->lastChild;
	Tree_InvalidateItemDInfo(tree, tree->columnTree,
		item->prevSibling, last);
    }

    /* Redraw the parent if the parent has "-button auto". */
    if (IS_VISIBLE(item) && (parent->flags & ITEM_FLAG_BUTTON_AUTO) &&
	    tree->showButtons && (tree->columnTree != NULL)) {
	Tree_InvalidateItemDInfo(tree, tree->columnTree, parent,
		NULL);
    }

    tree->updateIndex = 1;
    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

    /* Tree_UpdateItemIndex() also recalcs depth, but in one of my demos
     * I retrieve item depth during list creation. Since Tree_UpdateItemIndex()
     * is slow I will keep depth up-to-date here. */
    TreeItem_UpdateDepth(tree, item);

    Tree_InvalidateColumnWidth(tree, NULL);

    if (tree->debug.enable && tree->debug.data)
	Tree_Debug(tree);
}

/*
 *----------------------------------------------------------------------
 *
 * RemoveFromParentAux --
 *
 *	Recursively update Item.depth, Item.index and Item.indexVis.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

static void
RemoveFromParentAux(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item being removed. */
    int *index			/* New value of Item.index. Is incremented. */
    )
{
    TreeItem child;

    /* Invalidate display info. Don't free it because we may just be
     * moving the item to a new parent. FIXME: if it is being moved,
     * it might not actually need to be redrawn (just copied) */
    if (item->dInfo != NULL)
	Tree_InvalidateItemDInfo(tree, NULL, item, NULL);

    if (item->parent != NULL)
	item->depth = item->parent->depth + 1;
    else
	item->depth = 0;
    item->index = (*index)++;
    item->indexVis = -1;
    child = item->firstChild;
    while (child != NULL) {
	RemoveFromParentAux(tree, child, index);
	child = child->nextSibling;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_RemoveFromParent --
 *
 *	Remove an Item from its parent (if any).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Display changes. 
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_RemoveFromParent(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    TreeItem parent = item->parent;
    TreeItem last;
    int index = 0;

    if (parent == NULL)
	return;

    /* If this is the last child, redraw the lines of the previous
     * sibling and all of its descendants because the line from
     * the previous sibling to us is now gone */
    if ((item->prevSibling != NULL) &&
	    (item->nextSibling == NULL) &&
	    tree->showLines && (tree->columnTree != NULL)) {
	last = item->prevSibling;
	while (last->lastChild != NULL)
	    last = last->lastChild;
	Tree_InvalidateItemDInfo(tree, tree->columnTree,
		item->prevSibling, last);
    }

    /* Redraw the parent if the parent has "-button auto". */
    if (IS_VISIBLE(item) && (parent->flags & ITEM_FLAG_BUTTON_AUTO) &&
	    tree->showButtons && (tree->columnTree != NULL)) {
	Tree_InvalidateItemDInfo(tree, tree->columnTree, parent,
		NULL);
    }

    /*
     * Set a flag indicating that item indexes are out-of-date. This doesn't
     * cover the current item being removed.
     */
    tree->updateIndex = 1;
    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

    if (item->prevSibling)
	item->prevSibling->nextSibling = item->nextSibling;
    if (item->nextSibling)
	item->nextSibling->prevSibling = item->prevSibling;
    if (parent->firstChild == item) {
	parent->firstChild = item->nextSibling;
	if (!parent->firstChild)
	    parent->lastChild = NULL;
    }
    if (parent->lastChild == item)
	parent->lastChild = item->prevSibling;
    item->prevSibling = item->nextSibling = NULL;
    item->parent = NULL;
    parent->numChildren--;

    /*
     * Update Item.depth, Item.index and Item.indexVis for the item and its
     * descendants. An up-to-date Item.index is needed for some operations that
     * use a range of items, such as [item delete].
     */
    RemoveFromParentAux(tree, item, &index);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_RemoveColumns --
 *
 *	Free a range of Columns in an Item.
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
TreeItem_RemoveColumns(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int first,			/* 0-based column index at start of
				 * the range. Must be <= last */
    int last			/* 0-based column index at end of
				 * the range. Must be >= first */
    )
{
    Column *column = item->columns;
    Column *prev = NULL, *next = NULL;
    int i = 0;

    while (column != NULL) {
	next = column->next;
	if (i == first - 1)
	    prev = column;
	else if (i >= first)
	    Column_FreeResources(tree, column);
	if (i == last)
	    break;
	++i;
	column = next;
    }
    if (prev != NULL)
	prev->next = next;
    else if (first == 0)
	item->columns = next;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_RemoveAllColumns --
 *
 *	Free all the Columns in an Item.
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
TreeItem_RemoveAllColumns(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    Column *column = item->columns;

    while (column != NULL) {
	Column *next = column->next;
	Column_FreeResources(tree, column);
	column = next;
    }
    item->columns = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Item_CreateColumn --
 *
 *	Allocate a Column record for an Item if it doesn't already
 *	exist.
 *
 * Results:
 *	Pointer to new or existing Column record.
 *
 * Side effects:
 *	Any column records preceding the desired one are allocated
 *	if they weren't already. Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static Column *
Item_CreateColumn(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item to contain the column. */
    int columnIndex,		/* 0-based index of new column. */
    int *isNew			/* May be NULL. Set to TRUE if the
				 * column record was created. */
    )
{
    Column *column;
    int i;

    if (isNew != NULL) (*isNew) = FALSE;
    column = item->columns;
    if (column == NULL) {
	column = Column_Alloc(tree);
	item->columns = column;
	if (isNew != NULL) (*isNew) = TRUE;
    }
    for (i = 0; i < columnIndex; i++) {
	if (column->next == NULL) {
	    column->next = Column_Alloc(tree);
	    if (isNew != NULL) (*isNew) = TRUE;
	}
	column = column->next;
    }

    return column;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_MoveColumn --
 *
 *	Rearranges an Item's list of Column records by moving one
 *	in front of another.
 *
 * Results:
 *	If the Column to be moved does not exist and the Column to place it
 *	in front of does not exist, then nothing happens. If the Column is
 *	to be moved past all currently allocated Columns, then new
 *	Column records are allocated.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_MoveColumn(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int columnIndex,		/* 0-based index of the column to move. */
    int beforeIndex		/* 0-based index of the second column to move
				 * the first column to the left of. */
    )
{
    Column *before = NULL, *move = NULL;
    Column *prevM = NULL, *prevB = NULL;
    Column *last = NULL, *prev, *walk;
    int index = 0;

    prev = NULL;
    walk = item->columns;
    while (walk != NULL) {
	if (index == columnIndex) {
	    prevM = prev;
	    move = walk;
	}
	if (index == beforeIndex) {
	    prevB = prev;
	    before = walk;
	}
	prev = walk;
	if (walk->next == NULL)
	    last = walk;
	index++;
	walk = walk->next;
    }

    if (move == NULL && before == NULL)
	return;
    if (move == NULL)
	move = Column_Alloc(tree);
    else {
	if (before == NULL) {
	    prevB = Item_CreateColumn(tree, item, beforeIndex - 1, NULL);
	    last = prevB;
	}
	if (prevM == NULL)
	    item->columns = move->next;
	else
	    prevM->next = move->next;
    }
    if (before == NULL) {
	last->next = move;
	move->next = NULL;
    } else {
	if (prevB == NULL)
	    item->columns = move;
	else
	    prevB->next = move;
	move->next = before;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_FreeResources --
 *
 *	Free memory etc assocated with an Item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated. Display changes.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_FreeResources(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    Column *column;

    column = item->columns;
    while (column != NULL)
	column = Column_FreeResources(tree, column);
    if (item->dInfo != NULL)
	Tree_FreeItemDInfo(tree, item, NULL);
    if (item->rInfo != NULL)
	Tree_FreeItemRInfo(tree, item);
    if (item->spans != NULL)
	ckfree((char *) item->spans);
    Tk_FreeConfigOptions((char *) item, tree->itemOptionTable, tree->tkwin);

    /* Add the item record to the "preserved" list. It will be freed later. */
    TreeItemList_Append(&tree->preserveItemList, item);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Release --
 *
 *	Finally free an item record when it is no longer needed.
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
TreeItem_Release(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
#ifdef ALLOC_HAX
    TreeAlloc_Free(tree->allocData, ItemUid, (char *) item, sizeof(TreeItem_));
#else
    WFREE(item, TreeItem_);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Item_UseHeight --
 *
 *	Return the height used by styles in an Item.
 *
 * Results:
 *	Maximum height in pixels of the style in each visible Column.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Item_HeightOfStyles(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item record. */
    )
{
    Column *column = item->columns;
    TreeColumn treeColumn = tree->columns;
    StyleDrawArgs drawArgs;
    int height = 0;

    drawArgs.tree = tree;

    while (column != NULL) {
	if (TreeColumn_Visible(treeColumn) && (column->style != NULL)) {
	    drawArgs.state = item->state | column->cstate;
	    drawArgs.style = column->style;
	    drawArgs.indent = (treeColumn == tree->columnTree) ?
		TreeItem_Indent(tree, item) : 0;
	    if ((TreeColumn_FixedWidth(treeColumn) != -1) ||
		    TreeColumn_Squeeze(treeColumn)) {
		drawArgs.width = TreeColumn_UseWidth(treeColumn);
	    } else
		drawArgs.width = -1;
	    height = MAX(height, TreeStyle_UseHeight(&drawArgs));
	}
	treeColumn = TreeColumn_Next(treeColumn);
	column = column->next;
    }

    return height;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Height --
 *
 *	Return the height of an Item.
 *
 * Results:
 *	If the Item -height option is > 0, the result is the maximum
 *	of the button height (if a button is displayed) and the -height
 *	option.
 *	If the TreeCtrl -itemheight option is > 0, the result is the maximum
 *	of the button height (if a button is displayed) and the -itemheight
 *	option.
 *	Otherwise the result is the maximum of the button height (if a button
 *	is displayed) AND the TreeCtrl -minitemheight AND the height of
 *	the style in each visible Column.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_Height(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    int buttonHeight = 0;
    int useHeight;

    if (!IS_VISIBLE(item) || (IS_ROOT(item) && !tree->showRoot))
	return 0;

    /* Get requested height of the style in each column */
    useHeight = Item_HeightOfStyles(tree, item);

    /* Can't have less height than our button */
    if (TreeItem_HasButton(tree, item)) {
	buttonHeight = Tree_ButtonHeight(tree, item->state);
    }

    /* User specified a fixed height for this item */
    if (item->fixedHeight > 0)
	return MAX(item->fixedHeight, buttonHeight);

    /* Fixed height of all items */
    if (tree->itemHeight > 0)
	return MAX(tree->itemHeight, buttonHeight);

    /* Minimum height of all items */
    if (tree->minItemHeight > 0)
	useHeight = MAX(useHeight, tree->minItemHeight);

    /* No fixed height specified */
    return MAX(useHeight, buttonHeight);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_InvalidateHeight --
 *
 *	Marks Item.neededHeight out-of-date.
 *	NOTE: Item.neededHeight is unused.
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
TreeItem_InvalidateHeight(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
}

/*
 *----------------------------------------------------------------------
 *
 * Item_FindColumn --
 *
 *	Return a Column record given a zero-based index.
 *
 * Results:
 *	The Column record or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Column *
Item_FindColumn(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item record. */
    int columnIndex		/* 0-based index of column to find. */
    )
{
    Column *column;
    int i = 0;

    column = item->columns;
    if (!column)
	return NULL;
    while (column != NULL && i < columnIndex) {
	column = column->next;
	i++;
    }
    return column;
}

/*
 *----------------------------------------------------------------------
 *
 * Item_FindColumnFromObj --
 *
 *	Return a Column record given a Tcl_Obj column description.
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
Item_FindColumnFromObj(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item record. */
    Tcl_Obj *obj,		/* Column description. */
    Column **columnPtr,		/* Returned column, or NULL. */
    int *indexPtr		/* May be NULL. Returned 0-based index of
				 * the column. */
    )
{
    TreeColumn treeColumn;
    int columnIndex;

    if (TreeColumn_FromObj(tree, obj, &treeColumn, CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
	return TCL_ERROR;
    columnIndex = TreeColumn_Index(treeColumn);
    (*columnPtr) = Item_FindColumn(tree, item, columnIndex);
    if (indexPtr != NULL)
	(*indexPtr) = columnIndex;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_FindColumn --
 *
 *	Return an item-column token given a zero-based index.
 *
 * Results:
 *	The item-column token or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItemColumn
TreeItem_FindColumn(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int columnIndex		/* 0-based index of column to find. */
    )
{
    return (TreeItemColumn) Item_FindColumn(tree, item, columnIndex);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_ColumnFromObj --
 *
 *	Return an item-column token given a Tcl_Obj column description.
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
TreeItem_ColumnFromObj(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    Tcl_Obj *obj,		/* Column description. */
    TreeItemColumn *columnPtr,	/* Returned column, or NULL. */
    int *indexPtr		/* May be NULL. Returned 0-based index of
				 * the column. */
    )
{
    return Item_FindColumnFromObj(tree, item, obj, (Column **) columnPtr, indexPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Indent --
 *
 *	Return the amount of indentation for the given item. This is
 *	the width of the buttons/lines. 
 *
 * Results:
 *	Pixel value >= 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_Indent(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    int depth;

    if (IS_ROOT(item))
	return (tree->showRoot && tree->showButtons && tree->showRootButton)
	    ? tree->useIndent : 0;

    Tree_UpdateItemIndex(tree);

    depth = item->depth;
    if (tree->showRoot)
    {
	depth += 1;
	if (tree->showButtons && tree->showRootButton)
	    depth += 1;
    }
    else if (tree->showButtons && tree->showRootChildButtons)
	depth += 1;
    else if (tree->showLines && tree->showRootLines)
	depth += 1;

    return tree->useIndent * depth;
}

/*
 *----------------------------------------------------------------------
 *
 * ItemDrawBackground --
 *
 *	Draws part of the background area of an Item. The area is
 *	erased to the -itembackground color of the tree column or the
 *	TreeCtrl -background color. If the TreeCtrl -backgroundimage
 *	option is specified then that image is tiled over the given area.
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
ItemDrawBackground(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn treeColumn,	/* Tree-column token. */
    TreeItem item,		/* Item record. */
    Column *column,		/* First column. */
    TreeDrawable td,		/* Where to draw. */
    int x, int y,		/* Area of the item to draw. */
    int width, int height,	/* ^ */
    int index			/* Used to select a color from the
				 * tree-column's -itembackground option. */
    )
{
    TreeColor *tc;
    TreeClip clip, *clipPtr = &clip;
    TreeRectangle tr;

#if USE_ITEM_PIXMAP == 0
    clip.type = TREE_CLIP_AREA;
    switch (TreeColumn_Lock(treeColumn)) {
	case COLUMN_LOCK_LEFT:
	    clip.area = TREE_AREA_LEFT;
	    break;
	case COLUMN_LOCK_NONE:
	    clip.area = TREE_AREA_CONTENT;
	    break;
	case COLUMN_LOCK_RIGHT:
	    clip.area = TREE_AREA_RIGHT;
	    break;
    }
#else
    clipPtr = NULL;
#endif

    TreeRect_SetXYWH(tr, x, y, width, height);

    /*
     * If the -backgroundimage is being drawn and is opaque,
     * there is no need to erase first (unless it doesn't tile!).
     */
    if (!Tree_IsBgImageOpaque(tree)) {
	tc = TreeColumn_BackgroundColor(treeColumn, index);
	if (tc != NULL) {
	    TreeRectangle trBrush;
	    TreeColor_GetBrushBounds(tree, tc, tr,
		    tree->drawableXOrigin, tree->drawableYOrigin,
		    treeColumn, item, &trBrush);
	    if (!TreeColor_IsOpaque(tree, tc)
		    || (trBrush.width <= 0) || (trBrush.height <= 0)) {
		GC gc = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);
		Tree_FillRectangle(tree, td, clipPtr, gc, tr);
	    }
	    TreeColor_FillRect(tree, td, clipPtr, tc, trBrush, tr);
	} else {
	    GC gc = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);
	    Tree_FillRectangle(tree, td, clipPtr, gc, tr);
	}
    }
    if (tree->backgroundImage != NULL) {
	Tree_DrawBgImage(tree, td, tr, tree->drawableXOrigin,
		tree->drawableYOrigin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_SpansInvalidate --
 *
 *	Invalidates the Item.spans field of one or all items.
 *
 * Results:
 *	The item(s) are removed from the TreeCtrl.itemSpansHash to
 *	indicate that the list of spans must be recalculated.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_SpansInvalidate(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. NULL for all items. */
    )
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    int count = 0;

    if (item == NULL) {
	hPtr = Tcl_FirstHashEntry(&tree->itemSpansHash, &search);
	while (hPtr != NULL) {
	    item = (TreeItem) Tcl_GetHashKey(&tree->itemSpansHash, hPtr);
	    item->flags &= ~ITEM_FLAG_SPANS_VALID;
	    count++;
	    hPtr = Tcl_NextHashEntry(&search);
	}
	if (count) {
	    Tcl_DeleteHashTable(&tree->itemSpansHash);
	    Tcl_InitHashTable(&tree->itemSpansHash, TCL_ONE_WORD_KEYS);
	}
    } else if (item->flags & ITEM_FLAG_SPANS_VALID) {
	hPtr = Tcl_FindHashEntry(&tree->itemSpansHash, (char *) item);
	Tcl_DeleteHashEntry(hPtr);
	item->flags &= ~ITEM_FLAG_SPANS_VALID;
	count++;
    }

    if (count && tree->debug.enable && tree->debug.span)
	dbwin("TreeItem_SpansInvalidate forgot %d items\n", count);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_SpansRedo --
 *
 *	Updates the Item.spans field of an item.
 *
 * Results:
 *	Item.spans is resized if needed to (at least) the current number
 *	of tree columns. For tree column N, the index of the item
 *	column displayed there is written to spans[N].
 *
 *	The return value is 1 if every span is 1, otherwise 0.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_SpansRedo(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    TreeColumn treeColumn = tree->columns;
    Column *itemColumn = item->columns;
    int columnIndex = 0, spanner = 0, span = 1, simple = TRUE;
    int lock = TreeColumn_Lock(treeColumn);

    if (tree->debug.enable && tree->debug.span)
	dbwin("TreeItem_SpansRedo item %d\n", item->id);

    if (item->spans == NULL) {
	item->spans = (int *) ckalloc(sizeof(int) * tree->columnCount);
	item->spanAlloc = tree->columnCount;
    } else if (item->spanAlloc < tree->columnCount) {
	item->spans = (int *) ckrealloc((char *) item->spans,
		sizeof(int) * tree->columnCount);
	item->spanAlloc = tree->columnCount;
    }

    while (treeColumn != NULL) {
	/* End current span if column lock changes. */
	if (TreeColumn_Lock(treeColumn) != lock) {
	    lock = TreeColumn_Lock(treeColumn);
	    span = 1;
	}
	if (--span == 0) {
	    if (TreeColumn_Visible(treeColumn))
		span = itemColumn ? itemColumn->span : 1;
	    else
		span = 1;
	    if (span > 1)
		simple = FALSE;
	    spanner = columnIndex;
	}
	item->spans[columnIndex] = spanner;
	columnIndex++;
	treeColumn = TreeColumn_Next(treeColumn);
	if (itemColumn != NULL)
	    itemColumn = itemColumn->next;
    }

    return simple;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_SpansRedoIfNeeded --
 *
 *	Updates the Item.spans field of an item if needed.
 *
 * Results:
 *	If all spans are known to be 1, nothing is done. If the list of
 *	spans is marked valid, nothing is done. Otherwise the list of
 *	spans is recalculated; if any span is > 1 the item is added
 *	to TreeCtrl.itemSpansHash.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_SpansRedoIfNeeded(
    TreeCtrl *tree,
    TreeItem item
    )
{
    /* All the spans are 1. */
    if (item->flags & ITEM_FLAG_SPANS_SIMPLE)
	return;

    /* Some spans > 1, but we calculated them already. */
    if (item->flags & ITEM_FLAG_SPANS_VALID)
	return;

    if (TreeItem_SpansRedo(tree, item)) {
	/* Reverted to all spans=1. */
	item->flags |= ITEM_FLAG_SPANS_SIMPLE;
    } else {
	int isNew;
	Tcl_HashEntry *hPtr;

	hPtr = Tcl_CreateHashEntry(&tree->itemSpansHash, (char *) item, &isNew);
	Tcl_SetHashValue(hPtr, (ClientData) item);
	item->flags |= ITEM_FLAG_SPANS_VALID;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetSpans --
 *
 *	Returns the spans[] array for an item.
 *
 * Results:
 *	If all spans are known to be 1, the result is NULL. Otherwise the
 *	list of spans is returned.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

int *
TreeItem_GetSpans(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    TreeItem_SpansRedoIfNeeded(tree, item);
    if (item->flags & ITEM_FLAG_SPANS_SIMPLE)
	return NULL;
    return item->spans;
}

/*
 * The following structure holds information about which item column
 * is displayed at a given tree column.
 */
typedef struct SpanInfo {
    TreeColumn treeColumn;	/* Always non-null. */
    TreeItemColumn itemColumn;	/* May be null. */
    int span;			/* Number of tree-columns spanned. */
    int width;			/* Width of the span. */
} SpanInfo;

/*
 *----------------------------------------------------------------------
 *
 * Item_GetSpans --
 *
 *	Fills an array of SpanInfo records, one per visible span.
 *
 * Results:
 *	The return value is the number of SpanInfo records written.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Item_GetSpans(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    TreeColumn firstColumn,	/* Which columns. */
    SpanInfo spans[]		/* Returned span records. */
    )
{
    TreeColumn treeColumn = firstColumn;
    int columnIndex = TreeColumn_Index(firstColumn);
    Column *column = Item_FindColumn(tree, item, columnIndex);
    int spanCount = 0, span = 1;
    SpanInfo *spanPtr = NULL;

    while (treeColumn != NULL) {
	if (TreeColumn_Lock(treeColumn) != TreeColumn_Lock(firstColumn))
	    break;
	if (--span == 0) {
	    if (TreeColumn_Visible(treeColumn)) {
		span = column ? column->span : 1;
		if (spanPtr == NULL)
		    spanPtr = spans;
		else
		    spanPtr++;
		spanPtr->treeColumn = treeColumn;
		spanPtr->itemColumn = (TreeItemColumn) column;
		spanPtr->span = 0;
		spanPtr->width = 0;
		spanCount++;
	    } else {
		span = 1;
		goto next;
	    }
	}
	spanPtr->span++;
	spanPtr->width += TreeColumn_UseWidth(treeColumn);
next:
	++columnIndex;
	treeColumn = TreeColumn_Next(treeColumn);
	if (column != NULL)
	    column = column->next;
    }
    return spanCount;
}

typedef int (*TreeItemWalkSpansProc)(
    TreeCtrl *tree,
    TreeItem item,
    SpanInfo *spanPtr,
    StyleDrawArgs *drawArgs,
    ClientData clientData
    );

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_WalkSpans --
 *
 *	Iterates over the spans of an item and calls a callback routine
 *	for each span of non-zero width. This is used for drawing,
 *	hit-testing and other purposes.
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
TreeItem_WalkSpans(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int lock,			/* Which columns. */
    int x, int y,		/* Drawable coordinates of the item. */
    int width, int height,	/* Total size of the item. */
    TreeItemWalkSpansProc proc,	/* Callback routine. */
    ClientData clientData	/* Data passed to callback routine. */
    )
{
    int columnWidth, totalWidth;
    Column *itemColumn;
    StyleDrawArgs drawArgs;
    TreeColumn treeColumn = tree->columnLockNone;
    int spanCount, spanIndex, columnCount = tree->columnCountVis;
    SpanInfo staticSpans[STATIC_SIZE], *spans = staticSpans;
    int area = TREE_AREA_CONTENT;

    switch (lock) {
	case COLUMN_LOCK_LEFT:
	    treeColumn = tree->columnLockLeft;
	    columnCount = tree->columnCountVisLeft;
	    area = TREE_AREA_LEFT;
	    break;
	case COLUMN_LOCK_NONE:
	    break;
	case COLUMN_LOCK_RIGHT:
	    treeColumn = tree->columnLockRight;
	    columnCount = tree->columnCountVisRight;
	    area = TREE_AREA_RIGHT;
	    break;
    }

    if (!Tree_AreaBbox(tree, area, &drawArgs.bounds)) {
	TreeRect_SetXYWH(drawArgs.bounds, 0, 0, 0, 0);
    }

    STATIC_ALLOC(spans, SpanInfo, columnCount);
    spanCount = Item_GetSpans(tree, item, treeColumn, spans);

    drawArgs.tree = tree;
    drawArgs.item = item; /* needed for gradients */
    drawArgs.td.drawable = None;

    totalWidth = 0;
    for (spanIndex = 0; spanIndex < spanCount; spanIndex++) {
	treeColumn = spans[spanIndex].treeColumn;
	itemColumn = (Column *) spans[spanIndex].itemColumn;

	/* If this is the single visible column, use the provided width which
	 * may be different than the column's width. */
	if ((tree->columnCountVis == 1) && (treeColumn == tree->columnVis)) {
	    columnWidth = width;

	/* More than one column is visible, or this is not the visible
	 * column. */
	} else {
	    columnWidth = spans[spanIndex].width;
	}
	if (columnWidth <= 0)
	    continue;

	if (itemColumn != NULL) {
	    drawArgs.state = item->state | itemColumn->cstate;
	    drawArgs.style = itemColumn->style; /* may be NULL */
	} else {
	    drawArgs.state = item->state;
	    drawArgs.style = NULL;
	}
	if (treeColumn == tree->columnTree)
	    drawArgs.indent = TreeItem_Indent(tree, item);
	else
	    drawArgs.indent = 0;
	drawArgs.x = x + totalWidth;
	drawArgs.y = y;
	drawArgs.width = columnWidth;
	drawArgs.height = height;
	drawArgs.justify = TreeColumn_ItemJustify(treeColumn);
	drawArgs.column = treeColumn; /* needed for gradients */
	if ((*proc)(tree, item, &spans[spanIndex], &drawArgs, clientData))
	    break;

	totalWidth += columnWidth;
    }

    STATIC_FREE(spans, SpanInfo, columnCount);
}

/*
 *----------------------------------------------------------------------
 *
 * SpanWalkProc_Draw --
 *
 *	Callback routine to TreeItem_WalkSpans for TreeItem_Draw.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in a drawable.
 *
 *----------------------------------------------------------------------
 */

static int
SpanWalkProc_Draw(
    TreeCtrl *tree,
    TreeItem item,
    SpanInfo *spanPtr,
    StyleDrawArgs *drawArgs,
    ClientData clientData
    )
{
    TreeColumn treeColumn = spanPtr->treeColumn;
    Column *itemColumn = (Column *) spanPtr->itemColumn;
#if COLUMNGRID == 1
    TreeColor *leftColor, *rightColor;
    int leftWidth, rightWidth;
#endif
    int i, x;
    struct {
	TreeDrawable td;
	int minX;
	int maxX;
	int index;
    } *data = clientData;

    /* Draw nothing if the entire span is out-of-bounds. */
    if ((drawArgs->x >= data->maxX) ||
	    (drawArgs->x + drawArgs->width <= data->minX))
	return 0;

    drawArgs->td = data->td;

    /* Draw background colors. */
    if (spanPtr->span == 1) {
	/* Important point: use drawArgs->width since an item's width may
	 * be totally different than tree->columnVis' width. */
	ItemDrawBackground(tree, treeColumn, item, itemColumn,
		drawArgs->td, drawArgs->x, drawArgs->y,
		drawArgs->width, drawArgs->height, data->index);
    } else {
	x = drawArgs->x;
	for (i = 0; i < spanPtr->span; i++) {
	    int columnWidth = TreeColumn_UseWidth(treeColumn);
	    if ((columnWidth > 0) && (x < data->maxX) &&
		    (x + columnWidth > data->minX)) {
		ItemDrawBackground(tree, treeColumn, item, itemColumn,
			drawArgs->td, x, drawArgs->y,
			columnWidth, drawArgs->height, data->index);
	    }
	    x += columnWidth;
	    treeColumn = TreeColumn_Next(treeColumn);
	}
    }

    if (drawArgs->style != NULL) {
	StyleDrawArgs drawArgsCopy = *drawArgs;
	TreeStyle_Draw(&drawArgsCopy);
    }

#if COLUMNGRID == 1
    if (TreeColumn_GridColors(spanPtr->treeColumn, &leftColor, &rightColor,
	    &leftWidth, &rightWidth) != 0) {
	TreeRectangle tr, trBrush;

	if (leftColor != NULL && leftWidth > 0) {
	    TreeRect_SetXYWH(tr, drawArgs->x, drawArgs->y, leftWidth,
		    drawArgs->height);
	    TreeColor_GetBrushBounds(tree, leftColor, tr,
		    tree->drawableXOrigin, tree->drawableYOrigin,
		    spanPtr->treeColumn, item, &trBrush);
	    TreeColor_FillRect(tree, data->td, NULL, leftColor, trBrush, tr);
	}
	if (rightColor != NULL && rightWidth > 0) {
	    TreeRect_SetXYWH(tr, drawArgs->x + drawArgs->width - rightWidth,
		    drawArgs->y, rightWidth, drawArgs->height);
	    TreeColor_GetBrushBounds(tree, rightColor, tr,
		    tree->drawableXOrigin, tree->drawableYOrigin,
		    spanPtr->treeColumn, item, &trBrush);
	    TreeColor_FillRect(tree, data->td, NULL, rightColor, trBrush, tr);
	}
    }
#endif

    if (spanPtr->treeColumn == tree->columnTree) {
	if (tree->showLines)
	    TreeItem_DrawLines(tree, item, drawArgs->x, drawArgs->y,
		    drawArgs->width, drawArgs->height, data->td,
		    drawArgs->style);
	if (tree->showButtons)
	    TreeItem_DrawButton(tree, item, drawArgs->x, drawArgs->y,
		    drawArgs->width, drawArgs->height, data->td,
		    drawArgs->style);
    }

    /* Stop walking if we went past the right edge of the dirty area. */
    return drawArgs->x + drawArgs->width >= data->maxX;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Draw --
 *
 *	Draws part of an Item.
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
TreeItem_Draw(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int lock,			/* Which columns. */
    int x, int y,		/* Drawable coordinates of the item. */
    int width, int height,	/* Total size of the item. */
    TreeDrawable td,		/* Where to draw. */
    int minX, int maxX,		/* Left/right edge that needs to be drawn. */
    int index			/* Used to select a color from a
				 * tree-column's -itembackground option. */
    )
{
    struct {
	TreeDrawable td;
	int minX;
	int maxX;
	int index;
    } clientData;

    clientData.td = td;
    clientData.minX = minX;
    clientData.maxX = maxX;
    clientData.index = index;

    TreeItem_WalkSpans(tree, item, lock,
	    x, y, width, height,
	    SpanWalkProc_Draw, (ClientData) &clientData);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_DrawLines --
 *
 *	Draws horizontal and vertical lines indicating parent-child
 *	relationship in an item.
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
TreeItem_DrawLines(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int x, int y,		/* Drawable coordinates of columnTree. */
    int width, int height,	/* Total size of columnTree. */
    TreeDrawable td,		/* Where to draw. */
    TreeStyle style		/* Instance style or NULL. Used to get
				 * optional vertical offset of the button. */
    )
{
    TreeItem parent, walk;
    int buttonY = -1;
    int indent, left, lineLeft, lineTop;
    int hasPrev, hasNext;
    int i, vert = 0;

    indent = TreeItem_Indent(tree, item);

    if (style != NULL)
	buttonY = TreeStyle_GetButtonY(tree, style);

    /* Left edge of button/line area */
    left = x /* + tree->columnTreeLeft */ + indent - tree->useIndent;

    /* Left edge of vertical line */
    lineLeft = left + (tree->useIndent - tree->lineThickness) / 2;

    /* Top edge of horizontal line */
    if (buttonY < 0)
	lineTop = y + (height - tree->lineThickness) / 2;
    else
	lineTop = y + buttonY + (tree->buttonHeightMax - tree->lineThickness) / 2;

    /* NOTE: The next three checks do not call TreeItem_ReallyVisible()
     * since 'item' is ReallyVisible */

    /* Check for ReallyVisible previous sibling */
    walk = item->prevSibling;
    while ((walk != NULL) && !IS_VISIBLE(walk))
	walk = walk->prevSibling;
    hasPrev = (walk != NULL);

    /* Check for ReallyVisible parent */
    if ((item->parent != NULL) && (!IS_ROOT(item->parent) || tree->showRoot))
	hasPrev = TRUE;

    /* Check for ReallyVisible next sibling */
    walk = item->nextSibling;
    while ((walk != NULL) && !IS_VISIBLE(walk))
	walk = walk->nextSibling;
    hasNext = (walk != NULL);

    /* Option: Don't connect children of root item */
    if ((item->parent != NULL) && IS_ROOT(item->parent) && !tree->showRootLines)
	hasPrev = hasNext = FALSE;

    /* Vertical line to parent and/or previous/next sibling */
    if (hasPrev || hasNext) {
	int top = y, bottom = y + height;

	if (!hasPrev)
	    top = lineTop;
	if (!hasNext)
	    bottom = lineTop + tree->lineThickness;

	if (tree->lineStyle == LINE_STYLE_DOT) {
	    for (i = 0; i < tree->lineThickness; i++) {
		Tree_VDotLine(tree, td.drawable,
			lineLeft + i,
			top,
			bottom);
	    }
	} else {
	    XFillRectangle(tree->display, td.drawable, tree->lineGC[0],
		    lineLeft,
		    top,
		    tree->lineThickness,
		    bottom - top);
	}

	/* Don't overlap horizontal line */
	vert = tree->lineThickness;
    }

    /* Horizontal line to self */
    if (hasPrev || hasNext) {
	if (tree->lineStyle == LINE_STYLE_DOT) {
	    for (i = 0; i < tree->lineThickness; i++) {
		Tree_HDotLine(tree, td.drawable,
			lineLeft + vert,
			lineTop + i,
			x /* + tree->columnTreeLeft */ + indent);
	    }
	} else {
	    XFillRectangle(tree->display, td.drawable, tree->lineGC[0],
		    lineLeft + vert,
		    lineTop,
		    left + tree->useIndent - (lineLeft + vert),
		    tree->lineThickness);
	}
    }

    /* Vertical lines from ancestors to their next siblings */
    for (parent = item->parent;
	 parent != NULL;
	 parent = parent->parent) {
	lineLeft -= tree->useIndent;

	/* Option: Don't connect children of root item */
	if ((parent->parent != NULL) && IS_ROOT(parent->parent) && !tree->showRootLines)
	    continue;

	/* Check for ReallyVisible next sibling */
	item = parent->nextSibling;
	while ((item != NULL) && !IS_VISIBLE(item))
	    item = item->nextSibling;

	if (item != NULL) {
	    if (tree->lineStyle == LINE_STYLE_DOT) {
		for (i = 0; i < tree->lineThickness; i++) {
		    Tree_VDotLine(tree, td.drawable,
			    lineLeft + i,
			    y,
			    y + height);
		}
	    } else {
		XFillRectangle(tree->display, td.drawable, tree->lineGC[0],
			lineLeft,
			y,
			tree->lineThickness,
			height);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_DrawButton --
 *
 *	Draws the button (if any) in an item.
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
TreeItem_DrawButton(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int x, int y,		/* Drawable coordinates of columnTree. */
    int width, int height,	/* Total size of columnTree. */
    TreeDrawable td,		/* Where to draw. */
    TreeStyle style		/* Instance style or NULL. Used to get
				 * optional vertical offset of the button. */
    )
{
    int indent, left, lineLeft, lineTop;
    int buttonLeft, buttonTop, buttonY = -1, w1;
    Tk_Image image;
    Pixmap bitmap;

    if (!TreeItem_HasButton(tree, item))
	return;

    indent = TreeItem_Indent(tree, item);

    if (style != NULL)
	buttonY = TreeStyle_GetButtonY(tree, style);

    /* Left edge of button/line area */
    left = x /* + tree->columnTreeLeft */ + indent - tree->useIndent;

    image = PerStateImage_ForState(tree, &tree->buttonImage, item->state, NULL);
    if (image != NULL) {
	int imgW, imgH;
	Tk_SizeOfImage(image, &imgW, &imgH);
	if (buttonY < 0)
	    buttonY = (height - imgH) / 2;
	Tree_RedrawImage(image, 0, 0, imgW, imgH, td,
	    left + (tree->useIndent - imgW) / 2,
	    y + buttonY);
	return;
    }

    bitmap = PerStateBitmap_ForState(tree, &tree->buttonBitmap, item->state, NULL);
    if (bitmap != None) {
	int bmpW, bmpH;
	int bx, by;
	Tk_SizeOfBitmap(tree->display, bitmap, &bmpW, &bmpH);
	if (buttonY < 0)
	    buttonY = (height - bmpH) / 2;
	bx = left + (tree->useIndent - bmpW) / 2;
	by = y + buttonY;
	Tree_DrawBitmap(tree, bitmap, td.drawable, NULL, NULL,
		0, 0, (unsigned int) bmpW, (unsigned int) bmpH,
		bx, by);
	return;
    }

    if (tree->useTheme) {
	int bw, bh;
	int buttonState = item->state;

	/* FIXME: These may overwrite [state define] states */
	buttonState &= ~ITEM_FLAGS_BUTTONSTATE;
	if (item->flags & ITEM_FLAG_BUTTONSTATE_ACTIVE)
	    buttonState |= BUTTON_STATE_ACTIVE;
	if (item->flags & ITEM_FLAG_BUTTONSTATE_PRESSED)
	    buttonState |= BUTTON_STATE_PRESSED;

	if (TreeTheme_GetButtonSize(tree, td.drawable,
		(buttonState & STATE_OPEN) != 0, &bw, &bh) == TCL_OK) {
	    if (buttonY < 0)
		buttonY = (height - bh) / 2;
	    if (TreeTheme_DrawButton(tree, td, item, buttonState,
		    left + (tree->useIndent - bw) / 2, y + buttonY,
		    bw, bh) == TCL_OK) {
		return;
	    }
	}
    }

    w1 = tree->buttonThickness / 2;

    /* Left edge of vertical line */
    /* Make sure this matches TreeItem_DrawLines() */
    lineLeft = left + (tree->useIndent - tree->buttonThickness) / 2;

    /* Top edge of horizontal line */
    /* Make sure this matches TreeItem_DrawLines() */
    if (buttonY < 0)
	lineTop = y + (height - tree->lineThickness) / 2;
    else
	lineTop = y + buttonY + (tree->buttonHeightMax - tree->lineThickness) / 2;

    buttonLeft = left + (tree->useIndent - tree->buttonSize) / 2;
    if (buttonY < 0)
	buttonTop = y + (height - tree->buttonSize) / 2;
    else
	buttonTop = y + buttonY + (tree->buttonHeightMax - tree->buttonSize) / 2;

    /* Erase button background */
    XFillRectangle(tree->display, td.drawable,
	    Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC),
	    buttonLeft + tree->buttonThickness,
	    buttonTop + tree->buttonThickness,
	    tree->buttonSize - tree->buttonThickness,
	    tree->buttonSize - tree->buttonThickness);

    /* Draw button outline */
    XDrawRectangle(tree->display, td.drawable, tree->buttonGC,
	    buttonLeft + w1,
	    buttonTop + w1,
	    tree->buttonSize - tree->buttonThickness,
	    tree->buttonSize - tree->buttonThickness);

    /* Horizontal '-' */
    XFillRectangle(tree->display, td.drawable, tree->buttonGC,
	    buttonLeft + tree->buttonThickness * 2,
	    lineTop,
	    tree->buttonSize - tree->buttonThickness * 4,
	    tree->buttonThickness);

    if (!(item->state & STATE_OPEN)) {
	/* Finish '+' */
	XFillRectangle(tree->display, td.drawable, tree->buttonGC,
		lineLeft,
		buttonTop + tree->buttonThickness * 2,
		tree->buttonThickness,
		tree->buttonSize - tree->buttonThickness * 4);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SpanWalkProc_UpdateWindowPositions --
 *
 *	Callback routine to TreeItem_WalkSpans for
 *	TreeItem_UpdateWindowPositions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Windows in window elements may be resized/repositioned.
 *
 *----------------------------------------------------------------------
 */

static int
SpanWalkProc_UpdateWindowPositions(
    TreeCtrl *tree,
    TreeItem item,
    SpanInfo *spanPtr,
    StyleDrawArgs *drawArgs,
    ClientData clientData
    )
{
    StyleDrawArgs drawArgsCopy;
    int requests;

    if ((drawArgs->x >= TreeRect_Right(drawArgs->bounds)) ||
	    (drawArgs->x + drawArgs->width <= TreeRect_Left(drawArgs->bounds)) ||
	    (drawArgs->style == NULL))
	return 0;

    TreeDisplay_GetReadyForTrouble(tree, &requests);

    drawArgsCopy = *drawArgs; 
    TreeStyle_UpdateWindowPositions(&drawArgsCopy);

    if (TreeDisplay_WasThereTrouble(tree, requests))
	return 1;

    /* Stop walking if we went past the right edge of the display area. */
    return drawArgs->x + drawArgs->width >= TreeRect_Right(drawArgs->bounds);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_UpdateWindowPositions --
 *
 *	Updates the geometry of any on-screen window elements. Called
 *	by the display code when an item was possibly scrolled.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Windows in window elements may be resized/repositioned.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_UpdateWindowPositions(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int lock,			/* Columns we care about. */
    int x, int y,		/* Window coordinates of the item. */
    int width, int height	/* Total size of the item. */
    )
{
    TreeItem_WalkSpans(tree, item, lock,
	    x, y, width, height,
	    SpanWalkProc_UpdateWindowPositions, (ClientData) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_OnScreen --
 *
 *	Called by the display code when the item becomes visible
 *	(i.e., actually displayed) or hidden.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Windows in window elements may be mapped/unmapped.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_OnScreen(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int onScreen		/* TRUE if item is displayed. */
    )
{
#if 0
    Column *column = item->columns;

    while (column != NULL) {
	if (column->style != NULL) {
	    TreeStyle_OnScreen(tree, column->style, onScreen);
	}
	column = column->next;
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_ReallyVisible --
 *
 *	Return whether the given Item could be displayed.
 *
 * Results:
 *	TRUE if the item's -visible is TRUE, all of its ancestors'
 *	-visible options are TRUE, and all of its ancestors are open.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_ReallyVisible(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    TreeItem parent = item->parent;

    if (!tree->updateIndex)
	return item->indexVis != -1;

    if (!IS_VISIBLE(item))
	return 0;
    if (parent == NULL)
	return IS_ROOT(item) ? tree->showRoot : 0;
    if (IS_ROOT(parent)) {
	if (!IS_VISIBLE(parent))
	    return 0;
	if (!tree->showRoot)
	    return 1;
	if (!(parent->state & STATE_OPEN))
	    return 0;
    }
    if (!IS_VISIBLE(parent) || !(parent->state & STATE_OPEN))
	return 0;
    return TreeItem_ReallyVisible(tree, parent);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_RootAncestor --
 *
 *	Return the toplevel ancestor of an Item.
 *
 * Results:
 *	Returns the root, or an orphan ancestor, or the given Item.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_RootAncestor(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    while (item->parent != NULL)
	item = item->parent;
    return item;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_IsAncestor --
 *
 *	Determine if one Item is the ancestor of another.
 *
 * Results:
 *	TRUE if item1 is an ancestor of item2.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_IsAncestor(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item1,		/* Possible ancestor. */
    TreeItem item2		/* Item to check ancestry of. */
    )
{
    if (item1 == item2)
	return 0;
    while (item2 && item2 != item1)
	item2 = item2->parent;
    return item2 != NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_ToObj --
 *
 *	Convert an Item to a Tcl_Obj.
 *
 * Results:
 *	A new Tcl_Obj representing the Item.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TreeItem_ToObj(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    if (tree->itemPrefixLen) {
	char buf[100 + TCL_INTEGER_SPACE];
	(void) sprintf(buf, "%s%d", tree->itemPrefix, item->id);
	return Tcl_NewStringObj(buf, -1);
    }
    return Tcl_NewIntObj(item->id);
}

/*
 *----------------------------------------------------------------------
 *
 * Item_Configure --
 *
 *	This procedure is called to process an objc/objv list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	an Item.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then the interp's result contains an error message.
 *
 * Side effects:
 *	Configuration information gets set for item;  old resources get
 *	freed, if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
Item_Configure(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item to configure. */
    int objc,			/* Number of arguments */
    Tcl_Obj *CONST objv[]	/* Array of arguments */
    )
{
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;
    int mask;
    int lastVisible = IS_VISIBLE(item);
    int lastWrap = IS_WRAP(item);

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) item, tree->itemOptionTable,
			objc, objv, tree->tkwin, &savedOptions, &mask) != TCL_OK) {
		mask = 0;
		continue;
	    }

	    /* xxx */

	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    /* xxx */

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    if (mask & ITEM_CONF_SIZE) {
	Tree_FreeItemDInfo(tree, item, NULL);
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
    }

    if (mask & ITEM_CONF_BUTTON) {
	if (tree->columnTree != NULL)
	    Tree_InvalidateItemDInfo(tree, tree->columnTree, item, NULL);
    }

    if ((mask & ITEM_CONF_VISIBLE) && (IS_VISIBLE(item) != lastVisible)) {

	/* Changing the visibility of an item can change the width of
	 * any column. This is due to column expansion (this item may
	 * be the widest item in the column) and spans > 1. */
	Tree_InvalidateColumnWidth(tree, NULL);

	/* If this is the last child, redraw the lines of the previous
	 * sibling and all of its descendants because the line from
	 * the previous sibling to us is appearing/disappearing. */
	if ((item->prevSibling != NULL) &&
		(item->nextSibling == NULL) &&
		tree->showLines && (tree->columnTree != NULL)) {
	    TreeItem last = item->prevSibling;
	    while (last->lastChild != NULL)
		last = last->lastChild;
	    Tree_InvalidateItemDInfo(tree, tree->columnTree,
		    item->prevSibling,
		    last);
	}

	/* Redraw the parent if the parent has "-button auto". */
	if ((item->parent != NULL) &&
		(item->parent->flags & ITEM_FLAG_BUTTON_AUTO) &&
		tree->showButtons && (tree->columnTree != NULL)) {
	    Tree_InvalidateItemDInfo(tree, tree->columnTree, item->parent,
		    NULL);
	}

	tree->updateIndex = 1;
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES | DINFO_REDO_SELECTION);
    }

    if ((mask & ITEM_CONF_WRAP) && (IS_WRAP(item) != lastWrap)) {
	tree->updateIndex = 1;
	Tree_InvalidateColumnWidth(tree, NULL);
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ItemCreateCmd --
 *
 *	This procedure is invoked to process the [item create] widget
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
ItemCreateCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    static CONST char *optionNames[] = { "-button", "-count", "-enabled",
	"-height", "-nextsibling", "-open", "-parent", "-prevsibling",
	"-returnid", "-tags", "-visible", "-wrap",
	(char *) NULL };
    enum { OPT_BUTTON, OPT_COUNT, OPT_ENABLED, OPT_HEIGHT, OPT_NEXTSIBLING,
	OPT_OPEN, OPT_PARENT, OPT_PREVSIBLING, OPT_RETURNID, OPT_TAGS,
	OPT_VISIBLE, OPT_WRAP };
    int index, i, count = 1, button = 0, returnId = 1, open = 1, visible = 1;
    int enabled = 1, wrap = 0, height = 0;
    TreeItem item, parent = NULL, prevSibling = NULL, nextSibling = NULL;
    TreeItem head = NULL, tail = NULL;
    Tcl_Obj *listObj = NULL, *tagsObj = NULL;
    TagInfo *tagInfo = NULL;
    TreeColumn treeColumn;

    for (i = 3; i < objc; i += 2) {
	if (Tcl_GetIndexFromObj(interp, objv[i], optionNames, "option", 0,
		&index) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (i + 1 == objc) {
	    FormatResult(interp, "missing value for \"%s\" option",
		    optionNames[index]);
	    return TCL_ERROR;
	}
	switch (index) {
	    case OPT_BUTTON: {
		int length;
		char *s = Tcl_GetStringFromObj(objv[i + 1], &length);
		if (s[0] == 'a' && strncmp(s, "auto", length) == 0) {
		    button = ITEM_FLAG_BUTTON_AUTO;
		} else {
		    if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &button) != TCL_OK) {
			FormatResult(interp, "expected boolean or auto but got \"%s\"", s);
			return TCL_ERROR;
		    }
		    if (button) {
			button = ITEM_FLAG_BUTTON;
		    }
		}
		break;
	    }
	    case OPT_COUNT:
		if (Tcl_GetIntFromObj(interp, objv[i + 1], &count) != TCL_OK)
		    return TCL_ERROR;
		if (count <= 0) {
		    FormatResult(interp, "bad count \"%d\": must be > 0",
			    count);
		    return TCL_ERROR;
		}
		break;
	    case OPT_ENABLED:
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &enabled)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    case OPT_HEIGHT:
		if (Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
			&height) != TCL_OK)
		    return TCL_ERROR;
		if (height < 0) {
		    FormatResult(interp, "bad screen distance \"%s\": must be > 0",
			    Tcl_GetString(objv[i + 1]));
		    return TCL_ERROR;
		}
		break;
	    case OPT_NEXTSIBLING:
		if (TreeItem_FromObj(tree, objv[i + 1], &nextSibling,
			IFO_NOT_NULL | IFO_NOT_ROOT | IFO_NOT_ORPHAN) != TCL_OK) {
		    return TCL_ERROR;
		}
		parent = prevSibling = NULL;
		break;
	    case OPT_OPEN:
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &open)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    case OPT_PARENT:
		if (TreeItem_FromObj(tree, objv[i + 1], &parent, IFO_NOT_NULL) != TCL_OK) {
		    return TCL_ERROR;
		}
		prevSibling = nextSibling = NULL;
		break;
	    case OPT_PREVSIBLING:
		if (TreeItem_FromObj(tree, objv[i + 1], &prevSibling,
			IFO_NOT_NULL | IFO_NOT_ROOT | IFO_NOT_ORPHAN) != TCL_OK) {
		    return TCL_ERROR;
		}
		parent = nextSibling = NULL;
		break;
	    case OPT_RETURNID:
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &returnId)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    case OPT_TAGS:
		tagsObj = objv[i + 1];
		break;
	    case OPT_VISIBLE:
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &visible)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    case OPT_WRAP:
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &wrap)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	}
    }

    /* Do it here so I don't have to free it above if an error occurs. */
    if (tagsObj != NULL) {
	if (TagInfo_FromObj(tree, tagsObj, &tagInfo) != TCL_OK)
	    return TCL_ERROR;
    }

    if (returnId)
	listObj = Tcl_NewListObj(0, NULL);

    /* Don't allow non-deleted items to become children of a
    * deleted item. */
    if ((parent && IS_DELETED(parent)) ||
	(prevSibling && IS_DELETED(prevSibling->parent)) ||
	(nextSibling && IS_DELETED(nextSibling->parent)))
	parent = prevSibling = nextSibling = NULL;

    for (i = 0; i < count; i++) {
	item = Item_Alloc(tree);
	item->flags &= ~(ITEM_FLAG_BUTTON | ITEM_FLAG_BUTTON_AUTO);
	item->flags |= button;
	if (enabled) item->state |= STATE_ENABLED;
	else item->state &= ~STATE_ENABLED;
	if (open) item->state |= STATE_OPEN;
	else item->state &= ~STATE_OPEN;
	if (visible) item->flags |= ITEM_FLAG_VISIBLE;
	else item->flags &= ~ITEM_FLAG_VISIBLE;
	if (wrap) item->flags |= ITEM_FLAG_WRAP;
	else item->flags &= ~ITEM_FLAG_WRAP;
	item->fixedHeight = height;

	/* Apply each column's -itemstyle option. */
	for (treeColumn = tree->columns; treeColumn != NULL;
		treeColumn = TreeColumn_Next(treeColumn)) {
	    TreeStyle style = TreeColumn_ItemStyle(treeColumn);
	    if (style != NULL) {
		Column *column = Item_CreateColumn(tree, item,
			TreeColumn_Index(treeColumn), NULL);
		column->style = TreeStyle_NewInstance(tree, style);
	    }
	}
#ifdef DEPRECATED
	/* Apply default styles */
	if (tree->defaultStyle.numStyles) {
	    int i, n = MIN(tree->columnCount, tree->defaultStyle.numStyles);

	    for (i = 0; i < n; i++) {
		Column *column = Item_CreateColumn(tree, item, i, NULL);
		if (column->style != NULL)
		    continue;
		if (tree->defaultStyle.styles[i] != NULL) {
		    column->style = TreeStyle_NewInstance(tree,
			    tree->defaultStyle.styles[i]);
		}
	    }
	}
#endif /* DEPRECATED */

	if (tagInfo != NULL) {
	    if (count == 1) {
		item->tagInfo = tagInfo;
		tagInfo = NULL;
	    } else {
		item->tagInfo = TagInfo_Copy(tree, tagInfo);
	    }
	}

	/* Link the new items together as siblings */
	if (parent || prevSibling || nextSibling) {
	    if (head == NULL)
		head = item;
	    if (tail != NULL) {
		tail->nextSibling = item;
		item->prevSibling = tail;
	    }
	    tail = item;
	}

	if (returnId)
	    Tcl_ListObjAppendElement(interp, listObj, TreeItem_ToObj(tree,
		    item));
    }

    if (parent != NULL) {
	head->prevSibling = parent->lastChild;
	if (parent->lastChild != NULL)
	    parent->lastChild->nextSibling = head;
	else
	    parent->firstChild = head;
	parent->lastChild = tail;
    } else if (prevSibling != NULL) {
	parent = prevSibling->parent;
	if (prevSibling->nextSibling != NULL)
	    prevSibling->nextSibling->prevSibling = tail;
	else
	    parent->lastChild = tail;
	head->prevSibling = prevSibling;
	tail->nextSibling = prevSibling->nextSibling;
	prevSibling->nextSibling = head;
    } else if (nextSibling != NULL) {
	parent = nextSibling->parent;
	if (nextSibling->prevSibling != NULL)
	    nextSibling->prevSibling->nextSibling = head;
	else
	    parent->firstChild = head;
	head->prevSibling = nextSibling->prevSibling;
	tail->nextSibling = nextSibling;
	nextSibling->prevSibling = tail;
    }

    if (parent != NULL) {
	for (item = head; item != NULL; item = item->nextSibling) {
	    item->parent = parent;
	    item->depth = parent->depth + 1;
	}
	parent->numChildren += count;
	TreeItem_AddToParent(tree, head);
    }

    TagInfo_Free(tree, tagInfo);

    if (returnId)
	Tcl_SetObjResult(interp, listObj);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NoStyleMsg --
 *
 *	Utility to set the interpreter result with a message indicating
 *	a Column has no assigned style.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Interpreter result is changed.
 *
 *----------------------------------------------------------------------
 */

static void
NoStyleMsg(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item record. */
    int columnIndex		/* 0-based index of the column that
				 * has no style. */
    )
{
    FormatResult(tree->interp,
	    "item %s%d column %s%d has no style",
	    tree->itemPrefix, item->id,
	    tree->columnPrefix,
	    TreeColumn_GetID(Tree_FindColumn(tree, columnIndex)));
}

/*
 *----------------------------------------------------------------------
 *
 * ItemElementCmd --
 *
 *	This procedure is invoked to process the [item element] widget
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
ItemElementCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    static CONST char *commandNames[] = {
#ifdef DEPRECATED
	"actual",
#endif
	"cget", "configure",
	"perstate", (char *) NULL };
    enum {
#ifdef DEPRECATED
    COMMAND_ACTUAL,
#endif
    COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_PERSTATE };
    int index;
    int columnIndex;
    Column *column;
    TreeItemList itemList;
    TreeItem item;
    int flags = IFO_NOT_NULL;
    int result = TCL_OK;

    if (objc < 7) {
	Tcl_WrongNumArgs(interp, 3, objv, "command item column element ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
		&index) != TCL_OK)
	return TCL_ERROR;

    /*
     * [configure] without an option-value pair can operate on a single item
     * only. [cget] and [perstate] only operate on a single item.
     */
    if ((index != COMMAND_CONFIGURE) || (objc < 9))
	flags |= IFO_NOT_MANY;

    if (TreeItemList_FromObj(tree, objv[4], &itemList, flags) != TCL_OK)
	return TCL_ERROR;
    item = TreeItemList_Nth(&itemList, 0);

    switch (index) {
	/* T item element perstate I C E option ?stateList? */
#ifdef DEPRECATED
	case COMMAND_ACTUAL:
#endif
	case COMMAND_PERSTATE: {
	    int state;

	    if (objc < 8 || objc > 9) {
		Tcl_WrongNumArgs(tree->interp, 4, objv,
			"item column element option ?stateList?");
		result = TCL_ERROR;
		break;
	    }
	    if (Item_FindColumnFromObj(tree, item, objv[5], &column,
		    &columnIndex) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    if ((column == NULL) || (column->style == NULL)) {
		NoStyleMsg(tree, item, columnIndex);
		result = TCL_ERROR;
		break;
	    }
	    state = item->state | column->cstate;
	    if (objc == 9) {
		int states[3];

		if (Tree_StateFromListObj(tree, objv[8], states,
			    SFO_NOT_OFF | SFO_NOT_TOGGLE) != TCL_OK) {
		    result = TCL_ERROR;
		    break;
		}
		state = states[STATE_OP_ON];
	    }
	    result = TreeStyle_ElementActual(tree, column->style,
		    state, objv[6], objv[7]);
	    break;
	}

	/* T item element cget I C E option */
	case COMMAND_CGET: {
	    if (objc != 8) {
		Tcl_WrongNumArgs(tree->interp, 4, objv,
			"item column element option");
		result = TCL_ERROR;
		break;
	    }
	    if (Item_FindColumnFromObj(tree, item, objv[5], &column,
		    &columnIndex) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    if ((column == NULL) || (column->style == NULL)) {
		NoStyleMsg(tree, item, columnIndex);
		result = TCL_ERROR;
		break;
	    }
	    result = TreeStyle_ElementCget(tree, item,
		    (TreeItemColumn) column, column->style, objv[6], objv[7]);
	    break;
	}

	/* T item element configure I C E ... */
	case COMMAND_CONFIGURE: {
	    struct columnObj {
		TreeColumnList columns;
		int isColumn;
		int numArgs;
	    } staticCO[STATIC_SIZE], *co = staticCO;
	    int i, index, indexElem, prevColumn;
	    ItemForEach iter;

	    /* If no option-value pair is given, we can't specify more than
	     * one column. */
	    flags = CFO_NOT_NULL | CFO_NOT_TAIL;
	    if (objc < 9)
		flags |= CFO_NOT_MANY;

	    STATIC_ALLOC(co, struct columnObj, objc);
	    for (i = 5; i < objc; i++) {
		co[i].isColumn = FALSE;
		co[i].numArgs = -1;
	    }
	    indexElem = 6;

	    /* Get the first column(s) */
	    i = indexElem - 1;
	    if (TreeColumnList_FromObj(tree, objv[i], &co[i].columns,
		    flags) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    co[i].isColumn = TRUE;
	    prevColumn = i;

	    while (1) {
		int numArgs = 0;
		char breakChar = '\0';

		/* Look for a + or , */
		for (index = indexElem + 1; index < objc; index++) {
		    if (numArgs % 2 == 0) {
			int length;
			char *s = Tcl_GetStringFromObj(objv[index], &length);

			if ((length == 1) && ((s[0] == '+') || (s[0] == ','))) {
			    breakChar = s[0];
			    break;
			}
		    }
		    numArgs++;
		}

		/* Require at least one option-value pair if more than one
		* element is specified. */
		if ((breakChar || indexElem != 6) && (numArgs < 2)) {
		    FormatResult(interp,
			"missing option-value pair after element \"%s\"",
			Tcl_GetString(objv[indexElem]));
		    result = TCL_ERROR;
		    goto doneCONF;
		}

		co[indexElem].numArgs = numArgs;

		if (!breakChar)
		    break;

		if (index == objc - 1) {
		    FormatResult(interp, "missing %s after \"%c\"",
			(breakChar == '+') ? "element name" : "column",
			breakChar);
		    result = TCL_ERROR;
		    goto doneCONF;
		}

		/* + indicates start of another element */
		if (breakChar == '+') {
		    indexElem = index + 1;
		}

		/* , indicates start of another column */
		else if (breakChar == ',') {
		    co[prevColumn].numArgs = index - prevColumn;

		    if (TreeColumnList_FromObj(tree, objv[index + 1],
			    &co[index + 1].columns, CFO_NOT_NULL |
			    CFO_NOT_TAIL) != TCL_OK) {
			result = TCL_ERROR;
			goto doneCONF;
		    }
		    co[index + 1].isColumn = TRUE;
		    prevColumn = index + 1;

		    indexElem = index + 2;
		    if (indexElem == objc) {
			FormatResult(interp,
			    "missing element name after column \"%s\"",
			    Tcl_GetString(objv[index + 1]));
			result = TCL_ERROR;
			goto doneCONF;
		    }
		}
	    }
	    co[prevColumn].numArgs = index - prevColumn;

	    ITEM_FOR_EACH(item, &itemList, NULL, &iter) {
		/* T item element configure I C E option value \
		*     + E option value , C E option value */
		int iMask = 0;

		/* co[index].numArgs is the number of arguments from the C
		 * to the next separator (but not including that separator). */
		for (index = 5; index < objc; index += co[index].numArgs + 1) {
		    ColumnForEach citer;
		    TreeColumn treeColumn;

if (!co[index].isColumn) panic("isColumn == FALSE");

		    COLUMN_FOR_EACH(treeColumn, &co[index].columns, NULL, &citer) {
			int columnIndex, cMask = 0;

			columnIndex = TreeColumn_Index(treeColumn);
			column = Item_FindColumn(tree, item, columnIndex);
			if ((column == NULL) || (column->style == NULL)) {
			    NoStyleMsg(tree, item, columnIndex);
			    result = TCL_ERROR;
			    break;
			}

			indexElem = index + 1;

			/* Do each element in this column */
			while (1) {
			    int eMask, index2;

if (co[indexElem].numArgs == -1) panic("indexElem=%d (%s) objc=%d numArgs == -1", indexElem, Tcl_GetString(objv[indexElem]), objc);
			    result = TreeStyle_ElementConfigure(tree, item,
				    (TreeItemColumn) column, column->style, objv[indexElem],
				    co[indexElem].numArgs, (Tcl_Obj **) objv + indexElem + 1, &eMask);
			    if (result != TCL_OK)
				break;

			    cMask |= eMask;

			    /* co[indexElem].numArgs is the number of
			     * option-value arguments after the element. */
			    index2 = indexElem + co[indexElem].numArgs;
			    if (index2 == objc - 1)
				break;

			    /* Skip the '+' or ',' */
			    index2 += 2;

			    if (co[index2].isColumn)
				break;

			    indexElem = index2;
			}

			if (cMask & CS_LAYOUT) {
			    TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
			    Tree_InvalidateColumnWidth(tree, treeColumn);
			} else if (cMask & CS_DISPLAY) {
			    Tree_InvalidateItemDInfo(tree, treeColumn, item, NULL);
			}
			iMask |= cMask;
			if (result != TCL_OK)
			    break;
		    }
		    if (result != TCL_OK)
			break;
		}
		if (iMask & CS_LAYOUT) {
		    TreeItem_InvalidateHeight(tree, item);
		    Tree_FreeItemDInfo(tree, item, NULL);
		    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
		} else if (iMask & CS_DISPLAY) {
		}
		if (result != TCL_OK)
		    break;
	    }
doneCONF:
	    for (i = 5; i < objc; i++) {
		if (co[i].isColumn)
		    TreeColumnList_Free(&co[i].columns);
	    }
	    STATIC_FREE(co, struct columnObj, objc);
	    break;
	}
    }

    TreeItemList_Free(&itemList);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ItemStyleCmd --
 *
 *	This procedure is invoked to process the [item style] widget
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
ItemStyleCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    static CONST char *commandNames[] = { "elements", "map", "set", (char *) NULL };
    enum { COMMAND_ELEMENTS, COMMAND_MAP, COMMAND_SET };
    int index;
    TreeItemList itemList;
    TreeItem item;
    int flags = IFO_NOT_NULL;
    int result = TCL_OK;

    if (objc < 5) {
	Tcl_WrongNumArgs(interp, 3, objv, "command item ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
		&index) != TCL_OK) {
	return TCL_ERROR;
    }

    /* [style elements] only works on a single item.
     * [style set] only works on a single item without a column-style pair. */
    if ((index == COMMAND_ELEMENTS) || (index == COMMAND_SET && objc < 7))
	flags |= IFO_NOT_MANY;
    if (TreeItemList_FromObj(tree, objv[4], &itemList, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    item = TreeItemList_Nth(&itemList, 0);

    switch (index) {
	/* T item style elements I C */
	case COMMAND_ELEMENTS: {
	    Column *column;
	    int columnIndex;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 4, objv, "item column");
		result = TCL_ERROR;
		break;
	    }
	    if (Item_FindColumnFromObj(tree, item, objv[5], &column,
		    &columnIndex) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    if ((column == NULL) || (column->style == NULL)) {
		NoStyleMsg(tree, item, columnIndex);
		result = TCL_ERROR;
		break;
	    }
	    TreeStyle_ListElements(tree, column->style);
	    break;
	}

	/* T item style map I C S map */
	case COMMAND_MAP: {
	    TreeStyle style;
	    TreeColumnList columns;
	    TreeColumn treeColumn;
	    Column *column;
	    int columnIndex;
	    int objcM;
	    Tcl_Obj **objvM;
	    ItemForEach iter;
	    ColumnForEach citer;

	    if (objc != 8) {
		Tcl_WrongNumArgs(interp, 4, objv, "item column style map");
		return TCL_ERROR;
	    }
	    if (TreeColumnList_FromObj(tree, objv[5], &columns,
		    CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    if (TreeStyle_FromObj(tree, objv[6], &style) != TCL_OK) {
		result = TCL_ERROR;
		goto doneMAP;
	    }
	    if (Tcl_ListObjGetElements(interp, objv[7], &objcM, &objvM)
		    != TCL_OK) {
		result = TCL_ERROR;
		goto doneMAP;
	    }
	    if (objcM & 1) {
		FormatResult(interp, "list must contain even number of elements");
		result = TCL_ERROR;
		goto doneMAP;
	    }
	    ITEM_FOR_EACH(item, &itemList, NULL, &iter) {
		COLUMN_FOR_EACH(treeColumn, &columns, NULL, &citer) {
		    columnIndex = TreeColumn_Index(treeColumn);
		    column = Item_CreateColumn(tree, item, columnIndex, NULL);
		    if (column->style != NULL) {
			if (TreeStyle_Remap(tree, column->style, style, objcM,
				objvM) != TCL_OK) {
			    result = TCL_ERROR;
			    break;
			}
		    } else {
			column->style = TreeStyle_NewInstance(tree, style);
		    }
		    TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
		    Tree_InvalidateColumnWidth(tree, treeColumn);
		}
		TreeItem_InvalidateHeight(tree, item);
		Tree_FreeItemDInfo(tree, item, NULL);
		if (result != TCL_OK)
		    break;
	    }
	    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
doneMAP:
	    TreeColumnList_Free(&columns);
	    break;
	}

	/* T item style set I ?C? ?S? ?C S ...?*/
	case COMMAND_SET: {
	    struct columnStyle {
		TreeColumnList columns;
		TreeStyle style;
	    };
	    struct columnStyle staticCS[STATIC_SIZE], *cs = staticCS;
	    TreeColumn treeColumn;
	    Column *column;
	    int i, count = 0, length, changed = FALSE, changedI;
	    ItemForEach iter;
	    ColumnForEach citer;

	    if (objc < 5) {
		Tcl_WrongNumArgs(interp, 4, objv, "item ?column? ?style? ?column style ...?");
		return TCL_ERROR;
	    }
	    /* Return list of styles. */
	    if (objc == 5) {
		Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
		treeColumn = tree->columns;
		column = item->columns;
		while (treeColumn != NULL) {
		    if ((column != NULL) && (column->style != NULL))
			Tcl_ListObjAppendElement(interp, listObj,
				TreeStyle_ToObj(TreeStyle_GetMaster(
				tree, column->style)));
		    else
			Tcl_ListObjAppendElement(interp, listObj,
				Tcl_NewObj());
		    treeColumn = TreeColumn_Next(treeColumn);
		    if (column != NULL)
			column = column->next;
		}
		Tcl_SetObjResult(interp, listObj);
		break;
	    }
	    /* Return style in one column. */
	    if (objc == 6) {
		if (Item_FindColumnFromObj(tree, item, objv[5], &column, NULL) != TCL_OK)
		    return TCL_ERROR;
		if ((column != NULL) && (column->style != NULL))
		    Tcl_SetObjResult(interp, TreeStyle_ToObj(
			TreeStyle_GetMaster(tree, column->style)));
		break;
	    }
	    /* Get column/style pairs. */
	    STATIC_ALLOC(cs, struct columnStyle, objc / 2);
	    for (i = 5; i < objc; i += 2) {
		if (TreeColumnList_FromObj(tree, objv[i], &cs[count].columns,
			CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK) {
		    result = TCL_ERROR;
		    goto doneSET;
		}
		if (i + 1 == objc) {
		    FormatResult(interp, "missing style for column \"%s\"",
			Tcl_GetString(objv[i]));
		    result = TCL_ERROR;
		    goto doneSET;
		}
		(void) Tcl_GetStringFromObj(objv[i + 1], &length);
		if (length == 0) {
		    cs[count].style = NULL;
		} else {
		    if (TreeStyle_FromObj(tree, objv[i + 1], &cs[count].style)
			    != TCL_OK) {
			result = TCL_ERROR;
			goto doneSET;
		    }
		}
		count++;
	    }
	    ITEM_FOR_EACH(item, &itemList, NULL, &iter) {
		changedI = FALSE;
		for (i = 0; i < count; i++) {
		    COLUMN_FOR_EACH(treeColumn, &cs[i].columns, NULL, &citer) {
			if (cs[i].style == NULL) {
			    column = Item_FindColumn(tree, item,
				    TreeColumn_Index(treeColumn));
			    if (column == NULL || column->style == NULL)
				continue;
			    TreeItemColumn_ForgetStyle(tree,
				    (TreeItemColumn) column);
			} else {
			    column = Item_CreateColumn(tree, item,
				    TreeColumn_Index(treeColumn), NULL);
			    if (column->style != NULL) {
				if (TreeStyle_GetMaster(tree, column->style)
					== cs[i].style)
				    continue;
				TreeItemColumn_ForgetStyle(tree,
					(TreeItemColumn) column);
			    }
			    column->style = TreeStyle_NewInstance(tree,
				    cs[i].style);
			}
			TreeItemColumn_InvalidateSize(tree,
				(TreeItemColumn) column);
			Tree_InvalidateColumnWidth(tree, treeColumn);
			changedI = TRUE;
		    }
		    if (changedI) {
			TreeItem_InvalidateHeight(tree, item);
			Tree_FreeItemDInfo(tree, item, NULL);
			changed = TRUE;
		    }
		}
	    }
	    if (changed)
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
doneSET:
	    for (i = 0; i < count; i++) {
		TreeColumnList_Free(&cs[i].columns);
	    }
	    STATIC_FREE(cs, struct columnStyle, objc / 2);
	    break;
	}
    }

    TreeItemList_Free(&itemList);
    return result;
}

/* Quicksort is not a "stable" sorting algorithm, but it can become a
 * stable sort by using the pre-sort order of two items as a tie-breaker
 * for items that would otherwise be considered equal. */
#define STABLE_SORT

/* one per column per SortItem */
struct SortItem1
{
    long longValue;
    double doubleValue;
    char *string;
};

/* one per Item */
struct SortItem
{
    TreeItem item;
    struct SortItem1 *item1;
    Tcl_Obj *obj; /* TreeItem_ToObj() */
#ifdef STABLE_SORT
    int index; /* The pre-sort order of the item */
#endif
};

typedef struct SortData SortData;

/* Used to process -element option */
struct SortElement
{
    TreeStyle style;
    TreeElement elem;
    int elemIndex;
};

/* One per TreeColumn */
struct SortColumn
{
    int (*proc)(SortData *, struct SortItem *, struct SortItem *, int); 
    int sortBy;
    int column;
    int order;
    Tcl_Obj *command;
    struct SortElement elems[20];
    int elemCount;
};

/* Data for sort as a whole */
struct SortData
{
    TreeCtrl *tree;
    struct SortItem *items;
    struct SortItem1 *item1s; /* SortItem.item1 points in here */
#define MAX_SORT_COLUMNS 40
    struct SortColumn columns[MAX_SORT_COLUMNS];
    int columnCount; /* max number of columns to compare */
    int result;
};

/* from Tcl 8.4.0 */
static int
DictionaryCompare(
    char *left,
    char *right
    )
{
    Tcl_UniChar uniLeft, uniRight, uniLeftLower, uniRightLower;
    int diff, zeros;
    int secondaryDiff = 0;

    while (1) {
	if (isdigit(UCHAR(*right)) && isdigit(UCHAR(*left))) { /* INTL: digit */
	    /*
	     * There are decimal numbers embedded in the two
	     * strings.  Compare them as numbers, rather than
	     * strings.  If one number has more leading zeros than
	     * the other, the number with more leading zeros sorts
	     * later, but only as a secondary choice.
	     */

	    zeros = 0;
	    while ((*right == '0') && (isdigit(UCHAR(right[1])))) {
		right++;
		zeros--;
	    }
	    while ((*left == '0') && (isdigit(UCHAR(left[1])))) {
		left++;
		zeros++;
	    }
	    if (secondaryDiff == 0) {
		secondaryDiff = zeros;
	    }

	    /*
	     * The code below compares the numbers in the two
	     * strings without ever converting them to integers.  It
	     * does this by first comparing the lengths of the
	     * numbers and then comparing the digit values.
	     */

	    diff = 0;
	    while (1) {
		if (diff == 0) {
		    diff = UCHAR(*left) - UCHAR(*right);
		}
		right++;
		left++;
		if (!isdigit(UCHAR(*right))) {	/* INTL: digit */
		    if (isdigit(UCHAR(*left))) {	/* INTL: digit */
			return 1;
		    } else {
			/*
			 * The two numbers have the same length. See
			 * if their values are different.
			 */

			if (diff != 0) {
			    return diff;
			}
			break;
		    }
		} else if (!isdigit(UCHAR(*left))) {	/* INTL: digit */
		    return -1;
		}
	    }
	    continue;
	}

	/*
	 * Convert character to Unicode for comparison purposes.  If either
	 * string is at the terminating null, do a byte-wise comparison and
	 * bail out immediately.
	 */

	if ((*left != '\0') && (*right != '\0')) {
	    left += Tcl_UtfToUniChar(left, &uniLeft);
	    right += Tcl_UtfToUniChar(right, &uniRight);
	    /*
	     * Convert both chars to lower for the comparison, because
	     * dictionary sorts are case insensitve.  Covert to lower, not
	     * upper, so chars between Z and a will sort before A (where most
	     * other interesting punctuations occur)
	     */
	    uniLeftLower = Tcl_UniCharToLower(uniLeft);
	    uniRightLower = Tcl_UniCharToLower(uniRight);
	} else {
	    diff = UCHAR(*left) - UCHAR(*right);
	    break;
	}

	diff = uniLeftLower - uniRightLower;
	if (diff) {
	    return diff;
	} else if (secondaryDiff == 0) {
	    if (Tcl_UniCharIsUpper(uniLeft) &&
		    Tcl_UniCharIsLower(uniRight)) {
		secondaryDiff = -1;
	    } else if (Tcl_UniCharIsUpper(uniRight) &&
		    Tcl_UniCharIsLower(uniLeft)) {
		secondaryDiff = 1;
	    }
	}
    }
    if (diff == 0) {
	diff = secondaryDiff;
    }
    return diff;
}

static int
CompareAscii(
    SortData *sortData,
    struct SortItem *a,
    struct SortItem *b,
    int n			/* Column index. */
    )
{
    char *left  = a->item1[n].string;
    char *right = b->item1[n].string;

    /* make sure to handle case where no string value has been set */
    if (left == NULL) {
	return ((right == NULL) ? 0 : (0 - UCHAR(*right)));
    } else if (right == NULL) {
	return UCHAR(*left);
    } else {
	return strcmp(left, right);
    }
}

static int
CompareDict(
    SortData *sortData,
    struct SortItem *a,
    struct SortItem *b,
    int n			/* Column index. */
    )
{
    char *left  = a->item1[n].string;
    char *right = b->item1[n].string;

    /* make sure to handle case where no string value has been set */
    if (left == NULL) {
	return ((right == NULL) ? 0 : (0 - UCHAR(*right)));
    } else if (right == NULL) {
	return UCHAR(*left);
    } else {
	return DictionaryCompare(left, right);
    }
}

static int
CompareDouble(
    SortData *sortData,
    struct SortItem *a,
    struct SortItem *b,
    int n			/* Column index. */
    )
{
    return (a->item1[n].doubleValue < b->item1[n].doubleValue) ? -1 :
	((a->item1[n].doubleValue == b->item1[n].doubleValue) ? 0 : 1);
}

static int
CompareLong(
    SortData *sortData,
    struct SortItem *a,
    struct SortItem *b,
    int n			/* Column index. */
    )
{
    return (a->item1[n].longValue < b->item1[n].longValue) ? -1 :
	((a->item1[n].longValue == b->item1[n].longValue) ? 0 : 1);
}

static int
CompareCmd(
    SortData *sortData,
    struct SortItem *a,
    struct SortItem *b,
    int n			/* Column index. */
    )
{
    Tcl_Interp *interp = sortData->tree->interp;
    Tcl_Obj **objv, *paramObjv[2];
    int objc, v;

    paramObjv[0] = a->obj;
    paramObjv[1] = b->obj;

    Tcl_ListObjLength(interp, sortData->columns[n].command, &objc);
    Tcl_ListObjReplace(interp, sortData->columns[n].command, objc - 2,
	    2, 2, paramObjv);
    Tcl_ListObjGetElements(interp, sortData->columns[n].command,
	    &objc, &objv);

    sortData->result = Tcl_EvalObjv(interp, objc, objv, 0);

    if (sortData->result != TCL_OK) {
	Tcl_AddErrorInfo(interp, "\n    (evaluating item sort -command)");
	return 0;
    }

    sortData->result = Tcl_GetIntFromObj(interp, Tcl_GetObjResult(interp), &v);
    if (sortData->result != TCL_OK) {
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
		"-command returned non-numeric result", -1);
	return 0;
    }

    return v;
}

static int
CompareProc(
    SortData *sortData,
    struct SortItem *a,
    struct SortItem *b
    )
{
    int i, v;

    if (a->item == b->item)
	return 0;

    for (i = 0; i < sortData->columnCount; i++) {
	v = (*sortData->columns[i].proc)(sortData, a, b, i);

	/* -command returned error */
	if (sortData->result != TCL_OK)
	    return 0;

	if (v != 0) {
	    if (i && (sortData->columns[i].order != sortData->columns[0].order))
		v *= -1;
	    return v;
	}
    }
#ifdef STABLE_SORT
    return ((a->index < b->index) == sortData->columns[0].order) ? -1 : 1;
#else
    return 0;
#endif
}

/* BEGIN custom quicksort() */

static int
find_pivot(
    SortData *sortData,
    struct SortItem *left,
    struct SortItem *right,
    struct SortItem *pivot
    )
{
    struct SortItem *a, *b, *c, *p, *tmp;
    int v;

    a = left;
    b = (left + (right - left) / 2);
    c = right;

    /* Arrange a <= b <= c. */
    v = CompareProc(sortData, a, b);
    if (sortData->result != TCL_OK)
	return 0;
    if (v > 0) { tmp = a; a = b; b = tmp; }

    v = CompareProc(sortData, a, c);
    if (sortData->result != TCL_OK)
	return 0;
    if (v > 0) { tmp = a; a = c; c = tmp; }

    v = CompareProc(sortData, b, c);
    if (sortData->result != TCL_OK)
	return 0;
    if (v > 0) { tmp = b; b = c; c = tmp; }

    /* if (a < b) pivot = b */
    v = CompareProc(sortData, a, b);
    if (sortData->result != TCL_OK)
	return 0;
    if (v < 0) {
	(*pivot) = *b;
	return 1;
    }

    /* if (b < c) pivot = c */
    v = CompareProc(sortData, b, c);
    if (sortData->result != TCL_OK)
	return 0;
    if (v < 0) {
	(*pivot) = *c;
	return 1;
    }

    for (p = left + 1; p <= right; p++) {
	int v = CompareProc(sortData, p, left);
	if (sortData->result != TCL_OK)
	    return 0;
	if (v != 0) {
	    (*pivot) = (v < 0) ? *left : *p;
	    return 1;
	}
    }
    return 0;
}

/* If the user provides a -command which does not properly compare two
 * elements, quicksort may go into an infinite loop or access illegal memory.
 * This #define indicates parts of the code which are not part of a normal
 * quicksort, but are present to detect the aforementioned bugs. */
#define BUGGY_COMMAND

static struct SortItem *
partition(
    SortData *sortData,
    struct SortItem *left,
    struct SortItem *right,
    struct SortItem *pivot
    )
{
    int v;
#ifdef BUGGY_COMMAND
    struct SortItem *min = left, *max = right;
#endif

    while (left <= right) {
	/*
	  while (*left < *pivot)
	    ++left;
	*/
	while (1) {
	    v = CompareProc(sortData, left, pivot);
	    if (sortData->result != TCL_OK)
		return NULL;
	    if (v >= 0)
		break;
#ifdef BUGGY_COMMAND
	    /* If -command always returns < 0, 'left' becomes invalid */
	    if (left == max)
		goto buggy;
#endif
	    left++;
	}
	/*
	  while (*right >= *pivot)
	    --right;
	*/
	while (1) {
	    v = CompareProc(sortData, right, pivot);
	    if (sortData->result != TCL_OK)
		return NULL;
	    if (v < 0)
		break;
#ifdef BUGGY_COMMAND
	    /* If -command always returns >= 0, 'right' becomes invalid */
	    if (right == min)
		goto buggy;
#endif
	    right--;
	}
	if (left < right) {
	    struct SortItem tmp = *left;
	    *left = *right;
	    *right = tmp;
	    left++;
	    right--;
	}
    }
    return left;
#ifdef BUGGY_COMMAND
    buggy:
    FormatResult(sortData->tree->interp, "buggy item sort -command detected");
    sortData->result = TCL_ERROR;
    return NULL;
#endif
}

static void
quicksort(
    SortData *sortData,
    struct SortItem *left,
    struct SortItem *right
    )
{
    struct SortItem *p, pivot;

    if (sortData->result != TCL_OK)
	return;

    if (left == right)
	return;

    /* FIXME: switch to insertion sort or similar when the number of
     * elements is small. */

    if (find_pivot(sortData, left, right, &pivot) == 1) {
	p = partition(sortData, left, right, &pivot);
	quicksort(sortData, left, p - 1);
	quicksort(sortData, p, right);
    }
}

/* END custom quicksort() */

/*
 *----------------------------------------------------------------------
 *
 * ItemSortCmd --
 *
 *	This procedure is invoked to process the [item sort] widget
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
ItemSortCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    TreeItem item, first, last, walk, lastChild;
    Column *column;
    int i, j, count, elemIndex, index, indexF = 0, indexL = 0;
    int sawColumn = FALSE, sawCmd = FALSE;
    static int (*sortProc[5])(SortData *, struct SortItem *, struct SortItem *, int) =
	{ CompareAscii, CompareDict, CompareDouble, CompareLong, CompareCmd };
    SortData sortData;
    TreeColumn treeColumn;
    struct SortElement *elemPtr;
    int notReally = FALSE;
    int result = TCL_OK;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 3, objv, "item ?option ...?");
	return TCL_ERROR;
    }

    if (TreeItem_FromObj(tree, objv[3], &item, IFO_NOT_NULL) != TCL_OK)
	return TCL_ERROR;

    /* If the item has no children, then nothing is done and no error
     * is generated. */
    if (item->numChildren < 1)
	return TCL_OK;

    /* Defaults: sort ascii strings in column 0 only */
    sortData.tree = tree;
    sortData.columnCount = 1;
    sortData.columns[0].column = 0;
    sortData.columns[0].sortBy = SORT_ASCII;
    sortData.columns[0].order = 1;
    sortData.columns[0].elemCount = 0;
    sortData.result = TCL_OK;

    first = item->firstChild;
    last = item->lastChild;

    for (i = 4; i < objc; ) {
	static CONST char *optionName[] = { "-ascii", "-column", "-command",
					    "-decreasing", "-dictionary", "-element", "-first", "-increasing",
					    "-integer", "-last", "-notreally", "-real", NULL };
	int numArgs[] = { 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 1, 1 };
	enum { OPT_ASCII, OPT_COLUMN, OPT_COMMAND, OPT_DECREASING, OPT_DICT,
	       OPT_ELEMENT, OPT_FIRST, OPT_INCREASING, OPT_INTEGER, OPT_LAST,
	       OPT_NOT_REALLY, OPT_REAL };

	if (Tcl_GetIndexFromObj(interp, objv[i], optionName, "option", 0,
		    &index) != TCL_OK)
	    return TCL_ERROR;
	if (objc - i < numArgs[index]) {
	    FormatResult(interp, "missing value for \"%s\" option",
		    optionName[index]);
	    return TCL_ERROR;
	}
	switch (index) {
	    case OPT_ASCII:
		sortData.columns[sortData.columnCount - 1].sortBy = SORT_ASCII;
		break;
	    case OPT_COLUMN:
		if (TreeColumn_FromObj(tree, objv[i + 1], &treeColumn,
			    CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
		    return TCL_ERROR;
		/* The first -column we see is the first column we compare */
		if (sawColumn) {
		    if (sortData.columnCount + 1 > MAX_SORT_COLUMNS) {
			FormatResult(interp,
				"can't compare more than %d columns",
				MAX_SORT_COLUMNS);
			return TCL_ERROR;
		    }
		    sortData.columnCount++;
		    /* Defaults for this column */
		    sortData.columns[sortData.columnCount - 1].sortBy = SORT_ASCII;
		    sortData.columns[sortData.columnCount - 1].order = 1;
		    sortData.columns[sortData.columnCount - 1].elemCount = 0;
		}
		sortData.columns[sortData.columnCount - 1].column = TreeColumn_Index(treeColumn);
		sawColumn = TRUE;
		break;
	    case OPT_COMMAND:
		sortData.columns[sortData.columnCount - 1].command = objv[i + 1];
		sortData.columns[sortData.columnCount - 1].sortBy = SORT_COMMAND;
		sawCmd = TRUE;
		break;
	    case OPT_DECREASING:
		sortData.columns[sortData.columnCount - 1].order = 0;
		break;
	    case OPT_DICT:
		sortData.columns[sortData.columnCount - 1].sortBy = SORT_DICT;
		break;
	    case OPT_ELEMENT: {
		int listObjc;
		Tcl_Obj **listObjv;

		if (Tcl_ListObjGetElements(interp, objv[i + 1], &listObjc,
			    &listObjv) != TCL_OK)
		    return TCL_ERROR;
		elemPtr = sortData.columns[sortData.columnCount - 1].elems;
		sortData.columns[sortData.columnCount - 1].elemCount = 0;
		if (listObjc == 0) {
		} else if (listObjc == 1) {
		    if (TreeElement_FromObj(tree, listObjv[0], &elemPtr->elem)
			    != TCL_OK) {
			Tcl_AddErrorInfo(interp,
				"\n    (processing -element option)");
			return TCL_ERROR;
		    }
		    if (!TreeElement_IsType(tree, elemPtr->elem, "text")) {
			FormatResult(interp,
				"element %s is not of type \"text\"",
				Tcl_GetString(listObjv[0]));
			Tcl_AddErrorInfo(interp,
				"\n    (processing -element option)");
			return TCL_ERROR;
		    }
		    elemPtr->style = NULL;
		    elemPtr->elemIndex = -1;
		    sortData.columns[sortData.columnCount - 1].elemCount++;
		} else {
		    if (listObjc & 1) {
			FormatResult(interp,
				"list must have even number of elements");
			Tcl_AddErrorInfo(interp,
				"\n    (processing -element option)");
			return TCL_ERROR;
		    }
		    for (j = 0; j < listObjc; j += 2) {
			if ((TreeStyle_FromObj(tree, listObjv[j],
				     &elemPtr->style) != TCL_OK) ||
				(TreeElement_FromObj(tree, listObjv[j + 1],
					&elemPtr->elem) != TCL_OK) ||
				(TreeStyle_FindElement(tree, elemPtr->style,
					elemPtr->elem, &elemPtr->elemIndex) != TCL_OK)) {
			    Tcl_AddErrorInfo(interp,
				    "\n    (processing -element option)");
			    return TCL_ERROR;
			}
			if (!TreeElement_IsType(tree, elemPtr->elem, "text")) {
			    FormatResult(interp,
				    "element %s is not of type \"text\"",
				    Tcl_GetString(listObjv[j + 1]));
			    Tcl_AddErrorInfo(interp,
				    "\n    (processing -element option)");
			    return TCL_ERROR;
			}
			sortData.columns[sortData.columnCount - 1].elemCount++;
			elemPtr++;
		    }
		}
		break;
	    }
	    case OPT_FIRST:
		if (TreeItem_FromObj(tree, objv[i + 1], &first, IFO_NOT_NULL) != TCL_OK)
		    return TCL_ERROR;
		if (first->parent != item) {
		    FormatResult(interp,
			    "item %s%d is not a child of item %s%d",
			    tree->itemPrefix, first->id, tree->itemPrefix, item->id);
		    return TCL_ERROR;
		}
		break;
	    case OPT_INCREASING:
		sortData.columns[sortData.columnCount - 1].order = 1;
		break;
	    case OPT_INTEGER:
		sortData.columns[sortData.columnCount - 1].sortBy = SORT_LONG;
		break;
	    case OPT_LAST:
		if (TreeItem_FromObj(tree, objv[i + 1], &last, IFO_NOT_NULL) != TCL_OK)
		    return TCL_ERROR;
		if (last->parent != item) {
		    FormatResult(interp,
			    "item %s%d is not a child of item %s%d",
			    tree->itemPrefix, last->id, tree->itemPrefix, item->id);
		    return TCL_ERROR;
		}
		break;
	    case OPT_NOT_REALLY:
		notReally = TRUE;
		break;
	    case OPT_REAL:
		sortData.columns[sortData.columnCount - 1].sortBy = SORT_DOUBLE;
		break;
	}
	i += numArgs[index];
    }

    /* If there are no columns, we cannot perform a sort unless -command
     * is specified. */
    if ((tree->columnCount < 1) && (sortData.columns[0].sortBy != SORT_COMMAND)) {
	FormatResult(interp, "there are no columns");
	return TCL_ERROR;
    }

    /* If there is only one item to sort, then return early. */
    if (first == last) {
	if (notReally)
	    Tcl_SetObjResult(interp, TreeItem_ToObj(tree, first));
	return TCL_OK;
    }

    for (i = 0; i < sortData.columnCount; i++) {

	/* Initialize the sort procedure for this column. */
	sortData.columns[i].proc = sortProc[sortData.columns[i].sortBy];

	/* Append two dummy args to the -command argument. These two dummy
	 * args are replaced by the 2 item ids being compared. See
	 * CompareCmd(). */
	if (sortData.columns[i].sortBy == SORT_COMMAND) {
	    Tcl_Obj *obj = Tcl_DuplicateObj(sortData.columns[i].command);
	    Tcl_Obj *obj2 = Tcl_NewObj();
	    Tcl_IncrRefCount(obj);
	    if (Tcl_ListObjAppendElement(interp, obj, obj2) != TCL_OK) {
		Tcl_DecrRefCount(obj);
		Tcl_IncrRefCount(obj2);
		Tcl_DecrRefCount(obj2);

		for (j = 0; j < i; j++) {
		    if (sortData.columns[j].sortBy == SORT_COMMAND) {
			Tcl_DecrRefCount(sortData.columns[j].command);
		    }
		}

		return TCL_ERROR;
	    }
	    (void) Tcl_ListObjAppendElement(interp, obj, obj2);
	    sortData.columns[i].command = obj;
	}
    }

    index = 0;
    walk = item->firstChild;
    while (walk != NULL) {
	if (walk == first)
	    indexF = index;
	if (walk == last)
	    indexL = index;
	index++;
	walk = walk->nextSibling;
    }
    if (indexF > indexL) {
	walk = last;
	last = first;
	first = walk;

	index = indexL;
	indexL = indexF;
	indexF = index;
    }
    count = indexL - indexF + 1;

    sortData.item1s = (struct SortItem1 *) ckalloc(sizeof(struct SortItem1) * count * sortData.columnCount);
    sortData.items = (struct SortItem *) ckalloc(sizeof(struct SortItem) * count);
    for (i = 0; i < count; i++) {
	sortData.items[i].item1 = sortData.item1s + i * sortData.columnCount;
	sortData.items[i].obj = NULL;
    }

    index = 0;
    walk = first;
    while (walk != last->nextSibling) {
	struct SortItem *sortItem = &sortData.items[index];

	sortItem->item = walk;
#ifdef STABLE_SORT
	sortItem->index = index;
#endif
	if (sawCmd) {
	    Tcl_Obj *obj = TreeItem_ToObj(tree, walk);
	    Tcl_IncrRefCount(obj);
	    sortData.items[index].obj = obj;
	}
	for (i = 0; i < sortData.columnCount; i++) {
	    struct SortItem1 *sortItem1 = sortItem->item1 + i;

	    if (sortData.columns[i].sortBy == SORT_COMMAND)
		continue;

	    column = Item_FindColumn(tree, walk, sortData.columns[i].column);
	    if ((column == NULL) || (column->style == NULL)) {
		NoStyleMsg(tree, walk, sortData.columns[i].column);
		result = TCL_ERROR;
		goto done;
	    }

	    /* -element was empty. Find the first text element in the style */
	    if (sortData.columns[i].elemCount == 0)
		elemIndex = -1;

	    /* -element was element name. Find the element in the style */
	    else if ((sortData.columns[i].elemCount == 1) &&
		    (sortData.columns[i].elems[0].style == NULL)) {
		if (TreeStyle_FindElement(tree, column->style,
			    sortData.columns[i].elems[0].elem, &elemIndex) != TCL_OK) {
		    result = TCL_ERROR;
		    goto done;
		}
	    }

	    /* -element was style/element pair list */
	    else {
		TreeStyle masterStyle = TreeStyle_GetMaster(tree, column->style);

		/* If the item style does not match any in the -element list,
		 * we will use the first text element in the item style. */
		elemIndex = -1;

		/* Match a style from the -element list. Look in reverse order
		 * to handle duplicates. */
		for (j = sortData.columns[i].elemCount - 1; j >= 0; j--) {
		    if (sortData.columns[i].elems[j].style == masterStyle) {
			elemIndex = sortData.columns[i].elems[j].elemIndex;
			break;
		    }
		}
	    }
	    if (TreeStyle_GetSortData(tree, column->style, elemIndex,
			sortData.columns[i].sortBy,
			&sortItem1->longValue,
			&sortItem1->doubleValue,
			&sortItem1->string) != TCL_OK) {
		char msg[128];
		sprintf(msg, "\n    (preparing to sort item %s%d column %s%d)",
			tree->itemPrefix, walk->id,
			tree->columnPrefix, TreeColumn_GetID(
			Tree_FindColumn(tree, sortData.columns[i].column)));
		Tcl_AddErrorInfo(interp, msg);
		result = TCL_ERROR;
		goto done;
	    }
	}
	index++;
	walk = walk->nextSibling;
    }

    quicksort(&sortData, sortData.items, sortData.items + count - 1);

    if (sortData.result != TCL_OK) {
	result = sortData.result;
	goto done;
    }

    if (sawCmd)
	Tcl_ResetResult(interp);

    if (notReally) {
	Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
	Tcl_Obj *itemObj;

	/* Smallest to largest */
	if (sortData.columns[0].order == 1) {
	    for (i = 0; i < count; i++) {
		itemObj = sortData.items[i].obj;
		if (itemObj == NULL)
		    itemObj = TreeItem_ToObj(tree,
			    sortData.items[i].item);
		Tcl_ListObjAppendElement(interp, listObj, itemObj);
	    }
	}

	/* Largest to smallest */
	else {
	    for (i = count - 1; i >= 0; i--) {
		itemObj = sortData.items[i].obj;
		if (itemObj == NULL)
		    itemObj = TreeItem_ToObj(tree,
			    sortData.items[i].item);
		Tcl_ListObjAppendElement(interp, listObj, itemObj);
	    }
	}

	Tcl_SetObjResult(interp, listObj);
	goto done;
    }
    first = first->prevSibling;
    last = last->nextSibling;

    /* Smallest to largest */
    if (sortData.columns[0].order == 1) {
	for (i = 0; i < count - 1; i++) {
	    sortData.items[i].item->nextSibling = sortData.items[i + 1].item;
	    sortData.items[i + 1].item->prevSibling = sortData.items[i].item;
	}
	indexF = 0;
	indexL = count - 1;
    }

    /* Largest to smallest */
    else {
	for (i = count - 1; i > 0; i--) {
	    sortData.items[i].item->nextSibling = sortData.items[i - 1].item;
	    sortData.items[i - 1].item->prevSibling = sortData.items[i].item;
	}
	indexF = count - 1;
	indexL = 0;
    }

    lastChild = item->lastChild;

    sortData.items[indexF].item->prevSibling = first;
    if (first)
	first->nextSibling = sortData.items[indexF].item;
    else
	item->firstChild = sortData.items[indexF].item;

    sortData.items[indexL].item->nextSibling = last;
    if (last)
	last->prevSibling = sortData.items[indexL].item;
    else
	item->lastChild = sortData.items[indexL].item;

    /* Redraw the lines of the old/new lastchild */
    if ((item->lastChild != lastChild) && tree->showLines && (tree->columnTree != NULL)) {
	if (lastChild->dInfo != NULL)
	    Tree_InvalidateItemDInfo(tree, tree->columnTree,
		    lastChild,
		    NULL);
	if (item->lastChild->dInfo != NULL)
	    Tree_InvalidateItemDInfo(tree, tree->columnTree,
		    item->lastChild,
		    NULL);
    }

    tree->updateIndex = 1;
    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

    done:
    for (i = 0; i < count; i++) {
	if (sortData.items[i].obj != NULL) {
	    Tcl_DecrRefCount(sortData.items[i].obj);
	}
    }
    for (i = 0; i < sortData.columnCount; i++) {
	if (sortData.columns[i].sortBy == SORT_COMMAND) {
	    Tcl_DecrRefCount(sortData.columns[i].command);
	}
    }
    ckfree((char *) sortData.item1s);
    ckfree((char *) sortData.items);

    if (tree->debug.enable && tree->debug.data) {
	Tree_Debug(tree);
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemList_Sort --
 *
 *	Sorts a list of items.
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
TILSCompare(
    CONST VOID *first_,
    CONST VOID *second_
    )
{
    TreeItem first = *(TreeItem *) first_;
    TreeItem second = *(TreeItem *) second_;

    return first->index - second->index;
}

void
TreeItemList_Sort(
    TreeItemList *items
    )
{
    Tree_UpdateItemIndex(items->tree);

    /* TkTable uses this, but mentions possible lack of thread-safety. */
    qsort((VOID *) TreeItemList_Items(items),
	    (size_t) TreeItemList_Count(items),
	    sizeof(TreeItem),
	    TILSCompare);
}

/*
 *----------------------------------------------------------------------
 *
 * ItemStateCmd --
 *
 *	This procedure is invoked to process the [item state] widget
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
ItemStateCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    static CONST char *commandNames[] = {
	"forcolumn", "get", "set", (char *) NULL
    };
    enum {
	COMMAND_FORCOLUMN, COMMAND_GET, COMMAND_SET
    };
    int index;
    TreeItem item;

    if (objc < 5) {
	Tcl_WrongNumArgs(interp, 3, objv, "command item ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
		&index) != TCL_OK)
	return TCL_ERROR;

    switch (index) {
	/* T item state forcolumn I C ?stateList? */
	case COMMAND_FORCOLUMN: {
	    TreeItemList itemList;
	    TreeColumnList columns;
	    TreeColumn treeColumn;
	    Tcl_Obj *listObj;
	    Column *column;
	    int columnIndex;
	    int i, states[3], stateOn, stateOff;
	    ItemForEach iter;
	    ColumnForEach citer;
	    int flags = IFO_NOT_NULL;
	    int result = TCL_OK;

	    if (objc < 6 || objc > 7) {
		Tcl_WrongNumArgs(interp, 4, objv, "item column ?stateList?");
		return TCL_ERROR;
	    }
	    /* Without a stateList only one item is accepted. */
	    if (objc == 6)
		flags |= IFO_NOT_MANY;
	    if (TreeItemList_FromObj(tree, objv[4], &itemList, flags)
		    != TCL_OK)
		return TCL_ERROR;
	    TreeColumnList_Init(tree, &columns, 0);
	    if (objc == 6) {
		item = TreeItemList_Nth(&itemList, 0);
		if (Item_FindColumnFromObj(tree, item, objv[5], &column,
			    &columnIndex) != TCL_OK) {
		    result = TCL_ERROR;
		    goto doneFORC;
		}
		if ((column == NULL) || !column->cstate)
		    goto doneFORC;
		listObj = Tcl_NewListObj(0, NULL);
		for (i = 0; i < 32; i++) {
		    if (tree->stateNames[i] == NULL)
			continue;
		    if (column->cstate & (1L << i)) {
			Tcl_ListObjAppendElement(interp, listObj,
				Tcl_NewStringObj(tree->stateNames[i], -1));
		    }
		}
		Tcl_SetObjResult(interp, listObj);
		goto doneFORC;
	    }
	    if (TreeColumnList_FromObj(tree, objv[5], &columns,
		    CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK) {
		result = TCL_ERROR;
		goto doneFORC;
	    }
	    if (Tree_StateFromListObj(tree, objv[6], states, SFO_NOT_STATIC) != TCL_OK) {
		result = TCL_ERROR;
		goto doneFORC;
	    }
	    if ((states[0] | states[1] | states[2]) == 0)
		goto doneFORC;
	    ITEM_FOR_EACH(item, &itemList, NULL, &iter) {
		COLUMN_FOR_EACH(treeColumn, &columns, NULL, &citer) {
		    columnIndex = TreeColumn_Index(treeColumn);
		    column = Item_CreateColumn(tree, item, columnIndex, NULL);
		    stateOn = states[STATE_OP_ON];
		    stateOff = states[STATE_OP_OFF];
		    stateOn |= ~column->cstate & states[STATE_OP_TOGGLE];
		    stateOff |= column->cstate & states[STATE_OP_TOGGLE];
		    Column_ChangeState(tree, item, column, treeColumn,
			    stateOff, stateOn);
		}
	    }
doneFORC:
	    TreeColumnList_Free(&columns);
	    TreeItemList_Free(&itemList);
	    return result;
	}

	/* T item state get I ?state? */
	case COMMAND_GET: {
	    Tcl_Obj *listObj;
	    int i, states[3];

	    if (objc > 6) {
		Tcl_WrongNumArgs(interp, 5, objv, "?state?");
		return TCL_ERROR;
	    }
	    if (TreeItem_FromObj(tree, objv[4], &item, IFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    if (objc == 6) {
		states[STATE_OP_ON] = 0;
		if (Tree_StateFromObj(tree, objv[5], states, NULL,
			    SFO_NOT_OFF | SFO_NOT_TOGGLE) != TCL_OK)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp,
			Tcl_NewBooleanObj((item->state & states[STATE_OP_ON]) != 0));
		break;
	    }
	    listObj = Tcl_NewListObj(0, NULL);
	    for (i = 0; i < 32; i++) {
		if (tree->stateNames[i] == NULL)
		    continue;
		if (item->state & (1L << i)) {
		    Tcl_ListObjAppendElement(interp, listObj,
			    Tcl_NewStringObj(tree->stateNames[i], -1));
		}
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	/* T item state set I ?I? {state ...} */
	case COMMAND_SET: {
	    TreeItemList itemList, item2List;
	    int states[3], stateOn, stateOff;
	    ItemForEach iter;
	    int result = TCL_OK;

	    if (objc < 6 || objc > 7) {
		Tcl_WrongNumArgs(interp, 5, objv, "?last? stateList");
		return TCL_ERROR;
	    }
	    if (TreeItemList_FromObj(tree, objv[4], &itemList, IFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    if (objc == 6) {
		TreeItemList_Init(tree, &item2List, 0);
	    }
	    if (objc == 7) {
		if (TreeItemList_FromObj(tree, objv[5], &item2List, IFO_NOT_NULL) != TCL_OK) {
		    result =  TCL_ERROR;
		    goto doneSET;
		}
	    }
	    if (Tree_StateFromListObj(tree, objv[objc - 1], states,
			SFO_NOT_STATIC) != TCL_OK) {
		result =  TCL_ERROR;
		goto doneSET;
	    }
	    if ((states[0] | states[1] | states[2]) == 0)
		goto doneSET;
	    ITEM_FOR_EACH(item, &itemList, &item2List, &iter) {
		stateOn = states[STATE_OP_ON];
		stateOff = states[STATE_OP_OFF];
		stateOn |= ~item->state & states[STATE_OP_TOGGLE];
		stateOff |= item->state & states[STATE_OP_TOGGLE];
		TreeItem_ChangeState(tree, item, stateOff, stateOn);
	    }
	    if (iter.error)
		result = TCL_ERROR;
doneSET:
	    TreeItemList_Free(&itemList);
	    TreeItemList_Free(&item2List);
	    return result;
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ItemTagCmd --
 *
 *	This procedure is invoked to process the [item tag] widget
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
ItemTagCmd(
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
    ItemForEach iter;
    TreeItemList items;
    TreeItem item;
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
	/* T item tag add I tagList */
	case COMMAND_ADD: {
	    int i, numTags;
	    Tcl_Obj **listObjv;
	    Tk_Uid staticTags[STATIC_SIZE], *tags = staticTags;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 4, objv, "item tagList");
		return TCL_ERROR;
	    }
	    if (TreeItemList_FromObj(tree, objv[4], &items, IFO_NOT_NULL) != TCL_OK) {
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
	    ITEM_FOR_EACH(item, &items, NULL, &iter) {
		item->tagInfo = TagInfo_Add(tree, item->tagInfo, tags, numTags);
	    }
	    STATIC_FREE(tags, Tk_Uid, numTags);
	    break;
	}

	/* T item tag expr I tagExpr */
	case COMMAND_EXPR: {
	    TagExpr expr;
	    int ok = TRUE;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 4, objv, "item tagExpr");
		return TCL_ERROR;
	    }
	    if (TreeItemList_FromObj(tree, objv[4], &items, IFO_NOT_NULL) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (TagExpr_Init(tree, objv[5], &expr) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    ITEM_FOR_EACH(item, &items, NULL, &iter) {
		if (!TagExpr_Eval(&expr, item->tagInfo)) {
		    ok = FALSE;
		    break;
		}
	    }
	    TagExpr_Free(&expr);
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(ok));
	    break;
	}

	/* T item tag names I */
	case COMMAND_NAMES: {
	    Tcl_Obj *listObj;
	    Tk_Uid *tags = NULL;
	    int i, tagSpace, numTags = 0;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 4, objv, "item");
		return TCL_ERROR;
	    }
	    if (TreeItemList_FromObj(tree, objv[4], &items, IFO_NOT_NULL) != TCL_OK) {
		return TCL_ERROR;
	    }
	    ITEM_FOR_EACH(item, &items, NULL, &iter) {
		tags = TagInfo_Names(tree, item->tagInfo, tags, &numTags, &tagSpace);
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

	/* T item tag remove I tagList */
	case COMMAND_REMOVE: {
	    int i, numTags;
	    Tcl_Obj **listObjv;
	    Tk_Uid staticTags[STATIC_SIZE], *tags = staticTags;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 4, objv, "item tagList");
		return TCL_ERROR;
	    }
	    if (TreeItemList_FromObj(tree, objv[4], &items, IFO_NOT_NULL) != TCL_OK) {
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
	    ITEM_FOR_EACH(item, &items, NULL, &iter) {
		item->tagInfo = TagInfo_Remove(tree, item->tagInfo, tags, numTags);
	    }
	    STATIC_FREE(tags, Tk_Uid, numTags);
	    break;
	}
    }

    TreeItemList_Free(&items);
    return result;
}

#ifdef SELECTION_VISIBLE

/*
 *----------------------------------------------------------------------
 *
 * Tree_DeselectHidden --
 *
 *	Removes any selected items which are no longer ReallyVisible()
 *	from the selection.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	<Selection> event may be generated.
 *
 *----------------------------------------------------------------------
 */

/*
 * FIXME: optimize all calls to this routine.
 * Optionally call Tree_DInfoChanged(tree, DINFO_REDO_SELECTION) instead.
 */
void
Tree_DeselectHidden(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeItemList items;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    TreeItem item;
    int i;

    if (tree->selectCount < 1)
	return;

    /* This call is slow for large lists. */
    Tree_UpdateItemIndex(tree);

    TreeItemList_Init(tree, &items, tree->selectCount);

    hPtr = Tcl_FirstHashEntry(&tree->selection, &search);
    while (hPtr != NULL) {
	item = (TreeItem) Tcl_GetHashKey(&tree->selection, hPtr);
	if (!TreeItem_ReallyVisible(tree, item))
	    TreeItemList_Append(&items, item);
	hPtr = Tcl_NextHashEntry(&search);
    }
    for (i = 0; i < TreeItemList_Count(&items); i++)
	Tree_RemoveFromSelection(tree, TreeItemList_Nth(&items, i));
    if (TreeItemList_Count(&items)) {
	TreeNotify_Selection(tree, NULL, &items);
    }
    TreeItemList_Free(&items);
}

#endif /* SELECTION_VISIBLE */

/*
 *----------------------------------------------------------------------
 *
 * TreeItemCmd --
 *
 *	This procedure is invoked to process the [item] widget
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

int
TreeItemCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    enum {
	COMMAND_ANCESTORS,
	COMMAND_BBOX,
	COMMAND_BUTTONSTATE,
	COMMAND_CGET,
	COMMAND_CHILDREN,
	COMMAND_COLLAPSE,
	COMMAND_COMPARE,
#ifdef DEPRECATED
	COMMAND_COMPLEX,
#endif
	COMMAND_CONFIGURE,
	COMMAND_COUNT,
	COMMAND_CREATE,
	COMMAND_DELETE,
	COMMAND_DESCENDANTS,
	COMMAND_DUMP,
	COMMAND_ELEMENT,
	COMMAND_ENABLED,
	COMMAND_EXPAND,
	COMMAND_FIRSTCHILD,
	COMMAND_ID,
	COMMAND_IMAGE,
	COMMAND_ISANCESTOR,
	COMMAND_ISOPEN,
	COMMAND_LASTCHILD,
	COMMAND_NEXTSIBLING,
	COMMAND_NUMCHILDREN,
	COMMAND_ORDER,
	COMMAND_PARENT,
	COMMAND_PREVSIBLING,
	COMMAND_RANGE,
	COMMAND_REMOVE,
	COMMAND_RNC,
	COMMAND_SORT,
	COMMAND_SPAN,
	COMMAND_STATE,
	COMMAND_STYLE,
	COMMAND_TAG,
	COMMAND_TEXT,
	COMMAND_TOGGLE
    };

/* AF_xxx must not conflict with IFO_xxx. */
#define AF_NOT_ANCESTOR	0x00010000 /* item can't be ancestor of other item */
#define AF_NOT_EQUAL	0x00020000 /* second item can't be same as first */
#define AF_SAMEROOT	0x00040000 /* both items must be descendants of a
				    * common ancestor */
#define AF_NOT_ITEM	0x00080000 /* arg is not an Item */
#define AF_NOT_DELETED	0x00100000 /* item can't be deleted */

    struct {
	char *cmdName;
	int minArgs;
	int maxArgs;
	int flags;	/* AF_xxx | IFO_xxx for 1st arg. */
	int flags2;	/* AF_xxx | IFO_xxx for 2nd arg. */
	int flags3;	/* AF_xxx | IFO_xxx for 3rd arg. */
	char *argString;
	Tcl_ObjCmdProc *proc;
    } argInfo[] = {
	{ "ancestors", 1, 1, IFO_NOT_MANY | IFO_NOT_NULL, 0, 0, "item", NULL },
	{ "bbox", 1, 3, IFO_NOT_MANY | IFO_NOT_NULL, AF_NOT_ITEM, AF_NOT_ITEM,
		"item ?column? ?element?", NULL },
	{ "buttonstate", 1, 2, IFO_NOT_MANY | IFO_NOT_NULL, AF_NOT_ITEM, 0,
		"item ?state?", NULL },
	{ "cget", 2, 2, IFO_NOT_MANY | IFO_NOT_NULL, AF_NOT_ITEM, 0,
		"item option", NULL },
	{ "children", 1, 1, IFO_NOT_MANY | IFO_NOT_NULL, 0, 0,
		"item", NULL },
	{ "collapse", 1, 2, IFO_NOT_NULL, AF_NOT_ITEM, 0,
		"item ?-recurse?", NULL},
	{ "compare", 3, 3, IFO_NOT_MANY | IFO_NOT_NULL, AF_NOT_ITEM,
		IFO_NOT_MANY | IFO_NOT_NULL, "item1 op item2",
		NULL },
#ifdef DEPRECATED
	{ "complex", 2, 100000, IFO_NOT_MANY | IFO_NOT_NULL, AF_NOT_ITEM,
		AF_NOT_ITEM, "item list ...", NULL },
#endif
	{ "configure", 1, 100000, IFO_NOT_NULL, AF_NOT_ITEM, AF_NOT_ITEM,
		"item ?option? ?value? ?option value ...?", NULL },
	{ "count", 0, 1, 0, 0, 0, "?itemDesc?" , NULL},
	{ "create", 0, 0, 0, 0, 0, NULL, ItemCreateCmd },
	{ "delete", 1, 2, IFO_NOT_NULL, IFO_NOT_NULL | AF_SAMEROOT, 0,
		"first ?last?", NULL },
	{ "descendants", 1, 1, IFO_NOT_MANY | IFO_NOT_NULL, 0, 0, "item",
		NULL },
	{ "dump", 1, 1, IFO_NOT_MANY | IFO_NOT_NULL, 0, 0, "item", NULL },
	{ "element", 0, 0, 0, 0, 0, NULL, ItemElementCmd },
	{ "enabled", 1, 2, IFO_NOT_NULL, AF_NOT_ITEM, 0, "item ?boolean?",
		NULL },
	{ "expand", 1, 2, IFO_NOT_NULL, AF_NOT_ITEM, 0, "item ?-recurse?",
		NULL},
	{ "firstchild", 1, 2, IFO_NOT_MANY | IFO_NOT_NULL | AF_NOT_DELETED,
		IFO_NOT_MANY | IFO_NOT_NULL | IFO_NOT_ROOT | AF_NOT_ANCESTOR |
		AF_NOT_EQUAL | AF_NOT_DELETED, 0, "item ?newFirstChild?",
		NULL },
	{ "id", 1, 1, 0, 0, 0, "item", NULL },
	{ "image", 1, 100000, IFO_NOT_NULL, AF_NOT_ITEM, AF_NOT_ITEM,
		"item ?column? ?image? ?column image ...?", NULL },
	{ "isancestor", 2, 2, IFO_NOT_MANY | IFO_NOT_NULL, IFO_NOT_MANY |
		IFO_NOT_NULL, 0, "item item2", NULL },
	{ "isopen", 1, 1, IFO_NOT_MANY | IFO_NOT_NULL, 0, 0, "item", NULL },
	{ "lastchild", 1, 2, IFO_NOT_MANY | IFO_NOT_NULL | AF_NOT_DELETED,
		IFO_NOT_MANY | IFO_NOT_NULL | IFO_NOT_ROOT | AF_NOT_ANCESTOR |
		AF_NOT_EQUAL | AF_NOT_DELETED, 0, "item ?newLastChild?", NULL },
	{ "nextsibling", 1, 2, IFO_NOT_MANY | IFO_NOT_NULL | IFO_NOT_ROOT |
		IFO_NOT_ORPHAN, IFO_NOT_MANY | IFO_NOT_NULL | IFO_NOT_ROOT |
		AF_NOT_ANCESTOR | AF_NOT_EQUAL, 0, "item ?newNextSibling?",
		NULL },
	{ "numchildren", 1, 1, IFO_NOT_MANY | IFO_NOT_NULL, 0, 0, "item",
		NULL },
	{ "order", 1, 2, IFO_NOT_MANY | IFO_NOT_NULL, AF_NOT_ITEM, 0,
		"item ?-visible?", NULL },
	{ "parent", 1, 1, IFO_NOT_MANY | IFO_NOT_NULL, 0, 0, "item", NULL },
	{ "prevsibling", 1, 2, IFO_NOT_MANY | IFO_NOT_NULL | IFO_NOT_ROOT |
		IFO_NOT_ORPHAN, IFO_NOT_MANY | IFO_NOT_NULL | IFO_NOT_ROOT |
		AF_NOT_ANCESTOR | AF_NOT_EQUAL, 0, "item ?newPrevSibling?",
		NULL },
	{ "range", 2, 2, IFO_NOT_MANY | IFO_NOT_NULL, IFO_NOT_MANY |
		IFO_NOT_NULL | AF_SAMEROOT, 0, "first last", NULL },
	{ "remove", 1, 1, IFO_NOT_NULL | IFO_NOT_ROOT, 0, 0, "item", NULL },
	{ "rnc", 1, 1, IFO_NOT_MANY | IFO_NOT_NULL, 0, 0, "item", NULL },
	{ "sort", 0, 0, 0, 0, 0, NULL, ItemSortCmd },
	{ "span", 1, 100000, IFO_NOT_NULL, AF_NOT_ITEM, AF_NOT_ITEM,
		"item ?column? ?span? ?column span ...?", NULL },
	{ "state", 0, 0, 0, 0, 0, NULL, ItemStateCmd },
	{ "style", 0, 0, 0, 0, 0, NULL, ItemStyleCmd },
	{ "tag", 0, 0, 0, 0, 0, NULL, ItemTagCmd },
	{ "text", 1, 100000, IFO_NOT_NULL, AF_NOT_ITEM, AF_NOT_ITEM,
		"item ?column? ?text? ?column text ...?", NULL },
	{ "toggle", 1, 2, IFO_NOT_NULL, AF_NOT_ITEM, 0, "item ?-recurse?",
		NULL},
	{ NULL }
    };
    int index;
    int numArgs = objc - 3;
    TreeItemList itemList, item2List;
    TreeItem item = NULL, item2 = NULL, child;
    ItemForEach iter;
    int result = TCL_OK;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObjStruct(interp, objv[2], argInfo, sizeof(argInfo[0]),
	    "command", 0, &index) != TCL_OK) {
	return TCL_ERROR;
    }

    if (argInfo[index].proc != NULL)
	return argInfo[index].proc(clientData, interp, objc, objv);

    if ((numArgs < argInfo[index].minArgs) ||
	    (numArgs > argInfo[index].maxArgs)) {
	Tcl_WrongNumArgs(interp, 3, objv, argInfo[index].argString);
	return TCL_ERROR;
    }

    TreeItemList_Init(tree, &itemList, 0);
    TreeItemList_Init(tree, &item2List, 0);

    if ((numArgs >= 1) && !(argInfo[index].flags & AF_NOT_ITEM)) {
	if (TreeItemList_FromObj(tree, objv[3], &itemList,
		    argInfo[index].flags & 0xFFFF) != TCL_OK) {
	    goto errorExit;
	}
	item = TreeItemList_Nth(&itemList, 0); /* May be NULL. */
	if ((argInfo[index].flags & AF_NOT_DELETED) && IS_DELETED(item)) {
	    FormatResult(interp, "item %s%d is being deleted",
		    tree->itemPrefix, item->id);
	    goto errorExit;
	}
    }
    if (((numArgs >= 2) && !(argInfo[index].flags2 & AF_NOT_ITEM)) ||
	    ((numArgs >= 3) && !(argInfo[index].flags3 & AF_NOT_ITEM))) {
	int flags, obji;
	if (argInfo[index].flags2 & AF_NOT_ITEM) {
	    obji = 5;
	    flags = argInfo[index].flags3;
	} else {
	    obji = 4;
	    flags = argInfo[index].flags2;
	}
	if (TreeItemList_FromObj(tree, objv[obji], &item2List,
		flags & 0xFFFF) != TCL_OK) {
	    goto errorExit;
	}
	ITEM_FOR_EACH(item2, &item2List, NULL, &iter) {
	    if ((flags & AF_NOT_DELETED) && IS_DELETED(item2)) {
		FormatResult(interp, "item %s%d is being deleted",
			tree->itemPrefix, item2->id);
		goto errorExit;
	    }
	    if ((flags & AF_NOT_EQUAL) && (item == item2)) {
		FormatResult(interp, "item %s%d same as second item", tree->itemPrefix,
			item->id);
		goto errorExit;
	    }
	    if ((argInfo[index].flags & AF_NOT_ANCESTOR) &&
		    TreeItem_IsAncestor(tree, item, item2)) {
		FormatResult(interp, "item %s%d is ancestor of item %s%d",
			tree->itemPrefix, item->id, tree->itemPrefix, item2->id);
		goto errorExit;
	    }
	    if ((flags & AF_NOT_ANCESTOR) &&
		    TreeItem_IsAncestor(tree, item2, item)) {
		FormatResult(interp, "item %s%d is ancestor of item %s%d",
			tree->itemPrefix, item2->id, tree->itemPrefix, item->id);
		goto errorExit;
	    }
	    if ((flags & AF_SAMEROOT) &&
		    TreeItem_RootAncestor(tree, item) !=
		    TreeItem_RootAncestor(tree, item2)) {
reqSameRoot:
		FormatResult(interp,
			"item %s%d and item %s%d don't share a common ancestor",
			tree->itemPrefix, item->id, tree->itemPrefix, item2->id);
		goto errorExit;
	    }
	}
	item2 = TreeItemList_Nth(&item2List, 0); /* May be NULL. */
    }

    switch (index) {
	case COMMAND_ANCESTORS: {
	    Tcl_Obj *listObj;
	    TreeItem parent = item->parent;

	    if (parent == NULL)
		break; /* empty list */
	    listObj = Tcl_NewListObj(0, NULL);
	    while (parent != NULL) {
		Tcl_ListObjAppendElement(interp, listObj,
			TreeItem_ToObj(tree, parent));
		parent = parent->parent;
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}
	/* T item bbox I ?C? ?E? */
	case COMMAND_BBOX: {
	    int count;
	    TreeColumn treeColumn;
	    TreeRectangle rect;

	    if (objc == 4) {
		if (Tree_ItemBbox(tree, item, COLUMN_LOCK_NONE, &rect) < 0)
		    break;
	    } else {
		if (TreeColumn_FromObj(tree, objv[4], &treeColumn,
			CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
		    goto errorExit;

		/* Bounds of a column. */
		if (objc == 5) {
		    objc = 0;
		    objv = NULL;

		/* Single element in a column. */
		} else {
		    objc -= 5;
		    objv += 5;
		}

		count = TreeItem_GetRects(tree, item, treeColumn,
			objc, objv, &rect);
		if (count == 0)
		    break;
		if (count == -1)
		    goto errorExit;
	    }
	    FormatResult(interp, "%d %d %d %d",
		    TreeRect_Left(rect) - tree->xOrigin,
		    TreeRect_Top(rect) - tree->yOrigin,
		    TreeRect_Left(rect) - tree->xOrigin + TreeRect_Width(rect),
		    TreeRect_Top(rect) - tree->yOrigin + TreeRect_Height(rect));
	    break;
	}
	/* T item buttonstate I ?state? */
	case COMMAND_BUTTONSTATE: {
	    static const char *stateNames[] = {
		"active", "normal", "pressed", NULL
	    };
	    int state;
	    if (numArgs == 2) {
		int prevFlags = item->flags;
		if (Tcl_GetIndexFromObj(interp, objv[4], stateNames, "state",
			0, &state) != TCL_OK) {
		    goto errorExit;
		}
		item->flags &= ~ITEM_FLAGS_BUTTONSTATE;
		if (state == 0)
		    item->flags |= ITEM_FLAG_BUTTONSTATE_ACTIVE;
		else if (state == 2)
		    item->flags |= ITEM_FLAG_BUTTONSTATE_PRESSED;
		if (item->flags != prevFlags && tree->columnTree != NULL)
		    Tree_InvalidateItemDInfo(tree, tree->columnTree, item, NULL);
	    } else {
		if (item->flags & ITEM_FLAG_BUTTONSTATE_ACTIVE)
		    state = 0;
		else if (item->flags & ITEM_FLAG_BUTTONSTATE_PRESSED)
		    state = 2;
		else
		    state = 1;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(stateNames[state], -1));
	    break;
	}
	case COMMAND_CGET: {
	    Tcl_Obj *resultObjPtr;

	    resultObjPtr = Tk_GetOptionValue(interp, (char *) item,
		    tree->itemOptionTable, objv[4], tree->tkwin);
	    if (resultObjPtr == NULL)
		goto errorExit;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}
	case COMMAND_CHILDREN: {
	    if (item->numChildren != 0) {
		Tcl_Obj *listObj;

		listObj = Tcl_NewListObj(0, NULL);
		child = item->firstChild;
		while (child != NULL) {
		    Tcl_ListObjAppendElement(interp, listObj,
			    TreeItem_ToObj(tree, child));
		    child = child->nextSibling;
		}
		Tcl_SetObjResult(interp, listObj);
	    }
	    break;
	}
	case COMMAND_COLLAPSE:
	case COMMAND_EXPAND:
	case COMMAND_TOGGLE: {
	    int animate = 0;
	    int recurse = 0;
	    int mode = 0; /* lint */
	    int i, count;
	    TreeItemList items;

	    if (numArgs > 1) {
		static const char *optionName[] = { "-animate", "-recurse",
		    NULL };
		int option;
		for (i = 4; i < objc; i++) {
		    if (Tcl_GetIndexFromObj(interp, objv[i], optionName,
			    "option", 0, &option) != TCL_OK) {
			goto errorExit;
		    }
		    switch (option) {
			case 0: /* -animate */
			    animate = 1;
			    break;
			case 1: /* -recurse */
			    recurse = 1;
			    break;
		    }
		}
	    }
	    switch (index) {
		case COMMAND_COLLAPSE:
		    mode = 0;
		    break;
		case COMMAND_EXPAND:
		    mode = 1;
		    break;
		case COMMAND_TOGGLE:
		    mode = -1;
		    break;
	    }
	    if (animate) {
		Tk_Image image;
		Pixmap bitmap;
		int open;
		if (IS_ALL(item) || TreeItemList_Count(&itemList) != 1) {
		    FormatResult(interp,
			"only 1 item may be specified with -animate");
		    goto errorExit;
		}
		image = PerStateImage_ForState(tree, &tree->buttonImage, item->state, NULL);
		bitmap = PerStateBitmap_ForState(tree, &tree->buttonBitmap, item->state, NULL);
		if (image != NULL || bitmap != None || !tree->useTheme
			|| !TreeItem_HasButton(tree, item)) {
		    TreeItem_OpenClose(tree, item, mode);
		    break;
		}
		open = (item->state & STATE_OPEN) != 0;
		if (mode == -1 || open != mode) {
		    (void) TreeTheme_AnimateButtonStart(tree, item);
		}
		break;
	    }
	    TreeItemList_Init(tree, &items, 0);
	    ITEM_FOR_EACH(item, &itemList, NULL, &iter) {
		TreeItemList_Append(&items, item);
		if (!iter.all && recurse) {
		    TreeItem_ListDescendants(tree, item, &items);
		}
	    }
	    count = TreeItemList_Count(&items);
	    for (i = 0; i < count; i++) {
		item = TreeItemList_Nth(&items, i);
		TreeItem_OpenClose(tree, item, mode);
	    }
	    TreeItemList_Free(&items);
#ifdef SELECTION_VISIBLE
	    Tree_DeselectHidden(tree);
#endif
	    break;
	}
	/* T item compare I op I */
	case COMMAND_COMPARE: {
	    static CONST char *opName[] = { "<", "<=", "==", ">=", ">", "!=", NULL };
	    enum { COP_LT, COP_LE, COP_EQ, COP_GE, COP_GT, COP_NE };
	    int op, compare = 0, index1 = 0, index2 = 0;

	    if (Tcl_GetIndexFromObj(interp, objv[4], opName, "comparison operator", 0,
		    &op) != TCL_OK) {
		goto errorExit;
	    }
	    if (op != COP_EQ && op != COP_NE) {
		if (TreeItem_RootAncestor(tree, item) !=
		    TreeItem_RootAncestor(tree, item2)) {
		    goto reqSameRoot;
		}
		TreeItem_ToIndex(tree, item, &index1, NULL);
		TreeItem_ToIndex(tree, item2, &index2, NULL);
	    }
	    switch (op) {
		case COP_LT:
		    compare = index1 < index2;
		    break;
		case COP_LE:
		    compare = index1 <= index2;
		    break;
		case COP_EQ:
		    compare = item == item2;
		    break;
		case COP_GE:
		    compare = index1 >= index2;
		    break;
		case COP_GT:
		    compare = index1 > index2;
		    break;
		case COP_NE:
		    compare = item != item2;
		    break;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(compare));
	    break;
	}

#ifdef DEPRECATED
	case COMMAND_COMPLEX: {
	    int i, j, columnIndex;
	    int objc1, objc2;
	    Tcl_Obj **objv1, **objv2;
	    TreeColumn treeColumn = tree->columns;
	    Column *column;
	    int eMask, cMask, iMask = 0;

	    if (objc <= 4)
		break;
	    columnIndex = 0;
	    for (i = 4; i < objc; i++, columnIndex++,
		    treeColumn = TreeColumn_Next(treeColumn)) {
		if (treeColumn == NULL) {
		    FormatResult(interp, "column #%d doesn't exist",
			    columnIndex);
		    result = TCL_ERROR;
		    goto doneComplex;
		}
		column = Item_FindColumn(tree, item, columnIndex);
		if (column == NULL) {
		    FormatResult(interp, "item %s%d doesn't have column %s%d",
			    tree->itemPrefix, item->id,
			    tree->columnPrefix, TreeColumn_GetID(treeColumn));
		    result = TCL_ERROR;
		    goto doneComplex;
		}
		/* List of element-configs per column */
		if (Tcl_ListObjGetElements(interp, objv[i],
			    &objc1, &objv1) != TCL_OK) {
		    result = TCL_ERROR;
		    goto doneComplex;
		}
		if (objc1 == 0)
		    continue;
		if (column->style == NULL) {
		    FormatResult(interp, "item %s%d column %s%d has no style",
			    tree->itemPrefix, item->id,
			    tree->columnPrefix, TreeColumn_GetID(treeColumn));
		    result = TCL_ERROR;
		    goto doneComplex;
		}
		cMask = 0;
		for (j = 0; j < objc1; j++) {
		    /* elem option value... */
		    if (Tcl_ListObjGetElements(interp, objv1[j],
				&objc2, &objv2) != TCL_OK) {
			result = TCL_ERROR;
			goto doneComplex;
		    }
		    if (objc2 < 3) {
			FormatResult(interp,
				"wrong # args: should be \"element option value ...\"");
			result = TCL_ERROR;
			goto doneComplex;
		    }
		    if (TreeStyle_ElementConfigure(tree, item,
				(TreeItemColumn) column, column->style,
				objv2[0], objc2 - 1, objv2 + 1, &eMask) != TCL_OK) {
			result = TCL_ERROR;
			goto doneComplex;
		    }
		    cMask |= eMask;
		    iMask |= eMask;
		}
		if (cMask & CS_LAYOUT)
		    TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
	    }
	    doneComplex:
	    if (iMask & CS_DISPLAY)
		Tree_InvalidateItemDInfo(tree, NULL, item, NULL);
	    if (iMask & CS_LAYOUT) {
		Tree_InvalidateColumnWidth(tree, NULL);
		TreeItem_InvalidateHeight(tree, item);
		Tree_FreeItemDInfo(tree, item, NULL);
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	    }
	    break;
	}
#endif /* DEPRECATED*/

	/* T item configure I ?option? ?value? ?option value ...? */
	case COMMAND_CONFIGURE: {
	    if (objc <= 5) {
		Tcl_Obj *resultObjPtr;

		if (IS_ALL(item) || (TreeItemList_Count(&itemList) > 1)) {
		    FormatResult(interp, "can't specify > 1 item for this command");
		    goto errorExit;
		}
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) item,
			tree->itemOptionTable,
			(objc == 4) ? (Tcl_Obj *) NULL : objv[4],
			tree->tkwin);
		if (resultObjPtr == NULL)
		    goto errorExit;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    ITEM_FOR_EACH(item, &itemList, NULL, &iter) {
		result = Item_Configure(tree, item, objc - 4, objv + 4);
		if (result != TCL_OK)
		    break;
	    }
	    break;
	}
	case COMMAND_COUNT: {
	    int count = tree->itemCount;

	    if (objc == 4) {
		count = 0;
		ITEM_FOR_EACH(item, &itemList, &item2List, &iter) {
		    count++;
		}
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(count));
	    break;
	}
	case COMMAND_DELETE: {
	    TreeItemList deleted, selected;
	    int i, count;

	    /* The root is never deleted */
	    if (tree->itemCount == 1)
		break;

	    if (objc == 5) {
		if (item == NULL || item2 == NULL) {
		    FormatResult(interp, "invalid range \"%s\" to \"%s\"",
			    Tcl_GetString(objv[3]), Tcl_GetString(objv[4]));
		    goto errorExit;
		}
	    }

	    /*
	     * ITEM_FLAG_DELETED prevents us from adding the same item
	     * twice to the 'deleted' list. It also prevents nested
	     * calls to this command (through binding scripts) deleting
	     * the same item twice.
	     */

	    TreeItemList_Init(tree, &deleted, tree->itemCount - 1);
	    TreeItemList_Init(tree, &selected, tree->selectCount);

	    ITEM_FOR_EACH(item, &itemList, &item2List, &iter) {
		if (IS_ROOT(item))
		    continue;
		if (IS_DELETED(item))
		    continue;
		item->flags |= ITEM_FLAG_DELETED;
		TreeItemList_Append(&deleted, item);
		if (TreeItem_GetSelected(tree, item))
		    TreeItemList_Append(&selected, item);
		if (iter.all)
		    continue;
		/* Check every descendant. */
		if (item->firstChild == NULL)
		    continue;
		item2 = item;
		while (item2->lastChild != NULL)
		    item2 = item2->lastChild;
		item = item->firstChild;
		while (1) {
		    if (IS_DELETED(item)) {
			/* Skip all descendants (they are already flagged). */
			while (item->lastChild != NULL)
			    item = item->lastChild;
		    } else {
			item->flags |= ITEM_FLAG_DELETED;
			TreeItemList_Append(&deleted, item);
			if (TreeItem_GetSelected(tree, item))
			    TreeItemList_Append(&selected, item);
		    }
		    if (item == item2)
			break;
		    item = TreeItem_Next(tree, item);
		}
	    }

	    count = TreeItemList_Count(&selected);
	    if (count) {
		for (i = 0; i < count; i++) {
		    item = TreeItemList_Nth(&selected, i);
		    Tree_RemoveFromSelection(tree, item);
		}
		/* Generate <Selection> event for selected items being deleted. */
		TreeNotify_Selection(tree, NULL, &selected);
	    }

	    count = TreeItemList_Count(&deleted);
	    if (count) {
		int redoColumns = 0;

		/* Generate <ItemDelete> event for items being deleted. */
		TreeNotify_ItemDeleted(tree, &deleted);

		/* Remove every item from its parent. Needed because items
		 * are deleted recursively. */
		for (i = 0; i < count; i++) {
		    item = TreeItemList_Nth(&deleted, i);

		    /* Deleting a visible item may change the needed width
		     * of any column. */
		    if (TreeItem_ReallyVisible(tree, item))
			redoColumns = 1;

		    TreeItem_RemoveFromParent(tree, item);
		}

		/* Delete the items. The item record will be freed when no
		 * longer in use; however, the item cannot be referred to
		 * by commands from this point on. */
		for (i = 0; i < count; i++) {
		    item = TreeItemList_Nth(&deleted, i);
		    TreeItem_Delete(tree, item);
		}

		if (redoColumns)
		    Tree_InvalidateColumnWidth(tree, NULL);
	    }

	    TreeItemList_Free(&selected);
	    TreeItemList_Free(&deleted);
	    break;
	}
	case COMMAND_DESCENDANTS: {
	    Tcl_Obj *listObj;

	    if (item->firstChild == NULL)
		break;
	    item2 = item;
	    while (item2->lastChild != NULL)
		item2 = item2->lastChild;
	    item = item->firstChild;
	    listObj = Tcl_NewListObj(0, NULL);
	    while (1) {
		Tcl_ListObjAppendElement(interp, listObj,
			TreeItem_ToObj(tree, item));
		if (item == item2)
		    break;
		item = TreeItem_Next(tree, item);
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}
	case COMMAND_DUMP: {
	    Tree_UpdateItemIndex(tree);
	    FormatResult(interp, "index %d indexVis %d",
		    item->index, item->indexVis);
	    break;
	}
	/* T item enabled I ?boolean? */
	case COMMAND_ENABLED: {
	    int enabled;
	    TreeItemList newD;
	    int stateOff, stateOn;

	    if (objc == 4) {
		if (IS_ALL(item) || (TreeItemList_Count(&itemList) > 1)) {
		    FormatResult(interp, "can't specify > 1 item for this command");
		    goto errorExit;
		}
		Tcl_SetObjResult(interp,
			Tcl_NewBooleanObj(item->state & STATE_ENABLED));
		break;
	    }
	    if (Tcl_GetBooleanFromObj(interp, objv[4], &enabled) != TCL_OK)
		goto errorExit;
	    stateOff = enabled ? 0 : STATE_ENABLED;
	    stateOn = enabled ? STATE_ENABLED : 0;
	    TreeItemList_Init(tree, &newD, tree->selectCount);
	    ITEM_FOR_EACH(item, &itemList, NULL, &iter) {
		if (enabled != TreeItem_GetEnabled(tree, item)) {
		    TreeItem_ChangeState(tree, item, stateOff, stateOn);
		    /* Disabled items cannot be selected. */
		    if (!enabled && TreeItem_GetSelected(tree, item)) {
			Tree_RemoveFromSelection(tree, item);
			TreeItemList_Append(&newD, item);
		    }
		}
	    }
	    if (TreeItemList_Count(&newD))
		TreeNotify_Selection(tree, NULL, &newD);
	    TreeItemList_Free(&newD);
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(enabled));
	    break;
	}
	case COMMAND_FIRSTCHILD: {
	    if (item2 != NULL && item2 != item->firstChild) {
		TreeItem_RemoveFromParent(tree, item2);
		item2->nextSibling = item->firstChild;
		if (item->firstChild != NULL)
		    item->firstChild->prevSibling = item2;
		else
		    item->lastChild = item2;
		item->firstChild = item2;
		item2->parent = item;
		item->numChildren++;
		TreeItem_AddToParent(tree, item2);
#ifdef SELECTION_VISIBLE
		Tree_DeselectHidden(tree);
#endif
	    }
	    if (item->firstChild != NULL)
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, item->firstChild));
	    break;
	}
	/* T item id I */
	case COMMAND_ID: {
	    Tcl_Obj *listObj;

	    listObj = Tcl_NewListObj(0, NULL);
	    ITEM_FOR_EACH(item, &itemList, NULL, &iter) {
		Tcl_ListObjAppendElement(interp, listObj,
			TreeItem_ToObj(tree, item));
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}
	case COMMAND_ISANCESTOR: {
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(
				 TreeItem_IsAncestor(tree, item, item2)));
	    break;
	}
	case COMMAND_ISOPEN: {
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(item->state & STATE_OPEN));
	    break;
	}
	case COMMAND_LASTCHILD: {
	    /* Don't allow non-deleted items to become children of a
	     * deleted item. */
	    if (item2 != NULL && item2 != item->lastChild) {
		TreeItem_RemoveFromParent(tree, item2);
		item2->prevSibling = item->lastChild;
		if (item->lastChild != NULL)
		    item->lastChild->nextSibling = item2;
		else
		    item->firstChild = item2;
		item->lastChild = item2;
		item2->parent = item;
		item->numChildren++;
		TreeItem_AddToParent(tree, item2);
#ifdef SELECTION_VISIBLE
		Tree_DeselectHidden(tree);
#endif
	    }
	    if (item->lastChild != NULL)
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, item->lastChild));
	    break;
	}
	case COMMAND_NEXTSIBLING: {
	    if (item2 != NULL && item2 != item->nextSibling) {
		TreeItem_RemoveFromParent(tree, item2);
		item2->prevSibling = item;
		if (item->nextSibling != NULL) {
		    item->nextSibling->prevSibling = item2;
		    item2->nextSibling = item->nextSibling;
		} else
		    item->parent->lastChild = item2;
		item->nextSibling = item2;
		item2->parent = item->parent;
		item->parent->numChildren++;
		TreeItem_AddToParent(tree, item2);
#ifdef SELECTION_VISIBLE
		Tree_DeselectHidden(tree);
#endif
	    }
	    if (item->nextSibling != NULL)
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, item->nextSibling));
	    break;
	}
	case COMMAND_NUMCHILDREN: {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(item->numChildren));
	    break;
	}
	/* T item order I ?-visible? */
	case COMMAND_ORDER: {
	    int visible = FALSE;
	    if (objc == 5) {
		int len;
		char *s = Tcl_GetStringFromObj(objv[4], &len);
		if ((s[0] == '-') && (strncmp(s, "-visible", len) == 0))
		    visible = TRUE;
		else {
		    FormatResult(interp, "bad switch \"%s\": must be -visible",
			s);
		    goto errorExit;
		}
	    }
	    Tree_UpdateItemIndex(tree);
	    Tcl_SetObjResult(interp,
		    Tcl_NewIntObj(visible ? item->indexVis : item->index));
	    break;
	}
	/* T item range I I */
	case COMMAND_RANGE: {
	    TreeItem itemFirst = item;
	    TreeItem itemLast = item2;
	    Tcl_Obj *listObj;

	    if (itemFirst == itemLast) {
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, itemFirst));
		break;
	    }
	    (void) TreeItem_FirstAndLast(tree, &itemFirst, &itemLast);
	    listObj = Tcl_NewListObj(0, NULL);
	    while (itemFirst != NULL) {
		Tcl_ListObjAppendElement(interp, listObj,
			TreeItem_ToObj(tree, itemFirst));
		if (itemFirst == itemLast)
		    break;
		itemFirst = TreeItem_Next(tree, itemFirst);
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}
	case COMMAND_PARENT: {
	    if (item->parent != NULL)
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, item->parent));
	    break;
	}
	case COMMAND_PREVSIBLING: {
	    if (item2 != NULL && item2 != item->prevSibling) {
		TreeItem_RemoveFromParent(tree, item2);
		item2->nextSibling = item;
		if (item->prevSibling != NULL) {
		    item->prevSibling->nextSibling = item2;
		    item2->prevSibling = item->prevSibling;
		} else
		    item->parent->firstChild = item2;
		item->prevSibling = item2;
		item2->parent = item->parent;
		item->parent->numChildren++;
		TreeItem_AddToParent(tree, item2);
#ifdef SELECTION_VISIBLE
		Tree_DeselectHidden(tree);
#endif
	    }
	    if (item->prevSibling != NULL)
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, item->prevSibling));
	    break;
	}
	case COMMAND_REMOVE: {
	    int removed = FALSE;

	    ITEM_FOR_EACH(item, &itemList, NULL, &iter) {
		if (item->parent != NULL) {
		    TreeItem_RemoveFromParent(tree, item);
		    Tree_FreeItemDInfo(tree, item, NULL);
		    removed = TRUE;
		}
	    }
	    if (!removed)
		break;
	    if (tree->debug.enable && tree->debug.data)
		Tree_Debug(tree);
	    Tree_InvalidateColumnWidth(tree, NULL);
#ifdef SELECTION_VISIBLE
	    Tree_DeselectHidden(tree);
#endif
	    break;
	}
	case COMMAND_RNC: {
	    int row,col;

	    if (Tree_ItemToRNC(tree, item, &row, &col) == TCL_OK)
		FormatResult(interp, "%d %d", row, col);
	    break;
	}

	/* T item span I ?C? ?span? ?C span ...? */
	case COMMAND_SPAN: {
	    TreeColumn treeColumn = tree->columns;
	    Column *column;
	    Tcl_Obj *listObj;
	    struct columnSpan {
		TreeColumnList columns;
		int span;
	    } staticCS[STATIC_SIZE], *cs = staticCS;
	    int i, count = 0, span, changed = FALSE;
	    ColumnForEach citer;

	    if ((objc < 6) && (IS_ALL(item) ||
		    (TreeItemList_Count(&itemList) > 1))) {
		FormatResult(interp, "can't specify > 1 item for this command");
		goto errorExit;
	    }
	    if (objc == 4) {
		listObj = Tcl_NewListObj(0, NULL);
		column = item->columns;
		while (treeColumn != NULL) {
		    Tcl_ListObjAppendElement(interp, listObj,
			    Tcl_NewIntObj(column ? column->span : 1));
		    treeColumn = TreeColumn_Next(treeColumn);
		    if (column != NULL)
			column = column->next;
		}
		Tcl_SetObjResult(interp, listObj);
		break;
	    }
	    if (objc == 5) {
		if (Item_FindColumnFromObj(tree, item, objv[4], &column, NULL) != TCL_OK) {
		    goto errorExit;
		}
		Tcl_SetObjResult(interp, Tcl_NewIntObj(column ? column->span : 1));
		break;
	    }
	    if ((objc - 4) & 1) {
		FormatResult(interp, "missing argument after column \"%s\"",
			Tcl_GetString(objv[objc - 1]));
		goto errorExit;
	    }
	    /* Gather column/span pairs. */
	    STATIC_ALLOC(cs, struct columnSpan, objc / 2);
	    for (i = 4; i < objc; i += 2) {
		if (TreeColumnList_FromObj(tree, objv[i], &cs[count].columns,
			CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK) {
		    result = TCL_ERROR;
		    goto doneSPAN;
		}
		if (Tcl_GetIntFromObj(interp, objv[i + 1], &span) != TCL_OK) {
		    result = TCL_ERROR;
		    goto doneSPAN;
		}
		if (span <= 0) {
		    FormatResult(interp, "bad span \"%d\": must be > 0", span);
		    result = TCL_ERROR;
		    goto doneSPAN;
		}
		cs[count].span = span;
		count++;
	    }
	    ITEM_FOR_EACH(item, &itemList, NULL, &iter) {
		int changedI = FALSE;
		for (i = 0; i < count; i++) {
		    COLUMN_FOR_EACH(treeColumn, &cs[i].columns, NULL, &citer) {
			column = Item_CreateColumn(tree, item,
				TreeColumn_Index(treeColumn), NULL);
			if (column->span != cs[i].span) {
			    if (cs[i].span > 1) {
				item->flags &= ~ITEM_FLAG_SPANS_SIMPLE;
			    }
			    TreeItem_SpansInvalidate(tree, item);
			    column->span = cs[i].span;
			    TreeItemColumn_InvalidateSize(tree,
				(TreeItemColumn) column);
			    changedI = TRUE;
			    Tree_InvalidateColumnWidth(tree, treeColumn);
			}
		    }
		}
		if (changedI) {
		    TreeItem_InvalidateHeight(tree, item);
		    Tree_FreeItemDInfo(tree, item, NULL);
		    changed = TRUE;
		}
	    }
	    if (changed)
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
doneSPAN:
	    for (i = 0; i < count; i++) {
		TreeColumnList_Free(&cs[i].columns);
	    }
	    STATIC_FREE(cs, struct columnSpan, objc / 2);
	    break;
	}

	/* T item image I ?C? ?image? ?C image ...? */
	case COMMAND_IMAGE:
	/* T item text I ?C? ?text? ?C text ...? */
	case COMMAND_TEXT: {
	    TreeColumn treeColumn = tree->columns;
	    Column *column;
	    Tcl_Obj *objPtr;
	    int isImage = (index == COMMAND_IMAGE);
	    struct columnObj {
		TreeColumnList columns;
		Tcl_Obj *obj;
	    } staticCO[STATIC_SIZE], *co = staticCO;
	    int i, count = 0, changed = FALSE, columnIndex;
	    ColumnForEach citer;

	    if ((objc < 6) && (IS_ALL(item) ||
		    (TreeItemList_Count(&itemList) > 1))) {
		FormatResult(interp, "can't specify > 1 item for this command");
		goto errorExit;
	    }
	    if (objc == 4) {
		Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
		column = item->columns;
		while (treeColumn != NULL) {
		    if ((column != NULL) && (column->style != NULL))
			objPtr = isImage ?
			    TreeStyle_GetImage(tree, column->style) :
			    TreeStyle_GetText(tree, column->style);
		    else
			objPtr = NULL;
		    if (objPtr == NULL)
			objPtr = Tcl_NewObj();
		    Tcl_ListObjAppendElement(interp, listObj, objPtr);
		    treeColumn = TreeColumn_Next(treeColumn);
		    if (column != NULL)
			column = column->next;
		}
		Tcl_SetObjResult(interp, listObj);
		break;
	    }
	    if (objc == 5) {
		if (Item_FindColumnFromObj(tree, item, objv[4], &column, NULL) != TCL_OK) {
		    goto errorExit;
		}
		if ((column != NULL) && (column->style != NULL)) {
		    objPtr = isImage ?
			TreeStyle_GetImage(tree, column->style) :
			TreeStyle_GetText(tree, column->style);
		    if (objPtr != NULL)
			Tcl_SetObjResult(interp, objPtr);
		}
		break;
	    }
	    if ((objc - 4) & 1) {
		FormatResult(interp, "missing argument after column \"%s\"",
			Tcl_GetString(objv[objc - 1]));
		goto errorExit;
	    }
	    /* Gather column/obj pairs. */
	    STATIC_ALLOC(co, struct columnObj, objc / 2);
	    for (i = 4; i < objc; i += 2) {
		if (TreeColumnList_FromObj(tree, objv[i], &co[count].columns,
			CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK) {
		    result = TCL_ERROR;
		    goto doneTEXT;
		}
		co[count].obj = objv[i + 1];
		count++;
	    }
	    ITEM_FOR_EACH(item, &itemList, NULL, &iter) {
		int changedI = FALSE;
		for (i = 0; i < count; i++) {
		    COLUMN_FOR_EACH(treeColumn, &co[i].columns, NULL, &citer) {
			columnIndex = TreeColumn_Index(treeColumn);
			column = Item_FindColumn(tree, item, columnIndex);
			if ((column == NULL) || (column->style == NULL)) {
			    NoStyleMsg(tree, item, columnIndex);
			    result = TCL_ERROR;
			    goto doneTEXT;
			}
			result = isImage ?
			    TreeStyle_SetImage(tree, item,
				(TreeItemColumn) column, column->style, co[i].obj) :
			    TreeStyle_SetText(tree, item,
				(TreeItemColumn) column, column->style, co[i].obj);
			if (result != TCL_OK)
			    goto doneTEXT;
			TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
			Tree_InvalidateColumnWidth(tree, treeColumn);
			changedI = TRUE;
		    }
		}
		if (changedI) {
		    TreeItem_InvalidateHeight(tree, item);
		    Tree_FreeItemDInfo(tree, item, NULL);
		    changed = TRUE;
		}
	    }
	    if (changed)
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
doneTEXT:
	    for (i = 0; i < count; i++) {
		TreeColumnList_Free(&co[i].columns);
	    }
	    STATIC_FREE(co, struct columnObj, objc / 2);
	    break;
	}
    }

    TreeItemList_Free(&itemList);
    TreeItemList_Free(&item2List);
    return result;

errorExit:
    TreeItemList_Free(&itemList);
    TreeItemList_Free(&item2List);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Debug --
 *
 *	Perform some sanity checks on an Item and its descendants.
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
TreeItem_Debug(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    TreeItem child;
    Tcl_Interp *interp = tree->interp;
    int count;

    if (item->parent == item) {
	FormatResult(interp,
		"parent of %d is itself", item->id);
	return TCL_ERROR;
    }

    if (item->parent == NULL) {
	if (item->prevSibling != NULL) {
	    FormatResult(interp,
		    "parent of %d is nil, prevSibling is not nil",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->nextSibling != NULL) {
	    FormatResult(interp,
		    "parent of %d is nil, nextSibling is not nil",
		    item->id);
	    return TCL_ERROR;
	}
    }

    if (item->prevSibling != NULL) {
	if (item->prevSibling == item) {
	    FormatResult(interp,
		    "prevSibling of %d is itself",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->prevSibling->nextSibling != item) {
	    FormatResult(interp,
		    "item%d.prevSibling.nextSibling is not it",
		    item->id);
	    return TCL_ERROR;
	}
    }

    if (item->nextSibling != NULL) {
	if (item->nextSibling == item) {
	    FormatResult(interp,
		    "nextSibling of %d is itself",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->nextSibling->prevSibling != item) {
	    FormatResult(interp,
		    "item%d.nextSibling->prevSibling is not it",
		    item->id);
	    return TCL_ERROR;
	}
    }

    if (item->numChildren < 0) {
	FormatResult(interp,
		"numChildren of %d is %d",
		item->id, item->numChildren);
	return TCL_ERROR;
    }

    if (item->numChildren == 0) {
	if (item->firstChild != NULL) {
	    FormatResult(interp,
		    "item%d.numChildren is zero, firstChild is not nil",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->lastChild != NULL) {
	    FormatResult(interp,
		    "item%d.numChildren is zero, lastChild is not nil",
		    item->id);
	    return TCL_ERROR;
	}
    }

    if (item->numChildren > 0) {
	if (item->firstChild == NULL) {
	    FormatResult(interp,
		    "item%d.firstChild is nil",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->firstChild == item) {
	    FormatResult(interp,
		    "item%d.firstChild is itself",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->firstChild->parent != item) {
	    FormatResult(interp,
		    "item%d.firstChild.parent is not it",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->firstChild->prevSibling != NULL) {
	    FormatResult(interp,
		    "item%d.firstChild.prevSibling is not nil",
		    item->id);
	    return TCL_ERROR;
	}

	if (item->lastChild == NULL) {
	    FormatResult(interp,
		    "item%d.lastChild is nil",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->lastChild == item) {
	    FormatResult(interp,
		    "item%d.lastChild is itself",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->lastChild->parent != item) {
	    FormatResult(interp,
		    "item%d.lastChild.parent is not it",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->lastChild->nextSibling != NULL) {
	    FormatResult(interp,
		    "item%d.lastChild.nextSibling is not nil",
		    item->id);
	    return TCL_ERROR;
	}

	/* Count number of children */
	count = 0;
	child = item->firstChild;
	while (child != NULL) {
	    count++;
	    child = child->nextSibling;
	}
	if (count != item->numChildren) {
	    FormatResult(interp,
		    "item%d.numChildren is %d, but counted %d",
		    item->id, item->numChildren, count);
	    return TCL_ERROR;
	}

	/* Debug each child recursively */
	child = item->firstChild;
	while (child != NULL) {
	    if (child->parent != item) {
		FormatResult(interp,
			"child->parent of %d is not it",
			item->id);
		return TCL_ERROR;
	    }
	    if (TreeItem_Debug(tree, child) != TCL_OK)
		return TCL_ERROR;
	    child = child->nextSibling;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SpanWalkProc_Identify --
 *
 *	Callback routine to TreeItem_WalkSpans for TreeItem_Identify.
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
SpanWalkProc_Identify(
    TreeCtrl *tree,
    TreeItem item,
    SpanInfo *spanPtr,
    StyleDrawArgs *drawArgs,
    ClientData clientData
    )
{
    struct {
	int x;
	int y;
	char *buf;
    } *data = clientData;

    if ((data->x < drawArgs->x + drawArgs->indent) ||
	    (data->x >= drawArgs->x + drawArgs->width))
	return 0;

    sprintf(data->buf + strlen(data->buf), " column %s%d",
	    tree->columnPrefix, TreeColumn_GetID(spanPtr->treeColumn));

    if (drawArgs->style != NULL) {
	char *elem = TreeStyle_Identify(drawArgs, data->x, data->y);
	if (elem != NULL)
	    sprintf(data->buf + strlen(data->buf), " elem %s", elem);
    }
    return 1; /* stop */
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Identify --
 *
 *	Determine which column and element the given point is in.
 *	This is used by the [identify] widget command.
 *
 * Results:
 *	If the Item is not ReallyVisible() or no columns are visible
 *	then buf[] is untouched. Otherwise the given string may be
 *	appended with "column C" followed by "elem E" although both
 *	are optional.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_Identify(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int lock,			/* Columns to hit-test. */
    int x, int y,		/* Item coords to hit-test with. */
    char *buf			/* NULL-terminated string which may be
				 * appended. */
    )
{
    TreeRectangle tr;
    struct {
	int x;
	int y;
	char *buf;
    } clientData;

    if (Tree_ItemBbox(tree, item, lock, &tr) < 0)
	return;

    /* Tree_ItemBbox returns canvas coords. x/y are item coords. */
    clientData.x = x;
    clientData.y = y;
    clientData.buf = buf;

    TreeItem_WalkSpans(tree, item, lock,
	    0, 0, TreeRect_Width(tr), TreeRect_Height(tr),
	    SpanWalkProc_Identify, (ClientData) &clientData);
}

/*
 *----------------------------------------------------------------------
 *
 * SpanWalkProc_Identify2 --
 *
 *	Callback routine to TreeItem_WalkSpans for TreeItem_Identify2.
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
SpanWalkProc_Identify2(
    TreeCtrl *tree,
    TreeItem item,
    SpanInfo *spanPtr,
    StyleDrawArgs *drawArgs,
    ClientData clientData
    )
{
    Tcl_Obj *subListObj;
    struct {
	int x1; int y1;
	int x2; int y2;
	Tcl_Obj *listObj;
    } *data = clientData;

    if ((data->x2 < drawArgs->x + drawArgs->indent) ||
	    (data->x1 >= drawArgs->x + drawArgs->width))
	return 0;

    subListObj = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(tree->interp, subListObj,
	    TreeColumn_ToObj(tree, spanPtr->treeColumn));
    if (drawArgs->style != NULL) {
	StyleDrawArgs drawArgsCopy = *drawArgs;
	TreeStyle_Identify2(&drawArgsCopy, data->x1, data->y1,
		data->x2, data->y2,
		subListObj);
    }
    Tcl_ListObjAppendElement(tree->interp, data->listObj, subListObj);
    return drawArgs->x + drawArgs->width >= data->x2;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Identify2 --
 *
 *	Determine which columns and elements intersect the given
 *	area. This is used by the [marquee identify] widget command.
 *
 * Results:
 *	If the Item is not ReallyVisible() or no columns are visible
 *	then listObj is untouched. Otherwise the list is appended
 *	with C {E ...} C {E...}.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_Identify2(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int x1, int y1,		/* Top-left of area to hit-test. */
    int x2, int y2,		/* Bottom-right of area to hit-test. */
    Tcl_Obj *listObj		/* Initialized list object. */
    )
{
    TreeRectangle tr;
    struct {
	int x1; int y1;
	int x2; int y2;
	Tcl_Obj *listObj;
    } clientData;

    if (Tree_ItemBbox(tree, item, COLUMN_LOCK_NONE, &tr) < 0)
	return;

    /* Tree_ItemBbox returns canvas coords. x1 etc are canvas coords. */
    clientData.x1 = x1;
    clientData.y1 = y1;
    clientData.x2 = x2;
    clientData.y2 = y2;
    clientData.listObj = listObj;

    TreeItem_WalkSpans(tree, item, COLUMN_LOCK_NONE,
	    TreeRect_Left(tr), TreeRect_Top(tr),
	    TreeRect_Width(tr), TreeRect_Height(tr),
	    SpanWalkProc_Identify2, (ClientData) &clientData);
}

/*
 *----------------------------------------------------------------------
 *
 * SpanWalkProc_GetRects --
 *
 *	Callback routine to TreeItem_WalkSpans for TreeItem_GetRects.
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
SpanWalkProc_GetRects(
    TreeCtrl *tree,
    TreeItem item,
    SpanInfo *spanPtr,
    StyleDrawArgs *drawArgs,
    ClientData clientData
    )
{
    int objc;
    Tcl_Obj *CONST *objv;
    struct {
	TreeColumn treeColumn;
	int count;
	Tcl_Obj *CONST *objv;
	TreeRectangle *rects;
	int result;
   } *data = clientData;

    if (spanPtr->treeColumn != data->treeColumn)
	return 0;

    /* Bounds of span. */
    if (data->count == 0) {
	/* Not sure why I add 'indent' but for compatibility I will leave
	 * it in. */
	data->rects[0].x = drawArgs->x + drawArgs->indent;
	data->rects[0].y = drawArgs->y;
	data->rects[0].width = drawArgs->width - drawArgs->indent;
	data->rects[0].height = drawArgs->height;
	data->result = 1; /* # of rects */
	return 1; /* stop */
    }

    if (drawArgs->style == NULL) {
	NoStyleMsg(tree, item, TreeColumn_Index(spanPtr->treeColumn));
	data->result = -1; /* error */
	return 1; /* stop */
    }

    if (data->count == -1) {
	/* All the elements. */
	objc = 0;
	objv = NULL;
    } else {
	objc = data->count;
	objv = data->objv;
    }

    data->result = TreeStyle_GetElemRects(drawArgs, objc, objv, data->rects);
    return 1; /* stop */
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetRects --
 *
 *	Returns zero or more bounding boxes for an item-column or
 *	element(s) in an item-column.
 *
 * Results:
 *	If the item is not visible, or has zero height/width, or the given
 *	column is obscurred by a preceding span, or the column has zero
 *	width or is not visible, the return value is zero.
 *
 *	If count==0, the bounding box of the span is returned and the
 *	return value is 1 (for a single rect).
 *
 *	If count!=0, and the item-column does not have a style assigned, an
 *	error is left in the interpreter result and the return value is -1.
 *
 *	If count==-1, the bounding box of each element in the style is
 *	returned and the return value is the number of elements in the style.
 *
 *	If count>0, the bounding box of each element specified by the objv[]
 *	array is returned and the return value is equal to count. If any of
 *	the objv[] elements in not in the style, an error is left in the
 *	interpreter result and the return value is -1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetRects(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    TreeColumn treeColumn,	/* The column to get rects for. */
    int count,			/* -1 means get rects for all elements.
				 * 0 means get bounds of the span.
				 * 1+ means objv[] contains names of elements
				 *  to get rects for. */
    Tcl_Obj *CONST objv[],	/* Array of element names or NULL. */
    TreeRectangle rects[]	/* Out: returned bounding boxes. */
    )
{
    TreeRectangle tr;
    int lock = TreeColumn_Lock(treeColumn);
    struct {
	TreeColumn treeColumn;
	int count;
	Tcl_Obj *CONST *objv;
	TreeRectangle *rects;
	int result;
    } clientData;

    if (Tree_ItemBbox(tree, item, lock, &tr) < 0)
	return 0;

    clientData.treeColumn = treeColumn;
    clientData.count = count;
    clientData.objv = objv;
    clientData.rects = rects;
    clientData.result = 0; /* -1 error, 0 no rects, 1+ success */

    TreeItem_WalkSpans(tree, item, lock,
	    TreeRect_Left(tr), TreeRect_Top(tr),
	    TreeRect_Width(tr), TreeRect_Height(tr),
	    SpanWalkProc_GetRects, (ClientData) &clientData);

    return clientData.result;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Init --
 *
 *	Perform item-related initialization when a new TreeCtrl is
 *	created.
 *
 * Results:
 *	TCL_OK.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_Init(
    TreeCtrl *tree		/* Widget info. */
    )
{
    ItemButtonCO_Init(itemOptionSpecs, "-button", ITEM_FLAG_BUTTON,
	    ITEM_FLAG_BUTTON_AUTO);
    BooleanFlagCO_Init(itemOptionSpecs, "-visible", ITEM_FLAG_VISIBLE);
    BooleanFlagCO_Init(itemOptionSpecs, "-wrap", ITEM_FLAG_WRAP);

    tree->itemOptionTable = Tk_CreateOptionTable(tree->interp, itemOptionSpecs);

    tree->root = Item_AllocRoot(tree);
    tree->activeItem = tree->root; /* always non-null */
    tree->anchorItem = tree->root; /* always non-null */

    return TCL_OK;
}
