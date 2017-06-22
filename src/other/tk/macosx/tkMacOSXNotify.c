/*
 * tkMacOSXNotify.c --
 *
 *	This file contains the implementation of a tcl event source
 *	for the AppKit event loop.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2005-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright 2015 Marc Culler. 
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXEvent.h"
#include <tclInt.h>
#include <pthread.h>
#import <objc/objc-auto.h>

/* This is not used for anything at the moment. */
typedef struct ThreadSpecificData {
    int initialized;
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;

#define TSD_INIT() ThreadSpecificData *tsdPtr = Tcl_GetThreadData(&dataKey, \
	    sizeof(ThreadSpecificData))

static void TkMacOSXNotifyExitHandler(ClientData clientData);
static void TkMacOSXEventsSetupProc(ClientData clientData, int flags);
static void TkMacOSXEventsCheckProc(ClientData clientData, int flags);

#pragma mark TKApplication(TKNotify)

@interface NSApplication(TKNotify)
/* We need to declare this hidden method. */
- (void)_modalSession:(NSModalSession)session sendEvent:(NSEvent *)event;
@end

@implementation NSWindow(TKNotify)
- (id)tkDisplayIfNeeded {
    if (![self isAutodisplay]) {
	[self displayIfNeeded];
    }
    return nil;
}
@end

@implementation TKApplication(TKNotify)
/* Display all windows each time an event is removed from the queue.*/
- (NSEvent *)nextEventMatchingMask:(NSUInteger)mask
	untilDate:(NSDate *)expiration inMode:(NSString *)mode
	dequeue:(BOOL)deqFlag {
    NSEvent *event = [super nextEventMatchingMask:mask
					untilDate:expiration
					   inMode:mode
					  dequeue:deqFlag];
    /* Retain this event for later use. Must be released.*/
    [event retain];
    [NSApp makeWindowsPerform:@selector(tkDisplayIfNeeded) inOrder:NO];
    return event;
}

 /*
 * Call super then check the pasteboard.
  */
- (void)sendEvent:(NSEvent *)theEvent {
    [super sendEvent:theEvent];
    [NSApp tkCheckPasteboard];
}
@end

#pragma mark -

/*
 *----------------------------------------------------------------------
 *
 * GetRunLoopMode --
 *
 * Results:
 *	RunLoop mode that should be passed to -nextEventMatchingMask:
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static NSString *
GetRunLoopMode(NSModalSession modalSession)
{
    NSString *runLoopMode = nil;

    if (modalSession) {
	runLoopMode = NSModalPanelRunLoopMode;
    } else if (TkMacOSXGetCapture()) {
	runLoopMode = NSEventTrackingRunLoopMode;
    }
    if (!runLoopMode) {
	runLoopMode = [[NSRunLoop currentRunLoop] currentMode];
    }
    if (!runLoopMode) {
	runLoopMode = NSDefaultRunLoopMode;
    }
    return runLoopMode;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MacOSXSetupTkNotifier --
 *
 *	This procedure is called during Tk initialization to create
 *	the event source for TkAqua events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new event source is created.
 *
 *----------------------------------------------------------------------
 */

void
Tk_MacOSXSetupTkNotifier(void)
{
    TSD_INIT();
    if (!tsdPtr->initialized) {
	tsdPtr->initialized = 1;

	/*
	 * Install TkAqua event source in main event loop thread.
	 */

	if (CFRunLoopGetMain() == CFRunLoopGetCurrent()) {
	    if (!pthread_main_np()) {
		/*
		 * Panic if main runloop is not on the main application thread.
		 */

		Tcl_Panic("Tk_MacOSXSetupTkNotifier: %s",
		    "first [load] of TkAqua has to occur in the main thread!");
	    }
	    Tcl_CreateEventSource(TkMacOSXEventsSetupProc,
				  TkMacOSXEventsCheckProc,
				  GetMainEventQueue());
	    TkCreateExitHandler(TkMacOSXNotifyExitHandler, NULL);
	    Tcl_SetServiceMode(TCL_SERVICE_ALL);
	    TclMacOSXNotifierAddRunLoopMode(NSEventTrackingRunLoopMode);
	    TclMacOSXNotifierAddRunLoopMode(NSModalPanelRunLoopMode);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXNotifyExitHandler --
 *
 *	This function is called during finalization to clean up the
 *	TkMacOSXNotify module.
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
TkMacOSXNotifyExitHandler(
    ClientData clientData)	/* Not used. */
{
    TSD_INIT();
    Tcl_DeleteEventSource(TkMacOSXEventsSetupProc,
			  TkMacOSXEventsCheckProc,
			  GetMainEventQueue());
    tsdPtr->initialized = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXEventsSetupProc --
 *
 *	This procedure implements the setup part of the MacOSX event
 *	source. It is invoked by Tcl_DoOneEvent before calling
 *      TkMacOSXEventsProc to process all queued NSEvents.  In our
 *      case, all we need to do is to set the Tcl MaxBlockTime to
 *      0 before starting the loop to process all queued NSEvents.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *
 *	If NSEvents are queued, then the maximum block time will be set
 *	to 0 to ensure that control returns immediately to Tcl.
 *
 *----------------------------------------------------------------------
 */

static void
TkMacOSXEventsSetupProc(
    ClientData clientData,
    int flags)
{
    NSString *runloopMode = [[NSRunLoop currentRunLoop] currentMode];
    /* runloopMode will be nil if we are in the Tcl event loop. */
    if (flags & TCL_WINDOW_EVENTS && !runloopMode) {
	static Tcl_Time zeroBlockTime = { 0, 0 };
	/* Call this with dequeue=NO -- just checking if the queue is empty. */
	NSEvent *currentEvent = [NSApp nextEventMatchingMask:NSAnyEventMask
				       untilDate:[NSDate distantPast]
				       inMode:GetRunLoopMode(TkMacOSXGetModalSession())
				       dequeue:NO];
	if (currentEvent) {
	    if (currentEvent.type > 0) {
		Tcl_SetMaxBlockTime(&zeroBlockTime);
	    }
	    [currentEvent release];
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXEventsCheckProc --
 *
 *	This procedure loops through all NSEvents waiting in the
 *      TKApplication event queue, generating X events from them.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	NSevents are used to generate X events, which are added to the
 *      Tcl event queue.
 *
 *----------------------------------------------------------------------
 */
static void
TkMacOSXEventsCheckProc(
    ClientData clientData,
    int flags)
{
    NSString *runloopMode = [[NSRunLoop currentRunLoop] currentMode];
    /* runloopMode will be nil if we are in the Tcl event loop. */
    if (flags & TCL_WINDOW_EVENTS && !runloopMode) {
	NSEvent *currentEvent = nil;
	NSEvent *testEvent = nil;
	NSModalSession modalSession;

	do {
	    [NSApp _resetAutoreleasePool];
	    modalSession = TkMacOSXGetModalSession();
	    testEvent = [NSApp nextEventMatchingMask:NSAnyEventMask
					      untilDate:[NSDate distantPast]
						 inMode:GetRunLoopMode(modalSession)
						dequeue:NO];
	    /* We must not steal any events during LiveResize. */
	    if (testEvent && [[testEvent window] inLiveResize]) {
		break;
	    }

	    currentEvent = [NSApp nextEventMatchingMask:NSAnyEventMask
					      untilDate:[NSDate distantPast]
						 inMode:GetRunLoopMode(modalSession)
						dequeue:YES];
	    if (currentEvent) {
		/* Generate Xevents. */
		int oldServiceMode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
		NSEvent *processedEvent = [NSApp tkProcessEvent:currentEvent];
		Tcl_SetServiceMode(oldServiceMode);
		if (processedEvent) { /* Should always be non-NULL. */
#ifdef TK_MAC_DEBUG_EVENTS
		    TKLog(@"   event: %@", currentEvent);
#endif
		    if (modalSession) {
			[NSApp _modalSession:modalSession sendEvent:currentEvent];
		    } else {
			[NSApp sendEvent:currentEvent];
		    }
		}
		[currentEvent release];
	    } else {
		break;
	    }
	} while (1);
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
