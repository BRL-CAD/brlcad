/*
 * tkUnixFont.c --
 *
 *	Contains the Unix implementation of the platform-independant
 *	font package interface.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkUnixFont.c 1.16 97/10/23 12:47:53
 */
 
#include "tkPort.h"
#include "tkInt.h"
#include "tkUnixInt.h"

#include "tkFont.h"

#ifndef ABS
#define ABS(n)	(((n) < 0) ? -(n) : (n))
#endif

/*
 * The following structure represents Unix's implementation of a font.
 */
 
typedef struct UnixFont {
    TkFont font;		/* Stuff used by generic font package.  Must
				 * be first in structure. */
    Display *display;		/* The display to which font belongs. */
    XFontStruct *fontStructPtr;	/* X information about font. */
    char types[256];		/* Array giving types of all characters in
				 * the font, used when displaying control
				 * characters.  See below for definition. */
    int widths[256];		/* Array giving widths of all possible
				 * characters in the font. */
    int underlinePos;		/* Offset from baseline to origin of
				 * underline bar (used for simulating a native
				 * underlined font). */
    int barHeight;		/* Height of underline or overstrike bar
				 * (used for simulating a native underlined or
				 * strikeout font). */
} UnixFont;

/*
 * Possible values for entries in the "types" field in a UnixFont structure,
 * which classifies the types of all characters in the given font.  This
 * information is used when measuring and displaying characters.
 *
 * NORMAL:		Standard character.
 * REPLACE:		This character doesn't print:  instead of
 *			displaying character, display a replacement
 *			sequence like "\n" (for those characters where
 *			ANSI C defines such a sequence) or a sequence
 *			of the form "\xdd" where dd is the hex equivalent
 *			of the character.
 * SKIP:		Don't display anything for this character.  This
 *			is only used where the font doesn't contain
 *			all the characters needed to generate
 *			replacement sequences.
 */ 

#define NORMAL		0
#define REPLACE		1
#define SKIP		2

/*
 * Characters used when displaying control sequences.
 */

static char hexChars[] = "0123456789abcdefxtnvr\\";

/*
 * The following table maps some control characters to sequences like '\n'
 * rather than '\x10'.  A zero entry in the table means no such mapping
 * exists, and the table only maps characters less than 0x10.
 */

static char mapChars[] = {
    0, 0, 0, 0, 0, 0, 0,
    'a', 'b', 't', 'n', 'v', 'f', 'r',
    0
};


static UnixFont *	AllocFont _ANSI_ARGS_((TkFont *tkFontPtr,
			    Tk_Window tkwin, XFontStruct *fontStructPtr,
			    CONST char *fontName));
static void		DrawChars _ANSI_ARGS_((Display *display,
			    Drawable drawable, GC gc, UnixFont *fontPtr,
			    CONST char *source, int numChars, int x,
			    int y));
static int		GetControlCharSubst _ANSI_ARGS_((int c, char buf[4]));


/*
 *---------------------------------------------------------------------------
 *
 * TkpGetNativeFont --
 *
 *	Map a platform-specific native font name to a TkFont.
 *
 * Results:
 * 	The return value is a pointer to a TkFont that represents the
 *	native font.  If a native font by the given name could not be
 *	found, the return value is NULL.  
 *
 *	Every call to this procedure returns a new TkFont structure,
 *	even if the name has already been seen before.  The caller should
 *	call TkpDeleteFont() when the font is no longer needed.
 *
 *	The caller is responsible for initializing the memory associated
 *	with the generic TkFont when this function returns and releasing
 *	the contents of the generic TkFont before calling TkpDeleteFont().
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

TkFont *
TkpGetNativeFont(tkwin, name)
    Tk_Window tkwin;		/* For display where font will be used. */
    CONST char *name;		/* Platform-specific font name. */
{
    XFontStruct *fontStructPtr;

    fontStructPtr = XLoadQueryFont(Tk_Display(tkwin), name);
    if (fontStructPtr == NULL) {
	return NULL;
    }

    return (TkFont *) AllocFont(NULL, tkwin, fontStructPtr, name);
}

/*
 *---------------------------------------------------------------------------
 *
 * TkpGetFontFromAttributes -- 
 *
 *	Given a desired set of attributes for a font, find a font with
 *	the closest matching attributes.
 *
 * Results:
 * 	The return value is a pointer to a TkFont that represents the
 *	font with the desired attributes.  If a font with the desired
 *	attributes could not be constructed, some other font will be
 *	substituted automatically.
 *
 *	Every call to this procedure returns a new TkFont structure,
 *	even if the specified attributes have already been seen before.
 *	The caller should call TkpDeleteFont() to free the platform-
 *	specific data when the font is no longer needed.  
 *
 *	The caller is responsible for initializing the memory associated
 *	with the generic TkFont when this function returns and releasing
 *	the contents of the generic TkFont before calling TkpDeleteFont().
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
TkFont *
TkpGetFontFromAttributes(tkFontPtr, tkwin, faPtr)
    TkFont *tkFontPtr;		/* If non-NULL, store the information in
				 * this existing TkFont structure, rather than
				 * allocating a new structure to hold the
				 * font; the existing contents of the font
				 * will be released.  If NULL, a new TkFont
				 * structure is allocated. */
    Tk_Window tkwin;		/* For display where font will be used. */
    CONST TkFontAttributes *faPtr;  /* Set of attributes to match. */
{
    int numNames, score, i, scaleable, pixelsize, xaPixelsize;
    int bestIdx, bestScore, bestScaleableIdx, bestScaleableScore;
    TkXLFDAttributes xa;    
    char buf[256];
    UnixFont *fontPtr;
    char **nameList;
    XFontStruct *fontStructPtr;
    CONST char *fmt, *family;
    double d;

    family = faPtr->family;
    if (family == NULL) {
	family = "*";
    }

    pixelsize = -faPtr->pointsize;
    if (pixelsize < 0) {
        d = -pixelsize * 25.4 / 72;
	d *= WidthOfScreen(Tk_Screen(tkwin));
	d /= WidthMMOfScreen(Tk_Screen(tkwin));
	d += 0.5;
        pixelsize = (int) d;
    }

    /*
     * Replace the standard Windows and Mac family names with the names that
     * X likes.
     */

    if ((strcasecmp("Times New Roman", family) == 0)
	    || (strcasecmp("New York", family) == 0)) {
	family = "Times";
    } else if ((strcasecmp("Courier New", family) == 0)
	    || (strcasecmp("Monaco", family) == 0)) {
	family = "Courier";
    } else if ((strcasecmp("Arial", family) == 0)
	    || (strcasecmp("Geneva", family) == 0)) {
	family = "Helvetica";
    }

    /*
     * First try for the Q&D exact match.  
     */

#if 0
    sprintf(buf, "-*-%.200s-%s-%c-normal-*-*-%d-*-*-*-*-iso8859-1", family,
	    ((faPtr->weight > TK_FW_NORMAL) ? "bold" : "medium"),
	    ((faPtr->slant == TK_FS_ROMAN) ? 'r' :
		    (faPtr->slant == TK_FS_ITALIC) ? 'i' : 'o'),
	    faPtr->pointsize * 10);
    fontStructPtr = XLoadQueryFont(Tk_Display(tkwin), buf);
#else
    fontStructPtr = NULL;
#endif

    if (fontStructPtr != NULL) {
	goto end;
    }
    /*
     * Couldn't find exact match.  Now fall back to other available
     * physical fonts.  
     */

    fmt = "-*-%.240s-*-*-*-*-*-*-*-*-*-*-*-*";
    sprintf(buf, fmt, family);
    nameList = XListFonts(Tk_Display(tkwin), buf, 10000, &numNames);
    if (numNames == 0) {
	/*
	 * Try getting some system font.
	 */

	sprintf(buf, fmt, "fixed");
	nameList = XListFonts(Tk_Display(tkwin), buf, 10000, &numNames);
	if (numNames == 0) {
	    getsystem:
	    fontStructPtr = XLoadQueryFont(Tk_Display(tkwin), "fixed");
	    if (fontStructPtr == NULL) {
		fontStructPtr = XLoadQueryFont(Tk_Display(tkwin), "*");
		if (fontStructPtr == NULL) {
		    panic("TkpGetFontFromAttributes: cannot get any font");
		}
	    }
	    goto end;
	}
    }

    /*
     * Inspect each of the XLFDs and pick the one that most closely
     * matches the desired attributes.
     */

    bestIdx = 0;
    bestScore = INT_MAX;
    bestScaleableIdx = 0;
    bestScaleableScore = INT_MAX;

    for (i = 0; i < numNames; i++) {
	score = 0;
	scaleable = 0;
	if (TkParseXLFD(nameList[i], &xa) != TCL_OK) {
	    continue;
	}
	xaPixelsize = -xa.fa.pointsize;
	
	/*
	 * Since most people used to use -adobe-* in their XLFDs,
	 * preserve the preference for "adobe" foundry.  Otherwise
	 * some applications looks may change slightly if another foundry
	 * is chosen.
	 */
	 
	if (strcasecmp(xa.foundry, "adobe") != 0) {
	    score += 3000;
	}
	if (xa.fa.pointsize == 0) {
	    /*
	     * A scaleable font is almost always acceptable, but the
	     * corresponding bitmapped font would be better.
	     */

	    score += 10;
	    scaleable = 1;
	} else {
	    /*
	     * A font that is too small is better than one that is too
	     * big.
	     */

	    if (xaPixelsize > pixelsize) {
		score += (xaPixelsize - pixelsize) * 120;
	    } else { 
		score += (pixelsize - xaPixelsize) * 100;
	    }
	}

	score += ABS(xa.fa.weight - faPtr->weight) * 30;
	score += ABS(xa.fa.slant - faPtr->slant) * 25;
	if (xa.slant == TK_FS_OBLIQUE) {
	    /*
	     * Italic fonts are preferred over oblique. */

	    score += 4;
	}

	if (xa.setwidth != TK_SW_NORMAL) {
	    /*
	     * The normal setwidth is highly preferred.
	     */
	    score += 2000;
	}
	if (xa.charset == TK_CS_OTHER) {
	    /*
	     * The standard character set is highly preferred over
	     * foreign languages charsets (because we don't support
	     * other languages yet).
	     */
	    score += 11000;
	}
	if ((xa.charset == TK_CS_NORMAL) && (xa.encoding != 1)) {
	    /*
	     * The '1' encoding for the characters above 0x7f is highly
	     * preferred over the other encodings.
	     */
	    score += 8000;
	}

	if (scaleable) {
	    if (score < bestScaleableScore) {
		bestScaleableIdx = i;
		bestScaleableScore = score;
	    }
	} else {
	    if (score < bestScore) {
		bestIdx = i;
		bestScore = score;
	    }
	}
	if (score == 0) {
	    break;
	}
    }

    /*
     * Now we know which is the closest matching scaleable font and the
     * closest matching bitmapped font.  If the scaleable font was a
     * better match, try getting the scaleable font; however, if the
     * scalable font was not actually available in the desired
     * pointsize, fall back to the closest bitmapped font.
     */

    fontStructPtr = NULL;
    if (bestScaleableScore < bestScore) {
	char *str, *rest;
	
	/*
	 * Fill in the desired pointsize info for this font.
	 */

	tryscale:
	str = nameList[bestScaleableIdx];
	for (i = 0; i < XLFD_PIXEL_SIZE - 1; i++) {
	    str = strchr(str + 1, '-');
	}
	rest = str;
	for (i = XLFD_PIXEL_SIZE - 1; i < XLFD_REGISTRY; i++) {
	    rest = strchr(rest + 1, '-');
	}
	*str = '\0';
	sprintf(buf, "%.240s-*-%d-*-*-*-*-*%s", nameList[bestScaleableIdx],
		pixelsize, rest);
	*str = '-';
	fontStructPtr = XLoadQueryFont(Tk_Display(tkwin), buf);
	bestScaleableScore = INT_MAX;
    }
    if (fontStructPtr == NULL) {
	strcpy(buf, nameList[bestIdx]);
	fontStructPtr = XLoadQueryFont(Tk_Display(tkwin), buf);
	if (fontStructPtr == NULL) {
	    /*
	     * This shouldn't happen because the font name is one of the
	     * names that X gave us to use, but it does anyhow.
	     */

	    if (bestScaleableScore < INT_MAX) {
		goto tryscale;
	    } else {
		XFreeFontNames(nameList);
		goto getsystem;
	    }
	}
    }
    XFreeFontNames(nameList);

    end:
    fontPtr = AllocFont(tkFontPtr, tkwin, fontStructPtr, buf);
    fontPtr->font.fa.underline  = faPtr->underline;
    fontPtr->font.fa.overstrike = faPtr->overstrike;

    return (TkFont *) fontPtr;
}


/*
 *---------------------------------------------------------------------------
 *
 * TkpDeleteFont --
 *
 *	Called to release a font allocated by TkpGetNativeFont() or
 *	TkpGetFontFromAttributes().  The caller should have already
 *	released the fields of the TkFont that are used exclusively by
 *	the generic TkFont code.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	TkFont is deallocated.
 *
 *---------------------------------------------------------------------------
 */

void
TkpDeleteFont(tkFontPtr)
    TkFont *tkFontPtr;		/* Token of font to be deleted. */
{
    UnixFont *fontPtr;

    fontPtr = (UnixFont *) tkFontPtr;

    XFreeFont(fontPtr->display, fontPtr->fontStructPtr);
    ckfree((char *) fontPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TkpGetFontFamilies --
 *
 *	Return information about the font families that are available
 *	on the display of the given window.
 *
 * Results:
 *	interp->result is modified to hold a list of all the available
 *	font families.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
 
void
TkpGetFontFamilies(interp, tkwin)
    Tcl_Interp *interp;
    Tk_Window tkwin;
{
    int i, new, numNames;
    char *family, *end, *p;
    Tcl_HashTable familyTable;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    char **nameList;

    Tcl_InitHashTable(&familyTable, TCL_STRING_KEYS);

    nameList = XListFonts(Tk_Display(tkwin), "*", 10000, &numNames);
    for (i = 0; i < numNames; i++) {
	if (nameList[i][0] != '-') {
	    continue;
	}
	family = strchr(nameList[i] + 1, '-');
	if (family == NULL) {
	    continue;
	}
	family++;
	end = strchr(family, '-');
	if (end == NULL) {
	    continue;
	}
	*end = '\0';
	for (p = family; *p != '\0'; p++) {
	    if (isupper(UCHAR(*p))) {
		*p = tolower(UCHAR(*p));
	    }
	}
	Tcl_CreateHashEntry(&familyTable, family, &new);
    }

    hPtr = Tcl_FirstHashEntry(&familyTable, &search);
    while (hPtr != NULL) {
	Tcl_AppendElement(interp, Tcl_GetHashKey(&familyTable, hPtr));
	hPtr = Tcl_NextHashEntry(&search);
    }

    Tcl_DeleteHashTable(&familyTable);
    XFreeFontNames(nameList);
}

/*
 *---------------------------------------------------------------------------
 *
 *  Tk_MeasureChars --
 *
 *	Determine the number of characters from the string that will fit
 *	in the given horizontal span.  The measurement is done under the
 *	assumption that Tk_DrawChars() will be used to actually display
 *	the characters.
 *
 * Results:
 *	The return value is the number of characters from source that
 *	fit into the span that extends from 0 to maxLength.  *lengthPtr is
 *	filled with the x-coordinate of the right edge of the last
 *	character that did fit.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
int
Tk_MeasureChars(tkfont, source, numChars, maxLength, flags, lengthPtr)
    Tk_Font tkfont;		/* Font in which characters will be drawn. */
    CONST char *source;		/* Characters to be displayed.  Need not be
				 * '\0' terminated. */
    int numChars;		/* Maximum number of characters to consider
				 * from source string. */
    int maxLength;		/* If > 0, maxLength specifies the longest
				 * permissible line length; don't consider any
				 * character that would cross this
				 * x-position.  If <= 0, then line length is
				 * unbounded and the flags argument is
				 * ignored. */
    int flags;			/* Various flag bits OR-ed together:
				 * TK_PARTIAL_OK means include the last char
				 * which only partially fit on this line.
				 * TK_WHOLE_WORDS means stop on a word
				 * boundary, if possible.
				 * TK_AT_LEAST_ONE means return at least one
				 * character even if no characters fit. */
    int *lengthPtr;		/* Filled with x-location just after the
				 * terminating character. */
{
    UnixFont *fontPtr;
    CONST char *p;		/* Current character. */
    CONST char *term;		/* Pointer to most recent character that
				 * may legally be a terminating character. */
    int termX;			/* X-position just after term. */
    int curX;			/* X-position corresponding to p. */
    int newX;			/* X-position corresponding to p+1. */
    int c, sawNonSpace;

    fontPtr = (UnixFont *) tkfont;

    if (numChars == 0) {
	*lengthPtr = 0;
	return 0;
    }

    if (maxLength <= 0) {
	maxLength = INT_MAX;
    }

    newX = curX = termX = 0;
    p = term = source;
    sawNonSpace = !isspace(UCHAR(*p));

    /*
     * Scan the input string one character at a time, calculating width.
     */

    for (c = UCHAR(*p); ; ) {
	newX += fontPtr->widths[c];
	if (newX > maxLength) {
	    break;
	}
	curX = newX;
	numChars--;
	p++;
	if (numChars == 0) {
	    term = p;
	    termX = curX;
	    break;
	}

	c = UCHAR(*p);
	if (isspace(c)) {
	    if (sawNonSpace) {
		term = p;
		termX = curX;
		sawNonSpace = 0;
	    }
	} else {
	    sawNonSpace = 1;
	}
    }

    /*
     * P points to the first character that doesn't fit in the desired
     * span.  Use the flags to figure out what to return.
     */

    if ((flags & TK_PARTIAL_OK) && (numChars > 0) && (curX < maxLength)) {
	/*
	 * Include the first character that didn't quite fit in the desired
	 * span.  The width returned will include the width of that extra
	 * character.
	 */

	numChars--;
	curX = newX;
	p++;
    }
    if ((flags & TK_AT_LEAST_ONE) && (term == source) && (numChars > 0)) {
	term = p;
	termX = curX;
	if (term == source) {
	    term++;
	    termX = newX;
	}
    } else if ((numChars == 0) || !(flags & TK_WHOLE_WORDS)) {
	term = p;
	termX = curX;
    }

    *lengthPtr = termX;
    return term-source;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tk_DrawChars, DrawChars --
 *
 *	Draw a string of characters on the screen.  Tk_DrawChars()
 *	expands control characters that occur in the string to \X or
 *	\xXX sequences.  DrawChars() just draws the strings.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets drawn on the screen.
 *
 *---------------------------------------------------------------------------
 */

void
Tk_DrawChars(display, drawable, gc, tkfont, source, numChars, x, y)
    Display *display;		/* Display on which to draw. */
    Drawable drawable;		/* Window or pixmap in which to draw. */
    GC gc;			/* Graphics context for drawing characters. */
    Tk_Font tkfont;		/* Font in which characters will be drawn;
				 * must be the same as font used in GC. */
    CONST char *source;		/* Characters to be displayed.  Need not be
				 * '\0' terminated.  All Tk meta-characters
				 * (tabs, control characters, and newlines)
				 * should be stripped out of the string that
				 * is passed to this function.  If they are
				 * not stripped out, they will be displayed as
				 * regular printing characters. */
    int numChars;		/* Number of characters in string. */
    int x, y;			/* Coordinates at which to place origin of
				 * string when drawing. */
{
    UnixFont *fontPtr;
    CONST char *p;
    int i, type;
    char buf[4];

    fontPtr = (UnixFont *) tkfont;

    p = source;
    for (i = 0; i < numChars; i++) {
	type = fontPtr->types[UCHAR(*p)];
	if (type != NORMAL) {
	    DrawChars(display, drawable, gc, fontPtr, source, p - source, x, y);
	    x += XTextWidth(fontPtr->fontStructPtr, source, p - source);
	    if (type == REPLACE) {
		DrawChars(display, drawable, gc, fontPtr, buf,
			GetControlCharSubst(UCHAR(*p), buf), x, y);
		x += fontPtr->widths[UCHAR(*p)];
	    }
	    source = p + 1;
	}
	p++;
    }

    DrawChars(display, drawable, gc, fontPtr, source, p - source, x, y);
}

static void
DrawChars(display, drawable, gc, fontPtr, source, numChars, x, y)
    Display *display;		/* Display on which to draw. */
    Drawable drawable;		/* Window or pixmap in which to draw. */
    GC gc;			/* Graphics context for drawing characters. */
    UnixFont *fontPtr;		/* Font in which characters will be drawn;
				 * must be the same as font used in GC. */
    CONST char *source;		/* Characters to be displayed.  Need not be
				 * '\0' terminated.  All Tk meta-characters
				 * (tabs, control characters, and newlines)
				 * should be stripped out of the string that
				 * is passed to this function.  If they are
				 * not stripped out, they will be displayed as
				 * regular printing characters. */
    int numChars;		/* Number of characters in string. */
    int x, y;			/* Coordinates at which to place origin of
				 * string when drawing. */
{		
    XDrawString(display, drawable, gc, x, y, source, numChars);

    if (fontPtr->font.fa.underline != 0) {
	XFillRectangle(display, drawable, gc, x,
		y + fontPtr->underlinePos,
		(unsigned) XTextWidth(fontPtr->fontStructPtr, source, numChars),
		(unsigned) fontPtr->barHeight);
    }
    if (fontPtr->font.fa.overstrike != 0) {
	y -= fontPtr->font.fm.descent + (fontPtr->font.fm.ascent) / 10;
	XFillRectangle(display, drawable, gc, x, y,
		(unsigned) XTextWidth(fontPtr->fontStructPtr, source, numChars),
		(unsigned) fontPtr->barHeight);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * AllocFont --
 *
 *	Helper for TkpGetNativeFont() and TkpGetFontFromAttributes().
 *	Allocates and intializes the memory for a new TkFont that
 *	wraps the platform-specific data.
 *
 * Results:
 *	Returns pointer to newly constructed TkFont.  
 *
 *	The caller is responsible for initializing the fields of the
 *	TkFont that are used exclusively by the generic TkFont code, and
 *	for releasing those fields before calling TkpDeleteFont().
 *
 * Side effects:
 *	Memory allocated.
 *
 *---------------------------------------------------------------------------
 */ 

static UnixFont *
AllocFont(tkFontPtr, tkwin, fontStructPtr, fontName)
    TkFont *tkFontPtr;		/* If non-NULL, store the information in
				 * this existing TkFont structure, rather than
				 * allocating a new structure to hold the
				 * font; the existing contents of the font
				 * will be released.  If NULL, a new TkFont
				 * structure is allocated. */
    Tk_Window tkwin;		/* For display where font will be used. */
    XFontStruct *fontStructPtr;	/* X information about font. */
    CONST char *fontName;	/* The string passed to XLoadQueryFont() to
				 * construct the fontStructPtr. */
{
    UnixFont *fontPtr;
    unsigned long value;
    int i, width, firstChar, lastChar, n, replaceOK;
    char *name, *p;
    char buf[4];
    TkXLFDAttributes xa;
    double d;
    
    if (tkFontPtr != NULL) {
	fontPtr = (UnixFont *) tkFontPtr;
	XFreeFont(fontPtr->display, fontPtr->fontStructPtr);
    } else {
        fontPtr = (UnixFont *) ckalloc(sizeof(UnixFont));
    }

    /*
     * Encapsulate the generic stuff in the TkFont. 
     */

    fontPtr->font.fid		= fontStructPtr->fid;

    if (XGetFontProperty(fontStructPtr, XA_FONT, &value) && (value != 0)) {
	name = Tk_GetAtomName(tkwin, (Atom) value);
	TkInitFontAttributes(&xa.fa);
	if (TkParseXLFD(name, &xa) == TCL_OK) {
	    goto ok;
	}
    }
    TkInitFontAttributes(&xa.fa);
    if (TkParseXLFD(fontName, &xa) != TCL_OK) {
	TkInitFontAttributes(&fontPtr->font.fa);
	fontPtr->font.fa.family = Tk_GetUid(fontName);
    } else {
	ok:
	fontPtr->font.fa = xa.fa;
    }

    if (fontPtr->font.fa.pointsize < 0) {
	d = -fontPtr->font.fa.pointsize * 72 / 25.4;
	d *= WidthMMOfScreen(Tk_Screen(tkwin));
	d /= WidthOfScreen(Tk_Screen(tkwin));
	d += 0.5;
	fontPtr->font.fa.pointsize = (int) d;
    }
	
    fontPtr->font.fm.ascent	= fontStructPtr->ascent;
    fontPtr->font.fm.descent	= fontStructPtr->descent;
    fontPtr->font.fm.maxWidth	= fontStructPtr->max_bounds.width;
    fontPtr->font.fm.fixed	= 1;
    fontPtr->display		= Tk_Display(tkwin);
    fontPtr->fontStructPtr	= fontStructPtr;

    /*
     * Classify the characters.
     */

    firstChar = fontStructPtr->min_char_or_byte2;
    lastChar = fontStructPtr->max_char_or_byte2;
    for (i = 0; i < 256; i++) {
	if ((i == 0177) || (i < firstChar) || (i > lastChar)) {
	    fontPtr->types[i] = REPLACE;
	} else {
	    fontPtr->types[i] = NORMAL;
	}
    }

    /*
     * Compute the widths for all the normal characters.  Any other
     * characters are given an initial width of 0.  Also, this determines
     * if this is a fixed or variable width font, by comparing the widths
     * of all the normal characters.
     */

    width = 0;
    for (i = 0; i < 256; i++) {
	if (fontPtr->types[i] != NORMAL) {
	    n = 0;
	} else if (fontStructPtr->per_char == NULL) {
	    n = fontStructPtr->max_bounds.width;
	} else {
	    n = fontStructPtr->per_char[i - firstChar].width;
	}
	fontPtr->widths[i] = n;
	if (n != 0) {
	    if (width == 0) {
		width = n;
	    } else if (width != n) {
		fontPtr->font.fm.fixed = 0;
	    }
	}
    }

    /*
     * Compute the widths of the characters that should be replaced with
     * control character expansions.  If the appropriate chars are not
     * available in this font, then control character expansions will not
     * be used; control chars will be invisible & zero-width.
     */

    replaceOK = 1;
    for (p = hexChars; *p != '\0'; p++) {
	if ((UCHAR(*p) < firstChar) || (UCHAR(*p) > lastChar)) {
	    replaceOK = 0;
	    break;
	}
    }
    for (i = 0; i < 256; i++) {
	if (fontPtr->types[i] == REPLACE) {
	    if (replaceOK) {
		n = GetControlCharSubst(i, buf);
		for ( ; --n >= 0; ) {
		    fontPtr->widths[i] += fontPtr->widths[UCHAR(buf[n])];
		}
	    } else {
		fontPtr->types[i] = SKIP;
	    }
	}
    }

    if (XGetFontProperty(fontStructPtr, XA_UNDERLINE_POSITION, &value)) {
	fontPtr->underlinePos = value;
    } else {
	/*
	 * If the XA_UNDERLINE_POSITION property does not exist, the X
	 * manual recommends using the following value:
	 */

	fontPtr->underlinePos = fontStructPtr->descent / 2;
    }
    fontPtr->barHeight = 0;
    if (XGetFontProperty(fontStructPtr, XA_UNDERLINE_THICKNESS, &value)) {
	/*
	 * Sometimes this is 0 even though it shouldn't be.
	 */
	fontPtr->barHeight = value;
    }
    if (fontPtr->barHeight == 0) {
	/*
	 * If the XA_UNDERLINE_THICKNESS property does not exist, the X
	 * manual recommends using the width of the stem on a capital
	 * letter.  I don't know of a way to get the stem width of a letter,
	 * so guess and use 1/3 the width of a capital I.
	 */

	fontPtr->barHeight = fontPtr->widths['I'] / 3;
	if (fontPtr->barHeight == 0) {
	    fontPtr->barHeight = 1;
	}
    }
    if (fontPtr->underlinePos + fontPtr->barHeight > fontStructPtr->descent) {
	/*
	 * If this set of cobbled together values would cause the bottom of
	 * the underline bar to stick below the descent of the font, jack
	 * the underline up a bit higher.
	 */

	fontPtr->barHeight = fontStructPtr->descent - fontPtr->underlinePos;
	if (fontPtr->barHeight == 0) {
	    fontPtr->underlinePos--;
	    fontPtr->barHeight = 1;
	}
    }

    return fontPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetControlCharSubst --
 *
 *	When displaying text in a widget, a backslashed escape sequence
 *	is substituted for control characters that occur in the text.
 *	Given a control character, fill in a buffer with the replacement
 *	string that should be displayed.
 *
 * Results:
 *	The return value is the length of the substitute string.  buf is
 *	filled with the substitute string; it is not '\0' terminated.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static int
GetControlCharSubst(c, buf)
    int		c;		/* The control character to be replaced. */
    char	buf[4];		/* Buffer that gets replacement string.  It
				 * only needs to be 4 characters long. */
{
    buf[0] = '\\';
    if ((c < sizeof(mapChars)) && (mapChars[c] != 0)) {
	buf[1] = mapChars[c];
	return 2;
    } else {
	buf[1] = 'x';
	buf[2] = hexChars[(c >> 4) & 0xf];
	buf[3] = hexChars[c & 0xf];
	return 4;
    }
}
