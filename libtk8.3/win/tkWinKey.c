/*
 * tkWinKey.c --
 *
 *	This file contains X emulation routines for keyboard related
 *	functions.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkWinInt.h"
/*
 * The keymap table holds mappings of Windows keycodes to X keysyms.
 * If Windows ever comes along and changes the value of their keycodes,
 * this will break all kinds of things.  However, this table lookup is much
 * faster than the alternative, in which we walked a list of keycodes looking
 * for a match.  Since this lookup is performed for every Windows keypress
 * event, it seems like a worthwhile improvement to use the table.
 */
#define MAX_KEYCODE 145 /* VK_SCROLL is the last entry in our table below */
static KeySym keymap[] = {
    NoSymbol, NoSymbol, NoSymbol, XK_Cancel, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, XK_BackSpace, XK_Tab,
	NoSymbol, NoSymbol, XK_Clear, XK_Return, NoSymbol,
	NoSymbol, XK_Shift_L, XK_Control_L, XK_Alt_L, XK_Pause,
	XK_Caps_Lock, NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, XK_Escape, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, XK_space, XK_Prior, XK_Next,
	XK_End, XK_Home, XK_Left, XK_Up, XK_Right,
	XK_Down, XK_Select, XK_Print, XK_Execute, NoSymbol,
	XK_Insert, XK_Delete, XK_Help, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, XK_Win_L, XK_Win_R, XK_App, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, XK_F1, XK_F2, XK_F3,
	XK_F4, XK_F5, XK_F6, XK_F7, XK_F8,
	XK_F9, XK_F10, XK_F11, XK_F12, XK_F13,
	XK_F14, XK_F15, XK_F16, XK_F17, XK_F18,
	XK_F19,	XK_F20, XK_F21, XK_F22, XK_F23,
	XK_F24,	NoSymbol, NoSymbol, NoSymbol, NoSymbol,
	NoSymbol, NoSymbol, NoSymbol, NoSymbol, XK_Num_Lock,
	XK_Scroll_Lock
};

/*
 * Prototypes for local procedures defined in this file:
 */

static KeySym		KeycodeToKeysym _ANSI_ARGS_((unsigned int keycode,
			    int state, int noascii));

/*
 *----------------------------------------------------------------------
 *
 * TkpGetString --
 *
 *	Retrieve the UTF string equivalent for the given keyboard event.
 *
 * Results:
 *	Returns the UTF string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TkpGetString(winPtr, eventPtr, dsPtr)
    TkWindow *winPtr;		/* Window where event occurred:  needed to
				 * get input context. */
    XEvent *eventPtr;		/* X keyboard event. */
    Tcl_DString *dsPtr;		/* Uninitialized or empty string to hold
				 * result. */
{
    KeySym keysym;
    XKeyEvent* keyEv = &eventPtr->xkey;

    Tcl_DStringInit(dsPtr);
    if (eventPtr->xkey.send_event != -1) {
	/*
	 * This is an event generated from generic code.  It has no
	 * nchars or trans_chars members. 
	 */

	keysym = KeycodeToKeysym(eventPtr->xkey.keycode,
		eventPtr->xkey.state, 0);
	if (((keysym != NoSymbol) && (keysym > 0) && (keysym < 256)) 
		|| (keysym == XK_Return)
		|| (keysym == XK_Tab)) {
	    char buf[TCL_UTF_MAX];
	    int len = Tcl_UniCharToUtf((Tcl_UniChar) (keysym & 255), buf);
	    Tcl_DStringAppend(dsPtr, buf, len);
	}
    } else if (eventPtr->xkey.nbytes > 0) {
	Tcl_ExternalToUtfDString(NULL, eventPtr->xkey.trans_chars,
		eventPtr->xkey.nbytes, dsPtr);
    }
    return Tcl_DStringValue(dsPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * XKeycodeToKeysym --
 *
 *	Translate from a system-dependent keycode to a
 *	system-independent keysym.
 *
 * Results:
 *	Returns the translated keysym, or NoSymbol on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

KeySym
XKeycodeToKeysym(display, keycode, index)
    Display* display;
    unsigned int keycode;
    int index;
{
    int state = 0;

    if (index & 0x01) {
	state |= ShiftMask;
    }
    return KeycodeToKeysym(keycode, state, 0);
}



/*
 *----------------------------------------------------------------------
 *
 * KeycodeToKeysym --
 *
 *	Translate from a system-dependent keycode to a
 *	system-independent keysym.
 *
 * Results:
 *	Returns the translated keysym, or NoSymbol on failure.
 *
 * Side effects:
 *	It may affect the internal state of the keyboard, such as
 *      remembered dead key or lock indicator lamps.
 *
 *----------------------------------------------------------------------
 */

static KeySym
KeycodeToKeysym(keycode, state, noascii)
    unsigned int keycode;
    int state;
    int noascii;
{
    BYTE keys[256];
    int result, deadkey, shift;
    char buf[4];
    unsigned int scancode = MapVirtualKey(keycode, 0);

    /*
     * Do not run keycodes of lock keys through ToAscii().
     * One of ToAscii()'s side effects is to handle the lights
     * on the keyboard, and we don't want to mess that up.
     */

    if (noascii || keycode == VK_CAPITAL || keycode == VK_SCROLL ||
	    keycode == VK_NUMLOCK)
        goto skipToAscii;

    /*
     * Use MapVirtualKey() to detect some dead keys.
     */

    if (MapVirtualKey(keycode, 2) > 0x7fffUL)
        return XK_Multi_key;

    /*
     * Set up a keyboard with correct modifiers
     */

    memset(keys, 0, 256);
    if (state & ShiftMask)
        keys[VK_SHIFT] = 0x80;
    if (state & ControlMask)
	keys[VK_CONTROL] = 0x80;
    if (state & Mod2Mask)
	keys[VK_MENU] = 0x80;

    /* 
     * Make sure all lock button info is correct so we don't mess up the
     * lights
     */

    if (state & LockMask)
	keys[VK_CAPITAL] = 1;
    if (state & Mod3Mask)
	keys[VK_SCROLL] = 1;
    if (state & Mod1Mask)
	keys[VK_NUMLOCK] = 1;

    result = ToAscii(keycode, scancode, keys, (LPWORD) buf, 0);

    if (result < 0) {
        /*
         * Win95/98:
         * This was a dead char, which is now remembered by the keyboard.
         * Call ToAscii() again to forget it.
         * WinNT:
         * This was a dead char, overwriting any previously remembered
         * key. Calling ToAscii() again does not affect anything.
         */

        ToAscii(keycode, scancode, keys, (LPWORD) buf, 0);
        return XK_Multi_key;
    }
    if (result == 2) {
        /*
         * This was a dead char, and there were one previously remembered
         * by the keyboard.
         * Call ToAscii() again with proper parameters to restore it.
         */

        /* 
	 * Get information about the old char
	 */

        deadkey = VkKeyScan(buf[0]);
        shift = deadkey >> 8;
        deadkey &= 255;
        scancode = MapVirtualKey(deadkey, 0);

        /*
	 * Set up a keyboard with proper modifier keys
	 */

        memset(keys, 0, 256);
        if (shift & 1)
            keys[VK_SHIFT] = 0x80;
        if (shift & 2)
            keys[VK_CONTROL] = 0x80;
        if (shift & 4)
            keys[VK_MENU] = 0x80;
        ToAscii(deadkey, scancode, keys, (LPWORD) buf, 0);
        return XK_Multi_key;
    }

    /*
     * Keycode mapped to a valid Latin-1 character.  Since the keysyms
     * for alphanumeric characters map onto Latin-1, we just return it.
     *
     * We treat 0x7F as a special case mostly for backwards compatibility.
     * In versions of Tk<=8.2, Control-Backspace returned "XK_BackSpace"
     * as the X Keysym.  This was due to the fact that we did not
     * initialize the keys array properly when we passed it to ToAscii, above.
     * We had previously not been setting the state bit for the Control key.
     * When we fixed that, we found that Control-Backspace on Windows is
     * interpreted as ASCII-127 (0x7F), which corresponds to the Delete key.
     *
     * Upon discovering this, we realized we had two choices:  return XK_Delete
     * or return XK_BackSpace.  If we returned XK_Delete, that could be
     * considered "more correct" (although the correctness would be dependant
     * on whether you believe that ToAscii is doing the right thing in that
     * case); however, this would break backwards compatibility, and worse,
     * it would limit application programmers -- they would effectively be
     * unable to bind to <Control-Backspace> on Windows.  We therefore chose
     * instead to return XK_BackSpace (handled here by letting the code
     * "fall-through" to the return statement below, which works because the
     * keycode for this event is VK_BACKSPACE, and the keymap table maps that
     * keycode to XK_BackSpace).
     */

    if (result == 1 && UCHAR(buf[0]) >= 0x20 && UCHAR(buf[0]) != 0x7F) {
	return (KeySym) UCHAR(buf[0]);
    }

    /*
     * Keycode is a non-alphanumeric key, so we have to do the lookup.
     */

    skipToAscii:
    if (keycode < 0 || keycode > MAX_KEYCODE) {
	return NoSymbol;
    }
    switch (keycode) {
	/*
	 * Windows only gives us an undifferentiated VK_CONTROL
	 * code (for example) when either Control key is pressed.
	 * To distinguish between left and right, we have to query the
	 * state of one of the two to determine which was actually
	 * pressed.  So if the keycode indicates Control, Shift, or Menu
	 * (the key that everybody else calls Alt), do this extra test.
	 * If the right-side key was pressed, return the appropriate
	 * keycode.  Otherwise, we fall through and rely on the
	 * keymap table to hold the correct keysym value.
	 */
	case VK_CONTROL: {
	    if (GetKeyState(VK_RCONTROL) & 0x80) {
		return XK_Control_R;
	    }
	    break;
	}
	case VK_SHIFT: {
	    if (GetKeyState(VK_RSHIFT) & 0x80) {
		return XK_Shift_R;
	    }
	    break;
	}
	case VK_MENU: {
	    if (GetKeyState(VK_RMENU) & 0x80) {
		return XK_Alt_R;
	    }
	    break;
	}
    }
    return keymap[keycode];
}


/*
 *----------------------------------------------------------------------
 *
 * TkpGetKeySym --
 *
 *	Given an X KeyPress or KeyRelease event, map the
 *	keycode in the event into a KeySym.
 *
 * Results:
 *	The return value is the KeySym corresponding to
 *	eventPtr, or NoSymbol if no matching Keysym could be
 *	found.
 *
 * Side effects:
 *	In the first call for a given display, keycode-to-
 *	KeySym maps get loaded.
 *
 *----------------------------------------------------------------------
 */

KeySym
TkpGetKeySym(dispPtr, eventPtr)
    TkDisplay *dispPtr;		/* Display in which to map keycode. */
    XEvent *eventPtr;		/* Description of X event. */
{
    KeySym sym;
    int state = eventPtr->xkey.state;

    /*
     * Refresh the mapping information if it's stale
     */

    if (dispPtr->bindInfoStale) {
	TkpInitKeymapInfo(dispPtr);
    }

    sym = KeycodeToKeysym(eventPtr->xkey.keycode, state, 0);

    /*
     * Special handling: if this is a ctrl-alt or shifted key, and there
     * is no keysym defined, try without the modifiers.
     */

    if ((sym == NoSymbol) && ((state & ControlMask) || (state & Mod2Mask))) {
        state &=  ~(ControlMask | Mod2Mask);
        sym = KeycodeToKeysym(eventPtr->xkey.keycode, state, 0);
    }
    if ((sym == NoSymbol) && (state & ShiftMask)) {
        state &=  ~ShiftMask;
        sym = KeycodeToKeysym(eventPtr->xkey.keycode, state, 0);
    }
    return sym;
}

/*
 *--------------------------------------------------------------
 *
 * TkpInitKeymapInfo --
 *
 *	This procedure is invoked to scan keymap information
 *	to recompute stuff that's important for binding, such
 *	as the modifier key (if any) that corresponds to "mode
 *	switch".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Keymap-related information in dispPtr is updated.
 *
 *--------------------------------------------------------------
 */

void
TkpInitKeymapInfo(dispPtr)
    TkDisplay *dispPtr;		/* Display for which to recompute keymap
				 * information. */
{
    XModifierKeymap *modMapPtr;
    KeyCode *codePtr;
    KeySym keysym;
    int count, i, j, max, arraySize;
#define KEYCODE_ARRAY_SIZE 20

    dispPtr->bindInfoStale = 0;
    modMapPtr = XGetModifierMapping(dispPtr->display);

    /*
     * Check the keycodes associated with the Lock modifier.  If
     * any of them is associated with the XK_Shift_Lock modifier,
     * then Lock has to be interpreted as Shift Lock, not Caps Lock.
     */

    dispPtr->lockUsage = LU_IGNORE;
    codePtr = modMapPtr->modifiermap + modMapPtr->max_keypermod*LockMapIndex;
    for (count = modMapPtr->max_keypermod; count > 0; count--, codePtr++) {
	if (*codePtr == 0) {
	    continue;
	}
	keysym = KeycodeToKeysym(*codePtr, 0, 1);
	if (keysym == XK_Shift_Lock) {
	    dispPtr->lockUsage = LU_SHIFT;
	    break;
	}
	if (keysym == XK_Caps_Lock) {
	    dispPtr->lockUsage = LU_CAPS;
	    break;
	}
    }

    /*
     * Look through the keycodes associated with modifiers to see if
     * the the "mode switch", "meta", or "alt" keysyms are associated
     * with any modifiers.  If so, remember their modifier mask bits.
     */

    dispPtr->modeModMask = 0;
    dispPtr->metaModMask = 0;
    dispPtr->altModMask = 0;
    codePtr = modMapPtr->modifiermap;
    max = 8*modMapPtr->max_keypermod;
    for (i = 0; i < max; i++, codePtr++) {
	if (*codePtr == 0) {
	    continue;
	}
	keysym = KeycodeToKeysym(*codePtr, 0, 1);
	if (keysym == XK_Mode_switch) {
	    dispPtr->modeModMask |= ShiftMask << (i/modMapPtr->max_keypermod);
	}
	if ((keysym == XK_Meta_L) || (keysym == XK_Meta_R)) {
	    dispPtr->metaModMask |= ShiftMask << (i/modMapPtr->max_keypermod);
	}
	if ((keysym == XK_Alt_L) || (keysym == XK_Alt_R)) {
	    dispPtr->altModMask |= ShiftMask << (i/modMapPtr->max_keypermod);
	}
    }

    /*
     * Create an array of the keycodes for all modifier keys.
     */

    if (dispPtr->modKeyCodes != NULL) {
	ckfree((char *) dispPtr->modKeyCodes);
    }
    dispPtr->numModKeyCodes = 0;
    arraySize = KEYCODE_ARRAY_SIZE;
    dispPtr->modKeyCodes = (KeyCode *) ckalloc((unsigned)
	    (KEYCODE_ARRAY_SIZE * sizeof(KeyCode)));
    for (i = 0, codePtr = modMapPtr->modifiermap; i < max; i++, codePtr++) {
	if (*codePtr == 0) {
	    continue;
	}

	/*
	 * Make sure that the keycode isn't already in the array.
	 */

	for (j = 0; j < dispPtr->numModKeyCodes; j++) {
	    if (dispPtr->modKeyCodes[j] == *codePtr) {
		goto nextModCode;
	    }
	}
	if (dispPtr->numModKeyCodes >= arraySize) {
	    KeyCode *new;

	    /*
	     * Ran out of space in the array;  grow it.
	     */

	    arraySize *= 2;
	    new = (KeyCode *) ckalloc((unsigned)
		    (arraySize * sizeof(KeyCode)));
	    memcpy((VOID *) new, (VOID *) dispPtr->modKeyCodes,
		    (dispPtr->numModKeyCodes * sizeof(KeyCode)));
	    ckfree((char *) dispPtr->modKeyCodes);
	    dispPtr->modKeyCodes = new;
	}
	dispPtr->modKeyCodes[dispPtr->numModKeyCodes] = *codePtr;
	dispPtr->numModKeyCodes++;
	nextModCode: continue;
    }
    XFreeModifiermap(modMapPtr);
}

/*
 * When mapping from a keysym to a keycode, need
 * information about the modifier state that should be used
 * so that when they call XKeycodeToKeysym taking into
 * account the xkey.state, they will get back the original
 * keysym.
 */

void
TkpSetKeycodeAndState(tkwin, keySym, eventPtr)
    Tk_Window tkwin;
    KeySym keySym;
    XEvent *eventPtr;
{
    int i;
    SHORT result;
    int shift;
    
    eventPtr->xkey.keycode = 0;
    if (keySym == NoSymbol) {
        return;
    }

    /*
     * We check our private map first for a virtual keycode,
     * as VkKeyScan will return values that don't map to X
     * for the "extended" Syms.  This may be due to just casting
     * problems below, but this works.
     */
    for (i = 0; i <= MAX_KEYCODE; i++) {
	if (keymap[i] == keySym) {
            eventPtr->xkey.keycode = i;
            return;
	}
    }
    if (keySym >= 0x20) {
	result = VkKeyScan((char) keySym);
	if (result != -1) {
            shift = result >> 8;
            if (shift & 1)
                eventPtr->xkey.state |= ShiftMask;
            if (shift & 2)
                eventPtr->xkey.state |= ControlMask;
            if (shift & 4)
                eventPtr->xkey.state |= Mod2Mask;
            eventPtr->xkey.keycode = (KeyCode) (result & 0xff);
	}
    }
    {
        /* Debug log */
        FILE *fp = fopen("c:\\temp\\tklog.txt", "a");
        if (fp != NULL) {
            fprintf(fp, "TkpSetKeycode. Keycode %d State %d Keysym %d\n", eventPtr->xkey.keycode, eventPtr->xkey.state, keySym);
            fclose(fp);
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XKeysymToKeycode --
 *
 *	Translate a keysym back into a keycode.
 *
 * Results:
 *	Returns the keycode that would generate the specified keysym.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

KeyCode
XKeysymToKeycode(display, keysym)
    Display* display;
    KeySym keysym;
{
    int i;
    SHORT result;

    /*
     * We check our private map first for a virtual keycode,
     * as VkKeyScan will return values that don't map to X
     * for the "extended" Syms.  This may be due to just casting
     * problems below, but this works.
     */
    if (keysym == NoSymbol) {
	return 0;
    }
    for (i = 0; i <= MAX_KEYCODE; i++) {
	if (keymap[i] == keysym) {
	    return ((KeyCode) i);
	}
    }
    if (keysym >= 0x20) {
	result = VkKeyScan((char) keysym);
	if (result != -1) {
	    return (KeyCode) (result & 0xff);
	}
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * XGetModifierMapping --
 *
 *	Fetch the current keycodes used as modifiers.
 *
 * Results:
 *	Returns a new modifier map.
 *
 * Side effects:
 *	Allocates a new modifier map data structure.
 *
 *----------------------------------------------------------------------
 */

XModifierKeymap	*
XGetModifierMapping(display)
    Display* display;
{
    XModifierKeymap *map = (XModifierKeymap *)ckalloc(sizeof(XModifierKeymap));

    map->max_keypermod = 1;
    map->modifiermap = (KeyCode *) ckalloc(sizeof(KeyCode)*8);
    map->modifiermap[ShiftMapIndex] = VK_SHIFT;
    map->modifiermap[LockMapIndex] = VK_CAPITAL;
    map->modifiermap[ControlMapIndex] = VK_CONTROL;
    map->modifiermap[Mod1MapIndex] = VK_NUMLOCK;
    map->modifiermap[Mod2MapIndex] = VK_MENU;
    map->modifiermap[Mod3MapIndex] = VK_SCROLL;
    map->modifiermap[Mod4MapIndex] = 0;
    map->modifiermap[Mod5MapIndex] = 0;
    return map;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeModifiermap --
 *
 *	Deallocate a modifier map that was created by
 *	XGetModifierMapping.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the datastructure referenced by modmap.
 *
 *----------------------------------------------------------------------
 */

void
XFreeModifiermap(modmap)
    XModifierKeymap* modmap;
{
    ckfree((char *) modmap->modifiermap);
    ckfree((char *) modmap);
}

/*
 *----------------------------------------------------------------------
 *
 * XStringToKeysym --
 *
 *	Translate a keysym name to the matching keysym. 
 *
 * Results:
 *	Returns the keysym.  Since this is already handled by
 *	Tk's StringToKeysym function, we just return NoSymbol.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

KeySym
XStringToKeysym(string)
    _Xconst char *string;
{
    return NoSymbol;
}

/*
 *----------------------------------------------------------------------
 *
 * XKeysymToString --
 *
 *	Convert a keysym to character form.
 *
 * Results:
 *	Returns NULL, since Tk will have handled this already.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
XKeysymToString(keysym)
    KeySym keysym;
{
    return NULL;
}
