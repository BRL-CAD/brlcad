/*
 * tkMacOSXBitmap.c --
 *
 *	This file handles the implementation of native bitmaps.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"

/*
 * This structure holds information about native bitmaps.
 */

typedef struct {
    const char *name;		/* Name of icon. */
    OSType iconType;		/* OSType of icon. */
} BuiltInIcon;

/*
 * This array mapps a string name to the supported builtin icons
 * on the Macintosh.
 */

static BuiltInIcon builtInIcons[] = {
    {"document",	kGenericDocumentIcon},
    {"stationery",	kGenericStationeryIcon},
    {"edition",		kGenericEditionFileIcon},
    {"application",	kGenericApplicationIcon},
    {"accessory",	kGenericDeskAccessoryIcon},
    {"folder",		kGenericFolderIcon},
    {"pfolder",		kPrivateFolderIcon},
    {"trash",		kTrashIcon},
    {"floppy",		kGenericFloppyIcon},
    {"ramdisk",		kGenericRAMDiskIcon},
    {"cdrom",		kGenericCDROMIcon},
    {"preferences",	kGenericPreferencesIcon},
    {"querydoc",	kGenericQueryDocumentIcon},
    {"stop",		kAlertStopIcon},
    {"note",		kAlertNoteIcon},
    {"caution",		kAlertCautionIcon},
    {NULL}
};

#define builtInIconSize 32

static Tcl_HashTable iconBitmapTable = {};
typedef struct {
    int kind, width, height;
    char *value;
} IconBitmap;

static const char *iconBitmapOptionStrings[] = {
    "-file", "-fileType", "-osType", "-systemType", "-namedImage",
    "-imageFile", NULL
};
enum iconBitmapOptions {
    ICON_FILE, ICON_FILETYPE, ICON_OSTYPE, ICON_SYSTEMTYPE, ICON_NAMEDIMAGE,
    ICON_IMAGEFILE,
};


/*
 *----------------------------------------------------------------------
 *
 * TkpDefineNativeBitmaps --
 *
 *	Add native bitmaps.
 *
 * Results:
 *	A standard Tcl result. If an error occurs then TCL_ERROR is
 *	returned and a message is left in the interp's result.
 *
 * Side effects:
 *	"Name" is entered into the bitmap table and may be used from
 *	here on to refer to the given bitmap.
 *
 *----------------------------------------------------------------------
 */

void
TkpDefineNativeBitmaps(void)
{
    Tcl_HashTable *tablePtr = TkGetBitmapPredefTable();
    BuiltInIcon *builtInPtr;

    for (builtInPtr = builtInIcons; builtInPtr->name != NULL; builtInPtr++) {
	Tcl_HashEntry *predefHashPtr;
	Tk_Uid name;
	int isNew;

	name = Tk_GetUid(builtInPtr->name);
	predefHashPtr = Tcl_CreateHashEntry(tablePtr, name, &isNew);
	if (isNew) {
	    TkPredefBitmap *predefPtr = (TkPredefBitmap *)
		    ckalloc(sizeof(TkPredefBitmap));
	    predefPtr->source = UINT2PTR(builtInPtr->iconType);
	    predefPtr->width = builtInIconSize;
	    predefPtr->height = builtInIconSize;
	    predefPtr->native = 1;
	    Tcl_SetHashValue(predefHashPtr, predefPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetBitmapForIcon --
 *
 * Results:
 *	Bitmap for the given IconRef.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Pixmap
GetBitmapForIcon(
    Display *display,
    IconRef icon,
    CGSize size)
{
    TkMacOSXDrawingContext dc;
    Pixmap pixmap;

    pixmap = Tk_GetPixmap(display, None, size.width, size.height, 0);
    if (TkMacOSXSetupDrawingContext(pixmap, NULL, 1, &dc)) {
	if (dc.context) {
	    const CGAffineTransform t = { .a = 1, .b = 0, .c = 0, .d = -1,
		    .tx = 0, .ty = size.height };
	    const CGRect r = { .origin = { .x = 0, .y = 0 }, .size = size };

	    CGContextConcatCTM(dc.context, t);
	    PlotIconRefInContext(dc.context, &r, kAlignAbsoluteCenter,
		    kTransformNone, NULL, kPlotIconRefNormalFlags, icon);
	}
	TkMacOSXRestoreDrawingContext(&dc);
    }
    return pixmap;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpCreateNativeBitmap --
 *
 *	Create native bitmap.
 *
 * Results:
 *	Native bitmap.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Pixmap
TkpCreateNativeBitmap(
    Display *display,
    const char *source)		/* Info about the icon to build. */
{
    Pixmap pixmap;
    IconRef icon;
    OSErr err;

    err = ChkErr(GetIconRef, kOnSystemDisk, kSystemIconsCreator,
	    PTR2UINT(source), &icon);
    if (err == noErr) {
	pixmap = GetBitmapForIcon(display, icon, CGSizeMake(builtInIconSize,
		builtInIconSize));
	ReleaseIconRef(icon);
    } else {
	pixmap = Tk_GetPixmap(display, None, builtInIconSize,
		builtInIconSize, 0);
    }
    return pixmap;
}

/*
 *----------------------------------------------------------------------
 *
 * OSTypeFromString --
 *
 *	Helper to convert string to OSType.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	t is set to OSType if conversion successful.
 *
 *----------------------------------------------------------------------
 */

static int
OSTypeFromString(const char *s, OSType *t) {
    int result = TCL_ERROR;
    Tcl_DString ds;
    Tcl_Encoding encoding = Tcl_GetEncoding(NULL, "macRoman");

    Tcl_UtfToExternalDString(encoding, s, -1, &ds);
    if (Tcl_DStringLength(&ds) <= 4) {
	char string[4] = {};
	memcpy(string, Tcl_DStringValue(&ds), (size_t) Tcl_DStringLength(&ds));
	*t = (OSType) string[0] << 24 | (OSType) string[1] << 16 |
	     (OSType) string[2] <<  8 | (OSType) string[3];
	result = TCL_OK;
    }
    Tcl_DStringFree(&ds);
    Tcl_FreeEncoding(encoding);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpGetNativeAppBitmap --
 *
 *	Get a named native bitmap.
 *
 *	Attemps to interpret the given name in order as:
 *	    - name defined by ::tk::mac::iconBitmap
 *	    - NSImage named image name
 *	    - NSImage url string
 *	    - 4-char OSType of IconServices icon
 *
 * Results:
 *	Native bitmap or None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Pixmap
TkpGetNativeAppBitmap(
    Display *display,		/* The display. */
    const char *name,		/* The name of the bitmap. */
    int *width,			/* The width & height of the bitmap. */
    int *height)
{
    Tcl_HashEntry *hPtr;
    Pixmap pixmap = None;
    NSString *string;
    NSImage *image = nil;
    NSSize size = { .width = builtInIconSize, .height = builtInIconSize };

    if (iconBitmapTable.buckets &&
	    (hPtr = Tcl_FindHashEntry(&iconBitmapTable, name))) {
	OSType type;
	IconBitmap *iconBitmap = Tcl_GetHashValue(hPtr);
	name = NULL;
	size = NSMakeSize(iconBitmap->width, iconBitmap->height);
	switch (iconBitmap->kind) {
	case ICON_FILE:
	    string = [[NSString stringWithUTF8String:iconBitmap->value]
		    stringByExpandingTildeInPath];
	    image = [[NSWorkspace sharedWorkspace] iconForFile:string];
	    break;
	case ICON_FILETYPE:
	    string = [NSString stringWithUTF8String:iconBitmap->value];
	    image = [[NSWorkspace sharedWorkspace] iconForFileType:string];
	    break;
	case ICON_OSTYPE:
	    if (OSTypeFromString(iconBitmap->value, &type) == TCL_OK) {
		string = NSFileTypeForHFSTypeCode(type);
		image = [[NSWorkspace sharedWorkspace] iconForFileType:string];
	    }
	    break;
	case ICON_SYSTEMTYPE:
	    name = iconBitmap->value;
	    break;
	case ICON_NAMEDIMAGE:
	    string = [NSString stringWithUTF8String:iconBitmap->value];
	    image = [NSImage imageNamed:string];
	    break;
	case ICON_IMAGEFILE:
	    string = [[NSString stringWithUTF8String:iconBitmap->value]
		    stringByExpandingTildeInPath];
	    image = [[[NSImage alloc] initWithContentsOfFile:string]
		    autorelease];
	    break;
	}
	if (image) {
	    [image setSize:size];
	}
    } else {
	string = [NSString stringWithUTF8String:name];
	image = [NSImage imageNamed:string];
	if (!image) {
	    NSURL *url = [NSURL URLWithString:string];
	    if (url) {
		image = [[[NSImage alloc] initWithContentsOfURL:url]
			autorelease];
	    }
	}
	if (image) {
	    size = [image size];
	}
    }
    if (image) {
	TkMacOSXDrawingContext dc;
	int depth = 0;

#ifdef MAC_OSX_TK_TODO
	for (NSImageRep *r in [image representations]) {
	    NSInteger bitsPerSample = [r bitsPerSample];
	    if (bitsPerSample && bitsPerSample > depth) {
		depth = bitsPerSample;
	    };
	}
	if (depth == 1) {
	    /* TODO: convert BW NSImage to CGImageMask */
	}
#endif
	pixmap = Tk_GetPixmap(display, None, size.width, size.height, depth);
	*width = size.width;
	*height = size.height;
	if (TkMacOSXSetupDrawingContext(pixmap, NULL, 1, &dc)) {
	    if (dc.context) {
		CGAffineTransform t = { .a = 1, .b = 0, .c = 0, .d = -1,
			.tx = 0, .ty = size.height};

		CGContextConcatCTM(dc.context, t);
		[NSGraphicsContext saveGraphicsState];
		[NSGraphicsContext setCurrentContext:[NSGraphicsContext
			graphicsContextWithGraphicsPort:dc.context flipped:NO]];
		[image drawAtPoint:NSZeroPoint fromRect:NSZeroRect
			operation:NSCompositeCopy fraction:1.0];
		[NSGraphicsContext restoreGraphicsState];
	    }
	    TkMacOSXRestoreDrawingContext(&dc);
	}
    } else if (name) {
	OSType iconType;
	if (OSTypeFromString(name, &iconType) == TCL_OK) {
	    IconRef icon;
	    OSErr err = ChkErr(GetIconRef, kOnSystemDisk, kSystemIconsCreator,
		    iconType, &icon);
	    if (err == noErr) {
		pixmap = GetBitmapForIcon(display, icon, NSSizeToCGSize(size));
		*width = size.width;
		*height = size.height;
		ReleaseIconRef(icon);
	    }
	}
    }
    return pixmap;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXIconBitmapObjCmd --
 *
 *	Implements the ::tk::mac::iconBitmap command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	none
 *
 *----------------------------------------------------------------------
 */

int
TkMacOSXIconBitmapObjCmd(
    ClientData clientData,	/* Unused. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_HashEntry *hPtr;
    int i = 1, len, isNew, result = TCL_ERROR;
    const char *name, *value;
    IconBitmap ib, *iconBitmap;

    if (objc != 6) {
	Tcl_WrongNumArgs(interp, 1, objv, "name width height "
		"-file|-fileType|-osType|-systemType|-namedImage|-imageFile "
		"value");
	goto end;
    }
    name = Tcl_GetStringFromObj(objv[i++], &len);
    if (!len) {
	Tcl_AppendResult(interp, "empty bitmap name", NULL);
	goto end;
    }
    if (Tcl_GetIntFromObj(interp, objv[i++], &ib.width) != TCL_OK) {
	goto end;
    }
    if (Tcl_GetIntFromObj(interp, objv[i++], &ib.height) != TCL_OK) {
	goto end;
    }
    if (Tcl_GetIndexFromObj(interp, objv[i++], iconBitmapOptionStrings,
	    "kind", TCL_EXACT, &ib.kind) != TCL_OK) {
	goto end;
    }
    value = Tcl_GetStringFromObj(objv[i++], &len);
    if (!len) {
	Tcl_AppendResult(interp, "empty bitmap value", NULL);
	goto end;
    }
#if 0
    if ((kind == ICON_TYPE || kind == ICON_SYSTEM)) {
	Tcl_DString ds;
 	Tcl_Encoding encoding = Tcl_GetEncoding(NULL, "macRoman");
	Tcl_UtfToExternalDString(encoding, value, -1, &ds);
	len = Tcl_DStringLength(&ds);
	Tcl_DStringFree(&ds);
	Tcl_FreeEncoding(encoding);
	if (len > 4) {
	    Tcl_AppendResult(interp, "invalid bitmap value", NULL);
	    goto end;
	}
    }
#endif
    ib.value = ckalloc(len + 1);
    strcpy(ib.value, value);
    if (!iconBitmapTable.buckets) {
	Tcl_InitHashTable(&iconBitmapTable, TCL_STRING_KEYS);
    }
    hPtr = Tcl_CreateHashEntry(&iconBitmapTable, name, &isNew);
    if (!isNew) {
	iconBitmap = Tcl_GetHashValue(hPtr);
	ckfree(iconBitmap->value);
    } else {
	iconBitmap = (IconBitmap *) ckalloc(sizeof(IconBitmap));
	Tcl_SetHashValue(hPtr, iconBitmap);
    }
    *iconBitmap = ib;
    result = TCL_OK;
end:
    return result;
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
