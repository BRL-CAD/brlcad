/* 
 * qebind.h --
 *
 *	This module is the header for quasi-events.
 *
 * Copyright (c) 2002-2010 Tim Baker
 *
 * RCS: @(#) $Id$
 */

#ifndef INCLUDED_QEBIND_H
#define INCLUDED_QEBIND_H

/*
 * Used to tag functions that are only to be visible within the module being
 * built and not outside it (where this is supported by the linker).
 */

#ifndef MODULE_SCOPE
#   ifdef __cplusplus
#	define MODULE_SCOPE extern "C"
#   else
#	define MODULE_SCOPE extern
#   endif
#endif

typedef struct QE_BindingTable_ *QE_BindingTable;

/* Pass to QE_BindEvent */
typedef struct QE_Event {
	int type;
	int detail;
	ClientData clientData;
} QE_Event;

typedef struct QE_ExpandArgs {
	QE_BindingTable bindingTable;
	char which;
	ClientData object;
	Tcl_DString *result;
	int event;
	int detail;
	ClientData clientData;
} QE_ExpandArgs;

typedef void (*QE_ExpandProc)(QE_ExpandArgs *args);

MODULE_SCOPE int debug_bindings;

MODULE_SCOPE int QE_BindInit(Tcl_Interp *interp);
MODULE_SCOPE QE_BindingTable QE_CreateBindingTable(Tcl_Interp *interp);
MODULE_SCOPE void QE_DeleteBindingTable(QE_BindingTable bindingTable);
MODULE_SCOPE int QE_InstallEvent(QE_BindingTable bindingTable, char *name, QE_ExpandProc expand);
MODULE_SCOPE int QE_InstallDetail(QE_BindingTable bindingTable, char *name, int eventType, QE_ExpandProc expand);
MODULE_SCOPE int QE_UninstallEvent(QE_BindingTable bindingTable, int eventType);
MODULE_SCOPE int QE_UninstallDetail(QE_BindingTable bindingTable, int eventType, int detail);
MODULE_SCOPE int QE_CreateBinding(QE_BindingTable bindingTable,
	ClientData object, char *eventString, char *command, int append);
MODULE_SCOPE int QE_DeleteBinding(QE_BindingTable bindingTable,
	ClientData object, char *eventString);
MODULE_SCOPE int QE_GetAllObjects(QE_BindingTable bindingTable);
MODULE_SCOPE int QE_GetBinding(QE_BindingTable bindingTable,
	ClientData object, char *eventString);
MODULE_SCOPE int QE_GetAllBindings(QE_BindingTable bindingTable,
	ClientData object);
MODULE_SCOPE int QE_GetEventNames(QE_BindingTable bindingTable);
MODULE_SCOPE int QE_GetDetailNames(QE_BindingTable bindingTable, char *eventName);
MODULE_SCOPE int QE_BindEvent(QE_BindingTable bindingTable, QE_Event *eventPtr);
MODULE_SCOPE void QE_ExpandDouble(double number, Tcl_DString *result);
MODULE_SCOPE void QE_ExpandNumber(long number, Tcl_DString *result);
MODULE_SCOPE void QE_ExpandString(char *string, Tcl_DString *result);
MODULE_SCOPE void QE_ExpandEvent(QE_BindingTable bindingTable, int eventType, Tcl_DString *result);
MODULE_SCOPE void QE_ExpandDetail(QE_BindingTable bindingTable, int event, int detail, Tcl_DString *result);
MODULE_SCOPE void QE_ExpandPattern(QE_BindingTable bindingTable, int eventType, int detail, Tcl_DString *result);
MODULE_SCOPE void QE_ExpandUnknown(char which, Tcl_DString *result);
MODULE_SCOPE int QE_BindCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);
MODULE_SCOPE int QE_ConfigureCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);
MODULE_SCOPE int QE_GenerateCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);
MODULE_SCOPE int QE_InstallCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);
MODULE_SCOPE int QE_UnbindCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);
MODULE_SCOPE int QE_UninstallCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);
MODULE_SCOPE int QE_LinkageCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);

#endif /* INCLUDED_QEBIND_H */

