/*
 * tkMacOSXMenubutton.c --
 *
 *	This file implements the Macintosh specific portion of the
 *	menubutton widget.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkMenubutton.h"
#include "tkMacOSXFont.h"
#include "tkMacOSXDebug.h"

/*
#ifdef TK_MAC_DEBUG
#define TK_MAC_DEBUG_MENUBUTTON
#endif
*/

typedef struct MacMenuButton {
    TkMenuButton info;
    NSPopUpButton *button;
#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS
    int fix;
#endif
} MacMenuButton;

#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS

/*
 * Use the following heuristic conversion constants to make NSButton-based
 * widget metrics match up with the old Carbon control buttons (for the
 * default Lucida Grande 13 font).
 * TODO: provide a scriptable way to turn this off and use the raw NSButton
 *       metrics (will also need dynamic adjustment of the default padding,
 *       c.f. tkMacOSXDefault.h).
 */

typedef struct {
    int trimW, trimH, inset, shrinkW, offsetX, offsetY;
} BoundsFix;

#define fixForStyle(style) ( \
	style == NSRoundedBezelStyle ? 1 : \
	style == NSRegularSquareBezelStyle ? 2 : \
	style == NSShadowlessSquareBezelStyle ? 3 : \
	INT_MIN)

static const BoundsFix boundsFixes[] = {
    [fixForStyle(NSRoundedBezelStyle)] =	    { 14, 10, -2, -1},
    [fixForStyle(NSRegularSquareBezelStyle)] =	    {  6, 13, -2,  1, 1},
    [fixForStyle(NSShadowlessSquareBezelStyle)] =   { 15,  0, 2 },
};

#endif

/*
 * Forward declarations for procedures defined later in this file:
 */

static void MenuButtonEventProc(ClientData clientData, XEvent *eventPtr);

/*
 * The structure below defines menubutton class behavior by means of functions
 * that can be invoked from generic window code.
 */

Tk_ClassProcs tkpMenubuttonClass = {
    sizeof(Tk_ClassProcs),	/* size */
    TkMenuButtonWorldChanged,	/* worldChangedProc */
};

/*
 *----------------------------------------------------------------------
 *
 * TkpCreateMenuButton --
 *
 *	Allocate a new TkMenuButton structure.
 *
 * Results:
 *	Returns a newly allocated TkMenuButton structure.
 *
 * Side effects:
 *	Registers an event handler for the widget.
 *
 *----------------------------------------------------------------------
 */

TkMenuButton *
TkpCreateMenuButton(
    Tk_Window tkwin)
{
    MacMenuButton *macButtonPtr =
	    (MacMenuButton *) ckalloc(sizeof(MacMenuButton));

    macButtonPtr->button = nil;

    Tk_CreateEventHandler(tkwin, ActivateMask,
	    MenuButtonEventProc, (ClientData) macButtonPtr);
    return (TkMenuButton *) macButtonPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpDestroyMenuButton --
 *
 *	Free data structures associated with the menubutton control.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Restores the default control state.
 *
 *----------------------------------------------------------------------
 */

void
TkpDestroyMenuButton(
    TkMenuButton *mbPtr)
{
    MacMenuButton *macButtonPtr = (MacMenuButton *) mbPtr;

    TkMacOSXMakeCollectableAndRelease(macButtonPtr->button);
}

/*
 *----------------------------------------------------------------------
 *
 * TkpDisplayMenuButton --
 *
 *	This function is invoked to display a menubutton widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the menubutton in its current
 *	mode.
 *
 *----------------------------------------------------------------------
 */

void
TkpDisplayMenuButton(
    ClientData clientData)	/* Information about widget. */
{
    TkMenuButton *mbPtr = (TkMenuButton *) clientData;
    MacMenuButton *macButtonPtr = (MacMenuButton *) mbPtr;
    NSPopUpButton *button = macButtonPtr->button;
    Tk_Window tkwin = mbPtr->tkwin;
    TkWindow *winPtr = (TkWindow *) tkwin;
    MacDrawable *macWin =  (MacDrawable *) winPtr->window;
    TkMacOSXDrawingContext dc;
    NSView *view = TkMacOSXDrawableView(macWin);
    CGFloat viewHeight = [view bounds].size.height;
    CGAffineTransform t = { .a = 1, .b = 0, .c = 0, .d = -1, .tx = 0,
	    .ty = viewHeight};
    NSRect frame;
    int enabled;

    mbPtr->flags &= ~REDRAW_PENDING;
    if (!tkwin || !Tk_IsMapped(tkwin) || !view ||
	    !TkMacOSXSetupDrawingContext((Drawable) macWin, NULL, 1, &dc)) {
	return;
    }
    CGContextConcatCTM(dc.context, t);
    Tk_Fill3DRectangle(tkwin, (Pixmap) macWin, mbPtr->normalBorder, 0, 0,
	    Tk_Width(tkwin), Tk_Height(tkwin), 0, TK_RELIEF_FLAT);
    if ([button superview] != view) {
	[view addSubview:button];
    }
    enabled = !(mbPtr->state == STATE_DISABLED);
    [button setEnabled:enabled];
    if (enabled) {
	[[button cell] setHighlighted:(mbPtr->state == STATE_ACTIVE)];
    }
    frame = NSMakeRect(macWin->xOff, macWin->yOff, Tk_Width(tkwin),
	    Tk_Height(tkwin));
    frame = NSInsetRect(frame, mbPtr->inset, mbPtr->inset);
#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS
    if (tkMacOSXUseCompatibilityMetrics) {
	BoundsFix boundsFix = boundsFixes[macButtonPtr->fix];
	frame = NSOffsetRect(frame, boundsFix.offsetX, boundsFix.offsetY);
	frame.size.width -= boundsFix.shrinkW;
	frame = NSInsetRect(frame, boundsFix.inset, boundsFix.inset);
    }
#endif
    frame.origin.y = viewHeight - (frame.origin.y + frame.size.height);
    if (!NSEqualRects(frame, [button frame])) {
	[button setFrame:frame];
    }
    [button displayRectIgnoringOpacity:[button bounds]];
    TkMacOSXRestoreDrawingContext(&dc);
#ifdef TK_MAC_DEBUG_MENUBUTTON
    TKLog(@"menubutton %s frame %@ width %d height %d",
	    ((TkWindow *)mbPtr->tkwin)->pathName, NSStringFromRect(frame),
	    Tk_Width(tkwin), Tk_Height(tkwin));
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TkpComputeMenuButtonGeometry --
 *
 *	After changes in a menu button's text or bitmap, this function
 *	recomputes the menu button's geometry and passes this information
 *	along to the geometry manager for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The menu button's window may change size.
 *
 *----------------------------------------------------------------------
 */

void
TkpComputeMenuButtonGeometry(
    TkMenuButton *mbPtr)	/* Widget record for menu button. */
{
    MacMenuButton *macButtonPtr = (MacMenuButton *) mbPtr;
    NSPopUpButton *button = macButtonPtr->button;
    NSPopUpButtonCell *cell;
    NSMenuItem *menuItem;
    NSBezelStyle style = NSRoundedBezelStyle;
    NSFont *font;
    NSRect bounds = NSZeroRect, titleRect = NSZeroRect;
    int haveImage = (mbPtr->image || mbPtr->bitmap != None), haveText = 0;
    int haveCompound = (mbPtr->compound != COMPOUND_NONE);
    int width, height;

    if (!button) {
	button = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:YES];
	macButtonPtr->button = TkMacOSXMakeUncollectable(button);
	cell = [button cell];
	[cell setUsesItemFromMenu:NO];
	menuItem = [[[NSMenuItem alloc] initWithTitle:@""
		action:NULL keyEquivalent:@""] autorelease];
	[cell setMenuItem:menuItem];
    } else {
	cell = [button cell];
	menuItem = [cell menuItem];
    }
    if (haveImage) {
	style = NSShadowlessSquareBezelStyle;
    } else if (!mbPtr->indicatorOn) {
	style = NSRegularSquareBezelStyle;
    }
    [button setBezelStyle:style];
    [cell setArrowPosition:(mbPtr->indicatorOn ? NSPopUpArrowAtBottom :
	    NSPopUpNoArrow)];
#if 0
    NSControlSize controlSize = NSRegularControlSize;

    if (mbPtr->borderWidth <= 2) {
	controlSize = NSMiniControlSize;
    } else if (mbPtr->borderWidth == 3) {
	controlSize = NSSmallControlSize;
    }
    [cell setControlSize:controlSize];
#endif

    if (mbPtr->text && *(mbPtr->text) && (!haveImage || haveCompound)) {
	NSString *title = [[NSString alloc] initWithUTF8String:mbPtr->text];
	[button setTitle:title];
	[title release];
	haveText = 1;
    }
    haveCompound = (haveCompound && haveImage && haveText);
    if (haveText) {
	NSTextAlignment alignment = NSNaturalTextAlignment;

	switch (mbPtr->justify) {
	case TK_JUSTIFY_LEFT:
	    alignment = NSLeftTextAlignment;
	    break;
	case TK_JUSTIFY_RIGHT:
	    alignment = NSRightTextAlignment;
	    break;
	case TK_JUSTIFY_CENTER:
	    alignment = NSCenterTextAlignment;
	    break;
	}
	[button setAlignment:alignment];
    } else {
	[button setTitle:@""];
    }
    font = TkMacOSXNSFontForFont(mbPtr->tkfont);
    if (font) {
	[button setFont:font];
    }
    if (haveImage) {
	int width, height;
	NSImage *image;
	NSCellImagePosition pos = NSImageOnly;

	if (mbPtr->image) {
	    Tk_SizeOfImage(mbPtr->image, &width, &height);
	    image = TkMacOSXGetNSImageWithTkImage(mbPtr->display,
		    mbPtr->image, width, height);
	} else {
	    Tk_SizeOfBitmap(mbPtr->display, mbPtr->bitmap, &width, &height);
	    image = TkMacOSXGetNSImageWithBitmap(mbPtr->display,
		    mbPtr->bitmap, mbPtr->normalTextGC, width, height);
	}
	if (haveCompound) {
	    switch ((enum compound) mbPtr->compound) {
		case COMPOUND_TOP:
		    pos = NSImageAbove;
		    break;
		case COMPOUND_BOTTOM:
		    pos = NSImageBelow;
		    break;
		case COMPOUND_LEFT:
		    pos = NSImageLeft;
		    break;
		case COMPOUND_RIGHT:
		    pos = NSImageRight;
		    break;
		case COMPOUND_CENTER:
		    pos = NSImageOverlaps;
		    break;
		case COMPOUND_NONE:
		    pos = NSImageOnly;
		    break;
	    }
	}
	[button setImagePosition:pos];
	[menuItem setImage:image];
	bounds.size = cell ? [cell cellSize] : NSZeroSize;
	if (bounds.size.height < height + 8) { /* workaround AppKit sizing bug */
	    bounds.size.height = height + 8;
	}
#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS
	if (!mbPtr->indicatorOn && tkMacOSXUseCompatibilityMetrics) {
	    bounds.size.width -= 16;
	}
#endif
    } else {
	bounds.size = cell ? [cell cellSize] : NSZeroSize;
    }
    if (haveText) {
	titleRect =  cell ? [cell titleRectForBounds:bounds] : NSZeroRect;
	if (mbPtr->wrapLength > 0 &&
		titleRect.size.width > mbPtr->wrapLength) {
	    if (style == NSRoundedBezelStyle) {
		[button setBezelStyle:(style = NSRegularSquareBezelStyle)];
		bounds.size = cell ? [cell cellSize] : NSZeroSize;
		titleRect = cell ? [cell titleRectForBounds:bounds] : NSZeroRect;
	    }
	    bounds.size.width -= titleRect.size.width - mbPtr->wrapLength;
	    bounds.size.height = 40000.0;
	    [cell setWraps:YES];
	    bounds.size = cell ? [cell cellSizeForBounds:bounds] : NSZeroSize;
#ifdef TK_MAC_DEBUG_MENUBUTTON
	    titleRect = cell ? [cell titleRectForBounds:bounds] : NSZeroRect;
#endif
#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS
	    if (tkMacOSXUseCompatibilityMetrics) {
		bounds.size.height += 3;
	    }
#endif
	}
    }
    width = lround(bounds.size.width);
    height = lround(bounds.size.height);
#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS
    if (tkMacOSXUseCompatibilityMetrics) {
	macButtonPtr->fix = fixForStyle(style);
	width -= boundsFixes[macButtonPtr->fix].trimW;
	height -= boundsFixes[macButtonPtr->fix].trimH;
    }
#endif

    if (haveImage || haveCompound) {
	if (mbPtr->width > 0) {
	    width = mbPtr->width;
	}
	if (mbPtr->height > 0) {
	    height = mbPtr->height;
	}
    } else {
	if (mbPtr->width > 0) {
	    int avgWidth = Tk_TextWidth(mbPtr->tkfont, "0", 1);
	    width = mbPtr->width * avgWidth;
	}
	if (mbPtr->height > 0) {
	    Tk_FontMetrics fm;

	    Tk_GetFontMetrics(mbPtr->tkfont, &fm);
	    height = mbPtr->height * fm.linespace;
	}
    }
    if (!haveImage || haveCompound) {
	width += 2*mbPtr->padX;
	height += 2*mbPtr->padY;
    }
    if (mbPtr->highlightWidth < 0) {
	mbPtr->highlightWidth = 0;
    }
    if (haveImage) {
	mbPtr->inset = mbPtr->highlightWidth;
	width += 2*mbPtr->borderWidth;
	height += 2*mbPtr->borderWidth;
    } else {
	mbPtr->inset = mbPtr->highlightWidth + mbPtr->borderWidth;
    }
    Tk_GeometryRequest(mbPtr->tkwin, width + 2 * mbPtr->inset,
	    height + 2 * mbPtr->inset);
    Tk_SetInternalBorder(mbPtr->tkwin, mbPtr->inset);
#ifdef TK_MAC_DEBUG_MENUBUTTON
    TKLog(@"menubutton %s bounds %@ titleRect %@ width %d height %d inset %d borderWidth %d",
	    ((TkWindow *)mbPtr->tkwin)->pathName, NSStringFromRect(bounds),
	    NSStringFromRect(titleRect), width, height, mbPtr->inset,
	    mbPtr->borderWidth);
#endif
}

/*
 *--------------------------------------------------------------
 *
 * MenuButtonEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on buttons.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When activation state changes, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
MenuButtonEventProc(
    ClientData clientData,	/* Information about window. */
    XEvent *eventPtr)		/* Information about event. */
{
    TkMenuButton *mbPtr = (TkMenuButton *) clientData;

    if (!mbPtr->tkwin || !Tk_IsMapped(mbPtr->tkwin)) {
	return;
    }
    switch (eventPtr->type) {
    case ActivateNotify:
    case DeactivateNotify:
	if (!(mbPtr->flags & REDRAW_PENDING)) {
	    Tcl_DoWhenIdle(TkpDisplayMenuButton, (ClientData) mbPtr);
	    mbPtr->flags |= REDRAW_PENDING;
	}
	break;
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
