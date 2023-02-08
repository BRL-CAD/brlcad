/*
 * tkMacOSXMouseEvent.c --
 *
 *	This file implements functions that decode & handle mouse events on
 *	MacOS X.
 *
 * Copyright (c) 2001-2009, Apple Inc.
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

#pragma mark TKApplication(TKMouseEvent)

enum {
    NSWindowWillMoveEventType = 20
};

/*
 * In OS X 10.6 an NSEvent of type NSMouseMoved would always have a non-Nil
 * window attribute pointing to the key window.  As of 10.8 this behavior had
 * changed.  The new behavior was that if the mouse were ever moved outside of
 * a window, all subsequent NSMouseMoved NSEvents would have a Nil window
 * attribute until the mouse returned to the window.  In 11.1 it changed again.
 * The window attribute can be non-nil, but referencing a window which does not
 * belong to the application.
 */

/* The basic job of tkProcessMouseEvent is to generate a call to
 * Tk_UpdatePointer.  That function receives a Tk_Window which (ignoring cases
 * when a grab is in effect) should be the highest window within the focused
 * toplevel that contains the pointer, as well as the pointer location in
 * screen coordinates and the current button state.  Tk maintains a cache of
 * these three values.  A change in any of these values causes Tk_UpdatePointer
 * to generate, respectively, Enter/Leave events, or Motion events, or
 * button Press/Release events. The Tk_Window value is allowed to be NULL,
 * which indicates that the pointer is not in the focused toplevel.
 *
 * Enter or Leave events for toplevel windows are generated when the Tk_Window
 * value changes to or from NULL.  This is problematic on macOS due to the fact
 * that Tk_UpdatePointer does not generate Motion events when the Tk_Window
 * value is NULL.  A consequence of this is that Tk_UpdatePointer will either
 * fail to generate correct Enter/Leave events for toplevels or else be unable
 * to generate Motion events when the pointer is outside of the focus window.
 * It is important to be able to generate such events because otherwise a
 * scrollbar on the edge of a toplevel becomes unusable.  Any time that the
 * pointer wanders out of the window during a scroll, the scroll will stop.
 * That is an extremely annoying and unexpected behavior.  Much of the code in
 * this module, including the trickiest parts, is devoted to working around
 * this problem.  The other tricky parts are related to transcribing Apple's
 * NSMouseEntered, NSMouseExited, and NSLeftMouseDragged events into a form
 * that makes sense to Tk.
 */


@implementation TKApplication(TKMouseEvent)

- (NSEvent *) tkProcessMouseEvent: (NSEvent *) theEvent
{
    NSWindow *eventWindow = [theEvent window];
    NSEventType eventType = [theEvent type];
    TKContentView *contentView = [eventWindow contentView];
    NSPoint location = [theEvent locationInWindow];
    NSPoint viewLocation = [contentView convertPoint:location fromView:nil];
    TkWindow *winPtr = NULL, *grabWinPtr, *scrollTarget = NULL;
    Tk_Window tkwin = NULL, capture, target;
    NSPoint local, global;
    NSInteger button;
    TkWindow *newFocus = NULL;
    int win_x, win_y;
    unsigned int buttonState = 0;
    Bool isTestingEvent = NO;
    Bool isMotionEvent = NO;
    Bool isOutside = NO;
    Bool firstDrag = NO;
    static Bool ignoreDrags = NO;
    static Bool ignoreUpDown = NO;
    static NSTimeInterval timestamp = 0;

#ifdef TK_MAC_DEBUG_EVENTS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, theEvent);
#endif

    /*
     * If this event is not for a Tk toplevel, it should normally just be
     * passed up the responder chain.  However, there is are two exceptions.
     * One is for synthesized events, which are used in testing.  Those events
     * are recognized by having their timestamp set to 0.  The other is for
     * motion events sent by the local event monitor, which will have their
     * window attribute set to nil.
     */

    if (![eventWindow isMemberOfClass:[TKWindow class]]) {
	if ([theEvent timestamp] == 0) {
	    isTestingEvent = YES;
	    eventWindow = [NSApp keyWindow];
	}
	if (eventType == NSLeftMouseDragged ||
	    eventType == NSMouseMoved) {
	    eventWindow = [NSApp keyWindow];
	    isMotionEvent = YES;
	}
	if (!isTestingEvent && !isMotionEvent) {
	    return theEvent;
	}
    } else if (!NSPointInRect(viewLocation, [contentView bounds])) {
	isOutside = YES;
    }
    button = [theEvent buttonNumber] + Button1;
    switch (eventType) {
    case NSRightMouseUp:
    case NSOtherMouseUp:
	buttonState &= ~TkGetButtonMask(button);
	break;
    case NSLeftMouseDragged:
	buttonState |= TkGetButtonMask(button);
	if (![NSApp tkDragTarget]) {
	    if (isOutside) {
		ignoreDrags = YES;
	    } else {
		firstDrag = YES;
	    }
	}
	if (ignoreDrags) {
	    return theEvent;
	}
	if (![NSApp tkDragTarget]) {
	    [NSApp setTkDragTarget: [NSApp tkEventTarget]];
	}
	break;
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
	isMotionEvent = YES;
    case NSRightMouseDown:
    case NSOtherMouseDown:
	buttonState |= TkGetButtonMask(button);
	break;
    case NSMouseEntered:
	if (![eventWindow isKeyWindow] || isOutside) {
	    return theEvent;
	}
	[NSApp setTkLiveResizeEnded:NO];
	[NSApp setTkPointerWindow:[NSApp tkEventTarget]];
	break;
    case NSMouseExited:
	if (![eventWindow isKeyWindow] || !isOutside) {
	    return theEvent;
	}
	[NSApp setTkPointerWindow:nil];
	break;
    case NSLeftMouseUp:
	[NSApp setTkDragTarget: nil];
	if ([theEvent clickCount] == 2) {
	    ignoreUpDown = NO;
	}
	if (ignoreUpDown) {
	    return theEvent;
	}
	if (ignoreDrags) {
	    ignoreDrags = NO;
	    return theEvent;
	}
	buttonState &= ~TkGetButtonMask(Button1);
	break;
    case NSLeftMouseDown:

	/*
	 * There are situations where Apple does not send NSLeftMouseUp events
	 * even though the mouse button has been released.  One of these is
	 * whenever a menu is posted on the screen.  This causes Tk to have an
	 * inaccurate idea of the current button state and to behave strangely.
	 * (See ticket[a132b5507e].)  As a work-around we watch for NSButtonDown
	 * events that arrive when Tk thinks the button is already down and
	 * we attempt to correct Tk's cached button state by insering a call to
	 * Tk_UpdatePointer showing the button as up.
	 */

	if ([NSApp tkButtonState] & TkGetButtonMask(Button1)) {
	    int fakeState = [NSApp tkButtonState] & ~TkGetButtonMask(Button1);
	    int x = location.x;
	    int y = floor(TkMacOSXZeroScreenHeight() - location.y);
	    Tk_UpdatePointer((Tk_Window) [NSApp tkEventTarget], x, y, fakeState);
	}

	/*
	 * Ignore left mouse button events which are in an NSWindow but outside
	 * of its contentView (see tickets [d72abe6b54] and [39cbacb9e8]).
	 * Ignore the first left button press after a live resize ends. (Apple
	 * sends the button press event that started the resize after the
	 * resize ends.  It should not be seen by Tk.  See tickets [d72abe6b54]
	 * and [39cbacb9e8]).  Ignore button press events when ignoreUpDown is
	 * set.  These are extraneous events which appear when double-clicking
	 * in a window without focus, causing duplicate Double-1 events (see
	 * ticket [7bda9882cb]).  When a LeftMouseDown event with clickCount 2
	 * is received we set the ignoreUpDown flag and we clear it when the
	 * matching LeftMouseUp with click count 2 is received.
	 */

	/*
	 * Make sure we don't ignore LeftMouseUp and LeftMouseDown forever.
	 * Currently tkBind.c sets NEARBY_MS to 500 (the Windows default).
	 */

	if ([theEvent timestamp] - timestamp > 1) {
	    ignoreUpDown = NO;
	}
	if ([theEvent clickCount] == 2) {
	    if (ignoreUpDown == YES) {
		return theEvent;
	    } else {
		timestamp = [theEvent timestamp];
		ignoreUpDown = YES;
	    }
	}
	if (!isTestingEvent) {
	    NSRect bounds = [contentView bounds];
	    NSRect grip = NSMakeRect(bounds.size.width - 10, 0, 10, 10);
	    bounds = NSInsetRect(bounds, 2.0, 2.0);
	    if (!NSPointInRect(viewLocation, bounds)) {
		return theEvent;
	    }
	    if (NSPointInRect(viewLocation, grip)) {
		return theEvent;
	    }
	    if ([NSApp tkLiveResizeEnded]) {
		[NSApp setTkLiveResizeEnded:NO];
		return theEvent;
	    }
	}

	/*
	 * If this click will change the focus, the Tk event should
	 * be sent to the toplevel which will be receiving focus rather than to
	 * the current focus window.  So reset tkEventTarget.
	 */

	if (eventWindow != [NSApp keyWindow]) {
	    NSWindow *w;

	    if (eventWindow && isOutside) {
		return theEvent;
	    }
	    for (w in [NSApp orderedWindows]) {
		if (NSPointInRect([NSEvent mouseLocation], [w frame])) {
		    newFocus = TkMacOSXGetTkWindow(w);
		    break;
		}
	    }
	    if (newFocus) {
		[NSApp setTkEventTarget: newFocus];
		[NSApp setTkPointerWindow: newFocus];
		target = (Tk_Window) newFocus;
	    }
	}
	buttonState |= TkGetButtonMask(Button1);
	break;
    case NSMouseMoved:
	if (eventWindow && eventWindow != [NSApp keyWindow]) {
	    return theEvent;
	}
	isMotionEvent = YES;
	break;
    case NSScrollWheel:

	/*
	 * Scroll wheel events are sent to the window containing the pointer,
	 * or ignored if no window contains the pointer.  See TIP #171.  Note,
	 * however, that TIP #171 proposed sending scroll wheel events to the
	 * focus window when no window contains the pointer.  That proposal was
	 * ultimately rejected.
	 */

	scrollTarget = TkMacOSXGetTkWindow(eventWindow);
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
     * Find the toplevel window for the event.
     */

    if ([NSApp tkDragTarget]) {
	TkWindow *dragPtr = (TkWindow *) [NSApp tkDragTarget];
	TKWindow *dragWindow = nil;
	if (dragPtr) {
	    dragWindow = (TKWindow *)TkMacOSXGetNSWindowForDrawable(
			    dragPtr->window);
	}
	if (!dragWindow) {
	    [NSApp setTkDragTarget: nil];
	    target = NULL;
	    return theEvent;
	}
	winPtr = TkMacOSXGetHostToplevel((TkWindow *) [NSApp tkDragTarget])->winPtr;
    } else if (eventType == NSScrollWheel) {
	winPtr = scrollTarget;
    } else {
	winPtr = [NSApp tkEventTarget];
    }
    if (!winPtr) {

	/*
	 * If we couldn't find a toplevel for this event we have to ignore it.
	 * (But this should never happen.)
	 */

#ifdef TK_MAC_DEBUG_EVENTS
	TkMacOSXDbgMsg("Event received with no Tk window.");
#endif

	return theEvent;
    }
    tkwin = (Tk_Window) winPtr;

    /*
     * Compute the mouse position in local (toplevel) and global (screen)
     * coordinates.  These are Tk coordinates, meaning that the local origin is
     * at the top left corner of the containing toplevel and the global origin
     * is at top left corner of the primary screen.
     */

    global = [NSEvent mouseLocation];
    local = [eventWindow tkConvertPointFromScreen: global];
    global.x = floor(global.x);
    global.y = floor(TkMacOSXZeroScreenHeight() - global.y);
    local.x = floor(local.x);
    local.y = floor(eventWindow.frame.size.height - local.y);
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
    }
    else {
    	if (winPtr && winPtr->wmInfoPtr) {
    	    local.x -= winPtr->wmInfoPtr->xInParent;
    	    local.y -= winPtr->wmInfoPtr->yInParent;
    	} else {
    	    return theEvent;
    	}
    }

    /*
     * Use the toplevel coordinates to decide which Tk window should receive
     * this event.  Also convert the toplevel coordinates into the coordinate
     * system of that window.  These converted coordinates are needed for
     * XEvents that we generate, namely ScrollWheel events and Motion events
     * when the mouse is outside of the focused toplevel.
     */

    if ([NSApp tkDragTarget]) {
	Tk_Window dragTarget = (Tk_Window) [NSApp tkDragTarget];
	Tk_Window dragWidget = NULL;
	int x, y;
	Tk_GetRootCoords(dragTarget, &x, &y);
	win_x = global.x - x;
	win_y = global.y - y;
	if (firstDrag) {
	    dragWidget = Tk_TopCoordsToWindow(tkwin, local.x, local.y, &x, &y);
	    if (dragWidget) {
		[NSApp setTkDragTarget: (TkWindow *) dragWidget];
	    }
	}
	target = (Tk_Window) [NSApp tkDragTarget];
    } else {
	target = Tk_TopCoordsToWindow(tkwin, local.x, local.y, &win_x, &win_y);
    }


    grabWinPtr = winPtr->dispPtr->grabWinPtr;

    /*
     * Ignore the event if a local grab is in effect and the Tk window is
     * not in the grabber's subtree.
     */

    if (grabWinPtr && /* There is a grab in effect ... */
	!winPtr->dispPtr->grabFlags && /* and it is a local grab ... */
	grabWinPtr->mainPtr == winPtr->mainPtr){ /* in the same application. */
	Tk_Window w;
	if (!target) {
	    return theEvent;
	}
	for (w = target; !Tk_IsTopLevel(w); w = Tk_Parent(w)) {
	    if (w == (Tk_Window)grabWinPtr) {
		break;
	    }
	}
	if (w != (Tk_Window)grabWinPtr) {
	    return theEvent;
	}
    }

    /*
     * Ignore the event if a global grab is in effect and the Tk window is
     * not in the grabber's subtree.
     */

    if (grabWinPtr && /* There is a grab in effect ... */
	winPtr->dispPtr->grabFlags && /* and it is a global grab ... */
	grabWinPtr->mainPtr == winPtr->mainPtr) { /* in the same application. */
	Tk_Window w;
	if (!target) {
	    return theEvent;
	}
	for (w = target; !Tk_IsTopLevel(w); w = Tk_Parent(w)) {
	    if (w == (Tk_Window)grabWinPtr) {
		break;
	    }
	}
	if (w != (Tk_Window)grabWinPtr) {
	    /* Force the focus back to the grab window. */
	    TkpChangeFocus(grabWinPtr, 1);
	}
    }

    /*
     *  Translate the current button state into Tk's format.
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
    [NSApp setTkButtonState:state];

    /*
     * Send XEvents.  We do this here for Motion events outside of the focused
     * toplevel and for MouseWheel events.  In other cases the XEvents will be
     * sent when we call Tk_UpdatePointer.
     */

    if (eventType != NSScrollWheel) {
	if ([NSApp tkDragTarget]) {

	    /*
	     * When dragging the mouse into the resize area Apple shows the
	     * left button to be up, which confuses Tk_UpdatePointer.  So
	     * we make sure that the button state appears the way that Tk
	     * expects.
	     */

	    state |= TkGetButtonMask(Button1);
	}
	if (eventType == NSMouseEntered) {
	    Tk_UpdatePointer((Tk_Window) [NSApp tkPointerWindow],
				 global.x, global.y, state);
	} else if (eventType == NSMouseExited) {
	    if ([NSApp tkDragTarget]) {
	    	Tk_UpdatePointer((Tk_Window) [NSApp tkDragTarget],
	    			 global.x, global.y, state);
	    } else {
	    Tk_UpdatePointer(NULL, global.x, global.y, state);
	    }
	} else if (eventType == NSMouseMoved ||
		   eventType == NSLeftMouseDragged) {
	    if ([NSApp tkPointerWindow]) {
		Tk_UpdatePointer(target, global.x, global.y, state);
	    } else {
		XEvent xEvent = {0};

		xEvent.type = MotionNotify;
		xEvent.xany.send_event = false;
		xEvent.xany.display = Tk_Display(target);
		xEvent.xany.window = Tk_WindowId(target);
		xEvent.xany.serial = LastKnownRequestProcessed(Tk_Display(tkwin));
		xEvent.xmotion.x = win_x;
		xEvent.xmotion.y = win_y;
		xEvent.xmotion.x_root = global.x;
		xEvent.xmotion.y_root = global.y;
		xEvent.xmotion.state = state;
		Tk_QueueWindowEvent(&xEvent, TCL_QUEUE_TAIL);

		/*
		 * Tk_UpdatePointer must not be called in this case.  Doing so
		 * will break scrollbars; dragging will stop when the mouse
		 * leaves the window.
		 */

	    }
	} else {
	    Tk_UpdatePointer(target, global.x, global.y, state);
	}
    } else {
	CGFloat delta;
	int coarseDelta;
	XEvent xEvent = {0};

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

    /*
     * If button events are being captured, and the target is not in the
     * subtree below the capturing window, then the NSEvent should not be sent
     * up the responder chain.  This avoids, for example, beeps when clicking
     * the mouse button outside of a posted combobox.  See ticket [eb26d4ec8e].
     */

    capture = TkMacOSXGetCapture();
    if (capture && eventType == NSLeftMouseDown) {
	Tk_Window w;
	for (w = target; w != NULL;  w = Tk_Parent(w)) {
	    if (w == capture) {
		break;
	    }
	}
	if (w != capture) {
	    return nil;
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
    return [NSApp tkButtonState];
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
    TCL_UNUSED(Display *),
    Window w,
    TCL_UNUSED(Window *),
    TCL_UNUSED(Window *),
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

#ifdef UNUSED

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
