/*
 * tkMacOSXSubwindows.c --
 *
 *	Implements subwindows for the macintosh version of Tk.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkMacOSXDebug.h"
#include "tkMacOSXWm.h"

/*
#ifdef TK_MAC_DEBUG
#define TK_MAC_DEBUG_CLIP_REGIONS
#endif
*/

/*
 * Prototypes for functions used only in this file.
 */

static void		MoveResizeWindow(MacDrawable *macWin);
static void		GenerateConfigureNotify(TkWindow *winPtr,
			    int includeWin);
static void		UpdateOffsets(TkWindow *winPtr, int deltaX,
			    int deltaY);
static void		NotifyVisibility(TkWindow *winPtr, XEvent *eventPtr);


/*
 *----------------------------------------------------------------------
 *
 * XDestroyWindow --
 *
 *	Dealocates the given X Window.
 *
 * Results:
 *	The window id is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
XDestroyWindow(
    Display *display,		/* Display. */
    Window window)		/* Window. */
{
    MacDrawable *macWin = (MacDrawable *) window;

    /*
     * Remove any dangling pointers that may exist if the window we are
     * deleting is being tracked by the grab code.
     */

    TkPointerDeadWindow(macWin->winPtr);
    TkMacOSXSelDeadWindow(macWin->winPtr);
    macWin->toplevel->referenceCount--;

    if (!Tk_IsTopLevel(macWin->winPtr)) {
	TkMacOSXInvalidateWindow(macWin, TK_PARENT_WINDOW);
	if (macWin->winPtr->parentPtr != NULL) {
	    TkMacOSXInvalClipRgns((Tk_Window) macWin->winPtr->parentPtr);
	}
	if (macWin->visRgn) {
	    CFRelease(macWin->visRgn);
	}
	if (macWin->aboveVisRgn) {
	    CFRelease(macWin->aboveVisRgn);
	}
	if (macWin->drawRgn) {
	    CFRelease(macWin->drawRgn);
	}

	if (macWin->toplevel->referenceCount == 0) {
	    ckfree((char *) macWin->toplevel);
	}
	ckfree((char *) macWin);
	return;
    }
    if (macWin->visRgn) {
	CFRelease(macWin->visRgn);
    }
    if (macWin->aboveVisRgn) {
	CFRelease(macWin->aboveVisRgn);
    }
    if (macWin->drawRgn) {
	CFRelease(macWin->drawRgn);
    }
    macWin->view = nil;

    /*
     * Delay deletion of a toplevel data structure untill all children have
     * been deleted.
     */

    if (macWin->toplevel->referenceCount == 0) {
	ckfree((char *) macWin->toplevel);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XMapWindow --
 *
 *	Map the given X Window to the screen. See X window documentation for
 *	more details.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The subwindow or toplevel may appear on the screen.
 *
 *----------------------------------------------------------------------
 */

void
XMapWindow(
    Display *display,		/* Display. */
    Window window)		/* Window. */
{
    MacDrawable *macWin = (MacDrawable *) window;
    XEvent event;

    /*
     * Under certain situations it's possible for this function to be called
     * before the toplevel window it's associated with has actually been
     * mapped. In that case we need to create the real Macintosh window now as
     * this function as well as other X functions assume that the portPtr is
     * valid.
     */

    if (!TkMacOSXHostToplevelExists(macWin->toplevel->winPtr)) {
	TkMacOSXMakeRealWindowExist(macWin->toplevel->winPtr);
    }

    display->request++;
    macWin->winPtr->flags |= TK_MAPPED;
    if (Tk_IsTopLevel(macWin->winPtr)) {
	if (!Tk_IsEmbedded(macWin->winPtr)) {
	    NSWindow *win = TkMacOSXDrawableWindow(window);

	    [win makeKeyAndOrderFront:NSApp];
	    [win windowRef];
	    TkMacOSXApplyWindowAttributes(macWin->winPtr, win);
	}
	TkMacOSXInvalClipRgns((Tk_Window) macWin->winPtr);

	/*
	 * We only need to send the MapNotify event for toplevel windows.
	 */

	event.xany.serial = LastKnownRequestProcessed(display);
	event.xany.send_event = False;
	event.xany.display = display;

	event.xmap.window = window;
	event.xmap.type = MapNotify;
	event.xmap.event = window;
	event.xmap.override_redirect = macWin->winPtr->atts.override_redirect;
	Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
    } else {
	/*
	 * Generate damage for that area of the window.
	 */

	TkMacOSXInvalClipRgns((Tk_Window) macWin->winPtr->parentPtr);
	TkMacOSXInvalidateWindow(macWin, TK_PARENT_WINDOW);
    }

    /*
     * Generate VisibilityNotify events for window and all mapped children.
     */

    event.xany.send_event = False;
    event.xany.display = display;
    event.xvisibility.type = VisibilityNotify;
    event.xvisibility.state = VisibilityUnobscured;
    NotifyVisibility(macWin->winPtr, &event);
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyVisibility --
 *
 *	Recursively called helper proc for XMapWindow().
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	VisibilityNotify events are queued.
 *
 *----------------------------------------------------------------------
 */

static void
NotifyVisibility(
    TkWindow *winPtr,
    XEvent *eventPtr)
{
    if (winPtr->atts.event_mask & VisibilityChangeMask) {
	eventPtr->xany.serial = LastKnownRequestProcessed(winPtr->display);
	eventPtr->xvisibility.window = winPtr->window;
	Tk_QueueWindowEvent(eventPtr, TCL_QUEUE_TAIL);
    }
    for (winPtr = winPtr->childList; winPtr != NULL;
	    winPtr = winPtr->nextPtr) {
	if (winPtr->flags & TK_MAPPED) {
	    NotifyVisibility(winPtr, eventPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XUnmapWindow --
 *
 *	Unmap the given X Window to the screen. See X window documentation for
 *	more details.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The subwindow or toplevel may be removed from the screen.
 *
 *----------------------------------------------------------------------
 */

void
XUnmapWindow(
    Display *display,		/* Display. */
    Window window)		/* Window. */
{
    MacDrawable *macWin = (MacDrawable *) window;
    XEvent event;

    display->request++;
    macWin->winPtr->flags &= ~TK_MAPPED;
    if (Tk_IsTopLevel(macWin->winPtr)) {
	if (!Tk_IsEmbedded(macWin->winPtr) &&
		macWin->winPtr->wmInfoPtr->hints.initial_state!=IconicState) {
	    NSWindow *win = TkMacOSXDrawableWindow(window);

	    if ([win isVisible]) {
		[[win parentWindow] removeChildWindow:win];
		[win orderOut:NSApp];
	    }
	}
	TkMacOSXInvalClipRgns((Tk_Window) macWin->winPtr);

	/*
	 * We only need to send the UnmapNotify event for toplevel windows.
	 */

	event.xany.serial = LastKnownRequestProcessed(display);
	event.xany.send_event = False;
	event.xany.display = display;

	event.xunmap.type = UnmapNotify;
	event.xunmap.window = window;
	event.xunmap.event = window;
	event.xunmap.from_configure = false;
	Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
    } else {
	/*
	 * Generate damage for that area of the window.
	 */

	TkMacOSXInvalidateWindow(macWin, TK_PARENT_WINDOW);
	TkMacOSXInvalClipRgns((Tk_Window) macWin->winPtr->parentPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XResizeWindow --
 *
 *	Resize a given X window. See X windows documentation for further
 *	details.
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
XResizeWindow(
    Display *display,		/* Display. */
    Window window,		/* Window. */
    unsigned int width,
    unsigned int height)
{
    MacDrawable *macWin = (MacDrawable *) window;

    display->request++;
    if (Tk_IsTopLevel(macWin->winPtr) && !Tk_IsEmbedded(macWin->winPtr)) {
	NSWindow *w = macWin->winPtr->wmInfoPtr->window;

	if (w) {
	    NSRect r = [w contentRectForFrameRect:[w frame]];
	    r.origin.y += r.size.height - height;
	    r.size.width = width;
	    r.size.height = height;
	    [w setFrame:[w frameRectForContentRect:r] display:YES];
	}
    } else {
	MoveResizeWindow(macWin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XMoveResizeWindow --
 *
 *	Move or resize a given X window. See X windows documentation
 *	for further details.
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
XMoveResizeWindow(
    Display *display,		/* Display. */
    Window window,		/* Window. */
    int x, int y,
    unsigned int width,
    unsigned int height)
{
    MacDrawable *macWin = (MacDrawable *) window;

    display->request++;
    if (Tk_IsTopLevel(macWin->winPtr) && !Tk_IsEmbedded(macWin->winPtr)) {
	NSWindow *w = macWin->winPtr->wmInfoPtr->window;

	if (w) {
	    NSRect r = NSMakeRect(x + macWin->winPtr->wmInfoPtr->xInParent,
		    tkMacOSXZeroScreenHeight - (y +
		    macWin->winPtr->wmInfoPtr->yInParent + height),
		    width, height);
	    [w setFrame:[w frameRectForContentRect:r] display:YES];
	}
    } else {
	MoveResizeWindow(macWin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XMoveWindow --
 *
 *	Move a given X window. See X windows documentation for further
 *	details.
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
XMoveWindow(
    Display *display,		/* Display. */
    Window window,		/* Window. */
    int x, int y)
{
    MacDrawable *macWin = (MacDrawable *) window;

    display->request++;
    if (Tk_IsTopLevel(macWin->winPtr) && !Tk_IsEmbedded(macWin->winPtr)) {
	NSWindow *w = macWin->winPtr->wmInfoPtr->window;

	if (w) {
	    [w setFrameTopLeftPoint:NSMakePoint(x, tkMacOSXZeroScreenHeight - y)];
	}
    } else {
	MoveResizeWindow(macWin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MoveResizeWindow --
 *
 *	Helper proc for XResizeWindow, XMoveResizeWindow and XMoveWindow.
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
MoveResizeWindow(
    MacDrawable *macWin)
{
    int deltaX = 0, deltaY = 0, parentBorderwidth = 0;
    MacDrawable *macParent = NULL;
    NSWindow *macWindow = TkMacOSXDrawableWindow((Drawable) macWin);

    /*
     * Find the Parent window, for an embedded window it will be its container.
     */

    if (Tk_IsEmbedded(macWin->winPtr)) {
	TkWindow *contWinPtr = TkpGetOtherWindow(macWin->winPtr);

	if (contWinPtr) {
	    macParent = contWinPtr->privatePtr;
	} else {
	    /*
	     * Here we should handle out of process embedding. At this point,
	     * we are assuming that the changes.x,y is not maintained, if you
	     * need the info get it from Tk_GetRootCoords, and that the
	     * toplevel sits at 0,0 when it is drawn.
	     */
	}
    } else {
	/*
	 * TODO: update all xOff & yOffs
	 */

	macParent = macWin->winPtr->parentPtr->privatePtr;
	parentBorderwidth = macWin->winPtr->parentPtr->changes.border_width;
    }

    if (macParent) {
	deltaX = macParent->xOff + parentBorderwidth +
		macWin->winPtr->changes.x - macWin->xOff;
	deltaY = macParent->yOff + parentBorderwidth +
		macWin->winPtr->changes.y - macWin->yOff;
    }
    if (macWindow) {
	TkMacOSXInvalidateWindow(macWin, TK_PARENT_WINDOW);
	if (macParent) {
	    TkMacOSXInvalClipRgns((Tk_Window) macParent->winPtr);
	}
    }
    UpdateOffsets(macWin->winPtr, deltaX, deltaY);
    if (macWindow) {
	TkMacOSXInvalidateWindow(macWin, TK_PARENT_WINDOW);
    }
    GenerateConfigureNotify(macWin->winPtr, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateConfigureNotify --
 *
 *	Generates ConfigureNotify events for all the child widgets of the
 *	widget passed in the winPtr parameter. If includeWin is true, also
 *	generates ConfigureNotify event for the widget itself.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	ConfigureNotify events will be posted.
 *
 *----------------------------------------------------------------------
 */

static void
GenerateConfigureNotify(
    TkWindow *winPtr,
    int includeWin)
{
    TkWindow *childPtr;

    for (childPtr = winPtr->childList; childPtr != NULL;
	    childPtr = childPtr->nextPtr) {
	if (!Tk_IsMapped(childPtr) || Tk_IsTopLevel(childPtr)) {
	    continue;
	}
	GenerateConfigureNotify(childPtr, 1);
    }
    if (includeWin) {
	TkDoConfigureNotify(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XRaiseWindow --
 *
 *	Change the stacking order of a window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the stacking order of the specified window.
 *
 *----------------------------------------------------------------------
 */

void
XRaiseWindow(
    Display *display,		/* Display. */
    Window window)		/* Window. */
{
    MacDrawable *macWin = (MacDrawable *) window;

    display->request++;
    if (Tk_IsTopLevel(macWin->winPtr) && !Tk_IsEmbedded(macWin->winPtr)) {
	TkWmRestackToplevel(macWin->winPtr, Above, NULL);
    } else {
	/*
	 * TODO: this should generate damage
	 */
    }
}

#if 0
/*
 *----------------------------------------------------------------------
 *
 * XLowerWindow --
 *
 *	Change the stacking order of a window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the stacking order of the specified window.
 *
 *----------------------------------------------------------------------
 */

void
XLowerWindow(
    Display *display,		/* Display. */
    Window window)		/* Window. */
{
    MacDrawable *macWin = (MacDrawable *) window;

    display->request++;
    if (Tk_IsTopLevel(macWin->winPtr) && !Tk_IsEmbedded(macWin->winPtr)) {
	TkWmRestackToplevel(macWin->winPtr, Below, NULL);
    } else {
	/*
	 * TODO: this should generate damage
	 */
    }
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * XConfigureWindow --
 *
 *	Change the size, position, stacking, or border of the specified window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the attributes of the specified window. Note that we ignore the
 *	passed in values and use the values stored in the TkWindow data
 *	structure.
 *
 *----------------------------------------------------------------------
 */

void
XConfigureWindow(
    Display *display,		/* Display. */
    Window w,			/* Window. */
    unsigned int value_mask,
    XWindowChanges *values)
{
    MacDrawable *macWin = (MacDrawable *) w;
    TkWindow *winPtr = macWin->winPtr;

    display->request++;

    /*
     * Change the shape and/or position of the window.
     */

    if (value_mask & (CWX|CWY|CWWidth|CWHeight)) {
	XMoveResizeWindow(display, w, winPtr->changes.x, winPtr->changes.y,
		winPtr->changes.width, winPtr->changes.height);
    }

    /*
     * Change the stacking order of the window. Tk actually keeps all the
     * information we need for stacking order. All we need to do is make sure
     * the clipping regions get updated and generate damage that will ensure
     * things get drawn correctly.
     */

    if (value_mask & CWStackMode) {
	NSView *view = TkMacOSXDrawableView(macWin);
	Rect bounds;
	NSRect r;

	if (view) {
	    TkMacOSXInvalClipRgns((Tk_Window) winPtr->parentPtr);
	    TkMacOSXWinBounds(winPtr, &bounds);
	    r = NSMakeRect(bounds.left,
		[view bounds].size.height - bounds.bottom,
		bounds.right - bounds.left, bounds.bottom - bounds.top);
	    [view setNeedsDisplayInRect:r];
	}
    }

    /* TkGenWMMoveRequestEvent(macWin->winPtr,
	    macWin->winPtr->changes.x, macWin->winPtr->changes.y); */
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXUpdateClipRgn --
 *
 *	This function updates the cliping regions for a given window and all of
 *	its children. Once updated the TK_CLIP_INVALID flag in the subwindow
 *	data structure is unset. The TK_CLIP_INVALID flag should always be
 *	unset before any drawing is attempted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The clip regions for the window and its children are updated.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXUpdateClipRgn(
    TkWindow *winPtr)
{
    MacDrawable *macWin;

    if (winPtr == NULL) {
	return;
    }
    macWin = winPtr->privatePtr;
    if (macWin && macWin->flags & TK_CLIP_INVALID) {
	TkWindow *win2Ptr;

#ifdef TK_MAC_DEBUG_CLIP_REGIONS
	TkMacOSXDbgMsg("%s", winPtr->pathName);
#endif
	if (Tk_IsMapped(winPtr)) {
	    int rgnChanged = 0;
	    CGRect bounds;
	    HIMutableShapeRef rgn;

	    /*
	     * Start with a region defined by the window bounds.
	     */

	    TkMacOSXWinCGBounds(winPtr, &bounds);
	    rgn = TkMacOSXHIShapeCreateMutableWithRect(&bounds);

	    /*
	     * Clip away the area of any windows that may obscure this window.
	     * For a non-toplevel window, first, clip to the parents visible
	     * clip region. Second, clip away any siblings that are higher in
	     * the stacking order. For an embedded toplevel, just clip to the
	     * container's visible clip region. Remember, we only allow one
	     * contained window in a frame, and don't support any other widgets
	     * in the frame either. This is not currently enforced, however.
	     */

	    if (!Tk_IsTopLevel(winPtr)) {
		TkMacOSXUpdateClipRgn(winPtr->parentPtr);
		if (winPtr->parentPtr) {
		    ChkErr(HIShapeIntersect,
			    winPtr->parentPtr->privatePtr->aboveVisRgn,
			    rgn, rgn);
		}
		win2Ptr = winPtr;
		while ((win2Ptr = win2Ptr->nextPtr)) {
		    if (Tk_IsTopLevel(win2Ptr) || !Tk_IsMapped(win2Ptr)) {
			continue;
		    }
		    TkMacOSXWinCGBounds(win2Ptr, &bounds);
		    ChkErr(TkMacOSHIShapeDifferenceWithRect, rgn, &bounds);
		}
	    } else if (Tk_IsEmbedded(winPtr)) {
		win2Ptr = TkpGetOtherWindow(winPtr);
		if (win2Ptr) {
		    TkMacOSXUpdateClipRgn(win2Ptr);
		    ChkErr(HIShapeIntersect,
			    win2Ptr->privatePtr->aboveVisRgn, rgn, rgn);
		} else if (tkMacOSXEmbedHandler != NULL) {
		    TkRegion r = TkCreateRegion();
		    HIShapeRef visRgn;

		    tkMacOSXEmbedHandler->getClipProc((Tk_Window) winPtr, r);
		    visRgn = TkMacOSXGetNativeRegion(r);
		    ChkErr(HIShapeIntersect, visRgn, rgn, rgn);
		    CFRelease(visRgn);
		    TkpReleaseRegion(r);
		}

		/*
		 * TODO: Here we should handle out of process embedding.
		 */
	    } else if (winPtr->wmInfoPtr->attributes &
		    kWindowResizableAttribute) {
		NSWindow *w = TkMacOSXDrawableWindow(winPtr->window);

		if (w) {
		    bounds = NSRectToCGRect([w _growBoxRect]);
		    bounds.origin.y = [w contentRectForFrameRect:
			    [w frame]].size.height - bounds.size.height -
			    bounds.origin.y;
		    ChkErr(TkMacOSHIShapeDifferenceWithRect, rgn, &bounds);
		}
	    }
	    macWin->aboveVisRgn = HIShapeCreateCopy(rgn);

	    /*
	     * The final clip region is the aboveVis region (or visible region)
	     * minus all the children of this window. If the window is a
	     * container, we must also subtract the region of the embedded
	     * window.
	     */

	    win2Ptr = winPtr->childList;
	    while (win2Ptr) {
		if (Tk_IsTopLevel(win2Ptr) || !Tk_IsMapped(win2Ptr)) {
		    win2Ptr = win2Ptr->nextPtr;
		    continue;
		}
		TkMacOSXWinCGBounds(win2Ptr, &bounds);
		ChkErr(TkMacOSHIShapeDifferenceWithRect, rgn, &bounds);
		rgnChanged = 1;
		win2Ptr = win2Ptr->nextPtr;
	    }

	    if (Tk_IsContainer(winPtr)) {
		win2Ptr = TkpGetOtherWindow(winPtr);
		if (win2Ptr) {
		    if (Tk_IsMapped(win2Ptr)) {
			TkMacOSXWinCGBounds(win2Ptr, &bounds);
			ChkErr(TkMacOSHIShapeDifferenceWithRect, rgn, &bounds);
			rgnChanged = 1;
		    }
		}

		/*
		 * TODO: Here we should handle out of process embedding.
		 */
	    }

	    if (rgnChanged) {
		HIShapeRef diffRgn = HIShapeCreateDifference(
			macWin->aboveVisRgn, rgn);

		if (!HIShapeIsEmpty(diffRgn)) {
		    macWin->visRgn = HIShapeCreateCopy(rgn);
		}
		CFRelease(diffRgn);
	    }
	    CFRelease(rgn);
	} else {
	    /*
	     * An unmapped window has empty clip regions to prevent any
	     * (erroneous) drawing into it or its children from becoming
	     * visible. [Bug 940117]
	     */

	    if (!Tk_IsTopLevel(winPtr)) {
		TkMacOSXUpdateClipRgn(winPtr->parentPtr);
	    } else if (Tk_IsEmbedded(winPtr)) {
		win2Ptr = TkpGetOtherWindow(winPtr);
		if (win2Ptr) {
		    TkMacOSXUpdateClipRgn(win2Ptr);
		}
	    }
	    macWin->aboveVisRgn = TkMacOSXHIShapeCreateEmpty();
	}
	if (!macWin->visRgn) {
	    macWin->visRgn = HIShapeCreateCopy(macWin->aboveVisRgn);
	}
	macWin->flags &= ~TK_CLIP_INVALID;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXVisableClipRgn --
 *
 *	This function returns the Macintosh cliping region for the given
 *	window. The caller is responsible for disposing of the returned
 *	region via TkDestroyRegion().
 *
 * Results:
 *	The region.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkRegion
TkMacOSXVisableClipRgn(
    TkWindow *winPtr)
{
    if (winPtr->privatePtr->flags & TK_CLIP_INVALID) {
	TkMacOSXUpdateClipRgn(winPtr);
    }
    return (TkRegion)HIShapeCreateMutableCopy(winPtr->privatePtr->visRgn);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInvalidateViewRegion --
 *
 *	This function invalidates the given region of a view.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Damage is created.
 *
 *----------------------------------------------------------------------
 */

static OSStatus
InvalViewRect(int msg, HIShapeRef rgn, const CGRect *rect, void *ref) {
    static CGAffineTransform t;
    NSView *view = ref;

    if (!view) {
	return paramErr;
    }
    switch (msg) {
    case kHIShapeEnumerateInit:
	t = CGAffineTransformMake(1.0, 0.0, 0.0, -1.0, 0.0,
		NSHeight([view bounds]));
	break;
    case kHIShapeEnumerateRect:
	[view setNeedsDisplayInRect:NSRectFromCGRect(
		CGRectApplyAffineTransform(*rect, t))];
	break;
    }
    return noErr;
}

void
TkMacOSXInvalidateViewRegion(
    NSView *view,
    HIShapeRef rgn)
{
    if (view && !HIShapeIsEmpty(rgn)) {
	ChkErr(HIShapeEnumerate, rgn,
		kHIShapeParseFromBottom|kHIShapeParseFromLeft,
		InvalViewRect, view);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInvalidateWindow --
 *
 *	This function invalidates a window and (optionally) its children.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Damage is created.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXInvalidateWindow(
    MacDrawable *macWin,	/* Window to be invalidated. */
    int flag)			/* Should be TK_WINDOW_ONLY or
				 * TK_PARENT_WINDOW */
{
#ifdef TK_MAC_DEBUG_CLIP_REGIONS
    TkMacOSXDbgMsg("%s", winPtr->pathName);
#endif
    if (macWin->flags & TK_CLIP_INVALID) {
	TkMacOSXUpdateClipRgn(macWin->winPtr);
    }
    TkMacOSXInvalidateViewRegion(TkMacOSXDrawableView(macWin),
	    (flag == TK_WINDOW_ONLY) ? macWin->visRgn : macWin->aboveVisRgn);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXDrawableWindow --
 *
 *	This function returns the NSWindow for a given X drawable.
 *
 * Results:
 *	A NSWindow, or nil for off screen pixmaps.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

NSWindow*
TkMacOSXDrawableWindow(
    Drawable drawable)
{
    MacDrawable *macWin = (MacDrawable *) drawable;
    NSWindow *result = nil;

    if (!macWin || macWin->flags & TK_IS_PIXMAP) {
	result = nil;
    } else if (macWin->toplevel && macWin->toplevel->winPtr &&
	    macWin->toplevel->winPtr->wmInfoPtr &&
	    macWin->toplevel->winPtr->wmInfoPtr->window) {
	result = macWin->toplevel->winPtr->wmInfoPtr->window;
    } else if (macWin->winPtr && macWin->winPtr->wmInfoPtr &&
	    macWin->winPtr->wmInfoPtr->window) {
	result = macWin->winPtr->wmInfoPtr->window;
    } else if (macWin->toplevel && (macWin->toplevel->flags & TK_EMBEDDED)) {
	TkWindow *contWinPtr = TkpGetOtherWindow(macWin->toplevel->winPtr);
	if (contWinPtr) {
	    result = TkMacOSXDrawableWindow((Drawable) contWinPtr->privatePtr);
	}
    }
    return result;
}

void *
TkMacOSXDrawable(
    Drawable drawable)
{
    return TkMacOSXDrawableWindow(drawable);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetDrawablePort --
 *
 *	This function returns the Graphics Port for a given X drawable.
 *
 * Results:
 *	NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void *
TkMacOSXGetDrawablePort(
    Drawable drawable)
{
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXDrawableView --
 *
 *	This function returns the NSView for a given X drawable.
 *
 * Results:
 *	A NSView* or nil.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

NSView*
TkMacOSXDrawableView(
    MacDrawable *macWin)
{
    NSView *result = nil;

    if (!macWin) {
	result = nil;
    } else if (!macWin->toplevel) {
	result = macWin->view;
    } else if (!(macWin->toplevel->flags & TK_EMBEDDED)) {
	result = macWin->toplevel->view;
    } else {
	TkWindow *contWinPtr = TkpGetOtherWindow(macWin->toplevel->winPtr);
	if (contWinPtr) {
	    result = TkMacOSXDrawableView(contWinPtr->privatePtr);
	}
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXGetRootControl --
 *
 *	This function returns the NSView for a given X drawable.
 *
 * Results:
 *	A NSView* .
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void *
TkMacOSXGetRootControl(
    Drawable drawable)
{
    /*
     * will probably need to fix this up for embedding
     */

    return TkMacOSXDrawableView((MacDrawable *) drawable);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInvalClipRgns --
 *
 *	This function invalidates the clipping regions for a given window and
 *	all of its children. This function should be called whenever changes
 *	are made to subwindows that would affect the size or position of
 *	windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cliping regions for the window and its children are mark invalid.
 *	(Make sure they are valid before drawing.)
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXInvalClipRgns(
    Tk_Window tkwin)
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    TkWindow *childPtr;
    MacDrawable *macWin = winPtr->privatePtr;

    /*
     * If already marked we can stop because all descendants will also already
     * be marked.
     */

    if (!macWin || macWin->flags & TK_CLIP_INVALID) {
	return;
    }

    macWin->flags |= TK_CLIP_INVALID;
    if (macWin->visRgn) {
	CFRelease(macWin->visRgn);
	macWin->visRgn = NULL;
    }
    if (macWin->aboveVisRgn) {
	CFRelease(macWin->aboveVisRgn);
	macWin->aboveVisRgn = NULL;
    }
    if (macWin->drawRgn) {
	CFRelease(macWin->drawRgn);
	macWin->drawRgn = NULL;
    }

    /*
     * Invalidate clip regions for all children & their descendants, unless the
     * child is a toplevel.
     */

    childPtr = winPtr->childList;
    while (childPtr) {
	if (!Tk_IsTopLevel(childPtr)) {
	    TkMacOSXInvalClipRgns((Tk_Window) childPtr);
	}
	childPtr = childPtr->nextPtr;
    }

    /*
     * Also, if the window is a container, mark its embedded window.
     */

    if (Tk_IsContainer(winPtr)) {
	childPtr = TkpGetOtherWindow(winPtr);

	if (childPtr) {
	    TkMacOSXInvalClipRgns((Tk_Window) childPtr);
	}

	/*
	 * TODO: Here we should handle out of process embedding.
	 */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXWinBounds --
 *
 *	Given a Tk window this function determines the windows bounds in
 *	relation to the Macintosh window's coordinate system. This is also the
 *	same coordinate system as the Tk toplevel window in which this window
 *	is contained.
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
TkMacOSXWinBounds(
    TkWindow *winPtr,
    void *bounds)
{
    Rect *b = (Rect *)bounds;
    b->left = winPtr->privatePtr->xOff;
    b->top = winPtr->privatePtr->yOff;
    b->right = b->left + winPtr->changes.width;
    b->bottom = b->top + winPtr->changes.height;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXWinCGBounds --
 *
 *	Given a Tk window this function determines the windows bounds in
 *	relation to the Macintosh window's coordinate system. This is also the
 *	same coordinate system as the Tk toplevel window in which this window
 *	is contained.
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
TkMacOSXWinCGBounds(
    TkWindow *winPtr,
    CGRect *bounds)
{
    bounds->origin.x = winPtr->privatePtr->xOff;
    bounds->origin.y = winPtr->privatePtr->yOff;
    bounds->size.width = winPtr->changes.width;
    bounds->size.height = winPtr->changes.height;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateOffsets --
 *
 *	Updates the X & Y offsets of the given TkWindow from the TopLevel it is
 *	a decendant of.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The xOff & yOff fields for the Mac window datastructure is updated to
 *	the proper offset.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateOffsets(
    TkWindow *winPtr,
    int deltaX,
    int deltaY)
{
    TkWindow *childPtr;

    if (winPtr->privatePtr == NULL) {
	/*
	 * We haven't called Tk_MakeWindowExist for this window yet. The offset
	 * information will be postponed and calulated at that time. (This will
	 * usually only happen when a mapped parent is being moved but has
	 * child windows that have yet to be mapped.)
	 */

	return;
    }

    winPtr->privatePtr->xOff += deltaX;
    winPtr->privatePtr->yOff += deltaY;

    childPtr = winPtr->childList;
    while (childPtr != NULL) {
	if (!Tk_IsTopLevel(childPtr)) {
	    UpdateOffsets(childPtr, deltaX, deltaY);
	}
	childPtr = childPtr->nextPtr;
    }

    if (Tk_IsContainer(winPtr)) {
	childPtr = TkpGetOtherWindow(winPtr);
	if (childPtr != NULL) {
	    UpdateOffsets(childPtr,deltaX,deltaY);
	}

	/*
	 * TODO: Here we should handle out of process embedding.
	 */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetPixmap --
 *
 *	Creates an in memory drawing surface.
 *
 * Results:
 *	Returns a handle to a new pixmap.
 *
 * Side effects:
 *	Allocates a new Macintosh GWorld.
 *
 *----------------------------------------------------------------------
 */

Pixmap
Tk_GetPixmap(
    Display *display,	/* Display for new pixmap (can be null). */
    Drawable d,		/* Drawable where pixmap will be used (ignored). */
    int width,		/* Dimensions of pixmap. */
    int height,
    int depth)		/* Bits per pixel for pixmap. */
{
    MacDrawable *macPix;

    if (display != NULL) {
	display->request++;
    }
    macPix = (MacDrawable *) ckalloc(sizeof(MacDrawable));
    macPix->winPtr = NULL;
    macPix->xOff = 0;
    macPix->yOff = 0;
    macPix->visRgn = NULL;
    macPix->aboveVisRgn = NULL;
    macPix->drawRgn = NULL;
    macPix->referenceCount = 0;
    macPix->toplevel = NULL;
    macPix->flags = TK_IS_PIXMAP | (depth == 1 ? TK_IS_BW_PIXMAP : 0);
    macPix->view = nil;
    macPix->context = NULL;
    macPix->size = CGSizeMake(width, height);

    return (Pixmap) macPix;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_FreePixmap --
 *
 *	Release the resources associated with a pixmap.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes the Macintosh GWorld created by Tk_GetPixmap.
 *
 *----------------------------------------------------------------------
 */

void
Tk_FreePixmap(
    Display *display,		/* Display. */
    Pixmap pixmap)		/* Pixmap to destroy */
{
    MacDrawable *macPix = (MacDrawable *) pixmap;

    display->request++;
    if (macPix->context) {
	char *data = CGBitmapContextGetData(macPix->context);

	if (data) {
	    ckfree(data);
	}
	CFRelease(macPix->context);
    }
    ckfree((char *) macPix);
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
