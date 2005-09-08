/*
 *  bltDnd.c --
 *
 *	This module implements a drag-and-drop mechanism for the Tk
 *	Toolkit.  Allows widgets to be registered as drag&drop sources
 *	and targets for handling "drag-and-drop" operations between
 *	Tcl/Tk applications.
 *
 * Copyright 1993-1998 Lucent Technologies, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 *
 *	The "drag&drop" command was created by Michael J. McLennan.
 */
#include "bltInt.h"

#ifndef NO_DRAGDROP
#include "bltHash.h"
#include "bltChain.h"

#include <X11/Xatom.h>

#ifdef WIN32
#define MAX_PROP_SIZE 255	/* Maximum size of property. */
typedef HWND WINDOW;
#else
#define MAX_PROP_SIZE 1000	/* Maximum size of property. */
typedef Window WINDOW;
static Atom dndAtom;
#endif

/*
 *	Each "drag&drop" target widget is tagged with a "BltDrag&DropTarget" 
 *	property in XA_STRING format.  This property identifies the window 
 *	as a "drag&drop" target.  It's formated as a Tcl list and contains
 *	the following information:
 *
 *	    "INTERP_NAME TARGET_NAME DATA_TYPE DATA_TYPE ..."
 *
 *	  INTERP_NAME	Name of the target application's interpreter.
 *	  TARGET_NAME	Path name of widget registered as the drop target.  
 *	  DATA_TYPE	One or more "types" handled by the target.
 *
 *	When the user invokes the "drag" operation, the window hierarchy
 *	is progressively examined.  Window information is cached during
 *	the operation, to minimize X server traffic. Windows carrying a
 *	"BltDrag&DropTarget" property are identified.  When the token is 
 *	dropped over a valid site, the drop information is sent to the 
 *	application 
 *	via the usual "send" command.  If communication fails, the drag&drop 
 *	facility automatically posts a rejection symbol on the token window.  
 */

#define INTERP_NAME	0
#define TARGET_NAME	1
#define DATA_TYPE	2

/* Error Proc used to report drag&drop background errors */
#define DEF_ERROR_PROC              "bgerror"
/*
 *  CONFIG PARAMETERS
 */
#define DEF_DND_BUTTON_BACKGROUND		RGB_YELLOW
#define DEF_DND_BUTTON_BG_MONO		STD_NORMAL_BG_MONO
#define DEF_DND_BUTTON_NUMBER		"3"
#define DEF_DND_PACKAGE_COMMAND		(char *)NULL
#define DEF_DND_SELF_TARGET		"no"
#define DEF_DND_SEND			"all"
#define DEF_DND_SITE_COMMAND		(char *)NULL
#define DEF_TOKEN_ACTIVE_BACKGROUND	STD_ACTIVE_BACKGROUND
#define DEF_TOKEN_ACTIVE_BG_MONO	STD_ACTIVE_BG_MONO
#define DEF_TOKEN_ACTIVE_BORDERWIDTH	"3"
#define DEF_TOKEN_ACTIVE_RELIEF		"sunken"
#define DEF_TOKEN_ANCHOR		"se"
#define DEF_TOKEN_BACKGROUND		STD_NORMAL_BACKGROUND
#define DEF_TOKEN_BG_MONO		STD_NORMAL_BG_MONO
#define DEF_TOKEN_BORDERWIDTH		"3"
#define DEF_TOKEN_CURSOR		"arrow"
#define DEF_TOKEN_OUTLINE_COLOR		RGB_BLACK
#define DEF_TOKEN_OUTLINE_MONO		RGB_BLACK
#define DEF_TOKEN_REJECT_BACKGROUND	STD_NORMAL_BACKGROUND
#define DEF_TOKEN_REJECT_BG_MONO	RGB_WHITE
#define DEF_TOKEN_REJECT_FOREGROUND	RGB_RED
#define DEF_TOKEN_REJECT_FG_MONO	RGB_BLACK
#define DEF_TOKEN_REJECT_STIPPLE_COLOR	(char *)NULL
#define DEF_TOKEN_REJECT_STIPPLE_MONO	RGB_GREY50
#define DEF_TOKEN_RELIEF		"raised"

#if HAVE_NAMESPACES
static char dragDropCmd[] = "blt::drag&drop";
#else
static char dragDropCmd[] = "drag&drop";
#endif

static char className[] = "DragDropToken";	/* CLASS NAME of token window */
static char propName[] = "BltDrag&DropTarget";	/* Property name */

static Blt_HashTable sourceTable;
static Blt_HashTable targetTable;
static char *errorCmd;
static int nActive;
static int locX, locY;
static int initialized = FALSE;

/*
 *  Percent substitutions
 */
typedef struct {
    char letter;		/* character like 'x' in "%x" */
    char *value;		/* value to be substituted in place of "%x" */
} SubstDescriptors;


/*
 *  AnyWindow --
 *
 *	This structure represents a window hierarchy examined during
 *	a single "drag" operation.  It's used to cache information
 *	to reduce the round-trip calls to the server needed to query
 *	window geometry information and grab the target property.
 */
typedef struct AnyWindowStruct AnyWindow;

struct AnyWindowStruct {
    WINDOW nativeWindow;	/* Native window: HWINDOW (Win32) or 
				 * Window (X11). */

    int initialized;		/* If non-zero, the rest of this structure's
				 * information had been previously built. */

    int x1, y1, x2, y2;		/* Extents of the window (upper-left and
				 * lower-right corners). */

    AnyWindow *parentPtr;	/* Parent node. NULL if root. Used to
				  * compute offset for X11 windows. */

    Blt_Chain *chainPtr;	/* List of this window's children. If NULL,
				 * there are no children. */

    char **targetInfo;		/* An array of target window drag&drop
				 * information: target interpreter,
				 * pathname, and optionally possible
				 * type matches. NULL if the window is
				 * not a drag&drop target or is not a
				 * valid match for the drop source. */

};

/*
 *  Drag&Drop Registration Data
 */
typedef struct {

    /*
     * This is a goof in the Tk API.  It assumes that only an official
     * Tk "toplevel" widget will ever become a toplevel window (i.e. a
     * window whose parent is the root window).  Because under Win32,
     * Tk tries to use the widget record associated with the TopLevel
     * as a Tk frame widget, to read its menu name.  What this means
     * is that any widget that's going to be a toplevel, must also look
     * like a frame. Therefore we've copied the frame widget structure
     * fields into the token.
     */

    Tk_Window tkwin;		/* Window that embodies the frame.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up. */
    Display *display;		/* Display containing widget.  Used, among
				 * other things, so that resources can be
				 * freed even after tkwin has gone away. */
    Tcl_Interp *interp;		/* Interpreter associated with widget.  Used
				 * to delete widget command. */
    Tcl_Command widgetCmd;	/* Token for frame's widget command. */
    char *className;		/* Class name for widget (from configuration
				 * option).  Malloc-ed. */
    int mask;			/* Either FRAME or TOPLEVEL;  used to select
				 * which configuration options are valid for
				 * widget. */
    char *screenName;		/* Screen on which widget is created.  Non-null
				 * only for top-levels.  Malloc-ed, may be
				 * NULL. */
    char *visualName;		/* Textual description of visual for window,
				 * from -visual option.  Malloc-ed, may be
				 * NULL. */
    char *colormapName;		/* Textual description of colormap for window,
				 * from -colormap option.  Malloc-ed, may be
				 * NULL. */
    char *menuName;		/* Textual description of menu to use for
				 * menubar. Malloc-ed, may be NULL. */
    Colormap colormap;		/* If not None, identifies a colormap
				 * allocated for this window, which must be
				 * freed when the window is deleted. */
    Tk_3DBorder border;		/* Structure used to draw 3-D border and
				 * background.  NULL means no background
				 * or border. */
    int borderWidth;		/* Width of 3-D border (if any). */
    int relief;			/* 3-d effect: TK_RELIEF_RAISED etc. */
    int highlightWidth;		/* Width in pixels of highlight to draw
				 * around widget when it has the focus.
				 * 0 means don't draw a highlight. */
    XColor *highlightBgColorPtr;
				/* Color for drawing traversal highlight
				 * area when highlight is off. */
    XColor *highlightColorPtr;	/* Color for drawing traversal highlight. */
    int width;			/* Width to request for window.  <= 0 means
				 * don't request any size. */
    int height;			/* Height to request for window.  <= 0 means
				 * don't request any size. */
    Tk_Cursor cursor;		/* Current cursor for window, or None. */
    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    int isContainer;		/* 1 means this window is a container, 0 means
				 * that it isn't. */
    char *useThis;		/* If the window is embedded, this points to
				 * the name of the window in which it is
				 * embedded (malloc'ed).  For non-embedded
				 * windows this is NULL. */
    int flags;			/* Various flags;  see below for
				 * definitions. */

    /* Token specific fields */

    int lastX, lastY;		/* last position of token window */
    int active;			/* non-zero => over target window */
    Tcl_TimerToken timer;	/* token for routine to hide tokenwin */
    GC rejectFgGC;		/* GC used to draw rejection fg: (\) */
    GC rejectBgGC;		/* GC used to draw rejection bg: (\) */

    /* User-configurable fields */

    Tk_Anchor anchor;		/* Position of token win relative to mouse */
    Tk_3DBorder outline;	/* Outline border around token window */
    Tk_3DBorder normalBorder;	/* Border/background for token window */
    Tk_3DBorder activeBorder;	/* Border/background for token window */
    int activeRelief;
    int activeBorderWidth;	/* Border width in pixels */
    XColor *rejectFg;		/* Color used to draw rejection fg: (\) */
    XColor *rejectBg;		/* Color used to draw rejection bg: (\) */
    Pixmap rejectStipple;	/* Stipple used to draw rejection: (\) */
} Token;

typedef struct {
    Tcl_Interp *interp;		/* Interpreter associated with the Tk source 
				 * widget. */

    Tk_Window tkwin;		/* Tk window registered as the drag&drop 
				 * source. */

    Display *display;		/* Drag&drop source window display */

    Blt_HashTable handlerTable;	/* Table of data handlers (converters)
				 * registered for this source. */

    int button;			/* Button used to invoke drag operation. */

    Token token;		/* Token used to provide special cursor. */
    
    int pkgCmdInProgress;	/* Indicates if a pkgCmd is currently active. */
    char *pkgCmd;		/* Tcl command executed at start of "drag"
				 * operation to gather information about 
				 * the source data. */

    char *pkgCmdResult;		/* Result returned by the most recent 
				 * pkgCmd. */

    char *siteCmd;		/* Tcl command executed to update token 
				 * window. */

    AnyWindow *rootPtr;		/* Cached window information: Gathered
				 * and used during the "drag" operation 
				 * to see if the mouse pointer is over a 
				 * valid target. */

    int selfTarget;		/* Indicated if the source should drop onto 
				 * itself. */

    Tk_Cursor cursor;		/* cursor restored after dragging */

    char **sendTypes;		/* list of data handler names or "all" */

    Blt_HashEntry *hashPtr;

    AnyWindow *windowPtr;	/* Last target examined. If NULL, mouse 
				 * pointer is not currently over a valid 
				 * target. */
} Source;

typedef struct {
    Tcl_Interp *interp;
    Tk_Window tkwin;		/* drag&drop target window */
    Display *display;		/* drag&drop target window display */
    Blt_HashTable handlerTable;	/* Table of data handlers (converters)
				 * registered for this target. */
    Blt_HashEntry *hashPtr;
} Target;


extern Tk_CustomOption bltListOption;

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_INT, "-button", "buttonBinding", "ButtonBinding",
	DEF_DND_BUTTON_NUMBER, Tk_Offset(Source, button), 0},
    {TK_CONFIG_STRING, "-packagecmd", "packageCommand", "Command",
	DEF_DND_PACKAGE_COMMAND, Tk_Offset(Source, pkgCmd), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-rejectbg", "rejectBackground", "Background",
	DEF_TOKEN_REJECT_BACKGROUND, Tk_Offset(Source, token.rejectBg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-rejectbg", "rejectBackground", "Background",
	DEF_TOKEN_REJECT_BG_MONO, Tk_Offset(Source, token.rejectBg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-rejectfg", "rejectForeground", "Foreground",
	DEF_TOKEN_REJECT_FOREGROUND, Tk_Offset(Source, token.rejectFg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-rejectfg", "rejectForeground", "Foreground",
	DEF_TOKEN_REJECT_BACKGROUND, Tk_Offset(Source, token.rejectFg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BITMAP, "-rejectstipple", "rejectStipple", "Stipple",
	DEF_TOKEN_REJECT_STIPPLE_COLOR, Tk_Offset(Source, token.rejectStipple),
	TK_CONFIG_COLOR_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_BITMAP, "-rejectstipple", "rejectStipple", "Stipple",
	DEF_TOKEN_REJECT_STIPPLE_MONO, Tk_Offset(Source, token.rejectStipple),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BOOLEAN, "-selftarget", "selfTarget", "SelfTarget",
	DEF_DND_SELF_TARGET, Tk_Offset(Source, selfTarget), 0},
    {TK_CONFIG_CUSTOM, "-send", "send", "Send", DEF_DND_SEND, 
	Tk_Offset(Source, sendTypes), TK_CONFIG_NULL_OK, &bltListOption},
    {TK_CONFIG_STRING, "-sitecmd", "siteCommand", "Command",
	DEF_DND_SITE_COMMAND, Tk_Offset(Source, siteCmd), TK_CONFIG_NULL_OK},
    {TK_CONFIG_ANCHOR, "-tokenanchor", "tokenAnchor", "Anchor",
	DEF_TOKEN_ANCHOR, Tk_Offset(Source, token.anchor), 0},
    {TK_CONFIG_BORDER, "-tokenactivebackground", "tokenActiveBackground", 
	"ActiveBackground", DEF_TOKEN_ACTIVE_BACKGROUND, 
	Tk_Offset(Source, token.activeBorder), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-tokenactivebackground", "tokenActiveBackground", 
	"ActiveBackground", DEF_TOKEN_ACTIVE_BG_MONO, 
	Tk_Offset(Source, token.activeBorder), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-tokenbg", "tokenBackground", "Background",
	DEF_TOKEN_BACKGROUND, Tk_Offset(Source, token.normalBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-tokenbg", "tokenBackground", "Background",
	DEF_TOKEN_BG_MONO, Tk_Offset(Source, token.normalBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-tokenoutline", "tokenOutline", "Outline",
	DEF_TOKEN_OUTLINE_COLOR, Tk_Offset(Source, token.outline),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-tokenoutline", "tokenOutline", "Outline",
	DEF_TOKEN_OUTLINE_MONO, Tk_Offset(Source, token.outline),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_PIXELS, "-tokenborderwidth", "tokenBorderWidth", "BorderWidth",
	DEF_TOKEN_BORDERWIDTH, Tk_Offset(Source, token.borderWidth), 0},
    {TK_CONFIG_CURSOR, "-tokencursor", "tokenCursor", "Cursor",
	DEF_TOKEN_CURSOR, Tk_Offset(Source, token.cursor), 
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 
	0, 0},
};

static Tk_ConfigSpec tokenConfigSpecs[] =
{
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground",
	"ActiveBackground", DEF_TOKEN_ACTIVE_BACKGROUND,
	Tk_Offset(Token, activeBorder), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground",
	"ActiveBackground", DEF_TOKEN_ACTIVE_BG_MONO, 
	Tk_Offset(Token, activeBorder), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_RELIEF, "-activerelief", "activeRelief", "activeRelief",
	DEF_TOKEN_ACTIVE_RELIEF, Tk_Offset(Token, activeRelief), 0},
    {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_TOKEN_ANCHOR, Tk_Offset(Token, anchor), 0},
    {TK_CONFIG_PIXELS, "-activeborderwidth", "activeBorderWidth",
	"ActiveBorderWidth", DEF_TOKEN_ACTIVE_BORDERWIDTH, 
	Tk_Offset(Token, activeBorderWidth), 0},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_TOKEN_BACKGROUND, Tk_Offset(Token, normalBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_TOKEN_BG_MONO, Tk_Offset(Token, normalBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_TOKEN_BORDERWIDTH, Tk_Offset(Token, borderWidth), 0},
    {TK_CONFIG_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_TOKEN_CURSOR, Tk_Offset(Token, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BORDER, "-outline", "outline", "Outline",
	DEF_TOKEN_OUTLINE_COLOR, Tk_Offset(Token, outline),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-outline", "outline", "Outline",
	DEF_TOKEN_OUTLINE_MONO, Tk_Offset(Token, outline), 
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-rejectbg", "rejectBackground", "Background",
	DEF_TOKEN_REJECT_BACKGROUND, Tk_Offset(Token, rejectBg), 
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-rejectbg", "rejectBackground", "Background",
	DEF_TOKEN_REJECT_BG_MONO, Tk_Offset(Token, rejectBg), 
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-rejectfg", "rejectForeground", "Foreground",
	DEF_TOKEN_REJECT_FOREGROUND, Tk_Offset(Token, rejectFg), 
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-rejectfg", "rejectForeground", "Foreground",
	DEF_TOKEN_REJECT_BACKGROUND, Tk_Offset(Token, rejectFg), 
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BITMAP, "-rejectstipple", "rejectStipple", "Stipple",
	DEF_TOKEN_REJECT_STIPPLE_COLOR, Tk_Offset(Token, rejectStipple),
	TK_CONFIG_COLOR_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_BITMAP, "-rejectstipple", "rejectStipple", "Stipple",
	DEF_TOKEN_REJECT_STIPPLE_MONO, Tk_Offset(Token, rejectStipple),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_TOKEN_RELIEF, Tk_Offset(Token, relief), 0},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, 
	0, 0},
};


/*
 *  Forward Declarations
 */
static int DragDropCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	int argc, char **argv));
static void TokenEventProc _ANSI_ARGS_((ClientData clientData, 
	XEvent *eventPtr));
static void TargetEventProc _ANSI_ARGS_((ClientData clientData, 
	XEvent *eventPtr));
static void MoveToken _ANSI_ARGS_((Source * srcPtr, Token *tokenPtr));
static void UpdateToken _ANSI_ARGS_((ClientData clientData));
static void HideToken _ANSI_ARGS_((Token *tokenPtr));
static void RejectToken _ANSI_ARGS_((Token *tokenPtr));

static int GetSource _ANSI_ARGS_((Tcl_Interp *interp, char *name, 
	Source **srcPtrPtr));
static Source *CreateSource _ANSI_ARGS_((Tcl_Interp *interp, char *pathname,
	int *newEntry));
static void DestroySource _ANSI_ARGS_((Source * srcPtr));
static void SourceEventProc _ANSI_ARGS_((ClientData clientData,
	XEvent *eventPtr));
static int ConfigureSource _ANSI_ARGS_((Tcl_Interp *interp, Source * srcPtr,
	int argc, char **argv, int flags));
static int ConfigureToken _ANSI_ARGS_((Tcl_Interp *interp, Source * srcPtr,
	int argc, char **argv));

static Target *CreateTarget _ANSI_ARGS_((Tcl_Interp *interp, Tk_Window tkwin));
static Target *FindTarget _ANSI_ARGS_((Tk_Window tkwin));
static void DestroyTarget _ANSI_ARGS_((DestroyData dataPtr));
static int OverTarget _ANSI_ARGS_((Source * srcPtr, int x, int y));
static void AddTargetProperty _ANSI_ARGS_((Tcl_Interp *interp,
	Target * targetPtr));

static void DndSend _ANSI_ARGS_((Source * srcPtr));

static void InitRoot _ANSI_ARGS_((Source * srcPtr));
static void RemoveWindow _ANSI_ARGS_((AnyWindow *wr));
static void QueryWindow _ANSI_ARGS_((Display *display, AnyWindow * windowPtr));

static char *ExpandPercents _ANSI_ARGS_((char *str, SubstDescriptors * subs,
	int nsubs, Tcl_DString *resultPtr));



#ifdef	WIN32

#if defined( _MSC_VER) || defined(__BORLANDC__)
#include <tchar.h>
#endif /* _MSC_VER || __BORLANDC__ */

typedef struct {
    char *prefix;
    int prefixSize;
    char *propReturn;
} PropertyInfo;


static BOOL CALLBACK
GetEnumWindowsProc(HWND hWnd, LPARAM clientData)
{
    Blt_Chain *chainPtr = (Blt_Chain *) clientData;

    Blt_ChainAppend(chainPtr, (ClientData)hWnd);
    return TRUE;
}

static WINDOW
GetNativeWindow(Tk_Window tkwin)
{
    return (WINDOW) Tk_GetHWND(Tk_WindowId(tkwin));
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetWindowZOrder --
 *
 *	Returns a list of the child windows according to their stacking
 *	order.  The window handles are ordered from top to bottom.
 *
 * ------------------------------------------------------------------------
 */
static Blt_Chain *
GetWindowZOrder(
    Display *display,
    HWND parent)
{
    Blt_Chain *chainPtr;
    HWND hWnd;

    chainPtr = Blt_ChainCreate();
    for (hWnd = GetTopWindow(parent); hWnd != NULL;
	hWnd = GetNextWindow(hWnd, GW_HWNDNEXT)) {
	Blt_ChainAppend(chainPtr, (ClientData)hWnd);
    }
    return chainPtr;
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetEnumPropsExProc --
 *
 * ------------------------------------------------------------------------
 */
static BOOL CALLBACK
GetEnumPropsExProc(
    HWND hwnd, 
    LPCTSTR atom, 
    HANDLE hData, 
    DWORD clientData)
{
    PropertyInfo *infoPtr = (PropertyInfo *) clientData;

    if (strncmp(infoPtr->prefix, atom, infoPtr->prefixSize) == 0) {
	assert(infoPtr->propReturn == NULL);
	infoPtr->propReturn = (char *)atom;
	return FALSE;
    }
    return TRUE;
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetPropData --
 *
 *	This is a bad Windows hack to pass property information between
 *	applications.  (Ab)Normally the property data (one-word value) is
 *	stored in the data handle.  But the data content is available only
 *	within the application.  The pointer value is meaningless outside
 *	of the current application address space.  Not really useful at all.
 *
 *	So the trick here is to encode the property name with all the
 *	necessary information and to loop through all the properties
 *	of a window, looking for one that starts with our property name
 *	prefix.  The downside is that the property name is limited to
 *	255 bytes.  But that should be enough.  It's also slower since
 *	we examine each property until we find ours.
 *
 *	We'll plug in the OLE stuff later.
 *
 * ------------------------------------------------------------------------
 */

static char *
GetPropData(HWND hWnd, char *atom)
{
    PropertyInfo propInfo;
    if (hWnd == NULL) {
	return NULL;
    }
    propInfo.prefix = atom;
    propInfo.prefixSize = strlen(atom);
    propInfo.propReturn = NULL;
    EnumPropsEx(hWnd, (PROPENUMPROCEX)GetEnumPropsExProc, (DWORD)&propInfo);
    return propInfo.propReturn;
}


static char *
GetProperty(Display *display, HWND hWnd)
{
    ATOM atom;

    atom = (ATOM)GetProp(hWnd, propName);
    if (atom != (ATOM)0) {
	char buffer[MAX_PROP_SIZE + 1];
	UINT nBytes;

	nBytes = GlobalGetAtomName(atom, buffer, MAX_PROP_SIZE);
	if (nBytes > 0) {
	    buffer[nBytes] = '\0';
	    return Blt_Strdup(buffer);
	}
    }
    return NULL;
}

static void
SetProperty(Tk_Window tkwin, char *data)
{
    HWND hWnd;
    ATOM atom;

    hWnd = Tk_GetHWND(Tk_WindowId(tkwin));
    if (hWnd == NULL) {
	return;
    }
    atom = (ATOM)GetProp(hWnd, propName);
    if (atom != 0) {
	GlobalDeleteAtom(atom);
    }
    atom = GlobalAddAtom(data);
    if (atom != (ATOM)0) {
	SetProp(hWnd, propName, (HANDLE)atom);
    }
}

static void
RemoveProperty(Tk_Window tkwin)
{
    HWND hWnd;
    ATOM atom;

    hWnd = Tk_GetHWND(Tk_WindowId(tkwin));
    if (hWnd == NULL) {
	return;
    }
    atom = (ATOM)GetProp(hWnd, propName);
    if (atom != 0) {
	GlobalDeleteAtom(atom);
    }
    RemoveProp(hWnd, propName);
}

/*
 *----------------------------------------------------------------------
 *
 * GetWindowRegion --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
GetWindowRegion(
    Display *display,		/* Not used. */
    HWND hWnd,
    int *x1Ptr,
    int *y1Ptr,
    int *x2Ptr,
    int *y2Ptr)
{
    RECT rect;

    if (GetWindowRect(hWnd, &rect)) {
	*x1Ptr = rect.left;
	*y1Ptr = rect.top;
	*x2Ptr = rect.right;
	*y2Ptr = rect.bottom;
	return IsWindowVisible(hWnd);
    }
    return FALSE;
}

#else

static WINDOW
GetNativeWindow(tkwin)
    Tk_Window tkwin;
{
    return Tk_WindowId(tkwin);
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetWindowZOrder --
 *
 *	Returns a chain of the child windows according to their stacking
 *	order.  The window ids are ordered from top to bottom.
 *
 * ------------------------------------------------------------------------
 */
static Blt_Chain *
GetWindowZOrder(display, window)
    Display *display;
    Window window;
{
    Blt_Chain *chainPtr;
    Window *childArr;
    unsigned int nChildren;
    Window dummy;

    chainPtr = NULL;
    if ((XQueryTree(display, window, &dummy, &dummy, &childArr, &nChildren)) &&
	(nChildren > 0)) {
	register int i;

	chainPtr = Blt_ChainCreate();
	for (i = 0; i < nChildren; i++) {
	    /*
	     *  XQuery returns windows in bottom to top order.
	     *  We only care about the top window.
	     */
	    Blt_ChainPrepend(chainPtr, (ClientData)childArr[i]);
	}
	if (childArr != NULL) {
	    XFree((char *)childArr);	/* done with list of kids */
	}
    }
    return chainPtr;
}

static char *
GetProperty(display, window)
    Display *display;
    Window window;
{
    char *data;
    int result, actualFormat;
    Atom actualType;
    unsigned long nItems, bytesAfter;

    if (window == None) {
	return NULL;
    }
    data = NULL;
    result = XGetWindowProperty(display, window, dndAtom, 0, MAX_PROP_SIZE,
	False, XA_STRING, &actualType, &actualFormat, &nItems, &bytesAfter,
	(unsigned char **)&data);
    if ((result != Success) || (actualFormat != 8) ||
	(actualType != XA_STRING)) {
	if (data != NULL) {
	    XFree((char *)data);
	    data = NULL;
	}
    }
    return data;
}

static void
SetProperty(tkwin, data)
    Tk_Window tkwin;
    char *data;
{
    XChangeProperty(Tk_Display(tkwin), Tk_WindowId(tkwin), dndAtom, XA_STRING,
	8, PropModeReplace, (unsigned char *)data, strlen(data) + 1);
}

static int
GetWindowRegion(display, window, x1Ptr, y1Ptr, x2Ptr, y2Ptr)
    Display *display;
    Window window;
    int *x1Ptr, *y1Ptr, *x2Ptr, *y2Ptr;
{
    XWindowAttributes winAttrs;

    if (XGetWindowAttributes(display, window, &winAttrs)) {
	*x1Ptr = winAttrs.x;
	*y1Ptr = winAttrs.y;
	*x2Ptr = winAttrs.x + winAttrs.width - 1;
	*y2Ptr = winAttrs.y + winAttrs.height - 1;
    }
    return (winAttrs.map_state == IsViewable);
}

#endif /* WIN32 */

/*
 * ------------------------------------------------------------------------
 *
 *  ChangeToken --
 *
 * ------------------------------------------------------------------------
 */
static void
ChangeToken(tokenPtr, active)
    Token *tokenPtr;
    int active;
{
    int relief;
    Tk_3DBorder border;
    int borderWidth;

    Blt_Fill3DRectangle(tokenPtr->tkwin, Tk_WindowId(tokenPtr->tkwin),
	tokenPtr->outline, 0, 0, Tk_Width(tokenPtr->tkwin),
	Tk_Height(tokenPtr->tkwin), 0, TK_RELIEF_FLAT);
    if (active) {
	relief = tokenPtr->activeRelief;
	border = tokenPtr->activeBorder;
	borderWidth = tokenPtr->activeBorderWidth;
    } else {
	relief = tokenPtr->relief;
	border = tokenPtr->normalBorder;
	borderWidth = tokenPtr->borderWidth;
    }
    Blt_Fill3DRectangle(tokenPtr->tkwin, Tk_WindowId(tokenPtr->tkwin), border, 
	2, 2, Tk_Width(tokenPtr->tkwin) - 4, Tk_Height(tokenPtr->tkwin) - 4, 
	borderWidth, relief);
}

/*
 * ------------------------------------------------------------------------
 *
 *  TokenEventProc --
 *
 *	Invoked by the Tk dispatcher to handle widget events.
 *	Manages redraws for the drag&drop token window.
 *
 * ------------------------------------------------------------------------
 */
static void
TokenEventProc(clientData, eventPtr)
    ClientData clientData;	/* data associated with widget */
    XEvent *eventPtr;		/* information about event */
{
    Token *tokenPtr = clientData;

    if ((eventPtr->type == Expose) && (eventPtr->xexpose.count == 0)) {
	if (tokenPtr->tkwin != NULL) {
	    ChangeToken(tokenPtr, tokenPtr->active);
	}
    } else if (eventPtr->type == DestroyNotify) {
	tokenPtr->tkwin = NULL;
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  HideToken --
 *
 *	Unmaps the drag&drop token.  Invoked directly at the end of a
 *	successful communication, or after a delay if the communication
 *	fails (allowing the user to see a graphical picture of failure).
 *
 * ------------------------------------------------------------------------
 */
static void
HideToken(tokenPtr)
    Token *tokenPtr;
{
    if (tokenPtr->tkwin != NULL) {
	Tk_UnmapWindow(tokenPtr->tkwin);
    }
    tokenPtr->timer = NULL;
}

/*
 * ------------------------------------------------------------------------
 *
 *  RaiseToken --
 *
 * ------------------------------------------------------------------------
 */
static void
RaiseToken(tokenPtr)
    Token *tokenPtr;
{
    Blt_MapToplevel(tokenPtr->tkwin);
    Blt_RaiseToplevel(tokenPtr->tkwin);
}

/*
 * ------------------------------------------------------------------------
 *
 *  MoveToken --
 *
 *	Invoked during "drag" operations to move a token window to its
 *	current "drag" coordinate.
 *
 * ------------------------------------------------------------------------
 */
static void
MoveToken(srcPtr, tokenPtr)
    Source *srcPtr;		/* drag&drop source window data */
    Token *tokenPtr;
{
    int x, y;
    int maxX, maxY;
    int vx, vy, vw, vh;
    Screen *screenPtr;

    /* Adjust current location for virtual root windows.  */
    Tk_GetVRootGeometry(srcPtr->tkwin, &vx, &vy, &vw, &vh);
    x = tokenPtr->lastX + vx - 3;
    y = tokenPtr->lastY + vy - 3;

    screenPtr = Tk_Screen(srcPtr->tkwin);
    maxX = WidthOfScreen(screenPtr) - Tk_Width(tokenPtr->tkwin);
    maxY = HeightOfScreen(screenPtr) - Tk_Height(tokenPtr->tkwin);
    Blt_TranslateAnchor(x, y, Tk_Width(tokenPtr->tkwin),
	Tk_Height(tokenPtr->tkwin), tokenPtr->anchor, &x, &y);
    if (x > maxX) {
	x = maxX;
    } else if (x < 0) {
	x = 0;
    }
    if (y > maxY) {
	y = maxY;
    } else if (y < 0) {
	y = 0;
    }
    if ((x != Tk_X(tokenPtr->tkwin)) || (y != Tk_Y(tokenPtr->tkwin))) {
	Tk_MoveToplevelWindow(tokenPtr->tkwin, x, y);
    }
    RaiseToken(tokenPtr);
}

static Tk_Cursor
GetWidgetCursor(interp, tkwin)
    Tk_Window tkwin;
    Tcl_Interp *interp;
{
    CONST char *cursorName;
    Tk_Cursor cursor;

    cursor = None;
    if (Tcl_VarEval(interp, Tk_PathName(tkwin), " cget -cursor",
	    (char *)NULL) != TCL_OK) {
	return None;
    }
    cursorName = Tcl_GetStringResult(interp);
    if ((cursorName != NULL) && (cursorName[0] != '\0')) {
	cursor = Tk_GetCursor(interp, tkwin, Tk_GetUid((char *)cursorName));
    }
    Tcl_ResetResult(interp);
    return cursor;
}

/*
 * ------------------------------------------------------------------------
 *
 *  UpdateToken --
 *
 *	Invoked when the event loop is idle to determine whether or not
 *	the current drag&drop token position is over another drag&drop
 *	target.
 *
 * ------------------------------------------------------------------------
 */
static void
UpdateToken(clientData)
    ClientData clientData;	/* widget data */
{
    Source *srcPtr = clientData;
    Token *tokenPtr = &(srcPtr->token);

    ChangeToken(tokenPtr, tokenPtr->active);
    /*
     *  If the source has a site command, then invoke it to
     *  modify the appearance of the token window.  Pass any
     *  errors onto the drag&drop error handler.
     */
    if (srcPtr->siteCmd) {
	char buffer[200];
	Tcl_DString dString;
	int result;
	SubstDescriptors subs[2];
	
	sprintf(buffer, "%d", tokenPtr->active);
	subs[0].letter = 's';
	subs[0].value = buffer;
	subs[1].letter = 't';
	subs[1].value = Tk_PathName(tokenPtr->tkwin);
	
	Tcl_DStringInit(&dString);
	result = Tcl_Eval(srcPtr->interp, 
			  ExpandPercents(srcPtr->siteCmd, subs, 2, &dString));
	Tcl_DStringFree(&dString);
	if ((result != TCL_OK) && (errorCmd != NULL) && (*errorCmd)) {
	    (void)Tcl_VarEval(srcPtr->interp, errorCmd, " {",
	      Tcl_GetStringResult(srcPtr->interp), "}", (char *)NULL);
	}
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  RejectToken --
 *
 *	Draws a rejection mark on the current drag&drop token, and arranges
 *	for the token to be unmapped after a small delay.
 *
 * ------------------------------------------------------------------------
 */
static void
RejectToken(tokenPtr)
    Token *tokenPtr;
{
    int divisor = 6;		/* controls size of rejection symbol */
    int w, h, lineWidth, x, y, margin;

    margin = 2 * tokenPtr->borderWidth;
    w = Tk_Width(tokenPtr->tkwin) - 2 * margin;
    h = Tk_Height(tokenPtr->tkwin) - 2 * margin;
    lineWidth = (w < h) ? w / divisor : h / divisor;
    lineWidth = (lineWidth < 1) ? 1 : lineWidth;

    w = h = lineWidth * (divisor - 1);
    x = (Tk_Width(tokenPtr->tkwin) - w) / 2;
    y = (Tk_Height(tokenPtr->tkwin) - h) / 2;

    /*
     *  Draw the rejection symbol background (\) on the token window...
     */
    XSetLineAttributes(Tk_Display(tokenPtr->tkwin), tokenPtr->rejectBgGC,
	lineWidth + 4, LineSolid, CapButt, JoinBevel);

    XDrawArc(Tk_Display(tokenPtr->tkwin), Tk_WindowId(tokenPtr->tkwin),
	tokenPtr->rejectBgGC, x, y, w, h, 0, 23040);

    XDrawLine(Tk_Display(tokenPtr->tkwin), Tk_WindowId(tokenPtr->tkwin),
	tokenPtr->rejectBgGC, x + lineWidth, y + lineWidth, x + w - lineWidth,
	y + h - lineWidth);

    /*
     *  Draw the rejection symbol foreground (\) on the token window...
     */
    XSetLineAttributes(Tk_Display(tokenPtr->tkwin), tokenPtr->rejectFgGC,
	lineWidth, LineSolid, CapButt, JoinBevel);

    XDrawArc(Tk_Display(tokenPtr->tkwin), Tk_WindowId(tokenPtr->tkwin),
	tokenPtr->rejectFgGC, x, y, w, h, 0, 23040);

    XDrawLine(Tk_Display(tokenPtr->tkwin), Tk_WindowId(tokenPtr->tkwin),
	tokenPtr->rejectFgGC, x + lineWidth, y + lineWidth, x + w - lineWidth,
	y + h - lineWidth);

    /*
     *  Arrange for token window to disappear eventually.
     */
    tokenPtr->timer = Tcl_CreateTimerHandler(1000, (Tcl_TimerProc *) HideToken,
	     tokenPtr);
}

/*
 * ------------------------------------------------------------------------
 *
 *  ConfigureToken --
 *
 * ------------------------------------------------------------------------
 */
static int
ConfigureToken(interp, srcPtr, argc, argv)
    Tcl_Interp *interp;
    Source *srcPtr;
    int argc;
    char **argv;
{
    Token *tokenPtr;

    tokenPtr = &(srcPtr->token);
    if (Tk_ConfigureWidget(interp, srcPtr->tkwin, tokenConfigSpecs, argc, argv,
	    (char *)tokenPtr, TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    return ConfigureSource(interp, srcPtr, 0, (char **)NULL,
	TK_CONFIG_ARGV_ONLY);
}

/*
 * ------------------------------------------------------------------------
 *
 *  CreateToken --
 *
 * ------------------------------------------------------------------------
 */
static int
CreateToken(interp, srcPtr)
    Tcl_Interp *interp;
    Source *srcPtr;
{
    XSetWindowAttributes attrs;
    Tk_Window tkwin;
    char string[200];
    static int nextTokenId = 0;
    unsigned int mask;
    Token *tokenPtr = &(srcPtr->token);

    sprintf(string, "dd-token%d", ++nextTokenId);

    /* Create toplevel on parent's screen. */
    tkwin = Tk_CreateWindow(interp, srcPtr->tkwin, string, "");
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    Tk_SetClass(tkwin, className);
    Tk_CreateEventHandler(tkwin, ExposureMask | StructureNotifyMask,
	TokenEventProc, tokenPtr);

    attrs.override_redirect = True;
    attrs.backing_store = WhenMapped;
    attrs.save_under = True;
    mask = CWOverrideRedirect | CWSaveUnder | CWBackingStore;
    Tk_ChangeWindowAttributes(tkwin, mask, &attrs);

    Tk_SetInternalBorder(tkwin, tokenPtr->borderWidth + 2);
    tokenPtr->tkwin = tkwin;
#ifdef WIN32
    {
	Tk_FakeWin *winPtr = (Tk_FakeWin *) tkwin;
	winPtr->dummy18 = tokenPtr;
    }
#endif /* WIN32 */
    Tk_MakeWindowExist(tkwin);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  CreateSource --
 *
 *	Looks for a Source record in the hash table for drag&drop source
 *	widgets.  Creates a new record if the widget name is not already
 *	registered.  Returns a pointer to the desired record.
 *
 * ------------------------------------------------------------------------
 */
static Source *
CreateSource(interp, pathName, newPtr)
    Tcl_Interp *interp;
    char *pathName;		/* widget pathname for desired record */
    int *newPtr;		/* returns non-zero => new record created */
{
    Blt_HashEntry *hPtr;
    Tk_Window tkwin;
    Source *srcPtr;

    tkwin = Tk_NameToWindow(interp, pathName, Tk_MainWindow(interp));
    if (tkwin == NULL) {
	return NULL;
    }
    hPtr = Blt_CreateHashEntry(&sourceTable, (char *)tkwin, newPtr);
    if (!(*newPtr)) {
	return (Source *) Blt_GetHashValue(hPtr);
    }
    srcPtr = Blt_Calloc(1, sizeof(Source));
    assert(srcPtr);
    srcPtr->tkwin = tkwin;
    srcPtr->display = Tk_Display(tkwin);
    srcPtr->interp = interp;
    srcPtr->token.anchor = TK_ANCHOR_SE;
    srcPtr->token.relief = TK_RELIEF_RAISED;
    srcPtr->token.activeRelief = TK_RELIEF_SUNKEN;
    srcPtr->token.borderWidth = srcPtr->token.activeBorderWidth = 3;
    srcPtr->hashPtr = hPtr;
    Blt_InitHashTable(&(srcPtr->handlerTable), BLT_STRING_KEYS);
    if (ConfigureSource(interp, srcPtr, 0, (char **)NULL, 0) != TCL_OK) {
	DestroySource(srcPtr);
	return NULL;
    }
    Blt_SetHashValue(hPtr, srcPtr);
    /*
     *  Arrange for the window to unregister itself when it
     *  is destroyed.
     */
    Tk_CreateEventHandler(tkwin, StructureNotifyMask, SourceEventProc, srcPtr);
    return srcPtr;
}

/*
 * ------------------------------------------------------------------------
 *
 *  DestroySource --
 *
 *	Looks for a Source record in the hash table for drag&drop source
 *	widgets.  Destroys the record if found.
 *
 * ------------------------------------------------------------------------
 */
static void
DestroySource(srcPtr)
    Source *srcPtr;
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    char *cmd;

    Tcl_CancelIdleCall(UpdateToken, srcPtr);
    if (srcPtr->token.timer) {
	Tcl_DeleteTimerHandler(srcPtr->token.timer);
    }
    Tk_FreeOptions(configSpecs, (char *)srcPtr, srcPtr->display, 0);

    if (srcPtr->token.rejectFgGC != NULL) {
	Tk_FreeGC(srcPtr->display, srcPtr->token.rejectFgGC);
    }
    if (srcPtr->token.rejectBgGC != NULL) {
	Tk_FreeGC(srcPtr->display, srcPtr->token.rejectBgGC);
    }
    if (srcPtr->pkgCmdResult) {
	Blt_Free(srcPtr->pkgCmdResult);
    }
    if (srcPtr->rootPtr != NULL) {
	RemoveWindow(srcPtr->rootPtr);
    }
    if (srcPtr->cursor != None) {
	Tk_FreeCursor(srcPtr->display, srcPtr->cursor);
    }
    if (srcPtr->token.cursor != None) {
	Tk_FreeCursor(srcPtr->display, srcPtr->token.cursor);
    }
    Blt_Free(srcPtr->sendTypes);

    for (hPtr = Blt_FirstHashEntry(&(srcPtr->handlerTable), &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	cmd = (char *)Blt_GetHashValue(hPtr);
	if (cmd != NULL) {
	    Blt_Free(cmd);
	}
    }
    Blt_DeleteHashTable(&(srcPtr->handlerTable));
    if (srcPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(&sourceTable, srcPtr->hashPtr);
    }
    Blt_Free(srcPtr);
}

/*
 * ------------------------------------------------------------------------
 *
 *  GetSource --
 *
 *	Looks for a Source record in the hash table for drag&drop source
 *	widgets.  Returns a pointer to the desired record.
 *
 * ------------------------------------------------------------------------
 */
static int
GetSource(interp, pathName, srcPtrPtr)
    Tcl_Interp *interp;
    char *pathName;		/* widget pathname for desired record */
    Source **srcPtrPtr;
{
    Blt_HashEntry *hPtr;
    Tk_Window tkwin;

    tkwin = Tk_NameToWindow(interp, pathName, Tk_MainWindow(interp));
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    hPtr = Blt_FindHashEntry(&sourceTable, (char *)tkwin);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "window \"", pathName,
	     "\" has not been initialized as a drag&drop source", (char *)NULL);
	return TCL_ERROR;
    }
    *srcPtrPtr = (Source *)Blt_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *
 *  ConfigureSource --
 *
 *	Called to process an (argc,argv) list to configure (or
 *	reconfigure) a drag&drop source widget.
 *
 * ------------------------------------------------------------------------
 */
static int
ConfigureSource(interp, srcPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* current interpreter */
    register Source *srcPtr;	/* drag&drop source widget record */
    int argc;			/* number of arguments */
    char **argv;		/* argument strings */
    int flags;			/* flags controlling interpretation */
{
    unsigned long gcMask;
    XGCValues gcValues;
    GC newGC;
    Tcl_DString dString;
    Tcl_CmdInfo cmdInfo;
    int result;

    /*
     *  Handle the bulk of the options...
     */
    if (Tk_ConfigureWidget(interp, srcPtr->tkwin, configSpecs, argc, argv,
	    (char *)srcPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    /*
     *  Check the button binding for valid range (0 or 1-5)
     */
    if ((srcPtr->button < 0) || (srcPtr->button > 5)) {
	Tcl_AppendResult(interp, 
		 "button number must be 1-5, or 0 for no bindings",
		 (char *)NULL);
	return TCL_ERROR;
    }
    /*
     *  Set up the rejection foreground GC for the token window...
     */
    gcValues.foreground = srcPtr->token.rejectFg->pixel;
    gcValues.subwindow_mode = IncludeInferiors;
    gcValues.graphics_exposures = False;
    gcMask = GCForeground | GCSubwindowMode | GCGraphicsExposures;

    if (srcPtr->token.rejectStipple != None) {
	gcValues.stipple = srcPtr->token.rejectStipple;
	gcValues.fill_style = FillStippled;
	gcMask |= GCForeground | GCStipple | GCFillStyle;
    }
    newGC = Tk_GetGC(srcPtr->tkwin, gcMask, &gcValues);

    if (srcPtr->token.rejectFgGC != NULL) {
	Tk_FreeGC(srcPtr->display, srcPtr->token.rejectFgGC);
    }
    srcPtr->token.rejectFgGC = newGC;

    /*
     *  Set up the rejection background GC for the token window...
     */
    gcValues.foreground = srcPtr->token.rejectBg->pixel;
    gcValues.subwindow_mode = IncludeInferiors;
    gcValues.graphics_exposures = False;
    gcMask = GCForeground | GCSubwindowMode | GCGraphicsExposures;

    newGC = Tk_GetGC(srcPtr->tkwin, gcMask, &gcValues);

    if (srcPtr->token.rejectBgGC != NULL) {
	Tk_FreeGC(srcPtr->display, srcPtr->token.rejectBgGC);
    }
    srcPtr->token.rejectBgGC = newGC;

    /*
     *  Reset the border width in case it has changed...
     */
    if (srcPtr->token.tkwin) {
	Tk_SetInternalBorder(srcPtr->token.tkwin,
	    srcPtr->token.borderWidth + 2);
    }
    if (!Tcl_GetCommandInfo(interp, "blt::Drag&DropInit", &cmdInfo)) {
	static char cmd[] = "source [file join $blt_library dragdrop.tcl]";

	if (Tcl_GlobalEval(interp, cmd) != TCL_OK) {
	    Tcl_AddErrorInfo(interp,
		    "\n    (while loading bindings for blt::drag&drop)");
	    return TCL_ERROR;
	}
    }
    Tcl_DStringInit(&dString);
    Blt_DStringAppendElements(&dString, "blt::Drag&DropInit",
      Tk_PathName(srcPtr->tkwin), Blt_Itoa(srcPtr->button), (char *)NULL);
    result = Tcl_Eval(interp, Tcl_DStringValue(&dString));
    Tcl_DStringFree(&dString);
    return result;
}

/*
 * ------------------------------------------------------------------------
 *
 *  SourceEventProc --
 *
 *	Invoked by Tk_HandleEvent whenever a DestroyNotify event is received
 *	on a registered drag&drop source widget.
 *
 * ------------------------------------------------------------------------
 */
static void
SourceEventProc(clientData, eventPtr)
    ClientData clientData;	/* drag&drop registration list */
    XEvent *eventPtr;		/* event description */
{
    Source *srcPtr = (Source *) clientData;

    if (eventPtr->type == DestroyNotify) {
	DestroySource(srcPtr);
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  FindTarget --
 *
 *	Looks for a Target record in the hash table for drag&drop
 *	target widgets.  Creates a new record if the widget name is
 *	not already registered.  Returns a pointer to the desired
 *	record.
 *
 * ------------------------------------------------------------------------
 */
static Target *
FindTarget(tkwin)
    Tk_Window tkwin;		/* Widget pathname for desired record */
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&targetTable, (char *)tkwin);
    if (hPtr == NULL) {
	return NULL;
    }
    return (Target *) Blt_GetHashValue(hPtr);
}


/*
 * ------------------------------------------------------------------------
 *
 *  CreateTarget --
 *
 *	Looks for a Target record in the hash table for drag&drop
 *	target widgets.  Creates a new record if the widget name is
 *	not already registered.  Returns a pointer to the desired
 *	record.
 *
 * ------------------------------------------------------------------------
 */
static Target *
CreateTarget(interp, tkwin)
    Tcl_Interp *interp;
    Tk_Window tkwin;		/* Widget pathname for desired record */
{
    Target *targetPtr;
    int isNew;

    targetPtr = Blt_Calloc(1, sizeof(Target));
    assert(targetPtr);
    targetPtr->display = Tk_Display(tkwin);
    targetPtr->tkwin = tkwin;
    Blt_InitHashTable(&(targetPtr->handlerTable), BLT_STRING_KEYS);
    targetPtr->hashPtr = Blt_CreateHashEntry(&targetTable, (char *)tkwin, 
	     &isNew);
    Blt_SetHashValue(targetPtr->hashPtr, targetPtr);

    /* 
     * Arrange for the target to removed if the host window is destroyed.  
     */
    Tk_CreateEventHandler(tkwin, StructureNotifyMask, TargetEventProc,
	  targetPtr);
    /*
     *  If this is a new target, attach a property to identify
     *  window as "drag&drop" target, and arrange for the window
     *  to un-register itself when it is destroyed.
     */
    Tk_MakeWindowExist(targetPtr->tkwin);
    AddTargetProperty(interp, targetPtr);
    return targetPtr;
}

/*
 * ------------------------------------------------------------------------
 *
 *  DestroyTarget --
 *
 * ------------------------------------------------------------------------
 */
static void
DestroyTarget(data)
    DestroyData data;
{
    Target *targetPtr = (Target *)data;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    char *cmd;

    for (hPtr = Blt_FirstHashEntry(&(targetPtr->handlerTable), &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	cmd = (char *)Blt_GetHashValue(hPtr);
	if (cmd != NULL) {
	    Blt_Free(cmd);
	}
    }
    Blt_DeleteHashTable(&(targetPtr->handlerTable));
    if (targetPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(&targetTable, targetPtr->hashPtr);
    }
    Tk_DeleteEventHandler(targetPtr->tkwin, StructureNotifyMask,
	  TargetEventProc, targetPtr);
    Blt_Free(targetPtr);
}

/*
 * ------------------------------------------------------------------------
 *
 *  TargetEventProc --
 *
 *  Invoked by Tk_HandleEvent whenever a DestroyNotify event is received
 *  on a registered drag&drop target widget.
 *
 * ------------------------------------------------------------------------
 */
static void
TargetEventProc(clientData, eventPtr)
    ClientData clientData;	/* drag&drop registration list */
    XEvent *eventPtr;		/* event description */
{
    Target *targetPtr = (Target *) clientData;

    if (eventPtr->type == DestroyNotify) {
#ifdef	WIN32
	/*
	 * Under Win32 the properties must be removed before the window
	 * can be destroyed.
	 */
	RemoveProperty(targetPtr->tkwin);
#endif
	DestroyTarget((DestroyData)targetPtr);
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  DndSend --
 *
 *	Invoked after a drop operation to send data to the drop
 *	application.
 *
 * ------------------------------------------------------------------------
 */
static void
DndSend(srcPtr)
    register Source *srcPtr;	/* drag&drop source record */
{
    int status;
    SubstDescriptors subs[3];
    Tcl_DString dString;
    Blt_HashEntry *hPtr;
    char *dataType;
    char **targetInfo;
    char *cmd;

    /* See if current position is over drop point.  */
    if (!OverTarget(srcPtr, srcPtr->token.lastX, srcPtr->token.lastY)) {
	return;
    }
    targetInfo = srcPtr->windowPtr->targetInfo;
    Tcl_DStringInit(&dString);
    Blt_DStringAppendElements(&dString, "send", targetInfo[INTERP_NAME],
	dragDropCmd, "location", (char *)NULL);
    Tcl_DStringAppendElement(&dString, Blt_Itoa(srcPtr->token.lastX));
    Tcl_DStringAppendElement(&dString, Blt_Itoa(srcPtr->token.lastY));
    status = Tcl_Eval(srcPtr->interp, Tcl_DStringValue(&dString));

    Tcl_DStringFree(&dString);
    if (status != TCL_OK) {
	goto reject;
    }
    if (targetInfo[DATA_TYPE] == NULL) {
	Blt_HashSearch cursor;

	hPtr = Blt_FirstHashEntry(&(srcPtr->handlerTable), &cursor);
	dataType = Blt_GetHashKey(&(srcPtr->handlerTable), hPtr);
    } else {
	hPtr = Blt_FindHashEntry(&(srcPtr->handlerTable),
	    targetInfo[DATA_TYPE]);
	dataType = targetInfo[DATA_TYPE];
    }
    /* Start building the command line here, before we invoke any Tcl
     * commands. The is because the Tcl command may let in another
     * drag event and change the target property data. */
    Tcl_DStringInit(&dString);
    Blt_DStringAppendElements(&dString, "send", targetInfo[INTERP_NAME],
	dragDropCmd, "target", targetInfo[TARGET_NAME], "handle",
	dataType, (char *)NULL);
    cmd = NULL;
    if (hPtr != NULL) {
	cmd = (char *)Blt_GetHashValue(hPtr);
    }
    if (cmd != NULL) {
	Tcl_DString cmdString;

	subs[0].letter = 'i';
	subs[0].value = targetInfo[INTERP_NAME];
	subs[1].letter = 'w';
	subs[1].value = targetInfo[TARGET_NAME];
	subs[2].letter = 'v';
	subs[2].value = srcPtr->pkgCmdResult;
	
	Tcl_DStringInit(&cmdString);
	status = Tcl_Eval(srcPtr->interp, 
		ExpandPercents(cmd, subs, 3, &cmdString));
	Tcl_DStringFree(&cmdString);
        if (status != TCL_OK) {
	    goto reject;
        }
	Tcl_DStringAppendElement(&dString, Tcl_GetStringResult(srcPtr->interp));
    } else {
	Tcl_DStringAppendElement(&dString, srcPtr->pkgCmdResult);
    }

    /*
     *  Part 2:	Now tell target application to handle the data.
     */
    status = Tcl_Eval(srcPtr->interp, Tcl_DStringValue(&dString));
    Tcl_DStringFree(&dString);
    if (status != TCL_OK) {
	goto reject;
    }
    HideToken(&(srcPtr->token));
    return;
  reject:
    /*
     * Give failure information to user.  If an error occurred and an
     * error proc is defined, then use it to handle the error.
     */
    RejectToken(&(srcPtr->token));
    if (errorCmd != NULL) {
	Tcl_VarEval(srcPtr->interp, errorCmd, " {",
	    Tcl_GetStringResult(srcPtr->interp), "}", (char *)NULL);
    }
}

/*
 * ------------------------------------------------------------------------
 *
 *  InitRoot --
 *
 *	Invoked at the start of a "drag" operation to capture the
 *	positions of all windows on the current root.  Queries the
 *	entire window hierarchy and determines the placement of each
 *	window.  Queries the "BltDrag&DropTarget" property info where
 *	appropriate.  This information is used during the drag
 *	operation to determine when the drag&drop token is over a
 *	valid drag&drop target.
 *
 *  Results:
 *	Returns the record for the root window, which contains records
 *	for all other windows as children.
 *
 * ------------------------------------------------------------------------
 */
static void
InitRoot(srcPtr)
    Source *srcPtr;
{
    srcPtr->rootPtr = Blt_Calloc(1, sizeof(AnyWindow));
    assert(srcPtr->rootPtr);
#ifdef WIN32
    srcPtr->rootPtr->nativeWindow = GetDesktopWindow();
#else
    srcPtr->rootPtr->nativeWindow = DefaultRootWindow(srcPtr->display);
#endif
    srcPtr->windowPtr = NULL;
    QueryWindow(srcPtr->display, srcPtr->rootPtr);
}

/*
 * ------------------------------------------------------------------------
 *
 *  FindTopWindow --
 *
 *	Searches for the topmost window at a given pair of X-Y coordinates.
 *
 *  Results:
 *	Returns a pointer to the node representing the window containing
 *	the point.  If one can't be found, NULL is returned.
 *
 * ------------------------------------------------------------------------
 */
static AnyWindow *
FindTopWindow(srcPtr, x, y)
    Source *srcPtr;
    int x, y;
{
    AnyWindow *rootPtr;
    register Blt_ChainLink *linkPtr;
    register AnyWindow *windowPtr;
    WINDOW nativeTokenWindow;

    rootPtr = srcPtr->rootPtr;
    if (!rootPtr->initialized) {
	QueryWindow(srcPtr->display, rootPtr);
    }
    if ((x < rootPtr->x1) || (x > rootPtr->x2) ||
	(y < rootPtr->y1) || (y > rootPtr->y2)) {
	return NULL;		/* Point is not over window  */
    }
    windowPtr = rootPtr;

    nativeTokenWindow = (WINDOW)Blt_GetRealWindowId(srcPtr->token.tkwin);
    /*
     * The window list is ordered top to bottom, so stop when we find
     * the first child that contains the X-Y coordinate. It will be
     * the topmost window in that hierarchy.  If none exists, then we
     * already have the topmost window.
     */
  descend:
    for (linkPtr = Blt_ChainFirstLink(rootPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rootPtr = Blt_ChainGetValue(linkPtr);
	if (!rootPtr->initialized) {
	    QueryWindow(srcPtr->display, rootPtr);
	}
	if (rootPtr->nativeWindow == nativeTokenWindow) {
	    continue;		/* Don't examine the token window. */
	}
	if ((x >= rootPtr->x1) && (x <= rootPtr->x2) &&
	    (y >= rootPtr->y1) && (y <= rootPtr->y2)) {
	    /*
	     * Remember the last window containing the pointer and
	     * descend into its window hierarchy. We'll look for a
	     * child that also contains the pointer.
	     */
	    windowPtr = rootPtr;
	    goto descend;
	}
    }
    return windowPtr;
}

/*
 * ------------------------------------------------------------------------
 *
 *  OverTarget --
 *
 *      Checks to see if a compatible drag&drop target exists at the
 *      given position.  A target is "compatible" if it is a drag&drop
 *      window, and if it has a handler that is compatible with the
 *      current source window.
 *
 *  Results:
 *	Returns a pointer to a structure describing the target, or NULL
 *	if no compatible target is found.
 *
 * ------------------------------------------------------------------------
 */
static int
OverTarget(srcPtr, x, y)
    Source *srcPtr;		/* drag&drop source window */
    int x, y;			/* current drag&drop location
				 * (in virtual coords) */
{
    int virtX, virtY;
    int dummy;
    AnyWindow *newPtr, *oldPtr;
    char **elemArr;
    int nElems;
    char *data;
    int result;

    /*
     * If no window info has been been gathered yet for this target,
     * then abort this call.  This probably means that the token is
     * moved before it has been properly built.
     */
    if (srcPtr->rootPtr == NULL) {
	return FALSE;
    }
    if (srcPtr->sendTypes == NULL) {
	return FALSE;		/* Send is turned off. */
    }

    /* Adjust current location for virtual root windows.  */
    Tk_GetVRootGeometry(srcPtr->tkwin, &virtX, &virtY, &dummy, &dummy);
    x += virtX;
    y += virtY;

    oldPtr = srcPtr->windowPtr;
    srcPtr->windowPtr = NULL;

    newPtr = FindTopWindow(srcPtr, x, y);
    if (newPtr == NULL) {
	return FALSE;		/* Not over a window. */
    }
    if ((!srcPtr->selfTarget) &&
	(GetNativeWindow(srcPtr->tkwin) == newPtr->nativeWindow)) {
	return FALSE;		/* If the self-target flag isn't set,
				 *  don't allow the source window to
				 *  drop onto itself.  */
    }
    if (newPtr == oldPtr) {
	srcPtr->windowPtr = oldPtr;
	/* No need to collect the target information if we're still
	 * over the same window. */
	return (newPtr->targetInfo != NULL);
    }

    /* See if this window has a "BltDrag&DropTarget" property. */
    data = GetProperty(srcPtr->display, newPtr->nativeWindow);
    if (data == NULL) {
	return FALSE;		/* No such property on window. */
    }
    result = Tcl_SplitList(srcPtr->interp, data, &nElems, &elemArr);
    XFree((char *)data);
    if (result != TCL_OK) {
	return FALSE;		/* Malformed property list. */
    }
    srcPtr->windowPtr = newPtr;
    /* Interpreter name, target name, type1, type2, ... */
    if (nElems > 2) {
	register char **s;
	int count;
	register int i;

	/*
	 * The candidate target has a list of possible types.
	 * Compare this with what types the source is willing to
	 * transmit and compress the list down to just the matching
	 * types.  It's up to the target to request the specific type
	 * it wants.
	 */
	count = 2;
	for (i = 2; i < nElems; i++) {
	    for (s = srcPtr->sendTypes; *s != NULL; s++) {
		if (((**s == 'a') && (strcmp(*s, "all") == 0)) ||
		    ((**s == elemArr[i][0]) && (strcmp(*s, elemArr[i]) == 0))) {
		    elemArr[count++] = elemArr[i];
		}
	    }
	}
	if (count == 2) {
	    Blt_Free(elemArr);
	    fprintf(stderr, "source/target mismatch: No matching types\n");
	    return FALSE;	/* No matching data type. */
	}
	elemArr[count] = NULL;
    }
    newPtr->targetInfo = elemArr;
    return TRUE;
}

/*
 * ------------------------------------------------------------------------
 *
 *  RemoveWindow --
 *
 * ------------------------------------------------------------------------
 */
static void
RemoveWindow(windowPtr)
    AnyWindow *windowPtr;		/* window rep to be freed */
{
    AnyWindow *childPtr;
    Blt_ChainLink *linkPtr;

    /* Throw away leftover slots. */
    for (linkPtr = Blt_ChainFirstLink(windowPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	childPtr = Blt_ChainGetValue(linkPtr);
	RemoveWindow(childPtr);
    }
    Blt_ChainDestroy(windowPtr->chainPtr);
    if (windowPtr->targetInfo != NULL) {
	Blt_Free(windowPtr->targetInfo);
    }
    Blt_Free(windowPtr);
}

/*
 * ------------------------------------------------------------------------
 *
 *  QueryWindow --
 *
 *	Invoked during "drag" operations. Digs into the root window
 *	hierarchy and caches the resulting information.
 *	If a point coordinate lies within an uninitialized AnyWindow,
 *	this routine is called to query window coordinates and
 *	drag&drop info.  If this particular window has any children,
 *	more uninitialized AnyWindow structures are allocated.
 *	Further queries will cause these structures to be initialized
 *	in turn.
 *
 * ------------------------------------------------------------------------
 */
static void
QueryWindow(display, windowPtr)
    Display *display;
    AnyWindow *windowPtr;		/* window rep to be initialized */
{
    int visible;

    if (windowPtr->initialized) {
	return;
    }
    /*
     *  Query for the window coordinates.
     */
    visible = GetWindowRegion(display, windowPtr->nativeWindow, 
      &(windowPtr->x1), &(windowPtr->y1), &(windowPtr->x2), &(windowPtr->y2));
    if (visible) {
	Blt_ChainLink *linkPtr;
	Blt_Chain *chainPtr;
	AnyWindow *childPtr;

#ifndef WIN32
	/* Add offset from parent's origin to coordinates */
	if (windowPtr->parentPtr != NULL) {
	    windowPtr->x1 += windowPtr->parentPtr->x1;
	    windowPtr->y1 += windowPtr->parentPtr->y1;
	    windowPtr->x2 += windowPtr->parentPtr->x1;
	    windowPtr->y2 += windowPtr->parentPtr->y1;
	}
#endif
	/*
	 * Collect a list of child windows, sorted in z-order.  The
	 * topmost window will be first in the list.
	 */
	chainPtr = GetWindowZOrder(display, windowPtr->nativeWindow);

	/* Add and initialize extra slots if needed. */
	for (linkPtr = Blt_ChainFirstLink(chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    childPtr = Blt_Calloc(1, sizeof(AnyWindow));
	    assert(childPtr);
	    childPtr->initialized = FALSE;
	    childPtr->nativeWindow = (WINDOW)Blt_ChainGetValue(linkPtr);
	    childPtr->parentPtr = windowPtr;
	    Blt_ChainSetValue(linkPtr, childPtr);
	}
	windowPtr->chainPtr = chainPtr;
    } else {
	/* If it's not viewable don't bother doing anything else. */
	windowPtr->x1 = windowPtr->y1 = windowPtr->x2 = windowPtr->y2 = -1;
	windowPtr->chainPtr = NULL;
    }
    windowPtr->initialized = TRUE;
}

/*
 * ------------------------------------------------------------------------
 *
 *  AddTargetProperty --
 *
 *	Attaches a drag&drop property to the given target window.
 *	This property allows us to recognize the window later as a
 *	valid target. It also stores important information including
 *	the interpreter managing the target and the pathname of the
 *	target window.  Usually this routine is called when the target
 *	is first registered or first exposed (so that the X-window
 *	really exists).
 *
 * ------------------------------------------------------------------------
 */
static void
AddTargetProperty(interp, targetPtr)
    Tcl_Interp *interp;
    Target *targetPtr;		/* drag&drop target window data */
{
    Tcl_DString dString;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;

    if (targetPtr->tkwin == NULL) {
	return;
    }
    Tcl_DStringInit(&dString);
    /*
     * Each target window's dnd property contains
     *
     *	1. name of the application (ie. the interpreter's name).
     *	2. Tk path name of the target window.
     *  3. List of all the data types that can be handled. If none
     *     are listed, then all can be handled.
     */
    Tcl_DStringAppendElement(&dString, Tk_Name(Tk_MainWindow(interp)));
    Tcl_DStringAppendElement(&dString, Tk_PathName(targetPtr->tkwin));
    for (hPtr = Blt_FirstHashEntry(&(targetPtr->handlerTable), &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	Tcl_DStringAppendElement(&dString,
	    Blt_GetHashKey(&(targetPtr->handlerTable), hPtr));
    }
    SetProperty(targetPtr->tkwin, Tcl_DStringValue(&dString));
    Tcl_DStringFree(&dString);
}

/*
 * ------------------------------------------------------------------------
 *
 *  ExpandPercents --
 *
 *	Expands all percent substitutions found in the input "str"
 *	that match specifications in the "subs" list.  Any percent
 *	field that is not found in the "subs" list is left alone.
 *	Returns a string that remains valid until the next call to
 *	this routine.
 *
 * ------------------------------------------------------------------------
 */
static char *
ExpandPercents(string, subsArr, nSubs, resultPtr)
    char *string;		/* Incoming command string */
    SubstDescriptors *subsArr;	/* Array of known substitutions */
    int nSubs;			/* Number of elements in subs array */
    Tcl_DString *resultPtr;
{
    register char *chunk, *p;
    char letter;
    char percentSign;
    int i;

    /*
     *  Scan through the copy of the input string, look for
     *  the next '%' character, and try to make the substitution.
     *  Continue doing this to the end of the string.
     */
    chunk = p = string;
    while ((p = strchr(p, '%')) != NULL) {

	/* Copy up to the percent sign.  Repair the string afterwards */
	percentSign = *p;
	*p = '\0';
	Tcl_DStringAppend(resultPtr, chunk, -1);
	*p = percentSign;

	/* Search for a matching substition rule */
	letter = *(p + 1);
	for (i = 0; i < nSubs; i++) {
	    if (subsArr[i].letter == letter) {
		break;
	    }
	}
	if (i < nSubs) {
	    /* Make the substitution */
	    Tcl_DStringAppend(resultPtr, subsArr[i].value, -1);
	} else {
	    /* Copy in the %letter verbatim */
	    char verbatim[3];

	    verbatim[0] = '%';
	    verbatim[1] = letter;
	    verbatim[2] = '\0';
	    Tcl_DStringAppend(resultPtr, verbatim, -1);
	}
	p += 2;			/* Skip % + letter */
	if (letter == '\0') {
	    p += 1;		/* Premature % substitution (end of string) */
	}
	chunk = p;
    }
    /* Pick up last chunk if a substition wasn't the last thing in the string */
    if (*chunk != '\0') {
	Tcl_DStringAppend(resultPtr, chunk, -1);
    }
    return Tcl_DStringValue(resultPtr);
}


static int
DragOp(interp, argc, argv)
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int x, y;
    Token *tokenPtr;
    int status;
    Source *srcPtr;
    SubstDescriptors subst[2];
    int active;
    CONST char *result;

    /*
     *  HANDLE:  drag&drop drag <path> <x> <y>
     */
    if (argc != 5) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " drag pathname x y\"", (char *)NULL);
	return TCL_ERROR;
    }
    if ((GetSource(interp, argv[2], &srcPtr) != TCL_OK) || 
	(Tcl_GetInt(interp, argv[3], &x) != TCL_OK) ||
	(Tcl_GetInt(interp, argv[4], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    tokenPtr = &(srcPtr->token);

    tokenPtr->lastX = locX = x;	/* Save drag&drop location */
    tokenPtr->lastY = locY = y;

    /* If HideToken() is pending, then do it now!  */
    if (tokenPtr->timer != 0) {
	Tcl_DeleteTimerHandler(tokenPtr->timer);
	HideToken(tokenPtr);
    }

    /*
     *  If pkgCmd is in progress, then ignore subsequent calls
     *  until it completes.  Only perform drag if pkgCmd
     *  completed successfully and token window is mapped.
     */
    if ((!Tk_IsMapped(tokenPtr->tkwin)) && (!srcPtr->pkgCmdInProgress)) {
	Tcl_DString dString;

	/*
	 *  No list of send handlers?  Then source is disabled.
	 *  Abort drag quietly.
	 */
	if (srcPtr->sendTypes == NULL) {
	    return TCL_OK;
	}
	/*
	 *  No token command?  Then cannot build token.
	 *  Signal error.
	 */
	if (srcPtr->pkgCmd == NULL) {
	    Tcl_AppendResult(interp, "missing -packagecmd: ", argv[2],
		(char *)NULL);
	    return TCL_ERROR;
	}
	/*
	 *  Execute token command to initialize token window.
	 */
	srcPtr->pkgCmdInProgress = TRUE;
	subst[0].letter = 'W';
	subst[0].value = Tk_PathName(srcPtr->tkwin);
	subst[1].letter = 't';
	subst[1].value = Tk_PathName(tokenPtr->tkwin);

	Tcl_DStringInit(&dString);
	status = Tcl_Eval(srcPtr->interp,
	    ExpandPercents(srcPtr->pkgCmd, subst, 2, &dString));
	Tcl_DStringFree(&dString);

	srcPtr->pkgCmdInProgress = FALSE;

	result = Tcl_GetStringResult(interp);
	/*
	 *  Null string from the package command?
	 *  Then quietly abort the drag&drop operation.
	 */
	if (result[0] == '\0') {
	    return TCL_OK;
	}

	/* Save result of token command for send command.  */
	if (srcPtr->pkgCmdResult != NULL) {
	    Blt_Free(srcPtr->pkgCmdResult);
	}
	srcPtr->pkgCmdResult = Blt_Strdup(result);
	if (status != TCL_OK) {
	    /*
	     * Token building failed.  If an error handler is defined,
	     * then signal the error.  Otherwise, abort quietly.
	     */
	    if ((errorCmd != NULL) && (errorCmd[0] != '\0')) {
		return Tcl_VarEval(interp, errorCmd, " {", result, "}",
		    (char *)NULL);
	    }
	    return TCL_OK;
	}
	/* Install token cursor.  */
	if (tokenPtr->cursor != None) {
	    Tk_Cursor cursor;

	    /* Save the old cursor */
	    cursor = GetWidgetCursor(srcPtr->interp, srcPtr->tkwin);
	    if (srcPtr->cursor != None) {
		Tk_FreeCursor(srcPtr->display, srcPtr->cursor);
	    }
	    srcPtr->cursor = cursor;
	    /* Temporarily install the token cursor */
	    Tk_DefineCursor(srcPtr->tkwin, tokenPtr->cursor);
	}
	/*
	 *  Get ready to drag token window...
	 *  1) Cache info for all windows on root
	 *  2) Map token window to begin drag operation
	 */
	if (srcPtr->rootPtr != NULL) {
	    RemoveWindow(srcPtr->rootPtr);
	}
	InitRoot(srcPtr);

	nActive++;		/* One more drag&drop window active */

	if (Tk_WindowId(tokenPtr->tkwin) == None) {
	    Tk_MakeWindowExist(tokenPtr->tkwin);
	}
	if (!Tk_IsMapped(tokenPtr->tkwin)) {
	    Tk_MapWindow(tokenPtr->tkwin);
	}
	RaiseToken(tokenPtr);
    }

    /* Arrange to update status of token window.  */
    Tcl_CancelIdleCall(UpdateToken, srcPtr);

    active = OverTarget(srcPtr, x, y);
    if (tokenPtr->active != active) {
	tokenPtr->active = active;
	Tcl_DoWhenIdle(UpdateToken, srcPtr);
    }
    MoveToken(srcPtr, tokenPtr); /* Move token window to current drag point. */
    return TCL_OK;
}

/*
 *  HANDLE:  drag&drop drop <path> <x> <y>
 */
static int
DropOp(interp, argc, argv)
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Source *srcPtr;
    Token *tokenPtr;
    int x, y;

    if (argc < 5) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " drop pathname x y\"", (char *)NULL);
	return TCL_ERROR;
    }
    if ((GetSource(interp, argv[2], &srcPtr) != TCL_OK)  ||
	(Tcl_GetInt(interp, argv[3], &x) != TCL_OK) ||
	(Tcl_GetInt(interp, argv[4], &y) != TCL_OK)) {
	return TCL_ERROR;
    }
    tokenPtr = &(srcPtr->token);
    tokenPtr->lastX = locX = x;			/* Save drag&drop location */
    tokenPtr->lastY = locY = y;
    
    /* Put the cursor back to its usual state.  */
    if (srcPtr->cursor == None) {
	Tk_UndefineCursor(srcPtr->tkwin);
    } else {
	Tk_DefineCursor(srcPtr->tkwin, srcPtr->cursor);
    }
    Tcl_CancelIdleCall(UpdateToken, srcPtr);

    /*
     *  Make sure that token window was not dropped before it
     *  was either mapped or packed with info.
     */
    if ((Tk_IsMapped(tokenPtr->tkwin)) && (!srcPtr->pkgCmdInProgress)) {
	int active;

	active = OverTarget(srcPtr, tokenPtr->lastX, tokenPtr->lastY);
	if (tokenPtr->active != active) {
	    tokenPtr->active = active;
	    UpdateToken(srcPtr);
	}
	if (srcPtr->sendTypes != NULL) {
	    if (tokenPtr->active) {
		DndSend(srcPtr);
	    } else {
		HideToken(tokenPtr);
	    }
	}
	nActive--;		/* One less active token window. */
    }
    return TCL_OK;
}

/*
 *  HANDLE:  drag&drop errors ?<proc>?
 */
static int
ErrorsOp(interp, argc, argv)
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (argc == 3) {
	if (errorCmd) {
	    Blt_Free(errorCmd);
	}
	errorCmd = Blt_Strdup(argv[2]);
    } else if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
	    argv[0], " errors ?proc?\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, errorCmd, TCL_VOLATILE);
    return TCL_OK;
}

/*
 *  HANDLE:  drag&drop active
 */
static int
ActiveOp(interp, argc, argv)
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " active\"", (char *)NULL);
	return TCL_ERROR;
    }
    Blt_SetBooleanResult(interp, (nActive > 0));
    return TCL_OK;
}

/*
 *  HANDLE:  drag&drop location ?<x> <y>?
 */
static int
LocationOp(interp, argc, argv)
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if ((argc != 2) && (argc != 4)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " location ?x y?\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (argc == 4) {
	int x, y;

	if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK) ||
	    (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)) {
	    return TCL_ERROR;
	}
	locX = x;
	locY = y;
    }
    Tcl_AppendElement(interp, Blt_Itoa(locX));
    Tcl_AppendElement(interp, Blt_Itoa(locY));
    return TCL_OK;
}

/*
 *  HANDLE:  drag&drop token <pathName>
 */
static int
TokenOp(interp, argc, argv)
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Source *srcPtr;

    if (GetSource(interp, argv[2], &srcPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((argc > 3) && 
	(ConfigureToken(interp, srcPtr, argc - 3, argv + 3) != TCL_OK)) {
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, Tk_PathName(srcPtr->token.tkwin), TCL_VOLATILE);
    return TCL_OK;
}

static int
HandlerOpOp(srcPtr, interp, argc, argv)
    Source *srcPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    int isNew;
    char *cmd;

    /*
     *  HANDLE:  drag&drop source <pathName> handler \
     *             ?<data>? ?<scmd>...?
     */
    if (argc == 4) {
	/* Show source handler data types */
	for (hPtr = Blt_FirstHashEntry(&(srcPtr->handlerTable), &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    Tcl_AppendElement(interp,
		Blt_GetHashKey(&(srcPtr->handlerTable), hPtr));
	}
	return TCL_OK;
    }
    hPtr = Blt_CreateHashEntry(&(srcPtr->handlerTable), argv[4], &isNew);

    /*
     *  HANDLE:  drag&drop source <pathName> handler <data>
     *
     *    Create the new <data> type if it doesn't already
     *    exist, and return the code associated with it.
     */
    if (argc == 5) {
	cmd = (char *)Blt_GetHashValue(hPtr);
	if (cmd == NULL) {
	    cmd = "";
	}
	Tcl_SetResult(interp, cmd, TCL_VOLATILE);
	return TCL_OK;
    }
    /*
     *  HANDLE:  drag&drop source <pathName> handler \
     *               <data> <cmd> ?<arg>...?
     *
     *    Create the new <data> type and set its command
     */
    cmd = Tcl_Concat(argc - 5, argv + 5);
    Blt_SetHashValue(hPtr, cmd);
    return TCL_OK;
}

/*
 *  HANDLE:  drag&drop source
 *           drag&drop source <pathName> ?options...?
 *           drag&drop source <pathName> handler ?<data>? ?<scmd> <arg>...?
 */
static int
SourceOp(interp, argc, argv)
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Source *srcPtr;
    int isNew;
    Token *tokenPtr;

    if (argc == 2) {
	Blt_HashSearch cursor;
	Blt_HashEntry *hPtr;
	Tk_Window tkwin;

	for (hPtr = Blt_FirstHashEntry(&sourceTable, &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    tkwin = (Tk_Window)Blt_GetHashKey(&sourceTable, hPtr);
	    Tcl_AppendElement(interp, Tk_PathName(tkwin));
	}
	return TCL_OK;
    }
    /*
     *  Find or create source info...
     */
    srcPtr = CreateSource(interp, argv[2], &isNew);
    if (srcPtr == NULL) {
	return TCL_ERROR;
    }
    tokenPtr = &(srcPtr->token);
    if (argc > 3) {
	char c;
	int length;
	int status;

	/*
	 *  HANDLE:  drag&drop source <pathName> ?options...?
	 */
	c = argv[3][0];
	length = strlen(argv[3]);

	if (c == '-') {
	    if (argc == 3) {
		status = Tk_ConfigureInfo(interp, tokenPtr->tkwin, configSpecs,
		    (char *)srcPtr, (char *)NULL, 0);
	    } else if (argc == 4) {
		status = Tk_ConfigureInfo(interp, tokenPtr->tkwin, configSpecs,
		    (char *)srcPtr, argv[3], 0);
	    } else {
		status = ConfigureSource(interp, srcPtr, argc - 3, argv + 3,
		    TK_CONFIG_ARGV_ONLY);
	    }
	    if (status != TCL_OK) {
		return TCL_ERROR;
	    }
	} else if ((c == 'h') && strncmp(argv[3], "handler", length) == 0) {
	    return HandlerOpOp(srcPtr, interp, argc, argv);
	} else {
	    Tcl_AppendResult(interp, "bad operation \"", argv[3],
		"\": must be \"handler\" or a configuration option",
		(char *)NULL);
	    return TCL_ERROR;
	}
    }
    if (isNew) {
	/*
	 *  Create the window for the drag&drop token...
	 */
	if (CreateToken(interp, srcPtr) != TCL_OK) {
	    DestroySource(srcPtr);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *  HANDLE:  drag&drop target ?<pathName>? ?handling info...?
 */
static int
TargetOp(interp, argc, argv)
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    SubstDescriptors subst[2];
    Tk_Window tkwin;
    Blt_HashEntry *hPtr;
    Target *targetPtr;
    int isNew;

    if (argc == 2) {
	Blt_HashSearch cursor;

	for (hPtr = Blt_FirstHashEntry(&targetTable, &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    tkwin = (Tk_Window)Blt_GetHashKey(&targetTable, hPtr);
	    Tcl_AppendElement(interp, Tk_PathName(tkwin));
	}
	return TCL_OK;
    }
    tkwin = Tk_NameToWindow(interp, argv[2], Tk_MainWindow(interp));
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    targetPtr = FindTarget(tkwin);
    if (targetPtr == NULL) {
	targetPtr = CreateTarget(interp, tkwin);
    }
    if (targetPtr == NULL) {
	return TCL_ERROR;
    }

    if ((argc >= 4) && (strcmp(argv[3], "handler") == 0)) {
	/*
	 *  HANDLE:  drag&drop target <pathName> handler
	 *           drag&drop target <pathName> handler ?<data> <cmd> <arg>...?
	 */
	if (argc == 4) {
	    Blt_HashSearch cursor;

	    for (hPtr = Blt_FirstHashEntry(&(targetPtr->handlerTable), &cursor);
		hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
		Tcl_AppendElement(interp,
		    Blt_GetHashKey(&(targetPtr->handlerTable), hPtr));
	    }
	    return TCL_OK;
	} else if (argc >= 6) {
	    char *cmd;

	    /*
	     *  Process handler definition
	     */
	    hPtr = Blt_CreateHashEntry(&(targetPtr->handlerTable), argv[4],
		&isNew);
	    cmd = Tcl_Concat(argc - 5, argv + 5);
	    if (hPtr != NULL) {
		char *oldCmd;

		oldCmd = (char *)Blt_GetHashValue(hPtr);
		if (oldCmd != NULL) {
		    Blt_Free(oldCmd);
		}
	    }
	    Blt_SetHashValue(hPtr, cmd);
	    /*
	     * Update the target property on the window.
	     */
	    AddTargetProperty(interp, targetPtr);
	    return TCL_OK;
	}
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " ", argv[1], " ", argv[2], " ", argv[3],
	    " data command ?arg arg...?", (char *)NULL);
	return TCL_ERROR;
    } else if ((argc >= 4) && (strcmp(argv[3], "handle") == 0)) {
	/*
	 *  HANDLE:  drag&drop target <pathName> handle <data> ?<value>?
	 */
	Tcl_DString dString;
	int result;
	char *cmd;

	if (argc < 5 || argc > 6) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " ", argv[1], " ", argv[2], " handle data ?value?",
		(char *)NULL);
	    return TCL_ERROR;
	}
	hPtr = Blt_FindHashEntry(&(targetPtr->handlerTable), argv[4]);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "target can't handle datatype: ",
		argv[4], (char *)NULL);
	    return TCL_ERROR;	/* no handler found */
	}
	cmd = (char *)Blt_GetHashValue(hPtr);
	if (cmd != NULL) {
	    subst[0].letter = 'W';
	    subst[0].value = Tk_PathName(targetPtr->tkwin);
	    subst[1].letter = 'v';
	    if (argc > 5) {
		subst[1].value = argv[5];
	    } else {
		subst[1].value = "";
	    }
	    Tcl_DStringInit(&dString);
	    result = Tcl_Eval(interp, ExpandPercents(cmd, subst, 2, &dString));
	    Tcl_DStringFree(&dString);
	    return result;
	}
	return TCL_OK;
    }
    Tcl_AppendResult(interp, "usage: ", argv[0], " target ", argv[2],
	" handler ?data command arg arg...?\n   or: ",
	argv[0], " target ", argv[2], " handle <data>",
	(char *)NULL);
    return TCL_ERROR;
}

/*
 * ------------------------------------------------------------------------
 *
 *  DragDropCmd --
 *
 *  Invoked by TCL whenever the user issues a drag&drop command.
 *  Handles the following syntax:
 *
 *    drag&drop source
 *    drag&drop source <pathName> ?options...?
 *    drag&drop source <pathName> handler ?<dataType>? ?<cmd> <arg>...?
 *
 *    drag&drop target
 *    drag&drop target <pathName> handler ?<dataType> <cmd> <arg>...?
 *    drag&drop target <pathName> handle <dataType> ?<value>?
 *
 *    drag&drop token <pathName>
 *    drag&drop drag <pathName> <x> <y>
 *    drag&drop drop <pathName> <x> <y>
 *
 *    drag&drop errors ?<proc>?
 *    drag&drop active
 *    drag&drop location ?<x> <y>?
 *
 * ------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DragDropCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter */
    int argc;			/* # of arguments */
    char **argv;		/* Argument strings */
{
    int length;
    char c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " oper ?args?\"", (char *)NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);

    if ((c == 's') && strncmp(argv[1], "source", length) == 0) {
	return SourceOp(interp, argc, argv);
    } else if ((c == 't') && (length >= 2) &&
	(strncmp(argv[1], "target", length) == 0)) {
	return TargetOp(interp, argc, argv);
    } else if ((c == 't') && (length >= 2) &&
	(strncmp(argv[1], "token", length) == 0)) {
	return TokenOp(interp, argc, argv);
    } else if ((c == 'd') && strncmp(argv[1], "drag", length) == 0) {
	return DragOp(interp, argc, argv);
    } else if ((c == 'd') && strncmp(argv[1], "drop", length) == 0) {
	return DropOp(interp, argc, argv);
    } else if ((c == 'e') && strncmp(argv[1], "errors", length) == 0) {
	return ErrorsOp(interp, argc, argv);
    } else if ((c == 'a') && strncmp(argv[1], "active", length) == 0) {
	return ActiveOp(interp, argc, argv);
    } else if ((c == 'l') && strncmp(argv[1], "location", length) == 0) {
	return LocationOp(interp, argc, argv);
    }
    /*
     *  Report improper command arguments
     */
    Tcl_AppendResult(interp, "bad operation \"", argv[1],
	"\": must be active, drag, drop, errors, location, ",
	"source, target or token",
	(char *)NULL);
    return TCL_ERROR;
}

/*
 * ------------------------------------------------------------------------
 *
 *  Blt_DragDropInit --
 *
 *	Adds the drag&drop command to the given interpreter.  Should
 *	be invoked to properly install the command whenever a new
 *	interpreter is created.
 *
 * ------------------------------------------------------------------------
 */
int
Blt_DragDropInit(interp)
    Tcl_Interp *interp;		/* interpreter to be updated */
{
    static Blt_CmdSpec cmdSpec =
    {
	"drag&drop", DragDropCmd,
    };

    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    if (!initialized) {
	Blt_InitHashTable(&sourceTable, BLT_ONE_WORD_KEYS);
	Blt_InitHashTable(&targetTable, BLT_ONE_WORD_KEYS);
	errorCmd = Blt_Strdup(DEF_ERROR_PROC);
	nActive = 0;
	locX = locY = 0;
	initialized = TRUE;
#ifndef WIN32
	dndAtom = XInternAtom(Tk_Display(Tk_MainWindow(interp)), propName, 
		False);
#endif
    }
    return TCL_OK;
}
#endif /* NO_DRAGDROP */
