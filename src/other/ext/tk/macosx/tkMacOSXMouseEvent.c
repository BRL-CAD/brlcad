/*
 * tkMacOSXMouseEvent.c --
 *
 *	This file implements functions that decode & handle mouse events on
 *	MacOS X.
 *
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2005-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXWm.h"
#include "tkMacOSXEvent.h"
#include "tkMacOSXDebug.h"
#include "tkMacOSXConstants.h"

typedef struct {
    unsigned int state;
    long delta;
    Window window;
    Point global;
    Point local;
} MouseEventData;

static Tk_Window captureWinPtr = NULL;	/* Current capture window; may be
					 * NULL. */

static int		GenerateButtonEvent(MouseEventData *medPtr);
static unsigned int	ButtonModifiers2State(UInt32 buttonState,
			    UInt32 keyModifiers);

#pragma mark TKApplication(TKMouseEvent)

enum {
    NSWindowWillMoveEventType = 20
};

/*
 * In OS X 10.6 an NSEvent of type NSMouseMoved would always have a non-Nil
 * window attribute pointing to the active window.  As of 10.8 this behavior
 * had changed.  The new behavior was that if the mouse were ever moved outside
 * of a window, all subsequent NSMouseMoved NSEvents would have a Nil window
 * attribute until the mouse returned to the window.  In 11.1 it changed again.
 * The window attribute can be non-nil, but referencing a window which does not
 * belong to the application.
 */

@implementation TKApplication(TKMouseEvent)
- (NSEvent *) tkProcessMouseEvent: (NSEvent *) theEvent
{
    NSWindow *eventWindow = [theEvent window];
    NSEventType eventType = [theEvent type];
    NSRect viewFrame = [[eventWindow contentView] frame];
    NSPoint location = [theEvent locationInWindow];
    TkWindow *winPtr = NULL, *grabWinPtr;
    Tk_Window tkwin = None, capture, target;
    NSPoint local, global;
    NSInteger button;
    Bool inTitleBar = NO;
    int win_x, win_y;
    unsigned int buttonState = 0;
    static int validPresses = 0, ignoredPresses = 0;

#ifdef TK_MAC_DEBUG_EVENTS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, theEvent);
#endif

    /*
     * If this event is not for a Tk toplevel, it should just be passed up the
     * responder chain.  However, there is an exception for synthesized events,
     * which are used in testing.  Those events are recognized by having their
     * both the windowNumber and the eventNumber set to -1.
     */

    if (eventWindow && ![eventWindow isMemberOfClass:[TKWindow class]]) {
	if ([theEvent windowNumber] != -1 || [theEvent eventNumber] != -1)
	    return theEvent;
    }

    /*
     * Check if the event is located in the titlebar.
     */

    if (eventWindow) {
	inTitleBar = viewFrame.size.height < location.y;
    }

    button = [theEvent buttonNumber] + Button1;
    switch (eventType) {
    case NSRightMouseUp:
    case NSOtherMouseUp:
	buttonState &= ~TkGetButtonMask(button);
	break;
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
    case NSRightMouseDown:
    case NSOtherMouseDown:
	buttonState |= TkGetButtonMask(button);
	break;
    case NSMouseEntered:
	if ([eventWindow respondsToSelector:@selector(mouseInResizeArea)] &&
	    !inTitleBar) {
	    [(TKWindow *)eventWindow setMouseInResizeArea:YES];
	}
	break;
    case NSMouseExited:
	if ([eventWindow respondsToSelector:@selector(mouseInResizeArea)]) {
	    [(TKWindow *)eventWindow setMouseInResizeArea:NO];
	    break;
	}
    case NSLeftMouseUp:
    case NSLeftMouseDown:

	/*
	 * Ignore mouse button events which arrive while the app is inactive.
	 * These events will be resent after activation, causing duplicate
	 * actions when an app is activated by a bound mouse event. See ticket
	 * [7bda9882cb].
	 */

	if (! [NSApp isActive]) {
	    return theEvent;
	}
    case NSMouseMoved:
    case NSScrollWheel:
#if 0
    case NSCursorUpdate:
    case NSTabletPoint:
    case NSTabletProximity:
#endif
	break;
    default: /* This type of event is ignored. */
	return theEvent;
    }

    /*
     * Update the button state.  We ignore left button presses that start a
     * resize or occur in the title bar.  See tickets [d72abe6b54] and
     * [39cbacb9e8].
     */

    if (eventType == NSLeftMouseDown) {
	if ([eventWindow respondsToSelector:@selector(mouseInResizeArea)] &&
	    [(TKWindow *) eventWindow mouseInResizeArea]) {

	    /*
	     * When the left button is pressed in the resize area, we receive
	     * NSMouseDown, but when it is released we do not receive
	     * NSMouseUp.  So ignore the event and clear the button state but
	     * do not change the ignoredPresses count.
	     */

	    buttonState &= ~TkGetButtonMask(Button1);
	    return theEvent;
	}
	if (inTitleBar) {
	    ignoredPresses++;
	    return theEvent;
	}
	validPresses++;
	buttonState |= TkGetButtonMask(Button1);
    }
    if (eventType == NSLeftMouseUp) {
	if (ignoredPresses > 0) {
	    ignoredPresses--;
	} else if (validPresses > 0) {
	    validPresses--;
	}
	if (validPresses == 0) {
	    buttonState &= ~TkGetButtonMask(Button1);
	}
    }

    /*
     * Find an appropriate NSWindow to attach to this event, and its
     * associated Tk window.
     */

    capture = TkMacOSXGetCapture();
    if (eventWindow) {
	    winPtr = TkMacOSXGetTkWindow(eventWindow);
    } else if (capture) {
	winPtr = (TkWindow *) capture;
	eventWindow = TkMacOSXGetNSWindowForDrawable(winPtr->window);
	if (!eventWindow) {
	    return theEvent;
	}
    }
    if (!winPtr) {
	eventWindow = [NSApp mainWindow];
	winPtr = TkMacOSXGetTkWindow(eventWindow);
    }
    if (!winPtr) {

	/*
	 * We couldn't find a Tk window for this event.  We have to ignore it.
	 */

#ifdef TK_MAC_DEBUG_EVENTS
	TkMacOSXDbgMsg("Event received with no Tk window.");
#endif
	return theEvent;
    }
    tkwin = (Tk_Window) winPtr;

    /*
     * Compute the mouse position in local (window) and global (screen)
     * coordinates.  These are Tk coordinates, meaning that the local origin is
     * at the top left corner of the containing toplevel and the global origin
     * is at top left corner of the primary screen.
     */

    global = [NSEvent mouseLocation];
    local = [eventWindow tkConvertPointFromScreen: global];
    global.x = floor(global.x);
    global.y = floor(TkMacOSXZeroScreenHeight() - global.y);
    local.x = floor(local.x);
    local.y = floor([eventWindow frame].size.height - local.y);
    if (Tk_IsEmbedded(winPtr)) {
	TkWindow *contPtr = TkpGetOtherWindow(winPtr);
	if (Tk_IsTopLevel(contPtr)) {
	    local.x -= contPtr->wmInfoPtr->xInParent;
	    local.y -= contPtr->wmInfoPtr->yInParent;
	} else {
	    TkWindow *topPtr = TkMacOSXGetHostToplevel(winPtr)->winPtr;
	    local.x -= (topPtr->wmInfoPtr->xInParent + contPtr->changes.x);
	    local.y -= (topPtr->wmInfoPtr->yInParent + contPtr->changes.y);
	}
    } else {
	local.x -= winPtr->wmInfoPtr->xInParent;
	local.y -= winPtr->wmInfoPtr->yInParent;
    }

    /*
     * Use the local coordinates to find the Tk window which should receive
     * this event.  Also convert local into the coordinates of that window.
     * (The converted local coordinates are only needed for scrollwheel
     * events.)
     */

    target = Tk_TopCoordsToWindow(tkwin, local.x, local.y, &win_x, &win_y);

    /*
     * Ignore the event if a local grab is in effect and the Tk window is
     * not in the grabber's subtree.
     */

    grabWinPtr = winPtr->dispPtr->grabWinPtr;
    if (grabWinPtr && /* There is a grab in effect ... */
	!winPtr->dispPtr->grabFlags && /* and it is a local grab ... */
	grabWinPtr->mainPtr == winPtr->mainPtr){ /* in the same application. */
	Tk_Window tkwin2;
	if (!target) {
	    return theEvent;
	}
	for (tkwin2 = target;
	     !Tk_IsTopLevel(tkwin2);
	     tkwin2 = Tk_Parent(tkwin2)) {
	    if (tkwin2 == (Tk_Window)grabWinPtr) {
		break;
	    }
	}
	if (tkwin2 != (Tk_Window)grabWinPtr) {
	    return theEvent;
	}
    }

    /*
     *  Generate an XEvent for this mouse event.
     */

    unsigned int state = buttonState;
    NSUInteger modifiers = [theEvent modifierFlags];

    if (modifiers & NSAlphaShiftKeyMask) {
	state |= LockMask;
    }
    if (modifiers & NSShiftKeyMask) {
	state |= ShiftMask;
    }
    if (modifiers & NSControlKeyMask) {
	state |= ControlMask;
    }
    if (modifiers & NSCommandKeyMask) {
	state |= Mod1Mask;		/* command key */
    }
    if (modifiers & NSAlternateKeyMask) {
	state |= Mod2Mask;		/* option key */
    }
    if (modifiers & NSNumericPadKeyMask) {
	state |= Mod3Mask;
    }
    if (modifiers & NSFunctionKeyMask) {
	state |= Mod4Mask;
    }

    if (eventType != NSScrollWheel) {

	/*
	 * For normal mouse events, Tk_UpdatePointer will send the appropriate
	 * XEvents using its cached state information.  Unfortunately, it will
	 * also recompute the local coordinates.
	 */

#ifdef TK_MAC_DEBUG_EVENTS
	TKLog(@"UpdatePointer %p x %.1f y %.1f %d",
		target, global.x, global.y, state);
#endif

	Tk_UpdatePointer(target, global.x, global.y, state);
    } else {
	CGFloat delta;
	int coarseDelta;
	XEvent xEvent;

	/*
	 * For scroll wheel events we need to send the XEvent here.
	 */

	xEvent.type = MouseWheelEvent;
	xEvent.xbutton.x = win_x;
	xEvent.xbutton.y = win_y;
	xEvent.xbutton.x_root = global.x;
	xEvent.xbutton.y_root = global.y;
	xEvent.xany.send_event = false;
	xEvent.xany.display = Tk_Display(target);
	xEvent.xany.window = Tk_WindowId(target);

	delta = [theEvent deltaY];
	if (delta != 0.0) {
	    coarseDelta = (delta > -1.0 && delta < 1.0) ?
		    (signbit(delta) ? -1 : 1) : lround(delta);
	    xEvent.xbutton.state = state;
	    xEvent.xkey.keycode = coarseDelta;
	    xEvent.xany.serial = LastKnownRequestProcessed(Tk_Display(tkwin));
	    Tk_QueueWindowEvent(&xEvent, TCL_QUEUE_TAIL);
	}
	delta = [theEvent deltaX];
	if (delta != 0.0) {
	    coarseDelta = (delta > -1.0 && delta < 1.0) ?
		    (signbit(delta) ? -1 : 1) : lround(delta);
	    xEvent.xbutton.state = state | ShiftMask;
	    xEvent.xkey.keycode = coarseDelta;
	    xEvent.xany.serial = LastKnownRequestProcessed(Tk_Display(tkwin));
	    Tk_QueueWindowEvent(&xEvent, TCL_QUEUE_TAIL);
	}
    }
    return theEvent;
}
@end

#pragma mark -

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXButtonKeyState --
 *
 *	Returns the current state of the button & modifier keys.
 *
 * Results:
 *	A bitwise inclusive OR of a subset of the following: Button1Mask,
 *	ShiftMask, LockMask, ControlMask, Mod*Mask.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned int
TkMacOSXButtonKeyState(void)
{
    UInt32 buttonState = 0, keyModifiers;
    int isFrontProcess = (GetCurrentEvent() && Tk_MacOSXIsAppInFront());

    buttonState = isFrontProcess ? GetCurrentEventButtonState() :
	    GetCurrentButtonState();
    keyModifiers = isFrontProcess ? GetCurrentEventKeyModifiers() :
	    GetCurrentKeyModifiers();

    return ButtonModifiers2State(buttonState, keyModifiers);
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonModifiers2State --
 *
 *	Converts Carbon mouse button state and modifier values into a Tk
 *	button/modifier state.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static unsigned int
ButtonModifiers2State(
    UInt32 buttonState,
    UInt32 keyModifiers)
{
    unsigned int state;

    /*
     * Tk on OSX supports at most 5 buttons.
     */

    state = (buttonState & 0x1F) * Button1Mask;

    if (keyModifiers & alphaLock) {
	state |= LockMask;
    }
    if (keyModifiers & shiftKey) {
	state |= ShiftMask;
    }
    if (keyModifiers & controlKey) {
	state |= ControlMask;
    }
    if (keyModifiers & cmdKey) {
	state |= Mod1Mask;		/* command key */
    }
    if (keyModifiers & optionKey) {
	state |= Mod2Mask;		/* option key */
    }
    if (keyModifiers & kEventKeyModifierNumLockMask) {
	state |= Mod3Mask;
    }
    if (keyModifiers & kEventKeyModifierFnMask) {
	state |= Mod4Mask;
    }

    return state;
}

/*
 *----------------------------------------------------------------------
 *
 * XQueryPointer --
 *
 *	Check the current state of the mouse. This is not a complete
 *	implementation of this function. It only computes the root coordinates
 *	and the current mask.
 *
 * Results:
 *	Sets root_x_return, root_y_return, and mask_return. Returns true on
 *	success.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
XQueryPointer(
    Display *display,
    Window w,
    Window *root_return,
    Window *child_return,
    int *root_x_return,
    int *root_y_return,
    int *win_x_return,
    int *win_y_return,
    unsigned int *mask_return)
{
    int getGlobal = (root_x_return && root_y_return);
    int getLocal = (win_x_return && win_y_return && w != None);

    if (getGlobal || getLocal) {
	NSPoint global = [NSEvent mouseLocation];

	if (getLocal) {
	    MacDrawable *macWin = (MacDrawable *)w;
	    NSWindow *win = TkMacOSXGetNSWindowForDrawable(w);

	    if (win) {
		NSPoint local;

		local = [win tkConvertPointFromScreen:global];
		local.y = [win frame].size.height - local.y;
		if (macWin->winPtr && macWin->winPtr->wmInfoPtr) {
		    local.x -= macWin->winPtr->wmInfoPtr->xInParent;
		    local.y -= macWin->winPtr->wmInfoPtr->yInParent;
		}
		*win_x_return = local.x;
		*win_y_return = local.y;
	    }
	}
	if (getGlobal) {
	    *root_x_return = global.x;
	    *root_y_return = TkMacOSXZeroScreenHeight() - global.y;
	}
    }
    if (mask_return) {
	*mask_return = TkMacOSXButtonKeyState();
    }
    return True;
}

/*
 *----------------------------------------------------------------------
 *
 * TkGenerateButtonEventForXPointer --
 *
 *	This procedure generates an X button event for the current pointer
 *	state as reported by XQueryPointer().
 *
 * Results:
 *	True if event(s) are generated - false otherwise.
 *
 * Side effects:
 *	Additional events may be placed on the Tk event queue. Grab state may
 *	also change.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
TkGenerateButtonEventForXPointer(
    Window window)		/* X Window containing button event. */
{
    MouseEventData med;
    int global_x, global_y, local_x, local_y;

    bzero(&med, sizeof(MouseEventData));
    XQueryPointer(NULL, window, NULL, NULL, &global_x, &global_y,
	    &local_x, &local_y, &med.state);
    med.global.h = global_x;
    med.global.v = global_y;
    med.local.h = local_x;
    med.local.v = local_y;
    med.window = window;

    return GenerateButtonEvent(&med);
}

/*
 *----------------------------------------------------------------------
 *
 * TkGenerateButtonEvent --
 *
 *	Given a global x & y position and the button key status this procedure
 *	generates the appropriate X button event. It also handles the state
 *	changes needed to implement implicit grabs.
 *
 * Results:
 *	True if event(s) are generated, false otherwise.
 *
 * Side effects:
 *	Additional events may be placed on the Tk event queue. Grab state may
 *	also change.
 *
 *----------------------------------------------------------------------
 */

int
TkGenerateButtonEvent(
    int x,			/* X location of mouse, */
    int y,			/* Y location of mouse. */
    Window window,		/* X Window containing button event. */
    unsigned int state)		/* Button Key state suitable for X event. */
{
    MacDrawable *macWin = (MacDrawable *)window;
    NSWindow *win = TkMacOSXGetNSWindowForDrawable(window);
    MouseEventData med;

    bzero(&med, sizeof(MouseEventData));
    med.state = state;
    med.window = window;
    med.global.h = x;
    med.global.v = y;
    med.local = med.global;

    if (win) {
	NSPoint local = NSMakePoint(x, TkMacOSXZeroScreenHeight() - y);

	local = [win tkConvertPointFromScreen:local];
	local.y = [win frame].size.height - local.y;
	if (macWin->winPtr && macWin->winPtr->wmInfoPtr) {
	    local.x -= macWin->winPtr->wmInfoPtr->xInParent;
	    local.y -= macWin->winPtr->wmInfoPtr->yInParent;
	}
	med.local.h = local.x;
	med.local.v = TkMacOSXZeroScreenHeight() - local.y;
    }

    return GenerateButtonEvent(&med);
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateButtonEvent --
 *
 *	Generate an X button event from a MouseEventData structure. Handles
 *	the state changes needed to implement implicit grabs.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.
 *
 * Side effects:
 *	Additional events may be placed on the Tk event queue. Grab state may
 *	also change.
 *
 *----------------------------------------------------------------------
 */

static int
GenerateButtonEvent(
    MouseEventData *medPtr)
{
    Tk_Window tkwin;
    int dummy;
    TkDisplay *dispPtr;

#if UNUSED

    /*
     * ButtonDown events will always occur in the front window. ButtonUp
     * events, however, may occur anywhere on the screen. ButtonUp events
     * should only be sent to Tk if in the front window or during an implicit
     * grab.
     */

    if ((medPtr->activeNonFloating == NULL)
	    || ((!(TkpIsWindowFloating(medPtr->whichWin))
	    && (medPtr->activeNonFloating != medPtr->whichWin))
	    && TkMacOSXGetCapture() == NULL)) {
	return false;
    }
#endif

    dispPtr = TkGetDisplayList();
    tkwin = Tk_IdToWindow(dispPtr->display, medPtr->window);

    if (tkwin != NULL) {
	tkwin = Tk_TopCoordsToWindow(tkwin, medPtr->local.h, medPtr->local.v,
		&dummy, &dummy);
    }

    Tk_UpdatePointer(tkwin, medPtr->global.h, medPtr->global.v, medPtr->state);
    return true;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpWarpPointer --
 *
 *	Move the mouse cursor to the screen location specified by the warpX and
 *	warpY fields of a TkDisplay.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The mouse cursor is moved.
 *
 *----------------------------------------------------------------------
 */

void
TkpWarpPointer(
    TkDisplay *dispPtr)
{
    CGPoint pt;

    if (dispPtr->warpWindow) {
	int x, y;
	Tk_GetRootCoords(dispPtr->warpWindow, &x, &y);
	pt.x = x + dispPtr->warpX;
	pt.y = y + dispPtr->warpY;
    } else {
	pt.x = dispPtr->warpX;
	pt.y = dispPtr->warpY;
    }

    CGWarpMouseCursorPosition(pt);

    if (dispPtr->warpWindow) {
        TkGenerateButtonEventForXPointer(Tk_WindowId(dispPtr->warpWindow));
    } else {
        TkGenerateButtonEventForXPointer(None);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpSetCapture --
 *
 *	This function captures the mouse so that all future events will be
 *	reported to this window, even if the mouse is outside the window. If
 *	the specified window is NULL, then the mouse is released.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the capture flag and captures the mouse.
 *
 *----------------------------------------------------------------------
 */

void
TkpSetCapture(
    TkWindow *winPtr)		/* Capture window, or NULL. */
{
    while (winPtr && !Tk_IsTopLevel(winPtr)) {
	winPtr = winPtr->parentPtr;
    }
    captureWinPtr = (Tk_Window)winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetCapture --
 *
 * Results:
 *	Returns the current grab window
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
TkMacOSXGetCapture(void)
{
    return captureWinPtr;
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
