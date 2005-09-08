
/*
 * bltColor.c --
 *
 *	This module contains routines for color allocation strategies
 *	used with color images in the BLT toolkit.
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

/*
 * Color strategies of 8-bit visuals:
 *
 * Try to "best" represent an N-color image into 8-bit (256 color)
 * colormap.  The simplest method is use the high bits of each RGB
 * value (3 bits for red and green, 2 bits for blue).  But this
 * produces lots of contouring since the distribution of colors tends
 * to be clustered.  Other problems: probably can't allocate even 256
 * colors. Other applications will have already taken some color
 * slots.  Furthermore, we might be displaying several images, and we
 * can't assume that all images are representative of the colors used.
 *
 * If we use a private colormap, we may want to allocate some number
 * of colors from the default colormap to prevent flashing when
 * colormaps are switched.
 *
 * Switches:
 *
 *	-exact boolean		Try to match the colors of the image rather
 *				then generating a "best" color ramp.
 *
 *	-threshold value	Maximum average error. Indicates how far
 *				to reduce the quantized color palette.
 *
 *	-tolerance value	Allow colors within this distance to match.
 *				This will weight allocation towards harder
 *				to match colors, rather than the most
 *				frequent.
 *
 *	-mincolors number	Minimum number of reduced quantized colors.
 *				or color ramp.
 *
 *	-dither	boolean		Turn on/off Floyd-Steinberg dithering.
 *
 *	-keep number		Hint to keep the first N colors in the
 *				in the default colormap.  This only applies to
 *				private colormaps.
 *
 *	-ramp number		Number of colors to use in a linear ramp.
 *
 */

#include "bltInt.h"

#ifndef WIN32

#include "bltHash.h"
#include "bltImage.h"

#define NCOLORS		256


static void
GetPaletteSizes(nColors, nRedsPtr, nGreensPtr, nBluesPtr)
    int nColors;		/* Number of colors requested. */
    unsigned int *nRedsPtr;	/* (out) Number of red components. */
    unsigned int *nGreensPtr;	/* (out) Number of green components. */
    unsigned int *nBluesPtr;	/* (out) Number of blue components. */
{
    unsigned int nBlues, nReds, nGreens;

    assert(nColors > 1); 
    nBlues = nReds = nGreens = 0;
    while ((nBlues * nBlues * nBlues) <= nColors) {
	nBlues++;
    }
    nBlues--;
    while ((nReds * nReds * nBlues) <= nColors) {
	nReds++;
    }
    nReds--;
    nGreens = nColors / (nBlues * nReds);

    *nRedsPtr = nReds;
    *nGreensPtr = nGreens;
    *nBluesPtr = nBlues;
}

static void
BuildColorRamp(palettePtr, nColors)
    Pix32 *palettePtr;
    int nColors;
{
    register unsigned int r, g, b;
    unsigned int short red, green, blue;
    unsigned int nReds, nGreens, nBlues;

    GetPaletteSizes(nColors, &nReds, &nGreens, &nBlues);
    for (r = 0; r < nReds; r++) {
	red = (r * USHRT_MAX) / (nReds - 1);
	for (g = 0; g < nGreens; g++) {
	    green = (g * USHRT_MAX) / (nGreens - 1);
	    for (b = 0; b < nBlues; b++) {
		blue = (b * USHRT_MAX) / (nBlues - 1);
		palettePtr->Red = red;
		palettePtr->Green = green;
		palettePtr->Blue = blue;
		palettePtr++;
	    }
	}
    }

}

/*
 *----------------------------------------------------------------------
 *
 * QueryColormap --
 *
 *	This is for psuedo-color displays only.  Fills an array or
 *	XColors with the color values (RGB and pixel) currently
 *	allocated in the colormap.
 *
 * Results:
 *	The number of colors allocated is returned. The array "colorArr"
 *	will contain the XColor values of each color in the colormap.
 *
 *---------------------------------------------------------------------- 
 */

static int
QueryColormap(display, colorMap, mapColors, numMapColorsPtr)
    Display *display;
    Colormap colorMap;
    XColor mapColors[];
    int *numMapColorsPtr;
{
    unsigned long int pixelValues[NCOLORS];
    int numAvail, numMapColors;
    register int i;
    register XColor *colorPtr;
    register unsigned long *indexPtr;
    int inUse[NCOLORS];

    /* Initially, we assume all color cells are allocated. */
    memset((char *)inUse, 0, sizeof(int) * NCOLORS);

    /*
     * Start allocating color cells.  This will tell us which color cells
     * haven't already been allocated in the colormap.  We'll release the
     * cells as soon as we find out how many there are.
     */
    numAvail = 0;
    for (indexPtr = pixelValues, i = 0; i < NCOLORS; i++, indexPtr++) {
	if (!XAllocColorCells(display, colorMap, False, NULL, 0, indexPtr, 1)) {
	    break;
	}
	inUse[*indexPtr] = TRUE;/* Indicate the cell is unallocated */
	numAvail++;
    }
    XFreeColors(display, colorMap, pixelValues, numAvail, 0);

    /*
     * Put the indices of the cells already allocated into a color array.
     * We'll use the array to query the RGB values of the allocated colors.
     */
    numMapColors = 0;
    colorPtr = mapColors;
    for (i = 0; i < NCOLORS; i++) {
	if (!inUse[i]) {
	    colorPtr->pixel = i;
	    colorPtr->flags = (DoRed | DoGreen | DoBlue);
	    colorPtr++, numMapColors++;
	}
    }
    XQueryColors(display, colorMap, mapColors, numMapColors);
    *numMapColorsPtr = numMapColors;
#ifdef notdef
    fprintf(stderr, "Number of colors (allocated/free) %d/%d\n", numMapColors,
	numAvail);
#endif
    return numAvail;
}

static void
FindClosestColor(colorPtr, mapColors, numMapColors)
    ColorInfo *colorPtr;
    XColor mapColors[];
    int numMapColors;
{
    double r, g, b;
    register int i;
    double dist, min;
    XColor *lastMatch;
    register XColor *mapColorPtr;

    min = DBL_MAX;	/* Any color is closer. */
    lastMatch = NULL;

    /* Linear search of color */

    mapColorPtr = mapColors;
    for (i = 0; i < numMapColors; i++, mapColorPtr++) {
	r = (double)mapColorPtr->red - (double)colorPtr->exact.red;
	g = (double)mapColorPtr->green - (double)colorPtr->exact.green;
	b = (double)mapColorPtr->blue - (double)colorPtr->exact.blue;

	dist = (r * r) + (b * b) + (g * g);
	if (dist < min) {
	    min = dist;
	    lastMatch = mapColorPtr;
	}
    }
    colorPtr->best = *lastMatch;
    colorPtr->best.flags = (DoRed | DoGreen | DoBlue);
    colorPtr->error = (float)sqrt(min);
}

static int
CompareColors(a, b)
    void *a, *b;
{
    ColorInfo *i1Ptr, *i2Ptr;

    i1Ptr = *(ColorInfo **) a;
    i2Ptr = *(ColorInfo **) b;
    if (i2Ptr->error > i1Ptr->error) {
	return 1;
    } else if (i2Ptr->error < i1Ptr->error) {
	return -1;
    }
    return 0;
}

static float
MatchColors(colorTabPtr, rgbPtr, numColors, numAvailColors, numMapColors,
    mapColors)
    struct ColorTableStruct *colorTabPtr;
    Pix32 *rgbPtr;
    int numColors;
    int numAvailColors;
    int numMapColors;
    XColor mapColors[NCOLORS];
{
    int numMatched;
    float sum;
    register int i;
    register ColorInfo *colorPtr;

    /*
     * For each quantized color, compute and store the error (i.e
     * the distance from a color that's already been allocated).
     * We'll use this information to sort the colors based upon how
     * badly they match and their frequency to the color image.
     */
    colorPtr = colorTabPtr->colorInfo;
    for (i = 0; i < numColors; i++, colorPtr++, rgbPtr++) {
	colorPtr->index = i;
	colorTabPtr->sortedColors[i] = colorPtr;
	colorPtr->exact.red = rgbPtr->Red;
	colorPtr->exact.green = rgbPtr->Green;
	colorPtr->exact.blue = rgbPtr->Blue;
	colorPtr->exact.flags = (DoRed | DoGreen | DoBlue);
	FindClosestColor(colorPtr, mapColors, numMapColors);
    }

    /* Sort the colors, first by frequency (most to least), then by
     * matching error (worst to best).
     */
    qsort(colorTabPtr->sortedColors, numColors, sizeof(ColorInfo *),
	(QSortCompareProc *)CompareColors);

    for (i = 0; i < numColors; i++) {
	colorPtr = colorTabPtr->sortedColors[i];
	fprintf(stderr, "%d. %04x%04x%04x / %04x%04x%04x = %f (%d)\n", i,
	    colorPtr->exact.red, colorPtr->exact.green, colorPtr->exact.blue,
	    colorPtr->best.red, colorPtr->best.green, colorPtr->best.blue,
	    colorPtr->error, colorPtr->freq);
    }
    sum = 0.0;
    numMatched = 0;
    for (i = numAvailColors; i < numColors; i++) {
	colorPtr = colorTabPtr->sortedColors[i];
	sum += colorPtr->error;
	numMatched++;
    }
    if (numMatched > 0) {
	sum /= numMatched;
    }
    return sum;
}

    
static int
AllocateColors(nImageColors, colorTabPtr, matchOnly)
    int nImageColors;
    struct ColorTableStruct *colorTabPtr;
    int matchOnly;
{
    register int i;
    register ColorInfo *colorPtr;
    unsigned long int pixelValue;

    for (i = 0; i < nImageColors; i++) {
	colorPtr = colorTabPtr->sortedColors[i];
	if (matchOnly) {
	    XAllocColor(colorTabPtr->display, colorTabPtr->colorMap,
		&colorPtr->best);
	    pixelValue = colorPtr->best.pixel;
	} else {
	    colorPtr->allocated = XAllocColor(colorTabPtr->display,
		colorTabPtr->colorMap, &colorPtr->exact);

	    if (colorPtr->allocated) {
		pixelValue = colorPtr->exact.pixel;
	    } else {
		XAllocColor(colorTabPtr->display, colorTabPtr->colorMap,
		    &colorPtr->best);
		pixelValue = colorPtr->best.pixel;
	    }
	}
	colorTabPtr->pixelValues[colorPtr->index] = pixelValue;
    }
    colorTabPtr->nPixels = nImageColors;
    return 1;
}

ColorTable
Blt_CreateColorTable(tkwin)
    Tk_Window tkwin;
{
    XVisualInfo visualInfo, *visualInfoPtr;
    int nVisuals;
    Visual *visualPtr;
    Display *display;
    struct ColorTableStruct *colorTabPtr;

    display = Tk_Display(tkwin);
    visualPtr = Tk_Visual(tkwin);

    colorTabPtr = Blt_Calloc(1, sizeof(struct ColorTableStruct));
    assert(colorTabPtr);
    colorTabPtr->display = Tk_Display(tkwin);
    colorTabPtr->colorMap = Tk_Colormap(tkwin);

    visualInfo.screen = Tk_ScreenNumber(tkwin);
    visualInfo.visualid = XVisualIDFromVisual(visualPtr);
    visualInfoPtr = XGetVisualInfo(display, VisualScreenMask | VisualIDMask,
	&visualInfo, &nVisuals);

    colorTabPtr->visualInfo = *visualInfoPtr;
    XFree(visualInfoPtr);

    return colorTabPtr;
}

void
Blt_FreeColorTable(colorTabPtr)
    struct ColorTableStruct *colorTabPtr;
{
    if (colorTabPtr == NULL) {
	return;
    }
    if (colorTabPtr->nPixels > 0) {
	XFreeColors(colorTabPtr->display, colorTabPtr->colorMap,
	    colorTabPtr->pixelValues, colorTabPtr->nPixels, 0);
    }
    Blt_Free(colorTabPtr);
}

extern int redAdjust, greenAdjust, blueAdjust;
extern int redMaskShift, greenMaskShift, blueMaskShift;

/*
 *----------------------------------------------------------------------
 *
 * Blt_DirectColorTable --
 *
 *	Creates a color table using the DirectColor visual.  We try
 *	to allocate colors across the color spectrum.
 *
 * Results:
 *
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
ColorTable
Blt_DirectColorTable(interp, tkwin, image)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    Blt_ColorImage image;
{
    struct ColorTableStruct *colorTabPtr;
    Visual *visualPtr;
    Display *display;
    XColor color;
    int nr, ng, nb;
    int rBand, gBand, bBand;
    int rLast, gLast, bLast;
    unsigned int r, g, b;
    unsigned int value;
    register int i;

    display = Tk_Display(tkwin);
    visualPtr = Tk_Visual(tkwin);

    colorTabPtr = Blt_CreateColorTable(tkwin);
    /*
     * Compute the number of distinct colors in each band
     */
    nr = ((unsigned int)visualPtr->red_mask >> redMaskShift) + 1;
    ng = ((unsigned int)visualPtr->green_mask >> greenMaskShift) + 1;
    nb = ((unsigned int)visualPtr->blue_mask >> blueMaskShift) + 1;

#ifdef notdef
    assert((nr <= visualPtr->map_entries) && (ng <= visualPtr->map_entries) &&
	(nb <= visualPtr->map_entries));
#endif
    rBand = NCOLORS / nr;
    gBand = NCOLORS / ng;
    bBand = NCOLORS / nb;

  retry:
    color.flags = (DoRed | DoGreen | DoBlue);
    rLast = gLast = bLast = 0;
    r = g = b = 0;
    for (i = 0; i < visualPtr->map_entries; i++) {
	if (rLast < NCOLORS) {
	    r = rLast + rBand;
	    if (r > NCOLORS) {
		r = NCOLORS;
	    }
	}
	if (gLast < NCOLORS) {
	    g = gLast + gBand;
	    if (g > NCOLORS) {
		g = NCOLORS;
	    }
	}
	if (bLast < NCOLORS) {
	    b = bLast + bBand;
	    if (b > NCOLORS) {
		b = NCOLORS;
	    }
	}
	color.red = (r - 1) * (NCOLORS + 1);
	color.green = (g - 1) * (NCOLORS + 1);
	color.blue = (b - 1) * (NCOLORS + 1);

	if (!XAllocColor(display, colorTabPtr->colorMap, &color)) {
	    XFreeColors(display, colorTabPtr->colorMap,
		colorTabPtr->pixelValues, i, 0);
	    if ((colorTabPtr->flags & PRIVATE_COLORMAP) == 0) {
		/*
		 * If we can't allocate a color in the default
		 * colormap, try again, this time with a private
		 * colormap.
		 */
		fprintf(stderr, "Need to allocate private colormap\n");
		colorTabPtr->colorMap = Tk_GetColormap(interp, tkwin, ".");

		XSetWindowColormap(display, Tk_WindowId(tkwin),
		    colorTabPtr->colorMap);
		colorTabPtr->flags |= PRIVATE_COLORMAP;
		goto retry;
	    }
#ifdef notdef
	    fprintf(stderr, "Failed to allocate after %d colors\n", i);
#endif
	    Blt_Free(colorTabPtr);
	    return NULL;	/* Ran out of colors in private map? */
	}
	colorTabPtr->pixelValues[i] = color.pixel;
	/*
	 * Fill in pixel values for each band at this intensity
	 */
	value = color.pixel & visualPtr->red_mask;
	while (rLast < r) {
	    colorTabPtr->red[rLast++] = value;
	}
	value = color.pixel & visualPtr->green_mask;
	while (gLast < g) {
	    colorTabPtr->green[gLast++] = value;
	}
	value = color.pixel & visualPtr->blue_mask;
	while (bLast < b) {
	    colorTabPtr->blue[bLast++] = value;
	}
    }
    colorTabPtr->nPixels = i;
    return colorTabPtr;
}

/*
 * First attempt:
 *	Allocate colors all the colors in the image (up to NCOLORS). Bail out
 *	on the first failure or if we need more than NCOLORS.
 */
static int
GetUniqueColors(image)
    Blt_ColorImage image;
{
    register int i, nColors;
    register Pix32 *pixelPtr;
    Pix32 color;
    Blt_HashEntry *hPtr;
    int isNew, nPixels;
    int refCount;
    Blt_HashTable colorTable;

    Blt_InitHashTable(&colorTable, BLT_ONE_WORD_KEYS);

    nPixels = Blt_ColorImageWidth(image) * Blt_ColorImageHeight(image);
    nColors = 0;
    pixelPtr = Blt_ColorImageBits(image);
    for (i = 0; i < nPixels; i++, pixelPtr++) {
	color.value = pixelPtr->value;
	color.Alpha = 0xFF;	/* Ignore alpha-channel values */
	hPtr = Blt_CreateHashEntry(&colorTable, (char *)color.value, &isNew);
	if (isNew) {
	    refCount = 1;
	    nColors++;
	} else {
	    refCount = (int)Blt_GetHashValue(hPtr);
	    refCount++;
	}
	Blt_SetHashValue(hPtr, (ClientData)refCount);
    }
    Blt_DeleteHashTable(&colorTable);
    return nColors;
}

#define Blt_DefaultColormap(tkwin)  \
	DefaultColormap(Tk_Display(tkwin), Tk_ScreenNumber(tkwin))


static void
PrivateColormap(interp, colorTabPtr, image, tkwin)
    Tcl_Interp *interp;
    struct ColorTableStruct *colorTabPtr;
    Blt_ColorImage image;
    Tk_Window tkwin;
{
    int keepColors = 0;
    register int i;
    XColor usedColors[NCOLORS];
    int nFreeColors, nUsedColors;
    Colormap colorMap;
    int inUse[NCOLORS];
    XColor *colorPtr;
    XColor *imageColors;

    /*
     * Create a private colormap if one doesn't already exist for the
     * window.
     */

    colorTabPtr->colorMap = colorMap = Tk_Colormap(tkwin);

    nUsedColors = 0;		/* Number of colors allocated */

    if (colorTabPtr->nPixels > 0) {
	XFreeColors(colorTabPtr->display, colorTabPtr->colorMap,
	    colorTabPtr->pixelValues, colorTabPtr->nPixels, 0);
    }
    nFreeColors = QueryColormap(colorTabPtr->display, colorMap, usedColors,
	&nUsedColors);
    memset((char *)inUse, 0, sizeof(int) * NCOLORS);
    if ((nUsedColors == 0) && (keepColors > 0)) {

	/*
	 * We're starting with a clean colormap so find out what colors
	 * have been used in the default colormap.
	 */

	nFreeColors = QueryColormap(colorTabPtr->display,
	    Blt_DefaultColormap(tkwin), usedColors, &nUsedColors);

	/*
	 * Copy a number of colors from the default colormap into the private
	 * colormap.  We can assume that this is the working set from most
	 * (non-image related) applications. While this doesn't stop our
	 * image from flashing and looking dumb when colormaps are swapped
	 * in and out, at least everything else should remain unaffected.
	 */

	if (nUsedColors > keepColors) {
	    nUsedColors = keepColors;
	}
	/*
	 * We want to allocate colors in the same ordering as the old colormap,
	 * and we can't assume that the colors in the old map were contiguous.
	 * So mark the colormap locations (i.e. pixels) that we find in use.
	 */

    }
    for (colorPtr = usedColors, i = 0; i < nUsedColors; i++, colorPtr++) {
	inUse[colorPtr->pixel] = TRUE;
    }

    /*
     * In an "exact" colormap, we try to allocate as many of colors from the
     * image as we can fit.  If necessary, we'll cheat and reduce the number
     * of colors by quantizing.
     */
    imageColors = usedColors + nUsedColors;

    Tk_SetWindowColormap(tkwin, colorMap);
}

ColorTable
Blt_PseudoColorTable(interp, tkwin, image)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    Blt_ColorImage image;
{
    struct ColorTableStruct *colorTabPtr;
    Colormap defColorMap;
    int usePrivate;

    colorTabPtr = Blt_CreateColorTable(tkwin);
    defColorMap = DefaultColormap(colorTabPtr->display, Tk_ScreenNumber(tkwin));
    if (colorTabPtr->colorMap == defColorMap) {
	fprintf(stderr, "Using default colormap\n");
    }
    /* All other visuals use an 8-bit colormap */
    colorTabPtr->lut = Blt_Malloc(sizeof(unsigned int) * 33 * 33 * 33);
    assert(colorTabPtr->lut);

    usePrivate = TRUE;
    if (usePrivate) {
	PrivateColormap(interp, colorTabPtr, image, tkwin);
    } else {
#ifdef notdef
	ReadOnlyColormap(colorTabPtr, image, tkwin);
#endif
    }
    return colorTabPtr;
}

#ifdef notdef

static void
ConvoleColorImage(srcImage, destImage, kernelPtr)
    Blt_ColorImage srcImage, destImage;
    ConvoleKernel *kernelPtr;
{
    Pix32 *srcPtr, *destPtr;
    Pix32 *src[MAXROWS];
    register int x, y, i, j;
    int red, green, blue;

    /* i = 0 case, ignore left column of pixels */

    srcPtr = Blt_ColorImageBits(srcImage);
    destPtr = Blt_ColorImageBits(destImage);

    width = Blt_ColorImageWidth(srcImage);
    height = Blt_ColorImageHeight(srcImage);

    yOffset = kernelPtr->height / 2;
    xOffset = kernelPtr->width / 2;
    for (y = yOffset; y < (height - yOffset); y++) {
	/* Set up pointers to individual rows */
	for (i = 0; i < kernelPtr->height; i++) {
	    src[i] = srcPtr + (i * width);
	}
	for (x = xOffset; x < (width - xOffset); x++) {
	    red = green = blue = 0;
	    kernPtr = kernelPtr->values;
	    for (i = 0; i < kernelPtr->height; i++) {
		for (j = 0; j < kernelPtr->width; j++) {
		    red += *valuePtr * src[i][j].Red;
		    green += *valuePtr * src[i][j].Green;
		    blue += *valuePtr * src[i][j].Blue;
		    valuePtr++;
		}
	    }
	    destPtr->Red = red / kernelPtr->sum;
	    destPtr->Green = green / kernelPtr->sum;
	    destPtr->Blue = blue / kernelPtr->sum;
	    destPtr++;
	}
	srcPtr += width;
    }
    sum = bot[0].Red +
	red = bot[0].Red + bot[1].Red + mid[1].Red + top[0].Red + top[1].Red;
    green = bot[0].Green + bot[1].Green + mid[1].Green + top[0].Green +
	top[1].Green;
    blue = bot[0].Blue + bot[1].Blue + mid[1].Blue + top[0].Blue + top[1].Blue;
    error = (red / 5) - mid[0].Red;
    redVal = mid[0].Red - (error * blend / blend_divisor);
    error = (green / 5) - mid[0].Green;
    greenVal = mid[0].Green - (error * blend / blend_divisor);
    error = (blue / 5) - mid[0].Blue;
    blueVal = mid[0].Blue - (error * blend / blend_divisor);

    out[0].Red = CLAMP(redVal);
    out[0].Green = CLAMP(greenVal);
    out[0].Blue = CLAMP(blueVal);

    for (i = 1; i < (width - 1); i++) {
	for (chan = 0; chan < 3; chan++) {
	    total = bot[chan][i - 1] + bot[chan][i] + bot[chan][i + 1] +
		mid[chan][i - 1] + mid[chan][i + 1] +
		top[chan][i - 1] + top[chan][i] + top[chan][i + 1];
	    avg = total >> 3;	/* divide by 8 */
	    diff = avg - mid[chan][i];
	    result = mid[chan][i] - (diff * blend / blend_divisor);
	    out[chan][i] = CLAMP(result);
	}
    }
    /* i = in_hdr.xmax case, ignore right column of pixels */
    for (chan = 0; chan < 3; chan++) {
	total = bot[chan][i - 1] + bot[chan][i] +
	    mid[chan][i - 1] +
	    top[chan][i - 1] + top[chan][i];
	avg = total / 5;
	diff = avg - mid[chan][i];
	result = mid[chan][i] - (diff * blend / blend_divisor);
	out[chan][i] = CLAMP(result);
    }
}

static void
DitherRow(srcImage, destImage, lastRow, curRow)
    Blt_ColorImage srcImage, destImage;
    int width, height;
    int bottom, top;
{
    int width, height;

    width = Blt_ColorImageWidth(srcImage);
    topPtr = Blt_ColorImageBits(destPtr) + (width * row);
    rowPtr = topPtr + width;
    botPtr = rowPtr + width;

    for (x = 0; x < width; x++) {

	/* Clamp current error entry */

	midPtr->red = CLAMP(midPtr->red);
	midPtr->blue = CLAMP(midPtr->blue);
	midPtr->green = CLAMP(midPtr->green);

	r = (midPtr->red >> 3) + 1;
	g = (midPtr->green >> 3) + 1;
	b = (midPtr->blue >> 3) + 1;
	index = colorTabPtr->lut[r][g][b];

	redVal = midPtr->red * (NCOLORS + 1);
	greenVal = midPtr->green * (NCOLORS + 1);
	blueVal = midPtr->blue * (NCOLORS + 1);

	error = colorVal - colorMap[index].red;
	if (x < 511) {
	    currRow[x + 1].Red = currRow[x + 1].Red + 7 * error / 16;
	    nextRow[x + 1].Red = nextRow[x + 1].Red + error / 16;
	}
	nextRow[x].Red = nextRow[x].Red + 5 * error / 16;
	if (x > 0) {
	    nextRow[x - 1].Red = nextRow[x - 1].Red + 3 * error / 16;
	}
	error = row[x][c] - colormap[index][c];

	value = srcPtr->channel[i] * error[i];
	value = CLAMP(value);
	destPtr->channel[i] = value;

	/* Closest pixel */
	pixel = PsuedoColorPixel();
	error[RED] = colorPtr->Red - srcPtr->Red * (NCOLORS + 1);

	/* translate pixel to colorInfoPtr to get error */
	colorTabPtr->lut[r][g][b];
	colorPtr = PixelToColorInfo(pixel);
	error = colorPtr->error;

	register rle_pixel *optr;
	register int j;
	register short *thisptr, *nextptr = NULL;
	int chan;
	static int nchan = 0;
	int lastline = 0, lastpixel;
	static int *cval = 0;
	static rle_pixel *pixel = 0;

	if (nchan != in_hdr->ncolors)
	    if (cval) {
		Blt_Free(cval);
		Blt_Free(pixel);
	    }
	nchan = in_hdr->ncolors;
	if (!cval) {
	    if ((cval = Blt_Malloc(nchan * sizeof(int))) == 0)
		    malloc_ERR;
	    if ((pixel = Blt_Malloc(nchan * sizeof(rle_pixel))) == 0)
		malloc_ERR;
	}
	optr = outrow[RLE_RED];

	thisptr = row_top;
	if (row_bottom)
	    nextptr = row_bottom;
	else
	    lastline = 1;

	for (x = 0; x < width; x++) {
	    int cmap_index = 0;

	    lastpixel = (x == (width - 1));
	    val = srcPtr->Red;

	    for (chan = 0; chan < 3; chan++) {
		cval[chan] = *thisptr++;

		/*
		 * Current channel value has been accumulating error,
		 * it could be out of range.
		 */
		if (cval[chan] < 0)
		    cval[chan] = 0;
		else if (cval[chan] > 255)
		    cval[chan] = 255;

		pixel[chan] = cval[chan];
	    }

	    /* find closest color */
	    find_closest(map, nchan, maplen, pixel, &cmap_index);
	    *optr++ = cmap_index;

	    /* thisptr is now looking at pixel to the right of current pixel
	     * nextptr is looking at pixel below current pixel
	     * So, increment thisptr as stuff gets stored.  nextptr gets moved
	     * by one, and indexing is done +/- nchan.
	     */
	    for (chan = 0; chan < nchan; chan++) {
		cval[chan] -= map[chan][cmap_index];

		if (!lastpixel) {
		    thisptr[chan] += cval[chan] * 7 / 16;
		}
		if (!lastline) {
		    if (j != 0) {
			nextptr[-nchan] += cval[chan] * 3 / 16;
		    }
		    nextptr[0] += cval[chan] * 5 / 16;
		    if (!lastpixel) {
			nextptr[nchan] += cval[chan] / 16;
		    }
		    nextptr++;
		}
	    }
	}
    }
}

/********************************************/
static Blt_ColorImage
DoColorDither(pic24, pic8, w, h, rmap, gmap, bmap, rdisp, gdisp, bdisp, maplen)
    byte *pic24, *pic8, *rmap, *gmap, *bmap, *rdisp, *gdisp, *bdisp;
    int w, h, maplen;
{
    /* takes a 24 bit picture, of size w*h, dithers with the colors in
       rdisp, gdisp, bdisp (which have already been allocated),
       and generates an 8-bit w*h image, which it returns.
       ignores input value 'pic8'
       returns NULL on error

       note: the rdisp,gdisp,bdisp arrays should be the 'displayed' colors,
       not the 'desired' colors

       if pic24 is NULL, uses the passed-in pic8 (an 8-bit image) as
       the source, and the rmap,gmap,bmap arrays as the desired colors */

    byte *np, *ep, *newpic;
    short *cache;
    int r2, g2, b2;
    int *thisline, *nextline, *thisptr, *nextptr, *tmpptr;
    int i, j, rerr, gerr, berr, pwide3;
    int imax, jmax;
    int key;
    long cnt1, cnt2;
    int error[512];		/* -255 .. 0 .. +255 */

    /* compute somewhat non-linear floyd-steinberg error mapping table */
    for (i = j = 0; i <= 0x40; i++, j++) {
	error[256 + i] = j;
	error[256 - i] = -j;
    }
    for ( /*empty*/ ; i < 0x80; i++, j += !(i & 1) ? 1 : 0) {
	error[256 + i] = j;
	error[256 - i] = -j;
    }
    for ( /*empty*/ ; i <= 0xff; i++) {
	error[256 + i] = j;
	error[256 - i] = -j;
    }

    cnt1 = cnt2 = 0;
    pwide3 = w * 3;
    imax = h - 1;
    jmax = w - 1;
    ep = (pic24) ? pic24 : pic8;

    /* attempt to malloc things */
    newpic = Blt_Malloc((size_t) (w * h));
    cache = Blt_Calloc((size_t) (2 << 14), sizeof(short));
    thisline = Blt_Malloc(pwide3 * sizeof(int));
    nextline = Blt_Malloc(pwide3 * sizeof(int));
    if (!cache || !newpic || !thisline || !nextline) {
	if (newpic)
	    Blt_Free(newpic);
	if (cache)
	    Blt_Free(cache);
	if (thisline)
	    Blt_Free(thisline);
	if (nextline)
	    Blt_Free(nextline);
	return (byte *) NULL;
    }
    np = newpic;

    /* Get first line of picture in reverse order. */

    srcPtr = Blt_ColorImageBits(image), tempPtr = tempArr;
    for (x = 0; x < width; x++, tempPtr++, srcPtr--) {
	*tempPtr = *srcPtr;
    }

    for (y = 0; y < height; y++) {
	tempPtr = curRowPtr, curRowPtr = nextRowPtr, nextRowPtr = tempPtr;

	if (y != (height - 1)) {/* get next line */
	    for (x = 0; x < width; x++, tempPtr++, srcPtr--)
		*tempPtr = *srcPtr;
	}
    }


    Blt_Free(thisline);
    Blt_Free(nextline);
    Blt_Free(cache);

    return newpic;
}


static void
DitherImage(image)
    Blt_ColorImage image;
{
    int width, height;



}

#endif

#endif /* WIN32 */
