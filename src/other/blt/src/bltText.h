/*
 * bltText.h --
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

#ifndef _BLT_TEXT_H
#define _BLT_TEXT_H

#if (TK_MAJOR_VERSION == 4)

/*
 * The following structure is used by Tk_GetFontMetrics() to return
 * information about the properties of a Tk_Font.
 */
typedef struct {
    int ascent;			/* The amount in pixels that the tallest
				 * letter sticks up above the baseline, plus
				 * any extra blank space added by the designer
				 * of the font. */
    int descent;		/* The largest amount in pixels that any
				 * letter sticks below the baseline, plus any
				 * extra blank space added by the designer of
				 * the font. */
    int linespace;		/* The sum of the ascent and descent.  How
				 * far apart two lines of text in the same
				 * font should be placed so that none of the
				 * characters in one line overlap any of the
				 * characters in the other line. */
} Tk_FontMetrics;

typedef XFontStruct *Tk_Font;

#define Tk_FontId(font)			((font)->fid)
#define Tk_TextWidth(font, str, len)	(XTextWidth((font),(str),(len)))
#define Tk_GetFontMetrics(font, fmPtr)  \
	((fmPtr)->ascent = (font)->ascent, \
	(fmPtr)->descent = (font)->descent, \
	(fmPtr)->linespace = (font)->ascent + (font)->descent)

#define Tk_NameOfFont(font) (Tk_NameOfFontStruct(font))
#define Tk_DrawChars(dpy, draw, gc, font, str, len, x, y) \
    TkDisplayChars((dpy),(draw),(gc),(font),(str),(len),(x),(y), 0, DEF_TEXT_FLAGS)

#define Tk_MeasureChars(font, text, len, maxPixels, flags, lenPtr) \
    TkMeasureChars((font),(text), (len), 0, maxPixels, 0,(flags), (lenPtr))

extern int TkMeasureChars _ANSI_ARGS_((Tk_Font font, char *source,
	int maxChars, int startX, int maxX, int tabOrigin, int flags,
	int *nextXPtr));
extern void TkDisplayChars _ANSI_ARGS_((Display *display, Drawable drawable,
	GC gc, Tk_Font font, char *string, int length, int x, int y,
	int tabOrigin, int flags));
/*
 * FLAGS passed to TkMeasureChars:
 */
#define TK_WHOLE_WORDS			(1<<0)
#define TK_AT_LEAST_ONE			(1<<1)
#define TK_PARTIAL_OK			(1<<2)
#define TK_IGNORE_NEWLINES		(1<<3)
#define TK_IGNORE_TABS			(1<<4)
#define NO_FLAGS			0

#endif /* TK_MAJOR_VERSION == 4 */

#define DEF_TEXT_FLAGS (TK_PARTIAL_OK | TK_IGNORE_NEWLINES)



/*
 * ----------------------------------------------------------------------
 *
 * TextFragment --
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    char *text;			/* Text to be displayed */

    short int x, y;		/* X-Y offset of the baseline from the
				 * upper-left corner of the bbox. */

    short int sx, sy;		/* See bltWinUtil.c */

    short int count;		/* Number of bytes in text. The actual
				 * character count may differ because of
				 * multi-byte UTF encodings. */

    short int width;		/* Width of segment in pixels. This
				 * information is used to draw
				 * PostScript strings the same width
				 * as X. */
} TextFragment;

/*
 * ----------------------------------------------------------------------
 *
 * TextLayout --
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    int nFrags;			/* # fragments of text */
    short int width, height;	/* Dimensions of text bounding box */
    TextFragment fragArr[1];	/* Information about each fragment of text */
} TextLayout;

typedef struct {
    XColor *color;
    int offset;
} Shadow;

/*
 * ----------------------------------------------------------------------
 *
 * TextStyle --
 *
 * 	Represents a convenient structure to hold text attributes
 *	which determine how a text string is to be displayed on the
 *	window, or drawn with PostScript commands.  The alternative
 *	is to pass lots of parameters to the drawing and printing
 *	routines. This seems like a more efficient and less cumbersome
 *	way of passing parameters.
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    unsigned int state;		/* If non-zero, indicates to draw text
				 * in the active color */
    short int width, height;	/* Extents of text */

    XColor *color;		/* Normal color */
    XColor *activeColor;	/* Active color */
    Tk_Font font;		/* Font to use to draw text */
    Tk_3DBorder border;		/* Background color of text.  This is also
				 * used for drawing disabled text. */
    Shadow shadow;		/* Drop shadow color and offset */
    Tk_Justify justify;		/* Justification of the text string. This
				 * only matters if the text is composed
				 * of multiple lines. */
    GC gc;			/* GC used to draw the text */
    double theta;		/* Rotation of text in degrees. */
    Tk_Anchor anchor;		/* Indicates how the text is anchored around
				 * its x and y coordinates. */
    Blt_Pad padX, padY;		/* # pixels padding of around text region */
    short int leader;		/* # pixels spacing between lines of text */

} TextStyle;


extern TextLayout *Blt_GetTextLayout _ANSI_ARGS_((char *string,
	TextStyle *stylePtr));

extern void Blt_GetTextExtents _ANSI_ARGS_((TextStyle *stylePtr,
	char *text, int *widthPtr, int *heightPtr));

extern void Blt_InitTextStyle _ANSI_ARGS_((TextStyle *stylePtr));

extern void Blt_ResetTextStyle _ANSI_ARGS_((Tk_Window tkwin,
	TextStyle *stylePtr));

extern void Blt_FreeTextStyle _ANSI_ARGS_((Display *display,
	TextStyle *stylePtr));

extern void Blt_SetDrawTextStyle _ANSI_ARGS_((TextStyle *stylePtr,
	Tk_Font font, GC gc, XColor *normalColor, XColor *activeColor,
	XColor *shadowColor, double theta, Tk_Anchor anchor, Tk_Justify justify,
	int leader, int shadowOffset));

extern void Blt_SetPrintTextStyle _ANSI_ARGS_((TextStyle *stylePtr,
	Tk_Font font, XColor *fgColor, XColor *bgColor, XColor *shadowColor,
	double theta, Tk_Anchor anchor, Tk_Justify justify, int leader,
	int shadowOffset));

extern void Blt_DrawText _ANSI_ARGS_((Tk_Window tkwin, Drawable drawable,
	char *string, TextStyle *stylePtr, int x, int y));

extern void Blt_DrawTextLayout _ANSI_ARGS_((Tk_Window tkwin,
	Drawable drawable, TextLayout *textPtr, TextStyle *stylePtr,
	int x, int y));

extern void Blt_DrawText2 _ANSI_ARGS_((Tk_Window tkwin, Drawable drawable,
	char *string, TextStyle *stylePtr, int x, int y,
	Dim2D * dimPtr));

extern Pixmap Blt_CreateTextBitmap _ANSI_ARGS_((Tk_Window tkwin,
	TextLayout *textPtr, TextStyle *stylePtr, int *widthPtr,
	int *heightPtr));

extern int Blt_DrawRotatedText _ANSI_ARGS_((Display *display,
	Drawable drawable, int x, int y, double theta,
	TextStyle *stylePtr, TextLayout *textPtr));

#endif /* _BLT_TEXT_H */
