/*
 * tkMacOSXScrollbar.c --
 *
 *	This file implements the Macintosh specific portion of the scrollbar
 *	widget.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkScrollbar.h"

/*
#ifdef TK_MAC_DEBUG
#define TK_MAC_DEBUG_SCROLLBAR
#endif
*/

/*
 * Declaration of Mac specific scrollbar structure.
 */

typedef struct MacScrollbar {
    TkScrollbar info;
    NSScroller	*scroller;
    int variant;
} MacScrollbar;

typedef struct ScrollbarMetrics {
    SInt32 width, minThumbHeight;
    int minHeight, topArrowHeight, bottomArrowHeight;
    NSControlSize controlSize;
} ScrollbarMetrics;

static ScrollbarMetrics metrics[2] = {
    {15, 54, 26, 14, 14, NSRegularControlSize}, /* kThemeScrollBarMedium */
    {11, 40, 20, 10, 10, NSSmallControlSize},  /* kThemeScrollBarSmall  */
};

/*
 * Declarations for functions defined in this file.
 */

static void		UpdateScrollbarMetrics(void);
static void		ScrollbarEventProc(ClientData clientData,
			    XEvent *eventPtr);

/*
 * The class procedure table for the scrollbar widget.
 */

Tk_ClassProcs tkpScrollbarProcs = {
    sizeof(Tk_ClassProcs),	/* size */
    NULL,					/* worldChangedProc */
    NULL,					/* createProc */
    NULL					/* modalProc */
};

#pragma mark TKApplication(TKScrlbr)

#define NSAppleAquaScrollBarVariantChanged @"AppleAquaScrollBarVariantChanged"

@implementation TKApplication(TKScrlbr)
- (void) tkScroller: (NSScroller *) scroller
{
    NSScrollerPart hitPart = [scroller hitPart];
    TkScrollbar *scrollPtr = (TkScrollbar *)[scroller tag];
    Tcl_DString cmdString;
    Tcl_Interp *interp;
    int result;

    if (!scrollPtr || !scrollPtr->command || !scrollPtr->commandSize ||
	    hitPart == NSScrollerNoPart) {
	return;
    }

    Tcl_DStringInit(&cmdString);
    Tcl_DStringAppend(&cmdString, scrollPtr->command,
	    scrollPtr->commandSize);
    switch (hitPart) {
    case NSScrollerKnob:
    case NSScrollerKnobSlot: {
	char valueString[TCL_DOUBLE_SPACE];

	Tcl_PrintDouble(NULL, [scroller doubleValue] *
		(1.0 - [scroller knobProportion]), valueString);
	Tcl_DStringAppendElement(&cmdString, "moveto");
	Tcl_DStringAppendElement(&cmdString, valueString);
	break;
    }
    case NSScrollerDecrementLine:
    case NSScrollerIncrementLine:
	Tcl_DStringAppendElement(&cmdString, "scroll");
	Tcl_DStringAppendElement(&cmdString,
		(hitPart == NSScrollerDecrementLine) ? "-1" : "1");
	Tcl_DStringAppendElement(&cmdString, "unit");
	break;
    case NSScrollerDecrementPage:
    case NSScrollerIncrementPage:
	Tcl_DStringAppendElement(&cmdString, "scroll");
	Tcl_DStringAppendElement(&cmdString,
		(hitPart == NSScrollerDecrementPage) ? "-1" : "1");
	Tcl_DStringAppendElement(&cmdString, "page");
	break;
    }
    interp = scrollPtr->interp;
    Tcl_Preserve(interp);
    Tcl_Preserve(scrollPtr);
    result = Tcl_EvalEx(interp, Tcl_DStringValue(&cmdString),
	    Tcl_DStringLength(&cmdString), TCL_EVAL_GLOBAL);
    if (result != TCL_OK && result != TCL_CONTINUE && result != TCL_BREAK) {
	Tcl_AddErrorInfo(interp, "\n    (scrollbar command)");
	Tcl_BackgroundError(interp);
    }
    Tcl_Release(scrollPtr);
    Tcl_Release(interp);
    Tcl_DStringFree(&cmdString);
#ifdef TK_MAC_DEBUG_SCROLLBAR
    TKLog(@"scroller %s value %f knobProportion %f",
	    ((TkWindow *)scrollPtr->tkwin)->pathName, [scroller doubleValue],
	    [scroller knobProportion]);
#endif
}

- (void) scrollBarVariantChanged: (NSNotification *) notification
{
#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
#endif
    UpdateScrollbarMetrics();
}

- (void) _setupScrollBarNotifications
{
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];

#define observe(n, s) [nc addObserver:self selector:@selector(s) name:(n) object:nil]
    observe(NSAppleAquaScrollBarVariantChanged, scrollBarVariantChanged:);
#undef observe

    UpdateScrollbarMetrics();
}
@end

#pragma mark -

/*
 *----------------------------------------------------------------------
 *
 * UpdateScrollbarMetrics --
 *
 *	This function retrieves the current system metrics for a scrollbar.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Updates the geometry cache info for all scrollbars.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateScrollbarMetrics(void)
{
    const short height = 100, width = 50;
    HIThemeTrackDrawInfo info = {
	.version = 0,
	.bounds = {{0, 0}, {width, height}},
	.min = 0,
	.max = 1,
	.value = 0,
	.attributes = kThemeTrackShowThumb,
	.enableState = kThemeTrackActive,
	.trackInfo.scrollbar = {.viewsize = 1, .pressState = 0},
    };
    CGRect bounds;

    ChkErr(GetThemeMetric, kThemeMetricScrollBarWidth, &metrics[0].width);
    ChkErr(GetThemeMetric, kThemeMetricScrollBarMinThumbHeight,
	    &metrics[0].minThumbHeight);
    info.kind = kThemeScrollBarMedium;
    ChkErr(HIThemeGetTrackDragRect, &info, &bounds);
    metrics[0].topArrowHeight = bounds.origin.y;
    metrics[0].bottomArrowHeight = height - (bounds.origin.y +
	    bounds.size.height);
    metrics[0].minHeight = metrics[0].minThumbHeight +
	    metrics[0].topArrowHeight + metrics[0].bottomArrowHeight;
    ChkErr(GetThemeMetric, kThemeMetricSmallScrollBarWidth, &metrics[1].width);
    ChkErr(GetThemeMetric, kThemeMetricSmallScrollBarMinThumbHeight,
	    &metrics[1].minThumbHeight);
    info.kind = kThemeScrollBarSmall;
    ChkErr(HIThemeGetTrackDragRect, &info, &bounds);
    metrics[1].topArrowHeight = bounds.origin.y;
    metrics[1].bottomArrowHeight = height - (bounds.origin.y +
	    bounds.size.height);
    metrics[1].minHeight = metrics[1].minThumbHeight +
	    metrics[1].topArrowHeight + metrics[1].bottomArrowHeight;

    sprintf(tkDefScrollbarWidth, "%d", (int)(metrics[0].width));
}

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
    MacScrollbar *scrollPtr = (MacScrollbar *) ckalloc(sizeof(MacScrollbar));

    scrollPtr->scroller = nil;
    Tk_CreateEventHandler(tkwin, StructureNotifyMask|FocusChangeMask|
	    ActivateMask|ExposureMask, ScrollbarEventProc, (ClientData) scrollPtr);
    return (TkScrollbar *) scrollPtr;
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
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkpDestroyScrollbar(
    TkScrollbar *scrollPtr)
{
    MacScrollbar *macScrollPtr = (MacScrollbar *) scrollPtr;

    TkMacOSXMakeCollectableAndRelease(macScrollPtr->scroller);
}

/*
 *--------------------------------------------------------------
 *
 * TkpDisplayScrollbar --
 *
 *	This procedure redraws the contents of a scrollbar window. It is
 *	invoked as a do-when-idle handler, so it only runs when there's nothing
 *	else for the application to do.
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
    TkScrollbar *scrollPtr = (TkScrollbar *) clientData;
    MacScrollbar *macScrollPtr = (MacScrollbar *) clientData;
    NSScroller *scroller = macScrollPtr->scroller;
    Tk_Window tkwin = scrollPtr->tkwin;
    TkWindow *winPtr = (TkWindow *) tkwin;
    MacDrawable *macWin =  (MacDrawable *) winPtr->window;
    TkMacOSXDrawingContext dc;
    NSView *view = TkMacOSXDrawableView(macWin);
    CGFloat viewHeight = [view bounds].size.height;
    CGAffineTransform t = { .a = 1, .b = 0, .c = 0, .d = -1, .tx = 0,
	    .ty = viewHeight};
    NSRect frame;
    double knobProportion = scrollPtr->lastFraction - scrollPtr->firstFraction;

    scrollPtr->flags &= ~REDRAW_PENDING;
    if (!scrollPtr->tkwin || !Tk_IsMapped(tkwin) || !view ||
	    !TkMacOSXSetupDrawingContext((Drawable) macWin, NULL, 1, &dc)) {
	return;
    }
    CGContextConcatCTM(dc.context, t);
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
    if ([scroller superview] != view) {
	[view addSubview:scroller];
    }
    frame = NSMakeRect(macWin->xOff, macWin->yOff, Tk_Width(tkwin),
	    Tk_Height(tkwin));
    frame = NSInsetRect(frame, scrollPtr->inset, scrollPtr->inset);
    frame.origin.y = viewHeight - (frame.origin.y + frame.size.height);

    NSWindow *w = [view window];

    if ([w showsResizeIndicator]) {
	NSRect growBox = [view convertRect:[w _growBoxRect] fromView:nil];

	if (NSIntersectsRect(growBox, frame)) {
	    if (scrollPtr->vertical) {
		CGFloat y = frame.origin.y;

		frame.origin.y = growBox.origin.y + growBox.size.height;
		frame.size.height -= frame.origin.y - y;
 	    } else {
		frame.size.width = growBox.origin.x - frame.origin.x;
	    }
	    TkMacOSXSetScrollbarGrow(winPtr, true);
	}
    }
    if (!NSEqualRects(frame, [scroller frame])) {
	[scroller setFrame:frame];
    }
    [scroller setEnabled:(knobProportion < 1.0 &&
	    (scrollPtr->vertical ? frame.size.height : frame.size.width) >
	    metrics[macScrollPtr->variant].minHeight)];
    [scroller setDoubleValue:scrollPtr->firstFraction / (1.0 - knobProportion)];
    [scroller setKnobProportion:knobProportion];
    [scroller displayRectIgnoringOpacity:[scroller bounds]];
    TkMacOSXRestoreDrawingContext(&dc);
#ifdef TK_MAC_DEBUG_SCROLLBAR
    TKLog(@"scroller %s frame %@ width %d height %d",
	    ((TkWindow *)scrollPtr->tkwin)->pathName, NSStringFromRect(frame),
	    Tk_Width(tkwin), Tk_Height(tkwin));
#endif
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

void
TkpComputeScrollbarGeometry(
    register TkScrollbar *scrollPtr)
				/* Scrollbar whose geometry may have
				 * changed. */
{
    MacScrollbar *macScrollPtr = (MacScrollbar *) scrollPtr;
    NSScroller *scroller = macScrollPtr->scroller;
    int width, height, variant, fieldLength;

    if (scrollPtr->highlightWidth < 0) {
	scrollPtr->highlightWidth = 0;
    }
    scrollPtr->inset = scrollPtr->highlightWidth + scrollPtr->borderWidth;
    width = Tk_Width(scrollPtr->tkwin) - 2 * scrollPtr->inset;
    height = Tk_Height(scrollPtr->tkwin) - 2 * scrollPtr->inset;
    variant = ((scrollPtr->vertical ?  width : height) < metrics[0].width) ?
	    1 : 0;
    macScrollPtr->variant = variant;
    if (scroller) {
	NSSize size = [scroller frame].size;

	if ((size.width > size.height) ^ !scrollPtr->vertical) {
	    /*
	     * Orientation changed, need new scroller.
	     */

	    if ([scroller superview]) {
		[scroller removeFromSuperviewWithoutNeedingDisplay];
	    }
	    TkMacOSXMakeCollectableAndRelease(scroller);
	}
    }
    if (!scroller) {
	if ((width > height) ^ !scrollPtr->vertical) {
	    /* -[NSScroller initWithFrame:] determines horizonalness for the
	     * lifetime of the scroller via isHoriz = (width > height) */
	    if (scrollPtr->vertical) {
		width = height;
	    } else if (width > 1) {
		height = width - 1;
	    } else {
		height = 1;
		width = 2;
	    }
	}
	scroller = [[NSScroller alloc] initWithFrame:
		NSMakeRect(0, 0, width, height)];
	macScrollPtr->scroller = TkMacOSXMakeUncollectable(scroller);
	[scroller setAction:@selector(tkScroller:)];
	[scroller setTarget:NSApp];
	[scroller setTag:(NSInteger)scrollPtr];
    }
    [[scroller cell] setControlSize:metrics[variant].controlSize];

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
     * Adjust the slider so that some piece of it is always displayed in the
     * scrollbar and so that it has at least a minimal width (so it can be
     * grabbed with the mouse).
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
    scrollPtr->sliderFirst += scrollPtr->inset +
	    metrics[variant].topArrowHeight;
    scrollPtr->sliderLast += scrollPtr->inset +
	    metrics[variant].bottomArrowHeight;

    /*
     * Register the desired geometry for the window (leave enough space for
     * the two arrows plus a minimum-size slider, plus border around the whole
     * window, if any). Then arrange for the window to be redisplayed.
     */

    if (scrollPtr->vertical) {
	Tk_GeometryRequest(scrollPtr->tkwin, scrollPtr->width +
		2 * scrollPtr->inset, 2 * (scrollPtr->arrowLength +
		scrollPtr->borderWidth + scrollPtr->inset) +
		metrics[variant].minThumbHeight);
    } else {
	Tk_GeometryRequest(scrollPtr->tkwin, 2 * (scrollPtr->arrowLength +
		scrollPtr->borderWidth + scrollPtr->inset) +
		metrics[variant].minThumbHeight, scrollPtr->width +
		2 * scrollPtr->inset);
    }
    Tk_SetInternalBorder(scrollPtr->tkwin, scrollPtr->inset);
#ifdef TK_MAC_DEBUG_SCROLLBAR
    TKLog(@"scroller %s bounds %@ width %d height %d inset %d borderWidth %d",
	    ((TkWindow *)scrollPtr->tkwin)->pathName,
	    NSStringFromRect([scroller bounds]),
	    width, height, scrollPtr->inset, scrollPtr->borderWidth);
#endif
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
 *	None.
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
    NSScroller *scroller = ((MacScrollbar *) scrollPtr)->scroller;
    MacDrawable *macWin =  (MacDrawable *)
	    ((TkWindow *) scrollPtr->tkwin)->window;
    NSView *view = TkMacOSXDrawableView(macWin);

    switch ([scroller testPart:NSMakePoint(macWin->xOff + x,
	    [view bounds].size.height - (macWin->yOff + y))]) {
    case NSScrollerDecrementLine:
	return TOP_ARROW;
    case NSScrollerDecrementPage:
	return TOP_GAP;
    case NSScrollerKnob:
	return SLIDER;
    case NSScrollerIncrementPage:
	return BOTTOM_GAP;
    case NSScrollerIncrementLine:
	return BOTTOM_ARROW;
    case NSScrollerKnobSlot:
    case NSScrollerNoPart:
    default:
	return OUTSIDE;
    }
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
    TkScrollbar *scrollPtr = (TkScrollbar *) clientData;

    switch (eventPtr->type) {
    case UnmapNotify:
	TkMacOSXSetScrollbarGrow((TkWindow *) scrollPtr->tkwin, false);
	break;
    case ActivateNotify:
    case DeactivateNotify:
	TkScrollbarEventuallyRedraw((ClientData) scrollPtr);
	break;
    default:
	TkScrollbarEventProc(clientData, eventPtr);
    }
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
