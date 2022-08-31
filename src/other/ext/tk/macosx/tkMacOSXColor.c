/*
 * TkMacOSXColor.c --
 *
 *	This file maintains a database of color values for the Tk
 *	toolkit, in order to avoid round-trips to the server to
 *	map color names to pixel values.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright (c) 2020 Marc Culler
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkColor.h"
#include "tkMacOSXColor.h"

static Tcl_HashTable systemColors;
static int numSystemColors;
static int rgbColorIndex;
static int controlAccentIndex;
static int selectedTabTextIndex;
static int pressedButtonTextIndex;
static Bool useFakeAccentColor = NO;
static SystemColorDatum **systemColorIndex;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101400
static NSAppearance *lightAqua = nil;
static NSAppearance *darkAqua = nil;
#endif
static NSColorSpace* sRGB = NULL;
static const CGFloat WINDOWBACKGROUND[4] =
    {236.0 / 255, 236.0 / 255, 236.0 / 255, 1.0};

void initColorTable()
{
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    Tcl_InitHashTable(&systemColors, TCL_STRING_KEYS);
    SystemColorDatum *entry, *oldEntry;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    int newPtr, index = 0;
    NSColorList *systemColorList = [NSColorList colorListNamed:@"System"];
    NSString *key;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101400
    if (@available(macOS 10.14, *)) {
	darkAqua = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
        lightAqua = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    }
#endif

    /*
     * Build a hash table for looking up a color by its name.
     * First add all of the static entries from tkMacOSXColor.h
     */

    for (entry = systemColorData; entry->name != NULL; entry++) {
	hPtr = Tcl_CreateHashEntry(&systemColors, entry->name, &newPtr);
	if (entry->type == semantic) {
	    NSString *colorName = [[NSString alloc]
				   initWithCString:entry->macName
					  encoding:NSUTF8StringEncoding];
	    SEL colorSelector = NSSelectorFromString(colorName);
	    if (![NSColor respondsToSelector:colorSelector]) {
		if ([colorName isEqualToString:@"controlAccentColor"]) {
		    useFakeAccentColor = YES;
		} else if (   ![colorName isEqualToString:@"selectedTabTextColor"]
			   && ![colorName isEqualToString:@"pressedButtonTextColor"]) {
		    /* Uncomment to print all unsupported colors:              */
		    /* printf("Unsupported color %s\n", colorName.UTF8String); */
		    continue;
		}
	    }
	    entry->selector = [colorName retain];
	}
	if (newPtr == 0) {
	    oldEntry = (SystemColorDatum *) Tcl_GetHashValue(hPtr);
	    entry->index = oldEntry->index;
	    [oldEntry->selector release];
	} else {
	    entry->index = index++;
	}
	Tcl_SetHashValue(hPtr, entry);
    }

    /*
     * Add all of the colors in the System ColorList.
     */

    for (key in [systemColorList allKeys]) {
	int length = [key lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
	char *name;
	entry = (SystemColorDatum *)ckalloc(sizeof(SystemColorDatum));
	bzero(entry, sizeof(SystemColorDatum));
	name = (char *)ckalloc(length + 1);
	strcpy(name, key.UTF8String);
	name[0] = toupper(name[0]);
        if (!strcmp(name, "WindowBackgroundColor")) {

	    /*
	     * Avoid black windows on old systems.
	     */

	    continue;
	}
	entry->type=semantic;
	entry->name = name;
	entry->selector = [key retain];
	hPtr = Tcl_CreateHashEntry(&systemColors, entry->name, &newPtr);
	if (newPtr == 0) {
	    oldEntry = (SystemColorDatum *) Tcl_GetHashValue(hPtr);
	    entry->index = oldEntry->index;
	    [oldEntry->selector release];
	} else {
	    entry->index = index++;
	}
	Tcl_SetHashValue(hPtr, entry);
    }

    /*
     * Build an array for looking up a color by its index.
     */

    numSystemColors = index;
    systemColorIndex = (SystemColorDatum **)ckalloc(numSystemColors * sizeof(SystemColorDatum *));
    for (hPtr = Tcl_FirstHashEntry(&systemColors, &search); hPtr != NULL;
	 hPtr = Tcl_NextHashEntry(&search)) {
	entry = (SystemColorDatum *) Tcl_GetHashValue(hPtr);
	if (entry == NULL) {
	    Tcl_Panic("Unsupported semantic color with no supported backup!");
	}
	systemColorIndex[entry->index] = entry;
    }

    /*
     * Remember the indexes of some special entries.
     */

    hPtr = Tcl_FindHashEntry(&systemColors, "Pixel");
    entry = (SystemColorDatum *) Tcl_GetHashValue(hPtr);
    rgbColorIndex = entry->index;
    hPtr = Tcl_FindHashEntry(&systemColors, "ControlAccentColor");
    entry = (SystemColorDatum *) Tcl_GetHashValue(hPtr);
    controlAccentIndex = entry->index;
    hPtr = Tcl_FindHashEntry(&systemColors, "SelectedTabTextColor");
    entry = (SystemColorDatum *) Tcl_GetHashValue(hPtr);
    selectedTabTextIndex = entry->index;
    hPtr = Tcl_FindHashEntry(&systemColors, "PressedButtonTextColor");
    entry = (SystemColorDatum *) Tcl_GetHashValue(hPtr);
    pressedButtonTextIndex = entry->index;
    [pool drain];
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXRGBPixel --
 *
 *	Return an unsigned long value suitable for use in the pixel
 *	field of an XColor with the specified red, green and blue
 *	intensities.  The inputs are cast as unsigned longs but are
 *      expected to have values representable by an unsigned char.
 *
 *      This is called in the TkpGetPixel macro, used in xcolor.c,
 *      and in ImageGetPixel.
 *
 * Results:
 *	An unsigned long that can be used as the pixel field of an XColor.
 *
 * Side effects:
 *	None.
 *----------------------------------------------------------------------
 */
MODULE_SCOPE
unsigned long
TkMacOSXRGBPixel(
    unsigned long red,
    unsigned long green,
    unsigned long blue)
{
    MacPixel p = {0};
    p.pixel.colortype = rgbColor;
    p.pixel.value = ((red & 0xff) << 16)  |
	            ((green & 0xff) << 8) |
	            (blue & 0xff);
    return p.ulong;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXClearPixel --
 *
 *	Return the unsigned long value that appears in the pixel
 *	field of the XColor for systemTransparentColor.
 *
 *      This is used in tkMacOSXImage.c.
 *
 * Results:
 *	The unsigned long that appears in the pixel field of the XColor
 *      for systemTransparentPixel.
 *
 * Side effects:
 *	None.
 *----------------------------------------------------------------------
 */
MODULE_SCOPE
unsigned long TkMacOSXClearPixel(
    void)
{
    MacPixel p = {0};
    p.pixel.value = 0;
    p.pixel.colortype = clearColor;
    return p.ulong;
}


/*
 *----------------------------------------------------------------------
 *
 * GetEntryFromPixel --
 *
 *	Look up a SystemColorDatum which describes the XColor with
 *      the specified value as its pixel field.
 *
 * Results:
 *	A pointer to a SystemColorDatum, or NULL if the pixel value is
 *	invalid.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

SystemColorDatum*
GetEntryFromPixel(
    unsigned long pixel)
{
    MacPixel p = {0};
    int index = rgbColorIndex;

    p.ulong = pixel;
    if (p.pixel.colortype != rgbColor) {
	index = p.pixel.value;
    }
    if (index < numSystemColors) {
	return systemColorIndex[index];
    } else {
	return NULL;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * GetRGBA --
 *
 *	Given a SystemColorDatum and a pointer to an array of 4 CGFloats, store
 *      the associated RGBA color values in the array.  In the case of the
 *      RGBColor datum, the unsigned long pixel value containing the RGB values
 *      must also be provided as the pixel parameter.  Otherwise the pixel
 *      parameter is ignored.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The array rgba is filled in.
 *
 *----------------------------------------------------------------------
 */

static void
GetRGBA(
    SystemColorDatum *entry,
    unsigned long pixel,
    CGFloat *rgba)
{
    NSColor *bgColor, *color = nil;
    int OSVersion = [NSApp macOSVersion];

    if (!sRGB) {
	sRGB = [NSColorSpace sRGBColorSpace];
    }

    switch (entry->type) {
    case rgbColor:
	rgba[0] = ((pixel >> 16) & 0xff) / 255.0;
	rgba[1] = ((pixel >>  8) & 0xff) / 255.0;
	rgba[2] = ((pixel      ) & 0xff) / 255.0;
	break;
    case ttkBackground:

	/*
	 * Prior to OSX 10.14, getComponents returns black when applied to
	 * windowBackGroundColor.
	 */

	if ([NSApp macOSVersion] < 101400) {
	    for (int i = 0; i < 3; i++) {
		rgba[i] = WINDOWBACKGROUND[i];
	    }
	} else {
	    bgColor = [[NSColor windowBackgroundColor] colorUsingColorSpace:sRGB];
	    [bgColor getComponents: rgba];
	}
	if (rgba[0] + rgba[1] + rgba[2] < 1.5) {
	    for (int i=0; i<3; i++) {
		rgba[i] += entry->value*8.0 / 255.0;
	    }
	} else {
	    for (int i=0; i<3; i++) {
		rgba[i] -= entry->value*8.0 / 255.0;
	    }
	}
	break;
    case clearColor:
	rgba[0] = rgba[1] = rgba[2] = 1.0;
	rgba[3] = 0;
	break;
    case semantic:
	if (entry->index == controlAccentIndex && useFakeAccentColor) {
#if MAC_OS_X_VERSION_MIN_REQUIRED < 101400
	    color = [[NSColor colorForControlTint: [NSColor currentControlTint]]
			      colorUsingColorSpace:sRGB];
#endif
	} else if (entry->index == selectedTabTextIndex) {
	    if (OSVersion > 100600 && OSVersion < 110000) {
		color = [[NSColor whiteColor] colorUsingColorSpace:sRGB];
	    } else {
		color = [[NSColor textColor] colorUsingColorSpace:sRGB];
	    }
	} else if (entry->index == pressedButtonTextIndex) {
	    if (OSVersion < 120000) {
		color = [[NSColor whiteColor] colorUsingColorSpace:sRGB];
	    } else {
		color = [[NSColor blackColor] colorUsingColorSpace:sRGB];
	    }
	} else {
	    color = [[NSColor valueForKey:entry->selector] colorUsingColorSpace:sRGB];
	}
	[color getComponents: rgba];
	break;
    case HIText:
#ifdef __LP64__
	color = [[NSColor textColor] colorUsingColorSpace:sRGB];
	[color getComponents: rgba];
#else
	{
	    OSStatus err = noErr;
	    RGBColor rgb;
	    err = GetThemeTextColor(kThemeTextColorPushButtonActive, 32,
                    true, &rgb);
	    if (err == noErr) {
		rgba[0] = (CGFloat) rgb.red / 65535;
		rgba[1] = (CGFloat) rgb.green / 65535;
		rgba[2] = (CGFloat) rgb.blue / 65535;
	    }
	}
#endif
	break;
    case HIBackground:
	color = [[NSColor windowBackgroundColor] colorUsingColorSpace:sRGB];
	[color getComponents: rgba];
	break;
    default:
	break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SetCGColorComponents --
 *
 *	Set the components of a CGColorRef from an XColor pixel value and a
 *      SystemColorDatum.  The pixel value is only used in the case where
 *      the color is of type rgbColor.  In that case the normalized XColor RGB
 *      values are copied into the CGColorRef.  Otherwise the components are
 *      computed from the SystemColorDatum.
 *
 *      In 64 bit macOS systems there are no HITheme functions which convert
 *      HIText or HIBackground colors to CGColors.  (GetThemeTextColor was
 *      removed, and it was never possible with backgrounds.)  On 64-bit systems
 *      we replace all HIText colors by systemTextColor and all HIBackground
 *      colors by systemWindowBackgroundColor.
 *
 * Results:
 *	True if the function succeeds, false otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Bool
SetCGColorComponents(
    SystemColorDatum *entry,
    unsigned long pixel,
    CGColorRef *c)
{
    CGFloat rgba[4] = {0, 0, 0, 1};

    /*
     * This function is called before our autorelease pool is set up,
     * so it needs its own pool.
     */

    NSAutoreleasePool *pool = [NSAutoreleasePool new];

    if (entry->type == HIBrush) {
     	OSStatus err = ChkErr(HIThemeBrushCreateCGColor, entry->value, c);
     	return err == noErr;
    }
    GetRGBA(entry, pixel, rgba);
    *c = CGColorCreate(sRGB.CGColorSpace, rgba);
    [pool drain];
    return true;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInDarkMode --
 *
 *      Tests whether the given window's NSView has a DarkAqua Appearance.
 *
 * Results:
 *      Returns true if the NSView is in DarkMode, false if not.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE Bool
TkMacOSXInDarkMode(Tk_Window tkwin)
{

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101400
    if (@available(macOS 10.14, *)) {
        TkWindow *winPtr = (TkWindow*) tkwin;
	NSAppearanceName name;
	NSView *view = nil;
	if (winPtr && winPtr->privatePtr) {
	    view = TkMacOSXGetNSViewForDrawable((Drawable)winPtr->privatePtr);
	}
	if (view) {
	    name = [[view effectiveAppearance] name];
	} else {
	    name = [[NSAppearance currentAppearance] name];
	}
	return (name == NSAppearanceNameDarkAqua);
    }
#endif
    return false;
}

/*
 *----------------------------------------------------------------------
 *
 * TkSetMacColor --
 *
 *	Sets the components of a CGColorRef from an XColor pixel value.  The
 *      pixel value is used to look up the color in the system color table, and
 *      then SetCGColorComponents is called with the table entry and the pixel
 *      value.  The parameter macColor should be a pointer to a CGColorRef.
 *
 * Results:
 *      Returns false if the color is not found, true otherwise.
 *
 * Side effects:
 *	The CGColorRef referenced by the variable macColor may be modified.
 *
 *----------------------------------------------------------------------
 */

int
TkSetMacColor(
    unsigned long pixel,	/* Pixel value to convert. */
    void *macColor)		/* CGColorRef to modify. */
{
    CGColorRef *color = (CGColorRef*)macColor;
    SystemColorDatum *entry = GetEntryFromPixel(pixel);

    if (entry) {
	return SetCGColorComponents(entry, pixel, color);
    } else {
	return false;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetNSColor --
 *
 *	Creates an autoreleased NSColor from a X style pixel value.
 *      The return value is nil if the pixel value is invalid.
 *
 * Results:
 *	A possibly nil pointer to an NSColor.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

NSColor*
TkMacOSXGetNSColor(
    GC gc,
    unsigned long pixel)		/* Pixel value to convert. */
{
    CGColorRef cgColor;
    NSColor *nsColor = nil;

    TkSetMacColor(pixel, &cgColor);
    nsColor = [NSColor colorWithColorSpace:sRGB
		   components:CGColorGetComponents(cgColor)
		   count:CGColorGetNumberOfComponents(cgColor)];
    return nsColor;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXSetColorInContext --
 *
 *	Sets the fill and stroke colors in the given CGContext to the CGColor
 *	which corresponds to the XColor having the specified value for its pixel
 *	field.
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
TkMacOSXSetColorInContext(
    GC gc,
    unsigned long pixel,
    CGContextRef context)
{
    OSStatus err = noErr;
    CGColorRef cgColor = nil;
    SystemColorDatum *entry = GetEntryFromPixel(pixel);
    CGRect rect;
    HIThemeBackgroundDrawInfo info = {0, kThemeStateActive, 0};

    if (entry) {
	switch (entry->type) {
	case HIBrush:
	    err = ChkErr(HIThemeSetFill, entry->value, NULL, context,
		    kHIThemeOrientationNormal);
	    if (err == noErr) {
		err = ChkErr(HIThemeSetStroke, entry->value, NULL, context,
			kHIThemeOrientationNormal);
	    }
	    break;
	case HIText:
	    err = ChkErr(HIThemeSetTextFill, entry->value, NULL, context,
		    kHIThemeOrientationNormal);
	    break;
	case HIBackground:
	    info.kind = entry->value;
	    rect = CGContextGetClipBoundingBox(context);
	    err = ChkErr(HIThemeApplyBackground, &rect, &info,
		    context, kHIThemeOrientationNormal);
	    break;
	default:
	    SetCGColorComponents(entry, pixel, &cgColor);
	    break;
	}
    }
    if (cgColor) {
	CGContextSetFillColorWithColor(context, cgColor);
	CGContextSetStrokeColorWithColor(context, cgColor);
	CGColorRelease(cgColor);
    }
    if (err != noErr) {
	TkMacOSXDbgMsg("Ignored unknown pixel value 0x%lx", pixel);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpGetColor --
 *
 *	Create a new TkColor for the color with the given name, for use in the
 *      specified window. The colormap field is set to lightColormap if the
 *      window has a LightAqua appearance, or darkColormap if the window has a
 *      DarkAqua appearance.  TkColors with different colormaps are managed
 *      separately in the per-display table of TkColors maintained by Tk.
 *
 *      This function is called by Tk_GetColor.
 *
 * Results:
 *	Returns a newly allocated TkColor, or NULL on failure.
 *
 * Side effects:
 *
 *	Allocates memory for the TkColor structure.
 *
 *----------------------------------------------------------------------
 */

TkColor *
TkpGetColor(
    Tk_Window tkwin,		/* Window in which color will be used. */
    Tk_Uid name)		/* Name of color to be allocated (in form
				 * suitable for passing to XParseColor). */
{
    Display *display = NULL;
    TkColor *tkColPtr;
    XColor color;
    Colormap colormap = tkwin ? Tk_Colormap(tkwin) : noColormap;
    NSView *view = nil;
    static Bool initialized = NO;
    static NSColorSpace* sRGB = nil;

    if (!initialized) {
	initialized = YES;
	sRGB = [NSColorSpace sRGBColorSpace];
	initColorTable();
    }
    if (tkwin) {
	display = Tk_Display(tkwin);
	Drawable d = Tk_WindowId(tkwin);
	view = TkMacOSXGetNSViewForDrawable(d);
    }

    /*
     * Check to see if this is a system color. If not, just call XParseColor.
     */

    if (strncasecmp(name, "system", 6) == 0) {
	Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&systemColors, name + 6);
	MacPixel p = {0};

	if (hPtr != NULL) {
	    SystemColorDatum *entry = (SystemColorDatum *)Tcl_GetHashValue(hPtr);
	    CGColorRef c;

	    p.pixel.colortype = entry->type;
	    p.pixel.value = entry->index;
	    color.pixel = p.ulong;
	    if (entry->type == semantic) {
		CGFloat rgba[4];
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101400
		if (@available(macOS 10.14, *)) {
		    NSAppearance *savedAppearance = [NSAppearance currentAppearance];
		    NSAppearance *windowAppearance = savedAppearance;
		    if (view) {
			windowAppearance = [view effectiveAppearance];
		    }
		    if ([windowAppearance name] == NSAppearanceNameDarkAqua) {
			colormap = darkColormap;
		    } else {
			colormap = lightColormap;
		    }
		    [NSAppearance setCurrentAppearance:windowAppearance];
		    GetRGBA(entry, p.ulong, rgba);
		    [NSAppearance setCurrentAppearance:savedAppearance];
		} else {
		    GetRGBA(entry, p.ulong, rgba);
		}
#else
		GetRGBA(entry, p.ulong, rgba);
#endif
		color.red   = rgba[0] * 65535.0;
		color.green = rgba[1] * 65535.0;
		color.blue  = rgba[2] * 65535.0;
		goto validXColor;
	    } else if (SetCGColorComponents(entry, 0, &c)) {
		const size_t n = CGColorGetNumberOfComponents(c);
		const CGFloat *rgba = CGColorGetComponents(c);

		switch (n) {
		case 4:
		    color.red   = rgba[0] * 65535.0;
		    color.green = rgba[1] * 65535.0;
		    color.blue  = rgba[2] * 65535.0;
		    break;
		case 2:
		    color.red = color.green = color.blue = rgba[0] * 65535.0;
		    break;
		default:
		    Tcl_Panic("CGColor with %d components", (int) n);
		}
		CGColorRelease(c);
		goto validXColor;
	    }
	}
    }
    if (TkParseColor(display, colormap, name, &color) == 0) {
	return NULL;
    }

validXColor:
    tkColPtr = (TkColor *)ckalloc(sizeof(TkColor));
    tkColPtr->colormap = colormap;
    tkColPtr->color = color;
    return tkColPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpGetColorByValue --
 *
 *	Given an pointer to an XColor, construct a TkColor whose red, green and
 *	blue intensities match those of the XColor as closely as possible.  For
 *	the Macintosh, this means that the colortype bitfield of the pixel
 *	value will be RGBColor and that the color intensities stored in its
 *	24-bit value bitfield are computed from the 16-bit red green and blue
 *	values in the XColor by dividing by 256.
 *
 * Results:
 *	A pointer to a newly allocated TkColor structure.
 *
 * Side effects:
 *	May invalidate the colormap cache for the specified window.
 *	Allocates memory for a TkColor structure.
 *
 *----------------------------------------------------------------------
 */

TkColor *
TkpGetColorByValue(
    TCL_UNUSED(Tk_Window),		/* Window in which color will be used. */
    XColor *colorPtr)		/* Red, green, and blue fields indicate
				 * desired color. */
{
    TkColor *tkColPtr = (TkColor *)ckalloc(sizeof(TkColor));

    tkColPtr->color.red = colorPtr->red;
    tkColPtr->color.green = colorPtr->green;
    tkColPtr->color.blue = colorPtr->blue;
    tkColPtr->color.pixel = TkpGetPixel(colorPtr);
    return tkColPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Stub functions --
 *
 *	These functions are just stubs for functions that either
 *	don't make sense on the Mac or have yet to be implemented.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	These calls do nothing - which may not be expected.
 *
 *----------------------------------------------------------------------
 */

Status
XAllocColor(
    Display *display,		/* Display. */
    TCL_UNUSED(Colormap),		/* Not used. */
    XColor *colorPtr)		/* XColor struct to modify. */
{
    display->request++;
    colorPtr->pixel = TkpGetPixel(colorPtr);
    return 1;
}

Colormap
XCreateColormap(
    TCL_UNUSED(Display *),		/* Display. */
    TCL_UNUSED(Window),		/* X window. */
    TCL_UNUSED(Visual *),		/* Not used. */
    TCL_UNUSED(int))			/* Not used. */
{
    static Colormap index = 16;

    /*
     * Just return a new value each time, large enough that it will not
     * conflict with any value of the macColormap enum.
     */
    return index++;
}

int
XFreeColormap(
    TCL_UNUSED(Display *),		/* Display. */
    TCL_UNUSED(Colormap))		/* Colormap. */
{
    return Success;
}

int
XFreeColors(
    TCL_UNUSED(Display *),		/* Display. */
    TCL_UNUSED(Colormap),		/* Colormap. */
    TCL_UNUSED(unsigned long *),	/* Array of pixels. */
    TCL_UNUSED(int),		/* Number of pixels. */
    TCL_UNUSED(unsigned long))	/* Number of pixel planes. */
{
    /*
     * Nothing needs to be done to release colors as there really is no
     * colormap in the Tk sense.
     */
    return Success;
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
