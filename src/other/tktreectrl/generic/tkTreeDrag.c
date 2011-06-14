/* 
 * tkTreeDrag.c --
 *
 *	This module implements outline dragging for treectrl widgets.
 *
 * Copyright (c) 2002-2010 Tim Baker
 *
 * RCS: @(#) $Id$
 */

#include "tkTreeCtrl.h"

typedef struct TreeDragImage_ TreeDragImage_;
typedef struct DragElem DragElem;

/*
 * The following structure holds info about a single element of the drag
 * image.
 */
struct DragElem
{
    int x, y, width, height;
    DragElem *next;
};

/*
 * The following structure holds info about the drag image. There is one of
 * these per TreeCtrl.
 */
struct TreeDragImage_
{
    TreeCtrl *tree;
    Tk_OptionTable optionTable;
    int visible;
    int x, y; /* offset to draw at in canvas coords */
    TreeRectangle bounds; /* bounds of all DragElems */
    DragElem *elem;
    int onScreen; /* TRUE if is displayed */
    int sx, sy; /* Window coords where displayed */
    int sw, sh; /* Width/height of previously-displayed image */
#ifdef DRAG_PIXMAP
    int pixmapW, pixmapH;
    Pixmap pixmap;
    Tk_Image image;
#endif /* DRAG_PIXMAP */
#ifdef DRAGIMAGE_STYLE
    TreeStyle masterStyle; /* Style to create the drag image from. */
    TreeStyle instanceStyle; /* Style to create the drag image from. */
    int styleX, styleY; /* Mouse cursor hotspot offset into dragimage. */
    int styleW, styleH; /* Width/Height of dragimage style. */
    int pixmapW, pixmapH; /* Width/Height of 'pixmap'. */
    Pixmap pixmap; /* Pixmap -> Tk_Image. */
    Tk_Image tkimage; /* Transparent image that is drawn in the widget. */
#endif /* DRAGIMAGE_STYLE */
};

#define DRAG_CONF_VISIBLE		0x0001

static Tk_OptionSpec optionSpecs[] = {
#ifdef DRAGIMAGE_STYLE
    {TK_OPTION_CUSTOM, "-style", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeDragImage_, masterStyle),
     TK_OPTION_NULL_OK, (ClientData) &TreeCtrlCO_style, 0},
#endif /* DRAGIMAGE_STYLE */
    {TK_OPTION_BOOLEAN, "-visible", (char *) NULL, (char *) NULL,
	"0", -1, Tk_Offset(TreeDragImage_, visible),
	0, (ClientData) NULL, DRAG_CONF_VISIBLE},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, -1, 0, 0, 0}
};

#ifdef DRAG_PIXMAP
static void
UpdateImage(
    TreeDragImage dragImage	/* Drag image record. */
    )
{
    TreeCtrl *tree = dragImage->tree;
    Tk_PhotoHandle photoH;
    XImage *ximage;
    int width = TreeRect_Width(dragImage->bounds);
    int height = TreeRect_Height(dragImage->bounds);
    int alpha = 128;
    XColor *colorPtr;

    if (dragImage->image != NULL) {
	Tk_FreeImage(dragImage->image);
	dragImage->image = NULL;
    }

    photoH = Tk_FindPhoto(tree->interp, "::TreeCtrl::ImageDrag");
    if (photoH == NULL) {
	Tcl_GlobalEval(tree->interp, "image create photo ::TreeCtrl::ImageDrag");
	photoH = Tk_FindPhoto(tree->interp, "::TreeCtrl::ImageDrag");
	if (photoH == NULL)
	    return;
    }

    /* Pixmap -> XImage */
    ximage = XGetImage(tree->display, dragImage->pixmap, 0, 0,
	    (unsigned int)width, (unsigned int)height, AllPlanes, ZPixmap);
    if (ximage == NULL)
	panic("tkTreeDrag.c:UpdateImage() ximage is NULL");

    /* XImage -> Tk_Image */
    colorPtr = Tk_GetColor(tree->interp, tree->tkwin, "pink");
    Tree_XImage2Photo(tree->interp, photoH, ximage, colorPtr->pixel, alpha);

    XDestroyImage(ximage);

    dragImage->image = Tk_GetImage(tree->interp, tree->tkwin,
	"::TreeCtrl::ImageDrag", NULL, (ClientData) NULL);
}

static void
UpdatePixmap(
    TreeDragImage dragImage	/* Drag image record. */
    )
{
    TreeCtrl *tree = dragImage->tree;
    int w, h;
    XColor *colorPtr;
    GC gc;
    DragElem *elem;
    unsigned long trans;

    w = TreeRect_Width(dragImage->bounds);
    h = TreeRect_Height(dragImage->bounds);
    if (w > dragImage->pixmapW || h > dragImage->pixmapH) {
	if (dragImage->pixmap != None)
	    Tk_FreePixmap(tree->display, dragImage->pixmap);
	dragImage->pixmap = Tk_GetPixmap(tree->display,
	    Tk_WindowId(tree->tkwin),
	    w, h, Tk_Depth(tree->tkwin));

	dragImage->pixmapW = w;
	dragImage->pixmapH = h;
    }

    colorPtr = Tk_GetColor(tree->interp, tree->tkwin, "pink");
    gc = Tk_GCForColor(colorPtr, Tk_WindowId(tree->tkwin));
    XFillRectangle(tree->display, dragImage->pixmap, gc,
	0, 0, w, h);

    trans = colorPtr->pixel;

    colorPtr = Tk_GetColor(tree->interp, tree->tkwin, "gray50");
    gc = Tk_GCForColor(colorPtr, Tk_WindowId(tree->tkwin));

    for (elem = dragImage->elem; elem != NULL; elem = elem->next) {
	XFillRectangle(tree->display, dragImage->pixmap, gc,
	    elem->x - TreeRect_Left(dragImage->bounds),
	    elem->y - TreeRect_Top(dragImage->bounds),
	    elem->width, elem->height);
    }

    if (dragImage->image != NULL) {
	Tk_FreeImage(dragImage->image);
	dragImage->image = NULL;
    }
}

static void
DrawPixmap(
    TreeDragImage dragImage,	/* Drag image record. */
    TreeDrawable td
    )
{
    TreeCtrl *tree = dragImage->tree;
    int ix, iy, iw, ih;

    if (!dragImage->visible)
	return;

    if (dragImage->image == NULL)
	UpdateImage(dragImage);

    if (dragImage->image == NULL)
	return;

    ix = iy = 0;
    iw = TreeRect_Width(dragImage->bounds);
    ih = TreeRect_Height(dragImage->bounds);

    /* FIXME: clip src image to area to be redrawn */

    Tree_RedrawImage(dragImage->image, ix, iy, iw, ih, td,
	dragImage->x + TreeRect_Left(dragImage->bounds) - tree->drawableXOrigin,
	dragImage->y + TreeRect_Top(dragImage->bounds) - tree->drawableYOrigin);
}
#endif /* DRAG_PIXMAP */

#ifdef DRAGIMAGE_STYLE
void
TreeDragImage_StyleDeleted(
    TreeDragImage dragImage,	/* Drag image record. */
    TreeStyle style		/* Style that was deleted. */
    )
{
    TreeCtrl *tree = dragImage->tree;

    if (dragImage->masterStyle == style) {
	TreeStyle_FreeResources(tree, dragImage->instanceStyle);
	dragImage->masterStyle = NULL;
	dragImage->instanceStyle = NULL;
    }
}

static void
DragImage_UpdateStyleTkImage(
    TreeDragImage dragImage	/* Drag image record. */
    )
{
    TreeCtrl *tree = dragImage->tree;
    Tk_PhotoHandle photoH;
    XImage *ximage;
    int width = dragImage->styleW;
    int height = dragImage->styleH;
    int alpha = 128;
    XColor *colorPtr;

    if (dragImage->tkimage != NULL) {
	Tk_FreeImage(dragImage->tkimage);
	dragImage->tkimage = NULL;
    }

    photoH = Tk_FindPhoto(tree->interp, "::TreeCtrl::ImageDrag");
    if (photoH == NULL) {
	Tcl_GlobalEval(tree->interp, "image create photo ::TreeCtrl::ImageDrag");
	photoH = Tk_FindPhoto(tree->interp, "::TreeCtrl::ImageDrag");
	if (photoH == NULL)
	    return;
    }

    /* Pixmap -> XImage */
    ximage = XGetImage(tree->display, dragImage->pixmap, 0, 0,
	    (unsigned int)width, (unsigned int)height, AllPlanes, ZPixmap);
    if (ximage == NULL)
	panic("tkTreeDrag.c:DragImage_UpdateStyleTkImage() ximage is NULL");

    /* XImage -> Tk_Image */
    colorPtr = Tk_GetColor(tree->interp, tree->tkwin, "pink");
    Tree_XImage2Photo(tree->interp, photoH, ximage, colorPtr->pixel, alpha);

    XDestroyImage(ximage);

    dragImage->tkimage = Tk_GetImage(tree->interp, tree->tkwin,
	"::TreeCtrl::ImageDrag", NULL, (ClientData) NULL);
}

static void
DragImage_UpdateStylePixmap(
    TreeDragImage dragImage	/* Drag image record. */
    )
{
    TreeCtrl *tree = dragImage->tree;
    int w, h, state = 0;
    XColor *colorPtr;
    GC gc;
    StyleDrawArgs drawArgs;

    w = dragImage->styleW = TreeStyle_NeededWidth(tree, dragImage->instanceStyle, state);
    h = dragImage->styleH = TreeStyle_NeededHeight(tree, dragImage->instanceStyle, state);
    if (w > dragImage->pixmapW || h > dragImage->pixmapH)
    {

	if (dragImage->pixmap != None)
	    Tk_FreePixmap(tree->display, dragImage->pixmap);
	dragImage->pixmap = Tk_GetPixmap(tree->display,
	    Tk_WindowId(tree->tkwin),
	    w, h, Tk_Depth(tree->tkwin));

	dragImage->pixmapW = w;
	dragImage->pixmapH = h;
    }

    colorPtr = Tk_GetColor(tree->interp, tree->tkwin, "pink");
    gc = Tk_GCForColor(colorPtr, Tk_WindowId(tree->tkwin));
    XFillRectangle(tree->display, dragImage->pixmap, gc,
	0, 0, w, h);

    drawArgs.tree = tree;

    drawArgs.td.drawable = dragImage->pixmap;
    drawArgs.td.width = w; drawArgs.td.height = h;

    drawArgs.bounds[0] = drawArgs.bounds[1] = 0;
    drawArgs.bounds[2] = w; drawArgs.bounds[3] = h;

    drawArgs.state = state;
    drawArgs.style = dragImage->instanceStyle;

    drawArgs.indent = 0;

    drawArgs.x = drawArgs.y = 0;
    drawArgs.width = w; drawArgs.height = h;

    drawArgs.justify = TK_JUSTIFY_LEFT;

    TreeStyle_Draw(&drawArgs);

    if (dragImage->tkimage != NULL) {
	Tk_FreeImage(dragImage->tkimage);
	dragImage->tkimage = NULL;
    }
}

static void
DragImage_DrawStyle(
    TreeDragImage dragImage,	/* Drag image record. */
    TreeDrawable td		/* Where to draw. */
    )
{
    TreeCtrl *tree = dragImage->tree;
    int ix, iy, iw, ih;

    if (dragImage->tkimage == NULL)
	DragImage_UpdateStyleTkImage(dragImage);

    if (dragImage->tkimage == NULL)
	return;

    ix = iy = 0;
    iw = dragImage->styleW; ih = dragImage->styleH;

    Tree_RedrawImage(dragImage->tkimage, ix, iy, iw, ih, td,
	dragImage->x + -dragImage->styleX - tree->drawableXOrigin,
	dragImage->y + -dragImage->styleY - tree->drawableYOrigin);
}
#endif /* DRAGIMAGE_STYLE */

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_Draw --
 *
 *	Draw the elements that make up the drag image if it is visible.
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
TreeDragImage_Draw(
    TreeDragImage dragImage,	/* Drag image record. */
    TreeDrawable td		/* Where to draw. */
    )
{
#ifdef DRAG_PIXMAP
    DrawPixmap(tree->dragImage, tdrawable);

#elif 1 /* Use XOR dotted rectangles where possible. */
    TreeCtrl *tree = dragImage->tree;

    if (!dragImage->visible)
	return;

#ifdef DRAGIMAGE_STYLE
    if (dragImage->instanceStyle != NULL) {
	DragImage_DrawStyle(dragImage, td);
	return;
    }
#endif /* DRAGIMAGE_STYLE */

    /* Yes this is XOR drawing but we aren't erasing the previous
     * dragimage as when TreeDragImage_IsXOR() returns TRUE. */
    TreeDragImage_DrawXOR(dragImage, td.drawable,
	0 - tree->xOrigin, 0 - tree->yOrigin);
#else /* */
    TreeCtrl *tree = dragImage->tree;
    GC gc;
    DragElem *elem;
#if 1 /* Stippled rectangles: BUG not clipped to contentbox. */
    XGCValues gcValues;
    unsigned long mask;
    XPoint points[5];

    if (!dragImage->visible)
	return;

    gcValues.stipple = Tk_GetBitmap(tree->interp, tree->tkwin, "gray50");
    gcValues.fill_style = FillStippled;
    mask = GCStipple|GCFillStyle;
    gc = Tk_GetGC(tree->tkwin, mask, &gcValues);

    for (elem = dragImage->elem; elem != NULL; elem = elem->next) {
	XRectangle rect;
	rect.x = dragImage->x + elem->x /*- dragImage->bounds[0]*/ - tree->drawableXOrigin;
	rect.y = dragImage->y + elem->y /*- dragImage->bounds[1]*/ - tree->drawableYOrigin;
	rect.width = elem->width;
	rect.height = elem->height;

#ifdef WIN32
	/* XDrawRectangle ignores the stipple pattern. */
	points[0].x = rect.x, points[0].y = rect.y;
	points[1].x = rect.x + rect.width - 1, points[1].y = rect.y;
	points[2].x = rect.x + rect.width - 1, points[2].y = rect.y + rect.height - 1;
	points[3].x = rect.x, points[3].y = rect.y + rect.height - 1;
	points[4] = points[0];
	XDrawLines(tree->display, td.drawable, gc, points, 5, CoordModeOrigin);
#else /* !WIN32 */
	XDrawRectangle(tree->display, td.drawable, gc, rect.x, rect.y,
	    rect.width - 1, rect.height - 1);
#endif
    }

    Tk_FreeGC(tree->display, gc);
#else /* Debug/test: gray rectangles */
    XColor *colorPtr;
    TkRegion rgn;

    if (!dragImage->visible)
	return;

    colorPtr = Tk_GetColor(tree->interp, tree->tkwin, "gray50");
    gc = Tk_GCForColor(colorPtr, Tk_WindowId(tree->tkwin));

    rgn = Tree_GetRegion(tree);

    for (elem = dragImage->elem; elem != NULL; elem = elem->next) {
	XRectangle rect;
	rect.x = dragImage->x + elem->x /*- dragImage->bounds[0]*/ - tree->drawableXOrigin;
	rect.y = dragImage->y + elem->y /*- dragImage->bounds[1]*/ - tree->drawableYOrigin;
	rect.width = elem->width;
	rect.height = elem->height;
	TkUnionRectWithRegion(&rect, rgn, rgn);
    }

    Tree_FillRegion(tree->display, td.drawable, gc, rgn);

    Tree_FreeRegion(tree, rgn);
#endif /* Debug/test: gray rectangles */
#endif /* XOR */
}

/*
 *----------------------------------------------------------------------
 *
 * DragElem_Alloc --
 *
 *	Allocate and initialize a new DragElem record. Add the record
 *	to the list of records for the drag image.
 *
 * Results:
 *	Pointer to allocated DragElem.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static DragElem *
DragElem_Alloc(
    TreeDragImage dragImage	/* Drag image record. */
    )
{
    DragElem *elem = (DragElem *) ckalloc(sizeof(DragElem));
    DragElem *walk = dragImage->elem;
    memset(elem, '\0', sizeof(DragElem));
    if (dragImage->elem == NULL)
	dragImage->elem = elem;
    else {
	while (walk->next != NULL)
	    walk = walk->next;
	walk->next = elem;
    }
    return elem;
}

/*
 *----------------------------------------------------------------------
 *
 * DragElem_Free --
 *
 *	Free a DragElem.
 *
 * Results:
 *	Pointer to the next DragElem.
 *
 * Side effects:
 *	Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

static DragElem *
DragElem_Free(
    TreeDragImage dragImage,	/* Drag image record. */
    DragElem *elem		/* Drag element to free. */
    )
{
    DragElem *next = elem->next;
    WFREE(elem, DragElem);
    return next;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_Init --
 *
 *	Perform drag-image-related initialization when a new TreeCtrl is
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
TreeDragImage_Init(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeDragImage dragImage;

    dragImage = (TreeDragImage) ckalloc(sizeof(TreeDragImage_));
    memset(dragImage, '\0', sizeof(TreeDragImage_));
    dragImage->tree = tree;
    dragImage->optionTable = Tk_CreateOptionTable(tree->interp, optionSpecs);
    if (Tk_InitOptions(tree->interp, (char *) dragImage, dragImage->optionTable,
	tree->tkwin) != TCL_OK) {
	WFREE(dragImage, TreeDragImage_);
	return TCL_ERROR;
    }
    tree->dragImage = (TreeDragImage) dragImage;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_Free --
 *
 *	Free drag-image-related resources when a TreeCtrl is deleted.
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
TreeDragImage_Free(
    TreeDragImage dragImage	/* Drag image token. */
    )
{
    DragElem *elem = dragImage->elem;

    while (elem != NULL)
	elem = DragElem_Free(dragImage, elem);
#ifdef DRAG_PIXMAP
    if (dragImage->image != NULL)
	Tk_FreeImage(dragImage->image);
    if (dragImage->pixmap != None)
	Tk_FreePixmap(dragImage->tree->display, dragImage->pixmap);
#endif /* DRAG_PIXMAP */
    Tk_FreeConfigOptions((char *) dragImage, dragImage->optionTable,
	dragImage->tree->tkwin);
    WFREE(dragImage, TreeDragImage_);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_IsXOR --
 *
 *	Return true if the dragimage is being drawn with XOR.
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
TreeDragImage_IsXOR(
    TreeDragImage dragImage	/* Drag image token. */
    )
{
#if defined(WIN32)
    return FALSE; /* TRUE on XP, FALSE on Win7 (lots of flickering) */
#elif defined(MAC_OSX_TK)
    return FALSE; /* Cocoa doesn't have XOR */
#else /* X11 */
    /* With VirtualBox+Ubuntu get extreme lag if TRUE with Compiz. */
    /* With VirtualBox+Ubuntu get lots of flickering if TRUE without Compiz. */
    return FALSE;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_IsVisible --
 *
 *	Return true if the dragimage is being drawn.
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
TreeDragImage_IsVisible(
    TreeDragImage dragImage	/* Drag image token. */
    )
{
    return dragImage->visible;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_Display --
 *
 *	Draw the drag image if it is not already displayed and if
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
TreeDragImage_Display(
    TreeDragImage dragImage	/* Drag image token. */
    )
{
    TreeCtrl *tree = dragImage->tree;

    if (!dragImage->onScreen && dragImage->visible) {
	if (TreeDragImage_IsXOR(dragImage) == FALSE) {
	    dragImage->sx = dragImage->x + TreeRect_Left(dragImage->bounds) - tree->xOrigin;
	    dragImage->sy = dragImage->y + TreeRect_Top(dragImage->bounds) - tree->yOrigin;
	    dragImage->sw = TreeRect_Width(dragImage->bounds);
	    dragImage->sh = TreeRect_Height(dragImage->bounds);
/*	    Tree_InvalidateItemArea(tree, dragImage->sx, dragImage->sy,
		dragImage->sx + dragImage->sw, dragImage->sy + dragImage->sh);*/
	    Tree_EventuallyRedraw(tree);
	} else {
	    dragImage->sx = 0 - tree->xOrigin;
	    dragImage->sy = 0 - tree->yOrigin;
	    TreeDragImage_DrawXOR(dragImage, Tk_WindowId(tree->tkwin),
		    dragImage->sx, dragImage->sy);
	}
	dragImage->onScreen = TRUE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_Undisplay --
 *
 *	Erase the drag image if it is displayed.
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
TreeDragImage_Undisplay(
    TreeDragImage dragImage	/* Drag image token. */
    )
{
    TreeCtrl *tree = dragImage->tree;

    if (dragImage->onScreen) {
	if (TreeDragImage_IsXOR(dragImage) == FALSE) {
/*	    Tree_InvalidateItemArea(tree, dragImage->sx, dragImage->sy,
		dragImage->sx + dragImage->sw, dragImage->sy + dragImage->sh);*/
	    Tree_EventuallyRedraw(tree);
	} else {
	    TreeDragImage_DrawXOR(dragImage, Tk_WindowId(tree->tkwin),
		dragImage->sx, dragImage->sy);
	}
	dragImage->onScreen = FALSE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DragImage_Config --
 *
 *	This procedure is called to process an objc/objv list to set
 *	configuration options for a DragImage.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then an error message is left in interp's result.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for dragImage;  old resources get freed, if there
 *	were any.  Display changes may occur.
 *
 *----------------------------------------------------------------------
 */

static int
DragImage_Config(
    TreeDragImage dragImage,	/* Drag image record. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = dragImage->tree;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;
    int mask;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) dragImage, dragImage->optionTable,
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

    if (mask & DRAG_CONF_VISIBLE) {
	TreeDragImage_Undisplay((TreeDragImage) dragImage);
	TreeDragImage_Display((TreeDragImage) dragImage);
    }

#ifdef DRAGIMAGE_STYLE
    if (dragImage->instanceStyle != NULL) {
	TreeStyle_FreeResources(tree, dragImage->instanceStyle);
	dragImage->instanceStyle = NULL;
    }
    if (dragImage->masterStyle != NULL) {
	dragImage->instanceStyle = TreeStyle_NewInstance(tree, dragImage->masterStyle);
	DragImage_UpdateStylePixmap(dragImage);
    }
#endif /* DRAGIMAGE_STYLE */

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_DrawXOR --
 *
 *	Draw (or erase) the elements that make up the drag image.
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
TreeDragImage_DrawXOR(
    TreeDragImage dragImage,	/* Drag image record. */
    Drawable drawable,		/* Where to draw. */
    int xOffset,		/* Offset of the drawable from the top-left */
    int yOffset			/* of the canvas. */
    )
{
    TreeCtrl *tree = dragImage->tree;
    DragElem *elem = dragImage->elem;
    DotState dotState;

/*	if (!dragImage->visible)
	return; */
    if (elem == NULL)
	return;

    TreeDotRect_Setup(tree, drawable, &dotState);

    while (elem != NULL) {
	TreeDotRect_Draw(&dotState,
	    xOffset + dragImage->x + elem->x,
	    yOffset + dragImage->y + elem->y,
	    elem->width, elem->height);
	elem = elem->next;
    }

    TreeDotRect_Restore(&dotState);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImageCmd --
 *
 *	This procedure is invoked to process the [dragimage] widget
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
TreeDragImageCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = clientData;
    TreeDragImage dragImage = tree->dragImage;
    static CONST char *commandNames[] = { "add", "cget", "clear", "configure",
	"offset",
#ifdef DRAGIMAGE_STYLE
	"stylehotspot",
#endif /* DRAGIMAGE_STYLE */
	(char *) NULL };
    enum { COMMAND_ADD, COMMAND_CGET, COMMAND_CLEAR, COMMAND_CONFIGURE,
	COMMAND_OFFSET
#ifdef DRAGIMAGE_STYLE
	, COMMAND_STYLEHOTSPOT
#endif /* DRAGIMAGE_STYLE */
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
	/* T dragimage add I ?C? ?E ...? */
	case COMMAND_ADD: {
	    TreeItem item;
	    TreeItemColumn itemColumn;
	    TreeColumn treeColumn;
	    TreeRectangle rects[128];
	    DragElem *elem;
	    int i, count, result = TCL_OK;
	    int minX, minY, maxX, maxY;

	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "item ?column? ?element ...?");
		return TCL_ERROR;
	    }

	    if (TreeItem_FromObj(tree, objv[3], &item, IFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;

	    TreeDragImage_Undisplay(tree->dragImage);

	    /* Every element in every column. */
	    if (objc == 4) {

		treeColumn = tree->columns;
		itemColumn = TreeItem_GetFirstColumn(tree, item);
		while (itemColumn != NULL) {
		    if (TreeItemColumn_GetStyle(tree, itemColumn) != NULL) {
			count = TreeItem_GetRects(tree, item, treeColumn,
			    -1, NULL, rects);
			if (count == -1) {
			    result = TCL_ERROR;
			    goto doneADD;
			}
			for (i = 0; i < count; i++) {
			    elem = DragElem_Alloc(dragImage);
			    elem->x = rects[i].x;
			    elem->y = rects[i].y;
			    elem->width = rects[i].width;
			    elem->height = rects[i].height;
			}
		    }
		    treeColumn = TreeColumn_Next(treeColumn);
		    itemColumn = TreeItemColumn_GetNext(tree, itemColumn);
		}

	    } else {

		if (TreeColumn_FromObj(tree, objv[4], &treeColumn,
			CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK) {
		    result = TCL_ERROR;
		    goto doneADD;
		}

		/* Every element in a column. */
		if (objc == 5) {
		    objc = -1;
		    objv = NULL;

		/* List of elements in a column. */
		} else {
		    objc -= 5;
		    objv += 5;
		}
		 
		count = TreeItem_GetRects(tree, item, treeColumn, objc, objv,
			rects);
		if (count == -1) {
		    result = TCL_ERROR;
		    goto doneADD;
		}

		for (i = 0; i < count; i++) {
		    elem = DragElem_Alloc(dragImage);
		    elem->x = rects[i].x;
		    elem->y = rects[i].y;
		    elem->width = rects[i].width;
		    elem->height = rects[i].height;
		}
	    }

doneADD:
	    minX = 100000;
	    minY = 100000;
	    maxX = -100000;
	    maxY = -100000;
	    for (elem = dragImage->elem; elem != NULL; elem = elem->next) {
		if (elem->x < minX)
		    minX = elem->x;
		if (elem->y < minY)
		    minY = elem->y;
		if (elem->x + elem->width > maxX)
		    maxX = elem->x + elem->width;
		if (elem->y + elem->height > maxY)
		    maxY = elem->y + elem->height;
	    }
	    TreeRect_SetXYXY(dragImage->bounds, minX, minY, maxX, maxY);
#ifdef DRAG_PIXMAP
	    UpdatePixmap(dragImage);
#endif /* DRAG_PIXMAP */
	    TreeDragImage_Display(tree->dragImage);
	    return result;
	}

	/* T dragimage cget option */
	case COMMAND_CGET: {
	    Tcl_Obj *resultObjPtr;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "option");
		return TCL_ERROR;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) dragImage,
		dragImage->optionTable, objv[3], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T dragimage clear */
	case COMMAND_CLEAR: {
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
		return TCL_ERROR;
	    }
	    if (dragImage->elem != NULL) {
		DragElem *elem = dragImage->elem;
		TreeDragImage_Undisplay(tree->dragImage);
/*				if (dragImage->visible)
		    DragImage_Redraw(dragImage); */
		while (elem != NULL)
		    elem = DragElem_Free(dragImage, elem);
		dragImage->elem = NULL;
	    }
	    break;
	}

	/* T dragimage configure ?option? ?value? ?option value ...? */
	case COMMAND_CONFIGURE: {
	    Tcl_Obj *resultObjPtr;

	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 3, objv, "?option? ?value?");
		return TCL_ERROR;
	    }
	    if (objc <= 4) {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) dragImage,
		    dragImage->optionTable,
		    (objc == 3) ? (Tcl_Obj *) NULL : objv[3],
		    tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    return DragImage_Config(dragImage, objc - 3, objv + 3);
	}

	/* T dragimage offset ?x y? */
	case COMMAND_OFFSET: {
	    int x, y;

	    if (objc != 3 && objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "?x y?");
		return TCL_ERROR;
	    }
	    if (objc == 3) {
		FormatResult(interp, "%d %d", dragImage->x, dragImage->y);
		break;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)
		return TCL_ERROR;
	    TreeDragImage_Undisplay(tree->dragImage);
/*			if (dragImage->visible)
		DragImage_Redraw(dragImage); */
	    dragImage->x = x;
	    dragImage->y = y;
	    TreeDragImage_Display(tree->dragImage);
	    break;
	}

#ifdef DRAGIMAGE_STYLE
	/* T dragimage stylehotspot ?x y? */
	case COMMAND_STYLEHOTSPOT: {
	    int x, y;

	    if (objc != 3 && objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "?x y?");
		return TCL_ERROR;
	    }
	    if (objc == 3) {
		FormatResult(interp, "%d %d", dragImage->styleX, dragImage->styleY);
		break;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)
		return TCL_ERROR;
	    TreeDragImage_Undisplay(tree->dragImage);
	    dragImage->styleX = x;
	    dragImage->styleY = y;
	    TreeDragImage_Display(tree->dragImage);
	    break;
	}
#endif /* DRAGIMAGE_STYLE */
    }

    return TCL_OK;
}
