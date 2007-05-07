/*
 * tkMacOSXDraw.c --
 *
 *	This file contains functions that perform drawing to
 *	Xlib windows. Most of the functions simple emulate
 *	Xlib functions.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2006-2007 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXInt.h"
#include "tkMacOSXDebug.h"

/*
#ifdef TK_MAC_DEBUG
#define TK_MAC_DEBUG_DRAWING
#endif
*/

#define radians(d) ((d) * (M_PI/180.0))

/*
 * Non-antialiased CG drawing looks better and more like X11 drawing when using
 * very fine lines, so decrease all linewidths by the following constant.
 */
#define NON_AA_CG_OFFSET .999

/*
 * Temporary regions that can be reused.
 */

RgnHandle tkMacOSXtmpRgn1 = NULL;
RgnHandle tkMacOSXtmpRgn2 = NULL;

static PixPatHandle gPenPat = NULL;

static int useCGDrawing = 1;
static int tkMacOSXCGAntiAliasLimit = 0;
#define notAA(w) ((w) < tkMacOSXCGAntiAliasLimit)

static int useThemedToplevel = 0;
static int useThemedFrame = 0;

/*
 * Prototypes for functions used only in this file.
 */
static unsigned char InvertByte(unsigned char data);


/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInitCGDrawing --
 *
 *	Initializes link vars that control CG drawing.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
TkMacOSXInitCGDrawing(
    Tcl_Interp *interp,
    int enable,
    int limit)
{
    static Boolean initialized = FALSE;

    if (!initialized) {
	initialized = TRUE;

	if (Tcl_CreateNamespace(interp, "::tk::mac", NULL, NULL) == NULL) {
	    Tcl_ResetResult(interp);
	}
	if (Tcl_LinkVar(interp, "::tk::mac::useCGDrawing",
		(char *) &useCGDrawing, TCL_LINK_BOOLEAN) != TCL_OK) {
	    Tcl_ResetResult(interp);
	}
	useCGDrawing = enable;

	if (Tcl_LinkVar(interp, "::tk::mac::CGAntialiasLimit",
		(char *) &tkMacOSXCGAntiAliasLimit, TCL_LINK_INT) != TCL_OK) {
	    Tcl_ResetResult(interp);
	}
	tkMacOSXCGAntiAliasLimit = limit;

	/*
	 * Piggy-back the themed drawing var init here.
	 */

	if (Tcl_LinkVar(interp, "::tk::mac::useThemedToplevel",
		(char *) &useThemedToplevel, TCL_LINK_BOOLEAN) != TCL_OK) {
	    Tcl_ResetResult(interp);
	}
	if (Tcl_LinkVar(interp, "::tk::mac::useThemedFrame",
		(char *) &useThemedFrame, TCL_LINK_BOOLEAN) != TCL_OK) {
	    Tcl_ResetResult(interp);
	}

	if (tkMacOSXtmpRgn1 == NULL) {
	    tkMacOSXtmpRgn1 = NewRgn();
	}
	if (tkMacOSXtmpRgn2 == NULL) {
	    tkMacOSXtmpRgn2 = NewRgn();
	}
    }
#ifdef TK_MAC_DEBUG_DRAWING
    TkMacOSXInitNamedDebugSymbol(QD, void, QD_DebugPrint, char*);
    if (QD_DebugPrint) {
	; /* gdb: b *QD_DebugPrint */
    }
#endif /* TK_MAC_DEBUG_WINDOWS */
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * XCopyArea --
 *
 *	Copies data from one drawable to another using block transfer
 *	routines.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Data is moved from a window or bitmap to a second window or
 *	bitmap.
 *
 *----------------------------------------------------------------------
 */

void
XCopyArea(
    Display *display,		/* Display. */
    Drawable src,		/* Source drawable. */
    Drawable dst,		/* Destination drawable. */
    GC gc,			/* GC to use. */
    int src_x,			/* X & Y, width & height */
    int src_y,			/* define the source rectangle */
    unsigned int width,		/* the will be copied. */
    unsigned int height,
    int dest_x,			/* Dest X & Y on dest rect. */
    int dest_y)
{
    Rect srcRect, dstRect, *srcPtr, *dstPtr;
    const BitMap *srcBit, *dstBit;
    MacDrawable *srcDraw = (MacDrawable *) src, *dstDraw = (MacDrawable *) dst;
    CGrafPtr srcPort, dstPort, savePort;
    Boolean portChanged;
    short tmode;
    RGBColor origForeColor, origBackColor, whiteColor, blackColor;
    Rect clpRect;

    dstPort = TkMacOSXGetDrawablePort(dst);
    srcPort = TkMacOSXGetDrawablePort(src);

    display->request++;
    portChanged = QDSwapPort(dstPort, &savePort);
    GetForeColor(&origForeColor);
    GetBackColor(&origBackColor);
    whiteColor.red = 0;
    whiteColor.blue = 0;
    whiteColor.green = 0;
    RGBForeColor(&whiteColor);
    blackColor.red = 0xFFFF;
    blackColor.blue = 0xFFFF;
    blackColor.green = 0xFFFF;
    RGBBackColor(&blackColor);

    srcPtr = &srcRect;
    SetRect(&srcRect, (short) (srcDraw->xOff + src_x),
	    (short) (srcDraw->yOff + src_y),
	    (short) (srcDraw->xOff + src_x + width),
	    (short) (srcDraw->yOff + src_y + height));
    if (tkPictureIsOpen) {
	dstPtr = &srcRect;
    } else {
	dstPtr = &dstRect;
	SetRect(&dstRect, (short) (dstDraw->xOff + dest_x),
	    (short) (dstDraw->yOff + dest_y),
	    (short) (dstDraw->xOff + dest_x + width),
	    (short) (dstDraw->yOff + dest_y + height));
    }
    TkMacOSXSetUpClippingRgn(dst);

    /*
     * We will change the clip rgn in this routine, so we need to
     * be able to restore it when we exit.
     */

    TkMacOSXCheckTmpRgnEmpty(2);
    GetClip(tkMacOSXtmpRgn2);
    if (tkPictureIsOpen) {
	/*
	 * When rendering into a picture, after a call to "OpenCPicture"
	 * the clipping is seriously WRONG and also INCONSISTENT with the
	 * clipping for single plane bitmaps.
	 * To circumvent this problem, we clip to the whole window
	 * In this case, would have also clipped to the srcRect
	 * ClipRect(&srcRect);
	 */

	GetPortBounds(dstPort,&clpRect);
	dstPtr = &srcRect;
	ClipRect(&clpRect);
    }
    if (gc->clip_mask && ((TkpClipMask*)gc->clip_mask)->type
	    == TKP_CLIP_REGION) {
	RgnHandle clipRgn = (RgnHandle)
		((TkpClipMask*)gc->clip_mask)->value.region;
	int xOffset = 0, yOffset = 0;

	if (!tkPictureIsOpen) {
	    xOffset = dstDraw->xOff + gc->clip_x_origin;
	    yOffset = dstDraw->yOff + gc->clip_y_origin;
	    OffsetRgn(clipRgn, xOffset, yOffset);
	}
	TkMacOSXCheckTmpRgnEmpty(1);
	GetClip(tkMacOSXtmpRgn1);
	SectRgn(tkMacOSXtmpRgn1, clipRgn, tkMacOSXtmpRgn1);
	SetClip(tkMacOSXtmpRgn1);
	SetEmptyRgn(tkMacOSXtmpRgn1);
	if (!tkPictureIsOpen) {
	    OffsetRgn(clipRgn, -xOffset, -yOffset);
	}
    }
    srcBit = GetPortBitMapForCopyBits(srcPort);
    dstBit = GetPortBitMapForCopyBits(dstPort);
    tmode = srcCopy;

    CopyBits(srcBit, dstBit, srcPtr, dstPtr, tmode, NULL);
    RGBForeColor(&origForeColor);
    RGBBackColor(&origBackColor);
    SetClip(tkMacOSXtmpRgn2);
    SetEmptyRgn(tkMacOSXtmpRgn2);
    if (portChanged) {
	QDSwapPort(savePort, NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XCopyPlane --
 *
 *	Copies a bitmap from a source drawable to a destination
 *	drawable. The plane argument specifies which bit plane of
 *	the source contains the bitmap. Note that this implementation
 *	ignores the gc->function.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the destination drawable.
 *
 *----------------------------------------------------------------------
 */

void
XCopyPlane(
    Display *display,		/* Display. */
    Drawable src,		/* Source drawable. */
    Drawable dst,		/* Destination drawable. */
    GC gc,			/* The GC to use. */
    int src_x, int src_y,	/* X, Y, width & height define the source
				 * rect. */
    unsigned int width, unsigned int height,
    int dest_x, int dest_y,	/* X & Y on dest where we will copy. */
    unsigned long plane)	/* Which plane to copy. */
{
    Rect srcRect, dstRect, *srcPtr, *dstPtr;
    const BitMap *srcBit, *dstBit, *mskBit;
    MacDrawable *srcDraw = (MacDrawable *) src, *dstDraw = (MacDrawable *) dst;
    CGrafPtr srcPort, dstPort, mskPort, savePort;
    Boolean portChanged;
    TkpClipMask *clipPtr = (TkpClipMask *) gc->clip_mask;
    short tmode;

    srcPort = TkMacOSXGetDrawablePort(src);
    dstPort = TkMacOSXGetDrawablePort(dst);

    display->request++;
    portChanged = QDSwapPort(dstPort, &savePort);
    TkMacOSXSetUpClippingRgn(dst);

    srcBit = GetPortBitMapForCopyBits(srcPort);
    dstBit = GetPortBitMapForCopyBits(dstPort);
    SetRect(&srcRect, (short) (srcDraw->xOff + src_x),
	    (short) (srcDraw->yOff + src_y),
	    (short) (srcDraw->xOff + src_x + width),
	    (short) (srcDraw->yOff + src_y + height));
    srcPtr = &srcRect;
    if (tkPictureIsOpen) {
	/*
	 * When rendering into a picture, after a call to "OpenCPicture"
	 * the clipping is seriously WRONG and also INCONSISTENT with the
	 * clipping for color bitmaps.
	 * To circumvent this problem, we clip to the whole window
	 */

	Rect clpRect;
	GetPortBounds(dstPort,&clpRect);
	ClipRect(&clpRect);
	dstPtr = &srcRect;
    } else {
	dstPtr = &dstRect;
	SetRect(&dstRect, (short) (dstDraw->xOff + dest_x),
		(short) (dstDraw->yOff + dest_y),
		(short) (dstDraw->xOff + dest_x + width),
		(short) (dstDraw->yOff + dest_y + height));
    }
    tmode = srcOr;
    tmode = srcCopy + transparent;

    TkMacOSXSetColorInPort(gc->foreground, 1, NULL);

    if (clipPtr == NULL || clipPtr->type == TKP_CLIP_REGION) {
	/*
	 * Case 1: opaque bitmaps.
	 */

	TkMacOSXSetColorInPort(gc->background, 0, NULL);
	tmode = srcCopy;
	CopyBits(srcBit, dstBit, srcPtr, dstPtr, tmode, NULL);
    } else if (clipPtr->type == TKP_CLIP_PIXMAP) {
	if (clipPtr->value.pixmap == src) {
	    PixMapHandle pm;
	    /*
	     * Case 2: transparent bitmaps. If it's color we ignore
	     * the forecolor.
	     */

	    pm = GetPortPixMap(srcPort);
	    if (GetPixDepth(pm) == 1) {
		tmode = srcOr;
	    } else {
		tmode = transparent;
	    }
	    CopyBits(srcBit, dstBit, srcPtr, dstPtr, tmode, NULL);
	} else {
	    /*
	     * Case 3: two arbitrary bitmaps.
	     */

	    tmode = srcCopy;
	    mskPort = TkMacOSXGetDrawablePort(clipPtr->value.pixmap);
	    mskBit = GetPortBitMapForCopyBits(mskPort);
	    CopyDeepMask(srcBit, mskBit, dstBit,
		srcPtr, srcPtr, dstPtr, tmode, NULL);
	}
    }
    if (portChanged) {
	QDSwapPort(savePort, NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkPutImage --
 *
 *	Copies a subimage from an in-memory image to a rectangle of
 *	of the specified drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws the image on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
TkPutImage(
    unsigned long *colors,	/* Unused on Macintosh. */
    int ncolors,		/* Unused on Macintosh. */
    Display* display,		/* Display. */
    Drawable d,			/* Drawable to place image on. */
    GC gc,			/* GC to use. */
    XImage* image,		/* Image to place. */
    int src_x,			/* Source X & Y. */
    int src_y,
    int dest_x,			/* Destination X & Y. */
    int dest_y,
    unsigned int width,		/* Same width & height for both */
    unsigned int height)	/* distination and source. */
{
    CGrafPtr destPort, savePort;
    Boolean portChanged;
    const BitMap * destBits;
    MacDrawable *dstDraw = (MacDrawable *) d;
    int i, j;
    char *newData = NULL;
    Rect destRect, srcRect, *destPtr, *srcPtr;
    char *dataPtr, *newPtr, *oldPtr;
    int rowBytes = image->bytes_per_line;
    int slices, sliceRowBytes, lastSliceRowBytes, sliceWidth, lastSliceWidth;

    display->request++;
    destPort = TkMacOSXGetDrawablePort(d);
    portChanged = QDSwapPort(destPort, &savePort);
    destBits = GetPortBitMapForCopyBits(destPort);
    TkMacOSXSetUpClippingRgn(d);

    srcPtr = &srcRect;
    SetRect(srcPtr, src_x, src_y, src_x + width, src_y + height);
    if (tkPictureIsOpen) {
	/*
	 * When rendering into a picture, after a call to "OpenCPicture"
	 * the clipping is seriously WRONG and also INCONSISTENT with the
	 * clipping for single plane bitmaps.
	 * To circumvent this problem, we clip to the whole window
	 */

	Rect clpRect;

	GetPortBounds(destPort,&clpRect);
	ClipRect(&clpRect);
	destPtr = srcPtr;
    } else {
	destPtr = &destRect;
	SetRect(destPtr, dstDraw->xOff + dest_x, dstDraw->yOff + dest_y,
	    dstDraw->xOff + dest_x + width, dstDraw->yOff + dest_y + height);
    }

    if (image->obdata) {
	/*
	 * Image from XGetImage, copy from containing GWorld directly.
	 */

	CopyBits(GetPortBitMapForCopyBits(TkMacOSXGetDrawablePort((Drawable)
		image->obdata)), destBits, srcPtr, destPtr, srcCopy, NULL);
    } else if (image->depth == 1) {
	/*
	 * BW image
	 */

	const int maxRowBytes = 0x3ffe;
	BitMap bitmap;
	int odd;

	if (rowBytes > maxRowBytes) {
	    slices = rowBytes / maxRowBytes;
	    sliceRowBytes = maxRowBytes;
	    lastSliceRowBytes = rowBytes - (slices * maxRowBytes);
	    if (!lastSliceRowBytes) {
		slices--;
		lastSliceRowBytes = maxRowBytes;
	    }
	    sliceWidth = (long) image->width * maxRowBytes / rowBytes;
	    lastSliceWidth = image->width - (sliceWidth * slices);
	} else {
	    slices = 0;
	    sliceRowBytes = lastSliceRowBytes = rowBytes;
	    sliceWidth = lastSliceWidth = image->width;
	}
	bitmap.bounds.top = bitmap.bounds.left = 0;
	bitmap.bounds.bottom = (short) image->height;
	dataPtr = image->data;
	do {
	    if (slices) {
		bitmap.bounds.right = bitmap.bounds.left + sliceWidth;
	    } else {
		sliceRowBytes = lastSliceRowBytes;
		bitmap.bounds.right = bitmap.bounds.left + lastSliceWidth;
	    }
	    oldPtr = dataPtr;
	    odd = sliceRowBytes % 2;
	    if (!newData) {
		newData = ckalloc(image->height * (sliceRowBytes+odd));
	    }
	    newPtr = newData;
	    for (i = 0; i < image->height; i++) {
		for (j = 0; j < sliceRowBytes; j++) {
		    *newPtr = InvertByte((unsigned char) *oldPtr);
		    newPtr++; oldPtr++;
		}
		if (odd) {
		    *newPtr++ = 0;
		}
		oldPtr += rowBytes - sliceRowBytes;
	    }
	    bitmap.baseAddr = newData;
	    bitmap.rowBytes = sliceRowBytes + odd;
	    CopyBits(&bitmap, destBits, srcPtr, destPtr, srcCopy, NULL);
	    if (slices) {
		bitmap.bounds.left = bitmap.bounds.right;
		dataPtr += sliceRowBytes;
	    }
	} while (slices--);
    } else {
	/*
	 * Color image
	 */

	const int maxRowBytes = 0x3ffc;
	PixMap pixmap;

	pixmap.bounds.left = 0;
	pixmap.bounds.top = 0;
	pixmap.bounds.bottom = (short) image->height;
	pixmap.pixelType = RGBDirect;
	pixmap.pmVersion = baseAddr32;	/* 32bit clean */
	pixmap.packType = 0;
	pixmap.packSize = 0;
	pixmap.hRes = 0x00480000;
	pixmap.vRes = 0x00480000;
	pixmap.pixelSize = 32;
	pixmap.cmpCount = 3;
	pixmap.cmpSize = 8;
#ifdef WORDS_BIGENDIAN
	pixmap.pixelFormat = k32ARGBPixelFormat;
#else
	pixmap.pixelFormat = k32BGRAPixelFormat;
#endif
	pixmap.pmTable = NULL;
	pixmap.pmExt = 0;
	if (rowBytes > maxRowBytes) {
	    slices = rowBytes / maxRowBytes;
	    sliceRowBytes = maxRowBytes;
	    lastSliceRowBytes = rowBytes - (slices * maxRowBytes);
	    if (!lastSliceRowBytes) {
		slices--;
		lastSliceRowBytes = maxRowBytes;
	    }
	    sliceWidth = (long) image->width * maxRowBytes / rowBytes;
	    lastSliceWidth = image->width - (sliceWidth * slices);
	    dataPtr = image->data;
	    newData = (char *) ckalloc(image->height * sliceRowBytes);
	    do {
		if (slices) {
		    pixmap.bounds.right = pixmap.bounds.left + sliceWidth;
		} else {
		    sliceRowBytes = lastSliceRowBytes;
		    pixmap.bounds.right = pixmap.bounds.left + lastSliceWidth;
		}
		oldPtr = dataPtr;
		newPtr = newData;
		for (i = 0; i < image->height; i++) {
		    memcpy(newPtr, oldPtr, sliceRowBytes);
		    oldPtr += rowBytes;
		    newPtr += sliceRowBytes;
		}
		pixmap.baseAddr = newData;
		pixmap.rowBytes = sliceRowBytes | 0x8000;
		CopyBits((BitMap *) &pixmap, destBits, srcPtr, destPtr,
			srcCopy, NULL);
		if (slices) {
		    pixmap.bounds.left = pixmap.bounds.right;
		    dataPtr += sliceRowBytes;
		}
	    } while (slices--);
	} else {
	    pixmap.bounds.right = (short) image->width;
	    pixmap.baseAddr = image->data;
	    pixmap.rowBytes = rowBytes | 0x8000;
	    CopyBits((BitMap *) &pixmap, destBits, srcPtr, destPtr,
		    srcCopy, NULL);
	}
    }
    if (newData != NULL) {
	ckfree(newData);
    }
    if (portChanged) {
	QDSwapPort(savePort, NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawLines --
 *
 *	Draw connected lines.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Renders a series of connected lines.
 *
 *----------------------------------------------------------------------
 */

void
XDrawLines(
    Display *display,		/* Display. */
    Drawable d,			/* Draw on this. */
    GC gc,			/* Use this GC. */
    XPoint *points,		/* Array of points. */
    int npoints,		/* Number of points. */
    int mode)			/* Line drawing mode. */
{
    MacDrawable *macWin = (MacDrawable *) d;
    TkMacOSXDrawingContext dc;
    int i, lw = gc->line_width;

    if (npoints < 2) {
	return; /* TODO: generate BadValue error. */
    }

    display->request++;
    if (TkMacOSXSetupDrawingContext(d, gc, useCGDrawing, &dc)) {
	double prevx, prevy;
	double o = (lw % 2) ? .5 : 0;

	CGContextBeginPath(dc.context);
	prevx = macWin->xOff + points[0].x + o;
	prevy = macWin->yOff + points[0].y + o;
	CGContextMoveToPoint(dc.context, prevx, prevy);
	for (i = 1; i < npoints; i++) {
	    if (mode == CoordModeOrigin) {
		CGContextAddLineToPoint(dc.context,
			macWin->xOff + points[i].x + o,
			macWin->yOff + points[i].y + o);
	    } else {
		prevx += points[i].x;
		prevy += points[i].y;
		CGContextAddLineToPoint(dc.context, prevx, prevy);
	    }
	}
	CGContextStrokePath(dc.context);
    } else {
	int o = -lw/2;

	/* This is broken for fat lines, it is not possible to correctly
	 * imitate X11 drawing of oblique fat lines with QD line drawing,
	 * we should draw a filled polygon instead. */

	MoveTo((short) (macWin->xOff + points[0].x + o),
	       (short) (macWin->yOff + points[0].y + o));
	for (i = 1; i < npoints; i++) {
	    if (mode == CoordModeOrigin) {
		LineTo((short) (macWin->xOff + points[i].x + o),
		       (short) (macWin->yOff + points[i].y + o));
	    } else {
		Line((short) points[i].x, (short) points[i].y);
	    }
	}
    }
    TkMacOSXRestoreDrawingContext(&dc);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawSegments --
 *
 *	Draw unconnected lines.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Renders a series of unconnected lines.
 *
 *----------------------------------------------------------------------
 */

void
XDrawSegments(
    Display *display,
    Drawable d,
    GC gc,
    XSegment *segments,
    int nsegments)
{
    MacDrawable *macWin = (MacDrawable *) d;
    TkMacOSXDrawingContext dc;
    int i, lw = gc->line_width;

    display->request++;
    if (TkMacOSXSetupDrawingContext(d, gc, useCGDrawing, &dc)) {
	double o = (lw % 2) ? .5 : 0;

	for (i = 0; i < nsegments; i++) {
	    CGContextBeginPath(dc.context);
	    CGContextMoveToPoint(dc.context,
		    macWin->xOff + segments[i].x1 + o,
		    macWin->yOff + segments[i].y1 + o);
	    CGContextAddLineToPoint(dc.context,
		    macWin->xOff + segments[i].x2 + o,
		    macWin->yOff + segments[i].y2 + o);
	    CGContextStrokePath(dc.context);
	}
    } else {
	int o = -lw/2;

	/* This is broken for fat lines, it is not possible to correctly
	 * imitate X11 drawing of oblique fat lines with QD line drawing,
	 * we should draw a filled polygon instead. */

	for (i = 0; i < nsegments; i++) {
	    MoveTo((short) (macWin->xOff + segments[i].x1 + o),
		   (short) (macWin->yOff + segments[i].y1 + o));
	    LineTo((short) (macWin->xOff + segments[i].x2 + o),
		   (short) (macWin->yOff + segments[i].y2 + o));
	}
    }
    TkMacOSXRestoreDrawingContext(&dc);
}

/*
 *----------------------------------------------------------------------
 *
 * XFillPolygon --
 *
 *	Draws a filled polygon.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws a filled polygon on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XFillPolygon(
    Display* display,		/* Display. */
    Drawable d,			/* Draw on this. */
    GC gc,			/* Use this GC. */
    XPoint* points,		/* Array of points. */
    int npoints,		/* Number of points. */
    int shape,			/* Shape to draw. */
    int mode)			/* Drawing mode. */
{
    MacDrawable *macWin = (MacDrawable *) d;
    TkMacOSXDrawingContext dc;
    int i;

    display->request++;
    if (TkMacOSXSetupDrawingContext(d, gc, useCGDrawing, &dc)) {
	double prevx, prevy;
	double o = (gc->line_width % 2) ? .5 : 0;

	CGContextBeginPath(dc.context);
	prevx = macWin->xOff + points[0].x + o;
	prevy = macWin->yOff + points[0].y + o;
	CGContextMoveToPoint(dc.context, prevx, prevy);
	for (i = 1; i < npoints; i++) {
	    if (mode == CoordModeOrigin) {
		CGContextAddLineToPoint(dc.context,
			macWin->xOff + points[i].x + o,
			macWin->yOff + points[i].y + o);
	    } else {
		prevx += points[i].x;
		prevy += points[i].y;
		CGContextAddLineToPoint(dc.context, prevx, prevy);
	    }
	}
	CGContextEOFillPath(dc.context);
    } else {
	PolyHandle polygon;

	polygon = OpenPoly();
	MoveTo((short) (macWin->xOff + points[0].x),
	       (short) (macWin->yOff + points[0].y));
	for (i = 1; i < npoints; i++) {
	    if (mode == CoordModeOrigin) {
		LineTo((short) (macWin->xOff + points[i].x),
		       (short) (macWin->yOff + points[i].y));
	    } else {
		Line((short) points[i].x, (short) points[i].y);
	    }
	}
	ClosePoly();
	FillCPoly(polygon, dc.penPat);
	KillPoly(polygon);
    }
    TkMacOSXRestoreDrawingContext(&dc);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawRectangle --
 *
 *	Draws a rectangle.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws a rectangle on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XDrawRectangle(
    Display *display,		/* Display. */
    Drawable d,			/* Draw on this. */
    GC gc,			/* Use this GC. */
    int x, int y,		/* Upper left corner. */
    unsigned int width,		/* Width & height of rect. */
    unsigned int height)
{
    MacDrawable *macWin = (MacDrawable *) d;
    TkMacOSXDrawingContext dc;
    int lw = gc->line_width;

    if (width == 0 || height == 0) {
	return;
    }

    display->request++;
    if (TkMacOSXSetupDrawingContext(d, gc, useCGDrawing, &dc)) {
	CGRect rect;
	double o = (lw % 2) ? .5 : 0;

	rect = CGRectMake(
		macWin->xOff + x + o,
		macWin->yOff + y + o,
		width, height);
	CGContextStrokeRect(dc.context, rect);
    } else {
	Rect theRect;
	int o = -lw/2;

	theRect.left =	 (short) (macWin->xOff + x + o);
	theRect.top =	 (short) (macWin->yOff + y + o);
	theRect.right =	 (short) (theRect.left + width	+ lw);
	theRect.bottom = (short) (theRect.top  + height + lw);
	FrameRect(&theRect);
    }
    TkMacOSXRestoreDrawingContext(&dc);
}

#ifdef TK_MACOSXDRAW_UNUSED
/*
 *----------------------------------------------------------------------
 *
 * XDrawRectangles --
 *
 *	Draws the outlines of the specified rectangles as if a
 *	five-point PolyLine protocol request were specified for each
 *	rectangle:
 *
 *	    [x,y] [x+width,y] [x+width,y+height] [x,y+height] [x,y]
 *
 *	For the specified rectangles, these functions do not draw a
 *	pixel more than once. XDrawRectangles draws the rectangles in
 *	the order listed in the array. If rectangles intersect, the
 *	intersecting pixels are drawn multiple times. Draws a
 *	rectangle.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws rectangles on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XDrawRectangles(
    Display *display,
    Drawable drawable,
    GC gc,
    XRectangle *rectArr,
    int nRects)
{
    MacDrawable *macWin = (MacDrawable *) drawable;
    TkMacOSXDrawingContext dc;
    XRectangle * rectPtr;
    int i, lw = gc->line_width;

    display->request++;
    if (TkMacOSXSetupDrawingContext(d, gc, useCGDrawing, &dc)) {
	CGRect rect;
	double o = (lw % 2) ? .5 : 0;

	for (i = 0, rectPtr = rectArr; i < nRects; i++, rectPtr++) {
	    if (rectPtr->width == 0 || rectPtr->height == 0) {
		continue;
	    }
	    rect = CGRectMake(
		    macWin->xOff + rectPtr->x + o,
		    macWin->yOff + rectPtr->y + o,
		    rectPtr->width, rectPtr->height);
	    CGContextStrokeRect(dc.context, rect);
	}
    } else {
	Rect theRect;
	int o = -lw/2;

	for (i = 0, rectPtr = rectArr; i < nRects;i++, rectPtr++) {
	    theRect.left =   (short) (macWin->xOff + rectPtr->x + o);
	    theRect.top =    (short) (macWin->yOff + rectPtr->y + o);
	    theRect.right =  (short) (theRect.left + rectPtr->width  + lw);
	    theRect.bottom = (short) (theRect.top  + rectPtr->height + lw);
	    FrameRect(&theRect);
	}
    }
    TkMacOSXRestoreDrawingContext(&dc);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * XFillRectangles --
 *
 *	Fill multiple rectangular areas in the given drawable.
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
XFillRectangles(
    Display* display,		/* Display. */
    Drawable d,			/* Draw on this. */
    GC gc,			/* Use this GC. */
    XRectangle *rectangles,	/* Rectangle array. */
    int n_rectangles)		/* Number of rectangles. */
{
    MacDrawable *macWin = (MacDrawable *) d;
    TkMacOSXDrawingContext dc;
    XRectangle * rectPtr;
    int i;

    display->request++;
    if (TkMacOSXSetupDrawingContext(d, gc, useCGDrawing, &dc)) {
	CGRect rect;

	for (i = 0, rectPtr = rectangles; i < n_rectangles; i++, rectPtr++) {
	    if (rectPtr->width == 0 || rectPtr->height == 0) {
		continue;
	    }
	    rect = CGRectMake(
		    macWin->xOff + rectPtr->x,
		    macWin->yOff + rectPtr->y,
		    rectPtr->width, rectPtr->height);
	    CGContextFillRect(dc.context, rect);
	}
    } else {
	Rect theRect;

	for (i = 0, rectPtr = rectangles; i < n_rectangles; i++, rectPtr++) {
	    theRect.left =   (short) (macWin->xOff + rectPtr->x);
	    theRect.top =    (short) (macWin->yOff + rectPtr->y);
	    theRect.right =  (short) (theRect.left + rectPtr->width);
	    theRect.bottom = (short) (theRect.top  + rectPtr->height);
	    FillCRect(&theRect, dc.penPat);
	}
    }
    TkMacOSXRestoreDrawingContext(&dc);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawArc --
 *
 *	Draw an arc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws an arc on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XDrawArc(
    Display* display,		/* Display. */
    Drawable d,			/* Draw on this. */
    GC gc,			/* Use this GC. */
    int x, int y,		/* Upper left of bounding rect. */
    unsigned int width,		/* Width & height. */
    unsigned int height,
    int angle1,			/* Staring angle of arc. */
    int angle2)			/* Extent of arc. */
{
    MacDrawable *macWin = (MacDrawable *) d;
    TkMacOSXDrawingContext dc;
    int lw = gc->line_width;

    if (width == 0 || height == 0 || angle2 == 0) {
	return;
    }

    display->request++;
    if (TkMacOSXSetupDrawingContext(d, gc, useCGDrawing, &dc)) {
	CGRect rect;
	double o = (lw % 2) ? .5 : 0;

	rect = CGRectMake(
		macWin->xOff + x + o,
		macWin->yOff + y + o,
		width, height);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
	if (angle1 == 0 && angle2 == 23040
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1040
		&& CGContextStrokeEllipseInRect != NULL
#endif
	) {
	    CGContextStrokeEllipseInRect(dc.context, rect);
	} else
#endif
	{
	    CGMutablePathRef p = CGPathCreateMutable();
	    CGAffineTransform t = CGAffineTransformIdentity;
	    CGPoint c = CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
	    double w = CGRectGetWidth(rect);

	    if (width != height) {
		t = CGAffineTransformMakeScale(1.0, CGRectGetHeight(rect)/w);
		c = CGPointApplyAffineTransform(c, CGAffineTransformInvert(t));
	    }
	    CGPathAddArc(p, &t, c.x, c.y, w/2, radians(-angle1/64.0),
		    radians(-(angle1 + angle2)/64.0), angle2 > 0);
	    CGContextAddPath(dc.context, p);
	    CGPathRelease(p);
	    CGContextStrokePath(dc.context);
	}
    } else {
	Rect theRect;
	short start, extent;
	int o = -lw/2;

	theRect.left   = (short) (macWin->xOff + x + o);
	theRect.top    = (short) (macWin->yOff + y + o);
	theRect.right  = (short) (theRect.left + width + lw);
	theRect.bottom = (short) (theRect.top + height + lw);
	start  = (short) (90 - (angle1/64));
	extent = (short) (-(angle2/64));
	FrameArc(&theRect, start, extent);
    }
    TkMacOSXRestoreDrawingContext(&dc);
}

#ifdef TK_MACOSXDRAW_UNUSED
/*
 *----------------------------------------------------------------------
 *
 * XDrawArcs --
 *
 *	Draws multiple circular or elliptical arcs. Each arc is
 *	specified by a rectangle and two angles. The center of the
 *	circle or ellipse is the center of the rect- angle, and the
 *	major and minor axes are specified by the width and height.
 *	Positive angles indicate counterclock- wise motion, and
 *	negative angles indicate clockwise motion. If the magnitude
 *	of angle2 is greater than 360 degrees, XDrawArcs truncates it
 *	to 360 degrees.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws an arc for each array element on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XDrawArcs(
    Display *display,
    Drawable d,
    GC gc,
    XArc *arcArr,
    int nArcs)
{

    MacDrawable *macWin = (MacDrawable *) d;
    TkMacOSXDrawingContext dc;
    XArc *arcPtr;
    int i, lw = gc->line_width;

    display->request++;
    if (TkMacOSXSetupDrawingContext(d, gc, useCGDrawing, &dc)) {
	CGRect rect;
	double o = (lw % 2) ? .5 : 0;

	for (i=0, arcPtr = arcArr; i < nArcs; i++, arcPtr++) {
	    if (arcPtr->width == 0 || arcPtr->height == 0
		    || arcPtr->angle2 == 0) {
		continue;
	    }
	    rect = CGRectMake(
		    macWin->xOff + arcPtr->x + o,
		    macWin->yOff + arcPtr->y + o,
		    arcPtr->width, arcPtr->height);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
	    if (arcPtr->angle1 == 0 && arcPtr->angle2 == 23040
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1040
		    && CGContextStrokeEllipseInRect != NULL
#endif
	    ) {
		CGContextStrokeEllipseInRect(dc.context, rect);
	    } else
#endif
	    {
		CGMutablePathRef p = CGPathCreateMutable();
		CGAffineTransform t = CGAffineTransformIdentity;
		CGPoint c = CGPointMake(CGRectGetMidX(rect),
			CGRectGetMidY(rect));
		double w = CGRectGetWidth(rect);

		if (arcPtr->width != arcPtr->height) {
		    t = CGAffineTransformMakeScale(1, CGRectGetHeight(rect)/w);
		    c = CGPointApplyAffineTransform(c,
			    CGAffineTransformInvert(t));
		}
		CGPathAddArc(p, &t, c.x, c.y, w/2,
			radians(-arcPtr->angle1/64.0),
			radians(-(arcPtr->angle1 + arcPtr->angle2)/64.0),
			arcPtr->angle2 > 0);
		CGContextAddPath(dc.context, p);
		CGPathRelease(p);
		CGContextStrokePath(dc.context);
	    }
	}
    } else {
	Rect theRect;
	short start, extent;
	int o = -lw/2;

	for (i = 0, arcPtr = arcArr;i < nArcs;i++, arcPtr++) {
	    theRect.left =   (short) (macWin->xOff + arcPtr->x + o);
	    theRect.top =    (short) (macWin->yOff + arcPtr->y + o);
	    theRect.right =  (short) (theRect.left + arcPtr->width + lw);
	    theRect.bottom = (short) (theRect.top + arcPtr->height + lw);
	    start =  (short) (90 - (arcPtr->angle1/64));
	    extent = (short) (-(arcPtr->angle2/64));
	    FrameArc(&theRect, start, extent);
	}
    }
    TkMacOSXRestoreDrawingContext(&dc);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * XFillArc --
 *
 *	Draw a filled arc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws a filled arc on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XFillArc(
    Display* display,		/* Display. */
    Drawable d,			/* Draw on this. */
    GC gc,			/* Use this GC. */
    int x, int y,		/* Upper left of bounding rect. */
    unsigned int width,		/* Width & height. */
    unsigned int height,
    int angle1,			/* Staring angle of arc. */
    int angle2)			/* Extent of arc. */
{
    MacDrawable *macWin = (MacDrawable *) d;
    TkMacOSXDrawingContext dc;
    int lw = gc->line_width;

    if (width == 0 || height == 0 || angle2 == 0) {
	return;
    }

    display->request++;
    if (TkMacOSXSetupDrawingContext(d, gc, useCGDrawing, &dc)) {
	CGRect rect;
	double o = (lw % 2) ? .5 : 0, u = 0;

	if (notAA(lw)) {
	    o += NON_AA_CG_OFFSET/2;
	    u += NON_AA_CG_OFFSET;
	}
	rect = CGRectMake(
		macWin->xOff + x + o,
		macWin->yOff + y + o,
		width - u, height - u);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
	if (angle1 == 0 && angle2 == 23040
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1040
		&& CGContextFillEllipseInRect != NULL
#endif
	) {
	    CGContextFillEllipseInRect(dc.context, rect);
	} else
#endif
	{
	    CGMutablePathRef p = CGPathCreateMutable();
	    CGAffineTransform t = CGAffineTransformIdentity;
	    CGPoint c = CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
	    double w = CGRectGetWidth(rect);

	    if (width != height) {
		t = CGAffineTransformMakeScale(1, CGRectGetHeight(rect)/w);
		c = CGPointApplyAffineTransform(c, CGAffineTransformInvert(t));
	    }
	    if (gc->arc_mode == ArcPieSlice) {
		CGPathMoveToPoint(p, &t, c.x, c.y);
	    }
	    CGPathAddArc(p, &t, c.x, c.y, w/2, radians(-angle1/64.0),
		    radians(-(angle1 + angle2)/64.0), angle2 > 0);
	    CGPathCloseSubpath(p);
	    CGContextAddPath(dc.context, p);
	    CGPathRelease(p);
	    CGContextFillPath(dc.context);
	}
    } else {
	Rect theRect;
	short start, extent;
	int o = -lw/2;
	PolyHandle polygon;
	double sin1, cos1, sin2, cos2, angle;
	double boxWidth, boxHeight;
	double vertex[2], center1[2], center2[2];

	theRect.left =	 (short) (macWin->xOff + x + o);
	theRect.top =	 (short) (macWin->yOff + y + o);
	theRect.right =	 (short) (theRect.left + width + lw);
	theRect.bottom = (short) (theRect.top + height + lw);
	start = (short) (90 - (angle1/64));
	extent = (short) (-(angle2/64));
	if (gc->arc_mode == ArcChord) {
	    boxWidth = theRect.right - theRect.left;
	    boxHeight = theRect.bottom - theRect.top;
	    angle = radians(-angle1/64.0);
	    sin1 = sin(angle);
	    cos1 = cos(angle);
	    angle -= radians(angle2/64.0);
	    sin2 = sin(angle);
	    cos2 = cos(angle);
	    vertex[0] = (theRect.left + theRect.right)/2.0;
	    vertex[1] = (theRect.top + theRect.bottom)/2.0;
	    center1[0] = vertex[0] + cos1*boxWidth/2.0;
	    center1[1] = vertex[1] + sin1*boxHeight/2.0;
	    center2[0] = vertex[0] + cos2*boxWidth/2.0;
	    center2[1] = vertex[1] + sin2*boxHeight/2.0;

	    polygon = OpenPoly();
	    MoveTo((short) ((theRect.left + theRect.right)/2),
		   (short) ((theRect.top + theRect.bottom)/2));
	    LineTo((short) (center1[0] + .5), (short) (center1[1] + .5));
	    LineTo((short) (center2[0] + .5), (short) (center2[1] + .5));
	    ClosePoly();
	    FillCArc(&theRect, start, extent, dc.penPat);
	    FillCPoly(polygon, dc.penPat);
	    KillPoly(polygon);
	} else {
	    FillCArc(&theRect, start, extent, dc.penPat);
	}
    }
    TkMacOSXRestoreDrawingContext(&dc);
}

#ifdef TK_MACOSXDRAW_UNUSED
/*
 *----------------------------------------------------------------------
 *
 * XFillArcs --
 *
 *	Draw a filled arc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws a filled arc for each array element on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XFillArcs(
    Display *display,
    Drawable d,
    GC gc,
    XArc *arcArr,
    int nArcs)
{
    MacDrawable *macWin = (MacDrawable *) d;
    TkMacOSXDrawingContext dc;
    XArc * arcPtr;
    int i, lw = gc->line_width;

    display->request++;
    if (TkMacOSXSetupDrawingContext(d, gc, useCGDrawing, &dc)) {
	CGRect rect;
	double o = (lw % 2) ? .5 : 0, u = 0;

	if (notAA(lw)) {
	    o += NON_AA_CG_OFFSET/2;
	    u += NON_AA_CG_OFFSET;
	}
	for (i = 0, arcPtr = arcArr; i < nArcs; i++, arcPtr++) {
	    if (arcPtr->width == 0 || arcPtr->height == 0
		    || arcPtr->angle2 == 0) {
		continue;
	    }
	    rect = CGRectMake(
		    macWin->xOff + arcPtr->x + o,
		    macWin->yOff + arcPtr->y + o,
		    arcPtr->width - u, arcPtr->height - u);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
	    if (arcPtr->angle1 == 0 && arcPtr->angle2 == 23040
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1040
		    && CGContextFillEllipseInRect != NULL
#endif
	    ) {
		CGContextFillEllipseInRect(dc.context, rect);
	    } else
#endif
	    {
		CGMutablePathRef p = CGPathCreateMutable();
		CGAffineTransform t = CGAffineTransformIdentity;
		CGPoint c = CGPointMake(CGRectGetMidX(rect),
			CGRectGetMidY(rect));
		double w = CGRectGetWidth(rect);

		if (arcPtr->width != arcPtr->height) {
		    t = CGAffineTransformMakeScale(1, CGRectGetHeight(rect)/w);
		    c = CGPointApplyAffineTransform(c,
			    CGAffineTransformInvert(t));
		}
		if (gc->arc_mode == ArcPieSlice) {
		    CGPathMoveToPoint(p, &t, c.x, c.y);
		}
		CGPathAddArc(p, &t, c.x, c.y, w/2,
			radians(-arcPtr->angle1/64.0),
			radians(-(arcPtr->angle1 + arcPtr->angle2)/64.0),
			arcPtr->angle2 > 0);
		CGPathCloseSubpath(p);
		CGContextAddPath(dc.context, p);
		CGPathRelease(p);
		CGContextFillPath(dc.context);
	    }
	}
    } else {
	Rect theRect;
	short start, extent;
	int o = -lw/2;
	PolyHandle polygon;
	double sin1, cos1, sin2, cos2, angle;
	double boxWidth, boxHeight;
	double vertex[2], center1[2], center2[2];

	for (i = 0, arcPtr = arcArr;i<nArcs;i++, arcPtr++) {
	    theRect.left =   (short) (macWin->xOff + arcPtr->x + o);
	    theRect.top =    (short) (macWin->yOff + arcPtr->y + o);
	    theRect.right =  (short) (theRect.left + arcPtr->width + lw);
	    theRect.bottom = (short) (theRect.top + arcPtr->height + lw);
	    start = (short) (90 - (arcPtr->angle1/64));
	    extent = (short) (- (arcPtr->angle2/64));

	    if (gc->arc_mode == ArcChord) {
		boxWidth = theRect.right - theRect.left;
		boxHeight = theRect.bottom - theRect.top;
		angle = radians(-arcPtr->angle1/64.0);
		sin1 = sin(angle);
		cos1 = cos(angle);
		angle -= radians(arcPtr->angle2/64.0);
		sin2 = sin(angle);
		cos2 = cos(angle);
		vertex[0] = (theRect.left + theRect.right)/2.0;
		vertex[1] = (theRect.top + theRect.bottom)/2.0;
		center1[0] = vertex[0] + cos1*boxWidth/2.0;
		center1[1] = vertex[1] + sin1*boxHeight/2.0;
		center2[0] = vertex[0] + cos2*boxWidth/2.0;
		center2[1] = vertex[1] + sin2*boxHeight/2.0;

		polygon = OpenPoly();
		MoveTo((short) ((theRect.left + theRect.right)/2),
		       (short) ((theRect.top + theRect.bottom)/2));
		LineTo((short) (center1[0] + .5), (short) (center1[1] + .5));
		LineTo((short) (center2[0] + .5), (short) (center2[1] + .5));
		ClosePoly();
		FillCArc(&theRect, start, extent, dc.penPat);
		FillCPoly(polygon, dc.penPat);
		KillPoly(polygon);
	    } else {
		FillCArc(&theRect, start, extent, dc.penPat);
	    }
	}
    }
    TkMacOSXRestoreDrawingContext(&dc);
}
#endif

#ifdef TK_MACOSXDRAW_UNUSED
/*
 *----------------------------------------------------------------------
 *
 * XMaxRequestSize --
 *
 *----------------------------------------------------------------------
 */

long
XMaxRequestSize(
    Display *display)
{
    return (SHRT_MAX / 4);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TkScrollWindow --
 *
 *	Scroll a rectangle of the specified window and accumulate
 *	a damage region.
 *
 * Results:
 *	Returns 0 if the scroll genereated no additional damage.
 *	Otherwise, sets the region that needs to be repainted after
 *	scrolling and returns 1.
 *
 * Side effects:
 *	Scrolls the bits in the window.
 *
 *----------------------------------------------------------------------
 */

int
TkScrollWindow(
    Tk_Window tkwin,		/* The window to be scrolled. */
    GC gc,			/* GC for window to be scrolled. */
    int x, int y,		/* Position rectangle to be scrolled. */
    int width, int height,
    int dx, int dy,		/* Distance rectangle should be moved. */
    TkRegion damageRgn)		/* Region to accumulate damage in. */
{
    MacDrawable *destDraw = (MacDrawable *) Tk_WindowId(tkwin);
    RgnHandle rgn = (RgnHandle) damageRgn;
    CGrafPtr destPort, savePort;
    Boolean portChanged;
    Rect srcRect, scrollRect;

    destPort = TkMacOSXGetDrawablePort(Tk_WindowId(tkwin));
    portChanged = QDSwapPort(destPort, &savePort);
    TkMacOSXSetUpClippingRgn(Tk_WindowId(tkwin));

    /*
     * Due to the implementation below the behavior may be differnt
     * than X in certain cases that should never occur in Tk. The
     * scrollRect is the source rect extended by the offset (the union
     * of the source rect and the offset rect). Everything
     * in the extended scrollRect is scrolled. On X, it's possible
     * to "skip" over an area if the offset makes the source and
     * destination rects disjoint and non-aligned.
     */

    SetRect(&srcRect, (short) (destDraw->xOff + x),
	    (short) (destDraw->yOff + y),
	    (short) (destDraw->xOff + x + width),
	    (short) (destDraw->yOff + y + height));
    scrollRect = srcRect;
    if (dx < 0) {
	scrollRect.left += dx;
    } else {
	scrollRect.right += dx;
    }
    if (dy < 0) {
	scrollRect.top += dy;
    } else {
	scrollRect.bottom += dy;
    }

    /*
     * Adjust clip region so that we don't copy any windows
     * that may overlap us.
     */

    TkMacOSXCheckTmpRgnEmpty(1);
    TkMacOSXCheckTmpRgnEmpty(2);
    RectRgn(rgn, &srcRect);
    GetPortVisibleRegion(destPort,tkMacOSXtmpRgn1);
    DiffRgn(rgn, tkMacOSXtmpRgn1, rgn);
    OffsetRgn(rgn, dx, dy);
    GetPortClipRegion(destPort, tkMacOSXtmpRgn2);
    DiffRgn(tkMacOSXtmpRgn2, rgn, tkMacOSXtmpRgn2);
    SetPortClipRegion(destPort, tkMacOSXtmpRgn2);
    SetEmptyRgn(tkMacOSXtmpRgn1);
    SetEmptyRgn(tkMacOSXtmpRgn2);
    SetEmptyRgn(rgn);

    ScrollRect(&scrollRect, dx, dy, rgn);
    if (portChanged) {
	QDSwapPort(savePort, NULL);
    }
    /*
     * Fortunately, the region returned by ScrollRect is semantically
     * the same as what we need to return in this function. If the
     * region is empty we return zero to denote that no damage was
     * created.
     */
    if (EmptyRgn(rgn)) {
	return 0;
    } else {
	return 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXSetUpGraphicsPort --
 *
 *	Set up the graphics port from the given GC.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The current port is adjusted.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXSetUpGraphicsPort(
    GC gc,			/* GC to apply to current port. */
    GWorldPtr destPort)
{
    PenNormal();
    if (gc) {
	if (gPenPat == NULL) {
	    gPenPat = NewPixPat();
	}
	TkMacOSXSetColorInPort(gc->foreground, 1, gPenPat);
	PenPixPat(gPenPat);
	if(gc->function == GXxor) {
	    PenMode(patXor);
	}
	if (gc->line_width > 1) {
	    PenSize(gc->line_width, gc->line_width);
	}
	if (gc->line_style != LineSolid) {
	    /*
	     * FIXME:
	     * Here the dash pattern should be set in the drawing environment.
	     * This is not possible with QuickDraw line drawing.
	     */
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXSetUpDrawingContext --
 *
 *	Set up a drawing context for the given drawable and GC.
 *
 * Results:
 *	Boolean indicating whether to use CG drawing.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkMacOSXSetupDrawingContext(Drawable d, GC gc, int useCG,
	TkMacOSXDrawingContext *dc)
{
    MacDrawable *macDraw = ((MacDrawable*)d);
    CGContextRef context = macDraw->context;
    CGrafPtr port;
    Rect portBounds;

    port = TkMacOSXGetDrawablePort(d);
    if (port) {
	GetPortBounds(port, &portBounds);
    }

    dc->saveState = NULL;
    if (port && !context) {
	dc->portChanged = QDSwapPort(port, &(dc->savePort));
	TkMacOSXSetUpClippingRgn(d);
	TkMacOSXCheckTmpRgnEmpty(1);
	if (useCG) {
	    if (ChkErr(QDBeginCGContext, port, &context) == noErr) {
		/*
		 * Now clip the CG Context to the port. Note, we have already
		 * set up the port with our clip region, so we can just get
		 * the clip back out of there. If we use the macWin->clipRgn
		 * directly at this point, we get some odd drawing effects.
		 *
		 * We also have to intersect our clip region with the port
		 * visible region so we don't overwrite the window decoration.
		 */

		RectRgn(tkMacOSXtmpRgn1, &portBounds);
		SectRegionWithPortClipRegion(port, tkMacOSXtmpRgn1);
		SectRegionWithPortVisibleRegion(port, tkMacOSXtmpRgn1);
		if (gc && gc->clip_mask && ((TkpClipMask*)gc->clip_mask)->type
			== TKP_CLIP_REGION) {
		    RgnHandle clipRgn = (RgnHandle)
			    ((TkpClipMask*)gc->clip_mask)->value.region;
		    int xOffset = macDraw->xOff + gc->clip_x_origin;
		    int yOffset = macDraw->yOff + gc->clip_y_origin;

		    OffsetRgn(clipRgn, xOffset, yOffset);
		    SectRgn(clipRgn, tkMacOSXtmpRgn1, tkMacOSXtmpRgn1);
		    OffsetRgn(clipRgn, -xOffset, -yOffset);
		}
		ClipCGContextToRegion(context, &portBounds, tkMacOSXtmpRgn1);
		SetEmptyRgn(tkMacOSXtmpRgn1);

		/*
		 * Note: You have to call SyncCGContextOriginWithPort
		 * AFTER all the clip region manipulations.
		 */

		SyncCGContextOriginWithPort(context, port);
	    } else {
		context = NULL;
		useCG = 0;
	    }
	}
    } else if (context) {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
	if (!port
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1030
		&& CGContextGetClipBoundingBox != NULL
#endif
	) {
	    CGRect r = CGContextGetClipBoundingBox(context);

	    SetRect(&portBounds, r.origin.x + macDraw->xOff,
		    r.origin.y + macDraw->yOff,
		    r.origin.x + r.size.width + macDraw->xOff,
		    r.origin.y + r.size.height + macDraw->yOff);
	}
#endif
	CGContextSaveGState(context);
	TkMacOSXCheckTmpRgnEmpty(1);
	RectRgn(tkMacOSXtmpRgn1, &portBounds);
	if (port) {
	    TkMacOSXSetUpClippingRgn(d);
	    SectRegionWithPortClipRegion(port, tkMacOSXtmpRgn1);
	    SectRegionWithPortVisibleRegion(port, tkMacOSXtmpRgn1);
	} else if (macDraw->flags & TK_CLIPPED_DRAW) {
	    OffsetRgn(macDraw->drawRgn, macDraw->xOff, macDraw->yOff);
	    SectRgn(macDraw->clipRgn, macDraw->drawRgn, tkMacOSXtmpRgn1);
	    OffsetRgn(macDraw->drawRgn, -macDraw->xOff, -macDraw->yOff);
	}
	if (gc && gc->clip_mask && ((TkpClipMask*)gc->clip_mask)->type
		== TKP_CLIP_REGION) {
	    RgnHandle clipRgn = (RgnHandle)
		    ((TkpClipMask*)gc->clip_mask)->value.region;
	    int xOffset = macDraw->xOff + gc->clip_x_origin;
	    int yOffset = macDraw->yOff + gc->clip_y_origin;

	    OffsetRgn(clipRgn, xOffset, yOffset);
	    SectRgn(clipRgn, tkMacOSXtmpRgn1, tkMacOSXtmpRgn1);
	    OffsetRgn(clipRgn, -xOffset, -yOffset);
	}
	ClipCGContextToRegion(context, &portBounds, tkMacOSXtmpRgn1);
	SetEmptyRgn(tkMacOSXtmpRgn1);
	port = NULL;
	dc->portChanged = false;
	dc->saveState = (void*)1;
	useCG = 1;
    }
    if (useCG) {
	CGContextConcatCTM(context, CGAffineTransformMake(1.0, 0.0, 0.0, -1.0,
		0.0, portBounds.bottom - portBounds.top));
	if (gc) {
	    double w = gc->line_width;

	    TkMacOSXSetColorInContext(gc->foreground, context);
	    if (port) {
		CGContextSetPatternPhase(context, CGSizeMake(portBounds.right -
			portBounds.left, portBounds.bottom - portBounds.top));
	    }
	    if(gc->function == GXxor) {
		TkMacOSXDbgMsg("GXxor mode not supported for CG drawing!");
	    }
	    /* When should we antialias? */
	    if (notAA(gc->line_width)) {
		/* Make non-antialiased CG drawing look more like X11 */
		w -= (gc->line_width ? NON_AA_CG_OFFSET : 0);
		CGContextSetShouldAntialias(context, 0);
	    } else {
		CGContextSetShouldAntialias(context, 1);
	    }
	    CGContextSetLineWidth(context, w);

	    if (gc->line_style != LineSolid) {
		int num = 0;
		char *p = &(gc->dashes);
		double dashOffset = gc->dash_offset;
		float lengths[10];

		while (p[num] != '\0' && num < 10) {
		    lengths[num] = p[num];
		    num++;
		}
		CGContextSetLineDash(context, dashOffset, lengths, num);
	    }

	    if (gc->cap_style == CapButt) {
		/*
		 *  What about CapNotLast, CapProjecting?
		 */

		CGContextSetLineCap(context, kCGLineCapButt);
	    } else if (gc->cap_style == CapRound) {
		CGContextSetLineCap(context, kCGLineCapRound);
	    } else if (gc->cap_style == CapProjecting) {
		CGContextSetLineCap(context, kCGLineCapSquare);
	    }

	    if (gc->join_style == JoinMiter) {
		CGContextSetLineJoin(context, kCGLineJoinMiter);
	    } else if (gc->join_style == JoinRound) {
		CGContextSetLineJoin(context, kCGLineJoinRound);
	    } else if (gc->join_style == JoinBevel) {
		CGContextSetLineJoin(context, kCGLineJoinBevel);
	    }
	}
    } else {
	ChkErr(GetThemeDrawingState, &(dc->saveState));
	if (gc) {
	    PixPatHandle savePat = gPenPat;

	    gPenPat = NULL;
	    TkMacOSXSetUpGraphicsPort(gc, port);
	    dc->penPat = gPenPat;
	    gPenPat = savePat;
	    if (gc->clip_mask && ((TkpClipMask*)gc->clip_mask)->type
			== TKP_CLIP_REGION) {
		RgnHandle clipRgn = (RgnHandle)
			((TkpClipMask*)gc->clip_mask)->value.region;
		int xOffset = macDraw->xOff + gc->clip_x_origin;
		int yOffset = macDraw->yOff + gc->clip_y_origin;

		OffsetRgn(clipRgn, xOffset, yOffset);
		GetClip(tkMacOSXtmpRgn1);
		SectRgn(clipRgn, tkMacOSXtmpRgn1, tkMacOSXtmpRgn1);
		SetClip(tkMacOSXtmpRgn1);
		SetEmptyRgn(tkMacOSXtmpRgn1);
		OffsetRgn(clipRgn, -xOffset, -yOffset);
	    }
	} else {
	    TkMacOSXSetUpGraphicsPort(NULL, port);
	    dc->penPat = NULL;
	}
	ShowPen();
    }
    dc->port = port;
    dc->portBounds = portBounds;
    dc->context = context;
    return useCG;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXRestoreDrawingContext --
 *
 *	Restore drawing context.
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
TkMacOSXRestoreDrawingContext(TkMacOSXDrawingContext *dc)
{
    if (dc->context) {
	CGContextSynchronize(dc->context);
	if (dc->saveState) {
	    CGContextRestoreGState(dc->context);
	}
	if (dc->port) {
	    ChkErr(QDEndCGContext, dc->port, &(dc->context));
	}
    } else {
	HidePen();
	PenNormal();
	if (dc->penPat) {
	    DisposePixPat(dc->penPat);
	}
	if (dc->saveState) {
	    ChkErr(SetThemeDrawingState, dc->saveState, true);
	}
    }
    if (dc->portChanged) {
	QDSwapPort(dc->savePort, NULL);
    }
#ifdef TK_MAC_DEBUG
    bzero(dc, sizeof(dc));
#endif /* TK_MAC_DEBUG */
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXSetUpClippingRgn --
 *
 *	Set up the clipping region so that drawing only occurs on the
 *	specified X subwindow.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The clipping region in the current port is changed.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXSetUpClippingRgn(
    Drawable drawable)		/* Drawable to update. */
{
    MacDrawable *macDraw = (MacDrawable *) drawable;

    if (macDraw->winPtr != NULL) {
	if (macDraw->flags & TK_CLIP_INVALID) {
	    TkMacOSXUpdateClipRgn(macDraw->winPtr);
	}

#ifdef TK_MAC_DEBUG_DRAWING
	TkMacOSXInitNamedDebugSymbol(HIToolbox, int, QDDebugFlashRegion,
		CGrafPtr port, RgnHandle region);
	if (QDDebugFlashRegion) {
	    CGrafPtr grafPtr = TkMacOSXGetDrawablePort(drawable);

	    /*
	     * Carbon-internal region flashing SPI (c.f. Technote 2124)
	     */

	    QDDebugFlashRegion(grafPtr, macDraw->clipRgn);
	}
#endif /* TK_MAC_DEBUG_DRAWING */
    }

    if (macDraw->clipRgn != NULL) {
	if (macDraw->flags & TK_CLIPPED_DRAW) {
	    TkMacOSXCheckTmpRgnEmpty(1);
	    OffsetRgn(macDraw->drawRgn, macDraw->xOff, macDraw->yOff);
	    SectRgn(macDraw->clipRgn, macDraw->drawRgn, tkMacOSXtmpRgn1);
	    OffsetRgn(macDraw->drawRgn, -macDraw->xOff, -macDraw->yOff);
	    SetClip(tkMacOSXtmpRgn1);
	    SetEmptyRgn(tkMacOSXtmpRgn1);
	} else {
	    SetClip(macDraw->clipRgn);
	}
    } else if (macDraw->flags & TK_CLIPPED_DRAW) {
	OffsetRgn(macDraw->drawRgn, macDraw->xOff, macDraw->yOff);
	SetClip(macDraw->drawRgn);
	OffsetRgn(macDraw->drawRgn, -macDraw->xOff, -macDraw->yOff);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpClipDrawableToRect --
 *
 *	Clip all drawing into the drawable d to the given rectangle.
 *	If width and height are negative, reset to no clipping.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Subsequent drawing into d is offset and clipped as specified.
 *
 *----------------------------------------------------------------------
 */

void
TkpClipDrawableToRect(
    Display *display,
    Drawable d,
    int x, int y,
    int width, int height)
{
    MacDrawable *macDraw = (MacDrawable *) d;

    if (macDraw->drawRgn) {
	if (width < 0 && height < 0) {
	    SetEmptyRgn(macDraw->drawRgn);
	    macDraw->flags &= ~TK_CLIPPED_DRAW;
	} else {
	    SetRectRgn(macDraw->drawRgn, x, y, x + width, y + height);
	    macDraw->flags |= TK_CLIPPED_DRAW;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXMakeStippleMap --
 *
 *	Given a drawable and a stipple pattern this function draws the
 *	pattern repeatedly over the drawable. The drawable can then
 *	be used as a mask for bit-bliting a stipple pattern over an
 *	object.
 *
 * Results:
 *	A BitMap data structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

BitMapPtr
TkMacOSXMakeStippleMap(
    Drawable drawable,		/* Window to apply stipple. */
    Drawable stipple)		/* The stipple pattern. */
{
    CGrafPtr stipplePort;
    BitMapPtr bitmapPtr;
    const BitMap *stippleBitmap;
    Rect portRect;
    int width, height, stippleHeight, stippleWidth, i, j;
    Rect bounds;

    GetPortBounds(TkMacOSXGetDrawablePort(drawable), &portRect);
    width = portRect.right - portRect.left;
    height = portRect.bottom - portRect.top;
    bitmapPtr = (BitMap *) ckalloc(sizeof(BitMap));
    bitmapPtr->bounds.top = bitmapPtr->bounds.left = 0;
    bitmapPtr->bounds.right = (short) width;
    bitmapPtr->bounds.bottom = (short) height;
    bitmapPtr->rowBytes = (width / 8) + 1;
    bitmapPtr->baseAddr = ckalloc(height * bitmapPtr->rowBytes);

    stipplePort = TkMacOSXGetDrawablePort(stipple);
    stippleBitmap = GetPortBitMapForCopyBits(stipplePort);
    GetPortBounds(stipplePort, &portRect);
    stippleWidth = portRect.right - portRect.left;
    stippleHeight = portRect.bottom - portRect.top;

    for (i = 0; i < height; i += stippleHeight) {
	for (j = 0; j < width; j += stippleWidth) {
	    bounds.left = j;
	    bounds.top = i;
	    bounds.right = j + stippleWidth;
	    bounds.bottom = i + stippleHeight;
	    CopyBits(stippleBitmap, bitmapPtr, &portRect, &bounds, srcCopy,
		    NULL);
	}
    }
    return bitmapPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * InvertByte --
 *
 *	This function reverses the bits in the passed in Byte of data.
 *
 * Results:
 *	The incoming byte in reverse bit order.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static unsigned char
InvertByte(
    unsigned char data)		/* Byte of data. */
{
    unsigned char i;
    unsigned char mask = 1, result = 0;

    for (i = (1 << 7); i != 0; i /= 2) {
	if (data & mask) {
	    result |= i;
	}
	mask = mask << 1;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpDrawHighlightBorder --
 *
 *	This procedure draws a rectangular ring around the outside of
 *	a widget to indicate that it has received the input focus.
 *
 *	On the Macintosh, this puts a 1 pixel border in the bgGC color
 *	between the widget and the focus ring, except in the case where
 *	highlightWidth is 1, in which case the border is left out.
 *
 *	For proper Mac L&F, use highlightWidth of 3.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A rectangle "width" pixels wide is drawn in "drawable",
 *	corresponding to the outer area of "tkwin".
 *
 *----------------------------------------------------------------------
 */

void
TkpDrawHighlightBorder (
    Tk_Window tkwin,
    GC fgGC,
    GC bgGC,
    int highlightWidth,
    Drawable drawable)
{
    if (highlightWidth == 1) {
	TkDrawInsetFocusHighlight (tkwin, fgGC, highlightWidth, drawable, 0);
    } else {
	TkDrawInsetFocusHighlight (tkwin, bgGC, highlightWidth, drawable, 0);
	if (fgGC != bgGC) {
	    TkDrawInsetFocusHighlight (tkwin, fgGC, highlightWidth - 1,
		    drawable, 0);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpDrawFrame --
 *
 *	This procedure draws the rectangular frame area. If the user
 *	has request themeing, it draws with a the background theme.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws inside the tkwin area.
 *
 *----------------------------------------------------------------------
 */

void
TkpDrawFrame(
    Tk_Window tkwin,
    Tk_3DBorder border,
    int highlightWidth,
    int borderWidth,
    int relief)
{
    if (useThemedToplevel && Tk_IsTopLevel(tkwin)) {
	static Tk_3DBorder themedBorder = NULL;

	if (!themedBorder) {
	    themedBorder = Tk_Get3DBorder(NULL, tkwin,
		    "systemWindowHeaderBackground");
	}
	if (themedBorder) {
	    border = themedBorder;
	}
    }
    Tk_Fill3DRectangle(tkwin, Tk_WindowId(tkwin),
	    border, highlightWidth, highlightWidth,
	    Tk_Width(tkwin) - 2 * highlightWidth,
	    Tk_Height(tkwin) - 2 * highlightWidth,
	    borderWidth, relief);
}
