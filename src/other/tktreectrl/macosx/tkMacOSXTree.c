/* 
 * tkMacOSXTree.c --
 *
 *	Platform-specific parts of TkTreeCtrl for Mac OSX (Cocoa API).
 *
 * Copyright (c) 2010 Tim Baker
 *
 * RCS: @(#) $Id$
 */

#include "tkTreeCtrl.h"
#include "tkMacOSXInt.h"
#import <Carbon/Carbon.h>
#ifdef MAC_TK_COCOA
#import <Cocoa/Cocoa.h>
#endif

/* CGFloat is available Mac OS X v10.5 and later. */
#ifndef CGFLOAT_DEFINED
#  if __LP64__
#    define CGFloat double
#  else
#    define CGFloat float
#  endif
#endif

#define radians(d) ((d) * (M_PI/180.0))

#if MAC_TK_COCOA /*(TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION >= 6)*/
typedef struct {
    CGContextRef context;
} MacContextSetup;
#endif /* Tk 8.6 */

#if MAC_TK_CARBON /*(TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION < 6)*/
typedef struct {
    CGrafPtr port, savePort;
    Boolean portChanged;
    CGContextRef context;
} MacContextSetup;
#endif /* Tk 8.4 + 8.5 */

MODULE_SCOPE CGContextRef TreeMacOSX_GetContext(TreeCtrl *tree,
    Drawable pixmap, TreeRectangle tr, MacContextSetup *dc);
MODULE_SCOPE void TreeMacOSX_ReleaseContext(TreeCtrl *tree,
    MacContextSetup *dc);

#define BlueFloatFromXPixel(pixel)   (float) (((pixel >> 0)  & 0xFF)) / 255.0
#define GreenFloatFromXPixel(pixel)  (float) (((pixel >> 8)  & 0xFF)) / 255.0
#define RedFloatFromXPixel(pixel)    (float) (((pixel >> 16) & 0xFF)) / 255.0

/*
 *----------------------------------------------------------------------
 *
 * Tree_HDotLine --
 *
 *	Draws a horizontal 1-pixel-tall dotted line.
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
Tree_HDotLine(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    int x1, int y1, int x2	/* Left, top and right coordinates. */
    )
{
    int nw;
    int wx = x1 + tree->drawableXOrigin;
    int wy = y1 + tree->drawableYOrigin;
#if 1
    MacDrawable *macDraw = (MacDrawable *) drawable;
    TreeRectangle tr;
    MacContextSetup dc;
    CGContextRef context;
    CGFloat lengths[1] = {1.0f};

    if (!(macDraw->flags & TK_IS_PIXMAP)) {
	return;
    }

    tr.x = x1, tr.y = y1, tr.width = x2-x1, tr.height = 1;
    context = TreeMacOSX_GetContext(tree, drawable, tr, &dc);
    if (context == NULL) {
	return;
    }

    CGContextBeginPath(context);
    CGContextMoveToPoint(context, x1 /*+ 0.5*/, y1 + 0.5);
    CGContextAddLineToPoint(context, x2 /*- 0.5*/, y1 + 0.5);

    CGContextSetRGBStrokeColor(context,
	RedFloatFromXPixel(tree->lineGC[0]->foreground),
	GreenFloatFromXPixel(tree->lineGC[0]->foreground),
	BlueFloatFromXPixel(tree->lineGC[0]->foreground), 1.0);
    CGContextSetLineWidth(context, 0.01);
    nw = !(wx & 1) == !(wy & 1);
    CGContextSetLineDash(context, nw ? 0 : 1, lengths, 1);
    CGContextSetShouldAntialias(context, 0);
    CGContextStrokePath(context);

    TreeMacOSX_ReleaseContext(tree, &dc);
#else
    nw = !(wx & 1) == !(wy & 1);
    for (x1 += !nw; x1 < x2; x1 += 2) {
	XDrawPoint(tree->display, drawable, gc, x1, y1);
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_VDotLine --
 *
 *	Draws a vertical 1-pixel-wide dotted line.
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
Tree_VDotLine(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    int x1, int y1, int y2)	/* Left, top, and bottom coordinates. */
{
    int nw;
    int wx = x1 + tree->drawableXOrigin;
    int wy = y1 + tree->drawableYOrigin;
#if 1
    MacDrawable *macDraw = (MacDrawable *) drawable;
    TreeRectangle tr;
    MacContextSetup dc;
    CGContextRef context;
    CGFloat lengths[1] = {1.0f};

    if (!(macDraw->flags & TK_IS_PIXMAP)) {
	return;
    }

    tr.x = x1, tr.y = y1, tr.width = x1, tr.height = y2-y1;
    context = TreeMacOSX_GetContext(tree, drawable, tr, &dc);
    if (context == NULL) {
	return;
    }

    CGContextBeginPath(context);
    CGContextMoveToPoint(context, x1 + 0.5, y1 /*+ 0.5*/);
    CGContextAddLineToPoint(context, x1 + 0.5, y2 - 0.5);

    CGContextSetRGBStrokeColor(context,
	RedFloatFromXPixel(tree->lineGC[0]->foreground),
	GreenFloatFromXPixel(tree->lineGC[0]->foreground),
	BlueFloatFromXPixel(tree->lineGC[0]->foreground), 1.0);
    CGContextSetLineWidth(context, 0.01);
    nw = !(wx & 1) == !(wy & 1);
    CGContextSetLineDash(context, nw ? 0 : 1, lengths, 1);
    CGContextSetShouldAntialias(context, 0);
    CGContextStrokePath(context);

    TreeMacOSX_ReleaseContext(tree, &dc);
#else
    nw = !(wx & 1) == !(wy & 1);
    for (y1 += !nw; y1 < y2; y1 += 2) {
	XDrawPoint(tree->display, drawable, gc, x1, y1);
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_DrawActiveOutline --
 *
 *	Draws 0 or more sides of a rectangle, dot-on dot-off, XOR style.
 *	This is used by rectangle Elements to indicate the "active"
 *	item.
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
Tree_DrawActiveOutline(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    int x, int y,		/* Left and top coordinates. */
    int width, int height,	/* Size of rectangle. */
    int open			/* RECT_OPEN_x flags */
    )
{
#if 1
    int w = !(open & RECT_OPEN_W);
    int n = !(open & RECT_OPEN_N);
    int e = !(open & RECT_OPEN_E);
    int s = !(open & RECT_OPEN_S);
    XGCValues gcValues;
    unsigned long gcMask;
    GC gc;

    gcValues.function = GXcopy;
    gcMask = GCFunction;
    gc = Tree_GetGC(tree, gcMask, &gcValues);

    if (w) /* left */
    {
	Tree_VDotLine(tree, drawable, x, y, y + height);
    }
    if (n) /* top */
    {
	Tree_HDotLine(tree, drawable, x, y, x + width);
    }
    if (e) /* right */
    {
	Tree_VDotLine(tree, drawable, x + width - 1, y, y + height);
    }
    if (s) /* bottom */
    {
	Tree_HDotLine(tree, drawable, x, y + height - 1, x + width);
    }
#else
    int wx = x + tree->drawableXOrigin;
    int wy = y + tree->drawableYOrigin;
    int w = !(open & RECT_OPEN_W);
    int n = !(open & RECT_OPEN_N);
    int e = !(open & RECT_OPEN_E);
    int s = !(open & RECT_OPEN_S);
    int nw, ne, sw, se;
    int i;
    XGCValues gcValues;
    unsigned long gcMask;
    GC gc;

    /* Dots on even pixels only */
    nw = !(wx & 1) == !(wy & 1);
    ne = !((wx + width - 1) & 1) == !(wy & 1);
    sw = !(wx & 1) == !((wy + height - 1) & 1);
    se = !((wx + width - 1) & 1) == !((wy + height - 1) & 1);

    gcValues.function = GXcopy;
    gcMask = GCFunction;
    gc = Tree_GetGC(tree, gcMask, &gcValues);

    if (w) /* left */
    {
	for (i = !nw; i < height; i += 2) {
	    XDrawPoint(tree->display, drawable, gc, x, y + i);
	}
    }
    if (n) /* top */
    {
	for (i = nw ? w * 2 : 1; i < width; i += 2) {
	    XDrawPoint(tree->display, drawable, gc, x + i, y);
	}
    }
    if (e) /* right */
    {
	for (i = ne ? n * 2 : 1; i < height; i += 2) {
	    XDrawPoint(tree->display, drawable, gc, x + width - 1, y + i);
	}
    }
    if (s) /* bottom */
    {
	for (i = sw ? w * 2 : 1; i < width - (se && e); i += 2) {
	    XDrawPoint(tree->display, drawable, gc, x + i, y + height - 1);
	}
    }
#endif
}

/*
 * The following structure is used when drawing a number of dotted XOR
 * rectangles.
 */
struct DotStatePriv
{
    TreeCtrl *tree;
#if 1
    MacContextSetup dc;
    CGContextRef context;
#else
    Drawable drawable;
    GC gc;
    TkRegion rgn;
#endif
};

/*
 *----------------------------------------------------------------------
 *
 * TreeDotRect_Setup --
 *
 *	Prepare a drawable for drawing a series of dotted XOR rectangles.
 *
 * Results:
 *	State info is returned to be used by the other TreeDotRect_xxx()
 *	procedures.
 *
 * Side effects:
 *	On Win32 and OSX the device context/graphics port is altered
 *	in preparation for drawing. On X11 a new graphics context is
 *	created.
 *
 *----------------------------------------------------------------------
 */

void
TreeDotRect_Setup(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    DotState *p			/* Where to save state info. */
    )
{
#if 1
    struct DotStatePriv *dotState = (struct DotStatePriv *) p;
    MacDrawable *macDraw = (MacDrawable *) drawable;
    TreeRectangle tr;
    CGContextRef context;
    CGRect r;

    if (sizeof(*dotState) > sizeof(*p))
	panic("TreeDotRect_Setup: DotState hack is too small");

    dotState->context = NULL;
    if (!(macDraw->flags & TK_IS_PIXMAP)) {
	return;
    }

    tr.x = 0, tr.y = 0, tr.width = 1, tr.height = 1;
    context = dotState->context = TreeMacOSX_GetContext(tree, drawable, tr,
	&dotState->dc);
    if (context == NULL) {
	return;
    }

    /* Keep drawing inside the contentbox. */
    CGContextBeginPath(context);
    r = CGRectMake(Tree_ContentLeft(tree),
	Tree_ContentTop(tree),
	Tree_ContentWidth(tree),
	Tree_ContentHeight(tree));
    CGContextAddRect(context, r);
    CGContextClip(context);
#else
    struct DotStatePriv *dotState = (struct DotStatePriv *) p;
    XGCValues gcValues;
    unsigned long mask;
    XRectangle xrect;

    if (sizeof(*dotState) > sizeof(*p))
	panic("TreeDotRect_Setup: DotState hack is too small");

    dotState->tree = tree;
    dotState->drawable = drawable;

    gcValues.line_style = LineOnOffDash;
    gcValues.line_width = 1;
    gcValues.dash_offset = 0;
    gcValues.dashes = 1;

    /* Can't get a 1-pixel dash pattern when antialiasing is used. */
    /* See ::tk::mac::CGAntialiasLimit".*/
    gcValues.dashes = 2;
    gcValues.function = GXcopy;
    gcValues.cap_style = CapButt; /* doesn't affect display */
    mask = GCLineWidth | GCLineStyle | GCDashList | GCDashOffset | GCFunction | GCCapStyle;

    dotState->gc = Tk_GetGC(tree->tkwin, mask, &gcValues);

    /* Keep drawing inside the contentbox. */
    dotState->rgn = Tree_GetRegion(tree);
    xrect.x = Tree_ContentLeft(tree);
    xrect.y = Tree_ContentTop(tree);
    xrect.width = Tree_ContentRight(tree) - xrect.x;
    xrect.height = Tree_ContentBottom(tree) - xrect.y;
    TkUnionRectWithRegion(&xrect, dotState->rgn, dotState->rgn);
    TkSetRegion(tree->display, dotState->gc, dotState->rgn);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDotRect_Draw --
 *
 *	Draw a dotted XOR rectangle.
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
TreeDotRect_Draw(
    DotState *p,		/* Info returned by TreeDotRect_Setup(). */
    int x, int y,		/* Left and top coordinates. */
    int width, int height	/* Size of rectangle. */
    )
{
    struct DotStatePriv *dotState = (struct DotStatePriv *) p;
#if 1
    CGContextRef context = dotState->context;
    CGRect r = CGRectMake(x, y, width, height);

    if (dotState->context == NULL)
	return;
    CGContextAddRect(context, r);
#else
    XDrawRectangle(dotState->tree->display, dotState->drawable, dotState->gc,
	x, y, width - 1, height - 1);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDotRect_Restore --
 *
 *	Restore the drawing environment.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	On Win32 and OSX the device context/graphics port is restored.
 *	On X11 a new graphics context is freed.
 *
 *----------------------------------------------------------------------
 */

void
TreeDotRect_Restore(
    DotState *p			/* Info returned by TreeDotRect_Setup(). */
    )
{
    struct DotStatePriv *dotState = (struct DotStatePriv *) p;
#if 1
    CGContextRef context = dotState->context;
    CGFloat lengths[1] = {1.0f};

    if (dotState->context == NULL)
	return;

    CGContextSetRGBStrokeColor(context, 0.0, 0.0, 0.0, 1.0);
    CGContextSetLineWidth(context, 0.01);
    CGContextSetLineDash(context, 0, lengths, 1);
    CGContextSetShouldAntialias(context, 0);
    CGContextStrokePath(context);

    TreeMacOSX_ReleaseContext(dotState->tree, &dotState->dc);
#else
    XSetClipMask(dotState->tree->display, dotState->gc, None);
    Tree_FreeRegion(dotState->tree, dotState->rgn);
    Tk_FreeGC(dotState->tree->display, dotState->gc);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_FillRegion --
 *
 *	Paint a region with the foreground color of a graphics context.
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
Tree_FillRegion(
    Display *display,		/* Display. */
    Drawable drawable,		/* Where to draw. */
    GC gc,			/* Foreground color. */
    TkRegion rgn		/* Region to paint. */
    )
{
    XRectangle box;

    TkClipBox(rgn, &box);
    TkSetRegion(display, gc, rgn);
    XFillRectangle(display, drawable, gc, box.x, box.y, box.width, box.height);
    XSetClipMask(display, gc, None);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_OffsetRegion --
 *
 *	Offset a region.
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
Tree_OffsetRegion(
    TkRegion region,		/* Region to modify. */
    int xOffset, int yOffset	/* Horizontal and vertical offsets. */
    )
{
    HIShapeOffset((HIMutableShapeRef) region, xOffset, yOffset);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_UnionRegion --
 *
 *	Compute the union of 2 regions.
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
Tree_UnionRegion(
    TkRegion rgnA,
    TkRegion rgnB,
    TkRegion rgnOut)
{
    HIShapeUnion((HIShapeRef) rgnA, (HIShapeRef) rgnB,
	(HIMutableShapeRef) rgnOut);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_ScrollWindow --
 *
 *	Wrapper around TkScrollWindow() to fix an apparent bug with the
 *	Mac/OSX versions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is scrolled in a drawable.
 *
 *----------------------------------------------------------------------
 */

int
Tree_ScrollWindow(
    TreeCtrl *tree,		/* Widget info. */
    GC gc,			/* Arg to TkScrollWindow(). */
    int x, int y,		/* Arg to TkScrollWindow(). */
    int width, int height,	/* Arg to TkScrollWindow(). */
    int dx, int dy,		/* Arg to TkScrollWindow(). */
    TkRegion damageRgn		/* Arg to TkScrollWindow(). */
    )
{
    int result = TkScrollWindow(tree->tkwin, gc, x, y, width, height, dx, dy,
	damageRgn);
#if (TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION < 6)
    {
	MacDrawable *macWin = (MacDrawable *) Tk_WindowId(tree->tkwin);
	/* BUG IN TK? */
	Tree_OffsetRegion(damageRgn, -macWin->xOff, -macWin->yOff);
    }
#endif
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_UnsetClipMask --
 *
 *	Wrapper around XSetClipMask(). On Win32 Tk_DrawChars() does
 *	not clear the clipping region.
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
Tree_UnsetClipMask(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    GC gc			/* Graphics context to modify. */
    )
{
    XSetClipMask(tree->display, gc, None);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_DrawBitmapWithGC --
 *
 *	Draw part of a bitmap.
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
Tree_DrawBitmapWithGC(
    TreeCtrl *tree,		/* Widget info. */
    Pixmap bitmap,		/* Bitmap to draw. */
    Drawable drawable,		/* Where to draw. */
    GC gc,			/* Graphics context. */
    int src_x, int src_y,	/* Left and top of part of bitmap to copy. */
    int width, int height,	/* Width and height of part of bitmap to
				 * copy. */
    int dest_x, int dest_y	/* Left and top coordinates to copy part of
				 * the bitmap to. */
    )
{
    XSetClipOrigin(tree->display, gc, dest_x, dest_y);
    XCopyPlane(tree->display, bitmap, drawable, gc,
	src_x, src_y, (unsigned int) width, (unsigned int) height,
	dest_x, dest_y, 1);
    XSetClipOrigin(tree->display, gc, 0, 0);
}

/*
 * TIP #116 altered Tk_PhotoPutBlock API to add interp arg.
 * We need to remove that for compiling with 8.4.
 */
#if (TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION < 5)
#define TK_PHOTOPUTBLOCK(interp, hdl, blk, x, y, w, h, cr) \
	Tk_PhotoPutBlock(hdl, blk, x, y, w, h, cr)
#define TK_PHOTOPUTZOOMEDBLOCK(interp, hdl, blk, x, y, w, h, \
		zx, zy, sx, sy, cr) \
	Tk_PhotoPutZoomedBlock(hdl, blk, x, y, w, h, \
		zx, zy, sx, sy, cr)
#else
#define TK_PHOTOPUTBLOCK	Tk_PhotoPutBlock
#define TK_PHOTOPUTZOOMEDBLOCK	Tk_PhotoPutZoomedBlock
#endif

/*
 *----------------------------------------------------------------------
 *
 * Tree_XImage2Photo --
 *
 *	Copy pixels from an XImage to a Tk photo image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The given photo image is blanked and all the pixels from the
 *	XImage are put into the photo image.
 *
 *----------------------------------------------------------------------
 */

void
Tree_XImage2Photo(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tk_PhotoHandle photoH,	/* Existing photo image. */
    XImage *ximage,		/* XImage to copy pixels from. */
    unsigned long trans,	/* Pixel value in ximage that should be
				 * considered transparent. */
    int alpha			/* Desired transparency of photo image.*/
    )
{
    Tk_PhotoImageBlock photoBlock;
    unsigned char *pixelPtr;
    int x, y, w = ximage->width, h = ximage->height;
    unsigned long red_shift, green_shift, blue_shift;

    Tk_PhotoBlank(photoH);

    /* See TkPoscriptImage */

    red_shift = green_shift = blue_shift = 0;
    while ((0x0001 & (ximage->red_mask >> red_shift)) == 0)
	red_shift++;
    while ((0x0001 & (ximage->green_mask >> green_shift)) == 0)
	green_shift++;
    while ((0x0001 & (ximage->blue_mask >> blue_shift)) == 0)
	blue_shift++;

    pixelPtr = (unsigned char *) Tcl_Alloc(ximage->width * ximage->height * 4);
    photoBlock.pixelPtr  = pixelPtr;
    photoBlock.width     = ximage->width;
    photoBlock.height    = ximage->height;
    photoBlock.pitch     = ximage->width * 4;
    photoBlock.pixelSize = 4;
    photoBlock.offset[0] = 0;
    photoBlock.offset[1] = 1;
    photoBlock.offset[2] = 2;
    photoBlock.offset[3] = 3;

    for (y = 0; y < ximage->height; y++) {
	for (x = 0; x < ximage->width; x++) {
	    int r, g, b;
	    unsigned long pixel;

	    /* FIXME: I think this blows up on classic Mac??? */
	    pixel = XGetPixel(ximage, x, y);

	    /* Set alpha=0 for transparent pixel in the source XImage */
	    if (trans != 0 && pixel == trans) {
		pixelPtr[y * photoBlock.pitch + x * 4 + 3] = 0;
		continue;
	    }

	    r = (pixel & ximage->red_mask) >> red_shift;
	    g = (pixel & ximage->green_mask) >> green_shift;
	    b = (pixel & ximage->blue_mask) >> blue_shift;

	    pixelPtr[y * photoBlock.pitch + x * 4 + 0] = r;
	    pixelPtr[y * photoBlock.pitch + x * 4 + 1] = g;
	    pixelPtr[y * photoBlock.pitch + x * 4 + 2] = b;
	    pixelPtr[y * photoBlock.pitch + x * 4 + 3] = alpha;
	}
    }

    TK_PHOTOPUTBLOCK(interp, photoH, &photoBlock, 0, 0, w, h,
	    TK_PHOTO_COMPOSITE_SET);

    Tcl_Free((char *) pixelPtr);
}

typedef struct {
    TreeCtrl *tree;
    TreeClip *clip;
    GC gc;
    TkRegion region;
} TreeClipStateGC;

static void
TreeClip_ToGC(
    TreeCtrl *tree,		/* Widget info. */
    TreeClip *clip,		/* Clipping area or NULL. */
    GC gc,			/* Graphics context. */
    TreeClipStateGC *state
    )
{
    state->tree = tree;
    state->clip = clip;
    state->gc = gc;
    state->region = None;

    if (clip && clip->type == TREE_CLIP_RECT) {
	state->region = Tree_GetRegion(tree);
	Tree_SetRectRegion(state->region, &clip->tr);
	TkSetRegion(tree->display, gc, state->region);
    }
    if (clip && clip->type == TREE_CLIP_AREA) {
	TreeRectangle tr;
	if (Tree_AreaBbox(tree, clip->area, &tr) == 0)
	    return;
	state->region = Tree_GetRectRegion(tree, &tr);
	TkSetRegion(tree->display, gc, state->region);
    }
    if (clip && clip->type == TREE_CLIP_REGION) {
	TkSetRegion(tree->display, gc, clip->region);
    }
}

static void
TreeClip_FinishGC(
    TreeClipStateGC *state
    )
{
    XSetClipMask(state->tree->display, state->gc, None);
    if (state->region != None)
	Tree_FreeRegion(state->tree, state->region);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_FillRectangle --
 *
 *	Wrapper around XFillRectangle() because the clip region is
 *	ignored on Win32.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws onto the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
Tree_FillRectangle(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    GC gc,			/* Graphics context. */
    TreeRectangle tr		/* Rectangle to paint. */
    )
{
    TreeClipStateGC clipState;

    TreeClip_ToGC(tree, clip, gc, &clipState);
    XFillRectangle(tree->display, td.drawable, gc, tr.x, tr.y, tr.width, tr.height);
    TreeClip_FinishGC(&clipState);
}

/*** Themes ***/

struct TreeThemeData_ {
    TreeItem animButtonItem;
    Tcl_TimerToken animButtonTimer;
    int animButtonAngle;
};

static HIThemeButtonDrawInfo
GetThemeButtonDrawInfo(
    TreeCtrl *tree,
    int state,
    int arrow
    )
{
    HIThemeButtonDrawInfo info;

    info.version = 0;
    switch (state) {
	case COLUMN_STATE_ACTIVE:  info.state = kThemeStateActive /* kThemeStateRollover */; break;
	case COLUMN_STATE_PRESSED: info.state = kThemeStatePressed; break;
	default:		   info.state = kThemeStateActive; break;
    }
    /* Background window */
    if (!tree->isActive)
	info.state = kThemeStateInactive;
    info.kind = kThemeListHeaderButton;
    info.value = (arrow != COLUMN_ARROW_NONE) ? kThemeButtonOn : kThemeButtonOff;
    switch (arrow) {
	case COLUMN_ARROW_NONE: info.adornment = kThemeAdornmentHeaderButtonNoSortArrow; break;
	case COLUMN_ARROW_UP: info.adornment = kThemeAdornmentHeaderButtonSortUp; break;
	case COLUMN_ARROW_DOWN:
	default:
	    info.adornment = kThemeAdornmentDefault; break;
    }

    return info;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_DrawHeaderItem --
 *
 *	Draws the background of a single column header.  On Mac OS X
 *	this also draws the sort arrow, if any.
 *
 * Results:
 *	TCL_OK if drawing occurred, TCL_ERROR if the X11 fallback
 *	should be used.
 *
 * Side effects:
 *	Drawing.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_DrawHeaderItem(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    int state,			/* COLUMN_STATE_xxx flags. */
    int arrow,			/* COLUMN_ARROW_xxx flags. */
    int visIndex,		/* 0-based index in list of visible columns. */
    int x, int y,		/* Bounds of the header. */
    int width, int height	/* Bounds of the header. */
    )
{
    MacDrawable *macDraw = (MacDrawable *) td.drawable;
    CGRect bounds;
    HIThemeButtonDrawInfo info;
    MacContextSetup dc;
    CGContextRef context;
    HIShapeRef boundsRgn;
    int leftEdgeOffset;
    TreeRectangle tr;

    if (!(macDraw->flags & TK_IS_PIXMAP))
	return TCL_ERROR;

    info = GetThemeButtonDrawInfo(tree, state, arrow);

    tr.x = x, tr.y = y, tr.width = width, tr.height = height;
    context = TreeMacOSX_GetContext(tree, td.drawable, tr, &dc);
    if (context == NULL)
	return TCL_ERROR;

    /* See SF patch 'aqua header drawing - ID: 1356447' */
    /* The left edge overlaps the right edge of the previous column. */
    /* Only show the left edge if this is the first column or the
     * "blue" column (with a sort arrow). */
    if (visIndex == 0 || arrow == COLUMN_ARROW_NONE)
	leftEdgeOffset = 0;
    else
	leftEdgeOffset = -1;

    /* Create a clipping region as big as the header. */
    bounds.origin.x = macDraw->xOff + x + leftEdgeOffset;
    bounds.origin.y = macDraw->yOff + y;
    bounds.size.width = width - leftEdgeOffset;
    bounds.size.height = height;
    boundsRgn = HIShapeCreateWithRect(&bounds);

    /* Set the clipping region */
    HIShapeReplacePathInCGContext(boundsRgn, context);
    CGContextEOClip(context);

    /* See SF patch 'aqua header drawing - ID: 1356447' */
    if (visIndex == 0)
	leftEdgeOffset = 0;
    else
	leftEdgeOffset = -1;
    bounds.origin.x = macDraw->xOff + x + leftEdgeOffset;
    bounds.size.width = width - leftEdgeOffset;

    (void) HIThemeDrawButton(&bounds, &info, context,
	kHIThemeOrientationNormal, NULL);

    TreeMacOSX_ReleaseContext(tree, &dc);
    CFRelease(boundsRgn);

    return TCL_OK;
}

/* List headers are a fixed height on Aqua */
int
TreeTheme_GetHeaderFixedHeight(
    TreeCtrl *tree,
    int *heightPtr
    )
{
    SInt32 metric;

    GetThemeMetric(kThemeMetricListHeaderHeight, &metric);
    *heightPtr = metric;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_GetHeaderContentMargins --
 *
 *	Returns the padding inside the column header borders where
 *	text etc may be displayed.
 *
 * Results:
 *	TCL_OK if 'bounds' was set, TCL_ERROR otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_GetHeaderContentMargins(
    TreeCtrl *tree,		/* Widget info. */
    int state,			/* COLUMN_STATE_xxx flags. */
    int arrow,			/* COLUMN_ARROW_xxx flags. */
    int bounds[4]		/* Returned left-top-right-bottom padding. */
    )
{
#if 1
    /* HIThemeGetButtonContentBounds() returns a weird result.
     * The output rectangle starts 12 pixels from the *left* of the
     * input rectangle whereas I expected it end 12 from the right - where
     * the sort arrow is displayed.  Also, it returns the same result
     * regardless of the arrow being displayed or not.
     *
     * Eyeballing the sort arrow shows a 5-pixel gap on the right and a
     * 7-pixel arrow. */
    bounds[0] = 0;
    bounds[1] = 0;
    bounds[2] = (arrow != COLUMN_ARROW_NONE) ? 12 : 0;
    bounds[3] = 0;
#else
    CGRect inBounds, outBounds;
    HIThemeButtonDrawInfo info;
    SInt32 metric;

    inBounds.origin.x = 0;
    inBounds.origin.y = 0;
    inBounds.size.width = 100;
    GetThemeMetric(kThemeMetricListHeaderHeight, &metric);
    inBounds.size.height = metric;

    info = GetThemeButtonDrawInfo(tree, state, arrow);

    (void) HIThemeGetButtonContentBounds(
	&inBounds,
	&info,
	&outBounds);

    bounds[0] = CGRectGetMinX(outBounds) - CGRectGetMinX(inBounds);
    bounds[1] = CGRectGetMinY(outBounds) - CGRectGetMinY(inBounds);
    bounds[2] = CGRectGetMaxX(inBounds) - CGRectGetMaxX(outBounds);
    bounds[3] = CGRectGetMaxY(inBounds) - CGRectGetMaxY(outBounds);
#endif
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_DrawHeaderArrow --
 *
 *	Draws the sort arrow in a column header.
 *
 * Results:
 *	TCL_OK if drawing occurred, TCL_ERROR if the X11 fallback
 *	should be used.
 *
 * Side effects:
 *	Drawing.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_DrawHeaderArrow(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    int state,			/* COLUMN_STATE_xxx flags. */
    int up,			/* TRUE if up arrow, FALSE otherwise. */
    int x, int y,		/* Bounds of arrow.  Width and */
    int width, int height	/* height are the same as that returned */
				/* by TreeTheme_GetArrowSize(). */ 
    )
{
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_DrawButton --
 *
 *	Draws a single expand/collapse item button.
 *
 * Results:
 *	TCL_OK if drawing occurred, TCL_ERROR if the X11 fallback
 *	should be used.
 *
 * Side effects:
 *	Drawing.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_DrawButton(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeItem item,		/* Needed for animating. */
    int state,			/* STATE_xxx | BUTTON_STATE_xxx flags. */
    int x, int y,		/* Bounds of the button.  Width and height */
    int width, int height	/* are the same as that returned by */
				/* TreeTheme_GetButtonSize(). */
    )
{
    int open = (state & STATE_OPEN) != 0;
    int pressed = (state & BUTTON_STATE_PRESSED) != 0;
    MacDrawable *macDraw = (MacDrawable *) td.drawable;
    CGRect bounds;
    HIThemeButtonDrawInfo info;
    MacContextSetup dc;
    CGContextRef context;
    HIShapeRef clipRgn;
    TreeRectangle tr;

    if (!(macDraw->flags & TK_IS_PIXMAP))
	return TCL_ERROR;

    bounds.origin.x = macDraw->xOff + x;
    bounds.origin.y = macDraw->yOff + y;
    bounds.size.width = width;
    bounds.size.height = height;

    info.version = 0;
    info.state = pressed ? kThemeStatePressed : kThemeStateActive;
    info.kind = kThemeDisclosureButton;
    info.value = open ? kThemeDisclosureDown : kThemeDisclosureRight;
    info.adornment = kThemeAdornmentDrawIndicatorOnly;

    tr.x = x, tr.y = y, tr.width = width, tr.height = height;
    context = TreeMacOSX_GetContext(tree, td.drawable, tr, &dc);
    if (context == NULL)
	return TCL_ERROR;

    /* This is how the rotated button is drawn. */
    if (item == tree->themeData->animButtonItem) {
	int angle = tree->themeData->animButtonAngle * (open ? -1 : 1);
	CGContextTranslateCTM(context, x + width/2.0, y + height/2.0);
	CGContextRotateCTM(context, radians(angle));
	CGContextTranslateCTM(context, -(x + width/2.0), -(y + height/2.0));
	info.state = kThemeStatePressed;
    }

    /* Set the clipping region */
    clipRgn = HIShapeCreateWithRect(&bounds);
    HIShapeReplacePathInCGContext(clipRgn, context);
    CGContextEOClip(context);

    (void) HIThemeDrawButton(&bounds, &info, context,
	kHIThemeOrientationNormal, NULL);

    TreeMacOSX_ReleaseContext(tree, &dc);
    CFRelease(clipRgn);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_GetButtonSize --
 *
 *	Returns the width and height of an expand/collapse item button.
 *
 * Results:
 *	TCL_OK if *widthPtr and *heightPtr were set, TCL_ERROR
 *	if themed buttons can't be drawn.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_GetButtonSize(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Needed on MS Windows. */
    int open,			/* TRUE if expanded button. */
    int *widthPtr,		/* Returned width of button. */
    int *heightPtr		/* Returned height of button. */
    )
{
    SInt32 metric;

    (void) GetThemeMetric(
	kThemeMetricDisclosureTriangleWidth,
	&metric);
    *widthPtr = metric;

    (void) GetThemeMetric(
	kThemeMetricDisclosureTriangleHeight,
	&metric);
    *heightPtr = metric;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_GetArrowSize --
 *
 *	Returns the width and height of a column header sort arrow.
 *
 * Results:
 *	TCL_OK if *widthPtr and *heightPtr were set, TCL_ERROR
 *	if themed sort arrows can't be drawn.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_GetArrowSize(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Needed on MS Windows. */
    int up,			/* TRUE if up arrow. */
    int *widthPtr,		/* Returned width of arrow. */
    int *heightPtr		/* Returned height of arrow. */
    )
{
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_SetBorders --
 *
 *	Sets the TreeCtrl.inset pad values according to the needs of
 *	the system theme.
 *
 * Results:
 *	TCL_OK if the inset was set, TCL_ERROR if the -highlightthickness
 *	and -borderwidth values should be used.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_SetBorders(
    TreeCtrl *tree		/* Widget info. */
    )
{
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_DrawBorders --
 *
 *	Draws themed borders around the edges of the treectrl.
 *
 * Results:
 *	TCL_OK if drawing occurred, TCL_ERROR if the Tk focus rectangle
 *	and 3D border should be drawn.
 *
 * Side effects:
 *	Drawing.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_DrawBorders(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable		/* Where to draw. */
    )
{
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_GetColumnTextColor --
 *
 *	Returns the text fill color to display a column title with.
 *
 * Results:
 *	TCL_OK if the *colorPtrPtr was set, TCL_ERROR if a non-theme
 *	color should be used.
 *
 * Side effects:
 *	May allocate a new XColor.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_GetColumnTextColor(
    TreeCtrl *tree,		/* Widget info. */
    int columnState,		/* COLUMN_STATE_xxx flags. */
    XColor **colorPtrPtr	/* Returned text color. */
    )
{
    return TCL_ERROR;
}

#define ANIM_BUTTON_INTERVAL 50 /* guestimate */

static void
AnimButtonTimerProc(
    ClientData clientData
    )
{
    TreeCtrl *tree = clientData;
    TreeItem item = tree->themeData->animButtonItem;

    if (tree->themeData->animButtonAngle >= 90) {
	tree->themeData->animButtonTimer = NULL;
	tree->themeData->animButtonItem = NULL;
	TreeItem_OpenClose(tree, item, -1);
#ifdef SELECTION_VISIBLE
	Tree_DeselectHidden(tree);
#endif
    } else {
	int interval = ANIM_BUTTON_INTERVAL;
	tree->themeData->animButtonAngle += 30;
	if (tree->themeData->animButtonAngle >= 90) {
	    interval = 10;
	}
	tree->themeData->animButtonTimer = Tcl_CreateTimerHandler(
	    interval, AnimButtonTimerProc, tree);
	Tree_InvalidateItemDInfo(tree, tree->columnTree, item,
	    NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_AnimateButtonStart --
 *
 *	Starts an expand/collapse item button animating from open to
 *	closed or vice versa.
 *
 * Results:
 *	TCL_OK.
 *
 * Side effects:
 *	May create a new Tcl_TimerToken.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_AnimateButtonStart(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* The item whose button should animate. */
    )
{
    if (tree->themeData->animButtonTimer != NULL)
	Tcl_DeleteTimerHandler(tree->themeData->animButtonTimer);

    tree->themeData->animButtonItem = item;
    tree->themeData->animButtonTimer = Tcl_CreateTimerHandler(
	ANIM_BUTTON_INTERVAL, AnimButtonTimerProc, tree);
    tree->themeData->animButtonAngle = 30;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_ItemDeleted --
 *
 *	Cancels any item-button animation in progress.
 *
 * Results:
 *	TCL_OK.
 *
 * Side effects:
 *	May delete a TCL_TimerToken.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_ItemDeleted(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item being deleted. */
    )
{
    if (item != tree->themeData->animButtonItem)
	return TCL_OK;
    if (tree->themeData->animButtonTimer != NULL) {
	Tcl_DeleteTimerHandler(tree->themeData->animButtonTimer);
	tree->themeData->animButtonTimer = NULL;
	tree->themeData->animButtonItem = NULL;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_Relayout --
 *
 *	This gets called when certain config options change and when
 *	the size of the widget changes.
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
TreeTheme_Relayout(
    TreeCtrl *tree		/* Widget info. */
    )
{
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_IsDesktopComposited --
 *
 *	Determine if the OS windowing system is composited AKA
 *	double-buffered.
 *
 * Results:
 *	FALSE FALSE FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_IsDesktopComposited(
    TreeCtrl *tree		/* Widget info. */
    )
{
#if 1 /* with gradient marquee, can't draw into window, need a pixmap */
    return FALSE;
#else
    return TRUE;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_ThemeChanged --
 *
 *	Called after the system theme changes.
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
TreeTheme_ThemeChanged(
    TreeCtrl *tree		/* Widget info. */
    )
{
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_Init --
 *
 *	Performs theme-related initialization when a treectrl is
 *	created.
 *
 * Results:
 *	TCL_OK or TCL_ERROR, but result is ignored.
 *
 * Side effects:
 *	Depends on the platform.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_Init(
    TreeCtrl *tree		/* Widget info. */
    )
{
    tree->themeData = (TreeThemeData) ckalloc(sizeof(struct TreeThemeData_));
    tree->themeData->animButtonTimer = NULL;
    tree->themeData->animButtonAngle = 0;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_Free --
 *
 *	Performs theme-related cleanup a when a treectrl is destroyed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the platform.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_Free(
    TreeCtrl *tree		/* Widget info. */
    )
{
    if (tree->themeData != NULL) {
	ckfree((char *) tree->themeData);
	tree->themeData = NULL;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_InitInterp --
 *
 *	Performs theme-related initialization when the TkTreeCtrl
 *	package is loaded into an interpreter.
 *
 * Results:
 *	TCL_OK or TCL_ERROR, but result is ignored.
 *
 * Side effects:
 *	Depends on the platform.
 *
 *----------------------------------------------------------------------
 */

int
TreeTheme_InitInterp(
    Tcl_Interp *interp		/* Interp that loaded TkTreeCtrl pkg. */
    )
{
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeTheme_SetOptionDefault --
 *
 *	Sets the default value for an option.
 *
 * Results:
 *	Sets the defValue field if it wasn't done already.
 *
 * Side effects:
 *	Changes an existing option table.
 *
 *----------------------------------------------------------------------
 */

void
TreeTheme_SetOptionDefault(
    Tk_OptionSpec *specPtr
    )
{
#ifdef TREECTRL_DEBUG
    if (specPtr == NULL)
	panic("TreeTheme_SetOptionDefault specPtr == NULL");
#endif

    /* Only set the default value once per-application. */
    if (specPtr->defValue != NULL)
	return;

    if (!strcmp(specPtr->optionName, "-buttontracking"))
	specPtr->defValue = "1";
    else if (!strcmp(specPtr->optionName, "-showlines"))
	specPtr->defValue = "0";

#ifdef TREECTRL_DEBUG
    else
	panic("TreeTheme_SetOptionDefault unhandled option \"%s\"",
	    specPtr->optionName ? specPtr->optionName : "NULL");
#endif
}

int 
TreeThemeCmd(
    TreeCtrl *tree,		/* Widget info. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    Tcl_Interp *interp = tree->interp;
    static CONST char *commandName[] = {
	"platform", (char *) NULL
    };
    enum {
	COMMAND_PLATFORM
    };
    int index;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandName, "command", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
	/* T theme platform */
	case COMMAND_PLATFORM: {
	    char *platform = "aqua";
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(platform, -1));
	    break;
	}
    }

    return TCL_OK;
}

/*** Gradients ***/

#if MAC_TK_COCOA /*(TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION >= 6)*/

/*
 * THIS WON'T WORK FOR DRAWING IN A WINDOW.
 */
CGContextRef
TreeMacOSX_GetContext(
    TreeCtrl *tree,
    Drawable d,
    TreeRectangle tr,
    MacContextSetup *dc
    )
{
    MacDrawable *macDraw = (MacDrawable *) d;
    CGAffineTransform t = { .a = 1, .b = 0, .c = 0, .d = -1, .tx = 0,
	.ty = macDraw->size.height};

    if ((macDraw->flags & TK_IS_PIXMAP) && !macDraw->context) {
	GC gc = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);
	XFillRectangle(tree->display, d, gc, tr.x, tr.y, tr.width, tr.height);
    }

    dc->context = macDraw->context;
    if (dc->context) {
	CGContextSaveGState(dc->context);
	CGContextConcatCTM(dc->context, t);
    }
    
    return dc->context;
}

void
TreeMacOSX_ReleaseContext(
    TreeCtrl *tree,
    MacContextSetup *dc
    )
{
    if (dc->context != NULL) {
	CGContextSynchronize(dc->context); /* does nothing, expects window context */
	CGContextRestoreGState(dc->context);
    }
}

#endif /* Tk 8.6 */

#if MAC_TK_CARBON /*(TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION < 6)*/

/*
 * THIS WON'T WORK FOR DRAWING IN A WINDOW.
 */
CGContextRef
TreeMacOSX_GetContext(
    TreeCtrl *tree,
    Drawable d,
    TreeRectangle tr,
    MacContextSetup *dc
    )
{
    MacDrawable *macDraw = (MacDrawable *) d;
    CGAffineTransform t = { .a = 1, .b = 0, .c = 0, .d = -1, .tx = 0,
	.ty = macDraw->size.height};

    dc->port = TkMacOSXGetDrawablePort(d);
    dc->context = NULL;
    dc->portChanged = False;
    if (dc->port) {
	dc->portChanged = QDSwapPort(dc->port, &dc->savePort);
	if (QDBeginCGContext(dc->port, &dc->context) == noErr) {
	    SyncCGContextOriginWithPort(dc->context, dc->port);
	    CGContextSaveGState(dc->context);
	    CGContextConcatCTM(dc->context, t);
	}
    }
    
    return dc->context;
}

void
TreeMacOSX_ReleaseContext(
    TreeCtrl *tree,
    MacContextSetup *dc
    )
{
    if (dc->context != NULL) {
	CGContextSynchronize(dc->context); /* does nothing, expects window context */
	CGContextRestoreGState(dc->context);
	if (dc->port) {
	    QDEndCGContext(dc->port, &dc->context);
	}
    }
    if (dc->portChanged) {
	QDSwapPort(dc->savePort, NULL);
    }
}

#endif /* Tk 8.4 and 8.5 */

/*
 *----------------------------------------------------------------------
 *
 * Tree_HasNativeGradients --
 *
 *	Determine if this platform supports gradients natively.
 *
 * Results:
 *	1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tree_HasNativeGradients(
    TreeCtrl *tree)
{
    return 1;
}

/* Copy-and-paste from TkPath */

const float kValidDomain[2] = {0, 1};
const float kValidRange[8] = {0, 1, 0, 1, 0, 1, 0, 1};

/* Seems to work for both Endians. */
#define BlueFloatFromXColorPtr(xc)   (float) ((((xc)->pixel >> 0)  & 0xFF)) / 255.0
#define GreenFloatFromXColorPtr(xc)  (float) ((((xc)->pixel >> 8)  & 0xFF)) / 255.0
#define RedFloatFromXColorPtr(xc)    (float) ((((xc)->pixel >> 16) & 0xFF)) / 255.0

#if 0
typedef struct ShadeData {
    int nstops;
    unsigned short red[50], green[50], blue[50];
    float offset[50];
    float opacity[50];
} ShadeData;

static void
ShadeEvaluate(
    void *info,
    const float *in,
    float *out)
{
    ShadeData *data = info;
    int nstops = data->nstops;
    int iStop1, iStop2;
    float offset1, offset2;
    float opacity1, opacity2;
    int i = 0;
    float par = *in;
    float f1, f2;

    /* Find the two stops for this point. Tricky! */
    while ((i < nstops) && (data->offset[i] < par)) {
        i++;
    }
    if (i == 0) {
        /* First stop > 0. */
        iStop1 = iStop2 = 0;
    } else if (i == nstops) {
        /* We have stepped beyond the last stop; step back! */
        iStop1 = iStop2 = nstops - 1;
    } else {
        iStop1 = i-1, iStop2 = i;
    }
    opacity1 = data->opacity[iStop1];
    opacity2 = data->opacity[iStop2];
    offset1 = data->offset[iStop1];
    offset2 = data->offset[iStop2];
    /* Interpolate between the two stops. 
     * "If two gradient stops have the same offset value, 
     * then the latter gradient stop controls the color value at the 
     * overlap point."
     */
    if (fabs(offset2 - offset1) < 1e-6) {
        *out++ = data->red[iStop2];
        *out++ = data->green[iStop2];
        *out++ = data->blue[iStop2]; 
        *out++ = opacity2;
    } else {
    	int range, increment;
    	float factor = (par - offset1)/(offset2 - offset1);
   
	range = (data->red[iStop2] - data->red[iStop1]);
	increment = (int)(range * factor);
	*out++ = CLAMP(data->red[iStop1] + increment,0,USHRT_MAX)/((float)USHRT_MAX);

	range = (data->green[iStop2] - data->green[iStop1]);
	increment = (int)(range * factor);
	*out++ = CLAMP(data->green[iStop1] + increment,0,USHRT_MAX)/((float)USHRT_MAX);

	range = (data->blue[iStop2] - data->blue[iStop1]);
	increment = (int)(range * factor);
	*out++ = CLAMP(data->blue[iStop1] + increment,0,USHRT_MAX)/((float)USHRT_MAX);

        *out++ = 1.0f /*f1 * opacity1 + f2 * opacity2*/;
    }
}
#elif 1
typedef struct ShadeData {
    int nstops;
    float red[50], green[50], blue[50];
    float offset[50];
    float opacity[50];
} ShadeData;

static void
ShadeEvaluate(
    void *info,
    const float *in,
    float *out)
{
    ShadeData *data = info;
    int nstops = data->nstops;
    int iStop1, iStop2;
    float offset1, offset2;
    float opacity1, opacity2;
    int i = 0;
    float par = *in;
    float f1, f2;

    /* For a repeating pattern, clamp *in to 0-1 */
    par = par - floor(par);
    
    /* Find the two stops for this point. Tricky! */
    while ((i < nstops) && (data->offset[i] < par)) {
        i++;
    }
    if (i == 0) {
        /* First stop > 0. */
        iStop1 = iStop2 = 0;
    } else if (i == nstops) {
        /* We have stepped beyond the last stop; step back! */
        iStop1 = iStop2 = nstops - 1;
    } else {
        iStop1 = i-1, iStop2 = i;
    }
    opacity1 = data->opacity[iStop1];
    opacity2 = data->opacity[iStop2];
    offset1 = data->offset[iStop1];
    offset2 = data->offset[iStop2];
    /* Interpolate between the two stops. 
     * "If two gradient stops have the same offset value, 
     * then the latter gradient stop controls the color value at the 
     * overlap point."
     */
    if (fabs(offset2 - offset1) < 1e-6) {
        *out++ = data->red[iStop2];
        *out++ = data->green[iStop2];
        *out++ = data->blue[iStop2]; 
        *out++ = opacity2;
    } else {
        f1 = (offset2 - par)/(offset2 - offset1);
        f2 = (par - offset1)/(offset2 - offset1);
        *out++ = f1 * data->red[iStop1] + f2 * data->red[iStop2];
        *out++ = f1 * data->green[iStop1] + f2 * data->green[iStop2];
        *out++ = f1 * data->blue[iStop1] + f2 * data->blue[iStop2];
        *out++ = f1 * opacity1 + f2 * opacity2;
    }
}
#else
static void
ShadeEvaluate(
    void *info,
    const float *in,
    float *out)
{
    TreeGradient gradient = info;
    GradientStopArray *stopArrPtr = gradient->stopArrPtr;
    GradientStop **stopPtrPtr = stopArrPtr->stops;
    GradientStop *stop1 = NULL, *stop2 = NULL;
    int nstops = stopArrPtr->nstops;
    int i = 0;
    float par = *in;
    float f1, f2;

    /* Find the two stops for this point. Tricky! */
    while ((i < nstops) && ((*stopPtrPtr)->offset < par)) {
        stopPtrPtr++, i++;
    }
    if (i == 0) {
        /* First stop > 0. */
        stop1 = *stopPtrPtr;
        stop2 = stop1;
    } else if (i == nstops) {
        /* We have stepped beyond the last stop; step back! */
        stop1 = *(stopPtrPtr - 1);
        stop2 = stop1;
    } else {
        stop1 = *(stopPtrPtr - 1);
        stop2 = *stopPtrPtr;
    }
    /* Interpolate between the two stops. 
     * "If two gradient stops have the same offset value, 
     * then the latter gradient stop controls the color value at the 
     * overlap point."
     */
    if (fabs(stop2->offset - stop1->offset) < 1e-6) {
        *out++ = RedFloatFromXColorPtr(stop2->color);
        *out++ = GreenFloatFromXColorPtr(stop2->color);
        *out++ = BlueFloatFromXColorPtr(stop2->color); 
        *out++ = stop2->opacity;
    } else {
        f1 = (stop2->offset - par)/(stop2->offset - stop1->offset);
        f2 = (par - stop1->offset)/(stop2->offset - stop1->offset);
        *out++ = f1 * RedFloatFromXColorPtr(stop1->color) + 
                f2 * RedFloatFromXColorPtr(stop2->color);
        *out++ = f1 * GreenFloatFromXColorPtr(stop1->color) + 
                f2 * GreenFloatFromXColorPtr(stop2->color);
        *out++ = f1 * BlueFloatFromXColorPtr(stop1->color) + 
                f2 * BlueFloatFromXColorPtr(stop2->color);
        *out++ = f1 * stop1->opacity + f2 * stop2->opacity;
    }
}
#endif
static void
ShadeRelease(void *info)
{
    /* Not sure if anything to do here. */
}

#if 0
/* Get the colorspace used by the monitor. */
/* FIXME: 8.4+ only */
CGColorSpaceRef CreateSystemColorSpace() {
    CMProfileRef sysprof = NULL;
    CGColorSpaceRef dispColorSpace = NULL;
    if (CMGetSystemProfile(&sysprof) == noErr) {
	dispColorSpace = CGColorSpaceCreateWithPlatformColorSpace(sysprof);
	CMCloseProfile(sysprof);
    }
    return dispColorSpace;
}
#endif

typedef struct {
    CGFunctionRef function;
    CGColorSpaceRef colorSpaceRef;
    CGShadingRef shading;
    ShadeData data;
} MacShading;

static CGShadingRef
MakeLinearGradientShading(
    CGContextRef context,
    TreeGradient gradient,	/* Gradient token. */
    TreeRectangle trBrush,	/* Brush bounds. */
    TreeRectangle tr,		/* Rectangle to paint. */
    MacShading *ms
    )
{
    int i;
    CGFunctionCallbacks callbacks;
    CGPoint start, end;
    CGFloat input_value_range[2] = {0.f, 1.f};

    ms->data.nstops = gradient->stopArrPtr->nstops;
    for (i = 0; i < gradient->stopArrPtr->nstops; i++) {
	GradientStop *stop = gradient->stopArrPtr->stops[i];
#if 0
        ms->data.red[i] = stop->color->red;
        ms->data.green[i] = stop->color->green;
        ms->data.blue[i] = stop->color->blue;
#else
        ms->data.red[i] = RedFloatFromXColorPtr(stop->color);
        ms->data.green[i] = GreenFloatFromXColorPtr(stop->color);
        ms->data.blue[i] = BlueFloatFromXColorPtr(stop->color);
#endif
        ms->data.offset[i] = stop->offset;
        ms->data.opacity[i] = stop->opacity;
    }

/*    colorSpaceRef = CGColorSpaceCreateDeviceRGB();*/
/*    colorSpaceRef = CreateSystemColorSpace();*/
/*    colorSpaceRef = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);*/
    ms->colorSpaceRef = CGBitmapContextGetColorSpace(context);
    CGColorSpaceRetain(ms->colorSpaceRef);

    if (gradient->vertical) {
	start = CGPointMake(trBrush.x, trBrush.y);
	end = CGPointMake(trBrush.x, trBrush.y + trBrush.height);
    } else {
	start = CGPointMake(trBrush.x, trBrush.y);
	end = CGPointMake(trBrush.x + trBrush.width, trBrush.y);
    }

    /* Repeating pattern */
    if (gradient->vertical) {
	CGFloat dy = trBrush.height;
	input_value_range[0] = MIN((tr.y - trBrush.y) / dy, 0.0f);
	input_value_range[1] = MAX(((tr.y + tr.height) - trBrush.y) / dy, 1.0f);
	end.y = start.y + dy * input_value_range[1];
	start.y += dy * input_value_range[0];
    } else {
	CGFloat dx = trBrush.width;
	input_value_range[0] = MIN((tr.x - trBrush.x) / dx, 0.0f);
	input_value_range[1] = MAX(((tr.x + tr.width) - trBrush.x) / dx, 1.0f);
	end.x = start.x + dx * input_value_range[1];
	start.x += dx * input_value_range[0];
    }
    
    callbacks.version = 0;
    callbacks.evaluate = ShadeEvaluate;
    callbacks.releaseInfo = ShadeRelease;
    ms->function = CGFunctionCreate((void *) &ms->data,
	1, input_value_range/*kValidDomain*/,
	4, kValidRange,
	&callbacks);

    ms->shading = CGShadingCreateAxial(ms->colorSpaceRef, start, end,
	ms->function, 1, 1);

    return ms->shading;
}

static void
ReleaseLinearGradientShading(
    MacShading *ms
    )
{
    CGShadingRelease(ms->shading);
    CGFunctionRelease(ms->function);
    CGColorSpaceRelease(ms->colorSpaceRef);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeGradient_FillRect --
 *
 *	Paint a rectangle with a gradient.
 *
 * Results:
 *	If the gradient has <2 stops then nothing is drawn.
 *
 * Side effects:
 *	Drawing.
 *
 *----------------------------------------------------------------------
 */

void
TreeGradient_FillRect(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    TreeGradient gradient,	/* Gradient token. */
    TreeRectangle trBrush,	/* Brush bounds. */
    TreeRectangle tr		/* Rectangle to paint. */
    )
{
    MacDrawable *macDraw = (MacDrawable *) td.drawable;
    MacContextSetup dc;
    MacShading ms;
    CGContextRef context;
    CGShadingRef shading;
    CGRect r;

    if (gradient->stopArrPtr == NULL || gradient->stopArrPtr->nstops < 2)
	return;

    /* Draw nothing if the brush is zero-sized. */
    if (trBrush.width <= 0 || trBrush.height <= 0)
	return;

    if (!(macDraw->flags & TK_IS_PIXMAP) || !tree->nativeGradients) {
	TreeGradient_FillRectX11(tree, td, clip, gradient, trBrush, tr);
	return;
    }

    context = TreeMacOSX_GetContext(tree, td.drawable, tr, &dc);
    if (context == NULL) {
	TreeGradient_FillRectX11(tree, td, clip, gradient, trBrush, tr);
	return;
    }

    shading = MakeLinearGradientShading(context, gradient, trBrush, tr, &ms);
    if (shading) {

	/* Must clip to the area to be painted otherwise the entire context
	* is filled with the gradient. */
	CGContextBeginPath(context);
	r = CGRectMake(tr.x, tr.y, tr.width, tr.height);
	CGContextAddRect(context, r);
	CGContextClip(context);

	CGContextDrawShading(context, shading);

	ReleaseLinearGradientShading(&ms);
    }

    TreeMacOSX_ReleaseContext(tree, &dc);
}

static CGMutablePathRef
MakeRectPath_OutlineFilled(
    TreeRectangle tr,		/* Where to draw. */
    int outlineWidth,		/* Thickness of the outline. */
    int open			/* RECT_OPEN_x flags */
    )
{
    CGFloat x = tr.x, y = tr.y, w = tr.width, h = tr.height;
    int drawW = (open & RECT_OPEN_W) == 0;
    int drawN = (open & RECT_OPEN_N) == 0;
    int drawE = (open & RECT_OPEN_E) == 0;
    int drawS = (open & RECT_OPEN_S) == 0;
    CGMutablePathRef p = CGPathCreateMutable();
    CGRect r;

    if (drawN) {
	r = CGRectMake(x, y, w, outlineWidth);
	CGPathAddRect(p, NULL, r);
    }
    if (drawE) {
	r = CGRectMake(x + w - outlineWidth, y, outlineWidth, h);
	CGPathAddRect(p, NULL, r);
    }
    if (drawS) {
	r = CGRectMake(x, y + h - outlineWidth, w, outlineWidth);
	CGPathAddRect(p, NULL, r);
    }
    if (drawW) {
	r = CGRectMake(x, y, outlineWidth, h);
	CGPathAddRect(p, NULL, r);
    }

    return p;
}

void
TreeGradient_DrawRect(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    TreeGradient gradient,	/* Gradient. */
    TreeRectangle trBrush,	/* Brush bounds. */
    TreeRectangle tr,		/* Rectangle to draw. */
    int outlineWidth,		/* Width of outline. */
    int open			/* RECT_OPEN_x flags */
    )
{
    MacDrawable *macDraw = (MacDrawable *) td.drawable;
    MacContextSetup dc;
    MacShading ms;
    CGContextRef context;
    CGShadingRef shading;
    CGMutablePathRef p;

    if (!(macDraw->flags & TK_IS_PIXMAP) || !tree->nativeGradients) {
errorExit:
	TreeGradient_DrawRectX11(tree, td, clip, gradient, trBrush, tr,
	    outlineWidth, open);
	return;
    }

    context = TreeMacOSX_GetContext(tree, td.drawable, tr, &dc);
    if (context == NULL) {
	goto errorExit;
    }

    shading = MakeLinearGradientShading(context, gradient, trBrush, tr, &ms);
    if (shading) {

	p = MakeRectPath_OutlineFilled(tr, outlineWidth, open);
	if (p) {
	    CGContextAddPath(context, p);
	    CGPathRelease(p);

	    CGContextClip(context);

	    CGContextDrawShading(context, shading);
	}

	ReleaseLinearGradientShading(&ms);
    }

    TreeMacOSX_ReleaseContext(tree, &dc);
}

static void
AddArcToPath(
    CGMutablePathRef p,
    CGAffineTransform *t,
    CGAffineTransform *it,
    CGFloat x, CGFloat y,
    CGFloat radius,
    int startAngle,
    int endAngle
    )
{
    if (t) {
	CGPoint c = CGPointMake(x, y);
	c = CGPointApplyAffineTransform(c, *it);
	CGPathAddArc(p, t, c.x, c.y, radius, radians(startAngle), radians(endAngle), 0);
    } else {
	CGPathAddArc(p, NULL, x, y, radius, radians(startAngle), radians(endAngle), 0);
    }
}
    
static CGMutablePathRef
MakeRoundRectPath_Fill(
    TreeRectangle tr,		/* Where to draw. */
    int rx, int ry,		/* Corner radius */
    int open			/* RECT_OPEN_x flags */
    )
{
    CGFloat x = tr.x, y = tr.y, width = tr.width, height = tr.height;
    int drawW = (open & RECT_OPEN_W) == 0;
    int drawN = (open & RECT_OPEN_N) == 0;
    int drawE = (open & RECT_OPEN_E) == 0;
    int drawS = (open & RECT_OPEN_S) == 0;
    CGMutablePathRef p = CGPathCreateMutable();
    CGAffineTransform t, it, *tp, *itp;

    if (rx == ry) {
	itp = tp = NULL;
    } else {
	t = CGAffineTransformMakeScale(1.0, ry / (float)rx);
	it = CGAffineTransformInvert(t);
	tp = &t;
	itp = &it;
    }

    /* Simple case: draw all 4 corners and 4 edges */
    if (!open) {
	AddArcToPath(p, tp, itp, x + rx, y + ry, rx, 180, 270); /* top-left */
	AddArcToPath(p, tp, itp, x + width - rx, y + ry, rx, 270, 0); /* top-right */
	AddArcToPath(p, tp, itp, x + width - rx, y + height - ry, rx, 0, 90); /* bottom-right */
	AddArcToPath(p, tp, itp, x + rx, y + height - ry, rx, 90, 180); /* bottom-left */

    /* Complicated case: some edges are "open" */
    } else {
	CGPoint start[4], end[4]; /* start and end points of line segments*/
	start[0] = CGPointMake(x, y);
	end[3] = start[0];
	if (drawW && drawN) {
	    start[0].x += rx;
	    end[3].y += ry;
	}
	end[0] = CGPointMake(x + width, y);
	start[1]= end[0];
	if (drawE && drawN) {
	    end[0].x -= rx;
	    start[1].y += ry;
	}
	end[1] = CGPointMake(x + width, y + height);
	start[2] = end[1];
	if (drawE && drawS) {
	    end[1].y -= ry;
	    start[2].x -= rx;
	}
	end[2] = CGPointMake(x, y + height);
	start[3] = end[2];
	if (drawW && drawS) {
	    end[2].x += rx;
	    start[3].y -= ry;
	}

	if (drawW && drawN) {
	    AddArcToPath(p, tp, itp, x + rx, y + ry, rx, 180, 270); /* top-left */
	} else {
	    CGPathMoveToPoint(p, NULL, start[0].x, start[0].y);
	}
	CGPathAddLineToPoint(p, NULL, end[0].x, end[0].y);
	if (drawE && drawN)
	    AddArcToPath(p, tp, itp, x + width - rx, y + ry, rx, 270, 0); /* top-right */
	/*else
	    CGPathMoveToPoint(p, NULL, start[1].x, start[1].y);*/
	CGPathAddLineToPoint(p, NULL, end[1].x, end[1].y);
	if (drawE && drawS)
	    AddArcToPath(p, tp, itp, x + width - rx, y + height - ry, rx, 0, 90); /* bottom-right */
	/*else
	    CGPathMoveToPoint(p, NULL, start[2].x, start[2].y);*/
	CGPathAddLineToPoint(p, NULL, end[2].x, end[2].y);
	if (drawW && drawS)
	    AddArcToPath(p, tp, itp, x + rx, y + height - ry, rx, 90, 180); /* bottom-left */
	/*else
	    CGPathMoveToPoint(p, NULL, start[3].x, start[3].y);*/
	CGPathAddLineToPoint(p, NULL, end[3].x, end[3].y);
    }

    return p;
}

void
TreeGradient_FillRoundRect(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    TreeGradient gradient,	/* Gradient token. */
    TreeRectangle trBrush,	/* Brush bounds. */
    TreeRectangle tr,		/* Where to draw. */
    int rx, int ry,		/* Corner radius */
    int open			/* RECT_OPEN_x flags */
    )
{
    MacDrawable *macDraw = (MacDrawable *) td.drawable;
    MacContextSetup dc;
    MacShading ms;
    CGContextRef context;
    CGShadingRef shading;

    if (gradient->stopArrPtr == NULL || gradient->stopArrPtr->nstops < 2)
	return;

    /* Draw nothing if the brush is zero-sized. */
    if (trBrush.width <= 0 || trBrush.height <= 0)
	return;

    if (!(macDraw->flags & TK_IS_PIXMAP) || !tree->nativeGradients) {
	TreeGradient_FillRoundRectX11(tree, td, clip, gradient, trBrush, tr,
	    rx, ry, open);
	return;
    }

    context = TreeMacOSX_GetContext(tree, td.drawable, tr, &dc);
    if (context == NULL) {
	TreeGradient_FillRoundRectX11(tree, td, clip, gradient, trBrush, tr,
	    rx, ry, open);
	return;
    }

    shading = MakeLinearGradientShading(context, gradient, trBrush, tr, &ms);
    if (shading) {

	CGContextBeginPath(context);
	if (rx == ry && !open) {
	    CGContextAddArc(context, tr.x + rx, tr.y + ry, rx, radians(180), radians(270), 0); /* top-left */
	    CGContextAddArc(context, tr.x + tr.width - rx, tr.y + ry, rx, radians(270), radians(0), 0); /* top-right */
	    CGContextAddArc(context, tr.x + tr.width - rx, tr.y + tr.height - ry, rx, radians(0), radians(90), 0); /* bottom-right */
	    CGContextAddArc(context, tr.x + rx, tr.y + tr.height - ry, rx, radians(90), radians(180), 0); /* bottom-left */
	} else {
	    CGMutablePathRef p = MakeRoundRectPath_Fill(tr, rx, ry, open);
	    if (p) {
		CGContextAddPath(context, p);
		CGPathRelease(p);
	    }
	}
	/* Must clip to the area to be painted otherwise the entire context
	* is filled with the gradient. */
	CGContextClip(context);

	CGContextDrawShading(context, shading);

	ReleaseLinearGradientShading(&ms);
    }

    TreeMacOSX_ReleaseContext(tree, &dc);
}

static CGMutablePathRef
MakeRoundRectPath_Stroke(
    TreeRectangle tr,		/* Where to draw. */
    int outlineWidth,		/* Thickness of the outline. */
    int rx, int ry,		/* Corner radius */
    int open			/* RECT_OPEN_x flags */
    )
{
    CGFloat x = tr.x, y = tr.y, width = tr.width, height = tr.height;
    int drawW = (open & RECT_OPEN_W) == 0;
    int drawN = (open & RECT_OPEN_N) == 0;
    int drawE = (open & RECT_OPEN_E) == 0;
    int drawS = (open & RECT_OPEN_S) == 0;
    CGMutablePathRef p = CGPathCreateMutable();
    CGAffineTransform t, it, *tp, *itp;

    if (rx == ry) {
	itp = tp = NULL;
    } else {
	t = CGAffineTransformMakeScale(1.0, ry / (float)rx);
	it = CGAffineTransformInvert(t);
	tp = &t;
	itp = &it;
    }

    x += outlineWidth / 2.0, y += outlineWidth / 2.0;
    width -= outlineWidth, height -= outlineWidth;

    /* Simple case: draw all 4 corners and 4 edges */
    if (!open) {
	AddArcToPath(p, tp, itp, x + rx, y + ry, rx, 180, 270); /* top-left */
	AddArcToPath(p, tp, itp, x + width - rx, y + ry, rx, 270, 0); /* top-right */
	AddArcToPath(p, tp, itp, x + width - rx, y + height - ry, rx, 0, 90); /* bottom-right */
	AddArcToPath(p, tp, itp, x + rx, y + height - ry, rx, 90, 180); /* bottom-left */
	CGPathCloseSubpath(p);

    /* Complicated case: some edges are "open" */
    } else {
	CGPoint start[4], end[4]; /* start and end points of line segments*/
	start[0] = CGPointMake(x, y);
	end[3] = start[0];
	if (drawW && drawN) {
	    start[0].x += rx;
	    end[3].y += ry;
	}
	end[0] = CGPointMake(x + width, y);
	start[1]= end[0];
	if (drawE && drawN) {
	    end[0].x -= rx;
	    start[1].y += ry;
	}
	end[1] = CGPointMake(x + width, y + height);
	start[2] = end[1];
	if (drawE && drawS) {
	    end[1].y -= ry;
	    start[2].x -= rx;
	}
	end[2] = CGPointMake(x, y + height);
	start[3] = end[2];
	if (drawW && drawS) {
	    end[2].x += rx;
	    start[3].y -= ry;
	}

	if (drawW && drawN) {
	    AddArcToPath(p, tp, itp, x + rx, y + ry, rx, 180, 270); /* top-left */
	} else if (drawN) {
	    CGPathMoveToPoint(p, NULL, start[0].x, start[0].y);
	}
	if (drawN)
	    CGPathAddLineToPoint(p, NULL, end[0].x, end[0].y);
	if (drawE && drawN)
	    AddArcToPath(p, tp, itp, x + width - rx, y + ry, rx, 270, 0); /* top-right */
	else if (!drawN && drawE)
	    CGPathMoveToPoint(p, NULL, start[1].x, start[1].y);
	if (drawE)
	    CGPathAddLineToPoint(p, NULL, end[1].x, end[1].y);
	if (drawE && drawS)
	    AddArcToPath(p, tp, itp, x + width - rx, y + height - ry, rx, 0, 90); /* bottom-right */
	else if (!drawE && drawS)
	    CGPathMoveToPoint(p, NULL, start[2].x, start[2].y);
	if (drawS)
	    CGPathAddLineToPoint(p, NULL, end[2].x, end[2].y);
	if (drawW && drawS)
	    AddArcToPath(p, tp, itp, x + rx, y + height - ry, rx, 90, 180); /* bottom-left */
	else if (!drawS && drawW)
	    CGPathMoveToPoint(p, NULL, start[3].x, start[3].y);
	if (drawW)
	    CGPathAddLineToPoint(p, NULL, end[3].x, end[3].y);
    }

    return p;
}

void
Tree_DrawRoundRect(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    XColor *xcolor,		/* Color. */
    TreeRectangle tr,		/* Where to draw. */
    int outlineWidth,		/* Thickness of the outline. */
    int rx, int ry,		/* Corner radius */
    int open			/* RECT_OPEN_x flags */
    )
{
    MacDrawable *macDraw = (MacDrawable *) td.drawable;
    MacContextSetup dc;
    CGContextRef context;
    int antialias = !open; /* the arcs can be antialiased, but not the line ends! */
#if 0
    RGBColor c;
#endif

    if (!(macDraw->flags & TK_IS_PIXMAP) || !tree->nativeGradients) {
	GC gc = Tk_GCForColor(xcolor, Tk_WindowId(tree->tkwin));
	Tree_DrawRoundRectX11(tree, td, clip, gc, tr, outlineWidth, rx, ry, open);
	return;
    }

    context = TreeMacOSX_GetContext(tree, td.drawable, tr, &dc);
    if (context == NULL) {
	GC gc = Tk_GCForColor(xcolor, Tk_WindowId(tree->tkwin));
	Tree_DrawRoundRectX11(tree, td, clip, gc, tr, outlineWidth, rx, ry, open);
	return;
    }

    CGContextBeginPath(context);
    if (rx == ry && !open) {
	CGFloat x = tr.x, y = tr.y, width = tr.width, height = tr.height;
	x += outlineWidth / 2.0, y += outlineWidth / 2.0, width -= outlineWidth, height -= outlineWidth;
	CGContextAddArc(context, x + rx, y + ry, rx, radians(180), radians(270), 0); /* top-left */
	CGContextAddArc(context, x + width - rx, y + ry, rx, radians(270), radians(0), 0); /* top-right */
	CGContextAddArc(context, x + width - rx, y + height - ry, rx, radians(0), radians(90), 0); /* bottom-right */
	CGContextAddArc(context, x + rx, y + height - ry, rx, radians(90), radians(180), 0); /* bottom-left */
	CGContextClosePath(context);
    } else {
	CGMutablePathRef p = MakeRoundRectPath_Stroke(tr, outlineWidth, rx, ry, open);
	if (p) {
	    CGContextAddPath(context, p);
	    CGPathRelease(p);
	}
    }

    CGContextSetLineWidth(context, outlineWidth - (antialias ? 0 : 0.99) /*0.01*/);
    CGContextSetLineCap(context, kCGLineCapSquare);
    CGContextSetShouldAntialias(context, antialias);

    /* FIXME: the colors don't match the non-native version */
#if 0
    dbwin("%#0X", (int)xcolor->pixel);
    c.red = (xcolor->pixel >> 16) & 0xff;
    c.green = (xcolor->pixel >> 8) & 0xff;
    c.blue = (xcolor->pixel) & 0xff;
    c.red |= c.red << 8;
    c.green |= c.green << 8;
    c.blue |= c.blue << 8;
    CGContextSetRGBStrokeColor/*WithColor*/(context,
	c.red / 65535.0f,
	c.green / 65535.0f,
	c.blue / 65535.0f,
	1.0f);
#else
    CGContextSetRGBStrokeColor/*WithColor*/(context,
	RedFloatFromXColorPtr(xcolor),
	GreenFloatFromXColorPtr(xcolor),
	BlueFloatFromXColorPtr(xcolor),
	1.0f);
#endif
    CGContextStrokePath(context);

    TreeMacOSX_ReleaseContext(tree, &dc);
}

static CGMutablePathRef
MakeRoundRectPath_OutlineFilled(
    TreeRectangle tr,		/* Where to draw. */
    int outlineWidth,		/* Thickness of the outline. */
    int rx, int ry,		/* Corner radius */
    int open			/* RECT_OPEN_x flags */
    )
{
    CGFloat x = tr.x, y = tr.y, w = tr.width, h = tr.height;
    CGFloat ow = outlineWidth, yow = ow;
    CGFloat rOut = rx /*+ ow / 2*/, rIn = rx /*- ow / 2*/;
    int drawW = (open & RECT_OPEN_W) == 0;
    int drawN = (open & RECT_OPEN_N) == 0;
    int drawE = (open & RECT_OPEN_E) == 0;
    int drawS = (open & RECT_OPEN_S) == 0;
    CGMutablePathRef p = CGPathCreateMutable();
    CGRect r;
    CGAffineTransform t, it = {0}, *tp, *itp;

    if (rx == ry) {
	itp = tp = NULL;
    } else {
	t = CGAffineTransformMakeScale(1.0, ry / (float)rx);
	it = CGAffineTransformInvert(t);
	tp = &t;
	itp = &it;
    }
#if 1
    /* Place the edges with gaps at the corners. */
    if (drawN) {
	r = CGRectMake(x + drawW * rx, y, w - (drawW + drawE) * rx, ow);
	CGPathAddRect(p, NULL, r);
    }
    if (drawE) {
	r = CGRectMake(x + w - ow, y + drawN * ry, ow, h - (drawN + drawS) * ry);
	CGPathAddRect(p, NULL, r);
    }
    if (drawS) {
	r = CGRectMake(x + drawW * rx, y + h - ow, w - (drawW + drawE) * rx, ow);
	CGPathAddRect(p, NULL, r);
    }
    if (drawW) {
	r = CGRectMake(x, y + drawN * ry, ow, h - (drawN + drawS) * ry);
	CGPathAddRect(p, NULL, r);
    }
#endif
    if (rx != ry) {
	ry = rx;
	h = CGPointApplyAffineTransform(CGPointMake(0, h), it).y;
	yow = CGPointApplyAffineTransform(CGPointMake(0, yow), it).y;
    }
    if (drawW && drawN) {
	CGPathMoveToPoint(p, tp, x + ow, y + yow + ry);
	CGPathAddLineToPoint(p, tp, x, y + yow + ry);
	CGPathAddArcToPoint(p, tp, x, y, x + ow + rx, y, rOut);
	CGPathAddLineToPoint(p, tp, x + ow + rx, y + yow);
	CGPathAddArcToPoint(p, tp, x + ow, y + yow, x + ow, y + yow + ry, rIn);
    }

    if (drawE && drawN) {
	CGPathMoveToPoint(p, tp, x + w - ow - rx, y + yow);
	CGPathAddLineToPoint(p, tp, x + w - ow - rx, y);
	CGPathAddArcToPoint(p, tp, x + w, y, x + w, y + ry + yow, rOut);
	CGPathAddLineToPoint(p, tp, x + w - ow, y + ry + yow);
	CGPathAddArcToPoint(p, tp, x + w - ow, y + yow, x + w - ow - rx, y + yow, rIn);
    }
    if (drawE && drawS) {
	CGPathMoveToPoint(p, tp, x + w - ow, y + h - ry - yow);
	CGPathAddLineToPoint(p, tp, x + w, y + h - ry - yow);
	CGPathAddArcToPoint(p, tp, x + w, y + h, x + w - rx - ow, y + h, rOut);
	CGPathAddLineToPoint(p, tp, x + w - rx - ow, y + h - yow);
	CGPathAddArcToPoint(p, tp, x + w - ow, y + h - yow, x + w - ow, y + h - ry - yow, rIn);
    }
    if (drawW && drawS) {
	CGPathMoveToPoint(p, tp, x + rx + ow, y + h - yow);
	CGPathAddLineToPoint(p, tp, x + rx + ow, y + h);
	CGPathAddArcToPoint(p, tp, x, y + h, x, y + h - ry - yow, rOut);
	CGPathAddLineToPoint(p, tp, x + ow, y + h - ry - yow);
	CGPathAddArcToPoint(p, tp, x + ow, y + h - yow, x + rx + ow, y + h - yow, rIn);
    }

    return p;
}

void
TreeGradient_DrawRoundRect(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    TreeGradient gradient,	/* Gradient. */
    TreeRectangle trBrush,	/* Brush bounds. */
    TreeRectangle tr,		/* Rectangle to draw. */
    int outlineWidth,		/* Width of outline. */
    int rx, int ry,		/* Corner radius */
    int open			/* RECT_OPEN_x flags */
    )
{
    MacDrawable *macDraw = (MacDrawable *) td.drawable;
    MacContextSetup dc;
    CGContextRef context;
    int antialias = 1; /* the arcs can be antialiased, but not the line ends! */
    MacShading ms;
    CGShadingRef shading;

    if (gradient->stopArrPtr == NULL || gradient->stopArrPtr->nstops < 2)
	return;

    /* Draw nothing if the brush is zero-sized. */
    if (trBrush.width <= 0 || trBrush.height <= 0)
	return;

    if (!(macDraw->flags & TK_IS_PIXMAP) || !tree->nativeGradients) {
	XColor *xcolor = gradient->stopArrPtr->stops[0]->color; /* Use the first stop color */
	GC gc = Tk_GCForColor(xcolor, Tk_WindowId(tree->tkwin));
	Tree_DrawRoundRectX11(tree, td, clip, gc, tr, outlineWidth, rx, ry, open);
	return;
    }

    context = TreeMacOSX_GetContext(tree, td.drawable, tr, &dc);
    if (context == NULL) {
	XColor *xcolor = gradient->stopArrPtr->stops[0]->color; /* Use the first stop color */
	GC gc = Tk_GCForColor(xcolor, Tk_WindowId(tree->tkwin));
	Tree_DrawRoundRectX11(tree, td, clip, gc, tr, outlineWidth, rx, ry, open);
	return;
    }

    shading = MakeLinearGradientShading(context, gradient, trBrush, tr, &ms);
    if (shading) {
	CGMutablePathRef p;
    
	CGContextBeginPath(context);

	p = MakeRoundRectPath_OutlineFilled(tr, outlineWidth, rx, ry, open);
	if (p) {
	    CGContextAddPath(context, p);
	    CGPathRelease(p);

	    CGContextSetShouldAntialias(context, antialias);

	    /* Must clip to the area to be painted otherwise the entire context
	    * is filled with the gradient. */
	    CGContextClip(context);

	    CGContextDrawShading(context, shading);
	}

	ReleaseLinearGradientShading(&ms);
    }

    TreeMacOSX_ReleaseContext(tree, &dc);
}

void
Tree_FillRoundRect(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    XColor *xcolor,		/* Color. */
    TreeRectangle tr,		/* Where to draw. */
    int rx, int ry,		/* Corner radius */
    int open			/* RECT_OPEN_x flags */
    )
{
    GC gc = Tk_GCForColor(xcolor, Tk_WindowId(tree->tkwin));
    Tree_FillRoundRectX11(tree, td, clip, gc, tr, rx, ry, open);
}

int
TreeDraw_InitInterp(
    Tcl_Interp *interp
    )
{
    return TCL_OK;
}
