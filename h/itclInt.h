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

#include "tclInt.h"
#include "itcl.h"

#ifdef BUILD_itcl
# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLEXPORT
#endif

/*
 * Fix Tcl bug #803489 the right way.  We need to always use the old Stub
 * slot positions, not the new broken ones part of TIP 127.  I do like
 * that these functions have moved to the public space (about time), but
 * the slot change is the killer and is the painful side affect.
 */

#if defined(USE_TCL_STUBS) && \
	(TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION >= 5)
#   undef Tcl_CreateNamespace
#   define Tcl_CreateNamespace \
	(tclIntStubsPtr->tcl_CreateNamespace)
#   undef Tcl_DeleteNamespace
#   define Tcl_DeleteNamespace \
	(tclIntStubsPtr->tcl_DeleteNamespace)
#   undef Tcl_AppendExportList
#   define Tcl_AppendExportList \
	(tclIntStubsPtr->tcl_AppendExportList)
#   undef Tcl_Export
#   define Tcl_Export \
	(tclIntStubsPtr->tcl_Export)
#   undef Tcl_Import
#   define Tcl_Import \
	(tclIntStubsPtr->tcl_Import)
#   undef Tcl_ForgetImport
#   define Tcl_ForgetImport \
	(tclIntStubsPtr->tcl_ForgetImport)
#   undef Tcl_GetCurrentNamespace
#   define Tcl_GetCurrentNamespace \
	(tclIntStubsPtr->tcl_GetCurrentNamespace)
#   undef Tcl_GetGlobalNamespace
#   define Tcl_GetGlobalNamespace \
	(tclIntStubsPtr->tcl_GetGlobalNamespace)
#   undef Tcl_FindNamespace
#   define Tcl_FindNamespace \
	(tclIntStubsPtr->tcl_FindNamespace)
#   undef Tcl_FindCommand
#   define Tcl_FindCommand \
	(tclIntStubsPtr->tcl_FindCommand)
#   undef Tcl_GetCommandFromObj
#   define Tcl_GetCommandFromObj \
	(tclIntStubsPtr->tcl_GetCommandFromObj)
#   undef Tcl_GetCommandFullName
#   define Tcl_GetCommandFullName \
	(tclIntStubsPtr->tcl_GetCommandFullName)
#endif

/*
 * Some backward compatability adjustments.
 */

#if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 0
#   define Tcl_GetString(obj)	Tcl_GetStringFromObj((obj), NULL)
#   define TCL_DECLARE_MUTEX(mutexVar)
#   define Tcl_MutexLock(mutexVar)
#   define Tcl_MutexUnlock(mutexVar)
#   define Tcl_Panic panic
#endif

#define TCL_DOES_STUBS \
    (TCL_MAJOR_VERSION > 8 || TCL_MAJOR_VERSION == 8 && (TCL_MINOR_VERSION > 1 || \
    (TCL_MINOR_VERSION == 1 && TCL_RELEASE_LEVEL == TCL_FINAL_RELEASE)))


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
 *  Compatibility flags.  Used to support small "hacks".  These are stored
 *  in the global variable named itclCompatFlags.
 */
#define ITCL_COMPAT_USECMDFLAGS 0x0001	/* Tcl8.4a1 introduced a different Command
					 * structure, and we need to adapt
					 * dynamically */

#include "itclIntDecls.h"

/*
 * Since the Tcl/Tk distribution doesn't perform any asserts,
 * dynamic loading can fail to find the __assert function.
 * As a workaround, we'll include our own.
 */

#undef  assert
#ifndef  DEBUG
#define assert(EX) ((void)0)
#else
#define assert(EX) (void)((EX) || (Itcl_Assert(STRINGIFY(EX), __FILE__, __LINE__), 0))
#endif  /* DEBUG */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* ITCLINT_H */
