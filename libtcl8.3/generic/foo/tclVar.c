/* 
 * tclVar.c --
 *
 *	This file contains routines that implement Tcl variables
 *	(both scalars and arrays).
 *
 *	The implementation of arrays is modelled after an initial
 *	implementation by Mark Diekhans and Karl Lehenbauer.
 *
 * Copyright (c) 1987-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * The strings below are used to indicate what went wrong when a
 * variable access is denied.
 */

static char *noSuchVar =	"no such variable";
static char *isArray =		"variable is array";
static char *needArray =	"variable isn't array";
static char *noSuchElement =	"no such element in array";
static char *danglingElement =  "upvar refers to element in deleted array";
static char *danglingVar =     "upvar refers to variable in deleted namespace";
static char *badNamespace =	"parent namespace doesn't exist";
static char *missingName =	"missing variable name";
static char *isArrayElement =   "name refers to an element in an array";

/*
 * Forward references to procedures defined later in this file:
 */

static  char *		CallTraces _ANSI_ARGS_((Interp *iPtr, Var *arrayPtr,
			    Var *varPtr, char *part1, char *part2,
			    int flags));
static void		CleanupVar _ANSI_ARGS_((Var *varPtr,
			    Var *arrayPtr));
static void		DeleteSearches _ANSI_ARGS_((Var *arrayVarPtr));
static void		DeleteArray _ANSI_ARGS_((Interp *iPtr,
			    char *arrayName, Var *varPtr, int flags));
static int		MakeUpvar _ANSI_ARGS_((
			    Interp *iPtr, CallFrame *framePtr,
			    char *otherP1, char *otherP2, int otherFlags,
			    char *myName, int myFlags));
static Var *		NewVar _ANSI_ARGS_((void));
static ArraySearch *	ParseSearchId _ANSI_ARGS_((Tcl_Interp *interp,
			    Var *varPtr, char *varName, char *string));
static void		VarErrMsg _ANSI_ARGS_((Tcl_Interp *interp,
			    char *part1, char *part2, char *operation,
			    char *reason));

/*
 *----------------------------------------------------------------------
 *
 * TclLookupVar --
 *
 *	This procedure is used by virtually all of the variable code to
 *	locate a variable given its name(s).
 *
 * Results:
 *	The return value is a pointer to the variable structure indicated by
 *	part1 and part2, or NULL if the variable couldn't be found. If the
 *	variable is found, *arrayPtrPtr is filled in with the address of the
 *	variable structure for the array that contains the variable (or NULL
 *	if the variable is a scalar). If the variable can't be found and
 *	either createPart1 or createPart2 are 1, a new as-yet-undefined
 *	(VAR_UNDEFINED) variable structure is created, entered into a hash
 *	table, and returned.
 *
 *	If the variable isn't found and creation wasn't specified, or some
 *	other error occurs, NULL is returned and an error message is left in
 *	the interp's result if TCL_LEAVE_ERR_MSG is set in flags. 
 *
 *	Note: it's possible for the variable returned to be VAR_UNDEFINED
 *	even if createPart1 or createPart2 are 1 (these only cause the hash
 *	table entry or array to be created). For example, the variable might
 *	be a global that has been unset but is still referenced by a
 *	procedure, or a variable that has been unset but it only being kept
 *	in existence (if VAR_UNDEFINED) by a trace.
 *
 * Side effects:
 *	New hashtable entries may be created if createPart1 or createPart2
 *	are 1.
 *
 *----------------------------------------------------------------------
 */

Var *
TclLookupVar(interp, part1, part2, flags, msg, createPart1, createPart2,
        arrayPtrPtr)
    Tcl_Interp *interp;		/* Interpreter to use for lookup. */
    register char *part1;	/* If part2 isn't NULL, this is the name of
				 * an array. Otherwise, this
				 * is a full variable name that could
				 * include a parenthesized array element. */
    char *part2;		/* Name of element within array, or NULL. */
    int flags;			/* Only TCL_GLOBAL_ONLY, TCL_NAMESPACE_ONLY,
				 * and TCL_LEAVE_ERR_MSG bits matter. */
    char *msg;			/* Verb to use in error messages, e.g.
				 * "read" or "set". Only needed if
				 * TCL_LEAVE_ERR_MSG is set in flags. */
    int createPart1;		/* If 1, create hash table entry for part 1
				 * of name, if it doesn't already exist. If
				 * 0, return error if it doesn't exist. */
    int createPart2;		/* If 1, create hash table entry for part 2
				 * of name, if it doesn't already exist. If
				 * 0, return error if it doesn't exist. */
    Var **arrayPtrPtr;		/* If the name refers to an element of an
				 * array, *arrayPtrPtr gets filled in with
				 * address of array variable. Otherwise
				 * this is set to NULL. */
{
    Interp *iPtr = (Interp *) interp;
    CallFrame *varFramePtr = iPtr->varFramePtr;
				/* Points to the procedure call frame whose
				 * variables are currently in use. Same as
				 * the current procedure's frame, if any,
				 * unless an "uplevel" is executing. */
    Tcl_HashTable *tablePtr;	/* Points to the hashtable, if any, in which
				 * to look up the variable. */
    Tcl_Var var;                /* Used to search for global names. */
    Var *varPtr;		/* Points to the Var structure returned for
    				 * the variable. */
    char *elName;		/* Name of array element or NULL; may be
				 * same as part2, or may be openParen+1. */
    char *openParen, *closeParen;
                                /* If this procedure parses a name into
				 * array and index, these point to the
				 * parens around the index.  Otherwise they
				 * are NULL. These are needed to restore
				 * the parens after parsing the name. */
    Namespace *varNsPtr, *cxtNsPtr, *dummy1Ptr, *dummy2Ptr;
    ResolverScheme *resPtr;
    Tcl_HashEntry *hPtr;
    register char *p;
    int new, i, result;

    varPtr = NULL;
    *arrayPtrPtr = NULL;
    openParen = closeParen = NULL;
    varNsPtr = NULL;		/* set non-NULL if a nonlocal variable */

    /*
     * Parse part1 into array name and index.
     * Always check if part1 is an array element name and allow it only if
     * part2 is not given.   
     * (if one does not care about creating array elements that can't be used
     *  from tcl, and prefer slightly better performance, one can put
     *  the following in an   if (part2 == NULL) { ... } block and remove
     *  the part2's test and error reporting  or move that code in array set)
     */

    elName = part2;
    for (p = part1; *p ; p++) {
	if (*p == '(') {
	    openParen = p;
	    do {
		p++;
	    } while (*p != '\0');
	    p--;
	    if (*p == ')') {
		if (part2 != NULL) {
		    openParen = NULL;
		    if (flags & TCL_LEAVE_ERR_MSG) {
			VarErrMsg(interp, part1, part2, msg, needArray);
		    }
		    goto done;
		}
		closeParen = p;
		*openParen = 0;
		elName = openParen+1;
	    } else {
		openParen = NULL;
	    }
	    break;
	}
    }

    /*
     * If this namespace has a variable resolver, then give it first
     * crack at the variable resolution.  It may return a Tcl_Var
     * value, it may signal to continue onward, or it may signal
     * an error.
     */
    if ((flags & TCL_GLOBAL_ONLY) != 0 || iPtr->varFramePtr == NULL) {
        cxtNsPtr = iPtr->globalNsPtr;
    } else {
        cxtNsPtr = iPtr->varFramePtr->nsPtr;
    }

    if (cxtNsPtr->varResProc != NULL || iPtr->resolverPtr != NULL) {
        resPtr = iPtr->resolverPtr;

        if (cxtNsPtr->varResProc) {
            result = (*cxtNsPtr->varResProc)(interp, part1,
		    (Tcl_Namespace *) cxtNsPtr, flags, &var);
        } else {
            result = TCL_CONTINUE;
        }

        while (result == TCL_CONTINUE && resPtr) {
            if (resPtr->varResProc) {
                result = (*resPtr->varResProc)(interp, part1,
			(Tcl_Namespace *) cxtNsPtr, flags, &var);
            }
            resPtr = resPtr->nextPtr;
        }

        if (result == TCL_OK) {
            varPtr = (Var *) var;
            goto lookupVarPart2;
        } else if (result != TCL_CONTINUE) {
            return (Var *) NULL;
        }
    }

    /*
     * Look up part1. Look it up as either a namespace variable or as a
     * local variable in a procedure call frame (varFramePtr).
     * Interpret part1 as a namespace variable if:
     *    1) so requested by a TCL_GLOBAL_ONLY or TCL_NAMESPACE_ONLY flag,
     *    2) there is no active frame (we're at the global :: scope),
     *    3) the active frame was pushed to define the namespace context
     *       for a "namespace eval" or "namespace inscope" command,
     *    4) the name has namespace qualifiers ("::"s).
     * Otherwise, if part1 is a local variable, search first in the
     * frame's array of compiler-allocated local variables, then in its
     * hashtable for runtime-created local variables.
     *
     * If createPart1 and the variable isn't found, create the variable and,
     * if necessary, create varFramePtr's local var hashtable.
     */

    if (((flags & (TCL_GLOBAL_ONLY | TCL_NAMESPACE_ONLY)) != 0)
	    || (varFramePtr == NULL)
	    || !varFramePtr->isProcCallFrame
	    || (strstr(part1, "::") != NULL)) {
	char *tail;
	
	/*
	 * Don't pass TCL_LEAVE_ERR_MSG, we may yet create the variable,
	 * or otherwise generate our own error!
	 */
	var = Tcl_FindNamespaceVar(interp, part1, (Tcl_Namespace *) NULL,
		flags & ~TCL_LEAVE_ERR_MSG);
	if (var != (Tcl_Var) NULL) {
            varPtr = (Var *) var;
        }
	if (varPtr == NULL) {
	    if (createPart1) {   /* var wasn't found so create it  */
		TclGetNamespaceForQualName(interp, part1, (Namespace *) NULL,
			flags, &varNsPtr, &dummy1Ptr, &dummy2Ptr, &tail);

		if (varNsPtr == NULL) {
		    if (flags & TCL_LEAVE_ERR_MSG) {
			VarErrMsg(interp, part1, part2, msg, badNamespace);
		    }
		    goto done;
		}
		if (tail == NULL) {
		    if (flags & TCL_LEAVE_ERR_MSG) {
			VarErrMsg(interp, part1, part2, msg, missingName);
		    }
		    goto done;
		}
		hPtr = Tcl_CreateHashEntry(&varNsPtr->varTable, tail, &new);
		varPtr = NewVar();
		Tcl_SetHashValue(hPtr, varPtr);
		varPtr->hPtr = hPtr;
		varPtr->nsPtr = varNsPtr;
	    } else {		/* var wasn't found and not to create it */
		if (flags & TCL_LEAVE_ERR_MSG) {
		    VarErrMsg(interp, part1, part2, msg, noSuchVar);
		}
		goto done;
	    }
	}
    } else {			/* local var: look in frame varFramePtr */
	Proc *procPtr = varFramePtr->procPtr;
	int localCt = procPtr->numCompiledLocals;
	CompiledLocal *localPtr = procPtr->firstLocalPtr;
	Var *localVarPtr = varFramePtr->compiledLocals;
	int part1Len = strlen(part1);
	
	for (i = 0;  i < localCt;  i++) {
	    if (!TclIsVarTemporary(localPtr)) {
		register char *localName = localVarPtr->name;
		if ((part1[0] == localName[0])
		        && (part1Len == localPtr->nameLength)
		        && (strcmp(part1, localName) == 0)) {
		    varPtr = localVarPtr;
		    break;
		}
	    }
	    localVarPtr++;
	    localPtr = localPtr->nextPtr;
	}
	if (varPtr == NULL) {	/* look in the frame's var hash table */
	    tablePtr = varFramePtr->varTablePtr;
	    if (createPart1) {
		if (tablePtr == NULL) {
		    tablePtr = (Tcl_HashTable *)
			ckalloc(sizeof(Tcl_HashTable));
		    Tcl_InitHashTable(tablePtr, TCL_STRING_KEYS);
		    varFramePtr->varTablePtr = tablePtr;
		}
		hPtr = Tcl_CreateHashEntry(tablePtr, part1, &new);
		if (new) {
		    varPtr = NewVar();
		    Tcl_SetHashValue(hPtr, varPtr);
		    varPtr->hPtr = hPtr;
                    varPtr->nsPtr = NULL; /* a local variable */
		} else {
		    varPtr = (Var *) Tcl_GetHashValue(hPtr);
		}
	    } else {
		hPtr = NULL;
		if (tablePtr != NULL) {
		    hPtr = Tcl_FindHashEntry(tablePtr, part1);
		}
		if (hPtr == NULL) {
		    if (flags & TCL_LEAVE_ERR_MSG) {
			VarErrMsg(interp, part1, part2, msg, noSuchVar);
		    }
		    goto done;
		}
		varPtr = (Var *) Tcl_GetHashValue(hPtr);
	    }
	}
    }

    lookupVarPart2:
    if (openParen != NULL) {
	*openParen = '(';
	openParen = NULL;
    }

    /*
     * If varPtr is a link variable, we have a reference to some variable
     * that was created through an "upvar" or "global" command. Traverse
     * through any links until we find the referenced variable.
     */
	
    while (TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }

    /*
     * If we're not dealing with an array element, return varPtr.
     */
    
    if (elName == NULL) {
        goto done;
    }

    /*
     * We're dealing with an array element. Make sure the variable is an
     * array and look up the element (create the element if desired).
     */

    if (TclIsVarUndefined(varPtr) && !TclIsVarArrayElement(varPtr)) {
	if (!createPart1) {
	    if (flags & TCL_LEAVE_ERR_MSG) {
		VarErrMsg(interp, part1, part2, msg, noSuchVar);
	    }
	    varPtr = NULL;
	    goto done;
	}

	/*
	 * Make sure we are not resurrecting a namespace variable from a
	 * deleted namespace!
	 */
	if ((varPtr->flags & VAR_IN_HASHTABLE) && (varPtr->hPtr == NULL)) {
	    if (flags & TCL_LEAVE_ERR_MSG) {
		VarErrMsg(interp, part1, part2, msg, danglingVar);
	    }
	    varPtr = NULL;
	    goto done;
	}

	TclSetVarArray(varPtr);
	TclClearVarUndefined(varPtr);
	varPtr->value.tablePtr =
	    (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(varPtr->value.tablePtr, TCL_STRING_KEYS);
    } else if (!TclIsVarArray(varPtr)) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    VarErrMsg(interp, part1, part2, msg, needArray);
	}
	varPtr = NULL;
	goto done;
    }
    *arrayPtrPtr = varPtr;
    if (closeParen != NULL) {
	*closeParen = 0;
    }
    if (createPart2) {
	hPtr = Tcl_CreateHashEntry(varPtr->value.tablePtr, elName, &new);
	if (closeParen != NULL) {
	    *closeParen = ')';
	}
	if (new) {
	    if (varPtr->searchPtr != NULL) {
		DeleteSearches(varPtr);
	    }
	    varPtr = NewVar();
	    Tcl_SetHashValue(hPtr, varPtr);
	    varPtr->hPtr = hPtr;
	    varPtr->nsPtr = varNsPtr;
	    TclSetVarArrayElement(varPtr);
	}
    } else {
	hPtr = Tcl_FindHashEntry(varPtr->value.tablePtr, elName);
	if (closeParen != NULL) {
	    *closeParen = ')';
	}
	if (hPtr == NULL) {
	    if (flags & TCL_LEAVE_ERR_MSG) {
		VarErrMsg(interp, part1, part2, msg, noSuchElement);
	    }
	    varPtr = NULL;
	    goto done;
	}
    }
    varPtr = (Var *) Tcl_GetHashValue(hPtr);

    done:
    if (openParen != NULL) {
        *openParen = '(';
    }
    return varPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetVar --
 *
 *	Return the value of a Tcl variable as a string.
 *
 * Results:
 *	The return value points to the current value of varName as a string.
 *	If the variable is not defined or can't be read because of a clash
 *	in array usage then a NULL pointer is returned and an error message
 *	is left in the interp's result if the TCL_LEAVE_ERR_MSG flag is set.
 *	Note: the return value is only valid up until the next change to the
 *	variable; if you depend on the value lasting longer than that, then
 *	make yourself a private copy.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_GetVar(interp, varName, flags)
    Tcl_Interp *interp;		/* Command interpreter in which varName is
				 * to be looked up. */
    char *varName;		/* Name of a variable in interp. */
    int flags;			/* OR-ed combination of TCL_GLOBAL_ONLY,
				 * TCL_NAMESPACE_ONLY or TCL_LEAVE_ERR_MSG
				 * bits. */
{
    return Tcl_GetVar2(interp, varName, (char *) NULL, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetVar2 --
 *
 *	Return the value of a Tcl variable as a string, given a two-part
 *	name consisting of array name and element within array.
 *
 * Results:
 *	The return value points to the current value of the variable given
 *	by part1 and part2 as a string. If the specified variable doesn't
 *	exist, or if there is a clash in array usage, then NULL is returned
 *	and a message will be left in the interp's result if the
 *	TCL_LEAVE_ERR_MSG flag is set. Note: the return value is only valid
 *	up until the next change to the variable; if you depend on the value
 *	lasting longer than that, then make yourself a private copy.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_GetVar2(interp, part1, part2, flags)
    Tcl_Interp *interp;		/* Command interpreter in which variable is
				 * to be looked up. */
    char *part1;		/* Name of an array (if part2 is non-NULL)
				 * or the name of a variable. */
    char *part2;		/* If non-NULL, gives the name of an element
				 * in the array part1. */
    int flags;			/* OR-ed combination of TCL_GLOBAL_ONLY,
				 * TCL_NAMESPACE_ONLY and TCL_LEAVE_ERR_MSG
                                 * bits. */
{
    Tcl_Obj *objPtr;

    objPtr = Tcl_GetVar2Ex(interp, part1, part2, flags);
    if (objPtr == NULL) {
	return NULL;
    }
    return TclGetString(objPtr);
}
/*
 *----------------------------------------------------------------------
 *
 * Tcl_ObjGetVar2 --
 *
 *	Return the value of a Tcl variable as a Tcl object, given a
 *	two-part name consisting of array name and element within array.
 *
 * Results:
 *	The return value points to the current object value of the variable
 *	given by part1Ptr and part2Ptr. If the specified variable doesn't
 *	exist, or if there is a clash in array usage, then NULL is returned
 *	and a message will be left in the interpreter's result if the
 *	TCL_LEAVE_ERR_MSG flag is set.
 *
 * Side effects:
 *	The ref count for the returned object is _not_ incremented to
 *	reflect the returned reference; if you want to keep a reference to
 *	the object you must increment its ref count yourself.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_ObjGetVar2(interp, part1Ptr, part2Ptr, flags)
    Tcl_Interp *interp;		/* Command interpreter in which variable is
				 * to be looked up. */
    register Tcl_Obj *part1Ptr;	/* Points to an object holding the name of
				 * an array (if part2 is non-NULL) or the
				 * name of a variable. */
    register Tcl_Obj *part2Ptr;	/* If non-null, points to an object holding
				 * the name of an element in the array
				 * part1Ptr. */
    int flags;			/* OR-ed combination of TCL_GLOBAL_ONLY,
				 * TCL_LEAVE_ERR_MSG, and
				 * TCL_PARSE_PART1 bits. */
{
    char *part1, *part2;

    part1 = Tcl_GetString(part1Ptr);
    if (part2Ptr != NULL) {
	part2 = Tcl_GetString(part2Ptr);
    } else {
	part2 = NULL;
    }
    
    return Tcl_GetVar2Ex(interp, part1, part2, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetVar2Ex --
 *
 *	Return the value of a Tcl variable as a Tcl object, given a
 *	two-part name consisting of array name and element within array.
 *
 * Results:
 *	The return value points to the current object value of the variable
 *	given by part1Ptr and part2Ptr. If the specified variable doesn't
 *	exist, or if there is a clash in array usage, then NULL is returned
 *	and a message will be left in the interpreter's result if the
 *	TCL_LEAVE_ERR_MSG flag is set.
 *
 * Side effects:
 *	The ref count for the returned object is _not_ incremented to
 *	reflect the returned reference; if you want to keep a reference to
 *	the object you must increment its ref count yourself.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_GetVar2Ex(interp, part1, part2, flags)
    Tcl_Interp *interp;		/* Command interpreter in which variable is
				 * to be looked up. */
    char *part1;		/* Name of an array (if part2 is non-NULL)
				 * or the name of a variable. */
    char *part2;		/* If non-NULL, gives the name of an element
				 * in the array part1. */
    int flags;			/* OR-ed combination of TCL_GLOBAL_ONLY,
				 * and TCL_LEAVE_ERR_MSG bits. */
{
    Interp *iPtr = (Interp *) interp;
    register Var *varPtr;
    Var *arrayPtr;
    char *msg;

    varPtr = TclLookupVar(interp, part1, part2, flags, "read",
            /*createPart1*/ 0, /*createPart2*/ 1, &arrayPtr);
    if (varPtr == NULL) {
	return NULL;
    }

    /*
     * Invoke any traces that have been set for the variable.
     */

    if ((varPtr->tracePtr != NULL)
	    || ((arrayPtr != NULL) && (arrayPtr->tracePtr != NULL))) {
	msg = CallTraces(iPtr, arrayPtr, varPtr, part1, part2,
		(flags & (TCL_NAMESPACE_ONLY|TCL_GLOBAL_ONLY)) | TCL_TRACE_READS);
	if (msg != NULL) {
	    if (flags & TCL_LEAVE_ERR_MSG) {
		VarErrMsg(interp, part1, part2, "read", msg);
	    }
	    goto errorReturn;
	}
    }

    /*
     * Return the element if it's an existing scalar variable.
     */
    
    if (TclIsVarScalar(varPtr) && !TclIsVarUndefined(varPtr)) {
	return varPtr->value.objPtr;
    }
    
    if (flags & TCL_LEAVE_ERR_MSG) {
	if (TclIsVarUndefined(varPtr) && (arrayPtr != NULL)
	        && !TclIsVarUndefined(arrayPtr)) {
	    msg = noSuchElement;
	} else if (TclIsVarArray(varPtr)) {
	    msg = isArray;
	} else {
	    msg = noSuchVar;
	}
	VarErrMsg(interp, part1, part2, "read", msg);
    }

    /*
     * An error. If the variable doesn't exist anymore and no-one's using
     * it, then free up the relevant structures and hash table entries.
     */

    errorReturn:
    if (TclIsVarUndefined(varPtr)) {
	CleanupVar(varPtr, arrayPtr);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetIndexedScalar --
 *
 *	Return the Tcl object value of a local scalar variable in the active
 *	procedure, given its index in the procedure's array of compiler
 *	allocated local variables.
 *
 * Results:
 *	The return value points to the current object value of the variable
 *	given by localIndex. If the specified variable doesn't exist, or
 *	there is a clash in array usage, or an error occurs while executing
 *	variable traces, then NULL is returned and a message will be left in
 *	the interpreter's result if leaveErrorMsg is 1.
 *
 * Side effects:
 *	The ref count for the returned object is _not_ incremented to
 *	reflect the returned reference; if you want to keep a reference to
 *	the object you must increment its ref count yourself.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclGetIndexedScalar(interp, localIndex, leaveErrorMsg)
    Tcl_Interp *interp;		/* Command interpreter in which variable is
				 * to be looked up. */
    register int localIndex;	/* Index of variable in procedure's array
				 * of local variables. */
    int leaveErrorMsg;		/* 1 if to leave an error message in
				 * interpreter's result on an error.
				 * Otherwise no error message is left. */
{
    Interp *iPtr = (Interp *) interp;
    CallFrame *varFramePtr = iPtr->varFramePtr;
				/* Points to the procedure call frame whose
				 * variables are currently in use. Same as
				 * the current procedure's frame, if any,
				 * unless an "uplevel" is executing. */
    Var *compiledLocals = varFramePtr->compiledLocals;
    register Var *varPtr;	/* Points to the variable's in-frame Var
				 * structure. */
    char *varName;		/* Name of the local variable. */
    char *msg;

#ifdef TCL_COMPILE_DEBUG
    int localCt = varFramePtr->procPtr->numCompiledLocals;

    if (compiledLocals == NULL) {
	fprintf(stderr, "\nTclGetIndexedScalar: can't get local %i in frame 0x%x, no compiled locals\n",
		localIndex, (unsigned int) varFramePtr);
	panic("TclGetIndexedScalar: no compiled locals in frame 0x%x",
		(unsigned int) varFramePtr);
    }
    if ((localIndex < 0) || (localIndex >= localCt)) {
	fprintf(stderr, "\nTclGetIndexedScalar: can't get local %i in frame 0x%x with %i locals\n",
		localIndex, (unsigned int) varFramePtr, localCt);
	panic("TclGetIndexedScalar: bad local index %i in frame 0x%x",
		localIndex, (unsigned int) varFramePtr);
    }
#endif /* TCL_COMPILE_DEBUG */
    
    varPtr = &(compiledLocals[localIndex]);
    varName = varPtr->name;

    /*
     * If varPtr is a link variable, we have a reference to some variable
     * that was created through an "upvar" or "global" command, or we have a
     * reference to a variable in an enclosing namespace. Traverse through
     * any links until we find the referenced variable.
     */
	
    while (TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }

    /*
     * Invoke any traces that have been set for the variable.
     */

    if (varPtr->tracePtr != NULL) {
	msg = CallTraces(iPtr, /*arrayPtr*/ NULL, varPtr, varName, NULL,
		TCL_TRACE_READS);
	if (msg != NULL) {
	    if (leaveErrorMsg) {
		VarErrMsg(interp, varName, NULL, "read", msg);
	    }
	    return NULL;
	}
    }

    /*
     * Make sure we're dealing with a scalar variable and not an array, and
     * that the variable exists (isn't undefined).
     */

    if (!TclIsVarScalar(varPtr) || TclIsVarUndefined(varPtr)) {
	if (leaveErrorMsg) {
	    if (TclIsVarArray(varPtr)) {
		msg = isArray;
	    } else {
		msg = noSuchVar;
	    }
	    VarErrMsg(interp, varName, NULL, "read", msg);

	}
	return NULL;
    }
    return varPtr->value.objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetElementOfIndexedArray --
 *
 *	Return the Tcl object value for an element in a local array
 *	variable. The element is named by the object elemPtr while the 
 *	array is specified by its index in the active procedure's array
 *	of compiler allocated local variables.
 *
 * Results:
 *	The return value points to the current object value of the
 *	element. If the specified array or element doesn't exist, or there
 *	is a clash in array usage, or an error occurs while executing
 *	variable traces, then NULL is returned and a message will be left in
 *	the interpreter's result if leaveErrorMsg is 1.
 *
 * Side effects:
 *	The ref count for the returned object is _not_ incremented to
 *	reflect the returned reference; if you want to keep a reference to
 *	the object you must increment its ref count yourself.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclGetElementOfIndexedArray(interp, localIndex, elemPtr, leaveErrorMsg)
    Tcl_Interp *interp;		/* Command interpreter in which variable is
				 * to be looked up. */
    int localIndex;		/* Index of array variable in procedure's
				 * array of local variables. */
    Tcl_Obj *elemPtr;		/* Points to an object holding the name of
				 * an element to get in the array. */
    int leaveErrorMsg;		/* 1 if to leave an error message in
				 * the interpreter's result on an error.
				 * Otherwise no error message is left. */
{
    Interp *iPtr = (Interp *) interp;
    CallFrame *varFramePtr = iPtr->varFramePtr;
				/* Points to the procedure call frame whose
				 * variables are currently in use. Same as
				 * the current procedure's frame, if any,
				 * unless an "uplevel" is executing. */
    Var *compiledLocals = varFramePtr->compiledLocals;
    Var *arrayPtr;		/* Points to the array's in-frame Var
				 * structure. */
    char *arrayName;		/* Name of the local array. */
    Tcl_HashEntry *hPtr;
    Var *varPtr = NULL;		/* Points to the element's Var structure
				 * that we return. Initialized to avoid
				 * compiler warning. */
    char *elem, *msg;
    int new;

#ifdef TCL_COMPILE_DEBUG
    Proc *procPtr = varFramePtr->procPtr;
    int localCt = procPtr->numCompiledLocals;

    if (compiledLocals == NULL) {
	fprintf(stderr, "\nTclGetElementOfIndexedArray: can't get element of local %i in frame 0x%x, no compiled locals\n",
		localIndex, (unsigned int) varFramePtr);
	panic("TclGetIndexedScalar: no compiled locals in frame 0x%x",
		(unsigned int) varFramePtr);
    }
    if ((localIndex < 0) || (localIndex >= localCt)) {
	fprintf(stderr, "\nTclGetIndexedScalar: can't get element of local %i in frame 0x%x with %i locals\n",
		localIndex, (unsigned int) varFramePtr, localCt);
	panic("TclGetElementOfIndexedArray: bad local index %i in frame 0x%x",
		localIndex, (unsigned int) varFramePtr);
    }
#endif /* TCL_COMPILE_DEBUG */

    elem = TclGetString(elemPtr);
    arrayPtr = &(compiledLocals[localIndex]);
    arrayName = arrayPtr->name;

    /*
     * If arrayPtr is a link variable, we have a reference to some variable
     * that was created through an "upvar" or "global" command, or we have a
     * reference to a variable in an enclosing namespace. Traverse through
     * any links until we find the referenced variable.
     */
	
    while (TclIsVarLink(arrayPtr)) {
	arrayPtr = arrayPtr->value.linkPtr;
    }

    /*
     * Make sure we're dealing with an array and that the array variable
     * exists (isn't undefined).
     */

    if (!TclIsVarArray(arrayPtr) || TclIsVarUndefined(arrayPtr)) {
	if (leaveErrorMsg) {
	    VarErrMsg(interp, arrayName, elem, "read", noSuchVar);
	}
	goto errorReturn;
    } 

    /*
     * Look up the element. Note that we must create the element (but leave
     * it marked undefined) if it does not already exist. This allows a
     * trace to create new array elements "on the fly" that did not exist
     * before. A trace is always passed a variable for the array element. If
     * the trace does not define the variable, it will be deleted below (at
     * errorReturn) and an error returned.
     */

    hPtr = Tcl_CreateHashEntry(arrayPtr->value.tablePtr, elem, &new);
    if (new) {
	if (arrayPtr->searchPtr != NULL) {
	    DeleteSearches(arrayPtr);
	}
	varPtr = NewVar();
	Tcl_SetHashValue(hPtr, varPtr);
	varPtr->hPtr = hPtr;
	varPtr->nsPtr = varFramePtr->nsPtr;
	TclSetVarArrayElement(varPtr);
    } else {
	varPtr = (Var *) Tcl_GetHashValue(hPtr);
    }

    /*
     * Invoke any traces that have been set for the element variable.
     */

    if ((varPtr->tracePtr != NULL)
            || ((arrayPtr != NULL) && (arrayPtr->tracePtr != NULL))) {
	msg = CallTraces(iPtr, arrayPtr, varPtr, arrayName, elem,
	        TCL_TRACE_READS);
	if (msg != NULL) {
	    if (leaveErrorMsg) {
		VarErrMsg(interp, arrayName, elem, "read", msg);
	    }
	    goto errorReturn;
	}
    }

    /*
     * Return the element if it's an existing scalar variable.
     */
    
    if (TclIsVarScalar(varPtr) && !TclIsVarUndefined(varPtr)) {
	return varPtr->value.objPtr;
    }
    
    if (leaveErrorMsg) {
	if (TclIsVarArray(varPtr)) {
	    msg = isArray;
	} else {
	    msg = noSuchVar;
	}
	VarErrMsg(interp, arrayName, elem, "read", msg);
    }

    /*
     * An error. If the variable doesn't exist anymore and no-one's using
     * it, then free up the relevant structures and hash table entries.
     */

    errorReturn:
    if ((varPtr != NULL) && TclIsVarUndefined(varPtr)) {
	CleanupVar(varPtr, NULL); /* the array is not in a hashtable */
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetObjCmd --
 *
 *	This procedure is invoked to process the "set" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result value.
 *
 * Side effects:
 *	A variable's value may be changed.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_SetObjCmd(dummy, interp, objc, objv)
    ClientData dummy;			/* Not used. */
    register Tcl_Interp *interp;	/* Current interpreter. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *const objv[];		/* Argument objects. */
{
    Tcl_Obj *varValueObj;

    if (objc == 2) {
	varValueObj = Tcl_ObjGetVar2(interp, objv[1], NULL, TCL_LEAVE_ERR_MSG);
	if (varValueObj == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, varValueObj);
	return TCL_OK;
    } else if (objc == 3) {

	varValueObj = Tcl_ObjSetVar2(interp, objv[1], NULL, objv[2],
		TCL_LEAVE_ERR_MSG);
	if (varValueObj == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, varValueObj);
	return TCL_OK;
    } else {
	Tcl_WrongNumArgs(interp, 1, objv, "varName ?newValue?");
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetVar --
 *
 *	Change the value of a variable.
 *
 * Results:
 *	Returns a pointer to the malloc'ed string which is the character
 *	representation of the variable's new value. The caller must not
 *	modify this string. If the write operation was disallowed then NULL
 *	is returned; if the TCL_LEAVE_ERR_MSG flag is set, then an
 *	explanatory message will be left in the interp's result. Note that the
 *	returned string may not be the same as newValue; this is because
 *	variable traces may modify the variable's value.
 *
 * Side effects:
 *	If varName is defined as a local or global variable in interp,
 *	its value is changed to newValue. If varName isn't currently
 *	defined, then a new global variable by that name is created.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_SetVar(interp, varName, newValue, flags)
    Tcl_Interp *interp;		/* Command interpreter in which varName is
				 * to be looked up. */
    char *varName;		/* Name of a variable in interp. */
    char *newValue;		/* New value for varName. */
    int flags;			/* Various flags that tell how to set value:
				 * any of TCL_GLOBAL_ONLY,
				 * TCL_NAMESPACE_ONLY, TCL_APPEND_VALUE,
				 * TCL_LIST_ELEMENT, TCL_LEAVE_ERR_MSG. */
{
    return Tcl_SetVar2(interp, varName, (char *) NULL, newValue, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetVar2 --
 *
 *      Given a two-part variable name, which may refer either to a
 *      scalar variable or an element of an array, change the value
 *      of the variable.  If the named scalar or array or element
 *      doesn't exist then create one.
 *
 * Results:
 *	Returns a pointer to the malloc'ed string which is the character
 *	representation of the variable's new value. The caller must not
 *	modify this string. If the write operation was disallowed because an
 *	array was expected but not found (or vice versa), then NULL is
 *	returned; if the TCL_LEAVE_ERR_MSG flag is set, then an explanatory
 *	message will be left in the interp's result. Note that the returned
 *	string may not be the same as newValue; this is because variable
 *	traces may modify the variable's value.
 *
 * Side effects:
 *      The value of the given variable is set. If either the array
 *      or the entry didn't exist then a new one is created.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_SetVar2(interp, part1, part2, newValue, flags)
    Tcl_Interp *interp;         /* Command interpreter in which variable is
                                 * to be looked up. */
    char *part1;                /* If part2 is NULL, this is name of scalar
                                 * variable. Otherwise it is the name of
                                 * an array. */
    char *part2;                /* Name of an element within an array, or
				 * NULL. */
    char *newValue;             /* New value for variable. */
    int flags;                  /* Various flags that tell how to set value:
				 * any of TCL_GLOBAL_ONLY,
				 * TCL_NAMESPACE_ONLY, TCL_APPEND_VALUE,
				 * TCL_LIST_ELEMENT, or TCL_LEAVE_ERR_MSG */
{
    register Tcl_Obj *valuePtr;
    Tcl_Obj *varValuePtr;

    /*
     * Create an object holding the variable's new value and use
     * Tcl_SetVar2Ex to actually set the variable.
     */

    valuePtr = Tcl_NewStringObj(newValue, -1);
    Tcl_IncrRefCount(valuePtr);

    varValuePtr = Tcl_SetVar2Ex(interp, part1, part2, valuePtr, flags);
    Tcl_DecrRefCount(valuePtr); /* done with the object */
    
    if (varValuePtr == NULL) {
	return NULL;
    }
    return TclGetString(varValuePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ObjSetVar2 --
 *
 *	This function is the same as Tcl_SetVar2Ex below, except the
 *	variable names are passed in Tcl object instead of strings.
 *
 * Results:
 *	Returns a pointer to the Tcl_Obj holding the new value of the
 *	variable. If the write operation was disallowed because an array was
 *	expected but not found (or vice versa), then NULL is returned; if
 *	the TCL_LEAVE_ERR_MSG flag is set, then an explanatory message will
 *	be left in the interpreter's result. Note that the returned object
 *	may not be the same one referenced by newValuePtr; this is because
 *	variable traces may modify the variable's value.
 *
 * Side effects:
 *	The value of the given variable is set. If either the array or the
 *	entry didn't exist then a new variable is created.

 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_ObjSetVar2(interp, part1Ptr, part2Ptr, newValuePtr, flags)
    Tcl_Interp *interp;		/* Command interpreter in which variable is
				 * to be found. */
    register Tcl_Obj *part1Ptr;	/* Points to an object holding the name of
				 * an array (if part2 is non-NULL) or the
				 * name of a variable. */
    register Tcl_Obj *part2Ptr;	/* If non-null, points to an object holding
				 * the name of an element in the array
				 * part1Ptr. */
    Tcl_Obj *newValuePtr;	/* New value for variable. */
    int flags;			/* Various flags that tell how to set value:
				 * any of TCL_GLOBAL_ONLY,
				 * TCL_NAMESPACE_ONLY, TCL_APPEND_VALUE,
				 * TCL_LIST_ELEMENT, TCL_LEAVE_ERR_MSG, or
				 * TCL_PARSE_PART1. */
{
    char *part1, *part2;

    part1 = Tcl_GetString(part1Ptr);
    if (part2Ptr != NULL) {
	part2 = Tcl_GetString(part2Ptr);
    } else {
	part2 = NULL;
    }
    
    return Tcl_SetVar2Ex(interp, part1, part2, newValuePtr, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetVar2Ex --
 *
 *	Given a two-part variable name, which may refer either to a scalar
 *	variable or an element of an array, change the value of the variable
 *	to a new Tcl object value. If the named scalar or array or element
 *	doesn't exist then create one.
 *
 * Results:
 *	Returns a pointer to the Tcl_Obj holding the new value of the
 *	variable. If the write operation was disallowed because an array was
 *	expected but not found (or vice versa), then NULL is returned; if
 *	the TCL_LEAVE_ERR_MSG flag is set, then an explanatory message will
 *	be left in the interpreter's result. Note that the returned object
 *	may not be the same one referenced by newValuePtr; this is because
 *	variable traces may modify the variable's value.
 *
 * Side effects:
 *	The value of the given variable is set. If either the array or the
 *	entry didn't exist then a new variable is created.
 *
 *	The reference count is decremented for any old value of the variable
 *	and incremented for its new value. If the new value for the variable
 *	is not the same one referenced by newValuePtr (perhaps as a result
 *	of a variable trace), then newValuePtr's ref count is left unchanged
 *	by Tcl_SetVar2Ex. newValuePtr's ref count is also left unchanged if
 *	we are appending it as a string value: that is, if "flags" includes
 *	TCL_APPEND_VALUE but not TCL_LIST_ELEMENT.
 *
 *	The reference count for the returned object is _not_ incremented: if
 *	you want to keep a reference to the object you must increment its
 *	ref count yourself.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_SetVar2Ex(interp, part1, part2, newValuePtr, flags)
    Tcl_Interp *interp;		/* Command interpreter in which variable is
				 * to be found. */
    char *part1;		/* Name of an array (if part2 is non-NULL)
				 * or the name of a variable. */
    char *part2;		/* If non-NULL, gives the name of an element
				 * in the array part1. */
    Tcl_Obj *newValuePtr;	/* New value for variable. */
    int flags;			/* Various flags that tell how to set value:
				 * any of TCL_GLOBAL_ONLY,
				 * TCL_NAMESPACE_ONLY, TCL_APPEND_VALUE,
				 * TCL_LIST_ELEMENT or TCL_LEAVE_ERR_MSG. */
{
    Interp *iPtr = (Interp *) interp;
    register Var *varPtr;
    Var *arrayPtr;
    Tcl_Obj *oldValuePtr;
    Tcl_Obj *resultPtr = NULL;
    char *bytes;
    int length, result;

    varPtr = TclLookupVar(interp, part1, part2, flags, "set",
	    /*createPart1*/ 1, /*createPart2*/ 1, &arrayPtr);
    if (varPtr == NULL) {
	return NULL;
    }

    /*
     * If the variable is in a hashtable and its hPtr field is NULL, then we
     * may have an upvar to an array element where the array was deleted
     * or an upvar to a namespace variable whose namespace was deleted.
     * Generate an error (allowing the variable to be reset would screw up
     * our storage allocation and is meaningless anyway).
     */

    if ((varPtr->flags & VAR_IN_HASHTABLE) && (varPtr->hPtr == NULL)) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    if (TclIsVarArrayElement(varPtr)) {
		VarErrMsg(interp, part1, part2, "set", danglingElement);
	    } else {
		VarErrMsg(interp, part1, part2, "set", danglingVar);
	    }
	}
	return NULL;
    }

    /*
     * It's an error to try to set an array variable itself.
     */

    if (TclIsVarArray(varPtr) && !TclIsVarUndefined(varPtr)) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    VarErrMsg(interp, part1, part2, "set", isArray);
	}
	return NULL;
    }

    /*
     * At this point, if we were appending, we used to call read traces: we
     * treated append as a read-modify-write. However, it seemed unlikely to
     * us that a real program would be interested in such reads being done
     * during a set operation.
     */

    /*
     * Set the variable's new value. If appending, append the new value to
     * the variable, either as a list element or as a string. Also, if
     * appending, then if the variable's old value is unshared we can modify
     * it directly, otherwise we must create a new copy to modify: this is
     * "copy on write".
     */

    oldValuePtr = varPtr->value.objPtr;
    if (flags & TCL_APPEND_VALUE) {
	if (TclIsVarUndefined(varPtr) && (oldValuePtr != NULL)) {
	    Tcl_DecrRefCount(oldValuePtr);     /* discard old value */
	    varPtr->value.objPtr = NULL;
	    oldValuePtr = NULL;
	}
	if (flags & TCL_LIST_ELEMENT) {	       /* append list element */
	    if (oldValuePtr == NULL) {
		TclNewObj(oldValuePtr);
		varPtr->value.objPtr = oldValuePtr;
		Tcl_IncrRefCount(oldValuePtr); /* since var is referenced */
	    } else if (Tcl_IsShared(oldValuePtr)) {
		varPtr->value.objPtr = Tcl_DuplicateObj(oldValuePtr);
		Tcl_DecrRefCount(oldValuePtr);
		oldValuePtr = varPtr->value.objPtr;
		Tcl_IncrRefCount(oldValuePtr); /* since var is referenced */
	    }
	    result = Tcl_ListObjAppendElement(interp, oldValuePtr,
		    newValuePtr);
	    if (result != TCL_OK) {
		return NULL;
	    }
	} else {		               /* append string */
	    /*
	     * We append newValuePtr's bytes but don't change its ref count.
	     */

	    bytes = Tcl_GetStringFromObj(newValuePtr, &length);
	    if (oldValuePtr == NULL) {
		varPtr->value.objPtr = Tcl_NewStringObj(bytes, length);
		Tcl_IncrRefCount(varPtr->value.objPtr);
	    } else {
		if (Tcl_IsShared(oldValuePtr)) {   /* append to copy */
		    varPtr->value.objPtr = Tcl_DuplicateObj(oldValuePtr);
		    TclDecrRefCount(oldValuePtr);
		    oldValuePtr = varPtr->value.objPtr;
		    Tcl_IncrRefCount(oldValuePtr); /* since var is ref */
		}
		Tcl_AppendObjToObj(oldValuePtr, newValuePtr);
	    }
	}
    } else {
	if (flags & TCL_LIST_ELEMENT) {	       /* set var to list element */
	    int neededBytes, listFlags;

	    /*
	     * We set the variable to the result of converting newValuePtr's
	     * string rep to a list element. We do not change newValuePtr's
	     * ref count.
	     */

	    if (oldValuePtr != NULL) {
		Tcl_DecrRefCount(oldValuePtr); /* discard old value */
	    }
	    bytes = Tcl_GetStringFromObj(newValuePtr, &length);
	    neededBytes = Tcl_ScanElement(bytes, &listFlags);
	    oldValuePtr = Tcl_NewObj();
	    oldValuePtr->bytes = (char *)
		ckalloc((unsigned) (neededBytes + 1));
	    oldValuePtr->length = Tcl_ConvertElement(bytes,
		    oldValuePtr->bytes, listFlags);
	    varPtr->value.objPtr = oldValuePtr;
	    Tcl_IncrRefCount(varPtr->value.objPtr);
	} else if (newValuePtr != oldValuePtr) {
	    varPtr->value.objPtr = newValuePtr;
	    Tcl_IncrRefCount(newValuePtr);      /* var is another ref */
	    if (oldValuePtr != NULL) {
		TclDecrRefCount(oldValuePtr);   /* discard old value */
	    }
	}
    }
    TclSetVarScalar(varPtr);
    TclClearVarUndefined(varPtr);
    if (arrayPtr != NULL) {
	TclClearVarUndefined(arrayPtr);
    }

    /*
     * Invoke any write traces for the variable.
     */

    if ((varPtr->tracePtr != NULL)
	    || ((arrayPtr != NULL) && (arrayPtr->tracePtr != NULL))) {
	char *msg = CallTraces(iPtr, arrayPtr, varPtr, part1, part2,
	        (flags & (TCL_GLOBAL_ONLY|TCL_NAMESPACE_ONLY)) | TCL_TRACE_WRITES);
	if (msg != NULL) {
	    if (flags & TCL_LEAVE_ERR_MSG) {
		VarErrMsg(interp, part1, part2, "set", msg);
	    }
	    goto cleanup;
	}
    }

    /*
     * Return the variable's value unless the variable was changed in some
     * gross way by a trace (e.g. it was unset and then recreated as an
     * array). 
     */

    if (TclIsVarScalar(varPtr) && !TclIsVarUndefined(varPtr)) {
	return varPtr->value.objPtr;
    }

    /*
     * A trace changed the value in some gross way. Return an empty string
     * object.
     */
    
    resultPtr = iPtr->emptyObjPtr;

    /*
     * If the variable doesn't exist anymore and no-one's using it, then
     * free up the relevant structures and hash table entries.
     */

    cleanup:
    if (TclIsVarUndefined(varPtr)) {
	CleanupVar(varPtr, arrayPtr);
    }
    return resultPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclSetIndexedScalar --
 *
 *	Change the Tcl object value of a local scalar variable in the active
 *	procedure, given its compile-time allocated index in the procedure's
 *	array of local variables.
 *
 * Results:
 *	Returns a pointer to the Tcl_Obj holding the new value of the
 *	variable given by localIndex. If the specified variable doesn't
 *	exist, or there is a clash in array usage, or an error occurs while
 *	executing variable traces, then NULL is returned and a message will
 *	be left in the interpreter's result if leaveErrorMsg is 1. Note
 *	that the returned object may not be the same one referenced by
 *	newValuePtr; this is because variable traces may modify the
 *	variable's value.
 *
 * Side effects:
 *	The value of the given variable is set. The reference count is
 *	decremented for any old value of the variable and incremented for
 *	its new value. If as a result of a variable trace the new value for
 *	the variable is not the same one referenced by newValuePtr, then
 *	newValuePtr's ref count is left unchanged. The ref count for the
 *	returned object is _not_ incremented to reflect the returned
 *	reference; if you want to keep a reference to the object you must
 *	increment its ref count yourself. This procedure does not create
 *	new variables, but only sets those recognized at compile time.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclSetIndexedScalar(interp, localIndex, newValuePtr, leaveErrorMsg)
    Tcl_Interp *interp;		/* Command interpreter in which variable is
				 * to be found. */
    int localIndex;		/* Index of variable in procedure's array
				 * of local variables. */
    Tcl_Obj *newValuePtr;	/* New value for variable. */
    int leaveErrorMsg;		/* 1 if to leave an error message in
				 * the interpreter's result on an error.
				 * Otherwise no error message is left. */
{
    Interp *iPtr = (Interp *) interp;
    CallFrame *varFramePtr = iPtr->varFramePtr;
				/* Points to the procedure call frame whose
				 * variables are currently in use. Same as
				 * the current procedure's frame, if any,
				 * unless an "uplevel" is executing. */
    Var *compiledLocals = varFramePtr->compiledLocals;
    register Var *varPtr;	/* Points to the variable's in-frame Var
				 * structure. */
    char *varName;		/* Name of the local variable. */
    Tcl_Obj *oldValuePtr;
    Tcl_Obj *resultPtr = NULL;

#ifdef TCL_COMPILE_DEBUG
    Proc *procPtr = varFramePtr->procPtr;
    int localCt = procPtr->numCompiledLocals;

    if (compiledLocals == NULL) {
	fprintf(stderr, "\nTclSetIndexedScalar: can't set local %i in frame 0x%x, no compiled locals\n",
		localIndex, (unsigned int) varFramePtr);
	panic("TclSetIndexedScalar: no compiled locals in frame 0x%x",
		(unsigned int) varFramePtr);
    }
    if ((localIndex < 0) || (localIndex >= localCt)) {
	fprintf(stderr, "\nTclSetIndexedScalar: can't set local %i in frame 0x%x with %i locals\n",
		localIndex, (unsigned int) varFramePtr, localCt);
	panic("TclSetIndexedScalar: bad local index %i in frame 0x%x",
		localIndex, (unsigned int) varFramePtr);
    }
#endif /* TCL_COMPILE_DEBUG */
    
    varPtr = &(compiledLocals[localIndex]);
    varName = varPtr->name;

    /*
     * If varPtr is a link variable, we have a reference to some variable
     * that was created through an "upvar" or "global" command, or we have a
     * reference to a variable in an enclosing namespace. Traverse through
     * any links until we find the referenced variable.
     */
	
    while (TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }

    /*
     * If the variable is in a hashtable and its hPtr field is NULL, then we
     * may have an upvar to an array element where the array was deleted
     * or an upvar to a namespace variable whose namespace was deleted.
     * Generate an error (allowing the variable to be reset would screw up
     * our storage allocation and is meaningless anyway).
     */

    if ((varPtr->flags & VAR_IN_HASHTABLE) && (varPtr->hPtr == NULL)) {
	if (leaveErrorMsg) {
	    if (TclIsVarArrayElement(varPtr)) {
		VarErrMsg(interp, varName, NULL, "set", danglingElement);
	    } else {
		VarErrMsg(interp, varName, NULL, "set", danglingVar);
	    }
	}
	return NULL;
    }

    /*
     * It's an error to try to set an array variable itself.
     */

    if (TclIsVarArray(varPtr) && !TclIsVarUndefined(varPtr)) {
	if (leaveErrorMsg) {
	    VarErrMsg(interp, varName, NULL, "set", isArray);
	}
	return NULL;
    }

    /*
     * Set the variable's new value and discard its old value. We don't
     * append with this "set" procedure so the old value isn't needed.
     */

    oldValuePtr = varPtr->value.objPtr;
    if (newValuePtr != oldValuePtr) {        /* set new value */
	varPtr->value.objPtr = newValuePtr;
	Tcl_IncrRefCount(newValuePtr);       /* var is another ref to obj */
	if (oldValuePtr != NULL) {
	    TclDecrRefCount(oldValuePtr);    /* discard old value */
	}
    }
    TclSetVarScalar(varPtr);
    TclClearVarUndefined(varPtr);

    /*
     * Invoke any write traces for the variable.
     */

    if (varPtr->tracePtr != NULL) {
	char *msg = CallTraces(iPtr, /*arrayPtr*/ NULL, varPtr,
	        varName, (char *) NULL, TCL_TRACE_WRITES);
	if (msg != NULL) {
	    if (leaveErrorMsg) {
		VarErrMsg(interp, varName, NULL, "set", msg);
	    }
	    goto cleanup;
	}
    }

    /*
     * Return the variable's value unless the variable was changed in some
     * gross way by a trace (e.g. it was unset and then recreated as an
     * array). If it was changed is a gross way, just return an empty string
     * object.
     */

    if (TclIsVarScalar(varPtr) && !TclIsVarUndefined(varPtr)) {
	return varPtr->value.objPtr;
    }
    
    resultPtr = Tcl_NewObj();

    /*
     * If the variable doesn't exist anymore and no-one's using it, then
     * free up the relevant structures and hash table entries.
     */

    cleanup:
    if (TclIsVarUndefined(varPtr)) {
	CleanupVar(varPtr, NULL);
    }
    return resultPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclSetElementOfIndexedArray --
 *
 *	Change the Tcl object value of an element in a local array
 *	variable. The element is named by the object elemPtr while the array
 *	is specified by its index in the active procedure's array of
 *	compiler allocated local variables.
 *
 * Results:
 *	Returns a pointer to the Tcl_Obj holding the new value of the
 *	element. If the specified array or element doesn't exist, or there
 *	is a clash in array usage, or an error occurs while executing
 *	variable traces, then NULL is returned and a message will be left in
 *	the interpreter's result if leaveErrorMsg is 1. Note that the
 *	returned object may not be the same one referenced by newValuePtr;
 *	this is because variable traces may modify the variable's value.
 *
 * Side effects:
 *	The value of the given array element is set. The reference count is
 *	decremented for any old value of the element and incremented for its
 *	new value. If as a result of a variable trace the new value for the
 *	element is not the same one referenced by newValuePtr, then
 *	newValuePtr's ref count is left unchanged. The ref count for the
 *	returned object is _not_ incremented to reflect the returned
 *	reference; if you want to keep a reference to the object you must
 *	increment its ref count yourself. This procedure will not create new
 *	array variables, but only sets elements of those arrays recognized
 *	at compile time. However, if the entry doesn't exist then a new
 *	variable is created.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclSetElementOfIndexedArray(interp, localIndex, elemPtr, newValuePtr,
        leaveErrorMsg)
    Tcl_Interp *interp;		/* Command interpreter in which the array is
				 * to be found. */
    int localIndex;		/* Index of array variable in procedure's
				 * array of local variables. */
    Tcl_Obj *elemPtr;		/* Points to an object holding the name of
				 * an element to set in the array. */
    Tcl_Obj *newValuePtr;	/* New value for variable. */
    int leaveErrorMsg;		/* 1 if to leave an error message in
				 * the interpreter's result on an error.
				 * Otherwise no error message is left. */
{
    Interp *iPtr = (Interp *) interp;
    CallFrame *varFramePtr = iPtr->varFramePtr;
				/* Points to the procedure call frame whose
				 * variables are currently in use. Same as
				 * the current procedure's frame, if any,
				 * unless an "uplevel" is executing. */
    Var *compiledLocals = varFramePtr->compiledLocals;
    Var *arrayPtr;		/* Points to the array's in-frame Var
				 * structure. */
    char *arrayName;		/* Name of the local array. */
    char *elem;
    Tcl_HashEntry *hPtr;
    Var *varPtr = NULL;		/* Points to the element's Var structure
				 * that we return. */
    Tcl_Obj *resultPtr = NULL;
    Tcl_Obj *oldValuePtr;
    int new;
    
#ifdef TCL_COMPILE_DEBUG
    Proc *procPtr = varFramePtr->procPtr;
    int localCt = procPtr->numCompiledLocals;

    if (compiledLocals == NULL) {
	fprintf(stderr, "\nTclSetElementOfIndexedArray: can't set element of local %i in frame 0x%x, no compiled locals\n",
		localIndex, (unsigned int) varFramePtr);
	panic("TclSetIndexedScalar: no compiled locals in frame 0x%x",
		(unsigned int) varFramePtr);
    }
    if ((localIndex < 0) || (localIndex >= localCt)) {
	fprintf(stderr, "\nTclSetIndexedScalar: can't set elememt of local %i in frame 0x%x with %i locals\n",
		localIndex, (unsigned int) varFramePtr, localCt);
	panic("TclSetElementOfIndexedArray: bad local index %i in frame 0x%x",
		localIndex, (unsigned int) varFramePtr);
    }
#endif /* TCL_COMPILE_DEBUG */

    elem = TclGetString(elemPtr);
    arrayPtr = &(compiledLocals[localIndex]);
    arrayName = arrayPtr->name;

    /*
     * If arrayPtr is a link variable, we have a reference to some variable
     * that was created through an "upvar" or "global" command, or we have a
     * reference to a variable in an enclosing namespace. Traverse through
     * any links until we find the referenced variable.
     */
	
    while (TclIsVarLink(arrayPtr)) {
	arrayPtr = arrayPtr->value.linkPtr;
    }

    /*
     * If the variable is in a hashtable and its hPtr field is NULL, then we
     * may have an upvar to an array element where the array was deleted
     * or an upvar to a namespace variable whose namespace was deleted.
     * Generate an error (allowing the variable to be reset would screw up
     * our storage allocation and is meaningless anyway).
     */

    if ((arrayPtr->flags & VAR_IN_HASHTABLE) && (arrayPtr->hPtr == NULL)) {
	if (leaveErrorMsg) {
	    if (TclIsVarArrayElement(arrayPtr)) {
		VarErrMsg(interp, arrayName, elem, "set", danglingElement);
	    } else {
		VarErrMsg(interp, arrayName, elem, "set", danglingVar);
	    }
	}
	goto errorReturn;
    }

    /*
     * Make sure we're dealing with an array.
     */

    if (TclIsVarUndefined(arrayPtr) && !TclIsVarArrayElement(arrayPtr)) {
	TclSetVarArray(arrayPtr);
	arrayPtr->value.tablePtr =
	    (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(arrayPtr->value.tablePtr, TCL_STRING_KEYS);
	TclClearVarUndefined(arrayPtr);
    } else if (!TclIsVarArray(arrayPtr)) {
	if (leaveErrorMsg) {
	    VarErrMsg(interp, arrayName, elem, "set", needArray);
	}
	goto errorReturn;
    } 

    /*
     * Look up the element.
     */

    hPtr = Tcl_CreateHashEntry(arrayPtr->value.tablePtr, elem, &new);
    if (new) {
	if (arrayPtr->searchPtr != NULL) {
	    DeleteSearches(arrayPtr);
	}
	varPtr = NewVar();
	Tcl_SetHashValue(hPtr, varPtr);
	varPtr->hPtr = hPtr;
        varPtr->nsPtr = varFramePtr->nsPtr;
	TclSetVarArrayElement(varPtr);
    }
    varPtr = (Var *) Tcl_GetHashValue(hPtr);

    /*
     * It's an error to try to set an array variable itself.
     */

    if (TclIsVarArray(varPtr)) {
	if (leaveErrorMsg) {
	    VarErrMsg(interp, arrayName, elem, "set", isArray);
	}
	goto errorReturn;
    }

    /*
     * Set the variable's new value and discard the old one. We don't
     * append with this "set" procedure so the old value isn't needed.
     */

    oldValuePtr = varPtr->value.objPtr;
    if (newValuePtr != oldValuePtr) {	     /* set new value */
	varPtr->value.objPtr = newValuePtr;
	Tcl_IncrRefCount(newValuePtr);       /* var is another ref to obj */
	if (oldValuePtr != NULL) {
	    TclDecrRefCount(oldValuePtr);    /* discard old value */
	}
    }
    TclSetVarScalar(varPtr);
    TclClearVarUndefined(varPtr);

    /*
     * Invoke any write traces for the element variable.
     */

    if ((varPtr->tracePtr != NULL)
	    || ((arrayPtr != NULL) && (arrayPtr->tracePtr != NULL))) {
	char *msg = CallTraces(iPtr, arrayPtr, varPtr, arrayName, elem,
		TCL_TRACE_WRITES);
	if (msg != NULL) {
	    if (leaveErrorMsg) {
		VarErrMsg(interp, arrayName, elem, "set", msg);
	    }
	    goto errorReturn;
	}
    }

    /*
     * Return the element's value unless it was changed in some gross way by
     * a trace (e.g. it was unset and then recreated as an array). If it was
     * changed is a gross way, just return an empty string object.
     */

    if (TclIsVarScalar(varPtr) && !TclIsVarUndefined(varPtr)) {
	return varPtr->value.objPtr;
    }
    
    resultPtr = Tcl_NewObj();

    /*
     * An error. If the variable doesn't exist anymore and no-one's using
     * it, then free up the relevant structures and hash table entries.
     */

    errorReturn:
    if (varPtr != NULL) {
	if (TclIsVarUndefined(varPtr)) {
	    CleanupVar(varPtr, NULL); /* note: array isn't in hashtable */
	}
    }
    return resultPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclIncrVar2 --
 *
 *	Given a two-part variable name, which may refer either to a scalar
 *	variable or an element of an array, increment the Tcl object value
 *	of the variable by a specified amount.
 *
 * Results:
 *	Returns a pointer to the Tcl_Obj holding the new value of the
 *	variable. If the specified variable doesn't exist, or there is a
 *	clash in array usage, or an error occurs while executing variable
 *	traces, then NULL is returned and a message will be left in
 *	the interpreter's result.
 *
 * Side effects:
 *	The value of the given variable is incremented by the specified
 *	amount. If either the array or the entry didn't exist then a new
 *	variable is created. The ref count for the returned object is _not_
 *	incremented to reflect the returned reference; if you want to keep a
 *	reference to the object you must increment its ref count yourself.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclIncrVar2(interp, part1Ptr, part2Ptr, incrAmount, flags)
    Tcl_Interp *interp;		/* Command interpreter in which variable is
				 * to be found. */
    Tcl_Obj *part1Ptr;		/* Points to an object holding the name of
				 * an array (if part2 is non-NULL) or the
				 * name of a variable. */
    Tcl_Obj *part2Ptr;		/* If non-null, points to an object holding
				 * the name of an element in the array
				 * part1Ptr. */
    long incrAmount;		/* Amount to be added to variable. */
    int flags;                  /* Various flags that tell how to incr value:
				 * any of TCL_GLOBAL_ONLY,
				 * TCL_NAMESPACE_ONLY, TCL_APPEND_VALUE,
				 * TCL_LIST_ELEMENT, TCL_LEAVE_ERR_MSG. */
{
    register Tcl_Obj *varValuePtr;
    Tcl_Obj *resultPtr;
    int createdNewObj;		/* Set 1 if var's value object is shared
				 * so we must increment a copy (i.e. copy
				 * on write). */
    long i;
    int result;

    varValuePtr = Tcl_ObjGetVar2(interp, part1Ptr, part2Ptr, flags);
    if (varValuePtr == NULL) {
	Tcl_AddObjErrorInfo(interp,
		"\n    (reading value of variable to increment)", -1);
	return NULL;
    }

    /*
     * Increment the variable's value. If the object is unshared we can
     * modify it directly, otherwise we must create a new copy to modify:
     * this is "copy on write". Then free the variable's old string
     * representation, if any, since it will no longer be valid.
     */

    createdNewObj = 0;
    if (Tcl_IsShared(varValuePtr)) {
	varValuePtr = Tcl_DuplicateObj(varValuePtr);
	createdNewObj = 1;
    }
    result = Tcl_GetLongFromObj(interp, varValuePtr, &i);
    if (result != TCL_OK) {
	if (createdNewObj) {
	    Tcl_DecrRefCount(varValuePtr); /* free unneeded copy */
	}
	return NULL;
    }
    Tcl_SetLongObj(varValuePtr, (i + incrAmount));

    /*
     * Store the variable's new value and run any write traces.
     */
    
    resultPtr = Tcl_ObjSetVar2(interp, part1Ptr, part2Ptr, varValuePtr, flags);
    if (resultPtr == NULL) {
	return NULL;
    }
    return resultPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclIncrIndexedScalar --
 *
 *	Increments the Tcl object value of a local scalar variable in the
 *	active procedure, given its compile-time allocated index in the
 *	procedure's array of local variables.
 *
 * Results:
 *	Returns a pointer to the Tcl_Obj holding the new value of the
 *	variable given by localIndex. If the specified variable doesn't
 *	exist, or there is a clash in array usage, or an error occurs while
 *	executing variable traces, then NULL is returned and a message will
 *	be left in the interpreter's result. 
 *
 * Side effects:
 *	The value of the given variable is incremented by the specified
 *	amount. The ref count for the returned object is _not_ incremented
 *	to reflect the returned reference; if you want to keep a reference
 *	to the object you must increment its ref count yourself.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclIncrIndexedScalar(interp, localIndex, incrAmount)
    Tcl_Interp *interp;		/* Command interpreter in which variable is
				 * to be found. */
    int localIndex;		/* Index of variable in procedure's array
				 * of local variables. */
    long incrAmount;		/* Amount to be added to variable. */
{
    register Tcl_Obj *varValuePtr;
    Tcl_Obj *resultPtr;
    int createdNewObj;		/* Set 1 if var's value object is shared
				 * so we must increment a copy (i.e. copy
				 * on write). */
    long i;
    int result;

    varValuePtr = TclGetIndexedScalar(interp, localIndex,
	    /*leaveErrorMsg*/ 1);
    if (varValuePtr == NULL) {
	Tcl_AddObjErrorInfo(interp,
		"\n    (reading value of variable to increment)", -1);
	return NULL;
    }

    /*
     * Reach into the object's representation to extract and increment the
     * variable's value. If the object is unshared we can modify it
     * directly, otherwise we must create a new copy to modify: this is
     * "copy on write". Then free the variable's old string representation,
     * if any, since it will no longer be valid.
     */

    createdNewObj = 0;
    if (Tcl_IsShared(varValuePtr)) {
	createdNewObj = 1;
	varValuePtr = Tcl_DuplicateObj(varValuePtr);
    }
    result = Tcl_GetLongFromObj(interp, varValuePtr, &i);
    if (result != TCL_OK) {
	if (createdNewObj) {
	    Tcl_DecrRefCount(varValuePtr); /* free unneeded copy */
	}
	return NULL;
    }
    Tcl_SetLongObj(varValuePtr, (i + incrAmount));

    /*
     * Store the variable's new value and run any write traces.
     */
    
    resultPtr = TclSetIndexedScalar(interp, localIndex, varValuePtr,
	    /*leaveErrorMsg*/ 1);
    if (resultPtr == NULL) {
	return NULL;
    }
    return resultPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclIncrElementOfIndexedArray --
 *
 *	Increments the Tcl object value of an element in a local array
 *	variable. The element is named by the object elemPtr while the array
 *	is specified by its index in the active procedure's array of
 *	compiler allocated local variables.
 *
 * Results:
 *	Returns a pointer to the Tcl_Obj holding the new value of the
 *	element. If the specified array or element doesn't exist, or there
 *	is a clash in array usage, or an error occurs while executing
 *	variable traces, then NULL is returned and a message will be left in
 *	the interpreter's result.
 *
 * Side effects:
 *	The value of the given array element is incremented by the specified
 *	amount. The ref count for the returned object is _not_ incremented
 *	to reflect the returned reference; if you want to keep a reference
 *	to the object you must increment its ref count yourself. If the
 *	entry doesn't exist then a new variable is created.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclIncrElementOfIndexedArray(interp, localIndex, elemPtr, incrAmount)
    Tcl_Interp *interp;		/* Command interpreter in which the array is
				 * to be found. */
    int localIndex;		/* Index of array variable in procedure's
				 * array of local variables. */
    Tcl_Obj *elemPtr;		/* Points to an object holding the name of
				 * an element to increment in the array. */
    long incrAmount;		/* Amount to be added to variable. */
{
    register Tcl_Obj *varValuePtr;
    Tcl_Obj *resultPtr;
    int createdNewObj;		/* Set 1 if var's value object is shared
				 * so we must increment a copy (i.e. copy
				 * on write). */
    long i;
    int result;

    varValuePtr = TclGetElementOfIndexedArray(interp, localIndex, elemPtr,
	    /*leaveErrorMsg*/ 1);
    if (varValuePtr == NULL) {
	Tcl_AddObjErrorInfo(interp,
		"\n    (reading value of variable to increment)", -1);
	return NULL;
    }

    /*
     * Reach into the object's representation to extract and increment the
     * variable's value. If the object is unshared we can modify it
     * directly, otherwise we must create a new copy to modify: this is
     * "copy on write". Then free the variable's old string representation,
     * if any, since it will no longer be valid.
     */

    createdNewObj = 0;
    if (Tcl_IsShared(varValuePtr)) {
	createdNewObj = 1;
	varValuePtr = Tcl_DuplicateObj(varValuePtr);
    }
    result = Tcl_GetLongFromObj(interp, varValuePtr, &i);
    if (result != TCL_OK) {
	if (createdNewObj) {
	    Tcl_DecrRefCount(varValuePtr); /* free unneeded copy */
	}
	return NULL;
    }
    Tcl_SetLongObj(varValuePtr, (i + incrAmount));
    
    /*
     * Store the variable's new value and run any write traces.
     */
    
    resultPtr = TclSetElementOfIndexedArray(interp, localIndex, elemPtr,
	    varValuePtr,
	    /*leaveErrorMsg*/ 1);
    if (resultPtr == NULL) {
	return NULL;
    }
    return resultPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UnsetVar --
 *
 *	Delete a variable, so that it may not be accessed anymore.
 *
 * Results:
 *	Returns TCL_OK if the variable was successfully deleted, TCL_ERROR
 *	if the variable can't be unset.  In the event of an error,
 *	if the TCL_LEAVE_ERR_MSG flag is set then an error message
 *	is left in the interp's result.
 *
 * Side effects:
 *	If varName is defined as a local or global variable in interp,
 *	it is deleted.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UnsetVar(interp, varName, flags)
    Tcl_Interp *interp;		/* Command interpreter in which varName is
				 * to be looked up. */
    char *varName;		/* Name of a variable in interp.  May be
				 * either a scalar name or an array name
				 * or an element in an array. */
    int flags;			/* OR-ed combination of any of
				 * TCL_GLOBAL_ONLY, TCL_NAMESPACE_ONLY or
				 * TCL_LEAVE_ERR_MSG. */
{
    return Tcl_UnsetVar2(interp, varName, (char *) NULL, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UnsetVar2 --
 *
 *	Delete a variable, given a 2-part name.
 *
 * Results:
 *	Returns TCL_OK if the variable was successfully deleted, TCL_ERROR
 *	if the variable can't be unset.  In the event of an error,
 *	if the TCL_LEAVE_ERR_MSG flag is set then an error message
 *	is left in the interp's result.
 *
 * Side effects:
 *	If part1 and part2 indicate a local or global variable in interp,
 *	it is deleted.  If part1 is an array name and part2 is NULL, then
 *	the whole array is deleted.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UnsetVar2(interp, part1, part2, flags)
    Tcl_Interp *interp;		/* Command interpreter in which varName is
				 * to be looked up. */
    char *part1;		/* Name of variable or array. */
    char *part2;		/* Name of element within array or NULL. */
    int flags;			/* OR-ed combination of any of
				 * TCL_GLOBAL_ONLY, TCL_NAMESPACE_ONLY,
				 * TCL_LEAVE_ERR_MSG. */
{
    Var dummyVar;
    Var *varPtr, *dummyVarPtr;
    Interp *iPtr = (Interp *) interp;
    Var *arrayPtr;
    ActiveVarTrace *activePtr;
    Tcl_Obj *objPtr;
    int result;

    varPtr = TclLookupVar(interp, part1, part2, flags, "unset",
	    /*createPart1*/ 0, /*createPart2*/ 0, &arrayPtr);
    if (varPtr == NULL) {
	return TCL_ERROR;
    }
    result = (TclIsVarUndefined(varPtr)? TCL_ERROR : TCL_OK);

    if ((arrayPtr != NULL) && (arrayPtr->searchPtr != NULL)) {
	DeleteSearches(arrayPtr);
    }

    /*
     * The code below is tricky, because of the possibility that
     * a trace procedure might try to access a variable being
     * deleted. To handle this situation gracefully, do things
     * in three steps:
     * 1. Copy the contents of the variable to a dummy variable
     *    structure, and mark the original Var structure as undefined.
     * 2. Invoke traces and clean up the variable, using the dummy copy.
     * 3. If at the end of this the original variable is still
     *    undefined and has no outstanding references, then delete
     *	  it (but it could have gotten recreated by a trace).
     */

    dummyVar = *varPtr;
    TclSetVarUndefined(varPtr);
    TclSetVarScalar(varPtr);
    varPtr->value.objPtr = NULL; /* dummyVar points to any value object */
    varPtr->tracePtr = NULL;
    varPtr->searchPtr = NULL;

    /*
     * Call trace procedures for the variable being deleted. Then delete
     * its traces. Be sure to abort any other traces for the variable
     * that are still pending. Special tricks:
     * 1. We need to increment varPtr's refCount around this: CallTraces
     *    will use dummyVar so it won't increment varPtr's refCount itself.
     * 2. Turn off the VAR_TRACE_ACTIVE flag in dummyVar: we want to
     *    call unset traces even if other traces are pending.
     */

    if ((dummyVar.tracePtr != NULL)
	    || ((arrayPtr != NULL) && (arrayPtr->tracePtr != NULL))) {
	varPtr->refCount++;
	dummyVar.flags &= ~VAR_TRACE_ACTIVE;
	(void) CallTraces(iPtr, arrayPtr, &dummyVar, part1, part2,
		(flags & (TCL_GLOBAL_ONLY|TCL_NAMESPACE_ONLY)) | TCL_TRACE_UNSETS);
	while (dummyVar.tracePtr != NULL) {
	    VarTrace *tracePtr = dummyVar.tracePtr;
	    dummyVar.tracePtr = tracePtr->nextPtr;
	    ckfree((char *) tracePtr);
	}
	for (activePtr = iPtr->activeTracePtr;  activePtr != NULL;
	     activePtr = activePtr->nextPtr) {
	    if (activePtr->varPtr == varPtr) {
		activePtr->nextTracePtr = NULL;
	    }
	}
	varPtr->refCount--;
    }

    /*
     * If the variable is an array, delete all of its elements. This must be
     * done after calling the traces on the array, above (that's the way
     * traces are defined). If it is a scalar, "discard" its object
     * (decrement the ref count of its object, if any).
     */

    dummyVarPtr = &dummyVar;
    if (TclIsVarArray(dummyVarPtr) && !TclIsVarUndefined(dummyVarPtr)) {
	/*
	 * Deleting the elements of the array may cause traces to be fired
	 * on those elements.  Before deleting them, bump the reference count
	 * of the array, so that if those trace procs make a global or upvar
	 * link to the array, the array is not deleted when the call stack
	 * gets popped (we will delete the array ourselves later in this
	 * function).
	 *
	 * Bumping the count can lead to the odd situation that elements of the
	 * array are being deleted when the array still exists, but since the
	 * array is about to be removed anyway, that shouldn't really matter.
	 */
	varPtr->refCount++;
	DeleteArray(iPtr, part1, dummyVarPtr,
		(flags & (TCL_GLOBAL_ONLY|TCL_NAMESPACE_ONLY)) | TCL_TRACE_UNSETS);
	/* Decr ref count */
	varPtr->refCount--;
    }
    if (TclIsVarScalar(dummyVarPtr)
	    && (dummyVarPtr->value.objPtr != NULL)) {
	objPtr = dummyVarPtr->value.objPtr;
	TclDecrRefCount(objPtr);
	dummyVarPtr->value.objPtr = NULL;
    }

    /*
     * If the variable was a namespace variable, decrement its reference count.
     */
    
    if (varPtr->flags & VAR_NAMESPACE_VAR) {
	varPtr->flags &= ~VAR_NAMESPACE_VAR;
	varPtr->refCount--;
    }

    /*
     * It's an error to unset an undefined variable.
     */
	
    if (result != TCL_OK) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    VarErrMsg(interp, part1, part2, "unset", 
		    ((arrayPtr == NULL) ? noSuchVar : noSuchElement));
	}
    }

    /*
     * Finally, if the variable is truly not in use then free up its Var
     * structure and remove it from its hash table, if any. The ref count of
     * its value object, if any, was decremented above.
     */

    CleanupVar(varPtr, arrayPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_TraceVar --
 *
 *	Arrange for reads and/or writes to a variable to cause a
 *	procedure to be invoked, which can monitor the operations
 *	and/or change their actions.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	A trace is set up on the variable given by varName, such that
 *	future references to the variable will be intermediated by
 *	proc.  See the manual entry for complete details on the calling
 *	sequence for proc.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_TraceVar(interp, varName, flags, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter in which variable is
				 * to be traced. */
    char *varName;		/* Name of variable;  may end with "(index)"
				 * to signify an array reference. */
    int flags;			/* OR-ed collection of bits, including any
				 * of TCL_TRACE_READS, TCL_TRACE_WRITES,
				 * TCL_TRACE_UNSETS, TCL_GLOBAL_ONLY, and
				 * TCL_NAMESPACE_ONLY. */
    Tcl_VarTraceProc *proc;	/* Procedure to call when specified ops are
				 * invoked upon varName. */
    ClientData clientData;	/* Arbitrary argument to pass to proc. */
{
    return Tcl_TraceVar2(interp, varName, (char *) NULL, 
	    flags, proc, clientData);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_TraceVar2 --
 *
 *	Arrange for reads and/or writes to a variable to cause a
 *	procedure to be invoked, which can monitor the operations
 *	and/or change their actions.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	A trace is set up on the variable given by part1 and part2, such
 *	that future references to the variable will be intermediated by
 *	proc.  See the manual entry for complete details on the calling
 *	sequence for proc.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_TraceVar2(interp, part1, part2, flags, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter in which variable is
				 * to be traced. */
    char *part1;		/* Name of scalar variable or array. */
    char *part2;		/* Name of element within array;  NULL means
				 * trace applies to scalar variable or array
				 * as-a-whole. */
    int flags;			/* OR-ed collection of bits, including any
				 * of TCL_TRACE_READS, TCL_TRACE_WRITES,
				 * TCL_TRACE_UNSETS, TCL_GLOBAL_ONLY,
				 * and TCL_NAMESPACE_ONLY. */
    Tcl_VarTraceProc *proc;	/* Procedure to call when specified ops are
				 * invoked upon varName. */
    ClientData clientData;	/* Arbitrary argument to pass to proc. */
{
    Var *varPtr, *arrayPtr;
    register VarTrace *tracePtr;

    varPtr = TclLookupVar(interp, part1, part2, (flags | TCL_LEAVE_ERR_MSG),
	    "trace", /*createPart1*/ 1, /*createPart2*/ 1, &arrayPtr);
    if (varPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Set up trace information.
     */

    tracePtr = (VarTrace *) ckalloc(sizeof(VarTrace));
    tracePtr->traceProc = proc;
    tracePtr->clientData = clientData;
    tracePtr->flags = 
	flags & (TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS | 
		TCL_TRACE_ARRAY);
    tracePtr->nextPtr = varPtr->tracePtr;
    varPtr->tracePtr = tracePtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UntraceVar --
 *
 *	Remove a previously-created trace for a variable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there exists a trace for the variable given by varName
 *	with the given flags, proc, and clientData, then that trace
 *	is removed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_UntraceVar(interp, varName, flags, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *varName;		/* Name of variable; may end with "(index)"
				 * to signify an array reference. */
    int flags;			/* OR-ed collection of bits describing
				 * current trace, including any of
				 * TCL_TRACE_READS, TCL_TRACE_WRITES,
				 * TCL_TRACE_UNSETS, TCL_GLOBAL_ONLY
				 * and TCL_NAMESPACE_ONLY. */
    Tcl_VarTraceProc *proc;	/* Procedure assocated with trace. */
    ClientData clientData;	/* Arbitrary argument to pass to proc. */
{
    Tcl_UntraceVar2(interp, varName, (char *) NULL, flags, proc, clientData);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UntraceVar2 --
 *
 *	Remove a previously-created trace for a variable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there exists a trace for the variable given by part1
 *	and part2 with the given flags, proc, and clientData, then
 *	that trace is removed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_UntraceVar2(interp, part1, part2, flags, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *part1;		/* Name of variable or array. */
    char *part2;		/* Name of element within array;  NULL means
				 * trace applies to scalar variable or array
				 * as-a-whole. */
    int flags;			/* OR-ed collection of bits describing
				 * current trace, including any of
				 * TCL_TRACE_READS, TCL_TRACE_WRITES,
				 * TCL_TRACE_UNSETS, TCL_GLOBAL_ONLY,
				 * and TCL_NAMESPACE_ONLY. */
    Tcl_VarTraceProc *proc;	/* Procedure assocated with trace. */
    ClientData clientData;	/* Arbitrary argument to pass to proc. */
{
    register VarTrace *tracePtr;
    VarTrace *prevPtr;
    Var *varPtr, *arrayPtr;
    Interp *iPtr = (Interp *) interp;
    ActiveVarTrace *activePtr;

    varPtr = TclLookupVar(interp, part1, part2,
	    flags & (TCL_GLOBAL_ONLY|TCL_NAMESPACE_ONLY),
	    /*msg*/ (char *) NULL,
	    /*createPart1*/ 0, /*createPart2*/ 0, &arrayPtr);
    if (varPtr == NULL) {
	return;
    }

    flags &= (TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS |
	    TCL_TRACE_ARRAY);
    for (tracePtr = varPtr->tracePtr, prevPtr = NULL;  ;
	 prevPtr = tracePtr, tracePtr = tracePtr->nextPtr) {
	if (tracePtr == NULL) {
	    return;
	}
	if ((tracePtr->traceProc == proc) && (tracePtr->flags == flags)
		&& (tracePtr->clientData == clientData)) {
	    break;
	}
    }

    /*
     * The code below makes it possible to delete traces while traces
     * are active: it makes sure that the deleted trace won't be
     * processed by CallTraces.
     */

    for (activePtr = iPtr->activeTracePtr;  activePtr != NULL;
	 activePtr = activePtr->nextPtr) {
	if (activePtr->nextTracePtr == tracePtr) {
	    activePtr->nextTracePtr = tracePtr->nextPtr;
	}
    }
    if (prevPtr == NULL) {
	varPtr->tracePtr = tracePtr->nextPtr;
    } else {
	prevPtr->nextPtr = tracePtr->nextPtr;
    }
    ckfree((char *) tracePtr);

    /*
     * If this is the last trace on the variable, and the variable is
     * unset and unused, then free up the variable.
     */

    if (TclIsVarUndefined(varPtr)) {
	CleanupVar(varPtr, (Var *) NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_VarTraceInfo --
 *
 *	Return the clientData value associated with a trace on a
 *	variable.  This procedure can also be used to step through
 *	all of the traces on a particular variable that have the
 *	same trace procedure.
 *
 * Results:
 *	The return value is the clientData value associated with
 *	a trace on the given variable.  Information will only be
 *	returned for a trace with proc as trace procedure.  If
 *	the clientData argument is NULL then the first such trace is
 *	returned;  otherwise, the next relevant one after the one
 *	given by clientData will be returned.  If the variable
 *	doesn't exist, or if there are no (more) traces for it,
 *	then NULL is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_VarTraceInfo(interp, varName, flags, proc, prevClientData)
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *varName;		/* Name of variable;  may end with "(index)"
				 * to signify an array reference. */
    int flags;			/* OR-ed combo or TCL_GLOBAL_ONLY,
				 * TCL_NAMESPACE_ONLY (can be 0). */
    Tcl_VarTraceProc *proc;	/* Procedure assocated with trace. */
    ClientData prevClientData;	/* If non-NULL, gives last value returned
				 * by this procedure, so this call will
				 * return the next trace after that one.
				 * If NULL, this call will return the
				 * first trace. */
{
    return Tcl_VarTraceInfo2(interp, varName, (char *) NULL,
	    flags, proc, prevClientData);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_VarTraceInfo2 --
 *
 *	Same as Tcl_VarTraceInfo, except takes name in two pieces
 *	instead of one.
 *
 * Results:
 *	Same as Tcl_VarTraceInfo.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_VarTraceInfo2(interp, part1, part2, flags, proc, prevClientData)
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *part1;		/* Name of variable or array. */
    char *part2;		/* Name of element within array;  NULL means
				 * trace applies to scalar variable or array
				 * as-a-whole. */
    int flags;			/* OR-ed combination of TCL_GLOBAL_ONLY,
				 * TCL_NAMESPACE_ONLY. */
    Tcl_VarTraceProc *proc;	/* Procedure assocated with trace. */
    ClientData prevClientData;	/* If non-NULL, gives last value returned
				 * by this procedure, so this call will
				 * return the next trace after that one.
				 * If NULL, this call will return the
				 * first trace. */
{
    register VarTrace *tracePtr;
    Var *varPtr, *arrayPtr;

    varPtr = TclLookupVar(interp, part1, part2,
	    flags & (TCL_GLOBAL_ONLY|TCL_NAMESPACE_ONLY),
	    /*msg*/ (char *) NULL,
	    /*createPart1*/ 0, /*createPart2*/ 0, &arrayPtr);
    if (varPtr == NULL) {
	return NULL;
    }

    /*
     * Find the relevant trace, if any, and return its clientData.
     */

    tracePtr = varPtr->tracePtr;
    if (prevClientData != NULL) {
	for ( ;  tracePtr != NULL;  tracePtr = tracePtr->nextPtr) {
	    if ((tracePtr->clientData == prevClientData)
		    && (tracePtr->traceProc == proc)) {
		tracePtr = tracePtr->nextPtr;
		break;
	    }
	}
    }
    for ( ;  tracePtr != NULL;  tracePtr = tracePtr->nextPtr) {
	if (tracePtr->traceProc == proc) {
	    return tracePtr->clientData;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UnsetObjCmd --
 *
 *	This object-based procedure is invoked to process the "unset" Tcl
 *	command. See the user documentation for details on what it does.
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
Tcl_UnsetObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *const objv[];	/* Argument objects. */
{
    register int i;
    register char *name;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "varName ?varName ...?");
	return TCL_ERROR;
    }
    
    for (i = 1;  i < objc;  i++) {
	name = TclGetString(objv[i]);
	if (Tcl_UnsetVar2(interp, name, (char *) NULL,
		TCL_LEAVE_ERR_MSG) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendObjCmd --
 *
 *	This object-based procedure is invoked to process the "append" 
 *	Tcl command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result value.
 *
 * Side effects:
 *	A variable's value may be changed.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_AppendObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *const objv[];	/* Argument objects. */
{
    register Tcl_Obj *varValuePtr = NULL;
    					/* Initialized to avoid compiler
				         * warning. */
    int i;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "varName ?value value ...?");
	return TCL_ERROR;
    }
    if (objc == 2) {
	varValuePtr = Tcl_ObjGetVar2(interp, objv[1], NULL, TCL_LEAVE_ERR_MSG);
	if (varValuePtr == NULL) {
	    return TCL_ERROR;
	}
    } else {
	for (i = 2;  i < objc;  i++) {
	    varValuePtr = Tcl_ObjSetVar2(interp, objv[1], (Tcl_Obj *) NULL,
		    objv[i], (TCL_APPEND_VALUE | TCL_LEAVE_ERR_MSG));
	    if (varValuePtr == NULL) {
		return TCL_ERROR;
	    }
	}
    }
    Tcl_SetObjResult(interp, varValuePtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LappendObjCmd --
 *
 *	This object-based procedure is invoked to process the "lappend" 
 *	Tcl command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result value.
 *
 * Side effects:
 *	A variable's value may be changed.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_LappendObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *const objv[];	/* Argument objects. */
{
    Tcl_Obj *varValuePtr, *newValuePtr;
    register List *listRepPtr;
    register Tcl_Obj **elemPtrs;
    int numElems, numRequired, createdNewObj, createVar, i, j;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "varName ?value value ...?");
	return TCL_ERROR;
    }
    if (objc == 2) {
	newValuePtr = Tcl_ObjGetVar2(interp, objv[1], (Tcl_Obj *) NULL,
		(TCL_LEAVE_ERR_MSG));
	if (newValuePtr == NULL) {
	    /*
	     * The variable doesn't exist yet. Just create it with an empty
	     * initial value.
	     */
	    
	    Tcl_Obj *nullObjPtr = Tcl_NewObj();
	    newValuePtr = Tcl_ObjSetVar2(interp, objv[1], NULL,
		    nullObjPtr, TCL_LEAVE_ERR_MSG);
	    if (newValuePtr == NULL) {
		Tcl_DecrRefCount(nullObjPtr); /* free unneeded object */
		return TCL_ERROR;
	    }
	}
    } else {
	/*
	 * We have arguments to append. We used to call Tcl_SetVar2 to
	 * append each argument one at a time to ensure that traces were run
	 * for each append step. We now append the arguments all at once
	 * because it's faster. Note that a read trace and a write trace for
	 * the variable will now each only be called once. Also, if the
	 * variable's old value is unshared we modify it directly, otherwise
	 * we create a new copy to modify: this is "copy on write".
	 */

	createdNewObj = 0;
	createVar = 1;
	varValuePtr = Tcl_ObjGetVar2(interp, objv[1], NULL, 0);
	if (varValuePtr == NULL) {
	    /*
	     * We couldn't read the old value: either the var doesn't yet
	     * exist or it's an array element. If it's new, we will try to
	     * create it with Tcl_ObjSetVar2 below.
	     */
	    
	    char *p, *varName;
	    int nameBytes, i;

	    varName = Tcl_GetStringFromObj(objv[1], &nameBytes);
	    for (i = 0, p = varName;  i < nameBytes;  i++, p++) {
		if (*p == '(') {
		    p = (varName + nameBytes-1);	
		    if (*p == ')') { /* last char is ')' => array ref */
			createVar = 0;
		    }
		    break;
		}
	    }
	    varValuePtr = Tcl_NewObj();
	    createdNewObj = 1;
	} else if (Tcl_IsShared(varValuePtr)) {	
	    varValuePtr = Tcl_DuplicateObj(varValuePtr);
	    createdNewObj = 1;
	}

	/*
	 * Convert the variable's old value to a list object if necessary.
	 */

	if (varValuePtr->typePtr != &tclListType) {
	    int result = tclListType.setFromAnyProc(interp, varValuePtr);
	    if (result != TCL_OK) {
		if (createdNewObj) {
		    Tcl_DecrRefCount(varValuePtr); /* free unneeded obj. */
		}
		return result;
	    }
	}
	listRepPtr = (List *) varValuePtr->internalRep.otherValuePtr;
	elemPtrs = listRepPtr->elements;
	numElems = listRepPtr->elemCount;

	/*
	 * If there is no room in the current array of element pointers,
	 * allocate a new, larger array and copy the pointers to it.
	 */
	
	numRequired = numElems + (objc-2);
	if (numRequired > listRepPtr->maxElemCount) {
	    int newMax = (2 * numRequired);
	    Tcl_Obj **newElemPtrs = (Tcl_Obj **)
		ckalloc((unsigned) (newMax * sizeof(Tcl_Obj *)));
	    
	    memcpy((VOID *) newElemPtrs, (VOID *) elemPtrs,
		    (size_t) (numElems * sizeof(Tcl_Obj *)));
	    listRepPtr->maxElemCount = newMax;
	    listRepPtr->elements = newElemPtrs;
	    ckfree((char *) elemPtrs);
	    elemPtrs = newElemPtrs;
	}

	/*
	 * Insert the new elements at the end of the list.
	 */

	for (i = 2, j = numElems;  i < objc;  i++, j++) {
            elemPtrs[j] = objv[i];
            Tcl_IncrRefCount(objv[i]);
        }
	listRepPtr->elemCount = numRequired;

	/*
	 * Invalidate and free any old string representation since it no
	 * longer reflects the list's internal representation.
	 */

	Tcl_InvalidateStringRep(varValuePtr);

	/*
	 * Now store the list object back into the variable. If there is an
	 * error setting the new value, decrement its ref count if it
	 * was new and we didn't create the variable.
	 */
	
	newValuePtr = Tcl_ObjSetVar2(interp, objv[1], NULL, varValuePtr,
		TCL_LEAVE_ERR_MSG);
	if (newValuePtr == NULL) {
	    if (createdNewObj && !createVar) {
		Tcl_DecrRefCount(varValuePtr); /* free unneeded obj */
	    }
	    return TCL_ERROR;
	}
    }

    /*
     * Set the interpreter's object result to refer to the variable's value
     * object.
     */

    Tcl_SetObjResult(interp, newValuePtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ArrayObjCmd --
 *
 *	This object-based procedure is invoked to process the "array" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result object.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_ArrayObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *const objv[];	/* Argument objects. */
{
    /*
     * The list of constants below should match the arrayOptions string array
     * below.
     */

    enum {ARRAY_ANYMORE, ARRAY_DONESEARCH,  ARRAY_EXISTS, ARRAY_GET,
	  ARRAY_NAMES, ARRAY_NEXTELEMENT, ARRAY_SET, ARRAY_SIZE,
	  ARRAY_STARTSEARCH, ARRAY_UNSET}; 
    static char *arrayOptions[] = {
	"anymore", "donesearch", "exists", "get", "names", "nextelement",
	"set", "size", "startsearch", "unset", (char *) NULL
    };

    Interp *iPtr = (Interp *) interp;
    Var *varPtr, *arrayPtr;
    Tcl_HashEntry *hPtr;
    Tcl_Obj *resultPtr = Tcl_GetObjResult(interp);
    int notArray;
    char *varName, *msg;
    int index, result;


    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "option arrayName ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], arrayOptions, "option",
	    0, &index) != TCL_OK) {
    	return TCL_ERROR;
    }

    /*
     * Locate the array variable (and it better be an array).
     */
    
    varName = TclGetString(objv[2]);
    varPtr = TclLookupVar(interp, varName, (char *) NULL, /*flags*/ 0,
            /*msg*/ 0, /*createPart1*/ 0, /*createPart2*/ 0, &arrayPtr);

    notArray = 0;
    if ((varPtr == NULL) || !TclIsVarArray(varPtr)
	    || TclIsVarUndefined(varPtr)) {
	notArray = 1;
    }

    /*
     * Special array trace used to keep the env array in sync for
     * array names, array get, etc.
     */

    if (varPtr != NULL && varPtr->tracePtr != NULL) {
	msg = CallTraces(iPtr, arrayPtr, varPtr, varName, NULL,
		(TCL_LEAVE_ERR_MSG|TCL_NAMESPACE_ONLY|TCL_GLOBAL_ONLY|
		TCL_TRACE_ARRAY));
	if (msg != NULL) {
	    VarErrMsg(interp, varName, NULL, "trace array", msg);
	    return TCL_ERROR;
	}
    }

    switch (index) {
        case ARRAY_ANYMORE: {
	    ArraySearch *searchPtr;
	    char *searchId;
	    
	    if (objc != 4) {
	        Tcl_WrongNumArgs(interp, 2, objv, 
                        "arrayName searchId");
		return TCL_ERROR;
	    }
	    if (notArray) {
	        goto error;
	    }
	    searchId = Tcl_GetString(objv[3]);
	    searchPtr = ParseSearchId(interp, varPtr, varName, searchId);
	    if (searchPtr == NULL) {
	        return TCL_ERROR;
	    }
	    while (1) {
	        Var *varPtr2;

		if (searchPtr->nextEntry != NULL) {
		    varPtr2 = (Var *) Tcl_GetHashValue(searchPtr->nextEntry);
		    if (!TclIsVarUndefined(varPtr2)) {
		        break;
		    }
		}
		searchPtr->nextEntry = Tcl_NextHashEntry(&searchPtr->search);
		if (searchPtr->nextEntry == NULL) {
		    Tcl_SetIntObj(resultPtr, 0);
		    return TCL_OK;
		}
	    }
	    Tcl_SetIntObj(resultPtr, 1);
	    break;
	}
        case ARRAY_DONESEARCH: {
	    ArraySearch *searchPtr, *prevPtr;
	    char *searchId;

	    if (objc != 4) {
	        Tcl_WrongNumArgs(interp, 2, objv, 
                        "arrayName searchId");
		return TCL_ERROR;
	    }
	    if (notArray) {
	        goto error;
	    }
	    searchId = Tcl_GetString(objv[3]);
	    searchPtr = ParseSearchId(interp, varPtr, varName, searchId);
	    if (searchPtr == NULL) {
	        return TCL_ERROR;
	    }
	    if (varPtr->searchPtr == searchPtr) {
	        varPtr->searchPtr = searchPtr->nextPtr;
	    } else {
	        for (prevPtr = varPtr->searchPtr;  ;
		     prevPtr = prevPtr->nextPtr) {
		    if (prevPtr->nextPtr == searchPtr) {
		        prevPtr->nextPtr = searchPtr->nextPtr;
			break;
		    }
		}
	    }
	    ckfree((char *) searchPtr);
	    break;
	}
        case ARRAY_EXISTS: {
	    if (objc != 3) {
	        Tcl_WrongNumArgs(interp, 2, objv, "arrayName");
	        return TCL_ERROR;
	    }
	    Tcl_SetIntObj(resultPtr, !notArray);
	    break;
	}
        case ARRAY_GET: {
	    Tcl_HashSearch search;
	    Var *varPtr2;
	    char *pattern = NULL;
	    char *name;
	    Tcl_Obj *namePtr, *valuePtr;
	    
	    if ((objc != 3) && (objc != 4)) {
	        Tcl_WrongNumArgs(interp, 2, objv, "arrayName ?pattern?");
		return TCL_ERROR;
	    }
	    if (notArray) {
	        return TCL_OK;
	    }
	    if (objc == 4) {
	        pattern = TclGetString(objv[3]);
	    }
	    for (hPtr = Tcl_FirstHashEntry(varPtr->value.tablePtr, &search);
		 hPtr != NULL;  hPtr = Tcl_NextHashEntry(&search)) {
	        varPtr2 = (Var *) Tcl_GetHashValue(hPtr);
		if (TclIsVarUndefined(varPtr2)) {
		    continue;
		}
		name = Tcl_GetHashKey(varPtr->value.tablePtr, hPtr);
		if ((objc == 4) && !Tcl_StringMatch(name, pattern)) {
		    continue;	/* element name doesn't match pattern */
		}
		
		namePtr = Tcl_NewStringObj(name, -1);
		result = Tcl_ListObjAppendElement(interp, resultPtr,
		        namePtr);
		if (result != TCL_OK) {
		    Tcl_DecrRefCount(namePtr); /* free unneeded name obj */
		    return result;
		}

		valuePtr = Tcl_ObjGetVar2(interp, objv[2], namePtr,
	                TCL_LEAVE_ERR_MSG);
		if (valuePtr == NULL) {
		    Tcl_DecrRefCount(namePtr); /* free unneeded name obj */
		    return result;
		}
		result = Tcl_ListObjAppendElement(interp, resultPtr,
			valuePtr);
		if (result != TCL_OK) {
		    Tcl_DecrRefCount(namePtr); /* free unneeded name obj */
		    return result;
		}
	    }
	    break;
	}
        case ARRAY_NAMES: {
	    Tcl_HashSearch search;
	    Var *varPtr2;
	    char *pattern = NULL;
	    char *name;
	    Tcl_Obj *namePtr;
	    
	    if ((objc != 3) && (objc != 4)) {
  	        Tcl_WrongNumArgs(interp, 2, objv, "arrayName ?pattern?");
		return TCL_ERROR;
	    }
	    if (notArray) {
	        return TCL_OK;
	    }
	    if (objc == 4) {
	        pattern = Tcl_GetString(objv[3]);
	    }
	    for (hPtr = Tcl_FirstHashEntry(varPtr->value.tablePtr, &search);
		 hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	        varPtr2 = (Var *) Tcl_GetHashValue(hPtr);
		if (TclIsVarUndefined(varPtr2)) {
		    continue;
		}
		name = Tcl_GetHashKey(varPtr->value.tablePtr, hPtr);
		if ((objc == 4) && !Tcl_StringMatch(name, pattern)) {
 		    continue;	/* element name doesn't match pattern */
		}
		
		namePtr = Tcl_NewStringObj(name, -1);
		result = Tcl_ListObjAppendElement(interp, resultPtr, namePtr);
		if (result != TCL_OK) {
		    Tcl_DecrRefCount(namePtr); /* free unneeded name obj */
		    return result;
		}
	    }
	    break;
	}
        case ARRAY_NEXTELEMENT: {
	    ArraySearch *searchPtr;
	    char *searchId;
	    Tcl_HashEntry *hPtr;
	    
	    if (objc != 4) {
	        Tcl_WrongNumArgs(interp, 2, objv, 
                        "arrayName searchId");
		return TCL_ERROR;
	    }
	    if (notArray) {
  	        goto error;
	    }
	    searchId = Tcl_GetString(objv[3]);
	    searchPtr = ParseSearchId(interp, varPtr, varName, searchId);
	    if (searchPtr == NULL) {
	        return TCL_ERROR;
	    }
	    while (1) {
	        Var *varPtr2;

		hPtr = searchPtr->nextEntry;
		if (hPtr == NULL) {
		    hPtr = Tcl_NextHashEntry(&searchPtr->search);
		    if (hPtr == NULL) {
		        return TCL_OK;
		    }
		} else {
		    searchPtr->nextEntry = NULL;
		}
		varPtr2 = (Var *) Tcl_GetHashValue(hPtr);
		if (!TclIsVarUndefined(varPtr2)) {
		    break;
		}
	    }
	    Tcl_SetStringObj(resultPtr,
	            Tcl_GetHashKey(varPtr->value.tablePtr, hPtr), -1);
	    break;
	}
        case ARRAY_SET: {
	    if (objc != 4) {
	        Tcl_WrongNumArgs(interp, 2, objv, "arrayName list");
		return TCL_ERROR;
	    }
	    return(TclArraySet(interp, objv[2], objv[3]));
	}
        case ARRAY_SIZE: {
	    Tcl_HashSearch search;
	    Var *varPtr2;
	    int size;

	    if (objc != 3) {
	        Tcl_WrongNumArgs(interp, 2, objv, "arrayName");
		return TCL_ERROR;
	    }
	    size = 0;
	    if (!notArray) {
	        for (hPtr = Tcl_FirstHashEntry(varPtr->value.tablePtr, 
                        &search);
		     hPtr != NULL;  hPtr = Tcl_NextHashEntry(&search)) {
		    varPtr2 = (Var *) Tcl_GetHashValue(hPtr);
		    if (TclIsVarUndefined(varPtr2)) {
		        continue;
		    }
		    size++;
		}
	    }
	    Tcl_SetIntObj(resultPtr, size);
	    break;
	}
        case ARRAY_STARTSEARCH: {
	    ArraySearch *searchPtr;

	    if (objc != 3) {
	        Tcl_WrongNumArgs(interp, 2, objv, "arrayName");
		return TCL_ERROR;
	    }
	    if (notArray) {
	        goto error;
	    }
	    searchPtr = (ArraySearch *) ckalloc(sizeof(ArraySearch));
	    if (varPtr->searchPtr == NULL) {
	        searchPtr->id = 1;
		Tcl_AppendStringsToObj(resultPtr, "s-1-", varName,
		        (char *) NULL);
	    } else {
	        char string[TCL_INTEGER_SPACE];

		searchPtr->id = varPtr->searchPtr->id + 1;
		TclFormatInt(string, searchPtr->id);
		Tcl_AppendStringsToObj(resultPtr, "s-", string, "-", varName,
			(char *) NULL);
	    }
	    searchPtr->varPtr = varPtr;
	    searchPtr->nextEntry = Tcl_FirstHashEntry(varPtr->value.tablePtr,
		    &searchPtr->search);
	    searchPtr->nextPtr = varPtr->searchPtr;
	    varPtr->searchPtr = searchPtr;
	    break;
	}
        case ARRAY_UNSET: {
	    Tcl_HashSearch search;
	    Var *varPtr2;
	    char *pattern = NULL;
	    char *name;
          
	    if ((objc != 3) && (objc != 4)) {
		Tcl_WrongNumArgs(interp, 2, objv, "arrayName ?pattern?");
		return TCL_ERROR;
	    }
	    if (notArray) {
		return TCL_OK;
	    }
	    if (objc == 3) {
		/*
		 * When no pattern is given, just unset the whole array
		 */
		if (Tcl_UnsetVar2(interp, varName, (char *) NULL, 0)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
	    } else {
		pattern = Tcl_GetString(objv[3]);
		for (hPtr = Tcl_FirstHashEntry(varPtr->value.tablePtr,
			&search);
		     hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
		    varPtr2 = (Var *) Tcl_GetHashValue(hPtr);
		    if (TclIsVarUndefined(varPtr2)) {
			continue;
		    }
		    name = Tcl_GetHashKey(varPtr->value.tablePtr, hPtr);
		    if (Tcl_StringMatch(name, pattern) &&
			    (Tcl_UnsetVar2(interp, varName, name, 0)
				    != TCL_OK)) {
			return TCL_ERROR;
		    }
		}
	    }
	    break;
	}
    }
    return TCL_OK;

    error:
    Tcl_AppendStringsToObj(resultPtr, "\"", varName, "\" isn't an array",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclArraySet --
 *
 *	Set the elements of an array.  If there are no elements to
 *	set, create an empty array.  This routine is used by the
 *	Tcl_ArrayObjCmd and by the TclSetupEnv routine.
 *
 * Results:
 *	A standard Tcl result object.
 *
 * Side effects:
 *	A variable will be created if one does not already exist.
 *
 *----------------------------------------------------------------------
 */

int
TclArraySet(interp, arrayNameObj, arrayElemObj)
    Tcl_Interp *interp;		/* Current interpreter. */
    Tcl_Obj *arrayNameObj;	/* The array name. */
    Tcl_Obj *arrayElemObj;	/* The array elements list.  If this is
				 * NULL, create an empty array. */
{
    Var *varPtr, *arrayPtr;
    Tcl_Obj **elemPtrs;
    int result, elemLen, i;
    char *varName, *p;
    
    varName = TclGetString(arrayNameObj);
    for (p = varName; *p ; p++) {
	if (*p == '(') {
	    do {
		p++;
	    } while (*p != '\0');
	    p--;
	    if (*p == ')') {
		VarErrMsg(interp, varName, NULL, "set", needArray);
		return TCL_ERROR;
	    }
	    break;
	}
    }

    varPtr = TclLookupVar(interp, varName, (char *) NULL, /*flags*/ 0,
            /*msg*/ 0, /*createPart1*/ 0, /*createPart2*/ 0, &arrayPtr);

    if (arrayElemObj != NULL) {
	result = Tcl_ListObjGetElements(interp, arrayElemObj,
		&elemLen, &elemPtrs);
	if (result != TCL_OK) {
	    return result;
	}
	if (elemLen & 1) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		    "list must have an even number of elements", -1);
	    return TCL_ERROR;
	}
	if (elemLen > 0) {
	    for (i = 0;  i < elemLen;  i += 2) {
		if (Tcl_ObjSetVar2(interp, arrayNameObj, elemPtrs[i],
			elemPtrs[i+1], TCL_LEAVE_ERR_MSG) == NULL) {
		    result = TCL_ERROR;
		    break;
		}
	    }
	    return result;
	}
    }
    
    /*
     * The list is empty make sure we have an array, or create
     * one if necessary.
     */
    
    if (varPtr != NULL) {
	if (!TclIsVarUndefined(varPtr) && TclIsVarArray(varPtr)) {
	    /*
	     * Already an array, done.
	     */
	    
	    return TCL_OK;
	}
	if (TclIsVarArrayElement(varPtr) || !TclIsVarUndefined(varPtr)) {
	    /*
	     * Either an array element, or a scalar: lose!
	     */
	    
	    VarErrMsg(interp, varName, (char *)NULL, "array set", needArray);
	    return TCL_ERROR;
	}
    } else {
	/*
	 * Create variable for new array.
	 */
	
	varPtr = TclLookupVar(interp, varName, (char *) NULL,
		TCL_LEAVE_ERR_MSG, "set",
	        /*createPart1*/ 1, /*createPart2*/ 0, &arrayPtr);

	/*
	 * Still couldn't do it - this can occur if a non-existent
	 * namespace was specified
	 */
	if (varPtr == NULL) {
	    return TCL_ERROR;
	}
    }
    TclSetVarArray(varPtr);
    TclClearVarUndefined(varPtr);
    varPtr->value.tablePtr =
	(Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(varPtr->value.tablePtr, TCL_STRING_KEYS);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeUpvar --
 *
 *	This procedure does all of the work of the "global" and "upvar"
 *	commands.
 *
 * Results:
 *	A standard Tcl completion code. If an error occurs then an
 *	error message is left in iPtr->result.
 *
 * Side effects:
 *	The variable given by myName is linked to the variable in framePtr
 *	given by otherP1 and otherP2, so that references to myName are
 *	redirected to the other variable like a symbolic link.
 *
 *----------------------------------------------------------------------
 */

static int
MakeUpvar(iPtr, framePtr, otherP1, otherP2, otherFlags, myName, myFlags)
    Interp *iPtr;		/* Interpreter containing variables. Used
				 * for error messages, too. */
    CallFrame *framePtr;	/* Call frame containing "other" variable.
				 * NULL means use global :: context. */
    char *otherP1, *otherP2;	/* Two-part name of variable in framePtr. */
    int otherFlags;		/* 0, TCL_GLOBAL_ONLY or TCL_NAMESPACE_ONLY:
				 * indicates scope of "other" variable. */
    char *myName;		/* Name of variable which will refer to
				 * otherP1/otherP2. Must be a scalar. */
    int myFlags;		/* 0, TCL_GLOBAL_ONLY or TCL_NAMESPACE_ONLY:
				 * indicates scope of myName. */
{
    Tcl_HashEntry *hPtr;
    Var *otherPtr, *varPtr, *arrayPtr;
    CallFrame *varFramePtr;
    CallFrame *savedFramePtr = NULL;  /* Init. to avoid compiler warning. */
    Tcl_HashTable *tablePtr;
    Namespace *nsPtr, *altNsPtr, *dummyNsPtr;
    char *tail;
    int new;

    /*
     * Find "other" in "framePtr". If not looking up other in just the
     * current namespace, temporarily replace the current var frame
     * pointer in the interpreter in order to use TclLookupVar.
     */

    if (!(otherFlags & TCL_NAMESPACE_ONLY)) {
	savedFramePtr = iPtr->varFramePtr;
	iPtr->varFramePtr = framePtr;
    }
    otherPtr = TclLookupVar((Tcl_Interp *) iPtr, otherP1, otherP2,
	    (otherFlags | TCL_LEAVE_ERR_MSG), "access",
            /*createPart1*/ 1, /*createPart2*/ 1, &arrayPtr);
    if (!(otherFlags & TCL_NAMESPACE_ONLY)) {
	iPtr->varFramePtr = savedFramePtr;
    }
    if (otherPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Now create a hashtable entry for "myName". Create it as either a
     * namespace variable or as a local variable in a procedure call
     * frame. Interpret myName as a namespace variable if:
     *    1) so requested by a TCL_GLOBAL_ONLY or TCL_NAMESPACE_ONLY flag,
     *    2) there is no active frame (we're at the global :: scope),
     *    3) the active frame was pushed to define the namespace context
     *       for a "namespace eval" or "namespace inscope" command,
     *    4) the name has namespace qualifiers ("::"s).
     * If creating myName in the active procedure, look first in the
     * frame's array of compiler-allocated local variables, then in its
     * hashtable for runtime-created local variables. Create that
     * procedure's local variable hashtable if necessary.
     */

    varFramePtr = iPtr->varFramePtr;
    if ((myFlags & (TCL_GLOBAL_ONLY | TCL_NAMESPACE_ONLY))
	    || (varFramePtr == NULL)
	    || !varFramePtr->isProcCallFrame
	    || (strstr(myName, "::") != NULL)) {
	TclGetNamespaceForQualName((Tcl_Interp *) iPtr, myName,
		(Namespace *) NULL, myFlags, &nsPtr, &altNsPtr, &dummyNsPtr, &tail);

        if (nsPtr == NULL) {
            nsPtr = altNsPtr;
        }
        if (nsPtr == NULL) {
	    Tcl_AppendResult((Tcl_Interp *) iPtr, "bad variable name \"",
		    myName, "\": unknown namespace", (char *) NULL);
            return TCL_ERROR;
        }
	
	/*
	 * Check that we are not trying to create a namespace var linked to
	 * a local variable in a procedure. If we allowed this, the local
	 * variable in the shorter-lived procedure frame could go away
	 * leaving the namespace var's reference invalid.
	 */

	if ((otherP2 ? arrayPtr->nsPtr : otherPtr->nsPtr) == NULL) {
	    Tcl_AppendResult((Tcl_Interp *) iPtr, "bad variable name \"",
                    myName, "\": upvar won't create namespace variable that refers to procedure variable",
		    (char *) NULL);
            return TCL_ERROR;
        }
	
	hPtr = Tcl_CreateHashEntry(&nsPtr->varTable, tail, &new);
	if (new) {
	    varPtr = NewVar();
	    Tcl_SetHashValue(hPtr, varPtr);
	    varPtr->hPtr = hPtr;
            varPtr->nsPtr = nsPtr;
	} else {
	    varPtr = (Var *) Tcl_GetHashValue(hPtr);
	}
    } else {			/* look in the call frame */
	Proc *procPtr = varFramePtr->procPtr;
	int localCt = procPtr->numCompiledLocals;
	CompiledLocal *localPtr = procPtr->firstLocalPtr;
	Var *localVarPtr = varFramePtr->compiledLocals;
	int nameLen = strlen(myName);
	int i;

	varPtr = NULL;
	for (i = 0;  i < localCt;  i++) {
	    if (!TclIsVarTemporary(localPtr)) {
		char *localName = localVarPtr->name;
		if ((myName[0] == localName[0])
		        && (nameLen == localPtr->nameLength)
		        && (strcmp(myName, localName) == 0)) {
		    varPtr = localVarPtr;
		    new = 0;
		    break;
		}
	    }
	    localVarPtr++;
	    localPtr = localPtr->nextPtr;
	}
	if (varPtr == NULL) {	/* look in frame's local var hashtable */
	    tablePtr = varFramePtr->varTablePtr;
	    if (tablePtr == NULL) {
		tablePtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
		Tcl_InitHashTable(tablePtr, TCL_STRING_KEYS);
		varFramePtr->varTablePtr = tablePtr;
	    }
	    hPtr = Tcl_CreateHashEntry(tablePtr, myName, &new);
	    if (new) {
		varPtr = NewVar();
		Tcl_SetHashValue(hPtr, varPtr);
		varPtr->hPtr = hPtr;
                varPtr->nsPtr = varFramePtr->nsPtr;
	    } else {
		varPtr = (Var *) Tcl_GetHashValue(hPtr);
	    }
	}
    }

    if (!new) {
	/*
	 * The variable already exists. Make sure this variable "varPtr"
	 * isn't the same as "otherPtr" (avoid circular links). Also, if
	 * it's not an upvar then it's an error. If it is an upvar, then
	 * just disconnect it from the thing it currently refers to.
	 */

	if (varPtr == otherPtr) {
	    Tcl_SetResult((Tcl_Interp *) iPtr,
		    "can't upvar from variable to itself", TCL_STATIC);
	    return TCL_ERROR;
	}
	if (TclIsVarLink(varPtr)) {
	    Var *linkPtr = varPtr->value.linkPtr;
	    if (linkPtr == otherPtr) {
		return TCL_OK;
	    }
	    linkPtr->refCount--;
	    if (TclIsVarUndefined(linkPtr)) {
		CleanupVar(linkPtr, (Var *) NULL);
	    }
	} else if (!TclIsVarUndefined(varPtr)) {
	    Tcl_AppendResult((Tcl_Interp *) iPtr, "variable \"", myName,
		    "\" already exists", (char *) NULL);
	    return TCL_ERROR;
	} else if (varPtr->tracePtr != NULL) {
	    Tcl_AppendResult((Tcl_Interp *) iPtr, "variable \"", myName,
		    "\" has traces: can't use for upvar", (char *) NULL);
	    return TCL_ERROR;
	}
    }
    TclSetVarLink(varPtr);
    TclClearVarUndefined(varPtr);
    varPtr->value.linkPtr = otherPtr;
    otherPtr->refCount++;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UpVar --
 *
 *	This procedure links one variable to another, just like
 *	the "upvar" command.
 *
 * Results:
 *	A standard Tcl completion code.  If an error occurs then
 *	an error message is left in the interp's result.
 *
 * Side effects:
 *	The variable in frameName whose name is given by varName becomes
 *	accessible under the name localName, so that references to
 *	localName are redirected to the other variable like a symbolic
 *	link.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UpVar(interp, frameName, varName, localName, flags)
    Tcl_Interp *interp;		/* Command interpreter in which varName is
				 * to be looked up. */
    char *frameName;		/* Name of the frame containing the source
				 * variable, such as "1" or "#0". */
    char *varName;		/* Name of a variable in interp to link to.
				 * May be either a scalar name or an
				 * element in an array. */
    char *localName;		/* Name of link variable. */
    int flags;			/* 0, TCL_GLOBAL_ONLY or TCL_NAMESPACE_ONLY:
				 * indicates scope of localName. */
{
    int result;
    CallFrame *framePtr;
    register char *p;

    result = TclGetFrame(interp, frameName, &framePtr);
    if (result == -1) {
	return TCL_ERROR;
    }

    /*
     * Figure out whether varName is an array reference, then call
     * MakeUpvar to do all the real work.
     */

    for (p = varName;  *p != '\0';  p++) {
	if (*p == '(') {
	    char *openParen = p;
	    do {
		p++;
	    } while (*p != '\0');
	    p--;
	    if (*p != ')') {
		goto scalar;
	    }
	    *openParen = '\0';
	    *p = '\0';
	    result = MakeUpvar((Interp *) interp, framePtr, varName,
		    openParen+1, 0, localName, flags);
	    *openParen = '(';
	    *p = ')';
	    return result;
	}
    }

    scalar:
    return MakeUpvar((Interp *) interp, framePtr, varName, (char *) NULL,
	    0, localName, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UpVar2 --
 *
 *	This procedure links one variable to another, just like
 *	the "upvar" command.
 *
 * Results:
 *	A standard Tcl completion code.  If an error occurs then
 *	an error message is left in the interp's result.
 *
 * Side effects:
 *	The variable in frameName whose name is given by part1 and
 *	part2 becomes accessible under the name localName, so that
 *	references to localName are redirected to the other variable
 *	like a symbolic link.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UpVar2(interp, frameName, part1, part2, localName, flags)
    Tcl_Interp *interp;		/* Interpreter containing variables.  Used
				 * for error messages too. */
    char *frameName;		/* Name of the frame containing the source
				 * variable, such as "1" or "#0". */
    char *part1, *part2;	/* Two parts of source variable name to
				 * link to. */
    char *localName;		/* Name of link variable. */
    int flags;			/* 0, TCL_GLOBAL_ONLY or TCL_NAMESPACE_ONLY:
				 * indicates scope of localName. */
{
    int result;
    CallFrame *framePtr;

    result = TclGetFrame(interp, frameName, &framePtr);
    if (result == -1) {
	return TCL_ERROR;
    }
    return MakeUpvar((Interp *) interp, framePtr, part1, part2, 0,
	    localName, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetVariableFullName --
 *
 *	Given a Tcl_Var token returned by Tcl_FindNamespaceVar, this
 *	procedure appends to an object the namespace variable's full
 *	name, qualified by a sequence of parent namespace names.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The variable's fully-qualified name is appended to the string
 *	representation of objPtr.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_GetVariableFullName(interp, variable, objPtr)
    Tcl_Interp *interp;	        /* Interpreter containing the variable. */
    Tcl_Var variable;		/* Token for the variable returned by a
				 * previous call to Tcl_FindNamespaceVar. */
    Tcl_Obj *objPtr;		/* Points to the object onto which the
				 * variable's full name is appended. */
{
    Interp *iPtr = (Interp *) interp;
    register Var *varPtr = (Var *) variable;
    char *name;

    /*
     * Add the full name of the containing namespace (if any), followed by
     * the "::" separator, then the variable name.
     */

    if (varPtr != NULL) {
	if (!TclIsVarArrayElement(varPtr)) {
	    if (varPtr->nsPtr != NULL) {
		Tcl_AppendToObj(objPtr, varPtr->nsPtr->fullName, -1);
		if (varPtr->nsPtr != iPtr->globalNsPtr) {
		    Tcl_AppendToObj(objPtr, "::", 2);
		}
	    }
	    if (varPtr->name != NULL) {
		Tcl_AppendToObj(objPtr, varPtr->name, -1);
	    } else if (varPtr->hPtr != NULL) {
		name = Tcl_GetHashKey(varPtr->hPtr->tablePtr, varPtr->hPtr);
		Tcl_AppendToObj(objPtr, name, -1);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GlobalObjCmd --
 *
 *	This object-based procedure is invoked to process the "global" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result value.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GlobalObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *const objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    register Tcl_Obj *objPtr;
    char *varName;
    register char *tail;
    int result, i;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "varName ?varName ...?");
	return TCL_ERROR;
    }

    /*
     * If we are not executing inside a Tcl procedure, just return.
     */
    
    if ((iPtr->varFramePtr == NULL)
	    || !iPtr->varFramePtr->isProcCallFrame) {
	return TCL_OK;
    }

    for (i = 1;  i < objc;  i++) {
	/*
	 * Make a local variable linked to its counterpart in the global ::
	 * namespace.
	 */
	
	objPtr = objv[i];
	varName = TclGetString(objPtr);

	/*
	 * The variable name might have a scope qualifier, but the name for
         * the local "link" variable must be the simple name at the tail.
	 */

	for (tail = varName;  *tail != '\0';  tail++) {
	    /* empty body */
	}
        while ((tail > varName) && ((*tail != ':') || (*(tail-1) != ':'))) {
            tail--;
	}
        if (*tail == ':') {
            tail++;
	}

	/*
	 * Link to the variable "varName" in the global :: namespace.
	 */
	
	result = MakeUpvar(iPtr, (CallFrame *) NULL,
		varName, (char *) NULL, /*otherFlags*/ TCL_GLOBAL_ONLY,
	        /*myName*/ tail, /*myFlags*/ 0);
	if (result != TCL_OK) {
	    return result;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_VariableObjCmd --
 *
 *	Invoked to implement the "variable" command that creates one or more
 *	global variables. Handles the following syntax:
 *
 *	    variable ?name value...? name ?value?
 *
 *	One or more variables can be created. The variables are initialized
 *	with the specified values. The value for the last variable is
 *	optional.
 *
 *	If the variable does not exist, it is created and given the optional
 *	value. If it already exists, it is simply set to the optional
 *	value. Normally, "name" is an unqualified name, so it is created in
 *	the current namespace. If it includes namespace qualifiers, it can
 *	be created in another namespace.
 *
 *	If the variable command is executed inside a Tcl procedure, it
 *	creates a local variable linked to the newly-created namespace
 *	variable.
 *
 * Results:
 *	Returns TCL_OK if the variable is found or created. Returns
 *	TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	If anything goes wrong, this procedure returns an error message
 *	as the result in the interpreter's result object.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_VariableObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *const objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    char *varName, *tail, *cp;
    Var *varPtr, *arrayPtr;
    Tcl_Obj *varValuePtr;
    int i, result;

    for (i = 1;  i < objc;  i = i+2) {
	/*
	 * Look up each variable in the current namespace context, creating
	 * it if necessary.
	 */
	
	varName = TclGetString(objv[i]);
	varPtr = TclLookupVar(interp, varName, (char *) NULL,
                (TCL_NAMESPACE_ONLY | TCL_LEAVE_ERR_MSG), "define",
                /*createPart1*/ 1, /*createPart2*/ 0, &arrayPtr);
	
        if (arrayPtr != NULL) {
            /*
             * Variable cannot be an element in an array.  If arrayPtr is
             * non-null, it is, so throw up an error and return.
             */
            VarErrMsg(interp, varName, NULL, "define", isArrayElement);
            return TCL_ERROR;
        }

	if (varPtr == NULL) {
	    return TCL_ERROR;
	}

	/*
	 * Mark the variable as a namespace variable and increment its 
	 * reference count so that it will persist until its namespace is
	 * destroyed or until the variable is unset.
	 */

	if (!(varPtr->flags & VAR_NAMESPACE_VAR)) {
	    varPtr->flags |= VAR_NAMESPACE_VAR;
	    varPtr->refCount++;
	}

	/*
	 * If a value was specified, set the variable to that value.
	 * Otherwise, if the variable is new, leave it undefined.
	 * (If the variable already exists and no value was specified,
	 * leave its value unchanged; just create the local link if
	 * we're in a Tcl procedure).
	 */

	if (i+1 < objc) {	/* a value was specified */
	    varValuePtr = Tcl_ObjSetVar2(interp, objv[i], NULL, objv[i+1],
		    (TCL_NAMESPACE_ONLY | TCL_LEAVE_ERR_MSG));
	    if (varValuePtr == NULL) {
		return TCL_ERROR;
	    }
	}

	/*
	 * If we are executing inside a Tcl procedure, create a local
	 * variable linked to the new namespace variable "varName".
	 */

	if ((iPtr->varFramePtr != NULL)
	        && iPtr->varFramePtr->isProcCallFrame) {
	    /*
	     * varName might have a scope qualifier, but the name for the
	     * local "link" variable must be the simple name at the tail.
	     *
	     * Locate tail in one pass: drop any prefix after two *or more*
	     * consecutive ":" characters).
	     */

	    for (tail = cp = varName;  *cp != '\0'; ) {
		if (*cp++ == ':') {
		    while (*cp == ':') {
			tail = ++cp;
		    }
		}
	    }
	    
	    /*
	     * Create a local link "tail" to the variable "varName" in the
	     * current namespace.
	     */
	    
	    result = MakeUpvar(iPtr, (CallFrame *) NULL,
		    /*otherP1*/ varName, /*otherP2*/ (char *) NULL,
                    /*otherFlags*/ TCL_NAMESPACE_ONLY,
		    /*myName*/ tail, /*myFlags*/ 0);
	    if (result != TCL_OK) {
		return result;
	    }
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UpvarObjCmd --
 *
 *	This object-based procedure is invoked to process the "upvar"
 *	Tcl command. See the user documentation for details on what it does.
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
Tcl_UpvarObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *const objv[];	/* Argument objects. */
{
    register Interp *iPtr = (Interp *) interp;
    CallFrame *framePtr;
    char *frameSpec, *otherVarName, *myVarName;
    register char *p;
    int result;

    if (objc < 3) {
	upvarSyntax:
	Tcl_WrongNumArgs(interp, 1, objv,
		"?level? otherVar localVar ?otherVar localVar ...?");
	return TCL_ERROR;
    }

    /*
     * Find the call frame containing each of the "other variables" to be
     * linked to. 
     */

    frameSpec = TclGetString(objv[1]);
    result = TclGetFrame(interp, frameSpec, &framePtr);
    if (result == -1) {
	return TCL_ERROR;
    }
    objc -= result+1;
    if ((objc & 1) != 0) {
	goto upvarSyntax;
    }
    objv += result+1;

    /*
     * Iterate over each (other variable, local variable) pair.
     * Divide the other variable name into two parts, then call
     * MakeUpvar to do all the work of linking it to the local variable.
     */

    for ( ;  objc > 0;  objc -= 2, objv += 2) {
	myVarName = TclGetString(objv[1]);
	otherVarName = TclGetString(objv[0]);
	for (p = otherVarName;  *p != 0;  p++) {
	    if (*p == '(') {
		char *openParen = p;

		do {
		    p++;
		} while (*p != '\0');
		p--;
		if (*p != ')') {
		    goto scalar;
		}
		*openParen = '\0';
		*p = '\0';
		result = MakeUpvar(iPtr, framePtr,
		        otherVarName, openParen+1, /*otherFlags*/ 0,
			myVarName, /*flags*/ 0);
		*openParen = '(';
		*p = ')';
		goto checkResult;
	    }
	}
	scalar:
	result = MakeUpvar(iPtr, framePtr, otherVarName, (char *) NULL, 0,
	        myVarName, /*flags*/ 0);

	checkResult:
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CallTraces --
 *
 *	This procedure is invoked to find and invoke relevant
 *	trace procedures associated with a particular operation on
 *	a variable. This procedure invokes traces both on the
 *	variable and on its containing array (where relevant).
 *
 * Results:
 *	The return value is NULL if no trace procedures were invoked, or
 *	if all the invoked trace procedures returned successfully.
 *	The return value is non-NULL if a trace procedure returned an
 *	error (in this case no more trace procedures were invoked after
 *	the error was returned). In this case the return value is a
 *	pointer to a static string describing the error.
 *
 * Side effects:
 *	Almost anything can happen, depending on trace; this procedure
 *	itself doesn't have any side effects.
 *
 *----------------------------------------------------------------------
 */

static char *
CallTraces(iPtr, arrayPtr, varPtr, part1, part2, flags)
    Interp *iPtr;		/* Interpreter containing variable. */
    register Var *arrayPtr;	/* Pointer to array variable that contains
				 * the variable, or NULL if the variable
				 * isn't an element of an array. */
    Var *varPtr;		/* Variable whose traces are to be
				 * invoked. */
    char *part1, *part2;	/* Variable's two-part name. */
    int flags;			/* Flags passed to trace procedures:
				 * indicates what's happening to variable,
				 * plus other stuff like TCL_GLOBAL_ONLY,
				 * TCL_NAMESPACE_ONLY, and
				 * TCL_INTERP_DESTROYED. */
{
    register VarTrace *tracePtr;
    ActiveVarTrace active;
    char *result, *openParen, *p;
    Tcl_DString nameCopy;
    int copiedName;

    /*
     * If there are already similar trace procedures active for the
     * variable, don't call them again.
     */

    if (varPtr->flags & VAR_TRACE_ACTIVE) {
	return NULL;
    }
    varPtr->flags |= VAR_TRACE_ACTIVE;
    varPtr->refCount++;

    /*
     * If the variable name hasn't been parsed into array name and
     * element, do it here.  If there really is an array element,
     * make a copy of the original name so that NULLs can be
     * inserted into it to separate the names (can't modify the name
     * string in place, because the string might get used by the
     * callbacks we invoke).
     */

    copiedName = 0;
    if (part2 == NULL) {
	for (p = part1; *p ; p++) {
	    if (*p == '(') {
		openParen = p;
		do {
		    p++;
		} while (*p != '\0');
		p--;
		if (*p == ')') {
		    Tcl_DStringInit(&nameCopy);
		    Tcl_DStringAppend(&nameCopy, part1, (p-part1));
		    part2 = Tcl_DStringValue(&nameCopy)
			+ (openParen + 1 - part1);
		    part2[-1] = 0;
		    part1 = Tcl_DStringValue(&nameCopy);
		    copiedName = 1;
		}
		break;
	    }
	}
    }

    /*
     * Invoke traces on the array containing the variable, if relevant.
     */

    result = NULL;
    active.nextPtr = iPtr->activeTracePtr;
    iPtr->activeTracePtr = &active;
    if (arrayPtr != NULL) {
	arrayPtr->refCount++;
	active.varPtr = arrayPtr;
	for (tracePtr = arrayPtr->tracePtr;  tracePtr != NULL;
	     tracePtr = active.nextTracePtr) {
	    active.nextTracePtr = tracePtr->nextPtr;
	    if (!(tracePtr->flags & flags)) {
		continue;
	    }
	    result = (*tracePtr->traceProc)(tracePtr->clientData,
		    (Tcl_Interp *) iPtr, part1, part2, flags);
	    if (result != NULL) {
		if (flags & TCL_TRACE_UNSETS) {
		    result = NULL;
		} else {
		    goto done;
		}
	    }
	}
    }

    /*
     * Invoke traces on the variable itself.
     */

    if (flags & TCL_TRACE_UNSETS) {
	flags |= TCL_TRACE_DESTROYED;
    }
    active.varPtr = varPtr;
    for (tracePtr = varPtr->tracePtr; tracePtr != NULL;
	 tracePtr = active.nextTracePtr) {
	active.nextTracePtr = tracePtr->nextPtr;
	if (!(tracePtr->flags & flags)) {
	    continue;
	}
	result = (*tracePtr->traceProc)(tracePtr->clientData,
		(Tcl_Interp *) iPtr, part1, part2, flags);
	if (result != NULL) {
	    if (flags & TCL_TRACE_UNSETS) {
		result = NULL;
	    } else {
		goto done;
	    }
	}
    }

    /*
     * Restore the variable's flags, remove the record of our active
     * traces, and then return.
     */

    done:
    if (arrayPtr != NULL) {
	arrayPtr->refCount--;
    }
    if (copiedName) {
	Tcl_DStringFree(&nameCopy);
    }
    varPtr->flags &= ~VAR_TRACE_ACTIVE;
    varPtr->refCount--;
    iPtr->activeTracePtr = active.nextPtr;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * NewVar --
 *
 *	Create a new heap-allocated variable that will eventually be
 *	entered into a hashtable.
 *
 * Results:
 *	The return value is a pointer to the new variable structure. It is
 *	marked as a scalar variable (and not a link or array variable). Its
 *	value initially is NULL. The variable is not part of any hash table
 *	yet. Since it will be in a hashtable and not in a call frame, its
 *	name field is set NULL. It is initially marked as undefined.
 *
 * Side effects:
 *	Storage gets allocated.
 *
 *----------------------------------------------------------------------
 */

static Var *
NewVar()
{
    register Var *varPtr;

    varPtr = (Var *) ckalloc(sizeof(Var));
    varPtr->value.objPtr = NULL;
    varPtr->name = NULL;
    varPtr->nsPtr = NULL;
    varPtr->hPtr = NULL;
    varPtr->refCount = 0;
    varPtr->tracePtr = NULL;
    varPtr->searchPtr = NULL;
    varPtr->flags = (VAR_SCALAR | VAR_UNDEFINED | VAR_IN_HASHTABLE);
    return varPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseSearchId --
 *
 *	This procedure translates from a string to a pointer to an
 *	active array search (if there is one that matches the string).
 *
 * Results:
 *	The return value is a pointer to the array search indicated
 *	by string, or NULL if there isn't one.  If NULL is returned,
 *	the interp's result contains an error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static ArraySearch *
ParseSearchId(interp, varPtr, varName, string)
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    Var *varPtr;		/* Array variable search is for. */
    char *varName;		/* Name of array variable that search is
				 * supposed to be for. */
    char *string;		/* String containing id of search. Must have
				 * form "search-num-var" where "num" is a
				 * decimal number and "var" is a variable
				 * name. */
{
    char *end;
    int id;
    ArraySearch *searchPtr;

    /*
     * Parse the id into the three parts separated by dashes.
     */

    if ((string[0] != 's') || (string[1] != '-')) {
	syntax:
	Tcl_AppendResult(interp, "illegal search identifier \"", string,
		"\"", (char *) NULL);
	return NULL;
    }
    id = strtoul(string+2, &end, 10);
    if ((end == (string+2)) || (*end != '-')) {
	goto syntax;
    }
    if (strcmp(end+1, varName) != 0) {
	Tcl_AppendResult(interp, "search identifier \"", string,
		"\" isn't for variable \"", varName, "\"", (char *) NULL);
	return NULL;
    }

    /*
     * Search through the list of active searches on the interpreter
     * to see if the desired one exists.
     */

    for (searchPtr = varPtr->searchPtr; searchPtr != NULL;
	 searchPtr = searchPtr->nextPtr) {
	if (searchPtr->id == id) {
	    return searchPtr;
	}
    }
    Tcl_AppendResult(interp, "couldn't find search \"", string, "\"",
	    (char *) NULL);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteSearches --
 *
 *	This procedure is called to free up all of the searches
 *	associated with an array variable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is released to the storage allocator.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteSearches(arrayVarPtr)
    register Var *arrayVarPtr;		/* Variable whose searches are
					 * to be deleted. */
{
    ArraySearch *searchPtr;

    while (arrayVarPtr->searchPtr != NULL) {
	searchPtr = arrayVarPtr->searchPtr;
	arrayVarPtr->searchPtr = searchPtr->nextPtr;
	ckfree((char *) searchPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclDeleteVars --
 *
 *	This procedure is called to recycle all the storage space
 *	associated with a table of variables. For this procedure
 *	to work correctly, it must not be possible for any of the
 *	variables in the table to be accessed from Tcl commands
 *	(e.g. from trace procedures).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Variables are deleted and trace procedures are invoked, if
 *	any are declared.
 *
 *----------------------------------------------------------------------
 */

void
TclDeleteVars(iPtr, tablePtr)
    Interp *iPtr;		/* Interpreter to which variables belong. */
    Tcl_HashTable *tablePtr;	/* Hash table containing variables to
				 * delete. */
{
    Tcl_Interp *interp = (Tcl_Interp *) iPtr;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    register Var *varPtr;
    Var *linkPtr;
    int flags;
    ActiveVarTrace *activePtr;
    Tcl_Obj *objPtr;
    Namespace *currNsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);

    /*
     * Determine what flags to pass to the trace callback procedures.
     */

    flags = TCL_TRACE_UNSETS;
    if (tablePtr == &iPtr->globalNsPtr->varTable) {
	flags |= (TCL_INTERP_DESTROYED | TCL_GLOBAL_ONLY);
    } else if (tablePtr == &currNsPtr->varTable) {
	flags |= TCL_NAMESPACE_ONLY;
    }

    for (hPtr = Tcl_FirstHashEntry(tablePtr, &search);  hPtr != NULL;
	 hPtr = Tcl_NextHashEntry(&search)) {
	varPtr = (Var *) Tcl_GetHashValue(hPtr);

	/*
	 * For global/upvar variables referenced in procedures, decrement
	 * the reference count on the variable referred to, and free
	 * the referenced variable if it's no longer needed. Don't delete
	 * the hash entry for the other variable if it's in the same table
	 * as us: this will happen automatically later on.
	 */

	if (TclIsVarLink(varPtr)) {
	    linkPtr = varPtr->value.linkPtr;
	    linkPtr->refCount--;
	    if ((linkPtr->refCount == 0) && TclIsVarUndefined(linkPtr)
		    && (linkPtr->tracePtr == NULL)
		    && (linkPtr->flags & VAR_IN_HASHTABLE)) {
		if (linkPtr->hPtr == NULL) {
		    ckfree((char *) linkPtr);
		} else if (linkPtr->hPtr->tablePtr != tablePtr) {
		    Tcl_DeleteHashEntry(linkPtr->hPtr);
		    ckfree((char *) linkPtr);
		}
	    }
	}

	/*
	 * Invoke traces on the variable that is being deleted, then
	 * free up the variable's space (no need to free the hash entry
	 * here, unless we're dealing with a global variable: the
	 * hash entries will be deleted automatically when the whole
	 * table is deleted). Note that we give CallTraces the variable's
	 * fully-qualified name so that any called trace procedures can
	 * refer to these variables being deleted.
	 */

	if (varPtr->tracePtr != NULL) {
	    objPtr = Tcl_NewObj();
	    Tcl_IncrRefCount(objPtr); /* until done with traces */
	    Tcl_GetVariableFullName(interp, (Tcl_Var) varPtr, objPtr);
	    (void) CallTraces(iPtr, (Var *) NULL, varPtr,
		    Tcl_GetString(objPtr), (char *) NULL, flags);
	    Tcl_DecrRefCount(objPtr); /* free no longer needed obj */

	    while (varPtr->tracePtr != NULL) {
		VarTrace *tracePtr = varPtr->tracePtr;
		varPtr->tracePtr = tracePtr->nextPtr;
		ckfree((char *) tracePtr);
	    }
	    for (activePtr = iPtr->activeTracePtr; activePtr != NULL;
		 activePtr = activePtr->nextPtr) {
		if (activePtr->varPtr == varPtr) {
		    activePtr->nextTracePtr = NULL;
		}
	    }
	}
	    
	if (TclIsVarArray(varPtr)) {
	    DeleteArray(iPtr, Tcl_GetHashKey(tablePtr, hPtr), varPtr,
	            flags);
	    varPtr->value.tablePtr = NULL;
	}
	if (TclIsVarScalar(varPtr) && (varPtr->value.objPtr != NULL)) {
	    objPtr = varPtr->value.objPtr;
	    TclDecrRefCount(objPtr);
	    varPtr->value.objPtr = NULL;
	}
	varPtr->hPtr = NULL;
	varPtr->tracePtr = NULL;
	TclSetVarUndefined(varPtr);
	TclSetVarScalar(varPtr);

	/*
	 * If the variable was a namespace variable, decrement its 
	 * reference count. We are in the process of destroying its
	 * namespace so that namespace will no longer "refer" to the
	 * variable.
	 */

	if (varPtr->flags & VAR_NAMESPACE_VAR) {
	    varPtr->flags &= ~VAR_NAMESPACE_VAR;
	    varPtr->refCount--;
	}

	/*
	 * Recycle the variable's memory space if there aren't any upvar's
	 * pointing to it. If there are upvars to this variable, then the
	 * variable will get freed when the last upvar goes away.
	 */

	if (varPtr->refCount == 0) {
	    ckfree((char *) varPtr); /* this Var must be VAR_IN_HASHTABLE */
	}
    }
    Tcl_DeleteHashTable(tablePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclDeleteCompiledLocalVars --
 *
 *	This procedure is called to recycle storage space associated with
 *	the compiler-allocated array of local variables in a procedure call
 *	frame. This procedure resembles TclDeleteVars above except that each
 *	variable is stored in a call frame and not a hash table. For this
 *	procedure to work correctly, it must not be possible for any of the
 *	variable in the table to be accessed from Tcl commands (e.g. from
 *	trace procedures).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Variables are deleted and trace procedures are invoked, if
 *	any are declared.
 *
 *----------------------------------------------------------------------
 */

void
TclDeleteCompiledLocalVars(iPtr, framePtr)
    Interp *iPtr;		/* Interpreter to which variables belong. */
    CallFrame *framePtr;	/* Procedure call frame containing
				 * compiler-assigned local variables to
				 * delete. */
{
    register Var *varPtr;
    int flags;			/* Flags passed to trace procedures. */
    Var *linkPtr;
    ActiveVarTrace *activePtr;
    int numLocals, i;

    flags = TCL_TRACE_UNSETS;
    numLocals = framePtr->numCompiledLocals;
    varPtr = framePtr->compiledLocals;
    for (i = 0;  i < numLocals;  i++) {
	/*
	 * For global/upvar variables referenced in procedures, decrement
	 * the reference count on the variable referred to, and free
	 * the referenced variable if it's no longer needed. Don't delete
	 * the hash entry for the other variable if it's in the same table
	 * as us: this will happen automatically later on.
	 */

	if (TclIsVarLink(varPtr)) {
	    linkPtr = varPtr->value.linkPtr;
	    linkPtr->refCount--;
	    if ((linkPtr->refCount == 0) && TclIsVarUndefined(linkPtr)
		    && (linkPtr->tracePtr == NULL)
		    && (linkPtr->flags & VAR_IN_HASHTABLE)) {
		if (linkPtr->hPtr == NULL) {
		    ckfree((char *) linkPtr);
		} else {
		    Tcl_DeleteHashEntry(linkPtr->hPtr);
		    ckfree((char *) linkPtr);
		}
	    }
	}

	/*
	 * Invoke traces on the variable that is being deleted. Then delete
	 * the variable's trace records.
	 */

	if (varPtr->tracePtr != NULL) {
	    (void) CallTraces(iPtr, (Var *) NULL, varPtr,
		    varPtr->name, (char *) NULL, flags);
	    while (varPtr->tracePtr != NULL) {
		VarTrace *tracePtr = varPtr->tracePtr;
		varPtr->tracePtr = tracePtr->nextPtr;
		ckfree((char *) tracePtr);
	    }
	    for (activePtr = iPtr->activeTracePtr; activePtr != NULL;
		 activePtr = activePtr->nextPtr) {
		if (activePtr->varPtr == varPtr) {
		    activePtr->nextTracePtr = NULL;
		}
	    }
	}

        /*
	 * Now if the variable is an array, delete its element hash table.
	 * Otherwise, if it's a scalar variable, decrement the ref count
	 * of its value.
	 */
	    
	if (TclIsVarArray(varPtr) && (varPtr->value.tablePtr != NULL)) {
	    DeleteArray(iPtr, varPtr->name, varPtr, flags);
	}
	if (TclIsVarScalar(varPtr) && (varPtr->value.objPtr != NULL)) {
	    TclDecrRefCount(varPtr->value.objPtr);
	    varPtr->value.objPtr = NULL;
	}
	varPtr->hPtr = NULL;
	varPtr->tracePtr = NULL;
	TclSetVarUndefined(varPtr);
	TclSetVarScalar(varPtr);
	varPtr++;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteArray --
 *
 *	This procedure is called to free up everything in an array
 *	variable.  It's the caller's responsibility to make sure
 *	that the array is no longer accessible before this procedure
 *	is called.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All storage associated with varPtr's array elements is deleted
 *	(including the array's hash table). Deletion trace procedures for
 *	array elements are invoked, then deleted. Any pending traces for
 *	array elements are also deleted.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteArray(iPtr, arrayName, varPtr, flags)
    Interp *iPtr;			/* Interpreter containing array. */
    char *arrayName;			/* Name of array (used for trace
					 * callbacks). */
    Var *varPtr;			/* Pointer to variable structure. */
    int flags;				/* Flags to pass to CallTraces:
					 * TCL_TRACE_UNSETS and sometimes
					 * TCL_INTERP_DESTROYED,
					 * TCL_NAMESPACE_ONLY, or
					 * TCL_GLOBAL_ONLY. */
{
    Tcl_HashSearch search;
    register Tcl_HashEntry *hPtr;
    register Var *elPtr;
    ActiveVarTrace *activePtr;
    Tcl_Obj *objPtr;

    DeleteSearches(varPtr);
    for (hPtr = Tcl_FirstHashEntry(varPtr->value.tablePtr, &search);
	 hPtr != NULL;  hPtr = Tcl_NextHashEntry(&search)) {
	elPtr = (Var *) Tcl_GetHashValue(hPtr);
	if (TclIsVarScalar(elPtr) && (elPtr->value.objPtr != NULL)) {
	    objPtr = elPtr->value.objPtr;
	    TclDecrRefCount(objPtr);
	    elPtr->value.objPtr = NULL;
	}
	elPtr->hPtr = NULL;
	if (elPtr->tracePtr != NULL) {
	    elPtr->flags &= ~VAR_TRACE_ACTIVE;
	    (void) CallTraces(iPtr, (Var *) NULL, elPtr, arrayName,
		    Tcl_GetHashKey(varPtr->value.tablePtr, hPtr), flags);
	    while (elPtr->tracePtr != NULL) {
		VarTrace *tracePtr = elPtr->tracePtr;
		elPtr->tracePtr = tracePtr->nextPtr;
		ckfree((char *) tracePtr);
	    }
	    for (activePtr = iPtr->activeTracePtr; activePtr != NULL;
		 activePtr = activePtr->nextPtr) {
		if (activePtr->varPtr == elPtr) {
		    activePtr->nextTracePtr = NULL;
		}
	    }
	}
	TclSetVarUndefined(elPtr);
	TclSetVarScalar(elPtr);
	if (elPtr->refCount == 0) {
	    ckfree((char *) elPtr); /* element Vars are VAR_IN_HASHTABLE */
	}
    }
    Tcl_DeleteHashTable(varPtr->value.tablePtr);
    ckfree((char *) varPtr->value.tablePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CleanupVar --
 *
 *	This procedure is called when it looks like it may be OK to free up
 *	a variable's storage. If the variable is in a hashtable, its Var
 *	structure and hash table entry will be freed along with those of its
 *	containing array, if any. This procedure is called, for example,
 *	when a trace on a variable deletes a variable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the variable (or its containing array) really is dead and in a
 *	hashtable, then its Var structure, and possibly its hash table
 *	entry, is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
CleanupVar(varPtr, arrayPtr)
    Var *varPtr;		/* Pointer to variable that may be a
				 * candidate for being expunged. */
    Var *arrayPtr;		/* Array that contains the variable, or
				 * NULL if this variable isn't an array
				 * element. */
{
    if (TclIsVarUndefined(varPtr) && (varPtr->refCount == 0)
	    && (varPtr->tracePtr == NULL)
	    && (varPtr->flags & VAR_IN_HASHTABLE)) {
	if (varPtr->hPtr != NULL) {
	    Tcl_DeleteHashEntry(varPtr->hPtr);
	}
	ckfree((char *) varPtr);
    }
    if (arrayPtr != NULL) {
	if (TclIsVarUndefined(arrayPtr) && (arrayPtr->refCount == 0)
		&& (arrayPtr->tracePtr == NULL)
	        && (arrayPtr->flags & VAR_IN_HASHTABLE)) {
	    if (arrayPtr->hPtr != NULL) {
		Tcl_DeleteHashEntry(arrayPtr->hPtr);
	    }
	    ckfree((char *) arrayPtr);
	}
    }
}
/*
 *----------------------------------------------------------------------
 *
 * VarErrMsg --
 *
 *      Generate a reasonable error message describing why a variable
 *      operation failed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The interp's result is set to hold a message identifying the
 *      variable given by part1 and part2 and describing why the
 *      variable operation failed.
 *
 *----------------------------------------------------------------------
 */

static void
VarErrMsg(interp, part1, part2, operation, reason)
    Tcl_Interp *interp;         /* Interpreter in which to record message. */
    char *part1, *part2;        /* Variable's two-part name. */
    char *operation;            /* String describing operation that failed,
                                 * e.g. "read", "set", or "unset". */
    char *reason;               /* String describing why operation failed. */
{
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "can't ", operation, " \"", part1,
	    (char *) NULL);
    if (part2 != NULL) {
        Tcl_AppendResult(interp, "(", part2, ")", (char *) NULL);
    }
    Tcl_AppendResult(interp, "\": ", reason, (char *) NULL);
}


/*
 *----------------------------------------------------------------------
 *
 * TclTraceVarExists --
 *
 *	This is called from info exists.  We need to trigger read
 *	and/or array traces because they may end up creating a
 *	variable that doesn't currently exist.
 *
 * Results:
 *	A pointer to the Var structure, or NULL.
 *
 * Side effects:
 *	May fill in error messages in the interp.
 *
 *----------------------------------------------------------------------
 */

Var *
TclVarTraceExists(interp, varName)
    Tcl_Interp *interp;		/* The interpreter */
    char *varName;		/* The variable name */
{
    Var *varPtr;
    Var *arrayPtr;
    char *msg;

    /*
     * The choice of "create" flag values is delicate here, and
     * matches the semantics of GetVar.  Things are still not perfect,
     * however, because if you do "info exists x" you get a varPtr
     * and therefore trigger traces.  However, if you do 
     * "info exists x(i)", then you only get a varPtr if x is already
     * known to be an array.  Otherwise you get NULL, and no trace
     * is triggered.  This matches Tcl 7.6 semantics.
     */

    varPtr = TclLookupVar(interp, varName, (char *) NULL,
            0, "access",
            /*createPart1*/ 0, /*createPart2*/ 1, &arrayPtr);
    if (varPtr == NULL) {
	return NULL;
    }
    if ((varPtr != NULL) &&
	    ((varPtr->tracePtr != NULL)
	    || ((arrayPtr != NULL) && (arrayPtr->tracePtr != NULL)))) {
	msg = CallTraces((Interp *)interp, arrayPtr, varPtr, varName,
		(char *) NULL, TCL_TRACE_READS);
	if (msg != NULL) {
	    /*
	     * If the variable doesn't exist anymore and no-one's using
	     * it, then free up the relevant structures and hash table entries.
	     */

	    if (TclIsVarUndefined(varPtr)) {
		CleanupVar(varPtr, arrayPtr);
	    }
	    return NULL;
	}
    }
    return varPtr;
}

