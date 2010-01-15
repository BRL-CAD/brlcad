/*
 * itcl2TclOO.c --
 *
 *	This file contains code to create and manage methods.
 *
 * Copyright (c) 2007 by Arnulf P. Wiedemann
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include <tcl.h>
#include <tclInt.h>
#include <tclOO.h>
#include <tclOOInt.h>

int
Itcl_NRCallObjProc(
    ClientData clientData,
    Tcl_Interp *interp,
    Tcl_ObjCmdProc *objProc,
    int objc,
    Tcl_Obj *const *objv)
{
    return Tcl_NRCallObjProc(interp, objProc, clientData, objc, objv);
}

void
Itcl_NRAddCallback_(
    Tcl_Interp *interp,
    char *procName,
    void *procPtr,
    ClientData data0,
    ClientData data1,
    ClientData data2,
    ClientData data3)
{
    Tcl_NRAddCallback(interp, procPtr, data0, data1, data2, data3);
}

void *
Itcl_GetCurrentCallbackPtr(
    Tcl_Interp *interp)
{
    return TOP_CB(interp);
}

int
Itcl_NRRunCallbacks(
    Tcl_Interp *interp,
    void *rootPtr)
{
    return TclNRRunCallbacks(interp, TCL_OK, rootPtr, 0);
}

static int
CallFinalizePMCall(
    ClientData data[],
    Tcl_Interp *interp,
    int result)
{
    Tcl_Namespace *nsPtr = data[0];
    TclOO_PostCallProc postCallProc = data[1];
    ClientData clientData = data[2];

    /*
     * Give the post-call callback a chance to do some cleanup. Note that at
     * this point the call frame itself is invalid; it's already been popped.
     */

    if (postCallProc) {
        result = postCallProc(clientData, interp, NULL, nsPtr, result);
    }
    return result;
}

extern int ItclAfterCallMethod(ClientData clientData, Tcl_Interp *interp,
    Tcl_ObjectContext contextPtr, Tcl_Namespace *nsPtr, int call_result);

int
Tcl_InvokeClassProcedureMethod(
    Tcl_Interp *interp,
    Tcl_Obj *namePtr,           /* name of the method */
    Tcl_Namespace *nsPtr,       /* namespace for calling method */
    ProcedureMethod *pmPtr,     /* method type specific data */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Arguments as actually seen. */
{
    Proc *procPtr = pmPtr->procPtr;
    int flags = FRAME_IS_METHOD;
    CallFrame frame;
    CallFrame *framePtr = &frame;
    CallFrame **framePtrPtr1 = &framePtr;
    Tcl_CallFrame **framePtrPtr = (Tcl_CallFrame **)framePtrPtr1;
    Command cmd;
    int result;

    memset(&cmd, 0, sizeof(Command));
    cmd.nsPtr = (Namespace *) nsPtr;
    cmd.clientData = NULL;
    pmPtr->procPtr->cmdPtr = &cmd;

    result = TclProcCompileProc(interp, pmPtr->procPtr,
	    pmPtr->procPtr->bodyPtr, (Namespace *) nsPtr, "body of method",
	    Tcl_GetString(namePtr));
    if (result != TCL_OK) {
	return result;
    }
    /*
     * Make the stack frame and fill it out with information about this call.
     * This operation may fail.
     */


    flags |= FRAME_IS_PROC;
    result = TclPushStackFrame(interp, framePtrPtr, nsPtr, flags);
    if (result != TCL_OK) {
	return result;
    }

    framePtr->clientData = NULL;
    framePtr->objc = objc;
    framePtr->objv = objv;
    framePtr->procPtr = procPtr;

    /*
     * Give the pre-call callback a chance to do some setup and, possibly,
     * veto the call.
     */

    if (pmPtr->preCallProc != NULL) {
	int isFinished;

	result = pmPtr->preCallProc(pmPtr->clientData, interp, NULL,
		(Tcl_CallFrame *) framePtr, &isFinished);
	if (isFinished || result != TCL_OK) {
	    Tcl_PopCallFrame(interp);
	    TclStackFree(interp, framePtr);
	    goto done;
	}
    }

    /*
     * Now invoke the body of the method. Note that we need to take special
     * action when doing unknown processing to ensure that the missing method
     * name is passed as an argument.
     */

    Tcl_NRAddCallback(interp, CallFinalizePMCall, nsPtr, pmPtr->postCallProc, pmPtr->clientData, NULL);
    return TclNRInterpProcCore(interp, namePtr, 1, pmPtr->errProc);

done:
    return result;
}

int
Itcl_InvokeProcedureMethod(
    ClientData clientData,	/* Pointer to some per-method context. */
    Tcl_Interp *interp,
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Arguments as actually seen. */
{
    Tcl_Namespace *nsPtr;
    Method *mPtr;

    mPtr = clientData;
    if (mPtr->declaringClassPtr == NULL) {
	/* that is the case for typemethods */
        nsPtr = mPtr->declaringObjectPtr->namespacePtr;
    } else {
        nsPtr = mPtr->declaringClassPtr->thisPtr->namespacePtr;
    }

    return Tcl_InvokeClassProcedureMethod(interp, mPtr->namePtr, nsPtr,
            mPtr->clientData, objc, objv);
}

int
Itcl_InvokeEnsembleMethod(
    Tcl_Interp *interp,
    Tcl_Namespace *nsPtr,       /* namespace to call the method in */
    Tcl_Obj *namePtr,           /* name of the method */
    Proc *procPtr,
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Arguments as actually seen. */
{
    ProcedureMethod pm;

    pm.version = TCLOO_PROCEDURE_METHOD_VERSION;
    pm.procPtr = (Proc *)procPtr;
    pm.flags = USE_DECLARER_NS;
    pm.clientData = NULL;
    pm.deleteClientdataProc = NULL;
    pm.cloneClientdataProc = NULL;
    pm.errProc = NULL;
    pm.preCallProc = NULL;
    pm.postCallProc = NULL;
    pm.gfivProc = NULL;
    return Tcl_InvokeClassProcedureMethod(interp, namePtr, nsPtr,
            &pm, objc, objv);
}


/*
 * ----------------------------------------------------------------------
 *
 * Itcl_PublicObjectCmd, Itcl_PrivateObjectCmd --
 *
 *	Main entry point for object invokations. The Public* and Private*
 *	wrapper functions are just thin wrappers around the main ObjectCmd
 *	function that does call chain creation, management and invokation.
 *
 * ----------------------------------------------------------------------
 */

int
Itcl_PublicObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    Tcl_Class clsPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Object oPtr = (Tcl_Object)clientData;
    int result;

    result = TclOOInvokeObject(interp, oPtr, clsPtr, PUBLIC_METHOD,
            objc, objv);
    return result;
}

int
Itcl_PrivateObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    Tcl_Class clsPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Object oPtr = (Tcl_Object)clientData;
    int result;

    result = TclOOInvokeObject(interp, oPtr, clsPtr, PRIVATE_METHOD,
            objc, objv);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * Itcl_NewProcClassMethod --
 *
 *	Create a new procedure-like method for a class for Itcl.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Method
Itcl_NewProcClassMethod(
    Tcl_Interp *interp,		/* The interpreter containing the class. */
    Tcl_Class clsPtr,		/* The class to modify. */
    TclOO_PreCallProc preCallPtr,
    TclOO_PostCallProc postCallPtr,
    ProcErrorProc errProc,
    ClientData clientData,
    Tcl_Obj *nameObj,		/* The name of the method, which may be NULL;
				 * if so, up to caller to manage storage
				 * (e.g., because it is a constructor or
				 * destructor). */
    Tcl_Obj *argsObj,		/* The formal argument list for the method,
				 * which may be NULL; if so, it is equivalent
				 * to an empty list. */
    Tcl_Obj *bodyObj,		/* The body of the method, which must not be
				 * NULL. */
    ClientData *clientData2)
{
    Tcl_Method result;

    result = TclOONewProcMethodEx(interp, clsPtr, preCallPtr, postCallPtr,
           errProc, clientData, nameObj, argsObj, bodyObj,
           PUBLIC_METHOD | USE_DECLARER_NS, clientData2);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * Itcl_NewProcMethod --
 *
 *	Create a new procedure-like method for an object for Itcl.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Method
Itcl_NewProcMethod(
    Tcl_Interp *interp,		/* The interpreter containing the object. */
    Tcl_Object oPtr,		/* The object to modify. */
    TclOO_PreCallProc preCallPtr,
    TclOO_PostCallProc postCallPtr,
    ProcErrorProc errProc,
    ClientData clientData,
    Tcl_Obj *nameObj,		/* The name of the method, which must not be
				 * NULL. */
    Tcl_Obj *argsObj,		/* The formal argument list for the method,
				 * which must not be NULL. */
    Tcl_Obj *bodyObj,		/* The body of the method, which must not be
				 * NULL. */
    ClientData *clientData2)
{
    return TclOONewProcInstanceMethodEx(interp, oPtr, preCallPtr, postCallPtr,
           errProc, clientData, nameObj, argsObj, bodyObj,
           PUBLIC_METHOD | USE_DECLARER_NS, clientData2);
}

/*
 * ----------------------------------------------------------------------
 *
 * Itcl_NewForwardClassMethod --
 *
 *	Create a new forwarded method for a class for Itcl.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Method
Itcl_NewForwardClassMethod(
    Tcl_Interp *interp,
    Tcl_Class clsPtr,
    int flags,
    Tcl_Obj *nameObj,
    Tcl_Obj *prefixObj)
{
    return (Tcl_Method)TclOONewForwardMethod(interp, (Class *)clsPtr,
            flags, nameObj, prefixObj);
}

/*
 * ----------------------------------------------------------------------
 *
 * Itcl_NewForwardMethod --
 *
 *	Create a new forwarded method for an object for Itcl.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Method
Itcl_NewForwardMethod(
    Tcl_Interp *interp,
    Tcl_Object oPtr,
    int flags,
    Tcl_Obj *nameObj,
    Tcl_Obj *prefixObj)
{
    return (Tcl_Method)TclOONewForwardInstanceMethod(interp, (Object *)oPtr,
            flags, nameObj, prefixObj);
}

Tcl_Obj *
Itcl_TclOOObjectName(
    Tcl_Interp *interp,
    Object *oPtr)
{
    Tcl_Obj *namePtr;

    if (oPtr->cachedNameObj) {
        return oPtr->cachedNameObj;
    }
    namePtr = Tcl_NewObj();
    Tcl_GetCommandFullName(interp, oPtr->command, namePtr);
    Tcl_IncrRefCount(namePtr);
    oPtr->cachedNameObj = namePtr;
    return namePtr;
}

int
Itcl_SelfCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    CallFrame *framePtr = iPtr->varFramePtr;
    CallContext *contextPtr;


    if (framePtr == NULL || !(framePtr->isProcCallFrame & FRAME_IS_METHOD)) {
        Tcl_AppendResult(interp, TclGetString(objv[0]),
                " may only be called from inside a method", NULL);
        return TCL_ERROR;
    }

    contextPtr = framePtr->clientData; 

    if (objc == 1) {
        Tcl_SetObjResult(interp, Itcl_TclOOObjectName(interp, contextPtr->oPtr));
        return TCL_OK;
    }
    return TCL_ERROR;
}

