/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tcl]
 *  DESCRIPTION:  Object-Oriented Extensions to Tcl
 *
 *  [incr Tcl] provides object-oriented extensions to Tcl, much as
 *  C++ provides object-oriented extensions to C.  It provides a means
 *  of encapsulating related procedures together with their shared data
 *  in a local namespace that is hidden from the outside world.  It
 *  promotes code re-use through inheritance.  More than anything else,
 *  it encourages better organization of Tcl applications through the
 *  object-oriented paradigm, leading to code that is easier to
 *  understand and maintain.
 *
 *  This segment handles "objects" which are instantiated from class
 *  definitions.  Objects contain public/protected/private data members
 *  from all classes in a derivation hierarchy.
 *
 * ========================================================================
 *  AUTHOR:  Michael J. McLennan
 *           Bell Labs Innovations for Lucent Technologies
 *           mmclennan@lucent.com
 *           http://www.tcltk.com/itcl
 *
 *  overhauled version author: Arnulf Wiedemann Copyright (c) 2007
 *
 *     RCS:  $Id$
 * ========================================================================
 *           Copyright (c) 1993-1998  Lucent Technologies, Inc.
 * ------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#include "itclInt.h"

Tcl_ObjCmdProc Itcl_BiMyTypeMethodCmd;
Tcl_ObjCmdProc Itcl_BiMyMethodCmd;
Tcl_ObjCmdProc Itcl_BiMyProcCmd;
Tcl_ObjCmdProc Itcl_BiMyTypeVarCmd;
Tcl_ObjCmdProc Itcl_BiMyVarCmd;
Tcl_ObjCmdProc Itcl_BiItclHullCmd;
Tcl_ObjCmdProc Itcl_BiCallInstanceCmd;
Tcl_ObjCmdProc Itcl_BiGetInstanceVarCmd;
Tcl_ObjCmdProc Itcl_BiInstallComponentCmd;

/*
 *  FORWARD DECLARATIONS
 */
static char* ItclTraceThisVar(ClientData cdata, Tcl_Interp *interp,
	const char *name1, const char *name2, int flags);
static char* ItclTraceOptionVar(ClientData cdata, Tcl_Interp *interp,
	const char *name1, const char *name2, int flags);
static char* ItclTraceComponentVar(ClientData cdata, Tcl_Interp *interp,
	const char *name1, const char *name2, int flags);
static char* ItclTraceItclHullVar(ClientData cdata, Tcl_Interp *interp,
	const char *name1, const char *name2, int flags);

static void ItclDestroyObject(ClientData clientData);
static void ItclFreeObject(char * clientData);

static int ItclDestructBase(Tcl_Interp *interp, ItclObject *contextObj,
        ItclClass *contextClass, int flags);

static int ItclInitObjectVariables(Tcl_Interp *interp, ItclObject *ioPtr,
        ItclClass *iclsPtr, const char *name);
static int ItclInitObjectCommands(Tcl_Interp *interp, ItclObject *ioPtr,
        ItclClass *iclsPtr, const char *name);
static int ItclInitExtendedClassOptions(Tcl_Interp *interp, ItclObject *ioPtr);
static int ItclInitObjectOptions(Tcl_Interp *interp, ItclObject *ioPtr,
        ItclClass *iclsPtr, const char *name);
static const char * GetConstructorVar(Tcl_Interp *interp, ItclClass *iclsPtr,
        const char *varName);


/*
 * ------------------------------------------------------------------------
 *  ItclDeleteObjectMetadata()
 *
 *  Delete the metadata data if any
 *-------------------------------------------------------------------------
 */
void
ItclDeleteObjectMetadata(
    ClientData clientData)
{
    /*
     * nothing to to yet, as there are only ItclClass or ItclObject pointers
     * stored, which are freed elsewhere
     */
}

/*
 * ------------------------------------------------------------------------
 *  ObjectRenamedTrace()
 *
 * ------------------------------------------------------------------------
 */

static void
ObjectRenamedTrace(
    ClientData clientData,      /* The object being deleted. */
    Tcl_Interp *interp,         /* The interpreter containing the object. */
    const char *oldName,        /* What the object was (last) called. */
    const char *newName,        /* Always NULL ??. not for itk!! */
    int flags)                  /* Why was the object deleted? */
{
    ItclObject *ioPtr = clientData;
    Itcl_InterpState istate;

    if (newName != NULL) {
	/* FIXME should enter the new name in the hashtables for objects etc. */
        return;
    }
    if (ioPtr->flags & ITCL_OBJECT_CLASS_DESTRUCTED) {
        return;
    }
    ioPtr->flags |= ITCL_OBJECT_IS_RENAMED;
    if (ioPtr->flags & ITCL_TCLOO_OBJECT_IS_DELETED) {
        ioPtr->oPtr = NULL;
    }
    if (!(ioPtr->flags & ITCL_OBJECT_CLASS_DESTRUCTED)) {
        /*
         *  Attempt to destruct the object, but ignore any errors.
         */
        istate = Itcl_SaveInterpState(ioPtr->interp, 0);
        Itcl_DestructObject(ioPtr->interp, ioPtr, ITCL_IGNORE_ERRS);
        Itcl_RestoreInterpState(ioPtr->interp, istate);
        ioPtr->flags |= ITCL_OBJECT_CLASS_DESTRUCTED;
    }
}

/*
 * ------------------------------------------------------------------------
 *  ItclCreateObject()
 *
 *  Creates a new object instance belonging to the given class.
 *  Supports complex object names like "namesp::namesp::name" by
 *  following the namespace path and creating the object in the
 *  desired namespace.
 *
 *  Automatically creates and initializes data members, including the
 *  built-in protected "this" variable containing the object name.
 *  Installs an access command in the current namespace, and invokes
 *  the constructor to initialize the object.
 *
 *  If any errors are encountered, the object is destroyed and this
 *  procedure returns TCL_ERROR (along with an error message in the
 *  interpreter).  Otherwise, it returns TCL_OK
 * ------------------------------------------------------------------------
 */
int
ItclCreateObject(
    Tcl_Interp *interp,      /* interpreter mananging new object */
    const char* name,        /* name of new object */
    ItclClass *iclsPtr,        /* class for new object */
    int objc,                /* number of arguments */
    Tcl_Obj *const objv[])   /* argument objects */
{
    int result = TCL_OK;

    Tcl_DString buffer;
    Tcl_CmdInfo cmdInfo;
    Tcl_Command cmdPtr;
    Tcl_HashEntry *hPtr;
    Tcl_Obj **newObjv;
    Tcl_Obj *objPtr;
    ItclObjectInfo *infoPtr;
    ItclObject *saveCurrIoPtr;
    ItclObject *ioPtr;
    Itcl_InterpState istate;
    const char *nsName;
    const char *objName;
    char unique[256];    /* buffer used for unique part of object names */
    int newObjc;
    int newEntry;
    ItclResolveInfo *resolveInfoPtr;
    char str[100];

    infoPtr = NULL;
    ItclShowArgs(1, "ItclCreateObject", objc, objv);
    saveCurrIoPtr = NULL;
    if (iclsPtr->flags & (ITCL_TYPE|ITCL_WIDGETADAPTOR)) {
        /* check, if the object already exists and if yes delete it silently */
	cmdPtr = Tcl_FindCommand(interp, name, NULL, 0);
	if (cmdPtr != NULL) {
            Tcl_GetCommandInfoFromToken(cmdPtr, &cmdInfo);
	    if (cmdInfo.deleteProc == ItclDestroyObject) {
		Itcl_RenameCommand(interp, name, "");
	    }
        }
    }
    /* just init for the case of none ItclWidget objects */
    newObjc = objc;
    newObjv = (Tcl_Obj **)objv;
    infoPtr = iclsPtr->infoPtr;
    /*
     *  Create a new object and initialize it.
     */
    ioPtr = (ItclObject*)ckalloc(sizeof(ItclObject));
    memset(ioPtr, 0, sizeof(ItclObject));
    ioPtr->iclsPtr = iclsPtr;
    ioPtr->interp = interp;
    ioPtr->infoPtr = infoPtr;
    Itcl_PreserveData((ClientData)iclsPtr);

    ioPtr->constructed = (Tcl_HashTable*)ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitObjHashTable(ioPtr->constructed);

    /*
     *  Add a command to the current namespace with the object name.
     *  This is done before invoking the constructors so that the
     *  command can be used during construction to query info.
     */
    Itcl_PreserveData((ClientData)ioPtr);

    ioPtr->namePtr = Tcl_NewStringObj(name, -1);
    Tcl_IncrRefCount(ioPtr->namePtr);
    nsName = Tcl_GetCurrentNamespace(interp)->fullName;
    ioPtr->origNamePtr = Tcl_NewStringObj("", -1);
    if ((name[0] != ':') && (name[1] != ':')) {
        Tcl_AppendToObj(ioPtr->origNamePtr, nsName, -1);
        if (strcmp(nsName, "::") != 0) {
            Tcl_AppendToObj(ioPtr->origNamePtr, "::", -1);
        }
    }
    Tcl_AppendToObj(ioPtr->origNamePtr, name, -1);
    Tcl_IncrRefCount(ioPtr->origNamePtr);
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, ITCL_VARIABLES_NAMESPACE, -1);
    Tcl_DStringAppend(&buffer, "::", 2);
    Tcl_DStringAppend(&buffer, Tcl_GetString(ioPtr->namePtr), -1);
    ioPtr->varNsNamePtr = Tcl_NewStringObj(Tcl_DStringValue(&buffer), -1);
    Tcl_IncrRefCount(ioPtr->varNsNamePtr);
    Tcl_DStringFree(&buffer);

    Tcl_InitHashTable(&ioPtr->objectVariables, TCL_ONE_WORD_KEYS);
    Tcl_InitObjHashTable(&ioPtr->objectOptions);
    Tcl_InitObjHashTable(&ioPtr->objectComponents);
    Tcl_InitObjHashTable(&ioPtr->objectDelegatedOptions);
    Tcl_InitObjHashTable(&ioPtr->objectDelegatedFunctions);
    Tcl_InitObjHashTable(&ioPtr->objectMethodVariables);
    Tcl_InitHashTable(&ioPtr->contextCache, TCL_ONE_WORD_KEYS);

    Itcl_PreserveData((ClientData)ioPtr);  /* while we're using this... */
    Itcl_EventuallyFree((ClientData)ioPtr, ItclFreeObject);

    /*
     *  Install the class namespace and object context so that
     *  the object's data members can be initialized via simple
     *  "set" commands.
     */

    /* first create the object's class variables namespaces
     * and set all the init values for variables
     */

    if (ItclInitObjectVariables(interp, ioPtr, iclsPtr, name) != TCL_OK) {
	result = TCL_ERROR;
        goto errorReturn;
    }
    if (ItclInitObjectCommands(interp, ioPtr, iclsPtr, name) != TCL_OK) {
	Tcl_AppendResult(interp, "error in ItclInitObjectCommands", NULL);
	result = TCL_ERROR;
        goto errorReturn;
    }
    if (iclsPtr->flags & (ITCL_ECLASS|ITCL_NWIDGET|ITCL_WIDGET|
            ITCL_TYPE|ITCL_WIDGETADAPTOR)) {
	if (iclsPtr->flags & (ITCL_ECLASS|ITCL_TYPE|ITCL_WIDGET|
	        ITCL_WIDGETADAPTOR)) {
            ItclInitExtendedClassOptions(interp, ioPtr);
            if (ItclInitObjectOptions(interp, ioPtr, iclsPtr, name) != TCL_OK) {
	        Tcl_AppendResult(interp, "error in ItclInitObjectOptions",
		        NULL);
	        result = TCL_ERROR;
                goto errorReturn;
            }
        }
        if (ItclInitObjectMethodVariables(interp, ioPtr, iclsPtr, name)
	        != TCL_OK) {
	    Tcl_AppendResult(interp,
	            "error in ItclInitObjectMethodVariables", NULL);
	    result = TCL_ERROR;
            goto errorReturn;
        }
    }

    saveCurrIoPtr = infoPtr->currIoPtr;
    infoPtr->currIoPtr = ioPtr;
    if (iclsPtr->flags & ITCL_WIDGET) {
        newObjc = objc;
        newObjv = (Tcl_Obj **)ckalloc(sizeof(Tcl_Obj *) * (objc + 5));
        newObjv[0] = Tcl_NewStringObj(
                "::itcl::internal::commands::hullandoptionsinstall", -1);
        newObjv[1] = ioPtr->namePtr;
        Tcl_IncrRefCount(newObjv[1]);
        newObjv[2] = ioPtr->iclsPtr->namePtr;
        Tcl_IncrRefCount(newObjv[2]);
        if (ioPtr->iclsPtr->widgetClassPtr != NULL) {
            newObjv[3] = ioPtr->iclsPtr->widgetClassPtr;
        } else {
            newObjv[3] = Tcl_NewStringObj("", -1);
        }
        Tcl_IncrRefCount(newObjv[3]);
        if (ioPtr->iclsPtr->hullTypePtr != NULL) {
            newObjv[4] = ioPtr->iclsPtr->hullTypePtr;
        } else {
            newObjv[4] = Tcl_NewStringObj("", -1);
        }
        Tcl_IncrRefCount(newObjv[4]);
        memcpy(newObjv + 5, objv, (objc * sizeof(Tcl_Obj *)));
        result = Tcl_EvalObjv(interp, objc+5, newObjv, 0);
        Tcl_DecrRefCount(newObjv[0]);
        Tcl_DecrRefCount(newObjv[1]);
        Tcl_DecrRefCount(newObjv[2]);
        Tcl_DecrRefCount(newObjv[3]);
        Tcl_DecrRefCount(newObjv[4]);
        ckfree((char *)newObjv);
        if (result != TCL_OK) {
            goto errorReturn;
        }
    }
    if (iclsPtr->flags & (ITCL_WIDGET|ITCL_WIDGETADAPTOR)) {
        if (ItclInitObjectMethodVariables(interp, ioPtr, iclsPtr, name)
	        != TCL_OK) {
	    Tcl_AppendResult(interp,
	            "error in ItclInitObjectMethodVariables", NULL);
	    infoPtr->currIoPtr = saveCurrIoPtr;
	    result = TCL_ERROR;
            goto errorReturn;
        }
    }
    objName = name;
    if (iclsPtr->flags & ITCL_WIDGETADAPTOR) {
        /* use a temporary name here as widgetadaptors often hijack the
	 * name for use in installhull. Rename it after the constructor has
	 * been run to the wanted name
	 */
        /*
         *  Add a unique part, and keep
         *  incrementing a counter until a valid name is found.
         */
        do {
	    Tcl_CmdInfo dummy;

            sprintf(unique,"%.200s_%d", name, iclsPtr->unique++);
            unique[0] = tolower(unique[0]);

            Tcl_DStringTrunc(&buffer, 0);
            Tcl_DStringAppend(&buffer, unique, -1);
            objName = Tcl_DStringValue(&buffer);

	    /*
	     * [Fix 227811] Check for any command with the
	     * given name, not only objects.
	     */

            if (Tcl_GetCommandInfo (interp, objName, &dummy) == 0) {
                break;  /* if an error is found, bail out! */
            }
        } while (1);
	ioPtr->createNamePtr = Tcl_NewStringObj(objName, -1);
    }
    ioPtr->oPtr = Tcl_NewObjectInstance(interp, iclsPtr->clsPtr, objName,
            iclsPtr->nsPtr->fullName, /* objc */-1, /* objv */NULL,
	    /* skip */0);
    if (ioPtr->oPtr == NULL) {
	infoPtr->currIoPtr = saveCurrIoPtr;
        ItclDeleteObjectVariablesNamespace(interp, ioPtr);
        Itcl_ReleaseData((ClientData)ioPtr);
        Itcl_ReleaseData((ClientData)ioPtr);
        return TCL_ERROR;
    }
    Tcl_ObjectSetMethodNameMapper(ioPtr->oPtr, ItclMapMethodNameProc);

    ioPtr->accessCmd = Tcl_GetObjectCommand(ioPtr->oPtr);
    Tcl_GetCommandInfoFromToken(ioPtr->accessCmd, &cmdInfo);
    cmdInfo.deleteProc = (void *)ItclDestroyObject;
    cmdInfo.deleteData = ioPtr;
    Tcl_SetCommandInfoFromToken(ioPtr->accessCmd, &cmdInfo);
    ioPtr->resolvePtr = (Tcl_Resolve *)ckalloc(sizeof(Tcl_Resolve));
    ioPtr->resolvePtr->cmdProcPtr = Itcl_CmdAliasProc;
    ioPtr->resolvePtr->varProcPtr = Itcl_VarAliasProc;
    resolveInfoPtr = (ItclResolveInfo *)ckalloc(sizeof(ItclResolveInfo));
    memset (resolveInfoPtr, 0, sizeof(ItclResolveInfo));
    resolveInfoPtr->flags = ITCL_RESOLVE_OBJECT;
    resolveInfoPtr->ioPtr = ioPtr;
    ioPtr->resolvePtr->clientData = resolveInfoPtr;
    Tcl_TraceCommand(interp, Tcl_GetString(ioPtr->namePtr),
            TCL_TRACE_RENAME|TCL_TRACE_DELETE, ObjectRenamedTrace, ioPtr);

    Tcl_ObjectSetMetadata(ioPtr->oPtr, iclsPtr->infoPtr->object_meta_type,
            ioPtr);

    /* make the object known, if it is used in the constructor already! */
    hPtr = Tcl_CreateHashEntry(&iclsPtr->infoPtr->objectCmds,
        (char*)ioPtr->accessCmd, &newEntry);
    Tcl_SetHashValue(hPtr, (ClientData)ioPtr);

    hPtr = Tcl_CreateHashEntry(&iclsPtr->infoPtr->objects,
        (char*)ioPtr, &newEntry);
    Tcl_SetHashValue(hPtr, (ClientData)ioPtr);
    hPtr = Tcl_CreateHashEntry(&iclsPtr->infoPtr->objectNames,
            (char*)ioPtr->namePtr, &newEntry);
    Tcl_SetHashValue(hPtr, (ClientData)ioPtr);

    /* make the object instance known, for use as unique key if the object */
    /* is renamed. Used by mytypemethod etc. */
    sprintf(str, "ItclInst%d", iclsPtr->infoPtr->numInstances);
    /* FIXME need to free that when deleting object and to remove the entries!! */
    objPtr = Tcl_NewStringObj(str, -1);
    hPtr = Tcl_CreateHashEntry(&iclsPtr->infoPtr->instances,
        (char*)objPtr, &newEntry);
    Tcl_SetHashValue(hPtr, (ClientData)ioPtr);
    hPtr = Tcl_CreateHashEntry(&iclsPtr->infoPtr->objectInstances,
        (char*)ioPtr, &newEntry);
    Tcl_SetHashValue(hPtr, (ClientData)objPtr);
    iclsPtr->infoPtr->numInstances++;

    /*
     *  Now construct the object.  Look for a constructor in the
     *  most-specific class, and if there is one, invoke it.
     *  This will cause a chain reaction, making sure that all
     *  base classes constructors are invoked as well, in order
     *  from least- to most-specific.  Any constructors that are
     *  not called out explicitly in "initCode" code fragments are
     *  invoked implicitly without arguments.
     */
    ItclShowArgs(1, "OBJECTCONSTRUCTOR", objc, objv);
    ioPtr->hadConstructorError = 0;
    result = Itcl_InvokeMethodIfExists(interp, "constructor",
        iclsPtr, ioPtr, objc, objv);
    if (ioPtr->hadConstructorError) {
        result = TCL_ERROR;
    }
    ioPtr->hadConstructorError = -1;
    if (result != TCL_OK) {
        int constructorStackSize;
	/* clean up the constructor stack */
        constructorStackSize = Itcl_GetStackSize(&infoPtr->constructorStack);
	while (constructorStackSize > 0) {
	    Itcl_PopStack(&infoPtr->constructorStack);
	    constructorStackSize--;
	}
        istate = Itcl_SaveInterpState(interp, result);
	ItclDeleteObjectVariablesNamespace(interp, ioPtr);
	if (ioPtr->accessCmd != (Tcl_Command) NULL) {
	    Tcl_DeleteCommandFromToken(interp, ioPtr->accessCmd);
	    ioPtr->accessCmd = NULL;
	}
        result = Itcl_RestoreInterpState(interp, istate);
	infoPtr->currIoPtr = saveCurrIoPtr;
	/* need this for 2 ReleaseData at errorReturn!! */
        Itcl_PreserveData(ioPtr);
        goto errorReturn;
    } else {
	/* a constructor cannot return a result as the object name
	 * is returned as result */
        Tcl_ResetResult(interp);
    }

    /*
     *  If there is no constructor, construct the base classes
     *  in case they have constructors.  This will cause the
     *  same chain reaction.
     */
    objPtr = Tcl_NewStringObj("constructor", -1);
    if (Tcl_FindHashEntry(&iclsPtr->functions, (char *)objPtr) == NULL) {
        result = Itcl_ConstructBase(interp, ioPtr, iclsPtr, objc, objv);
    }
    Tcl_DecrRefCount(objPtr);

    if (iclsPtr->flags & ITCL_ECLASS) {
        ItclInitExtendedClassOptions(interp, ioPtr);
        if (ItclInitObjectOptions(interp, ioPtr, iclsPtr, name) != TCL_OK) {
                Tcl_AppendResult(interp, "error in ItclInitObjectOptions",
	        NULL);
            result = TCL_ERROR;
            goto errorReturn;
        }
    }
    /*
     *  If construction failed, then delete the object access
     *  command.  This will destruct the object and delete the
     *  object data.  Be careful to save and restore the interpreter
     *  state, since the destructors may generate errors of their own.
     */
    if (result != TCL_OK) {
        istate = Itcl_SaveInterpState(interp, result);

	/* Bug 227824.
	 * The constructor may destroy the object, possibly indirectly
	 * through the destruction of the main widget in the iTk
	 * megawidget it tried to construct. If this happens we must
	 * not try to destroy the access command a second time.
	 */
	if (ioPtr->accessCmd != (Tcl_Command) NULL) {
	    Tcl_DeleteCommandFromToken(interp, ioPtr->accessCmd);
	    ioPtr->accessCmd = NULL;
	}
        result = Itcl_RestoreInterpState(interp, istate);
	/* need this for 2 ReleaseData at errorReturn!! */
        Itcl_PreserveData(ioPtr);
        goto errorReturn;
    }

    if (iclsPtr->flags & ITCL_WIDGETADAPTOR) {
        Itcl_RenameCommand(interp, objName, name);
        ioPtr->createNamePtr = NULL;
        Tcl_TraceCommand(interp, Tcl_GetString(ioPtr->namePtr),
                TCL_TRACE_RENAME|TCL_TRACE_DELETE, ObjectRenamedTrace, ioPtr);
    }
    if (iclsPtr->flags & (ITCL_WIDGETADAPTOR)) {
        /* 
         * set all the init values for options
         */

        objPtr = Tcl_NewStringObj(
	        ITCL_NAMESPACE"::internal::commands::widgetinitobjectoptions ",
		-1);
	Tcl_AppendToObj(objPtr, Tcl_GetString(ioPtr->varNsNamePtr), -1);
	Tcl_AppendToObj(objPtr, " ", -1);
	Tcl_AppendToObj(objPtr, Tcl_GetString(ioPtr->namePtr), -1);
	Tcl_AppendToObj(objPtr, " ", -1);
	Tcl_AppendToObj(objPtr, Tcl_GetString(iclsPtr->fullNamePtr), -1);
	Tcl_IncrRefCount(objPtr);
        result = Tcl_EvalObjEx(interp, objPtr, 0);
	Tcl_DecrRefCount(objPtr);
	if (result != TCL_OK) {
	    infoPtr->currIoPtr = saveCurrIoPtr;
	    result = TCL_ERROR;
            goto errorReturn;
	}
    }
    if (iclsPtr->flags & (ITCL_ECLASS|ITCL_TYPE|ITCL_WIDGETADAPTOR)) {
	/* FIXME have to check for hierarchy if ITCL_ECLASS !! */
	result = ItclCheckForInitializedComponents(interp, ioPtr->iclsPtr,
	        ioPtr);
	if (result != TCL_OK) {
            istate = Itcl_SaveInterpState(interp, result);
	    if (ioPtr->accessCmd != (Tcl_Command) NULL) {
	        Tcl_DeleteCommandFromToken(interp, ioPtr->accessCmd);
	        ioPtr->accessCmd = NULL;
	    }
            result = Itcl_RestoreInterpState(interp, istate);
	    /* need this for 2 ReleaseData at errorReturn!! */
            Itcl_PreserveData(ioPtr);
            goto errorReturn;
	}
    }

    /*
     *  Add it to the list of all known objects. The only
     *  tricky thing to watch out for is the case where the
     *  object deleted itself inside its own constructor.
     *  In that case, we don't want to add the object to
     *  the list of valid objects. We can determine that
     *  the object deleted itself by checking to see if
     *  its accessCmd member is NULL.
     */
    if (result == TCL_OK && (ioPtr->accessCmd != NULL))  {
	ClientData pmPtr;
	Tcl_Obj *namePtr;
	Tcl_Obj *argumentPtr;
	Tcl_Obj *bodyPtr;

	if (!(ioPtr->iclsPtr->flags & ITCL_CLASS)) {
	    result = DelegationInstall(interp, ioPtr, iclsPtr);
	    if (result != TCL_OK) {
		goto errorReturn;
	    }
	}
        hPtr = Tcl_CreateHashEntry(&iclsPtr->infoPtr->objectCmds,
                (char*)ioPtr->accessCmd, &newEntry);
        Tcl_SetHashValue(hPtr, (ClientData)ioPtr);
        hPtr = Tcl_CreateHashEntry(&iclsPtr->infoPtr->objects,
                (char*)ioPtr, &newEntry);
        Tcl_SetHashValue(hPtr, (ClientData)ioPtr);

        /* add the objects unknow command to handle all unknown sub commands */
	namePtr = Tcl_NewStringObj("unknown", -1);
	Tcl_IncrRefCount(namePtr);
	argumentPtr = Tcl_NewStringObj("args", -1);
	Tcl_IncrRefCount(argumentPtr);
	bodyPtr = Tcl_NewStringObj("uplevel 1 ::itcl::builtin::objectunknown ",
	        -1);
	Tcl_AppendToObj(bodyPtr, Tcl_GetString(ioPtr->namePtr), -1);
	Tcl_AppendToObj(bodyPtr, " $args", -1);
	Tcl_IncrRefCount(bodyPtr);
        Itcl_NewProcMethod(interp, ioPtr->oPtr, NULL, NULL, ItclProcErrorProc, 
	    (ItclMemberFunc *)ioPtr, namePtr, argumentPtr, bodyPtr, &pmPtr);
	Tcl_DecrRefCount(namePtr);
	Tcl_DecrRefCount(argumentPtr);
	Tcl_DecrRefCount(bodyPtr);
        if (iclsPtr->flags & (ITCL_TYPE|ITCL_WIDGETADAPTOR)) {
            Tcl_Obj *objPtr = Tcl_NewObj();
            Tcl_GetCommandFullName(interp, ioPtr->accessCmd, objPtr);
            if (iclsPtr->flags & ITCL_WIDGETADAPTOR) {
	        /* skip over the leading :: */
		char *objName;
		char *lastObjName;
		lastObjName = Tcl_GetString(objPtr);
		objName = lastObjName;
		while (1) {
		    objName = strstr(objName, "::");
		    if (objName == NULL) {
		        break;
		    }
		    objName += 2;
		    lastObjName = objName;
		}

                Tcl_AppendResult(interp, lastObjName, NULL);
            } else {
                Tcl_AppendResult(interp, Tcl_GetString(objPtr), NULL);
	    }
	    Tcl_DecrRefCount(objPtr);
	}
    } else {
        if (ioPtr->accessCmd != NULL) {
            hPtr = Tcl_FindHashEntry(&iclsPtr->infoPtr->objectCmds,
                (char*)ioPtr->accessCmd);
	    if (hPtr != NULL) {
                Tcl_DeleteHashEntry(hPtr);
	    }
        }
    }

    /*
     *  Release the object.  If it was destructed above, it will
     *  die at this point.
     */
    /*
     *  At this point, the object is fully constructed.
     *  Destroy the "constructed" table in the object data, since
     *  it is no longer needed.
     */
    if (infoPtr != NULL) {
        infoPtr->currIoPtr = saveCurrIoPtr;
    }
    Tcl_DeleteHashTable(ioPtr->constructed);
    ckfree((char*)ioPtr->constructed);
    ioPtr->constructed = NULL;
    ItclAddObjectsDictInfo(interp, ioPtr);
    Itcl_ReleaseData((ClientData)ioPtr);
    return result;

errorReturn:
    /*
     *  At this point, the object is not constructed as there was an error.
     *  Destroy the "constructed" table in the object data, since
     *  it is no longer needed.
     */
    if (infoPtr != NULL) {
        infoPtr->currIoPtr = saveCurrIoPtr;
    }
    if (ioPtr->constructed != NULL) {
        Tcl_DeleteHashTable(ioPtr->constructed);
        ckfree((char*)ioPtr->constructed);
        ioPtr->constructed = NULL;
    }
    ItclDeleteObjectVariablesNamespace(interp, ioPtr);
    Itcl_ReleaseData((ClientData)ioPtr);
    Itcl_ReleaseData((ClientData)ioPtr);
    return result;
}

/*
 * ------------------------------------------------------------------------
 *  ItclInitObjectCommands()
 *
 *  Init all instance commands.
 *  This is usually invoked automatically
 *  by Itcl_CreateObject(), when an object is created.
 * ------------------------------------------------------------------------
 */
static int
ItclInitObjectCommands(
   Tcl_Interp *interp,
   ItclObject *ioPtr,
   ItclClass *iclsPtr,
   const char *name)
{
#ifdef NEW_PROTO_RESOLVER
    Tcl_HashEntry *hPtr;
    Tcl_HashEntry *entry;
    Tcl_HashSearch place;
    Tcl_Command cmdPtr;
    Tcl_Obj *objPtr;
    Tcl_Namespace *nsPtr;
    ItclClass *iclsPtr2;
    ItclClass *lastIclsPtr;
    ItclHierIter hier;
    ItclMemberFunc *imPtr;
    ItclCmdLookup *clookup;
    ItclCmdLookup *info_clookup;

    info_clookup = NULL;
    lastIclsPtr = NULL;
    Tcl_ResetResult(interp);
    Itcl_InitHierIter(&hier, iclsPtr);
    iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    while (iclsPtr2 != NULL) {
        entry = Tcl_FirstHashEntry(&iclsPtr2->functions, &place);
        while (entry) {
            imPtr = (ItclMemberFunc *)Tcl_GetHashValue(entry);
	    hPtr = Tcl_FindHashEntry(&iclsPtr->resolveCmds,
	            (char *)imPtr->namePtr);
	    clookup = (ItclCmdLookup *)Tcl_GetHashValue(hPtr);
	    cmdPtr = imPtr->accessCmd;
            nsPtr = iclsPtr->nsPtr;
	    if ((imPtr->flags & ITCL_COMMON) == 0) {
		cmdPtr = Itcl_RegisterObjectCommand(interp, ioPtr,
		        Tcl_GetString(imPtr->namePtr), clookup->classCmdInfoPtr,
			cmdPtr, iclsPtr->nsPtr);
            } else {
		cmdPtr = Itcl_RegisterObjectCommand(interp, ioPtr,
		        Tcl_GetString(imPtr->namePtr), clookup->classCmdInfoPtr,
			cmdPtr, iclsPtr->nsPtr);
	    }
            entry = Tcl_NextHashEntry(&place);
        }
        lastIclsPtr = iclsPtr2;
        iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    }

    /* add some builtin functions to every class!! */
    Itcl_InitHierIter(&hier, iclsPtr);
    iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    while (iclsPtr2 != NULL) {
	objPtr = Tcl_NewStringObj("info", -1);
	hPtr = Tcl_FindHashEntry(&iclsPtr2->resolveCmds, objPtr);
	Tcl_DecrRefCount(objPtr);
	if (hPtr != NULL) {
	    clookup = (ItclCmdLookup *)Tcl_GetHashValue(hPtr);
	    cmdPtr = Itcl_RegisterObjectCommand(interp, ioPtr, "info",
	            clookup->classCmdInfoPtr, cmdPtr, iclsPtr->nsPtr);
	}
	objPtr = Tcl_NewStringObj("isa", -1);
	hPtr = Tcl_FindHashEntry(&iclsPtr2->resolveCmds, objPtr);
	Tcl_DecrRefCount(objPtr);
	if (hPtr != NULL) {
	    clookup = (ItclCmdLookup *)Tcl_GetHashValue(hPtr);
	    cmdPtr = Itcl_RegisterObjectCommand(interp, ioPtr, "isa",
	            clookup->classCmdInfoPtr, cmdPtr, iclsPtr->nsPtr);
	}
	objPtr = Tcl_NewStringObj("setget", -1);
	hPtr = Tcl_FindHashEntry(&iclsPtr2->resolveCmds, objPtr);
	Tcl_DecrRefCount(objPtr);
	if (hPtr != NULL) {
	    clookup = (ItclCmdLookup *)Tcl_GetHashValue(hPtr);
	    cmdPtr = Itcl_RegisterObjectCommand(interp, ioPtr, "setget",
	            clookup->classCmdInfoPtr, cmdPtr, iclsPtr->nsPtr);
	}
        iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    }
    Itcl_DeleteHierIter(&hier);
#endif
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  ItclInitObjectVariables()
 *
 *  Init all instance variables and create the necessary variable namespaces
 *  for the given object instance.  This is usually invoked automatically
 *  by Itcl_CreateObject(), when an object is created.
 * ------------------------------------------------------------------------
 */
static int
ItclInitObjectVariables(
   Tcl_Interp *interp,
   ItclObject *ioPtr,
   ItclClass *iclsPtr,
   const char *name)
{
    Tcl_DString buffer;
    Tcl_DString buffer2;
    Tcl_HashEntry *hPtr;
    Tcl_HashEntry *hPtr2;
    Tcl_HashSearch place;
    Tcl_Namespace *varNsPtr;
    Tcl_Namespace *varNsPtr2;
    Tcl_CallFrame frame;
    Tcl_Var varPtr;
    ItclClass *iclsPtr2;
    ItclHierIter hier;
    ItclVariable *ivPtr;
    ItclComponent *icPtr;
    ItclVarLookup *vlookup;
    const char *varName;
    const char *inheritComponentName;
    int itclOptionsIsSet;
    int isNew;
    int numInheritComponents;

    ivPtr = NULL;
    /*
     * create all the variables for each class in the
     * ::itcl::variables::<object>::<class> namespace as an
     * undefined variable using the Tcl "variable xx" command
     */
    numInheritComponents = 0;
    itclOptionsIsSet = 0;
    inheritComponentName = NULL;
    Itcl_InitHierIter(&hier, iclsPtr);
    iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    Tcl_ResetResult(interp);
    while (iclsPtr2 != NULL) {
	Tcl_DStringInit(&buffer);
	Tcl_DStringAppend(&buffer, ITCL_VARIABLES_NAMESPACE, -1);
	if ((name[0] != ':') && (name[1] != ':')) {
             Tcl_DStringAppend(&buffer, "::", 2);
	}
	Tcl_DStringAppend(&buffer, name, -1);
	Tcl_DStringAppend(&buffer, iclsPtr2->nsPtr->fullName, -1);
	varNsPtr = Tcl_FindNamespace(interp, Tcl_DStringValue(&buffer),
	        NULL, 0);
	if (varNsPtr == NULL) {
	    varNsPtr = Tcl_CreateNamespace(interp, Tcl_DStringValue(&buffer),
	            NULL, 0);
	}
	/* now initialize the variables which have an init value */
        if (Itcl_PushCallFrame(interp, &frame, varNsPtr,
                /*isProcCallFrame*/0) != TCL_OK) {
	    goto errorCleanup2;
        }
        hPtr = Tcl_FirstHashEntry(&iclsPtr2->variables, &place);
        while (hPtr) {
            ivPtr = (ItclVariable*)Tcl_GetHashValue(hPtr);
	    varName = Tcl_GetString(ivPtr->namePtr);
            if ((ivPtr->flags & ITCL_OPTIONS_VAR) && !itclOptionsIsSet) {
                /* this is the special code for the "itcl_options" variable */
		itclOptionsIsSet = 1;
                Tcl_DStringInit(&buffer2);
	        Tcl_DStringAppend(&buffer2, ITCL_VARIABLES_NAMESPACE, -1);
	        if ((name[0] != ':') && (name[1] != ':')) {
                     Tcl_DStringAppend(&buffer2, "::", 2);
	        }
	        Tcl_DStringAppend(&buffer2, name, -1);
	        varNsPtr2 = Tcl_FindNamespace(interp,
		        Tcl_DStringValue(&buffer2), NULL, 0);
	        if (varNsPtr2 == NULL) {
	            varNsPtr2 = Tcl_CreateNamespace(interp,
		            Tcl_DStringValue(&buffer2), NULL, 0);
	        }
                Tcl_DStringFree(&buffer2);
	        Itcl_PopCallFrame(interp);
	        /* now initialize the variables which have an init value */
                if (Itcl_PushCallFrame(interp, &frame, varNsPtr2,
                        /*isProcCallFrame*/0) != TCL_OK) {
		    goto errorCleanup2;
                }
                if (Tcl_SetVar2(interp, "itcl_options", "",
	                "", TCL_NAMESPACE_ONLY) == NULL) {
		    goto errorCleanup;
                }
                Tcl_TraceVar2(interp, "itcl_options",
                        NULL,
                        TCL_TRACE_READS|TCL_TRACE_WRITES,
                        ItclTraceOptionVar, (ClientData)ioPtr);
	        Itcl_PopCallFrame(interp);
                if (Itcl_PushCallFrame(interp, &frame, varNsPtr,
                        /*isProcCallFrame*/0) != TCL_OK) {
		    goto errorCleanup2;
                }
                hPtr = Tcl_NextHashEntry(&place);
	        continue;
            }
            if (ivPtr->flags & ITCL_COMPONENT_VAR) {
		hPtr2 = Tcl_FindHashEntry(&ivPtr->iclsPtr->components,
		        (char *)ivPtr->namePtr);
		if (hPtr2 == NULL) {
		    Tcl_AppendResult(interp, "cannot find component \"",
		            Tcl_GetString(ivPtr->namePtr), "\" in class \"",
			    Tcl_GetString(ivPtr->iclsPtr->namePtr), NULL);
		    goto errorCleanup;
		}
		icPtr = Tcl_GetHashValue(hPtr2);
		if (icPtr->flags & ITCL_COMPONENT_INHERIT) {
		    if (inheritComponentName != NULL) {
		        Tcl_AppendResult(interp, "object \"",
			        Tcl_GetString(ioPtr->namePtr),
				"\" can only have one component with inherit.",
				" Had already component \"",
				inheritComponentName,
				"\" now component \"",
				Tcl_GetString(icPtr->namePtr), "\"", NULL);
		        goto errorCleanup;
				
		    } else {
		        inheritComponentName = Tcl_GetString(icPtr->namePtr);
		    }
		}
                hPtr2 = Tcl_CreateHashEntry(&ioPtr->objectComponents,
                        (char *)ivPtr->namePtr, &isNew);
		if (isNew) {
		    Tcl_SetHashValue(hPtr2, icPtr);
		}
                /* this is a component variable */
		/* FIXME initialize it to the empty string */
		/* the initialization  is arguable, should it be done? */
                if (Tcl_SetVar2(interp, varName, NULL,
	                "", TCL_NAMESPACE_ONLY) == NULL) {
                    Tcl_AppendResult(interp, "INTERNAL ERROR cannot set",
		            " variable \"", varName, "\"\n", NULL);
		    goto errorCleanup;
                }
	    }
            hPtr2 = Tcl_FindHashEntry(&ivPtr->iclsPtr->resolveVars, varName);
            if (hPtr2 == NULL) {
                Tcl_Panic("before Itcl_RegisterObjectVariable hPtr2 == NULL!\n");
            }
	    vlookup = Tcl_GetHashValue(hPtr2);
	    if ((ivPtr->flags & ITCL_COMMON) == 0) {
                IctlVarTraceInfo *traceInfoPtr;
#ifndef NEW_PROTO_RESOLVER
                varPtr = Tcl_NewNamespaceVar(interp, varNsPtr,
                        Tcl_GetString(ivPtr->namePtr));
#else
		varPtr = Itcl_RegisterObjectVariable(interp, ioPtr,
		        Tcl_GetString(ivPtr->namePtr), vlookup->classVarInfoPtr,
			NULL, varNsPtr);
#endif
	        hPtr2 = Tcl_CreateHashEntry(&ioPtr->objectVariables,
		        (char *)ivPtr, &isNew);
	        if (isNew) {
		    Tcl_SetHashValue(hPtr2, varPtr);
		} else {
		}
                traceInfoPtr = (IctlVarTraceInfo *)ckalloc(
		        sizeof(IctlVarTraceInfo));
                memset (traceInfoPtr, 0, sizeof(IctlVarTraceInfo));
                traceInfoPtr->flags = ITCL_TRACE_OBJECT;
                traceInfoPtr->ioPtr = ioPtr;
                traceInfoPtr->iclsPtr = iclsPtr2;
                traceInfoPtr->ivPtr = ivPtr;
                Tcl_TraceVar2(interp, Tcl_GetString(ivPtr->namePtr), NULL,
                       TCL_TRACE_UNSETS, ItclTraceUnsetVar,
                       (ClientData)traceInfoPtr);
	        if (ivPtr->flags & (ITCL_THIS_VAR|ITCL_TYPE_VAR|
		        ITCL_SELF_VAR|ITCL_SELFNS_VAR|ITCL_WIN_VAR)) {
		    if (Tcl_SetVar2(interp, varName, NULL,
		        "", TCL_NAMESPACE_ONLY) == NULL) {
                        Tcl_AppendResult(interp, "INTERNAL ERROR cannot set",
			        " variable \"", varNsPtr->fullName, "::",
				varName, "\"\n", NULL);
		        goto errorCleanup;
	            }
	            Tcl_TraceVar2(interp, varName, NULL,
		        TCL_TRACE_READS|TCL_TRACE_WRITES, ItclTraceThisVar,
		        (ClientData)ioPtr);
	        } else {
	            if (ivPtr->flags & ITCL_HULL_VAR) {
	                Tcl_TraceVar2(interp, varName, NULL,
		            TCL_TRACE_READS|TCL_TRACE_WRITES,
			    ItclTraceItclHullVar,
		            (ClientData)ioPtr);
		    } else {
	              if (ivPtr->init != NULL) {
			if (Tcl_SetVar(interp,
			        Tcl_GetString(ivPtr->namePtr),
			        Tcl_GetString(ivPtr->init),
				TCL_NAMESPACE_ONLY) == NULL) {
			    goto errorCleanup;
	                }
	              }
	              if (ivPtr->arrayInitPtr != NULL) {
			Tcl_DString buffer3;
	                int i;
	                int argc;
	                const char **argv;
	                const char *val;
			int result;

			Tcl_DStringInit(&buffer3);
                        Tcl_DStringAppend(&buffer3, varNsPtr->fullName, -1);
                        Tcl_DStringAppend(&buffer3, "::", -1);
                        Tcl_DStringAppend(&buffer3,
			        Tcl_GetString(ivPtr->namePtr), -1);
	                result = Tcl_SplitList(interp,
			        Tcl_GetString(ivPtr->arrayInitPtr),
	                        &argc, &argv);
	                for (i = 0; i < argc; i++) {
                            val = Tcl_SetVar2(interp,
			            Tcl_DStringValue(&buffer3), argv[i],
                                    argv[i + 1], TCL_NAMESPACE_ONLY);
                            if (!val) {
                                Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                                    "cannot initialize variable \"",
                                    Tcl_GetString(ivPtr->namePtr), "\"",
                                    (char*)NULL);
                                return TCL_ERROR;
                            }
	                    i++;
                        }
			Tcl_DStringFree(&buffer3);
		        ckfree((char *)argv);
		        }
		      }
		    }
	        } else {
	            if (ivPtr->flags & ITCL_HULL_VAR) {
	                Tcl_TraceVar2(interp, varName, NULL,
		            TCL_TRACE_READS|TCL_TRACE_WRITES,
			    ItclTraceItclHullVar,
		            (ClientData)ioPtr);
		    }
	            hPtr2 = Tcl_FindHashEntry(&iclsPtr2->classCommons,
		            (char *)ivPtr);
		    if (hPtr2 == NULL) {
		        goto errorCleanup;
		    }
		    varPtr = Tcl_GetHashValue(hPtr2);
	            hPtr2 = Tcl_CreateHashEntry(&ioPtr->objectVariables,
		            (char *)ivPtr, &isNew);
	            if (isNew) {
		        Tcl_SetHashValue(hPtr2, varPtr);
		    } else {
#ifdef NEW_PROTO_RESOLVER
		    varPtr = Itcl_RegisterObjectVariable(interp, ioPtr,
		            Tcl_GetString(ivPtr->namePtr),
			    vlookup->classVarInfoPtr,
			    varPtr, varNsPtr);
#endif
	        }
	        if (ivPtr->flags & ITCL_COMPONENT_VAR) {
	            if (ivPtr->flags & ITCL_COMMON) {
                        Tcl_Obj *objPtr2;
		        objPtr2 = Tcl_NewStringObj(ITCL_VARIABLES_NAMESPACE,
			        -1);
                        Tcl_AppendToObj(objPtr2,
		                Tcl_GetString(ivPtr->iclsPtr->fullNamePtr), -1);
                        Tcl_AppendToObj(objPtr2, "::", -1);
                        Tcl_AppendToObj(objPtr2, varName, -1);
			/* itcl_hull is traced in itclParse.c */
			if (strcmp(varName, "itcl_hull") == 0) {
                            Tcl_TraceVar2(interp,
                                    Tcl_GetString(objPtr2), NULL,
	                            TCL_TRACE_WRITES, ItclTraceItclHullVar,
	                            (ClientData)ioPtr);
			} else {
                            Tcl_TraceVar2(interp,
                                    Tcl_GetString(objPtr2), NULL,
	                            TCL_TRACE_WRITES, ItclTraceComponentVar,
	                            (ClientData)ioPtr);
		        }
		        Tcl_DecrRefCount(objPtr2);
		    } else {
                        Tcl_TraceVar2(interp,
		                varName, NULL,
	                        TCL_TRACE_WRITES, ItclTraceComponentVar,
	                        (ClientData)ioPtr);
	            }
	        }
	    }
            hPtr = Tcl_NextHashEntry(&place);
        }
	Itcl_PopCallFrame(interp);
        iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    }
    Tcl_DStringFree(&buffer);
    Itcl_DeleteHierIter(&hier);
    return TCL_OK;
errorCleanup:
    Itcl_PopCallFrame(interp);
errorCleanup2:
    varNsPtr = Tcl_FindNamespace(interp, Tcl_GetString(ioPtr->varNsNamePtr),
            NULL, 0);
    if (varNsPtr != NULL) {
        Tcl_DeleteNamespace(varNsPtr);
    }
    return TCL_ERROR;
}

/*
 * ------------------------------------------------------------------------
 *  ItclInitObjectOptions()
 *
 *  Collect all instance options for the given object instance to allow
 *  faster runtime access to the options.
 *  if the same option name is used in more than one class the first one
 *  found is used (for initializing and for the class name)!!
 *  # It is assumed, that an option can only exist in one class??
 *  # So no duplicates allowed??
 *  This is usually invoked automatically by Itcl_CreateObject(),
 *  when an object is created.
 * ------------------------------------------------------------------------
 */
int
ItclInitObjectOptions(
   Tcl_Interp *interp,
   ItclObject *ioPtr,
   ItclClass *iclsPtr,
   const char *name)
{
    Tcl_DString buffer;
    Tcl_HashEntry *hPtr;
    Tcl_HashEntry *hPtr2;
    Tcl_HashSearch place;
    Tcl_CallFrame frame;
    Tcl_Namespace *varNsPtr;
    ItclClass *iclsPtr2;
    ItclHierIter hier;
    ItclOption *ioptPtr;
    ItclDelegatedOption *idoPtr;
    const char *itclOptionsName;
    int isNew;

    ioptPtr = NULL;
    Itcl_InitHierIter(&hier, iclsPtr);
    iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    while (iclsPtr2 != NULL) {
	/* now initialize the options which have an init value */
        hPtr = Tcl_FirstHashEntry(&iclsPtr2->options, &place);
        while (hPtr) {
            ioptPtr = (ItclOption*)Tcl_GetHashValue(hPtr);
	    hPtr2 = Tcl_CreateHashEntry(&ioPtr->objectOptions,
	            (char *)ioptPtr->namePtr, &isNew);
	    if (isNew) {
		Tcl_SetHashValue(hPtr2, ioptPtr);
                itclOptionsName = Tcl_GetString(ioptPtr->namePtr);
                Tcl_DStringInit(&buffer);
	        Tcl_DStringAppend(&buffer, ITCL_VARIABLES_NAMESPACE, -1);
	        if ((name[0] != ':') && (name[1] != ':')) {
                     Tcl_DStringAppend(&buffer, "::", 2);
	        }
	        Tcl_DStringAppend(&buffer, name, -1);
	        varNsPtr = Tcl_FindNamespace(interp,
		        Tcl_DStringValue(&buffer), NULL, 0);
	        if (varNsPtr == NULL) {
	            varNsPtr = Tcl_CreateNamespace(interp,
		            Tcl_DStringValue(&buffer), NULL, 0);
		}
                Tcl_DStringFree(&buffer);
	        /* now initialize the options which have an init value */
                if (Itcl_PushCallFrame(interp, &frame, varNsPtr,
                        /*isProcCallFrame*/0) != TCL_OK) {
                    return TCL_ERROR;
                }
		if ((ioptPtr != NULL) && (ioptPtr->namePtr != NULL) &&
		        (ioptPtr->defaultValuePtr != NULL)) {
                    if (Tcl_SetVar2(interp, "itcl_options",
		            Tcl_GetString(ioptPtr->namePtr),
	                    Tcl_GetString(ioptPtr->defaultValuePtr),
			    TCL_NAMESPACE_ONLY) == NULL) {
	                Itcl_PopCallFrame(interp);
		        return TCL_ERROR;
                    }
                    Tcl_TraceVar2(interp, "itcl_options",
                            NULL,
                            TCL_TRACE_READS|TCL_TRACE_WRITES,
                            ItclTraceOptionVar, (ClientData)ioPtr);
		}
	        Itcl_PopCallFrame(interp);
            }
            hPtr = Tcl_NextHashEntry(&place);
        }
	/* now check for options which are delegated */
        hPtr = Tcl_FirstHashEntry(&iclsPtr2->delegatedOptions, &place);
        while (hPtr) {
            idoPtr = (ItclDelegatedOption*)Tcl_GetHashValue(hPtr);
	    hPtr2 = Tcl_CreateHashEntry(&ioPtr->objectDelegatedOptions,
	            (char *)idoPtr->namePtr, &isNew);
	    if (isNew) {
		Tcl_SetHashValue(hPtr2, idoPtr);
	    }
            hPtr = Tcl_NextHashEntry(&place);
        }
        iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    }
    Itcl_DeleteHierIter(&hier);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  ItclInitObjectComponents()
 *
 *  Initialize all instance components with the empty string
 *  This is usually invoked automatically by Itcl_CreateObject(),
 *  when an object is created.
 * ------------------------------------------------------------------------
 */
int
ItclInitObjectComponents(
   Tcl_Interp *interp,
   ItclObject *ioPtr,
   ItclClass *iclsPtr,
   const char *name)
{
    Tcl_DString buffer;
    ItclClass *iclsPtr2;
    ItclHierIter hier;
    ItclComponent *icPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashEntry *hPtr2;
    Tcl_HashSearch place;
    Tcl_CallFrame frame;
    Tcl_Namespace *varNsPtr;
    const char *componentName;
    int isNew;

    icPtr = NULL;
    Itcl_InitHierIter(&hier, iclsPtr);
    iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    while (iclsPtr2 != NULL) {
	/* now initialize the options which have an init value */
        hPtr = Tcl_FirstHashEntry(&iclsPtr2->components, &place);
        while (hPtr) {
            icPtr = (ItclComponent *)Tcl_GetHashValue(hPtr);
	    hPtr2 = Tcl_CreateHashEntry(&ioPtr->objectComponents,
	            (char *)icPtr->namePtr, &isNew);
	    if (isNew) {
		Tcl_SetHashValue(hPtr2, icPtr);
                componentName = Tcl_GetString(icPtr->namePtr);
                Tcl_DStringInit(&buffer);
	        Tcl_DStringAppend(&buffer, ITCL_VARIABLES_NAMESPACE, -1);
	        if ((name[0] != ':') && (name[1] != ':')) {
                     Tcl_DStringAppend(&buffer, "::", 2);
	        }
	        Tcl_DStringAppend(&buffer, name, -1);
	        varNsPtr = Tcl_FindNamespace(interp,
		        Tcl_DStringValue(&buffer), NULL, 0);
	        if (varNsPtr == NULL) {
	            varNsPtr = Tcl_CreateNamespace(interp,
		            Tcl_DStringValue(&buffer), NULL, 0);
	        }
                Tcl_DStringFree(&buffer);
	        /* now initialize the options which have an init value */
                if (Itcl_PushCallFrame(interp, &frame, varNsPtr,
                        /*isProcCallFrame*/0) != TCL_OK) {
                    return TCL_ERROR;
                }
                if (Tcl_SetVar2(interp, Tcl_GetString(icPtr->namePtr),
		        NULL,
	                "",
			TCL_NAMESPACE_ONLY) == NULL) {
	            Itcl_PopCallFrame(interp);
		    return TCL_ERROR;
                }
                Tcl_TraceVar2(interp, Tcl_GetString(icPtr->namePtr),
                        NULL,
                        TCL_TRACE_READS|TCL_TRACE_WRITES,
                        ItclTraceComponentVar, (ClientData)ioPtr);
	        Itcl_PopCallFrame(interp);
            }
            hPtr = Tcl_NextHashEntry(&place);
        }
        iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    }
    Itcl_DeleteHierIter(&hier);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  ItclInitObjectMethodVariables()
 *
 *  Collect all instance methdovariables for the given object instance to allow
 *  faster runtime access to the methdovariables.
 *  This is usually invoked *  automatically by Itcl_CreateObject(),
 *  when an object is created.
 * ------------------------------------------------------------------------
 */
int
ItclInitObjectMethodVariables(
   Tcl_Interp *interp,
   ItclObject *ioPtr,
   ItclClass *iclsPtr,
   const char *name)
{
    ItclClass *iclsPtr2;
    ItclHierIter hier;
    ItclMethodVariable *imvPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashEntry *hPtr2;
    Tcl_HashSearch place;
    int isNew;

    imvPtr = NULL;
    Itcl_InitHierIter(&hier, iclsPtr);
    iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    while (iclsPtr2 != NULL) {
        hPtr = Tcl_FirstHashEntry(&iclsPtr2->methodVariables, &place);
        while (hPtr) {
            imvPtr = (ItclMethodVariable*)Tcl_GetHashValue(hPtr);
	    hPtr2 = Tcl_CreateHashEntry(&ioPtr->objectMethodVariables,
	            (char *)imvPtr->namePtr, &isNew);
	    if (isNew) {
		Tcl_SetHashValue(hPtr2, imvPtr);
            }
            hPtr = Tcl_NextHashEntry(&place);
        }
        iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    }
    Itcl_DeleteHierIter(&hier);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  Itcl_DeleteObject()
 *
 *  Attempts to delete an object by invoking its destructor.
 *
 *  If the destructor is successful, then the object is deleted by
 *  removing its access command, and this procedure returns TCL_OK.
 *  Otherwise, the object will remain alive, and this procedure
 *  returns TCL_ERROR (along with an error message in the interpreter).
 * ------------------------------------------------------------------------
 */
int
Itcl_DeleteObject(
    Tcl_Interp *interp,      /* interpreter mananging object */
    ItclObject *contextIoPtr)  /* object to be deleted */
{
    Tcl_CmdInfo cmdInfo;
    Tcl_HashEntry *hPtr;

    Tcl_GetCommandInfoFromToken(contextIoPtr->accessCmd, &cmdInfo);

    contextIoPtr->flags |= ITCL_OBJECT_IS_DELETED;
    Itcl_PreserveData((ClientData)contextIoPtr);

    /*
     *  Invoke the object's destructors.
     */
    if (Itcl_DestructObject(interp, contextIoPtr, 0) != TCL_OK) {
        Itcl_ReleaseData((ClientData)contextIoPtr);
	contextIoPtr->flags |=
	        ITCL_TCLOO_OBJECT_IS_DELETED|ITCL_OBJECT_DESTRUCT_ERROR;
        return TCL_ERROR;
    }
    /*
     *  Remove the object from the global list.
     */
    hPtr = Tcl_FindHashEntry(&contextIoPtr->infoPtr->objects,
        (char*)contextIoPtr);

    if (hPtr) {
        Tcl_DeleteHashEntry(hPtr);
    }

    /*
     *  Change the object's access command so that it can be
     *  safely deleted without attempting to destruct the object
     *  again.  Then delete the access command.  If this is
     *  the last use of the object data, the object will die here.
     */
    if ((contextIoPtr->accessCmd != NULL) && (!(contextIoPtr->flags &
            (ITCL_OBJECT_IS_RENAMED)))) {
    if (Tcl_GetCommandInfoFromToken(contextIoPtr->accessCmd, &cmdInfo) == 1) {
        cmdInfo.deleteProc = Itcl_ReleaseData;
	Tcl_SetCommandInfoFromToken(contextIoPtr->accessCmd, &cmdInfo);

        Tcl_DeleteCommandFromToken(interp, contextIoPtr->accessCmd);
    }
    }
    contextIoPtr->oPtr = NULL;
    contextIoPtr->accessCmd = NULL;

    Itcl_ReleaseData((ClientData)contextIoPtr);  /* object should die here */

    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  ItclDeleteObjectVariablesNamespace()
 *
 * ------------------------------------------------------------------------
 */
void
ItclDeleteObjectVariablesNamespace(
    Tcl_Interp *interp,
    ItclObject *ioPtr)
{
    Tcl_Namespace *varNsPtr;

    if (ioPtr->callRefCount < 1) {
        /* free the object's variables namespace and variables in it */
	ioPtr->flags &= ~ITCL_OBJECT_SHOULD_VARNS_DELETE;
        varNsPtr = Tcl_FindNamespace(interp, Tcl_GetString(ioPtr->varNsNamePtr),
	        NULL, 0);
        if (varNsPtr != NULL) {
            Tcl_DeleteNamespace(varNsPtr);
        }
    } else {
        ioPtr->flags |= ITCL_OBJECT_SHOULD_VARNS_DELETE;
    }
}

static int
FinalizeDeleteObject(
    ClientData data[],
    Tcl_Interp *interp,
    int result)
{
    ItclObject *contextIoPtr = data[0];
    ItclDeleteObjectVariablesNamespace(interp, contextIoPtr);
    if (result == TCL_OK) {
        Tcl_ResetResult(interp);
    }

    Tcl_DeleteHashTable(contextIoPtr->destructed);
    ckfree((char*)contextIoPtr->destructed);
    contextIoPtr->destructed = NULL;
    return result;
}

static int
CallDestructBase(
    ClientData data[],
    Tcl_Interp *interp,
    int result)
{
    Tcl_Obj *objPtr;
    ItclObject *contextIoPtr = data[0];
    int flags = PTR2INT(data[1]);

    if (result != TCL_OK) {
        return result;
    }
    result = ItclDestructBase(interp, contextIoPtr, contextIoPtr->iclsPtr,
            flags);
    if (result != TCL_OK) {
        return result;
    }
    /* destroy the hull */
    if (contextIoPtr->hullWindowNamePtr != NULL) {
        objPtr = Tcl_NewStringObj("destroy ", -1);
        Tcl_AppendToObj(objPtr,
	        Tcl_GetString(contextIoPtr->hullWindowNamePtr), -1);
        result = Tcl_EvalObjEx(interp, objPtr, 0);
    }
    return result;
}
/*
 * ------------------------------------------------------------------------
 *  Itcl_DestructObject()
 *
 *  Invokes the destructor for a particular object.  Usually invoked
 *  by Itcl_DeleteObject() or Itcl_DestroyObject() as a part of the
 *  object destruction process.  If the ITCL_IGNORE_ERRS flag is
 *  included, all destructors are invoked even if errors are
 *  encountered, and the result will always be TCL_OK.
 *
 *  Returns TCL_OK on success, or TCL_ERROR (along with an error
 *  message in the interpreter) if anything goes wrong.
 * ------------------------------------------------------------------------
 */
int
Itcl_DestructObject(
    Tcl_Interp *interp,      /* interpreter mananging new object */
    ItclObject *contextIoPtr,  /* object to be destructed */
    int flags)               /* flags: ITCL_IGNORE_ERRS */
{
    int result;

    if ((contextIoPtr->flags & (ITCL_OBJECT_IS_DESTRUCTED))) {
            return TCL_OK;
    }
    contextIoPtr->flags |= ITCL_OBJECT_IS_DESTRUCTED;
    /*
     *  If there is a "destructed" table, then this object is already
     *  being destructed.  Flag an error, unless errors are being
     *  ignored.
     */
    if (contextIoPtr->destructed) {
        if ((flags & ITCL_IGNORE_ERRS) == 0) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "can't delete an object while it is being destructed",
                (char*)NULL);
            return TCL_ERROR;
        }
        return TCL_OK;
    }

    result = TCL_OK;
    if (contextIoPtr->oPtr != NULL) {
        void *callbackPtr;
        /*
         *  Create a "destructed" table to keep track of which destructors
         *  have been invoked.  This is used in ItclDestructBase to make
         *  sure that all base class destructors have been called,
         *  explicitly or implicitly.
         */
        contextIoPtr->destructed = (Tcl_HashTable*)ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitObjHashTable(contextIoPtr->destructed);

        /*
         *  Destruct the object starting from the most-specific class.
         *  If all goes well, return the null string as the result.
         */
        callbackPtr = Itcl_GetCurrentCallbackPtr(interp);
        Itcl_NRAddCallback(interp, FinalizeDeleteObject, contextIoPtr,
	        NULL, NULL, NULL);
        Itcl_NRAddCallback(interp, CallDestructBase, contextIoPtr,
	        INT2PTR(flags), NULL, NULL);
        result = Itcl_NRRunCallbacks(interp, callbackPtr);
    }
    return result;
}

/*
 * ------------------------------------------------------------------------
 *  ItclDestructBase()
 *
 *  Invoked by Itcl_DestructObject() to recursively destruct an object
 *  from the specified class level.  Finds and invokes the destructor
 *  for the specified class, and then recursively destructs all base
 *  classes.  If the ITCL_IGNORE_ERRS flag is included, all destructors
 *  are invoked even if errors are encountered, and the result will
 *  always be TCL_OK.
 *
 *  Returns TCL_OK on success, or TCL_ERROR (along with an error message
 *  in interp->result) on error.
 * ------------------------------------------------------------------------
 */
static int
ItclDestructBase(
    Tcl_Interp *interp,         /* interpreter */
    ItclObject *contextIoPtr,   /* object being destructed */
    ItclClass *contextIclsPtr,  /* current class being destructed */
    int flags)                  /* flags: ITCL_IGNORE_ERRS */
{
    int result;
    Itcl_ListElem *elem;
    ItclClass *iclsPtr;

    if (contextIoPtr->flags & ITCL_OBJECT_CLASS_DESTRUCTED) {
        return TCL_OK;
    }
    /*
     *  Look for a destructor in this class, and if found,
     *  invoke it.
     */
    if (Tcl_FindHashEntry(contextIoPtr->destructed,
            (char *)contextIclsPtr->namePtr) == NULL) {
        result = Itcl_InvokeMethodIfExists(interp, "destructor",
            contextIclsPtr, contextIoPtr, 0, (Tcl_Obj* const*)NULL);
        if (result != TCL_OK) {
            return TCL_ERROR;
        }
    }

    /*
     *  Scan through the list of base classes recursively and destruct
     *  them.  Traverse the list in normal order, so that we destruct
     *  from most- to least-specific.
     */
    elem = Itcl_FirstListElem(&contextIclsPtr->bases);
    while (elem) {
        iclsPtr = (ItclClass*)Itcl_GetListValue(elem);

        if (ItclDestructBase(interp, contextIoPtr, iclsPtr, flags) != TCL_OK) {
            return TCL_ERROR;
        }
        elem = Itcl_NextListElem(elem);
    }

    /*
     *  Throw away any result from the destructors and return.
     */
    Tcl_ResetResult(interp);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  Itcl_FindObject()
 *
 *  Searches for an object with the specified name, which have
 *  namespace scope qualifiers like "namesp::namesp::name", or may
 *  be a scoped value such as "namespace inscope ::foo obj".
 *
 *  If an error is encountered, this procedure returns TCL_ERROR
 *  along with an error message in the interpreter.  Otherwise, it
 *  returns TCL_OK.  If an object was found, "roPtr" returns a
 *  pointer to the object data.  Otherwise, it returns NULL.
 * ------------------------------------------------------------------------
 */
int
Itcl_FindObject(
    Tcl_Interp *interp,      /* interpreter containing this object */
    const char *name,        /* name of the object */
    ItclObject **roPtr)      /* returns: object data or NULL */
{
    Tcl_Command cmd;
    Tcl_CmdInfo cmdInfo;
    Tcl_Namespace *contextNs;
    char *cmdName;

    contextNs = NULL;
    cmdName = NULL;
    /*
     *  The object name may be a scoped value of the form
     *  "namespace inscope <namesp> <command>".  If it is,
     *  decode it.
     */
    if (Itcl_DecodeScopedCommand(interp, name, &contextNs, &cmdName)
            != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Look for the object's access command, and see if it has
     *  the appropriate command handler.
     */
    cmd = Tcl_FindCommand(interp, cmdName, contextNs, /* flags */ 0);
    if (cmd != NULL && Itcl_IsObject(cmd)) {
        if (Tcl_GetCommandInfoFromToken(cmd, &cmdInfo) != 1) {
            *roPtr = NULL;
        }
        *roPtr = cmdInfo.deleteData;
    } else {
        *roPtr = NULL;
    }

    ckfree(cmdName);

    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_IsObject()
 *
 *  Checks the given Tcl command to see if it represents an itcl object.
 *  Returns non-zero if the command is associated with an object.
 * ------------------------------------------------------------------------
 */
int
Itcl_IsObject(
    Tcl_Command cmd)         /* command being tested */
{
    Tcl_CmdInfo cmdInfo;

    if (Tcl_GetCommandInfoFromToken(cmd, &cmdInfo) != 1) {
        return 0;
    }

    if ((void *)cmdInfo.deleteProc == (void *)ItclDestroyObject) {
        return 1;
    }

    /*
     *  This may be an imported command.  Try to get the real
     *  command and see if it represents an object.
     */
    cmd = Tcl_GetOriginalCommand(cmd);
    if (cmd != NULL) {
        if (Tcl_GetCommandInfoFromToken(cmd, &cmdInfo) != 1) {
            return 0;
        }

        if ((void *)cmdInfo.deleteProc == (void *)ItclDestroyObject) {
            return 1;
        }
    }
    return 0;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_ObjectIsa()
 *
 *  Checks to see if an object belongs to the given class.  An object
 *  "is-a" member of the class if the class appears anywhere in its
 *  inheritance hierarchy.  Returns non-zero if the object belongs to
 *  the class, and zero otherwise.
 * ------------------------------------------------------------------------
 */
int
Itcl_ObjectIsa(
    ItclObject *contextIoPtr, /* object being tested */
    ItclClass *iclsPtr)       /* class to test for "is-a" relationship */
{
    Tcl_HashEntry *entry;
    entry = Tcl_FindHashEntry(&contextIoPtr->iclsPtr->heritage, (char*)iclsPtr);
    return (entry != NULL);
}

/*
 * ------------------------------------------------------------------------
 *  ItclGetInstanceVar()
 *
 *  Returns the current value for an object data member.  The member
 *  name is interpreted with respect to the given class scope, which
 *  is usually the most-specific class for the object.
 *
 *  If successful, this procedure returns a pointer to a string value
 *  which remains alive until the variable changes it value.  If
 *  anything goes wrong, this returns NULL.
 * ------------------------------------------------------------------------
 */
const char*
ItclGetInstanceVar(
    Tcl_Interp *interp,        /* current interpreter */
    const char *name1,         /* name of desired instance variable */
    const char *name2,         /* array element or NULL */
    ItclObject *contextIoPtr,  /* current object */
    ItclClass *contextIclsPtr) /* name is interpreted in this scope */
{
    Tcl_HashEntry *hPtr;
    Tcl_CallFrame frame;
    Tcl_CallFrame *framePtr;
    Tcl_Namespace *nsPtr;
    Tcl_DString buffer;
    ItclClass *iclsPtr;
    ItclVariable *ivPtr;
    ItclVarLookup *vlookup;
    const char *val;
    int isItclOptions;
    int doAppend;

    /*
     *  Make sure that the current namespace context includes an
     *  object that is being manipulated.
     */
    if (contextIoPtr == NULL) {
        Tcl_ResetResult(interp);
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "cannot access object-specific info without an object context",
            (char*)NULL);
        return NULL;
    }

    /* get the variable definition to check if that is an ITCL_COMMON */
    if (contextIclsPtr == NULL) {
        iclsPtr = contextIoPtr->iclsPtr;
    } else {
        iclsPtr = contextIclsPtr;
    }
    ivPtr = NULL;
    hPtr = Tcl_FindHashEntry(&iclsPtr->resolveVars, (char *)name1);
    if (hPtr != NULL) {
        vlookup = Tcl_GetHashValue(hPtr);
        ivPtr = vlookup->ivPtr;
    } else {
    }
    /*
     *  Install the object context and access the data member
     *  like any other variable.
     */
    isItclOptions = 0;
    if (strcmp(name1, "itcl_options") == 0) {
        isItclOptions = 1;
    }
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, Tcl_GetString(contextIoPtr->varNsNamePtr), -1);
    doAppend = 1;
    if ((contextIclsPtr == NULL) || (contextIclsPtr->flags &
            (ITCL_ECLASS|ITCL_TYPE|ITCL_WIDGET|ITCL_WIDGETADAPTOR))) {
        if (isItclOptions) {
	    doAppend = 0;
        }
    }
    if ((ivPtr != NULL) && (ivPtr->flags & ITCL_COMMON)) {
	if (!isItclOptions) {
            Tcl_DStringSetLength(&buffer, 0);
	    if (ivPtr->protection != ITCL_PUBLIC) {
	        Tcl_DStringAppend(&buffer, ITCL_VARIABLES_NAMESPACE, -1);
	    }
	    doAppend = 1;
        }
    }
    if (doAppend) {
        Tcl_DStringAppend(&buffer,
                Tcl_GetString(contextIclsPtr->fullNamePtr), -1);
    }
    nsPtr = Tcl_FindNamespace(interp, Tcl_DStringValue(&buffer), NULL, 0);
    Tcl_DStringFree(&buffer);
    val = NULL;
    if (nsPtr != NULL) {
	framePtr = &frame;
	Itcl_PushCallFrame(interp, framePtr, nsPtr, /*isProcCallFrame*/0);
        val = Tcl_GetVar2(interp, (const char *)name1, (char*)name2,
	        TCL_LEAVE_ERR_MSG);
        Itcl_PopCallFrame(interp);
    }

    return val;
}

/*
 * ------------------------------------------------------------------------
 *  ItclGetCommonInstanceVar()
 *
 *  Returns the current value for an object data member.  The member
 *  name is interpreted with respect to the given class scope, which
 *  is usually the most-specific class for the object.
 *
 *  If successful, this procedure returns a pointer to a string value
 *  which remains alive until the variable changes it value.  If
 *  anything goes wrong, this returns NULL.
 * ------------------------------------------------------------------------
 */
const char*
ItclGetCommonInstanceVar(
    Tcl_Interp *interp,        /* current interpreter */
    const char *name1,         /* name of desired instance variable */
    const char *name2,         /* array element or NULL */
    ItclObject *contextIoPtr,  /* current object */
    ItclClass *contextIclsPtr) /* name is interpreted in this scope */
{
    Tcl_CallFrame frame;
    Tcl_CallFrame *framePtr;
    Tcl_Namespace *nsPtr;
    Tcl_DString buffer;
    const char *val;
    int doAppend;

    /*
     *  Make sure that the current namespace context includes an
     *  object that is being manipulated.
     */
    if (contextIoPtr == NULL) {
        Tcl_ResetResult(interp);
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "cannot access object-specific info without an object context",
            (char*)NULL);
        return NULL;
    }

    /*
     *  Install the object context and access the data member
     *  like any other variable.
     */
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, ITCL_VARIABLES_NAMESPACE, -1);
    doAppend = 1;
    if ((contextIclsPtr == NULL) || (contextIclsPtr->flags &
            (ITCL_ECLASS|ITCL_TYPE|ITCL_WIDGETADAPTOR))) {
        if (strcmp(name1, "itcl_options") == 0) {
	    doAppend = 0;
        }
    }
    if (doAppend) {
        Tcl_DStringAppend(&buffer,
                Tcl_GetString(contextIclsPtr->fullNamePtr), -1);
    }
    nsPtr = Tcl_FindNamespace(interp, Tcl_DStringValue(&buffer), NULL, 0);
    Tcl_DStringFree(&buffer);
    val = NULL;
    if (nsPtr != NULL) {
	framePtr = &frame;
	Itcl_PushCallFrame(interp, framePtr, nsPtr, /*isProcCallFrame*/0);
        val = Tcl_GetVar2(interp, (const char *)name1, (char*)name2,
	        TCL_LEAVE_ERR_MSG);
        Itcl_PopCallFrame(interp);
    }

    return val;
}

/*
 * ------------------------------------------------------------------------
 *  Itcl_GetInstanceVar()
 *
 *  Returns the current value for an object data member.  The member
 *  name is interpreted with respect to the given class scope, which
 *  is usually the most-specific class for the object.
 *
 *  If successful, this procedure returns a pointer to a string value
 *  which remains alive until the variable changes it value.  If
 *  anything goes wrong, this returns NULL.
 * ------------------------------------------------------------------------
 */
const char*
Itcl_GetInstanceVar(
    Tcl_Interp *interp,        /* current interpreter */
    const char *name,          /* name of desired instance variable */
    ItclObject *contextIoPtr,  /* current object */
    ItclClass *contextIclsPtr) /* name is interpreted in this scope */
{
    return ItclGetInstanceVar(interp, name, NULL, contextIoPtr,
            contextIclsPtr);
}

/*
 * ------------------------------------------------------------------------
 *  ItclSetInstanceVar()
 *
 *  Sets the current value for an object data member.  The member
 *  name is interpreted with respect to the given class scope, which
 *  is usually the most-specific class for the object.
 *
 *  If successful, this procedure returns a pointer to a string value
 *  which remains alive until the variable changes it value.  If
 *  anything goes wrong, this returns NULL.
 * ------------------------------------------------------------------------
 */
const char*
ItclSetInstanceVar(
    Tcl_Interp *interp,        /* current interpreter */
    const char *name1,         /* name of desired instance variable */
    const char *name2,         /* array member or NULL */
    const char *value,         /* the value to set */
    ItclObject *contextIoPtr,  /* current object */
    ItclClass *contextIclsPtr) /* name is interpreted in this scope */
{
    Tcl_HashEntry *hPtr;
    Tcl_CallFrame frame;
    Tcl_CallFrame *framePtr;
    Tcl_Namespace *nsPtr;
    Tcl_DString buffer;
    ItclVariable *ivPtr;
    ItclVarLookup *vlookup;
    ItclClass *iclsPtr;
    const char *val;
    int isItclOptions;
    int doAppend;

    ivPtr = NULL;
    /*
     *  Make sure that the current namespace context includes an
     *  object that is being manipulated.
     */
    if (contextIoPtr == NULL) {
        Tcl_ResetResult(interp);
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "cannot access object-specific info without an object context",
            (char*)NULL);
        return NULL;
    }
    /* get the variable definition to check if that is an ITCL_COMMON */
    if (contextIclsPtr == NULL) {
        iclsPtr = contextIoPtr->iclsPtr;
    } else {
        iclsPtr = contextIclsPtr;
    }
    hPtr = Tcl_FindHashEntry(&iclsPtr->resolveVars, (char *)name1);
    if (hPtr != NULL) {
        vlookup = Tcl_GetHashValue(hPtr);
        ivPtr = vlookup->ivPtr;
    } else {
        return NULL;
    }
    /*
     *  Install the object context and access the data member
     *  like any other variable.
     */
    isItclOptions = 0;
    if (strcmp(name1, "itcl_options") == 0) {
        isItclOptions = 1;
    }
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, Tcl_GetString(contextIoPtr->varNsNamePtr), -1);
    doAppend = 1;
    if ((contextIclsPtr == NULL) ||
            (contextIclsPtr->flags & (ITCL_ECLASS|ITCL_TYPE|
	    ITCL_WIDGET|ITCL_WIDGETADAPTOR))) {
        if (isItclOptions) {
	    doAppend = 0;
        }
    }
    if (ivPtr->flags & ITCL_COMMON) {
	if (!isItclOptions) {
            Tcl_DStringSetLength(&buffer, 0);
	    if (ivPtr->protection != ITCL_PUBLIC) {
	        Tcl_DStringAppend(&buffer, ITCL_VARIABLES_NAMESPACE, -1);
	    }
	    doAppend = 1;
        }
    }
    if (doAppend) {
        Tcl_DStringAppend(&buffer,
                Tcl_GetString(contextIclsPtr->fullNamePtr), -1);
    }
    nsPtr = Tcl_FindNamespace(interp, Tcl_DStringValue(&buffer), NULL, 0);
    Tcl_DStringFree(&buffer);
    val = NULL;
    if (nsPtr != NULL) {
	framePtr = &frame;
	Itcl_PushCallFrame(interp, framePtr, nsPtr, /*isProcCallFrame*/0);
        val = Tcl_SetVar2(interp, (const char *)name1, (char*)name2,
	        value, TCL_LEAVE_ERR_MSG);
        Itcl_PopCallFrame(interp);
    }

    return val;
}

/*
 * ------------------------------------------------------------------------
 *  ItclReportObjectUsage()
 *
 *  Appends information to the given interp summarizing the usage
 *  for all of the methods available for this object.  Useful when
 *  reporting errors in Itcl_HandleInstance().
 * ------------------------------------------------------------------------
 */
void
ItclReportObjectUsage(
    Tcl_Interp *interp,           /* current interpreter */
    ItclObject *contextIoPtr,     /* current object */
    Tcl_Namespace *callerNsPtr,
    Tcl_Namespace *contextNsPtr)  /* the context namespace */
{
    Tcl_Obj *namePtr;
    Tcl_HashEntry *entry;
    Tcl_HashSearch place;
    Tcl_Obj *resultPtr;
    ItclClass *iclsPtr;
    Itcl_List cmdList;
    Itcl_ListElem *elem;
    ItclMemberFunc *imPtr;
    ItclMemberFunc *cmpFunc;
    ItclCmdLookup *clookup;
    char *name;
    int ignore;
    int cmp;

    iclsPtr = (ItclClass*)contextIoPtr->iclsPtr;
    ignore = ITCL_CONSTRUCTOR | ITCL_DESTRUCTOR | ITCL_COMMON;
    /*
     *  Scan through all methods in the virtual table and sort
     *  them in alphabetical order.  Report only the methods
     *  that have simple names (no ::'s) and are accessible.
     */
    Itcl_InitList(&cmdList);
    entry = Tcl_FirstHashEntry(&iclsPtr->resolveCmds, &place);
    while (entry) {
        namePtr  = (Tcl_Obj *)Tcl_GetHashKey(&iclsPtr->resolveCmds, entry);
	name = Tcl_GetString(namePtr);
	clookup = (ItclCmdLookup *)Tcl_GetHashValue(entry);
	imPtr = clookup->imPtr;

        if (strstr(name,"::") || (imPtr->flags & ignore) != 0) {
            imPtr = NULL;
        } else {
	    if (imPtr->protection != ITCL_PUBLIC) {
		if (contextNsPtr != NULL) {
                    if (!Itcl_CanAccessFunc(imPtr, contextNsPtr)) {
                        imPtr = NULL;
                    }
                }
	    }
        }
        if ((imPtr != NULL) && (imPtr->codePtr != NULL)) {
	    if (imPtr->codePtr->flags & ITCL_BUILTIN) {
	        char *body;
	        if (imPtr->codePtr != NULL) {
	            body = Tcl_GetString(imPtr->codePtr->bodyPtr);
	            if (*body == '@') {
                        if (strcmp(body, "@itcl-builtin-info") == 0) {
	                    imPtr = NULL;
		        }
                        if (strcmp(body, "@itcl-builtin-setget") == 0) {
			    if (!(imPtr->iclsPtr->flags & ITCL_ECLASS)) {
	                        imPtr = NULL;
			    }
		        }
                        if (strcmp(body, "@itcl-builtin-installcomponent")
			        == 0) {
			    if (!(imPtr->iclsPtr->flags &
			            (ITCL_WIDGET|ITCL_WIDGETADAPTOR))) {
	                        imPtr = NULL;
			    }
		        }
	            }
	        }
	    }
	}

        if (imPtr) {
            elem = Itcl_FirstListElem(&cmdList);
            while (elem) {
                cmpFunc = (ItclMemberFunc*)Itcl_GetListValue(elem);
                cmp = strcmp(Tcl_GetString(imPtr->namePtr),
		        Tcl_GetString(cmpFunc->namePtr));
                if (cmp < 0) {
                    Itcl_InsertListElem(elem, (ClientData)imPtr);
                    imPtr = NULL;
                    break;
                } else {
		    if (cmp == 0) {
                        imPtr = NULL;
                        break;
		    }
                }
                elem = Itcl_NextListElem(elem);
            }
            if (imPtr) {
                Itcl_AppendList(&cmdList, (ClientData)imPtr);
            }
        }
        entry = Tcl_NextHashEntry(&place);
    }

    /*
     *  Add a series of statements showing usage info.
     */
    resultPtr = Tcl_GetObjResult(interp);
    elem = Itcl_FirstListElem(&cmdList);
    while (elem) {
        imPtr = (ItclMemberFunc*)Itcl_GetListValue(elem);
        Tcl_AppendToObj(resultPtr, "\n  ", -1);
        Itcl_GetMemberFuncUsage(imPtr, contextIoPtr, resultPtr);

        elem = Itcl_NextListElem(elem);
    }
    Itcl_DeleteList(&cmdList);
}

/*
 * ------------------------------------------------------------------------
 *  ItclTraceThisVar()
 *
 *  Invoked to handle read/write traces on the "this" variable built
 *  into each object.
 *
 *  On read, this procedure updates the "this" variable to contain the
 *  current object name.  This is done dynamically, since an object's
 *  identity can change if its access command is renamed.
 *
 *  On write, this procedure returns an error string, warning that
 *  the "this" variable cannot be set.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static char*
ItclTraceThisVar(
    ClientData cdata,	    /* object instance data */
    Tcl_Interp *interp,	    /* interpreter managing this variable */
    const char *name1,    /* variable name */
    const char *name2,    /* unused */
    int flags)		    /* flags indicating read/write */
{
    ItclObject *contextIoPtr = (ItclObject*)cdata;
    const char *objName;
    Tcl_Obj *objPtr;
    int isDone;

    /*
     *  Handle read traces on "this"
     */
    if ((flags & TCL_TRACE_READS) != 0) {
        objPtr = Tcl_NewStringObj("", -1);
	if (strcmp(name1, "this") == 0) {
            if (contextIoPtr->accessCmd) {
                Tcl_GetCommandFullName(contextIoPtr->iclsPtr->interp,
                    contextIoPtr->accessCmd, objPtr);
            }

	} else {
	    isDone = 0;
	    if (strcmp(name1, "type") == 0) {
		Tcl_SetStringObj(objPtr,
		        Tcl_GetCurrentNamespace(
			contextIoPtr->iclsPtr->interp)->fullName, -1);
	        isDone = 1;
	    }
	    if (!isDone && (strcmp(name1, "self") == 0)) {
		if (contextIoPtr->iclsPtr->flags &
		        (ITCL_WIDGET|ITCL_WIDGETADAPTOR)) {
		    const char *objectName;

		    objectName = ItclGetInstanceVar(
			    contextIoPtr->iclsPtr->interp,
			    "itcl_hull", NULL, contextIoPtr,
			    contextIoPtr->iclsPtr);
		    if (strlen(objectName) == 0) {
			objPtr = contextIoPtr->namePtr;
			Tcl_IncrRefCount(objPtr);
		    } else {
		        Tcl_SetStringObj(objPtr, objectName, -1);
		    }
		} else {
                    Tcl_GetCommandFullName(contextIoPtr->iclsPtr->interp,
                            contextIoPtr->accessCmd, objPtr);
		}
	        isDone = 1;
	    }
	    if (!isDone && (strcmp(name1, "selfns") == 0)) {
		Tcl_SetStringObj(objPtr,
		        Tcl_GetString(contextIoPtr->varNsNamePtr), -1);
		Tcl_AppendToObj(objPtr,
		        Tcl_GetString(contextIoPtr->iclsPtr->fullNamePtr), -1);
	        isDone = 1;
            }
	    if (!isDone && (strcmp(name1, "win") == 0)) {
		Tcl_SetStringObj(objPtr,
                        Tcl_GetString(contextIoPtr->origNamePtr)+2, -1);
	        isDone = 1;
	    }
	    if (!isDone && (contextIoPtr->iclsPtr->flags & ITCL_WIDGET)) {
	        /* thiswidget variable */
	        Tcl_SetStringObj(objPtr, Tcl_GetCommandName(
		        contextIoPtr->iclsPtr->interp,
			contextIoPtr->accessCmd), -1);
	    }
	}
        objName = Tcl_GetString(objPtr);
        Tcl_SetVar(interp, (const char *)name1, objName, 0);

        Tcl_DecrRefCount(objPtr);
        return NULL;
    }

    /*
     *  Handle write traces on "this"
     */
    if ((flags & TCL_TRACE_WRITES) != 0) {
	if (strcmp(name1, "this") == 0) {
            return "variable \"this\" cannot be modified";
	}
	if (strcmp(name1, "win") == 0) {
	    if (!(contextIoPtr->iclsPtr->flags & ITCL_ECLASS)) {
                return "variable \"win\" cannot be modified";
	    }
	}
	if (strcmp(name1, "type") == 0) {
            return "variable \"type\" cannot be modified";
	}
	if (strcmp(name1, "self") == 0) {
            return "variable \"self\" cannot be modified";
	}
	if (strcmp(name1, "selfns") == 0) {
            return "variable \"selfns\" cannot be modified";
	}
    }
    return NULL;
}

/*
 * ------------------------------------------------------------------------
 *  ItclTraceOptionVar()
 *
 *  Invoked to handle read/write traces on "option" variables
 *
 *  On read, this procedure checks if there is a cgetMethodPtr and calls it
 *  On write, this procedure checks if there is a configureMethodPtr
 *  or validateMethodPtr and calls it
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static char*
ItclTraceOptionVar(
    ClientData cdata,	    /* object instance data */
    Tcl_Interp *interp,	    /* interpreter managing this variable */
    const char *name1,      /* variable name */
    const char *name2,      /* unused */
    int flags)		    /* flags indicating read/write */
{
    ItclObject *ioPtr;
    ItclOption *ioptPtr;

/* FIXME !!! */
/* don't know yet if ItclTraceOptionVar is really needed !! */
/* FIXME should free memory on unset or rename!! */
    if (cdata != NULL) {
        ioPtr = (ItclObject*)cdata;
    } else {
        ioptPtr = (ItclOption*)cdata;
        /*
         *  Handle read traces "itcl_options"
         */
        if ((flags & TCL_TRACE_READS) != 0) {
            return NULL;
        }
    
        /*
         *  Handle write traces "itcl_options"
         */
        if ((flags & TCL_TRACE_WRITES) != 0) {
            return NULL;
        }
    }
    return NULL;
}

/*
 * ------------------------------------------------------------------------
 *  ItclTraceComponentVar()
 *
 *  Invoked to handle read/write traces on "component" variables
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static char*
ItclTraceComponentVar(
    ClientData cdata,	    /* object instance data */
    Tcl_Interp *interp,	    /* interpreter managing this variable */
    const char *name1,      /* variable name */
    const char *name2,      /* unused */
    int flags)		    /* flags indicating read/write */
{
    FOREACH_HASH_DECLS;
    Tcl_HashEntry *hPtr2;
    Tcl_Obj *objPtr;
    Tcl_Obj *namePtr;
    Tcl_Obj *componentValuePtr;
    ItclObjectInfo *infoPtr;
    ItclObject *ioPtr;
    ItclComponent *icPtr;
    ItclDelegatedFunction *idmPtr;
    const char *val;

/* FIXME should free memory on unset or rename!! */
    if (cdata != NULL) {
        ioPtr = (ItclObject*)cdata;
        infoPtr = (ItclObjectInfo *)Tcl_GetAssocData(interp,
                ITCL_INTERP_DATA, NULL);
        hPtr = Tcl_FindHashEntry(&infoPtr->objects, (char *)ioPtr);
	if (hPtr == NULL) {
	    /* object does no longer exist or is being destructed */
	    return NULL;
	}
        objPtr = Tcl_NewStringObj(name1, -1);
	hPtr = Tcl_FindHashEntry(&ioPtr->objectComponents, (char *)objPtr);
        Tcl_DecrRefCount(objPtr);

        /*
         *  Handle write traces
         */
        if ((flags & TCL_TRACE_WRITES) != 0) {
	    if (ioPtr->noComponentTrace) {
	        return NULL;
	    }
            /* need to redo the delegation for this component !! */
            if (hPtr == NULL) {
                return " INTERNAL ERROR cannot get component to write to";
            }
            icPtr = Tcl_GetHashValue(hPtr);
	    val = ItclGetInstanceVar(interp, name1, NULL, ioPtr,
                    ioPtr->iclsPtr);
	    if ((val == NULL) || (strlen(val) == 0)) {
	        return " INTERNAL ERROR cannot get value for component";
	    }
	    componentValuePtr = Tcl_NewStringObj(val, -1);
            Tcl_IncrRefCount(componentValuePtr);
	    namePtr = Tcl_NewStringObj(name1, -1);
            FOREACH_HASH_VALUE(idmPtr, &ioPtr->iclsPtr->delegatedFunctions) {
                if (idmPtr->icPtr == icPtr) {
		    hPtr2 = Tcl_FindHashEntry(&idmPtr->exceptions,
		            (char *)namePtr);
                    if (hPtr2 == NULL) {
                        DelegateFunction(interp, ioPtr, ioPtr->iclsPtr,
                                  componentValuePtr, idmPtr);
		     }
	        }
	    }
	    Tcl_DecrRefCount(componentValuePtr);
	    Tcl_DecrRefCount(namePtr);
            return NULL;
        }
        /*
         *  Handle read traces
         */
        if ((flags & TCL_TRACE_READS) != 0) {
        }

    } else {
        icPtr = (ItclComponent *)cdata;
        /*
         *  Handle read traces 
         */
        if ((flags & TCL_TRACE_READS) != 0) {
            return NULL;
        }
    
        /*
         *  Handle write traces
         */
        if ((flags & TCL_TRACE_WRITES) != 0) {
            return NULL;
        }
    }
    return NULL;
}
/*
 * ------------------------------------------------------------------------
 *  ItclTraceItclHullVar()
 *
 *  Invoked to handle read/write traces on "itcl_hull" variables
 *
 *  On write, this procedure returns an error as "itcl_hull" may not be modfied
 *  after the first initialization
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static char*
ItclTraceItclHullVar(
    ClientData cdata,	    /* object instance data */
    Tcl_Interp *interp,	    /* interpreter managing this variable */
    const char *name1,      /* variable name */
    const char *name2,      /* unused */
    int flags)		    /* flags indicating read/write */
{
    Tcl_HashEntry *hPtr;
    Tcl_Obj *objPtr;
    ItclObjectInfo *infoPtr;
    ItclObject *ioPtr;
    ItclVariable *ivPtr;

/* FIXME !!! */
/* FIXME should free memory on unset or rename!! */
    if (cdata != NULL) {
        ioPtr = (ItclObject*)cdata;
        infoPtr = (ItclObjectInfo *)Tcl_GetAssocData(interp,
                ITCL_INTERP_DATA, NULL);
        hPtr = Tcl_FindHashEntry(&infoPtr->objects, (char *)ioPtr);
	if (hPtr == NULL) {
	    /* object does no longer exist or is being destructed */
	    return NULL;
	}
        objPtr = Tcl_NewStringObj(name1, -1);
	hPtr = Tcl_FindHashEntry(&ioPtr->iclsPtr->variables, (char *)objPtr);
        Tcl_DecrRefCount(objPtr);
	if (hPtr == NULL) {
	    return "INTERNAL ERROR cannot find itcl_hull variable in class definition!!";
	}
	ivPtr = Tcl_GetHashValue(hPtr);
        /*
         *  Handle write traces
         */
        if ((flags & TCL_TRACE_WRITES) != 0) {
	    if (ivPtr->initted == 0) {
		ivPtr->initted = 1;
                return NULL;
	    } else {
	        return "The itcl_hull component cannot be redefined";
	    }
        }

    } else {
        ivPtr = (ItclVariable *)cdata;
        /*
         *  Handle read traces 
         */
        if ((flags & TCL_TRACE_READS) != 0) {
            return NULL;
        }
    
        /*
         *  Handle write traces
         */
        if ((flags & TCL_TRACE_WRITES) != 0) {
            return NULL;
        }
    }
    return NULL;
}

/*
 * ------------------------------------------------------------------------
 *  ItclDestroyObject()
 *
 *  Invoked when the object access command is deleted to implicitly
 *  destroy the object.  Invokes the object's destructors, ignoring
 *  any errors encountered along the way.  Removes the object from
 *  the list of all known objects and releases the access command's
 *  claim to the object data.
 *
 *  Note that the usual way to delete an object is via Itcl_DeleteObject().
 *  This procedure is provided as a back-up, to handle the case when
 *  an object is deleted by removing its access command.
 * ------------------------------------------------------------------------
 */
static void
ItclDestroyObject(
    ClientData cdata)  /* object instance data */
{
    ItclObject *contextIoPtr = (ItclObject*)cdata;
    Tcl_HashEntry *hPtr;
    Itcl_InterpState istate;

    if (contextIoPtr->flags & ITCL_OBJECT_IS_DESTROYED) {
        return;
    }
    contextIoPtr->flags |= ITCL_OBJECT_IS_DESTROYED;

    if (!(contextIoPtr->flags & ITCL_OBJECT_IS_DESTRUCTED)) {
        /*
         *  Attempt to destruct the object, but ignore any errors.
         */
        istate = Itcl_SaveInterpState(contextIoPtr->interp, 0);
        Itcl_DestructObject(contextIoPtr->interp, contextIoPtr,
                ITCL_IGNORE_ERRS);
        Itcl_RestoreInterpState(contextIoPtr->interp, istate);
    }

    /*
     *  Now, remove the object from the global object list.
     *  We're careful to do this here, after calling the destructors.
     *  Once the access command is nulled out, the "this" variable
     *  won't work properly.
     */
    if (contextIoPtr->accessCmd != NULL) {
        hPtr = Tcl_FindHashEntry(&contextIoPtr->infoPtr->objects,
            (char*)contextIoPtr);

        if (hPtr) {
            Tcl_DeleteHashEntry(hPtr);
        }
        contextIoPtr->accessCmd = NULL;
    }
    Itcl_ReleaseData((ClientData)contextIoPtr);
}

/*
 * ------------------------------------------------------------------------
 *  ItclFreeObject()
 *
 *  Deletes all instance variables and frees all memory associated with
 *  the given object instance.  This is usually invoked automatically
 *  by Itcl_ReleaseData(), when an object's data is no longer being used.
 * ------------------------------------------------------------------------
 */
static void
ItclFreeObject(
    char * cdata)  /* object instance data */
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch place;
    ItclCallContext *callContextPtr;
    ItclObject *ioPtr;
    
    ioPtr = (ItclObject*)cdata;

    /*
     *  Install the class namespace and object context so that
     *  the object's data members can be destroyed via simple
     *  "unset" commands.  This makes sure that traces work properly
     *  and all memory gets cleaned up.
     *
     *  NOTE:  Be careful to save and restore the interpreter state.
     *    Data can get freed in the middle of any operation, and
     *    we can't affort to clobber the interpreter with any errors
     *    from below.
     */

    Itcl_ReleaseData((ClientData)ioPtr->iclsPtr);
    if (ioPtr->constructed) {
        Tcl_DeleteHashTable(ioPtr->constructed);
        ckfree((char*)ioPtr->constructed);
    }
    if (ioPtr->destructed) {
        Tcl_DeleteHashTable(ioPtr->destructed);
        ckfree((char*)ioPtr->destructed);
    }
    ItclDeleteObjectsDictInfo(ioPtr->interp, ioPtr);
    /*
     *  Delete all context definitions.
     */
    while (1) {
        hPtr = Tcl_FirstHashEntry(&ioPtr->contextCache, &place);
        if (hPtr == NULL) {
            break;
        }
        callContextPtr = Tcl_GetHashValue(hPtr);
	Tcl_DeleteHashEntry(hPtr);
        ckfree((char *)callContextPtr);
    }
    Tcl_DeleteHashTable(&ioPtr->contextCache);
    Tcl_DeleteHashTable(&ioPtr->objectVariables);
    Tcl_DeleteHashTable(&ioPtr->objectOptions);
    Tcl_DeleteHashTable(&ioPtr->objectComponents);
    Tcl_DeleteHashTable(&ioPtr->objectMethodVariables);
    Tcl_DeleteHashTable(&ioPtr->objectDelegatedOptions);
    Tcl_DeleteHashTable(&ioPtr->objectDelegatedFunctions);
    hPtr = Tcl_FindHashEntry(&ioPtr->infoPtr->objectNames,
            (char *)ioPtr->namePtr);
    if (hPtr != NULL) {
        Tcl_DeleteHashEntry(hPtr);
    }
    Tcl_DecrRefCount(ioPtr->namePtr);
    Tcl_DecrRefCount(ioPtr->origNamePtr);
    if (ioPtr->createNamePtr != NULL) {
        Tcl_DecrRefCount(ioPtr->createNamePtr);
    }
    if (ioPtr->hullWindowNamePtr != NULL) {
        Tcl_DecrRefCount(ioPtr->hullWindowNamePtr);
    }
    Tcl_DecrRefCount(ioPtr->varNsNamePtr);
    if (ioPtr->resolvePtr != NULL) {
	ckfree((char *)ioPtr->resolvePtr->clientData);
        ckfree((char*)ioPtr->resolvePtr);
    }
    ckfree((char*)ioPtr);
}

/*
 * ------------------------------------------------------------------------
 *  ItclObjectCmd()
 *
 * ------------------------------------------------------------------------
 */

int Itcl_InvokeProcedureMethod(ClientData clientData, Tcl_Interp *interp,
	int objc, Tcl_Obj *const *objv);

static int
CallPublicObjectCmd(
    ClientData data[],
    Tcl_Interp *interp,
    int result)
{
    Tcl_Object *oPtr = data[0];
    Tcl_Class clsPtr = data[1];
    Tcl_Obj *const* objv = data[3];
    int objc = PTR2INT(data[2]);

    ItclShowArgs(1, "CallPublicObjectCmd", objc, objv);
    result = Itcl_PublicObjectCmd(oPtr, interp, clsPtr, objc, objv);
    ItclShowArgs(1, "CallPublicObjectCmd DONE", objc, objv);
    return result;
}

int
ItclObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    Tcl_Object oPtr,
    Tcl_Class clsPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *methodNamePtr;
    Tcl_Obj **newObjv;
    Tcl_DString buffer;
    Tcl_Obj *myPtr;
    ItclObjectInfo *infoPtr;
    ItclMemberFunc *imPtr;
    ItclClass *iclsPtr;
    Itcl_ListElem *elem;
    ItclClass *basePtr;
    void *callbackPtr;
    char *className;
    char *tail;
    char *cp;
    int isDirectCall;
    int incr;
    int result;
    int found;

    ItclShowArgs(1, "ItclObjectCmd", objc, objv);

    incr = 0;
    found = 0;
    isDirectCall = 0;
    myPtr = NULL;
    imPtr = (ItclMemberFunc *)clientData;
    iclsPtr = imPtr->iclsPtr;
    infoPtr = imPtr->iclsPtr->infoPtr;
    if ((oPtr == NULL) && (clsPtr == NULL)) {
         isDirectCall = 1;
    }
    if (oPtr == NULL) {
	ClientData clientData;
	if ((imPtr->flags & ITCL_COMMON)
	        && (imPtr->codePtr != NULL)
	        && !(imPtr->codePtr->flags & ITCL_BUILTIN)) {
	    result = Itcl_InvokeProcedureMethod(imPtr->tmPtr, interp,
	            objc, objv);
            return result;
	}
	oPtr = NULL;
	clientData = Itcl_GetCallFrameClientData(interp);
	if ((clientData == NULL) && (oPtr == NULL)) {
	    if (((imPtr->codePtr != NULL)
	            && (imPtr->codePtr->flags & ITCL_BUILTIN))) {
	        result = Itcl_InvokeProcedureMethod(imPtr->tmPtr, interp,
	                objc, objv);
                return result;
	    }
	    if (infoPtr->currIoPtr != NULL) {
	        /* if we want to call methods in the constructor for example
	         * config* methods, clientData
	         * is still NULL, but we can use infoPtr->currIoPtr
	         * for getting the TclOO object ptr
	         */
	        oPtr = infoPtr->currIoPtr->oPtr;
	    } else {
	        Tcl_AppendResult(interp,
	                "ItclObjectCmd cannot get context object (NULL)", NULL);
	        return TCL_ERROR;
	    }
	}
	if (oPtr == NULL) {
            oPtr = Tcl_ObjectContextObject((Tcl_ObjectContext)clientData);
        }
    }
    methodNamePtr = NULL;
    if (objv[0] != NULL) {
        Itcl_ParseNamespPath(Tcl_GetString(objv[0]), &buffer,
	        &className, &tail);
        if (className != NULL) {
            methodNamePtr = Tcl_NewStringObj(tail, -1);
	    /* look for the class in the hierarchy */
	    cp = className;
	    if ((*cp == ':') && (*(cp+1) == ':')) {
	        cp += 2;
	    }
            elem = Itcl_FirstListElem(&iclsPtr->bases);
	    if (elem == NULL) {
	        /* check the class itself */
		if (strcmp((const char *)cp,
		        (const char *)Tcl_GetString(iclsPtr->namePtr)) == 0) {
		    found = 1;
		    clsPtr = iclsPtr->clsPtr;
		}
	    }
            while (elem != NULL) {
                basePtr = (ItclClass*)Itcl_GetListValue(elem);
		if (strcmp((const char *)cp,
		        (const char *)Tcl_GetString(basePtr->namePtr)) == 0) {
		    clsPtr = basePtr->clsPtr;
		    found = 1;
		    break;
		}
                elem = Itcl_NextListElem(elem);
	    }
        }
        Tcl_DStringFree(&buffer);
    }
    if (isDirectCall) {
	if (!found) {
	    if (methodNamePtr != NULL) {
	        Tcl_DecrRefCount(methodNamePtr);
	    }
            methodNamePtr = objv[0];
            Tcl_IncrRefCount(methodNamePtr);
        }
    }
    callbackPtr = Itcl_GetCurrentCallbackPtr(interp);
    newObjv = NULL;
    if (methodNamePtr != NULL) {
	if (iclsPtr->flags & (ITCL_TYPE|ITCL_WIDGETADAPTOR)) {
	    char *myName;
	    /* special handling for mytypemethod, mymethod, myproc */
	    myName = Tcl_GetString(methodNamePtr);
	    if (strcmp(myName, "mytypemethod") == 0) {
                result = Itcl_BiMyTypeMethodCmd(iclsPtr, interp, objc, objv);
		return result;
	    }
	    if (strcmp(myName, "mymethod") == 0) {
                result = Itcl_BiMyMethodCmd(iclsPtr, interp, objc, objv);
		return result;
	    }
	    if (strcmp(myName, "myproc") == 0) {
                result = Itcl_BiMyProcCmd(iclsPtr, interp, objc, objv);
		return result;
	    }
	    if (strcmp(myName, "mytypevar") == 0) {
                result = Itcl_BiMyTypeVarCmd(iclsPtr, interp, objc, objv);
		return result;
	    }
	    if (strcmp(myName, "myvar") == 0) {
                result = Itcl_BiMyVarCmd(iclsPtr, interp, objc, objv);
		return result;
	    }
	    if (strcmp(myName, "itcl_hull") == 0) {
                result = Itcl_BiItclHullCmd(iclsPtr, interp, objc, objv);
		return result;
	    }
	    if (strcmp(myName, "callinstance") == 0) {
                result = Itcl_BiCallInstanceCmd(iclsPtr, interp, objc, objv);
		return result;
	    }
	    if (strcmp(myName, "getinstancevar") == 0) {
                result = Itcl_BiGetInstanceVarCmd(iclsPtr, interp, objc, objv);
		return result;
	    }
	    if (strcmp(myName, "installcomponent") == 0) {
                result = Itcl_BiInstallComponentCmd(iclsPtr, interp, objc, objv);
		return result;
	    }
	}
        incr = 1;
        newObjv = (Tcl_Obj **)ckalloc(sizeof(Tcl_Obj *)*(objc+incr));
	myPtr = Tcl_NewStringObj("my", 2);
        Tcl_IncrRefCount(myPtr);
        newObjv[0] = myPtr;
        newObjv[1] = methodNamePtr;
        memcpy(newObjv+incr+1, objv+1, (sizeof(Tcl_Obj*)*(objc-1)));
	ItclShowArgs(1, "run CallPublicObjectCmd1", objc+incr, newObjv);
        Itcl_NRAddCallback(interp, CallPublicObjectCmd, oPtr, clsPtr,
	        INT2PTR(objc+incr), newObjv);

    } else {
	ItclShowArgs(1, "run CallPublicObjectCmd2", objc, objv);
        Itcl_NRAddCallback(interp, CallPublicObjectCmd, oPtr, clsPtr,
	        INT2PTR(objc), (ClientData)objv);
    }

    result = Itcl_NRRunCallbacks(interp, callbackPtr);
    if (methodNamePtr != NULL) {
        ckfree((char *)newObjv);
        Tcl_DecrRefCount(methodNamePtr);
    }
    if (myPtr != NULL) {
        Tcl_DecrRefCount(myPtr);
    }
    return result;
}

/*
 * ------------------------------------------------------------------------
 *  ItclObjectUnknownCommand()
 *  syntax: is
 *  objv[0]    command name of myself (::itcl::methodset::objectUnknownCommand)
 *  objv[1]    object name for [self]
 *  objv[2]    object name as found on the stack
 *  objv[3]    method name
 * ------------------------------------------------------------------------
 */

int
ItclObjectUnknownCommand(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Object oPtr;
    Tcl_Command cmd;
    Tcl_CmdInfo cmdInfo;
    ItclObject *ioPtr;
    ItclObjectInfo *infoPtr;

    ItclShowArgs(1, "ItclObjectUnknownCommand", objc, objv);
    cmd = Tcl_GetCommandFromObj(interp, objv[1]);
    if (Tcl_GetCommandInfoFromToken(cmd, &cmdInfo) != 1) {
    }
    oPtr = cmdInfo.objClientData;
    infoPtr = (ItclObjectInfo *)Tcl_GetAssocData(interp,
            ITCL_INTERP_DATA, NULL);
    ioPtr = (ItclObject *)Tcl_ObjectGetMetadata(oPtr,
            infoPtr->object_meta_type);
    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "bad option \"", Tcl_GetString(objv[3]), "\": should be one of...",
	    (char*)NULL);
    ItclReportObjectUsage(interp, ioPtr, NULL, NULL);
    return TCL_ERROR;
}

/*
 * ------------------------------------------------------------------------
 *  GetClassFromClassName()
 * ------------------------------------------------------------------------
 */

static ItclClass *
GetClassFromClassName(
    const char *className,
    ItclClass *iclsPtr)
{
    Tcl_Obj *objPtr;
    Tcl_HashEntry *hPtr;
    ItclClass *basePtr;
    Itcl_ListElem *elem;
    const char *chkPtr;
    int chkLgth;
    int lgth;

    /* look for the class in the hierarchy */
    /* first check the class itself */
    if (strcmp(className,
            (const char *)Tcl_GetString(iclsPtr->namePtr)) == 0) {
	return iclsPtr;
    }
    elem = Itcl_FirstListElem(&iclsPtr->bases);
    while (elem != NULL) {
        basePtr = (ItclClass*)Itcl_GetListValue(elem);
	basePtr = GetClassFromClassName(className, basePtr);
	if (basePtr != NULL) {
	    return basePtr;
	}
        elem = Itcl_NextListElem(elem);
    }
    /* now try to match the classes full name last part with the className */
    lgth = strlen(className);
    elem = Itcl_FirstListElem(&iclsPtr->bases);
    while (elem != NULL) {
        basePtr = (ItclClass*)Itcl_GetListValue(elem);
	chkPtr = basePtr->nsPtr->fullName;
	chkLgth = strlen(chkPtr);
	if (chkLgth >= lgth) {
	    chkPtr = chkPtr + chkLgth - lgth;
	    if (strcmp(chkPtr, className) == 0) {
	        return basePtr;
	    }
	}
        elem = Itcl_NextListElem(elem);
    }
    /* as a last chance try with className in hash table */
    objPtr = Tcl_NewStringObj(className, -1);
    Tcl_IncrRefCount(objPtr);
    hPtr = Tcl_FindHashEntry(&iclsPtr->infoPtr->nameClasses, (char *)objPtr);
    if (hPtr != NULL) {
        iclsPtr = Tcl_GetHashValue(hPtr);
    } else {
        iclsPtr = NULL;
    }
    Tcl_DecrRefCount(objPtr);
    return iclsPtr;
}

/*
 * ------------------------------------------------------------------------
 *  ItclMapMethodNameProc()
 * ------------------------------------------------------------------------
 */

int
ItclMapMethodNameProc(
    Tcl_Interp *interp,
    Tcl_Object oPtr,
    Tcl_Class *startClsPtr,
    Tcl_Obj *methodObj)
{
    Tcl_Obj *methodName;
    Tcl_Obj *className;
    Tcl_DString buffer;
    Tcl_HashEntry *hPtr;
    ItclObject *ioPtr;
    ItclClass *iclsPtr;
    ItclClass *iclsPtr2;
    ItclObjectInfo *infoPtr;
    char *head;
    char *tail;
    char *sp;

    iclsPtr = NULL;
    iclsPtr2 = NULL;
    methodName = NULL;
    infoPtr = (ItclObjectInfo *)Tcl_GetAssocData(interp,
            ITCL_INTERP_DATA, NULL);
    ioPtr = (ItclObject *)Tcl_ObjectGetMetadata(oPtr,
            infoPtr->object_meta_type);
    hPtr = Tcl_FindHashEntry(&infoPtr->objects, (char *)ioPtr);
    if ((hPtr == NULL) || (ioPtr == NULL)) {
        /* try to get the class (if a class is creating an object) */
        iclsPtr = (ItclClass *)Tcl_ObjectGetMetadata(oPtr,
            infoPtr->class_meta_type);
        hPtr = Tcl_FindHashEntry(&infoPtr->classes, (char *)iclsPtr);
	if (hPtr == NULL) {
	    char str[20];
	    sprintf(str, "%p", iclsPtr);
	    Tcl_AppendResult(interp, "context class has vanished 1", str, NULL);
            return TCL_ERROR;
	}
    } else {
        hPtr = Tcl_FindHashEntry(&infoPtr->classes, (char *)ioPtr->iclsPtr);
	if (hPtr == NULL) {
	    char str[20];
	    sprintf(str, "%p", ioPtr->iclsPtr);
	    Tcl_AppendResult(interp, "context class has vanished 2", str, NULL);
            return TCL_ERROR;
	}
        iclsPtr = ioPtr->iclsPtr;
    }
    sp = Tcl_GetString(methodObj);
    Itcl_ParseNamespPath(sp, &buffer, &head, &tail);
    if (head != NULL) {
        className = NULL;
        methodName = Tcl_NewStringObj(tail, -1);
        Tcl_IncrRefCount(methodName);
        className = Tcl_NewStringObj(head, -1);
        Tcl_IncrRefCount(className);
	if (strlen(head) > 0) {
	    iclsPtr2 = GetClassFromClassName(head, iclsPtr);
	} else {
	    iclsPtr2 = NULL;
	}
	if (iclsPtr2 != NULL) {
	    *startClsPtr = iclsPtr2->clsPtr;
	    Tcl_SetStringObj(methodObj, Tcl_GetString(methodName), -1);
	}
        Tcl_DecrRefCount(className);
        Tcl_DecrRefCount(methodName);
    }
    hPtr = Tcl_FindHashEntry(&iclsPtr->resolveCmds, (char *)methodObj);
    if (hPtr != NULL) {
	ItclMemberFunc *imPtr;
	Tcl_Namespace *nsPtr;
	ItclCmdLookup *clookup;

	nsPtr = Tcl_GetCurrentNamespace(interp);
	clookup = (ItclCmdLookup *)Tcl_GetHashValue(hPtr);
	imPtr = clookup->imPtr;
        if (!Itcl_CanAccessFunc(imPtr, nsPtr)) {
	    char *token = Tcl_GetString(imPtr->namePtr);
	    if ((*token != 'i') || (strcmp(token, "info") != 0)) {
		/* needed for test protect-2.5 */
	        ItclMemberFunc *imPtr2 = NULL;
                Tcl_HashEntry *hPtr;
	        Tcl_ObjectContext context;
	        context = Itcl_GetCallFrameClientData(interp);
                if (context != NULL) {
	            hPtr = Tcl_FindHashEntry(
		            &imPtr->iclsPtr->infoPtr->procMethods,
	                    (char *)Tcl_ObjectContextMethod(context));
	            if (hPtr != NULL) {
	                imPtr2 = Tcl_GetHashValue(hPtr);
	            }
		    if ((imPtr->protection & ITCL_PRIVATE) &&
		            (imPtr2 != NULL) &&
		            (imPtr->iclsPtr->nsPtr != imPtr2->iclsPtr->nsPtr)) {
                        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		                "invalid command name \"",
			        token,
			        "\"", NULL);
		        return TCL_ERROR;
		    }
                }
		/* END needed for test protect-2.5 */
                Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                       "bad option \"", token, "\": should be one of...",
                        (char*)NULL);
	        ItclReportObjectUsage(interp, ioPtr, nsPtr, nsPtr);
                return TCL_ERROR;
            }
        }
    }
    Tcl_DStringFree(&buffer);
    return TCL_OK;
}

int
ExpandDelegateAs(
    Tcl_Interp *interp,
    ItclObject *ioPtr,
    ItclClass *iclsPtr,
    ItclDelegatedFunction *idmPtr,
    const char *funcName,
    Tcl_Obj *listPtr)
{
    Tcl_Obj *componentNamePtr;
    Tcl_Obj *objPtr;
    const char **argv;
    const char *val;
    int argc;
    int j;


    if (idmPtr->icPtr == NULL) {
        componentNamePtr = NULL;
    } else {
        componentNamePtr = idmPtr->icPtr->namePtr;
    }
    if (idmPtr->asPtr != NULL) {
        Tcl_SplitList(interp, Tcl_GetString(idmPtr->asPtr),
	        &argc, &argv);
        for(j=0;j<argc;j++) {
            Tcl_ListObjAppendElement(interp, listPtr,
                    Tcl_NewStringObj(argv[j], -1));
        }
    } else {
	if (idmPtr->usingPtr != NULL) {
	    char *cp;
	    char *ep;
	    int hadDoublePercent;
            Tcl_Obj *strPtr;

	    strPtr = NULL;
	    hadDoublePercent = 0;
	    cp = Tcl_GetString(idmPtr->usingPtr);
	    ep = cp;
	    strPtr = Tcl_NewStringObj("", -1);
	    while (*ep != '\0') {
	        if (*ep == '%') {
		    if (*(ep+1) == '%') {
			cp++;
			cp++;
		        ep++;
		        ep++;
			hadDoublePercent = 1;
			Tcl_AppendToObj(strPtr, "%", -1);
		        continue;
		    }
		    switch (*(ep+1)) {
		    case 'c':
			if (componentNamePtr == NULL) {
			    ep++;
			    continue;
			}
			if (ep-cp-1 > 0) {
		            Tcl_ListObjAppendElement(interp, listPtr,
			            Tcl_NewStringObj(cp, ep-cp-1));
			}
                        objPtr = Tcl_NewStringObj(ITCL_VARIABLES_NAMESPACE, -1);
                        Tcl_AppendToObj(objPtr, iclsPtr->nsPtr->fullName, -1);
                        Tcl_AppendToObj(objPtr, "::", -1);
                        Tcl_AppendToObj(objPtr,
			        Tcl_GetString(componentNamePtr), -1);
                        val = Tcl_GetVar2(interp, Tcl_GetString(objPtr),
			        NULL, 0);
			Tcl_DecrRefCount(objPtr);
			Tcl_AppendToObj(strPtr,
			        val, -1);
		        break;
		    case 'j':
		    case 'm':
		    case 'M':
			if (ep-cp-1 > 0) {
		            Tcl_ListObjAppendElement(interp, listPtr,
			            Tcl_NewStringObj(cp, ep-cp-1));
			}
			if (strcmp(Tcl_GetString(idmPtr->namePtr), "*") == 0) {
			    Tcl_AppendToObj(strPtr, funcName, -1);
			} else {
			    Tcl_AppendToObj(strPtr,
			            Tcl_GetString(idmPtr->namePtr), -1);
			}
		        break;
		    case 'n':
			if (iclsPtr->flags & ITCL_TYPE) {
			    ep++;
			    continue;
			} else {
			    if (ep-cp-1 > 0) {
		                Tcl_ListObjAppendElement(interp, listPtr,
			                Tcl_NewStringObj(cp, ep-cp-1));
			    }
			    Tcl_AppendToObj(strPtr, iclsPtr->nsPtr->name, -1);
			}
		        break;
		    case 's':
			if (iclsPtr->flags & ITCL_TYPE) {
			    ep++;
			    continue;
			} else {
			    if (ep-cp-1 > 0) {
		                Tcl_ListObjAppendElement(interp, listPtr,
			                Tcl_NewStringObj(cp, ep-cp-1));
			    }
                            Tcl_AppendToObj(strPtr,
			            Tcl_GetString(ioPtr->namePtr), -1);
			}
		        break;
		    case 't':
			if (ep-cp-1 > 0) {
		            Tcl_ListObjAppendElement(interp, listPtr,
			            Tcl_NewStringObj(cp, ep-cp-1));
			}
                        Tcl_AppendToObj(strPtr, iclsPtr->nsPtr->fullName, -1);
		        break;
		    case 'w':
			if (iclsPtr->flags & ITCL_TYPE) {
			    ep++;
			    continue;
			} else {
			    if (ep-cp-1 > 0) {
		                Tcl_ListObjAppendElement(interp, listPtr,
			                Tcl_NewStringObj(cp, ep-cp-1));
			    }
		        }
		        break;
		    case ':':
		        /* substitute with contents of variable after ':' */
			if (iclsPtr->flags & ITCL_ECLASS) {
			    if (ep-cp-1 > 0) {
		                Tcl_ListObjAppendElement(interp, listPtr,
			                Tcl_NewStringObj(cp, ep-cp-1));
			    }
			    ep++;
			    cp = ep + 1;
			    while (*ep && (*ep != ' ')) {
			        ep++;
			    }
			    if (ep-cp > 0) {
				Tcl_Obj *my_obj;
				const char *cp2;

                                my_obj = Tcl_NewStringObj(cp, ep-cp);
				if (iclsPtr->infoPtr->currIoPtr != NULL) {
				  cp2 = GetConstructorVar(interp, iclsPtr,
				      Tcl_GetString(my_obj));
				} else {
                                  cp2 = ItclGetInstanceVar(interp,
                                      Tcl_GetString(my_obj), NULL, ioPtr,
				      iclsPtr);
				}
				if (cp2 != NULL) {
                                    Tcl_AppendToObj(strPtr, cp2, -1);
				}
			        ep -= 2; /* to fit for code after default !! */
			    }
		            break;
		        } else {
			    /* fall through */
			}
		    default:
		      {
			char buf[2];
			buf[1] = '\0';
			sprintf(buf, "%c", *(ep+1));
			Tcl_AppendResult(interp,
			        "there is no %%", buf, " substitution",
				NULL);
			if (strPtr != NULL) {
			    Tcl_DecrRefCount(strPtr);
			}
			return TCL_ERROR;
		      }
		    }
		    Tcl_ListObjAppendElement(interp, listPtr, strPtr);
		    hadDoublePercent = 0;
		    strPtr = Tcl_NewStringObj("", -1);
		    ep +=2;
		    cp = ep;
		} else {
		    if (*ep == ' ') {
                        if (strlen(Tcl_GetString(strPtr)) > 0) {
			    if (ep-cp == 0) {
			        Tcl_ListObjAppendElement(interp, listPtr,
				        strPtr);
			        strPtr = Tcl_NewStringObj("", -1);
			    }
			}
			if (ep-cp > 0) {
			    Tcl_AppendToObj(strPtr, cp, ep-cp);
		            Tcl_ListObjAppendElement(interp, listPtr, strPtr);
	                    strPtr = Tcl_NewStringObj("", -1);
			}
		        while((*ep != '\0') && (*ep == ' ')) {
			    ep++;
			}
			cp = ep;
		    } else {
		        ep++;
		    }
		}
	    }
	    if (cp != ep) {
		if (*ep == '\0') {
                    Tcl_ListObjAppendElement(interp, listPtr,
	                    Tcl_NewStringObj(cp, ep-cp));
		} else {
                    Tcl_ListObjAppendElement(interp, listPtr,
	                    Tcl_NewStringObj(cp, ep-cp-1));
	        }
	    }
	    if (strPtr != NULL) {
	        Tcl_DecrRefCount(strPtr);
	    }
	} else {
            Tcl_ListObjAppendElement(interp, listPtr, idmPtr->namePtr);
        }
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  DelegationFunction()
 * ------------------------------------------------------------------------
 */

int
DelegateFunction(
    Tcl_Interp *interp,
    ItclObject *ioPtr,
    ItclClass *iclsPtr,
    Tcl_Obj *componentValuePtr,
    ItclDelegatedFunction *idmPtr)
{
    Tcl_Obj *listPtr;
    const char *val;
    int result;
    Tcl_Method mPtr;

    listPtr = Tcl_NewListObj(0, NULL);
    if (componentValuePtr != NULL) {
	if (idmPtr->usingPtr == NULL) {
            Tcl_ListObjAppendElement(interp, listPtr, componentValuePtr);
        }
    }
    result = ExpandDelegateAs(interp, ioPtr, iclsPtr, idmPtr,
            Tcl_GetString(idmPtr->namePtr), listPtr);
    if (result != TCL_OK) {
	Tcl_DecrRefCount(listPtr);
        return result;
    }
    val = Tcl_GetString(listPtr);
    if (componentValuePtr != NULL) {
        mPtr = Itcl_NewForwardClassMethod(interp, iclsPtr->clsPtr, 1,
                idmPtr->namePtr, listPtr);
        if (mPtr != NULL) {
            return TCL_OK;
        }
    }
    if (idmPtr->usingPtr != NULL) {
        mPtr = Itcl_NewForwardClassMethod(interp, iclsPtr->clsPtr, 1,
                idmPtr->namePtr, listPtr);
        if (mPtr != NULL) {
            return TCL_OK;
        }
    }
    return TCL_ERROR;
}

/*
 * ------------------------------------------------------------------------
 *  DelegatedOptionsInstall()
 * ------------------------------------------------------------------------
 */

int
DelegatedOptionsInstall(
    Tcl_Interp *interp,
    ItclClass *iclsPtr)
{
    Tcl_HashEntry *hPtr2;
    Tcl_HashSearch search2;
    ItclDelegatedOption *idoPtr;
    ItclOption *ioptPtr;
    FOREACH_HASH_DECLS;
    char *optionName;

    FOREACH_HASH_VALUE(idoPtr, &iclsPtr->delegatedOptions) {
	optionName = Tcl_GetString(idoPtr->namePtr);
	if (*optionName == '*') {
	    /* allow nested FOREACH */
	    search2 = search;
	    FOREACH_HASH_VALUE(ioptPtr, &iclsPtr->options) {
	        if (Tcl_FindHashEntry(&idoPtr->exceptions,
		        (char *)idoPtr->namePtr) == NULL) {
		    ioptPtr->idoPtr = idoPtr;
		    Itcl_PreserveData(ioptPtr->idoPtr);
		}
	    }
	    search = search2;
	} else {
            hPtr2 = Tcl_FindHashEntry(&iclsPtr->options,
	            (char *)idoPtr->namePtr);
	    if (hPtr2 == NULL) {
		ioptPtr = NULL;
	    } else {
	        ioptPtr = Tcl_GetHashValue(hPtr2);
	        ioptPtr->idoPtr = idoPtr;
	    }
	    idoPtr->ioptPtr = ioptPtr;
        }
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  GetConstructorVar()
 *  get an object variable when in executing the constructor
 * ------------------------------------------------------------------------
 */

static const char *
GetConstructorVar(
    Tcl_Interp *interp,
    ItclClass *iclsPtr,
    const char *varName)

{
    Tcl_HashEntry *hPtr;
    Tcl_Obj *objPtr;
    Tcl_DString buffer;
    ItclVarLookup *vlookup;
    ItclVariable *ivPtr;
    const char *val;

    hPtr = Tcl_FindHashEntry(&iclsPtr->resolveVars, (char *)varName);
    if (hPtr == NULL) {
	/* no such variable */
        return NULL;
    }
    vlookup = (ItclVarLookup *)Tcl_GetHashValue(hPtr);
    if (vlookup == NULL) {
        return NULL;
    }
    ivPtr = vlookup->ivPtr;
    if (ivPtr == NULL) {
        return NULL;
    }
    if (ivPtr->flags & ITCL_COMMON) {
        /* look for a common variable */
        objPtr = Tcl_NewStringObj(ITCL_VARIABLES_NAMESPACE, -1);
        Tcl_AppendToObj(objPtr, iclsPtr->nsPtr->fullName, -1);
        Tcl_AppendToObj(objPtr, "::", -1);
        Tcl_AppendToObj(objPtr, varName, -1);
        val = Tcl_GetVar2(interp, Tcl_GetString(objPtr), NULL, 0);
        Tcl_DecrRefCount(objPtr);
    } else {
        /* look for a normal variable */
        Tcl_DStringInit(&buffer);
        Tcl_DStringAppend(&buffer,
                Tcl_GetString(iclsPtr->infoPtr->currIoPtr->varNsNamePtr), -1);
        Tcl_DStringAppend(&buffer, ivPtr->iclsPtr->nsPtr->fullName, -1);
        Tcl_DStringAppend(&buffer, "::", -1);
        Tcl_DStringAppend(&buffer, varName, -1);
        val = Tcl_GetVar2(interp, Tcl_DStringValue(&buffer), NULL, 0);
    }
    return val;
}

/*
 * ------------------------------------------------------------------------
 *  DelegationInstall()
 * ------------------------------------------------------------------------
 */

int
DelegationInstall(
    Tcl_Interp *interp,
    ItclObject *ioPtr,
    ItclClass *iclsPtr)
{
    Tcl_HashEntry *hPtr2;
    Tcl_HashSearch search2;
    Tcl_Obj *componentValuePtr;
    Tcl_DString buffer;
    ItclDelegatedFunction *idmPtr;
    ItclMemberFunc *imPtr;
    ItclVariable *ivPtr;
    FOREACH_HASH_DECLS;
    char *methodName;
    const char *val;
    int result;
    int noDelegate;
    int delegateAll;

    result = TCL_OK;
    delegateAll = 0;
    ioPtr->noComponentTrace = 1;
    noDelegate = ITCL_CONSTRUCTOR|ITCL_DESTRUCTOR|ITCL_COMPONENT;
    componentValuePtr = NULL;
    FOREACH_HASH_VALUE(idmPtr, &iclsPtr->delegatedFunctions) {
	methodName = Tcl_GetString(idmPtr->namePtr);
	if (*methodName == '*') {
	    delegateAll = 1;
	}
	if (idmPtr->icPtr != NULL) {
	    Tcl_Obj *objPtr;
	    /* we cannot use Itcl_GetInstanceVar here as the object is not
	     * yet completely built. So use the varNsNamePtr
	     */
	    ivPtr = idmPtr->icPtr->ivPtr;
            if (ivPtr->flags & ITCL_COMMON) {
	        objPtr = Tcl_NewStringObj(ITCL_VARIABLES_NAMESPACE, -1);
	        Tcl_AppendToObj(objPtr, ivPtr->iclsPtr->nsPtr->fullName, -1);
	        Tcl_AppendToObj(objPtr, "::", -1);
	        Tcl_AppendToObj(objPtr,
		        Tcl_GetString(idmPtr->icPtr->namePtr), -1);
	        val = Tcl_GetVar2(interp, Tcl_GetString(objPtr), NULL, 0);
	        Tcl_DecrRefCount(objPtr);
	    } else {
                Tcl_DStringInit(&buffer);
                Tcl_DStringAppend(&buffer,
                        Tcl_GetString(ioPtr->varNsNamePtr), -1);
                Tcl_DStringAppend(&buffer,
                        Tcl_GetString(ivPtr->fullNamePtr), -1);
                val = Tcl_GetVar2(interp,
                            Tcl_DStringValue(&buffer), NULL, 0);
	    }
	    componentValuePtr = Tcl_NewStringObj(val, -1);
            Tcl_IncrRefCount(componentValuePtr);
	} else {
	    componentValuePtr = NULL;
	}
	if (!delegateAll) {
	    result = DelegateFunction(interp, ioPtr, iclsPtr,
	            componentValuePtr, idmPtr);
	    if (result != TCL_OK) {
                ioPtr->noComponentTrace = 0;
	        return result;
	    }
	} else {
	    /* save to allow nested FOREACH */
            search2 = search;
            FOREACH_HASH_VALUE(imPtr, &iclsPtr->functions) {
	        methodName = Tcl_GetString(imPtr->namePtr);
                if (imPtr->flags & noDelegate) {
		    continue;
		}
                if (strcmp(methodName, "info") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "isa") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "createhull") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "keepcomponentoption") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "setupcomponent") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "itcl_initoptions") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "mytypemethod") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "mymethod") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "myproc") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "mytypevar") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "myvar") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "itcl_hull") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "callinstance") == 0) {
	            continue;
	        }
                if (strcmp(methodName, "getinstancevar") == 0) {
	            continue;
	        }
                hPtr2 = Tcl_FindHashEntry(&idmPtr->exceptions,
		        (char *)imPtr->namePtr);
                if (hPtr2 != NULL) {
		    continue;
		}
	        result = DelegateFunction(interp, ioPtr, iclsPtr,
	                componentValuePtr, idmPtr);
	        if (result != TCL_OK) {
	            break;
	        }
            }
            search = search2;
        }
	if (componentValuePtr != NULL) {
            Tcl_DecrRefCount(componentValuePtr);
        }
    }
    ioPtr->noComponentTrace = 0;
    result = DelegatedOptionsInstall(interp, iclsPtr);
    return result;
}

/*
 * ------------------------------------------------------------------------
 *  ItclInitExtendedClassOptions()
 * ------------------------------------------------------------------------
 */

static int
ItclInitExtendedClassOptions(
    Tcl_Interp *interp,
    ItclObject *ioPtr)
{
    ItclClass *iclsPtr;
    ItclOption *ioptPtr;
    ItclHierIter hier;
    FOREACH_HASH_DECLS;

    iclsPtr = ioPtr->iclsPtr;
    Itcl_InitHierIter(&hier, iclsPtr);
    while ((iclsPtr = Itcl_AdvanceHierIter(&hier)) != NULL) {
        FOREACH_HASH_VALUE(ioptPtr, &iclsPtr->options) {
            if (ioptPtr->defaultValuePtr != NULL) {
		if (ItclGetInstanceVar(interp, "itcl_options",
		        Tcl_GetString(ioptPtr->namePtr), ioPtr, iclsPtr)
			== NULL) {
                    ItclSetInstanceVar(interp, "itcl_options",
                            Tcl_GetString(ioptPtr->namePtr),
                            Tcl_GetString(ioptPtr->defaultValuePtr),
			    ioPtr, iclsPtr);
	        }
            }
	}
    }
    Itcl_DeleteHierIter(&hier);
    return TCL_OK;
}
