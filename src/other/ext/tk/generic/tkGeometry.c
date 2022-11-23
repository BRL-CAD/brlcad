/*
 * tkGeometry.c --
 *
 *	This file contains generic Tk code for geometry management (stuff
 *	that's used by all geometry managers).
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkInt.h"

/*
 * Data structures of the following type are used by Tk_MaintainGeometry. For
 * each content managed by Tk_MaintainGeometry, there is one of these structures
 * associated with its container.
 */

typedef struct MaintainContent {
    Tk_Window content;		/* The content window being positioned. */
    Tk_Window container;		/* The container that determines content's
				 * position; it must be a descendant of
				 * content's parent. */
    int x, y;			/* Desired position of content relative to
				 * container. */
    int width, height;		/* Desired dimensions of content. */
    struct MaintainContent *nextPtr;
				/* Next in list of Maintains associated with
				 * container. */
} MaintainContent;

/*
 * For each window that has been specified as a content to Tk_MaintainGeometry,
 * there is a structure of the following type:
 */

typedef struct MaintainContainer {
    Tk_Window ancestor;		/* The lowest ancestor of this window for
				 * which we have *not* created a
				 * StructureNotify handler. May be the same as
				 * the window itself. */
    int checkScheduled;		/* Non-zero means that there is already a call
				 * to MaintainCheckProc scheduled as an idle
				 * handler. */
    MaintainContent *contentPtr;	/* First in list of all content associated with
				 * this container. */
} MaintainContainer;

/*
 * Prototypes for static procedures in this file:
 */

static void		MaintainCheckProc(ClientData clientData);
static void		MaintainContainerProc(ClientData clientData,
			    XEvent *eventPtr);
static void		MaintainContentProc(ClientData clientData,
			    XEvent *eventPtr);

/*
 *--------------------------------------------------------------
 *
 * Tk_ManageGeometry --
 *
 *	Arrange for a particular procedure to manage the geometry of a given
 *	content window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Proc becomes the new geometry manager for tkwin, replacing any
 *	previous geometry manager. The geometry manager will be notified (by
 *	calling procedures in *mgrPtr) when interesting things happen in the
 *	future. If there was an existing geometry manager for tkwin different
 *	from the new one, it is notified by calling its lostSlaveProc.
 *
 *--------------------------------------------------------------
 */

void
Tk_ManageGeometry(
    Tk_Window tkwin,		/* Window whose geometry is to be managed by
				 * proc. */
    const Tk_GeomMgr *mgrPtr,	/* Static structure describing the geometry
				 * manager. This structure must never go
				 * away. */
    ClientData clientData)	/* Arbitrary one-word argument to pass to
				 * geometry manager procedures. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;

    if ((winPtr->geomMgrPtr != NULL) && (mgrPtr != NULL)
	    && ((winPtr->geomMgrPtr != mgrPtr)
		|| (winPtr->geomData != clientData))
	    && (winPtr->geomMgrPtr->lostSlaveProc != NULL)) {
	winPtr->geomMgrPtr->lostSlaveProc(winPtr->geomData, tkwin);
    }

    winPtr->geomMgrPtr = mgrPtr;
    winPtr->geomData = clientData;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_GeometryRequest --
 *
 *	This procedure is invoked by widget code to indicate its preferences
 *	about the size of a window it manages. In general, widget code should
 *	call this procedure rather than Tk_ResizeWindow.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The geometry manager for tkwin (if any) is invoked to handle the
 *	request. If possible, it will reconfigure tkwin and/or other windows
 *	to satisfy the request. The caller gets no indication of success or
 *	failure, but it will get X events if the window size was actually
 *	changed.
 *
 *--------------------------------------------------------------
 */

void
Tk_GeometryRequest(
    Tk_Window tkwin,		/* Window that geometry information pertains
				 * to. */
    int reqWidth, int reqHeight)/* Minimum desired dimensions for window, in
				 * pixels. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;

    /*
     * X gets very upset if a window requests a width or height of zero, so
     * rounds requested sizes up to at least 1.
     */

    if (reqWidth <= 0) {
	reqWidth = 1;
    }
    if (reqHeight <= 0) {
	reqHeight = 1;
    }
    if ((reqWidth == winPtr->reqWidth) && (reqHeight == winPtr->reqHeight)) {
	return;
    }
    winPtr->reqWidth = reqWidth;
    winPtr->reqHeight = reqHeight;
    if ((winPtr->geomMgrPtr != NULL)
	    && (winPtr->geomMgrPtr->requestProc != NULL)) {
	winPtr->geomMgrPtr->requestProc(winPtr->geomData, tkwin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SetInternalBorderEx --
 *
 *	Notify relevant geometry managers that a window has an internal border
 *	of a given width and that child windows should not be placed on that
 *	border.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The border widths are recorded for the window, and all geometry
 *	managers of all children are notified so that can re-layout, if
 *	necessary.
 *
 *----------------------------------------------------------------------
 */

void
Tk_SetInternalBorderEx(
    Tk_Window tkwin,		/* Window that will have internal border. */
    int left, int right,	/* Width of internal border, in pixels. */
    int top, int bottom)
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    int changed = 0;

    if (left < 0) {
	left = 0;
    }
    if (left != winPtr->internalBorderLeft) {
	winPtr->internalBorderLeft = left;
	changed = 1;
    }

    if (right < 0) {
	right = 0;
    }
    if (right != winPtr->internalBorderRight) {
	winPtr->internalBorderRight = right;
	changed = 1;
    }

    if (top < 0) {
	top = 0;
    }
    if (top != winPtr->internalBorderTop) {
	winPtr->internalBorderTop = top;
	changed = 1;
    }

    if (bottom < 0) {
	bottom = 0;
    }
    if (bottom != winPtr->internalBorderBottom) {
	winPtr->internalBorderBottom = bottom;
	changed = 1;
    }

    /*
     * All the content for which this is the container window must now be
     * repositioned to take account of the new internal border width. To
     * signal all the geometry managers to do this, trigger a ConfigureNotify
     * event. This will cause geometry managers to recompute everything.
     */

    if (changed) {
	TkDoConfigureNotify(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SetInternalBorder --
 *
 *	Notify relevant geometry managers that a window has an internal border
 *	of a given width and that child windows should not be placed on that
 *	border.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The border width is recorded for the window, and all geometry managers
 *	of all children are notified so that can re-layout, if necessary.
 *
 *----------------------------------------------------------------------
 */

void
Tk_SetInternalBorder(
    Tk_Window tkwin,		/* Window that will have internal border. */
    int width)			/* Width of internal border, in pixels. */
{
    Tk_SetInternalBorderEx(tkwin, width, width, width, width);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SetMinimumRequestSize --
 *
 *	Notify relevant geometry managers that a window has a minimum request
 *	size.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The minimum request size is recorded for the window, and a new size is
 *	requested for the window, if necessary.
 *
 *----------------------------------------------------------------------
 */

void
Tk_SetMinimumRequestSize(
    Tk_Window tkwin,		/* Window that will have internal border. */
    int minWidth, int minHeight)/* Minimum requested size, in pixels. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;

    if ((winPtr->minReqWidth == minWidth) &&
	    (winPtr->minReqHeight == minHeight)) {
	return;
    }

    winPtr->minReqWidth = minWidth;
    winPtr->minReqHeight = minHeight;

    /*
     * The changed min size may cause geometry managers to get a different
     * result, so make them recompute. To signal all the geometry managers to
     * do this, just resize the window to its current size. The
     * ConfigureNotify event will cause geometry managers to recompute
     * everything.
     */

    Tk_ResizeWindow(tkwin, Tk_Width(tkwin), Tk_Height(tkwin));
}

/*
 *----------------------------------------------------------------------
 *
 * TkSetGeometryContainer --
 *
 *	Set a geometry container for this window. Only one container may own
 *	a window at any time.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The geometry container is recorded for the window.
 *
 *----------------------------------------------------------------------
 */

int
TkSetGeometryContainer(
    Tcl_Interp *interp,		/* Current interpreter, for error. */
    Tk_Window tkwin,		/* Window that will have geometry container
				 * set. */
    const char *name)		/* The name of the geometry manager. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;

    if (winPtr->geomMgrName != NULL &&
	    strcmp(winPtr->geomMgrName, name) == 0) {
	return TCL_OK;
    }
    if (winPtr->geomMgrName != NULL) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "cannot use geometry manager %s inside %s which already"
		    " has slaves managed by %s",
		    name, Tk_PathName(tkwin), winPtr->geomMgrName));
	    Tcl_SetErrorCode(interp, "TK", "GEOMETRY", "FIGHT", NULL);
	}
	return TCL_ERROR;
    }

    winPtr->geomMgrName = (char *)ckalloc(strlen(name) + 1);
    strcpy(winPtr->geomMgrName, name);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFreeGeometryContainer --
 *
 *	Remove a geometry container for this window. Only one container may own
 *	a window at any time.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The geometry container is cleared for the window.
 *
 *----------------------------------------------------------------------
 */

void
TkFreeGeometryContainer(
    Tk_Window tkwin,		/* Window that will have geometry container
				 * cleared. */
    const char *name)		/* The name of the geometry manager. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;

    if (winPtr->geomMgrName != NULL &&
	    strcmp(winPtr->geomMgrName, name) != 0) {
	Tcl_Panic("Trying to free %s from geometry manager %s",
		winPtr->geomMgrName, name);
    }
    if (winPtr->geomMgrName != NULL) {
	ckfree(winPtr->geomMgrName);
	winPtr->geomMgrName = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MaintainGeometry --
 *
 *	This procedure is invoked by geometry managers to handle content whose
 *	container's are not their parents. It translates the desired geometry for
 *	the content into the coordinate system of the parent and respositions
 *	the content if it isn't already at the right place. Furthermore, it sets
 *	up event handlers so that if the container (or any of its ancestors up to
 *	the content's parent) is mapped, unmapped, or moved, then the content will
 *	be adjusted to match.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Event handlers are created and state is allocated to keep track of
 *	content. Note: if content was already managed for container by
 *	Tk_MaintainGeometry, then the previous information is replaced with
 *	the new information. The caller must eventually call
 *	Tk_UnmaintainGeometry to eliminate the correspondence (or, the state
 *	is automatically freed when either window is destroyed).
 *
 *----------------------------------------------------------------------
 */

void
Tk_MaintainGeometry(
    Tk_Window window,		/* Window for geometry management. */
    Tk_Window container,		/* Container for window; must be a descendant of
				 * window's parent. */
    int x, int y,		/* Desired position of window within container. */
    int width, int height)	/* Desired dimensions for window. */
{
    Tcl_HashEntry *hPtr;
    MaintainContainer *containerPtr;
    MaintainContent *contentPtr;
    int isNew, map;
    Tk_Window ancestor, parent;
    TkDisplay *dispPtr = ((TkWindow *) container)->dispPtr;

    ((TkWindow *)window)->maintainerPtr = (TkWindow *)container;

    if (container == Tk_Parent(window)) {
	/*
	 * If the window is a direct descendant of the container, don't bother
	 * setting up the extra infrastructure for management, just make a
	 * call to Tk_MoveResizeWindow; the parent/child relationship will
	 * take care of the rest.
	 */

	Tk_MoveResizeWindow(window, x, y, width, height);

	/*
	 * Map the window if the container is already mapped; otherwise, wait
	 * until the container is mapped later (in which case mapping the window
	 * is taken care of elsewhere).
	 */

	if (Tk_IsMapped(container)) {
	    Tk_MapWindow(window);
	}
	return;
    }

    if (!dispPtr->geomInit) {
	dispPtr->geomInit = 1;
	Tcl_InitHashTable(&dispPtr->maintainHashTable, TCL_ONE_WORD_KEYS);
    }

    /*
     * See if there is already a MaintainContainer structure for the container; if
     * not, then create one.
     */

    parent = Tk_Parent(window);
    hPtr = Tcl_CreateHashEntry(&dispPtr->maintainHashTable,
	    (char *) container, &isNew);
    if (!isNew) {
	containerPtr = (MaintainContainer *)Tcl_GetHashValue(hPtr);
    } else {
	containerPtr = (MaintainContainer *)ckalloc(sizeof(MaintainContainer));
	containerPtr->ancestor = container;
	containerPtr->checkScheduled = 0;
	containerPtr->contentPtr = NULL;
	Tcl_SetHashValue(hPtr, containerPtr);
    }

    /*
     * Create a MaintainContent structure for the window if there isn't already
     * one.
     */

    for (contentPtr = containerPtr->contentPtr; contentPtr != NULL;
	    contentPtr = contentPtr->nextPtr) {
	if (contentPtr->content == window) {
	    goto gotContent;
	}
    }
    contentPtr = (MaintainContent *)ckalloc(sizeof(MaintainContent));
    contentPtr->content = window;
    contentPtr->container = container;
    contentPtr->nextPtr = containerPtr->contentPtr;
    containerPtr->contentPtr = contentPtr;
    Tk_CreateEventHandler(window, StructureNotifyMask, MaintainContentProc,
	    contentPtr);

    /*
     * Make sure that there are event handlers registered for all the windows
     * between container and windows's parent (including container but not window's
     * parent). There may already be handlers for container and some of its
     * ancestors (containerPtr->ancestor tells how many).
     */

    for (ancestor = container; ancestor != parent;
	    ancestor = Tk_Parent(ancestor)) {
	if (ancestor == containerPtr->ancestor) {
	    Tk_CreateEventHandler(ancestor, StructureNotifyMask,
		    MaintainContainerProc, containerPtr);
	    containerPtr->ancestor = Tk_Parent(ancestor);
	}
    }

    /*
     * Fill in up-to-date information in the structure, then update the window
     * if it's not currently in the right place or state.
     */

  gotContent:
    contentPtr->x = x;
    contentPtr->y = y;
    contentPtr->width = width;
    contentPtr->height = height;
    map = 1;
    for (ancestor = contentPtr->container; ; ancestor = Tk_Parent(ancestor)) {
	if (!Tk_IsMapped(ancestor) && (ancestor != parent)) {
	    map = 0;
	}
	if (ancestor == parent) {
	    if ((x != Tk_X(contentPtr->content))
		    || (y != Tk_Y(contentPtr->content))
		    || (width != Tk_Width(contentPtr->content))
		    || (height != Tk_Height(contentPtr->content))) {
		Tk_MoveResizeWindow(contentPtr->content, x, y, width, height);
	    }
	    if (map) {
		Tk_MapWindow(contentPtr->content);
	    } else {
		Tk_UnmapWindow(contentPtr->content);
	    }
	    break;
	}
	x += Tk_X(ancestor) + Tk_Changes(ancestor)->border_width;
	y += Tk_Y(ancestor) + Tk_Changes(ancestor)->border_width;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_UnmaintainGeometry --
 *
 *	This procedure cancels a previous Tk_MaintainGeometry call, so that
 *	the relationship between window and container is no longer maintained.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is unmapped and state is released, so that window won't track
 *	container any more. If we weren't previously managing window relative to
 *	container, then this procedure has no effect.
 *
 *----------------------------------------------------------------------
 */

void
Tk_UnmaintainGeometry(
    Tk_Window window,		/* WIndow for geometry management. */
    Tk_Window container)		/* Container for window; must be a descendant of
				 * window's parent. */
{
    Tcl_HashEntry *hPtr;
    MaintainContainer *containerPtr;
    MaintainContent *contentPtr, *prevPtr;
    Tk_Window ancestor;
    TkDisplay *dispPtr = ((TkWindow *) window)->dispPtr;

    ((TkWindow *)window)->maintainerPtr = NULL;

    if (container == Tk_Parent(window)) {
	/*
	 * If the window is a direct descendant of the container,
	 * Tk_MaintainGeometry will not have set up any of the extra
	 * infrastructure. Don't even bother to look for it, just return.
	 */
	return;
    }

    if (!dispPtr->geomInit) {
	dispPtr->geomInit = 1;
	Tcl_InitHashTable(&dispPtr->maintainHashTable, TCL_ONE_WORD_KEYS);
    }

    if (!(((TkWindow *) window)->flags & TK_ALREADY_DEAD)) {
	Tk_UnmapWindow(window);
    }
    hPtr = Tcl_FindHashEntry(&dispPtr->maintainHashTable, container);
    if (hPtr == NULL) {
	return;
    }
    containerPtr = (MaintainContainer *)Tcl_GetHashValue(hPtr);
    contentPtr = containerPtr->contentPtr;
    if (contentPtr->content == window) {
	containerPtr->contentPtr = contentPtr->nextPtr;
    } else {
	for (prevPtr = contentPtr, contentPtr = contentPtr->nextPtr; ;
		prevPtr = contentPtr, contentPtr = contentPtr->nextPtr) {
	    if (contentPtr == NULL) {
		return;
	    }
	    if (contentPtr->content == window) {
		prevPtr->nextPtr = contentPtr->nextPtr;
		break;
	    }
	}
    }
    Tk_DeleteEventHandler(contentPtr->content, StructureNotifyMask,
	    MaintainContentProc, contentPtr);
    ckfree(contentPtr);
    if (containerPtr->contentPtr == NULL) {
	if (containerPtr->ancestor != NULL) {
	    for (ancestor = container; ; ancestor = Tk_Parent(ancestor)) {
		Tk_DeleteEventHandler(ancestor, StructureNotifyMask,
			MaintainContainerProc, containerPtr);
		if (ancestor == containerPtr->ancestor) {
		    break;
		}
	    }
	}
	if (containerPtr->checkScheduled) {
	    Tcl_CancelIdleCall(MaintainCheckProc, containerPtr);
	}
	Tcl_DeleteHashEntry(hPtr);
	ckfree(containerPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MaintainContainerProc --
 *
 *	This procedure is invoked by the Tk event dispatcher in response to
 *	StructureNotify events on the container or one of its ancestors, on
 *	behalf of Tk_MaintainGeometry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	It schedules a call to MaintainCheckProc, which will eventually caused
 *	the postions and mapped states to be recalculated for all the
 *	maintained windows of the container. Or, if the container window is being
 *	deleted then state is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void
MaintainContainerProc(
    ClientData clientData,	/* Pointer to MaintainContainer structure for the
				 * container window. */
    XEvent *eventPtr)		/* Describes what just happened. */
{
    MaintainContainer *containerPtr = (MaintainContainer *)clientData;
    MaintainContent *contentPtr;
    int done;

    if ((eventPtr->type == ConfigureNotify)
	    || (eventPtr->type == MapNotify)
	    || (eventPtr->type == UnmapNotify)) {
	if (!containerPtr->checkScheduled) {
	    containerPtr->checkScheduled = 1;
	    Tcl_DoWhenIdle(MaintainCheckProc, containerPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	/*
	 * Delete all of the state associated with this container, but be careful
	 * not to use containerPtr after the last window is deleted, since its
	 * memory will have been freed.
	 */

	done = 0;
	do {
	    contentPtr = containerPtr->contentPtr;
	    if (contentPtr->nextPtr == NULL) {
		done = 1;
	    }
	    Tk_UnmaintainGeometry(contentPtr->content, contentPtr->container);
	} while (!done);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MaintainContentProc --
 *
 *	This procedure is invoked by the Tk event dispatcher in response to
 *	StructureNotify events on a window being managed by
 *	Tk_MaintainGeometry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the event is a DestroyNotify event then the Maintain state and
 *	event handlers for this window are deleted.
 *
 *----------------------------------------------------------------------
 */

static void
MaintainContentProc(
    ClientData clientData,	/* Pointer to MaintainContent structure for
				 * container-window pair. */
    XEvent *eventPtr)		/* Describes what just happened. */
{
    MaintainContent *contentPtr = (MaintainContent *)clientData;

    if (eventPtr->type == DestroyNotify) {
	Tk_UnmaintainGeometry(contentPtr->content, contentPtr->container);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MaintainCheckProc --
 *
 *	This procedure is invoked by the Tk event dispatcher as an idle
 *	handler, when a container or one of its ancestors has been reconfigured,
 *	mapped, or unmapped. Its job is to scan all of the windows for the
 *	container and reposition them, map them, or unmap them as needed to
 *	maintain their geometry relative to the container.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Windows can get repositioned, mapped, or unmapped.
 *
 *----------------------------------------------------------------------
 */

static void
MaintainCheckProc(
    ClientData clientData)	/* Pointer to MaintainContainer structure for the
				 * container window. */
{
    MaintainContainer *containerPtr = (MaintainContainer *)clientData;
    MaintainContent *contentPtr;
    Tk_Window ancestor, parent;
    int x, y, map;

    containerPtr->checkScheduled = 0;
    for (contentPtr = containerPtr->contentPtr; contentPtr != NULL;
	    contentPtr = contentPtr->nextPtr) {
	parent = Tk_Parent(contentPtr->content);
	x = contentPtr->x;
	y = contentPtr->y;
	map = 1;
	for (ancestor = contentPtr->container; ; ancestor = Tk_Parent(ancestor)) {
	    if (!Tk_IsMapped(ancestor) && (ancestor != parent)) {
		map = 0;
	    }
	    if (ancestor == parent) {
		if ((x != Tk_X(contentPtr->content))
			|| (y != Tk_Y(contentPtr->content))) {
		    Tk_MoveWindow(contentPtr->content, x, y);
		}
		if (map) {
		    Tk_MapWindow(contentPtr->content);
		} else {
		    Tk_UnmapWindow(contentPtr->content);
		}
		break;
	    }
	    x += Tk_X(ancestor) + Tk_Changes(ancestor)->border_width;
	    y += Tk_Y(ancestor) + Tk_Changes(ancestor)->border_width;
	}
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
