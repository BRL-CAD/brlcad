/*
 * tclInt.h --
 *
 *	Declarations of things used internally by the Tcl interpreter.
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1993-1997 Lucent Technologies.
 * Copyright (c) 1994-1998 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * Copyright (c) 2001, 2002 by Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifndef _TCLINT
#define _TCLINT

/*
 * Common include files needed by most of the Tcl source files are
 * included here, so that system-dependent personalizations for the
 * include files only have to be made in once place.  This results
 * in a few extra includes, but greater modularity.  The order of
 * the three groups of #includes is important.	For example, stdio.h
 * is needed by tcl.h, and the _ANSI_ARGS_ declaration in tcl.h is
 * needed by stdlib.h in some configurations.
 */

#ifndef _TCL
#include "tcl.h"
#endif

#include <stdio.h>

#include <ctype.h>
#ifdef NO_LIMITS_H
#   include "compat/limits.h"
#else
#   include <limits.h>
#endif
#ifdef NO_STDLIB_H
#   include "compat/stdlib.h"
#else
#   include <stdlib.h>
#endif
#ifdef NO_STRING_H
#include "compat/string.h"
#else
#include <string.h>
#endif

#undef TCL_STORAGE_CLASS
#ifdef BUILD_tcl
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# ifdef USE_TCL_STUBS
#  define TCL_STORAGE_CLASS
# else
#  define TCL_STORAGE_CLASS DLLIMPORT
# endif
#endif

/*
 * The following procedures allow namespaces to be customized to
 * support special name resolution rules for commands/variables.
 * 
 */

struct Tcl_ResolvedVarInfo;

typedef Tcl_Var (Tcl_ResolveRuntimeVarProc) _ANSI_ARGS_((
    Tcl_Interp* interp, struct Tcl_ResolvedVarInfo *vinfoPtr));

typedef void (Tcl_ResolveVarDeleteProc) _ANSI_ARGS_((
    struct Tcl_ResolvedVarInfo *vinfoPtr));

/*
 * The following structure encapsulates the routines needed to resolve a
 * variable reference at runtime.  Any variable specific state will typically
 * be appended to this structure.
 */


typedef struct Tcl_ResolvedVarInfo {
    Tcl_ResolveRuntimeVarProc *fetchProc;
    Tcl_ResolveVarDeleteProc *deleteProc;
} Tcl_ResolvedVarInfo;



typedef int (Tcl_ResolveCompiledVarProc) _ANSI_ARGS_((
    Tcl_Interp* interp, CONST84 char* name, int length,
    Tcl_Namespace *context, Tcl_ResolvedVarInfo **rPtr));

typedef int (Tcl_ResolveVarProc) _ANSI_ARGS_((
    Tcl_Interp* interp, CONST84 char* name, Tcl_Namespace *context,
    int flags, Tcl_Var *rPtr));

typedef int (Tcl_ResolveCmdProc) _ANSI_ARGS_((Tcl_Interp* interp,
    CONST84 char* name, Tcl_Namespace *context, int flags,
    Tcl_Command *rPtr));
 
typedef struct Tcl_ResolverInfo {
    Tcl_ResolveCmdProc *cmdResProc;	/* Procedure handling command name
					 * resolution. */
    Tcl_ResolveVarProc *varResProc;	/* Procedure handling variable name
					 * resolution for variables that
					 * can only be handled at runtime. */
    Tcl_ResolveCompiledVarProc *compiledVarResProc;
					/* Procedure handling variable name
					 * resolution at compile time. */
} Tcl_ResolverInfo;

/*
 *----------------------------------------------------------------
 * Data structures related to namespaces.
 *----------------------------------------------------------------
 */

/*
 * The structure below defines a namespace.
 * Note: the first five fields must match exactly the fields in a
 * Tcl_Namespace structure (see tcl.h). If you change one, be sure to
 * change the other.
 */

typedef struct Namespace {
    char *name;			 /* The namespace's simple (unqualified)
				  * name. This contains no ::'s. The name of
				  * the global namespace is "" although "::"
				  * is an synonym. */
    char *fullName;		 /* The namespace's fully qualified name.
				  * This starts with ::. */
    ClientData clientData;	 /* An arbitrary value associated with this
				  * namespace. */
    Tcl_NamespaceDeleteProc *deleteProc;
				 /* Procedure invoked when deleting the
				  * namespace to, e.g., free clientData. */
    struct Namespace *parentPtr; /* Points to the namespace that contains
				  * this one. NULL if this is the global
				  * namespace. */
    Tcl_HashTable childTable;	 /* Contains any child namespaces. Indexed
				  * by strings; values have type
				  * (Namespace *). */
    long nsId;			 /* Unique id for the namespace. */
    Tcl_Interp *interp;		 /* The interpreter containing this
				  * namespace. */
    int flags;			 /* OR-ed combination of the namespace
				  * status flags NS_DYING and NS_DEAD
				  * listed below. */
    int activationCount;	 /* Number of "activations" or active call
				  * frames for this namespace that are on
				  * the Tcl call stack. The namespace won't
				  * be freed until activationCount becomes
				  * zero. */
    int refCount;		 /* Count of references by namespaceName *
				  * objects. The namespace can't be freed
				  * until refCount becomes zero. */
    Tcl_HashTable cmdTable;	 /* Contains all the commands currently
				  * registered in the namespace. Indexed by
				  * strings; values have type (Command *).
				  * Commands imported by Tcl_Import have
				  * Command structures that point (via an
				  * ImportedCmdRef structure) to the
				  * Command structure in the source
				  * namespace's command table. */
    Tcl_HashTable varTable;	 /* Contains all the (global) variables
				  * currently in this namespace. Indexed
				  * by strings; values have type (Var *). */
    char **exportArrayPtr;	 /* Points to an array of string patterns
				  * specifying which commands are exported.
				  * A pattern may include "string match"
				  * style wildcard characters to specify
				  * multiple commands; however, no namespace
				  * qualifiers are allowed. NULL if no
				  * export patterns are registered. */
    int numExportPatterns;	 /* Number of export patterns currently
				  * registered using "namespace export". */
    int maxExportPatterns;	 /* Mumber of export patterns for which
				  * space is currently allocated. */
    int cmdRefEpoch;		 /* Incremented if a newly added command
				  * shadows a command for which this
				  * namespace has already cached a Command *
				  * pointer; this causes all its cached
				  * Command* pointers to be invalidated. */
    int resolverEpoch;		 /* Incremented whenever (a) the name resolution
				  * rules change for this namespace or (b) a 
				  * newly added command shadows a command that
				  * is compiled to bytecodes.
				  * This invalidates all byte codes compiled
				  * in the namespace, causing the code to be
				  * recompiled under the new rules.*/
    Tcl_ResolveCmdProc *cmdResProc;
				 /* If non-null, this procedure overrides
				  * the usual command resolution mechanism
				  * in Tcl.  This procedure is invoked
				  * within Tcl_FindCommand to resolve all
				  * command references within the namespace. */
    Tcl_ResolveVarProc *varResProc;
				 /* If non-null, this procedure overrides
				  * the usual variable resolution mechanism
				  * in Tcl.  This procedure is invoked
				  * within Tcl_FindNamespaceVar to resolve all
				  * variable references within the namespace
				  * at runtime. */
    Tcl_ResolveCompiledVarProc *compiledVarResProc;
				 /* If non-null, this procedure overrides
				  * the usual variable resolution mechanism
				  * in Tcl.  This procedure is invoked
				  * within LookupCompiledLocal to resolve
				  * variable references within the namespace
				  * at compile time. */
} Namespace;

/*
 * Flags used to represent the status of a namespace:
 *
 * NS_DYING -	1 means Tcl_DeleteNamespace has been called to delete the
 *		namespace but there are still active call frames on the Tcl
 *		stack that refer to the namespace. When the last call frame
 *		referring to it has been popped, it's variables and command
 *		will be destroyed and it will be marked "dead" (NS_DEAD).
 *		The namespace can no longer be looked up by name.
 * NS_DEAD -	1 means Tcl_DeleteNamespace has been called to delete the
 *		namespace and no call frames still refer to it. Its
 *		variables and command have already been destroyed. This bit
 *		allows the namespace resolution code to recognize that the
 *		namespace is "deleted". When the last namespaceName object
 *		in any byte code code unit that refers to the namespace has
 *		been freed (i.e., when the namespace's refCount is 0), the
 *		namespace's storage will be freed.
 */

#define NS_DYING	0x01
#define NS_DEAD		0x02

/*
 * Flag passed to TclGetNamespaceForQualName to have it create all namespace
 * components of a namespace-qualified name that cannot be found. The new
 * namespaces are created within their specified parent. Note that this
 * flag's value must not conflict with the values of the flags
 * TCL_GLOBAL_ONLY, TCL_NAMESPACE_ONLY, and FIND_ONLY_NS (defined in
 * tclNamesp.c).
 */

#define CREATE_NS_IF_UNKNOWN 0x800

/*
 *----------------------------------------------------------------
 * Data structures related to variables.   These are used primarily
 * in tclVar.c
 *----------------------------------------------------------------
 */

/*
 * The following structure defines a variable trace, which is used to
 * invoke a specific C procedure whenever certain operations are performed
 * on a variable.
 */

typedef struct VarTrace {
    Tcl_VarTraceProc *traceProc;/* Procedure to call when operations given
				 * by flags are performed on variable. */
    ClientData clientData;	/* Argument to pass to proc. */
    int flags;			/* What events the trace procedure is
				 * interested in:  OR-ed combination of
				 * TCL_TRACE_READS, TCL_TRACE_WRITES,
				 * TCL_TRACE_UNSETS and TCL_TRACE_ARRAY. */
    struct VarTrace *nextPtr;	/* Next in list of traces associated with
				 * a particular variable. */
} VarTrace;

/*
 * The following structure defines a command trace, which is used to
 * invoke a specific C procedure whenever certain operations are performed
 * on a command.
 */

typedef struct CommandTrace {
    Tcl_CommandTraceProc *traceProc;/* Procedure to call when operations given
				     * by flags are performed on command. */
    ClientData clientData;	    /* Argument to pass to proc. */
    int flags;			    /* What events the trace procedure is
				     * interested in:  OR-ed combination of
				     * TCL_TRACE_RENAME, TCL_TRACE_DELETE. */
    struct CommandTrace *nextPtr;   /* Next in list of traces associated with
				     * a particular command. */
    int refCount;                   /* Used to ensure this structure is
                                     * not deleted too early.  Keeps track
                                     * of how many pieces of code have
                                     * a pointer to this structure. */
} CommandTrace;

/*
 * When a command trace is active (i.e. its associated procedure is
 * executing), one of the following structures is linked into a list
 * associated with the command's interpreter.  The information in
 * the structure is needed in order for Tcl to behave reasonably
 * if traces are deleted while traces are active.
 */

typedef struct ActiveCommandTrace {
    struct Command *cmdPtr;	/* Command that's being traced. */
    struct ActiveCommandTrace *nextPtr;
				/* Next in list of all active command
				 * traces for the interpreter, or NULL
				 * if no more. */
    CommandTrace *nextTracePtr;	/* Next trace to check after current
				 * trace procedure returns;  if this
				 * trace gets deleted, must update pointer
				 * to avoid using free'd memory. */
} ActiveCommandTrace;

/*
 * When a variable trace is active (i.e. its associated procedure is
 * executing), one of the following structures is linked into a list
 * associated with the variable's interpreter.	The information in
 * the structure is needed in order for Tcl to behave reasonably
 * if traces are deleted while traces are active.
 */

typedef struct ActiveVarTrace {
    struct Var *varPtr;		/* Variable that's being traced. */
    struct ActiveVarTrace *nextPtr;
				/* Next in list of all active variable
				 * traces for the interpreter, or NULL
				 * if no more. */
    VarTrace *nextTracePtr;	/* Next trace to check after current
				 * trace procedure returns;  if this
				 * trace gets deleted, must update pointer
				 * to avoid using free'd memory. */
} ActiveVarTrace;

/*
 * The following structure describes an enumerative search in progress on
 * an array variable;  this are invoked with options to the "array"
 * command.
 */

typedef struct ArraySearch {
    int id;			/* Integer id used to distinguish among
				 * multiple concurrent searches for the
				 * same array. */
    struct Var *varPtr;		/* Pointer to array variable that's being
				 * searched. */
    Tcl_HashSearch search;	/* Info kept by the hash module about
				 * progress through the array. */
    Tcl_HashEntry *nextEntry;	/* Non-null means this is the next element
				 * to be enumerated (it's leftover from
				 * the Tcl_FirstHashEntry call or from
				 * an "array anymore" command).	 NULL
				 * means must call Tcl_NextHashEntry
				 * to get value to return. */
    struct ArraySearch *nextPtr;/* Next in list of all active searches
				 * for this variable, or NULL if this is
				 * the last one. */
} ArraySearch;

/*
 * The structure below defines a variable, which associates a string name
 * with a Tcl_Obj value. These structures are kept in procedure call frames
 * (for local variables recognized by the compiler) or in the heap (for
 * global variables and any variable not known to the compiler). For each
 * Var structure in the heap, a hash table entry holds the variable name and
 * a pointer to the Var structure.
 */

typedef struct Var {
    union {
	Tcl_Obj *objPtr;	/* The variable's object value. Used for 
				 * scalar variables and array elements. */
	Tcl_HashTable *tablePtr;/* For array variables, this points to
				 * information about the hash table used
				 * to implement the associative array. 
				 * Points to malloc-ed data. */
	struct Var *linkPtr;	/* If this is a global variable being
				 * referred to in a procedure, or a variable
				 * created by "upvar", this field points to
				 * the referenced variable's Var struct. */
    } value;
    char *name;			/* NULL if the variable is in a hashtable,
				 * otherwise points to the variable's
				 * name. It is used, e.g., by TclLookupVar
				 * and "info locals". The storage for the
				 * characters of the name is not owned by
				 * the Var and must not be freed when
				 * freeing the Var. */
    Namespace *nsPtr;		/* Points to the namespace that contains
				 * this variable or NULL if the variable is
				 * a local variable in a Tcl procedure. */
    Tcl_HashEntry *hPtr;	/* If variable is in a hashtable, either the
				 * hash table entry that refers to this
				 * variable or NULL if the variable has been
				 * detached from its hash table (e.g. an
				 * array is deleted, but some of its
				 * elements are still referred to in
				 * upvars). NULL if the variable is not in a
				 * hashtable. This is used to delete an
				 * variable from its hashtable if it is no
				 * longer needed. */
    int refCount;		/* Counts number of active uses of this
				 * variable, not including its entry in the
				 * call frame or the hash table: 1 for each
				 * additional variable whose linkPtr points
				 * here, 1 for each nested trace active on
				 * variable, and 1 if the variable is a 
				 * namespace variable. This record can't be
				 * deleted until refCount becomes 0. */
    VarTrace *tracePtr;		/* First in list of all traces set for this
				 * variable. */
    ArraySearch *searchPtr;	/* First in list of all searches active
				 * for this variable, or NULL if none. */
    int flags;			/* Miscellaneous bits of information about
				 * variable. See below for definitions. */
} Var;

/*
 * Flag bits for variables. The first three (VAR_SCALAR, VAR_ARRAY, and
 * VAR_LINK) are mutually exclusive and give the "type" of the variable.
 * VAR_UNDEFINED is independent of the variable's type. 
 *
 * VAR_SCALAR -			1 means this is a scalar variable and not
 *				an array or link. The "objPtr" field points
 *				to the variable's value, a Tcl object.
 * VAR_ARRAY -			1 means this is an array variable rather
 *				than a scalar variable or link. The
 *				"tablePtr" field points to the array's
 *				hashtable for its elements.
 * VAR_LINK -			1 means this Var structure contains a
 *				pointer to another Var structure that
 *				either has the real value or is itself
 *				another VAR_LINK pointer. Variables like
 *				this come about through "upvar" and "global"
 *				commands, or through references to variables
 *				in enclosing namespaces.
 * VAR_UNDEFINED -		1 means that the variable is in the process
 *				of being deleted. An undefined variable
 *				logically does not exist and survives only
 *				while it has a trace, or if it is a global
 *				variable currently being used by some
 *				procedure.
 * VAR_IN_HASHTABLE -		1 means this variable is in a hashtable and
 *				the Var structure is malloced. 0 if it is
 *				a local variable that was assigned a slot
 *				in a procedure frame by	the compiler so the
 *				Var storage is part of the call frame.
 * VAR_TRACE_ACTIVE -		1 means that trace processing is currently
 *				underway for a read or write access, so
 *				new read or write accesses should not cause
 *				trace procedures to be called and the
 *				variable can't be deleted.
 * VAR_ARRAY_ELEMENT -		1 means that this variable is an array
 *				element, so it is not legal for it to be
 *				an array itself (the VAR_ARRAY flag had
 *				better not be set).
 * VAR_NAMESPACE_VAR -		1 means that this variable was declared
 *				as a namespace variable. This flag ensures
 *				it persists until its namespace is
 *				destroyed or until the variable is unset;
 *				it will persist even if it has not been
 *				initialized and is marked undefined.
 *				The variable's refCount is incremented to
 *				reflect the "reference" from its namespace.
 *
 * The following additional flags are used with the CompiledLocal type
 * defined below:
 *
 * VAR_ARGUMENT -		1 means that this variable holds a procedure
 *				argument. 
 * VAR_TEMPORARY -		1 if the local variable is an anonymous
 *				temporary variable. Temporaries have a NULL
 *				name.
 * VAR_RESOLVED -		1 if name resolution has been done for this
 *				variable.
 */

#define VAR_SCALAR		0x1
#define VAR_ARRAY		0x2
#define VAR_LINK		0x4
#define VAR_UNDEFINED		0x8
#define VAR_IN_HASHTABLE	0x10
#define VAR_TRACE_ACTIVE	0x20
#define VAR_ARRAY_ELEMENT	0x40
#define VAR_NAMESPACE_VAR	0x80

#define VAR_ARGUMENT		0x100
#define VAR_TEMPORARY		0x200
#define VAR_RESOLVED		0x400	

/*
 * Macros to ensure that various flag bits are set properly for variables.
 * The ANSI C "prototypes" for these macros are:
 *
 * EXTERN void	TclSetVarScalar _ANSI_ARGS_((Var *varPtr));
 * EXTERN void	TclSetVarArray _ANSI_ARGS_((Var *varPtr));
 * EXTERN void	TclSetVarLink _ANSI_ARGS_((Var *varPtr));
 * EXTERN void	TclSetVarArrayElement _ANSI_ARGS_((Var *varPtr));
 * EXTERN void	TclSetVarUndefined _ANSI_ARGS_((Var *varPtr));
 * EXTERN void	TclClearVarUndefined _ANSI_ARGS_((Var *varPtr));
 */

#define TclSetVarScalar(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~(VAR_ARRAY|VAR_LINK)) | VAR_SCALAR

#define TclSetVarArray(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~(VAR_SCALAR|VAR_LINK)) | VAR_ARRAY

#define TclSetVarLink(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~(VAR_SCALAR|VAR_ARRAY)) | VAR_LINK

#define TclSetVarArrayElement(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~VAR_ARRAY) | VAR_ARRAY_ELEMENT

#define TclSetVarUndefined(varPtr) \
    (varPtr)->flags |= VAR_UNDEFINED

#define TclClearVarUndefined(varPtr) \
    (varPtr)->flags &= ~VAR_UNDEFINED

/*
 * Macros to read various flag bits of variables.
 * The ANSI C "prototypes" for these macros are:
 *
 * EXTERN int	TclIsVarScalar _ANSI_ARGS_((Var *varPtr));
 * EXTERN int	TclIsVarLink _ANSI_ARGS_((Var *varPtr));
 * EXTERN int	TclIsVarArray _ANSI_ARGS_((Var *varPtr));
 * EXTERN int	TclIsVarUndefined _ANSI_ARGS_((Var *varPtr));
 * EXTERN int	TclIsVarArrayElement _ANSI_ARGS_((Var *varPtr));
 * EXTERN int	TclIsVarTemporary _ANSI_ARGS_((Var *varPtr));
 * EXTERN int	TclIsVarArgument _ANSI_ARGS_((Var *varPtr));
 * EXTERN int	TclIsVarResolved _ANSI_ARGS_((Var *varPtr));
 */
    
#define TclIsVarScalar(varPtr) \
    ((varPtr)->flags & VAR_SCALAR)

#define TclIsVarLink(varPtr) \
    ((varPtr)->flags & VAR_LINK)

#define TclIsVarArray(varPtr) \
    ((varPtr)->flags & VAR_ARRAY)

#define TclIsVarUndefined(varPtr) \
    ((varPtr)->flags & VAR_UNDEFINED)

#define TclIsVarArrayElement(varPtr) \
    ((varPtr)->flags & VAR_ARRAY_ELEMENT)

#define TclIsVarTemporary(varPtr) \
    ((varPtr)->flags & VAR_TEMPORARY)
    
#define TclIsVarArgument(varPtr) \
    ((varPtr)->flags & VAR_ARGUMENT)
    
#define TclIsVarResolved(varPtr) \
    ((varPtr)->flags & VAR_RESOLVED)

/*
 *----------------------------------------------------------------
 * Data structures related to procedures.  These are used primarily
 * in tclProc.c, tclCompile.c, and tclExecute.c.
 *----------------------------------------------------------------
 */

/*
 * Forward declaration to prevent an error when the forward reference to
 * Command is encountered in the Proc and ImportRef types declared below.
 */

struct Command;

/*
 * The variable-length structure below describes a local variable of a
 * procedure that was recognized by the compiler. These variables have a
 * name, an element in the array of compiler-assigned local variables in the
 * procedure's call frame, and various other items of information. If the
 * local variable is a formal argument, it may also have a default value.
 * The compiler can't recognize local variables whose names are
 * expressions (these names are only known at runtime when the expressions
 * are evaluated) or local variables that are created as a result of an
 * "upvar" or "uplevel" command. These other local variables are kept
 * separately in a hash table in the call frame.
 */

typedef struct CompiledLocal {
    struct CompiledLocal *nextPtr;
				/* Next compiler-recognized local variable
				 * for this procedure, or NULL if this is
				 * the last local. */
    int nameLength;		/* The number of characters in local
				 * variable's name. Used to speed up
				 * variable lookups. */
    int frameIndex;		/* Index in the array of compiler-assigned
				 * variables in the procedure call frame. */
    int flags;			/* Flag bits for the local variable. Same as
				 * the flags for the Var structure above,
				 * although only VAR_SCALAR, VAR_ARRAY, 
				 * VAR_LINK, VAR_ARGUMENT, VAR_TEMPORARY, and
				 * VAR_RESOLVED make sense. */
    Tcl_Obj *defValuePtr;	/* Pointer to the default value of an
				 * argument, if any. NULL if not an argument
				 * or, if an argument, no default value. */
    Tcl_ResolvedVarInfo *resolveInfo;
				/* Customized variable resolution info
				 * supplied by the Tcl_ResolveCompiledVarProc
				 * associated with a namespace. Each variable
				 * is marked by a unique ClientData tag
				 * during compilation, and that same tag
				 * is used to find the variable at runtime. */
    char name[4];		/* Name of the local variable starts here.
				 * If the name is NULL, this will just be
				 * '\0'. The actual size of this field will
				 * be large enough to hold the name. MUST
				 * BE THE LAST FIELD IN THE STRUCTURE! */
} CompiledLocal;

/*
 * The structure below defines a command procedure, which consists of a
 * collection of Tcl commands plus information about arguments and other
 * local variables recognized at compile time.
 */

typedef struct Proc {
    struct Interp *iPtr;	  /* Interpreter for which this command
				   * is defined. */
    int refCount;		  /* Reference count: 1 if still present
				   * in command table plus 1 for each call
				   * to the procedure that is currently
				   * active. This structure can be freed
				   * when refCount becomes zero. */
    struct Command *cmdPtr;	  /* Points to the Command structure for
				   * this procedure. This is used to get
				   * the namespace in which to execute
				   * the procedure. */
    Tcl_Obj *bodyPtr;		  /* Points to the ByteCode object for
				   * procedure's body command. */
    int numArgs;		  /* Number of formal parameters. */
    int numCompiledLocals;	  /* Count of local variables recognized by
				   * the compiler including arguments and
				   * temporaries. */
    CompiledLocal *firstLocalPtr; /* Pointer to first of the procedure's
				   * compiler-allocated local variables, or
				   * NULL if none. The first numArgs entries
				   * in this list describe the procedure's
				   * formal arguments. */
    CompiledLocal *lastLocalPtr;  /* Pointer to the last allocated local
				   * variable or NULL if none. This has
				   * frame index (numCompiledLocals-1). */
} Proc;

/*
 * The structure below defines a command trace.	 This is used to allow Tcl
 * clients to find out whenever a command is about to be executed.
 */

typedef struct Trace {
    int level;			/* Only trace commands at nesting level
				 * less than or equal to this. */
    Tcl_CmdObjTraceProc *proc;	/* Procedure to call to trace command. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
    struct Trace *nextPtr;	/* Next in list of traces for this interp. */
    int flags;			/* Flags governing the trace - see
				 * Tcl_CreateObjTrace for details */
    Tcl_CmdObjTraceDeleteProc* delProc;
				/* Procedure to call when trace is deleted */
} Trace;

/*
 * When an interpreter trace is active (i.e. its associated procedure
 * is executing), one of the following structures is linked into a list
 * associated with the interpreter.  The information in the structure
 * is needed in order for Tcl to behave reasonably if traces are
 * deleted while traces are active.
 */

typedef struct ActiveInterpTrace {
    struct ActiveInterpTrace *nextPtr;
				/* Next in list of all active command
				 * traces for the interpreter, or NULL
				 * if no more. */
    Trace *nextTracePtr;	/* Next trace to check after current
				 * trace procedure returns;  if this
				 * trace gets deleted, must update pointer
				 * to avoid using free'd memory. */
} ActiveInterpTrace;

/*
 * The structure below defines an entry in the assocData hash table which
 * is associated with an interpreter. The entry contains a pointer to a
 * function to call when the interpreter is deleted, and a pointer to
 * a user-defined piece of data.
 */

typedef struct AssocData {
    Tcl_InterpDeleteProc *proc;	/* Proc to call when deleting. */
    ClientData clientData;	/* Value to pass to proc. */
} AssocData;	

/*
 * The structure below defines a call frame. A call frame defines a naming
 * context for a procedure call: its local naming scope (for local
 * variables) and its global naming scope (a namespace, perhaps the global
 * :: namespace). A call frame can also define the naming context for a
 * namespace eval or namespace inscope command: the namespace in which the
 * command's code should execute. The Tcl_CallFrame structures exist only
 * while procedures or namespace eval/inscope's are being executed, and
 * provide a kind of Tcl call stack.
 * 
 * WARNING!! The structure definition must be kept consistent with the
 * Tcl_CallFrame structure in tcl.h. If you change one, change the other.
 */

typedef struct CallFrame {
    Namespace *nsPtr;		/* Points to the namespace used to resolve
				 * commands and global variables. */
    int isProcCallFrame;	/* If nonzero, the frame was pushed to
				 * execute a Tcl procedure and may have
				 * local vars. If 0, the frame was pushed
				 * to execute a namespace command and var
				 * references are treated as references to
				 * namespace vars; varTablePtr and
				 * compiledLocals are ignored. */
    int objc;			/* This and objv below describe the
				 * arguments for this procedure call. */
    Tcl_Obj *CONST *objv;	/* Array of argument objects. */
    struct CallFrame *callerPtr;
				/* Value of interp->framePtr when this
				 * procedure was invoked (i.e. next higher
				 * in stack of all active procedures). */
    struct CallFrame *callerVarPtr;
				/* Value of interp->varFramePtr when this
				 * procedure was invoked (i.e. determines
				 * variable scoping within caller). Same
				 * as callerPtr unless an "uplevel" command
				 * or something equivalent was active in
				 * the caller). */
    int level;			/* Level of this procedure, for "uplevel"
				 * purposes (i.e. corresponds to nesting of
				 * callerVarPtr's, not callerPtr's). 1 for
				 * outermost procedure, 0 for top-level. */
    Proc *procPtr;		/* Points to the structure defining the
				 * called procedure. Used to get information
				 * such as the number of compiled local
				 * variables (local variables assigned
				 * entries ["slots"] in the compiledLocals
				 * array below). */
    Tcl_HashTable *varTablePtr;	/* Hash table containing local variables not
				 * recognized by the compiler, or created at
				 * execution time through, e.g., upvar.
				 * Initially NULL and created if needed. */
    int numCompiledLocals;	/* Count of local variables recognized by
				 * the compiler including arguments. */
    Var* compiledLocals;	/* Points to the array of local variables
				 * recognized by the compiler. The compiler
				 * emits code that refers to these variables
				 * using an index into this array. */
} CallFrame;

/*
 *----------------------------------------------------------------
 * Data structures and procedures related to TclHandles, which
 * are a very lightweight method of preserving enough information
 * to determine if an arbitrary malloc'd block has been deleted.
 *----------------------------------------------------------------
 */

typedef VOID **TclHandle;

/*
 *----------------------------------------------------------------
 * Data structures related to expressions.  These are used only in
 * tclExpr.c.
 *----------------------------------------------------------------
 */

/*
 * The data structure below defines a math function (e.g. sin or hypot)
 * for use in Tcl expressions.
 */

#define MAX_MATH_ARGS 5
typedef struct MathFunc {
    int builtinFuncIndex;	/* If this is a builtin math function, its
				 * index in the array of builtin functions.
				 * (tclCompilation.h lists these indices.)
				 * The value is -1 if this is a new function
				 * defined by Tcl_CreateMathFunc. The value
				 * is also -1 if a builtin function is
				 * replaced by a Tcl_CreateMathFunc call. */
    int numArgs;		/* Number of arguments for function. */
    Tcl_ValueType argTypes[MAX_MATH_ARGS];
				/* Acceptable types for each argument. */
    Tcl_MathProc *proc;		/* Procedure that implements this function.
				 * NULL if isBuiltinFunc is 1. */
    ClientData clientData;	/* Additional argument to pass to the
				 * function when invoking it. NULL if
				 * isBuiltinFunc is 1. */
} MathFunc;

/*
 * These are a thin layer over TclpThreadKeyDataGet and TclpThreadKeyDataSet
 * when threads are used, or an emulation if there are no threads.  These
 * are really internal and Tcl clients should use Tcl_GetThreadData.
 */

EXTERN VOID *TclThreadDataKeyGet _ANSI_ARGS_((Tcl_ThreadDataKey *keyPtr));
EXTERN void TclThreadDataKeySet _ANSI_ARGS_((Tcl_ThreadDataKey *keyPtr, VOID *data));

/*
 * This is a convenience macro used to initialize a thread local storage ptr.
 */
#define TCL_TSD_INIT(keyPtr)	(ThreadSpecificData *)Tcl_GetThreadData((keyPtr), sizeof(ThreadSpecificData))


/*
 *----------------------------------------------------------------
 * Data structures related to bytecode compilation and execution.
 * These are used primarily in tclCompile.c, tclExecute.c, and
 * tclBasic.c.
 *----------------------------------------------------------------
 */

/*
 * Forward declaration to prevent errors when the forward references to
 * Tcl_Parse and CompileEnv are encountered in the procedure type
 * CompileProc declared below.
 */

struct CompileEnv;

/*
 * The type of procedures called by the Tcl bytecode compiler to compile
 * commands. Pointers to these procedures are kept in the Command structure
 * describing each command. When a CompileProc returns, the interpreter's
 * result is set to error information, if any. In addition, the CompileProc
 * returns an integer value, which is one of the following:
 *
 * TCL_OK		Compilation completed normally.
 * TCL_ERROR		Compilation failed because of an error;
 *			the interpreter's result describes what went wrong.
 * TCL_OUT_LINE_COMPILE	Compilation failed because, e.g., the command is
 *			too complex for effective inline compilation. The
 *			CompileProc believes the command is legal but 
 *			should be compiled "out of line" by emitting code
 *			to invoke its command procedure at runtime.
 */

#define TCL_OUT_LINE_COMPILE	(TCL_CONTINUE + 1)

typedef int (CompileProc) _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_Parse *parsePtr, struct CompileEnv *compEnvPtr));

/*
 * The type of procedure called from the compilation hook point in
 * SetByteCodeFromAny.
 */

typedef int (CompileHookProc) _ANSI_ARGS_((Tcl_Interp *interp,
	struct CompileEnv *compEnvPtr, ClientData clientData));

/*
 * The data structure defining the execution environment for ByteCode's.
 * There is one ExecEnv structure per Tcl interpreter. It holds the
 * evaluation stack that holds command operands and results. The stack grows
 * towards increasing addresses. The "stackTop" member is cached by
 * TclExecuteByteCode in a local variable: it must be set before calling
 * TclExecuteByteCode and will be restored by TclExecuteByteCode before it
 * returns.
 */

typedef struct ExecEnv {
    Tcl_Obj **stackPtr;		/* Points to the first item in the
				 * evaluation stack on the heap. */
    int stackTop;		/* Index of current top of stack; -1 when
				 * the stack is empty. */
    int stackEnd;		/* Index of last usable item in stack. */
    Tcl_Obj *errorInfo;
    Tcl_Obj *errorCode;
} ExecEnv;

/*
 * The definitions for the LiteralTable and LiteralEntry structures. Each
 * interpreter contains a LiteralTable. It is used to reduce the storage
 * needed for all the Tcl objects that hold the literals of scripts compiled
 * by the interpreter. A literal's object is shared by all the ByteCodes
 * that refer to the literal. Each distinct literal has one LiteralEntry
 * entry in the LiteralTable. A literal table is a specialized hash table
 * that is indexed by the literal's string representation, which may contain
 * null characters.
 *
 * Note that we reduce the space needed for literals by sharing literal
 * objects both within a ByteCode (each ByteCode contains a local
 * LiteralTable) and across all an interpreter's ByteCodes (with the
 * interpreter's global LiteralTable).
 */

typedef struct LiteralEntry {
    struct LiteralEntry *nextPtr;	/* Points to next entry in this
					 * hash bucket or NULL if end of
					 * chain. */
    Tcl_Obj *objPtr;			/* Points to Tcl object that
					 * holds the literal's bytes and
					 * length. */
    int refCount;			/* If in an interpreter's global
					 * literal table, the number of
					 * ByteCode structures that share
					 * the literal object; the literal
					 * entry can be freed when refCount
					 * drops to 0. If in a local literal
					 * table, -1. */
} LiteralEntry;

typedef struct LiteralTable {
    LiteralEntry **buckets;		/* Pointer to bucket array. Each
					 * element points to first entry in
					 * bucket's hash chain, or NULL. */
    LiteralEntry *staticBuckets[TCL_SMALL_HASH_TABLE];
					/* Bucket array used for small
					 * tables to avoid mallocs and
					 * frees. */
    int numBuckets;			/* Total number of buckets allocated
					 * at **buckets. */
    int numEntries;			/* Total number of entries present
					 * in table. */
    int rebuildSize;			/* Enlarge table when numEntries
					 * gets to be this large. */
    int mask;				/* Mask value used in hashing
					 * function. */
} LiteralTable;

/*
 * The following structure defines for each Tcl interpreter various
 * statistics-related information about the bytecode compiler and
 * interpreter's operation in that interpreter.
 */

#ifdef TCL_COMPILE_STATS
typedef struct ByteCodeStats {
    long numExecutions;		  /* Number of ByteCodes executed. */
    long numCompilations;	  /* Number of ByteCodes created. */
    long numByteCodesFreed;	  /* Number of ByteCodes destroyed. */
    long instructionCount[256];	  /* Number of times each instruction was
				   * executed. */

    double totalSrcBytes;	  /* Total source bytes ever compiled. */
    double totalByteCodeBytes;	  /* Total bytes for all ByteCodes. */
    double currentSrcBytes;	  /* Src bytes for all current ByteCodes. */
    double currentByteCodeBytes;  /* Code bytes in all current ByteCodes. */

    long srcCount[32];		  /* Source size distribution: # of srcs of
				   * size [2**(n-1)..2**n), n in [0..32). */
    long byteCodeCount[32];	  /* ByteCode size distribution. */
    long lifetimeCount[32];	  /* ByteCode lifetime distribution (ms). */
    
    double currentInstBytes;	  /* Instruction bytes-current ByteCodes. */
    double currentLitBytes;	  /* Current literal bytes. */
    double currentExceptBytes;	  /* Current exception table bytes. */
    double currentAuxBytes;	  /* Current auxiliary information bytes. */
    double currentCmdMapBytes;	  /* Current src<->code map bytes. */
    
    long numLiteralsCreated;	  /* Total literal objects ever compiled. */
    double totalLitStringBytes;	  /* Total string bytes in all literals. */
    double currentLitStringBytes; /* String bytes in current literals. */
    long literalCount[32];	  /* Distribution of literal string sizes. */
} ByteCodeStats;
#endif /* TCL_COMPILE_STATS */

/*
 *----------------------------------------------------------------
 * Data structures related to commands.
 *----------------------------------------------------------------
 */

/*
 * An imported command is created in an namespace when it imports a "real"
 * command from another namespace. An imported command has a Command
 * structure that points (via its ClientData value) to the "real" Command
 * structure in the source namespace's command table. The real command
 * records all the imported commands that refer to it in a list of ImportRef
 * structures so that they can be deleted when the real command is deleted.  */

typedef struct ImportRef {
    struct Command *importedCmdPtr;
				/* Points to the imported command created in
				 * an importing namespace; this command
				 * redirects its invocations to the "real"
				 * command. */
    struct ImportRef *nextPtr;	/* Next element on the linked list of
				 * imported commands that refer to the
				 * "real" command. The real command deletes
				 * these imported commands on this list when
				 * it is deleted. */
} ImportRef;

/*
 * Data structure used as the ClientData of imported commands: commands
 * created in an namespace when it imports a "real" command from another
 * namespace.
 */

typedef struct ImportedCmdData {
    struct Command *realCmdPtr;	/* "Real" command that this imported command
				 * refers to. */
    struct Command *selfPtr;	/* Pointer to this imported command. Needed
				 * only when deleting it in order to remove
				 * it from the real command's linked list of
				 * imported commands that refer to it. */
} ImportedCmdData;

/*
 * A Command structure exists for each command in a namespace. The
 * Tcl_Command opaque type actually refers to these structures.
 */

typedef struct Command {
    Tcl_HashEntry *hPtr;	/* Pointer to the hash table entry that
				 * refers to this command. The hash table is
				 * either a namespace's command table or an
				 * interpreter's hidden command table. This
				 * pointer is used to get a command's name
				 * from its Tcl_Command handle. NULL means
				 * that the hash table entry has been
				 * removed already (this can happen if
				 * deleteProc causes the command to be
				 * deleted or recreated). */
    Namespace *nsPtr;		/* Points to the namespace containing this
				 * command. */
    int refCount;		/* 1 if in command hashtable plus 1 for each
				 * reference from a CmdName Tcl object
				 * representing a command's name in a
				 * ByteCode instruction sequence. This
				 * structure can be freed when refCount
				 * becomes zero. */
    int cmdEpoch;		/* Incremented to invalidate any references
				 * that point to this command when it is
				 * renamed, deleted, hidden, or exposed. */
    CompileProc *compileProc;	/* Procedure called to compile command. NULL
				 * if no compile proc exists for command. */
    Tcl_ObjCmdProc *objProc;	/* Object-based command procedure. */
    ClientData objClientData;	/* Arbitrary value passed to object proc. */
    Tcl_CmdProc *proc;		/* String-based command procedure. */
    ClientData clientData;	/* Arbitrary value passed to string proc. */
    Tcl_CmdDeleteProc *deleteProc;
				/* Procedure invoked when deleting command
				 * to, e.g., free all client data. */
    ClientData deleteData;	/* Arbitrary value passed to deleteProc. */
    int flags;			/* Miscellaneous bits of information about
				 * command. See below for definitions. */
    ImportRef *importRefPtr;	/* List of each imported Command created in
				 * another namespace when this command is
				 * imported. These imported commands
				 * redirect invocations back to this
				 * command. The list is used to remove all
				 * those imported commands when deleting
				 * this "real" command. */
    CommandTrace *tracePtr;	/* First in list of all traces set for this
				 * command. */
} Command;

/*
 * Flag bits for commands. 
 *
 * CMD_IS_DELETED -		Means that the command is in the process
 *                              of being deleted (its deleteProc is
 *                              currently executing). Other attempts to
 *                              delete the command should be ignored.
 * CMD_TRACE_ACTIVE -		1 means that trace processing is currently
 *				underway for a rename/delete change.
 *				See the two flags below for which is
 *				currently being processed.
 * CMD_HAS_EXEC_TRACES -	1 means that this command has at least
 *                              one execution trace (as opposed to simple
 *                              delete/rename traces) in its tracePtr list.
 * TCL_TRACE_RENAME -           A rename trace is in progress. Further
 *                              recursive renames will not be traced.
 * TCL_TRACE_DELETE -           A delete trace is in progress. Further 
 *                              recursive deletes will not be traced.
 * (these last two flags are defined in tcl.h)
 */
#define CMD_IS_DELETED		0x1
#define CMD_TRACE_ACTIVE	0x2
#define CMD_HAS_EXEC_TRACES	0x4

/*
 *----------------------------------------------------------------
 * Data structures related to name resolution procedures.
 *----------------------------------------------------------------
 */

/*
 * The interpreter keeps a linked list of name resolution schemes.
 * The scheme for a namespace is consulted first, followed by the
 * list of schemes in an interpreter, followed by the default
 * name resolution in Tcl.  Schemes are added/removed from the
 * interpreter's list by calling Tcl_AddInterpResolver and
 * Tcl_RemoveInterpResolver.
 */

typedef struct ResolverScheme {
    char *name;			/* Name identifying this scheme. */
    Tcl_ResolveCmdProc *cmdResProc;
				/* Procedure handling command name
				 * resolution. */
    Tcl_ResolveVarProc *varResProc;
				/* Procedure handling variable name
				 * resolution for variables that
				 * can only be handled at runtime. */
    Tcl_ResolveCompiledVarProc *compiledVarResProc;
				/* Procedure handling variable name
				 * resolution at compile time. */

    struct ResolverScheme *nextPtr;
				/* Pointer to next record in linked list. */
} ResolverScheme;

/*
 *----------------------------------------------------------------
 * This structure defines an interpreter, which is a collection of
 * commands plus other state information related to interpreting
 * commands, such as variable storage. Primary responsibility for
 * this data structure is in tclBasic.c, but almost every Tcl
 * source file uses something in here.
 *----------------------------------------------------------------
 */

typedef struct Interp {

    /*
     * Note:  the first three fields must match exactly the fields in
     * a Tcl_Interp struct (see tcl.h).	 If you change one, be sure to
     * change the other.
     *
     * The interpreter's result is held in both the string and the
     * objResultPtr fields. These fields hold, respectively, the result's
     * string or object value. The interpreter's result is always in the
     * result field if that is non-empty, otherwise it is in objResultPtr.
     * The two fields are kept consistent unless some C code sets
     * interp->result directly. Programs should not access result and
     * objResultPtr directly; instead, they should always get and set the
     * result using procedures such as Tcl_SetObjResult, Tcl_GetObjResult,
     * and Tcl_GetStringResult. See the SetResult man page for details.
     */

    char *result;		/* If the last command returned a string
				 * result, this points to it. Should not be
				 * accessed directly; see comment above. */
    Tcl_FreeProc *freeProc;	/* Zero means a string result is statically
				 * allocated. TCL_DYNAMIC means string
				 * result was allocated with ckalloc and
				 * should be freed with ckfree. Other values
				 * give address of procedure to invoke to
				 * free the string result. Tcl_Eval must
				 * free it before executing next command. */
    int errorLine;		/* When TCL_ERROR is returned, this gives
				 * the line number in the command where the
				 * error occurred (1 means first line). */
    struct TclStubs *stubTable;
				/* Pointer to the exported Tcl stub table.
				 * On previous versions of Tcl this is a
				 * pointer to the objResultPtr or a pointer
				 * to a buckets array in a hash table. We
				 * therefore have to do some careful checking
				 * before we can use this. */

    TclHandle handle;		/* Handle used to keep track of when this
				 * interp is deleted. */

    Namespace *globalNsPtr;	/* The interpreter's global namespace. */
    Tcl_HashTable *hiddenCmdTablePtr;
				/* Hash table used by tclBasic.c to keep
				 * track of hidden commands on a per-interp
				 * basis. */
    ClientData interpInfo;	/* Information used by tclInterp.c to keep
				 * track of master/slave interps on
				 * a per-interp basis. */
    Tcl_HashTable mathFuncTable;/* Contains all the math functions currently
				 * defined for the interpreter.	 Indexed by
				 * strings (function names); values have
				 * type (MathFunc *). */



    /*
     * Information related to procedures and variables. See tclProc.c
     * and tclVar.c for usage.
     */

    int numLevels;		/* Keeps track of how many nested calls to
				 * Tcl_Eval are in progress for this
				 * interpreter.	 It's used to delay deletion
				 * of the table until all Tcl_Eval
				 * invocations are completed. */
    int maxNestingDepth;	/* If numLevels exceeds this value then Tcl
				 * assumes that infinite recursion has
				 * occurred and it generates an error. */
    CallFrame *framePtr;	/* Points to top-most in stack of all nested
				 * procedure invocations.  NULL means there
				 * are no active procedures. */
    CallFrame *varFramePtr;	/* Points to the call frame whose variables
				 * are currently in use (same as framePtr
				 * unless an "uplevel" command is
				 * executing). NULL means no procedure is
				 * active or "uplevel 0" is executing. */
    ActiveVarTrace *activeVarTracePtr;
				/* First in list of active traces for
				 * interp, or NULL if no active traces. */
    int returnCode;		/* Completion code to return if current
				 * procedure exits with TCL_RETURN code. */
    char *errorInfo;		/* Value to store in errorInfo if returnCode
				 * is TCL_ERROR.  Malloc'ed, may be NULL */
    char *errorCode;		/* Value to store in errorCode if returnCode
				 * is TCL_ERROR.  Malloc'ed, may be NULL */

    /*
     * Information used by Tcl_AppendResult to keep track of partial
     * results.	 See Tcl_AppendResult code for details.
     */

    char *appendResult;		/* Storage space for results generated
				 * by Tcl_AppendResult.	 Malloc-ed.  NULL
				 * means not yet allocated. */
    int appendAvl;		/* Total amount of space available at
				 * partialResult. */
    int appendUsed;		/* Number of non-null bytes currently
				 * stored at partialResult. */

    /*
     * Information about packages.  Used only in tclPkg.c.
     */

    Tcl_HashTable packageTable;	/* Describes all of the packages loaded
				 * in or available to this interpreter.
				 * Keys are package names, values are
				 * (Package *) pointers. */
    char *packageUnknown;	/* Command to invoke during "package
				 * require" commands for packages that
				 * aren't described in packageTable. 
				 * Malloc'ed, may be NULL. */

    /*
     * Miscellaneous information:
     */

    int cmdCount;		/* Total number of times a command procedure
				 * has been called for this interpreter. */
    int evalFlags;		/* Flags to control next call to Tcl_Eval.
				 * Normally zero, but may be set before
				 * calling Tcl_Eval.  See below for valid
				 * values. */
    int termOffset;		/* Offset of character just after last one
				 * compiled or executed by Tcl_EvalObj. */
    LiteralTable literalTable;	/* Contains LiteralEntry's describing all
				 * Tcl objects holding literals of scripts
				 * compiled by the interpreter. Indexed by
				 * the string representations of literals.
				 * Used to avoid creating duplicate
				 * objects. */
    int compileEpoch;		/* Holds the current "compilation epoch"
				 * for this interpreter. This is
				 * incremented to invalidate existing
				 * ByteCodes when, e.g., a command with a
				 * compile procedure is redefined. */
    Proc *compiledProcPtr;	/* If a procedure is being compiled, a
				 * pointer to its Proc structure; otherwise,
				 * this is NULL. Set by ObjInterpProc in
				 * tclProc.c and used by tclCompile.c to
				 * process local variables appropriately. */
    ResolverScheme *resolverPtr;
				/* Linked list of name resolution schemes
				 * added to this interpreter.  Schemes
				 * are added/removed by calling
				 * Tcl_AddInterpResolvers and
				 * Tcl_RemoveInterpResolver. */
    Tcl_Obj *scriptFile;	/* NULL means there is no nested source
				 * command active;  otherwise this points to
				 * pathPtr of the file being sourced. */
    int flags;			/* Various flag bits.  See below. */
    long randSeed;		/* Seed used for rand() function. */
    Trace *tracePtr;		/* List of traces for this interpreter. */
    Tcl_HashTable *assocData;	/* Hash table for associating data with
				 * this interpreter. Cleaned up when
				 * this interpreter is deleted. */
    struct ExecEnv *execEnvPtr;	/* Execution environment for Tcl bytecode
				 * execution. Contains a pointer to the
				 * Tcl evaluation stack. */
    Tcl_Obj *emptyObjPtr;	/* Points to an object holding an empty
				 * string. Returned by Tcl_ObjSetVar2 when
				 * variable traces change a variable in a
				 * gross way. */
    char resultSpace[TCL_RESULT_SIZE+1];
				/* Static space holding small results. */
    Tcl_Obj *objResultPtr;	/* If the last command returned an object
				 * result, this points to it. Should not be
				 * accessed directly; see comment above. */
    Tcl_ThreadId threadId;	/* ID of thread that owns the interpreter */

    ActiveCommandTrace *activeCmdTracePtr;
				/* First in list of active command traces for
				 * interp, or NULL if no active traces. */
    ActiveInterpTrace *activeInterpTracePtr;
				/* First in list of active traces for
				 * interp, or NULL if no active traces. */

    int tracesForbiddingInline; /* Count of traces (in the list headed by
				 * tracePtr) that forbid inline bytecode
				 * compilation */
    /*
     * Statistical information about the bytecode compiler and interpreter's
     * operation.
     */

#ifdef TCL_COMPILE_STATS
    ByteCodeStats stats;	/* Holds compilation and execution
				 * statistics for this interpreter. */
#endif /* TCL_COMPILE_STATS */	  
} Interp;

/*
 * EvalFlag bits for Interp structures:
 *
 * TCL_BRACKET_TERM	1 means that the current script is terminated by
 *			a close bracket rather than the end of the string.
 * TCL_ALLOW_EXCEPTIONS	1 means it's OK for the script to terminate with
 *			a code other than TCL_OK or TCL_ERROR;	0 means
 *			codes other than these should be turned into errors.
 */

#define TCL_BRACKET_TERM	  1
#define TCL_ALLOW_EXCEPTIONS	  4

/*
 * Flag bits for Interp structures:
 *
 * DELETED:		Non-zero means the interpreter has been deleted:
 *			don't process any more commands for it, and destroy
 *			the structure as soon as all nested invocations of
 *			Tcl_Eval are done.
 * ERR_IN_PROGRESS:	Non-zero means an error unwind is already in
 *			progress. Zero means a command proc has been
 *			invoked since last error occured.
 * ERR_ALREADY_LOGGED:	Non-zero means information has already been logged
 *			in $errorInfo for the current Tcl_Eval instance,
 *			so Tcl_Eval needn't log it (used to implement the
 *			"error message log" command).
 * ERROR_CODE_SET:	Non-zero means that Tcl_SetErrorCode has been
 *			called to record information for the current
 *			error.	Zero means Tcl_Eval must clear the
 *			errorCode variable if an error is returned.
 * EXPR_INITIALIZED:	Non-zero means initialization specific to
 *			expressions has	been carried out.
 * DONT_COMPILE_CMDS_INLINE: Non-zero means that the bytecode compiler
 *			should not compile any commands into an inline
 *			sequence of instructions. This is set 1, for
 *			example, when command traces are requested.
 * RAND_SEED_INITIALIZED: Non-zero means that the randSeed value of the
 *			interp has not be initialized.	This is set 1
 *			when we first use the rand() or srand() functions.
 * SAFE_INTERP:		Non zero means that the current interp is a
 *			safe interp (ie it has only the safe commands
 *			installed, less priviledge than a regular interp).
 * USE_EVAL_DIRECT:	Non-zero means don't use the compiler or byte-code
 *			interpreter; instead, have Tcl_EvalObj call
 *			Tcl_EvalEx. Used primarily for testing the
 *			new parser.
 * INTERP_TRACE_IN_PROGRESS: Non-zero means that an interp trace is currently
 *			active; so no further trace callbacks should be
 *			invoked.
 */

#define DELETED				    1
#define ERR_IN_PROGRESS			    2
#define ERR_ALREADY_LOGGED		    4
#define ERROR_CODE_SET			    8
#define EXPR_INITIALIZED		 0x10
#define DONT_COMPILE_CMDS_INLINE	 0x20
#define RAND_SEED_INITIALIZED		 0x40
#define SAFE_INTERP			 0x80
#define USE_EVAL_DIRECT			0x100
#define INTERP_TRACE_IN_PROGRESS	0x200

/*
 *----------------------------------------------------------------
 * Data structures related to command parsing. These are used in
 * tclParse.c and its clients.
 *----------------------------------------------------------------
 */

/*
 * The following data structure is used by various parsing procedures
 * to hold information about where to store the results of parsing
 * (e.g. the substituted contents of a quoted argument, or the result
 * of a nested command).  At any given time, the space available
 * for output is fixed, but a procedure may be called to expand the
 * space available if the current space runs out.
 */

typedef struct ParseValue {
    char *buffer;		/* Address of first character in
				 * output buffer. */
    char *next;			/* Place to store next character in
				 * output buffer. */
    char *end;			/* Address of the last usable character
				 * in the buffer. */
    void (*expandProc) _ANSI_ARGS_((struct ParseValue *pvPtr, int needed));
				/* Procedure to call when space runs out;
				 * it will make more space. */
    ClientData clientData;	/* Arbitrary information for use of
				 * expandProc. */
} ParseValue;


/*
 * Maximum number of levels of nesting permitted in Tcl commands (used
 * to catch infinite recursion).
 */

#define MAX_NESTING_DEPTH	1000

/*
 * The macro below is used to modify a "char" value (e.g. by casting
 * it to an unsigned character) so that it can be used safely with
 * macros such as isspace.
 */

#define UCHAR(c) ((unsigned char) (c))

/*
 * This macro is used to determine the offset needed to safely allocate any
 * data structure in memory. Given a starting offset or size, it "rounds up"
 * or "aligns" the offset to the next 8-byte boundary so that any data
 * structure can be placed at the resulting offset without fear of an
 * alignment error.
 *
 * WARNING!! DO NOT USE THIS MACRO TO ALIGN POINTERS: it will produce
 * the wrong result on platforms that allocate addresses that are divisible
 * by 4 or 2. Only use it for offsets or sizes.
 */

#define TCL_ALIGN(x) (((int)(x) + 7) & ~7)

/*
 * The following enum values are used to specify the runtime platform
 * setting of the tclPlatform variable.
 */

typedef enum {
    TCL_PLATFORM_UNIX,		/* Any Unix-like OS. */
    TCL_PLATFORM_MAC,		/* MacOS. */
    TCL_PLATFORM_WINDOWS	/* Any Microsoft Windows OS. */
} TclPlatformType;

/*
 *  The following enum values are used to indicate the translation
 *  of a Tcl channel.  Declared here so that each platform can define
 *  TCL_PLATFORM_TRANSLATION to the native translation on that platform
 */

typedef enum TclEolTranslation {
    TCL_TRANSLATE_AUTO,                 /* Eol == \r, \n and \r\n. */
    TCL_TRANSLATE_CR,                   /* Eol == \r. */
    TCL_TRANSLATE_LF,                   /* Eol == \n. */
    TCL_TRANSLATE_CRLF                  /* Eol == \r\n. */
} TclEolTranslation;

/*
 * Flags for TclInvoke:
 *
 * TCL_INVOKE_HIDDEN		Invoke a hidden command; if not set,
 *				invokes an exposed command.
 * TCL_INVOKE_NO_UNKNOWN	If set, "unknown" is not invoked if
 *				the command to be invoked is not found.
 *				Only has an effect if invoking an exposed
 *				command, i.e. if TCL_INVOKE_HIDDEN is not
 *				also set.
 * TCL_INVOKE_NO_TRACEBACK	Does not record traceback information if
 *				the invoked command returns an error.  Used
 *				if the caller plans on recording its own
 *				traceback information.
 */

#define	TCL_INVOKE_HIDDEN	(1<<0)
#define TCL_INVOKE_NO_UNKNOWN	(1<<1)
#define TCL_INVOKE_NO_TRACEBACK	(1<<2)

/*
 * The structure used as the internal representation of Tcl list
 * objects. This is an array of pointers to the element objects. This array
 * is grown (reallocated and copied) as necessary to hold all the list's
 * element pointers. The array might contain more slots than currently used
 * to hold all element pointers. This is done to make append operations
 * faster.
 */

typedef struct List {
    int maxElemCount;		/* Total number of element array slots. */
    int elemCount;		/* Current number of list elements. */
    Tcl_Obj **elements;		/* Array of pointers to element objects. */
} List;


/*
 * The following types are used for getting and storing platform-specific
 * file attributes in tclFCmd.c and the various platform-versions of
 * that file. This is done to have as much common code as possible
 * in the file attributes code. For more information about the callbacks,
 * see TclFileAttrsCmd in tclFCmd.c.
 */

typedef int (TclGetFileAttrProc) _ANSI_ARGS_((Tcl_Interp *interp,
	int objIndex, Tcl_Obj *fileName, Tcl_Obj **attrObjPtrPtr));
typedef int (TclSetFileAttrProc) _ANSI_ARGS_((Tcl_Interp *interp,
	int objIndex, Tcl_Obj *fileName, Tcl_Obj *attrObjPtr));

typedef struct TclFileAttrProcs {
    TclGetFileAttrProc *getProc;	/* The procedure for getting attrs. */
    TclSetFileAttrProc *setProc;	/* The procedure for setting attrs. */
} TclFileAttrProcs;

/*
 * Opaque handle used in pipeline routines to encapsulate platform-dependent
 * state. 
 */

typedef struct TclFile_ *TclFile;
    
/*
 * Opaque names for platform specific types.
 */

typedef struct TclpTime_t_    *TclpTime_t;

/*
 * The "globParameters" argument of the function TclGlob is an
 * or'ed combination of the following values:
 */

#define TCL_GLOBMODE_NO_COMPLAIN      1
#define TCL_GLOBMODE_JOIN             2
#define TCL_GLOBMODE_DIR              4
#define TCL_GLOBMODE_TAILS            8

/*
 *----------------------------------------------------------------
 * Data structures related to obsolete filesystem hooks
 *----------------------------------------------------------------
 */

typedef int (TclStatProc_) _ANSI_ARGS_((CONST char *path, struct stat *buf));
typedef int (TclAccessProc_) _ANSI_ARGS_((CONST char *path, int mode));
typedef Tcl_Channel (TclOpenFileChannelProc_) _ANSI_ARGS_((Tcl_Interp *interp,
	CONST char *fileName, CONST char *modeString,
	int permissions));


/*
 *----------------------------------------------------------------
 * Data structures related to procedures
 *----------------------------------------------------------------
 */

typedef Tcl_CmdProc *TclCmdProcType;
typedef Tcl_ObjCmdProc *TclObjCmdProcType;

/*
 *----------------------------------------------------------------
 * Variables shared among Tcl modules but not used by the outside world.
 *----------------------------------------------------------------
 */

extern Tcl_Time			tclBlockTime;
extern int			tclBlockTimeSet;
extern char *			tclExecutableName;
extern char *			tclNativeExecutableName;
extern char *			tclDefaultEncodingDir;
extern Tcl_ChannelType		tclFileChannelType;
extern char *			tclMemDumpFileName;
extern TclPlatformType		tclPlatform;

/*
 * Variables denoting the Tcl object types defined in the core.
 */

extern Tcl_ObjType	tclBooleanType;
extern Tcl_ObjType	tclByteArrayType;
extern Tcl_ObjType	tclByteCodeType;
extern Tcl_ObjType	tclDoubleType;
extern Tcl_ObjType	tclEndOffsetType;
extern Tcl_ObjType	tclIntType;
extern Tcl_ObjType	tclListType;
extern Tcl_ObjType	tclProcBodyType;
extern Tcl_ObjType	tclStringType;
extern Tcl_ObjType	tclArraySearchType;
extern Tcl_ObjType	tclIndexType;
extern Tcl_ObjType	tclNsNameType;
#ifndef TCL_WIDE_INT_IS_LONG
extern Tcl_ObjType	tclWideIntType;
#endif

/*
 * Variables denoting the hash key types defined in the core.
 */

extern Tcl_HashKeyType tclArrayHashKeyType;
extern Tcl_HashKeyType tclOneWordHashKeyType;
extern Tcl_HashKeyType tclStringHashKeyType;
extern Tcl_HashKeyType tclObjHashKeyType;

/*
 * The head of the list of free Tcl objects, and the total number of Tcl
 * objects ever allocated and freed.
 */

extern Tcl_Obj *	tclFreeObjList;

#ifdef TCL_COMPILE_STATS
extern long		tclObjsAlloced;
extern long		tclObjsFreed;
#define TCL_MAX_SHARED_OBJ_STATS 5
extern long		tclObjsShared[TCL_MAX_SHARED_OBJ_STATS];
#endif /* TCL_COMPILE_STATS */

/*
 * Pointer to a heap-allocated string of length zero that the Tcl core uses
 * as the value of an empty string representation for an object. This value
 * is shared by all new objects allocated by Tcl_NewObj.
 */

extern char *		tclEmptyStringRep;
extern char		tclEmptyString;

/*
 *----------------------------------------------------------------
 * Procedures shared among Tcl modules but not used by the outside
 * world:
 *----------------------------------------------------------------
 */

EXTERN int		TclArraySet _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *arrayNameObj, Tcl_Obj *arrayElemObj));
EXTERN int		TclCheckBadOctal _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *value));
EXTERN void		TclExpandTokenArray _ANSI_ARGS_((
			    Tcl_Parse *parsePtr));
EXTERN int		TclFileAttrsCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
EXTERN int		TclFileCopyCmd _ANSI_ARGS_((Tcl_Interp *interp, 
			    int objc, Tcl_Obj *CONST objv[])) ;
EXTERN int		TclFileDeleteCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
EXTERN int		TclFileMakeDirsCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[])) ;
EXTERN int		TclFileRenameCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[])) ;
EXTERN void		TclFinalizeAllocSubsystem _ANSI_ARGS_((void));
EXTERN void		TclFinalizeCompExecEnv _ANSI_ARGS_((void));
EXTERN void		TclFinalizeCompilation _ANSI_ARGS_((void));
EXTERN void		TclFinalizeEncodingSubsystem _ANSI_ARGS_((void));
EXTERN void		TclFinalizeEnvironment _ANSI_ARGS_((void));
EXTERN void		TclFinalizeExecution _ANSI_ARGS_((void));
EXTERN void		TclFinalizeIOSubsystem _ANSI_ARGS_((void));
EXTERN void		TclFinalizeFilesystem _ANSI_ARGS_((void));
EXTERN void		TclResetFilesystem _ANSI_ARGS_((void));
EXTERN void		TclFinalizeLoad _ANSI_ARGS_((void));
EXTERN void		TclFinalizeMemorySubsystem _ANSI_ARGS_((void));
EXTERN void		TclFinalizeNotifier _ANSI_ARGS_((void));
EXTERN void		TclFinalizeAsync _ANSI_ARGS_((void));
EXTERN void		TclFinalizeSynchronization _ANSI_ARGS_((void));
EXTERN void		TclFinalizeThreadData _ANSI_ARGS_((void));
EXTERN void		TclFindEncodings _ANSI_ARGS_((CONST char *argv0));
EXTERN int		TclGlob _ANSI_ARGS_((Tcl_Interp *interp,
			    char *pattern, Tcl_Obj *unquotedPrefix, 
			    int globFlags, Tcl_GlobTypeData* types));
EXTERN void		TclInitAlloc _ANSI_ARGS_((void));
EXTERN void		TclInitDbCkalloc _ANSI_ARGS_((void));
EXTERN void		TclInitEncodingSubsystem _ANSI_ARGS_((void));
EXTERN void		TclInitIOSubsystem _ANSI_ARGS_((void));
EXTERN void		TclInitNamespaceSubsystem _ANSI_ARGS_((void));
EXTERN void		TclInitNotifier _ANSI_ARGS_((void));
EXTERN void		TclInitObjSubsystem _ANSI_ARGS_((void));
EXTERN void		TclInitSubsystems _ANSI_ARGS_((CONST char *argv0));
EXTERN int		TclIsLocalScalar _ANSI_ARGS_((CONST char *src,
			    int len));
EXTERN int              TclJoinThread _ANSI_ARGS_((Tcl_ThreadId id,
			    int* result));
EXTERN Tcl_Obj *	TclLindexList _ANSI_ARGS_((Tcl_Interp* interp,
						   Tcl_Obj* listPtr,
						   Tcl_Obj* argPtr ));
EXTERN Tcl_Obj *	TclLindexFlat _ANSI_ARGS_((Tcl_Interp* interp,
						   Tcl_Obj* listPtr,
						   int indexCount,
						   Tcl_Obj *CONST indexArray[]
						   ));
EXTERN Tcl_Obj *	TclLsetList _ANSI_ARGS_((Tcl_Interp* interp,
						 Tcl_Obj* listPtr,
						 Tcl_Obj* indexPtr,
						 Tcl_Obj* valuePtr  
						 ));
EXTERN Tcl_Obj *	TclLsetFlat _ANSI_ARGS_((Tcl_Interp* interp,
						 Tcl_Obj* listPtr,
						 int indexCount,
						 Tcl_Obj *CONST indexArray[],
						 Tcl_Obj* valuePtr
						 ));
EXTERN int              TclParseBackslash _ANSI_ARGS_((CONST char *src,
                            int numBytes, int *readPtr, char *dst));
EXTERN int		TclParseHex _ANSI_ARGS_((CONST char *src, int numBytes,
                            Tcl_UniChar *resultPtr));
EXTERN int		TclParseInteger _ANSI_ARGS_((CONST char *string,
			    int numBytes));
EXTERN int		TclParseWhiteSpace _ANSI_ARGS_((CONST char *src,
			    int numBytes, Tcl_Parse *parsePtr, char *typePtr));
EXTERN int		TclpObjAccess _ANSI_ARGS_((Tcl_Obj *filename,
			    int mode));
EXTERN int              TclpObjLstat _ANSI_ARGS_((Tcl_Obj *pathPtr, 
			    Tcl_StatBuf *buf));
EXTERN int		TclpCheckStackSpace _ANSI_ARGS_((void));
EXTERN Tcl_Obj*         TclpTempFileName _ANSI_ARGS_((void));
EXTERN Tcl_Obj*         TclNewFSPathObj _ANSI_ARGS_((Tcl_Obj *dirPtr, 
			    CONST char *addStrRep, int len));
EXTERN int              TclpDeleteFile _ANSI_ARGS_((CONST char *path));
EXTERN void		TclpFinalizeCondition _ANSI_ARGS_((
			    Tcl_Condition *condPtr));
EXTERN void		TclpFinalizeMutex _ANSI_ARGS_((Tcl_Mutex *mutexPtr));
EXTERN void		TclpFinalizeThreadData _ANSI_ARGS_((
			    Tcl_ThreadDataKey *keyPtr));
EXTERN void		TclpFinalizeThreadDataKey _ANSI_ARGS_((
			    Tcl_ThreadDataKey *keyPtr));
EXTERN char *		TclpFindExecutable _ANSI_ARGS_((
			    CONST char *argv0));
EXTERN int		TclpFindVariable _ANSI_ARGS_((CONST char *name,
			    int *lengthPtr));
EXTERN void		TclpInitLibraryPath _ANSI_ARGS_((CONST char *argv0));
EXTERN void		TclpInitLock _ANSI_ARGS_((void));
EXTERN void		TclpInitPlatform _ANSI_ARGS_((void));
EXTERN void		TclpInitUnlock _ANSI_ARGS_((void));
EXTERN int              TclpLoadFile _ANSI_ARGS_((Tcl_Interp *interp, 
				Tcl_Obj *pathPtr,
				CONST char *sym1, CONST char *sym2, 
				Tcl_PackageInitProc **proc1Ptr,
				Tcl_PackageInitProc **proc2Ptr, 
				ClientData *clientDataPtr,
				Tcl_FSUnloadFileProc **unloadProcPtr));
EXTERN Tcl_Obj*		TclpObjListVolumes _ANSI_ARGS_((void));
EXTERN void		TclpMasterLock _ANSI_ARGS_((void));
EXTERN void		TclpMasterUnlock _ANSI_ARGS_((void));
EXTERN int		TclpMatchFiles _ANSI_ARGS_((Tcl_Interp *interp,
			    char *separators, Tcl_DString *dirPtr,
			    char *pattern, char *tail));
EXTERN int              TclpObjNormalizePath _ANSI_ARGS_((Tcl_Interp *interp, 
			    Tcl_Obj *pathPtr, int nextCheckpoint));
EXTERN int		TclpObjCreateDirectory _ANSI_ARGS_((Tcl_Obj *pathPtr));
EXTERN void             TclpNativeJoinPath _ANSI_ARGS_((Tcl_Obj *prefix, 
							char *joining));
EXTERN Tcl_Obj*         TclpNativeSplitPath _ANSI_ARGS_((Tcl_Obj *pathPtr, 
							 int *lenPtr));
EXTERN Tcl_PathType     TclpGetNativePathType _ANSI_ARGS_((Tcl_Obj *pathObjPtr,
			    int *driveNameLengthPtr, Tcl_Obj **driveNameRef));
EXTERN int 		TclCrossFilesystemCopy _ANSI_ARGS_((Tcl_Interp *interp, 
			    Tcl_Obj *source, Tcl_Obj *target));
EXTERN int		TclpObjDeleteFile _ANSI_ARGS_((Tcl_Obj *pathPtr));
EXTERN int		TclpObjCopyDirectory _ANSI_ARGS_((Tcl_Obj *srcPathPtr, 
				Tcl_Obj *destPathPtr, Tcl_Obj **errorPtr));
EXTERN int		TclpObjCopyFile _ANSI_ARGS_((Tcl_Obj *srcPathPtr, 
				Tcl_Obj *destPathPtr));
EXTERN int		TclpObjRemoveDirectory _ANSI_ARGS_((Tcl_Obj *pathPtr, 
				int recursive, Tcl_Obj **errorPtr));
EXTERN int		TclpObjRenameFile _ANSI_ARGS_((Tcl_Obj *srcPathPtr, 
				Tcl_Obj *destPathPtr));
EXTERN int		TclpMatchInDirectory _ANSI_ARGS_((Tcl_Interp *interp, 
			        Tcl_Obj *resultPtr, Tcl_Obj *pathPtr, 
				CONST char *pattern, Tcl_GlobTypeData *types));
EXTERN Tcl_Obj*		TclpObjGetCwd _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN Tcl_Obj*		TclpObjLink _ANSI_ARGS_((Tcl_Obj *pathPtr, 
				Tcl_Obj *toPtr, int linkType));
EXTERN int		TclpObjChdir _ANSI_ARGS_((Tcl_Obj *pathPtr));
EXTERN Tcl_Obj*         TclFileDirname _ANSI_ARGS_((Tcl_Interp *interp, 
						    Tcl_Obj*pathPtr));
EXTERN int		TclpObjStat _ANSI_ARGS_((Tcl_Obj *pathPtr, Tcl_StatBuf *buf));
EXTERN Tcl_Channel	TclpOpenFileChannel _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *pathPtr, int mode,
			    int permissions));
EXTERN void		TclpCutFileChannel _ANSI_ARGS_((Tcl_Channel chan));
EXTERN void		TclpSpliceFileChannel _ANSI_ARGS_((Tcl_Channel chan));
EXTERN void		TclpPanic _ANSI_ARGS_(TCL_VARARGS(CONST char *,
			    format));
EXTERN char *		TclpReadlink _ANSI_ARGS_((CONST char *fileName,
			    Tcl_DString *linkPtr));
EXTERN void		TclpReleaseFile _ANSI_ARGS_((TclFile file));
EXTERN void		TclpSetVariables _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN void		TclpUnloadFile _ANSI_ARGS_((Tcl_LoadHandle loadHandle));
EXTERN VOID *		TclpThreadDataKeyGet _ANSI_ARGS_((
			    Tcl_ThreadDataKey *keyPtr));
EXTERN void		TclpThreadDataKeyInit _ANSI_ARGS_((
			    Tcl_ThreadDataKey *keyPtr));
EXTERN void		TclpThreadDataKeySet _ANSI_ARGS_((
			    Tcl_ThreadDataKey *keyPtr, VOID *data));
EXTERN void		TclpThreadExit _ANSI_ARGS_((int status));
EXTERN void		TclRememberCondition _ANSI_ARGS_((Tcl_Condition *mutex));
EXTERN void		TclRememberDataKey _ANSI_ARGS_((Tcl_ThreadDataKey *mutex));
EXTERN VOID             TclRememberJoinableThread _ANSI_ARGS_((Tcl_ThreadId id));
EXTERN void		TclRememberMutex _ANSI_ARGS_((Tcl_Mutex *mutex));
EXTERN VOID             TclSignalExitThread _ANSI_ARGS_((Tcl_ThreadId id,
			     int result));
EXTERN void		TclTransferResult _ANSI_ARGS_((Tcl_Interp *sourceInterp,
			    int result, Tcl_Interp *targetInterp));
EXTERN Tcl_Obj*         TclpNativeToNormalized 
                            _ANSI_ARGS_((ClientData clientData));
EXTERN Tcl_Obj*	        TclpFilesystemPathType
					_ANSI_ARGS_((Tcl_Obj* pathObjPtr));
EXTERN Tcl_PackageInitProc* TclpFindSymbol _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_LoadHandle loadHandle, CONST char *symbol));
EXTERN int              TclpDlopen _ANSI_ARGS_((Tcl_Interp *interp, 
			    Tcl_Obj *pathPtr, 
	                    Tcl_LoadHandle *loadHandle, 
		            Tcl_FSUnloadFileProc **unloadProcPtr));
EXTERN int              TclpUtime _ANSI_ARGS_((Tcl_Obj *pathPtr,
					       struct utimbuf *tval));

/*
 *----------------------------------------------------------------
 * Command procedures in the generic core:
 *----------------------------------------------------------------
 */

EXTERN int	Tcl_AfterObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_AppendObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ArrayObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_BinaryObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_BreakObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_CaseObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_CatchObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_CdObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ClockObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_CloseObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ConcatObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ContinueObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_EncodingObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_EofObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ErrorObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_EvalObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ExecObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ExitObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ExprObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_FblockedObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_FconfigureObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_FcopyObjCmd _ANSI_ARGS_((ClientData dummy,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_FileObjCmd _ANSI_ARGS_((ClientData dummy,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_FileEventObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_FlushObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ForObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ForeachObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_FormatObjCmd _ANSI_ARGS_((ClientData dummy,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_GetsObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_GlobalObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_GlobObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_IfObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_IncrObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_InfoObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_InterpObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_JoinObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LappendObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LindexObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LinsertObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LlengthObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ListObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LoadObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LrangeObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LreplaceObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LsearchObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LsetObjCmd _ANSI_ARGS_((ClientData clientData,
                    Tcl_Interp* interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_LsortObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_NamespaceObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_OpenObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_PackageObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_PidObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_PutsObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_PwdObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ReadObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_RegexpObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_RegsubObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_RenameObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ReturnObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ScanObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_SeekObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_SetObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_SplitObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_SocketObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_SourceObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_StringObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_SubstObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_SwitchObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_TellObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_TimeObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_TraceObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_UnsetObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_UpdateObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_UplevelObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_UpvarObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_VariableObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_VwaitObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_WhileObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));

/*
 *----------------------------------------------------------------
 * Command procedures found only in the Mac version of the core:
 *----------------------------------------------------------------
 */

#ifdef MAC_TCL
EXTERN int	Tcl_EchoCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, CONST84 char **argv));
EXTERN int	Tcl_LsObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_BeepObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_MacSourceObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tcl_ResourceObjCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
#endif

/*
 *----------------------------------------------------------------
 * Compilation procedures for commands in the generic core:
 *----------------------------------------------------------------
 */

EXTERN int	TclCompileAppendCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileBreakCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileCatchCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileContinueCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileExprCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileForCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileForeachCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileIfCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileIncrCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileLappendCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileLindexCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileListCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileLlengthCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileLsetCmd _ANSI_ARGS_((Tcl_Interp* interp,
		    Tcl_Parse* parsePtr, struct CompileEnv* envPtr));
EXTERN int	TclCompileRegexpCmd _ANSI_ARGS_((Tcl_Interp* interp,
		    Tcl_Parse* parsePtr, struct CompileEnv* envPtr));
EXTERN int	TclCompileReturnCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileSetCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileStringCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
EXTERN int	TclCompileWhileCmd _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));

/*
 * Functions defined in generic/tclVar.c and currenttly exported only 
 * for use by the bytecode compiler and engine. Some of these could later 
 * be placed in the public interface.
 */

EXTERN Var *	TclLookupArrayElement _ANSI_ARGS_((Tcl_Interp *interp,
		    CONST char *arrayName, CONST char *elName, CONST int flags,
		    CONST char *msg, CONST int createPart1,
		    CONST int createPart2, Var *arrayPtr));	
EXTERN Var *    TclObjLookupVar _ANSI_ARGS_((Tcl_Interp *interp,
		    Tcl_Obj *part1Ptr, CONST char *part2, int flags,
		    CONST char *msg, CONST int createPart1,
		    CONST int createPart2, Var **arrayPtrPtr));
EXTERN Tcl_Obj *TclPtrGetVar _ANSI_ARGS_((Tcl_Interp *interp, Var *varPtr,
		    Var *arrayPtr, CONST char *part1, CONST char *part2,
		    CONST int flags));
EXTERN Tcl_Obj *TclPtrSetVar _ANSI_ARGS_((Tcl_Interp *interp, Var *varPtr,
		    Var *arrayPtr, CONST char *part1, CONST char *part2,
		    Tcl_Obj *newValuePtr, CONST int flags));
EXTERN Tcl_Obj *TclPtrIncrVar _ANSI_ARGS_((Tcl_Interp *interp, Var *varPtr,
		    Var *arrayPtr, CONST char *part1, CONST char *part2,
		    CONST long i, CONST int flags));

/*
 *----------------------------------------------------------------
 * Macros used by the Tcl core to create and release Tcl objects.
 * TclNewObj(objPtr) creates a new object denoting an empty string.
 * TclDecrRefCount(objPtr) decrements the object's reference count,
 * and frees the object if its reference count is zero.
 * These macros are inline versions of Tcl_NewObj() and
 * Tcl_DecrRefCount(). Notice that the names differ in not having
 * a "_" after the "Tcl". Notice also that these macros reference
 * their argument more than once, so you should avoid calling them
 * with an expression that is expensive to compute or has
 * side effects. The ANSI C "prototypes" for these macros are:
 *
 * EXTERN void	TclNewObj _ANSI_ARGS_((Tcl_Obj *objPtr));
 * EXTERN void	TclDecrRefCount _ANSI_ARGS_((Tcl_Obj *objPtr));
 *
 * These macros are defined in terms of two macros that depend on 
 * memory allocator in use: TclAllocObjStorage, TclFreeObjStorage.
 * They are defined below.
 *----------------------------------------------------------------
 */

#ifdef TCL_COMPILE_STATS
#  define TclIncrObjsAllocated() \
    tclObjsAlloced++
#  define TclIncrObjsFreed() \
    tclObjsFreed++
#else
#  define TclIncrObjsAllocated()
#  define TclIncrObjsFreed()
#endif /* TCL_COMPILE_STATS */

#define TclNewObj(objPtr) \
    TclAllocObjStorage(objPtr); \
    TclIncrObjsAllocated(); \
    (objPtr)->refCount = 0; \
    (objPtr)->bytes    = tclEmptyStringRep; \
    (objPtr)->length   = 0; \
    (objPtr)->typePtr  = NULL

#define TclDecrRefCount(objPtr) \
    if (--(objPtr)->refCount <= 0) { \
	if (((objPtr)->typePtr != NULL) \
		&& ((objPtr)->typePtr->freeIntRepProc != NULL)) { \
	    (objPtr)->typePtr->freeIntRepProc(objPtr); \
	} \
	if (((objPtr)->bytes != NULL) \
		&& ((objPtr)->bytes != tclEmptyStringRep)) { \
	    ckfree((char *) (objPtr)->bytes); \
	} \
        TclFreeObjStorage(objPtr); \
	TclIncrObjsFreed(); \
    }

#ifdef TCL_MEM_DEBUG
#  define TclAllocObjStorage(objPtr) \
       (objPtr) = (Tcl_Obj *) \
           Tcl_DbCkalloc(sizeof(Tcl_Obj), __FILE__, __LINE__)

#  define TclFreeObjStorage(objPtr) \
       if ((objPtr)->refCount < -1) { \
           panic("Reference count for %lx was negative: %s line %d", \
	           (objPtr), __FILE__, __LINE__); \
       } \
       ckfree((char *) (objPtr))
     
#  define TclDbNewObj(objPtr, file, line) \
       (objPtr) = (Tcl_Obj *) Tcl_DbCkalloc(sizeof(Tcl_Obj), (file), (line)); \
       (objPtr)->refCount = 0; \
       (objPtr)->bytes    = tclEmptyStringRep; \
       (objPtr)->length   = 0; \
       (objPtr)->typePtr  = NULL; \
       TclIncrObjsAllocated()
     
#elif defined(PURIFY)

/*
 * The PURIFY mode is like the regular mode, but instead of doing block
 * Tcl_Obj allocation and keeping a freed list for efficiency, it always
 * allocates and frees a single Tcl_Obj so that tools like Purify can
 * better track memory leaks
 */

#  define TclAllocObjStorage(objPtr) \
       (objPtr) = (Tcl_Obj *) Tcl_Ckalloc(sizeof(Tcl_Obj))

#  define TclFreeObjStorage(objPtr) \
       ckfree((char *) (objPtr))

#elif defined(TCL_THREADS) && defined(USE_THREAD_ALLOC)

/*
 * The TCL_THREADS mode is like the regular mode but allocates Tcl_Obj's
 * from per-thread caches.
 */

EXTERN Tcl_Obj *TclThreadAllocObj _ANSI_ARGS_((void));
EXTERN void TclThreadFreeObj _ANSI_ARGS_((Tcl_Obj *));

#  define TclAllocObjStorage(objPtr) \
       (objPtr) = TclThreadAllocObj()

#  define TclFreeObjStorage(objPtr) \
       TclThreadFreeObj((objPtr))

#else /* not TCL_MEM_DEBUG */

#ifdef TCL_THREADS
/* declared in tclObj.c */
extern Tcl_Mutex tclObjMutex;
#endif

#  define TclAllocObjStorage(objPtr) \
       Tcl_MutexLock(&tclObjMutex); \
       if (tclFreeObjList == NULL) { \
	   TclAllocateFreeObjects(); \
       } \
       (objPtr) = tclFreeObjList; \
       tclFreeObjList = (Tcl_Obj *) \
	   tclFreeObjList->internalRep.otherValuePtr; \
       Tcl_MutexUnlock(&tclObjMutex)

#  define TclFreeObjStorage(objPtr) \
       Tcl_MutexLock(&tclObjMutex); \
       (objPtr)->internalRep.otherValuePtr = (VOID *) tclFreeObjList; \
       tclFreeObjList = (objPtr); \
       Tcl_MutexUnlock(&tclObjMutex)

#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to set a Tcl_Obj's string representation
 * to a copy of the "len" bytes starting at "bytePtr". This code
 * works even if the byte array contains NULLs as long as the length
 * is correct. Because "len" is referenced multiple times, it should
 * be as simple an expression as possible. The ANSI C "prototype" for
 * this macro is:
 *
 * EXTERN void	TclInitStringRep _ANSI_ARGS_((Tcl_Obj *objPtr,
 *		    char *bytePtr, int len));
 *----------------------------------------------------------------
 */

#define TclInitStringRep(objPtr, bytePtr, len) \
    if ((len) == 0) { \
	(objPtr)->bytes	 = tclEmptyStringRep; \
	(objPtr)->length = 0; \
    } else { \
	(objPtr)->bytes = (char *) ckalloc((unsigned) ((len) + 1)); \
	memcpy((VOID *) (objPtr)->bytes, (VOID *) (bytePtr), \
		(unsigned) (len)); \
	(objPtr)->bytes[len] = '\0'; \
	(objPtr)->length = (len); \
    }

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to get the string representation's
 * byte array pointer from a Tcl_Obj. This is an inline version
 * of Tcl_GetString(). The macro's expression result is the string
 * rep's byte pointer which might be NULL. The bytes referenced by 
 * this pointer must not be modified by the caller.
 * The ANSI C "prototype" for this macro is:
 *
 * EXTERN char *  TclGetString _ANSI_ARGS_((Tcl_Obj *objPtr));
 *----------------------------------------------------------------
 */

#define TclGetString(objPtr) \
    ((objPtr)->bytes? (objPtr)->bytes : Tcl_GetString((objPtr)))

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core get a unicode char from a utf string.
 * It checks to see if we have a one-byte utf char before calling
 * the real Tcl_UtfToUniChar, as this will save a lot of time for
 * primarily ascii string handling. The macro's expression result
 * is 1 for the 1-byte case or the result of Tcl_UtfToUniChar.
 * The ANSI C "prototype" for this macro is:
 *
 * EXTERN int TclUtfToUniChar _ANSI_ARGS_((CONST char *string,
 *					   Tcl_UniChar *ch));
 *----------------------------------------------------------------
 */

#define TclUtfToUniChar(str, chPtr) \
	((((unsigned char) *(str)) < 0xC0) ? \
	    ((*(chPtr) = (Tcl_UniChar) *(str)), 1) \
	    : Tcl_UtfToUniChar(str, chPtr))

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to compare Unicode strings.  On
 * big-endian systems we can use the more efficient memcmp, but
 * this would not be lexically correct on little-endian systems.
 * The ANSI C "prototype" for this macro is:
 *
 * EXTERN int TclUniCharNcmp _ANSI_ARGS_((CONST Tcl_UniChar *cs,
 *         CONST Tcl_UniChar *ct, unsigned long n));
 *----------------------------------------------------------------
 */
#ifdef WORDS_BIGENDIAN
#   define TclUniCharNcmp(cs,ct,n) memcmp((cs),(ct),(n)*sizeof(Tcl_UniChar))
#else /* !WORDS_BIGENDIAN */
#   define TclUniCharNcmp Tcl_UniCharNcmp
#endif /* WORDS_BIGENDIAN */

#include "tclIntDecls.h"

# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _TCLINT */

