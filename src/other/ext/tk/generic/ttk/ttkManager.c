/*
 * Copyright 2005, Joe English.  Freely redistributable.
 *
 * Support routines for geometry managers.
 */

#include "tkInt.h"
#include "ttkManager.h"

/*------------------------------------------------------------------------
 * +++ The Geometry Propagation Dance.
 *
 * When a content window requests a new size or some other parameter changes,
 * the manager recomputes the required size for the container window and calls
 * Tk_GeometryRequest().  This is scheduled as an idle handler so multiple
 * updates can be processed as a single batch.
 *
 * If all goes well, the container's manager will process the request
 * (and so on up the chain to the toplevel window), and the container
 * window will eventually receive a <Configure> event.  At this point
 * it recomputes the size and position of all content windows and places them.
 *
 * If all does not go well, however, the container's request may be ignored
 * (typically because the top-level window has a fixed, user-specified size).
 * Tk doesn't provide any notification when this happens; to account for this,
 * we also schedule an idle handler to call the layout procedure
 * after making a geometry request.
 *
 * +++ Content window removal <<NOTE-LOSTCONTENT>>.
 *
 * There are three conditions under which a content window is removed:
 *
 * (1) Another GM claims control
 * (2) Manager voluntarily relinquishes control
 * (3) Content window is destroyed
 *
 * In case (1), Tk calls the manager's lostSlaveProc.
 * Case (2) is performed by calling Tk_ManageGeometry(window,NULL,0);
 * in this case Tk does _not_ call the lostSlaveProc (documented behavior).
 * Tk doesn't handle case (3) either; to account for that we
 * register an event handler on the content window to track <Destroy> events.
 */

/* ++ Data structures.
 */
typedef struct
{
    Tk_Window 		window;
    Ttk_Manager 	*manager;
    void 		*data;
    unsigned		flags;
} Ttk_Content;

/* content->flags bits:
 */
#define CONTENT_MAPPED 	0x1	/* content windows to be mapped when container is */

struct TtkManager_
{
    Ttk_ManagerSpec	*managerSpec;
    void 		*managerData;
    Tk_Window   	window;
    unsigned		flags;
    int 	 	nContent;
    Ttk_Content 		**content;
};

/* manager->flags bits:
 */
#define MGR_UPDATE_PENDING	0x1
#define MGR_RESIZE_REQUIRED	0x2
#define MGR_RELAYOUT_REQUIRED	0x4

static void ManagerIdleProc(void *);	/* forward */

/* ++ ScheduleUpdate --
 * 	Schedule a call to recompute the size and/or layout,
 *	depending on flags.
 */
static void ScheduleUpdate(Ttk_Manager *mgr, unsigned flags)
{
    if (!(mgr->flags & MGR_UPDATE_PENDING)) {
	Tcl_DoWhenIdle(ManagerIdleProc, mgr);
	mgr->flags |= MGR_UPDATE_PENDING;
    }
    mgr->flags |= flags;
}

/* ++ RecomputeSize --
 * 	Recomputes the required size of the container window,
 * 	makes geometry request.
 */
static void RecomputeSize(Ttk_Manager *mgr)
{
    int width = 1, height = 1;

    if (mgr->managerSpec->RequestedSize(mgr->managerData, &width, &height)) {
	Tk_GeometryRequest(mgr->window, width, height);
	ScheduleUpdate(mgr, MGR_RELAYOUT_REQUIRED);
    }
    mgr->flags &= ~MGR_RESIZE_REQUIRED;
}

/* ++ RecomputeLayout --
 * 	Recompute geometry of all content windows.
 */
static void RecomputeLayout(Ttk_Manager *mgr)
{
    mgr->managerSpec->PlaceSlaves(mgr->managerData);
    mgr->flags &= ~MGR_RELAYOUT_REQUIRED;
}

/* ++ ManagerIdleProc --
 * 	DoWhenIdle procedure for deferred updates.
 */
static void ManagerIdleProc(ClientData clientData)
{
    Ttk_Manager *mgr = (Ttk_Manager *)clientData;
    mgr->flags &= ~MGR_UPDATE_PENDING;

    if (mgr->flags & MGR_RESIZE_REQUIRED) {
	RecomputeSize(mgr);
    }
    if (mgr->flags & MGR_RELAYOUT_REQUIRED) {
	if (mgr->flags & MGR_UPDATE_PENDING) {
	    /* RecomputeSize has scheduled another update; relayout later */
	    return;
	}
	RecomputeLayout(mgr);
    }
}

/*------------------------------------------------------------------------
 * +++ Event handlers.
 */

/* ++ ManagerEventHandler --
 * 	Recompute content layout when container widget is resized.
 * 	Keep the content's map state in sync with the container's.
 */
static const int ManagerEventMask = StructureNotifyMask;
static void ManagerEventHandler(ClientData clientData, XEvent *eventPtr)
{
    Ttk_Manager *mgr = (Ttk_Manager *)clientData;
    int i;

    switch (eventPtr->type)
    {
	case ConfigureNotify:
	    RecomputeLayout(mgr);
	    break;
	case MapNotify:
	    for (i = 0; i < mgr->nContent; ++i) {
		Ttk_Content *content = mgr->content[i];
		if (content->flags & CONTENT_MAPPED) {
		    Tk_MapWindow(content->window);
		}
	    }
	    break;
	case UnmapNotify:
	    for (i = 0; i < mgr->nContent; ++i) {
		Ttk_Content *content = mgr->content[i];
		Tk_UnmapWindow(content->window);
	    }
	    break;
    }
}

/* ++ ContentLostEventHandler --
 * 	Notifies manager when a content window is destroyed
 * 	(see <<NOTE-LOSTCONTENT>>).
 */
static void ContentLostEventHandler(void *clientData, XEvent *eventPtr)
{
    Ttk_Content *content = (Ttk_Content *)clientData;
    if (eventPtr->type == DestroyNotify) {
	content->manager->managerSpec->tkGeomMgr.lostSlaveProc(
	    content->manager, content->window);
    }
}

/*------------------------------------------------------------------------
 * +++ Content initialization and cleanup.
 */

static Ttk_Content *NewContent(
    Ttk_Manager *mgr, Tk_Window window, void *data)
{
    Ttk_Content *content = (Ttk_Content *)ckalloc(sizeof(Ttk_Content));

    content->window = window;
    content->manager = mgr;
    content->flags = 0;
    content->data = data;

    return content;
}

static void DeleteContent(Ttk_Content *content)
{
    ckfree(content);
}

/*------------------------------------------------------------------------
 * +++ Manager initialization and cleanup.
 */

Ttk_Manager *Ttk_CreateManager(
    Ttk_ManagerSpec *managerSpec, void *managerData, Tk_Window window)
{
    Ttk_Manager *mgr = (Ttk_Manager *)ckalloc(sizeof(*mgr));

    mgr->managerSpec 	= managerSpec;
    mgr->managerData	= managerData;
    mgr->window	= window;
    mgr->nContent 	= 0;
    mgr->content 	= NULL;
    mgr->flags  	= 0;

    Tk_CreateEventHandler(
	mgr->window, ManagerEventMask, ManagerEventHandler, mgr);

    return mgr;
}

void Ttk_DeleteManager(Ttk_Manager *mgr)
{
    Tk_DeleteEventHandler(
	mgr->window, ManagerEventMask, ManagerEventHandler, mgr);

    while (mgr->nContent > 0) {
	Ttk_ForgetContent(mgr, mgr->nContent - 1);
    }
    if (mgr->content) {
	ckfree(mgr->content);
    }

    Tcl_CancelIdleCall(ManagerIdleProc, mgr);

    ckfree(mgr);
}

/*------------------------------------------------------------------------
 * +++ Content window management.
 */

/* ++ InsertContent --
 * 	Adds content to the list of managed windows.
 */
static void InsertContent(Ttk_Manager *mgr, Ttk_Content *content, int index)
{
    int endIndex = mgr->nContent++;
    mgr->content = (Ttk_Content **)ckrealloc(mgr->content, mgr->nContent * sizeof(Ttk_Content *));

    while (endIndex > index) {
	mgr->content[endIndex] = mgr->content[endIndex - 1];
	--endIndex;
    }

    mgr->content[index] = content;

    Tk_ManageGeometry(content->window,
	&mgr->managerSpec->tkGeomMgr, mgr);

    Tk_CreateEventHandler(content->window,
	StructureNotifyMask, ContentLostEventHandler, content);

    ScheduleUpdate(mgr, MGR_RESIZE_REQUIRED);
}

/* RemoveContent --
 * 	Unmanage and delete the content window.
 *
 * NOTES/ASSUMPTIONS:
 *
 * [1] It's safe to call Tk_UnmapWindow / Tk_UnmaintainGeometry even if this
 * routine is called from the content window's DestroyNotify event handler.
 */
static void RemoveContent(Ttk_Manager *mgr, int index)
{
    Ttk_Content *content = mgr->content[index];
    int i;

    /* Notify manager:
     */
    mgr->managerSpec->SlaveRemoved(mgr->managerData, index);

    /* Remove from array:
     */
    --mgr->nContent;
    for (i = index ; i < mgr->nContent; ++i) {
	mgr->content[i] = mgr->content[i+1];
    }

    /* Clean up:
     */
    Tk_DeleteEventHandler(
	content->window, StructureNotifyMask, ContentLostEventHandler, content);

    /* Note [1] */
    Tk_UnmaintainGeometry(content->window, mgr->window);
    Tk_UnmapWindow(content->window);

    DeleteContent(content);

    ScheduleUpdate(mgr, MGR_RESIZE_REQUIRED);
}

/*------------------------------------------------------------------------
 * +++ Tk_GeomMgr hooks.
 */

void Ttk_GeometryRequestProc(ClientData clientData, Tk_Window window)
{
    Ttk_Manager *mgr = (Ttk_Manager *)clientData;
    int index = Ttk_ContentIndex(mgr, window);
    int reqWidth = Tk_ReqWidth(window);
    int reqHeight= Tk_ReqHeight(window);

    if (mgr->managerSpec->SlaveRequest(
		mgr->managerData, index, reqWidth, reqHeight))
    {
	ScheduleUpdate(mgr, MGR_RESIZE_REQUIRED);
    }
}

void Ttk_LostContentProc(ClientData clientData, Tk_Window window)
{
    Ttk_Manager *mgr = (Ttk_Manager *)clientData;
    int index = Ttk_ContentIndex(mgr, window);

    /* ASSERT: index >= 0 */
    RemoveContent(mgr, index);
}

/*------------------------------------------------------------------------
 * +++ Public API.
 */

/* ++ Ttk_InsertContent --
 * 	Add a new content window at the specified index.
 */
void Ttk_InsertContent(
    Ttk_Manager *mgr, int index, Tk_Window tkwin, void *data)
{
    Ttk_Content *content = NewContent(mgr, tkwin, data);
    InsertContent(mgr, content, index);
}

/* ++ Ttk_ForgetContent --
 * 	Unmanage the specified content window.
 */
void Ttk_ForgetContent(Ttk_Manager *mgr, int index)
{
    Tk_Window window = mgr->content[index]->window;
    RemoveContent(mgr, index);
    Tk_ManageGeometry(window, NULL, 0);
}

/* ++ Ttk_PlaceContent --
 * 	Set the position and size of the specified content window.
 *
 * NOTES:
 * 	Contrary to documentation, Tk_MaintainGeometry doesn't always
 * 	map the content window.
 */
void Ttk_PlaceContent(
    Ttk_Manager *mgr, int index, int x, int y, int width, int height)
{
    Ttk_Content *content = mgr->content[index];
    Tk_MaintainGeometry(content->window,mgr->window,x,y,width,height);
    content->flags |= CONTENT_MAPPED;
    if (Tk_IsMapped(mgr->window)) {
	Tk_MapWindow(content->window);
    }
}

/* ++ Ttk_UnmapContent --
 * 	Unmap the specified content window, but leave it managed.
 */
void Ttk_UnmapContent(Ttk_Manager *mgr, int index)
{
    Ttk_Content *content = mgr->content[index];
    Tk_UnmaintainGeometry(content->window, mgr->window);
    content->flags &= ~CONTENT_MAPPED;
    /* Contrary to documentation, Tk_UnmaintainGeometry doesn't always
     * unmap the content window:
     */
    Tk_UnmapWindow(content->window);
}

/* LayoutChanged, SizeChanged --
 * 	Schedule a relayout, resp. resize request.
 */
void Ttk_ManagerLayoutChanged(Ttk_Manager *mgr)
{
    ScheduleUpdate(mgr, MGR_RELAYOUT_REQUIRED);
}

void Ttk_ManagerSizeChanged(Ttk_Manager *mgr)
{
    ScheduleUpdate(mgr, MGR_RESIZE_REQUIRED);
}

/* +++ Accessors.
 */
int Ttk_NumberContent(Ttk_Manager *mgr)
{
    return mgr->nContent;
}
void *Ttk_ContentData(Ttk_Manager *mgr, int index)
{
    return mgr->content[index]->data;
}
Tk_Window Ttk_ContentWindow(Ttk_Manager *mgr, int index)
{
    return mgr->content[index]->window;
}

/*------------------------------------------------------------------------
 * +++ Utility routines.
 */

/* ++ Ttk_ContentIndex --
 * 	Returns the index of specified content window, -1 if not found.
 */
int Ttk_ContentIndex(Ttk_Manager *mgr, Tk_Window window)
{
    int index;
    for (index = 0; index < mgr->nContent; ++index)
	if (mgr->content[index]->window == window)
	    return index;
    return -1;
}

/* ++ Ttk_GetContentIndexFromObj(interp, mgr, objPtr, indexPtr) --
 * 	Return the index of the content window specified by objPtr.
 * 	Content windows may be specified as an integer index or
 * 	as the name of the managed window.
 *
 * Returns:
 * 	Standard Tcl completion code.  Leaves an error message in case of error.
 */

int Ttk_GetContentIndexFromObj(
    Tcl_Interp *interp, Ttk_Manager *mgr, Tcl_Obj *objPtr, int *indexPtr)
{
    const char *string = Tcl_GetString(objPtr);
    int index = 0;
    Tk_Window tkwin;

    /* Try interpreting as an integer first:
     */
    if (Tcl_GetIntFromObj(NULL, objPtr, &index) == TCL_OK) {
	if (index < 0 || index >= mgr->nContent) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"Slave index %d out of bounds", index));
	    Tcl_SetErrorCode(interp, "TTK", "SLAVE", "INDEX", NULL);
	    return TCL_ERROR;
	}
	*indexPtr = index;
	return TCL_OK;
    }

    /* Try interpreting as a window name;
     */
    if ((*string == '.') &&
	    (tkwin = Tk_NameToWindow(interp, string, mgr->window))) {
	index = Ttk_ContentIndex(mgr, tkwin);
	if (index < 0) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "%s is not managed by %s", string,
		    Tk_PathName(mgr->window)));
	    Tcl_SetErrorCode(interp, "TTK", "SLAVE", "MANAGER", NULL);
	    return TCL_ERROR;
	}
	*indexPtr = index;
	return TCL_OK;
    }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "Invalid slave specification %s", string));
    Tcl_SetErrorCode(interp, "TTK", "SLAVE", "SPEC", NULL);
    return TCL_ERROR;
}

/* ++ Ttk_ReorderContent(mgr, fromIndex, toIndex) --
 * 	Change content window order.
 */
void Ttk_ReorderContent(Ttk_Manager *mgr, int fromIndex, int toIndex)
{
    Ttk_Content *moved = mgr->content[fromIndex];

    /* Shuffle down: */
    while (fromIndex > toIndex) {
	mgr->content[fromIndex] = mgr->content[fromIndex - 1];
	--fromIndex;
    }
    /* Or, shuffle up: */
    while (fromIndex < toIndex) {
	mgr->content[fromIndex] = mgr->content[fromIndex + 1];
	++fromIndex;
    }
    /* ASSERT: fromIndex == toIndex */
    mgr->content[fromIndex] = moved;

    /* Schedule a relayout.  In general, rearranging content
     * may also change the size:
     */
    ScheduleUpdate(mgr, MGR_RESIZE_REQUIRED);
}

/* ++ Ttk_Maintainable(interp, window, container) --
 * 	Utility routine.  Verifies that 'container' may be used to maintain
 *	the geometry of 'window' via Tk_MaintainGeometry:
 *
 * 	+ 'container' is either 'window's parent -OR-
 * 	+ 'container is a descendant of 'window's parent.
 * 	+ 'window' is not a toplevel window
 * 	+ 'window' belongs to the same toplevel as 'container'
 *
 * Returns: 1 if OK; otherwise 0, leaving an error message in 'interp'.
 */
int Ttk_Maintainable(Tcl_Interp *interp, Tk_Window window, Tk_Window container)
{
    Tk_Window ancestor = container, parent = Tk_Parent(window);

    if (Tk_IsTopLevel(window) || window == container) {
	goto badWindow;
    }

    while (ancestor != parent) {
	if (Tk_IsTopLevel(ancestor)) {
	    goto badWindow;
	}
	ancestor = Tk_Parent(ancestor);
    }

    return 1;

badWindow:
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("can't add %s as slave of %s",
	    Tk_PathName(window), Tk_PathName(container)));
    Tcl_SetErrorCode(interp, "TTK", "GEOMETRY", "MAINTAINABLE", NULL);
    return 0;
}

