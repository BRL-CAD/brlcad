/*
 * tkMacOSXEvent.c --
 *
 *	This file contains the basic Mac OS X Event handling routines.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2005-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXEvent.h"
#include "tkMacOSXDebug.h"

#pragma mark TKApplication(TKEvent)

enum {
    NSWindowWillMoveEventType = 20
};

@implementation TKApplication(TKEvent)
/* TODO: replace by +[addLocalMonitorForEventsMatchingMask ? */
- (NSEvent *)tkProcessEvent:(NSEvent *)theEvent {
#ifdef TK_MAC_DEBUG_EVENTS
    TKLog(@"-[%@(%p) %s] %@", [self class], self, _cmd, theEvent);
#endif
    NSEvent	    *processedEvent = theEvent;
    NSEventType	    type = [theEvent type];
    NSInteger	    subtype;
    NSUInteger	    flags;

    switch ((NSInteger)type) {
    case NSAppKitDefined:
        subtype = [theEvent subtype];

	switch (subtype) {
	case NSApplicationActivatedEventType:
	    break;
	case NSApplicationDeactivatedEventType:
	    break;
	case NSWindowExposedEventType:
	case NSScreenChangedEventType:
	    break;
	case NSWindowMovedEventType:
	    break;
        case NSWindowWillMoveEventType:
            break;

        default:
            break;
	}
	break;
    case NSKeyUp:
    case NSKeyDown:
    case NSFlagsChanged:
	flags = [theEvent modifierFlags];
	processedEvent = [self tkProcessKeyEvent:theEvent];
	break;
    case NSLeftMouseDown:
    case NSLeftMouseUp:
    case NSRightMouseDown:
    case NSRightMouseUp:
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSMouseMoved:
    case NSMouseEntered:
    case NSMouseExited:
    case NSScrollWheel:
    case NSOtherMouseDown:
    case NSOtherMouseUp:
    case NSOtherMouseDragged:
    case NSTabletPoint:
    case NSTabletProximity:
	processedEvent = [self tkProcessMouseEvent:theEvent];
	break;
#if 0
    case NSSystemDefined:
        subtype = [theEvent subtype];
	break;
    case NSApplicationDefined: {
	id win;
	win = [theEvent window];
	break;
	}
    case NSCursorUpdate:
        break;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
    case NSEventTypeGesture:
    case NSEventTypeMagnify:
    case NSEventTypeRotate:
    case NSEventTypeSwipe:
    case NSEventTypeBeginGesture:
    case NSEventTypeEndGesture:
        break;
#endif
#endif

    default:
	break;
    }
    return processedEvent;
}
@end

#pragma mark -

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXFlushWindows --
 *
 *	This routine flushes all the windows of the application. It is
 *	called by XSync().
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Flushes all Carbon windows
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE void
TkMacOSXFlushWindows(void)
{
    NSInteger windowCount;
    NSInteger *windowNumbers;

    NSCountWindows(&windowCount);
    if(windowCount) {
	windowNumbers = (NSInteger *) ckalloc(windowCount * sizeof(NSInteger));
	NSWindowList(windowCount, windowNumbers);
	for (NSInteger index = 0; index < windowCount; index++) {
	    NSWindow *w = [NSApp windowWithWindowNumber:windowNumbers[index]];
	    if (TkMacOSXGetXWindow(w)) {
		[w flushWindow];
	    }
	}
	ckfree((char*) windowNumbers);
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
