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
 *  ADDING [incr Tcl] TO A Tcl-BASED APPLICATION:
 *
 *    To add [incr Tcl] facilities to a Tcl application, modify the
 *    Tcl_AppInit() routine as follows:
 *
 *    1) Include this header file near the top of the file containing
 *       Tcl_AppInit():
 *
 *         #include "itcl.h"
 *
 *    2) Within the body of Tcl_AppInit(), add the following lines:
 *
 *         if (Itcl_Init(interp) == TCL_ERROR) {
 *             return TCL_ERROR;
 *         }
 * 
 *    3) Link your application with libitcl.a
 *
 *    NOTE:  An example file "tclAppInit.c" containing the changes shown
 *           above is included in this distribution.
 *  
 * ========================================================================
 *  AUTHOR:  Michael J. McLennan
 *           Bell Labs Innovations for Lucent Technologies
 *           mmclennan@lucent.com
 *           http://www.tcltk.com/itcl
 *
 *     RCS:  $Id$
 * ========================================================================
 *           Copyright (c) 1993-1998  Lucent Technologies, Inc.
 * ------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#ifndef ITCLINT_H
#define ITCLINT_H

#include "itcl.h"
#include "tclInt.h"

#ifdef BUILD_itcl
# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLEXPORT
#endif

/*
 * Since the Tcl/Tk distribution doesn't perform any asserts,
 * dynamic loading can fail to find the __assert function.
 * As a workaround, we'll include our own.
 */
#undef  assert
#ifdef  NDEBUG
#define assert(EX) ((void)0)
#else
EXTERN void Itcl_Assert _ANSI_ARGS_((char *testExpr, char *fileName, int lineNum)
);
#if defined(__STDC__)
#define assert(EX) (void)((EX) || (Itcl_Assert(#EX, __FILE__, __LINE__), 0))
#else
#define assert(EX) (void)((EX) || (Itcl_Assert("EX", __FILE__, __LINE__), 0))
#endif  /* __STDC__ */
#endif  /* NDEBUG */


/*
 *  Common info for managing all known objects.
 *  Each interpreter has one of these data structures stored as
 *  clientData in the "itcl" namespace.  It is also accessible
 *  as associated data via the key ITCL_INTERP_DATA.
 */
struct ItclObject;
typedef struct ItclObjectInfo {
    Tcl_Interp *interp;             /* interpreter that manages this info */
    Tcl_HashTable objects;          /* list of all known objects */

    Itcl_Stack transparentFrames;   /* stack of call frames that should be
                                     * treated transparently.  When
                                     * Itcl_EvalMemberCode is invoked in
                                     * one of these contexts, it does an
                                     * "uplevel" to get past the transparent
                                     * frame and back to the calling context. */
    Tcl_HashTable contextFrames;    /* object contexts for active call frames */

    int protection;                 /* protection level currently in effect */

    Itcl_Stack cdefnStack;          /* stack of class definitions currently
                                     * being parsed */
} ItclObjectInfo;

#define ITCL_INTERP_DATA "itcl_data"

/*
 *  Representation for each [incr Tcl] class.
 */
typedef struct ItclClass {
    char *name;                   /* class name */
    char *fullname;               /* fully qualified class name */
    Tcl_Interp *interp;           /* interpreter that manages this info */
    Tcl_Namespace *namesp;        /* namespace representing class scope */
    Tcl_Command accessCmd;        /* access command for creating instances */

    struct ItclObjectInfo *info;  /* info about all known objects */
    Itcl_List bases;              /* list of base classes */
    Itcl_List derived;            /* list of all derived classes */
    Tcl_HashTable heritage;       /* table of all base classes.  Look up
                                   * by pointer to class definition.  This
                                   * provides fast lookup for inheritance
                                   * tests. */
    Tcl_Obj *initCode;            /* initialization code for new objs */
    Tcl_HashTable variables;      /* definitions for all data members
                                     in this class.  Look up simple string
                                     names and get back ItclVarDefn* ptrs */
    Tcl_HashTable functions;      /* definitions for all member functions
                                     in this class.  Look up simple string
                                     names and get back ItclMemberFunc* ptrs */
    int numInstanceVars;          /* number of instance vars in variables
                                     table */
    Tcl_HashTable resolveVars;    /* all possible names for variables in
                                   * this class (e.g., x, foo::x, etc.) */
    Tcl_HashTable resolveCmds;    /* all possible names for functions in
                                   * this class (e.g., x, foo::x, etc.) */
    int unique;                   /* unique number for #auto generation */
    int flags;                    /* maintains class status */
} ItclClass;

typedef struct ItclHierIter {
    ItclClass *current;           /* current position in hierarchy */
    Itcl_Stack stack;             /* stack used for traversal */
} ItclHierIter;

/*
 *  Representation for each [incr Tcl] object.
 */
typedef struct ItclObject {
    ItclClass *classDefn;        /* most-specific class */
    Tcl_Command accessCmd;       /* object access command */

    int dataSize;                /* number of elements in data array */
    Var** data;                  /* all object-specific data members */
    Tcl_HashTable* constructed;  /* temp storage used during construction */
    Tcl_HashTable* destructed;   /* temp storage used during destruction */
} ItclObject;

#define ITCL_IGNORE_ERRS  0x002  /* useful for construction/destruction */

/*
 *  Implementation for any code body in an [incr Tcl] class.
 */
typedef struct ItclMemberCode {
    int flags;                  /* flags describing implementation */
    CompiledLocal *arglist;     /* list of arg names and initial values */
    int argcount;               /* number of args in arglist */
    Proc *procPtr;              /* Tcl proc representation (needed to
                                 * handle compiled locals) */
    union {
        Tcl_CmdProc *argCmd;    /* (argc,argv) C implementation */
        Tcl_ObjCmdProc *objCmd; /* (objc,objv) C implementation */
    } cfunc;

    ClientData clientData;      /* client data for C implementations */

} ItclMemberCode;

/*
 *  Basic representation for class members (commands/variables)
 */
typedef struct ItclMember {
    Tcl_Interp* interp;         /* interpreter containing the class */
    ItclClass* classDefn;       /* class containing this member */
    char* name;                 /* member name */
    char* fullname;             /* member name with "class::" qualifier */
    int protection;             /* protection level */
    int flags;                  /* flags describing member (see below) */
    ItclMemberCode *code;       /* code associated with member */
} ItclMember;

/*
 *  Flag bits for ItclMemberCode and ItclMember:
 */
#define ITCL_IMPLEMENT_NONE    0x001  /* no implementation */
#define ITCL_IMPLEMENT_TCL     0x002  /* Tcl implementation */
#define ITCL_IMPLEMENT_ARGCMD  0x004  /* (argc,argv) C implementation */
#define ITCL_IMPLEMENT_OBJCMD  0x008  /* (objc,objv) C implementation */
#define ITCL_IMPLEMENT_C       0x00c  /* either kind of C implementation */
#define ITCL_CONSTRUCTOR       0x010  /* non-zero => is a constructor */
#define ITCL_DESTRUCTOR        0x020  /* non-zero => is a destructor */
#define ITCL_COMMON            0x040  /* non-zero => is a "proc" */
#define ITCL_ARG_SPEC          0x080  /* non-zero => has an argument spec */

#define ITCL_OLD_STYLE         0x100  /* non-zero => old-style method
                                       * (process "config" argument) */

#define ITCL_THIS_VAR          0x200  /* non-zero => built-in "this" variable */

/*
 *  Representation of member functions in an [incr Tcl] class.
 */
typedef struct ItclMemberFunc {
    ItclMember *member;          /* basic member info */
    Tcl_Command accessCmd;       /* Tcl command installed for this function */
    CompiledLocal *arglist;      /* list of arg names and initial values */
    int argcount;                /* number of args in arglist */
} ItclMemberFunc;

/*
 *  Instance variables.
 */
typedef struct ItclVarDefn {
    ItclMember *member;          /* basic member info */
    char* init;                  /* initial value */
} ItclVarDefn;

/*
 *  Instance variable lookup entry.
 */
typedef struct ItclVarLookup {
    ItclVarDefn* vdefn;       /* variable definition */
    int usage;                /* number of uses for this record */
    int accessible;           /* non-zero => accessible from class with
                               * this lookup record in its resolveVars */
    char *leastQualName;      /* simplist name for this variable, with
                               * the fewest qualifiers.  This string is
                               * taken from the resolveVars table, so
                               * it shouldn't be freed. */
    union {
        int index;            /* index into virtual table (instance data) */
        Tcl_Var common;       /* variable (common data) */
    } var;
} ItclVarLookup;

/*
 *  Representation for the context in which a body of [incr Tcl]
 *  code executes.  In ordinary Tcl, this is a CallFrame.  But for
 *  [incr Tcl] code bodies, we must be careful to set up the
 *  CallFrame properly, to plug in instance variables before
 *  executing the code body.
 */
typedef struct ItclContext {
    ItclClass *classDefn;     /* class definition */
    CallFrame frame;          /* call frame for object context */
    Var *compiledLocals;      /* points to storage for compiled locals */
    Var localStorage[20];     /* default storage for compiled locals */
} ItclContext;


/*
 *  Functions used within the package, but not considered "public"
 */

EXTERN int Itcl_IsClassNamespace _ANSI_ARGS_((Tcl_Namespace *namesp));
EXTERN int Itcl_IsClass _ANSI_ARGS_((Tcl_Command cmd));
EXTERN ItclClass* Itcl_FindClass _ANSI_ARGS_((Tcl_Interp* interp,
    char* path, int autoload));

EXTERN int Itcl_FindObject _ANSI_ARGS_((Tcl_Interp *interp,
    char *name, ItclObject **roPtr));
EXTERN int Itcl_IsObject _ANSI_ARGS_((Tcl_Command cmd));
EXTERN int Itcl_ObjectIsa _ANSI_ARGS_((ItclObject *contextObj,
    ItclClass *cdefn));


EXTERN int Itcl_Protection _ANSI_ARGS_((Tcl_Interp *interp,
    int newLevel));
EXTERN char* Itcl_ProtectionStr _ANSI_ARGS_((int pLevel));
EXTERN int Itcl_CanAccess _ANSI_ARGS_((ItclMember* memberPtr,
    Tcl_Namespace* fromNsPtr));
EXTERN int Itcl_CanAccessFunc _ANSI_ARGS_((ItclMemberFunc* mfunc,
    Tcl_Namespace* fromNsPtr));
EXTERN Tcl_Namespace* Itcl_GetTrueNamespace _ANSI_ARGS_((Tcl_Interp *interp,
    ItclObjectInfo *info));

EXTERN void Itcl_ParseNamespPath _ANSI_ARGS_((char *name,
    Tcl_DString *buffer, char **head, char **tail));
EXTERN int Itcl_DecodeScopedCommand _ANSI_ARGS_((Tcl_Interp *interp,
    char *name, Tcl_Namespace **rNsPtr, char **rCmdPtr));
EXTERN int Itcl_EvalArgs _ANSI_ARGS_((Tcl_Interp *interp, int objc,
    Tcl_Obj *CONST objv[]));
EXTERN Tcl_Obj* Itcl_CreateArgs _ANSI_ARGS_((Tcl_Interp *interp,
    char *string, int objc, Tcl_Obj *CONST objv[]));

EXTERN int Itcl_PushContext _ANSI_ARGS_((Tcl_Interp *interp,
    ItclMember *member, ItclClass *contextClass, ItclObject *contextObj,
    ItclContext *contextPtr));
EXTERN void Itcl_PopContext _ANSI_ARGS_((Tcl_Interp *interp,
    ItclContext *contextPtr));
EXTERN int Itcl_GetContext _ANSI_ARGS_((Tcl_Interp *interp,
    ItclClass **cdefnPtr, ItclObject **odefnPtr));

EXTERN void Itcl_InitHierIter _ANSI_ARGS_((ItclHierIter *iter,
    ItclClass *cdefn));
EXTERN void Itcl_DeleteHierIter _ANSI_ARGS_((ItclHierIter *iter));
EXTERN ItclClass* Itcl_AdvanceHierIter _ANSI_ARGS_((ItclHierIter *iter));

EXTERN int Itcl_FindClassesCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_FindObjectsCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ProtectionCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_DelClassCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_DelObjectCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ScopeCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_CodeCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_StubCreateCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_StubExistsCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_IsStub _ANSI_ARGS_((Tcl_Command cmd));


/*
 *  Functions for manipulating classes
 */
EXTERN int Itcl_CreateClass _ANSI_ARGS_((Tcl_Interp* interp, char* path,
    ItclObjectInfo *info, ItclClass **rPtr));
EXTERN int Itcl_DeleteClass _ANSI_ARGS_((Tcl_Interp *interp,
    ItclClass *cdefnPtr));
EXTERN Tcl_Namespace* Itcl_FindClassNamespace _ANSI_ARGS_((Tcl_Interp* interp,
    char* path));
EXTERN int Itcl_HandleClass _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ClassCmdResolver _ANSI_ARGS_((Tcl_Interp *interp,
    char* name, Tcl_Namespace *context, int flags, Tcl_Command *rPtr));
EXTERN int Itcl_ClassVarResolver _ANSI_ARGS_((Tcl_Interp *interp,
    char* name, Tcl_Namespace *context, int flags, Tcl_Var *rPtr));
EXTERN int Itcl_ClassCompiledVarResolver _ANSI_ARGS_((Tcl_Interp *interp,
    char* name, int length, Tcl_Namespace *context, Tcl_ResolvedVarInfo **rPtr));
EXTERN void Itcl_BuildVirtualTables _ANSI_ARGS_((ItclClass* cdefnPtr));
EXTERN int Itcl_CreateVarDefn _ANSI_ARGS_((Tcl_Interp *interp,
    ItclClass* cdefn, char* name, char* init, char* config,
    ItclVarDefn** vdefnPtr));
EXTERN void Itcl_DeleteVarDefn _ANSI_ARGS_((ItclVarDefn *vdefn));
EXTERN char* Itcl_GetCommonVar _ANSI_ARGS_((Tcl_Interp *interp,
    char *name, ItclClass *contextClass));
EXTERN ItclMember* Itcl_CreateMember _ANSI_ARGS_((Tcl_Interp* interp,
    ItclClass *cdefn, char* name));
EXTERN void Itcl_DeleteMember _ANSI_ARGS_((ItclMember *memPtr));


/*
 *  Functions for manipulating objects
 */
EXTERN int Itcl_CreateObject _ANSI_ARGS_((Tcl_Interp *interp,
    char* name, ItclClass *cdefn, int objc, Tcl_Obj *CONST objv[],
    ItclObject **roPtr));
EXTERN int Itcl_DeleteObject _ANSI_ARGS_((Tcl_Interp *interp,
    ItclObject *contextObj));
EXTERN int Itcl_DestructObject _ANSI_ARGS_((Tcl_Interp *interp,
    ItclObject *contextObj, int flags));
EXTERN int Itcl_HandleInstance _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN char* Itcl_GetInstanceVar _ANSI_ARGS_((Tcl_Interp *interp,
    char *name, ItclObject *contextObj, ItclClass *contextClass));
EXTERN int Itcl_ScopedVarResolver _ANSI_ARGS_((Tcl_Interp *interp,
    char *name, Tcl_Namespace *contextNs, int flags, Tcl_Var *rPtr));


/*
 *  Functions for manipulating methods and procs
 */
EXTERN int Itcl_BodyCmd _ANSI_ARGS_((ClientData dummy,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ConfigBodyCmd _ANSI_ARGS_((ClientData dummy,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_CreateMethod _ANSI_ARGS_((Tcl_Interp* interp,
    ItclClass *cdefn, char* name, char* arglist, char* body));
EXTERN int Itcl_CreateProc _ANSI_ARGS_((Tcl_Interp* interp,
    ItclClass *cdefn, char* name, char* arglist, char* body));
EXTERN int Itcl_CreateMemberFunc _ANSI_ARGS_((Tcl_Interp* interp,
    ItclClass *cdefn, char* name, char* arglist, char* body,
    ItclMemberFunc** mfuncPtr));
EXTERN int Itcl_ChangeMemberFunc _ANSI_ARGS_((Tcl_Interp* interp,
    ItclMemberFunc* mfunc, char* arglist, char* body));
EXTERN void Itcl_DeleteMemberFunc _ANSI_ARGS_((char* cdata));
EXTERN int Itcl_CreateMemberCode _ANSI_ARGS_((Tcl_Interp* interp,
    ItclClass *cdefn, char* arglist, char* body, ItclMemberCode** mcodePtr));
EXTERN void Itcl_DeleteMemberCode _ANSI_ARGS_((char* cdata));
EXTERN int Itcl_GetMemberCode _ANSI_ARGS_((Tcl_Interp* interp,
    ItclMember* member));
EXTERN int Itcl_CompileMemberCodeBody _ANSI_ARGS_((Tcl_Interp *interp,
    ItclMember *member, char *desc, Tcl_Obj *bodyPtr));
EXTERN int Itcl_EvalMemberCode _ANSI_ARGS_((Tcl_Interp *interp,
    ItclMemberFunc *mfunc, ItclMember *member, ItclObject *contextObj,
    int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_CreateArgList _ANSI_ARGS_((Tcl_Interp* interp,
    char* decl, int* argcPtr, CompiledLocal** argPtr));
EXTERN CompiledLocal* Itcl_CreateArg _ANSI_ARGS_((char* name,
    char* init));
EXTERN void Itcl_DeleteArgList _ANSI_ARGS_((CompiledLocal *arglist));
EXTERN Tcl_Obj* Itcl_ArgList _ANSI_ARGS_((int argc, CompiledLocal* arglist));
EXTERN int Itcl_EquivArgLists _ANSI_ARGS_((CompiledLocal* arg1, int arg1c,
    CompiledLocal* arg2, int arg2c));
EXTERN void Itcl_GetMemberFuncUsage _ANSI_ARGS_((ItclMemberFunc *mfunc,
    ItclObject *contextObj, Tcl_Obj *objPtr));
EXTERN int Itcl_ExecMethod _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ExecProc _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_AssignArgs _ANSI_ARGS_((Tcl_Interp *interp,
    int objc, Tcl_Obj *CONST objv[], ItclMemberFunc *mfunc));
EXTERN int Itcl_ConstructBase _ANSI_ARGS_((Tcl_Interp *interp,
    ItclObject *contextObj, ItclClass *contextClass));
EXTERN int Itcl_InvokeMethodIfExists _ANSI_ARGS_((Tcl_Interp *interp,
    char *name, ItclClass *contextClass, ItclObject *contextObj,
    int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_EvalBody _ANSI_ARGS_((Tcl_Interp *interp,
    Tcl_Obj *bodyPtr));
EXTERN int Itcl_ReportFuncErrors _ANSI_ARGS_((Tcl_Interp* interp,
    ItclMemberFunc *mfunc, ItclObject *contextObj, int result));


/*
 *  Commands for parsing class definitions
 */
EXTERN int Itcl_ParseInit _ANSI_ARGS_((Tcl_Interp *interp,
    ItclObjectInfo *info));
EXTERN int Itcl_ClassCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ClassInheritCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ClassProtectionCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ClassConstructorCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ClassDestructorCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ClassMethodCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ClassProcCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ClassVariableCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ClassCommonCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_ParseVarResolver _ANSI_ARGS_((Tcl_Interp *interp,
    char* name, Tcl_Namespace *contextNs, int flags, Tcl_Var* rPtr));


/*
 *  Commands in the "builtin" namespace
 */
EXTERN int Itcl_BiInit _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int Itcl_InstallBiMethods _ANSI_ARGS_((Tcl_Interp *interp,
    ItclClass *cdefn));
EXTERN int Itcl_BiIsaCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_BiConfigureCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_BiCgetCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_BiChainCmd _ANSI_ARGS_((ClientData dummy,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_BiInfoClassCmd _ANSI_ARGS_((ClientData dummy,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_BiInfoInheritCmd _ANSI_ARGS_((ClientData dummy,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_BiInfoHeritageCmd _ANSI_ARGS_((ClientData dummy,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_BiInfoFunctionCmd _ANSI_ARGS_((ClientData dummy,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_BiInfoVariableCmd _ANSI_ARGS_((ClientData dummy,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_BiInfoBodyCmd _ANSI_ARGS_((ClientData dummy,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_BiInfoArgsCmd _ANSI_ARGS_((ClientData dummy,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_DefaultInfoCmd _ANSI_ARGS_((ClientData dummy,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));


/*
 *  Ensembles
 */
EXTERN int Itcl_EnsembleInit _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int Itcl_CreateEnsemble _ANSI_ARGS_((Tcl_Interp *interp,
    char* ensName));
EXTERN int Itcl_AddEnsemblePart _ANSI_ARGS_((Tcl_Interp *interp,
    char* ensName, char* partName, char* usageInfo,
    Tcl_ObjCmdProc *objProc, ClientData clientData,
    Tcl_CmdDeleteProc *deleteProc));
EXTERN int Itcl_GetEnsemblePart _ANSI_ARGS_((Tcl_Interp *interp,
    char *ensName, char *partName, Tcl_CmdInfo *infoPtr));
EXTERN int Itcl_IsEnsemble _ANSI_ARGS_((Tcl_CmdInfo* infoPtr));
EXTERN int Itcl_GetEnsembleUsage _ANSI_ARGS_((Tcl_Interp *interp,
    char *ensName, Tcl_Obj *objPtr));
EXTERN int Itcl_GetEnsembleUsageForObj _ANSI_ARGS_((Tcl_Interp *interp,
    Tcl_Obj *ensObjPtr, Tcl_Obj *objPtr));
EXTERN int Itcl_EnsembleCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_EnsPartCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int Itcl_EnsembleErrorCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));


/*
 *  Commands provided for backward compatibility
 */
EXTERN int Itcl_OldInit _ANSI_ARGS_((Tcl_Interp* interp,
    ItclObjectInfo* info));
EXTERN int Itcl_InstallOldBiMethods _ANSI_ARGS_((Tcl_Interp *interp,
    ItclClass *cdefn));


/*
 *  Things that should be in the Tcl core.
 */
EXTERN Tcl_CallFrame* _Tcl_GetCallFrame _ANSI_ARGS_((Tcl_Interp *interp,
    int level));
EXTERN Tcl_CallFrame* _Tcl_ActivateCallFrame _ANSI_ARGS_((Tcl_Interp *interp,
    Tcl_CallFrame *framePtr));
EXTERN Var* _TclNewVar _ANSI_ARGS_((void));

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* ITCLINT_H */
