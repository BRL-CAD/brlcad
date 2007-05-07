/*
 * tkMacOSXInt.h --
 *
 *	Declarations of Macintosh specific shared variables and procedures.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2005-2007 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifndef _TKMACINT
#define _TKMACINT

#ifndef _TKINT
#include "tkInt.h"
#endif

#define TextStyle MacTextStyle
#include <Carbon/Carbon.h>
#undef TextStyle

/* Define constants only available on Mac OS X 10.3 or later */
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1030
    #define kEventAppAvailableWindowBoundsChanged 110
    #define kEventParamTransactionID 'trns'
    #define kEventParamWindowPartCode 'wpar'
    #define typeWindowPartCode 'wpar'
    #define kMenuAttrDoNotUseUserCommandKeys (1 << 7)
    #define kSimpleWindowClass 18
    #define kWindowDoesNotCycleAttribute (1L << 15)
    #define kWindowAsyncDragAttribute (1L << 23)
    #define kThemeBrushAlternatePrimaryHighlightColor -5
    #define kThemeResizeUpCursor 19
    #define kThemeResizeDownCursor 19
    #define kThemeResizeUpDownCursor 19
    #define kThemePoofCursor 19
    #define kThemeBackgroundMetal 6
    #define kThemeIncDecButtonSmall 21
    #define kThemeIncDecButtonMini 22
    #define kAppearancePartUpButton 20
    #define kAppearancePartDownButton 21
    #define kAppearancePartPageUpArea 22
    #define kAppearancePartPageDownArea 23
    #define kAppearancePartIndicator 129
    #define FixedToInt(a) ((short)(((Fixed)(a) + fixed1/2) >> 16))
    #define IntToFixed(a) ((Fixed)(a) << 16)
#endif
/* Define constants only available on Mac OS X 10.4 or later */
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1040
    #define kWindowNoTitleBarAttribute (1L << 9)
    #define kWindowMetalNoContentSeparatorAttribute (1L << 11)
    #define kThemeDisclosureTriangle 6
    #define kThemeBrushListViewOddRowBackground 56
    #define kThemeBrushListViewEvenRowBackground 57
    #define kThemeBrushListViewColumnDivider 58
    #define kThemeMetricScrollBarMinThumbHeight 132
    #define kThemeMetricSmallScrollBarMinThumbHeight 134
    #define kThemeScrollBarMedium kThemeMediumScrollBar
    #define kThemeScrollBarSmall kThemeSmallScrollBar
    #ifdef __BIG_ENDIAN__
    #define kCGBitmapByteOrder32Host (4 << 12)
    #else
    #define kCGBitmapByteOrder32Host (2 << 12)
    #endif
    #endif
/* Define constants only available on Mac OS X 10.5 or later */
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
    #define kWindowUnifiedTitleAndToolbarAttribute (1L << 7)
    #define kWindowTexturedSquareCornersAttribute (1L << 10)
#endif
/* Runtime HIToolbox version checking */
#ifndef kHIToolboxVersionNumber10_3
    #define kHIToolboxVersionNumber10_3 (145)
#endif
#ifndef kHIToolboxVersionNumber10_4
    #define kHIToolboxVersionNumber10_4 (219)
#endif
#ifndef kHIToolboxVersionNumber10_5
    #define kHIToolboxVersionNumber10_5 (291)
#endif

/*
 * Include platform specific public interfaces.
 */

#ifndef _TKMAC
#include "tkMacOSX.h"
#endif

struct TkWindowPrivate {
    TkWindow *winPtr;		/* Ptr to tk window or NULL if Pixmap */
    CGrafPtr grafPtr;
    CGContextRef context;
    ControlRef rootControl;
    int xOff;			/* X offset from toplevel window */
    int yOff;			/* Y offset from toplevel window */
    RgnHandle clipRgn;		/* Visible region of window */
    RgnHandle aboveClipRgn;	/* Visible region of window & it's children */
    RgnHandle drawRgn;		/* Clipped drawing region */
    int referenceCount;		/* Don't delete toplevel until children are
				 * gone. */
    struct TkWindowPrivate *toplevel;
				/* Pointer to the toplevel datastruct. */
    int flags;			/* Various state see defines below. */
};
typedef struct TkWindowPrivate MacDrawable;

/*
 * This list is used to keep track of toplevel windows that have a Mac
 * window attached. This is useful for several things, not the least
 * of which is maintaining floating windows.
 */

typedef struct TkMacOSXWindowList {
    struct TkMacOSXWindowList *nextPtr;
				/* The next window in the list. */
    TkWindow *winPtr;		/* This window */
} TkMacOSXWindowList;

/*
 * Defines use for the flags field of the MacDrawable data structure.
 */

#define TK_SCROLLBAR_GROW	0x01
#define TK_CLIP_INVALID		0x02
#define TK_HOST_EXISTS		0x04
#define TK_DRAWN_UNDER_MENU	0x08
#define TK_CLIPPED_DRAW		0x10

/*
 * I am reserving TK_EMBEDDED = 0x100 in the MacDrawable flags
 * This is defined in tk.h. We need to duplicate the TK_EMBEDDED flag in the
 * TkWindow structure for the window, but in the MacWin. This way we can
 * still tell what the correct port is after the TKWindow structure has been
 * freed. This actually happens when you bind destroy of a toplevel to
 * Destroy of a child.
 */

/*
 * This structure is for handling Netscape-type in process
 * embedding where Tk does not control the top-level. It contains
 * various functions that are needed by Mac specific routines, like
 * TkMacOSXGetDrawablePort. The definitions of the function types
 * are in tkMacOSX.h.
 */

typedef struct {
    Tk_MacOSXEmbedRegisterWinProc *registerWinProc;
    Tk_MacOSXEmbedGetGrafPortProc *getPortProc;
    Tk_MacOSXEmbedMakeContainerExistProc *containerExistProc;
    Tk_MacOSXEmbedGetClipProc *getClipProc;
    Tk_MacOSXEmbedGetOffsetInParentProc *getOffsetProc;
} TkMacOSXEmbedHandler;

MODULE_SCOPE TkMacOSXEmbedHandler *tkMacOSXEmbedHandler;

/*
 * Structure encapsulating current drawing environment.
 */

typedef struct TkMacOSXDrawingContext {
    CGContextRef context;
    CGrafPtr port, savePort;
    ThemeDrawingState saveState;
    PixPatHandle penPat;
    Rect portBounds;
    Boolean portChanged;
} TkMacOSXDrawingContext;

/*
 * Defines used for TkMacOSXInvalidateWindow
 */

#define TK_WINDOW_ONLY 0
#define TK_PARENT_WINDOW 1

/*
 * Accessor for the privatePtr flags field for the TK_HOST_EXISTS field
 */

#define TkMacOSXHostToplevelExists(tkwin) \
    (((TkWindow *) (tkwin))->privatePtr->toplevel->flags & TK_HOST_EXISTS)

/*
 * Defines use for the flags argument to TkGenWMConfigureEvent.
 */

#define TK_LOCATION_CHANGED	1
#define TK_SIZE_CHANGED		2
#define TK_BOTH_CHANGED		3

/*
 * Defines for tkTextDisp.c
 */

#define TK_LAYOUT_WITH_BASE_CHUNKS	1
#define TK_DRAW_IN_CONTEXT	1

#if !TK_DRAW_IN_CONTEXT
MODULE_SCOPE int TkMacOSXCompareColors(unsigned long c1, unsigned long c2);
#endif

/*
 * Macros abstracting checks only active in a debug build.
 */

#ifdef TK_MAC_DEBUG
/*
 * Macro to do debug message output.
 */
#define TkMacOSXDbgMsg(m, ...) do { \
	    fprintf(stderr, "%s:%d: %s(): " m "\n", strrchr(__FILE__, '/')+1, \
	    __LINE__, __func__, ##__VA_ARGS__); \
	} while (0)
/*
 * Macro to do very common check for noErr return from given API and output
 * debug message in case of failure.
 */
#define ChkErr(f, ...) ({ \
	OSStatus err = f(__VA_ARGS__); \
	if (err != noErr) { \
	    TkMacOSXDbgMsg("%s failed: %ld", #f, err); \
	} \
	err;})
/*
 * Macro to check emptyness of shared temp regions before use in debug builds.
 */
#define TkMacOSXCheckTmpRgnEmpty(r) do { \
	    if (!EmptyRgn(tkMacOSXtmpRgn##r)) { \
		Tcl_Panic("tkMacOSXtmpRgn%s nonempty", #r); \
	    } \
	} while(0)
#else /* TK_MAC_DEBUG */
#define TkMacOSXDbgMsg(m, ...)
#define ChkErr(f, ...) ({f(__VA_ARGS__);})
#define TkMacOSXCheckTmpRgnEmpty(r)
#endif /* TK_MAC_DEBUG */

/*
 * Variables shared among various Mac Tk modules but are not
 * exported to the outside world.
 */

MODULE_SCOPE RgnHandle tkMacOSXtmpRgn1;
MODULE_SCOPE RgnHandle tkMacOSXtmpRgn2;

/*
 * Globals shared among Macintosh Tk
 */

MODULE_SCOPE MenuHandle tkCurrentAppleMenu; /* Handle to current Apple Menu */
MODULE_SCOPE MenuHandle tkAppleMenu;	/* Handle to default Apple Menu */
MODULE_SCOPE MenuHandle tkFileMenu;	/* Handles to menus */
MODULE_SCOPE MenuHandle tkEditMenu;	/* Handles to menus */
MODULE_SCOPE int tkPictureIsOpen;	/* If this is 1, we are drawing to a
					 * picture The clipping should then be
					 * done relative to the bounds of the
					 * picture rather than the window. As
					 * of OS X.0.4, something is seriously
					 * wrong: The clipping bounds only
					 * seem to work if the top,left values
					 * are 0,0 The destination rectangle
					 * for CopyBits should also have
					 * top,left values of 0,0
					 */
MODULE_SCOPE TkMacOSXWindowList *tkMacOSXWindowListPtr; /* List of toplevels */
MODULE_SCOPE Tcl_Encoding TkMacOSXCarbonEncoding;

/*
 * Prototypes of internal procs not in the stubs table.
 */

#if 0
MODULE_SCOPE int XSetClipRectangles(Display *d, GC gc, int clip_x_origin,
	int clip_y_origin, XRectangle* rectangles, int n, int ordering);
#endif
MODULE_SCOPE void TkpClipDrawableToRect(Display *display, Drawable d, int x,
	int y, int width, int height);
MODULE_SCOPE void TkMacOSXDisplayChanged(Display *display);
MODULE_SCOPE void TkMacOSXInitScrollbarMetrics(void);
MODULE_SCOPE int TkMacOSXUseAntialiasedText(Tcl_Interp *interp, int enable);
MODULE_SCOPE void TkMacOSXInitCarbonEvents(Tcl_Interp *interp);
MODULE_SCOPE int TkMacOSXInitCGDrawing(Tcl_Interp *interp, int enable,
	int antiAlias);
MODULE_SCOPE void TkMacOSXInitKeyboard(Tcl_Interp *interp);
MODULE_SCOPE void TkMacOSXDefaultStartupScript(void);
MODULE_SCOPE int TkMacOSXGenerateFocusEvent(Window window, int activeFlag);
MODULE_SCOPE int TkMacOSXGenerateParentMenuSelectEvent(MenuRef menu);
MODULE_SCOPE int TkMacOSXGenerateMenuSelectEvent(MenuRef menu,
	MenuItemIndex index);
MODULE_SCOPE void TkMacOSXClearActiveMenu(MenuRef menu);
MODULE_SCOPE WindowClass TkMacOSXWindowClass(TkWindow *winPtr);
MODULE_SCOPE int TkMacOSXIsWindowZoomed(TkWindow *winPtr);
MODULE_SCOPE int TkGenerateButtonEventForXPointer(Window window);
MODULE_SCOPE EventModifiers TkMacOSXModifierState(void);
MODULE_SCOPE int TkMacOSXSetupDrawingContext(Drawable d, GC gc, int useCG,
    TkMacOSXDrawingContext *dc);
MODULE_SCOPE void TkMacOSXRestoreDrawingContext(TkMacOSXDrawingContext *dc);
MODULE_SCOPE void TkMacOSXSetColorInPort(unsigned long pixel, int fg,
	PixPatHandle penPat);
MODULE_SCOPE void TkMacOSXSetColorInContext(unsigned long pixel,
	CGContextRef context);
MODULE_SCOPE int TkMacOSXRunTclEventLoop(void);
MODULE_SCOPE OSStatus TkMacOSXStartTclEventLoopCarbonTimer(void);
MODULE_SCOPE OSStatus TkMacOSXStopTclEventLoopCarbonTimer(void);
MODULE_SCOPE void TkMacOSXTrackingLoop(int tracking);
MODULE_SCOPE OSStatus TkMacOSXReceiveAndDispatchEvent(void);
MODULE_SCOPE void TkMacOSXInstallWindowCarbonEventHandler(Tcl_Interp *interp,
	WindowRef window);
MODULE_SCOPE int TkMacOSXMakeFullscreen(TkWindow *winPtr, WindowRef window,
	int fullscreen, Tcl_Interp *interp);
MODULE_SCOPE void TkMacOSXEnterExitFullscreen(TkWindow *winPtr, int active);

MODULE_SCOPE void* TkMacOSXGetNamedSymbol(const char* module,
	const char* symbol);

/*
 * Macro abstracting use of TkMacOSXGetNamedSymbol to init named symbols.
 */

#define TkMacOSXInitNamedSymbol(module, ret, symbol, ...) \
    static ret (* symbol)(__VA_ARGS__) = (void*)(-1L); \
    if (symbol == (void*)(-1L)) { \
	symbol = TkMacOSXGetNamedSymbol(STRINGIFY(module), \
		STRINGIFY(_##symbol)); \
    }

/*
 * Include the stubbed internal platform-specific API.
 */

#include "tkIntPlatDecls.h"

#endif /* _TKMACINT */
