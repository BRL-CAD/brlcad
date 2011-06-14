/* 
 * tkTreeTheme.c --
 *
 *	This module implements visual themes.
 *
 * Copyright (c) 2010 Tim Baker
 *
 * RCS: @(#) $Id$
 */

#ifdef USE_TTK

#include "tkTreeCtrl.h"
#include "ttk/ttkTheme.h"
#include "ttk/ttk-extra.h"

typedef struct TreeThemeData_
{
    Ttk_Layout layout;
    Ttk_Layout buttonLayout;
    Ttk_Layout headingLayout;
    Tk_OptionTable buttonOptionTable;
    Tk_OptionTable headingOptionTable;
    Ttk_Box clientBox;
    int buttonWidth[2], buttonHeight[2];
    Ttk_Padding buttonPadding[2];
} TreeThemeData_;

int TreeTheme_DrawHeaderItem(TreeCtrl *tree, Drawable drawable, int state,
    int arrow, int visIndex, int x, int y, int width, int height)
{
    TreeThemeData themeData = tree->themeData;
    Ttk_Layout layout = themeData->headingLayout;
    Ttk_State ttk_state = 0;
    Ttk_Box box;

    if (layout == NULL)
	return TCL_ERROR;

    box = Ttk_MakeBox(x, y, width, height);

    switch (state) {
	case COLUMN_STATE_ACTIVE:  ttk_state = TTK_STATE_ACTIVE; break;
	case COLUMN_STATE_PRESSED: ttk_state = TTK_STATE_PRESSED; break;
    }

    eTtk_RebindSublayout(layout, NULL); /* !!! rebind to column */
    eTtk_PlaceLayout(layout, ttk_state, box);
    eTtk_DrawLayout(layout, ttk_state, drawable);

    return TCL_OK;
}

int TreeTheme_GetHeaderContentMargins(TreeCtrl *tree, int state, int arrow, int bounds[4])
{
    return TCL_ERROR;
}

int TreeTheme_DrawHeaderArrow(TreeCtrl *tree, Drawable drawable, int state, int up, int x, int y, int width, int height)
{
    return TCL_ERROR;
}

/* From ttkTreeview.c */
#define TTK_STATE_OPEN TTK_STATE_USER1

int TreeTheme_DrawButton(TreeCtrl *tree, Drawable drawable, int open, int x, int y, int width, int height)
{
    TreeThemeData themeData = tree->themeData;
    Ttk_Layout layout = themeData->buttonLayout;
    Ttk_State ttk_state = 0;
    Ttk_Box box;
    Ttk_Padding padding;

    if (layout == NULL)
	return TCL_ERROR;

    open = open ? 1 : 0;
    padding = themeData->buttonPadding[open];
    x -= padding.left;
    y -= padding.top;
    width = themeData->buttonWidth[open];
    height = themeData->buttonHeight[open];
    box = Ttk_MakeBox(x, y, width, height);

    ttk_state = open ? TTK_STATE_OPEN : 0;

    eTtk_RebindSublayout(layout, NULL); /* !!! rebind to item */
    eTtk_PlaceLayout(layout, ttk_state, box);
    eTtk_DrawLayout(layout, ttk_state, drawable);

    return TCL_OK;
}

int TreeTheme_GetButtonSize(TreeCtrl *tree, Drawable drawable, int open, int *widthPtr, int *heightPtr)
{
    TreeThemeData themeData = tree->themeData;
    Ttk_Padding padding;

    if (themeData->buttonLayout == NULL)
	return TCL_ERROR;

    open = open ? 1 : 0;
    padding = themeData->buttonPadding[open];
    *widthPtr = themeData->buttonWidth[open] - padding.left - padding.right;
    *heightPtr = themeData->buttonHeight[open] - padding.top - padding.bottom;
    return TCL_OK;
}

int TreeTheme_GetArrowSize(TreeCtrl *tree, Drawable drawable, int up, int *widthPtr, int *heightPtr)
{
    return TCL_ERROR;
}

int TreeTheme_SetBorders(TreeCtrl *tree)
{
    TreeThemeData themeData = tree->themeData;
    Tk_Window tkwin = tree->tkwin;
    Ttk_Box clientBox = themeData->clientBox;

    tree->inset.left = clientBox.x;
    tree->inset.top = clientBox.y;
    tree->inset.right = Tk_Width(tkwin) - (clientBox.x + clientBox.width);
    tree->inset.bottom = Tk_Height(tkwin) - (clientBox.y + clientBox.height);

    return TCL_OK;
}

/*
 * This routine is a big hack so that the "field" element (of the TreeCtrl
 * layout) doesn't erase the entire background of the window. This routine
 * draws each edge of the layout into a pixmap and copies the pixmap to the
 * window.
 */
int
TreeTheme_DrawBorders(
    TreeCtrl *tree,
    Drawable drawable
    )
{
    TreeThemeData themeData = tree->themeData;
    Tk_Window tkwin = tree->tkwin;
    Ttk_Box winBox = Ttk_WinBox(tree->tkwin);
    Ttk_State state = 0; /* ??? */
    int left, top, right, bottom;
    Drawable pixmapLR = None, pixmapTB = None;

    left = tree->inset.left;
    top = tree->inset.top;
    right = tree->inset.right;
    bottom = tree->inset.bottom;

    /* If the Ttk layout doesn't specify any borders or padding, then
     * draw nothing. */
    if (left < 1 && top < 1 && right < 1 && bottom < 1)
	return TCL_OK;

    if (left > 0 || top > 0)
	eTtk_PlaceLayout(themeData->layout, state, winBox);

    if (left > 0 || right > 0) {
	pixmapLR = Tk_GetPixmap(tree->display, Tk_WindowId(tkwin),
		MAX(left, right), Tk_Height(tkwin), Tk_Depth(tkwin));
    }

    if (top > 0 || bottom > 0) {
	pixmapTB = Tk_GetPixmap(tree->display, Tk_WindowId(tkwin),
		Tk_Width(tkwin), MAX(top, bottom), Tk_Depth(tkwin));
    }

/*    DebugDrawBorder(tree, 0, left, top, right, bottom);*/

    if (left > 0) {
	eTtk_DrawLayout(themeData->layout, state, pixmapLR);
	XCopyArea(tree->display, pixmapLR, drawable,
		tree->copyGC, 0, 0,
		left, Tk_Height(tkwin),
		0, 0);
    }

    if (top > 0) {
	eTtk_DrawLayout(themeData->layout, state, pixmapTB);
	XCopyArea(tree->display, pixmapTB, drawable,
		tree->copyGC, 0, 0,
		Tk_Width(tkwin), top,
		0, 0);
    }

    if (right > 0) {
	winBox.x -= winBox.width - right;
	eTtk_PlaceLayout(themeData->layout, state, winBox);

	eTtk_DrawLayout(themeData->layout, state, pixmapLR);
	XCopyArea(tree->display, pixmapLR, drawable,
		tree->copyGC, 0, 0,
		right, Tk_Height(tkwin),
		Tree_BorderRight(tree), 0);
    }

    if (bottom > 0) {
	winBox.x = 0;
	winBox.y -= winBox.height - bottom;
	eTtk_PlaceLayout(themeData->layout, state, winBox);

	eTtk_DrawLayout(themeData->layout, state, pixmapTB);
	XCopyArea(tree->display, pixmapTB, drawable,
		tree->copyGC, 0, 0,
		Tk_Width(tkwin), bottom,
		0, Tree_BorderBottom(tree));
    }

    if (pixmapLR != None)
	Tk_FreePixmap(tree->display, pixmapLR);
    if (pixmapTB != None)
	Tk_FreePixmap(tree->display, pixmapTB);

    return TCL_OK;
}

static Tk_OptionSpec NullOptionSpecs[] =
{
    {TK_OPTION_END, 0,0,0, NULL, -1,-1, 0,0,0}
};

/* from ttkTreeview.c */
static Ttk_Layout
GetSublayout(
    Tcl_Interp *interp,
    Ttk_Theme themePtr,
    Ttk_Layout parentLayout,
    const char *layoutName,
    Tk_OptionTable optionTable,
    Ttk_Layout *layoutPtr)
{
    Ttk_Layout newLayout = eTtk_CreateSublayout(
	    interp, themePtr, parentLayout, layoutName, optionTable);

    if (newLayout) {
	if (*layoutPtr)
	    eTtk_FreeLayout(*layoutPtr);
	*layoutPtr = newLayout;
    }
    return newLayout;
}

Ttk_Layout
TreeCtrlGetLayout(
    Tcl_Interp *interp,
    Ttk_Theme themePtr,
    void *recordPtr
    )
{
    TreeCtrl *tree = recordPtr;
    TreeThemeData themeData = tree->themeData;
    Ttk_Layout treeLayout, newLayout;

    if (themeData->headingOptionTable == NULL)
	themeData->headingOptionTable = Tk_CreateOptionTable(interp, NullOptionSpecs);
    if (themeData->buttonOptionTable == NULL)
	themeData->buttonOptionTable = Tk_CreateOptionTable(interp, NullOptionSpecs);

    /* Create a new layout record based on widget -style or class */
    treeLayout = eTtk_CreateLayout(interp, themePtr, "TreeCtrl", tree,
	    tree->optionTable, tree->tkwin);

    /* Create a sublayout for drawing the column headers. The sublayout is
     * called "TreeCtrl.TreeCtrlHeading" by default. The actual layout specification
     * was defined by Ttk_RegisterLayout("TreeCtrlHeading") below. */
    newLayout = GetSublayout(interp, themePtr, treeLayout,
	    ".TreeCtrlHeading", themeData->headingOptionTable,
	    &themeData->headingLayout);
    if (newLayout == NULL)
	return NULL;

    newLayout = GetSublayout(interp, themePtr, treeLayout,
	    ".TreeCtrlButton", themeData->buttonOptionTable,
	    &themeData->buttonLayout);
    if (newLayout == NULL)
	return NULL;

    return treeLayout;
}

void
TreeCtrlDoLayout(
    void *recordPtr
    )
{
    TreeCtrl *tree = recordPtr;
    TreeThemeData themeData = tree->themeData;
    Ttk_LayoutNode *node;
    Ttk_Box winBox = Ttk_WinBox(tree->tkwin);
    Ttk_State state = 0; /* ??? */

    eTtk_PlaceLayout(themeData->layout, state, winBox);
    node = eTtk_LayoutFindNode(themeData->layout, "client");
    if (node != NULL)
	themeData->clientBox = eTtk_LayoutNodeInternalParcel(themeData->layout, node);
    else
	themeData->clientBox = winBox;

    /* Size of opened and closed buttons. */
    eTtk_LayoutSize(themeData->buttonLayout, TTK_STATE_OPEN,
	    &themeData->buttonWidth[1], &themeData->buttonHeight[1]);
    eTtk_LayoutSize(themeData->buttonLayout, 0,
	    &themeData->buttonWidth[0], &themeData->buttonHeight[0]);

    node = eTtk_LayoutFindNode(themeData->buttonLayout, "indicator");
    if (node != NULL) {
	Ttk_Box box1, box2;

	box1 = Ttk_MakeBox(0, 0, themeData->buttonWidth[1], themeData->buttonHeight[1]);
	eTtk_PlaceLayout(themeData->buttonLayout, TTK_STATE_OPEN, box1);
	box2 = eTtk_LayoutNodeInternalParcel(themeData->buttonLayout, node);
	themeData->buttonPadding[1] = Ttk_MakePadding(box2.x, box2.y,
		(box1.x + box1.width) - (box2.x + box2.width),
		(box1.y + box1.height) - (box2.y + box2.height));

	box1 = Ttk_MakeBox(0, 0, themeData->buttonWidth[0], themeData->buttonHeight[0]);
	eTtk_PlaceLayout(themeData->buttonLayout, 0, box1);
	box2 = eTtk_LayoutNodeInternalParcel(themeData->buttonLayout, node);
	themeData->buttonPadding[0] = Ttk_MakePadding(box2.x, box2.y,
		(box1.x + box1.width) - (box2.x + box2.width),
		(box1.y + box1.height) - (box2.y + box2.height));

    } else {
	themeData->buttonPadding[1] = Ttk_MakePadding(0,0,0,0);
	themeData->buttonPadding[0] = Ttk_MakePadding(0,0,0,0);
    }
}

void
TreeTheme_Relayout(
    TreeCtrl *tree
    )
{
    TreeThemeData themeData = tree->themeData;
    Ttk_Theme themePtr = Ttk_GetCurrentTheme(tree->interp);
    Ttk_Layout newLayout = TreeCtrlGetLayout(tree->interp, themePtr, tree);

    if (newLayout) {
	if (themeData->layout) {
	    eTtk_FreeLayout(themeData->layout);
	}
	themeData->layout = newLayout;
	TreeCtrlDoLayout(tree);
    }
}

/* HeaderElement is used for Treeheading.cell. The platform-specific code
 * will draw the native heading. */
typedef struct
{
    Tcl_Obj *backgroundObj;
} HeaderElement;

static Ttk_ElementOptionSpec HeaderElementOptions[] =
{
    { "-background", TK_OPTION_COLOR,
	Tk_Offset(HeaderElement, backgroundObj), DEFAULT_BACKGROUND },
    {NULL}
};

static void HeaderElementDraw(
    void *clientData, void *elementRecord, Tk_Window tkwin,
    Drawable d, Ttk_Box b, Ttk_State state)
{
    HeaderElement *e = elementRecord;
    XColor *color = Tk_GetColorFromObj(tkwin, e->backgroundObj);
    GC gc = Tk_GCForColor(color, d);
    XFillRectangle(Tk_Display(tkwin), d, gc,
	    b.x, b.y, b.width, b.height);
}

static Ttk_ElementSpec HeaderElementSpec =
{
    TK_STYLE_VERSION_2,
    sizeof(HeaderElement),
    HeaderElementOptions,
    Ttk_NullElementGeometry,
    HeaderElementDraw
};

/* Default button element (aka Treeitem.indicator). */
typedef struct
{
    Tcl_Obj *backgroundObj;
    Tcl_Obj *colorObj;
    Tcl_Obj *sizeObj;
    Tcl_Obj *thicknessObj;
} TreeitemIndicator;

static Ttk_ElementOptionSpec TreeitemIndicatorOptions[] =
{
    { "-buttonbackground", TK_OPTION_COLOR,
	Tk_Offset(TreeitemIndicator, backgroundObj), "white" },
    { "-buttoncolor", TK_OPTION_COLOR,
	Tk_Offset(TreeitemIndicator, colorObj), "#808080" },
    { "-buttonsize", TK_OPTION_PIXELS,
	Tk_Offset(TreeitemIndicator, sizeObj), "9" },
    { "-buttonthickness", TK_OPTION_PIXELS,
	Tk_Offset(TreeitemIndicator, thicknessObj), "1" },
    {NULL}
};

static void TreeitemIndicatorSize(
    void *clientData, void *elementRecord, Tk_Window tkwin,
    int *widthPtr, int *heightPtr, Ttk_Padding *paddingPtr)
{
    TreeitemIndicator *indicator = elementRecord;
    int size = 0;

    Tk_GetPixelsFromObj(NULL, tkwin, indicator->sizeObj, &size);

    *widthPtr = *heightPtr = size;
    *paddingPtr = Ttk_MakePadding(0,0,0,0);
}

static void TreeitemIndicatorDraw(
    void *clientData, void *elementRecord, Tk_Window tkwin,
    Drawable d, Ttk_Box b, Ttk_State state)
{
    TreeitemIndicator *indicator = elementRecord;
    int w1, lineLeft, lineTop, buttonLeft, buttonTop, buttonThickness, buttonSize;
    XColor *bgColor = Tk_GetColorFromObj(tkwin, indicator->backgroundObj);
    XColor *buttonColor = Tk_GetColorFromObj(tkwin, indicator->colorObj);
    XGCValues gcValues;
    unsigned long gcMask;
    GC buttonGC;
    Ttk_Padding padding = Ttk_MakePadding(0,0,0,0);

    b = Ttk_PadBox(b, padding);

    Tk_GetPixelsFromObj(NULL, tkwin, indicator->sizeObj, &buttonSize);
    Tk_GetPixelsFromObj(NULL, tkwin, indicator->thicknessObj, &buttonThickness);

    w1 = buttonThickness / 2;

    /* Left edge of vertical line */
    /* Make sure this matches TreeItem_DrawLines() */
    lineLeft = b.x + (b.width - buttonThickness) / 2;

    /* Top edge of horizontal line */
    /* Make sure this matches TreeItem_DrawLines() */
    lineTop = b.y + (b.height - buttonThickness) / 2;

    buttonLeft = b.x;
    buttonTop = b.y;

    /* Erase button background */
    XFillRectangle(Tk_Display(tkwin), d,
	    Tk_GCForColor(bgColor, d),
	    buttonLeft + buttonThickness,
	    buttonTop + buttonThickness,
	    buttonSize - buttonThickness,
	    buttonSize - buttonThickness);

    gcValues.foreground = buttonColor->pixel;
    gcValues.line_width = buttonThickness;
    gcMask = GCForeground | GCLineWidth;
    buttonGC = Tk_GetGC(tkwin, gcMask, &gcValues);

    /* Draw button outline */
    XDrawRectangle(Tk_Display(tkwin), d, buttonGC,
	    buttonLeft + w1,
	    buttonTop + w1,
	    buttonSize - buttonThickness,
	    buttonSize - buttonThickness);

    /* Horizontal '-' */
    XFillRectangle(Tk_Display(tkwin), d, buttonGC,
	    buttonLeft + buttonThickness * 2,
	    lineTop,
	    buttonSize - buttonThickness * 4,
	    buttonThickness);

    if (!(state & TTK_STATE_OPEN)) {
	/* Finish '+' */
	XFillRectangle(Tk_Display(tkwin), d, buttonGC,
		lineLeft,
		buttonTop + buttonThickness * 2,
		buttonThickness,
		buttonSize - buttonThickness * 4);
    }

    Tk_FreeGC(Tk_Display(tkwin), buttonGC);
}

static Ttk_ElementSpec TreeitemIndicatorElementSpec =
{
    TK_STYLE_VERSION_2,
    sizeof(TreeitemIndicator),
    TreeitemIndicatorOptions,
    TreeitemIndicatorSize,
    TreeitemIndicatorDraw
};

TTK_BEGIN_LAYOUT(HeadingLayout)
    TTK_NODE("Treeheading.cell", TTK_FILL_BOTH)
    TTK_NODE("Treeheading.border", TTK_FILL_BOTH)
TTK_END_LAYOUT

TTK_BEGIN_LAYOUT(ButtonLayout)
    TTK_NODE("Treeitem.indicator", TTK_PACK_LEFT)
TTK_END_LAYOUT

TTK_BEGIN_LAYOUT(TreeCtrlLayout)
    TTK_GROUP("TreeCtrl.field", TTK_FILL_BOTH|TTK_BORDER,
	TTK_GROUP("TreeCtrl.padding", TTK_FILL_BOTH,
	    TTK_NODE("TreeCtrl.client", TTK_FILL_BOTH)))
TTK_END_LAYOUT

void TreeTheme_ThemeChanged(TreeCtrl *tree)
{
}

int TreeTheme_Init(TreeCtrl *tree)
{
    tree->themeData = (TreeThemeData) ckalloc(sizeof(TreeThemeData_));
    memset(tree->themeData, '\0', sizeof(TreeThemeData_));

    return TCL_OK;
}

int TreeTheme_Free(TreeCtrl *tree)
{
    TreeThemeData themeData = tree->themeData;

    if (themeData != NULL) {
	if (themeData->layout != NULL)
	    eTtk_FreeLayout(themeData->layout);
	if (themeData->buttonLayout != NULL)
	    eTtk_FreeLayout(themeData->buttonLayout);
	if (themeData->headingLayout != NULL)
	    eTtk_FreeLayout(themeData->headingLayout);
	ckfree((char *) themeData);
    }
    return TCL_OK;
}

int TreeTheme_InitInterp(Tcl_Interp *interp)
{
    Ttk_Theme theme = Ttk_GetDefaultTheme(interp);

    Ttk_RegisterLayout(theme, "TreeCtrl", TreeCtrlLayout);

    /* Problem: what if Treeview also defines this? */
    Ttk_RegisterElement(interp, theme, "Treeheading.cell", &HeaderElementSpec, 0);

    /* Problem: what if Treeview also defines this? */
    Ttk_RegisterElement(interp, theme, "Treeitem.indicator", &TreeitemIndicatorElementSpec, 0);

    Ttk_RegisterLayout(theme, "TreeCtrlHeading", HeadingLayout);
    Ttk_RegisterLayout(theme, "TreeCtrlButton", ButtonLayout);

    return TCL_OK;
}

#endif /* USE_TTK */
