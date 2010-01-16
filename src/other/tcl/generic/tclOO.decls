# $Id$

library tclOO

######################################################################
# public API
#

interface tclOO
hooks tclOOInt

declare 0 generic {
    Tcl_Object Tcl_CopyObjectInstance(Tcl_Interp *interp,
	    Tcl_Object sourceObject, const char *targetName,
	    const char *targetNamespaceName)
}
declare 1 generic {
    Tcl_Object Tcl_GetClassAsObject(Tcl_Class clazz)
}
declare 2 generic {
    Tcl_Class Tcl_GetObjectAsClass(Tcl_Object object)
}
declare 3 generic {
    Tcl_Command Tcl_GetObjectCommand(Tcl_Object object)
}
declare 4 generic {
    Tcl_Object Tcl_GetObjectFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 5 generic {
    Tcl_Namespace *Tcl_GetObjectNamespace(Tcl_Object object)
}
declare 6 generic {
    Tcl_Class Tcl_MethodDeclarerClass(Tcl_Method method)
}
declare 7 generic {
    Tcl_Object Tcl_MethodDeclarerObject(Tcl_Method method)
}
declare 8 generic {
    int Tcl_MethodIsPublic(Tcl_Method method)
}
declare 9 generic {
    int Tcl_MethodIsType(Tcl_Method method, const Tcl_MethodType *typePtr,
	    ClientData *clientDataPtr)
}
declare 10 generic {
    Tcl_Obj *Tcl_MethodName(Tcl_Method method)
}
declare 11 generic {
    Tcl_Method Tcl_NewInstanceMethod(Tcl_Interp *interp, Tcl_Object object,
	    Tcl_Obj *nameObj, int isPublic, const Tcl_MethodType *typePtr,
	    ClientData clientData)
}
declare 12 generic {
    Tcl_Method Tcl_NewMethod(Tcl_Interp *interp, Tcl_Class cls,
	    Tcl_Obj *nameObj, int isPublic, const Tcl_MethodType *typePtr,
	    ClientData clientData)
}
declare 13 generic {
    Tcl_Object Tcl_NewObjectInstance(Tcl_Interp *interp, Tcl_Class cls,
	    const char *nameStr, const char *nsNameStr, int objc,
	    Tcl_Obj *const *objv, int skip)
}
declare 14 generic {
    int Tcl_ObjectDeleted(Tcl_Object object)
}
declare 15 generic {
    int Tcl_ObjectContextIsFiltering(Tcl_ObjectContext context)
}
declare 16 generic {
    Tcl_Method Tcl_ObjectContextMethod(Tcl_ObjectContext context)
}
declare 17 generic {
    Tcl_Object Tcl_ObjectContextObject(Tcl_ObjectContext context)
}
declare 18 generic {
    int Tcl_ObjectContextSkippedArgs(Tcl_ObjectContext context)
}
declare 19 generic {
    ClientData Tcl_ClassGetMetadata(Tcl_Class clazz,
	    const Tcl_ObjectMetadataType *typePtr)
}
declare 20 generic {
    void Tcl_ClassSetMetadata(Tcl_Class clazz,
	    const Tcl_ObjectMetadataType *typePtr, ClientData metadata)
}
declare 21 generic {
    ClientData Tcl_ObjectGetMetadata(Tcl_Object object,
	    const Tcl_ObjectMetadataType *typePtr)
}
declare 22 generic {
    void Tcl_ObjectSetMetadata(Tcl_Object object,
	    const Tcl_ObjectMetadataType *typePtr, ClientData metadata)
}
declare 23 generic {
    int Tcl_ObjectContextInvokeNext(Tcl_Interp *interp,
	    Tcl_ObjectContext context, int objc, Tcl_Obj *const *objv,
	    int skip)
}
declare 24 generic {
    Tcl_ObjectMapMethodNameProc Tcl_ObjectGetMethodNameMapper(
	    Tcl_Object object)
}
declare 25 generic {
    void Tcl_ObjectSetMethodNameMapper(Tcl_Object object,
	    Tcl_ObjectMapMethodNameProc mapMethodNameProc)
}
declare 26 generic {
    void Tcl_ClassSetConstructor(Tcl_Interp *interp, Tcl_Class clazz,
	    Tcl_Method method)
}
declare 27 generic {
    void Tcl_ClassSetDestructor(Tcl_Interp *interp, Tcl_Class clazz,
	    Tcl_Method method)
}
declare 28 generic {
    Tcl_Obj *Tcl_GetObjectName(Tcl_Interp *interp, Tcl_Object object)
}

######################################################################
# private API, exposed to support advanced OO systems that plug in on top
#

interface tclOOInt

declare 0 generic {
    Tcl_Object TclOOGetDefineCmdContext(Tcl_Interp *interp)
}
declare 1 generic {
    Tcl_Method TclOOMakeProcInstanceMethod(Tcl_Interp *interp, Object *oPtr,
	    int flags, Tcl_Obj *nameObj, Tcl_Obj *argsObj, Tcl_Obj *bodyObj,
	    const Tcl_MethodType *typePtr, ClientData clientData,
	    Proc **procPtrPtr)
}
declare 2 generic {
    Tcl_Method TclOOMakeProcMethod(Tcl_Interp *interp, Class *clsPtr,
	    int flags, Tcl_Obj *nameObj, const char *namePtr,
	    Tcl_Obj *argsObj, Tcl_Obj *bodyObj, const Tcl_MethodType *typePtr,
	    ClientData clientData, Proc **procPtrPtr)
}
declare 3 generic {
    Method *TclOONewProcInstanceMethod(Tcl_Interp *interp, Object *oPtr,
	    int flags, Tcl_Obj *nameObj, Tcl_Obj *argsObj, Tcl_Obj *bodyObj,
	    ProcedureMethod **pmPtrPtr)
}
declare 4 generic {
    Method *TclOONewProcMethod(Tcl_Interp *interp, Class *clsPtr,
	    int flags, Tcl_Obj *nameObj, Tcl_Obj *argsObj, Tcl_Obj *bodyObj,
	    ProcedureMethod **pmPtrPtr)
}
declare 5 generic {
    int TclOOObjectCmdCore(Object *oPtr, Tcl_Interp *interp, int objc,
	    Tcl_Obj *const *objv, int publicOnly, Class *startCls)
}
declare 6 generic {
    int TclOOIsReachable(Class *targetPtr, Class *startPtr)
}
declare 7 generic {
    Method *TclOONewForwardMethod(Tcl_Interp *interp, Class *clsPtr,
	    int isPublic, Tcl_Obj *nameObj, Tcl_Obj *prefixObj)
}
declare 8 generic {
    Method *TclOONewForwardInstanceMethod(Tcl_Interp *interp, Object *oPtr,
	    int isPublic, Tcl_Obj *nameObj, Tcl_Obj *prefixObj)
}
declare 9 generic {
    Tcl_Method TclOONewProcInstanceMethodEx(Tcl_Interp *interp,
	    Tcl_Object oPtr, TclOO_PreCallProc preCallPtr,
	    TclOO_PostCallProc postCallPtr, ProcErrorProc errProc,
	    ClientData clientData, Tcl_Obj *nameObj, Tcl_Obj *argsObj,
	    Tcl_Obj *bodyObj, int flags, void **internalTokenPtr)
}
declare 10 generic {
    Tcl_Method TclOONewProcMethodEx(Tcl_Interp *interp, Tcl_Class clsPtr,
	    TclOO_PreCallProc preCallPtr, TclOO_PostCallProc postCallPtr,
	    ProcErrorProc errProc, ClientData clientData, Tcl_Obj *nameObj,
	    Tcl_Obj *argsObj, Tcl_Obj *bodyObj, int flags,
	    void **internalTokenPtr)
}
declare 11 generic {
    int TclOOInvokeObject(Tcl_Interp *interp, Tcl_Object object,
	    Tcl_Class startCls, int publicPrivate, int objc,
	    Tcl_Obj *const *objv)
}
declare 12 generic {
    void TclOOObjectSetFilters(Object *oPtr, int numFilters,
	    Tcl_Obj *const *filters)
}
declare 13 generic {
    void TclOOClassSetFilters(Tcl_Interp *interp, Class *classPtr,
	    int numFilters, Tcl_Obj *const *filters)
}
declare 14 generic {
    void TclOOObjectSetMixins(Object *oPtr, int numMixins,
	    Class *const *mixins)
}
declare 15 generic {
    void TclOOClassSetMixins(Tcl_Interp *interp, Class *classPtr,
	    int numMixins, Class *const *mixins)
}

return

# Local Variables:
# mode: tcl
# End:
