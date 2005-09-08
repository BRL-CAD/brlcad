
/*
 * bltWinImage.c --
 *
 *	This module implements image processing procedures for the BLT
 *	toolkit.
 *
 * Copyright 1997-1998 Lucent Technologies, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 */

#include "bltInt.h"
#include "bltImage.h"
#include <X11/Xutil.h>

#define CLAMP(c)	((((c) < 0.0) ? 0.0 : ((c) > 255.0) ? 255.0 : (c)))

#define GetBit(x, y) \
   srcBits[(srcBytesPerRow * (srcHeight - y - 1)) + (x>>3)] & (0x80 >> (x&7))
#define SetBit(x, y) \
   destBits[(destBytesPerRow * (destHeight - y - 1)) + (x>>3)] |= (0x80 >>(x&7))

/*
 *----------------------------------------------------------------------
 *
 * Blt_ColorImageToPixmap --
 *
 *      Converts a color image into a pixmap.
 *
 *	Right now this only handles TrueColor visuals.
 *
 * Results:
 *      The new pixmap is returned.
 *
 *----------------------------------------------------------------------
 */
Pixmap
Blt_ColorImageToPixmap(
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Blt_ColorImage image,
    ColorTable *colorTablePtr)	/* Points to array of colormap indices */
{
    HDC pixmapDC;
    TkWinDCState state;
    Display *display;
    int width, height, depth;
    Pixmap pixmap;
    register int x, y;
    register Pix32 *srcPtr;
    COLORREF rgb;

    *colorTablePtr = NULL;
    width = Blt_ColorImageWidth(image);
    height = Blt_ColorImageHeight(image);
    display = Tk_Display(tkwin);
    depth = Tk_Depth(tkwin);

    pixmap = Tk_GetPixmap(display, Tk_WindowId(tkwin), width, height, depth);
    pixmapDC = TkWinGetDrawableDC(display, pixmap, &state);

    srcPtr = Blt_ColorImageBits(image);
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    rgb = PALETTERGB(srcPtr->Red, srcPtr->Green, srcPtr->Blue);
	    SetPixelV(pixmapDC, x, y, rgb);
	    srcPtr++;
	}
    }
    TkWinReleaseDrawableDC(pixmap, pixmapDC, &state);
    return pixmap;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ColorImageToPixmap2 --
 *
 *      Converts a color image into a pixmap.
 *
 *	Right now this only handles TrueColor visuals.
 *
 * Results:
 *      The new pixmap is returned.
 *
 *----------------------------------------------------------------------
 */
Pixmap
Blt_ColorImageToPixmap2(
    Display *display,
    int depth,
    Blt_ColorImage image,
    ColorTable *colorTablePtr)	/* Points to array of colormap indices */
{
    BITMAP bm;
    HBITMAP hBitmap;
    TkWinBitmap *twdPtr;
    int width, height;
    register Pix32 *srcPtr;
    register int x, y;
    register unsigned char *destPtr;
    unsigned char *bits;

    *colorTablePtr = NULL;
    width = Blt_ColorImageWidth(image);
    height = Blt_ColorImageHeight(image);

    /* 
     * Copy the color image RGB data into the DIB. The DIB scanlines
     * are stored bottom-to-top and the order of the RGB color
     * components is BGR. Who says Win32 GDI programming isn't
     * backwards?  
     */
    bits = Blt_Malloc(width * height * sizeof(unsigned char));
    assert(bits);
    srcPtr = Blt_ColorImageBits(image);    
    for (y = height - 1; y >= 0; y--) {
	destPtr = bits + (y * width);
	for (x = 0; x < width; x++) {
	    *destPtr++ = srcPtr->Blue;
	    *destPtr++ = srcPtr->Green;
	    *destPtr++ = srcPtr->Red;
	    *destPtr++ = (unsigned char)-1;
	    srcPtr++;
	}
    }
    bm.bmType = 0;
    bm.bmWidth = width;
    bm.bmHeight = height;
    bm.bmWidthBytes = width;
    bm.bmPlanes = 1;
    bm.bmBitsPixel = 32;
    bm.bmBits = bits;
    hBitmap = CreateBitmapIndirect(&bm);

    /* Create a windows version of a drawable. */
    twdPtr = Blt_Malloc(sizeof(TkWinBitmap));
    assert(twdPtr);
    twdPtr->type = TWD_BITMAP;
    twdPtr->handle = hBitmap;
    twdPtr->depth = depth;
    twdPtr->colormap = DefaultColormap(display, DefaultScreen(display));
    return (Pixmap)twdPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_DrawableToColorImage --
 *
 *      Takes a snapshot of an X drawable (pixmap or window) and
 *	converts it to a color image.
 *
 * Results:
 *      Returns a color image of the drawable.  If an error occurred,
 *	NULL is returned.
 *
 *----------------------------------------------------------------------
 */
Blt_ColorImage
Blt_DrawableToColorImage(
    Tk_Window tkwin,
    Drawable drawable,
    int x, int y,
    int width, int height,	/* Dimension of the drawable. */
    double inputGamma)
{
    void *data;
    BITMAPINFO info;
    DIBSECTION ds;
    HBITMAP hBitmap, oldBitmap;
    HPALETTE hPalette;
    HDC memDC;
    unsigned char *srcArr;
    register unsigned char *srcPtr;
    HDC hDC;
    TkWinDCState state;
    register Pix32 *destPtr;
    Blt_ColorImage image;
    unsigned char lut[256];

    hDC = TkWinGetDrawableDC(Tk_Display(tkwin), drawable, &state);

    /* Create the intermediate drawing surface at window resolution. */
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    hBitmap = CreateDIBSection(hDC, &info, DIB_RGB_COLORS, &data, NULL, 0);
    memDC = CreateCompatibleDC(hDC);
    oldBitmap = SelectBitmap(memDC, hBitmap);

    hPalette = Blt_GetSystemPalette();
    if (hPalette != NULL) {
	SelectPalette(hDC, hPalette, FALSE);
	RealizePalette(hDC);
	SelectPalette(memDC, hPalette, FALSE);
	RealizePalette(memDC);
    }
    image = NULL;
    /* Copy the window contents to the memory surface. */
    if (!BitBlt(memDC, 0, 0, width, height, hDC, x, y, SRCCOPY)) {
#ifdef notdef
	PurifyPrintf("can't blit: %s\n", Blt_LastError());
#endif
	goto done;
    }
    if (GetObject(hBitmap, sizeof(DIBSECTION), &ds) == 0) {
#ifdef notdef
	PurifyPrintf("can't get object: %s\n", Blt_LastError());
#endif
	goto done;
    }
    srcArr = (unsigned char *)ds.dsBm.bmBits;
    image = Blt_CreateColorImage(width, height);
    destPtr = Blt_ColorImageBits(image);

    {
	register int i;
	double value;

	for (i = 0; i < 256; i++) {
	    value = pow(i / 255.0, inputGamma) * 255.0 + 0.5;
	    lut[i] = (unsigned char)CLAMP(value);
	}
    }

    /* 
     * Copy the DIB RGB data into the color image. The DIB scanlines
     * are stored bottom-to-top and the order of the RGB color
     * components is BGR. Who says Win32 GDI programming isn't
     * backwards?  
     */

    for (y = height - 1; y >= 0; y--) {
	srcPtr = srcArr + (y * ds.dsBm.bmWidthBytes);
	for (x = 0; x < width; x++) {
	    destPtr->Blue = lut[*srcPtr++];
	    destPtr->Green = lut[*srcPtr++];
	    destPtr->Red = lut[*srcPtr++];
	    destPtr->Alpha = (unsigned char)-1;
	    destPtr++;
	    srcPtr++;
	}
    }
  done:
    DeleteBitmap(SelectBitmap(memDC, oldBitmap));
    DeleteDC(memDC);
    TkWinReleaseDrawableDC(drawable, hDC, &state);
    if (hPalette != NULL) {
	DeletePalette(hPalette);
    }
    return image;
}


Pixmap
Blt_PhotoImageMask(
    Tk_Window tkwin,
    Tk_PhotoImageBlock src)
{
    TkWinBitmap *twdPtr;
    int offset, count;
    register int x, y;
    unsigned char *srcPtr;
    int destBytesPerRow;
    int destHeight;
    unsigned char *destBits;

    destBytesPerRow = ((src.width + 31) & ~31) / 8;
    destBits = Blt_Calloc(src.height, destBytesPerRow);
    destHeight = src.height;

    offset = count = 0;
    /* FIXME: figure out why this is so! */
    for (y = src.height - 1; y >= 0; y--) {
	srcPtr = src.pixelPtr + offset;
	for (x = 0; x < src.width; x++) {
	    if (srcPtr[src.offset[3]] == 0x00) {
		SetBit(x, y);
		count++;
	    }
	    srcPtr += src.pixelSize;
	}
	offset += src.pitch;
    }
    if (count > 0) {
	HBITMAP hBitmap;
	BITMAP bm;

	bm.bmType = 0;
	bm.bmWidth = src.width;
	bm.bmHeight = src.height;
	bm.bmWidthBytes = destBytesPerRow;
	bm.bmPlanes = 1;
	bm.bmBitsPixel = 1;
	bm.bmBits = destBits;
	hBitmap = CreateBitmapIndirect(&bm);

	twdPtr = Blt_Malloc(sizeof(TkWinBitmap));
	assert(twdPtr);
	twdPtr->type = TWD_BITMAP;
	twdPtr->handle = hBitmap;
	twdPtr->depth = 1;
	if (Tk_WindowId(tkwin) == None) {
	    twdPtr->colormap = DefaultColormap(Tk_Display(tkwin), 
			 DefaultScreen(Tk_Display(tkwin)));
	} else {
	    twdPtr->colormap = Tk_Colormap(tkwin);
	}
    } else {
	twdPtr = NULL;
    }
    if (destBits != NULL) {
	Blt_Free(destBits);
    }
    return (Pixmap)twdPtr;
}

Pixmap
Blt_ColorImageMask(
    Tk_Window tkwin,
    Blt_ColorImage image)
{
    TkWinBitmap *twdPtr;
    int count;
    register int x, y;
    Pix32 *srcPtr;
    int destBytesPerRow;
    int destWidth, destHeight;
    unsigned char *destBits;

    destWidth = Blt_ColorImageWidth(image);
    destHeight = Blt_ColorImageHeight(image);
    destBytesPerRow = ((destWidth + 31) & ~31) / 8;
    destBits = Blt_Calloc(destHeight, destBytesPerRow);
    count = 0;
    srcPtr = Blt_ColorImageBits(image);
    for (y = 0; y < destHeight; y++) {
	for (x = 0; x < destWidth; x++) {
	    if (srcPtr->Alpha == 0x00) {
		SetBit(x, y);
		count++;
	    }
	    srcPtr++;
	}
    }
    if (count > 0) {
	HBITMAP hBitmap;
	BITMAP bm;

	bm.bmType = 0;
	bm.bmWidth = Blt_ColorImageWidth(image);
	bm.bmHeight = Blt_ColorImageHeight(image);
	bm.bmWidthBytes = destBytesPerRow;
	bm.bmPlanes = 1;
	bm.bmBitsPixel = 1;
	bm.bmBits = destBits;
	hBitmap = CreateBitmapIndirect(&bm);

	twdPtr = Blt_Malloc(sizeof(TkWinBitmap));
	assert(twdPtr);
	twdPtr->type = TWD_BITMAP;
	twdPtr->handle = hBitmap;
	twdPtr->depth = 1;
	if (Tk_WindowId(tkwin) == None) {
	    twdPtr->colormap = DefaultColormap(Tk_Display(tkwin), 
			 DefaultScreen(Tk_Display(tkwin)));
	} else {
	    twdPtr->colormap = Tk_Colormap(tkwin);
	}
    } else {
	twdPtr = NULL;
    }
    if (destBits != NULL) {
	Blt_Free(destBits);
    }
    return (Pixmap)twdPtr;
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_RotateBitmap --
 *
 *	Creates a new bitmap containing the rotated image of the given
 *	bitmap.  We also need a special GC of depth 1, so that we do
 *	not need to rotate more than one plane of the bitmap.
 *
 *	Note that under Windows, monochrome bitmaps are stored
 *	bottom-to-top.  This is why the right angle rotations 0/180
 *	and 90/270 look reversed.
 *
 * Results:
 *	Returns a new bitmap containing the rotated image.
 *
 * -----------------------------------------------------------------
 */
Pixmap
Blt_RotateBitmap(
    Tk_Window tkwin,
    Pixmap srcBitmap,		/* Source bitmap to be rotated */
    int srcWidth, 
    int srcHeight,		/* Width and height of the source bitmap */
    double theta,		/* Right angle rotation to perform */
    int *destWidthPtr, 
    int *destHeightPtr)
{
    Display *display;		/* X display */
    Window root;		/* Root window drawable */
    Pixmap destBitmap;
    double rotWidth, rotHeight;
    HDC hDC;
    TkWinDCState state;
    register int x, y;		/* Destination bitmap coordinates */
    register int sx, sy;	/* Source bitmap coordinates */
    unsigned long pixel;
    HBITMAP hBitmap;
    int result;
    struct MonoBitmap {
	BITMAPINFOHEADER bi;
	RGBQUAD colors[2];
    } mb;
    int srcBytesPerRow, destBytesPerRow;
    int destWidth, destHeight;
    unsigned char *srcBits, *destBits;

    display = Tk_Display(tkwin);
    root = RootWindow(Tk_Display(tkwin), Tk_ScreenNumber(tkwin));
    Blt_GetBoundingBox(srcWidth, srcHeight, theta, &rotWidth, &rotHeight,
	(Point2D *)NULL);

    destWidth = (int)ceil(rotWidth);
    destHeight = (int)ceil(rotHeight);
    destBitmap = Tk_GetPixmap(display, root, destWidth, destHeight, 1);
    if (destBitmap == None) {
	return None;		/* Can't allocate pixmap. */
    }
    srcBits = Blt_GetBitmapData(display, srcBitmap, srcWidth, srcHeight,
	&srcBytesPerRow);
    if (srcBits == NULL) {
	OutputDebugString("Blt_GetBitmapData failed");
	return None;
    }
    destBytesPerRow = ((destWidth + 31) & ~31) / 8;
    destBits = Blt_Calloc(destHeight, destBytesPerRow);

    theta = FMOD(theta, 360.0);
    if (FMOD(theta, (double)90.0) == 0.0) {
	int quadrant;

	/* Handle right-angle rotations specially. */

	quadrant = (int)(theta / 90.0);
	switch (quadrant) {
	case ROTATE_270:	/* 270 degrees */
	    for (y = 0; y < destHeight; y++) {
		sx = y;
		for (x = 0; x < destWidth; x++) {
		    sy = destWidth - x - 1;
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	case ROTATE_180:		/* 180 degrees */
	    for (y = 0; y < destHeight; y++) {
		sy = destHeight - y - 1;
		for (x = 0; x < destWidth; x++) {
		    sx = destWidth - x - 1;
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	case ROTATE_90:		/* 90 degrees */
	    for (y = 0; y < destHeight; y++) {
		sx = destHeight - y - 1;
		for (x = 0; x < destWidth; x++) {
		    sy = x;
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	case ROTATE_0:		/* 0 degrees */
	    for (y = 0; y < destHeight; y++) {
		for (x = 0; x < destWidth; x++) {
		    pixel = GetBit(x, y);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	default:
	    /* The calling routine should never let this happen. */
	    break;
	}
    } else {
	double radians, sinTheta, cosTheta;
	double srcCX, srcCY;	/* Center of source rectangle */
	double destCX, destCY;	/* Center of destination rectangle */
	double tx, ty;
	double rx, ry;		/* Angle of rotation for x and y coordinates */

	radians = (theta / 180.0) * M_PI;
	sinTheta = sin(radians), cosTheta = cos(radians);

	/*
	 * Coordinates of the centers of the source and destination rectangles
	 */
	srcCX = srcWidth * 0.5;
	srcCY = srcHeight * 0.5;
	destCX = destWidth * 0.5;
	destCY = destHeight * 0.5;

	/* Rotate each pixel of dest image, placing results in source image */

	for (y = 0; y < destHeight; y++) {
	    ty = y - destCY;
	    for (x = 0; x < destWidth; x++) {

		/* Translate origin to center of destination image */
		tx = x - destCX;

		/* Rotate the coordinates about the origin */
		rx = (tx * cosTheta) - (ty * sinTheta);
		ry = (tx * sinTheta) + (ty * cosTheta);

		/* Translate back to the center of the source image */
		rx += srcCX;
		ry += srcCY;

		sx = ROUND(rx);
		sy = ROUND(ry);

		/*
		 * Verify the coordinates, since the destination image can be
		 * bigger than the source
		 */

		if ((sx >= srcWidth) || (sx < 0) || (sy >= srcHeight) ||
		    (sy < 0)) {
		    continue;
		}
		pixel = GetBit(sx, sy);
		if (pixel) {
		    SetBit(x, y);
		}
	    }
	}
    }
    hBitmap = ((TkWinDrawable *)destBitmap)->bitmap.handle;
    ZeroMemory(&mb, sizeof(mb));
    mb.bi.biSize = sizeof(BITMAPINFOHEADER);
    mb.bi.biPlanes = 1;
    mb.bi.biBitCount = 1;
    mb.bi.biCompression = BI_RGB;
    mb.bi.biWidth = destWidth;
    mb.bi.biHeight = destHeight;
    mb.bi.biSizeImage = destBytesPerRow * destHeight;
    mb.colors[0].rgbBlue = mb.colors[0].rgbRed = mb.colors[0].rgbGreen = 0x0;
    mb.colors[1].rgbBlue = mb.colors[1].rgbRed = mb.colors[1].rgbGreen = 0xFF;
    hDC = TkWinGetDrawableDC(display, destBitmap, &state);
    result = SetDIBits(hDC, hBitmap, 0, destHeight, (LPVOID)destBits, 
	(BITMAPINFO *)&mb, DIB_RGB_COLORS);
    TkWinReleaseDrawableDC(destBitmap, hDC, &state);
    if (!result) {
#if WINDEBUG
	PurifyPrintf("can't setDIBits: %s\n", Blt_LastError());
#endif
	destBitmap = None;
    }
    if (destBits != NULL) {
         Blt_Free(destBits);
    }
    if (srcBits != NULL) {
         Blt_Free(srcBits);
    }

    *destWidthPtr = destWidth;
    *destHeightPtr = destHeight;
    return destBitmap;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_ScaleBitmap --
 *
 *	Creates a new scaled bitmap from another bitmap. 
 *
 * Results:
 *	The new scaled bitmap is returned.
 *
 * Side Effects:
 *	A new pixmap is allocated. The caller must release this.
 *
 * -----------------------------------------------------------------------
 */
Pixmap
Blt_ScaleBitmap(
    Tk_Window tkwin,
    Pixmap srcBitmap,
    int srcWidth, 
    int srcHeight, 
    int destWidth, 
    int destHeight)
{
    TkWinDCState srcState, destState;
    HDC src, dest;
    Pixmap destBitmap;
    Window root;
    Display *display;

    /* Create a new bitmap the size of the region and clear it */

    display = Tk_Display(tkwin);
    root = RootWindow(Tk_Display(tkwin), Tk_ScreenNumber(tkwin));
    destBitmap = Tk_GetPixmap(display, root, destWidth, destHeight, 1);
    if (destBitmap == None) {
	return None;
    }
    src = TkWinGetDrawableDC(display, srcBitmap, &srcState);
    dest = TkWinGetDrawableDC(display, destBitmap, &destState);

    StretchBlt(dest, 0, 0, destWidth, destHeight, src, 0, 0,
	srcWidth, srcHeight, SRCCOPY);

    TkWinReleaseDrawableDC(srcBitmap, src, &srcState);
    TkWinReleaseDrawableDC(destBitmap, dest, &destState);
    return destBitmap;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_ScaleRotateBitmapRegion --
 *
 *	Creates a scaled and rotated bitmap from a given bitmap.  The
 *	caller also provides (offsets and dimensions) the region of
 *	interest in the destination bitmap.  This saves having to
 *	process the entire destination bitmap is only part of it is
 *	showing in the viewport.
 *
 *	This uses a simple rotation/scaling of each pixel in the 
 *	destination image.  For each pixel, the corresponding 
 *	pixel in the source bitmap is used.  This means that 
 *	destination coordinates are first scaled to the size of 
 *	the rotated source bitmap.  These coordinates are then
 *	rotated back to their original orientation in the source.
 *
 * Results:
 *	The new rotated and scaled bitmap is returned.
 *
 * Side Effects:
 *	A new pixmap is allocated. The caller must release this.
 *
 * -----------------------------------------------------------------------
 */
Pixmap
Blt_ScaleRotateBitmapRegion(
    Tk_Window tkwin,
    Pixmap srcBitmap,		/* Source bitmap. */
    unsigned int srcWidth, 
    unsigned int srcHeight,	/* Size of source bitmap */
    int regionX, 
    int regionY,		/* Offset of region in virtual
				 * destination bitmap. */
    unsigned int regionWidth, 
    unsigned int regionHeight,	/* Desire size of bitmap region. */
    unsigned int virtWidth,		
    unsigned int virtHeight,	/* Virtual size of destination bitmap. */
    double theta)		/* Angle to rotate bitmap.  */
{
    Display *display;		/* X display */
    HBITMAP hBitmap;
    HDC hDC;
    Pixmap destBitmap;
    TkWinDCState state;
    Window root;		/* Root window drawable */
    double rotWidth, rotHeight;
    double xScale, yScale;
    int srcBytesPerRow, destBytesPerRow;
    int destHeight;
    int result;
    register int sx, sy;	/* Source bitmap coordinates */
    register int x, y;		/* Destination bitmap coordinates */
    unsigned char *srcBits, *destBits;
    unsigned long pixel;
    struct MonoBitmap {
	BITMAPINFOHEADER bi;
	RGBQUAD colors[2];
    } mb;

    display = Tk_Display(tkwin);
    root = RootWindow(Tk_Display(tkwin), Tk_ScreenNumber(tkwin));

    /* Create a bitmap and image big enough to contain the rotated text */
    destBitmap = Tk_GetPixmap(display, root, regionWidth, regionHeight, 1);
    if (destBitmap == None) {
	return None;		/* Can't allocate pixmap. */
    }
    srcBits = Blt_GetBitmapData(display, srcBitmap, srcWidth, srcHeight,
	&srcBytesPerRow);
    if (srcBits == NULL) {
	OutputDebugString("Blt_GetBitmapData failed");
	return None;
    }
    destBytesPerRow = ((regionWidth + 31) & ~31) / 8;
    destBits = Blt_Calloc(regionHeight, destBytesPerRow);
    destHeight = regionHeight;

    theta = FMOD(theta, 360.0);
    Blt_GetBoundingBox(srcWidth, srcHeight, theta, &rotWidth, &rotHeight,
	       (Point2D *)NULL);
    xScale = rotWidth / (double)virtWidth;
    yScale = rotHeight / (double)virtHeight;

    if (FMOD(theta, (double)90.0) == 0.0) {
	int quadrant;

	/* Handle right-angle rotations specifically */

	quadrant = (int)(theta / 90.0);
	switch (quadrant) {
	case ROTATE_270:	/* 270 degrees */
	    for (y = 0; y < (int)regionHeight; y++) {
		sx = (int)(yScale * (double)(y+regionY));
		for (x = 0; x < (int)regionWidth; x++) {
		    sy = (int)(xScale *(double)(virtWidth - (x+regionX) - 1));
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	case ROTATE_180:	/* 180 degrees */
	    for (y = 0; y < (int)regionHeight; y++) {
		sy = (int)(yScale * (double)(virtHeight - (y + regionY) - 1));
		for (x = 0; x < (int)regionWidth; x++) {
		    sx = (int)(xScale *(double)(virtWidth - (x+regionX) - 1));
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	case ROTATE_90:		/* 90 degrees */
	    for (y = 0; y < (int)regionHeight; y++) {
		sx = (int)(yScale * (double)(virtHeight - (y + regionY) - 1));
		for (x = 0; x < (int)regionWidth; x++) {
		    sy = (int)(xScale * (double)(x + regionX));
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	case ROTATE_0:		/* 0 degrees */
	    for (y = 0; y < (int)regionHeight; y++) {
		sy = (int)(yScale * (double)(y + regionY));
		for (x = 0; x < (int)regionWidth; x++) {
		    sx = (int)(xScale * (double)(x + regionX));
		    pixel = GetBit(sx, sy);
		    if (pixel) {
			SetBit(x, y);
		    }
		}
	    }
	    break;

	default:
	    /* The calling routine should never let this happen. */
	    break;
	}
    } else {
	double radians, sinTheta, cosTheta;
	double scx, scy; 	/* Offset from the center of the
				 * source rectangle. */
	double rcx, rcy; 	/* Offset to the center of the
				 * rotated rectangle. */
	double tx, ty;		/* Translated coordinates from center */
	double rx, ry;		/* Angle of rotation for x and y coordinates */

	radians = (theta / 180.0) * M_PI;
	sinTheta = sin(radians), cosTheta = cos(radians);

	/*
	 * Coordinates of the centers of the source and destination rectangles
	 */
	scx = srcWidth * 0.5;
	scy = srcHeight * 0.5;
	rcx = rotWidth * 0.5;
	rcy = rotHeight * 0.5;

	/* For each pixel of the destination image, transform back to the
	 * associated pixel in the source image. */

	for (y = 0; y < (int)regionHeight; y++) {
	    ty = (yScale * (double)(y + regionY)) - rcy;
	    for (x = 0; x < (int)regionWidth; x++) {

		/* Translate origin to center of destination image. */
		tx = (xScale * (double)(x + regionX)) - rcx;

		/* Rotate the coordinates about the origin. */
		rx = (tx * cosTheta) - (ty * sinTheta);
		ry = (tx * sinTheta) + (ty * cosTheta);

		/* Translate back to the center of the source image. */
		rx += scx;
		ry += scy;

		sx = ROUND(rx);
		sy = ROUND(ry);

		/*
		 * Verify the coordinates, since the destination image can be
		 * bigger than the source.
		 */

		if ((sx >= (int)srcWidth) || (sx < 0) || 
		    (sy >= (int)srcHeight) || (sy < 0)) {
		    continue;
		}
		pixel = GetBit(sx, sy);
		if (pixel) {
		    SetBit(x, y);
		}
	    }
	}
    }
    /* Write the rotated image into the destination bitmap. */
    hBitmap = ((TkWinDrawable *)destBitmap)->bitmap.handle;
    ZeroMemory(&mb, sizeof(mb));
    mb.bi.biSize = sizeof(BITMAPINFOHEADER);
    mb.bi.biPlanes = 1;
    mb.bi.biBitCount = 1;
    mb.bi.biCompression = BI_RGB;
    mb.bi.biWidth = regionWidth;
    mb.bi.biHeight = regionHeight;
    mb.bi.biSizeImage = destBytesPerRow * regionHeight;
    mb.colors[0].rgbBlue = mb.colors[0].rgbRed = mb.colors[0].rgbGreen = 0x0;
    mb.colors[1].rgbBlue = mb.colors[1].rgbRed = mb.colors[1].rgbGreen = 0xFF;
    hDC = TkWinGetDrawableDC(display, destBitmap, &state);
    result = SetDIBits(hDC, hBitmap, 0, regionHeight, (LPVOID)destBits, 
	(BITMAPINFO *)&mb, DIB_RGB_COLORS);
    TkWinReleaseDrawableDC(destBitmap, hDC, &state);
    if (!result) {
#if WINDEBUG
	PurifyPrintf("can't setDIBits: %s\n", Blt_LastError());
#endif
	destBitmap = None;
    }
    if (destBits != NULL) {
         Blt_Free(destBits);
    }
    if (srcBits != NULL) {
         Blt_Free(srcBits);
    }
    return destBitmap;
}

#ifdef notdef
/*
 *----------------------------------------------------------------------
 *
 * Blt_BlendColorImage --
 *
 *      Takes a snapshot of an X drawable (pixmap or window) and
 *	converts it to a color image.
 *
 * Results:
 *      Returns a color image of the drawable.  If an error occurred,
 *	NULL is returned.
 *
 *----------------------------------------------------------------------
 */
void
Blt_BlendColorImage(
    Tk_Window tkwin,
    Drawable drawable,
    int width, int height,	/* Dimension of the drawable. */
    Region2D *regionPtr)	/* Region to be snapped. */
{
    void *data;
    BITMAPINFO info;
    DIBSECTION ds;
    HBITMAP hBitmap, oldBitmap;
    HPALETTE hPalette;
    HDC memDC;
    unsigned char *srcArr;
    register unsigned char *srcPtr;
    HDC hDC;
    TkWinDCState state;
    register Pix32 *destPtr;
    Blt_ColorImage image;
    register int x, y;

    if (regionPtr == NULL) {
	regionPtr = Blt_SetRegion(0, 0, ColorImageWidth(image), 
		ColorImageHeight(image), &region);
    }
    if (regionPtr->left < 0) {
	regionPtr->left = 0;
    }
    if (regionPtr->right >= destWidth) {
	regionPtr->right = destWidth - 1;
    }
    if (regionPtr->top < 0) {
	regionPtr->top = 0;
    }
    if (regionPtr->bottom >= destHeight) {
	regionPtr->bottom = destHeight - 1;
    }
    width = RegionWidth(regionPtr);
    height = RegionHeight(regionPtr);

    hDC = TkWinGetDrawableDC(display, drawable, &state);

    /* Create the intermediate drawing surface at window resolution. */
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    hBitmap = CreateDIBSection(hDC, &info, DIB_RGB_COLORS, &data, NULL, 0);
    memDC = CreateCompatibleDC(hDC);
    oldBitmap = SelectBitmap(memDC, hBitmap);

    hPalette = Blt_GetSystemPalette();
    if (hPalette != NULL) {
	SelectPalette(hDC, hPalette, FALSE);
	RealizePalette(hDC);
	SelectPalette(memDC, hPalette, FALSE);
	RealizePalette(memDC);
    }
    image = NULL;
    /* Copy the window contents to the memory surface. */
    if (!BitBlt(memDC, 0, 0, width, height, hDC, regionPtr->left, 
	regionPtr->top, SRCCOPY)) {
#ifdef notdef
	PurifyPrintf("can't blit: %s\n", Blt_LastError());
#endif
	goto done;
    }
    if (GetObject(hBitmap, sizeof(DIBSECTION), &ds) == 0) {
#ifdef notdef
	PurifyPrintf("can't get object: %s\n", Blt_LastError());
#endif
	goto done;
    }
    srcArr = (unsigned char *)ds.dsBm.bmBits;
    image = Blt_CreateColorImage(width, height);
    destPtr = Blt_ColorImageBits(image);

    /* 
     * Copy the DIB RGB data into the color image. The DIB scanlines
     * are stored bottom-to-top and the order of the RGBA color
     * components is BGRA. Who says Win32 GDI programming isn't
     * backwards?  
     */
    for (y = height - 1; y >= 0; y--) {
	srcPtr = srcArr + (y * ds.dsBm.bmWidthBytes);
	for (x = 0; x < width; x++) {
	    if (destPtr->Alpha > 0) {
		/* Blend colorimage with background. */
		destPtr->Blue = *srcPtr++;
		destPtr->Green = *srcPtr++;
		destPtr->Red = *srcPtr++;
		destPtr->Alpha = (unsigned char)-1;
		srcPtr++;
	    }
	    destPtr++;
	}
    }
  done:
    DeleteBitmap(SelectBitmap(memDC, oldBitmap));
    DeleteDC(memDC);
    TkWinReleaseDrawableDC(drawable, hDC, &state);
    if (hPalette != NULL) {
	DeletePalette(hPalette);
    }
    return image;
}
#endif

#ifdef HAVE_IJL_H

#include <ijl.h>

Blt_ColorImage
Blt_JPEGToColorImage(interp, fileName)
    Tcl_Interp *interp;
    char *fileName;
{
    JPEG_CORE_PROPERTIES jpgProps;
    Blt_ColorImage image;

    ZeroMemory(&jpgProps, sizeof(JPEG_CORE_PROPERTIES));
    if(ijlInit(&jpgProps) != IJL_OK) {
	Tcl_AppendResult(interp, "can't initialize Intel JPEG library",
			 (char *)NULL);
	return NULL;
    }
    jpgProps.JPGFile = fileName;
    if (ijlRead(&jpgProps, IJL_JFILE_READPARAMS) != IJL_OK) {
	Tcl_AppendResult(interp, "can't read JPEG file header from \"",
			 fileName, "\" file.", (char *)NULL);
	goto error;
    }

    // !dudnik: to fix bug case 584680, [OT:287A305B]
    // Set the JPG color space ... this will always be
    // somewhat of an educated guess at best because JPEG
    // is "color blind" (i.e., nothing in the bit stream
    // tells you what color space the data was encoded from).
    // However, in this example we assume that we are
    // reading JFIF files which means that 3 channel images
    // are in the YCbCr color space and 1 channel images are
    // in the Y color space.
    switch(jpgProps.JPGChannels) {
    case 1:
	jpgProps.JPGColor = IJL_G;
	jpgProps.DIBChannels = 4;
	jpgProps.DIBColor = IJL_RGBA_FPX;
	break;
	
    case 3:
	jpgProps.JPGColor = IJL_YCBCR;
	jpgProps.DIBChannels = 4;
	jpgProps.DIBColor = IJL_RGBA_FPX;
	break;

    case 4:
	jpgProps.JPGColor = IJL_YCBCRA_FPX;
	jpgProps.DIBChannels = 4;
	jpgProps.DIBColor = IJL_RGBA_FPX;
	break;

    default:
	/* This catches everything else, but no color twist will be
           performed by the IJL. */
	jpgProps.DIBColor = (IJL_COLOR)IJL_OTHER;
 	jpgProps.JPGColor = (IJL_COLOR)IJL_OTHER;
	jpgProps.DIBChannels = jpgProps.JPGChannels;
	break;
    }

    jpgProps.DIBWidth    = jpgProps.JPGWidth;
    jpgProps.DIBHeight   = jpgProps.JPGHeight;
    jpgProps.DIBPadBytes = IJL_DIB_PAD_BYTES(jpgProps.DIBWidth, 
					     jpgProps.DIBChannels);

    image = Blt_CreateColorImage(jpgProps.JPGWidth, jpgProps.JPGHeight);

    jpgProps.DIBBytes = (BYTE *)Blt_ColorImageBits(image);
    if (ijlRead(&jpgProps, IJL_JFILE_READWHOLEIMAGE) != IJL_OK) {
	Tcl_AppendResult(interp, "can't read image data from \"", fileName,
		 "\"", (char *)NULL);
	goto error;
    }
    if (ijlFree(&jpgProps) != IJL_OK) {
	fprintf(stderr, "can't free Intel(R) JPEG library\n");
    }
    return image;

 error:
    ijlFree(&jpgProps);
    if (image != NULL) {
	Blt_FreeColorImage(image);
    }
    ijlFree(&jpgProps);
    return NULL;
} 

#else 

#ifdef HAVE_JPEGLIB_H

#undef HAVE_STDLIB_H
#undef EXTERN
#ifdef WIN32
#define XMD_H	1
#endif
#include "jpeglib.h"
#include <setjmp.h>

typedef struct {
    struct jpeg_error_mgr pub;	/* "public" fields */
    jmp_buf jmpBuf;
    Tcl_DString dString;
} ReaderHandler;

static void ErrorProc _ANSI_ARGS_((j_common_ptr jpegInfo));
static void MessageProc _ANSI_ARGS_((j_common_ptr jpegInfo));

/*
 * Here's the routine that will replace the standard error_exit method:
 */

static void
ErrorProc(jpgPtr)
    j_common_ptr jpgPtr;
{
    ReaderHandler *handlerPtr = (ReaderHandler *)jpgPtr->err;

    (*handlerPtr->pub.output_message) (jpgPtr);
    longjmp(handlerPtr->jmpBuf, 1);
}

static void
MessageProc(jpgPtr)
    j_common_ptr jpgPtr;
{
    ReaderHandler *handlerPtr = (ReaderHandler *)jpgPtr->err;
    char buffer[JMSG_LENGTH_MAX];

    /* Create the message and append it into the dynamic string. */
    (*handlerPtr->pub.format_message) (jpgPtr, buffer);
    Tcl_DStringAppend(&(handlerPtr->dString), " ", -1);
    Tcl_DStringAppend(&(handlerPtr->dString), buffer, -1);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_JPEGToColorImage --
 *
 *      Reads a JPEG file and converts it into a color image.
 *
 * Results:
 *      The color image is returned.  If an error occured, such
 *	as the designated file could not be opened, NULL is returned.
 *
 *----------------------------------------------------------------------
 */
Blt_ColorImage
Blt_JPEGToColorImage(interp, fileName)
    Tcl_Interp *interp;
    char *fileName;
{
    struct jpeg_decompress_struct jpg;
    Blt_ColorImage image;
    unsigned int imageWidth, imageHeight;
    register Pix32 *destPtr;
    ReaderHandler handler;
    FILE *f;
    JSAMPLE **readBuffer;
    int row_stride;
    register int i;
    register JSAMPLE *bufPtr;

    f = fopen(fileName, "rb");
    if (f == NULL) {
	Tcl_AppendResult(interp, "can't open \"", fileName, "\":",
	    Tcl_PosixError(interp), (char *)NULL);
	return NULL;
    }
    image = NULL;

    /* Step 1: allocate and initialize JPEG decompression object */

    /* We set up the normal JPEG error routines, then override error_exit. */
    jpg.dct_method = JDCT_IFAST;
    jpg.err = jpeg_std_error(&handler.pub);
    handler.pub.error_exit = ErrorProc;
    handler.pub.output_message = MessageProc;

    Tcl_DStringInit(&handler.dString);
    Tcl_DStringAppend(&handler.dString, "error reading \"", -1);
    Tcl_DStringAppend(&handler.dString, fileName, -1);
    Tcl_DStringAppend(&handler.dString, "\": ", -1);

    if (setjmp(handler.jmpBuf)) {
	jpeg_destroy_decompress(&jpg);
	fclose(f);
	Tcl_DStringResult(interp, &(handler.dString));
	return NULL;
    }
    jpeg_create_decompress(&jpg);
    jpeg_stdio_src(&jpg, f);

    jpeg_read_header(&jpg, TRUE);	/* Step 3: read file parameters */

    jpeg_start_decompress(&jpg);	/* Step 5: Start decompressor */
    imageWidth = jpg.output_width;
    imageHeight = jpg.output_height;
    if ((imageWidth < 1) || (imageHeight < 1)) {
	Tcl_AppendResult(interp, "bad JPEG image size", (char *)NULL);
	fclose(f);
	return NULL;
    }
    /* JSAMPLEs per row in output buffer */
    row_stride = imageWidth * jpg.output_components;

    /* Make a one-row-high sample array that will go away when done
     * with image */
    readBuffer = (*jpg.mem->alloc_sarray) ((j_common_ptr)&jpg, JPOOL_IMAGE, 
	row_stride, 1);
    image = Blt_CreateColorImage(imageWidth, imageHeight);
    destPtr = Blt_ColorImageBits(image);

    if (jpg.output_components == 1) {
	while (jpg.output_scanline < imageHeight) {
	    jpeg_read_scanlines(&jpg, readBuffer, 1);
	    bufPtr = readBuffer[0];
	    for (i = 0; i < (int)imageWidth; i++) {
		destPtr->Red = destPtr->Green = destPtr->Blue = *bufPtr++;
		destPtr->Alpha = (unsigned char)-1;
		destPtr++;
	    }
	}
    } else {
	while (jpg.output_scanline < imageHeight) {
	    jpeg_read_scanlines(&jpg, readBuffer, 1);
	    bufPtr = readBuffer[0];
	    for (i = 0; i < (int)imageWidth; i++) {
		destPtr->Red = *bufPtr++;
		destPtr->Green = *bufPtr++;
		destPtr->Blue = *bufPtr++;
		destPtr->Alpha = (unsigned char)-1;
		destPtr++;
	    }
	}
    }
    jpeg_finish_decompress(&jpg);	/* We can ignore the return value
					 * since suspension is not
					 * possible with the stdio data
					 * source.  */
    jpeg_destroy_decompress(&jpg);


    /*  
     * After finish_decompress, we can close the input file.  Here we
     * postpone it until after no more JPEG errors are possible, so as
     * to simplify the setjmp error logic above.  (Actually, I don't
     * think that jpeg_destroy can do an error exit, but why assume
     * anything...)  
     */
    fclose(f);

    /* 
     * At this point you may want to check to see whether any corrupt-data
     * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
     */
    if (handler.pub.num_warnings > 0) {
	Tcl_SetErrorCode(interp, "IMAGE", "JPEG", 
		 Tcl_DStringValue(&(handler.dString)), (char *)NULL);
    } else {
	Tcl_SetErrorCode(interp, "NONE", (char *)NULL);
    }
    /*
     * We're ready to call the Tk_Photo routines. They'll take the RGB
     * array we've processed to build the Tk image of the JPEG.
     */
    Tcl_DStringFree(&(handler.dString));
    return image;
}

#endif /* HAVE_JPEGLIB_H */
#endif /* HAVE_IJL_H */

