/* 
 * tkWinX.c --
 *
 *	This file contains Windows emulation procedures for X routines. 
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 * Copyright (c) 1994 Software Research Associates, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinX.c 1.51 97/09/02 13:06:57
 */

#include "tkInt.h"
#include "tkWinInt.h"

/*
 * Definitions of extern variables supplied by this file.
 */

int tkpIsWin32s = -1;

/*
 * Declarations of static variables used in this file.
 */

static HINSTANCE tkInstance = (HINSTANCE) NULL;
				/* Global application instance handle. */
static TkDisplay *winDisplay;	/* Display that represents Windows screen. */
static char winScreenName[] = ":0";
				/* Default name of windows display. */
static WNDCLASS childClass;	/* Window class for child windows. */
static childClassInitialized = 0; /* Registered child class? */

/*
 * Forward declarations of procedures used in this file.
 */

static void		GenerateXEvent _ANSI_ARGS_((HWND hwnd, UINT message,
			    WPARAM wParam, LPARAM lParam));
static unsigned int	GetState _ANSI_ARGS_((UINT message, WPARAM wParam,
			    LPARAM lParam));
static void 		GetTranslatedKey _ANSI_ARGS_((XKeyEvent *xkey));

/*
 *----------------------------------------------------------------------
 *
 * TkGetServerInfo --
 *
 *	Given a window, this procedure returns information about
 *	the window server for that window.  This procedure provides
 *	the guts of the "winfo server" command.
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
TkGetServerInfo(interp, tkwin)
    Tcl_Interp *interp;		/* The server information is returned in
				 * this interpreter's result. */
    Tk_Window tkwin;		/* Token for window;  this selects a
				 * particular display and server. */
{
    char buffer[50];
    OSVERSIONINFO info;

    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&info);
    sprintf(buffer, "Windows %d.%d %d ", info.dwMajorVersion,
	    info.dwMinorVersion, info.dwBuildNumber);
    Tcl_AppendResult(interp, buffer,
	    (info.dwPlatformId == VER_PLATFORM_WIN32s) ? "Win32s" : "Win32",
	    (char *) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetHINSTANCE --
 *
 *	Retrieves the global instance handle used by the Tk library.
 *
 * Results:
 *	Returns the global instance handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

HINSTANCE
Tk_GetHINSTANCE()
{
    return tkInstance;
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinXInit --
 *
 *	Initialize Xlib emulation layer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets up various data structures.
 *
 *----------------------------------------------------------------------
 */

void
TkWinXInit(hInstance)
    HINSTANCE hInstance;
{
    OSVERSIONINFO info;

    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&info);
    tkpIsWin32s = (info.dwPlatformId == VER_PLATFORM_WIN32s);

    if (childClassInitialized != 0) {
	return;
    }
    childClassInitialized = 1;

    tkInstance = hInstance;

    childClass.style = CS_HREDRAW | CS_VREDRAW | CS_CLASSDC;
    childClass.cbClsExtra = 0;
    childClass.cbWndExtra = 0;
    childClass.hInstance = hInstance;
    childClass.hbrBackground = NULL;
    childClass.lpszMenuName = NULL;

    /*
     * Register the Child window class.
     */

    childClass.lpszClassName = TK_WIN_CHILD_CLASS_NAME;
    childClass.lpfnWndProc = TkWinChildProc;
    childClass.hIcon = NULL;
    childClass.hCursor = NULL;

    if (!RegisterClass(&childClass)) {
	panic("Unable to register TkChild class");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinXCleanup --
 *
 *	Removes the registered classes for Tk.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes window classes from the system.
 *
 *----------------------------------------------------------------------
 */

void
TkWinXCleanup(hInstance)
    HINSTANCE hInstance;
{
    /*
     * Clean up our own class.
     */
    
    if (childClassInitialized) {
        childClassInitialized = 0;
        UnregisterClass(TK_WIN_CHILD_CLASS_NAME, hInstance);
    }

    /*
     * And let the window manager clean up its own class(es).
     */
    
    TkWinWmCleanup(hInstance);
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetDefaultScreenName --
 *
 *	Returns the name of the screen that Tk should use during
 *	initialization.
 *
 * Results:
 *	Returns a statically allocated string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TkGetDefaultScreenName(interp, screenName)
    Tcl_Interp *interp;		/* Not used. */
    char *screenName;		/* If NULL, use default string. */
{
    if ((screenName == NULL) || (screenName[0] == '\0')) {
	screenName = winScreenName;
    }
    return screenName;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpOpenDisplay --
 *
 *	Create the Display structure and fill it with device
 *	specific information.
 *
 * Results:
 *	Returns a Display structure on success or NULL on failure.
 *
 * Side effects:
 *	Allocates a new Display structure.
 *
 *----------------------------------------------------------------------
 */

TkDisplay *
TkpOpenDisplay(display_name)
    char *display_name;
{
    Screen *screen;
    HDC dc;
    TkWinDrawable *twdPtr;
    Display *display;

    if (winDisplay != NULL) {
	if (strcmp(winDisplay->display->display_name, display_name) == 0) {
	    return winDisplay;
	} else {
	    return NULL;
	}
    }

    display = (Display *) ckalloc(sizeof(Display));
    display->display_name = (char *) ckalloc(strlen(display_name)+1);
    strcpy(display->display_name, display_name);

    display->cursor_font = 1;
    display->nscreens = 1;
    display->request = 1;
    display->qlen = 0;

    screen = (Screen *) ckalloc(sizeof(Screen));
    screen->display = display;

    dc = GetDC(NULL);
    screen->width = GetDeviceCaps(dc, HORZRES);
    screen->height = GetDeviceCaps(dc, VERTRES);
    screen->mwidth = MulDiv(screen->width, 254,
	    GetDeviceCaps(dc, LOGPIXELSX) * 10);
    screen->mheight = MulDiv(screen->height, 254,
	    GetDeviceCaps(dc, LOGPIXELSY) * 10);
    
    /*
     * Set up the root window.
     */

    twdPtr = (TkWinDrawable*) ckalloc(sizeof(TkWinDrawable));
    if (twdPtr == NULL) {
	return None;
    }
    twdPtr->type = TWD_WINDOW;
    twdPtr->window.winPtr = NULL;
    twdPtr->window.handle = NULL;
    screen->root = (Window)twdPtr;
 
    /*
     * On windows, when creating a color bitmap, need two pieces of 
     * information: the number of color planes and the number of 
     * pixels per plane.  Need to remember both quantities so that
     * when constructing an HBITMAP for offscreen rendering, we can
     * specify the correct value for the number of planes.  Otherwise
     * the HBITMAP won't be compatible with the HWND and we'll just
     * get blank spots copied onto the screen.
     */

    screen->ext_data = (XExtData *) GetDeviceCaps(dc, PLANES);
    screen->root_depth = GetDeviceCaps(dc, BITSPIXEL) * (int) screen->ext_data;

    screen->root_visual = (Visual *) ckalloc(sizeof(Visual));
    screen->root_visual->visualid = 0;
    if (GetDeviceCaps(dc, RASTERCAPS) & RC_PALETTE) {
	screen->root_visual->map_entries = GetDeviceCaps(dc, SIZEPALETTE);
	screen->root_visual->class = PseudoColor;
	screen->root_visual->red_mask = 0x0;
	screen->root_visual->green_mask = 0x0;
	screen->root_visual->blue_mask = 0x0;
    } else {
	if (screen->root_depth == 4) {
	    screen->root_visual->class = StaticColor;
	    screen->root_visual->map_entries = 16;
	} else if (screen->root_depth == 8) {
	    screen->root_visual->class = StaticColor;
	    screen->root_visual->map_entries = 256;
	} else if (screen->root_depth == 12) {
	    screen->root_visual->class = TrueColor;
	    screen->root_visual->map_entries = 32;
	    screen->root_visual->red_mask = 0xf0;
	    screen->root_visual->green_mask = 0xf000;
	    screen->root_visual->blue_mask = 0xf00000;
	} else if (screen->root_depth == 16) {
	    screen->root_visual->class = TrueColor;
	    screen->root_visual->map_entries = 64;
	    screen->root_visual->red_mask = 0xf8;
	    screen->root_visual->green_mask = 0xfc00;
	    screen->root_visual->blue_mask = 0xf80000;
	} else if (screen->root_depth >= 24) {
	    screen->root_visual->class = TrueColor;
	    screen->root_visual->map_entries = 256;
	    screen->root_visual->red_mask = 0xff;
	    screen->root_visual->green_mask = 0xff00;
	    screen->root_visual->blue_mask = 0xff0000;
	}
    }
    screen->root_visual->bits_per_rgb = screen->root_depth;
    ReleaseDC(NULL, dc);

    /*
     * Note that these pixel values are not palette relative.
     */

    screen->white_pixel = RGB(255, 255, 255);
    screen->black_pixel = RGB(0, 0, 0);

    display->screens = screen;
    display->nscreens = 1;
    display->default_screen = 0;
    screen->cmap = XCreateColormap(display, None, screen->root_visual,
	    AllocNone);
    winDisplay = (TkDisplay *) ckalloc(sizeof(TkDisplay));
    winDisplay->display = display;
    return winDisplay;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpCloseDisplay --
 *
 *	Closes and deallocates a Display structure created with the
 *	TkpOpenDisplay function.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees up memory.
 *
 *----------------------------------------------------------------------
 */

void
TkpCloseDisplay(dispPtr)
    TkDisplay *dispPtr;
{
    Display *display = dispPtr->display;
    HWND hwnd;

    if (dispPtr != winDisplay) {
        panic("TkpCloseDisplay: tried to call TkpCloseDisplay on another display");
        return;
    }

    /*
     * Force the clipboard to be rendered if we are the clipboard owner.
     */
    
    if (dispPtr->clipWindow) {
	hwnd = Tk_GetHWND(Tk_WindowId(dispPtr->clipWindow));
	if (GetClipboardOwner() == hwnd) {
	    OpenClipboard(hwnd);
	    EmptyClipboard();
	    TkWinClipboardRender(dispPtr, CF_TEXT);
	    CloseClipboard();
	}
    }

    winDisplay = NULL;

    if (display->display_name != (char *) NULL) {
        ckfree(display->display_name);
    }
    if (display->screens != (Screen *) NULL) {
        if (display->screens->root_visual != NULL) {
            ckfree((char *) display->screens->root_visual);
        }
        if (display->screens->root != None) {
            ckfree((char *) display->screens->root);
        }
        if (display->screens->cmap != None) {
            XFreeColormap(display, display->screens->cmap);
        }
        ckfree((char *) display->screens);
    }
    ckfree((char *) display);
    ckfree((char *) dispPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * XBell --
 *
 *	Generate a beep.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Plays a sounds out the system speakers.
 *
 *----------------------------------------------------------------------
 */

void
XBell(display, percent)
    Display* display;
    int percent;
{
    MessageBeep(MB_OK);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinChildProc --
 *
 *	Callback from Windows whenever an event occurs on a child
 *	window.
 *
 * Results:
 *	Standard Windows return value.
 *
 * Side effects:
 *	May process events off the Tk event queue.
 *
 *----------------------------------------------------------------------
 */

LRESULT CALLBACK
TkWinChildProc(hwnd, message, wParam, lParam)
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
{
    LRESULT result;

    switch (message) {
	case WM_SETCURSOR:
	    /*
	     * Short circuit the WM_SETCURSOR message since we set
	     * the cursor elsewhere.
	     */

	    result = TRUE;
	    break;

	case WM_CREATE:
	case WM_ERASEBKGND:
	case WM_WINDOWPOSCHANGED:
	    result = 0;
	    break;

	case WM_PAINT:
	    GenerateXEvent(hwnd, message, wParam, lParam);
	    result = DefWindowProc(hwnd, message, wParam, lParam);
	    break;

        case TK_CLAIMFOCUS:
	case TK_GEOMETRYREQ:
	case TK_ATTACHWINDOW:
	case TK_DETACHWINDOW:
	    result =  TkWinEmbeddedEventProc(hwnd, message, wParam, lParam);
	    break;

	default:
	    if (!Tk_TranslateWinEvent(hwnd, message, wParam, lParam,
		    &result)) {
		result = DefWindowProc(hwnd, message, wParam, lParam);
	    }
	    break;
    }

    /*
     * Handle any newly queued events before returning control to Windows.
     */

    Tcl_ServiceAll();
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_TranslateWinEvent --
 *
 *	This function is called by widget window procedures to handle
 *	the translation from Win32 events to Tk events.
 *
 * Results:
 *	Returns 1 if the event was handled, else 0.
 *
 * Side effects:
 *	Depends on the event.
 *
 *----------------------------------------------------------------------
 */

int
Tk_TranslateWinEvent(hwnd, message, wParam, lParam, resultPtr)
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    LRESULT *resultPtr;
{
    *resultPtr = 0;
    switch (message) {
	case WM_RENDERFORMAT: {
	    TkWindow *winPtr = (TkWindow *) Tk_HWNDToWindow(hwnd);
	    if (winPtr) {
		TkWinClipboardRender(winPtr->dispPtr, wParam);
	    }
	    return 1;
	}

	case WM_COMMAND:
	case WM_NOTIFY:
	case WM_VSCROLL:
	case WM_HSCROLL: {
	    /*
	     * Reflect these messages back to the sender so that they
	     * can be handled by the window proc for the control.  Note
	     * that we need to be careful not to reflect a message that
	     * is targeted to this window, or we will loop.
	     */

	    HWND target = (message == WM_NOTIFY)
		? ((NMHDR*)lParam)->hwndFrom : (HWND) lParam;
	    if (target && target != hwnd) {
		*resultPtr = SendMessage(target, message, wParam, lParam);
		return 1;
	    }
	    break;
	}

	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
	    Tk_PointerEvent(hwnd, (short) LOWORD(lParam),
		    (short) HIWORD(lParam));
	    return 1;

	case WM_CLOSE:
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
	case WM_DESTROYCLIPBOARD:
	case WM_CHAR:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
 	    GenerateXEvent(hwnd, message, wParam, lParam);
	    return 1;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateXEvent --
 *
 *	This routine generates an X event from the corresponding
 * 	Windows event.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Queues one or more X events.
 *
 *----------------------------------------------------------------------
 */

static void
GenerateXEvent(hwnd, message, wParam, lParam)
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
{
    XEvent event;
    TkWindow *winPtr = (TkWindow *)Tk_HWNDToWindow(hwnd);

    if (!winPtr || winPtr->window == None) {
	return;
    }

    event.xany.serial = winPtr->display->request++;
    event.xany.send_event = False;
    event.xany.display = winPtr->display;
    event.xany.window = winPtr->window;

    switch (message) {
	case WM_PAINT: {
	    PAINTSTRUCT ps;

	    event.type = Expose;
	    BeginPaint(hwnd, &ps);
	    event.xexpose.x = ps.rcPaint.left;
	    event.xexpose.y = ps.rcPaint.top;
	    event.xexpose.width = ps.rcPaint.right - ps.rcPaint.left;
	    event.xexpose.height = ps.rcPaint.bottom - ps.rcPaint.top;
	    EndPaint(hwnd, &ps);
	    event.xexpose.count = 0;
	    break;
	}

	case WM_CLOSE:
	    event.type = ClientMessage;
	    event.xclient.message_type =
		Tk_InternAtom((Tk_Window) winPtr, "WM_PROTOCOLS");
	    event.xclient.format = 32;
	    event.xclient.data.l[0] =
		Tk_InternAtom((Tk_Window) winPtr, "WM_DELETE_WINDOW");
	    break;

	case WM_SETFOCUS:
	case WM_KILLFOCUS: {
	    TkWindow *otherWinPtr = (TkWindow *)Tk_HWNDToWindow((HWND) wParam);
	    
	    /*
	     * Compare toplevel windows to avoid reporting focus
	     * changes within the same toplevel.
	     */

	    while (!(winPtr->flags & TK_TOP_LEVEL)) {
		winPtr = winPtr->parentPtr;
		if (winPtr == NULL) {
		    return;
		}
	    }
	    while (otherWinPtr && !(otherWinPtr->flags & TK_TOP_LEVEL)) {
		otherWinPtr = otherWinPtr->parentPtr;
	    }
	    if (otherWinPtr == winPtr) {
		return;
	    }

	    event.xany.window = winPtr->window;
	    event.type = (message == WM_SETFOCUS) ? FocusIn : FocusOut;
	    event.xfocus.mode = NotifyNormal;
	    event.xfocus.detail = NotifyNonlinear;
	    break;
	}

	case WM_DESTROYCLIPBOARD:
	    event.type = SelectionClear;
	    event.xselectionclear.selection =
		Tk_InternAtom((Tk_Window)winPtr, "CLIPBOARD");
	    event.xselectionclear.time = TkpGetMS();
	    break;
	    
	case WM_CHAR:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP: {
	    unsigned int state = GetState(message, wParam, lParam);
	    Time time = TkpGetMS();
	    POINT clientPoint;
	    POINTS rootPoint;	/* Note: POINT and POINTS are different */
	    DWORD msgPos;

	    /*
	     * Compute the screen and window coordinates of the event.
	     */
	    
	    msgPos = GetMessagePos();
	    rootPoint = MAKEPOINTS(msgPos);
	    clientPoint.x = rootPoint.x;
	    clientPoint.y = rootPoint.y;
	    ScreenToClient(hwnd, &clientPoint);

	    /*
	     * Set up the common event fields.
	     */

	    event.xbutton.root = RootWindow(winPtr->display,
		    winPtr->screenNum);
	    event.xbutton.subwindow = None;
	    event.xbutton.x = clientPoint.x;
	    event.xbutton.y = clientPoint.y;
	    event.xbutton.x_root = rootPoint.x;
	    event.xbutton.y_root = rootPoint.y;
	    event.xbutton.state = state;
	    event.xbutton.time = time;
	    event.xbutton.same_screen = True;

	    /*
	     * Now set up event specific fields.
	     */

	    switch (message) {
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		    /*
		     * Check for translated characters in the event queue.
		     * Setting xany.send_event to -1 indicates to the
		     * Windows implementation of XLookupString that this
		     * event was generated by windows and that the Windows
		     * extension xkey.trans_chars is filled with the
		     * characters that came from the TranslateMessage
		     * call.  If it is not -1, xkey.keycode is the
		     * virtual key being sent programmatically by generic
		     * code.
		     */

		    event.type = KeyPress;
		    event.xany.send_event = -1;
		    event.xkey.keycode = wParam;
		    GetTranslatedKey(&event.xkey);
		    break;

		case WM_SYSKEYUP:
		case WM_KEYUP:
		    /*
		     * We don't check for translated characters on keyup
		     * because Tk won't know what to do with them.  Instead, we
		     * wait for the WM_CHAR messages which will follow.
		     */
		    event.type = KeyRelease;
		    event.xkey.keycode = wParam;
		    event.xkey.nchars = 0;
		    break;

		case WM_CHAR:
		    /*
		     * Synthesize both a KeyPress and a KeyRelease.
		     */

		    event.type = KeyPress;
		    event.xany.send_event = -1;
		    event.xkey.keycode = 0;
		    event.xkey.nchars = 1;
		    event.xkey.trans_chars[0] = (char) wParam;
		    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
		    event.type = KeyRelease;
		    break;
	    }
	    break;
	}

	default:
	    return;
    }
    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
}

/*
 *----------------------------------------------------------------------
 *
 * GetState --
 *
 *	This function constructs a state mask for the mouse buttons 
 *	and modifier keys as they were before the event occured.
 *
 * Results:
 *	Returns a composite value of all the modifier and button state
 *	flags that were set at the time the event occurred.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static unsigned int
GetState(message, wParam, lParam)
    UINT message;		/* Win32 message type */
    WPARAM wParam;		/* wParam of message, used if key message */
    LPARAM lParam;		/* lParam of message, used if key message */
{
    int mask;
    int prevState;		/* 1 if key was previously down */
    unsigned int state = TkWinGetModifierState();

    /*
     * If the event is a key press or release, we check for modifier
     * keys so we can report the state of the world before the event.
     */

    if (message == WM_SYSKEYDOWN || message == WM_KEYDOWN
	    || message == WM_SYSKEYUP || message == WM_KEYUP) {
	mask = 0;
	prevState = HIWORD(lParam) & KF_REPEAT;
	switch(wParam) {
	    case VK_SHIFT:
		mask = ShiftMask;
		break;
	    case VK_CONTROL:
		mask = ControlMask;
		break;
	    case VK_MENU:
		mask = Mod2Mask;
		break;
	    case VK_CAPITAL:
		if (message == WM_SYSKEYDOWN || message == WM_KEYDOWN) {
		    mask = LockMask;
		    prevState = ((state & mask) ^ prevState) ? 0 : 1;
		}
		break;
	    case VK_NUMLOCK:
		if (message == WM_SYSKEYDOWN || message == WM_KEYDOWN) {
		    mask = Mod1Mask;
		    prevState = ((state & mask) ^ prevState) ? 0 : 1;
		}
		break;
	    case VK_SCROLL:
		if (message == WM_SYSKEYDOWN || message == WM_KEYDOWN) {
		    mask = Mod3Mask;
		    prevState = ((state & mask) ^ prevState) ? 0 : 1;
		}
		break;
	}
	if (prevState) {
	    state |= mask;
	} else {
	    state &= ~mask;
	}
    }
    return state;
}

/*
 *----------------------------------------------------------------------
 *
 * GetTranslatedKey --
 *
 *	Retrieves WM_CHAR messages that are placed on the system queue
 *	by the TranslateMessage system call and places them in the
 *	given KeyPress event.
 *
 * Results:
 *	Sets the trans_chars and nchars member of the key event.
 *
 * Side effects:
 *	Removes any WM_CHAR messages waiting on the top of the system
 *	event queue.
 *
 *----------------------------------------------------------------------
 */

static void
GetTranslatedKey(xkey)
    XKeyEvent *xkey;
{
    MSG msg;
    
    xkey->nchars = 0;

    while (xkey->nchars < XMaxTransChars
	    && PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
	if (msg.message == WM_CHAR) {
	    xkey->trans_chars[xkey->nchars] = (char) msg.wParam;
	    xkey->nchars++;
	    GetMessage(&msg, NULL, 0, 0);
	    if ((msg.message == WM_CHAR) && (msg.lParam & 0x20000000)) {
		xkey->state = 0;
	    }
	} else {
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_FreeXId --
 *
 *	This inteface is not needed under Windows.
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
Tk_FreeXId(display, xid)
    Display *display;
    XID xid;
{
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinResendEvent --
 *
 *	This function converts an X event into a Windows event and
 *	invokes the specified windo procedure.
 *
 * Results:
 *	A standard Windows result.
 *
 * Side effects:
 *	Invokes the window procedure
 *
 *----------------------------------------------------------------------
 */

LRESULT
TkWinResendEvent(wndproc, hwnd, eventPtr)
    WNDPROC wndproc;
    HWND hwnd;
    XEvent *eventPtr;
{
    UINT msg;
    WPARAM wparam;
    LPARAM lparam;

    if (eventPtr->type == ButtonPress) {
	switch (eventPtr->xbutton.button) {
	    case Button1:
		msg = WM_LBUTTONDOWN;
		wparam = MK_LBUTTON;
		break;
	    case Button2:
		msg = WM_MBUTTONDOWN;
		wparam = MK_MBUTTON;
		break;
	    case Button3:
		msg = WM_RBUTTONDOWN;
		wparam = MK_RBUTTON;
		break;
	    default:
		return 0;
	}
	if (eventPtr->xbutton.state & Button1Mask) {
	    wparam |= MK_LBUTTON;
	}
	if (eventPtr->xbutton.state & Button2Mask) {
	    wparam |= MK_MBUTTON;
	}
	if (eventPtr->xbutton.state & Button3Mask) {
	    wparam |= MK_RBUTTON;
	}
	if (eventPtr->xbutton.state & ShiftMask) {
	    wparam |= MK_SHIFT;
	}
	if (eventPtr->xbutton.state & ControlMask) {
	    wparam |= MK_CONTROL;
	}
	lparam = MAKELPARAM((short) eventPtr->xbutton.x,
		(short) eventPtr->xbutton.y);
    } else {
	return 0;
    }
    return CallWindowProc(wndproc, hwnd, msg, wparam, lparam);
}

/*
 *----------------------------------------------------------------------
 *
 * TkpGetMS --
 *
 *	Return a relative time in milliseconds.  It doesn't matter
 *	when the epoch was.
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
TkpGetMS()
{
    return GetCurrentTime();
}
