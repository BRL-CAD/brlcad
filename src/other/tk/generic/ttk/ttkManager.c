/* $Id$
 *
 * Copyright 2005, Joe English.  Freely redistributable.
 *
 * Ttk widget set: support routines for geometry managers.
 */

#include <string.h>
#include <tk.h>
#include "ttkManager.h"

/*------------------------------------------------------------------------
 * +++ The Geometry Propagation Dance.
 *
 * When a slave window requests a new size or some other parameter changes,
 * the manager recomputes the required size for the master window and calls
 * Tk_GeometryRequest().  This is scheduled as an idle handler so multiple
 * updates can be processed as a single batch.
 *
 * If all goes well, the master's manager will process the request
 * (and so on up the chain to the toplevel window), and the master
 * window will eventually receive a <Configure> event.  At this point
 * it recomputes the size and position of all slaves and places them.
 *
 * If all does not go well, however, the master's request may be ignored
 * (typically because the top-level window has a fixed, user-specified size).
 * Tk doesn't provide any notification when this happens; to account for this,
 * we also schedule an idle handler to call the layout procedure
 * after making a geometry request.
 *
 * +++ Slave removal <<NOTE-LOSTSLAVE>>.
 *
 * There are three conditions under which a slave is removed:
 *
 * (1) Another GM claims control
 * (2) Manager voluntarily relinquishes control
 * (3) Slave is destroyed
 *
 * In case (1), Tk calls the manager's lostSlaveProc.
 * Case (2) is performed by calling Tk_ManageGeometry(slave,NULL,0);
 * in this case Tk does _not_ call the LostSlaveProc (documented behavior).
 * Tk doesn't handle case (3) either; to account for that we
 * register an event handler on the slave widget to track <Destroy> events.
 *
 */

/* ++ manager->flags bits:
 */
#define MGR_UPDATE_PENDING	0x1
#define MGR_RESIZE_REQUIRED	0x2
#define MGR_RELAYOUT_REQUIRED	0x4

/* ++ slave->flags bits:
 */
#define SLAVE_MAPPED 		0x1	/* slave to be mapped when master is */

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
 * 	Recomputes the required size of the master window,
 * 	makes geometry request.
 */
static void RecomputeSize(Ttk_Manager *mgr)
{
    int width = 1, height = 1;

    if (mgr->managerSpec->RequestedSize(mgr->managerData, &width, &height)) {
	Tk_GeometryRequest(mgr->masterWindow, width, height);
	ScheduleUpdate(mgr, MGR_RELAYOUT_REQUIRED);
    }
    mgr->flags &= ~MGR_RESIZE_REQUIRED;
}

/* ++ RecomputeLayout --
 * 	Recompute geometry of all slaves.
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
    Ttk_Manager *mgr = clientData;
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
 * 	Recompute slave layout when master widget is resized.
 * 	Keep the slave's map state in sync with the master's.
 */
static const int ManagerEventMask = StructureNotifyMask;
static void ManagerEventHandler(ClientData clientData, XEvent *eventPtr)
{
    Ttk_Manager *mgr = clientData;
    int i;

    switch (eventPtr->type) 
    {
	case ConfigureNotify:
	    RecomputeLayout(mgr);
	    break;
	case MapNotify:
	    for (i = 0; i < mgr->nSlaves; ++i) {
		Ttk_Slave *slave = mgr->slaves[i];
		if (slave->flags & SLAVE_MAPPED) {
		    Tk_MapWindow(slave->slaveWindow);
		}
	    }
	    break;
	case UnmapNotify:
	    for (i = 0; i < mgr->nSlaves; ++i) {
		Ttk_Slave *slave = mgr->slaves[i];
		Tk_UnmapWindow(slave->slaveWindow);
	    }
	    break;
    }
}

/* ++ SlaveEventHandler --
 * 	Notifies manager when a slave is destroyed
 * 	(see <<NOTE-LOSTSLAVE>>).
 */
static const unsigned SlaveEventMask = StructureNotifyMask;
static void SlaveEventHandler(ClientData clientData, XEvent *eventPtr)
{
    Ttk_Slave *slave = clientData;
    if (eventPtr->type == DestroyNotify) {
	slave->manager->managerSpec->tkGeomMgr.lostSlaveProc(
	    clientData, slave->slaveWindow);
    }
}

/*------------------------------------------------------------------------
 * +++ Slave initialization and cleanup.
 */

static Ttk_Slave *CreateSlave(
    Tcl_Interp *interp, Ttk_Manager *mgr, Tk_Window slaveWindow)
{
    Ttk_Slave *slave = (Ttk_Slave*)ckalloc(sizeof(*slave));
    int status;

    slave->slaveWindow = slaveWindow;
    slave->manager = mgr;
    slave->flags = 0;
    slave->slaveData = ckalloc(mgr->managerSpec->slaveSize);
    memset(slave->slaveData, 0, mgr->managerSpec->slaveSize);

    if (!mgr->slaveOptionTable) {
	mgr->slaveOptionTable =
	    Tk_CreateOptionTable(interp, mgr->managerSpec->slaveOptionSpecs);
    }

    status = Tk_InitOptions(
	interp, slave->slaveData, mgr->slaveOptionTable, slaveWindow);

    if (status != TCL_OK) {
	ckfree((ClientData)slave->slaveData);
	ckfree((ClientData)slave);
	return NULL;
    }

    return slave;
}

static void DeleteSlave(Ttk_Slave *slave)
{
    Tk_FreeConfigOptions(
	slave->slaveData, slave->manager->slaveOptionTable, slave->slaveWindow);
    ckfree((ClientData)slave->slaveData);
    ckfree((ClientData)slave);
}

/*------------------------------------------------------------------------
 * +++ Manager initialization and cleanup.
 */

Ttk_Manager *Ttk_CreateManager(
    Ttk_ManagerSpec *managerSpec, void *managerData, Tk_Window masterWindow)
{
    Ttk_Manager *mgr = (Ttk_Manager*)ckalloc(sizeof(*mgr));

    mgr->managerSpec 	= managerSpec;
    mgr->managerData	= managerData;
    mgr->masterWindow	= masterWindow;
    mgr->slaveOptionTable= 0;
    mgr->nSlaves 	= 0;
    mgr->slaves 	= NULL;
    mgr->flags  	= 0;

    Tk_CreateEventHandler(
	mgr->masterWindow, ManagerEventMask, ManagerEventHandler, mgr);

    return mgr;
}

void Ttk_DeleteManager(Ttk_Manager *mgr)
{
    Tk_DeleteEventHandler(
	mgr->masterWindow, ManagerEventMask, ManagerEventHandler, mgr);

    while (mgr->nSlaves > 0) {
	Ttk_ForgetSlave(mgr, mgr->nSlaves - 1);
    }
    if (mgr->slaves) {
	ckfree((ClientData)mgr->slaves);
    }
    if (mgr->slaveOptionTable) {
	Tk_DeleteOptionTable(mgr->slaveOptionTable);
    }

    Tk_CancelIdleCall(ManagerIdleProc, mgr);

    ckfree((ClientData)mgr);
}

/*------------------------------------------------------------------------
 * +++ Slave management.
 */

/* ++ InsertSlave --
 * 	Adds slave to the list of managed windows.
 */
static void InsertSlave(Ttk_Manager *mgr, Ttk_Slave *slave, int index)
{
    int endIndex = mgr->nSlaves++;
    mgr->slaves = (Ttk_Slave**)ckrealloc(
	    (ClientData)mgr->slaves, mgr->nSlaves * sizeof(Ttk_Slave *));

    while (endIndex > index) {
	mgr->slaves[endIndex] = mgr->slaves[endIndex - 1];
	--endIndex;
    }

    mgr->slaves[index] = slave;

    Tk_ManageGeometry(slave->slaveWindow,
	&mgr->managerSpec->tkGeomMgr, (ClientData)slave);

    Tk_CreateEventHandler(slave->slaveWindow,
	SlaveEventMask, SlaveEventHandler, (ClientData)slave);

    ScheduleUpdate(mgr, MGR_RESIZE_REQUIRED);
}

/* RemoveSlave --
 * 	Unmanage and delete the slave.
 *
 * NOTES/ASSUMPTIONS:
 *
 * [1] It's safe to call Tk_UnmapWindow / Tk_UnmaintainGeometry even if this 
 * routine is called from the slave's DestroyNotify event handler.
 */
static void RemoveSlave(Ttk_Manager *mgr, int index)
{
    Ttk_Slave *slave = mgr->slaves[index];
    int i;

    /* Notify manager:
     */
    mgr->managerSpec->SlaveRemoved(mgr, index);

    /* Remove from array:
     */
    --mgr->nSlaves;
    for (i = index ; i < mgr->nSlaves; ++i) {
	mgr->slaves[i] = mgr->slaves[i+1];
    }

    /* Clean up:
     */
    Tk_DeleteEventHandler(
	slave->slaveWindow, SlaveEventMask, SlaveEventHandler, slave);

    /* Note [1] */
    Tk_UnmaintainGeometry(slave->slaveWindow, mgr->masterWindow);
    Tk_UnmapWindow(slave->slaveWindow);

    DeleteSlave(slave);

    ScheduleUpdate(mgr, MGR_RESIZE_REQUIRED);
}

/*------------------------------------------------------------------------
 * +++ Tk_GeomMgr hooks.
 */

void Ttk_GeometryRequestProc(ClientData clientData, Tk_Window slaveWindow)
{
    Ttk_Slave *slave = clientData;
    ScheduleUpdate(slave->manager, MGR_RESIZE_REQUIRED);
}

void Ttk_LostSlaveProc(ClientData clientData, Tk_Window slaveWindow)
{
    Ttk_Slave *slave = clientData;
    int index = Ttk_SlaveIndex(slave->manager, slave->slaveWindow);

    /* ASSERT: index >= 0 */
    RemoveSlave(slave->manager, index);
}

/*------------------------------------------------------------------------
 * +++ Public API.
 */

/* ++ Ttk_AddSlave --
 * 	Create and configure new slave window, insert at specified index.
 *
 * Returns:
 * 	TCL_OK or TCL_ERROR; in the case of TCL_ERROR, the slave
 * 	is not added and an error message is left in interp.
 */
int Ttk_AddSlave(
    Tcl_Interp *interp, Ttk_Manager *mgr, Tk_Window slaveWindow,
    int index, int objc, Tcl_Obj *CONST objv[])
{
    Ttk_Slave *slave;

    /* Sanity-checks:
     */
    if (!Ttk_Maintainable(interp, slaveWindow, mgr->masterWindow)) {
	return TCL_ERROR;
    }
    if (Ttk_SlaveIndex(mgr, slaveWindow) >= 0) {
	Tcl_AppendResult(interp,
	    Tk_PathName(slaveWindow), " already added",
	    NULL);
	return TCL_ERROR;
    }

    /* Create, configure, and insert slave:
     */
    slave = CreateSlave(interp, mgr, slaveWindow);
    if (Ttk_ConfigureSlave(interp, mgr, slave, objc, objv) != TCL_OK) {
	DeleteSlave(slave);
	return TCL_ERROR;
    }
    InsertSlave(mgr, slave, index);
    mgr->managerSpec->SlaveAdded(mgr, index);
    return TCL_OK;
}

/* ++ Ttk_ConfigureSlave --
 */
int Ttk_ConfigureSlave(
    Tcl_Interp *interp, Ttk_Manager *mgr, Ttk_Slave *slave,
    int objc, Tcl_Obj *CONST objv[])
{
    Tk_SavedOptions savedOptions;
    int mask = 0;

    /* ASSERT: mgr->slaveOptionTable != NULL */

    if (Tk_SetOptions(interp, slave->slaveData, mgr->slaveOptionTable,
	    objc, objv, slave->slaveWindow, &savedOptions, &mask) != TCL_OK)
    {
	return TCL_ERROR;
    }

    if (mgr->managerSpec->SlaveConfigured(interp,mgr,slave,mask) != TCL_OK) {
	Tk_RestoreSavedOptions(&savedOptions);
	return TCL_ERROR;
    }

    Tk_FreeSavedOptions(&savedOptions);
    ScheduleUpdate(mgr, MGR_RELAYOUT_REQUIRED);
    return TCL_OK;
}

/* ++ Ttk_ForgetSlave --
 * 	Unmanage the specified slave.
 */
void Ttk_ForgetSlave(Ttk_Manager *mgr, int slaveIndex)
{
    Tk_Window slaveWindow = mgr->slaves[slaveIndex]->slaveWindow;
    RemoveSlave(mgr, slaveIndex);
    Tk_ManageGeometry(slaveWindow, NULL, 0);
}

/* ++ Ttk_PlaceSlave --
 * 	Set the position and size of the specified slave window.
 *
 * NOTES:
 * 	Contrary to documentation, Tk_MaintainGeometry doesn't always
 * 	map the slave.
 */
void Ttk_PlaceSlave(
    Ttk_Manager *mgr, int slaveIndex, int x, int y, int width, int height)
{
    Ttk_Slave *slave = mgr->slaves[slaveIndex];
    Tk_MaintainGeometry(slave->slaveWindow,mgr->masterWindow,x,y,width,height);
    slave->flags |= SLAVE_MAPPED;
    if (Tk_IsMapped(mgr->masterWindow)) {
	Tk_MapWindow(slave->slaveWindow);
    }
}

/* ++ Ttk_UnmapSlave --
 * 	Unmap the specified slave, but leave it managed.
 */
void Ttk_UnmapSlave(Ttk_Manager *mgr, int slaveIndex)
{
    Ttk_Slave *slave = mgr->slaves[slaveIndex];
    Tk_UnmaintainGeometry(slave->slaveWindow, mgr->masterWindow);
    slave->flags &= ~SLAVE_MAPPED;
    /* Contrary to documentation, Tk_UnmaintainGeometry doesn't always 
     * unmap the slave:
     */
    Tk_UnmapWindow(slave->slaveWindow);
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
int Ttk_NumberSlaves(Ttk_Manager *mgr) 
{
    return mgr->nSlaves; 
}
void *Ttk_SlaveData(Ttk_Manager *mgr, int slaveIndex)
{
    return mgr->slaves[slaveIndex]->slaveData;
}
Tk_Window Ttk_SlaveWindow(Ttk_Manager *mgr, int slaveIndex)
{
    return mgr->slaves[slaveIndex]->slaveWindow;
}

/*------------------------------------------------------------------------
 * +++ Utility routines.
 */

/* ++ Ttk_SlaveIndex --
 * 	Returns the index of specified slave window, -1 if not found.
 */
int Ttk_SlaveIndex(Ttk_Manager *mgr, Tk_Window slaveWindow)
{
    int index;
    for (index = 0; index < mgr->nSlaves; ++index)
	if (mgr->slaves[index]->slaveWindow == slaveWindow)
	    return index;
    return -1;
}

/* ++ Ttk_GetSlaveFromObj(interp, mgr, objPtr, indexPtr) --
 * 	Return the index of the slave specified by objPtr.
 * 	Slaves may be specified as an integer index or
 * 	as the name of the managed window.
 *
 * Returns:
 * 	Pointer to slave; stores slave index in *indexPtr.
 * 	On error, returns NULL and leaves an error message in interp.
 */

Ttk_Slave *Ttk_GetSlaveFromObj(
    Tcl_Interp *interp, Ttk_Manager *mgr, Tcl_Obj *objPtr, int *indexPtr)
{
    const char *string = Tcl_GetString(objPtr);
    int slaveIndex = 0;
    Tk_Window tkwin;

    /* Try interpreting as an integer first:
     */
    if (Tcl_GetIntFromObj(NULL, objPtr, &slaveIndex) == TCL_OK) {
	if (slaveIndex < 0 || slaveIndex >= mgr->nSlaves) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp,
		"Slave index ", Tcl_GetString(objPtr), " out of bounds",
		NULL);
	    return NULL;
	}
	*indexPtr = slaveIndex;
	return mgr->slaves[slaveIndex];
    }

    /* Try interpreting as a slave window name;
     */
    if (   (*string == '.')
	&& (tkwin = Tk_NameToWindow(interp, string, mgr->masterWindow)))
    {
	slaveIndex = Ttk_SlaveIndex(mgr, tkwin);
	if (slaveIndex < 0) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp,
		string, " is not managed by ", Tk_PathName(mgr->masterWindow),
		NULL);
	    return NULL;
	}
	*indexPtr = slaveIndex;
	return mgr->slaves[slaveIndex];
    }

    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "Invalid slave specification ", string, NULL);
    return NULL;
}

/* ++ Ttk_ReorderSlave(mgr, fromIndex, toIndex) --
 * 	Change slave order.
 */
void Ttk_ReorderSlave(Ttk_Manager *mgr, int fromIndex, int toIndex)
{
    Ttk_Slave *moved = mgr->slaves[fromIndex];

    /* Shuffle down: */
    while (fromIndex > toIndex) {
	mgr->slaves[fromIndex] = mgr->slaves[fromIndex - 1];
	--fromIndex;
    }
    /* Or, shuffle up: */
    while (fromIndex < toIndex) {
	mgr->slaves[fromIndex] = mgr->slaves[fromIndex + 1];
	++fromIndex;
    }
    /* ASSERT: fromIndex == toIndex */
    mgr->slaves[fromIndex] = moved;

    /* Schedule a relayout.  In general, rearranging slaves
     * may also change the size:
     */
    ScheduleUpdate(mgr, MGR_RESIZE_REQUIRED);
}

/* ++ Ttk_Maintainable(interp, slave, master) --
 * 	Utility routine.  Verifies that 'master' may be used to maintain
 *	the geometry of 'slave' via Tk_MaintainGeometry:
 * 
 * 	+ 'master' is either 'slave's parent -OR-
 * 	+ 'master is a descendant of 'slave's parent.
 * 	+ 'slave' is not a toplevel window
 * 	+ 'slave' belongs to the same toplevel as 'master'
 *
 * Returns: 1 if OK; otherwise 0, leaving an error message in 'interp'.
 */
int Ttk_Maintainable(Tcl_Interp *interp, Tk_Window slave, Tk_Window master)
{
    Tk_Window ancestor = master, parent = Tk_Parent(slave), sibling = NULL;

    if (Tk_IsTopLevel(slave) || slave == master) {
	goto badWindow;
    }

    while (ancestor != parent) {
	if (Tk_IsTopLevel(ancestor)) {
	    goto badWindow;
	}
	sibling = ancestor;
	ancestor = Tk_Parent(ancestor);
    }

    return 1;

badWindow:
    Tcl_AppendResult(interp, 
	"can't add ", Tk_PathName(slave),
	" as slave of ", Tk_PathName(master),
	NULL);
    return 0;
}

