/*
 * Copyright (c) 2005, Joe English.  Freely redistributable.
 *
 * Geometry manager utilities.
 */

#ifndef _TTKMANAGER
#define _TTKMANAGER

#include "ttkTheme.h"

typedef struct TtkManager_ Ttk_Manager;

/*
 * Geometry manager specification record:
 *
 * RequestedSize computes the requested size of the container window.
 *
 * PlaceSlaves sets the position and size of all managed content windows
 * by calling Ttk_PlaceContent().
 *
 * SlaveRemoved() is called immediately before a content window is removed.
 * NB: the associated content window may have been destroyed when this
 * routine is called.
 *
 * SlaveRequest() is called when a content window requests a size change.
 * It should return 1 if the request should propagate, 0 otherwise.
 */
typedef struct {			/* Manager hooks */
    Tk_GeomMgr tkGeomMgr;		/* "real" Tk Geometry Manager */

    int  (*RequestedSize)(void *managerData, int *widthPtr, int *heightPtr);
    void (*PlaceSlaves)(void *managerData);
    int  (*SlaveRequest)(void *managerData, int index, int w, int h);
    void (*SlaveRemoved)(void *managerData, int index);
} Ttk_ManagerSpec;

/*
 * Default implementations for Tk_GeomMgr hooks:
 */
#define Ttk_LostContentProc Ttk_LostSlaveProc
MODULE_SCOPE void Ttk_GeometryRequestProc(ClientData, Tk_Window window);
MODULE_SCOPE void Ttk_LostContentProc(ClientData, Tk_Window window);

/*
 * Public API:
 */
MODULE_SCOPE Ttk_Manager *Ttk_CreateManager(
	Ttk_ManagerSpec *, void *managerData, Tk_Window window);
MODULE_SCOPE void Ttk_DeleteManager(Ttk_Manager *);

#define Ttk_InsertContent  Ttk_InsertSlave
MODULE_SCOPE void Ttk_InsertContent(
    Ttk_Manager *, int position, Tk_Window, void *data);

#define Ttk_ForgetContent Ttk_ForgetSlave
MODULE_SCOPE void Ttk_ForgetContent(Ttk_Manager *, int index);

#define Ttk_ReorderContent Ttk_ReorderSlave
MODULE_SCOPE void Ttk_ReorderContent(Ttk_Manager *, int fromIndex, int toIndex);
    /* Rearrange content window positions */

#define Ttk_PlaceContent Ttk_PlaceSlave
MODULE_SCOPE void Ttk_PlaceContent(
    Ttk_Manager *, int index, int x, int y, int width, int height);
    /* Position and map the content window */

#define Ttk_UnmapContent Ttk_UnmapSlave
MODULE_SCOPE void Ttk_UnmapContent(Ttk_Manager *, int index);
    /* Unmap the content window */

MODULE_SCOPE void Ttk_ManagerSizeChanged(Ttk_Manager *);
MODULE_SCOPE void Ttk_ManagerLayoutChanged(Ttk_Manager *);
    /* Notify manager that size (resp. layout) needs to be recomputed */

/* Utilities:
 */
#define Ttk_ContentIndex Ttk_SlaveIndex
MODULE_SCOPE int Ttk_ContentIndex(Ttk_Manager *, Tk_Window);
    /* Returns: index in content array of specified window, -1 if not found */

#define Ttk_GetContentIndexFromObj Ttk_GetSlaveIndexFromObj
MODULE_SCOPE int Ttk_GetContentIndexFromObj(
    Tcl_Interp *, Ttk_Manager *, Tcl_Obj *, int *indexPtr);

/* Accessor functions:
 */
#define Ttk_NumberContent Ttk_NumberSlaves
MODULE_SCOPE int Ttk_NumberContent(Ttk_Manager *);
    /* Returns: number of managed content windows */

#define Ttk_ContentData Ttk_SlaveData
MODULE_SCOPE void *Ttk_ContentData(Ttk_Manager *, int index);
    /* Returns: client data associated with content window */

#define Ttk_ContentWindow Ttk_SlaveWindow
MODULE_SCOPE Tk_Window Ttk_ContentWindow(Ttk_Manager *, int index);
    /* Returns: content window */

MODULE_SCOPE int Ttk_Maintainable(Tcl_Interp *, Tk_Window content, Tk_Window container);
    /* Returns: 1 if container can manage content; 0 otherwise leaving error msg */

#endif /* _TTKMANAGER */
