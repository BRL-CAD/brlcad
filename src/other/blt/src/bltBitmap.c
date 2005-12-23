
/*
 * bltBitmap.c --
 *
 *	This module implements Tcl bitmaps for the Tk toolkit.
 *
 *	Much of the code is taken from XRdBitF.c and XWrBitF.c
 *	from the MIT X11R5 distribution.
 *
 * Copyright, 1987, Massachusetts Institute of Technology Permission
 * to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of M.I.T. not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
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
 *
 * The "bitmap" command created by George Howlett.  */

/*
  Predefined table holds bitmap info (source width, height)
  Name table holds bitmap names  
  Id table hold bitmap ids
  Both id and name tables get you the actual bitmap.
 */
#include "bltInt.h"

#ifndef NO_BITMAP
#include "bltHash.h"
#include <X11/Xutil.h>

#define BITMAP_THREAD_KEY	"BLT Bitmap Data"

/* 
 * BitmapInterpData --
 *
 *	Tk's routine to create a bitmap, Tk_DefineBitmap, assumes that
 *	the source (bit array) is always statically allocated.  This
 *	isn't true here (we dynamically allocate the arrays), so we have 
 *	to save them in a hashtable and cleanup after the interpreter 
 *	is deleted.
 */
typedef struct {
    Blt_HashTable bitmapTable;	/* Hash table of bitmap data keyed by 
				 * the name of the bitmap. */
    Tcl_Interp *interp;
    Display *display;		/* Display of interpreter. */
    Tk_Window tkwin;		/* Main window of interpreter. */
} BitmapInterpData;

#define MAX_SIZE 255

/* 
 * BitmapInfo --
 */
typedef struct {
    double rotate;		/* Rotation of text string */
    double scale;		/* Scaling factor */
    Tk_Font font;		/* Font pointer */
    Tk_Justify justify;		/* Justify text */
    Blt_Pad padX, padY;		/* Padding around the text */
} BitmapInfo;

/* 
 * BitmapData --
 */
typedef struct {
    int width, height;		/* Dimension of image */
    unsigned char *bits;	/* Data array for bitmap image */
    int arraySize;		/* Number of bytes in data array */
} BitmapData;

#define DEF_BITMAP_FONT		STD_FONT
#define DEF_BITMAP_PAD		"4"
#define DEF_BITMAP_ROTATE	"0.0"
#define DEF_BITMAP_SCALE	"1.0"
#define DEF_BITMAP_JUSTIFY	"center"

#define ROTATE_0	0
#define ROTATE_90	1
#define ROTATE_180	2
#define ROTATE_270	3


extern Tk_CustomOption bltPadOption;

static Tk_ConfigSpec composeConfigSpecs[] =
{
    {TK_CONFIG_FONT, "-font", (char *)NULL, (char *)NULL,
	DEF_BITMAP_FONT, Tk_Offset(BitmapInfo, font), 0},
    {TK_CONFIG_JUSTIFY, "-justify", (char *)NULL, (char *)NULL,
	DEF_BITMAP_JUSTIFY, Tk_Offset(BitmapInfo, justify),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-padx", (char *)NULL, (char *)NULL,
	DEF_BITMAP_PAD, Tk_Offset(BitmapInfo, padX),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-pady", (char *)NULL, (char *)NULL,
	DEF_BITMAP_PAD, Tk_Offset(BitmapInfo, padY),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_DOUBLE, "-rotate", (char *)NULL, (char *)NULL,
	DEF_BITMAP_ROTATE, Tk_Offset(BitmapInfo, rotate),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_DOUBLE, "-scale", (char *)NULL, (char *)NULL,
	DEF_BITMAP_SCALE, Tk_Offset(BitmapInfo, scale),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

static Tk_ConfigSpec defineConfigSpecs[] =
{
    {TK_CONFIG_DOUBLE, "-rotate", (char *)NULL, (char *)NULL,
	DEF_BITMAP_ROTATE, Tk_Offset(BitmapInfo, rotate),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_DOUBLE, "-scale", (char *)NULL, (char *)NULL,
	DEF_BITMAP_SCALE, Tk_Offset(BitmapInfo, scale),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

/* Shared data for the image read/parse logic */
static char hexTable[256];	/* conversion value */
static int initialized = 0;	/* easier to fill in at run time */

#define blt_width 40
#define blt_height 40
static unsigned char blt_bits[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x03, 0x00, 0x04,
    0x00, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x02, 0x00, 0xe4, 0x33, 0x3f,
    0x01, 0x00, 0x64, 0x36, 0x0c, 0x01, 0x00, 0x64, 0x36, 0x8c, 0x00, 0x00,
    0xe4, 0x33, 0x8c, 0x00, 0x00, 0x64, 0x36, 0x8c, 0x00, 0x00, 0x64, 0x36,
    0x0c, 0x01, 0x00, 0xe4, 0xf3, 0x0d, 0x01, 0x00, 0x04, 0x00, 0x00, 0x02,
    0x00, 0x04, 0x00, 0x00, 0x02, 0x00, 0xfc, 0xff, 0xff, 0x03, 0x00, 0x0c,
    0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x0c, 0xf8, 0xff,
    0x03, 0x80, 0xed, 0x07, 0x00, 0x04, 0xe0, 0x0c, 0x00, 0x20, 0x09, 0x10,
    0x0c, 0x00, 0x00, 0x12, 0x10, 0x0c, 0x00, 0x00, 0x10, 0x30, 0x00, 0x00,
    0x00, 0x19, 0xd0, 0x03, 0x00, 0x00, 0x14, 0xb0, 0xfe, 0xff, 0xff, 0x1b,
    0x50, 0x55, 0x55, 0x55, 0x0d, 0xe8, 0xaa, 0xaa, 0xaa, 0x16, 0xe4, 0xff,
    0xff, 0xff, 0x2f, 0xf4, 0xff, 0xff, 0xff, 0x27, 0xd8, 0xae, 0xaa, 0xbd,
    0x2d, 0x6c, 0x5f, 0xd5, 0x67, 0x1b, 0xbc, 0xf3, 0x7f, 0xd0, 0x36, 0xf8,
    0x01, 0x10, 0xcc, 0x1f, 0xe0, 0x45, 0x8e, 0x92, 0x0f, 0xb0, 0x32, 0x41,
    0x43, 0x0b, 0xd0, 0xcf, 0x3c, 0x7c, 0x0d, 0xb0, 0xaa, 0xc2, 0xab, 0x0a,
    0x60, 0x55, 0x55, 0x55, 0x05, 0xc0, 0xff, 0xab, 0xaa, 0x03, 0x00, 0x00,
    0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#define bigblt_width 64
#define bigblt_height 64
static unsigned char bigblt_bits[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0x3f, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x00, 0x00, 0xe2, 0x0f, 0xc7, 0xff, 0x10, 0x00, 0x00, 0x00, 0xe2, 0x1f,
    0xc7, 0xff, 0x10, 0x00, 0x00, 0x00, 0xe2, 0x38, 0x07, 0x1c, 0x08, 0x00,
    0x00, 0x00, 0xe2, 0x38, 0x07, 0x1c, 0x08, 0x00, 0x00, 0x00, 0xe2, 0x38,
    0x07, 0x1c, 0x08, 0x00, 0x00, 0x00, 0xe2, 0x1f, 0x07, 0x1c, 0x04, 0x00,
    0x00, 0x00, 0xe2, 0x1f, 0x07, 0x1c, 0x04, 0x00, 0x00, 0x00, 0xe2, 0x38,
    0x07, 0x1c, 0x08, 0x00, 0x00, 0x00, 0xe2, 0x38, 0x07, 0x1c, 0x08, 0x00,
    0x00, 0x00, 0xe2, 0x38, 0x07, 0x1c, 0x08, 0x00, 0x00, 0x00, 0xe2, 0x1f,
    0xff, 0x1c, 0x10, 0x00, 0x00, 0x00, 0xe2, 0x0f, 0xff, 0x1c, 0x10, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00,
    0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x06, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0xc0, 0xff, 0xff, 0x07, 0x00,
    0x00, 0xe0, 0xf6, 0x3f, 0x00, 0x00, 0x38, 0x00, 0x00, 0x1c, 0x06, 0x00,
    0x00, 0x00, 0xc0, 0x00, 0x80, 0x03, 0x06, 0x00, 0x00, 0xc0, 0x08, 0x03,
    0x40, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x04, 0x40, 0x00, 0x06, 0x00,
    0x00, 0x00, 0x40, 0x04, 0x40, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x04,
    0x40, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x04, 0xc0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0c, 0x06, 0x40, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0xc0, 0xfe, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x06, 0x40, 0x55, 0xff, 0xff,
    0xff, 0xff, 0x7f, 0x05, 0x80, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0x06,
    0x80, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x03, 0x40, 0xab, 0xaa, 0xaa,
    0xaa, 0xaa, 0xaa, 0x01, 0x70, 0x57, 0x55, 0x55, 0x55, 0x55, 0xd5, 0x04,
    0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0b, 0xd8, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x14, 0xd0, 0xf7, 0xff, 0xff, 0xff, 0xff, 0xff, 0x13,
    0xf0, 0xda, 0xbf, 0xaa, 0xba, 0xfd, 0xd6, 0x0b, 0x70, 0xed, 0x77, 0x55,
    0x57, 0xe5, 0xad, 0x07, 0xb8, 0xf7, 0xab, 0xaa, 0xaa, 0xd2, 0x5b, 0x0f,
    0xf8, 0xfb, 0x54, 0x55, 0x75, 0x94, 0xf7, 0x1e, 0xf0, 0x7b, 0xfa, 0xff,
    0x9f, 0xa9, 0xef, 0x1f, 0xc0, 0xbf, 0x00, 0x20, 0x40, 0x54, 0xfe, 0x0f,
    0x00, 0x1f, 0x92, 0x00, 0x04, 0xa9, 0xfc, 0x01, 0xc0, 0x5f, 0x41, 0xf9,
    0x04, 0x21, 0xfd, 0x00, 0xc0, 0x9b, 0x28, 0x04, 0xd8, 0x0a, 0x9a, 0x03,
    0x40, 0x5d, 0x08, 0x40, 0x44, 0x44, 0x62, 0x03, 0xc0, 0xaa, 0x67, 0xe2,
    0x03, 0x64, 0xba, 0x02, 0x40, 0x55, 0xd5, 0x55, 0xfd, 0xdb, 0x55, 0x03,
    0x80, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0x01, 0x00, 0x57, 0x55, 0x55,
    0x55, 0x55, 0xd5, 0x00, 0x00, 0xac, 0xaa, 0xaa, 0xaa, 0xaa, 0x2a, 0x00,
    0x00, 0xf0, 0xff, 0x57, 0x55, 0x55, 0x1d, 0x00, 0x00, 0x00, 0x00, 0xf8,
    0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static Tcl_CmdProc BitmapCmd;
static Tcl_InterpDeleteProc BitmapInterpDeleteProc;

/*
 * -----------------------------------------------------------------------
 *
 * GetHexValue --
 *
 *	Converts the hexadecimal string into an unsigned integer
 *	value.  The hexadecimal string need not have a leading "0x".
 *
 * Results:
 *	Returns a standard TCL result. If the conversion was
 *	successful, TCL_OK is returned, otherwise TCL_ERROR.
 *
 * Side Effects:
 * 	If the conversion fails, interp->result is filled with an
 *	error message.
 *
 * -----------------------------------------------------------------------
 */
static int
GetHexValue(interp, string, valuePtr)
    Tcl_Interp *interp;
    char *string;
    int *valuePtr;
{
    register int c;
    register char *s;
    register int value;

    s = string;
    if ((s[0] == '0') && ((s[1] == 'x') || (s[1] == 'X'))) {
	s += 2;
    }
    if (s[0] == '\0') {
	Tcl_AppendResult(interp, "expecting hex value: got \"", string, "\"",
	    (char *)NULL);
	return TCL_ERROR;	/* Only found "0x"  */
    }
    value = 0;
    for ( /*empty*/ ; *s != '\0'; s++) {
	/* Trim high bits, check type and accumulate */
	c = *s & 0xff;
	if (!isxdigit(c)) {
	    Tcl_AppendResult(interp, "expecting hex value: got \"", string,
		"\"", (char *)NULL);
	    return TCL_ERROR;	/* Not a hexadecimal number */
	}
	value = (value << 4) + hexTable[c];
    }
    *valuePtr = value;
    return TCL_OK;
}

#ifdef WIN32
/*
 * -----------------------------------------------------------------------
 *
 * BitmapToData --
 *
 *	Converts a bitmap into an data array.
 *
 * Results:
 *	Returns the number of bytes in an data array representing the bitmap.
 *
 * Side Effects:
 *	Memory is allocated for the data array. Caller must free
 *	array later.
 *
 * -----------------------------------------------------------------------
 */
static int
BitmapToData(
    Tk_Window tkwin,		/* Main window of interpreter */
    Pixmap bitmap,		/* Bitmap to be queried */
    int width, int height,	/* Dimensions of the bitmap */
    unsigned char **bitsPtr)	/* Pointer to converted array of data */
{
    int value, bitMask;
    unsigned long pixel;
    register int x, y;
    int count;
    int arraySize, bytes_per_line;
    unsigned char *bits;
    unsigned char *srcPtr, *srcBits;
    int bytesPerRow;

    *bitsPtr = NULL;
    srcBits = Blt_GetBitmapData(Tk_Display(tkwin), bitmap, width, height,
	&bytesPerRow);
    if (srcBits == NULL) {
        OutputDebugString("BitmapToData: Can't get bitmap data");
	return 0;
    }
    bytes_per_line = (width + 7) / 8;
    arraySize = height * bytes_per_line;
    bits = Blt_Malloc(sizeof(unsigned char) * arraySize);
    assert(bits);
    count = 0;
    for (y = height - 1; y >= 0; y--) {
	srcPtr = srcBits + (bytesPerRow * y);
	value = 0, bitMask = 1;
	for (x = 0; x < width; /* empty */ ) {
	    pixel = (*srcPtr & (0x80 >> (x % 8)));
	    if (pixel) {
		value |= bitMask;
	    }
	    bitMask <<= 1;
	    x++;
	    if (!(x & 7)) {
		bits[count++] = (unsigned char)value;
		value = 0, bitMask = 1;
		srcPtr++;
	    }
	}
	if (x & 7) {
	    bits[count++] = (unsigned char)value;
	}
    }
    *bitsPtr = bits;
    return count;
}

#else

/*
 * -----------------------------------------------------------------------
 *
 * BitmapToData --
 *
 *	Converts a bitmap into an data array.
 *
 * Results:
 *	Returns the number of bytes in an data array representing the bitmap.
 *
 * Side Effects:
 *	Memory is allocated for the data array. Caller must free
 *	array later.
 *
 * -----------------------------------------------------------------------
 */
static int
BitmapToData(tkwin, bitmap, width, height, bitsPtr)
    Tk_Window tkwin;		/* Main window of interpreter */
    Pixmap bitmap;		/* Bitmap to be queried */
    int width, height;		/* Dimensions of the bitmap */
    unsigned char **bitsPtr;	/* Pointer to converted array of data */
{
    int value, bitMask;
    unsigned long pixel;
    register int x, y;
    int count;
    int arraySize, bytes_per_line;
    Display *display;
    XImage *imagePtr;
    unsigned char *bits;

    display = Tk_Display(tkwin);
    /* Convert the bitmap to an image */
    imagePtr = XGetImage(display, bitmap, 0, 0, width, height, 1L, XYPixmap);
    /*
     * The slow but robust brute force method of converting an image:
     */
    bytes_per_line = (width + 7) / 8;
    arraySize = height * bytes_per_line;
    bits = Blt_Malloc(sizeof(unsigned char) * arraySize);
    assert(bits);
    count = 0;
    for (y = 0; y < height; y++) {
	value = 0, bitMask = 1;
	for (x = 0; x < width; /*empty*/ ) {
	    pixel = XGetPixel(imagePtr, x, y);
	    if (pixel) {
		value |= bitMask;
	    }
	    bitMask <<= 1;
	    x++;
	    if (!(x & 7)) {
		bits[count++] = (unsigned char)value;
		value = 0, bitMask = 1;
	    }
	}
	if (x & 7) {
	    bits[count++] = (unsigned char)value;
	}
    }
    XDestroyImage(imagePtr);
    *bitsPtr = bits;
    return count;
}

#endif

/*
 * -----------------------------------------------------------------------
 *
 * AsciiToData --
 *
 *	Converts a Tcl list of ASCII values into a data array.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side Effects:
 * 	If an error occurs while processing the data, interp->result
 * 	is filled with a corresponding error message.
 *
 * -----------------------------------------------------------------------
 */
static int
AsciiToData(interp, elemList, width, height, bitsPtr)
    Tcl_Interp *interp;		/* Interpreter to report results to */
    char *elemList;		/* List of of hex numbers representing
				 * bitmap data */
    int width, height;		/* Height and width */
    unsigned char **bitsPtr;	/* data array (output) */
{
    int arraySize;		/* Number of bytes of data */
    int value;			/* from an input line */
    int padding;		/* to handle alignment */
    int bytesPerLine;		/* per scanline of data */
    unsigned char *bits;
    register int count;
    enum Formats {
	V10, V11
    } format;
    register int i;		/*  */
    char **valueArr;
    int nValues;

    /* First time through initialize the ascii->hex translation table */
    if (!initialized) {
	Blt_InitHexTable(hexTable);
	initialized = 1;
    }
    if (Tcl_SplitList(interp, elemList, &nValues, &valueArr) != TCL_OK) {
	return -1;
    }
    bytesPerLine = (width + 7) / 8;
    arraySize = bytesPerLine * height;
    if (nValues == arraySize) {
	format = V11;
    } else if (nValues == (arraySize / 2)) {
	format = V10;
    } else {
	Tcl_AppendResult(interp, "bitmap has wrong # of data values",
	    (char *)NULL);
	goto error;
    }
    padding = 0;
    if (format == V10) {
	padding = ((width % 16) && ((width % 16) < 9));
	if (padding) {
	    bytesPerLine = (width + 7) / 8 + padding;
	    arraySize = bytesPerLine * height;
	}
    }
    bits = Blt_Calloc(sizeof(unsigned char), arraySize);
    if (bits == NULL) {
	Tcl_AppendResult(interp, "can't allocate memory for bitmap",
	    (char *)NULL);
	goto error;
    }
    count = 0;
    for (i = 0; i < nValues; i++) {
	if (GetHexValue(interp, valueArr[i], &value) != TCL_OK) {
	    Blt_Free(bits);
	    goto error;
	}
	bits[count++] = (unsigned char)value;
	if (format == V10) {
	    if ((!padding) || (((i * 2) + 2) % bytesPerLine)) {
		bits[count++] = value >> 8;
	    }
	}
    }
    Blt_Free(valueArr);
    *bitsPtr = bits;
    return count;
  error:
    Blt_Free(valueArr);
    return -1;
}


static int
ParseListData(interp, string, widthPtr, heightPtr, bitsPtr)
    Tcl_Interp *interp;
    char *string;
    int *widthPtr;
    int *heightPtr;
    unsigned char **bitsPtr;
{
    register char *p;
    char **elemArr;
    int nElem;
    int width, height;
    int result;
    int arraySize;

    arraySize = -1;
    if (Tcl_SplitList(interp, string, &nElem, &elemArr) != TCL_OK) {
	return -1;
    }
    if (nElem == 2) {
	char **dimArr;
	int nDim;
	
	if (Tcl_SplitList(interp, elemArr[0], &nDim, &dimArr) != TCL_OK) {
	    goto error;
	}
	if (nDim != 2) {
	    Tcl_AppendResult(interp, "wrong # of bitmap dimensions: ",
			     "should be \"width height\"", (char *)NULL);
	    result = TCL_ERROR;
	} else {
	    result = ((Tcl_GetInt(interp, dimArr[0], &width) == TCL_OK) &&
		      (Tcl_GetInt(interp, dimArr[1], &height) == TCL_OK));
	}
	Blt_Free(dimArr);
	if (!result) {
	    goto error;
	}
	string = elemArr[1];
    } else if (nElem == 3) {
	if ((Tcl_GetInt(interp, elemArr[0], &width) != TCL_OK) ||
	    (Tcl_GetInt(interp, elemArr[1], &height) != TCL_OK)) {
	    goto error;
	}
	string = elemArr[2];
    } else {
	Tcl_AppendResult(interp, "wrong # of bitmap data components: ",
			 "should be \"dimensions sourceData\"", (char *)NULL);
	goto error;
    }
    if ((width < 1) || (height < 1)) {
	Tcl_AppendResult(interp, "bad bitmap dimensions", (char *)NULL);
	goto error;
    }
    /* Convert commas to blank spaces */
    
    for (p = string; *p != '\0'; p++) {
	if (*p == ',') {
	    *p = ' ';
	}
    }
    arraySize = AsciiToData(interp, string, width, height, bitsPtr);
    *widthPtr = width;
    *heightPtr = height;
 error:
    Blt_Free(elemArr);
    return arraySize;
}

/*
 * Parse the lines that define the dimensions of the bitmap,
 * plus the first line that defines the bitmap data (it declares
 * the name of a data variable but doesn't include any actual
 * data).  These lines look something like the following:
 *
 *		#define foo_width 16
 *		#define foo_height 16
 *		#define foo_x_hot 3
 *		#define foo_y_hot 3
 *		static char foo_bits[] = {
 *
 * The x_hot and y_hot lines may or may not be present.  It's
 * important to check for "char" in the last line, in order to
 * reject old X10-style bitmaps that used shorts.
 */

static int
ParseStructData(interp, string, widthPtr, heightPtr, bitsPtr)
    Tcl_Interp *interp;
    char *string;
    int *widthPtr;
    int *heightPtr;
    unsigned char **bitsPtr;
{
    int width, height;
    int hotX, hotY;
    char *line, *nextline;
    register char *p;
    Tcl_RegExp re;
    char *name, *value, *data;
    int len;
    int arraySize;

    width = height = 0;
    hotX = hotY = -1;
    data = NULL;
    nextline = string;
    for (line = string; nextline != NULL; line = nextline + 1) {
	nextline = strchr(line, '\n');
	if ((nextline == NULL) || (line == nextline)) {
	    continue;		/* Empty line */
	}
	*nextline = '\0';
	re = Tcl_RegExpCompile(interp, " *# *define +");
	if (Tcl_RegExpExec(interp, re, line, line)) {
	    char *start, *end;

	    Tcl_RegExpRange(re, 0, &start, &end);
	    name = strtok(end, " \t"); 
	    value = strtok(NULL, " \t");
	    if ((name == NULL) || (value == NULL)) {
		return TCL_ERROR;
	    }
	    len = strlen(name);
	    if ((len >= 6) && (name[len-6] == '_') && 
		(strcmp(name+len-6, "_width") == 0)) {
		if (Tcl_GetInt(interp, value, &width) != TCL_OK) {
		    return -1;
		}
	    } else if ((len >= 7) && (name[len-7] == '_') && 
		       (strcmp(name+len-7, "_height") == 0)) {
		if (Tcl_GetInt(interp, value, &height) != TCL_OK) {
		    return -1;
		}
	    } else if ((len >= 6) && (name[len-6] == '_') && 
		       (strcmp(name+len-6, "_x_hot") == 0)) {
		if (Tcl_GetInt(interp, value, &hotX) != TCL_OK) {
		    return -1;
		}
	    } else if ((len >= 6) && (name[len-6] == '_') && 
		       (strcmp(name+len-6, "_y_hot") == 0)) {
		if (Tcl_GetInt(interp, value, &hotY) != TCL_OK) {
		    return -1;
		}
	    } 
	} else {
	    re = Tcl_RegExpCompile(interp, " *static +.*char +");
	    if (Tcl_RegExpExec(interp, re, line, line)) {
		/* Find the { */
	        /* Repair the string so we can search the entire string. */
 	        *nextline = ' ';   
		p = strchr(line, '{');
		if (p == NULL) {
		    return -1;
		}
		data = p + 1;
		break;
	    } else {
		Tcl_AppendResult(interp, "unknown bitmap format: ",
		 "obsolete X10 bitmap file?", (char *) NULL);
		return -1;
	    }
	}
    }
    /*
     * Now we've read everything but the data.  Allocate an array
     * and read in the data.
     */
    if ((width <= 0) || (height <= 0)) {
	Tcl_AppendResult(interp, "invalid bitmap dimensions \"", (char *)NULL);
	Tcl_AppendResult(interp, Blt_Itoa(width), " x ", (char *)NULL);
	Tcl_AppendResult(interp, Blt_Itoa(height), "\"", (char *)NULL);
	return -1;
    }
    *widthPtr = width;
    *heightPtr = height;
    for (p = data; *p != '\0'; p++) {
	if ((*p == ',') || (*p == ';') || (*p == '}')) {
	    *p = ' ';
	}
    }
    arraySize = AsciiToData(interp, data, width, height, bitsPtr);
    return arraySize;
}

/*
 * -----------------------------------------------------------------------
 *
 * ScaleRotateData --
 *
 *	Creates a new data array of the rotated and scaled bitmap.
 *
 * Results:
 *	A standard Tcl result. If the bitmap data is rotated
 *	successfully, TCL_OK is returned.  But if memory could not be
 *	allocated for the new data array, TCL_ERROR is returned and an
 *	error message is left in interp->result.
 *
 * Side Effects:
 *	Memory is allocated for rotated, scaled data array. Caller
 *	must free array later.
 *
 * -----------------------------------------------------------------------
 */
static int
ScaleRotateData(
    Tcl_Interp *interp,		/* Interpreter to report results to */
    BitmapData *srcPtr,		/* Source bitmap to transform. */
    double theta,		/* Number of degrees to rotate the bitmap. */
    double scale,		/* Factor to scale the bitmap. */
    BitmapData *destPtr)	/* Destination bitmap. */
{
    register int x, y, sx, sy;
    double srcX, srcY, destX, destY;	/* Origins of source and destination
					 * bitmaps */
    double dx, dy;
    double sinTheta, cosTheta;
    double rotWidth, rotHeight;
    double radians;
    unsigned char *bits;
    int arraySize;
    int pixel, ipixel;
    int srcBytesPerLine, destBytesPerLine;

    srcBytesPerLine = (srcPtr->width + 7) / 8;
    Blt_GetBoundingBox(srcPtr->width, srcPtr->height, theta, &rotWidth, 
	&rotHeight, (Point2D *)NULL);
    destPtr->width = (int)(rotWidth * scale + 0.5) ;
    destPtr->height = (int)(rotHeight * scale + 0.5);

    destBytesPerLine = (destPtr->width + 7) / 8;
    arraySize = destPtr->height * destBytesPerLine;
    bits = Blt_Calloc(arraySize, sizeof(unsigned char));
    if (bits == NULL) {
	Tcl_AppendResult(interp, "can't allocate bitmap data array",
	    (char *)NULL);
	return TCL_ERROR;
    }
    scale = 1.0 / scale;
    destPtr->bits = bits;
    destPtr->arraySize = arraySize;

    radians = (theta / 180.0) * M_PI;
    sinTheta = sin(radians);
    cosTheta = cos(radians);

    /*
     * Coordinates of the centers of the source and destination rectangles
     */
    srcX = srcPtr->width * 0.5;
    srcY = srcPtr->height * 0.5;
    destX = rotWidth * 0.5;
    destY = rotHeight * 0.5;

    /*
     * Rotate each pixel of dest image, placing results in source image
     */
    for (y = 0; y < destPtr->height; y++) {
	for (x = 0; x < destPtr->width; x++) {
	    dx = scale * (double)x;
	    dy = scale * (double)y;
	    if (theta == 270.0) {
		sx = (int)dy, sy = (int)(rotWidth - dx) - 1;
	    } else if (theta == 180.0) {
		sx = (int)(rotWidth - dx) - 1, sy = (int)(rotHeight - dy) - 1;
	    } else if (theta == 90.0) {
		sx = (int)(rotHeight - dy) - 1, sy = (int)dx;
	    } else if (theta == 0.0) {
		sx = (int)dx, sy = (int)dy;
	    } else {
		double transX, transY, rotX, rotY;
		/* Translate origin to center of destination image */

		transX = dx - destX;
		transY = dy - destY;

		/* Rotate the coordinates about the origin */

		rotX = (transX * cosTheta) - (transY * sinTheta);
		rotY = (transX * sinTheta) + (transY * cosTheta);

		/* Translate back to the center of the source image */
		rotX += srcX;
		rotY += srcY;

		sx = ROUND(rotX);
		sy = ROUND(rotY);

		/*
		 * Verify the coordinates, since the destination image
		 * can be bigger than the source.
		 */

		if ((sx >= srcPtr->width) || (sx < 0) ||
		    (sy >= srcPtr->height) || (sy < 0)) {
		    continue;
		}
	    }
	    ipixel = (srcBytesPerLine * sy) + (sx / 8);
	    pixel = srcPtr->bits[ipixel] & (1 << (sx % 8));
	    if (pixel) {
		ipixel = (destBytesPerLine * y) + (x / 8);
		bits[ipixel] |= (1 << (x % 8));
	    }
	}
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * BitmapDataToString --
 *
 *	Returns a list of hex values corresponding to the data
 *	bits of the bitmap given.
 *
 *	Converts the unsigned character value into a two character
 *	hexadecimal string.  A separator is also added, which may
 *	either a newline or space according the the number of bytes
 *	already output.
 *
 * Results:
 *	Returns TCL_ERROR if a data array can't be generated
 *	from the bitmap (memory allocation failure), otherwise TCL_OK.
 *
 * -----------------------------------------------------------------------
 */
static void
BitmapDataToString(tkwin, bitmap, resultPtr)
    Tk_Window tkwin;		/* Main window of interpreter */
    Pixmap bitmap;		/* Bitmap to be queried */
    Tcl_DString *resultPtr;	/* Dynamic string to output results to */
{
    unsigned char *bits;
    char *separator;
    int arraySize;
    register int i;
    char string[200];
    int width, height;

    /* Get the dimensions of the bitmap */
    Tk_SizeOfBitmap(Tk_Display(tkwin), bitmap, &width, &height);
    arraySize = BitmapToData(tkwin, bitmap, width, height, &bits);
#define BYTES_PER_OUTPUT_LINE 24
    for (i = 0; i < arraySize; i++) {
	separator = (i % BYTES_PER_OUTPUT_LINE) ? " " : "\n    ";
	sprintf(string, "%s%02x", separator, bits[i]);
	Tcl_DStringAppend(resultPtr, string, -1);
    }
    if (bits != NULL) {
        Blt_Free(bits);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ComposeOp --
 *
 *	Converts the text string into an internal bitmap.
 *
 *	There's a lot of extra (read unnecessary) work going on here,
 *	but I don't (right now) think that it matters much.  The
 *	rotated bitmap (formerly an image) is converted back to an
 *	image just so we can convert it to a data array for
 *	Tk_DefineBitmap.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side Effects:
 * 	If an error occurs while processing the data, interp->result
 * 	is filled with a corresponding error message.
 *
 *--------------------------------------------------------------
 */
static int
ComposeOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data for bitmaps. */
    Tcl_Interp *interp;		/* Interpreter to report results to */
    int argc;			/* Number of arguments */
    char **argv;		/* Argument list */
{
    BitmapInterpData *dataPtr = clientData;
    int width, height;		/* Dimensions of bitmap */
    Pixmap bitmap;		/* Text bitmap */
    unsigned char *bits;	/* Data array derived from text bitmap */
    int arraySize;
    BitmapInfo info;		/* Text rotation and font information */
    int result;
    double theta;
    TextStyle ts;
    TextLayout *textPtr;
    Tk_Window tkwin;		/* Main window of interpreter */
    Blt_HashEntry *hPtr;
    int isNew;

    tkwin = dataPtr->tkwin;
    bitmap = Tk_GetBitmap(interp, tkwin, Tk_GetUid(argv[2]));
    Tcl_ResetResult(interp);
    if (bitmap != None) {
	Tk_FreeBitmap(dataPtr->display, bitmap);
	return TCL_OK;
    }
    /* Initialize info and process flags */
    info.justify = TK_JUSTIFY_CENTER;
    info.rotate = 0.0;		/* No rotation or scaling by default */
    info.scale = 1.0;
    info.padLeft = info.padRight = 0;
    info.padTop = info.padBottom = 0;
    info.font = (Tk_Font)NULL;	/* Initialized by Tk_ConfigureWidget */
    if (Tk_ConfigureWidget(interp, tkwin, composeConfigSpecs,
	    argc - 4, argv + 4, (char *)&info, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    theta = FMOD(info.rotate, (double)360.0);
    if (theta < 0.0) {
	theta += 360.0;
    }
    Blt_InitTextStyle(&ts);
    ts.font = info.font;
    ts.theta = 0.0;
    ts.justify = info.justify;
    ts.padX = info.padX;
    ts.padY = info.padY;
    ts.leader = 0;
    ts.anchor = TK_ANCHOR_CENTER;

    textPtr = Blt_GetTextLayout(argv[3], &ts);
    bitmap = Blt_CreateTextBitmap(tkwin, textPtr, &ts, &width, &height);
    Blt_Free(textPtr);
    if (bitmap == None) {
	Tcl_AppendResult(interp, "can't create bitmap", (char *)NULL);
	return TCL_ERROR;
    }
    /* Free the font structure, since we don't need it anymore */
    Tk_FreeOptions(composeConfigSpecs, (char *)&info, dataPtr->display, 0);

    /* Convert bitmap back to a data array */
    arraySize = BitmapToData(tkwin, bitmap, width, height, &bits);
    Tk_FreePixmap(dataPtr->display, bitmap);
    if (arraySize == 0) {
	Tcl_AppendResult(interp, "can't get bitmap data", (char *)NULL);
	return TCL_ERROR;
    }
    /* If bitmap is to be rotated or scale, do it here */
    if ((theta != 0.0) || (info.scale != 1.0)) {
	BitmapData srcData, destData;

	srcData.bits = bits;
	srcData.width = width;
	srcData.height = height;
	srcData.arraySize = arraySize;

	result = ScaleRotateData(interp, &srcData, theta, info.scale, 
		 &destData);
	Blt_Free(bits);		/* Free the un-transformed data array. */
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	bits = destData.bits;
	width = destData.width;
	height = destData.height;
    }
    /* Create the bitmap again, this time using Tk's bitmap facilities */
    result = Tk_DefineBitmap(interp, Tk_GetUid(argv[2]), (char *)bits,
	width, height);
    if (result != TCL_OK) {
	Blt_Free(bits);
    }
    hPtr = Blt_CreateHashEntry(&(dataPtr->bitmapTable), argv[2], &isNew);
    Blt_SetHashValue(hPtr, bits);
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * DefineOp --
 *
 *	Converts the dataList into an internal bitmap.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side Effects:
 * 	If an error occurs while processing the data, interp->result
 *	is filled with a corresponding error message.
 *
 *--------------------------------------------------------------
 */
/* ARGSUSED */
static int
DefineOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data for bitmaps. */
    Tcl_Interp *interp;		/* Interpreter to report results to */
    int argc;			/* Number of arguments */
    char **argv;		/* Argument list */
{
    BitmapInterpData *dataPtr = clientData;
    int width, height;		/* Dimensions of bitmap */
    unsigned char *bits;	/* working variable */
    register char *p;
    char *defn;			/* Definition of bitmap. */
    BitmapInfo info;		/* Not used. */
    int arraySize;
    int result;
    double theta;
    Pixmap bitmap;
    Blt_HashEntry *hPtr;
    int isNew;

    bitmap = Tk_GetBitmap(interp, dataPtr->tkwin, Tk_GetUid(argv[2]));
    Tcl_ResetResult(interp);
    if (bitmap != None) {
	Tk_FreeBitmap(dataPtr->display, bitmap);
	return TCL_OK;
    }
    /* Initialize info and then process flags */
    info.rotate = 0.0;		/* No rotation by default */
    info.scale = 1.0;		/* No scaling by default */
    if (Tk_ConfigureWidget(interp, dataPtr->tkwin, defineConfigSpecs,
	    argc - 4, argv + 4, (char *)&info, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Skip leading spaces. */
    for (p = argv[3]; isspace(UCHAR(*p)); p++) {
	/*empty*/
    }
    defn = Blt_Strdup(p);
    bits = NULL;
    if (*p == '#') {
	arraySize = ParseStructData(interp, defn, &width, &height, &bits);
    } else {
	arraySize = ParseListData(interp, defn, &width, &height, &bits);
    }
    Blt_Free(defn);
    if (arraySize <= 0) {
	return TCL_ERROR;
    }
    theta = FMOD(info.rotate, 360.0);
    if (theta < 0.0) {
	theta += 360.0;
    }
    /* If bitmap is to be rotated or scale, do it here */
    if ((theta != 0.0) || (info.scale != 1.0)) { 
	BitmapData srcData, destData;

	srcData.bits = bits;
	srcData.width = width;
	srcData.height = height;
	srcData.arraySize = arraySize;

	result = ScaleRotateData(interp, &srcData, theta, info.scale, 
		 &destData);
	Blt_Free(bits);		/* Free the array of un-transformed data. */
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	bits = destData.bits;
	width = destData.width;
	height = destData.height;
    }
    result = Tk_DefineBitmap(interp, Tk_GetUid(argv[2]), (char *)bits,
	width, height);
    if (result != TCL_OK) {
	Blt_Free(bits);
    }
    hPtr = Blt_CreateHashEntry(&(dataPtr->bitmapTable), argv[2], &isNew);
    Blt_SetHashValue(hPtr, bits);
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * ExistOp --
 *
 *	Indicates if the named bitmap exists.
 *
 *--------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ExistsOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data for bitmaps. */
    Tcl_Interp *interp;		/* Interpreter to report results to */
    int argc;			/* Not used. */
    char **argv;		/* Argument list */
{
    BitmapInterpData *dataPtr = clientData;
    Pixmap bitmap;

    bitmap = Tk_GetBitmap(interp, dataPtr->tkwin, Tk_GetUid(argv[2]));
    Tcl_ResetResult(interp);
    if (bitmap != None) {
	Tk_FreeBitmap(dataPtr->display, bitmap);
    }
    Blt_SetBooleanResult(interp, (bitmap != None));
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * HeightOp --
 *
 *	Returns the height of the named bitmap.
 *
 *--------------------------------------------------------------
 */
/*ARGSUSED*/
static int
HeightOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data for bitmaps. */
    Tcl_Interp *interp;		/* Interpreter to report results to */
    int argc;			/* Not used. */
    char **argv;		/* Argument list */
{
    BitmapInterpData *dataPtr = clientData;
    int width, height;
    Pixmap bitmap;
    
    bitmap = Tk_GetBitmap(interp, dataPtr->tkwin, Tk_GetUid(argv[2]));
    if (bitmap == None) {
	return TCL_ERROR;
    }
    Tk_SizeOfBitmap(dataPtr->display, bitmap, &width, &height);
    Tcl_SetResult(interp, Blt_Itoa(height), TCL_VOLATILE);
    Tk_FreeBitmap(dataPtr->display, bitmap);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * WidthOp --
 *
 *	Returns the width of the named bitmap.
 *
 *--------------------------------------------------------------
 */
/*ARGSUSED*/
static int
WidthOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data for bitmaps. */
    Tcl_Interp *interp;		/* Interpreter to report results to */
    int argc;			/* Not used. */
    char **argv;		/* Argument list */
{
    BitmapInterpData *dataPtr = clientData;
    int width, height;
    Pixmap bitmap;

    bitmap = Tk_GetBitmap(interp, dataPtr->tkwin, Tk_GetUid(argv[2]));
    if (bitmap == None) {
	return TCL_ERROR;
    }
    Tk_SizeOfBitmap(dataPtr->display, bitmap, &width, &height);
    Tcl_SetResult(interp, Blt_Itoa(width), TCL_VOLATILE);
    Tk_FreeBitmap(dataPtr->display, bitmap);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * SourceOp --
 *
 *	Returns the data array (excluding width and height)
 *	of the named bitmap.
 *
 *--------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SourceOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data for bitmaps. */
    Tcl_Interp *interp;		/* Interpreter to report results to */
    int argc;			/* Not used. */
    char **argv;		/* Argument list */
{
    BitmapInterpData *dataPtr = clientData;
    Pixmap bitmap;
    Tcl_DString dString;

    bitmap = Tk_GetBitmap(interp, dataPtr->tkwin, Tk_GetUid(argv[2]));
    if (bitmap == None) {
	return TCL_ERROR;
    }
    Tcl_DStringInit(&dString);
    BitmapDataToString(dataPtr->tkwin, bitmap, &dString);
    Tk_FreeBitmap(dataPtr->display, bitmap);
    Tcl_DStringResult(interp, &dString);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DataOp --
 *
 *	Returns the data array, including width and height,
 *	of the named bitmap.
 *
 *--------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DataOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data for bitmaps. */
    Tcl_Interp *interp;		/* Interpreter to report results to */
    int argc;			/* Not used. */
    char **argv;		/* Argument list */
{
    BitmapInterpData *dataPtr = clientData;
    Pixmap bitmap;
    int width, height;
    Tcl_DString dString;

    bitmap = Tk_GetBitmap(interp, dataPtr->tkwin, Tk_GetUid(argv[2]));
    if (bitmap == None) {
	return TCL_ERROR;
    }
    Tk_SizeOfBitmap(dataPtr->display, bitmap, &width, &height);
    Tcl_DStringInit(&dString);
    Tcl_DStringAppendElement(&dString, Blt_Itoa(width));
    Tcl_DStringAppendElement(&dString, Blt_Itoa(height));
    Tcl_DStringStartSublist(&dString);
    BitmapDataToString(dataPtr->tkwin, bitmap, &dString);
    Tcl_DStringEndSublist(&dString);
    Tk_FreeBitmap(dataPtr->display, bitmap);
    Tcl_DStringResult(interp, &dString);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * BLT Sub-command specification:
 *
 *	- Name of the sub-command.
 *	- Minimum number of characters needed to unambiguously
 *        recognize the sub-command.
 *	- Pointer to the function to be called for the sub-command.
 *	- Minimum number of arguments accepted.
 *	- Maximum number of arguments accepted.
 *	- String to be displayed for usage.
 *
 *--------------------------------------------------------------
 */
static Blt_OpSpec bitmapOps[] =
{
    {"compose", 1, (Blt_Op)ComposeOp, 4, 0, "bitmapName text ?flags?",},
    {"data", 2, (Blt_Op)DataOp, 3, 3, "bitmapName",},
    {"define", 2, (Blt_Op)DefineOp, 4, 0, "bitmapName data ?flags?",},
    {"exists", 1, (Blt_Op)ExistsOp, 3, 3, "bitmapName",},
    {"height", 1, (Blt_Op)HeightOp, 3, 3, "bitmapName",},
    {"source", 1, (Blt_Op)SourceOp, 3, 3, "bitmapName",},
    {"width", 1, (Blt_Op)WidthOp, 3, 3, "bitmapName",},
};
static int nBitmapOps = sizeof(bitmapOps) / sizeof(Blt_OpSpec);

/*
 *--------------------------------------------------------------
 *
 * BitmapCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to bitmaps managed by this module.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BitmapCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data for bitmaps. */
    Tcl_Interp *interp;		/* Interpreter to report results to */
    int argc;
    char **argv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nBitmapOps, bitmapOps, BLT_OP_ARG1, argc, argv,0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (clientData, interp, argc, argv);
    return result;
}

/*
 * -----------------------------------------------------------------------
 *
 * BitmapInterpDeleteProc --
 *
 *	This is called when the interpreter is deleted. All the tiles
 *	are specific to that interpreter are destroyed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys the tile table.
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
BitmapInterpDeleteProc(clientData, interp)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
{
    BitmapInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    unsigned char *bits;
    Blt_HashSearch cursor;
    
    for (hPtr = Blt_FirstHashEntry(&(dataPtr->bitmapTable), &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	bits = (unsigned char *)Blt_GetHashValue(hPtr);
	Blt_Free(bits);
    }
    Blt_DeleteHashTable(&(dataPtr->bitmapTable));
    Tcl_DeleteAssocData(interp, BITMAP_THREAD_KEY);
    Blt_Free(dataPtr);
}

static BitmapInterpData *
GetBitmapInterpData(interp)
    Tcl_Interp *interp;
{
    BitmapInterpData *dataPtr;
    Tcl_InterpDeleteProc *proc;

    dataPtr = (BitmapInterpData *)
	Tcl_GetAssocData(interp, BITMAP_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_Malloc(sizeof(BitmapInterpData));
	assert(dataPtr);
	dataPtr->interp = interp;
	dataPtr->tkwin = Tk_MainWindow(interp);
	dataPtr->display = Tk_Display(dataPtr->tkwin);
	Tcl_SetAssocData(interp, BITMAP_THREAD_KEY, BitmapInterpDeleteProc, 
		 dataPtr);
	Blt_InitHashTable(&(dataPtr->bitmapTable), BLT_STRING_KEYS);
    }
    return dataPtr;
}

/*
 *--------------------------------------------------------------
 *
 * Blt_BitmapInit --
 *
 *	This procedure is invoked to initialize the bitmap command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds the command to the interpreter and sets an array variable
 *	which its version number.
 *
 *--------------------------------------------------------------
 */
int
Blt_BitmapInit(interp)
    Tcl_Interp *interp;
{
    BitmapInterpData *dataPtr;
    static Blt_CmdSpec cmdSpec =
    {"bitmap", BitmapCmd};

    /* Define the BLT logo bitmaps */

    dataPtr = GetBitmapInterpData(interp);
    cmdSpec.clientData = dataPtr;
    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    Tk_DefineBitmap(interp, Tk_GetUid("bigBLT"), (char *)bigblt_bits,
	bigblt_width, bigblt_height);
    Tk_DefineBitmap(interp, Tk_GetUid("BLT"), (char *)blt_bits,
	blt_width, blt_height);
    Tcl_ResetResult(interp);
    return TCL_OK;
}

#endif /* NO_BITMAP */
