# -*- tcl -*-
# $Id$

# public API
library itcl
interface itcl
hooks {itclInt}
epoch 0
scspec ITCLAPI

# Declare each of the functions in the public Tcl interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

declare 0 current {
    int Itcl_Init(Tcl_Interp *interp);
}
declare 1 current {
    int Itcl_SafeInit(Tcl_Interp *interp)
}
declare 2 current {
    int Itcl_RegisterC(Tcl_Interp *interp, CONST char *name, \
        Tcl_CmdProc *proc, ClientData clientData, \
        Tcl_CmdDeleteProc *deleteProc)
}
declare 3 current {
    int Itcl_RegisterObjC (Tcl_Interp *interp, CONST char *name, \
        Tcl_ObjCmdProc *proc, ClientData clientData, \
        Tcl_CmdDeleteProc *deleteProc)
}
declare 4 current {
    int Itcl_FindC(Tcl_Interp *interp, CONST char *name, \
	Tcl_CmdProc **argProcPtr, Tcl_ObjCmdProc **objProcPtr, \
	ClientData *cDataPtr)
}
declare 5 current {
    void Itcl_InitStack(Itcl_Stack *stack)
}
declare 6 current {
    void Itcl_DeleteStack(Itcl_Stack *stack)
}
declare 7 current {
    void Itcl_PushStack(ClientData cdata, Itcl_Stack *stack)
}
declare 8 current {
    ClientData Itcl_PopStack(Itcl_Stack *stack)
}
declare 9 current {
    ClientData Itcl_PeekStack(Itcl_Stack *stack)
}
declare 10 current {
    ClientData Itcl_GetStackValue(Itcl_Stack *stack, int pos)
}
declare 11 current {
    void Itcl_InitList(Itcl_List *listPtr)
}
declare 12 current {
    void Itcl_DeleteList(Itcl_List *listPtr)
}
declare 13 current {
    Itcl_ListElem* Itcl_CreateListElem(Itcl_List *listPtr)
}
declare 14 current {
    Itcl_ListElem* Itcl_DeleteListElem(Itcl_ListElem *elemPtr)
}
declare 15 current {
    Itcl_ListElem* Itcl_InsertList(Itcl_List *listPtr, ClientData val)
}
declare 16 current {
    Itcl_ListElem* Itcl_InsertListElem (Itcl_ListElem *pos, ClientData val)
}
declare 17 current {
    Itcl_ListElem* Itcl_AppendList(Itcl_List *listPtr, ClientData val)
}
declare 18 current {
    Itcl_ListElem* Itcl_AppendListElem(Itcl_ListElem *pos, ClientData val)
}
declare 19 current {
    void Itcl_SetListValue(Itcl_ListElem *elemPtr, ClientData val)
}
declare 20 current {
    void Itcl_EventuallyFree(ClientData cdata, Tcl_FreeProc *fproc)
}
declare 21 current {
    void Itcl_PreserveData(ClientData cdata)
}
declare 22 current {
    void Itcl_ReleaseData(ClientData cdata)
}
declare 23 current {
    Itcl_InterpState Itcl_SaveInterpState(Tcl_Interp* interp, int status)
}
declare 24 current {
    int Itcl_RestoreInterpState(Tcl_Interp* interp, Itcl_InterpState state)
}
declare 25 current {
    void Itcl_DiscardInterpState(Itcl_InterpState state)
}


# private API
interface itclInt
#
# Functions used within the package, but not considered "public"
#

declare 0 current {
    int Itcl_IsClassNamespace(Tcl_Namespace *namesp)
}
declare 1 current {
    int Itcl_IsClass (Tcl_Command cmd)
}
declare 2 current {
    ItclClass* Itcl_FindClass (Tcl_Interp* interp, CONST char* path, int autoload)
}
declare 3 current {
    int Itcl_FindObject (Tcl_Interp *interp, CONST char *name, ItclObject **roPtr)
}
declare 4 current {
    int Itcl_IsObject (Tcl_Command cmd)
}
declare 5 current {
    int Itcl_ObjectIsa (ItclObject *contextObj, ItclClass *cdefn)
}
declare 6 current {
    int Itcl_Protection (Tcl_Interp *interp, int newLevel)
}
declare 7 current {
    char* Itcl_ProtectionStr (int pLevel)
}
declare 8 current {
    int Itcl_CanAccess (ItclMemberFunc* memberPtr, Tcl_Namespace* fromNsPtr)
}
declare 9 current {
    int Itcl_CanAccessFunc (ItclMemberFunc* mfunc, Tcl_Namespace* fromNsPtr)
}
declare 11 current {
    void Itcl_ParseNamespPath (CONST char *name, Tcl_DString *buffer,
        char **head, char **tail)
}
declare 12 current {
    int Itcl_DecodeScopedCommand (Tcl_Interp *interp, CONST char *name, \
        Tcl_Namespace **rNsPtr, char **rCmdPtr)
}
declare 13 current {
    int Itcl_EvalArgs (Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
}
declare 14 current {
    Tcl_Obj* Itcl_CreateArgs (Tcl_Interp *interp, const char *string,
        int objc, Tcl_Obj *const objv[])
}
declare 17 current {
    int Itcl_GetContext (Tcl_Interp *interp, ItclClass **iclsPtrPtr, \
        ItclObject **ioPtrPtr)
}
declare 18 current {
    void Itcl_InitHierIter (ItclHierIter *iter, ItclClass *iclsPtr)
}
declare 19 current {
    void Itcl_DeleteHierIter (ItclHierIter *iter)
}
declare 20 current {
    ItclClass* Itcl_AdvanceHierIter (ItclHierIter *iter)
}
declare 21 current {
    int Itcl_FindClassesCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 22 current {
    int Itcl_FindObjectsCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 24 current {
    int Itcl_DelClassCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 25 current {
    int Itcl_DelObjectCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 26 current {
    int Itcl_ScopeCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 27 current {
    int Itcl_CodeCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 28 current {
    int Itcl_StubCreateCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 29 current {
    int Itcl_StubExistsCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 30 current {
    int Itcl_IsStub (Tcl_Command cmd)
}


#
#  Functions for manipulating classes
#

declare 31 current {
    int Itcl_CreateClass (Tcl_Interp* interp, CONST char* path, \
        ItclObjectInfo *info, ItclClass **rPtr)
}
declare 32 current {
    int Itcl_DeleteClass (Tcl_Interp *interp, ItclClass *iclsPtr)
}
declare 33 current {
    Tcl_Namespace* Itcl_FindClassNamespace (Tcl_Interp* interp, CONST char* path)
}
declare 34 current {
    int Itcl_HandleClass (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 38 current {
    void Itcl_BuildVirtualTables (ItclClass *iclsPtr)
}
declare 39 current {
    int Itcl_CreateVariable (Tcl_Interp *interp, ItclClass *iclsPtr, \
        Tcl_Obj *name, char *init, char *config, ItclVariable **ivPtr)
}
declare 40 current {
    void Itcl_DeleteVariable (char *cdata)
}
declare 41 current {
    CONST char* Itcl_GetCommonVar (Tcl_Interp *interp, CONST char *name, \
        ItclClass *contextClass)
}


#
#  Functions for manipulating objects
#

declare 45 current {
    int Itcl_DeleteObject (Tcl_Interp *interp, ItclObject *contextObj)
}
declare 46 current {
    int Itcl_DestructObject (Tcl_Interp *interp, ItclObject *contextObj, \
        int flags)
}
declare 48 current {
    CONST char* Itcl_GetInstanceVar (Tcl_Interp *interp, CONST char *name, \
        ItclObject *contextIoPtr, ItclClass *contextIclsPtr)
}

#
#  Functions for manipulating methods and procs
#

declare 50 current {
    int Itcl_BodyCmd (ClientData dummy, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 51 current {
    int Itcl_ConfigBodyCmd (ClientData dummy, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 52 current {
    int Itcl_CreateMethod (Tcl_Interp* interp, ItclClass *iclsPtr,
	Tcl_Obj *namePtr, CONST char* arglist, CONST char* body)
}
declare 53 current {
    int Itcl_CreateProc (Tcl_Interp* interp, ItclClass *iclsPtr,
	Tcl_Obj *namePtr, CONST char* arglist, CONST char* body)
}
declare 54 current {
    int Itcl_CreateMemberFunc (Tcl_Interp* interp, ItclClass *iclsPtr, \
        Tcl_Obj *name, CONST char* arglist, CONST char* body, \
	ItclMemberFunc** mfuncPtr)
}
declare 55 current {
    int Itcl_ChangeMemberFunc (Tcl_Interp* interp, ItclMemberFunc* mfunc, \
        CONST char* arglist, CONST char* body)
}
declare 56 current {
    void Itcl_DeleteMemberFunc (char *cdata)
}
declare 57 current {
    int Itcl_CreateMemberCode (Tcl_Interp* interp, ItclClass *iclsPtr, \
        CONST char* arglist, CONST char* body, ItclMemberCode** mcodePtr)
}
declare 58 current {
    void Itcl_DeleteMemberCode (char *cdata)
}
declare 59 current {
    int Itcl_GetMemberCode (Tcl_Interp* interp, ItclMemberFunc* mfunc)
}
declare 61 current {
    int Itcl_EvalMemberCode (Tcl_Interp *interp, ItclMemberFunc *mfunc, \
        ItclObject *contextObj, int objc, Tcl_Obj *CONST objv[])
}
declare 67 current {
    void Itcl_GetMemberFuncUsage (ItclMemberFunc *mfunc, \
        ItclObject *contextObj, Tcl_Obj *objPtr)
}
declare 68 current {
    int Itcl_ExecMethod (ClientData clientData, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 69 current {
    int Itcl_ExecProc (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 71 current {
    int Itcl_ConstructBase (Tcl_Interp *interp, ItclObject *contextObj, \
        ItclClass *contextClass, int objc, Tcl_Obj *CONST *objv)
}
declare 72 current {
    int Itcl_InvokeMethodIfExists (Tcl_Interp *interp, CONST char *name, \
        ItclClass *contextClass, ItclObject *contextObj, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 74 current {
    int Itcl_ReportFuncErrors (Tcl_Interp* interp, ItclMemberFunc *mfunc, \
        ItclObject *contextObj, int result)
}


#
#  Commands for parsing class definitions
#

declare 75 current {
    int Itcl_ParseInit (Tcl_Interp *interp, ItclObjectInfo *info)
}
declare 76 current {
    int Itcl_ClassCmd (ClientData clientData, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 77 current {
    int Itcl_ClassInheritCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 78 current {
    int Itcl_ClassProtectionCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 79 current {
    int Itcl_ClassConstructorCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 80 current {
    int Itcl_ClassDestructorCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 81 current {
    int Itcl_ClassMethodCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 82 current {
    int Itcl_ClassProcCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 83 current {
    int Itcl_ClassVariableCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 84 current {
    int Itcl_ClassCommonCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 85 current {
    int Itcl_ParseVarResolver (Tcl_Interp *interp, const char* name, \
        Tcl_Namespace *contextNs, int flags, Tcl_Var* rPtr)
}

#
#  Commands in the "builtin" namespace
#

declare 86 current {
    int Itcl_BiInit (Tcl_Interp *interp, ItclObjectInfo *infoPtr)
}
declare 87 current {
    int Itcl_InstallBiMethods (Tcl_Interp *interp, ItclClass *cdefn)
}
declare 88 current {
    int Itcl_BiIsaCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 89 current {
    int Itcl_BiConfigureCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 90 current {
    int Itcl_BiCgetCmd (ClientData clientData, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 91 current {
    int Itcl_BiChainCmd (ClientData dummy, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 92 current {
    int Itcl_BiInfoClassCmd (ClientData dummy, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 93 current {
    int Itcl_BiInfoInheritCmd (ClientData dummy, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 94 current {
    int Itcl_BiInfoHeritageCmd (ClientData dummy, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 95 current {
    int Itcl_BiInfoFunctionCmd (ClientData dummy, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 96 current {
    int Itcl_BiInfoVariableCmd (ClientData dummy, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 97 current {
    int Itcl_BiInfoBodyCmd (ClientData dummy, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 98 current {
    int Itcl_BiInfoArgsCmd (ClientData dummy, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 99 current {
    int Itcl_DefaultInfoCmd (ClientData dummy, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}


#
#  Ensembles
#

declare 100 current {
    int Itcl_EnsembleInit (Tcl_Interp *interp)
}
declare 101 current {
    int Itcl_CreateEnsemble (Tcl_Interp *interp, CONST char* ensName)
}
declare 102 current {
    int Itcl_AddEnsemblePart (Tcl_Interp *interp, CONST char* ensName, \
        CONST char* partName, CONST char* usageInfo, Tcl_ObjCmdProc *objProc, \
        ClientData clientData, Tcl_CmdDeleteProc *deleteProc)
}
declare 103 current {
    int Itcl_GetEnsemblePart (Tcl_Interp *interp, CONST char *ensName, \
        CONST char *partName, Tcl_CmdInfo *infoPtr)
}
declare 104 current {
    int Itcl_IsEnsemble (Tcl_CmdInfo* infoPtr)
}
declare 105 current {
    int Itcl_GetEnsembleUsage (Tcl_Interp *interp, CONST char *ensName, \
        Tcl_Obj *objPtr)
}
declare 106 current {
    int Itcl_GetEnsembleUsageForObj (Tcl_Interp *interp, Tcl_Obj *ensObjPtr, \
        Tcl_Obj *objPtr)
}
declare 107 current {
    int Itcl_EnsembleCmd (ClientData clientData, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 108 current {
    int Itcl_EnsPartCmd (ClientData clientData, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 109 current {
    int Itcl_EnsembleErrorCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 115 current {
    void Itcl_Assert (CONST char *testExpr, CONST char *fileName, int lineNum)
}
declare 116 current {
    int Itcl_IsObjectCmd (ClientData clientData, Tcl_Interp *interp, \
    int objc, Tcl_Obj *CONST objv[])
}
declare 117 current {
    int Itcl_IsClassCmd (ClientData clientData, Tcl_Interp *interp, \
    int objc, Tcl_Obj *CONST objv[])
}

#
#  new commands to use TclOO functionality
#

declare 140 current {
    int Itcl_FilterAddCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 141 current {
    int Itcl_FilterDeleteCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 142 current {
    int Itcl_ForwardAddCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 143 current {
    int Itcl_ForwardDeleteCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 144 current {
    int Itcl_MixinAddCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 145 current {
    int Itcl_MixinDeleteCmd (ClientData clientData, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}

#
#  Helper commands
#

declare 150 current {
    int Itcl_BiInfoCmd (ClientData clientData, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 151 current {
    int Itcl_BiInfoUnknownCmd (ClientData dummy, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 152 current {
    int Itcl_BiInfoVarsCmd (ClientData dummy, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 153 current {
    int Itcl_CanAccess2 (ItclClass* iclsPtr, int protection, \
        Tcl_Namespace* fromNsPtr)
}
declare 160 current {
    int Itcl_SetCallFrameResolver(Tcl_Interp *interp, \
                    Tcl_Resolve *resolvePtr)
}
declare 161 current {
    int ItclEnsembleSubCmd(ClientData clientData, Tcl_Interp *interp, \
            const char *ensembleName, int objc, Tcl_Obj *const *objv, \
            const char *functionName)
}
declare 162 current {
    Tcl_Namespace * Itcl_GetUplevelNamespace(Tcl_Interp *interp, int level)
}
declare 163 current {
    ClientData Itcl_GetCallFrameClientData(Tcl_Interp *interp)
}
declare 165 current {
    int Itcl_SetCallFrameNamespace(Tcl_Interp *interp, Tcl_Namespace *nsPtr)
}
declare 166 current {
    int Itcl_GetCallFrameObjc(Tcl_Interp *interp)
}
declare 167 current {
    Tcl_Obj * const * Itcl_GetCallFrameObjv(Tcl_Interp *interp)
}
declare 168 current {
    int Itcl_NWidgetCmd (ClientData infoPtr, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 169 current {
    int Itcl_AddOptionCmd (ClientData infoPtr, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 170 current {
    int Itcl_AddComponentCmd (ClientData infoPtr, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 171 current {
    int Itcl_BiInfoOptionCmd (ClientData dummy, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}
declare 172 current {
    int Itcl_BiInfoComponentCmd (ClientData dummy, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 173 current {
    int Itcl_RenameCommand (Tcl_Interp *interp, const char *oldName, \
	const char *newName)
}
declare 174 current {
    int Itcl_PushCallFrame(Tcl_Interp * interp, Tcl_CallFrame * framePtr, \
	Tcl_Namespace * nsPtr, int isProcCallFrame);
}
declare 175 current {
    void Itcl_PopCallFrame (Tcl_Interp * interp);
}
declare 176 current {
    Tcl_CallFrame * Itcl_GetUplevelCallFrame (Tcl_Interp * interp, \
                                    int level);
}
declare 177 current {
    Tcl_CallFrame * Itcl_ActivateCallFrame(Tcl_Interp *interp, \
            Tcl_CallFrame *framePtr);
}
