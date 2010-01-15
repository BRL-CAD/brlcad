#ifndef _TCLOOINT_H
typedef int (*TclOO_PreCallProc)(ClientData clientData, Tcl_Interp *interp,
        Tcl_ObjectContext contextPtr, Tcl_CallFrame *framePtr, int *isFinished);
typedef int (*TclOO_PostCallProc)(ClientData clientData, Tcl_Interp *interp,
        Tcl_ObjectContext contextPtr, Tcl_Namespace *nsPtr, int result);
#endif


#define Tcl_NewProcMethod _Tcl_NewProcMethod
#define Tcl_NewProcClassMethod _Tcl_NewProcClassMethod
#define Tcl_NewForwardMethod _Tcl_NewForwardMethod
#define Tcl_NewForwardClassMethod _Tcl_NewForwardClassMethod
#define Tcl_AddToMixinSubs  _Tcl_AddToMixinSubs
#define Tcl_RemoveFromMixinSubs _Tcl_RemoveFromMixinSubs

EXTERN ClientData _Tcl_ProcPtrFromPM(ClientData clientData);
EXTERN Tcl_Method _Tcl_NewProcMethod(Tcl_Interp *interp, Tcl_Object oPtr,
        TclOO_PreCallProc preCallPtr, TclOO_PostCallProc postCallPtr,
        Tcl_ProcErrorProc errProc, ClientData clientData, Tcl_Obj *nameObj,
	Tcl_Obj *argsObj, Tcl_Obj *bodyObj, int flags, ClientData *clientData2);
EXTERN Tcl_Method _Tcl_NewProcClassMethod(Tcl_Interp *interp, Tcl_Class clsPtr,
        TclOO_PreCallProc preCallPtr, TclOO_PostCallProc postCallPtr,
        Tcl_ProcErrorProc errProc, ClientData clientData, Tcl_Obj *nameObj,
	Tcl_Obj *argsObj, Tcl_Obj *bodyObj, int flags, ClientData *clientData2);
EXTERN Tcl_Method _Tcl_NewForwardMethod(Tcl_Interp *interp, Tcl_Object oPtr,
        int flags, Tcl_Obj *nameObj, Tcl_Obj *prefixObj);
EXTERN Tcl_Method _Tcl_NewForwardClassMethod(Tcl_Interp *interp,
        Tcl_Class clsPtr, int flags, Tcl_Obj *nameObj, Tcl_Obj *prefixObj);
EXTERN void _Tcl_AddToMixinSubs(Tcl_Class subPtr, Tcl_Class superPtr);
EXTERN void _Tcl_RemoveFromMixinSubs(Tcl_Class subPtr, Tcl_Class superPtr);

