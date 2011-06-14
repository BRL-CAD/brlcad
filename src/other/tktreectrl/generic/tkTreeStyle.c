/* 
 * tkTreeStyle.c --
 *
 *	This module implements styles for treectrl widgets.
 *
 * Copyright (c) 2002-2010 Tim Baker
 *
 * RCS: @(#) $Id$
 */

#include "tkTreeCtrl.h"
#include "tkTreeElem.h"

/* This is the roundUp argument to TreeAlloc_CAlloc. */
#define ELEMENT_LINK_ROUND 1

/* Define this for performance gain and increased memory usage. */
/* When undefined, there is quite a performance hit when elements are
 * squeezable and a style has less than its needed width. The memory
 * savings are not too great when undefined. */
#define CACHE_STYLE_SIZE

/* Define this for performance gain and increased memory usage. */
#define CACHE_ELEM_SIZE

typedef struct MStyle MStyle;
typedef struct IStyle IStyle;
typedef struct MElementLink MElementLink;
typedef struct IElementLink IElementLink;

/*
 * A data structure of the following type is kept for each master style.
 * Master styles are created by the [style create] widget command.
 */
struct MStyle
{
    MStyle *master;		/* Always NULL. Needed to distinguish between
				 * an MStyle and IStyle. */
    Tk_Uid name;		/* Unique identifier. */
    int numElements;		/* Size of elements[]. */
    MElementLink *elements;	/* Array of master elements. */
    int vertical;		/* -orient */
    int buttonY;		/* -buttony */
    Tcl_Obj *buttonYObj;	/* -buttony */
};

/*
 * A data structure of the following type is kept for each instance style.
 * Instance styles are created when a style is assigned to an item-column.
 */
struct IStyle
{
    MStyle *master;		/* Always non-NULL. */
    IElementLink *elements;	/* Array of master or instance elements. */
    int neededWidth;		/* Requested size of this style based on */
    int neededHeight;		/* layout of the elements. */
#ifdef TREECTRL_DEBUG
    int neededState;
#endif
#ifdef CACHE_STYLE_SIZE
    int minWidth;
    int minHeight;
    int layoutWidth;
    int layoutHeight;
#endif
};

#define ELF_eEXPAND_W 0x0001 /* expand Layout.ePadX[0] */
#define ELF_eEXPAND_N 0x0002
#define ELF_eEXPAND_E 0x0004
#define ELF_eEXPAND_S 0x0008
#define ELF_iEXPAND_W 0x0010 /* expand Layout.iPadX[0] */
#define ELF_iEXPAND_N 0x0020
#define ELF_iEXPAND_E 0x0040
#define ELF_iEXPAND_S 0x0080
#define ELF_SQUEEZE_X 0x0100 /* shrink Layout.useWidth if needed */
#define ELF_SQUEEZE_Y 0x0200
#define ELF_DETACH 0x0400
#define ELF_INDENT 0x0800 /* don't layout under button&line area */
#define ELF_STICKY_W 0x1000
#define ELF_STICKY_N 0x2000
#define ELF_STICKY_E 0x4000
#define ELF_STICKY_S 0x8000
#define ELF_iEXPAND_X 0x00010000 /* expand Layout.useWidth */
#define ELF_iEXPAND_Y 0x00020000

#define ELF_eEXPAND_WE (ELF_eEXPAND_W | ELF_eEXPAND_E)
#define ELF_eEXPAND_NS (ELF_eEXPAND_N | ELF_eEXPAND_S)
#define ELF_eEXPAND (ELF_eEXPAND_WE | ELF_eEXPAND_NS)
#define ELF_iEXPAND_WE (ELF_iEXPAND_W | ELF_iEXPAND_E)
#define ELF_iEXPAND_NS (ELF_iEXPAND_N | ELF_iEXPAND_S)
#define ELF_iEXPAND (ELF_iEXPAND_WE | ELF_iEXPAND_NS)
#define ELF_EXPAND_WE (ELF_eEXPAND_WE | ELF_iEXPAND_WE)
#define ELF_EXPAND_NS (ELF_eEXPAND_NS | ELF_iEXPAND_NS)
#define ELF_EXPAND_W (ELF_eEXPAND_W | ELF_iEXPAND_W)
#define ELF_EXPAND_N (ELF_eEXPAND_N | ELF_iEXPAND_N)
#define ELF_EXPAND_E (ELF_eEXPAND_E | ELF_iEXPAND_E)
#define ELF_EXPAND_S (ELF_eEXPAND_S | ELF_iEXPAND_S)
#define ELF_STICKY (ELF_STICKY_W | ELF_STICKY_N | ELF_STICKY_E | ELF_STICKY_S)

#define IS_DETACH(e) (((e)->flags & ELF_DETACH) != 0)
#define IS_UNION(e) ((e)->onion != NULL)
#define DETACH_OR_UNION(e) (IS_DETACH(e) || IS_UNION(e))

/*
 * An array of these is kept for each master style, one per element.
 * Most of the fields are set by the [style layout] widget command.
 */
struct MElementLink
{
    TreeElement elem;		/* Master element. */
    int ePadX[2]; /* external horizontal padding */
    int ePadY[2]; /* external vertical padding */
    int iPadX[2]; /* internal horizontal padding */
    int iPadY[2]; /* internal vertical padding */
    int flags; /* ELF_xxx */
    int *onion, onionCount; /* -union option info */
    int minWidth, fixedWidth, maxWidth;
    int minHeight, fixedHeight, maxHeight;
    PerStateInfo draw; /* -draw */
    PerStateInfo visible; /* -visible */
};

/*
 * An array of these is kept for each instance style, one per element.
 */
struct IElementLink
{
    TreeElement elem;		/* Master or instance element. */
#ifdef CACHE_ELEM_SIZE
    int neededWidth;
    int neededHeight;
    int layoutWidth;
    int layoutHeight;
#endif
};

static CONST char *MStyleUid = "MStyle", *IStyleUid = "IStyle",
    *MElementLinkUid = "MElementLink", *IElementLinkUid = "IElementLink";

static char *orientStringTable[] = { "horizontal", "vertical", (char *) NULL };

static Tk_OptionSpec styleOptionSpecs[] = {
    {TK_OPTION_PIXELS, "-buttony", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(MStyle, buttonYObj),
	Tk_Offset(MStyle, buttonY),
	TK_OPTION_NULL_OK, (ClientData) NULL, 0},
    {TK_OPTION_STRING_TABLE, "-orient", (char *) NULL, (char *) NULL,
	"horizontal", -1, Tk_Offset(MStyle, vertical),
	0, (ClientData) orientStringTable, 0},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

/*
 * The following structure is used to hold layout information about a
 * single element. This information is not cached anywhere.
 */
struct Layout
{
    MElementLink *master;
    IElementLink *eLink;
    int useWidth;
    int useHeight;
#ifndef CACHE_ELEM_SIZE
    int neededWidth;
    int neededHeight;
#endif
    int x;		/* left of ePad */
    int y;		/* above ePad */
    int eWidth;		/* ePad + iPad + useWidth + iPad + ePad */
    int eHeight;	/* ePad + iPad + useHeight + iPad + ePad */
    int iWidth;		/* iPad + useWidth + iPad */
    int iHeight;	/* iPad + useHeight + iPad */
    int ePadX[2];	/* external horizontal padding */
    int ePadY[2];	/* external vertical padding */
    int iPadX[2];	/* internal horizontal padding */
    int iPadY[2];	/* internal vertical padding */
    int uPadX[2];	/* padding due to -union */
    int uPadY[2];	/* padding due to -union */
    int temp;
    int visible;	/* TRUE if the element should be displayed. */
    int unionFirst, unionLast; /* First and last visible elements in this
				* element's -union */
    int unionParent;	/* TRUE if this element is in one or more element's
			 * -union */
};

#define IS_HIDDEN(L) ((L)->visible == 0)

/*
 *----------------------------------------------------------------------
 *
 * Style_DoExpandH --
 *
 *	Add extra horizontal space to an element. The space is
 *	distributed from right to left until all available space
 *	is used or expansion is not possible.
 *
 * Results:
 *	Layout.ePadX, Layout.iPadX, and Layout.useWidth may be
 *	updated. The amount of available space that was used is
 *	returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Style_DoExpandH(
    struct Layout *layout, 	/* Layout to be adjusted. */
    int right			/* Limit of expansion. */
    )
{
    MElementLink *eLink1 = layout->master;
    int flags = eLink1->flags;
    int numExpand = 0, spaceRemaining, spaceUsed = 0;
    int *ePadX, *iPadX, *uPadX;

    if (!(flags & (ELF_EXPAND_WE | ELF_iEXPAND_X)))
	return 0;

    ePadX = layout->ePadX;
    iPadX = layout->iPadX;
    uPadX = layout->uPadX;

    spaceRemaining = right - (layout->x + ePadX[PAD_TOP_LEFT] +
	layout->iWidth + MAX(ePadX[PAD_BOTTOM_RIGHT], uPadX[PAD_BOTTOM_RIGHT]));
    if (spaceRemaining <= 0)
	return 0;

    if (layout->temp)
	numExpand = layout->temp;
    /* For -detach or vertical layout, just set layout->temp to zero */
    else {
	if (flags & ELF_eEXPAND_W) numExpand++;
	if (flags & ELF_iEXPAND_W) numExpand++;
	if (flags & ELF_iEXPAND_X) {
	    if ((eLink1->maxWidth < 0) ||
		(eLink1->maxWidth > layout->useWidth))
		numExpand++;
	}
	if (flags & ELF_iEXPAND_E) numExpand++;
	if (flags & ELF_eEXPAND_E) numExpand++;
    }

    while ((spaceRemaining > 0) && (numExpand > 0)) {
	int each = (spaceRemaining >= numExpand) ? (spaceRemaining / numExpand) : 1;

	numExpand = 0;

	/* Allocate extra space to the *right* padding first so that any
	 * extra single pixel is given to the right. */

	if (flags & ELF_eEXPAND_E) {
	    int add = each;
	    ePadX[PAD_BOTTOM_RIGHT] += add;
	    layout->eWidth += add;
	    spaceRemaining -= add;
	    spaceUsed += add;
	    if (!spaceRemaining)
		break;
	    numExpand++;
	}

	if (flags & ELF_iEXPAND_E) {
	    int add = each;
	    iPadX[PAD_BOTTOM_RIGHT] += add;
	    layout->iWidth += add;
	    layout->eWidth += add;
	    spaceRemaining -= add;
	    spaceUsed += add;
	    if (!spaceRemaining)
		break;
	    numExpand++;
	}

	if (flags & ELF_iEXPAND_X) {
	    int max = eLink1->maxWidth;
	    if ((max < 0) || (layout->useWidth < max)) {
		int add = (max < 0) ? each : MIN(each, max - layout->useWidth);
		layout->useWidth += add;
		layout->iWidth += add;
		layout->eWidth += add;
		spaceRemaining -= add;
		spaceUsed += add;
		if ((max >= 0) && (max == layout->useWidth))
		    layout->temp--;
		if (!spaceRemaining)
		    break;
		if ((max < 0) || (max > layout->useWidth))
		    numExpand++;
	    }
	}

	if (flags & ELF_iEXPAND_W) {
	    int add = each;
	    iPadX[PAD_TOP_LEFT] += add;
	    layout->iWidth += add;
	    layout->eWidth += add;
	    spaceRemaining -= add;
	    spaceUsed += add;
	    if (!spaceRemaining)
		break;
	    numExpand++;
	}

	if (flags & ELF_eEXPAND_W) {
	    int add = each;
	    ePadX[PAD_TOP_LEFT] += add;
	    layout->eWidth += add;
	    spaceRemaining -= add;
	    spaceUsed += add;
	    if (!spaceRemaining)
		break;
	    numExpand++;
	}
    }

    return spaceUsed;
}

/*
 *----------------------------------------------------------------------
 *
 * Style_DoExpandV --
 *
 *	Add extra vertical space to an element. The space is
 *	distributed from bottom to top until all available space
 *	is used or expansion is not possible.
 *
 * Results:
 *	Layout.ePadY, Layout.iPadY, and Layout.useHeight may be
 *	updated. The amount of available space that was used is
 *	returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Style_DoExpandV(
    struct Layout *layout,	/* Layout to be adjusted. */
    int bottom			/* Limit of expansion. */
    )
{
    MElementLink *eLink1 = layout->master;
    int flags = eLink1->flags;
    int numExpand = 0, spaceRemaining, spaceUsed = 0;
    int *ePadY, *iPadY, *uPadY;

    if (!(flags & (ELF_EXPAND_NS | ELF_iEXPAND_Y)))
	return 0;

    ePadY = layout->ePadY;
    iPadY = layout->iPadY;
    uPadY = layout->uPadY;

    spaceRemaining = bottom - (layout->y + ePadY[PAD_TOP_LEFT] +
	layout->iHeight + MAX(ePadY[PAD_BOTTOM_RIGHT], uPadY[PAD_BOTTOM_RIGHT]));
    if (spaceRemaining <= 0)
	return 0;

    if (layout->temp)
	numExpand = layout->temp;
    /* For -detach or vertical layout, just set layout->temp to zero */
    else {
	if (flags & ELF_eEXPAND_N) numExpand++;
	if (flags & ELF_iEXPAND_N) numExpand++;
	if (flags & ELF_iEXPAND_Y) {
	    if ((eLink1->maxHeight < 0) ||
		(eLink1->maxHeight > layout->useHeight))
		numExpand++;
	}
	if (flags & ELF_iEXPAND_S) numExpand++;
	if (flags & ELF_eEXPAND_S) numExpand++;
    }

    while ((spaceRemaining > 0) && (numExpand > 0)) {
	int each = (spaceRemaining >= numExpand) ? (spaceRemaining / numExpand) : 1;

	numExpand = 0;

	/* Allocate extra space to the *bottom* padding first so that any
	 * extra single pixel is given to the bottom. */

	if (flags & ELF_eEXPAND_S) {
	    int add = each;
	    ePadY[PAD_BOTTOM_RIGHT] += add;
	    layout->eHeight += add;
	    spaceRemaining -= add;
	    spaceUsed += add;
	    if (!spaceRemaining)
		break;
	    numExpand++;
	}

	if (flags & ELF_iEXPAND_S) {
	    int add = each;
	    iPadY[PAD_BOTTOM_RIGHT] += add;
	    layout->iHeight += add;
	    layout->eHeight += add;
	    spaceRemaining -= add;
	    spaceUsed += add;
	    if (!spaceRemaining)
		break;
	    numExpand++;
	}

	if (flags & ELF_iEXPAND_Y) {
	    int max = eLink1->maxHeight;
	    if ((max < 0) || (layout->useHeight < max)) {
		int add = (max < 0) ? each : MIN(each, max - layout->useHeight);
		layout->useHeight += add;
		layout->iHeight += add;
		layout->eHeight += add;
		spaceRemaining -= add;
		spaceUsed += add;
		if ((max >= 0) && (max == layout->useHeight))
		    layout->temp--;
		if (!spaceRemaining)
		    break;
		if ((max < 0) || (max > layout->useHeight))
		    numExpand++;
	    }
	}

	if (flags & ELF_iEXPAND_N) {
	    int add = each;
	    iPadY[PAD_TOP_LEFT] += add;
	    layout->iHeight += add;
	    layout->eHeight += add;
	    spaceRemaining -= add;
	    spaceUsed += add;
	    if (!spaceRemaining)
		break;
	    numExpand++;
	}

	if (flags & ELF_eEXPAND_N) {
	    int add = each;
	    ePadY[PAD_TOP_LEFT] += add;
	    layout->eHeight += add;
	    spaceRemaining -= add;
	    spaceUsed += add;
	    if (!spaceRemaining)
		break;
	    numExpand++;
	}
    }

    return spaceUsed;
}

/*
 *----------------------------------------------------------------------
 *
 * ElementLink_NeededSize --
 *
 *	Calculate the needed width and height of an element.
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
Element_NeededSize(
    TreeCtrl *tree,		/* Widget info. */
    MElementLink *eLink1,	/* Master style layout info. */
    TreeElement elem,		/* Master/Instance element. */
    int state,			/* STATE_xxx flags. */
    int *widthPtr,		/* Out: width */
    int *heightPtr		/* Out: height */
    )
{
    TreeElementArgs args;
    int width, height;

    if ((eLink1->fixedWidth >= 0) && (eLink1->fixedHeight >= 0)) {
	width = eLink1->fixedWidth;
	height = eLink1->fixedHeight;
    } else {
	args.tree = tree;
	args.state = state;
	args.elem = elem;
	args.needed.fixedWidth = eLink1->fixedWidth;
	args.needed.fixedHeight = eLink1->fixedHeight;
	if (eLink1->maxWidth > eLink1->minWidth)
	    args.needed.maxWidth = eLink1->maxWidth;
	else
	    args.needed.maxWidth = -1;
	if (eLink1->maxHeight > eLink1->minHeight)
	    args.needed.maxHeight = eLink1->maxHeight;
	else
	    args.needed.maxHeight = -1;
	(*args.elem->typePtr->neededProc)(&args);
	width = args.needed.width;
	height = args.needed.height;

	if (eLink1->fixedWidth >= 0)
	    width = eLink1->fixedWidth;
	else if ((eLink1->minWidth >= 0) &&
	    (width < eLink1->minWidth))
	    width = eLink1->minWidth;
	else if ((eLink1->maxWidth >= 0) &&
	    (width > eLink1->maxWidth))
	    width = eLink1->maxWidth;

	if (eLink1->fixedHeight >= 0)
	    height = eLink1->fixedHeight;
	else if ((eLink1->minHeight >= 0) &&
	    (height < eLink1->minHeight))
	    height = eLink1->minHeight;
	else if ((eLink1->maxHeight >= 0) &&
	    (height > eLink1->maxHeight))
	    height = eLink1->maxHeight;
    }

    *widthPtr = width;
    *heightPtr = height;
}

/*
 *----------------------------------------------------------------------
 *
 * Layout_CalcVisibility --
 *
 *	Recursively calculate the visibility of each element.
 *
 * Results:
 *	Layout.visible is set for each element if it wasn't done
 *	already.  For -union elements the first and last visible
 *	elements in the union are determined.  If there are no
 *	visible elements in the union then the union element
 *	itself is marked 'not visible'.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Layout_CalcVisibility(
    TreeCtrl *tree,
    int state,
    MStyle *masterStyle,
    struct Layout layouts[],
    int iElem
    )
{
    struct Layout *layout = &layouts[iElem];
    MElementLink *eLink = &masterStyle->elements[iElem];
    int i, visible = 0;

    if (layout->temp != 0)
	return; /* Already did this one */
    layout->temp = 1;

    layout->visible = PerStateBoolean_ForState(tree, &eLink->visible,
	state, NULL) != 0;

    if (IS_HIDDEN(layout) || !IS_UNION(eLink))
	return;

    /* Remember the first and last visible elements surrounded by
     * this -union element. */
    layout->unionFirst = layout->unionLast = -1;

    for (i = 0; i < eLink->onionCount; i++) {
	struct Layout *layout2 = &layouts[eLink->onion[i]];
	Layout_CalcVisibility(tree, state, masterStyle, layouts, eLink->onion[i]);
	if (!IS_HIDDEN(layout2)) {
	    if (layout->unionFirst == -1)
		layout->unionFirst = eLink->onion[i];
	    layout->unionLast =  eLink->onion[i];
	    visible++;
	}
    }

    /* If there are no visible elements surrounded by this -union
     * element, then hide it. */
    if (visible == 0)
	layout->visible = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Layout_AddUnionPadding --
 *
 *	Recursively determine the amount of -union padding around
 *	each element that is part of a -union.
 *
 * Results:
 *	If the element isn't a -union element itself then the
 *	Layout.uPadX and Layout.uPadY fields are updated.
 *	If the element is a -union element itself then its padding
 *	is added to the total padding and passed on to its -union
 *	elements recursively.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Layout_AddUnionPadding(
    MStyle *masterStyle,		/* Style being layed out. */
    struct Layout layouts[],		/* Layout info for every element. */
    int iElemParent,			/* Whose -union iElem is in. */
    int iElem,				/* The element to update. */
    const int totalPadX[2],		/* The cumulative padding around */
    const int totalPadY[2]		/* iElemParent plus the -ipad */
					/* padding of iElemParent itself. */
    )
{
    struct Layout *layoutP = &layouts[iElemParent];
    struct Layout *layout = &layouts[iElem];
    MElementLink *eLink = &masterStyle->elements[iElem];
    int *ePadX, *ePadY, *iPadX, *iPadY, *uPadX, *uPadY;
    int padX[2], padY[2];
    int i;

#ifdef TREECTRL_DEBUG
    if (IS_HIDDEN(layoutP) || IS_HIDDEN(layout))
	panic("Layout_AddUnionPadding: element is hidden");
#endif

    uPadX = layout->uPadX;
    uPadY = layout->uPadY;

    if (masterStyle->vertical) {
	uPadX[PAD_TOP_LEFT] = MAX(uPadX[PAD_TOP_LEFT], totalPadX[PAD_TOP_LEFT]);
	uPadX[PAD_BOTTOM_RIGHT] = MAX(uPadX[PAD_BOTTOM_RIGHT], totalPadX[PAD_BOTTOM_RIGHT]);
	if (iElem == layoutP->unionFirst) /* topmost */
	    uPadY[PAD_TOP_LEFT] = MAX(uPadY[PAD_TOP_LEFT], totalPadY[PAD_TOP_LEFT]);
	if (iElem == layoutP->unionLast) /* bottommost */
	    uPadY[PAD_BOTTOM_RIGHT] = MAX(uPadY[PAD_BOTTOM_RIGHT], totalPadY[PAD_BOTTOM_RIGHT]);
    } else {
	if (iElem == layoutP->unionFirst) /* leftmost */
	    uPadX[PAD_TOP_LEFT] = MAX(uPadX[PAD_TOP_LEFT], totalPadX[PAD_TOP_LEFT]);
	if (iElem == layoutP->unionLast) /* rightmost */
	    uPadX[PAD_BOTTOM_RIGHT] = MAX(uPadX[PAD_BOTTOM_RIGHT], totalPadX[PAD_BOTTOM_RIGHT]);
	uPadY[PAD_TOP_LEFT] = MAX(uPadY[PAD_TOP_LEFT], totalPadY[PAD_TOP_LEFT]);
	uPadY[PAD_BOTTOM_RIGHT] = MAX(uPadY[PAD_BOTTOM_RIGHT], totalPadY[PAD_BOTTOM_RIGHT]);
    }

    if (!IS_UNION(eLink)){
	return;
    }

    ePadX = layout->ePadX;
    ePadY = layout->ePadY;
    iPadX = layout->iPadX;
    iPadY = layout->iPadY;

    for (i = 0; i < 2; i++) {
	padX[i] = MAX(totalPadX[i], ePadX[i]) + iPadX[i];
	padY[i] = MAX(totalPadY[i], ePadY[i]) + iPadY[i];
    }
    for (i = 0; i < eLink->onionCount; i++) {
	struct Layout *layout2 = &layouts[eLink->onion[i]];
	if (IS_HIDDEN(layout2))
	    continue;
	Layout_AddUnionPadding(masterStyle, layouts, iElem, eLink->onion[i],
	    padX, padY);
    }
}

    /* Expand -union elements if needed: horizontal. */
    /* Expansion of "-union" elements is different than non-"-union" elements.
     * Expanding a -union element never changes the size or position of any
     * element other than the -union element itself. */
static void
Layout_ExpandUnionH(
    StyleDrawArgs *drawArgs,		/* Various args. */
    MStyle *masterStyle,		/* Style being layed out. */
    struct Layout layouts[],		/* Layout info for every element. */
    int iElem				/* The element to update. */
    )
{
    struct Layout *layout = &layouts[iElem];
    MElementLink *eLink1 = &masterStyle->elements[iElem];
    int *ePadX, *iPadX, *uPadX;
    int extraWidth, x;

#ifdef TREECTRL_DEBUG
    if (IS_HIDDEN(layout))
	panic("Layout_ExpandUnionH: element is hidden");
    if (!IS_UNION(eLink1))
	panic("Layout_ExpandUnionH: element is !union");
#endif

    if (!(eLink1->flags & ELF_EXPAND_WE))
	return;

    if (drawArgs->width - (layout->eWidth + drawArgs->indent) <= 0)
	return;

    ePadX = layout->ePadX;
    iPadX = layout->iPadX;
    uPadX = layout->uPadX;

    x = layout->x + ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]);
    extraWidth = x - drawArgs->indent;
    if ((extraWidth > 0) && (eLink1->flags & ELF_EXPAND_W)) {

	/* External and internal expansion: W */
	if ((eLink1->flags & ELF_EXPAND_W) == ELF_EXPAND_W) {
	    int eExtra = extraWidth / 2;
	    int iExtra = extraWidth - extraWidth / 2;

	    layout->x = drawArgs->indent + uPadX[PAD_TOP_LEFT];

	    /* External expansion */
	    ePadX[PAD_TOP_LEFT] += eExtra;
	    layout->eWidth += extraWidth;

	    /* Internal expansion */
	    iPadX[PAD_TOP_LEFT] += iExtra;
	    layout->iWidth += iExtra;
	}

	/* External expansion only: W */
	else if (eLink1->flags & ELF_eEXPAND_W) {
	    ePadX[PAD_TOP_LEFT] += extraWidth;
	    layout->x = drawArgs->indent + uPadX[PAD_TOP_LEFT];
	    layout->eWidth += extraWidth;
	}

	/* Internal expansion only: W */
	else {
	    iPadX[PAD_TOP_LEFT] += extraWidth;
	    layout->x = drawArgs->indent + uPadX[PAD_TOP_LEFT];
	    layout->iWidth += extraWidth;
	    layout->eWidth += extraWidth;
	}
    }

    x = layout->x + layout->eWidth - ePadX[PAD_BOTTOM_RIGHT] + MAX(ePadX[PAD_BOTTOM_RIGHT], uPadX[PAD_BOTTOM_RIGHT]);
    extraWidth = drawArgs->width - x;
    if ((extraWidth > 0) && (eLink1->flags & ELF_EXPAND_E)) {

	/* External and internal expansion: E */
	if ((eLink1->flags & ELF_EXPAND_E) == ELF_EXPAND_E) {
	    int eExtra = extraWidth / 2;
	    int iExtra = extraWidth - extraWidth / 2;

	    /* External expansion */
	    ePadX[PAD_BOTTOM_RIGHT] += eExtra;
	    layout->eWidth += extraWidth; /* all the space */

	    /* Internal expansion */
	    iPadX[PAD_BOTTOM_RIGHT] += iExtra;
	    layout->iWidth += iExtra;
	}

	/* External expansion only: E */
	else if (eLink1->flags & ELF_eEXPAND_E) {
	    ePadX[PAD_BOTTOM_RIGHT] += extraWidth;
	    layout->eWidth += extraWidth;
	}

	/* Internal expansion only: E */
	else {
	    iPadX[PAD_BOTTOM_RIGHT] += extraWidth;
	    layout->iWidth += extraWidth;
	    layout->eWidth += extraWidth;
	}
    }
}

/* Expand -union elements if needed: vertical */
static void
Layout_ExpandUnionV(
    StyleDrawArgs *drawArgs,		/* Various args. */
    MStyle *masterStyle,		/* Style being layed out. */
    struct Layout layouts[],		/* Layout info for every element. */
    int iElem				/* The element to update. */
    )
{
    struct Layout *layout = &layouts[iElem];
    MElementLink *eLink1 = &masterStyle->elements[iElem];
    int *ePadY, *iPadY, *uPadY;
    int extraHeight, y;

#ifdef TREECTRL_DEBUG
    if (IS_HIDDEN(layout))
	panic("Layout_ExpandUnionV: element is hidden");
    if (!IS_UNION(eLink1))
	panic("Layout_ExpandUnionV: element is !union");
#endif

    if (!(eLink1->flags & ELF_EXPAND_NS))
	return;

    if (drawArgs->height - layout->eHeight <= 0)
	return;

    ePadY = layout->ePadY;
    iPadY = layout->iPadY;
    uPadY = layout->uPadY;

    y = layout->y + ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]);
    extraHeight = y;
    if ((extraHeight > 0) && (eLink1->flags & ELF_EXPAND_N)) {

	/* External and internal expansion: N */
	if ((eLink1->flags & ELF_EXPAND_N) == ELF_EXPAND_N) {
	    int eExtra = extraHeight / 2;
	    int iExtra = extraHeight - extraHeight / 2;

	    /* External expansion */
	    ePadY[PAD_TOP_LEFT] += eExtra;
	    layout->y = uPadY[PAD_TOP_LEFT];
	    layout->eHeight += extraHeight;

	    /* Internal expansion */
	    iPadY[PAD_TOP_LEFT] += iExtra;
	    layout->iHeight += iExtra;
	}

	/* External expansion only: N */
	else if (eLink1->flags & ELF_eEXPAND_N) {
	    ePadY[PAD_TOP_LEFT] += extraHeight;
	    layout->y = uPadY[PAD_TOP_LEFT];
	    layout->eHeight += extraHeight;
	}

	/* Internal expansion only: N */
	else {
	    iPadY[PAD_TOP_LEFT] += extraHeight;
	    layout->y = uPadY[PAD_TOP_LEFT];
	    layout->iHeight += extraHeight;
	    layout->eHeight += extraHeight;
	}
    }

    y = layout->y + layout->eHeight - ePadY[PAD_BOTTOM_RIGHT] + MAX(ePadY[PAD_BOTTOM_RIGHT], uPadY[PAD_BOTTOM_RIGHT]);
    extraHeight = drawArgs->height - y;
    if ((extraHeight > 0) && (eLink1->flags & ELF_EXPAND_S)) {

	/* External and internal expansion: S */
	if ((eLink1->flags & ELF_EXPAND_S) == ELF_EXPAND_S) {
	    int eExtra = extraHeight / 2;
	    int iExtra = extraHeight - extraHeight / 2;

	    /* External expansion */
	    ePadY[PAD_BOTTOM_RIGHT] += eExtra;
	    layout->eHeight += extraHeight; /* all the space */

	    /* Internal expansion */
	    iPadY[PAD_BOTTOM_RIGHT] += iExtra;
	    layout->iHeight += iExtra;
	}

	/* External expansion only: S */
	else if (eLink1->flags & ELF_eEXPAND_S) {
	    ePadY[PAD_BOTTOM_RIGHT] += extraHeight;
	    layout->eHeight += extraHeight;
	}

	/* Internal expansion only */
	else {
	    iPadY[PAD_BOTTOM_RIGHT] += extraHeight;
	    layout->iHeight += extraHeight;
	    layout->eHeight += extraHeight;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Layout_CalcUnionLayoutH --
 *
 *	Recursively calculate the horizontal size and position of
 *	a -union element.
 *
 * Results:
 *	The Layout record for the element is updated by getting the
 *	horizontal bounds of each element in its -union (after they
 *	have their size and position calculated). Then expansion
 *	is performed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Layout_CalcUnionLayoutH(
    StyleDrawArgs *drawArgs,		/* Various args. */
    MStyle *masterStyle,		/* Style being layed out. */
    struct Layout layouts[],		/* Layout info for every element. */
    int iElem				/* The element to update. */
    )
{
    struct Layout *layout = &layouts[iElem];
    MElementLink *eLink = &masterStyle->elements[iElem];
    int *ePadX, *iPadX;
    int i, w, e;

#ifdef TREECTRL_DEBUG
    if (IS_HIDDEN(layout))
	panic("Layout_CalcUnionLayoutH: element is hidden");
#endif

    if (!IS_UNION(eLink))
	return;

    w = 1000000, e = -1000000;

    for (i = 0; i < eLink->onionCount; i++) {
	struct Layout *layout2 = &layouts[eLink->onion[i]];
	if (IS_HIDDEN(layout2))
	    continue;
	Layout_CalcUnionLayoutH(drawArgs, masterStyle, layouts, eLink->onion[i]);
	w = MIN(w, layout2->x + layout2->ePadX[PAD_TOP_LEFT]);
	e = MAX(e, layout2->x + layout2->ePadX[PAD_TOP_LEFT] + layout2->iWidth);
    }

    ePadX = layout->ePadX;
    iPadX = layout->iPadX;

    layout->x = w - iPadX[PAD_TOP_LEFT] - ePadX[PAD_TOP_LEFT];
    layout->useWidth = (e - w);
    layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
    layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];

    Layout_ExpandUnionH(drawArgs, masterStyle, layouts, iElem);
}

/*
 *----------------------------------------------------------------------
 *
 * Layout_CalcUnionLayoutV --
 *
 *	Recursively calculate the vertical size and position of
 *	a -union element.
 *
 * Results:
 *	The Layout record for the element is updated by getting the
 *	vertical bounds of each element in its -union (after they
 *	have their size and position calculated). Then expansion
 *	is performed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Layout_CalcUnionLayoutV(
    StyleDrawArgs *drawArgs,		/* Various args. */
    MStyle *masterStyle,
    struct Layout layouts[],
    int iElem
    )
{
    struct Layout *layout = &layouts[iElem];
    MElementLink *eLink = &masterStyle->elements[iElem];
    int *ePadY, *iPadY;
    int i, n, s;

#ifdef TREECTRL_DEBUG
    if (IS_HIDDEN(layout))
	panic("Layout_CalcUnionLayoutV: element is hidden");
#endif

    if (!IS_UNION(eLink))
	return;

    n = 1000000, s = -1000000;

    for (i = 0; i < eLink->onionCount; i++) {
	struct Layout *layout2 = &layouts[eLink->onion[i]];
	if (IS_HIDDEN(layout2))
	    continue;
	Layout_CalcUnionLayoutV(drawArgs, masterStyle, layouts, eLink->onion[i]);
	n = MIN(n, layout2->y + layout2->ePadY[PAD_TOP_LEFT]);
	s = MAX(s, layout2->y + layout2->ePadY[PAD_TOP_LEFT] + layout2->iHeight);
    }

    ePadY = layout->ePadY;
    iPadY = layout->iPadY;

    layout->y = n - iPadY[PAD_TOP_LEFT] - ePadY[PAD_TOP_LEFT];
    layout->useHeight = (s - n);
    layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
    layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

    Layout_ExpandUnionV(drawArgs, masterStyle, layouts, iElem);
}

/*
 *----------------------------------------------------------------------
 *
 * Style_DoLayoutH --
 *
 *	Calculate the horizontal size and position of each element.
 *	This gets called if the style -orient option is horizontal or
 *	vertical.
 *
 * Results:
 *	layouts[] is updated.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Style_DoLayoutH(
    StyleDrawArgs *drawArgs,	/* Various args. */
    struct Layout layouts[]	/* Array of layout records to be
				 * filled in, one per element. Should be
				 * uninitialized. */
    )
{
    IStyle *style = (IStyle *) drawArgs->style;
    MStyle *masterStyle = style->master;
    MElementLink *eLinks1, *eLink1;
    IElementLink *eLinks2, *eLink2;
    int x = drawArgs->indent;
    int *ePadX, *iPadX, *uPadX, *ePadY, *iPadY;
    int numExpandWE = 0;
    int numSqueezeX = 0;
    int i, j, eLinkCount = 0;
    int rightEdge = 0;

    eLinks1 = masterStyle->elements;
    eLinks2 = style->elements;
    eLinkCount = masterStyle->numElements;

    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];
	layout->unionParent = 0;
	layout->temp = 0; /* see Layout_CalcVisibility */
    }

    for (i = 0; i < eLinkCount; i++) {
	Layout_CalcVisibility(drawArgs->tree, drawArgs->state, masterStyle,
	    layouts, i);
    }

    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];
	eLink2 = &eLinks2[i];

	if (IS_UNION(eLink1)) {
	    for (j = 0; j < eLink1->onionCount; j++) {
		struct Layout *layout2 = &layouts[eLink1->onion[j]];
		if (!IS_HIDDEN(layout2)) {
		    layout2->unionParent = 1;
		}
	    }
	}

	layout->eLink = eLink2;
	layout->master = eLink1;

	/* Width before squeezing/expanding */
	if (IS_UNION(eLink1)) {
	    layout->useWidth = 0;
	} else {
#ifdef CACHE_ELEM_SIZE
	    layout->useWidth = eLink2->neededWidth;
#else
	    Element_NeededSize(drawArgs->tree, eLink1, eLink2->elem,
		    drawArgs->state, &layout->neededWidth, &layout->neededHeight);
	    layout->useWidth = layout->neededWidth;
#endif
	}

	for (j = 0; j < 2; j++) {
	    /* Pad values before expansion */
	    layout->ePadX[j] = eLink1->ePadX[j];
	    layout->ePadY[j] = eLink1->ePadY[j];
	    layout->iPadX[j] = eLink1->iPadX[j];
	    layout->iPadY[j] = eLink1->iPadY[j];

	    /* No -union padding yet */
	    layout->uPadX[j] = 0;
	    layout->uPadY[j] = 0;
	}

	/* Count all non-union, non-detach squeezeable elements */
	if (DETACH_OR_UNION(eLink1))
	    continue;
	if (eLink1->flags & ELF_SQUEEZE_X)
	    numSqueezeX++;
    }

    /*
    e1 {
	e2 <----------
	e3 {
	    e7
	    e4 {
		e2 <----------
		e5
	    }
	}
    }
    */
    /* Calculate the padding around elements surrounded by -union elements */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];
	int padx[2], pady[2];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];

	/* Start at the top level of each -union heirarchy. */
	if (!IS_UNION(eLink1) || layout->unionParent)
	    continue;

	ePadX = eLink1->ePadX;
	ePadY = eLink1->ePadY;
	iPadX = eLink1->iPadX;
	iPadY = eLink1->iPadY;

	for (j = 0; j < 2; j++) {
	    padx[j] = ePadX[j] + iPadX[j];
	    pady[j] = ePadY[j] + iPadY[j];
	}
	for (j = 0; j < eLink1->onionCount; j++) {
	    struct Layout *layout2 = &layouts[eLink1->onion[j]];
	    if (IS_HIDDEN(layout2))
		continue;
	    Layout_AddUnionPadding(masterStyle, layouts, i, eLink1->onion[j],
		padx, pady);
	}
    }

    /* Left-to-right layout. Make the width of some elements less than they
     * need */
    if (!masterStyle->vertical &&
	    (drawArgs->width < style->neededWidth + drawArgs->indent) &&
	    (numSqueezeX > 0)) {
	int numSqueeze = numSqueezeX;
	int spaceRemaining  = (style->neededWidth + drawArgs->indent) - drawArgs->width;

	while ((spaceRemaining > 0) && (numSqueeze > 0)) {
	    int each = (spaceRemaining >= numSqueeze) ? (spaceRemaining / numSqueeze) : 1;

	    numSqueeze = 0;
	    for (i = 0; i < eLinkCount; i++) {
		struct Layout *layout = &layouts[i];
		int min = 0;

		if (IS_HIDDEN(layout))
		    continue;

		eLink1 = &eLinks1[i];

		if (DETACH_OR_UNION(eLink1))
		    continue;

		if (!(eLink1->flags & ELF_SQUEEZE_X))
		    continue;

		if (eLink1->minWidth >= 0)
		    min = eLink1->minWidth;
		if (layout->useWidth > min) {
		    int sub = MIN(each, layout->useWidth - min);
		    layout->useWidth -= sub;
		    spaceRemaining -= sub;
		    if (!spaceRemaining) break;
		    if (layout->useWidth > min)
			numSqueeze++;
		}
	    }
	}
    }

    /* Reduce the width of all non-union elements, except for the
     * cases handled above. */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];
	int width, subtract;

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];

	if (IS_UNION(eLink1))
	    continue;

	if (!(eLink1->flags & ELF_SQUEEZE_X))
	    continue;

	if (!IS_DETACH(eLink1) && !masterStyle->vertical)
	    continue;

	ePadX = eLink1->ePadX;
	iPadX = eLink1->iPadX;
	uPadX = layout->uPadX;

	width =
	    MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]) +
	    iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT] +
	    MAX(ePadX[PAD_BOTTOM_RIGHT], uPadX[PAD_BOTTOM_RIGHT]);
	subtract = width - drawArgs->width;

	if (!IS_DETACH(eLink1) || (eLink1->flags & ELF_INDENT))
	    subtract += drawArgs->indent;

	if (subtract > 0) {
	    if ((eLink1->minWidth >= 0) &&
		(eLink1->minWidth <= layout->useWidth) &&
		(layout->useWidth - subtract < eLink1->minWidth))
		layout->useWidth = eLink1->minWidth;
	    else
		layout->useWidth -= subtract;
	}
    }

    /* Layout elements left-to-right */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];
	int right;

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];
	eLink2 = &eLinks2[i];

	if (DETACH_OR_UNION(eLink1))
	    continue;

	ePadX = eLink1->ePadX;
	iPadX = eLink1->iPadX;
	uPadX = layout->uPadX;

	layout->x = x + abs(ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]));
	layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
	layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];

	right = layout->x + layout->ePadX[PAD_TOP_LEFT] +
	    layout->iWidth +
	    MAX(ePadX[PAD_BOTTOM_RIGHT], uPadX[PAD_BOTTOM_RIGHT]);

	rightEdge = MAX(rightEdge, right);
	if (!masterStyle->vertical)
	    x = layout->x + layout->eWidth;

	/* Count number that want to expand */
	if (eLink1->flags & (ELF_EXPAND_WE | ELF_iEXPAND_X))
	    numExpandWE++;
    }

    /* Left-to-right layout. Expand some elements horizontally if we have
     * more space available horizontally than is needed by the Style. */
    if (!masterStyle->vertical &&
	(drawArgs->width > rightEdge) &&
	(numExpandWE > 0)) {
	int numExpand = 0;
	int spaceRemaining = drawArgs->width - rightEdge;

	/* Each element has 5 areas that can optionally expand. */
	for (i = 0; i < eLinkCount; i++) {
	    struct Layout *layout = &layouts[i];

	    if (IS_HIDDEN(layout))
		continue;

	    eLink1 = &eLinks1[i];

	    layout->temp = 0;

	    if (DETACH_OR_UNION(eLink1))
		continue;

	    if (eLink1->flags & ELF_eEXPAND_W) layout->temp++;
	    if (eLink1->flags & ELF_iEXPAND_W) layout->temp++;
	    if (eLink1->flags & ELF_iEXPAND_X) {
		if ((eLink1->maxWidth < 0) ||
		    (eLink1->maxWidth > layout->useWidth))
		    layout->temp++;
	    }
	    if (eLink1->flags & ELF_iEXPAND_E) layout->temp++;
	    if (eLink1->flags & ELF_eEXPAND_E) layout->temp++;

	    numExpand += layout->temp;
	}

	while ((spaceRemaining > 0) && (numExpand > 0)) {
	    int each = (spaceRemaining >= numExpand) ? spaceRemaining / numExpand : 1;

	    numExpand = 0;
	    for (i = 0; i < eLinkCount; i++) {
		struct Layout *layout = &layouts[i];
		int spaceUsed;

		if (IS_HIDDEN(layout))
		    continue;

		if (!layout->temp)
		    continue;

		eLink1 = &eLinks1[i];

		spaceUsed = Style_DoExpandH(layout,
		    layout->x + layout->ePadX[PAD_TOP_LEFT] + layout->iWidth +
		    MAX(layout->ePadX[PAD_BOTTOM_RIGHT], layout->uPadX[PAD_BOTTOM_RIGHT]) +
		    MIN(each * layout->temp, spaceRemaining));

		if (spaceUsed) {
		    /* Shift following elements to the right */
		    for (j = i + 1; j < eLinkCount; j++)
			if (!DETACH_OR_UNION(&eLinks1[j]))
			    layouts[j].x += spaceUsed;

		    rightEdge += spaceUsed;

		    spaceRemaining -= spaceUsed;
		    if (!spaceRemaining)
			break;

		    numExpand += layout->temp;
		} else
		    layout->temp = 0;
	    }
	}
    }

    /* Top-to-bottom layout. Expand some elements horizontally */
    if (masterStyle->vertical && (numExpandWE > 0)) {
	for (i = 0; i < eLinkCount; i++) {
	    struct Layout *layout = &layouts[i];
	    int right;

	    if (IS_HIDDEN(layout))
		continue;

	    eLink1 = &eLinks1[i];

	    if (DETACH_OR_UNION(eLink1))
		continue;

	    layout->temp = 0;
	    Style_DoExpandH(layout, drawArgs->width);

	    right = layout->x + layout->ePadX[PAD_TOP_LEFT] +
		layout->iWidth +
		MAX(layout->ePadX[PAD_BOTTOM_RIGHT], layout->uPadX[PAD_BOTTOM_RIGHT]);
	    rightEdge = MAX(rightEdge, right);
	}
    }

    /* Now handle column justification */
    /* All the non-union, non-detach elements are moved as a group */
    if (drawArgs->width > rightEdge) {
	int dx = drawArgs->width - rightEdge;

	for (i = 0; i < eLinkCount; i++) {
	    struct Layout *layout = &layouts[i];

	    if (IS_HIDDEN(layout))
		continue;

	    eLink1 = &eLinks1[i];

	    if (DETACH_OR_UNION(eLink1))
		continue;

	    switch (drawArgs->justify) {
		case TK_JUSTIFY_LEFT:
		    break;
		case TK_JUSTIFY_RIGHT:
		    layout->x += dx;
		    break;
		case TK_JUSTIFY_CENTER:
		    layout->x += dx / 2;
		    break;
	    }
	}
    }

    /* Position and expand -detach elements */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];
	eLink2 = &eLinks2[i];

	if (!IS_DETACH(eLink1) || IS_UNION(eLink1))
	    continue;

	ePadX = eLink1->ePadX;
	iPadX = eLink1->iPadX;
	uPadX = layout->uPadX;

	layout->x = abs(ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]));
	if (eLink1->flags & ELF_INDENT)
	    layout->x += drawArgs->indent;
	layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
	layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];

	layout->temp = 0;
	Style_DoExpandH(layout, drawArgs->width);
    }

    /* Position and expand -union elements. */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];

	/* Start at the top level of each -union heirarchy. */
	if (!IS_UNION(eLink1) || layout->unionParent)
	    continue;

	Layout_CalcUnionLayoutH(drawArgs, masterStyle, layouts, i);
    }

    /* Add internal padding to display area for -union elements */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];

	if (!IS_UNION(eLink1))
	    continue;

	iPadX = layout->iPadX;

	layout->useWidth += iPadX[PAD_TOP_LEFT] + iPadX[PAD_BOTTOM_RIGHT];
	iPadX[PAD_TOP_LEFT] = iPadX[PAD_BOTTOM_RIGHT] = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Style_DoLayoutV --
 *
 *	Calculate the vertical size and position of each element.
 *	This gets called if the style -orient option is horizontal or
 *	vertical.
 *
 * Results:
 *	layouts[] is updated.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Style_DoLayoutV(
    StyleDrawArgs *drawArgs,	/* Various args. */
    struct Layout layouts[]	/* Array of layout records to be updated,
				 * one per element. Should be initialized
				 * by Style_DoLayoutH(). */
    )
{
    IStyle *style = (IStyle *) drawArgs->style;
    MStyle *masterStyle = style->master;
    MElementLink *eLinks1, *eLink1;
    IElementLink *eLinks2, *eLink2;
    int y = 0;
    int *ePadY, *iPadY, *uPadY;
    int numExpandNS = 0;
    int numSqueezeY = 0;
    int i, j, eLinkCount = 0;
    int bottomEdge = 0;

    eLinks1 = masterStyle->elements;
    eLinks2 = style->elements;
    eLinkCount = masterStyle->numElements;

    for (i = 0; i < eLinkCount; i++) {
	if (IS_HIDDEN(&layouts[i]))
	    continue;

	eLink1 = &eLinks1[i];

	/* Count all non-union, non-detach squeezeable elements */
	if (DETACH_OR_UNION(eLink1))
	    continue;
	if (eLink1->flags & ELF_SQUEEZE_Y)
	    numSqueezeY++;
    }

    /* Top-top-bottom layout. Make the height of some elements less than they
     * need */
    if (masterStyle->vertical &&
	    (drawArgs->height < style->neededHeight) &&
	    (numSqueezeY > 0)) {
	int numSqueeze = numSqueezeY;
	int spaceRemaining  = style->neededHeight - drawArgs->height;

	while ((spaceRemaining > 0) && (numSqueeze > 0)) {
	    int each = (spaceRemaining >= numSqueeze) ? (spaceRemaining / numSqueeze) : 1;

	    numSqueeze = 0;
	    for (i = 0; i < eLinkCount; i++) {
		struct Layout *layout = &layouts[i];
		int min = 0;

		if (IS_HIDDEN(layout))
		    continue;

		eLink1 = &eLinks1[i];

		if (DETACH_OR_UNION(eLink1))
		    continue;

		if (!(eLink1->flags & ELF_SQUEEZE_Y))
		    continue;

		if (eLink1->minHeight >= 0)
		    min = eLink1->minHeight;
		if (layout->useHeight > min) {
		    int sub = MIN(each, layout->useHeight - min);
		    layout->useHeight -= sub;
		    spaceRemaining -= sub;
		    if (!spaceRemaining) break;
		    if (layout->useHeight > min)
			numSqueeze++;
		}
	    }
	}
    }

    /* Reduce the height of all non-union elements, except for the
     * cases handled above. */
    if (drawArgs->height < style->neededHeight) {
	for (i = 0; i < eLinkCount; i++) {
	    struct Layout *layout = &layouts[i];
	    int height, subtract;

	    if (IS_HIDDEN(layout))
		continue;

	    eLink1 = &eLinks1[i];

	    if (IS_UNION(eLink1))
		continue;

	    if (!(eLink1->flags & ELF_SQUEEZE_Y))
		continue;

	    if (!IS_DETACH(eLink1) && masterStyle->vertical)
		continue;

	    ePadY = eLink1->ePadY;
	    iPadY = eLink1->iPadY;
	    uPadY = layout->uPadY;

	    height =
		MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]) +
		iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT] +
		MAX(ePadY[PAD_BOTTOM_RIGHT], uPadY[PAD_BOTTOM_RIGHT]);
	    subtract = height - drawArgs->height;

	    if (subtract > 0) {
		if ((eLink1->minHeight >= 0) &&
		    (eLink1->minHeight <= layout->useHeight) &&
		    (layout->useHeight - subtract < eLink1->minHeight))
		    layout->useHeight = eLink1->minHeight;
		else
		    layout->useHeight -= subtract;
	    }
	}
    }

    /* Layout elements top-to-bottom */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];
	int bottom;

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];
	eLink2 = &eLinks2[i];

	if (DETACH_OR_UNION(eLink1))
	    continue;

	ePadY = eLink1->ePadY;
	iPadY = eLink1->iPadY;
	uPadY = layout->uPadY;

	layout->y = y + abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
	layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
	layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

	if (masterStyle->vertical)
	    y = layout->y + layout->eHeight;

	if (masterStyle->vertical) {
	    bottom = layout->y + layout->ePadY[PAD_TOP_LEFT] +
		layout->iHeight +
		MAX(ePadY[PAD_BOTTOM_RIGHT], uPadY[PAD_BOTTOM_RIGHT]);
	    bottomEdge = MAX(bottomEdge, bottom);
	}

	/* Count number that want to expand */
	if (eLink1->flags & (ELF_EXPAND_NS | ELF_iEXPAND_Y))
	    numExpandNS++;
    }

    /* Top-to-bottom layout. Expand some elements vertically if we have
     * more space available vertically than is needed by the Style. */
    if (masterStyle->vertical &&
	(drawArgs->height > bottomEdge) &&
	(numExpandNS > 0)) {
	int numExpand = 0;
	int spaceRemaining = drawArgs->height - bottomEdge;

	for (i = 0; i < eLinkCount; i++) {
	    struct Layout *layout = &layouts[i];

	    if (IS_HIDDEN(layout))
		continue;

	    eLink1 = &eLinks1[i];

	    layout->temp = 0;

	    if (DETACH_OR_UNION(eLink1))
		continue;

	    if (eLink1->flags & ELF_eEXPAND_N) layout->temp++;
	    if (eLink1->flags & ELF_iEXPAND_N) layout->temp++;
	    if (eLink1->flags & ELF_iEXPAND_Y) {
		if ((eLink1->maxHeight < 0) ||
		    (eLink1->maxHeight > layout->useHeight))
		    layout->temp++;
	    }
	    if (eLink1->flags & ELF_iEXPAND_S) layout->temp++;
	    if (eLink1->flags & ELF_eEXPAND_S) layout->temp++;

	    numExpand += layout->temp;
	}

	while ((spaceRemaining > 0) && (numExpand > 0)) {
	    int each = (spaceRemaining >= numExpand) ? spaceRemaining / numExpand : 1;

	    numExpand = 0;
	    for (i = 0; i < eLinkCount; i++) {
		struct Layout *layout = &layouts[i];
		int spaceUsed;

		if (IS_HIDDEN(layout))
		    continue;

		if (!layout->temp)
		    continue;

		eLink1 = &eLinks1[i];

		spaceUsed = Style_DoExpandV(layout,
		    layout->y + layout->ePadY[PAD_TOP_LEFT] + layout->iHeight +
		    MAX(layout->ePadY[PAD_BOTTOM_RIGHT], layout->uPadY[PAD_BOTTOM_RIGHT]) +
		    MIN(each * layout->temp, spaceRemaining));

		if (spaceUsed) {
		    /* Shift following elements down */
		    for (j = i + 1; j < eLinkCount; j++)
			if (!DETACH_OR_UNION(&eLinks1[j]))
			    layouts[j].y += spaceUsed;

		    spaceRemaining -= spaceUsed;
		    if (!spaceRemaining)
			break;

		    numExpand += layout->temp;
		} else
		    layout->temp = 0;
	    }
	}
    }

    /* Left-to-right layout. Expand some elements vertically */
    if (!masterStyle->vertical && (numExpandNS > 0)) {
	for (i = 0; i < eLinkCount; i++) {
	    struct Layout *layout = &layouts[i];

	    if (IS_HIDDEN(layout))
		continue;

	    eLink1 = &eLinks1[i];

	    if (DETACH_OR_UNION(eLink1))
		continue;

	    layout->temp = 0;
	    Style_DoExpandV(layout, drawArgs->height);
	}
    }

    /* Position and expand -detach elements */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];
	eLink2 = &eLinks2[i];

	if (!IS_DETACH(eLink1) || IS_UNION(eLink1))
	    continue;

	ePadY = eLink1->ePadY;
	iPadY = eLink1->iPadY;
	uPadY = layout->uPadY;

	layout->y = abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
	layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
	layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

	layout->temp = 0;
	Style_DoExpandV(layout, drawArgs->height);
    }

    /* Now calculate layout of -union elements. */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];

	/* Start at the top level of each -union heirarchy. */
	if (!IS_UNION(eLink1) || layout->unionParent)
	    continue;

	Layout_CalcUnionLayoutV(drawArgs, masterStyle, layouts, i);
    }

    /* Add internal padding to display area for -union elements */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];

	if (!IS_UNION(eLink1))
	    continue;

	iPadY = layout->iPadY;

	layout->useHeight += iPadY[PAD_TOP_LEFT] + iPadY[PAD_BOTTOM_RIGHT];
	iPadY[PAD_TOP_LEFT] = iPadY[PAD_BOTTOM_RIGHT] = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Layout_Size --
 *
 *	Calculate the height and width of a style after all the
 *	elements have been arranged.
 *
 * Results:
 *	The height and width of the style.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Layout_Size(
    int vertical,		/* TRUE if elements are arranged from top
				 * to bottom. */
    int numLayouts,		/* Number of layout records. */
    struct Layout layouts[],	/* Initialized layout records. */
    int *widthPtr,		/* Returned width. */
    int *heightPtr		/* Returned height. */
    )
{
    int i, W, N, E, S;
    int width = 0, height = 0;

    W = 1000000, N = 1000000, E = -1000000, S = -1000000;

    for (i = 0; i < numLayouts; i++) {
	struct Layout *layout = &layouts[i];
	int w, n, e, s;
	int *ePadX, *iPadX, *uPadX, *ePadY, *iPadY, *uPadY;

	if (IS_HIDDEN(layout))
	    continue;

	ePadX = layout->ePadX, iPadX = layout->iPadX, uPadX = layout->uPadX;
	ePadY = layout->ePadY, iPadY = layout->iPadY, uPadY = layout->uPadY;

	w = layout->x + ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]);
	n = layout->y + ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]);
	e = layout->x + layout->eWidth - ePadX[PAD_BOTTOM_RIGHT] + MAX(ePadX[PAD_BOTTOM_RIGHT], uPadX[PAD_BOTTOM_RIGHT]);
	s = layout->y + layout->eHeight - ePadY[PAD_BOTTOM_RIGHT] + MAX(ePadY[PAD_BOTTOM_RIGHT], uPadY[PAD_BOTTOM_RIGHT]);

	if (vertical) {
	    N = MIN(N, n);
	    S = MAX(S, s);
	    width = MAX(width, e - w);
	} else {
	    W = MIN(W, w);
	    E = MAX(E, e);
	    height = MAX(height, s - n);
	}
    }

    if (vertical)
	height = MAX(height, S - N);
    else
	width = MAX(width, E - W);

    (*widthPtr) = width;
    (*heightPtr) = height;
}

/*
 *----------------------------------------------------------------------
 *
 * Style_DoLayoutNeededV --
 *
 *	Calculate the vertical size and position of each element.
 *	This is similar to Style_DoLayoutV but without expansion or
 *	squeezing. Also, the size and position of -union elements
 *	is not calculated.
 *
 * Results:
 *	layouts[] is updated.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Style_DoLayoutNeededV(
    StyleDrawArgs *drawArgs,	/* Various args. */
    struct Layout layouts[]	/* Array of layout records to be updated,
				 * one per element. Should be initialized
				 * by Style_DoLayoutH(). */
    )
{
    IStyle *style = (IStyle *) drawArgs->style;
    MStyle *masterStyle = style->master;
    MElementLink *eLinks1, *eLink1;
    IElementLink *eLinks2, *eLink2;
    int *ePadY, *iPadY, *uPadY;
    int i;
    int y = 0;

    eLinks1 = masterStyle->elements;
    eLinks2 = style->elements;

    /* Layout elements left-to-right, or top-to-bottom */
    for (i = 0; i < masterStyle->numElements; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];
	eLink2 = &eLinks2[i];

	/* The size of a -union element is determined by the elements
	 * it surrounds */
	if (IS_UNION(eLink1)) {
	    /* I don't need good values because I'm only calculating the
	     * needed height */
	    layout->y = layout->iHeight = layout->eHeight = 0;
	    continue;
	}

	/* -detach elements are positioned by themselves */
	if (IS_DETACH(eLink1))
	    continue;

	ePadY = eLink1->ePadY;
	iPadY = eLink1->iPadY;
	uPadY = layout->uPadY;

	layout->y = y + abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
	layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
	layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

	if (masterStyle->vertical)
	    y = layout->y + layout->eHeight;
    }

    /* -detach elements */
    for (i = 0; i < masterStyle->numElements; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];
	eLink2 = &eLinks2[i];

	if (!IS_DETACH(eLink1) || IS_UNION(eLink1))
	    continue;

	ePadY = eLink1->ePadY;
	iPadY = eLink1->iPadY;
	uPadY = layout->uPadY;

	layout->y = abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
	layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
	layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Style_DoLayout --
 *
 *	Calculate the size and position of each element.
 *
 * Results:
 *	layouts[] is updated.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

/* Arrange all the Elements considering drawArgs.width and maybe drawArgs.height */
static void
Style_DoLayout(
    StyleDrawArgs *drawArgs,	/* Various args. */
    struct Layout layouts[],	/* Uninitialized records to be filled in. */
    int neededV,		/* TRUE if drawArgs.height should be ignored. */
    char *file,			/* debug */
    int line			/* debug */
    )
{
    TreeCtrl *tree = drawArgs->tree;
    IStyle *style = (IStyle *) drawArgs->style;
    MStyle *masterStyle = style->master;
    int state = drawArgs->state;
    int i;

    if (style->neededWidth == -1)
	panic("Style_DoLayout(file %s line %d): style.neededWidth == -1",
	    file, line);
#ifdef CACHE_STYLE_STYLE
    if (style->minWidth + drawArgs->indent > drawArgs->width)
	panic("Style_DoLayout(file %s line %d): style.minWidth + drawArgs->indent %d > drawArgs.width %d",
	    file, line, style->minWidth + drawArgs->indent, drawArgs->width);
#endif
    Style_DoLayoutH(drawArgs, layouts);

    for (i = 0; i < masterStyle->numElements; i++) {
	struct Layout *layout = &layouts[i];
	MElementLink *eLink1 = layout->master;
	IElementLink *eLink2 = layout->eLink;
	TreeElementArgs args;

	if (IS_HIDDEN(layout))
	    continue;

	/* The size of a -union element is determined by the elements
	 * it surrounds */
	if (IS_UNION(eLink1)) {
	    layout->useHeight = 0;
	    continue;
	}

#ifdef CACHE_ELEM_SIZE
	layout->useHeight = eLink2->neededHeight;
#else
	layout->useHeight = layout->neededHeight;
#endif

	/* If a Text Element is given less width than it needs (due to
	 * -squeeze x layout), then it may wrap lines. This means
	 * the height can vary depending on the width. */
	if (eLink2->elem->typePtr->heightProc == NULL)
	    continue;

	if (eLink1->fixedHeight >= 0)
	    continue;

#ifdef CACHE_ELEM_SIZE
	/* Not squeezed */
	if (layout->useWidth >= eLink2->neededWidth)
	    continue;

	/* Already calculated the height at this width */
	if (layout->useWidth == eLink2->layoutWidth) {
	    layout->useHeight = eLink2->layoutHeight;
	    continue;
	}
#else
	/* Not squeezed */
	if (layout->useWidth >= layout->neededWidth)
	    continue;
#endif
	args.tree = tree;
	args.state = state;
	args.elem = eLink2->elem;
	args.height.fixedWidth = layout->useWidth;
	(*args.elem->typePtr->heightProc)(&args);

	if (eLink1->fixedHeight >= 0)
	    layout->useHeight = eLink1->fixedHeight;
	else if ((eLink1->minHeight >= 0) &&
	    (args.height.height <  eLink1->minHeight))
	    layout->useHeight = eLink1->minHeight;
	else if ((eLink1->maxHeight >= 0) &&
	    (args.height.height >  eLink1->maxHeight))
	    layout->useHeight = eLink1->maxHeight;
	else
	    layout->useHeight = args.height.height;

#ifdef CACHE_ELEM_SIZE
	eLink2->layoutWidth = layout->useWidth;
	eLink2->layoutHeight = layout->useHeight;
#endif
    }

    if (neededV) {
        Style_DoLayoutNeededV(drawArgs, layouts);
    } else {
        Style_DoLayoutV(drawArgs, layouts);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Style_NeededSize --
 *
 *	Calculate the width and height of a style based only on
 *	the requested size of each element.
 *
 * Results:
 *	The width and height. The minimum width and height is equal to
 *	the requested width and height minus any squeezing.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Style_NeededSize(
    TreeCtrl *tree,		/* Widget info. */
    IStyle *style,		/* Style to calculate size of. */
    int state,			/* STATE_xxx flags. */
    int *widthPtr,		/* Returned width. */
    int *heightPtr,		/* Returned height. */
    int *minWidthPtr,		/* Returned minimum width. */
    int *minHeightPtr		/* Returned minimum height. */
    )
{
    MStyle *masterStyle = style->master;
    MElementLink *eLinks1, *eLink1;
    IElementLink *eLinks2, *eLink2;
    struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
    int *ePadX, *iPadX, *uPadX, *ePadY, *iPadY, *uPadY;
    int i, j, eLinkCount = masterStyle->numElements;
    int x = 0, y = 0;
    int squeezeX = 0, squeezeY = 0;

    STATIC_ALLOC(layouts, struct Layout, eLinkCount);

    eLinks1 = masterStyle->elements;
    eLinks2 = style->elements;

    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];
	layout->unionParent = 0;
	layout->temp = 0; /* see Layout_CalcVisibility */
    }

    for (i = 0; i < eLinkCount; i++) {
	Layout_CalcVisibility(tree, state, masterStyle, layouts, i);
    }

    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];

	eLink1 = &eLinks1[i];
	eLink2 = &eLinks2[i];

	if (IS_HIDDEN(layout))
	    continue;

	if (IS_UNION(eLink1)) {
	    for (j = 0; j < eLink1->onionCount; j++) {
		struct Layout *layout2 = &layouts[eLink1->onion[j]];
		if (!IS_HIDDEN(layout2))
		    layout2->unionParent = 1;
	    }
	}

	layout->master = eLink1;
	layout->eLink = eLink2;

	if (!IS_UNION(eLink1)) {
#ifdef CACHE_ELEM_SIZE
	    if ((eLink2->neededWidth == -1) || (eLink2->neededHeight == -1)) {
		Element_NeededSize(tree, eLink1, eLink2->elem, state,
			&eLink2->neededWidth, &eLink2->neededHeight);
		eLink2->layoutWidth = -1;
	    }
	    layout->useWidth = eLink2->neededWidth;
	    layout->useHeight = eLink2->neededHeight;
#else
	    Element_NeededSize(tree, eLink1, eLink2->elem, state,
		    &layout->neededWidth, &layout->neededHeight);
	    layout->useWidth = layout->neededWidth;
#endif
	}

	for (j = 0; j < 2; j++) {
	    layout->ePadX[j] = eLink1->ePadX[j];
	    layout->ePadY[j] = eLink1->ePadY[j];
	    layout->iPadX[j] = eLink1->iPadX[j];
	    layout->iPadY[j] = eLink1->iPadY[j];

	    /* No -union padding yet */
	    layout->uPadX[j] = 0;
	    layout->uPadY[j] = 0;
	}
    }

    /* Calculate the padding around elements surrounded by -union elements */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];
	int padx[2], pady[2];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];

	/* Start at the top level of each -union heirarchy. */
	if (!IS_UNION(eLink1) || layout->unionParent)
	    continue;

	ePadX = eLink1->ePadX;
	ePadY = eLink1->ePadY;
	iPadX = eLink1->iPadX;
	iPadY = eLink1->iPadY;

	for (j = 0; j < 2; j++) {
	    padx[j] = ePadX[j] + iPadX[j];
	    pady[j] = ePadY[j] + iPadY[j];
	}
	for (j = 0; j < eLink1->onionCount; j++) {
	    struct Layout *layout2 = &layouts[eLink1->onion[j]];
	    if (IS_HIDDEN(layout2))
		continue;
	    Layout_AddUnionPadding(masterStyle, layouts, i, eLink1->onion[j],
		padx, pady);
	}
    }

    /* Layout elements left-to-right, or top-to-bottom */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];
	eLink2 = &eLinks2[i];

	/* The size of a -union element is determined by the elements
	 * it surrounds */
	if (IS_UNION(eLink1)) {
	    layout->x = layout->y = layout->eWidth = layout->eHeight = 0;
	    for (j = 0; j < 2; j++) {
		layout->ePadX[j] = 0;
		layout->ePadY[j] = 0;
		layout->iPadX[j] = 0;
		layout->iPadY[j] = 0;
		layout->uPadX[j] = 0;
		layout->uPadY[j] = 0;
	    }
	    continue;
	}

	if (eLink1->flags & ELF_SQUEEZE_X) squeezeX++;
	if (eLink1->flags & ELF_SQUEEZE_Y) squeezeY++;

	/* -detach elements are positioned by themselves */
	if (IS_DETACH(eLink1))
	    continue;

	ePadX = eLink1->ePadX;
	ePadY = eLink1->ePadY;
	iPadX = eLink1->iPadX;
	iPadY = eLink1->iPadY;
	uPadX = layout->uPadX;
	uPadY = layout->uPadY;

	layout->x = x + abs(ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]));
	layout->y = y + abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
	layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
	layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
	layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];
	layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

	if (masterStyle->vertical)
	    y = layout->y + layout->eHeight;
	else
	    x = layout->x + layout->eWidth;
    }

    /* -detach elements */
    for (i = 0; i < eLinkCount; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink1 = &eLinks1[i];
	eLink2 = &eLinks2[i];

	if (!IS_DETACH(eLink1) || IS_UNION(eLink1))
	    continue;

	ePadX = eLink1->ePadX;
	ePadY = eLink1->ePadY;
	iPadX = eLink1->iPadX;
	iPadY = eLink1->iPadY;
	uPadX = layout->uPadX;
	uPadY = layout->uPadY;

	layout->x = abs(ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]));
	layout->y = abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
	layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
	layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
	layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];
	layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];
    }

    Layout_Size(masterStyle->vertical, eLinkCount, layouts,
	widthPtr, heightPtr);

    if (squeezeX || squeezeY) {
	for (i = 0; i < eLinkCount; i++) {
	    struct Layout *layout = &layouts[i];
	    int subtract;

	    if (IS_HIDDEN(layout))
		continue;

	    eLink1 = &eLinks1[i];

	    if (IS_UNION(eLink1))
		continue;

	    if (eLink1->flags & ELF_SQUEEZE_X) {
		if ((eLink1->minWidth >= 0) &&
			(eLink1->minWidth <= layout->useWidth)) {
		    subtract = layout->useWidth - eLink1->minWidth;
		} else {
		    subtract = layout->useWidth;
		}
		layout->eWidth -= subtract;
		if (!masterStyle->vertical && !IS_DETACH(eLink1)) {
		    for (j = i + 1; j < eLinkCount; j++)
			if (!IS_HIDDEN(&layouts[j]) && !DETACH_OR_UNION(&eLinks1[j]))
			    layouts[j].x -= subtract;
		}
	    }
	    if (eLink1->flags & ELF_SQUEEZE_Y) {
		if ((eLink1->minHeight >= 0) &&
			(eLink1->minHeight <= layout->useHeight)) {
		    subtract = layout->useHeight - eLink1->minHeight;
		} else {
		    subtract = layout->useHeight;
		}
		layout->eHeight -= subtract;
		if (masterStyle->vertical && !IS_DETACH(eLink1)) {
		    for (j = i + 1; j < eLinkCount; j++)
			if (!IS_HIDDEN(&layouts[j]) && !DETACH_OR_UNION(&eLinks1[j]))
			    layouts[j].y -= subtract;
		}
	    }
	}

	Layout_Size(masterStyle->vertical, eLinkCount, layouts,
	    minWidthPtr, minHeightPtr);
    } else {
	*minWidthPtr = *widthPtr;
	*minHeightPtr = *heightPtr;
    }

    STATIC_FREE(layouts, struct Layout, eLinkCount);
}

/*
 *----------------------------------------------------------------------
 *
 * Style_CheckNeededSize --
 *
 *	If the style's requested size is out-of-date then recalculate
 *	Style.neededWidth, Style.neededHeight, Style.minWidth, and
 *	Style.minHeight.
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
Style_CheckNeededSize(
    TreeCtrl *tree,		/* Widget info. */
    IStyle *style,		/* Style info. */
    int state			/* STATE_xxx flags. */
    )
{
    if (style->neededWidth == -1) {
	int minWidth, minHeight;

	Style_NeededSize(tree, style, state,
	    &style->neededWidth, &style->neededHeight, &minWidth, &minHeight);
#ifdef CACHE_STYLE_SIZE
	style->minWidth = minWidth;
	style->minHeight = minHeight;
	style->layoutWidth = -1;
#endif /* CACHE_STYLE_SIZE */
#ifdef TREECTRL_DEBUG
	style->neededState = state;
#endif
    }
#ifdef TREECTRL_DEBUG
    if (style->neededState != state)
	panic("Style_CheckNeededSize: neededState %d != state %d\n",
	    style->neededState, state);
#endif
}

#ifndef CACHE_STYLE_SIZE

static void
Style_MinSize(
    TreeCtrl *tree,		/* Widget info. */
    IStyle *style,		/* Style info. */
    int state,			/* STATE_xxx flags. */
    int *minWidthPtr,
    int *minHeightPtr
    )
{
    int i, hasSqueeze = FALSE;

    for (i = 0; i < style->master->numElements; i++) {
	MElementLink *eLink1 = &style->master->elements[i];
	if (!IS_UNION(eLink1) &&
		(eLink1->flags & (ELF_SQUEEZE_X | ELF_SQUEEZE_Y))) {
	    hasSqueeze = TRUE;
	    break;
	}
    }
    if (hasSqueeze) {
	int width, height;
	Style_NeededSize(tree, style, state, &width, &height,
		minWidthPtr, minHeightPtr);
    } else {
	*minWidthPtr = style->neededWidth;
	*minHeightPtr = style->neededHeight;
    }
}

#endif /* !CACHE_STYLE_SIZE */

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_NeededWidth --
 *
 *	Return the requested width of a style.
 *
 * Results:
 *	The requested width. If the requested size is out-of-date
 *	then it is recalculated.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeStyle_NeededWidth(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_,		/* Style token. */
    int state			/* STATE_xxx flags. */
    )
{
    IStyle *style = (IStyle *) style_;

    Style_CheckNeededSize(tree, style, state);
    return style->neededWidth;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_NeededHeight --
 *
 *	Return the requested height of a style.
 *
 * Results:
 *	The requested height. If the requested size is out-of-date
 *	then it is recalculated.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeStyle_NeededHeight(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_,		/* Style token. */
    int state			/* STATE_xxx flags. */
    )
{
    IStyle *style = (IStyle *) style_;

    Style_CheckNeededSize(tree, style, state);
    return style->neededHeight;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_UseHeight --
 *
 *	Return the height of a style for a given state and width.
 *
 * Results:
 *	The height of the style.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeStyle_UseHeight(
    StyleDrawArgs *drawArgs	/* Various args. */
    )
{
    TreeCtrl *tree = drawArgs->tree;
    IStyle *style = (IStyle *) drawArgs->style;
    MStyle *masterStyle = style->master;
    int state = drawArgs->state;
    struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
    int width, height, minWidth;
#ifndef CACHE_STYLE_SIZE
    int minHeight;
#endif

    Style_CheckNeededSize(tree, style, state);
#ifdef CACHE_STYLE_SIZE
    minWidth = style->minWidth;
#else
    if (drawArgs->width < style->neededWidth + drawArgs->indent)
	Style_MinSize(tree, style, state, &minWidth, &minHeight);
    else
	minWidth = style->neededWidth;
#endif

    /*
     * If we have:
     * a) infinite space available, or
     * b) more width than the style needs, or
     * c) less width than the style needs, but it has no -squeeze x elements
     * then return the needed height of the style. This is safe since no
     * text elements will be growing vertically when lines wrap.
     */
    if ((drawArgs->width == -1) ||
	(drawArgs->width >= style->neededWidth + drawArgs->indent) ||
	(style->neededWidth == minWidth)) {
	return style->neededHeight;
    }

    /* We never lay out the style at less than the minimum width */
    if (drawArgs->width < minWidth + drawArgs->indent)
	drawArgs->width = minWidth + drawArgs->indent;

#ifdef CACHE_STYLE_SIZE
    /* We have less space than the style needs, and have already calculated
     * the height of the style at this width. (The height may change because
     * of text elements wrapping lines). */
    if (drawArgs->width == style->layoutWidth)
	return style->layoutHeight;
#endif

    STATIC_ALLOC(layouts, struct Layout, masterStyle->numElements);

    Style_DoLayout(drawArgs, layouts, TRUE, __FILE__, __LINE__);

    Layout_Size(style->master->vertical, masterStyle->numElements, layouts,
	&width, &height);

    STATIC_FREE(layouts, struct Layout, masterStyle->numElements);

#ifdef CACHE_STYLE_SIZE
    style->layoutWidth = drawArgs->width;
    style->layoutHeight = height;
#endif

    return height;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_Draw --
 *
 *	Draw all the elements in a style.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn.
 *
 *----------------------------------------------------------------------
 */

void
TreeStyle_Draw(
    StyleDrawArgs *drawArgs	/* Various args. */
    )
{
    IStyle *style = (IStyle *) drawArgs->style;
    MStyle *masterStyle = style->master;
    TreeCtrl *tree = drawArgs->tree;
    TreeRectangle bounds;
    TreeElementArgs args;
    int i, x, y, minWidth, minHeight;
    struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
#undef DEBUG_DRAW
#ifdef DEBUG_DRAW
    int debugDraw = FALSE;
#endif

    Style_CheckNeededSize(tree, style, drawArgs->state);
#ifdef CACHE_STYLE_SIZE
    minWidth = style->minWidth;
    minHeight = style->minHeight;
#else
    if ((drawArgs->width < style->neededWidth + drawArgs->indent) ||
	    (drawArgs->height < style->neededHeight)) {
	Style_MinSize(tree, style, drawArgs->state, &minWidth, &minHeight);
    } else {
	minWidth = style->neededWidth;
	minHeight = style->neededHeight;
    }
#endif

    /* Get the bounds allowed for drawing (in window coordinates), inside
     * the item-column(s) and inside the header/borders. */
    x = drawArgs->x + tree->drawableXOrigin - tree->xOrigin;
    y = drawArgs->y + tree->drawableYOrigin - tree->yOrigin;
    TreeRect_SetXYWH(bounds, x, y, drawArgs->width, drawArgs->height);
    TreeRect_Intersect(&args.display.bounds, &bounds, &drawArgs->bounds);

    /* We never lay out the style at less than the minimum size */
    if (drawArgs->width < minWidth + drawArgs->indent)
	drawArgs->width = minWidth + drawArgs->indent;
    if (drawArgs->height < minHeight)
	drawArgs->height = minHeight;

    STATIC_ALLOC(layouts, struct Layout, masterStyle->numElements);

    Style_DoLayout(drawArgs, layouts, FALSE, __FILE__, __LINE__);

    args.tree = tree;
    args.state = drawArgs->state;
    args.display.td = drawArgs->td;
    args.display.drawable = drawArgs->td.drawable;
    args.display.column = drawArgs->column; /* needed for gradients */
    args.display.item = drawArgs->item; /* needed for gradients */

    for (i = 0; i < masterStyle->numElements; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	/* Don't "draw" window elements. TreeStyle_UpdateWindowPositions()
	 * does that for us. */
	if (ELEMENT_TYPE_MATCHES(layout->eLink->elem->typePtr, &treeElemTypeWindow))
	    continue;

	if (PerStateBoolean_ForState(tree, &layout->master->draw,
		drawArgs->state, NULL) == 0)
	    continue;

#ifdef DEBUG_DRAW
	if (debugDraw && layout->master->onion != NULL)
	    continue;
#endif

	if ((layout->useWidth > 0) && (layout->useHeight > 0)) {
	    args.elem = layout->eLink->elem;
	    args.display.x = drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT];
	    args.display.y = drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT];
	    args.display.x += layout->iPadX[PAD_TOP_LEFT];
	    args.display.y += layout->iPadY[PAD_TOP_LEFT];
	    args.display.width = layout->useWidth;
	    args.display.height = layout->useHeight;
	    args.display.sticky = layout->master->flags & ELF_STICKY;
#ifdef DEBUG_DRAW
	    if (debugDraw) {
		XColor *color[3];
		GC gc[3];

		if (layout->master->onion != NULL) {
		    color[0] = Tk_GetColor(tree->interp, tree->tkwin, "blue2");
		    gc[0] = Tk_GCForColor(color[0], Tk_WindowId(tree->tkwin));
		    color[1] = Tk_GetColor(tree->interp, tree->tkwin, "blue3");
		    gc[1] = Tk_GCForColor(color[1], Tk_WindowId(tree->tkwin));
		} else {
		    color[0] = Tk_GetColor(tree->interp, tree->tkwin, "gray50");
		    gc[0] = Tk_GCForColor(color[0], Tk_WindowId(tree->tkwin));
		    color[1] = Tk_GetColor(tree->interp, tree->tkwin, "gray60");
		    gc[1] = Tk_GCForColor(color[1], Tk_WindowId(tree->tkwin));
		    color[2] = Tk_GetColor(tree->interp, tree->tkwin, "gray70");
		    gc[2] = Tk_GCForColor(color[2], Tk_WindowId(args.tree->tkwin));
		}

		/* external */
		XFillRectangle(tree->display, args.display.drawable,
		    gc[2],
		    args.display.x - layout->ePadX[PAD_TOP_LEFT],
		    args.display.y - layout->ePadY[PAD_TOP_LEFT],
		    layout->eWidth, layout->eHeight);
		/* internal */
		XFillRectangle(tree->display, args.display.drawable,
		    gc[1],
		    args.display.x, args.display.y,
		    args.display.width, args.display.height);
		/* needed */
		if (!layout->master->onion && !(layout->master->flags & ELF_DETACH))
		XFillRectangle(tree->display, args.display.drawable,
		    gc[0],
		    args.display.x + layout->iPadX[PAD_TOP_LEFT],
		    args.display.y + layout->iPadY[PAD_TOP_LEFT],
		    layout->eLink->neededWidth, layout->eLink->neededHeight);
	    } else
#endif /* DEBUG_DRAW */
		(*args.elem->typePtr->displayProc)(&args);
	}
    }

#ifdef DEBUG_DRAW
    if (debugDraw)
	for (i = 0; i < masterStyle->numElements; i++) {
	    struct Layout *layout = &layouts[i];

	    if (IS_HIDDEN(layout))
		continue;

	    if (layout->master->onion == NULL)
		continue;
	    if (layout->useWidth > 0 && layout->useHeight > 0) {
		args.elem = layout->eLink->elem;
		args.display.x = drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT];
		args.display.y = drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT];
		args.display.width = layout->iWidth;
		args.display.height = layout->iHeight;
		{
		    XColor *color[3];
		    GC gc[3];

		    color[0] = Tk_GetColor(tree->interp, tree->tkwin, "blue2");
		    gc[0] = Tk_GCForColor(color[0], Tk_WindowId(tree->tkwin));
		    color[1] = Tk_GetColor(tree->interp, tree->tkwin, "blue3");
		    gc[1] = Tk_GCForColor(color[1], Tk_WindowId(tree->tkwin));

		    /* external */
		    XDrawRectangle(tree->display, args.display.drawable,
			gc[0],
			args.display.x - layout->ePadX[PAD_TOP_LEFT],
			args.display.y - layout->ePadY[PAD_TOP_LEFT],
			layout->eWidth - 1, layout->eHeight - 1);
		    /* internal */
		    XDrawRectangle(tree->display, args.display.drawable,
			gc[1],
			args.display.x, args.display.y,
			args.display.width - 1, args.display.height - 1);
		}
	    }
	}
#endif /* DEBUG_DRAW */

    STATIC_FREE(layouts, struct Layout, masterStyle->numElements);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_UpdateWindowPositions --
 *
 *	Call the displayProc on each window element so it can update
 *	its geometry. This is needed if an item was scrolled and its
 *	displayProc wasn't otherwise called.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Possible window geometry changes.
 *
 *----------------------------------------------------------------------
 */

void
TreeStyle_UpdateWindowPositions(
    StyleDrawArgs *drawArgs	/* Various args. */
    )
{
    IStyle *style = (IStyle *) drawArgs->style;
    MStyle *masterStyle = style->master;
    TreeCtrl *tree = drawArgs->tree;
    TreeRectangle bounds;
    TreeElementArgs args;
    int i, x, y, minWidth, minHeight;
    struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
    int numElements = masterStyle->numElements;

    /* FIXME: Perhaps remember whether this style has any window
     * elements */
    for (i = 0; i < numElements; i++) {
	if (ELEMENT_TYPE_MATCHES(masterStyle->elements[i].elem->typePtr, &treeElemTypeWindow))
	    break;
    }
    if (i == numElements)
	return;

    Style_CheckNeededSize(tree, style, drawArgs->state);
#ifdef CACHE_STYLE_SIZE
    minWidth = style->minWidth;
    minHeight = style->minHeight;
#else
    if ((drawArgs->width < style->neededWidth + drawArgs->indent) ||
	    (drawArgs->height < style->neededHeight)) {
	Style_MinSize(tree, style, drawArgs->state, &minWidth, &minHeight);
    } else {
	minWidth = style->neededWidth;
	minHeight = style->neededHeight;
    }
#endif

    /* Get the bounds allowed for drawing (in window coordinates), inside
     * the item-column(s) and inside the header/borders. */
    x = drawArgs->x + tree->drawableXOrigin - tree->xOrigin;
    y = drawArgs->y + tree->drawableYOrigin - tree->yOrigin;
    TreeRect_SetXYWH(bounds, x, y, drawArgs->width, drawArgs->height);
    TreeRect_Intersect(&args.display.bounds, &bounds, &drawArgs->bounds);

    /* We never lay out the style at less than the minimum size */
    if (drawArgs->width < minWidth + drawArgs->indent)
	drawArgs->width = minWidth + drawArgs->indent;
    if (drawArgs->height < minHeight)
	drawArgs->height = minHeight;

    STATIC_ALLOC(layouts, struct Layout, numElements);

    Style_DoLayout(drawArgs, layouts, FALSE, __FILE__, __LINE__);

    args.tree = tree;
    args.state = drawArgs->state;
    args.display.td = drawArgs->td;
    args.display.drawable = drawArgs->td.drawable;

    for (i = 0; i < numElements; i++) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	if (!ELEMENT_TYPE_MATCHES(layout->eLink->elem->typePtr, &treeElemTypeWindow))
	    continue;

	if (PerStateBoolean_ForState(tree, &layout->master->draw,
		drawArgs->state, NULL) == 0)
	    continue;

	if ((layout->useWidth > 0) && (layout->useHeight > 0)) {
	    int requests;

	    TreeDisplay_GetReadyForTrouble(tree, &requests);

	    args.elem = layout->eLink->elem;
	    args.display.x = drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT];
	    args.display.y = drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT];
	    args.display.x += layout->iPadX[PAD_TOP_LEFT];
	    args.display.y += layout->iPadY[PAD_TOP_LEFT];
	    args.display.width = layout->useWidth;
	    args.display.height = layout->useHeight;
	    args.display.sticky = layout->master->flags & ELF_STICKY;
	    (*args.elem->typePtr->displayProc)(&args);

	    /* Updating the position of a window may generate a <Configure>
	     * or <Map> event on that window. Binding scripts on those
	     * events could do anything, including deleting items and
	     * thus the style we are drawing. In other cases (such as when
	     * using Tile widgets I notice), the Tk_GeomMgr.requestProc
	     * may get called which calls Tree_ElementChangedItself which
	     * calls FreeDItemInfo which frees a DItem we are in the middle
	     * of displaying. So if anything was done that caused a display
	     * request, then abort abort abort. */
	    if (TreeDisplay_WasThereTrouble(tree, requests))
		break;
	}
    }

    STATIC_FREE(layouts, struct Layout, numElements);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_OnScreen --
 *
 *	Call the onScreenProc (if non-NULL) on each element so it can
 *	update its visibility when an item's visibility changes.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Possible window visibility changes.
 *
 *----------------------------------------------------------------------
 */

void
TreeStyle_OnScreen(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_,		/* Style token. */
    int onScreen		/* Boolean indicating whether the item
				 * using the style is on screen anymore. */
    )
{
    IStyle *style = (IStyle *) style_;
    TreeElementArgs args;
    int i;

    args.tree = tree;
    args.screen.visible = onScreen;

    for (i = 0; i < style->master->numElements; i++) {
	IElementLink *eLink = &style->elements[i];

	if (eLink->elem->typePtr->onScreenProc == NULL)
	    continue;

	args.elem = eLink->elem;
	(*args.elem->typePtr->onScreenProc)(&args);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Element_FreeResources --
 *
 *	Free memory etc associated with an Element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

static void
Element_FreeResources(
    TreeCtrl *tree,		/* Widget info. */
    TreeElement elem		/* Record to free. */
    )
{
    TreeElementType *typePtr = elem->typePtr;
    TreeElementArgs args;
    Tcl_HashEntry *hPtr;

    if (elem->master == NULL) {
	hPtr = Tcl_FindHashEntry(&tree->elementHash, elem->name);
	Tcl_DeleteHashEntry(hPtr);
    }
    args.tree = tree;
    args.elem = elem;
    (*typePtr->deleteProc)(&args);
    Tk_FreeConfigOptions((char *) elem,
	typePtr->optionTable,
	tree->tkwin);
    DynamicOption_Free(tree, elem->options, typePtr->optionSpecs);
#ifdef ALLOC_HAX
    TreeAlloc_Free(tree->allocData, typePtr->name, (char *) elem, typePtr->size);
#else
    WFREE(elem, TreeElement_);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * MElementLink_Init --
 *
 *	Initialize (don't allocate) a MElementLink.
 *
 * Results:
 *	eLink is filled with default values.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static MElementLink *
MElementLink_Init(
    MElementLink *eLink,	/* Existing record to initialize. */
    TreeElement elem		/* Existing element to point to. */
    )
{
    memset(eLink, '\0', sizeof(MElementLink));
    eLink->elem = elem;
    eLink->flags |= ELF_INDENT;
    eLink->minWidth = eLink->fixedWidth = eLink->maxWidth = -1;
    eLink->minHeight = eLink->fixedHeight = eLink->maxHeight = -1;
    eLink->flags |= ELF_STICKY;
    return eLink;
}

/*
 *----------------------------------------------------------------------
 *
 * MElementLink_FreeResources --
 *
 *	Free memory etc associated with an MElementLink.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

static void
MElementLink_FreeResources(
    TreeCtrl *tree,		/* Widget info. */
    MElementLink *eLink		/* Record to free. */
    )
{
    if (eLink->onion != NULL)
	WCFREE(eLink->onion, int, eLink->onionCount);
    PerStateInfo_Free(tree, &pstBoolean, &eLink->draw);
    if (eLink->draw.obj != NULL) {
	Tcl_DecrRefCount(eLink->draw.obj);
    }
    PerStateInfo_Free(tree, &pstBoolean, &eLink->visible);
    if (eLink->visible.obj != NULL) {
	Tcl_DecrRefCount(eLink->visible.obj);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * IElementLink_FreeResources --
 *
 *	Free memory etc associated with an ElementLink.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

static void
IElementLink_FreeResources(
    TreeCtrl *tree,		/* Widget info. */
    IElementLink *eLink		/* Record to free. */
    )
{
    if (eLink->elem->master != NULL)
	Element_FreeResources(tree, eLink->elem);
}

/*
 *----------------------------------------------------------------------
 *
 * MStyle_FreeResources --
 *
 *	Free memory etc associated with a Style.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

static void
MStyle_FreeResources(
    TreeCtrl *tree,		/* Widget info. */
    MStyle *style		/* Style to free. */
    )
{
    Tcl_HashEntry *hPtr;
    int i;

    hPtr = Tcl_FindHashEntry(&tree->styleHash, style->name);
    Tcl_DeleteHashEntry(hPtr);

    if (style->numElements > 0) {
	for (i = 0; i < style->numElements; i++)
	    MElementLink_FreeResources(tree, &style->elements[i]);
#ifdef ALLOC_HAX
	TreeAlloc_CFree(tree->allocData, MElementLinkUid, (char *) style->elements,
		sizeof(MElementLink), style->numElements, ELEMENT_LINK_ROUND);
#else
	WCFREE(style->elements, MElementLink, style->numElements);
#endif
    }
#ifdef ALLOC_HAX
    TreeAlloc_Free(tree->allocData, MStyleUid, (char *) style, sizeof(MStyle));
#else
    WFREE(style, MStyle);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * IStyle_FreeResources --
 *
 *	Free memory etc associated with a Style.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

static void
IStyle_FreeResources(
    TreeCtrl *tree,		/* Widget info. */
    IStyle *style		/* Style to free. */
    )
{
    MStyle *masterStyle = style->master;
    int i;

    if (masterStyle->numElements > 0) {
	for (i = 0; i < masterStyle->numElements; i++)
	    IElementLink_FreeResources(tree, &style->elements[i]);
#ifdef ALLOC_HAX
	TreeAlloc_CFree(tree->allocData, IElementLinkUid,
		(char *) style->elements, sizeof(IElementLink),
		masterStyle->numElements, ELEMENT_LINK_ROUND);
#else
	WCFREE(style->elements, IElementLink, masterStyle->numElements);
#endif
    }
#ifdef ALLOC_HAX
    TreeAlloc_Free(tree->allocData, IStyleUid, (char *) style, sizeof(IStyle));
#else
    WFREE(style, IStyle);
#endif
}
/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_FreeResources --
 *
 *	Free memory etc associated with a Style.
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
TreeStyle_FreeResources(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_		/* Token of style to free. */
    )
{
    MStyle *masterStyle = (MStyle *) style_;
    IStyle *style = (IStyle *) style_;

    if (style->master == NULL)
	MStyle_FreeResources(tree, masterStyle);
    else
	IStyle_FreeResources(tree, style);
}

/*
 *----------------------------------------------------------------------
 *
 * MStyle_FindElem --
 *
 *	Find an ElementLink in a style.
 *
 * Results:
 *	If found, a pointer to the ElementLink and index in the
 *	style's array of ElementLinks is returned; otherwise NULL
 *	is returned.
 *
 * Side effects:
 *	World peace.
 *
 *----------------------------------------------------------------------
 */

static MElementLink *
MStyle_FindElem(
    TreeCtrl *tree,		/* Widget info. */
    MStyle *style,		/* Style to search. */
    TreeElement master,		/* Master element to find. */
    int *index			/* Returned index, may be NULL. */
    )
{
    int i;

    for (i = 0; i < style->numElements; i++) {
	MElementLink *eLink = &style->elements[i];
	if (eLink->elem->name == master->name) {
	    if (index != NULL) (*index) = i;
	    return eLink;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * IStyle_FindElem --
 *
 *	Find an ElementLink in a style.
 *
 * Results:
 *	If found, a pointer to the ElementLink and index in the
 *	style's array of ElementLinks is returned; otherwise NULL
 *	is returned.
 *
 * Side effects:
 *	World peace.
 *
 *----------------------------------------------------------------------
 */

static IElementLink *
IStyle_FindElem(
    TreeCtrl *tree,		/* Widget info. */
    IStyle *style,		/* Style to search. */
    TreeElement master,		/* Master element to find. */
    int *index			/* Returned index, may be NULL. */
    )
{
    MStyle *masterStyle = style->master;
    int i;

    for (i = 0; i < masterStyle->numElements; i++) {
	IElementLink *eLink = &style->elements[i];
	if (eLink->elem->name == master->name) {
	    if (index != NULL) (*index) = i;
	    return eLink;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_FindElement --
 *
 *	Find an ElementLink in a style.
 *
 * Results:
 *	If found, the index in the style's array of ElementLinks is
 *	returned with TCL_OK. Otherwise TCL_ERROR is returned and an
 *	error message is placed in the interpreter result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeStyle_FindElement(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_,		/* Token of style to search. */
    TreeElement elem,		/* Master element to find. */
    int *index			/* Returned index, may be NULL. */
    )
{
    MStyle *masterStyle = (MStyle *) style_;
    IStyle *style = (IStyle *) style_;

    if (((style->master == NULL) &&
	    (MStyle_FindElem(tree, masterStyle, elem, index) == NULL)) ||
	    ((style->master != NULL) &&
	    (IStyle_FindElem(tree, style, elem, index) == NULL))) {
	FormatResult(tree->interp, "style %s does not use element %s",
	    style->master ? style->master->name : masterStyle->name,
	    elem->name);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Element_CreateAndConfig --
 *
 *	Allocate and initialize a new Element (master or instance).
 *
 * Results:
 *	An Element is allocated, its createProc is called, default
 *	configuration options are set, then the configProc and changeProc
 *	are called to handle any given configurations options. If an
 *	error occurs NULL is returned.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static TreeElement
Element_CreateAndConfig(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item containing the element. Should
				 * be NULL for a master element. */
    TreeItemColumn column,	/* Item-column containing the element.
				 * Should be NULL for a master element. */
    TreeElement masterElem,	/* Master element if creating an instance. */
    TreeElementType *type,	/* Element type. Should be NULL when
				 * creating an instance. */
    CONST char *name,		/* Name of master element, NULL for an
				 * instance. */
    int objc,			/* Array of intialial configuration. */
    Tcl_Obj *CONST objv[]	/* options. */
    )
{
    TreeElement elem;
    TreeElementArgs args;

    if (masterElem != NULL) {
	type = masterElem->typePtr;
	name = masterElem->name;
    }

#ifdef ALLOC_HAX
    elem = (TreeElement) TreeAlloc_Alloc(tree->allocData, type->name,
	    type->size);
#else
    elem = (TreeElement) ckalloc(type->size);
#endif
    memset(elem, '\0', type->size);
    elem->name = Tk_GetUid(name);
    elem->typePtr = type;
    elem->master = masterElem;

    args.tree = tree;
    args.elem = elem;
    args.create.item = item;
    args.create.column = column;
    if ((*type->createProc)(&args) != TCL_OK) {
#ifdef ALLOC_HAX
	TreeAlloc_Free(tree->allocData, type->name, (char *) elem, type->size);
#else
	WFREE(elem, TreeElement_);
#endif
	return NULL;
    }

    if (Tk_InitOptions(tree->interp, (char *) elem,
	type->optionTable, tree->tkwin) != TCL_OK) {
#ifdef ALLOC_HAX
	TreeAlloc_Free(tree->allocData, type->name, (char *) elem, type->size);
#else
	WFREE(elem, TreeElement_);
#endif
	return NULL;
    }
    args.config.objc = objc;
    args.config.objv = objv;
    args.config.flagSelf = 0;
    args.config.item = item;
    args.config.column = column;
    if ((*type->configProc)(&args) != TCL_OK) {
	(*type->deleteProc)(&args);
	Tk_FreeConfigOptions((char *) elem,
	    type->optionTable,
	    tree->tkwin);
	DynamicOption_Free(tree, elem->options, type->optionSpecs);
#ifdef ALLOC_HAX
	TreeAlloc_Free(tree->allocData, type->name, (char *) elem, type->size);
#else
	WFREE(elem, TreeElement_);
#endif
	return NULL;
    }

    args.change.flagSelf = args.config.flagSelf;
    args.change.flagTree = 0;
    args.change.flagMaster = 0;
    (*type->changeProc)(&args);

    return elem;
}

/*
 *----------------------------------------------------------------------
 *
 * Style_CreateElem --
 *
 *	Allocate and initialize a new instance Element in a IStyle
 *	(if it doesn't already exist) and return its associated
 *	IElementLink.
 *
 * Results:
 *	If the style already has a matching instance element, then a
 *	pointer to an existing IElementLink is returned.
 *	If the style does not already have a matching instance element,
 *	then a new one is created and a pointer to an existing
 *	IElementLink is returned.
 *	If an error occurs creating the new element the result is
 *	NULL.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static IElementLink *
Style_CreateElem(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item containing the element. */
    TreeItemColumn column,	/* Item-column containing the element. */
    IStyle *style,		/* Style to search/add the element to. */
    TreeElement masterElem,	/* Element to find or create and instance of. */
    int *isNew)			/* If non-NULL, set to TRUE if a new instance
				 * element was created. */
{
    MStyle *masterStyle = style->master;
    IElementLink *eLink = NULL;
    TreeElement elem;
    int i;

    if (masterElem->master != NULL)
	panic("Style_CreateElem called with instance Element");

    if (isNew != NULL) (*isNew) = FALSE;

    for (i = 0; i < masterStyle->numElements; i++) {
	eLink = &style->elements[i];
	if (eLink->elem == masterElem) {
	    /* Allocate instance Element here */
	    break;
	}

	/* Instance Style already has instance Element */
	if (eLink->elem->name == masterElem->name)
	    return eLink;
    }

    /* Error: Element isn't in the master Style */
    if (i == masterStyle->numElements)
	return NULL;

    elem = Element_CreateAndConfig(tree, item, column, masterElem, NULL, NULL, 0, NULL);
    if (elem == NULL)
	return NULL;

    eLink->elem = elem;
    if (isNew != NULL) (*isNew) = TRUE;
    return eLink;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_NewInstance --
 *
 *	Create and initialize a new instance of a master style.
 *
 * Results:
 *	A new instance Style. The new array of ElementLinks is
 *	initialized to contain pointers to master elements; instance
 *	elements are created the first time they are configured.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

TreeStyle
TreeStyle_NewInstance(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_		/* Master style to create instance of. */
    )
{
    MStyle *style = (MStyle *) style_;
    IStyle *copy;
    IElementLink *eLink;
    int i;

#ifdef ALLOC_HAX
    copy = (IStyle *) TreeAlloc_Alloc(tree->allocData, IStyleUid, sizeof(IStyle));
#else
    copy = (IStyle *) ckalloc(sizeof(IStyle));
#endif
    memset(copy, '\0', sizeof(IStyle));
    copy->master = style;
    copy->neededWidth = -1;
    copy->neededHeight = -1;
    if (style->numElements > 0) {
#ifdef ALLOC_HAX
	copy->elements = (IElementLink *) TreeAlloc_CAlloc(tree->allocData,
		IElementLinkUid, sizeof(IElementLink), style->numElements,
		ELEMENT_LINK_ROUND);
#else
	copy->elements = (IElementLink *) ckalloc(sizeof(IElementLink) *
		style->numElements);
#endif
	memset(copy->elements, '\0', sizeof(IElementLink) * style->numElements);
	for (i = 0; i < style->numElements; i++) {
	    eLink = &copy->elements[i];
	    eLink->elem = style->elements[i].elem;
#ifdef CACHE_ELEM_SIZE
	    eLink->neededWidth = -1;
	    eLink->neededHeight = -1;
#endif
	}
    }

    return (TreeStyle) copy;
}

/*
 *----------------------------------------------------------------------
 *
 * Element_FromObj --
 *
 *	Convert a Tcl_Obj to a master element.
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
Element_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *obj,		/* Object to convert from. */
    TreeElement *elemPtr	/* Returned record. */
    )
{
    char *name;
    Tcl_HashEntry *hPtr;

    name = Tcl_GetString(obj);
    hPtr = Tcl_FindHashEntry(&tree->elementHash, name);
    if (hPtr == NULL) {
	Tcl_AppendResult(tree->interp, "element \"", name, "\" doesn't exist",
	    NULL);
	return TCL_ERROR;
    }
    (*elemPtr) = (TreeElement) Tcl_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeElement_FromObj --
 *
 *	Convert a Tcl_Obj to a master element.
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
TreeElement_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *obj,		/* Object to convert from. */
    TreeElement *elemPtr	/* Returned master element token. */
    )
{
    return Element_FromObj(tree, obj, elemPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * TreeElement_IsType --
 *
 *	Determine if an element is of a certain type.
 *
 * Results:
 *	TRUE if the type matches, otherwise FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeElement_IsType(
    TreeCtrl *tree,		/* Widget info. */
    TreeElement elem,		/* Element to check. */
    CONST char *type		/* NULL-terminated element type name. */
    )
{
    return strcmp(elem->typePtr->name, type) == 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_FromObj --
 *
 *	Convert a Tcl_Obj to a master style.
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
TreeStyle_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *obj,		/* Object to convert from. */
    TreeStyle *stylePtr)	/* Returned master style token. */
{
    char *name;
    Tcl_HashEntry *hPtr;

    name = Tcl_GetString(obj);
    hPtr = Tcl_FindHashEntry(&tree->styleHash, name);
    if (hPtr == NULL) {
	Tcl_AppendResult(tree->interp, "style \"", name, "\" doesn't exist",
	    NULL);
	return TCL_ERROR;
    }
    (*stylePtr) = (TreeStyle) Tcl_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Element_ToObj --
 *
 *	Create a new Tcl_Obj representing an element.
 *
 * Results:
 *	A Tcl_Obj.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
Element_ToObj(
    TreeElement elem		/* Element to create Tcl_Obj from. */
    )
{
    return Tcl_NewStringObj(elem->name, -1);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_ToObj --
 *
 *	Create a new Tcl_Obj representing a style.
 *
 * Results:
 *	A Tcl_Obj.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TreeStyle_ToObj(
    TreeStyle style_		/* Style token to create Tcl_Obj from. */
    )
{
    MStyle *masterStyle = (MStyle *) style_;
    IStyle *style = (IStyle *) style_;

    if (style->master != NULL)
	masterStyle = style->master;
    return Tcl_NewStringObj(masterStyle->name, -1);
}

/*
 *----------------------------------------------------------------------
 *
 * Style_Changed --
 *
 *	Called when a master style is configured or the layout of one
 *	of its elements changes.
 *
 * Results:
 *	For each item-column using an instance of the given master
 *	style, size and display info is marked out-of-date.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

static void
Style_Changed(
    TreeCtrl *tree,		/* Widget info. */
    MStyle *masterStyle		/* Style that changed. */
    )
{
    TreeItem item;
    TreeItemColumn column;
    TreeColumn treeColumn;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    int columnIndex, layout;
    int updateDInfo = FALSE;
    IStyle *style;

    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
    while (hPtr != NULL) {
	item = (TreeItem) Tcl_GetHashValue(hPtr);
	treeColumn = tree->columns;
	column = TreeItem_GetFirstColumn(tree, item);
	columnIndex = 0;
	layout = FALSE;
	while (column != NULL) {
	    style = (IStyle *) TreeItemColumn_GetStyle(tree, column);
	    if ((style != NULL) && (style->master == masterStyle)) {
#ifdef CACHE_ELEM_SIZE
		int i;
		for (i = 0; i < masterStyle->numElements; i++) {
		    IElementLink *eLink = &style->elements[i];
		    /* This is needed if the -width/-height layout options change */
		    eLink->neededWidth = eLink->neededHeight = -1;
		}
#endif
		style->neededWidth = style->neededHeight = -1;
		Tree_InvalidateColumnWidth(tree, treeColumn);
		TreeItemColumn_InvalidateSize(tree, column);
		layout = TRUE;
	    }
	    columnIndex++;
	    column = TreeItemColumn_GetNext(tree, column);
	    treeColumn = TreeColumn_Next(treeColumn);
	}
	if (layout) {
	    TreeItem_InvalidateHeight(tree, item);
	    Tree_FreeItemDInfo(tree, item, NULL);
	    updateDInfo = TRUE;
	}
	hPtr = Tcl_NextHashEntry(&search);
    }
    if (updateDInfo)
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
}

/*
 *----------------------------------------------------------------------
 *
 * MStyle_ChangeElementsAux --
 *
 *	Update the list of elements used by a style. Elements
 *	may be inserted or deleted.
 *
 * Results:
 *	The list of elements in the style is updated.
 *
 * Side effects:
 *	Memory may be allocated/deallocated.
 *
 *----------------------------------------------------------------------
 */

static void
MStyle_ChangeElementsAux(
    TreeCtrl *tree,		/* Widget info. */
    MStyle *style,		/* Master style to be updated. */
    int count,			/* The number of elements in the style after
				 * this routine finishes. */
    TreeElement *elemList,	/* List of master elements the style uses. */
    int *map			/* Array of indexes into the list of elements
				 * currently used by the style. */
    )
{
    MElementLink *eLink, *eLinks = NULL;
    int i, staticKeep[STATIC_SIZE], *keep = staticKeep;

    STATIC_ALLOC(keep, int, style->numElements);

    if (count > 0) {
#ifdef ALLOC_HAX
	eLinks = (MElementLink *) TreeAlloc_CAlloc(tree->allocData,
		MElementLinkUid, sizeof(MElementLink), count,
		ELEMENT_LINK_ROUND);
#else
	eLinks = (MElementLink *) ckalloc(sizeof(MElementLink) * count);
#endif
    }

    /* Assume we are discarding all the old ElementLinks */
    for (i = 0; i < style->numElements; i++)
	keep[i] = 0;

    for (i = 0; i < count; i++) {
	if (map[i] != -1) {
	    eLinks[i] = style->elements[map[i]];
	    keep[map[i]] = 1;
	} else {
	    eLink = MElementLink_Init(&eLinks[i], elemList[i]);
	}
    }

    if (style->numElements > 0) {
	/* Free unused ElementLinks */
	for (i = 0; i < style->numElements; i++) {
	    if (!keep[i]) {
		MElementLink_FreeResources(tree, &style->elements[i]);
	    }
	}
#ifdef ALLOC_HAX
	TreeAlloc_CFree(tree->allocData, MElementLinkUid,
		(char *) style->elements, sizeof(MElementLink),
		style->numElements, ELEMENT_LINK_ROUND);
#else
	WCFREE(style->elements, MElementLink, style->numElements);
#endif
    }

    STATIC_FREE(keep, int, style->numElements);

    style->elements = eLinks;
    style->numElements = count;
}

/*
 *----------------------------------------------------------------------
 *
 * IStyle_ChangeElementsAux --
 *
 *	Update the list of elements used by a style. Elements
 *	may be inserted or deleted.
 *
 * Results:
 *	The list of elements in the style is updated.
 *
 * Side effects:
 *	Memory may be allocated/deallocated.
 *
 *----------------------------------------------------------------------
 */

static void
IStyle_ChangeElementsAux(
    TreeCtrl *tree,		/* Widget info. */
    IStyle *style,		/* Instance style to be updated. */
    int oldCount,		/* The previous number of elements. */
    int count,			/* The number of elements in the style after
				 * this routine finishes. */
    TreeElement *elemList,	/* List of master elements the style uses. */
    int *map			/* Array of indexes into the list of elements
				 * currently used by the style. */
    )
{
    IElementLink *eLink, *eLinks = NULL;
    int i, staticKeep[STATIC_SIZE], *keep = staticKeep;

    STATIC_ALLOC(keep, int, oldCount);

    if (count > 0) {
#ifdef ALLOC_HAX
	eLinks = (IElementLink *) TreeAlloc_CAlloc(tree->allocData,
		IElementLinkUid, sizeof(IElementLink), count,
		ELEMENT_LINK_ROUND);
#else
	eLinks = (IElementLink *) ckalloc(sizeof(IElementLink) * count);
#endif
    }

    /* Assume we are discarding all the old ElementLinks */
    for (i = 0; i < oldCount; i++)
	keep[i] = 0;

    for (i = 0; i < count; i++) {
	if (map[i] != -1) {
	    eLinks[i] = style->elements[map[i]];
	    keep[map[i]] = 1;
	} else {
	    eLink = &eLinks[i];
	    eLink->elem = elemList[i];
#ifdef CACHE_ELEM_SIZE
	    eLink->neededWidth = eLink->neededHeight = -1;
#endif
	}
    }

    if (oldCount > 0) {
	/* Free unused ElementLinks */
	for (i = 0; i < oldCount; i++) {
	    if (!keep[i]) {
		IElementLink_FreeResources(tree, &style->elements[i]);
	    }
	}
#ifdef ALLOC_HAX
	TreeAlloc_CFree(tree->allocData, IElementLinkUid,
		(char *) style->elements, sizeof(IElementLink),
		oldCount, ELEMENT_LINK_ROUND);
#else
	WCFREE(style->elements, IElementLink, oldCount);
#endif
    }

    STATIC_FREE(keep, int, oldCount);

    style->elements = eLinks;
}

/*
 *----------------------------------------------------------------------
 *
 * Style_ChangeElements --
 *
 *	Update the list of elements used by a style. Elements
 *	may be inserted or deleted.
 *
 * Results:
 *	The list of elements in the master style is updated. For
 *	each item-column using an instance of the master style,
 *	the list of elements is updated.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

static void
Style_ChangeElements(
    TreeCtrl *tree,		/* Widget info. */
    MStyle *masterStyle,	/* Master style to be updated. */
    int count,			/* The number of elements in the style after
				 * this routine finishes. */
    TreeElement *elemList,	/* List of master elements the style uses. */
    int *map			/* Array of indexes into the list of elements
				 * currently used by the style. */
    )
{
    TreeItem item;
    TreeItemColumn column;
    TreeColumn treeColumn;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    int columnIndex, layout;
    int updateDInfo = FALSE;
    IStyle *style;
    int i, j, k, oldCount;

    /* Update -union lists */
    for (i = 0; i < masterStyle->numElements; i++) {
	MElementLink *eLink = &masterStyle->elements[i];
	int staticKeep[STATIC_SIZE], *keep = staticKeep;
	int onionCnt = 0, *onion = NULL;

	if (eLink->onion == NULL)
	    continue;

	STATIC_ALLOC(keep, int, eLink->onionCount);

	/* Check every Element in this -union */
	for (j = 0; j < eLink->onionCount; j++) {
	    MElementLink *eLink2 = &masterStyle->elements[eLink->onion[j]];

	    /* Check the new list of Elements */
	    keep[j] = -1;
	    for (k = 0; k < count; k++) {
		/* This new Element is in the -union */
		if (elemList[k] == eLink2->elem) {
		    keep[j] = k;
		    onionCnt++;
		    break;
		}
	    }
	}

	if (onionCnt > 0) {
	    if (onionCnt != eLink->onionCount)
		onion = (int *) ckalloc(sizeof(int) * onionCnt);
	    else
		onion = eLink->onion;
	    k = 0;
	    for (j = 0; j < eLink->onionCount; j++) {
		if (keep[j] != -1)
		    onion[k++] = keep[j];
	    }
	}

	STATIC_FREE(keep, int, eLink->onionCount);

	if (onionCnt != eLink->onionCount) {
	    WCFREE(eLink->onion, int, eLink->onionCount);
	    eLink->onion = onion;
	    eLink->onionCount = onionCnt;
	}
    }

    oldCount = masterStyle->numElements;
    MStyle_ChangeElementsAux(tree, masterStyle, count, elemList, map);

    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
    while (hPtr != NULL) {
	item = (TreeItem) Tcl_GetHashValue(hPtr);
	treeColumn = tree->columns;
	column = TreeItem_GetFirstColumn(tree, item);
	columnIndex = 0;
	layout = FALSE;
	while (column != NULL) {
	    style = (IStyle *) TreeItemColumn_GetStyle(tree, column);
	    if ((style != NULL) && (style->master == masterStyle)) {
		IStyle_ChangeElementsAux(tree, style, oldCount, count, elemList, map);
		style->neededWidth = style->neededHeight = -1;
		Tree_InvalidateColumnWidth(tree, treeColumn);
		TreeItemColumn_InvalidateSize(tree, column);
		layout = TRUE;
	    }
	    columnIndex++;
	    column = TreeItemColumn_GetNext(tree, column);
	    treeColumn = TreeColumn_Next(treeColumn);
	}
	if (layout) {
	    TreeItem_InvalidateHeight(tree, item);
	    Tree_FreeItemDInfo(tree, item, NULL);
	    updateDInfo = TRUE;
	}
	hPtr = Tcl_NextHashEntry(&search);
    }
    if (updateDInfo)
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
}

/*
 *----------------------------------------------------------------------
 *
 * Style_ElemChanged --
 *
 *	Called when a master element or TreeCtrl is configured.
 *
 * Results:
 *	A check is made on each item-column to see if it is using
 *	the element. The size of any element/column/item affected
 *	is marked out-of-date.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

static void
Style_ElemChanged(
    TreeCtrl *tree,		/* Widget info. */
    MStyle *masterStyle,	/* Master style that uses the element. */
    TreeElement masterElem,	/* Master element affected by the change. */
    int masterElemIndex,	/* Index of masterElem in masterStyle. */
    int flagM,			/* Flags returned by TreeElementType.configProc()
				 * if the master element was configured,
				 * zero if the TreeCtrl was configured. */
    int flagT,			/* TREE_CONF_xxx flags if the TreeCtrl was
				 * configured, zero if the master element
				 * was configured. */
    int csM			/* CS_xxx flags returned by
				 * TreeElementType.changeProc(). */
    )
{
    TreeItem item;
    TreeItemColumn column;
    TreeColumn treeColumn;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    IElementLink *eLink;
    int columnIndex;
    TreeElementArgs args;
    IStyle *style;
    int eMask, cMask, iMask;
    int updateDInfo = FALSE;

    args.tree = tree;
    args.change.flagTree = flagT;
    args.change.flagMaster = flagM;
    args.change.flagSelf = 0;

    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
    while (hPtr != NULL) {
	item = (TreeItem) Tcl_GetHashValue(hPtr);
	treeColumn = tree->columns;
	column = TreeItem_GetFirstColumn(tree, item);
	columnIndex = 0;
	iMask = 0;
	while (column != NULL) {
	    cMask = 0;
	    style = (IStyle *) TreeItemColumn_GetStyle(tree, column);
	    if ((style != NULL) && (style->master == masterStyle)) {
		eLink = &style->elements[masterElemIndex];
		if (eLink->elem == masterElem) {
#ifdef CACHE_ELEM_SIZE
		    if (csM & CS_LAYOUT)
			eLink->neededWidth = eLink->neededHeight = -1;
#endif
		    cMask |= csM;
		}
		/* Instance element */
		else {
		    args.elem = eLink->elem;
		    eMask = (*masterElem->typePtr->changeProc)(&args);
#ifdef CACHE_ELEM_SIZE
		    if (eMask & CS_LAYOUT)
			eLink->neededWidth = eLink->neededHeight = -1;
#endif
		    cMask |= eMask;
		}
		iMask |= cMask;
		if (cMask & CS_LAYOUT) {
		    style->neededWidth = style->neededHeight = -1;
		    Tree_InvalidateColumnWidth(tree, treeColumn);
		    TreeItemColumn_InvalidateSize(tree, column);
		}
		else if (cMask & CS_DISPLAY) {
		    Tree_InvalidateItemDInfo(tree, treeColumn, item, NULL);
		}
	    }
	    columnIndex++;
	    column = TreeItemColumn_GetNext(tree, column);
	    treeColumn = TreeColumn_Next(treeColumn);
	}
	if (iMask & CS_LAYOUT) {
	    TreeItem_InvalidateHeight(tree, item);
	    Tree_FreeItemDInfo(tree, item, NULL);
	    updateDInfo = TRUE;
	}
	else if (iMask & CS_DISPLAY) {
	}
	hPtr = Tcl_NextHashEntry(&search);
    }
    if (updateDInfo)
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_GetButtonY --
 *
 *	Return the value of the -buttony style option.
 *
 * Results:
 *	Pixel value or -1 if unspecified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeStyle_GetButtonY(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_		/* Master or instance style token. */
    )
{
    MStyle *style = (MStyle *) style_;
    MStyle *master = (style->master != NULL) ? style->master : style;
    return (master->buttonYObj == NULL) ? -1 : master->buttonY;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_GetMaster --
 *
 *	Return the master style for an instance style.
 *
 * Results:
 *	Token for the master style.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeStyle
TreeStyle_GetMaster(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_		/* Instance style token. */
    )
{
    return (TreeStyle) ((IStyle *) style_)->master;
}

static Tcl_Obj *confImageObj = NULL;
static Tcl_Obj *confTextObj = NULL;

/*
 *----------------------------------------------------------------------
 *
 * Style_GetImageOrText --
 *
 *	Return the value of a configuration option for an element.
 *
 * Results:
 *	The result of Tk_GetOptionValue for an option of the first
 *	element of the proper type (if any), otherwise NULL.
 *
 * Side effects:
 *	A Tcl_Obj may be allocated.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
Style_GetImageOrText(
    TreeCtrl *tree,		/* Widget info. */
    IStyle *style,		/* Style. */
    TreeElementType *typePtr,	/* Type of element to look for. */
    CONST char *optionName,	/* Name of config option to query. */
    Tcl_Obj **optionNameObj	/* Pointer to a Tcl_Obj to hold the
				 * option name. Initialized
				 * on the first call. */
    )
{
    IElementLink *eLink;
    int i;

    if (*optionNameObj == NULL) {
	*optionNameObj = Tcl_NewStringObj(optionName, -1);
	Tcl_IncrRefCount(*optionNameObj);
    }

    for (i = 0; i < style->master->numElements; i++) {
	eLink = &style->elements[i];
	if (ELEMENT_TYPE_MATCHES(eLink->elem->typePtr, typePtr)) {
	    Tcl_Obj *resultObjPtr;
	    resultObjPtr = Tk_GetOptionValue(tree->interp,
		(char *) eLink->elem, eLink->elem->typePtr->optionTable,
		*optionNameObj, tree->tkwin);
	    return resultObjPtr;
	}
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_GetImage --
 *
 *	Return the value of the -image option for the first
 *	image element in a style (if any).
 *
 * Results:
 *	The result of Tk_GetOptionValue if the element was found,
 *	otherwise NULL.
 *
 * Side effects:
 *	A Tcl_Obj may be allocated.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TreeStyle_GetImage(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_		/* Token for style to examine. */
    )
{
    return Style_GetImageOrText(tree, (IStyle *) style_, &treeElemTypeImage,
	"-image", &confImageObj);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_GetText --
 *
 *	Return the value of the -text option for the first
 *	text element in a style (if any).
 *
 * Results:
 *	The result of Tk_GetOptionValue if the element was found,
 *	otherwise NULL.
 *
 * Side effects:
 *	A Tcl_Obj may be allocated.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TreeStyle_GetText(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_		/* Token for style to examine. */
    )
{
    return Style_GetImageOrText(tree, (IStyle *) style_, &treeElemTypeText,
	"-text", &confTextObj);
}

/*
 *----------------------------------------------------------------------
 *
 * Style_SetImageOrText --
 *
 *	Set the value of a configuration option for the first
 *	element of the proper type in a style (if any).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Size of the element and style will be marked out-of-date.
 *	A Tcl_Obj may be allocated.
 *
 *----------------------------------------------------------------------
 */

static int
Style_SetImageOrText(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item containing the style. Needed if
				 * a new instance Element is created. */
    TreeItemColumn column,	/* Item-column containing the style */
    IStyle *style,		/* The style */
    TreeElementType *typePtr,	/* Element type to look for. */
    CONST char *optionName,	/* NULL-terminated config option name. */
    Tcl_Obj **optionNameObj,	/* Pointer to Tcl_Obj to hold the option
				 * name; initialized on the first call. */
    Tcl_Obj *valueObj		/* New value for the config option. */
    )
{
    MStyle *masterStyle = style->master;
    IElementLink *eLink;
    int i;

    if (*optionNameObj == NULL) {
	*optionNameObj = Tcl_NewStringObj(optionName, -1);
	Tcl_IncrRefCount(*optionNameObj);
    }

    for (i = 0; i < masterStyle->numElements; i++) {
	TreeElement masterElem = masterStyle->elements[i].elem;
	if (ELEMENT_TYPE_MATCHES(masterElem->typePtr, typePtr)) {
	    Tcl_Obj *objv[2];
	    TreeElementArgs args;

	    eLink = Style_CreateElem(tree, item, column, style, masterElem, NULL);

	    objv[0] = *optionNameObj;
	    objv[1] = valueObj;
	    args.tree = tree;
	    args.elem = eLink->elem;
	    args.config.objc = 2;
	    args.config.objv = objv;
	    args.config.flagSelf = 0;
	    args.config.item = item;
	    args.config.column = column;
	    if ((*eLink->elem->typePtr->configProc)(&args) != TCL_OK)
		return TCL_ERROR;

	    args.change.flagSelf = args.config.flagSelf;
	    args.change.flagTree = 0;
	    args.change.flagMaster = 0;
	    (void) (*eLink->elem->typePtr->changeProc)(&args);

#ifdef CACHE_ELEM_SIZE
	    eLink->neededWidth = eLink->neededHeight = -1;
#endif
	    style->neededWidth = style->neededHeight = -1;
	    break;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_SetImage --
 *
 *	Set the value of the -image option for the first image
 *	element in a style (if any).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Size of the element and style will be marked out-of-date.
 *	A Tcl_Obj may be allocated.
 *
 *----------------------------------------------------------------------
 */

int
TreeStyle_SetImage(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item containing the style. */
    TreeItemColumn column,	/* Item-column containing the style. */
    TreeStyle style_,		/* The instance style. */
    Tcl_Obj *valueObj		/* New value for -image option. */
    )
{
    return Style_SetImageOrText(tree, item, column, (IStyle *) style_,
	&treeElemTypeImage, "-image", &confImageObj, valueObj);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_SetText --
 *
 *	Set the value of the -text option for the first text
 *	element in a style (if any).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Size of the element and style will be marked out-of-date.
 *	A Tcl_Obj may be allocated.
 *
 *----------------------------------------------------------------------
 */

int
TreeStyle_SetText(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item containing the style. */
    TreeItemColumn column,	/* Item-column containing the style. */
    TreeStyle style_,		/* The instance style. */
    Tcl_Obj *valueObj		/* New value for -text option. */
    )
{
    return Style_SetImageOrText(tree, item, column, (IStyle *) style_,
	&treeElemTypeText, "-text", &confTextObj, valueObj);
}

/*
 *----------------------------------------------------------------------
 *
 * Style_Deleted --
 *
 *	Called when a master style is about to be deleted. Any
 *	item-columns using an instance of the style have their style
 *	freed.
 *
 * Results:
 *	The TreeCtrl -defaultstyle option is updated if the deleted
 *	style was specified in the value of the option.
 *
 * Side effects:
 *	Display changes. Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

static void
Style_Deleted(
    TreeCtrl *tree,		/* Widget info. */
    MStyle *masterStyle		/* The master style being deleted. */
    )
{
    TreeItem item;
    TreeItemColumn column;
    TreeColumn treeColumn;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    IStyle *style;
    int columnIndex;

    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
    while (hPtr != NULL) {
	item = (TreeItem) Tcl_GetHashValue(hPtr);
	treeColumn = tree->columns;
	column = TreeItem_GetFirstColumn(tree, item);
	columnIndex = 0;
	while (column != NULL) {
	    style = (IStyle *) TreeItemColumn_GetStyle(tree, column);
	    if ((style != NULL) && (style->master == masterStyle)) {
		Tree_InvalidateColumnWidth(tree, treeColumn);
		TreeItemColumn_ForgetStyle(tree, column);
		TreeItem_InvalidateHeight(tree, item);
		Tree_FreeItemDInfo(tree, item, NULL);
	    }
	    columnIndex++;
	    column = TreeItemColumn_GetNext(tree, column);
	    treeColumn = TreeColumn_Next(treeColumn);
	}
	hPtr = Tcl_NextHashEntry(&search);
    }

    /* Update each column's -itemstyle option */
    treeColumn = tree->columns;
    while (treeColumn != NULL) {
	TreeColumn_StyleDeleted(treeColumn, (TreeStyle) masterStyle);
	treeColumn = TreeColumn_Next(treeColumn);
    }

#ifdef DEPRECATED
    /* Update -defaultstyle option */
    if (tree->defaultStyle.stylesObj != NULL) {
	Tcl_Obj *stylesObj = tree->defaultStyle.stylesObj;
	if (Tcl_IsShared(stylesObj)) {
	    stylesObj = Tcl_DuplicateObj(stylesObj);
	    Tcl_DecrRefCount(tree->defaultStyle.stylesObj);
	    Tcl_IncrRefCount(stylesObj);
	    tree->defaultStyle.stylesObj = stylesObj;
	}
	for (columnIndex = 0; columnIndex < tree->defaultStyle.numStyles; columnIndex++) {
	    Tcl_Obj *emptyObj;
	    if (tree->defaultStyle.styles[columnIndex] != (TreeStyle) masterStyle)
		continue;
	    tree->defaultStyle.styles[columnIndex] = NULL;
	    emptyObj = Tcl_NewObj();
	    Tcl_ListObjReplace(tree->interp, stylesObj, columnIndex, 1, 1, &emptyObj);
	}
    }
#endif /* DEPRECATED */

#ifdef DRAGIMAGE_STYLE
    TreeDragImage_StyleDeleted(tree->dragImage, (TreeStyle) masterStyle);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Element_Changed --
 *
 *	Called when a master element or TreeCtrl has been configured.
 *
 * Results:
 *	Every master and instance style using the element is updated.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

static void
Element_Changed(
    TreeCtrl *tree,		/* Widget info. */
    TreeElement masterElem,	/* Master element that may have changed. */
    int flagM,			/* Flags returned by TreeElementType.configProc()
				 * if the master element was configured,
				 * zero if the TreeCtrl was configured. */
    int flagT,			/* TREE_CONF_xxx flags if the TreeCtrl was
				 * configured, zero if the master element
				 * was configured. */
    int csM			/* CS_xxx flags returned by
				 * TreeElementType.changeProc(). */
    )
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    MStyle *masterStyle;
    MElementLink *eLink;
    int i;

    hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
    while (hPtr != NULL) {
	masterStyle = (MStyle *) Tcl_GetHashValue(hPtr);
	for (i = 0; i < masterStyle->numElements; i++) {
	    eLink = &masterStyle->elements[i];
	    if (eLink->elem == masterElem) {
		Style_ElemChanged(tree, masterStyle, masterElem, i, flagM, flagT, csM);
		break;
	    }
	}
	hPtr = Tcl_NextHashEntry(&search);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Element_Deleted --
 *
 *	Called when a master element is about to be deleted.
 *
 * Results:
 *	The list of elements in any master styles using the element is
 *	updated. Ditto for instance styles.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

static void
Element_Deleted(
    TreeCtrl *tree,		/* Widget info. */
    TreeElement masterElem	/* Master element being deleted. */
    )
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    MStyle *masterStyle;
    MElementLink *eLink;
    int i, j;

    hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
    while (hPtr != NULL) {
	masterStyle = (MStyle *) Tcl_GetHashValue(hPtr);
	for (i = 0; i < masterStyle->numElements; i++) {
	    eLink = &masterStyle->elements[i];
	    if (eLink->elem == masterElem) {
		TreeElement staticElemList[STATIC_SIZE],
		    *elemList = staticElemList;
		int staticElemMap[STATIC_SIZE], *elemMap = staticElemMap;

		STATIC_ALLOC(elemList, TreeElement, masterStyle->numElements);
		STATIC_ALLOC(elemMap, int, masterStyle->numElements);

		for (j = 0; j < masterStyle->numElements; j++) {
		    if (j == i)
			continue;
		    elemList[(j < i) ? j : (j - 1)] =
			masterStyle->elements[j].elem;
		    elemMap[(j < i) ? j : (j - 1)] = j;
		}
		Style_ChangeElements(tree, masterStyle,
		    masterStyle->numElements - 1, elemList, elemMap);
		STATIC_FREE(elemList, TreeElement, masterStyle->numElements + 1);
		STATIC_FREE(elemMap, int, masterStyle->numElements + 1);
		break;
	    }
	}
	hPtr = Tcl_NextHashEntry(&search);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_RedrawElement --
 *
 *	A STUB export. Schedules a redraw of the given item.
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
Tree_RedrawElement(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item containing the element. */
    TreeElement elem		/* The element that changed. */
    )
{
    /* Master element */
    if (elem->master == NULL) {
    }

    /* Instance element */
    else {
	Tree_InvalidateItemDInfo(tree, NULL, item, NULL);
    }
}

typedef struct Iterate
{
    TreeCtrl *tree;
    TreeItem item;
    TreeItemColumn column;
    int columnIndex;
    IStyle *style;
    TreeElementType *elemTypePtr;
    IElementLink *eLink;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
} Iterate;

static int IterateItem(Iterate *iter)
{
    int i;

    while (iter->column != NULL) {
	iter->style = (IStyle *) TreeItemColumn_GetStyle(iter->tree, iter->column);
	if (iter->style != NULL) {
	    for (i = 0; i < iter->style->master->numElements; i++) {
		iter->eLink = &iter->style->elements[i];
		if (ELEMENT_TYPE_MATCHES(iter->eLink->elem->typePtr, iter->elemTypePtr))
		    return 1;
	    }
	}
	iter->column = TreeItemColumn_GetNext(iter->tree, iter->column);
	iter->columnIndex++;
    }
    return 0;
}

TreeIterate
Tree_ElementIterateBegin(
    TreeCtrl *tree,
    TreeElementType *elemTypePtr)
{
    Iterate *iter;

    iter = (Iterate *) ckalloc(sizeof(Iterate));
    iter->tree = tree;
    iter->elemTypePtr = elemTypePtr;
    iter->hPtr = Tcl_FirstHashEntry(&tree->itemHash, &iter->search);
    while (iter->hPtr != NULL) {
	iter->item = (TreeItem) Tcl_GetHashValue(iter->hPtr);
	iter->column = TreeItem_GetFirstColumn(tree, iter->item);
	iter->columnIndex = 0;
	if (IterateItem(iter))
	    return (TreeIterate) iter;
	iter->hPtr = Tcl_NextHashEntry(&iter->search);
    }
    ckfree((char *) iter);
    return NULL;
}

TreeIterate
Tree_ElementIterateNext(
    TreeIterate iter_)
{
    Iterate *iter = (Iterate *) iter_;

    iter->column = TreeItemColumn_GetNext(iter->tree, iter->column);
    iter->columnIndex++;
    if (IterateItem(iter))
	return iter_;
    iter->hPtr = Tcl_NextHashEntry(&iter->search);
    while (iter->hPtr != NULL) {
	iter->item = (TreeItem) Tcl_GetHashValue(iter->hPtr);
	iter->column = TreeItem_GetFirstColumn(iter->tree, iter->item);
	iter->columnIndex = 0;
	if (IterateItem(iter))
	    return iter_;
	iter->hPtr = Tcl_NextHashEntry(&iter->search);
    }
    ckfree((char *) iter);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_ElementChangedItself --
 *
 *	Called when an element has reconfigured itself outside of
 *	any API calls. For example, when a window associated with a
 *	window element is resized, or a text element's -textvariable
 *	is set.
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
Tree_ElementChangedItself(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item containing the element. */
    TreeItemColumn column,	/* Item-column containing the element. */
    TreeElement elem,		/* The element that changed. */
    int flags,			/* Element-specific configuration flags. */
    int csM			/* CS_xxx flags detailing the effects of
				 * the change. */
    )
{
    /* Master element. */
    if (item == NULL) {
	Element_Changed(tree, elem, flags, 0, csM);
	return;
    }
    if (csM & CS_LAYOUT) {
	IStyle *style = (IStyle *) TreeItemColumn_GetStyle(tree, column);
	int i;
	IElementLink *eLink = NULL;
	int columnIndex;

	if (style == NULL)
	    panic("Tree_ElementChangedItself but style is NULL\n");

	for (i = 0; i < style->master->numElements; i++) {
	    eLink = &style->elements[i];
	    if (eLink->elem == elem)
		break;
	}

	if (eLink == NULL)
	    panic("Tree_ElementChangedItself but eLink is NULL\n");

	columnIndex = TreeItemColumn_Index(tree, item, column);

#ifdef CACHE_ELEM_SIZE
	eLink->neededWidth = eLink->neededHeight = -1;
#endif
	style->neededWidth = style->neededHeight = -1;

	Tree_InvalidateColumnWidth(tree, Tree_FindColumn(tree, columnIndex));
	TreeItemColumn_InvalidateSize(tree, column);
	TreeItem_InvalidateHeight(tree, item);
	Tree_FreeItemDInfo(tree, item, NULL);
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
    }
    else if (csM & CS_DISPLAY) {
	int columnIndex;

	columnIndex = TreeItemColumn_Index(tree, item, column);
	Tree_InvalidateItemDInfo(tree, Tree_FindColumn(tree, columnIndex),
		item, NULL);
    }
}

void Tree_ElementIterateChanged(TreeIterate iter_, int mask)
{
    Iterate *iter = (Iterate *) iter_;

    if (mask & CS_LAYOUT) {
#ifdef CACHE_ELEM_SIZE
	iter->eLink->neededWidth = iter->eLink->neededHeight = -1;
#endif
	iter->style->neededWidth = iter->style->neededHeight = -1;
	Tree_InvalidateColumnWidth(iter->tree,
	    Tree_FindColumn(iter->tree, iter->columnIndex));
	TreeItemColumn_InvalidateSize(iter->tree, iter->column);
	TreeItem_InvalidateHeight(iter->tree, iter->item);
	Tree_FreeItemDInfo(iter->tree, iter->item, NULL);
	Tree_DInfoChanged(iter->tree, DINFO_REDO_RANGES);
    }
    if (mask & CS_DISPLAY)
	Tree_InvalidateItemDInfo(iter->tree, NULL, iter->item, NULL);
}

TreeElement Tree_ElementIterateGet(TreeIterate iter_)
{
    Iterate *iter = (Iterate *) iter_;

    return iter->eLink->elem;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_TreeChanged --
 *
 *	Called when a TreeCtrl is configured. This handles changes to
 *	the -font option affecting text elements for example.
 *
 * Results:
 *	Calls the changeProc on every master element. Any elements
 *	affected by the change are eventually redisplayed.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

void
TreeStyle_TreeChanged(
    TreeCtrl *tree,		/* Widget info. */
    int flagT			/* TREE_CONF_xxx flags. */
    )
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    TreeElement masterElem;
    TreeElementArgs args;
    int eMask;

    if (flagT == 0)
	return;

    args.tree = tree;
    args.change.flagTree = flagT;
    args.change.flagMaster = 0;
    args.change.flagSelf = 0;

    hPtr = Tcl_FirstHashEntry(&tree->elementHash, &search);
    while (hPtr != NULL) {
	masterElem = (TreeElement) Tcl_GetHashValue(hPtr);
	args.elem = masterElem;
	eMask = (*masterElem->typePtr->changeProc)(&args);
	Element_Changed(tree, masterElem, 0, flagT, eMask);
	hPtr = Tcl_NextHashEntry(&search);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_ElementCget --
 *
 *	This procedure is invoked to process the [item element cget]
 *	widget command.  See the user documentation for details on what
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
TreeStyle_ElementCget(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item containing the element. */
    TreeItemColumn column,	/* Item-column containing the element. */
    TreeStyle style_,		/* Style containing the element. */
    Tcl_Obj *elemObj,		/* Name of the element. */
    Tcl_Obj *optionNameObj	/* Name of the config option. */
    )
{
    IStyle *style = (IStyle *) style_;
    Tcl_Obj *resultObjPtr = NULL;
    TreeElement elem;
    IElementLink *eLink;

    if (Element_FromObj(tree, elemObj, &elem) != TCL_OK)
	return TCL_ERROR;

    eLink = IStyle_FindElem(tree, style, elem, NULL);
    if ((eLink != NULL) && (eLink->elem == elem)) {
	int index = TreeItemColumn_Index(tree, item, column);
	TreeColumn treeColumn = Tree_FindColumn(tree, index);

	FormatResult(tree->interp,
	    "element %s is not configured in item %s%d column %s%d",
	    elem->name, tree->itemPrefix, TreeItem_GetID(tree, item),
	    tree->columnPrefix, TreeColumn_GetID(treeColumn));
	return TCL_ERROR;
    }
    if (eLink == NULL) {
	FormatResult(tree->interp, "style %s does not use element %s",
	    style->master->name, elem->name);
	return TCL_ERROR;
    }

    resultObjPtr = Tk_GetOptionValue(tree->interp, (char *) eLink->elem,
	eLink->elem->typePtr->optionTable, optionNameObj, tree->tkwin);
    if (resultObjPtr == NULL)
	return TCL_ERROR;
    Tcl_SetObjResult(tree->interp, resultObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_ElementConfigure --
 *
 *	This procedure is invoked to process the [item element configure]
 *	widget command.  See the user documentation for details on what
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
TreeStyle_ElementConfigure(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item containing the element. */
    TreeItemColumn column,	/* Item-column containing the element. */
    TreeStyle style_,		/* Style containing the element. */
    Tcl_Obj *elemObj,		/* Name of the element. */
    int objc,			/* Number of arguments. */
    Tcl_Obj **objv,		/* Argument values. */
    int *eMask			/* Returned CS_xxx flags. */
    )
{
    IStyle *style = (IStyle *) style_;
    TreeElement elem;
    IElementLink *eLink;
    TreeElementArgs args;

    (*eMask) = 0;

    if (Element_FromObj(tree, elemObj, &elem) != TCL_OK)
	return TCL_ERROR;

    if (objc <= 1) {
	Tcl_Obj *resultObjPtr;

	eLink = IStyle_FindElem(tree, style, elem, NULL);
	if ((eLink != NULL) && (eLink->elem == elem)) {
	    int index = TreeItemColumn_Index(tree, item, column);
	    TreeColumn treeColumn = Tree_FindColumn(tree, index);

	    FormatResult(tree->interp,
		"element %s is not configured in item %s%d column %s%d",
		elem->name, tree->itemPrefix, TreeItem_GetID(tree, item),
		tree->columnPrefix, TreeColumn_GetID(treeColumn));
	    return TCL_ERROR;
	}
	if (eLink == NULL) {
	    FormatResult(tree->interp, "style %s does not use element %s",
		style->master->name, elem->name);
	    return TCL_ERROR;
	}

	resultObjPtr = Tk_GetOptionInfo(tree->interp, (char *) eLink->elem,
	    eLink->elem->typePtr->optionTable,
	    (objc == 0) ? (Tcl_Obj *) NULL : objv[0],
	    tree->tkwin);
	if (resultObjPtr == NULL)
	    return TCL_ERROR;
	Tcl_SetObjResult(tree->interp, resultObjPtr);
    } else {
	int isNew;

	eLink = Style_CreateElem(tree, item, column, style, elem, &isNew);
	if (eLink == NULL) {
	    FormatResult(tree->interp, "style %s does not use element %s",
		style->master->name, elem->name);
	    return TCL_ERROR;
	}

	/* Do this before configProc(). If eLink was just allocated and an
	 * error occurs in configProc() it won't be done */
	(*eMask) = 0;
	if (isNew) {
#ifdef CACHE_ELEM_SIZE
	    eLink->neededWidth = eLink->neededHeight = -1;
#endif
	    style->neededWidth = style->neededHeight = -1;
	    (*eMask) = CS_DISPLAY | CS_LAYOUT;
	}

	args.tree = tree;
	args.elem = eLink->elem;
	args.config.objc = objc;
	args.config.objv = objv;
	args.config.flagSelf = 0;
	args.config.item = item;
	args.config.column = column;
	if ((*args.elem->typePtr->configProc)(&args) != TCL_OK)
	    return TCL_ERROR;

	args.change.flagSelf = args.config.flagSelf;
	args.change.flagTree = 0;
	args.change.flagMaster = 0;
	(*eMask) |= (*elem->typePtr->changeProc)(&args);

	if (!isNew && ((*eMask) & CS_LAYOUT)) {
#ifdef CACHE_ELEM_SIZE
	    eLink->neededWidth = eLink->neededHeight = -1;
#endif
	    style->neededWidth = style->neededHeight = -1;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_ElementActual --
 *
 *	This procedure is invoked to process the [item element perstate]
 *	widget command.  See the user documentation for details on what
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
TreeStyle_ElementActual(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_,		/* The style. */
    int state,			/* STATE_xxx flags. */
    Tcl_Obj *elemObj,		/* Name of the element. */
    Tcl_Obj *optionNameObj	/* Name of the config option. */
    )
{
    IStyle *style = (IStyle *) style_;
    TreeElement masterElem;
    IElementLink *eLink;
    TreeElementArgs args;

    if (Element_FromObj(tree, elemObj, &masterElem) != TCL_OK)
	return TCL_ERROR;

    eLink = IStyle_FindElem(tree, style, masterElem, NULL);
    if (eLink == NULL) {
	FormatResult(tree->interp, "style %s does not use element %s",
	    style->master->name, masterElem->name);
	return TCL_ERROR;
    }

    args.tree = tree;
    args.elem = eLink->elem;
    args.state = state;
    args.actual.obj = optionNameObj;
    return (*masterElem->typePtr->actualProc)(&args);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeElementCmd --
 *
 *	This procedure is invoked to process the [element]
 *	widget command.  See the user documentation for details on what
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
TreeElementCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    static CONST char *commandNames[] = {
	"cget", "configure", "create", "delete", "names", "perstate", "type",
	(char *) NULL
    };
    enum {
	COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_CREATE, COMMAND_DELETE,
	COMMAND_NAMES, COMMAND_PERSTATE, COMMAND_TYPE
    };
    int index;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandNames, "command", 0,
	&index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
	case COMMAND_CGET: {
	    Tcl_Obj *resultObjPtr = NULL;
	    TreeElement elem;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "name option");
		return TCL_ERROR;
	    }
	    if (Element_FromObj(tree, objv[3], &elem) != TCL_OK)
		return TCL_ERROR;
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) elem,
		elem->typePtr->optionTable, objv[4], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	case COMMAND_CONFIGURE: {
	    Tcl_Obj *resultObjPtr = NULL;
	    TreeElement elem;
	    int eMask;

	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "name ?option? ?value option value ...?");
		return TCL_ERROR;
	    }
	    if (Element_FromObj(tree, objv[3], &elem) != TCL_OK)
		return TCL_ERROR;
	    if (objc <= 5) {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) elem,
		    elem->typePtr->optionTable,
		    (objc == 4) ? (Tcl_Obj *) NULL : objv[4],
		    tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
	    } else {
		TreeElementArgs args;

		args.tree = tree;
		args.elem = elem;
		args.config.objc = objc - 4;
		args.config.objv = objv + 4;
		args.config.flagSelf = 0;
		args.config.item = NULL;
		args.config.column = NULL;
		if ((*elem->typePtr->configProc)(&args) != TCL_OK)
		    return TCL_ERROR;

		args.change.flagSelf = args.config.flagSelf;
		args.change.flagTree = 0;
		args.change.flagMaster = 0;
		eMask = (*elem->typePtr->changeProc)(&args);

		Element_Changed(tree, elem, args.change.flagSelf, 0, eMask);
	    }
	    break;
	}

	case COMMAND_CREATE: {
	    char *name;
	    int length;
	    int isNew;
	    TreeElement elem;
	    TreeElementType *typePtr;
	    Tcl_HashEntry *hPtr;

	    if (objc < 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "name type ?option value ...?");
		return TCL_ERROR;
	    }
	    name = Tcl_GetStringFromObj(objv[3], &length);
	    if (!length)
		return TCL_ERROR;
	    hPtr = Tcl_FindHashEntry(&tree->elementHash, name);
	    if (hPtr != NULL) {
		FormatResult(interp, "element \"%s\" already exists", name);
		return TCL_ERROR;
	    }
	    if (TreeElement_TypeFromObj(tree, objv[4], &typePtr) != TCL_OK)
		return TCL_ERROR;
	    elem = Element_CreateAndConfig(tree, NULL, NULL, NULL, typePtr, name, objc - 5, objv + 5);
	    if (elem == NULL)
		return TCL_ERROR;
	    hPtr = Tcl_CreateHashEntry(&tree->elementHash, name, &isNew);
	    Tcl_SetHashValue(hPtr, elem);
	    Tcl_SetObjResult(interp, Element_ToObj(elem));
	    break;
	}

	case COMMAND_DELETE: {
	    TreeElement elem;
	    int i;

	    for (i = 3; i < objc; i++) {
		if (Element_FromObj(tree, objv[i], &elem) != TCL_OK)
		    return TCL_ERROR;
		Element_Deleted(tree, elem);
		Element_FreeResources(tree, elem);
	    }
	    break;
	}

	case COMMAND_NAMES: {
	    Tcl_Obj *listObj;
	    Tcl_HashSearch search;
	    Tcl_HashEntry *hPtr;
	    TreeElement elem;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, NULL);
		return TCL_ERROR;
	    }
	    listObj = Tcl_NewListObj(0, NULL);
	    hPtr = Tcl_FirstHashEntry(&tree->elementHash, &search);
	    while (hPtr != NULL) {
		elem = (TreeElement) Tcl_GetHashValue(hPtr);
		Tcl_ListObjAppendElement(interp, listObj, Element_ToObj(elem));
		hPtr = Tcl_NextHashEntry(&search);
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	/* T element perstate E option stateList */
	case COMMAND_PERSTATE: {
	    TreeElement elem;
	    int states[3];
	    TreeElementArgs args;

	    if (objc != 6) {
		Tcl_WrongNumArgs(tree->interp, 3, objv,
		    "element option stateList");
		return TCL_ERROR;
	    }

	    if (Element_FromObj(tree, objv[3], &elem) != TCL_OK)
		return TCL_ERROR;

	    if (Tree_StateFromListObj(tree, objv[5], states,
		    SFO_NOT_OFF | SFO_NOT_TOGGLE) != TCL_OK)
		return TCL_ERROR;

	    args.tree = tree;
	    args.elem = elem;
	    args.state = states[STATE_OP_ON];
	    args.actual.obj = objv[4];
	    return (*elem->typePtr->actualProc)(&args);
	}

	case COMMAND_TYPE: {
	    TreeElement elem;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "name");
		return TCL_ERROR;
	    }
	    if (Element_FromObj(tree, objv[3], &elem) != TCL_OK)
		return TCL_ERROR;
	    Tcl_SetResult(interp, elem->typePtr->name, TCL_STATIC); /* Tk_Uid */
	    break;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Style_CreateAndConfig --
 *
 *	Allocate and initialize a master style.
 *
 * Results:
 *	Pointer to the new Style, or NULL if an error occurs.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static MStyle *
Style_CreateAndConfig(
    TreeCtrl *tree,		/* Widget info. */
    char *name,			/* Name of new style. */
    int objc,			/* Number of config-option arg-value pairs. */
    Tcl_Obj *CONST objv[]	/* Config-option arg-value pairs. */
    )
{
    MStyle *style;

#ifdef ALLOC_HAX
    style = (MStyle *) TreeAlloc_Alloc(tree->allocData, MStyleUid,
	    sizeof(MStyle));
#else
    style = (MStyle *) ckalloc(sizeof(MStyle));
#endif
    memset(style, '\0', sizeof(MStyle));
    style->name = Tk_GetUid(name);

    if (Tk_InitOptions(tree->interp, (char *) style,
	tree->styleOptionTable, tree->tkwin) != TCL_OK) {
#ifdef ALLOC_HAX
	TreeAlloc_Free(tree->allocData, MStyleUid, (char *) style, sizeof(MStyle));
#else
	WFREE(style, MStyle);
#endif
	return NULL;
    }

    if (Tk_SetOptions(tree->interp, (char *) style,
	tree->styleOptionTable, objc, objv, tree->tkwin,
	NULL, NULL) != TCL_OK) {
	Tk_FreeConfigOptions((char *) style, tree->styleOptionTable, tree->tkwin);
#ifdef ALLOC_HAX
	TreeAlloc_Free(tree->allocData, MStyleUid, (char *) style, sizeof(MStyle));
#else
	WFREE(style, MStyle);
#endif
	return NULL;
    }

    return style;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_ListElements --
 *
 *	Creates a Tcl list with the names of elements in a style.
 *
 * Results:
 *	If the style is a master style, the interpreter result holds
 *	a list of each element in the style. If the style is an
 *	instance style, the interpreter result holds a list of those
 *	elements configured for the style (i.e., instance elements).
 *
 * Side effects:
 *	Memory is allocated, interpreter result changed.
 *
 *----------------------------------------------------------------------
 */

void
TreeStyle_ListElements(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_		/* The style. */
    )
{
    MStyle *masterStyle = (MStyle *) style_;
    IStyle *style = (IStyle *) style_;
    Tcl_Obj *listObj;
    TreeElement elem;
    int i, numElements = TreeStyle_NumElements(tree, style_);

    if (numElements <= 0)
	return;

    listObj = Tcl_NewListObj(0, NULL);
    for (i = 0; i < numElements; i++) {
	if (style->master != NULL) {
	    elem = style->elements[i].elem;
	    if (elem->master == NULL)
		continue;
	} else {
	    elem = masterStyle->elements[i].elem;
	}
	Tcl_ListObjAppendElement(tree->interp, listObj, Element_ToObj(elem));
    }
    Tcl_SetObjResult(tree->interp, listObj);
}

enum {
    OPTION_DETACH, OPTION_DRAW, OPTION_EXPAND, OPTION_HEIGHT, OPTION_iEXPAND,
    OPTION_INDENT, OPTION_iPADX, OPTION_iPADY, OPTION_MAXHEIGHT,
    OPTION_MAXWIDTH, OPTION_MINHEIGHT, OPTION_MINWIDTH, OPTION_PADX,
    OPTION_PADY, OPTION_SQUEEZE, OPTION_STICKY, OPTION_UNION,
    OPTION_WIDTH, OPTION_VISIBLE
};

/*
 *----------------------------------------------------------------------
 *
 * LayoutOptionToObj --
 *
 *	Return a Tcl_Obj holding the value of a style layout option
 *	for an element.
 *
 * Results:
 *	Pointer to a new Tcl_Obj or NULL if the option has no value.
 *
 * Side effects:
 *	A Tcl_Obj may be allocated.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
LayoutOptionToObj(
    TreeCtrl *tree,		/* Widget info. */
    MStyle *style,		/* Master style using the element. */
    MElementLink *eLink,	/* Layout info for the element. */
    int option			/* OPTION_xxx constant. */
    )
{
    Tcl_Interp *interp = tree->interp;

    switch (option) {
	case OPTION_PADX:
	    return TreeCtrl_NewPadAmountObj(eLink->ePadX);
	case OPTION_PADY:
	    return TreeCtrl_NewPadAmountObj(eLink->ePadY);
	case OPTION_iPADX:
	    return TreeCtrl_NewPadAmountObj(eLink->iPadX);
	case OPTION_iPADY:
	    return TreeCtrl_NewPadAmountObj(eLink->iPadY);
	case OPTION_DETACH:
	    return Tcl_NewStringObj(IS_DETACH(eLink) ? "yes" : "no", -1);
	case OPTION_EXPAND: {
	    char flags[4];
	    int n = 0;

	    if (eLink->flags & ELF_eEXPAND_W) flags[n++] = 'w';
	    if (eLink->flags & ELF_eEXPAND_N) flags[n++] = 'n';
	    if (eLink->flags & ELF_eEXPAND_E) flags[n++] = 'e';
	    if (eLink->flags & ELF_eEXPAND_S) flags[n++] = 's';
	    if (n)
		return Tcl_NewStringObj(flags, n);
	    break;
	}
	case OPTION_iEXPAND: {
	    char flags[6];
	    int n = 0;

	    if (eLink->flags & ELF_iEXPAND_X) flags[n++] = 'x';
	    if (eLink->flags & ELF_iEXPAND_Y) flags[n++] = 'y';
	    if (eLink->flags & ELF_iEXPAND_W) flags[n++] = 'w';
	    if (eLink->flags & ELF_iEXPAND_N) flags[n++] = 'n';
	    if (eLink->flags & ELF_iEXPAND_E) flags[n++] = 'e';
	    if (eLink->flags & ELF_iEXPAND_S) flags[n++] = 's';
	    if (n)
		return Tcl_NewStringObj(flags, n);
	    break;
	}
	case OPTION_INDENT:
	    return Tcl_NewStringObj((eLink->flags & ELF_INDENT) ? "yes" : "no", -1);
	case OPTION_SQUEEZE: {
	    char flags[2];
	    int n = 0;

	    if (eLink->flags & ELF_SQUEEZE_X) flags[n++] = 'x';
	    if (eLink->flags & ELF_SQUEEZE_Y) flags[n++] = 'y';
	    if (n)
		return Tcl_NewStringObj(flags, n);
	    break;
	}
	case OPTION_UNION: {
	    int i;
	    Tcl_Obj *objPtr;

	    if (eLink->onionCount == 0)
		break;
	    objPtr = Tcl_NewListObj(0, NULL);
	    for (i = 0; i < eLink->onionCount; i++)
		Tcl_ListObjAppendElement(interp, objPtr,
		    Element_ToObj(style->elements[eLink->onion[i]].elem));
	    return objPtr;
	}
	case OPTION_MAXHEIGHT: {
	    if (eLink->maxHeight >= 0)
		return Tcl_NewIntObj(eLink->maxHeight);
	    break;
	}
	case OPTION_MINHEIGHT: {
	    if (eLink->minHeight >= 0)
		return Tcl_NewIntObj(eLink->minHeight);
	    break;
	}
	case OPTION_HEIGHT: {
	    if (eLink->fixedHeight >= 0)
		return Tcl_NewIntObj(eLink->fixedHeight);
	    break;
	}
	case OPTION_MAXWIDTH: {
	    if (eLink->maxWidth >= 0)
		return Tcl_NewIntObj(eLink->maxWidth);
	    break;
	}
	case OPTION_MINWIDTH: {
	    if (eLink->minWidth >= 0)
		return Tcl_NewIntObj(eLink->minWidth);
	    break;
	}
	case OPTION_WIDTH: {
	    if (eLink->fixedWidth >= 0)
		return Tcl_NewIntObj(eLink->fixedWidth);
	    break;
	}
	case OPTION_STICKY: {
	    char flags[4];
	    int n = 0;

	    if (eLink->flags & ELF_STICKY_W) flags[n++] = 'w';
	    if (eLink->flags & ELF_STICKY_N) flags[n++] = 'n';
	    if (eLink->flags & ELF_STICKY_E) flags[n++] = 'e';
	    if (eLink->flags & ELF_STICKY_S) flags[n++] = 's';
	    if (n)
		return Tcl_NewStringObj(flags, n);
	    break;
	}
	case OPTION_DRAW: {
	    return eLink->draw.obj;
	}
	case OPTION_VISIBLE: {
	    return eLink->visible.obj;
	}
    }
    return NULL;
}

static int
UnionRecursiveCheck(
    MStyle *mstyle,
    int iElemUnion,
    int iElemFind
    )
{
    int i;

    for (i = 0; i < mstyle->elements[iElemUnion].onionCount; i++) {
	if (mstyle->elements[iElemUnion].onion[i] == iElemFind)
	    return 1;
	if (UnionRecursiveCheck(mstyle, mstyle->elements[iElemUnion].onion[i], iElemFind))
	    return 1;
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * StyleLayoutCmd --
 *
 *	This procedure is invoked to process the [style layout]
 *	widget command.  See the user documentation for details on what
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
StyleLayoutCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* The current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    TreeStyle _style;
    MStyle *style;
    TreeElement elem;
    MElementLink saved, *eLink;
    int i, index, eIndex;
    static CONST char *optionNames[] = {
	"-detach", "-draw", "-expand", "-height", "-iexpand",
	"-indent", "-ipadx", "-ipady", "-maxheight", "-maxwidth", "-minheight",
	"-minwidth", "-padx", "-pady", "-squeeze", "-sticky", "-union",
	"-width", "-visible",
	(char *) NULL
    };

    if (objc < 5) {
	Tcl_WrongNumArgs(interp, 3, objv, "name element ?option? ?value? ?option value ...?");
	return TCL_ERROR;
    }

    if (TreeStyle_FromObj(tree, objv[3], &_style) != TCL_OK)
	return TCL_ERROR;
    style = (MStyle *) _style;

    if (Element_FromObj(tree, objv[4], &elem) != TCL_OK)
	return TCL_ERROR;

    eLink = MStyle_FindElem(tree, style, elem, &eIndex);
    if (eLink == NULL) {
	FormatResult(interp, "style %s does not use element %s",
	    style->name, elem->name);
	return TCL_ERROR;
    }

    /* T style layout S E */
    if (objc == 5) {
	Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
	Tcl_Obj *objPtr;

	for (i = 0; optionNames[i] != NULL; i++) {
	    Tcl_ListObjAppendElement(interp, listObj,
		Tcl_NewStringObj(optionNames[i], -1));
	    objPtr = LayoutOptionToObj(tree, style, eLink, i);
	    Tcl_ListObjAppendElement(interp, listObj,
		objPtr ? objPtr : Tcl_NewObj());
	}
	Tcl_SetObjResult(interp, listObj);
	return TCL_OK;
    }

    /* T style layout S E option */
    if (objc == 6) {
	Tcl_Obj *objPtr;

	if (Tcl_GetIndexFromObj(interp, objv[5], optionNames, "option",
	    0, &index) != TCL_OK)
	    return TCL_ERROR;
	objPtr = LayoutOptionToObj(tree, style, eLink, index);
	if (objPtr != NULL)
	    Tcl_SetObjResult(interp, objPtr);
	return TCL_OK;
    }

    saved = *eLink;

    for (i = 5; i < objc; i += 2) {
	if (i + 2 > objc) {
	    FormatResult(interp, "value for \"%s\" missing",
		Tcl_GetString(objv[i]));
	    goto badConfig;
	}
	if (Tcl_GetIndexFromObj(interp, objv[i], optionNames, "option",
	    0, &index) != TCL_OK) {
	    goto badConfig;
	}
	switch (index) {
	    case OPTION_PADX: {
		if (TreeCtrl_GetPadAmountFromObj(interp,
		    tree->tkwin, objv[i + 1],
		    &eLink->ePadX[PAD_TOP_LEFT],
		    &eLink->ePadX[PAD_BOTTOM_RIGHT]) != TCL_OK)
		    goto badConfig;
		break;
	    }
	    case OPTION_PADY: {
		if (TreeCtrl_GetPadAmountFromObj(interp,
		    tree->tkwin, objv[i + 1],
		    &eLink->ePadY[PAD_TOP_LEFT],
		    &eLink->ePadY[PAD_BOTTOM_RIGHT]) != TCL_OK)
		    goto badConfig;
		break;
	    }
	    case OPTION_iPADX: {
		if (TreeCtrl_GetPadAmountFromObj(interp,
		    tree->tkwin, objv[i + 1],
		    &eLink->iPadX[PAD_TOP_LEFT],
		    &eLink->iPadX[PAD_BOTTOM_RIGHT]) != TCL_OK)
		    goto badConfig;
		break;
	    }
	    case OPTION_iPADY: {
		if (TreeCtrl_GetPadAmountFromObj(interp,
		    tree->tkwin, objv[i + 1],
		    &eLink->iPadY[PAD_TOP_LEFT],
		    &eLink->iPadY[PAD_BOTTOM_RIGHT]) != TCL_OK)
		    goto badConfig;
		break;
	    }
	    case OPTION_DETACH: {
		int detach;
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &detach) != TCL_OK)
		    goto badConfig;
		if (detach)
		    eLink->flags |= ELF_DETACH;
		else
		    eLink->flags &= ~ELF_DETACH;
		break;
	    }
	    case OPTION_EXPAND: {
		static const CharFlag charFlags[] = {
		    { 'n', ELF_eEXPAND_N },
		    { 'e', ELF_eEXPAND_E },
		    { 's', ELF_eEXPAND_S },
		    { 'w', ELF_eEXPAND_W },
		    { 0, 0 }
		};
		if (Tree_GetFlagsFromObj(tree, objv[i + 1], "expand value",
			charFlags, &eLink->flags) != TCL_OK) {
		    goto badConfig;
		}
		break;
	    }
	    case OPTION_iEXPAND: {
		static const CharFlag charFlags[] = {
		    { 'x', ELF_iEXPAND_X },
		    { 'y', ELF_iEXPAND_Y },
		    { 'n', ELF_iEXPAND_N },
		    { 'e', ELF_iEXPAND_E },
		    { 's', ELF_iEXPAND_S },
		    { 'w', ELF_iEXPAND_W },
		    { 0, 0 }
		};
		if (Tree_GetFlagsFromObj(tree, objv[i + 1], "iexpand value",
			charFlags, &eLink->flags) != TCL_OK) {
		    goto badConfig;
		}
		break;
	    }
	    case OPTION_INDENT: {
		int indent;
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &indent) != TCL_OK)
		    goto badConfig;
		if (indent)
		    eLink->flags |= ELF_INDENT;
		else
		    eLink->flags &= ~ELF_INDENT;
		break;
	    }
	    case OPTION_SQUEEZE: {
		static const CharFlag charFlags[] = {
		    { 'x', ELF_SQUEEZE_X },
		    { 'y', ELF_SQUEEZE_Y },
		    { 0, 0 }
		};
		if (Tree_GetFlagsFromObj(tree, objv[i + 1], "squeeze value",
			charFlags, &eLink->flags) != TCL_OK) {
		    goto badConfig;
		}
		break;
	    }
	    case OPTION_UNION: {
		int objc1;
		Tcl_Obj **objv1;
		int j, k, eIndex2, *onion, count = 0;

		if (Tcl_ListObjGetElements(interp, objv[i + 1],
		    &objc1, &objv1) != TCL_OK)
		    goto badConfig;
		if (objc1 == 0) {
		    if (eLink->onion != NULL) {
			if (eLink->onion != saved.onion)
			    WCFREE(eLink->onion, int, eLink->onionCount);
			eLink->onionCount = 0;
			eLink->onion = NULL;
		    }
		    break;
		}
		onion = (int *) ckalloc(sizeof(int) * objc1);
		for (j = 0; j < objc1; j++) {
		    TreeElement elem2;
		    MElementLink *eLink2;

		    if (Element_FromObj(tree, objv1[j], &elem2) != TCL_OK) {
			ckfree((char *) onion);
			goto badConfig;
		    }

		    eLink2 = MStyle_FindElem(tree, style, elem2, &eIndex2);
		    if (eLink2 == NULL) {
			ckfree((char *) onion);
			FormatResult(interp,
			    "style %s does not use element %s",
			    style->name, elem2->name);
			goto badConfig;
		    }
		    if (eLink == eLink2) {
			ckfree((char *) onion);
			FormatResult(interp,
			    "element %s can't form union with itself",
			    elem2->name);
			goto badConfig;
		    }
		    if (UnionRecursiveCheck(style, eIndex2, eIndex)) {
			ckfree((char *) onion);
			FormatResult(interp,
			    "can't form a recursive union with element %s",
			    elem2->name);
			goto badConfig;
		    }
		    /* Silently ignore duplicates */
		    for (k = 0; k < count; k++) {
			if (onion[k] == eIndex2)
			    break;
		    }
		    if (k < count)
			continue;
		    onion[count++] = eIndex2;
		}
		if ((eLink->onion != NULL) && (eLink->onion != saved.onion))
		    WCFREE(eLink->onion, int, eLink->onionCount);
		if (count == objc1)
		    eLink->onion = onion;
		else {
		    eLink->onion = (int *) ckalloc(sizeof(int) * count);
		    for (k = 0; k < count; k++)
			eLink->onion[k] = onion[k];
		    ckfree((char *) onion);
		}
		eLink->onionCount = count;
		break;
	    }
	    case OPTION_MAXHEIGHT: {
		int height;
		if (ObjectIsEmpty(objv[i + 1])) {
		    eLink->maxHeight = -1;
		    break;
		}
		if ((Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
		    &height) != TCL_OK) || (height < 0)) {
		    FormatResult(interp, "bad screen distance \"%s\"",
			Tcl_GetString(objv[i + 1]));
		    goto badConfig;
		}
		eLink->maxHeight = height;
		break;
	    }
	    case OPTION_MINHEIGHT: {
		int height;
		if (ObjectIsEmpty(objv[i + 1])) {
		    eLink->minHeight = -1;
		    break;
		}
		if ((Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
		    &height) != TCL_OK) || (height < 0)) {
		    FormatResult(interp, "bad screen distance \"%s\"",
			Tcl_GetString(objv[i + 1]));
		    goto badConfig;
		}
		eLink->minHeight = height;
		break;
	    }
	    case OPTION_HEIGHT: {
		int height;
		if (ObjectIsEmpty(objv[i + 1])) {
		    eLink->fixedHeight = -1;
		    break;
		}
		if ((Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
			&height) != TCL_OK) || (height < 0)) {
		    FormatResult(interp, "bad screen distance \"%s\"",
			Tcl_GetString(objv[i + 1]));
		    goto badConfig;
		}
		eLink->fixedHeight = height;
		break;
	    }
	    case OPTION_MAXWIDTH: {
		int width;
		if (ObjectIsEmpty(objv[i + 1])) {
		    eLink->maxWidth = -1;
		    break;
		}
		if ((Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
		    &width) != TCL_OK) || (width < 0)) {
		    FormatResult(interp, "bad screen distance \"%s\"",
			Tcl_GetString(objv[i + 1]));
		    goto badConfig;
		}
		eLink->maxWidth = width;
		break;
	    }
	    case OPTION_MINWIDTH: {
		int width;
		if (ObjectIsEmpty(objv[i + 1])) {
		    eLink->minWidth = -1;
		    break;
		}
		if ((Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
			&width) != TCL_OK) || (width < 0)) {
		    FormatResult(interp, "bad screen distance \"%s\"",
			Tcl_GetString(objv[i + 1]));
		    goto badConfig;
		}
		eLink->minWidth = width;
		break;
	    }
	    case OPTION_WIDTH: {
		int width;
		if (ObjectIsEmpty(objv[i + 1])) {
		    eLink->fixedWidth = -1;
		    break;
		}
		if ((Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
			&width) != TCL_OK) || (width < 0)) {
		    FormatResult(interp, "bad screen distance \"%s\"",
			Tcl_GetString(objv[i + 1]));
		    goto badConfig;
		}
		eLink->fixedWidth = width;
		break;
	    }
	    case OPTION_STICKY: {
		static const CharFlag charFlags[] = {
		    { 'n', ELF_STICKY_N },
		    { 'e', ELF_STICKY_E },
		    { 's', ELF_STICKY_S },
		    { 'w', ELF_STICKY_W },
		    { 0, 0 }
		};
		if (Tree_GetFlagsFromObj(tree, objv[i + 1], "sticky value",
			charFlags, &eLink->flags) != TCL_OK) {
		    goto badConfig;
		}
		break;
	    }
	    case OPTION_DRAW:
	    case OPTION_VISIBLE: {
		PerStateInfo *psi, *psiSaved;

		if (index == OPTION_DRAW) {
		    psi = &eLink->draw;
		    psiSaved = &saved.draw;
		} else {
		    psi = &eLink->visible;
		    psiSaved = &saved.visible;
		}

		/* Already configured this once. */
		if (psi->obj != NULL && psi->obj != psiSaved->obj) {
		    PerStateInfo_Free(tree, &pstBoolean, psi);
		    Tcl_DecrRefCount(psi->obj);

		/* First configure. Don't free the saved data. */
		} else {
		    psi->data = NULL;
		    psi->count = 0;
		}
		psi->obj = objv[i + 1];
		Tcl_IncrRefCount(psi->obj);
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstBoolean,
			psi) != TCL_OK) {
		    goto badConfig;
		}
		break;
	    }
	}
    }
    if (saved.onion && (eLink->onion != saved.onion))
	WCFREE(saved.onion, int, saved.onionCount);
    if (saved.draw.obj != NULL &&
	    saved.draw.obj != eLink->draw.obj) {
	PerStateInfo_Free(tree, &pstBoolean, &saved.draw);
	Tcl_DecrRefCount(saved.draw.obj);
    }
    if (saved.visible.obj != NULL &&
	    saved.visible.obj != eLink->visible.obj) {
	PerStateInfo_Free(tree, &pstBoolean, &saved.visible);
	Tcl_DecrRefCount(saved.visible.obj);
    }
    Style_Changed(tree, style);
    return TCL_OK;

badConfig:
    if (eLink->onion && (eLink->onion != saved.onion))
	WCFREE(eLink->onion, int, eLink->onionCount);
    if (eLink->draw.obj != NULL &&
	    eLink->draw.obj != saved.draw.obj) {
	PerStateInfo_Free(tree, &pstBoolean, &eLink->draw);
	Tcl_DecrRefCount(eLink->draw.obj);
    }
    if (eLink->visible.obj != NULL &&
	    eLink->visible.obj != saved.visible.obj) {
	PerStateInfo_Free(tree, &pstBoolean, &eLink->visible);
	Tcl_DecrRefCount(eLink->visible.obj);
    }
    *eLink = saved;
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyleCmd --
 *
 *	This procedure is invoked to process the [style] widget
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
TreeStyleCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    static CONST char *commandNames[] = {
	"cget", "configure", "create", "delete", "elements", "layout",
	"names", (char *) NULL };
    enum {
	COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_CREATE, COMMAND_DELETE,
	COMMAND_ELEMENTS, COMMAND_LAYOUT, COMMAND_NAMES };
    int index;
    TreeStyle _style;
    MStyle *style;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandNames, "command", 0,
	&index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
	case COMMAND_CGET: {
	    Tcl_Obj *resultObjPtr;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "name option");
		return TCL_ERROR;
	    }
	    if (TreeStyle_FromObj(tree, objv[3], &_style) != TCL_OK)
		return TCL_ERROR;
	    style = (MStyle *) _style;
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) style,
		tree->styleOptionTable, objv[4], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	case COMMAND_CONFIGURE: {
	    Tcl_Obj *resultObjPtr = NULL;

	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "name ?option? ?value option value ...?");
		return TCL_ERROR;
	    }
	    if (TreeStyle_FromObj(tree, objv[3], &_style) != TCL_OK)
		return TCL_ERROR;
	    style = (MStyle *) _style;
	    if (objc <= 5) {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) style,
		    tree->styleOptionTable,
		    (objc == 4) ? (Tcl_Obj *) NULL : objv[4],
		    tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
	    } else {
		if (Tk_SetOptions(tree->interp, (char *) style,
		    tree->styleOptionTable, objc - 4, objv + 4, tree->tkwin,
		    NULL, NULL) != TCL_OK)
		    return TCL_ERROR;
		Style_Changed(tree, style);
	    }
	    break;
	}

	case COMMAND_CREATE: {
	    char *name;
	    int len;
	    Tcl_HashEntry *hPtr;
	    int isNew;

	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "name ?option value ...?");
		return TCL_ERROR;
	    }
	    name = Tcl_GetStringFromObj(objv[3], &len);
	    if (!len) {
		FormatResult(interp, "invalid style name \"\"");
		return TCL_ERROR;
	    }
	    hPtr = Tcl_FindHashEntry(&tree->styleHash, name);
	    if (hPtr != NULL) {
		FormatResult(interp, "style \"%s\" already exists", name);
		return TCL_ERROR;
	    }
	    style = Style_CreateAndConfig(tree, name, objc - 4, objv + 4);
	    if (style == NULL)
		return TCL_ERROR;
	    hPtr = Tcl_CreateHashEntry(&tree->styleHash, name, &isNew);
	    Tcl_SetHashValue(hPtr, style);
	    Tcl_SetObjResult(interp, TreeStyle_ToObj((TreeStyle) style));
	    break;
	}

	case COMMAND_DELETE: {
	    int i;

	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 3, objv, "?name ...?");
		return TCL_ERROR;
	    }
	    for (i = 3; i < objc; i++) {
		if (TreeStyle_FromObj(tree, objv[i], &_style) != TCL_OK)
		    return TCL_ERROR;
		Style_Deleted(tree, (MStyle *) _style);
		TreeStyle_FreeResources(tree, _style);
	    }
	    break;
	}

	/* T style elements S ?{E ...}? */
	case COMMAND_ELEMENTS: {
	    TreeElement elem, *elemList = NULL;
	    int i, j, count = 0;
	    int staticMap[STATIC_SIZE], *map = staticMap;
	    int listObjc;
	    Tcl_Obj **listObjv;

	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "name ?elementList?");
		return TCL_ERROR;
	    }
	    if (TreeStyle_FromObj(tree, objv[3], &_style) != TCL_OK)
		return TCL_ERROR;
	    style = (MStyle *) _style;
	    if (objc == 5) {
		if (Tcl_ListObjGetElements(interp, objv[4], &listObjc, &listObjv) != TCL_OK)
		    return TCL_ERROR;
		if (listObjc > 0)
		    elemList = (TreeElement *) ckalloc(sizeof(TreeElement_) * listObjc);
		for (i = 0; i < listObjc; i++) {
		    if (Element_FromObj(tree, listObjv[i], &elem) != TCL_OK) {
			ckfree((char *) elemList);
			return TCL_ERROR;
		    }

		    /* Ignore duplicate elements */
		    for (j = 0; j < count; j++) {
			if (elemList[j] == elem)
			    break;
		    }
		    if (j < count)
			continue;

		    elemList[count++] = elem;
		}

		STATIC_ALLOC(map, int, count);

		for (i = 0; i < count; i++)
		    map[i] = -1;

		/* Reassigning Elements to a Style */
		if (style->numElements > 0) {
		    /* Check each Element */
		    for (i = 0; i < count; i++) {
			/* See if this Element is already used by the Style */
			for (j = 0; j < style->numElements; j++) {
			    if (elemList[i] == style->elements[j].elem) {
				/* Preserve it */
				map[i] = j;
				break;
			    }
			}
		    }
		}
		Style_ChangeElements(tree, style, count, elemList, map);
		if (elemList != NULL)
		    ckfree((char *) elemList);
		STATIC_FREE(map, int, count);
		break;
	    }
	    TreeStyle_ListElements(tree, (TreeStyle) style);
	    break;
	}

	/* T style layout S E ?option? ?value? ?option value ...? */
	case COMMAND_LAYOUT: {
	    return StyleLayoutCmd(clientData, interp, objc, objv);
	}

	case COMMAND_NAMES: {
	    Tcl_Obj *listObj;
	    Tcl_HashSearch search;
	    Tcl_HashEntry *hPtr;

	    listObj = Tcl_NewListObj(0, NULL);
	    hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
	    while (hPtr != NULL) {
		_style = (TreeStyle) Tcl_GetHashValue(hPtr);
		Tcl_ListObjAppendElement(interp, listObj,
		    TreeStyle_ToObj(_style));
		hPtr = Tcl_NextHashEntry(&search);
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_ButtonMaxSize --
 *
 *	Return the maximum possible size of a button in any state. This
 *	includes the size of the -buttonimage and -buttonbitmap options,
 *	as well as the theme button and default +/- button.
 *
 * Results:
 *	Pixel size >= 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tree_ButtonMaxSize(
    TreeCtrl *tree,		/* Widget info. */
    int *maxWidth,		/* Returned maximum width. */
    int *maxHeight		/* Returned maximum height. */
    )
{
    int w, h, width = 0, height = 0;

    PerStateImage_MaxSize(tree, &tree->buttonImage, &w, &h);
    width = MAX(width, w);
    height = MAX(height, h);

    PerStateBitmap_MaxSize(tree, &tree->buttonBitmap, &w, &h);
    width = MAX(width, w);
    height = MAX(height, h);

    if (tree->useTheme) {
	if (TreeTheme_GetButtonSize(tree, Tk_WindowId(tree->tkwin),
		TRUE, &w, &h) == TCL_OK) {
	    width = MAX(width, w);
	    height = MAX(height, h);
	}
	if (TreeTheme_GetButtonSize(tree, Tk_WindowId(tree->tkwin),
		FALSE, &w, &h) == TCL_OK) {
	    width = MAX(width, w);
	    height = MAX(height, h);
	}
    }

    (*maxWidth) = MAX(width, tree->buttonSize);
    (*maxHeight) = MAX(height, tree->buttonSize);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_ButtonHeight --
 *
 *	Return the size of a button for a certain state.
 *
 * Results:
 *	Pixel size >= 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tree_ButtonHeight(
    TreeCtrl *tree,		/* Widget info. */
    int state			/* STATE_xxx flags. */
    )
{
    Tk_Image image;
    Pixmap bitmap;
    int w, h;

    image = PerStateImage_ForState(tree, &tree->buttonImage, state, NULL);
    if (image != NULL) {
        Tk_SizeOfImage(image, &w, &h);
        return h;
    }

    bitmap = PerStateBitmap_ForState(tree, &tree->buttonBitmap, state, NULL);
    if (bitmap != None) {
	Tk_SizeOfBitmap(tree->display, bitmap, &w, &h);
	return h;
    }

    if (tree->useTheme &&
	TreeTheme_GetButtonSize(tree, Tk_WindowId(tree->tkwin),
	    (state & STATE_OPEN) != 0, &w, &h) == TCL_OK)
	return h;

    return tree->buttonSize;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_Identify --
 *
 *	Perform hit-testing on a style.
 *
 * Results:
 *	The name of the element containing the given point, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TreeStyle_Identify(
    StyleDrawArgs *drawArgs,	/* Various args. */
    int x,			/* Window x-coord to hit-test against. */
    int y			/* Window y-coord to hit-test against. */
    )
{
    TreeCtrl *tree = drawArgs->tree;
    IStyle *style = (IStyle *) drawArgs->style;
    MStyle *masterStyle = style->master;
    int state = drawArgs->state;
    IElementLink *eLink = NULL;
    int i, minWidth, minHeight;
    struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;

    Style_CheckNeededSize(tree, style, state);
#ifdef CACHE_STYLE_SIZE
    minWidth = style->minWidth;
    minHeight = style->minHeight;
#else
    Style_MinSize(tree, style, state, &minWidth, &minHeight);
#endif

    if (drawArgs->width < minWidth + drawArgs->indent)
	drawArgs->width = minWidth + drawArgs->indent;
    if (drawArgs->height < minHeight)
	drawArgs->height = minHeight;

    x -= drawArgs->x;

    STATIC_ALLOC(layouts, struct Layout, masterStyle->numElements);

    Style_DoLayout(drawArgs, layouts, FALSE, __FILE__, __LINE__);

    for (i = style->master->numElements - 1; i >= 0; i--) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink = layout->eLink;
	if ((x >= layout->x + layout->ePadX[PAD_TOP_LEFT]) && (x < layout->x + layout->ePadX[PAD_TOP_LEFT] + layout->iWidth) &&
	    (y >= layout->y + layout->ePadY[PAD_TOP_LEFT]) && (y < layout->y + layout->ePadY[PAD_TOP_LEFT] + layout->iHeight)) {
	    goto done;
	}
    }
    eLink = NULL;
done:
    STATIC_FREE(layouts, struct Layout, masterStyle->numElements);
    if (eLink != NULL)
	return (char *) eLink->elem->name;
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_Identify2 --
 *
 *	Return a list of elements overlapping the given area.
 *
 * Results:
 *	The names of any elements overlapping the given area are
 *	appended to the supplied list.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

void
TreeStyle_Identify2(
    StyleDrawArgs *drawArgs,	/* Various args. */
    int x1, int y1,		/* Top-left of area to hit-test. */
    int x2, int y2,		/* Bottom-right of area to hit-test. */
    Tcl_Obj *listObj		/* Initialized list object to hold
				 * the result. */
    )
{
    TreeCtrl *tree = drawArgs->tree;
    IStyle *style = (IStyle *) drawArgs->style;
    MStyle *masterStyle = style->master;
    int state = drawArgs->state;
    IElementLink *eLink;
    int i, minWidth, minHeight;
    struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;

    Style_CheckNeededSize(tree, style, state);
#ifdef CACHE_STYLE_SIZE
    minWidth = style->minWidth;
    minHeight = style->minHeight;
#else
    Style_MinSize(tree, style, state, &minWidth, &minHeight);
#endif

    if (drawArgs->width < minWidth + drawArgs->indent)
	drawArgs->width = minWidth + drawArgs->indent;
    if (drawArgs->height < minHeight)
	drawArgs->height = minHeight;

    STATIC_ALLOC(layouts, struct Layout, masterStyle->numElements);

    Style_DoLayout(drawArgs, layouts, FALSE, __FILE__, __LINE__);

    for (i = style->master->numElements - 1; i >= 0; i--) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	eLink = layout->eLink;
	if ((drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT] < x2) &&
	    (drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT] + layout->iWidth > x1) &&
	    (drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT] < y2) &&
	    (drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT] + layout->iHeight > y1)) {
	    Tcl_ListObjAppendElement(drawArgs->tree->interp, listObj,
		Tcl_NewStringObj(eLink->elem->name, -1));
	}
    }

    STATIC_FREE(layouts, struct Layout, masterStyle->numElements);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_Remap --
 *
 *	The guts of the [item style map] command.  See the user
 *	documentation for details on what it does.
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
TreeStyle_Remap(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle styleFrom_,	/* Current instance style. */
    TreeStyle styleTo_,		/* Master style to "convert" the current
				 * style to. */
    int objc,			/* Must be even number. */
    Tcl_Obj *CONST objv[]	/* Array of old-new element names. */
    )
{
    IStyle *styleFrom = (IStyle *) styleFrom_;
    MStyle *styleTo = (MStyle *) styleTo_;
    int i, indexFrom, indexTo;
    int staticMap[STATIC_SIZE], *map = staticMap;
    IElementLink *eLink;
    TreeElement elemFrom, elemTo;
    TreeElement staticElemMap[STATIC_SIZE], *elemMap = staticElemMap;
    int styleFromNumElements = styleFrom->master->numElements;
    int result = TCL_OK;

    /* Must be instance */
    if ((styleFrom == NULL) || (styleFrom->master == NULL))
	return TCL_ERROR;

    /* Must be master */
    if ((styleTo == NULL) || (styleTo->master != NULL))
	return TCL_ERROR;

    /* Nothing to do */
    if (styleFrom->master == styleTo)
	return TCL_OK;

    if (objc & 1)
	return TCL_ERROR;

    STATIC_ALLOC(map, int, styleFromNumElements);
    STATIC_ALLOC(elemMap, TreeElement, styleFromNumElements);

    for (i = 0; i < styleFromNumElements; i++)
	map[i] = -1;

    for (i = 0; i < objc; i += 2) {
	/* Get the old-style element */
	if (Element_FromObj(tree, objv[i], &elemFrom) != TCL_OK) {
	    result = TCL_ERROR;
	    goto done;
	}

	/* Verify the old style uses the element */
	if (MStyle_FindElem(tree, styleFrom->master, elemFrom, &indexFrom) == NULL) {
	    FormatResult(tree->interp, "style %s does not use element %s",
		styleFrom->master->name, elemFrom->name);
	    result = TCL_ERROR;
	    goto done;
	}

	/* Get the new-style element */
	if (Element_FromObj(tree, objv[i + 1], &elemTo) != TCL_OK) {
	    result = TCL_ERROR;
	    goto done;
	}

	/* Verify the new style uses the element */
	if (MStyle_FindElem(tree, styleTo, elemTo, &indexTo) == NULL) {
	    FormatResult(tree->interp, "style %s does not use element %s",
		styleTo->name, elemTo->name);
	    result = TCL_ERROR;
	    goto done;
	}

	/* Must be the same type */
	if (elemFrom->typePtr != elemTo->typePtr) {
	    FormatResult(tree->interp, "can't map element type %s to %s",
		elemFrom->typePtr->name, elemTo->typePtr->name);
	    result = TCL_ERROR;
	    goto done;
	}

	/* See if the instance style has any info for this element */
	eLink = &styleFrom->elements[indexFrom];
	if (eLink->elem->master != NULL) {
	    map[indexFrom] = indexTo;
	    elemMap[indexFrom] = eLink->elem;
	}
    }

    for (i = 0; i < styleFromNumElements; i++) {
	eLink = &styleFrom->elements[i];
	indexTo = map[i];

	/* Free info for any Elements not being remapped */
	if ((indexTo == -1) && (eLink->elem->master != NULL)) {
	    elemFrom = eLink->elem->master;
	    Element_FreeResources(tree, eLink->elem);
	    eLink->elem = elemFrom;
	}

	/* Remap this Element */
	if (indexTo != -1) {
	    elemMap[i]->master = styleTo->elements[indexTo].elem;
	    elemMap[i]->name = styleTo->elements[indexTo].elem->name;
	}
    }

    if (styleFromNumElements != styleTo->numElements) {
#ifdef ALLOC_HAX
	if (styleFromNumElements > 0)
	    TreeAlloc_CFree(tree->allocData, IElementLinkUid,
		(char *) styleFrom->elements, sizeof(IElementLink),
		styleFromNumElements, ELEMENT_LINK_ROUND);
	styleFrom->elements = (IElementLink *) TreeAlloc_CAlloc(tree->allocData,
	    IElementLinkUid, sizeof(IElementLink), styleTo->numElements,
	    ELEMENT_LINK_ROUND);
#else
	if (styleFromNumElements > 0)
	    WCFREE(styleFrom->elements, IElementLink, styleFromNumElements);
	styleFrom->elements = (IElementLink *) ckalloc(sizeof(IElementLink) *
	    styleTo->numElements);
#endif
	memset(styleFrom->elements, '\0', sizeof(IElementLink) * styleTo->numElements);
    }
    for (i = 0; i < styleTo->numElements; i++) {
	styleFrom->elements[i].elem = styleTo->elements[i].elem;
#ifdef CACHE_ELEM_SIZE
	styleFrom->elements[i].neededWidth = -1;
	styleFrom->elements[i].neededHeight = -1;
#endif
    }
    for (i = 0; i < styleFromNumElements; i++) {
	indexTo = map[i];
	if (indexTo != -1)
	    styleFrom->elements[indexTo].elem = elemMap[i];
    }
    styleFrom->master = styleTo;
    styleFrom->neededWidth = styleFrom->neededHeight = -1;

done:
    STATIC_FREE(map, int, styleFromNumElements);
    STATIC_FREE(elemMap, TreeElement, styleFromNumElements);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_GetSortData --
 *
 *	Called by the [item sort] code. Returns a long, double or
 *	string value from a text element.
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
TreeStyle_GetSortData(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_,		/* The style. */
    int elemIndex,		/* Index of a text element, or -1 to use
				 * the first text element. */
    int type,			/* SORT_xxx constant. */
    long *lv,			/* Returned for SORT_LONG. */
    double *dv,			/* Returned for SORT_DOUBLE. */
    char **sv			/* Returned for SORT_ASCII or SORT_DICT. */
    )
{
    IStyle *style = (IStyle *) style_;
    IElementLink *eLink = style->elements;
    int i;

    if (elemIndex == -1) {
	for (i = 0; i < style->master->numElements; i++) {
	    if (ELEMENT_TYPE_MATCHES(eLink->elem->typePtr, &treeElemTypeText))
		return TreeElement_GetSortData(tree, eLink->elem, type, lv, dv, sv);
	    eLink++;
	}
    } else {
	if ((elemIndex < 0) || (elemIndex >= style->master->numElements))
	    panic("bad elemIndex %d to TreeStyle_GetSortData", elemIndex);
	eLink = &style->elements[elemIndex];
	if (ELEMENT_TYPE_MATCHES(eLink->elem->typePtr, &treeElemTypeText))
	    return TreeElement_GetSortData(tree, eLink->elem, type, lv, dv, sv);
    }

    FormatResult(tree->interp, "can't find text element in style %s",
	style->master->name);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_GetElemRects --
 *
 *	Return a list of rectangles for specified elements in a style.
 *
 * Results:
 *	The number of rects[] written.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeStyle_GetElemRects(
    StyleDrawArgs *drawArgs,	/* Various args. */
    int objc,			/* Number of element names. */
    Tcl_Obj *CONST objv[],	/* Array of element names. */
    TreeRectangle rects[]	/* Returned rectangles. */
    )
{
    IStyle *style = (IStyle *) drawArgs->style;
    MStyle *master = style->master;
    int i, j, count = 0, minWidth, minHeight;
    struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
    TreeElement staticElems[STATIC_SIZE], *elems = staticElems;
    MElementLink *eLink;

    STATIC_ALLOC(elems, TreeElement, objc);

    for (j = 0; j < objc; j++) {
	if (Element_FromObj(drawArgs->tree, objv[j], &elems[j]) != TCL_OK) {
	    count = -1;
	    goto done;
	}

	eLink = MStyle_FindElem(drawArgs->tree, master, elems[j], NULL);
	if (eLink == NULL) {
	    FormatResult(drawArgs->tree->interp,
		"style %s does not use element %s",
		master->name, elems[j]->name);
	    count = -1;
	    goto done;
	}
    }

    Style_CheckNeededSize(drawArgs->tree, style, drawArgs->state);
#ifdef CACHE_STYLE_SIZE
    minWidth = style->minWidth;
    minHeight = style->minHeight;
#else
    Style_MinSize(drawArgs->tree, style, drawArgs->state, &minWidth, &minHeight);
#endif

    if (drawArgs->width < minWidth + drawArgs->indent)
	drawArgs->width = minWidth + drawArgs->indent;
    if (drawArgs->height < minHeight)
	drawArgs->height = minHeight;

    STATIC_ALLOC(layouts, struct Layout, master->numElements);

    Style_DoLayout(drawArgs, layouts, FALSE, __FILE__, __LINE__);

    for (i = master->numElements - 1; i >= 0; i--) {
	struct Layout *layout = &layouts[i];

	if (IS_HIDDEN(layout))
	    continue;

	if (objc > 0) {
	    for (j = 0; j < objc; j++)
		if (elems[j] == layout->eLink->elem ||
		    elems[j] == layout->master->elem)
		    break;
	    if (j == objc)
		continue;
	}
	rects[count].x = drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT];
	rects[count].y = drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT];
	if (layout->master->onion == NULL) {
	    rects[count].x += layout->iPadX[PAD_TOP_LEFT];
	    rects[count].y += layout->iPadY[PAD_TOP_LEFT];
	    rects[count].width = layout->useWidth;
	    rects[count].height = layout->useHeight;
	} else {
	    rects[count].width = layout->iWidth;
	    rects[count].height = layout->iHeight;
	}
	count++;
    }

    STATIC_FREE(layouts, struct Layout, master->numElements);

done:
    STATIC_FREE(elems, TreeElement, objc);
    return count;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_ChangeState --
 *
 *	Called when the state of an item or item-column changes.
 *
 * Results:
 *	A bitmask of CS_DISPLAY and CS_LAYOUT values.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeStyle_ChangeState(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_,		/* The instance style. */
    int state1,			/* The previous state. */
    int state2			/* The current state. */
    )
{
    IStyle *style = (IStyle *) style_;
    MStyle *masterStyle = style->master;
    MElementLink *eLink1;
    IElementLink *eLink2;
    TreeElementArgs args;
    int i, eMask, mask = 0;
    int undisplay;

    if (state1 == state2)
	return 0;

    args.tree = tree;

    for (i = 0; i < masterStyle->numElements; i++) {
	eLink2 = &style->elements[i];
	args.elem = eLink2->elem;
	args.states.state1 = state1;
	args.states.state2 = state2;
	args.states.draw1 = args.states.draw2 = TRUE;
	args.states.visible1 = args.states.visible2 = TRUE;

	eLink1 = &masterStyle->elements[i];
	undisplay = FALSE;
	eMask = 0;

	/* Check for the style layout option -draw changing. */
	if (eLink1->draw.count > 0) {
	    args.states.draw1 = PerStateBoolean_ForState(tree,
		    &eLink1->draw, state1, NULL) != 0;
	    args.states.draw2 = PerStateBoolean_ForState(tree,
		    &eLink1->draw, state2, NULL) != 0;
	    if (args.states.draw1 != args.states.draw2) {
		eMask |= CS_DISPLAY;
		if (!args.states.draw2)
		    undisplay = TRUE;
	    }
	}

	/* Check for the style layout option -visible changing. */
	if (eLink1->visible.count > 0) {
	    args.states.visible1 = PerStateBoolean_ForState(tree,
		    &eLink1->visible, state1, NULL) != 0;
	    args.states.visible2 = PerStateBoolean_ForState(tree,
		    &eLink1->visible, state2, NULL) != 0;
	    /* FIXME: Changing visibility might not change the layout. */
	    if (args.states.visible1 != args.states.visible2) {
		eMask |= CS_DISPLAY | CS_LAYOUT;
		if (!args.states.visible2)
		    undisplay = TRUE;
	    }
	}

	/* Tell the element about the state change. */
	eMask |= (*args.elem->typePtr->stateProc)(&args);

	/* Hack: If a window element becomes hidden, then tell it it is
	 * not onscreen, otherwise it will never be "drawn" in the
	 * hidden state. */
	if (undisplay && ELEMENT_TYPE_MATCHES(args.elem->typePtr,
		&treeElemTypeWindow)) {
	    args.screen.visible = FALSE;
	    (*args.elem->typePtr->onScreenProc)(&args);
	}

	if (eMask) {
#ifdef CACHE_ELEM_SIZE
	    if (eMask & CS_LAYOUT)
		eLink2->neededWidth = eLink2->neededHeight = -1;
#endif
	    mask |= eMask;
	}
    }

    if (mask & CS_LAYOUT)
	style->neededWidth = style->neededHeight = -1;

#ifdef TREECTRL_DEBUG
    if (style->neededWidth != -1)
	style->neededState = state2;
#endif

    return mask;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_UndefineState --
 *
 *	The guts of the [state undefine] widget command.
 *
 * Results:
 *	The undefProc of every element is called to respond to the
 *	undefined state flag. The size of every element/column/item is
 *	marked out-of-date regardless of whether the state change
 *	affected the element.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

void
Tree_UndefineState(
    TreeCtrl *tree,		/* Widget info. */
    int state			/* STATE_xxx flag. */
    )
{
    TreeItem item;
    TreeItemColumn column;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    IElementLink *eLink;
    int i, columnIndex;
    TreeElementArgs args;

    /* Undefine the state for the -draw and -visible style layout
     * options for each element of this style. */
    hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
    while (hPtr != NULL) {
	MStyle *masterStyle = (MStyle *) Tcl_GetHashValue(hPtr);
	for (i = 0; i < masterStyle->numElements; i++) {
	    MElementLink *eLink1 = &masterStyle->elements[i];
	    PerStateInfo_Undefine(tree, &pstBoolean, &eLink1->draw, state);
	    PerStateInfo_Undefine(tree, &pstBoolean, &eLink1->visible, state);
	}
	hPtr = Tcl_NextHashEntry(&search);
    }

    args.tree = tree;
    args.state = state;

    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
    while (hPtr != NULL) {
	item = (TreeItem) Tcl_GetHashValue(hPtr);
	column = TreeItem_GetFirstColumn(tree, item);
	columnIndex = 0;
	while (column != NULL) {
	    IStyle *style = (IStyle *) TreeItemColumn_GetStyle(tree, column);
	    if (style != NULL) {
		for (i = 0; i < style->master->numElements; i++) {
		    eLink = &style->elements[i];
		    /* Instance element */
		    if (eLink->elem->master != NULL) {
			args.elem = eLink->elem;
			(*args.elem->typePtr->undefProc)(&args);
		    }
#ifdef CACHE_ELEM_SIZE
		    eLink->neededWidth = eLink->neededHeight = -1;
#endif
		}
		style->neededWidth = style->neededHeight = -1;
		TreeItemColumn_InvalidateSize(tree, column);
	    }
	    columnIndex++;
	    column = TreeItemColumn_GetNext(tree, column);
	}
	TreeItem_InvalidateHeight(tree, item);
	Tree_FreeItemDInfo(tree, item, NULL);
	TreeItem_UndefineState(tree, item, state);
	hPtr = Tcl_NextHashEntry(&search);
    }
    Tree_InvalidateColumnWidth(tree, NULL);
    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

    hPtr = Tcl_FirstHashEntry(&tree->elementHash, &search);
    while (hPtr != NULL) {
	args.elem = (TreeElement) Tcl_GetHashValue(hPtr);
	(*args.elem->typePtr->undefProc)(&args);
	hPtr = Tcl_NextHashEntry(&search);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_NumElements --
 *
 *	Return the number of elements in a style.
 *
 * Results:
 *	The number of... oh nevermind.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeStyle_NumElements(
    TreeCtrl *tree,		/* Widget info. */
    TreeStyle style_		/* The style. */
    )
{
    MStyle *masterStyle = ((MStyle *) style_);
    IStyle *style = ((IStyle *) style_);
    return (style->master == NULL) ?
	masterStyle->numElements :
	style->master->numElements;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_Init --
 *
 *	Style-related package initialization.
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
TreeStyle_Init(
    TreeCtrl *tree		/* Widget info. */
    )
{
    tree->styleOptionTable = Tk_CreateOptionTable(tree->interp,
	styleOptionSpecs); 
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeStyle_Free --
 *
 *	Free style-related resources for a deleted TreeCtrl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

void
TreeStyle_Free(
    TreeCtrl *tree		/* Widget info. */
    )
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    TreeElement elem;
    TreeStyle style;

    while (1) {
	hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
	if (hPtr == NULL)
	    break;
	style = (TreeStyle) Tcl_GetHashValue(hPtr);
	TreeStyle_FreeResources(tree, style);
    }

    while (1) {
	hPtr = Tcl_FirstHashEntry(&tree->elementHash, &search);
	if (hPtr == NULL)
	    break;
	elem = (TreeElement) Tcl_GetHashValue(hPtr);
	Element_FreeResources(tree, elem);
    }

    Tcl_DeleteHashTable(&tree->elementHash);
    Tcl_DeleteHashTable(&tree->styleHash);
}

