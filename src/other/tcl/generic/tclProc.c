/*
 * tclProc.c --
 *
 *	This file contains routines that implement Tcl procedures, including
 *	the "proc" and "uplevel" commands.
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1998 Sun Microsystems, Inc.
 * Copyright (c) 2004-2006 Miguel Sofer
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclInt.h"
#include "tclCompile.h"

/*
 * Prototypes for static functions in this file
 */

static void		DupLambdaInternalRep(Tcl_Obj *objPtr,
			    Tcl_Obj *copyPtr);
static void		FreeLambdaInternalRep(Tcl_Obj *objPtr);
static void		InitCompiledLocals(Tcl_Interp *interp,
			    ByteCode *codePtr, CompiledLocal *localPtr,
			    Var *varPtr, Namespace *nsPtr);
static int		ObjInterpProcEx(ClientData clientData,
			    register Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[], int isLambda,
			    ProcErrorProc errorProc);
static void		ProcBodyDup(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr);
static void		ProcBodyFree(Tcl_Obj *objPtr);
static void		MakeProcError(Tcl_Interp *interp,
			    Tcl_Obj *procNameObj);
static void		MakeLambdaError(Tcl_Interp *interp,
			    Tcl_Obj *procNameObj);
static int		SetLambdaFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static int		ProcCompileProc(Tcl_Interp *interp, Proc *procPtr,
			    Tcl_Obj *bodyPtr, Namespace *nsPtr,
			    CONST char *description, CONST char *procName,
			    Proc **procPtrPtr);

/*
 * The ProcBodyObjType type
 */

Tcl_ObjType tclProcBodyType = {
    "procbody",			/* name for this type */
    ProcBodyFree,		/* FreeInternalRep function */
    ProcBodyDup,		/* DupInternalRep function */
    NULL,			/* UpdateString function; Tcl_GetString and
				 * Tcl_GetStringFromObj should panic
				 * instead. */
    NULL			/* SetFromAny function; Tcl_ConvertToType
				 * should panic instead. */
};

/*
 * The [upvar]/[uplevel] level reference type. Uses the twoPtrValue field,
 * encoding the type of level reference in ptr1 and the actual parsed out
 * offset in ptr2.
 *
 * Uses the default behaviour throughout, and never disposes of the string
 * rep; it's just a cache type.
 */

static Tcl_ObjType levelReferenceType = {
    "levelReference",
    NULL, NULL, NULL, NULL
};

/*
 * The type of lambdas. Note that every lambda will *always* have a string
 * representation.
 *
 * Internally, ptr1 is a pointer to a Proc instance that is not bound to a
 * command name, and ptr2 is a pointer to the namespace that the Proc instance
 * will execute within.
 */

static Tcl_ObjType lambdaType = {
    "lambdaExpr",		/* name */
    FreeLambdaInternalRep,	/* freeIntRepProc */
    DupLambdaInternalRep,	/* dupIntRepProc */
    NULL,			/* updateStringProc */
    SetLambdaFromAny		/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ProcObjCmd --
 *
 *	This object-based function is invoked to process the "proc" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result value.
 *
 * Side effects:
 *	A new procedure gets created.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_ProcObjCmd(
    ClientData dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[])	/* Argument objects. */
{
    register Interp *iPtr = (Interp *) interp;
    Proc *procPtr;
    char *fullName;
    CONST char *procName, *procArgs, *procBody;
    Namespace *nsPtr, *altNsPtr, *cxtNsPtr;
    Tcl_Command cmd;
    Tcl_DString ds;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "name args body");
	return TCL_ERROR;
    }

    /*
     * Determine the namespace where the procedure should reside. Unless the
     * command name includes namespace qualifiers, this will be the current
     * namespace.
     */

    fullName = TclGetString(objv[1]);
    TclGetNamespaceForQualName(interp, fullName, NULL, 0,
	    &nsPtr, &altNsPtr, &cxtNsPtr, &procName);

    if (nsPtr == NULL) {
	Tcl_AppendResult(interp, "can't create procedure \"", fullName,
		"\": unknown namespace", NULL);
	return TCL_ERROR;
    }
    if (procName == NULL) {
	Tcl_AppendResult(interp, "can't create procedure \"", fullName,
		"\": bad procedure name", NULL);
	return TCL_ERROR;
    }
    if ((nsPtr != iPtr->globalNsPtr)
	    && (procName != NULL) && (procName[0] == ':')) {
	Tcl_AppendResult(interp, "can't create procedure \"", procName,
		"\" in non-global namespace with name starting with \":\"",
		NULL);
	return TCL_ERROR;
    }

    /*
     * Create the data structure to represent the procedure.
     */

    if (TclCreateProc(interp, nsPtr, procName, objv[2], objv[3],
	    &procPtr) != TCL_OK) {
	Tcl_AddErrorInfo(interp, "\n    (creating proc \"");
	Tcl_AddErrorInfo(interp, procName);
	Tcl_AddErrorInfo(interp, "\")");
	return TCL_ERROR;
    }

    /*
     * Now create a command for the procedure. This will initially be in the
     * current namespace unless the procedure's name included namespace
     * qualifiers. To create the new command in the right namespace, we
     * generate a fully qualified name for it.
     */

    Tcl_DStringInit(&ds);
    if (nsPtr != iPtr->globalNsPtr) {
	Tcl_DStringAppend(&ds, nsPtr->fullName, -1);
	Tcl_DStringAppend(&ds, "::", 2);
    }
    Tcl_DStringAppend(&ds, procName, -1);

    cmd = Tcl_CreateObjCommand(interp, Tcl_DStringValue(&ds),
	    TclObjInterpProc, (ClientData) procPtr, TclProcDeleteProc);

    Tcl_DStringFree(&ds);

    /*
     * Now initialize the new procedure's cmdPtr field. This will be used
     * later when the procedure is called to determine what namespace the
     * procedure will run in. This will be different than the current
     * namespace if the proc was renamed into a different namespace.
     */

    procPtr->cmdPtr = (Command *) cmd;

    /*
     * TIP #280: Remember the line the procedure body is starting on. In a
     * bytecode context we ask the engine to provide us with the necessary
     * information. This is for the initialization of the byte code compiler
     * when the body is used for the first time.
     *
     * This code is nearly identical to the #280 code in SetLambdaFromAny, see
     * this file. The differences are the different index of the body in the
     * line array of the context, and the lamdba code requires some special
     * processing. Find a way to factor the common elements into a single
     * function.
     */

    if (iPtr->cmdFramePtr) {
	CmdFrame context = *iPtr->cmdFramePtr;

	if (context.type == TCL_LOCATION_BC) {
	    TclGetSrcInfoForPc(&context);

	    /*
	     * May get path in context.
	     */
	} else if (context.type == TCL_LOCATION_SOURCE) {
	    /*
	     * Context now holds another reference.
	     */

	    Tcl_IncrRefCount(context.data.eval.path);
	}

	/*
	 * type == TCL_LOCATION_PREBC implies that 'line' is NULL here! We
	 * cannot assume that 'line' is valid here, we have to check.
	 */

	if ((context.type == TCL_LOCATION_SOURCE) && context.line
		&& (context.nline >= 4) && (context.line[3] >= 0)) {
	    int isNew;
	    CmdFrame *cfPtr = (CmdFrame *) ckalloc(sizeof(CmdFrame));

	    cfPtr->level = -1;
	    cfPtr->type = context.type;
	    cfPtr->line = (int *) ckalloc(sizeof(int));
	    cfPtr->line[0] = context.line[3];
	    cfPtr->nline = 1;
	    cfPtr->framePtr = NULL;
	    cfPtr->nextPtr = NULL;

	    if (context.type == TCL_LOCATION_SOURCE) {
		cfPtr->data.eval.path = context.data.eval.path;

		/*
		 * Transfer of reference. The reference going away (release of
		 * the context) is replaced by the reference in the
		 * constructed cmdframe.
		 */
	    } else {
		cfPtr->type = TCL_LOCATION_EVAL;
		cfPtr->data.eval.path = NULL;
	    }

	    cfPtr->cmd.str.cmd = NULL;
	    cfPtr->cmd.str.len = 0;

	    Tcl_SetHashValue(Tcl_CreateHashEntry(iPtr->linePBodyPtr,
		    (char *) procPtr, &isNew), cfPtr);
	}
    }

    /*
     * Optimize for no-op procs: if the body is not precompiled (like a TclPro
     * procbody), and the argument list is just "args" and the body is empty,
     * define a compileProc to compile a no-op.
     *
     * Notes:
     *	 - cannot be done for any argument list without having different
     *	   compiled/not-compiled behaviour in the "wrong argument #" case, or
     *	   making this code much more complicated. In any case, it doesn't
     *	   seem to make a lot of sense to verify the number of arguments we
     *	   are about to ignore ...
     *	 - could be enhanced to handle also non-empty bodies that contain only
     *	   comments; however, parsing the body will slow down the compilation
     *	   of all procs whose argument list is just _args_
     */

    if (objv[3]->typePtr == &tclProcBodyType) {
	goto done;
    }

    procArgs = TclGetString(objv[2]);

    while (*procArgs == ' ') {
	procArgs++;
    }

    if ((procArgs[0] == 'a') && (strncmp(procArgs, "args", 4) == 0)) {
	procArgs +=4;
	while(*procArgs != '\0') {
	    if (*procArgs != ' ') {
		goto done;
	    }
	    procArgs++;
	}

	/*
	 * The argument list is just "args"; check the body
	 */

	procBody = TclGetString(objv[3]);
	while (*procBody != '\0') {
	    if (!isspace(UCHAR(*procBody))) {
		goto done;
	    }
	    procBody++;
	}

	/*
	 * The body is just spaces: link the compileProc
	 */

	((Command *) cmd)->compileProc = TclCompileNoOp;
    }

  done:
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateProc --
 *
 *	Creates the data associated with a Tcl procedure definition. This
 *	function knows how to handle two types of body objects: strings and
 *	procbody. Strings are the traditional (and common) value for bodies,
 *	procbody are values created by extensions that have loaded a
 *	previously compiled script.
 *
 * Results:
 *	Returns TCL_OK on success, along with a pointer to a Tcl procedure
 *	definition in procPtrPtr where the cmdPtr field is not initialised.
 *	This definition should be freed by calling TclProcCleanupProc() when
 *	it is no longer needed. Returns TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	If anything goes wrong, this function returns an error message in the
 *	interpreter.
 *
 *----------------------------------------------------------------------
 */

int
TclCreateProc(
    Tcl_Interp *interp,		/* interpreter containing proc */
    Namespace *nsPtr,		/* namespace containing this proc */
    CONST char *procName,	/* unqualified name of this proc */
    Tcl_Obj *argsPtr,		/* description of arguments */
    Tcl_Obj *bodyPtr,		/* command body */
    Proc **procPtrPtr)		/* returns: pointer to proc data */
{
    Interp *iPtr = (Interp *) interp;
    CONST char **argArray = NULL;

    register Proc *procPtr;
    int i, length, result, numArgs;
    CONST char *args, *bytes, *p;
    register CompiledLocal *localPtr = NULL;
    Tcl_Obj *defPtr;
    int precompiled = 0;

    if (bodyPtr->typePtr == &tclProcBodyType) {
	/*
	 * Because the body is a TclProProcBody, the actual body is already
	 * compiled, and it is not shared with anyone else, so it's OK not to
	 * unshare it (as a matter of fact, it is bad to unshare it, because
	 * there may be no source code).
	 *
	 * We don't create and initialize a Proc structure for the procedure;
	 * rather, we use what is in the body object. We increment the ref
	 * count of the Proc struct since the command (soon to be created)
	 * will be holding a reference to it.
	 */

	procPtr = bodyPtr->internalRep.otherValuePtr;
	procPtr->iPtr = iPtr;
	procPtr->refCount++;
	precompiled = 1;
    } else {
	/*
	 * If the procedure's body object is shared because its string value
	 * is identical to, e.g., the body of another procedure, we must
	 * create a private copy for this procedure to use. Such sharing of
	 * procedure bodies is rare but can cause problems. A procedure body
	 * is compiled in a context that includes the number of
	 * compiler-allocated "slots" for local variables. Each formal
	 * parameter is given a local variable slot (the
	 * "procPtr->numCompiledLocals = numArgs" assignment below). This
	 * means that the same code can not be shared by two procedures that
	 * have a different number of arguments, even if their bodies are
	 * identical. Note that we don't use Tcl_DuplicateObj since we would
	 * not want any bytecode internal representation.
	 */

	if (Tcl_IsShared(bodyPtr)) {
	    bytes = Tcl_GetStringFromObj(bodyPtr, &length);
	    bodyPtr = Tcl_NewStringObj(bytes, length);
	}

	/*
	 * Create and initialize a Proc structure for the procedure. We
	 * increment the ref count of the procedure's body object since there
	 * will be a reference to it in the Proc structure.
	 */

	Tcl_IncrRefCount(bodyPtr);

	procPtr = (Proc *) ckalloc(sizeof(Proc));
	procPtr->iPtr = iPtr;
	procPtr->refCount = 1;
	procPtr->bodyPtr = bodyPtr;
	procPtr->numArgs = 0;	/* actual argument count is set below. */
	procPtr->numCompiledLocals = 0;
	procPtr->firstLocalPtr = NULL;
	procPtr->lastLocalPtr = NULL;
    }

    /*
     * Break up the argument list into argument specifiers, then process each
     * argument specifier. If the body is precompiled, processing is limited
     * to checking that the parsed argument is consistent with the one stored
     * in the Proc.
     *
     * THIS FAILS IF THE ARG LIST OBJECT'S STRING REP CONTAINS NULS.
     */

    args = Tcl_GetStringFromObj(argsPtr, &length);
    result = Tcl_SplitList(interp, args, &numArgs, &argArray);
    if (result != TCL_OK) {
	goto procError;
    }

    if (precompiled) {
	if (numArgs > procPtr->numArgs) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "procedure \"%s\": arg list contains %d entries, "
		    "precompiled header expects %d", procName, numArgs,
		    procPtr->numArgs));
	    goto procError;
	}
	localPtr = procPtr->firstLocalPtr;
    } else {
	procPtr->numArgs = numArgs;
	procPtr->numCompiledLocals = numArgs;
    }

    for (i = 0; i < numArgs; i++) {
	int fieldCount, nameLength, valueLength;
	CONST char **fieldValues;

	/*
	 * Now divide the specifier up into name and default.
	 */

	result = Tcl_SplitList(interp, argArray[i], &fieldCount,
		&fieldValues);
	if (result != TCL_OK) {
	    goto procError;
	}
	if (fieldCount > 2) {
	    ckfree((char *) fieldValues);
	    Tcl_AppendResult(interp,
		    "too many fields in argument specifier \"",
		    argArray[i], "\"", NULL);
	    goto procError;
	}
	if ((fieldCount == 0) || (*fieldValues[0] == 0)) {
	    ckfree((char *) fieldValues);
	    Tcl_AppendResult(interp, "argument with no name", NULL);
	    goto procError;
	}

	nameLength = strlen(fieldValues[0]);
	if (fieldCount == 2) {
	    valueLength = strlen(fieldValues[1]);
	} else {
	    valueLength = 0;
	}

	/*
	 * Check that the formal parameter name is a scalar.
	 */

	p = fieldValues[0];
	while (*p != '\0') {
	    if (*p == '(') {
		CONST char *q = p;
		do {
		    q++;
		} while (*q != '\0');
		q--;
		if (*q == ')') { /* we have an array element */
		    Tcl_AppendResult(interp, "formal parameter \"",
			    fieldValues[0],
			    "\" is an array element", NULL);
		    ckfree((char *) fieldValues);
		    goto procError;
		}
	    } else if ((*p == ':') && (*(p+1) == ':')) {
		Tcl_AppendResult(interp, "formal parameter \"",
			fieldValues[0],
			"\" is not a simple name", NULL);
		ckfree((char *) fieldValues);
		goto procError;
	    }
	    p++;
	}

	if (precompiled) {
	    /*
	     * Compare the parsed argument with the stored one. For the flags,
	     * we and out VAR_UNDEFINED to support bridging precompiled <= 8.3
	     * code in 8.4 where this is now used as an optimization
	     * indicator. Yes, this is a hack. -- hobbs
	     */

	    if ((localPtr->nameLength != nameLength)
		    || (strcmp(localPtr->name, fieldValues[0]))
		    || (localPtr->frameIndex != i)
		    || ((localPtr->flags & ~VAR_UNDEFINED)
			    != (VAR_SCALAR | VAR_ARGUMENT))
		    || (localPtr->defValuePtr == NULL && fieldCount == 2)
		    || (localPtr->defValuePtr != NULL && fieldCount != 2)) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"procedure \"%s\": formal parameter %d is "
			"inconsistent with precompiled body", procName, i));
		ckfree((char *) fieldValues);
		goto procError;
	    }

	    /*
	     * compare the default value if any
	     */

	    if (localPtr->defValuePtr != NULL) {
		int tmpLength;
		char *tmpPtr = Tcl_GetStringFromObj(localPtr->defValuePtr,
			&tmpLength);
		if ((valueLength != tmpLength) ||
			strncmp(fieldValues[1], tmpPtr, (size_t) tmpLength)) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			    "procedure \"%s\": formal parameter \"%s\" has "
			    "default value inconsistent with precompiled body",
			    procName, fieldValues[0]));
		    ckfree((char *) fieldValues);
		    goto procError;
		}
	    }
	    if ((i == numArgs - 1)
		    && (localPtr->nameLength == 4)
		    && (localPtr->name[0] == 'a')
		    && (strcmp(localPtr->name, "args") == 0)) {
		localPtr->flags |= VAR_IS_ARGS;
	    }

	    localPtr = localPtr->nextPtr;
	} else {
	    /*
	     * Allocate an entry in the runtime procedure frame's array of
	     * local variables for the argument.
	     */

	    localPtr = (CompiledLocal *) ckalloc((unsigned)
		    (sizeof(CompiledLocal) - sizeof(localPtr->name)
			    + nameLength + 1));
	    if (procPtr->firstLocalPtr == NULL) {
		procPtr->firstLocalPtr = procPtr->lastLocalPtr = localPtr;
	    } else {
		procPtr->lastLocalPtr->nextPtr = localPtr;
		procPtr->lastLocalPtr = localPtr;
	    }
	    localPtr->nextPtr = NULL;
	    localPtr->nameLength = nameLength;
	    localPtr->frameIndex = i;
	    localPtr->flags = VAR_SCALAR | VAR_ARGUMENT;
	    localPtr->resolveInfo = NULL;

	    if (fieldCount == 2) {
		localPtr->defValuePtr =
			Tcl_NewStringObj(fieldValues[1], valueLength);
		Tcl_IncrRefCount(localPtr->defValuePtr);
	    } else {
		localPtr->defValuePtr = NULL;
	    }
	    strcpy(localPtr->name, fieldValues[0]);
	    if ((i == numArgs - 1)
		    && (localPtr->nameLength == 4)
		    && (localPtr->name[0] == 'a')
		    && (strcmp(localPtr->name, "args") == 0)) {
		localPtr->flags |= VAR_IS_ARGS;
	    }
	}

	ckfree((char *) fieldValues);
    }

    *procPtrPtr = procPtr;
    ckfree((char *) argArray);
    return TCL_OK;

  procError:
    if (precompiled) {
	procPtr->refCount--;
    } else {
	Tcl_DecrRefCount(bodyPtr);
	while (procPtr->firstLocalPtr != NULL) {
	    localPtr = procPtr->firstLocalPtr;
	    procPtr->firstLocalPtr = localPtr->nextPtr;

	    defPtr = localPtr->defValuePtr;
	    if (defPtr != NULL) {
		Tcl_DecrRefCount(defPtr);
	    }

	    ckfree((char *) localPtr);
	}
	ckfree((char *) procPtr);
    }
    if (argArray != NULL) {
	ckfree((char *) argArray);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetFrame --
 *
 *	Given a description of a procedure frame, such as the first argument
 *	to an "uplevel" or "upvar" command, locate the call frame for the
 *	appropriate level of procedure.
 *
 * Results:
 *	The return value is -1 if an error occurred in finding the frame (in
 *	this case an error message is left in the interp's result). 1 is
 *	returned if string was either a number or a number preceded by "#" and
 *	it specified a valid frame. 0 is returned if string isn't one of the
 *	two things above (in this case, the lookup acts as if string were
 *	"1"). The variable pointed to by framePtrPtr is filled in with the
 *	address of the desired frame (unless an error occurs, in which case it
 *	isn't modified).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGetFrame(
    Tcl_Interp *interp,		/* Interpreter in which to find frame. */
    CONST char *name,		/* String describing frame. */
    CallFrame **framePtrPtr)	/* Store pointer to frame here (or NULL if
				 * global frame indicated). */
{
    register Interp *iPtr = (Interp *) interp;
    int curLevel, level, result;
    CallFrame *framePtr;

    /*
     * Parse string to figure out which level number to go to.
     */

    result = 1;
    curLevel = iPtr->varFramePtr->level;
    if (*name== '#') {
	if (Tcl_GetInt(interp, name+1, &level) != TCL_OK || level < 0) {
	    goto levelError;
	}
    } else if (isdigit(UCHAR(*name))) { /* INTL: digit */
	if (Tcl_GetInt(interp, name, &level) != TCL_OK) {
	    goto levelError;
	}
	level = curLevel - level;
    } else {
	level = curLevel - 1;
	result = 0;
    }

    /*
     * Figure out which frame to use, and return it to the caller.
     */

    for (framePtr = iPtr->varFramePtr; framePtr != NULL;
	    framePtr = framePtr->callerVarPtr) {
	if (framePtr->level == level) {
	    break;
	}
    }
    if (framePtr == NULL) {
	goto levelError;
    }

    *framePtrPtr = framePtr;
    return result;

  levelError:
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "bad level \"", name, "\"", NULL);
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclObjGetFrame --
 *
 *	Given a description of a procedure frame, such as the first argument
 *	to an "uplevel" or "upvar" command, locate the call frame for the
 *	appropriate level of procedure.
 *
 * Results:
 *	The return value is -1 if an error occurred in finding the frame (in
 *	this case an error message is left in the interp's result). 1 is
 *	returned if objPtr was either a number or a number preceded by "#" and
 *	it specified a valid frame. 0 is returned if objPtr isn't one of the
 *	two things above (in this case, the lookup acts as if objPtr were
 *	"1"). The variable pointed to by framePtrPtr is filled in with the
 *	address of the desired frame (unless an error occurs, in which case it
 *	isn't modified).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclObjGetFrame(
    Tcl_Interp *interp,		/* Interpreter in which to find frame. */
    Tcl_Obj *objPtr,		/* Object describing frame. */
    CallFrame **framePtrPtr)	/* Store pointer to frame here (or NULL if
				 * global frame indicated). */
{
    register Interp *iPtr = (Interp *) interp;
    int curLevel, level, result;
    CallFrame *framePtr;
    CONST char *name = TclGetString(objPtr);

    /*
     * Parse object to figure out which level number to go to.
     */

    result = 1;
    curLevel = iPtr->varFramePtr->level;
    if (objPtr->typePtr == &levelReferenceType) {
	if (PTR2INT(objPtr->internalRep.twoPtrValue.ptr1)) {
	    level = curLevel - PTR2INT(objPtr->internalRep.twoPtrValue.ptr2);
	} else {
	    level = PTR2INT(objPtr->internalRep.twoPtrValue.ptr2);
	}
	if (level < 0) {
	    goto levelError;
	}
	/* TODO: Consider skipping the typePtr checks */
    } else if (objPtr->typePtr == &tclIntType
#ifndef NO_WIDE_TYPE
	    || objPtr->typePtr == &tclWideIntType
#endif
	    ) {
	if (Tcl_GetIntFromObj(NULL, objPtr, &level) != TCL_OK || level < 0) {
	    goto levelError;
	}
	level = curLevel - level;
    } else if (*name == '#') {
	if (Tcl_GetInt(interp, name+1, &level) != TCL_OK || level < 0) {
	    goto levelError;
	}

	/*
	 * Cache for future reference.
	 *
	 * TODO: Use the new ptrAndLongRep intrep
	 */

	TclFreeIntRep(objPtr);
	objPtr->typePtr = &levelReferenceType;
	objPtr->internalRep.twoPtrValue.ptr1 = (void *) 0;
	objPtr->internalRep.twoPtrValue.ptr2 = INT2PTR(level);
    } else if (isdigit(UCHAR(*name))) { /* INTL: digit */
	if (Tcl_GetInt(interp, name, &level) != TCL_OK) {
	    return -1;
	}

	/*
	 * Cache for future reference.
	 *
	 * TODO: Use the new ptrAndLongRep intrep
	 */

	TclFreeIntRep(objPtr);
	objPtr->typePtr = &levelReferenceType;
	objPtr->internalRep.twoPtrValue.ptr1 = (void *) 1;
	objPtr->internalRep.twoPtrValue.ptr2 = INT2PTR(level);
	level = curLevel - level;
    } else {
	/*
	 * Don't cache as the object *isn't* a level reference.
	 */

	level = curLevel - 1;
	result = 0;
    }

    /*
     * Figure out which frame to use, and return it to the caller.
     */

    for (framePtr = iPtr->varFramePtr; framePtr != NULL;
	    framePtr = framePtr->callerVarPtr) {
	if (framePtr->level == level) {
	    break;
	}
    }
    if (framePtr == NULL) {
	goto levelError;
    }
    *framePtrPtr = framePtr;
    return result;

  levelError:
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "bad level \"", name, "\"", NULL);
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UplevelObjCmd --
 *
 *	This object function is invoked to process the "uplevel" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result value.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_UplevelObjCmd(
    ClientData dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[])	/* Argument objects. */
{
    register Interp *iPtr = (Interp *) interp;
    int result;
    CallFrame *savedVarFramePtr, *framePtr;

    if (objc < 2) {
    uplevelSyntax:
	Tcl_WrongNumArgs(interp, 1, objv, "?level? command ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * Find the level to use for executing the command.
     */

    result = TclObjGetFrame(interp, objv[1], &framePtr);
    if (result == -1) {
	return TCL_ERROR;
    }
    objc -= (result+1);
    if (objc == 0) {
	goto uplevelSyntax;
    }
    objv += (result+1);

    /*
     * Modify the interpreter state to execute in the given frame.
     */

    savedVarFramePtr = iPtr->varFramePtr;
    iPtr->varFramePtr = framePtr;

    /*
     * Execute the residual arguments as a command.
     */

    if (objc == 1) {
	result = Tcl_EvalObjEx(interp, objv[0], TCL_EVAL_DIRECT);
    } else {
	/*
	 * More than one argument: concatenate them together with spaces
	 * between, then evaluate the result. Tcl_EvalObjEx will delete the
	 * object when it decrements its refcount after eval'ing it.
	 */

	Tcl_Obj *objPtr;

	objPtr = Tcl_ConcatObj(objc, objv);
	result = Tcl_EvalObjEx(interp, objPtr, TCL_EVAL_DIRECT);
    }
    if (result == TCL_ERROR) {
	Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
		"\n    (\"uplevel\" body line %d)", interp->errorLine));
    }

    /*
     * Restore the variable frame, and return.
     */

    iPtr->varFramePtr = savedVarFramePtr;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFindProc --
 *
 *	Given the name of a procedure, return a pointer to the record
 *	describing the procedure. The procedure will be looked up using the
 *	usual rules: first in the current namespace and then in the global
 *	namespace.
 *
 * Results:
 *	NULL is returned if the name doesn't correspond to any procedure.
 *	Otherwise, the return value is a pointer to the procedure's record. If
 *	the name is found but refers to an imported command that points to a
 *	"real" procedure defined in another namespace, a pointer to that
 *	"real" procedure's structure is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Proc *
TclFindProc(
    Interp *iPtr,		/* Interpreter in which to look. */
    CONST char *procName)	/* Name of desired procedure. */
{
    Tcl_Command cmd;
    Tcl_Command origCmd;
    Command *cmdPtr;

    cmd = Tcl_FindCommand((Tcl_Interp *) iPtr, procName, NULL, /*flags*/ 0);
    if (cmd == (Tcl_Command) NULL) {
	return NULL;
    }
    cmdPtr = (Command *) cmd;

    origCmd = TclGetOriginalCommand(cmd);
    if (origCmd != NULL) {
	cmdPtr = (Command *) origCmd;
    }
    if (cmdPtr->objProc != TclObjInterpProc) {
	return NULL;
    }
    return (Proc *) cmdPtr->objClientData;
}

/*
 *----------------------------------------------------------------------
 *
 * TclIsProc --
 *
 *	Tells whether a command is a Tcl procedure or not.
 *
 * Results:
 *	If the given command is actually a Tcl procedure, the return value is
 *	the address of the record describing the procedure. Otherwise the
 *	return value is 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Proc *
TclIsProc(
    Command *cmdPtr)		/* Command to test. */
{
    Tcl_Command origCmd;

    origCmd = TclGetOriginalCommand((Tcl_Command) cmdPtr);
    if (origCmd != NULL) {
	cmdPtr = (Command *) origCmd;
    }
    if (cmdPtr->objProc == TclObjInterpProc) {
	return (Proc *) cmdPtr->objClientData;
    }
    return (Proc *) 0;
}

/*
 *----------------------------------------------------------------------
 *
 * InitCompiledLocals --
 *
 *	This routine is invoked in order to initialize the compiled locals
 *	table for a new call frame.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May invoke various name resolvers in order to determine which
 *	variables are being referenced at runtime.
 *
 *----------------------------------------------------------------------
 */

static void
InitCompiledLocals(
    Tcl_Interp *interp,		/* Current interpreter. */
    ByteCode *codePtr,
    CompiledLocal *localPtr,
    Var *varPtr,
    Namespace *nsPtr)		/* Pointer to current namespace. */
{
    Interp *iPtr = (Interp *) interp;
    int haveResolvers = (nsPtr->compiledVarResProc || iPtr->resolverPtr);
    CompiledLocal *firstLocalPtr;

    if (!(haveResolvers && (codePtr->flags & TCL_BYTECODE_RESOLVE_VARS))) {
	/*
	 * Initialize the array of local variables stored in the call frame.
	 * Some variables may have special resolution rules. In that case, we
	 * call their "resolver" procs to get our hands on the variable, and
	 * we make the compiled local a link to the real variable.
	 */

    doInitCompiledLocals:
	if (!haveResolvers) {
	    for (; localPtr != NULL; varPtr++, localPtr = localPtr->nextPtr) {
		varPtr->value.objPtr = NULL;
		varPtr->name = localPtr->name;	/* Will be just '\0' if temp
						 * var. */
		varPtr->nsPtr = NULL;
		varPtr->hPtr = NULL;
		varPtr->refCount = 0;
		varPtr->tracePtr = NULL;
		varPtr->searchPtr = NULL;
		varPtr->flags = localPtr->flags;
	    }
	    return;
	} else {
	    Tcl_ResolvedVarInfo *resVarInfo;

	    for (; localPtr != NULL; varPtr++, localPtr = localPtr->nextPtr) {
		varPtr->value.objPtr = NULL;
		varPtr->name = localPtr->name;	/* Will be just '\0' if temp
						 * var. */
		varPtr->nsPtr = NULL;
		varPtr->hPtr = NULL;
		varPtr->refCount = 0;
		varPtr->tracePtr = NULL;
		varPtr->searchPtr = NULL;
		varPtr->flags = localPtr->flags;

		/*
		 * Now invoke the resolvers to determine the exact variables
		 * that should be used.
		 */

		resVarInfo = localPtr->resolveInfo;
		if (resVarInfo && resVarInfo->fetchProc) {
		    Var *resolvedVarPtr = (Var *)
			    (*resVarInfo->fetchProc)(interp, resVarInfo);
		    if (resolvedVarPtr) {
			resolvedVarPtr->refCount++;
			varPtr->value.linkPtr = resolvedVarPtr;
			varPtr->flags = VAR_LINK;
		    }
		}
	    }
	    return;
	}
    } else {
	/*
	 * This is the first run after a recompile, or else the resolver epoch
	 * has changed: update the resolver cache.
	 */

	firstLocalPtr = localPtr;
	for (; localPtr != NULL; localPtr = localPtr->nextPtr) {
	    if (localPtr->resolveInfo) {
		if (localPtr->resolveInfo->deleteProc) {
		    localPtr->resolveInfo->deleteProc(localPtr->resolveInfo);
		} else {
		    ckfree((char *) localPtr->resolveInfo);
		}
		localPtr->resolveInfo = NULL;
	    }
	    localPtr->flags &= ~VAR_RESOLVED;

	    if (haveResolvers &&
		    !(localPtr->flags & (VAR_ARGUMENT|VAR_TEMPORARY))) {
		ResolverScheme *resPtr = iPtr->resolverPtr;
		Tcl_ResolvedVarInfo *vinfo;
		int result;

		if (nsPtr->compiledVarResProc) {
		    result = (*nsPtr->compiledVarResProc)(nsPtr->interp,
			    localPtr->name, localPtr->nameLength,
			    (Tcl_Namespace *) nsPtr, &vinfo);
		} else {
		    result = TCL_CONTINUE;
		}

		while ((result == TCL_CONTINUE) && resPtr) {
		    if (resPtr->compiledVarResProc) {
			result = (*resPtr->compiledVarResProc)(nsPtr->interp,
				localPtr->name, localPtr->nameLength,
				(Tcl_Namespace *) nsPtr, &vinfo);
		    }
		    resPtr = resPtr->nextPtr;
		}
		if (result == TCL_OK) {
		    localPtr->resolveInfo = vinfo;
		    localPtr->flags |= VAR_RESOLVED;
		}
	    }
	}
	localPtr = firstLocalPtr;
	codePtr->flags &= ~TCL_BYTECODE_RESOLVE_VARS;
	goto doInitCompiledLocals;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitCompiledLocals --
 *
 *	This routine is invoked in order to initialize the compiled locals
 *	table for a new call frame.
 *
 *	DEPRECATED: functionality has been inlined elsewhere; this function
 *	remains to insure binary compatibility with Itcl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May invoke various name resolvers in order to determine which
 *	variables are being referenced at runtime.
 *
 *----------------------------------------------------------------------
 */

void
TclInitCompiledLocals(
    Tcl_Interp *interp,		/* Current interpreter. */
    CallFrame *framePtr,	/* Call frame to initialize. */
    Namespace *nsPtr)		/* Pointer to current namespace. */
{
    Var *varPtr = framePtr->compiledLocals;
    Tcl_Obj *bodyPtr;
    ByteCode *codePtr;
    CompiledLocal *localPtr = framePtr->procPtr->firstLocalPtr;

    bodyPtr = framePtr->procPtr->bodyPtr;
    if (bodyPtr->typePtr != &tclByteCodeType) {
	Tcl_Panic("body object for proc attached to frame is not a byte code type");
    }
    codePtr = bodyPtr->internalRep.otherValuePtr;

    InitCompiledLocals(interp, codePtr, localPtr, varPtr, nsPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclObjInterpProc, ObjInterpProcEx --
 *
 *	When a Tcl procedure gets invoked during bytecode evaluation, this
 *	object-based routine gets invoked to interpret the procedure.
 *
 * Results:
 *	A standard Tcl object result value.
 *
 * Side effects:
 *	Depends on the commands in the procedure.
 *
 *----------------------------------------------------------------------
 */

int
TclObjInterpProc(
    ClientData clientData, 	/* Record describing procedure to be
				 * interpreted. */
    register Tcl_Interp *interp,/* Interpreter in which procedure was
				 * invoked. */
    int objc,			/* Count of number of arguments to this
				 * procedure. */
    Tcl_Obj *CONST objv[])	/* Argument value objects. */
{
    return ObjInterpProcEx(clientData, interp, objc, objv, /*isLambda*/ 0,
	    &MakeProcError);
}

static int
ObjInterpProcEx(
    ClientData clientData, 	/* Record describing procedure to be
				 * interpreted. */
    register Tcl_Interp *interp,/* Interpreter in which procedure was
				 * invoked. */
    int objc,			/* Count of number of arguments to this
				 * procedure. */
    Tcl_Obj *CONST objv[],	/* Argument value objects. */
    int isLambda,		/* 1 if this is a call by ApplyObjCmd: it
				 * needs special rules for error msg */
    ProcErrorProc errorProc)	/* How to convert results from the script into
				 * results of the overall procedure. */
{
    Proc *procPtr = (Proc *) clientData;
    Namespace *nsPtr = procPtr->cmdPtr->nsPtr;
    CallFrame *framePtr, **framePtrPtr;
    int result;

    /*
     * If necessary, compile the procedure's body. The compiler will allocate
     * frame slots for the procedure's non-argument local variables. Note that
     * compiling the body might increase procPtr->numCompiledLocals if new
     * local variables are found while compiling.
     */

    if (procPtr->bodyPtr->typePtr == &tclByteCodeType) {
	Interp *iPtr = (Interp *) interp;
	ByteCode *codePtr = procPtr->bodyPtr->internalRep.otherValuePtr;

 	if (((Interp *) *codePtr->interpHandle != iPtr)
		|| (codePtr->compileEpoch != iPtr->compileEpoch)
		|| (codePtr->nsPtr != nsPtr)) {
	recompileBody:
	    result = ProcCompileProc(interp, procPtr, procPtr->bodyPtr, nsPtr,
		    (isLambda ? "body of lambda term" : "body of proc"),
		    TclGetString(objv[isLambda]), &procPtr);

	    if (result != TCL_OK) {
		return result;
	    }
	}
    } else {
	goto recompileBody;
    }

    /*
     * Set up and push a new call frame for the new procedure invocation.
     * This call frame will execute in the proc's namespace, which might be
     * different than the current namespace. The proc's namespace is that of
     * its command, which can change if the command is renamed from one
     * namespace to another.
     */

    framePtrPtr = &framePtr;
    result = TclPushStackFrame(interp, (Tcl_CallFrame **) framePtrPtr,
	    (Tcl_Namespace *) nsPtr, FRAME_IS_PROC);

    if (result != TCL_OK) {
	return result;
    }

    framePtr->objc = objc;
    framePtr->objv = objv;
    framePtr->procPtr = procPtr;

    return TclObjInterpProcCore(interp, framePtr, objv[isLambda], isLambda,
	    isLambda+1, errorProc);
}

/*
 *----------------------------------------------------------------------
 *
 * TclObjInterpProcCore --
 *
 *	When a Tcl procedure, lambda term or anything else that works like a
 *	procedure gets invoked during bytecode evaluation, this object-based
 *	routine gets invoked to interpret the body.
 *
 * Results:
 *	A standard Tcl object result value.
 *
 * Side effects:
 *	Nearly anything; depends on the commands in the procedure body.
 *
 *----------------------------------------------------------------------
 */

int
TclObjInterpProcCore(
    register Tcl_Interp *interp,/* Interpreter in which procedure was
				 * invoked. */
    CallFrame *framePtr,	/* The context to execute. The procPtr field
				 * must be non-NULL. */
    Tcl_Obj *procNameObj,	/* Procedure name for error reporting. */
    int isLambda,		/* 1 if this is a call by ApplyObjCmd: it
				 * needs special rules for error msg. */
    int skip,			/* Number of initial arguments to be skipped,
				 * i.e., words in the "command name". */
    ProcErrorProc errorProc)	/* How to convert results from the script into
				 * results of the overall procedure. */
{
    register Proc *procPtr = framePtr->procPtr;
    register Var *varPtr;
    register CompiledLocal *localPtr;
    int localCt, numArgs, argCt, i, imax, result;
    Var *compiledLocals;
    Tcl_Obj *const *argObjs;

    /*
     * Create the "compiledLocals" array. Make sure it is large enough to hold
     * all the procedure's compiled local variables, including its formal
     * parameters.
     */

    localCt = procPtr->numCompiledLocals;
    compiledLocals = (Var*) TclStackAlloc(interp, (int)(localCt*sizeof(Var)));
    framePtr->numCompiledLocals = localCt;
    framePtr->compiledLocals = compiledLocals;

    /*
     * Match and assign the call's actual parameters to the procedure's formal
     * arguments. The formal arguments are described by the first numArgs
     * entries in both the Proc structure's local variable list and the call
     * frame's local variable array.
     */

    numArgs = procPtr->numArgs;
    argCt = framePtr->objc - skip;	/* Set it to the number of args to the
					 * procedure. */
    argObjs = framePtr->objv + skip;
    varPtr = framePtr->compiledLocals;
    localPtr = procPtr->firstLocalPtr;
    if (numArgs == 0) {
	if (argCt) {
	    goto incorrectArgs;
	} else {
	    goto runProc;
	}
    }
    imax = ((argCt < numArgs - 1)? argCt : (numArgs - 1));
    for (i = 0; i < imax; i++) {
	/*
	 * "Normal" arguments; last formal is special, depends on it being
	 * 'args'.
	 */

	Tcl_Obj *objPtr = argObjs[i];

	varPtr->value.objPtr = objPtr;
	Tcl_IncrRefCount(objPtr);	/* local var is a reference */
	varPtr->name = localPtr->name;
	varPtr->nsPtr = NULL;
	varPtr->hPtr = NULL;
	varPtr->refCount = 0;
	varPtr->tracePtr = NULL;
	varPtr->searchPtr = NULL;
	varPtr->flags = localPtr->flags;
	varPtr++;
	localPtr = localPtr->nextPtr;
    }
    for (; i < (numArgs - 1); i++) {
	/*
	 * This loop is entered if argCt < (numArgs-1). Set default values;
	 * last formal is special.
	 */

	if (localPtr->defValuePtr != NULL) {
	    Tcl_Obj *objPtr = localPtr->defValuePtr;

	    varPtr->value.objPtr = objPtr;
	    Tcl_IncrRefCount(objPtr);	/* local var is a reference */
	    varPtr->name = localPtr->name;
	    varPtr->nsPtr = NULL;
	    varPtr->hPtr = NULL;
	    varPtr->refCount = 0;
	    varPtr->tracePtr = NULL;
	    varPtr->searchPtr = NULL;
	    varPtr->flags = localPtr->flags;
	    varPtr++;
	    localPtr = localPtr->nextPtr;
	} else {
	    goto incorrectArgs;
	}
    }

    /*
     * When we get here, the last formal argument remains to be defined:
     * localPtr and varPtr point to the last argument to be initialized.
     */

    if (localPtr->flags & VAR_IS_ARGS) {
	Tcl_Obj *listPtr = Tcl_NewListObj(argCt-i, argObjs+i);

	varPtr->value.objPtr = listPtr;
	Tcl_IncrRefCount(listPtr);	/* local var is a reference */
    } else if (argCt == numArgs) {
	Tcl_Obj *objPtr = argObjs[i];

	varPtr->value.objPtr = objPtr;
	Tcl_IncrRefCount(objPtr);	/* local var is a reference */
    } else if ((argCt < numArgs) && (localPtr->defValuePtr != NULL)) {
	Tcl_Obj *objPtr = localPtr->defValuePtr;

	varPtr->value.objPtr = objPtr;
	Tcl_IncrRefCount(objPtr);	/* local var is a reference */
    } else {
	Tcl_Obj **desiredObjs;
	ByteCode *codePtr;
	const char *final;

	/*
	 * Do initialise all compiled locals, to avoid problems at
	 * DeleteLocalVars.
	 */

    incorrectArgs:
	final = NULL;
	codePtr = procPtr->bodyPtr->internalRep.otherValuePtr;
	InitCompiledLocals(interp, codePtr, localPtr, varPtr, framePtr->nsPtr);

	/*
	 * Build up desired argument list for Tcl_WrongNumArgs
	 */

	desiredObjs = (Tcl_Obj **) TclStackAlloc(interp,
		(int) sizeof(Tcl_Obj *) * (numArgs+1));

#ifdef AVOID_HACKS_FOR_ITCL
	desiredObjs[0] = framePtr->objv[skip-1];
#else
	desiredObjs[0] = (isLambda ? framePtr->objv[skip-1] :
		Tcl_NewListObj(skip, framePtr->objv));
#endif /* AVOID_HACKS_FOR_ITCL */

	localPtr = procPtr->firstLocalPtr;
	for (i=1 ; i<=numArgs ; i++) {
	    Tcl_Obj *argObj;

	    if (localPtr->defValuePtr != NULL) {
		TclNewObj(argObj);
		Tcl_AppendStringsToObj(argObj, "?", localPtr->name, "?", NULL);
	    } else if ((i==numArgs) && !strcmp(localPtr->name, "args")) {
		numArgs--;
		final = "...";
		break;
	    } else {
		argObj = Tcl_NewStringObj(localPtr->name, -1);
	    }
	    desiredObjs[i] = argObj;
	    localPtr = localPtr->nextPtr;
	}

	Tcl_ResetResult(interp);
	Tcl_WrongNumArgs(interp, numArgs+1, desiredObjs, final);
	result = TCL_ERROR;

#ifndef AVOID_HACKS_FOR_ITCL
	if (!isLambda) {
	    TclDecrRefCount(desiredObjs[0]);
	}
#endif /* AVOID_HACKS_FOR_ITCL */

	for (i=1 ; i<=numArgs ; i++) {
	    TclDecrRefCount(desiredObjs[i]);
	}
	TclStackFree(interp);
	goto procDone;
    }

    varPtr->name = localPtr->name;
    varPtr->nsPtr = NULL;
    varPtr->hPtr = NULL;
    varPtr->refCount = 0;
    varPtr->tracePtr = NULL;
    varPtr->searchPtr = NULL;
    varPtr->flags = localPtr->flags;

    localPtr = localPtr->nextPtr;
    varPtr++;

    /*
     * Initialise and resolve the remaining compiledLocals.
     */

  runProc:
    if (localPtr) {
	ByteCode *codePtr = procPtr->bodyPtr->internalRep.otherValuePtr;

	InitCompiledLocals(interp, codePtr, localPtr, varPtr, framePtr->nsPtr);
    }

#if defined(TCL_COMPILE_DEBUG)
    if (tclTraceExec >= 1) {
	if (isLambda) {
	    fprintf(stdout, "Calling lambda ");
	} else {
	    fprintf(stdout, "Calling proc ");
	}
	for (i = 0; i < framePtr->objc; i++) {
	    TclPrintObject(stdout, framePtr->objv[i], 15);
	    fprintf(stdout, " ");
	}
	fprintf(stdout, "\n");
	fflush(stdout);
    }
#endif /*TCL_COMPILE_DEBUG*/

    /*
     * Invoke the commands in the procedure's body.
     */

    procPtr->refCount++;

    /*
     * TIP #280: No need to set the invoking context here. The body has
     * already been compiled, so the part of CompEvalObj using it is bypassed.
     */

    result = TclCompEvalObj(interp, procPtr->bodyPtr, NULL, 0);
    procPtr->refCount--;
    if (procPtr->refCount <= 0) {
	TclProcCleanupProc(procPtr);
    }

    /*
     * If the procedure is completing normally, we can skip directly to the
     * part where we clean up any associated memory.
     */

    if (result == TCL_OK) {
	goto procDone;
    }

    /*
     * Non-standard results are processed by passing them through quickly.
     * This means they all work as exceptions, unwinding the stack quickly and
     * neatly. Who knows how well they are handled by third-party code
     * though...
     */

    if ((result > TCL_CONTINUE) || (result < TCL_OK)) {
	goto procDone;
    }

    /*
     * If it is a 'return', do the TIP#90 processing now.
     */

    if (result == TCL_RETURN) {
	result = TclUpdateReturnInfo((Interp *) interp);
	goto procDone;
    }

    /*
     * Must be an error, a 'break' or a 'continue'. It's an error to get to
     * this point from a 'break' or 'continue' though, so transform to an
     * error now.
     */

    if (result != TCL_ERROR) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "invoked \"",
		((result == TCL_BREAK) ? "break" : "continue"),
		"\" outside of a loop", NULL);
	result = TCL_ERROR;
    }

    /*
     * Now it _must_ be an error, so we need to log it as such. This means
     * filling out the error trace.
     */

    (*errorProc)(interp, procNameObj);

  procDone:
    /*
     * Free the stack-allocated compiled locals and CallFrame. It is important
     * to pop the call frame without freeing it first: the compiledLocals
     * cannot be freed before the frame is popped, as the local variables must
     * be deleted. But the compiledLocals must be freed first, as they were
     * allocated later on the stack.
     */

    Tcl_PopCallFrame(interp);		/* Pop but do not free. */
    TclStackFree(interp);		/* Free compiledLocals. */
    TclStackFree(interp);		/* Free CallFrame. */
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclProcCompileProc --
 *
 *	Called just before a procedure is executed to compile the body to byte
 *	codes. If the type of the body is not "byte code" or if the compile
 *	conditions have changed (namespace context, epoch counters, etc.) then
 *	the body is recompiled. Otherwise, this function does nothing.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May change the internal representation of the body object to compiled
 *	code.
 *
 *----------------------------------------------------------------------
 */

int
TclProcCompileProc(
    Tcl_Interp *interp,		/* Interpreter containing procedure. */
    Proc *procPtr,		/* Data associated with procedure. */
    Tcl_Obj *bodyPtr,		/* Body of proc. (Usually procPtr->bodyPtr,
 				 * but could be any code fragment compiled in
 				 * the context of this procedure.) */
    Namespace *nsPtr,		/* Namespace containing procedure. */
    CONST char *description,	/* string describing this body of code. */
    CONST char *procName)	/* Name of this procedure. */
{
    return ProcCompileProc(interp, procPtr, bodyPtr, nsPtr, description,
	    procName, NULL);
}

static int
ProcCompileProc(
    Tcl_Interp *interp,		/* Interpreter containing procedure. */
    Proc *procPtr,		/* Data associated with procedure. */
    Tcl_Obj *bodyPtr,		/* Body of proc. (Usually procPtr->bodyPtr,
 				 * but could be any code fragment compiled in
 				 * the context of this procedure.) */
    Namespace *nsPtr,		/* Namespace containing procedure. */
    CONST char *description,	/* string describing this body of code. */
    CONST char *procName,	/* Name of this procedure. */
    Proc **procPtrPtr)		/* Points to storage where a replacement
				 * (Proc *) value may be written. */
{
    Interp *iPtr = (Interp *) interp;
    int i, result;
    Tcl_CallFrame *framePtr;
    Proc *saveProcPtr;
    ByteCode *codePtr = bodyPtr->internalRep.otherValuePtr;
    CompiledLocal *localPtr;

    /*
     * If necessary, compile the procedure's body. The compiler will allocate
     * frame slots for the procedure's non-argument local variables. If the
     * ByteCode already exists, make sure it hasn't been invalidated by
     * someone redefining a core command (this might make the compiled code
     * wrong). Also, if the code was compiled in/for a different interpreter,
     * we recompile it. Note that compiling the body might increase
     * procPtr->numCompiledLocals if new local variables are found while
     * compiling.
     *
     * Precompiled procedure bodies, however, are immutable and therefore they
     * are not recompiled, even if things have changed.
     */

    if (bodyPtr->typePtr == &tclByteCodeType) {
 	if (((Interp *) *codePtr->interpHandle == iPtr)
		&& (codePtr->compileEpoch == iPtr->compileEpoch)
		&& (codePtr->nsPtr == nsPtr)) {
	    return TCL_OK;
	} else {
	    if (codePtr->flags & TCL_BYTECODE_PRECOMPILED) {
		if ((Interp *) *codePtr->interpHandle != iPtr) {
		    Tcl_AppendResult(interp,
			    "a precompiled script jumped interps", NULL);
		    return TCL_ERROR;
		}
		codePtr->compileEpoch = iPtr->compileEpoch;
		codePtr->nsPtr = nsPtr;
	    } else {
		bodyPtr->typePtr->freeIntRepProc(bodyPtr);
		bodyPtr->typePtr = NULL;
	    }
 	}
    }
    if (bodyPtr->typePtr != &tclByteCodeType) {
#ifdef TCL_COMPILE_DEBUG
 	if (tclTraceCompile >= 1) {
 	    /*
 	     * Display a line summarizing the top level command we are about
 	     * to compile.
 	     */

	    Tcl_Obj *message;

	    TclNewLiteralStringObj(message, "Compiling ");
	    Tcl_IncrRefCount(message);
	    Tcl_AppendStringsToObj(message, description, " \"", NULL);
	    Tcl_AppendLimitedToObj(message, procName, -1, 50, NULL);
 	    fprintf(stdout, "%s\"\n", TclGetString(message));
	    Tcl_DecrRefCount(message);
 	}
#endif

 	/*
 	 * Plug the current procPtr into the interpreter and coerce the code
 	 * body to byte codes. The interpreter needs to know which proc it's
 	 * compiling so that it can access its list of compiled locals.
 	 *
 	 * TRICKY NOTE: Be careful to push a call frame with the proper
 	 *   namespace context, so that the byte codes are compiled in the
 	 *   appropriate class context.
 	 */

 	saveProcPtr = iPtr->compiledProcPtr;

	if (procPtrPtr != NULL && procPtr->refCount > 1) {
	    Tcl_Command token;
	    Tcl_CmdInfo info;
	    Proc *newProc = (Proc *) ckalloc(sizeof(Proc));

	    newProc->iPtr = procPtr->iPtr;
	    newProc->refCount = 1;
	    newProc->cmdPtr = procPtr->cmdPtr;
	    token = (Tcl_Command) newProc->cmdPtr;
	    newProc->bodyPtr = Tcl_DuplicateObj(bodyPtr);
	    bodyPtr = newProc->bodyPtr;
	    Tcl_IncrRefCount(bodyPtr);
	    newProc->numArgs = procPtr->numArgs;

	    newProc->numCompiledLocals = newProc->numArgs;
	    newProc->firstLocalPtr = NULL;
	    newProc->lastLocalPtr = NULL;
	    localPtr = procPtr->firstLocalPtr;
	    for (i=0; i<newProc->numArgs; i++, localPtr=localPtr->nextPtr) {
		CompiledLocal *copy = (CompiledLocal *) ckalloc((unsigned)
			(sizeof(CompiledLocal) - sizeof(localPtr->name)
			+ localPtr->nameLength + 1));

		if (newProc->firstLocalPtr == NULL) {
		    newProc->firstLocalPtr = newProc->lastLocalPtr = copy;
		} else {
		    newProc->lastLocalPtr->nextPtr = copy;
		    newProc->lastLocalPtr = copy;
		}
		copy->nextPtr = NULL;
		copy->nameLength = localPtr->nameLength;
		copy->frameIndex = localPtr->frameIndex;
		copy->flags = localPtr->flags;
		copy->defValuePtr = localPtr->defValuePtr;
		if (copy->defValuePtr) {
		    Tcl_IncrRefCount(copy->defValuePtr);
		}
		copy->resolveInfo = localPtr->resolveInfo;
		strcpy(copy->name, localPtr->name);
	    }

	    /* Reset the ClientData */
	    Tcl_GetCommandInfoFromToken(token, &info);
	    if (info.objClientData == (ClientData) procPtr) {
		info.objClientData = (ClientData) newProc;
	    }
	    if (info.clientData == (ClientData) procPtr) {
		info.clientData = (ClientData) newProc;
	    }
	    if (info.deleteData == (ClientData) procPtr) {
		info.deleteData = (ClientData) newProc;
	    }
	    Tcl_SetCommandInfoFromToken(token, &info);

	    procPtr->refCount--;
	    *procPtrPtr = procPtr = newProc;
	}
 	iPtr->compiledProcPtr = procPtr;

 	result = TclPushStackFrame(interp, &framePtr,
		(Tcl_Namespace *) nsPtr, /* isProcCallFrame */ 0);

 	if (result == TCL_OK) {
	    /*
	     * TIP #280: We get the invoking context from the cmdFrame which
	     * was saved by 'Tcl_ProcObjCmd' (using linePBodyPtr).
	     */

	    Tcl_HashEntry *hePtr = Tcl_FindHashEntry(iPtr->linePBodyPtr,
		    (char *) procPtr);

	    /*
	     * Constructed saved frame has body as word 0. See Tcl_ProcObjCmd.
	     */

	    iPtr->invokeWord = 0;
	    iPtr->invokeCmdFramePtr =
		    (hePtr ? (CmdFrame *) Tcl_GetHashValue(hePtr) : NULL);
	    result = tclByteCodeType.setFromAnyProc(interp, bodyPtr);
	    iPtr->invokeCmdFramePtr = NULL;
	    TclPopStackFrame(interp);
	}

 	iPtr->compiledProcPtr = saveProcPtr;

 	if (result != TCL_OK) {
 	    if (result == TCL_ERROR) {
		int length = strlen(procName);
		int limit = 50;
		int overflow = (length > limit);

		Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
			"\n    (compiling %s \"%.*s%s\", line %d)",
			description, (overflow ? limit : length), procName,
			(overflow ? "..." : ""), interp->errorLine));
	    }
 	    return result;
 	}
    } else if (codePtr->nsEpoch != nsPtr->resolverEpoch) {
	/*
	 * The resolver epoch has changed, but we only need to invalidate the
	 * resolver cache.
	 */

	codePtr->flags |= TCL_BYTECODE_RESOLVE_VARS;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeProcError --
 *
 *	Function called by TclObjInterpProc to create the stack information
 *	upon an error from a procedure.
 *
 * Results:
 *	The interpreter's error info trace is set to a value that supplements
 *	the error code.
 *
 * Side effects:
 *	none.
 *
 *----------------------------------------------------------------------
 */

static void
MakeProcError(
    Tcl_Interp *interp,		/* The interpreter in which the procedure was
				 * called. */
    Tcl_Obj *procNameObj)	/* Name of the procedure. Used for error
				 * messages and trace information. */
{
    int overflow, limit = 60, nameLen;
    const char *procName = Tcl_GetStringFromObj(procNameObj, &nameLen);

    overflow = (nameLen > limit);
    Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
	    "\n    (procedure \"%.*s%s\" line %d)",
	    (overflow ? limit : nameLen), procName,
	    (overflow ? "..." : ""), interp->errorLine));
}

/*
 *----------------------------------------------------------------------
 *
 * TclProcDeleteProc --
 *
 *	This function is invoked just before a command procedure is removed
 *	from an interpreter. Its job is to release all the resources allocated
 *	to the procedure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets freed, unless the procedure is actively being executed.
 *	In this case the cleanup is delayed until the last call to the current
 *	procedure completes.
 *
 *----------------------------------------------------------------------
 */

void
TclProcDeleteProc(
    ClientData clientData)	/* Procedure to be deleted. */
{
    Proc *procPtr = (Proc *) clientData;

    procPtr->refCount--;
    if (procPtr->refCount <= 0) {
	TclProcCleanupProc(procPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclProcCleanupProc --
 *
 *	This function does all the real work of freeing up a Proc structure.
 *	It's called only when the structure's reference count becomes zero.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets freed.
 *
 *----------------------------------------------------------------------
 */

void
TclProcCleanupProc(
    register Proc *procPtr)	/* Procedure to be deleted. */
{
    register CompiledLocal *localPtr;
    Tcl_Obj *bodyPtr = procPtr->bodyPtr;
    Tcl_Obj *defPtr;
    Tcl_ResolvedVarInfo *resVarInfo;
    Tcl_HashEntry *hePtr = NULL;
    CmdFrame *cfPtr = NULL;
    Interp *iPtr = procPtr->iPtr;

    if (bodyPtr != NULL) {
	Tcl_DecrRefCount(bodyPtr);
    }
    for (localPtr = procPtr->firstLocalPtr; localPtr != NULL; ) {
	CompiledLocal *nextPtr = localPtr->nextPtr;

	resVarInfo = localPtr->resolveInfo;
	if (resVarInfo) {
	    if (resVarInfo->deleteProc) {
		(*resVarInfo->deleteProc)(resVarInfo);
	    } else {
		ckfree((char *) resVarInfo);
	    }
	}

	if (localPtr->defValuePtr != NULL) {
	    defPtr = localPtr->defValuePtr;
	    Tcl_DecrRefCount(defPtr);
	}
	ckfree((char *) localPtr);
	localPtr = nextPtr;
    }
    ckfree((char *) procPtr);

    /*
     * TIP #280: Release the location data associated with this Proc
     * structure, if any. The interpreter may not exist (For example for
     * procbody structurues created by tbcload.
     */

    if (!iPtr) {
	return;
    }

    hePtr = Tcl_FindHashEntry(iPtr->linePBodyPtr, (char *) procPtr);
    if (!hePtr) {
	return;
    }

    cfPtr = (CmdFrame *) Tcl_GetHashValue(hePtr);

    if (cfPtr->type == TCL_LOCATION_SOURCE) {
	Tcl_DecrRefCount(cfPtr->data.eval.path);
	cfPtr->data.eval.path = NULL;
    }
    ckfree((char *) cfPtr->line);
    cfPtr->line = NULL;
    ckfree((char *) cfPtr);
    Tcl_DeleteHashEntry(hePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclUpdateReturnInfo --
 *
 *	This function is called when procedures return, and at other points
 *	where the TCL_RETURN code is used. It examines the returnLevel and
 *	returnCode to determine the real return status.
 *
 * Results:
 *	The return value is the true completion code to use for the procedure
 *	or script, instead of TCL_RETURN.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclUpdateReturnInfo(
    Interp *iPtr)		/* Interpreter for which TCL_RETURN exception
				 * is being processed. */
{
    int code = TCL_RETURN;

    iPtr->returnLevel--;
    if (iPtr->returnLevel < 0) {
	Tcl_Panic("TclUpdateReturnInfo: negative return level");
    }
    if (iPtr->returnLevel == 0) {
	/*
	 * Now we've reached the level to return the requested -code.
	 */

	code = iPtr->returnCode;
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetObjInterpProc --
 *
 *	Returns a pointer to the TclObjInterpProc function; this is different
 *	from the value obtained from the TclObjInterpProc reference on systems
 *	like Windows where import and export versions of a function exported
 *	by a DLL exist.
 *
 * Results:
 *	Returns the internal address of the TclObjInterpProc function.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TclObjCmdProcType
TclGetObjInterpProc(void)
{
    return (TclObjCmdProcType) TclObjInterpProc;
}

/*
 *----------------------------------------------------------------------
 *
 * TclNewProcBodyObj --
 *
 *	Creates a new object, of type "procbody", whose internal
 *	representation is the given Proc struct. The newly created object's
 *	reference count is 0.
 *
 * Results:
 *	Returns a pointer to a newly allocated Tcl_Obj, NULL on error.
 *
 * Side effects:
 *	The reference count in the ByteCode attached to the Proc is bumped up
 *	by one, since the internal rep stores a pointer to it.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclNewProcBodyObj(
    Proc *procPtr)		/* the Proc struct to store as the internal
				 * representation. */
{
    Tcl_Obj *objPtr;

    if (!procPtr) {
	return NULL;
    }

    TclNewObj(objPtr);
    if (objPtr) {
	objPtr->typePtr = &tclProcBodyType;
	objPtr->internalRep.otherValuePtr = procPtr;

	procPtr->refCount++;
    }

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ProcBodyDup --
 *
 *	Tcl_ObjType's Dup function for the proc body object. Bumps the
 *	reference count on the Proc stored in the internal representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets up the object in dupPtr to be a duplicate of the one in srcPtr.
 *
 *----------------------------------------------------------------------
 */

static void
ProcBodyDup(
    Tcl_Obj *srcPtr,		/* object to copy */
    Tcl_Obj *dupPtr)		/* target object for the duplication */
{
    Proc *procPtr = srcPtr->internalRep.otherValuePtr;

    dupPtr->typePtr = &tclProcBodyType;
    dupPtr->internalRep.otherValuePtr = procPtr;
    procPtr->refCount++;
}

/*
 *----------------------------------------------------------------------
 *
 * ProcBodyFree --
 *
 *	Tcl_ObjType's Free function for the proc body object. The reference
 *	count on its Proc struct is decreased by 1; if the count reaches 0,
 *	the proc is freed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the reference count on the Proc struct reaches 0, the struct is
 *	freed.
 *
 *----------------------------------------------------------------------
 */

static void
ProcBodyFree(
    Tcl_Obj *objPtr)		/* the object to clean up */
{
    Proc *procPtr = objPtr->internalRep.otherValuePtr;

    procPtr->refCount--;
    if (procPtr->refCount <= 0) {
	TclProcCleanupProc(procPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DupLambdaInternalRep, FreeLambdaInternalRep, SetLambdaFromAny --
 *
 *	How to manage the internal representations of lambda term objects.
 *	Syntactically they look like a two- or three-element list, where the
 *	first element is the formal arguments, the second is the the body, and
 *	the (optional) third is the namespace to execute the lambda term
 *	within (the global namespace is assumed if it is absent).
 *
 *----------------------------------------------------------------------
 */

static void
DupLambdaInternalRep(
    Tcl_Obj *srcPtr,		/* Object with internal rep to copy. */
    register Tcl_Obj *copyPtr)	/* Object with internal rep to set. */
{
    Proc *procPtr = srcPtr->internalRep.twoPtrValue.ptr1;
    Tcl_Obj *nsObjPtr = srcPtr->internalRep.twoPtrValue.ptr2;

    copyPtr->internalRep.twoPtrValue.ptr1 = procPtr;
    copyPtr->internalRep.twoPtrValue.ptr2 = nsObjPtr;

    procPtr->refCount++;
    Tcl_IncrRefCount(nsObjPtr);
    copyPtr->typePtr = &lambdaType;
}

static void
FreeLambdaInternalRep(
    register Tcl_Obj *objPtr)	/* CmdName object with internal representation
				 * to free. */
{
    Proc *procPtr = objPtr->internalRep.twoPtrValue.ptr1;
    Tcl_Obj *nsObjPtr = objPtr->internalRep.twoPtrValue.ptr2;

    procPtr->refCount--;
    if (procPtr->refCount == 0) {
	TclProcCleanupProc(procPtr);
    }
    TclDecrRefCount(nsObjPtr);
}

static int
SetLambdaFromAny(
    Tcl_Interp *interp,		/* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr)	/* The object to convert. */
{
    Interp *iPtr = (Interp *) interp;
    char *name;
    Tcl_Obj *argsPtr, *bodyPtr, *nsObjPtr, **objv, *errPtr;
    int objc, result;
    Proc *procPtr;

    /*
     * Convert objPtr to list type first; if it cannot be converted, or if its
     * length is not 2, then it cannot be converted to lambdaType.
     */

    result = Tcl_ListObjGetElements(interp, objPtr, &objc, &objv);
    if ((result != TCL_OK) || ((objc != 2) && (objc != 3))) {
	TclNewLiteralStringObj(errPtr, "can't interpret \"");
	Tcl_AppendObjToObj(errPtr, objPtr);
	Tcl_AppendToObj(errPtr, "\" as a lambda expression", -1);
	Tcl_SetObjResult(interp, errPtr);
	return TCL_ERROR;
    }

    argsPtr = objv[0];
    bodyPtr = objv[1];

    /*
     * Create and initialize the Proc struct. The cmdPtr field is set to NULL
     * to signal that this is an anonymous function.
     */

    name = TclGetString(objPtr);

    if (TclCreateProc(interp, /*ignored nsPtr*/ NULL, name, argsPtr, bodyPtr,
	    &procPtr) != TCL_OK) {
	Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
		"\n    (parsing lambda expression \"%s\")", name));
	return TCL_ERROR;
    }

    /*
     * CAREFUL: TclCreateProc returns refCount==1! [Bug 1578454]
     * procPtr->refCount = 1;
     */

    procPtr->cmdPtr = NULL;

    /*
     * TIP #280: Remember the line the apply body is starting on. In a Byte
     * code context we ask the engine to provide us with the necessary
     * information. This is for the initialization of the byte code compiler
     * when the body is used for the first time.
     *
     * NOTE: The body is the second word in the 'objPtr'. Its location,
     * accessible through 'context.line[1]' (see below) is therefore only the
     * first approximation of the actual line the body is on. We have to use
     * the string rep of the 'objPtr' to determine the exact line. This is
     * available already through 'name'. Use 'TclListLines', see 'switch'
     * (tclCmdMZ.c).
     *
     * This code is nearly identical to the #280 code in Tcl_ProcObjCmd, see
     * this file. The differences are the different index of the body in the
     * line array of the context, and the special processing mentioned in the
     * previous paragraph to track into the list. Find a way to factor the
     * common elements into a single function.
     */

    if (iPtr->cmdFramePtr) {
	CmdFrame context = *iPtr->cmdFramePtr;

	if (context.type == TCL_LOCATION_BC) {
	    TclGetSrcInfoForPc(&context);

	    /*
	     * May get path in context.
	     */
	} else if (context.type == TCL_LOCATION_SOURCE) {
	    /*
	     * Context now holds another reference.
	     */

	    Tcl_IncrRefCount(context.data.eval.path);
	}

	/*
	 * type == TCL_LOCATION_PREBC implies that 'line' is NULL here! We
	 * cannot assume that 'line' is valid here, we have to check.
	 */

	if ((context.type == TCL_LOCATION_SOURCE) && context.line
		&& (context.nline >= 2) && (context.line[1] >= 0)) {
	    int isNew, buf[2];
	    CmdFrame *cfPtr = (CmdFrame *) ckalloc(sizeof(CmdFrame));

	    /*
	     * Move from approximation (line of list cmd word) to actual
	     * location (line of 2nd list element).
	     */

	    TclListLines(name, context.line[1], 2, buf);

	    cfPtr->level = -1;
	    cfPtr->type = context.type;
	    cfPtr->line = (int *) ckalloc(sizeof(int));
	    cfPtr->line[0] = buf[1];
	    cfPtr->nline = 1;
	    cfPtr->framePtr = NULL;
	    cfPtr->nextPtr = NULL;

	    if (context.type == TCL_LOCATION_SOURCE) {
		cfPtr->data.eval.path = context.data.eval.path;

		/*
		 * Transfer of reference. The reference going away (release of
		 * the context) is replaced by the reference in the
		 * constructed cmdframe.
		 */
	    } else {
		cfPtr->type = TCL_LOCATION_EVAL;
		cfPtr->data.eval.path = NULL;
	    }

	    cfPtr->cmd.str.cmd = NULL;
	    cfPtr->cmd.str.len = 0;

	    Tcl_SetHashValue(Tcl_CreateHashEntry(iPtr->linePBodyPtr,
		    (char *) procPtr, &isNew), cfPtr);
	}
    }

    /*
     * Set the namespace for this lambda: given by objv[2] understood as a
     * global reference, or else global per default.
     */

    if (objc == 2) {
	TclNewLiteralStringObj(nsObjPtr, "::");
    } else {
	char *nsName = Tcl_GetString(objv[2]);

	if ((*nsName != ':') || (*(nsName+1) != ':')) {
	    TclNewLiteralStringObj(nsObjPtr, "::");
	    Tcl_AppendObjToObj(nsObjPtr, objv[2]);
	} else {
	    nsObjPtr = objv[2];
	}
    }

    Tcl_IncrRefCount(nsObjPtr);

    /*
     * Free the list internalrep of objPtr - this will free argsPtr, but
     * bodyPtr retains a reference from the Proc structure. Then finish the
     * conversion to lambdaType.
     */

    objPtr->typePtr->freeIntRepProc(objPtr);

    objPtr->internalRep.twoPtrValue.ptr1 = procPtr;
    objPtr->internalRep.twoPtrValue.ptr2 = nsObjPtr;
    objPtr->typePtr = &lambdaType;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ApplyObjCmd --
 *
 *	This object-based function is invoked to process the "apply" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result value.
 *
 * Side effects:
 *	Depends on the content of the lambda term (i.e., objv[1]).
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ApplyObjCmd(
    ClientData dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[])	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    Proc *procPtr = NULL;
    Tcl_Obj *lambdaPtr, *nsObjPtr, *errPtr;
    int result;
    Command cmd;
    Tcl_Namespace *nsPtr;
    int isRootEnsemble;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "lambdaExpr ?arg1 arg2 ...?");
	return TCL_ERROR;
    }

    /*
     * Set lambdaPtr, convert it to lambdaType in the current interp if
     * necessary.
     */

    lambdaPtr = objv[1];
    if (lambdaPtr->typePtr == &lambdaType) {
	procPtr = lambdaPtr->internalRep.twoPtrValue.ptr1;
    }

#define JOE_EXTENSION 0
#if JOE_EXTENSION
    else {
	/*
	 * Joe English's suggestion to allow cmdNames to function as lambdas.
	 * Also requires making tclCmdNameType non-static in tclObj.c
	 */

	Tcl_Obj *elemPtr;
	int numElem;

	if ((lambdaPtr->typePtr == &tclCmdNameType) ||
		(Tcl_ListObjGetElements(interp, lambdaPtr, &numElem,
		&elemPtr) == TCL_OK && numElem == 1)) {
	    return Tcl_EvalObjv(interp, objc-1, objv+1, 0);
	}
    }
#endif

    if ((procPtr == NULL) || (procPtr->iPtr != iPtr)) {
	result = SetLambdaFromAny(interp, lambdaPtr);
	if (result != TCL_OK) {
	    return result;
	}
	procPtr = lambdaPtr->internalRep.twoPtrValue.ptr1;
    }

    memset(&cmd, 0, sizeof(Command));
    procPtr->cmdPtr = &cmd;

    /*
     * TIP#280 HACK!
     *
     * Using cmd.clientData to remember the 'lambdaPtr' for 'info frame'.  The
     * InfoFrameCmd will detect this case by testing cmd.hPtr for NULL. This
     * condition holds here because of the 'memset' above, and nowhere else.
     * Regular commands always have a valid 'hPtr', and lambda's never.
     */

    cmd.clientData = (ClientData) lambdaPtr;

    /*
     * Find the namespace where this lambda should run, and push a call frame
     * for that namespace. Note that TclObjInterpProc() will pop it.
     */

    nsObjPtr = lambdaPtr->internalRep.twoPtrValue.ptr2;
    result = TclGetNamespaceFromObj(interp, nsObjPtr, &nsPtr);
    if (result != TCL_OK) {
	return result;
    }

    if (nsPtr == NULL) {
	TclNewLiteralStringObj(errPtr, "cannot find namespace \"");
	Tcl_AppendObjToObj(errPtr, nsObjPtr);
	Tcl_AppendToObj(errPtr, "\"", -1);
	Tcl_SetObjResult(interp, errPtr);
	return TCL_ERROR;
    }

    cmd.nsPtr = (Namespace *) nsPtr;

    isRootEnsemble = (iPtr->ensembleRewrite.sourceObjs == NULL);
    if (isRootEnsemble) {
	iPtr->ensembleRewrite.sourceObjs = objv;
	iPtr->ensembleRewrite.numRemovedObjs = 1;
	iPtr->ensembleRewrite.numInsertedObjs = 0;
    } else {
	iPtr->ensembleRewrite.numInsertedObjs -= 1;
    }

    result = ObjInterpProcEx((ClientData) procPtr, interp, objc, objv, 1,
	    &MakeLambdaError);

    if (isRootEnsemble) {
	iPtr->ensembleRewrite.sourceObjs = NULL;
	iPtr->ensembleRewrite.numRemovedObjs = 0;
	iPtr->ensembleRewrite.numInsertedObjs = 0;
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeLambdaError --
 *
 *	Function called by TclObjInterpProc to create the stack information
 *	upon an error from a lambda term.
 *
 * Results:
 *	The interpreter's error info trace is set to a value that supplements
 *	the error code.
 *
 * Side effects:
 *	none.
 *
 *----------------------------------------------------------------------
 */

static void
MakeLambdaError(
    Tcl_Interp *interp,		/* The interpreter in which the procedure was
				 * called. */
    Tcl_Obj *procNameObj)	/* Name of the procedure. Used for error
				 * messages and trace information. */
{
    int overflow, limit = 60, nameLen;
    const char *procName = Tcl_GetStringFromObj(procNameObj, &nameLen);

    overflow = (nameLen > limit);
    Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
	    "\n    (lambda term \"%.*s%s\" line %d)",
	    (overflow ? limit : nameLen), procName,
	    (overflow ? "..." : ""), interp->errorLine));
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
