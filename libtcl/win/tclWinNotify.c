/* 
 * tclWinNotify.c --
 *
 *	This file contains Windows-specific procedures for the notifier,
 *	which is the lowest-level part of the Tcl event loop.  This file
 *	works together with ../generic/tclNotify.c.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinNotify.c 1.17 97/05/23 10:48:44
 */

#include "tclInt.h"
#include "tclPort.h"
#include <winsock.h>

/*
 * The follwing static indicates whether this module has been initialized.
 */

static int initialized = 0;

#define INTERVAL_TIMER 1		/* Handle of interval timer. */

/*
 * The following static structure contains the state information for the
 * Windows implementation of the Tcl notifier.
 */

static struct {
    HWND hwnd;			/* Messaging window. */
    int timeout;		/* Current timeout value. */
    int timerActive;		/* 1 if interval timer is running. */
} notifier;

/*
 * Static routines defined in this file.
 */

static void		InitNotifier(void);
static void		NotifierExitHandler(ClientData clientData);
static LRESULT CALLBACK	NotifierProc(HWND hwnd, UINT message,
			    WPARAM wParam, LPARAM lParam);
static void		UpdateTimer(int timeout);

/*
 *----------------------------------------------------------------------
 *
 * InitNotifier --
 *
 *	Initializes the notifier window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates a new notifier window and window class.
 *
 *----------------------------------------------------------------------
 */

static void
InitNotifier(void)
{
    WNDCLASS class;

    initialized = 1;
    notifier.timerActive = 0;
    class.style = 0;
    class.cbClsExtra = 0;
    class.cbWndExtra = 0;
    class.hInstance = TclWinGetTclInstance();
    class.hbrBackground = NULL;
    class.lpszMenuName = NULL;
    class.lpszClassName = "TclNotifier";
    class.lpfnWndProc = NotifierProc;
    class.hIcon = NULL;
    class.hCursor = NULL;

    if (!RegisterClass(&class)) {
	panic("Unable to register TclNotifier window class");
    }
    notifier.hwnd = CreateWindow("TclNotifier", "TclNotifier", WS_TILED,
	    0, 0, 0, 0, NULL, NULL, TclWinGetTclInstance(), NULL);
    Tcl_CreateExitHandler(NotifierExitHandler, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * NotifierExitHandler --
 *
 *	This function is called to cleanup the notifier state before
 *	Tcl is unloaded.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys the notifier window.
 *
 *----------------------------------------------------------------------
 */

static void
NotifierExitHandler(
    ClientData clientData)	/* Old window proc */
{
    initialized = 0;
    if (notifier.hwnd) {
	KillTimer(notifier.hwnd, INTERVAL_TIMER);
	DestroyWindow(notifier.hwnd);
	UnregisterClass("TclNotifier", TclWinGetTclInstance());
	notifier.hwnd = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateTimer --
 *
 *	This function starts or stops the notifier interval timer.
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
UpdateTimer(
    int timeout)		/* ms timeout, 0 means cancel timer */
{
    notifier.timeout = timeout;
    if (timeout != 0) {
	notifier.timerActive = 1;
	SetTimer(notifier.hwnd, INTERVAL_TIMER,
		    (unsigned long) notifier.timeout, NULL);
    } else {
	notifier.timerActive = 0;
	KillTimer(notifier.hwnd, INTERVAL_TIMER);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetTimer --
 *
 *	This procedure sets the current notifier timer value.  The
 *	notifier will ensure that Tcl_ServiceAll() is called after
 *	the specified interval, even if no events have occurred.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Replaces any previous timer.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetTimer(
    Tcl_Time *timePtr)		/* Maximum block time, or NULL. */
{
    UINT timeout;

    if (!initialized) {
	InitNotifier();
    }

    if (!timePtr) {
	timeout = 0;
    } else {
	/*
	 * Make sure we pass a non-zero value into the timeout argument.
	 * Windows seems to get confused by zero length timers.
	 */
	timeout = timePtr->sec * 1000 + timePtr->usec / 1000;
	if (timeout == 0) {
	    timeout = 1;
	}
    }
    UpdateTimer(timeout);
}

/*
 *----------------------------------------------------------------------
 *
 * NotifierProc --
 *
 *	This procedure is invoked by Windows to process the timer
 *	message whenever we are using an external dispatch loop.
 *
 * Results:
 *	A standard windows result.
 *
 * Side effects:
 *	Services any pending events.
 *
 *----------------------------------------------------------------------
 */

static LRESULT CALLBACK
NotifierProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{

    if (message != WM_TIMER) {
	return DefWindowProc(hwnd, message, wParam, lParam);
    }
	
    /*
     * Process all of the runnable events.
     */

    Tcl_ServiceAll();
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitForEvent --
 *
 *	This function is called by Tcl_DoOneEvent to wait for new
 *	events on the message queue.  If the block time is 0, then
 *	Tcl_WaitForEvent just polls the event queue without blocking.
 *
 * Results:
 *	Returns -1 if a WM_QUIT message is detected, returns 1 if
 *	a message was dispatched, otherwise returns 0.
 *
 * Side effects:
 *	Dispatches a message to a window procedure, which could do
 *	anything.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WaitForEvent(
    Tcl_Time *timePtr)		/* Maximum block time, or NULL. */
{
    MSG msg;
    int timeout;

    if (!initialized) {
	InitNotifier();
    }

    /*
     * Only use the interval timer for non-zero timeouts.  This avoids
     * generating useless messages when we really just want to poll.
     */

    if (timePtr) {
	timeout = timePtr->sec * 1000 + timePtr->usec / 1000;
    } else {
	timeout = 0;
    }
    UpdateTimer(timeout);
	
    if (!timePtr || (timeout != 0)
	    || PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
	if (!GetMessage(&msg, NULL, 0, 0)) {

	    /*
	     * The application is exiting, so repost the quit message
	     * and start unwinding.
	     */

	    PostQuitMessage(msg.wParam);
	    return -1;
	}

	/*
	 * Handle timer expiration as a special case so we don't
	 * claim to be doing work when we aren't.
	 */

	if (msg.message == WM_TIMER && msg.hwnd == notifier.hwnd) {
	    return 0;
	}

	TranslateMessage(&msg);
	DispatchMessage(&msg);
	return 1;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Sleep --
 *
 *	Delay execution for the specified number of milliseconds.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Time passes.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Sleep(ms)
    int ms;			/* Number of milliseconds to sleep. */
{
    Sleep(ms);
}
