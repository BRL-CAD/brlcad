/* 
 * tkMacOSXDraw.c --
 *
 *        This file contains functions that perform drawing to
 *        Xlib windows.  Most of the functions simple emulate
 *        Xlib functions.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2006 Daniel A. Steffen <das@users.sourceforge.net>
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

#define RGBFLOATRED(c)   ((c).red   / 65535.0)
#define RGBFLOATGREEN(c) ((c).green / 65535.0)
#define RGBFLOATBLUE(c)  ((c).blue  / 65535.0)
#define radians(d)       ((d) * (M_PI/180.0))

/*
 * Non-antialiased CG drawing looks better and more like X11 drawing when using
 * very fine lines, so decrease all linewidths by the following constant.
 */
#define NON_AA_CG_OFFSET .999

/*
 * Temporary regions that can be reused.
 */

static RgnHandle tmpRgn = NULL;
static RgnHandle tmpRgn2 = NULL;
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
static void TkMacOSXSetUpCGContext(MacDrawable *macWin, CGrafPtr destPort,
	GC gc, CGContextRef *contextPtr);
static void TkMacOSXReleaseCGContext(MacDrawable *macWin, CGrafPtr destPort,
	CGContextRef *context);

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInitCGDrawing --
 *
 *        Initializes link vars that control CG drawing.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
TkMacOSXInitCGDrawing(interp, enable, limit)
	Tcl_Interp *interp;
	int enable;
	int limit;
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
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * XCopyArea --
 *
 *        Copies data from one drawable to another using block transfer
 *        routines.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Data is moved from a window or bitmap to a second window or
 *        bitmap.
 *
 *----------------------------------------------------------------------
 */

void
XCopyArea(
    Display* display,                /* Display. */
    Drawable src,                /* Source drawable. */
    Drawable dst,                /* Destination drawable. */
    GC gc,                        /* GC to use. */
    int src_x,                        /* X & Y, width & height */
    int src_y,                        /* define the source rectangle */
    unsigned int width,                /* the will be copied. */
    unsigned int height,
    int dest_x,                        /* Dest X & Y on dest rect. */
    int dest_y)
{
    Rect srcRect, dstRect;
    Rect * srcPtr, * dstPtr;
    const BitMap * srcBit;
    const BitMap * dstBit;
    MacDrawable *srcDraw = (MacDrawable *) src;
    MacDrawable *dstDraw = (MacDrawable *) dst;
    CGrafPtr srcPort, dstPort;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    short tmode;
    RGBColor origForeColor, origBackColor, whiteColor, blackColor;
    Rect clpRect;

    dstPort = TkMacOSXGetDrawablePort(dst);
    srcPort = TkMacOSXGetDrawablePort(src);

    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(dstPort, NULL);
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

    if (tmpRgn2 == NULL) {
	tmpRgn2 = NewRgn();
    }
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
     *  We will change the clip rgn in this routine, so we need to
     *  be able to restore it when we exit.
     */

    GetClip(tmpRgn2);
    if (tkPictureIsOpen) {
	/*
	 * When rendering into a picture, after a call to "OpenCPicture"
	 * the clipping is seriously WRONG and also INCONSISTENT with the
	 * clipping for single plane bitmaps.
	 * To circumvent this problem,  we clip to the whole window
	 * In this case, would have also clipped to the srcRect
	 * ClipRect(&srcRect);
	 */

	GetPortBounds(dstPort,&clpRect);
	dstPtr = &srcRect;
	ClipRect(&clpRect);
    }
    if (!gc->clip_mask) {
    } else if (((TkpClipMask*)gc->clip_mask)->type == TKP_CLIP_REGION) {
	RgnHandle clipRgn = (RgnHandle)
		((TkpClipMask*)gc->clip_mask)->value.region;

	int xOffset = 0, yOffset = 0;
	if (tmpRgn == NULL) {
	    tmpRgn = NewRgn();
	}
	if (!tkPictureIsOpen) {
	    xOffset = dstDraw->xOff + gc->clip_x_origin;
	    yOffset = dstDraw->yOff + gc->clip_y_origin;
	    OffsetRgn(clipRgn, xOffset, yOffset);
	}
	GetClip(tmpRgn);
	SectRgn(tmpRgn, clipRgn, tmpRgn);
	SetClip(tmpRgn);
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
    SetClip(tmpRgn2);
    SetGWorld(saveWorld, saveDevice);
}

/*
 *----------------------------------------------------------------------
 *
 * XCopyPlane --
 *
 *        Copies a bitmap from a source drawable to a destination
 *        drawable.  The plane argument specifies which bit plane of
 *        the source contains the bitmap.  Note that this implementation
 *        ignores the gc->function.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Changes the destination drawable.
 *
 *----------------------------------------------------------------------
 */

void
XCopyPlane(
    Display* display,                /* Display. */
    Drawable src,                /* Source drawable. */
    Drawable dst,                /* Destination drawable. */
    GC gc,                        /* The GC to use. */
    int src_x,                        /* X, Y, width & height */
    int src_y,                        /* define the source rect. */
    unsigned int width,
    unsigned int height,
    int dest_x,                        /* X & Y on dest where we will copy. */
    int dest_y,
    unsigned long plane)        /* Which plane to copy. */
{
    Rect srcRect, dstRect;
    Rect * srcPtr, * dstPtr;
    const BitMap * srcBit;
    const BitMap * dstBit;
    const BitMap * mskBit;
    MacDrawable *srcDraw = (MacDrawable *) src;
    MacDrawable *dstDraw = (MacDrawable *) dst;
    GWorldPtr srcPort, dstPort, mskPort;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    RGBColor macColor;
    TkpClipMask *clipPtr = (TkpClipMask *) gc->clip_mask;
    short tmode;

    srcPort = TkMacOSXGetDrawablePort(src);
    dstPort = TkMacOSXGetDrawablePort(dst);

    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(dstPort, NULL);
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
	 * To circumvent this problem,  we clip to the whole window
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

    if (TkSetMacColor(gc->foreground, &macColor) == true) {
	RGBForeColor(&macColor);
    }

    if (clipPtr == NULL || clipPtr->type == TKP_CLIP_REGION) {
	/*
	 * Case 1: opaque bitmaps.
	 */

	TkSetMacColor(gc->background, &macColor);
	RGBBackColor(&macColor);
	tmode = srcCopy;
	CopyBits(srcBit, dstBit, srcPtr, dstPtr, tmode, NULL);
    } else if (clipPtr->type == TKP_CLIP_PIXMAP) {
	if (clipPtr->value.pixmap == src) {
	    PixMapHandle pm;
	    /*
	     * Case 2: transparent bitmaps.  If it's color we ignore
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

    SetGWorld(saveWorld, saveDevice);
}

/*
 *----------------------------------------------------------------------
 *
 * TkPutImage --
 *
 *        Copies a subimage from an in-memory image to a rectangle of
 *        of the specified drawable.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Draws the image on the specified drawable.
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
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    const BitMap * destBits;
    MacDrawable *dstDraw = (MacDrawable *) d;
    int i, j;
    char *newData = NULL;
    Rect destRect, srcRect, *destPtr, *srcPtr;
    char *dataPtr, *newPtr, *oldPtr;
    int rowBytes = image->bytes_per_line;
    int slices, sliceRowBytes, lastSliceRowBytes, sliceWidth, lastSliceWidth;

    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    destPort = TkMacOSXGetDrawablePort(d);
    SetGWorld(destPort, NULL);
    destBits = GetPortBitMapForCopyBits(destPort);
    TkMacOSXSetUpClippingRgn(d);

    srcPtr = &srcRect;
    SetRect(srcPtr, src_x, src_y, src_x + width, src_y + height);
    if (tkPictureIsOpen) {
	/*
	 * When rendering into a picture, after a call to "OpenCPicture"
	 * the clipping is seriously WRONG and also INCONSISTENT with the
	 * clipping for single plane bitmaps.
	 * To circumvent this problem,	we clip to the whole window
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
	/* Image from XGetImage, copy from containing GWorld directly */
	GWorldPtr srcPort = TkMacOSXGetDrawablePort((Drawable)image->obdata);
	CopyBits(GetPortBitMapForCopyBits(srcPort),
		destBits, srcPtr, destPtr, srcCopy, NULL);
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
		newData = (char *) ckalloc(image->height * (sliceRowBytes+odd));
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
	pixmap.pmVersion = baseAddr32;	      /* 32bit clean */
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

    SetGWorld(saveWorld, saveDevice);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawLines --
 *
 *        Draw connected lines.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Renders a series of connected lines.
 *
 *----------------------------------------------------------------------
 */

void
XDrawLines(
    Display* display,		/* Display. */
    Drawable d,			/* Draw on this. */
    GC gc,			/* Use this GC. */
    XPoint* points,		/* Array of points. */
    int npoints,		/* Number of points. */
    int mode)			/* Line drawing mode. */
{
    MacDrawable *macWin = (MacDrawable *) d;
    CGrafPtr saveWorld;
    GWorldPtr destPort;
    GDHandle saveDevice;
    int i, lw = gc->line_width;

    if (npoints < 2) {
	return; /* TODO: generate BadValue error. */
    }

    destPort = TkMacOSXGetDrawablePort(d);
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(d);

    if (useCGDrawing) {
	CGContextRef outContext;
	float prevx, prevy;
	float o = (lw % 2) ? .5 : 0;

	TkMacOSXSetUpCGContext(macWin, destPort, gc, &outContext);
	CGContextBeginPath(outContext);
	prevx = macWin->xOff + points[0].x + o;
	prevy = macWin->yOff + points[0].y + o;
	CGContextMoveToPoint(outContext, prevx, prevy);
	for (i = 1; i < npoints; i++) {
	    if (mode == CoordModeOrigin) {
		CGContextAddLineToPoint(outContext,
			macWin->xOff + points[i].x + o,
			macWin->yOff + points[i].y + o);
	    } else {
		prevx += points[i].x;
		prevy += points[i].y;
		CGContextAddLineToPoint(outContext, prevx, prevy);
	    }
	}
	CGContextStrokePath(outContext);
	TkMacOSXReleaseCGContext(macWin, destPort, &outContext);
    } else {
	int o = -lw/2;

	TkMacOSXSetUpGraphicsPort(gc, destPort);
	ShowPen();
	PenPixPat(gPenPat);
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
	HidePen();
    }

    SetGWorld(saveWorld, saveDevice);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawSegments --
 *
 *        Draw unconnected lines.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Renders a series of unconnected lines.
 *
 *----------------------------------------------------------------------
 */

void XDrawSegments(
    Display *display,
    Drawable  d,
    GC gc,
    XSegment *segments,
    int  nsegments)
{
    MacDrawable *macWin = (MacDrawable *) d;
    CGrafPtr saveWorld;
    GWorldPtr destPort;
    GDHandle saveDevice;
    int i, lw = gc->line_width;

    destPort = TkMacOSXGetDrawablePort(d);
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(d);

    if (useCGDrawing) {
	CGContextRef outContext;
	float o = (lw % 2) ? .5 : 0;

	TkMacOSXSetUpCGContext(macWin, destPort, gc, &outContext);
	for (i = 0; i < nsegments; i++) {
	    CGContextBeginPath(outContext);
	    CGContextMoveToPoint(outContext,
		    macWin->xOff + segments[i].x1 + o,
		    macWin->yOff + segments[i].y1 + o);
	    CGContextAddLineToPoint(outContext,
		    macWin->xOff + segments[i].x2 + o,
		    macWin->yOff + segments[i].y2 + o);
	    CGContextStrokePath(outContext);
	}
	TkMacOSXReleaseCGContext(macWin, destPort, &outContext);
    } else {
	int o = -lw/2;

	TkMacOSXSetUpGraphicsPort(gc, destPort);
	ShowPen();
	PenPixPat(gPenPat);
	/* This is broken for fat lines, it is not possible to correctly
	 * imitate X11 drawing of oblique fat lines with QD line drawing,
	 * we should draw a filled polygon instead. */
	for (i = 0; i < nsegments; i++) {
	    MoveTo((short) (macWin->xOff + segments[i].x1 + o),
		   (short) (macWin->yOff + segments[i].y1 + o));
	    LineTo((short) (macWin->xOff + segments[i].x2 + o),
		   (short) (macWin->yOff + segments[i].y2 + o));
	}
	HidePen();
    }

    SetGWorld(saveWorld, saveDevice);
}

/*
 *----------------------------------------------------------------------
 *
 * XFillPolygon --
 *
 *        Draws a filled polygon.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Draws a filled polygon on the specified drawable.
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
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    int i;

    destPort = TkMacOSXGetDrawablePort(d);
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(d);

    if (useCGDrawing) {
	CGContextRef outContext;
	float prevx, prevy;
	float o = (gc->line_width % 2) ? .5 : 0;

	TkMacOSXSetUpCGContext(macWin, destPort, gc, &outContext);
	CGContextBeginPath(outContext);
	prevx = macWin->xOff + points[0].x + o;
	prevy = macWin->yOff + points[0].y + o;
	CGContextMoveToPoint(outContext, prevx, prevy);
	for (i = 1; i < npoints; i++) {
	    if (mode == CoordModeOrigin) {
		CGContextAddLineToPoint(outContext,
			macWin->xOff + points[i].x + o,
			macWin->yOff + points[i].y + o);
	    } else {
		prevx += points[i].x;
		prevy += points[i].y;
		CGContextAddLineToPoint(outContext, prevx, prevy);
	    }
	}
	CGContextEOFillPath(outContext);
	TkMacOSXReleaseCGContext(macWin, destPort, &outContext);
    } else {
	PolyHandle polygon;

	TkMacOSXSetUpGraphicsPort(gc, destPort);
	PenNormal();
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
	FillCPoly(polygon, gPenPat);
	KillPoly(polygon);
    }

    SetGWorld(saveWorld, saveDevice);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawRectangle --
 *
 *        Draws a rectangle.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Draws a rectangle on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XDrawRectangle(
    Display* display,                /* Display. */
    Drawable d,                        /* Draw on this. */
    GC gc,                        /* Use this GC. */
    int x,                        /* Upper left corner. */
    int y,
    unsigned int width,                /* Width & height of rect. */
    unsigned int height)
{
    MacDrawable *macWin = (MacDrawable *) d;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    int lw = gc->line_width;

    if (width == 0 || height == 0) {
	return;
    }

    destPort = TkMacOSXGetDrawablePort(d);
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(d);

    if (useCGDrawing) {
	CGContextRef outContext;
	CGRect rect;
	float o = (lw % 2) ? .5 : 0;

	TkMacOSXSetUpCGContext(macWin, destPort, gc, &outContext);
	rect = CGRectMake(
		macWin->xOff + x + o,
		macWin->yOff + y + o,
		width, height);
	CGContextStrokeRect(outContext, rect);
	TkMacOSXReleaseCGContext(macWin, destPort, &outContext);
    } else {
	Rect theRect;
	int o = -lw/2;

	TkMacOSXSetUpGraphicsPort(gc, destPort);
	ShowPen();
	PenPixPat(gPenPat);
	theRect.left =   (short) (macWin->xOff + x + o);
	theRect.top =    (short) (macWin->yOff + y + o);
	theRect.right =  (short) (theRect.left + width  + lw);
	theRect.bottom = (short) (theRect.top  + height + lw);
	FrameRect(&theRect);
	HidePen();
    }

    SetGWorld(saveWorld, saveDevice);
}

#ifdef TK_MACOSXDRAW_UNUSED
/*
 *----------------------------------------------------------------------
 *
 * XDrawRectangles --
 *
 *       Draws the outlines of the specified rectangles as if a
 *       five-point PolyLine protocol request were specified for each
 *       rectangle:
 *
 *             [x,y] [x+width,y] [x+width,y+height] [x,y+height]
 *             [x,y]
 *
 *      For the specified rectangles, these functions do not draw a
 *      pixel more than once.  XDrawRectangles draws the rectangles in
 *      the order listed in the array.  If rectangles intersect, the
 *      intersecting pixels are drawn multiple times.  Draws a
 *      rectangle.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Draws rectangles on the specified drawable.
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
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    XRectangle * rectPtr;
    int i, lw = gc->line_width;

    destPort = TkMacOSXGetDrawablePort(drawable);
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(drawable);

    if (useCGDrawing) {
	CGContextRef outContext;
	CGRect rect;
	float o = (lw % 2) ? .5 : 0;

	TkMacOSXSetUpCGContext(macWin, destPort, gc, &outContext);
	for (i = 0, rectPtr = rectArr; i < nRects; i++, rectPtr++) {
	    if (rectPtr->width == 0 || rectPtr->height == 0) {
		continue;
	    }
	    rect = CGRectMake(
		    macWin->xOff + rectPtr->x + o,
		    macWin->yOff + rectPtr->y + o,
		    rectPtr->width, rectPtr->height);
	    CGContextStrokeRect(outContext, rect);
	}
	TkMacOSXReleaseCGContext(macWin, destPort, &outContext);
    } else {
	Rect theRect;
	int o = -lw/2;

	TkMacOSXSetUpGraphicsPort(gc, destPort);
	ShowPen();
	PenPixPat(gPenPat);
	for (i = 0, rectPtr = rectArr; i < nRects;i++, rectPtr++) {
	    theRect.left =   (short) (macWin->xOff + rectPtr->x + o);
	    theRect.top =    (short) (macWin->yOff + rectPtr->y + o);
	    theRect.right =  (short) (theRect.left + rectPtr->width  + lw);
	    theRect.bottom = (short) (theRect.top  + rectPtr->height + lw);
	    FrameRect(&theRect);
	}
	HidePen();
    }

    SetGWorld(saveWorld, saveDevice);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * XFillRectangles --
 *
 *        Fill multiple rectangular areas in the given drawable.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Draws onto the specified drawable.
 *
 *----------------------------------------------------------------------
 */
void
XFillRectangles(
    Display* display,                /* Display. */
    Drawable d,                        /* Draw on this. */
    GC gc,                        /* Use this GC. */
    XRectangle *rectangles,        /* Rectangle array. */
    int n_rectangles)                /* Number of rectangles. */
{
    MacDrawable *macWin = (MacDrawable *) d;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    XRectangle * rectPtr;
    int i;

    destPort = TkMacOSXGetDrawablePort(d);
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(d);

    if (useCGDrawing) {
	CGContextRef outContext;
	CGRect rect;

	TkMacOSXSetUpCGContext(macWin, destPort, gc, &outContext);
	for (i = 0, rectPtr = rectangles; i < n_rectangles; i++, rectPtr++) {
	    if (rectPtr->width == 0 || rectPtr->height == 0) {
		continue;
	    }
	    rect = CGRectMake(
		    macWin->xOff + rectPtr->x,
		    macWin->yOff + rectPtr->y,
		    rectPtr->width, rectPtr->height);
	    CGContextFillRect(outContext, rect);
	}
	TkMacOSXReleaseCGContext(macWin, destPort, &outContext);
    } else {
	Rect theRect;

	TkMacOSXSetUpGraphicsPort(gc, destPort);
	for (i = 0, rectPtr = rectangles; i < n_rectangles; i++, rectPtr++) {
	    theRect.left =   (short) (macWin->xOff + rectPtr->x);
	    theRect.top =    (short) (macWin->yOff + rectPtr->y);
	    theRect.right =  (short) (theRect.left + rectPtr->width);
	    theRect.bottom = (short) (theRect.top  + rectPtr->height);
	    FillCRect(&theRect, gPenPat);
	}
    }

    SetGWorld(saveWorld, saveDevice);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawArc --
 *
 *        Draw an arc.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Draws an arc on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XDrawArc(
    Display* display,                /* Display. */
    Drawable d,                        /* Draw on this. */
    GC gc,                        /* Use this GC. */
    int x,                        /* Upper left of */
    int y,                        /* bounding rect. */
    unsigned int width,                /* Width & height. */
    unsigned int height,
    int angle1,                        /* Staring angle of arc. */
    int angle2)                        /* Extent of arc. */
{
    MacDrawable *macWin = (MacDrawable *) d;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    int lw = gc->line_width;

    if (width == 0 || height == 0 || angle2 == 0) {
	return;
    }

    destPort = TkMacOSXGetDrawablePort(d);
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(d);

    if (useCGDrawing) {
	CGContextRef outContext;
	CGRect rect;
	float o = (lw % 2) ? .5 : 0;

	TkMacOSXSetUpCGContext(macWin, destPort, gc, &outContext);
	rect = CGRectMake(
		macWin->xOff + x + o,
		macWin->yOff + y + o,
		width, height);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
	if (angle1 == 0 && angle2 == 23040 &&
		CGContextStrokeEllipseInRect != NULL) {
	    CGContextStrokeEllipseInRect(outContext, rect);
	} else
#endif
	{
	    CGMutablePathRef p = CGPathCreateMutable();
	    CGAffineTransform t = CGAffineTransformIdentity;
	    CGPoint c = CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
	    float w = CGRectGetWidth(rect);

	    if (width != height) {
		t = CGAffineTransformMakeScale(1, CGRectGetHeight(rect)/w);
		c = CGPointApplyAffineTransform(c, CGAffineTransformInvert(t));
	    }
	    CGPathAddArc(p, &t, c.x, c.y, w/2, radians(-angle1/64.0),
		    radians(-(angle1 + angle2)/64.0), angle2 > 0);
	    CGContextAddPath(outContext, p);
	    CGPathRelease(p);
	    CGContextStrokePath(outContext);
	}
	TkMacOSXReleaseCGContext(macWin, destPort, &outContext);
    } else {
	Rect theRect;
	short start, extent;
	int o = -lw/2;

	TkMacOSXSetUpGraphicsPort(gc, destPort);
	ShowPen();
	PenPixPat(gPenPat);
	theRect.left =   (short) (macWin->xOff + x + o);
	theRect.top =    (short) (macWin->yOff + y + o);
	theRect.right =  (short) (theRect.left + width + lw);
	theRect.bottom = (short) (theRect.top + height + lw);
	start =  (short) (90 - (angle1/64));
	extent = (short) (-(angle2/64));
	FrameArc(&theRect, start, extent);
	HidePen();
    }

    SetGWorld(saveWorld, saveDevice);
}

#ifdef TK_MACOSXDRAW_UNUSED
/*
 *----------------------------------------------------------------------
 *
 * XDrawArcs --
 *
 *      Draws multiple circular or elliptical arcs.  Each arc is
 *      specified by a rectangle and two angles.  The center of the
 *      circle or ellipse is the center of the rect- angle, and the
 *      major and minor axes are specified by the width and height.
 *      Positive angles indicate counterclock- wise motion, and
 *      negative angles indicate clockwise motion.  If the magnitude
 *      of angle2 is greater than 360 degrees, XDrawArcs truncates it
 *      to 360 degrees.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Draws an arc for each array element on the specified drawable.
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
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    XArc * arcPtr;
    int i, lw = gc->line_width;

    destPort = TkMacOSXGetDrawablePort(d);
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(d);

    if (useCGDrawing) {
	CGContextRef outContext;
	CGRect rect;
	float o = (lw % 2) ? .5 : 0;

	TkMacOSXSetUpCGContext(macWin, destPort, gc, &outContext);
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
	    if (arcPtr->angle1 == 0 && arcPtr->angle2 == 23040 &&
		    CGContextStrokeEllipseInRect != NULL) {
		CGContextStrokeEllipseInRect(outContext, rect);
	    } else
#endif
	    {
		CGMutablePathRef p = CGPathCreateMutable();
		CGAffineTransform t = CGAffineTransformIdentity;
		CGPoint c = CGPointMake(CGRectGetMidX(rect),
					CGRectGetMidY(rect));
		float w = CGRectGetWidth(rect);

		if (arcPtr->width != arcPtr->height) {
		    t = CGAffineTransformMakeScale(1, CGRectGetHeight(rect)/w);
		    c = CGPointApplyAffineTransform(c,
						    CGAffineTransformInvert(t));
		}
		CGPathAddArc(p, &t, c.x, c.y, w/2,
			radians(-arcPtr->angle1/64.0),
			radians(-(arcPtr->angle1 + arcPtr->angle2)/64.0),
			arcPtr->angle2 > 0);
		CGContextAddPath(outContext, p);
		CGPathRelease(p);
		CGContextStrokePath(outContext);
	    }
	}
	TkMacOSXReleaseCGContext(macWin, destPort, &outContext);
    } else {
	Rect theRect;
	short start, extent;
	int o = -lw/2;

	TkMacOSXSetUpGraphicsPort(gc, destPort);
	ShowPen();
	PenPixPat(gPenPat);
	for (i = 0, arcPtr = arcArr;i < nArcs;i++, arcPtr++) {
	    theRect.left =   (short) (macWin->xOff + arcPtr->x + o);
	    theRect.top =    (short) (macWin->yOff + arcPtr->y + o);
	    theRect.right =  (short) (theRect.left + arcPtr->width + lw);
	    theRect.bottom = (short) (theRect.top + arcPtr->height + lw);
	    start =  (short) (90 - (arcPtr->angle1/64));
	    extent = (short) (-(arcPtr->angle2/64));
	    FrameArc(&theRect, start, extent);
	}
	HidePen();
    }

    SetGWorld(saveWorld, saveDevice);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * XFillArc --
 *
 *        Draw a filled arc.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Draws a filled arc on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XFillArc(
    Display* display,                /* Display. */
    Drawable d,                        /* Draw on this. */
    GC gc,                        /* Use this GC. */
    int x,                        /* Upper left of */
    int y,                        /* bounding rect. */
    unsigned int width,                /* Width & height. */
    unsigned int height,
    int angle1,                        /* Staring angle of arc. */
    int angle2)                        /* Extent of arc. */
{
    MacDrawable *macWin = (MacDrawable *) d;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    int lw = gc->line_width;

    if (width == 0 || height == 0 || angle2 == 0) {
	return;
    }

    destPort = TkMacOSXGetDrawablePort(d);
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(d);

    if (useCGDrawing) {
	CGContextRef outContext;
	CGRect rect;
	float o = (lw % 2) ? .5 : 0, u = 0;

	if (notAA(lw)) {
	    o += NON_AA_CG_OFFSET/2;
	    u += NON_AA_CG_OFFSET;
	}
	TkMacOSXSetUpCGContext(macWin, destPort, gc, &outContext);
	rect = CGRectMake(
		macWin->xOff + x + o,
		macWin->yOff + y + o,
		width - u, height - u);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
	if (angle1 == 0 && angle2 == 23040 &&
		CGContextFillEllipseInRect != NULL) {
	    CGContextFillEllipseInRect(outContext, rect);
	} else
#endif
	{
	    CGMutablePathRef p = CGPathCreateMutable();
	    CGAffineTransform t = CGAffineTransformIdentity;
	    CGPoint c = CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
	    float w = CGRectGetWidth(rect);

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
	    CGContextAddPath(outContext, p);
	    CGPathRelease(p);
	    CGContextFillPath(outContext);
	}
	TkMacOSXReleaseCGContext(macWin, destPort, &outContext);
    } else {
	Rect theRect;
	short start, extent;
	int o = -lw/2;
	PolyHandle polygon;
	double sin1, cos1, sin2, cos2, angle;
	double boxWidth, boxHeight;
	double vertex[2], center1[2], center2[2];

	TkMacOSXSetUpGraphicsPort(gc, destPort);
	theRect.left =   (short) (macWin->xOff + x + o);
	theRect.top =    (short) (macWin->yOff + y + o);
	theRect.right =  (short) (theRect.left + width + lw);
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
	    ShowPen();
	    FillCArc(&theRect, start, extent, gPenPat);
	    FillCPoly(polygon, gPenPat);
	    HidePen();
	    KillPoly(polygon);
	} else {
	    ShowPen();
	    FillCArc(&theRect, start, extent, gPenPat);
	    HidePen();
	}
    }

    SetGWorld(saveWorld, saveDevice);
}

#ifdef TK_MACOSXDRAW_UNUSED
/*
 *----------------------------------------------------------------------
 *
 * XFillArcs --
 *
 *      Draw a filled arc.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Draws a filled arc for each array element on the specified drawable.
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
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    XArc * arcPtr;
    int i, lw = gc->line_width;

    destPort = TkMacOSXGetDrawablePort(d);
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(d);

    if (useCGDrawing) {
	CGContextRef outContext;
	CGRect rect;
	float o = (lw % 2) ? .5 : 0, u = 0;

	if (notAA(lw)) {
	    o += NON_AA_CG_OFFSET/2;
	    u += NON_AA_CG_OFFSET;
	}
	TkMacOSXSetUpCGContext(macWin, destPort, gc, &outContext);
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
	    if (arcPtr->angle1 == 0 && arcPtr->angle2 == 23040 &&
		    CGContextFillEllipseInRect != NULL) {
		CGContextFillEllipseInRect(outContext, rect);
	    } else
#endif
	    {
		CGMutablePathRef p = CGPathCreateMutable();
		CGAffineTransform t = CGAffineTransformIdentity;
		CGPoint c = CGPointMake(CGRectGetMidX(rect),
					CGRectGetMidY(rect));
		float w = CGRectGetWidth(rect);

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
		CGContextAddPath(outContext, p);
		CGPathRelease(p);
		CGContextFillPath(outContext);
	    }
	}
	TkMacOSXReleaseCGContext(macWin, destPort, &outContext);
    } else {
	Rect theRect;
	short start, extent;
	int o = -lw/2;
	PolyHandle polygon;
	double sin1, cos1, sin2, cos2, angle;
	double boxWidth, boxHeight;
	double vertex[2], center1[2], center2[2];

	TkMacOSXSetUpGraphicsPort(gc, destPort);
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
		ShowPen();
		FillCArc(&theRect, start, extent, gPenPat);
		FillCPoly(polygon, gPenPat);
		HidePen();
		KillPoly(polygon);
	    } else {
		ShowPen();
		FillCArc(&theRect, start, extent, gPenPat);
		HidePen();
	    }
	}
    }

    SetGWorld(saveWorld, saveDevice);
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
XMaxRequestSize(Display *display)
{
    return (SHRT_MAX / 4);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TkScrollWindow --
 *
 *        Scroll a rectangle of the specified window and accumulate
 *        a damage region.
 *
 * Results:
 *        Returns 0 if the scroll genereated no additional damage.
 *        Otherwise, sets the region that needs to be repainted after
 *        scrolling and returns 1.
 *
 * Side effects:
 *        Scrolls the bits in the window.
 *
 *----------------------------------------------------------------------
 */

int
TkScrollWindow(
    Tk_Window tkwin,                /* The window to be scrolled. */
    GC gc,                        /* GC for window to be scrolled. */
    int x,                        /* Position rectangle to be scrolled. */
    int y,
    int width,
    int height,
    int dx,                        /* Distance rectangle should be moved. */
    int dy,
    TkRegion damageRgn)                /* Region to accumulate damage in. */
{
    MacDrawable *destDraw = (MacDrawable *) Tk_WindowId(tkwin);
    RgnHandle rgn = (RgnHandle) damageRgn;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    Rect srcRect, scrollRect;
    RgnHandle visRgn, clipRgn;

    destPort = TkMacOSXGetDrawablePort(Tk_WindowId(tkwin));
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(Tk_WindowId(tkwin));

    /*
     * Due to the implementation below the behavior may be differnt
     * than X in certain cases that should never occur in Tk.  The
     * scrollRect is the source rect extended by the offset (the union
     * of the source rect and the offset rect).  Everything
     * in the extended scrollRect is scrolled.  On X, it's possible
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
    visRgn = NewRgn();
    clipRgn = NewRgn();
    RectRgn(rgn, &srcRect);
    GetPortVisibleRegion(destPort,visRgn);
    DiffRgn(rgn, visRgn, rgn);
    OffsetRgn(rgn, dx, dy);
    GetPortClipRegion(destPort, clipRgn);
    DiffRgn(clipRgn, rgn, clipRgn);
    SetPortClipRegion(destPort, clipRgn);
    SetEmptyRgn(rgn);

    /*
     * When a menu is up, the Mac does not expect drawing to occur and
     * does not clip out the menu. We have to do it ourselves. This
     * is pretty gross.
     */

    if (tkUseMenuCascadeRgn == 1) {
	    Point scratch = {0, 0};
	    MacDrawable *macDraw = (MacDrawable *) Tk_WindowId(tkwin);

	LocalToGlobal(&scratch);
	CopyRgn(tkMenuCascadeRgn, rgn);
	OffsetRgn(rgn, -scratch.h, -scratch.v);
	DiffRgn(clipRgn, rgn, clipRgn);
	SetPortClipRegion(destPort, clipRgn);
	SetEmptyRgn(rgn);
	macDraw->toplevel->flags |= TK_DRAWN_UNDER_MENU;
    }

    ScrollRect(&scrollRect, dx, dy, rgn);

    SetGWorld(saveWorld, saveDevice);

    DisposeRgn(clipRgn);
    DisposeRgn(visRgn);
    /*
     * Fortunantly, the region returned by ScrollRect is symanticlly
     * the same as what we need to return in this function.  If the
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
 *        Set up the graphics port from the given GC.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        The current port is adjusted.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXSetUpGraphicsPort(
    GC gc,
    GWorldPtr destPort)                /* GC to apply to current port. */
{
    RGBColor macColor;

    if (gPenPat == NULL) {
	gPenPat = NewPixPat();
    }

    if (TkSetMacColor(gc->foreground, &macColor) == true) {
	/* TODO: cache RGBPats for preformace - measure gains...  */
	MakeRGBPat(gPenPat, &macColor);
    }

    PenNormal();
    if(gc->function == GXxor) {
	PenMode(patXor);
    }
    if (gc->line_width > 1) {
	PenSize(gc->line_width, gc->line_width);
    }
    if (gc->line_style != LineSolid) {
	/*
	 * Here the dash pattern should be set in the drawing,
	 * environment, but I don't know how to do that for the Mac.
	 *
	 * p[] is an array of unsigned chars containing the dash list.
	 * A '\0' indicates the end of this list.
	 *
	 * Someone knows how to implement this? If you have a more
	 * complete implementation of SetUpGraphicsPort() for
	 * the Mac (or for Windows), please let me know.
	 *
	 *        Jan Nijtmans
	 *        CMG Arnhem, B.V.
	 *        email: j.nijtmans@chello.nl (private)
	 *               jan.nijtmans@cmg.nl (work)
	 *        url:   http://purl.oclc.org/net/nijtmans/
	 *
	 * FIXME:
	 * This is not possible with QuickDraw line drawing.  As of
	 * Tk 8.4.7 we have a complete set of drawing routines using
	 * CG, so there is no reason to support this here.
	 */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXSetUpCGContext --
 *
 *        Set up a CGContext for the given graphics port.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */

static void
TkMacOSXSetUpCGContext(
    MacDrawable *macWin,
    CGrafPtr destPort,
    GC gc,
    CGContextRef *contextPtr)
{
    RGBColor macColor;
    CGContextRef outContext;
    OSStatus err;
    Rect boundsRect;
    CGAffineTransform coordsTransform;
    static RgnHandle clipRgn = NULL;
    float w;

    err = QDBeginCGContext(destPort, contextPtr);
    outContext = *contextPtr;

    /*
     * Now clip the CG Context to the port.  Note, we have already
     * set up the port with our clip region, so we can just get
     * the clip back out of there.  If we use the macWin->clipRgn
     * directly at this point, we get some odd drawing effects.
     *
     * We also have to intersect our clip region with the port
     * visible region so we don't overwrite the window decoration.
     */

    if (!clipRgn) {
	clipRgn = NewRgn();
    }

    GetPortBounds(destPort, &boundsRect);

    RectRgn(clipRgn, &boundsRect);
    SectRegionWithPortClipRegion(destPort, clipRgn);
    SectRegionWithPortVisibleRegion(destPort, clipRgn);
    ClipCGContextToRegion(outContext, &boundsRect, clipRgn);
    SetEmptyRgn(clipRgn);

    /*
     * Note: You have to call SyncCGContextOriginWithPort
     * AFTER all the clip region manipulations.
     */

    SyncCGContextOriginWithPort(outContext, destPort);

    coordsTransform = CGAffineTransformMake(1, 0, 0, -1, 0,
	    boundsRect.bottom - boundsRect.top);
    CGContextConcatCTM(outContext, coordsTransform);

    /* Now offset the CTM to the subwindow offset */

    if (TkSetMacColor(gc->foreground, &macColor) == true) {
	CGContextSetRGBFillColor(outContext,
		RGBFLOATRED(macColor),
		RGBFLOATGREEN(macColor),
		RGBFLOATBLUE(macColor),
		1);
	CGContextSetRGBStrokeColor(outContext,
		RGBFLOATRED(macColor),
		RGBFLOATGREEN(macColor),
		RGBFLOATBLUE(macColor),
		1);
    }

    if(gc->function == GXxor) {
    }

    w = gc->line_width;
    /* When should we antialias? */
    if (notAA(gc->line_width)) {
	/* Make non-antialiased CG drawing look more like X11 */
	w -= (gc->line_width ? NON_AA_CG_OFFSET : 0);
	CGContextSetShouldAntialias(outContext, 0);
    } else {
	CGContextSetShouldAntialias(outContext, 1);
    }
    CGContextSetLineWidth(outContext, w);

    if (gc->line_style != LineSolid) {
	int num = 0;
	char *p = &(gc->dashes);
	float dashOffset = gc->dash_offset;
	float lengths[10];

	while (p[num] != '\0') {
	    lengths[num] = p[num];
	    num++;
	}
	CGContextSetLineDash(outContext, dashOffset, lengths, num);
    }

    if (gc->cap_style == CapButt) {
	/*
	 *  What about CapNotLast, CapProjecting?
	 */

	CGContextSetLineCap(outContext, kCGLineCapButt);
    } else if (gc->cap_style == CapRound) {
	CGContextSetLineCap(outContext, kCGLineCapRound);
    } else if (gc->cap_style == CapProjecting) {
	CGContextSetLineCap(outContext, kCGLineCapSquare);
    }

    if (gc->join_style == JoinMiter) {
	CGContextSetLineJoin(outContext, kCGLineJoinMiter);
    } else if (gc->join_style == JoinRound) {
	CGContextSetLineJoin(outContext, kCGLineJoinRound);
    } else if (gc->join_style == JoinBevel) {
	CGContextSetLineJoin(outContext, kCGLineJoinBevel);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXReleaseCGContext --
 *
 *        Release the CGContext for the given graphics port.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */

static void
TkMacOSXReleaseCGContext(
	MacDrawable *macWin,
	CGrafPtr destPort,
	CGContextRef *outContext)
{
    CGContextSynchronize(*outContext);
    QDEndCGContext(destPort, outContext);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXSetUpClippingRgn --
 *
 *        Set up the clipping region so that drawing only occurs on the
 *        specified X subwindow.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        The clipping region in the current port is changed.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXSetUpClippingRgn(
    Drawable drawable)                /* Drawable to update. */
{
    MacDrawable *macDraw = (MacDrawable *) drawable;

    if (macDraw->winPtr != NULL) {
	if (macDraw->flags & TK_CLIP_INVALID) {
	    TkMacOSXUpdateClipRgn(macDraw->winPtr);
	}

#if defined(TK_MAC_DEBUG) && defined(TK_MAC_DEBUG_DRAWING)
	TkMacOSXInitNamedDebugSymbol(HIToolbox, int, QDDebugFlashRegion,
				     CGrafPtr port, RgnHandle region);
	if (QDDebugFlashRegion) {
	    CGrafPtr grafPtr = TkMacOSXGetDrawablePort(drawable);
	    /* Carbon-internal region flashing SPI (c.f. Technote 2124) */
	    QDDebugFlashRegion(grafPtr, macDraw->clipRgn);
	}
#endif /* TK_MAC_DEBUG_DRAWING */

	/*
	 * When a menu is up, the Mac does not expect drawing to occur and
	 * does not clip out the menu. We have to do it ourselves. This
	 * is pretty gross.
	 */

	if (macDraw->clipRgn != NULL) {
	    if (tkUseMenuCascadeRgn == 1) {
		    Point scratch = {0, 0};
		    GDHandle saveDevice;
		    GWorldPtr saveWorld;

		    GetGWorld(&saveWorld, &saveDevice);
		    SetGWorld(TkMacOSXGetDrawablePort(drawable), NULL);
		    LocalToGlobal(&scratch);
		    SetGWorld(saveWorld, saveDevice);
		    if (tmpRgn == NULL) {
			tmpRgn = NewRgn();
		    }
		    CopyRgn(tkMenuCascadeRgn, tmpRgn);
		    OffsetRgn(tmpRgn, -scratch.h, -scratch.v);
		    DiffRgn(macDraw->clipRgn, tmpRgn, tmpRgn);
		    SetClip(tmpRgn);
		    macDraw->toplevel->flags |= TK_DRAWN_UNDER_MENU;
	    } else {
		    SetClip(macDraw->clipRgn);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXMakeStippleMap --
 *
 *        Given a drawable and a stipple pattern this function draws the
 *        pattern repeatedly over the drawable.  The drawable can then
 *        be used as a mask for bit-bliting a stipple pattern over an
 *        object.
 *
 * Results:
 *        A BitMap data structure.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */

BitMapPtr
TkMacOSXMakeStippleMap(
    Drawable drawable,                /* Window to apply stipple. */
    Drawable stipple)                /* The stipple pattern. */
{
    GWorldPtr destPort;
    BitMapPtr bitmapPtr;
    Rect      portRect;
    int width, height, stippleHeight, stippleWidth;
    int i, j;
    char * data;
    Rect bounds;

    destPort = TkMacOSXGetDrawablePort(drawable);

    GetPortBounds (destPort, &portRect);
    width = portRect.right - portRect.left;
    height = portRect.bottom - portRect.top;

    bitmapPtr = (BitMap *) ckalloc(sizeof(BitMap));
    data = (char *) ckalloc(height * ((width / 8) + 1));
    bitmapPtr->bounds.top = bitmapPtr->bounds.left = 0;
    bitmapPtr->bounds.right = (short) width;
    bitmapPtr->bounds.bottom = (short) height;
    bitmapPtr->baseAddr = data;
    bitmapPtr->rowBytes = (width / 8) + 1;

    destPort = TkMacOSXGetDrawablePort(stipple);
    stippleWidth = portRect.right - portRect.left;
    stippleHeight = portRect.bottom - portRect.top;

    for (i = 0; i < height; i += stippleHeight) {
	for (j = 0; j < width; j += stippleWidth) {
	    bounds.left = j;
	    bounds.top = i;
	    bounds.right = j + stippleWidth;
	    bounds.bottom = i + stippleHeight;

	    CopyBits(GetPortBitMapForCopyBits(destPort), bitmapPtr,
		&portRect, &bounds, srcCopy, NULL);
	}
    }
    return bitmapPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * InvertByte --
 *
 *      This function reverses the bits in the passed in Byte of data.
 *
 * Results:
 *      The incoming byte in reverse bit order.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static unsigned char
InvertByte(
    unsigned char data)                /* Byte of data. */
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
 *        This procedure draws a rectangular ring around the outside of
 *        a widget to indicate that it has received the input focus.
 *
 *      On the Macintosh, this puts a 1 pixel border in the bgGC color
 *      between the widget and the focus ring, except in the case where
 *      highlightWidth is 1, in which case the border is left out.
 *
 *      For proper Mac L&F, use highlightWidth of 3.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        A rectangle "width" pixels wide is drawn in "drawable",
 *        corresponding to the outer area of "tkwin".
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
 *	This procedure draws the rectangular frame area.  If the user
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
TkpDrawFrame (Tk_Window tkwin, Tk_3DBorder border,
	int highlightWidth, int borderWidth, int relief)
{
    if (useThemedToplevel && Tk_IsTopLevel(tkwin)) {
	/*
	 * Currently only support themed toplevels, until we can better
	 * factor this to handle individual windows (blanket theming of
	 * frames will work for very few UIs).
	 */
	Rect bounds;
	Point origin;
	CGrafPtr saveWorld;
	GDHandle saveDevice;
	XGCValues gcValues;
	GC gc;
	Pixmap pixmap;
	Display *display = Tk_Display(tkwin);

	pixmap = Tk_GetPixmap(display, Tk_WindowId(tkwin),
		Tk_Width(tkwin), Tk_Height(tkwin), Tk_Depth(tkwin));

	gc = Tk_GetGC(tkwin, 0, &gcValues);
	TkMacOSXWinBounds((TkWindow *) tkwin, &bounds);
	origin.v = -bounds.top;
	origin.h = -bounds.left;
	bounds.top = bounds.left = 0;
	bounds.right = Tk_Width(tkwin);
	bounds.bottom = Tk_Height(tkwin);

	GetGWorld(&saveWorld, &saveDevice);
	SetGWorld(TkMacOSXGetDrawablePort(pixmap), 0);
	ApplyThemeBackground(kThemeBackgroundWindowHeader, &bounds,
		kThemeStateActive, 32 /* depth */, true /* inColor */);
	QDSetPatternOrigin(origin);
	EraseRect(&bounds);
	SetGWorld(saveWorld, saveDevice);

	XCopyArea(display, pixmap, Tk_WindowId(tkwin),
		gc, 0, 0, bounds.right, bounds.bottom, 0, 0);
	Tk_FreePixmap(display, pixmap);
	Tk_FreeGC(display, gc);
    } else {
	Tk_Fill3DRectangle(tkwin, Tk_WindowId(tkwin),
		border, highlightWidth, highlightWidth,
		Tk_Width(tkwin) - 2 * highlightWidth,
		Tk_Height(tkwin) - 2 * highlightWidth,
		borderWidth, relief);
    }
}
