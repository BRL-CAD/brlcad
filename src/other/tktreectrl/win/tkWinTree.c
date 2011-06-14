/* 
 * tkWinTree.c --
 *
 *	Platform-specific parts of TkTreeCtrl for Microsoft Windows.
 *
 * Copyright (c) 2010 Tim Baker
 *
 * RCS: @(#) $Id$
 */

#define WINVER 0x0501 /* MingW32 */
#define _WIN32_WINNT 0x0501 /* ACTCTX stuff */

#include "tkTreeCtrl.h"
#include "tkWinInt.h"

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
    TkWinDCState state;
    HDC dc;
    HPEN pen, oldPen;
    int nw;
    int wx = x1 + tree->drawableXOrigin;
    int wy = y1 + tree->drawableYOrigin;

    dc = TkWinGetDrawableDC(tree->display, drawable, &state);
    SetROP2(dc, R2_COPYPEN);

    pen = CreatePen(PS_SOLID, 1, tree->lineGC[0]->foreground);
    oldPen = SelectObject(dc, pen);

    nw = !(wx & 1) == !(wy & 1);
    for (x1 += !nw; x1 < x2; x1 += 2) {
	MoveToEx(dc, x1, y1, NULL);
	LineTo(dc, x1 + 1, y1);
    }

    SelectObject(dc, oldPen);
    DeleteObject(pen);

    TkWinReleaseDrawableDC(drawable, dc, &state);
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
    TkWinDCState state;
    HDC dc;
    HPEN pen, oldPen;
    int nw;
    int wx = x1 + tree->drawableXOrigin;
    int wy = y1 + tree->drawableYOrigin;

    dc = TkWinGetDrawableDC(tree->display, drawable, &state);
    SetROP2(dc, R2_COPYPEN);

    pen = CreatePen(PS_SOLID, 1, tree->lineGC[0]->foreground);
    oldPen = SelectObject(dc, pen);

    nw = !(wx & 1) == !(wy & 1);
    for (y1 += !nw; y1 < y2; y1 += 2) {
	MoveToEx(dc, x1, y1, NULL);
	LineTo(dc, x1 + 1, y1);
    }

    SelectObject(dc, oldPen);
    DeleteObject(pen);

    TkWinReleaseDrawableDC(drawable, dc, &state);
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
    int wx = x + tree->drawableXOrigin;
    int wy = y + tree->drawableYOrigin;
    int w = !(open & RECT_OPEN_W);
    int n = !(open & RECT_OPEN_N);
    int e = !(open & RECT_OPEN_E);
    int s = !(open & RECT_OPEN_S);
    int nw, ne, sw, se;
    int i;
    TkWinDCState state;
    HDC dc;

    /* Dots on even pixels only */
    nw = !(wx & 1) == !(wy & 1);
    ne = !((wx + width - 1) & 1) == !(wy & 1);
    sw = !(wx & 1) == !((wy + height - 1) & 1);
    se = !((wx + width - 1) & 1) == !((wy + height - 1) & 1);

    dc = TkWinGetDrawableDC(tree->display, drawable, &state);
    SetROP2(dc, R2_NOT);

    if (w) /* left */
    {
	for (i = !nw; i < height; i += 2) {
	    MoveToEx(dc, x, y + i, NULL);
	    LineTo(dc, x + 1, y + i);
	}
    }
    if (n) /* top */
    {
	for (i = nw ? w * 2 : 1; i < width; i += 2) {
	    MoveToEx(dc, x + i, y, NULL);
	    LineTo(dc, x + i + 1, y);
	}
    }
    if (e) /* right */
    {
	for (i = ne ? n * 2 : 1; i < height; i += 2) {
	    MoveToEx(dc, x + width - 1, y + i, NULL);
	    LineTo(dc, x + width, y + i);
	}
    }
    if (s) /* bottom */
    {
	for (i = sw ? w * 2 : 1; i < width - (se && e); i += 2) {
	    MoveToEx(dc, x + i, y + height - 1, NULL);
	    LineTo(dc, x + i + 1, y + height - 1);
	}
    }

    TkWinReleaseDrawableDC(drawable, dc, &state);
}

/*
 * The following structure is used when drawing a number of dotted XOR
 * rectangles.
 */
struct DotStatePriv
{
    TreeCtrl *tree;
    Drawable drawable;
    HDC dc;
    TkWinDCState dcState;
    HRGN rgn;
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
    struct DotStatePriv *dotState = (struct DotStatePriv *) p;

    if (sizeof(*dotState) > sizeof(*p))
	panic("TreeDotRect_Setup: DotState hack is too small");

    dotState->tree = tree;
    dotState->drawable = drawable;
    dotState->dc = TkWinGetDrawableDC(tree->display, drawable, &dotState->dcState);

    /* XOR drawing */
    SetROP2(dotState->dc, R2_NOT);

    /* Keep drawing inside the contentbox. */
    dotState->rgn = CreateRectRgn(
	Tree_ContentLeft(tree),
	Tree_ContentTop(tree),
	Tree_ContentRight(tree),
	Tree_ContentBottom(tree));
    SelectClipRgn(dotState->dc, dotState->rgn);
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
    RECT rect;

    rect.left = x;
    rect.right = x + width;
    rect.top = y;
    rect.bottom = y + height;
    DrawFocusRect(dotState->dc, &rect);
#else
    HDC dc = dotState->dc; 
    int i;
    int wx = x + dotState->tree->drawableXOrigin;
    int wy = y + dotState->tree->drawableYOrigin;
    int nw, ne, sw, se;

    /* Dots on even pixels only */
    nw = !(wx & 1) == !(wy & 1);
    ne = !((wx + width - 1) & 1) == !(wy & 1);
    sw = !(wx & 1) == !((wy + height - 1) & 1);
    se = !((wx + width - 1) & 1) == !((wy + height - 1) & 1);

    for (i = !nw; i < height; i += 2) {
	MoveToEx(dc, x, y + i, NULL);
	LineTo(dc, x + 1, y + i);
    }
    for (i = nw ? 2 : 1; i < width; i += 2) {
	MoveToEx(dc, x + i, y, NULL);
	LineTo(dc, x + i + 1, y);
    }
    for (i = ne ? 2 : 1; i < height; i += 2) {
	MoveToEx(dc, x + width - 1, y + i, NULL);
	LineTo(dc, x + width, y + i);
    }
    for (i = sw ? 2 : 1; i < width - se; i += 2) {
	MoveToEx(dc, x + i, y + height - 1, NULL);
	LineTo(dc, x + i + 1, y + height - 1);
    }
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
    SelectClipRgn(dotState->dc, NULL);
    DeleteObject(dotState->rgn);
    TkWinReleaseDrawableDC(dotState->drawable, dotState->dc, &dotState->dcState);
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
    HDC dc;
    TkWinDCState dcState;
    HBRUSH brush;

    dc = TkWinGetDrawableDC(display, drawable, &dcState);
    SetROP2(dc, R2_COPYPEN);
    brush = CreateSolidBrush(gc->foreground);
    FillRgn(dc, (HRGN) rgn, brush);
    DeleteObject(brush);
    TkWinReleaseDrawableDC(drawable, dc, &dcState);
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
    OffsetRgn((HRGN) region, xOffset, yOffset);
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
    TkRegion rgnOut
    )
{
    CombineRgn((HRGN) rgnA, (HRGN) rgnB, (HRGN) rgnOut, RGN_OR);
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
#if 0
    /* It would be best to call ScrollWindowEx with SW_SCROLLCHILDREN so
     * that windows in window elements scroll smoothly with a minimum of
     * redrawing. */
    HWND hwnd = TkWinGetHWND(Tk_WindowId(tree->tkwin));
    HWND hwndChild;
    RECT scrollRect, childRect;
    struct {
	int x;
	int y;
	TkWindow *winPtr;
    } winInfo[128], *winInfoPtr;
    TkWindow *winPtr = (TkWindow *) tree->tkwin;
    int winCount = 0;
    int result;

    winInfoPtr = winInfo;
    for (winPtr = winPtr->childList; winPtr != NULL; winPtr = winPtr->nextPtr) {
	if (winPtr->window != None) {
	    hwndChild = TkWinGetHWND(winPtr->window);
	    GetWindowRect(hwndChild, &childRect);
	    winInfoPtr->x = childRect.left;
	    winInfoPtr->y = childRect.top;
	    winInfoPtr->winPtr = winPtr;
	    winInfoPtr++;
	    winCount++;
	}
    }

    scrollRect.left = x;
    scrollRect.top = y;
    scrollRect.right = x + width;
    scrollRect.bottom = y + height;
    result = (ScrollWindowEx(hwnd, dx, dy, &scrollRect, NULL, (HRGN) damageRgn,
	    NULL, SW_SCROLLCHILDREN) == NULLREGION) ? 0 : 1;

    winInfoPtr = winInfo;
    while (winCount--) {
	winPtr = winInfoPtr->winPtr;
	hwndChild = TkWinGetHWND(winPtr->window);
	GetWindowRect(hwndChild, &childRect);
	if (childRect.left != winInfoPtr->x ||
		childRect.top != winInfoPtr->y) {
	    dbwin("moved window %s %d,%d\n", winPtr->pathName, childRect.left - winInfoPtr->x, childRect.top - winInfoPtr->y);
	    winPtr->changes.x += childRect.left - winInfoPtr->x;
	    winPtr->changes.y += childRect.top - winInfoPtr->y;
	    /* TkDoConfigureNotify(winPtr); */
	}
	winInfoPtr++;
    }
#else
    int result = TkScrollWindow(tree->tkwin, gc, x, y, width, height, dx, dy,
	damageRgn);
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

    /* Tk_DrawChars does not clear the clip region */
    if (drawable == Tk_WindowId(tree->tkwin)) {
	HDC dc;
	TkWinDCState dcState;

	dc = TkWinGetDrawableDC(tree->display, drawable, &dcState);
	SelectClipRgn(dc, NULL);
	TkWinReleaseDrawableDC(drawable, dc, &dcState);
    }
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
    TkpClipMask *clipPtr = (TkpClipMask *) gc->clip_mask;

    XSetClipOrigin(tree->display, gc, dest_x, dest_y);

    /*
     * It seems as though the device context is not set up properly
     * when drawing a transparent bitmap into a window. Normally Tk draws
     * into an offscreen pixmap which gets a temporary device context.
     * This fixes a bug with -doublebuffer none in the demo "Bitmaps".
     */
    if (drawable == Tk_WindowId(tree->tkwin)) {
	if ((clipPtr != NULL) &&
	    (clipPtr->type == TKP_CLIP_PIXMAP) &&
	    (clipPtr->value.pixmap == bitmap)) {
	    HDC dc;
	    TkWinDCState dcState;

	    dc = TkWinGetDrawableDC(tree->display, drawable, &dcState);
	    SetTextColor(dc, RGB(0,0,0));
	    SetBkColor(dc, RGB(255,255,255));
	    TkWinReleaseDrawableDC(drawable, dc, &dcState);
	}
    }
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

    Tk_PhotoBlank(photoH);

    /* See TkPoscriptImage */

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

	    r = GetRValue(pixel);
	    g = GetGValue(pixel);
	    b = GetBValue(pixel);

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
    HDC dc;
    TkRegion region;
} TreeClipStateDC;

static void
TreeClip_ToDC(
    TreeCtrl *tree,		/* Widget info. */
    TreeClip *clip,		/* Clipping area or NULL. */
    HDC dc,			/* Windows device context. */
    TreeClipStateDC *state
    )
{
    state->tree = tree;
    state->clip = clip;
    state->dc = dc;
    state->region = None;

    if (clip && clip->type == TREE_CLIP_RECT) {
	state->region = Tree_GetRectRegion(tree, &clip->tr);
	SelectClipRgn(dc, (HRGN) state->region);
    }
    if (clip && clip->type == TREE_CLIP_AREA) {
	TreeRectangle tr;
	if (Tree_AreaBbox(tree, clip->area, &tr) == 0)
	    return;
	state->region = Tree_GetRectRegion(tree, &tr);
	SelectClipRgn(dc, (HRGN) state->region);
    }
    if (clip && clip->type == TREE_CLIP_REGION) {
	SelectClipRgn(dc, (HRGN) clip->region);
    }
}

static void
TreeClip_FinishDC(
    TreeClipStateDC *state
    )
{
    SelectClipRgn(state->dc, NULL);
    if (state->region != NULL)
	Tree_FreeRegion(state->tree, state->region);
}

#if USE_ITEM_PIXMAP == 0
/*
 *----------------------------------------------------------------------
 *
 * DrawOrFillArc --
 *	Copied from tkTwinDraw.c because the clip region is ignored on
 *	Win32.
 *
 *	This function handles the rendering of drawn or filled arcs and
 *	chords.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Renders the requested arc.
 *
 *----------------------------------------------------------------------
 */

/*
 * These macros convert between X's bizarre angle units to radians.
 */

#define PI 3.14159265358979
#define XAngleToRadians(a) ((double)(a) / 64 * PI / 180);

static void
DrawOrFillArc(
    TreeCtrl *tree,		/* Widget info. */
    Drawable d,
    TreeClip *clip,		/* Clipping area or NULL. */
    GC gc,
    int x, int y,		/* left top */
    unsigned int width, unsigned int height,
    int start,			/* start: three-o'clock (deg*64) */
    int extent,			/* extent: relative (deg*64) */
    int fill)			/* ==0 draw, !=0 fill */
{
    HDC dc;
    HBRUSH brush, oldBrush;
    HPEN pen, oldPen;
    TkWinDCState state;
    TreeClipStateDC clipState;
    int clockwise = (extent < 0); /* non-zero if clockwise */
    int xstart, ystart, xend, yend;
    double radian_start, radian_end, xr, yr;

    if (d == None) {
	return;
    }

    dc = TkWinGetDrawableDC(tree->display, d, &state);
    TreeClip_ToDC(tree, clip, dc, &clipState);

/*    SetROP2(dc, tkpWinRopModes[gc->function]);*/

    /*
     * Compute the absolute starting and ending angles in normalized radians.
     * Swap the start and end if drawing clockwise.
     */

    start = start % (64*360);
    if (start < 0) {
	start += (64*360);
    }
    extent = (start+extent) % (64*360);
    if (extent < 0) {
	extent += (64*360);
    }
    if (clockwise) {
	int tmp = start;
	start = extent;
	extent = tmp;
    }
    radian_start = XAngleToRadians(start);
    radian_end = XAngleToRadians(extent);

    /*
     * Now compute points on the radial lines that define the starting and
     * ending angles. Be sure to take into account that the y-coordinate
     * system is inverted.
     */

    xr = x + width / 2.0;
    yr = y + height / 2.0;
    xstart = (int)((xr + cos(radian_start)*width/2.0) + 0.5);
    ystart = (int)((yr + sin(-radian_start)*height/2.0) + 0.5);
    xend = (int)((xr + cos(radian_end)*width/2.0) + 0.5);
    yend = (int)((yr + sin(-radian_end)*height/2.0) + 0.5);

    /*
     * Now draw a filled or open figure. Note that we have to increase the
     * size of the bounding box by one to account for the difference in pixel
     * definitions between X and Windows.
     */

    pen = CreatePen((int) PS_SOLID, gc->line_width, gc->foreground);
    oldPen = SelectObject(dc, pen);
    if (!fill) {
	/*
	 * Note that this call will leave a gap of one pixel at the end of the
	 * arc for thin arcs. We can't use ArcTo because it's only supported
	 * under Windows NT.
	 */

	SetBkMode(dc, TRANSPARENT);
	Arc(dc, x, y, (int) (x+width+1), (int) (y+height+1), xstart, ystart,
		xend, yend);
    } else {
	brush = CreateSolidBrush(gc->foreground);
	oldBrush = SelectObject(dc, brush);
	if (gc->arc_mode == ArcChord) {
	    Chord(dc, x, y, (int) (x+width+1), (int) (y+height+1),
		    xstart, ystart, xend, yend);
	} else if (gc->arc_mode == ArcPieSlice) {
	    Pie(dc, x, y, (int) (x+width+1), (int) (y+height+1),
		    xstart, ystart, xend, yend);
	}
	DeleteObject(SelectObject(dc, oldBrush));
    }
    DeleteObject(SelectObject(dc, oldPen));
    TreeClip_FinishDC(&clipState);
    TkWinReleaseDrawableDC(d, dc, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_DrawArc --
 *
 *	Wrapper around XDrawArc() because the clip region is
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
Tree_DrawArc(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    GC gc,
    int x, int y,
    unsigned int width, unsigned int height,
    int start, int extent)
{
    tree->display->request++;

    DrawOrFillArc(tree, td.drawable, clip, gc, x, y, width, height,
	start, extent, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_FillArc --
 *
 *	Wrapper around XFillArc() because the clip region is
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
Tree_FillArc(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    GC gc,
    int x, int y,
    unsigned int width, unsigned int height,
    int start, int extent)
{
    tree->display->request++;

    DrawOrFillArc(tree, td.drawable, clip, gc, x, y, width, height,
	start, extent, 1);
}

#endif /* USE_ITEM_PIXMAP == 0 */

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
#if 1
    HDC dc;
    TkWinDCState dcState;
    TreeClipStateDC clipState;
    RECT rect;
    COLORREF oldColor;

    dc = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);
    TreeClip_ToDC(tree, clip, dc, &clipState);

    rect.left = tr.x, rect.top = tr.y,
	rect.right = tr.x + tr.width, rect.bottom = tr.y + tr.height;

    oldColor = SetBkColor(dc, (COLORREF)gc->foreground);
    SetBkMode(dc, OPAQUE);
    ExtTextOut(dc, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
    SetBkColor(dc, oldColor);

    TreeClip_FinishDC(&clipState);
    TkWinReleaseDrawableDC(td.drawable, dc, &dcState);
#else
    HDC dc;
    TkWinDCState dcState;
    HBRUSH brush;
    RECT rect;
    TreeClipStateDC clipState;

    dc = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);
    TreeClip_ToDC(tree, clip, dc, &clipState);

    brush = CreateSolidBrush(gc->foreground);
    rect.left = tr.x, rect.top = tr.y,
	rect.right = tr.x + tr.width, rect.bottom = tr.y + tr.height;
    FillRect(dc, &rect, brush);

    TreeClip_FinishDC(&clipState);
    DeleteObject(brush);
    TkWinReleaseDrawableDC(td.drawable, dc, &dcState);
#endif
}

#if USE_ITEM_PIXMAP == 0

static void
FastFillRect(
    HDC dc,
    int x, int y, int width, int height,
    COLORREF pixel
    )
{
    RECT rect;
    COLORREF oldColor;

    rect.left = x, rect.top = y,
	rect.right = x + width, rect.bottom = y + height;

    oldColor = SetBkColor(dc, pixel);
    SetBkMode(dc, OPAQUE);
    ExtTextOut(dc, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
    SetBkColor(dc, oldColor);
}
    
static void
_3DVerticalBevel(
    HDC dc,
    Tk_Window tkwin,		/* Window for which border was allocated. */
    Tk_3DBorder border,		/* Token for border to draw. */
    int x, int y, int width, int height,
				/* Area of vertical bevel. */
    int leftBevel,		/* Non-zero means this bevel forms the left
				 * side of the object; 0 means it forms the
				 * right side. */
    int relief)			/* Kind of bevel to draw. For example,
				 * TK_RELIEF_RAISED means interior of object
				 * should appear higher than exterior. */
{
    COLORREF left, right;
    int half;

    switch (relief) {
    case TK_RELIEF_RAISED:
	left = (leftBevel)
		? TkWinGetBorderPixels(tkwin, border, TK_3D_LIGHT_GC)
		: TkWinGetBorderPixels(tkwin, border, TK_3D_DARK_GC);
	right = (leftBevel)
		? TkWinGetBorderPixels(tkwin, border, TK_3D_LIGHT2)
		: TkWinGetBorderPixels(tkwin, border, TK_3D_DARK2);
	break;
    case TK_RELIEF_SUNKEN:
	left = (leftBevel)
		? TkWinGetBorderPixels(tkwin, border, TK_3D_DARK_GC)
		: TkWinGetBorderPixels(tkwin, border, TK_3D_LIGHT2);
	right = (leftBevel)
		? TkWinGetBorderPixels(tkwin, border, TK_3D_DARK2)
		: TkWinGetBorderPixels(tkwin, border, TK_3D_LIGHT_GC);
	break;
    case TK_RELIEF_RIDGE:
	left = TkWinGetBorderPixels(tkwin, border, TK_3D_LIGHT_GC);
	right = TkWinGetBorderPixels(tkwin, border, TK_3D_DARK_GC);
	break;
    case TK_RELIEF_GROOVE:
	left = TkWinGetBorderPixels(tkwin, border, TK_3D_DARK_GC);
	right = TkWinGetBorderPixels(tkwin, border, TK_3D_LIGHT_GC);
	break;
    case TK_RELIEF_FLAT:
	left = right = TkWinGetBorderPixels(tkwin, border, TK_3D_FLAT_GC);
	break;
    case TK_RELIEF_SOLID:
    default:
	left = right = RGB(0,0,0);
	break;
    }
    half = width/2;
    if (leftBevel && (width & 1)) {
	half++;
    }
    FastFillRect(dc, x, y, half, height, left);
    FastFillRect(dc, x+half, y, width-half, height, right);
}

static void
_3DHorizontalBevel(
    HDC dc,
    Tk_Window tkwin,		/* Window for which border was allocated. */
    Tk_3DBorder border,		/* Token for border to draw. */
    int x, int y, int width, int height,
				/* Bounding box of area of bevel. Height gives
				 * width of border. */
    int leftIn, int rightIn,	/* Describes whether the left and right edges
				 * of the bevel angle in or out as they go
				 * down. For example, if "leftIn" is true, the
				 * left side of the bevel looks like this:
				 *	___________
				 *	 __________
				 *	  _________
				 *	   ________
				 */
    int topBevel,		/* Non-zero means this bevel forms the top
				 * side of the object; 0 means it forms the
				 * bottom side. */
    int relief)			/* Kind of bevel to draw. For example,
				 * TK_RELIEF_RAISED means interior of object
				 * should appear higher than exterior. */
{
    int bottom, halfway, x1, x2, x1Delta, x2Delta;
    int topColor, bottomColor;

    switch (relief) {
    case TK_RELIEF_RAISED:
	topColor = (topBevel)
		? TkWinGetBorderPixels(tkwin, border, TK_3D_LIGHT_GC)
		: TkWinGetBorderPixels(tkwin, border, TK_3D_DARK_GC);
	bottomColor = (topBevel)
		? TkWinGetBorderPixels(tkwin, border, TK_3D_LIGHT2)
		: TkWinGetBorderPixels(tkwin, border, TK_3D_DARK2);
	break;
    case TK_RELIEF_SUNKEN:
	topColor = (topBevel)
		? TkWinGetBorderPixels(tkwin, border, TK_3D_DARK_GC)
		: TkWinGetBorderPixels(tkwin, border, TK_3D_LIGHT2);
	bottomColor = (topBevel)
		? TkWinGetBorderPixels(tkwin, border, TK_3D_DARK2)
		: TkWinGetBorderPixels(tkwin, border, TK_3D_LIGHT_GC);
	break;
    case TK_RELIEF_RIDGE:
	topColor = TkWinGetBorderPixels(tkwin, border, TK_3D_LIGHT_GC);
	bottomColor = TkWinGetBorderPixels(tkwin, border, TK_3D_DARK_GC);
	break;
    case TK_RELIEF_GROOVE:
	topColor = TkWinGetBorderPixels(tkwin, border, TK_3D_DARK_GC);
	bottomColor = TkWinGetBorderPixels(tkwin, border, TK_3D_LIGHT_GC);
	break;
    case TK_RELIEF_FLAT:
	topColor = bottomColor = TkWinGetBorderPixels(tkwin, border, TK_3D_FLAT_GC);
	break;
    case TK_RELIEF_SOLID:
    default:
	topColor = bottomColor = RGB(0,0,0);
    }

    if (leftIn) {
	x1 = x+1;
    } else {
	x1 = x+height-1;
    }
    x2 = x+width;
    if (rightIn) {
	x2--;
    } else {
	x2 -= height;
    }
    x1Delta = (leftIn) ? 1 : -1;
    x2Delta = (rightIn) ? -1 : 1;
    halfway = y + height/2;
    if (topBevel && (height & 1)) {
	halfway++;
    }
    bottom = y + height;

    for ( ; y < bottom; y++) {
	if (x1 < x2) {
	    FastFillRect(dc, x1, y, x2-x1, 1,
		(y < halfway) ? topColor : bottomColor);
	}
	x1 += x1Delta;
	x2 += x2Delta;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_Draw3DRectangle --
 *
 *	Reimplementation of Tk_Draw3DRectangle() because the clip
 *	region is ignored on Win32.
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
Tree_Draw3DRectangle(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    Tk_3DBorder border,		/* Token for border to draw. */
    int x, int y, int width, int height,
				/* Outside area of region in which border will
				 * be drawn. */
    int borderWidth,		/* Desired width for border, in pixels. */
    int relief			/* Type of relief: TK_RELIEF_RAISED,
				 * TK_RELIEF_SUNKEN, TK_RELIEF_GROOVE, etc. */
    )
{
    Tk_Window tkwin = tree->tkwin;
    HDC dc;
    TkWinDCState dcState;
    TreeClipStateDC clipState;

    dc = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);
    TreeClip_ToDC(tree, clip, dc, &clipState);

    if (width < 2*borderWidth) {
	borderWidth = width/2;
    }
    if (height < 2*borderWidth) {
	borderWidth = height/2;
    }
    _3DVerticalBevel(dc, tkwin, border, x, y, borderWidth, height,
	    1, relief);
    _3DVerticalBevel(dc, tkwin, border, x+width-borderWidth, y,
	    borderWidth, height, 0, relief);
    _3DHorizontalBevel(dc, tkwin, border, x, y, width, borderWidth,
	    1, 1, 1, relief);
    _3DHorizontalBevel(dc, tkwin, border, x, y+height-borderWidth,
	    width, borderWidth, 0, 0, 0, relief);

    TreeClip_FinishDC(&clipState);
    TkWinReleaseDrawableDC(td.drawable, dc, &dcState);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_Fill3DRectangle --
 *
 *	Reimplementation of Tree_Fill3DRectangle() because the clip
 *	region is ignored on Win32.
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
Tree_Fill3DRectangle(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    Tk_3DBorder border,		/* Token for border to draw. */
    int x, int y, int width, int height,
				/* Outside area of rectangular region. */
    int borderWidth,		/* Desired width for border, in pixels. Border
				 * will be *inside* region. */
    int relief			/* Indicates 3D effect: TK_RELIEF_FLAT,
				 * TK_RELIEF_RAISED, or TK_RELIEF_SUNKEN. */
    )
{
    Tk_Window tkwin = tree->tkwin;
    HDC dc;
    TkWinDCState dcState;
    TreeClipStateDC clipState;
    int doubleBorder;

    dc = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);
    TreeClip_ToDC(tree, clip, dc, &clipState);

    if (relief == TK_RELIEF_FLAT) {
	borderWidth = 0;
    } else {
	if (width < 2*borderWidth) {
	    borderWidth = width/2;
	}
	if (height < 2*borderWidth) {
	    borderWidth = height/2;
	}
    }
    doubleBorder = 2*borderWidth;

    if ((width > doubleBorder) && (height > doubleBorder)) {
	FastFillRect(dc,
		x + borderWidth, y + borderWidth,
		(unsigned int) (width - doubleBorder),
		(unsigned int) (height - doubleBorder),
		TkWinGetBorderPixels(tkwin, border, TK_3D_FLAT_GC));
    }

    TreeClip_FinishDC(&clipState);
    TkWinReleaseDrawableDC(td.drawable, dc, &dcState);

    if (borderWidth) {
	Tree_Draw3DRectangle(tree, td, clip, border, x, y, width,
		height, borderWidth, relief);
    }
}

#endif /* USE_ITEM_PIXMAP == 0 */

/*** Themes ***/

#include <uxtheme.h>
#ifdef __MINGW32__
#include <tmschema.h>
#else /* __MING32__ */
/* <vsstyle.h> is part of the Windows SDK but not the Platform SDK. */
/*#include <vsstyle.h>*/
#ifndef HP_HEADERITEM
#define HP_HEADERITEM 1
#define HIS_NORMAL 1
#define HIS_HOT 2
#define HIS_PRESSED 3
#define TVP_GLYPH 2
#define TVP_HOTGLYPH 4
#define GLPS_CLOSED 1
#define GLPS_OPENED 2
#define HGLPS_CLOSED 1
#define HGLPS_OPENED 2
#endif /* HP_HEADERITEM */
#endif /* __MING32__ */
#include <shlwapi.h>
#include <basetyps.h> /* MingW32 */

#ifdef __MINGW32__
/* vsstyle.h */
#define TVP_HOTGLYPH 4
#define HGLPS_CLOSED 1
#define HGLPS_OPENED 2
#endif

#ifndef TMT_CONTENTMARGINS
#define TMT_CONTENTMARGINS 3602
#endif

typedef HTHEME (STDAPICALLTYPE OpenThemeDataProc)(HWND hwnd,
    LPCWSTR pszClassList);
typedef HRESULT (STDAPICALLTYPE CloseThemeDataProc)(HTHEME hTheme);
typedef HRESULT (STDAPICALLTYPE DrawThemeBackgroundProc)(HTHEME hTheme,
    HDC hdc, int iPartId, int iStateId, const RECT *pRect,
    OPTIONAL const RECT *pClipRect);
typedef HRESULT (STDAPICALLTYPE DrawThemeBackgroundExProc)(HTHEME hTheme,
    HDC hdc, int iPartId, int iStateId, const RECT *pRect,
    DTBGOPTS *PDTBGOPTS);
typedef HRESULT (STDAPICALLTYPE DrawThemeParentBackgroundProc)(HWND hwnd,
    HDC hdc, OPTIONAL const RECT *prc);
typedef HRESULT (STDAPICALLTYPE DrawThemeEdgeProc)(HTHEME hTheme, HDC hdc,
    int iPartId, int iStateId, const RECT *pDestRect,
    UINT uEdge, UINT uFlags, RECT *pContentRect);
typedef HRESULT (STDAPICALLTYPE DrawThemeTextProc)(HTHEME hTheme, HDC hdc,
    int iPartId, int iStateId, LPCWSTR pszText, int iCharCount,
    DWORD dwTextFlags, DWORD dwTextFlags2, const RECT *pRect);
typedef HRESULT (STDAPICALLTYPE GetThemeBackgroundContentRectProc)(
    HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
    const RECT *pBoundingRect, RECT *pContentRect);
typedef HRESULT (STDAPICALLTYPE GetThemeBackgroundExtentProc)(HTHEME hTheme,
    HDC hdc, int iPartId, int iStateId, const RECT *pContentRect,
    RECT *pExtentRect);
typedef HRESULT (STDAPICALLTYPE GetThemeMarginsProc)(HTHEME, HDC,
    int iPartId, int iStateId, int iPropId, OPTIONAL RECT *prc,
    MARGINS *pMargins);
typedef HRESULT (STDAPICALLTYPE GetThemePartSizeProc)(HTHEME, HDC, int iPartId,
    int iStateId, RECT *prc, enum THEMESIZE eSize, SIZE *psz);
typedef HRESULT (STDAPICALLTYPE GetThemeTextExtentProc)(HTHEME hTheme, HDC hdc,
    int iPartId, int iStateId, LPCWSTR pszText, int iCharCount,
    DWORD dwTextFlags, const RECT *pBoundingRect, RECT *pExtentRect);
typedef BOOL (STDAPICALLTYPE IsThemeActiveProc)(VOID);
typedef BOOL (STDAPICALLTYPE IsAppThemedProc)(VOID);
typedef BOOL (STDAPICALLTYPE IsThemePartDefinedProc)(HTHEME, int, int);
typedef HRESULT (STDAPICALLTYPE IsThemeBackgroundPartiallyTransparentProc)(
    HTHEME, int, int);
typedef HRESULT (STDAPICALLTYPE SetWindowThemeProc)(HWND, LPCWSTR, LPCWSTR);

typedef struct
{
    OpenThemeDataProc				*OpenThemeData;
    CloseThemeDataProc				*CloseThemeData;
    DrawThemeBackgroundProc			*DrawThemeBackground;
    DrawThemeBackgroundExProc			*DrawThemeBackgroundEx;
    DrawThemeParentBackgroundProc		*DrawThemeParentBackground;
    DrawThemeEdgeProc				*DrawThemeEdge;
    DrawThemeTextProc				*DrawThemeText;
    GetThemeBackgroundContentRectProc		*GetThemeBackgroundContentRect;
    GetThemeBackgroundExtentProc		*GetThemeBackgroundExtent;
    GetThemeMarginsProc				*GetThemeMargins;
    GetThemePartSizeProc			*GetThemePartSize;
    GetThemeTextExtentProc			*GetThemeTextExtent;
    IsThemeActiveProc				*IsThemeActive;
    IsAppThemedProc				*IsAppThemed;
    IsThemePartDefinedProc			*IsThemePartDefined;
    IsThemeBackgroundPartiallyTransparentProc 	*IsThemeBackgroundPartiallyTransparent;
    SetWindowThemeProc				*SetWindowTheme;
} XPThemeProcs;

typedef struct
{
    HINSTANCE hlibrary;
    XPThemeProcs *procs;
    int registered;
    int themeEnabled;
} XPThemeData;

typedef struct TreeThemeData_
{
    HTHEME hThemeHEADER;
    HTHEME hThemeTREEVIEW;
    SIZE buttonOpen;
    SIZE buttonClosed;
} TreeThemeData_;

static XPThemeProcs *procs = NULL;
static XPThemeData *appThemeData = NULL; 
TCL_DECLARE_MUTEX(themeMutex)

/* Functions imported from kernel32.dll requiring windows XP or greater. */
/* But I already link to GetVersionEx so is this importing needed? */
typedef HANDLE (STDAPICALLTYPE CreateActCtxAProc)(PCACTCTXA pActCtx);
typedef BOOL (STDAPICALLTYPE ActivateActCtxProc)(HANDLE hActCtx, ULONG_PTR *lpCookie);
typedef BOOL (STDAPICALLTYPE DeactivateActCtxProc)(DWORD dwFlags, ULONG_PTR ulCookie);
typedef VOID (STDAPICALLTYPE ReleaseActCtxProc)(HANDLE hActCtx);

typedef struct
{
    CreateActCtxAProc *CreateActCtxA;
    ActivateActCtxProc *ActivateActCtx;
    DeactivateActCtxProc *DeactivateActCtx;
    ReleaseActCtxProc *ReleaseActCtx;
} ActCtxProcs;

static ActCtxProcs *
GetActCtxProcs(void)
{
    HINSTANCE hInst;
    ActCtxProcs *procs = (ActCtxProcs *) ckalloc(sizeof(ActCtxProcs));

    hInst = LoadLibrary("kernel32.dll"); /* FIXME: leak? */
    if (hInst != 0)
    {
 #define LOADPROC(name) \
	(0 != (procs->name = (name ## Proc *)GetProcAddress(hInst, #name) ))

	if (LOADPROC(CreateActCtxA) &&
	    LOADPROC(ActivateActCtx) &&
	    LOADPROC(DeactivateActCtx) &&
	    LOADPROC(ReleaseActCtx))
	{
	    return procs;
	}

#undef LOADPROC    
    }

    ckfree((char*)procs);
    return NULL;
}

static HMODULE thisModule = NULL;

/* Return the HMODULE for this treectrl.dll. */
static HMODULE
GetMyHandle(void)
{
#if 1
    return thisModule;
#else
    HMODULE hModule = NULL;

    /* FIXME: Only >=NT so I shouldn't link to it? But I already linked to
     * GetVersionEx so will it run on 95/98? */
    GetModuleHandleEx(
	GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
	(LPCTSTR)&appThemeData,
	&hModule);
    return hModule;
#endif
}

BOOL WINAPI
DllMain(
    HINSTANCE hInst,	/* Library instance handle. */
    DWORD reason,	/* Reason this function is being called. */
    LPVOID reserved)	/* Not used. */
{
    if (reason == DLL_PROCESS_ATTACH) {
	thisModule = (HMODULE) hInst;
    }
    return TRUE;
}

static HANDLE
ActivateManifestContext(ActCtxProcs *procs, ULONG_PTR *ulpCookie)
{
    ACTCTXA actctx;
    HANDLE hCtx;
#if 1
    char myPath[1024];
    DWORD len;

    if (procs == NULL)
	return INVALID_HANDLE_VALUE;

    len = GetModuleFileName(GetMyHandle(),myPath,1024);
    myPath[len] = 0;

    ZeroMemory(&actctx, sizeof(actctx));
    actctx.cbSize = sizeof(actctx);
    actctx.lpSource = myPath;
    actctx.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID;
#else

    if (procs == NULL)
	return INVALID_HANDLE_VALUE;

    ZeroMemory(&actctx, sizeof(actctx));
    actctx.cbSize = sizeof(actctx);
    actctx.dwFlags = ACTCTX_FLAG_HMODULE_VALID | ACTCTX_FLAG_RESOURCE_NAME_VALID;
    actctx.hModule = GetMyHandle();
#endif
    actctx.lpResourceName = MAKEINTRESOURCE(2);

    hCtx = procs->CreateActCtxA(&actctx);
    if (hCtx == INVALID_HANDLE_VALUE)
    {
	char msg[1024];
	DWORD err = GetLastError();
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS|
		FORMAT_MESSAGE_MAX_WIDTH_MASK, 0L, err, 0, (LPVOID)msg,
		sizeof(msg), 0);
	return INVALID_HANDLE_VALUE;
    }

    if (procs->ActivateActCtx(hCtx, ulpCookie))
	return hCtx;

    return INVALID_HANDLE_VALUE;
}

static void
DeactivateManifestContext(ActCtxProcs *procs, HANDLE hCtx, ULONG_PTR ulpCookie)
{
    if (procs == NULL)
	return;

    if (hCtx != INVALID_HANDLE_VALUE)
    {
	procs->DeactivateActCtx(0, ulpCookie);
	procs->ReleaseActCtx(hCtx);
    }

    ckfree((char*)procs);
}

/* http://www.manbu.net/Lib/En/Class5/Sub16/1/29.asp */
static int
ComCtlVersionOK(void)
{
    HINSTANCE handle;
    typedef HRESULT (STDAPICALLTYPE DllGetVersionProc)(DLLVERSIONINFO *);
    DllGetVersionProc *pDllGetVersion;
    int result = FALSE;
    ActCtxProcs *procs;
    HANDLE hCtx;
    ULONG_PTR ulpCookie;

    procs = GetActCtxProcs();
    hCtx = ActivateManifestContext(procs, &ulpCookie);
    handle = LoadLibrary("comctl32.dll");
    DeactivateManifestContext(procs, hCtx, ulpCookie);
    if (handle == NULL)
	return FALSE;
    pDllGetVersion = (DllGetVersionProc *) GetProcAddress(handle,
	    "DllGetVersion");
    if (pDllGetVersion != NULL) {
	DLLVERSIONINFO dvi;

	memset(&dvi, '\0', sizeof(dvi));
	dvi.cbSize = sizeof(dvi);
	if ((*pDllGetVersion)(&dvi) == NOERROR)
	    result = dvi.dwMajorVersion >= 6;
    }
    FreeLibrary(handle);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * LoadXPThemeProcs --
 *	Initialize XP theming support.
 *
 *	XP theme support is included in UXTHEME.DLL
 *	We dynamically load this DLL at runtime instead of linking
 *	to it at build-time.
 *
 * Returns:
 *	A pointer to an XPThemeProcs table if successful, NULL otherwise.
 *----------------------------------------------------------------------
 */

static XPThemeProcs *
LoadXPThemeProcs(HINSTANCE *phlib)
{
    OSVERSIONINFO os;

    /*
     * We have to check whether we are running at least on Windows XP.
     * In order to determine this we call GetVersionEx directly, although
     * it would be a good idea to wrap it inside a function similar to
     * TkWinGetPlatformId...
     */
    ZeroMemory(&os, sizeof(os));
    os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&os);
    if ((os.dwMajorVersion >= 5 && os.dwMinorVersion >= 1) ||
	    (os.dwMajorVersion > 5)) {
	/*
	 * We are running under Windows XP or a newer version.
	 * Load the library "uxtheme.dll", where the native widget
	 * drawing routines are implemented.
	 */
	HINSTANCE handle;
	*phlib = handle = LoadLibrary("uxtheme.dll");
	if (handle != 0) {
	    /*
	     * We have successfully loaded the library. Proceed in storing the
	     * addresses of the functions we want to use.
	     */
	    XPThemeProcs *procs = (XPThemeProcs*)ckalloc(sizeof(XPThemeProcs));
#define LOADPROC(name) \
	(0 != (procs->name = (name ## Proc *)GetProcAddress(handle, #name) ))

	    if (   LOADPROC(OpenThemeData)
		&& LOADPROC(CloseThemeData)
		&& LOADPROC(DrawThemeBackground)
		&& LOADPROC(DrawThemeBackgroundEx)
		&& LOADPROC(DrawThemeParentBackground)
		&& LOADPROC(DrawThemeEdge)
		&& LOADPROC(DrawThemeText)
		&& LOADPROC(GetThemeBackgroundContentRect)
		&& LOADPROC(GetThemeBackgroundExtent)
		&& LOADPROC(GetThemeMargins)
		&& LOADPROC(GetThemePartSize)
		&& LOADPROC(GetThemeTextExtent)
		&& LOADPROC(IsThemeActive)
		&& LOADPROC(IsAppThemed)
		&& LOADPROC(IsThemePartDefined)
		&& LOADPROC(IsThemeBackgroundPartiallyTransparent)
		&& LOADPROC(SetWindowTheme)
		&& ComCtlVersionOK()
	    ) {
		return procs;
	    }
#undef LOADPROC
	    ckfree((char*)procs);
	}
    }
    return 0;
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
    HTHEME hTheme;
    HDC hDC;
    TkWinDCState dcState;
    RECT rc;
    HRESULT hr;

    int iPartId = HP_HEADERITEM;
    int iStateId = HIS_NORMAL;

    switch (state) {
	case COLUMN_STATE_ACTIVE:  iStateId = HIS_HOT; break;
	case COLUMN_STATE_PRESSED: iStateId = HIS_PRESSED; break;
    }

    if (!appThemeData->themeEnabled || !procs)
	return TCL_ERROR;

    hTheme = tree->themeData->hThemeHEADER;
    if (!hTheme)
	return TCL_ERROR;

#if 0 /* Always returns FALSE */
    if (!procs->IsThemePartDefined(
	hTheme,
	iPartId,
	iStateId)) {
	return TCL_ERROR;
    }
#endif

    rc.left = x;
    rc.top = y;
    rc.right = x + width;
    rc.bottom = y + height;

    /* Is transparent for the default XP style. */
    if (procs->IsThemeBackgroundPartiallyTransparent(
	hTheme,
	iPartId,
	iStateId)) {
#if 1
	/* What color should I use? */
	Tk_Fill3DRectangle(tree->tkwin, td.drawable, tree->border, x, y, width, height, 0, TK_RELIEF_FLAT);
#else
	/* This draws nothing, maybe because the parent window is not
	 * themed */
	procs->DrawThemeParentBackground(
	    hwnd,
	    hDC,
	    &rc);
#endif
    }

    hDC = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);

#if 0
    {
	/* Default XP theme gives rect 3 pixels narrower than rc */
	RECT contentRect, extentRect;
	hr = procs->GetThemeBackgroundContentRect(
	    hTheme,
	    hDC,
	    iPartId,
	    iStateId,
	    &rc,
	    &contentRect
	);
	dbwin("GetThemeBackgroundContentRect width=%d height=%d\n",
	    contentRect.right - contentRect.left,
	    contentRect.bottom - contentRect.top);

	/* Gives rc */
	hr = procs->GetThemeBackgroundExtent(
	    hTheme,
	    hDC,
	    iPartId,
	    iStateId,
	    &contentRect,
	    &extentRect
	);
	dbwin("GetThemeBackgroundExtent width=%d height=%d\n",
	    extentRect.right - extentRect.left,
	    extentRect.bottom - extentRect.top);
    }
#endif

    hr = procs->DrawThemeBackground(
	hTheme,
	hDC,
	iPartId,
	iStateId,
	&rc,
	NULL);

    TkWinReleaseDrawableDC(td.drawable, hDC, &dcState);

    if (hr != S_OK)
	return TCL_ERROR;

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
    Window win = Tk_WindowId(tree->tkwin);
    HTHEME hTheme;
    HDC hDC;
    TkWinDCState dcState;
    HRESULT hr;
    MARGINS margins;

    int iPartId = HP_HEADERITEM;
    int iStateId = HIS_NORMAL;

    switch (state) {
	case COLUMN_STATE_ACTIVE:  iStateId = HIS_HOT; break;
	case COLUMN_STATE_PRESSED: iStateId = HIS_PRESSED; break;
    }

    if (!appThemeData->themeEnabled || !procs)
	return TCL_ERROR;

    hTheme = tree->themeData->hThemeHEADER;
    if (!hTheme)
	return TCL_ERROR;

    hDC = TkWinGetDrawableDC(tree->display, win, &dcState);

    /* The default XP themes give 3,0,0,0 which makes little sense since
     * it is the *right* side that should not be drawn over by text; the
     * 2-pixel wide header divider is on the right */
    hr = procs->GetThemeMargins(
	hTheme,
	hDC,
	iPartId,
	iStateId,
	TMT_CONTENTMARGINS,
	NULL,
	&margins);

    TkWinReleaseDrawableDC(win, hDC, &dcState);

    if (hr != S_OK)
	return TCL_ERROR;

    bounds[0] = margins.cxLeftWidth;
    bounds[1] = margins.cyTopHeight;
    bounds[2] = margins.cxRightWidth;
    bounds[3] = margins.cyBottomHeight;
/*
dbwin("margins %d %d %d %d\n", bounds[0], bounds[1], bounds[2], bounds[3]);
*/
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
#define THEME_ARROW 0
#if THEME_ARROW==0
    XColor *color;
    GC gc;
    int i;

    if (!appThemeData->themeEnabled || !procs)
	return TCL_ERROR;

    color = Tk_GetColor(tree->interp, tree->tkwin, "#ACA899");
    gc = Tk_GCForColor(color, td.drawable);

    if (up) {
	for (i = 0; i < height; i++) {
	    XDrawLine(tree->display, td.drawable, gc,
		x + width / 2 - i, y + i,
		x + width / 2 + i + 1, y + i);
	}
    } else {
	for (i = 0; i < height; i++) {
	    XDrawLine(tree->display, td.drawable, gc,
		x + width / 2 - i, y + (height - 1) - i,
		x + width / 2 + i + 1, y + (height - 1) - i);
	}
    }

    Tk_FreeColor(color);
    return TCL_OK;
#else
    /* Doesn't seem that Microsoft actually implemented this */
    Window win = Tk_WindowId(tree->tkwin);
    HWND hwnd = Tk_GetHWND(win);
    HTHEME hTheme;
    HDC hDC;
    TkWinDCState dcState;
    RECT rc;
    HRESULT hr;

    int iPartId = HP_HEADERSORTARROW;
    int iStateId = up ? HSAS_SORTEDUP : HSAS_SORTEDDOWN;

    if (!appThemeData->themeEnabled || !procs)
	return TCL_ERROR;

    hTheme = tree->themeData->hThemeHEADER;
    if (!hTheme)
	return TCL_ERROR;

    if (!procs->IsThemePartDefined(
	hTheme,
	iPartId,
	iStateId)) {
	return TCL_ERROR;
    }

    hDC = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);

    rc.left = x;
    rc.top = y;
    rc.right = x + width;
    rc.bottom = y + height;

    hr = procs->DrawThemeBackground(
	hTheme,
	hDC,
	iPartId,
	iStateId,
	&rc,
	NULL);

    TkWinReleaseDrawableDC(td.drawable, hDC, &dcState);
    return TCL_OK;
#endif /* THEME_ARROW==1 */
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
    int open = state & STATE_OPEN;
    int active = state & (BUTTON_STATE_ACTIVE|BUTTON_STATE_PRESSED); /* windows theme has no "pressed" state */
    HTHEME hTheme;
    HDC hDC;
    TkWinDCState dcState;
    RECT rc;
    HRESULT hr;
    int iPartId, iStateId;

    if (!appThemeData->themeEnabled || !procs)
	return TCL_ERROR;

    hTheme = tree->themeData->hThemeTREEVIEW;
    if (!hTheme)
	return TCL_ERROR;

    /* On Win7 IsThemePartDefined(TVP_HOTGLYPH) correctly returns
     * TRUE when SetWindowTheme("explorer") is called and FALSE when it
     * wasn't called. */
    if (active && procs->IsThemePartDefined(hTheme, TVP_HOTGLYPH, 0)) {
	iPartId  = TVP_HOTGLYPH;
	iStateId = open ? HGLPS_OPENED : HGLPS_CLOSED;
    } else {
	iPartId  = TVP_GLYPH;
	iStateId = open ? GLPS_OPENED : GLPS_CLOSED;
    }

#if 0 /* Always returns FALSE */
    if (!procs->IsThemePartDefined(
	hTheme,
	iPartId,
	iStateId)) {
	return TCL_ERROR;
    }
#endif

    hDC = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);

    rc.left = x;
    rc.top = y;
    rc.right = x + width;
    rc.bottom = y + height;
    hr = procs->DrawThemeBackground(
	hTheme,
	hDC,
	iPartId,
	iStateId,
	&rc,
	NULL);

    TkWinReleaseDrawableDC(td.drawable, hDC, &dcState);

    if (hr != S_OK)
	return TCL_ERROR;

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
    TreeThemeData themeData = tree->themeData;
    HTHEME hTheme;
    HDC hDC;
    TkWinDCState dcState;
    HRESULT hr;
    SIZE size;
    int iPartId, iStateId;

    if (!appThemeData->themeEnabled || !procs)
	return TCL_ERROR;

    /* Use cached values */
    size = open ? themeData->buttonOpen : themeData->buttonClosed;
    if (size.cx > 1) {
	*widthPtr = size.cx;
	*heightPtr = size.cy;
	return TCL_OK;
    }

    hTheme = themeData->hThemeTREEVIEW;
    if (!hTheme)
	return TCL_ERROR;

    iPartId  = TVP_GLYPH;
    iStateId = open ? GLPS_OPENED : GLPS_CLOSED;

#if 0 /* Always returns FALSE */
    if (!procs->IsThemePartDefined(
	hTheme,
	iPartId,
	iStateId)) {
	return TCL_ERROR;
    }
#endif

    hDC = TkWinGetDrawableDC(tree->display, drawable, &dcState);

    /* Returns 9x9 for default XP style */
    hr = procs->GetThemePartSize(
	hTheme,
	hDC,
	iPartId,
	iStateId,
	NULL,
	TS_DRAW,
	&size
    );

    TkWinReleaseDrawableDC(drawable, hDC, &dcState);

    /* With RandomN of 10000, I eventually get hr=E_HANDLE, invalid handle */
    /* Not any longer since I don't call OpenThemeData/CloseThemeData for
     * every call. */
    if (hr != S_OK)
	return TCL_ERROR;

    /* Gave me 0,0 for a non-default theme, even though glyph existed */
    if ((size.cx <= 1) && (size.cy <= 1))
	return TCL_ERROR;

    /* Cache the values */
    if (open)
	themeData->buttonOpen = size;
    else
	themeData->buttonClosed = size;

    *widthPtr = size.cx;
    *heightPtr = size.cy;
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
#if THEME_ARROW==0
    if (!appThemeData->themeEnabled || !procs)
	return TCL_ERROR;

    *widthPtr = 9;
    *heightPtr = 5;

    return TCL_OK;
#else
    TreeThemeData themeData = tree->themeData;
    HTHEME hTheme;
    HDC hDC;
    TkWinDCState dcState;
    HRESULT hr;
    SIZE size;
    int iPartId, iStateId;

    if (!appThemeData->themeEnabled || !procs)
	return TCL_ERROR;

    hTheme = themeData->hThemeTREEVIEW;
    if (!hTheme)
	return TCL_ERROR;

    iPartId = HP_HEADERSORTARROW;
    iStateId = up ? HSAS_SORTEDUP : HSAS_SORTEDDOWN;

#if 0 /* Always returns FALSE */
    if (!procs->IsThemePartDefined(
	hTheme,
	iPartId,
	iStateId)) {
	return TCL_ERROR;
    }
#endif

    hDC = TkWinGetDrawableDC(tree->display, drawable, &dcState);

    hr = procs->GetThemePartSize(
	hTheme,
	hDC,
	iPartId,
	iStateId,
	NULL,
	TS_DRAW,
	&size
    );

    TkWinReleaseDrawableDC(drawable, hDC, &dcState);

    if (hr != S_OK)
	return TCL_ERROR;

    if ((size.cx <= 1) && (size.cy <= 1))
	return TCL_ERROR;

    *widthPtr = size.cx;
    *heightPtr = size.cy;

    return TCL_OK;
#endif /* THEME_ARROW==1 */
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
    TreeItem_OpenClose(tree, item, -1);
#ifdef SELECTION_VISIBLE
    Tree_DeselectHidden(tree);
#endif
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
    /* TODO:
	Detect Vista/Win7 use of Desktop Window Manager using
	  DwmIsCompositionEnabled().
	WndProc should listen for WM_DWMCOMPOSITIONCHANGED.
    */
#if 1
    /* On Win7 I see lots of flickering with the dragimage in the demo
     * "Explorer (Large Icons)", so Composition must not work quite how I
     * expected. */
    return FALSE;
#elif 0
    HMODULE library = LoadLibrary("dwmapi.dll");
    int result = FALSE;

    if (0 != library) {
	typedef BOOL (STDAPICALLTYPE DwmIsCompositionEnabledProc)(BOOL *pfEnabled);
	DwmIsCompositionEnabledProc *proc;

	if (0 != (proc = GetProcAddress(library, "DwmIsCompositionEnabled"))) {
	    BOOL enabled = FALSE;
	    result = SUCCEEDED(proc(&enabled)) && enabled;
	}

	FreeLibrary(library);
    }

    return result;
#else
/* http://weblogs.asp.net/kennykerr/archive/2006/08/10/Windows-Vista-for-Developers-_1320_-Part-3-_1320_-The-Desktop-Window-Manager.aspx */
bool IsCompositionEnabled()
{
    HMODULE library = ::LoadLibrary(L"dwmapi.dll");
    bool result = false;

    if (0 != library)
    {
        if (0 != ::GetProcAddress(library,
                                  "DwmIsCompositionEnabled"))
        {
            BOOL enabled = FALSE;
            result = SUCCEEDED(::DwmIsCompositionEnabled(&enabled)) && enabled;
        }

        VERIFY(::FreeLibrary(library));
    }

    return result;
}
#endif
    return FALSE;
}

#if !defined(WM_THEMECHANGED)
#define WM_THEMECHANGED 0x031A
#endif

static LRESULT WINAPI
ThemeMonitorWndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wp,
    LPARAM lp)
{
    Tcl_Interp *interp = (Tcl_Interp *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
	case WM_THEMECHANGED:
	    Tcl_MutexLock(&themeMutex);
	    appThemeData->themeEnabled = procs->IsThemeActive() &&
		    procs->IsAppThemed();
	    Tcl_MutexUnlock(&themeMutex);
	    Tree_TheWorldHasChanged(interp);
	    /* FIXME: must get tree->themeData->hThemeHEADER etc for each widget */
	    break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static CHAR windowClassName[32] = "TreeCtrlMonitorClass";

static BOOL
RegisterThemeMonitorWindowClass(
    HINSTANCE hinst)
{
    WNDCLASSEX wc;
    
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = (WNDPROC) ThemeMonitorWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hinst;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) COLOR_WINDOW;
    wc.lpszMenuName  = windowClassName;
    wc.lpszClassName = windowClassName;
    
    return RegisterClassEx(&wc);
}

static HWND
CreateThemeMonitorWindow(
    HINSTANCE hinst,
    Tcl_Interp *interp)
{
    CHAR title[32] = "TreeCtrlMonitorWindow";
    HWND hwnd;

    hwnd = CreateWindow(windowClassName, title, WS_OVERLAPPEDWINDOW,
	CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
	NULL, NULL, hinst, NULL);
    if (!hwnd)
	return NULL;

    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)interp);
    ShowWindow(hwnd, SW_HIDE);
    UpdateWindow(hwnd);

    return hwnd;
}

typedef struct PerInterpData PerInterpData;
struct PerInterpData
{
    HWND hwnd;
};

static void
ThemeFreeAssocData(
    ClientData clientData,
    Tcl_Interp *interp)
{
    PerInterpData *data = (PerInterpData *) clientData;

    DestroyWindow(data->hwnd);
    ckfree((char *) data);
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
    Window win = Tk_WindowId(tree->tkwin);
    HWND hwnd = Tk_GetHWND(win);

    if (tree->themeData != NULL) {
	if (tree->themeData->hThemeHEADER != NULL) {
	    procs->CloseThemeData(tree->themeData->hThemeHEADER);
	    tree->themeData->hThemeHEADER = NULL;
	}
	if (tree->themeData->hThemeTREEVIEW != NULL) {
	    procs->CloseThemeData(tree->themeData->hThemeTREEVIEW);
	    tree->themeData->hThemeTREEVIEW = NULL;
	}
    }

    if (!appThemeData->themeEnabled || !procs)
	return;

    if (tree->themeData == NULL)
	tree->themeData = (TreeThemeData) ckalloc(sizeof(TreeThemeData_));

    tree->themeData->hThemeHEADER = procs->OpenThemeData(hwnd, L"HEADER");
    tree->themeData->hThemeTREEVIEW = procs->OpenThemeData(hwnd, L"TREEVIEW");

    tree->themeData->buttonClosed.cx = tree->themeData->buttonOpen.cx = -1;
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
    Window win = Tk_WindowId(tree->tkwin);
    HWND hwnd = Tk_GetHWND(win);

    if (!appThemeData->themeEnabled || !procs)
	return TCL_ERROR;

    tree->themeData = (TreeThemeData) ckalloc(sizeof(TreeThemeData_));

    /* http://www.codeproject.com/cs/miscctrl/themedtabpage.asp?msg=1445385#xx1445385xx */
    /* http://msdn2.microsoft.com/en-us/library/ms649781.aspx */

    tree->themeData->hThemeHEADER = procs->OpenThemeData(hwnd, L"HEADER");
    tree->themeData->hThemeTREEVIEW = procs->OpenThemeData(hwnd, L"TREEVIEW");

    tree->themeData->buttonClosed.cx = tree->themeData->buttonOpen.cx = -1;

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
	if (tree->themeData->hThemeHEADER != NULL)
	    procs->CloseThemeData(tree->themeData->hThemeHEADER);
	if (tree->themeData->hThemeTREEVIEW != NULL)
	    procs->CloseThemeData(tree->themeData->hThemeTREEVIEW);
	ckfree((char *) tree->themeData);
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
    HWND hwnd;
    PerInterpData *data;

    Tcl_MutexLock(&themeMutex);

    /* This is done once per-application */
    if (appThemeData == NULL) {
	appThemeData = (XPThemeData *) ckalloc(sizeof(XPThemeData));
	appThemeData->procs = LoadXPThemeProcs(&appThemeData->hlibrary);
	appThemeData->registered = FALSE;
	appThemeData->themeEnabled = FALSE;

	procs = appThemeData->procs;

	if (appThemeData->procs) {
	    /* Check this again if WM_THEMECHANGED arrives */
	    appThemeData->themeEnabled = procs->IsThemeActive() &&
		    procs->IsAppThemed();

	    appThemeData->registered =
		RegisterThemeMonitorWindowClass(Tk_GetHINSTANCE());
	}
    }

    Tcl_MutexUnlock(&themeMutex);

    if (!procs || !appThemeData->registered)
	return TCL_ERROR;

    /* Per-interp */
    hwnd = CreateThemeMonitorWindow(Tk_GetHINSTANCE(), interp);
    if (!hwnd)
	return TCL_ERROR;

    data = (PerInterpData *) ckalloc(sizeof(PerInterpData));
    data->hwnd = hwnd;
    Tcl_SetAssocData(interp, "TreeCtrlTheme", ThemeFreeAssocData, (ClientData) data);

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
	specPtr->defValue = "0";
    else if (!strcmp(specPtr->optionName, "-showlines"))
	specPtr->defValue = "1";

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
	"platform", "setwindowtheme", (char *) NULL
    };
    enum {
	COMMAND_PLATFORM, COMMAND_SETWINDOWTHEME
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
	    char *platform = "X11"; /* X11, xlib, whatever */
	    if (appThemeData->themeEnabled && appThemeData->procs)
		platform = "visualstyles";
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(platform, -1));
	    break;
	}
	/* T theme setwindowtheme $appname */
	case COMMAND_SETWINDOWTHEME: {
	    LPCWSTR pszSubAppName; /* L"Explorer" for example */
	    int length;
	    Window win;
	    HWND hwnd;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "appname");
		return TCL_ERROR;
	    }
	    if (!appThemeData->themeEnabled || !appThemeData->procs)
		break;
	    win = Tk_WindowId(tree->tkwin);
	    hwnd = Tk_GetHWND(win);
	    pszSubAppName = Tcl_GetUnicodeFromObj(objv[3], &length);
	    procs->SetWindowTheme(hwnd, length ? pszSubAppName : NULL, NULL);

	    /* uxtheme.h says a WM_THEMECHANGED is sent to the window. */
	    /* FIXME: only this window needs to be updated. */
	    /* This calls TreeTheme_ThemeChanged which is needed. */
	    Tree_TheWorldHasChanged(tree->interp);
	    break;
	}
    }

    return TCL_OK;
}

/*** Gradients ***/

/*
 * GDI+ flat api
 */

/* gdiplus.h is a C++ header file with MSVC. */
/* gdiplus.h works with C and C++ with MinGW. */
/* However gdiplus.h is not included with MinGW-w64 for some reason and
 * also does not come with the Linux i586-mingw32msvc cross-compiler. */
#if 1
#define WINGDIPAPI __stdcall
#define GDIPCONST const
#define VOID void
typedef float REAL;
typedef enum CombineMode {
    CombineModeReplace = 0
} CombineMode;
typedef enum GpFillMode
{
    FillModeAlternate,
    FillModeWinding
} GpFillMode;
typedef enum GpLineCap {
    LineCapSquare = 1
} GpLineCap;
typedef enum GpUnit {
    UnitWorld = 0,
    UnitDisplay = 1,
    UnitPixel = 2,
    UnitPoint = 3,
    UnitInch = 4,
    UnitDocument = 5,
    UnitMillimeter = 6
} GpUnit;
typedef enum GpStatus {
    Ok = 0
} GpStatus;
typedef enum GpWrapMode
{
    WrapModeTile = 0, /* repeat */
    WrapModeTileFlipXY = 3 /* reflect */
} GpWrapMode;
typedef enum LinearGradientMode
{
    LinearGradientModeHorizontal,
    LinearGradientModeVertical
} LinearGradientMode;
typedef enum GpPenAlignment {
    PenAlignmentCenter = 0,
    PenAlignmentInset = 1
} GpPenAlignment;
typedef enum SmoothingMode {
    SmoothingModeHighQuality = 2,
    SmoothingModeAntiAlias = 4
} SmoothingMode;
typedef struct GdiplusStartupInput
{
    UINT32 GdiplusVersion;
    /*DebugEventProc*/VOID* DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} GdiplusStartupInput;
typedef struct GdiplusStartupOutput
{
    /*NotificationHookProc*/VOID* NotificationHook;
    /*NotificationUnhookProc*/VOID* NotificationUnhook;
} GdiplusStartupOutput;
typedef struct GpPoint {
    INT X;
    INT Y;
} GpPoint;
typedef struct GpPointF {
    REAL X;
    REAL Y;
} GpPointF;
typedef struct GpRect {
    INT X;
    INT Y;
    INT Width;
    INT Height;
} GpRect;
typedef DWORD ARGB;
typedef void GpBrush;
typedef void GpGraphics;
typedef void GpLineGradient;
typedef void GpPath;
typedef void GpPen;
typedef void GpSolidFill;
#endif

/* After gdiplus.dll is dynamically loaded, this structure is
 * filled in with pointers to functions that are used below. */
static struct
{
    HMODULE handle; /* gdiplus.dll */

    VOID* (WINGDIPAPI *_GdipAlloc)(size_t);
    VOID (WINGDIPAPI *_GdipFree)(VOID*);

    GpStatus (WINGDIPAPI *_GdiplusStartup)(ULONG_PTR*,GDIPCONST GdiplusStartupInput*,GdiplusStartupOutput*);
    VOID (WINGDIPAPI *_GdiplusShutdown)(ULONG_PTR);

    /* Graphics */
    GpStatus (WINGDIPAPI *_GdipCreateFromHDC)(HDC,GpGraphics**);
    GpStatus (WINGDIPAPI *_GdipFillRectangleI)(GpGraphics*,GpBrush*,INT,INT,INT,INT);
    GpStatus (WINGDIPAPI *_GdipDeleteGraphics)(GpGraphics*);
    GpStatus (WINGDIPAPI *_GdipDrawPath)(GpGraphics*,GpPen*,GpPath*);
    GpStatus (WINGDIPAPI *_GdipFillPath)(GpGraphics*,GpBrush*,GpPath*);
    GpStatus (WINGDIPAPI *_GdipSetClipHrgn)(GpGraphics*,HRGN,CombineMode);
    GpStatus (WINGDIPAPI *_GdipSetClipPath)(GpGraphics*,GpPath*,CombineMode);
    GpStatus (WINGDIPAPI *_GdipSetClipRectI)(GpGraphics*,INT,INT,INT,INT,CombineMode);
    GpStatus (WINGDIPAPI *_GdipSetSmoothingMode)(GpGraphics*,SmoothingMode);

    /* GraphicsPath */
    GpStatus (WINGDIPAPI *_GdipCreatePath)(GpFillMode,GpPath**);
    GpStatus (WINGDIPAPI *_GdipDeletePath)(GpPath*);
    GpStatus (WINGDIPAPI *_GdipResetPath)(GpPath*);
    GpStatus (WINGDIPAPI *_GdipAddPathArc)(GpPath*,REAL,REAL,REAL,REAL,REAL,REAL);
    GpStatus (WINGDIPAPI *_GdipAddPathLine)(GpPath*,REAL,REAL,REAL,REAL);
    GpStatus (WINGDIPAPI *_GdipAddPathRectangle)(GpPath*,REAL,REAL,REAL,REAL);
    GpStatus (WINGDIPAPI *_GdipStartPathFigure)(GpPath*);
    GpStatus (WINGDIPAPI *_GdipClosePathFigure)(GpPath*);

    /* Linear Gradient brush */
    GpStatus (WINGDIPAPI *_GdipCreateLineBrushFromRectI)(GDIPCONST GpRect*,ARGB,ARGB,LinearGradientMode,GpWrapMode,GpLineGradient**);
    GpStatus (WINGDIPAPI *_GdipSetLinePresetBlend)(GpLineGradient*,GDIPCONST ARGB*,GDIPCONST REAL*,INT);
    GpStatus (WINGDIPAPI *_GdipDeleteBrush)(GpBrush*);

    /* Pen */
    GpStatus (WINGDIPAPI *_GdipCreatePen1)(ARGB,REAL,GpUnit,GpPen**);
    GpStatus (WINGDIPAPI *_GdipCreatePen2)(GpBrush*,REAL,GpUnit,GpPen**);
    GpStatus (WINGDIPAPI *_GdipSetPenEndCap)(GpPen*,GpLineCap);
    GpStatus (WINGDIPAPI *_GdipSetPenStartCap)(GpPen*,GpLineCap);
    GpStatus (WINGDIPAPI *_GdipSetPenMode)(GpPen*,GpPenAlignment);
    GpStatus (WINGDIPAPI *_GdipDeletePen)(GpPen*);

    /* SolidFill brush */
    GpStatus (WINGDIPAPI *_GdipCreateSolidFill)(ARGB,GpSolidFill**);

} DllExports = {0};

/* Per-application global data */
typedef struct
{
    ULONG_PTR token;			/* Result of GdiplusStartup() */
#if 0
    GdiplusStartupOutput output;	/* Result of GdiplusStartup() */
#endif
} TreeDrawAppData;

static TreeDrawAppData *appDrawData = NULL;

/* Tcl_CreateExitHandler() callback that shuts down GDI+ */
static void
TreeDraw_ExitHandler(
    ClientData clientData
    )
{
    if (appDrawData != NULL) {
	if (DllExports.handle != NULL)
	    DllExports._GdiplusShutdown(appDrawData->token);
    }
}

/* Load gdiplus.dll (if it exists) and fill in the DllExports global */
/* If gdiplus.dll can't be loaded DllExports.handle is set to NULL which
 * should be checked to test whether GDI+ can be used. */
static int
LoadGdiplus(void)
{
    DllExports.handle = LoadLibrary("gdiplus.dll");
    if (DllExports.handle != NULL) {
#define LOADPROC(name) \
	(0 != (DllExports._ ## name = (VOID *)GetProcAddress(DllExports.handle, #name) ))
	if (   LOADPROC(GdipAlloc)
	    && LOADPROC(GdipFree)
	    && LOADPROC(GdiplusStartup)
	    && LOADPROC(GdiplusShutdown)
	    && LOADPROC(GdipCreateFromHDC)
	    && LOADPROC(GdipFillRectangleI)
	    && LOADPROC(GdipDeleteGraphics)
	    && LOADPROC(GdipDrawPath)
	    && LOADPROC(GdipFillPath)
	    && LOADPROC(GdipSetClipHrgn)
	    && LOADPROC(GdipSetClipPath)
	    && LOADPROC(GdipSetClipRectI)
	    && LOADPROC(GdipSetSmoothingMode)
	    && LOADPROC(GdipCreatePath)
	    && LOADPROC(GdipDeletePath)
	    && LOADPROC(GdipResetPath)
	    && LOADPROC(GdipAddPathArc)
	    && LOADPROC(GdipAddPathLine)
	    && LOADPROC(GdipAddPathRectangle)
	    && LOADPROC(GdipStartPathFigure)
	    && LOADPROC(GdipClosePathFigure)
	    && LOADPROC(GdipCreateLineBrushFromRectI)
	    && LOADPROC(GdipSetLinePresetBlend)
	    && LOADPROC(GdipDeleteBrush)
	    && LOADPROC(GdipCreatePen1)
	    && LOADPROC(GdipCreatePen2)
	    && LOADPROC(GdipSetPenEndCap)
	    && LOADPROC(GdipSetPenStartCap)
	    && LOADPROC(GdipSetPenMode)
	    && LOADPROC(GdipDeletePen)
	    && LOADPROC(GdipCreateSolidFill)
	) {
	    return 1;
	}
#undef LOADPROC
    }
    DllExports.handle = NULL;
    return 0;
}

/* Per-interp init */
int
TreeDraw_InitInterp(
    Tcl_Interp *interp
    )
{
    /* This is done once per-application */
    if (appDrawData == NULL) {
	appDrawData = (TreeDrawAppData *) ckalloc(sizeof(TreeDrawAppData));
	memset(appDrawData, '\0', sizeof(TreeDrawAppData));
	if (LoadGdiplus()) {
	    GdiplusStartupInput input;
	    GpStatus status;
	    input.GdiplusVersion = 1;
	    input.DebugEventCallback = NULL;
	    input.SuppressBackgroundThread = FALSE;
	    input.SuppressExternalCodecs = FALSE;
	    /* Not sure what happens when the main application or other
	     * DLLs also call this, probably it is okay. */
	    status = DllExports._GdiplusStartup(&appDrawData->token, &input,
#if 1
		NULL);
#else
		&appDrawData->output);
#endif
	    if (status != Ok) {
		DllExports.handle = NULL;
	    }
	}
	Tcl_CreateExitHandler(TreeDraw_ExitHandler, NULL);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_HasNativeGradients --
 *
 *	Determine if this platform supports gradients natively.
 *
 * Results:
 *	1 if GDI+ is available,
 *	0 otherwise.
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
    return (DllExports.handle != NULL);
}

/* ARGB is a DWORD color used by GDI+ */
static ARGB MakeARGB(BYTE a, BYTE r, BYTE g, BYTE b)
{
    return (ARGB) ((((DWORD) a) << 24) | (((DWORD) r) << 16)
		 | (((DWORD) g) << 8) | ((DWORD) b));
}

static ARGB MakeGDIPlusColor(XColor *xc, double opacity)
{
    return MakeARGB(
	(BYTE)(opacity*255),
	(BYTE)((xc)->pixel & 0xFF),
	(BYTE)(((xc)->pixel >> 8) & 0xFF),
	(BYTE)(((xc)->pixel >> 16) & 0xFF));
}

typedef struct {
    TreeCtrl *tree;
    TreeClip *clip;
    GpGraphics *graphics;
} TreeClipStateGraphics;

static GpStatus
TreeClip_ToGraphics(
    TreeCtrl *tree,
    TreeClip *clip,
    GpGraphics *graphics,
    TreeClipStateGraphics *state
    )
{
    GpStatus status = Ok;

    state->tree = tree;
    state->clip = clip;
    state->graphics = graphics;

    if (clip && clip->type == TREE_CLIP_RECT) {
	status = DllExports._GdipSetClipRectI(graphics,
	    clip->tr.x, clip->tr.y, clip->tr.width, clip->tr.height,
	    CombineModeReplace);
    }
    if (clip && clip->type == TREE_CLIP_AREA) {
	TreeRectangle tr;
	if (Tree_AreaBbox(tree, clip->area, &tr) == 0) {
	    TreeRect_SetXYWH(tr, 0, 0, 0, 0);
	}
	status = DllExports._GdipSetClipRectI(graphics,
	    TreeRect_Left(tr), TreeRect_Top(tr),
	    TreeRect_Width(tr), TreeRect_Height(tr),
	    CombineModeReplace);
    }
    if (clip && clip->type == TREE_CLIP_REGION) {
	status = DllExports._GdipSetClipHrgn(graphics, (HRGN) clip->region,
	    CombineModeReplace);
    }

    return status;
}

static GpStatus
MakeLinearGradientBrush(
    TreeGradient gradient,	/* Gradient token. */
    TreeRectangle trBrush,	/* Brush bounds. */
    GpLineGradient **lgPtr	/* Result. */
    )
{
    GpLineGradient *lineGradient;
    GpStatus status;
    GpRect rect;
    GradientStop *stop;
    int i, nstops;
    ARGB color1, color2;

    (*lgPtr) = NULL;

    nstops = gradient->stopArrPtr->nstops;

    rect.X = trBrush.x, rect.Y = trBrush.y,
	rect.Width = trBrush.width, rect.Height = trBrush.height;

    /* BUG BUG BUG: A linear gradient brush will *sometimes* wrap when it
     * shouldn't due to rounding errors or something, resulting in a line
     * of color2 where color1 starts. The recommended solution is to
     * make the brush 1-pixel larger on all sides than the area being
     * painted.  The downside of this is you will lose a bit of the gradient. */
    if (gradient->vertical)
	rect.Y -= 1, rect.Height += 1;
    else
	rect.X -= 1, rect.Width += 1;

    stop = gradient->stopArrPtr->stops[0];
    color1 = MakeGDIPlusColor(stop->color, stop->opacity);
    stop = gradient->stopArrPtr->stops[nstops-1];
    color2 = MakeGDIPlusColor(stop->color, stop->opacity);

    status = DllExports._GdipCreateLineBrushFromRectI(
	&rect, color1, color2,
	gradient->vertical ? LinearGradientModeVertical : LinearGradientModeHorizontal,
	WrapModeTile, &lineGradient);
    if (status != Ok)
	return status;

    if (nstops > 2) {
	ARGB *col = DllExports._GdipAlloc(nstops * sizeof(ARGB));
	if (col != NULL) {
	    REAL *pos = DllExports._GdipAlloc(nstops * sizeof(REAL));
	    if (pos != NULL) {
		for (i = 0; i < nstops; i++) {
		    stop = gradient->stopArrPtr->stops[i];
		    col[i] = MakeGDIPlusColor(stop->color, stop->opacity);
		    pos[i] = (REAL)stop->offset;
		}
		status = DllExports._GdipSetLinePresetBlend(lineGradient,
		    col, pos, nstops);
		DllExports._GdipFree((void*) pos);
	    }
	    DllExports._GdipFree((void*) col);
	}
    }

    (*lgPtr) = lineGradient;
    return Ok;
}
    
/*
 *----------------------------------------------------------------------
 *
 * TreeGradient_FillRect --
 *
 *	Paint a rectangle with a gradient using GDI+.
 *
 * Results:
 *	If GDI+ isn't available then fall back to X11.  If the gradient
 *	has <2 stops then nothing is drawn.
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
    HDC hDC;
    TkWinDCState dcState;
    TreeClipStateGraphics clipState;
    GpGraphics *graphics;
    GpLineGradient *lineGradient = NULL;
    GpStatus status;
    GpRect rect;
    int nstops;

    /* Draw nothing if the brush is zero-sized. */
    if (trBrush.width <= 0 || trBrush.height <= 0)
	return;

    if (!tree->nativeGradients || (DllExports.handle == NULL)) {
	TreeGradient_FillRectX11(tree, td, clip, gradient, trBrush, tr);
	return;
    }

    nstops = gradient->stopArrPtr ? gradient->stopArrPtr->nstops : 0;
    if (nstops < 2) /* can be 0, but < 2 isn't allowed */
	return;

    hDC = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);

    status = DllExports._GdipCreateFromHDC(hDC, &graphics);
    if (status != Ok)
	goto error1;

    status = TreeClip_ToGraphics(tree, clip, graphics, &clipState);
    if (status != Ok)
	goto error2;

    status = MakeLinearGradientBrush(gradient, trBrush, &lineGradient);
    if (status != Ok)
	goto error2;

    rect.X = tr.x, rect.Y = tr.y, rect.Width = tr.width, rect.Height = tr.height;

    DllExports._GdipFillRectangleI(graphics, lineGradient,
	rect.X, rect.Y, rect.Width, rect.Height);

    DllExports._GdipDeleteBrush(lineGradient);

error2:
    DllExports._GdipDeleteGraphics(graphics);

error1:
    TkWinReleaseDrawableDC(td.drawable, hDC, &dcState);

#ifdef TREECTRL_DEBUG
    if (status != Ok) dbwin("TreeGradient_FillRect gdiplus status != Ok");
#endif
}

static void
GetRectPath_Outline(
    GpPath *path,		/* GDI+ path to set. */
    TreeRectangle tr,		/* Rectangle to draw. */
    int open,			/* RECT_OPEN_x flags. */
    int outlineWidth		/* Thickness of the outline. */
    )
{
    REAL x = (REAL)tr.x, y = (REAL)tr.y, width = (REAL)tr.width, height = (REAL)tr.height;
    int drawW = (open & RECT_OPEN_W) == 0;
    int drawN = (open & RECT_OPEN_N) == 0;
    int drawE = (open & RECT_OPEN_E) == 0;
    int drawS = (open & RECT_OPEN_S) == 0;

    /* Weird issue, if outlineWidth == 1 the NW corner is missing a pixel,
     * even when calling GdipAddPathRectangle.  So don't draw on the 1/2
     * pixel boundary in that case. */
    if (outlineWidth > 1) {
	x += outlineWidth / 2.0f, y += outlineWidth / 2.0f;
    }
    width -= outlineWidth, height -= outlineWidth;

    /* Simple case: draw all 4 edges */
    if (drawW && drawN && drawE && drawS) {
	DllExports._GdipAddPathRectangle(path, x, y, width, height);

    /* Complicated case: some edges are "open" */
    } else {
	if (drawN)
	    DllExports._GdipAddPathLine(path, x, y, x + width, y);
	if (drawE)
	    DllExports._GdipAddPathLine(path, x + width, y, x + width, y + height);
	else if (drawN)
	    DllExports._GdipStartPathFigure(path);
	if (drawS)
	    DllExports._GdipAddPathLine(path, x + width, y + height, x, y + height);
	else if (drawE)
	    DllExports._GdipStartPathFigure(path);
	if (drawW)
	    DllExports._GdipAddPathLine(path, x, y + height, x, y);
    }
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
    HDC hDC;
    TkWinDCState dcState;
    TreeClipStateGraphics clipState;
    GpGraphics *graphics;
    GpPath *path;
    GpLineGradient *lineGradient;
    GpPen *pen;
    GpStatus status;

    if (gradient->stopArrPtr == NULL || gradient->stopArrPtr->nstops < 2)
	return;

    /* Draw nothing if the brush is zero-sized. */
    if (trBrush.width <= 0 || trBrush.height <= 0)
	return;

    if (!tree->nativeGradients || (DllExports.handle == NULL)) {
	TreeGradient_DrawRectX11(tree, td, clip, gradient, trBrush, tr, outlineWidth, open);
	return;
    }

    hDC = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);

    status = DllExports._GdipCreateFromHDC(hDC, &graphics);
    if (status != Ok)
	goto error1;

    status = DllExports._GdipCreatePath(FillModeAlternate, &path);
    if (status != Ok)
	goto error2;

    status = MakeLinearGradientBrush(gradient, trBrush, &lineGradient);
    if (status != Ok)
	goto error3;

    status = DllExports._GdipCreatePen2(lineGradient, (REAL) outlineWidth,
	UnitPixel, &pen);
    if (status != Ok)
	goto error4;

    status = TreeClip_ToGraphics(tree, clip, graphics, &clipState);
    if (status != Ok)
	goto error5;

    GetRectPath_Outline(path, tr, open, outlineWidth);
    DllExports._GdipSetPenStartCap(pen, LineCapSquare);
    DllExports._GdipSetPenEndCap(pen, LineCapSquare);
    DllExports._GdipDrawPath(graphics, pen, path);

error5:
    DllExports._GdipDeletePen(pen);

error4:
    DllExports._GdipDeleteBrush(path);

error3:
    DllExports._GdipDeletePath(path);

error2:
    DllExports._GdipDeleteGraphics(graphics);

error1:
    TkWinReleaseDrawableDC(td.drawable, hDC, &dcState);
}

static void
GetRoundRectPath_Outline(
    GpPath *path,		/* GDI+ path to set. */
    TreeRectangle tr,		/* Rectangle to draw. */
    int rx, int ry,		/* Corner radius. */
    int open,			/* RECT_OPEN_x flags. */
    int fudgeX,			/* Fix for "open" edge endpoints when */
    int fudgeY,			/* outlineWidth>1. */
    int outlineWidth
    )
{
    REAL x = (REAL)tr.x, y = (REAL)tr.y, width = (REAL)tr.width, height = (REAL)tr.height;
    int drawW = (open & RECT_OPEN_W) == 0;
    int drawN = (open & RECT_OPEN_N) == 0;
    int drawE = (open & RECT_OPEN_E) == 0;
    int drawS = (open & RECT_OPEN_S) == 0;

    if (outlineWidth > 0) {
	x += outlineWidth / 2.0f, y += outlineWidth / 2.0f;
	width -= outlineWidth, height -= outlineWidth;
    } else {
	width -= 1, height -= 1;
    }

    /* Simple case: draw all 4 corners and 4 edges */
    if (drawW && drawN && drawE && drawS) {
	DllExports._GdipAddPathArc(path, x, y, rx*2.0f, ry*2.0f, 180.0f, 90.0f); /* top-left */
	DllExports._GdipAddPathArc(path, x + width - rx*2.0f, y, rx*2.0f, ry*2.0f, 270.0f, 90.0f); /* top-right */
	DllExports._GdipAddPathArc(path, x + width - rx*2.0f, y + height - ry*2.0f, rx*2.0f, ry*2.0f, 0.0f, 90.0f); /* bottom-right */
	DllExports._GdipAddPathArc(path, x, y + height - ry*2.0f, rx*2.0f, ry*2.0f, 90.0f, 90.0f); /* bottom-left */
	DllExports._GdipClosePathFigure(path);

    /* Complicated case: some edges are "open" */
    } else {
	GpPointF start[4], end[4]; /* start and end points of line segments*/
	start[0].X = x, start[0].Y = y;
	end[3] = start[0];
	if (drawW && drawN) {
	    start[0].X += rx;
	    end[3].Y += ry;
	} else {
	    start[0].X -= fudgeX;
	    end[3].Y -= fudgeY;
	}
	end[0].X = x + width, end[0].Y = y;
	start[1]= end[0];
	if (drawE && drawN) {
	    end[0].X -= rx;
	    start[1].Y += ry;
	} else {
	    end[0].X += fudgeX;
	    start[1].Y -= fudgeY;
	}
	end[1].X = x + width, end[1].Y = y + height;
	start[2] = end[1];
	if (drawE && drawS) {
	    end[1].Y -= ry;
	    start[2].X -= rx;
	} else {
	    end[1].Y += fudgeY;
	    start[2].X += fudgeX;
	}
	end[2].X = x, end[2].Y = y + height;
	start[3] = end[2];
	if (drawW && drawS) {
	    end[2].X += rx;
	    start[3].Y -= ry;
	} else {
	    end[2].X -= fudgeX;
	    start[3].Y += fudgeY;
	}

	if (drawW && drawN)
	    DllExports._GdipAddPathArc(path, x, y, rx*2.0f, ry*2.0f, 180.0f, 90.0f); /* top-left */
	if (drawN)
	    DllExports._GdipAddPathLine(path, start[0].X, start[0].Y, end[0].X, end[0].Y);
	if (drawE && drawN)
	    DllExports._GdipAddPathArc(path, x + width - rx*2.0f, y, rx*2.0f, ry*2.0f, 270.0f, 90.0f); /* top-right */
	if (drawE)
	    DllExports._GdipAddPathLine(path, start[1].X, start[1].Y, end[1].X, end[1].Y);
	else if (drawN)
	    DllExports._GdipStartPathFigure(path);
	if (drawE && drawS)
	    DllExports._GdipAddPathArc(path, x + width - rx*2.0f, y + height - ry*2.0f, rx*2.0f, ry*2.0f, 0.0f, 90.0f); /* bottom-right */
	if (drawS)
	    DllExports._GdipAddPathLine(path, start[2].X, start[2].Y, end[2].X, end[2].Y);
	else if (drawE)
	    DllExports._GdipStartPathFigure(path);
	if (drawW && drawS)
	    DllExports._GdipAddPathArc(path, x, y + height - ry*2.0f, rx*2.0f, ry*2.0f, 90.0f, 90.0f); /* bottom-left */
	if (drawW)
	    DllExports._GdipAddPathLine(path, start[3].X, start[3].Y, end[3].X, end[3].Y);
    }
}

void
Tree_DrawRoundRect(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    XColor *xcolor,		/* Color. */
    TreeRectangle tr,		/* Rectangle to draw. */
    int outlineWidth,		/* Width of outline. */
    int rx, int ry,		/* Corner radius */
    int open			/* RECT_OPEN_x flags */
    )
{
    HDC hDC;
    TkWinDCState dcState;
    TreeClipStateGraphics clipState;
    GpGraphics *graphics;
    GpPath *path;
    ARGB color;
    GpPen *pen;
    GpStatus status;
    int i;

    if (!tree->nativeGradients || (DllExports.handle == NULL)) {
	GC gc = Tk_GCForColor(xcolor, td.drawable);
	Tree_DrawRoundRectX11(tree, td, clip, gc, tr, outlineWidth, rx, ry, open);
	return;
    }

    hDC = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);

    status = DllExports._GdipCreateFromHDC(hDC, &graphics);
    if (status != Ok)
	goto error1;

    status = DllExports._GdipCreatePath(FillModeAlternate, &path);
    if (status != Ok)
	goto error2;

    color = MakeGDIPlusColor(xcolor,1.0f);
    status = DllExports._GdipCreatePen1(color, 1, UnitPixel, &pen);
    if (status != Ok)
	goto error3;

    status = TreeClip_ToGraphics(tree, clip, graphics, &clipState);
    if (status != Ok)
	goto error3;

    GetRoundRectPath_Outline(path, tr, rx, ry, open, 0, 0, 0);
    DllExports._GdipDrawPath(graphics, pen, path);

    /* http://www.codeproject.com/KB/GDI-plus/RoundRect.aspx */
    for (i = 1; i < outlineWidth; i++) {
	tr.x += 1, tr.width -= 2;
	DllExports._GdipResetPath(path);
	GetRoundRectPath_Outline(path, tr, rx, ry, open, i, i-1, 0);
	DllExports._GdipDrawPath(graphics, pen, path);
    
	tr.y += 1, tr.height -= 2;
	DllExports._GdipResetPath(path);
	GetRoundRectPath_Outline(path, tr, rx, ry, open, i, i, 0);
	DllExports._GdipDrawPath(graphics, pen, path);
    }

    DllExports._GdipDeletePen(pen);

error3:
    DllExports._GdipDeletePath(path);

error2:
    DllExports._GdipDeleteGraphics(graphics);

error1:
    TkWinReleaseDrawableDC(td.drawable, hDC, &dcState);
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
    HDC hDC;
    TkWinDCState dcState;
    TreeClipStateGraphics clipState;
    GpGraphics *graphics;
    GpPath *path;
    GpLineGradient *lineGradient;
    GpPen *pen;
    GpStatus status;
    int i;
    int canRepairCorners;

    if (gradient->stopArrPtr == NULL || gradient->stopArrPtr->nstops < 2)
	return;

    /* Draw nothing if the brush is zero-sized. */
    if (trBrush.width <= 0 || trBrush.height <= 0)
	return;

    if (!tree->nativeGradients || (DllExports.handle == NULL)) {
	XColor *xcolor = gradient->stopArrPtr->stops[0]->color; /* Use the first stop color */
	GC gc = Tk_GCForColor(xcolor, td.drawable);
	Tree_DrawRoundRectX11(tree, td, clip, gc, tr, outlineWidth, rx, ry, open);
	return;
    }

    hDC = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);

    status = DllExports._GdipCreateFromHDC(hDC, &graphics);
    if (status != Ok)
	goto error1;

    status = DllExports._GdipCreatePath(FillModeAlternate, &path);
    if (status != Ok)
	goto error2;

    status = MakeLinearGradientBrush(gradient, trBrush, &lineGradient);
    if (status != Ok)
	goto error3;

    canRepairCorners = (outlineWidth == 1) || TreeGradient_IsOpaque(tree, gradient);

    status = DllExports._GdipCreatePen2(lineGradient, canRepairCorners ?
	1.0f : (REAL) outlineWidth, UnitPixel, &pen);
    if (status != Ok)
	goto error4;

    status = TreeClip_ToGraphics(tree, clip, graphics, &clipState);
    if (status != Ok)
	goto error5;

    GetRoundRectPath_Outline(path, tr, rx, ry, open, 0, 0,
	canRepairCorners ? 0 : outlineWidth);

    DllExports._GdipDrawPath(graphics, pen, path);

    /* http://www.codeproject.com/KB/GDI-plus/RoundRect.aspx */
    if (canRepairCorners) {
	for (i = 1; i < outlineWidth; i++) {
	    tr.x += 1, tr.width -= 2;
	    DllExports._GdipResetPath(path);
	    GetRoundRectPath_Outline(path, tr, rx, ry, open, i, i-1, 0);
	    DllExports._GdipDrawPath(graphics, pen, path);
	
	    tr.y += 1, tr.height -= 2;
	    DllExports._GdipResetPath(path);
	    GetRoundRectPath_Outline(path, tr, rx, ry, open, i, i, 0);
	    DllExports._GdipDrawPath(graphics, pen, path);
	}
    }

error5:
    DllExports._GdipDeletePen(pen);

error4:
    DllExports._GdipDeleteBrush(path);

error3:
    DllExports._GdipDeletePath(path);

error2:
    DllExports._GdipDeleteGraphics(graphics);

error1:
    TkWinReleaseDrawableDC(td.drawable, hDC, &dcState);
}

#define ROUND_RECT_SYMMETRY_HACK

/* This returns a path 1-pixel smaller on the right and bottom edges than
 * it should be.
 * For some reason GdipFillPath produces different (and asymmetric) results
 * than GdipDrawPath.  So after filling the round rectangle with this path
 * GetRoundRectPath_Outline should be called to paint the right and bottom
 * edges. */
/* http://www.codeproject.com/KB/GDI-plus/RoundRect.aspx */
static void
GetRoundRectPath_Fill(
    GpPath *path,		/* GDI+ path to set. */
    TreeRectangle tr,		/* Rectangle to paint. */
    int rx, int ry,		/* Corner radius. */
    int open			/* RECT_OPEN_x flags. */
#ifdef ROUND_RECT_SYMMETRY_HACK
    , int rrhack
#endif
    )
{
    REAL x = (REAL)tr.x, y = (REAL)tr.y, width = (REAL)tr.width, height = (REAL)tr.height;
    int drawW = (open & RECT_OPEN_W) == 0;
    int drawN = (open & RECT_OPEN_N) == 0;
    int drawE = (open & RECT_OPEN_E) == 0;
    int drawS = (open & RECT_OPEN_S) == 0;

    /* Simple case: draw all 4 corners and 4 edges */
    if (drawW && drawN && drawE && drawS) {
#ifdef ROUND_RECT_SYMMETRY_HACK
	if (rrhack)
	    width -= 1, height -= 1;
#endif
	DllExports._GdipAddPathArc(path, x, y, rx*2.0f, ry*2.0f, 180.0f, 90.0f); /* top-left */
	DllExports._GdipAddPathArc(path, x + width - rx*2.0f, y, rx*2.0f, ry*2.0f, 270.0f, 90.0f); /* top-right */
	DllExports._GdipAddPathArc(path, x + width - rx*2.0f, y + height - ry*2.0f, rx*2.0f, ry*2.0f, 0.0f, 90.0f); /* bottom-right */
	DllExports._GdipAddPathArc(path, x, y + height - ry*2.0f, rx*2.0f, ry*2.0f, 90.0f, 90.0f); /* bottom-left */
	DllExports._GdipClosePathFigure(path);

    /* Complicated case: some edges are "open" */
    } else {
	GpPointF start[4], end[4]; /* start and end points of line segments*/
#ifdef ROUND_RECT_SYMMETRY_HACK
	if (rrhack) {
	    if (drawE)
		width -= 1;
	    if (drawS)
		height -= 1;
	}
#endif
	start[0].X = x, start[0].Y = y;
	end[3] = start[0];
	if (drawW && drawN) {
	    start[0].X += rx;
	    end[3].Y += ry;
	}
	end[0].X = x + width, end[0].Y = y;
	start[1]= end[0];
	if (drawE && drawN) {
	    end[0].X -= rx;
	    start[1].Y += ry;
	}
	end[1].X = x + width, end[1].Y = y + height;
	start[2] = end[1];
	if (drawE && drawS) {
	    end[1].Y -= ry;
	    start[2].X -= rx;
	}
	end[2].X = x, end[2].Y = y + height;
	start[3] = end[2];
	if (drawW && drawS) {
	    end[2].X += rx;
	    start[3].Y -= ry;
	}

	if (drawW && drawN)
	    DllExports._GdipAddPathArc(path, x, y, rx*2.0f, ry*2.0f, 180.0f, 90.0f); /* top-left */
	DllExports._GdipAddPathLine(path, start[0].X, start[0].Y, end[0].X, end[0].Y);
	if (drawE && drawN)
	    DllExports._GdipAddPathArc(path, x + width - rx*2.0f, y, rx*2.0f, ry*2.0f, 270.0f, 90.0f); /* top-right */
	DllExports._GdipAddPathLine(path, start[1].X, start[1].Y, end[1].X, end[1].Y);
	if (drawE && drawS)
	    DllExports._GdipAddPathArc(path, x + width - rx*2.0f, y + height - ry*2.0f, rx*2.0f, ry*2.0f, 0.0f, 90.0f); /* bottom-right */
	DllExports._GdipAddPathLine(path, start[2].X, start[2].Y, end[2].X, end[2].Y);
	if (drawW && drawS)
	    DllExports._GdipAddPathArc(path, x, y + height - ry*2.0f, rx*2.0f, ry*2.0f, 90.0f, 90.0f); /* bottom-left */
	DllExports._GdipAddPathLine(path, start[3].X, start[3].Y, end[3].X, end[3].Y);
    }
}

void
Tree_FillRoundRect(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    XColor *xcolor,		/* Color. */
    TreeRectangle tr,		/* Recangle to paint. */
    int rx, int ry,		/* Corner radius */
    int open			/* RECT_OPEN_x flags */
    )
{
    HDC hDC;
    TkWinDCState dcState;
    TreeClipStateGraphics clipState;
    GpGraphics *graphics;
    GpPath *path;
    ARGB color;
    GpSolidFill *brush;
#ifdef ROUND_RECT_SYMMETRY_HACK
    GpPen *pen;
#endif
    GpStatus status;

    if (!tree->nativeGradients || (DllExports.handle == NULL)) {
	GC gc = Tk_GCForColor(xcolor, td.drawable);
	Tree_FillRoundRectX11(tree, td, clip, gc, tr, rx, ry, open);
	return;
    }

    hDC = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);

    status = DllExports._GdipCreateFromHDC(hDC, &graphics);
    if (status != Ok)
	goto error1;

    status = DllExports._GdipCreatePath(FillModeAlternate, &path);
    if (status != Ok)
	goto error2;

    color = MakeGDIPlusColor(xcolor,1.0f);
    status = DllExports._GdipCreateSolidFill(color, &brush);
    if (status != Ok)
	goto error3;

#if !defined(ROUND_RECT_SYMMETRY_HACK) && 0
    /* SmoothingModeHighQuality and SmoothingModeAntiAlias seem the same. */
    DllExports._GdipSetSmoothingMode(graphics, SmoothingModeHighQuality);

    /* Antialiasing paints outside the rectangle.  If I clip drawing to the
     * rectangle I still get artifacts on the "open" edges. */
//    status = DllExports._GdipSetClipRectI(graphics,
//	tr.x, tr.y, tr.width, tr.height, CombineModeReplace);
#endif

    GetRoundRectPath_Fill(path, tr, rx, ry, open
#ifdef ROUND_RECT_SYMMETRY_HACK
	, 1
#endif
    );

    status = TreeClip_ToGraphics(tree, clip, graphics, &clipState);
    if (status != Ok)
	goto error4;

    DllExports._GdipFillPath(graphics, brush, path);

#ifdef ROUND_RECT_SYMMETRY_HACK
    status = DllExports._GdipCreatePen1(color, 1, UnitPixel, &pen);
    if (status != Ok)
	goto error4;

    /* See comments above for why this is done */
    DllExports._GdipResetPath(path);
    GetRoundRectPath_Outline(path, tr, rx, ry, open, 0, 0, 0);
    DllExports._GdipDrawPath(graphics, pen, path);

    DllExports._GdipDeletePen(pen);
#endif /* ROUND_RECT_SYMMETRY_HACK */

error4:
    DllExports._GdipDeleteBrush(brush);

error3:
    DllExports._GdipDeletePath(path);

error2:
    DllExports._GdipDeleteGraphics(graphics);

error1:
    TkWinReleaseDrawableDC(td.drawable, hDC, &dcState);
}

void
TreeGradient_FillRoundRect(
    TreeCtrl *tree,		/* Widget info. */
    TreeDrawable td,		/* Where to draw. */
    TreeClip *clip,		/* Clipping area or NULL. */
    TreeGradient gradient,	/* Gradient token. */
    TreeRectangle trBrush,	/* Brush bounds. */
    TreeRectangle tr,		/* Rectangle to paint. */
    int rx, int ry,		/* Corner radius */
    int open			/* RECT_OPEN_x flags */
    )
{
    HDC hDC;
    TkWinDCState dcState;
    TreeClipStateGraphics clipState;
    GpGraphics *graphics;
    GpPath *path;
    GpLineGradient *lineGradient;
    GpPen *pen;
    GpStatus status;
    int canRepairCorners;

    if (gradient->stopArrPtr == NULL || gradient->stopArrPtr->nstops < 2)
	return;

    /* Draw nothing if the brush is zero-sized. */
    if (trBrush.width <= 0 || trBrush.height <= 0)
	return;

    if (!tree->nativeGradients || (DllExports.handle == NULL)) {
	TreeGradient_FillRoundRectX11(tree, td, NULL, gradient, trBrush, tr,
	    rx, ry, open);
	return;
    }

    hDC = TkWinGetDrawableDC(tree->display, td.drawable, &dcState);

    status = DllExports._GdipCreateFromHDC(hDC, &graphics);
    if (status != Ok)
	goto error1;

    status = DllExports._GdipCreatePath(FillModeAlternate, &path);
    if (status != Ok)
	goto error2;

    status = MakeLinearGradientBrush(gradient, trBrush, &lineGradient);
    if (status != Ok)
	goto error3;

    /* Filling the rounded rectangle gives asymmetric results at the corners.
     * If the gradient is fully opaque then the corners can be "repaired" by
     * drawing a 1-pixel thick outline after filling the shape. */
    canRepairCorners = TreeGradient_IsOpaque(tree, gradient);

    GetRoundRectPath_Fill(path, tr, rx, ry, open
#ifdef ROUND_RECT_SYMMETRY_HACK
	, canRepairCorners
#endif
    );

    status = TreeClip_ToGraphics(tree, clip, graphics, &clipState);
    if (status != Ok)
	goto error4;

    DllExports._GdipFillPath(graphics, lineGradient, path);

    if (canRepairCorners) {
	status = DllExports._GdipCreatePen2(lineGradient, 1, UnitPixel, &pen);
	if (status != Ok)
	    goto error4;

	DllExports._GdipResetPath(path);
	GetRoundRectPath_Outline(path, tr, rx, ry, open, 0, 0, 0);
	DllExports._GdipDrawPath(graphics, pen, path);

	DllExports._GdipDeletePen(pen);
    }

error4:
    DllExports._GdipDeleteBrush(lineGradient);

error3:
    DllExports._GdipDeletePath(path);

error2:
    DllExports._GdipDeleteGraphics(graphics);

error1:
    TkWinReleaseDrawableDC(td.drawable, hDC, &dcState);
}
