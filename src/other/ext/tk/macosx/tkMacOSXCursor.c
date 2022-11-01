/*
 * tkMacOSXCursor.c --
 *
 *	This file contains Macintosh specific cursor related routines.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXCursors.h"
#include "tkMacOSXXCursors.h"

/*
 * Mac Cursor Types.
 */

#define NONE		-1	/* Hidden cursor */
#define SELECTOR	1	/* NSCursor class method */
#define IMAGENAMED	2	/* Named NSImage */
#define IMAGEPATH	3	/* Path to NSImage */
#define IMAGEBITMAP	4	/* Pointer to 16x16 cursor bitmap data */

#define pix		16	/* Pixel width & height of cursor bitmap data */

/*
 * The following data structure contains the system specific data necessary to
 * control Windows cursors.
 */

typedef struct {
    TkCursor info;		/* Generic cursor info used by tkCursor.c */
    NSCursor *macCursor;	/* Macintosh cursor */
    int type;			/* Type of Mac cursor */
} TkMacOSXCursor;

/*
 * The table below is used to map from the name of a predefined cursor
 * to a NSCursor.
 */

struct CursorName {
    const char *name;
    const int kind;
    id id1, id2;
    NSPoint hotspot;
};

#define MacCursorData(n)	((id)tkMacOSXCursors[TK_MAC_CURSOR_##n])
#define MacXCursorData(n)	((id)tkMacOSXXCursors[TK_MAC_XCURSOR_##n])

static const struct CursorName cursorNames[] = {
    {"none",			NONE,	    nil, nil, {0, 0}},
    {"arrow",			SELECTOR,    @"arrowCursor", nil, {0, 0}},
    {"top_left_arrow",		SELECTOR,    @"arrowCursor", nil, {0, 0}},
    {"left_ptr",		SELECTOR,    @"arrowCursor", nil, {0, 0}},
    {"copyarrow",		SELECTOR,    @"dragCopyCursor", @"_copyDragCursor", {0, 0}},
    {"aliasarrow",		SELECTOR,    @"dragLinkCursor", @"_linkDragCursor", {0, 0}},
    {"contextualmenuarrow",	SELECTOR,    @"contextualMenuCursor", nil, {0, 0}},
    {"movearrow",		SELECTOR,    @"_moveCursor", nil, {0, 0}},
    {"ibeam",			SELECTOR,    @"IBeamCursor", nil, {0, 0}},
    {"text",			SELECTOR,    @"IBeamCursor", nil, {0, 0}},
    {"xterm",			SELECTOR,    @"IBeamCursor", nil, {0, 0}},
    {"cross",			SELECTOR,    @"crosshairCursor", nil, {0, 0}},
    {"crosshair",		SELECTOR,    @"crosshairCursor", nil, {0, 0}},
    {"cross-hair",		SELECTOR,    @"crosshairCursor", nil, {0, 0}},
    {"tcross",			SELECTOR,    @"crosshairCursor", nil, {0, 0}},
    {"hand",			SELECTOR,    @"openHandCursor", nil, {0, 0}},
    {"openhand",		SELECTOR,    @"openHandCursor", nil, {0, 0}},
    {"closedhand",		SELECTOR,    @"closedHandCursor", nil, {0, 0}},
    {"fist",			SELECTOR,    @"closedHandCursor", nil, {0, 0}},
    {"pointinghand",		SELECTOR,    @"pointingHandCursor", nil, {0, 0}},
    {"resize",			SELECTOR,    @"arrowCursor", nil, {0, 0}},
    {"resizeleft",		SELECTOR,    @"resizeLeftCursor", nil, {0, 0}},
    {"resizeright",		SELECTOR,    @"resizeRightCursor", nil, {0, 0}},
    {"resizeleftright",		SELECTOR,    @"resizeLeftRightCursor", nil, {0, 0}},
    {"resizeup",		SELECTOR,    @"resizeUpCursor", nil, {0, 0}},
    {"resizedown",		SELECTOR,    @"resizeDownCursor", nil, {0, 0}},
    {"resizeupdown",		SELECTOR,    @"resizeUpDownCursor", nil, {0, 0}},
    {"resizebottomleft",	SELECTOR,    @"_bottomLeftResizeCursor", nil, {0, 0}},
    {"resizetopleft",		SELECTOR,    @"_topLeftResizeCursor", nil, {0, 0}},
    {"resizebottomright",	SELECTOR,    @"_bottomRightResizeCursor", nil, {0, 0}},
    {"resizetopright",		SELECTOR,    @"_topRightResizeCursor", nil, {0, 0}},
    {"notallowed",		SELECTOR,    @"operationNotAllowedCursor", nil, {0, 0}},
    {"poof",			SELECTOR,    @"disappearingItemCursor", nil, {0, 0}},
    {"wait",			SELECTOR,    @"busyButClickableCursor", nil, {0, 0}},
    {"spinning",		SELECTOR,    @"busyButClickableCursor", nil, {0, 0}},
    {"countinguphand",		SELECTOR,    @"busyButClickableCursor", nil, {0, 0}},
    {"countingdownhand",	SELECTOR,    @"busyButClickableCursor", nil, {0, 0}},
    {"countingupanddownhand",	SELECTOR,    @"busyButClickableCursor", nil, {0, 0}},
    {"help",			IMAGENAMED,  @"NSHelpCursor", nil, {8, 8}},
//  {"hand",			IMAGEBITMAP, MacCursorData(hand), nil, {0, 0}},
    {"bucket",			IMAGEBITMAP, MacCursorData(bucket), nil, {0, 0}},
    {"cancel",			IMAGEBITMAP, MacCursorData(cancel), nil, {0, 0}},
//  {"resize",			IMAGEBITMAP, MacCursorData(resize), nil, {0, 0}},
    {"eyedrop",			IMAGEBITMAP, MacCursorData(eyedrop), nil, {0, 0}},
    {"eyedrop-full",		IMAGEBITMAP, MacCursorData(eyedrop_full), nil, {0, 0}},
    {"zoom-in",			IMAGEBITMAP, MacCursorData(zoom_in), nil, {0, 0}},
    {"zoom-out",		IMAGEBITMAP, MacCursorData(zoom_out), nil, {0, 0}},
    {"X_cursor",		IMAGEBITMAP, MacXCursorData(X_cursor), nil, {0, 0}},
//  {"arrow",			IMAGEBITMAP, MacXCursorData(arrow), nil, {0, 0}},
    {"based_arrow_down",	IMAGEBITMAP, MacXCursorData(based_arrow_down), nil, {0, 0}},
    {"based_arrow_up",		IMAGEBITMAP, MacXCursorData(based_arrow_up), nil, {0, 0}},
    {"boat",			IMAGEBITMAP, MacXCursorData(boat), nil, {0, 0}},
    {"bogosity",		IMAGEBITMAP, MacXCursorData(bogosity), nil, {0, 0}},
    {"bottom_left_corner",	IMAGEBITMAP, MacXCursorData(bottom_left_corner), nil, {0, 0}},
    {"bottom_right_corner",	IMAGEBITMAP, MacXCursorData(bottom_right_corner), nil, {0, 0}},
    {"bottom_side",		IMAGEBITMAP, MacXCursorData(bottom_side), nil, {0, 0}},
    {"bottom_tee",		IMAGEBITMAP, MacXCursorData(bottom_tee), nil, {0, 0}},
    {"box_spiral",		IMAGEBITMAP, MacXCursorData(box_spiral), nil, {0, 0}},
    {"center_ptr",		IMAGEBITMAP, MacXCursorData(center_ptr), nil, {0, 0}},
    {"circle",			IMAGEBITMAP, MacXCursorData(circle), nil, {0, 0}},
    {"clock",			IMAGEBITMAP, MacXCursorData(clock), nil, {0, 0}},
    {"coffee_mug",		IMAGEBITMAP, MacXCursorData(coffee_mug), nil, {0, 0}},
//  {"cross",			IMAGEBITMAP, MacXCursorData(cross), nil, {0, 0}},
    {"cross_reverse",		IMAGEBITMAP, MacXCursorData(cross_reverse), nil, {0, 0}},
//  {"crosshair",		IMAGEBITMAP, MacXCursorData(crosshair), nil, {0, 0}},
    {"diamond_cross",		IMAGEBITMAP, MacXCursorData(diamond_cross), nil, {0, 0}},
    {"dot",			IMAGEBITMAP, MacXCursorData(dot), nil, {0, 0}},
    {"dotbox",			IMAGEBITMAP, MacXCursorData(dotbox), nil, {0, 0}},
    {"double_arrow",		IMAGEBITMAP, MacXCursorData(double_arrow), nil, {0, 0}},
    {"draft_large",		IMAGEBITMAP, MacXCursorData(draft_large), nil, {0, 0}},
    {"draft_small",		IMAGEBITMAP, MacXCursorData(draft_small), nil, {0, 0}},
    {"draped_box",		IMAGEBITMAP, MacXCursorData(draped_box), nil, {0, 0}},
    {"exchange",		IMAGEBITMAP, MacXCursorData(exchange), nil, {0, 0}},
    {"fleur",			IMAGEBITMAP, MacXCursorData(fleur), nil, {0, 0}},
    {"gobbler",			IMAGEBITMAP, MacXCursorData(gobbler), nil, {0, 0}},
    {"gumby",			IMAGEBITMAP, MacXCursorData(gumby), nil, {0, 0}},
    {"hand1",			IMAGEBITMAP, MacXCursorData(hand1), nil, {0, 0}},
    {"hand2",			IMAGEBITMAP, MacXCursorData(hand2), nil, {0, 0}},
    {"heart",			IMAGEBITMAP, MacXCursorData(heart), nil, {0, 0}},
    {"icon",			IMAGEBITMAP, MacXCursorData(icon), nil, {0, 0}},
    {"iron_cross",		IMAGEBITMAP, MacXCursorData(iron_cross), nil, {0, 0}},
//  {"left_ptr",		IMAGEBITMAP, MacXCursorData(left_ptr), nil, {0, 0}},
    {"left_side",		IMAGEBITMAP, MacXCursorData(left_side), nil, {0, 0}},
    {"left_tee",		IMAGEBITMAP, MacXCursorData(left_tee), nil, {0, 0}},
    {"leftbutton",		IMAGEBITMAP, MacXCursorData(leftbutton), nil, {0, 0}},
    {"ll_angle",		IMAGEBITMAP, MacXCursorData(ll_angle), nil, {0, 0}},
    {"lr_angle",		IMAGEBITMAP, MacXCursorData(lr_angle), nil, {0, 0}},
    {"man",			IMAGEBITMAP, MacXCursorData(man), nil, {0, 0}},
    {"middlebutton",		IMAGEBITMAP, MacXCursorData(middlebutton), nil, {0, 0}},
    {"mouse",			IMAGEBITMAP, MacXCursorData(mouse), nil, {0, 0}},
    {"pencil",			IMAGEBITMAP, MacXCursorData(pencil), nil, {0, 0}},
    {"pirate",			IMAGEBITMAP, MacXCursorData(pirate), nil, {0, 0}},
    {"plus",			IMAGEBITMAP, MacXCursorData(plus), nil, {0, 0}},
    {"question_arrow",		IMAGEBITMAP, MacXCursorData(question_arrow), nil, {0, 0}},
    {"right_ptr",		IMAGEBITMAP, MacXCursorData(right_ptr), nil, {0, 0}},
    {"right_side",		IMAGEBITMAP, MacXCursorData(right_side), nil, {0, 0}},
    {"right_tee",		IMAGEBITMAP, MacXCursorData(right_tee), nil, {0, 0}},
    {"rightbutton",		IMAGEBITMAP, MacXCursorData(rightbutton), nil, {0, 0}},
    {"rtl_logo",		IMAGEBITMAP, MacXCursorData(rtl_logo), nil, {0, 0}},
    {"sailboat",		IMAGEBITMAP, MacXCursorData(sailboat), nil, {0, 0}},
    {"sb_down_arrow",		IMAGEBITMAP, MacXCursorData(sb_down_arrow), nil, {0, 0}},
    {"sb_h_double_arrow",	IMAGEBITMAP, MacXCursorData(sb_h_double_arrow), nil, {0, 0}},
    {"sb_left_arrow",		IMAGEBITMAP, MacXCursorData(sb_left_arrow), nil, {0, 0}},
    {"sb_right_arrow",		IMAGEBITMAP, MacXCursorData(sb_right_arrow), nil, {0, 0}},
    {"sb_up_arrow",		IMAGEBITMAP, MacXCursorData(sb_up_arrow), nil, {0, 0}},
    {"sb_v_double_arrow",	IMAGEBITMAP, MacXCursorData(sb_v_double_arrow), nil, {0, 0}},
    {"shuttle",			IMAGEBITMAP, MacXCursorData(shuttle), nil, {0, 0}},
    {"sizing",			IMAGEBITMAP, MacXCursorData(sizing), nil, {0, 0}},
    {"spider",			IMAGEBITMAP, MacXCursorData(spider), nil, {0, 0}},
    {"spraycan",		IMAGEBITMAP, MacXCursorData(spraycan), nil, {0, 0}},
    {"star",			IMAGEBITMAP, MacXCursorData(star), nil, {0, 0}},
    {"target",			IMAGEBITMAP, MacXCursorData(target), nil, {0, 0}},
//  {"tcross",			IMAGEBITMAP, MacXCursorData(tcross), nil, {0, 0}},
//  {"top_left_arrow",		IMAGEBITMAP, MacXCursorData(top_left_arrow), nil, {0, 0}},
    {"top_left_corner",		IMAGEBITMAP, MacXCursorData(top_left_corner), nil, {0, 0}},
    {"top_right_corner",	IMAGEBITMAP, MacXCursorData(top_right_corner), nil, {0, 0}},
    {"top_side",		IMAGEBITMAP, MacXCursorData(top_side), nil, {0, 0}},
    {"top_tee",			IMAGEBITMAP, MacXCursorData(top_tee), nil, {0, 0}},
    {"trek",			IMAGEBITMAP, MacXCursorData(trek), nil, {0, 0}},
    {"ul_angle",		IMAGEBITMAP, MacXCursorData(ul_angle), nil, {0, 0}},
    {"umbrella",		IMAGEBITMAP, MacXCursorData(umbrella), nil, {0, 0}},
    {"ur_angle",		IMAGEBITMAP, MacXCursorData(ur_angle), nil, {0, 0}},
    {"watch",			IMAGEBITMAP, MacXCursorData(watch), nil, {0, 0}},
//  {"xterm",			IMAGEBITMAP, MacXCursorData(xterm), nil, {0, 0}},
    {NULL, 0, nil, nil, {0, 0}}
};

/*
 * Declarations of static variables used in this file.
 */

static TkMacOSXCursor *gCurrentCursor = NULL;
				/* A pointer to the current cursor. */
static int gResizeOverride = false;
				/* A boolean indicating whether we should use
				 * the resize cursor during installations. */
static int gTkOwnsCursor = true;/* A boolean indicating whether Tk owns the
				 * cursor. If not (for instance, in the case
				 * where a Tk window is embedded in another
				 * app's window, and the cursor is out of the
				 * Tk window, we will not attempt to adjust
				 * the cursor. */

/*
 * Declarations of procedures local to this file
 */

static void FindCursorByName(TkMacOSXCursor *macCursorPtr, const char *string);

/*
 *----------------------------------------------------------------------
 *
 * FindCursorByName --
 *
 *	Retrieve a system cursor by name, and fill the macCursorPtr
 *	structure. If the cursor cannot be found, the macCursor field
 *	will be nil.
 *
 * Results:
 *	Fills the macCursorPtr record.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
FindCursorByName(
    TkMacOSXCursor *macCursorPtr,
    const char *name)
{
    NSString *path = nil;
    NSImage *image = nil;
    NSPoint hotSpot = NSZeroPoint;
    int haveHotSpot = 0, result = TCL_ERROR;
    NSCursor *macCursor = nil;

    if (name[0] == '@') {
	/*
	 * System cursor of type @filename
	 */

	macCursorPtr->type = IMAGEPATH;
	path = [NSString stringWithUTF8String:&name[1]];
    } else {
	Tcl_Obj *strPtr = Tcl_NewStringObj(name, -1);
	int idx;

	result = Tcl_GetIndexFromObjStruct(NULL, strPtr, cursorNames,
		    sizeof(struct CursorName), NULL, TCL_EXACT, &idx);
	Tcl_DecrRefCount(strPtr);
	if (result == TCL_OK) {
	    macCursorPtr->type = cursorNames[idx].kind;
	    switch (cursorNames[idx].kind) {
	    case SELECTOR: {
		SEL selector = NSSelectorFromString(cursorNames[idx].id1);
		if ([NSCursor respondsToSelector:selector]) {
		    macCursor = [[NSCursor performSelector:selector] retain];
		} else if (cursorNames[idx].id2) {
		    selector = NSSelectorFromString(cursorNames[idx].id2);
		    if ([NSCursor respondsToSelector:selector]) {
			macCursor = [[NSCursor performSelector:selector] retain];
		    }
		}
		break;
	    }
	    case IMAGENAMED:
		image = [[NSImage imageNamed:cursorNames[idx].id1] retain];
		hotSpot = cursorNames[idx].hotspot;
		haveHotSpot = 1;
		break;
	    case IMAGEPATH:
		path = [NSApp tkFrameworkImagePath:cursorNames[idx].id1];
		break;
	    case IMAGEBITMAP: {
		unsigned char *bitmap = (unsigned char *)(cursorNames[idx].id1);
		NSBitmapImageRep *bitmapImageRep = nil;
		CGImageRef img = NULL, mask = NULL, maskedImg = NULL;
		static const CGFloat decodeWB[] = {1, 0};
		CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(
			kCGColorSpaceGenericGray);
		CGDataProviderRef provider = CGDataProviderCreateWithData(NULL,
			bitmap, pix*pix/8, NULL);

		if (provider) {
		    img = CGImageCreate(pix, pix, 1, 1, pix/8, colorspace,
			    kCGBitmapByteOrderDefault, provider, decodeWB, 0,
			    kCGRenderingIntentDefault);
		    CFRelease(provider);
		}
		provider = CGDataProviderCreateWithData(NULL, bitmap +
			pix*pix/8, pix*pix/8, NULL);
		if (provider) {
		    mask = CGImageMaskCreate(pix, pix, 1, 1, pix/8, provider,
			    decodeWB, 0);
		    CFRelease(provider);
		}
		if (img && mask) {
		    maskedImg = CGImageCreateWithMask(img, mask);
		}
		if (maskedImg) {
		    bitmapImageRep = [[NSBitmapImageRep alloc]
			    initWithCGImage:maskedImg];
		    CFRelease(maskedImg);
		}
		if (mask) {
		    CFRelease(mask);
		}
		if (img) {
		    CFRelease(img);
		}
		if (colorspace) {
		    CFRelease(colorspace);
		}
		if (bitmapImageRep) {
		    image = [[NSImage alloc] initWithSize:NSMakeSize(pix, pix)];
		    [image addRepresentation:bitmapImageRep];
		    [image setTemplate:YES];
		    [bitmapImageRep release];
		}

		uint16_t *hotSpotData = (uint16_t*)(bitmap + 2*pix*pix/8);
		hotSpot.y = CFSwapInt16BigToHost(*hotSpotData++);
		hotSpot.x = CFSwapInt16BigToHost(*hotSpotData);
		haveHotSpot = 1;
		break;
	    }
	    }
	}
    }
    if (path) {
	image = [[NSImage alloc] initWithContentsOfFile:path];
    }
    if (!image && !macCursor && result != TCL_OK) {
	macCursorPtr->type = IMAGENAMED;
	image = [[NSImage imageNamed:[NSString stringWithUTF8String:name]]
		retain];
	haveHotSpot = 0;
    }
    if (image) {
	if (!haveHotSpot && [[path pathExtension] isEqualToString:@"cur"]) {
	    NSData *data = [NSData dataWithContentsOfFile:path];
	    if ([data length] > 14) {
		uint16_t *hotSpotData = (uint16_t*)((char*) [data bytes] + 10);
		hotSpot.x = CFSwapInt16LittleToHost(*hotSpotData++);
		hotSpot.y = CFSwapInt16LittleToHost(*hotSpotData);
		haveHotSpot = 1;
	    }
	}
	if (!haveHotSpot) {
	    NSSize size = [image size];
	    hotSpot.x = size.width * 0.5;
	    hotSpot.y = size.height * 0.5;
	}
	hotSpot.y = -hotSpot.y;
	macCursor = [[NSCursor alloc] initWithImage:image hotSpot:hotSpot];
	[image release];
    }
    macCursorPtr->macCursor = macCursor;
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetCursorByName --
 *
 *	Retrieve a system cursor by name.
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkGetCursorByName(
    Tcl_Interp *interp,		/* Interpreter to use for error reporting. */
    TCL_UNUSED(Tk_Window),		/* Window in which cursor will be used. */
    Tk_Uid string)		/* Description of cursor. See manual entry
				 * for details on legal syntax. */
{
    TkMacOSXCursor *macCursorPtr = NULL;
    const char **argv = NULL;
    int argc;

    /*
     * All cursor names are valid lists of one element (for
     * TkX11-compatibility), even unadorned system cursor names.
     */

    if (Tcl_SplitList(interp, string, &argc, &argv) == TCL_OK) {
	if (argc) {
	    macCursorPtr = (TkMacOSXCursor *)ckalloc(sizeof(TkMacOSXCursor));
	    macCursorPtr->info.cursor = (Tk_Cursor) macCursorPtr;
	    macCursorPtr->macCursor = nil;
	    macCursorPtr->type = 0;
	    FindCursorByName(macCursorPtr, argv[0]);
	}
	ckfree(argv);
    }
    if (!macCursorPtr || (!macCursorPtr->macCursor &&
	    macCursorPtr->type != NONE)) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"bad cursor spec \"%s\"", string));
	Tcl_SetErrorCode(interp, "TK", "VALUE", "CURSOR", NULL);
	if (macCursorPtr) {
	    ckfree(macCursorPtr);
	    macCursorPtr = NULL;
	}
    }
    return (TkCursor *) macCursorPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkCreateCursorFromData --
 *
 *	Creates a cursor from the source and mask bits.
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkCreateCursorFromData(
    TCL_UNUSED(Tk_Window),		/* Window in which cursor will be used. */
    TCL_UNUSED(const char *),		/* Bitmap data for cursor shape. */
    TCL_UNUSED(const char *),		/* Bitmap data for cursor mask. */
    TCL_UNUSED(int),	/* Dimensions of cursor. */
    TCL_UNUSED(int),
    TCL_UNUSED(int),		/* Location of hot-spot in cursor. */
    TCL_UNUSED(int),
    TCL_UNUSED(XColor),		/* Foreground color for cursor. */
    TCL_UNUSED(XColor))		/* Background color for cursor. */
{
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpFreeCursor --
 *
 *	This procedure is called to release a cursor allocated by
 *	TkGetCursorByName.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor data structure is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TkpFreeCursor(
    TkCursor *cursorPtr)
{
    TkMacOSXCursor *macCursorPtr = (TkMacOSXCursor *) cursorPtr;

    [macCursorPtr->macCursor release];
    macCursorPtr->macCursor = nil;
    if (macCursorPtr == gCurrentCursor) {
	gCurrentCursor = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInstallCursor --
 *
 *	Installs either the current cursor as defined by TkpSetCursor or a
 *	resize cursor as the cursor the Macintosh should currently display.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the Macintosh mouse cursor.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXInstallCursor(
    int resizeOverride)
{
    TkMacOSXCursor *macCursorPtr = gCurrentCursor;
    static int cursorHidden = 0;
    int cursorNone = 0;

    gResizeOverride = resizeOverride;

    if (resizeOverride || !macCursorPtr) {
	[[NSCursor arrowCursor] set];
    } else {
	switch (macCursorPtr->type) {
	case NONE:
	    if (!cursorHidden) {
		cursorHidden = 1;
		[NSCursor hide];
	    }
	    cursorNone = 1;
	    break;
	case SELECTOR:
	case IMAGENAMED:
	case IMAGEPATH:
	case IMAGEBITMAP:
	default:
	    [macCursorPtr->macCursor set];
	    break;
	}
    }
    if (cursorHidden && !cursorNone) {
	cursorHidden = 0;
	[NSCursor unhide];
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpSetCursor --
 *
 *	Set the current cursor and install it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the current cursor.
 *
 *----------------------------------------------------------------------
 */

void
TkpSetCursor(
    TkpCursor cursor)
{
    int cursorChanged = 1;

    if (!gTkOwnsCursor) {
	return;
    }

    if (cursor == NULL) {
	/*
	 * This is a little tricky. We can't really tell whether
	 * gCurrentCursor is NULL because it was NULL last time around or
	 * because we just freed the current cursor. So if the input cursor is
	 * NULL, we always need to reset it, we can't trust the cursorChanged
	 * logic.
	 */

	gCurrentCursor = NULL;
    } else {
	if (gCurrentCursor == (TkMacOSXCursor *) cursor) {
	    cursorChanged = 0;
	}
	gCurrentCursor = (TkMacOSXCursor *) cursor;
    }

    if (Tk_MacOSXIsAppInFront() && cursorChanged) {
	TkMacOSXInstallCursor(gResizeOverride);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MacOSXTkOwnsCursor --
 *
 *	Sets whether Tk has the right to adjust the cursor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May keep Tk from changing the cursor.
 *
 *----------------------------------------------------------------------
 */

void
Tk_MacOSXTkOwnsCursor(
    int tkOwnsIt)
{
    gTkOwnsCursor = tkOwnsIt;
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
