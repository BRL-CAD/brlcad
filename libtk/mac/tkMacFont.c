/* 
 * tkMacFont.c --
 *
 *	Contains the Macintosh implementation of the platform-independant
 *	font package interface.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS:@(#) tkMacFont.c 1.52 97/11/20 18:29:51 
 */
 
#include <Windows.h>
#include <Strings.h>
#include <Fonts.h>
#include <Resources.h>

#include "tkMacInt.h"
#include "tkFont.h"
#include "tkPort.h"

/*
 * The following structure represents the Macintosh's' implementation of a
 * font.
 */

typedef struct MacFont {
    TkFont font;		/* Stuff used by generic font package.  Must
				 * be first in structure. */
    short family;
    short size;
    short style;
} MacFont;

static GWorldPtr gWorld = NULL;

static TkFont *		AllocMacFont _ANSI_ARGS_((TkFont *tkfont, 
			    Tk_Window tkwin, int family, int size, int style));


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
 *	the contents of the generics TkFont before calling TkpDeleteFont().
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

TkFont *
TkpGetNativeFont(
    Tk_Window tkwin,	/* For display where font will be used. */
    CONST char *name)	/* Platform-specific font name. */
{
    short family;
    
    if (strcmp(name, "system") == 0) {
	family = GetSysFont();
    } else if (strcmp(name, "application") == 0) {
	family = GetAppFont();
    } else {
	return NULL;
    }

    return AllocMacFont(NULL, tkwin, family, 0, 0);
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
TkpGetFontFromAttributes(
    TkFont *tkFontPtr,		/* If non-NULL, store the information in
				 * this existing TkFont structure, rather than
				 * allocating a new structure to hold the
				 * font; the existing contents of the font
				 * will be released.  If NULL, a new TkFont
				 * structure is allocated. */
    Tk_Window tkwin,		/* For display where font will be used. */
    CONST TkFontAttributes *faPtr)  /* Set of attributes to match. */
{
    char buf[257];
    size_t len;
    short family, size, style;

    if (faPtr->family == NULL) {
	family = 0;
    } else {
	CONST char *familyName;

	familyName = faPtr->family;
	if (strcasecmp(familyName, "Times New Roman") == 0) {
	    familyName = "Times";
	} else if (strcasecmp(familyName, "Courier New") == 0) {
	    familyName = "Courier";
	} else if (strcasecmp(familyName, "Arial") == 0) {
	    familyName = "Helvetica";
	}
	    
	len = strlen(familyName);
	if (len > 255) {
	    len = 255;
	}
	buf[0] = (char) len;
	memcpy(buf + 1, familyName, len);
	buf[len + 1] = '\0';
	GetFNum((StringPtr) buf, &family);
    }

    size = faPtr->pointsize;
    if (size <= 0) {
	size = GetDefFontSize();
    }

    style = 0;
    if (faPtr->weight != TK_FW_NORMAL) {
	style |= bold;
    }
    if (faPtr->slant != TK_FS_ROMAN) {
	style |= italic;
    }
    if (faPtr->underline) {
	style |= underline;
    }

    return AllocMacFont(tkFontPtr, tkwin, family, size, style);
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
TkpDeleteFont(
    TkFont *tkFontPtr)		/* Token of font to be deleted. */
{
    ckfree((char *) tkFontPtr);
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
TkpGetFontFamilies(
    Tcl_Interp *interp,		/* Interp to hold result. */
    Tk_Window tkwin)		/* For display to query. */
{    
    MenuHandle fontMenu;
    int i;
    char itemText[257];
    
    fontMenu = NewMenu(1, "\px");
    AddResMenu(fontMenu, 'FONT');
    
    for (i = 1; i < CountMItems(fontMenu); i++) {
    	/*
    	 * Each item is a pascal string. Convert it to C and append.
    	 */
    	GetMenuItemText(fontMenu, i, (unsigned char *) itemText);
    	itemText[itemText[0] + 1] = '\0';
    	Tcl_AppendElement(interp, &itemText[1]);
    }
    DisposeMenu(fontMenu);
}


/*
 *---------------------------------------------------------------------------
 *
 * TkMacIsCharacterMissing --
 *
 *	Given a tkFont and a character determines whether the character has
 *	a glyph defined in the font or not. Note that this is potentially
 *	not compatible with Mac OS 8 as it looks at the font handle
 *	structure directly. Looks into the character array of the font
 *	handle to determine whether the glyph is defined or not.
 *
 * Results:
 *	Returns a 1 if the character is missing, a 0 if it is not.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

int
TkMacIsCharacterMissing(
    Tk_Font tkfont,		/* The font we are looking in. */
    unsigned int searchChar)	/* The character we are looking for. */
{
    MacFont *fontPtr = (MacFont *) tkfont;
    FMInput fm;
    FontRec **fontRecHandle;
    
    fm.family = fontPtr->family;
    fm.size = fontPtr->size;
    fm.face = fontPtr->style;
    fm.needBits = 0;
    fm.device = 0;
    fm.numer.h = fm.numer.v = fm.denom.h = fm.denom.v = 1;

    /*
     * This element of the FMOutput structure was changed between the 2.0 & 3.0
     * versions of the Universal Headers.
     */
        
#if !defined(UNIVERSAL_INTERFACES_VERSION) || (UNIVERSAL_INTERFACES_VERSION < 0x0300)
    fontRecHandle = (FontRec **) FMSwapFont(&fm)->fontResult;
#else
    fontRecHandle = (FontRec **) FMSwapFont(&fm)->fontHandle;
#endif
    return *(short *) ((long) &(*fontRecHandle)->owTLoc 
    	    + ((long)((*fontRecHandle)->owTLoc + searchChar 
    	    - (*fontRecHandle)->firstChar) * sizeof(short))) == -1;
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
Tk_MeasureChars(
    Tk_Font tkfont,		/* Font in which characters will be drawn. */
    CONST char *source,		/* Characters to be displayed.  Need not be
				 * '\0' terminated. */
    int numChars,		/* Maximum number of characters to consider
				 * from source string. */
    int maxLength,		/* If > 0, maxLength specifies the longest
				 * permissible line length; don't consider any
				 * character that would cross this
				 * x-position.  If <= 0, then line length is
				 * unbounded and the flags argument is
				 * ignored. */
    int flags,			/* Various flag bits OR-ed together:
				 * TK_PARTIAL_OK means include the last char
				 * which only partially fit on this line.
				 * TK_WHOLE_WORDS means stop on a word
				 * boundary, if possible.
				 * TK_AT_LEAST_ONE means return at least one
				 * character even if no characters fit. */
    int *lengthPtr)		/* Filled with x-location just after the
				 * terminating character. */
{
    short staticWidths[128];
    short *widths;
    CONST char *p, *term;
    int curX, termX, curIdx, sawNonSpace;
    MacFont *fontPtr;
    CGrafPtr saveWorld;
    GDHandle saveDevice;

    if (numChars == 0) {
	*lengthPtr = 0;
	return 0;
    }

    if (gWorld == NULL) {
	Rect rect = {0, 0, 1, 1};

	if (NewGWorld(&gWorld, 0, &rect, NULL, NULL, 0) != noErr) {
	    panic("NewGWorld failed in Tk_MeasureChars");
	}
    }
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(gWorld, NULL);

    fontPtr = (MacFont *) tkfont;
    TextFont(fontPtr->family);
    TextSize(fontPtr->size);
    TextFace(fontPtr->style);

    if (maxLength <= 0) {
        *lengthPtr = TextWidth(source, 0, numChars);
        SetGWorld(saveWorld, saveDevice);
        return numChars;
    }

    if (numChars > maxLength) {
        /*
	 * Assume that all chars are at least 1 pixel wide, so there's no
	 * need to measure more characters than there are pixels.  This
	 * assumption could be refined to an iterative approach that would
	 * use that as a starting point and try more chars if necessary (if
	 * there actually were some zero-width chars).
	 */

	numChars = maxLength;
    }
    if (numChars > SHRT_MAX) {
	/*
	 * If they are trying to measure more than 32767 chars at one time,
	 * it would require several separate measurements.
	 */

	numChars = SHRT_MAX;
    }

    widths = staticWidths;
    if (numChars >= sizeof(staticWidths) / sizeof(staticWidths[0])) {
	widths = (short *) ckalloc((numChars + 1) * sizeof(short));
    }
    
    MeasureText((short) numChars, source, widths);
    
    if (widths[numChars] <= maxLength) {
        curX = widths[numChars];
        curIdx = numChars;
    } else {
        p = term = source;
        curX = termX = 0;

	sawNonSpace = !isspace(UCHAR(*p));
        for (curIdx = 1; ; curIdx++) {
            if (isspace(UCHAR(*p))) {
		if (sawNonSpace) {
		    term = p;
		    termX = widths[curIdx - 1];
		    sawNonSpace = 0;
		}
            } else {
		sawNonSpace = 1;
	    }
            if (widths[curIdx] > maxLength) {
                curIdx--;
                curX = widths[curIdx];
                break;
            }
            p++;
        }
        if (flags & TK_PARTIAL_OK) {
            curIdx++;
            curX = widths[curIdx];
        }
        if ((curIdx == 0) && (flags & TK_AT_LEAST_ONE)) {
	    /*
	     * The space was too small to hold even one character.  Since at
	     * least one character must always fit on a line, return the width
	     * of the first character.
	     */

	    curX = TextWidth(source, 0, 1);
	    curIdx = 1;
        } else if (flags & TK_WHOLE_WORDS) {
	    /*
	     * Break at last word that fits on the line.
	     */
	     
	    if ((flags & TK_AT_LEAST_ONE) && (term == source)) {
		/*
		 * The space was too small to hold an entire word.  This
		 * is the only word on the line, so just return the part of th
		 * word that fit.
		 */
		 
		 ;
            } else {
		curIdx = term - source;
		curX = termX;
	    }
	}
    }

    if (widths != staticWidths) {
	ckfree((char *) widths);
    }

    *lengthPtr = curX;
    
    SetGWorld(saveWorld, saveDevice);
    
    return curIdx;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tk_DrawChars --
 *
 *	Draw a string of characters on the screen.  
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
Tk_DrawChars(
    Display *display,		/* Display on which to draw. */
    Drawable drawable,		/* Window or pixmap in which to draw. */
    GC gc,			/* Graphics context for drawing characters. */
    Tk_Font tkfont,		/* Font in which characters will be drawn;
				 * must be the same as font used in GC. */
    CONST char *source,		/* Characters to be displayed.  Need not be
				 * '\0' terminated.  All Tk meta-characters
				 * (tabs, control characters, and newlines)
				 * should be stripped out of the string that
				 * is passed to this function.  If they are
				 * not stripped out, they will be displayed as
				 * regular printing characters. */
    int numChars,		/* Number of characters in string. */
    int x, int y)		/* Coordinates at which to place origin of
				 * string when drawing. */
{
    MacFont *fontPtr;
    MacDrawable *macWin;
    RGBColor macColor, origColor;
    GWorldPtr destPort;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    short txFont, txFace, txSize;
    BitMapPtr stippleMap;

    fontPtr = (MacFont *) tkfont;
    macWin = (MacDrawable *) drawable;

    destPort = TkMacGetDrawablePort(drawable);
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    
    TkMacSetUpClippingRgn(drawable);
    TkMacSetUpGraphicsPort(gc);
    
    txFont = tcl_macQdPtr->thePort->txFont;
    txFace = tcl_macQdPtr->thePort->txFace;
    txSize = tcl_macQdPtr->thePort->txSize;
    GetForeColor(&origColor);
    
    if ((gc->fill_style == FillStippled
	    || gc->fill_style == FillOpaqueStippled)
	    && gc->stipple != None) {
	Pixmap pixmap;
	GWorldPtr bufferPort;
	
	stippleMap = TkMacMakeStippleMap(drawable, gc->stipple);

	pixmap = Tk_GetPixmap(display, drawable, 	
		stippleMap->bounds.right, stippleMap->bounds.bottom, 0);
		
	bufferPort = TkMacGetDrawablePort(pixmap);
	SetGWorld(bufferPort, NULL);
	
	TextFont(fontPtr->family);
	TextSize(fontPtr->size);
	TextFace(fontPtr->style);
	
	if (TkSetMacColor(gc->foreground, &macColor) == true) {
	    RGBForeColor(&macColor);
	}

	ShowPen();
	MoveTo((short) 0, (short) 0);
	FillRect(&stippleMap->bounds, &tcl_macQdPtr->white);
	MoveTo((short) x, (short) y);
	DrawText(source, 0, (short) numChars);

	SetGWorld(destPort, NULL);
	CopyDeepMask(&((GrafPtr) bufferPort)->portBits, stippleMap, 
		&((GrafPtr) destPort)->portBits, &stippleMap->bounds,
		&stippleMap->bounds, &((GrafPtr) destPort)->portRect,
		srcOr, NULL);
	
	/* TODO: this doesn't work quite right - it does a blend.   you can't
	 * draw white text when you have a stipple.
	 */
		
	Tk_FreePixmap(display, pixmap);
	ckfree(stippleMap->baseAddr);
	ckfree((char *)stippleMap);
    } else {
	TextFont(fontPtr->family);
	TextSize(fontPtr->size);
	TextFace(fontPtr->style);
	
	if (TkSetMacColor(gc->foreground, &macColor) == true) {
	    RGBForeColor(&macColor);
	}

	ShowPen();
	MoveTo((short) (macWin->xOff + x), (short) (macWin->yOff + y));
	DrawText(source, 0, (short) numChars);
    }

    TextFont(txFont);
    TextSize(txSize);
    TextFace(txFace);
    RGBForeColor(&origColor);
    SetGWorld(saveWorld, saveDevice);
}

/*
 *---------------------------------------------------------------------------
 *
 * AllocMacFont --
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

static TkFont *
AllocMacFont(
    TkFont *tkFontPtr,		/* If non-NULL, store the information in
				 * this existing TkFont structure, rather than
				 * allocating a new structure to hold the
				 * font; the existing contents of the font
				 * will be released.  If NULL, a new TkFont
				 * structure is allocated. */
    Tk_Window tkwin,		/* For display where font will be used. */
    int family,			/* Macintosh font family. */
    int size,			/* Point size for Macintosh font. */
    int style)			/* Macintosh style bits. */
{
    char buf[257];
    FontInfo fi;
    MacFont *fontPtr;
    TkFontAttributes *faPtr;
    TkFontMetrics *fmPtr;
    CGrafPtr saveWorld;
    GDHandle saveDevice;

    if (gWorld == NULL) {
	Rect rect = {0, 0, 1, 1};

	if (NewGWorld(&gWorld, 0, &rect, NULL, NULL, 0) != noErr) {
	    panic("NewGWorld failed in AllocMacFont");
	}
    }
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(gWorld, NULL);

    if (tkFontPtr == NULL) {
	fontPtr = (MacFont *) ckalloc(sizeof(MacFont));
    } else {
	fontPtr = (MacFont *) tkFontPtr;
    }

    fontPtr->font.fid	= (Font) fontPtr;

    faPtr = &fontPtr->font.fa;
    GetFontName(family, (StringPtr) buf);
    buf[UCHAR(buf[0]) + 1] = '\0';
    faPtr->family	= Tk_GetUid(buf + 1);
    faPtr->pointsize	= size;
    faPtr->weight	= (style & bold) ? TK_FW_BOLD : TK_FW_NORMAL;
    faPtr->slant	= (style & italic) ? TK_FS_ITALIC : TK_FS_ROMAN;
    faPtr->underline	= ((style & underline) != 0);
    faPtr->overstrike	= 0;

    fmPtr = &fontPtr->font.fm;
    TextFont(family);
    TextSize(size);
    TextFace(style);
    GetFontInfo(&fi);
    fmPtr->ascent	= fi.ascent;	
    fmPtr->descent	= fi.descent;	
    fmPtr->maxWidth	= fi.widMax;
    fmPtr->fixed	= (CharWidth('i') == CharWidth('w'));

    fontPtr->family	= (short) family;
    fontPtr->size	= (short) size;
    fontPtr->style	= (short) style;

    SetGWorld(saveWorld, saveDevice);

    return (TkFont *) fontPtr;
}

