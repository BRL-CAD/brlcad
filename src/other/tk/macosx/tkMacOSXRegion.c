/* 
 * tkMacOSXRegion.c --
 *
 *        Implements X window calls for manipulating regions
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXInt.h"

/*
 * Temporary region that can be reused.
 */
static RgnHandle tmpRgn = NULL;


/*
 *----------------------------------------------------------------------
 *
 * TkCreateRegion --
 *
 *        Implements the equivelent of the X window function
 *        XCreateRegion.  See X window documentation for more details.
 *
 * Results:
 *      Returns an allocated region handle.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */

TkRegion
TkCreateRegion()
{
    RgnHandle rgn;
    rgn = NewRgn();
    return (TkRegion) rgn;
}

/*
 *----------------------------------------------------------------------
 *
 * TkDestroyRegion --
 *
 *        Implements the equivelent of the X window function
 *        XDestroyRegion.  See X window documentation for more details.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Memory is freed.
 *
 *----------------------------------------------------------------------
 */

void 
TkDestroyRegion(
    TkRegion r)
{
    RgnHandle rgn = (RgnHandle) r;
    DisposeRgn(rgn);
}

/*
 *----------------------------------------------------------------------
 *
 * TkIntersectRegion --
 *
 *        Implements the equivilent of the X window function
 *        XIntersectRegion.  See X window documentation for more details.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */

void 
TkIntersectRegion(
    TkRegion sra,
    TkRegion srb,
    TkRegion dr_return)
{
    RgnHandle srcRgnA = (RgnHandle) sra;
    RgnHandle srcRgnB = (RgnHandle) srb;
    RgnHandle destRgn = (RgnHandle) dr_return;
    SectRgn(srcRgnA, srcRgnB, destRgn);
}

/*
 *----------------------------------------------------------------------
 *
 * TkUnionRectWithRegion --
 *
 *        Implements the equivelent of the X window function
 *        XUnionRectWithRegion.  See X window documentation for more
 *        details.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */

void 
TkUnionRectWithRegion(
    XRectangle* rectangle,
    TkRegion src_region,
    TkRegion dest_region_return)
{
    RgnHandle srcRgn = (RgnHandle) src_region;
    RgnHandle destRgn = (RgnHandle) dest_region_return;

    if (tmpRgn == NULL) {
        tmpRgn = NewRgn();
    }
    SetRectRgn(tmpRgn, rectangle->x, rectangle->y,
	    rectangle->x + rectangle->width, rectangle->y + rectangle->height);
    UnionRgn(srcRgn, tmpRgn, destRgn);
}

/*
 *----------------------------------------------------------------------
 *
 * TkRectInRegion --
 *
 *        Implements the equivelent of the X window function
 *        XRectInRegion.  See X window documentation for more details.
 *
 * Results:
 *        Returns one of: RectangleOut, RectangleIn, RectanglePart.
 *
 * Side effects:
 *        None.
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
    RgnHandle rgn = (RgnHandle) region;
    RgnHandle rectRgn, destRgn;
    int result;
    
    rectRgn = NewRgn();
    destRgn = NewRgn();
    SetRectRgn(rectRgn, x,  y, x + width, y + height);
    SectRgn(rgn, rectRgn, destRgn);
    if (EmptyRgn(destRgn)) {
	result = RectangleOut;
    } else if (EqualRgn(rgn, destRgn)) {
	result = RectangleIn;
    } else {
	result = RectanglePart;
    }
    DisposeRgn(rectRgn);
    DisposeRgn(destRgn);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TkClipBox --
 *
 *        Implements the equivelent of the X window function XClipBox.
 *        See X window documentation for more details.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */

void 
TkClipBox(
    TkRegion r,
    XRectangle* rect_return)
{
    RgnHandle rgn = (RgnHandle) r;
    Rect      rect;

    GetRegionBounds(rgn,&rect);

    rect_return->x = rect.left;
    rect_return->y = rect.top;
    rect_return->width = rect.right-rect.left;
    rect_return->height = rect.bottom-rect.top;
}

/*
 *----------------------------------------------------------------------
 *
 * TkSubtractRegion --
 *
 *	Implements the equivilent of the X window function
 *	XSubtractRegion.  See X window documentation for more details.
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
    RgnHandle srcRgnA = (RgnHandle) sra;
    RgnHandle srcRgnB = (RgnHandle) srb;
    RgnHandle destRgn = (RgnHandle) dr_return;

    DiffRgn(srcRgnA, srcRgnB, destRgn);
}

/*
 *----------------------------------------------------------------------
 *
 * TkpBuildRegionFromAlphaData --
 *
 *	Set up a rectangle of the given region based on the supplied
 *	alpha data.
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
	    /* search for first non-transparent pixel */
	    while ((x1 < width) && !*lineDataPtr) {
		x1++;
		lineDataPtr += pixelStride;
	    }
	    end = x1;
	    /* search for first transparent pixel */
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
