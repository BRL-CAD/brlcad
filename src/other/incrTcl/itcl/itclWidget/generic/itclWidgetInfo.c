/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tcl]
 *  DESCRIPTION:  Object-Oriented Extensions to Tcl
 *
 *  These procedures handle the "info" method for package ItclWidget
 *
 * ========================================================================
 *  Author: Arnulf Wiedemann
 *
 *     RCS:  $Id$
 * ========================================================================
 *           Copyright (c) 2007 Arnulf Wiedemann
 * ------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#include "itclWidgetInt.h"

Tcl_ObjCmdProc ItclBiInfoHullTypesCmd;
Tcl_ObjCmdProc ItclBiInfoWidgetClassesCmd;
Tcl_ObjCmdProc ItclBiInfoWidgetsCmd;
Tcl_ObjCmdProc ItclBiInfoWidgetAdaptorsCmd;

static const char *hullTypes[] = {
    "frame",
    "toplevel",
    "labelframe",
    "ttk:frame",
    "ttk:toplevel",
    "ttk:labelframe",
    NULL
};

struct NameProcMap { const char *name; Tcl_ObjCmdProc *proc; };

/*
 * List of commands that are used to implement the [info object] subcommands.
 */

static const struct NameProcMap infoCmds2[] = {
    { "::itcl::builtin::Info::hulltypes", ItclBiInfoHullTypesCmd },
    { "::itcl::builtin::Info::widgetclasses", ItclBiInfoWidgetClassesCmd },
    { "::itcl::builtin::Info::widgets", ItclBiInfoWidgetsCmd },
    { "::itcl::builtin::Info::widgetadaptors", ItclBiInfoWidgetAdaptorsCmd },
    { NULL, NULL }
};


/*
 * ------------------------------------------------------------------------
 *  ItclWidgetInfoInit()
 *
 *  Creates a namespace full of built-in methods/procs for [incr Tcl]
 *  classes.  This includes things like the "info"
 *  for querying class info.  Usually invoked by Itcl_Init() when
 *  [incr Tcl] is first installed into an interpreter.
 *
 *  Returns TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
int
ItclWidgetInfoInit(
    Tcl_Interp *interp,      /* current interpreter */
    ItclObjectInfo *infoPtr)
{
    int i;

    for (i=0 ; infoCmds2[i].name!=NULL ; i++) {
        Tcl_CreateObjCommand(interp, infoCmds2[i].name,
                infoCmds2[i].proc, infoPtr, NULL);
    }
    return TCL_OK;
}

int
ItclBiInfoHullTypesCmd(
    ClientData clientData,   /* info for all known objects */
    Tcl_Interp *interp,      /* current interpreter */
    int objc,                /* number of arguments */
    Tcl_Obj *const objv[])   /* argument objects */
{
    Tcl_Obj *listPtr;
    Tcl_Obj *objPtr;
    ItclObjectInfo *infoPtr;
    ItclClass *iclsPtr;
    const char **cPtrPtr;
    const char *name;
    const char *pattern;

    ItclShowArgs(0, "ItclBiInfoHullTypesCmd", objc, objv);
    infoPtr = (ItclObjectInfo *)clientData;
    iclsPtr = NULL;
    pattern = NULL;
    if (objc > 2) {
	Tcl_AppendResult(interp, "wrong # args should be: info hulltypes ",
	        "?pattern?", NULL);
        return TCL_ERROR;
    }
    if (objc == 2) {
        pattern = Tcl_GetString(objv[1]);
    }
    listPtr = Tcl_NewListObj(0, NULL);
    cPtrPtr = hullTypes;
    while (*cPtrPtr != NULL) {
	name = *cPtrPtr;
        objPtr = Tcl_NewStringObj(name, -1);
        if ((pattern == NULL) ||
                 Tcl_StringMatch(name, pattern)) {
            Tcl_ListObjAppendElement(interp, listPtr, objPtr);
        }
        cPtrPtr++;
    }
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

int
ItclBiInfoWidgetClassesCmd(
    ClientData clientData,   /* info for all known objects */
    Tcl_Interp *interp,      /* current interpreter */
    int objc,                /* number of arguments */
    Tcl_Obj *const objv[])   /* argument objects */
{
    FOREACH_HASH_DECLS;
    Tcl_HashEntry *hPtr2;
    Tcl_Obj *listPtr;
    Tcl_HashTable  wClasses;
    ItclObjectInfo *infoPtr;
    ItclClass *iclsPtr;
    const char *name;
    const char *pattern;
    int isNew;

    ItclShowArgs(1, "ItclBiInfoWidgetClassesCmd", objc, objv);
    infoPtr = (ItclObjectInfo *)clientData;
    iclsPtr = NULL;
    pattern = NULL;
    if (objc > 2) {
	Tcl_AppendResult(interp, "wrong # args should be: info widgetclasses ",
	        "?pattern?", NULL);
        return TCL_ERROR;
    }

    if (objc == 2) {
        pattern = Tcl_GetString(objv[1]);
    }
    Tcl_InitObjHashTable(&wClasses);
    listPtr = Tcl_NewListObj(0, NULL);
    FOREACH_HASH_VALUE(iclsPtr, &infoPtr->classes) {
	if (iclsPtr->flags & ITCL_WIDGET) {
	    if (iclsPtr->widgetClassPtr != NULL) {
	        hPtr2 = Tcl_CreateHashEntry(&wClasses,
	                (char *)iclsPtr->widgetClassPtr, &isNew);
	        if (isNew) {
	            name = Tcl_GetString(iclsPtr->widgetClassPtr);
	            if ((pattern == NULL) ||
                             Tcl_StringMatch(name, pattern)) {
                        Tcl_ListObjAppendElement(interp, listPtr,
		                iclsPtr->widgetClassPtr);
	            }
	        }
            }
        }
    }
    Tcl_DeleteHashTable(&wClasses);
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

int
ItclBiInfoWidgetsCmd(
    ClientData clientData,   /* info for all known objects */
    Tcl_Interp *interp,      /* current interpreter */
    int objc,                /* number of arguments */
    Tcl_Obj *const objv[])   /* argument objects */
{
    FOREACH_HASH_DECLS;
    Tcl_Obj *listPtr;
    ItclObjectInfo *infoPtr;
    ItclClass *iclsPtr;
    const char *name;
    const char *pattern;

    ItclShowArgs(1, "ItclBiInfoWidgetsCmd", objc, objv);
    infoPtr = (ItclObjectInfo *)clientData;
    iclsPtr = NULL;
    pattern = NULL;
    if (objc > 2) {
	Tcl_AppendResult(interp, "wrong # args should be: info widgets ",
	        "?pattern?", NULL);
        return TCL_ERROR;
    }
    if (objc == 2) {
        pattern = Tcl_GetString(objv[1]);
    }
    listPtr = Tcl_NewListObj(0, NULL);
    FOREACH_HASH_VALUE(iclsPtr, &infoPtr->classes) {
	if (iclsPtr->flags & ITCL_WIDGET) {
	    name = Tcl_GetString(iclsPtr->namePtr);
	    Tcl_IncrRefCount(iclsPtr->namePtr);
	    if ((pattern == NULL) ||
                     Tcl_StringMatch(name, pattern)) {
                Tcl_ListObjAppendElement(interp, listPtr, iclsPtr->namePtr);
            }
        }
    }
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

int
ItclBiInfoWidgetAdaptorsCmd(
    ClientData clientData,   /* info for all known objects */
    Tcl_Interp *interp,      /* current interpreter */
    int objc,                /* number of arguments */
    Tcl_Obj *const objv[])   /* argument objects */
{
    FOREACH_HASH_DECLS;
    Tcl_Obj *listPtr;
    ItclObjectInfo *infoPtr;
    ItclClass *iclsPtr;
    const char *name;
    const char *pattern;

    ItclShowArgs(1, "ItclBiInfoWidgetadaptorsCmd", objc, objv);
    infoPtr = (ItclObjectInfo *)clientData;
    iclsPtr = NULL;
    pattern = NULL;
    if (objc > 2) {
	Tcl_AppendResult(interp, "wrong # args should be: info widgetadaptors ",
	        "?pattern?", NULL);
        return TCL_ERROR;
    }
    if (objc == 2) {
        pattern = Tcl_GetString(objv[1]);
    }
    listPtr = Tcl_NewListObj(0, NULL);
    FOREACH_HASH_VALUE(iclsPtr, &infoPtr->classes) {
	if (iclsPtr->flags & ITCL_WIDGETADAPTOR) {
	    name = Tcl_GetString(iclsPtr->namePtr);
	    if ((pattern == NULL) ||
                     Tcl_StringMatch(name, pattern)) {
                Tcl_ListObjAppendElement(interp, listPtr, iclsPtr->namePtr);
            }
        }
    }
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}
