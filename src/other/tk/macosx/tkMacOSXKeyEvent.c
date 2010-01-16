/*
 * tkMacOSXKeyEvent.c --
 *
 *	This file implements functions that decode & handle keyboard events on
 *	MacOS X.
 *
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXEvent.h"

/*
#ifdef TK_MAC_DEBUG
#define TK_MAC_DEBUG_KEYBOARD
#endif
*/

static Tk_Window grabWinPtr = NULL;
				/* Current grab window, NULL if no grab. */
static Tk_Window keyboardGrabWinPtr = NULL;
				/* Current keyboard grab window. */
static NSModalSession modalSession = NULL;

#pragma mark TKApplication(TKKeyEvent)

@implementation TKApplication(TKKeyEvent)
- (NSEvent *) tkProcessKeyEvent: (NSEvent *) theEvent
{
#ifdef TK_MAC_DEBUG_EVENTS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, theEvent);
#endif
    NSWindow*	    w;
    NSEventType	    type = [theEvent type];
    NSUInteger	    modifiers, len;
    BOOL	    repeat = NO;
    unsigned short  keyCode;
    NSString	    *characters = nil, *charactersIgnoringModifiers = nil;
    static NSUInteger savedModifiers = 0;

    switch (type) {
    case NSKeyUp:
    case NSKeyDown:
	repeat = [theEvent isARepeat];
	characters = [theEvent characters];
	charactersIgnoringModifiers = [theEvent charactersIgnoringModifiers];
    case NSFlagsChanged:
	modifiers = [theEvent modifierFlags];
	keyCode = [theEvent keyCode];
	w = [self windowWithWindowNumber:[theEvent windowNumber]];
#ifdef TK_MAC_DEBUG_EVENTS
	TKLog(@"-[%@(%p) %s] %d %u %@ %@ %u %@", [self class], self, _cmd, repeat, modifiers, characters, charactersIgnoringModifiers, keyCode, w);
#endif
	break;

    default:
	return theEvent;
    }

    unsigned int state = 0;

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

    /*
     * The focus must be in the FrontWindow on the Macintosh. We then query Tk
     * to determine the exact Tk window that owns the focus.
     */

    TkWindow *winPtr = TkMacOSXGetTkWindow(w);
    Tk_Window tkwin = (Tk_Window) winPtr;

    if (!tkwin) {
	TkMacOSXDbgMsg("tkwin == NULL");
	return theEvent;
    }
    tkwin = (Tk_Window) winPtr->dispPtr->focusPtr;
    if (!tkwin) {
	TkMacOSXDbgMsg("tkwin == NULL");
	return theEvent;
    }

    XEvent xEvent;

    memset(&xEvent, 0, sizeof(XEvent));
    xEvent.xany.serial = LastKnownRequestProcessed(Tk_Display(tkwin));
    xEvent.xany.send_event = false;
    xEvent.xany.display = Tk_Display(tkwin);
    xEvent.xany.window = Tk_WindowId(tkwin);

    xEvent.xkey.root = XRootWindow(Tk_Display(tkwin), 0);
    xEvent.xkey.subwindow = None;
    xEvent.xkey.time = TkpGetMS();
    xEvent.xkey.state = state;
    xEvent.xkey.same_screen = true;
    xEvent.xkey.trans_chars[0] = 0;
    xEvent.xkey.nbytes = 0;

    if (type == NSFlagsChanged) {
	if (savedModifiers > modifiers) {
	    xEvent.xany.type = KeyRelease;
	} else {
	    xEvent.xany.type = KeyPress;
	}

	/*
	 * Use special '-1' to signify a special keycode to our platform
	 * specific code in tkMacOSXKeyboard.c. This is rather like what
	 * happens on Windows.
	 */

	xEvent.xany.send_event = -1;

	/*
	 * Set keycode (which was zero) to the changed modifier
	 */

	xEvent.xkey.keycode = (modifiers ^ savedModifiers);
    } else {
	if (type == NSKeyUp || repeat) {
	    xEvent.xany.type = KeyRelease;
	} else {
	    xEvent.xany.type = KeyPress;
	}
	xEvent.xkey.keycode = (keyCode << 16) | (UInt16)
		[characters characterAtIndex:0];
	if (![characters getCString:xEvent.xkey.trans_chars
		maxLength:XMaxTransChars encoding:NSUTF8StringEncoding]) {
	    TkMacOSXDbgMsg("characters too long");
	    return theEvent;
	}
	len = [charactersIgnoringModifiers length];
	if (len) {
	    xEvent.xkey.nbytes = [charactersIgnoringModifiers characterAtIndex:0];
	    if (len > 1) {
		TkMacOSXDbgMsg("more than one charactersIgnoringModifiers");
	    }
	}
	if (repeat) {
	    Tk_QueueWindowEvent(&xEvent, TCL_QUEUE_TAIL);
	    xEvent.xany.type = KeyPress;
	    xEvent.xany.serial = LastKnownRequestProcessed(Tk_Display(tkwin));
	}
    }
    Tk_QueueWindowEvent(&xEvent, TCL_QUEUE_TAIL);
    savedModifiers = modifiers;

    return theEvent;
}
@end

#pragma mark -

/*
 *----------------------------------------------------------------------
 *
 * XGrabKeyboard --
 *
 *	Simulates a keyboard grab by setting the focus.
 *
 * Results:
 *	Always returns GrabSuccess.
 *
 * Side effects:
 *	Sets the keyboard focus to the specified window.
 *
 *----------------------------------------------------------------------
 */

int
XGrabKeyboard(
    Display* display,
    Window grab_window,
    Bool owner_events,
    int pointer_mode,
    int keyboard_mode,
    Time time)
{
    keyboardGrabWinPtr = Tk_IdToWindow(display, grab_window);
    if (keyboardGrabWinPtr && grabWinPtr) {
	NSWindow *w = TkMacOSXDrawableWindow(grab_window);
	MacDrawable *macWin = (MacDrawable *) grab_window;

	if (w && macWin->toplevel->winPtr == (TkWindow*) grabWinPtr) {
	    if (modalSession) {
		Tcl_Panic("XGrabKeyboard: already grabbed");
	    }
	    modalSession = [NSApp beginModalSessionForWindow:[w retain]];
	}
    }
    return GrabSuccess;
}

/*
 *----------------------------------------------------------------------
 *
 * XUngrabKeyboard --
 *
 *	Releases the simulated keyboard grab.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the keyboard focus back to the value before the grab.
 *
 *----------------------------------------------------------------------
 */

void
XUngrabKeyboard(
    Display* display,
    Time time)
{
    if (modalSession) {
	NSWindow *w = keyboardGrabWinPtr ? TkMacOSXDrawableWindow(
		((TkWindow *) keyboardGrabWinPtr)->window) : nil;
	[NSApp endModalSession:modalSession];
	[w release];
	modalSession = NULL;
    }
    keyboardGrabWinPtr = NULL;
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
    return grabWinPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetModalSession --
 *
 * Results:
 *	Returns the current modal session
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE NSModalSession
TkMacOSXGetModalSession(void)
{
    return modalSession;
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
    grabWinPtr = (Tk_Window) winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SetCaretPos --
 *
 *	This enables correct placement of the XIM caret. This is called by
 *	widgets to indicate their cursor placement, and the caret location is
 *	used by TkpGetString to place the XIM caret.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
Tk_SetCaretPos(
    Tk_Window tkwin,
    int x,
    int y,
    int height)
{
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
