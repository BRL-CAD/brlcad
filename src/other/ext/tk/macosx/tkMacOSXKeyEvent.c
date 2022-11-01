/*
 * tkMacOSXKeyEvent.c --
 *
 *	This file implements functions that decode & handle keyboard events on
 *	MacOS X.
 *
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright (c) 2012 Adrian Robert.
 * Copyright 2015-2020 Marc Culler.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXInt.h"
#include "tkMacOSXConstants.h"
#include "tkMacOSXWm.h"

/*
 * See tkMacOSXPrivate.h for macros related to key event processing.
 */

/*
#ifdef TK_MAC_DEBUG
#define TK_MAC_DEBUG_KEYBOARD
#endif
*/

#define NS_KEYLOG 0
#define XEVENT_MOD_MASK (ControlMask | Mod1Mask | Mod3Mask | Mod4Mask)
static Tk_Window keyboardGrabWinPtr = NULL; /* Current keyboard grab window. */
static NSWindow *keyboardGrabNSWindow = nil; /* Its underlying NSWindow.*/
static NSModalSession modalSession = nil;
static BOOL processingCompose = NO;
static Tk_Window composeWin = NULL;
static int caret_x = 0, caret_y = 0, caret_height = 0;
static void setupXEvent(XEvent *xEvent, Tk_Window tkwin, NSUInteger modifiers);
static void setXEventPoint(XEvent *xEvent, Tk_Window tkwin, NSWindow *w);
static NSUInteger textInputModifiers;

#pragma mark TKApplication(TKKeyEvent)

@implementation TKApplication(TKKeyEvent)

- (NSEvent *) tkProcessKeyEvent: (NSEvent *) theEvent
{
#ifdef TK_MAC_DEBUG_EVENTS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, theEvent);
#endif
    NSWindow *w = [theEvent window];
    TkWindow *winPtr = TkMacOSXGetTkWindow(w), *grabWinPtr, *focusWinPtr;
    Tk_Window tkwin = (Tk_Window)winPtr;
    NSEventType type = [theEvent type];
    NSUInteger virt = [theEvent keyCode];
    NSUInteger modifiers = ([theEvent modifierFlags] &
			    NSDeviceIndependentModifierFlagsMask);
    XEvent xEvent;
    MacKeycode macKC;
    UniChar keychar = 0;
    Bool can_input_text, has_modifiers = NO, use_text_input = NO;
    static NSUInteger savedModifiers = 0;
    static NSMutableArray *nsEvArray = nil;

    if (nsEvArray == nil) {
        nsEvArray = [[NSMutableArray alloc] initWithCapacity: 1];
        processingCompose = NO;
    }
    if (!winPtr) {
	return theEvent;
    }

    /*
     * Discard repeating KeyDown events if the repeat speed has been set to
     * "off" in System Preferences.  It is unclear why we get these, but we do.
     * See ticket [2ecb09d118].
     */

    if ([theEvent type] ==  NSKeyDown &&
	[theEvent isARepeat] &&
	[NSEvent keyRepeatDelay] < 0) {
            return theEvent;
	}

    /*
     * If a local grab is in effect, key events for windows in the
     * grabber's application are redirected to the grabber.  Key events
     * for other applications are delivered normally.  If a global
     * grab is in effect all key events are redirected to the grabber.
     */

    grabWinPtr = winPtr->dispPtr->grabWinPtr;
    if (grabWinPtr) {
	if (winPtr->dispPtr->grabFlags ||  /* global grab */
	    grabWinPtr->mainPtr == winPtr->mainPtr){ /* same application */
	    winPtr = winPtr->dispPtr->focusPtr;
	    if (!winPtr) {
		return theEvent;
	    }
	    tkwin = (Tk_Window)winPtr;
	}
    }

    /*
     * Extract the unicode character from KeyUp and KeyDown events.
     */

    if (type == NSKeyUp || type == NSKeyDown) {
	NSString *characters = [theEvent characters];
	if (characters.length > 0) {
	    keychar = [characters characterAtIndex:0];

	    /*
	     * Currently, real keys always send BMP characters, but who knows?
	     */

	    if (CFStringIsSurrogateHighCharacter(keychar)) {
		UniChar lowChar = [characters characterAtIndex:1];
		keychar = CFStringGetLongCharacterForSurrogatePair(
		    keychar, lowChar);
	    }
	} else {

	    /*
	     * This is a dead key, such as Option-e, so it usually should get
	     * passed to the TextInputClient.  But if it has a Command modifier
	     * then it is not functioning as a dead key and should not be
	     * handled by the TextInputClient.  See ticket [1626ed65b8] and the
	     * method performKeyEquivalent which is implemented in
	     * tkMacOSXMenu.c.
	     */

	    if (!(modifiers & NSCommandKeyMask)) {
		use_text_input = YES;
	    }
	}

	/*
	 * Apple uses 0x10 for unrecognized keys.
	 */

	if (keychar == 0x10) {
	    keychar = UNKNOWN_KEYCHAR;
	}

#if defined(TK_MAC_DEBUG_EVENTS) || NS_KEYLOG == 1
	TKLog(@"-[%@(%p) %s] repeat=%d mods=%x char=%x code=%lu c=%d type=%d",
	      [self class], self, _cmd,
	      (type == NSKeyDown) && [theEvent isARepeat], modifiers, keychar,
	      virt, w, type);
#endif

    }

    /*
     * Build a skeleton XEvent.  We need to build it here, even if we will not
     * send it, so we can pass it to TkFocusKeyEvent to determine whether the
     * target widget can input text.
     */

    setupXEvent(&xEvent, tkwin, modifiers);
    has_modifiers = xEvent.xkey.state & XEVENT_MOD_MASK;
    focusWinPtr = TkFocusKeyEvent(winPtr, &xEvent);
    if (focusWinPtr == NULL) {
	TKContentView *contentView = [w contentView];

	/*
	 * This NSEvent is being sent to a window which does not have focus.
	 * This could mean, for example, that the user deactivated the Tk app
	 * while the NSTextInputClient's popup character selection window was
	 * still open.  We attempt to abandon any ongoing composition operation
	 * and discard the event.
	 */

	[contentView cancelComposingText];
	return theEvent;
    }
    can_input_text = ((focusWinPtr->flags & TK_CAN_INPUT_TEXT) != 0);

#if (NS_KEYLOG)
    TKLog(@"keyDown: %s compose sequence.\n",
	  processingCompose == YES ? "Continue" : "Begin");
#endif

    /*
     * Decide whether this event should be processed with the NSTextInputClient
     * protocol.
     */

    if (processingCompose ||
	(type == NSKeyDown && can_input_text && !has_modifiers &&
	 IS_PRINTABLE(keychar))
	) {
	use_text_input = YES;
    }

    /*
     * If we are processing this KeyDown event as an NSTextInputClient we do
     * not queue an XEvent.  We pass the NSEvent to our interpretKeyEvents
     * method.  When the composition sequence is complete, the callback method
     * insertText: replacementRange will be called.  That method generates a
     * keyPress XEvent with the selected character.
     */

    if (use_text_input) {
	textInputModifiers = modifiers;

	/*
	 * In IME the Enter key is used to terminate a composition sequence.
	 * When there are multiple choices of input text available, and the
	 * user's selected choice is not the default, it may be necessary to
	 * hit the Enter key multiple times before the text is accepted and
	 * rendered (See ticket 39de9677aa]). So when sending an Enter key
	 * during composition, we continue sending Enter keys until the
	 * inputText method has cleared the processingCompose flag.
	 */

	if (processingCompose && [theEvent keyCode] == 36) {
	    [nsEvArray addObject: theEvent];
	    while(processingCompose) {
		[[w contentView] interpretKeyEvents: nsEvArray];
	    }
	    [nsEvArray removeObject: theEvent];
	} else {
	    [nsEvArray addObject: theEvent];
	    [[w contentView] interpretKeyEvents: nsEvArray];
	    [nsEvArray removeObject: theEvent];
	}
	return theEvent;
    }

    /*
     * We are not handling this event as an NSTextInputClient, so we need to
     * finish constructing the XEvent and queue it.
     */

    macKC.v.o_s =  ((modifiers & NSShiftKeyMask ? INDEX_SHIFT : 0) |
		    (modifiers & NSAlternateKeyMask ? INDEX_OPTION : 0));
    macKC.v.virt = virt;
    switch (type) {
    case NSFlagsChanged:

	/*
	 * This XEvent is a simulated KeyPress or KeyRelease event for a
	 * modifier key.  To determine the type, note that the highest bit
	 * where the flags differ is 1 if and only if it is a KeyPress. The
	 * modifiers are saved so we can detect the next flag change.
	 */

	xEvent.xany.type = modifiers > savedModifiers ? KeyPress : KeyRelease;
	savedModifiers = modifiers;

	/*
	 * Set the keychar to MOD_KEYCHAR as a signal to TkpGetKeySym (see
	 * tkMacOSXKeyboard.c) that this is a modifier key event.
	 */

	keychar = MOD_KEYCHAR;
	break;
    case NSKeyUp:
	xEvent.xany.type = KeyRelease;
	break;
    case NSKeyDown:
	xEvent.xany.type = KeyPress;
	break;
    default:
	return theEvent; /* Unrecognized key event. */
    }
    macKC.v.keychar = keychar;
    xEvent.xkey.keycode = macKC.uint;
    setXEventPoint(&xEvent, tkwin, w);

    /*
     * Finally we can queue the XEvent, inserting a KeyRelease before a
     * repeated KeyPress.
     */

    if (type == NSKeyDown && [theEvent isARepeat]) {
	xEvent.xany.type = KeyRelease;
	Tk_QueueWindowEvent(&xEvent, TCL_QUEUE_TAIL);
	xEvent.xany.type = KeyPress;
    }
    Tk_QueueWindowEvent(&xEvent, TCL_QUEUE_TAIL);
    return theEvent;
}
@end


@implementation TKContentView
@synthesize tkDirtyRect = _tkDirtyRect;
@synthesize tkNeedsDisplay = _tkNeedsDisplay;

/*
 * Implementation of the NSTextInputClient protocol.
 */

/* [NSTextInputClient inputText: replacementRange:] is called by
 * interpretKeyEvents when a composition sequence is complete.  It is also
 * called when we delete working text.  In that case the call is followed
 * immediately by doCommandBySelector: deleteBackward:
 */
- (void)insertText: (id)aString
  replacementRange: (NSRange)repRange
{
    int i, len, state;
    XEvent xEvent;
    NSString *str, *keystr, *lower;
    TkWindow *winPtr = TkMacOSXGetTkWindow([self window]);
    Tk_Window tkwin = (Tk_Window)winPtr;
    Bool sendingIMEText = NO;

    str = ([aString isKindOfClass: [NSAttributedString class]]) ?
        [aString string] : aString;
    len = [str length];

    if (NS_KEYLOG) {
	TKLog(@"insertText '%@'\tlen = %d", aString, len);
    }

    /*
     * Clear any working text.
     */

    if (privateWorkingText != nil) {
	sendingIMEText = YES;
    	[self deleteWorkingText];
    }

    /*
     * Insert the string as a sequence of keystrokes.
     */

    setupXEvent(&xEvent, tkwin, textInputModifiers);
    setXEventPoint(&xEvent, tkwin, [self window]);
    xEvent.xany.type = KeyPress;

    /*
     * Apple evidently sets location to 0 to signal that an accented letter has
     * been selected from the accent menu.  An unaccented letter has already
     * been displayed and we need to erase it before displaying the accented
     * letter.
     */

    if (repRange.location == 0) {
	Tk_Window focusWin = (Tk_Window)winPtr->dispPtr->focusPtr;
	TkSendVirtualEvent(focusWin, "TkAccentBackspace", NULL);
    }

    /*
     * Next we generate an XEvent for each unicode character in our string.
     * This string could contain non-BMP characters, for example if the
     * emoji palette was used.
     *
     * NSString uses UTF-16 internally, which means that a non-BMP character is
     * represented by a sequence of two 16-bit "surrogates".  We record this in
     * the XEvent by setting the low order 21-bits of the keycode to the UCS-32
     * value value of the character and the virtual keycode in the high order
     * byte to the special value NON_BMP.
     */

    state = xEvent.xkey.state;
    for (i = 0; i < len; i++) {
	UniChar keychar;
	MacKeycode macKC = {0};

	keychar = [str characterAtIndex:i];
	macKC.v.keychar = keychar;
	if (CFStringIsSurrogateHighCharacter(keychar)) {
	    UniChar lowChar = [str characterAtIndex:++i];
	    macKC.v.keychar = CFStringGetLongCharacterForSurrogatePair(
				  (UniChar)keychar, lowChar);
	    macKC.v.virt = NON_BMP_VIRTUAL;
	} else if (repRange.location == 0 || sendingIMEText) {
	    macKC.v.virt = REPLACEMENT_VIRTUAL;
	} else {
	    macKC.uint = TkMacOSXAddVirtual(macKC.uint);
	    xEvent.xkey.state |= INDEX2STATE(macKC.x.xvirtual);
	}
	keystr = [[NSString alloc] initWithCharacters:&keychar length:1];
	lower = [keystr lowercaseString];
	if (![keystr isEqual: lower]) {
	    macKC.v.o_s |= INDEX_SHIFT;
	    xEvent.xkey.state |= ShiftMask;
	}
	if (xEvent.xkey.state & Mod2Mask) {
	    macKC.v.o_s |= INDEX_OPTION;
	}
	xEvent.xkey.keycode = macKC.uint;
    	xEvent.xany.type = KeyPress;
    	Tk_QueueWindowEvent(&xEvent, TCL_QUEUE_TAIL);
	xEvent.xkey.state = state;
    }
}

/*
 * This required method is allowed to return nil.
 */

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)theRange
      actualRange:(NSRangePointer)thePointer
{
    return nil;
}

/*
 * This method is supposed to insert (or replace selected text with) the string
 * argument. If the argument is an NSString, it should be displayed with a
 * distinguishing appearance, e.g underlined.
 */

- (void)setMarkedText: (id)aString
	selectedRange: (NSRange)selRange
     replacementRange: (NSRange)repRange
{
    TkWindow *winPtr = TkMacOSXGetTkWindow([self window]);
    Tk_Window focusWin = (Tk_Window)winPtr->dispPtr->focusPtr;
    NSString *temp;
    NSString *str;

    str = ([aString isKindOfClass: [NSAttributedString class]]) ?
        [aString string] : aString;
    if (focusWin) {

	/*
	 * Remember the widget where the composition is happening, in case it
	 * gets defocussed during the composition.
	 */

	composeWin = focusWin;
    } else {
	return;
    }
    if (NS_KEYLOG) {
	TKLog(@"setMarkedText '%@' len =%lu range %lu from %lu", str,
	      (unsigned long) [str length], (unsigned long) selRange.length,
	      (unsigned long) selRange.location);
    }

    if (privateWorkingText != nil) {
	[self deleteWorkingText];
    }

    if ([str length] == 0) {
	return;
    }

    /*
     * Use our insertText method to display the marked text.
     */

    TkSendVirtualEvent(focusWin, "TkStartIMEMarkedText", NULL);
    processingCompose = YES;
    temp = [str copy];
    [self insertText: temp replacementRange:repRange];
    privateWorkingText = temp;
    TkSendVirtualEvent(focusWin, "TkEndIMEMarkedText", NULL);
}

- (BOOL)hasMarkedText
{
    return privateWorkingText != nil;
}

- (NSRange)markedRange
{
    NSRange rng = privateWorkingText != nil
	? NSMakeRange(0, [privateWorkingText length])
	: NSMakeRange(NSNotFound, 0);

    if (NS_KEYLOG) {
	TKLog(@"markedRange request");
    }
    return rng;
}

- (void)unmarkText
{
    if (NS_KEYLOG) {
	TKLog(@"unmarkText");
    }
    [self deleteWorkingText];
    processingCompose = NO;
}

/*
 * Called by the system to get a position for popup character selection windows
 * such as a Character Palette, or a selection menu for IME.
 */

- (NSRect)firstRectForCharacterRange: (NSRange)theRange
			 actualRange: (NSRangePointer)thePointer
{
    NSRect rect;
    NSPoint pt;
    pt.x = caret_x;
    pt.y = caret_y;

    pt = [self convertPoint: pt toView: nil];
    pt = [[self window] tkConvertPointToScreen: pt];
    pt.y -= caret_height;

    rect.origin = pt;
    rect.size.width = 0;
    rect.size.height = caret_height;
    return rect;
}

- (NSInteger)conversationIdentifier
{
    return (NSInteger) self;
}

- (void)doCommandBySelector: (SEL)aSelector
{
    if (NS_KEYLOG) {
	TKLog(@"doCommandBySelector: %@", NSStringFromSelector(aSelector));
    }
    processingCompose = NO;
    if (aSelector == @selector (deleteBackward:)) {
	TkWindow *winPtr = TkMacOSXGetTkWindow([self window]);
	Tk_Window focusWin = (Tk_Window)winPtr->dispPtr->focusPtr;
	TkSendVirtualEvent(focusWin, "TkAccentBackspace", NULL);
    }
}

- (NSArray *)validAttributesForMarkedText
{
    static NSArray *arr = nil;
    if (arr == nil) {
	arr = [[NSArray alloc] initWithObjects:
	    NSUnderlineStyleAttributeName,
	    NSUnderlineColorAttributeName,
	    nil];
	[arr retain];
    }
    return arr;
}

- (NSRange)selectedRange
{
    if (NS_KEYLOG) {
	TKLog(@"selectedRange request");
    }
    return NSMakeRange(0, 0);
}

- (NSUInteger)characterIndexForPoint: (NSPoint)thePoint
{
    if (NS_KEYLOG) {
	TKLog(@"characterIndexForPoint request");
    }
    return NSNotFound;
}

- (NSAttributedString *)attributedSubstringFromRange: (NSRange)theRange
{
    static NSAttributedString *str = nil;
    if (str == nil) {
	str = [NSAttributedString new];
    }
    if (NS_KEYLOG) {
	TKLog(@"attributedSubstringFromRange request");
    }
    return str;
}
/* End of NSTextInputClient implementation. */

@end


@implementation TKContentView(TKKeyEvent)

/*
 * Tell the widget to erase the displayed composing characters.  This
 * is not part of the NSTextInputClient protocol.
 */

- (void)deleteWorkingText
{
    if (privateWorkingText == nil) {
	return;
    } else {

	if (NS_KEYLOG) {
	    TKLog(@"deleteWorkingText len = %lu\n",
		  (unsigned long)[privateWorkingText length]);
	}

	[privateWorkingText release];
	privateWorkingText = nil;
	processingCompose = NO;
	if (composeWin) {
	    TkSendVirtualEvent(composeWin, "TkClearIMEMarkedText", NULL);
	}
    }
}

- (void)cancelComposingText
{
    if (NS_KEYLOG) {
	TKLog(@"cancelComposingText");
    }
    [self deleteWorkingText];
    processingCompose = NO;
}

@end

/*
 * Set up basic fields in xevent for keyboard input.
 */

static void
setupXEvent(XEvent *xEvent, Tk_Window tkwin, NSUInteger modifiers)
{
    unsigned int state = 0;
    Display *display;

    if (tkwin == NULL) {
	return;
    }
    display = Tk_Display(tkwin);
    if (modifiers) {
	state = (modifiers & NSAlphaShiftKeyMask ? LockMask    : 0) |
	        (modifiers & NSShiftKeyMask      ? ShiftMask   : 0) |
	        (modifiers & NSControlKeyMask    ? ControlMask : 0) |
	        (modifiers & NSCommandKeyMask    ? Mod1Mask    : 0) |
	        (modifiers & NSAlternateKeyMask  ? Mod2Mask    : 0) |
	        (modifiers & NSNumericPadKeyMask ? Mod3Mask    : 0) |
	        (modifiers & NSFunctionKeyMask   ? Mod4Mask    : 0) ;
    }
    memset(xEvent, 0, sizeof(XEvent));
    xEvent->xany.serial = LastKnownRequestProcessed(display);
    xEvent->xany.display = Tk_Display(tkwin);
    xEvent->xany.window = Tk_WindowId(tkwin);

    xEvent->xkey.root = XRootWindow(display, 0);
    xEvent->xkey.time = TkpGetMS();
    xEvent->xkey.state = state;
    xEvent->xkey.same_screen = true;
    /* No need to initialize other fields implicitly here,
     * because of the memset() above. */
}

static void
setXEventPoint(
    XEvent *xEvent,
    Tk_Window tkwin,
    NSWindow *w)
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    NSPoint local = [w  mouseLocationOutsideOfEventStream];
    NSPoint global = [w tkConvertPointToScreen: local];
    int win_x, win_y;

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
    } else if (winPtr->wmInfoPtr != NULL) {
	local.x -= winPtr->wmInfoPtr->xInParent;
	local.y -= winPtr->wmInfoPtr->yInParent;
    }
    tkwin = Tk_TopCoordsToWindow(tkwin, local.x, local.y, &win_x, &win_y);
    local.x = win_x;
    local.y = win_y;
    global.y = TkMacOSXZeroScreenHeight() - global.y;
    xEvent->xbutton.x = local.x;
    xEvent->xbutton.y = local.y;
    xEvent->xbutton.x_root = global.x;
    xEvent->xbutton.y_root = global.y;
}

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
    TkWindow *captureWinPtr = (TkWindow *) TkpGetCapture();

    if (keyboardGrabWinPtr && captureWinPtr) {
	NSWindow *w = TkMacOSXGetNSWindowForDrawable(grab_window);
	MacDrawable *macWin = (MacDrawable *)grab_window;

	if (w && macWin->toplevel->winPtr == (TkWindow *) captureWinPtr) {
	    if (modalSession ) {
		if (keyboardGrabNSWindow == w) {
		    return GrabSuccess;
		} else {
		    Tcl_Panic("XGrabKeyboard: already grabbed");
		}
	    }
	    keyboardGrabNSWindow = w;
	    [w retain];
	    modalSession = [NSApp beginModalSessionForWindow:w];
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

int
XUngrabKeyboard(
    Display* display,
    Time time)
{
    if (modalSession) {
	[NSApp endModalSession:modalSession];
	modalSession = nil;
    }
    if (keyboardGrabNSWindow) {
	[keyboardGrabNSWindow release];
	keyboardGrabNSWindow = nil;
    }
    keyboardGrabWinPtr = NULL;
    return Success;
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
 * Tk_SetCaretPos --
 *
 *	This enables correct placement of the popups used for character
 *      selection by the NSTextInputClient.  It gets called by text entry
 *      widgets whenever the cursor is drawn.  It does nothing if the widget's
 *      NSWindow is not the current KeyWindow.  Otherwise it updates the
 *      display's caret structure and records the caret geometry in static
 *      variables for use by the NSTextInputClient implementation.  Any
 *      widget passed to this function will be marked as being able to input
 *      text by setting the TK_CAN_INPUT_TEXT flag.
 *
 * Results:
 *	None
 *
 * Side effects:
 *      Sets the CAN_INPUT_TEXT flag on the widget passed as tkwin.  May update
 *      the display's caret structure as well as the static variables caret_x,
 *      caret_y and caret_height.
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
    TkWindow *winPtr = (TkWindow *) tkwin;
    TkCaret *caretPtr = &(winPtr->dispPtr->caret);
    NSWindow *w = TkMacOSXGetNSWindowForDrawable(Tk_WindowId(tkwin));

    /*
     * Register this widget as being capable of text input, so we know we
     * should process (appropriate) key events for this window with the
     * NSTextInputClient protocol.
     */

    winPtr->flags |= TK_CAN_INPUT_TEXT;
    if (w && ![w isKeyWindow]) {
	return;
    }
    if ((caretPtr->winPtr == winPtr
	 && caretPtr->x == x) && (caretPtr->y == y)) {
	return;
    }

    /*
     * Update the display's caret information.
     */

    caretPtr->winPtr = winPtr;
    caretPtr->x = x;
    caretPtr->y = y;
    caretPtr->height = height;

    /*
     * Record the caret geometry in static variables for use when processing
     * key events.  We use the TKContextView coordinate system for this.
     */

    caret_height = height;
    while (!Tk_IsTopLevel(tkwin)) {
	x += Tk_X(tkwin);
	y += Tk_Y(tkwin);
	tkwin = Tk_Parent(tkwin);
	if (tkwin == NULL) {
	    return;
	}
    }
    caret_x = x;
    caret_y = Tk_Height(tkwin) - y;
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
