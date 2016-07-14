/*
 * tkMacOSXScrollbar.c --
 *
 *	This file implements the Macintosh specific portion of the scrollbar
 *	widget.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright (c) 2015 Kevin Walzer/WordTech Commununications LLC.
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkInt.h"
#include "tkScrollbar.h"
#include "tkMacOSXPrivate.h"


#define MIN_SCROLLBAR_VALUE		0

/*Borrowed from ttkMacOSXTheme.c to provide appropriate scaling of scrollbar values.*/
#ifdef __LP64__
#define RangeToFactor(maximum) (((double) (INT_MAX >> 1)) / (maximum))
#else
#define RangeToFactor(maximum) (((double) (LONG_MAX >> 1)) / (maximum))
#endif /* __LP64__ */

#define MOUNTAIN_LION_STYLE (NSAppKitVersionNumber < 1138)

/*
 * Declaration of Mac specific scrollbar structure.
 */

typedef struct MacScrollbar {
    TkScrollbar information;	 /* Generic scrollbar info. */
    GC troughGC;		/* For drawing trough. */
    GC copyGC;			/* Used for copying from pixmap onto screen. */
} MacScrollbar;

/*
 * The class procedure table for the scrollbar widget. All fields except size
 * are left initialized to NULL, which should happen automatically since the
 * variable is declared at this scope.
 */

const Tk_ClassProcs tkpScrollbarProcs = {
    sizeof(Tk_ClassProcs),	/* size */
    NULL,					/* worldChangedProc */
    NULL,					/* createProc */
    NULL					/* modalProc */
};


/*Information on scrollbar layout, metrics, and draw info.*/
typedef struct ScrollbarMetrics {
    SInt32 width, minThumbHeight;
    int minHeight, topArrowHeight, bottomArrowHeight;
    NSControlSize controlSize;
} ScrollbarMetrics;

static ScrollbarMetrics metrics[2] = {
    {15, 54, 26, 14, 14, NSRegularControlSize}, /* kThemeScrollBarMedium */
    {11, 40, 20, 10, 10, NSSmallControlSize},  /* kThemeScrollBarSmall  */
};

HIThemeTrackDrawInfo info = {
    .version = 0,
    .min = 0.0,
    .max = 100.0,
    .attributes = kThemeTrackShowThumb,
    .kind = kThemeScrollBarMedium,
};


/*
 * Forward declarations for procedures defined later in this file:
 */

static void ScrollbarEventProc(ClientData clientData, XEvent *eventPtr);
static int ScrollbarPress(TkScrollbar *scrollPtr, XEvent *eventPtr);
static void UpdateControlValues(TkScrollbar  *scrollPtr);

/*
 *----------------------------------------------------------------------
 *
 * TkpCreateScrollbar --
 *
 *	Allocate a new TkScrollbar structure.
 *
 * Results:
 *	Returns a newly allocated TkScrollbar structure.
 *
 * Side effects:
 *	Registers an event handler for the widget.
 *
 *----------------------------------------------------------------------
 */

TkScrollbar *
TkpCreateScrollbar(
		   Tk_Window tkwin)
{

    MacScrollbar *scrollPtr = (MacScrollbar *)ckalloc(sizeof(MacScrollbar));

    scrollPtr->troughGC = None;
    scrollPtr->copyGC = None;

    Tk_CreateEventHandler(tkwin,ExposureMask|StructureNotifyMask|FocusChangeMask|ButtonPressMask|VisibilityChangeMask, ScrollbarEventProc, scrollPtr);

    return (TkScrollbar *) scrollPtr;
}

/*
 *--------------------------------------------------------------
 *
 * TkpDisplayScrollbar --
 *
 *	This procedure redraws the contents of a scrollbar window. It is
 *	invoked as a do-when-idle handler, so it only runs when there's
 *	nothing else for the application to do.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information appears on the screen.
 *
 *--------------------------------------------------------------
 */

void
TkpDisplayScrollbar(
		    ClientData clientData)	/* Information about window. */
{
    register TkScrollbar *scrollPtr = (TkScrollbar *) clientData;
    register Tk_Window tkwin = scrollPtr->tkwin;
    TkWindow *winPtr = (TkWindow *) tkwin;
    TkMacOSXDrawingContext dc;

    scrollPtr->flags &= ~REDRAW_PENDING;

    if (tkwin == NULL || !Tk_IsMapped(tkwin)) {
  	return;
    }

    MacDrawable *macWin =  (MacDrawable *) winPtr->window;
    NSView *view = TkMacOSXDrawableView(macWin);
    if (!view ||
	macWin->flags & TK_DO_NOT_DRAW ||
	!TkMacOSXSetupDrawingContext((Drawable) macWin, NULL, 1, &dc)) {
      return;
    }

    CGFloat viewHeight = [view bounds].size.height;
    CGAffineTransform t = { .a = 1, .b = 0, .c = 0, .d = -1, .tx = 0,
			    .ty = viewHeight};
    CGContextConcatCTM(dc.context, t);

    /*Draw Unix-style scroll trough to provide rect for native scrollbar.*/
    if (scrollPtr->highlightWidth != 0) {
    	GC fgGC, bgGC;

    	bgGC = Tk_GCForColor(scrollPtr->highlightBgColorPtr, (Pixmap) macWin);
    	if (scrollPtr->flags & GOT_FOCUS) {
    	    fgGC = Tk_GCForColor(scrollPtr->highlightColorPtr, (Pixmap) macWin);
    	} else {
    	    fgGC = bgGC;
    	}
    	TkpDrawHighlightBorder(tkwin, fgGC, bgGC, scrollPtr->highlightWidth,
    			       (Pixmap) macWin);
    }

    Tk_Draw3DRectangle(tkwin, (Pixmap) macWin, scrollPtr->bgBorder,
    		       scrollPtr->highlightWidth, scrollPtr->highlightWidth,
    		       Tk_Width(tkwin) - 2*scrollPtr->highlightWidth,
    		       Tk_Height(tkwin) - 2*scrollPtr->highlightWidth,
    		       scrollPtr->borderWidth, scrollPtr->relief);
    Tk_Fill3DRectangle(tkwin, (Pixmap) macWin, scrollPtr->bgBorder,
    		       scrollPtr->inset, scrollPtr->inset,
    		       Tk_Width(tkwin) - 2*scrollPtr->inset,
    		       Tk_Height(tkwin) - 2*scrollPtr->inset, 0, TK_RELIEF_FLAT);

    /*Update values and draw in native rect.*/
    UpdateControlValues(scrollPtr);
    if (MOUNTAIN_LION_STYLE) {
      HIThemeDrawTrack (&info, 0, dc.context, kHIThemeOrientationInverted);
    } else {
      HIThemeDrawTrack (&info, 0, dc.context, kHIThemeOrientationNormal);
    }
    TkMacOSXRestoreDrawingContext(&dc);

    scrollPtr->flags &= ~REDRAW_PENDING;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpComputeScrollbarGeometry --
 *
 *	After changes in a scrollbar's size or configuration, this procedure
 *	recomputes various geometry information used in displaying the
 *	scrollbar.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The scrollbar will be displayed differently.
 *
 *----------------------------------------------------------------------
 */

extern void
TkpComputeScrollbarGeometry(
			    register TkScrollbar *scrollPtr)
/* Scrollbar whose geometry may have
 * changed. */
{

    int variant, fieldLength;

    if (scrollPtr->highlightWidth < 0) {
    	scrollPtr->highlightWidth = 0;
    }
    scrollPtr->inset = scrollPtr->highlightWidth + scrollPtr->borderWidth;
    variant = ((scrollPtr->vertical ? Tk_Width(scrollPtr->tkwin) :
		Tk_Height(scrollPtr->tkwin)) - 2 * scrollPtr->inset
	       < metrics[0].width) ? 1 : 0;
    scrollPtr->arrowLength = (metrics[variant].topArrowHeight +
			      metrics[variant].bottomArrowHeight) / 2;
    fieldLength = (scrollPtr->vertical ? Tk_Height(scrollPtr->tkwin)
		   : Tk_Width(scrollPtr->tkwin))
	- 2 * (scrollPtr->arrowLength + scrollPtr->inset);
    if (fieldLength < 0) {
    	fieldLength = 0;
    }
    scrollPtr->sliderFirst = fieldLength * scrollPtr->firstFraction;
    scrollPtr->sliderLast = fieldLength * scrollPtr->lastFraction;

    /*
     * Adjust the slider so that some piece of it is always
     * displayed in the scrollbar and so that it has at least
     * a minimal width (so it can be grabbed with the mouse).
     */

    if (scrollPtr->sliderFirst > (fieldLength - 2*scrollPtr->borderWidth)) {
    	scrollPtr->sliderFirst = fieldLength - 2*scrollPtr->borderWidth;
    }
    if (scrollPtr->sliderFirst < 0) {
    	scrollPtr->sliderFirst = 0;
    }
    if (scrollPtr->sliderLast < (scrollPtr->sliderFirst +
				 metrics[variant].minThumbHeight)) {
    	scrollPtr->sliderLast = scrollPtr->sliderFirst +
	    metrics[variant].minThumbHeight;
    }
    if (scrollPtr->sliderLast > fieldLength) {
    	scrollPtr->sliderLast = fieldLength;
    }
    if (!(MOUNTAIN_LION_STYLE)) {
      scrollPtr->sliderFirst += scrollPtr->inset +
	metrics[variant].topArrowHeight;
      scrollPtr->sliderLast += scrollPtr->inset +
	metrics[variant].bottomArrowHeight;
    }
    /*
     * Register the desired geometry for the window (leave enough space
     * for the two arrows plus a minimum-size slider, plus border around
     * the whole window, if any). Then arrange for the window to be
     * redisplayed.
     */

    if (scrollPtr->vertical) {
    	Tk_GeometryRequest(scrollPtr->tkwin, scrollPtr->width + 2 * scrollPtr->inset, 2 * (scrollPtr->arrowLength + scrollPtr->borderWidth + scrollPtr->inset) +  metrics[variant].minThumbHeight);
    } else {
    	Tk_GeometryRequest(scrollPtr->tkwin, 2 * (scrollPtr->arrowLength + scrollPtr->borderWidth + scrollPtr->inset) + metrics[variant].minThumbHeight, scrollPtr->width + 2 * scrollPtr->inset);
    }
    Tk_SetInternalBorder(scrollPtr->tkwin, scrollPtr->inset);

}

/*
 *----------------------------------------------------------------------
 *
 * TkpDestroyScrollbar --
 *
 *	Free data structures associated with the scrollbar control.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the GCs associated with the scrollbar.
 *
 *----------------------------------------------------------------------
 */

void
TkpDestroyScrollbar(
		    TkScrollbar *scrollPtr)
{
    MacScrollbar *macScrollPtr = (MacScrollbar *)scrollPtr;

    if (macScrollPtr->troughGC != None) {
	Tk_FreeGC(scrollPtr->display, macScrollPtr->troughGC);
    }
    if (macScrollPtr->copyGC != None) {
	Tk_FreeGC(scrollPtr->display, macScrollPtr->copyGC);
    }

    macScrollPtr=NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpConfigureScrollbar --
 *
 *	This procedure is called after the generic code has finished
 *	processing configuration options, in order to configure platform
 *	specific options.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Configuration info may get changed.
 *
 *----------------------------------------------------------------------
 */

void
TkpConfigureScrollbar(
		      register TkScrollbar *scrollPtr)
/* Information about widget; may or may not
 * already have values for some fields. */
{

}

/*
 *--------------------------------------------------------------
 *
 * TkpScrollbarPosition --
 *
 *	Determine the scrollbar element corresponding to a given position.
 *
 * Results:
 *	One of TOP_ARROW, TOP_GAP, etc., indicating which element of the
 *	scrollbar covers the position given by (x, y). If (x,y) is outside the
 *	scrollbar entirely, then OUTSIDE is returned.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
TkpScrollbarPosition(
    register TkScrollbar *scrollPtr,
				/* Scrollbar widget record. */
    int x, int y)		/* Coordinates within scrollPtr's window. */
{

  /*
   * Using code from tkUnixScrlbr.c because Unix scroll bindings are
   * driving the display at the script level. All the Mac scrollbar
   * has to do is re-draw itself.
   */

    int length, fieldlength, width, tmp;
    register const int inset = scrollPtr->inset;
    register const int arrowSize = scrollPtr->arrowLength + inset;

    if (scrollPtr->vertical) {
	length = Tk_Height(scrollPtr->tkwin);
	fieldlength = length - 2 * arrowSize;
	width = Tk_Width(scrollPtr->tkwin);
    } else {
	tmp = x;
	x = y;
	y = tmp;
	length = Tk_Width(scrollPtr->tkwin);
	fieldlength = length - 2 * arrowSize;
	width = Tk_Height(scrollPtr->tkwin);
    }
    fieldlength = fieldlength < 0 ? 0 : fieldlength;

    if (x<inset || x>=width-inset || y<inset || y>=length-inset) {
	return OUTSIDE;
    }

    /*
     * All of the calculations in this procedure mirror those in
     * TkpDisplayScrollbar. Be sure to keep the two consistent.
     */

    if (y < scrollPtr->sliderFirst) {
	return TOP_GAP;
    }
    if (y < scrollPtr->sliderLast) {
	return SLIDER;
    }
    if (y < fieldlength){
	return BOTTOM_GAP;
    }
    if (y < fieldlength + arrowSize) {
	return TOP_ARROW;
    }
    return BOTTOM_ARROW;
}

/*
 *--------------------------------------------------------------
 *
 * UpdateControlValues --
 *
 *	This procedure updates the Macintosh scrollbar control to
 *	display the values defined by the Tk scrollbar. This is the
 *	key interface to the Mac-native * scrollbar; the Unix bindings
 *	drive scrolling in the Tk window and all the Mac scrollbar has
 *	to do is redraw itself.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Macintosh control is updated.
 *
 *--------------------------------------------------------------
 */

static void
UpdateControlValues(
		    TkScrollbar *scrollPtr)		/* Scrollbar data struct. */
{
    Tk_Window tkwin = scrollPtr->tkwin;
    MacDrawable *macWin = (MacDrawable *) Tk_WindowId(scrollPtr->tkwin);
    double dViewSize;
    HIRect  contrlRect;
    int variant;
    short width, height;

    NSView *view = TkMacOSXDrawableView(macWin);
    CGFloat viewHeight = [view bounds].size.height;
    NSRect frame;
    frame = NSMakeRect(macWin->xOff, macWin->yOff, Tk_Width(tkwin),
		       Tk_Height(tkwin));
    frame = NSInsetRect(frame, scrollPtr->inset, scrollPtr->inset);
    frame.origin.y = viewHeight - (frame.origin.y + frame.size.height);

    contrlRect = NSRectToCGRect(frame);
    info.bounds = contrlRect;

    width = contrlRect.size.width;
    height = contrlRect.size.height;

    variant = contrlRect.size.width < metrics[0].width ? 1 : 0;

    /*
     * Ensure we set scrollbar control bounds only once all size adjustments
     * have been computed.
     */

    info.bounds = contrlRect;
    if (scrollPtr->vertical) {
      info.attributes &= ~kThemeTrackHorizontal;
    } else {
      info.attributes |= kThemeTrackHorizontal;
    }

    /*
     * Given the Tk parameters for the fractions of the start and end of the
     * thumb, the following calculation determines the location for the
     * Macintosh thumb. The Aqua scroll control works as follows. The
     * scrollbar's value is the position of the left (or top) side of the view
     * area in the content area being scrolled. The maximum value of the
     * control is therefore the dimension of the content area less the size of
     * the view area.
     */

    double maximum = 100,  factor;
    factor = RangeToFactor(maximum);
    dViewSize = (scrollPtr->lastFraction - scrollPtr->firstFraction)
	* factor;
    info.max =  MIN_SCROLLBAR_VALUE +
	factor - dViewSize;
    info.trackInfo.scrollbar.viewsize = dViewSize;
    if (scrollPtr->vertical) {
      if (MOUNTAIN_LION_STYLE) {
	info.value = factor * scrollPtr->firstFraction;
      } else {
	info.value = info.max - factor * scrollPtr->firstFraction;
      }
    } else {
	 info.value =  MIN_SCROLLBAR_VALUE + factor * scrollPtr->firstFraction;
    }

    if((scrollPtr->firstFraction <= 0.0 && scrollPtr->lastFraction >= 1.0)
       || height <= metrics[variant].minHeight) {
    	info.enableState = kThemeTrackHideTrack;
    } else {
    	info.enableState = kThemeTrackActive;
    	info.attributes = kThemeTrackShowThumb |  kThemeTrackThumbRgnIsNotGhost;
    }

}

/*
 *--------------------------------------------------------------
 *
 * ScrollbarPress --
 *
 *	This procedure is invoked in response to <ButtonPress> events.
 *	Enters a modal loop to handle scrollbar interactions.
 *
 *--------------------------------------------------------------
 */

static int
ScrollbarPress(TkScrollbar *scrollPtr, XEvent *eventPtr)
{

  if (eventPtr->type == ButtonPress) {
    UpdateControlValues(scrollPtr);
  }
  return TCL_OK;
}



/*
 *--------------------------------------------------------------
 *
 * ScrollbarEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various events on
 *	scrollbars.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get cleaned up. When
 *	it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
ScrollbarEventProc(
		   ClientData clientData,	/* Information about window. */
		   XEvent *eventPtr)		/* Information about event. */
{
    TkScrollbar *scrollPtr = clientData;

    switch (eventPtr->type) {
    case UnmapNotify:
	TkMacOSXSetScrollbarGrow((TkWindow *) scrollPtr->tkwin, false);
	break;
    case ActivateNotify:
    case DeactivateNotify:
	TkScrollbarEventuallyRedraw(scrollPtr);
	break;
    case ButtonPress:
    	ScrollbarPress(clientData, eventPtr);
	break;
    default:
	TkScrollbarEventProc(clientData, eventPtr);
    }
}
