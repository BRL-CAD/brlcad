/*
 * tkMacOSXKeyboard.c --
 *
 *	Routines to support keyboard events on the Macintosh.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2005-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright (c) 2020 Marc Culler
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXEvent.h"
#include "tkMacOSXConstants.h"
#include "tkMacOSXKeysyms.h"

/*
 * About keyboards
 * ---------------
 * Keyboards are complicated.  This long comment is an attempt to provide
 * enough information about them to make it possible to read and understand
 * the code in this file.
 *
 * Every key on a keyboard is identified by a number between 0 and 127.  In
 * macOS, pressing or releasing a key on the keyboard generates an NSEvent of
 * type KeyDown, KeyUp or FlagsChanged.  The 8-bit identifier of the key that
 * was involved in this event is provided in the attribute [NSEvent keyCode].
 * Apple also refers to this number as a "Virtual KeyCode".  In this file, to
 * avoid confusion with other uses of the word keycode, we will refer to this
 * key identifier as a "virtual keycode", usually the value of a variable named
 * "virtual".
 *
 * Some of the keys on a keyboard, such as the Shift, Option, Command or
 * Control keys, are "modifier" keys.  The effect of pressing or releasing a
 * key depends on three quantities:
 *     - which key is being pressed or released
 *     - which modifier keys are being held down at the moment
 *     - the current keyboard layout
 * If the key is a modifier key then the effect of pressing or releasing it is
 * only to change the list of which modifier keys are being held down.  Apple
 * reports this by sending an NSEvent of type FlagsChanged.  X11 reports this
 * as a KeyPress or KeyRelease event for the modifier key.  Note that there may
 * be combinations of modifier key states and key presses which have no effect.
 *
 * In X11 every meaningful effect from a key action is identified by a 16 bit
 * value known as a keysym.  Every keysym has an associated string name, also
 * known as a keysym.  The Tk bind command uses the X11 keysym string to
 * specify a key event which should invoke a certain action and it provides the
 * numeric and symbolic keysyms to the bound proc as %N and %K respectively.
 * An X11 XEvent which reports a KeyPress or KeyRelease does not include the
 * keysym.  Instead it includes a platform-specific numerical value called a
 * keycode which is available to the bound procedure as %k.  A platform port of
 * Tk must provide functions which convert between keycodes and numerical
 * keysyms.  Conversion between numerical and symbolic keysyms is provided by
 * the generic Tk code, although platforms are allowed to provide their own by
 * defining the XKeysymToString and XStringToKeysym functions and undefining
 * the macro REDO_KEYSYM_LOOKUP.  This macOS port uses the conversion provided
 * by the generic code.
 *
 * When the keyboard focus is on a Tk widget which provides text input, there
 * are some X11 KeyPress events which cause text to be inserted.  We will call
 * these "printable" events. The UCS-32 character stored in the keycode field
 * of an XKeyEvent depends on more than the three items above.  It may also
 * depend on the sequence of keypresses that preceded the one being reported by
 * the XKeyEvent.  For example, on macOS an <Alt-e> event does not cause text
 * to be inserted but a following <a> event causes an accented 'a' to be
 * inserted.  The events in such a composition sequence, other than the final
 * one, are known as "dead-key" events.
 *
 * MacOS packages the information described above in a different way.  Every
 * meaningful effect from a key action *other than changing the state of
 * modifier keys* is identified by a unicode string which is provided as the
 * [NSEvent characters] attribute of a KeyDown or KeyUp event.  FlagsChanged
 * events do not have characters.  In principle, the characters attribute could
 * be an arbitrary unicode string but in practice it is always a single UTF-16
 * character which we usually store in a variable named keychar.  While the
 * keychar is a legal unicode code point, it does not necessarily represent a
 * glyph. MacOS uses unicode code points in the private-use range 0xF700-0xF8FF
 * for non-printable events which have no associated ASCII code point.  For
 * example, pressing the F2 key generates an NSEvent with the character 0xF705,
 * the Backspace key produces 0x7F (ASCII del) and the Delete key produces
 * 0xF728.
 *
 * With the exception of modifier keys, it is possible to translate between
 * numerical X11 keysyms and macOS keychars; this file constructs Tcl hash
 * tables to do this job, using data defined in the file tkMacOSXKeysyms.h.
 * The code here adopts the convention that the keychar of any modifier key
 * is MOD_KEYCHAR.  Keys which do not appear on any Macintosh keyboard, such
 * as the Menu key on PC keyboards, are assigned UNKNOWN_KEYCHAR.
 *
 * The macosx platform-specific scheme for generating a keycode when mapping an
 * NSEvent of type KeyUp, KeyDown or FlagsChanged to an XEvent of type KeyPress
 * or KeyRelease is as follows:
 *     keycode = (virtual << 24) | index << 22 | keychar
 * where index is a 2-bit quantity whose bits indicate the state of the Option
 * and Shift keys.
 *
 * A few remarks are in order.  First, we are using 32 bits for the keycode and
 * we are allowing room for up to 22 bits for the keychar.  This means that
 * there is enough room in the keycode to hold a UTF-32 character, which only
 * requires 21 bits.  Second, the KeyCode type for the keycode field in an
 * XEvent is currently defined as unsigned int, which was modified from the
 * unsigned short used in X11 in order to accomodate macOS. Finally, there is
 * no obstruction to generating KeyPress events for keys that represent letters
 * which do not exist on the current keyboard layout.  And different keyboard
 * layouts can assign a given letter to different keys.  So we need a
 * convention for what value to assign to "virtual" when computing the keycode
 * for a generated event.  The convention used here is as follows: If there is
 * a key on the current keyboard which produces the keychar, use the virtual
 * keycode of that key.  Otherwise set virtual = NO_VIRTUAL.
 */


/*
 * See tkMacOSXPrivate.h for macros and structures related to key event processing.
 */

/*
 * Hash tables and array used to translate between various key attributes.
 */

static Tcl_HashTable special2keysym;	/* Special virtual keycode to keysym */
static Tcl_HashTable keysym2keycode;	/* keysym to XEvent keycode */
static Tcl_HashTable keysym2unichar;	/* keysym to unichar */
static Tcl_HashTable unichar2keysym;	/* unichar to X11 keysym */
static Tcl_HashTable unichar2xvirtual;	/* unichar to virtual with index */
static UniChar xvirtual2unichar[512];	/* virtual with index to unichar */

/*
 * Flags.
 */

static BOOL initialized = NO;
static BOOL keyboardChanged = YES;

/*
 * Prototypes for static functions used in this file.
 */

static void	InitHashTables(void);
static void     UpdateKeymaps(void);
static int	KeyDataToUnicode(UniChar *uniChars, int maxChars,
			UInt16 keyaction, UInt32 virt, UInt32 modifiers,
			UInt32 * deadKeyStatePtr);

#pragma mark TKApplication(TKKeyboard)

@implementation TKApplication(TKKeyboard)
- (void) keyboardChanged: (NSNotification *) notification
{
    (void)notification;
#ifdef TK_MAC_DEBUG_NOTIFICATIONS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, notification);
#else
    (void)notification;
#endif
    keyboardChanged = YES;
    UpdateKeymaps();
}
@end

#pragma mark -

/*
 *----------------------------------------------------------------------
 *
 * InitHashTables --
 *
 *	Creates hash tables used by some of the functions in this file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocates memory & creates some hash tables.
 *
 *----------------------------------------------------------------------
 */

static void
InitHashTables(void)
{
    Tcl_HashEntry *hPtr;
    const KeyInfo *kPtr;
    const KeysymInfo *ksPtr;
    int dummy, index;

    Tcl_InitHashTable(&special2keysym, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&keysym2keycode, TCL_ONE_WORD_KEYS);
    for (kPtr = keyArray; kPtr->virt != 0; kPtr++) {
	MacKeycode macKC;
	macKC.v.o_s = 0;
	hPtr = Tcl_CreateHashEntry(&special2keysym, INT2PTR(kPtr->virt),
				   &dummy);
	Tcl_SetHashValue(hPtr, INT2PTR(kPtr->keysym));
	hPtr = Tcl_CreateHashEntry(&keysym2keycode, INT2PTR(kPtr->keysym),
				   &dummy);
	macKC.v.virt = kPtr->virt;
	macKC.v.keychar = kPtr->keychar;
	Tcl_SetHashValue(hPtr, INT2PTR(macKC.uint));

	/*
	 * The Carbon framework does not work for finding the unicode character
	 * of a special key.  But that does not depend on the keyboard layout,
	 * so we can record the information here.
	 */

	for (index = 3; index >= 0; index--) {
	    macKC.v.o_s = index;
	    xvirtual2unichar[macKC.x.xvirtual] = macKC.x.keychar;
	}
    }
    Tcl_InitHashTable(&keysym2unichar, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&unichar2keysym, TCL_ONE_WORD_KEYS);
    for (ksPtr = keysymTable; ksPtr->keysym != 0; ksPtr++) {
	hPtr = Tcl_CreateHashEntry(&keysym2unichar, INT2PTR(ksPtr->keysym),
				   &dummy);
	Tcl_SetHashValue(hPtr, INT2PTR(ksPtr->keycode));
	hPtr = Tcl_CreateHashEntry(&unichar2keysym, INT2PTR(ksPtr->keycode),
				   &dummy);
	Tcl_SetHashValue(hPtr, INT2PTR(ksPtr->keysym));
    }
    UpdateKeymaps();
    initialized = YES;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateKeymaps --
 *
 *	Called when the keyboard changes to update the hash tables that provide
 *      maps between unicode characters and virtual keycodes with indexes.  In
 *      order for the map from characters to virtual keycodes to be
 *      well-defined we have to ignore virtual keycodes for keypad keys, since
 *      each keypad key has the same character as the corresponding key on the
 *      main keyboard.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Initializes, if necessary, and updates the unichar2xvirtual hash table
 *      and the xvirtual2unichar array.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateKeymaps()
{
    static Bool keymapInitialized = false;
    Tcl_HashEntry *hPtr;
    int virt, index, dummy;

    if (!keymapInitialized) {
	Tcl_InitHashTable(&unichar2xvirtual, TCL_ONE_WORD_KEYS);
	keymapInitialized = true;
    } else {
	Tcl_DeleteHashTable(&unichar2xvirtual);
	Tcl_InitHashTable(&unichar2xvirtual, TCL_ONE_WORD_KEYS);
    }
    /*
     * This loop goes backwards so that a lookup by keychar will provide the
     * minimal modifier mask.  Simpler combinations will overwrite more complex
     * ones when constructing the table.
     */

    for (index = 3; index >= 0; index--) {
        for (virt = 0; virt < 128; virt++) {
	    MacKeycode macKC;
	    macKC.v = (keycode_v) {.virt = virt, .o_s = index, .keychar = 0};
	    int modifiers = INDEX2CARBON(index), result;
	    UniChar keychar = 0;
	    result = KeyDataToUnicode(&keychar, 1, kUCKeyActionDown, virt,
				      modifiers, NULL);
	    if (keychar == 0x10) {

		/*
		 * This is a special key, handled in InitHashTables.
		 */

		continue;
	    }
	    macKC.v.keychar = keychar;
	    if (! ON_KEYPAD(virt)) {
		hPtr = Tcl_CreateHashEntry(&unichar2xvirtual,
					   INT2PTR(macKC.x.keychar), &dummy);
		Tcl_SetHashValue(hPtr, INT2PTR(macKC.x.xvirtual));
            }
	    xvirtual2unichar[macKC.x.xvirtual] = macKC.x.keychar;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * KeyDataToUnicode --
 *
 *	Given MacOS key event data this function generates the keychar.  It
 *	does this by using OS resources from the Carbon framework.  Note that
 *      the Carbon functions used here are not aware of the keychars in the
 *      private-use range which macOS now uses for special keys.  For those
 *      keys this function returns 0x10 (ASCII dle).
 *
 *	The parameter deadKeyStatePtr can be NULL, if no deadkey handling is
 *	needed (which is always the case here).
 *
 *	This function is called in XKeycodeToKeysym and UpdateKeymaps.
 *
 * Results:
 *	The number of characters generated if any, 0 if we are waiting for
 *	another byte of a dead-key sequence.
 *
 * Side Effects:
 *	 Fills in the uniChars array with a Unicode string.
 *
 *----------------------------------------------------------------------
 */


static int
KeyDataToUnicode(
    UniChar *uniChars,
    int maxChars,
    UInt16 keyaction,
    UInt32 virt,
    UInt32 modifiers,
    UInt32 *deadKeyStatePtr)
{
    static const void *layoutData = NULL;
    static UInt32 keyboardType = 0;
    UniCharCount actuallength = 0;

    if (keyboardChanged) {
	TISInputSourceRef currentKeyboardLayout =
		TISCopyCurrentKeyboardLayoutInputSource();

	if (currentKeyboardLayout) {
	    CFDataRef keyLayoutData = (CFDataRef) TISGetInputSourceProperty(
		    currentKeyboardLayout, kTISPropertyUnicodeKeyLayoutData);

	    if (keyLayoutData) {
		layoutData = CFDataGetBytePtr(keyLayoutData);
		keyboardType = LMGetKbdType();
	    }
	    CFRelease(currentKeyboardLayout);
	}
	keyboardChanged = 0;
    }
    if (layoutData) {
	OptionBits options = 0;
	UInt32 dummyState;
	OSStatus err;

	virt &= 0xFF;
	modifiers = (modifiers >> 8) & 0xFF;
	if (!deadKeyStatePtr) {
	    options = kUCKeyTranslateNoDeadKeysMask;
	    dummyState = 0;
	    deadKeyStatePtr = &dummyState;
	}
	err = ChkErr(UCKeyTranslate, (const UCKeyboardLayout *)layoutData, virt, keyaction, modifiers,
		keyboardType, options, deadKeyStatePtr, maxChars,
		&actuallength, uniChars);
	if (!actuallength && *deadKeyStatePtr) {

	    /*
	     * We are waiting for another key.
	     */

	    return 0;
	}
	*deadKeyStatePtr = 0;
	if (err != noErr) {
	    actuallength = 0;
	}
    }
    return actuallength;
}

/*
 *----------------------------------------------------------------------
 *
 * XKeycodeToKeysym --
 *
 *	This is a stub function which translates from the keycode used in an
 *      XEvent to a numerical keysym.  On macOS, the display parameter is
 *      ignored and only the the virtual keycode stored in the .virtual bitfield
 *      of a MacKeycode.v.
 *
 * Results:
 *      Returns the corresponding numerical keysym, or NoSymbol if the keysym
 *      cannot be found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

KeySym
XkbKeycodeToKeysym(
    TCL_UNUSED(Display *),
    unsigned int keycode,
    TCL_UNUSED(int),
    int index)
{
    Tcl_HashEntry *hPtr;
    MacKeycode macKC;
    int modifiers, result;
    UniChar keychar = 0;

    if (!initialized) {
	InitHashTables();
    }
    macKC.uint = keycode;
    macKC.v.o_s = index;

    /*
     * First check if the virtual keycode corresponds to a special key, such as
     * an Fn function key or Tab, Backspace, Home, End, etc.
     */

    hPtr = Tcl_FindHashEntry(&special2keysym, INT2PTR(macKC.v.virt));
    if (hPtr != NULL) {
	return (KeySym) Tcl_GetHashValue(hPtr);
    }

    /*
     * If the virtual value in this keycode does not correspond to an actual
     * key in the current keyboard layout, try using its keychar to look up a
     * keysym.
     */

    if (macKC.v.virt > 127) {
	hPtr = Tcl_FindHashEntry(&unichar2keysym, INT2PTR(macKC.v.keychar));
	if (hPtr != NULL) {
	    return (KeySym) Tcl_GetHashValue(hPtr);
	}
    }

    /*
     * If the virtual keycode does belong to a key, use the virtual and the
     * Option-Shift from index to look up a keychar by using the Carbon
     * Framework; then translate the keychar to a keysym using the
     * unicode2keysym hash table.
     */

    modifiers = INDEX2CARBON(macKC.v.o_s);
    result = KeyDataToUnicode(&keychar, 1, kUCKeyActionDown, macKC.v.virt,
			      modifiers, NULL);
    if (result) {
	hPtr = Tcl_FindHashEntry(&unichar2keysym, INT2PTR(keychar));
	if (hPtr != NULL) {
	    return (KeySym) Tcl_GetHashValue(hPtr);
	}
    }
    return NoSymbol;
}

KeySym
XKeycodeToKeysym(
    TCL_UNUSED(Display *),
    KeyCode keycode,
    int index)
{
    return XkbKeycodeToKeysym(NULL, keycode, 0, index);
}

/*
 *----------------------------------------------------------------------
 *
 * TkpGetString --
 *
 *	This is a stub function which retrieves the string stored in the
 *      transchars field of an XEvent and converts it to a Tcl_DString.
 *
 * Results:
 *	Returns a pointer to the string value of the DString.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const char *
TkpGetString(
    TCL_UNUSED(TkWindow *),	/* Window where event occurred: Needed to get
				 * input context. */
    XEvent *eventPtr,		/* X keyboard event. */
    Tcl_DString *dsPtr)		/* Uninitialized or empty string to hold
				 * result. */
{
    MacKeycode macKC;
    char utfChars[8];
    int length = 0;

    macKC.uint = eventPtr->xkey.keycode;
    if (IS_PRINTABLE(macKC.v.keychar)) {
	length = TkUniCharToUtf(macKC.v.keychar, utfChars);
    }
    utfChars[length] = 0;

    Tcl_DStringInit(dsPtr);
    return Tcl_DStringAppend(dsPtr, utfChars, length);
}

/*
 *----------------------------------------------------------------------
 *
 * XGetModifierMapping --
 *
 *	X11 stub function to get the keycodes used as modifiers.  This
 *      is never called by the macOS port.
 *
 * Results:
 *	Returns a newly allocated modifier map.
 *
 * Side effects:
 *	Allocates a new modifier map data structure.
 *
 *----------------------------------------------------------------------
 */

XModifierKeymap *
XGetModifierMapping(
    TCL_UNUSED(Display *))
{
    XModifierKeymap *modmap;

    modmap = (XModifierKeymap *)ckalloc(sizeof(XModifierKeymap));
    modmap->max_keypermod = 0;
    modmap->modifiermap = NULL;
    return modmap;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeModifiermap --
 *
 *	Deallocates a modifier map that was created by XGetModifierMapping.
 *      This is also never called by the macOS port.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the datastructure referenced by modmap.
 *
 *----------------------------------------------------------------------
 */

int
XFreeModifiermap(
    XModifierKeymap *modmap)
{
    if (modmap->modifiermap != NULL) {
	ckfree(modmap->modifiermap);
    }
    ckfree(modmap);
    return Success;
}

/*
 *----------------------------------------------------------------------
 *
 * XKeysymToString, XStringToKeysym --
 *
 *	These X11 stub functions map keysyms to strings & strings to keysyms.
 *      A platform can do its own conversion by defining these and undefining
 *      REDO_KEYSYM_LOOKUP.  The macOS port defines REDO_KEYSYM_LOOKUP so these
 *      are never called and Tk does the conversion for us.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
XKeysymToString(
    TCL_UNUSED(KeySym))
{
    return NULL;
}

KeySym
XStringToKeysym(
    TCL_UNUSED(const char *))
{
    return NoSymbol;
}

/*
 *----------------------------------------------------------------------
 *
 * XKeysymToKeycode --
 *
 *	This is a stub function which converts a numerical keysym to the
 *      platform-specific keycode used in a KeyPress or KeyRelease XEvent.
 *      For macOS the keycode is an unsigned int with bitfields described
 *      in the definition of the MacKeycode type.
 *
 * Results:
 *
 *      A macOS KeyCode. See the description of keycodes at the top of this
 *	file and the definition of the MacKeycode type in tkMacOSXPrivate.h.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

KeyCode
XKeysymToKeycode(
    TCL_UNUSED(Display *),
    KeySym keysym)
{
    Tcl_HashEntry *hPtr;
    MacKeycode macKC;
    if (!initialized) {
	InitHashTables();
    }

    /*
     * First check for a special key.
     */

    hPtr = Tcl_FindHashEntry(&keysym2keycode, INT2PTR(keysym));
    if (hPtr != NULL) {
	return (KeyCode) PTR2INT(Tcl_GetHashValue(hPtr));
    }

    /*
     * Initialize the keycode as if the keysym cannot be converted to anything
     * else.
     */

    macKC.v.virt = NO_VIRTUAL;
    macKC.v.o_s = 0;
    macKC.v.keychar = 0;

    /*
     * If the keysym is recognized fill in the keychar.  Also fill in the
     * xvirtual field if the key exists on the current keyboard.
     */

    hPtr = (Tcl_HashEntry *) Tcl_FindHashEntry(&keysym2unichar,
					       INT2PTR(keysym));
    if (hPtr != NULL) {
	unsigned long data = (unsigned long) Tcl_GetHashValue(hPtr);
	macKC.x.keychar = (unsigned int) data;
	hPtr = Tcl_FindHashEntry(&unichar2xvirtual, INT2PTR(macKC.x.keychar));
	if (hPtr != NULL) {
	    data = (unsigned long) Tcl_GetHashValue(hPtr);
	    macKC.x.xvirtual = (unsigned int) data;
	}
    }
    return (KeyCode) macKC.uint;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpSetKeycodeAndState --
 *
 *	This function accepts a keysym and an XEvent and sets some fields of
 *	the XEvent.  It is used by the event generate command.
 *
 * Results:
 *      None
 *
 * Side effects:
 *
 *	Modifies the XEvent. Sets the xkey.keycode to a keycode value formatted
 *	by XKeysymToKeycode and updates the shift and option flags in
 *	xkey.state if either of those modifiers is required to generate the
 *	keysym.
 *
 *----------------------------------------------------------------------
 */
void
TkpSetKeycodeAndState(
    TCL_UNUSED(Tk_Window),
    KeySym keysym,
    XEvent *eventPtr)
{
    if (keysym == NoSymbol) {
	eventPtr->xkey.keycode = 0;
    } else {
	int eventIndex = STATE2INDEX(eventPtr->xkey.state);
	MacKeycode macKC;
	macKC.uint = XKeysymToKeycode(NULL, keysym);

	/*
	 * We have a virtual keycode and a minimal choice for Shift and Option
	 * modifiers which generates the keychar that corresponds to the
	 * specified keysym.  But we might not have the correct keychar yet,
	 * because the xEvent may have specified modifiers beyond our minimal
	 * set.  For example, the events described by <Oslash>, <Shift-oslash>,
	 * <Shift-Option-O> and <Shift-Option-o> should all produce the same
	 * uppercase Danish O.  So we may need to add the extra modifiers and
	 * do another lookup for the keychar.  We don't want to do this for
	 * special keys, however.
	 */

	if (macKC.v.o_s != eventIndex) {
	    macKC.v.o_s |= eventIndex;
	}
	if (macKC.v.keychar < 0xF700) {
	    UniChar keychar = macKC.v.keychar;
	    NSString *str, *lower, *upper;
	    if (macKC.v.virt != NO_VIRTUAL) {
		macKC.x.keychar = xvirtual2unichar[macKC.x.xvirtual];
	    } else {
		str = [[NSString alloc] initWithCharacters:&keychar length:1];
		lower = [str lowercaseString];
		upper = [str uppercaseString];
		if (![str isEqual: lower]) {
		    macKC.v.o_s |= INDEX_SHIFT;
		}
		if (macKC.v.o_s & INDEX_SHIFT) {
		    macKC.v.keychar = [upper characterAtIndex:0];
		}
	    }
	}
	eventPtr->xkey.keycode = macKC.uint;
	eventPtr->xkey.state |= INDEX2STATE(macKC.v.o_s);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpGetKeySym --
 *
 *	This is a stub function called in tkBind.c.  Given a KeyPress or
 *	KeyRelease XEvent, it maps the keycode in the event to a numerical
 *      keysym.
 *
 * Results:
 *	The return value is the keysym corresponding to eventPtr, or NoSymbol
 *	if no matching keysym could be found.
 *
 * Side effects:
 *	In the first call for a given display, calls TkpInitKeymapInfo.
 *
 *
 *----------------------------------------------------------------------
 */

KeySym
TkpGetKeySym(
    TkDisplay *dispPtr,		/* Display in which to map keycode. */
    XEvent *eventPtr)		/* Description of X event. */
{
    KeySym sym;
    int index;
    MacKeycode macKC;
    macKC.uint = eventPtr->xkey.keycode;

    /*
     * Refresh the mapping information if it's stale.
     */

    if (dispPtr->bindInfoStale) {
	TkpInitKeymapInfo(dispPtr);
    }

    /*
     * Modifier key events have a special mac keycode (see tkProcessKeyEvent).
     */

    if (macKC.v.keychar == MOD_KEYCHAR) {
	switch (macKC.v.virt) {
	case 54:
	    return XK_Meta_R;
	case 55:
	    return XK_Meta_L;
	case 56:
	    return XK_Shift_L;
	case 57:
	    return XK_Caps_Lock;
	case 58:
	    return XK_Alt_L;
	case 59:
	    return XK_Control_L;
	case 60:
	    return XK_Shift_R;
	case 61:
	    return XK_Alt_R;
	case 62:
	    return XK_Control_R;
	case 63:
	    return XK_Super_L;
	default:
	    return NoSymbol;
	}
    }

    /*
     * Figure out which of the four slots in the keymap vector to use for this
     * key. Refer to Xlib documentation for more info on how this computation
     * works.
     */

    index = STATE2INDEX(eventPtr->xkey.state);
    if (eventPtr->xkey.state & LockMask) {
	index |= INDEX_SHIFT;
    }

    /*
     * First do the straightforward lookup.
     */

    sym = XkbKeycodeToKeysym(dispPtr->display, macKC.uint, 0, index);

    /*
     * Special handling: If the key was shifted because of Lock, which is only
     * caps lock on macOS, not shift lock, and if the shifted keysym isn't
     * upper-case alphabetic, then switch back to the unshifted keysym.
     */

    if ((index & INDEX_SHIFT) && !(eventPtr->xkey.state & ShiftMask)) {
	if ((sym == NoSymbol) || !Tcl_UniCharIsUpper(sym)) {
	    sym = XkbKeycodeToKeysym(dispPtr->display, macKC.uint, 0,
				   index & ~INDEX_SHIFT);
	}
    }

    /*
     * Another bit of special handling: If this is a shifted key and there is
     * no keysym defined, then use the keysym for the unshifted key.
     */

    if ((index & INDEX_SHIFT) && (sym == NoSymbol)) {
	sym = XkbKeycodeToKeysym(dispPtr->display, macKC.uint, 0,
			       index & ~INDEX_SHIFT);
    }
    return sym;
}

/*
 *--------------------------------------------------------------
 *
 * TkpInitKeymapInfo --
 *
 *	This procedure initializes fields in the display that pertain
 *      to modifier keys.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifier key information in dispPtr is initialized.
 *
 *--------------------------------------------------------------
 */

void
TkpInitKeymapInfo(
    TkDisplay *dispPtr)		/* Display for which to recompute keymap
				 * information. */
{
    dispPtr->bindInfoStale = 0;

    /*
     * On macOS the caps lock key is always interpreted to mean that alphabetic
     * keys become uppercase but other keys do not get shifted.  (X11 allows
     * a configuration option which makes the caps lock equivalent to holding
     * down the shift key.)
     * There is no offical "Mode_switch" key.
     */

    dispPtr->lockUsage = LU_CAPS;

    /* This field is no longer used by tkBind.c */

    dispPtr->modeModMask = 0;

    /* The Alt and Meta keys are interchanged on Macintosh keyboards compared
     * to PC keyboards.  These fields could be set to make the Alt key on a PC
     * keyboard behave likd an Alt key. That would also require interchanging
     * Mod1Mask and Mod2Mask in tkMacOSXKeyEvent.c.
     */

    dispPtr->altModMask = 0;
    dispPtr->metaModMask = 0;

    /*
     * The modKeyCodes table lists the keycodes that appear in KeyPress or
     * KeyRelease XEvents for modifier keys.  In tkBind.c this table is
     * searched to determine whether an XEvent corresponds to a modifier key.
     */

    if (dispPtr->modKeyCodes != NULL) {
	ckfree(dispPtr->modKeyCodes);
    }
    dispPtr->numModKeyCodes = NUM_MOD_KEYCODES;
    dispPtr->modKeyCodes = (KeyCode *)ckalloc(NUM_MOD_KEYCODES * sizeof(KeyCode));
    for (int i = 0; i < NUM_MOD_KEYCODES; i++) {
	dispPtr->modKeyCodes[i] = XKeysymToKeycode(NULL, modKeyArray[i]);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TkMacOSXAddVirtual --
 *
 *	This procedure is an internal utility which accepts an unsigned int
 *      that has been partially filled as a MacKeycode, having the Option and
 *      Shift state set in the o_s field and the keychar field set but with the
 *      virtual keycode blank.  It looks up the virtual keycode for the keychar
 *      (possibly NO_VIRTUAL) and returns an unsigned int which is a complete
 *      MacKeycode with the looked up virtual keycode added.  This is used when
 *      creating XEvents for the unicode characters which are generated by the
 *      NSTextInputClient.
 *
 * Results:
 *      An unsigned int which is a complete MacKeycode, including a virtual
 *	keycode which matches the Option-Shift state and keychar.
 *
 * Side effects:
 *	None
 *
 *--------------------------------------------------------------
 */
unsigned
TkMacOSXAddVirtual(
    unsigned int keycode)
{
    MacKeycode macKC;
    Tcl_HashEntry *hPtr;
    macKC.uint = keycode;

    if (!initialized) {
	InitHashTables();
    }

    hPtr = (Tcl_HashEntry *) Tcl_FindHashEntry(&unichar2xvirtual,
					       INT2PTR(macKC.v.keychar));
    if (hPtr != NULL) {
	unsigned long data = (unsigned long) Tcl_GetHashValue(hPtr);
	macKC.x.xvirtual = (unsigned int) data;
    } else {
	macKC.v.virt = NO_VIRTUAL;
    }
    return macKC.uint;
}
/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
