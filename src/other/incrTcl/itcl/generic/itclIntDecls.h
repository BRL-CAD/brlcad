/*
 * $Id$
 *
 * This file is (mostly) automatically generated from itcl.decls.
 */


#ifdef NOTDEF
#if defined(USE_ITCL_STUBS)

extern const char *Itcl_InitStubs(
	Tcl_Interp *, const char *version, int exact);
#define Itcl_InitStubs(interp,version,exact) Itcl_InitStubs( \
	interp, ITCL_PATCH_LEVEL, 1)
#else

#define Itcl_InitStubs(interp,version,exact) Tcl_PkgRequire(interp,"itcl",version, exact)

#endif
#endif


/* !BEGIN!: Do not edit below this line. */

#define ITCLINT_STUBS_EPOCH 0
#define ITCLINT_STUBS_REVISION 146

#if !defined(USE_ITCL_STUBS)

/*
 * Exported function declarations:
 */

/* 0 */
ITCLAPI int		Itcl_IsClassNamespace (Tcl_Namespace * namesp);
/* 1 */
ITCLAPI int		Itcl_IsClass (Tcl_Command cmd);
/* 2 */
ITCLAPI ItclClass*	Itcl_FindClass (Tcl_Interp* interp, CONST char* path, 
				int autoload);
/* 3 */
ITCLAPI int		Itcl_FindObject (Tcl_Interp * interp, 
				CONST char * name, ItclObject ** roPtr);
/* 4 */
ITCLAPI int		Itcl_IsObject (Tcl_Command cmd);
/* 5 */
ITCLAPI int		Itcl_ObjectIsa (ItclObject * contextObj, 
				ItclClass * cdefn);
/* 6 */
ITCLAPI int		Itcl_Protection (Tcl_Interp * interp, int newLevel);
/* 7 */
ITCLAPI char*		Itcl_ProtectionStr (int pLevel);
/* 8 */
ITCLAPI int		Itcl_CanAccess (ItclMemberFunc* memberPtr, 
				Tcl_Namespace* fromNsPtr);
/* 9 */
ITCLAPI int		Itcl_CanAccessFunc (ItclMemberFunc* mfunc, 
				Tcl_Namespace* fromNsPtr);
/* Slot 10 is reserved */
/* 11 */
ITCLAPI void		Itcl_ParseNamespPath (CONST char * name, 
				Tcl_DString * buffer, char ** head, 
				char ** tail);
/* 12 */
ITCLAPI int		Itcl_DecodeScopedCommand (Tcl_Interp * interp, 
				CONST char * name, Tcl_Namespace ** rNsPtr, 
				char ** rCmdPtr);
/* 13 */
ITCLAPI int		Itcl_EvalArgs (Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 14 */
ITCLAPI Tcl_Obj*	Itcl_CreateArgs (Tcl_Interp * interp, 
				const char * string, int objc, 
				Tcl_Obj *const objv[]);
/* Slot 15 is reserved */
/* Slot 16 is reserved */
/* 17 */
ITCLAPI int		Itcl_GetContext (Tcl_Interp * interp, 
				ItclClass ** iclsPtrPtr, 
				ItclObject ** ioPtrPtr);
/* 18 */
ITCLAPI void		Itcl_InitHierIter (ItclHierIter * iter, 
				ItclClass * iclsPtr);
/* 19 */
ITCLAPI void		Itcl_DeleteHierIter (ItclHierIter * iter);
/* 20 */
ITCLAPI ItclClass*	Itcl_AdvanceHierIter (ItclHierIter * iter);
/* 21 */
ITCLAPI int		Itcl_FindClassesCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 22 */
ITCLAPI int		Itcl_FindObjectsCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* Slot 23 is reserved */
/* 24 */
ITCLAPI int		Itcl_DelClassCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 25 */
ITCLAPI int		Itcl_DelObjectCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 26 */
ITCLAPI int		Itcl_ScopeCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 27 */
ITCLAPI int		Itcl_CodeCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 28 */
ITCLAPI int		Itcl_StubCreateCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 29 */
ITCLAPI int		Itcl_StubExistsCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 30 */
ITCLAPI int		Itcl_IsStub (Tcl_Command cmd);
/* 31 */
ITCLAPI int		Itcl_CreateClass (Tcl_Interp* interp, 
				CONST char* path, ItclObjectInfo * info, 
				ItclClass ** rPtr);
/* 32 */
ITCLAPI int		Itcl_DeleteClass (Tcl_Interp * interp, 
				ItclClass * iclsPtr);
/* 33 */
ITCLAPI Tcl_Namespace*	Itcl_FindClassNamespace (Tcl_Interp* interp, 
				CONST char* path);
/* 34 */
ITCLAPI int		Itcl_HandleClass (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* Slot 35 is reserved */
/* Slot 36 is reserved */
/* Slot 37 is reserved */
/* 38 */
ITCLAPI void		Itcl_BuildVirtualTables (ItclClass * iclsPtr);
/* 39 */
ITCLAPI int		Itcl_CreateVariable (Tcl_Interp * interp, 
				ItclClass * iclsPtr, Tcl_Obj * name, 
				char * init, char * config, 
				ItclVariable ** ivPtr);
/* 40 */
ITCLAPI void		Itcl_DeleteVariable (char * cdata);
/* 41 */
ITCLAPI CONST char*	Itcl_GetCommonVar (Tcl_Interp * interp, 
				CONST char * name, ItclClass * contextClass);
/* Slot 42 is reserved */
/* Slot 43 is reserved */
/* Slot 44 is reserved */
/* 45 */
ITCLAPI int		Itcl_DeleteObject (Tcl_Interp * interp, 
				ItclObject * contextObj);
/* 46 */
ITCLAPI int		Itcl_DestructObject (Tcl_Interp * interp, 
				ItclObject * contextObj, int flags);
/* Slot 47 is reserved */
/* 48 */
ITCLAPI CONST char*	Itcl_GetInstanceVar (Tcl_Interp * interp, 
				CONST char * name, ItclObject * contextIoPtr, 
				ItclClass * contextIclsPtr);
/* Slot 49 is reserved */
/* 50 */
ITCLAPI int		Itcl_BodyCmd (ClientData dummy, Tcl_Interp * interp, 
				int objc, Tcl_Obj *CONST objv[]);
/* 51 */
ITCLAPI int		Itcl_ConfigBodyCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 52 */
ITCLAPI int		Itcl_CreateMethod (Tcl_Interp* interp, 
				ItclClass * iclsPtr, Tcl_Obj * namePtr, 
				CONST char* arglist, CONST char* body);
/* 53 */
ITCLAPI int		Itcl_CreateProc (Tcl_Interp* interp, 
				ItclClass * iclsPtr, Tcl_Obj * namePtr, 
				CONST char* arglist, CONST char* body);
/* 54 */
ITCLAPI int		Itcl_CreateMemberFunc (Tcl_Interp* interp, 
				ItclClass * iclsPtr, Tcl_Obj * name, 
				CONST char* arglist, CONST char* body, 
				ItclMemberFunc** mfuncPtr);
/* 55 */
ITCLAPI int		Itcl_ChangeMemberFunc (Tcl_Interp* interp, 
				ItclMemberFunc* mfunc, CONST char* arglist, 
				CONST char* body);
/* 56 */
ITCLAPI void		Itcl_DeleteMemberFunc (char * cdata);
/* 57 */
ITCLAPI int		Itcl_CreateMemberCode (Tcl_Interp* interp, 
				ItclClass * iclsPtr, CONST char* arglist, 
				CONST char* body, ItclMemberCode** mcodePtr);
/* 58 */
ITCLAPI void		Itcl_DeleteMemberCode (char * cdata);
/* 59 */
ITCLAPI int		Itcl_GetMemberCode (Tcl_Interp* interp, 
				ItclMemberFunc* mfunc);
/* Slot 60 is reserved */
/* 61 */
ITCLAPI int		Itcl_EvalMemberCode (Tcl_Interp * interp, 
				ItclMemberFunc * mfunc, 
				ItclObject * contextObj, int objc, 
				Tcl_Obj *CONST objv[]);
/* Slot 62 is reserved */
/* Slot 63 is reserved */
/* Slot 64 is reserved */
/* Slot 65 is reserved */
/* Slot 66 is reserved */
/* 67 */
ITCLAPI void		Itcl_GetMemberFuncUsage (ItclMemberFunc * mfunc, 
				ItclObject * contextObj, Tcl_Obj * objPtr);
/* 68 */
ITCLAPI int		Itcl_ExecMethod (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 69 */
ITCLAPI int		Itcl_ExecProc (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* Slot 70 is reserved */
/* 71 */
ITCLAPI int		Itcl_ConstructBase (Tcl_Interp * interp, 
				ItclObject * contextObj, 
				ItclClass * contextClass, int objc, 
				Tcl_Obj *CONST * objv);
/* 72 */
ITCLAPI int		Itcl_InvokeMethodIfExists (Tcl_Interp * interp, 
				CONST char * name, ItclClass * contextClass, 
				ItclObject * contextObj, int objc, 
				Tcl_Obj *CONST objv[]);
/* Slot 73 is reserved */
/* 74 */
ITCLAPI int		Itcl_ReportFuncErrors (Tcl_Interp* interp, 
				ItclMemberFunc * mfunc, 
				ItclObject * contextObj, int result);
/* 75 */
ITCLAPI int		Itcl_ParseInit (Tcl_Interp * interp, 
				ItclObjectInfo * info);
/* 76 */
ITCLAPI int		Itcl_ClassCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 77 */
ITCLAPI int		Itcl_ClassInheritCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 78 */
ITCLAPI int		Itcl_ClassProtectionCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 79 */
ITCLAPI int		Itcl_ClassConstructorCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 80 */
ITCLAPI int		Itcl_ClassDestructorCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 81 */
ITCLAPI int		Itcl_ClassMethodCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 82 */
ITCLAPI int		Itcl_ClassProcCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 83 */
ITCLAPI int		Itcl_ClassVariableCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 84 */
ITCLAPI int		Itcl_ClassCommonCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 85 */
ITCLAPI int		Itcl_ParseVarResolver (Tcl_Interp * interp, 
				const char* name, Tcl_Namespace * contextNs, 
				int flags, Tcl_Var* rPtr);
/* 86 */
ITCLAPI int		Itcl_BiInit (Tcl_Interp * interp, 
				ItclObjectInfo * infoPtr);
/* 87 */
ITCLAPI int		Itcl_InstallBiMethods (Tcl_Interp * interp, 
				ItclClass * cdefn);
/* 88 */
ITCLAPI int		Itcl_BiIsaCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 89 */
ITCLAPI int		Itcl_BiConfigureCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 90 */
ITCLAPI int		Itcl_BiCgetCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 91 */
ITCLAPI int		Itcl_BiChainCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 92 */
ITCLAPI int		Itcl_BiInfoClassCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 93 */
ITCLAPI int		Itcl_BiInfoInheritCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 94 */
ITCLAPI int		Itcl_BiInfoHeritageCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 95 */
ITCLAPI int		Itcl_BiInfoFunctionCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 96 */
ITCLAPI int		Itcl_BiInfoVariableCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 97 */
ITCLAPI int		Itcl_BiInfoBodyCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 98 */
ITCLAPI int		Itcl_BiInfoArgsCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 99 */
ITCLAPI int		Itcl_DefaultInfoCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 100 */
ITCLAPI int		Itcl_EnsembleInit (Tcl_Interp * interp);
/* 101 */
ITCLAPI int		Itcl_CreateEnsemble (Tcl_Interp * interp, 
				CONST char* ensName);
/* 102 */
ITCLAPI int		Itcl_AddEnsemblePart (Tcl_Interp * interp, 
				CONST char* ensName, CONST char* partName, 
				CONST char* usageInfo, 
				Tcl_ObjCmdProc * objProc, 
				ClientData clientData, 
				Tcl_CmdDeleteProc * deleteProc);
/* 103 */
ITCLAPI int		Itcl_GetEnsemblePart (Tcl_Interp * interp, 
				CONST char * ensName, CONST char * partName, 
				Tcl_CmdInfo * infoPtr);
/* 104 */
ITCLAPI int		Itcl_IsEnsemble (Tcl_CmdInfo* infoPtr);
/* 105 */
ITCLAPI int		Itcl_GetEnsembleUsage (Tcl_Interp * interp, 
				CONST char * ensName, Tcl_Obj * objPtr);
/* 106 */
ITCLAPI int		Itcl_GetEnsembleUsageForObj (Tcl_Interp * interp, 
				Tcl_Obj * ensObjPtr, Tcl_Obj * objPtr);
/* 107 */
ITCLAPI int		Itcl_EnsembleCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 108 */
ITCLAPI int		Itcl_EnsPartCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 109 */
ITCLAPI int		Itcl_EnsembleErrorCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* Slot 110 is reserved */
/* Slot 111 is reserved */
/* Slot 112 is reserved */
/* Slot 113 is reserved */
/* Slot 114 is reserved */
/* 115 */
ITCLAPI void		Itcl_Assert (CONST char * testExpr, 
				CONST char * fileName, int lineNum);
/* 116 */
ITCLAPI int		Itcl_IsObjectCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 117 */
ITCLAPI int		Itcl_IsClassCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* Slot 118 is reserved */
/* Slot 119 is reserved */
/* Slot 120 is reserved */
/* Slot 121 is reserved */
/* Slot 122 is reserved */
/* Slot 123 is reserved */
/* Slot 124 is reserved */
/* Slot 125 is reserved */
/* Slot 126 is reserved */
/* Slot 127 is reserved */
/* Slot 128 is reserved */
/* Slot 129 is reserved */
/* Slot 130 is reserved */
/* Slot 131 is reserved */
/* Slot 132 is reserved */
/* Slot 133 is reserved */
/* Slot 134 is reserved */
/* Slot 135 is reserved */
/* Slot 136 is reserved */
/* Slot 137 is reserved */
/* Slot 138 is reserved */
/* Slot 139 is reserved */
/* 140 */
ITCLAPI int		Itcl_FilterAddCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 141 */
ITCLAPI int		Itcl_FilterDeleteCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 142 */
ITCLAPI int		Itcl_ForwardAddCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 143 */
ITCLAPI int		Itcl_ForwardDeleteCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 144 */
ITCLAPI int		Itcl_MixinAddCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 145 */
ITCLAPI int		Itcl_MixinDeleteCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* Slot 146 is reserved */
/* Slot 147 is reserved */
/* Slot 148 is reserved */
/* Slot 149 is reserved */
/* 150 */
ITCLAPI int		Itcl_BiInfoCmd (ClientData clientData, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 151 */
ITCLAPI int		Itcl_BiInfoUnknownCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 152 */
ITCLAPI int		Itcl_BiInfoVarsCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 153 */
ITCLAPI int		Itcl_CanAccess2 (ItclClass* iclsPtr, int protection, 
				Tcl_Namespace* fromNsPtr);
/* Slot 154 is reserved */
/* Slot 155 is reserved */
/* Slot 156 is reserved */
/* Slot 157 is reserved */
/* Slot 158 is reserved */
/* Slot 159 is reserved */
/* 160 */
ITCLAPI int		Itcl_SetCallFrameResolver (Tcl_Interp * interp, 
				Tcl_Resolve * resolvePtr);
/* 161 */
ITCLAPI int		ItclEnsembleSubCmd (ClientData clientData, 
				Tcl_Interp * interp, 
				const char * ensembleName, int objc, 
				Tcl_Obj *const * objv, 
				const char * functionName);
/* 162 */
ITCLAPI Tcl_Namespace *	 Itcl_GetUplevelNamespace (Tcl_Interp * interp, 
				int level);
/* 163 */
ITCLAPI ClientData	Itcl_GetCallFrameClientData (Tcl_Interp * interp);
/* Slot 164 is reserved */
/* 165 */
ITCLAPI int		Itcl_SetCallFrameNamespace (Tcl_Interp * interp, 
				Tcl_Namespace * nsPtr);
/* 166 */
ITCLAPI int		Itcl_GetCallFrameObjc (Tcl_Interp * interp);
/* 167 */
ITCLAPI Tcl_Obj * const * Itcl_GetCallFrameObjv (Tcl_Interp * interp);
/* 168 */
ITCLAPI int		Itcl_NWidgetCmd (ClientData infoPtr, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 169 */
ITCLAPI int		Itcl_AddOptionCmd (ClientData infoPtr, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 170 */
ITCLAPI int		Itcl_AddComponentCmd (ClientData infoPtr, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 171 */
ITCLAPI int		Itcl_BiInfoOptionCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 172 */
ITCLAPI int		Itcl_BiInfoComponentCmd (ClientData dummy, 
				Tcl_Interp * interp, int objc, 
				Tcl_Obj *CONST objv[]);
/* 173 */
ITCLAPI int		Itcl_RenameCommand (Tcl_Interp * interp, 
				const char * oldName, const char * newName);
/* 174 */
ITCLAPI int		Itcl_PushCallFrame (Tcl_Interp * interp, 
				Tcl_CallFrame * framePtr, 
				Tcl_Namespace * nsPtr, int isProcCallFrame);
/* 175 */
ITCLAPI void		Itcl_PopCallFrame (Tcl_Interp * interp);
/* 176 */
ITCLAPI Tcl_CallFrame *	 Itcl_GetUplevelCallFrame (Tcl_Interp * interp, 
				int level);
/* 177 */
ITCLAPI Tcl_CallFrame *	 Itcl_ActivateCallFrame (Tcl_Interp * interp, 
				Tcl_CallFrame * framePtr);

#endif /* !defined(USE_ITCL_STUBS) */

typedef struct ItclIntStubs {
    int magic;
    int epoch;
    int revision;
    struct ItclIntStubHooks *hooks;

    int (*itcl_IsClassNamespace) (Tcl_Namespace * namesp); /* 0 */
    int (*itcl_IsClass) (Tcl_Command cmd); /* 1 */
    ItclClass* (*itcl_FindClass) (Tcl_Interp* interp, CONST char* path, int autoload); /* 2 */
    int (*itcl_FindObject) (Tcl_Interp * interp, CONST char * name, ItclObject ** roPtr); /* 3 */
    int (*itcl_IsObject) (Tcl_Command cmd); /* 4 */
    int (*itcl_ObjectIsa) (ItclObject * contextObj, ItclClass * cdefn); /* 5 */
    int (*itcl_Protection) (Tcl_Interp * interp, int newLevel); /* 6 */
    char* (*itcl_ProtectionStr) (int pLevel); /* 7 */
    int (*itcl_CanAccess) (ItclMemberFunc* memberPtr, Tcl_Namespace* fromNsPtr); /* 8 */
    int (*itcl_CanAccessFunc) (ItclMemberFunc* mfunc, Tcl_Namespace* fromNsPtr); /* 9 */
    void (*reserved10)(void);
    void (*itcl_ParseNamespPath) (CONST char * name, Tcl_DString * buffer, char ** head, char ** tail); /* 11 */
    int (*itcl_DecodeScopedCommand) (Tcl_Interp * interp, CONST char * name, Tcl_Namespace ** rNsPtr, char ** rCmdPtr); /* 12 */
    int (*itcl_EvalArgs) (Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 13 */
    Tcl_Obj* (*itcl_CreateArgs) (Tcl_Interp * interp, const char * string, int objc, Tcl_Obj *const objv[]); /* 14 */
    void (*reserved15)(void);
    void (*reserved16)(void);
    int (*itcl_GetContext) (Tcl_Interp * interp, ItclClass ** iclsPtrPtr, ItclObject ** ioPtrPtr); /* 17 */
    void (*itcl_InitHierIter) (ItclHierIter * iter, ItclClass * iclsPtr); /* 18 */
    void (*itcl_DeleteHierIter) (ItclHierIter * iter); /* 19 */
    ItclClass* (*itcl_AdvanceHierIter) (ItclHierIter * iter); /* 20 */
    int (*itcl_FindClassesCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 21 */
    int (*itcl_FindObjectsCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 22 */
    void (*reserved23)(void);
    int (*itcl_DelClassCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 24 */
    int (*itcl_DelObjectCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 25 */
    int (*itcl_ScopeCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 26 */
    int (*itcl_CodeCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 27 */
    int (*itcl_StubCreateCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 28 */
    int (*itcl_StubExistsCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 29 */
    int (*itcl_IsStub) (Tcl_Command cmd); /* 30 */
    int (*itcl_CreateClass) (Tcl_Interp* interp, CONST char* path, ItclObjectInfo * info, ItclClass ** rPtr); /* 31 */
    int (*itcl_DeleteClass) (Tcl_Interp * interp, ItclClass * iclsPtr); /* 32 */
    Tcl_Namespace* (*itcl_FindClassNamespace) (Tcl_Interp* interp, CONST char* path); /* 33 */
    int (*itcl_HandleClass) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 34 */
    void (*reserved35)(void);
    void (*reserved36)(void);
    void (*reserved37)(void);
    void (*itcl_BuildVirtualTables) (ItclClass * iclsPtr); /* 38 */
    int (*itcl_CreateVariable) (Tcl_Interp * interp, ItclClass * iclsPtr, Tcl_Obj * name, char * init, char * config, ItclVariable ** ivPtr); /* 39 */
    void (*itcl_DeleteVariable) (char * cdata); /* 40 */
    CONST char* (*itcl_GetCommonVar) (Tcl_Interp * interp, CONST char * name, ItclClass * contextClass); /* 41 */
    void (*reserved42)(void);
    void (*reserved43)(void);
    void (*reserved44)(void);
    int (*itcl_DeleteObject) (Tcl_Interp * interp, ItclObject * contextObj); /* 45 */
    int (*itcl_DestructObject) (Tcl_Interp * interp, ItclObject * contextObj, int flags); /* 46 */
    void (*reserved47)(void);
    CONST char* (*itcl_GetInstanceVar) (Tcl_Interp * interp, CONST char * name, ItclObject * contextIoPtr, ItclClass * contextIclsPtr); /* 48 */
    void (*reserved49)(void);
    int (*itcl_BodyCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 50 */
    int (*itcl_ConfigBodyCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 51 */
    int (*itcl_CreateMethod) (Tcl_Interp* interp, ItclClass * iclsPtr, Tcl_Obj * namePtr, CONST char* arglist, CONST char* body); /* 52 */
    int (*itcl_CreateProc) (Tcl_Interp* interp, ItclClass * iclsPtr, Tcl_Obj * namePtr, CONST char* arglist, CONST char* body); /* 53 */
    int (*itcl_CreateMemberFunc) (Tcl_Interp* interp, ItclClass * iclsPtr, Tcl_Obj * name, CONST char* arglist, CONST char* body, ItclMemberFunc** mfuncPtr); /* 54 */
    int (*itcl_ChangeMemberFunc) (Tcl_Interp* interp, ItclMemberFunc* mfunc, CONST char* arglist, CONST char* body); /* 55 */
    void (*itcl_DeleteMemberFunc) (char * cdata); /* 56 */
    int (*itcl_CreateMemberCode) (Tcl_Interp* interp, ItclClass * iclsPtr, CONST char* arglist, CONST char* body, ItclMemberCode** mcodePtr); /* 57 */
    void (*itcl_DeleteMemberCode) (char * cdata); /* 58 */
    int (*itcl_GetMemberCode) (Tcl_Interp* interp, ItclMemberFunc* mfunc); /* 59 */
    void (*reserved60)(void);
    int (*itcl_EvalMemberCode) (Tcl_Interp * interp, ItclMemberFunc * mfunc, ItclObject * contextObj, int objc, Tcl_Obj *CONST objv[]); /* 61 */
    void (*reserved62)(void);
    void (*reserved63)(void);
    void (*reserved64)(void);
    void (*reserved65)(void);
    void (*reserved66)(void);
    void (*itcl_GetMemberFuncUsage) (ItclMemberFunc * mfunc, ItclObject * contextObj, Tcl_Obj * objPtr); /* 67 */
    int (*itcl_ExecMethod) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 68 */
    int (*itcl_ExecProc) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 69 */
    void (*reserved70)(void);
    int (*itcl_ConstructBase) (Tcl_Interp * interp, ItclObject * contextObj, ItclClass * contextClass, int objc, Tcl_Obj *CONST * objv); /* 71 */
    int (*itcl_InvokeMethodIfExists) (Tcl_Interp * interp, CONST char * name, ItclClass * contextClass, ItclObject * contextObj, int objc, Tcl_Obj *CONST objv[]); /* 72 */
    void (*reserved73)(void);
    int (*itcl_ReportFuncErrors) (Tcl_Interp* interp, ItclMemberFunc * mfunc, ItclObject * contextObj, int result); /* 74 */
    int (*itcl_ParseInit) (Tcl_Interp * interp, ItclObjectInfo * info); /* 75 */
    int (*itcl_ClassCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 76 */
    int (*itcl_ClassInheritCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 77 */
    int (*itcl_ClassProtectionCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 78 */
    int (*itcl_ClassConstructorCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 79 */
    int (*itcl_ClassDestructorCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 80 */
    int (*itcl_ClassMethodCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 81 */
    int (*itcl_ClassProcCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 82 */
    int (*itcl_ClassVariableCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 83 */
    int (*itcl_ClassCommonCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 84 */
    int (*itcl_ParseVarResolver) (Tcl_Interp * interp, const char* name, Tcl_Namespace * contextNs, int flags, Tcl_Var* rPtr); /* 85 */
    int (*itcl_BiInit) (Tcl_Interp * interp, ItclObjectInfo * infoPtr); /* 86 */
    int (*itcl_InstallBiMethods) (Tcl_Interp * interp, ItclClass * cdefn); /* 87 */
    int (*itcl_BiIsaCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 88 */
    int (*itcl_BiConfigureCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 89 */
    int (*itcl_BiCgetCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 90 */
    int (*itcl_BiChainCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 91 */
    int (*itcl_BiInfoClassCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 92 */
    int (*itcl_BiInfoInheritCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 93 */
    int (*itcl_BiInfoHeritageCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 94 */
    int (*itcl_BiInfoFunctionCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 95 */
    int (*itcl_BiInfoVariableCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 96 */
    int (*itcl_BiInfoBodyCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 97 */
    int (*itcl_BiInfoArgsCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 98 */
    int (*itcl_DefaultInfoCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 99 */
    int (*itcl_EnsembleInit) (Tcl_Interp * interp); /* 100 */
    int (*itcl_CreateEnsemble) (Tcl_Interp * interp, CONST char* ensName); /* 101 */
    int (*itcl_AddEnsemblePart) (Tcl_Interp * interp, CONST char* ensName, CONST char* partName, CONST char* usageInfo, Tcl_ObjCmdProc * objProc, ClientData clientData, Tcl_CmdDeleteProc * deleteProc); /* 102 */
    int (*itcl_GetEnsemblePart) (Tcl_Interp * interp, CONST char * ensName, CONST char * partName, Tcl_CmdInfo * infoPtr); /* 103 */
    int (*itcl_IsEnsemble) (Tcl_CmdInfo* infoPtr); /* 104 */
    int (*itcl_GetEnsembleUsage) (Tcl_Interp * interp, CONST char * ensName, Tcl_Obj * objPtr); /* 105 */
    int (*itcl_GetEnsembleUsageForObj) (Tcl_Interp * interp, Tcl_Obj * ensObjPtr, Tcl_Obj * objPtr); /* 106 */
    int (*itcl_EnsembleCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 107 */
    int (*itcl_EnsPartCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 108 */
    int (*itcl_EnsembleErrorCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 109 */
    void (*reserved110)(void);
    void (*reserved111)(void);
    void (*reserved112)(void);
    void (*reserved113)(void);
    void (*reserved114)(void);
    void (*itcl_Assert) (CONST char * testExpr, CONST char * fileName, int lineNum); /* 115 */
    int (*itcl_IsObjectCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 116 */
    int (*itcl_IsClassCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 117 */
    void (*reserved118)(void);
    void (*reserved119)(void);
    void (*reserved120)(void);
    void (*reserved121)(void);
    void (*reserved122)(void);
    void (*reserved123)(void);
    void (*reserved124)(void);
    void (*reserved125)(void);
    void (*reserved126)(void);
    void (*reserved127)(void);
    void (*reserved128)(void);
    void (*reserved129)(void);
    void (*reserved130)(void);
    void (*reserved131)(void);
    void (*reserved132)(void);
    void (*reserved133)(void);
    void (*reserved134)(void);
    void (*reserved135)(void);
    void (*reserved136)(void);
    void (*reserved137)(void);
    void (*reserved138)(void);
    void (*reserved139)(void);
    int (*itcl_FilterAddCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 140 */
    int (*itcl_FilterDeleteCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 141 */
    int (*itcl_ForwardAddCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 142 */
    int (*itcl_ForwardDeleteCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 143 */
    int (*itcl_MixinAddCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 144 */
    int (*itcl_MixinDeleteCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 145 */
    void (*reserved146)(void);
    void (*reserved147)(void);
    void (*reserved148)(void);
    void (*reserved149)(void);
    int (*itcl_BiInfoCmd) (ClientData clientData, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 150 */
    int (*itcl_BiInfoUnknownCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 151 */
    int (*itcl_BiInfoVarsCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 152 */
    int (*itcl_CanAccess2) (ItclClass* iclsPtr, int protection, Tcl_Namespace* fromNsPtr); /* 153 */
    void (*reserved154)(void);
    void (*reserved155)(void);
    void (*reserved156)(void);
    void (*reserved157)(void);
    void (*reserved158)(void);
    void (*reserved159)(void);
    int (*itcl_SetCallFrameResolver) (Tcl_Interp * interp, Tcl_Resolve * resolvePtr); /* 160 */
    int (*itclEnsembleSubCmd) (ClientData clientData, Tcl_Interp * interp, const char * ensembleName, int objc, Tcl_Obj *const * objv, const char * functionName); /* 161 */
    Tcl_Namespace * (*itcl_GetUplevelNamespace) (Tcl_Interp * interp, int level); /* 162 */
    ClientData (*itcl_GetCallFrameClientData) (Tcl_Interp * interp); /* 163 */
    void (*reserved164)(void);
    int (*itcl_SetCallFrameNamespace) (Tcl_Interp * interp, Tcl_Namespace * nsPtr); /* 165 */
    int (*itcl_GetCallFrameObjc) (Tcl_Interp * interp); /* 166 */
    Tcl_Obj * const * (*itcl_GetCallFrameObjv) (Tcl_Interp * interp); /* 167 */
    int (*itcl_NWidgetCmd) (ClientData infoPtr, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 168 */
    int (*itcl_AddOptionCmd) (ClientData infoPtr, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 169 */
    int (*itcl_AddComponentCmd) (ClientData infoPtr, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 170 */
    int (*itcl_BiInfoOptionCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 171 */
    int (*itcl_BiInfoComponentCmd) (ClientData dummy, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 172 */
    int (*itcl_RenameCommand) (Tcl_Interp * interp, const char * oldName, const char * newName); /* 173 */
    int (*itcl_PushCallFrame) (Tcl_Interp * interp, Tcl_CallFrame * framePtr, Tcl_Namespace * nsPtr, int isProcCallFrame); /* 174 */
    void (*itcl_PopCallFrame) (Tcl_Interp * interp); /* 175 */
    Tcl_CallFrame * (*itcl_GetUplevelCallFrame) (Tcl_Interp * interp, int level); /* 176 */
    Tcl_CallFrame * (*itcl_ActivateCallFrame) (Tcl_Interp * interp, Tcl_CallFrame * framePtr); /* 177 */
} ItclIntStubs;

#ifdef __cplusplus
extern "C" {
#endif
extern const ItclIntStubs *itclIntStubsPtr;
#ifdef __cplusplus
}
#endif

#if defined(USE_ITCL_STUBS)

/*
 * Inline function declarations:
 */

#ifndef Itcl_IsClassNamespace
#define Itcl_IsClassNamespace \
	(itclIntStubsPtr->itcl_IsClassNamespace) /* 0 */
#endif
#ifndef Itcl_IsClass
#define Itcl_IsClass \
	(itclIntStubsPtr->itcl_IsClass) /* 1 */
#endif
#ifndef Itcl_FindClass
#define Itcl_FindClass \
	(itclIntStubsPtr->itcl_FindClass) /* 2 */
#endif
#ifndef Itcl_FindObject
#define Itcl_FindObject \
	(itclIntStubsPtr->itcl_FindObject) /* 3 */
#endif
#ifndef Itcl_IsObject
#define Itcl_IsObject \
	(itclIntStubsPtr->itcl_IsObject) /* 4 */
#endif
#ifndef Itcl_ObjectIsa
#define Itcl_ObjectIsa \
	(itclIntStubsPtr->itcl_ObjectIsa) /* 5 */
#endif
#ifndef Itcl_Protection
#define Itcl_Protection \
	(itclIntStubsPtr->itcl_Protection) /* 6 */
#endif
#ifndef Itcl_ProtectionStr
#define Itcl_ProtectionStr \
	(itclIntStubsPtr->itcl_ProtectionStr) /* 7 */
#endif
#ifndef Itcl_CanAccess
#define Itcl_CanAccess \
	(itclIntStubsPtr->itcl_CanAccess) /* 8 */
#endif
#ifndef Itcl_CanAccessFunc
#define Itcl_CanAccessFunc \
	(itclIntStubsPtr->itcl_CanAccessFunc) /* 9 */
#endif
/* Slot 10 is reserved */
#ifndef Itcl_ParseNamespPath
#define Itcl_ParseNamespPath \
	(itclIntStubsPtr->itcl_ParseNamespPath) /* 11 */
#endif
#ifndef Itcl_DecodeScopedCommand
#define Itcl_DecodeScopedCommand \
	(itclIntStubsPtr->itcl_DecodeScopedCommand) /* 12 */
#endif
#ifndef Itcl_EvalArgs
#define Itcl_EvalArgs \
	(itclIntStubsPtr->itcl_EvalArgs) /* 13 */
#endif
#ifndef Itcl_CreateArgs
#define Itcl_CreateArgs \
	(itclIntStubsPtr->itcl_CreateArgs) /* 14 */
#endif
/* Slot 15 is reserved */
/* Slot 16 is reserved */
#ifndef Itcl_GetContext
#define Itcl_GetContext \
	(itclIntStubsPtr->itcl_GetContext) /* 17 */
#endif
#ifndef Itcl_InitHierIter
#define Itcl_InitHierIter \
	(itclIntStubsPtr->itcl_InitHierIter) /* 18 */
#endif
#ifndef Itcl_DeleteHierIter
#define Itcl_DeleteHierIter \
	(itclIntStubsPtr->itcl_DeleteHierIter) /* 19 */
#endif
#ifndef Itcl_AdvanceHierIter
#define Itcl_AdvanceHierIter \
	(itclIntStubsPtr->itcl_AdvanceHierIter) /* 20 */
#endif
#ifndef Itcl_FindClassesCmd
#define Itcl_FindClassesCmd \
	(itclIntStubsPtr->itcl_FindClassesCmd) /* 21 */
#endif
#ifndef Itcl_FindObjectsCmd
#define Itcl_FindObjectsCmd \
	(itclIntStubsPtr->itcl_FindObjectsCmd) /* 22 */
#endif
/* Slot 23 is reserved */
#ifndef Itcl_DelClassCmd
#define Itcl_DelClassCmd \
	(itclIntStubsPtr->itcl_DelClassCmd) /* 24 */
#endif
#ifndef Itcl_DelObjectCmd
#define Itcl_DelObjectCmd \
	(itclIntStubsPtr->itcl_DelObjectCmd) /* 25 */
#endif
#ifndef Itcl_ScopeCmd
#define Itcl_ScopeCmd \
	(itclIntStubsPtr->itcl_ScopeCmd) /* 26 */
#endif
#ifndef Itcl_CodeCmd
#define Itcl_CodeCmd \
	(itclIntStubsPtr->itcl_CodeCmd) /* 27 */
#endif
#ifndef Itcl_StubCreateCmd
#define Itcl_StubCreateCmd \
	(itclIntStubsPtr->itcl_StubCreateCmd) /* 28 */
#endif
#ifndef Itcl_StubExistsCmd
#define Itcl_StubExistsCmd \
	(itclIntStubsPtr->itcl_StubExistsCmd) /* 29 */
#endif
#ifndef Itcl_IsStub
#define Itcl_IsStub \
	(itclIntStubsPtr->itcl_IsStub) /* 30 */
#endif
#ifndef Itcl_CreateClass
#define Itcl_CreateClass \
	(itclIntStubsPtr->itcl_CreateClass) /* 31 */
#endif
#ifndef Itcl_DeleteClass
#define Itcl_DeleteClass \
	(itclIntStubsPtr->itcl_DeleteClass) /* 32 */
#endif
#ifndef Itcl_FindClassNamespace
#define Itcl_FindClassNamespace \
	(itclIntStubsPtr->itcl_FindClassNamespace) /* 33 */
#endif
#ifndef Itcl_HandleClass
#define Itcl_HandleClass \
	(itclIntStubsPtr->itcl_HandleClass) /* 34 */
#endif
/* Slot 35 is reserved */
/* Slot 36 is reserved */
/* Slot 37 is reserved */
#ifndef Itcl_BuildVirtualTables
#define Itcl_BuildVirtualTables \
	(itclIntStubsPtr->itcl_BuildVirtualTables) /* 38 */
#endif
#ifndef Itcl_CreateVariable
#define Itcl_CreateVariable \
	(itclIntStubsPtr->itcl_CreateVariable) /* 39 */
#endif
#ifndef Itcl_DeleteVariable
#define Itcl_DeleteVariable \
	(itclIntStubsPtr->itcl_DeleteVariable) /* 40 */
#endif
#ifndef Itcl_GetCommonVar
#define Itcl_GetCommonVar \
	(itclIntStubsPtr->itcl_GetCommonVar) /* 41 */
#endif
/* Slot 42 is reserved */
/* Slot 43 is reserved */
/* Slot 44 is reserved */
#ifndef Itcl_DeleteObject
#define Itcl_DeleteObject \
	(itclIntStubsPtr->itcl_DeleteObject) /* 45 */
#endif
#ifndef Itcl_DestructObject
#define Itcl_DestructObject \
	(itclIntStubsPtr->itcl_DestructObject) /* 46 */
#endif
/* Slot 47 is reserved */
#ifndef Itcl_GetInstanceVar
#define Itcl_GetInstanceVar \
	(itclIntStubsPtr->itcl_GetInstanceVar) /* 48 */
#endif
/* Slot 49 is reserved */
#ifndef Itcl_BodyCmd
#define Itcl_BodyCmd \
	(itclIntStubsPtr->itcl_BodyCmd) /* 50 */
#endif
#ifndef Itcl_ConfigBodyCmd
#define Itcl_ConfigBodyCmd \
	(itclIntStubsPtr->itcl_ConfigBodyCmd) /* 51 */
#endif
#ifndef Itcl_CreateMethod
#define Itcl_CreateMethod \
	(itclIntStubsPtr->itcl_CreateMethod) /* 52 */
#endif
#ifndef Itcl_CreateProc
#define Itcl_CreateProc \
	(itclIntStubsPtr->itcl_CreateProc) /* 53 */
#endif
#ifndef Itcl_CreateMemberFunc
#define Itcl_CreateMemberFunc \
	(itclIntStubsPtr->itcl_CreateMemberFunc) /* 54 */
#endif
#ifndef Itcl_ChangeMemberFunc
#define Itcl_ChangeMemberFunc \
	(itclIntStubsPtr->itcl_ChangeMemberFunc) /* 55 */
#endif
#ifndef Itcl_DeleteMemberFunc
#define Itcl_DeleteMemberFunc \
	(itclIntStubsPtr->itcl_DeleteMemberFunc) /* 56 */
#endif
#ifndef Itcl_CreateMemberCode
#define Itcl_CreateMemberCode \
	(itclIntStubsPtr->itcl_CreateMemberCode) /* 57 */
#endif
#ifndef Itcl_DeleteMemberCode
#define Itcl_DeleteMemberCode \
	(itclIntStubsPtr->itcl_DeleteMemberCode) /* 58 */
#endif
#ifndef Itcl_GetMemberCode
#define Itcl_GetMemberCode \
	(itclIntStubsPtr->itcl_GetMemberCode) /* 59 */
#endif
/* Slot 60 is reserved */
#ifndef Itcl_EvalMemberCode
#define Itcl_EvalMemberCode \
	(itclIntStubsPtr->itcl_EvalMemberCode) /* 61 */
#endif
/* Slot 62 is reserved */
/* Slot 63 is reserved */
/* Slot 64 is reserved */
/* Slot 65 is reserved */
/* Slot 66 is reserved */
#ifndef Itcl_GetMemberFuncUsage
#define Itcl_GetMemberFuncUsage \
	(itclIntStubsPtr->itcl_GetMemberFuncUsage) /* 67 */
#endif
#ifndef Itcl_ExecMethod
#define Itcl_ExecMethod \
	(itclIntStubsPtr->itcl_ExecMethod) /* 68 */
#endif
#ifndef Itcl_ExecProc
#define Itcl_ExecProc \
	(itclIntStubsPtr->itcl_ExecProc) /* 69 */
#endif
/* Slot 70 is reserved */
#ifndef Itcl_ConstructBase
#define Itcl_ConstructBase \
	(itclIntStubsPtr->itcl_ConstructBase) /* 71 */
#endif
#ifndef Itcl_InvokeMethodIfExists
#define Itcl_InvokeMethodIfExists \
	(itclIntStubsPtr->itcl_InvokeMethodIfExists) /* 72 */
#endif
/* Slot 73 is reserved */
#ifndef Itcl_ReportFuncErrors
#define Itcl_ReportFuncErrors \
	(itclIntStubsPtr->itcl_ReportFuncErrors) /* 74 */
#endif
#ifndef Itcl_ParseInit
#define Itcl_ParseInit \
	(itclIntStubsPtr->itcl_ParseInit) /* 75 */
#endif
#ifndef Itcl_ClassCmd
#define Itcl_ClassCmd \
	(itclIntStubsPtr->itcl_ClassCmd) /* 76 */
#endif
#ifndef Itcl_ClassInheritCmd
#define Itcl_ClassInheritCmd \
	(itclIntStubsPtr->itcl_ClassInheritCmd) /* 77 */
#endif
#ifndef Itcl_ClassProtectionCmd
#define Itcl_ClassProtectionCmd \
	(itclIntStubsPtr->itcl_ClassProtectionCmd) /* 78 */
#endif
#ifndef Itcl_ClassConstructorCmd
#define Itcl_ClassConstructorCmd \
	(itclIntStubsPtr->itcl_ClassConstructorCmd) /* 79 */
#endif
#ifndef Itcl_ClassDestructorCmd
#define Itcl_ClassDestructorCmd \
	(itclIntStubsPtr->itcl_ClassDestructorCmd) /* 80 */
#endif
#ifndef Itcl_ClassMethodCmd
#define Itcl_ClassMethodCmd \
	(itclIntStubsPtr->itcl_ClassMethodCmd) /* 81 */
#endif
#ifndef Itcl_ClassProcCmd
#define Itcl_ClassProcCmd \
	(itclIntStubsPtr->itcl_ClassProcCmd) /* 82 */
#endif
#ifndef Itcl_ClassVariableCmd
#define Itcl_ClassVariableCmd \
	(itclIntStubsPtr->itcl_ClassVariableCmd) /* 83 */
#endif
#ifndef Itcl_ClassCommonCmd
#define Itcl_ClassCommonCmd \
	(itclIntStubsPtr->itcl_ClassCommonCmd) /* 84 */
#endif
#ifndef Itcl_ParseVarResolver
#define Itcl_ParseVarResolver \
	(itclIntStubsPtr->itcl_ParseVarResolver) /* 85 */
#endif
#ifndef Itcl_BiInit
#define Itcl_BiInit \
	(itclIntStubsPtr->itcl_BiInit) /* 86 */
#endif
#ifndef Itcl_InstallBiMethods
#define Itcl_InstallBiMethods \
	(itclIntStubsPtr->itcl_InstallBiMethods) /* 87 */
#endif
#ifndef Itcl_BiIsaCmd
#define Itcl_BiIsaCmd \
	(itclIntStubsPtr->itcl_BiIsaCmd) /* 88 */
#endif
#ifndef Itcl_BiConfigureCmd
#define Itcl_BiConfigureCmd \
	(itclIntStubsPtr->itcl_BiConfigureCmd) /* 89 */
#endif
#ifndef Itcl_BiCgetCmd
#define Itcl_BiCgetCmd \
	(itclIntStubsPtr->itcl_BiCgetCmd) /* 90 */
#endif
#ifndef Itcl_BiChainCmd
#define Itcl_BiChainCmd \
	(itclIntStubsPtr->itcl_BiChainCmd) /* 91 */
#endif
#ifndef Itcl_BiInfoClassCmd
#define Itcl_BiInfoClassCmd \
	(itclIntStubsPtr->itcl_BiInfoClassCmd) /* 92 */
#endif
#ifndef Itcl_BiInfoInheritCmd
#define Itcl_BiInfoInheritCmd \
	(itclIntStubsPtr->itcl_BiInfoInheritCmd) /* 93 */
#endif
#ifndef Itcl_BiInfoHeritageCmd
#define Itcl_BiInfoHeritageCmd \
	(itclIntStubsPtr->itcl_BiInfoHeritageCmd) /* 94 */
#endif
#ifndef Itcl_BiInfoFunctionCmd
#define Itcl_BiInfoFunctionCmd \
	(itclIntStubsPtr->itcl_BiInfoFunctionCmd) /* 95 */
#endif
#ifndef Itcl_BiInfoVariableCmd
#define Itcl_BiInfoVariableCmd \
	(itclIntStubsPtr->itcl_BiInfoVariableCmd) /* 96 */
#endif
#ifndef Itcl_BiInfoBodyCmd
#define Itcl_BiInfoBodyCmd \
	(itclIntStubsPtr->itcl_BiInfoBodyCmd) /* 97 */
#endif
#ifndef Itcl_BiInfoArgsCmd
#define Itcl_BiInfoArgsCmd \
	(itclIntStubsPtr->itcl_BiInfoArgsCmd) /* 98 */
#endif
#ifndef Itcl_DefaultInfoCmd
#define Itcl_DefaultInfoCmd \
	(itclIntStubsPtr->itcl_DefaultInfoCmd) /* 99 */
#endif
#ifndef Itcl_EnsembleInit
#define Itcl_EnsembleInit \
	(itclIntStubsPtr->itcl_EnsembleInit) /* 100 */
#endif
#ifndef Itcl_CreateEnsemble
#define Itcl_CreateEnsemble \
	(itclIntStubsPtr->itcl_CreateEnsemble) /* 101 */
#endif
#ifndef Itcl_AddEnsemblePart
#define Itcl_AddEnsemblePart \
	(itclIntStubsPtr->itcl_AddEnsemblePart) /* 102 */
#endif
#ifndef Itcl_GetEnsemblePart
#define Itcl_GetEnsemblePart \
	(itclIntStubsPtr->itcl_GetEnsemblePart) /* 103 */
#endif
#ifndef Itcl_IsEnsemble
#define Itcl_IsEnsemble \
	(itclIntStubsPtr->itcl_IsEnsemble) /* 104 */
#endif
#ifndef Itcl_GetEnsembleUsage
#define Itcl_GetEnsembleUsage \
	(itclIntStubsPtr->itcl_GetEnsembleUsage) /* 105 */
#endif
#ifndef Itcl_GetEnsembleUsageForObj
#define Itcl_GetEnsembleUsageForObj \
	(itclIntStubsPtr->itcl_GetEnsembleUsageForObj) /* 106 */
#endif
#ifndef Itcl_EnsembleCmd
#define Itcl_EnsembleCmd \
	(itclIntStubsPtr->itcl_EnsembleCmd) /* 107 */
#endif
#ifndef Itcl_EnsPartCmd
#define Itcl_EnsPartCmd \
	(itclIntStubsPtr->itcl_EnsPartCmd) /* 108 */
#endif
#ifndef Itcl_EnsembleErrorCmd
#define Itcl_EnsembleErrorCmd \
	(itclIntStubsPtr->itcl_EnsembleErrorCmd) /* 109 */
#endif
/* Slot 110 is reserved */
/* Slot 111 is reserved */
/* Slot 112 is reserved */
/* Slot 113 is reserved */
/* Slot 114 is reserved */
#ifndef Itcl_Assert
#define Itcl_Assert \
	(itclIntStubsPtr->itcl_Assert) /* 115 */
#endif
#ifndef Itcl_IsObjectCmd
#define Itcl_IsObjectCmd \
	(itclIntStubsPtr->itcl_IsObjectCmd) /* 116 */
#endif
#ifndef Itcl_IsClassCmd
#define Itcl_IsClassCmd \
	(itclIntStubsPtr->itcl_IsClassCmd) /* 117 */
#endif
/* Slot 118 is reserved */
/* Slot 119 is reserved */
/* Slot 120 is reserved */
/* Slot 121 is reserved */
/* Slot 122 is reserved */
/* Slot 123 is reserved */
/* Slot 124 is reserved */
/* Slot 125 is reserved */
/* Slot 126 is reserved */
/* Slot 127 is reserved */
/* Slot 128 is reserved */
/* Slot 129 is reserved */
/* Slot 130 is reserved */
/* Slot 131 is reserved */
/* Slot 132 is reserved */
/* Slot 133 is reserved */
/* Slot 134 is reserved */
/* Slot 135 is reserved */
/* Slot 136 is reserved */
/* Slot 137 is reserved */
/* Slot 138 is reserved */
/* Slot 139 is reserved */
#ifndef Itcl_FilterAddCmd
#define Itcl_FilterAddCmd \
	(itclIntStubsPtr->itcl_FilterAddCmd) /* 140 */
#endif
#ifndef Itcl_FilterDeleteCmd
#define Itcl_FilterDeleteCmd \
	(itclIntStubsPtr->itcl_FilterDeleteCmd) /* 141 */
#endif
#ifndef Itcl_ForwardAddCmd
#define Itcl_ForwardAddCmd \
	(itclIntStubsPtr->itcl_ForwardAddCmd) /* 142 */
#endif
#ifndef Itcl_ForwardDeleteCmd
#define Itcl_ForwardDeleteCmd \
	(itclIntStubsPtr->itcl_ForwardDeleteCmd) /* 143 */
#endif
#ifndef Itcl_MixinAddCmd
#define Itcl_MixinAddCmd \
	(itclIntStubsPtr->itcl_MixinAddCmd) /* 144 */
#endif
#ifndef Itcl_MixinDeleteCmd
#define Itcl_MixinDeleteCmd \
	(itclIntStubsPtr->itcl_MixinDeleteCmd) /* 145 */
#endif
/* Slot 146 is reserved */
/* Slot 147 is reserved */
/* Slot 148 is reserved */
/* Slot 149 is reserved */
#ifndef Itcl_BiInfoCmd
#define Itcl_BiInfoCmd \
	(itclIntStubsPtr->itcl_BiInfoCmd) /* 150 */
#endif
#ifndef Itcl_BiInfoUnknownCmd
#define Itcl_BiInfoUnknownCmd \
	(itclIntStubsPtr->itcl_BiInfoUnknownCmd) /* 151 */
#endif
#ifndef Itcl_BiInfoVarsCmd
#define Itcl_BiInfoVarsCmd \
	(itclIntStubsPtr->itcl_BiInfoVarsCmd) /* 152 */
#endif
#ifndef Itcl_CanAccess2
#define Itcl_CanAccess2 \
	(itclIntStubsPtr->itcl_CanAccess2) /* 153 */
#endif
/* Slot 154 is reserved */
/* Slot 155 is reserved */
/* Slot 156 is reserved */
/* Slot 157 is reserved */
/* Slot 158 is reserved */
/* Slot 159 is reserved */
#ifndef Itcl_SetCallFrameResolver
#define Itcl_SetCallFrameResolver \
	(itclIntStubsPtr->itcl_SetCallFrameResolver) /* 160 */
#endif
#ifndef ItclEnsembleSubCmd
#define ItclEnsembleSubCmd \
	(itclIntStubsPtr->itclEnsembleSubCmd) /* 161 */
#endif
#ifndef Itcl_GetUplevelNamespace
#define Itcl_GetUplevelNamespace \
	(itclIntStubsPtr->itcl_GetUplevelNamespace) /* 162 */
#endif
#ifndef Itcl_GetCallFrameClientData
#define Itcl_GetCallFrameClientData \
	(itclIntStubsPtr->itcl_GetCallFrameClientData) /* 163 */
#endif
/* Slot 164 is reserved */
#ifndef Itcl_SetCallFrameNamespace
#define Itcl_SetCallFrameNamespace \
	(itclIntStubsPtr->itcl_SetCallFrameNamespace) /* 165 */
#endif
#ifndef Itcl_GetCallFrameObjc
#define Itcl_GetCallFrameObjc \
	(itclIntStubsPtr->itcl_GetCallFrameObjc) /* 166 */
#endif
#ifndef Itcl_GetCallFrameObjv
#define Itcl_GetCallFrameObjv \
	(itclIntStubsPtr->itcl_GetCallFrameObjv) /* 167 */
#endif
#ifndef Itcl_NWidgetCmd
#define Itcl_NWidgetCmd \
	(itclIntStubsPtr->itcl_NWidgetCmd) /* 168 */
#endif
#ifndef Itcl_AddOptionCmd
#define Itcl_AddOptionCmd \
	(itclIntStubsPtr->itcl_AddOptionCmd) /* 169 */
#endif
#ifndef Itcl_AddComponentCmd
#define Itcl_AddComponentCmd \
	(itclIntStubsPtr->itcl_AddComponentCmd) /* 170 */
#endif
#ifndef Itcl_BiInfoOptionCmd
#define Itcl_BiInfoOptionCmd \
	(itclIntStubsPtr->itcl_BiInfoOptionCmd) /* 171 */
#endif
#ifndef Itcl_BiInfoComponentCmd
#define Itcl_BiInfoComponentCmd \
	(itclIntStubsPtr->itcl_BiInfoComponentCmd) /* 172 */
#endif
#ifndef Itcl_RenameCommand
#define Itcl_RenameCommand \
	(itclIntStubsPtr->itcl_RenameCommand) /* 173 */
#endif
#ifndef Itcl_PushCallFrame
#define Itcl_PushCallFrame \
	(itclIntStubsPtr->itcl_PushCallFrame) /* 174 */
#endif
#ifndef Itcl_PopCallFrame
#define Itcl_PopCallFrame \
	(itclIntStubsPtr->itcl_PopCallFrame) /* 175 */
#endif
#ifndef Itcl_GetUplevelCallFrame
#define Itcl_GetUplevelCallFrame \
	(itclIntStubsPtr->itcl_GetUplevelCallFrame) /* 176 */
#endif
#ifndef Itcl_ActivateCallFrame
#define Itcl_ActivateCallFrame \
	(itclIntStubsPtr->itcl_ActivateCallFrame) /* 177 */
#endif

#endif /* defined(USE_ITCL_STUBS) */

/* !END!: Do not edit above this line. */

struct ItclStubAPI {
    ItclStubs *stubsPtr;
    ItclIntStubs *intStubsPtr;
};
