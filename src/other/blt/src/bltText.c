
/*
 * bltText.c --
 *
 *	This module implements multi-line, rotate-able text for the BLT toolkit.
 *
 * Copyright 1993-1998 Lucent Technologies, Inc.
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
#include <X11/Xutil.h>
#include "bltImage.h"

#define WINDEBUG	0

static Blt_HashTable bitmapGCTable;
static int initialized;

static void
DrawTextLayout(display, drawable, gc, font, x, y, textPtr)
    Display *display;
    Drawable drawable;
    GC gc;
    Tk_Font font;
    register int x, y;		/* Origin of text */
    TextLayout *textPtr;
{
    register TextFragment *fragPtr;
    register int i;

    fragPtr = textPtr->fragArr;
    for (i = 0; i < textPtr->nFrags; i++, fragPtr++) {
#if HAVE_UTF
	Tk_DrawChars(display, drawable, gc, font, fragPtr->text,
	    fragPtr->count, x + fragPtr->x, y + fragPtr->y);
#else
	XDrawString(display, drawable, gc, x + fragPtr->x, y + fragPtr->y,
	    fragPtr->text, fragPtr->count);
#endif /*HAVE_UTF*/
    }
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_GetTextLayout --
 *
 *	Get the extents of a possibly multiple-lined text string.
 *
 * Results:
 *	Returns via *widthPtr* and *heightPtr* the dimensions of
 *	the text string.
 *
 * -----------------------------------------------------------------
 */
TextLayout *
Blt_GetTextLayout(string, tsPtr)
    char string[];
    TextStyle *tsPtr;
{
    int maxHeight, maxWidth;
    int count;			/* Count # of characters on each line */
    int nFrags;
    int width;			/* Running dimensions of the text */
    TextFragment *fragPtr;
    TextLayout *textPtr;
    int lineHeight;
    int size;
    register char *p;
    register int i;
    Tk_FontMetrics fontMetrics;

    Tk_GetFontMetrics(tsPtr->font, &fontMetrics);
    lineHeight = fontMetrics.linespace + 
	tsPtr->leader + tsPtr->shadow.offset;
    nFrags = 0;
    for (p = string; *p != '\0'; p++) {
	if (*p == '\n') {
	    nFrags++;
	}
    }
    if ((p != string) && (*(p - 1) != '\n')) {
	nFrags++;
    }
    size = sizeof(TextLayout) + (sizeof(TextFragment) * (nFrags - 1));
    textPtr = Blt_Calloc(1, size);
    textPtr->nFrags = nFrags;
    nFrags = count = 0;
    width = maxWidth = 0;
    maxHeight = tsPtr->padTop;
    fragPtr = textPtr->fragArr;
    for (p = string; *p != '\0'; p++) {
	if (*p == '\n') {
	    if (count > 0) {
		width = Tk_TextWidth(tsPtr->font, string, count) +
		    tsPtr->shadow.offset;
		if (width > maxWidth) {
		    maxWidth = width;
		}
	    }
	    fragPtr->width = width;
	    fragPtr->count = count;
	    fragPtr->y = maxHeight + fontMetrics.ascent;
	    fragPtr->text = string;
	    fragPtr++;
	    nFrags++;
	    maxHeight += lineHeight;
	    string = p + 1;	/* Start the string on the next line */
	    count = 0;		/* Reset to indicate the start of a new line */
	    continue;
	}
	count++;
    }
    if (nFrags < textPtr->nFrags) {
	width = Tk_TextWidth(tsPtr->font, string, count) + tsPtr->shadow.offset;
	if (width > maxWidth) {
	    maxWidth = width;
	}
	fragPtr->width = width;
	fragPtr->count = count;
	fragPtr->y = maxHeight + fontMetrics.ascent;
	fragPtr->text = string;
	maxHeight += lineHeight;
	nFrags++;
    }
    maxHeight += tsPtr->padBottom;
    maxWidth += PADDING(tsPtr->padX);
    fragPtr = textPtr->fragArr;
    for (i = 0; i < nFrags; i++, fragPtr++) {
	switch (tsPtr->justify) {
	default:
	case TK_JUSTIFY_LEFT:
	    /* No offset for left justified text strings */
	    fragPtr->x = tsPtr->padLeft;
	    break;
	case TK_JUSTIFY_RIGHT:
	    fragPtr->x = (maxWidth - fragPtr->width) - tsPtr->padRight;
	    break;
	case TK_JUSTIFY_CENTER:
	    fragPtr->x = (maxWidth - fragPtr->width) / 2;
	    break;
	}
    }
    textPtr->width = maxWidth;
    textPtr->height = maxHeight - tsPtr->leader;
    return textPtr;
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_GetTextExtents --
 *
 *	Get the extents of a possibly multiple-lined text string.
 *
 * Results:
 *	Returns via *widthPtr* and *heightPtr* the dimensions of
 *	the text string.
 *
 * -----------------------------------------------------------------
 */
void
Blt_GetTextExtents(tsPtr, string, widthPtr, heightPtr)
    TextStyle *tsPtr;
    char *string;
    int *widthPtr, *heightPtr;
{
    int count;			/* Count # of characters on each line */
    int width, height;
    int w, lineHeight;
    register char *p;
    Tk_FontMetrics fontMetrics;

    if (string == NULL) {
	return;			/* NULL string? */
    }
    Tk_GetFontMetrics(tsPtr->font, &fontMetrics);
    lineHeight = fontMetrics.linespace + tsPtr->leader + tsPtr->shadow.offset;
    count = 0;
    width = height = 0;
    for (p = string; *p != '\0'; p++) {
	if (*p == '\n') {
	    if (count > 0) {
		w = Tk_TextWidth(tsPtr->font, string, count) +
		    tsPtr->shadow.offset;
		if (w > width) {
		    width = w;
		}
	    }
	    height += lineHeight;
	    string = p + 1;	/* Start the string on the next line */
	    count = 0;		/* Reset to indicate the start of a new line */
	    continue;
	}
	count++;
    }
    if ((count > 0) && (*(p - 1) != '\n')) {
	height += lineHeight;
	w = Tk_TextWidth(tsPtr->font, string, count) + tsPtr->shadow.offset;
	if (w > width) {
	    width = w;
	}
    }
    *widthPtr = width + PADDING(tsPtr->padX);
    *heightPtr = height + PADDING(tsPtr->padY);
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_GetBoundingBox
 *
 *	Computes the dimensions of the bounding box surrounding a
 *	rectangle rotated about its center.  If pointArr isn't NULL,
 *	the coordinates of the rotated rectangle are also returned.
 *
 *	The dimensions are determined by rotating the rectangle, and
 *	doubling the maximum x-coordinate and y-coordinate.
 *
 *		w = 2 * maxX,  h = 2 * maxY
 *
 *	Since the rectangle is centered at 0,0, the coordinates of
 *	the bounding box are (-w/2,-h/2 w/2,-h/2, w/2,h/2 -w/2,h/2).
 *
 *  		0 ------- 1
 *  		|         |
 *  		|    x    |
 *  		|         |
 *  		3 ------- 2
 *
 * Results:
 *	The width and height of the bounding box containing the
 *	rotated rectangle are returned.
 *
 * -----------------------------------------------------------------
 */
void
Blt_GetBoundingBox(width, height, theta, rotWidthPtr, rotHeightPtr, bbox)
    int width, height;		/* Unrotated region */
    double theta;		/* Rotation of box */
    double *rotWidthPtr, *rotHeightPtr;	/* (out) Bounding box region */
    Point2D *bbox;		/* (out) Points of the rotated box */
{
    register int i;
    double sinTheta, cosTheta;
    double xMax, yMax;
    register double x, y;
    Point2D corner[4];

    theta = FMOD(theta, 360.0);
    if (FMOD(theta, (double)90.0) == 0.0) {
	int ll, ur, ul, lr;
	double rotWidth, rotHeight;
	int quadrant;

	/* Handle right-angle rotations specifically */

	quadrant = (int)(theta / 90.0);
	switch (quadrant) {
	case ROTATE_270:	/* 270 degrees */
	    ul = 3, ur = 0, lr = 1, ll = 2;
	    rotWidth = (double)height;
	    rotHeight = (double)width;
	    break;
	case ROTATE_90:		/* 90 degrees */
	    ul = 1, ur = 2, lr = 3, ll = 0;
	    rotWidth = (double)height;
	    rotHeight = (double)width;
	    break;
	case ROTATE_180:	/* 180 degrees */
	    ul = 2, ur = 3, lr = 0, ll = 1;
	    rotWidth = (double)width;
	    rotHeight = (double)height;
	    break;
	default:
	case ROTATE_0:		/* 0 degrees */
	    ul = 0, ur = 1, lr = 2, ll = 3;
	    rotWidth = (double)width;
	    rotHeight = (double)height;
	    break;
	}
	if (bbox != NULL) {
	    x = rotWidth * 0.5;
	    y = rotHeight * 0.5;
	    bbox[ll].x = bbox[ul].x = -x;
	    bbox[ur].y = bbox[ul].y = -y;
	    bbox[lr].x = bbox[ur].x = x;
	    bbox[ll].y = bbox[lr].y = y;
	}
	*rotWidthPtr = rotWidth;
	*rotHeightPtr = rotHeight;
	return;
    }
    /* Set the four corners of the rectangle whose center is the origin */

    corner[1].x = corner[2].x = (double)width *0.5;
    corner[0].x = corner[3].x = -corner[1].x;
    corner[2].y = corner[3].y = (double)height *0.5;
    corner[0].y = corner[1].y = -corner[2].y;

    theta = (-theta / 180.0) * M_PI;
    sinTheta = sin(theta), cosTheta = cos(theta);
    xMax = yMax = 0.0;

    /* Rotate the four corners and find the maximum X and Y coordinates */

    for (i = 0; i < 4; i++) {
	x = (corner[i].x * cosTheta) - (corner[i].y * sinTheta);
	y = (corner[i].x * sinTheta) + (corner[i].y * cosTheta);
	if (x > xMax) {
	    xMax = x;
	}
	if (y > yMax) {
	    yMax = y;
	}
	if (bbox != NULL) {
	    bbox[i].x = x;
	    bbox[i].y = y;
	}
    }

    /*
     * By symmetry, the width and height of the bounding box are
     * twice the maximum x and y coordinates.
     */
    *rotWidthPtr = xMax + xMax;
    *rotHeightPtr = yMax + yMax;
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_TranslateAnchor --
 *
 * 	Translate the coordinates of a given bounding box based
 *	upon the anchor specified.  The anchor indicates where
 *	the given xy position is in relation to the bounding box.
 *
 *  		nw --- n --- ne
 *  		|            |
 *  		w   center   e
 *  		|            |
 *  		sw --- s --- se
 *
 * 	The coordinates returned are translated to the origin of the
 * 	bounding box (suitable for giving to XCopyArea, XCopyPlane, etc.)
 *
 * Results:
 *	The translated coordinates of the bounding box are returned.
 *
 * -----------------------------------------------------------------
 */
void
Blt_TranslateAnchor(x, y, width, height, anchor, transXPtr, transYPtr)
    int x, y;			/* Window coordinates of anchor */
    int width, height;		/* Extents of the bounding box */
    Tk_Anchor anchor;		/* Direction of the anchor */
    int *transXPtr, *transYPtr;
{
    switch (anchor) {
    case TK_ANCHOR_NW:		/* Upper left corner */
	break;
    case TK_ANCHOR_W:		/* Left center */
	y -= (height / 2);
	break;
    case TK_ANCHOR_SW:		/* Lower left corner */
	y -= height;
	break;
    case TK_ANCHOR_N:		/* Top center */
	x -= (width / 2);
	break;
    case TK_ANCHOR_CENTER:	/* Center */
	x -= (width / 2);
	y -= (height / 2);
	break;
    case TK_ANCHOR_S:		/* Bottom center */
	x -= (width / 2);
	y -= height;
	break;
    case TK_ANCHOR_NE:		/* Upper right corner */
	x -= width;
	break;
    case TK_ANCHOR_E:		/* Right center */
	x -= width;
	y -= (height / 2);
	break;
    case TK_ANCHOR_SE:		/* Lower right corner */
	x -= width;
	y -= height;
	break;
    }
    *transXPtr = x;
    *transYPtr = y;
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_TranslatePoint --
 *
 * 	Translate the coordinates of a given bounding box based
 *	upon the anchor specified.  The anchor indicates where
 *	the given xy position is in relation to the bounding box.
 *
 *  		nw --- n --- ne
 *  		|            |
 *  		w   center   e
 *  		|            |
 *  		sw --- s --- se
 *
 * 	The coordinates returned are translated to the origin of the
 * 	bounding box (suitable for giving to XCopyArea, XCopyPlane, etc.)
 *
 * Results:
 *	The translated coordinates of the bounding box are returned.
 *
 * -----------------------------------------------------------------
 */
Point2D
Blt_TranslatePoint(pointPtr, width, height, anchor)
    Point2D *pointPtr;		/* Window coordinates of anchor */
    int width, height;		/* Extents of the bounding box */
    Tk_Anchor anchor;		/* Direction of the anchor */
{
    Point2D trans;

    trans = *pointPtr;
    switch (anchor) {
    case TK_ANCHOR_NW:		/* Upper left corner */
	break;
    case TK_ANCHOR_W:		/* Left center */
	trans.y -= (height * 0.5);
	break;
    case TK_ANCHOR_SW:		/* Lower left corner */
	trans.y -= height;
	break;
    case TK_ANCHOR_N:		/* Top center */
	trans.x -= (width * 0.5);
	break;
    case TK_ANCHOR_CENTER:	/* Center */
	trans.x -= (width * 0.5);
	trans.y -= (height * 0.5);
	break;
    case TK_ANCHOR_S:		/* Bottom center */
	trans.x -= (width * 0.5);
	trans.y -= height;
	break;
    case TK_ANCHOR_NE:		/* Upper right corner */
	trans.x -= width;
	break;
    case TK_ANCHOR_E:		/* Right center */
	trans.x -= width;
	trans.y -= (height * 0.5);
	break;
    case TK_ANCHOR_SE:		/* Lower right corner */
	trans.x -= width;
	trans.y -= height;
	break;
    }
    return trans;
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_CreateTextBitmap --
 *
 *	Draw a bitmap, using the the given window coordinates
 *	as an anchor for the text bounding box.
 *
 * Results:
 *	Returns the bitmap representing the text string.
 *
 * Side Effects:
 *	Bitmap is drawn using the given font and GC in the
 *	drawable at the given coordinates, anchor, and rotation.
 *
 * -----------------------------------------------------------------
 */
Pixmap
Blt_CreateTextBitmap(tkwin, textPtr, tsPtr, bmWidthPtr, bmHeightPtr)
    Tk_Window tkwin;
    TextLayout *textPtr;	/* Text string to draw */
    TextStyle *tsPtr;		/* Text attributes: rotation, color, font,
				 * linespacing, justification, etc. */
    int *bmWidthPtr;
    int *bmHeightPtr;		/* Extents of rotated text string */
{
    int width, height;
    Pixmap bitmap;
    Display *display;
    Window root;
    GC gc;
#ifdef WIN32
    HDC hDC;
    TkWinDCState state;
#endif
    display = Tk_Display(tkwin);

    width = textPtr->width;
    height = textPtr->height;

    /* Create a temporary bitmap to contain the text string */
    root = RootWindow(display, Tk_ScreenNumber(tkwin));
    bitmap = Tk_GetPixmap(display, root, width, height, 1);
    assert(bitmap != None);
    if (bitmap == None) {
	return None;		/* Can't allocate pixmap. */
    }
    /* Clear the pixmap and draw the text string into it */
    gc = Blt_GetBitmapGC(tkwin);
#ifdef WIN32
    hDC = TkWinGetDrawableDC(display, bitmap, &state);
    PatBlt(hDC, 0, 0, width, height, WHITENESS);
    TkWinReleaseDrawableDC(bitmap, hDC, &state);
#else
    XSetForeground(display, gc, 0);
    XFillRectangle(display, bitmap, gc, 0, 0, width, height);
#endif /* WIN32 */

    XSetFont(display, gc, Tk_FontId(tsPtr->font));
    XSetForeground(display, gc, 1);
    DrawTextLayout(display, bitmap, gc, tsPtr->font, 0, 0, textPtr);

#ifdef WIN32
    /*
     * Under Win32 when drawing into a bitmap, the bits are
     * reversed. Which is why we are inverting the bitmap here.
     */
    hDC = TkWinGetDrawableDC(display, bitmap, &state);
    PatBlt(hDC, 0, 0, width, height, DSTINVERT);
    TkWinReleaseDrawableDC(bitmap, hDC, &state);
#endif
    if (tsPtr->theta != 0.0) {
	Pixmap rotBitmap;

	/* Replace the text pixmap with a rotated one */

	rotBitmap = Blt_RotateBitmap(tkwin, bitmap, width, height, 
		tsPtr->theta, bmWidthPtr, bmHeightPtr);
	assert(rotBitmap);
	if (rotBitmap != None) {
	    Tk_FreePixmap(display, bitmap);
	    return rotBitmap;
	}
    }
    *bmWidthPtr = textPtr->width, *bmHeightPtr = textPtr->height;
    return bitmap;
}

/*LINTLIBRARY*/
void
Blt_InitTextStyle(tsPtr)
    TextStyle *tsPtr;
{
    /* Initialize these attributes to zero */
    tsPtr->activeColor = (XColor *)NULL;
    tsPtr->anchor = TK_ANCHOR_CENTER;
    tsPtr->color = (XColor *)NULL;
    tsPtr->font = NULL;
    tsPtr->justify = TK_JUSTIFY_CENTER;
    tsPtr->leader = 0;
    tsPtr->padLeft = tsPtr->padRight = 0;
    tsPtr->padTop = tsPtr->padBottom = 0;
    tsPtr->shadow.color = (XColor *)NULL;
    tsPtr->shadow.offset = 0;
    tsPtr->state = 0;
    tsPtr->theta = 0.0;
}

void
Blt_SetDrawTextStyle(tsPtr, font, gc, normalColor, activeColor, shadowColor, 
	theta, anchor, justify, leader, shadowOffset)
    TextStyle *tsPtr;
    Tk_Font font;
    GC gc;
    XColor *normalColor, *activeColor, *shadowColor;
    double theta;
    Tk_Anchor anchor;
    Tk_Justify justify;
    int leader, shadowOffset;
{
    Blt_InitTextStyle(tsPtr);
    tsPtr->activeColor = activeColor;
    tsPtr->anchor = anchor;
    tsPtr->color = normalColor;
    tsPtr->font = font;
    tsPtr->gc = gc;
    tsPtr->justify = justify;
    tsPtr->leader = leader;
    tsPtr->shadow.color = shadowColor;
    tsPtr->shadow.offset = shadowOffset;
    tsPtr->theta = theta;
}

void
Blt_SetPrintTextStyle(tsPtr, font, fgColor, activeColor, shadowColor, theta, 
	anchor, justify, leader, shadowOffset)
    TextStyle *tsPtr;
    Tk_Font font;
    XColor *fgColor, *activeColor, *shadowColor;
    double theta;
    Tk_Anchor anchor;
    Tk_Justify justify;
    int leader, shadowOffset;
{
    Blt_InitTextStyle(tsPtr);
    tsPtr->color = fgColor;
    tsPtr->activeColor = activeColor;
    tsPtr->shadow.color = shadowColor;
    tsPtr->font = font;
    tsPtr->theta = theta;
    tsPtr->anchor = anchor;
    tsPtr->justify = justify;
    tsPtr->leader = leader;
    tsPtr->shadow.offset = shadowOffset;
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_DrawTextLayout --
 *
 *	Draw a text string, possibly rotated, using the the given
 *	window coordinates as an anchor for the text bounding box.
 *	If the text is not rotated, simply use the X text drawing
 *	routines. Otherwise, generate a bitmap of the rotated text.
 *
 * Results:
 *	Returns the x-coordinate to the right of the text.
 *
 * Side Effects:
 *	Text string is drawn using the given font and GC at the
 *	the given window coordinates.
 *
 *      The Stipple, FillStyle, and TSOrigin fields of the GC are
 *      modified for rotated text.  This assumes the GC is private,
 *      *not* shared (via Tk_GetGC)
 *
 * -----------------------------------------------------------------
 */
void
Blt_DrawTextLayout(tkwin, drawable, textPtr, tsPtr, x, y)
    Tk_Window tkwin;
    Drawable drawable;
    TextLayout *textPtr;
    TextStyle *tsPtr;		/* Text attribute information */
    int x, y;			/* Window coordinates to draw text */
{
    int width, height;
    double theta;
    Display *display;
    Pixmap bitmap;
    int active;

    display = Tk_Display(tkwin);
    theta = FMOD(tsPtr->theta, (double)360.0);
    if (theta < 0.0) {
	theta += 360.0;
    }
    active = tsPtr->state & STATE_ACTIVE;
    if (theta == 0.0) {

	/*
	 * This is the easy case of no rotation. Simply draw the text
	 * using the standard drawing routines.  Handle offset printing
	 * for engraved (disabled) and shadowed text.
	 */
	width = textPtr->width, height = textPtr->height;
	Blt_TranslateAnchor(x, y, width, height, tsPtr->anchor, &x, &y);
	if (tsPtr->state & (STATE_DISABLED | STATE_EMPHASIS)) {
	    TkBorder *borderPtr = (TkBorder *) tsPtr->border;
	    XColor *color1, *color2;

	    color1 = borderPtr->lightColor, color2 = borderPtr->darkColor;
	    if (tsPtr->state & STATE_EMPHASIS) {
		XColor *hold;

		hold = color1, color1 = color2, color2 = hold;
	    }
	    if (color1 != NULL) {
		XSetForeground(display, tsPtr->gc, color1->pixel);
	    }
	    DrawTextLayout(display, drawable, tsPtr->gc, tsPtr->font, x + 1, 
		y + 1, textPtr);
	    if (color2 != NULL) {
		XSetForeground(display, tsPtr->gc, color2->pixel);
	    }
	    DrawTextLayout(display, drawable, tsPtr->gc, tsPtr->font, x, y, 
		textPtr);

	    /* Reset the foreground color back to its original setting,
	     * so not to invalidate the GC cache. */
	    XSetForeground(display, tsPtr->gc, tsPtr->color->pixel);

	    return;		/* Done */
	}
	if ((tsPtr->shadow.offset > 0) && (tsPtr->shadow.color != NULL)) {
	    XSetForeground(display, tsPtr->gc, tsPtr->shadow.color->pixel);
	    DrawTextLayout(display, drawable, tsPtr->gc, tsPtr->font, 
		   x + tsPtr->shadow.offset, y + tsPtr->shadow.offset, textPtr);
	    XSetForeground(display, tsPtr->gc, tsPtr->color->pixel);
	}
	if (active) {
	    XSetForeground(display, tsPtr->gc, tsPtr->activeColor->pixel);
	}
	DrawTextLayout(display, drawable, tsPtr->gc, tsPtr->font, x, y, 
		textPtr);
	if (active) {
	    XSetForeground(display, tsPtr->gc, tsPtr->color->pixel);
	}
	return;			/* Done */
    }
#ifdef WIN32
    if (Blt_DrawRotatedText(display, drawable, x, y, theta, tsPtr, textPtr)) {
	return;
    }
#endif
    /*
     * Rotate the text by writing the text into a bitmap and rotating
     * the bitmap.  Set the clip mask and origin in the GC first.  And
     * make sure we restore the GC because it may be shared.
     */
    tsPtr->theta = theta;
    bitmap = Blt_CreateTextBitmap(tkwin, textPtr, tsPtr, &width, &height);
    if (bitmap == None) {
	return;
    }
    Blt_TranslateAnchor(x, y, width, height, tsPtr->anchor, &x, &y);
#ifdef notdef
    theta = FMOD(theta, (double)90.0);
#endif
    XSetClipMask(display, tsPtr->gc, bitmap);

    if (tsPtr->state & (STATE_DISABLED | STATE_EMPHASIS)) {
	TkBorder *borderPtr = (TkBorder *) tsPtr->border;
	XColor *color1, *color2;

	color1 = borderPtr->lightColor, color2 = borderPtr->darkColor;
	if (tsPtr->state & STATE_EMPHASIS) {
	    XColor *hold;

	    hold = color1, color1 = color2, color2 = hold;
	}
	if (color1 != NULL) {
	    XSetForeground(display, tsPtr->gc, color1->pixel);
	}
	XSetClipOrigin(display, tsPtr->gc, x + 1, y + 1);
	XCopyPlane(display, bitmap, drawable, tsPtr->gc, 0, 0, width, 
		height, x + 1, y + 1, 1);
	if (color2 != NULL) {
	    XSetForeground(display, tsPtr->gc, color2->pixel);
	}
	XSetClipOrigin(display, tsPtr->gc, x, y);
	XCopyPlane(display, bitmap, drawable, tsPtr->gc, 0, 0, width, 
		height, x, y, 1);
	XSetForeground(display, tsPtr->gc, tsPtr->color->pixel);
    } else {
	if ((tsPtr->shadow.offset > 0) && (tsPtr->shadow.color != NULL)) {
	    XSetClipOrigin(display, tsPtr->gc, x + tsPtr->shadow.offset,
			   y + tsPtr->shadow.offset);
	    XSetForeground(display, tsPtr->gc, tsPtr->shadow.color->pixel);
	    XCopyPlane(display, bitmap, drawable, tsPtr->gc, 0, 0, width, 
		height, x + tsPtr->shadow.offset, y + tsPtr->shadow.offset, 1);
	    XSetForeground(display, tsPtr->gc, tsPtr->color->pixel);
	}
	if (active) {
	    XSetForeground(display, tsPtr->gc, tsPtr->activeColor->pixel);
	}
	XSetClipOrigin(display, tsPtr->gc, x, y);
	XCopyPlane(display, bitmap, drawable, tsPtr->gc, 0, 0, width, height, 
		x, y, 1);
	if (active) {
	    XSetForeground(display, tsPtr->gc, tsPtr->color->pixel);
	}
    }
    XSetClipMask(display, tsPtr->gc, None);
    Tk_FreePixmap(display, bitmap);
}

void
Blt_DrawText2(tkwin, drawable, string, tsPtr, x, y, areaPtr)
    Tk_Window tkwin;
    Drawable drawable;
    char string[];
    TextStyle *tsPtr;		/* Text attribute information */
    int x, y;			/* Window coordinates to draw text */
    Dim2D *areaPtr;
{
    TextLayout *textPtr;
    int width, height;
    double theta;

    if ((string == NULL) || (*string == '\0')) {
	return;			/* Empty string, do nothing */
    }
    textPtr = Blt_GetTextLayout(string, tsPtr);
    Blt_DrawTextLayout(tkwin, drawable, textPtr, tsPtr, x, y);
    theta = FMOD(tsPtr->theta, (double)360.0);
    if (theta < 0.0) {
	theta += 360.0;
    }
    width = textPtr->width;
    height = textPtr->height;
    if (theta != 0.0) {
	double rotWidth, rotHeight;

	Blt_GetBoundingBox(width, height, theta, &rotWidth, &rotHeight, 
	   (Point2D *)NULL);
	width = ROUND(rotWidth);
	height = ROUND(rotHeight);
    }
    areaPtr->width = width;
    areaPtr->height = height;
    Blt_Free(textPtr);
}

void
Blt_DrawText(tkwin, drawable, string, tsPtr, x, y)
    Tk_Window tkwin;
    Drawable drawable;
    char string[];
    TextStyle *tsPtr;		/* Text attribute information */
    int x, y;			/* Window coordinates to draw text */
{
    TextLayout *textPtr;

    if ((string == NULL) || (*string == '\0')) {
	return;			/* Empty string, do nothing */
    }
    textPtr = Blt_GetTextLayout(string, tsPtr);
    Blt_DrawTextLayout(tkwin, drawable, textPtr, tsPtr, x, y);
    Blt_Free(textPtr);
}

GC
Blt_GetBitmapGC(tkwin)
    Tk_Window tkwin;
{
    int isNew;
    GC gc;
    Display *display;
    Blt_HashEntry *hPtr;

    if (!initialized) {
	Blt_InitHashTable(&bitmapGCTable, BLT_ONE_WORD_KEYS);
	initialized = TRUE;
    }
    display = Tk_Display(tkwin);
    hPtr = Blt_CreateHashEntry(&bitmapGCTable, (char *)display, &isNew);
    if (isNew) {
	Pixmap bitmap;
	XGCValues gcValues;
	unsigned long gcMask;
	Window root;

	root = RootWindow(display, Tk_ScreenNumber(tkwin));
	bitmap = Tk_GetPixmap(display, root, 1, 1, 1);
	gcValues.foreground = gcValues.background = 0;
	gcMask = (GCForeground | GCBackground);
	gc = Blt_GetPrivateGCFromDrawable(display, bitmap, gcMask, &gcValues);
	Tk_FreePixmap(display, bitmap);
	Blt_SetHashValue(hPtr, gc);
    } else {
	gc = (GC)Blt_GetHashValue(hPtr);
    }
    return gc;
}

void
Blt_ResetTextStyle(tkwin, tsPtr)
    Tk_Window tkwin;
    TextStyle *tsPtr;
{
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;

    gcMask = GCFont;
    gcValues.font = Tk_FontId(tsPtr->font);
    if (tsPtr->color != NULL) {
	gcMask |= GCForeground;
	gcValues.foreground = tsPtr->color->pixel;
    }
    newGC = Tk_GetGC(tkwin, gcMask, &gcValues);
    if (tsPtr->gc != NULL) {
	Tk_FreeGC(Tk_Display(tkwin), tsPtr->gc);
    }
    tsPtr->gc = newGC;
}

void
Blt_FreeTextStyle(display, tsPtr)
    Display *display;
    TextStyle *tsPtr;
{
    if (tsPtr->gc != NULL) {
	Tk_FreeGC(display, tsPtr->gc);
    }
}
