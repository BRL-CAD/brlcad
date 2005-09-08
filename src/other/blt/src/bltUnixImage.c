
/*
 * bltUnixImage.c --
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
#include "bltHash.h"
#include <X11/Xutil.h>
#include <X11/Xproto.h>

#define CLAMP(c)	((((c) < 0.0) ? 0.0 : ((c) > 255.0) ? 255.0 : (c)))

int redAdjust, greenAdjust, blueAdjust;
int redMaskShift, greenMaskShift, blueMaskShift;


/*
 *----------------------------------------------------------------------
 *
 * ShiftCount --
 *
 *	Returns the position of the least significant (low) bit in
 *	the given mask.
 *
 *	For TrueColor and DirectColor visuals, a pixel value is
 *	formed by OR-ing the red, green, and blue colormap indices
 *	into a single 32-bit word.  The visual's color masks tell
 *	you where in the word the indices are supposed to be.  The
 *	masks contain bits only where the index is found.  By counting
 *	the leading zeros in the mask, we know how many bits to shift
 *	to the individual red, green, and blue values to form a pixel.
 *
 * Results:
 *      The number of the least significant bit.
 *
 *----------------------------------------------------------------------
 */
static int
ShiftCount(mask)
    register unsigned int mask;
{
    register int count;

    for (count = 0; count < 32; count++) {
	if (mask & 0x01) {
	    break;
	}
	mask >>= 1;
    }
    return count;
}

/*
 *----------------------------------------------------------------------
 *
 * CountBits --
 *
 *	Returns the number of bits set in the given mask.
 *
 *	Reference: Graphics Gems Volume 2.
 *	
 * Results:
 *      The number of bits to set in the mask.
 *
 *
 *----------------------------------------------------------------------
 */
static int
CountBits(mask)
    register unsigned long mask; /* 32  1-bit tallies */
{
    /* 16  2-bit tallies */
    mask = (mask & 0x55555555) + ((mask >> 1) & (0x55555555));  
    /* 8  4-bit tallies */
    mask = (mask & 0x33333333) + ((mask >> 2) & (0x33333333)); 
    /* 4  8-bit tallies */
    mask = (mask & 0x07070707) + ((mask >> 4) & (0x07070707));  
    /* 2 16-bit tallies */
    mask = (mask & 0x000F000F) + ((mask >> 8) & (0x000F000F));  
    /* 1 32-bit tally */
    mask = (mask & 0x0000001F) + ((mask >> 16) & (0x0000001F));  
    return mask;
}

static void
ComputeMasks(visualPtr)
    Visual *visualPtr;
{
    int count;

    redMaskShift = ShiftCount((unsigned int)visualPtr->red_mask);
    greenMaskShift = ShiftCount((unsigned int)visualPtr->green_mask);
    blueMaskShift = ShiftCount((unsigned int)visualPtr->blue_mask);

    redAdjust = greenAdjust = blueAdjust = 0;
    count = CountBits((unsigned long)visualPtr->red_mask);
    if (count < 8) {
	redAdjust = 8 - count;
    }
    count = CountBits((unsigned long)visualPtr->green_mask);
    if (count < 8) {
	greenAdjust = 8 - count;
    }
    count = CountBits((unsigned long)visualPtr->blue_mask);
    if (count < 8) {
	blueAdjust = 8 - count;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TrueColorPixel --
 *
 *      Computes a pixel index from the 3 component RGB values.
 *
 * Results:
 *      The pixel index is returned.
 *
 *----------------------------------------------------------------------
 */
static INLINE unsigned int
TrueColorPixel(visualPtr, pixelPtr)
    Visual *visualPtr;
    Pix32 *pixelPtr;
{
    unsigned int red, green, blue;

    /*
     * The number of bits per color may be less than eight. For example,
     * 15/16 bit displays (hi-color) use only 5 bits, 8-bit displays
     * use 2 or 3 bits (don't ask me why you'd have an 8-bit TrueColor
     * display). So shift off the least significant bits.
     */
    red = ((unsigned int)pixelPtr->Red >> redAdjust);
    green = ((unsigned int)pixelPtr->Green >> greenAdjust);
    blue = ((unsigned int)pixelPtr->Blue >> blueAdjust);

    /* Shift each color into the proper location of the pixel index. */
    red = (red << redMaskShift) & visualPtr->red_mask;
    green = (green << greenMaskShift) & visualPtr->green_mask;
    blue = (blue << blueMaskShift) & visualPtr->blue_mask;
    return (red | green | blue);
}

/*
 *----------------------------------------------------------------------
 *
 * DirectColorPixel --
 *
 *      Translates the 3 component RGB values into a pixel index.
 *      This differs from TrueColor only in that it first translates
 *	the RGB values through a color table.
 *
 * Results:
 *      The pixel index is returned.
 *
 *----------------------------------------------------------------------
 */
static INLINE unsigned int
DirectColorPixel(colorTabPtr, pixelPtr)
    struct ColorTableStruct *colorTabPtr;
    Pix32 *pixelPtr;
{
    unsigned int red, green, blue;

    red = colorTabPtr->red[pixelPtr->Red];
    green = colorTabPtr->green[pixelPtr->Green];
    blue = colorTabPtr->blue[pixelPtr->Blue];
    return (red | green | blue);
}

/*
 *----------------------------------------------------------------------
 *
 * PseudoColorPixel --
 *
 *      Translates the 3 component RGB values into a pixel index.
 *      This differs from TrueColor only in that it first translates
 *	the RGB values through a color table.
 *
 * Results:
 *      The pixel index is returned.
 *
 *----------------------------------------------------------------------
 */
static INLINE unsigned int
PseudoColorPixel(pixelPtr, lut)
    Pix32 *pixelPtr;
    unsigned int *lut;
{
    int red, green, blue;
    int pixel;

    red = (pixelPtr->Red >> 3) + 1;
    green = (pixelPtr->Green >> 3) + 1;
    blue = (pixelPtr->Blue >> 3) + 1;
    pixel = RGBIndex(red, green, blue);
    return lut[pixel];
}

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
Blt_ColorImageToPixmap(interp, tkwin, image, colorTablePtr)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    Blt_ColorImage image;
    ColorTable *colorTablePtr;	/* Points to array of colormap indices */
{
    Display *display;
    int width, height;
    Pixmap pixmap;
    GC pixmapGC;
    Visual *visualPtr;
    XImage *imagePtr;
    int nPixels;

    visualPtr = Tk_Visual(tkwin);
    width = Blt_ColorImageWidth(image);
    height = Blt_ColorImageHeight(image);
    display = Tk_Display(tkwin);

    ComputeMasks(visualPtr);

    *colorTablePtr = NULL;
    imagePtr = XCreateImage(Tk_Display(tkwin), visualPtr, Tk_Depth(tkwin),
	ZPixmap, 0, (char *)NULL, width, height, 32, 0);
    assert(imagePtr);

    nPixels = width * height;
    imagePtr->data = Blt_Malloc(sizeof(Pix32) * nPixels);
    assert(imagePtr->data);

    imagePtr->byte_order = MSBFirst;	/* Force the byte order */
    imagePtr->bitmap_bit_order = imagePtr->byte_order;
    imagePtr->bytes_per_line = width * sizeof(Pix32);

    switch (visualPtr->class) {
    case TrueColor:
	{
	    register int x, y;
	    register Pix32 *srcPtr;
	    register char *destPtr;
	    unsigned int pixel;
	    int rowOffset;

	    /*
	     * Compute the colormap locations directly from pixel RGB values.
	     */
	    srcPtr = Blt_ColorImageBits(image);
	    rowOffset = 0;
	    for (y = 0; y < height; y++) {
		destPtr = imagePtr->data + rowOffset;
		for (x = 0; x < width; x++, srcPtr++) {
		    pixel = TrueColorPixel(visualPtr, srcPtr);
		    switch (imagePtr->bits_per_pixel) {
		    case 32:
			*destPtr++ = (pixel >> 24) & 0xFF;
			/*FALLTHRU*/
		    case 24:
			*destPtr++ = (pixel >> 16) & 0xFF;
			/*FALLTHRU*/
		    case 16:
			*destPtr++ = (pixel >> 8) & 0xFF;
			/*FALLTHRU*/
		    case 8:
			*destPtr++ = pixel & 0xFF;
			/*FALLTHRU*/
		    }
		}
		rowOffset += imagePtr->bytes_per_line;
	    }
	}
	break;

    case DirectColor:
	{
	    register int x, y;
	    register Pix32 *srcPtr;
	    register char *destPtr;
	    unsigned int pixel;
	    int rowOffset;
	    struct ColorTableStruct *colorTabPtr;

	    /* Build a color table first */
	    colorTabPtr = Blt_DirectColorTable(interp, tkwin, image);

	    /*
	     * Compute the colormap locations directly from pixel RGB values.
	     */
	    srcPtr = Blt_ColorImageBits(image);
	    rowOffset = 0;
	    for (y = 0; y < height; y++) {
		destPtr = imagePtr->data + rowOffset;
		for (x = 0; x < width; x++, srcPtr++) {
		    pixel = DirectColorPixel(colorTabPtr, srcPtr);
		    switch (imagePtr->bits_per_pixel) {
		    case 32:
			*destPtr++ = (pixel >> 24) & 0xFF;
			/*FALLTHRU*/
		    case 24:
			*destPtr++ = (pixel >> 16) & 0xFF;
			/*FALLTHRU*/
		    case 16:
			*destPtr++ = (pixel >> 8) & 0xFF;
			/*FALLTHRU*/
		    case 8:
			*destPtr++ = pixel & 0xFF;
			/*FALLTHRU*/
		    }
		}
		rowOffset += imagePtr->bytes_per_line;
	    }
	    *colorTablePtr = colorTabPtr;
	}
	break;

    case GrayScale:
    case StaticGray:
    case PseudoColor:
    case StaticColor:
	{
	    register int x, y;
	    register Pix32 *srcPtr;
	    register char *destPtr;
	    unsigned int pixel;
	    int rowOffset;
	    struct ColorTableStruct *colorTabPtr;

	    colorTabPtr = Blt_PseudoColorTable(interp, tkwin, image);

	    srcPtr = Blt_ColorImageBits(image);
	    rowOffset = 0;
	    for (y = 0; y < height; y++) {
		destPtr = imagePtr->data + rowOffset;
		for (x = 0; x < width; x++, srcPtr++) {
		    pixel = PseudoColorPixel(srcPtr, colorTabPtr->lut);
		    switch (imagePtr->bits_per_pixel) {
		    case 32:
			*destPtr++ = (pixel >> 24) & 0xFF;
			/*FALLTHRU*/
		    case 24:
			*destPtr++ = (pixel >> 16) & 0xFF;
			/*FALLTHRU*/
		    case 16:
			*destPtr++ = (pixel >> 8) & 0xFF;
			/*FALLTHRU*/
		    case 8:
			*destPtr++ = pixel & 0xFF;
			/*FALLTHRU*/
		    }
		}
		rowOffset += imagePtr->bytes_per_line;
	    }
	    Blt_Free(colorTabPtr->lut);
	    *colorTablePtr = colorTabPtr;
	}
	break;
    default:
	return None;		/* Bad or unknown visual class. */
    }
    pixmapGC = Tk_GetGC(tkwin, 0L, (XGCValues *)NULL);
    pixmap = Tk_GetPixmap(display, Tk_WindowId(tkwin), width, height,
	Tk_Depth(tkwin));
    XPutImage(display, pixmap, pixmapGC, imagePtr, 0, 0, 0, 0, width, height);
    XDestroyImage(imagePtr);
    Tk_FreeGC(display, pixmapGC);
    return pixmap;
}

/* ARGSUSED */
static int
XGetImageErrorProc(clientData, errEventPtr)
    ClientData clientData;
    XErrorEvent *errEventPtr;
{
    int *errorPtr = clientData;

    *errorPtr = TCL_ERROR;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_DrawableToColorImage --
 *
 *      Takes a snapshot of an X drawable (pixmap or window) and
 *	converts it to a color image.
 *
 *	The trick here is to efficiently convert the pixel values
 *	(indices into the color table) into RGB color values.  In the
 *	days of 8-bit displays, it was simpler to get RGB values for
 *	all 256 indices into the colormap.  Instead we'll build a
 *	hashtable of unique pixels and from that an array of pixels to
 *	pass to XQueryColors.  For TrueColor visuals, we'll simple
 *	compute the colors from the pixel.
 *
 *	[I don't know how much faster it would be to take advantage
 *	of all the different visual types.  This pretty much depends
 *	on the size of the image and the number of colors it uses.]
 *
 * Results:
 *      Returns a color image of the drawable.  If an error occurred,
 *	NULL is returned.
 *
 *----------------------------------------------------------------------
 */
Blt_ColorImage
Blt_DrawableToColorImage(tkwin, drawable, x, y, width, height, inputGamma)
    Tk_Window tkwin;
    Drawable drawable;
    register int x, y;		/* Offset of image from the drawable's
				 * origin. */
    int width, height;		/* Dimension of the image.  Image must
				 * be completely contained by the
				 * drawable. */
    double inputGamma;
{
    XImage *imagePtr;
    Blt_ColorImage image;
    register Pix32 *destPtr;
    unsigned long pixel;
    int result = TCL_OK;
    Tk_ErrorHandler errHandler;
    Visual *visualPtr;
    unsigned char lut[256];

    errHandler = Tk_CreateErrorHandler(Tk_Display(tkwin), BadMatch,
	X_GetImage, -1, XGetImageErrorProc, &result);
    imagePtr = XGetImage(Tk_Display(tkwin), drawable, x, y, width, height, 
	AllPlanes, ZPixmap);
    Tk_DeleteErrorHandler(errHandler);
    XSync(Tk_Display(tkwin), False);
    if (result != TCL_OK) {
	return NULL;
    }

    {
	register int i;
	double value;
	
	for (i = 0; i < 256; i++) {
	    value = pow(i / 255.0, inputGamma) * 255.0 + 0.5;
	    lut[i] = (unsigned char)CLAMP(value);
	}
    }
    /*
     * First allocate a color image to hold the screen snapshot.
     */
    image = Blt_CreateColorImage(width, height);
    visualPtr = Tk_Visual(tkwin);
    if (visualPtr->class == TrueColor) {
	unsigned int red, green, blue;
	/*
	 * Directly compute the RGB color values from the pixel index
	 * rather than of going through XQueryColors.
	 */
	ComputeMasks(visualPtr);
	destPtr = Blt_ColorImageBits(image);
	for (y = 0; y < height; y++) {
	    for (x = 0; x < width; x++) {
		pixel = XGetPixel(imagePtr, x, y);

		red = ((pixel & visualPtr->red_mask) >> redMaskShift) << redAdjust;
		green = ((pixel & visualPtr->green_mask) >> greenMaskShift) << greenAdjust;
		blue = ((pixel & visualPtr->blue_mask) >> blueMaskShift) << blueAdjust;

		/*
		 * The number of bits per color in the pixel may be
		 * less than eight. For example, 15/16 bit displays
		 * (hi-color) use only 5 bits, 8-bit displays use 2 or
		 * 3 bits (don't ask me why you'd have an 8-bit
		 * TrueColor display). So shift back the least
		 * significant bits.
		 */
		destPtr->Red = lut[red];
		destPtr->Green = lut[green];
		destPtr->Blue = lut[blue];
		destPtr->Alpha = (unsigned char)-1;
		destPtr++;
	    }
	}
	XDestroyImage(imagePtr);
    } else {
	Blt_HashEntry *hPtr;
	Blt_HashSearch cursor;
	Blt_HashTable pixelTable;
	XColor *colorPtr, *colorArr;
	Pix32 *endPtr;
	int nPixels;
	int nColors;
	int isNew;

	/*
	 * Fill the array with each pixel of the image. At the same time, build
	 * up a hashtable of the pixels used.
	 */
	nPixels = width * height;
	Blt_InitHashTableWithPool(&pixelTable, BLT_ONE_WORD_KEYS);
	destPtr = Blt_ColorImageBits(image);
	for (y = 0; y < height; y++) {
	    for (x = 0; x < width; x++) {
		pixel = XGetPixel(imagePtr, x, y);
		hPtr = Blt_CreateHashEntry(&pixelTable, (char *)pixel, &isNew);
		if (isNew) {
		    Blt_SetHashValue(hPtr, (char *)pixel);
		}
		destPtr->value = pixel;
		destPtr++;
	    }
	}
	XDestroyImage(imagePtr);

	/* 
	 * Convert the hashtable of pixels into an array of XColors so
	 * that we can call XQueryColors with it. XQueryColors will
	 * convert the pixels into their RGB values.  
	 */
	nColors = pixelTable.numEntries;
	colorArr = Blt_Malloc(sizeof(XColor) * nColors);
	assert(colorArr);

	colorPtr = colorArr;
	for (hPtr = Blt_FirstHashEntry(&pixelTable, &cursor); hPtr != NULL;
	    hPtr = Blt_NextHashEntry(&cursor)) {
	    colorPtr->pixel = (unsigned long)Blt_GetHashValue(hPtr);
	    Blt_SetHashValue(hPtr, (char *)colorPtr);
	    colorPtr++;
	}
	XQueryColors(Tk_Display(tkwin), Tk_Colormap(tkwin), colorArr, nColors);

	/* 
	 * Go again through the array of pixels, replacing each pixel
	 * of the image with its RGB value.  
	 */
	destPtr = Blt_ColorImageBits(image);
	endPtr = destPtr + nPixels;
	for (/* empty */; destPtr < endPtr; destPtr++) {
	    hPtr = Blt_FindHashEntry(&pixelTable, (char *)destPtr->value);
	    colorPtr = (XColor *)Blt_GetHashValue(hPtr);
	    destPtr->Red = lut[colorPtr->red >> 8];
	    destPtr->Green = lut[colorPtr->green >> 8];
	    destPtr->Blue = lut[colorPtr->blue >> 8];
	    destPtr->Alpha = (unsigned char)-1;
	}
	Blt_Free(colorArr);
	Blt_DeleteHashTable(&pixelTable);
    }
    return image;
}


Pixmap
Blt_PhotoImageMask(tkwin, src)
    Tk_Window tkwin;
    Tk_PhotoImageBlock src;
{
    Pixmap bitmap;
    int arraySize, bytes_per_line;
    int offset, count;
    int value, bitMask;
    register int x, y;
    unsigned char *bits;
    unsigned char *srcPtr;
    unsigned char *destPtr;
    unsigned long pixel;

    bytes_per_line = (src.width + 7) / 8;
    arraySize = src.height * bytes_per_line;
    bits = Blt_Malloc(sizeof(unsigned char) * arraySize);
    assert(bits);
    destPtr = bits;
    offset = count = 0;
    for (y = 0; y < src.height; y++) {
	value = 0, bitMask = 1;
	srcPtr = src.pixelPtr + offset;
	for (x = 0; x < src.width; /*empty*/ ) {
	    pixel = (srcPtr[src.offset[3]] != 0x00);
	    if (pixel) {
		value |= bitMask;
	    } else {
		count++;	/* Count the number of transparent pixels. */
	    }
	    bitMask <<= 1;
	    x++;
	    if (!(x & 7)) {
		*destPtr++ = (unsigned char)value;
		value = 0, bitMask = 1;
	    }
	    srcPtr += src.pixelSize;
	}
	if (x & 7) {
	    *destPtr++ = (unsigned char)value;
	}
	offset += src.pitch;
    }
    if (count > 0) {
	Tk_MakeWindowExist(tkwin);
	bitmap = XCreateBitmapFromData(Tk_Display(tkwin), Tk_WindowId(tkwin),
	    (char *)bits, (unsigned int)src.width, (unsigned int)src.height);
    } else {
	bitmap = None;		/* Image is opaque. */
    }
    Blt_Free(bits);
    return bitmap;
}

Pixmap
Blt_ColorImageMask(tkwin, image)
    Tk_Window tkwin;
    Blt_ColorImage image;
{
    Pixmap bitmap;
    int arraySize, bytes_per_line;
    int count;
    int value, bitMask;
    register int x, y;
    unsigned char *bits;
    Pix32 *srcPtr;
    unsigned char *destPtr;
    unsigned long pixel;
    int width, height;

    width = Blt_ColorImageWidth(image);
    height = Blt_ColorImageHeight(image);
    bytes_per_line = (width + 7) / 8;
    arraySize = height * bytes_per_line;
    bits = Blt_Malloc(sizeof(unsigned char) * arraySize);
    assert(bits);
    destPtr = bits;
    count = 0;
    srcPtr = Blt_ColorImageBits(image);
    for (y = 0; y < height; y++) {
	value = 0, bitMask = 1;
	for (x = 0; x < width; /*empty*/ ) {
	    pixel = (srcPtr->Alpha != 0x00);
	    if (pixel) {
		value |= bitMask;
	    } else {
		count++;	/* Count the number of transparent pixels. */
	    }
	    bitMask <<= 1;
	    x++;
	    if (!(x & 7)) {
		*destPtr++ = (unsigned char)value;
		value = 0, bitMask = 1;
	    }
	    srcPtr++;
	}
	if (x & 7) {
	    *destPtr++ = (unsigned char)value;
	}
    }
    if (count > 0) {
	Tk_MakeWindowExist(tkwin);
	bitmap = XCreateBitmapFromData(Tk_Display(tkwin), Tk_WindowId(tkwin),
	    (char *)bits, (unsigned int)width, (unsigned int)height);
    } else {
	bitmap = None;		/* Image is opaque. */
    }
    Blt_Free(bits);
    return bitmap;
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
 * Results:
 *	Returns a new bitmap containing the rotated image.
 *
 * -----------------------------------------------------------------
 */
Pixmap
Blt_RotateBitmap(tkwin, srcBitmap, srcWidth, srcHeight, theta,
    destWidthPtr, destHeightPtr)
    Tk_Window tkwin;
    Pixmap srcBitmap;		/* Source bitmap to be rotated */
    int srcWidth, srcHeight;	/* Width and height of the source bitmap */
    double theta;		/* Right angle rotation to perform */
    int *destWidthPtr, *destHeightPtr;
{
    Display *display;		/* X display */
    Window root;		/* Root window drawable */
    Pixmap destBitmap;
    int destWidth, destHeight;
    XImage *src, *dest;
    register int x, y;		/* Destination bitmap coordinates */
    register int sx, sy;	/* Source bitmap coordinates */
    unsigned long pixel;
    GC bitmapGC;
    double rotWidth, rotHeight;

    display = Tk_Display(tkwin);
    root = RootWindow(Tk_Display(tkwin), Tk_ScreenNumber(tkwin));

    /* Create a bitmap and image big enough to contain the rotated text */
    Blt_GetBoundingBox(srcWidth, srcHeight, theta, &rotWidth, &rotHeight,
	(Point2D *)NULL);
    destWidth = ROUND(rotWidth);
    destHeight = ROUND(rotHeight);
    destBitmap = Tk_GetPixmap(display, root, destWidth, destHeight, 1);
    bitmapGC = Blt_GetBitmapGC(tkwin);
    XSetForeground(display, bitmapGC, 0x0);
    XFillRectangle(display, destBitmap, bitmapGC, 0, 0, destWidth, destHeight);

    src = XGetImage(display, srcBitmap, 0, 0, srcWidth, srcHeight, 1, ZPixmap);
    dest = XGetImage(display, destBitmap, 0, 0, destWidth, destHeight, 1,
	ZPixmap);
    theta = FMOD(theta, 360.0);
    if (FMOD(theta, (double)90.0) == 0.0) {
	int quadrant;

	/* Handle right-angle rotations specifically */

	quadrant = (int)(theta / 90.0);
	switch (quadrant) {
	case ROTATE_270:	/* 270 degrees */
	    for (y = 0; y < destHeight; y++) {
		sx = y;
		for (x = 0; x < destWidth; x++) {
		    sy = destWidth - x - 1;
		    pixel = XGetPixel(src, sx, sy);
		    if (pixel) {
			XPutPixel(dest, x, y, pixel);
		    }
		}
	    }
	    break;

	case ROTATE_180:	/* 180 degrees */
	    for (y = 0; y < destHeight; y++) {
		sy = destHeight - y - 1;
		for (x = 0; x < destWidth; x++) {
		    sx = destWidth - x - 1, 
		    pixel = XGetPixel(src, sx, sy);
		    if (pixel) {
			XPutPixel(dest, x, y, pixel);
		    }
		}
	    }
	    break;

	case ROTATE_90:		/* 90 degrees */
	    for (y = 0; y < destHeight; y++) {
		sx = destHeight - y - 1;
		for (x = 0; x < destWidth; x++) {
		    sy = x;
		    pixel = XGetPixel(src, sx, sy);
		    if (pixel) {
			XPutPixel(dest, x, y, pixel);
		    }
		}
	    }
	    break;

	case ROTATE_0:		/* 0 degrees */
	    for (y = 0; y < destHeight; y++) {
		for (x = 0; x < destWidth; x++) {
		    pixel = XGetPixel(src, x, y);
		    if (pixel) {
			XPutPixel(dest, x, y, pixel);
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
	double sox, soy;	/* Offset from the center of
				 * the source rectangle. */
	double destCX, destCY;	/* Offset to the center of the destination
				 * rectangle. */
	double tx, ty;		/* Translated coordinates from center */
	double rx, ry;		/* Angle of rotation for x and y coordinates */

	radians = (theta / 180.0) * M_PI;
	sinTheta = sin(radians), cosTheta = cos(radians);

	/*
	 * Coordinates of the centers of the source and destination rectangles
	 */
	sox = srcWidth * 0.5;
	soy = srcHeight * 0.5;
	destCX = destWidth * 0.5;
	destCY = destHeight * 0.5;

	/* For each pixel of the destination image, transform back to the
	 * associated pixel in the source image. */

	for (y = 0; y < destHeight; y++) {
	    ty = y - destCY;
	    for (x = 0; x < destWidth; x++) {

		/* Translate origin to center of destination image. */
		tx = x - destCX;

		/* Rotate the coordinates about the origin. */
		rx = (tx * cosTheta) - (ty * sinTheta);
		ry = (tx * sinTheta) + (ty * cosTheta);

		/* Translate back to the center of the source image. */
		rx += sox;
		ry += soy;

		sx = ROUND(rx);
		sy = ROUND(ry);

		/*
		 * Verify the coordinates, since the destination image can be
		 * bigger than the source.
		 */

		if ((sx >= srcWidth) || (sx < 0) || (sy >= srcHeight) ||
		    (sy < 0)) {
		    continue;
		}
		pixel = XGetPixel(src, sx, sy);
		if (pixel) {
		    XPutPixel(dest, x, y, pixel);
		}
	    }
	}
    }
    /* Write the rotated image into the destination bitmap. */
    XPutImage(display, destBitmap, bitmapGC, dest, 0, 0, 0, 0, destWidth,
	destHeight);

    /* Clean up the temporary resources used. */
    XDestroyImage(src), XDestroyImage(dest);
    *destWidthPtr = destWidth;
    *destHeightPtr = destHeight;
    return destBitmap;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_ScaleBitmap --
 *
 *	Creates a new scaled bitmap from another bitmap. The new bitmap
 *	is bounded by a specified region. Only this portion of the bitmap
 *	is scaled from the original bitmap.
 *
 *	By bounding scaling to a region we can generate a new bitmap
 *	which is no bigger than the specified viewport.
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
Blt_ScaleBitmap(tkwin, srcBitmap, srcWidth, srcHeight, destWidth, destHeight)
    Tk_Window tkwin;
    Pixmap srcBitmap;
    int srcWidth, srcHeight, destWidth, destHeight;
{
    Display *display;
    GC bitmapGC;
    Pixmap destBitmap;
    Window root;
    XImage *src, *dest;
    double xScale, yScale;
    register int sx, sy;	/* Source bitmap coordinates */
    register int x, y;		/* Destination bitmap coordinates */
    unsigned long pixel;

    /* Create a new bitmap the size of the region and clear it */

    display = Tk_Display(tkwin);

    root = RootWindow(Tk_Display(tkwin), Tk_ScreenNumber(tkwin));
    destBitmap = Tk_GetPixmap(display, root, destWidth, destHeight, 1);
    bitmapGC = Blt_GetBitmapGC(tkwin);
    XSetForeground(display, bitmapGC, 0x0);
    XFillRectangle(display, destBitmap, bitmapGC, 0, 0, destWidth, destHeight);

    src = XGetImage(display, srcBitmap, 0, 0, srcWidth, srcHeight, 1, ZPixmap);
    dest = XGetImage(display, destBitmap, 0, 0, destWidth, destHeight, 1, 
		     ZPixmap);

    /*
     * Scale each pixel of destination image from results of source
     * image. Verify the coordinates, since the destination image can
     * be bigger than the source
     */
    xScale = (double)srcWidth / (double)destWidth;
    yScale = (double)srcHeight / (double)destHeight;

    /* Map each pixel in the destination image back to the source. */
    for (y = 0; y < destHeight; y++) {
	sy = (int)(yScale * (double)y);
	for (x = 0; x < destWidth; x++) {
	    sx = (int)(xScale * (double)x);
	    pixel = XGetPixel(src, sx, sy);
	    if (pixel) {
		XPutPixel(dest, x, y, pixel);
	    }
	}
    }
    /* Write the scaled image into the destination bitmap */

    XPutImage(display, destBitmap, bitmapGC, dest, 0, 0, 0, 0, 
	destWidth, destHeight);
    XDestroyImage(src), XDestroyImage(dest);
    return destBitmap;
}


/*
 * -----------------------------------------------------------------------
 *
 * Blt_RotateScaleBitmapRegion --
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
    unsigned int destWidth,		
    unsigned int destHeight,	/* Virtual size of destination bitmap. */
    double theta)		/* Angle to rotate bitmap.  */
{
    Display *display;		/* X display */
    Window root;		/* Root window drawable */
    Pixmap destBitmap;
    XImage *src, *dest;
    register int x, y;		/* Destination bitmap coordinates */
    register int sx, sy;	/* Source bitmap coordinates */
    unsigned long pixel;
    double xScale, yScale;
    double rotWidth, rotHeight;
    GC bitmapGC;

    display = Tk_Display(tkwin);
    root = RootWindow(Tk_Display(tkwin), Tk_ScreenNumber(tkwin));

    /* Create a bitmap and image big enough to contain the rotated text */
    bitmapGC = Blt_GetBitmapGC(tkwin);
    destBitmap = Tk_GetPixmap(display, root, regionWidth, regionHeight, 1);
    XSetForeground(display, bitmapGC, 0x0);
    XFillRectangle(display, destBitmap, bitmapGC, 0, 0, regionWidth, 
	regionHeight);

    src = XGetImage(display, srcBitmap, 0, 0, srcWidth, srcHeight, 1, ZPixmap);
    dest = XGetImage(display, destBitmap, 0, 0, regionWidth, regionHeight, 1,
	ZPixmap);
    theta = FMOD(theta, 360.0);

    Blt_GetBoundingBox(srcWidth, srcHeight, theta, &rotWidth, &rotHeight,
		       (Point2D *)NULL);
    xScale = rotWidth / (double)destWidth;
    yScale = rotHeight / (double)destHeight;

    if (FMOD(theta, (double)90.0) == 0.0) {
	int quadrant;

	/* Handle right-angle rotations specifically */

	quadrant = (int)(theta / 90.0);
	switch (quadrant) {
	case ROTATE_270:	/* 270 degrees */
	    for (y = 0; y < regionHeight; y++) {
		sx = (int)(yScale * (double)(y + regionY));
		for (x = 0; x < regionWidth; x++) {
		    sy = (int)(xScale *(double)(destWidth - (x + regionX) - 1));
		    pixel = XGetPixel(src, sx, sy);
		    if (pixel) {
			XPutPixel(dest, x, y, pixel);
		    }
		}
	    }
	    break;

	case ROTATE_180:	/* 180 degrees */
	    for (y = 0; y < regionHeight; y++) {
		sy = (int)(yScale * (double)(destHeight - (y + regionY) - 1));
		for (x = 0; x < regionWidth; x++) {
		    sx = (int)(xScale *(double)(destWidth - (x + regionX) - 1));
		    pixel = XGetPixel(src, sx, sy);
		    if (pixel) {
			XPutPixel(dest, x, y, pixel);
		    }
		}
	    }
	    break;

	case ROTATE_90:		/* 90 degrees */
	    for (y = 0; y < regionHeight; y++) {
		sx = (int)(yScale * (double)(destHeight - (y + regionY) - 1));
		for (x = 0; x < regionWidth; x++) {
		    sy = (int)(xScale * (double)(x + regionX));
		    pixel = XGetPixel(src, sx, sy);
		    if (pixel) {
			XPutPixel(dest, x, y, pixel);
		    }
		}
	    }
	    break;

	case ROTATE_0:		/* 0 degrees */
	    for (y = 0; y < regionHeight; y++) {
		sy = (int)(yScale * (double)(y + regionY));
		for (x = 0; x < regionWidth; x++) {
		    sx = (int)(xScale * (double)(x + regionX));
		    pixel = XGetPixel(src, sx, sy);
		    if (pixel) {
			XPutPixel(dest, x, y, pixel);
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
	double sox, soy; 	/* Offset from the center of the
				 * source rectangle. */
	double rox, roy; 	/* Offset to the center of the
				 * rotated rectangle. */
	double tx, ty;		/* Translated coordinates from center */
	double rx, ry;		/* Angle of rotation for x and y coordinates */

	radians = (theta / 180.0) * M_PI;
	sinTheta = sin(radians), cosTheta = cos(radians);

	/*
	 * Coordinates of the centers of the source and destination rectangles
	 */
	sox = srcWidth * 0.5;
	soy = srcHeight * 0.5;
	rox = rotWidth * 0.5;
	roy = rotHeight * 0.5;

	/* For each pixel of the destination image, transform back to the
	 * associated pixel in the source image. */

	for (y = 0; y < regionHeight; y++) {
	    ty = (yScale * (double)(y + regionY)) - roy;
	    for (x = 0; x < regionWidth; x++) {

		/* Translate origin to center of destination image. */
		tx = (xScale * (double)(x + regionX)) - rox;

		/* Rotate the coordinates about the origin. */
		rx = (tx * cosTheta) - (ty * sinTheta);
		ry = (tx * sinTheta) + (ty * cosTheta);

		/* Translate back to the center of the source image. */
		rx += sox;
		ry += soy;

		sx = ROUND(rx);
		sy = ROUND(ry);

		/*
		 * Verify the coordinates, since the destination image can be
		 * bigger than the source.
		 */

		if ((sx >= srcWidth) || (sx < 0) || (sy >= srcHeight) ||
		    (sy < 0)) {
		    continue;
		}
		pixel = XGetPixel(src, sx, sy);
		if (pixel) {
		    XPutPixel(dest, x, y, pixel);
		}
	    }
	}
    }
    /* Write the rotated image into the destination bitmap. */
    XPutImage(display, destBitmap, bitmapGC, dest, 0, 0, 0, 0, regionWidth,
      regionHeight);

    /* Clean up the temporary resources used. */
    XDestroyImage(src), XDestroyImage(dest);
    return destBitmap;

}

#if HAVE_JPEGLIB_H

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
