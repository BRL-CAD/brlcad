/* 
 * tkTreeDisplay.c --
 *
 *	This module implements treectrl widget's main display code.
 *
 * Copyright (c) 2002-2010 Tim Baker
 *
 * RCS: @(#) $Id$
 */

#include "tkTreeCtrl.h"

/* Window -> Canvas */
#define W2Cx(x) ((x) + tree->xOrigin)
#define W2Cy(y) ((y) + tree->yOrigin)
#define DW2Cx(x) ((x) + dInfo->xOrigin)
#define DW2Cy(y) ((y) + dInfo->yOrigin)

/* Canvas -> Window */
#define C2Wx(x) ((x) - tree->xOrigin)
#define C2Wy(y) ((y) - tree->yOrigin)

#define COMPLEX_WHITESPACE
#define REDRAW_RGN 0
#define CACHE_BG_IMG 1

typedef struct TreeColumnDInfo_ TreeColumnDInfo_;
typedef struct TreeDInfo_ TreeDInfo_;
typedef struct RItem RItem;
typedef struct Range Range;
typedef struct DItem DItem;
typedef struct DItemArea DItemArea;
typedef struct DScrollIncrements DScrollIncrements;

static void CheckPendingHeaderUpdate(TreeCtrl *tree);
static void Range_RedoIfNeeded(TreeCtrl *tree);
static int Range_TotalWidth(TreeCtrl *tree, Range *range_);
static int Range_TotalHeight(TreeCtrl *tree, Range *range_);
static void Range_Redo(TreeCtrl *tree);
static Range *Range_UnderPoint(TreeCtrl *tree, int *x_, int *y_, int nearest);

static Pixmap DisplayGetPixmap(TreeCtrl *tree, TreeDrawable *dPixmap,
    int width, int height);
#if COLUMNGRID == 1
static int GridLinesInWhiteSpace(TreeCtrl *tree);
#endif

/* One of these per TreeItem that is ReallyVisible(). */
struct RItem
{
    TreeItem item;		/* The item. */
    Range *range;		/* Range the item is in. */
    int size;			/* Height or width consumed in Range. */
    int offset;			/* Vertical or horizontal offset in Range. */
    struct {			/* Spacing between adjacent items: */
	int x, y;		/* x is to right of this item. */
    } gap;			/* y is below this item. */
    int index;			/* 0-based index in Range. */
};

/* A collection of visible TreeItems. */
struct Range
{
    RItem *first;
    RItem *last;
    int totalWidth;
    int totalHeight;
    int index;			/* 0-based index in list of Ranges. */
    struct {
	int x, y;		/* vertical/horizontal offset from canvas */
    } offset;			/* top/left. */
    Range *prev;
    Range *next;
};

struct DItemArea
{
    int x;			/* Where it should be drawn, window coords. */
    int width;			/* Current width. */
    int dirty[4];		/* Dirty area in item coords. */
#define DITEM_DIRTY 0x0001
#define DITEM_ALL_DIRTY 0x0002
#define DITEM_DRAWN 0x0004
    int flags;
};

/* Display information for a TreeItem that is onscreen. */
struct DItem
{
#ifdef TREECTRL_DEBUG
    char magic[4];
#endif
    TreeItem item;
    int y;			/* Where it should be drawn, window coords. */
    int height;			/* Current height. */
    DItemArea area;		/* COLUMN_LOCK_NONE. */
    DItemArea left, right;	/* COLUMN_LOCK_LEFT, COLUMN_LOCK_RIGHT. */
#define DITEM_INVALIDATE_ON_SCROLL_X 0x0001
#define DITEM_INVALIDATE_ON_SCROLL_Y 0x0002
    int flags;
    int oldX, oldY;		/* Where it was last drawn, window coords. */
    Range *range;		/* Range the TreeItem is in. */
    int index;			/* Used for alternating background colors. */
    int oldIndex;		/* Used for alternating background colors. */
    int *spans;			/* span[n] is the column index of the item
				 * column displayed at the n'th tree column. */
    DItem *next;		/* Linked list of displayed items. */
};

/* Display information for a TreeColumn. */
struct TreeColumnDInfo_
{
    int offset;			/* Last seen x-offset */
    int width;			/* Last seen column width */
};

struct DScrollIncrements
{
    int *increments;		/* When TreeCtrl.x|yScrollIncrement is zero */
    int count;			/* Size of increments[]. */
};

/* Display information for a TreeCtrl. */
struct TreeDInfo_
{
    GC scrollGC;
    int xOrigin;		/* Last seen TreeCtrl.xOrigin */
    int yOrigin;		/* Last seen TreeCtrl.yOrigin */
    int totalWidth;		/* Last seen Tree_CanvasWidth() */
    int totalHeight;		/* Last seen Tree_CanvasHeight() */
    int fakeCanvasWidth;	/* totalWidth plus any "fake" width resulting
				 * from the view being locked to the edge of
				 * a scroll increment. */
    int fakeCanvasHeight;	/* totalHeight plus any "fake" height resulting
				 * from the view being locked to the edge of
				 * a scroll increment.*/
    int headerHeight;		/* Last seen TreeCtrl.headerHeight */
    DItem *dItem;		/* Head of list for each displayed item */
    DItem *dItemLast;		/* Temp for UpdateDInfo() */
    DItem *dItemFree;		/* List of unused DItems */
    Range *rangeFirst;		/* Head of Ranges */
    Range *rangeLast;		/* Tail of Ranges */
    Range *rangeFirstD;		/* First range with valid display info */
    Range *rangeLastD; 		/* Last range with valid display info */
    RItem *rItem;		/* Block of RItems for all Ranges */
    int rItemMax;		/* size of rItem[] */
    int itemHeight;		/* Observed max TreeItem height */
    int itemWidth;		/* Observed max TreeItem width */
    TreeDrawable pixmapW;	/* Pixmap as big as the window */
    TreeDrawable pixmapI;	/* Pixmap as big as the largest item */
    TreeDrawable pixmapT;	/* Pixmap as big as the window.
				   Use on non-composited desktops when
				   displaying non-XOR dragimage, marquee
				   and/or proxies. */
    TkRegion dirtyRgn;		/* DOUBLEBUFFER_WINDOW */
    int flags;			/* DINFO_XXX */
    DScrollIncrements xScrollIncrements;
    DScrollIncrements yScrollIncrements;
    TkRegion wsRgn;		/* Region containing whitespace */
#ifdef COMPLEX_WHITESPACE
    int complexWhitespace;
#endif
    Tcl_HashTable itemVisHash;	/* Table of visible items */
    int requests;		/* Incremented for every call to
				   Tree_EventuallyRedraw */
    TreeRectangle bounds;	/* Bounds of TREE_AREA_CONTENT. */
    TreeRectangle boundsL;	/* Bounds of TREE_AREA_LEFT. */
    TreeRectangle boundsR;	/* Bounds of TREE_AREA_RIGHT. */
    int empty, emptyL, emptyR;	/* Whether bounds above are zero-sized.*/
    int widthOfColumnsLeft;	/* Last seen Tree_WidthOfLeftColumns() */
    int widthOfColumnsRight;	/* Last seen Tree_WidthOfRightColumns() */
    Range *rangeLock;		/* If there is no Range for non-locked
				 * columns, this range holds the vertical
				 * offset and height of each ReallyVisible
				 * item for displaying locked columns. */
#if REDRAW_RGN == 1
    TkRegion redrawRgn;		/* Contains all redrawn (not copied) pixels
				 * during a single Tree_Display call. */
#endif /* REDRAW_RGN */
    int overlays;		/* TRUE if the dragimage|marquee|proxy
				 * were drawn in non-XOR mode. */
#if CACHE_BG_IMG
    TreeDrawable pixmapBgImg;	/* Pixmap containing the -backgroundimage
				 * for efficient blitting.  Not used if the
				 * -backgroundimage is transparent. */
#endif
};

#ifdef COMPLEX_WHITESPACE
static int ComplexWhitespace(TreeCtrl *tree);
#endif

static int CalcBgImageBounds(TreeCtrl *tree, TreeRectangle *trImage);

/*========*/

void
Tree_FreeItemRInfo(
    TreeCtrl *tree,
    TreeItem item
    )
{
    TreeItem_SetRInfo(tree, item, NULL);
}

static Range *
Range_Free(
    TreeCtrl *tree,
    Range *range
    )
{
    Range *next = range->next;
    WFREE(range, Range);
    return next;
}

/*
 *----------------------------------------------------------------------
 *
 * ItemWidthParams --
 *
 *	Calculates the fixed or incremental width of all items.
 *	This is called when -orient=horizontal.
 *
 * Results:
 *	Returns the fixed width all items should be, or -1.
 *	Returns the width all item widths should be a multiple of, or -1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
ItemWidthParams(
    TreeCtrl *tree,		/* Widget info. */
    int *fixedWidthPtr,		/* Returned fixed width or -1. */
    int *stepWidthPtr		/* Returned incremental width or -1. */
    )
{
    int fixedWidth = -1, stepWidth = -1;

    /* More than one item column, so all items have the same width */
    if (tree->columnCountVis > 1)
	fixedWidth = Tree_WidthOfColumns(tree);

    /* Single item column, fixed width for all items */
    else if (tree->itemWidth > 0)
	fixedWidth = tree->itemWidth;

    /* Single item column, fixed width for all items */
    /* THIS IS FOR COMPATIBILITY ONLY */
    else if (TreeColumn_FixedWidth(tree->columnVis) != -1)
	fixedWidth = TreeColumn_FixedWidth(tree->columnVis);

    /* Single item column, want all items same width */
    else if (tree->itemWidthEqual
#ifdef DEPRECATED
	    || TreeColumn_WidthHack(tree->columnVis)
#endif /* DEPRECATED */
	) {
	fixedWidth = TreeColumn_WidthOfItems(tree->columnVis);

	/* Each item is a multiple of this width */
	if (tree->itemWidMult > 0)
	    stepWidth = tree->itemWidMult;
#ifdef DEPRECATED
	else
	    stepWidth = TreeColumn_StepWidth(tree->columnVis);
#endif /* DEPRECATED */

	if ((stepWidth != -1) && (fixedWidth % stepWidth))
	    fixedWidth += stepWidth - fixedWidth % stepWidth;
    }

    /* Single item column, variable item width */
    else {
	/* Each item is a multiple of this width */
	if (tree->itemWidMult > 0)
	    stepWidth = tree->itemWidMult;
#ifdef DEPRECATED
	else
	    stepWidth = TreeColumn_StepWidth(tree->columnVis);
#endif /* DEPRECATED */
    }

    (*fixedWidthPtr) = fixedWidth;
    (*stepWidthPtr) = stepWidth;
}

/*
 *----------------------------------------------------------------------
 *
 * Range_Redo --
 *
 *	This procedure puts all ReallyVisible() TreeItems into a list of
 *	Ranges. If tree->wrapMode is TREE_WRAP_NONE and no visible items
 *	have the -wrap=true option there will only be a single Range.
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
Range_Redo(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    Range *rangeList = dInfo->rangeFirst;
    Range *range;
    RItem *rItem;
    TreeItem item = tree->root;
    int fixedWidth = -1, stepWidth = -1;
    int wrapCount = 0, wrapPixels = 0;
    int count, pixels, rItemCount = 0;
    int rangeIndex = 0, rItemIndex;
    int *canvasPad;

    if (tree->debug.enable && tree->debug.display)
	dbwin("Range_Redo %s\n", Tk_PathName(tree->tkwin));

    /* Update column variables */
    (void) Tree_WidthOfColumns(tree);

    dInfo->rangeFirst = NULL;
    dInfo->rangeLast = NULL;

    if (tree->columnCountVis < 1)
	goto freeRanges;

    switch (tree->wrapMode) {
	case TREE_WRAP_NONE:
	    break;
	case TREE_WRAP_ITEMS:
	    wrapCount = tree->wrapArg;
	    break;
	case TREE_WRAP_PIXELS:
	    wrapPixels = tree->wrapArg;
	    break;
	case TREE_WRAP_WINDOW:
	    wrapPixels = tree->vertical ?
		Tree_ContentHeight(tree) :
		Tree_ContentWidth(tree);
	    if (wrapPixels < 0)
		wrapPixels = 0;
	    break;
    }

    /* For horizontal layout with wrapping I need to know how wide each item
     * is. */
    if ((wrapPixels > 0) && !tree->vertical) {
	ItemWidthParams(tree, &fixedWidth, &stepWidth);
    }

    /* Speed up ReallyVisible() and get itemVisCount */
    Tree_UpdateItemIndex(tree);

    if (dInfo->rItemMax < tree->itemVisCount) {
	dInfo->rItem = (RItem *) ckrealloc((char *) dInfo->rItem,
		tree->itemVisCount * sizeof(RItem));
	dInfo->rItemMax = tree->itemVisCount;
    }

    if (!TreeItem_ReallyVisible(tree, item))
	item = TreeItem_NextVisible(tree, item);
    while (item != NULL) {
	if (rangeList == NULL)
	    range = (Range *) ckalloc(sizeof(Range));
	else {
	    range = rangeList;
	    rangeList = rangeList->next;
	}
	memset(range, '\0', sizeof(Range));
	range->totalWidth = -1;
	range->totalHeight = -1;
	range->index = rangeIndex++;
	count = 0;
	canvasPad = tree->vertical ? tree->canvasPadY : tree->canvasPadX;
	pixels = 0;
	rItemIndex = 0;
	while (1) {
	    rItem = dInfo->rItem + rItemCount;
	    if (rItemCount >= dInfo->rItemMax)
		panic("rItemCount > dInfo->rItemMax");
	    if (range->first == NULL)
		range->first = range->last = rItem;
	    TreeItem_SetRInfo(tree, item, (TreeItemRInfo) rItem);
	    rItem->item = item;
	    rItem->range = range;
	    rItem->index = rItemIndex;
	    rItem->gap.x = rItem->gap.y = 0;

	    /* Range must be <= this number of pixels */
	    if (wrapPixels > 0) {
		rItem->offset = pixels;
		if (tree->vertical) {
		    rItem->size = TreeItem_Height(tree, item);
		} else {
		    if (fixedWidth != -1) {
			rItem->size = fixedWidth;
		    } else {
			TreeItemColumn itemColumn =
			    TreeItem_FindColumn(tree, item,
				    TreeColumn_Index(tree->columnVis));
			if (itemColumn != NULL) {
			    int columnWidth =
				TreeItemColumn_NeededWidth(tree, item,
					itemColumn);
			    if (tree->columnTreeVis)
				columnWidth += TreeItem_Indent(tree, item);
			    rItem->size = columnWidth;
			} else
			    rItem->size = 0;
			if ((stepWidth != -1) && (rItem->size % stepWidth))
			    rItem->size += stepWidth - rItem->size % stepWidth;
		    }
		}
		/* Check if this item's bounds exceeds the -wrap distance. */
		if (canvasPad[PAD_TOP_LEFT]
		    + pixels + rItem->size
		    + canvasPad[PAD_BOTTOM_RIGHT] > wrapPixels) {

		    /* Remove the gap from the previous item in this range. */
		    if (rItem->index > 0) {
			if (tree->vertical) {
			    pixels -= (rItem-1)->gap.y;
			    (rItem-1)->gap.y = 0;
			} else {
			    pixels -= (rItem-1)->gap.x;
			    (rItem-1)->gap.x = 0;
			}
		    }

		    /* Ensure at least one item is in this Range */
		    if (rItemIndex == 0) {

			if (tree->vertical) {
			    rItem->gap.y = tree->itemGapY;
			    pixels += rItem->gap.y;
			} else {
			    rItem->gap.x = tree->itemGapX;
			    pixels += rItem->gap.x;
			}

			pixels += rItem->size;
			rItemCount++;
		    }
		    break;
		}

		if (tree->vertical) {
		    rItem->gap.y = tree->itemGapY;
		    pixels += rItem->gap.y;
		} else {
		    rItem->gap.x = tree->itemGapX;
		    pixels += rItem->gap.x;
		}

		pixels += rItem->size;
	    }
	    range->last = rItem;
	    rItemIndex++;
	    rItemCount++;
	    if (++count == wrapCount)
		break;
	    item = TreeItem_NextVisible(tree, item);
	    if (item == NULL)
		break;
	    if (TreeItem_GetWrap(tree, item))
		break;
	}
	/* If we needed to calculate the height or width of this range,
	 * we don't need to do it later in Range_TotalWidth/Height() */
	if (wrapPixels > 0) {
	    if (tree->vertical) {
		/* Remove the gap from the last item in this range. */
		if (range->last->gap.y > 0) {
		    pixels -= range->last->gap.y;
		    range->last->gap.y = 0;
		}
		range->totalHeight = pixels;
	    } else {
		/* Remove the gap from the last item in this range. */
		if (range->last->gap.x > 0) {
		    pixels -= range->last->gap.x;
		    range->last->gap.x = 0;
		}
		range->totalWidth = pixels;
	    }
	}

	if (dInfo->rangeFirst == NULL)
	    dInfo->rangeFirst = range;
	else {
	    range->prev = dInfo->rangeLast;
	    dInfo->rangeLast->next = range;
	}
	dInfo->rangeLast = range;
	item = TreeItem_NextVisible(tree, range->last->item);
    }

freeRanges:
    while (rangeList != NULL)
	rangeList = Range_Free(tree, rangeList);

    /* If there are no visible non-locked columns, we won't have a Range.
     * But we need to know the offset/size of each item for drawing any
     * locked columns (and for vertical scrolling... and hit testing). */
    if (dInfo->rangeLock != NULL) {
	(void) Range_Free(tree, dInfo->rangeLock);
	dInfo->rangeLock = NULL;
    }
    (void) Tree_WidthOfColumns(tree); /* update columnCountVisLeft etc */
    if ((dInfo->rangeFirst == NULL) &&
	    (tree->columnCountVisLeft ||
	    tree->columnCountVisRight)) {

	/* Speed up ReallyVisible() and get itemVisCount */
	Tree_UpdateItemIndex(tree);

	if (tree->itemVisCount == 0)
	    return;

	if (dInfo->rItemMax < tree->itemVisCount) {
	    dInfo->rItem = (RItem *) ckrealloc((char *) dInfo->rItem,
		    tree->itemVisCount * sizeof(RItem));
	    dInfo->rItemMax = tree->itemVisCount;
	}

	dInfo->rangeLock = (Range *) ckalloc(sizeof(Range));
	range = dInfo->rangeLock;

	pixels = 0;
	rItemIndex = 0;
	rItem = dInfo->rItem;
	item = tree->root;
	if (!TreeItem_ReallyVisible(tree, item))
	    item = TreeItem_NextVisible(tree, item);
	while (item != NULL) {
	    rItem->item = item;
	    rItem->range = range;
	    rItem->size = TreeItem_Height(tree, item);
	    rItem->offset = pixels;
	    rItem->gap.x = 0;
	    if (TreeItem_NextVisible(tree, item) != NULL) {
		rItem->gap.y = tree->itemGapY;
	    } else {
		rItem->gap.y = 0;
	    }
	    pixels += rItem->gap.y;
	    rItem->index = rItemIndex++;
	    TreeItem_SetRInfo(tree, item, (TreeItemRInfo) rItem);
	    pixels += rItem->size;
	    rItem++;
	    item = TreeItem_NextVisible(tree, item);
	}

	range->offset.x = 0;
	range->offset.y = 0;
	range->first = dInfo->rItem;
	range->last = dInfo->rItem + tree->itemVisCount - 1;
	range->totalWidth = 1;
	range->totalHeight = pixels;
	range->prev = range->next = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Range_TotalWidth --
 *
 *	Return the width of a Range. The width is only calculated if
 *	it hasn't been done yet by Range_Redo().
 *
 * Results:
 *	Pixel width of the Range.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Range_TotalWidth(
    TreeCtrl *tree,		/* Widget info. */
    Range *range		/* Range to return the width of. */
    )
{
    TreeItem item;
    TreeItemColumn itemColumn;
    RItem *rItem;
    int fixedWidth = -1, stepWidth = -1;
    int itemWidth;

    if (range->totalWidth >= 0)
	return range->totalWidth;

    if (tree->vertical) {

	/* More than one item column, so all ranges have the same width */
	if (tree->columnCountVis > 1)
	    return range->totalWidth = Tree_WidthOfColumns(tree);

	/* If wrapping is disabled, then use the column width,
	 * since it may expand to fill the window */
	if ((tree->wrapMode == TREE_WRAP_NONE) && (tree->itemWrapCount <= 0))
	    return range->totalWidth = TreeColumn_UseWidth(tree->columnVis);

	/* Single item column, fixed width for all ranges */
	if (tree->itemWidth > 0)
	    return range->totalWidth = tree->itemWidth;

	/* Single item column, fixed width for all ranges */
	/* THIS IS FOR COMPATIBILITY ONLY */
	if (TreeColumn_FixedWidth(tree->columnVis) != -1)
	    return range->totalWidth = TreeColumn_FixedWidth(tree->columnVis);

	/* Single item column, each item is a multiple of this width */
	if (tree->itemWidMult > 0)
	    stepWidth = tree->itemWidMult;
#ifdef DEPRECATED
	else
	    stepWidth = TreeColumn_StepWidth(tree->columnVis);
#endif /* DEPRECATED */

	/* Single item column, want all items same width */
	if (tree->itemWidthEqual
#ifdef DEPRECATED
		|| TreeColumn_WidthHack(tree->columnVis)
#endif /* DEPRECATED */
	    ) {
	    range->totalWidth = TreeColumn_WidthOfItems(tree->columnVis);
	    if ((stepWidth != -1) && (range->totalWidth % stepWidth))
		range->totalWidth += stepWidth - range->totalWidth % stepWidth;
	    return range->totalWidth;
	}

	/* Max needed width of items in this range */
	range->totalWidth = 0;
	rItem = range->first;
	while (1) {
	    item = rItem->item;
	    itemColumn = TreeItem_FindColumn(tree, item,
		    TreeColumn_Index(tree->columnVis));
	    if (itemColumn != NULL)
		itemWidth = TreeItemColumn_NeededWidth(tree, item,
			itemColumn);
	    else
		itemWidth = 0;
	    if (tree->columnTreeVis)
		itemWidth += TreeItem_Indent(tree, item);
	    if (itemWidth > range->totalWidth)
		range->totalWidth = itemWidth;
	    if (rItem == range->last)
		break;
	    rItem++;
	}
	if ((stepWidth != -1) && (range->totalWidth % stepWidth))
	    range->totalWidth += stepWidth - range->totalWidth % stepWidth;
	return range->totalWidth;

    /* -orient=horizontal */
    } else {
	ItemWidthParams(tree, &fixedWidth, &stepWidth);

	/* Sum of widths of items in this range */
	range->totalWidth = 0;
	rItem = range->first;
	while (1) {
	    item = rItem->item;
	    if (fixedWidth != -1)
		itemWidth = fixedWidth;
	    else {
		itemColumn = TreeItem_FindColumn(tree, item,
			TreeColumn_Index(tree->columnVis));
		if (itemColumn != NULL)
		    itemWidth = TreeItemColumn_NeededWidth(tree, item,
			    itemColumn);
		else
		    itemWidth = 0;

		if (tree->columnTreeVis)
		    itemWidth += TreeItem_Indent(tree, item);

		if ((stepWidth != -1) && (itemWidth % stepWidth))
		    itemWidth += stepWidth - itemWidth % stepWidth;
	    }

	    rItem = (RItem *) TreeItem_GetRInfo(tree, item);
	    rItem->offset = range->totalWidth;
	    rItem->size = itemWidth;
	    if (rItem != range->last) {
		rItem->gap.x = tree->itemGapX;
	    } else {
		rItem->gap.x = 0;
	    }
	    range->totalWidth += rItem->gap.x;
	    range->totalWidth += rItem->size;
	    if (rItem == range->last)
		break;
	    rItem++;
	}
	return range->totalWidth;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Range_TotalHeight --
 *
 *	Return the height of a Range. The height is only calculated if
 *	it hasn't been done yet by Range_Redo().
 *
 * Results:
 *	Pixel height of the Range.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Range_TotalHeight(
    TreeCtrl *tree,		/* Widget info. */
    Range *range		/* Range to return the height of. */
    )
{
    TreeItem item;
    RItem *rItem;
    int itemHeight;

    if (range->totalHeight >= 0)
	return range->totalHeight;

    range->totalHeight = 0;
    rItem = range->first;
    while (1) {
	item = rItem->item;
	itemHeight = TreeItem_Height(tree, item);
	if (tree->vertical) {
	    rItem->offset = range->totalHeight;
	    rItem->size = itemHeight;
	    if (rItem != range->last) {
		rItem->gap.y = tree->itemGapY;
	    } else {
		rItem->gap.y = 0;
	    }
	    range->totalHeight += rItem->gap.y;
	    range->totalHeight += rItem->size;
	}
	else {
	    if (itemHeight > range->totalHeight)
		range->totalHeight = itemHeight;
	}
	if (rItem == range->last)
	    break;
	rItem++;
    }
    return range->totalHeight;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_CanvasWidth --
 *
 *	Return the width needed by all the Ranges. The width is only
 *	recalculated if it was marked out-of-date.
 *
 * Results:
 *	Pixel width of the "canvas".
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *----------------------------------------------------------------------
 */

int
Tree_CanvasWidth(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    Range *range;
    int rangeWidth;

    Range_RedoIfNeeded(tree);

    if (tree->totalWidth >= 0)
	return tree->totalWidth;

    if (dInfo->rangeFirst == NULL)
	return tree->totalWidth =
		tree->canvasPadX[PAD_TOP_LEFT]
		+ Tree_WidthOfColumns(tree)
		+ tree->canvasPadX[PAD_BOTTOM_RIGHT];

    tree->totalWidth = tree->canvasPadX[PAD_TOP_LEFT];
    range = dInfo->rangeFirst;
    while (range != NULL) {
	rangeWidth = Range_TotalWidth(tree, range);
	if (tree->vertical) {
	    range->offset.x = tree->totalWidth;
	    tree->totalWidth += rangeWidth;
	    if (range->next != NULL)
		tree->totalWidth += tree->itemGapX;
	}
	else {
	    range->offset.x = tree->canvasPadX[PAD_TOP_LEFT];
	    if (range->offset.x + rangeWidth > tree->totalWidth)
		tree->totalWidth = range->offset.x + rangeWidth;
	}
	range = range->next;
    }
    tree->totalWidth += tree->canvasPadX[PAD_BOTTOM_RIGHT];

    return tree->totalWidth;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_CanvasHeight --
 *
 *	Return the height needed by all the Ranges. The height is only
 *	recalculated if it was marked out-of-date.
 *
 * Results:
 *	Pixel height of the "canvas".
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *----------------------------------------------------------------------
 */

int
Tree_CanvasHeight(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    Range *range;
    int rangeHeight;

    Range_RedoIfNeeded(tree);

    if (tree->totalHeight >= 0)
	return tree->totalHeight;

    tree->totalHeight = tree->canvasPadY[PAD_TOP_LEFT];
    range = dInfo->rangeFirst;
    while (range != NULL) {
	rangeHeight = Range_TotalHeight(tree, range);
	if (tree->vertical) {
	    range->offset.y = tree->canvasPadY[PAD_TOP_LEFT];
	    if (range->offset.y + rangeHeight > tree->totalHeight)
		tree->totalHeight = range->offset.y + rangeHeight;
	}
	else {
	    range->offset.y = tree->totalHeight;
	    tree->totalHeight += rangeHeight;
	    if (range->next != NULL)
		tree->totalHeight += tree->itemGapY;
	}
	range = range->next;
    }
    tree->totalHeight += tree->canvasPadY[PAD_BOTTOM_RIGHT];

    /* If dInfo->rangeLock is not NULL, then we are displaying some items
     * in locked columns but no non-locked columns. */
    if (dInfo->rangeLock != NULL) {
	if (dInfo->rangeLock->totalHeight > tree->totalHeight)
	    tree->totalHeight = dInfo->rangeLock->totalHeight;
    }

    return tree->totalHeight;
}

/*
 *----------------------------------------------------------------------
 *
 * Range_UnderPoint --
 *
 *	Return the Range containing the given coordinates.
 *
 * Results:
 *	Range containing the coordinates or NULL if the point is
 *	outside any Range.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *----------------------------------------------------------------------
 */

static Range *
Range_UnderPoint(
    TreeCtrl *tree,		/* Widget info. */
    int *x_,			/* In: window x coordinate.
				 * Out: x coordinate relative to the top-left
				 * of the Range. */
    int *y_,			/* In: window y coordinate.
				 * Out: y coordinate relative to the top-left
				 * of the Range. */
    int nearest			/* TRUE if the Range nearest the coordinates
				 * should be returned. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    Range *range;
    int x = *x_, y = *y_;

    Range_RedoIfNeeded(tree);

    if (Tree_CanvasWidth(tree) - tree->canvasPadX[PAD_TOP_LEFT]
	    - tree->canvasPadX[PAD_BOTTOM_RIGHT] <= 0)
	return NULL;
    if (Tree_CanvasHeight(tree) - tree->canvasPadY[PAD_TOP_LEFT]
	    - tree->canvasPadY[PAD_BOTTOM_RIGHT] <= 0)
	return NULL;

    range = dInfo->rangeFirst;

    if (nearest) {

	TreeRectangle tr;

	/* Keep inside borders and header. Perhaps another flag needed. */
	if (!Tree_AreaBbox(tree, TREE_AREA_CONTENT, &tr))
	    return NULL;
	TreeRect_ClipPoint(tr, &x, &y);

	x = W2Cx(x);
	y = W2Cy(y);

	if (tree->vertical) {
	    while (range != NULL) {
		if (x < range->offset.x)
		    x = range->offset.x;
		if (y < range->offset.y)
		    y = range->offset.y;
		if (x < range->offset.x + range->totalWidth || range->next == NULL) {
		    (*x_) = MIN(x - range->offset.x, range->totalWidth - 1);
		    (*y_) = MIN(y - range->offset.y, range->totalHeight - 1);
		    return range;
		}
		if (x - (range->offset.x + range->totalWidth) <
		    range->next->offset.x - x) {
		    (*x_) = MIN(x - range->offset.x, range->totalWidth - 1);
		    (*y_) = MIN(y - range->offset.y, range->totalHeight - 1);
		    return range;
		}
		range = range->next;
	    }
	} else {
	    while (range != NULL) {
		if (x < range->offset.x)
		    x = range->offset.x;
		if (y < range->offset.y)
		    y = range->offset.y;
		if (y < range->offset.y + range->totalHeight || range->next == NULL) {
		    (*x_) = MIN(x - range->offset.x, range->totalWidth - 1);
		    (*y_) = MIN(y - range->offset.y, range->totalHeight - 1);
		    return range;
		}
		if (y - (range->offset.y + range->totalHeight) <
		    range->next->offset.y - y) {
		    (*x_) = MIN(x - range->offset.x, range->totalWidth - 1);
		    (*y_) = MIN(y - range->offset.y, range->totalHeight - 1);
		    return range;
		}
		range = range->next;
	    }
	}
	return NULL;
    }

    x = W2Cx(x);
    y = W2Cy(y);

    while (range != NULL) {
	if (tree->vertical && x < range->offset.x)
	    return NULL;
	if (!tree->vertical && y < range->offset.y)
	    return NULL;
	if ((x >= range->offset.x) &&
	    (x < range->offset.x + range->totalWidth) &&
	    (y >= range->offset.y) &&
	    (y < range->offset.y + range->totalHeight)) {

	    (*x_) = x - range->offset.x;
	    (*y_) = y - range->offset.y;
	    return range;
	}
	range = range->next;
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Range_ItemUnderPoint --
 *
 *	Return the RItem containing the given x and/or y coordinates.
 *
 * Results:
 *	RItem containing the coordinates, or the RItem nearest the
 *	coordinates, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static RItem *
Range_ItemUnderPoint(
    TreeCtrl *tree,		/* Widget info. */
    Range *range,		/* Range to search. */
    int rcX,			/* x coordinate relative to top-left of
				 * the Range. */
    int rcY,			/* y coordinate relative to top-left of
				 * the Range. */
    int *icX,			/* x coordinate relative to top-left of
				 * the returned RItem. May be NULL. */
    int *icY,			/* y coordinate relative to top-left of
				 * the returned RItem. May be NULL. */
    int nearest			/* '0' means return NULL if point is outside
				 * every item.
				 * '1' means return the nearest item.
				 * '2' means return the nearest item.  If the
				 * point is in whitespace before/between
				 * items, the item before the gap is returned.
				 * '3' means return the nearest item.  If the
				 * point is in whitespace before/between
				 * items, the item after the gap is returned.
				 */
    )
{
    RItem *rItem;
    int i, l, u;
    int rcOffset = tree->vertical ? rcY : rcX;

    if (nearest == 0) {
	if (!tree->vertical && (rcX < 0 || rcX >= range->totalWidth))
	    return NULL;
	if (tree->vertical && (rcY < 0 || rcY >= range->totalHeight))
	    return NULL;

	/* Binary search */
	l = 0;
	u = range->last->index;
	while (l <= u) {
	    i = (l + u) / 2;
	    rItem = range->first + i;
	    if ((rcOffset >= rItem->offset) && (rcOffset < rItem->offset + rItem->size)) {
foundItem:
		/* Range -> item coords */
		if (tree->vertical) {
		    if (icX != NULL) (*icX) = rcX;
		    if (icY != NULL) (*icY) = rcY - rItem->offset;
		} else {
		    if (icX != NULL) (*icX) = rcX - rItem->offset;
		    if (icY != NULL) (*icY) = rcY;
		}
		return rItem;
	    }
	    if ((rcOffset < rItem->offset) && ((rItem == range->first)
		    || (rcOffset >= (rItem-1)->offset + (rItem-1)->size))) {
		/* Between previous item and this one. */
		return NULL;
	    }
	    if ((rcOffset >= rItem->offset + rItem->size) &&
		    ((rItem == range->last)
		    || (rcOffset < (rItem+1)->offset))) {
		/* Between next item and this one. */
		return NULL;
	    }
	    if (rcOffset < rItem->offset)
		u = i - 1;
	    else
		l = i + 1;
	}

	return NULL;
    }

    /* Binary search */
    l = 0;
    u = range->last->index;
    while (l <= u) {
	i = (l + u) / 2;
	rItem = range->first + i;
	if ((rcOffset >= rItem->offset) && (rcOffset < rItem->offset + rItem->size)) {
	    goto foundItem;
	}
	if ((rcOffset < rItem->offset) && ((rItem == range->first)
		|| (rcOffset >= (rItem-1)->offset + (rItem-1)->size))) {
	    /* Between previous item and this one. */
	    if (rItem != range->first) {
		RItem *prev = rItem - 1;
		if (nearest == 1) {
		    int edgeMin = prev->offset + prev->size;
		    int edgeMax = rItem->offset;
		    if (rcOffset < edgeMin + (edgeMax - edgeMin) / 2.0f)
			rItem = prev;
		} else if (nearest == 2)
		    rItem = prev;
	    }
	    goto foundItem;
	}
	if ((rcOffset >= rItem->offset + rItem->size) &&
		((rItem == range->last)
		|| (rcOffset < (rItem+1)->offset))) {
	    /* Between next item and this one. */
	    if (rItem != range->last) {
		RItem *next = rItem + 1;
		if (nearest == 1) {
		    int edgeMin = rItem->offset + rItem->size;
		    int edgeMax = next->offset;
		    if (rcOffset >= edgeMin + (edgeMax - edgeMin) / 2.0f)
			rItem = next;
		} else if (nearest == 3)
		    rItem = next;
	    }
	    goto foundItem;
	}
	if (rcOffset < rItem->offset)
	    u = i - 1;
	else
	    l = i + 1;
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Increment_AddX --
 *
 *	Appends one or more values to the list of horizontal scroll
 *	increments.
 *
 * Results:
 *	New size of DInfo.xScrollIncrements.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static int
Increment_AddX(
    TreeCtrl *tree,		/* Widget info. */
    int offset,			/* Offset to add. */
    int size			/* Current size of DInfo.xScrollIncrements. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DScrollIncrements *dIncr = &dInfo->xScrollIncrements;
    int visWidth = Tree_ContentWidth(tree);

    while ((visWidth > 1) && (dIncr->count > 0) &&
	    (offset - dIncr->increments[dIncr->count - 1] > visWidth)) {
	size = Increment_AddX(tree,
		dIncr->increments[dIncr->count - 1] + visWidth,
		size);
    }
#ifdef TREECTRL_DEBUG
    if ((dIncr->count > 0) &&
	    (offset == dIncr->increments[dIncr->count - 1])) {
	panic("Increment_AddX: offset %d same as previous offset", offset);
    }
#endif
    if (dIncr->count + 1 > size) {
	size *= 2;
	dIncr->increments = (int *) ckrealloc(
	    (char *) dIncr->increments, size * sizeof(int));
    }
    dIncr->increments[dIncr->count++] = offset;
    return size;
}

/*
 *----------------------------------------------------------------------
 *
 * Increment_AddY --
 *
 *	Appends one or more values to the list of vertical scroll
 *	increments.
 *
 * Results:
 *	New size of DInfo.yScrollIncrements.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static int
Increment_AddY(
    TreeCtrl *tree,		/* Widget info. */
    int offset,			/* Offset to add. */
    int size			/* Current size of DInfo.yScrollIncrements. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DScrollIncrements *dIncr = &dInfo->yScrollIncrements;
    int visHeight = Tree_ContentHeight(tree);

    while ((visHeight > 1) && (dIncr->count > 0) &&
	    (offset - dIncr->increments[dIncr->count - 1] > visHeight)) {
	size = Increment_AddY(tree,
		dIncr->increments[dIncr->count - 1] + visHeight,
		size);
    }
#ifdef TREECTRL_DEBUG
    if ((dIncr->count > 0) &&
	    (offset == dIncr->increments[dIncr->count - 1])) {
	panic("Increment_AddY: offset %d same as previous offset", offset);
    }
#endif
    if (dIncr->count + 1 > size) {
	size *= 2;
	dIncr->increments = (int *) ckrealloc(
	    (char *) dIncr->increments, size * sizeof(int));
    }
    dIncr->increments[dIncr->count++] = offset;
    return size;
}

/*
 *----------------------------------------------------------------------
 *
 * RItemsToIncrementsX --
 *
 *	Recalculate the list of horizontal scroll increments. This gets
 *	called when -orient=horizontal and -xscrollincrement=0.
 *
 * Results:
 *	DInfo.xScrollIncrements is updated if the canvas width is > 0.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
RItemsToIncrementsX(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DScrollIncrements *dIncr = &dInfo->xScrollIncrements;
    Range *range, *rangeFirst = dInfo->rangeFirst;
    RItem *rItem;
    int visWidth = Tree_ContentWidth(tree);
    int totalWidth = Tree_CanvasWidth(tree);
    int x1, increment, prev;
    int size;

    if (totalWidth <= 0)
	return;

    /* First increment is zero */
    size = 10;
    dIncr->increments = (int *) ckalloc(size * sizeof(int));
    dIncr->increments[dIncr->count++] = 0;
    prev = 0;

    if (rangeFirst == NULL) {
	/* Only the column headers are shown. */
    } else if (rangeFirst->next == NULL) {
	/* A single horizontal range is easy. Add one increment for the
	 * left edge of each item. */
	rItem = rangeFirst->first;

	while (1) {
	    increment = rangeFirst->offset.x + rItem->offset;
	    if (increment > prev) {
		size = Increment_AddX(tree, increment, size);
		prev = increment;
	    }
	    if (rItem == rangeFirst->last)
		break;
	    rItem++;
	}
    } else {
	x1 = 0;
	while (1) {
	    int minLeft1 = totalWidth, minLeft2 = totalWidth;
	    increment = totalWidth;
	    /* Find the RItem whose left edge is >= x1 by the smallest
	    * amount. */
	    for (range = rangeFirst; range != NULL; range = range->next) {
		if (x1 >= range->offset.x + range->totalWidth)
		    continue;

		rItem = Range_ItemUnderPoint(tree, range,
		    x1 - range->offset.x, -666, NULL, NULL, 3);
		if (range->offset.x + rItem->offset >= x1)
		    increment = MIN(increment, range->offset.x + rItem->offset);
		if (range->offset.x + rItem->offset > x1)
		    minLeft1 = MIN(minLeft1, range->offset.x + rItem->offset);
		if (rItem != range->last) {
		    rItem += 1;
		    if (range->offset.x + rItem->offset > x1)
			minLeft2 = MIN(minLeft2, range->offset.x + rItem->offset);
		}
	    }
	    if (increment == totalWidth)
		break;
	    if (increment > prev) {
		size = Increment_AddX(tree, increment, size);
		prev = increment;
	    }
	    if (minLeft1 == totalWidth && minLeft2 == totalWidth)
		break; /* no increments > x1 */
	    if (minLeft1 != totalWidth && minLeft1 > increment)
		x1 = minLeft1;
	    else
		x1 = minLeft2;
	}
    }
    if ((visWidth > 1) && (totalWidth -
		dIncr->increments[dIncr->count - 1] > visWidth)) {
	Increment_AddX(tree, totalWidth, size);
	dIncr->count--;
	dIncr->increments[dIncr->count - 1] = totalWidth - visWidth;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RItemsToIncrementsY --
 *
 *	Recalculate the list of vertical scroll increments. This gets
 *	called when -orient=vertical and -yscrollincrement=0.
 *
 * Results:
 *	DInfo.yScrollIncrements is updated if the canvas height is > 0.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
RItemsToIncrementsY(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DScrollIncrements *dIncr = &dInfo->yScrollIncrements;
    Range *range, *rangeFirst;
    RItem *rItem;
    int visHeight = Tree_ContentHeight(tree);
    int totalHeight = Tree_CanvasHeight(tree);
    int y1, increment, prev;
    int size;

    if (totalHeight <= 0)
	return;

    /* First increment is zero */
    size = 10;
    dIncr->increments = (int *) ckalloc(size * sizeof(int));
    dIncr->increments[dIncr->count++] = 0;
    prev = 0;

    /* If only locked columns are visible, we still scroll vertically. */
    rangeFirst = dInfo->rangeFirst;
    if (rangeFirst == NULL)
	rangeFirst = dInfo->rangeLock;

    if (rangeFirst == NULL) {
	/* Only -canvaspady spacing, no items! */
    } else if (rangeFirst->next == NULL) {
	/* A single vertical range is easy. Add one increment for the
	 * top edge of each item. */
	rItem = rangeFirst->first;

	while (1) {
	    increment = rangeFirst->offset.y + rItem->offset;
	    if (increment > prev) {
		size = Increment_AddY(tree, increment, size);
		prev = increment;
	    }
	    if (rItem == rangeFirst->last)
		break;
	    rItem++;
	}
    } else {
	y1 = 0;
	while (1) {
	    int minTop1 = totalHeight, minTop2 = totalHeight;
	    increment = totalHeight;
	    /* Find the RItem whose top edge is >= y1 by smallest amount. */
	    for (range = rangeFirst; range != NULL; range = range->next) {
		if (y1 >= range->totalHeight)
		    continue;

		rItem = Range_ItemUnderPoint(tree, range, -666,
		    y1 - range->offset.y, NULL, NULL, 3);
		if (range->offset.y + rItem->offset >= y1)
		    increment = MIN(increment, range->offset.y + rItem->offset);
		if (range->offset.y + rItem->offset > y1)
		    minTop1 = MIN(minTop1, range->offset.y + rItem->offset);
		if (rItem != range->last) {
		    rItem += 1;
		    if (range->offset.y + rItem->offset > y1)
			minTop2 = MIN(minTop2, range->offset.y + rItem->offset);
		}
	    }
	    if (increment == totalHeight)
		break;
	    if (increment > prev) {
		size = Increment_AddY(tree, increment, size);
		prev = increment;
	    }
	    if (minTop1 == totalHeight && minTop2 == totalHeight)
		break; /* no increments > y1 */
	    if (minTop1 != totalHeight && minTop1 > increment)
		y1 = minTop1;
	    else
		y1 = minTop2;
	}
    }

    if ((visHeight > 1) && (totalHeight -
		dIncr->increments[dIncr->count - 1] > visHeight)) {
	size = Increment_AddY(tree, totalHeight, size);
	dIncr->count--;
	dIncr->increments[dIncr->count - 1] = totalHeight - visHeight;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RangesToIncrementsX --
 *
 *	Recalculate the list of horizontal scroll increments. This gets
 *	called when -orient=vertical and -xscrollincrement=0.
 *
 * Results:
 *	DInfo.xScrollIncrements is updated if there are any Ranges.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
RangesToIncrementsX(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DScrollIncrements *dIncr = &dInfo->xScrollIncrements;
    Range *range = dInfo->rangeFirst;
    int visWidth = Tree_ContentWidth(tree);
    int totalWidth = Tree_CanvasWidth(tree);
    int size, prev;

    if (totalWidth <= 0)
	return;

    /* First increment is zero */
    size = 10;
    dIncr->increments = (int *) ckalloc(size * sizeof(int));
    dIncr->increments[dIncr->count++] = 0;
    prev = 0;

    while (range != NULL) {
	if (range->offset.x > prev) {
	    size = Increment_AddX(tree, range->offset.x, size);
	    prev = range->offset.x;
	}
	range = range->next;
    }

    if ((visWidth > 1) && (totalWidth -
		dIncr->increments[dIncr->count - 1] > visWidth)) {
	size = Increment_AddX(tree, totalWidth, size);
	dIncr->count--;
	dIncr->increments[dIncr->count - 1] = totalWidth - visWidth;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RangesToIncrementsY --
 *
 *	Recalculate the list of vertical scroll increments. This gets
 *	called when -orient=horizontal and -yscrollincrement=0.
 *
 * Results:
 *	DInfo.yScrollIncrements is updated if there are any Ranges.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
RangesToIncrementsY(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DScrollIncrements *dIncr = &dInfo->yScrollIncrements;
    Range *range = dInfo->rangeFirst;
    int visHeight = Tree_ContentHeight(tree);
    int totalHeight = Tree_CanvasHeight(tree);
    int size, prev;

    if (totalHeight <= 0)
	return;

    /* First increment is zero */
    size = 10;
    dIncr->increments = (int *) ckalloc(size * sizeof(int));
    dIncr->increments[dIncr->count++] = 0;
    prev = 0;

    while (range != NULL) {
	if (range->offset.y > prev) {
	    size = Increment_AddY(tree, range->offset.y, size);
	    prev = range->offset.y;
	}
	range = range->next;
    }

    if ((visHeight > 1) && (totalHeight -
		dIncr->increments[dIncr->count - 1] > visHeight)) {
	size = Increment_AddY(tree, totalHeight, size);
	dIncr->count--;
	dIncr->increments[dIncr->count - 1] = totalHeight - visHeight;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Increment_Redo --
 *
 *	Recalculate the lists of scroll increments.
 *
 * Results:
 *	DInfo.xScrollIncrements and DInfo.yScrollIncrements are updated.
 *	Either may be set to NULL. The old values are freed, if any.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
Increment_Redo(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DScrollIncrements *xIncr = &dInfo->xScrollIncrements;
    DScrollIncrements *yIncr = &dInfo->yScrollIncrements;

    /* Free x */
    if (xIncr->increments != NULL)
	ckfree((char *) xIncr->increments);
    xIncr->increments = NULL;
    xIncr->count = 0;

    /* Free y */
    if (yIncr->increments != NULL)
	ckfree((char *) yIncr->increments);
    yIncr->increments = NULL;
    yIncr->count = 0;

    if (tree->vertical) {
	/* No xScrollIncrement is given. Snap to left edge of a Range */
	if (tree->xScrollIncrement <= 0)
	    RangesToIncrementsX(tree);

	/* No yScrollIncrement is given. Snap to top edge of a TreeItem */
	if (tree->yScrollIncrement <= 0)
	    RItemsToIncrementsY(tree);
    } else {
	/* No xScrollIncrement is given. Snap to left edge of a TreeItem */
	if (tree->xScrollIncrement <= 0)
	    RItemsToIncrementsX(tree);

	/* No yScrollIncrement is given. Snap to top edge of a Range */
	if (tree->yScrollIncrement <= 0)
	    RangesToIncrementsY(tree);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Increment_RedoIfNeeded --
 *
 *	Recalculate the lists of scroll increments if needed.
 *
 * Results:
 *	DInfo.xScrollIncrements and DInfo.yScrollIncrements may be
 *	updated.
 *
 * Side effects:
 *	Memory may be allocated. The list of Ranges will be recalculated
 *	if needed.
 *
 *----------------------------------------------------------------------
 */

static void
Increment_RedoIfNeeded(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;

    Range_RedoIfNeeded(tree);

    if (dInfo->flags & DINFO_REDO_INCREMENTS) {
	Increment_Redo(tree);
	dInfo->fakeCanvasWidth = dInfo->fakeCanvasHeight = -1;
	dInfo->flags &= ~DINFO_REDO_INCREMENTS;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * B_IncrementFind --
 *
 *	Search a list of increments and return one nearest to the
 *	given offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *	Panic() if no appropriate offset if found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
B_IncrementFind(
    int *increments,		/* DInfo.x|yScrollIncrements. */
    int count,			/* Length of increments[]. */
    int offset			/* Offset to search for. */
    )
{
    int i, l, u, v;

    if (offset < 0)
	offset = 0;

    /* Binary search */
    l = 0;
    u = count - 1;
    while (l <= u) {
	i = (l + u) / 2;
	v = increments[i];
	if ((offset >= v) && ((i == count - 1) || (offset < increments[i + 1])))
	    return i;
	if (offset < v)
	    u = i - 1;
	else
	    l = i + 1;
    }
    panic("B_IncrementFind failed (count %d offset %d)", count, offset);
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * B_IncrementFindX --
 *
 *	Search DInfo.xScrollIncrements and return one nearest to the
 *	given offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *	Panic() if no appropriate offset if found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
B_IncrementFindX(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Offset to search for. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DScrollIncrements *dIncr = &dInfo->xScrollIncrements;

    return B_IncrementFind(
	dIncr->increments,
	dIncr->count,
	offset);
}

/*
 *----------------------------------------------------------------------
 *
 * B_IncrementFindY --
 *
 *	Search DInfo.yScrollIncrements and return one nearest to the
 *	given offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *	Panic() if no appropriate offset if found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
B_IncrementFindY(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Offset to search with. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DScrollIncrements *dIncr = &dInfo->yScrollIncrements;

    return B_IncrementFind(
	dIncr->increments,
	dIncr->count,
	offset);
}

/*
 *--------------------------------------------------------------
 *
 * TreeXviewCmd --
 *
 *	This procedure is invoked to process the "xview" option for
 *	the widget command for a TreeCtrl. See the user documentation
 *	for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
TreeXviewCmd(
    TreeCtrl *tree,		/* Widget info. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    Tcl_Interp *interp = tree->interp;

    if (objc == 2) {
	double fractions[2];
	Tcl_Obj *listObj;

	Tree_GetScrollFractionsX(tree, fractions);

	listObj = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(interp, listObj,
	    Tcl_NewDoubleObj(fractions[0]));
	Tcl_ListObjAppendElement(interp, listObj,
	    Tcl_NewDoubleObj(fractions[1]));
	Tcl_SetObjResult(interp, listObj);
    } else {
	int count, index = 0, indexMax, offset, type;
	double fraction;
	int visWidth = Tree_ContentWidth(tree);
	int totWidth = Tree_CanvasWidth(tree);

	if (visWidth < 0)
	    visWidth = 0;
	if (totWidth <= visWidth)
	    return TCL_OK;

	type = Tk_GetScrollInfoObj(interp, objc, objv, &fraction, &count);
	Tree_SetScrollSmoothingX(tree, type != TK_SCROLL_UNITS);

	totWidth = Tree_FakeCanvasWidth(tree);
	if (visWidth > 1) {
	    indexMax = Increment_FindX(tree, totWidth - visWidth);
	} else {
	    indexMax = Increment_FindX(tree, totWidth);
	    visWidth = 1;
	}

	switch (type) {
	    case TK_SCROLL_ERROR:
		return TCL_ERROR;
	    case TK_SCROLL_MOVETO:
		offset = (int) (fraction * totWidth + 0.5);
		index = Increment_FindX(tree, offset);
		break;
	    case TK_SCROLL_PAGES:
		offset = W2Cx(Tree_ContentLeft(tree));
		offset += (int) (count * visWidth * 0.9);
		index = Increment_FindX(tree, offset);
		if ((count > 0) && (index ==
			    Increment_FindX(tree, W2Cx(Tree_ContentLeft(tree)))))
		    index++;
		break;
	    case TK_SCROLL_UNITS:
		offset = W2Cx(Tree_ContentLeft(tree));
		index = Increment_FindX(tree, offset);
		offset = Increment_ToOffsetX(tree, index);
		/* If the left-most increment is partially visible due to
		 * non-unit scrolling, then scrolling by -1 units should
		 * snap to the left of the partially-visible increment. */
		if ((C2Wx(offset) < Tree_ContentLeft(tree)) && (count < 0))
		    index++;
		index += count;
		break;
	}

	/* Don't scroll too far left */
	if (index < 0)
	    index = 0;

	/* Don't scroll too far right */
	if (index > indexMax)
	    index = indexMax;

	offset = Increment_ToOffsetX(tree, index);
	if (tree->xOrigin != offset - Tree_ContentLeft(tree)) {
	    tree->xOrigin = offset - Tree_ContentLeft(tree);
	    Tree_EventuallyRedraw(tree);
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TreeYviewCmd --
 *
 *	This procedure is invoked to process the "yview" option for
 *	the widget command for a TreeCtrl. See the user documentation
 *	for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
TreeYviewCmd(
    TreeCtrl *tree,		/* Widget info. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    Tcl_Interp *interp = tree->interp;

    if (objc == 2) {
	double fractions[2];
	Tcl_Obj *listObj;

	Tree_GetScrollFractionsY(tree, fractions);

	listObj = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(interp, listObj,
	    Tcl_NewDoubleObj(fractions[0]));
	Tcl_ListObjAppendElement(interp, listObj,
	    Tcl_NewDoubleObj(fractions[1]));
	Tcl_SetObjResult(interp, listObj);
    }
    else {
	int count, index = 0, indexMax, offset, type;
	double fraction;
	int visHeight = Tree_ContentHeight(tree);
	int totHeight = Tree_CanvasHeight(tree);

	if (visHeight < 0)
	    visHeight = 0;
	if (totHeight <= visHeight)
	    return TCL_OK;

	type = Tk_GetScrollInfoObj(interp, objc, objv, &fraction, &count);
	Tree_SetScrollSmoothingY(tree, type != TK_SCROLL_UNITS);

	totHeight = Tree_FakeCanvasHeight(tree);
	if (visHeight > 1) {
	    indexMax = Increment_FindY(tree, totHeight - visHeight);
	} else {
	    indexMax = Increment_FindY(tree, totHeight);
	    visHeight = 1;
	}

	switch (type) {
	    case TK_SCROLL_ERROR:
		return TCL_ERROR;
	    case TK_SCROLL_MOVETO:
		offset = (int) (fraction * totHeight + 0.5);
		index = Increment_FindY(tree, offset);
		break;
	    case TK_SCROLL_PAGES:
		offset = W2Cy(Tree_ContentTop(tree));
		offset += (int) (count * visHeight * 0.9);
		index = Increment_FindY(tree, offset);
		if ((count > 0) && (index ==
			    Increment_FindY(tree, W2Cy(Tree_ContentTop(tree)))))
		    index++;
		break;
	    case TK_SCROLL_UNITS:
		offset = W2Cy(Tree_ContentTop(tree));
		index = Increment_FindY(tree, offset);
		offset = Increment_ToOffsetY(tree, index);
		/* If the top-most increment is partially visible due to
		 * non-unit scrolling, then scrolling by -1 units should
		 * snap to the top of the partially-visible increment. */
		if ((C2Wy(offset) < Tree_ContentTop(tree)) && (count < 0))
		    index++;
		index += count;
		break;
	}

	/* Don't scroll too far up */
	if (index < 0)
	    index = 0;

	/* Don't scroll too far down */
	if (index > indexMax)
	    index = indexMax;

	offset = Increment_ToOffsetY(tree, index);
	if (tree->yOrigin != offset - Tree_ContentTop(tree)) {
	    tree->yOrigin = offset - Tree_ContentTop(tree);
	    Tree_EventuallyRedraw(tree);
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ItemUnderPoint --
 *
 *	Return a TreeItem containing the given coordinates.
 *
 * Results:
 *	TreeItem token or NULL if no item contains the point.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *--------------------------------------------------------------
 */

TreeItem
Tree_ItemUnderPoint(
    TreeCtrl *tree,		/* Widget info. */
    int *x_, int *y_,		/* In: window coordinates.
				 * Out: coordinates relative to top-left
				 * corner of the returned item. */
    int nearest			/* TRUE if the item nearest the coordinates
				 * should be returned. */
    )
{
    Range *range;
    RItem *rItem;
    int hit;

    hit = Tree_HitTest(tree, *x_, *y_);
    if (!nearest && ((hit == TREE_AREA_LEFT) || (hit == TREE_AREA_RIGHT))) {
	TreeDInfo dInfo = tree->dInfo;

	Range_RedoIfNeeded(tree);
	range = dInfo->rangeFirst;

	/* If range is NULL use dInfo->rangeLock. */
	if (range == NULL) {
	    if (dInfo->rangeLock == NULL)
		return NULL;
	    range = dInfo->rangeLock;
	}

	if (W2Cy(*y_) < range->offset.y + range->totalHeight) {
	    int x = *x_;
	    int y = *y_;

	    if (hit == TREE_AREA_RIGHT) {
		x -= Tree_ContentRight(tree);
	    } else {
		x -= Tree_BorderLeft(tree);
	    }

	    y = W2Cy(y) - range->offset.y;

	    rItem = Range_ItemUnderPoint(tree, range, -666, y, NULL, &y, 0);
	    if (rItem != NULL) {
		*x_ = x;
		*y_ = y;
		return rItem->item;
	    }
	}
	return NULL;
    }

    range = Range_UnderPoint(tree, x_, y_, nearest);
    if (range == NULL)
	return NULL;
    rItem = Range_ItemUnderPoint(tree, range, *x_, *y_, x_, y_, nearest ? 1 : 0);
    if (rItem != NULL)
	return rItem->item;
    return NULL;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_AreaBbox --
 *
 *	Return the bounding box of a visible area.
 *
 * Results:
 *	Return value is TRUE if the area is non-empty.
 *
 * Side effects:
 *	Column and item layout will be updated if needed.
 *
 *--------------------------------------------------------------
 */

int
Tree_AreaBbox(
    TreeCtrl *tree,
    int area,
    TreeRectangle *tr
    )
{
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;

    switch (area) {
	case TREE_AREA_HEADER:
	    x1 = Tree_BorderLeft(tree);
	    y1 = Tree_BorderTop(tree);
	    x2 = Tree_BorderRight(tree);
	    y2 = Tree_ContentTop(tree);
	    break;
	case TREE_AREA_CONTENT:
	    x1 = Tree_ContentLeft(tree);
	    y1 = Tree_ContentTop(tree);
	    x2 = Tree_ContentRight(tree);
	    y2 = Tree_ContentBottom(tree);
	    break;
	case TREE_AREA_LEFT:
	    x1 = Tree_BorderLeft(tree);
	    y1 = Tree_ContentTop(tree);
	    x2 = Tree_ContentLeft(tree);
	    y2 = Tree_ContentBottom(tree);
	    /* Don't overlap right-locked columns. */
	    if (x2 > Tree_ContentRight(tree))
		x2 = Tree_ContentRight(tree);
	    break;
	case TREE_AREA_RIGHT:
	    x1 = Tree_ContentRight(tree);
	    y1 = Tree_ContentTop(tree);
	    x2 = Tree_BorderRight(tree);
	    y2 = Tree_ContentBottom(tree);
	    break;
    }

    if (x2 <= x1 || y2 <= y1)
	return FALSE;

    if (x1 < Tree_BorderLeft(tree))
	x1 = Tree_BorderLeft(tree);
    if (x2 > Tree_BorderRight(tree))
	x2 = Tree_BorderRight(tree);

    if (y1 < Tree_BorderTop(tree))
	y1 = Tree_BorderTop(tree);
    if (y2 > Tree_BorderBottom(tree))
	y2 = Tree_BorderBottom(tree);

    TreeRect_SetXYXY(*tr, x1, y1, x2, y2);
    return (x2 > x1) && (y2 > y1);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_HitTest --
 *
 *	Determine which are of the window contains the given point.
 *
 * Results:
 *	Return value is one of the TREE_AREA_xxx constants.
 *
 * Side effects:
 *	Column layout will be updated if needed.
 *
 *--------------------------------------------------------------
 */

int
Tree_HitTest(
    TreeCtrl *tree,
    int x,
    int y
    )
{
    if ((x < Tree_BorderLeft(tree)) || (x >= Tree_BorderRight(tree)))
	return TREE_AREA_NONE;
    if ((y < Tree_BorderTop(tree)) || (y >= Tree_BorderBottom(tree)))
	return TREE_AREA_NONE;

    if (y < Tree_HeaderBottom(tree)) {
	return TREE_AREA_HEADER;
    }
    /* Right-locked columns are drawn over the left. */
    if (x >= Tree_ContentRight(tree)) {
	return TREE_AREA_RIGHT;
    }
    /* Left-locked columns are drawn over the non-locked. */
    if (x < Tree_ContentLeft(tree)) {
	return TREE_AREA_LEFT;
    }
    if (Tree_ContentLeft(tree) >= Tree_ContentRight(tree)) {
	return TREE_AREA_NONE;
    }
    return TREE_AREA_CONTENT;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ItemBbox --
 *
 *	Return the bounding box for an item.
 *
 * Results:
 *	Return value is -1 if the item is not ReallyVisible()
 *	or if there are no visible columns. The coordinates
 *	are relative to the top-left corner of the canvas.
 *
 * Side effects:
 *	Column layout will be updated if needed.
 *	The list of Ranges will be recalculated if needed.
 *
 *--------------------------------------------------------------
 */

int
Tree_ItemBbox(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item whose bbox is needed. */
    int lock,			/* COLUMN_LOCK_xxx. */
    TreeRectangle *tr		/* Returned bbox. */
    )
{
    Range *range;
    RItem *rItem;

    if (!TreeItem_ReallyVisible(tree, item))
	return -1;

    /* Update columnCountVisXXX if needed */
    (void) Tree_WidthOfColumns(tree);

    Range_RedoIfNeeded(tree);
    rItem = (RItem *) TreeItem_GetRInfo(tree, item);

    switch (lock) {
	case COLUMN_LOCK_LEFT:
	    if (tree->columnCountVisLeft == 0)
		return -1;
	    TreeRect_SetXYWH(*tr,
		W2Cx(Tree_BorderLeft(tree)),
		rItem->offset,
		Tree_WidthOfLeftColumns(tree),
		rItem->size);
	    return 0;
	case COLUMN_LOCK_NONE:
	    break;
	case COLUMN_LOCK_RIGHT:
	    if (tree->columnCountVisRight == 0)
		return -1;
	    TreeRect_SetXYWH(*tr,
		W2Cx(Tree_ContentRight(tree)),
		rItem->offset,
		Tree_WidthOfRightColumns(tree),
		rItem->size);
	    return 0;
    }

    if (tree->columnCountVis < 1)
	return -1;

    range = rItem->range;
    if (tree->vertical) {
	TreeRect_SetXYWH(*tr,
	    range->offset.x,
	    range->offset.y + rItem->offset,
	    range->totalWidth,
	    rItem->size);
    }
    else {
	TreeRect_SetXYWH(*tr,
	    range->offset.x + rItem->offset,
	    range->offset.y,
	    rItem->size,
	    range->totalHeight);
    }
    return 0;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ItemLARB --
 *
 *	Return an adjacent item above, below, to the left or to the
 *	right of the given item.
 *
 * Results:
 *	An adjacent item or NULL if there is no such item.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *--------------------------------------------------------------
 */

TreeItem
Tree_ItemLARB(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item to use as a reference. */
    int vertical,		/* TRUE if items are arranged
				 * from top-to-bottom in each Range. */
    int prev			/* TRUE for above/left,
				 * FALSE for below/right. */
    )
{
    RItem *rItem;
    Range *range;

    if (!TreeItem_ReallyVisible(tree, item) || (tree->columnCountVis < 1))
	return NULL;
    Range_RedoIfNeeded(tree);
    rItem = (RItem *) TreeItem_GetRInfo(tree, item);
    if (vertical) {
	if (prev) {
	    if (rItem == rItem->range->first)
		return NULL;
	    rItem--;
	}
	else {
	    if (rItem == rItem->range->last)
		return NULL;
	    rItem++;
	}
	return rItem->item;
    }
    else {
	range = prev ? rItem->range->prev : rItem->range->next;
	if (range == NULL)
	    return NULL;

	/* Find item with same index */
	if (range->last->index < rItem->index)
	    return NULL;
	return (range->first + rItem->index)->item;
    }
    return NULL;
}

TreeItem
Tree_ItemLeft(
    TreeCtrl *tree,
    TreeItem item)
{
    return Tree_ItemLARB(tree, item, !tree->vertical, TRUE);
}

TreeItem
Tree_ItemAbove(
    TreeCtrl *tree,
    TreeItem item)
{
    return Tree_ItemLARB(tree, item, tree->vertical, TRUE);
}

TreeItem
Tree_ItemRight(
    TreeCtrl *tree,
    TreeItem item)
{
    return Tree_ItemLARB(tree, item, !tree->vertical, FALSE);
}

TreeItem
Tree_ItemBelow(
    TreeCtrl *tree,
    TreeItem item)
{
    return Tree_ItemLARB(tree, item, tree->vertical, FALSE);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ItemFL --
 *
 *	Return the first or last item in the same row or column
 *	as the given item.
 *
 * Results:
 *	First/last item or NULL if there is no such item.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *--------------------------------------------------------------
 */

TreeItem
Tree_ItemFL(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item to use as a reference. */
    int vertical,		/* TRUE if items are arranged
				 * from top-to-bottom in each Range. */
    int first			/* TRUE for top/left,
				 * FALSE for bottom/right. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    RItem *rItem;
    Range *range;

    if (!TreeItem_ReallyVisible(tree, item) || (tree->columnCountVis < 1)) {
	return NULL;
    }
    Range_RedoIfNeeded(tree);
    rItem = (RItem *) TreeItem_GetRInfo(tree, item);
    if (vertical) {
	return (first ? rItem->range->first->item : rItem->range->last->item);
    } else {
	/* Find the first/last range */
	range = first ? dInfo->rangeFirst : dInfo->rangeLast;

	/* Check next/prev range until happy */
	while (1) {
	    if (range == rItem->range)
		return item;

	    if (range->last->index >= rItem->index)
		return (range->first + rItem->index)->item;

	    range = first ? range->next : range->prev;
	}
    }
    return NULL;
}

TreeItem
Tree_ItemTop(
    TreeCtrl *tree,
    TreeItem item)
{
    return Tree_ItemFL(tree, item, tree->vertical, TRUE);
}

TreeItem
Tree_ItemBottom(
    TreeCtrl *tree,
    TreeItem item)
{
    return Tree_ItemFL(tree, item, tree->vertical, FALSE);
}

TreeItem
Tree_ItemLeftMost(
    TreeCtrl *tree,
    TreeItem item)
{
    return Tree_ItemFL(tree, item, !tree->vertical, TRUE);
}

TreeItem
Tree_ItemRightMost(
    TreeCtrl *tree,
    TreeItem item)
{
    return Tree_ItemFL(tree, item, !tree->vertical, FALSE);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ItemToRNC --
 *
 *	Return the row and column for the given item.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *--------------------------------------------------------------
 */

int
Tree_ItemToRNC(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item to get row n' column of. */
    int *row, int *col		/* Returned row and column. */
    )
{
    RItem *rItem;

    if (!TreeItem_ReallyVisible(tree, item) || (tree->columnCountVis < 1))
	return TCL_ERROR;
    Range_RedoIfNeeded(tree);
    rItem = (RItem *) TreeItem_GetRInfo(tree, item);
    if (tree->vertical) {
	(*row) = rItem->index;
	(*col) = rItem->range->index;
    }
    else {
	(*row) = rItem->range->index;
	(*col) = rItem->index;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_RNCToItem --
 *
 *	Return the item at a given row and column.
 *
 * Results:
 *	Token for the item. Never returns NULL unless there are no
 *	Ranges.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *--------------------------------------------------------------
 */

TreeItem
Tree_RNCToItem(
    TreeCtrl *tree,		/* Widget info. */
    int row, int col		/* Row and column. These values are
				 * clipped to valid values. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    Range *range;
    RItem *rItem;

    Range_RedoIfNeeded(tree);
    range = dInfo->rangeFirst;
    if (range == NULL)
	return NULL;
    if (row < 0)
	row = 0;
    if (col < 0)
	col = 0;
    if (tree->vertical) {
	if (col > dInfo->rangeLast->index)
	    col = dInfo->rangeLast->index;
	while (range->index != col)
	    range = range->next;
	rItem = range->last;
	if (row > rItem->index)
	    row = rItem->index;
	return (range->first + row)->item;
    }
    else {
	if (row > dInfo->rangeLast->index)
	    row = dInfo->rangeLast->index;
	while (range->index != row)
	    range = range->next;
	rItem = range->last;
	if (col > rItem->index)
	    col = rItem->index;
	return (range->first + col)->item;
    }
    return rItem->item;
}

/*=============*/

static void
DisplayDelay(TreeCtrl *tree)
{
    if (tree->debug.enable &&
	    tree->debug.display &&
	    tree->debug.displayDelay > 0) {
#if !defined(WIN32) && !defined(MAC_OSX_TK)
	XSync(tree->display, False);
#endif
	Tcl_Sleep(tree->debug.displayDelay);
    }
}

/*
 *--------------------------------------------------------------
 *
 * DItem_Alloc --
 *
 *	Allocate and initialize a new DItem, and store a pointer to it
 *	in the given item.
 *
 * Results:
 *	Pointer to the DItem which may come from an existing pool of
 *	unused DItems.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *--------------------------------------------------------------
 */

static DItem *
DItem_Alloc(
    TreeCtrl *tree,		/* Widget info. */
    RItem *rItem		/* Range info for the item. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DItem *dItem;

    dItem = (DItem *) TreeItem_GetDInfo(tree, rItem->item);
    if (dItem != NULL)
	panic("tried to allocate duplicate DItem");

    /* Pop unused DItem from stack */
    if (dInfo->dItemFree != NULL) {
	dItem = dInfo->dItemFree;
	dInfo->dItemFree = dItem->next;
    /* No free DItems, alloc a new one */
    } else {
	dItem = (DItem *) ckalloc(sizeof(DItem));
    }
    memset(dItem, '\0', sizeof(DItem));
#ifdef TREECTRL_DEBUG
    strncpy(dItem->magic, "MAGC", 4);
#endif
    dItem->item = rItem->item;
    dItem->area.flags = DITEM_DIRTY | DITEM_ALL_DIRTY;
    dItem->left.flags = DITEM_DIRTY | DITEM_ALL_DIRTY;
    dItem->right.flags = DITEM_DIRTY | DITEM_ALL_DIRTY;
    TreeItem_SetDInfo(tree, rItem->item, (TreeItemDInfo) dItem);
    return dItem;
}

/*
 *--------------------------------------------------------------
 *
 * DItem_Unlink --
 *
 *	Remove a DItem from a linked list of DItems.
 *
 * Results:
 *	Pointer to the given list of DItems.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static DItem *
DItem_Unlink(
    DItem *head,		/* First in linked list. */
    DItem *dItem		/* DItem to remove from list. */
    )
{
    DItem *prev;

    if (head == dItem)
	head = dItem->next;
    else {
	for (prev = head;
	     prev->next != dItem;
	     prev = prev->next) {
	    /* nothing */
	}
	prev->next = dItem->next;
    }
    dItem->next = NULL;
    return head;
}

/*
 *--------------------------------------------------------------
 *
 * DItem_Free --
 *
 *	Add a DItem to the pool of unused DItems. If the DItem belongs
 *	to a TreeItem the pointer to the DItem is set to NULL in that
 *	TreeItem.
 *
 * Results:
 *	Pointer to the next DItem.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static DItem *
DItem_Free(
    TreeCtrl *tree,		/* Widget info. */
    DItem *dItem		/* DItem to free. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DItem *next = dItem->next;
#ifdef TREECTRL_DEBUG
    if (strncmp(dItem->magic, "MAGC", 4) != 0)
	panic("DItem_Free: dItem.magic != MAGC");
#endif
    if (dItem->item != NULL) {
	TreeItem_SetDInfo(tree, dItem->item, (TreeItemDInfo) NULL);
	dItem->item = NULL;
    }
    /* Push unused DItem on the stack */
    dItem->next = dInfo->dItemFree;
    dInfo->dItemFree = dItem;
    return next;
}

/*
 *--------------------------------------------------------------
 *
 * FreeDItems --
 *
 *	Add a list of DItems to the pool of unused DItems,
 *	optionally removing the DItems from the DInfo.dItem list.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
FreeDItems(
    TreeCtrl *tree,		/* Widget info. */
    DItem *first,		/* First DItem to free. */
    DItem *last,		/* DItem after the last one to free. */
    int unlink			/* TRUE if the DItems should be removed
				 * from the DInfo.dItem list. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DItem *prev;

    if (unlink) {
	if (dInfo->dItem == first)
	    dInfo->dItem = last;
	else {
	    for (prev = dInfo->dItem;
		 prev->next != first;
		 prev = prev->next) {
		/* nothing */
	    }
	    prev->next = last;
	}
    }
    while (first != last)
	first = DItem_Free(tree, first);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ItemsInArea --
 *
 *	Return a list of items overlapping the given area.
 *
 * Results:
 *	Initializes the given TreeItemList and appends any items
 *	in the given area.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed. Memory may
 *	be allocated.
 *
 *--------------------------------------------------------------
 */

void
Tree_ItemsInArea(
    TreeCtrl *tree,		/* Widget info. */
    TreeItemList *items,	/* Uninitialized list. The caller must free
				 * it with TreeItemList_Free. */
    int minX, int minY,		/* Left, top in canvas coordinates. */
    int maxX, int maxY		/* Right, bottom in canvas coordinates.
				 * Points on the right/bottom edge are not
				 * included in the area. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    int rx, ry;
    Range *range;
    RItem *rItem;

    TreeItemList_Init(tree, items, 0);

    Range_RedoIfNeeded(tree);
    range = dInfo->rangeFirst;

    if (tree->vertical) {
	/* Find the first range which could be in the area horizontally */
	while (range != NULL) {
	    if ((range->offset.x < maxX) &&
		    (range->offset.x + range->totalWidth > minX)) {
		break;
	    }
	    range = range->next;
	}
    }
    else {
	/* Find the first range which could be in the area vertically */
	while (range != NULL) {
	    if ((range->offset.y < maxY) &&
		    (range->offset.y + range->totalHeight > minY)) {
		break;
	    }
	    range = range->next;
	}
    }

    if (range == NULL)
	return;

    while (range != NULL) {
	rx = range->offset.x;
	ry = range->offset.y;
	if ((rx + range->totalWidth > minX) &&
		(ry + range->totalHeight > minY)) {

	    rItem = Range_ItemUnderPoint(tree, range, minX - rx, minY - ry,
		NULL, NULL, 3);

	    while (1) {
		if (tree->vertical) {
		    if (ry + rItem->offset >= maxY)
			break;
		}
		else {
		    if (rx + rItem->offset >= maxX)
			break;
		}
		TreeItemList_Append(items, rItem->item);
		if (rItem == range->last)
		    break;
		rItem++;
	    }
	}
	if (tree->vertical) {
	    if (rx + range->totalWidth >= maxX)
		break;
	    rx += range->totalWidth;
	}
	else {
	    if (ry + range->totalHeight >= maxY)
		break;
	    ry += range->totalHeight;
	}
	range = range->next;
    }
}

#define DCOLUMN
#ifdef DCOLUMN

/*
 *--------------------------------------------------------------
 *
 * GetOnScreenColumnsForItemAux --
 *
 *	Determine which columns of an item are onscreen.
 *
 * Results:
 *	Sets the column list.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *--------------------------------------------------------------
 */

static void
GetOnScreenColumnsForItemAux(
    TreeCtrl *tree,		/* Widget info. */
    DItem *dItem,		/* Display info for an item. */
    DItemArea *area,		/* Layout info. */
    TreeRectangle bounds,	/* TREE_AREA_xxx bounds. */
    int lock,			/* Set of columns we care about. */
    TreeColumnList *columns	/* Initialized list to append to. */
    )
{
    int minX, maxX, columnIndex = 0, x = 0, i, width;
    TreeColumn column = NULL, column2;

    minX = MAX(area->x, TreeRect_Left(bounds));
    maxX = MIN(area->x + area->width, TreeRect_Right(bounds));

    minX -= area->x;
    maxX -= area->x;

    switch (lock) {
	case COLUMN_LOCK_LEFT:
	    column = tree->columnLockLeft;
	    break;
	case COLUMN_LOCK_NONE:
	    column = tree->columnLockNone;
	    break;
	case COLUMN_LOCK_RIGHT:
	    column = tree->columnLockRight;
	    break;
    }

    for (columnIndex = TreeColumn_Index(column);
	    columnIndex < tree->columnCount; columnIndex++) {
	if (TreeColumn_Lock(column) != lock)
	    break;
	column2 = TreeColumn_Next(column);
	width = TreeColumn_GetDInfo(column)->width;
	if (width == 0) /* also handles hidden columns */
	    goto next;
	if (dItem->spans != NULL) {
	    /* FIXME: not possible since I skip over the entire span. */
	    if (dItem->spans[columnIndex] != columnIndex)
		goto next;
	    /* Calculate the width of the span. */
	    for (i = columnIndex + 1; i < tree->columnCount &&
		    dItem->spans[i] == columnIndex; i++) {
		width += TreeColumn_GetDInfo(column2)->width;
		column2 = TreeColumn_Next(column2);
	    }
	    columnIndex = i - 1;
	}
	if (x < maxX && x + width > minX) {
	    TreeColumnList_Append(columns, column);
	}
next:
	x += width;
	if (x >= maxX)
	    break;
	column = column2;
    }
}

/*
 *--------------------------------------------------------------
 *
 * GetOnScreenColumnsForItem --
 *
 *	Determine which columns of an item are onscreen.
 *
 * Results:
 *	Sets the column list.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *--------------------------------------------------------------
 */

static int
GetOnScreenColumnsForItem(
    TreeCtrl *tree,		/* Widget info. */
    DItem *dItem,		/* Display info for an item. */
    TreeColumnList *columns	/* Initialized list to append to. */
    )
{
    TreeDInfo dInfo = tree->dInfo;

    if (!dInfo->emptyL) {
	GetOnScreenColumnsForItemAux(tree, dItem, &dItem->left,
		dInfo->boundsL, COLUMN_LOCK_LEFT, columns);
    }
    if (!dInfo->empty && dInfo->rangeFirstD != NULL) {
	GetOnScreenColumnsForItemAux(tree, dItem, &dItem->area,
		dInfo->bounds, COLUMN_LOCK_NONE, columns);
    }
    if (!dInfo->emptyR) {
	GetOnScreenColumnsForItemAux(tree, dItem, &dItem->right,
		dInfo->boundsR, COLUMN_LOCK_RIGHT, columns);
    }
    return TreeColumnList_Count(columns);
}

/*
 *--------------------------------------------------------------
 *
 * TrackOnScreenColumnsForItem --
 *
 *	Compares the list of onscreen columns for an item to the
 *	list of previously-onscreen columns for the item.
 *
 * Results:
 *	Hides window elements for columns that are no longer
 *	onscreen.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *--------------------------------------------------------------
 */

static void
TrackOnScreenColumnsForItem(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    Tcl_HashEntry *hPtr		/* DInfo.itemVisHash entry. */
    )
{
    TreeColumnList columns;
    TreeColumn column, *value;
    DItem *dItem;
    int i, j, count = 0, n = 0;
    Tcl_DString dString;

    TreeColumnList_Init(tree, &columns, 0);
    Tcl_DStringInit(&dString);

    /* dItem is NULL if the item just went offscreen. */
    dItem = (DItem *) TreeItem_GetDInfo(tree, item);
    if (dItem != NULL)
	count = GetOnScreenColumnsForItem(tree, dItem, &columns);

    if (tree->debug.enable && tree->debug.span)
	DStringAppendf(&dString, "onscreen columns for item %d:",
		TreeItem_GetID(tree, item));

    /* value is NULL if the item just came onscreen. */
    value = (TreeColumn *) Tcl_GetHashValue(hPtr);
    if (value == NULL) {
	value = (TreeColumn *) ckalloc(sizeof(TreeColumn) * (count + 1));
	value[0] = NULL;
    }

    /* Track newly-visible columns */
    for (i = 0; i < count; i++) {
	column = TreeColumnList_Nth(&columns, i);
	for (j = 0; value[j] != NULL; j++) {
	    if (column == value[j])
		break;
	}
	if (value[j] == NULL) {
	    if (tree->debug.enable && tree->debug.span)
		DStringAppendf(&dString, " +%d", TreeColumn_GetID(column));
	    n++;
	}
    }

    /* Track newly-hidden columns */
    for (j = 0; value[j] != NULL; j++) {
	column = value[j];
	for (i = 0; i < count; i++) {
	    if (TreeColumnList_Nth(&columns, i) == column)
		break;
	}
	if (i == count) {
	    TreeItemColumn itemColumn = TreeItem_FindColumn(tree, item,
		TreeColumn_Index(column));
	    if (itemColumn != NULL) {
		TreeStyle style = TreeItemColumn_GetStyle(tree, itemColumn);
		if (style != NULL)
		    TreeStyle_OnScreen(tree, style, FALSE);
	    }
	    if (tree->debug.enable && tree->debug.span)
		DStringAppendf(&dString, " -%d", TreeColumn_GetID(column));
	    n++;
	}
    }

    if (n && tree->debug.enable && tree->debug.span)
	dbwin("%s\n", Tcl_DStringValue(&dString));

    /* Set the list of onscreen columns unless it is the same or the item
    * is hidden. */
    if (n > 0 && dItem != NULL) {
	value = (TreeColumn *) ckrealloc((char *) value,
		sizeof(TreeColumn) * (count + 1));
	memcpy(value, (TreeColumn *) columns.pointers,
		sizeof(TreeColumn) * count);
	value[count] = NULL;
	Tcl_SetHashValue(hPtr, (ClientData) value);
    }

    Tcl_DStringFree(&dString);
    TreeColumnList_Free(&columns);
}

#endif /* DCOLUMN */

/*
 *--------------------------------------------------------------
 *
 * UpdateDInfoForRange --
 *
 *	Allocates or updates a DItem for every on-screen item in a Range.
 *	If an item already has a DItem (because the item was previously
 *	displayed), then the DItem may be marked dirty if there were
 *	changes to the item's on-screen size or position.
 *
 * Results:
 *	The return value is the possibly-updated dItemHead.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *--------------------------------------------------------------
 */

static DItem *
UpdateDInfoForRange(
    TreeCtrl *tree,		/* Widget info. */
    DItem *dItemHead,		/* Linked list of used DItems. */
    Range *range,		/* Range to update DItems for. */
    RItem *rItem,		/* First item in the Range we care about. */
    int x, int y		/* Left & top window coordinates of rItem. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DItem *dItem;
    DItemArea *area;
    TreeItem item;
    int maxX, maxY;
    int index, indexVis;
    int bgImgWidth, bgImgHeight;

    if (tree->backgroundImage != NULL) {
	Tk_SizeOfImage(tree->backgroundImage, &bgImgWidth, &bgImgHeight);
    }

    maxX = Tree_ContentRight(tree);
    maxY = Tree_ContentBottom(tree);

    /* Top-to-bottom */
    if (tree->vertical) {
	while (1) {
	    item = rItem->item;

	    /* Update item/style layout. This can be needed when using fixed
	     * column widths. */
	    (void) TreeItem_Height(tree, item);

	    TreeItem_ToIndex(tree, item, &index, &indexVis);
	    switch (tree->backgroundMode) {
#ifdef DEPRECATED
		case BG_MODE_INDEX:
#endif
		case BG_MODE_ORDER: break;
#ifdef DEPRECATED
		case BG_MODE_VISINDEX:
#endif
		case BG_MODE_ORDERVIS: index = indexVis; break;
		case BG_MODE_COLUMN: index = range->index; break;
		case BG_MODE_ROW: index = rItem->index; break;
	    }

	    y = C2Wy(range->offset.y + rItem->offset);

	    dItem = (DItem *) TreeItem_GetDInfo(tree, item);

	    /* Re-use a previously allocated DItem */
	    if (dItem != NULL) {
		dItemHead = DItem_Unlink(dItemHead, dItem);
		area = &dItem->area;

		/* This item is already marked for total redraw */
		if (area->flags & DITEM_ALL_DIRTY)
		    ; /* nothing */

		/* All display info is marked as invalid */
		else if (dInfo->flags & DINFO_INVALIDATE)
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		else if ((dItem->flags & DITEM_INVALIDATE_ON_SCROLL_X)
			&& (x != dItem->oldX))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		else if ((dItem->flags & DITEM_INVALIDATE_ON_SCROLL_Y)
			&& (y != dItem->oldY))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* The range may have changed size */
		else if ((area->width != range->totalWidth) ||
			(dItem->height != rItem->size))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* Items may have alternating background colors. */
		else if ((tree->columnBgCnt > 1) &&
			((index % tree->columnBgCnt) !=
				(dItem->index % tree->columnBgCnt)))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* We don't copy items horizontally to their new position,
		 * except for horizontal scrolling which moves the whole
		 * range */
		else if (x - dItem->oldX != dInfo->xOrigin - tree->xOrigin)
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* If we are displaying dotted lines and the item has moved
		 * from odd-top to non-odd-top or vice versa, must redraw
		 * the lines for this item. */
		else if (tree->showLines &&
			(tree->lineStyle == LINE_STYLE_DOT) &&
			tree->columnTreeVis &&
			(TreeColumn_Lock(tree->columnTree) == COLUMN_LOCK_NONE) &&
			((DW2Cy(dItem->oldY) & 1) != (W2Cy(y) & 1)))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* We can't copy the item to its new position unless it
		 * has the same part of the background image behind it */
		else if ((tree->backgroundImage != NULL) &&
		    (dInfo->xOrigin != tree->xOrigin) &&
		    !(tree->bgImageScroll & BGIMG_SCROLL_X))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		else if ((tree->backgroundImage != NULL) &&
		    (dInfo->yOrigin != tree->yOrigin) &&
		    !(tree->bgImageScroll & BGIMG_SCROLL_Y))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		else if ((tree->backgroundImage != NULL) &&
			((DW2Cy(dItem->oldY) % bgImgHeight) !=
			(W2Cy(y) % bgImgHeight)))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;
	    }

	    /* Make a new DItem */
	    else {
		dItem = DItem_Alloc(tree, rItem);
		area = &dItem->area;
	    }

	    area->x = x;
	    dItem->y = y;
	    area->width = Range_TotalWidth(tree, range);
	    dItem->height = rItem->size;
	    dItem->range = range;
	    dItem->index = index;

	    dItem->spans = TreeItem_GetSpans(tree, dItem->item);

	    /* Keep track of the maximum item size */
	    if (area->width > dInfo->itemWidth)
		dInfo->itemWidth = area->width;
	    if (dItem->height > dInfo->itemHeight)
		dInfo->itemHeight = dItem->height;

	    /* Linked list of DItems */
	    if (dInfo->dItem == NULL)
		dInfo->dItem = dItem;
	    else
		dInfo->dItemLast->next = dItem;
	    dInfo->dItemLast = dItem;

	    if (rItem == range->last)
		break;
	    rItem++;

	    /* Stop when out of bounds */
	    if (dItem->y + dItem->height >= maxY)
		break;
	}
    }

    /* Left-to-right */
    else {
	while (1) {
	    item = rItem->item;

	    /* Update item/style layout. This can be needed when using fixed
	     * column widths. */
	    (void) TreeItem_Height(tree, item);

	    TreeItem_ToIndex(tree, item, &index, &indexVis);
	    switch (tree->backgroundMode) {
#ifdef DEPRECATED
		case BG_MODE_INDEX:
#endif
		case BG_MODE_ORDER: break;
#ifdef DEPRECATED
		case BG_MODE_VISINDEX:
#endif
		case BG_MODE_ORDERVIS: index = indexVis; break;
		case BG_MODE_COLUMN: index = rItem->index; break;
		case BG_MODE_ROW: index = range->index; break;
	    }

	    x = C2Wx(range->offset.x + rItem->offset);

	    dItem = (DItem *) TreeItem_GetDInfo(tree, item);

	    /* Re-use a previously allocated DItem */
	    if (dItem != NULL) {
		dItemHead = DItem_Unlink(dItemHead, dItem);
		area = &dItem->area;

		/* This item is already marked for total redraw */
		if (area->flags & DITEM_ALL_DIRTY)
		    ; /* nothing */

		/* All display info is marked as invalid */
		else if (dInfo->flags & DINFO_INVALIDATE)
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		else if ((dItem->flags & DITEM_INVALIDATE_ON_SCROLL_X)
			&& (x != dItem->oldX))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		else if ((dItem->flags & DITEM_INVALIDATE_ON_SCROLL_Y)
			&& (y != dItem->oldY))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* The range may have changed size */
		else if ((area->width != rItem->size) ||
			(dItem->height != range->totalHeight))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* Items may have alternating background colors. */
		else if ((tree->columnBgCnt > 1) &&
			((index % tree->columnBgCnt) !=
				 (dItem->index % tree->columnBgCnt)))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* We don't copy items vertically to their new position,
		 * except for vertical scrolling which moves the whole range */
		else if (y - dItem->oldY != dInfo->yOrigin - tree->yOrigin)
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* If we are displaying dotted lines and the item has moved
		 * from odd-top to non-odd-top or vice versa, must redraw
		 * the lines for this item. */
		else if (tree->showLines &&
			(tree->lineStyle == LINE_STYLE_DOT) &&
			tree->columnTreeVis &&
			((DW2Cy(dItem->oldY) & 1) != (W2Cy(y) & 1)))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* We can't copy the item to its new position unless it
		 * has the same part of the background image behind it */
		else if ((tree->backgroundImage != NULL) &&
		    (dInfo->xOrigin != tree->xOrigin) &&
		    !(tree->bgImageScroll & BGIMG_SCROLL_X))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		else if ((tree->backgroundImage != NULL) &&
		    (dInfo->yOrigin != tree->yOrigin) &&
		    !(tree->bgImageScroll & BGIMG_SCROLL_Y))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		else if ((tree->backgroundImage != NULL) &&
			((DW2Cx(dItem->oldX) % bgImgWidth) !=
				(W2Cx(x) % bgImgWidth)))
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;
	    }

	    /* Make a new DItem */
	    else {
		dItem = DItem_Alloc(tree, rItem);
		area = &dItem->area;
	    }

	    area->x = x;
	    dItem->y = y;
	    area->width = rItem->size;
	    dItem->height = Range_TotalHeight(tree, range);
	    dItem->range = range;
	    dItem->index = index;

	    dItem->spans = TreeItem_GetSpans(tree, dItem->item);

	    /* Keep track of the maximum item size */
	    if (area->width > dInfo->itemWidth)
		dInfo->itemWidth = area->width;
	    if (dItem->height > dInfo->itemHeight)
		dInfo->itemHeight = dItem->height;

	    /* Linked list of DItems */
	    if (dInfo->dItem == NULL)
		dInfo->dItem = dItem;
	    else
		dInfo->dItemLast->next = dItem;
	    dInfo->dItemLast = dItem;

	    if (rItem == range->last)
		break;
	    rItem++;

	    /* Stop when out of bounds */
	    if (area->x + area->width >= maxX)
		break;
	}
    }

    return dItemHead;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_UpdateDInfo --
 *
 *	At the finish of this procedure every on-screen item will have
 *	a DItem associated with it and no off-screen item will have
 *	a DItem.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *--------------------------------------------------------------
 */

static void
Tree_UpdateDInfo(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DItem *dItemHead = dInfo->dItem;
    int x, y, rx = 0, ry = 0, ix, iy, dx, dy;
    int minX, minY, maxX, maxY;
    Range *range;
    RItem *rItem;
    DItem *dItem;

    if (tree->debug.enable && tree->debug.display)
	dbwin("Tree_UpdateDInfo %s\n", Tk_PathName(tree->tkwin));

    dInfo->dItem = dInfo->dItemLast = NULL;
    dInfo->rangeFirstD = dInfo->rangeLastD = NULL;
    dInfo->itemWidth = dInfo->itemHeight = 0;

    dInfo->empty = !Tree_AreaBbox(tree, TREE_AREA_CONTENT, &dInfo->bounds);
    dInfo->emptyL = !Tree_AreaBbox(tree, TREE_AREA_LEFT, &dInfo->boundsL);
    dInfo->emptyR = !Tree_AreaBbox(tree, TREE_AREA_RIGHT, &dInfo->boundsR);

    if (dInfo->empty)
	goto done;

    TreeRect_XYXY(dInfo->bounds, &minX, &minY, &maxX, &maxY);

    range = dInfo->rangeFirst;
    if (tree->vertical) {
	/* Find the first range which could be onscreen horizontally.
	 * It may not be onscreen if it has less height than other ranges. */
	while (range != NULL) {
	    if ((range->offset.x < W2Cx(maxX)) &&
		    (range->offset.x + range->totalWidth > W2Cx(minX))) {
		rx = range->offset.x;
		ry = range->offset.y;
		break;
	    }
	    range = range->next;
	}
    }
    else {
	/* Find the first range which could be onscreen vertically.
	 * It may not be onscreen if it has less width than other ranges. */
	while (range != NULL) {
	    if ((range->offset.y < W2Cy(maxY)) &&
		    (range->offset.y + range->totalHeight > W2Cy(minY))) {
		rx = range->offset.x;
		ry = range->offset.y;
		break;
	    }
	    range = range->next;
	}
    }

    while (range != NULL) {
	rx = range->offset.x;
	ry = range->offset.y;
	if (tree->vertical) {
	    if (rx >= W2Cx(maxX))
		break;
	}
	else {
	    if (ry >= W2Cy(maxY))
		break;
	}
	if ((rx + range->totalWidth > W2Cx(minX)) &&
		(ry + range->totalHeight > W2Cy(minY))) {
	    dx = MAX(W2Cx(minX) - rx, 0);
	    dy = MAX(W2Cy(minY) - ry, 0);

	    rItem = Range_ItemUnderPoint(tree, range, dx, dy, &ix, &iy, 3);

	    /* Window coords of top-left of item */
	    x = C2Wx(rx) + dx - ix;
	    y = C2Wy(ry) + dy - iy;

	    dItemHead = UpdateDInfoForRange(tree, dItemHead, range, rItem, x, y);
	}

	/* Track this range even if it has no DItems, so whitespace is
	 * erased */
	if (dInfo->rangeFirstD == NULL)
	    dInfo->rangeFirstD = range;
	dInfo->rangeLastD = range;

	range = range->next;
    }

    if (dInfo->dItemLast != NULL)
	dInfo->dItemLast->next = NULL;
done:

    if (dInfo->dItem != NULL)
	goto skipLock;
    if (!tree->itemVisCount)
	goto skipLock;
    if (dInfo->emptyL && dInfo->emptyR)
	goto skipLock;
    range = dInfo->rangeFirst;
    if ((range != NULL) && !range->totalHeight)
	goto skipLock;

    {
	int y = W2Cy(Tree_ContentTop(tree));
	int index, indexVis;

	/* If no non-locked columns are displayed, we have no Range and
	 * must use dInfo->rangeLock. */
	if (range == NULL) {
	    range = dInfo->rangeLock;
	}

	/* Find the first item on-screen vertically. */
	rItem = Range_ItemUnderPoint(tree, range, -666, y, NULL, &y, 3);

	y = C2Wy(range->offset.y + rItem->offset);

	while (1) {
	    DItem *dItem = (DItem *) TreeItem_GetDInfo(tree, rItem->item);

	    /* Re-use a previously allocated DItem */
	    if (dItem != NULL) {
		dItemHead = DItem_Unlink(dItemHead, dItem);
	    } else {
		dItem = DItem_Alloc(tree, rItem);
	    }

	    TreeItem_ToIndex(tree, rItem->item, &index, &indexVis);
	    switch (tree->backgroundMode) {
#ifdef DEPRECATED
		case BG_MODE_INDEX:
#endif
		case BG_MODE_ORDER: break;
#ifdef DEPRECATED
		case BG_MODE_VISINDEX:
#endif
		case BG_MODE_ORDERVIS: index = indexVis; break;
		case BG_MODE_COLUMN: index = range->index; break;
		case BG_MODE_ROW: index = rItem->index; break;
	    }

	    dItem->y = C2Wy(range->offset.y + rItem->offset); /* Canvas -> Window */
	    dItem->height = rItem->size;
	    dItem->range = range;
	    dItem->index = index;

	    dItem->spans = TreeItem_GetSpans(tree, dItem->item);

	    /* Keep track of the maximum item size */
	    if (dItem->height > dInfo->itemHeight)
		dInfo->itemHeight = dItem->height;

	    /* Linked list of DItems */
	    if (dInfo->dItem == NULL)
		dInfo->dItem = dItem;
	    else
		dInfo->dItemLast->next = dItem;
	    dInfo->dItemLast = dItem;

	    if (rItem == range->last)
		break;
	    if (dItem->y + dItem->height >= Tree_ContentBottom(tree))
		break;
	    rItem++;
	}
    }
skipLock:
    if (!dInfo->emptyL || !dInfo->emptyR) {
	int bgImgWidth, bgImgHeight;
	DItemArea *area;

	if (!dInfo->emptyL) {
	    /* Keep track of the maximum item size */
	    if (dInfo->widthOfColumnsLeft > dInfo->itemWidth)
		dInfo->itemWidth = dInfo->widthOfColumnsLeft;
	}
	if (!dInfo->emptyR) {
	    /* Keep track of the maximum item size */
	    if (dInfo->widthOfColumnsRight > dInfo->itemWidth)
		dInfo->itemWidth = dInfo->widthOfColumnsRight;
	}

	if (tree->backgroundImage != NULL)
	    Tk_SizeOfImage(tree->backgroundImage, &bgImgWidth, &bgImgHeight);

	for (dItem = dInfo->dItem; dItem != NULL; dItem = dItem->next) {

	    if (!dInfo->emptyL) {

		area = &dItem->left;
		area->x = Tree_BorderLeft(tree);
		area->width = dInfo->widthOfColumnsLeft;

		/* This item is already marked for total redraw */
		if (area->flags & DITEM_ALL_DIRTY) {
		    ; /* nothing */

		/* All display info is marked as invalid */
		} else if (dInfo->flags & DINFO_INVALIDATE) {
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* Items may have alternating background colors. */
		} else if ((tree->columnBgCnt > 1) &&
			((dItem->oldIndex % tree->columnBgCnt) !=
				(dItem->index % tree->columnBgCnt))) {
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* If we are displaying dotted lines and the item has moved
		 * from odd-top to non-odd-top or vice versa, must redraw
		 * the lines for this item. */
		} else if (tree->showLines &&
			(tree->lineStyle == LINE_STYLE_DOT) &&
			tree->columnTreeVis &&
			(TreeColumn_Lock(tree->columnTree) == COLUMN_LOCK_LEFT) &&
			((DW2Cy(dItem->oldY) & 1) != (W2Cy(dItem->y) & 1))) {
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* We can't copy the item to its new position unless it
		* has the same part of the background image behind it */
		} else if ((tree->backgroundImage != NULL) &&
			((dInfo->xOrigin % bgImgWidth) !=
				(tree->xOrigin % bgImgWidth))) {
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;
		}
	    }

	    if (!dInfo->emptyR) {

		area = &dItem->right;
		area->x = Tree_ContentRight(tree);
		area->width = dInfo->widthOfColumnsRight;

		/* This item is already marked for total redraw */
		if (area->flags & DITEM_ALL_DIRTY) {
		    ; /* nothing */

		/* All display info is marked as invalid */
		} else if (dInfo->flags & DINFO_INVALIDATE) {
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* Items may have alternating background colors. */
		} else if ((tree->columnBgCnt > 1) &&
			((dItem->oldIndex % tree->columnBgCnt) !=
				(dItem->index % tree->columnBgCnt))) {
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* If we are displaying dotted lines and the item has moved
		 * from odd-top to non-odd-top or vice versa, must redraw
		 * the lines for this item. */
		} else if (tree->showLines &&
			(tree->lineStyle == LINE_STYLE_DOT) &&
			tree->columnTreeVis &&
			(TreeColumn_Lock(tree->columnTree) == COLUMN_LOCK_RIGHT) &&
			((DW2Cy(dItem->oldY) & 1) != (W2Cy(dItem->y) & 1))) {
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* We can't copy the item to its new position unless it
		* has the same part of the background image behind it */
		} else if ((tree->backgroundImage != NULL) &&
			((dInfo->xOrigin % bgImgWidth) !=
				(tree->xOrigin % bgImgWidth))) {
		    area->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;
		}
	    }
	}
    }

    while (dItemHead != NULL)
	dItemHead = DItem_Free(tree, dItemHead);

    dInfo->flags &= ~DINFO_INVALIDATE;
}

/*
 *--------------------------------------------------------------
 *
 * InvalidateWhitespace --
 *
 *	Subtract a rectangular area from the current whitespace
 *	region.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
InvalidateWhitespace(
    TreeCtrl *tree,		/* Widget info. */
    int x1, int y1,		/* Window coords to invalidate. */
    int x2, int y2)		/* Window coords to invalidate. */
{
    TreeDInfo dInfo = tree->dInfo;

    if ((x1 < x2 && y1 < y2) && TkRectInRegion(dInfo->wsRgn, x1, y1,
	    x2 - x1, y2 - y1)) {
	XRectangle rect;
	TkRegion rgn = Tree_GetRegion(tree);

	rect.x = x1;
	rect.y = y1;
	rect.width = x2 - x1;
	rect.height = y2 - y1;
	TkUnionRectWithRegion(&rect, rgn, rgn);
	TkSubtractRegion(dInfo->wsRgn, rgn, dInfo->wsRgn);
	Tree_FreeRegion(tree, rgn);
    }
}

/*
 *--------------------------------------------------------------
 *
 * InvalidateDItemX --
 *
 *	Mark a horizontal span of a DItem as dirty (needing to be
 *	redrawn). The caller must set the DITEM_DIRTY flag afterwards.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
InvalidateDItemX(
    DItem *dItem,		/* Item to mark dirty. */
    DItemArea *area,
    int itemX,			/* x-coordinate of item. */
    int dirtyX,			/* Left edge of area to mark as dirty. */
    int dirtyWidth		/* Width of area to mark as dirty. */
    )
{
    int x1, x2;

    if (dirtyX <= itemX)
	area->dirty[LEFT] = 0;
    else {
	x1 = dirtyX - itemX;
	if (!(area->flags & DITEM_DIRTY) || (x1 < area->dirty[LEFT]))
	    area->dirty[LEFT] = x1;
    }

    if (dirtyX + dirtyWidth >= itemX + area->width)
	area->dirty[RIGHT] = area->width;
    else {
	x2 = dirtyX + dirtyWidth - itemX;
	if (!(area->flags & DITEM_DIRTY) || (x2 > area->dirty[RIGHT]))
	    area->dirty[RIGHT] = x2;
    }
}

/*
 *--------------------------------------------------------------
 *
 * InvalidateDItemY --
 *
 *	Mark a vertical span of a DItem as dirty (needing to be
 *	redrawn). The caller must set the DITEM_DIRTY flag afterwards.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
InvalidateDItemY(
    DItem *dItem,		/* Item to mark dirty. */
    DItemArea *area,
    int itemY,			/* y-coordinate of item. */
    int dirtyY,			/* Top edge of area to mark as dirty. */
    int dirtyHeight		/* Height of area to mark as dirty. */
    )
{
    int y1, y2;

    if (dirtyY <= itemY)
	area->dirty[TOP] = 0;
    else {
	y1 = dirtyY - itemY;
	if (!(area->flags & DITEM_DIRTY) || (y1 < area->dirty[TOP]))
	    area->dirty[TOP] = y1;
    }

    if (dirtyY + dirtyHeight >= itemY + dItem->height)
	area->dirty[BOTTOM] = dItem->height;
    else {
	y2 = dirtyY + dirtyHeight - itemY;
	if (!(area->flags & DITEM_DIRTY) || (y2 > area->dirty[BOTTOM]))
	    area->dirty[BOTTOM] = y2;
    }
}

/*
 *--------------------------------------------------------------
 *
 * Range_RedoIfNeeded --
 *
 *	Recalculate the list of Ranges if they are marked out-of-date.
 *	Also calculate the height and width of the canvas based on the
 *	list of Ranges.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *--------------------------------------------------------------
 */

static void
Range_RedoIfNeeded(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;

    CheckPendingHeaderUpdate(tree);

    if (dInfo->flags & DINFO_REDO_RANGES) {
	dInfo->rangeFirstD = dInfo->rangeLastD = NULL;
	dInfo->flags |= DINFO_OUT_OF_DATE;
	Range_Redo(tree);
	dInfo->flags &= ~DINFO_REDO_RANGES;

#ifdef COMPLEX_WHITESPACE
	if (ComplexWhitespace(tree)) {
	    dInfo->flags |= DINFO_DRAW_WHITESPACE;
	}
#endif
#if COLUMNGRID == 1
	if (GridLinesInWhiteSpace(tree)) {
	    dInfo->flags |= DINFO_DRAW_WHITESPACE;
	}
#endif

	/* Do this after clearing REDO_RANGES to prevent infinite loop */
	tree->totalWidth = tree->totalHeight = -1;
	(void) Tree_CanvasWidth(tree);
	(void) Tree_CanvasHeight(tree);
	dInfo->flags |= DINFO_REDO_INCREMENTS;
    }
}

/*
 *--------------------------------------------------------------
 *
 * DblBufWinDirty --
 *
 *	Add a rectangle to the dirty region of the "-doublebuffer window"
 *	pixmap.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
DblBufWinDirty(
    TreeCtrl *tree,
    int x1,
    int y1,
    int x2,
    int y2
    )
{
    TreeDInfo dInfo = tree->dInfo;
    XRectangle rect;

    /* Fix BUG ID: 3015429 */
    if (x1 >= x2 || y1 >= y2)
	return;

    rect.x = x1;
    rect.y = y1;
    rect.width = x2 - x1;
    rect.height = y2 - y1;
    TkUnionRectWithRegion(&rect, dInfo->dirtyRgn, dInfo->dirtyRgn);
}

#if REDRAW_RGN == 1
static void
AddRgnToRedrawRgn(
    TreeCtrl *tree,
    TkRegion rgn
    )
{
    TreeDInfo dInfo = tree->dInfo;

    Tree_UnionRegion(dInfo->redrawRgn, rgn, dInfo->redrawRgn);
}

static void
AddRectToRedrawRgn(
    TreeCtrl *tree,
    int minX,
    int minY,
    int maxX,
    int maxY
    )
{
    TkRegion rgn = Tree_GetRegion(tree);
    TreeRectangle rect;

    rect.x = minX;
    rect.y = minY;
    rect.width = maxX - minX;
    rect.height = maxY - minY;
    Tree_SetRectRegion(rgn, &rect);

    AddRgnToRedrawRgn(tree, rgn);

    Tree_FreeRegion(tree, rgn);
}
#endif /* REDRAW_RGN */

/*
 *--------------------------------------------------------------
 *
 * DItemAllDirty --
 *
 *	Determine if a DItem will be entirely redrawn in all columns.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
DItemAllDirty(
    TreeCtrl *tree,
    DItem *dItem
    )
{
    if ((dItem->area.flags & DITEM_DRAWN) &&
	    !(dItem->area.flags & DITEM_ALL_DIRTY))
	return 0;
    if ((dItem->left.flags & DITEM_DRAWN) &&
	    !(dItem->left.flags & DITEM_ALL_DIRTY))
	return 0;
    if ((dItem->right.flags & DITEM_DRAWN) &&
	    !(dItem->right.flags & DITEM_ALL_DIRTY))
	return 0;
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * ScrollVerticalComplex --
 *
 *	Perform scrolling by copying the pixels of items from the
 *	previous display position to the current position. Any areas
 *	of items copied over by the moved items are marked dirty.
 *	This is called when -orient=vertical.
 *
 * Results:
 *	The number of items whose pixels were copied.
 *
 * Side effects:
 *	Pixels are copied in the TreeCtrl window or in the
 *	offscreen pixmap (if double-buffering is used).
 *
 *--------------------------------------------------------------
 */

static int
ScrollVerticalComplex(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DItem *dItem, *dItem2;
    Range *range;
    TkRegion damageRgn;
    int minX, minY, maxX, maxY;
    int oldX, oldY, width, height, offset;
    int y;
    int numCopy = 0;

    if (dInfo->empty && dInfo->emptyL && dInfo->emptyR)
	return 0;

    minX = Tree_BorderLeft(tree);
    minY = Tree_ContentTop(tree);
    maxX = Tree_BorderRight(tree);
    maxY = Tree_ContentBottom(tree);

    /* Try updating the display by copying items on the screen to their
     * new location */
    for (dItem = dInfo->dItem;
	 dItem != NULL;
	 dItem = dItem->next) {
	/* Copy an item to its new location unless:
	 * (a) item display info is invalid
	 * (b) item is in same location as last draw */
	if (DItemAllDirty(tree, dItem) ||
		(dItem->oldY == dItem->y))
	    continue;

	numCopy++;

	range = dItem->range;

	/* This item was previously displayed so it only needs to be
	 * copied to the new location. Copy all such items as one */
	offset = dItem->y - dItem->oldY;
	height = dItem->height;
	for (dItem2 = dItem->next;
	     dItem2 != NULL;
	     dItem2 = dItem2->next) {
	    if ((dItem2->range != range) ||
		    DItemAllDirty(tree, dItem2) ||
		    (dItem2->oldY + offset != dItem2->y))
		break;
	    numCopy++;
	    height = dItem2->y + dItem2->height - dItem->y;
	}

	y = dItem->y;
	oldY = dItem->oldY;

	/* Don't copy part of the window border */
	if (oldY < minY) {
	    height -= minY - oldY;
	    oldY = minY;
	}
	if (oldY + height > maxY)
	    height = maxY - oldY;

	/* Don't copy over the window border */
	if (oldY + offset < minY) {
	    height -= minY - (oldY + offset);
	    oldY += minY - (oldY + offset);
	}
	if (oldY + offset + height > maxY)
	    height = maxY - (oldY + offset);

	if (!dInfo->emptyL || !dInfo->emptyR) {
	    oldX = minX;
	    width = maxX - minX;
	} else {
	    oldX = dItem->oldX;
	    width = dItem->area.width;
	}

	if (oldX < minX) {
	    width -= minX - oldX;
	    oldX = minX;
	}
	if (oldX + width > maxX)
	    width = maxX - oldX;

	/* Update oldY of copied items */
	while (1) {
	    /* If an item was partially visible, invalidate the exposed area */
	    if ((dItem->oldY < minY) && (offset > 0)) {
		if (!dInfo->empty && dInfo->rangeFirstD != NULL) {
		    InvalidateDItemX(dItem, &dItem->area, dItem->oldX, oldX, width);
		    InvalidateDItemY(dItem, &dItem->area, dItem->oldY, dItem->oldY, minY - dItem->oldY);
		    dItem->area.flags |= DITEM_DIRTY;
		}
		if (!dInfo->emptyL) {
		    InvalidateDItemX(dItem, &dItem->left, dItem->left.x, oldX, width);
		    InvalidateDItemY(dItem, &dItem->left, dItem->oldY, dItem->oldY, minY - dItem->oldY);
		    dItem->left.flags |= DITEM_DIRTY;
		}
		if (!dInfo->emptyR) {
		    InvalidateDItemX(dItem, &dItem->right, dItem->right.x, oldX, width);
		    InvalidateDItemY(dItem, &dItem->right, dItem->oldY, dItem->oldY, minY - dItem->oldY);
		    dItem->right.flags |= DITEM_DIRTY;
		}
	    }
	    if ((dItem->oldY + dItem->height > maxY) && (offset < 0)) {
		if (!dInfo->empty && dInfo->rangeFirstD != NULL) {
		    InvalidateDItemX(dItem, &dItem->area, dItem->oldX, oldX, width);
		    InvalidateDItemY(dItem, &dItem->area, dItem->oldY, maxY, maxY - dItem->oldY + dItem->height);
		    dItem->area.flags |= DITEM_DIRTY;
		}
		if (!dInfo->emptyL) {
		    InvalidateDItemX(dItem, &dItem->left, dItem->left.x, oldX, width);
		    InvalidateDItemY(dItem, &dItem->left, dItem->oldY, maxY, maxY - dItem->oldY + dItem->height);
		    dItem->left.flags |= DITEM_DIRTY;
		}
		if (!dInfo->emptyR) {
		    InvalidateDItemX(dItem, &dItem->right, dItem->right.x, oldX, width);
		    InvalidateDItemY(dItem, &dItem->right, dItem->oldY, maxY, maxY - dItem->oldY + dItem->height);
		    dItem->right.flags |= DITEM_DIRTY;
		}
	    }
	    dItem->oldY = dItem->y;
	    if (dItem->next == dItem2)
		break;
	    dItem = dItem->next;
	}

	/* Invalidate parts of items being copied over */
	for ( ; dItem2 != NULL; dItem2 = dItem2->next) {
	    if (dItem2->range != range)
		break;
	    if (!DItemAllDirty(tree, dItem2) &&
		    (dItem2->oldY + dItem2->height > y) &&
		    (dItem2->oldY < y + height)) {
		if (!dInfo->empty && dInfo->rangeFirstD != NULL) {
		    InvalidateDItemX(dItem2, &dItem2->area, dItem2->oldX, oldX, width);
		    InvalidateDItemY(dItem2, &dItem2->area, dItem2->oldY, y, height);
		    dItem2->area.flags |= DITEM_DIRTY;
		}
		if (!dInfo->emptyL) {
		    InvalidateDItemX(dItem2, &dItem2->left, dItem2->left.x, oldX, width);
		    InvalidateDItemY(dItem2, &dItem2->left, dItem2->oldY, y, height);
		    dItem2->left.flags |= DITEM_DIRTY;
		}
		if (!dInfo->emptyR) {
		    InvalidateDItemX(dItem2, &dItem2->right, dItem2->right.x, oldX, width);
		    InvalidateDItemY(dItem2, &dItem2->right, dItem2->oldY, y, height);
		    dItem2->right.flags |= DITEM_DIRTY;
		}
	    }
	}

	if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	    int dirtyMin, dirtyMax;
	    XCopyArea(tree->display, dInfo->pixmapW.drawable,
		    dInfo->pixmapW.drawable, tree->copyGC,
		    oldX, oldY, width, height,
		    oldX, oldY + offset);
	    if (offset < 0) {
		dirtyMin = oldY + offset + height;
		dirtyMax = oldY + height;
	    } else {
		dirtyMin = oldY;
		dirtyMax = oldY + offset;
	    }
	    Tree_InvalidateArea(tree, oldX, dirtyMin, oldX + width, dirtyMax);
	    DblBufWinDirty(tree, oldX, oldY + offset,
		    oldX + width, oldY + offset + height);
	    continue;
	}

	/* Copy */
	damageRgn = Tree_GetRegion(tree);
	if (Tree_ScrollWindow(tree, dInfo->scrollGC,
		    oldX, oldY, width, height, 0, offset, damageRgn)) {
	    DisplayDelay(tree);
	    Tree_InvalidateRegion(tree, damageRgn);
	}
	Tree_FreeRegion(tree, damageRgn);
    }
    return numCopy;
}

/*
 *--------------------------------------------------------------
 *
 * ScrollHorizontalSimple --
 *
 *	Perform scrolling by shifting the pixels in the content
 *	area of the list to the left or right.
 *	This is called when -orient=vertical.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is copied/scrolled in the TreeCtrl window or in the
 *	offscreen pixmap (if double-buffering is used).
 *
 *--------------------------------------------------------------
 */

static void
ScrollHorizontalSimple(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DItem *dItem;
    TkRegion damageRgn;
    int minX, minY, maxX, maxY;
    int width, offset;
    int x, y;
    int dirtyMin, dirtyMax;

    if (dInfo->xOrigin == tree->xOrigin)
	return;

    /* Only column headers are visible. */
    if (dInfo->rangeFirst == NULL)
	return;

    if (dInfo->empty)
	return;

    TreeRect_XYXY(dInfo->bounds, &minX, &minY, &maxX, &maxY);

    /* Update oldX */
    for (dItem = dInfo->dItem;
	    dItem != NULL;
	    dItem = dItem->next) {
	dItem->oldX = dItem->area.x;
    }

    offset = dInfo->xOrigin - tree->xOrigin;

    /* We only scroll the content, not the whitespace */
    y = C2Wy(Tree_CanvasHeight(tree));
    if (y < maxY)
	maxY = y;

    /* Simplify if a whole screen was scrolled. */
    if (abs(offset) >= maxX - minX) {
	Tree_InvalidateArea(tree, minX, minY, maxX, maxY);
	return;
    }

    /* We only scroll the content, not the whitespace */
    x = C2Wx(Tree_CanvasWidth(tree));
    if (x < maxX)
	maxX = x;

    width = maxX - minX - abs(offset);

    /* Move pixels right */
    if (offset > 0) {
	x = minX;
	dirtyMin = minX;
	dirtyMax = maxX - width;

    /* Move pixels left */
    } else {
	x = maxX - width;
	dirtyMin = minX + width;
	dirtyMax = maxX;
    }

    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	XCopyArea(tree->display, dInfo->pixmapW.drawable,
		dInfo->pixmapW.drawable,
		tree->copyGC,
		x, minY, width, maxY - minY,
		x + offset, minY);
    } else {
	damageRgn = Tree_GetRegion(tree);
	if (Tree_ScrollWindow(tree, dInfo->scrollGC,
		    x, minY, width, maxY - minY, offset, 0, damageRgn)) {
	    DisplayDelay(tree);
	    Tree_InvalidateRegion(tree, damageRgn);
	}
	Tree_FreeRegion(tree, damageRgn);
    }
    Tree_InvalidateArea(tree, dirtyMin, minY, dirtyMax, maxY);

    /* Invalidate the part of the whitespace that the content was copied
     * over. */
    {
	TkRegion rgn;

	rgn = Tree_GetRectRegion(tree, &dInfo->bounds);
	TkSubtractRegion(rgn, dInfo->wsRgn, rgn);
	Tree_OffsetRegion(rgn, offset, 0);
	TkSubtractRegion(dInfo->wsRgn, rgn, dInfo->wsRgn);
	Tree_FreeRegion(tree, rgn);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ScrollVerticalSimple --
 *
 *	Perform scrolling by shifting the pixels in the content area of
 *	the list up or down.  This is called when -orient=horizontal.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is copied/scrolled in the TreeCtrl window or in the
 *	offscreen pixmap (if double-buffering is used).
 *
 *--------------------------------------------------------------
 */

static void
ScrollVerticalSimple(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DItem *dItem;
    TkRegion damageRgn;
    int minX, minY, maxX, maxY;
    int height, offset;
    int x, y;
    int dirtyMin, dirtyMax;

    if (dInfo->yOrigin == tree->yOrigin)
	return;

    /* Update oldY */
    for (dItem = dInfo->dItem;
	    dItem != NULL;
	    dItem = dItem->next) {
	dItem->oldY = dItem->y;
    }

    if (dInfo->empty)
	return;

    TreeRect_XYXY(dInfo->bounds, &minX, &minY, &maxX, &maxY);

    offset = dInfo->yOrigin - tree->yOrigin;

    /* Scroll the items, not the whitespace to the right */
    x = C2Wx(Tree_CanvasWidth(tree));
    if (x < maxX)
	maxX = x;

    /* Simplify if a whole screen was scrolled. */
    if (abs(offset) > maxY - minY) {
	Tree_InvalidateArea(tree, minX, minY, maxX, maxY);
	return;
    }

    height = maxY - minY - abs(offset);

    /* Move pixels down */
    if (offset > 0) {
	y = minY;
	dirtyMin = minY;
	dirtyMax = maxY - height;

    /* Move pixels up */
    } else {
	y = maxY - height;
	dirtyMin = minY + height;
	dirtyMax = maxY;
    }

    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	XCopyArea(tree->display, dInfo->pixmapW.drawable,
		dInfo->pixmapW.drawable, tree->copyGC,
		minX, y, maxX - minX, height,
		minX, y + offset);
    } else {
	damageRgn = Tree_GetRegion(tree);
	if (Tree_ScrollWindow(tree, dInfo->scrollGC,
		    minX, y, maxX - minX, height, 0, offset, damageRgn)) {
	    DisplayDelay(tree);
	    Tree_InvalidateRegion(tree, damageRgn);
	}
	Tree_FreeRegion(tree, damageRgn);
    }
    Tree_InvalidateArea(tree, minX, dirtyMin, maxX, dirtyMax);

    /* Invalidate the part of the whitespace that the content was copied
     * over. */
    {
	TkRegion rgn;

	rgn = Tree_GetRectRegion(tree, &dInfo->bounds);
	TkSubtractRegion(rgn, dInfo->wsRgn, rgn);
	Tree_OffsetRegion(rgn, 0, offset);
	TkSubtractRegion(dInfo->wsRgn, rgn, dInfo->wsRgn);
	Tree_FreeRegion(tree, rgn);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ScrollHorizontalComplex --
 *
 *	Perform scrolling by copying the pixels of items from the
 *	previous display position to the current position. Any areas
 *	of items copied over by the moved items are marked dirty.
 *	This is called when -orient=horizontal.
 *
 * Results:
 *	The number of items whose pixels were copied.
 *
 * Side effects:
 *	Pixels are copied in the TreeCtrl window or in the
 *	offscreen pixmap (if double-buffering is used).
 *
 *--------------------------------------------------------------
 */

static int
ScrollHorizontalComplex(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DItem *dItem, *dItem2;
    Range *range;
    TkRegion damageRgn;
    TreeRectangle tr;
    int minX, minY, maxX, maxY;
    int oldX, oldY, width, height, offset;
    int x;
    int numCopy = 0;

    if (!Tree_AreaBbox(tree, TREE_AREA_CONTENT, &tr))
	return 0;
    TreeRect_XYXY(tr, &minX, &minY, &maxX, &maxY);

    /* Try updating the display by copying items on the screen to their
     * new location */
    for (dItem = dInfo->dItem;
	 dItem != NULL;
	 dItem = dItem->next) {
	/* Copy an item to its new location unless:
	 * (a) item display info is invalid
	 * (b) item is in same location as last draw */
	if ((dItem->area.flags & DITEM_ALL_DIRTY) ||
		(dItem->oldX == dItem->area.x))
	    continue;

	numCopy++;

	range = dItem->range;

	/* This item was previously displayed so it only needs to be
	 * copied to the new location. Copy all such items as one */
	offset = dItem->area.x - dItem->oldX;
	width = dItem->area.width;
	for (dItem2 = dItem->next;
	     dItem2 != NULL;
	     dItem2 = dItem2->next) {
	    if ((dItem2->range != range) ||
		    (dItem2->area.flags & DITEM_ALL_DIRTY) ||
		    (dItem2->oldX + offset != dItem2->area.x))
		break;
	    numCopy++;
	    width = dItem2->area.x + dItem2->area.width - dItem->area.x;
	}

	x = dItem->area.x;
	oldX = dItem->oldX;

	/* Don't copy part of the window border */
	if (oldX < minX) {
	    width -= minX - oldX;
	    oldX = minX;
	}
	if (oldX + width > maxX)
	    width = maxX - oldX;

	/* Don't copy over the window border */
	if (oldX + offset < minX) {
	    width -= minX - (oldX + offset);
	    oldX += minX - (oldX + offset);
	}
	if (oldX + offset + width > maxX)
	    width = maxX - (oldX + offset);

	oldY = dItem->oldY;
	height = dItem->height; /* range->totalHeight */
	if (oldY < minY) {
	    height -= minY - oldY;
	    oldY = minY;
	}
	if (oldY + height > maxY)
	    height = maxY - oldY;

	/* Update oldX of copied items */
	while (1) {
	    /* If an item was partially visible, invalidate the exposed area */
	    if ((dItem->oldX < minX) && (offset > 0)) {
		InvalidateDItemX(dItem, &dItem->area, dItem->oldX, dItem->oldX, minX - dItem->oldX);
		InvalidateDItemY(dItem, &dItem->area, oldY, oldY, height);
		dItem->area.flags |= DITEM_DIRTY;
	    }
	    if ((dItem->oldX + dItem->area.width > maxX) && (offset < 0)) {
		InvalidateDItemX(dItem, &dItem->area, dItem->oldX, maxX, maxX - dItem->oldX + dItem->area.width);
		InvalidateDItemY(dItem, &dItem->area, oldY, oldY, height);
		dItem->area.flags |= DITEM_DIRTY;
	    }
	    dItem->oldX = dItem->area.x;
	    if (dItem->next == dItem2)
		break;
	    dItem = dItem->next;
	}

	/* Invalidate parts of items being copied over */
	for ( ; dItem2 != NULL; dItem2 = dItem2->next) {
	    if (dItem2->range != range)
		break;
	    if (!(dItem2->area.flags & DITEM_ALL_DIRTY) &&
		    (dItem2->oldX + dItem2->area.width > x) &&
		    (dItem2->oldX < x + width)) {
		InvalidateDItemX(dItem2, &dItem2->area, dItem2->oldX, x, width);
		InvalidateDItemY(dItem2, &dItem2->area, oldY, oldY, height);
		dItem2->area.flags |= DITEM_DIRTY;
	    }
	}

	if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	    int dirtyMin, dirtyMax;
	    XCopyArea(tree->display, dInfo->pixmapW.drawable,
		    dInfo->pixmapW.drawable, tree->copyGC,
		    oldX, oldY, width, height,
		    oldX + offset, oldY);
	    if (offset < 0) {
		dirtyMin = oldX + offset + width;
		dirtyMax = oldX + width;
	    } else {
		dirtyMin = oldX;
		dirtyMax = oldX + offset;
	    }
	    Tree_InvalidateArea(tree, dirtyMin, oldY, dirtyMax, oldY + height);
	    DblBufWinDirty(tree, oldX + offset, oldY, oldX + offset + width,
		    oldY + height);
	    continue;
	}

	/* Copy */
	damageRgn = Tree_GetRegion(tree);
	if (Tree_ScrollWindow(tree, dInfo->scrollGC,
		    oldX, oldY, width, height, offset, 0, damageRgn)) {
	    DisplayDelay(tree);
	    Tree_InvalidateRegion(tree, damageRgn);
	}
	Tree_FreeRegion(tree, damageRgn);
    }
    return numCopy;
}

/*
 *----------------------------------------------------------------------
 *
 * Proxy_IsXOR --
 *
 *	Return true if the column/row proxies should be drawn with XOR.
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
Proxy_IsXOR(void)
{
#if defined(WIN32)
    return FALSE; /* TRUE on XP, FALSE on Win7 (lots of flickering) */
#elif defined(MAC_OSX_TK)
    return FALSE; /* Cocoa doesn't have XOR */
#else
    return TRUE; /* X11 */
#endif
}

/*
 *--------------------------------------------------------------
 *
 * Proxy_DrawXOR --
 *
 *	Draw (or erase) the visual indicator used when the user is
 *	resizing a column or row (and -columnresizemode is "proxy").
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in the TreeCtrl window (or erased, since this
 *	is XOR drawing).
 *
 *--------------------------------------------------------------
 */

static void
Proxy_DrawXOR(
    TreeCtrl *tree,		/* Widget info. */
    int x1,			/* Vertical or horizontal line window coords. */
    int y1,
    int x2,
    int y2
    )
{
    XGCValues gcValues;
    unsigned long gcMask;
    GC gc;

#if defined(MAC_OSX_TK)
    gcValues.function = GXcopy;
#else
    gcValues.function = GXinvert;
#endif
    gcValues.graphics_exposures = False;
    gcMask = GCFunction | GCGraphicsExposures;
    gc = Tree_GetGC(tree, gcMask, &gcValues);

#if defined(WIN32)
    /* GXinvert doesn't work with XFillRectangle() on Win32 */
    XDrawLine(tree->display, Tk_WindowId(tree->tkwin), gc, x1, y1, x2, y2);
#else
    XFillRectangle(tree->display, Tk_WindowId(tree->tkwin), gc,
	    x1, y1, MAX(x2 - x1, 1), MAX(y2 - y1, 1));
#endif
}

/*
 *--------------------------------------------------------------
 *
 * TreeColumnProxy_Display --
 *
 *	Display the visual indicator used when the user is
 *	resizing a column (if it isn't displayed and should be
 *	displayed).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in the TreeCtrl window.
 *
 *--------------------------------------------------------------
 */

void
TreeColumnProxy_Display(
    TreeCtrl *tree		/* Widget info. */
    )
{
    if (!tree->columnProxy.onScreen && (tree->columnProxy.xObj != NULL)) {
	tree->columnProxy.sx = tree->columnProxy.x;
	if (Proxy_IsXOR()) {
	    Proxy_DrawXOR(tree, tree->columnProxy.x, Tree_BorderTop(tree),
		    tree->columnProxy.x, Tree_BorderBottom(tree));
	} else {
	    Tree_EventuallyRedraw(tree);
	}
	tree->columnProxy.onScreen = TRUE;
    }
}

/*
 *--------------------------------------------------------------
 *
 * TreeColumnProxy_Undisplay --
 *
 *	Hide the visual indicator used when the user is
 *	resizing a column (if it is displayed).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is erased in the TreeCtrl window.
 *
 *--------------------------------------------------------------
 */

void
TreeColumnProxy_Undisplay(
    TreeCtrl *tree		/* Widget info. */
    )
{
    if (tree->columnProxy.onScreen) {
	if (Proxy_IsXOR()) {
	    Proxy_DrawXOR(tree, tree->columnProxy.sx, Tree_BorderTop(tree),
		    tree->columnProxy.sx, Tree_BorderBottom(tree));
	} else {
	    Tree_EventuallyRedraw(tree);
	}
	tree->columnProxy.onScreen = FALSE;
    }
}

/*
 *--------------------------------------------------------------
 *
 * TreeRowProxy_Display --
 *
 *	Display the visual indicator used when the user is
 *	resizing a row (if it isn't displayed and should be
 *	displayed).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in the TreeCtrl window.
 *
 *--------------------------------------------------------------
 */

void
TreeRowProxy_Display(
    TreeCtrl *tree		/* Widget info. */
    )
{
    if (!tree->rowProxy.onScreen && (tree->rowProxy.yObj != NULL)) {
	tree->rowProxy.sy = tree->rowProxy.y;
	if (Proxy_IsXOR()) {
	    Proxy_DrawXOR(tree, Tree_BorderLeft(tree), tree->rowProxy.y,
		    Tree_BorderRight(tree), tree->rowProxy.y);
	} else {
	    Tree_EventuallyRedraw(tree);
	}
	tree->rowProxy.onScreen = TRUE;
    }
}

/*
 *--------------------------------------------------------------
 *
 * TreeRowProxy_Undisplay --
 *
 *	Hide the visual indicator used when the user is
 *	resizing a row (if it is displayed).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is erased in the TreeCtrl window.
 *
 *--------------------------------------------------------------
 */

void
TreeRowProxy_Undisplay(
    TreeCtrl *tree		/* Widget info. */
    )
{
    if (tree->rowProxy.onScreen) {
	if (Proxy_IsXOR()) {
	    Proxy_DrawXOR(tree, Tree_BorderLeft(tree), tree->rowProxy.sy,
		    Tree_BorderRight(tree), tree->rowProxy.sy);
	} else {
	    Tree_EventuallyRedraw(tree);
	}
	tree->rowProxy.onScreen = FALSE;
    }
}

/*
 *--------------------------------------------------------------
 *
 * Proxy_Draw --
 *
 *	Draw the non-XOR -columnproxy or -rowproxy indicator.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn into a drawable.
 *
 *--------------------------------------------------------------
 */

static void
Proxy_Draw(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    int x1,			/* Vertical or horizontal line window coords. */
    int y1,
    int x2,
    int y2
    )
{
    XGCValues gcValues;
    unsigned long gcMask;
    GC gc;

    gcValues.function = GXcopy;
    gcValues.graphics_exposures = False;
    gcMask = GCFunction | GCGraphicsExposures;
    gc = Tree_GetGC(tree, gcMask, &gcValues);

    XDrawLine(tree->display, td.drawable, gc, x1, y1, x2, y2);
}

/*
 *--------------------------------------------------------------
 *
 * TreeColumnProxy_Draw --
 *
 *	Draw the non-XOR -columnproxy indicator if it is visible.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn into a drawable.
 *
 *--------------------------------------------------------------
 */

static void
TreeColumnProxy_Draw(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td		/* Where to draw. */
    )
{
    if (tree->columnProxy.xObj == NULL)
	return;
    Proxy_Draw(tree, td, tree->columnProxy.x, Tree_BorderTop(tree),
	    tree->columnProxy.x, Tree_BorderBottom(tree));
}

/*
 *--------------------------------------------------------------
 *
 * TreeRowProxy_Draw --
 *
 *	Draw the non-XOR -rowproxy indicator if it is visible.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn into a drawable.
 *
 *--------------------------------------------------------------
 */

static void
TreeRowProxy_Draw(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td		/* Where to draw. */
    )
{
    if (tree->rowProxy.yObj == NULL)
	return;
    Proxy_Draw(tree, td, Tree_BorderLeft(tree), tree->rowProxy.y,
	    Tree_BorderRight(tree), tree->rowProxy.y);
}

/*
 *--------------------------------------------------------------
 *
 * CalcWhiteSpaceRegion --
 *
 *	Create a new region containing all the whitespace of the list
 *	The whitespace is the area inside the borders/header where items
 *	are not displayed.
 *
 * Results:
 *	The new whitespace region, which may be empty.
 *
 * Side effects:
 *	A new region is allocated.
 *
 *--------------------------------------------------------------
 */

static TkRegion
CalcWhiteSpaceRegion(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    int minX, minY, maxX, maxY;
    int left, right, top, bottom;
    TkRegion wsRgn;
    TkRegion itemRgn;
    XRectangle rect;
    Range *range;

    wsRgn = Tree_GetRegion(tree);

    /* Start with a region as big as the window minus borders + headers */
    minX = Tree_BorderLeft(tree);
    minY = Tree_HeaderBottom(tree);
    maxX = Tree_BorderRight(tree);
    maxY = Tree_BorderBottom(tree);

    /* Nothing is visible? Return empty region. */
    if (minX >= maxX || minY >= maxY)
	return wsRgn;

    rect.x = minX;
    rect.y = minY;
    rect.width = maxX - minX;
    rect.height = maxY - minY;
    TkUnionRectWithRegion(&rect, wsRgn, wsRgn);

    itemRgn = Tree_GetRegion(tree);

    if (tree->itemGapX > 0 || tree->itemGapY > 0) {
	TreeRectangle boundsRect, boundsRectL, boundsRectR;
	DItem *dItem = dInfo->dItem;

	boundsRect = dInfo->bounds;
	boundsRectL = dInfo->boundsL;
	boundsRectR = dInfo->boundsR;

	while (dItem != NULL) {
	    TreeRectangle tr;
	    if (!dInfo->emptyL) {
		tr.x = dItem->left.x;
		tr.y = dItem->y;
		tr.width = dItem->left.width;
		tr.height = dItem->height;
		TreeRect_Intersect(&tr, &tr, &boundsRectL);
		TreeRect_ToXRect(tr, &rect);
		TkUnionRectWithRegion(&rect, itemRgn, itemRgn);
	    }
	    if (!dInfo->emptyR) {
		tr.x = dItem->right.x;
		tr.y = dItem->y;
		tr.width = dItem->right.width;
		tr.height = dItem->height;
		TreeRect_Intersect(&tr, &tr, &boundsRectR);
		TreeRect_ToXRect(tr, &rect);
		TkUnionRectWithRegion(&rect, itemRgn, itemRgn);
	    }
	    if (!dInfo->empty) {
		tr.x = dItem->area.x;
		tr.y = dItem->y;
		tr.width = dItem->area.width;
		tr.height = dItem->height;
		TreeRect_Intersect(&tr, &tr, &boundsRect);
		TreeRect_ToXRect(tr, &rect);
		TkUnionRectWithRegion(&rect, itemRgn, itemRgn);
	    }
	    dItem = dItem->next;
	}
	TkSubtractRegion(wsRgn, itemRgn, wsRgn);
	Tree_FreeRegion(tree, itemRgn);
	return wsRgn;
    }

    /* Subtract area covered by items in left columns */
    if (!dInfo->emptyL) {
	int pad1 = tree->canvasPadY[PAD_TOP_LEFT];
	int pad2 = tree->canvasPadY[PAD_BOTTOM_RIGHT];
	TreeRect_XYXY(dInfo->boundsL, &minX, &minY, &maxX, &maxY);
	left = minX;
	top = MAX(C2Wy(pad1), minY);
	right = maxX;
	bottom = MIN(C2Wy(Tree_CanvasHeight(tree) - pad2), maxY);
	if (top < bottom) {
	    rect.x = left;
	    rect.y = top;
	    rect.width = right - left;
	    rect.height = bottom - top;
	    TkUnionRectWithRegion(&rect, itemRgn, itemRgn);
	}
    }

    /* Subtract area covered by items in right columns */
    if (!dInfo->emptyR) {
	int pad1 = tree->canvasPadY[PAD_TOP_LEFT];
	int pad2 = tree->canvasPadY[PAD_BOTTOM_RIGHT];
	TreeRect_XYXY(dInfo->boundsR, &minX, &minY, &maxX, &maxY);
	left = minX;
	top = MAX(C2Wy(pad1), minY);
	right = maxX;
	bottom = MIN(C2Wy(Tree_CanvasHeight(tree) - pad2), maxY);
	if (top < bottom) {
	    rect.x = left;
	    rect.y = top;
	    rect.width = right - left;
	    rect.height = bottom - top;
	    TkUnionRectWithRegion(&rect, itemRgn, itemRgn);
	}
    }

    /* Subtract area covered by items in unlocked columns */
    if (!dInfo->empty) {
	TreeRect_XYXY(dInfo->bounds, &minX, &minY, &maxX, &maxY);
	for (range = dInfo->rangeFirstD;
	    range != NULL;
	    range = range->next) {

	    left = MAX(C2Wx(range->offset.x), minX);
	    top = MAX(C2Wy(range->offset.y), minY);
	    right = MIN(C2Wx(range->offset.x + range->totalWidth), maxX);
	    bottom = MIN(C2Wy(range->offset.y + range->totalHeight), maxY);
	    if (left < right && top < bottom) {
		rect.x = left;
		rect.y = top;
		rect.width = right - left;
		rect.height = bottom - top;
		TkUnionRectWithRegion(&rect, itemRgn, itemRgn);
	    }

	    if (range == dInfo->rangeLastD)
		break;
	}
    }
    TkSubtractRegion(wsRgn, itemRgn, wsRgn);
    Tree_FreeRegion(tree, itemRgn);
    return wsRgn;
}

#ifdef COMPLEX_WHITESPACE

/*
 *--------------------------------------------------------------
 *
 * TreeRect_Intersect --
 *
 *	Determine the area of overlap between two rectangles.
 *
 * Results:
 *	If the rectangles have non-zero size and overlap, resultPtr
 *	holds the area of overlap, and the return value is 1.
 *	Otherwise 0 is returned and resultPtr is untouched.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
TreeRect_Intersect(
    TreeRectangle *resultPtr,	/* Out: area of overlap. May be the same
				 * as r1 or r2. */
    CONST TreeRectangle *r1,	/* First rectangle. */
    CONST TreeRectangle *r2	/* Second rectangle. */
    )
{
    TreeRectangle result;

    if (r1->width == 0 || r1->height == 0) return 0;
    if (r2->width == 0 || r2->height == 0) return 0;
    if (r1->x >= r2->x + r2->width) return 0;
    if (r2->x >= r1->x + r1->width) return 0;
    if (r1->y >= r2->y + r2->height) return 0;
    if (r2->y >= r1->y + r1->height) return 0;
    
    result.x = MAX(r1->x, r2->x);
    result.width = MIN(r1->x + r1->width, r2->x + r2->width) - result.x;
    result.y = MAX(r1->y, r2->y);
    result.height = MIN(r1->y + r1->height, r2->y + r2->height) - result.y;

    *resultPtr = result;
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * GetItemBgIndex --
 *
 *	Determine the index used to pick an -itembackground color
 *	for a displayed item.
 *	This is only valid for tree->vertical=1.
 *
 * Results:
 *	Integer index.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
GetItemBgIndex(
    TreeCtrl *tree,		/* Widget info. */
    RItem *rItem		/* Range info for an item. */
    )
{
    Range *range = rItem->range;
    int index, indexVis;

    TreeItem_ToIndex(tree, rItem->item, &index, &indexVis);
    switch (tree->backgroundMode) {
#ifdef DEPRECATED
	case BG_MODE_INDEX:
#endif
	case BG_MODE_ORDER:
	    break;
#ifdef DEPRECATED
	case BG_MODE_VISINDEX:
#endif
	case BG_MODE_ORDERVIS:
	    index = indexVis;
	    break;
	case BG_MODE_COLUMN:
	    index = range->index;
	    break;
	case BG_MODE_ROW:
	    index = rItem->index;
	    break;
    }
    return index;
}

#ifdef ITEMBG_ABOVE

/*
 *--------------------------------------------------------------
 *
 * DrawColumnBackgroundReverse --
 *
 *	Draws rows of -itembackground colors in a column in the
 *	whitespace region.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
DrawColumnBackgroundReverse(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeColumn treeColumn,	/* Column to get background colors from. */
    TkRegion dirtyRgn,		/* Area that needs painting. Will be
				 * inside 'bounds' and inside borders. */
    TreeRectangle *bounds,	/* Window coords of column to paint. */
    RItem *rItem,		/* Item(s) to get row heights from when drawing
				 * in the tail column, otherwise NULL. */
    int height,			/* Height of each row below actual items. */
    int index			/* Used for alternating background colors. */
    )
{
#if 0 /* REMOVED BECAUSE OF GRADIENTS */
    int bgCount = TreeColumn_BackgroundCount(treeColumn);
#endif
    GC gc = None, backgroundGC;
    TreeRectangle dirtyBox, drawBox, rowBox;
    int top, bottom;
    TreeColor *tc;

    Tree_GetRegionBounds(dirtyRgn, &dirtyBox);
    if (!dirtyBox.width || !dirtyBox.height)
	return;

    backgroundGC = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);
#if 0 /* REMOVED BECAUSE OF GRADIENTS */
    /* If the column has zero -itembackground colors, paint with the
     * treectrl's -background color. If a single -itembackground color
     * is specified, then paint with it. */
    if (bgCount < 2) {
	if (bgCount == 1)
	    gc = TreeColumn_BackgroundGC(treeColumn, 0);
	if (gc == None)
	    gc = backgroundGC;
	Tree_FillRegion(tree->display, drawable, gc, dirtyRgn);
	return;
    }
#endif

#if 0
    /* If -itembackground colors are transparent, we must draw the tree background
    * color first then the -itembackground colors.  This results in flickering
    * when drawing directly to the toplevel. */
    if (tree->doubleBuffer == DOUBLE_BUFFER_ITEM) {
	tpixmap.width = drawBox.width
	tpixmap.drawable = DisplayGetPixmap(tree, &dInfo->pixmapI,
	    tpixmap.width, tpixmap.height);
    }
#endif

    top = dirtyBox.y;
    bottom = bounds->y + bounds->height;
    while (top < bottom) {
	/* Can't use clipping regions with XFillRectangle
	 * because the clip region is ignored on Win32. */
	rowBox.x = bounds->x;
	rowBox.width = bounds->width;
	rowBox.height = rItem ? rItem->size : height;
	rowBox.y = bottom - rowBox.height;
	if (TreeRect_Intersect(&drawBox, &rowBox, &dirtyBox)) {
	    if (rItem != NULL) {
		index = GetItemBgIndex(tree, rItem);
	    }
	    tc = TreeColumn_BackgroundColor(treeColumn, index);
	    if (tc == NULL) {
		gc = backgroundGC;
		XFillRectangle(tree->display, td.drawable, gc,
		    drawBox.x, drawBox.y, drawBox.width, drawBox.height);
	    } else {
		if (!TreeColor_IsOpaque(tree ,tc)) {
		    XFillRectangle(tree->display, td.drawable, backgroundGC,
			drawBox.x, drawBox.y, drawBox.width, drawBox.height);
		}
		TreeColor_FillRect(tree, td, NULL, tc, rowBox, drawBox);
	    }
	}
	if (rItem != NULL && rItem == rItem->range->last) {
	    index = GetItemBgIndex(tree, rItem);
	    rItem = NULL;
	}
	if (rItem != NULL) {
	    rItem++;
	}
	index++; /* FIXME: -- */
	bottom -= rowBox.height;
    }
}

/*
 *--------------------------------------------------------------
 *
 * DrawWhitespaceAboveItem --
 *
 *	Draws rows of -itembackground colors in each column in the
 *	whitespace region.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
DrawWhitespaceAboveItem(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    int lock,			/* Which columns to draw. */
    int bounds[4],		/* TREE_AREA_xxx bounds. */
    int left,			/* Window coord of first column's left edge. */
    int bottom,			/* Window coord just above the first item. */
    TkRegion dirtyRgn,		/* Area of whitespace that needs painting. */
    TkRegion columnRgn,		/* Existing region to set and use. */
    int height,			/* Height of each row. */
    int index			/* Used for alternating background colors. */
    )
{
    int i = 0, width;
    TreeColumn treeColumn = NULL;
    TreeRectangle boundsBox, columnBox, visBox;

    switch (lock) {
	case COLUMN_LOCK_LEFT:
	    treeColumn = tree->columnLockLeft;
	    break;
	case COLUMN_LOCK_NONE:
	    treeColumn = tree->columnLockNone;
	    break;
	case COLUMN_LOCK_RIGHT:
	    treeColumn = tree->columnLockRight;
	    break;
    }

    boundsBox.x = bounds[0];
    boundsBox.y = bounds[1];
    boundsBox.width = bounds[2] - bounds[0];
    boundsBox.height = bounds[3] - bounds[1];

    for (i = TreeColumn_Index(treeColumn); i < tree->columnCount; i++) {
	if (TreeColumn_Lock(treeColumn) != lock)
	    break;
	width = TreeColumn_GetDInfo(treeColumn)->width;
	if (width == 0) /* also handles hidden columns */
	    goto next;
	columnBox.x = left;
	columnBox.y = bounds[1];
	columnBox.width = width;
	columnBox.height = bottom - columnBox.y;
	if (TreeRect_Intersect(&visBox, &boundsBox, &columnBox)) {
	    Tree_SetRectRegion(columnRgn, &visBox);
	    TkIntersectRegion(dirtyRgn, columnRgn, columnRgn);
	    DrawColumnBackgroundReverse(tree, td, treeColumn,
		    columnRgn, &columnBox, (RItem *) NULL, height, index);
	}
	left += width;
next:
	treeColumn = TreeColumn_Next(treeColumn);
    }
}

#endif /* ITEMBG_ABOVE */

/*
 *--------------------------------------------------------------
 *
 * DrawColumnBackground --
 *
 *	Draws rows of -itembackground colors in a column in the
 *	whitespace region.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
DrawColumnBackground(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeColumn treeColumn,	/* Column to get background colors from. */
    TkRegion dirtyRgn,		/* Area that needs painting. Will be
				 * inside 'bounds' and inside borders. */
    TreeRectangle *bounds,	/* Window coords of column to paint. */
    RItem *rItem,		/* Item(s) to get row heights from when drawing
				 * in the tail column, otherwise NULL. */
    int height,			/* Height of each row below actual items. */
    int index			/* Used for alternating background colors. */
    )
{
#if 0 /* REMOVED BECAUSE OF GRADIENTS */
    int bgCount = TreeColumn_BackgroundCount(treeColumn);
#endif
    GC gc = None, backgroundGC;
    TreeRectangle dirtyBox, drawBox, rowBox;
    int top, bottom;
    TreeColor *tc;

    Tree_GetRegionBounds(dirtyRgn, &dirtyBox);
    if (!dirtyBox.width || !dirtyBox.height)
	return;

    backgroundGC = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);
#if 0 /* REMOVED BECAUSE OF GRADIENTS */
    /* If the column has zero -itembackground colors, paint with the
     * treectrl's -background color. If a single -itembackground color
     * is specified, then paint with it. */
    if (bgCount < 2) {
	if (bgCount == 1)
	    gc = TreeColumn_BackgroundGC(treeColumn, 0);
	if (gc == None)
	    gc = backgroundGC;
	Tree_FillRegion(tree->display, drawable, gc, dirtyRgn);
	return;
    }
#endif

#if 0
    /* If -itembackground colors are transparent, we must draw the tree background
    * color first then the -itembackground colors.  This results in flickering
    * when drawing directly to the toplevel. */
    if (tree->doubleBuffer == DOUBLE_BUFFER_ITEM) {
	tpixmap.width = drawBox.width
	tpixmap.drawable = DisplayGetPixmap(tree, &dInfo->pixmapI,
	    tpixmap.width, tpixmap.height);
    }
#endif

    top = bounds->y;
    bottom = dirtyBox.y + dirtyBox.height;
    while (top < bottom) {
	/* Can't use clipping regions with XFillRectangle
	 * because the clip region is ignored on Win32. */
	TreeRect_SetXYWH(rowBox,
		bounds->x,
		top,
		bounds->width,
		rItem ? rItem->size : height);
	if (TreeRect_Intersect(&drawBox, &rowBox, &dirtyBox)) {
	    TreeRectangle trBrush;
	    if (rItem != NULL) {
		index = GetItemBgIndex(tree, rItem);
	    }
	    tc = TreeColumn_BackgroundColor(treeColumn, index);

	    /* Handle the drawable offset from the top-left of the window */
	    drawBox.x -= tree->drawableXOrigin;
	    drawBox.y -= tree->drawableYOrigin;

	    if (tc == NULL) {
		gc = backgroundGC;
		XFillRectangle(tree->display, td.drawable, gc,
		    drawBox.x, drawBox.y, drawBox.width, drawBox.height);
	    } else {
		TreeColor_GetBrushBounds(tree, tc, rowBox,
			tree->xOrigin, tree->yOrigin,
			treeColumn, (TreeItem) NULL, &trBrush);
		if (!TreeColor_IsOpaque(tree, tc)
			|| (trBrush.width <= 0)
			|| (trBrush.height <= 0)) {
		    XFillRectangle(tree->display, td.drawable, backgroundGC,
			drawBox.x, drawBox.y, drawBox.width, drawBox.height);
		}

		/* Handle the drawable offset from the top-left of the window */
		trBrush.x -= tree->drawableXOrigin;
		trBrush.y -= tree->drawableYOrigin;

		TreeColor_FillRect(tree, td, NULL, tc, trBrush, drawBox);
	    }
	}
	if (rItem != NULL && rItem == rItem->range->last) {
	    index = GetItemBgIndex(tree, rItem);
	    rItem = NULL;
	}
	if (rItem != NULL) {
	    rItem++;
	}
	if (tree->backgroundMode != BG_MODE_COLUMN)
	    index++;
	top += rowBox.height;
	top += tree->itemGapY;
    }
}

/*
 *--------------------------------------------------------------
 *
 * DrawWhitespaceBelowItem --
 *
 *	Draws rows of -itembackground colors in each column in the
 *	whitespace region.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
DrawWhitespaceBelowItem(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeColumn treeColumn,	/* Which columns to draw. */
    TreeRectangle bounds,	/* TREE_AREA_xxx bounds. */
    int left,			/* Window coord of first column's left edge. */
    int rangeWidth,		/* Width of range, needed when only 1 column
				 * is visible with wrapping. */
    int top,			/* Window coord just below the last item. */
    TkRegion dirtyRgn,		/* Area of whitespace that needs painting. */
    TkRegion columnRgn,		/* Existing region to set and use. */
    int height,			/* Height of each row. */
    int index			/* Used for alternating background colors. */
    )
{
    int lock = TreeColumn_Lock(treeColumn);
    int width;
    TreeRectangle boundsBox, columnBox, visBox;

    boundsBox = bounds;

    for (;
	    (treeColumn != NULL) && (TreeColumn_Lock(treeColumn) == lock);
	    treeColumn = TreeColumn_Next(treeColumn)) {
	width = TreeColumn_GetDInfo(treeColumn)->width;
	if (width == 0) /* also handles hidden columns */
	    continue;
	if (tree->columnCountVis == 1 && rangeWidth != -1)
	    width = rangeWidth;
	TreeRect_SetXYWH(columnBox,
	    left, top,
	    width, TreeRect_Bottom(bounds) - top);
	if (TreeRect_Intersect(&visBox, &boundsBox, &columnBox)) {
	    Tree_SetRectRegion(columnRgn, &visBox);
	    TkIntersectRegion(dirtyRgn, columnRgn, columnRgn);
	    DrawColumnBackground(tree, td, treeColumn,
		    columnRgn, &columnBox, (RItem *) NULL, height, index);
	}
	left += width;
    }
}

/*
 *--------------------------------------------------------------
 *
 * ComplexWhitespace --
 *
 *	Return 1 if -itembackground colors should be drawn into the
 *	whitespace region.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
ComplexWhitespace(
    TreeCtrl *tree
    )
{
    if (tree->columnBgCnt == 0 &&
	    TreeColumn_BackgroundCount(tree->columnTail) == 0)
	return 0;

    if (!tree->vertical /*|| (tree->wrapMode != TREE_WRAP_NONE) ||
	    (tree->itemWrapCount > 0)*/)
	return 0;

    if (tree->itemHeight <= 0 && tree->minItemHeight <= 0)
	return 0;

    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * DrawWhitespace --
 *
 *	Paints part of the whitespace region.
 *
 * Results:
 *	If -itembackground colors are not being drawn into the
 *	whitespace region, the dirtyRgn is filled with the treectrl's
 *	-background color. Otherwise rows of color are drawn below
 *	the last item and in the tail column if those columns have
 *	any -itembackground colors specified.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
DrawWhitespace(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TkRegion dirtyRgn		/* The region that needs repainting. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    int minX, minY, maxX, maxY;
    int top, bottom;
    int height, index;
    TreeRectangle columnBox;
    TkRegion columnRgn;
    Range *range;
    RItem *rItem;

    /* If we aren't drawing -itembackground colors in the whitespace region,
     * then just paint the entire dirty area with the treectrl's -background
     * color. */
    if (!ComplexWhitespace(tree)) {
	GC gc = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);

	/* Handle the drawable offset from the top-left of the window */
	Tree_OffsetRegion(dirtyRgn, -tree->drawableXOrigin, -tree->drawableYOrigin);

	Tree_FillRegion(tree->display, td.drawable, gc, dirtyRgn);

	/* Handle the drawable offset from the top-left of the window */
	Tree_OffsetRegion(dirtyRgn, tree->drawableXOrigin, tree->drawableYOrigin);
	return;
    }

    /* Erase whitespace in the gaps between items using the treectrl's
     * background color. */
    if (tree->itemGapX > 0 || tree->itemGapY > 0) {
	GC gc = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);
	Tree_FillRegion(tree->display, td.drawable, gc, dirtyRgn);
    }

    /* Figure out the height of each row of color below the items. */
    if (tree->backgroundMode == BG_MODE_COLUMN)
	height = -1; /* solid block of color */
    else if (tree->itemHeight > 0)
	height = tree->itemHeight;
    else
	height = tree->minItemHeight;

    columnRgn = Tree_GetRegion(tree);

    range = dInfo->rangeFirst;
    if (range == NULL)
	range = dInfo->rangeLock;

    if (!dInfo->empty) {
	int leftEdgeOfColumns = tree->canvasPadX[PAD_TOP_LEFT];
	int rightEdgeOfColumns = Tree_CanvasWidth(tree) - tree->canvasPadX[PAD_BOTTOM_RIGHT];

	TreeRect_XYXY(dInfo->bounds, &minX, &minY, &maxX, &maxY);

	if (tree->backgroundMode == BG_MODE_COLUMN) {
	    top = MAX(C2Wy(tree->canvasPadY[PAD_TOP_LEFT]),
		minY);
	    bottom = maxY;
	    height = bottom - top; /* solid block of color */
	}

	/* Draw to the right of the items using the tail column's
	 * -itembackground colors. The height of each row matches
	 * the height of the adjacent item. */
	if (C2Wx(rightEdgeOfColumns) < maxX) {
	    columnBox.y = minY;
	    if (range == NULL) {
		rItem = NULL;
		index = 0;
	    } else {
		/* Get the item at the top of the screen. */
		if (range->totalHeight == 0) {
		    rItem = range->last; /* all items have zero height */
		} else {
		    int ccContentTop = W2Cy(minY);
		    int rcContentTop = ccContentTop - range->offset.y; /* could be < 0 */
		    int rcY = MAX(rcContentTop, 0);
		    rItem = Range_ItemUnderPoint(tree, range, -666, rcY, NULL, NULL, 3);
		    columnBox.y = C2Wy(range->offset.y + rItem->offset);
		}
		index = GetItemBgIndex(tree, rItem);
	    }
	    columnBox.x = C2Wx(rightEdgeOfColumns);
	    columnBox.width = maxX - columnBox.x;
	    columnBox.height = maxY - columnBox.y;
	    Tree_SetRectRegion(columnRgn, &columnBox);
	    TkIntersectRegion(dirtyRgn, columnRgn, columnRgn);
	    DrawColumnBackground(tree, td, tree->columnTail,
		    columnRgn, &columnBox, rItem, height, index);
	}

	/* Draw to the left of the items using the first visible
	 * column's -itembackground colors. The height of each row matches
	 * the height of the adjacent item. */
	if (C2Wx(leftEdgeOfColumns) > minX) {
	    columnBox.y = minY;
	    if (range == NULL) {
		rItem = NULL;
		index = 0;
	    } else {
		/* Get the item at the top of the screen. */
		if (range->totalHeight == 0) {
		    rItem = range->last; /* all items have zero height */
		} else {
		    int ccContentTop = W2Cy(minY);
		    int rcContentTop = ccContentTop - range->offset.y; /* could be < 0 */
		    int rcY = MAX(rcContentTop,0);
		    rItem = Range_ItemUnderPoint(tree, range, -666, rcY, NULL, NULL, 3);
		    columnBox.y = C2Wy(range->offset.y + rItem->offset);
		}
		index = GetItemBgIndex(tree, rItem);
	    }
	    columnBox.x = minX;
	    columnBox.width = C2Wx(leftEdgeOfColumns) - columnBox.x;
	    columnBox.height = maxY - columnBox.y;
	    Tree_SetRectRegion(columnRgn, &columnBox);
	    TkIntersectRegion(dirtyRgn, columnRgn, columnRgn);
	    DrawColumnBackground(tree, td, tree->columnVis ?
		    tree->columnVis : tree->columnTail,
		    columnRgn, &columnBox, rItem, height, index);
	}
    }

    /* Draw below non-locked columns. */
    if (!dInfo->empty && tree->columnVis != NULL) {
	if (dInfo->rangeFirst == NULL) {
	    index = 0;
	    top = Tree_ContentTop(tree);
	    bottom = Tree_ContentBottom(tree);
	    if (tree->backgroundMode == BG_MODE_COLUMN)
		height = bottom - top; /* solid block of color */
	    DrawWhitespaceBelowItem(tree, td, tree->columnLockNone,
		    dInfo->bounds, C2Wx(tree->canvasPadX[PAD_TOP_LEFT]), -1,
		    top, dirtyRgn, columnRgn,
		    height, index);
	} else {
	    int left = tree->canvasPadX[PAD_TOP_LEFT];
	    while (range != NULL) {
		top = MAX(C2Wy(range->offset.y + range->totalHeight),
			Tree_ContentTop(tree));
		bottom = Tree_ContentBottom(tree);
		if ((C2Wx(left + range->totalWidth) > TreeRect_Left(dInfo->bounds))
			&& (top < bottom)) {
		    rItem = range->last;
		    index = GetItemBgIndex(tree, rItem);
		    if (tree->backgroundMode != BG_MODE_COLUMN) {
			index++;
		    }
		    if (tree->backgroundMode == BG_MODE_COLUMN)
			height = bottom - top; /* solid block of color */
		    DrawWhitespaceBelowItem(tree, td, tree->columnLockNone,
			    dInfo->bounds, C2Wx(left), range->totalWidth,
			    top, dirtyRgn, columnRgn,
			    height, index);
		}
		left += range->totalWidth;
		if (C2Wx(left) >= TreeRect_Right(dInfo->bounds))
		    break;
		range = range->next;
	    }
	}
    }

    top = MAX(C2Wy(Tree_CanvasHeight(tree))
	- tree->canvasPadY[PAD_BOTTOM_RIGHT]
	+ tree->itemGapY, Tree_ContentTop(tree));
    bottom = Tree_ContentBottom(tree);

    if ((top < bottom) && !(dInfo->emptyL && dInfo->emptyR)) {

	if (tree->backgroundMode == BG_MODE_COLUMN)
	    height = bottom - top; /* solid block of color */

	range = dInfo->rangeFirst;
	if (range == NULL)
	    range = dInfo->rangeLock;

	/* Get the display index of the last visible item, if any. */
	if (range == NULL) {
	    index = 0;
	} else {
	    rItem = range->last;
	    index = GetItemBgIndex(tree, rItem);
	    if (tree->backgroundMode != BG_MODE_COLUMN) {
		index++;
	    }
	}

	/* Draw below the left columns. */
	if (!dInfo->emptyL) {
	    minX = TreeRect_Left(dInfo->boundsL);
	    DrawWhitespaceBelowItem(tree, td, tree->columnLockLeft,
		    dInfo->boundsL,
		    minX, -1, top, dirtyRgn, columnRgn,
		    height, index);
	}

	/* Draw below the right columns. */
	if (!dInfo->emptyR) {
	    minX = TreeRect_Left(dInfo->boundsR);
	    DrawWhitespaceBelowItem(tree, td, tree->columnLockRight,
		    dInfo->boundsR,
		    minX, -1, top, dirtyRgn, columnRgn,
		    height, index);
	}
    }

    top = MAX(C2Wy(0), Tree_ContentTop(tree));
    bottom = MAX(C2Wy(tree->canvasPadY[PAD_TOP_LEFT]), Tree_ContentTop(tree));
    if (top < bottom) {
#ifndef ITEMBG_ABOVE
	GC gc = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);

	columnBox.x = Tree_BorderLeft(tree);
	columnBox.y = Tree_ContentTop(tree);
	columnBox.width = Tree_BorderRight(tree) - Tree_BorderLeft(tree);
	columnBox.height = bottom - top;
	Tree_SetRectRegion(columnRgn, &columnBox);
	TkIntersectRegion(dirtyRgn, columnRgn, columnRgn);

	/* Handle the drawable offset from the top-left of the window */
	Tree_OffsetRegion(columnRgn, -tree->drawableXOrigin, -tree->drawableYOrigin);

	Tree_FillRegion(tree->display, td.drawable, gc, columnRgn);

	/* Handle the drawable offset from the top-left of the window */
	Tree_OffsetRegion(columnRgn, tree->drawableXOrigin, tree->drawableYOrigin);
#else
	/* Get the display index of the first visible item. */
	if (range == NULL) {
	    index = 0;
	} else {
	    rItem = range->first;
	    index = GetItemBgIndex(tree, rItem);
	    if (tree->backgroundMode != BG_MODE_COLUMN) {
		index++;  /* FIXME: -- */
	    }
	}

	/* Draw above non-locked columns. */
	if (!dInfo->empty && Tree_CanvasWidth(tree)/* && dInfo->rangeFirst != NULL */) {
	    DrawWhitespaceAboveItem(tree, td, COLUMN_LOCK_NONE,
		    dInfo->bounds, x + tree->canvasPadX[PAD_TOP_LEFT], bottom, dirtyRgn, columnRgn,
		    height, index);
	}

	/* Draw above the left columns. */
	if (!dInfo->emptyL) {
	    minX = TreeRect_Left(dInfo->boundsL);
	    DrawWhitespaceAboveItem(tree, td, COLUMN_LOCK_LEFT,
		    dInfo->boundsL,
		    minX, bottom, dirtyRgn, columnRgn,
		    height, index);
	}

	/* Draw above the right columns. */
	if (!dInfo->emptyR) {
	    minX = TreeRect_Left(dInfo->boundsR);
	    DrawWhitespaceAboveItem(tree, td, COLUMN_LOCK_RIGHT,
		    dInfo->boundsR,
		    minX, bottom, dirtyRgn, columnRgn,
		    height, index);
	}
#endif
    }

    Tree_FreeRegion(tree, columnRgn);
}

#endif /* COMPLEX_WHITESPACE */

#if COLUMNGRID == 1

static int
GridLinesInWhiteSpace(
    TreeCtrl *tree
    )
{
    if (tree->columnsWithGridLines <= 0)
	return 0;

    if (!tree->vertical)
	return 0;

/*    if ((tree->wrapMode != TREE_WRAP_NONE) || (tree->itemWrapCount > 0))
	return 0;*/

    return 1;
}

static void
DrawColumnGridLinesAux(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn treeColumn,	/* First column. */
    TreeDrawable td,		/* Where to draw. */
    const TreeRectangle *boundsPtr,/* TREA_AREA_xxx bounds. */
    int left,			/* Left edge of first column. */
    int rangeWidth,		/* Width of range, needed when only 1 column
				 * is visible with wrapping. */
    int minY, int maxY,		/* Top & bottom of area to draw in. */
    TkRegion dirtyRgn		/* The region that needs repainting. */
    )
{
    int lock = TreeColumn_Lock(treeColumn);
    int columnWidth;
    TreeRectangle columnBox, gridBox, trBrush;
    TreeColor *leftColor, *rightColor;
    int leftWidth, rightWidth;
    TreeClip clip;

    clip.type = TREE_CLIP_REGION;
    clip.region = dirtyRgn;

    for (;
	    treeColumn != NULL && TreeColumn_Lock(treeColumn) == lock;
	    treeColumn = TreeColumn_Next(treeColumn)) {

	if (TreeColumn_GridColors(treeColumn, &leftColor, &rightColor,
		&leftWidth, &rightWidth) == 0) {
	    continue;
	}

	columnWidth = TreeColumn_GetDInfo(treeColumn)->width;
	if (columnWidth == 0) /* also handles hidden columns */
	    continue;
	if (tree->columnCountVis == 1 && rangeWidth != -1)
	    columnWidth = rangeWidth;

	TreeRect_SetXYWH(columnBox, left + TreeColumn_Offset(treeColumn),
		minY, columnWidth, maxY - minY);

	if (TreeRect_Right(columnBox) <= TreeRect_Left(*boundsPtr))
	    continue;
	if (TreeRect_Left(columnBox) >= TreeRect_Right(*boundsPtr))
	    break;

	if (leftColor != NULL && leftWidth > 0) {
	    TreeRect_SetXYWH(gridBox, columnBox.x, columnBox.y, leftWidth,
		    columnBox.height);
	    if (TreeRect_Intersect(&gridBox, boundsPtr, &gridBox)) {
		TreeColor_GetBrushBounds(tree, leftColor, gridBox,
			tree->xOrigin, tree->yOrigin,
			treeColumn, (TreeItem) NULL, &trBrush);
		TreeColor_FillRect(tree, td, &clip, leftColor, trBrush,
			gridBox);
	    }
	}
	if (rightColor != NULL && rightWidth > 0) {
	    TreeRect_SetXYWH(gridBox, columnBox.x + columnBox.width - rightWidth,
		    columnBox.y, rightWidth, columnBox.height);
	    if (TreeRect_Intersect(&gridBox, boundsPtr, &gridBox)) {
		TreeColor_GetBrushBounds(tree, rightColor, gridBox,
			tree->xOrigin, tree->yOrigin,
			treeColumn, (TreeItem) NULL, &trBrush);
		TreeColor_FillRect(tree, td, &clip, rightColor, trBrush,
			gridBox);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DrawColumnGridLines --
 *
 *	Draws the column gridlines in the whitespace region below any
 *	items.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn.
 *
 *----------------------------------------------------------------------
 */

static void
DrawColumnGridLines(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TkRegion dirtyRgn		/* The region that needs repainting. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    int minY, maxY;
    Range *range = dInfo->rangeFirst;

    if (!GridLinesInWhiteSpace(tree))
	return;

    maxY = Tree_ContentBottom(tree);

    /* Draw gridlines below non-locked columns in every Range. */
    if (!dInfo->empty && tree->columnVis != NULL) {
	int left = tree->canvasPadX[PAD_TOP_LEFT];
	if (range == NULL) {
	    minY = Tree_ContentTop(tree);
	    if (minY < maxY) {
		DrawColumnGridLinesAux(tree, tree->columnLockNone, td,
			&dInfo->bounds,
			C2Wx(left - tree->canvasPadX[PAD_TOP_LEFT]),
			-1,
			minY, maxY, dirtyRgn);
	    }
	} else {
	    while (range != NULL) {
		minY = MAX(C2Wy(range->offset.y + range->totalHeight),
			Tree_ContentTop(tree));
		if ((C2Wx(left + range->totalWidth) > TreeRect_Left(dInfo->bounds))
			&& (minY < maxY)) {
		    DrawColumnGridLinesAux(tree, tree->columnLockNone, td,
			    &dInfo->bounds,
			    C2Wx(left - tree->canvasPadX[PAD_TOP_LEFT]),
			    range->totalWidth,
			    minY, maxY, dirtyRgn);
		}
		left += range->totalWidth;
		if (C2Wx(left) >= TreeRect_Right(dInfo->bounds))
		    break;
		range = range->next;
	    }
	}
    }

    minY = MAX(C2Wy(Tree_CanvasHeight(tree))
	- tree->canvasPadY[PAD_BOTTOM_RIGHT],
	Tree_ContentTop(tree));
    if (minY >= maxY)
	return;

    if (!dInfo->emptyL) {
	DrawColumnGridLinesAux(tree, tree->columnLockLeft, td, &dInfo->boundsL,
		Tree_BorderLeft(tree), -1, minY, maxY, dirtyRgn);
    }
    if (!dInfo->emptyR) {
	DrawColumnGridLinesAux(tree, tree->columnLockRight, td, &dInfo->boundsR,
		Tree_ContentRight(tree), -1, minY, maxY, dirtyRgn);
    }
}

#endif

/*
 *----------------------------------------------------------------------
 *
 * Tree_IsBgImageOpaque --
 *
 *	Determines if there is any need to erase before drawing the
 *	-backgroundimage.
 *
 * Results:
 *	Return 1 if the -backgroundimage will completely fill any
 *	whitespace it is drawn into.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tree_IsBgImageOpaque(
    TreeCtrl *tree		/* Widget info. */
    )
{
    if (tree->backgroundImage == NULL)
	return 0;
    if ((tree->bgImageTile & (BGIMG_TILE_X|BGIMG_TILE_Y)) !=
	    (BGIMG_TILE_X|BGIMG_TILE_Y))
	return 0;
    return tree->bgImageOpaque;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_DrawTiledImage --
 *
 *	This procedure draws a tiled image in the indicated box.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn.
 *
 *----------------------------------------------------------------------
 */

int
Tree_DrawTiledImage(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    Tk_Image image,		/* The image to draw. */
    TreeRectangle tr,		/* Area to paint, may not be filled. */
    int xOffset, int yOffset,	/* X and Y coord of where to start tiling. */
    int tileX, int tileY	/* Axes to tile along. */
    )
{
    int imgWidth, imgHeight;
    TreeRectangle trImage, trPaint;
    int drawn = 0;
#if CACHE_BG_IMG
    Pixmap pixmap = None;
#endif

    Tk_SizeOfImage(image, &imgWidth, &imgHeight);
    if (imgWidth <= 0 || imgHeight <= 0)
	return 0;

#if CACHE_BG_IMG
    /* This pixmap is destroyed at the end of each call to Tree_Display,
     * so any changes to -backgroundimage will be seen. */
    if ((image == tree->backgroundImage) && tree->bgImageOpaque) {
	pixmap = tree->dInfo->pixmapBgImg.drawable;
	if (pixmap == None) {
	    pixmap = DisplayGetPixmap(tree,
		&tree->dInfo->pixmapBgImg, imgWidth, imgHeight);
	    Tk_RedrawImage(image, 0, 0, imgWidth, imgHeight, pixmap, 0, 0);
	}
    }
#endif

    while (tileX && xOffset > tr.x)
	xOffset -= imgWidth;
    while (tileY && yOffset > tr.y)
	yOffset -= imgHeight;

    trImage.x = xOffset, trImage.y = yOffset;
    trImage.width = imgWidth, trImage.height = imgHeight;

    do {
	do {
	    if (TreeRect_Intersect(&trPaint, &trImage, &tr)) {
#if CACHE_BG_IMG
		if (pixmap != None) {
		    XCopyArea(tree->display, pixmap, td.drawable, tree->copyGC,
			    trPaint.x - trImage.x,
			    trPaint.y - trImage.y,
			    trPaint.width, trPaint.height,
			    trPaint.x, trPaint.y);
		} else
#endif
		Tk_RedrawImage(image, trPaint.x - trImage.x,
		    trPaint.y - trImage.y, trPaint.width, trPaint.height,
		    td.drawable, trPaint.x, trPaint.y);
		drawn = 1;
	    }
	    trImage.y += trImage.height;
	} while (tileY && trImage.y < tr.y + tr.height);
	trImage.x += trImage.width;
	trImage.y = yOffset;
    } while (tileX && trImage.x < tr.x + tr.width);

    return drawn;
}

/*
 *----------------------------------------------------------------------
 *
 * CalcBgImageBounds --
 *
 *	Calculate the bounds (in canvas coordinates) of the background
 *	image.  The background image may be tiled relative to the
 *	returned bounds.
 *
 * Results:
 *	Sets trImage with the bounds and returns 1.
 *
 * Side effects:
 *	May update item layout.
 *
 *----------------------------------------------------------------------
 */

static int
CalcBgImageBounds(
    TreeCtrl *tree,		/* Widget info. */
    TreeRectangle *trImage	/* Returned image bounds. */
    )
{
    int x1, y1, x2, y2;
    int imgWidth, imgHeight;

    if (tree->bgImageScroll & BGIMG_SCROLL_X) {
	x1 = 0;
	x2 = Tree_FakeCanvasWidth(tree);
    } else {
	x1 = W2Cx(Tree_ContentLeft(tree));
	x2 = x1 + Tree_ContentWidth(tree);
    }
    if (tree->bgImageScroll & BGIMG_SCROLL_Y) {
	y1 = 0;
	y2 = Tree_FakeCanvasHeight(tree);
    } else {
	y1 = W2Cy(Tree_ContentTop(tree));
	y2 = y1 + Tree_ContentHeight(tree);
    }

    Tk_SizeOfImage(tree->backgroundImage, &imgWidth, &imgHeight);

    switch (tree->bgImageAnchor) {
	case TK_ANCHOR_NW:
	case TK_ANCHOR_W:
	case TK_ANCHOR_SW:
	    break;
	case TK_ANCHOR_N:
	case TK_ANCHOR_CENTER:
	case TK_ANCHOR_S:
	    x1 = x1 + (x2 - x1) / 2 - imgWidth / 2;
	    break;
	case TK_ANCHOR_NE:
	case TK_ANCHOR_E:
	case TK_ANCHOR_SE:
	    x1 = x2 - imgWidth;
	    break;
    }

    switch (tree->bgImageAnchor) {
	case TK_ANCHOR_NW:
	case TK_ANCHOR_N:
	case TK_ANCHOR_NE:
	    break;
	case TK_ANCHOR_W:
	case TK_ANCHOR_CENTER:
	case TK_ANCHOR_E:
	    y1 = y1 + (y2 - y1) / 2 - imgHeight / 2;
	    break;
	case TK_ANCHOR_SW:
	case TK_ANCHOR_S:
	case TK_ANCHOR_SE:
	    y1 = y2 - imgHeight;
	    break;
    }

    trImage->x = x1, trImage->y = y1;
    trImage->width = /*(tree->bgImageTile & BGIMG_TILE_X) ? (x2 - x1) :*/ imgWidth;
    trImage->height = /*(tree->bgImageTile & BGIMG_TILE_Y) ? (y2 - y1) :*/ imgHeight;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_DrawBgImage --
 *
 *	Draws the -backgroundimage into the specified drawable inside
 *	the given rectangle, positioning and tiling the image according
 *	to the various bgimage widget options.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn.
 *
 *----------------------------------------------------------------------
 */

int
Tree_DrawBgImage(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeRectangle tr,		/* Rect to paint with the image. */
    int xOrigin,		/* Origin of the given drawable in */
    int yOrigin			/* canvas coordinates. */
    )
{
    TreeRectangle trImage;

    (void) CalcBgImageBounds(tree, &trImage);

    return Tree_DrawTiledImage(tree, td, tree->backgroundImage, tr,
	trImage.x - xOrigin, trImage.y - yOrigin,
	(tree->bgImageTile & BGIMG_TILE_X) != 0,
	(tree->bgImageTile & BGIMG_TILE_Y) != 0);
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayDItem --
 *
 *	Draw a single item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn.
 *
 *----------------------------------------------------------------------
 */

static int
DisplayDItem(
    TreeCtrl *tree,		/* Widget info. */
    DItem *dItem,		/* Display info for an item. */
    DItemArea *area,
    int lock,			/* Which set of columns. */
    TreeRectangle bounds,	/* TREE_AREA_xxx bounds of drawing. */
    TreeDrawable pixmap,	/* Where to draw. */
    TreeDrawable drawable	/* Where to copy to. */
    )
{
    Tk_Window tkwin = tree->tkwin;
    int left, top, right, bottom;

    left = area->x;
    right = left + area->width;
    top = dItem->y;
    bottom = top + dItem->height;

    if (!(area->flags & DITEM_ALL_DIRTY)) {
	left += area->dirty[LEFT];
	right = area->x + area->dirty[RIGHT];
	top += area->dirty[TOP];
	bottom = dItem->y + area->dirty[BOTTOM];
    }

    area->flags &= ~(DITEM_DIRTY | DITEM_ALL_DIRTY);
    area->flags |= DITEM_DRAWN;

    dItem->flags &= ~(DITEM_INVALIDATE_ON_SCROLL_X | DITEM_INVALIDATE_ON_SCROLL_Y);

    if (left < TreeRect_Left(bounds))
	left = TreeRect_Left(bounds);
    if (right > TreeRect_Right(bounds))
	right = TreeRect_Right(bounds);
    if (top < TreeRect_Top(bounds))
	top = TreeRect_Top(bounds);
    if (bottom > TreeRect_Bottom(bounds))
	bottom = TreeRect_Bottom(bounds);

    if (right <= left || bottom <= top)
	return 0;

    if (tree->debug.enable && tree->debug.display && tree->debug.drawColor) {
	XFillRectangle(tree->display, Tk_WindowId(tkwin),
		tree->debug.gcDraw, left, top, right - left, bottom - top);
	DisplayDelay(tree);
    }

#if USE_ITEM_PIXMAP == 0
    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	DblBufWinDirty(tree, left, top, right, bottom);
    }

    /* The top-left corner of the drawable is at this
    * point in the canvas */
    tree->drawableXOrigin = tree->xOrigin;
    tree->drawableYOrigin = tree->yOrigin;

    TreeItem_Draw(tree, dItem->item,
	    lock,
	    area->x,
	    dItem->y,
	    area->width, dItem->height,
	    drawable,
	    left, right,
	    dItem->index);
#else
    if (tree->doubleBuffer != DOUBLEBUFFER_NONE) {

	if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	    DblBufWinDirty(tree, left, top, right, bottom);
	}

	/* The top-left corner of the drawable is at this
	* point in the canvas */
	tree->drawableXOrigin = W2Cx(left);
	tree->drawableYOrigin = W2Cy(top);

	TreeItem_Draw(tree, dItem->item, lock,
		area->x - left, dItem->y - top,
		area->width, dItem->height,
		pixmap,
		0, right - left,
		dItem->index);
	XCopyArea(tree->display, pixmap.drawable, drawable.drawable,
		tree->copyGC,
		0, 0,
		right - left, bottom - top,
		left, top);
    } else {
	/* The top-left corner of the drawable is at this
	* point in the canvas */
	tree->drawableXOrigin = tree->xOrigin;
	tree->drawableYOrigin = tree->yOrigin;

	TreeItem_Draw(tree, dItem->item,
		lock,
		area->x,
		dItem->y,
		area->width, dItem->height,
		drawable,
		left, right,
		dItem->index);
    }
#endif

#if REDRAW_RGN == 1
    AddRectToRedrawRgn(tree, left, top, right, bottom);
#endif /* REDRAW_RGN */

    return 1;
}

static void
DebugDrawBorder(
    TreeCtrl *tree,
    int inset,
    int left,
    int top,
    int right,
    int bottom
    )
{
    Tk_Window tkwin = tree->tkwin;

    if (tree->debug.enable && tree->debug.display && tree->debug.drawColor) {
	if (left > 0) {
	    XFillRectangle(tree->display, Tk_WindowId(tkwin),
		    tree->debug.gcDraw,
		    inset, inset,
		    left, Tk_Height(tkwin) - inset * 2);
	}
	if (top > 0) {
	    XFillRectangle(tree->display, Tk_WindowId(tkwin),
		    tree->debug.gcDraw,
		    inset, inset,
		    Tk_Width(tkwin) - inset * 2, top);
	}
	if (right > 0) {
	    XFillRectangle(tree->display, Tk_WindowId(tkwin),
		    tree->debug.gcDraw,
		    Tk_Width(tkwin) - inset - right, inset,
		    right, Tk_Height(tkwin) - inset * 2);
	}
	if (bottom > 0) {
	    XFillRectangle(tree->display, Tk_WindowId(tkwin),
		    tree->debug.gcDraw,
		    inset, Tk_Height(tkwin) - inset - bottom,
		    Tk_Width(tkwin) - inset * 2, bottom);
	}
	DisplayDelay(tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TreeDisplay_GetReadyForTrouble --
 * TreeDisplay_WasThereTrouble --
 *
 *	These 2 procedures are used to detect when something happens
 *	during a display update that requests another display update.
 *	If that happens, then the current display is aborted and we
 *	try again (unless the window was destroyed).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
TreeDisplay_GetReadyForTrouble(
    TreeCtrl *tree,
    int *requestsPtr
    )
{
    TreeDInfo dInfo = tree->dInfo;

    *requestsPtr = dInfo->requests;
}

int
TreeDisplay_WasThereTrouble(
    TreeCtrl *tree,
    int requests
    )
{
    TreeDInfo dInfo = tree->dInfo;

    if (tree->deleted || (requests != dInfo->requests)) {
	if (tree->debug.enable)
	    dbwin("TreeDisplay_WasThereTrouble: %p\n", tree);
	return 1;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayGetPixmap --
 *
 *	Allocate or reallocate a pixmap of needed size.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Pixmap
DisplayGetPixmap(
    TreeCtrl *tree,
    TreeDrawable *dPixmap,
    int width,
    int height
    )
{
    Tk_Window tkwin = tree->tkwin;

    if (dPixmap->drawable == None) {
	dPixmap->drawable = Tk_GetPixmap(tree->display,
		Tk_WindowId(tkwin), width, height, Tk_Depth(tkwin));
	dPixmap->width = width;
	dPixmap->height = height;

    } else if ((dPixmap->width < width) || (dPixmap->height < height)) {
	Tk_FreePixmap(tree->display, dPixmap->drawable);
	dPixmap->drawable = Tk_GetPixmap(tree->display,
		Tk_WindowId(tkwin), width, height, Tk_Depth(tkwin));
	dPixmap->width = width;
	dPixmap->height = height;
    }
    return dPixmap->drawable;
}

/*
 *--------------------------------------------------------------
 *
 * CheckPendingHeaderUpdate --
 *
 *	This block of code used to be in Tree_Display but it needs to
 *	be checked wherever Range_RedoIfNeeded is called because it
 *	may set the DINFO_REDO_RANGES flag.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
CheckPendingHeaderUpdate(
    TreeCtrl *tree
    )
{
    TreeDInfo dInfo = tree->dInfo;

    /* DINFO_REDO_COLUMN_WIDTH  - A column was created or deleted. */
    /* DINFO_CHECK_COLUMN_WIDTH - The width, offset or visibility of one or
     * 				  more columns *might* have changed. */
    if (dInfo->flags & (DINFO_REDO_COLUMN_WIDTH | DINFO_CHECK_COLUMN_WIDTH)) {
	TreeColumn treeColumn = tree->columns;
	TreeColumnDInfo dColumn;
	int force = (dInfo->flags & DINFO_REDO_COLUMN_WIDTH) != 0;
	int redoRanges = force, drawItems = force, drawHeader = force;
	int offset, width;

	/* Set max -itembackground as well. */
	tree->columnBgCnt = 0;

	while (treeColumn != NULL) {
	    offset = TreeColumn_Offset(treeColumn);
	    width = TreeColumn_UseWidth(treeColumn);
	    dColumn = TreeColumn_GetDInfo(treeColumn);

	    /* Haven't seen this column before. */
	    if (dColumn == NULL) {
		dColumn = (TreeColumnDInfo) ckalloc(sizeof(TreeColumnDInfo_));
		TreeColumn_SetDInfo(treeColumn, dColumn);
		if (width > 0)
		    redoRanges = drawItems = drawHeader = TRUE;
	    } else {
		/* Changes to observed width also detects column visibililty
		 * changing. */
		if (dColumn->width != width) {
		    redoRanges = drawItems = drawHeader = TRUE;
		} else if ((dColumn->offset != offset) && (width > 0)) {
		    drawItems = drawHeader = TRUE;
		}
	    }
	    dColumn->offset = offset;
	    dColumn->width = width;
	    if (TreeColumn_Visible(treeColumn) &&
		    (TreeColumn_BackgroundCount(treeColumn) > tree->columnBgCnt))
		tree->columnBgCnt = TreeColumn_BackgroundCount(treeColumn);
	    treeColumn = TreeColumn_Next(treeColumn);
	}
	if (redoRanges) dInfo->flags |= DINFO_REDO_RANGES | DINFO_OUT_OF_DATE;
	if (drawHeader) dInfo->flags |= DINFO_DRAW_HEADER;
	if (drawItems)  dInfo->flags |= DINFO_INVALIDATE;
	dInfo->flags &= ~(DINFO_REDO_COLUMN_WIDTH | DINFO_CHECK_COLUMN_WIDTH);
    }
    if (dInfo->headerHeight != Tree_HeaderHeight(tree)) {
	dInfo->headerHeight = Tree_HeaderHeight(tree);
	dInfo->flags |=
	    DINFO_OUT_OF_DATE |
	    DINFO_SET_ORIGIN_Y |
	    DINFO_UPDATE_SCROLLBAR_Y |
	    DINFO_DRAW_HEADER;
	if (tree->vertical && (tree->wrapMode == TREE_WRAP_WINDOW))
	    dInfo->flags |= DINFO_REDO_RANGES;
    }
    if (dInfo->widthOfColumnsLeft != Tree_WidthOfLeftColumns(tree) ||
	    dInfo->widthOfColumnsRight != Tree_WidthOfRightColumns(tree)) {
	dInfo->widthOfColumnsLeft = Tree_WidthOfLeftColumns(tree);
	dInfo->widthOfColumnsRight = Tree_WidthOfRightColumns(tree);
	dInfo->flags |=
	    DINFO_SET_ORIGIN_X |
	    DINFO_UPDATE_SCROLLBAR_X/* |
	    DINFO_OUT_OF_DATE |
	    DINFO_REDO_RANGES |
	    DINFO_DRAW_HEADER*/;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SetBuffering --
 *
 *	Chooses the appropriate level of offscreen buffering depending
 *	on whether we need to draw the dragimage|marquee|proxies in non-XOR.
 *
 * Results:
 *	tree->doubleBuffer is possibly updated.
 *
 * Side effects:
 *	If the buffering level changes then the whole list is redrawn.
 *
 *----------------------------------------------------------------------
 */

static void
SetBuffering(
    TreeCtrl *tree)
{
    TreeDInfo dInfo = tree->dInfo;
    int overlays = FALSE;

    if ((TreeDragImage_IsVisible(tree->dragImage) &&
	!TreeDragImage_IsXOR(tree->dragImage)) ||
	(TreeMarquee_IsVisible(tree->marquee) &&
	!TreeMarquee_IsXOR(tree->marquee)) ||
	((tree->columnProxy.xObj || tree->rowProxy.yObj) &&
	!Proxy_IsXOR())) {

	overlays = TRUE;
    }

#if 1
    tree->doubleBuffer = DOUBLEBUFFER_WINDOW;
#else
#if defined(MAC_OSX_TK) && (TK_MAJOR_VERSION==8) && (TK_MINOR_VERSION>=6)
    /* Do NOT call TkScrollWindow(), it generates an <Expose> event which redraws *all*
     * child windows of children of the toplevel this treectrl is in. See Tk bug 3086887. */
    tree->doubleBuffer = DOUBLEBUFFER_WINDOW;
#else
    if (overlays) {
	tree->doubleBuffer = DOUBLEBUFFER_WINDOW;
    } else {
	tree->doubleBuffer = DOUBLEBUFFER_ITEM;
    }
#endif
#endif

    if (overlays != dInfo->overlays) {
	dInfo->flags |=
	    DINFO_DRAW_HEADER |
	    DINFO_INVALIDATE |
	    DINFO_DRAW_WHITESPACE;
	dInfo->overlays = overlays;
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tree_Display --
 *
 *	This procedure is called at idle time when something has happened
 *	that might require the list to be redisplayed. An effort is made
 *	to only redraw what is needed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in the TreeCtrl window.
 *
 *--------------------------------------------------------------
 */

static void
Tree_Display(
    ClientData clientData	/* Widget info. */
    )
{
    TreeCtrl *tree = clientData;
    TreeDInfo dInfo = tree->dInfo;
    DItem *dItem;
    Tk_Window tkwin = tree->tkwin;
    int didScrollX = 0, didScrollY = 0;
    Drawable drawable;
    TreeDrawable tdrawable;
    TreeRectangle tr;
    int count;
    int numCopy = 0, numDraw = 0;
    TkRegion wsRgnNew, wsRgnDif;
#ifdef COMPLEX_WHITESPACE
    int complexWhitespace;
#endif
    TreeRectangle wsBox;
    int requests;

    if (tree->debug.enable && tree->debug.display && 0)
	dbwin("Tree_Display %s\n", Tk_PathName(tkwin));

    if (tree->deleted) {
	dInfo->flags &= ~(DINFO_REDRAW_PENDING);
	return;
    }

    /* After this point this function must only exit via the displayExit
     * label. */
    Tcl_Preserve((ClientData) tree);
    Tree_PreserveItems(tree);

displayRetry:

    SetBuffering(tree);

    /* Some change requires selection changes */
    if (dInfo->flags & DINFO_REDO_SELECTION) {
#ifdef SELECTION_VISIBLE
	/* Possible <Selection> event. */
	Tree_DeselectHidden(tree);
	if (tree->deleted)
	    goto displayExit;
#endif
	dInfo->flags &= ~(DINFO_REDO_SELECTION);
    }

    Range_RedoIfNeeded(tree);
    Increment_RedoIfNeeded(tree);
    if (dInfo->xOrigin != tree->xOrigin) {
	dInfo->flags |=
	    DINFO_UPDATE_SCROLLBAR_X |
	    DINFO_OUT_OF_DATE |
	    DINFO_DRAW_HEADER;
    }
    if (dInfo->yOrigin != tree->yOrigin) {
	dInfo->flags |=
	    DINFO_UPDATE_SCROLLBAR_Y |
	    DINFO_OUT_OF_DATE;
    }
    if (dInfo->totalWidth != Tree_CanvasWidth(tree)) {
	dInfo->totalWidth = Tree_CanvasWidth(tree);
	dInfo->flags |=
	    DINFO_SET_ORIGIN_X |
	    DINFO_UPDATE_SCROLLBAR_X |
	    DINFO_OUT_OF_DATE;
    }
    if (dInfo->totalHeight != Tree_CanvasHeight(tree)) {
	dInfo->totalHeight = Tree_CanvasHeight(tree);
	dInfo->flags |=
	    DINFO_SET_ORIGIN_Y |
	    DINFO_UPDATE_SCROLLBAR_Y |
	    DINFO_OUT_OF_DATE;
    }
    if (dInfo->flags & DINFO_SET_ORIGIN_X) {
	Tree_SetOriginX(tree, tree->xOrigin);
	dInfo->flags &= ~DINFO_SET_ORIGIN_X;
    }
    if (dInfo->flags & DINFO_SET_ORIGIN_Y) {
	Tree_SetOriginY(tree, tree->yOrigin);
	dInfo->flags &= ~DINFO_SET_ORIGIN_Y;
    }
#ifdef COMPLEX_WHITESPACE
    /* If -itembackground colors are being drawn in the whitespace region,
     * then redraw all the whitespace if:
     * a) scrolling occurs, or
     * b) all the display info was marked as invalid (such as when
     *    -itembackground colors change, or a column moves), or
     * c) item/column sizes change (handled by Range_RedoIfNeeded).
     */
    complexWhitespace = ComplexWhitespace(tree);
    if (complexWhitespace) {
	if ((dInfo->xOrigin != tree->xOrigin) ||
		(dInfo->yOrigin != tree->yOrigin) ||
		(dInfo->flags & DINFO_INVALIDATE)) {
	    dInfo->flags |= DINFO_DRAW_WHITESPACE;
	}
    }
    /* If tree->columnBgCnt was > 0 but is now 0, redraw whitespace. */
    if (complexWhitespace != dInfo->complexWhitespace) {
	dInfo->complexWhitespace = complexWhitespace;
	dInfo->flags |= DINFO_DRAW_WHITESPACE;
    }
#endif
#if COLUMNGRID == 1
    /* If gridlines are drawn in the whitespace region then redraw all the
     * whitespace if:
     * a) horizontal scrolling occurs
     * b) all the display info was marked as invalid (such as when
     *    -itembackground colors change, or a column moves)
     * c) item/column sizes change (handled by Range_RedoIfNeeded).
     */
    if (GridLinesInWhiteSpace(tree)) {
	if ((dInfo->xOrigin != tree->xOrigin)
		|| (dInfo->flags & DINFO_INVALIDATE)) {
	    dInfo->flags |= DINFO_DRAW_WHITESPACE;
	}
    }
#endif
    /*
     * dInfo->requests counts the number of calls to Tree_EventuallyRedraw().
     * If binding scripts do something that causes a redraw to be requested,
     * then we abort the current draw and start again.
     */
    TreeDisplay_GetReadyForTrouble(tree, &requests);
    if (dInfo->flags & DINFO_UPDATE_SCROLLBAR_X) {
	/* Possible <Scroll-x> event. */
	Tree_UpdateScrollbarX(tree);
	dInfo->flags &= ~DINFO_UPDATE_SCROLLBAR_X;
    }
    if (dInfo->flags & DINFO_UPDATE_SCROLLBAR_Y) {
	/* Possible <Scroll-y> event. */
	Tree_UpdateScrollbarY(tree);
	dInfo->flags &= ~DINFO_UPDATE_SCROLLBAR_Y;
    }
    if (tree->deleted || !Tk_IsMapped(tkwin))
	goto displayExit;
    if (TreeDisplay_WasThereTrouble(tree, requests)) {
	goto displayRetry;
    }
    if (dInfo->flags & DINFO_OUT_OF_DATE) {
	Tree_UpdateDInfo(tree);
	dInfo->flags &= ~DINFO_OUT_OF_DATE;
    }
    if (dInfo->flags & DINFO_INVALIDATE) {
	for (dItem = dInfo->dItem; dItem != NULL; dItem = dItem->next) {
	    dItem->area.flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;
	    dItem->left.flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;
	    dItem->right.flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;
	}
	dInfo->flags &= ~DINFO_INVALIDATE;
    }

    /*
     * When an item goes from visible to hidden, "window" elements in the
     * item must be hidden. An item may become hidden because of scrolling,
     * or because an ancestor was collapsed, or because the -visible option
     * of the item changed.
     */
    {
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	TreeItemList newV, newH;
	TreeItem item;
	int isNew, i, count;

	TreeItemList_Init(tree, &newV, 0);
	TreeItemList_Init(tree, &newH, 0);

	for (dItem = dInfo->dItem;
	    dItem != NULL;
	    dItem = dItem->next) {

	    hPtr = Tcl_FindHashEntry(&dInfo->itemVisHash, (char *) dItem->item);
	    if (hPtr == NULL) {
		/* This item is now visible, wasn't before */
		TreeItemList_Append(&newV, dItem->item);
		TreeItem_OnScreen(tree, dItem->item, TRUE);
	    }
#ifdef DCOLUMN
	    /* The item was onscreen and still is. Figure out which
	     * item-columns have become visible or hidden. */
	    else {
		TrackOnScreenColumnsForItem(tree, dItem->item, hPtr);
	    }
#endif /* DCOLUMN */
	}

	hPtr = Tcl_FirstHashEntry(&dInfo->itemVisHash, &search);
	while (hPtr != NULL) {
	    item = (TreeItem) Tcl_GetHashKey(&dInfo->itemVisHash, hPtr);
	    if (TreeItem_GetDInfo(tree, item) == NULL) {
		/* This item was visible but isn't now */
		TreeItemList_Append(&newH, item);
		TreeItem_OnScreen(tree, item, FALSE);
	    }
	    hPtr = Tcl_NextHashEntry(&search);
	}

	/* Remove newly-hidden items from itemVisHash */
	count = TreeItemList_Count(&newH);
	for (i = 0; i < count; i++) {
	    item = TreeItemList_Nth(&newH, i);
	    hPtr = Tcl_FindHashEntry(&dInfo->itemVisHash, (char *) item);
#ifdef DCOLUMN
	    TrackOnScreenColumnsForItem(tree, item, hPtr);
	    ckfree((char *) Tcl_GetHashValue(hPtr));
#endif
	    Tcl_DeleteHashEntry(hPtr);
	}

	/* Add newly-visible items to itemVisHash */
	count = TreeItemList_Count(&newV);
	for (i = 0; i < count; i++) {
	    item = TreeItemList_Nth(&newV, i);
	    hPtr = Tcl_CreateHashEntry(&dInfo->itemVisHash, (char *) item, &isNew);
#ifdef DCOLUMN
	    TrackOnScreenColumnsForItem(tree, item, hPtr);
#endif /* DCOLUMN */
	}

	/*
	 * Generate an <ItemVisibility> event here. This can be used to set
	 * an item's styles when the item is about to be displayed, and to
	 * clear an item's styles when the item is no longer displayed.
	 */
	if (TreeItemList_Count(&newV) || TreeItemList_Count(&newH)) {
	    TreeNotify_ItemVisibility(tree, &newV, &newH);
	}

	TreeItemList_Free(&newV);
	TreeItemList_Free(&newH);

	if (tree->deleted || !Tk_IsMapped(tkwin))
	    goto displayExit;

	if (TreeDisplay_WasThereTrouble(tree, requests))
	    goto displayRetry;
    }

    tdrawable.width = Tk_Width(tkwin);
    tdrawable.height = Tk_Height(tkwin);
    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	tdrawable.drawable = DisplayGetPixmap(tree, &dInfo->pixmapW,
		tdrawable.width, tdrawable.height);
    } else {
	tdrawable.drawable = Tk_WindowId(tkwin);
    }
    drawable = tdrawable.drawable;

    /* XOR off */
    if (Proxy_IsXOR())
	TreeColumnProxy_Undisplay(tree);
    if (Proxy_IsXOR())
	TreeRowProxy_Undisplay(tree);
    if (TreeDragImage_IsXOR(tree->dragImage))
	TreeDragImage_Undisplay(tree->dragImage);
    if (TreeMarquee_IsXOR(tree->marquee))
	TreeMarquee_Undisplay(tree->marquee);

#if REDRAW_RGN == 1
    /* Collect all the pixels that are redrawn below into the redrawRgn.
     * The redrawRgn is used to clip drawing of the marquee and dragimage. */
    Tree_SetEmptyRegion(dInfo->redrawRgn);
#endif /* REDRAW_RGN */

    if (dInfo->flags & DINFO_DRAW_HEADER) {
	if (Tree_AreaBbox(tree, TREE_AREA_HEADER, &tr)) {
	    if (tree->debug.enable && tree->debug.display && tree->debug.drawColor) {
		XFillRectangle(tree->display, Tk_WindowId(tkwin),
			tree->debug.gcDraw,
			TreeRect_Left(tr), TreeRect_Top(tr),
			TreeRect_Width(tr), TreeRect_Height(tr));
		DisplayDelay(tree);
	    }
	    Tree_DrawHeader(tree, tdrawable, C2Wx(0), Tree_HeaderTop(tree));
	    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
		DblBufWinDirty(tree, TreeRect_Left(tr), TreeRect_Top(tr),
		    TreeRect_Right(tr), TreeRect_Bottom(tr));
	    }
#if REDRAW_RGN == 1
/*	    AddRectToRedrawRgn(tree, minX, minY, maxX, maxY); */
#endif /* REDRAW_RGN */
	}
	dInfo->flags &= ~DINFO_DRAW_HEADER;
    }

    if (tree->vertical) {
	numCopy = ScrollVerticalComplex(tree);
	ScrollHorizontalSimple(tree);
    }
    else {
	ScrollVerticalSimple(tree);
	numCopy = ScrollHorizontalComplex(tree);
    }

    /* If we scrolled, then copy the entire pixmap, plus the header
     * if needed. */
    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	if ((dInfo->xOrigin != tree->xOrigin) ||
		(dInfo->yOrigin != tree->yOrigin)) {
	    DblBufWinDirty(tree, Tree_BorderLeft(tree), Tree_ContentTop(tree),
		Tree_BorderRight(tree), Tree_ContentBottom(tree));
	}
    }

    if (dInfo->flags & DINFO_DRAW_WHITESPACE) {
	Tree_SetEmptyRegion(dInfo->wsRgn);
	dInfo->flags &= ~DINFO_DRAW_WHITESPACE;
    }

    didScrollX = dInfo->xOrigin != tree->xOrigin;
    didScrollY = dInfo->yOrigin != tree->yOrigin;

    dInfo->xOrigin = tree->xOrigin;
    dInfo->yOrigin = tree->yOrigin;

    dInfo->flags &= ~(DINFO_REDRAW_PENDING);

    if (tree->backgroundImage != NULL) {
	wsRgnNew = CalcWhiteSpaceRegion(tree);

	/* If we scrolled, redraw entire whitespace area */
	if ((didScrollX /*&& (tree->bgImageScroll & BGIMG_SCROLL_X)*/) ||
		(didScrollY /*&& (tree->bgImageScroll & BGIMG_SCROLL_Y)*/)) {
	    wsRgnDif = wsRgnNew;
	} else {
	    wsRgnDif = Tree_GetRegion(tree);
	    TkSubtractRegion(wsRgnNew, dInfo->wsRgn, wsRgnDif);
	}
	Tree_GetRegionBounds(wsRgnDif, &wsBox);
	if ((wsBox.width > 0) && (wsBox.height > 0)) {
	    TreeDrawable tdPixmap;
	    TreeRectangle trPaint = wsBox;

	    tdPixmap.width = wsBox.width;
	    tdPixmap.height = wsBox.height;
	    tdPixmap.drawable = Tk_GetPixmap(tree->display, Tk_WindowId(tkwin),
		    wsBox.width, wsBox.height, Tk_Depth(tkwin));

	    if (tree->debug.enable && tree->debug.display && tree->debug.drawColor) {
		Tree_FillRegion(tree->display, Tk_WindowId(tkwin),
			tree->debug.gcDraw, wsRgnDif);
		DisplayDelay(tree);
	    }

	    if (!Tree_IsBgImageOpaque(tree)) {
#ifdef COMPLEX_WHITESPACE
		/* The drawable offset from the top-left of the window */
		tree->drawableXOrigin = wsBox.x;
		tree->drawableYOrigin = wsBox.y;
		DrawWhitespace(tree, tdPixmap, wsRgnDif);
#else
		GC gc = Tk_3DBorderGC(tkwin, tree->border, TK_3D_FLAT_GC);
		Tree_OffsetRegion(wsRgnDif, -wsBox.x, -wsBox.y);
		Tree_FillRegion(tree->display, tdPixmap.drawable, gc, wsRgnDif);
		Tree_OffsetRegion(wsRgnDif, wsBox.x, wsBox.y);
#endif
	    }

	    trPaint.x = trPaint.y = 0;
	    Tree_DrawBgImage(tree, tdPixmap, trPaint,
		    W2Cx(wsBox.x), W2Cy(wsBox.y));

	    TkSetRegion(tree->display, tree->copyGC, wsRgnDif);
	    XCopyArea(tree->display, tdPixmap.drawable, drawable, tree->copyGC,
		    0, 0, wsBox.width, wsBox.height,
		    wsBox.x, wsBox.y);
	    XSetClipMask(tree->display, tree->copyGC, None);

	    Tk_FreePixmap(tree->display, tdPixmap.drawable);

#if COLUMNGRID == 1
	    DrawColumnGridLines(tree, tdrawable, wsRgnDif);
#endif

	    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
		DblBufWinDirty(tree, wsBox.x, wsBox.y, wsBox.x + wsBox.width,
			wsBox.y + wsBox.height);
	    }
#if REDRAW_RGN == 1
	    AddRgnToRedrawRgn(tree, wsRgnDif);
#endif /* REDRAW_RGN */
	}
	if (wsRgnDif != wsRgnNew)
	    Tree_FreeRegion(tree, wsRgnDif);
	Tree_FreeRegion(tree, dInfo->wsRgn);
	dInfo->wsRgn = wsRgnNew;
    }

    if (tree->backgroundImage == NULL) {
	/* Calculate the current whitespace region, subtract the old whitespace
	 * region, and fill the difference with the background color. */
	wsRgnNew = CalcWhiteSpaceRegion(tree);
	wsRgnDif = Tree_GetRegion(tree);
	TkSubtractRegion(wsRgnNew, dInfo->wsRgn, wsRgnDif);
	Tree_GetRegionBounds(wsRgnDif, &wsBox);
	if ((wsBox.width > 0) && (wsBox.height > 0)) {
#ifndef COMPLEX_WHITESPACE
	    GC gc = Tk_3DBorderGC(tkwin, tree->border, TK_3D_FLAT_GC);
#endif
	    if (tree->debug.enable && tree->debug.display && tree->debug.drawColor) {
		Tree_FillRegion(tree->display, Tk_WindowId(tkwin),
			tree->debug.gcDraw, wsRgnDif);
		DisplayDelay(tree);
	    }
#ifdef COMPLEX_WHITESPACE
	    /* The drawable offset from the top-left of the window */
	    tree->drawableXOrigin = 0;
	    tree->drawableYOrigin = 0;
	    DrawWhitespace(tree, tdrawable, wsRgnDif);
#else
	    Tree_FillRegion(tree->display, drawable, gc, wsRgnDif);
#endif
	    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
		DblBufWinDirty(tree, wsBox.x, wsBox.y, wsBox.x + wsBox.width,
			wsBox.y + wsBox.height);
	    }
#if COLUMNGRID==1
	    DrawColumnGridLines(tree, tdrawable, wsRgnDif);
#endif
#if REDRAW_RGN == 1
	    AddRgnToRedrawRgn(tree, wsRgnDif);
#endif /* REDRAW_RGN */
	}
	Tree_FreeRegion(tree, wsRgnDif);
	Tree_FreeRegion(tree, dInfo->wsRgn);
	dInfo->wsRgn = wsRgnNew;
    }

    /* See if there are any dirty items */
    count = 0;
    for (dItem = dInfo->dItem;
	 dItem != NULL;
	 dItem = dItem->next) {
	if ((!dInfo->empty && dInfo->rangeFirstD != NULL) &&
		(dItem->area.flags & DITEM_DIRTY)) {
	    count++;
	    break;
	}
	if (!dInfo->emptyL && (dItem->left.flags & DITEM_DIRTY)) {
	    count++;
	    break;
	}
	if (!dInfo->emptyR && (dItem->right.flags & DITEM_DIRTY)) {
	    count++;
	    break;
	}
    }

    /* This is the main loop for redrawing dirty items as well as
     * updating window-element positions.  The positions of window
     * elements must be updated whenever scrolling occurs even if
     * the scrolling did not invalidate any items. */
    if (count > 0 || didScrollX || didScrollY) {
	TreeDrawable tpixmap = tdrawable;

#if USE_ITEM_PIXMAP == 1
	if (count > 0 && tree->doubleBuffer != DOUBLEBUFFER_NONE) {
	    /* Allocate pixmap for largest item */
	    tpixmap.width = MIN(Tk_Width(tkwin), dInfo->itemWidth);
	    tpixmap.height = MIN(Tk_Height(tkwin), dInfo->itemHeight);
	    tpixmap.drawable = DisplayGetPixmap(tree, &dInfo->pixmapI,
		tpixmap.width, tpixmap.height);
	}
#endif

	for (dItem = dInfo->dItem;
	     dItem != NULL;
	     dItem = dItem->next) {

	    int drawn = 0;
	    if (!dInfo->empty && dInfo->rangeFirstD != NULL) {
		tree->drawableXOrigin = tree->xOrigin;
		tree->drawableYOrigin = tree->yOrigin;
		TreeItem_UpdateWindowPositions(tree, dItem->item, COLUMN_LOCK_NONE,
		    dItem->area.x, dItem->y, dItem->area.width, dItem->height);
		if (TreeDisplay_WasThereTrouble(tree, requests)) {
		    if (tree->deleted || !Tk_IsMapped(tree->tkwin))
			goto displayExit;
		    goto displayRetry;
		}
		if (dItem->area.flags & DITEM_DIRTY) {
		    drawn += DisplayDItem(tree, dItem, &dItem->area,
			    COLUMN_LOCK_NONE, dInfo->bounds, tpixmap, tdrawable);
		}
	    } else {
		dItem->area.flags &= ~DITEM_DRAWN;
	    }
	    if (!dInfo->emptyL) {
		tree->drawableXOrigin = tree->xOrigin;
		tree->drawableYOrigin = tree->yOrigin;
		TreeItem_UpdateWindowPositions(tree, dItem->item,
		    COLUMN_LOCK_LEFT, dItem->left.x, dItem->y,
		    dItem->left.width, dItem->height);
		if (TreeDisplay_WasThereTrouble(tree, requests)) {
		    if (tree->deleted || !Tk_IsMapped(tree->tkwin))
			goto displayExit;
		    goto displayRetry;
		}
		if (dItem->left.flags & DITEM_DIRTY) {
		    drawn += DisplayDItem(tree, dItem, &dItem->left, COLUMN_LOCK_LEFT,
			    dInfo->boundsL, tpixmap, tdrawable);
		}
	    } else {
		dItem->left.flags &= ~DITEM_DRAWN;
	    }
	    if (!dInfo->emptyR) {
		tree->drawableXOrigin = tree->xOrigin;
		tree->drawableYOrigin = tree->yOrigin;
		TreeItem_UpdateWindowPositions(tree, dItem->item,
		    COLUMN_LOCK_RIGHT, dItem->right.x, dItem->y,
		    dItem->right.width, dItem->height);
		if (TreeDisplay_WasThereTrouble(tree, requests)) {
		    if (tree->deleted || !Tk_IsMapped(tree->tkwin))
			goto displayExit;
		    goto displayRetry;
		}
		if (dItem->right.flags & DITEM_DIRTY) {
		    drawn += DisplayDItem(tree, dItem, &dItem->right, COLUMN_LOCK_RIGHT,
			    dInfo->boundsR, tpixmap, tdrawable);
		}
	    } else {
		dItem->right.flags &= ~DITEM_DRAWN;
	    }
	    numDraw += drawn ? 1 : 0;

	    dItem->oldX = dItem->area.x; /* FIXME: could have dInfo->empty */
	    dItem->oldY = dItem->y;
	    dItem->oldIndex = dItem->index;
	}
    }

    if (tree->debug.enable && tree->debug.display)
	dbwin("copy %d draw %d %s\n", numCopy, numDraw, Tk_PathName(tkwin));

#if 0 && REDRAW_RGN == 1
    tree->drawableXOrigin = tree->xOrigin;
    tree->drawableYOrigin = tree->yOrigin;
    if (TreeDragImage_IsXOR(tree->dragImage) == FALSE)
	TreeDragImage_DrawClipped(tree->dragImage, tdrawable, dInfo->redrawRgn);
    if (TreeMarquee_IsXOR(tree->marquee) == FALSE)
	TreeMarquee_DrawClipped(tree->marquee, tdrawable, dInfo->redrawRgn);
    Tree_SetEmptyRegion(dInfo->redrawRgn);
#endif /* REDRAW_RGN */

    if (dInfo->overlays) {

	tdrawable.width = Tk_Width(tkwin);
	tdrawable.height = Tk_Height(tkwin);

	if (TreeTheme_IsDesktopComposited(tree)) {
	    tdrawable.drawable = Tk_WindowId(tkwin);
	} else {
	    tdrawable.drawable = DisplayGetPixmap(tree, &dInfo->pixmapT,
		Tk_Width(tree->tkwin), Tk_Height(tree->tkwin));
	}

	/* Copy double-buffer */
	/* FIXME: only copy what is in dirtyRgn plus overlays */
	XCopyArea(tree->display, dInfo->pixmapW.drawable,
	    tdrawable.drawable,
	    tree->copyGC,
	    Tree_BorderLeft(tree), Tree_BorderTop(tree),
	    Tree_BorderRight(tree) - Tree_BorderLeft(tree),
	    Tree_BorderBottom(tree) - Tree_BorderTop(tree),
	    Tree_BorderLeft(tree), Tree_BorderTop(tree));

	/* Draw dragimage|marquee|proxies */
	tree->drawableXOrigin = tree->xOrigin;
	tree->drawableYOrigin = tree->yOrigin;
	if (TreeDragImage_IsXOR(tree->dragImage) == FALSE)
	    TreeDragImage_Draw(tree->dragImage, tdrawable);
	if (TreeMarquee_IsXOR(tree->marquee) == FALSE)
	    TreeMarquee_Draw(tree->marquee, tdrawable);
	if (Proxy_IsXOR() == FALSE)
	    TreeColumnProxy_Draw(tree, tdrawable);
	if (Proxy_IsXOR() == FALSE)
	    TreeRowProxy_Draw(tree, tdrawable);

	if (TreeTheme_IsDesktopComposited(tree) == FALSE) {

	    /* Copy tripple-buffer to window */
	    /* FIXME: only copy what is in dirtyRgn plus overlays */
	    XCopyArea(tree->display, dInfo->pixmapT.drawable,
		Tk_WindowId(tkwin),
		tree->copyGC,
		Tree_BorderLeft(tree), Tree_BorderTop(tree),
		Tree_BorderRight(tree) - Tree_BorderLeft(tree),
		Tree_BorderBottom(tree) - Tree_BorderTop(tree),
		Tree_BorderLeft(tree), Tree_BorderTop(tree));
	}

	Tree_SetEmptyRegion(dInfo->dirtyRgn);
	DisplayDelay(tree);
    }
    else if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	TreeRectangle box;

	drawable = Tk_WindowId(tkwin);

	Tree_GetRegionBounds(dInfo->dirtyRgn, &box);
	if (box.width > 0 && box.height > 0) {
	    TkSetRegion(tree->display, tree->copyGC, dInfo->dirtyRgn);
	    XCopyArea(tree->display, dInfo->pixmapW.drawable, drawable,
		    tree->copyGC,
		    box.x, box.y,
		    box.width, box.height,
		    box.x, box.y);
	    XSetClipMask(tree->display, tree->copyGC, None);
	}
	Tree_SetEmptyRegion(dInfo->dirtyRgn);
	DisplayDelay(tree);
    }

    /* XOR on */
    if (TreeMarquee_IsXOR(tree->marquee))
	TreeMarquee_Display(tree->marquee);
    if (TreeDragImage_IsXOR(tree->dragImage))
	TreeDragImage_Display(tree->dragImage);
    if (Proxy_IsXOR())
	TreeRowProxy_Display(tree);
    if (Proxy_IsXOR())
	TreeColumnProxy_Display(tree);

    if (tree->doubleBuffer == DOUBLEBUFFER_NONE)
	dInfo->flags |= DINFO_DRAW_HIGHLIGHT | DINFO_DRAW_BORDER;

    if (dInfo->flags & (DINFO_DRAW_BORDER | DINFO_DRAW_HIGHLIGHT)) {
	drawable = Tk_WindowId(tkwin);
	if (tree->useTheme && TreeTheme_DrawBorders(tree, drawable) == TCL_OK) {
	    /* nothing */
	} else {

	    /* Draw focus rectangle (outside of 3D-border) */
	    if ((dInfo->flags & DINFO_DRAW_HIGHLIGHT) &&
		    (tree->highlightWidth > 0)) {
		GC fgGC, bgGC;

		DebugDrawBorder(tree, 0, tree->highlightWidth,
			tree->highlightWidth, tree->highlightWidth,
			tree->highlightWidth);

		bgGC = Tk_GCForColor(tree->highlightBgColorPtr, drawable);
		if (tree->gotFocus)
		    fgGC = Tk_GCForColor(tree->highlightColorPtr, drawable);
		else
		    fgGC = bgGC;
		TkpDrawHighlightBorder(tkwin, fgGC, bgGC, tree->highlightWidth,
			drawable);
		dInfo->flags &= ~DINFO_DRAW_HIGHLIGHT;
	    }

	    /* Draw 3D-border (inside of focus rectangle) */
	    if ((dInfo->flags & DINFO_DRAW_BORDER) && (tree->borderWidth > 0)) {
		DebugDrawBorder(tree, tree->highlightWidth,
			tree->borderWidth, tree->borderWidth,
			tree->borderWidth, tree->borderWidth);

		Tk_Draw3DRectangle(tkwin, drawable, tree->border,
			tree->highlightWidth, tree->highlightWidth,
			Tk_Width(tkwin) - tree->highlightWidth * 2,
			Tk_Height(tkwin) - tree->highlightWidth * 2,
			tree->borderWidth, tree->relief);
		dInfo->flags &= ~DINFO_DRAW_BORDER;
	    }
	}
	dInfo->flags &= ~(DINFO_DRAW_BORDER | DINFO_DRAW_HIGHLIGHT);
    }

displayExit:
#if CACHE_BG_IMG
    if (dInfo->pixmapBgImg.drawable != None) {
	Tk_FreePixmap(tree->display, dInfo->pixmapBgImg.drawable);
	dInfo->pixmapBgImg.drawable = None;
    }
#endif
    dInfo->flags &= ~(DINFO_REDRAW_PENDING);
    Tree_ReleaseItems(tree);
    Tcl_Release((ClientData) tree);
}

/*
 *--------------------------------------------------------------
 *
 * A_IncrementFindX --
 *
 *	Return a horizontal scroll position nearest to the given
 *	offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
A_IncrementFindX(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Canvas x-coordinate. */
    )
{
    int totWidth = Tree_CanvasWidth(tree);
    int xIncr = tree->xScrollIncrement;
    int index, indexMax;

    indexMax = totWidth / xIncr;
    if (totWidth % xIncr == 0)
	indexMax--;
    if (offset < 0)
	offset = 0;
    index = offset / xIncr;
    if (index > indexMax)
	index = indexMax;
    return index;
}

/*
 *--------------------------------------------------------------
 *
 * A_IncrementFindY --
 *
 *	Return a vertical scroll position nearest to the given
 *	offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
A_IncrementFindY(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Canvas y-coordinate. */
    )
{
    int totHeight = Tree_CanvasHeight(tree);
    int yIncr = tree->yScrollIncrement;
    int index, indexMax;

    indexMax = totHeight / yIncr;
    if (totHeight % yIncr == 0)
	indexMax--;
    if (offset < 0)
	offset = 0;
    index = offset / yIncr;
    if (index > indexMax)
	index = indexMax;
    return index;
}

int
Tree_FakeCanvasWidth(
    TreeCtrl *tree
    )
{
    TreeDInfo dInfo = tree->dInfo;
    int visWidth, totWidth;
    int oldSmoothing = tree->scrollSmoothing;
    int indexMax, offset;

    Increment_RedoIfNeeded(tree);

    /* Use the cached value, if valid. */
    if (dInfo->fakeCanvasWidth >= 0)
	return dInfo->fakeCanvasWidth;

    totWidth = Tree_CanvasWidth(tree);
    if (totWidth <= 0)
	return dInfo->fakeCanvasWidth = MAX(0, Tree_BorderRight(tree) - Tree_BorderLeft(tree));

    visWidth = Tree_ContentWidth(tree);
    if (visWidth > 1) {
	tree->scrollSmoothing = 0;

	/* Find the increment at the left edge of the content area when
	 * the view is scrolled all the way to the right. */
	indexMax = Increment_FindX(tree, totWidth - visWidth);
	offset = Increment_ToOffsetX(tree, indexMax);

	/* If the increment starts before the left edge of the content area
	 * then use the next increment to the right. */
	if (offset < totWidth - visWidth) {
	    indexMax++;
	    offset = Increment_ToOffsetX(tree, indexMax);
	}

	/* Add some fake content to right. */
	if (offset + visWidth > totWidth)
	    totWidth = offset + visWidth;

	tree->scrollSmoothing = oldSmoothing;
    }
    return dInfo->fakeCanvasWidth = totWidth;
}

int
Tree_FakeCanvasHeight(
    TreeCtrl *tree
    )
{
    TreeDInfo dInfo = tree->dInfo;
    int visHeight, totHeight;
    int oldSmoothing = tree->scrollSmoothing;
    int indexMax, offset;

    Increment_RedoIfNeeded(tree);

    /* Use the cached value, if valid. */
    if (dInfo->fakeCanvasHeight >= 0)
	return dInfo->fakeCanvasHeight;

    totHeight = Tree_CanvasHeight(tree);
    if (totHeight <= 0)
	return dInfo->fakeCanvasHeight = MAX(0, Tree_ContentHeight(tree));

    visHeight = Tree_ContentHeight(tree);
    if (visHeight > 1) {
	tree->scrollSmoothing = 0;

	/* Find the increment at the top edge of the content area when
	 * the view is scrolled all the way down. */
	indexMax = Increment_FindY(tree, totHeight - visHeight);
	offset = Increment_ToOffsetY(tree, indexMax);

	/* If the increment starts before the top edge of the content area
	 * then use the next increment down. */
	if (offset < totHeight - visHeight) {
	    indexMax++;
	    offset = Increment_ToOffsetY(tree, indexMax);
	}

	/* Add some fake content to bottom. */
	if (offset + visHeight > totHeight)
	    totHeight = offset + visHeight;

	tree->scrollSmoothing = oldSmoothing;
    }
    return dInfo->fakeCanvasHeight = totHeight;
}

static int
Smooth_IncrementFindX(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Canvas x-coordinate. */
    )
{
    int totWidth = Tree_FakeCanvasWidth(tree);
    int xIncr = 1;
    int index, indexMax;

    indexMax = totWidth / xIncr;
    if (totWidth % xIncr == 0)
	indexMax--;
    if (offset < 0)
	offset = 0;
    index = offset / xIncr;
    if (index > indexMax)
	index = indexMax;
    return index;
}

static int
Smooth_IncrementFindY(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Canvas y-coordinate. */
    )
{
    int totHeight = Tree_FakeCanvasHeight(tree);
    int yIncr = 1;
    int index, indexMax;

    indexMax = totHeight / yIncr;
    if (totHeight % yIncr == 0)
	indexMax--;
    if (offset < 0)
	offset = 0;
    index = offset / yIncr;
    if (index > indexMax)
	index = indexMax;
    return index;
}

/*
 *--------------------------------------------------------------
 *
 * Increment_FindX --
 *
 *	Return a horizontal scroll position nearest to the given
 *	offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Increment_FindX(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Canvas x-coordinate. */
    )
{
    if (tree->scrollSmoothing & SMOOTHING_X)
	return Smooth_IncrementFindX(tree, offset);
    if (tree->xScrollIncrement <= 0) {
	Increment_RedoIfNeeded(tree);
	return B_IncrementFindX(tree, offset);
    }
    return A_IncrementFindX(tree, offset);
}

/*
 *--------------------------------------------------------------
 *
 * Increment_FindY --
 *
 *	Return a vertical scroll position nearest to the given
 *	offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Increment_FindY(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Canvas y-coordinate. */
    )
{
    if (tree->scrollSmoothing & SMOOTHING_Y)
	return Smooth_IncrementFindY(tree, offset);
    if (tree->yScrollIncrement <= 0) {
	Increment_RedoIfNeeded(tree);
	return B_IncrementFindY(tree, offset);
    }
    return A_IncrementFindY(tree, offset);
}

/*
 *--------------------------------------------------------------
 *
 * Increment_ToOffsetX --
 *
 *	Return the canvas coordinate for a scroll position.
 *
 * Results:
 *	Pixel distance.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Increment_ToOffsetX(
    TreeCtrl *tree,		/* Widget info. */
    int index			/* Index of the increment. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    int xIncr = tree->xScrollIncrement;

    if (tree->scrollSmoothing & SMOOTHING_X)
	return index * 1;
    if (xIncr <= 0) {
	DScrollIncrements *dIncr = &dInfo->xScrollIncrements;
	if (index < 0 || index >= dIncr->count) {
	    panic("Increment_ToOffsetX: bad index %d (must be 0-%d)",
		    index, dIncr->count-1);
	}
	return dIncr->increments[index];
    }
    return index * xIncr;
}

/*
 *--------------------------------------------------------------
 *
 * Increment_ToOffsetY --
 *
 *	Return the canvas coordinate for a scroll position.
 *
 * Results:
 *	Pixel distance.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Increment_ToOffsetY(
    TreeCtrl *tree,		/* Widget info. */
    int index			/* Index of the increment. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    int yIncr = tree->yScrollIncrement;

    if (tree->scrollSmoothing & SMOOTHING_Y)
	return index * 1;
    if (yIncr <= 0) {
	DScrollIncrements *dIncr = &dInfo->yScrollIncrements;
	if (index < 0 || index >= dIncr->count) {
	    panic("Increment_ToOffsetY: bad index %d (must be 0-%d)\ntotHeight %d visHeight %d",
		    index, dIncr->count - 1,
		    Tree_CanvasHeight(tree), Tree_ContentHeight(tree));
	}
	return dIncr->increments[index];
    }
    return index * yIncr;
}

/*
 *--------------------------------------------------------------
 *
 * GetScrollFractions --
 *
 *	Return the fractions that may be passed to a scrollbar "set"
 *	command.
 *
 * Results:
 *	Two fractions from 0.0 to 1.0.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
GetScrollFractions(
    int screen1, int screen2,	/* Min/max coordinates that are visible in
				 * the window. */
    int object1, int object2,	/* Min/max coordinates of the scrollable
				 * content (usually 0 to N where N is the
				 * total width or height of the canvas). */
    double fractions[2]		/* Returned values. */
    )
{
    double range, f1, f2;

    range = object2 - object1;
    if (range <= 0) {
	f1 = 0;
	f2 = 1.0;
    }
    else {
	f1 = (screen1 - object1) / range;
	if (f1 < 0)
	    f1 = 0.0;
	f2 = (screen2 - object1) / range;
	if (f2 > 1.0)
	    f2 = 1.0;
	if (f2 < f1)
	    f2 = f1;
    }

    fractions[0] = f1;
    fractions[1] = f2;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_GetScrollFractionsX --
 *
 *	Return the fractions that may be passed to a scrollbar "set"
 *	command for a horizontal scrollbar.
 *
 * Results:
 *	Two fractions from 0 to 1.0.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_GetScrollFractionsX(
    TreeCtrl *tree,		/* Widget info. */
    double fractions[2]		/* Returned values. */
    )
{
    int left = W2Cx(Tree_ContentLeft(tree));
    int visWidth = Tree_ContentWidth(tree);
    int totWidth = Tree_CanvasWidth(tree);

    /* The tree is empty, or everything fits in the window */
    if (visWidth < 0)
	visWidth = 0;
    if (totWidth <= visWidth) {
	fractions[0] = 0.0;
	fractions[1] = 1.0;
	return;
    }

    if (visWidth <= 1) {
	GetScrollFractions(left, left + 1, 0, totWidth, fractions);
	return;
    }

    totWidth = Tree_FakeCanvasWidth(tree);

    GetScrollFractions(left, left + visWidth, 0, totWidth, fractions);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_GetScrollFractionsY --
 *
 *	Return the fractions that may be passed to a scrollbar "set"
 *	command for a vertical scrollbar.
 *
 * Results:
 *	Two fractions from 0 to 1.0.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_GetScrollFractionsY(
    TreeCtrl *tree,		/* Widget info. */
    double fractions[2]		/* Returned values. */
    )
{
    int top = W2Cy(Tree_ContentTop(tree));
    int visHeight = Tree_ContentHeight(tree);
    int totHeight = Tree_CanvasHeight(tree);

    /* The tree is empty, or everything fits in the window */
    if (visHeight < 0)
	visHeight = 0;
    if (totHeight <= visHeight) {
	fractions[0] = 0.0;
	fractions[1] = 1.0;
	return;
    }

    if (visHeight <= 1) {
	GetScrollFractions(top, top + 1, 0, totHeight, fractions);
	return;
    }

    totHeight = Tree_FakeCanvasHeight(tree);

    GetScrollFractions(top, top + visHeight, 0, totHeight, fractions);
}

void
Tree_SetScrollSmoothingX(
    TreeCtrl *tree,		/* Widget info. */
    int smoothing		/* TRUE or FALSE */
    )
{
    if (smoothing && tree->xScrollSmoothing)
	tree->scrollSmoothing |= SMOOTHING_X;
    else
	tree->scrollSmoothing &= ~SMOOTHING_X;
}

void
Tree_SetScrollSmoothingY(
    TreeCtrl *tree,		/* Widget info. */
    int smoothing		/* TRUE or FALSE */
    )
{
    if (smoothing && tree->yScrollSmoothing)
	tree->scrollSmoothing |= SMOOTHING_Y;
    else
	tree->scrollSmoothing &= ~SMOOTHING_Y;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_SetOriginX --
 *
 *	Change the horizontal scroll position.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the horizontal scroll position changes, then the widget is
 *	redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_SetOriginX(
    TreeCtrl *tree,		/* Widget info. */
    int xOrigin			/* The desired offset from the left edge
				 * of the window to the left edge of the
				 * canvas. The actual value will be clipped
				 * to the nearest scroll increment. */
    )
{
    int totWidth = Tree_CanvasWidth(tree);
    int visWidth = Tree_ContentWidth(tree);
    int index, indexMax, offset;

    /* The tree is empty, or everything fits in the window */
    if (visWidth < 0)
	visWidth = 0;
    if (totWidth <= visWidth) {
	xOrigin = 0 - Tree_ContentLeft(tree);
	if (xOrigin != tree->xOrigin) {
	    tree->xOrigin = xOrigin;
	    Tree_EventuallyRedraw(tree);
	}
	return;
    }

    totWidth = Tree_FakeCanvasWidth(tree);
    if (visWidth > 1) {
	indexMax = Increment_FindX(tree, totWidth - visWidth);
    } else {
	indexMax = Increment_FindX(tree, totWidth);
    }

    xOrigin += Tree_ContentLeft(tree); /* origin -> canvas */
    index = Increment_FindX(tree, xOrigin);

    /* Don't scroll too far left */
    if (index < 0)
	index = 0;

    /* Don't scroll too far right */
    if (index > indexMax)
	index = indexMax;

    offset = Increment_ToOffsetX(tree, index);
    xOrigin = offset - Tree_ContentLeft(tree);

    if (xOrigin == tree->xOrigin)
	return;

    tree->xOrigin = xOrigin;

    Tree_EventuallyRedraw(tree);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_SetOriginY --
 *
 *	Change the vertical scroll position.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the vertical scroll position changes, then the widget is
 *	redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_SetOriginY(
    TreeCtrl *tree,		/* Widget info. */
    int yOrigin			/* The desired offset from the top edge
				 * of the window to the top edge of the
				 * canvas. The actual value will be clipped
				 * to the nearest scroll increment. */
    )
{
    int visHeight = Tree_ContentHeight(tree);
    int totHeight = Tree_CanvasHeight(tree);
    int index, indexMax, offset;

    /* The tree is empty, or everything fits in the window */
    if (visHeight < 0)
	visHeight = 0;
    if (totHeight <= visHeight) {
	yOrigin = 0 - Tree_ContentTop(tree);
	if (yOrigin != tree->yOrigin) {
	    tree->yOrigin = yOrigin;
	    Tree_EventuallyRedraw(tree);
	}
	return;
    }

    totHeight = Tree_FakeCanvasHeight(tree);
    if (visHeight > 1) {
	indexMax = Increment_FindY(tree, totHeight - visHeight);
    } else {
	indexMax = Increment_FindY(tree, totHeight);
    }

    yOrigin += Tree_ContentTop(tree); /* origin -> canvas */
    index = Increment_FindY(tree, yOrigin);

    /* Don't scroll too far up */
    if (index < 0)
	index = 0;

    /* Don't scroll too far down */
    if (index > indexMax)
	index = indexMax;

    offset = Increment_ToOffsetY(tree, index);
    yOrigin = offset - Tree_ContentTop(tree);
    if (yOrigin == tree->yOrigin)
	return;

    tree->yOrigin = yOrigin;

    Tree_EventuallyRedraw(tree);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_GetOriginX --
 *
 *	Return the horizontal scroll position.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May update the horizontal scroll position.
 *	If the horizontal scroll position changes, then the widget is
 *	redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

int
Tree_GetOriginX(
    TreeCtrl *tree		/* Widget info. */
    )
{
    /* Update the value if needed. */
    Tree_SetOriginX(tree, tree->xOrigin);

    return tree->xOrigin;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_GetOriginY --
 *
 *	Return the vertical scroll position.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May update the vertical scroll position.
 *	If the vertical scroll position changes, then the widget is
 *	redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

int
Tree_GetOriginY(
    TreeCtrl *tree		/* Widget info. */
    )
{
    /* Update the value if needed. */
    Tree_SetOriginY(tree, tree->yOrigin);

    return tree->yOrigin;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_EventuallyRedraw --
 *
 *	Schedule an idle task to redisplay the widget, if one is not
 *	already scheduled and the widget is mapped and the widget
 *	hasn't been deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget may be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_EventuallyRedraw(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;

    dInfo->requests++;
    if ((dInfo->flags & DINFO_REDRAW_PENDING) ||
	    tree->deleted ||
	    !Tk_IsMapped(tree->tkwin)) {
	return;
    }
    dInfo->flags |= DINFO_REDRAW_PENDING;
    Tcl_DoWhenIdle(Tree_Display, (ClientData) tree);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_RelayoutWindow --
 *
 *	Invalidate all the layout info for the widget and schedule a
 *	redisplay at idle time. This gets called when certain config
 *	options change and when the size of the widget changes.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget will be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_RelayoutWindow(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;

    FreeDItems(tree, dInfo->dItem, NULL, 0);
    dInfo->dItem = NULL;
    dInfo->flags |=
	DINFO_REDO_RANGES |
	DINFO_OUT_OF_DATE |
	DINFO_CHECK_COLUMN_WIDTH |
	DINFO_DRAW_HEADER |
	DINFO_DRAW_HIGHLIGHT |
	DINFO_DRAW_BORDER |
	DINFO_SET_ORIGIN_X |
	DINFO_SET_ORIGIN_Y |
	DINFO_UPDATE_SCROLLBAR_X |
	DINFO_UPDATE_SCROLLBAR_Y;
    dInfo->xOrigin = tree->xOrigin;
    dInfo->yOrigin = tree->yOrigin;

    /* Needed if -background color changes. */
    dInfo->flags |= DINFO_DRAW_WHITESPACE;

    if (tree->doubleBuffer != DOUBLEBUFFER_WINDOW) {
	if (dInfo->pixmapW.drawable != None) {
	    Tk_FreePixmap(tree->display, dInfo->pixmapW.drawable);
	    dInfo->pixmapW.drawable = None;
	}
    }
    if (tree->doubleBuffer == DOUBLEBUFFER_NONE) {
	if (dInfo->pixmapI.drawable != None) {
	    Tk_FreePixmap(tree->display, dInfo->pixmapI.drawable);
	    dInfo->pixmapI.drawable = None;
	}
    }

    if (tree->useTheme) {
	TreeTheme_Relayout(tree);
	TreeTheme_SetBorders(tree);
    }

    Tree_EventuallyRedraw(tree);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_FocusChanged --
 *
 *	This procedure handles the widget gaining or losing the input
 *	focus. The state of every item has STATE_FOCUS toggled on or
 *	off.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget may be redisplayed at idle time if -highlightthickness
 *	is > 0, or if any Elements change appearance because of the
 *	state change.
 *
 *--------------------------------------------------------------
 */

void
Tree_FocusChanged(
    TreeCtrl *tree,		/* Widget info. */
    int gotFocus		/* TRUE if the widget has the focus,
				 * otherwise FALSE. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    TreeItem item;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    int stateOn, stateOff;

    tree->gotFocus = gotFocus;

    if (gotFocus)
	stateOff = 0, stateOn = STATE_FOCUS;
    else
	stateOff = STATE_FOCUS, stateOn = 0;

    /* Slow. Change state of every item */
    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
    while (hPtr != NULL) {
	item = (TreeItem) Tcl_GetHashValue(hPtr);
	TreeItem_ChangeState(tree, item, stateOff, stateOn);
	hPtr = Tcl_NextHashEntry(&search);
    }

#ifdef USE_TTK
    dInfo->flags |= DINFO_DRAW_HIGHLIGHT;
    Tree_EventuallyRedraw(tree);
#else
    if (tree->highlightWidth > 0) {
	dInfo->flags |= DINFO_DRAW_HIGHLIGHT;
	Tree_EventuallyRedraw(tree);
    }
#endif
}

/*
 *--------------------------------------------------------------
 *
 * Tree_Activate --
 *
 *	This procedure handles the widget's toplevel being the "active"
 *	foreground window (on Macintosh and Windows). Currently it just
 *	redraws the header if -usetheme is TRUE and the header is
 *	visible.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget may be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_Activate(
    TreeCtrl *tree,		/* Widget info. */
    int isActive		/* TRUE if the widget's toplevel is the
				 * active window, otherwise FALSE. */
    )
{
    TreeDInfo dInfo = tree->dInfo;

    tree->isActive = isActive;

    /* TODO: Like Tree_FocusChanged, change state of every item. */
    /* Would need a new item state STATE_ACTIVEWINDOW or something. */
    /* Would want to merge this with Tree_FocusChanged code to avoid
     * double-iteration of items. */

    /* Aqua column header looks different when window is not active */
    if (tree->useTheme && tree->showHeader) {
	dInfo->flags |= DINFO_DRAW_HEADER;
	Tree_EventuallyRedraw(tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tree_FreeItemDInfo --
 *
 *	Free any DItem associated with each item in a range of items.
 *	This is called when the size of an item changed or an item is
 *	deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget will be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_FreeItemDInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item1,		/* First item in the range. */
    TreeItem item2		/* Last item in the range, or NULL. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DItem *dItem;
    TreeItem item = item1;
    int changed = 0;

    while (item != NULL) {
	dItem = (DItem *) TreeItem_GetDInfo(tree, item);
	if (dItem != NULL) {
	    FreeDItems(tree, dItem, dItem->next, 1);
	    changed = 1;
	}
	if (item == item2 || item2 == NULL)
	    break;
	item = TreeItem_Next(tree, item);
    }
    changed = 1;
    if (changed) {
	dInfo->flags |= DINFO_OUT_OF_DATE;
	Tree_EventuallyRedraw(tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tree_InvalidateItemDInfo --
 *
 *	Mark as dirty any DItem associated with each item in a range
 *	of items. This is called when the appearance of an item changed
 *	(but not its size).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget will be redisplayed at idle time if any of the items
 *	had a DItem.
 *
 *--------------------------------------------------------------
 */

void
Tree_InvalidateItemDInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn column,		/* Column to invalidate, or NULL for all. */
    TreeItem item1,		/* First item in the range. */
    TreeItem item2		/* Last item in the range, or NULL. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    TreeColumn column2;
    DItem *dItem;
    TreeItem item = item1;
    int changed = 0;

    if (dInfo->flags & (DINFO_INVALIDATE | DINFO_REDO_COLUMN_WIDTH))
	return;

    while (item != NULL) {
	dItem = (DItem *) TreeItem_GetDInfo(tree, item);
	if ((dItem == NULL) || DItemAllDirty(tree, dItem))
	    goto next;

	if (column == NULL) {
	    dItem->area.flags |= (DITEM_DIRTY | DITEM_ALL_DIRTY);
	    dItem->left.flags |= (DITEM_DIRTY | DITEM_ALL_DIRTY);
	    dItem->right.flags |= (DITEM_DIRTY | DITEM_ALL_DIRTY);
	    changed = 1;
	} else {
	    TreeColumnDInfo dColumn = TreeColumn_GetDInfo(column);
	    int columnIndex, left, width, i;
	    DItemArea *area = NULL;

	    switch (TreeColumn_Lock(column)) {
		case COLUMN_LOCK_NONE:
		    area = &dItem->area;
		    break;
		case COLUMN_LOCK_LEFT:
		    area = &dItem->left;
		    break;
		case COLUMN_LOCK_RIGHT:
		    area = &dItem->right;
		    break;
	    }

	    if (area->flags & DITEM_ALL_DIRTY)
		goto next;

	    columnIndex = TreeColumn_Index(column);
	    left = dColumn->offset;

	    if (TreeColumn_Lock(column) == COLUMN_LOCK_NONE)
		left -= tree->canvasPadX[PAD_TOP_LEFT]; /* canvas -> item coords */

	    /* If only one column is visible, the width may be
	    * different than the column width. */
	    if ((TreeColumn_Lock(column) == COLUMN_LOCK_NONE) &&
		    (tree->columnCountVis == 1)) {
		width = area->width;

	    /* All spans are 1. */
	    } else if (dItem->spans == NULL) {
		width = dColumn->width;

	    /* If the column being redrawn is not the first in the span,
	     * then do nothing. */
	    } else if (columnIndex != dItem->spans[columnIndex]) {
		goto next;

	    /* Calculate the width of the entire span. */
	    /* Do NOT call TreeColumn_UseWidth() or another routine
	     * that calls Tree_WidthOfColumns() because that may end
	     * up recalculating the size of items whose display info
	     * is currently being invalidated. */
	    } else {
		width = 0;
		column2 = column;
		i = columnIndex;
		while (dItem->spans[i] == columnIndex) {
		    width += TreeColumn_GetDInfo(column2)->width;
		    if (++i == tree->columnCount)
			break;
		    column2 = TreeColumn_Next(column2);
		}
	    }

	    if (width > 0) {
		InvalidateDItemX(dItem, area, 0, left, width);
		InvalidateDItemY(dItem, area, 0, 0, dItem->height);
		area->flags |= DITEM_DIRTY;
		changed = 1;
	    }
	}
next:
	if (item == item2 || item2 == NULL)
	    break;
	item = TreeItem_Next(tree, item);
    }
    if (changed) {
	Tree_EventuallyRedraw(tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TreeDisplay_ItemDeleted --
 *
 *	Removes an item from the hash table of on-screen items.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
TreeDisplay_ItemDeleted(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item to remove. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&dInfo->itemVisHash, (char *) item);
    if (hPtr != NULL) {
#ifdef DCOLUMN
	ckfree((char *) Tcl_GetHashValue(hPtr));
#endif
	Tcl_DeleteHashEntry(hPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TreeDisplay_ColumnDeleted --
 *
 *	Removes a column from the list of on-screen columns for
 *	all on-screen items.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
TreeDisplay_ColumnDeleted(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn column		/* Column to remove. */
    )
{
#ifdef DCOLUMN
    TreeDInfo dInfo = tree->dInfo;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    TreeColumn *value;
    int i;

    hPtr = Tcl_FirstHashEntry(&dInfo->itemVisHash, &search);
    while (hPtr != NULL) {
	value = (TreeColumn *) Tcl_GetHashValue(hPtr);
	for (i = 0; value[i] != NULL; i++) {
	    if (value[i] == column) {
		while (value[i] != NULL) {
		    value[i] = value[i + 1];
		    ++i;
		}
		if (tree->debug.enable && tree->debug.span)
		    dbwin("TreeDisplay_ColumnDeleted item %d column %d\n",
			TreeItem_GetID(tree, (TreeItem) Tcl_GetHashKey(
			    &dInfo->itemVisHash, hPtr)),
			TreeColumn_GetID(column));
		break;
	    }
	}
	hPtr = Tcl_NextHashEntry(&search);
    }
#endif
}

/*
 *--------------------------------------------------------------
 *
 * TreeDisplay_FreeColumnDInfo --
 *
 *	Free any display info associated with a column when it is
 *	deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
TreeDisplay_FreeColumnDInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn column		/* Column info. */
    )
{
    TreeColumnDInfo dColumn = TreeColumn_GetDInfo(column);

    if (dColumn != NULL)
	ckfree((char *) dColumn);    
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ShouldDisplayLockedColumns --
 *
 *	Figure out if we are allowed to draw any locked columns.
 *
 * Results:
 *	TRUE if locked columns should be displayed, otherwise FALSE.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tree_ShouldDisplayLockedColumns(
    TreeCtrl *tree		/* Widget info. */
    )
{
    if (!tree->vertical)
	return 0;

    if (tree->wrapMode != TREE_WRAP_NONE)
	return 0;

    Tree_UpdateItemIndex(tree); /* update tree->itemWrapCount */
    if (tree->itemWrapCount > 0)
	return 0;
    
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_DInfoChanged --
 *
 *	Set some DINFO_xxx flags and schedule a redisplay.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget will be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_DInfoChanged(
    TreeCtrl *tree,		/* Widget info. */
    int flags			/* DINFO_xxx flags. */
    )
{
    TreeDInfo dInfo = tree->dInfo;

    dInfo->flags |= flags;
    Tree_EventuallyRedraw(tree);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_InvalidateArea --
 *
 *	Mark as dirty parts of any DItems in the given area. If the given
 *	area overlaps the borders they are marked as needing to be
 *	redrawn.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_InvalidateArea(
    TreeCtrl *tree,		/* Widget info. */
    int x1, int y1,		/* Left & top of dirty area in window
				 * coordinates. */
    int x2, int y2		/* Right & bottom of dirty area in window
				 * coordinates. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DItem *dItem;

    if (x1 >= x2 || y1 >= y2)
	return;

    if ((y2 > Tree_HeaderTop(tree)) && (y1 < Tree_HeaderBottom(tree)))
	dInfo->flags |= DINFO_DRAW_HEADER;

    dItem = dInfo->dItem;
    while (dItem != NULL) {
	if ((!dInfo->empty && (dItem->area.flags & DITEM_DRAWN)/*dInfo->rangeFirstD != NULL*/) &&
		!(dItem->area.flags & DITEM_ALL_DIRTY) &&
		(x2 > dItem->area.x) && (x1 < dItem->area.x + dItem->area.width) &&
		(y2 > dItem->y) && (y1 < dItem->y + dItem->height)) {
	    InvalidateDItemX(dItem, &dItem->area, dItem->area.x, x1, x2 - x1);
	    InvalidateDItemY(dItem, &dItem->area, dItem->y, y1, y2 - y1);
	    dItem->area.flags |= DITEM_DIRTY;
	}
	if (!dInfo->emptyL && !(dItem->left.flags & DITEM_ALL_DIRTY) &&
		(x2 > TreeRect_Left(dInfo->boundsL)) && (x1 < TreeRect_Right(dInfo->boundsL)) &&
		(y2 > dItem->y) && (y1 < dItem->y + dItem->height)) {
	    InvalidateDItemX(dItem, &dItem->left, dItem->left.x, x1, x2 - x1);
	    InvalidateDItemY(dItem, &dItem->left, dItem->y, y1, y2 - y1);
	    dItem->left.flags |= DITEM_DIRTY;
	}
	if (!dInfo->emptyR && !(dItem->right.flags & DITEM_ALL_DIRTY) &&
		(x2 > TreeRect_Left(dInfo->boundsR)) && (x1 < TreeRect_Right(dInfo->boundsR)) &&
		(y2 > dItem->y) && (y1 < dItem->y + dItem->height)) {
	    InvalidateDItemX(dItem, &dItem->right, dItem->right.x, x1, x2 - x1);
	    InvalidateDItemY(dItem, &dItem->right, dItem->y, y1, y2 - y1);
	    dItem->right.flags |= DITEM_DIRTY;
	}
	dItem = dItem->next;
    }

    if ((x1 < Tree_BorderLeft(tree)) ||
	    (y1 < Tree_BorderTop(tree)) ||
	    (x2 > Tree_BorderRight(tree)) ||
	    (y2 > Tree_BorderBottom(tree))) {
	dInfo->flags |= DINFO_DRAW_HIGHLIGHT;
	dInfo->flags |= DINFO_DRAW_BORDER;
    }

    /* Invalidate part of the whitespace */
    InvalidateWhitespace(tree, x1, y1, x2, y2);

    if (tree->debug.enable && tree->debug.display && tree->debug.eraseColor) {
	XFillRectangle(tree->display, Tk_WindowId(tree->tkwin),
		tree->debug.gcErase, x1, y1, x2 - x1, y2 - y1);
	DisplayDelay(tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tree_InvalidateRegion --
 *
 *	Mark as dirty parts of any DItems in the given area. If the given
 *	area overlaps the borders they are marked as needing to be
 *	redrawn.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_InvalidateRegion(
    TreeCtrl *tree,		/* Widget info. */
    TkRegion region		/* Region to mark as dirty, in window
				 * coordinates. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    DItem *dItem;
    TreeRectangle rect;
    int x1, x2, y1, y2;
    TkRegion rgn;

    Tree_GetRegionBounds(region, &rect);
    if (!rect.width || !rect.height)
	return;

    if (Tree_AreaBbox(tree, TREE_AREA_HEADER, &rect) && 
	    TkRectInRegion(region, TreeRect_Left(rect), TreeRect_Top(rect),
		TreeRect_Width(rect), TreeRect_Height(rect))
		!= RectangleOut) {
	dInfo->flags |= DINFO_DRAW_HEADER;
    }

    rgn = Tree_GetRegion(tree);

    dItem = dInfo->dItem;
    while (dItem != NULL) {
	if ((!dInfo->empty && (dItem->area.flags & DITEM_DRAWN)/*dInfo->rangeFirstD != NULL*/) && !(dItem->area.flags & DITEM_ALL_DIRTY)) {
	    rect.x = dItem->area.x;
	    rect.y = dItem->y;
	    rect.width = dItem->area.width;
	    rect.height = dItem->height;
	    Tree_SetRectRegion(rgn, &rect);
	    TkIntersectRegion(region, rgn, rgn);
	    Tree_GetRegionBounds(rgn, &rect);
	    if (rect.width > 0 && rect.height > 0) {
		InvalidateDItemX(dItem, &dItem->area, dItem->area.x, rect.x, rect.width);
		InvalidateDItemY(dItem, &dItem->area, dItem->y, rect.y, rect.height);
		dItem->area.flags |= DITEM_DIRTY;
	    }
	}
	if (!dInfo->emptyL && !(dItem->left.flags & DITEM_ALL_DIRTY)) {
	    rect.x = dItem->left.x;
	    rect.y = dItem->y;
	    rect.width = dItem->left.width;
	    rect.height = dItem->height;
	    Tree_SetRectRegion(rgn, &rect);
	    TkIntersectRegion(region, rgn, rgn);
	    Tree_GetRegionBounds(rgn, &rect);
	    if (rect.width > 0 && rect.height > 0) {
		InvalidateDItemX(dItem, &dItem->left, dItem->left.x, rect.x, rect.width);
		InvalidateDItemY(dItem, &dItem->left, dItem->y, rect.y, rect.height);
		dItem->left.flags |= DITEM_DIRTY;
	    }
	}
	if (!dInfo->emptyR && !(dItem->right.flags & DITEM_ALL_DIRTY)) {
	    rect.x = dItem->right.x;
	    rect.y = dItem->y;
	    rect.width = dItem->right.width;
	    rect.height = dItem->height;
	    Tree_SetRectRegion(rgn, &rect);
	    TkIntersectRegion(region, rgn, rgn);
	    Tree_GetRegionBounds(rgn, &rect);
	    if (rect.width > 0 && rect.height > 0) {
		InvalidateDItemX(dItem, &dItem->right, dItem->right.x, rect.x, rect.width);
		InvalidateDItemY(dItem, &dItem->right, dItem->y, rect.y, rect.height);
		dItem->right.flags |= DITEM_DIRTY;
	    }
	}
	dItem = dItem->next;
    }

    Tree_GetRegionBounds(region, &rect);
    x1 = rect.x, x2 = rect.x + rect.width;
    y1 = rect.y, y2 = rect.y + rect.height;
    if ((x1 < Tree_BorderLeft(tree)) ||
	    (y1 < Tree_BorderTop(tree)) ||
	    (x2 > Tree_BorderRight(tree)) ||
	    (y2 > Tree_BorderBottom(tree))) {
	dInfo->flags |= DINFO_DRAW_HIGHLIGHT;
	dInfo->flags |= DINFO_DRAW_BORDER;
    }

    /* Invalidate part of the whitespace */
    TkSubtractRegion(dInfo->wsRgn, region, dInfo->wsRgn);

    Tree_FreeRegion(tree, rgn);

    if (tree->debug.enable && tree->debug.display && tree->debug.eraseColor) {
	Tree_FillRegion(tree->display, Tk_WindowId(tree->tkwin),
		tree->debug.gcErase, region);
	DisplayDelay(tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tree_InvalidateItemArea --
 *
 *	Mark as dirty parts of any DItems in the given area. This is
 *	like Tree_InvalidateArea() but the given area is clipped inside
 *	the borders/header.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_InvalidateItemArea(
    TreeCtrl *tree,		/* Widget info. */
    int x1, int y1,		/* Left & top of dirty area in window
				 * coordinates. */
    int x2, int y2		/* Right & bottom of dirty area in window
				 * coordinates. */
    )
{
    if (x1 < Tree_ContentLeft(tree))
	x1 = Tree_ContentLeft(tree);
    if (y1 < Tree_ContentTop(tree))
	y1 = Tree_ContentTop(tree);
    if (x2 > Tree_ContentRight(tree))
	x2 = Tree_ContentRight(tree);
    if (y2 > Tree_ContentBottom(tree))
	y2 = Tree_ContentBottom(tree);
    Tree_InvalidateArea(tree, x1, y1, x2, y2);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_InvalidateItemOnScrollX --
 *
 *	Mark an item as needing to be redrawn completely if
 *	the list scrolls horizontally.  This is called when a
 *	gradient whose coordinates aren't canvas-relative
 *	is drawn in the item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_InvalidateItemOnScrollX(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item
    )
{
    DItem *dItem = (DItem *) TreeItem_GetDInfo(tree, item);

    if ((dItem == NULL) || DItemAllDirty(tree, dItem))
	return;

    dItem->flags |= DITEM_INVALIDATE_ON_SCROLL_X;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_InvalidateItemOnScrollY --
 *
 *	Mark an item as needing to be redrawn completely if
 *	the list scrolls vertically.  This is called when a
 *	gradient whose coordinates aren't canvas-relative
 *	is drawn in the item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_InvalidateItemOnScrollY(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item
    )
{
    DItem *dItem = (DItem *) TreeItem_GetDInfo(tree, item);

    if ((dItem == NULL) || DItemAllDirty(tree, dItem))
	return;

    dItem->flags |= DITEM_INVALIDATE_ON_SCROLL_Y;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_RedrawArea --
 *
 *	Mark as dirty parts of any DItems in the given area. If the given
 *	area overlaps the borders they are marked as needing to be
 *	redrawn. The given area is subtracted from the whitespace region
 *	so that that part of the whitespace region will be redrawn.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget will be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_RedrawArea(
    TreeCtrl *tree,		/* Widget info. */
    int x1, int y1,		/* Left & top of dirty area in window
				 * coordinates. */
    int x2, int y2		/* Right & bottom of dirty area in window
				 * coordinates. */
    )
{
    Tree_InvalidateArea(tree, x1, y1, x2, y2);
    Tree_EventuallyRedraw(tree);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ExposeArea --
 *
 *	Called in response to <Expose> events. Causes part of the window
 *	to be redisplayed. With "-doublebuffer window", part of the
 *	offscreen pixmap is marked as needing to be copied but no redrawing
 *	of items is done. Without "-doublebuffer window", items will be
 *	redrawn.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget will be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_ExposeArea(
    TreeCtrl *tree,		/* Widget info. */
    int x1, int y1,		/* Left & top of dirty area in window
				 * coordinates. */
    int x2, int y2		/* Right & bottom of dirty area in window
				 * coordinates. */
    )
{
    TreeDInfo dInfo = tree->dInfo;

    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	if ((x1 < Tree_BorderLeft(tree)) ||
		(y1 < Tree_BorderTop(tree)) ||
		(x2 > Tree_BorderRight(tree)) ||
		(y2 > Tree_BorderBottom(tree))) {
	    dInfo->flags |= DINFO_DRAW_HIGHLIGHT;
	    dInfo->flags |= DINFO_DRAW_BORDER;
	}
	if (x1 < Tree_BorderLeft(tree))
	    x1 = Tree_BorderLeft(tree);
	if (x2 > Tree_BorderRight(tree))
	    x2 = Tree_BorderRight(tree);
	if (y1 < Tree_BorderTop(tree))
	    y1 = Tree_BorderTop(tree);
	if (y2 > Tree_BorderBottom(tree))
	    y2 = Tree_BorderBottom(tree);

	/* Got some 0,0,0,0 expose events from Windows Tk. */
	if (x1 >= x2 || y1 >= y2)
	    return;

	DblBufWinDirty(tree, x1, y1, x2, y2);
	if (tree->debug.enable && tree->debug.display && tree->debug.eraseColor) {
	    XFillRectangle(tree->display, Tk_WindowId(tree->tkwin),
		    tree->debug.gcErase, x1, y1, x2 - x1, y2 - y1);
	    DisplayDelay(tree);
	}
    } else {
	Tree_InvalidateArea(tree, x1, y1, x2, y2);
    }
    Tree_EventuallyRedraw(tree);
}

/*
 *--------------------------------------------------------------
 *
 * TreeDInfo_Init --
 *
 *	Perform display-related initialization when a new TreeCtrl is
 *	created.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *--------------------------------------------------------------
 */

void
TreeDInfo_Init(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo;
    XGCValues gcValues;

    dInfo = (TreeDInfo) ckalloc(sizeof(TreeDInfo_));
    memset(dInfo, '\0', sizeof(TreeDInfo_));
    gcValues.graphics_exposures = True;
    dInfo->scrollGC = Tk_GetGC(tree->tkwin, GCGraphicsExposures, &gcValues);
    dInfo->flags = DINFO_OUT_OF_DATE;
    dInfo->wsRgn = Tree_GetRegion(tree);
    dInfo->dirtyRgn = TkCreateRegion();
    Tcl_InitHashTable(&dInfo->itemVisHash, TCL_ONE_WORD_KEYS);
#if REDRAW_RGN == 1
    dInfo->redrawRgn = TkCreateRegion();
#endif /* REDRAW_RGN */
    tree->dInfo = dInfo;
}

/*
 *--------------------------------------------------------------
 *
 * TreeDInfo_Free --
 *
 *	Free display-related resources for a deleted TreeCtrl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *--------------------------------------------------------------
 */

void
TreeDInfo_Free(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDInfo dInfo = tree->dInfo;
    Range *range = dInfo->rangeFirst;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;

    if (dInfo->rItem != NULL)
	ckfree((char *) dInfo->rItem);
    if (dInfo->rangeLock != NULL)
	ckfree((char *) dInfo->rangeLock);
    while (dInfo->dItem != NULL) {
	DItem *next = dInfo->dItem->next;
	WFREE(dInfo->dItem, DItem);
	dInfo->dItem = next;
    }
    while (dInfo->dItemFree != NULL) {
	DItem *next = dInfo->dItemFree->next;
	WFREE(dInfo->dItemFree, DItem);
	dInfo->dItemFree = next;
    }
    while (range != NULL)
	range = Range_Free(tree, range);
    Tk_FreeGC(tree->display, dInfo->scrollGC);
    if (dInfo->flags & DINFO_REDRAW_PENDING)
	Tcl_CancelIdleCall(Tree_Display, (ClientData) tree);
    if (dInfo->pixmapW.drawable != None)
	Tk_FreePixmap(tree->display, dInfo->pixmapW.drawable);
    if (dInfo->pixmapI.drawable != None)
	Tk_FreePixmap(tree->display, dInfo->pixmapI.drawable);
    if (dInfo->pixmapT.drawable != None)
	Tk_FreePixmap(tree->display, dInfo->pixmapT.drawable);
#if CACHE_BG_IMG
    if (dInfo->pixmapBgImg.drawable != None)
	Tk_FreePixmap(tree->display, dInfo->pixmapBgImg.drawable);
#endif
    if (dInfo->xScrollIncrements.increments != NULL)
	ckfree((char *) dInfo->xScrollIncrements.increments);
    if (dInfo->yScrollIncrements.increments != NULL)
	ckfree((char *) dInfo->yScrollIncrements.increments);
    Tree_FreeRegion(tree, dInfo->wsRgn);
    TkDestroyRegion(dInfo->dirtyRgn);
#ifdef DCOLUMN
    hPtr = Tcl_FirstHashEntry(&dInfo->itemVisHash, &search);
    while (hPtr != NULL) {
	ckfree((char *) Tcl_GetHashValue(hPtr));
	hPtr = Tcl_NextHashEntry(&search);
    }
#endif
    Tcl_DeleteHashTable(&dInfo->itemVisHash);
#if REDRAW_RGN == 1
    TkDestroyRegion(dInfo->redrawRgn);
#endif /* REDRAW_RGN */
    WFREE(dInfo, TreeDInfo_);
}

int
Tree_DumpDInfo(
    TreeCtrl *tree,		/* Widget info. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    Tcl_Interp *interp = tree->interp;
    TreeDInfo dInfo = tree->dInfo;
    Tcl_DString dString;
    DItem *dItem;
    Range *range;
    RItem *rItem;
    int index;

    static CONST char *optionNames[] = {
	"alloc", "ditem", "onscreen", "range", (char *) NULL
    };
#undef DUMP_ALLOC /* [BUG 2233922] SunOS: build error */
    enum { DUMP_ALLOC, DUMP_DITEM, DUMP_ONSCREEN, DUMP_RANGE };

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 3, objv, "option");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], optionNames, "option", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    Tcl_DStringInit(&dString);

    if (index == DUMP_ALLOC) {
	int count = 0, size = 0;
	for (dItem = dInfo->dItem; dItem != NULL; dItem = dItem->next) {
	    count += 1;
	}
	for (dItem = dInfo->dItemFree; dItem != NULL; dItem = dItem->next) {
	    count += 1;
	}
	size = count * sizeof(DItem);
	DStringAppendf(&dString, "%-20s: %8d : %8d B %5d KB\n",
		"DItem", count, size, (size + 1023) / 1024);

	count = dInfo->rItemMax;
	size = count * sizeof(RItem);
	DStringAppendf(&dString, "%-20s: %8d : %8d B %5d KB\n",
		"RItem", count, size, (size + 1023) / 1024);
    }

    if (index == DUMP_DITEM) {
	DStringAppendf(&dString, "DumpDInfo: itemW,H %d,%d totalW,H %d,%d flags 0x%0x vertical %d itemVisCount %d\n",
		dInfo->itemWidth, dInfo->itemHeight,
		dInfo->totalWidth, dInfo->totalHeight,
		dInfo->flags, tree->vertical, tree->itemVisCount);
	DStringAppendf(&dString, "    empty=%d bounds=%d,%d,%d,%d\n", dInfo->empty,
		dInfo->bounds.x, dInfo->bounds.y,
		TreeRect_Right(dInfo->bounds), TreeRect_Bottom(dInfo->bounds));
	DStringAppendf(&dString, "    emptyL=%d boundsL=%d,%d,%d,%d\n", dInfo->emptyL,
		dInfo->boundsL.x, dInfo->boundsL.y,
		TreeRect_Right(dInfo->boundsL), TreeRect_Bottom(dInfo->boundsL));
	DStringAppendf(&dString, "    emptyR=%d boundsR=%d,%d,%d,%d\n", dInfo->emptyR,
		dInfo->boundsR.x, dInfo->boundsR.y,
		TreeRect_Right(dInfo->boundsR), TreeRect_Bottom(dInfo->boundsR));
	dItem = dInfo->dItem;
	while (dItem != NULL) {
	    if (dItem->item == NULL) {
		DStringAppendf(&dString, "    item NULL\n");
	    } else {
		DStringAppendf(&dString, "    item %d x,y,w,h %d,%d,%d,%d dirty %d,%d,%d,%d flags %0X\n",
			TreeItem_GetID(tree, dItem->item),
			dItem->area.x, dItem->y, dItem->area.width, dItem->height,
			dItem->area.dirty[LEFT], dItem->area.dirty[TOP],
			dItem->area.dirty[RIGHT], dItem->area.dirty[BOTTOM],
			dItem->area.flags);
		DStringAppendf(&dString, "       left:  dirty %d,%d,%d,%d flags %0X\n",
			dItem->left.dirty[LEFT], dItem->left.dirty[TOP],
			dItem->left.dirty[RIGHT], dItem->left.dirty[BOTTOM],
			dItem->left.flags);
		DStringAppendf(&dString, "       right: dirty %d,%d,%d,%d flags %0X\n",
			dItem->right.dirty[LEFT], dItem->right.dirty[TOP],
			dItem->right.dirty[RIGHT], dItem->right.dirty[BOTTOM],
			dItem->right.flags);
	    }
	    dItem = dItem->next;
	}
    }

    if (index == DUMP_ONSCREEN) {
	dItem = dInfo->dItem;
	while (dItem != NULL) {
	    Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&dInfo->itemVisHash, (char *) dItem->item);
	    TreeColumn *value = (TreeColumn *) Tcl_GetHashValue(hPtr);
	    DStringAppendf(&dString, "item %d:", TreeItem_GetID(tree, dItem->item));
	    while (*value != NULL) {
		DStringAppendf(&dString, " %d", TreeColumn_GetID(*value));
		++value;
	    }
	    DStringAppendf(&dString, "\n");
	    dItem = dItem->next;
	}
    }

    if (index == DUMP_RANGE) {
	DStringAppendf(&dString, "  dInfo.rangeFirstD %p dInfo.rangeLastD %p dInfo.rangeLock %p\n",
		dInfo->rangeFirstD, dInfo->rangeLastD, dInfo->rangeLock);
	for (range = dInfo->rangeFirstD;
	    range != NULL;
	    range = range->next) {
	    DStringAppendf(&dString, "  Range: x,y,w,h %d,%d,%d,%d\n",
		    range->offset.x, range->offset.y,
		    range->totalWidth, range->totalHeight);
	    if (range == dInfo->rangeLastD)
		break;
	}

	DStringAppendf(&dString, "  dInfo.rangeFirst %p dInfo.rangeLast %p\n",
		dInfo->rangeFirst, dInfo->rangeLast);
	for (range = dInfo->rangeFirst;
	    range != NULL;
	    range = range->next) {
	    DStringAppendf(&dString, "   Range: first %p last %p x,y,w,h %d,%d,%d,%d\n",
		    range->first, range->last,
		    range->offset.x, range->offset.y,
		    range->totalWidth, range->totalHeight, range->offset);
	    rItem = range->first;
	    while (1) {
		DStringAppendf(&dString, "    RItem: item %d index %d offset %d size %d\n",
			TreeItem_GetID(tree, rItem->item), rItem->index, rItem->offset, rItem->size);
		if (rItem == range->last)
		    break;
		rItem++;
	    }
	}
    }

    Tcl_DStringResult(tree->interp, &dString);
    return TCL_OK;
}

