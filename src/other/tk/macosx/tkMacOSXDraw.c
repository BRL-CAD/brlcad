/*
 * tkMacOSXDraw.c --
 *
 *	This file contains functions that perform drawing to
 *	Xlib windows. Most of the functions simple emulate
 *	Xlib functions.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXDebug.h"
#include "xbytes.h"

/*
#ifdef TK_MAC_DEBUG
#define TK_MAC_DEBUG_DRAWING
#define TK_MAC_DEBUG_IMAGE_DRAWING
#endif
*/

#define radians(d) ((d) * (M_PI/180.0))

/*
 * Non-antialiased CG drawing looks better and more like X11 drawing when using
 * very fine lines, so decrease all linewidths by the following constant.
 */
#define NON_AA_CG_OFFSET .999

static int cgAntiAliasLimit = 0;
#define notAA(w) ((w) < cgAntiAliasLimit)

static int useThemedToplevel = 0;
static int useThemedFrame = 0;

/*
 * Prototypes for functions used only in this file.
 */

static void ClipToGC(Drawable d, GC gc, HIShapeRef *clipRgnPtr);
static CGImageRef CreateCGImageWithXImage(XImage *ximage);
static CGContextRef GetCGContextForDrawable(Drawable d);
static void DrawCGImage(Drawable d, GC gc, CGContextRef context, CGImageRef image,
	unsigned long imageForeground, unsigned long imageBackground,
	CGRect imageBounds, CGRect srcBounds, CGRect dstBounds);


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

	if (Tcl_LinkVar(interp, "::tk::mac::CGAntialiasLimit",
		(char *) &cgAntiAliasLimit, TCL_LINK_INT) != TCL_OK) {
	    Tcl_ResetResult(interp);
	}
	cgAntiAliasLimit = limit;

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
#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS
	if (Tcl_LinkVar(interp, "::tk::mac::useCompatibilityMetrics",
		(char *) &tkMacOSXUseCompatibilityMetrics, TCL_LINK_BOOLEAN)
		!= TCL_OK) {
	    Tcl_ResetResult(interp);
	}
#endif
    }
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
    unsigned int width,		/* that will be copied. */
    unsigned int height,
    int dest_x,			/* Dest X & Y on dest rect. */
    int dest_y)
{
    TkMacOSXDrawingContext dc;
    MacDrawable *srcDraw = (MacDrawable *) src;

    display->request++;
    if (!width || !height) {
	/* TkMacOSXDbgMsg("Drawing of emtpy area requested"); */
	return;
    }
    if (srcDraw->flags & TK_IS_PIXMAP) {
	if (!TkMacOSXSetupDrawingContext(dst, gc, 1, &dc)) {
	    return;
	}
	if (dc.context) {
	    CGImageRef img = TkMacOSXCreateCGImageWithDrawable(src);

	    if (img) {
		DrawCGImage(dst, gc, dc.context, img, gc->foreground,
			gc->background, CGRectMake(0, 0,
			srcDraw->size.width, srcDraw->size.height),
			CGRectMake(src_x, src_y, width, height),
			CGRectMake(dest_x, dest_y, width, height));
		CFRelease(img);
	    } else {
		TkMacOSXDbgMsg("Invalid source drawable");
	    }
	} else {
	    TkMacOSXDbgMsg("Invalid destination drawable");
	}
	TkMacOSXRestoreDrawingContext(&dc);
    } else if (TkMacOSXDrawableWindow(src)) {
	NSView *view = TkMacOSXDrawableView(srcDraw);
	NSWindow *w = [view window];
	NSInteger gs = [w windowNumber] > 0 ? [w gState] : 0;
	/* // alternative using per-view gState:
	NSInteger gs = [view gState];
	if (!gs) {
	    [view allocateGState];
	    if ([view lockFocusIfCanDraw]) {
		[view unlockFocus];
	    }
	    gs = [view gState];
	}
	*/
	if (!gs || !TkMacOSXSetupDrawingContext(dst, gc, 1, &dc)) {
	    return;
	}
	if (dc.context) {
	    NSGraphicsContext *gc = nil;
	    CGFloat boundsH = [view bounds].size.height;
	    NSRect srcRect = NSMakeRect(srcDraw->xOff + src_x, boundsH -
		    height - (srcDraw->yOff + src_y), width, height);

	    if (((MacDrawable *) dst)->flags & TK_IS_PIXMAP) {
		gc = [NSGraphicsContext graphicsContextWithGraphicsPort:
			dc.context flipped:NO];
		if (gc) {
		    [NSGraphicsContext saveGraphicsState];
		    [NSGraphicsContext setCurrentContext:gc];
		}
	    }
	    NSCopyBits(gs, srcRect, NSMakePoint(dest_x,
		    dc.portBounds.size.height - dest_y));
	    if (gc) {
		[NSGraphicsContext restoreGraphicsState];
	    }
	} else {
	    TkMacOSXDbgMsg("Invalid destination drawable");
	}
	TkMacOSXRestoreDrawingContext(&dc);
    } else {
	TkMacOSXDbgMsg("Invalid source drawable");
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
    GC gc,			/* GC to use. */
    int src_x,			/* X & Y, width & height */
    int src_y,			/* define the source rectangle */
    unsigned int width,		/* that will be copied. */
    unsigned int height,
    int dest_x,			/* Dest X & Y on dest rect. */
    int dest_y,
    unsigned long plane)	/* Which plane to copy. */
{
    TkMacOSXDrawingContext dc;
    MacDrawable *srcDraw = (MacDrawable *) src;

    display->request++;
    if (!width || !height) {
	/* TkMacOSXDbgMsg("Drawing of emtpy area requested"); */
	return;
    }
    if (plane != 1) {
	Tcl_Panic("Unexpected plane specified for XCopyPlane");
    }
    if (srcDraw->flags & TK_IS_PIXMAP) {
	if (!TkMacOSXSetupDrawingContext(dst, gc, 1, &dc)) {
	    return;
	}
	if (dc.context) {
	    CGImageRef img = TkMacOSXCreateCGImageWithDrawable(src);

	    if (img) {
		TkpClipMask *clipPtr = (TkpClipMask *) gc->clip_mask;
		unsigned long imageBackground  = gc->background;

		if (clipPtr && clipPtr->type == TKP_CLIP_PIXMAP &&
			clipPtr->value.pixmap == src) {
		    imageBackground = TRANSPARENT_PIXEL << 24;
		}
		DrawCGImage(dst, gc, dc.context, img, gc->foreground,
			imageBackground, CGRectMake(0, 0,
			srcDraw->size.width, srcDraw->size.height),
			CGRectMake(src_x, src_y, width, height),
			CGRectMake(dest_x, dest_y, width, height));
		CFRelease(img);
	    } else {
		TkMacOSXDbgMsg("Invalid source drawable");
	    }
	} else {
	    TkMacOSXDbgMsg("Invalid destination drawable");
	}
	TkMacOSXRestoreDrawingContext(&dc);
    } else {
	XCopyArea(display, src, dst, gc, src_x, src_y, width, height, dest_x,
		dest_y);
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

int
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
    TkMacOSXDrawingContext dc;

    display->request++;
    if (!TkMacOSXSetupDrawingContext(d, gc, 1, &dc)) {
	return BadDrawable;
    }
    if (dc.context) {
	CGImageRef img = CreateCGImageWithXImage(image);

	if (img) {
	    DrawCGImage(d, gc, dc.context, img, gc->foreground, gc->background,
		    CGRectMake(0, 0, image->width, image->height),
		    CGRectMake(src_x, src_y, width, height),
		    CGRectMake(dest_x, dest_y, width, height));
	    CFRelease(img);
	} else {
	    TkMacOSXDbgMsg("Invalid source drawable");
	}
    } else {
	TkMacOSXDbgMsg("Invalid destination drawable");
    }
    TkMacOSXRestoreDrawingContext(&dc);
}

/*
 *----------------------------------------------------------------------
 *
 * CreateCGImageWithXImage --
 *
 *	Create CGImage from XImage, copying the image data.
 *
 * Results:
 *	CGImage, release after use.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void ReleaseData(void *info, const void *data, size_t size) {
    ckfree(info);
}

CGImageRef
CreateCGImageWithXImage(
    XImage *image)
{
    CGImageRef img = NULL;
    size_t bitsPerComponent, bitsPerPixel;
    size_t len = image->bytes_per_line * image->height;
    const CGFloat *decode = NULL;
    CGBitmapInfo bitmapInfo;
    CGDataProviderRef provider = NULL;
    char *data = NULL;
    CGDataProviderReleaseDataCallback releaseData = ReleaseData;

    if (image->obdata) {
	/*
	 * Image from XGetImage
	 */

	img = TkMacOSXCreateCGImageWithDrawable((Pixmap) image->obdata);
    } else if (image->bits_per_pixel == 1) {
	/*
	 * BW image
	 */

	static const CGFloat decodeWB[2] = {1, 0};

	bitsPerComponent = 1;
	bitsPerPixel = 1;
	decode = decodeWB;
	if (image->bitmap_bit_order != MSBFirst) {
	    char *srcPtr = image->data + image->xoffset;
	    char *endPtr = srcPtr + len;
	    char *destPtr = (data = ckalloc(len));

	    while (srcPtr < endPtr) {
		*destPtr++ = xBitReverseTable[(unsigned char)(*(srcPtr++))];
	    }
	} else {
	    data = memcpy(ckalloc(len), image->data + image->xoffset,
		    len);
	}
	if (data) {
	    provider = CGDataProviderCreateWithData(data, data, len, releaseData);
	}
	if (provider) {
	    img = CGImageMaskCreate(image->width, image->height, bitsPerComponent,
		    bitsPerPixel, image->bytes_per_line,
		    provider, decode, 0);
	}
    } else if (image->format == ZPixmap && image->bits_per_pixel == 32) {
	/*
	 * Color image
	 */

	CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);

	bitsPerComponent = 8;
	bitsPerPixel = 32;
	bitmapInfo = (image->byte_order == MSBFirst ?
		kCGBitmapByteOrder32Big : kCGBitmapByteOrder32Little) |
		kCGImageAlphaNoneSkipFirst;
	data = memcpy(ckalloc(len), image->data + image->xoffset, len);
	if (data) {
	    provider = CGDataProviderCreateWithData(data, data, len, releaseData);
	}
	if (provider) {
	    img = CGImageCreate(image->width, image->height, bitsPerComponent,
		    bitsPerPixel, image->bytes_per_line, colorspace, bitmapInfo,
		    provider, decode, 0, kCGRenderingIntentDefault);
	}
	if (colorspace) {
	    CFRelease(colorspace);
	}
    } else {
	TkMacOSXDbgMsg("Unsupported image type");
    }
    if (provider) {
	CFRelease(provider);
    }

    return img;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXCreateCGImageWithDrawable --
 *
 *	Create a CGImage from the given Drawable.
 *
 * Results:
 *	CGImage, release after use.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

CGImageRef
TkMacOSXCreateCGImageWithDrawable(
    Drawable drawable)
{
    CGImageRef img = NULL;
    CGContextRef context = GetCGContextForDrawable(drawable);

    if (context) {
	img = CGBitmapContextCreateImage(context);
    }
    return img;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateNSImageWithPixmap --
 *
 *	Create NSImage for Pixmap.
 *
 * Results:
 *	NSImage.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static NSImage*
CreateNSImageWithPixmap(
    Pixmap pixmap,
    int width,
    int height)
{
    CGImageRef cgImage;
    NSImage *nsImage;
    NSBitmapImageRep *bitmapImageRep;

    cgImage = TkMacOSXCreateCGImageWithDrawable(pixmap);
    nsImage = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
    bitmapImageRep = [[NSBitmapImageRep alloc] initWithCGImage:cgImage];
    [nsImage addRepresentation:bitmapImageRep];
    [bitmapImageRep release];
    CFRelease(cgImage);

    return nsImage;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetNSImageWithTkImage --
 *
 *	Get autoreleased NSImage for Tk_Image.
 *
 * Results:
 *	NSImage.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

NSImage*
TkMacOSXGetNSImageWithTkImage(
    Display *display,
    Tk_Image image,
    int width,
    int height)
{
    Pixmap pixmap = Tk_GetPixmap(display, None, width, height, 0);
    NSImage *nsImage;

    Tk_RedrawImage(image, 0, 0, width, height, pixmap, 0, 0);
    nsImage = CreateNSImageWithPixmap(pixmap, width, height);
    Tk_FreePixmap(display, pixmap);

    return [nsImage autorelease];
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetNSImageWithBitmap --
 *
 *	Get autoreleased NSImage for Bitmap.
 *
 * Results:
 *	NSImage.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

NSImage*
TkMacOSXGetNSImageWithBitmap(
    Display *display,
    Pixmap bitmap,
    GC gc,
    int width,
    int height)
{
    Pixmap pixmap = Tk_GetPixmap(display, None, width, height, 0);
    NSImage *nsImage;

    unsigned long origBackground = gc->background;

    gc->background = TRANSPARENT_PIXEL << 24;
    XSetClipOrigin(display, gc, 0, 0);
    XCopyPlane(display, bitmap, pixmap, gc, 0, 0, width, height, 0, 0, 1);
    gc->background = origBackground;
    nsImage = CreateNSImageWithPixmap(pixmap, width, height);
    Tk_FreePixmap(display, pixmap);

    return [nsImage autorelease];
}

/*
 *----------------------------------------------------------------------
 *
 * GetCGContextForDrawable --
 *
 *	Get CGContext for given Drawable, creating one if necessary.
 *
 * Results:
 *	CGContext.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

CGContextRef
GetCGContextForDrawable(
    Drawable d)
{
    MacDrawable *macDraw = (MacDrawable *) d;

    if (macDraw && (macDraw->flags & TK_IS_PIXMAP) && !macDraw->context) {
	const size_t bitsPerComponent = 8;
	size_t bitsPerPixel, bytesPerRow, len;
	CGColorSpaceRef colorspace = NULL;
	CGBitmapInfo bitmapInfo =
#ifdef __LITTLE_ENDIAN__
		kCGBitmapByteOrder32Host;
#else
		kCGBitmapByteOrderDefault;
#endif
	char *data;
	CGRect bounds = CGRectMake(0, 0, macDraw->size.width,
		macDraw->size.height);

	if (macDraw->flags & TK_IS_BW_PIXMAP) {
	    bitsPerPixel = 8;
	    bitmapInfo = kCGImageAlphaOnly;
	} else {
	    colorspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
	    bitsPerPixel = 32;
	    bitmapInfo |= kCGImageAlphaPremultipliedFirst;
	}
	bytesPerRow = ((size_t) macDraw->size.width * bitsPerPixel + 127) >> 3
		& ~15;
	len = macDraw->size.height * bytesPerRow;
	data = ckalloc(len);
	bzero(data, len);
	macDraw->context = CGBitmapContextCreate(data, macDraw->size.width,
		macDraw->size.height, bitsPerComponent, bytesPerRow,
		colorspace, bitmapInfo);
	if (macDraw->context) {
	    CGContextClearRect(macDraw->context, bounds);
	}
	if (colorspace) {
	    CFRelease(colorspace);
	}
    }

    return (macDraw ? macDraw->context : NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * DrawCGImage --
 *
 *	Draw CG image into drawable.
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
DrawCGImage(
    Drawable d,
    GC gc,
    CGContextRef context,
    CGImageRef image,
    unsigned long imageForeground,
    unsigned long imageBackground,
    CGRect imageBounds,
    CGRect srcBounds,
    CGRect dstBounds)
{
    MacDrawable *macDraw = (MacDrawable *) d;

    if (macDraw && context && image) {
	CGImageRef subImage = NULL;

	if (!CGRectEqualToRect(imageBounds, srcBounds)) {
	    if (!CGRectContainsRect(imageBounds, srcBounds)) {
		TkMacOSXDbgMsg("Mismatch of sub CGImage bounds");
	    }
	    subImage = CGImageCreateWithImageInRect(image, CGRectOffset(
		    srcBounds, -imageBounds.origin.x, -imageBounds.origin.y));
	    if (subImage) {
		image = subImage;
	    }
	}
	dstBounds = CGRectOffset(dstBounds, macDraw->xOff, macDraw->yOff);
	if (CGImageIsMask(image)) {
	    /*CGContextSaveGState(context);*/
	    if (macDraw->flags & TK_IS_BW_PIXMAP) {
		if (imageBackground != TRANSPARENT_PIXEL << 24) {
		    CGContextClearRect(context, dstBounds);
		}
		CGContextSetRGBFillColor(context, 0.0, 0.0, 0.0, 1.0);
	    } else {
		if (imageBackground != TRANSPARENT_PIXEL << 24) {
		    TkMacOSXSetColorInContext(gc, imageBackground, context);
		    CGContextFillRect(context, dstBounds);
		}
		TkMacOSXSetColorInContext(gc, imageForeground, context);
	    }
	}
#ifdef TK_MAC_DEBUG_IMAGE_DRAWING
	CGContextSaveGState(context);
	CGContextSetLineWidth(context, 1.0);
	CGContextSetRGBStrokeColor(context, 0, 0, 0, 0.1);
	CGContextSetRGBFillColor(context, 0, 1, 0, 0.1);
	CGContextFillRect(context, dstBounds);
	CGContextStrokeRect(context, dstBounds);
	CGPoint p[4] = {dstBounds.origin,
	    CGPointMake(CGRectGetMaxX(dstBounds), CGRectGetMaxY(dstBounds)),
	    CGPointMake(CGRectGetMinX(dstBounds), CGRectGetMaxY(dstBounds)),
	    CGPointMake(CGRectGetMaxX(dstBounds), CGRectGetMinY(dstBounds))
	};
	CGContextStrokeLineSegments(context, p, 4);
	CGContextRestoreGState(context);
	TkMacOSXDbgMsg("Drawing CGImage at (x=%f, y=%f), (w=%f, h=%f)",
		dstBounds.origin.x, dstBounds.origin.y,
		dstBounds.size.width, dstBounds.size.height);
#else /* TK_MAC_DEBUG_IMAGE_DRAWING */
	CGContextSaveGState(context);
	CGContextTranslateCTM(context, 0, dstBounds.origin.y + CGRectGetMaxY(dstBounds));
	CGContextScaleCTM(context, 1, -1);
	CGContextDrawImage(context, dstBounds, image);
	CGContextRestoreGState(context);
#endif /* TK_MAC_DEBUG_IMAGE_DRAWING */
	/*if (CGImageIsMask(image)) {
	    CGContextRestoreGState(context);
	}*/
	if (subImage) {
	    CFRelease(subImage);
	}
    } else {
	TkMacOSXDbgMsg("Drawing of empty CGImage requested");
    }
    return Success;
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

int
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
	return BadValue;
    }

    display->request++;
    if (!TkMacOSXSetupDrawingContext(d, gc, 1, &dc)) {
	return BadDrawable;
    }
    if (dc.context) {
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
    }
    TkMacOSXRestoreDrawingContext(&dc);
    return Success;
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
    if (!TkMacOSXSetupDrawingContext(d, gc, 1, &dc)) {
	return;
    }
    if (dc.context) {
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
    if (!TkMacOSXSetupDrawingContext(d, gc, 1, &dc)) {
	return;
    }
    if (dc.context) {
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
    if (!TkMacOSXSetupDrawingContext(d, gc, 1, &dc)) {
	return;
    }
    if (dc.context) {
	CGRect rect;
	double o = (lw % 2) ? .5 : 0;

	rect = CGRectMake(
		macWin->xOff + x + o,
		macWin->yOff + y + o,
		width, height);
	CGContextStrokeRect(dc.context, rect);
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
    if (!TkMacOSXSetupDrawingContext(d, gc, 1, &dc)) {
	return;
    }
    if (dc.context) {
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

int
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
    if (!TkMacOSXSetupDrawingContext(d, gc, 1, &dc)) {
	return BadDrawable;
    }
    if (dc.context) {
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
    }
    TkMacOSXRestoreDrawingContext(&dc);
    return Success;
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
    if (!TkMacOSXSetupDrawingContext(d, gc, 1, &dc)) {
	return;
    }
    if (dc.context) {
	CGRect rect;
	double o = (lw % 2) ? .5 : 0;

	rect = CGRectMake(
		macWin->xOff + x + o,
		macWin->yOff + y + o,
		width, height);
	if (angle1 == 0 && angle2 == 23040) {
	    CGContextStrokeEllipseInRect(dc.context, rect);
	} else {
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
    if (!TkMacOSXSetupDrawingContext(d, gc, 1, &dc)) {
	return;
    }
    if (dc.context) {
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

	    if (arcPtr->angle1 == 0 && arcPtr->angle2 == 23040) {
		CGContextStrokeEllipseInRect(dc.context, rect);
	    } else {
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
    if (!TkMacOSXSetupDrawingContext(d, gc, 1, &dc)) {
	return;
    }
    if (dc.context) {
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

	if (angle1 == 0 && angle2 == 23040) {
	    CGContextFillEllipseInRect(dc.context, rect);
	} else {
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
    if (!TkMacOSXSetupDrawingContext(d, gc, 1, &dc)) {
	return;
    }
    if (dc.context) {
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
	    if (arcPtr->angle1 == 0 && arcPtr->angle2 == 23040) {
		CGContextFillEllipseInRect(dc.context, rect);
	    } else {
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
    MacDrawable *macDraw = (MacDrawable *) Tk_WindowId(tkwin);
    NSView *view = TkMacOSXDrawableView(macDraw);
    CGRect visRect, srcRect, dstRect;
    CGFloat boundsH;
    HIShapeRef dmgRgn, dstRgn;
    int result;

    if (view && !CGRectIsEmpty(visRect = NSRectToCGRect([view visibleRect]))) {
	boundsH = [view bounds].size.height;
	srcRect = CGRectMake(macDraw->xOff + x, boundsH - height -
		(macDraw->yOff + y), width, height);
	dstRect = CGRectIntersection(CGRectOffset(srcRect, dx, -dy), visRect);
	srcRect = CGRectIntersection(srcRect, visRect);
	if (!CGRectIsEmpty(srcRect) && !CGRectIsEmpty(dstRect)) {
	    /*
	    CGRect sRect = CGRectIntersection(CGRectOffset(dstRect, -dx, dy),
		    srcRect);
	    NSCopyBits(0, NSRectFromCGRect(sRect),
		    NSPointFromCGPoint(CGRectOffset(sRect, dx, -dy).origin));
	    */
	    [view scrollRect:NSRectFromCGRect(srcRect) by:NSMakeSize(dx, -dy)];
	}
	srcRect.origin.y = boundsH - srcRect.size.height - srcRect.origin.y;
	dstRect.origin.y = boundsH - dstRect.size.height - dstRect.origin.y;
	srcRect = CGRectUnion(srcRect, dstRect);
	dmgRgn = HIShapeCreateMutableWithRect(&srcRect);
	dstRgn = HIShapeCreateWithRect(&dstRect);
	ChkErr(HIShapeDifference, dmgRgn, dstRgn, (HIMutableShapeRef) dmgRgn);
	CFRelease(dstRgn);
	TkMacOSXInvalidateViewRegion(view, dmgRgn);
    } else {
	dmgRgn = HIShapeCreateEmpty();
    }
    TkMacOSXSetWithNativeRegion(damageRgn, dmgRgn);
    result = HIShapeIsEmpty(dmgRgn) ? 0 : 1;
    CFRelease(dmgRgn);

    return result;
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
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXSetUpGraphicsPort(
    GC gc,			/* GC to apply to current port. */
    void *destPort)
{
    Tcl_Panic("TkMacOSXSetUpGraphicsPort: Obsolete, no more QD!");
}


/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXSetUpDrawingContext --
 *
 *	Set up a drawing context for the given drawable and GC.
 *
 * Results:
 *	Boolean indicating whether it is ok to draw; if false, drawing
 *	context was not setup, so do not attempt to draw and do not call
 *	TkMacOSXRestoreDrawingContext().
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkMacOSXSetupDrawingContext(
    Drawable d,
    GC gc,
    int useCG, /* advisory only ! */
    TkMacOSXDrawingContext *dcPtr)
{
    MacDrawable *macDraw = ((MacDrawable*)d);
    int dontDraw = 0, isWin = 0;
    TkMacOSXDrawingContext dc = {};
    CGRect clipBounds;

    dc.clipRgn = TkMacOSXGetClipRgn(d);
    if (!dontDraw) {
	ClipToGC(d, gc, &dc.clipRgn);
	dontDraw = dc.clipRgn ? HIShapeIsEmpty(dc.clipRgn) : 0;
    }
    if (dontDraw) {
	goto end;
    }
    if (useCG) {
	dc.context = GetCGContextForDrawable(d);
    }
    if (!dc.context || !(macDraw->flags & TK_IS_PIXMAP)) {
	isWin = (TkMacOSXDrawableWindow(d) != nil);
    }
    if (dc.context) {
	dc.portBounds = clipBounds = CGContextGetClipBoundingBox(dc.context);
    } else if (isWin) {
	NSView *view = TkMacOSXDrawableView(macDraw);
	if (view) {
	    if (view != [NSView focusView]) {
		dc.focusLocked = [view lockFocusIfCanDraw];
		dontDraw = !dc.focusLocked;
	    } else {
		dontDraw = ![view canDraw];
	    }
	    if (dontDraw) {
		goto end;
	    }
	    [[view window] disableFlushWindow];
	    dc.view = view;
	    dc.context = [[NSGraphicsContext currentContext] graphicsPort];
	    dc.portBounds = NSRectToCGRect([view bounds]);
	    if (dc.clipRgn) {
		clipBounds = CGContextGetClipBoundingBox(dc.context);
	    }
	} else {
	    Tcl_Panic("TkMacOSXSetupDrawingContext(): "
		    "no NSView to draw into !");
	}
    } else {
	Tcl_Panic("TkMacOSXSetupDrawingContext(): "
		"no context to draw into !");
    }
    if (dc.context) {
	CGAffineTransform t = { .a = 1, .b = 0, .c = 0, .d = -1, .tx = 0,
		.ty = dc.portBounds.size.height};
	dc.portBounds.origin.x += macDraw->xOff;
	dc.portBounds.origin.y += macDraw->yOff;
	if (!dc.focusLocked) {
	    CGContextSaveGState(dc.context);
	}
	CGContextSetTextDrawingMode(dc.context, kCGTextFill);
	CGContextConcatCTM(dc.context, t);
	if (dc.clipRgn) {
#ifdef TK_MAC_DEBUG_DRAWING
	    CGContextSaveGState(dc.context);
	    ChkErr(HIShapeReplacePathInCGContext, dc.clipRgn, dc.context);
	    CGContextSetRGBFillColor(dc.context, 1.0, 0.0, 0.0, 0.1);
	    CGContextEOFillPath(dc.context);
	    CGContextRestoreGState(dc.context);
#endif /* TK_MAC_DEBUG_DRAWING */
	    CGRect r;
	    if (!HIShapeIsRectangular(dc.clipRgn) || !CGRectContainsRect(
		    *HIShapeGetBounds(dc.clipRgn, &r),
		    CGRectApplyAffineTransform(clipBounds, t))) {
		ChkErr(HIShapeReplacePathInCGContext, dc.clipRgn, dc.context);
		CGContextEOClip(dc.context);
	    }
	}
	if (gc) {
	    static const CGLineCap cgCap[] = {
		[CapNotLast] = kCGLineCapButt,
		[CapButt] = kCGLineCapButt,
		[CapRound] = kCGLineCapRound,
		[CapProjecting] = kCGLineCapSquare,
	    };
	    static const CGLineJoin cgJoin[] = {
		[JoinMiter] = kCGLineJoinMiter,
		[JoinRound] = kCGLineJoinRound,
		[JoinBevel] = kCGLineJoinBevel,
	    };
	    bool shouldAntialias;
	    double w = gc->line_width;

	    TkMacOSXSetColorInContext(gc, gc->foreground, dc.context);
	    if (isWin) {
		CGContextSetPatternPhase(dc.context, CGSizeMake(
			dc.portBounds.size.width, dc.portBounds.size.height));
	    }
	    if(gc->function != GXcopy) {
		TkMacOSXDbgMsg("Logical functions other than GXcopy are "
			"not supported for CG drawing!");
	    }
	    /* When should we antialias? */
	    shouldAntialias = !notAA(gc->line_width);
	    if (!shouldAntialias) {
		/* Make non-antialiased CG drawing look more like X11 */
		w -= (gc->line_width ? NON_AA_CG_OFFSET : 0);
	    }
	    CGContextSetShouldAntialias(dc.context, shouldAntialias);
	    CGContextSetLineWidth(dc.context, w);
	    if (gc->line_style != LineSolid) {
		int num = 0;
		char *p = &(gc->dashes);
		CGFloat dashOffset = gc->dash_offset;
		CGFloat lengths[10];

		while (p[num] != '\0' && num < 10) {
		    lengths[num] = p[num];
		    num++;
		}
		CGContextSetLineDash(dc.context, dashOffset, lengths, num);
	    }
	    if ((unsigned)gc->cap_style < sizeof(cgCap)/sizeof(CGLineCap)) {
		CGContextSetLineCap(dc.context,
			cgCap[(unsigned)gc->cap_style]);
	    }
	    if ((unsigned)gc->join_style < sizeof(cgJoin)/sizeof(CGLineJoin)) {
		CGContextSetLineJoin(dc.context,
			cgJoin[(unsigned)gc->join_style]);
	    }
	}
    }
end:
    if (dontDraw && dc.clipRgn) {
	CFRelease(dc.clipRgn);
	dc.clipRgn = NULL;
    }
    *dcPtr = dc;
    return !dontDraw;
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
TkMacOSXRestoreDrawingContext(
    TkMacOSXDrawingContext *dcPtr)
{
    if (dcPtr->context) {
	CGContextSynchronize(dcPtr->context);
	[[dcPtr->view window] enableFlushWindow];
	if (dcPtr->focusLocked) {
	    [dcPtr->view unlockFocus];
	} else {
	    CGContextRestoreGState(dcPtr->context);
	}
    }
    if (dcPtr->clipRgn) {
	CFRelease(dcPtr->clipRgn);
    }
#ifdef TK_MAC_DEBUG
    bzero(dcPtr, sizeof(TkMacOSXDrawingContext));
#endif /* TK_MAC_DEBUG */
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetClipRgn --
 *
 *	Get the clipping region needed to restrict drawing to the given
 *	drawable.
 *
 * Results:
 *	Clipping region. If non-NULL, CFRelease it when done.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

HIShapeRef
TkMacOSXGetClipRgn(
    Drawable drawable)		/* Drawable. */
{
    MacDrawable *macDraw = (MacDrawable *) drawable;
    HIShapeRef clipRgn = NULL;

    if (macDraw->winPtr && macDraw->flags & TK_CLIP_INVALID) {
	TkMacOSXUpdateClipRgn(macDraw->winPtr);
#ifdef TK_MAC_DEBUG_DRAWING
	TkMacOSXDbgMsg("%s", macDraw->winPtr->pathName);
	NSView *view = TkMacOSXDrawableView(macDraw);
	if ([view lockFocusIfCanDraw]) {
	    CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
	    CGContextSaveGState(context);
	    CGContextConcatCTM(context, CGAffineTransformMake(1.0, 0.0, 0.0,
		    -1.0, 0.0, [view bounds].size.height));
	    ChkErr(HIShapeReplacePathInCGContext, macDraw->visRgn, context);
	    CGContextSetRGBFillColor(context, 0.0, 1.0, 0.0, 0.1);
	    CGContextEOFillPath(context);
	    CGContextRestoreGState(context);
	    [view unlockFocus];
	}
#endif /* TK_MAC_DEBUG_DRAWING */
    }

    if (macDraw->drawRgn) {
	clipRgn = HIShapeCreateCopy(macDraw->drawRgn);
    } else if (macDraw->visRgn) {
	clipRgn = HIShapeCreateCopy(macDraw->visRgn);
    }

    return clipRgn;
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
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXSetUpClippingRgn(
    Drawable drawable)		/* Drawable to update. */
{
}

/*
 *----------------------------------------------------------------------
 *
 * TkpClipDrawableToRect --
 *
 *	Clip all drawing into the drawable d to the given rectangle.
 *	If width or height are negative, reset to no clipping.
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
    NSView *view = TkMacOSXDrawableView(macDraw);

    if (macDraw->drawRgn) {
	CFRelease(macDraw->drawRgn);
	macDraw->drawRgn = NULL;
    }
    if (width >= 0 && height >= 0) {
	CGRect drawRect = CGRectMake(x + macDraw->xOff, y + macDraw->yOff,
		width, height);
	HIShapeRef drawRgn = HIShapeCreateWithRect(&drawRect);

	if (macDraw->winPtr && macDraw->flags & TK_CLIP_INVALID) {
	    TkMacOSXUpdateClipRgn(macDraw->winPtr);
	}
	if (macDraw->visRgn) {
	    macDraw->drawRgn = HIShapeCreateIntersection(macDraw->visRgn,
		    drawRgn);
	    CFRelease(drawRgn);
	} else {
	    macDraw->drawRgn = drawRgn;
	}
	if (view && view != [NSView focusView] && [view lockFocusIfCanDraw]) {
	    drawRect.origin.y = [view bounds].size.height -
		    (drawRect.origin.y + drawRect.size.height);
	    NSRectClip(NSRectFromCGRect(drawRect));
	    macDraw->flags |= TK_FOCUSED_VIEW;
	}
    } else {
	if (view && (macDraw->flags & TK_FOCUSED_VIEW)) {
	    [view unlockFocus];
	    macDraw->flags &= ~TK_FOCUSED_VIEW;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ClipToGC --
 *
 *	Helper function to intersect given region with gc clip region.
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
ClipToGC(
    Drawable d,
    GC gc,
    HIShapeRef *clipRgnPtr) /* must point to initialized variable */
{
    if (gc && gc->clip_mask &&
	    ((TkpClipMask*)gc->clip_mask)->type == TKP_CLIP_REGION) {
	TkRegion gcClip = ((TkpClipMask*)gc->clip_mask)->value.region;
	int xOffset = ((MacDrawable *) d)->xOff + gc->clip_x_origin;
	int yOffset = ((MacDrawable *) d)->yOff + gc->clip_y_origin;
	HIShapeRef clipRgn = *clipRgnPtr, gcClipRgn;

	TkMacOSXOffsetRegion(gcClip, xOffset, yOffset);
	gcClipRgn = TkMacOSXGetNativeRegion(gcClip);
	if (clipRgn) {
	    *clipRgnPtr = HIShapeCreateIntersection(gcClipRgn, clipRgn);
	    CFRelease(clipRgn);
	} else {
	    *clipRgnPtr = HIShapeCreateCopy(gcClipRgn);
	}
	CFRelease(gcClipRgn);
	TkMacOSXOffsetRegion(gcClip, -xOffset, -yOffset);
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

void *
TkMacOSXMakeStippleMap(
    Drawable drawable,		/* Window to apply stipple. */
    Drawable stipple)		/* The stipple pattern. */
{
    return NULL;
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

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
