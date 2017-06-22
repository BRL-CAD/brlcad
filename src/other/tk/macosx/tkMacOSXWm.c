/*
 * tkMacOSXWm.c --
 *
 *	This module takes care of the interactions between a Tk-based
 *	application and the window manager. Among other things, it implements
 *	the "wm" command and passes geometry information to the window
 *	manager.
 *
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright (c) 2010 Kevin Walzer/WordTech Communications LLC.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkScrollbar.h"
#include "tkMacOSXWm.h"
#include "tkMacOSXEvent.h"
#include "tkMacOSXDebug.h"
#include <Carbon/Carbon.h>

#define DEBUG_ZOMBIES 0

/*
#ifdef TK_MAC_DEBUG
#define TK_MAC_DEBUG_WINDOWS
#endif
*/

/*
 * Window attributes and classes
 */

#define WM_NSMASK_SHIFT 36
#define tkWindowDoesNotHideAttribute \
	((UInt64) 1 << kHIWindowBitDoesNotHide)
#define tkCanJoinAllSpacesAttribute \
	((UInt64) NSWindowCollectionBehaviorCanJoinAllSpaces << 34)
#define tkMoveToActiveSpaceAttribute \
	((UInt64) NSWindowCollectionBehaviorMoveToActiveSpace << 34)
#define tkNonactivatingPanelAttribute \
	((UInt64) NSNonactivatingPanelMask << WM_NSMASK_SHIFT)
#define tkHUDWindowAttribute \
	((UInt64) NSHUDWindowMask << WM_NSMASK_SHIFT)
#define tkAlwaysValidAttributes (kWindowNoUpdatesAttribute \
	| kWindowNoActivatesAttribute	    | kWindowHideOnSuspendAttribute \
	| kWindowHideOnFullScreenAttribute  | kWindowNoConstrainAttribute \
	| kWindowNoShadowAttribute	    | kWindowLiveResizeAttribute \
	| kWindowOpaqueForEventsAttribute   | kWindowIgnoreClicksAttribute \
	| kWindowDoesNotCycleAttribute	    | tkWindowDoesNotHideAttribute \
	| tkCanJoinAllSpacesAttribute	    | tkMoveToActiveSpaceAttribute \
	| tkNonactivatingPanelAttribute	    | tkHUDWindowAttribute)


/*Objects for use in setting background color and opacity of window.*/
NSColor *colorName = NULL;
BOOL opaqueTag = FALSE;

static const struct {
    const UInt64 validAttrs, defaultAttrs, forceOnAttrs, forceOffAttrs;
    int flags; NSUInteger styleMask;
} macClassAttrs[] = {
    [kAlertWindowClass] = {
	.defaultAttrs = kWindowDoesNotCycleAttribute, },
    [kMovableAlertWindowClass] = {
	.defaultAttrs = kWindowDoesNotCycleAttribute, },
    [kModalWindowClass] = {
	.defaultAttrs = kWindowDoesNotCycleAttribute, },
    [kMovableModalWindowClass] = {
	.validAttrs = kWindowCloseBoxAttribute | kWindowMetalAttribute |
		kWindowFullZoomAttribute | kWindowResizableAttribute,
	.defaultAttrs = kWindowDoesNotCycleAttribute, },
    [kFloatingWindowClass] = {
	.validAttrs = kWindowCloseBoxAttribute | kWindowCollapseBoxAttribute |
		kWindowMetalAttribute | kWindowToolbarButtonAttribute |
		kWindowNoTitleBarAttribute | kWindowFullZoomAttribute |
		kWindowResizableAttribute | kWindowSideTitlebarAttribute,
	.defaultAttrs = kWindowStandardFloatingAttributes |
		kWindowHideOnSuspendAttribute | kWindowDoesNotCycleAttribute,
	.forceOnAttrs = kWindowResizableAttribute,
	.forceOffAttrs = kWindowCollapseBoxAttribute,
	.styleMask = NSUtilityWindowMask, },
    [kDocumentWindowClass] = {
	.validAttrs = kWindowCloseBoxAttribute | kWindowCollapseBoxAttribute |
		kWindowMetalAttribute | kWindowToolbarButtonAttribute |
		kWindowNoTitleBarAttribute |
		kWindowUnifiedTitleAndToolbarAttribute |
		kWindowInWindowMenuAttribute | kWindowFullZoomAttribute |
		kWindowResizableAttribute,
	.forceOnAttrs = kWindowResizableAttribute,
	.defaultAttrs = kWindowStandardDocumentAttributes |
		kWindowLiveResizeAttribute | kWindowInWindowMenuAttribute, },
    [kUtilityWindowClass] = {
	.validAttrs = kWindowCloseBoxAttribute | kWindowCollapseBoxAttribute |
		kWindowMetalAttribute | kWindowToolbarButtonAttribute |
		kWindowNoTitleBarAttribute | kWindowFullZoomAttribute |
		kWindowResizableAttribute | kWindowSideTitlebarAttribute,
	.defaultAttrs = kWindowStandardFloatingAttributes |
		kWindowHideOnFullScreenAttribute |
		tkWindowDoesNotHideAttribute | tkNonactivatingPanelAttribute |
		kWindowDoesNotCycleAttribute,
	.forceOnAttrs = kWindowResizableAttribute,
	.forceOffAttrs = kWindowCollapseBoxAttribute,
	.flags = WM_TOPMOST,
	.styleMask = NSUtilityWindowMask, },
    [kHelpWindowClass] = {
	.defaultAttrs = kWindowHideOnSuspendAttribute,
	.forceOnAttrs = kWindowNoTitleBarAttribute |
		kWindowDoesNotCycleAttribute,
	.flags = WM_TOPMOST, },
    [kSheetWindowClass] = {
	.validAttrs = kWindowResizableAttribute,
	.forceOnAttrs = kWindowNoTitleBarAttribute |
		kWindowDoesNotCycleAttribute,
	.styleMask = NSDocModalWindowMask, },
    [kToolbarWindowClass] = {
	.defaultAttrs = kWindowHideOnSuspendAttribute,
	.forceOnAttrs = kWindowNoTitleBarAttribute |
		kWindowDoesNotCycleAttribute,
	.styleMask = NSUtilityWindowMask, },
    [kPlainWindowClass] = {
	.defaultAttrs = kWindowDoesNotCycleAttribute,
	.forceOnAttrs = kWindowNoTitleBarAttribute, },
    [kOverlayWindowClass] = {
	.forceOnAttrs = kWindowNoTitleBarAttribute |
		kWindowDoesNotCycleAttribute,
	.flags = WM_TOPMOST | WM_TRANSPARENT, },
    [kSheetAlertWindowClass] = {
	.forceOnAttrs = kWindowNoTitleBarAttribute |
		kWindowDoesNotCycleAttribute,
	.styleMask = NSDocModalWindowMask, },
    [kAltPlainWindowClass] = {
	.defaultAttrs = kWindowDoesNotCycleAttribute,
	.forceOnAttrs = kWindowNoTitleBarAttribute, },
    [kSimpleWindowClass] = {
	.defaultAttrs = kWindowDoesNotCycleAttribute,
	.forceOnAttrs = kWindowNoTitleBarAttribute, },
    [kDrawerWindowClass] = {
	.validAttrs = kWindowMetalAttribute | kWindowResizableAttribute,
	.forceOnAttrs = kWindowNoTitleBarAttribute |
		kWindowDoesNotCycleAttribute, },
};

#define ForceAttributes(attributes, class) \
	((attributes) & (~macClassAttrs[(class)].forceOffAttrs | \
	(macClassAttrs[(class)].forceOnAttrs & ~kWindowResizableAttribute)))

/*
 * Data for [wm attributes] command:
 */

typedef enum {
    WMATT_ALPHA, WMATT_FULLSCREEN, WMATT_MODIFIED, WMATT_NOTIFY,
    WMATT_TITLEPATH, WMATT_TOPMOST, WMATT_TRANSPARENT,
    _WMATT_LAST_ATTRIBUTE
} WmAttribute;

static const char *const WmAttributeNames[] = {
    "-alpha", "-fullscreen", "-modified", "-notify",
    "-titlepath", "-topmost", "-transparent",
    NULL
};

/*
 * The variable below is used to enable or disable tracing in this module. If
 * tracing is enabled, then information is printed on standard output about
 * interesting interactions with the window manager.
 */

static int wmTracing = 0;

/*
 * The following structure is the official type record for geometry management
 * of top-level windows.
 */

static void TopLevelReqProc(ClientData dummy, Tk_Window tkwin);

static const Tk_GeomMgr wmMgrType = {
    "wm",			/* name */
    TopLevelReqProc,		/* requestProc */
    NULL,			/* lostSlaveProc */
};

/*
 * The following keeps state for Aqua dock icon bounce notification.
 */

static int tkMacOSXWmAttrNotifyVal = 0;

/*
 * Hash table for Mac Window -> TkWindow mapping.
 */

static Tcl_HashTable windowTable;
static int windowHashInit = false;



#pragma mark NSWindow(TKWm)

/*
 * Conversion of coordinates between window and screen.
 */

@implementation NSWindow(TKWm)
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1070
- (NSPoint) convertPointToScreen: (NSPoint) point
{
    return [self convertBaseToScreen:point];
}
- (NSPoint) convertPointFromScreen: (NSPoint)point
{
    return [self convertScreenToBase:point];
}
@end
#else
- (NSPoint) convertPointToScreen: (NSPoint) point
{
    NSRect pointrect;
    pointrect.origin = point;
    pointrect.size.width = 0;
    pointrect.size.height = 0;
    return [self convertRectToScreen:pointrect].origin;
}
- (NSPoint) convertPointFromScreen: (NSPoint)point
{
    NSRect pointrect;
    pointrect.origin = point;
    pointrect.size.width = 0;
    pointrect.size.height = 0;
    return [self convertRectFromScreen:pointrect].origin;
}
@end
#endif

#pragma mark -


/*
 * Forward declarations for procedures defined in this file:
 */

static NSRect		InitialWindowBounds(TkWindow *winPtr,
			    NSWindow *macWindow);
static int		ParseGeometry(Tcl_Interp *interp, char *string,
			    TkWindow *winPtr);
static void		TopLevelEventProc(ClientData clientData,
			    XEvent *eventPtr);
static void		WmStackorderToplevelWrapperMap(TkWindow *winPtr,
			    Display *display, Tcl_HashTable *table);
static void		UpdateGeometryInfo(ClientData clientData);
static void		UpdateSizeHints(TkWindow *winPtr);
static void		UpdateVRootGeometry(WmInfo *wmPtr);
static int		WmAspectCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmAttributesCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmClientCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmColormapwindowsCmd(Tk_Window tkwin,
			    TkWindow *winPtr, Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmCommandCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmDeiconifyCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmFocusmodelCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmForgetCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmFrameCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmGeometryCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmGridCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmGroupCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmIconbitmapCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmIconifyCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmIconmaskCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmIconnameCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmIconphotoCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmIconpositionCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmIconwindowCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmManageCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmMaxsizeCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmMinsizeCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmOverrideredirectCmd(Tk_Window tkwin,
			    TkWindow *winPtr, Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmPositionfromCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmProtocolCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmResizableCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmSizefromCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmStackorderCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmStateCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmTitleCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmTransientCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		WmWithdrawCmd(Tk_Window tkwin, TkWindow *winPtr,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static void		WmUpdateGeom(WmInfo *wmPtr, TkWindow *winPtr);
static int		WmWinStyle(Tcl_Interp *interp, TkWindow *winPtr,
			    int objc, Tcl_Obj *const objv[]);
static void		ApplyWindowAttributeFlagChanges(TkWindow *winPtr,
			    NSWindow *macWindow, UInt64 oldAttributes,
			    int oldFlags, int create, int initial);
static void		ApplyMasterOverrideChanges(TkWindow *winPtr,
			    NSWindow *macWindow);
static void		GetMinSize(TkWindow *winPtr, int *minWidthPtr,
			    int *minHeightPtr);
static void		GetMaxSize(TkWindow *winPtr, int *maxWidthPtr,
			    int *maxHeightPtr);
static void		RemapWindows(TkWindow *winPtr,
			    MacDrawable *parentWin);

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
#define TK_GOT_AT_LEAST_SNOW_LEOPARD 1
#endif

#pragma mark TKWindow(TKWm)

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
@interface NSWindow(TkWm)
- (void) setCanCycle: (BOOL) canCycleFlag;
@end
#endif

@interface NSDrawerWindow : NSWindow
{
    id _i1, _i2;
}
@end

@implementation TKWindow
@end

@implementation TKWindow(TKWm)

- (BOOL) canBecomeKeyWindow
{
    TkWindow *winPtr = TkMacOSXGetTkWindow(self);

    return (winPtr && winPtr->wmInfoPtr && (winPtr->wmInfoPtr->macClass ==
	    kHelpWindowClass || winPtr->wmInfoPtr->attributes &
	    kWindowNoActivatesAttribute)) ? NO : YES;
}

#if DEBUG_ZOMBIES
- (id) retain
{
    id result = [super retain];
    const char *title = [[self title] UTF8String];
    if (title == nil) {
	title = "unnamed window";
    }
    if (DEBUG_ZOMBIES > 1){
	printf("Retained <%s>. Count is: %lu\n", title, [self retainCount]);
    }
    return result;
}

- (id) autorelease
{
    static int xcount = 0;
    id result = [super autorelease];
    const char *title = [[self title] UTF8String];
    if (title == nil) {
	title = "unnamed window";
    }
    if (DEBUG_ZOMBIES > 1){
	printf("Autoreleased <%s>. Count is %lu\n", title, [self retainCount]);
    }
    return result;
}

- (oneway void) release {
    const char *title = [[self title] UTF8String];
    if (title == nil) {
	title = "unnamed window";
    }
    if (DEBUG_ZOMBIES > 1){
	printf("Releasing <%s>. Count is %lu\n", title, [self retainCount]);
    }
    [super release];
}

- (void) dealloc {
    const char *title = [[self title] UTF8String];
    if (title == nil) {
	title = "unnamed window";
    }
    if (DEBUG_ZOMBIES > 0){
	printf(">>>> Freeing <%s>. Count is %lu\n", title, [self retainCount]);
    }
    [super dealloc];
}


#endif
@end

#pragma mark -

/*
 *----------------------------------------------------------------------
 *
 * SetWindowSizeLimits --
 *
 *	Sets NSWindow size limits
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
SetWindowSizeLimits(
    TkWindow *winPtr)
{
    NSWindow *macWindow = TkMacOSXDrawableWindow(winPtr->window);
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    int minWidth, minHeight, maxWidth, maxHeight, base;

    if (!macWindow) {
	return;
    }
    GetMinSize(winPtr, &minWidth, &minHeight);
    GetMaxSize(winPtr, &maxWidth, &maxHeight);
    if (wmPtr->gridWin) {
	base = winPtr->reqWidth - (wmPtr->reqGridWidth * wmPtr->widthInc);
	if (base < 0) {
	    base = 0;
	}
	minWidth = base + (minWidth * wmPtr->widthInc);
	maxWidth = base + (maxWidth * wmPtr->widthInc);
	base = winPtr->reqHeight - (wmPtr->reqGridHeight * wmPtr->heightInc);
	if (base < 0) {
	    base = 0;
	}
	minHeight = base + (minHeight * wmPtr->heightInc);
	maxHeight = base + (maxHeight * wmPtr->heightInc);
    }
    if (wmPtr->flags & WM_WIDTH_NOT_RESIZABLE) {
	minWidth = maxWidth = wmPtr->configWidth;
    }
    if (wmPtr->flags & WM_HEIGHT_NOT_RESIZABLE) {
	minHeight = maxHeight = wmPtr->configHeight;
    }
    if (wmPtr->gridWin) {
	[macWindow setResizeIncrements:NSMakeSize(wmPtr->widthInc,
		wmPtr->heightInc)];
    } else if (wmPtr->sizeHintsFlags & PAspect && wmPtr->minAspect.x ==
	    wmPtr->maxAspect.x && wmPtr->minAspect.y == wmPtr->maxAspect.y) {
	NSSize aspect = NSMakeSize(wmPtr->minAspect.x, wmPtr->minAspect.y);
	CGFloat ratio = aspect.width/aspect.height;
	[macWindow setContentAspectRatio:aspect];
	if ((CGFloat)minWidth/(CGFloat)minHeight > ratio) {
	    minHeight = lround(minWidth / ratio);
	} else {
	    minWidth = lround(minHeight * ratio);
	}
	if ((CGFloat)maxWidth/(CGFloat)maxHeight > ratio) {
	    maxWidth = lround(maxHeight * ratio);
	} else {
	    maxHeight = lround(maxWidth / ratio);
	}
	if ((CGFloat)wmPtr->configWidth/(CGFloat)wmPtr->configHeight > ratio) {
	    wmPtr->configWidth = lround(wmPtr->configHeight * ratio);
	    if (wmPtr->configWidth < minWidth) {
		wmPtr->configWidth = minWidth;
		wmPtr->configHeight = minHeight;
	    }
	} else {
	    wmPtr->configHeight = lround(wmPtr->configWidth / ratio);
	    if (wmPtr->configHeight < minHeight) {
		wmPtr->configWidth = minWidth;
		wmPtr->configHeight = minHeight;
	    }
	}
    } else {
	[macWindow setResizeIncrements:NSMakeSize(1.0, 1.0)];
    }
    [macWindow setContentMinSize:NSMakeSize(minWidth, minHeight)];
    [macWindow setContentMaxSize:NSMakeSize(maxWidth, maxHeight)];
}

/*
 *----------------------------------------------------------------------
 *
 * FrontWindowAtPoint --
 *
 *	Find frontmost toplevel window at a given screen location.
 *
 * Results:
 *	TkWindow*.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TkWindow*
FrontWindowAtPoint(
    int x, int y)
{
    NSPoint p = NSMakePoint(x, tkMacOSXZeroScreenHeight - y);
    NSArray *windows = [NSApp orderedWindows];
    TkWindow *front = NULL;

    for (NSWindow *w in windows) {
	    if (w && NSMouseInRect(p, [w frame], NO)) {
		front = TkMacOSXGetTkWindow(w);
		break;
	    }
	}
    return front;
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmNewWindow --
 *
 *	This procedure is invoked whenever a new top-level window is created.
 *	Its job is to initialize the WmInfo structure for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A WmInfo structure gets allocated and initialized.
 *
 *----------------------------------------------------------------------
 */

void
TkWmNewWindow(
    TkWindow *winPtr)		/* Newly-created top-level window. */
{
    WmInfo *wmPtr = (WmInfo *) ckalloc(sizeof(WmInfo));

    wmPtr->winPtr = winPtr;
    wmPtr->reparent = None;
    wmPtr->titleUid = NULL;
    wmPtr->iconName = NULL;
    wmPtr->master = None;
    wmPtr->hints.flags = InputHint | StateHint;
    wmPtr->hints.input = True;
    wmPtr->hints.initial_state = NormalState;
    wmPtr->hints.icon_pixmap = None;
    wmPtr->hints.icon_window = None;
    wmPtr->hints.icon_x = wmPtr->hints.icon_y = 0;
    wmPtr->hints.icon_mask = None;
    wmPtr->hints.window_group = None;
    wmPtr->leaderName = NULL;
    wmPtr->masterWindowName = NULL;
    wmPtr->icon = NULL;
    wmPtr->iconFor = NULL;
    wmPtr->sizeHintsFlags = 0;
    wmPtr->minWidth = wmPtr->minHeight = 1;
    wmPtr->maxWidth = 0;
    wmPtr->maxHeight = 0;
    wmPtr->gridWin = NULL;
    wmPtr->widthInc = wmPtr->heightInc = 1;
    wmPtr->minAspect.x = wmPtr->minAspect.y = 1;
    wmPtr->maxAspect.x = wmPtr->maxAspect.y = 1;
    wmPtr->reqGridWidth = wmPtr->reqGridHeight = -1;
    wmPtr->gravity = NorthWestGravity;
    wmPtr->width = -1;
    wmPtr->height = -1;
    wmPtr->x = winPtr->changes.x;
    wmPtr->y = winPtr->changes.y;
    wmPtr->parentWidth = winPtr->changes.width
	    + 2*winPtr->changes.border_width;
    wmPtr->parentHeight = winPtr->changes.height
	    + 2*winPtr->changes.border_width;
    wmPtr->xInParent = 0;
    wmPtr->yInParent = 0;
    wmPtr->cmapList = NULL;
    wmPtr->cmapCount = 0;
    wmPtr->configX = 0;
    wmPtr->configY = 0;
    wmPtr->configWidth = -1;
    wmPtr->configHeight = -1;
    wmPtr->vRoot = None;
    wmPtr->protPtr = NULL;
    wmPtr->cmdArgv = NULL;
    wmPtr->clientMachine = NULL;
    wmPtr->flags = WM_NEVER_MAPPED;
    wmPtr->macClass = kDocumentWindowClass;
    wmPtr->attributes = macClassAttrs[kDocumentWindowClass].defaultAttrs;
    wmPtr->scrollWinPtr = NULL;
    wmPtr->menuPtr = NULL;
    wmPtr->window = nil;
    winPtr->wmInfoPtr = wmPtr;

    UpdateVRootGeometry(wmPtr);


    /*
     * Tk must monitor structure events for top-level windows, in order to
     * detect size and position changes caused by window managers.
     */

    Tk_CreateEventHandler((Tk_Window) winPtr, StructureNotifyMask,
	    TopLevelEventProc, winPtr);

    /*
     * Arrange for geometry requests to be reflected from the window to the
     * window manager.
     */

    Tk_ManageGeometry((Tk_Window) winPtr, &wmMgrType, (ClientData) 0);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmMapWindow --
 *
 *	This procedure is invoked to map a top-level window. This module gets
 *	a chance to update all window-manager-related information in
 *	properties before the window manager sees the map event and checks the
 *	properties. It also gets to decide whether or not to even map the
 *	window after all.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Properties of winPtr may get updated to provide up-to-date information
 *	to the window manager. The window may also get mapped, but it may not
 *	be if this procedure decides that isn't appropriate (e.g. because the
 *	window is withdrawn).
 *
 *----------------------------------------------------------------------
 */

void
TkWmMapWindow(
    TkWindow *winPtr)		/* Top-level window that's about to be
				 * mapped. */
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;

    if (wmPtr->flags & WM_NEVER_MAPPED) {
	/*
	 * Create the underlying Mac window for this Tk window.
	 */

	if (!TkMacOSXHostToplevelExists(winPtr)) {
	    TkMacOSXMakeRealWindowExist(winPtr);
	}

	wmPtr->flags &= ~WM_NEVER_MAPPED;

	/*
	 * Generate configure event when we first map the window.
	 */

	TkGenWMConfigureEvent((Tk_Window) winPtr, wmPtr->x, wmPtr->y, -1, -1,
		TK_LOCATION_CHANGED);

	/*
	 * This is the first time this window has ever been mapped. Store all
	 * the window-manager-related information for the window.
	 */

	if (wmPtr->titleUid == NULL) {
	    wmPtr->titleUid = winPtr->nameUid;
	}

	if (!Tk_IsEmbedded(winPtr)) {
	    TkSetWMName(winPtr, wmPtr->titleUid);
	}

	TkWmSetClass(winPtr);

	if (wmPtr->iconName != NULL) {
	    XSetIconName(winPtr->display, winPtr->window, wmPtr->iconName);
	}

	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    }
    if (wmPtr->hints.initial_state == WithdrawnState) {
	return;
    }

    /*
     * TODO: we need to display a window if it's iconic on creation.
     */

    if (wmPtr->hints.initial_state == IconicState) {
	return;
    }

    /*
     * Update geometry information.
     */

    wmPtr->flags |= WM_ABOUT_TO_MAP;
    if (wmPtr->flags & WM_UPDATE_PENDING) {
	Tcl_CancelIdleCall(UpdateGeometryInfo, winPtr);
    }
    UpdateGeometryInfo(winPtr);
    wmPtr->flags &= ~WM_ABOUT_TO_MAP;

    /*
     * Map the window.
     */

    XMapWindow(winPtr->display, winPtr->window);

}

/*
 *----------------------------------------------------------------------
 *
 * TkWmUnmapWindow --
 *
 *	This procedure is invoked to unmap a top-level window. On the
 *	Macintosh all we do is call XUnmapWindow.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Unmaps the window.
 *
 *----------------------------------------------------------------------
 */

void
TkWmUnmapWindow(
    TkWindow *winPtr)		/* Top-level window that's about to be
				 * unmapped. */
{
    XUnmapWindow(winPtr->display, winPtr->window);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmDeadWindow --
 *
 *	This procedure is invoked when a top-level window is about to be
 *	deleted. It cleans up the wm-related data structures for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The WmInfo structure for winPtr gets freed up.
 *
 *----------------------------------------------------------------------
 */

void
TkWmDeadWindow(
    TkWindow *winPtr)		/* Top-level window that's being deleted. */
{
    WmInfo *wmPtr = winPtr->wmInfoPtr, *wmPtr2;

    if (wmPtr == NULL) {
	return;
    }
     Tk_ManageGeometry((Tk_Window) winPtr, NULL, NULL);
    Tk_DeleteEventHandler((Tk_Window) winPtr, StructureNotifyMask,
	    TopLevelEventProc, winPtr);
    if (wmPtr->hints.flags & IconPixmapHint) {
	Tk_FreeBitmap(winPtr->display, wmPtr->hints.icon_pixmap);
    }
    if (wmPtr->hints.flags & IconMaskHint) {
	Tk_FreeBitmap(winPtr->display, wmPtr->hints.icon_mask);
    }
    if (wmPtr->iconName != NULL) {
	ckfree(wmPtr->iconName);
    }
    if (wmPtr->leaderName != NULL) {
	ckfree(wmPtr->leaderName);
    }
    if (wmPtr->masterWindowName != NULL) {
	ckfree(wmPtr->masterWindowName);
    }
    if (wmPtr->icon != NULL) {
	wmPtr2 = ((TkWindow *) wmPtr->icon)->wmInfoPtr;
	wmPtr2->iconFor = NULL;
    }
    if (wmPtr->iconFor != NULL) {
	wmPtr2 = ((TkWindow *) wmPtr->iconFor)->wmInfoPtr;
	wmPtr2->icon = NULL;
	wmPtr2->hints.flags &= ~IconWindowHint;
    }
    while (wmPtr->protPtr != NULL) {
	ProtocolHandler *protPtr;

	protPtr = wmPtr->protPtr;
	wmPtr->protPtr = protPtr->nextPtr;
	Tcl_EventuallyFree(protPtr, TCL_DYNAMIC);
    }
    if (wmPtr->cmdArgv != NULL) {
	ckfree((char *) wmPtr->cmdArgv);
    }
    if (wmPtr->clientMachine != NULL) {
	ckfree(wmPtr->clientMachine);
    }
    if (wmPtr->flags & WM_UPDATE_PENDING) {
	Tcl_CancelIdleCall(UpdateGeometryInfo, winPtr);
    }

    /*
     * Delete the Mac window and remove it from the windowTable. The window
     * could be nil if the window was never mapped. However, we don't do this
     * for embedded windows, they don't go in the window list, and they do not
     * own their portPtr's.
     */

    NSWindow *window = wmPtr->window;

    if (window && !Tk_IsEmbedded(winPtr) ) {
	NSWindow *parent = [window parentWindow];
	if (parent) {
	    [parent removeChildWindow:window];
	}
	[window close];
	TkMacOSXUnregisterMacWindow(window);
        if (winPtr->window) {
            ((MacDrawable *) winPtr->window)->view = nil;
        }
#if DEBUG_ZOMBIES > 0
	{
	    const char *title = [[window title] UTF8String];
	    if (title == nil) {
		title = "unnamed window";
	    }
	    printf(">>>> Closing <%s>. Count is: %lu\n", title, [window retainCount]);
	}
#endif
        [window release];
	wmPtr->window = NULL;

	/* Activate the highest window left on the screen. */
	NSArray *windows = [NSApp orderedWindows];
	if ( [windows count] > 0 ) {
	    NSWindow *front = [windows objectAtIndex:0];
	    if ( front && [front canBecomeKeyWindow] ) {
		[front makeKeyAndOrderFront:NSApp];
	    }
	}
	[NSApp _resetAutoreleasePool];

#if DEBUG_ZOMBIES > 0
	fprintf(stderr, "================= Pool dump ===================\n");
	[NSAutoreleasePool showPools];
#endif
    }
    ckfree((char *)wmPtr);
    winPtr->wmInfoPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmSetClass --
 *
 *	This procedure is invoked whenever a top-level window's class is
 *	changed. If the window has been mapped then this procedure updates the
 *	window manager property for the class. If the window hasn't been
 *	mapped, the update is deferred until just before the first mapping.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A window property may get updated.
 *
 *----------------------------------------------------------------------
 */

void
TkWmSetClass(
    TkWindow *winPtr)		/* Newly-created top-level window. */
{
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_WmObjCmd --
 *
 *	This procedure is invoked to process the "wm" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
int
Tk_WmObjCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    static const char *const optionStrings[] = {
	"aspect", "attributes", "client", "colormapwindows",
	"command", "deiconify", "focusmodel", "forget",
	"frame", "geometry", "grid", "group",
	"iconbitmap", "iconify", "iconmask", "iconname",
	"iconphoto", "iconposition", "iconwindow",
	"manage", "maxsize", "minsize", "overrideredirect",
	"positionfrom", "protocol", "resizable", "sizefrom",
	"stackorder", "state", "title", "transient",
	"withdraw", NULL };
    enum options {
	WMOPT_ASPECT, WMOPT_ATTRIBUTES, WMOPT_CLIENT, WMOPT_COLORMAPWINDOWS,
	WMOPT_COMMAND, WMOPT_DEICONIFY, WMOPT_FOCUSMODEL, WMOPT_FORGET,
	WMOPT_FRAME, WMOPT_GEOMETRY, WMOPT_GRID, WMOPT_GROUP,
	WMOPT_ICONBITMAP, WMOPT_ICONIFY, WMOPT_ICONMASK, WMOPT_ICONNAME,
	WMOPT_ICONPHOTO, WMOPT_ICONPOSITION, WMOPT_ICONWINDOW,
	WMOPT_MANAGE, WMOPT_MAXSIZE, WMOPT_MINSIZE, WMOPT_OVERRIDEREDIRECT,
	WMOPT_POSITIONFROM, WMOPT_PROTOCOL, WMOPT_RESIZABLE, WMOPT_SIZEFROM,
	WMOPT_STACKORDER, WMOPT_STATE, WMOPT_TITLE, WMOPT_TRANSIENT,
	WMOPT_WITHDRAW };
    int index, length;
    char *argv1;
    TkWindow *winPtr;

    if (objc < 2) {
    wrongNumArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "option window ?arg ...?");
	return TCL_ERROR;
    }

    argv1 = Tcl_GetStringFromObj(objv[1], &length);
    if ((argv1[0] == 't') && (strncmp(argv1, "tracing", length) == 0)
	&& (length >= 3)) {
	if ((objc != 2) && (objc != 3)) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?boolean?");
	    return TCL_ERROR;
	}
	if (objc == 2) {
	    Tcl_SetResult(interp, ((wmTracing) ? "on" : "off"), TCL_STATIC);
	    return TCL_OK;
	}
	return Tcl_GetBooleanFromObj(interp, objv[2], &wmTracing);
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], optionStrings, "option", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    if (objc < 3) {
	goto wrongNumArgs;
    }

    if (TkGetWindowFromObj(interp, tkwin, objv[2], (Tk_Window *) &winPtr)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    if (!Tk_IsTopLevel(winPtr)
	    && (index != WMOPT_MANAGE) && (index != WMOPT_FORGET)) {
	Tcl_AppendResult(interp, "window \"", winPtr->pathName,
		"\" isn't a top-level window", NULL);
	return TCL_ERROR;
    }

    switch ((enum options) index) {
    case WMOPT_ASPECT:
	return WmAspectCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_ATTRIBUTES:
	return WmAttributesCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_CLIENT:
	return WmClientCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_COLORMAPWINDOWS:
	return WmColormapwindowsCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_COMMAND:
	return WmCommandCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_DEICONIFY:
	return WmDeiconifyCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_FOCUSMODEL:
	return WmFocusmodelCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_FORGET:
	return WmForgetCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_FRAME:
	return WmFrameCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_GEOMETRY:
	return WmGeometryCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_GRID:
	return WmGridCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_GROUP:
	return WmGroupCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_ICONBITMAP:
	return WmIconbitmapCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_ICONIFY:
	return WmIconifyCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_ICONMASK:
	return WmIconmaskCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_ICONNAME:
	return WmIconnameCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_ICONPHOTO:
	return WmIconphotoCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_ICONPOSITION:
	return WmIconpositionCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_ICONWINDOW:
	return WmIconwindowCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_MANAGE:
	return WmManageCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_MAXSIZE:
	return WmMaxsizeCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_MINSIZE:
	return WmMinsizeCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_OVERRIDEREDIRECT:
	return WmOverrideredirectCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_POSITIONFROM:
	return WmPositionfromCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_PROTOCOL:
	return WmProtocolCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_RESIZABLE:
	return WmResizableCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_SIZEFROM:
	return WmSizefromCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_STACKORDER:
	return WmStackorderCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_STATE:
	return WmStateCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_TITLE:
	return WmTitleCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_TRANSIENT:
	return WmTransientCmd(tkwin, winPtr, interp, objc, objv);
    case WMOPT_WITHDRAW:
	return WmWithdrawCmd(tkwin, winPtr, interp, objc, objv);
    }

    /* This should not happen */
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * WmAspectCmd --
 *
 *	This procedure is invoked to process the "wm aspect" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmAspectCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    int numer1, denom1, numer2, denom2;

    if ((objc != 3) && (objc != 7)) {
	Tcl_WrongNumArgs(interp, 2, objv,
		"window ?minNumer minDenom maxNumer maxDenom?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->sizeHintsFlags & PAspect) {
	    char buf[TCL_INTEGER_SPACE * 4];

	    sprintf(buf, "%d %d %d %d", wmPtr->minAspect.x,
		    wmPtr->minAspect.y, wmPtr->maxAspect.x,
		    wmPtr->maxAspect.y);
	    Tcl_SetResult(interp, buf, TCL_VOLATILE);
	}
	return TCL_OK;
    }
    if (*Tcl_GetString(objv[3]) == '\0') {
	wmPtr->sizeHintsFlags &= ~PAspect;
    } else {
	if ((Tcl_GetIntFromObj(interp, objv[3], &numer1) != TCL_OK)
		|| (Tcl_GetIntFromObj(interp, objv[4], &denom1) != TCL_OK)
		|| (Tcl_GetIntFromObj(interp, objv[5], &numer2) != TCL_OK)
		|| (Tcl_GetIntFromObj(interp, objv[6], &denom2) != TCL_OK)) {
	    return TCL_ERROR;
	}
	if ((numer1 <= 0) || (denom1 <= 0) || (numer2 <= 0) ||
		(denom2 <= 0)) {
	    Tcl_SetResult(interp, "aspect number can't be <= 0", TCL_STATIC);
	    return TCL_ERROR;
	}
	wmPtr->minAspect.x = numer1;
	wmPtr->minAspect.y = denom1;
	wmPtr->maxAspect.x = numer2;
	wmPtr->maxAspect.y = denom2;
	wmPtr->sizeHintsFlags |= PAspect;
    }
    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    WmUpdateGeom(wmPtr, winPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmSetAttribute --
 *
 *	Helper routine for WmAttributesCmd. Sets the value of the specified
 *	attribute.
 *
 * Returns:
 *
 *	TCL_OK if successful, TCL_ERROR otherwise. In case of an error, leaves
 *	a message in the interpreter's result.
 *
 *----------------------------------------------------------------------
 */

static int
WmSetAttribute(
    TkWindow *winPtr,		/* Toplevel to work with */
    NSWindow *macWindow,
    Tcl_Interp *interp,		/* Current interpreter */
    WmAttribute attribute,	/* Code of attribute to set */
    Tcl_Obj *value)		/* New value */
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    int boolean;

    switch (attribute) {
    case WMATT_ALPHA: {
	double dval;

	if (Tcl_GetDoubleFromObj(interp, value, &dval) != TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * The user should give (transparent) 0 .. 1.0 (opaque)
	 */

	if (dval < 0.0) {
	    dval = 0.0;
	} else if (dval > 1.0) {
	    dval = 1.0;
	}
	[macWindow setAlphaValue:dval];
	break;
    }
    case WMATT_FULLSCREEN:
	if (Tcl_GetBooleanFromObj(interp, value, &boolean) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (boolean != ((wmPtr->flags & WM_FULLSCREEN) != 0)) {
	    if (TkMacOSXMakeFullscreen(winPtr, macWindow, boolean, interp)
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	break;
    case WMATT_MODIFIED:
	if (Tcl_GetBooleanFromObj(interp, value, &boolean) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (boolean != [macWindow isDocumentEdited]) {
	    [macWindow setDocumentEdited:boolean];
	}
	break;
    case WMATT_NOTIFY:
	if (Tcl_GetBooleanFromObj(interp, value, &boolean) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (boolean == !tkMacOSXWmAttrNotifyVal) {
	    static NSInteger request = -1;

	    if (request >= 0) {
		[NSApp cancelUserAttentionRequest:request];
		request = -1;
	    }
	    if (boolean) {
		request = [NSApp requestUserAttention:NSCriticalRequest];
	    }
	    tkMacOSXWmAttrNotifyVal = boolean;
	}
	break;
    case WMATT_TITLEPATH: {
	const char *path = Tcl_FSGetNativePath(value);
	NSString *filename = @"";

	if (path && *path) {
	    filename = [NSString stringWithUTF8String:path];
	}
	[macWindow setRepresentedFilename:filename];
	break;
    }
    case WMATT_TOPMOST:
	if (Tcl_GetBooleanFromObj(interp, value, &boolean) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (boolean != ((wmPtr->flags & WM_TOPMOST) != 0)) {
	    int oldFlags = wmPtr->flags;

	    if (boolean) {
		wmPtr->flags |= WM_TOPMOST;
	    } else {
		wmPtr->flags &= ~WM_TOPMOST;
	    }
	    ApplyWindowAttributeFlagChanges(winPtr, macWindow,
		    wmPtr->attributes, oldFlags, 1, 0);
	}
	break;
    case WMATT_TRANSPARENT:
	if (Tcl_GetBooleanFromObj(interp, value, &boolean) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (boolean != ((wmPtr->flags & WM_TRANSPARENT) != 0)) {
	    UInt64 oldAttributes = wmPtr->attributes;
	    int oldFlags = wmPtr->flags;

	    if (boolean) {
		wmPtr->flags |= WM_TRANSPARENT;
		wmPtr->attributes |= kWindowNoShadowAttribute;
	    } else {
		wmPtr->flags &= ~WM_TRANSPARENT;
		wmPtr->attributes &= ~kWindowNoShadowAttribute;
	    }
	    ApplyWindowAttributeFlagChanges(winPtr, macWindow, oldAttributes,
		    oldFlags, 1, 0);
	    [macWindow setBackgroundColor:boolean ? [NSColor clearColor] : nil];
	    [macWindow setOpaque:!boolean];
	    TkMacOSXInvalidateWindow((MacDrawable *) winPtr->window,
		    TK_PARENT_WINDOW);
	    }
	break;
    case _WMATT_LAST_ATTRIBUTE:
    default:
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmGetAttribute --
 *
 *	Helper routine for WmAttributesCmd. Returns the current value of the
 *	specified attribute.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
WmGetAttribute(
    TkWindow *winPtr,		/* Toplevel to work with */
    NSWindow *macWindow,
    WmAttribute attribute)	/* Code of attribute to get */
{
    Tcl_Obj *result = NULL;
    WmInfo *wmPtr = winPtr->wmInfoPtr;

    switch (attribute) {
    case WMATT_ALPHA:
	result = Tcl_NewDoubleObj([macWindow alphaValue]);
	break;
    case WMATT_FULLSCREEN:
	result = Tcl_NewBooleanObj(wmPtr->flags & WM_FULLSCREEN);
	break;
    case WMATT_MODIFIED:
	result = Tcl_NewBooleanObj([macWindow isDocumentEdited]);
	break;
    case WMATT_NOTIFY:
	result = Tcl_NewBooleanObj(tkMacOSXWmAttrNotifyVal);
	break;
    case WMATT_TITLEPATH:
	result = Tcl_NewStringObj([[macWindow representedFilename] UTF8String],
		-1);
	break;
    case WMATT_TOPMOST:
	result = Tcl_NewBooleanObj(wmPtr->flags & WM_TOPMOST);
	break;
    case WMATT_TRANSPARENT:
	result = Tcl_NewBooleanObj(wmPtr->flags & WM_TRANSPARENT);
	break;
    case _WMATT_LAST_ATTRIBUTE:
    default:
	break;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * WmAttributesCmd --
 *
 *	This procedure is invoked to process the "wm attributes" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmAttributesCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int attribute = 0;
    NSWindow *macWindow;

    if (winPtr->window == None) {
	Tk_MakeWindowExist((Tk_Window) winPtr);
    }
    if (!TkMacOSXHostToplevelExists(winPtr)) {
	TkMacOSXMakeRealWindowExist(winPtr);
    }
    macWindow = TkMacOSXDrawableWindow(winPtr->window);

    if (objc == 3) {		/* wm attributes $win */
	Tcl_Obj *result = Tcl_NewListObj(0,0);

	for (attribute = 0; attribute < _WMATT_LAST_ATTRIBUTE; ++attribute) {
	    Tcl_ListObjAppendElement(interp, result,
		    Tcl_NewStringObj(WmAttributeNames[attribute], -1));
	    Tcl_ListObjAppendElement(interp, result,
		    WmGetAttribute(winPtr, macWindow, attribute));
	}
	Tcl_SetObjResult(interp, result);
    } else if (objc == 4)  {	/* wm attributes $win -attribute */
	if (Tcl_GetIndexFromObj(interp, objv[3], WmAttributeNames,
		    "attribute", 0, &attribute) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, WmGetAttribute(winPtr, macWindow, attribute));
    } else if ((objc - 3) % 2 == 0) {	/* wm attributes $win -att value... */
	int i;

	for (i = 3; i < objc; i += 2) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], WmAttributeNames,
			"attribute", 0, &attribute) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (WmSetAttribute(winPtr, macWindow, interp, attribute, objv[i+1])
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    } else {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?-attribute ?value ...??");
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmClientCmd --
 *
 *	This procedure is invoked to process the "wm client" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmClientCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    char *argv3;
    int length;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?name?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->clientMachine != NULL) {
	    Tcl_SetResult(interp, wmPtr->clientMachine, TCL_STATIC);
	}
	return TCL_OK;
    }
    argv3 = Tcl_GetStringFromObj(objv[3], &length);
    if (argv3[0] == 0) {
	if (wmPtr->clientMachine != NULL) {
	    ckfree(wmPtr->clientMachine);
	    wmPtr->clientMachine = NULL;
	}
	return TCL_OK;
    }
    if (wmPtr->clientMachine != NULL) {
	ckfree(wmPtr->clientMachine);
    }
    wmPtr->clientMachine = ckalloc(length + 1);
    strcpy(wmPtr->clientMachine, argv3);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmColormapwindowsCmd --
 *
 *	This procedure is invoked to process the "wm colormapwindows" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmColormapwindowsCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    TkWindow **cmapList;
    TkWindow *winPtr2;
    int i, windowObjc, gotToplevel = 0;
    Tcl_Obj **windowObjv;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?windowList?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	Tk_MakeWindowExist((Tk_Window) winPtr);
	for (i = 0; i < wmPtr->cmapCount; i++) {
	    if ((i == (wmPtr->cmapCount-1))
		&& (wmPtr->flags & WM_ADDED_TOPLEVEL_COLORMAP)) {
		break;
	    }
	    Tcl_AppendElement(interp, wmPtr->cmapList[i]->pathName);
	}
	return TCL_OK;
    }
    if (Tcl_ListObjGetElements(interp, objv[3], &windowObjc, &windowObjv)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    cmapList = (TkWindow **) ckalloc((windowObjc+1) * sizeof(TkWindow*));
    for (i = 0; i < windowObjc; i++) {
	if (TkGetWindowFromObj(interp, tkwin, windowObjv[i],
		(Tk_Window *) &winPtr2) != TCL_OK) {
	    ckfree((char *) cmapList);
	    return TCL_ERROR;
	}
	if (winPtr2 == winPtr) {
	    gotToplevel = 1;
	}
	if (winPtr2->window == None) {
	    Tk_MakeWindowExist((Tk_Window) winPtr2);
	}
	cmapList[i] = winPtr2;
    }
    if (!gotToplevel) {
	wmPtr->flags |= WM_ADDED_TOPLEVEL_COLORMAP;
	cmapList[windowObjc] = winPtr;
	windowObjc++;
    } else {
	wmPtr->flags &= ~WM_ADDED_TOPLEVEL_COLORMAP;
    }
    wmPtr->flags |= WM_COLORMAPS_EXPLICIT;
    if (wmPtr->cmapList != NULL) {
	ckfree((char *) wmPtr->cmapList);
    }
    wmPtr->cmapList = cmapList;
    wmPtr->cmapCount = windowObjc;

    /*
     * On the Macintosh all of this is just an excercise in compatability as
     * we don't support colormaps. If we did they would be installed here.
     */

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmCommandCmd --
 *
 *	This procedure is invoked to process the "wm command" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmCommandCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    char *argv3;
    int cmdArgc;
    const char **cmdArgv;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?value?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->cmdArgv != NULL) {
		argv3 = Tcl_Merge(wmPtr->cmdArgc, wmPtr->cmdArgv);
	    Tcl_SetResult(interp, argv3, TCL_VOLATILE);
	    ckfree(argv3);
	}
	return TCL_OK;
    }
    argv3 = Tcl_GetString(objv[3]);
    if (argv3[0] == 0) {
	if (wmPtr->cmdArgv != NULL) {
	    ckfree((char *) wmPtr->cmdArgv);
	    wmPtr->cmdArgv = NULL;
	}
	return TCL_OK;
    }
    if (Tcl_SplitList(interp, argv3, &cmdArgc, &cmdArgv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (wmPtr->cmdArgv != NULL) {
	ckfree((char *) wmPtr->cmdArgv);
    }
    wmPtr->cmdArgc = cmdArgc;
    wmPtr->cmdArgv = cmdArgv;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmDeiconifyCmd --
 *
 *	This procedure is invoked to process the "wm deiconify" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmDeiconifyCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "window");
	return TCL_ERROR;
    }
    if (wmPtr->iconFor != NULL) {
	Tcl_AppendResult(interp, "can't deiconify ", Tcl_GetString(objv[2]),
		": it is an icon for ", Tk_PathName(wmPtr->iconFor), NULL);
	return TCL_ERROR;
    }
    if (winPtr->flags & TK_EMBEDDED) {
	Tcl_AppendResult(interp, "can't deiconify ", winPtr->pathName,
		": it is an embedded window", NULL);
	return TCL_ERROR;
    }
    TkpWmSetState(winPtr, TkMacOSXIsWindowZoomed(winPtr) ?
	    ZoomState : NormalState);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmFocusmodelCmd --
 *
 *	This procedure is invoked to process the "wm focusmodel" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmFocusmodelCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    static const char *const optionStrings[] = {
	"active", "passive", NULL };
    enum options {
	OPT_ACTIVE, OPT_PASSIVE };
    int index;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?active|passive?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	Tcl_SetResult(interp, (wmPtr->hints.input ? "passive" : "active"),
		TCL_STATIC);
	return TCL_OK;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], optionStrings, "argument", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }
    if (index == OPT_ACTIVE) {
	wmPtr->hints.input = False;
    } else { /* OPT_PASSIVE */
	wmPtr->hints.input = True;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmForgetCmd --
 *
 *	This procedure is invoked to process the "wm forget" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmForgetCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel or Frame to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{

    register Tk_Window frameWin = (Tk_Window)winPtr;

    if (Tk_IsTopLevel(frameWin)) {

	MacDrawable *macWin;

	Tk_MakeWindowExist(frameWin);
	Tk_MakeWindowExist((Tk_Window)winPtr->parentPtr);

	macWin = (MacDrawable *) winPtr->window;

	TkFocusJoin(winPtr);
	Tk_UnmapWindow(frameWin);

	macWin->toplevel->referenceCount--;
	macWin->toplevel = winPtr->parentPtr->privatePtr->toplevel;
	macWin->toplevel->referenceCount++;
	macWin->flags &= ~TK_HOST_EXISTS;

	TkWmDeadWindow(winPtr);
	RemapWindows(winPtr, (MacDrawable *) winPtr->parentPtr->window);

	winPtr->flags &=~(TK_TOP_HIERARCHY|TK_TOP_LEVEL|TK_HAS_WRAPPER|TK_WIN_MANAGED);

	/*
	 * Flags (above) must be cleared before calling TkMapTopFrame (below).
	 */

	TkMapTopFrame(frameWin);
    } else {
    	/* Already not managed by wm - ignore it */
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmFrameCmd --
 *
 *	This procedure is invoked to process the "wm frame" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmFrameCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    Window window;
    char buf[TCL_INTEGER_SPACE];

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "window");
	return TCL_ERROR;
    }
    window = wmPtr->reparent;
    if (window == None) {
	window = Tk_WindowId((Tk_Window) winPtr);
    }
    sprintf(buf, "0x%x", (unsigned) window);
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmGeometryCmd --
 *
 *	This procedure is invoked to process the "wm geometry" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmGeometryCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    char xSign, ySign;
    int width, height;
    char *argv3;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?newGeometry?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	char buf[16 + TCL_INTEGER_SPACE * 4];

	xSign = (wmPtr->flags & WM_NEGATIVE_X) ? '-' : '+';
	ySign = (wmPtr->flags & WM_NEGATIVE_Y) ? '-' : '+';
	if (wmPtr->gridWin != NULL) {
	    width = wmPtr->reqGridWidth + (winPtr->changes.width
		    - winPtr->reqWidth)/wmPtr->widthInc;
	    height = wmPtr->reqGridHeight + (winPtr->changes.height
		    - winPtr->reqHeight)/wmPtr->heightInc;
	} else {
	    width = winPtr->changes.width;
	    height = winPtr->changes.height;
	}
	sprintf(buf, "%dx%d%c%d%c%d",
		width, height, xSign, wmPtr->x, ySign, wmPtr->y);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
	return TCL_OK;
    }
    argv3 = Tcl_GetString(objv[3]);
    if (*argv3 == '\0') {
	wmPtr->width = -1;
	wmPtr->height = -1;
	WmUpdateGeom(wmPtr, winPtr);
	return TCL_OK;
    }
    return ParseGeometry(interp, argv3, winPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * WmGridCmd --
 *
 *	This procedure is invoked to process the "wm grid" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmGridCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    int reqWidth, reqHeight, widthInc, heightInc;

    if ((objc != 3) && (objc != 7)) {
	Tcl_WrongNumArgs(interp, 2, objv,
		"window ?baseWidth baseHeight widthInc heightInc?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->sizeHintsFlags & PBaseSize) {
	    char buf[TCL_INTEGER_SPACE * 4];

	    sprintf(buf, "%d %d %d %d", wmPtr->reqGridWidth,
		    wmPtr->reqGridHeight, wmPtr->widthInc,
		    wmPtr->heightInc);
	    Tcl_SetResult(interp, buf, TCL_VOLATILE);
	}
	return TCL_OK;
    }
    if (*Tcl_GetString(objv[3]) == '\0') {
	/*
	 * Turn off gridding and reset the width and height to make sense as
	 * ungridded numbers.
	 */

	wmPtr->sizeHintsFlags &= ~(PBaseSize|PResizeInc);
	if (wmPtr->width != -1) {
	    wmPtr->width = winPtr->reqWidth + (wmPtr->width
		    - wmPtr->reqGridWidth)*wmPtr->widthInc;
	    wmPtr->height = winPtr->reqHeight + (wmPtr->height
		    - wmPtr->reqGridHeight)*wmPtr->heightInc;
	}
	wmPtr->widthInc = 1;
	wmPtr->heightInc = 1;
    } else {
	if ((Tcl_GetIntFromObj(interp, objv[3], &reqWidth) != TCL_OK)
		|| (Tcl_GetIntFromObj(interp, objv[4], &reqHeight) != TCL_OK)
		|| (Tcl_GetIntFromObj(interp, objv[5], &widthInc) != TCL_OK)
		|| (Tcl_GetIntFromObj(interp, objv[6], &heightInc)!=TCL_OK)) {
	    return TCL_ERROR;
	}
	if (reqWidth < 0) {
	    Tcl_SetResult(interp, "baseWidth can't be < 0", TCL_STATIC);
	    return TCL_ERROR;
	}
	if (reqHeight < 0) {
	    Tcl_SetResult(interp, "baseHeight can't be < 0", TCL_STATIC);
	    return TCL_ERROR;
	}
	if (widthInc <= 0) {
	    Tcl_SetResult(interp, "widthInc can't be <= 0", TCL_STATIC);
	    return TCL_ERROR;
	}
	if (heightInc <= 0) {
	    Tcl_SetResult(interp, "heightInc can't be <= 0", TCL_STATIC);
	    return TCL_ERROR;
	}
	Tk_SetGrid((Tk_Window) winPtr, reqWidth, reqHeight, widthInc,
		heightInc);
    }
    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    WmUpdateGeom(wmPtr, winPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmGroupCmd --
 *
 *	This procedure is invoked to process the "wm group" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmGroupCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    Tk_Window tkwin2;
    char *argv3;
    int length;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?pathName?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->hints.flags & WindowGroupHint) {
	    Tcl_SetResult(interp, wmPtr->leaderName, TCL_STATIC);
	}
	return TCL_OK;
    }
    argv3 = Tcl_GetStringFromObj(objv[3], &length);
    if (*argv3 == '\0') {
	wmPtr->hints.flags &= ~WindowGroupHint;
	if (wmPtr->leaderName != NULL) {
	    ckfree(wmPtr->leaderName);
	}
	wmPtr->leaderName = NULL;
    } else {
	if (TkGetWindowFromObj(interp, tkwin, objv[3], &tkwin2) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tk_MakeWindowExist(tkwin2);
	if (wmPtr->leaderName != NULL) {
	    ckfree(wmPtr->leaderName);
	}
	wmPtr->hints.window_group = Tk_WindowId(tkwin2);
	wmPtr->hints.flags |= WindowGroupHint;
	wmPtr->leaderName = ckalloc(length + 1);
	strcpy(wmPtr->leaderName, argv3);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmIconbitmapCmd --
 *
 *	This procedure is invoked to process the "wm iconbitmap" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmIconbitmapCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    Pixmap pixmap;
    char *str;
    int len;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?bitmap?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->hints.flags & IconPixmapHint) {
	    Tcl_SetResult(interp, (char*)Tk_NameOfBitmap(winPtr->display,
		    wmPtr->hints.icon_pixmap), TCL_STATIC);
	}
	return TCL_OK;
    }
    str = Tcl_GetStringFromObj(objv[3], &len);
    if (winPtr->window == None) {
	Tk_MakeWindowExist((Tk_Window) winPtr);
    }
    if (!TkMacOSXHostToplevelExists(winPtr)) {
	TkMacOSXMakeRealWindowExist(winPtr);
    }
    if (WmSetAttribute(winPtr, TkMacOSXDrawableWindow(winPtr->window), interp,
	    WMATT_TITLEPATH, objv[3]) == TCL_OK) {
	if (!len) {
	    if (wmPtr->hints.icon_pixmap != None) {
		Tk_FreeBitmap(winPtr->display, wmPtr->hints.icon_pixmap);
		wmPtr->hints.icon_pixmap = None;
	    }
	    wmPtr->hints.flags &= ~IconPixmapHint;
	}
    } else {
	pixmap = Tk_GetBitmap(interp, (Tk_Window) winPtr, Tk_GetUid(str));
	if (pixmap == None) {
	    return TCL_ERROR;
	}
	wmPtr->hints.icon_pixmap = pixmap;
	wmPtr->hints.flags |= IconPixmapHint;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmIconifyCmd --
 *
 *	This procedure is invoked to process the "wm iconify" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmIconifyCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "window");
	return TCL_ERROR;
    }
    if (Tk_Attributes((Tk_Window) winPtr)->override_redirect) {
	Tcl_AppendResult(interp, "can't iconify \"", winPtr->pathName,
		"\": override-redirect flag is set", NULL);
	return TCL_ERROR;
    }
    if (wmPtr->master != None) {
	Tcl_AppendResult(interp, "can't iconify \"", winPtr->pathName,
		"\": it is a transient", NULL);
	return TCL_ERROR;
    }
    if (wmPtr->iconFor != NULL) {
	Tcl_AppendResult(interp, "can't iconify ", winPtr->pathName,
		": it is an icon for ", Tk_PathName(wmPtr->iconFor), NULL);
	return TCL_ERROR;
    }
    if (winPtr->flags & TK_EMBEDDED) {
	Tcl_AppendResult(interp, "can't iconify ", winPtr->pathName,
		": it is an embedded window", NULL);
	return TCL_ERROR;
    }
    TkpWmSetState(winPtr, IconicState);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmIconmaskCmd --
 *
 *	This procedure is invoked to process the "wm iconmask" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmIconmaskCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    Pixmap pixmap;
    char *argv3;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?bitmap?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->hints.flags & IconMaskHint) {
	    Tcl_SetResult(interp, (char *) Tk_NameOfBitmap(winPtr->display,
		    wmPtr->hints.icon_mask), TCL_STATIC);
	}
	return TCL_OK;
    }
    argv3 = Tcl_GetString(objv[3]);
    if (*argv3 == '\0') {
	if (wmPtr->hints.icon_mask != None) {
	    Tk_FreeBitmap(winPtr->display, wmPtr->hints.icon_mask);
	}
	wmPtr->hints.flags &= ~IconMaskHint;
    } else {
	pixmap = Tk_GetBitmap(interp, tkwin, argv3);
	if (pixmap == None) {
	    return TCL_ERROR;
	}
	wmPtr->hints.icon_mask = pixmap;
	wmPtr->hints.flags |= IconMaskHint;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmIconnameCmd --
 *
 *	This procedure is invoked to process the "wm iconname" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmIconnameCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    const char *argv3;
    int length;

    if (objc > 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?newName?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->iconName != NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(wmPtr->iconName, -1));
	}
	return TCL_OK;
    }

    if (wmPtr->iconName != NULL) {
	ckfree(wmPtr->iconName);
    }
    argv3 = Tcl_GetStringFromObj(objv[3], &length);
    wmPtr->iconName = ckalloc(length + 1);
    strcpy(wmPtr->iconName, argv3);
    if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	XSetIconName(winPtr->display, winPtr->window, wmPtr->iconName);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmIconphotoCmd --
 *
 *	This procedure is invoked to process the "wm iconphoto" Tcl command.
 *	See the user documentation for details on what it does. Not yet
 *	implemented for OS X.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmIconphotoCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tk_PhotoHandle photo;
    int i, width, height, isDefault = 0;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 2, objv,
		"window ?-default? image1 ?image2 ...?");
	return TCL_ERROR;
    }
    if (strcmp(Tcl_GetString(objv[3]), "-default") == 0) {
	isDefault = 1;
	if (objc == 4) {
	    Tcl_WrongNumArgs(interp, 2, objv,
		    "window ?-default? image1 ?image2 ...?");
	    return TCL_ERROR;
	}
    }

    /*
     * Iterate over all images to retrieve their sizes, in order to allocate a
     * buffer large enough to hold all images.
     */

    for (i = 3 + isDefault; i < objc; i++) {
	photo = Tk_FindPhoto(interp, Tcl_GetString(objv[i]));
	if (photo == NULL) {
	    Tcl_AppendResult(interp, "can't use \"", Tcl_GetString(objv[i]),
		    "\" as iconphoto: not a photo image", NULL);
	    return TCL_ERROR;
	}
	Tk_PhotoGetSize(photo, &width, &height);
    }

    /*
     * TODO: This requires implementation for OS X, but we silently return for
     * now.
     */

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmIconpositionCmd --
 *
 *	This procedure is invoked to process the "wm iconposition" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmIconpositionCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    int x, y;

    if ((objc != 3) && (objc != 5)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?x y?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->hints.flags & IconPositionHint) {
	    char buf[TCL_INTEGER_SPACE * 2];

	    sprintf(buf, "%d %d", wmPtr->hints.icon_x,
		    wmPtr->hints.icon_y);
	    Tcl_SetResult(interp, buf, TCL_VOLATILE);
	}
	return TCL_OK;
    }
    if (*Tcl_GetString(objv[3]) == '\0') {
	wmPtr->hints.flags &= ~IconPositionHint;
    } else {
	if ((Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK)
	    || (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)){
	    return TCL_ERROR;
	}
	wmPtr->hints.icon_x = x;
	wmPtr->hints.icon_y = y;
	wmPtr->hints.flags |= IconPositionHint;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmIconwindowCmd --
 *
 *	This procedure is invoked to process the "wm iconwindow" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmIconwindowCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    Tk_Window tkwin2;
    WmInfo *wmPtr2;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?pathName?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->icon != NULL) {
	    Tcl_SetResult(interp, Tk_PathName(wmPtr->icon), TCL_STATIC);
	}
	return TCL_OK;
    }
    if (*Tcl_GetString(objv[3]) == '\0') {
	wmPtr->hints.flags &= ~IconWindowHint;
	if (wmPtr->icon != NULL) {
	    wmPtr2 = ((TkWindow *) wmPtr->icon)->wmInfoPtr;
	    wmPtr2->iconFor = NULL;
	    wmPtr2->hints.initial_state = WithdrawnState;
	}
	wmPtr->icon = NULL;
    } else {
	if (TkGetWindowFromObj(interp, tkwin, objv[3], &tkwin2) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Tk_IsTopLevel(tkwin2)) {
	    Tcl_AppendResult(interp, "can't use ", Tcl_GetString(objv[3]),
		    " as icon window: not at top level", NULL);
	    return TCL_ERROR;
	}
	wmPtr2 = ((TkWindow *) tkwin2)->wmInfoPtr;
	if (wmPtr2->iconFor != NULL) {
	    Tcl_AppendResult(interp, Tcl_GetString(objv[3]),
		    " is already an icon for ",
		    Tk_PathName(wmPtr2->iconFor), NULL);
	    return TCL_ERROR;
	}
	if (wmPtr->icon != NULL) {
	    WmInfo *wmPtr3 = ((TkWindow *) wmPtr->icon)->wmInfoPtr;
	    wmPtr3->iconFor = NULL;
	}
	Tk_MakeWindowExist(tkwin2);
	wmPtr->hints.icon_window = Tk_WindowId(tkwin2);
	wmPtr->hints.flags |= IconWindowHint;
	wmPtr->icon = tkwin2;
	wmPtr2->iconFor = (Tk_Window) winPtr;
	if (!(wmPtr2->flags & WM_NEVER_MAPPED)) {
	    /*
	     * Don't have iconwindows on the Mac. We just withdraw.
	     */

	    Tk_UnmapWindow(tkwin2);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmManageCmd --
 *
 *	This procedure is invoked to process the "wm manage" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmManageCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,           /* Toplevel or Frame to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{

    register Tk_Window frameWin = (Tk_Window)winPtr;
    register WmInfo *wmPtr = winPtr->wmInfoPtr;

    if (!Tk_IsTopLevel(frameWin)) {
	MacDrawable *macWin = (MacDrawable *) winPtr->window;

	if (!Tk_IsManageable(frameWin)) {
	    Tcl_AppendResult(interp, "window \"",
		Tk_PathName(frameWin), "\" is not manageable: must be "
		"a frame, labelframe or toplevel", NULL);
	    return TCL_ERROR;
	}
	TkFocusSplit(winPtr);
	Tk_UnmapWindow(frameWin);
	if (wmPtr == NULL) {
	    TkWmNewWindow(winPtr);
	    if (winPtr->window == None) {
		Tk_MakeWindowExist((Tk_Window) winPtr);
		macWin = (MacDrawable *) winPtr->window;
	    }
	    TkWmMapWindow(winPtr);
	    Tk_UnmapWindow(frameWin);
	}
	wmPtr = winPtr->wmInfoPtr;
	winPtr->flags &= ~TK_MAPPED;
	macWin->toplevel->referenceCount--;
	macWin->toplevel = macWin;
	macWin->toplevel->referenceCount++;
	RemapWindows(winPtr, macWin);
	winPtr->flags |=
		(TK_TOP_HIERARCHY|TK_TOP_LEVEL|TK_HAS_WRAPPER|TK_WIN_MANAGED);
	TkMapTopFrame(frameWin);
    } else if (Tk_IsTopLevel(frameWin)) {
	/* Already managed by wm - ignore it */
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmMaxsizeCmd --
 *
 *	This procedure is invoked to process the "wm maxsize" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmMaxsizeCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    int width, height;

    if ((objc != 3) && (objc != 5)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?width height?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	char buf[TCL_INTEGER_SPACE * 2];

	GetMaxSize(winPtr, &width, &height);
	sprintf(buf, "%d %d", width, height);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
	return TCL_OK;
    }
    if ((Tcl_GetIntFromObj(interp, objv[3], &width) != TCL_OK)
	|| (Tcl_GetIntFromObj(interp, objv[4], &height) != TCL_OK)) {
	return TCL_ERROR;
    }
    wmPtr->maxWidth = width;
    wmPtr->maxHeight = height;
    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    WmUpdateGeom(wmPtr, winPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmMinsizeCmd --
 *
 *	This procedure is invoked to process the "wm minsize" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmMinsizeCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    int width, height;

    if ((objc != 3) && (objc != 5)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?width height?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	char buf[TCL_INTEGER_SPACE * 2];

	GetMinSize(winPtr, &width, &height);
	sprintf(buf, "%d %d", width, height);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
	return TCL_OK;
    }
    if ((Tcl_GetIntFromObj(interp, objv[3], &width) != TCL_OK)
	    || (Tcl_GetIntFromObj(interp, objv[4], &height) != TCL_OK)) {
	return TCL_ERROR;
    }
    wmPtr->minWidth = width;
    wmPtr->minHeight = height;
    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    WmUpdateGeom(wmPtr, winPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmOverrideredirectCmd --
 *
 *	This procedure is invoked to process the "wm overrideredirect" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmOverrideredirectCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int boolean;
    XSetWindowAttributes atts;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?boolean?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(
		Tk_Attributes((Tk_Window) winPtr)->override_redirect));
	return TCL_OK;
    }
    if (Tcl_GetBooleanFromObj(interp, objv[3], &boolean) != TCL_OK) {
	return TCL_ERROR;
    }
    atts.override_redirect = (boolean) ? True : False;
    Tk_ChangeWindowAttributes((Tk_Window) winPtr, CWOverrideRedirect, &atts);
    ApplyMasterOverrideChanges(winPtr, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmPositionfromCmd --
 *
 *	This procedure is invoked to process the "wm positionfrom" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmPositionfromCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    static const char *const optionStrings[] = {
	"program", "user", NULL };
    enum options {
	OPT_PROGRAM, OPT_USER };
    int index;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?user/program?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->sizeHintsFlags & USPosition) {
	    Tcl_SetResult(interp, "user", TCL_STATIC);
	} else if (wmPtr->sizeHintsFlags & PPosition) {
	    Tcl_SetResult(interp, "program", TCL_STATIC);
	}
	return TCL_OK;
    }
    if (*Tcl_GetString(objv[3]) == '\0') {
	wmPtr->sizeHintsFlags &= ~(USPosition|PPosition);
    } else {
	if (Tcl_GetIndexFromObj(interp, objv[3], optionStrings, "argument", 0,
		&index) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (index == OPT_USER) {
	    wmPtr->sizeHintsFlags &= ~PPosition;
	    wmPtr->sizeHintsFlags |= USPosition;
	} else {
	    wmPtr->sizeHintsFlags &= ~USPosition;
	    wmPtr->sizeHintsFlags |= PPosition;
	}
    }
    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    WmUpdateGeom(wmPtr, winPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmProtocolCmd --
 *
 *	This procedure is invoked to process the "wm protocol" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmProtocolCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    register ProtocolHandler *protPtr, *prevPtr;
    Atom protocol;
    char *cmd;
    int cmdLength;

    if ((objc < 3) || (objc > 5)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?name? ?command?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	/*
	 * Return a list of all defined protocols for the window.
	 */

	for (protPtr = wmPtr->protPtr; protPtr != NULL;
		protPtr = protPtr->nextPtr) {
	    Tcl_AppendElement(interp,
		    Tk_GetAtomName((Tk_Window) winPtr, protPtr->protocol));
	}
	return TCL_OK;
    }
    protocol = Tk_InternAtom((Tk_Window) winPtr, Tcl_GetString(objv[3]));
    if (objc == 4) {
	/*
	 * Return the command to handle a given protocol.
	 */

	for (protPtr = wmPtr->protPtr; protPtr != NULL;
		protPtr = protPtr->nextPtr) {
	    if (protPtr->protocol == protocol) {
		Tcl_SetResult(interp, protPtr->command, TCL_STATIC);
		return TCL_OK;
	    }
	}
	return TCL_OK;
    }

    /*
     * Delete any current protocol handler, then create a new one with the
     * specified command, unless the command is empty.
     */

    for (protPtr = wmPtr->protPtr, prevPtr = NULL; protPtr != NULL;
	prevPtr = protPtr, protPtr = protPtr->nextPtr) {
	if (protPtr->protocol == protocol) {
	    if (prevPtr == NULL) {
		wmPtr->protPtr = protPtr->nextPtr;
	    } else {
		prevPtr->nextPtr = protPtr->nextPtr;
	    }
	    Tcl_EventuallyFree(protPtr, TCL_DYNAMIC);
	    break;
	}
    }
    cmd = Tcl_GetStringFromObj(objv[4], &cmdLength);
    if (cmdLength > 0) {
	protPtr = (ProtocolHandler *) ckalloc(HANDLER_SIZE(cmdLength));
	protPtr->protocol = protocol;
	protPtr->nextPtr = wmPtr->protPtr;
	wmPtr->protPtr = protPtr;
	protPtr->interp = interp;
	strcpy(protPtr->command, cmd);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmResizableCmd --
 *
 *	This procedure is invoked to process the "wm resizable" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmResizableCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    int width, height;
    UInt64 oldAttributes = wmPtr->attributes;
    int oldFlags = wmPtr->flags;

    if ((objc != 3) && (objc != 5)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?width height?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	char buf[TCL_INTEGER_SPACE * 2];

	sprintf(buf, "%d %d",
		(wmPtr->flags  & WM_WIDTH_NOT_RESIZABLE) ? 0 : 1,
		(wmPtr->flags  & WM_HEIGHT_NOT_RESIZABLE) ? 0 : 1);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
	return TCL_OK;
    }
    if ((Tcl_GetBooleanFromObj(interp, objv[3], &width) != TCL_OK)
	|| (Tcl_GetBooleanFromObj(interp, objv[4], &height) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (width) {
	wmPtr->flags &= ~WM_WIDTH_NOT_RESIZABLE;
	wmPtr->attributes |= kWindowHorizontalZoomAttribute;
    } else {
	wmPtr->flags |= WM_WIDTH_NOT_RESIZABLE;
	wmPtr->attributes &= ~kWindowHorizontalZoomAttribute;
    }
    if (height) {
	wmPtr->flags &= ~WM_HEIGHT_NOT_RESIZABLE;
	wmPtr->attributes |= kWindowVerticalZoomAttribute;
    } else {
	wmPtr->flags |= WM_HEIGHT_NOT_RESIZABLE;
	wmPtr->attributes &= ~kWindowVerticalZoomAttribute;
    }
    if (width || height) {
	wmPtr->attributes |= kWindowResizableAttribute;
    } else {
	wmPtr->attributes &= ~kWindowResizableAttribute;
    }
    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    if (wmPtr->scrollWinPtr != NULL) {
	TkScrollbarEventuallyRedraw((TkScrollbar *)
		wmPtr->scrollWinPtr->instanceData);
    }
    WmUpdateGeom(wmPtr, winPtr);
    ApplyWindowAttributeFlagChanges(winPtr, NULL, oldAttributes, oldFlags, 1,0);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmSizefromCmd --
 *
 *	This procedure is invoked to process the "wm sizefrom" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmSizefromCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    static const char *const optionStrings[] = {
	"program", "user", NULL };
    enum options {
	OPT_PROGRAM, OPT_USER };
    int index;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?user|program?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->sizeHintsFlags & USSize) {
	    Tcl_SetResult(interp, "user", TCL_STATIC);
	} else if (wmPtr->sizeHintsFlags & PSize) {
	    Tcl_SetResult(interp, "program", TCL_STATIC);
	}
	return TCL_OK;
    }

    if (*Tcl_GetString(objv[3]) == '\0') {
	wmPtr->sizeHintsFlags &= ~(USSize|PSize);
    } else {
	if (Tcl_GetIndexFromObj(interp, objv[3], optionStrings, "argument", 0,
		&index) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (index == OPT_USER) {
	    wmPtr->sizeHintsFlags &= ~PSize;
	    wmPtr->sizeHintsFlags |= USSize;
	} else { /* OPT_PROGRAM */
	    wmPtr->sizeHintsFlags &= ~USSize;
	    wmPtr->sizeHintsFlags |= PSize;
	}
    }
    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    WmUpdateGeom(wmPtr, winPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmStackorderCmd --
 *
 *	This procedure is invoked to process the "wm stackorder" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmStackorderCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    TkWindow **windows, **window_ptr;
    static const char *const optionStrings[] = {
	"isabove", "isbelow", NULL };
    enum options {
	OPT_ISABOVE, OPT_ISBELOW };
    int index;

    if ((objc != 3) && (objc != 5)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?isabove|isbelow window?");
	return TCL_ERROR;
    }

    if (objc == 3) {
	windows = TkWmStackorderToplevel(winPtr);
	if (windows == NULL) {
	    Tcl_Panic("TkWmStackorderToplevel failed");
	}

	for (window_ptr = windows; *window_ptr ; window_ptr++) {
	    Tcl_AppendElement(interp, (*window_ptr)->pathName);
	}
	ckfree((char *) windows);
	return TCL_OK;
    } else {
	TkWindow *winPtr2;
	int index1=-1, index2=-1, result;

	if (TkGetWindowFromObj(interp, tkwin, objv[4], (Tk_Window *) &winPtr2)
	    != TCL_OK) {
	    return TCL_ERROR;
	}

	if (!Tk_IsTopLevel(winPtr2)) {
	    Tcl_AppendResult(interp, "window \"", winPtr2->pathName,
		    "\" isn't a top-level window", NULL);
	    return TCL_ERROR;
	}

	if (!Tk_IsMapped(winPtr)) {
	    Tcl_AppendResult(interp, "window \"", winPtr->pathName,
		    "\" isn't mapped", NULL);
	    return TCL_ERROR;
	}

	if (!Tk_IsMapped(winPtr2)) {
	    Tcl_AppendResult(interp, "window \"", winPtr2->pathName,
		    "\" isn't mapped", NULL);
	    return TCL_ERROR;
	}

	/*
	 * Lookup stacking order of all toplevels that are children of "." and
	 * find the position of winPtr and winPtr2 in the stacking order.
	 */

	windows = TkWmStackorderToplevel(winPtr->mainPtr->winPtr);
	if (windows == NULL) {
	    Tcl_AppendResult(interp, "TkWmStackorderToplevel failed", NULL);
	    return TCL_ERROR;
	}

	for (window_ptr = windows; *window_ptr ; window_ptr++) {
	    if (*window_ptr == winPtr) {
		index1 = (window_ptr - windows);
	    }
	    if (*window_ptr == winPtr2) {
		index2 = (window_ptr - windows);
	    }
	}
	if (index1 == -1) {
	    Tcl_Panic("winPtr window not found");
	}
	if (index2 == -1) {
	    Tcl_Panic("winPtr2 window not found");
	}

	ckfree((char *) windows);

	if (Tcl_GetIndexFromObj(interp, objv[3], optionStrings, "argument", 0,
		&index) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (index == OPT_ISABOVE) {
	    result = index1 > index2;
	} else { /* OPT_ISBELOW */
	    result = index1 < index2;
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(result));
	return TCL_OK;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmStateCmd --
 *
 *	This procedure is invoked to process the "wm state" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmStateCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    static const char *const optionStrings[] = {
	"normal", "iconic", "withdrawn", "zoomed", NULL };
    enum options {
	OPT_NORMAL, OPT_ICONIC, OPT_WITHDRAWN, OPT_ZOOMED };
    int index;

    if ((objc < 3) || (objc > 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?state?");
	return TCL_ERROR;
    }
    if (objc == 4) {
	if (wmPtr->iconFor != NULL) {
	    Tcl_AppendResult(interp, "can't change state of ",
		    Tcl_GetString(objv[2]), ": it is an icon for ",
		    Tk_PathName(wmPtr->iconFor), NULL);
	    return TCL_ERROR;
	}
	if (winPtr->flags & TK_EMBEDDED) {
	    Tcl_AppendResult(interp, "can't change state of ",
		    winPtr->pathName, ": it is an embedded window", NULL);
	    return TCL_ERROR;
	}

	if (Tcl_GetIndexFromObj(interp, objv[3], optionStrings, "argument", 0,
				&index) != TCL_OK) {
	    return TCL_ERROR;
	}

	if (index == OPT_NORMAL) {
	    TkpWmSetState(winPtr, NormalState);

	    /*
	     * This varies from 'wm deiconify' because it does not force the
	     * window to be raised and receive focus
	     */
	} else if (index == OPT_ICONIC) {
	    if (Tk_Attributes((Tk_Window) winPtr)->override_redirect) {
		Tcl_AppendResult(interp, "can't iconify \"", winPtr->pathName,
			"\": override-redirect flag is set", NULL);
		return TCL_ERROR;
	    }
	    if (wmPtr->master != None) {
		Tcl_AppendResult(interp, "can't iconify \"", winPtr->pathName,
			"\": it is a transient", NULL);
		return TCL_ERROR;
	    }
	    TkpWmSetState(winPtr, IconicState);
	} else if (index == OPT_WITHDRAWN) {
	    TkpWmSetState(winPtr, WithdrawnState);
	} else { /* OPT_ZOOMED */
	    TkpWmSetState(winPtr, ZoomState);
	}
    } else if (wmPtr->iconFor != NULL) {
	Tcl_SetResult(interp, "icon", TCL_STATIC);
    } else {
	if (wmPtr->hints.initial_state == NormalState ||
		wmPtr->hints.initial_state == ZoomState) {
	    wmPtr->hints.initial_state = (TkMacOSXIsWindowZoomed(winPtr) ?
		    ZoomState : NormalState);
	}
	switch (wmPtr->hints.initial_state) {
	case NormalState:
	    Tcl_SetResult(interp, "normal", TCL_STATIC);
	    break;
	case IconicState:
	    Tcl_SetResult(interp, "iconic", TCL_STATIC);
	    break;
	case WithdrawnState:
	    Tcl_SetResult(interp, "withdrawn", TCL_STATIC);
	    break;
	case ZoomState:
	    Tcl_SetResult(interp, "zoomed", TCL_STATIC);
	    break;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmTitleCmd --
 *
 *	This procedure is invoked to process the "wm title" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmTitleCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    char *argv3;
    int length;

    if (objc > 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?newTitle?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	Tcl_SetResult(interp, (char *)((wmPtr->titleUid != NULL) ?
		wmPtr->titleUid : winPtr->nameUid), TCL_STATIC);
	return TCL_OK;
    }
    argv3 = Tcl_GetStringFromObj(objv[3], &length);
    wmPtr->titleUid = Tk_GetUid(argv3);
    if (!(wmPtr->flags & WM_NEVER_MAPPED) && !Tk_IsEmbedded(winPtr)) {
	TkSetWMName(winPtr, wmPtr->titleUid);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmTransientCmd --
 *
 *	This procedure is invoked to process the "wm transient" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmTransientCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    Tk_Window master;
    WmInfo *wmPtr2;
    char *masterWindowName;
    int length;

    if ((objc != 3) && (objc != 4)) {
	Tcl_WrongNumArgs(interp, 2, objv, "window ?master?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (wmPtr->master != None) {
	    Tcl_SetResult(interp, wmPtr->masterWindowName, TCL_STATIC);
	}
	return TCL_OK;
    }
    if (Tcl_GetString(objv[3])[0] == '\0') {
	wmPtr->master = None;
	if (wmPtr->masterWindowName != NULL) {
	    ckfree(wmPtr->masterWindowName);
	}
	wmPtr->masterWindowName = NULL;
    } else {
	if (TkGetWindowFromObj(interp, tkwin, objv[3], &master) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tk_MakeWindowExist(master);

	if (wmPtr->iconFor != NULL) {
	    Tcl_AppendResult(interp, "can't make \"", Tcl_GetString(objv[2]),
		    "\" a transient: it is an icon for ",
		    Tk_PathName(wmPtr->iconFor), NULL);
	    return TCL_ERROR;
	}

	wmPtr2 = ((TkWindow *) master)->wmInfoPtr;

	/* Under some circumstances, wmPtr2 is NULL here */
	if (wmPtr2 != NULL && wmPtr2->iconFor != NULL) {
	    Tcl_AppendResult(interp, "can't make \"", Tcl_GetString(objv[3]),
		    "\" a master: it is an icon for ",
		    Tk_PathName(wmPtr2->iconFor), NULL);
	    return TCL_ERROR;
	}

	if ((TkWindow *) master == winPtr) {
	    Tcl_AppendResult(interp, "can't make \"", Tk_PathName(winPtr),
		    "\" its own master", NULL);
	    return TCL_ERROR;
	}

	wmPtr->master = Tk_WindowId(master);
	masterWindowName = Tcl_GetStringFromObj(objv[3], &length);
	if (wmPtr->masterWindowName != NULL) {
	    ckfree(wmPtr->masterWindowName);
	}
	wmPtr->masterWindowName = ckalloc(length+1);
	strcpy(wmPtr->masterWindowName, masterWindowName);
    }
    ApplyMasterOverrideChanges(winPtr, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WmWithdrawCmd --
 *
 *	This procedure is invoked to process the "wm withdraw" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
WmWithdrawCmd(
    Tk_Window tkwin,		/* Main window of the application. */
    TkWindow *winPtr,		/* Toplevel to work with */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "window");
	return TCL_ERROR;
    }
    if (wmPtr->iconFor != NULL) {
	Tcl_AppendResult(interp, "can't withdraw ", Tcl_GetString(objv[2]),
		": it is an icon for ", Tk_PathName(wmPtr->iconFor), NULL);
	return TCL_ERROR;
    }
    TkpWmSetState(winPtr, WithdrawnState);
    return TCL_OK;
}

/*
 * Invoked by those wm subcommands that affect geometry.
 * Schedules a geometry update.
 */

static void
WmUpdateGeom(
    WmInfo *wmPtr,
    TkWindow *winPtr)
{
    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SetGrid --
 *
 *	This procedure is invoked by a widget when it wishes to set a grid
 *	coordinate system that controls the size of a top-level window. It
 *	provides a C interface equivalent to the "wm grid" command and is
 *	usually asscoiated with the -setgrid option.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Grid-related information will be passed to the window manager, so that
 *	the top-level window associated with tkwin will resize on even grid
 *	units. If some other window already controls gridding for the
 *	top-level window then this procedure call has no effect.
 *
 *----------------------------------------------------------------------
 */

void
Tk_SetGrid(
    Tk_Window tkwin,		/* Token for window. New window mgr info will
				 * be posted for the top-level window
				 * associated with this window. */
    int reqWidth,		/* Width (in grid units) corresponding to the
				 * requested geometry for tkwin. */
    int reqHeight,		/* Height (in grid units) corresponding to the
				 * requested geometry for tkwin. */
    int widthInc, int heightInc)/* Pixel increments corresponding to a change
				 * of one grid unit. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    WmInfo *wmPtr;

    /*
     * Ensure widthInc and heightInc are greater than 0
     */

    if (widthInc <= 0) {
	widthInc = 1;
    }
    if (heightInc <= 0) {
	heightInc = 1;
    }

    /*
     * Find the top-level window for tkwin, plus the window manager
     * information.
     */

    while (!(winPtr->flags & TK_TOP_LEVEL)) {
	winPtr = winPtr->parentPtr;
    }
    wmPtr = winPtr->wmInfoPtr;

    if ((wmPtr->gridWin != NULL) && (wmPtr->gridWin != tkwin)) {
	return;
    }

    if ((wmPtr->reqGridWidth == reqWidth)
	    && (wmPtr->reqGridHeight == reqHeight)
	    && (wmPtr->widthInc == widthInc)
	    && (wmPtr->heightInc == heightInc)
	    && ((wmPtr->sizeHintsFlags & (PBaseSize|PResizeInc))
		    == (PBaseSize|PResizeInc))) {
	return;
    }

    /*
     * If gridding was previously off, then forget about any window size
     * requests made by the user or via "wm geometry": these are in pixel
     * units and there's no easy way to translate them to grid units since the
     * new requested size of the top-level window in pixels may not yet have
     * been registered yet (it may filter up the hierarchy in DoWhenIdle
     * handlers). However, if the window has never been mapped yet then just
     * leave the window size alone: assume that it is intended to be in grid
     * units but just happened to have been specified before this procedure
     * was called.
     */

    if ((wmPtr->gridWin == NULL) && !(wmPtr->flags & WM_NEVER_MAPPED)) {
	wmPtr->width = -1;
	wmPtr->height = -1;
    }

    /*
     * Set the new gridding information, and start the process of passing all
     * of this information to the window manager.
     */

    wmPtr->gridWin = tkwin;
    wmPtr->reqGridWidth = reqWidth;
    wmPtr->reqGridHeight = reqHeight;
    wmPtr->widthInc = widthInc;
    wmPtr->heightInc = heightInc;
    wmPtr->sizeHintsFlags |= PBaseSize|PResizeInc;
    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_UnsetGrid --
 *
 *	This procedure cancels the effect of a previous call to Tk_SetGrid.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If tkwin currently controls gridding for its top-level window,
 *	gridding is cancelled for that top-level window; if some other window
 *	controls gridding then this procedure has no effect.
 *
 *----------------------------------------------------------------------
 */

void
Tk_UnsetGrid(
    Tk_Window tkwin)		/* Token for window that is currently
				 * controlling gridding. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    WmInfo *wmPtr;

    /*
     * Find the top-level window for tkwin, plus the window manager
     * information.
     */

    while (!(winPtr->flags & TK_TOP_LEVEL)) {
	winPtr = winPtr->parentPtr;
    }
    wmPtr = winPtr->wmInfoPtr;
    if (tkwin != wmPtr->gridWin) {
	return;
    }

    wmPtr->gridWin = NULL;
    wmPtr->sizeHintsFlags &= ~(PBaseSize|PResizeInc);
    if (wmPtr->width != -1) {
	wmPtr->width = winPtr->reqWidth + (wmPtr->width
		- wmPtr->reqGridWidth)*wmPtr->widthInc;
	wmPtr->height = winPtr->reqHeight + (wmPtr->height
		- wmPtr->reqGridHeight)*wmPtr->heightInc;
    }
    wmPtr->widthInc = 1;
    wmPtr->heightInc = 1;

    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TopLevelEventProc --
 *
 *	This procedure is invoked when a top-level (or other externally-
 *	managed window) is restructured in any way.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tk's internal data structures for the window get modified to reflect
 *	the structural change.
 *
 *----------------------------------------------------------------------
 */

static void
TopLevelEventProc(
    ClientData clientData,	/* Window for which event occurred. */
    XEvent *eventPtr)		/* Event that just happened. */
{
    TkWindow *winPtr = clientData;

    winPtr->wmInfoPtr->flags |= WM_VROOT_OFFSET_STALE;
    if (eventPtr->type == DestroyNotify) {
	if (!(winPtr->flags & TK_ALREADY_DEAD)) {
	    /*
	     * A top-level window was deleted externally (e.g., by the window
	     * manager). This is probably not a good thing, but cleanup as
	     * best we can. The error handler is needed because
	     * Tk_DestroyWindow will try to destroy the window, but of course
	     * it's already gone.
	     */

	    Tk_ErrorHandler handler = Tk_CreateErrorHandler(winPtr->display,
		    -1, -1, -1, NULL, NULL);

	    Tk_DestroyWindow((Tk_Window) winPtr);
	    Tk_DeleteErrorHandler(handler);
	}
	if (wmTracing) {
	    TkMacOSXDbgMsg("TopLevelEventProc: %s deleted", winPtr->pathName);
	}
    } else if (eventPtr->type == ReparentNotify) {
	Tcl_Panic("recieved unwanted reparent event");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TopLevelReqProc --
 *
 *	This procedure is invoked by the geometry manager whenever the
 *	requested size for a top-level window is changed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arrange for the window to be resized to satisfy the request (this
 *	happens as a when-idle action).
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
TopLevelReqProc(
    ClientData dummy,		/* Not used. */
    Tk_Window tkwin)		/* Information about window. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    WmInfo *wmPtr;

    wmPtr = winPtr->wmInfoPtr;
    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateGeometryInfo --
 *
 *	This procedure is invoked when a top-level window is first mapped, and
 *	also as a when-idle procedure, to bring the geometry and/or position
 *	of a top-level window back into line with what has been requested by
 *	the user and/or widgets. This procedure doesn't return until the
 *	window manager has responded to the geometry change.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window's size and location may change, unless the WM prevents that
 *	from happening.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateGeometryInfo(
    ClientData clientData)	/* Pointer to the window's record. */
{
    TkWindow *winPtr = clientData;
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    int x, y, width, height, min, max;

    wmPtr->flags &= ~WM_UPDATE_PENDING;

    if (wmPtr->flags & WM_FULLSCREEN) {
	return;
    }

    /*
     * Compute the new size for the top-level window. See the user
     * documentation for details on this, but the size requested depends on
     * (a) the size requested internally by the window's widgets, (b) the size
     * requested by the user in a "wm geometry" command or via wm-based
     * interactive resizing (if any), and (c) whether or not the window is
     * gridded. Don't permit sizes <= 0 because this upsets the X server.
     */

    if (wmPtr->width == -1) {
	width = winPtr->reqWidth;
    } else if (wmPtr->gridWin != NULL) {
	width = winPtr->reqWidth
		+ (wmPtr->width - wmPtr->reqGridWidth)*wmPtr->widthInc;
    } else {
	width = wmPtr->width;
    }
    if (width <= 0) {
	width = 1;
    }

    /*
     * Account for window max/min width
     */

    if (wmPtr->gridWin != NULL) {
	min = winPtr->reqWidth
		+ (wmPtr->minWidth - wmPtr->reqGridWidth)*wmPtr->widthInc;
	if (wmPtr->maxWidth > 0) {
	    max = winPtr->reqWidth
		    + (wmPtr->maxWidth - wmPtr->reqGridWidth)*wmPtr->widthInc;
	} else {
	    max = 0;
	}
    } else {
	min = wmPtr->minWidth;
	max = wmPtr->maxWidth;
    }
    if (width < min) {
	width = min;
    } else if ((max > 0) && (width > max)) {
	width = max;
    }

    if (wmPtr->height == -1) {
	height = winPtr->reqHeight;
    } else if (wmPtr->gridWin != NULL) {
	height = winPtr->reqHeight
		+ (wmPtr->height - wmPtr->reqGridHeight)*wmPtr->heightInc;
    } else {
	height = wmPtr->height;
    }
    if (height <= 0) {
	height = 1;
    }

    /*
     * Account for window max/min height
     */

    if (wmPtr->gridWin != NULL) {
	min = winPtr->reqHeight
		+ (wmPtr->minHeight - wmPtr->reqGridHeight)*wmPtr->heightInc;
	if (wmPtr->maxHeight > 0) {
	    max = winPtr->reqHeight
		    + (wmPtr->maxHeight-wmPtr->reqGridHeight)*wmPtr->heightInc;
	} else {
	    max = 0;
	}
    } else {
	min = wmPtr->minHeight;
	max = wmPtr->maxHeight;
    }
    if (height < min) {
	height = min;
    } else if ((max > 0) && (height > max)) {
	height = max;
    }

    /*
     * Compute the new position for the upper-left pixel of the window's
     * decorative frame. This is tricky, because we need to include the border
     * widths supplied by a reparented parent in this calculation, but can't
     * use the parent's current overall size since that may change as a result
     * of this code.
     */

    if (wmPtr->flags & WM_NEGATIVE_X) {
	x = wmPtr->vRootWidth - wmPtr->x
	    - (width + (wmPtr->parentWidth - winPtr->changes.width));
    } else {
	x =  wmPtr->x;
    }
    if (wmPtr->flags & WM_NEGATIVE_Y) {
	y = wmPtr->vRootHeight - wmPtr->y
	    - (height + (wmPtr->parentHeight - winPtr->changes.height));
    } else {
	y =  wmPtr->y;
    }

    /*
     * If the window's size is going to change and the window is supposed to
     * not be resizable by the user, then we have to update the size hints.
     * There may also be a size-hint-update request pending from somewhere
     * else, too.
     */

    if (((width != winPtr->changes.width)
	    || (height != winPtr->changes.height))
	    && (wmPtr->gridWin == NULL)
	    && ((wmPtr->sizeHintsFlags & (PMinSize|PMaxSize)) == 0)) {
	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    }
    if (wmPtr->flags & WM_UPDATE_SIZE_HINTS) {
	UpdateSizeHints(winPtr);
    }

    /*
     * Reconfigure the window if it isn't already configured correctly. A few
     * tricky points:
     *
     * 1. If the window is embedded and the container is also in this process,
     *    don't actually reconfigure the window; just pass the desired size on
     *    to the container. Also, zero out any position information, since
     *    embedded windows are not allowed to move.
     * 2. Sometimes the window manager will give us a different size than we
     *    asked for (e.g. mwm has a minimum size for windows), so base the
     *    size check on what we *asked for* last time, not what we got.
     * 3. Don't move window unless a new position has been requested for it.
     *    This is because of "features" in some window managers (e.g. twm, as
     *    of 4/24/91) where they don't interpret coordinates according to
     *    ICCCM. Moving a window to its current location may cause it to shift
     *    position on the screen.
     */

    if (Tk_IsEmbedded(winPtr)) {
	TkWindow *contWinPtr = TkpGetOtherWindow(winPtr);

	/*
	 * TODO: Here we should handle out of process embedding.
	 */

	if (contWinPtr != NULL) {
	    /*
	     * This window is embedded and the container is also in this
	     * process, so we don't need to do anything special about the
	     * geometry, except to make sure that the desired size is known by
	     * the container. Also, zero out any position information, since
	     * embedded windows are not allowed to move.
	     */

	    wmPtr->x = wmPtr->y = 0;
	    wmPtr->flags &= ~(WM_NEGATIVE_X|WM_NEGATIVE_Y);
	    Tk_GeometryRequest((Tk_Window) contWinPtr, width, height);
	}
	return;
    }
    if (wmPtr->flags & WM_MOVE_PENDING) {
	wmPtr->configWidth = width;
	wmPtr->configHeight = height;
	if (wmTracing) {
	    TkMacOSXDbgMsg("Moving to %d %d, resizing to %d x %d", x, y,
		    width, height);
	}
	SetWindowSizeLimits(winPtr);
	wmPtr->flags |= WM_SYNC_PENDING;
	XMoveResizeWindow(winPtr->display, winPtr->window, x, y,
		wmPtr->configWidth, wmPtr->configHeight);
	wmPtr->flags &= ~WM_SYNC_PENDING;
    } else if ((width != wmPtr->configWidth)
	    || (height != wmPtr->configHeight)) {
	wmPtr->configWidth = width;
	wmPtr->configHeight = height;
	if (wmTracing) {
	    TkMacOSXDbgMsg("Resizing to %d x %d\n", width, height);
	}
	SetWindowSizeLimits(winPtr);
	wmPtr->flags |= WM_SYNC_PENDING;
	XResizeWindow(winPtr->display, winPtr->window, wmPtr->configWidth,
		wmPtr->configHeight);
	wmPtr->flags &= ~WM_SYNC_PENDING;
    } else {
	SetWindowSizeLimits(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateSizeHints --
 *
 *	This procedure is called to update the window manager's size hints
 *	information from the information in a WmInfo structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Properties get changed for winPtr.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateSizeHints(
    TkWindow *winPtr)
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;

    wmPtr->flags &= ~WM_UPDATE_SIZE_HINTS;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseGeometry --
 *
 *	This procedure parses a geometry string and updates information used
 *	to control the geometry of a top-level window.
 *
 * Results:
 *	A standard Tcl return value, plus an error message in the interp's
 *	result if an error occurs.
 *
 * Side effects:
 *	The size and/or location of winPtr may change.
 *
 *----------------------------------------------------------------------
 */

static int
ParseGeometry(
    Tcl_Interp *interp,		/* Used for error reporting. */
    char *string,		/* String containing new geometry. Has the
				 * standard form "=wxh+x+y". */
    TkWindow *winPtr)		/* Pointer to top-level window whose geometry
				 * is to be changed. */
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    int x, y, width, height, flags;
    char *end;
    char *p = string;

    /*
     * The leading "=" is optional.
     */

    if (*p == '=') {
	p++;
    }

    /*
     * Parse the width and height, if they are present. Don't actually update
     * any of the fields of wmPtr until we've successfully parsed the entire
     * geometry string.
     */

    width = wmPtr->width;
    height = wmPtr->height;
    x = wmPtr->x;
    y = wmPtr->y;
    flags = wmPtr->flags;
    if (isdigit(UCHAR(*p))) {
	width = strtoul(p, &end, 10);
	p = end;
	if (*p != 'x') {
	    goto error;
	}
	p++;
	if (!isdigit(UCHAR(*p))) {
	    goto error;
	}
	height = strtoul(p, &end, 10);
	p = end;
    }

    /*
     * Parse the X and Y coordinates, if they are present.
     */

    if (*p != '\0') {
	flags &= ~(WM_NEGATIVE_X | WM_NEGATIVE_Y);
	if (*p == '-') {
	    flags |= WM_NEGATIVE_X;
	} else if (*p != '+') {
	    goto error;
	}
	p++;
	if (!isdigit(UCHAR(*p)) && (*p != '-')) {
	    goto error;
	}
	x = strtol(p, &end, 10);
	p = end;
	if (*p == '-') {
	    flags |= WM_NEGATIVE_Y;
	} else if (*p != '+') {
	    goto error;
	}
	p++;
	if (!isdigit(UCHAR(*p)) && (*p != '-')) {
	    goto error;
	}
	y = strtol(p, &end, 10);
	if (*end != '\0') {
	    goto error;
	}

	/*
	 * Assume that the geometry information came from the user, unless an
	 * explicit source has been specified. Otherwise most window managers
	 * assume that the size hints were program-specified and they ignore
	 * them.
	 */

	if ((wmPtr->sizeHintsFlags & (USPosition|PPosition)) == 0) {
	    wmPtr->sizeHintsFlags |= USPosition;
	    flags |= WM_UPDATE_SIZE_HINTS;
	}
    }

    /*
     * Everything was parsed OK. Update the fields of *wmPtr and arrange for
     * the appropriate information to be percolated out to the window manager
     * at the next idle moment.
     */

    wmPtr->width = width;
    wmPtr->height = height;
    if ((x != wmPtr->x) || (y != wmPtr->y)
	    || ((flags & (WM_NEGATIVE_X|WM_NEGATIVE_Y))
		    != (wmPtr->flags & (WM_NEGATIVE_X|WM_NEGATIVE_Y)))) {
	if (wmPtr->flags & WM_FULLSCREEN) {
	    wmPtr->configX = x;
	    wmPtr->configY = y;
	} else {
	    wmPtr->x = x;
	    wmPtr->y = y;
	}
	flags |= WM_MOVE_PENDING;
    }
    wmPtr->flags = flags;

    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
    return TCL_OK;

  error:
    Tcl_AppendResult(interp, "bad geometry specifier \"", string, "\"", NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetRootCoords --
 *
 *	Given a token for a window, this procedure traces through the window's
 *	lineage to find the (virtual) root-window coordinates corresponding to
 *	point (0,0) in the window.
 *
 * Results:
 *	The locations pointed to by xPtr and yPtr are filled in with the root
 *	coordinates of the (0,0) point in tkwin. If a virtual root window is
 *	in effect for the window, then the coordinates in the virtual root are
 *	returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tk_GetRootCoords(
    Tk_Window tkwin,		/* Token for window. */
    int *xPtr,			/* Where to store x-displacement of (0,0). */
    int *yPtr)			/* Where to store y-displacement of (0,0). */
{
    int x, y;
    TkWindow *winPtr = (TkWindow *) tkwin;

    /*
     * Search back through this window's parents all the way to a top-level
     * window, combining the offsets of each window within its parent.
     */

    x = y = 0;
    while (1) {
	x += winPtr->changes.x + winPtr->changes.border_width;
	y += winPtr->changes.y + winPtr->changes.border_width;
	if (winPtr->flags & TK_TOP_LEVEL) {
	    TkWindow *otherPtr;

	    if (!(Tk_IsEmbedded(winPtr))) {
		x += winPtr->wmInfoPtr->xInParent;
		y += winPtr->wmInfoPtr->yInParent;
		break;
	    }

	    otherPtr = TkpGetOtherWindow(winPtr);
	    if (otherPtr == NULL) {
		if (tkMacOSXEmbedHandler->getOffsetProc != NULL) {
		    Point theOffset;

		    /*
		     * We do not require that the changes.x & changes.y for a
		     * non-Tk master window be kept up to date. So we first
		     * subtract off the possibly bogus values that have been
		     * added on at the top of this pass through the loop, and
		     * then call out to the getOffsetProc to give us the
		     * correct offset.
		     */

		    x -= winPtr->changes.x + winPtr->changes.border_width;
		    y -= winPtr->changes.y + winPtr->changes.border_width;

		    tkMacOSXEmbedHandler->getOffsetProc((Tk_Window) winPtr,
			    &theOffset);

		    x += theOffset.h;
		    y += theOffset.v;
		}
		break;
	    }

	    /*
	     * The container window is in the same application. Query its
	     * coordinates.
	     */

	    winPtr = otherPtr;

	    /*
	     * Remember to offset by the container window here, since at the
	     * end of this if branch, we will pop out to the container's
	     * parent...
	     */

	    x += winPtr->changes.x + winPtr->changes.border_width;
	    y += winPtr->changes.y + winPtr->changes.border_width;
	}
	winPtr = winPtr->parentPtr;
    }
    *xPtr = x;
    *yPtr = y;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_CoordsToWindow --
 *
 *	This is a Macintosh specific implementation of this function. Given
 *	the root coordinates of a point, this procedure returns the token for
 *	the top-most window covering that point, if there exists such a window
 *	in this application.
 *
 * Results:
 *	The return result is either a token for the window corresponding to
 *	rootX and rootY, or else NULL to indicate that there is no such
 *	window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
Tk_CoordsToWindow(
    int rootX, int rootY,	/* Coordinates of point in root window. If a
				 * virtual-root window manager is in use,
				 * these coordinates refer to the virtual
				 * root, not the real root. */
    Tk_Window tkwin)		/* Token for any window in application; used
				 * to identify the display. */
{
    TkWindow *winPtr, *childPtr;
    TkWindow *nextPtr;		/* Coordinates of highest child found so far
				 * that contains point. */
    int x, y;			/* Coordinates in winPtr. */
    int tmpx, tmpy, bd;

    /*
     * Step 1: find the top-level window that contains the desired point.
     */

    winPtr = FrontWindowAtPoint(rootX, rootY);
    if (!winPtr) {
	return NULL;
    }

    /*
     * Step 2: work down through the hierarchy underneath this window. At each
     * level, scan through all the children to find the highest one in the
     * stacking order that contains the point. Then repeat the whole process
     * on that child.
     */

    x = rootX - winPtr->wmInfoPtr->xInParent;
    y = rootY - winPtr->wmInfoPtr->yInParent;
    while (1) {
	x -= winPtr->changes.x;
	y -= winPtr->changes.y;
	nextPtr = NULL;

	/*
	 * Container windows cannot have children. So if it is a container,
	 * look there, otherwise inspect the children.
	 */

	if (Tk_IsContainer(winPtr)) {
	    childPtr = TkpGetOtherWindow(winPtr);
	    if (childPtr != NULL) {
		if (Tk_IsMapped(childPtr)) {
		    tmpx = x - childPtr->changes.x;
		    tmpy = y - childPtr->changes.y;
		    bd = childPtr->changes.border_width;

		    if ((tmpx >= -bd) && (tmpy >= -bd)
			    && (tmpx < (childPtr->changes.width + bd))
			    && (tmpy < (childPtr->changes.height + bd))) {
			nextPtr = childPtr;
		    }
		}
	    }

	    /*
	     * TODO: Here we should handle out of process embedding.
	     */
	} else {
	    for (childPtr = winPtr->childList; childPtr != NULL;
		    childPtr = childPtr->nextPtr) {
		if (!Tk_IsMapped(childPtr) ||
			(childPtr->flags & TK_TOP_LEVEL)) {
		    continue;
		}
		tmpx = x - childPtr->changes.x;
		tmpy = y - childPtr->changes.y;
		bd = childPtr->changes.border_width;
		if ((tmpx >= -bd) && (tmpy >= -bd)
			&& (tmpx < (childPtr->changes.width + bd))
			&& (tmpy < (childPtr->changes.height + bd))) {
		    nextPtr = childPtr;
		}
	    }
	}
	if (nextPtr == NULL) {
	    break;
	}
	winPtr = nextPtr;
    }
    return (Tk_Window) winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_TopCoordsToWindow --
 *
 *	Given a Tk Window, and coordinates of a point relative to that window
 *	this procedure returns the top-most child of the window (excluding
 *	toplevels) covering that point, if there exists such a window in this
 *	application. It also sets newX, and newY to the coords of the point
 *	relative to the window returned.
 *
 * Results:
 *	The return result is either a token for the window corresponding to
 *	rootX and rootY, or else NULL to indicate that there is no such
 *	window. newX and newY are also set to the coords of the point relative
 *	to the returned window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
Tk_TopCoordsToWindow(
    Tk_Window tkwin,		/* Token for a Tk Window which defines the
				 * coordinates for rootX & rootY */
    int rootX, int rootY,	/* Coordinates of a point in tkWin. */
    int *newX, int *newY)	/* Coordinates of point in the upperMost child
				 * of tkWin containing (rootX,rootY) */
{
    TkWindow *winPtr, *childPtr;
    TkWindow *nextPtr;		/* Coordinates of highest child found so far
				 * that contains point. */
    int x, y;			/* Coordinates in winPtr. */
    Window *children;		/* Children of winPtr, or NULL. */

    winPtr = (TkWindow *) tkwin;
    x = rootX;
    y = rootY;
    while (1) {
	nextPtr = NULL;
	children = NULL;

	/*
	 * Container windows cannot have children. So if it is a container,
	 * look there, otherwise inspect the children.
	 */

	if (Tk_IsContainer(winPtr)) {
	    childPtr = TkpGetOtherWindow(winPtr);
	    if (childPtr != NULL) {
		if (Tk_IsMapped(childPtr) &&
			x > childPtr->changes.x &&
			x < childPtr->changes.x + childPtr->changes.width &&
			y > childPtr->changes.y &&
			y < childPtr->changes.y + childPtr->changes.height) {
		    nextPtr = childPtr;
		}
	    }

	    /*
	     * TODO: Here we should handle out of process embedding.
	     */
	} else {
	    for (childPtr = winPtr->childList; childPtr != NULL;
		    childPtr = childPtr->nextPtr) {
		if (!Tk_IsMapped(childPtr) ||
			(childPtr->flags & TK_TOP_LEVEL)) {
		    continue;
		}
		if (x < childPtr->changes.x || y < childPtr->changes.y) {
		    continue;
		}
		if (x > childPtr->changes.x + childPtr->changes.width ||
			y > childPtr->changes.y + childPtr->changes.height) {
		    continue;
		}
		nextPtr = childPtr;
	    }
	}
	if (nextPtr == NULL) {
	    break;
	}
	winPtr = nextPtr;
	x -= winPtr->changes.x;
	y -= winPtr->changes.y;
    }
    *newX = x;
    *newY = y;
    return (Tk_Window) winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateVRootGeometry --
 *
 *	This procedure is called to update all the virtual root geometry
 *	information in wmPtr.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The vRootX, vRootY, vRootWidth, and vRootHeight fields in wmPtr are
 *	filled with the most up-to-date information.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateVRootGeometry(
    WmInfo *wmPtr)		/* Window manager information to be updated.
				 * The wmPtr->vRoot field must be valid. */
{
    TkWindow *winPtr = wmPtr->winPtr;
    unsigned int bd, dummy;
    Window dummy2;
    Status status;
    Tk_ErrorHandler handler;

    /*
     * If this isn't a virtual-root window manager, just return information
     * about the screen.
     */

    wmPtr->flags &= ~WM_VROOT_OFFSET_STALE;
    if (wmPtr->vRoot == None) {
	noVRoot:
	wmPtr->vRootX = wmPtr->vRootY = 0;
	wmPtr->vRootWidth = DisplayWidth(winPtr->display, winPtr->screenNum);
	wmPtr->vRootHeight = DisplayHeight(winPtr->display, winPtr->screenNum);
	return;
    }

    /*
     * Refresh the virtual root information if it's out of date.
     */

    handler = Tk_CreateErrorHandler(winPtr->display, -1, -1, -1, NULL, NULL);
    status = XGetGeometry(winPtr->display, wmPtr->vRoot,
	    &dummy2, &wmPtr->vRootX, &wmPtr->vRootY,
	    &wmPtr->vRootWidth, &wmPtr->vRootHeight, &bd, &dummy);
    if (wmTracing) {
	TkMacOSXDbgMsg("x = %d, y = %d, width = %d, height = %d, status = %d",
		wmPtr->vRootX, wmPtr->vRootY, wmPtr->vRootWidth,
		wmPtr->vRootHeight, status);
    }
    Tk_DeleteErrorHandler(handler);
    if (status == 0) {
	/*
	 * The virtual root is gone! Pretend that it never existed.
	 */

	wmPtr->vRoot = None;
	goto noVRoot;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetVRootGeometry --
 *
 *	This procedure returns information about the virtual root window
 *	corresponding to a particular Tk window.
 *
 * Results:
 *	The values at xPtr, yPtr, widthPtr, and heightPtr are set with the
 *	offset and dimensions of the root window corresponding to tkwin. If
 *	tkwin is being managed by a virtual root window manager these values
 *	correspond to the virtual root window being used for tkwin; otherwise
 *	the offsets will be 0 and the dimensions will be those of the screen.
 *
 * Side effects:
 *	Vroot window information is refreshed if it is out of date.
 *
 *----------------------------------------------------------------------
 */

void
Tk_GetVRootGeometry(
    Tk_Window tkwin,		/* Window whose virtual root is to be
				 * queried. */
    int *xPtr, int *yPtr,	/* Store x and y offsets of virtual root
				 * here. */
    int *widthPtr,		/* Store dimensions of virtual root here. */
    int *heightPtr)
{
    WmInfo *wmPtr;
    TkWindow *winPtr = (TkWindow *) tkwin;

    /*
     * Find the top-level window for tkwin, and locate the window manager
     * information for that window.
     */

    while (!(winPtr->flags & TK_TOP_LEVEL)) {
	winPtr = winPtr->parentPtr;
    }
    wmPtr = winPtr->wmInfoPtr;

    /*
     * Make sure that the geometry information is up-to-date, then copy it out
     * to the caller.
     */

    if (wmPtr->flags & WM_VROOT_OFFSET_STALE) {
	UpdateVRootGeometry(wmPtr);
    }
    *xPtr = wmPtr->vRootX;
    *yPtr = wmPtr->vRootY;
    *widthPtr = wmPtr->vRootWidth;
    *heightPtr = wmPtr->vRootHeight;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MoveToplevelWindow --
 *
 *	This procedure is called instead of Tk_MoveWindow to adjust the x-y
 *	location of a top-level window. It delays the actual move to a later
 *	time and keeps window-manager information up-to-date with the move.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is eventually moved so that its upper-left corner
 *	(actually, the upper-left corner of the window's decorative frame, if
 *	there is one) is at (x,y).
 *
 *----------------------------------------------------------------------
 */

void
Tk_MoveToplevelWindow(
    Tk_Window tkwin,		/* Window to move. */
    int x, int y)		/* New location for window (within parent). */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    WmInfo *wmPtr = winPtr->wmInfoPtr;

    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	Tcl_Panic("Tk_MoveToplevelWindow called with non-toplevel window");
    }
    wmPtr->x = x;
    wmPtr->y = y;
    wmPtr->flags |= WM_MOVE_PENDING;
    wmPtr->flags &= ~(WM_NEGATIVE_X|WM_NEGATIVE_Y);
    if ((wmPtr->sizeHintsFlags & (USPosition|PPosition)) == 0) {
	wmPtr->sizeHintsFlags |= USPosition;
	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    }

    /*
     * If the window has already been mapped, must bring its geometry
     * up-to-date immediately, otherwise an event might arrive from the server
     * that would overwrite wmPtr->x and wmPtr->y and lose the new position.
     */

    if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	if (wmPtr->flags & WM_UPDATE_PENDING) {
	    Tcl_CancelIdleCall(UpdateGeometryInfo, winPtr);
	}
	UpdateGeometryInfo(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmRestackToplevel --
 *
 *	This procedure restacks a top-level window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	WinPtr gets restacked as specified by aboveBelow and otherPtr. This
 *	procedure doesn't return until the restack has taken effect and the
 *	ConfigureNotify event for it has been received.
 *
 *----------------------------------------------------------------------
 */

void
TkWmRestackToplevel(
    TkWindow *winPtr,		/* Window to restack. */
    int aboveBelow,		/* Gives relative position for restacking;
				 * must be Above or Below. */
    TkWindow *otherPtr)		/* Window relative to which to restack; if
				 * NULL, then winPtr gets restacked above or
				 * below *all* siblings. */
{
    NSWindow *macWindow;
    NSInteger otherMacWindowNumber;

    /*
     * Get the mac window. Make sure it exists & is mapped.
     */

    if (winPtr->window == None) {
	Tk_MakeWindowExist((Tk_Window) winPtr);
    }
    if (winPtr->wmInfoPtr->flags & WM_NEVER_MAPPED) {
	/*
	 * Can't set stacking order properly until the window is on the screen
	 * (mapping it may give it a reparent window), so make sure it's on
	 * the screen.
	 */

	TkWmMapWindow(winPtr);
    }
    macWindow = TkMacOSXDrawableWindow(winPtr->window);

    /*
     * Get the window in which a raise or lower is in relation to.
     */

    if (otherPtr != NULL) {
	if (otherPtr->window == None) {
	    Tk_MakeWindowExist((Tk_Window) otherPtr);
	}
	if (otherPtr->wmInfoPtr->flags & WM_NEVER_MAPPED) {
	    TkWmMapWindow(otherPtr);
	}
	otherMacWindowNumber = [TkMacOSXDrawableWindow(otherPtr->window)
		windowNumber];
    } else {
	otherMacWindowNumber = 0;
    }
    [macWindow orderWindow:(aboveBelow == Above ? NSWindowAbove : NSWindowBelow)
	    relativeTo:otherMacWindowNumber];
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmAddToColormapWindows --
 *
 *	This procedure is called to add a given window to the
 *	WM_COLORMAP_WINDOWS property for its top-level, if it isn't already
 *	there. It is invoked by the Tk code that creates a new colormap, in
 *	order to make sure that colormap information is propagated to the
 *	window manager by default.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	WinPtr's window gets added to the WM_COLORMAP_WINDOWS property of its
 *	nearest top-level ancestor, unless the colormaps have been set
 *	explicitly with the "wm colormapwindows" command.
 *
 *----------------------------------------------------------------------
 */

void
TkWmAddToColormapWindows(
    TkWindow *winPtr)		/* Window with a non-default colormap. Should
				 * not be a top-level window. */
{
    TkWindow *topPtr;
    TkWindow **oldPtr, **newPtr;
    int count, i;

    if (winPtr->window == None) {
	return;
    }

    for (topPtr = winPtr->parentPtr; ; topPtr = topPtr->parentPtr) {
	if (topPtr == NULL) {
	    /*
	     * Window is being deleted. Skip the whole operation.
	     */

	    return;
	}
	if (topPtr->flags & TK_TOP_LEVEL) {
	    break;
	}
    }
    if (topPtr->wmInfoPtr->flags & WM_COLORMAPS_EXPLICIT) {
	return;
    }

    /*
     * Make sure that the window isn't already in the list.
     */

    count = topPtr->wmInfoPtr->cmapCount;
    oldPtr = topPtr->wmInfoPtr->cmapList;

    for (i = 0; i < count; i++) {
	if (oldPtr[i] == winPtr) {
	    return;
	}
    }

    /*
     * Make a new bigger array and use it to reset the property. Automatically
     * add the toplevel itself as the last element of the list.
     */

    newPtr = (TkWindow**)ckalloc((count+2) * sizeof(TkWindow *));
    if (count > 0) {
	memcpy(newPtr, oldPtr, count * sizeof(TkWindow *));
    }
    if (count == 0) {
	count++;
    }
    newPtr[count-1] = winPtr;
    newPtr[count] = topPtr;
    if (oldPtr != NULL) {
	ckfree((char *) oldPtr);
    }

    topPtr->wmInfoPtr->cmapList = newPtr;
    topPtr->wmInfoPtr->cmapCount = count+1;

    /*
     * On the Macintosh all of this is just an excercise in compatability as
     * we don't support colormaps. If we did they would be installed here.
     */
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmRemoveFromColormapWindows --
 *
 *	This procedure is called to remove a given window from the
 *	WM_COLORMAP_WINDOWS property for its top-level. It is invoked when
 *	windows are deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	WinPtr's window gets removed from the WM_COLORMAP_WINDOWS property of
 *	its nearest top-level ancestor, unless the top-level itself is being
 *	deleted too.
 *
 *----------------------------------------------------------------------
 */

void
TkWmRemoveFromColormapWindows(
    TkWindow *winPtr)		/* Window that may be present in
				 * WM_COLORMAP_WINDOWS property for its
				 * top-level. Should not be a top-level
				 * window. */
{
    TkWindow *topPtr, **oldPtr;
    int count, i, j;

    for (topPtr = winPtr->parentPtr; ; topPtr = topPtr->parentPtr) {
	if (topPtr == NULL) {
	    /*
	     * Ancestors have been deleted, so skip the whole operation. Seems
	     * like this can't ever happen?
	     */

	    return;
	}
	if (topPtr->flags & TK_TOP_LEVEL) {
	    break;
	}
    }
    if (topPtr->flags & TK_ALREADY_DEAD) {
	/*
	 * Top-level is being deleted, so there's no need to cleanup the
	 * WM_COLORMAP_WINDOWS property.
	 */

	return;
    }

    /*
     * Find the window and slide the following ones down to cover it up.
     */

    count = topPtr->wmInfoPtr->cmapCount;
    oldPtr = topPtr->wmInfoPtr->cmapList;
    for (i = 0; i < count; i++) {
	if (oldPtr[i] == winPtr) {
	    for (j = i ; j < count-1; j++) {
		oldPtr[j] = oldPtr[j+1];
	    }
	    topPtr->wmInfoPtr->cmapCount = count - 1;
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetPointerCoords --
 *
 *	Fetch the position of the mouse pointer.
 *
 * Results:
 *	*xPtr and *yPtr are filled in with the (virtual) root coordinates of
 *	the mouse pointer for tkwin's display. If the pointer isn't on tkwin's
 *	screen, then -1 values are returned for both coordinates. The argument
 *	tkwin must be a toplevel window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkGetPointerCoords(
    Tk_Window tkwin,		/* Toplevel window that identifies screen on
				 * which lookup is to be done. */
    int *xPtr, int *yPtr)	/* Store pointer coordinates here. */
{
    XQueryPointer(NULL, None, NULL, NULL, xPtr, yPtr, NULL, NULL, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * InitialWindowBounds --
 *
 *	This function calculates the initial bounds for a new Mac toplevel
 *	window. Unless the geometry is specified by the user this code will
 *	auto place the windows in a cascade diagonially across the main
 *	monitor of the Mac.
 *
 * Results:
 *	Window bounds.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static NSRect
InitialWindowBounds(
    TkWindow *winPtr,		/* Window to get initial bounds for. */
    NSWindow *macWindow)
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;

    if (!(wmPtr->sizeHintsFlags & (USPosition | PPosition))) {
	static NSPoint cascadePoint = { .x = 0, .y = 0 };
	NSRect frame;

	cascadePoint = [macWindow cascadeTopLeftFromPoint:cascadePoint];
	frame = [macWindow frame];
	wmPtr->x = frame.origin.x;
	wmPtr->y = tkMacOSXZeroScreenHeight - (frame.origin.y +
		frame.size.height);
    }
    return NSMakeRect(wmPtr->x, wmPtr->y, winPtr->changes.width,
	    winPtr->changes.height);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXResizable --
 *
 *	This function determines if the passed in window is part of a toplevel
 *	window that is resizable. If the window is resizable in the x, y or
 *	both directions, true is returned.
 *
 * Results:
 *	True if resizable, false otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkMacOSXResizable(
    TkWindow *winPtr)		/* Tk window or NULL. */
{
    WmInfo *wmPtr;

    if (winPtr == NULL) {
	return false;
    }
    while (winPtr->wmInfoPtr == NULL) {
	winPtr = winPtr->parentPtr;
    }

    wmPtr = winPtr->wmInfoPtr;
    if ((wmPtr->flags & WM_WIDTH_NOT_RESIZABLE) &&
	    (wmPtr->flags & WM_HEIGHT_NOT_RESIZABLE)) {
	return false;
    } else {
	return true;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGrowToplevel --
 *
 *	The function is invoked when the user clicks in the grow region of a
 *	Tk window. The function will handle the dragging procedure and not
 *	return until completed. Finally, the function may place information
 *	Tk's event queue is the window was resized.
 *
 * Results:
 *	True if events were placed on event queue, false otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkMacOSXGrowToplevel(
    void *whichWindow,
    XPoint start)
{
    return false;
}

/*
 *----------------------------------------------------------------------
 *
 * TkSetWMName --
 *
 *	Set the title for a toplevel window. If the window is embedded, do not
 *	change the window title.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The title of the window is changed.
 *
 *----------------------------------------------------------------------
 */

void
TkSetWMName(
    TkWindow *winPtr,
    Tk_Uid titleUid)
{
    if (Tk_IsEmbedded(winPtr)) {
	return;
    }

    NSString *title = [[NSString alloc] initWithUTF8String:titleUid];
    [TkMacOSXDrawableWindow(winPtr->window) setTitle:title];
    [title release];
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetTransientMaster --
 *
 *	If the passed window has the TRANSIENT_FOR property set this will
 *	return the master window. Otherwise it will return None.
 *
 * Results:
 *	The master window or None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Window
TkGetTransientMaster(
    TkWindow *winPtr)
{
    if (winPtr->wmInfoPtr != NULL) {
	return winPtr->wmInfoPtr->master;
    }
    return None;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetXWindow --
 *
 *	Returns the X window Id associated with the given NSWindow*.
 *
 * Results:
 *	The window id is returned. None is returned if not a Tk window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Window
TkMacOSXGetXWindow(
    void *macWinPtr)
{
    Tcl_HashEntry *hPtr;

    if (!macWinPtr || !windowHashInit) {
	return None;
    }
    hPtr = Tcl_FindHashEntry(&windowTable, macWinPtr);
    if (hPtr == NULL) {
	return None;
    }
    return (Window) Tcl_GetHashValue(hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetTkWindow --
 *
 *	Returns the TkWindow* associated with the given NSWindow*.
 *
 * Results:
 *	The TkWindow* returned. NULL is returned if not a Tk window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkWindow*
TkMacOSXGetTkWindow(
    NSWindow *w)
{
    Window window = TkMacOSXGetXWindow(w);
    TkDisplay *dispPtr = TkGetDisplayList();

    return (window != None ?
	    (TkWindow *)Tk_IdToWindow(dispPtr->display, window) : NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXIsWindowZoomed --
 *
 *	Ask Carbon if the given window is in the zoomed out state. Because
 *	dragging & growing a window can change the Carbon zoom state, we
 *	cannot rely on wmInfoPtr->hints.initial_state for this information.
 *
 * Results:
 *	True if window is zoomed out, false otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
TkMacOSXIsWindowZoomed(
    TkWindow *winPtr)
{
    return [TkMacOSXDrawableWindow(winPtr->window) isZoomed];
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXZoomToplevel --
 *
 *	The function is invoked when the user clicks in the zoom region of a
 *	Tk window or when the window state is set/unset to "zoomed" manually.
 *	If the window is to be zoomed (in or out), the window size is changed
 *	and events are generated to let Tk know what happened.
 *
 * Results:
 *	True if events were placed on event queue, false otherwise.
 *
 * Side effects:
 *	The window may be resized & events placed on Tk's queue.
 *
 *----------------------------------------------------------------------
 */

int
TkMacOSXZoomToplevel(
    void *whichWindow,		/* The Macintosh window to zoom. */
    short zoomPart)		/* Either inZoomIn or inZoomOut */
{
    NSWindow *window = whichWindow;
    TkWindow *winPtr = TkMacOSXGetTkWindow(window);
    WmInfo *wmPtr;

    if (!winPtr || !winPtr->wmInfoPtr) {
	return false;
    }
    wmPtr = winPtr->wmInfoPtr;
    if ((wmPtr->flags & WM_WIDTH_NOT_RESIZABLE) &&
	    (wmPtr->flags & WM_HEIGHT_NOT_RESIZABLE)) {
	return false;
    }

    /*
     * Do nothing if already in desired zoom state.
     */

    if (![window isZoomed] == (zoomPart == inZoomIn)) {
	return false;
    }
    [window zoom:NSApp];
    wmPtr->hints.initial_state =
	    (zoomPart == inZoomIn ? NormalState : ZoomState);
    return true;
}

/*
 *----------------------------------------------------------------------
 *
 * TkUnsupported1Cmd --
 *
 *	This procedure is invoked to process the
 *	"::tk::unsupported::MacWindowStyle" Tcl command. This command allows
 *	you to set the style of decoration for a Macintosh window.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Changes the style of a new Mac window.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
int
TkUnsupported1ObjCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    static const char *const subcmds[] = {
	"style", NULL
    };
    enum SubCmds {
	TKMWS_STYLE
    };
    Tk_Window tkwin = clientData;
    TkWindow *winPtr;
    int index, i;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "option window ?arg ...?");
	return TCL_ERROR;
    }


    /* Iterate through objc/objv to set correct background color and toggle opacity of window. */
    for (i= 0; i < objc; i++) {
    	if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*black*")) {
    	    colorName = [NSColor blackColor];	// use #000000 in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*dark*")) {
    	    colorName = [NSColor darkGrayColor]; //use #545454 in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*light*")) {
    	    colorName = [NSColor lightGrayColor]; //use #ababab in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*white*")) {
    	    colorName = [NSColor whiteColor];	//use #ffffff in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "gray*")) {
    	    colorName = [NSColor grayColor];	//use #7f7f7f in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*red*")) {
    	    colorName = [NSColor redColor];	//use #ff0000 in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*green*")) {
    	    colorName = [NSColor greenColor];	//use #00ff00 in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*blue*")) {
    	    colorName = [NSColor blueColor];	//use #0000ff in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*cyan*")) {
    	    colorName = [NSColor cyanColor];	//use #00ffff in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*yellow*")) {
    	    colorName = [NSColor yellowColor];	//use #ffff00 in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*magenta*")) {
    	    colorName = [NSColor magentaColor];	//use #ff00ff in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*orange*")) {
    	    colorName = [NSColor orangeColor];	//use #ff8000 in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*purple*")) {
    	    colorName = [NSColor purpleColor];	//use #800080 in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*brown*")){
    	    colorName = [NSColor brownColor];	//use #996633 in Tk scripts to match
    	} else if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*clear*")) {
    	    colorName = [NSColor clearColor];	//use systemTransparent in Tk scripts to match
    	}
    	if (Tcl_StringMatch(Tcl_GetString(objv[i]), "*opacity*")) {
    	    opaqueTag = YES;
    	}
    }


    winPtr = (TkWindow *)
	    Tk_NameToWindow(interp, Tcl_GetString(objv[2]), tkwin);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }
    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "window \"", winPtr->pathName,
		"\" isn't a top-level window", NULL);
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], subcmds, "option", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }
    if (((enum SubCmds) index) == TKMWS_STYLE) {
	if ((objc < 3) || (objc > 5)) {
	    Tcl_WrongNumArgs(interp, 2, objv, "window ?class attributes?");
	    return TCL_ERROR;
	}
	return WmWinStyle(interp, winPtr, objc, objv);
    }
    /* won't be reached */
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * WmWinStyle --
 *
 *	This procedure is invoked to process the
 *	"::tk::unsupported::MacWindowStyle style" subcommand. This command
 *	allows you to set the style of decoration for a Macintosh window.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Changes the style of a new Mac window.
 *
 *----------------------------------------------------------------------
 */

static int
WmWinStyle(
    Tcl_Interp *interp,		/* Current interpreter. */
    TkWindow *winPtr,		/* Window to be manipulated. */
    int objc,			/* Number of arguments. */
    Tcl_Obj * const objv[])	/* Argument objects. */
{
    struct StrIntMap {
	const char *strValue;
	UInt64 intValue;
    };
    static const struct StrIntMap classMap[] = {
	{ "alert",		kAlertWindowClass			     },
	{ "moveableAlert",	kMovableAlertWindowClass		     },
	{ "modal",		kModalWindowClass			     },
	{ "moveableModal",	kMovableModalWindowClass		     },
	{ "floating",		kFloatingWindowClass			     },
	{ "document",		kDocumentWindowClass			     },
	{ "utility",		kUtilityWindowClass			     },
	{ "help",		kHelpWindowClass			     },
	{ "sheet",		kSheetWindowClass			     },
	{ "toolbar",		kToolbarWindowClass			     },
	{ "plain",		kPlainWindowClass			     },
	{ "overlay",		kOverlayWindowClass			     },
	{ "sheetAlert",		kSheetAlertWindowClass			     },
	{ "altPlain",		kAltPlainWindowClass			     },
	{ "simple",		kSimpleWindowClass			     },
	{ "drawer",		kDrawerWindowClass			     },
	{ NULL }
    };
    static const struct StrIntMap compositeAttrMap[] = {
	{ "none",		kWindowNoAttributes			     },
	{ "standardDocument",	kWindowStandardDocumentAttributes	     },
	{ "standardFloating",	kWindowStandardFloatingAttributes	     },
	{ "fullZoom",		kWindowFullZoomAttribute		     },
	{ NULL }
    };

    /* Map window attributes. Color and opacity are mapped to NULL; these are parsed from the objv in TkUnsupported1ObjCmd.*/

    static const struct StrIntMap attrMap[] = {
	{ "closeBox",		kWindowCloseBoxAttribute		     },
	{ "horizontalZoom",	kWindowHorizontalZoomAttribute		     },
	{ "verticalZoom",	kWindowVerticalZoomAttribute		     },
	{ "collapseBox",	kWindowCollapseBoxAttribute		     },
	{ "resizable",		kWindowResizableAttribute		     },
	{ "sideTitlebar",	kWindowSideTitlebarAttribute		     },
	{ "toolbarButton",	kWindowToolbarButtonAttribute		     },
	{ "unifiedTitleAndToolbar", kWindowUnifiedTitleAndToolbarAttribute   },
	{ "metal",		kWindowMetalAttribute			     },
	{ "noTitleBar",		kWindowNoTitleBarAttribute		     },
	{ "texturedSquareCorners", kWindowTexturedSquareCornersAttribute     },
	{ "metalNoContentSeparator", kWindowMetalNoContentSeparatorAttribute },
	{ "doesNotCycle",	kWindowDoesNotCycleAttribute		     },
	{ "noUpdates",		kWindowNoUpdatesAttribute		     },
	{ "noActivates",	kWindowNoActivatesAttribute		     },
	{ "opaqueForEvents",	kWindowOpaqueForEventsAttribute		     },
	{ "noShadow",		kWindowNoShadowAttribute		     },
	{ "hideOnSuspend",	kWindowHideOnSuspendAttribute		     },
	{ "hideOnFullScreen",	kWindowHideOnFullScreenAttribute	     },
	{ "inWindowMenu",	kWindowInWindowMenuAttribute		     },
	{ "liveResize",		kWindowLiveResizeAttribute		     },
	{ "ignoreClicks",	kWindowIgnoreClicksAttribute		     },
	{ "noConstrain",	kWindowNoConstrainAttribute		     },
	{ "doesNotHide",	tkWindowDoesNotHideAttribute		     },
	{ "canJoinAllSpaces",	tkCanJoinAllSpacesAttribute		     },
	{ "moveToActiveSpace",	tkMoveToActiveSpaceAttribute		     },
	{ "nonActivating",	tkNonactivatingPanelAttribute		     },
	{ "hud",		tkHUDWindowAttribute			     },
	{ "black",		0			                     },
	{ "dark",		0			                     },
	{ "light",		0			                     },
	{ "gray",		0			                     },
	{ "red",		0 			                     },
	{ "green",		0                			     },
	{ "blue",		0           			             },
	{ "cyan",		0			                     },
	{ "yellow",		0			                     },
	{ "magenta",		0  			                     },
	{ "orange",		0 			                     },
	{ "purple",		0			                     },
	{ "brown",		0			                     },
	{ "clear",		0			                     },
	{ "opacity",		0			                     },
	{ "fullscreen",         0			                     },
	{ NULL }
    };

    int index, i;
    WmInfo *wmPtr = winPtr->wmInfoPtr;

    if (objc == 3) {
	Tcl_Obj *attributeList, *newResult = NULL;
	UInt64 attributes;

	for (i = 0; classMap[i].strValue != NULL; i++) {
	    if (wmPtr->macClass == classMap[i].intValue) {
		newResult = Tcl_NewStringObj(classMap[i].strValue, -1);
		break;
	    }
	}
	if (newResult == NULL) {
	    Tcl_Panic("invalid class");
	}


	attributeList = Tcl_NewListObj(0, NULL);
	attributes = wmPtr->attributes;

	for (i = 0; compositeAttrMap[i].strValue != NULL; i++) {
	    UInt64 intValue = compositeAttrMap[i].intValue;

	    if (intValue && (attributes & intValue) == intValue) {
		Tcl_ListObjAppendElement(interp, attributeList,
			Tcl_NewStringObj(compositeAttrMap[i].strValue,
			-1));
		attributes &= ~intValue;
		break;
	    }
	}
	for (i = 0; attrMap[i].strValue != NULL; i++) {
	    if (attributes & attrMap[i].intValue) {
		Tcl_ListObjAppendElement(interp, attributeList,
			Tcl_NewStringObj(attrMap[i].strValue, -1));
	    }
	}
	Tcl_ListObjAppendElement(interp, newResult, attributeList);
	Tcl_SetObjResult(interp, newResult);
    } else {
	int attrObjc;
	Tcl_Obj **attrObjv = NULL;
	WindowClass macClass;
	UInt64 oldAttributes = wmPtr->attributes;
	int oldFlags = wmPtr->flags;

	if (Tcl_GetIndexFromObjStruct(interp, objv[3], classMap,
		sizeof(struct StrIntMap), "class", 0, &index) != TCL_OK) {
	    goto badClassAttrs;
	}
	macClass = classMap[index].intValue;
	if (objc == 5) {
	    if (Tcl_ListObjGetElements(interp, objv[4], &attrObjc, &attrObjv)
		    != TCL_OK) {
		goto badClassAttrs;
	    }
	    wmPtr->attributes = kWindowNoAttributes;
	    for (i = 0; i < attrObjc; i++) {
		if (Tcl_GetIndexFromObjStruct(interp, attrObjv[i],
			compositeAttrMap, sizeof(struct StrIntMap),
			"attribute", 0, &index) == TCL_OK) {
		    wmPtr->attributes |= compositeAttrMap[index].intValue;
		} else if (Tcl_GetIndexFromObjStruct(interp, attrObjv[i],
			attrMap, sizeof(struct StrIntMap),
			"attribute", 0, &index) == TCL_OK) {
		    Tcl_ResetResult(interp);
		    wmPtr->attributes |= attrMap[index].intValue;
		} else {
		    goto badClassAttrs;
		}
	    }
	} else {
	    wmPtr->attributes = macClassAttrs[macClass].defaultAttrs;
	}
	wmPtr->attributes &= (tkAlwaysValidAttributes |
		macClassAttrs[macClass].validAttrs);
	wmPtr->flags |= macClassAttrs[macClass].flags;
	wmPtr->macClass = macClass;

	ApplyWindowAttributeFlagChanges(winPtr, NULL, oldAttributes, oldFlags,
		0, 1);

	return TCL_OK;

    badClassAttrs:
	wmPtr->attributes = oldAttributes;
	return TCL_ERROR;
    }


    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpMakeMenuWindow --
 *
 *	Configure the window to be either a undecorated pull-down (or pop-up)
 *	menu, or as a toplevel floating menu (palette).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the style bit used to create a new Mac toplevel.
 *
 *----------------------------------------------------------------------
 */

void
TkpMakeMenuWindow(
    Tk_Window tkwin,		/* New window. */
    int transient)		/* 1 means menu is only posted briefly as a
				 * popup or pulldown or cascade. 0 means menu
				 * is always visible, e.g. as a floating
				 * menu. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;

    if (transient) {
	winPtr->wmInfoPtr->macClass = kSimpleWindowClass;
	winPtr->wmInfoPtr->attributes = kWindowNoActivatesAttribute;
    } else {
	winPtr->wmInfoPtr->macClass = kFloatingWindowClass;
	winPtr->wmInfoPtr->attributes = kWindowStandardFloatingAttributes;
	winPtr->wmInfoPtr->flags |= WM_WIDTH_NOT_RESIZABLE;
	winPtr->wmInfoPtr->flags |= WM_HEIGHT_NOT_RESIZABLE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXMakeRealWindowExist --
 *
 *	This function finally creates the real Macintosh window that the Mac
 *	actually understands.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new Macintosh toplevel is created.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXMakeRealWindowExist(
    TkWindow *winPtr)		/* Tk window. */
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    MacDrawable *macWin;

    if (TkMacOSXHostToplevelExists(winPtr)) {
	return;
    }

    macWin = (MacDrawable *) winPtr->window;

    /*
     * If this is embedded, make sure its container's toplevel exists,
     * then return...
     */

    if (Tk_IsEmbedded(winPtr)) {
	TkWindow *contWinPtr = TkpGetOtherWindow(winPtr);

	if (contWinPtr != NULL) {
	    TkMacOSXMakeRealWindowExist(
		    contWinPtr->privatePtr->toplevel->winPtr);
	    macWin->flags |= TK_HOST_EXISTS;
	    return;
	}

	if (tkMacOSXEmbedHandler == NULL) {
	    Tcl_Panic("TkMacOSXMakeRealWindowExist could not find container");
	}
	if (tkMacOSXEmbedHandler->containerExistProc &&
		tkMacOSXEmbedHandler->containerExistProc((Tk_Window) winPtr)
		!= TCL_OK) {
	    Tcl_Panic("ContainerExistProc could not make container");
	}
	return;

	/*
	 * TODO: Here we should handle out of process embedding.
	 */
    }
    WindowClass macClass = wmPtr->macClass;
    wmPtr->attributes &= (tkAlwaysValidAttributes |
	    macClassAttrs[macClass].validAttrs);
    wmPtr->flags |= macClassAttrs[macClass].flags |
	    ((wmPtr->attributes & kWindowResizableAttribute) ? 0 :
	    WM_WIDTH_NOT_RESIZABLE|WM_HEIGHT_NOT_RESIZABLE);
    UInt64 attributes = (wmPtr->attributes &
	    ~macClassAttrs[macClass].forceOffAttrs) |
	    macClassAttrs[macClass].forceOnAttrs;
    NSUInteger styleMask = macClassAttrs[macClass].styleMask |
	((attributes & kWindowNoTitleBarAttribute) ? 0 : NSTitledWindowMask) |
	((attributes & kWindowCloseBoxAttribute) ? NSClosableWindowMask : 0) |
	((attributes & kWindowCollapseBoxAttribute) ?
		NSMiniaturizableWindowMask : 0) |
	((attributes & kWindowResizableAttribute) ? NSResizableWindowMask : 0) |
	((attributes & kWindowMetalAttribute) ?
		NSTexturedBackgroundWindowMask : 0) |
	((attributes & kWindowUnifiedTitleAndToolbarAttribute) ?
		NSUnifiedTitleAndToolbarWindowMask : 0) |
	((attributes & kWindowSideTitlebarAttribute) ? 1 << 9 : 0) |
	(attributes >> WM_NSMASK_SHIFT);
    Class winClass = (macClass == kDrawerWindowClass ? [NSDrawerWindow class] :
	    (styleMask & (NSUtilityWindowMask|NSDocModalWindowMask|
	    NSNonactivatingPanelMask|NSHUDWindowMask)) ? [NSPanel class] :
	    [TKWindow class]);
    NSRect structureRect = [winClass frameRectForContentRect:NSZeroRect
	    styleMask:styleMask];
    NSRect contentRect = NSMakeRect(5 - structureRect.origin.x,
	    tkMacOSXZeroScreenHeight - (tkMacOSXZeroScreenTop + 5 +
	    structureRect.origin.y + structureRect.size.height + 200), 200, 200);
    NSWindow *window = [[winClass alloc] initWithContentRect:contentRect
	    styleMask:styleMask backing:NSBackingStoreBuffered defer:YES];
    if (!window) {
    	Tcl_Panic("couldn't allocate new Mac window");
    }
    TKContentView *contentView = [[TKContentView alloc]
				     initWithFrame:NSZeroRect];
    [window setContentView:contentView];
    [contentView release];
    [window setDelegate:NSApp];
    [window setAcceptsMouseMovedEvents:YES];
    [window setReleasedWhenClosed:NO];
    [window setAutodisplay:NO];
    if (styleMask & NSUtilityWindowMask) {
	[(NSPanel*)window setFloatingPanel:YES];
    }
    if ((styleMask & (NSTexturedBackgroundWindowMask|NSHUDWindowMask)) &&
	    !(styleMask & NSDocModalWindowMask)) {
        /*
	 * Workaround for [Bug 2824538]: Texured windows are draggable
	 *                               from opaque content.
	 */
	[window setMovableByWindowBackground:NO];
    }


    /* Set background color and opacity of window if those flags are set.  */
    if (colorName != NULL) {
    	[window setBackgroundColor: colorName];
    }

    if (opaqueTag) {
#ifdef TK_GOT_AT_LEAST_SNOW_LEOPARD
    	[window setOpaque: opaqueTag];
#else
	[window setOpaque: YES];
#endif
    }

    [window setDocumentEdited:NO];
    wmPtr->window = window;
    macWin->view = window.contentView;
    TkMacOSXApplyWindowAttributes(winPtr, window);

    NSRect geometry = InitialWindowBounds(winPtr, window);
    geometry.size.width +=  structureRect.size.width;
    geometry.size.height += structureRect.size.height;
    geometry.origin.y = tkMacOSXZeroScreenHeight - (geometry.origin.y +
	    geometry.size.height);
    [window setFrame:geometry display:NO];
    TkMacOSXRegisterOffScreenWindow((Window) macWin, window);
    macWin->flags |= TK_HOST_EXISTS;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXRegisterOffScreenWindow --
 *
 *	This function adds the passed in Off Screen Port to the hash table
 *	that maps Mac windows to root X windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An entry is added to the windowTable hash table.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXRegisterOffScreenWindow(
    Window window,		/* Window structure. */
    void *portPtr)		/* Pointer to a Mac Window. */
{
    Tcl_HashEntry *valueHashPtr;
    int isNew;

    if (!windowHashInit) {
	Tcl_InitHashTable(&windowTable, TCL_ONE_WORD_KEYS);
	windowHashInit = true;
    }
    valueHashPtr = Tcl_CreateHashEntry(&windowTable, (char *) portPtr, &isNew);
    if (!isNew) {
	Tcl_Panic("Same macintosh window allocated twice!");
    }
    Tcl_SetHashValue(valueHashPtr, window);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXUnregisterMacWindow --
 *
 *	Given a macintosh port window, this function removes the association
 *	between this window and the root X window that Tk cares about.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An entry is removed from the windowTable hash table.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXUnregisterMacWindow(
    void *macWinPtr)	/* Reference to a Mac Window */
{
    Tcl_HashEntry *entryPtr;

    if (!windowHashInit) {
	Tcl_Panic("TkMacOSXUnregisterMacWindow: unmapping before inited");
    }
    entryPtr = Tcl_FindHashEntry(&windowTable, macWinPtr);
    if (!entryPtr) {
	TkMacOSXDbgMsg("Failed to find window %p", macWinPtr);
    } else {
	Tcl_DeleteHashEntry(entryPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXSetScrollbarGrow --
 *
 *	Sets a flag for a toplevel window indicating that the passed Tk
 *	scrollbar window will display the grow region for the toplevel window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A flag is set int windows toplevel parent.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXSetScrollbarGrow(
    TkWindow *winPtr,		/* Tk scrollbar window. */
    int flag)			/* Boolean value true or false. */
{
    if (flag) {
	winPtr->privatePtr->toplevel->flags |= TK_SCROLLBAR_GROW;
	winPtr->privatePtr->toplevel->winPtr->wmInfoPtr->scrollWinPtr = winPtr;
    } else if (winPtr->privatePtr->toplevel->winPtr->wmInfoPtr->scrollWinPtr
	    == winPtr) {
	winPtr->privatePtr->toplevel->flags &= ~TK_SCROLLBAR_GROW;
	winPtr->privatePtr->toplevel->winPtr->wmInfoPtr->scrollWinPtr = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmFocusToplevel --
 *
 *	This is a utility procedure invoked by focus-management code. It
 *	exists because of the extra wrapper windows that exist under Unix; its
 *	job is to map from wrapper windows to the corresponding toplevel
 *	windows. On PCs and Macs there are no wrapper windows so no mapping is
 *	necessary; this procedure just determines whether a window is a
 *	toplevel or not.
 *
 * Results:
 *	If winPtr is a toplevel window, returns the pointer to the window;
 *	otherwise returns NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkWindow *
TkWmFocusToplevel(
    TkWindow *winPtr)		/* Window that received a focus-related
				 * event. */
{
    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	return NULL;
    }
    return winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpGetWrapperWindow --
 *
 *	This is a utility procedure invoked by focus-management code. It maps
 *	to the wrapper for a top-level, which is just the same as the
 *	top-level on Macs and PCs.
 *
 * Results:
 *	If winPtr is a toplevel window, returns the pointer to the window;
 *	otherwise returns NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkWindow *
TkpGetWrapperWindow(
    TkWindow *winPtr)		/* Window that received a focus-related
				 * event. */
{
    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	return NULL;
    }
    return winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpWmSetState --
 *
 *	Sets the window manager state for the wrapper window of a given
 *	toplevel window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May maximize, minimize, restore, or withdraw a window.
 *
 *----------------------------------------------------------------------
 */

void
TkpWmSetState(
    TkWindow *winPtr,		/* Toplevel window to operate on. */
    int state)			/* One of IconicState, ZoomState, NormalState,
				 * or WithdrawnState. */
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    NSWindow *macWin;

    wmPtr->hints.initial_state = state;
    if (wmPtr->flags & WM_NEVER_MAPPED) {
	return;
    }

    macWin = TkMacOSXDrawableWindow(winPtr->window);

    if (state == WithdrawnState) {
	Tk_UnmapWindow((Tk_Window) winPtr);
    } else if (state == IconicState) {
	/*
	 * The window always gets unmapped. If we can show the icon version of
	 * the window we also collapse it.
	 */

	if (macWin && ([macWin styleMask] & NSMiniaturizableWindowMask) &&
		![macWin isMiniaturized]) {
	    [macWin miniaturize:NSApp];
	}
	Tk_UnmapWindow((Tk_Window) winPtr);
    } else if (state == NormalState || state == ZoomState) {
	Tk_MapWindow((Tk_Window) winPtr);
	if (macWin && ([macWin styleMask] & NSMiniaturizableWindowMask) &&
		[macWin isMiniaturized]) {
	    [macWin deminiaturize:NSApp];
	}
	TkMacOSXZoomToplevel(macWin, state == NormalState ? inZoomIn :
		inZoomOut);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpIsWindowFloating --
 *
 *	Returns 1 if a window is floating, 0 otherwise.
 *
 * Results:
 *	1 or 0 depending on window's floating attribute.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkpIsWindowFloating(
    void *wRef)
{
    return [(NSWindow *)wRef level] == kCGFloatingWindowLevel;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXWindowClass --
 *
 *	Returns OS X window class of window
 *
 * Results:
 *	1 or 0 depending on window's floating attribute.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE WindowClass
TkMacOSXWindowClass(
    TkWindow *winPtr)
{
    return winPtr->wmInfoPtr->macClass;
}

/*
 *--------------------------------------------------------------
 *
 * TkMacOSXWindowOffset --
 *
 *	Determines the x and y offset from the orgin of the toplevel window
 *	dressing (the structure region, i.e. title bar) and the orgin of the
 *	content area.
 *
 * Results:
 *	The x & y offset in pixels.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXWindowOffset(
    void *wRef,
    int *xOffset,
    int *yOffset)
{
    TkWindow *winPtr = TkMacOSXGetTkWindow(wRef);

    if (winPtr && winPtr->wmInfoPtr) {
	*xOffset = winPtr->wmInfoPtr->xInParent;
	*yOffset = winPtr->wmInfoPtr->yInParent;
    } else {
	*xOffset = 0;
	*yOffset = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpGetMS --
 *
 *	Return a relative time in milliseconds. It doesn't matter when the
 *	epoch was.
 *
 * Results:
 *	Number of milliseconds.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned long
TkpGetMS(void)
{
    Tcl_Time now;

    Tcl_GetTime(&now);
    return (long) now.sec * 1000 + now.usec / 1000;
}

/*
 *----------------------------------------------------------------------
 *
 * XSetInputFocus --
 *
 *	Change the focus window for the application.
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
XSetInputFocus(
    Display* display,
    Window focus,
    int revert_to,
    Time time)
{
    /*
     * Don't need to do a thing. Tk manages the focus for us.
     */
}

/*
 *----------------------------------------------------------------------
 *
 * TkpChangeFocus --
 *
 *	This procedure is a stub on the Mac because we always own the focus if
 *	we are a front most application.
 *
 * Results:
 *	The return value is the serial number of the command that changed the
 *	focus. It may be needed by the caller to filter out focus change
 *	events that were queued before the command. If the procedure doesn't
 *	actually change the focus then it returns 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkpChangeFocus(
    TkWindow *winPtr,		/* Window that is to receive the X focus. */
    int force)			/* Non-zero means claim the focus even if it
				 * didn't originally belong to topLevelPtr's
				 * application. */
{
    if (winPtr->atts.override_redirect) {
	return 0;
    }

    if (Tk_IsTopLevel(winPtr) && !Tk_IsEmbedded(winPtr) ){
    	NSWindow *win = TkMacOSXDrawableWindow(winPtr->window);
    	TkWmRestackToplevel(winPtr, Above, NULL);
    	if (force ) {
    	    [NSApp activateIgnoringOtherApps:YES];
    	}
	if ( win && [win canBecomeKeyWindow] ) {
	    [win makeKeyAndOrderFront:NSApp];
	}
    }
	
    /*
     * Remember the current serial number for the X server and issue a dummy
     * server request. This marks the position at which we changed the focus,
     * so we can distinguish FocusIn and FocusOut events on either side of the
     * mark.
     */

    return NextRequest(winPtr->display);
}

/*
 *----------------------------------------------------------------------
 *
 * WmStackorderToplevelWrapperMap --
 *
 *	This procedure will create a table that maps the reparent wrapper X id
 *	for a toplevel to the TkWindow structure that it wraps. Tk keeps track
 *	of a mapping from the window X id to the TkWindow structure but that
 *	does us no good here since we only get the X id of the wrapper window.
 *	Only those toplevel windows that are mapped have a position in the
 *	stacking order.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds entries to the passed hashtable.
 *
 *----------------------------------------------------------------------
 */

static void
WmStackorderToplevelWrapperMap(
    TkWindow *winPtr,		/* TkWindow to recurse on */
    Display *display,		/* X display of parent window */
    Tcl_HashTable *table)	/* Maps mac window to TkWindow */
{
    TkWindow *childPtr;
    Tcl_HashEntry *hPtr;
    int newEntry;

    if (Tk_IsMapped(winPtr) && Tk_IsTopLevel(winPtr)
	    && (winPtr->display == display)) {
	hPtr = Tcl_CreateHashEntry(table,
		(char*) TkMacOSXDrawableWindow(winPtr->window), &newEntry);
	Tcl_SetHashValue(hPtr, winPtr);
    }

    for (childPtr = winPtr->childList; childPtr != NULL;
	    childPtr = childPtr->nextPtr) {
	WmStackorderToplevelWrapperMap(childPtr, display, table);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmStackorderToplevel --
 *
 *	This procedure returns the stack order of toplevel windows.
 *
 * Results:
 *	An array of pointers to tk window objects in stacking order or else
 *	NULL if there was an error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkWindow **
TkWmStackorderToplevel(
    TkWindow *parentPtr)	/* Parent toplevel window. */
{
    TkWindow *childWinPtr, **windows, **windowPtr;
    Tcl_HashTable table;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;

    /*
     * Map mac windows to a TkWindow of the wrapped toplevel.
     */

    Tcl_InitHashTable(&table, TCL_ONE_WORD_KEYS);
    WmStackorderToplevelWrapperMap(parentPtr, parentPtr->display, &table);

    windows = (TkWindow**)ckalloc((table.numEntries+1) * sizeof(TkWindow *));

    /*
     * Special cases: If zero or one toplevels were mapped there is no need to
     * enumerate Windows.
     */

    switch (table.numEntries) {
    case 0:
	windows[0] = NULL;
	goto done;
    case 1:
	hPtr = Tcl_FirstHashEntry(&table, &search);
	windows[0] = Tcl_GetHashValue(hPtr);
	windows[1] = NULL;
	goto done;
    }

    NSArray *macWindows = [NSApp orderedWindows];
    NSInteger windowCount = [macWindows count];

    if (!windowCount) {
	ckfree((char *)windows);
	windows = NULL;
    } else {
	windowPtr = windows + table.numEntries;
	*windowPtr-- = NULL;
	for (NSWindow *w in macWindows) {
	    hPtr = Tcl_FindHashEntry(&table, (char*) w);
	    if (hPtr != NULL) {
		childWinPtr = Tcl_GetHashValue(hPtr);
		*windowPtr-- = childWinPtr;
	    }
	}
	if (windowPtr != windows-1) {
	    Tcl_Panic("num matched toplevel windows does not equal num "
		      "children");
	}
    }

  done:
    Tcl_DeleteHashTable(&table);
    return windows;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXApplyWindowAttributes --
 *
 *	This procedure applies all window attributes to the NSWindow.
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
TkMacOSXApplyWindowAttributes(
    TkWindow *winPtr,
    NSWindow *macWindow)
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    ApplyWindowAttributeFlagChanges(winPtr, macWindow, 0, 0, 0, 1);
    if (wmPtr->master != None || winPtr->atts.override_redirect) {
	ApplyMasterOverrideChanges(winPtr, macWindow);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ApplyWindowAttributeFlagChanges --
 *
 *	This procedure applies window attribute and flag changes.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
ApplyWindowAttributeFlagChanges(
    TkWindow *winPtr,
    NSWindow *macWindow,
    UInt64 oldAttributes,
    int oldFlags,
    int create,
    int initial)
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    UInt64 newAttributes = ForceAttributes(wmPtr->attributes, wmPtr->macClass);
    UInt64 changedAttributes = newAttributes ^ ForceAttributes(oldAttributes,
	    wmPtr->macClass);

    if (changedAttributes || wmPtr->flags != oldFlags || initial) {
	if (!macWindow) {
	    if (winPtr->window == None) {
		if (create) {
		    Tk_MakeWindowExist((Tk_Window) winPtr);
		} else {
		    return;
		}
	    }
	    if (!TkMacOSXHostToplevelExists(winPtr)) {
		if (create) {
		    TkMacOSXMakeRealWindowExist(winPtr);
		} else {
		    return;
		}
	    }
	    macWindow = TkMacOSXDrawableWindow(winPtr->window);
	}
	if ((changedAttributes & kWindowCloseBoxAttribute) || initial) {
	    [[macWindow standardWindowButton:NSWindowCloseButton]
		    setEnabled:!!(newAttributes & kWindowCloseBoxAttribute)];
	}
	if ((changedAttributes & kWindowCollapseBoxAttribute) || initial) {
	    [[macWindow standardWindowButton:NSWindowMiniaturizeButton]
		    setEnabled:!!(newAttributes & kWindowCollapseBoxAttribute)];
	}
	if ((changedAttributes & (kWindowResizableAttribute |
		kWindowFullZoomAttribute)) || initial) {
	    [macWindow setShowsResizeIndicator:
		    !!(newAttributes & kWindowResizableAttribute)];
	    [[macWindow standardWindowButton:NSWindowZoomButton]
		    setEnabled:(newAttributes & kWindowResizableAttribute) &&
		    (newAttributes & kWindowFullZoomAttribute)];
	    if (newAttributes & kWindowResizableAttribute) {
		wmPtr->flags &= ~(WM_WIDTH_NOT_RESIZABLE |
			WM_HEIGHT_NOT_RESIZABLE);
	    } else {
		wmPtr->flags |= (WM_WIDTH_NOT_RESIZABLE |
			WM_HEIGHT_NOT_RESIZABLE);
	    }
	    WmUpdateGeom(wmPtr, winPtr);
	}
	if ((changedAttributes & kWindowToolbarButtonAttribute) || initial) {
	    [macWindow setShowsToolbarButton:
		    !!(newAttributes & kWindowToolbarButtonAttribute)];
	    if ((newAttributes & kWindowToolbarButtonAttribute) &&
		    ![macWindow toolbar]) {
		NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@""];
		[toolbar setVisible:NO];
		[macWindow setToolbar:toolbar];
		[toolbar release];
		NSCell *toolbarButtonCell = [[macWindow standardWindowButton:
			NSWindowToolbarButton] cell];
		[toolbarButtonCell setTarget:[macWindow contentView]];
		[toolbarButtonCell setAction:@selector(tkToolbarButton:)];
	    }
	}
	if ((changedAttributes & kWindowNoShadowAttribute) || initial) {
	    [macWindow setHasShadow:
		    !(newAttributes & kWindowNoShadowAttribute)];
	}
	if ((changedAttributes & kWindowHideOnSuspendAttribute) || initial) {
	    [macWindow setHidesOnDeactivate:
		    !!(newAttributes & kWindowHideOnSuspendAttribute)];
	}
	if ((changedAttributes & kWindowInWindowMenuAttribute) || initial) {
	    [macWindow setExcludedFromWindowsMenu:
		    !(newAttributes & kWindowInWindowMenuAttribute)];
	}
	if ((changedAttributes & kWindowIgnoreClicksAttribute) || initial) {
	    [macWindow setIgnoresMouseEvents:
		    !!(newAttributes & kWindowIgnoreClicksAttribute)];
	}
	if ((changedAttributes & tkWindowDoesNotHideAttribute) || initial) {
	    [macWindow setCanHide:
		    !(newAttributes & tkWindowDoesNotHideAttribute)];
	}
	if ((changedAttributes & (kWindowDoesNotCycleAttribute |
		tkCanJoinAllSpacesAttribute | tkMoveToActiveSpaceAttribute)) ||
		initial) {
	    NSWindowCollectionBehavior b = NSWindowCollectionBehaviorDefault;
	    if (newAttributes & tkCanJoinAllSpacesAttribute) {
		b |= NSWindowCollectionBehaviorCanJoinAllSpaces;
	    } else if (newAttributes & tkMoveToActiveSpaceAttribute) {
		b |= NSWindowCollectionBehaviorMoveToActiveSpace;
	    }
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	    if (newAttributes & kWindowDoesNotCycleAttribute) {
		b |= NSWindowCollectionBehaviorIgnoresCycle;
	    } else {
		b |= NSWindowCollectionBehaviorParticipatesInCycle;
	    }
#endif
	    [macWindow setCollectionBehavior:b];
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
	    if (((changedAttributes & kWindowDoesNotCycleAttribute) || initial)
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
		    && tkMacOSXMacOSXVersion < 1060
#endif
	    ) {
		[macWindow setCanCycle:
			!(newAttributes & kWindowDoesNotCycleAttribute)];
	    }
#endif
	}
	if ((wmPtr->flags & WM_TOPMOST) != (oldFlags & WM_TOPMOST)) {
	    [macWindow setLevel:(wmPtr->flags & WM_TOPMOST) ?
		    kCGUtilityWindowLevel : ([macWindow isKindOfClass:
		    [NSPanel class]] && [macWindow isFloatingPanel] ?
		    kCGFloatingWindowLevel : kCGNormalWindowLevel)];
	}

	/*
	 * The change of window class/attributes might have changed the window
	 * structure widths:
	 */

	NSRect structureRect = [macWindow frameRectForContentRect:NSZeroRect];
	wmPtr->xInParent = -structureRect.origin.x;
	wmPtr->yInParent = structureRect.origin.y + structureRect.size.height;
	wmPtr->parentWidth = winPtr->changes.width + structureRect.size.width;
	wmPtr->parentHeight = winPtr->changes.height + structureRect.size.height;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ApplyMasterOverrideChanges --
 *
 *	This procedure applies changes to override_redirect or master.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
ApplyMasterOverrideChanges(
    TkWindow *winPtr,
    NSWindow *macWindow)
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    UInt64 oldAttributes = wmPtr->attributes;
    int oldFlags = wmPtr->flags;

    /*
     * FIX: We need an UpdateWrapper equivalent to make this 100% correct
     */

    if (winPtr->atts.override_redirect) {
	if (wmPtr->macClass == kDocumentWindowClass) {
	    wmPtr->macClass = kSimpleWindowClass;
	    wmPtr->attributes = macClassAttrs[kSimpleWindowClass].defaultAttrs;
	}
	wmPtr->attributes |= kWindowNoActivatesAttribute;
    } else {
	if (wmPtr->macClass == kSimpleWindowClass &&
		oldAttributes == kWindowNoActivatesAttribute) {
	    wmPtr->macClass = kDocumentWindowClass;
	    wmPtr->attributes =
		    macClassAttrs[kDocumentWindowClass].defaultAttrs;
	}
	wmPtr->attributes &= ~kWindowNoActivatesAttribute;
    }
    if (!macWindow && winPtr->window != None &&
	    TkMacOSXHostToplevelExists(winPtr)) {
	macWindow = TkMacOSXDrawableWindow(winPtr->window);
    }
    if (macWindow) {
	if (winPtr->atts.override_redirect && wmPtr->master != None) {
	    wmPtr->flags |= WM_TOPMOST;
	} else {
	    wmPtr->flags &= ~WM_TOPMOST;
	}
	NSWindow *parentWindow = [macWindow parentWindow];
	if (wmPtr->master != None) {
	    TkDisplay *dispPtr = TkGetDisplayList();
	    TkWindow *masterWinPtr = (TkWindow *)
		    Tk_IdToWindow(dispPtr->display, wmPtr->master);

	    if (masterWinPtr && masterWinPtr->window != None &&
		    TkMacOSXHostToplevelExists(masterWinPtr)) {
		NSWindow *masterMacWin =
			TkMacOSXDrawableWindow(masterWinPtr->window);

		if (masterMacWin && masterMacWin != parentWindow &&
			(winPtr->flags & TK_MAPPED)) {
		    if (parentWindow) {
			[parentWindow removeChildWindow:macWindow];
		    }
		    [masterMacWin addChildWindow:macWindow
			    ordered:NSWindowAbove];
		    if (wmPtr->flags & WM_TOPMOST) {
			[macWindow setLevel:kCGUtilityWindowLevel];
		    }
		}
	    }
	} else if (parentWindow) {
	    [parentWindow removeChildWindow:macWindow];
	}
	ApplyWindowAttributeFlagChanges(winPtr, macWindow, oldAttributes,
		oldFlags, 0, 0);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXMakeFullscreen --
 *
 *	This procedure sets a fullscreen window to the size of the screen.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkMacOSXMakeFullscreen(
    TkWindow *winPtr,
    NSWindow *window,
    int fullscreen,
    Tcl_Interp *interp)
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    int result = TCL_OK, wasFullscreen = (wmPtr->flags & WM_FULLSCREEN);

#ifdef TK_GOT_AT_LEAST_SNOW_LEOPARD
    static unsigned long prevMask = 0, prevPres = 0;
#endif /*TK_GOT_AT_LEAST_SNOW_LEOPARD*/

    if (fullscreen) {
	int screenWidth =  WidthOfScreen(Tk_Screen(winPtr));
	int screenHeight = HeightOfScreen(Tk_Screen(winPtr));

	/*
	 * Check max width and height if set by the user.
	 */

	if ((wmPtr->maxWidth > 0 && wmPtr->maxWidth < screenWidth)
		|| (wmPtr->maxHeight > 0 && wmPtr->maxHeight < screenHeight)) {
	    if (interp) {
		Tcl_AppendResult(interp,
			"can't set fullscreen attribute for \"",
			winPtr->pathName,
			"\": max width/height is too small", NULL);
	    }
	    result = TCL_ERROR;
	    wmPtr->flags &= ~WM_FULLSCREEN;
	} else {
	    NSRect bounds = [window contentRectForFrameRect:[window frame]];
	    NSRect screenBounds = NSMakeRect(0, 0, screenWidth, screenHeight);

    	    if (!NSEqualRects(bounds, screenBounds) && !wasFullscreen) {
		wmPtr->configX = wmPtr->x;
		wmPtr->configY = wmPtr->y;
		wmPtr->configAttributes = wmPtr->attributes;
		wmPtr->attributes &= ~kWindowResizableAttribute;
		ApplyWindowAttributeFlagChanges(winPtr, window,
			wmPtr->configAttributes, wmPtr->flags, 1, 0);
		wmPtr->flags |= WM_SYNC_PENDING;
		[window setFrame:[window frameRectForContentRect:
					   screenBounds] display:YES];

		wmPtr->flags &= ~WM_SYNC_PENDING;
	    }
	    wmPtr->flags |= WM_FULLSCREEN;
	}

#ifdef TK_GOT_AT_LEAST_SNOW_LEOPARD
	/*
	 * We can't set these features on Leopard or earlier, as they don't
	 * exist (neither options nor API that uses them). This formally means
	 * that there's a bug with full-screen windows with Tk on old OSX, but
	 * it isn't worth blocking a build just for this.
	 */

	prevMask = [window styleMask];
	prevPres = [NSApp presentationOptions];
	[window setStyleMask: NSBorderlessWindowMask];
	[NSApp setPresentationOptions: NSApplicationPresentationAutoHideDock
	     | NSApplicationPresentationAutoHideMenuBar];
        
#endif /*TK_GOT_AT_LEAST_SNOW_LEOPARD*/
    } else {
	wmPtr->flags &= ~WM_FULLSCREEN;
#ifdef TK_GOT_AT_LEAST_SNOW_LEOPARD
	[NSApp setPresentationOptions: prevPres];
	[window setStyleMask: prevMask];
#endif /*TK_GOT_AT_LEAST_SNOW_LEOPARD*/
    }

    if (wasFullscreen && !(wmPtr->flags & WM_FULLSCREEN)) {
	UInt64 oldAttributes = wmPtr->attributes;
	NSRect bounds = NSMakeRect(wmPtr->configX, tkMacOSXZeroScreenHeight -
		(wmPtr->configY + wmPtr->yInParent + wmPtr->configHeight),
		wmPtr->xInParent + wmPtr->configWidth,
		wmPtr->yInParent + wmPtr->configHeight);

	wmPtr->attributes |= wmPtr->configAttributes &
		kWindowResizableAttribute;
	ApplyWindowAttributeFlagChanges(winPtr, window, oldAttributes,
		wmPtr->flags, 1, 0);
	wmPtr->flags |= WM_SYNC_PENDING;
	[window setFrame:[window frameRectForContentRect:bounds] display:YES];
	wmPtr->flags &= ~WM_SYNC_PENDING;
    }
    return result;
}



/*
 *----------------------------------------------------------------------
 *
 * GetMinSize --
 *
 *	This function computes the current minWidth and minHeight values for a
 *	window, taking into account the possibility that they may be
 *	defaulted.
 *
 * Results:
 *	The values at *minWidthPtr and *minHeightPtr are filled in with the
 *	minimum allowable dimensions of wmPtr's window, in grid units. If the
 *	requested minimum is smaller than the system required minimum, then
 *	this function computes the smallest size that will satisfy both the
 *	system and the grid constraints.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
GetMinSize(
    TkWindow *winPtr,		/* Toplevel window to operate on. */
    int *minWidthPtr,		/* Where to store the current minimum width of
				 * the window. */
    int *minHeightPtr)		/* Where to store the current minimum height
				 * of the window. */
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    int minWidth = 1, minHeight = 1;

    /*
     * Compute the minimum width & height by taking the default client size and
     * rounding it up to the nearest grid unit. Return the greater of the
     * default minimum and the specified minimum.
     */

    switch (wmPtr->macClass) {
    case kDocumentWindowClass:
    case kMovableAlertWindowClass:
    case kMovableModalWindowClass:
	minWidth = 72;
	if (wmPtr->attributes & kWindowResizableAttribute) {
	    minHeight = 15;
	}
	if (wmPtr->attributes & kWindowToolbarButtonAttribute) {
	    minWidth += 29;
	}
	break;
    case kFloatingWindowClass:
    case kUtilityWindowClass:
	minWidth = 59;
	if (wmPtr->attributes & kWindowResizableAttribute) {
	    minHeight = 11;
	}
	if (wmPtr->attributes & kWindowSideTitlebarAttribute) {
	    int tmp = minWidth;

	    minWidth = minHeight;
	    minHeight = tmp;
	} else if (wmPtr->attributes & kWindowToolbarButtonAttribute) {
	    minWidth += 29;
	}
	break;
    default:
	if (wmPtr->attributes & kWindowResizableAttribute) {
	    minWidth = 15;
	    minHeight = 15;
	}
	break;
    }

    if (wmPtr->gridWin != NULL) {
	int base = winPtr->reqWidth - (wmPtr->reqGridWidth * wmPtr->widthInc);

	if (base < 0) {
	    base = 0;
	}
	minWidth = ((minWidth - base) + wmPtr->widthInc-1)/wmPtr->widthInc;
	base = winPtr->reqHeight - (wmPtr->reqGridHeight * wmPtr->heightInc);
	if (base < 0) {
	    base = 0;
	}
	minHeight = ((minHeight - base) + wmPtr->heightInc-1)/wmPtr->heightInc;
    }
    if (minWidth < wmPtr->minWidth) {
	minWidth = wmPtr->minWidth;
    }
    if (minHeight < wmPtr->minHeight) {
	minHeight = wmPtr->minHeight;
    }
    *minWidthPtr = minWidth;
    *minHeightPtr = minHeight;
}

/*
 *----------------------------------------------------------------------
 *
 * GetMaxSize --
 *
 *	This function computes the current maxWidth and maxHeight values for a
 *	window, taking into account the possibility that they may be
 *	defaulted.
 *
 * Results:
 *	The values at *maxWidthPtr and *maxHeightPtr are filled in with the
 *	maximum allowable dimensions of wmPtr's window, in grid units. If no
 *	maximum has been specified for the window, then this function computes
 *	the largest sizes that will fit on the screen.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
GetMaxSize(
    TkWindow *winPtr,		/* Toplevel window to operate on. */
    int *maxWidthPtr,		/* Where to store the current maximum width of
				 * the window. */
    int *maxHeightPtr)		/* Where to store the current maximum height
				 * of the window. */
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    NSRect *maxBounds = (NSRect*)(winPtr->display->screens->ext_data);

    if (wmPtr->maxWidth > 0) {
	*maxWidthPtr = wmPtr->maxWidth;
    } else {
	int maxWidth = maxBounds->size.width - wmPtr->xInParent;

	if (wmPtr->gridWin != NULL) {
	    maxWidth = wmPtr->reqGridWidth
		    + (maxWidth - winPtr->reqWidth)/wmPtr->widthInc;
	}
	*maxWidthPtr = maxWidth;
    }
    if (wmPtr->maxHeight > 0) {
	*maxHeightPtr = wmPtr->maxHeight;
    } else {
	int maxHeight = maxBounds->size.height - wmPtr->yInParent;

	if (wmPtr->gridWin != NULL) {
	    maxHeight = wmPtr->reqGridHeight
		    + (maxHeight - winPtr->reqHeight)/wmPtr->heightInc;
	}
	*maxHeightPtr = maxHeight;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RemapWindows
 *
 *	Adjust parent/child relation ships of the given window hierarchy.
 *
 * Results:
 *	none
 *
 * Side effects:
 *	keeps windowing system (X11) happy
 *
 *----------------------------------------------------------------------
 */

static void
RemapWindows(
    TkWindow *winPtr,
    MacDrawable *parentWin)
{
    TkWindow *childPtr;
    /*
     * Remove the OS specific window. It will get rebuilt when the window gets
     * Mapped.
     */

    if (winPtr->window != None) {
	MacDrawable *macWin = (MacDrawable *) winPtr->window;

	macWin->toplevel->referenceCount--;
	macWin->toplevel = parentWin->toplevel;
	macWin->toplevel->referenceCount++;
	winPtr->flags &= ~TK_MAPPED;
#ifdef TK_REBUILD_TOPLEVEL
	winPtr->flags |= TK_REBUILD_TOPLEVEL;
#endif
    }

    /* Repeat for all the children */
    for (childPtr = winPtr->childList; childPtr != NULL;
	    childPtr = childPtr->nextPtr) {
	RemapWindows(childPtr, (MacDrawable *) winPtr->window);
    }
}



/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
