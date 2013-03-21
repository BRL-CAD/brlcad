/*
 * tkMacOSXButton.c --
 *
 *	This file implements the Macintosh specific portion of the
 *	button widgets.
 *
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkButton.h"
#include "tkMacOSXFont.h"
#include "tkMacOSXDebug.h"

/*
#ifdef TK_MAC_DEBUG
#define TK_MAC_DEBUG_BUTTON
#endif
*/

typedef struct MacButton {
    TkButton info;
    NSButton *button;
    NSImage *image, *selectImage, *tristateImage;
#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS
    int fix;
#endif
} MacButton;

#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS

int tkMacOSXUseCompatibilityMetrics = 1;

/*
 * Use the following heuristic conversion constants to make NSButton-based
 * widget metrics match up with the old Carbon control buttons (for the
 * default Lucida Grande 13 font).
 */

#define NATIVE_BUTTON_INSET 2
#define NATIVE_BUTTON_EXTRA_H 2

typedef struct {
    int trimW, trimH, inset, shrinkH, offsetX, offsetY;
} BoundsFix;

#define fixForTypeStyle(type, style) ( \
	type == NSSwitchButton ? 0 : \
	type == NSRadioButton ? 1 : \
	style == NSRoundedBezelStyle ? 2 : \
	style == NSRegularSquareBezelStyle ? 3 : \
	style == NSShadowlessSquareBezelStyle ? 4 : \
	INT_MIN)

static const BoundsFix boundsFixes[] = {
    [fixForTypeStyle(NSSwitchButton,0)] =		{  2,  2, -1,  0, 2, 1 },
    [fixForTypeStyle(NSRadioButton,0)] =		{  0,  2, -1,  0, 1, 1 },
    [fixForTypeStyle(0,NSRoundedBezelStyle)] =		{ 28, 16, -6,  0, 0, 3 },
    [fixForTypeStyle(0,NSRegularSquareBezelStyle)] =	{ 28, 15, -2, -1 },
    [fixForTypeStyle(0,NSShadowlessSquareBezelStyle)] =	{  2,  2 },
};

#endif

static void DisplayNativeButton(TkButton *butPtr);
static void ComputeNativeButtonGeometry(TkButton *butPtr);
static void DisplayUnixButton(TkButton *butPtr);
static void ComputeUnixButtonGeometry(TkButton *butPtr);

/*
 * The class procedure table for the button widgets.
 */

Tk_ClassProcs tkpButtonProcs = {
    sizeof(Tk_ClassProcs),	/* size */
    TkButtonWorldChanged,	/* worldChangedProc */
    NULL,					/* createProc */
    NULL					/* modalProc */
};


/*
 *----------------------------------------------------------------------
 *
 * TkpCreateButton --
 *
 *	Allocate a new TkButton structure.
 *
 * Results:
 *	Returns a newly allocated TkButton structure.
 *
 * Side effects:
 *	Registers an event handler for the widget.
 *
 *----------------------------------------------------------------------
 */

TkButton *
TkpCreateButton(
    Tk_Window tkwin)
{
    MacButton *macButtonPtr = (MacButton *) ckalloc(sizeof(MacButton));

    macButtonPtr->button = nil;
    macButtonPtr->image = nil;
    macButtonPtr->selectImage = nil;
    macButtonPtr->tristateImage = nil;

    return (TkButton *) macButtonPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpDestroyButton --
 *
 *	Free data structures associated with the button control.
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
TkpDestroyButton(
    TkButton *butPtr)
{
    MacButton *macButtonPtr = (MacButton *) butPtr;

    TkMacOSXMakeCollectableAndRelease(macButtonPtr->button);
    TkMacOSXMakeCollectableAndRelease(macButtonPtr->selectImage);
    TkMacOSXMakeCollectableAndRelease(macButtonPtr->selectImage);
    TkMacOSXMakeCollectableAndRelease(macButtonPtr->tristateImage);
}

/*
 *----------------------------------------------------------------------
 *
 * TkpDisplayButton --
 *
 *	This procedure is invoked to display a button widget. It is
 *	normally invoked as an idle handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the button in its
 *	current mode. The REDRAW_PENDING flag is cleared.
 *
 *----------------------------------------------------------------------
 */

void
TkpDisplayButton(
    ClientData clientData)	/* Information about widget. */
{
    TkButton *butPtr = (TkButton *) clientData;

    butPtr->flags &= ~REDRAW_PENDING;
    if (!butPtr->tkwin || !Tk_IsMapped(butPtr->tkwin)) {
	return;
    }

    switch (butPtr->type) {
    case TYPE_LABEL:
	DisplayUnixButton(butPtr);
	break;
    case TYPE_BUTTON:
    case TYPE_CHECK_BUTTON:
    case TYPE_RADIO_BUTTON:
	DisplayNativeButton(butPtr);
	break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpComputeButtonGeometry --
 *
 *	After changes in a button's text or bitmap, this procedure
 *	recomputes the button's geometry and passes this information
 *	along to the geometry manager for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The button's window may change size.
 *
 *----------------------------------------------------------------------
 */

void
TkpComputeButtonGeometry(
    register TkButton *butPtr)	/* Button whose geometry may have changed. */
{
    MacButton *macButtonPtr = (MacButton *) butPtr;

    switch (butPtr->type) {
    case TYPE_LABEL:
	if (macButtonPtr->button && [macButtonPtr->button superview]) {
	    [macButtonPtr->button removeFromSuperviewWithoutNeedingDisplay];
	}
	ComputeUnixButtonGeometry(butPtr);
	break;
    case TYPE_BUTTON:
    case TYPE_CHECK_BUTTON:
    case TYPE_RADIO_BUTTON:
	if (!macButtonPtr->button) {
	    NSButton *button = [[NSButton alloc] initWithFrame:NSZeroRect];
	    macButtonPtr->button = TkMacOSXMakeUncollectable(button);
	}
	ComputeNativeButtonGeometry(butPtr);
	break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpButtonSetDefaults --
 *
 *	This procedure is invoked before option tables are created for
 *	buttons. It modifies some of the default values to match the current
 *	values defined for this platform.
 *
 * Results:
 *	Some of the default values in *specPtr are modified.
 *
 * Side effects:
 *	Updates some of.
 *
 *----------------------------------------------------------------------
 */

void
TkpButtonSetDefaults()
{
#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS
    if (!tkMacOSXUseCompatibilityMetrics) {
    	strcpy(tkDefButtonHighlightWidth, DEF_BUTTON_HIGHLIGHT_WIDTH_NOCM);
    	strcpy(tkDefLabelHighlightWidth, DEF_BUTTON_HIGHLIGHT_WIDTH_NOCM);
    	strcpy(tkDefButtonPadx, DEF_BUTTON_PADX_NOCM);
    	strcpy(tkDefLabelPadx, DEF_BUTTON_PADX_NOCM);
    	strcpy(tkDefButtonPady, DEF_BUTTON_PADY_NOCM);
    	strcpy(tkDefLabelPady, DEF_BUTTON_PADY_NOCM);
    }
#endif
}

#pragma mark -
#pragma mark Native Buttons:


/*
 *----------------------------------------------------------------------
 *
 * DisplayNativeButton --
 *
 *	This procedure is invoked to display a button widget. It is
 *	normally invoked as an idle handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the button in its
 *	current mode. The REDRAW_PENDING flag is cleared.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayNativeButton(
    TkButton *butPtr)
{
    MacButton *macButtonPtr = (MacButton *) butPtr;
    NSButton *button = macButtonPtr->button;
    Tk_Window tkwin = butPtr->tkwin;
    TkWindow *winPtr = (TkWindow *) tkwin;
    MacDrawable *macWin =  (MacDrawable *) winPtr->window;
    TkMacOSXDrawingContext dc;
    NSView *view = TkMacOSXDrawableView(macWin);
    CGFloat viewHeight = [view bounds].size.height;
    CGAffineTransform t = { .a = 1, .b = 0, .c = 0, .d = -1, .tx = 0,
	    .ty = viewHeight};
    NSRect frame;
    int enabled;
    NSCellStateValue state;

    if (!view ||
	    !TkMacOSXSetupDrawingContext((Drawable) macWin, NULL, 1, &dc)) {
	return;
    }
    CGContextConcatCTM(dc.context, t);

    /*
     * We cannot change the background color of the button itself, only the
     * color of the background of its container.
     * This will be the color that peeks around the rounded corners of the
     * button. We make this the highlightbackground rather than the background,
     * because if you color the background of a frame containing a
     * button, you usually also color the highlightbackground as well,
     * or you will get a thin grey ring around the button.
     */

    Tk_Fill3DRectangle(tkwin, (Pixmap) macWin, butPtr->type == TYPE_BUTTON ?
	    butPtr->highlightBorder : butPtr->normalBorder, 0, 0,
	    Tk_Width(tkwin), Tk_Height(tkwin), 0, TK_RELIEF_FLAT);
    if ([button superview] != view) {
	[view addSubview:button];
    }
    if (macButtonPtr->tristateImage) {
	NSImage *selectImage = macButtonPtr->selectImage ?
		macButtonPtr->selectImage : macButtonPtr->image;
	[button setImage:(butPtr->flags & TRISTATED ?
		selectImage : macButtonPtr->image)];
	[button setAlternateImage:(butPtr->flags & TRISTATED ?
		macButtonPtr->tristateImage : selectImage)];
    }
    if (butPtr->flags & SELECTED) {
	state = NSOnState;
    } else if (butPtr->flags & TRISTATED) {
	state = NSMixedState;
    } else {
	state = NSOffState;
    }
    [button setState:state];
    enabled = !(butPtr->state == STATE_DISABLED);
    [button setEnabled:enabled];
    if (enabled) {
	//[button highlight:(butPtr->state == STATE_ACTIVE)];
	//[cell setHighlighted:(butPtr->state == STATE_ACTIVE)];
    }
    if (butPtr->type == TYPE_BUTTON && butPtr->defaultState == STATE_ACTIVE) {
	//[[view window] setDefaultButtonCell:cell];
	[button setKeyEquivalent:@"\r"];
    } else {
	[button setKeyEquivalent:@""];
    }
    frame = NSMakeRect(macWin->xOff, macWin->yOff, Tk_Width(tkwin),
	    Tk_Height(tkwin));
#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS
    if (tkMacOSXUseCompatibilityMetrics) {
	BoundsFix boundsFix = boundsFixes[macButtonPtr->fix];
	frame = NSOffsetRect(frame, boundsFix.offsetX, boundsFix.offsetY);
	frame.size.height -= boundsFix.shrinkH + NATIVE_BUTTON_EXTRA_H;
	frame = NSInsetRect(frame, boundsFix.inset + NATIVE_BUTTON_INSET,
		boundsFix.inset + NATIVE_BUTTON_INSET);
    }
#endif
    frame.origin.y = viewHeight - (frame.origin.y + frame.size.height);
    if (!NSEqualRects(frame, [button frame])) {
	[button setFrame:frame];
    }
    [button displayRectIgnoringOpacity:[button bounds]];
    TkMacOSXRestoreDrawingContext(&dc);
#ifdef TK_MAC_DEBUG_BUTTON
    TKLog(@"button %s frame %@ width %d height %d",
	    ((TkWindow *)butPtr->tkwin)->pathName, NSStringFromRect(frame),
	    Tk_Width(tkwin), Tk_Height(tkwin));
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeNativeButtonGeometry --
 *
 *	After changes in a button's text or bitmap, this procedure
 *	recomputes the button's geometry and passes this information
 *	along to the geometry manager for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The button's window may change size.
 *
 *----------------------------------------------------------------------
 */

static void
ComputeNativeButtonGeometry(
    TkButton *butPtr)		/* Button whose geometry may have changed. */
{
    MacButton *macButtonPtr = (MacButton *) butPtr;
    NSButton *button = macButtonPtr->button;
    NSButtonCell *cell = [button cell];
    NSButtonType type = -1;
    NSBezelStyle style = 0;
    NSInteger highlightsBy = 0, showsStateBy = 0;
    NSFont *font;
    NSRect bounds = NSZeroRect, titleRect = NSZeroRect;
    int haveImage = (butPtr->image || butPtr->bitmap != None), haveText = 0;
    int haveCompound = (butPtr->compound != COMPOUND_NONE);
    int width, height, border = 0;

    butPtr->indicatorSpace = 0;
    butPtr->inset = 0;
    if (butPtr->highlightWidth < 0) {
	butPtr->highlightWidth = 0;
    }
    switch (butPtr->type) {
    case TYPE_BUTTON:
	type = NSMomentaryPushInButton;
	if (!haveImage) {
	    style = NSRoundedBezelStyle;
	    butPtr->inset = butPtr->defaultState != STATE_DISABLED ?
		    butPtr->highlightWidth : 0;
	    [button setImage:nil];
	    [button setImagePosition:NSNoImage];
	} else {
	    style = NSShadowlessSquareBezelStyle;
	    highlightsBy = butPtr->selectImage || butPtr->bitmap ?
		    NSContentsCellMask : 0;
	    border = butPtr->borderWidth;
	}
	break;
    case TYPE_RADIO_BUTTON:
    case TYPE_CHECK_BUTTON:
	if (!haveImage /*|| butPtr->indicatorOn*/) { // TODO: indicatorOn
	    type = butPtr->type == TYPE_RADIO_BUTTON ?
		    NSRadioButton : NSSwitchButton;
	    butPtr->inset = /*butPtr->indicatorOn ? 0 :*/ butPtr->borderWidth;
	} else {
	    type = NSPushOnPushOffButton;
	    style = NSShadowlessSquareBezelStyle;
	    highlightsBy = butPtr->selectImage || butPtr->bitmap ?
		    NSContentsCellMask : 0;
	    showsStateBy = butPtr->selectImage || butPtr->tristateImage ?
		    NSContentsCellMask : 0;
#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS
	    if (tkMacOSXUseCompatibilityMetrics) {
		border = butPtr->borderWidth > 1 ? butPtr->borderWidth - 1 : 1;
	    } else
#endif
	    {
		border = butPtr->borderWidth;
	    }
	}
	break;
    }
    [button setButtonType:type];
    if (style) {
	[button setBezelStyle:style];
    }
    if (highlightsBy) {
	[cell setHighlightsBy:highlightsBy|[cell highlightsBy]];
    }
    if (showsStateBy) {
	[cell setShowsStateBy:showsStateBy|[cell showsStateBy]];
    }
#if 0
    if (style == NSShadowlessSquareBezelStyle) {
	NSControlSize controlSize = NSRegularControlSize;

	if (butPtr->borderWidth <= 2) {
	    controlSize = NSMiniControlSize;
	} else if (butPtr->borderWidth == 3) {
	    controlSize = NSSmallControlSize;
	}
	[cell setControlSize:controlSize];
    }
#endif
    [button setAllowsMixedState:YES];

    if (!haveImage || haveCompound) {
	int len;
	char *text = Tcl_GetStringFromObj(butPtr->textPtr, &len);

	if (len) {
	    NSString *title = [[NSString alloc] initWithBytes:text length:len
		    encoding:NSUTF8StringEncoding];
	    [button setTitle:title];
	    [title release];
	    haveText = 1;
	}
    }
    haveCompound = (haveCompound && haveImage && haveText);
    if (haveText) {
	NSTextAlignment alignment = NSNaturalTextAlignment;

	switch (butPtr->justify) {
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
    font = TkMacOSXNSFontForFont(butPtr->tkfont);
    if (font) {
	[button setFont:font];
    }
    TkMacOSXMakeCollectableAndRelease(macButtonPtr->image);
    TkMacOSXMakeCollectableAndRelease(macButtonPtr->selectImage);
    TkMacOSXMakeCollectableAndRelease(macButtonPtr->tristateImage);
    if (haveImage) {
	int width, height;
	NSImage *image, *selectImage = nil, *tristateImage = nil;
	NSCellImagePosition pos = NSImageOnly;

	if (butPtr->image) {
	    Tk_SizeOfImage(butPtr->image, &width, &height);
	    image = TkMacOSXGetNSImageWithTkImage(butPtr->display,
		    butPtr->image, width, height);
	    if (butPtr->selectImage) {
		selectImage = TkMacOSXGetNSImageWithTkImage(butPtr->display,
			butPtr->selectImage, width, height);
	    }
	    if (butPtr->tristateImage) {
		tristateImage = TkMacOSXGetNSImageWithTkImage(butPtr->display,
			butPtr->tristateImage, width, height);
	    }
	} else {
	    Tk_SizeOfBitmap(butPtr->display, butPtr->bitmap, &width, &height);
	    image = TkMacOSXGetNSImageWithBitmap(butPtr->display,
		    butPtr->bitmap, butPtr->normalTextGC, width, height);
	    selectImage = TkMacOSXGetNSImageWithBitmap(butPtr->display,
		    butPtr->bitmap, butPtr->activeTextGC, width, height);
	}
	[button setImage:image];
	if (selectImage) {
	    [button setAlternateImage:selectImage];
	}
	if (tristateImage) {
	    macButtonPtr->image = TkMacOSXMakeUncollectableAndRetain(image);
	    if (selectImage) {
		macButtonPtr->selectImage =
			TkMacOSXMakeUncollectableAndRetain(selectImage);
	    }
	    macButtonPtr->tristateImage =
		    TkMacOSXMakeUncollectableAndRetain(tristateImage);
	}
	if (haveCompound) {
	    switch ((enum compound) butPtr->compound) {
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
    }

    // if font is too tall, we can't use the fixed-height rounded bezel
    if (!haveImage && haveText && style == NSRoundedBezelStyle) {
      Tk_FontMetrics fm;
      Tk_GetFontMetrics(butPtr->tkfont, &fm);
      if (fm.linespace > 18) {
        [button setBezelStyle:(style = NSRegularSquareBezelStyle)];
      }
    }

    bounds.size = [cell cellSize];
    if (haveText) {
	titleRect = [cell titleRectForBounds:bounds];
	if (butPtr->wrapLength > 0 &&
		titleRect.size.width > butPtr->wrapLength) {
	    if (style == NSRoundedBezelStyle) {
		[button setBezelStyle:(style = NSRegularSquareBezelStyle)];
		bounds.size = [cell cellSize];
		titleRect = [cell titleRectForBounds:bounds];
	    }
	    bounds.size.width -= titleRect.size.width - butPtr->wrapLength;
	    bounds.size.height = 40000.0;
	    [cell setWraps:YES];
	    bounds.size = [cell cellSizeForBounds:bounds];
#ifdef TK_MAC_DEBUG_BUTTON
	    titleRect = [cell titleRectForBounds:bounds];
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
	macButtonPtr->fix = fixForTypeStyle(type, style);
	width -= boundsFixes[macButtonPtr->fix].trimW;
	height -= boundsFixes[macButtonPtr->fix].trimH;
    }
#endif

    if (haveImage || haveCompound) {
	if (butPtr->width > 0) {
	    width = butPtr->width;
	}
	if (butPtr->height > 0) {
	    height = butPtr->height;
	}
    } else {
	if (butPtr->width > 0) {
	    int avgWidth = Tk_TextWidth(butPtr->tkfont, "0", 1);
	    width = butPtr->width * avgWidth;
	}
	if (butPtr->height > 0) {
	    Tk_FontMetrics fm;

	    Tk_GetFontMetrics(butPtr->tkfont, &fm);
	    height = butPtr->height * fm.linespace;
	}
    }
    if (!haveImage || haveCompound) {
	width += 2*butPtr->padX;
	height += 2*butPtr->padY;
    }
    if (haveImage) {
	width += 2*border;
	height += 2*border;
    }
#if TK_MAC_BUTTON_USE_COMPATIBILITY_METRICS
    if (tkMacOSXUseCompatibilityMetrics) {
	width += 2*NATIVE_BUTTON_INSET;
	height += 2*NATIVE_BUTTON_INSET + NATIVE_BUTTON_EXTRA_H;
    }
#endif
    Tk_GeometryRequest(butPtr->tkwin, width, height);
    Tk_SetInternalBorder(butPtr->tkwin, butPtr->inset);
#ifdef TK_MAC_DEBUG_BUTTON
    TKLog(@"button %s bounds %@ titleRect %@ width %d height %d inset %d borderWidth %d",
	    ((TkWindow *)butPtr->tkwin)->pathName, NSStringFromRect(bounds),
	    NSStringFromRect(titleRect), width, height, butPtr->inset,
	    butPtr->borderWidth);
#endif
}

#pragma mark -
#pragma mark Unix Buttons:


/*
 *----------------------------------------------------------------------
 *
 * DisplayUnixButton --
 *
 *	This procedure is invoked to display a button widget. It is
 *	normally invoked as an idle handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the button in its
 *	current mode. The REDRAW_PENDING flag is cleared.
 *
 *----------------------------------------------------------------------
 */

void
DisplayUnixButton(
    TkButton *butPtr)
{
    GC gc;
    Tk_3DBorder border;
    Pixmap pixmap;
    int x = 0;			/* Initialization only needed to stop compiler
				 * warning. */
    int y, relief;
    Tk_Window tkwin = butPtr->tkwin;
    int width = 0, height = 0, fullWidth, fullHeight;
    int textXOffset, textYOffset;
    int haveImage = 0, haveText = 0;
    int imageWidth, imageHeight;
    int imageXOffset = 0, imageYOffset = 0;
				/* image information that will be used to
				 * restrict disabled pixmap as well */

    border = butPtr->normalBorder;
    if ((butPtr->state == STATE_DISABLED) && (butPtr->disabledFg != NULL)) {
	gc = butPtr->disabledGC;
    } else if ((butPtr->state == STATE_ACTIVE)
	    && !Tk_StrictMotif(butPtr->tkwin)) {
	gc = butPtr->activeTextGC;
	border = butPtr->activeBorder;
    } else {
	gc = butPtr->normalTextGC;
    }
    if ((butPtr->flags & SELECTED) && (butPtr->state != STATE_ACTIVE)
	    && (butPtr->selectBorder != NULL) && !butPtr->indicatorOn) {
	border = butPtr->selectBorder;
    }

    relief = butPtr->relief;

    pixmap = (Pixmap) Tk_WindowId(tkwin);
    Tk_Fill3DRectangle(tkwin, pixmap, border, 0, 0, Tk_Width(tkwin),
	    Tk_Height(tkwin), 0, TK_RELIEF_FLAT);

    /*
     * Display image or bitmap or text for button.
     */

    if (butPtr->image != NULL) {
	Tk_SizeOfImage(butPtr->image, &width, &height);
	haveImage = 1;
    } else if (butPtr->bitmap != None) {
	Tk_SizeOfBitmap(butPtr->display, butPtr->bitmap, &width, &height);
	haveImage = 1;
    }
    imageWidth = width;
    imageHeight = height;

    haveText = (butPtr->textWidth != 0 && butPtr->textHeight != 0);

    if (butPtr->compound != COMPOUND_NONE && haveImage && haveText) {
	textXOffset = 0;
	textYOffset = 0;
	fullWidth = 0;
	fullHeight = 0;

	switch ((enum compound) butPtr->compound) {
	case COMPOUND_TOP:
	case COMPOUND_BOTTOM:
	    /*
	     * Image is above or below text.
	     */

	    if (butPtr->compound == COMPOUND_TOP) {
		textYOffset = height + butPtr->padY;
	    } else {
		imageYOffset = butPtr->textHeight + butPtr->padY;
	    }
	    fullHeight = height + butPtr->textHeight + butPtr->padY;
	    fullWidth = (width > butPtr->textWidth ? width :
		    butPtr->textWidth);
	    textXOffset = (fullWidth - butPtr->textWidth)/2;
	    imageXOffset = (fullWidth - width)/2;
	    break;
	case COMPOUND_LEFT:
	case COMPOUND_RIGHT:
	    /*
	     * Image is left or right of text.
	     */

	    if (butPtr->compound == COMPOUND_LEFT) {
		textXOffset = width + butPtr->padX;
	    } else {
		imageXOffset = butPtr->textWidth + butPtr->padX;
	    }
	    fullWidth = butPtr->textWidth + butPtr->padX + width;
	    fullHeight = (height > butPtr->textHeight ? height :
		    butPtr->textHeight);
	    textYOffset = (fullHeight - butPtr->textHeight)/2;
	    imageYOffset = (fullHeight - height)/2;
	    break;
	case COMPOUND_CENTER:
	    /*
	     * Image and text are superimposed.
	     */

	    fullWidth = (width > butPtr->textWidth ? width :
		    butPtr->textWidth);
	    fullHeight = (height > butPtr->textHeight ? height :
		    butPtr->textHeight);
	    textXOffset = (fullWidth - butPtr->textWidth)/2;
	    imageXOffset = (fullWidth - width)/2;
	    textYOffset = (fullHeight - butPtr->textHeight)/2;
	    imageYOffset = (fullHeight - height)/2;
	    break;
	case COMPOUND_NONE:
	    break;
	}

	TkComputeAnchor(butPtr->anchor, tkwin, butPtr->padX, butPtr->padY,
		fullWidth, fullHeight, &x, &y);

	imageXOffset += x;
	imageYOffset += y;

	if (butPtr->image != NULL) {
	    /*
	     * Do boundary clipping, so that Tk_RedrawImage is passed valid
	     * coordinates. [Bug 979239]
	     */

	    if (imageXOffset < 0) {
		imageXOffset = 0;
	    }
	    if (imageYOffset < 0) {
		imageYOffset = 0;
	    }
	    if (width > Tk_Width(tkwin)) {
		width = Tk_Width(tkwin);
	    }
	    if (height > Tk_Height(tkwin)) {
		height = Tk_Height(tkwin);
	    }
	    if ((width + imageXOffset) > Tk_Width(tkwin)) {
		imageXOffset = Tk_Width(tkwin) - width;
	    }
	    if ((height + imageYOffset) > Tk_Height(tkwin)) {
		imageYOffset = Tk_Height(tkwin) - height;
	    }

	    if ((butPtr->selectImage != NULL) && (butPtr->flags & SELECTED)) {
		Tk_RedrawImage(butPtr->selectImage, 0, 0,
			width, height, pixmap, imageXOffset, imageYOffset);
	    } else if ((butPtr->tristateImage != NULL) && (butPtr->flags & TRISTATED)) {
		Tk_RedrawImage(butPtr->tristateImage, 0, 0,
			width, height, pixmap, imageXOffset, imageYOffset);
	    } else {
		Tk_RedrawImage(butPtr->image, 0, 0, width,
			height, pixmap, imageXOffset, imageYOffset);
	    }
	} else {
	    XSetClipOrigin(butPtr->display, gc, imageXOffset, imageYOffset);
	    XCopyPlane(butPtr->display, butPtr->bitmap, pixmap, gc,
		    0, 0, (unsigned int) width, (unsigned int) height,
		    imageXOffset, imageYOffset, 1);
	    XSetClipOrigin(butPtr->display, gc, 0, 0);
	}

	Tk_DrawTextLayout(butPtr->display, pixmap, gc,
		butPtr->textLayout, x + textXOffset, y + textYOffset, 0, -1);
	Tk_UnderlineTextLayout(butPtr->display, pixmap, gc,
		butPtr->textLayout, x + textXOffset, y + textYOffset,
		butPtr->underline);
	y += fullHeight/2;
    } else {
	if (haveImage) {
	    TkComputeAnchor(butPtr->anchor, tkwin, 0, 0,
		    width, height, &x, &y);
	    imageXOffset += x;
	    imageYOffset += y;
	    if (butPtr->image != NULL) {
		/*
		 * Do boundary clipping, so that Tk_RedrawImage is passed
		 * valid coordinates. [Bug 979239]
		 */

		if (imageXOffset < 0) {
		    imageXOffset = 0;
		}
		if (imageYOffset < 0) {
		    imageYOffset = 0;
		}
		if (width > Tk_Width(tkwin)) {
		    width = Tk_Width(tkwin);
		}
		if (height > Tk_Height(tkwin)) {
		    height = Tk_Height(tkwin);
		}
		if ((width + imageXOffset) > Tk_Width(tkwin)) {
		    imageXOffset = Tk_Width(tkwin) - width;
		}
		if ((height + imageYOffset) > Tk_Height(tkwin)) {
		    imageYOffset = Tk_Height(tkwin) - height;
		}

		if ((butPtr->selectImage != NULL) &&
			(butPtr->flags & SELECTED)) {
		    Tk_RedrawImage(butPtr->selectImage, 0, 0, width,
			    height, pixmap, imageXOffset, imageYOffset);
		} else if ((butPtr->tristateImage != NULL) &&
			(butPtr->flags & TRISTATED)) {
		    Tk_RedrawImage(butPtr->tristateImage, 0, 0, width,
			    height, pixmap, imageXOffset, imageYOffset);
		} else {
		    Tk_RedrawImage(butPtr->image, 0, 0, width, height, pixmap,
			    imageXOffset, imageYOffset);
		}
	    } else {
		XSetClipOrigin(butPtr->display, gc, x, y);
		XCopyPlane(butPtr->display, butPtr->bitmap, pixmap, gc, 0, 0,
			(unsigned int) width, (unsigned int) height, x, y, 1);
		XSetClipOrigin(butPtr->display, gc, 0, 0);
	    }
	    y += height/2;
	} else {
 	    TkComputeAnchor(butPtr->anchor, tkwin, butPtr->padX, butPtr->padY,
		    butPtr->textWidth, butPtr->textHeight, &x, &y);

	    Tk_DrawTextLayout(butPtr->display, pixmap, gc, butPtr->textLayout,
		    x, y, 0, -1);
	    Tk_UnderlineTextLayout(butPtr->display, pixmap, gc,
		    butPtr->textLayout, x, y, butPtr->underline);
	    y += butPtr->textHeight/2;
	}
    }

    /*
     * If the button is disabled with a stipple rather than a special
     * foreground color, generate the stippled effect. If the widget is
     * selected and we use a different background color when selected, must
     * temporarily modify the GC so the stippling is the right color.
     */

    if ((butPtr->state == STATE_DISABLED)
	    && ((butPtr->disabledFg == NULL) || (butPtr->image != NULL))) {
	if ((butPtr->flags & SELECTED) && !butPtr->indicatorOn
		&& (butPtr->selectBorder != NULL)) {
	    XSetForeground(butPtr->display, butPtr->stippleGC,
		    Tk_3DBorderColor(butPtr->selectBorder)->pixel);
	}

	/*
	 * Stipple the whole button if no disabledFg was specified, otherwise
	 * restrict stippling only to displayed image
	 */

	if (butPtr->disabledFg == NULL) {
	    XFillRectangle(butPtr->display, pixmap, butPtr->stippleGC, 0, 0,
		    (unsigned) Tk_Width(tkwin), (unsigned) Tk_Height(tkwin));
	} else {
	    XFillRectangle(butPtr->display, pixmap, butPtr->stippleGC,
		    imageXOffset, imageYOffset,
		    (unsigned) imageWidth, (unsigned) imageHeight);
	}
	if ((butPtr->flags & SELECTED) && !butPtr->indicatorOn
		&& (butPtr->selectBorder != NULL)) {
	    XSetForeground(butPtr->display, butPtr->stippleGC,
		    Tk_3DBorderColor(butPtr->normalBorder)->pixel);
	}
    }

    /*
     * Draw the border and traversal highlight last. This way, if the button's
     * contents overflow they'll be covered up by the border. This code is
     * complicated by the possible combinations of focus highlight and default
     * rings. We draw the focus and highlight rings using the highlight border
     * and highlight foreground color.
     */

    if (relief != TK_RELIEF_FLAT) {
	int inset = butPtr->highlightWidth;

	if (butPtr->defaultState == DEFAULT_ACTIVE) {
	    /*
	     * Draw the default ring with 2 pixels of space between the
	     * default ring and the button and the default ring and the focus
	     * ring. Note that we need to explicitly draw the space in the
	     * highlightBorder color to ensure that we overwrite any overflow
	     * text and/or a different button background color.
	     */

	    Tk_Draw3DRectangle(tkwin, pixmap, butPtr->highlightBorder, inset,
		    inset, Tk_Width(tkwin) - 2*inset,
		    Tk_Height(tkwin) - 2*inset, 2, TK_RELIEF_FLAT);
	    inset += 2;
	    Tk_Draw3DRectangle(tkwin, pixmap, butPtr->highlightBorder, inset,
		    inset, Tk_Width(tkwin) - 2*inset,
		    Tk_Height(tkwin) - 2*inset, 1, TK_RELIEF_SUNKEN);
	    inset++;
	    Tk_Draw3DRectangle(tkwin, pixmap, butPtr->highlightBorder, inset,
		    inset, Tk_Width(tkwin) - 2*inset,
		    Tk_Height(tkwin) - 2*inset, 2, TK_RELIEF_FLAT);

	    inset += 2;
	} else if (butPtr->defaultState == DEFAULT_NORMAL) {
	    /*
	     * Leave room for the default ring and write over any text or
	     * background color.
	     */

	    Tk_Draw3DRectangle(tkwin, pixmap, butPtr->highlightBorder, 0,
		    0, Tk_Width(tkwin), Tk_Height(tkwin), 5, TK_RELIEF_FLAT);
	    inset += 5;
	}

	/*
	 * Draw the button border.
	 */

	Tk_Draw3DRectangle(tkwin, pixmap, border, inset, inset,
		Tk_Width(tkwin) - 2*inset, Tk_Height(tkwin) - 2*inset,
		butPtr->borderWidth, relief);
    }
    if (butPtr->highlightWidth > 0) {
	GC gc;

	if (butPtr->flags & GOT_FOCUS) {
	    gc = Tk_GCForColor(butPtr->highlightColorPtr, pixmap);
	} else {
	    gc = Tk_GCForColor(Tk_3DBorderColor(butPtr->highlightBorder),
		    pixmap);
	}

	/*
	 * Make sure the focus ring shrink-wraps the actual button, not the
	 * padding space left for a default ring.
	 */

	if (butPtr->defaultState == DEFAULT_NORMAL) {
	    TkDrawInsetFocusHighlight(tkwin, gc, butPtr->highlightWidth,
		    pixmap, 5);
	} else {
	    Tk_DrawFocusHighlight(tkwin, gc, butPtr->highlightWidth, pixmap);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeUnixButtonGeometry --
 *
 *	After changes in a button's text or bitmap, this procedure
 *	recomputes the button's geometry and passes this information
 *	along to the geometry manager for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The button's window may change size.
 *
 *----------------------------------------------------------------------
 */

void
ComputeUnixButtonGeometry(
    register TkButton *butPtr)	/* Button whose geometry may have changed. */
{
    int width, height, avgWidth, txtWidth, txtHeight;
    int haveImage = 0, haveText = 0;
    Tk_FontMetrics fm;

    butPtr->inset = butPtr->highlightWidth + butPtr->borderWidth;

    /*
     * Leave room for the default ring if needed.
     */

    if (butPtr->defaultState != DEFAULT_DISABLED) {
	butPtr->inset += 5;
    }
    butPtr->indicatorSpace = 0;

    width = 0;
    height = 0;
    txtWidth = 0;
    txtHeight = 0;
    avgWidth = 0;

    if (butPtr->image != NULL) {
	Tk_SizeOfImage(butPtr->image, &width, &height);
	haveImage = 1;
    } else if (butPtr->bitmap != None) {
	Tk_SizeOfBitmap(butPtr->display, butPtr->bitmap, &width, &height);
	haveImage = 1;
    }

    if (haveImage == 0 || butPtr->compound != COMPOUND_NONE) {
	Tk_FreeTextLayout(butPtr->textLayout);

	butPtr->textLayout = Tk_ComputeTextLayout(butPtr->tkfont,
		Tcl_GetString(butPtr->textPtr), -1, butPtr->wrapLength,
		butPtr->justify, 0, &butPtr->textWidth, &butPtr->textHeight);

	txtWidth = butPtr->textWidth;
	txtHeight = butPtr->textHeight;
	avgWidth = Tk_TextWidth(butPtr->tkfont, "0", 1);
	Tk_GetFontMetrics(butPtr->tkfont, &fm);
	haveText = (txtWidth != 0 && txtHeight != 0);
    }

    /*
     * If the button is compound (i.e., it shows both an image and text), the
     * new geometry is a combination of the image and text geometry. We only
     * honor the compound bit if the button has both text and an image,
     * because otherwise it is not really a compound button.
     */

    if (butPtr->compound != COMPOUND_NONE && haveImage && haveText) {
	switch ((enum compound) butPtr->compound) {
	case COMPOUND_TOP:
	case COMPOUND_BOTTOM:
	    /*
	     * Image is above or below text.
	     */

	    height += txtHeight + butPtr->padY;
	    width = (width > txtWidth ? width : txtWidth);
	    break;
	case COMPOUND_LEFT:
	case COMPOUND_RIGHT:
	    /*
	     * Image is left or right of text.
	     */

	    width += txtWidth + butPtr->padX;
	    height = (height > txtHeight ? height : txtHeight);
	    break;
	case COMPOUND_CENTER:
	    /*
	     * Image and text are superimposed.
	     */

	    width = (width > txtWidth ? width : txtWidth);
	    height = (height > txtHeight ? height : txtHeight);
	    break;
	case COMPOUND_NONE:
	    break;
	}
	if (butPtr->width > 0) {
	    width = butPtr->width;
	}
	if (butPtr->height > 0) {
	    height = butPtr->height;
	}

	width += 2*butPtr->padX;
	height += 2*butPtr->padY;
    } else {
	if (haveImage) {
	    if (butPtr->width > 0) {
		width = butPtr->width;
	    }
	    if (butPtr->height > 0) {
		height = butPtr->height;
	    }
	} else {
	    width = txtWidth;
	    height = txtHeight;

	    if (butPtr->width > 0) {
		width = butPtr->width * avgWidth;
	    }
	    if (butPtr->height > 0) {
		height = butPtr->height * fm.linespace;
	    }
	}
    }

    if (!haveImage) {
	width += 2*butPtr->padX;
	height += 2*butPtr->padY;
    }
    Tk_GeometryRequest(butPtr->tkwin, (int) (width
	    + 2*butPtr->inset), (int) (height + 2*butPtr->inset));
    Tk_SetInternalBorder(butPtr->tkwin, butPtr->inset);
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
