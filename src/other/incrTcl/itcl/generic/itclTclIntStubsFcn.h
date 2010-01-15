/* these functions are Tcl internal stubs so make an Itcl_* wrapper */
extern void Itcl_GetVariableFullName (Tcl_Interp * interp,
                Tcl_Var variable, Tcl_Obj * objPtr);
extern Tcl_Var Itcl_FindNamespaceVar (Tcl_Interp * interp,
                CONST char * name, Tcl_Namespace * contextNsPtr, int flags);
extern void Itcl_SetNamespaceResolvers (Tcl_Namespace * namespacePtr,
        Tcl_ResolveCmdProc * cmdProc, Tcl_ResolveVarProc * varProc,
        Tcl_ResolveCompiledVarProc * compiledVarProc);

#ifndef _TCL_PROC_DEFINED
typedef struct Tcl_Proc_ *Tcl_Proc;
#define _TCL_PROC_DEFINED 1
#endif
#ifndef _TCL_RESOLVE_DEFINED
struct Tcl_Resolve;
#endif

#define Tcl_GetOriginalCommand _Tcl_GetOriginalCommand
#define Tcl_CreateProc _Tcl_CreateProc
#define Tcl_ProcDeleteProc _Tcl_ProcDeleteProc
#define Tcl_GetObjInterpProc _Tcl_GetObjInterpProc
#define Tcl_SetNamespaceResolver _Tcl_SetNamespaceResolver

extern Tcl_Command _Tcl_GetOriginalCommand(Tcl_Command command);
extern int _Tcl_CreateProc(Tcl_Interp *interp, Tcl_Namespace *nsPtr,
	CONST char *procName, Tcl_Obj *argsPtr, Tcl_Obj *bodyPtr,
        Tcl_Proc *procPtrPtr);
extern void _Tcl_ProcDeleteProc(ClientData clientData);
extern void *_Tcl_GetObjInterpProc(void);
extern int _Tcl_SetNamespaceResolver(Tcl_Namespace *nsPtr,
        struct Tcl_Resolve *resolvePtr);
extern int Tcl_RenameCommand(Tcl_Interp *interp, const char *oldName,
	const char *newName);
extern Tcl_HashTable *Itcl_GetNamespaceChildTable(Tcl_Namespace *nsPtr);
extern Tcl_HashTable *Itcl_GetNamespaceCommandTable(Tcl_Namespace *nsPtr);
extern int Itcl_InitRewriteEnsemble(Tcl_Interp *interp, int numRemoved,
	int numInserted, int objc, Tcl_Obj *const *objv);
extern void Itcl_ResetRewriteEnsemble(Tcl_Interp *interp,
        int isRootEnsemble);


