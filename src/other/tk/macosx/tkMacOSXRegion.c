/*
 * tkMacOSXRegion.c --
 *
 *	Implements X window calls for manipulating regions
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXPrivate.h"


/*
 *----------------------------------------------------------------------
 *
 * TkCreateRegion --
 *
 *	Implements the equivelent of the X window function XCreateRegion. See
 *	Xwindow documentation for more details.
 *
 * Results:
 *	Returns an allocated region handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkRegion
TkCreateRegion(void)
{
    return (TkRegion) HIShapeCreateMutable();
}

/*
 *----------------------------------------------------------------------
 *
 * TkDestroyRegion --
 *
 *	Implements the equivelent of the X window function XDestroyRegion. See
 *	Xwindow documentation for more details.
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
TkDestroyRegion(
    TkRegion r)
{
    if (r) {
	CFRelease(r);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkIntersectRegion --
 *
 *	Implements the equivalent of the X window function XIntersectRegion.
 *	See Xwindow documentation for more details.
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
TkIntersectRegion(
    TkRegion sra,
    TkRegion srb,
    TkRegion dr_return)
{
    ChkErr(HIShapeIntersect, (HIShapeRef) sra, (HIShapeRef) srb,
	   (HIMutableShapeRef) dr_return);
}

/*
 *----------------------------------------------------------------------
 *
 * TkSubtractRegion --
 *
 *	Implements the equivalent of the X window function XSubtractRegion.
 *	See X window documentation for more details.
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
TkSubtractRegion(
    TkRegion sra,
    TkRegion srb,
    TkRegion dr_return)
{
    ChkErr(HIShapeDifference, (HIShapeRef) sra, (HIShapeRef) srb,
	   (HIMutableShapeRef) dr_return);
}

/*
 *----------------------------------------------------------------------
 *
 * TkUnionRectWithRegion --
 *
 *	Implements the equivelent of the X window function
 *	XUnionRectWithRegion. See Xwindow documentation for more details.
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
TkUnionRectWithRegion(
    XRectangle* rectangle,
    TkRegion src_region,
    TkRegion dest_region_return)
{
    const CGRect r = CGRectMake(rectangle->x, rectangle->y,
	    rectangle->width, rectangle->height);

    if (src_region == dest_region_return) {
	ChkErr(TkMacOSHIShapeUnionWithRect,
		(HIMutableShapeRef) dest_region_return, &r);
    } else {
	HIShapeRef rectRgn = HIShapeCreateWithRect(&r);

	ChkErr(TkMacOSHIShapeUnion, rectRgn, (HIShapeRef) src_region,
		(HIMutableShapeRef) dest_region_return);
	CFRelease(rectRgn);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkRectInRegion --
 *
 *	Implements the equivelent of the X window function XRectInRegion. See
 *	Xwindow documentation for more details.
 *
 * Results:
 *	Returns RectanglePart or RectangleOut. Note that this is not a
 *	complete implementation since it doesn't test for RectangleIn.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkRectInRegion(
    TkRegion region,
    int x,
    int y,
    unsigned int width,
    unsigned int height)
{
    int result;
    const CGRect r = CGRectMake(x, y, width, height);

    result = HIShapeIntersectsRect((HIShapeRef) region, &r) ?
	    RectanglePart : RectangleOut;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TkClipBox --
 *
 *	Implements the equivelent of the X window function XClipBox. See
 *	Xwindow documentation for more details.
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
TkClipBox(
    TkRegion r,
    XRectangle* rect_return)
{
    CGRect rect;

    HIShapeGetBounds((HIShapeRef) r, &rect);
    rect_return->x = rect.origin.x;
    rect_return->y = rect.origin.y;
    rect_return->width = rect.size.width;
    rect_return->height = rect.size.height;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpBuildRegionFromAlphaData --
 *
 *	Set up a rectangle of the given region based on the supplied alpha
 *	data.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The region is updated, with extra pixels added to it.
 *
 *----------------------------------------------------------------------
 */

void
TkpBuildRegionFromAlphaData(
    TkRegion region,			/* Region to update. */
    unsigned int x,			/* Where in region to update. */
    unsigned int y,			/* Where in region to update. */
    unsigned int width,			/* Size of rectangle to update. */
    unsigned int height,		/* Size of rectangle to update. */
    unsigned char *dataPtr,		/* Data to read from. */
    unsigned int pixelStride,		/* num bytes from one piece of alpha
					 * data to the next in the line. */
    unsigned int lineStride)		/* num bytes from one line of alpha
					 * data to the next line. */
{
    unsigned char *lineDataPtr;
    unsigned int x1, y1, end;
    XRectangle rect;

    for (y1 = 0; y1 < height; y1++) {
	lineDataPtr = dataPtr;
	for (x1 = 0; x1 < width; x1 = end) {
	    /*
	     * Search for first non-transparent pixel.
	     */

	    while ((x1 < width) && !*lineDataPtr) {
		x1++;
		lineDataPtr += pixelStride;
	    }
	    end = x1;

	    /*
	     * Search for first transparent pixel.
	     */

	    while ((end < width) && *lineDataPtr) {
		end++;
		lineDataPtr += pixelStride;
	    }
	    if (end > x1) {
		rect.x = x + x1;
		rect.y = y + y1;
		rect.width = end - x1;
		rect.height = 1;
		TkUnionRectWithRegion(&rect, region, region);
	    }
	}
	dataPtr += lineStride;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpRetainRegion --
 *
 *	Increases reference count of region.
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
TkpRetainRegion(
    TkRegion r)
{
    CFRetain(r);
}

/*
 *----------------------------------------------------------------------
 *
 * TkpReleaseRegion --
 *
 *	Decreases reference count of region.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May free memory.
 *
 *----------------------------------------------------------------------
 */

void
TkpReleaseRegion(
    TkRegion r)
{
    CFRelease(r);
}
#if 0

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXEmtpyRegion --
 *
 *	Set region to emtpy.
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
TkMacOSXEmtpyRegion(
    TkRegion r)
{
    ChkErr(HIShapeSetEmpty, (HIMutableShapeRef) r);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXIsEmptyRegion --
 *
 *	Return native region for given tk region.
 *
 * Results:
 *	1 if empty, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkMacOSXIsEmptyRegion(
    TkRegion r)
{
    return HIShapeIsEmpty((HIMutableShapeRef) r) ? 1 : 0;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetNativeRegion --
 *
 *	Return native region for given tk region.
 *
 * Results:
 *	Native region, CFRelease when done.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

HIShapeRef
TkMacOSXGetNativeRegion(
    TkRegion r)
{
    return (HIShapeRef) CFRetain(r);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXSetWithNativeRegion --
 *
 *	Set region to the native region.
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
TkMacOSXSetWithNativeRegion(
    TkRegion r,
    HIShapeRef rgn)
{
    ChkErr(TkMacOSXHIShapeSetWithShape, (HIMutableShapeRef) r, rgn);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXOffsetRegion --
 *
 *	Offsets region by given distances.
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
TkMacOSXOffsetRegion(
    TkRegion r,
    short dx,
    short dy)
{
    ChkErr(HIShapeOffset, (HIMutableShapeRef) r, dx, dy);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXHIShapeCreateEmpty, TkMacOSXHIShapeCreateMutableWithRect,
 * TkMacOSXHIShapeSetWithShape, TkMacOSXHIShapeSetWithRect,
 * TkMacOSHIShapeDifferenceWithRect, TkMacOSHIShapeUnionWithRect,
 * TkMacOSHIShapeUnion --
 *
 *	Wrapper functions for missing/buggy HIShape API
 *
 *----------------------------------------------------------------------
 */

HIShapeRef
TkMacOSXHIShapeCreateEmpty(void)
{
    HIShapeRef result;

    result = HIShapeCreateEmpty();
    return result;
}

HIMutableShapeRef
TkMacOSXHIShapeCreateMutableWithRect(
    const CGRect *inRect)
{
    HIMutableShapeRef result;

    result = HIShapeCreateMutableWithRect(inRect);
    return result;
}

OSStatus
TkMacOSXHIShapeSetWithShape(
    HIMutableShapeRef inDestShape,
    HIShapeRef inSrcShape)
{
    OSStatus result;

    result = HIShapeSetWithShape(inDestShape, inSrcShape);
    return result;
}

#if 0
OSStatus
TkMacOSXHIShapeSetWithRect(
    HIMutableShapeRef inShape,
    const CGRect *inRect)
{
    OSStatus result;
    HIShapeRef rgn = HIShapeCreateWithRect(inRect);

    result = TkMacOSXHIShapeSetWithShape(inShape, rgn);
    CFRelease(rgn);

    return result;
}
#endif

OSStatus
TkMacOSHIShapeDifferenceWithRect(
    HIMutableShapeRef inShape,
    const CGRect *inRect)
{
    OSStatus result;
    HIShapeRef rgn = HIShapeCreateWithRect(inRect);

    result = HIShapeDifference(inShape, rgn, inShape);
    CFRelease(rgn);

    return result;
}

OSStatus
TkMacOSHIShapeUnionWithRect(
    HIMutableShapeRef inShape,
    const CGRect *inRect)
{
    OSStatus result;

    result = HIShapeUnionWithRect(inShape, inRect);
    return result;
}

OSStatus
TkMacOSHIShapeUnion(
    HIShapeRef inShape1,
    HIShapeRef inShape2,
    HIMutableShapeRef outResult)
{
    OSStatus result;

    result = HIShapeUnion(inShape1, inShape2, outResult);
    return result;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
