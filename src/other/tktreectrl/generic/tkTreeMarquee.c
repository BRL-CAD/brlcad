/* 
 * tkTreeMarquee.c --
 *
 *	This module implements the selection rectangle for treectrl widgets.
 *
 * Copyright (c) 2002-2010 Tim Baker
 *
 * RCS: @(#) $Id$
 */

#include "tkTreeCtrl.h"

typedef struct TreeMarquee_ TreeMarquee_;

/*
 * The following structure holds info about the selection rectangle.
 * There is one of these per TreeCtrl.
 */
struct TreeMarquee_
{
    TreeCtrl *tree;
    Tk_OptionTable optionTable;
    int visible;			/* -visible option. */
    int x1, y1, x2, y2;			/* Opposing corners. */
    int onScreen;			/* TRUE if it was drawn. */
    int sx, sy;				/* Offset of canvas from top-left
					 * corner of the window when we
					 * were drawn. */
    int sw, sh;				/* Width & height when drawn. */

    TreeColor *fillColorPtr;		/* -fill */
    Tcl_Obj *fillObj;			/* -fill */
    TreeColor *outlineColorPtr;		/* -outline */
    Tcl_Obj *outlineObj;		/* -outline */
    int outlineWidth;			/* -outlinewidth */
    Tcl_Obj *outlineWidthObj;		/* -outlinewidth */
};

#define MARQ_CONF_VISIBLE		0x0001
#define MARQ_CONF_COLORS		0x0002

static Tk_OptionSpec optionSpecs[] = {
    {TK_OPTION_CUSTOM, "-fill", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TreeMarquee_, fillObj),
	Tk_Offset(TreeMarquee_, fillColorPtr), TK_OPTION_NULL_OK,
	(ClientData) &TreeCtrlCO_treecolor, MARQ_CONF_COLORS},
    {TK_OPTION_CUSTOM, "-outline", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TreeMarquee_, outlineObj),
	Tk_Offset(TreeMarquee_, outlineColorPtr), TK_OPTION_NULL_OK,
	(ClientData) &TreeCtrlCO_treecolor, MARQ_CONF_COLORS},
    {TK_OPTION_PIXELS, "-outlinewidth", (char *) NULL, (char *) NULL,
	"1", Tk_Offset(TreeMarquee_, outlineWidthObj),
	Tk_Offset(TreeMarquee_, outlineWidth), 0,
	(ClientData) NULL, MARQ_CONF_COLORS},
    {TK_OPTION_BOOLEAN, "-visible", (char *) NULL, (char *) NULL,
	"0", -1, Tk_Offset(TreeMarquee_, visible),
	0, (ClientData) NULL, MARQ_CONF_VISIBLE},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, -1, 0, 0, 0}
};

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_Init --
 *
 *	Perform marquee-related initialization when a new TreeCtrl is
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

int
TreeMarquee_Init(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeMarquee marquee;

    marquee = (TreeMarquee) ckalloc(sizeof(TreeMarquee_));
    memset(marquee, '\0', sizeof(TreeMarquee_));
    marquee->tree = tree;
    marquee->optionTable = Tk_CreateOptionTable(tree->interp, optionSpecs);
    if (Tk_InitOptions(tree->interp, (char *) marquee, marquee->optionTable,
	tree->tkwin) != TCL_OK) {
	WFREE(marquee, TreeMarquee_);
	return TCL_ERROR;
    }
    tree->marquee = marquee;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_Free --
 *
 *	Free marquee-related resources when a TreeCtrl is deleted.
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
TreeMarquee_Free(
    TreeMarquee marquee	/* Marquee token. */
    )
{
    Tk_FreeConfigOptions((char *) marquee, marquee->optionTable,
	marquee->tree->tkwin);
    WFREE(marquee, TreeMarquee_);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_IsXOR --
 *
 *	Return true if the marquee is being drawn with XOR.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int TreeMarquee_IsXOR(TreeMarquee marquee)
{
    if (marquee->fillColorPtr || marquee->outlineColorPtr)
	return FALSE;

#if defined(WIN32)
    return FALSE; /* TRUE on XP, FALSE on Win7 (lots of flickering) */
#elif defined(MAC_OSX_TK)
    return FALSE;
#else
    return TRUE; /* X11 */
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_IsVisible --
 *
 *	Return true if the marquee is being drawn.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int TreeMarquee_IsVisible(TreeMarquee marquee)
{
    return marquee->visible;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_Display --
 *
 *	Draw the selection rectangle if it is not already displayed and if
 *	it's -visible option is TRUE.
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
TreeMarquee_Display(
    TreeMarquee marquee		/* Marquee token. */
    )
{
    TreeCtrl *tree = marquee->tree;

    if (!marquee->onScreen && marquee->visible) {
	if (TreeMarquee_IsXOR(marquee)) {
	    marquee->sx = 0 - tree->xOrigin;
	    marquee->sy = 0 - tree->yOrigin;
	    TreeMarquee_DrawXOR(marquee, Tk_WindowId(tree->tkwin),
		marquee->sx, marquee->sy);
	} else {
	    marquee->sx = MIN(marquee->x1, marquee->x2) - tree->xOrigin;
	    marquee->sy = MIN(marquee->y1, marquee->y2) - tree->yOrigin;
	    marquee->sw = abs(marquee->x2 - marquee->x1) + 1;
	    marquee->sh = abs(marquee->y2 - marquee->y1) + 1;
/*	    Tree_InvalidateItemArea(tree, marquee->sx, marquee->sy,
		marquee->sx + marquee->sw, marquee->sy + marquee->sh);*/
	    Tree_EventuallyRedraw(tree);
	}
	marquee->onScreen = TRUE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_Undisplay --
 *
 *	Erase the selection rectangle if it is displayed.
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
TreeMarquee_Undisplay(
    TreeMarquee marquee		/* Marquee token. */
    )
{
    TreeCtrl *tree = marquee->tree;

    if (marquee->onScreen) {
	if (TreeMarquee_IsXOR(marquee)) {
	    TreeMarquee_DrawXOR(marquee, Tk_WindowId(tree->tkwin), marquee->sx, marquee->sy);
	} else {
/*	    Tree_InvalidateItemArea(tree, marquee->sx, marquee->sy,
		marquee->sx + marquee->sw, marquee->sy + marquee->sh);*/
	    Tree_EventuallyRedraw(tree);
	}
	marquee->onScreen = FALSE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_DrawXOR --
 *
 *	Draw (or erase) the selection rectangle.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn (or erased, since this is XOR drawing).
 *
 *----------------------------------------------------------------------
 */

void
TreeMarquee_DrawXOR(
    TreeMarquee marquee,	/* Marquee token. */
    Drawable drawable,		/* Where to draw. */
    int x1, int y1		/* Offset of canvas from top-left corner
				 * of the window. */
    )
{
    TreeCtrl *tree = marquee->tree;
    int x, y, w, h;
    DotState dotState;

    x = MIN(marquee->x1, marquee->x2);
    w = abs(marquee->x1 - marquee->x2) + 1;
    y = MIN(marquee->y1, marquee->y2);
    h = abs(marquee->y1 - marquee->y2) + 1;

    TreeDotRect_Setup(tree, drawable, &dotState);
    TreeDotRect_Draw(&dotState, x1 + x, y1 + y, w, h);
    TreeDotRect_Restore(&dotState);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_Draw --
 *
 *	Draw the selection rectangle if it is visible.
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
TreeMarquee_Draw(
    TreeMarquee marquee,	/* Marquee token. */
    TreeDrawable td)		/* Where to draw. */
{
#if 1 /* Use XOR dotted rectangles where possible. */
    TreeCtrl *tree = marquee->tree;

    if (!marquee->visible)
	return;

    if (marquee->fillColorPtr || marquee->outlineColorPtr) {
	TreeRectangle tr;
	TreeClip clip;

	tr.x = 0 - tree->xOrigin + MIN(marquee->x1, marquee->x2);
	tr.width = abs(marquee->x1 - marquee->x2) + 1;
	tr.y = 0 - tree->yOrigin + MIN(marquee->y1, marquee->y2);
	tr.height = abs(marquee->y1 - marquee->y2) + 1;

	clip.type = TREE_CLIP_AREA, clip.area = TREE_AREA_CONTENT;

	if (marquee->fillColorPtr) {
	    TreeRectangle trBrush;
	    TreeColor_GetBrushBounds(tree, marquee->fillColorPtr, tr,
		    tree->xOrigin, tree->yOrigin,
		    (TreeColumn) NULL, (TreeItem) NULL, &trBrush);
	    TreeColor_FillRect(tree, td, &clip, marquee->fillColorPtr, trBrush, tr);
	}
	if (marquee->outlineColorPtr && marquee->outlineWidth > 0) {
	    TreeRectangle trBrush;
	    TreeColor_GetBrushBounds(tree, marquee->outlineColorPtr, tr,
		    tree->xOrigin, tree->yOrigin,
		    (TreeColumn) NULL, (TreeItem) NULL, &trBrush);
	    TreeColor_DrawRect(tree, td, &clip, marquee->outlineColorPtr,
		trBrush, tr, marquee->outlineWidth, 0);
	}
	return;
    }

    /* Yes this is XOR drawing but we aren't erasing the previous
     * marquee as when TreeMarquee_IsXOR() returns TRUE. */
    TreeMarquee_DrawXOR(marquee, td.drawable,
	0 - tree->xOrigin, 0 - tree->yOrigin);
#else /* */
    TreeCtrl *tree = marquee->tree;
    int x, y, w, h;
    GC gc;
    XGCValues gcValues;
    unsigned long mask;
#ifdef WIN32
    XPoint points[5];
    XRectangle rect;
#endif
#if 0
    XColor *colorPtr;
#endif

    if (!marquee->visible)
	return;

    x = MIN(marquee->x1, marquee->x2);
    w = abs(marquee->x1 - marquee->x2) + 1;
    y = MIN(marquee->y1, marquee->y2);
    h = abs(marquee->y1 - marquee->y2) + 1;

#if 0
    colorPtr = Tk_GetColor(tree->interp, tree->tkwin, "gray50");
    gc = Tk_GCForColor(colorPtr, Tk_WindowId(tree->tkwin));

    XFillRectangle(tree->display, td.drawable, gc,
	x - tree->drawableXOrigin, y - tree->drawableYOrigin,
	w - 1, h - 1);
#else /* Stippled rectangles: BUG not clipped to contentbox. */
    gcValues.stipple = Tk_GetBitmap(tree->interp, tree->tkwin, "gray50");
    gcValues.fill_style = FillStippled;
    mask = GCStipple|GCFillStyle;
    gc = Tk_GetGC(tree->tkwin, mask, &gcValues);

#ifdef WIN32
    /* XDrawRectangle ignores the stipple pattern. */
    rect.x = x - tree->drawableXOrigin;
    rect.y = y - tree->drawableYOrigin;
    rect.width = w;
    rect.height = h;
    points[0].x = rect.x, points[0].y = rect.y;
    points[1].x = rect.x + rect.width - 1, points[1].y = rect.y;
    points[2].x = rect.x + rect.width - 1, points[2].y = rect.y + rect.height - 1;
    points[3].x = rect.x, points[3].y = rect.y + rect.height - 1;
    points[4] = points[0];
    XDrawLines(tree->display, td.drawable, gc, points, 5, CoordModeOrigin);
#else
    XDrawRectangle(tree->display, td.drawable, gc,
	x - tree->drawableXOrigin, y - tree->drawableYOrigin,
	w - 1, h - 1);
#endif
    Tk_FreeGC(tree->display, gc);
#endif
#endif /* */
}

/*
 *----------------------------------------------------------------------
 *
 * Marquee_Config --
 *
 *	This procedure is called to process an objc/objv list to set
 *	configuration options for a Marquee.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then an error message is left in interp's result.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for marquee;  old resources get freed, if there
 *	were any.  Display changes may occur.
 *
 *----------------------------------------------------------------------
 */

static int
Marquee_Config(
    TreeMarquee marquee,	/* Marquee record. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = marquee->tree;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;
    int mask;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) marquee, marquee->optionTable,
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

    if (mask & MARQ_CONF_VISIBLE) {
	TreeMarquee_Undisplay(marquee);
	TreeMarquee_Display(marquee);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarqueeCmd --
 *
 *	This procedure is invoked to process the [marquee] widget
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
TreeMarqueeCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    TreeMarquee marquee = tree->marquee;
    static CONST char *commandNames[] = { "anchor", "cget", "configure",
	"coords", "corner", "identify", (char *) NULL };
    enum { COMMAND_ANCHOR, COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_COORDS,
	COMMAND_CORNER, COMMAND_IDENTIFY };
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
	/* T marquee anchor ?x y?*/
	case COMMAND_ANCHOR: {
	    int x, y;

	    if (objc != 3 && objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "?x y?");
		return TCL_ERROR;
	    }
	    if (objc == 3) {
		FormatResult(interp, "%d %d", marquee->x1, marquee->y1);
		break;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)
		return TCL_ERROR;
	    if ((x == marquee->x1) && (y == marquee->y1))
		break;
	    TreeMarquee_Undisplay(tree->marquee);
	    marquee->x1 = x;
	    marquee->y1 = y;
	    TreeMarquee_Display(tree->marquee);
	    break;
	}

	/* T marquee cget option */
	case COMMAND_CGET: {
	    Tcl_Obj *resultObjPtr;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "option");
		return TCL_ERROR;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) marquee,
		marquee->optionTable, objv[3], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T marquee configure ?option? ?value? ?option value ...? */
	case COMMAND_CONFIGURE: {
	    Tcl_Obj *resultObjPtr;

	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 3, objv, "?option? ?value?");
		return TCL_ERROR;
	    }
	    if (objc <= 4) {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) marquee,
		    marquee->optionTable,
		    (objc == 3) ? (Tcl_Obj *) NULL : objv[3],
		    tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    return Marquee_Config(marquee, objc - 3, objv + 3);
	}

	/* T marquee coords ?x y x y? */
	case COMMAND_COORDS: {
	    int x1, y1, x2, y2;

	    if (objc != 3 && objc != 7) {
		Tcl_WrongNumArgs(interp, 3, objv, "?x y x y?");
		return TCL_ERROR;
	    }
	    if (objc == 3) {
		FormatResult(interp, "%d %d %d %d", marquee->x1, marquee->y1,
		    marquee->x2, marquee->y2);
		break;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[3], &x1) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[4], &y1) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[5], &x2) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[6], &y2) != TCL_OK)
		return TCL_ERROR;
	    if (x1 == marquee->x1 && y1 == marquee->y1 &&
		x2 == marquee->x2 && y2 == marquee->y2)
		break;
	    TreeMarquee_Undisplay(tree->marquee);
	    marquee->x1 = x1;
	    marquee->y1 = y1;
	    marquee->x2 = x2;
	    marquee->y2 = y2;
	    TreeMarquee_Display(tree->marquee);
	    break;
	}

	/* T marquee corner ?x y?*/
	case COMMAND_CORNER: {
	    int x, y;

	    if (objc != 3 && objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "?x y?");
		return TCL_ERROR;
	    }
	    if (objc == 3) {
		FormatResult(interp, "%d %d", marquee->x2, marquee->y2);
		break;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)
		return TCL_ERROR;
	    if (x == marquee->x2 && y == marquee->y2)
		break;
	    TreeMarquee_Undisplay(tree->marquee);
	    marquee->x2 = x;
	    marquee->y2 = y;
	    TreeMarquee_Display(tree->marquee);
	    break;
	}

	/* T marquee identify */
	case COMMAND_IDENTIFY: {
	    int x1, y1, x2, y2, n = 0;
	    int totalWidth = Tree_CanvasWidth(tree);
	    int totalHeight = Tree_CanvasHeight(tree);
	    TreeItemList items;
	    Tcl_Obj *listObj;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
		return TCL_ERROR;
	    }

	    x1 = MIN(marquee->x1, marquee->x2);
	    x2 = MAX(marquee->x1, marquee->x2);
	    y1 = MIN(marquee->y1, marquee->y2);
	    y2 = MAX(marquee->y1, marquee->y2);

	    if (x2 <= 0)
		break;
	    if (x1 >= totalWidth)
		break;

	    if (y2 <= 0)
		break;
	    if (y1 >= totalHeight)
		break;

	    if (x1 < 0)
		x1 = 0;
	    if (x2 > totalWidth)
		x2 = totalWidth;

	    if (y1 < 0)
		y1 = 0;
	    if (y2 > totalHeight)
		y2 = totalHeight;

	    Tree_ItemsInArea(tree, &items, x1, y1, x2, y2);
	    if (TreeItemList_Count(&items) == 0) {
		TreeItemList_Free(&items);
		break;
	    }

	    listObj = Tcl_NewListObj(0, NULL);
	    for (n = 0; n < TreeItemList_Count(&items); n++) {
		Tcl_Obj *subListObj = Tcl_NewListObj(0, NULL);
		TreeItem item = TreeItemList_Nth(&items, n);
		Tcl_ListObjAppendElement(interp, subListObj,
		    TreeItem_ToObj(tree, item));
		TreeItem_Identify2(tree, item, x1, y1, x2, y2, subListObj);
		Tcl_ListObjAppendElement(interp, listObj, subListObj);
	    }
	    TreeItemList_Free(&items);
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}
    }

    return TCL_OK;
}
