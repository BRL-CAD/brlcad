/*
 * tkMacOSXFont.c --
 *
 *      Contains the Macintosh implementation of the platform-independant
 *      font package interface.  This version uses ATSU instead of Quickdraw.
 *
 * Copyright 2002-2004 Benjamin Riefenstahl, Benjamin.Riefenstahl@epost.de
 *
 * Some functions were originally copied verbatim from the QuickDraw version
 * of tkMacOSXFont.c, which had these copyright notices:
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 *
 * Todos:
 *
 * - Get away from Font Manager and Quickdraw functions as much as possible,
 *   replace with ATS functions instead.
 *
 * - Use Font Manager functions to translate ids from ATS to Font Manager
 *   instead of just assuming that they are the same.
 *
 * - Get a second font register going for fonts that are not assigned to a
 *   font family by the OS.  On my system I have 27 fonts of that type,
 *   Hebrew, Arabic and Hindi fonts that actually come with the system.
 *   FMGetFontFamilyInstanceFromFont() returns -981 (kFMInvalidFontFamilyErr)
 *   for these and they are not listed when enumerating families, but they
 *   are when enumerating fonts directly.  The problem that the OS sees may
 *   be that at least some of them do not contain any Latin characters.  Note
 *   that such fonts can not be used for controls, because controls
 *   definitely require a family id (this assertion needs testing).
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXInt.h"
#include "tkMacOSXFont.h"

/*
#ifdef	TK_MAC_DEBUG
#define TK_MAC_DEBUG_FONTS
#endif
*/

/* Define macros only available on Mac OS X 10.3 or later */
#ifndef FixedToInt
#define FixedToInt(a)       ((short)(((Fixed)(a) + fixed1/2) >> 16))
#endif
#ifndef IntToFixed
#define IntToFixed(a)       ((Fixed)(a) << 16)
#endif

/*
 * Problem: The sum of two parts is not the same as the whole.  In particular
 * the width of two separately measured strings will usually be larger than
 * the width of them pasted together.  Tk has a design bug here, because it
 * generally assumes that this kind of arithmetic works.
 * To workaround this, #define TK_MAC_COALESCE_LINE to 1 below, we then avoid
 * lines that tremble and shiver while the cursor passes through them by
 * undercutting the system and behind the scenes pasting strings together that
 * look like they are on the same line and adjacent and that are drawn with
 * the same font. To do this we need some global data.
 */
#define TK_MAC_COALESCE_LINE 0

typedef TkMacOSXFont MacFont;
typedef TkMacOSXFontDrawingContext DrawingContext;

/*
 * Information about font families, initialized at startup time.  Font
 * families are described by a mapping from UTF-8 names to MacOS font family
 * IDs.  The whole list is kept as the sorted array "familyList", allocated
 * with ckrealloc().
 *
 * Note: This would have been easier, if we could just have used Tcl hash
 * arrays.  Unfortunately there seems to be no pre-packaged
 * non-case-sensitive version of that available.
 */

typedef struct {
    const char * name;
    FMFontFamily familyId;
} MacFontFamily;

static MacFontFamily * familyList = NULL;
static int
    familyListNextFree = 0,     /* The next free slot in familyList. */
    familyListMaxValid = 0,     /* The top of the sorted area. */
    familyListSize = 0;         /* The size of the whole array. */

/*
 * A simple one-shot sub-allocator for fast and efficient allocation of
 * strings.  Used by the familyList array for the names.  These strings are
 * only allocated once at startup and never freed.  If you ever need to
 * re-initialize this, you can just ckfree() all the StringBlocks in the list
 * and start over.
 */

#define STRING_BLOCK_MAX (1024-8)       /* Make sizeof(StringBlock) ==
                                         * 1024. */
typedef struct StringBlock {
    struct StringBlock * next;          /* Starting from "stringMemory" these
                                         * blocks form a linked list. */
    int nextFree;                       /* Top of the used area in the
                                         * "strings" member. */
    char strings[STRING_BLOCK_MAX];     /* The actual memory managed here. */
} StringBlock;

static StringBlock * stringMemory = NULL;


#if TK_MAC_COALESCE_LINE
static Tcl_DString currentLine; /* The current line as seen so far.  This
                                 * contains a Tcl_UniChar DString. */
static int
    currentY = -1,              /* The Y position (row in pixels) of the
                                 * current line. */
    currentLeft = -1,           /* The left edge (pixels) of the current
                                 * line. */
    currentRight = -1;          /* The right edge (pixels) of the current
                                 * line. */
static const MacFont * currentFontPtr = NULL;
                                /* The font of the current line. */
#endif /* TK_MAC_COALESCE_LINE */

static int antialiasedTextEnabled;

/*
 * The names for our two "native" fonts.
 */

#define SYSTEMFONT_NAME "system"
#define APPLFONT_NAME   "application"

/*
 * Procedures used only in this file.
 */

/*
 * The actual workers.
 */

static void MacFontDrawText(
    const MacFont * fontPtr,
    const char * source, int numBytes,
    int rangeStart, int rangeLength,
    int x, int y);
static int MeasureStringWidth(
    const MacFont * fontPtr,
    int start, int end);

#if TK_MAC_COALESCE_LINE
static const Tcl_UniChar * UpdateLineBuffer(
    const MacFont * fontPtr,
    const DrawingContext * drawingContextPtr,
    const char * source, int numBytes,
    int x, int y,
    int * offset);
#endif /* TK_MAC_COALESCE_LINE */

/*
 * Initialization and setup of a font data structure.
 */

static void InitFont(
    Tk_Window tkwin,
    FMFontFamily familyId, const char * familyName,
    int size, int qdStyle, MacFont * fontPtr);
static void InitATSUObjects(
    FMFontFamily familyId,
    short qdsize, short qdStyle,
    ATSUFontID * fontIdPtr,
    ATSUTextLayout * layoutPtr, ATSUStyle * stylePtr);
static void InitATSUStyle(
    ATSUFontID fontId, short ptSize, short qdStyle,
    ATSUStyle style);
static void SetFontFeatures(
    ATSUFontID fontId, int fixed,
    ATSUStyle style);
static void AdjustFontHeight(
    MacFont * fontPtr);
static void InitATSULayout(
    const DrawingContext * drawingContextPtr,
    ATSUTextLayout layout, int fixed);
static void ReleaseFont(
    MacFont * fontPtr);

/*
 * Finding fonts by name.
 */

static const MacFontFamily * FindFontFamilyOrAlias(
    const char * name);
static const MacFontFamily * FindFontFamilyOrAliasOrFallback(
    const char * name);

/*
 * Doing interesting things with font families and fonts.
 */

static void InitFontFamilies(void);
static OSStatus GetFontFamilyName(
    FMFontFamily fontFamily, char * name, int numBytes);

/*
 * Accessor functions and internal utilities for the font family list.
 */

static const MacFontFamily * AddFontFamily(
    const char * name, FMFontFamily familyId);
static const MacFontFamily * FindFontFamily(const char * name);
static Tcl_Obj * EnumFontFamilies(void);

static OSStatus FontFamilyEnumCallback(ATSFontFamilyRef family, void *refCon);
static void SortFontFamilies(void);
static int CompareFontFamilies(const void * vp1, const void * vp2);
static const char * AddString(const char * in);


/*
 *-------------------------------------------------------------------------
 *
 * TkpFontPkgInit --
 *
 *        This procedure is called when an application is created.  It
 *        initializes all the structures that are used by the
 *        platform-dependant code on a per application basis.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Initialization of variables local to this file.
 *
 *-------------------------------------------------------------------------
 */

void
TkpFontPkgInit(
    TkMainInfo * mainPtr)    /* The application being created. */
{
    InitFontFamilies();

#if TK_MAC_COALESCE_LINE
    Tcl_DStringInit(&currentLine);
#endif

#ifdef TK_MAC_DEBUG_FONTS
    fprintf(stderr, "tkMacOSXFont.c (ATSU version) intialized "
            "(" __TIME__ ")\n");
#endif
}

/*
 *---------------------------------------------------------------------------
 *
 * TkpGetNativeFont --
 *
 *        Map a platform-specific native font name to a TkFont.
 *
 * Results:
 *        The return value is a pointer to a TkFont that represents the
 *        native font.  If a native font by the given name could not be
 *        found, the return value is NULL.
 *
 *        Every call to this procedure returns a new TkFont structure, even
 *        if the name has already been seen before.  The caller should call
 *        TkpDeleteFont() when the font is no longer needed.
 *
 *        The caller is responsible for initializing the memory associated
 *        with the generic TkFont when this function returns and releasing
 *        the contents of the generics TkFont before calling TkpDeleteFont().
 *
 * Side effects:
 *        None.
 *
 *---------------------------------------------------------------------------
 */

TkFont *
TkpGetNativeFont(
    Tk_Window tkwin,        /* For display where font will be used. */
    const char * name)      /* Platform-specific font name. */
{
    FMFontFamily familyId;
    MacFont * fontPtr;

    if (strcmp(name, SYSTEMFONT_NAME) == 0) {
        familyId = GetSysFont();
    } else if (strcmp(name, APPLFONT_NAME) == 0) {
        familyId = GetAppFont();
    } else {
        return NULL;
    }

    fontPtr = (MacFont *) ckalloc(sizeof(MacFont));
    InitFont(tkwin, familyId, NULL, 0, 0, fontPtr);

    return (TkFont *) fontPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * TkpGetFontFromAttributes --
 *
 *        Given a desired set of attributes for a font, find a font with the
 *        closest matching attributes.
 *
 * Results:
 *        The return value is a pointer to a TkFont that represents the font
 *        with the desired attributes.  If a font with the desired attributes
 *        could not be constructed, some other font will be substituted
 *        automatically.
 *
 *        Every call to this procedure returns a new TkFont structure, even
 *        if the specified attributes have already been seen before.  The
 *        caller should call TkpDeleteFont() to free the platform- specific
 *        data when the font is no longer needed.
 *
 *        The caller is responsible for initializing the memory associated
 *        with the generic TkFont when this function returns and releasing
 *        the contents of the generic TkFont before calling TkpDeleteFont().
 *
 * Side effects:
 *        None.
 *
 *---------------------------------------------------------------------------
 */

TkFont *
TkpGetFontFromAttributes(
    TkFont * tkFontPtr,     /* If non-NULL, store the information in this
                             * existing TkFont structure, rather than
                             * allocating a new structure to hold the font;
                             * the existing contents of the font will be
                             * released.  If NULL, a new TkFont structure is
                             * allocated. */
    Tk_Window tkwin,        /* For display where font will be used. */
    const TkFontAttributes * faPtr)
                            /* Set of attributes to match. */
{
    short qdStyle;
    FMFontFamily familyId;
    const char * name;
    const MacFontFamily * familyPtr;
    MacFont * fontPtr;

    familyId = GetAppFont();
    name = NULL;
    qdStyle = 0;

    if (faPtr->family != NULL) {
        familyPtr = FindFontFamilyOrAliasOrFallback(faPtr->family);
        if (familyPtr != NULL) {
            name = familyPtr->name;
            familyId = familyPtr->familyId;
        }
    }

    if (faPtr->weight != TK_FW_NORMAL) {
        qdStyle |= bold;
    }
    if (faPtr->slant != TK_FS_ROMAN) {
        qdStyle |= italic;
    }
    if (faPtr->underline) {
        qdStyle |= underline;
    }
    if (tkFontPtr == NULL) {
        fontPtr = (MacFont *) ckalloc(sizeof(MacFont));
    } else {
        fontPtr = (MacFont *) tkFontPtr;
        ReleaseFont(fontPtr);
    }
    InitFont(tkwin, familyId, name, faPtr->size, qdStyle, fontPtr);

    return (TkFont *) fontPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * TkpDeleteFont --
 *
 *        Called to release a font allocated by TkpGetNativeFont() or
 *        TkpGetFontFromAttributes().  The caller should have already
 *        released the fields of the TkFont that are used exclusively by the
 *        generic TkFont code.
 *
 * Results:
 *        TkFont is deallocated.
 *
 * Side effects:
 *        None.
 *
 *---------------------------------------------------------------------------
 */

void
TkpDeleteFont(
    TkFont * tkFontPtr)             /* Token of font to be deleted. */
{
    ReleaseFont((MacFont *) tkFontPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TkpGetFontFamilies --
 *
 *        Return information about the font families that are available on
 *        the display of the given window.
 *
 * Results:
 *        Modifies interp's result object to hold a list of all the available
 *        font families.
 *
 * Side effects:
 *        None.
 *
 *---------------------------------------------------------------------------
 */

void
TkpGetFontFamilies(
    Tcl_Interp * interp,            /* Interp to hold result. */
    Tk_Window tkwin)                /* For display to query. */
{
    Tcl_SetObjResult(interp,EnumFontFamilies());
}

/*
 *-------------------------------------------------------------------------
 *
 * TkpGetSubFonts --
 *
 *        A function used by the testing package for querying the actual
 *        screen fonts that make up a font object.
 *
 * Results:
 *        Modifies interp's result object to hold a list containing the names
 *        of the screen fonts that make up the given font object.
 *
 * Side effects:
 *        None.
 *
 *-------------------------------------------------------------------------
 */

void
TkpGetSubFonts(
    Tcl_Interp * interp,    /* Interp to hold result. */
    Tk_Font tkfont)         /* Font object to query. */
{
    /* We don't know much about our fallback fonts, ATSU does all that for
     * us.  We could use ATSUMatchFont to implement this function.  But as
     * the information is only used for testing, such an effort seems not
     * very useful. */
}

/*
 *---------------------------------------------------------------------------
 *
 *  Tk_MeasureChars --
 *
 *      Determine the number of characters from the string that will fit in
 *      the given horizontal span.  The measurement is done under the
 *      assumption that Tk_DrawChars() will be used to actually display the
 *      characters.
 *
 *      With ATSUI we need the line context to do this right, so we have the
 *      actual implementation in TkpMeasureCharsInContext().
 *
 * Results:
 *
 *      The return value is the number of bytes from source that fit into the
 *      span that extends from 0 to maxLength.  *lengthPtr is filled with the
 *      x-coordinate of the right edge of the last character that did fit.
 *
 * Side effects:
 *
 *      None.
 *
 * Todo:
 *
 *      Effects of the "flags" parameter are untested.
 *
 *---------------------------------------------------------------------------
 */

int
Tk_MeasureChars(
    Tk_Font tkfont,         /* Font in which characters will be drawn. */
    const char * source,    /* UTF-8 string to be displayed.  Need not be
                             * '\0' terminated. */
    int numBytes,           /* Maximum number of bytes to consider from
                             * source string. */
    int maxLength,          /* If >= 0, maxLength specifies the longest
                             * permissible line length; don't consider any
                             * character that would cross this x-position.
                             * If < 0, then line length is unbounded and the
                             * flags argument is ignored. */
    int flags,              /* Various flag bits OR-ed together:
                             * TK_PARTIAL_OK means include the last char
                             * which only partially fit on this line.
                             * TK_WHOLE_WORDS means stop on a word boundary,
                             * if possible.  TK_AT_LEAST_ONE means return at
                             * least one character even if no characters
                             * fit. */
    int * lengthPtr)        /* Filled with x-location just after the
                             * terminating character. */
{
    return TkpMeasureCharsInContext(
        tkfont, source, numBytes, 0, numBytes, maxLength, flags, lengthPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 *  TkpMeasureCharsInContext --
 *
 *      Determine the number of bytes from the string that will fit in the
 *      given horizontal span.  The measurement is done under the assumption
 *      that TkpDrawCharsInContext() will be used to actually display the
 *      characters.
 *
 *      This one is almost the same as Tk_MeasureChars(), but with access to
 *      all the characters on the line for context.
 *
 * Results:
 *      The return value is the number of bytes from source that
 *      fit into the span that extends from 0 to maxLength.  *lengthPtr is
 *      filled with the x-coordinate of the right edge of the last
 *      character that did fit.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

int
TkpMeasureCharsInContext(
    Tk_Font tkfont,         /* Font in which characters will be drawn. */
    const char * source,    /* UTF-8 string to be displayed.  Need not be
                             * '\0' terminated. */
    int numBytes,           /* Maximum number of bytes to consider from
                             * source string in all. */
    int rangeStart,         /* Index of first byte to measure. */
    int rangeLength,        /* Length of range to measure in bytes. */
    int maxLength,          /* If >= 0, maxLength specifies the longest
                             * permissible line length; don't consider any
                             * character that would cross this x-position.
                             * If < 0, then line length is unbounded and the
                             * flags argument is ignored. */
    int flags,  			/* Various flag bits OR-ed together:
                             * TK_PARTIAL_OK means include the last char
                             * which only partially fits on this line.
                             * TK_WHOLE_WORDS means stop on a word boundary,
                             * if possible.
                             * TK_AT_LEAST_ONE means return at least one
                             * character (or at least the first partial word
                             * in case TK_WHOLE_WORDS is also set) even if no
                             * characters (words) fit.
                             * TK_ISOLATE_END means that the last character
                             * should not be considered in context with the
                             * rest of the string (used for breaking
                             * lines).  */
    int * lengthPtr)        /* Filled with x-location just after the
                             * terminating character. */
{
    const MacFont * fontPtr = (const MacFont *) tkfont;
    int curX = -1;
    int curByte = 0;
    UniChar * uchars;
    int ulen, urstart, urlen, urend;
    Tcl_DString ucharBuffer;
    DrawingContext drawingContext;


    /*
     * Sanity checks.
     */

    if ((rangeStart < 0) || ((rangeStart+rangeLength) > numBytes)) {
#ifdef TK_MAC_DEBUG_FONTS
        fprintf(stderr, "MeasureChars: bad parameters\n");
#endif
        *lengthPtr = 0;
        return 0;
    }

    /*
     * Get simple no-brainers out of the way.
     */

    if (rangeLength == 0 || (maxLength == 0 && !(flags & TK_AT_LEAST_ONE))) {
#ifdef TK_MAC_DEBUG_FONTS
        fflush(stdout);
        fprintf(stderr, "measure: '%.*s', empty\n",
                rangeLength, source+rangeStart);
        fflush(stderr);
#endif
        *lengthPtr = 0;
        return 0;
    }

#if TK_MAC_USE_QUARZ
    TkMacOSXQuarzStartDraw(&drawingContext);
#endif

    Tcl_DStringInit(&ucharBuffer);
    uchars = Tcl_UtfToUniCharDString(source, numBytes, &ucharBuffer);
    ulen = Tcl_DStringLength(&ucharBuffer) / sizeof(Tcl_UniChar);
    TkMacOSXLayoutSetString(fontPtr, &drawingContext, uchars, ulen);

    urstart = Tcl_NumUtfChars(source, rangeStart);
    urlen = Tcl_NumUtfChars(source+rangeStart,rangeLength);
    urend = urstart + urlen;

    if (maxLength < 0) {

        curX = MeasureStringWidth(fontPtr, urstart, urend);
        curByte = rangeLength;

    } else {

        UniCharArrayOffset offset = 0;
        OSStatus err;

        /*
         * Have some upper limit on the size actually used.
         */

        if (maxLength > 32767) {
            maxLength = 32767;
        }

        offset = urstart;
        err = noErr;

        if (maxLength > 1) {

            /*
             * Let the system do some work by calculating a line break.
             *
             * Somehow ATSUBreakLine seems to assume that it needs at least
             * one pixel padding.  So we add one to the limit.  Note also
             * that ATSUBreakLine sometimes runs into an endless loop when
             * the third parameter is equal or less than IntToFixed(2), so we
             * need at least IntToFixed(3) (at least that's the current state
             * of my knowledge).
             */

            err = ATSUBreakLine(
                fontPtr->atsuLayout,
                urstart,
                IntToFixed(maxLength+1),
                false, /* !iUseAsSoftLineBreak */
                &offset);

            /*
             * There is no way to signal an error from this routine, so we
             * use predefined offset=urstart and otherwise ignore the
             * possibility.
             */

#ifdef TK_MAC_DEBUG_FONTS
            if ((err != noErr) && (err != kATSULineBreakInWord)) {
                fprintf(stderr, "ATSUBreakLine(): Error %d for '%.*s'\n",
                        (int) err, rangeLength, source+rangeStart);
            }
#endif

#ifdef TK_MAC_DEBUG_FONTS
            fprintf(stderr, "measure: '%.*s', break offset=%d, errcode=%d\n",
                    rangeLength, source+rangeStart, (int) offset, (int) err);
#endif

            /*
             * ATSUBreakLine includes the whitespace that separates words,
             * but we don't want that.  Besides, ATSUBreakLine thinks that
             * spaces don't occupy pixels at the end of the break, which is
             * also something we like to decide for ourself.
             */

            while ((offset > (UniCharArrayOffset)urstart) && (uchars[offset-1] == ' ')) {
                offset--;
            }

            /*
             * Fix up left-overs for the TK_WHOLE_WORDS case.
             */

            if (flags & TK_WHOLE_WORDS) {
                if(flags & TK_AT_LEAST_ONE) {

                    /*
                     * If we are the the start of the range, we need to look
                     * forward.  If we are not at the end of a word, we must
                     * be in the middle of the first word, so we also look
                     * forward.
                     */

                    if ((offset == (UniCharArrayOffset)urstart) || (uchars[offset] != ' ')) {
                        while ((offset < (UniCharArrayOffset)urend)
                                && (uchars[offset] != ' ')) {
                            offset++;
                        }
                    }
                } else {

                    /*
                     * If we are not at the end of a word, we need to look
                     * backward.
                     */

                    if ((offset != (UniCharArrayOffset)urend) && (uchars[offset] != ' ')) {
                        while ((offset > (UniCharArrayOffset)urstart)
                                && (uchars[offset-1] != ' ')) {
                            offset--;
                        }
                        while ((offset > (UniCharArrayOffset)urstart)
                                && (uchars[offset-1] == ' ')) {
                            offset--;
                        }
                    }
                }
            }
        }

        if (offset > (UniCharArrayOffset)urend) {
            offset = urend;
        }

        /*
         * If "flags" says that we don't actually want a word break, we need
         * to find the next character break ourself, as ATSUBreakLine() will
         * only give us word breaks.  Do a simple linear search.
         */

        if ((err != kATSULineBreakInWord)
                && !(flags & TK_WHOLE_WORDS)
                && (offset <= (UniCharArrayOffset)urend)) {

            UniCharArrayOffset lastOffset = offset;
            UniCharArrayOffset nextoffset;
            int lastX = -1;
            int wantonemorechar = -1; /* undecided */

            while (offset <= (UniCharArrayOffset)urend) {

                if (flags & TK_ISOLATE_END) {
                    TkMacOSXLayoutSetString(fontPtr, &drawingContext,
                            uchars, offset);
                }
                curX = MeasureStringWidth(fontPtr, urstart, offset);

#ifdef TK_MAC_DEBUG_FONTS
                fprintf(stderr, "measure: '%.*s', try until=%d, width=%d\n",
                        rangeLength, source+rangeStart, (int) offset, curX);
#endif

                if (curX > maxLength) {

                    /*
                     * Even if we are over the limit, we may want another
                     * character in some situations.  Than we keep looking
                     * for one more character.
                     */

                    if (wantonemorechar == -1) {
                        wantonemorechar = 
                                ((flags & TK_AT_LEAST_ONE)
                                        && (lastOffset == (UniCharArrayOffset)urstart))
                                ||
                                ((flags & TK_PARTIAL_OK) 
                                        && (lastX != maxLength))
                                ;
                        if (!wantonemorechar) {
                            break;
                        }
                        lastX = curX;
                    }

                    /*
                     * There may belong combining marks to this character.
                     * Wait for a new curX to collect them all.
                     */

                    if (lastX != curX) {
                        break;
                    }
                }

                /*
                 * Save this position, so we can come back to it.
                 */

                lastX = curX;
                lastOffset = offset;

                /*
                 * Increment offset by one character, taking combining marks
                 * into account.
                 */

                if (offset >= (UniCharArrayOffset)urend) {
                    break;
                }
                nextoffset = 0;
                if (flags & TK_ISOLATE_END) {
                    TkMacOSXLayoutSetString(fontPtr, &drawingContext,
                            uchars, ulen);
                }
                err = ATSUNextCursorPosition(
                    fontPtr->atsuLayout,
                    offset,
                    kATSUByCluster,
                    &nextoffset);
                if (err != noErr) {
#ifdef TK_MAC_DEBUG_FONTS
                    fprintf(stderr, "ATSUNextCursorPosition(): "
                            "Error %d\n", (int) err);
#endif
                    break;
                }
                if (nextoffset <= offset) {
#ifdef TK_MAC_DEBUG_FONTS
                    fprintf(stderr, "ATSUNextCursorPosition(): "
                            "Can't move further "
                            "(shouldn't happen, bad data?)\n");
#endif
                    break;
                }

                offset = nextoffset;
            }

            /*
             * We have overshot one character, so backup one position.
             */

            curX = lastX;
            offset = lastOffset;
        }

        if (curX < 0) {
            if (flags & TK_ISOLATE_END) {
                TkMacOSXLayoutSetString(fontPtr, &drawingContext,
                        uchars, offset);
            }
            curX = MeasureStringWidth(fontPtr, urstart, offset);
        }

        curByte = Tcl_UtfAtIndex(source, offset) - source;
        curByte -= rangeStart;
    }

    Tcl_DStringFree(&ucharBuffer);

#if TK_MAC_USE_QUARZ
    TkMacOSXQuarzEndDraw(&drawingContext);
#endif

#ifdef TK_MAC_DEBUG_FONTS
    fflush(stdout);
    fprintf(stderr, "measure: '%.*s', maxpix=%d, -> width=%d, bytes=%d, "
            "flags=%s%s%s%s\n",
            rangeLength, source+rangeStart, maxLength, curX, curByte,
            flags & TK_PARTIAL_OK   ? "partialOk "  : "",
            flags & TK_WHOLE_WORDS  ? "wholeWords " : "",
            flags & TK_AT_LEAST_ONE ? "atLeastOne " : "",
            flags & TK_ISOLATE_END  ? "isolateEnd " : "");
    fflush(stderr);
#endif

    *lengthPtr = curX;
    return curByte;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tk_DrawChars --
 *
 *      Draw a string of characters on the screen.
 *
 *      With ATSUI we need the line context to do this right, so we have the
 *      actual implementation in TkpDrawCharsInContext().
 *
 * Results:
 *
 *      None.
 *
 * Side effects:
 *
 *      Information gets drawn on the screen.
 *
 *---------------------------------------------------------------------------
 */

void
Tk_DrawChars(
    Display * display,      /* Display on which to draw. */
    Drawable drawable,      /* Window or pixmap in which to draw. */
    GC gc,                  /* Graphics context for drawing characters. */
    Tk_Font tkfont,         /* Font in which characters will be drawn; must
                             * be the same as font used in GC. */
    const char * source,    /* UTF-8 string to be displayed.  Need not be
                             * '\0' terminated.  All Tk meta-characters
                             * (tabs, control characters, and newlines)
                             * should be stripped out of the string that is
                             * passed to this function.  If they are not
                             * stripped out, they will be displayed as
                             * regular printing characters. */
    int numBytes,           /* Number of bytes in string. */
    int x, int y)           /* Coordinates at which to place origin of the
                             * string when drawing. */
{
    TkpDrawCharsInContext(display, drawable, gc, tkfont, source, numBytes,
            0, numBytes, x, y);
}


/*
 *---------------------------------------------------------------------------
 *
 * TkpDrawCharsInContext --
 *
 *      Draw a string of characters on the screen like Tk_DrawChars(), with
 *      access to all the characters on the line for context.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Information gets drawn on the screen.
 *
 * Todo:
 *
 *      We could try to implement a correct stipple algorithm.
 *
 *      The fiddling with QD GraphPorts can be replaced with just working
 *      with the Quarz CGContext (see TkMacOSXQuarzStartDraw()), once we
 *      decide to stick to Quarz and really forget about QD drawing in here.
 *
 *---------------------------------------------------------------------------
 */

void
TkpDrawCharsInContext(
    Display * display,      /* Display on which to draw. */
    Drawable drawable,      /* Window or pixmap in which to draw. */
    GC gc,                  /* Graphics context for drawing characters. */
    Tk_Font tkfont,         /* Font in which characters will be drawn; must
                             * be the same as font used in GC. */
    const char * source,    /* UTF-8 string to be displayed.  Need not be
                             * '\0' terminated.  All Tk meta-characters
                             * (tabs, control characters, and newlines)
                             * should be stripped out of the string that is
                             * passed to this function.  If they are not
                             * stripped out, they will be displayed as
                             * regular printing characters. */
    int numBytes,           /* Number of bytes in string. */
    int rangeStart,         /* Index of first byte to draw. */
    int rangeLength,        /* Length of range to draw in bytes. */
    int x, int y)           /* Coordinates at which to place origin of the
                             * whole (not just the range) string when
                             * drawing. */
{
    const MacFont * fontPtr;
    MacDrawable * macWin;
    RGBColor macColor, origColor;
    GWorldPtr destPort;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    BitMapPtr stippleMap;
    Rect portRect;

    fontPtr = (const MacFont *) tkfont;
    macWin = (MacDrawable *) drawable;

    destPort = TkMacOSXGetDrawablePort(drawable);
    GetPortBounds(destPort, &portRect);
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);

    TkMacOSXSetUpClippingRgn(drawable);
    TkMacOSXSetUpGraphicsPort(gc, destPort);

    GetForeColor(&origColor);

    if ((gc->fill_style == FillStippled
            || gc->fill_style == FillOpaqueStippled)
            && gc->stipple != None) {

        Pixmap pixmap;
        GWorldPtr bufferPort;
        Pattern   white;

        stippleMap = TkMacOSXMakeStippleMap(drawable, gc->stipple);

        pixmap = Tk_GetPixmap(display, drawable,
            stippleMap->bounds.right, stippleMap->bounds.bottom, 0);

        bufferPort = TkMacOSXGetDrawablePort(pixmap);
        SetGWorld(bufferPort, NULL);

        if (TkSetMacColor(gc->foreground, &macColor)) {
            RGBForeColor(&macColor);
        }
        GetQDGlobalsWhite(&white);
        ShowPen();
        FillRect(&stippleMap->bounds, &white);
        MacFontDrawText(fontPtr, source, numBytes, rangeStart, rangeLength,
                0, 0);
        HidePen();

        SetGWorld(destPort, NULL);

        CopyDeepMask(GetPortBitMapForCopyBits(bufferPort), stippleMap,
            GetPortBitMapForCopyBits(destPort), &stippleMap->bounds,
            &stippleMap->bounds, &portRect,
            srcOr, NULL);

        /* 
         * TODO: this doesn't work quite right - it does a blend.  you can't
         * draw white text when you have a stipple.
         */

        Tk_FreePixmap(display, pixmap);
        ckfree(stippleMap->baseAddr);
        ckfree((char *)stippleMap);
    } else {
        if (TkSetMacColor(gc->foreground, &macColor)) {
            RGBForeColor(&macColor);
        }
        ShowPen();
        MacFontDrawText(fontPtr, source, numBytes, rangeStart, rangeLength,
                macWin->xOff + x, macWin->yOff + y);
        HidePen();
    }

    RGBForeColor(&origColor);

    SetGWorld(saveWorld, saveDevice);
}

#if TK_MAC_USE_QUARZ
#define RGBFLOATRED(c)   (float)((float)(c.red)   / 65535.0f)
#define RGBFLOATGREEN(c) (float)((float)(c.green) / 65535.0f)
#define RGBFLOATBLUE(c)  (float)((float)(c.blue)  / 65535.0f)
/*
 *-------------------------------------------------------------------------
 *
 * TkMacOSXQuarzStartDraw --
 *
 *      Setup a Quarz CGContext from the current QD GraphPort for use in
 *      drawing or measuring.
 *
 * Results:
 *
 *      A CGContext is allocated, configured and returned in
 *      drawingContextPtr.  Also drawingContextPtr->portRect is filled in.
 *
 * Side effects:
 *
 *      None.
 *
 * Assumptions:
 *
 *      The current QD GraphPort contains all the data necessary.  This is
 *      clearly the case for the actual drawing, but not so clear for
 *      measuring.  OTOH for measuring the specific parameters are not really
 *      interesting and the GraphPort is not changed either.  The
 *      availability of a CGContext may be important for the measuring
 *      process though.
 *
 *-------------------------------------------------------------------------
 */

void
TkMacOSXQuarzStartDraw(
    DrawingContext * drawingContextPtr)     /* Quarz context data filled in
                                             * by this function. */
{
    GDHandle currentDevice;
    CGrafPtr destPort;
    RGBColor macColor;
    CGContextRef outContext;
    OSStatus err;
    Rect boundsRect;
    static RgnHandle clipRgn = NULL;

    GetGWorld(&destPort, &currentDevice);

    err = QDBeginCGContext(destPort, &outContext);

    if (err == noErr && outContext) {
	/*
	 * Now clip the CG Context to the port. We also have to intersect our clip
	 * region with the port visible region so we don't overwrite the window
	 * decoration.
	 */

	if (!clipRgn) {
	    clipRgn = NewRgn();
	}

	GetPortBounds(destPort, &boundsRect);

	RectRgn(clipRgn, &boundsRect);
	SectRegionWithPortClipRegion(destPort, clipRgn);
	SectRegionWithPortVisibleRegion(destPort, clipRgn);
	ClipCGContextToRegion(outContext, &boundsRect, clipRgn);
	SetEmptyRgn(clipRgn);

	/*
	 * Note: You have to call SyncCGContextOriginWithPort
	 * AFTER all the clip region manipulations.
	 */

	SyncCGContextOriginWithPort(outContext, destPort);

	/*
	 * Scale the color values, as QD uses UInt16 with the range [0..2^16-1]
	 * while Quarz uses float with [0..1].  NB: Only
	 * CGContextSetRGBFillColor() seems to be actually used by ATSU.
	 */

	GetForeColor(&macColor);
	CGContextSetRGBFillColor(outContext,
		RGBFLOATRED(macColor),
		RGBFLOATGREEN(macColor),
		RGBFLOATBLUE(macColor),
		1.0f);
 #ifdef TK_MAC_DEBUG_FONTS
    } else {
	fprintf(stderr, "QDBeginCGContext(): Error %d\n", (int) err);
 #endif
    }

    drawingContextPtr->graphPort = destPort;
    drawingContextPtr->cgContext = outContext;
    drawingContextPtr->portRect = boundsRect;

}

/*
 *-------------------------------------------------------------------------
 *
 * TkMacOSXQuarzEndDraw --
 *
 *      Free the Quarz CGContext in drawingContextPtr.
 *
 * Results:
 *
 *      The CGContext is de-allocated.  drawingContextPtr->cgContext will be
 *      invalid after this.
 *
 * Side effects:
 *
 *      None.
 *
 *-------------------------------------------------------------------------
 */

void
TkMacOSXQuarzEndDraw(
    DrawingContext * drawingContextPtr)
{
    if (drawingContextPtr->cgContext) {
	QDEndCGContext(
		drawingContextPtr->graphPort,
		&drawingContextPtr->cgContext);
    }
}
#endif /* TK_MAC_USE_QUARZ */

/*
 *-------------------------------------------------------------------------
 *
 * MacFontDrawText --
 *
 *        Helper function for Tk_DrawChars.  Draws characters, using the
 *        screen font in fontPtr to draw multilingual characters.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Information gets drawn on the screen.
 *
 *-------------------------------------------------------------------------
 */

static void
MacFontDrawText(
    const MacFont * fontPtr,    /* Contains font to use when drawing
                                 * following string. */
    const char * source,        /* Potentially multilingual UTF-8 string. */
    int numBytes,               /* Length of string in bytes. */
    int rangeStart,             /* Index of first byte to draw. */
    int rangeLength,            /* Length of range to draw in bytes. */
    int x, int y)               /* Coordinates at which to place origin of
                                 * string when drawing. */
{
    Fixed fx, fy;
    int ulen, urstart, urlen;
    const UniChar * uchars;
    int lineOffset;
    DrawingContext drawingContext;
    OSStatus err;

#if !TK_MAC_COALESCE_LINE
    Tcl_DString runString;
#endif


#if TK_MAC_USE_QUARZ
    TkMacOSXQuarzStartDraw(&drawingContext);

    /*
     * Turn the y coordinate upside-down for Quarz drawing.  We would have
     * liked to just use a CTM transform in the CGContext, but than we get
     * upside-down text, so doing it that way gets more painfull than to just
     * hack around the problem right here.
     */

    y = drawingContext.portRect.bottom - drawingContext.portRect.top - y;
    fy = IntToFixed(y);
#else
    fy = IntToFixed(y);
#endif


#if TK_MAC_COALESCE_LINE
    UpdateLineBuffer(
            fontPtr, &drawingContext, source, numBytes, x, y, &lineOffset);

    fx = IntToFixed(currentLeft);

    uchars = (const Tcl_UniChar*) Tcl_DStringValue(&currentLine);
    ulen = Tcl_DStringLength(&currentLine) / sizeof(uchars[0]);
#else
    lineOffset = 0;
    fx = IntToFixed(x);

    Tcl_DStringInit(&runString);
    uchars = Tcl_UtfToUniCharDString(source, numBytes, &runString);
    ulen = Tcl_DStringLength(&runString) / sizeof(uchars[0]);

    TkMacOSXLayoutSetString(fontPtr, &drawingContext, uchars, ulen);
#endif

    urstart = Tcl_NumUtfChars(source, rangeStart);
    urlen = Tcl_NumUtfChars(source+rangeStart,rangeLength);

    err = ATSUDrawText(
            fontPtr->atsuLayout,
            lineOffset+urstart, urlen,
            fx, fy);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "ATSUDrawText(): Error %d\n", (int) err);
    }
#endif

#if !TK_MAC_COALESCE_LINE
    Tcl_DStringFree(&runString);
#endif

#if TK_MAC_USE_QUARZ
    TkMacOSXQuarzEndDraw(&drawingContext);
#endif
}


/*
 *---------------------------------------------------------------------------
 *
 * MeasureStringWidth --
 *
 *      Low-level measuring of strings.
 *
 * Results:
 *
 *      The width of the string in pixels.
 *
 * Side effects:
 *
 *      None.
 *
 * Assumptions:
 *
 *      fontPtr->atsuLayout is setup with the actual string data to measure.
 *
 *---------------------------------------------------------------------------
 */
static int
MeasureStringWidth(
    const MacFont * fontPtr,    /* Contains font, ATSU layout and string data
                                 * to measure. */
    int start, int end)         /* Start and end positions to measure in that
                                 * string. */
{
    /*
     * This implementation of measuring via ATSUGetGlyphBounds() does not
     * quite conform with the specification given for [font measure]:
     *
     *     The return value is the total width in pixels of text, not
     *     including the extra pixels used by highly exagerrated characters
     *     such as cursive "f".
     *
     * Instead the result of ATSUGetGlyphBounds() *does* include these
     * "extra pixels".
     */

    ATSTrapezoid bounds;
    ItemCount numBounds;
    OSStatus err;

    if (end <= start) {
        return 0;
    }

    bounds.upperRight.x = bounds.upperLeft.x = 0;
    err = ATSUGetGlyphBounds(
            fontPtr->atsuLayout,
            0, 0,
            start, end-start,
            kATSUseFractionalOrigins,
            1, &bounds, &numBounds);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "ATSUGetGlyphBounds(): Error %d\n", (int) err);
    } else if (numBounds < 1) {
        fprintf(stderr, "ATSUGetGlyphBounds(): No output\n");
    } else if (numBounds > 1) {
        fprintf(stderr, "ATSUGetGlyphBounds(): More output\n");
    }
#endif

    return FixedToInt(bounds.upperRight.x - bounds.upperLeft.x);
}

#if TK_MAC_COALESCE_LINE
/*
 *-------------------------------------------------------------------------
 *
 * UpdateLineBuffer --
 *
 *      See the general dicussion of TK_MAC_COALESCE_LINE on the header
 *      pages.  This function maintains the data for this feature.
 *
 * Results:
 *
 *      The Tcl_UniChar string of the whole line as seen so far.
 *
 * Side effects:
 *
 *      "*offset" is filled with the index of the first new character in
 *      "currentLine".  The globals currentLine, currentY, currentLeft,
 *      currentRight and currentFontPtr are updated as necessary.
 *
 *      The currentLine string is set as the current text in
 *      fontPtr->atsuLayout (see TkMacOSXLayoutSetString()).
 *
 *-------------------------------------------------------------------------
 */

static const Tcl_UniChar *
UpdateLineBuffer(
    const MacFont * fontPtr,/* The font to be used for the new piece of
                             * text. */
    const DrawingContext * drawingContextPtr,
                            /* The Quarz drawing parameters.  Needed for
                             * measuring the new piece.  */
    const char * source,    /* A new piece of line to be added. */
    int numBytes,           /* Length of the new piece. */
    int x, int y,           /* Position of the new piece in the window. */
    int * offset)           /* Filled with the offset of the new piece in
                             * currentLine. */
{
    const Tcl_UniChar * uchars;
    int ulen;

    if (y != currentY
            || x < currentRight-1 || x > currentRight+2
            || currentFontPtr != fontPtr) {
        Tcl_DStringFree(&currentLine);
        Tcl_DStringInit(&currentLine);
        currentY = y;
        currentLeft = x;
        currentFontPtr = fontPtr;
        *offset = 0;
    } else {
        *offset = Tcl_DStringLength(&currentLine) / 2;
    }

    Tcl_UtfToUniCharDString(source, numBytes, &currentLine);
    uchars = (const Tcl_UniChar*) Tcl_DStringValue(&currentLine);
    ulen = Tcl_DStringLength(&currentLine) / sizeof(*uchars);
    TkMacOSXLayoutSetString(fontPtr, drawingContextPtr, uchars, ulen);
    currentRight = x + MeasureStringWidth(fontPtr, *offset, ulen);

    return uchars;
}
#endif /* TK_MAC_COALESCE_LINE */

/*
 *---------------------------------------------------------------------------
 *
 * InitFont --
 *
 *        Helper for TkpGetNativeFont() and TkpGetFontFromAttributes().
 *        Initializes the memory for a MacFont that wraps the
 *        platform-specific data.
 *
 *        The caller is responsible for initializing the fields of the TkFont
 *        that are used exclusively by the generic TkFont code, and for
 *        releasing those fields before calling TkpDeleteFont().
 *
 * Results:
 *        Fills the MacFont structure.
 *
 * Side effects:
 *        Memory allocated.
 *
 *---------------------------------------------------------------------------
 */

static void
InitFont(
    Tk_Window tkwin,        /* For display where font will be used. */
    FMFontFamily familyId,  /* The font family to initialize for. */
    const char * familyName,/* The font family name, if known.  Otherwise
                             * this can be NULL. */
    int size,               /* Point size for the font. */
    int qdStyle,            /* QuickDraw style bits. */
    MacFont * fontPtr)      /* Filled with information constructed from the
                             * above arguments. */
{
    OSStatus err;
    FontInfo fi;
    TkFontAttributes * faPtr;
    TkFontMetrics * fmPtr;
    short points;
    int periodWidth, wWidth;

    if (size == 0) {
            size = GetDefFontSize();
    }
    points = (short) TkFontGetPoints(tkwin, size);

    err = FetchFontInfo(familyId, points, qdStyle, &fi);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "FetchFontInfo(): Error %d\n", (int) err);
    }
#endif

    if (familyName == NULL) {
        char name[256] = "";
        const MacFontFamily * familyPtr;

        err = GetFontFamilyName(familyId, name, sizeof(name));
        if (err == noErr) {

            /*
             * We find the canonical font name, so we can avoid unnecessary
             * memory management.
             */

            familyPtr = FindFontFamily(name);
            if (familyPtr != NULL) {
                familyName = familyPtr->name;
            } else {
#ifdef TK_MAC_DEBUG_FONTS
                fprintf(stderr, "Font family '%s': Not found\n", name);
#endif
            }
        }
    }

    fontPtr->font.fid = (Font) fontPtr;

    faPtr = &fontPtr->font.fa;
    faPtr->family = familyName;
    faPtr->size = points;
    faPtr->weight = (qdStyle & bold) ? TK_FW_BOLD : TK_FW_NORMAL;
    faPtr->slant = (qdStyle & italic) ? TK_FS_ITALIC : TK_FS_ROMAN;
    faPtr->underline = ((qdStyle & underline) != 0);
    faPtr->overstrike = 0;

    fmPtr = &fontPtr->font.fm;

    /*
     * Note: Macs measure the line height as ascent + descent +
     * leading.  Leading as a separate entity does not exist in X11
     * and Tk.  We add it to the ascent at the moment, because adding
     * it to the descent, as the Mac docs would indicate, would change
     * the position of self-drawn underlines.
     */

    fmPtr->ascent = fi.ascent + fi.leading;
    fmPtr->descent = fi.descent;
    fmPtr->maxWidth = fi.widMax;

    fontPtr->qdFont = familyId;
    fontPtr->qdSize = points;
    fontPtr->qdStyle = (short) qdStyle;

    InitATSUObjects(
            familyId, points, qdStyle,
            &fontPtr->atsuFontId, &fontPtr->atsuLayout, &fontPtr->atsuStyle);

    Tk_MeasureChars((Tk_Font)fontPtr, ".", 1, -1, 0, &periodWidth);
    Tk_MeasureChars((Tk_Font)fontPtr, "W", 1, -1, 0, &wWidth);
    fmPtr->fixed = periodWidth == wWidth;

    SetFontFeatures(fontPtr->atsuFontId, fmPtr->fixed, fontPtr->atsuStyle);

    AdjustFontHeight(fontPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * InitATSUObjects --
 *
 *      Helper for InitFont().  Initializes the ATSU data handles for a
 *      MacFont.
 *
 * Results:
 *
 *      Sets up all we know and can do at this point in time in fontIdPtr,
 *      layoutPtr and stylePtr.
 *
 * Side effects:
 *
 *      Allocates data structures inside of ATSU.
 *
 *---------------------------------------------------------------------------
 */

static void
InitATSUObjects(
    FMFontFamily familyId,          /* The font family to use. */
    short ptSize, short qdStyles,   /* The additional font parameters. */
    ATSUFontID * fontIdPtr,         /* Filled with the font id. */
    ATSUTextLayout * layoutPtr,     /* Filled with the ATSU layout handle. */
    ATSUStyle * stylePtr)           /* Filled with the ATSU style handle,
                                     * configured with all parameters. */
{
    OSStatus err;
    FMFontStyle stylesDone, stylesLeft;

    /*
     * Defaults in case of error.
     */

    *fontIdPtr = GetAppFont();
    *stylePtr = 0;
    *layoutPtr = 0;

    /*
     * Generate a font id from family id and QD style bits.
     */

    err = FMGetFontFromFontFamilyInstance(
            familyId, qdStyles, fontIdPtr, &stylesDone);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "FMGetFontFromFontFamilyInstance(): Error %d\n",
                (int) err);
    }
#endif

    /*
     * We see what style bits are left and tell ATSU to synthesize what's
     * left like QD does it.
     */

    stylesLeft = qdStyles & ~(unsigned)stylesDone;

    /*
     * Create the style and set its attributes.
     */

    err = ATSUCreateStyle(stylePtr);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "ATSUCreateStyle(): Error %d\n", (int) err);
    }
#endif
    InitATSUStyle(*fontIdPtr, ptSize, stylesLeft, *stylePtr);

    /*
     * Create the layout.  Note: We can't set the layout attributes here,
     * because the text and the style must be set first.
     */

    err = ATSUCreateTextLayout(layoutPtr);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "ATSUCreateTextLayout(): Error %d\n", (int) err);
    }
#endif
    /*InitATSULayout(*layoutPtr);*/
}

/*
 *---------------------------------------------------------------------------
 *
 * InitATSUStyle --
 *
 *      Helper for InitATSUObjects().  Initializes the ATSU style for a
 *      MacFont.
 *
 * Results:
 *
 *      Sets up all parameters needed for an ATSU style.
 *
 * Side effects:
 *
 *      Allocates data structures for the style inside of ATSU.
 *
 *---------------------------------------------------------------------------
 */

static void
InitATSUStyle(
    ATSUFontID fontId,              /* The font id to use. */
    short ptSize, short qdStyles,   /* Additional font parameters. */
    ATSUStyle style)                /* The style handle to configure. */
{
    /*
     * Attributes for the style.
     */

    Fixed fsize = IntToFixed(ptSize);
    Boolean
        isBold = (qdStyles&bold) != 0,
        isUnderline = (qdStyles&underline) != 0,
        isItalic = (qdStyles&italic) != 0;

    ATSStyleRenderingOptions options =
        antialiasedTextEnabled == -1 ? kATSStyleNoOptions :
        antialiasedTextEnabled == 0  ? kATSStyleNoAntiAliasing :
                                       kATSStyleApplyAntiAliasing;

    static const ATSUAttributeTag styleTags[] = {
        kATSUFontTag, kATSUSizeTag,
        kATSUQDBoldfaceTag, kATSUQDItalicTag, kATSUQDUnderlineTag,
        kATSUStyleRenderingOptionsTag,
    };
    static const ByteCount styleSizes[] = {
        sizeof(ATSUFontID), sizeof(Fixed),
        sizeof(Boolean), sizeof(Boolean), sizeof(Boolean),
        sizeof(ATSStyleRenderingOptions),
    };
    const ATSUAttributeValuePtr styleValues[] = {
        &fontId, &fsize,
        &isBold, &isItalic, &isUnderline,
        &options,
    };

    OSStatus err;

    err = ATSUSetAttributes(
        style,
        sizeof(styleTags)/sizeof(styleTags[0]),
        styleTags, styleSizes, styleValues);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "ATSUSetAttributes(): Error %d\n", (int) err);
    }
#endif
}


/*
 *---------------------------------------------------------------------------
 *
 * SetFontFeatures --
 *
 *      Helper for InitFont().  Request specific font features of the ATSU
 *      style object for a MacFont.
 *
 * Results:
 *
 *      None. 
 *
 * Side effects:
 *
 *      Specific font features are enabled on the ATSU style object.
 *
 *---------------------------------------------------------------------------
 */

static void
SetFontFeatures(
    ATSUFontID fontId,              /* The font id to use. */
    int fixed,                      /* Is this a fixed font? */
    ATSUStyle style)                /* The style handle to configure. */
{
    /*
     * Don't use the standard latin ligatures, if this is determined to be a
     * fixed-width font.
     */

    static const ATSUFontFeatureType fixed_featureTypes[] = {
        kLigaturesType, kLigaturesType
    };
    static const ATSUFontFeatureSelector fixed_featureSelectors[] = {
        kCommonLigaturesOffSelector, kRareLigaturesOffSelector 
    };

    OSStatus err;

    if (fixed) {
        err = ATSUSetFontFeatures(
            style,
            sizeof(fixed_featureTypes)/sizeof(fixed_featureTypes[0]),
            fixed_featureTypes, fixed_featureSelectors);
#ifdef TK_MAC_DEBUG_FONTS
        if (err != noErr) {
            fprintf(stderr, "ATSUSetFontFeatures(): Error %d\n", (int) err);
        }
#endif
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * AdjustFontHeight --
 *
 *      Helper for InitFont().  Check font height against some real world
 *      examples.
 *
 * Results:
 *
 *      None. 
 *
 * Side effects:
 *
 *      The metrics in fontPtr->font.fm are adjusted so that typical combined
 *      characters fit into ascent+descent.
 *
 *---------------------------------------------------------------------------
 */

static void
AdjustFontHeight(
    MacFont * fontPtr)
{
    /*
     * The standard values for ascent, descent and leading as determined in
     * InitFont do not take composition into account, they are designed for
     * plain ASCII characters.  This code measures the actual size of some
     * typical composed characters from the Latin-1 range and corrects these
     * factors, especially the ascent.
     *
     * A font requested with a pixel size may thus have a larger line height
     * than requested.
     *
     * An alternative would be to instruct ATSU to shrink oversized combined
     * characters.  I think I have seen that feature somewhere, but I can't
     * find it now [BR].
     */

    static const UniChar chars[]
            /* Auml,   Aacute, Acirc,  Atilde, Ccedilla */
            = {0x00C4, 0x00C1, 0x00C2, 0x00C3, 0x00C7};
    static const int charslen = sizeof(chars) / sizeof(chars[0]);

    DrawingContext drawingContext;
    Rect size;
    OSStatus err;


#if TK_MAC_USE_QUARZ
    TkMacOSXQuarzStartDraw(&drawingContext);
#endif

    TkMacOSXLayoutSetString(fontPtr, &drawingContext, chars, charslen);

    size.top = size.bottom = 0;
    err = ATSUMeasureTextImage(
        fontPtr->atsuLayout,
        0, charslen,
        0, 0,
        &size);

#if TK_MAC_USE_QUARZ
    TkMacOSXQuarzEndDraw(&drawingContext);
#endif

    if (err != noErr) {
#ifdef TK_MAC_DEBUG_FONTS
        fprintf(stderr, "ATSUMeasureTextImage(): Error %d\n", (int) err);
#endif
    } else {
        TkFontMetrics * fmPtr = &fontPtr->font.fm;
        int ascent = -size.top;
        int descent = size.bottom;

        if (ascent > fmPtr->ascent) {
            fmPtr->ascent = ascent;
        }
        if (descent > fmPtr->descent) {
            fmPtr->descent = descent;
        }
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * InitATSULayout --
 *
 *      Helper for TkMacOSXLayoutSetString().  Initializes the ATSU layout
 *      object for a MacFont and a specific string.
 *
 * Results:
 *
 *      Sets up all parameters needed for an ATSU layout object.
 *
 * Side effects:
 *
 *      Allocates data structures for the layout object inside of ATSU.
 *
 * Assumptions:
 *
 *      The actual string data and style information is already set by
 *      ATSUSetTextPointerLocation() and ATSUSetRunStyle() (see
 *      TkMacOSXLayoutSetString()).
 *
 *---------------------------------------------------------------------------
 */

static void
InitATSULayout(
    const DrawingContext * drawingContextPtr,
                                /* Specifies the CGContext to use. */
    ATSUTextLayout layout,      /* The layout object to configure. */
    int fixed)                  /* Is this a fixed font? */
{
    /*
     * Attributes for the layout.
     */

    ATSLineLayoutOptions layoutOptions =
        0
#if TK_MAC_COALESCE_LINE
    /*
     * Options to use unconditionally, when we try to do coalescing in here.
     */
        | kATSLineDisableAllLayoutOperations
        | kATSLineFractDisable
        | kATSLineUseDeviceMetrics
#endif
        ;

    static const ATSUAttributeTag layoutTags[] = {
#if TK_MAC_USE_QUARZ
        kATSUCGContextTag,
#endif
        kATSULineLayoutOptionsTag,
    };
    static const ByteCount layoutSizes[] = {
#if TK_MAC_USE_QUARZ
        sizeof(CGContextRef),
#endif
        sizeof(ATSLineLayoutOptions),
    };
    const ATSUAttributeValuePtr layoutValues[] = {
#if TK_MAC_USE_QUARZ
        (void*)&drawingContextPtr->cgContext,
#endif
        &layoutOptions,
    };

    OSStatus err;

    /*
     * Ensure W(abcdefg) == W(a)*7 for fixed fonts (Latin scripts only).
     */

    if (fixed) {
        layoutOptions |=
                  kATSLineFractDisable
                | kATSLineUseDeviceMetrics
                ;
    }

    err = ATSUSetLayoutControls(
            layout,
            sizeof(layoutTags)/sizeof(layoutTags[0]),
            layoutTags, layoutSizes, layoutValues);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "ATSUSetLayoutControls(): Error %d\n", (int) err);
    }
#endif

    err = ATSUSetTransientFontMatching(layout, true);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "ATSUSetTransientFontMatching(): Error %d\n",
                (int) err);
    }
#endif
}

/*
 *---------------------------------------------------------------------------
 *
 * TkMacOSXLayoutSetString --
 *
 *      Setup the MacFont for a specific string.
 *
 * Results:
 *
 *      Sets up all parameters so that ATSU can work with the objects in
 *      MacFont.
 *
 * Side effects:
 *
 *      Sets parameters on the layout object fontPtr->atsuLayout.
 *
 *---------------------------------------------------------------------------
 */

void
TkMacOSXLayoutSetString(
    const MacFont * fontPtr,            /* The fontPtr to configure. */
    const DrawingContext * drawingContextPtr,
                                        /* For the CGContext to be used.*/
    const UniChar * uchars, int ulen)   /* The UniChar string to set into
                                         * fontPtr->atsuLayout. */
{
    OSStatus err;
    err = ATSUSetTextPointerLocation(
            fontPtr->atsuLayout,
            uchars, kATSUFromTextBeginning, ulen,
            ulen);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "ATSUSetTextPointerLocation(): Error %d\n", (int) err);
    }
#endif

    /*
     * Styles can only be set after the text is set.
     */

    err = ATSUSetRunStyle(
            fontPtr->atsuLayout, fontPtr->atsuStyle,
            kATSUFromTextBeginning, kATSUToTextEnd);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "ATSUSetRunStyle(): Error %d\n", (int) err);
    }
#endif

    /*
     * Layout attributes can only be set after the styles are set.
     */

    InitATSULayout(
            drawingContextPtr, fontPtr->atsuLayout,
            fontPtr->font.fm.fixed);
}


/*
 *-------------------------------------------------------------------------
 *
 * ReleaseFont --
 *
 *        Called to release the Macintosh-specific contents of a TkFont.  The
 *        caller is responsible for freeing the memory used by the font
 *        itself.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Memory is freed.
 *
 *---------------------------------------------------------------------------
 */

static void
ReleaseFont(
    MacFont * fontPtr)               /* The font to delete. */
{
    ATSUDisposeTextLayout(fontPtr->atsuLayout);
    ATSUDisposeStyle(fontPtr->atsuStyle);
}

/*
 *-------------------------------------------------------------------------
 *
 * FindFontFamilyOrAlias, FindFontFamilyOrAliasOrFallback --
 *
 *      Determine if any physical screen font exists on the system with The
 *      given family name.  If the family exists, then it should be possible
 *      to construct some physical screen font with that family name.
 *
 *      FindFontFamilyOrAlias also considers font aliases as determined by
 *      TkFontGetAliasList().
 *
 *      FindFontFamilyOrAliasOrFallback also considers font aliases as
 *      determined by TkFontGetFallbacks().
 *
 *      The overall algorithm to get the closest font to the one requested is
 *      this:
 *
 *              try fontname
 *              try all aliases for fontname
 *              foreach fallback for fontname
 *                      try the fallback
 *                      try all aliases for the fallback
 *
 * Results:
 *
 *      The return value is NULL if the specified font family does not exist,
 *      a valid MacFontFamily* otherwise.
 *
 * Side effects:
 *
 *      None.
 *
 *-------------------------------------------------------------------------
 */

static const MacFontFamily *
FindFontFamilyOrAlias(
    const char * name)      /* Name or alias name of the font to find. */
{
    const MacFontFamily * familyPtr;
    char ** aliases;
    int i;

    familyPtr = FindFontFamily(name);
    if (familyPtr != NULL) {
        return familyPtr;
    }

    aliases = TkFontGetAliasList(name);
    if (aliases != NULL) {
        for (i = 0; aliases[i] != NULL; i++) {
            familyPtr = FindFontFamily(aliases[i]);
            if (familyPtr != NULL) {
                return familyPtr;
            }
        }
    }
    return NULL;
}

static const MacFontFamily *
FindFontFamilyOrAliasOrFallback(
    const char * name)      /* Name or alias name of the font to find. */
{
    const MacFontFamily * familyPtr;
    const char * fallback;
    char *** fallbacks;
    int i, j;

    familyPtr = FindFontFamilyOrAlias(name);
    if (familyPtr != NULL) {
        return familyPtr;
    }
    fallbacks = TkFontGetFallbacks();
    for (i = 0; fallbacks[i] != NULL; i++) {
        for (j = 0; (fallback = fallbacks[i][j]) != NULL; j++) {
            if (strcasecmp(name, fallback) == 0) {
                for (j = 0; (fallback = fallbacks[i][j]) != NULL; j++) {
                    familyPtr = FindFontFamilyOrAlias(fallback);
                    if (familyPtr != NULL) {
                        return familyPtr;
                    }
                }
            }
            break; /* benny: This "break" is a carry-over from
                    * tkMacOSXFont.c, but what is actually its purpose
                    * ???? */
        }
    }


    /*
     * FIXME: We would have liked to recover by re-enumerating fonts.  But
     * that doesn't work, because Carbon seems to cache the inital list of
     * fonts.  Fonts newly installed don't show up with
     * FMCreateFontFamilyIterator()/FMGetNextFontFamily() without a restart
     * of the app.  Similar problem with fonts removed.
     */

#ifdef TK_MAC_DEBUG_FONTS
    fprintf(stderr, "Font family '%s': Not found\n", name);
#endif

    return NULL;
}

/*
 *-------------------------------------------------------------------------
 *
 * InitFontFamilies --
 *
 *      Helper to TkpFontPkgInit.  Use the Font Manager to fill in the
 *      familyList global array.
 *
 * Results:
 *
 *      None.
 *
 * Side effects:
 *
 *      Allocates memory.
 *
 *-------------------------------------------------------------------------
 */

static void
InitFontFamilies(void)
{
    OSStatus err;

    /*
     * Has this been called before?
     */

    if (familyListNextFree > 0) {
	return;
    }

    err = ATSFontFamilyApplyFunction(FontFamilyEnumCallback,NULL);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "ATSFontFamilyApplyFunction(): Error %d\n", (int) err);
    }
#endif

    AddFontFamily(APPLFONT_NAME, GetAppFont());
    AddFontFamily(SYSTEMFONT_NAME, GetSysFont());

    SortFontFamilies();
}

static OSStatus
FontFamilyEnumCallback(
    ATSFontFamilyRef family, 
    void *refCon)
{
    OSStatus err;
    char name[260] = "";

    (void) refCon;

    err = GetFontFamilyName(family, name, sizeof(name));
    if (err == noErr) {
        AddFontFamily(name, family);
    }

    return noErr;
}


/*
 *-------------------------------------------------------------------------
 *
 * GetFontFamilyName --
 *
 *      Use the Font Manager to get the name of a given FMFontfamily.  This
 *      currently gets the standard, non-localized QuickDraw name.  Other
 *      names would be possible, see docs for ATSUFindFontName for a
 *      selection.  Hint: The MacOSX font selector seems to use the localized
 *      family name given by ATSUFindFontName(kFontFamilyName,
 *      GetCurrentLanguage()).
 *
 * Results:
 *
 *      An OS error code, noErr on success.  name is filled with the
 *      resulting name.
 *
 * Side effects:
 *
 *      None.
 *
 *-------------------------------------------------------------------------
 */

static OSStatus
GetFontFamilyName(
    FMFontFamily fontFamily,        /* The font family for which to find the
                                     * name. */
    char * name, int numBytes)      /* Filled with the result. */
{
    OSStatus err;
    Str255 nativeName;
    CFStringRef cfString;
    TextEncoding encoding;
    ScriptCode nameencoding;

    nativeName[0] = 0;
    name[0] = 0;

    err = FMGetFontFamilyName(fontFamily, nativeName);
    if (err != noErr) {
#ifdef TK_MAC_DEBUG_FONTS
        fprintf(stderr, "FMGetFontFamilyName(): Error %d\n", (int) err);
#endif
        return err;
    }

    /*
     * QuickDraw font names are encoded with the script that the font uses.
     * So we determine that encoding and than we reencode the name.
     */

    encoding = kTextEncodingMacRoman;
    err = FMGetFontFamilyTextEncoding(fontFamily, &encoding);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "FMGetFontFamilyTextEncoding(): Error %d\n", (int) err);
    }
#endif

    nameencoding = encoding;
    err = RevertTextEncodingToScriptInfo(encoding, &nameencoding, NULL, NULL);
#ifdef TK_MAC_DEBUG_FONTS
    if (err != noErr) {
        fprintf(stderr, "RevertTextEncodingToScriptInfo(): Error %d\n",
                (int) err);
    }
#endif

    /*
     * Note: We could use Tcl facilities to do the re-encoding here.  We'd
     * have to maintain tables to map OS encoding codes to Tcl encoding names
     * like tkMacOSXFont.c did.  Using native re-encoding directly instead is
     * a lot easier and future-proof than that.  There is one snag, though: I
     * have seen CFStringGetCString() crash with invalid encoding ids.  But
     * than if that happens it would be a bug in
     * FMGetFontFamilyTextEncoding() or RevertTextEncodingToScriptInfo().
     */

    cfString = CFStringCreateWithPascalStringNoCopy(
            NULL, nativeName, nameencoding, kCFAllocatorNull);
    CFStringGetCString(
            cfString, name, numBytes, kCFStringEncodingUTF8);
    CFRelease(cfString);

    return noErr;
}

/*
 *-------------------------------------------------------------------------
 *
 * FindFontFamily --
 *
 *      Find the font family with the given name in the global familyList.
 *      Uses bsearch() for convenient access.  Comparision is done
 *      non-case-sensitively with CompareFontFamilies() which see.
 *
 * Results:
 *
 *      MacFontFamily: A pair of family id and the actual name registered for
 *      the font.
 *
 * Side effects:
 *
 *      None.
 *
 * Assumption:
 *
 *      Requires the familyList array to be sorted.
 *
 *-------------------------------------------------------------------------
 */

static const MacFontFamily *
FindFontFamily(
    const char * name)       /* The family name. Note: Names are compared
                              * non-case-sensitive. */
{
    const MacFontFamily key = {name,-1};

    if(familyListMaxValid <= 0) {
        return NULL;
    }

    return bsearch(
            &key,
            familyList, familyListMaxValid, sizeof(*familyList),
            CompareFontFamilies);
}

/*
 *-------------------------------------------------------------------------
 *
 * EnumFontFamilies --
 *
 *      Create a Tcl list with the registered names in the global familyList.
 *
 * Results:
 *
 *      A Tcl list of names.
 *
 * Side effects:
 *
 *      None.
 *
 *-------------------------------------------------------------------------
 */

static Tcl_Obj *
EnumFontFamilies(void)
{
    int i;
    Tcl_Obj * tclList;

    tclList = Tcl_NewListObj(0, NULL);
    for (i=0; i<familyListMaxValid; ++i) {
        Tcl_ListObjAppendElement(
                NULL, tclList, Tcl_NewStringObj(familyList[i].name, -1));
    }

    return tclList;
}

/*
 *-------------------------------------------------------------------------
 *
 * AddFontFamily --
 *
 *      Register a font family in familyList.  Until SortFontFamilies() is
 *      called, this is not actually available for FindFontFamily().
 *
 * Results:
 *
 *      MacFontFamily: The new pair of family id and the actual name
 *      registered for the font.
 *
 * Side effects:
 *
 *      New entry in familyList and familyListNextFree updated.
 *
 *-------------------------------------------------------------------------
 */

static const MacFontFamily *
AddFontFamily(
    const char * name,      /* Font family name to register. */
    FMFontFamily familyId)  /* Font family id to register. */
{
    MacFontFamily * familyPtr;

    if (familyListNextFree >= familyListSize) {
        familyListSize += 100;
        familyList = (MacFontFamily *) ckrealloc(
                (void*) familyList,       
                familyListSize * sizeof(*familyList));
    }

    familyPtr = familyList + familyListNextFree;
    ++familyListNextFree;

    familyPtr->name = AddString(name);
    familyPtr->familyId = familyId;

    return familyPtr;
}

/*
 *-------------------------------------------------------------------------
 *
 * SortFontFamilies --
 *
 *      Sort the entries in familyList.  Only after calling
 *      SortFontFamilies(), the new families registered with AddFontFamily()
 *      are actually available for FindFontFamily(), because FindFontFamily()
 *      requires the array to be sorted.
 *
 * Results:
 *
 *      None.
 *
 * Side effects:
 *
 *      familyList is sorted and familyListMaxValid is updated.
 *
 *-------------------------------------------------------------------------
 */

static void
SortFontFamilies(void)
{
    if (familyListNextFree > 0) {
        qsort(  familyList, familyListNextFree, sizeof(*familyList),
                CompareFontFamilies);
    }
    familyListMaxValid = familyListNextFree;
}

/*
 *-------------------------------------------------------------------------
 *
 * CompareFontFamilies --
 *
 *      Comparison function used by SortFontFamilies() and FindFontFamily().
 *
 * Results:
 *
 *      Result as required to generate a stable sort order for bsearch() and
 *      qsort().  The ordering is not case-sensitive as far as
 *      Tcl_UtfNcasecmp() (which see) can provide that.
 *
 *      Note: It would be faster to compare first the length and the actual
 *      strings only as a tie-breaker, but than the ordering wouldn't look so
 *      pretty in [font families] ;-).
 *
 * Side effects:
 *
 *      None.
 *
 *-------------------------------------------------------------------------
 */

static int
CompareFontFamilies(
    const void * vp1,
    const void * vp2)
{
    const char * name1;
    const char * name2;
    int len1;
    int len2;
    int diff;

    name1 = ((const MacFontFamily *) vp1)->name;
    name2 = ((const MacFontFamily *) vp2)->name;

    len1 = Tcl_NumUtfChars(name1, -1);
    len2 = Tcl_NumUtfChars(name2, -1);

    diff = Tcl_UtfNcasecmp(name1, name2, len1<len2 ? len1 : len2);

    return diff == 0 ? len1-len2 : diff;
}

/*
 *-------------------------------------------------------------------------
 *
 * AddString --
 *
 *      Helper for AddFontFamily().  Allocates a string in the one-shot
 *      allocator.
 *
 * Results:
 *
 *      A duplicated string in the one-shot allocator.
 *
 * Side effects:
 *
 *      May allocate a new memory block.
 *
 *-------------------------------------------------------------------------
 */

static const char *
AddString(
    const char * in)        /* String to add, zero-terminated. */
{
    int len;
    char * result;
    
    len = strlen(in) +1;

    if (stringMemory == NULL
            || (stringMemory->nextFree+len) > STRING_BLOCK_MAX ) {
        StringBlock * newblock =
            (StringBlock *) ckalloc(sizeof(StringBlock));
        newblock->next = stringMemory;
        newblock->nextFree = 0;
        stringMemory = newblock;
    }

    result = stringMemory->strings + stringMemory->nextFree;
    stringMemory->nextFree += len;

    memcpy(result, in, len);

    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * TkMacOSXIsCharacterMissing --
 *
 *        Given a tkFont and a character determine whether the character has
 *        a glyph defined in the font or not.
 *
 * Results:
 *        Returns a 1 if the character is missing, a 0 if it is not.
 *
 * Side effects:
 *        None.
 *
 *---------------------------------------------------------------------------
 */

int
TkMacOSXIsCharacterMissing(
    Tk_Font tkfont,                /* The font we are looking in. */
    unsigned int searchChar)       /* The character we are looking for. */
{
    /* Background: This function is private and only used in
     * tkMacOSXMenu.c:FindMarkCharacter().
     *
     * We could use ATSUMatchFont() to implement.  We'd have to change the
     * definition of the encoding of the parameter searchChar from MacRoman
     * to UniChar for that.
     *
     * The system uses font fallback for controls, so we don't really need
     * this. */

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInitControlFontStyle --
 *
 *	This procedure sets up the appropriate ControlFontStyleRec
 *	for a Mac control.
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
TkMacOSXInitControlFontStyle(
    Tk_Font tkfont,             /* Tk font object to use for the control. */
    ControlFontStylePtr fsPtr)  /* The style object to configure. */
{
    const MacFont * fontPtr;
    fontPtr = (MacFont *) tkfont;
    fsPtr->flags =
        kControlUseFontMask|
        kControlUseSizeMask|
        kControlUseFaceMask|
        kControlUseJustMask;
    fsPtr->font = fontPtr->qdFont;
    fsPtr->size = fontPtr->qdSize;
    fsPtr->style = fontPtr->qdStyle;
    fsPtr->just = teCenter;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXUseAntialiasedText --
 *
 *      Enables or disables application-wide use of antialiased text (where
 *      available).  Sets up a linked Tcl global variable to allow
 *      disabling of antialiased text from tcl.
 *      The possible values for this variable are:
 *
 *      -1 - Use system default as configurable in "System Preferences" ->
 *           "General".
 *       0 - Unconditionally disable antialiasing.
 *       1 - Unconditionally enable antialiasing.
 *
 * Results:
 *
 *      TCL_OK.
 *
 * Side effects:
 *
 *      None.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
TkMacOSXUseAntialiasedText(
    Tcl_Interp * interp,    /* The Tcl interpreter to receive the
                             * variable .*/
    int enable)             /* Initial value. */
{
    static Boolean initialized = FALSE;

    if(!initialized) {
        initialized = TRUE;

        if (Tcl_CreateNamespace(interp, "::tk::mac", NULL, NULL) == NULL) {
            Tcl_ResetResult(interp);
        }
        if (Tcl_LinkVar(interp, "::tk::mac::antialiasedtext",
                (char *) &antialiasedTextEnabled,
                TCL_LINK_INT) != TCL_OK) {
            Tcl_ResetResult(interp);
        }
    }
    antialiasedTextEnabled = enable;
    return TCL_OK;
}
