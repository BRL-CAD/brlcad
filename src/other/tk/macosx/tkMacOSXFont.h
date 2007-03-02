/*
 * tkMacOSXFont.h --
 *
 *      Private functions and structs exported from tkMacOSXFont.c
 *      for use in ATSU specific extensions.
 *
 * Copyright 2002-2004 Benjamin Riefenstahl, Benjamin.Riefenstahl@epost.de
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifndef	TKMACOSXFONT_H
#define	TKMACOSXFONT_H	1

#include "tkFont.h"

#ifndef _TKMACINT
#include "tkMacOSXInt.h"
#endif

/*
 * Switches
 */

#define TK_MAC_USE_QUARZ 1

/*
 * Types
 */

/*
 * The following structure represents our Macintosh-specific implementation
 * of a font object.
 */

typedef struct {
    TkFont font;                /* Stuff used by generic font package.  Must
                                 * be first in structure. */

    /*
     * The ATSU view of the font and other text properties.  Used for drawing
     * and measuring.
     */

    ATSUFontID atsuFontId;      /* == FMFont. */
    ATSUTextLayout atsuLayout;  /* ATSU layout object, representing the whole
                                 * text that ATSU sees with some option
                                 * bits. */
    ATSUStyle atsuStyle;        /* ATSU style object, representing a run of
                                 * text with the same properties. */

    /*
     * The QuickDraw view of the font.  Used to configure controls.
     */

    FMFontFamily qdFont;        /* == FMFontFamilyId, Carbon replacement for
                                 * QD face numbers. */
    short qdSize;               /* Font size in points. */
    short qdStyle;              /* QuickDraw style bits. */
} TkMacOSXFont;


#if TK_MAC_USE_QUARZ

/*
 * To use Quarz drawing we need some additional context.  FIXME: We would
 * have liked to use the similar functions from tkMacOSXDraw.c to do this
 * (TkMacOSXSetUpCGContext(), etc), but a) those don't quite work for us
 * (e.g. we can't use a simple upside-down coordinate system transformation,
 * as we don't want upside-down characters ;-), and b) we don't have the
 * necessary context information (MacDrawable), that we need as parameter for
 * those functions.  So I just cobbled together a limited edition, getting
 * the necessary parameters from the current QD GraphPort.
 */

typedef struct {
    CGContextRef cgContext;     /* Quarz context. */
    CGrafPtr graphPort;         /* QD graph port to which this belongs.
                                 * Needed for releasing cgContext. */
    Rect portRect;              /* Cached size of port. */
} TkMacOSXFontDrawingContext;

#else /* ! TK_MAC_USE_QUARZ */

/*
 * Just a dummy, so we don't have to #ifdef the parameter lists of functions
 * that use this.
 */

typedef struct {} DrawingContext;

#endif /* ? TK_MAC_USE_QUARZ */

/*
 * Function prototypes
 */

MODULE_SCOPE void	TkMacOSXLayoutSetString(const TkMacOSXFont * fontPtr,
			    const TkMacOSXFontDrawingContext *drawingContextPtr,
			    const UniChar * uchars, int ulen);
MODULE_SCOPE void	TkMacOSXInitControlFontStyle(Tk_Font tkfont,
			    ControlFontStylePtr fsPtr);

#if TK_MAC_USE_QUARZ
MODULE_SCOPE void	TkMacOSXQuarzStartDraw(
			    TkMacOSXFontDrawingContext * contextPtr);
MODULE_SCOPE void	TkMacOSXQuarzEndDraw(
			    TkMacOSXFontDrawingContext * contextPtr);
#endif /* TK_MAC_USE_QUARZ */

#endif	/*TKMACOSXFONT_H*/
