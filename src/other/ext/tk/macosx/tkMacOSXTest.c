/*
 * tkMacOSXTest.c --
 *
 *	Contains commands for platform specific tests for
 *	the Macintosh platform.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2005-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXConstants.h"
#include "tkMacOSXWm.h"


/*
 * Forward declarations of procedures defined later in this file:
 */

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1080
static int		DebuggerObjCmd (ClientData dummy, Tcl_Interp *interp,
					int objc, Tcl_Obj *const objv[]);
#endif
static int		PressButtonObjCmd (ClientData dummy, Tcl_Interp *interp,
					int objc, Tcl_Obj *const *objv);
static int		InjectKeyEventObjCmd (ClientData dummy, Tcl_Interp *interp,
					int objc, Tcl_Obj *const *objv);
static int		MenuBarHeightObjCmd (ClientData dummy, Tcl_Interp *interp,
					int objc, Tcl_Obj *const *objv);


/*
 *----------------------------------------------------------------------
 *
 * TkplatformtestInit --
 *
 *	Defines commands that test platform specific functionality for
 *	Unix platforms.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Defines new commands.
 *
 *----------------------------------------------------------------------
 */

int
TkplatformtestInit(
    Tcl_Interp *interp)		/* Interpreter to add commands to. */
{
    /*
     * Add commands for platform specific tests on MacOS here.
     */

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1080
    Tcl_CreateObjCommand(interp, "debugger", DebuggerObjCmd, NULL, NULL);
#endif
    Tcl_CreateObjCommand(interp, "pressbutton", PressButtonObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "injectkeyevent", InjectKeyEventObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "menubarheight", MenuBarHeightObjCmd, NULL, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DebuggerObjCmd --
 *
 *	This procedure simply calls the low level debugger, which was
 *      deprecated in OSX 10.8.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1080
static int
DebuggerObjCmd(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Not used. */
    int objc,				/* Not used. */
    Tcl_Obj *const objv[])			/* Not used. */
{
    Debugger();
    return TCL_OK;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * MenuBarHeightObjCmd --
 *
 *	This procedure calls [NSMenu menuBarHeight] and returns the result
 *      as an integer.  Windows can never be placed to overlap the MenuBar,
 *      so tests need to be aware of its size.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
MenuBarHeightObjCmd(
    TCL_UNUSED(void *),		/* Not used. */
    Tcl_Interp *interp,			/* Not used. */
    TCL_UNUSED(int),				/* Not used. */
    TCL_UNUSED(Tcl_Obj *const *))		/* Not used. */
{
    static int height = 0;
    if (height == 0) {
	height = (int) [[NSApp mainMenu] menuBarHeight];
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(height));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkTestLogDisplay --
 *
 *      The test image display procedure calls this to determine whether it
 *      should write a log message recording that it has being run.
 *
 * Results:
 *      Returns true if and only if the NSView of the drawable is the
 *      current focusView, which on 10.14 and newer systems can only be the
 *      case when within [NSView drawRect].
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */
MODULE_SCOPE Bool
TkTestLogDisplay(
    Drawable drawable)
{
    MacDrawable *macWin = (MacDrawable *)drawable;
    NSWindow *win = nil;
    if (macWin->toplevel && macWin->toplevel->winPtr &&
	macWin->toplevel->winPtr->wmInfoPtr &&
	macWin->toplevel->winPtr->wmInfoPtr->window) {
	win = macWin->toplevel->winPtr->wmInfoPtr->window;
    } else if (macWin->winPtr && macWin->winPtr->wmInfoPtr &&
	       macWin->winPtr->wmInfoPtr->window) {
	win = macWin->winPtr->wmInfoPtr->window;
    }
    if (win) {
	return ([win contentView] == [NSView focusView]);
    } else {
	return True;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PressButtonObjCmd --
 *
 *	This Tcl command simulates a button press at a specific screen
 *      location.  It injects NSEvents into the NSApplication event queue, as
 *      opposed to adding events to the Tcl queue as event generate would do.
 *      One application is for testing the grab command. These events have
 *      their unused context property set to 1 as a signal indicating that they
 *      should not be ignored by [NSApp tkProcessMouseEvent].
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
PressButtonObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int x = 0, y = 0, i, value;
    NSInteger signal = -1;
    CGPoint pt;
    NSPoint loc;
    NSEvent *motion, *press, *release;
    NSArray *screens = [NSScreen screens];
    CGFloat ScreenHeight = 0;
    enum {X=1, Y};

    if (screens && [screens count]) {
	ScreenHeight = [[screens objectAtIndex:0] frame].size.height;
    }

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "x y");
        return TCL_ERROR;
    }
    for (i = 1; i < objc; i++) {
	if (Tcl_GetIntFromObj(interp,objv[i],&value) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (i) {
	case X:
	    x = value;
	    break;
	case Y:
	    y = value;
	    break;
	default:
	    break;
	}
    }
    pt.x = loc.x = x;
    pt.y = y;
    loc.y = ScreenHeight - y;

    /*
     *  We set the window number and the eventNumber to -1 as a signal to
     *  processMouseEvent.
     */

    CGWarpMouseCursorPosition(pt);
    motion = [NSEvent mouseEventWithType:NSMouseMoved
	location:loc
	modifierFlags:0
	timestamp:GetCurrentEventTime()
	windowNumber:signal
	context:nil
	eventNumber:signal
	clickCount:1
	pressure:0.0];
    [NSApp postEvent:motion atStart:NO];
    press = [NSEvent mouseEventWithType:NSLeftMouseDown
	location:loc
	modifierFlags:0
	timestamp:GetCurrentEventTime()
	windowNumber:signal
	context:nil
	eventNumber:signal
	clickCount:1
	pressure:0.0];
    [NSApp postEvent:press atStart:NO];
    release = [NSEvent mouseEventWithType:NSLeftMouseUp
	location:loc
	modifierFlags:0
	timestamp:GetCurrentEventTime()
	windowNumber:signal
	context:nil
	eventNumber:signal
	clickCount:1
	pressure:-1.0];
    [NSApp postEvent:release atStart:NO];
    return TCL_OK;
}

static int
InjectKeyEventObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    static const char *const optionStrings[] = {
	"press", "release", "flagschanged", NULL};
    NSUInteger types[3] = {NSKeyDown, NSKeyUp, NSFlagsChanged};
    static const char *const argStrings[] = {
	"-shift", "-control", "-option", "-command", "-function", "-x", "-y", NULL};
    enum args {KEYEVENT_SHIFT, KEYEVENT_CONTROL, KEYEVENT_OPTION, KEYEVENT_COMMAND,
	       KEYEVENT_FUNCTION, KEYEVENT_X, KEYEVENT_Y};
    int i, index, keysym, mods = 0, x = 0, y = 0;
    NSString *chars = nil, *unmod = nil, *upper, *lower;
    NSEvent *keyEvent;
    NSUInteger type;
    MacKeycode macKC;

    if (objc < 3) {
    wrongArgs:
        Tcl_WrongNumArgs(interp, 1, objv, "option keysym ?arg?");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], optionStrings, "option", 0,
            &index) != TCL_OK) {
        return TCL_ERROR;
    }
    type = types[index];
    if (Tcl_GetIntFromObj(interp, objv[2], &keysym) != TCL_OK) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			 "keysym must be an integer"));
	Tcl_SetErrorCode(interp, "TK", "TEST", "INJECT", "KEYSYM", NULL);
	return TCL_ERROR;
    }
    macKC.uint = XKeysymToKeycode(NULL, keysym);
    for (i = 3; i < objc; i++) {
	if (Tcl_GetIndexFromObjStruct(interp, objv[i], argStrings,
                sizeof(char *), "option", TCL_EXACT, &index) != TCL_OK) {
            return TCL_ERROR;
        }
        switch ((enum args) index) {
	case KEYEVENT_SHIFT:
	    mods |= NSShiftKeyMask;
            break;
	case KEYEVENT_CONTROL:
	    mods |= NSControlKeyMask;
            break;
	case KEYEVENT_OPTION:
	    mods |= NSAlternateKeyMask;
            break;
	case KEYEVENT_COMMAND:
	    mods |= NSCommandKeyMask;
            break;
	case KEYEVENT_FUNCTION:
	    mods |= NSFunctionKeyMask;
            break;
	case KEYEVENT_X:
	    if (++i >= objc) {
                goto wrongArgs;
            }
	    if (Tcl_GetIntFromObj(interp,objv[i], &x) != TCL_OK) {
		return TCL_ERROR;
	    }
	    break;
	case KEYEVENT_Y:
	    if (++i >= objc) {
                goto wrongArgs;
            }
	    if (Tcl_GetIntFromObj(interp,objv[i], &y) != TCL_OK) {
		return TCL_ERROR;
	    }
	    break;
	}
    }
    if (type != NSFlagsChanged) {
	UniChar keychar = macKC.v.keychar;
	chars = [[NSString alloc] initWithCharacters: &keychar length:1];
	upper = [chars uppercaseString];
	lower = [chars lowercaseString];
	if (![upper isEqual: lower] && [chars isEqual: upper]) {
	    mods |= NSShiftKeyMask;
	}
	if (mods & NSShiftKeyMask) {
	    chars = upper;
	    unmod = lower;
	    macKC.v.o_s |= INDEX_SHIFT;
	} else {
	    unmod = chars;
	}
	if (macKC.v.o_s & INDEX_OPTION) {
	    mods |= NSAlternateKeyMask;
	}
    }
    keyEvent = [NSEvent keyEventWithType:type
	location:NSMakePoint(x, y)
        modifierFlags:mods
	timestamp:GetCurrentEventTime()
	windowNumber:0
	context:nil
	characters:chars
	charactersIgnoringModifiers:unmod
	isARepeat:NO
	keyCode:macKC.v.virt];
    [NSApp postEvent:keyEvent atStart:NO];
    return TCL_OK;
}
/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
