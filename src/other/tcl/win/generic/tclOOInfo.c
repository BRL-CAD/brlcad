/*
 * tclOODefineCmds.c --
 *
 *	This file contains the implementation of the ::oo-related [info]
 *	subcommands.
 *
 * Copyright (c) 2006-2008 by Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "tclInt.h"
#include "tclOOInt.h"

static inline Class *	GetClassFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr);
static Tcl_ObjCmdProc InfoObjectClassCmd;
static Tcl_ObjCmdProc InfoObjectDefnCmd;
static Tcl_ObjCmdProc InfoObjectFiltersCmd;
static Tcl_ObjCmdProc InfoObjectForwardCmd;
static Tcl_ObjCmdProc InfoObjectIsACmd;
static Tcl_ObjCmdProc InfoObjectMethodsCmd;
static Tcl_ObjCmdProc InfoObjectMixinsCmd;
static Tcl_ObjCmdProc InfoObjectNsCmd;
static Tcl_ObjCmdProc InfoObjectVarsCmd;
static Tcl_ObjCmdProc InfoObjectVariablesCmd;
static Tcl_ObjCmdProc InfoClassConstrCmd;
static Tcl_ObjCmdProc InfoClassDefnCmd;
static Tcl_ObjCmdProc InfoClassDestrCmd;
static Tcl_ObjCmdProc InfoClassFiltersCmd;
static Tcl_ObjCmdProc InfoClassForwardCmd;
static Tcl_ObjCmdProc InfoClassInstancesCmd;
static Tcl_ObjCmdProc InfoClassMethodsCmd;
static Tcl_ObjCmdProc InfoClassMixinsCmd;
static Tcl_ObjCmdProc InfoClassSubsCmd;
static Tcl_ObjCmdProc InfoClassSupersCmd;
static Tcl_ObjCmdProc InfoClassVariablesCmd;

struct NameProcMap { const char *name; Tcl_ObjCmdProc *proc; };

/*
 * List of commands that are used to implement the [info object] subcommands.
 */

static const struct NameProcMap infoObjectCmds[] = {
    {"::oo::InfoObject::class",		InfoObjectClassCmd},
    {"::oo::InfoObject::definition",	InfoObjectDefnCmd},
    {"::oo::InfoObject::filters",	InfoObjectFiltersCmd},
    {"::oo::InfoObject::forward",	InfoObjectForwardCmd},
    {"::oo::InfoObject::isa",		InfoObjectIsACmd},
    {"::oo::InfoObject::methods",	InfoObjectMethodsCmd},
    {"::oo::InfoObject::mixins",	InfoObjectMixinsCmd},
    {"::oo::InfoObject::namespace",	InfoObjectNsCmd},
    {"::oo::InfoObject::variables",	InfoObjectVariablesCmd},
    {"::oo::InfoObject::vars",		InfoObjectVarsCmd},
    {NULL, NULL}
};

/*
 * List of commands that are used to implement the [info class] subcommands.
 */

static const struct NameProcMap infoClassCmds[] = {
    {"::oo::InfoClass::constructor",	InfoClassConstrCmd},
    {"::oo::InfoClass::definition",	InfoClassDefnCmd},
    {"::oo::InfoClass::destructor",	InfoClassDestrCmd},
    {"::oo::InfoClass::filters",	InfoClassFiltersCmd},
    {"::oo::InfoClass::forward",	InfoClassForwardCmd},
    {"::oo::InfoClass::instances",	InfoClassInstancesCmd},
    {"::oo::InfoClass::methods",	InfoClassMethodsCmd},
    {"::oo::InfoClass::mixins",		InfoClassMixinsCmd},
    {"::oo::InfoClass::subclasses",	InfoClassSubsCmd},
    {"::oo::InfoClass::superclasses",	InfoClassSupersCmd},
    {"::oo::InfoClass::variables",	InfoClassVariablesCmd},
    {NULL, NULL}
};

/*
 * ----------------------------------------------------------------------
 *
 * TclOOInitInfo --
 *
 *	Adjusts the Tcl core [info] command to contain subcommands ("object"
 *	and "class") for introspection of objects and classes.
 *
 * ----------------------------------------------------------------------
 */

void
TclOOInitInfo(
    Tcl_Interp *interp)
{
    Tcl_Namespace *nsPtr;
    Tcl_Command infoCmd;
    int i;

    /*
     * Build the ensemble used to implement [info object].
     */

    nsPtr = Tcl_CreateNamespace(interp, "::oo::InfoObject", NULL, NULL);
    Tcl_CreateEnsemble(interp, nsPtr->fullName, nsPtr, TCL_ENSEMBLE_PREFIX);
    Tcl_Export(interp, nsPtr, "[a-z]*", 1);
    for (i=0 ; infoObjectCmds[i].name!=NULL ; i++) {
	Tcl_CreateObjCommand(interp, infoObjectCmds[i].name,
		infoObjectCmds[i].proc, NULL, NULL);
    }

    /*
     * Build the ensemble used to implement [info class].
     */

    nsPtr = Tcl_CreateNamespace(interp, "::oo::InfoClass", NULL, NULL);
    Tcl_CreateEnsemble(interp, nsPtr->fullName, nsPtr, TCL_ENSEMBLE_PREFIX);
    Tcl_Export(interp, nsPtr, "[a-z]*", 1);
    for (i=0 ; infoClassCmds[i].name!=NULL ; i++) {
	Tcl_CreateObjCommand(interp, infoClassCmds[i].name,
		infoClassCmds[i].proc, NULL, NULL);
    }

    /*
     * Install into the master [info] ensemble.
     */

    infoCmd = Tcl_FindCommand(interp, "info", NULL, TCL_GLOBAL_ONLY);
    if (infoCmd != NULL && Tcl_IsEnsemble(infoCmd)) {
	Tcl_Obj *mapDict, *objectObj, *classObj;

	Tcl_GetEnsembleMappingDict(NULL, infoCmd, &mapDict);
	if (mapDict != NULL) {
	    objectObj = Tcl_NewStringObj("object", -1);
	    classObj = Tcl_NewStringObj("class", -1);

	    Tcl_IncrRefCount(objectObj);
	    Tcl_IncrRefCount(classObj);
	    Tcl_DictObjPut(NULL, mapDict, objectObj,
		    Tcl_NewStringObj("::oo::InfoObject", -1));
	    Tcl_DictObjPut(NULL, mapDict, classObj,
		    Tcl_NewStringObj("::oo::InfoClass", -1));
	    Tcl_DecrRefCount(objectObj);
	    Tcl_DecrRefCount(classObj);
	    Tcl_SetEnsembleMappingDict(interp, infoCmd, mapDict);
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * GetClassFromObj --
 *
 *	How to correctly get a class from a Tcl_Obj. Just a wrapper round
 *	Tcl_GetObjectFromObj, but this is an idiom that was used heavily.
 *
 * ----------------------------------------------------------------------
 */

static inline Class *
GetClassFromObj(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr)
{
    Object *oPtr = (Object *) Tcl_GetObjectFromObj(interp, objPtr);

    if (oPtr == NULL) {
	return NULL;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "\"", TclGetString(objPtr),
		"\" is not a class", NULL);
	return NULL;
    }
    return oPtr->classPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectClassCmd --
 *
 *	Implements [info object class $objName ?$className?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectClassCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName ?className?");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    if (objc == 2) {
	Tcl_SetObjResult(interp,
		TclOOObjectName(interp, oPtr->selfCls->thisPtr));
	return TCL_OK;
    } else {
	Object *o2Ptr;
	Class *mixinPtr;
	int i;

	o2Ptr = (Object *) Tcl_GetObjectFromObj(interp, objv[2]);
	if (o2Ptr == NULL) {
	    return TCL_ERROR;
	}
	if (o2Ptr->classPtr == NULL) {
	    Tcl_AppendResult(interp, "object \"", TclGetString(objv[2]),
		    "\" is not a class", NULL);
	    return TCL_ERROR;
	}

	FOREACH(mixinPtr, oPtr->mixins) {
	    if (TclOOIsReachable(o2Ptr->classPtr, mixinPtr)) {
		Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
		return TCL_OK;
	    }
	}
	Tcl_SetObjResult(interp, Tcl_NewIntObj(
		TclOOIsReachable(o2Ptr->classPtr, oPtr->selfCls)));
	return TCL_OK;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectDefnCmd --
 *
 *	Implements [info object definition $objName $methodName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectDefnCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Tcl_HashEntry *hPtr;
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *resultObjs[2];

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName methodName");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    if (!oPtr->methodsPtr) {
	goto unknownMethod;
    }
    hPtr = Tcl_FindHashEntry(oPtr->methodsPtr, (char *) objv[2]);
    if (hPtr == NULL) {
    unknownMethod:
	Tcl_AppendResult(interp, "unknown method \"", TclGetString(objv[2]),
		"\"", NULL);
	return TCL_ERROR;
    }
    procPtr = TclOOGetProcFromMethod(Tcl_GetHashValue(hPtr));
    if (procPtr == NULL) {
	Tcl_AppendResult(interp,
		"definition not available for this kind of method", NULL);
	return TCL_ERROR;
    }

    resultObjs[0] = Tcl_NewObj();
    for (localPtr=procPtr->firstLocalPtr; localPtr!=NULL;
	    localPtr=localPtr->nextPtr) {
	if (TclIsVarArgument(localPtr)) {
	    Tcl_Obj *argObj;

	    argObj = Tcl_NewObj();
	    Tcl_ListObjAppendElement(NULL, argObj,
		    Tcl_NewStringObj(localPtr->name, -1));
	    if (localPtr->defValuePtr != NULL) {
		Tcl_ListObjAppendElement(NULL, argObj, localPtr->defValuePtr);
	    }
	    Tcl_ListObjAppendElement(NULL, resultObjs[0], argObj);
	}
    }
    resultObjs[1] = TclOOGetMethodBody(Tcl_GetHashValue(hPtr));
    Tcl_SetObjResult(interp, Tcl_NewListObj(2, resultObjs));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectFiltersCmd --
 *
 *	Implements [info object filters $objName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectFiltersCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int i;
    Tcl_Obj *filterObj, *resultObj;
    Object *oPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    resultObj = Tcl_NewObj();

    FOREACH(filterObj, oPtr->filters) {
	Tcl_ListObjAppendElement(NULL, resultObj, filterObj);
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectForwardCmd --
 *
 *	Implements [info object forward $objName $methodName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectForwardCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Tcl_HashEntry *hPtr;
    Tcl_Obj *prefixObj;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName methodName");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    if (!oPtr->methodsPtr) {
	goto unknownMethod;
    }
    hPtr = Tcl_FindHashEntry(oPtr->methodsPtr, (char *) objv[2]);
    if (hPtr == NULL) {
    unknownMethod:
	Tcl_AppendResult(interp, "unknown method \"", TclGetString(objv[2]),
		"\"", NULL);
	return TCL_ERROR;
    }
    prefixObj = TclOOGetFwdFromMethod(Tcl_GetHashValue(hPtr));
    if (prefixObj == NULL) {
	Tcl_AppendResult(interp,
		"prefix argument list not available for this kind of method",
		NULL);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, prefixObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectIsACmd --
 *
 *	Implements [info object isa $category $objName ...]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectIsACmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    static const char *const categories[] = {
	"class", "metaclass", "mixin", "object", "typeof", NULL
    };
    enum IsACats {
	IsClass, IsMetaclass, IsMixin, IsObject, IsType
    };
    Object *oPtr, *o2Ptr;
    int idx, i;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "category objName ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], categories, "category", 0,
	    &idx) != TCL_OK) {
	return TCL_ERROR;
    }

    if (idx == IsObject) {
	int ok = (Tcl_GetObjectFromObj(interp, objv[2]) != NULL);

	if (!ok) {
	    Tcl_ResetResult(interp);
	}
	Tcl_SetObjResult(interp, Tcl_NewIntObj(ok ? 1 : 0));
	return TCL_OK;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[2]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    switch ((enum IsACats) idx) {
    case IsClass:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "objName");
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, Tcl_NewIntObj(oPtr->classPtr ? 1 : 0));
	return TCL_OK;
    case IsMetaclass:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "objName");
	    return TCL_ERROR;
	}
	if (oPtr->classPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
	} else {
	    Class *classCls = TclOOGetFoundation(interp)->classCls;

	    Tcl_SetObjResult(interp, Tcl_NewIntObj(
		    TclOOIsReachable(classCls, oPtr->classPtr) ? 1 : 0));
	}
	return TCL_OK;
    case IsMixin:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "objName className");
	    return TCL_ERROR;
	}
	o2Ptr = (Object *) Tcl_GetObjectFromObj(interp, objv[3]);
	if (o2Ptr == NULL) {
	    return TCL_ERROR;
	}
	if (o2Ptr->classPtr == NULL) {
	    Tcl_AppendResult(interp, "non-classes cannot be mixins", NULL);
	    return TCL_ERROR;
	} else {
	    Class *mixinPtr;

	    FOREACH(mixinPtr, oPtr->mixins) {
		if (mixinPtr == o2Ptr->classPtr) {
		    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
		    return TCL_OK;
		}
	    }
	}
	Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
	return TCL_OK;
    case IsType:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "objName className");
	    return TCL_ERROR;
	}
	o2Ptr = (Object *) Tcl_GetObjectFromObj(interp, objv[3]);
	if (o2Ptr == NULL) {
	    return TCL_ERROR;
	}
	if (o2Ptr->classPtr == NULL) {
	    Tcl_AppendResult(interp, "non-classes cannot be types", NULL);
	    return TCL_ERROR;
	}
	if (TclOOIsReachable(o2Ptr->classPtr, oPtr->selfCls)) {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
	}
	return TCL_OK;
    case IsObject:
	Tcl_Panic("unexpected fallthrough");
    }
    return TCL_ERROR;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectMethodsCmd --
 *
 *	Implements [info object methods $objName ?$option ...?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectMethodsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    int flag = PUBLIC_METHOD, recurse = 0;
    FOREACH_HASH_DECLS;
    Tcl_Obj *namePtr, *resultObj;
    Method *mPtr;
    static const char *const options[] = {
	"-all", "-localprivate", "-private", NULL
    };
    enum Options {
	OPT_ALL, OPT_LOCALPRIVATE, OPT_PRIVATE
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName ?-option value ...?");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (objc != 2) {
	int i, idx;

	for (i=2 ; i<objc ; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], options, "option", 0,
		    &idx) != TCL_OK) {
		return TCL_ERROR;
	    }
	    switch ((enum Options) idx) {
	    case OPT_ALL:
		recurse = 1;
		break;
	    case OPT_LOCALPRIVATE:
		flag = PRIVATE_METHOD;
		break;
	    case OPT_PRIVATE:
		flag = 0;
		break;
	    }
	}
    }

    resultObj = Tcl_NewObj();
    if (recurse) {
	const char **names;
	int i, numNames = TclOOGetSortedMethodList(oPtr, flag, &names);

	for (i=0 ; i<numNames ; i++) {
	    Tcl_ListObjAppendElement(NULL, resultObj,
		    Tcl_NewStringObj(names[i], -1));
	}
	if (numNames > 0) {
	    ckfree((char *) names);
	}
    } else if (oPtr->methodsPtr) {
	FOREACH_HASH(namePtr, mPtr, oPtr->methodsPtr) {
	    if (mPtr->typePtr != NULL && (mPtr->flags & flag) == flag) {
		Tcl_ListObjAppendElement(NULL, resultObj, namePtr);
	    }
	}
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectMixinsCmd --
 *
 *	Implements [info object mixins $objName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectMixinsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *mixinPtr;
    Object *oPtr;
    Tcl_Obj *resultObj;
    int i;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    resultObj = Tcl_NewObj();
    FOREACH(mixinPtr, oPtr->mixins) {
	Tcl_ListObjAppendElement(NULL, resultObj,
		TclOOObjectName(interp, mixinPtr->thisPtr));
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectNsCmd --
 *
 *	Implements [info object namespace $objName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectNsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp,
	    Tcl_NewStringObj(oPtr->namespacePtr->fullName, -1));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectVariablesCmd --
 *
 *	Implements [info object variables $objName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectVariablesCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Tcl_Obj *variableObj, *resultObj;
    int i;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    resultObj = Tcl_NewObj();
    FOREACH(variableObj, oPtr->variables) {
	Tcl_ListObjAppendElement(NULL, resultObj, variableObj);
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectVarsCmd --
 *
 *	Implements [info object vars $objName ?$pattern?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectVarsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    const char *pattern = NULL;
    FOREACH_HASH_DECLS;
    VarInHash *vihPtr;
    Tcl_Obj *nameObj, *resultObj;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName ?pattern?");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (objc == 3) {
	pattern = TclGetString(objv[2]);
    }
    resultObj = Tcl_NewObj();

    /*
     * Extract the information we need from the object's namespace's table of
     * variables. Note that this involves horrific knowledge of the guts of
     * tclVar.c, so we can't leverage our hash-iteration macros properly.
     */

    FOREACH_HASH_VALUE(vihPtr,
	    &((Namespace *) oPtr->namespacePtr)->varTable.table) {
	nameObj = vihPtr->entry.key.objPtr;

	if (TclIsVarUndefined(&vihPtr->var)
		|| !TclIsVarNamespaceVar(&vihPtr->var)) {
	    continue;
	}
	if (pattern != NULL
		&& !Tcl_StringMatch(TclGetString(nameObj), pattern)) {
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, resultObj, nameObj);
    }

    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassConstrCmd --
 *
 *	Implements [info class constructor $clsName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassConstrCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *resultObjs[2];
    Class *clsPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    if (clsPtr->constructorPtr == NULL) {
	return TCL_OK;
    }
    procPtr = TclOOGetProcFromMethod(clsPtr->constructorPtr);
    if (procPtr == NULL) {
	Tcl_AppendResult(interp,
		"definition not available for this kind of method", NULL);
	return TCL_ERROR;
    }

    resultObjs[0] = Tcl_NewObj();
    for (localPtr=procPtr->firstLocalPtr; localPtr!=NULL;
	    localPtr=localPtr->nextPtr) {
	if (TclIsVarArgument(localPtr)) {
	    Tcl_Obj *argObj;

	    argObj = Tcl_NewObj();
	    Tcl_ListObjAppendElement(NULL, argObj,
		    Tcl_NewStringObj(localPtr->name, -1));
	    if (localPtr->defValuePtr != NULL) {
		Tcl_ListObjAppendElement(NULL, argObj, localPtr->defValuePtr);
	    }
	    Tcl_ListObjAppendElement(NULL, resultObjs[0], argObj);
	}
    }
    resultObjs[1] = TclOOGetMethodBody(clsPtr->constructorPtr);
    Tcl_SetObjResult(interp, Tcl_NewListObj(2, resultObjs));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassDefnCmd --
 *
 *	Implements [info class definition $clsName $methodName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassDefnCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_HashEntry *hPtr;
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *resultObjs[2];
    Class *clsPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className methodName");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    hPtr = Tcl_FindHashEntry(&clsPtr->classMethods, (char *) objv[2]);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown method \"", TclGetString(objv[2]),
		"\"", NULL);
	return TCL_ERROR;
    }
    procPtr = TclOOGetProcFromMethod(Tcl_GetHashValue(hPtr));
    if (procPtr == NULL) {
	Tcl_AppendResult(interp,
		"definition not available for this kind of method", NULL);
	return TCL_ERROR;
    }

    resultObjs[0] = Tcl_NewObj();
    for (localPtr=procPtr->firstLocalPtr; localPtr!=NULL;
	    localPtr=localPtr->nextPtr) {
	if (TclIsVarArgument(localPtr)) {
	    Tcl_Obj *argObj;

	    argObj = Tcl_NewObj();
	    Tcl_ListObjAppendElement(NULL, argObj,
		    Tcl_NewStringObj(localPtr->name, -1));
	    if (localPtr->defValuePtr != NULL) {
		Tcl_ListObjAppendElement(NULL, argObj, localPtr->defValuePtr);
	    }
	    Tcl_ListObjAppendElement(NULL, resultObjs[0], argObj);
	}
    }
    resultObjs[1] = TclOOGetMethodBody(Tcl_GetHashValue(hPtr));
    Tcl_SetObjResult(interp, Tcl_NewListObj(2, resultObjs));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassDestrCmd --
 *
 *	Implements [info class destructor $clsName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassDestrCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Proc *procPtr;
    Class *clsPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }

    if (clsPtr->destructorPtr == NULL) {
	return TCL_OK;
    }
    procPtr = TclOOGetProcFromMethod(clsPtr->destructorPtr);
    if (procPtr == NULL) {
	Tcl_AppendResult(interp,
		"definition not available for this kind of method", NULL);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, TclOOGetMethodBody(clsPtr->destructorPtr));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassFiltersCmd --
 *
 *	Implements [info class filters $clsName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassFiltersCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int i;
    Tcl_Obj *filterObj, *resultObj;
    Class *clsPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }

    resultObj = Tcl_NewObj();
    FOREACH(filterObj, clsPtr->filters) {
	Tcl_ListObjAppendElement(NULL, resultObj, filterObj);
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassForwardCmd --
 *
 *	Implements [info class forward $clsName $methodName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassForwardCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_HashEntry *hPtr;
    Tcl_Obj *prefixObj;
    Class *clsPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className methodName");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    hPtr = Tcl_FindHashEntry(&clsPtr->classMethods, (char *) objv[2]);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown method \"", TclGetString(objv[2]),
		"\"", NULL);
	return TCL_ERROR;
    }
    prefixObj = TclOOGetFwdFromMethod(Tcl_GetHashValue(hPtr));
    if (prefixObj == NULL) {
	Tcl_AppendResult(interp,
		"prefix argument list not available for this kind of method",
		NULL);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, prefixObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassInstancesCmd --
 *
 *	Implements [info class instances $clsName ?$pattern?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassInstancesCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Class *clsPtr;
    int i;
    const char *pattern = NULL;
    Tcl_Obj *resultObj;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className ?pattern?");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    if (objc == 3) {
	pattern = TclGetString(objv[2]);
    }

    resultObj = Tcl_NewObj();
    FOREACH(oPtr, clsPtr->instances) {
	Tcl_Obj *tmpObj = TclOOObjectName(interp, oPtr);

	if (pattern && !Tcl_StringMatch(TclGetString(tmpObj), pattern)) {
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, resultObj, tmpObj);
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassMethodsCmd --
 *
 *	Implements [info class methods $clsName ?-private?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassMethodsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int flag = PUBLIC_METHOD, recurse = 0;
    Tcl_Obj *namePtr, *resultObj;
    Method *mPtr;
    Class *clsPtr;
    static const char *const options[] = {
	"-all", "-localprivate", "-private", NULL
    };
    enum Options {
	OPT_ALL, OPT_LOCALPRIVATE, OPT_PRIVATE
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className ?-option value ...?");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    if (objc != 2) {
	int i, idx;

	for (i=2 ; i<objc ; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], options, "option", 0,
		    &idx) != TCL_OK) {
		return TCL_ERROR;
	    }
	    switch ((enum Options) idx) {
	    case OPT_ALL:
		recurse = 1;
		break;
	    case OPT_LOCALPRIVATE:
		flag = PRIVATE_METHOD;
		break;
	    case OPT_PRIVATE:
		flag = 0;
		break;
	    }
	}
    }

    resultObj = Tcl_NewObj();
    if (recurse) {
	const char **names;
	int i, numNames = TclOOGetSortedClassMethodList(clsPtr, flag, &names);

	for (i=0 ; i<numNames ; i++) {
	    Tcl_ListObjAppendElement(NULL, resultObj,
		    Tcl_NewStringObj(names[i], -1));
	}
	if (numNames > 0) {
	    ckfree((char *) names);
	}
    } else {
	FOREACH_HASH_DECLS;

	FOREACH_HASH(namePtr, mPtr, &clsPtr->classMethods) {
	    if (mPtr->typePtr != NULL && (mPtr->flags & flag) == flag) {
		Tcl_ListObjAppendElement(NULL, resultObj, namePtr);
	    }
	}
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassMixinsCmd --
 *
 *	Implements [info class mixins $clsName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassMixinsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *clsPtr, *mixinPtr;
    Tcl_Obj *resultObj;
    int i;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }

    resultObj = Tcl_NewObj();
    FOREACH(mixinPtr, clsPtr->mixins) {
	Tcl_ListObjAppendElement(NULL, resultObj,
		TclOOObjectName(interp, mixinPtr->thisPtr));
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassSubsCmd --
 *
 *	Implements [info class subclasses $clsName ?$pattern?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassSubsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *clsPtr, *subclassPtr;
    Tcl_Obj *resultObj;
    int i;
    const char *pattern = NULL;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className ?pattern?");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    if (objc == 3) {
	pattern = TclGetString(objv[2]);
    }

    resultObj = Tcl_NewObj();
    FOREACH(subclassPtr, clsPtr->subclasses) {
	Tcl_Obj *tmpObj = TclOOObjectName(interp, subclassPtr->thisPtr);

	if (pattern && !Tcl_StringMatch(TclGetString(tmpObj), pattern)) {
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, resultObj, tmpObj);
    }
    FOREACH(subclassPtr, clsPtr->mixinSubs) {
	Tcl_Obj *tmpObj = TclOOObjectName(interp, subclassPtr->thisPtr);

	if (pattern && !Tcl_StringMatch(TclGetString(tmpObj), pattern)) {
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, resultObj, tmpObj);
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassSupersCmd --
 *
 *	Implements [info class superclasses $clsName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassSupersCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *clsPtr, *superPtr;
    Tcl_Obj *resultObj;
    int i;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }

    resultObj = Tcl_NewObj();
    FOREACH(superPtr, clsPtr->superclasses) {
	Tcl_ListObjAppendElement(NULL, resultObj,
		TclOOObjectName(interp, superPtr->thisPtr));
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassVariablesCmd --
 *
 *	Implements [info class variables $clsName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassVariablesCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *clsPtr;
    Tcl_Obj *variableObj, *resultObj;
    int i;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }

    resultObj = Tcl_NewObj();
    FOREACH(variableObj, clsPtr->variables) {
	Tcl_ListObjAppendElement(NULL, resultObj, variableObj);
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
