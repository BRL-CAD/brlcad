/*
 * tclExecute.c --
 *
 *	This file contains procedures that execute byte-compiled Tcl commands.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 by Scriptics Corporation.
 * Copyright (c) 2001 by Kevin B. Kenny.  All rights reserved.
 * Copyright (c) 2002-2005 by Miguel Sofer.
 * Copyright (c) 2005 by Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclInt.h"
#include "tclCompile.h"
#include "tommath.h"

#include <math.h>
#include <float.h>

/*
 * Hack to determine whether we may expect IEEE floating point. The hack is
 * formally incorrect in that non-IEEE platforms might have the same precision
 * and range, but VAX, IBM, and Cray do not; are there any other floating
 * point units that we might care about?
 */

#if ( FLT_RADIX == 2 ) && ( DBL_MANT_DIG == 53 ) && ( DBL_MAX_EXP == 1024 )
#define IEEE_FLOATING_POINT
#endif

/*
 * The stuff below is a bit of a hack so that this file can be used in
 * environments that include no UNIX.
 * TODO: Does this serve any purpose anymore?
 */

#ifdef TCL_GENERIC_ONLY
#   ifndef NO_FLOAT_H
#	include <float.h>
#   else /* NO_FLOAT_H */
#	ifndef NO_VALUES_H
#	    include <values.h>
#	endif /* !NO_VALUES_H */
#   endif /* !NO_FLOAT_H */
#endif /* !TCL_GENERIC_ONLY */

/*
 * A mask (should be 2**n-1) that is used to work out when the bytecode engine
 * should call Tcl_AsyncReady() to see whether there is a signal that needs
 * handling.
 */

#ifndef ASYNC_CHECK_COUNT_MASK
#   define ASYNC_CHECK_COUNT_MASK	63
#endif /* !ASYNC_CHECK_COUNT_MASK */


/*
 * Boolean flag indicating whether the Tcl bytecode interpreter has been
 * initialized.
 */

static int execInitialized = 0;
TCL_DECLARE_MUTEX(execMutex)

#ifdef TCL_COMPILE_DEBUG
/*
 * Variable that controls whether execution tracing is enabled and, if so,
 * what level of tracing is desired:
 *    0: no execution tracing
 *    1: trace invocations of Tcl procs only
 *    2: trace invocations of all (not compiled away) commands
 *    3: display each instruction executed
 * This variable is linked to the Tcl variable "tcl_traceExec".
 */

int tclTraceExec = 0;
#endif

/*
 * Mapping from expression instruction opcodes to strings; used for error
 * messages. Note that these entries must match the order and number of the
 * expression opcodes (e.g., INST_LOR) in tclCompile.h.
 *
 * Does not include the string for INST_EXPON (and beyond), as that is
 * disjoint for backward-compatability reasons
 */

static CONST char *operatorStrings[] = {
    "||", "&&", "|", "^", "&", "==", "!=", "<", ">", "<=", ">=", "<<", ">>",
    "+", "-", "*", "/", "%", "+", "-", "~", "!",
    "BUILTIN FUNCTION", "FUNCTION",
    "", "", "", "", "", "", "", "", "eq", "ne"
};

/*
 * Mapping from Tcl result codes to strings; used for error and debugging
 * messages.
 */

#ifdef TCL_COMPILE_DEBUG
static char *resultStrings[] = {
    "TCL_OK", "TCL_ERROR", "TCL_RETURN", "TCL_BREAK", "TCL_CONTINUE"
};
#endif

/*
 * These are used by evalstats to monitor object usage in Tcl.
 */

#ifdef TCL_COMPILE_STATS
long		tclObjsAlloced = 0;
long		tclObjsFreed   = 0;
#define TCL_MAX_SHARED_OBJ_STATS 5
long		tclObjsShared[TCL_MAX_SHARED_OBJ_STATS] = { 0, 0, 0, 0, 0 };
#endif /* TCL_COMPILE_STATS */

/*
 * The new macro for ending an instruction; note that a reasonable C-optimiser
 * will resolve all branches at compile time. (result) is always a constant;
 * the macro NEXT_INST_F handles constant (nCleanup), NEXT_INST_V is resolved
 * at runtime for variable (nCleanup).
 *
 * ARGUMENTS:
 *    pcAdjustment: how much to increment pc
 *    nCleanup: how many objects to remove from the stack
 *    resultHandling: 0 indicates no object should be pushed on the stack;
 *	otherwise, push objResultPtr. If (result < 0), objResultPtr already
 *	has the correct reference count.
 */

#define NEXT_INST_F(pcAdjustment, nCleanup, resultHandling) \
    if (nCleanup == 0) {\
	if (resultHandling != 0) {\
	    if ((resultHandling) > 0) {\
		PUSH_OBJECT(objResultPtr);\
	    } else {\
		*(++tosPtr) = objResultPtr;\
	    }\
	} \
	pc += (pcAdjustment);\
	goto cleanup0;\
    } else if (resultHandling != 0) {\
	if ((resultHandling) > 0) {\
	    Tcl_IncrRefCount(objResultPtr);\
	}\
	pc += (pcAdjustment);\
	switch (nCleanup) {\
	    case 1: goto cleanup1_pushObjResultPtr;\
	    case 2: goto cleanup2_pushObjResultPtr;\
	    default: Tcl_Panic("bad usage of macro NEXT_INST_F");\
	}\
    } else {\
	pc += (pcAdjustment);\
	switch (nCleanup) {\
	    case 1: goto cleanup1;\
	    case 2: goto cleanup2;\
	    default: Tcl_Panic("bad usage of macro NEXT_INST_F");\
	}\
    }

#define NEXT_INST_V(pcAdjustment, nCleanup, resultHandling) \
    pc += (pcAdjustment);\
    cleanup = (nCleanup);\
    if (resultHandling) {\
	if ((resultHandling) > 0) {\
	    Tcl_IncrRefCount(objResultPtr);\
	}\
	goto cleanupV_pushObjResultPtr;\
    } else {\
	goto cleanupV;\
    }


/*
 * Macros used to cache often-referenced Tcl evaluation stack information
 * in local variables. Note that a DECACHE_STACK_INFO()-CACHE_STACK_INFO()
 * pair must surround any call inside TclExecuteByteCode (and a few other
 * procedures that use this scheme) that could result in a recursive call
 * to TclExecuteByteCode.
 */

#define CACHE_STACK_INFO() \
    tosPtr = eePtr->tosPtr

#define DECACHE_STACK_INFO() \
    eePtr->tosPtr = tosPtr;\
    checkInterp = 1


/*
 * Macros used to access items on the Tcl evaluation stack. PUSH_OBJECT
 * increments the object's ref count since it makes the stack have another
 * reference pointing to the object. However, POP_OBJECT does not decrement
 * the ref count. This is because the stack may hold the only reference to the
 * object, so the object would be destroyed if its ref count were decremented
 * before the caller had a chance to, e.g., store it in a variable. It is the
 * caller's responsibility to decrement the ref count when it is finished with
 * an object.
 *
 * WARNING! It is essential that objPtr only appear once in the PUSH_OBJECT
 * macro. The actual parameter might be an expression with side effects, and
 * this ensures that it will be executed only once.
 */

#define PUSH_OBJECT(objPtr) \
    Tcl_IncrRefCount(*(++tosPtr) = (objPtr))

#define POP_OBJECT() \
    *(tosPtr--)

/*
 * Macros used to trace instruction execution. The macros TRACE,
 * TRACE_WITH_OBJ, and O2S are only used inside TclExecuteByteCode.  O2S is
 * only used in TRACE* calls to get a string from an object.
 */

#ifdef TCL_COMPILE_DEBUG
#   define TRACE(a) \
    if (traceInstructions) { \
	fprintf(stdout, "%2d: %2d (%u) %s ", iPtr->numLevels, \
		(tosPtr - eePtr->stackPtr), \
		(unsigned int)(pc - codePtr->codeStart), \
		GetOpcodeName(pc)); \
	printf a; \
    }
#   define TRACE_APPEND(a) \
    if (traceInstructions) { \
	printf a; \
    }
#   define TRACE_WITH_OBJ(a, objPtr) \
    if (traceInstructions) { \
	fprintf(stdout, "%2d: %2d (%u) %s ", iPtr->numLevels, \
		(tosPtr - eePtr->stackPtr), \
		(unsigned int)(pc - codePtr->codeStart), \
		GetOpcodeName(pc)); \
	printf a; \
	TclPrintObject(stdout, objPtr, 30); \
	fprintf(stdout, "\n"); \
    }
#   define O2S(objPtr) \
    (objPtr ? TclGetString(objPtr) : "")
#else /* !TCL_COMPILE_DEBUG */
#   define TRACE(a)
#   define TRACE_APPEND(a)
#   define TRACE_WITH_OBJ(a, objPtr)
#   define O2S(objPtr)
#endif /* TCL_COMPILE_DEBUG */

/*
 * Macro used in this file to save a function call for common uses of
 * TclGetNumberFromObj(). The ANSI C "prototype" is:
 *
 * MODULE_SCOPE int GetNumberFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
 *			ClientData *ptrPtr, int *tPtr);
 */

#ifdef NO_WIDE_TYPE

#define GetNumberFromObj(interp, objPtr, ptrPtr, tPtr)			\
    (((objPtr)->typePtr == &tclIntType)					\
	?	(*(tPtr) = TCL_NUMBER_LONG,				\
		*(ptrPtr) = (ClientData)				\
		    (&((objPtr)->internalRep.longValue)), TCL_OK) :	\
    ((objPtr)->typePtr == &tclDoubleType)				\
	?	(((TclIsNaN((objPtr)->internalRep.doubleValue))		\
		    ?	(*(tPtr) = TCL_NUMBER_NAN)			\
		    :	(*(tPtr) = TCL_NUMBER_DOUBLE)),			\
		*(ptrPtr) = (ClientData)				\
		    (&((objPtr)->internalRep.doubleValue)), TCL_OK) :	\
    ((((objPtr)->typePtr == NULL) && ((objPtr)->bytes == NULL)) ||	\
    (((objPtr)->bytes != NULL) && ((objPtr)->length == 0)))		\
	? TCL_ERROR :							\
    TclGetNumberFromObj((interp), (objPtr), (ptrPtr), (tPtr)))

#else

#define GetNumberFromObj(interp, objPtr, ptrPtr, tPtr)			\
    (((objPtr)->typePtr == &tclIntType)					\
	?	(*(tPtr) = TCL_NUMBER_LONG,				\
		*(ptrPtr) = (ClientData)				\
		    (&((objPtr)->internalRep.longValue)), TCL_OK) :	\
    ((objPtr)->typePtr == &tclWideIntType)				\
	?	(*(tPtr) = TCL_NUMBER_WIDE,				\
		*(ptrPtr) = (ClientData)				\
		    (&((objPtr)->internalRep.wideValue)), TCL_OK) :	\
    ((objPtr)->typePtr == &tclDoubleType)				\
	?	(((TclIsNaN((objPtr)->internalRep.doubleValue))		\
		    ?	(*(tPtr) = TCL_NUMBER_NAN)			\
		    :	(*(tPtr) = TCL_NUMBER_DOUBLE)),			\
		*(ptrPtr) = (ClientData)				\
		    (&((objPtr)->internalRep.doubleValue)), TCL_OK) :	\
    ((((objPtr)->typePtr == NULL) && ((objPtr)->bytes == NULL)) ||	\
    (((objPtr)->bytes != NULL) && ((objPtr)->length == 0)))		\
	? TCL_ERROR :							\
    TclGetNumberFromObj((interp), (objPtr), (ptrPtr), (tPtr)))

#endif

/*
 * Macro used in this file to save a function call for common uses of
 * Tcl_GetBooleanFromObj().  The ANSI C "prototype" is:
 *
 * MODULE_SCOPE int TclGetBooleanFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
 *			int *boolPtr);
 */

#define TclGetBooleanFromObj(interp, objPtr, boolPtr)			\
    ((((objPtr)->typePtr == &tclIntType)				\
	|| ((objPtr)->typePtr == &tclBooleanType))			\
	? (*(boolPtr) = ((objPtr)->internalRep.longValue!=0), TCL_OK)	\
	: Tcl_GetBooleanFromObj((interp), (objPtr), (boolPtr)))

/*
 * Macro used in this file to save a function call for common uses of
 * Tcl_GetWideIntFromObj().  The ANSI C "prototype" is:
 *
 * MODULE_SCOPE int TclGetWideIntFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
 *			Tcl_WideInt *wideIntPtr);
 */

#ifdef NO_WIDE_TYPE
#define TclGetWideIntFromObj(interp, objPtr, wideIntPtr)		\
    (((objPtr)->typePtr == &tclIntType)					\
	? (*(wideIntPtr) = (Tcl_WideInt)				\
		((objPtr)->internalRep.longValue), TCL_OK) :		\
	Tcl_GetWideIntFromObj((interp), (objPtr), (wideIntPtr)))
#else
#define TclGetWideIntFromObj(interp, objPtr, wideIntPtr)		\
    (((objPtr)->typePtr == &tclWideIntType)				\
	? (*(wideIntPtr) = (objPtr)->internalRep.wideValue, TCL_OK) :	\
    ((objPtr)->typePtr == &tclIntType)					\
	? (*(wideIntPtr) = (Tcl_WideInt)				\
		((objPtr)->internalRep.longValue), TCL_OK) :		\
	Tcl_GetWideIntFromObj((interp), (objPtr), (wideIntPtr)))
#endif

static Tcl_ObjType dictIteratorType = {
    "dictIterator",
    NULL, NULL, NULL, NULL
};

/*
 * Declarations for local procedures to this file:
 */

static int		TclExecuteByteCode(Tcl_Interp *interp,
			    ByteCode *codePtr);
#ifdef TCL_COMPILE_STATS
static int		EvalStatsCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]);
#endif /* TCL_COMPILE_STATS */
#ifdef TCL_COMPILE_DEBUG
static char *		GetOpcodeName(unsigned char *pc);
#endif /* TCL_COMPILE_DEBUG */
static ExceptionRange *	GetExceptRangeForPc(unsigned char *pc,
			    int catchOnly, ByteCode* codePtr);
static char *		GetSrcInfoForPc(unsigned char *pc,
			    ByteCode* codePtr, int *lengthPtr);
static void		GrowEvaluationStack(ExecEnv *eePtr);
static void		IllegalExprOperandType(Tcl_Interp *interp,
			    unsigned char *pc, Tcl_Obj *opndPtr);
static void		InitByteCodeExecution(Tcl_Interp *interp);
#ifdef TCL_COMPILE_DEBUG
static void		PrintByteCodeInfo(ByteCode *codePtr);
static char *		StringForResultCode(int result);
static void		ValidatePcAndStackTop(ByteCode *codePtr,
			    unsigned char *pc, int stackTop,
			    int stackLowerBound, int checkStack);
#endif /* TCL_COMPILE_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * InitByteCodeExecution --
 *
 *	This procedure is called once to initialize the Tcl bytecode
 *	interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	This procedure initializes the array of instruction names. If
 *	compiling with the TCL_COMPILE_STATS flag, it initializes the array
 *	that counts the executions of each instruction and it creates the
 *	"evalstats" command. It also establishes the link between the Tcl
 *	"tcl_traceExec" and C "tclTraceExec" variables.
 *
 *----------------------------------------------------------------------
 */

static void
InitByteCodeExecution(
    Tcl_Interp *interp)		/* Interpreter for which the Tcl variable
				 * "tcl_traceExec" is linked to control
				 * instruction tracing. */
{
#ifdef TCL_COMPILE_DEBUG
    if (Tcl_LinkVar(interp, "tcl_traceExec", (char *) &tclTraceExec,
	    TCL_LINK_INT) != TCL_OK) {
	Tcl_Panic("InitByteCodeExecution: can't create link for tcl_traceExec variable");
    }
#endif
#ifdef TCL_COMPILE_STATS
    Tcl_CreateObjCommand(interp, "evalstats", EvalStatsCmd,
	    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
#endif /* TCL_COMPILE_STATS */
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateExecEnv --
 *
 *	This procedure creates a new execution environment for Tcl bytecode
 *	execution. An ExecEnv points to a Tcl evaluation stack. An ExecEnv is
 *	typically created once for each Tcl interpreter (Interp structure) and
 *	recursively passed to TclExecuteByteCode to execute ByteCode sequences
 *	for nested commands.
 *
 * Results:
 *	A newly allocated ExecEnv is returned. This points to an empty
 *	evaluation stack of the standard initial size.
 *
 * Side effects:
 *	The bytecode interpreter is also initialized here, as this procedure
 *	will be called before any call to TclExecuteByteCode.
 *
 *----------------------------------------------------------------------
 */

#define TCL_STACK_INITIAL_SIZE 2000

ExecEnv *
TclCreateExecEnv(
    Tcl_Interp *interp)		/* Interpreter for which the execution
				 * environment is being created. */
{
    ExecEnv *eePtr = (ExecEnv *) ckalloc(sizeof(ExecEnv));
    Tcl_Obj **stackPtr;

    stackPtr = (Tcl_Obj **)
	    ckalloc((size_t) (TCL_STACK_INITIAL_SIZE * sizeof(Tcl_Obj *)));

    /*
     * Use the bottom pointer to keep a reference count; the execution
     * environment holds a reference.
     */

    stackPtr++;
    eePtr->stackPtr = stackPtr;
    stackPtr[-1] = (Tcl_Obj *) ((char *) 1);

    eePtr->tosPtr = stackPtr - 1;
    eePtr->endPtr = stackPtr + (TCL_STACK_INITIAL_SIZE - 2);

    TclNewBooleanObj(eePtr->constants[0], 0);
    Tcl_IncrRefCount(eePtr->constants[0]);
    TclNewBooleanObj(eePtr->constants[1], 1);
    Tcl_IncrRefCount(eePtr->constants[1]);

    Tcl_MutexLock(&execMutex);
    if (!execInitialized) {
	TclInitAuxDataTypeTable();
	InitByteCodeExecution(interp);
	execInitialized = 1;
    }
    Tcl_MutexUnlock(&execMutex);

    return eePtr;
}
#undef TCL_STACK_INITIAL_SIZE

/*
 *----------------------------------------------------------------------
 *
 * TclDeleteExecEnv --
 *
 *	Frees the storage for an ExecEnv.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage for an ExecEnv and its contained storage (e.g. the evaluation
 *	stack) is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclDeleteExecEnv(
    ExecEnv *eePtr)		/* Execution environment to free. */
{
    if (eePtr->stackPtr[-1] == (Tcl_Obj *) ((char *) 1)) {
	ckfree((char *) (eePtr->stackPtr-1));
    } else {
	Tcl_Panic("freeing an execEnv whose stack is still in use");
    }
    TclDecrRefCount(eePtr->constants[0]);
    TclDecrRefCount(eePtr->constants[1]);
    ckfree((char *) eePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeExecution --
 *
 *	Finalizes the execution environment setup so that it can be later
 *	reinitialized.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	After this call, the next time TclCreateExecEnv will be called it will
 *	call InitByteCodeExecution.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeExecution(void)
{
    Tcl_MutexLock(&execMutex);
    execInitialized = 0;
    Tcl_MutexUnlock(&execMutex);
    TclFinalizeAuxDataTypeTable();
}

/*
 *----------------------------------------------------------------------
 *
 * GrowEvaluationStack --
 *
 *	This procedure grows a Tcl evaluation stack stored in an ExecEnv.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The size of the evaluation stack is doubled.
 *
 *----------------------------------------------------------------------
 */

static void
GrowEvaluationStack(
    register ExecEnv *eePtr)	/* Points to the ExecEnv with an evaluation
				 * stack to enlarge. */
{
    /*
     * The current Tcl stack elements are stored from *(eePtr->stackPtr) to
     * *(eePtr->endPtr) (inclusive).
     */

    int currElems = (eePtr->endPtr - eePtr->stackPtr + 1);
    int newElems  = 2*currElems;
    int currBytes = currElems * sizeof(Tcl_Obj *);
    int newBytes  = 2*currBytes;
    Tcl_Obj **newStackPtr = (Tcl_Obj **) ckalloc((unsigned) newBytes);
    Tcl_Obj **oldStackPtr = eePtr->stackPtr;

    /*
     * We keep the stack reference count as a (char *), as that works nicely
     * as a portable pointer-sized counter.
     */

    char *refCount = (char *) oldStackPtr[-1];

    /*
     * Copy the existing stack items to the new stack space, free the old
     * storage if appropriate, and record the refCount of the new stack held
     * by the environment.
     */

    newStackPtr++;
    memcpy((VOID *) newStackPtr, (VOID *) oldStackPtr,
	   (size_t) currBytes);

    if (refCount == (char *) 1) {
	ckfree((VOID *) (oldStackPtr-1));
    } else {
	/*
	 * Remove the reference corresponding to the environment pointer.
	 */

	oldStackPtr[-1] = (Tcl_Obj *) (refCount-1);
    }

    eePtr->stackPtr = newStackPtr;
    eePtr->endPtr = newStackPtr + (newElems - 2); /* index of last usable item */
    eePtr->tosPtr += (newStackPtr - oldStackPtr);
    newStackPtr[-1] = (Tcl_Obj *) ((char *) 1);
}

/*
 *--------------------------------------------------------------
 *
 * TclStackAlloc --
 *
 *	Allocate memory from the execution stack; it has to be returned later
 *	with a call to TclStackFree
 *
 * Results:
 *	A pointer to the first byte allocated, or panics if the allocation did
 *	not succeed.
 *
 * Side effects:
 *	The execution stack may be grown.
 *
 *--------------------------------------------------------------
 */

char *
TclStackAlloc(
    Tcl_Interp *interp,
    int numBytes)
{
    Interp *iPtr = (Interp *) interp;
    ExecEnv *eePtr = iPtr->execEnvPtr;
    int numWords;
    Tcl_Obj **tosPtr = eePtr->tosPtr;
    char **stackRefCountPtr;

    /*
     * Add two words to store
     *   - a pointer to the used execution stack
     *   - the number of words reserved
     * These will be used later by TclStackFree.
     */

    numWords = (numBytes + 3*sizeof(void *) - 1)/sizeof(void *);

    while ((tosPtr + numWords) > eePtr->endPtr) {
	GrowEvaluationStack(eePtr);
	tosPtr = eePtr->tosPtr;
    }

    /*
     * Increase the stack's reference count, to make sure it is not freed
     * prematurely.
     */

    stackRefCountPtr = (char **) (eePtr->stackPtr-1);
    ++*stackRefCountPtr;

    /*
     * Reserve the space in the exec stack, and store the data for freeing.
     */

    eePtr->tosPtr += numWords;
    *(eePtr->tosPtr-1) = (Tcl_Obj *) stackRefCountPtr;
    *(eePtr->tosPtr)   = (Tcl_Obj *) numWords;

    return (char *) (tosPtr+1);
}

void
TclStackFree(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;
    ExecEnv *eePtr = iPtr->execEnvPtr;
    char **stackRefCountPtr;

    stackRefCountPtr = (char **) *(eePtr->tosPtr-1);
    eePtr->tosPtr -= (int) *(eePtr->tosPtr);

    --*stackRefCountPtr;
    if (*stackRefCountPtr == (char *) 0) {
	ckfree((VOID *) stackRefCountPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_ExprObj --
 *
 *	Evaluate an expression in a Tcl_Obj.
 *
 * Results:
 *	A standard Tcl object result. If the result is other than TCL_OK, then
 *	the interpreter's result contains an error message. If the result is
 *	TCL_OK, then a pointer to the expression's result value object is
 *	stored in resultPtrPtr. In that case, the object's ref count is
 *	incremented to reflect the reference returned to the caller; the
 *	caller is then responsible for the resulting object and must, for
 *	example, decrement the ref count when it is finished with the object.
 *
 * Side effects:
 *	Any side effects caused by subcommands in the expression, if any. The
 *	interpreter result is not modified unless there is an error.
 *
 *--------------------------------------------------------------
 */

int
Tcl_ExprObj(
    Tcl_Interp *interp,		/* Context in which to evaluate the
				 * expression. */
    register Tcl_Obj *objPtr,	/* Points to Tcl object containing expression
				 * to evaluate. */
    Tcl_Obj **resultPtrPtr)	/* Where the Tcl_Obj* that is the expression
				 * result is stored if no errors occur. */
{
    Interp *iPtr = (Interp *) interp;
    CompileEnv compEnv;		/* Compilation environment structure allocated
				 * in frame. */
    LiteralTable *localTablePtr = &(compEnv.localLitTable);
    register ByteCode *codePtr = NULL;
    				/* Tcl Internal type of bytecode. Initialized
				 * to avoid compiler warning. */
    AuxData *auxDataPtr;
    LiteralEntry *entryPtr;
    Tcl_Obj *saveObjPtr, *resultPtr;
    char *string;
    int length, i, result;

    /*
     * First handle some common expressions specially.
     */

    string = Tcl_GetStringFromObj(objPtr, &length);
    if (length == 1) {
	if (*string == '0') {
	    TclNewBooleanObj(resultPtr, 0);
	    Tcl_IncrRefCount(resultPtr);
	    *resultPtrPtr = resultPtr;
	    return TCL_OK;
	} else if (*string == '1') {
	    TclNewBooleanObj(resultPtr, 1);
	    Tcl_IncrRefCount(resultPtr);
	    *resultPtrPtr = resultPtr;
	    return TCL_OK;
	}
    } else if ((length == 2) && (*string == '!')) {
	if (*(string+1) == '0') {
	    TclNewBooleanObj(resultPtr, 1);
	    Tcl_IncrRefCount(resultPtr);
	    *resultPtrPtr = resultPtr;
	    return TCL_OK;
	} else if (*(string+1) == '1') {
	    TclNewBooleanObj(resultPtr, 0);
	    Tcl_IncrRefCount(resultPtr);
	    *resultPtrPtr = resultPtr;
	    return TCL_OK;
	}
    }

    /*
     * Get the ByteCode from the object. If it exists, make sure it hasn't
     * been invalidated by, e.g., someone redefining a command with a compile
     * procedure (this might make the compiled code wrong). If necessary,
     * convert the object to be a ByteCode object and compile it.  Also, if
     * the code was compiled in/for a different interpreter, we recompile it.
     *
     * Precompiled expressions, however, are immutable and therefore they are
     * not recompiled, even if the epoch has changed.
     */

    if (objPtr->typePtr == &tclByteCodeType) {
	codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;
	if (((Interp *) *codePtr->interpHandle != iPtr)
		|| (codePtr->compileEpoch != iPtr->compileEpoch)) {
	    if (codePtr->flags & TCL_BYTECODE_PRECOMPILED) {
		if ((Interp *) *codePtr->interpHandle != iPtr) {
		    Tcl_Panic("Tcl_ExprObj: compiled expression jumped interps");
		}
		codePtr->compileEpoch = iPtr->compileEpoch;
	    } else {
		objPtr->typePtr->freeIntRepProc(objPtr);
		objPtr->typePtr = (Tcl_ObjType *) NULL;
	    }
	}
    }
    if (objPtr->typePtr != &tclByteCodeType) {
	TclInitCompileEnv(interp, &compEnv, string, length);
	result = TclCompileExpr(interp, string, length, &compEnv);

	/*
	 * Free the compilation environment's literal table bucket array if it
	 * was dynamically allocated.
	 */

	if (localTablePtr->buckets != localTablePtr->staticBuckets) {
	    ckfree((char *) localTablePtr->buckets);
	}

	if (result != TCL_OK) {
	    /*
	     * Compilation errors. Free storage allocated for compilation.
	     */

#ifdef TCL_COMPILE_DEBUG
	    TclVerifyLocalLiteralTable(&compEnv);
#endif /*TCL_COMPILE_DEBUG*/
	    entryPtr = compEnv.literalArrayPtr;
	    for (i = 0;  i < compEnv.literalArrayNext;  i++) {
		TclReleaseLiteral(interp, entryPtr->objPtr);
		entryPtr++;
	    }
#ifdef TCL_COMPILE_DEBUG
	    TclVerifyGlobalLiteralTable(iPtr);
#endif /*TCL_COMPILE_DEBUG*/

	    auxDataPtr = compEnv.auxDataArrayPtr;
	    for (i = 0;  i < compEnv.auxDataArrayNext;  i++) {
		if (auxDataPtr->type->freeProc != NULL) {
		    auxDataPtr->type->freeProc(auxDataPtr->clientData);
		}
		auxDataPtr++;
	    }
	    TclFreeCompileEnv(&compEnv);
	    return result;
	}

	/*
	 * Successful compilation. If the expression yielded no instructions,
	 * push an zero object as the expression's result.
	 */

	if (compEnv.codeNext == compEnv.codeStart) {
	    TclEmitPush(TclRegisterNewLiteral(&compEnv, "0", 1),
		    &compEnv);
	}

	/*
	 * Add a "done" instruction as the last instruction and change the
	 * object into a ByteCode object. Ownership of the literal objects and
	 * aux data items is given to the ByteCode object.
	 */

	TclEmitOpcode(INST_DONE, &compEnv);
	TclInitByteCodeObj(objPtr, &compEnv);
	TclFreeCompileEnv(&compEnv);
	codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;
#ifdef TCL_COMPILE_DEBUG
	if (tclTraceCompile == 2) {
	    TclPrintByteCodeObj(interp, objPtr);
	}
#endif /* TCL_COMPILE_DEBUG */
    }

    /*
     * Execute the expression after first saving the interpreter's result.
     */

    saveObjPtr = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(saveObjPtr);
    Tcl_ResetResult(interp);

    /*
     * Increment the code's ref count while it is being executed. If
     * afterwards no references to it remain, free the code.
     */

    codePtr->refCount++;
    result = TclExecuteByteCode(interp, codePtr);
    codePtr->refCount--;
    if (codePtr->refCount <= 0) {
	TclCleanupByteCode(codePtr);
	objPtr->typePtr = NULL;
	objPtr->internalRep.otherValuePtr = NULL;
    }

    /*
     * If the expression evaluated successfully, store a pointer to its value
     * object in resultPtrPtr then restore the old interpreter result. We
     * increment the object's ref count to reflect the reference that we are
     * returning to the caller. We also decrement the ref count of the
     * interpreter's result object after calling Tcl_SetResult since we next
     * store into that field directly.
     */

    if (result == TCL_OK) {
	*resultPtrPtr = iPtr->objResultPtr;
	Tcl_IncrRefCount(iPtr->objResultPtr);

	Tcl_SetObjResult(interp, saveObjPtr);
    }
    TclDecrRefCount(saveObjPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompEvalObj --
 *
 *	This procedure evaluates the script contained in a Tcl_Obj by first
 *	compiling it and then passing it to TclExecuteByteCode.
 *
 * Results:
 *	The return value is one of the return codes defined in tcl.h (such as
 *	TCL_OK), and interp->objResultPtr refers to a Tcl object that either
 *	contains the result of executing the code or an error message.
 *
 * Side effects:
 *	Almost certainly, depending on the ByteCode's instructions.
 *
 *----------------------------------------------------------------------
 */

int
TclCompEvalObj(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr)
{
    register Interp *iPtr = (Interp *) interp;
    register ByteCode* codePtr;		/* Tcl Internal type of bytecode. */
    int result;
    Namespace *namespacePtr;

    /*
     * Check that the interpreter is ready to execute scripts
     */

    iPtr->numLevels++;
    if (TclInterpReady(interp) == TCL_ERROR) {
	iPtr->numLevels--;
	return TCL_ERROR;
    }

    if (iPtr->varFramePtr != NULL) {
	namespacePtr = iPtr->varFramePtr->nsPtr;
    } else {
	namespacePtr = iPtr->globalNsPtr;
    }

    /*
     * If the object is not already of tclByteCodeType, compile it (and reset
     * the compilation flags in the interpreter; this should be done after any
     * compilation). Otherwise, check that it is "fresh" enough.
     */

    if (objPtr->typePtr != &tclByteCodeType) {
    recompileObj:
	iPtr->errorLine = 1;
	result = tclByteCodeType.setFromAnyProc(interp, objPtr);
	if (result != TCL_OK) {
	    iPtr->numLevels--;
	    return result;
	}
	codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;
    } else {
	/*
	 * Make sure the Bytecode hasn't been invalidated by, e.g., someone
	 * redefining a command with a compile procedure (this might make the
	 * compiled code wrong). The object needs to be recompiled if it was
	 * compiled in/for a different interpreter, or for a different
	 * namespace, or for the same namespace but with different name
	 * resolution rules. Precompiled objects, however, are immutable and
	 * therefore they are not recompiled, even if the epoch has changed.
	 *
	 * To be pedantically correct, we should also check that the
	 * originating procPtr is the same as the current context procPtr
	 * (assuming one exists at all - none for global level). This code is
	 * #def'ed out because [info body] was changed to never return a
	 * bytecode type object, which should obviate us from the extra checks
	 * here.
	 */

	codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;
	if (((Interp *) *codePtr->interpHandle != iPtr)
		|| (codePtr->compileEpoch != iPtr->compileEpoch)
#ifdef CHECK_PROC_ORIGINATION	/* [Bug: 3412 Pedantic] */
		|| (codePtr->procPtr != NULL && !(iPtr->varFramePtr &&
			iPtr->varFramePtr->procPtr == codePtr->procPtr))
#endif
		|| (codePtr->nsPtr != namespacePtr)
		|| (codePtr->nsEpoch != namespacePtr->resolverEpoch)) {
	    if (codePtr->flags & TCL_BYTECODE_PRECOMPILED) {
		if ((Interp *) *codePtr->interpHandle != iPtr) {
		    Tcl_Panic("Tcl_EvalObj: compiled script jumped interps");
		}
		codePtr->compileEpoch = iPtr->compileEpoch;
	    } else {
		/*
		 * This byteCode is invalid: free it and recompile
		 */
		objPtr->typePtr->freeIntRepProc(objPtr);
		goto recompileObj;
	    }
	}
    }

    /*
     * Increment the code's ref count while it is being executed. If
     * afterwards no references to it remain, free the code.
     */

    codePtr->refCount++;
    result = TclExecuteByteCode(interp, codePtr);
    codePtr->refCount--;
    if (codePtr->refCount <= 0) {
	TclCleanupByteCode(codePtr);
    }
    iPtr->numLevels--;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclIncrObj --
 *
 *	Increment an integeral value in a Tcl_Obj by an integeral value held
 *	in another Tcl_Obj. Caller is responsible for making sure we can
 *	update the first object.
 *
 * Results:
 *	TCL_ERROR if either object is non-integer, and TCL_OK otherwise. On
 *	error, an error message is left in the interpreter (if it is not NULL,
 *	of course).
 *
 * Side effects:
 *	valuePtr gets the new incrmented value.
 *
 *----------------------------------------------------------------------
 */

int
TclIncrObj(
    Tcl_Interp *interp,
    Tcl_Obj *valuePtr,
    Tcl_Obj *incrPtr)
{
    ClientData ptr1, ptr2;
    int type1, type2;
    mp_int value, incr;

    if (Tcl_IsShared(valuePtr)) {
	Tcl_Panic("%s called with shared object", "TclIncrObj");
    }

    if (GetNumberFromObj(NULL, valuePtr, &ptr1, &type1) != TCL_OK) {
	/* Produce error message (reparse?!) */
	return Tcl_GetIntFromObj(interp, valuePtr, &type1);
    }
    if (GetNumberFromObj(NULL, incrPtr, &ptr2, &type2) != TCL_OK) {
	/* Produce error message (reparse?!) */
	Tcl_GetIntFromObj(interp, incrPtr, &type1);
	Tcl_AddErrorInfo(interp, "\n    (reading increment)");
	return TCL_ERROR;
    }

    if ((type1 == TCL_NUMBER_LONG) && (type2 == TCL_NUMBER_LONG)) {
	long augend = *((CONST long *)ptr1);
	long addend = *((CONST long *)ptr2);
	long sum = augend + addend;
	/* Test for overflow */
	if ((augend >= 0 || addend >= 0 || sum < 0)
	    && (sum >= 0 || addend < 0 || augend < 0)) {
	    TclSetLongObj(valuePtr, sum);
	    return TCL_OK;
	}
#ifndef TCL_WIDE_INT_IS_LONG
	{
	    Tcl_WideInt w1 = (Tcl_WideInt)augend;
	    Tcl_WideInt w2 = (Tcl_WideInt)addend;
	    /* We know the sum value is outside the long range,
	     * so we use the macro form that doesn't range test again */
	    TclSetWideIntObj(valuePtr, w1 + w2);
	    return TCL_OK;
	}
#endif
    }

    if ((type1 == TCL_NUMBER_DOUBLE) || (type1 == TCL_NUMBER_NAN)) {
	/* Produce error message (reparse?!) */
	return Tcl_GetIntFromObj(interp, valuePtr, &type1);
    }
    if ((type1 == TCL_NUMBER_DOUBLE) || (type1 == TCL_NUMBER_NAN)) {
	/* Produce error message (reparse?!) */
	Tcl_GetIntFromObj(interp, incrPtr, &type1);
	Tcl_AddErrorInfo(interp, "\n    (reading increment)");
	return TCL_ERROR;
    }

#ifndef NO_WIDE_TYPE
    if ((type1 != TCL_NUMBER_BIG) && (type2 != TCL_NUMBER_BIG)) {
	Tcl_WideInt w1, w2, sum;
	TclGetWideIntFromObj(NULL, valuePtr, &w1);
	TclGetWideIntFromObj(NULL, incrPtr, &w2);
	sum = w1 + w2;
	/* Check for overflow */
	if ((w1 >= 0 || w2 >= 0 || sum < 0)
	    && (w1 < 0 || w2 < 0 || sum >= 0)) {
	    Tcl_SetWideIntObj(valuePtr, sum);
	    return TCL_OK;
	}
    }
#endif

    Tcl_GetBignumAndClearObj(interp, valuePtr, &value);
    Tcl_GetBignumFromObj(interp, incrPtr, &incr);
    mp_add(&value, &incr, &value);
    mp_clear(&incr);
    Tcl_SetBignumObj(valuePtr, &value);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclExecuteByteCode --
 *
 *	This procedure executes the instructions of a ByteCode structure. It
 *	returns when a "done" instruction is executed or an error occurs.
 *
 * Results:
 *	The return value is one of the return codes defined in tcl.h (such as
 *	TCL_OK), and interp->objResultPtr refers to a Tcl object that either
 *	contains the result of executing the code or an error message.
 *
 * Side effects:
 *	Almost certainly, depending on the ByteCode's instructions.
 *
 *----------------------------------------------------------------------
 */

static int
TclExecuteByteCode(
    Tcl_Interp *interp,		/* Token for command interpreter. */
    ByteCode *codePtr)		/* The bytecode sequence to interpret. */
{
    /*
     * Compiler cast directive - not a real variable.
     *	   Interp *iPtr = (Interp *) interp;
     */
#define iPtr ((Interp *) interp)

    /*
     * Constants: variables that do not change during the execution, used
     * sporadically.
     */

    ExecEnv *eePtr;		/* Points to the execution environment. */
    int initStackTop;		/* Stack top at start of execution. */
    int initCatchTop;		/* Catch stack top at start of execution. */
    Var *compiledLocals;
    Namespace *namespacePtr;

    /*
     * Globals: variables that store state, must remain valid at all times.
     */

    int catchTop;
    register Tcl_Obj **tosPtr;  /* Cached pointer to top of evaluation stack. */
    register unsigned char *pc = codePtr->codeStart;
				/* The current program counter. */
    int instructionCount = 0;	/* Counter that is used to work out when to
				 * call Tcl_AsyncReady() */
    Tcl_Obj *expandNestList = NULL;
    int checkInterp = 0;	/* Indicates when a check of interp readyness
				 * is necessary. Set by DECACHE_STACK_INFO() */

    /*
     * Transfer variables - needed only between opcodes, but not while
     * executing an instruction.
     */

    register int cleanup;
    Tcl_Obj *objResultPtr;

    /*
     * Result variable - needed only when going to checkForcatch or other
     * error handlers; also used as local in some opcodes.
     */

    int result = TCL_OK;	/* Return code returned after execution. */

    /*
     * Locals - variables that are used within opcodes or bounded sections of
     * the file (jumps between opcodes within a family).
     * NOTE: These are now defined locally where needed.
     */

#ifdef TCL_COMPILE_DEBUG
    int traceInstructions = (tclTraceExec == 3);
    char cmdNameBuf[21];
#endif

    /*
     * The execution uses a unified stack: first the catch stack, immediately
     * above it the execution stack.
     *
     * Make sure the catch stack is large enough to hold the maximum number of
     * catch commands that could ever be executing at the same time (this will
     * be no more than the exception range array's depth). Make sure the
     * execution stack is large enough to execute this ByteCode.
     */

    eePtr = iPtr->execEnvPtr;
    initCatchTop = eePtr->tosPtr - eePtr->stackPtr;
    catchTop = initCatchTop;
    tosPtr = eePtr->tosPtr + codePtr->maxExceptDepth;

    while ((tosPtr + codePtr->maxStackDepth) > eePtr->endPtr) {
	GrowEvaluationStack(eePtr);
	tosPtr = eePtr->tosPtr + codePtr->maxExceptDepth;
    }
    initStackTop = tosPtr - eePtr->stackPtr;

#ifdef TCL_COMPILE_DEBUG
    if (tclTraceExec >= 2) {
	PrintByteCodeInfo(codePtr);
	fprintf(stdout, "  Starting stack top=%d\n", initStackTop);
	fflush(stdout);
    }
#endif

#ifdef TCL_COMPILE_STATS
    iPtr->stats.numExecutions++;
#endif

    if (iPtr->varFramePtr != NULL) {
	namespacePtr = iPtr->varFramePtr->nsPtr;
	compiledLocals = iPtr->varFramePtr->compiledLocals;
    } else {
	namespacePtr = iPtr->globalNsPtr;
	compiledLocals = NULL;
    }

    /*
     * Loop executing instructions until a "done" instruction, a TCL_RETURN,
     * or some error.
     */

    goto cleanup0;

    /*
     * Targets for standard instruction endings; unrolled for speed in the
     * most frequent cases (instructions that consume up to two stack
     * elements).
     *
     * This used to be a "for(;;)" loop, with each instruction doing its own
     * cleanup.
     */

    {
	Tcl_Obj *valuePtr;

    cleanupV_pushObjResultPtr:
	switch (cleanup) {
	case 0:
	    *(++tosPtr) = (objResultPtr);
	    goto cleanup0;
	default:
	    cleanup -= 2;
	    while (cleanup--) {
		valuePtr = POP_OBJECT();
		TclDecrRefCount(valuePtr);
	    }
	case 2:
	cleanup2_pushObjResultPtr:
	    valuePtr = POP_OBJECT();
	    TclDecrRefCount(valuePtr);
	case 1:
	cleanup1_pushObjResultPtr:
	    valuePtr = *tosPtr;
	    TclDecrRefCount(valuePtr);
	}
	*tosPtr = objResultPtr;
	goto cleanup0;

    cleanupV:
	switch (cleanup) {
	default:
	    cleanup -= 2;
	    while (cleanup--) {
		valuePtr = POP_OBJECT();
		TclDecrRefCount(valuePtr);
	    }
	case 2:
	cleanup2:
	    valuePtr = POP_OBJECT();
	    TclDecrRefCount(valuePtr);
	case 1:
	cleanup1:
	    valuePtr = POP_OBJECT();
	    TclDecrRefCount(valuePtr);
	case 0:
	    /*
	     * We really want to do nothing now, but this is needed for some
	     * compilers (SunPro CC)
	     */
	    break;
	}
    }
 cleanup0:

#ifdef TCL_COMPILE_DEBUG
    /*
     * Skip the stack depth check if an expansion is in progress
     */

    ValidatePcAndStackTop(codePtr, pc, (tosPtr - eePtr->stackPtr),
	    initStackTop, /*checkStack*/ (expandNestList == NULL));
    if (traceInstructions) {
	fprintf(stdout, "%2d: %2d ", iPtr->numLevels, (tosPtr - eePtr->stackPtr));
	TclPrintInstruction(codePtr, pc);
	fflush(stdout);
    }
#endif /* TCL_COMPILE_DEBUG */

#ifdef TCL_COMPILE_STATS
    iPtr->stats.instructionCount[*pc]++;
#endif

    /*
     * Check for asynchronous handlers [Bug 746722]; we do the check every
     * ASYNC_CHECK_COUNT_MASK instruction, of the form (2**n-1).
     */

    if ((instructionCount++ & ASYNC_CHECK_COUNT_MASK) == 0) {
	if (Tcl_AsyncReady()) {
	    int localResult;

	    DECACHE_STACK_INFO();
	    localResult = Tcl_AsyncInvoke(interp, result);
	    CACHE_STACK_INFO();
	    if (localResult == TCL_ERROR) {
		result = localResult;
		goto checkForCatch;
	    }
	}
	if (Tcl_LimitReady(interp)) {
	    int localResult;

	    DECACHE_STACK_INFO();
	    localResult = Tcl_LimitCheck(interp);
	    CACHE_STACK_INFO();
	    if (localResult == TCL_ERROR) {
		result = localResult;
		goto checkForCatch;
	    }
	}
    }

    switch (*pc) {
    case INST_RETURN_IMM: {
	int code = TclGetInt4AtPtr(pc+1);
	int level = TclGetUInt4AtPtr(pc+5);
	Tcl_Obj *returnOpts;

	TRACE(("%u %u => ", code, level));
	returnOpts = POP_OBJECT();
	result = TclProcessReturn(interp, code, level, returnOpts);
	Tcl_DecrRefCount(returnOpts);
	if (result != TCL_OK) {
	    Tcl_SetObjResult(interp, *tosPtr);
	    cleanup = 1;
	    goto processExceptionReturn;
	}
	TRACE_APPEND(("continuing to next instruction (result=\"%.30s\")",
		O2S(objResultPtr)));
	NEXT_INST_F(9, 0, 0);
    }

    case INST_RETURN_STK:
	TRACE(("=> "));
	objResultPtr = POP_OBJECT();
	result = Tcl_SetReturnOptions(interp, POP_OBJECT());
	if (result != TCL_OK) {
	    Tcl_SetObjResult(interp, objResultPtr);
	    Tcl_DecrRefCount(objResultPtr);
	    cleanup = 0;
	    goto processExceptionReturn;
	}
	TRACE_APPEND(("continuing to next instruction (result=\"%.30s\")",
		O2S(objResultPtr)));
	NEXT_INST_F(1, 0, -1);

    case INST_DONE:
	if (tosPtr <= eePtr->stackPtr + initStackTop) {
	    tosPtr--;
	    goto abnormalReturn;
	}

	/*
	 * Set the interpreter's object result to point to the topmost object
	 * from the stack, and check for a possible [catch]. The stackTop's
	 * level and refCount will be handled by "processCatch" or
	 * "abnormalReturn".
	 */

	Tcl_SetObjResult(interp, *tosPtr);
#ifdef TCL_COMPILE_DEBUG
	TRACE_WITH_OBJ(("=> return code=%d, result=", result),
		iPtr->objResultPtr);
	if (traceInstructions) {
	    fprintf(stdout, "\n");
	}
#endif
	goto checkForCatch;

    case INST_PUSH1:
#if !TCL_COMPILE_DEBUG
	instPush1Peephole:
#endif
	PUSH_OBJECT(codePtr->objArrayPtr[TclGetUInt1AtPtr(pc+1)]);
	TRACE_WITH_OBJ(("%u => ", TclGetInt1AtPtr(pc+1)), *tosPtr);
	pc += 2;
#if !TCL_COMPILE_DEBUG
	/*
	 * Runtime peephole optimisation: check if we are pushing again.
	 */

	if (*pc == INST_PUSH1) {
	    goto instPush1Peephole;
	}
#endif
	NEXT_INST_F(0, 0, 0);

    case INST_PUSH4:
	objResultPtr = codePtr->objArrayPtr[TclGetUInt4AtPtr(pc+1)];
	TRACE_WITH_OBJ(("%u => ", TclGetUInt4AtPtr(pc+1)), objResultPtr);
	NEXT_INST_F(5, 0, 1);

    case INST_POP: {
	Tcl_Obj *valuePtr;

	TRACE_WITH_OBJ(("=> discarding "), *tosPtr);
	valuePtr = POP_OBJECT();
	TclDecrRefCount(valuePtr);

	/*
	 * Runtime peephole optimisation: an INST_POP is scheduled at the end
	 * of most commands. If the next instruction is an INST_START_CMD,
	 * fall through to it.
	 */

	pc++;
#if !TCL_COMPILE_DEBUG
	if (*pc == INST_START_CMD) {
	    goto instStartCmdPeephole;
	}
#endif
	NEXT_INST_F(0, 0, 0);
    }

    case INST_START_CMD:
#if !TCL_COMPILE_DEBUG
    instStartCmdPeephole:
#endif
	/*
	 * Remark that if the interpreter is marked for deletion its
	 * compileEpoch is modified, so that the epoch check also verifies
	 * that the interp is not deleted.  If no outside call has been made
	 * since the last check, it is safe to omit the check.
	 */

	iPtr->cmdCount++;
	if (!checkInterp ||
		(((codePtr->compileEpoch == iPtr->compileEpoch)
			&& (codePtr->nsEpoch == namespacePtr->resolverEpoch))
			|| (codePtr->flags & TCL_BYTECODE_PRECOMPILED))) {
#if !TCL_COMPILE_DEBUG
	    /*
	     * Peephole optimisations: check if there are several
	     * INST_START_CMD in a row. Many commands start by pushing a
	     * literal argument or command name; optimise that case too.
	     */

	    while (*(pc += 5) == INST_START_CMD) {
		iPtr->cmdCount++;
	    }
	    if (*pc == INST_PUSH1) {
		goto instPush1Peephole;
	    }
	    NEXT_INST_F(0, 0, 0);
#else
	    NEXT_INST_F(5, 0, 0);
#endif
	} else {
	    char *bytes;
	    int length, opnd;
	    Tcl_Obj *newObjResultPtr;

	    bytes = GetSrcInfoForPc(pc, codePtr, &length);
	    DECACHE_STACK_INFO();
	    result = Tcl_EvalEx(interp, bytes, length, 0);
	    CACHE_STACK_INFO();
	    if (result != TCL_OK) {
		cleanup = 0;
		goto processExceptionReturn;
	    }
	    opnd = TclGetUInt4AtPtr(pc+1);
	    objResultPtr = Tcl_GetObjResult(interp);
	    TclNewObj(newObjResultPtr);
	    Tcl_IncrRefCount(newObjResultPtr);
	    iPtr->objResultPtr = newObjResultPtr;
	    NEXT_INST_V(opnd, 0, -1);
	}

    case INST_DUP:
	objResultPtr = *tosPtr;
	TRACE_WITH_OBJ(("=> "), objResultPtr);
	NEXT_INST_F(1, 0, 1);

    case INST_OVER: {
	int opnd;

	opnd = TclGetUInt4AtPtr(pc+1);
	objResultPtr = *(tosPtr - opnd);
	TRACE_WITH_OBJ(("=> "), objResultPtr);
	NEXT_INST_F(5, 0, 1);
    }

    case INST_CONCAT1: {
	int opnd, length, appendLen = 0;
	char *bytes, *p;
	Tcl_Obj **currPtr;

	opnd = TclGetUInt1AtPtr(pc+1);

	/*
	 * Compute the length to be appended.
	 */

	for (currPtr = tosPtr - (opnd-2); currPtr <= tosPtr; currPtr++) {
	    bytes = Tcl_GetStringFromObj(*currPtr, &length);
	    if (bytes != NULL) {
		appendLen += length;
	    }
	}

	/*
	 * If nothing is to be appended, just return the first object by
	 * dropping all the others from the stack; this saves both the
	 * computation and copy of the string rep of the first object,
	 * enabling the fast '$x[set x {}]' idiom for 'K $x [set x{}]'.
	 */

	if (appendLen == 0) {
	    TRACE_WITH_OBJ(("%u => ", opnd), objResultPtr);
	    NEXT_INST_V(2, (opnd-1), 0);
	}

	/*
	 * If the first object is shared, we need a new obj for the result;
	 * otherwise, we can reuse the first object. In any case, make sure it
	 * has enough room to accomodate all the concatenated bytes. Note that
	 * if it is unshared its bytes are already copied by
	 * Tcl_SetObjectLength, so that we set the loop parameters to avoid
	 * copying them again: p points to the end of the already copied
	 * bytes, currPtr to the second object.
	 */

	objResultPtr = *(tosPtr-(opnd-1));
	bytes = Tcl_GetStringFromObj(objResultPtr, &length);
#if !TCL_COMPILE_DEBUG
	if (!Tcl_IsShared(objResultPtr)) {
	    Tcl_SetObjLength(objResultPtr, (length + appendLen));
	    p = TclGetString(objResultPtr) + length;
	    currPtr = tosPtr - (opnd - 2);
	} else {
#endif
	    p = (char *) ckalloc((unsigned) (length + appendLen + 1));
	    TclNewObj(objResultPtr);
	    objResultPtr->bytes = p;
	    objResultPtr->length = length + appendLen;
	    currPtr = tosPtr - (opnd - 1);
#if !TCL_COMPILE_DEBUG
	}
#endif

	/*
	 * Append the remaining characters.
	 */

	for (; currPtr <= tosPtr; currPtr++) {
	    bytes = Tcl_GetStringFromObj(*currPtr, &length);
	    if (bytes != NULL) {
		memcpy((VOID *) p, (VOID *) bytes, (size_t) length);
		p += length;
	    }
	}
	*p = '\0';

	TRACE_WITH_OBJ(("%u => ", opnd), objResultPtr);
	NEXT_INST_V(2, opnd, 1);
    }

    case INST_EXPAND_START: {
	/*
	 * Push an element to the expandNestList. This records the current
	 * tosPtr - i.e., the point in the stack where the expanded command
	 * starts.
	 *
	 * Use a Tcl_Obj as linked list element; slight mem waste, but faster
	 * allocation than ckalloc. This also abuses the Tcl_Obj structure, as
	 * we do not define a special tclObjType for it. It is not dangerous
	 * as the obj is never passed anywhere, so that all manipulations are
	 * performed here and in INST_INVOKE_EXPANDED (in case of an expansion
	 * error, also in INST_EXPAND_STKTOP).
	 */

	Tcl_Obj *objPtr;

	TclNewObj(objPtr);
	objPtr->internalRep.twoPtrValue.ptr1 = (VOID *)
		(tosPtr - eePtr->stackPtr);
	objPtr->internalRep.twoPtrValue.ptr2 = (VOID *) expandNestList;
	expandNestList = objPtr;
	NEXT_INST_F(1, 0, 0);
    }

    case INST_EXPAND_STKTOP: {
	int objc, length, i;
	Tcl_Obj **objv, *valuePtr, *objPtr;

	/*
	 * Make sure that the element at stackTop is a list; if not, remove
	 * the element from the expand link list and leave.
	 */

	valuePtr = *tosPtr;
	if (Tcl_ListObjGetElements(interp, valuePtr, &objc, &objv) != TCL_OK) {
	    result = TCL_ERROR;
	    TRACE_WITH_OBJ(("%.30s => ERROR: ", O2S(valuePtr)),
		    Tcl_GetObjResult(interp));
	    objPtr = expandNestList;
	    expandNestList = (Tcl_Obj *) objPtr->internalRep.twoPtrValue.ptr2;
	    TclDecrRefCount(objPtr);
	    goto checkForCatch;
	}
	tosPtr--;

	/*
	 * Make sure there is enough room in the stack to expand this list
	 * *and* process the rest of the command (at least up to the next
	 * argument expansion or command end).  The operand is the current
	 * stack depth, as seen by the compiler.
	 */

	length = objc + codePtr->maxStackDepth - TclGetInt4AtPtr(pc+1);
	while ((tosPtr + length) > eePtr->endPtr) {
	    DECACHE_STACK_INFO();
	    GrowEvaluationStack(eePtr);
	    CACHE_STACK_INFO();
	}

	/*
	 * Expand the list at stacktop onto the stack; free the list.
	 */

	for (i = 0; i < objc; i++) {
	    PUSH_OBJECT(objv[i]);
	}
	TclDecrRefCount(valuePtr);
	NEXT_INST_F(5, 0, 0);
    }

    {
	/*
	 * INVOCATION BLOCK
	 */

	int objc, pcAdjustment;

    case INST_INVOKE_EXPANDED:
	{
	    Tcl_Obj *objPtr;

	    objPtr = expandNestList;
	    expandNestList = (Tcl_Obj *) objPtr->internalRep.twoPtrValue.ptr2;
	    objc = tosPtr - eePtr->stackPtr
		    - (ptrdiff_t) objPtr->internalRep.twoPtrValue.ptr1;
	    TclDecrRefCount(objPtr);
	}

	if (objc == 0) {
	    /*
	     * Nothing was expanded, return {}.
	     */

	    TclNewObj(objResultPtr);
	    NEXT_INST_F(1, 0, 1);
	}

	pcAdjustment = 1;
	goto doInvocation;

    case INST_INVOKE_STK4:
	objc = TclGetUInt4AtPtr(pc+1);
	pcAdjustment = 5;
	goto doInvocation;

    case INST_INVOKE_STK1:
	objc = TclGetUInt1AtPtr(pc+1);
	pcAdjustment = 2;

    doInvocation:
	{
	    Tcl_Obj **objv = (tosPtr - (objc-1));
	    int length;
	    char *bytes;

	    /*
	     * We keep the stack reference count as a (char *), as that works
	     * nicely as a portable pointer-sized counter.
	     */

	    char **preservedStackRefCountPtr;

#ifdef TCL_COMPILE_DEBUG
	    if (tclTraceExec >= 2) {
		int i;

		if (traceInstructions) {
		    strncpy(cmdNameBuf, TclGetString(objv[0]), 20);
		    TRACE(("%u => call ", objc));
		} else {
		    fprintf(stdout, "%d: (%u) invoking ", iPtr->numLevels,
			    (unsigned int)(pc - codePtr->codeStart));
		}
		for (i = 0;  i < objc;  i++) {
		    TclPrintObject(stdout, objv[i], 15);
		    fprintf(stdout, " ");
		}
		fprintf(stdout, "\n");
		fflush(stdout);
	    }
#endif /*TCL_COMPILE_DEBUG*/

	    /*
	     * If trace procedures will be called, we need a command string to
	     * pass to TclEvalObjvInternal; note that a copy of the string
	     * will be made there to include the ending \0.
	     */

	    bytes = NULL;
	    length = 0;
	    if (iPtr->tracePtr != NULL) {
		Trace *tracePtr, *nextTracePtr;

		for (tracePtr = iPtr->tracePtr;  tracePtr != NULL;
			tracePtr = nextTracePtr) {
		    nextTracePtr = tracePtr->nextPtr;
		    if (tracePtr->level == 0 ||
			    iPtr->numLevels <= tracePtr->level) {
			/*
			 * Traces will be called: get command string
			 */

			bytes = GetSrcInfoForPc(pc, codePtr, &length);
			break;
		    }
		}
	    } else {
		Command *cmdPtr;

		cmdPtr = (Command *) Tcl_GetCommandFromObj(interp, objv[0]);
		if ((cmdPtr!=NULL) && (cmdPtr->flags & CMD_HAS_EXEC_TRACES)) {
		    bytes = GetSrcInfoForPc(pc, codePtr, &length);
		}
	    }

	    /*
	     * A reference to part of the stack vector itself escapes our
	     * control: increase its refCount to stop it from being
	     * deallocated by a recursive call to ourselves.  The extra
	     * variable is needed because all others are liable to change due
	     * to the trace procedures.
	     */

	    preservedStackRefCountPtr = (char **) (eePtr->stackPtr-1);
	    ++*preservedStackRefCountPtr;

	    /*
	     * Reset the instructionCount variable, since we're about to check
	     * for async stuff anyway while processing TclEvalObjvInternal.
	     */

	    instructionCount = 1;

	    /*
	     * Finally, let TclEvalObjvInternal handle the command.
	     */

	    DECACHE_STACK_INFO();
	    /*Tcl_ResetResult(interp);*/
	    result = TclEvalObjvInternal(interp, objc, objv, bytes, length, 0);
	    CACHE_STACK_INFO();

	    /*
	     * If the old stack is going to be released, it is safe to do so
	     * now, since no references to objv are going to be used from now
	     * on.
	     */

	    --*preservedStackRefCountPtr;
	    if (*preservedStackRefCountPtr == (char *) 0) {
		ckfree((VOID *) preservedStackRefCountPtr);
	    }

	    if (result == TCL_OK) {
		Tcl_Obj *objPtr;
		/*
		 * Push the call's object result and continue execution with
		 * the next instruction.
		 */

		TRACE_WITH_OBJ(("%u => ... after \"%.20s\": TCL_OK, result=",
			objc, cmdNameBuf), Tcl_GetObjResult(interp));

		objResultPtr = Tcl_GetObjResult(interp);

		/*
		 * Reset the interp's result to avoid possible duplications of
		 * large objects [Bug 781585]. We do not call Tcl_ResetResult
		 * to avoid any side effects caused by the resetting of
		 * errorInfo and errorCode [Bug 804681], which are not needed
		 * here. We chose instead to manipulate the interp's object
		 * result directly.
		 *
		 * Note that the result object is now in objResultPtr, it
		 * keeps the refCount it had in its role of
		 * iPtr->objResultPtr.
		 */

		TclNewObj(objPtr);
		Tcl_IncrRefCount(objPtr);
		iPtr->objResultPtr = objPtr;
		NEXT_INST_V(pcAdjustment, objc, -1);
	    } else {
		cleanup = objc;
		goto processExceptionReturn;
	    }
	}
    }

    case INST_EVAL_STK: {
	/*
	 * Note to maintainers: it is important that INST_EVAL_STK pop its
	 * argument from the stack before jumping to checkForCatch! DO NOT
	 * OPTIMISE!
	 */

	Tcl_Obj *objPtr;

	objPtr = *tosPtr;
	DECACHE_STACK_INFO();
	result = TclCompEvalObj(interp, objPtr);
	CACHE_STACK_INFO();
	if (result == TCL_OK) {
	    /*
	     * Normal return; push the eval's object result.
	     */

	    objResultPtr = Tcl_GetObjResult(interp);
	    TRACE_WITH_OBJ(("\"%.30s\" => ", O2S(objPtr)),
		    Tcl_GetObjResult(interp));

	    /*
	     * Reset the interp's result to avoid possible duplications of
	     * large objects [Bug 781585]. We do not call Tcl_ResetResult to
	     * avoid any side effects caused by the resetting of errorInfo and
	     * errorCode [Bug 804681], which are not needed here. We chose
	     * instead to manipulate the interp's object result directly.
	     *
	     * Note that the result object is now in objResultPtr, it keeps
	     * the refCount it had in its role of iPtr->objResultPtr.
	     */

	    TclNewObj(objPtr);
	    Tcl_IncrRefCount(objPtr);
	    iPtr->objResultPtr = objPtr;
	    NEXT_INST_F(1, 1, -1);
	} else {
	    cleanup = 1;
	    goto processExceptionReturn;
	}
    }

    case INST_EXPR_STK: {
	Tcl_Obj *objPtr, *valuePtr;

	objPtr = *tosPtr;
	DECACHE_STACK_INFO();
	/*Tcl_ResetResult(interp);*/
	result = Tcl_ExprObj(interp, objPtr, &valuePtr);
	CACHE_STACK_INFO();
	if (result != TCL_OK) {
	    TRACE_WITH_OBJ(("\"%.30s\" => ERROR: ", O2S(objPtr)),
		    Tcl_GetObjResult(interp));
	    goto checkForCatch;
	}
	objResultPtr = valuePtr;
	TRACE_WITH_OBJ(("\"%.30s\" => ", O2S(objPtr)), valuePtr);
	NEXT_INST_F(1, 1, -1); /* already has right refct */
    }

    /*
     * ---------------------------------------------------------
     *	   Start of INST_LOAD instructions.
     *
     * WARNING: more 'goto' here than your doctor recommended!  The different
     * instructions set the value of some variables and then jump to somme
     * common execution code.
     */
    {
	int opnd, pcAdjustment;
	char *part1, *part2;
	Var *varPtr, *arrayPtr;
	Tcl_Obj *objPtr;

    case INST_LOAD_SCALAR1:
	opnd = TclGetUInt1AtPtr(pc+1);
	varPtr = &(compiledLocals[opnd]);
	part1 = varPtr->name;
	while (TclIsVarLink(varPtr)) {
	    varPtr = varPtr->value.linkPtr;
	}
	TRACE(("%u => ", opnd));
	if (TclIsVarDirectReadable(varPtr)) {
	    /*
	     * No errors, no traces: just get the value.
	     */
	    objResultPtr = varPtr->value.objPtr;
	    TRACE_APPEND(("%.30s\n", O2S(objResultPtr)));
	    NEXT_INST_F(2, 0, 1);
	}
	pcAdjustment = 2;
	cleanup = 0;
	arrayPtr = NULL;
	part2 = NULL;
	goto doCallPtrGetVar;

    case INST_LOAD_SCALAR4:
	opnd = TclGetUInt4AtPtr(pc+1);
	varPtr = &(compiledLocals[opnd]);
	part1 = varPtr->name;
	while (TclIsVarLink(varPtr)) {
	    varPtr = varPtr->value.linkPtr;
	}
	TRACE(("%u => ", opnd));
	if (TclIsVarDirectReadable(varPtr)) {
	    /*
	     * No errors, no traces: just get the value.
	     */
	    objResultPtr = varPtr->value.objPtr;
	    TRACE_APPEND(("%.30s\n", O2S(objResultPtr)));
	    NEXT_INST_F(5, 0, 1);
	}
	pcAdjustment = 5;
	cleanup = 0;
	arrayPtr = NULL;
	part2 = NULL;
	goto doCallPtrGetVar;

    case INST_LOAD_ARRAY_STK:
	cleanup = 2;
	part2 = Tcl_GetString(*tosPtr);	/* element name */
	objPtr = *(tosPtr - 1);		/* array name */
	TRACE(("\"%.30s(%.30s)\" => ", O2S(objPtr), part2));
	goto doLoadStk;

    case INST_LOAD_STK:
    case INST_LOAD_SCALAR_STK:
	cleanup = 1;
	part2 = NULL;
	objPtr = *tosPtr;			/* variable name */
	TRACE(("\"%.30s\" => ", O2S(objPtr)));

    doLoadStk:
	part1 = TclGetString(objPtr);
	varPtr = TclObjLookupVar(interp, objPtr, part2, TCL_LEAVE_ERR_MSG,
		"read", /*createPart1*/ 0, /*createPart2*/ 1, &arrayPtr);
	if (varPtr == NULL) {
	    TRACE_APPEND(("ERROR: %.30s\n", O2S(Tcl_GetObjResult(interp))));
	    result = TCL_ERROR;
	    goto checkForCatch;
	}
	if (TclIsVarDirectReadable(varPtr)
		&& ((arrayPtr == NULL) || TclIsVarUntraced(arrayPtr))) {
	    /*
	     * No errors, no traces: just get the value.
	     */
	    objResultPtr = varPtr->value.objPtr;
	    TRACE_APPEND(("%.30s\n", O2S(objResultPtr)));
	    NEXT_INST_V(1, cleanup, 1);
	}
	pcAdjustment = 1;
	goto doCallPtrGetVar;

    case INST_LOAD_ARRAY4:
	opnd = TclGetUInt4AtPtr(pc+1);
	pcAdjustment = 5;
	goto doLoadArray;

    case INST_LOAD_ARRAY1:
	opnd = TclGetUInt1AtPtr(pc+1);
	pcAdjustment = 2;

    doLoadArray:
	part2 = TclGetString(*tosPtr);
	arrayPtr = &(compiledLocals[opnd]);
	part1 = arrayPtr->name;
	while (TclIsVarLink(arrayPtr)) {
	    arrayPtr = arrayPtr->value.linkPtr;
	}
	TRACE(("%u \"%.30s\" => ", opnd, part2));
	varPtr = TclLookupArrayElement(interp, part1, part2,
		TCL_LEAVE_ERR_MSG, "read", 0, 1, arrayPtr);
	if (varPtr == NULL) {
	    TRACE_APPEND(("ERROR: %.30s\n", O2S(Tcl_GetObjResult(interp))));
	    result = TCL_ERROR;
	    goto checkForCatch;
	}
	if (TclIsVarDirectReadable(varPtr)
		&& ((arrayPtr == NULL) || TclIsVarUntraced(arrayPtr))) {
	    /*
	     * No errors, no traces: just get the value.
	     */
	    objResultPtr = varPtr->value.objPtr;
	    TRACE_APPEND(("%.30s\n", O2S(objResultPtr)));
	    NEXT_INST_F(pcAdjustment, 1, 1);
	}
	cleanup = 1;
	goto doCallPtrGetVar;

    doCallPtrGetVar:
	/*
	 * There are either errors or the variable is traced: call
	 * TclPtrGetVar to process fully.
	 */

	DECACHE_STACK_INFO();
	objResultPtr = TclPtrGetVar(interp, varPtr, arrayPtr, part1, part2,
		TCL_LEAVE_ERR_MSG);
	CACHE_STACK_INFO();
	if (objResultPtr == NULL) {
	    TRACE_APPEND(("ERROR: %.30s\n", O2S(Tcl_GetObjResult(interp))));
	    result = TCL_ERROR;
	    goto checkForCatch;
	}
	TRACE_APPEND(("%.30s\n", O2S(objResultPtr)));
	NEXT_INST_V(pcAdjustment, cleanup, 1);
    }

    /*
     *	   End of INST_LOAD instructions.
     * ---------------------------------------------------------
     */

    /*
     * ---------------------------------------------------------
     *	   Start of INST_STORE and related instructions.
     *
     * WARNING: more 'goto' here than your doctor recommended!  The different
     * instructions set the value of some variables and then jump to somme
     * common execution code.
     */

    {
	int opnd, pcAdjustment, storeFlags;
	char *part1, *part2;
	Var *varPtr, *arrayPtr;
	Tcl_Obj *objPtr, *valuePtr;

    case INST_LAPPEND_STK:
	valuePtr = *tosPtr; /* value to append */
	part2 = NULL;
	storeFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE
		| TCL_LIST_ELEMENT | TCL_TRACE_READS);
	goto doStoreStk;

    case INST_LAPPEND_ARRAY_STK:
	valuePtr = *tosPtr; /* value to append */
	part2 = TclGetString(*(tosPtr - 1));
	storeFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE
		| TCL_LIST_ELEMENT | TCL_TRACE_READS);
	goto doStoreStk;

    case INST_APPEND_STK:
	valuePtr = *tosPtr; /* value to append */
	part2 = NULL;
	storeFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE);
	goto doStoreStk;

    case INST_APPEND_ARRAY_STK:
	valuePtr = *tosPtr; /* value to append */
	part2 = TclGetString(*(tosPtr - 1));
	storeFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE);
	goto doStoreStk;

    case INST_STORE_ARRAY_STK:
	valuePtr = *tosPtr;
	part2 = TclGetString(*(tosPtr - 1));
	storeFlags = TCL_LEAVE_ERR_MSG;
	goto doStoreStk;

    case INST_STORE_STK:
    case INST_STORE_SCALAR_STK:
	valuePtr = *tosPtr;
	part2 = NULL;
	storeFlags = TCL_LEAVE_ERR_MSG;

    doStoreStk:
	objPtr = *(tosPtr - 1 - (part2 != NULL)); /* variable name */
	part1 = TclGetString(objPtr);
#ifdef TCL_COMPILE_DEBUG
	if (part2 == NULL) {
	    TRACE(("\"%.30s\" <- \"%.30s\" =>", part1, O2S(valuePtr)));
	} else {
	    TRACE(("\"%.30s(%.30s)\" <- \"%.30s\" => ",
		    part1, part2, O2S(valuePtr)));
	}
#endif
	varPtr = TclObjLookupVar(interp, objPtr, part2, TCL_LEAVE_ERR_MSG,
		"set", /*createPart1*/ 1, /*createPart2*/ 1, &arrayPtr);
	if (varPtr == NULL) {
	    TRACE_APPEND(("ERROR: %.30s\n", O2S(Tcl_GetObjResult(interp))));
	    result = TCL_ERROR;
	    goto checkForCatch;
	}
	cleanup = ((part2 == NULL)? 2 : 3);
	pcAdjustment = 1;
	goto doCallPtrSetVar;

    case INST_LAPPEND_ARRAY4:
	opnd = TclGetUInt4AtPtr(pc+1);
	pcAdjustment = 5;
	storeFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE
		| TCL_LIST_ELEMENT | TCL_TRACE_READS);
	goto doStoreArray;

    case INST_LAPPEND_ARRAY1:
	opnd = TclGetUInt1AtPtr(pc+1);
	pcAdjustment = 2;
	storeFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE
		| TCL_LIST_ELEMENT | TCL_TRACE_READS);
	goto doStoreArray;

    case INST_APPEND_ARRAY4:
	opnd = TclGetUInt4AtPtr(pc+1);
	pcAdjustment = 5;
	storeFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE);
	goto doStoreArray;

    case INST_APPEND_ARRAY1:
	opnd = TclGetUInt1AtPtr(pc+1);
	pcAdjustment = 2;
	storeFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE);
	goto doStoreArray;

    case INST_STORE_ARRAY4:
	opnd = TclGetUInt4AtPtr(pc+1);
	pcAdjustment = 5;
	storeFlags = TCL_LEAVE_ERR_MSG;
	goto doStoreArray;

    case INST_STORE_ARRAY1:
	opnd = TclGetUInt1AtPtr(pc+1);
	pcAdjustment = 2;
	storeFlags = TCL_LEAVE_ERR_MSG;

    doStoreArray:
	valuePtr = *tosPtr;
	part2 = TclGetString(*(tosPtr - 1));
	arrayPtr = &(compiledLocals[opnd]);
	part1 = arrayPtr->name;
	TRACE(("%u \"%.30s\" <- \"%.30s\" => ", opnd, part2, O2S(valuePtr)));
	while (TclIsVarLink(arrayPtr)) {
	    arrayPtr = arrayPtr->value.linkPtr;
	}
	varPtr = TclLookupArrayElement(interp, part1, part2,
		TCL_LEAVE_ERR_MSG, "set", 1, 1, arrayPtr);
	if (varPtr == NULL) {
	    TRACE_APPEND(("ERROR: %.30s\n", O2S(Tcl_GetObjResult(interp))));
	    result = TCL_ERROR;
	    goto checkForCatch;
	}
	cleanup = 2;
	goto doCallPtrSetVar;

    case INST_LAPPEND_SCALAR4:
	opnd = TclGetUInt4AtPtr(pc+1);
	pcAdjustment = 5;
	storeFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE
		| TCL_LIST_ELEMENT | TCL_TRACE_READS);
	goto doStoreScalar;

    case INST_LAPPEND_SCALAR1:
	opnd = TclGetUInt1AtPtr(pc+1);
	pcAdjustment = 2;
	storeFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE
		| TCL_LIST_ELEMENT | TCL_TRACE_READS);
	goto doStoreScalar;

    case INST_APPEND_SCALAR4:
	opnd = TclGetUInt4AtPtr(pc+1);
	pcAdjustment = 5;
	storeFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE);
	goto doStoreScalar;

    case INST_APPEND_SCALAR1:
	opnd = TclGetUInt1AtPtr(pc+1);
	pcAdjustment = 2;
	storeFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE);
	goto doStoreScalar;

    case INST_STORE_SCALAR4:
	opnd = TclGetUInt4AtPtr(pc+1);
	pcAdjustment = 5;
	storeFlags = TCL_LEAVE_ERR_MSG;
	goto doStoreScalar;

    case INST_STORE_SCALAR1:
	opnd = TclGetUInt1AtPtr(pc+1);
	pcAdjustment = 2;
	storeFlags = TCL_LEAVE_ERR_MSG;

    doStoreScalar:
	valuePtr = *tosPtr;
	varPtr = &(compiledLocals[opnd]);
	part1 = varPtr->name;
	TRACE(("%u <- \"%.30s\" => ", opnd, O2S(valuePtr)));
	while (TclIsVarLink(varPtr)) {
	    varPtr = varPtr->value.linkPtr;
	}
	cleanup = 1;
	arrayPtr = NULL;
	part2 = NULL;

    doCallPtrSetVar:
	if ((storeFlags == TCL_LEAVE_ERR_MSG)
		&& TclIsVarDirectWritable(varPtr)
		&& ((arrayPtr == NULL) || TclIsVarUntraced(arrayPtr))) {
	    /*
	     * No traces, no errors, plain 'set': we can safely inline. The
	     * value *will* be set to what's requested, so that the stack top
	     * remains pointing to the same Tcl_Obj.
	     */

	    valuePtr = varPtr->value.objPtr;
	    objResultPtr = *tosPtr;
	    if (valuePtr != objResultPtr) {
		if (valuePtr != NULL) {
		    TclDecrRefCount(valuePtr);
		} else {
		    TclSetVarScalar(varPtr);
		    TclClearVarUndefined(varPtr);
		}
		varPtr->value.objPtr = objResultPtr;
		Tcl_IncrRefCount(objResultPtr);
	    }
#ifndef TCL_COMPILE_DEBUG
	    if (*(pc+pcAdjustment) == INST_POP) {
		NEXT_INST_V((pcAdjustment+1), cleanup, 0);
	    }
#else
	    TRACE_APPEND(("%.30s\n", O2S(objResultPtr)));
#endif
	    NEXT_INST_V(pcAdjustment, cleanup, 1);
	} else {
	    DECACHE_STACK_INFO();
	    objResultPtr = TclPtrSetVar(interp, varPtr, arrayPtr,
		    part1, part2, valuePtr, storeFlags);
	    CACHE_STACK_INFO();
	    if (objResultPtr == NULL) {
		TRACE_APPEND(("ERROR: %.30s\n", O2S(Tcl_GetObjResult(interp))));
		result = TCL_ERROR;
		goto checkForCatch;
	    }
	}
#ifndef TCL_COMPILE_DEBUG
	if (*(pc+pcAdjustment) == INST_POP) {
	    NEXT_INST_V((pcAdjustment+1), cleanup, 0);
	}
#endif
	TRACE_APPEND(("%.30s\n", O2S(objResultPtr)));
	NEXT_INST_V(pcAdjustment, cleanup, 1);
    }

    /*
     *	   End of INST_STORE and related instructions.
     * ---------------------------------------------------------
     */

    /*
     * ---------------------------------------------------------
     *	   Start of INST_INCR instructions.
     *
     * WARNING: more 'goto' here than your doctor recommended!  The different
     * instructions set the value of some variables and then jump to somme
     * common execution code.
     */

/*TODO: Consider more untangling here;  merge with LOAD and STORE ? */

    {
	Tcl_Obj *objPtr, *incrPtr;
	int opnd, pcAdjustment;
#ifndef NO_WIDE_TYPE
	Tcl_WideInt w;
#endif
	long i;
	char *part1, *part2;
	Var *varPtr, *arrayPtr;

    case INST_INCR_SCALAR1:
    case INST_INCR_ARRAY1:
    case INST_INCR_ARRAY_STK:
    case INST_INCR_SCALAR_STK:
    case INST_INCR_STK:
	opnd = TclGetUInt1AtPtr(pc+1);
	incrPtr = *tosPtr;
	tosPtr--;
	switch (*pc) {
	case INST_INCR_SCALAR1:
	    pcAdjustment = 2;
	    goto doIncrScalar;
	case INST_INCR_ARRAY1:
	    pcAdjustment = 2;
	    goto doIncrArray;
	default:
	    pcAdjustment = 1;
	    goto doIncrStk;
	}

    case INST_INCR_ARRAY_STK_IMM:
    case INST_INCR_SCALAR_STK_IMM:
    case INST_INCR_STK_IMM:
	i = TclGetInt1AtPtr(pc+1);
	incrPtr = Tcl_NewIntObj(i);
	Tcl_IncrRefCount(incrPtr);
	pcAdjustment = 2;

    doIncrStk:
	if ((*pc == INST_INCR_ARRAY_STK_IMM)
		|| (*pc == INST_INCR_ARRAY_STK)) {
	    part2 = TclGetString(*tosPtr);
	    objPtr = *(tosPtr - 1);
	    TRACE(("\"%.30s(%.30s)\" (by %ld) => ",
		    O2S(objPtr), part2, i));
	} else {
	    part2 = NULL;
	    objPtr = *tosPtr;
	    TRACE(("\"%.30s\" (by %ld) => ", O2S(objPtr), i));
	}
	part1 = TclGetString(objPtr);

	varPtr = TclObjLookupVar(interp, objPtr, part2,
		TCL_LEAVE_ERR_MSG, "read", 1, 1, &arrayPtr);
	if (varPtr == NULL) {
	    Tcl_AddObjErrorInfo(interp,
		    "\n    (reading value of variable to increment)", -1);
	    TRACE_APPEND(("ERROR: %.30s\n", O2S(Tcl_GetObjResult(interp))));
	    result = TCL_ERROR;
	    Tcl_DecrRefCount(incrPtr);
	    goto checkForCatch;
	}
	cleanup = ((part2 == NULL)? 1 : 2);
	goto doIncrVar;

    case INST_INCR_ARRAY1_IMM:
	opnd = TclGetUInt1AtPtr(pc+1);
	i = TclGetInt1AtPtr(pc+2);
	incrPtr = Tcl_NewIntObj(i);
	Tcl_IncrRefCount(incrPtr);
	pcAdjustment = 3;

    doIncrArray:
	part2 = TclGetString(*tosPtr);
	arrayPtr = &(compiledLocals[opnd]);
	part1 = arrayPtr->name;
	while (TclIsVarLink(arrayPtr)) {
	    arrayPtr = arrayPtr->value.linkPtr;
	}
	TRACE(("%u \"%.30s\" (by %ld) => ", opnd, part2, i));
	varPtr = TclLookupArrayElement(interp, part1, part2,
		TCL_LEAVE_ERR_MSG, "read", 1, 1, arrayPtr);
	if (varPtr == NULL) {
	    TRACE_APPEND(("ERROR: %.30s\n", O2S(Tcl_GetObjResult(interp))));
	    result = TCL_ERROR;
	    Tcl_DecrRefCount(incrPtr);
	    goto checkForCatch;
	}
	cleanup = 1;
	goto doIncrVar;

    case INST_INCR_SCALAR1_IMM:
	opnd = TclGetUInt1AtPtr(pc+1);
	i = TclGetInt1AtPtr(pc+2);
	pcAdjustment = 3;
	cleanup = 0;
	varPtr = &(compiledLocals[opnd]);
	while (TclIsVarLink(varPtr)) {
	    varPtr = varPtr->value.linkPtr;
	}

	if (TclIsVarDirectReadable(varPtr)) {
	    ClientData ptr;
	    int type;

	    objPtr = varPtr->value.objPtr;
	    if (GetNumberFromObj(NULL, objPtr, &ptr, &type) == TCL_OK) {
		if (type == TCL_NUMBER_LONG) {
		    long augend = *((CONST long *)ptr);
		    long sum = augend + i;
		    /* Test for overflow */
		    /* TODO: faster checking with known limits on i ? */
		    if ((augend >= 0 || i >= 0 || sum < 0)
			    && (sum >= 0 || i < 0 || augend < 0)) {

			TRACE(("%u %ld => ", opnd, i));
			if (Tcl_IsShared(objPtr)) {
			    objPtr->refCount--;  /* we know it's shared */
			    TclNewLongObj(objResultPtr, sum);
			    Tcl_IncrRefCount(objResultPtr);
			    varPtr->value.objPtr = objResultPtr;
			} else {
			    objResultPtr = objPtr;
			    TclSetLongObj(objPtr, sum);
			}
			goto doneIncr;
		    }
#ifndef NO_WIDE_TYPE
		    {
			w = (Tcl_WideInt)augend;

			TRACE(("%u %ld => ", opnd, i));
			if (Tcl_IsShared(objPtr)) {
			    objPtr->refCount--;  /* we know it's shared */
			    objResultPtr = Tcl_NewWideIntObj(w+i);
			    Tcl_IncrRefCount(objResultPtr);
			    varPtr->value.objPtr = objResultPtr;
			} else {
			    objResultPtr = objPtr;
			    /* We know the sum value is outside the long range;
			     * use macro form that doesn't range test again */
			    TclSetWideIntObj(objPtr, w+i);
			}
			goto doneIncr;
		    }
#endif
		}	/* end if (type == TCL_NUMBER_LONG) */
#ifndef NO_WIDE_TYPE
		if (type == TCL_NUMBER_WIDE) {
		    Tcl_WideInt sum;
		    w = *((CONST Tcl_WideInt *)ptr);
		    sum = w + i;

		    /* Check for overflow */
		    if ((w >= 0 || i >= 0 || sum < 0)
			    && (w < 0 || i < 0 || sum >= 0)) {
			TRACE(("%u %ld => ", opnd, i));
			if (Tcl_IsShared(objPtr)) {
			    objPtr->refCount--;  /* we know it's shared */
			    objResultPtr = Tcl_NewWideIntObj(sum);
			    Tcl_IncrRefCount(objResultPtr);
			    varPtr->value.objPtr = objResultPtr;
			} else {
			    objResultPtr = objPtr;
			    /* We *do not* know the sum value is outside
			     * the long range (wide + long can yield long);
			     * use the function call that checks range. */
			    Tcl_SetWideIntObj(objPtr, sum);
			}
			goto doneIncr;
		    }
		}
#endif
	    }
	    if (Tcl_IsShared(objPtr)) {
		objPtr->refCount--;  /* we know it's shared */
		objResultPtr = Tcl_DuplicateObj(objPtr);
		Tcl_IncrRefCount(objResultPtr);
		varPtr->value.objPtr = objResultPtr;
	    } else {
		objResultPtr = objPtr;
	    }
	    TclNewLongObj(incrPtr, i);
	    result = TclIncrObj(interp, objResultPtr, incrPtr);
	    Tcl_DecrRefCount(incrPtr);
	    if (result != TCL_OK) {
		TRACE_APPEND(("ERROR: %.30s\n", O2S(Tcl_GetObjResult(interp))));
		goto checkForCatch;
	    }
	    goto doneIncr;
	}
	/* All other cases, flow through to generic handling */
	TclNewLongObj(incrPtr, i);
	Tcl_IncrRefCount(incrPtr);

    doIncrScalar:
	varPtr = &(compiledLocals[opnd]);
	part1 = varPtr->name;
	while (TclIsVarLink(varPtr)) {
	    varPtr = varPtr->value.linkPtr;
	}
	arrayPtr = NULL;
	part2 = NULL;
	cleanup = 0;
	TRACE(("%u %ld => ", opnd, i));

    doIncrVar:
	if (TclIsVarDirectReadable(varPtr)
	    && ((arrayPtr == NULL) || TclIsVarUntraced(arrayPtr))) {
	    objPtr = varPtr->value.objPtr;
	    if (Tcl_IsShared(objPtr)) {
		objPtr->refCount--;  /* we know it's shared */
		objResultPtr = Tcl_DuplicateObj(objPtr);
		Tcl_IncrRefCount(objResultPtr);
		varPtr->value.objPtr = objResultPtr;
	    } else {
		objResultPtr = objPtr;
	    }
	    result = TclIncrObj(interp, objResultPtr, incrPtr);
	    Tcl_DecrRefCount(incrPtr);
	    if (result != TCL_OK) {
		TRACE_APPEND(("ERROR: %.30s\n", O2S(Tcl_GetObjResult(interp))));
		goto checkForCatch;
	    }
	} else {
	    DECACHE_STACK_INFO();
	    objResultPtr = TclPtrIncrObjVar(interp, varPtr, arrayPtr,
		    part1, part2, incrPtr, TCL_LEAVE_ERR_MSG);
	    CACHE_STACK_INFO();
	    Tcl_DecrRefCount(incrPtr);
	    if (objResultPtr == NULL) {
		TRACE_APPEND(("ERROR: %.30s\n", O2S(Tcl_GetObjResult(interp))));
		result = TCL_ERROR;
		goto checkForCatch;
	    }
	}
    doneIncr:
	TRACE_APPEND(("%.30s\n", O2S(objResultPtr)));
#ifndef TCL_COMPILE_DEBUG
	if (*(pc+pcAdjustment) == INST_POP) {
	    NEXT_INST_V((pcAdjustment+1), cleanup, 0);
	}
#endif
	NEXT_INST_V(pcAdjustment, cleanup, 1);
    }

    /*
     *	   End of INST_INCR instructions.
     * ---------------------------------------------------------
     */

    case INST_JUMP1: {
	int opnd;

	opnd = TclGetInt1AtPtr(pc+1);
	TRACE(("%d => new pc %u\n", opnd,
		(unsigned int)(pc + opnd - codePtr->codeStart)));
	NEXT_INST_F(opnd, 0, 0);
    }

    case INST_JUMP4: {
	int opnd;

	opnd = TclGetInt4AtPtr(pc+1);
	TRACE(("%d => new pc %u\n", opnd,
		(unsigned int)(pc + opnd - codePtr->codeStart)));
	NEXT_INST_F(opnd, 0, 0);
    }

    {
	int jmpOffset[2];
	int b;
	Tcl_Obj *valuePtr;

/* TODO: consider rewrite so we don't compute the offset we're
 * not going to take. */
    case INST_JUMP_FALSE4:
	jmpOffset[0] = TclGetInt4AtPtr(pc+1);	/* FALSE offset */
	jmpOffset[1] = 5;			/* TRUE  offset*/
	goto doCondJump;

    case INST_JUMP_TRUE4:
	jmpOffset[0] = 5;
	jmpOffset[1] = TclGetInt4AtPtr(pc+1);
	goto doCondJump;

    case INST_JUMP_FALSE1:
	jmpOffset[0] = TclGetInt1AtPtr(pc+1);
	jmpOffset[1] = 2;
	goto doCondJump;

    case INST_JUMP_TRUE1:
	jmpOffset[0] = 2;
	jmpOffset[1] = TclGetInt1AtPtr(pc+1);

    doCondJump:
	valuePtr = *tosPtr;

	/* TODO - check claim that taking address of b harms performance */
	/* TODO - consider optimization search for eePtr->constants */
	result = TclGetBooleanFromObj(interp, valuePtr, &b);
	if (result != TCL_OK) {
	    TRACE_WITH_OBJ(("%d => ERROR: ", jmpOffset[
		    ((*pc == INST_JUMP_FALSE1) || (*pc == INST_JUMP_FALSE4))
		    ? 0 : 1]), Tcl_GetObjResult(interp));
	    goto checkForCatch;
	}

#ifdef TCL_COMPILE_DEBUG
	if (b) {
	    if ((*pc == INST_JUMP_TRUE1) || (*pc == INST_JUMP_TRUE4)) {
		TRACE(("%d => %.20s true, new pc %u\n", jmpOffset[1], O2S(valuePtr),
			(unsigned int)(pc+jmpOffset[1] - codePtr->codeStart)));
	    } else {
		TRACE(("%d => %.20s true\n", jmpOffset[0], O2S(valuePtr)));
	    }
	} else {
	    if ((*pc == INST_JUMP_TRUE1) || (*pc == INST_JUMP_TRUE4)) {
		TRACE(("%d => %.20s false\n", jmpOffset[0], O2S(valuePtr)));
	    } else {
		TRACE(("%d => %.20s false, new pc %u\n", jmpOffset[0], O2S(valuePtr),
			(unsigned int)(pc + jmpOffset[1] - codePtr->codeStart)));
	    }
	}
#endif
	NEXT_INST_F(jmpOffset[b], 1, 0);
    }

    case INST_JUMP_TABLE: {
	Tcl_HashEntry *hPtr;
	JumptableInfo *jtPtr;
	int opnd;

	/*
	 * Jump to location looked up in a hashtable; fall through to next
	 * instr if lookup fails.
	 */

	opnd = TclGetInt4AtPtr(pc+1);
	jtPtr = (JumptableInfo *) codePtr->auxDataArrayPtr[opnd].clientData;
	TRACE(("%d => %.20s ", opnd, O2S(*tosPtr)));
	hPtr = Tcl_FindHashEntry(&jtPtr->hashTable, Tcl_GetString(*tosPtr));
	if (hPtr != NULL) {
	    int jumpOffset = (int) Tcl_GetHashValue(hPtr);

	    TRACE_APPEND(("found in table, new pc %u\n",
		    (unsigned int)(pc - codePtr->codeStart + jumpOffset)));
	    NEXT_INST_F(jumpOffset, 1, 0);
	} else {
	    TRACE_APPEND(("not found in table\n"));
	    NEXT_INST_F(5, 1, 0);
	}
    }

    /*
     * These two instructions are now redundant: the complete logic of the LOR
     * and LAND is now handled by the expression compiler.
     */

    case INST_LOR:
    case INST_LAND:
    {
	/*
	 * Operands must be boolean or numeric. No int->double conversions are
	 * performed.
	 */

	int i1, i2, iResult;
	Tcl_Obj *value2Ptr = *tosPtr;
	Tcl_Obj *valuePtr  = *(tosPtr - 1);

	result = TclGetBooleanFromObj(NULL, valuePtr, &i1);
	if (result != TCL_OK) {
	    TRACE(("\"%.20s\" => ILLEGAL TYPE %s \n", O2S(valuePtr),
		    (valuePtr->typePtr? valuePtr->typePtr->name : "null")));
	    IllegalExprOperandType(interp, pc, valuePtr);
	    goto checkForCatch;
	}

	result = TclGetBooleanFromObj(NULL, value2Ptr, &i2);
	if (result != TCL_OK) {
	    TRACE(("\"%.20s\" => ILLEGAL TYPE %s \n", O2S(value2Ptr),
		    (value2Ptr->typePtr? value2Ptr->typePtr->name : "null")));
	    IllegalExprOperandType(interp, pc, value2Ptr);
	    goto checkForCatch;
	}

	if (*pc == INST_LOR) {
	    iResult = (i1 || i2);
	} else {
	    iResult = (i1 && i2);
	}
	objResultPtr = eePtr->constants[iResult];
	TRACE(("%.20s %.20s => %d\n", O2S(valuePtr), O2S(value2Ptr), iResult));
	NEXT_INST_F(1, 2, 1);
    }

    /*
     * ---------------------------------------------------------
     *	   Start of INST_LIST and related instructions.
     */

    case INST_LIST: {
	/*
	 * Pop the opnd (objc) top stack elements into a new list obj and then
	 * decrement their ref counts.
	 */

	int opnd;

	opnd = TclGetUInt4AtPtr(pc+1);
	objResultPtr = Tcl_NewListObj(opnd, (tosPtr - (opnd-1)));
	TRACE_WITH_OBJ(("%u => ", opnd), objResultPtr);
	NEXT_INST_V(5, opnd, 1);
    }

    case INST_LIST_LENGTH: {
	Tcl_Obj *valuePtr;
	int length;

	valuePtr = *tosPtr;

	result = Tcl_ListObjLength(interp, valuePtr, &length);
	if (result != TCL_OK) {
	    TRACE_WITH_OBJ(("%.30s => ERROR: ", O2S(valuePtr)),
		    Tcl_GetObjResult(interp));
	    goto checkForCatch;
	}
	TclNewIntObj(objResultPtr, length);
	TRACE(("%.20s => %d\n", O2S(valuePtr), length));
	NEXT_INST_F(1, 1, 1);
    }

    case INST_LIST_INDEX: {
	/*** lindex with objc == 3 ***/

	Tcl_Obj *valuePtr, *value2Ptr;

	/*
	 * Pop the two operands
	 */

	value2Ptr = *tosPtr;
	valuePtr  = *(tosPtr - 1);

	/*
	 * Extract the desired list element
	 */

	objResultPtr = TclLindexList(interp, valuePtr, value2Ptr);
	if (objResultPtr == NULL) {
	    TRACE_WITH_OBJ(("%.30s %.30s => ERROR: ", O2S(valuePtr), O2S(value2Ptr)),
		    Tcl_GetObjResult(interp));
	    result = TCL_ERROR;
	    goto checkForCatch;
	}

	/*
	 * Stash the list element on the stack
	 */

	TRACE(("%.20s %.20s => %s\n",
		O2S(valuePtr), O2S(value2Ptr), O2S(objResultPtr)));
	NEXT_INST_F(1, 2, -1); /* already has the correct refCount */
    }

    case INST_LIST_INDEX_IMM: {
	/*** lindex with objc==3 and index in bytecode stream ***/

	int listc, idx, opnd;
	Tcl_Obj **listv;
	Tcl_Obj *valuePtr;

	/*
	 * Pop the list and get the index
	 */

	valuePtr = *tosPtr;
	opnd = TclGetInt4AtPtr(pc+1);

	/*
	 * Get the contents of the list, making sure that it really is a list
	 * in the process.
	 */

	result = Tcl_ListObjGetElements(interp, valuePtr, &listc, &listv);
	if (result != TCL_OK) {
	    TRACE_WITH_OBJ(("\"%.30s\" %d => ERROR: ", O2S(valuePtr), opnd),
		    Tcl_GetObjResult(interp));
	    goto checkForCatch;
	}

	/*
	 * Select the list item based on the index.  Negative operand means
	 * end-based indexing.
	 */

	if (opnd < -1) {
	    idx = opnd+1 + listc;
	} else {
	    idx = opnd;
	}
	if (idx >= 0 && idx < listc) {
	    objResultPtr = listv[idx];
	} else {
	    TclNewObj(objResultPtr);
	}

	TRACE_WITH_OBJ(("\"%.30s\" %d => ", O2S(valuePtr), opnd), objResultPtr);
	NEXT_INST_F(5, 1, 1);
    }

    case INST_LIST_INDEX_MULTI: {
	/*
	 * 'lindex' with multiple index args:
	 *
	 * Determine the count of index args.
	 */

	int numIdx, opnd;

	opnd = TclGetUInt4AtPtr(pc+1);
	numIdx = opnd-1;

	/*
	 * Do the 'lindex' operation.
	 */

	objResultPtr = TclLindexFlat(interp, *(tosPtr - numIdx),
		numIdx, tosPtr - numIdx + 1);

	/*
	 * Check for errors
	 */
	if (objResultPtr == NULL) {
	    TRACE_WITH_OBJ(("%d => ERROR: ", opnd), Tcl_GetObjResult(interp));
	    result = TCL_ERROR;
	    goto checkForCatch;
	}

	/*
	 * Set result
	 */
	TRACE(("%d => %s\n", opnd, O2S(objResultPtr)));
	NEXT_INST_V(5, opnd, -1);
    }

    case INST_LSET_FLAT: {
	/*
	 * Lset with 3, 5, or more args.  Get the number of index args.
	 */
	int numIdx,opnd;
	Tcl_Obj *valuePtr, *value2Ptr;

	opnd = TclGetUInt4AtPtr(pc + 1);
	numIdx = opnd - 2;

	/*
	 * Get the old value of variable, and remove the stack ref.  This is
	 * safe because the variable still references the object; the ref
	 * count will never go zero here.
	 */
	value2Ptr = POP_OBJECT();
	TclDecrRefCount(value2Ptr); /* This one should be done here */

	/*
	 * Get the new element value.
	 */
	valuePtr = *tosPtr;

	/*
	 * Compute the new variable value
	 */
	objResultPtr = TclLsetFlat(interp, value2Ptr, numIdx,
		tosPtr - numIdx, valuePtr);

	/*
	 * Check for errors
	 */
	if (objResultPtr == NULL) {
	    TRACE_WITH_OBJ(("%d => ERROR: ", opnd), Tcl_GetObjResult(interp));
	    result = TCL_ERROR;
	    goto checkForCatch;
	}

	/*
	 * Set result
	 */
	TRACE(("%d => %s\n", opnd, O2S(objResultPtr)));
	NEXT_INST_V(5, (numIdx+1), -1);
    }

    case INST_LSET_LIST: {
	/*
	 * 'lset' with 4 args.
	 */

	Tcl_Obj *objPtr, *valuePtr, *value2Ptr;

	/*
	 * Get the old value of variable, and remove the stack ref.  This is
	 * safe because the variable still references the object; the ref
	 * count will never go zero here.
	 */
	objPtr = POP_OBJECT();
	TclDecrRefCount(objPtr); /* This one should be done here */

	/*
	 * Get the new element value, and the index list
	 */
	valuePtr = *tosPtr;
	value2Ptr = *(tosPtr - 1);

	/*
	 * Compute the new variable value
	 */
	objResultPtr = TclLsetList(interp, objPtr, value2Ptr, valuePtr);

	/*
	 * Check for errors
	 */
	if (objResultPtr == NULL) {
	    TRACE_WITH_OBJ(("\"%.30s\" => ERROR: ", O2S(value2Ptr)),
		    Tcl_GetObjResult(interp));
	    result = TCL_ERROR;
	    goto checkForCatch;
	}

	/*
	 * Set result
	 */
	TRACE(("=> %s\n", O2S(objResultPtr)));
	NEXT_INST_F(1, 2, -1);
    }

    case INST_LIST_RANGE_IMM: {
	/*** lrange with objc==4 and both indices in bytecode stream ***/

	int listc, fromIdx, toIdx;
	Tcl_Obj **listv;
	Tcl_Obj *valuePtr;

	/*
	 * Pop the list and get the indices
	 */
	valuePtr = *tosPtr;
	fromIdx = TclGetInt4AtPtr(pc+1);
	toIdx = TclGetInt4AtPtr(pc+5);

	/*
	 * Get the contents of the list, making sure that it really is a list
	 * in the process.
	 */
	result = Tcl_ListObjGetElements(interp, valuePtr, &listc, &listv);
	if (result != TCL_OK) {
	    TRACE_WITH_OBJ(("\"%.30s\" %d %d => ERROR: ", O2S(valuePtr),
		    fromIdx, toIdx), Tcl_GetObjResult(interp));
	    goto checkForCatch;
	}

	/*
	 * Skip a lot of work if we're about to throw the result away (common
	 * with uses of [lassign].)
	 */
#ifndef TCL_COMPILE_DEBUG
	if (*(pc+9) == INST_POP) {
	    NEXT_INST_F(10, 1, 0);
	}
#endif

	/*
	 * Adjust the indices for end-based handling.
	 */
	if (fromIdx < -1) {
	    fromIdx += 1+listc;
	    if (fromIdx < -1) {
		fromIdx = -1;
	    }
	} else if (fromIdx > listc) {
	    fromIdx = listc;
	}
	if (toIdx < -1) {
	    toIdx += 1+listc;
	    if (toIdx < -1) {
		toIdx = -1;
	    }
	} else if (toIdx > listc) {
	    toIdx = listc;
	}

	/*
	 * Check if we are referring to a valid, non-empty list range, and if
	 * so, build the list of elements in that range.
	 */
	if (fromIdx<=toIdx && fromIdx<listc && toIdx>=0) {
	    if (fromIdx<0) {
		fromIdx = 0;
	    }
	    if (toIdx >= listc) {
		toIdx = listc-1;
	    }
	    objResultPtr = Tcl_NewListObj(toIdx-fromIdx+1, listv+fromIdx);
	} else {
	    TclNewObj(objResultPtr);
	}

	TRACE_WITH_OBJ(("\"%.30s\" %d %d => ", O2S(valuePtr),
		TclGetInt4AtPtr(pc+1), TclGetInt4AtPtr(pc+5)), objResultPtr);
	NEXT_INST_F(9, 1, 1);
    }

    case INST_LIST_IN:
    case INST_LIST_NOT_IN: {
	/*
	 * Basic list containment operators.
	 */

	int found, s1len, s2len, llen, i;
	Tcl_Obj *valuePtr, *value2Ptr, *o;
	char *s1, *s2;

	value2Ptr = *tosPtr;
	valuePtr = *(tosPtr - 1);

	/* TODO: Consider more efficient tests than strcmp() */
	s1 = Tcl_GetStringFromObj(valuePtr, &s1len);
	result = Tcl_ListObjLength(interp, value2Ptr, &llen);
	if (result != TCL_OK) {
	    TRACE_WITH_OBJ(("\"%.30s\" \"%.30s\" => ERROR: ", O2S(valuePtr),
		    O2S(value2Ptr)), Tcl_GetObjResult(interp));
	    goto checkForCatch;
	}
	found = 0;
	if (llen > 0) {
	    /* An empty list doesn't match anything */
	    i = 0;
	    do {
		Tcl_ListObjIndex(NULL, value2Ptr, i, &o);
		if (o != NULL) {
		    s2 = Tcl_GetStringFromObj(o, &s2len);
		} else {
		    s2 = "";
		}
		if (s1len == s2len) {
		    found = (strcmp(s1, s2) == 0);
		}
		i++;
	    } while (i < llen && found == 0);
	}

	if (*pc == INST_LIST_NOT_IN) {
	    found = !found;
	}

	TRACE(("%.20s %.20s => %d\n", O2S(valuePtr), O2S(value2Ptr), found));

	/*
	 * Peep-hole optimisation: if you're about to jump, do jump from here.
	 * We're saving the effort of pushing a boolean value only to pop it
	 * for branching.
	 */

	pc++;
#ifndef TCL_COMPILE_DEBUG
	switch (*pc) {
	case INST_JUMP_FALSE1:
	    NEXT_INST_F((found ? 2 : TclGetInt1AtPtr(pc+1)), 2, 0);
	case INST_JUMP_TRUE1:
	    NEXT_INST_F((found ? TclGetInt1AtPtr(pc+1) : 2), 2, 0);
	case INST_JUMP_FALSE4:
	    NEXT_INST_F((found ? 5 : TclGetInt4AtPtr(pc+1)), 2, 0);
	case INST_JUMP_TRUE4:
	    NEXT_INST_F((found ? TclGetInt4AtPtr(pc+1) : 5), 2, 0);
	}
#endif
	objResultPtr = eePtr->constants[found];
	NEXT_INST_F(0, 2, 1);
    }

    /*
     *	   End of INST_LIST and related instructions.
     * ---------------------------------------------------------
     */

    case INST_STR_EQ:
    case INST_STR_NEQ: {
	/*
	 * String (in)equality check
	 * TODO: Consider merging into INST_STR_CMP
	 */

	int iResult;
	Tcl_Obj *valuePtr, *value2Ptr;

	value2Ptr = *tosPtr;
	valuePtr = *(tosPtr - 1);

	if (valuePtr == value2Ptr) {
	    /*
	     * On the off-chance that the objects are the same, we don't
	     * really have to think hard about equality.
	     */

	    iResult = (*pc == INST_STR_EQ);
	} else {
	    char *s1, *s2;
	    int s1len, s2len;

	    s1 = Tcl_GetStringFromObj(valuePtr, &s1len);
	    s2 = Tcl_GetStringFromObj(value2Ptr, &s2len);
	    if (s1len == s2len) {
		/*
		 * We only need to check (in)equality when we have equal
		 * length strings.
		 */

		if (*pc == INST_STR_NEQ) {
		    iResult = (strcmp(s1, s2) != 0);
		} else {
		    /* INST_STR_EQ */
		    iResult = (strcmp(s1, s2) == 0);
		}
	    } else {
		iResult = (*pc == INST_STR_NEQ);
	    }
	}

	TRACE(("%.20s %.20s => %d\n", O2S(valuePtr), O2S(value2Ptr), iResult));

	/*
	 * Peep-hole optimisation: if you're about to jump, do jump from here.
	 */

	pc++;
#ifndef TCL_COMPILE_DEBUG
	switch (*pc) {
	case INST_JUMP_FALSE1:
	    NEXT_INST_F((iResult? 2 : TclGetInt1AtPtr(pc+1)), 2, 0);
	case INST_JUMP_TRUE1:
	    NEXT_INST_F((iResult? TclGetInt1AtPtr(pc+1) : 2), 2, 0);
	case INST_JUMP_FALSE4:
	    NEXT_INST_F((iResult? 5 : TclGetInt4AtPtr(pc+1)), 2, 0);
	case INST_JUMP_TRUE4:
	    NEXT_INST_F((iResult? TclGetInt4AtPtr(pc+1) : 5), 2, 0);
	}
#endif
	objResultPtr = eePtr->constants[iResult];
	NEXT_INST_F(0, 2, 1);
    }

    case INST_STR_CMP: {
	/*
	 * String compare
	 */

	CONST char *s1, *s2;
	int s1len, s2len, iResult;
	Tcl_Obj *valuePtr, *value2Ptr;

    stringCompare:
	value2Ptr = *tosPtr;
	valuePtr = *(tosPtr - 1);

	/*
	 * The comparison function should compare up to the minimum byte
	 * length only.
	 */

	if (valuePtr == value2Ptr) {
	    /*
	     * In the pure equality case, set lengths too for the checks below
	     * (or we could goto beyond it).
	     */

	    iResult = s1len = s2len = 0;
	} else if ((valuePtr->typePtr == &tclByteArrayType)
		&& (value2Ptr->typePtr == &tclByteArrayType)) {
	    s1 = (char *) Tcl_GetByteArrayFromObj(valuePtr, &s1len);
	    s2 = (char *) Tcl_GetByteArrayFromObj(value2Ptr, &s2len);
	    iResult = memcmp(s1, s2,
		    (size_t) ((s1len < s2len) ? s1len : s2len));
	} else if (((valuePtr->typePtr == &tclStringType)
		&& (value2Ptr->typePtr == &tclStringType))) {
	    /*
	     * Do a unicode-specific comparison if both of the args are of
	     * String type. If the char length == byte length, we can do a
	     * memcmp. In benchmark testing this proved the most efficient
	     * check between the unicode and string comparison operations.
	     */

	    s1len = Tcl_GetCharLength(valuePtr);
	    s2len = Tcl_GetCharLength(value2Ptr);
	    if ((s1len == valuePtr->length) && (s2len == value2Ptr->length)) {
		iResult = memcmp(valuePtr->bytes, value2Ptr->bytes,
			(unsigned) ((s1len < s2len) ? s1len : s2len));
	    } else {
		iResult = TclUniCharNcmp(Tcl_GetUnicode(valuePtr),
			Tcl_GetUnicode(value2Ptr),
			(unsigned) ((s1len < s2len) ? s1len : s2len));
	    }
	} else {
	    /*
	     * We can't do a simple memcmp in order to handle the special Tcl
	     * \xC0\x80 null encoding for utf-8.
	     */
	    s1 = Tcl_GetStringFromObj(valuePtr, &s1len);
	    s2 = Tcl_GetStringFromObj(value2Ptr, &s2len);
	    iResult = TclpUtfNcmp2(s1, s2,
		    (size_t) ((s1len < s2len) ? s1len : s2len));
	}

	/*
	 * Make sure only -1,0,1 is returned
	 * TODO: consider peephole opt.
	 */
	if (iResult == 0) {
	    iResult = s1len - s2len;
	}

	if (*pc != INST_STR_CMP) {
	    /* Take care of the opcodes that goto'ed into here */
	    switch (*pc) {
	    case INST_EQ:
		iResult = (iResult == 0);
		break;
	    case INST_NEQ:
		iResult = (iResult != 0);
		break;
	    case INST_LT:
		iResult = (iResult < 0);
		break;
	    case INST_GT:
		iResult = (iResult > 0);
		break;
	    case INST_LE:
		iResult = (iResult <= 0);
		break;
	    case INST_GE:
		iResult = (iResult >= 0);
		break;
	    }
	}
	if (iResult < 0) {
	    TclNewIntObj(objResultPtr, -1);
	    TRACE(("%.20s %.20s => %d\n", O2S(valuePtr), O2S(value2Ptr), -1));
	} else {
	    objResultPtr = eePtr->constants[(iResult>0)];
	    TRACE(("%.20s %.20s => %d\n", O2S(valuePtr), O2S(value2Ptr),
		(iResult > 0)));
	}

	NEXT_INST_F(1, 2, 1);
    }

    case INST_STR_LEN: {
	int length;
	Tcl_Obj *valuePtr;

	valuePtr = *tosPtr;

	if (valuePtr->typePtr == &tclByteArrayType) {
	    (void) Tcl_GetByteArrayFromObj(valuePtr, &length);
	} else {
	    length = Tcl_GetCharLength(valuePtr);
	}
	TclNewIntObj(objResultPtr, length);
	TRACE(("%.20s => %d\n", O2S(valuePtr), length));
	NEXT_INST_F(1, 1, 1);
    }

    case INST_STR_INDEX: {
	/*
	 * String compare
	 */
	int index, length;
	char *bytes;
	Tcl_Obj *valuePtr, *value2Ptr;

	bytes = NULL; /* lint */
	value2Ptr = *tosPtr;
	valuePtr = *(tosPtr - 1);

	/*
	 * If we have a ByteArray object, avoid indexing in the Utf string
	 * since the byte array contains one byte per character.  Otherwise,
	 * use the Unicode string rep to get the index'th char.
	 */

	if (valuePtr->typePtr == &tclByteArrayType) {
	    bytes = (char *)Tcl_GetByteArrayFromObj(valuePtr, &length);
	} else {
	    /*
	     * Get Unicode char length to calulate what 'end' means.
	     */
	    length = Tcl_GetCharLength(valuePtr);
	}

	result = TclGetIntForIndex(interp, value2Ptr, length - 1, &index);
	if (result != TCL_OK) {
	    goto checkForCatch;
	}

	if ((index >= 0) && (index < length)) {
	    if (valuePtr->typePtr == &tclByteArrayType) {
		objResultPtr = Tcl_NewByteArrayObj((unsigned char *)
			(&bytes[index]), 1);
	    } else if (valuePtr->bytes && length == valuePtr->length) {
		objResultPtr = Tcl_NewStringObj((CONST char *)
			(&valuePtr->bytes[index]), 1);
	    } else {
		char buf[TCL_UTF_MAX];
		Tcl_UniChar ch;

		ch = Tcl_GetUniChar(valuePtr, index);

		/*
		 * This could be: Tcl_NewUnicodeObj((CONST Tcl_UniChar *)&ch,
		 * 1) but creating the object as a string seems to be faster
		 * in practical use.
		 */

		length = Tcl_UniCharToUtf(ch, buf);
		objResultPtr = Tcl_NewStringObj(buf, length);
	    }
	} else {
	    TclNewObj(objResultPtr);
	}

	TRACE(("%.20s %.20s => %s\n", O2S(valuePtr), O2S(value2Ptr),
		O2S(objResultPtr)));
	NEXT_INST_F(1, 2, 1);
    }

    case INST_STR_MATCH: {
	int nocase, match;
	Tcl_Obj *valuePtr, *value2Ptr;

	nocase    = TclGetInt1AtPtr(pc+1);
	valuePtr  = *tosPtr;		/* String */
	value2Ptr = *(tosPtr - 1);	/* Pattern */

	/*
	 * Check that at least one of the objects is Unicode before promoting
	 * both.
	 */

	if ((valuePtr->typePtr == &tclStringType)
		|| (value2Ptr->typePtr == &tclStringType)) {
	    Tcl_UniChar *ustring1, *ustring2;
	    int length1, length2;

	    ustring1 = Tcl_GetUnicodeFromObj(valuePtr, &length1);
	    ustring2 = Tcl_GetUnicodeFromObj(value2Ptr, &length2);
	    match = TclUniCharMatch(ustring1, length1, ustring2, length2,
		    nocase);
	} else {
	    match = Tcl_StringCaseMatch(TclGetString(valuePtr),
		    TclGetString(value2Ptr), nocase);
	}

	/*
	 * Reuse value2Ptr object already on stack if possible. Adjustment is
	 * 2 due to the nocase byte
	 * TODO: consider peephole opt.
	 */

	TRACE(("%.20s %.20s => %d\n", O2S(valuePtr), O2S(value2Ptr), match));
	objResultPtr = eePtr->constants[match];
	NEXT_INST_F(2, 2, 1);
    }

    case INST_EQ:
    case INST_NEQ:
    case INST_LT:
    case INST_GT:
    case INST_LE:
    case INST_GE: {
	Tcl_Obj *valuePtr = *(tosPtr - 1);
	Tcl_Obj *value2Ptr = *tosPtr;
	ClientData ptr1, ptr2;
	int iResult = 0, compare = 0, type1, type2;
	double d1, d2, tmp;
	long l1, l2;
	mp_int big1, big2;
#ifndef NO_WIDE_TYPE
	Tcl_WideInt w1, w2;
#endif

	if (GetNumberFromObj(NULL, valuePtr, &ptr1, &type1) != TCL_OK) {
	    /* At least one non-numeric argument - compare as strings */
	    goto stringCompare;
	}
	if (type1 == TCL_NUMBER_NAN) {
	    /* NaN first arg: NaN != to everything, other compares are false */
	    iResult = (*pc == INST_NEQ);
	    goto foundResult;
	}
	if (valuePtr == value2Ptr) {
	    compare = MP_EQ;
	    goto convertComparison;
	}
	if (GetNumberFromObj(NULL, value2Ptr, &ptr2, &type2) != TCL_OK) {
	    /* At least one non-numeric argument - compare as strings */
	    goto stringCompare;
	}
	if (type2 == TCL_NUMBER_NAN) {
	    /* NaN 2nd arg: NaN != to everything, other compares are false */
	    iResult = (*pc == INST_NEQ);
	    goto foundResult;
	}
	switch (type1) {
	case TCL_NUMBER_LONG:
	    l1 = *((CONST long *)ptr1);
	    switch (type2) {
	    case TCL_NUMBER_LONG:
		l2 = *((CONST long *)ptr2);
	    longCompare:
		compare = (l1 < l2) ? MP_LT : ((l1 > l2) ? MP_GT : MP_EQ);
		break;
#ifndef NO_WIDE_TYPE
	    case TCL_NUMBER_WIDE:
		w2 = *((CONST Tcl_WideInt *)ptr2);
		w1 = (Tcl_WideInt)l1;
		goto wideCompare;
#endif
	    case TCL_NUMBER_DOUBLE:
		d2 = *((CONST double *)ptr2);
		d1 = (double) l1;

		/*
		 * If the double has a fractional part, or if the long can be
		 * converted to double without loss of precision, then compare
		 * as doubles.
		 */

		if ((DBL_MANT_DIG > CHAR_BIT*sizeof(long))
			|| (l1 == (long) d1) || (modf(d2, &tmp) != 0.0)) {
		    goto doubleCompare;
		}

		/*
		 * Otherwise, to make comparision based on full precision,
		 * need to convert the double to a suitably sized integer.
		 *
		 * Need this to get comparsions like
		 * 	expr 20000000000000003 < 20000000000000004.0
		 * right. Converting the first argument to double will yield
		 * two double values that are equivalent within double
		 * precision.  Converting the double to an integer gets done
		 * exactly, then integer comparison can tell the difference.
		 */

		if (d2 < (double)LONG_MIN) {
		    compare = MP_GT;
		    break;
		}
		if (d2 > (double)LONG_MAX) {
		    compare = MP_LT;
		    break;
		}
		l2 = (long) d2;
		goto longCompare;
	    case TCL_NUMBER_BIG:
		if (Tcl_IsShared(value2Ptr)) {
		    Tcl_GetBignumFromObj(NULL, value2Ptr, &big2);
		} else {
		    Tcl_GetBignumAndClearObj(NULL, value2Ptr, &big2);
		}
		if (mp_cmp_d(&big2, 0) == MP_LT) {
		    compare = MP_GT;
		} else {
		    compare = MP_LT;
		}
		mp_clear(&big2);
	    }
	    break;

#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE:
	    w1 = *((CONST Tcl_WideInt *)ptr1);
	    switch (type2) {
	    case TCL_NUMBER_WIDE:
		w2 = *((CONST Tcl_WideInt *)ptr2);
	    wideCompare:
		compare = (w1 < w2) ? MP_LT : ((w1 > w2) ? MP_GT : MP_EQ);
		break;
	    case TCL_NUMBER_LONG:
		l2 = *((CONST long *)ptr2);
		w2 = (Tcl_WideInt)l2;
		goto wideCompare;
	    case TCL_NUMBER_DOUBLE:
		d2 = *((CONST double *)ptr2);
		d1 = (double) w1;
		if ((DBL_MANT_DIG > CHAR_BIT*sizeof(Tcl_WideInt))
			|| (w1 == (Tcl_WideInt) d1) || (modf(d2, &tmp) != 0.0)) {
		    goto doubleCompare;
		}
		if (d2 < (double)LLONG_MIN) {
		    compare = MP_GT;
		    break;
		}
		if (d2 > (double)LLONG_MAX) {
		    compare = MP_LT;
		    break;
		}
		w2 = (Tcl_WideInt) d2;
		goto wideCompare;
	    case TCL_NUMBER_BIG:
		if (Tcl_IsShared(value2Ptr)) {
		    Tcl_GetBignumFromObj(NULL, value2Ptr, &big2);
		} else {
		    Tcl_GetBignumAndClearObj(NULL, value2Ptr, &big2);
		}
		if (mp_cmp_d(&big2, 0) == MP_LT) {
		    compare = MP_GT;
		} else {
		    compare = MP_LT;
		}
		mp_clear(&big2);
	    }
	    break;
#endif

	case TCL_NUMBER_DOUBLE:
	    d1 = *((CONST double *)ptr1);
	    switch (type2) {
	    case TCL_NUMBER_DOUBLE:
		d2 = *((CONST double *)ptr2);
	    doubleCompare:
		compare = (d1 < d2) ? MP_LT : ((d1 > d2) ? MP_GT : MP_EQ);
		break;
	    case TCL_NUMBER_LONG:
		l2 = *((CONST long *)ptr2);
		d2 = (double) l2;

		if ((DBL_MANT_DIG > CHAR_BIT*sizeof(long))
			|| (l2 == (long) d2) || (modf(d1, &tmp) != 0.0)) {
		    goto doubleCompare;
		}
		if (d1 < (double)LONG_MIN) {
		    compare = MP_LT;
		    break;
		}
		if (d1 > (double)LONG_MAX) {
		    compare = MP_GT;
		    break;
		}
		l1 = (long) d1;
		goto longCompare;
#ifndef NO_WIDE_TYPE
	    case TCL_NUMBER_WIDE:
		w2 = *((CONST Tcl_WideInt *)ptr2);
		d2 = (double) w2;
		if ((DBL_MANT_DIG > CHAR_BIT*sizeof(Tcl_WideInt))
			|| (w2 == (Tcl_WideInt) d2) || (modf(d1, &tmp) != 0.0)) {
		    goto doubleCompare;
		}
		if (d1 < (double)LLONG_MIN) {
		    compare = MP_LT;
		    break;
		}
		if (d1 > (double)LLONG_MAX) {
		    compare = MP_GT;
		    break;
		}
		w1 = (Tcl_WideInt) d1;
		goto wideCompare;
#endif
	    case TCL_NUMBER_BIG:
		if (TclIsInfinite(d1)) {
		    compare = (d1 > 0.0) ? MP_GT : MP_LT;
		    break;
		}
		if (Tcl_IsShared(value2Ptr)) {
		    Tcl_GetBignumFromObj(NULL, value2Ptr, &big2);
		} else {
		    Tcl_GetBignumAndClearObj(NULL, value2Ptr, &big2);
		}
		if ((d1 < (double)LONG_MAX) && (d1 > (double)LONG_MIN)) {
		    if (mp_cmp_d(&big2, 0) == MP_LT) {
			compare = MP_GT;
		    } else {
			compare = MP_LT;
		    }
		    mp_clear(&big2);
		    break;
		}
		if ((DBL_MANT_DIG > CHAR_BIT*sizeof(long))
			&& (modf(d1, &tmp) != 0.0)) {
		    d2 = TclBignumToDouble(&big2);
		    mp_clear(&big2);
		    goto doubleCompare;
		}
		Tcl_InitBignumFromDouble(NULL, d1, &big1);
		goto bigCompare;
	    }
	    break;

	case TCL_NUMBER_BIG:
	    if (Tcl_IsShared(valuePtr)) {
		Tcl_GetBignumFromObj(NULL, valuePtr, &big1);
	    } else {
		Tcl_GetBignumAndClearObj(NULL, valuePtr, &big1);
	    }
	    switch (type2) {
#ifndef NO_WIDE_TYPE
	    case TCL_NUMBER_WIDE:
#endif
	    case TCL_NUMBER_LONG:
		compare = mp_cmp_d(&big1, 0);
		mp_clear(&big1);
		break;
	    case TCL_NUMBER_DOUBLE:
		d2 = *((CONST double *)ptr2);
		if (TclIsInfinite(d2)) {
		    compare = (d2 > 0.0) ? MP_LT : MP_GT;
		    mp_clear(&big1);
		    break;
		}
		if ((d2 < (double)LONG_MAX) && (d2 > (double)LONG_MIN)) {
		    compare = mp_cmp_d(&big1, 0);
		    mp_clear(&big1);
		    break;
		}
		if ((DBL_MANT_DIG > CHAR_BIT*sizeof(long))
			&& (modf(d2, &tmp) != 0.0)) {
		    d1 = TclBignumToDouble(&big1);
		    mp_clear(&big1);
		    goto doubleCompare;
		}
		Tcl_InitBignumFromDouble(NULL, d2, &big2);
		goto bigCompare;
	    case TCL_NUMBER_BIG:
		if (Tcl_IsShared(value2Ptr)) {
		    Tcl_GetBignumFromObj(NULL, value2Ptr, &big2);
		} else {
		    Tcl_GetBignumAndClearObj(NULL, value2Ptr, &big2);
		}
	    bigCompare:
		compare = mp_cmp(&big1, &big2);
		mp_clear(&big1);
		mp_clear(&big2);
	    }
	}

	/* Turn comparison outcome into appropriate result for opcode */

    convertComparison:
	switch (*pc) {
	case INST_EQ:
	    iResult = (compare == MP_EQ);
	    break;
	case INST_NEQ:
	    iResult = (compare != MP_EQ);
	    break;
	case INST_LT:
	    iResult = (compare == MP_LT);
	    break;
	case INST_GT:
	    iResult = (compare == MP_GT);
	    break;
	case INST_LE:
	    iResult = (compare != MP_GT);
	    break;
	case INST_GE:
	    iResult = (compare != MP_LT);
	    break;
	}

	/*
	 * Peep-hole optimisation: if you're about to jump, do jump from here.
	 */

    foundResult:
	pc++;
#ifndef TCL_COMPILE_DEBUG
	switch (*pc) {
	case INST_JUMP_FALSE1:
	    NEXT_INST_F((iResult? 2 : TclGetInt1AtPtr(pc+1)), 2, 0);
	case INST_JUMP_TRUE1:
	    NEXT_INST_F((iResult? TclGetInt1AtPtr(pc+1) : 2), 2, 0);
	case INST_JUMP_FALSE4:
	    NEXT_INST_F((iResult? 5 : TclGetInt4AtPtr(pc+1)), 2, 0);
	case INST_JUMP_TRUE4:
	    NEXT_INST_F((iResult? TclGetInt4AtPtr(pc+1) : 5), 2, 0);
	}
#endif
	objResultPtr = eePtr->constants[iResult];
	NEXT_INST_F(0, 2, 1);
    }

    case INST_MOD:
    case INST_LSHIFT:
    case INST_RSHIFT: {
	Tcl_Obj *value2Ptr = *tosPtr;
	Tcl_Obj *valuePtr  = *(tosPtr - 1);
	ClientData ptr1, ptr2;
	int invalid, shift, type1, type2;
	long l1;

	result = GetNumberFromObj(NULL, valuePtr, &ptr1, &type1);
	if ((result != TCL_OK)
		|| (type1 == TCL_NUMBER_DOUBLE) || (type1 == TCL_NUMBER_NAN)) {
	    result = TCL_ERROR;
	    TRACE(("%.20s %.20s => ILLEGAL 1st TYPE %s\n", O2S(valuePtr),
		    O2S(value2Ptr), (valuePtr->typePtr?
		    valuePtr->typePtr->name : "null")));
	    IllegalExprOperandType(interp, pc, valuePtr);
	    goto checkForCatch;
	}

	result = GetNumberFromObj(NULL, value2Ptr, &ptr2, &type2);
	if ((result != TCL_OK)
		|| (type2 == TCL_NUMBER_DOUBLE) || (type2 == TCL_NUMBER_NAN)) {
	    result = TCL_ERROR;
	    TRACE(("%.20s %.20s => ILLEGAL 2nd TYPE %s\n", O2S(valuePtr),
		    O2S(value2Ptr), (value2Ptr->typePtr?
		    value2Ptr->typePtr->name : "null")));
	    IllegalExprOperandType(interp, pc, value2Ptr);
	    goto checkForCatch;
	}

	if (*pc == INST_MOD) {
	    /* TODO: Attempts to re-use unshared operands on stack */

	    long l2 = 0; /* silence gcc warning */
	    
	    if (type2 == TCL_NUMBER_LONG) {
		l2 = *((CONST long *)ptr2);
		if (l2 == 0) {
		    TRACE(("%s %s => DIVIDE BY ZERO\n", O2S(valuePtr),
			    O2S(value2Ptr)));
		    goto divideByZero;
		}
		if ((l2 == 1) || (l2 == -1)) {
		    /* Div. by |1| always yields remainder of 0 */
		    objResultPtr = eePtr->constants[0];
		    TRACE(("%s\n", O2S(objResultPtr)));
		    NEXT_INST_F(1, 2, 1);
		}
	    }
	    if (type1 == TCL_NUMBER_LONG) {
		l1 = *((CONST long *)ptr1);
		if (l1 == 0) {
		    /* 0 % (non-zero) always yields remainder of 0 */
		    objResultPtr = eePtr->constants[0];
		    TRACE(("%s\n", O2S(objResultPtr)));
		    NEXT_INST_F(1, 2, 1);
		}
		if (type2 == TCL_NUMBER_LONG) {
		    /* Both operands are long; do native calculation */
		    long lRemainder, lQuotient = l1 / l2;

		    /* Force Tcl's integer division rules */
		    /* TODO: examine for logic simplification */
		    if (((lQuotient < 0) || ((lQuotient == 0) &&
			    ((l1 < 0 && l2 > 0) || (l1 > 0 && l2 < 0)))) &&
			    ((lQuotient * l2) != l1)) {
			lQuotient -= 1;
		    }
		    lRemainder = l1 - l2*lQuotient;
		    TclNewLongObj(objResultPtr, lRemainder);
		    TRACE(("%s\n", O2S(objResultPtr)));
		    NEXT_INST_F(1, 2, 1);
		}
		/*
		 * first operand fits in long; second does not, so the second
		 * has greater magnitude than first.  No need to divide to
		 * determine the remainder.
		 */
#ifndef NO_WIDE_TYPE
		if (type2 == TCL_NUMBER_WIDE) {
		    Tcl_WideInt w2 = *((CONST Tcl_WideInt *)ptr2);

		    if ((l1 > 0) ^ (w2 > (Tcl_WideInt)0)) {
			/* Arguments are opposite sign; remainder is sum */
			objResultPtr = Tcl_NewWideIntObj(w2+(Tcl_WideInt)l1);
			TRACE(("%s\n", O2S(objResultPtr)));
			NEXT_INST_F(1, 2, 1);
		    }
		    /* Arguments are same sign; remainder is first operand */
		    TRACE(("%s\n", O2S(valuePtr)));
		    NEXT_INST_F(1, 1, 0);
		}
#endif
		{
		    mp_int big2;
		    if (Tcl_IsShared(value2Ptr)) {
			Tcl_GetBignumFromObj(NULL, value2Ptr, &big2);
		    } else {
			Tcl_GetBignumAndClearObj(NULL, value2Ptr, &big2);
		    }

		    /* TODO: internals intrusion */
		    if ((l1 > 0) ^ big2.sign) {
			/* Arguments are opposite sign; remainder is sum */
			mp_int big1;
			TclBNInitBignumFromLong(&big1, l1);
			mp_add(&big2, &big1, &big2);
			objResultPtr = Tcl_NewBignumObj(&big2);
			TRACE(("%s\n", O2S(objResultPtr)));
			NEXT_INST_F(1, 2, 1);
		    }
		    /* Arguments are same sign; remainder is first operand */
		    TRACE(("%s\n", O2S(valuePtr)));
		    NEXT_INST_F(1, 1, 0);
		}
	    }
#ifndef NO_WIDE_TYPE
	    if (type1 == TCL_NUMBER_WIDE) {
		Tcl_WideInt w1 = *((CONST Tcl_WideInt *)ptr1);
		if (type2 != TCL_NUMBER_BIG) {
		    Tcl_WideInt w2, wQuotient, wRemainder;

		    Tcl_GetWideIntFromObj(NULL, value2Ptr, &w2);
		    wQuotient = w1 / w2;

		    /* Force Tcl's integer division rules */
		    /* TODO: examine for logic simplification */
		    if (((wQuotient < ((Tcl_WideInt) 0)) 
			    || ((wQuotient == ((Tcl_WideInt) 0))
			    && ((w1 < ((Tcl_WideInt) 0)
			    && w2 > ((Tcl_WideInt) 0))
			    || (w1 > ((Tcl_WideInt) 0) 
			    && w2 < ((Tcl_WideInt) 0))))) &&
			    ((wQuotient * w2) != w1)) {
			wQuotient -= (Tcl_WideInt) 1;
		    }
		    wRemainder = w1 - w2*wQuotient;
		    objResultPtr = Tcl_NewWideIntObj(wRemainder);
		    TRACE(("%s\n", O2S(objResultPtr)));
		    NEXT_INST_F(1, 2, 1);
		}
		{
		    mp_int big2;
		    if (Tcl_IsShared(value2Ptr)) {
			Tcl_GetBignumFromObj(NULL, value2Ptr, &big2);
		    } else {
			Tcl_GetBignumAndClearObj(NULL, value2Ptr, &big2);
		    }

		    /* TODO: internals intrusion */
		    if ((w1 > ((Tcl_WideInt) 0)) ^ big2.sign) {
			/* Arguments are opposite sign; remainder is sum */
			mp_int big1;
			TclBNInitBignumFromWideInt(&big1, w1);
			mp_add(&big2, &big1, &big2);
			objResultPtr = Tcl_NewBignumObj(&big2);
			TRACE(("%s\n", O2S(objResultPtr)));
			NEXT_INST_F(1, 2, 1);
		    }
		    /* Arguments are same sign; remainder is first operand */
		    TRACE(("%s\n", O2S(valuePtr)));
		    NEXT_INST_F(1, 1, 0);
		}
	    }
#endif
	    {
		mp_int big1, big2, bigResult, bigRemainder;

		Tcl_GetBignumFromObj(NULL, valuePtr, &big1);
		Tcl_GetBignumFromObj(NULL, value2Ptr, &big2);
		mp_init(&bigResult);
		mp_init(&bigRemainder);
		mp_div(&big1, &big2, &bigResult, &bigRemainder);
		if (!mp_iszero(&bigRemainder)
			&& (bigRemainder.sign != big2.sign)) {
		    /* Convert to Tcl's integer division rules */
		    mp_sub_d(&bigResult, 1, &bigResult);
		    mp_add(&bigRemainder, &big2, &bigRemainder);
		}
		mp_copy(&bigRemainder, &bigResult);
		mp_clear(&bigRemainder);
		mp_clear(&big1);
		mp_clear(&big2);
		TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
		if (Tcl_IsShared(valuePtr)) {
		    objResultPtr = Tcl_NewBignumObj(&bigResult);
		    TRACE(("%s\n", O2S(objResultPtr)));
		    NEXT_INST_F(1, 2, 1);
		}
		Tcl_SetBignumObj(valuePtr, &bigResult);
		TRACE(("%s\n", O2S(valuePtr)));
		NEXT_INST_F(1, 1, 0);
	    }
	}

	/* reject negative shift argument */
	switch (type2) {
	case TCL_NUMBER_LONG:
	    invalid = (*((CONST long *)ptr2) < (long)0);
	    break;
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE:
	    invalid = (*((CONST Tcl_WideInt *)ptr2) < (Tcl_WideInt)0);
	    break;
#endif
	case TCL_NUMBER_BIG:
	    /* TODO: const correctness ? */
	    invalid = (mp_cmp_d((mp_int *)ptr2, 0) == MP_LT);
	    break;
	default:
	    /* Unused, here to silence compiler warning */
	    invalid = 0;
	}
	if (invalid) {
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("negative shift argument", -1));
	    result = TCL_ERROR;
	    goto checkForCatch;
	}

	/* Zero shifted any number of bits is still zero */
	if ((type1 == TCL_NUMBER_LONG) && (*((CONST long *)ptr1) == (long)0)) {
	    TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
	    objResultPtr = eePtr->constants[0];
	    TRACE(("%s\n", O2S(objResultPtr)));
	    NEXT_INST_F(1, 2, 1);
	}

	if (*pc == INST_LSHIFT) {
	    /* Large left shifts create integer overflow */
	    result = Tcl_GetIntFromObj(NULL, value2Ptr, &shift);
	    if (result != TCL_OK) {
		/*
		 * Technically, we could hold the value (1 << (INT_MAX+1)) in
		 * an mp_int, but since we're using mp_mul_2d() to do the
		 * work, and it takes only an int argument, that's a good
		 * place to draw the line.
		 */

		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"integer value too large to represent", -1));
		goto checkForCatch;
	    }
	    /* Handle shifts within the native long range */
	    TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
	    if ((type1 == TCL_NUMBER_LONG) && ((size_t)shift < CHAR_BIT*sizeof(long))
		    && (l1 = *((CONST long *)ptr1))
		    && !(((l1>0) ? l1 : ~l1)
			    & -(1L<<(CHAR_BIT*sizeof(long)-1-shift)))) {
		TclNewLongObj(objResultPtr, (l1<<shift));
		TRACE(("%s\n", O2S(objResultPtr)));
		NEXT_INST_F(1, 2, 1);
	    }

	    /* Handle shifts within the native wide range */
	    TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
	    if ((type1 != TCL_NUMBER_BIG)
		    && ((size_t)shift < CHAR_BIT*sizeof(Tcl_WideInt))) {
		Tcl_WideInt w;

		TclGetWideIntFromObj(NULL, valuePtr, &w);
		if (!(((w>0) ? w : ~w)
			& -(((Tcl_WideInt)1)
			<<(CHAR_BIT*sizeof(Tcl_WideInt)-1-shift)))) {
		    objResultPtr = Tcl_NewWideIntObj(w<<shift);
		    TRACE(("%s\n", O2S(objResultPtr)));
		    NEXT_INST_F(1, 2, 1);
		}
	    }
	} else {
	    /* Quickly force large right shifts to 0 or -1 */
	    TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
	    if ((type2 != TCL_NUMBER_LONG)
		    || (*((CONST long *)ptr2) > INT_MAX)) {
		/*
		 * Again, technically, the value to be shifted could be an
		 * mp_int so huge that a right shift by (INT_MAX+1) bits could
		 * not take us to the result of 0 or -1, but since we're using
		 * mp_div_2d to do the work, and it takes only an int
		 * argument, we draw the line there.
		 */

		int zero;

		switch (type1) {
		case TCL_NUMBER_LONG:
		    zero = (*((CONST long *)ptr1) > (long)0);
		    break;
#ifndef NO_WIDE_TYPE
		case TCL_NUMBER_WIDE:
		    zero = (*((CONST Tcl_WideInt *)ptr1) > (Tcl_WideInt)0);
		    break;
#endif
		case TCL_NUMBER_BIG:
		    /* TODO: const correctness ? */
		    zero = (mp_cmp_d((mp_int *)ptr1, 0) == MP_GT);
		    break;
		default:
		    /* Unused, here to silence compiler warning. */
		    zero = 0;
		}
		if (zero) {
		    objResultPtr = eePtr->constants[0];
		} else {
		    TclNewIntObj(objResultPtr, -1);
		}
		TRACE(("%s\n", O2S(objResultPtr)));
		NEXT_INST_F(1, 2, 1);
	    }
	    shift = (int)(*((CONST long *)ptr2));
	    /* Handle shifts within the native long range */
	    if (type1 == TCL_NUMBER_LONG) {
		l1 = *((CONST long *)ptr1);
		if ((size_t)shift >= CHAR_BIT*sizeof(long)) {
		    if (l1 >= (long)0) {
			objResultPtr = eePtr->constants[0];
		    } else {
			TclNewIntObj(objResultPtr, -1);
		    }
		} else {
		    TclNewLongObj(objResultPtr, (l1 >> shift));
		}
		TRACE(("%s\n", O2S(objResultPtr)));
		NEXT_INST_F(1, 2, 1);
	    }
#ifndef NO_WIDE_TYPE
	    /* Handle shifts within the native wide range */
	    if (type1 == TCL_NUMBER_WIDE) {
		Tcl_WideInt w = *((CONST Tcl_WideInt *)ptr1);
		if ((size_t)shift >= CHAR_BIT*sizeof(Tcl_WideInt)) {
		    if (w >= (Tcl_WideInt)0) {
			objResultPtr = eePtr->constants[0];
		    } else {
			TclNewIntObj(objResultPtr, -1);
		    }
		} else {
		    objResultPtr = Tcl_NewWideIntObj(w >> shift);
		}
		TRACE(("%s\n", O2S(objResultPtr)));
		NEXT_INST_F(1, 2, 1);
	    }
#endif
	}

	{
	    mp_int big, bigResult, bigRemainder;

	    if (Tcl_IsShared(valuePtr)) {
		Tcl_GetBignumFromObj(NULL, valuePtr, &big);
	    } else {
		Tcl_GetBignumAndClearObj(NULL, valuePtr, &big);
	    }

	    mp_init(&bigResult);
	    if (*pc == INST_LSHIFT) {
		mp_mul_2d(&big, shift, &bigResult);
	    } else {
		mp_init(&bigRemainder);
		mp_div_2d(&big, shift, &bigResult, &bigRemainder);
		if (mp_cmp_d(&bigRemainder, 0) == MP_LT) {
		    /* Convert to Tcl's integer division rules */
		    mp_sub_d(&bigResult, 1, &bigResult);
		}
		mp_clear(&bigRemainder);
	    }
	    mp_clear(&big);

	    if (!Tcl_IsShared(valuePtr)) {
		Tcl_SetBignumObj(valuePtr, &bigResult);
		TRACE(("%s\n", O2S(valuePtr)));
		NEXT_INST_F(1, 1, 0);
	    }
	    objResultPtr = Tcl_NewBignumObj(&bigResult);
	}
	TRACE(("%s\n", O2S(objResultPtr)));
	NEXT_INST_F(1, 2, 1);
    }

    case INST_BITOR:
    case INST_BITXOR:
    case INST_BITAND: {
	ClientData ptr1, ptr2;
	int type1, type2;
	Tcl_Obj *value2Ptr = *tosPtr;
	Tcl_Obj *valuePtr = *(tosPtr - 1);

	result = GetNumberFromObj(NULL, valuePtr, &ptr1, &type1);
	if ((result != TCL_OK)
		|| (type1 == TCL_NUMBER_NAN) || (type1 == TCL_NUMBER_DOUBLE)) {
	    result = TCL_ERROR;
	    TRACE(("%.20s %.20s => ILLEGAL 1st TYPE %s\n", O2S(valuePtr),
		    O2S(value2Ptr), (valuePtr->typePtr?
		    valuePtr->typePtr->name : "null")));
	    IllegalExprOperandType(interp, pc, valuePtr);
	    goto checkForCatch;
	}
	result = GetNumberFromObj(NULL, value2Ptr, &ptr2, &type2);
	if ((result != TCL_OK)
		|| (type2 == TCL_NUMBER_NAN) || (type2 == TCL_NUMBER_DOUBLE)) {
	    result = TCL_ERROR;
	    TRACE(("%.20s %.20s => ILLEGAL 2nd TYPE %s\n", O2S(valuePtr),
		    O2S(value2Ptr), (value2Ptr->typePtr?
		    value2Ptr->typePtr->name : "null")));
	    IllegalExprOperandType(interp, pc, value2Ptr);
	    goto checkForCatch;
	}

	if ((type1 == TCL_NUMBER_BIG) || (type2 == TCL_NUMBER_BIG)) {
	    mp_int big1, big2, bigResult;
	    mp_int *First, *Second;
	    int numPos;

	    if (Tcl_IsShared(valuePtr)) {
		Tcl_GetBignumFromObj(NULL, valuePtr, &big1);
	    } else {
		Tcl_GetBignumAndClearObj(NULL, valuePtr, &big1);
	    }
	    if (Tcl_IsShared(value2Ptr)) {
		Tcl_GetBignumFromObj(NULL, value2Ptr, &big2);
	    } else {
		Tcl_GetBignumAndClearObj(NULL, value2Ptr, &big2);
	    }

	    /*
	     * Count how many positive arguments we have. If only one of the
	     * arguments is negative, store it in 'Second'.
	     */

	    if (mp_cmp_d(&big1, 0) != MP_LT) {
		numPos = 1 + (mp_cmp_d(&big2, 0) != MP_LT);
		First  = &big1;
		Second = &big2;
	    } else {
		First  = &big2;
		Second = &big1;
		numPos = (mp_cmp_d(First, 0) != MP_LT);
	    }
	    mp_init(&bigResult);

	    switch (*pc) {
	    case INST_BITAND:
		switch (numPos) {
		case 2:
		    /* Both arguments positive, base case */
		    mp_and(First, Second, &bigResult);
		    break;
		case 1:
		    /* First is positive; Second negative
		     * P & N = P & ~~N = P&~(-N-1) = P & (P ^ (-N-1)) */
		    mp_neg(Second, Second);
		    mp_sub_d(Second, 1, Second);
		    mp_xor(First, Second, &bigResult);
		    mp_and(First, &bigResult, &bigResult);
		    break;
		case 0:
		    /* Both arguments negative
		     * a & b = ~ (~a | ~b) = -(-a-1|-b-1)-1 */
		    mp_neg(First, First);
		    mp_sub_d(First, 1, First);
		    mp_neg(Second, Second);
		    mp_sub_d(Second, 1, Second);
		    mp_or(First, Second, &bigResult);
		    mp_neg(&bigResult, &bigResult);
		    mp_sub_d(&bigResult, 1, &bigResult);
		    break;
		}
		break;

	    case INST_BITOR:
		switch (numPos) {
		case 2:
		    /* Both arguments positive, base case */
		    mp_or(First, Second, &bigResult);
		    break;
		case 1:
		    /* First is positive; Second negative
		     * N|P = ~(~N&~P) = ~((-N-1)&~P) = -((-N-1)&((-N-1)^P))-1 */
		    mp_neg(Second, Second);
		    mp_sub_d(Second, 1, Second);
		    mp_xor(First, Second, &bigResult);
		    mp_and(Second, &bigResult, &bigResult);
		    mp_neg(&bigResult, &bigResult);
		    mp_sub_d(&bigResult, 1, &bigResult);
		    break;
		case 0:
		    /* Both arguments negative
		     * a | b = ~ (~a & ~b) = -(-a-1&-b-1)-1 */
		    mp_neg(First, First);
		    mp_sub_d(First, 1, First);
		    mp_neg(Second, Second);
		    mp_sub_d(Second, 1, Second);
		    mp_and(First, Second, &bigResult);
		    mp_neg(&bigResult, &bigResult);
		    mp_sub_d(&bigResult, 1, &bigResult);
		    break;
		}
		break;

	    case INST_BITXOR:
		switch (numPos) {
		case 2:
		    /* Both arguments positive, base case */
		    mp_xor(First, Second, &bigResult);
		    break;
		case 1:
		    /* First is positive; Second negative
		     * P^N = ~(P^~N) = -(P^(-N-1))-1
		     */
		    mp_neg(Second, Second);
		    mp_sub_d(Second, 1, Second);
		    mp_xor(First, Second, &bigResult);
		    mp_neg(&bigResult, &bigResult);
		    mp_sub_d(&bigResult, 1, &bigResult);
		    break;
		case 0:
		    /* Both arguments negative
		     * a ^ b = (~a ^ ~b) = (-a-1^-b-1) */
		    mp_neg(First, First);
		    mp_sub_d(First, 1, First);
		    mp_neg(Second, Second);
		    mp_sub_d(Second, 1, Second);
		    mp_xor(First, Second, &bigResult);
		    break;
		}
		break;
	    }

	    mp_clear(&big1);
	    mp_clear(&big2);
	    TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
	    if (Tcl_IsShared(valuePtr)) {
		objResultPtr = Tcl_NewBignumObj(&bigResult);
		TRACE(("%s\n", O2S(objResultPtr)));
		NEXT_INST_F(1, 2, 1);
	    }
	    Tcl_SetBignumObj(valuePtr, &bigResult);
	    TRACE(("%s\n", O2S(valuePtr)));
	    NEXT_INST_F(1, 1, 0);
	}

#ifndef NO_WIDE_TYPE
	if ((type1 == TCL_NUMBER_WIDE) || (type2 == TCL_NUMBER_WIDE)) {
	    Tcl_WideInt wResult, w1, w2;
	    TclGetWideIntFromObj(NULL, valuePtr, &w1);
	    TclGetWideIntFromObj(NULL, value2Ptr, &w2);

	    switch (*pc) {
	    case INST_BITAND:
		wResult = w1 & w2;
		break;
	    case INST_BITOR:
		wResult = w1 | w2;
		break;
	    case INST_BITXOR:
		wResult = w1 ^ w2;
		break;
	    default:
		/* Unused, here to silence compiler warning. */
		wResult = 0;
	    }

	    TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
	    if (Tcl_IsShared(valuePtr)) {
		objResultPtr = Tcl_NewWideIntObj(wResult);
		TRACE(("%s\n", O2S(objResultPtr)));
		NEXT_INST_F(1, 2, 1);
	    }
	    Tcl_SetWideIntObj(valuePtr, wResult);
	    TRACE(("%s\n", O2S(valuePtr)));
	    NEXT_INST_F(1, 1, 0);
	}
#endif
	{
	    long lResult, l1 = *((CONST long *)ptr1);
	    long l2 = *((CONST long *)ptr2);

	    switch (*pc) {
	    case INST_BITAND:
		lResult = l1 & l2;
		break;
	    case INST_BITOR:
		lResult = l1 | l2;
		break;
	    case INST_BITXOR:
		lResult = l1 ^ l2;
		break;
	    default:
		/* Unused, here to silence compiler warning. */
		lResult = 0;
	    }

	    TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
	    if (Tcl_IsShared(valuePtr)) {
		TclNewLongObj(objResultPtr, lResult);
		TRACE(("%s\n", O2S(objResultPtr)));
		NEXT_INST_F(1, 2, 1);
	    }
	    TclSetLongObj(valuePtr, lResult);
	    TRACE(("%s\n", O2S(valuePtr)));
	    NEXT_INST_F(1, 1, 0);
	}
    }

#if 0
/*
 * Macro to read a string containing either a wide or an int and decide which
 * it is while decoding it at the same time.  This enforces the policy that
 * integer constants between LONG_MIN and LONG_MAX (inclusive) are represented
 * by normal longs, and integer constants outside that range are represented
 * by wide ints.
 */

#define REQUIRE_WIDE_OR_INT(resultVar, objPtr, longVar, wideVar)	\
    (resultVar) = Tcl_GetWideIntFromObj(interp, (objPtr), &(wideVar));	\
    if ((resultVar) == TCL_OK && (wideVar) >= Tcl_LongAsWide(LONG_MIN)	\
	    && (wideVar) <= Tcl_LongAsWide(LONG_MAX)) {			\
	(objPtr)->typePtr = &tclIntType;				\
	(objPtr)->internalRep.longValue = (longVar)			\
		= Tcl_WideAsLong(wideVar);				\
    }

#define W0	Tcl_LongAsWide(0)
/*
 * For tracing that uses wide values.
 */
#define LLD				"%" TCL_LL_MODIFIER "d"
    case INST_MOD:
    {
	/*
	 * Only integers are allowed. We compute value op value2.
	 */

	long i = 0, i2 = 0, rem, neg_divisor = 0;
	long iResult = 0; /* Init. avoids compiler warning. */
	Tcl_WideInt w, w2, wResult = W0;
	int doWide = 0;
	Tcl_Obj *valuePtr, *value2Ptr;

	value2Ptr = *tosPtr;
	valuePtr  = *(tosPtr - 1);
	if (valuePtr->typePtr == &tclIntType) {
	    i = valuePtr->internalRep.longValue;
	} else if (valuePtr->typePtr == &tclWideIntType) {
	    TclGetWide(w,valuePtr);
	} else {	/* try to convert to int */
	    REQUIRE_WIDE_OR_INT(result, valuePtr, i, w);
	    if (result != TCL_OK) {
		TRACE(("%.20s %.20s => ILLEGAL 1st TYPE %s\n",
			O2S(valuePtr), O2S(value2Ptr),
			(valuePtr->typePtr?
			    valuePtr->typePtr->name : "null")));
		IllegalExprOperandType(interp, pc, valuePtr);
		goto checkForCatch;
	    }
	}
	if (value2Ptr->typePtr == &tclIntType) {
	    i2 = value2Ptr->internalRep.longValue;
	} else if (value2Ptr->typePtr == &tclWideIntType) {
	    TclGetWide(w2,value2Ptr);
	} else {
	    REQUIRE_WIDE_OR_INT(result, value2Ptr, i2, w2);
	    if (result != TCL_OK) {
		TRACE(("%.20s %.20s => ILLEGAL 2nd TYPE %s\n",
			O2S(valuePtr), O2S(value2Ptr),
			(value2Ptr->typePtr?
			    value2Ptr->typePtr->name : "null")));
		IllegalExprOperandType(interp, pc, value2Ptr);
		goto checkForCatch;
	    }
	}

	do {
	    /*
	     * This code is tricky: C doesn't guarantee much about the
	     * quotient or remainder, and results with a negative divisor are
	     * not specified. Tcl guarantees that the remainder will have the
	     * same sign as the divisor and a smaller absolute value.
	     */

	    if (value2Ptr->typePtr == &tclWideIntType && w2 == W0) {
		if (valuePtr->typePtr == &tclIntType) {
		    TRACE(("%ld "LLD" => DIVIDE BY ZERO\n", i, w2));
		} else {
		    TRACE((LLD" "LLD" => DIVIDE BY ZERO\n", w, w2));
		}
		goto divideByZero;
	    }
	    if (value2Ptr->typePtr == &tclIntType && i2 == 0) {
		if (valuePtr->typePtr == &tclIntType) {
		    TRACE(("%ld %ld => DIVIDE BY ZERO\n", i, i2));
		} else {
		    TRACE((LLD" %ld => DIVIDE BY ZERO\n", w, i2));
		}
		goto divideByZero;
	    }
	    if (valuePtr->typePtr == &tclWideIntType
		    || value2Ptr->typePtr == &tclWideIntType) {
		Tcl_WideInt wRemainder;
		/*
		 * Promote to wide
		 */
		if (valuePtr->typePtr == &tclIntType) {
		    w = Tcl_LongAsWide(i);
		} else if (value2Ptr->typePtr == &tclIntType) {
		    w2 = Tcl_LongAsWide(i2);
		}
		if (w == LLONG_MIN && w2 == -1) {
		    /*
		     * Integer overflow could happen with (LLONG_MIN % -1)
		     * even though it is not possible in the code below.
		     */

		    wRemainder = 0;
		} else if (w == LLONG_MIN && w2 == LLONG_MAX) {
		    wRemainder = LLONG_MAX - 1;
		} else if (w2 == LLONG_MIN) {
		    /*
		     * In C, a modulus operation is not well defined when the
		     * divisor is a negative number. So w % LLONG_MIN is not
		     * well defined in the code below because -LLONG_MIN is
		     * still a negative number.
		     */

		    if (w == 0 || w == LLONG_MIN) {
			wRemainder = 0;
		    } else if (w < 0) {
			wRemainder = w;
		    } else {
			wRemainder = LLONG_MIN + w;
		    }
		    neg_divisor = 1;
		} else {
		    if (w2 < 0) {
			w2 = -w2;
			w = -w; /* Note: -LLONG_MIN == LLONG_MIN */
			neg_divisor = 1;
		    }
		    wRemainder = w % w2;

		    /*
		     * remainder is (remainder + divisor) when the remainder
		     * is negative. Watch out for the special case of a
		     * LLONG_MIN dividend and a negative divisor. Don't add
		     * the divisor in that case because the remainder should
		     * not be negative.
		     */

		    if (wRemainder < 0 && !(neg_divisor && (w == LLONG_MIN))) {
			wRemainder += w2;
		    }
		}
		if ((neg_divisor && (wRemainder > 0)) ||
			(!neg_divisor && (wRemainder < 0))) {
		    wRemainder = -wRemainder;
		}
		wResult = wRemainder;
		doWide = 1;
		break;
	    }

	    if (i == LONG_MIN && i2 == -1) {
		/*
		 * Integer overflow could happen with (LONG_MIN % -1) even
		 * though it is not possible in the code below.
		 */

		rem = 0;
	    } else if (i == LONG_MIN && i2 == LONG_MAX) {
		rem = LONG_MAX - 1;
	    } else if (i2 == LONG_MIN) {
		/*
		 * In C, a modulus operation is not well defined when the
		 * divisor is a negative number. So i % LONG_MIN is not well
		 * defined in the code below because -LONG_MIN is still a
		 * negative number.
		 */

		if (i == 0 || i == LONG_MIN) {
		    rem = 0;
		} else if (i < 0) {
		    rem = i;
		} else {
		    rem = LONG_MIN + i;
		}
		neg_divisor = 1;
	    } else {
		if (i2 < 0) {
		    i2 = -i2;
		    i = -i; /* Note: -LONG_MIN == LONG_MIN */
		    neg_divisor = 1;
		}
		rem = i % i2;

		/*
		 * remainder is (remainder + divisor) when the remainder is
		 * negative. Watch out for the special case of a LONG_MIN
		 * dividend and a negative divisor. Don't add the divisor in
		 * that case because the remainder should not be negative.
		 */

		if (rem < 0 && !(neg_divisor && (i == LONG_MIN))) {
		    rem += i2;
		}
	    }

	    if ((neg_divisor && (rem > 0)) ||
		    (!neg_divisor && (rem < 0))) {
		rem = -rem;
	    }
	    iResult = rem;
	} while (0);

	/*
	 * Reuse the valuePtr object already on stack if possible.
	 */

	if (Tcl_IsShared(valuePtr)) {
	    if (doWide) {
		TclNewWideIntObj(objResultPtr, wResult);
		TRACE((LLD" "LLD" => "LLD"\n", w, w2, wResult));
	    } else {
		TclNewLongObj(objResultPtr, iResult);
		TRACE(("%ld %ld => %ld\n", i, i2, iResult));
	    }
	    NEXT_INST_F(1, 2, 1);
	} else {	/* reuse the valuePtr object */
	    if (doWide) {
		TRACE((LLD" "LLD" => "LLD"\n", w, w2, wResult));
		TclSetWideIntObj(valuePtr, wResult);
	    } else {
		TRACE(("%ld %ld => %ld\n", i, i2, iResult));
		TclSetLongObj(valuePtr, iResult);
	    }
	    NEXT_INST_F(1, 1, 0);
	}
    }
#endif

    case INST_EXPON:
    case INST_ADD:
    case INST_SUB:
    case INST_DIV:
    case INST_MULT: {
	ClientData ptr1, ptr2;
	int type1, type2;
	Tcl_Obj *value2Ptr = *tosPtr;
	Tcl_Obj *valuePtr = *(tosPtr - 1);

	result = GetNumberFromObj(NULL, valuePtr, &ptr1, &type1);
	if ((result != TCL_OK)
#ifndef ACCEPT_NAN
		|| (type1 == TCL_NUMBER_NAN)
#endif
		) {
	    result = TCL_ERROR;
	    TRACE(("%.20s %.20s => ILLEGAL 1st TYPE %s\n",
		    O2S(value2Ptr), O2S(valuePtr),
		    (valuePtr->typePtr? valuePtr->typePtr->name: "null")));
	    IllegalExprOperandType(interp, pc, valuePtr);
	    goto checkForCatch;
	}

#ifdef ACCEPT_NAN
	if (type1 == TCL_NUMBER_NAN) {
	    /* NaN first argument -> result is also NaN */
	    NEXT_INST_F(1, 1, 0);
	}
#endif

	result = GetNumberFromObj(NULL, value2Ptr, &ptr2, &type2);
	if ((result != TCL_OK)
#ifndef ACCEPT_NAN
		|| (type2 == TCL_NUMBER_NAN)
#endif
		) {
	    result = TCL_ERROR;
	    TRACE(("%.20s %.20s => ILLEGAL 2nd TYPE %s\n",
		    O2S(value2Ptr), O2S(valuePtr),
		    (value2Ptr->typePtr? value2Ptr->typePtr->name: "null")));
	    IllegalExprOperandType(interp, pc, value2Ptr);
	    goto checkForCatch;
	}

#ifdef ACCEPT_NAN
	if (type2 == TCL_NUMBER_NAN) {
	    /* NaN second argument -> result is also NaN */
	    objResultPtr = value2Ptr;
	    NEXT_INST_F(1, 2, 1);
	}
#endif

	if ((type1 == TCL_NUMBER_DOUBLE) || (type2 == TCL_NUMBER_DOUBLE)) {
	    /* At least one of the values is floating-point, so perform
	     * floating point calculations */
	    double d1, d2, dResult;
	    Tcl_GetDoubleFromObj(NULL, valuePtr, &d1);
	    Tcl_GetDoubleFromObj(NULL, value2Ptr, &d2);

	    switch (*pc) {
	    case INST_ADD:
		dResult = d1 + d2;
		break;
	    case INST_SUB:
		dResult = d1 - d2;
		break;
	    case INST_MULT:
		dResult = d1 * d2;
		break;
	    case INST_DIV:
#ifndef IEEE_FLOATING_POINT
		if (d2 == 0.0) {
		    TRACE(("%.6g %.6g => DIVIDE BY ZERO\n", d1, d2));
		    goto divideByZero;
		}
#endif
		/*
		 * We presume that we are running with zero-divide unmasked if
		 * we're on an IEEE box. Otherwise, this statement might cause
		 * demons to fly out our noses.
		 */

		dResult = d1 / d2;
		break;
	    case INST_EXPON:
		if (d1==0.0 && d2<0.0) {
		    TRACE(("%.6g %.6g => EXPONENT OF ZERO\n", d1, d2));
		    goto exponOfZero;
		}
		dResult = pow(d1, d2);
		break;
	    default:
		/* Unused, here to silence compiler warning. */
		dResult = 0;
	    }

#ifndef ACCEPT_NAN
	    /*
	     * Check now for IEEE floating-point error.
	     */

	    if (TclIsNaN(dResult)) {
		TRACE(("%.20s %.20s => IEEE FLOATING PT ERROR\n",
			O2S(valuePtr), O2S(value2Ptr)));
		TclExprFloatError(interp, dResult);
		result = TCL_ERROR;
		goto checkForCatch;
	    }
#endif
	    TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
	    if (Tcl_IsShared(valuePtr)) {
		TclNewDoubleObj(objResultPtr, dResult);
		TRACE(("%s\n", O2S(objResultPtr)));
		NEXT_INST_F(1, 2, 1);
	    }
	    TclSetDoubleObj(valuePtr, dResult);
	    TRACE(("%s\n", O2S(valuePtr)));
	    NEXT_INST_F(1, 1, 0);
	}

	if ((sizeof(long) >= 2*sizeof(int)) && (*pc == INST_MULT)
		&& (type1 == TCL_NUMBER_LONG) && (type2 == TCL_NUMBER_LONG)) {
	    long l1 = *((CONST long *)ptr1);
	    long l2 = *((CONST long *)ptr2);
	    if ((l1 <= INT_MAX) && (l1 >= INT_MIN)
		    && (l2 <= INT_MAX) && (l2 >= INT_MIN)) {
		long lResult = l1 * l2;

		TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
		if (Tcl_IsShared(valuePtr)) {
		    TclNewLongObj(objResultPtr,lResult);
		    TRACE(("%s\n", O2S(objResultPtr)));
		    NEXT_INST_F(1, 2, 1);
		}
		TclSetLongObj(valuePtr, lResult);
		TRACE(("%s\n", O2S(valuePtr)));
		NEXT_INST_F(1, 1, 0);
	    }
	}

	if ((sizeof(Tcl_WideInt) >= 2*sizeof(long)) && (*pc == INST_MULT)
		&& (type1 == TCL_NUMBER_LONG) && (type2 == TCL_NUMBER_LONG)) {
	    Tcl_WideInt w1, w2, wResult;
	    TclGetWideIntFromObj(NULL, valuePtr, &w1);
	    TclGetWideIntFromObj(NULL, value2Ptr, &w2);

	    wResult = w1 * w2;

	    TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
	    if (Tcl_IsShared(valuePtr)) {
		objResultPtr = Tcl_NewWideIntObj(wResult);
		TRACE(("%s\n", O2S(objResultPtr)));
		NEXT_INST_F(1, 2, 1);
	    }
	    Tcl_SetWideIntObj(valuePtr, wResult);
	    TRACE(("%s\n", O2S(valuePtr)));
	    NEXT_INST_F(1, 1, 0);
	}

	/* TODO: Attempts to re-use unshared operands on stack */
	if (*pc == INST_EXPON) {
	    long l1, l2 = 0;
	    int oddExponent = 0, negativeExponent = 0;
	    if (type2 == TCL_NUMBER_LONG) {
		l2 = *((CONST long *)ptr2);
		if (l2 == 0) {
		    /* Anything to the zero power is 1 */
		    objResultPtr = eePtr->constants[1];
		    NEXT_INST_F(1, 2, 1);
		}
	    }
	    switch (type2) {
	    case TCL_NUMBER_LONG: {
		negativeExponent = (l2 < 0);
		oddExponent = (int) (l2 & 1);
		break;
	    }
#ifndef NO_WIDE_TYPE
	    case TCL_NUMBER_WIDE: {
		Tcl_WideInt w2 = *((CONST Tcl_WideInt *)ptr2);
		negativeExponent = (w2 < 0);
		oddExponent = (int) (w2 & (Tcl_WideInt)1);
		break;
	    }
#endif
	    case TCL_NUMBER_BIG: {
		mp_int big2;
		if (Tcl_IsShared(value2Ptr)) {
		    Tcl_GetBignumFromObj(NULL, value2Ptr, &big2);
		} else {
		    Tcl_GetBignumAndClearObj(NULL, value2Ptr, &big2);
		}
		negativeExponent = (mp_cmp_d(&big2, 0) == MP_LT);
		mp_mod_2d(&big2, 1, &big2);
		oddExponent = !mp_iszero(&big2);
		mp_clear(&big2);
		break;
	    }
	    }

	    if (negativeExponent) {
		if (type1 == TCL_NUMBER_LONG) {
		    l1 = *((CONST long *)ptr1);
		    switch (l1) {
		    case 0:
			/* zero to a negative power is div by zero error */
			TRACE(("%s %s => EXPONENT OF ZERO\n", O2S(valuePtr),
			    O2S(value2Ptr)));
			goto exponOfZero;
		    case -1:
			if (oddExponent) {
			    TclNewIntObj(objResultPtr, -1);
			} else {
			    objResultPtr = eePtr->constants[1];
			}
			NEXT_INST_F(1, 2, 1);
		    case 1:
			/* 1 to any power is 1 */
			objResultPtr = eePtr->constants[1];
			NEXT_INST_F(1, 2, 1);
		    }
		}
		/* Integers with magnitude greater than 1 raise to a negative
		 * power yield the answer zero (see TIP 123) */
		objResultPtr = eePtr->constants[0];
		NEXT_INST_F(1, 2, 1);
	    }

	    if (type1 == TCL_NUMBER_LONG) {
		l1 = *((CONST long *)ptr1);
		switch (l1) {
		case 0:
		    /* zero to a positive power is zero */
		    objResultPtr = eePtr->constants[0];
		    NEXT_INST_F(1, 2, 1);
		case 1:
		    /* 1 to any power is 1 */
		    objResultPtr = eePtr->constants[1];
		    NEXT_INST_F(1, 2, 1);
		case -1:
		    if (oddExponent) {
			TclNewIntObj(objResultPtr, -1);
		    } else {
			objResultPtr = eePtr->constants[1];
		    }
		    NEXT_INST_F(1, 2, 1);
		}
	    }
	    /* TODO: Perform those computations that fit in native types */
	    goto overflow;
	}

	if ((*pc != INST_MULT)
		&& (type1 != TCL_NUMBER_BIG) && (type2 != TCL_NUMBER_BIG)) {
	    Tcl_WideInt w1, w2, wResult;
	    TclGetWideIntFromObj(NULL, valuePtr, &w1);
	    TclGetWideIntFromObj(NULL, value2Ptr, &w2);

	    switch (*pc) {
	    case INST_ADD:
		wResult = w1 + w2;
#ifndef NO_WIDE_TYPE
		if ((type1 == TCL_NUMBER_WIDE) || (type2 == TCL_NUMBER_WIDE))
#endif
		{
		    /* Check for overflow */
		    if (((w1 < 0) && (w2 < 0) && (wResult > 0))
			    || ((w1 > 0) && (w2 > 0) && (wResult < 0))) {
			goto overflow;
		    }
		}
		break;

	    case INST_SUB:
		wResult = w1 - w2;
#ifndef NO_WIDE_TYPE
		if ((type1 == TCL_NUMBER_WIDE) || (type2 == TCL_NUMBER_WIDE))
#endif
		{
		    /* Must check for overflow */
		    if (((w1 < 0) && (w2 > 0) && (wResult > 0))
			    || ((w1 > 0) && (w2 < 0) && (wResult < 0))) {
			goto overflow;
		    }
		}
		break;

	    case INST_DIV:
		if (w2 == 0) {
		    TRACE(("%s %s => DIVIDE BY ZERO\n",
			    O2S(valuePtr), O2S(value2Ptr)));
		    goto divideByZero;
		}

		/* Need a bignum to represent (LLONG_MIN / -1) */
		if ((w1 == LLONG_MIN) && (w2 == -1)) {
		    goto overflow;
		}
		wResult = w1 / w2;

		/* Force Tcl's integer division rules */
		/* TODO: examine for logic simplification */
		if (((wResult < 0) || ((wResult == 0) &&
			((w1 < 0 && w2 > 0) || (w1 > 0 && w2 < 0)))) &&
			((wResult * w2) != w1)) {
		    wResult -= 1;
		}
		break;
	    default:
		/* Unused, here to silence compiler warning. */
		wResult = 0;
	    }

	    TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
	    if (Tcl_IsShared(valuePtr)) {
		objResultPtr = Tcl_NewWideIntObj(wResult);
		TRACE(("%s\n", O2S(objResultPtr)));
		NEXT_INST_F(1, 2, 1);
	    }
	    Tcl_SetWideIntObj(valuePtr, wResult);
	    TRACE(("%s\n", O2S(valuePtr)));
	    NEXT_INST_F(1, 1, 0);
	}

    overflow:
	{
	    mp_int big1, big2, bigResult, bigRemainder;
	    TRACE(("%s %s => ", O2S(valuePtr), O2S(value2Ptr)));
	    if (Tcl_IsShared(valuePtr)) {
		Tcl_GetBignumFromObj(NULL, valuePtr, &big1);
	    } else {
		Tcl_GetBignumAndClearObj(NULL, valuePtr, &big1);
	    }
	    if (Tcl_IsShared(value2Ptr)) {
		Tcl_GetBignumFromObj(NULL, value2Ptr, &big2);
	    } else {
		Tcl_GetBignumAndClearObj(NULL, value2Ptr, &big2);
	    }
	    mp_init(&bigResult);
	    switch (*pc) {
	    case INST_ADD:
		mp_add(&big1, &big2, &bigResult);
		break;
	    case INST_SUB:
		mp_sub(&big1, &big2, &bigResult);
		break;
	    case INST_MULT:
		mp_mul(&big1, &big2, &bigResult);
		break;
	    case INST_DIV:
		if (mp_iszero(&big2)) {
		    TRACE(("%s %s => DIVIDE BY ZERO\n", O2S(valuePtr),
			    O2S(value2Ptr)));
		    mp_clear(&big1);
		    mp_clear(&big2);
		    goto divideByZero;
		}
		mp_init(&bigRemainder);
		mp_div(&big1, &big2, &bigResult, &bigRemainder);
		/* TODO: internals intrusion */
		if (!mp_iszero(&bigRemainder)
			&& (bigRemainder.sign != big2.sign)) {
		    /* Convert to Tcl's integer division rules */
		    mp_sub_d(&bigResult, 1, &bigResult);
		    mp_add(&bigRemainder, &big2, &bigRemainder);
		}
		mp_clear(&bigRemainder);
		break;
	    case INST_EXPON:
		if (big2.used > 1) {
		    Tcl_SetObjResult(interp,
			    Tcl_NewStringObj("exponent too large", -1));
		    mp_clear(&big1);
		    mp_clear(&big2);
		    goto checkForCatch;
		}
		mp_expt_d(&big1, big2.dp[0], &bigResult);
		break;
	    }
	    mp_clear(&big1);
	    mp_clear(&big2);
	    if (Tcl_IsShared(valuePtr)) {
		objResultPtr = Tcl_NewBignumObj(&bigResult);
		TRACE(("%s\n", O2S(objResultPtr)));
		NEXT_INST_F(1, 2, 1);
	    }
	    Tcl_SetBignumObj(valuePtr, &bigResult);
	    TRACE(("%s\n", O2S(valuePtr)));
	    NEXT_INST_F(1, 1, 0);
	}
    }

    case INST_LNOT: {
	int b;
	Tcl_Obj *valuePtr = *tosPtr;

	/* TODO - check claim that taking address of b harms performance */
	/* TODO - consider optimization search for eePtr->constants */
	result = TclGetBooleanFromObj(NULL, valuePtr, &b);
	if (result != TCL_OK) {
	    TRACE(("\"%.20s\" => ILLEGAL TYPE %s\n", O2S(valuePtr),
		    (valuePtr->typePtr? valuePtr->typePtr->name : "null")));
	    IllegalExprOperandType(interp, pc, valuePtr);
	    goto checkForCatch;
	}
	/* TODO: Consider peephole opt. */
	objResultPtr = eePtr->constants[!b];
	NEXT_INST_F(1, 1, 1);
    }

    case INST_BITNOT: {
	mp_int big;
	ClientData ptr;
	int type;
	Tcl_Obj *valuePtr = *tosPtr;

	result = GetNumberFromObj(NULL, valuePtr, &ptr, &type);
	if ((result != TCL_OK)
		|| (type == TCL_NUMBER_NAN) || (type == TCL_NUMBER_DOUBLE)) {
	    /* ... ~$NonInteger => raise an error */
	    result = TCL_ERROR;
	    TRACE(("\"%.20s\" => ILLEGAL TYPE %s \n", O2S(valuePtr),
		    (valuePtr->typePtr? valuePtr->typePtr->name : "null")));
	    IllegalExprOperandType(interp, pc, valuePtr);
	    goto checkForCatch;
	}
	if (type == TCL_NUMBER_LONG) {
	    long l = *((CONST long *)ptr);
	    if (Tcl_IsShared(valuePtr)) {
		TclNewLongObj(objResultPtr, ~l);
		NEXT_INST_F(1, 1, 1);
	    }
	    TclSetLongObj(valuePtr, ~l);
	    NEXT_INST_F(1, 0, 0);
	}
#ifndef NO_WIDE_TYPE
	if (type == TCL_NUMBER_WIDE) {
	    Tcl_WideInt w = *((CONST Tcl_WideInt *)ptr);
	    if (Tcl_IsShared(valuePtr)) {
		objResultPtr = Tcl_NewWideIntObj(~w);
		NEXT_INST_F(1, 1, 1);
	    }
	    Tcl_SetWideIntObj(valuePtr, ~w);
	    NEXT_INST_F(1, 0, 0);
	}
#endif
	if (Tcl_IsShared(valuePtr)) {
	    Tcl_GetBignumFromObj(NULL, valuePtr, &big);
	} else {
	    Tcl_GetBignumAndClearObj(NULL, valuePtr, &big);
	}
	/* ~a = - a - 1 */
	mp_neg(&big, &big);
	mp_sub_d(&big, 1, &big);
	if (Tcl_IsShared(valuePtr)) {
	    objResultPtr = Tcl_NewBignumObj(&big);
	    NEXT_INST_F(1, 1, 1);
	}
	Tcl_SetBignumObj(valuePtr, &big);
	NEXT_INST_F(1, 0, 0);
    }

    case INST_UMINUS: {
	ClientData ptr;
	int type;
	Tcl_Obj *valuePtr = *tosPtr;

	result = GetNumberFromObj(NULL, valuePtr, &ptr, &type);
	if ((result != TCL_OK)
#ifndef ACCEPT_NAN
		|| (type == TCL_NUMBER_NAN)
#endif
		) {
	    result = TCL_ERROR;
	    TRACE(("\"%.20s\" => ILLEGAL TYPE %s \n", O2S(valuePtr),
		    (valuePtr->typePtr? valuePtr->typePtr->name : "null")));
	    IllegalExprOperandType(interp, pc, valuePtr);
	    goto checkForCatch;
	}
	switch (type) {
	case TCL_NUMBER_DOUBLE: {
	    double d;

	    if (Tcl_IsShared(valuePtr)) {
		TclNewDoubleObj(objResultPtr, -(*((CONST double *)ptr)));
		NEXT_INST_F(1, 1, 1);
	    }
	    d = *((CONST double *)ptr);
	    TclSetDoubleObj(valuePtr, -d);
	    NEXT_INST_F(1, 0, 0);
	}
	case TCL_NUMBER_LONG: {
	    long l = *((CONST long *)ptr);
	    if (l != LONG_MIN) {
		if (Tcl_IsShared(valuePtr)) {
		    TclNewLongObj(objResultPtr, -l);
		    NEXT_INST_F(1, 1, 1);
		}
		TclSetLongObj(valuePtr, -l);
		NEXT_INST_F(1, 0, 0);
	    }
	    /* FALLTHROUGH */
	}
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE: {
	    Tcl_WideInt w;
	    if (type == TCL_NUMBER_LONG) {
		w = (Tcl_WideInt)(*((CONST long *)ptr));
	    } else {
		w = *((CONST Tcl_WideInt *)ptr);
	    }
	    if (w != LLONG_MIN) {
		if (Tcl_IsShared(valuePtr)) {
		    objResultPtr = Tcl_NewWideIntObj(-w);
		    NEXT_INST_F(1, 1, 1);
		}
		Tcl_SetWideIntObj(valuePtr, -w);
		NEXT_INST_F(1, 0, 0);
	    }
	    /* FALLTHROUGH */
	}
#endif
	case TCL_NUMBER_BIG: {
	    mp_int big;
	    switch (type) {
#ifdef NO_WIDE_TYPE
	    case TCL_NUMBER_LONG:
		TclBNInitBignumFromLong(&big, *((CONST long *)ptr));
		break;
#else
	    case TCL_NUMBER_WIDE:
		TclBNInitBignumFromWideInt(&big, *((CONST Tcl_WideInt*)ptr));
		break;
#endif
	    case TCL_NUMBER_BIG:
		if (Tcl_IsShared(valuePtr)) {
		    Tcl_GetBignumFromObj(NULL, valuePtr, &big);
		} else {
		    Tcl_GetBignumAndClearObj(NULL, valuePtr, &big);
		}
	    }
	    mp_neg(&big, &big);
	    if (Tcl_IsShared(valuePtr)) {
		objResultPtr = Tcl_NewBignumObj(&big);
		NEXT_INST_F(1, 1, 1);
	    }
	    Tcl_SetBignumObj(valuePtr, &big);
	    NEXT_INST_F(1, 0, 0);
	}
	case TCL_NUMBER_NAN:
	    /* -NaN => NaN */
	    NEXT_INST_F(1, 0, 0);
	}
    }

    case INST_CALL_BUILTIN_FUNC1: {
	Tcl_Panic("TclExecuteByteCode: obsolete INST_CALL_BUILTIN_FUNC1 found");
    }

    case INST_CALL_FUNC1: {
	Tcl_Panic("TclExecuteByteCode: obsolete INST_CALL_FUNC1 found");
    }

    case INST_UPLUS:
    case INST_TRY_CVT_TO_NUMERIC: {
	/*
	 * Try to convert the topmost stack object to numeric object.
	 * This is done in order to support [expr]'s policy of interpreting
	 * operands if at all possible as numbers first, then strings.
	 */

	ClientData ptr;
	int type;
	Tcl_Obj *valuePtr = *tosPtr;

	if (GetNumberFromObj(NULL, valuePtr, &ptr, &type) != TCL_OK) {
	    if (*pc == INST_UPLUS) {
		/* ... +$NonNumeric => raise an error */
		result = TCL_ERROR;
		TRACE(("\"%.20s\" => ILLEGAL TYPE %s \n", O2S(valuePtr),
			(valuePtr->typePtr? valuePtr->typePtr->name : "null")));
		IllegalExprOperandType(interp, pc, valuePtr);
		goto checkForCatch;
	    } else {
		/* ... TryConvertToNumeric($NonNumeric) is acceptable */
		TRACE(("\"%.20s\" => not numeric\n", O2S(valuePtr)));
		NEXT_INST_F(1, 0, 0);
	    }
	}
#ifndef ACCEPT_NAN
	if (type == TCL_NUMBER_NAN) {
	    result = TCL_ERROR;
	    if (*pc == INST_UPLUS) {
		/* ... +$NonNumeric => raise an error */
		TRACE(("\"%.20s\" => ILLEGAL TYPE %s \n", O2S(valuePtr),
			(valuePtr->typePtr? valuePtr->typePtr->name : "null")));
		IllegalExprOperandType(interp, pc, valuePtr);
	    } else {
		/* Numeric conversion of NaN -> error */
		TRACE(("\"%.20s\" => IEEE FLOATING PT ERROR\n",
			O2S(objResultPtr)));
		TclExprFloatError(interp, *((CONST double *)ptr));
	    }
	    goto checkForCatch;
	}
#endif

	/*
	 * Ensure that the numeric value has a string rep the same as
	 * the formatted version of its internal rep. This is used, e.g.,
	 * to make sure that "expr {0001}" yields "1", not "0001".
	 * We implement this by _discarding_ the string rep since we
	 * know it will be regenerated, if needed later, by formatting
	 * the internal rep's value.
	 */
	if (valuePtr->bytes == NULL) {
	    TRACE(("\"%.20s\" => numeric, same Tcl_Obj\n", O2S(valuePtr)));
	    NEXT_INST_F(1, 0, 0);
	}
	if (Tcl_IsShared(valuePtr)) {
	    /*
	     * Here we do some surgery within the Tcl_Obj internals.
	     * We want to copy the intrep, but not the string, so we
	     * temporarily hide the string so we do not copy it.
	     */
	    char *savedString = valuePtr->bytes;
	    valuePtr->bytes = NULL;
	    objResultPtr = Tcl_DuplicateObj(valuePtr);
	    valuePtr->bytes = savedString;
	    TRACE(("\"%.20s\" => numeric, new Tcl_Obj\n", O2S(valuePtr)));
	    NEXT_INST_F(1, 1, 1);
	}
	TclInvalidateStringRep(valuePtr);
	TRACE(("\"%.20s\" => numeric, same Tcl_Obj\n", O2S(valuePtr)));
	NEXT_INST_F(1, 0, 0);
    }

    case INST_BREAK:
	/*
	DECACHE_STACK_INFO();
	Tcl_ResetResult(interp);
	CACHE_STACK_INFO();
	*/
	result = TCL_BREAK;
	cleanup = 0;
	goto processExceptionReturn;

    case INST_CONTINUE:
	/*
	DECACHE_STACK_INFO();
	Tcl_ResetResult(interp);
	CACHE_STACK_INFO();
	*/
	result = TCL_CONTINUE;
	cleanup = 0;
	goto processExceptionReturn;

    case INST_FOREACH_START4: {
	/*
	 * Initialize the temporary local var that holds the count of the
	 * number of iterations of the loop body to -1.
	 */

	int opnd;
	ForeachInfo *infoPtr;
	int iterTmpIndex;
	Var *iterVarPtr;
	Tcl_Obj *oldValuePtr;

	opnd = TclGetUInt4AtPtr(pc+1);
	infoPtr = (ForeachInfo *) codePtr->auxDataArrayPtr[opnd].clientData;
	iterTmpIndex = infoPtr->loopCtTemp;
	iterVarPtr = &(compiledLocals[iterTmpIndex]);
	oldValuePtr = iterVarPtr->value.objPtr;

	if (oldValuePtr == NULL) {
	    TclNewLongObj(iterVarPtr->value.objPtr, -1);
	    Tcl_IncrRefCount(iterVarPtr->value.objPtr);
	} else {
	    TclSetLongObj(oldValuePtr, -1);
	}
	TclSetVarScalar(iterVarPtr);
	TclClearVarUndefined(iterVarPtr);
	TRACE(("%u => loop iter count temp %d\n", opnd, iterTmpIndex));

#ifndef TCL_COMPILE_DEBUG
	/*
	 * Remark that the compiler ALWAYS sets INST_FOREACH_STEP4 immediately
	 * after INST_FOREACH_START4 - let us just fall through instead of
	 * jumping back to the top.
	 */

	pc += 5;
#else
	NEXT_INST_F(5, 0, 0);
#endif
    }

    case INST_FOREACH_STEP4: {
	/*
	 * "Step" a foreach loop (i.e., begin its next iteration) by assigning
	 * the next value list element to each loop var.
	 */

	int opnd;
	ForeachInfo *infoPtr;
	ForeachVarList *varListPtr;
	int numLists;
	Tcl_Obj *listPtr,*valuePtr, *value2Ptr;
	Tcl_Obj **elements;
	Var *iterVarPtr, *listVarPtr;
	int iterNum, listTmpIndex, listLen, numVars;
	int varIndex, valIndex, continueLoop, j;
	long i;
	Var *varPtr;
	char *part1;

	opnd = TclGetUInt4AtPtr(pc+1);
	infoPtr = (ForeachInfo *) codePtr->auxDataArrayPtr[opnd].clientData;
	numLists = infoPtr->numLists;

	/*
	 * Increment the temp holding the loop iteration number.
	 */

	iterVarPtr = &(compiledLocals[infoPtr->loopCtTemp]);
	valuePtr = iterVarPtr->value.objPtr;
	iterNum = (valuePtr->internalRep.longValue + 1);
	TclSetLongObj(valuePtr, iterNum);

	/*
	 * Check whether all value lists are exhausted and we should stop the
	 * loop.
	 */

	continueLoop = 0;
	listTmpIndex = infoPtr->firstValueTemp;
	for (i = 0;  i < numLists;  i++) {
	    varListPtr = infoPtr->varLists[i];
	    numVars = varListPtr->numVars;

	    listVarPtr = &(compiledLocals[listTmpIndex]);
	    listPtr = listVarPtr->value.objPtr;
	    result = Tcl_ListObjLength(interp, listPtr, &listLen);
	    if (result != TCL_OK) {
		TRACE_WITH_OBJ(("%u => ERROR converting list %ld, \"%s\": ",
			opnd, i, O2S(listPtr)), Tcl_GetObjResult(interp));
		goto checkForCatch;
	    }
	    if (listLen > (iterNum * numVars)) {
		continueLoop = 1;
	    }
	    listTmpIndex++;
	}

	/*
	 * If some var in some var list still has a remaining list element
	 * iterate one more time. Assign to var the next element from its
	 * value list. We already checked above that each list temp holds a
	 * valid list object (by calling Tcl_ListObjLength), but cannot rely
	 * on that check remaining valid: one list could have been shimmered
	 * as a side effect of setting a traced variable.
	 */

	if (continueLoop) {
	    listTmpIndex = infoPtr->firstValueTemp;
	    for (i = 0;  i < numLists;  i++) {
		varListPtr = infoPtr->varLists[i];
		numVars = varListPtr->numVars;

		listVarPtr = &(compiledLocals[listTmpIndex]);
		listPtr = listVarPtr->value.objPtr;
		Tcl_ListObjGetElements(interp, listPtr, &listLen, &elements);

		valIndex = (iterNum * numVars);
		for (j = 0;  j < numVars;  j++) {
		    if (valIndex >= listLen) {
			TclNewObj(valuePtr);
		    } else {
			valuePtr = elements[valIndex];
		    }

		    varIndex = varListPtr->varIndexes[j];
		    varPtr = &(compiledLocals[varIndex]);
		    part1 = varPtr->name;
		    while (TclIsVarLink(varPtr)) {
			varPtr = varPtr->value.linkPtr;
		    }
		    if (TclIsVarDirectWritable(varPtr)) {
			value2Ptr = varPtr->value.objPtr;
			if (valuePtr != value2Ptr) {
			    if (value2Ptr != NULL) {
				TclDecrRefCount(value2Ptr);
			    } else {
				TclSetVarScalar(varPtr);
				TclClearVarUndefined(varPtr);
			    }
			    varPtr->value.objPtr = valuePtr;
			    Tcl_IncrRefCount(valuePtr);
			}
		    } else {
			DECACHE_STACK_INFO();
			value2Ptr = TclPtrSetVar(interp, varPtr, NULL, part1,
				NULL, valuePtr, TCL_LEAVE_ERR_MSG);
			CACHE_STACK_INFO();
			if (value2Ptr == NULL) {
			    TRACE_WITH_OBJ(("%u => ERROR init. index temp %d: ",
				    opnd, varIndex), Tcl_GetObjResult(interp));
			    result = TCL_ERROR;
			    goto checkForCatch;
			}
		    }
		    valIndex++;
		}
		listTmpIndex++;
	    }
	}
	TRACE(("%u => %d lists, iter %d, %s loop\n", opnd, numLists,
		iterNum, (continueLoop? "continue" : "exit")));

	/*
	 * Run-time peep-hole optimisation: the compiler ALWAYS follows
	 * INST_FOREACH_STEP4 with an INST_JUMP_FALSE. We just skip that
	 * instruction and jump direct from here.
	 */

	pc += 5;
	if (*pc == INST_JUMP_FALSE1) {
	    NEXT_INST_F((continueLoop? 2 : TclGetInt1AtPtr(pc+1)), 0, 0);
	} else {
	    NEXT_INST_F((continueLoop? 5 : TclGetInt4AtPtr(pc+1)), 0, 0);
	}
    }

    case INST_BEGIN_CATCH4:
	/*
	 * Record start of the catch command with exception range index equal
	 * to the operand. Push the current stack depth onto the special catch
	 * stack.
	 */
	eePtr->stackPtr[++catchTop] = (Tcl_Obj *) (tosPtr - eePtr->stackPtr);
	TRACE(("%u => catchTop=%d, stackTop=%d\n",
		TclGetUInt4AtPtr(pc+1), (catchTop - initCatchTop - 1),
		tosPtr - eePtr->stackPtr));
	NEXT_INST_F(5, 0, 0);

    case INST_END_CATCH:
	catchTop--;
	Tcl_ResetResult(interp);
	result = TCL_OK;
	TRACE(("=> catchTop=%d\n", (catchTop - initCatchTop - 1)));
	NEXT_INST_F(1, 0, 0);

    case INST_PUSH_RESULT:
	objResultPtr = Tcl_GetObjResult(interp);
	TRACE_WITH_OBJ(("=> "), objResultPtr);

	/*
	 * See the comments at INST_INVOKE_STK
	 */
	{
	    Tcl_Obj *newObjResultPtr;
	    TclNewObj(newObjResultPtr);
	    Tcl_IncrRefCount(newObjResultPtr);
	    iPtr->objResultPtr = newObjResultPtr;
	}

	NEXT_INST_F(1, 0, -1);

    case INST_PUSH_RETURN_CODE:
	TclNewIntObj(objResultPtr, result);
	TRACE(("=> %u\n", result));
	NEXT_INST_F(1, 0, 1);

    case INST_PUSH_RETURN_OPTIONS:
	objResultPtr = Tcl_GetReturnOptions(interp, result);
	TRACE_WITH_OBJ(("=> "), objResultPtr);
	NEXT_INST_F(1, 0, 1);

/* TODO: normalize "valPtr" to "valuePtr" */
    {
	int opnd, opnd2, allocateDict;
	Tcl_Obj *dictPtr, *valPtr;
	Var *varPtr;
	char *part1;

    case INST_DICT_GET:
	opnd = TclGetUInt4AtPtr(pc+1);
	TRACE(("%u => ", opnd));
	dictPtr = *(tosPtr - opnd);
	if (opnd > 1) {
	    dictPtr = TclTraceDictPath(interp, dictPtr, opnd-1,
		    tosPtr - (opnd-1), DICT_PATH_READ);
	    if (dictPtr == NULL) {
		TRACE_WITH_OBJ((
			"%u => ERROR tracing dictionary path into \"%s\": ",
			opnd, O2S(*(tosPtr - opnd))),
			Tcl_GetObjResult(interp));
		result = TCL_ERROR;
		cleanup = opnd + 1;
		goto checkForCatch;
	    }
	}
	result = Tcl_DictObjGet(interp, dictPtr, *tosPtr, &objResultPtr);
	if (result != TCL_OK) {
	    TRACE_WITH_OBJ((
		    "%u => ERROR reading leaf dictionary key \"%s\": ",
			opnd, O2S(dictPtr)), Tcl_GetObjResult(interp));
	    cleanup = opnd + 1;
	    goto checkForCatch;
	}
	if (objResultPtr == NULL) {
	    /*Tcl_ResetResult(interp);*/
	    Tcl_AppendResult(interp, "key \"", TclGetString(*tosPtr),
		    "\" not known in dictionary", NULL);
	    TRACE_WITH_OBJ(("%u => ERROR ", opnd), Tcl_GetObjResult(interp));
	    result = TCL_ERROR;
	    cleanup = opnd + 1;
	    goto checkForCatch;
	}
	TRACE_APPEND(("%.30s\n", O2S(objResultPtr)));
	NEXT_INST_V(5, opnd+1, 1);

    case INST_DICT_SET:
    case INST_DICT_UNSET:
    case INST_DICT_INCR_IMM:
	opnd = TclGetUInt4AtPtr(pc+1);
	opnd2 = TclGetUInt4AtPtr(pc+5);

	varPtr = &(compiledLocals[opnd2]);
	part1 = varPtr->name;
	while (TclIsVarLink(varPtr)) {
	    varPtr = varPtr->value.linkPtr;
	}
	TRACE(("%u %u => ", opnd, opnd2));
	if (TclIsVarDirectReadable(varPtr)) {
	    dictPtr = varPtr->value.objPtr;
	} else {
	    DECACHE_STACK_INFO();
	    dictPtr = TclPtrGetVar(interp, varPtr, NULL, part1, NULL, 0);
	    CACHE_STACK_INFO();
	}
	if (dictPtr == NULL) {
	    TclNewObj(dictPtr);
	    allocateDict = 1;
	} else {
	    allocateDict = Tcl_IsShared(dictPtr);
	    if (allocateDict) {
		dictPtr = Tcl_DuplicateObj(dictPtr);
	    }
	}

	switch (*pc) {
	case INST_DICT_SET:
	    cleanup = opnd + 1;
	    result = Tcl_DictObjPutKeyList(interp, dictPtr, opnd, tosPtr-opnd,
		    *tosPtr);
	    break;
	case INST_DICT_INCR_IMM:
	    cleanup = 1;
	    opnd = TclGetInt4AtPtr(pc+1);
	    result = Tcl_DictObjGet(interp, dictPtr, *tosPtr, &valPtr);
	    if (result != TCL_OK) {
		break;
	    }
	    if (valPtr == NULL) {
		Tcl_DictObjPut(NULL, dictPtr, *tosPtr, Tcl_NewIntObj(opnd));
	    } else {
		Tcl_Obj *incrPtr = Tcl_NewIntObj(opnd);
		Tcl_IncrRefCount(incrPtr);
		if (Tcl_IsShared(valPtr)) {
		    valPtr = Tcl_DuplicateObj(valPtr);
		    Tcl_DictObjPut(NULL, dictPtr, *tosPtr, valPtr);
		}
		result = TclIncrObj(interp, valPtr, incrPtr);
		if (result == TCL_OK) {
		    Tcl_InvalidateStringRep(dictPtr);
		}
		Tcl_DecrRefCount(incrPtr);
	    }
	    break;
	case INST_DICT_UNSET:
	    cleanup = opnd;
	    result = Tcl_DictObjRemoveKeyList(interp, dictPtr, opnd,
		    tosPtr - (opnd-1));
	    break;
	default:
	    cleanup = 0; /* stop compiler warning */
	    Tcl_Panic("Should not happen!");
	}

	if (result != TCL_OK) {
	    if (allocateDict) {
		Tcl_DecrRefCount(dictPtr);
	    }
	    TRACE_WITH_OBJ(("%u %u => ERROR updating dictionary: ",opnd,opnd2),
		    Tcl_GetObjResult(interp));
	    goto checkForCatch;
	}

	if (TclIsVarDirectWritable(varPtr)) {
	    if (allocateDict) {
		Tcl_Obj *oldValuePtr = varPtr->value.objPtr;

		Tcl_IncrRefCount(dictPtr);
		if (oldValuePtr != NULL) {
		    Tcl_DecrRefCount(oldValuePtr);
		} else {
		    TclSetVarScalar(varPtr);
		    TclClearVarUndefined(varPtr);
		}
		varPtr->value.objPtr = dictPtr;
	    }
	    objResultPtr = dictPtr;
	} else {
	    Tcl_IncrRefCount(dictPtr);
	    DECACHE_STACK_INFO();
	    objResultPtr = TclPtrSetVar(interp, varPtr, NULL, part1, NULL,
		    dictPtr, TCL_LEAVE_ERR_MSG);
	    CACHE_STACK_INFO();
	    Tcl_DecrRefCount(dictPtr);
	    if (objResultPtr == NULL) {
		TRACE_APPEND(("ERROR: %.30s\n",O2S(Tcl_GetObjResult(interp))));
		result = TCL_ERROR;
		goto checkForCatch;
	    }
	}
#ifndef TCL_COMPILE_DEBUG
	if (*(pc+9) == INST_POP) {
	    NEXT_INST_V(10, cleanup, 0);
	}
#endif
	TRACE_APPEND(("%.30s\n", O2S(objResultPtr)));
	NEXT_INST_V(9, cleanup, 1);

    case INST_DICT_APPEND:
    case INST_DICT_LAPPEND:
	opnd = TclGetUInt4AtPtr(pc+1);
	cleanup = 2;

	varPtr = &(compiledLocals[opnd]);
	part1 = varPtr->name;
	while (TclIsVarLink(varPtr)) {
	    varPtr = varPtr->value.linkPtr;
	}
	TRACE(("%u => ", opnd));
	if (TclIsVarDirectReadable(varPtr)) {
	    dictPtr = varPtr->value.objPtr;
	} else {
	    DECACHE_STACK_INFO();
	    dictPtr = TclPtrGetVar(interp, varPtr, NULL, part1, NULL, 0);
	    CACHE_STACK_INFO();
	}
	if (dictPtr == NULL) {
	    TclNewObj(dictPtr);
	    allocateDict = 1;
	} else {
	    allocateDict = Tcl_IsShared(dictPtr);
	    if (allocateDict) {
		dictPtr = Tcl_DuplicateObj(dictPtr);
	    }
	}

	result = Tcl_DictObjGet(interp, dictPtr, *(tosPtr - 1), &valPtr);
	if (result != TCL_OK) {
	    if (allocateDict) {
		Tcl_DecrRefCount(dictPtr);
	    }
	    goto checkForCatch;
	}

	/*
	 * Note that a non-existent key results in a NULL valPtr, which is a
	 * case handled separately below. What we *can* say at this point is
	 * that the write-back will always succeed.
	 */

	switch (*pc) {
	case INST_DICT_APPEND:
	    if (valPtr == NULL) {
		valPtr = *tosPtr;
	    } else {
		if (Tcl_IsShared(valPtr)) {
		    valPtr = Tcl_DuplicateObj(valPtr);
		}
		Tcl_AppendObjToObj(valPtr, *tosPtr);
	    }
	    break;
	case INST_DICT_LAPPEND:
	    /*
	     * More complex because list-append can fail.
	     */

	    if (valPtr == NULL) {
		valPtr = Tcl_NewListObj(1, tosPtr);
	    } else if (Tcl_IsShared(valPtr)) {
		valPtr = Tcl_DuplicateObj(valPtr);
		result = Tcl_ListObjAppendElement(interp, valPtr, *tosPtr);
		if (result != TCL_OK) {
		    Tcl_DecrRefCount(valPtr);
		    if (allocateDict) {
			Tcl_DecrRefCount(dictPtr);
		    }
		    goto checkForCatch;
		}
	    } else {
		result = Tcl_ListObjAppendElement(interp, valPtr, *tosPtr);
		if (result != TCL_OK) {
		    if (allocateDict) {
			Tcl_DecrRefCount(dictPtr);
		    }
		    goto checkForCatch;
		}
	    }
	    break;
	default:
	    Tcl_Panic("Should not happen!");
	}

	Tcl_DictObjPut(NULL, dictPtr, *(tosPtr - 1), valPtr);

	if (TclIsVarDirectWritable(varPtr)) {
	    if (allocateDict) {
		Tcl_Obj *oldValuePtr = varPtr->value.objPtr;

		Tcl_IncrRefCount(dictPtr);
		if (oldValuePtr != NULL) {
		    Tcl_DecrRefCount(oldValuePtr);
		} else {
		    TclSetVarScalar(varPtr);
		    TclClearVarUndefined(varPtr);
		}
		varPtr->value.objPtr = dictPtr;
	    }
	    objResultPtr = dictPtr;
	} else {
	    Tcl_IncrRefCount(dictPtr);
	    DECACHE_STACK_INFO();
	    objResultPtr = TclPtrSetVar(interp, varPtr, NULL, part1, NULL,
		    dictPtr, TCL_LEAVE_ERR_MSG);
	    CACHE_STACK_INFO();
	    Tcl_DecrRefCount(dictPtr);
	    if (objResultPtr == NULL) {
		TRACE_APPEND(("ERROR: %.30s\n",O2S(Tcl_GetObjResult(interp))));
		result = TCL_ERROR;
		goto checkForCatch;
	    }
	}
#ifndef TCL_COMPILE_DEBUG
	if (*(pc+5) == INST_POP) {
	    NEXT_INST_F(6, 2, 0);
	}
#endif
	TRACE_APPEND(("%.30s\n", O2S(objResultPtr)));
	NEXT_INST_F(5, 2, 1);
    }

    {
	int opnd, done;
	Tcl_Obj *statePtr, *dictPtr, *keyPtr, *valuePtr, *emptyPtr;
	Var *varPtr;
	Tcl_DictSearch *searchPtr;

    case INST_DICT_FIRST:
	opnd = TclGetUInt4AtPtr(pc+1);
	TRACE(("%u => ", opnd));
	dictPtr = POP_OBJECT();
	searchPtr = (Tcl_DictSearch *) ckalloc(sizeof(Tcl_DictSearch));
	result = Tcl_DictObjFirst(interp, dictPtr, searchPtr, &keyPtr,
		&valuePtr, &done);
	if (result != TCL_OK) {
	    ckfree((char *) searchPtr);
	    cleanup = 0;
	    goto checkForCatch;
	}
	TclNewObj(statePtr);
	statePtr->typePtr = &dictIteratorType;
	statePtr->internalRep.twoPtrValue.ptr1 = (void *) searchPtr;
	statePtr->internalRep.twoPtrValue.ptr2 = (void *) dictPtr;
	varPtr = compiledLocals + opnd;
	if (varPtr->value.objPtr == NULL) {
	    TclSetVarScalar(compiledLocals + opnd);
	    TclClearVarUndefined(compiledLocals + opnd);
	} else if (varPtr->value.objPtr->typePtr == &dictIteratorType) {
	    Tcl_Panic("mis-issued dictFirst!");
	} else {
	    Tcl_DecrRefCount(varPtr->value.objPtr);
	}
	varPtr->value.objPtr = statePtr;
	Tcl_IncrRefCount(statePtr);
	goto pushDictIteratorResult;

    case INST_DICT_NEXT:
	opnd = TclGetUInt4AtPtr(pc+1);
	TRACE(("%u => ", opnd));
	statePtr = compiledLocals[opnd].value.objPtr;
	if (statePtr == NULL || statePtr->typePtr != &dictIteratorType) {
	    Tcl_Panic("mis-issued dictNext!");
	}
	searchPtr = (Tcl_DictSearch *) statePtr->internalRep.twoPtrValue.ptr1;
	Tcl_DictObjNext(searchPtr, &keyPtr, &valuePtr, &done);
    pushDictIteratorResult:
	if (done) {
	    TclNewObj(emptyPtr);
	    PUSH_OBJECT(emptyPtr);
	    PUSH_OBJECT(emptyPtr);
	} else {
	    PUSH_OBJECT(valuePtr);
	    PUSH_OBJECT(keyPtr);
	}
	TRACE_APPEND(("\"%.30s\" \"%.30s\" %d",
		O2S(*(tosPtr-1)), O2S(*tosPtr), done));
	objResultPtr = eePtr->constants[done];
	/*TODO: consider opt like INST_FOREACH_STEP4 */
	NEXT_INST_F(5, 0, 1);

    case INST_DICT_DONE:
	opnd = TclGetUInt4AtPtr(pc+1);
	TRACE(("%u => ", opnd));
	statePtr = compiledLocals[opnd].value.objPtr;
	if (statePtr == NULL) {
	    Tcl_Panic("mis-issued dictDone!");
	}

	if (statePtr->typePtr == &dictIteratorType) {
	    /*
	     * First kill the search, and then release the reference to the
	     * dictionary that we were holding.
	     */

	    searchPtr = (Tcl_DictSearch *)
		    statePtr->internalRep.twoPtrValue.ptr1;
	    Tcl_DictObjDone(searchPtr);
	    ckfree((char *) searchPtr);

	    dictPtr = (Tcl_Obj *) statePtr->internalRep.twoPtrValue.ptr2;
	    Tcl_DecrRefCount(dictPtr);

	    /*
	     * Set the internal variable to an empty object to signify that we
	     * don't hold an iterator.
	     */

	    Tcl_DecrRefCount(statePtr);
	    TclNewObj(emptyPtr);
	    compiledLocals[opnd].value.objPtr = emptyPtr;
	    Tcl_IncrRefCount(emptyPtr);
	}
	NEXT_INST_F(5, 0, 0);
    }

    {
	int opnd, i, length, length2, allocdict;
	Tcl_Obj **keyPtrPtr, **varIdxPtrPtr, *dictPtr;
	Var *varPtr;
	char *part1;

    case INST_DICT_UPDATE_START:
	opnd = TclGetUInt4AtPtr(pc+1);
	varPtr = &(compiledLocals[opnd]);
	part1 = varPtr->name;
	while (TclIsVarLink(varPtr)) {
	    varPtr = varPtr->value.linkPtr;
	}
	TRACE(("%u => ", opnd));
	if (TclIsVarDirectReadable(varPtr)) {
	    dictPtr = varPtr->value.objPtr;
	} else {
	    DECACHE_STACK_INFO();
	    dictPtr = TclPtrGetVar(interp, varPtr, NULL, part1, NULL,
		    TCL_LEAVE_ERR_MSG);
	    CACHE_STACK_INFO();
	    if (dictPtr == NULL) {
		goto dictUpdateStartFailed;
	    }
	}
	if (Tcl_ListObjGetElements(interp, *(tosPtr - 1), &length,
			&keyPtrPtr) != TCL_OK ||
		Tcl_ListObjGetElements(interp, *tosPtr, &length2,
			&varIdxPtrPtr) != TCL_OK) {
	    goto dictUpdateStartFailed;
	}
	if (length != length2) {
	    Tcl_Panic("dictUpdateStart argument length mismatch");
	}
	for (i=0 ; i<length ; i++) {
	    Tcl_Obj *valPtr;
	    int varIdx;

	    if (Tcl_DictObjGet(interp, dictPtr, keyPtrPtr[i],
		    &valPtr) != TCL_OK) {
		goto dictUpdateStartFailed;
	    }
	    Tcl_GetIntFromObj(NULL, varIdxPtrPtr[i], &varIdx);
	    varPtr = &(compiledLocals[varIdx]);
	    part1 = varPtr->name;
	    while (TclIsVarLink(varPtr)) {
		varPtr = varPtr->value.linkPtr;
	    }
	    DECACHE_STACK_INFO();
	    if (valPtr == NULL) {
		Tcl_UnsetVar(interp, part1, 0);
	    } else if (TclPtrSetVar(interp, varPtr, NULL, part1, NULL,
		    valPtr, TCL_LEAVE_ERR_MSG) == NULL) {
		CACHE_STACK_INFO();
	    dictUpdateStartFailed:
		cleanup = 2;
		result = TCL_ERROR;
		goto checkForCatch;
	    }
	    CACHE_STACK_INFO();
	}
	NEXT_INST_F(5, 2, 0);

    case INST_DICT_UPDATE_END:
	opnd = TclGetUInt4AtPtr(pc+1);
	varPtr = &(compiledLocals[opnd]);
	part1 = varPtr->name;
	while (TclIsVarLink(varPtr)) {
	    varPtr = varPtr->value.linkPtr;
	}
	TRACE(("%u => ", opnd));
	if (TclIsVarDirectReadable(varPtr)) {
	    dictPtr = varPtr->value.objPtr;
	} else {
	    DECACHE_STACK_INFO();
	    dictPtr = TclPtrGetVar(interp, varPtr, NULL, part1, NULL, 0);
	    CACHE_STACK_INFO();
	}
	if (dictPtr == NULL) {
	    NEXT_INST_F(5, 2, 0);
	}
	if (Tcl_DictObjSize(interp, dictPtr, &length) != TCL_OK ||
		Tcl_ListObjGetElements(interp, *(tosPtr - 1), &length,
			&keyPtrPtr) != TCL_OK ||
		Tcl_ListObjGetElements(interp, *tosPtr, &length2,
			&varIdxPtrPtr) != TCL_OK) {
	    cleanup = 2;
	    result = TCL_ERROR;
	    goto checkForCatch;
	}
	allocdict = Tcl_IsShared(dictPtr);
	if (allocdict) {
	    dictPtr = Tcl_DuplicateObj(dictPtr);
	}
	for (i=0 ; i<length ; i++) {
	    Tcl_Obj *valPtr;
	    int varIdx;
	    Var *var2Ptr;
	    char *part1a;

	    Tcl_GetIntFromObj(NULL, varIdxPtrPtr[i], &varIdx);
	    var2Ptr = &(compiledLocals[varIdx]);
	    part1a = var2Ptr->name;
	    while (TclIsVarLink(var2Ptr)) {
		var2Ptr = var2Ptr->value.linkPtr;
	    }
	    if (TclIsVarDirectReadable(var2Ptr)) {
		valPtr = var2Ptr->value.objPtr;
	    } else {
		DECACHE_STACK_INFO();
		valPtr = TclPtrGetVar(interp, var2Ptr, NULL, part1a, NULL, 0);
		CACHE_STACK_INFO();
	    }
	    if (valPtr == NULL) {
		Tcl_DictObjRemove(interp, dictPtr, keyPtrPtr[i]);
	    } else {
		Tcl_DictObjPut(interp, dictPtr, keyPtrPtr[i], valPtr);
	    }
	}
	if (TclIsVarDirectWritable(varPtr)) {
	    Tcl_IncrRefCount(dictPtr);
	    Tcl_DecrRefCount(varPtr->value.objPtr);
	    varPtr->value.objPtr = dictPtr;
	} else {
	    DECACHE_STACK_INFO();
	    objResultPtr = TclPtrSetVar(interp, varPtr, NULL, part1, NULL,
		    dictPtr, TCL_LEAVE_ERR_MSG);
	    CACHE_STACK_INFO();
	    if (objResultPtr == NULL) {
		if (allocdict) {
		    Tcl_DecrRefCount(dictPtr);
		}
		cleanup = 2;
		result = TCL_ERROR;
		goto checkForCatch;
	    }
	}
	NEXT_INST_F(5, 2, 0);
    }

    default:
	Tcl_Panic("TclExecuteByteCode: unrecognized opCode %u", *pc);
    } /* end of switch on opCode */

    /*
     * Division by zero in an expression. Control only reaches this point by
     * "goto divideByZero".
     */

 divideByZero:
    Tcl_SetObjResult(interp, Tcl_NewStringObj("divide by zero", -1));
    Tcl_SetErrorCode(interp, "ARITH", "DIVZERO", "divide by zero",
	    (char *) NULL);

    result = TCL_ERROR;
    goto checkForCatch;

    /*
     * Exponentiation of zero by negative number in an expression.  Control
     * only reaches this point by "goto exponOfZero".
     */

 exponOfZero:
    Tcl_SetObjResult(interp, Tcl_NewStringObj(
	    "exponentiation of zero by negative power", -1));
    Tcl_SetErrorCode(interp, "ARITH", "DOMAIN",
	    "exponentiation of zero by negative power", (char *) NULL);
    result = TCL_ERROR;
    goto checkForCatch;

    /*
     * Block for variables needed to process exception returns
     */

    {
	ExceptionRange *rangePtr;  /* Points to closest loop or catch
				    * exception range enclosing the pc. Used
				    * by various instructions and processCatch
				    * to process break, continue, and
				    * errors. */
	Tcl_Obj *valuePtr;
	char *bytes;
	int length;
#if TCL_COMPILE_DEBUG
	int opnd;
#endif

	/*
	 * An external evaluation (INST_INVOKE or INST_EVAL) returned
	 * something different from TCL_OK, or else INST_BREAK or
	 * INST_CONTINUE were called.
	 */

    processExceptionReturn:
#if TCL_COMPILE_DEBUG
	switch (*pc) {
	case INST_INVOKE_STK1:
	    opnd = TclGetUInt1AtPtr(pc+1);
	    TRACE(("%u => ... after \"%.20s\": ", opnd, cmdNameBuf));
	    break;
	case INST_INVOKE_STK4:
	    opnd = TclGetUInt4AtPtr(pc+1);
	    TRACE(("%u => ... after \"%.20s\": ", opnd, cmdNameBuf));
	    break;
	case INST_EVAL_STK:
	    /*
	     * Note that the object at stacktop has to be used before doing
	     * the cleanup.
	     */

	    TRACE(("\"%.30s\" => ", O2S(*tosPtr)));
	    break;
	default:
	    TRACE(("=> "));
	}
#endif
	if ((result == TCL_CONTINUE) || (result == TCL_BREAK)) {
	    rangePtr = GetExceptRangeForPc(pc, /*catchOnly*/ 0, codePtr);
	    if (rangePtr == NULL) {
		TRACE_APPEND(("no encl. loop or catch, returning %s\n",
			StringForResultCode(result)));
		goto abnormalReturn;
	    }
	    if (rangePtr->type == CATCH_EXCEPTION_RANGE) {
		TRACE_APPEND(("%s ...\n", StringForResultCode(result)));
		goto processCatch;
	    }
	    while (cleanup--) {
		valuePtr = POP_OBJECT();
		TclDecrRefCount(valuePtr);
	    }
	    if (result == TCL_BREAK) {
		result = TCL_OK;
		pc = (codePtr->codeStart + rangePtr->breakOffset);
		TRACE_APPEND(("%s, range at %d, new pc %d\n",
			StringForResultCode(result),
			rangePtr->codeOffset, rangePtr->breakOffset));
		NEXT_INST_F(0, 0, 0);
	    } else {
		if (rangePtr->continueOffset == -1) {
		    TRACE_APPEND(("%s, loop w/o continue, checking for catch\n",
			    StringForResultCode(result)));
		    goto checkForCatch;
		}
		result = TCL_OK;
		pc = (codePtr->codeStart + rangePtr->continueOffset);
		TRACE_APPEND(("%s, range at %d, new pc %d\n",
			StringForResultCode(result),
			rangePtr->codeOffset, rangePtr->continueOffset));
		NEXT_INST_F(0, 0, 0);
	    }
#if TCL_COMPILE_DEBUG
	} else if (traceInstructions) {
	    if ((result != TCL_ERROR) && (result != TCL_RETURN))  {
		Tcl_Obj *objPtr = Tcl_GetObjResult(interp);
		TRACE_APPEND(("OTHER RETURN CODE %d, result= \"%s\"\n ",
			result, O2S(objPtr)));
	    } else {
		Tcl_Obj *objPtr = Tcl_GetObjResult(interp);
		TRACE_APPEND(("%s, result= \"%s\"\n",
			StringForResultCode(result), O2S(objPtr)));
	    }
#endif
	}

	/*
	 * Execution has generated an "exception" such as TCL_ERROR. If the
	 * exception is an error, record information about what was being
	 * executed when the error occurred. Find the closest enclosing catch
	 * range, if any. If no enclosing catch range is found, stop execution
	 * and return the "exception" code.
	 */

	checkForCatch:
	if ((result == TCL_ERROR) && !(iPtr->flags & ERR_ALREADY_LOGGED)) {
	    bytes = GetSrcInfoForPc(pc, codePtr, &length);
	    if (bytes != NULL) {
		Tcl_LogCommandInfo(interp, codePtr->source, bytes, length);
	    }
	}
	iPtr->flags &= ~ERR_ALREADY_LOGGED;

	/*
	 * Clear all expansions that may have started after the last
	 * INST_BEGIN_CATCH.
	 */

	while ((expandNestList != NULL) && ((catchTop == initCatchTop) ||
		((ptrdiff_t) eePtr->stackPtr[catchTop] <=
		(ptrdiff_t) expandNestList->internalRep.twoPtrValue.ptr1))) {
	    Tcl_Obj *objPtr = expandNestList->internalRep.twoPtrValue.ptr2;
	    TclDecrRefCount(expandNestList);
	    expandNestList = objPtr;
	}

	/*
	 * We must not catch an exceeded limit.  Instead, it blows outwards
	 * until we either hit another interpreter (presumably where the limit
	 * is not exceeded) or we get to the top-level.
	 */
	if (Tcl_LimitExceeded(interp)) {
#ifdef TCL_COMPILE_DEBUG
	    if (traceInstructions) {
		fprintf(stdout, "   ... limit exceeded, returning %s\n",
			StringForResultCode(result));
	    }
#endif
	    goto abnormalReturn;
	}
	if (catchTop == initCatchTop) {
#ifdef TCL_COMPILE_DEBUG
	    if (traceInstructions) {
		fprintf(stdout, "   ... no enclosing catch, returning %s\n",
			StringForResultCode(result));
	    }
#endif
	    goto abnormalReturn;
	}
	rangePtr = GetExceptRangeForPc(pc, /*catchOnly*/ 1, codePtr);
	if (rangePtr == NULL) {
	    /*
	     * This is only possible when compiling a [catch] that sends its
	     * script to INST_EVAL. Cannot correct the compiler without
	     * breakingcompat with previous .tbc compiled scripts.
	     */
#ifdef TCL_COMPILE_DEBUG
	    if (traceInstructions) {
		fprintf(stdout, "   ... no enclosing catch, returning %s\n",
			StringForResultCode(result));
	    }
#endif
	    goto abnormalReturn;
	}

	/*
	 * A catch exception range (rangePtr) was found to handle an
	 * "exception". It was found either by checkForCatch just above or by
	 * an instruction during break, continue, or error processing.  Jump
	 * to its catchOffset after unwinding the operand stack to the depth
	 * it had when starting to execute the range's catch command.
	 */

	processCatch:
	while (tosPtr > ((ptrdiff_t) (eePtr->stackPtr[catchTop])) + eePtr->stackPtr) {
	    valuePtr = POP_OBJECT();
	    TclDecrRefCount(valuePtr);
	}
#ifdef TCL_COMPILE_DEBUG
	if (traceInstructions) {
	    fprintf(stdout, "  ... found catch at %d, catchTop=%d, unwound to %d, new pc %u\n",
		    rangePtr->codeOffset, (catchTop - initCatchTop - 1),
		    (int) eePtr->stackPtr[catchTop],
		    (unsigned int)(rangePtr->catchOffset));
	}
#endif
	pc = (codePtr->codeStart + rangePtr->catchOffset);
	NEXT_INST_F(0, 0, 0); /* restart the execution loop at pc */

	/*
	 * end of infinite loop dispatching on instructions.
	 */

	/*
	 * Abnormal return code. Restore the stack to state it had when
	 * starting to execute the ByteCode. Panic if the stack is below the
	 * initial level.
	 */

	abnormalReturn:
	{
	    Tcl_Obj **initTosPtr = eePtr->stackPtr + initStackTop;
	    while (tosPtr > initTosPtr) {
		Tcl_Obj *objPtr = POP_OBJECT();
		TclDecrRefCount(objPtr);
	    }

	    /*
	     * Clear all expansions.
	     */

	    while (expandNestList) {
		Tcl_Obj *objPtr = expandNestList->internalRep.twoPtrValue.ptr2;
		TclDecrRefCount(expandNestList);
		expandNestList = objPtr;
	    }
	    if (tosPtr < initTosPtr) {
		fprintf(stderr, "\nTclExecuteByteCode: abnormal return at pc %u: stack top %d < entry stack top %d\n",
			(unsigned int)(pc - codePtr->codeStart),
			(unsigned int) (tosPtr - eePtr->stackPtr),
			(unsigned int) initStackTop);
		Tcl_Panic("TclExecuteByteCode execution failure: end stack top < start stack top");
	    }
	    eePtr->tosPtr = initTosPtr - codePtr->maxExceptDepth;
	}
    }
    return result;
#undef iPtr
}

#ifdef TCL_COMPILE_DEBUG
/*
 *----------------------------------------------------------------------
 *
 * PrintByteCodeInfo --
 *
 *	This procedure prints a summary about a bytecode object to stdout. It
 *	is called by TclExecuteByteCode when starting to execute the bytecode
 *	object if tclTraceExec has the value 2 or more.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
PrintByteCodeInfo(
    register ByteCode *codePtr)	/* The bytecode whose summary is printed to
				 * stdout. */
{
    Proc *procPtr = codePtr->procPtr;
    Interp *iPtr = (Interp *) *codePtr->interpHandle;

    fprintf(stdout, "\nExecuting ByteCode 0x%x, refCt %u, epoch %u, interp 0x%x (epoch %u)\n",
	    (unsigned int) codePtr, codePtr->refCount,
	    codePtr->compileEpoch, (unsigned int) iPtr,
	    iPtr->compileEpoch);

    fprintf(stdout, "  Source: ");
    TclPrintSource(stdout, codePtr->source, 60);

    fprintf(stdout, "\n  Cmds %d, src %d, inst %u, litObjs %u, aux %d, stkDepth %u, code/src %.2f\n",
	    codePtr->numCommands, codePtr->numSrcBytes,
	    codePtr->numCodeBytes, codePtr->numLitObjects,
	    codePtr->numAuxDataItems, codePtr->maxStackDepth,
#ifdef TCL_COMPILE_STATS
	    codePtr->numSrcBytes?
		((float)codePtr->structureSize)/codePtr->numSrcBytes :
#endif
	    0.0);

#ifdef TCL_COMPILE_STATS
    fprintf(stdout, "  Code %lu = header %lu+inst %d+litObj %lu+exc %lu+aux %lu+cmdMap %d\n",
	    (unsigned long) codePtr->structureSize,
	    (unsigned long) (sizeof(ByteCode) - (sizeof(size_t) + sizeof(Tcl_Time))),
	    codePtr->numCodeBytes,
	    (unsigned long) (codePtr->numLitObjects * sizeof(Tcl_Obj *)),
	    (unsigned long) (codePtr->numExceptRanges * sizeof(ExceptionRange)),
	    (unsigned long) (codePtr->numAuxDataItems * sizeof(AuxData)),
	    codePtr->numCmdLocBytes);
#endif /* TCL_COMPILE_STATS */
    if (procPtr != NULL) {
	fprintf(stdout,
		"  Proc 0x%x, refCt %d, args %d, compiled locals %d\n",
		(unsigned int) procPtr, procPtr->refCount,
		procPtr->numArgs, procPtr->numCompiledLocals);
    }
}
#endif /* TCL_COMPILE_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * ValidatePcAndStackTop --
 *
 *	This procedure is called by TclExecuteByteCode when debugging to
 *	verify that the program counter and stack top are valid during
 *	execution.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Prints a message to stderr and panics if either the pc or stack top
 *	are invalid.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_COMPILE_DEBUG
static void
ValidatePcAndStackTop(
    register ByteCode *codePtr,	/* The bytecode whose summary is printed to
				 * stdout. */
    unsigned char *pc,		/* Points to first byte of a bytecode
				 * instruction. The program counter. */
    int stackTop,		/* Current stack top. Must be between
				 * stackLowerBound and stackUpperBound
				 * (inclusive). */
    int stackLowerBound,	/* Smallest legal value for stackTop. */
    int checkStack)		/* 0 if the stack depth check should be
				 * skipped. */
{
    int stackUpperBound = stackLowerBound +  codePtr->maxStackDepth;
				/* Greatest legal value for stackTop. */
    unsigned int relativePc = (unsigned int) (pc - codePtr->codeStart);
    unsigned int codeStart = (unsigned int) codePtr->codeStart;
    unsigned int codeEnd = (unsigned int)
	    (codePtr->codeStart + codePtr->numCodeBytes);
    unsigned char opCode = *pc;

    if (((unsigned int) pc < codeStart) || ((unsigned int) pc > codeEnd)) {
	fprintf(stderr, "\nBad instruction pc 0x%x in TclExecuteByteCode\n",
		(unsigned int) pc);
	Tcl_Panic("TclExecuteByteCode execution failure: bad pc");
    }
    if ((unsigned int) opCode > LAST_INST_OPCODE) {
	fprintf(stderr, "\nBad opcode %d at pc %u in TclExecuteByteCode\n",
		(unsigned int) opCode, relativePc);
	Tcl_Panic("TclExecuteByteCode execution failure: bad opcode");
    }
    if (checkStack &&
	    ((stackTop < stackLowerBound) || (stackTop > stackUpperBound))) {
	int numChars;
	char *cmd = GetSrcInfoForPc(pc, codePtr, &numChars);

	fprintf(stderr, "\nBad stack top %d at pc %u in TclExecuteByteCode (min %i, max %i)",
		stackTop, relativePc, stackLowerBound, stackUpperBound);
	if (cmd != NULL) {
	    Tcl_Obj *message = Tcl_NewStringObj("\n executing ", -1);
	    Tcl_IncrRefCount(message);
	    TclAppendLimitedToObj(message, cmd, numChars, 100, NULL);
	    fprintf(stderr,"%s\n", Tcl_GetString(message));
	    Tcl_DecrRefCount(message);
	} else {
	    fprintf(stderr, "\n");
	}
	Tcl_Panic("TclExecuteByteCode execution failure: bad stack top");
    }
}
#endif /* TCL_COMPILE_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * IllegalExprOperandType --
 *
 *	Used by TclExecuteByteCode to append an error message to the interp
 *	result when an illegal operand type is detected by an expression
 *	instruction. The argument opndPtr holds the operand object in error.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An error message is appended to the interp result.
 *
 *----------------------------------------------------------------------
 */

static void
IllegalExprOperandType(
    Tcl_Interp *interp,		/* Interpreter to which error information
				 * pertains. */
    unsigned char *pc,		/* Points to the instruction being executed
				 * when the illegal type was found. */
    Tcl_Obj *opndPtr)		/* Points to the operand holding the value
				 * with the illegal type. */
{
    ClientData ptr;
    int type;
    unsigned char opcode = *pc;
    CONST char *description, *operator = operatorStrings[opcode - INST_LOR];
    Tcl_Obj *msg = Tcl_NewObj();

    if (opcode == INST_EXPON) {
	operator = "**";
    }

    if (GetNumberFromObj(NULL, opndPtr, &ptr, &type) != TCL_OK) {
	int numBytes;
	CONST char *bytes = Tcl_GetStringFromObj(opndPtr, &numBytes);
	if (numBytes == 0) {
	    description = "empty string";
	} else if (TclCheckBadOctal(NULL, bytes)) {
	    description = "invalid octal number";
	} else {
	    description = "non-numeric string";
	}
    } else if (type == TCL_NUMBER_NAN) {
	description = "non-numeric floating-point value";
    } else if (type == TCL_NUMBER_DOUBLE) {
	description = "floating-point value";
    } else {
	/* TODO: No caller needs this.  Eliminate? */
	description = "(big) integer";
    }

    TclObjPrintf(NULL, msg, "can't use %s as operand of \"%s\"",
	    description, operator);
    Tcl_SetObjResult(interp, msg);
}

/*
 *----------------------------------------------------------------------
 *
 * GetSrcInfoForPc --
 *
 *	Given a program counter value, finds the closest command in the
 *	bytecode code unit's CmdLocation array and returns information about
 *	that command's source: a pointer to its first byte and the number of
 *	characters.
 *
 * Results:
 *	If a command is found that encloses the program counter value, a
 *	pointer to the command's source is returned and the length of the
 *	source is stored at *lengthPtr. If multiple commands resulted in code
 *	at pc, information about the closest enclosing command is returned. If
 *	no matching command is found, NULL is returned and *lengthPtr is
 *	unchanged.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
GetSrcInfoForPc(
    unsigned char *pc,		/* The program counter value for which to
				 * return the closest command's source info.
				 * This points to a bytecode instruction in
				 * codePtr's code. */
    ByteCode *codePtr,		/* The bytecode sequence in which to look up
				 * the command source for the pc. */
    int *lengthPtr)		/* If non-NULL, the location where the length
				 * of the command's source should be stored.
				 * If NULL, no length is stored. */
{
    register int pcOffset = (pc - codePtr->codeStart);
    int numCmds = codePtr->numCommands;
    unsigned char *codeDeltaNext, *codeLengthNext;
    unsigned char *srcDeltaNext, *srcLengthNext;
    int codeOffset, codeLen, codeEnd, srcOffset, srcLen, delta, i;
    int bestDist = INT_MAX;	/* Distance of pc to best cmd's start pc. */
    int bestSrcOffset = -1;	/* Initialized to avoid compiler warning. */
    int bestSrcLength = -1;	/* Initialized to avoid compiler warning. */

    if ((pcOffset < 0) || (pcOffset >= codePtr->numCodeBytes)) {
	return NULL;
    }

    /*
     * Decode the code and source offset and length for each command. The
     * closest enclosing command is the last one whose code started before
     * pcOffset.
     */

    codeDeltaNext = codePtr->codeDeltaStart;
    codeLengthNext = codePtr->codeLengthStart;
    srcDeltaNext  = codePtr->srcDeltaStart;
    srcLengthNext = codePtr->srcLengthStart;
    codeOffset = srcOffset = 0;
    for (i = 0;  i < numCmds;  i++) {
	if ((unsigned int) (*codeDeltaNext) == (unsigned int) 0xFF) {
	    codeDeltaNext++;
	    delta = TclGetInt4AtPtr(codeDeltaNext);
	    codeDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(codeDeltaNext);
	    codeDeltaNext++;
	}
	codeOffset += delta;

	if ((unsigned int) (*codeLengthNext) == (unsigned int) 0xFF) {
	    codeLengthNext++;
	    codeLen = TclGetInt4AtPtr(codeLengthNext);
	    codeLengthNext += 4;
	} else {
	    codeLen = TclGetInt1AtPtr(codeLengthNext);
	    codeLengthNext++;
	}
	codeEnd = (codeOffset + codeLen - 1);

	if ((unsigned int) (*srcDeltaNext) == (unsigned int) 0xFF) {
	    srcDeltaNext++;
	    delta = TclGetInt4AtPtr(srcDeltaNext);
	    srcDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(srcDeltaNext);
	    srcDeltaNext++;
	}
	srcOffset += delta;

	if ((unsigned int) (*srcLengthNext) == (unsigned int) 0xFF) {
	    srcLengthNext++;
	    srcLen = TclGetInt4AtPtr(srcLengthNext);
	    srcLengthNext += 4;
	} else {
	    srcLen = TclGetInt1AtPtr(srcLengthNext);
	    srcLengthNext++;
	}

	if (codeOffset > pcOffset) {	  /* best cmd already found */
	    break;
	} else if (pcOffset <= codeEnd) { /* this cmd's code encloses pc */
	    int dist = (pcOffset - codeOffset);
	    if (dist <= bestDist) {
		bestDist = dist;
		bestSrcOffset = srcOffset;
		bestSrcLength = srcLen;
	    }
	}
    }

    if (bestDist == INT_MAX) {
	return NULL;
    }

    if (lengthPtr != NULL) {
	*lengthPtr = bestSrcLength;
    }
    return (codePtr->source + bestSrcOffset);
}

/*
 *----------------------------------------------------------------------
 *
 * GetExceptRangeForPc --
 *
 *	Given a program counter value, return the closest enclosing
 *	ExceptionRange.
 *
 * Results:
 *	In the normal case, catchOnly is 0 (false) and this procedure returns
 *	a pointer to the most closely enclosing ExceptionRange structure
 *	regardless of whether it is a loop or catch exception range. This is
 *	appropriate when processing a TCL_BREAK or TCL_CONTINUE, which will be
 *	"handled" either by a loop exception range or a closer catch range. If
 *	catchOnly is nonzero, this procedure ignores loop exception ranges and
 *	returns a pointer to the closest catch range. If no matching
 *	ExceptionRange is found that encloses pc, a NULL is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static ExceptionRange *
GetExceptRangeForPc(
    unsigned char *pc,		/* The program counter value for which to
				 * search for a closest enclosing exception
				 * range. This points to a bytecode
				 * instruction in codePtr's code. */
    int catchOnly,		/* If 0, consider either loop or catch
				 * ExceptionRanges in search. If nonzero
				 * consider only catch ranges (and ignore any
				 * closer loop ranges). */
    ByteCode* codePtr)		/* Points to the ByteCode in which to search
				 * for the enclosing ExceptionRange. */
{
    ExceptionRange *rangeArrayPtr;
    int numRanges = codePtr->numExceptRanges;
    register ExceptionRange *rangePtr;
    int pcOffset = (pc - codePtr->codeStart);
    register int start;

    if (numRanges == 0) {
	return NULL;
    }

    /*
     * This exploits peculiarities of our compiler: nested ranges are always
     * *after* their containing ranges, so that by scanning backwards we are
     * sure that the first matching range is indeed the deepest.
     */

    rangeArrayPtr = codePtr->exceptArrayPtr;
    rangePtr = rangeArrayPtr + numRanges;
    while (--rangePtr >= rangeArrayPtr) {
	start = rangePtr->codeOffset;
	if ((start <= pcOffset) &&
		(pcOffset < (start + rangePtr->numCodeBytes))) {
	    if ((!catchOnly)
		    || (rangePtr->type == CATCH_EXCEPTION_RANGE)) {
		return rangePtr;
	    }
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * GetOpcodeName --
 *
 *	This procedure is called by the TRACE and TRACE_WITH_OBJ macros used
 *	in TclExecuteByteCode when debugging. It returns the name of the
 *	bytecode instruction at a specified instruction pc.
 *
 * Results:
 *	A character string for the instruction.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_COMPILE_DEBUG
static char *
GetOpcodeName(
    unsigned char *pc)		/* Points to the instruction whose name should
				 * be returned. */
{
    unsigned char opCode = *pc;

    return tclInstructionTable[opCode].name;
}
#endif /* TCL_COMPILE_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * TclExprFloatError --
 *
 *	This procedure is called when an error occurs during a floating-point
 *	operation. It reads errno and sets interp->objResultPtr accordingly.
 *
 * Results:
 *	interp->objResultPtr is set to hold an error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclExprFloatError(
    Tcl_Interp *interp,		/* Where to store error message. */
    double value)		/* Value returned after error; used to
				 * distinguish underflows from overflows. */
{
    CONST char *s;

    if ((errno == EDOM) || TclIsNaN(value)) {
	s = "domain error: argument not in valid range";
	Tcl_SetObjResult(interp, Tcl_NewStringObj(s, -1));
	Tcl_SetErrorCode(interp, "ARITH", "DOMAIN", s, (char *) NULL);
    } else if ((errno == ERANGE) || TclIsInfinite(value)) {
	if (value == 0.0) {
	    s = "floating-point value too small to represent";
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(s, -1));
	    Tcl_SetErrorCode(interp, "ARITH", "UNDERFLOW", s, (char *) NULL);
	} else {
	    s = "floating-point value too large to represent";
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(s, -1));
	    Tcl_SetErrorCode(interp, "ARITH", "OVERFLOW", s, (char *) NULL);
	}
    } else {
	Tcl_Obj *objPtr = Tcl_NewObj();
	TclObjPrintf(NULL, objPtr,
		"unknown floating-point error, errno = %d", errno);
	Tcl_SetErrorCode(interp, "ARITH", "UNKNOWN",
		Tcl_GetString(objPtr), (char *) NULL);
	Tcl_SetObjResult(interp, objPtr);
    }
}

#ifdef TCL_COMPILE_STATS
/*
 *----------------------------------------------------------------------
 *
 * TclLog2 --
 *
 *	Procedure used while collecting compilation statistics to determine
 *	the log base 2 of an integer.
 *
 * Results:
 *	Returns the log base 2 of the operand. If the argument is less than or
 *	equal to zero, a zero is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclLog2(
    register int value)		/* The integer for which to compute the log
				 * base 2. */
{
    register int n = value;
    register int result = 0;

    while (n > 1) {
	n = n >> 1;
	result++;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * EvalStatsCmd --
 *
 *	Implements the "evalstats" command that prints instruction execution
 *	counts to stdout.
 *
 * Results:
 *	Standard Tcl results.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
EvalStatsCmd(
    ClientData unused,		/* Unused. */
    Tcl_Interp *interp,		/* The current interpreter. */
    int objc,			/* The number of arguments. */
    Tcl_Obj *CONST objv[])	/* The argument strings. */
{
    Interp *iPtr = (Interp *) interp;
    LiteralTable *globalTablePtr = &(iPtr->literalTable);
    ByteCodeStats *statsPtr = &(iPtr->stats);
    double totalCodeBytes, currentCodeBytes;
    double totalLiteralBytes, currentLiteralBytes;
    double objBytesIfUnshared, strBytesIfUnshared, sharingBytesSaved;
    double strBytesSharedMultX, strBytesSharedOnce;
    double numInstructions, currentHeaderBytes;
    long numCurrentByteCodes, numByteCodeLits;
    long refCountSum, literalMgmtBytes, sum;
    int numSharedMultX, numSharedOnce;
    int decadeHigh, minSizeDecade, maxSizeDecade, length, i;
    char *litTableStats;
    LiteralEntry *entryPtr;

    numInstructions = 0.0;
    for (i = 0;  i < 256;  i++) {
	if (statsPtr->instructionCount[i] != 0) {
	    numInstructions += statsPtr->instructionCount[i];
	}
    }

    totalLiteralBytes = sizeof(LiteralTable)
	    + iPtr->literalTable.numBuckets * sizeof(LiteralEntry *)
	    + (statsPtr->numLiteralsCreated * sizeof(LiteralEntry))
	    + (statsPtr->numLiteralsCreated * sizeof(Tcl_Obj))
	    + statsPtr->totalLitStringBytes;
    totalCodeBytes = statsPtr->totalByteCodeBytes + totalLiteralBytes;

    numCurrentByteCodes =
	    statsPtr->numCompilations - statsPtr->numByteCodesFreed;
    currentHeaderBytes = numCurrentByteCodes
	    * (sizeof(ByteCode) - (sizeof(size_t) + sizeof(Tcl_Time)));
    literalMgmtBytes = sizeof(LiteralTable)
	    + (iPtr->literalTable.numBuckets * sizeof(LiteralEntry *))
	    + (iPtr->literalTable.numEntries * sizeof(LiteralEntry));
    currentLiteralBytes = literalMgmtBytes
	    + iPtr->literalTable.numEntries * sizeof(Tcl_Obj)
	    + statsPtr->currentLitStringBytes;
    currentCodeBytes = statsPtr->currentByteCodeBytes + currentLiteralBytes;

    /*
     * Summary statistics, total and current source and ByteCode sizes.
     */

    fprintf(stdout, "\n----------------------------------------------------------------\n");
    fprintf(stdout,
	    "Compilation and execution statistics for interpreter 0x%x\n",
	    (unsigned int) iPtr);

    fprintf(stdout, "\nNumber ByteCodes executed	%ld\n",
	    statsPtr->numExecutions);
    fprintf(stdout, "Number ByteCodes compiled	%ld\n",
	    statsPtr->numCompilations);
    fprintf(stdout, "  Mean executions/compile	%.1f\n",
	    ((float)statsPtr->numExecutions) / ((float)statsPtr->numCompilations));

    fprintf(stdout, "\nInstructions executed		%.0f\n",
	    numInstructions);
    fprintf(stdout, "  Mean inst/compile		%.0f\n",
	    numInstructions / statsPtr->numCompilations);
    fprintf(stdout, "  Mean inst/execution		%.0f\n",
	    numInstructions / statsPtr->numExecutions);

    fprintf(stdout, "\nTotal ByteCodes			%ld\n",
	    statsPtr->numCompilations);
    fprintf(stdout, "  Source bytes			%.6g\n",
	    statsPtr->totalSrcBytes);
    fprintf(stdout, "  Code bytes			%.6g\n",
	    totalCodeBytes);
    fprintf(stdout, "    ByteCode bytes		%.6g\n",
	    statsPtr->totalByteCodeBytes);
    fprintf(stdout, "    Literal bytes		%.6g\n",
	    totalLiteralBytes);
    fprintf(stdout, "      table %lu + bkts %lu + entries %lu + objects %lu + strings %.6g\n",
	    (unsigned long) sizeof(LiteralTable),
	    (unsigned long) (iPtr->literalTable.numBuckets * sizeof(LiteralEntry *)),
	    (unsigned long) (statsPtr->numLiteralsCreated * sizeof(LiteralEntry)),
	    (unsigned long) (statsPtr->numLiteralsCreated * sizeof(Tcl_Obj)),
	    statsPtr->totalLitStringBytes);
    fprintf(stdout, "  Mean code/compile		%.1f\n",
	    totalCodeBytes / statsPtr->numCompilations);
    fprintf(stdout, "  Mean code/source		%.1f\n",
	    totalCodeBytes / statsPtr->totalSrcBytes);

    fprintf(stdout, "\nCurrent (active) ByteCodes	%ld\n",
	    numCurrentByteCodes);
    fprintf(stdout, "  Source bytes			%.6g\n",
	    statsPtr->currentSrcBytes);
    fprintf(stdout, "  Code bytes			%.6g\n",
	    currentCodeBytes);
    fprintf(stdout, "    ByteCode bytes		%.6g\n",
	    statsPtr->currentByteCodeBytes);
    fprintf(stdout, "    Literal bytes		%.6g\n",
	    currentLiteralBytes);
    fprintf(stdout, "      table %lu + bkts %lu + entries %lu + objects %lu + strings %.6g\n",
	    (unsigned long) sizeof(LiteralTable),
	    (unsigned long) (iPtr->literalTable.numBuckets * sizeof(LiteralEntry *)),
	    (unsigned long) (iPtr->literalTable.numEntries * sizeof(LiteralEntry)),
	    (unsigned long) (iPtr->literalTable.numEntries * sizeof(Tcl_Obj)),
	    statsPtr->currentLitStringBytes);
    fprintf(stdout, "  Mean code/source		%.1f\n",
	    currentCodeBytes / statsPtr->currentSrcBytes);
    fprintf(stdout, "  Code + source bytes		%.6g (%0.1f mean code/src)\n",
	    (currentCodeBytes + statsPtr->currentSrcBytes),
	    (currentCodeBytes / statsPtr->currentSrcBytes) + 1.0);

    /*
     * Tcl_IsShared statistics check
     *
     * This gives the refcount of each obj as Tcl_IsShared was called for it.
     * Shared objects must be duplicated before they can be modified.
     */

    numSharedMultX = 0;
    fprintf(stdout, "\nTcl_IsShared object check (all objects):\n");
    fprintf(stdout, "  Object had refcount <=1 (not shared)	%ld\n",
	    tclObjsShared[1]);
    for (i = 2;  i < TCL_MAX_SHARED_OBJ_STATS;  i++) {
	fprintf(stdout, "  refcount ==%d		%ld\n",
		i, tclObjsShared[i]);
	numSharedMultX += tclObjsShared[i];
    }
    fprintf(stdout, "  refcount >=%d		%ld\n",
	    i, tclObjsShared[0]);
    numSharedMultX += tclObjsShared[0];
    fprintf(stdout, "  Total shared objects			%d\n",
	    numSharedMultX);

    /*
     * Literal table statistics.
     */

    numByteCodeLits = 0;
    refCountSum = 0;
    numSharedMultX = 0;
    numSharedOnce  = 0;
    objBytesIfUnshared  = 0.0;
    strBytesIfUnshared  = 0.0;
    strBytesSharedMultX = 0.0;
    strBytesSharedOnce  = 0.0;
    for (i = 0;  i < globalTablePtr->numBuckets;  i++) {
	for (entryPtr = globalTablePtr->buckets[i];  entryPtr != NULL;
		entryPtr = entryPtr->nextPtr) {
	    if (entryPtr->objPtr->typePtr == &tclByteCodeType) {
		numByteCodeLits++;
	    }
	    (void) Tcl_GetStringFromObj(entryPtr->objPtr, &length);
	    refCountSum += entryPtr->refCount;
	    objBytesIfUnshared += (entryPtr->refCount * sizeof(Tcl_Obj));
	    strBytesIfUnshared += (entryPtr->refCount * (length+1));
	    if (entryPtr->refCount > 1) {
		numSharedMultX++;
		strBytesSharedMultX += (length+1);
	    } else {
		numSharedOnce++;
		strBytesSharedOnce += (length+1);
	    }
	}
    }
    sharingBytesSaved = (objBytesIfUnshared + strBytesIfUnshared)
	    - currentLiteralBytes;

    fprintf(stdout, "\nTotal objects (all interps)	%ld\n",
	    tclObjsAlloced);
    fprintf(stdout, "Current objects			%ld\n",
	    (tclObjsAlloced - tclObjsFreed));
    fprintf(stdout, "Total literal objects		%ld\n",
	    statsPtr->numLiteralsCreated);

    fprintf(stdout, "\nCurrent literal objects		%d (%0.1f%% of current objects)\n",
	    globalTablePtr->numEntries,
	    (globalTablePtr->numEntries * 100.0) / (tclObjsAlloced-tclObjsFreed));
    fprintf(stdout, "  ByteCode literals	 	%ld (%0.1f%% of current literals)\n",
	    numByteCodeLits,
	    (numByteCodeLits * 100.0) / globalTablePtr->numEntries);
    fprintf(stdout, "  Literals reused > 1x	 	%d\n",
	    numSharedMultX);
    fprintf(stdout, "  Mean reference count	 	%.2f\n",
	    ((double) refCountSum) / globalTablePtr->numEntries);
    fprintf(stdout, "  Mean len, str reused >1x 	%.2f\n",
	    (numSharedMultX? (strBytesSharedMultX/numSharedMultX) : 0.0));
    fprintf(stdout, "  Mean len, str used 1x	 	%.2f\n",
	    (numSharedOnce? (strBytesSharedOnce/numSharedOnce) : 0.0));
    fprintf(stdout, "  Total sharing savings	 	%.6g (%0.1f%% of bytes if no sharing)\n",
	    sharingBytesSaved,
	    (sharingBytesSaved * 100.0) / (objBytesIfUnshared + strBytesIfUnshared));
    fprintf(stdout, "    Bytes with sharing		%.6g\n",
	    currentLiteralBytes);
    fprintf(stdout, "      table %lu + bkts %lu + entries %lu + objects %lu + strings %.6g\n",
	    (unsigned long) sizeof(LiteralTable),
	    (unsigned long) (iPtr->literalTable.numBuckets * sizeof(LiteralEntry *)),
	    (unsigned long) (iPtr->literalTable.numEntries * sizeof(LiteralEntry)),
	    (unsigned long) (iPtr->literalTable.numEntries * sizeof(Tcl_Obj)),
	    statsPtr->currentLitStringBytes);
    fprintf(stdout, "    Bytes if no sharing		%.6g = objects %.6g + strings %.6g\n",
	    (objBytesIfUnshared + strBytesIfUnshared),
	    objBytesIfUnshared, strBytesIfUnshared);
    fprintf(stdout, "  String sharing savings 	%.6g = unshared %.6g - shared %.6g\n",
	    (strBytesIfUnshared - statsPtr->currentLitStringBytes),
	    strBytesIfUnshared, statsPtr->currentLitStringBytes);
    fprintf(stdout, "  Literal mgmt overhead	 	%ld (%0.1f%% of bytes with sharing)\n",
	    literalMgmtBytes,
	    (literalMgmtBytes * 100.0) / currentLiteralBytes);
    fprintf(stdout, "    table %lu + buckets %lu + entries %lu\n",
	    (unsigned long) sizeof(LiteralTable),
	    (unsigned long) (iPtr->literalTable.numBuckets * sizeof(LiteralEntry *)),
	    (unsigned long) (iPtr->literalTable.numEntries * sizeof(LiteralEntry)));

    /*
     * Breakdown of current ByteCode space requirements.
     */

    fprintf(stdout, "\nBreakdown of current ByteCode requirements:\n");
    fprintf(stdout, "                         Bytes      Pct of    Avg per\n");
    fprintf(stdout, "                                     total    ByteCode\n");
    fprintf(stdout, "Total             %12.6g     100.00%%   %8.1f\n",
	    statsPtr->currentByteCodeBytes,
	    statsPtr->currentByteCodeBytes / numCurrentByteCodes);
    fprintf(stdout, "Header            %12.6g   %8.1f%%   %8.1f\n",
	    currentHeaderBytes,
	    ((currentHeaderBytes * 100.0) / statsPtr->currentByteCodeBytes),
	    currentHeaderBytes / numCurrentByteCodes);
    fprintf(stdout, "Instructions      %12.6g   %8.1f%%   %8.1f\n",
	    statsPtr->currentInstBytes,
	    ((statsPtr->currentInstBytes * 100.0) / statsPtr->currentByteCodeBytes),
	    statsPtr->currentInstBytes / numCurrentByteCodes);
    fprintf(stdout, "Literal ptr array %12.6g   %8.1f%%   %8.1f\n",
	    statsPtr->currentLitBytes,
	    ((statsPtr->currentLitBytes * 100.0) / statsPtr->currentByteCodeBytes),
	    statsPtr->currentLitBytes / numCurrentByteCodes);
    fprintf(stdout, "Exception table   %12.6g   %8.1f%%   %8.1f\n",
	    statsPtr->currentExceptBytes,
	    ((statsPtr->currentExceptBytes * 100.0) / statsPtr->currentByteCodeBytes),
	    statsPtr->currentExceptBytes / numCurrentByteCodes);
    fprintf(stdout, "Auxiliary data    %12.6g   %8.1f%%   %8.1f\n",
	    statsPtr->currentAuxBytes,
	    ((statsPtr->currentAuxBytes * 100.0) / statsPtr->currentByteCodeBytes),
	    statsPtr->currentAuxBytes / numCurrentByteCodes);
    fprintf(stdout, "Command map       %12.6g   %8.1f%%   %8.1f\n",
	    statsPtr->currentCmdMapBytes,
	    ((statsPtr->currentCmdMapBytes * 100.0) / statsPtr->currentByteCodeBytes),
	    statsPtr->currentCmdMapBytes / numCurrentByteCodes);

    /*
     * Detailed literal statistics.
     */

    fprintf(stdout, "\nLiteral string sizes:\n");
    fprintf(stdout, "	 Up to length		Percentage\n");
    maxSizeDecade = 0;
    for (i = 31;  i >= 0;  i--) {
	if (statsPtr->literalCount[i] > 0) {
	    maxSizeDecade = i;
	    break;
	}
    }
    sum = 0;
    for (i = 0;  i <= maxSizeDecade;  i++) {
	decadeHigh = (1 << (i+1)) - 1;
	sum += statsPtr->literalCount[i];
	fprintf(stdout, "	%10d		%8.0f%%\n",
		decadeHigh, (sum * 100.0) / statsPtr->numLiteralsCreated);
    }

    litTableStats = TclLiteralStats(globalTablePtr);
    fprintf(stdout, "\nCurrent literal table statistics:\n%s\n",
	    litTableStats);
    ckfree((char *) litTableStats);

    /*
     * Source and ByteCode size distributions.
     */

    fprintf(stdout, "\nSource sizes:\n");
    fprintf(stdout, "	 Up to size		Percentage\n");
    minSizeDecade = maxSizeDecade = 0;
    for (i = 0;  i < 31;  i++) {
	if (statsPtr->srcCount[i] > 0) {
	    minSizeDecade = i;
	    break;
	}
    }
    for (i = 31;  i >= 0;  i--) {
	if (statsPtr->srcCount[i] > 0) {
	    maxSizeDecade = i;
	    break;
	}
    }
    sum = 0;
    for (i = minSizeDecade;  i <= maxSizeDecade;  i++) {
	decadeHigh = (1 << (i+1)) - 1;
	sum += statsPtr->srcCount[i];
	fprintf(stdout, "	%10d		%8.0f%%\n",
		decadeHigh, (sum * 100.0) / statsPtr->numCompilations);
    }

    fprintf(stdout, "\nByteCode sizes:\n");
    fprintf(stdout, "	 Up to size		Percentage\n");
    minSizeDecade = maxSizeDecade = 0;
    for (i = 0;  i < 31;  i++) {
	if (statsPtr->byteCodeCount[i] > 0) {
	    minSizeDecade = i;
	    break;
	}
    }
    for (i = 31;  i >= 0;  i--) {
	if (statsPtr->byteCodeCount[i] > 0) {
	    maxSizeDecade = i;
	    break;
	}
    }
    sum = 0;
    for (i = minSizeDecade;  i <= maxSizeDecade;  i++) {
	decadeHigh = (1 << (i+1)) - 1;
	sum += statsPtr->byteCodeCount[i];
	fprintf(stdout, "	%10d		%8.0f%%\n",
		decadeHigh, (sum * 100.0) / statsPtr->numCompilations);
    }

    fprintf(stdout, "\nByteCode longevity (excludes Current ByteCodes):\n");
    fprintf(stdout, "	       Up to ms		Percentage\n");
    minSizeDecade = maxSizeDecade = 0;
    for (i = 0;  i < 31;  i++) {
	if (statsPtr->lifetimeCount[i] > 0) {
	    minSizeDecade = i;
	    break;
	}
    }
    for (i = 31;  i >= 0;  i--) {
	if (statsPtr->lifetimeCount[i] > 0) {
	    maxSizeDecade = i;
	    break;
	}
    }
    sum = 0;
    for (i = minSizeDecade;  i <= maxSizeDecade;  i++) {
	decadeHigh = (1 << (i+1)) - 1;
	sum += statsPtr->lifetimeCount[i];
	fprintf(stdout, "	%12.3f		%8.0f%%\n",
		decadeHigh / 1000.0,
		(sum * 100.0) / statsPtr->numByteCodesFreed);
    }

    /*
     * Instruction counts.
     */

    fprintf(stdout, "\nInstruction counts:\n");
    for (i = 0;  i <= LAST_INST_OPCODE;  i++) {
	if (statsPtr->instructionCount[i]) {
	    fprintf(stdout, "%20s %8ld %6.1f%%\n",
		    tclInstructionTable[i].name,
		    statsPtr->instructionCount[i],
		    (statsPtr->instructionCount[i]*100.0) / numInstructions);
	}
    }

    fprintf(stdout, "\nInstructions NEVER executed:\n");
    for (i = 0;  i <= LAST_INST_OPCODE;  i++) {
	if (statsPtr->instructionCount[i] == 0) {
	    fprintf(stdout, "%20s\n", tclInstructionTable[i].name);
	}
    }

#ifdef TCL_MEM_DEBUG
    fprintf(stdout, "\nHeap Statistics:\n");
    TclDumpMemoryInfo(stdout);
#endif
    fprintf(stdout, "\n----------------------------------------------------------------\n");
    return TCL_OK;
}
#endif /* TCL_COMPILE_STATS */

#ifdef TCL_COMPILE_DEBUG
/*
 *----------------------------------------------------------------------
 *
 * StringForResultCode --
 *
 *	Procedure that returns a human-readable string representing a Tcl
 *	result code such as TCL_ERROR.
 *
 * Results:
 *	If the result code is one of the standard Tcl return codes, the result
 *	is a string representing that code such as "TCL_ERROR".  Otherwise,
 *	the result string is that code formatted as a sequence of decimal
 *	digit characters. Note that the resulting string must not be modified
 *	by the caller.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
StringForResultCode(
    int result)			/* The Tcl result code for which to generate a
				 * string. */
{
    static char buf[TCL_INTEGER_SPACE];

    if ((result >= TCL_OK) && (result <= TCL_CONTINUE)) {
	return resultStrings[result];
    }
    TclFormatInt(buf, result);
    return buf;
}
#endif /* TCL_COMPILE_DEBUG */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
