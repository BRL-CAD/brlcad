/*
 * tclCmdIL.c --
 *
 *	This file contains the top-level command routines for most of the Tcl
 *	built-in commands whose names begin with the letters I through L. It
 *	contains only commands in the generic core (i.e. those that don't
 *	depend much upon UNIX facilities).
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1993-1997 Lucent Technologies.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * Copyright (c) 2001 by Kevin B. Kenny.  All rights reserved.
 * Copyright (c) 2005 Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclInt.h"
#include "tclRegexp.h"

/*
 * During execution of the "lsort" command, structures of the following type
 * are used to arrange the objects being sorted into a collection of linked
 * lists.
 */

typedef struct SortElement {
    Tcl_Obj *objPtr;		/* Object being sorted. */
    int count;			/* number of same elements in list */
    struct SortElement *nextPtr;/* Next element in the list, or NULL for end
				 * of list. */
} SortElement;

/*
 * These function pointer types are used with the "lsearch" and "lsort"
 * commands to facilitate the "-nocase" option.
 */

typedef int (*SortStrCmpFn_t) (const char *, const char *);
typedef int (*SortMemCmpFn_t) (const void *, const void *, size_t);

/*
 * The "lsort" command needs to pass certain information down to the function
 * that compares two list elements, and the comparison function needs to pass
 * success or failure information back up to the top-level "lsort" command.
 * The following structure is used to pass this information.
 */

typedef struct SortInfo {
    int isIncreasing;		/* Nonzero means sort in increasing order. */
    int sortMode;		/* The sort mode. One of SORTMODE_* values
				 * defined below */
    SortStrCmpFn_t strCmpFn;	/* Basic string compare command (used with
				 * ASCII mode). */
    Tcl_Obj *compareCmdPtr;	/* The Tcl comparison command when sortMode is
				 * SORTMODE_COMMAND. Pre-initialized to hold
				 * base of command.*/
    int *indexv;		/* If the -index option was specified, this
				 * holds the indexes contained in the list
				 * supplied as an argument to that option.
				 * NULL if no indexes supplied, and points to
				 * singleIndex field when only one
				 * supplied. */
    int indexc;			/* Number of indexes in indexv array. */
    int singleIndex;		/* Static space for common index case. */
    Tcl_Interp *interp;		/* The interpreter in which the sort is being
				 * done. */
    int resultCode;		/* Completion code for the lsort command. If
				 * an error occurs during the sort this is
				 * changed from TCL_OK to TCL_ERROR. */
} SortInfo;

/*
 * The "sortMode" field of the SortInfo structure can take on any of the
 * following values.
 */

#define SORTMODE_ASCII		0
#define SORTMODE_INTEGER	1
#define SORTMODE_REAL		2
#define SORTMODE_COMMAND	3
#define SORTMODE_DICTIONARY	4

/*
 * Magic values for the index field of the SortInfo structure. Note that the
 * index "end-1" will be translated to SORTIDX_END-1, etc.
 */

#define SORTIDX_NONE	-1	/* Not indexed; use whole value. */
#define SORTIDX_END	-2	/* Indexed from end. */

/*
 * Forward declarations for procedures defined in this file:
 */

static void		AppendLocals(Tcl_Interp *interp, Tcl_Obj *listPtr,
			    CONST char *pattern, int includeLinks);
static int		DictionaryCompare(char *left, char *right);
static int		InfoArgsCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoBodyCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoCmdCountCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoCommandsCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoCompleteCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoDefaultCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoExistsCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoFunctionsCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoGlobalsCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoHostnameCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoLevelCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoLibraryCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoLoadedCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoLocalsCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoNameOfExecutableCmd(ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]);
static int		InfoPatchLevelCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoProcsCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoScriptCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoSharedlibCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoTclVersionCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static int		InfoVarsCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]);
static SortElement *    MergeSort(SortElement *headPt, SortInfo *infoPtr);
static SortElement *    MergeLists(SortElement *leftPtr, SortElement *rightPtr,
			    SortInfo *infoPtr);
static int		SortCompare(Tcl_Obj *firstPtr, Tcl_Obj *second,
			    SortInfo *infoPtr);
static Tcl_Obj *	SelectObjFromSublist(Tcl_Obj *firstPtr,
			    SortInfo *infoPtr);

/*
 *----------------------------------------------------------------------
 *
 * Tcl_IfObjCmd --
 *
 *	This procedure is invoked to process the "if" Tcl command. See the
 *	user documentation for details on what it does.
 *
 *	With the bytecode compiler, this procedure is only called when a
 *	command name is computed at runtime, and is "if" or the name to which
 *	"if" was renamed: e.g., "set z if; $z 1 {puts foo}"
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_IfObjCmd(dummy, interp, objc, objv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    int thenScriptIndex = 0;		/* "then" script to be evaled after
					 * syntax check */
    int i, result, value;
    char *clause;
    i = 1;
    while (1) {
	/*
	 * At this point in the loop, objv and objc refer to an expression to
	 * test, either for the main expression or an expression following an
	 * "elseif". The arguments after the expression must be "then"
	 * (optional) and a script to execute if the expression is true.
	 */

	if (i >= objc) {
	    clause = TclGetString(objv[i-1]);
	    Tcl_AppendResult(interp, "wrong # args: no expression after \"",
		    clause, "\" argument", (char *) NULL);
	    return TCL_ERROR;
	}
	if (!thenScriptIndex) {
	    result = Tcl_ExprBooleanObj(interp, objv[i], &value);
	    if (result != TCL_OK) {
		return result;
	    }
	}
	i++;
	if (i >= objc) {
	    missingScript:
	    clause = TclGetString(objv[i-1]);
	    Tcl_AppendResult(interp, "wrong # args: no script following \"",
		    clause, "\" argument", (char *) NULL);
	    return TCL_ERROR;
	}
	clause = TclGetString(objv[i]);
	if ((i < objc) && (strcmp(clause, "then") == 0)) {
	    i++;
	}
	if (i >= objc) {
	    goto missingScript;
	}
	if (value) {
	    thenScriptIndex = i;
	    value = 0;
	}

	/*
	 * The expression evaluated to false. Skip the command, then see if
	 * there is an "else" or "elseif" clause.
	 */

	i++;
	if (i >= objc) {
	    if (thenScriptIndex) {
		return Tcl_EvalObjEx(interp, objv[thenScriptIndex], 0);
	    }
	    return TCL_OK;
	}
	clause = TclGetString(objv[i]);
	if ((clause[0] == 'e') && (strcmp(clause, "elseif") == 0)) {
	    i++;
	    continue;
	}
	break;
    }

    /*
     * Couldn't find a "then" or "elseif" clause to execute. Check now for an
     * "else" clause. We know that there's at least one more argument when we
     * get here.
     */

    if (strcmp(clause, "else") == 0) {
	i++;
	if (i >= objc) {
	    Tcl_AppendResult(interp,
		    "wrong # args: no script following \"else\" argument",
		    (char *) NULL);
	    return TCL_ERROR;
	}
    }
    if (i < objc - 1) {
	Tcl_AppendResult(interp,
		"wrong # args: extra words after \"else\" clause in \"if\" command",
		(char *) NULL);
	return TCL_ERROR;
    }
    if (thenScriptIndex) {
	return Tcl_EvalObjEx(interp, objv[thenScriptIndex], 0);
    }
    return Tcl_EvalObjEx(interp, objv[i], 0);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_IncrObjCmd --
 *
 *	This procedure is invoked to process the "incr" Tcl command. See the
 *	user documentation for details on what it does.
 *
 *	With the bytecode compiler, this procedure is only called when a
 *	command name is computed at runtime, and is "incr" or the name to
 *	which "incr" was renamed: e.g., "set z incr; $z i -1"
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
int
Tcl_IncrObjCmd(dummy, interp, objc, objv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument objects. */
{
    Tcl_Obj *newValuePtr, *incrPtr;

    if ((objc != 2) && (objc != 3)) {
	Tcl_WrongNumArgs(interp, 1, objv, "varName ?increment?");
	return TCL_ERROR;
    }

    if (objc == 3) {
	incrPtr = objv[2];
    } else {
	incrPtr = Tcl_NewIntObj(1);
    }
    Tcl_IncrRefCount(incrPtr);
    newValuePtr = TclIncrObjVar2(interp, objv[1], NULL,
	    incrPtr, TCL_LEAVE_ERR_MSG);
    Tcl_DecrRefCount(incrPtr);

    if (newValuePtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Set the interpreter's object result to refer to the variable's new
     * value object.
     */

    Tcl_SetObjResult(interp, newValuePtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InfoObjCmd --
 *
 *	This procedure is invoked to process the "info" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_InfoObjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Arbitrary value passed to the command. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    static CONST char *subCmds[] = {
	    "args", "body", "cmdcount", "commands",
	    "complete", "default", "exists", "functions", "globals",
	    "hostname", "level", "library", "loaded",
	    "locals", "nameofexecutable", "patchlevel", "procs",
	    "script", "sharedlibextension", "tclversion", "vars",
	    (char *) NULL};
    enum ISubCmdIdx {
	    IArgsIdx, IBodyIdx, ICmdCountIdx, ICommandsIdx,
	    ICompleteIdx, IDefaultIdx, IExistsIdx, IFunctionsIdx, IGlobalsIdx,
	    IHostnameIdx, ILevelIdx, ILibraryIdx, ILoadedIdx,
	    ILocalsIdx, INameOfExecutableIdx, IPatchLevelIdx, IProcsIdx,
	    IScriptIdx, ISharedLibExtensionIdx, ITclVersionIdx, IVarsIdx
    };
    int index, result;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[1], subCmds, "option", 0,
	    (int *) &index);
    if (result != TCL_OK) {
	return result;
    }

    switch (index) {
	case IArgsIdx:
	    result = InfoArgsCmd(clientData, interp, objc, objv);
	    break;
	case IBodyIdx:
	    result = InfoBodyCmd(clientData, interp, objc, objv);
	    break;
	case ICmdCountIdx:
	    result = InfoCmdCountCmd(clientData, interp, objc, objv);
	    break;
	case ICommandsIdx:
	    result = InfoCommandsCmd(clientData, interp, objc, objv);
	    break;
	case ICompleteIdx:
	    result = InfoCompleteCmd(clientData, interp, objc, objv);
	    break;
	case IDefaultIdx:
	    result = InfoDefaultCmd(clientData, interp, objc, objv);
	    break;
	case IExistsIdx:
	    result = InfoExistsCmd(clientData, interp, objc, objv);
	    break;
	case IFunctionsIdx:
	    result = InfoFunctionsCmd(clientData, interp, objc, objv);
	    break;
	case IGlobalsIdx:
	    result = InfoGlobalsCmd(clientData, interp, objc, objv);
	    break;
	case IHostnameIdx:
	    result = InfoHostnameCmd(clientData, interp, objc, objv);
	    break;
	case ILevelIdx:
	    result = InfoLevelCmd(clientData, interp, objc, objv);
	    break;
	case ILibraryIdx:
	    result = InfoLibraryCmd(clientData, interp, objc, objv);
	    break;
	case ILoadedIdx:
	    result = InfoLoadedCmd(clientData, interp, objc, objv);
	    break;
	case ILocalsIdx:
	    result = InfoLocalsCmd(clientData, interp, objc, objv);
	    break;
	case INameOfExecutableIdx:
	    result = InfoNameOfExecutableCmd(clientData, interp, objc, objv);
	    break;
	case IPatchLevelIdx:
	    result = InfoPatchLevelCmd(clientData, interp, objc, objv);
	    break;
	case IProcsIdx:
	    result = InfoProcsCmd(clientData, interp, objc, objv);
	    break;
	case IScriptIdx:
	    result = InfoScriptCmd(clientData, interp, objc, objv);
	    break;
	case ISharedLibExtensionIdx:
	    result = InfoSharedlibCmd(clientData, interp, objc, objv);
	    break;
	case ITclVersionIdx:
	    result = InfoTclVersionCmd(clientData, interp, objc, objv);
	    break;
	case IVarsIdx:
	    result = InfoVarsCmd(clientData, interp, objc, objv);
	    break;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoArgsCmd --
 *
 *	Called to implement the "info args" command that returns the argument
 *	list for a procedure. Handles the following syntax:
 *
 *	    info args procName
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoArgsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Interp *iPtr = (Interp *) interp;
    char *name;
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *listObjPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "procname");
	return TCL_ERROR;
    }

    name = TclGetString(objv[2]);
    procPtr = TclFindProc(iPtr, name);
    if (procPtr == NULL) {
	Tcl_AppendResult(interp, "\"", name,
		"\" isn't a procedure", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Build a return list containing the arguments.
     */

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (localPtr = procPtr->firstLocalPtr;  localPtr != NULL;
	    localPtr = localPtr->nextPtr) {
	if (TclIsVarArgument(localPtr)) {
	    Tcl_ListObjAppendElement(interp, listObjPtr,
		    Tcl_NewStringObj(localPtr->name, -1));
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoBodyCmd --
 *
 *	Called to implement the "info body" command that returns the body for
 *	a procedure. Handles the following syntax:
 *
 *	    info body procName
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoBodyCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Interp *iPtr = (Interp *) interp;
    char *name;
    Proc *procPtr;
    Tcl_Obj *bodyPtr, *resultPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "procname");
	return TCL_ERROR;
    }

    name = TclGetString(objv[2]);
    procPtr = TclFindProc(iPtr, name);
    if (procPtr == NULL) {
	Tcl_AppendResult(interp, "\"", name,
		"\" isn't a procedure", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Here we used to return procPtr->bodyPtr, except when the body was
     * bytecompiled - in that case, the return was a copy of the body's string
     * rep. In order to better isolate the implementation details of the
     * compiler/engine subsystem, we now always return a copy of the string
     * rep. It is important to return a copy so that later manipulations of
     * the object do not invalidate the internal rep.
     */

    bodyPtr = procPtr->bodyPtr;
    if (bodyPtr->bytes == NULL) {
	/*
	 * The string rep might not be valid if the procedure has never been
	 * run before. [Bug #545644]
	 */

	(void) Tcl_GetString(bodyPtr);
    }
    resultPtr = Tcl_NewStringObj(bodyPtr->bytes, bodyPtr->length);

    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoCmdCountCmd --
 *
 *	Called to implement the "info cmdcount" command that returns the
 *	number of commands that have been executed. Handles the following
 *	syntax:
 *
 *	    info cmdcount
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoCmdCountCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 2, objv, NULL);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewIntObj(iPtr->cmdCount));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoCommandsCmd --
 *
 *	Called to implement the "info commands" command that returns the list
 *	of commands in the interpreter that match an optional pattern. The
 *	pattern, if any, consists of an optional sequence of namespace names
 *	separated by "::" qualifiers, which is followed by a glob-style
 *	pattern that restricts which commands are returned. Handles the
 *	following syntax:
 *
 *	    info commands ?pattern?
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoCommandsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *cmdName, *pattern;
    CONST char *simplePattern;
    register Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Namespace *nsPtr;
    Namespace *globalNsPtr = (Namespace *) Tcl_GetGlobalNamespace(interp);
    Namespace *currNsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    Tcl_Obj *listPtr, *elemObjPtr;
    int specificNsInPattern = 0;/* Init. to avoid compiler warning. */
    Tcl_Command cmd;
    int i;

    /*
     * Get the pattern and find the "effective namespace" in which to list
     * commands.
     */

    if (objc == 2) {
	simplePattern = NULL;
	nsPtr = currNsPtr;
	specificNsInPattern = 0;
    } else if (objc == 3) {
	/*
	 * From the pattern, get the effective namespace and the simple
	 * pattern (no namespace qualifiers or ::'s) at the end. If an error
	 * was found while parsing the pattern, return it. Otherwise, if the
	 * namespace wasn't found, just leave nsPtr NULL: we will return an
	 * empty list since no commands there can be found.
	 */

	Namespace *dummy1NsPtr, *dummy2NsPtr;

	pattern = TclGetString(objv[2]);
	TclGetNamespaceForQualName(interp, pattern, (Namespace *) NULL, 0,
		&nsPtr, &dummy1NsPtr, &dummy2NsPtr, &simplePattern);

	if (nsPtr != NULL) {	/* we successfully found the pattern's ns */
	    specificNsInPattern = (strcmp(simplePattern, pattern) != 0);
	}
    } else {
	Tcl_WrongNumArgs(interp, 2, objv, "?pattern?");
	return TCL_ERROR;
    }

    /*
     * Exit as quickly as possible if we couldn't find the namespace.
     */

    if (nsPtr == NULL) {
	return TCL_OK;
    }

    /*
     * Scan through the effective namespace's command table and create a list
     * with all commands that match the pattern. If a specific namespace was
     * requested in the pattern, qualify the command names with the namespace
     * name.
     */

    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);

    if (simplePattern != NULL && TclMatchIsTrivial(simplePattern)) {
	/*
	 * Special case for when the pattern doesn't include any of glob's
	 * special characters. This lets us avoid scans of any hash tables.
	 */

	entryPtr = Tcl_FindHashEntry(&nsPtr->cmdTable, simplePattern);
	if (entryPtr != NULL) {
	    if (specificNsInPattern) {
		cmd = (Tcl_Command) Tcl_GetHashValue(entryPtr);
		elemObjPtr = Tcl_NewObj();
		Tcl_GetCommandFullName(interp, cmd, elemObjPtr);
	    } else {
		cmdName = Tcl_GetHashKey(&nsPtr->cmdTable, entryPtr);
		elemObjPtr = Tcl_NewStringObj(cmdName, -1);
	    }
	    Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	    Tcl_SetObjResult(interp, listPtr);
	    return TCL_OK;
	}
	if ((nsPtr != globalNsPtr) && !specificNsInPattern) {
	    Tcl_HashTable *tablePtr = NULL;	/* Quell warning */

	    for (i=0 ; i<nsPtr->commandPathLength ; i++) {
		Namespace *pathNsPtr = nsPtr->commandPathArray[i].nsPtr;

		if (pathNsPtr == NULL) {
		    continue;
		}
		tablePtr = &pathNsPtr->cmdTable;
		entryPtr = Tcl_FindHashEntry(tablePtr, simplePattern);
		if (entryPtr != NULL) {
		    break;
		}
	    }
	    if (entryPtr == NULL) {
		tablePtr = &globalNsPtr->cmdTable;
		entryPtr = Tcl_FindHashEntry(tablePtr, simplePattern);
	    }
	    if (entryPtr != NULL) {
		cmdName = Tcl_GetHashKey(tablePtr, entryPtr);
		Tcl_ListObjAppendElement(interp, listPtr,
			Tcl_NewStringObj(cmdName, -1));
		Tcl_SetObjResult(interp, listPtr);
		return TCL_OK;
	    }
	}
    } else if (nsPtr->commandPathLength == 0 || specificNsInPattern) {
	/*
	 * The pattern is non-trivial, but either there is no explicit path or
	 * there is an explicit namespace in the pattern. In both cases, the
	 * old matching scheme is perfect.
	 */

	entryPtr = Tcl_FirstHashEntry(&nsPtr->cmdTable, &search);
	while (entryPtr != NULL) {
	    cmdName = Tcl_GetHashKey(&nsPtr->cmdTable, entryPtr);
	    if ((simplePattern == NULL)
		    || Tcl_StringMatch(cmdName, simplePattern)) {
		if (specificNsInPattern) {
		    cmd = (Tcl_Command) Tcl_GetHashValue(entryPtr);
		    elemObjPtr = Tcl_NewObj();
		    Tcl_GetCommandFullName(interp, cmd, elemObjPtr);
		} else {
		    elemObjPtr = Tcl_NewStringObj(cmdName, -1);
		}
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	    }
	    entryPtr = Tcl_NextHashEntry(&search);
	}

	/*
	 * If the effective namespace isn't the global :: namespace, and a
	 * specific namespace wasn't requested in the pattern, then add in all
	 * global :: commands that match the simple pattern. Of course, we add
	 * in only those commands that aren't hidden by a command in the
	 * effective namespace.
	 */

	if ((nsPtr != globalNsPtr) && !specificNsInPattern) {
	    entryPtr = Tcl_FirstHashEntry(&globalNsPtr->cmdTable, &search);
	    while (entryPtr != NULL) {
		cmdName = Tcl_GetHashKey(&globalNsPtr->cmdTable, entryPtr);
		if ((simplePattern == NULL)
			|| Tcl_StringMatch(cmdName, simplePattern)) {
		    if (Tcl_FindHashEntry(&nsPtr->cmdTable, cmdName) == NULL) {
			Tcl_ListObjAppendElement(interp, listPtr,
				Tcl_NewStringObj(cmdName, -1));
		    }
		}
		entryPtr = Tcl_NextHashEntry(&search);
	    }
	}
    } else {
	/*
	 * The pattern is non-trivial (can match more than one command name),
	 * there is an explicit path, and there is no explicit namespace in
	 * the pattern. This means that we have to traverse the path to
	 * discover all the commands defined.
	 */

	Tcl_HashTable addedCommandsTable;
	int isNew;
	int foundGlobal = (nsPtr == globalNsPtr);

	/*
	 * We keep a hash of the objects already added to the result list.
	 */

	Tcl_InitObjHashTable(&addedCommandsTable);

	entryPtr = Tcl_FirstHashEntry(&nsPtr->cmdTable, &search);
	while (entryPtr != NULL) {
	    cmdName = Tcl_GetHashKey(&nsPtr->cmdTable, entryPtr);
	    if ((simplePattern == NULL)
		    || Tcl_StringMatch(cmdName, simplePattern)) {
		elemObjPtr = Tcl_NewStringObj(cmdName, -1);
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		(void) Tcl_CreateHashEntry(&addedCommandsTable,
			(char *)elemObjPtr, &isNew);
	    }
	    entryPtr = Tcl_NextHashEntry(&search);
	}

	/*
	 * Search the path next.
	 */

	for (i=0 ; i<nsPtr->commandPathLength ; i++) {
	    Namespace *pathNsPtr = nsPtr->commandPathArray[i].nsPtr;

	    if (pathNsPtr == NULL) {
		continue;
	    }
	    if (pathNsPtr == globalNsPtr) {
		foundGlobal = 1;
	    }
	    entryPtr = Tcl_FirstHashEntry(&pathNsPtr->cmdTable, &search);
	    while (entryPtr != NULL) {
		cmdName = Tcl_GetHashKey(&pathNsPtr->cmdTable, entryPtr);
		if ((simplePattern == NULL)
			|| Tcl_StringMatch(cmdName, simplePattern)) {
		    elemObjPtr = Tcl_NewStringObj(cmdName, -1);
		    (void) Tcl_CreateHashEntry(&addedCommandsTable,
			    (char *) elemObjPtr, &isNew);
		    if (isNew) {
			Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		    } else {
			TclDecrRefCount(elemObjPtr);
		    }
		}
		entryPtr = Tcl_NextHashEntry(&search);
	    }
	}

	/*
	 * If the effective namespace isn't the global :: namespace, and a
	 * specific namespace wasn't requested in the pattern, then add in all
	 * global :: commands that match the simple pattern. Of course, we add
	 * in only those commands that aren't hidden by a command in the
	 * effective namespace.
	 */

	if (!foundGlobal) {
	    entryPtr = Tcl_FirstHashEntry(&globalNsPtr->cmdTable, &search);
	    while (entryPtr != NULL) {
		cmdName = Tcl_GetHashKey(&globalNsPtr->cmdTable, entryPtr);
		if ((simplePattern == NULL)
			|| Tcl_StringMatch(cmdName, simplePattern)) {
		    elemObjPtr = Tcl_NewStringObj(cmdName, -1);
		    if (Tcl_FindHashEntry(&addedCommandsTable,
			    (char *) elemObjPtr) == NULL) {
			Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		    } else {
			TclDecrRefCount(elemObjPtr);
		    }
		}
		entryPtr = Tcl_NextHashEntry(&search);
	    }
	}

	Tcl_DeleteHashTable(&addedCommandsTable);
    }

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoCompleteCmd --
 *
 *	Called to implement the "info complete" command that determines
 *	whether a string is a complete Tcl command. Handles the following
 *	syntax:
 *
 *	    info complete command
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoCompleteCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command");
	return TCL_ERROR;
    }

    if (TclObjCommandComplete(objv[2])) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
    } else {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoDefaultCmd --
 *
 *	Called to implement the "info default" command that returns the
 *	default value for a procedure argument. Handles the following syntax:
 *
 *	    info default procName arg varName
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoDefaultCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    char *procName, *argName, *varName;
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *valueObjPtr;

    if (objc != 5) {
	Tcl_WrongNumArgs(interp, 2, objv, "procname arg varname");
	return TCL_ERROR;
    }

    procName = TclGetString(objv[2]);
    argName = TclGetString(objv[3]);

    procPtr = TclFindProc(iPtr, procName);
    if (procPtr == NULL) {
	Tcl_AppendResult(interp, "\"", procName,
		"\" isn't a procedure", (char *) NULL);
	return TCL_ERROR;
    }

    for (localPtr = procPtr->firstLocalPtr;  localPtr != NULL;
	    localPtr = localPtr->nextPtr) {
	if (TclIsVarArgument(localPtr)
		&& (strcmp(argName, localPtr->name) == 0)) {
	    if (localPtr->defValuePtr != NULL) {
		valueObjPtr = Tcl_ObjSetVar2(interp, objv[4], NULL,
			localPtr->defValuePtr, 0);
		if (valueObjPtr == NULL) {
		    defStoreError:
		    varName = TclGetString(objv[4]);
		    Tcl_AppendResult(interp,
			    "couldn't store default value in variable \"",
			    varName, "\"", (char *) NULL);
		    return TCL_ERROR;
		}
		Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
	    } else {
		Tcl_Obj *nullObjPtr = Tcl_NewObj();
		valueObjPtr = Tcl_ObjSetVar2(interp, objv[4], NULL,
			nullObjPtr, 0);
		if (valueObjPtr == NULL) {
		    goto defStoreError;
		}
		Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
	    }
	    return TCL_OK;
	}
    }

    Tcl_AppendResult(interp, "procedure \"", procName,
	    "\" doesn't have an argument \"", argName, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoExistsCmd --
 *
 *	Called to implement the "info exists" command that determines whether
 *	a variable exists. Handles the following syntax:
 *
 *	    info exists varName
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoExistsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *varName;
    Var *varPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "varName");
	return TCL_ERROR;
    }

    varName = TclGetString(objv[2]);
    varPtr = TclVarTraceExists(interp, varName);
    if ((varPtr != NULL) && !TclIsVarUndefined(varPtr)) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
    } else {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoFunctionsCmd --
 *
 *	Called to implement the "info functions" command that returns the list
 *	of math functions matching an optional pattern. Handles the following
 *	syntax:
 *
 *	    info functions ?pattern?
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoFunctionsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *pattern;
    Tcl_Obj *listPtr;

    if (objc == 2) {
	pattern = NULL;
    } else if (objc == 3) {
	pattern = TclGetString(objv[2]);
    } else {
	Tcl_WrongNumArgs(interp, 2, objv, "?pattern?");
	return TCL_ERROR;
    }

    listPtr = Tcl_ListMathFuncs(interp, pattern);
    if (listPtr == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoGlobalsCmd --
 *
 *	Called to implement the "info globals" command that returns the list
 *	of global variables matching an optional pattern. Handles the
 *	following syntax:
 *
 *	    info globals ?pattern?
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoGlobalsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *varName, *pattern;
    Namespace *globalNsPtr = (Namespace *) Tcl_GetGlobalNamespace(interp);
    register Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Var *varPtr;
    Tcl_Obj *listPtr;

    if (objc == 2) {
	pattern = NULL;
    } else if (objc == 3) {
	pattern = TclGetString(objv[2]);
	/*
	 * Strip leading global-namespace qualifiers. [Bug 1057461]
	 */

	if (pattern[0] == ':' && pattern[1] == ':') {
	    while (*pattern == ':') {
		pattern++;
	    }
	}
    } else {
	Tcl_WrongNumArgs(interp, 2, objv, "?pattern?");
	return TCL_ERROR;
    }

    /*
     * Scan through the global :: namespace's variable table and create a list
     * of all global variables that match the pattern.
     */

    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    if (pattern != NULL && TclMatchIsTrivial(pattern)) {
	entryPtr = Tcl_FindHashEntry(&globalNsPtr->varTable, pattern);
	if (entryPtr != NULL) {
	    varPtr = (Var *) Tcl_GetHashValue(entryPtr);
	    if (!TclIsVarUndefined(varPtr)) {
		Tcl_ListObjAppendElement(interp, listPtr,
			Tcl_NewStringObj(pattern, -1));
	    }
	}
    } else {
	for (entryPtr = Tcl_FirstHashEntry(&globalNsPtr->varTable, &search);
		entryPtr != NULL;
		entryPtr = Tcl_NextHashEntry(&search)) {
	    varPtr = (Var *) Tcl_GetHashValue(entryPtr);
	    if (TclIsVarUndefined(varPtr)) {
		continue;
	    }
	    varName = Tcl_GetHashKey(&globalNsPtr->varTable, entryPtr);
	    if ((pattern == NULL) || Tcl_StringMatch(varName, pattern)) {
		Tcl_ListObjAppendElement(interp, listPtr,
			Tcl_NewStringObj(varName, -1));
	    }
	}
    }
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoHostnameCmd --
 *
 *	Called to implement the "info hostname" command that returns the host
 *	name. Handles the following syntax:
 *
 *	    info hostname
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoHostnameCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    CONST char *name;
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 2, objv, NULL);
	return TCL_ERROR;
    }

    name = Tcl_GetHostName();
    if (name) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(name, -1));
	return TCL_OK;
    } else {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"unable to determine name of host", -1));
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InfoLevelCmd --
 *
 *	Called to implement the "info level" command that returns information
 *	about the call stack. Handles the following syntax:
 *
 *	    info level ?number?
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoLevelCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    int level;
    CallFrame *framePtr;
    Tcl_Obj *listPtr;

    if (objc == 2) {		/* just "info level" */
	if (iPtr->varFramePtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(iPtr->varFramePtr->level));
	}
	return TCL_OK;
    } else if (objc == 3) {
	if (Tcl_GetIntFromObj(interp, objv[2], &level) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (level <= 0) {
	    if (iPtr->varFramePtr == NULL) {
		levelError:
		Tcl_AppendResult(interp, "bad level \"",
			TclGetString(objv[2]), "\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    level += iPtr->varFramePtr->level;
	}
	for (framePtr = iPtr->varFramePtr;  framePtr != NULL;
		framePtr = framePtr->callerVarPtr) {
	    if (framePtr->level == level) {
		break;
	    }
	}
	if (framePtr == NULL) {
	    goto levelError;
	}

	listPtr = Tcl_NewListObj(framePtr->objc, framePtr->objv);
	Tcl_SetObjResult(interp, listPtr);
	return TCL_OK;
    }

    Tcl_WrongNumArgs(interp, 2, objv, "?number?");
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoLibraryCmd --
 *
 *	Called to implement the "info library" command that returns the
 *	library directory for the Tcl installation. Handles the following
 *	syntax:
 *
 *	    info library
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoLibraryCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    CONST char *libDirName;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 2, objv, NULL);
	return TCL_ERROR;
    }

    libDirName = Tcl_GetVar(interp, "tcl_library", TCL_GLOBAL_ONLY);
    if (libDirName != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(libDirName, -1));
	return TCL_OK;
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(
	    "no library has been specified for Tcl", -1));
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoLoadedCmd --
 *
 *	Called to implement the "info loaded" command that returns the
 *	packages that have been loaded into an interpreter. Handles the
 *	following syntax:
 *
 *	    info loaded ?interp?
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoLoadedCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *interpName;
    int result;

    if ((objc != 2) && (objc != 3)) {
	Tcl_WrongNumArgs(interp, 2, objv, "?interp?");
	return TCL_ERROR;
    }

    if (objc == 2) {		/* get loaded pkgs in all interpreters */
	interpName = NULL;
    } else {			/* get pkgs just in specified interp */
	interpName = TclGetString(objv[2]);
    }
    result = TclGetLoadedPackages(interp, interpName);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoLocalsCmd --
 *
 *	Called to implement the "info locals" command to return a list of
 *	local variables that match an optional pattern. Handles the following
 *	syntax:
 *
 *	    info locals ?pattern?
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoLocalsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    char *pattern;
    Tcl_Obj *listPtr;

    if (objc == 2) {
	pattern = NULL;
    } else if (objc == 3) {
	pattern = TclGetString(objv[2]);
    } else {
	Tcl_WrongNumArgs(interp, 2, objv, "?pattern?");
	return TCL_ERROR;
    }

    if (iPtr->varFramePtr == NULL ||
	    !(iPtr->varFramePtr->isProcCallFrame & FRAME_IS_PROC )) {
	return TCL_OK;
    }

    /*
     * Return a list containing names of first the compiled locals (i.e. the
     * ones stored in the call frame), then the variables in the local hash
     * table (if one exists).
     */

    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    AppendLocals(interp, listPtr, pattern, 0);
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AppendLocals --
 *
 *	Append the local variables for the current frame to the specified list
 *	object.
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
AppendLocals(interp, listPtr, pattern, includeLinks)
    Tcl_Interp *interp;		/* Current interpreter. */
    Tcl_Obj *listPtr;		/* List object to append names to. */
    CONST char *pattern;	/* Pattern to match against. */
    int includeLinks;		/* 1 if upvars should be included, else 0. */
{
    Interp *iPtr = (Interp *) interp;
    CompiledLocal *localPtr;
    Var *varPtr;
    int i, localVarCt;
    char *varName;
    Tcl_HashTable *localVarTablePtr;
    register Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;

    localPtr = iPtr->varFramePtr->procPtr->firstLocalPtr;
    localVarCt = iPtr->varFramePtr->numCompiledLocals;
    varPtr = iPtr->varFramePtr->compiledLocals;
    localVarTablePtr = iPtr->varFramePtr->varTablePtr;

    for (i = 0; i < localVarCt; i++) {
	/*
	 * Skip nameless (temporary) variables and undefined variables
	 */

	if (!TclIsVarTemporary(localPtr) && !TclIsVarUndefined(varPtr)
		&& (includeLinks || !TclIsVarLink(varPtr))) {
	    varName = varPtr->name;
	    if ((pattern == NULL) || Tcl_StringMatch(varName, pattern)) {
		Tcl_ListObjAppendElement(interp, listPtr,
			Tcl_NewStringObj(varName, -1));
	    }
	}
	varPtr++;
	localPtr = localPtr->nextPtr;
    }

    /*
     * Do nothing if no local variables.
     */

    if (localVarTablePtr == NULL) {
	return;
    }

    /*
     * Check for the simple and fast case.
     */

    if ((pattern != NULL) && TclMatchIsTrivial(pattern)) {
	entryPtr = Tcl_FindHashEntry(localVarTablePtr, pattern);
	if (entryPtr != NULL) {
	    varPtr = (Var *) Tcl_GetHashValue(entryPtr);
	    if (!TclIsVarUndefined(varPtr)
		    && (includeLinks || !TclIsVarLink(varPtr))) {
		Tcl_ListObjAppendElement(interp, listPtr,
			Tcl_NewStringObj(pattern,-1));
	    }
	}
	return;
    }

    /*
     * Scan over and process all local variables.
     */

    for (entryPtr = Tcl_FirstHashEntry(localVarTablePtr, &search);
	    entryPtr != NULL;
	    entryPtr = Tcl_NextHashEntry(&search)) {
	varPtr = (Var *) Tcl_GetHashValue(entryPtr);
	if (!TclIsVarUndefined(varPtr)
		&& (includeLinks || !TclIsVarLink(varPtr))) {
	    varName = Tcl_GetHashKey(localVarTablePtr, entryPtr);
	    if ((pattern == NULL) || Tcl_StringMatch(varName, pattern)) {
		Tcl_ListObjAppendElement(interp, listPtr,
			Tcl_NewStringObj(varName, -1));
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InfoNameOfExecutableCmd --
 *
 *	Called to implement the "info nameofexecutable" command that returns
 *	the name of the binary file running this application. Handles the
 *	following syntax:
 *
 *	    info nameofexecutable
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoNameOfExecutableCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 2, objv, NULL);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, TclGetObjNameOfExecutable());
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoPatchLevelCmd --
 *
 *	Called to implement the "info patchlevel" command that returns the
 *	default value for an argument to a procedure. Handles the following
 *	syntax:
 *
 *	    info patchlevel
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoPatchLevelCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    CONST char *patchlevel;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 2, objv, NULL);
	return TCL_ERROR;
    }

    patchlevel = Tcl_GetVar(interp, "tcl_patchLevel",
	    (TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG));
    if (patchlevel != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(patchlevel, -1));
	return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoProcsCmd --
 *
 *	Called to implement the "info procs" command that returns the list of
 *	procedures in the interpreter that match an optional pattern. The
 *	pattern, if any, consists of an optional sequence of namespace names
 *	separated by "::" qualifiers, which is followed by a glob-style
 *	pattern that restricts which commands are returned. Handles the
 *	following syntax:
 *
 *	    info procs ?pattern?
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoProcsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *cmdName, *pattern;
    CONST char *simplePattern;
    Namespace *nsPtr;
#ifdef INFO_PROCS_SEARCH_GLOBAL_NS
    Namespace *globalNsPtr = (Namespace *) Tcl_GetGlobalNamespace(interp);
#endif
    Namespace *currNsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    Tcl_Obj *listPtr, *elemObjPtr;
    int specificNsInPattern = 0;/* Init. to avoid compiler warning. */
    register Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Command *cmdPtr, *realCmdPtr;

    /*
     * Get the pattern and find the "effective namespace" in which to list
     * procs.
     */

    if (objc == 2) {
	simplePattern = NULL;
	nsPtr = currNsPtr;
	specificNsInPattern = 0;
    } else if (objc == 3) {
	/*
	 * From the pattern, get the effective namespace and the simple
	 * pattern (no namespace qualifiers or ::'s) at the end. If an error
	 * was found while parsing the pattern, return it. Otherwise, if the
	 * namespace wasn't found, just leave nsPtr NULL: we will return an
	 * empty list since no commands there can be found.
	 */

	Namespace *dummy1NsPtr, *dummy2NsPtr;

	pattern = TclGetString(objv[2]);
	TclGetNamespaceForQualName(interp, pattern, (Namespace *) NULL,
		/*flags*/ 0, &nsPtr, &dummy1NsPtr, &dummy2NsPtr,
		&simplePattern);

	if (nsPtr != NULL) {	/* we successfully found the pattern's ns */
	    specificNsInPattern = (strcmp(simplePattern, pattern) != 0);
	}
    } else {
	Tcl_WrongNumArgs(interp, 2, objv, "?pattern?");
	return TCL_ERROR;
    }

    if (nsPtr == NULL) {
	return TCL_OK;
    }

    /*
     * Scan through the effective namespace's command table and create a list
     * with all procs that match the pattern. If a specific namespace was
     * requested in the pattern, qualify the command names with the namespace
     * name.
     */

    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
#ifndef INFO_PROCS_SEARCH_GLOBAL_NS
    if (simplePattern != NULL && TclMatchIsTrivial(simplePattern)) {
	entryPtr = Tcl_FindHashEntry(&nsPtr->cmdTable, simplePattern);
	if (entryPtr != NULL) {
	    cmdPtr = (Command *) Tcl_GetHashValue(entryPtr);

	    if (!TclIsProc(cmdPtr)) {
		realCmdPtr = (Command *)
			TclGetOriginalCommand((Tcl_Command) cmdPtr);
		if (realCmdPtr != NULL && TclIsProc(realCmdPtr)) {
		    goto simpleProcOK;
		}
	    } else {
	    simpleProcOK:
		if (specificNsInPattern) {
		    elemObjPtr = Tcl_NewObj();
		    Tcl_GetCommandFullName(interp, (Tcl_Command) cmdPtr,
			    elemObjPtr);
		} else {
		    elemObjPtr = Tcl_NewStringObj(simplePattern, -1);
		}
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	    }
	}
    } else
#endif /* !INFO_PROCS_SEARCH_GLOBAL_NS */
    {
	entryPtr = Tcl_FirstHashEntry(&nsPtr->cmdTable, &search);
	while (entryPtr != NULL) {
	    cmdName = Tcl_GetHashKey(&nsPtr->cmdTable, entryPtr);
	    if ((simplePattern == NULL)
		    || Tcl_StringMatch(cmdName, simplePattern)) {
		cmdPtr = (Command *) Tcl_GetHashValue(entryPtr);

		if (!TclIsProc(cmdPtr)) {
		    realCmdPtr = (Command *)
			    TclGetOriginalCommand((Tcl_Command) cmdPtr);
		    if (realCmdPtr != NULL && TclIsProc(realCmdPtr)) {
			goto procOK;
		    }
		} else {
		procOK:
		    if (specificNsInPattern) {
			elemObjPtr = Tcl_NewObj();
			Tcl_GetCommandFullName(interp, (Tcl_Command) cmdPtr,
				elemObjPtr);
		    } else {
			elemObjPtr = Tcl_NewStringObj(cmdName, -1);
		    }
		    Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		}
	    }
	    entryPtr = Tcl_NextHashEntry(&search);
	}

	/*
	 * If the effective namespace isn't the global :: namespace, and a
	 * specific namespace wasn't requested in the pattern, then add in all
	 * global :: procs that match the simple pattern. Of course, we add in
	 * only those procs that aren't hidden by a proc in the effective
	 * namespace.
	 */

#ifdef INFO_PROCS_SEARCH_GLOBAL_NS
	/*
	 * If "info procs" worked like "info commands", returning the commands
	 * also seen in the global namespace, then you would include this
	 * code. As this could break backwards compatibilty with 8.0-8.2, we
	 * decided not to "fix" it in 8.3, leaving the behavior slightly
	 * different.
	 */

	if ((nsPtr != globalNsPtr) && !specificNsInPattern) {
	    entryPtr = Tcl_FirstHashEntry(&globalNsPtr->cmdTable, &search);
	    while (entryPtr != NULL) {
		cmdName = Tcl_GetHashKey(&globalNsPtr->cmdTable, entryPtr);
		if ((simplePattern == NULL)
			|| Tcl_StringMatch(cmdName, simplePattern)) {
		    if (Tcl_FindHashEntry(&nsPtr->cmdTable, cmdName) == NULL) {
			cmdPtr = (Command *) Tcl_GetHashValue(entryPtr);
			realCmdPtr = (Command *) TclGetOriginalCommand(
				(Tcl_Command) cmdPtr);

			if (TclIsProc(cmdPtr) || ((realCmdPtr != NULL)
				&& TclIsProc(realCmdPtr))) {
			    Tcl_ListObjAppendElement(interp, listPtr,
				    Tcl_NewStringObj(cmdName, -1));
			}
		    }
		}
		entryPtr = Tcl_NextHashEntry(&search);
	    }
	}
#endif
    }

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoScriptCmd --
 *
 *	Called to implement the "info script" command that returns the script
 *	file that is currently being evaluated. Handles the following syntax:
 *
 *	    info script ?newName?
 *
 *	If newName is specified, it will set that as the internal name.
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message. It may change the internal
 *	script filename.
 *
 *----------------------------------------------------------------------
 */

static int
InfoScriptCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    if ((objc != 2) && (objc != 3)) {
	Tcl_WrongNumArgs(interp, 2, objv, "?filename?");
	return TCL_ERROR;
    }

    if (objc == 3) {
	if (iPtr->scriptFile != NULL) {
	    Tcl_DecrRefCount(iPtr->scriptFile);
	}
	iPtr->scriptFile = objv[2];
	Tcl_IncrRefCount(iPtr->scriptFile);
    }
    if (iPtr->scriptFile != NULL) {
	Tcl_SetObjResult(interp, iPtr->scriptFile);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoSharedlibCmd --
 *
 *	Called to implement the "info sharedlibextension" command that returns
 *	the file extension used for shared libraries. Handles the following
 *	syntax:
 *
 *	    info sharedlibextension
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoSharedlibCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 2, objv, NULL);
	return TCL_ERROR;
    }

#ifdef TCL_SHLIB_EXT
    Tcl_SetObjResult(interp, Tcl_NewStringObj(TCL_SHLIB_EXT, -1));
#endif
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoTclVersionCmd --
 *
 *	Called to implement the "info tclversion" command that returns the
 *	version number for this Tcl library. Handles the following syntax:
 *
 *	    info tclversion
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoTclVersionCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Obj *version;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 2, objv, NULL);
	return TCL_ERROR;
    }

    version = Tcl_GetVar2Ex(interp, "tcl_version", NULL,
	    (TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG));
    if (version != NULL) {
	Tcl_SetObjResult(interp, version);
	return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InfoVarsCmd --
 *
 *	Called to implement the "info vars" command that returns the list of
 *	variables in the interpreter that match an optional pattern. The
 *	pattern, if any, consists of an optional sequence of namespace names
 *	separated by "::" qualifiers, which is followed by a glob-style
 *	pattern that restricts which variables are returned. Handles the
 *	following syntax:
 *
 *	    info vars ?pattern?
 *
 * Results:
 *	Returns TCL_OK if successful and TCL_ERROR if there is an error.
 *
 * Side effects:
 *	Returns a result in the interpreter's result object. If there is an
 *	error, the result is an error message.
 *
 *----------------------------------------------------------------------
 */

static int
InfoVarsCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Interp *iPtr = (Interp *) interp;
    char *varName, *pattern;
    CONST char *simplePattern;
    register Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Var *varPtr;
    Namespace *nsPtr;
    Namespace *globalNsPtr = (Namespace *) Tcl_GetGlobalNamespace(interp);
    Namespace *currNsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    Tcl_Obj *listPtr, *elemObjPtr;
    int specificNsInPattern = 0;/* Init. to avoid compiler warning. */

    /*
     * Get the pattern and find the "effective namespace" in which to list
     * variables. We only use this effective namespace if there's no active
     * Tcl procedure frame.
     */

    if (objc == 2) {
	simplePattern = NULL;
	nsPtr = currNsPtr;
	specificNsInPattern = 0;
    } else if (objc == 3) {
	/*
	 * From the pattern, get the effective namespace and the simple
	 * pattern (no namespace qualifiers or ::'s) at the end. If an error
	 * was found while parsing the pattern, return it. Otherwise, if the
	 * namespace wasn't found, just leave nsPtr NULL: we will return an
	 * empty list since no variables there can be found.
	 */

	Namespace *dummy1NsPtr, *dummy2NsPtr;

	pattern = TclGetString(objv[2]);
	TclGetNamespaceForQualName(interp, pattern, (Namespace *) NULL,
		/*flags*/ 0, &nsPtr, &dummy1NsPtr, &dummy2NsPtr,
		&simplePattern);

	if (nsPtr != NULL) {	/* We successfully found the pattern's ns */
	    specificNsInPattern = (strcmp(simplePattern, pattern) != 0);
	}
    } else {
	Tcl_WrongNumArgs(interp, 2, objv, "?pattern?");
	return TCL_ERROR;
    }

    /*
     * If the namespace specified in the pattern wasn't found, just return.
     */

    if (nsPtr == NULL) {
	return TCL_OK;
    }

    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);

    if ((iPtr->varFramePtr == NULL)
	    || !(iPtr->varFramePtr->isProcCallFrame & FRAME_IS_PROC)
	    || specificNsInPattern) {
	/*
	 * There is no frame pointer, the frame pointer was pushed only to
	 * activate a namespace, or we are in a procedure call frame but a
	 * specific namespace was specified. Create a list containing only the
	 * variables in the effective namespace's variable table.
	 */

	if (simplePattern != NULL && TclMatchIsTrivial(simplePattern)) {
	    /*
	     * If we can just do hash lookups, that simplifies things a lot.
	     */

	    entryPtr = Tcl_FindHashEntry(&nsPtr->varTable, simplePattern);
	    if (entryPtr != NULL) {
		varPtr = (Var *) Tcl_GetHashValue(entryPtr);
		if (!TclIsVarUndefined(varPtr)
			|| TclIsVarNamespaceVar(varPtr)) {
		    if (specificNsInPattern) {
			elemObjPtr = Tcl_NewObj();
			Tcl_GetVariableFullName(interp, (Tcl_Var) varPtr,
				    elemObjPtr);
		    } else {
			elemObjPtr = Tcl_NewStringObj(simplePattern, -1);
		    }
		    Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		}
	    } else if ((nsPtr != globalNsPtr) && !specificNsInPattern) {
		entryPtr = Tcl_FindHashEntry(&globalNsPtr->varTable,
			simplePattern);
		if (entryPtr != NULL) {
		    varPtr = (Var *) Tcl_GetHashValue(entryPtr);
		    if (!TclIsVarUndefined(varPtr)
			    || TclIsVarNamespaceVar(varPtr)) {
			Tcl_ListObjAppendElement(interp, listPtr,
				Tcl_NewStringObj(simplePattern, -1));
		    }
		}
	    }
	} else {
	    /*
	     * Have to scan the tables of variables.
	     */

	    entryPtr = Tcl_FirstHashEntry(&nsPtr->varTable, &search);
	    while (entryPtr != NULL) {
		varPtr = (Var *) Tcl_GetHashValue(entryPtr);
		if (!TclIsVarUndefined(varPtr)
			|| TclIsVarNamespaceVar(varPtr)) {
		    varName = Tcl_GetHashKey(&nsPtr->varTable, entryPtr);
		    if ((simplePattern == NULL)
			    || Tcl_StringMatch(varName, simplePattern)) {
			if (specificNsInPattern) {
			    elemObjPtr = Tcl_NewObj();
			    Tcl_GetVariableFullName(interp, (Tcl_Var) varPtr,
				    elemObjPtr);
			} else {
			    elemObjPtr = Tcl_NewStringObj(varName, -1);
			}
			Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		    }
		}
		entryPtr = Tcl_NextHashEntry(&search);
	    }

	    /*
	     * If the effective namespace isn't the global :: namespace, and a
	     * specific namespace wasn't requested in the pattern (i.e., the
	     * pattern only specifies variable names), then add in all global
	     * :: variables that match the simple pattern. Of course, add in
	     * only those variables that aren't hidden by a variable in the
	     * effective namespace.
	     */

	    if ((nsPtr != globalNsPtr) && !specificNsInPattern) {
		entryPtr = Tcl_FirstHashEntry(&globalNsPtr->varTable, &search);
		while (entryPtr != NULL) {
		    varPtr = (Var *) Tcl_GetHashValue(entryPtr);
		    if (!TclIsVarUndefined(varPtr)
			    || TclIsVarNamespaceVar(varPtr)) {
			varName = Tcl_GetHashKey(&globalNsPtr->varTable,
				entryPtr);
			if ((simplePattern == NULL)
				|| Tcl_StringMatch(varName, simplePattern)) {
			    if (Tcl_FindHashEntry(&nsPtr->varTable,
				    varName) == NULL) {
				Tcl_ListObjAppendElement(interp, listPtr,
					Tcl_NewStringObj(varName, -1));
			    }
			}
		    }
		    entryPtr = Tcl_NextHashEntry(&search);
		}
	    }
	}
    } else if (((Interp *)interp)->varFramePtr->procPtr != NULL) {
	AppendLocals(interp, listPtr, simplePattern, 1);
    }

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_JoinObjCmd --
 *
 *	This procedure is invoked to process the "join" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_JoinObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* The argument objects. */
{
    char *joinString, *bytes;
    int joinLength, listLen, length, i, result;
    Tcl_Obj **elemPtrs;
    Tcl_Obj *resObjPtr;

    if (objc == 2) {
	joinString = " ";
	joinLength = 1;
    } else if (objc == 3) {
	joinString = Tcl_GetStringFromObj(objv[2], &joinLength);
    } else {
	Tcl_WrongNumArgs(interp, 1, objv, "list ?joinString?");
	return TCL_ERROR;
    }

    /*
     * Make sure the list argument is a list object and get its length and a
     * pointer to its array of element pointers.
     */

    result = Tcl_ListObjGetElements(interp, objv[1], &listLen, &elemPtrs);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Now concatenate strings to form the "joined" result.
     */

    resObjPtr = Tcl_NewObj();
    for (i = 0;  i < listLen;  i++) {
	bytes = Tcl_GetStringFromObj(elemPtrs[i], &length);
	if (i > 0) {
	    Tcl_AppendToObj(resObjPtr, joinString, joinLength);
	}
	Tcl_AppendToObj(resObjPtr, bytes, length);
    }
    Tcl_SetObjResult(interp, resObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LassignObjCmd --
 *
 *	This object-based procedure is invoked to process the "lassign" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
int
Tcl_LassignObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Obj *valueObj;		/* Value to assign to variable, as read from
				 * the list object or created in the emptyObj
				 * variable. */
    Tcl_Obj *emptyObj = NULL;	/* If non-NULL, an empty object created for
				 * being assigned to variables once we have
				 * run out of values from the list object. */
    Tcl_Obj **listObjv;		/* The contents of the list. */
    int listObjc;		/* The length of the list. */
    int i;
    Tcl_Obj *resPtr;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "list varName ?varName ...?");
	return TCL_ERROR;
    }

    /*
     * First assign values out of the list to variables.
     */

    for (i=0 ; i+2<objc ; i++) {
	/*
	 * We do this each time round the loop because that is robust against
	 * shimmering nasties.
	 */

	if (Tcl_ListObjIndex(interp, objv[1], i, &valueObj) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (valueObj == NULL) {
	    if (emptyObj == NULL) {
		TclNewObj(emptyObj);
		Tcl_IncrRefCount(emptyObj);
	    }
	    valueObj = emptyObj;
	}

	/*
	 * Make sure the reference count for the value being assigned is
	 * greater than one (other reference minimally in the list) so we
	 * can't get hammered by shimmering.
	 */

	Tcl_IncrRefCount(valueObj);
	resPtr = Tcl_ObjSetVar2(interp, objv[i+2], NULL, valueObj,
		TCL_LEAVE_ERR_MSG);
	TclDecrRefCount(valueObj);
	if (resPtr == NULL) {
	    if (emptyObj != NULL) {
		Tcl_DecrRefCount(emptyObj);
	    }
	    return TCL_ERROR;
	}
    }
    if (emptyObj != NULL) {
	Tcl_DecrRefCount(emptyObj);
    }

    /*
     * Now place a list of any values left over into the interpreter result.
     *
     * First, figure out how many values were not assigned by getting the
     * length of the list. Note that I do not expect this operation to fail.
     */

    if (Tcl_ListObjGetElements(interp, objv[1],
	    &listObjc, &listObjv) != TCL_OK) {
	return TCL_ERROR;
    }

    if (listObjc > objc-2) {
	/*
	 * OK, there were left-overs. Make a list of them and slap that back
	 * in the interpreter result.
	 */

	Tcl_SetObjResult(interp,
		Tcl_NewListObj(listObjc - objc + 2, listObjv + objc - 2));
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LindexObjCmd --
 *
 *	This object-based procedure is invoked to process the "lindex" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
int
Tcl_LindexObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{

    Tcl_Obj *elemPtr;		/* Pointer to the element being extracted */

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "list ?index...?");
	return TCL_ERROR;
    }

    /*
     * If objc==3, then objv[2] may be either a single index or a list of
     * indices: go to TclLindexList to determine which. If objc>=4, or
     * objc==2, then objv[2 .. objc-2] are all single indices and processed as
     * such in TclLindexFlat.
     */

    if (objc == 3) {
	elemPtr = TclLindexList(interp, objv[1], objv[2]);
    } else {
	elemPtr = TclLindexFlat(interp, objv[1], objc-2, objv+2);
    }

    /*
     * Set the interpreter's object result to the last element extracted.
     */

    if (elemPtr == NULL) {
	return TCL_ERROR;
    } else {
	Tcl_SetObjResult(interp, elemPtr);
	Tcl_DecrRefCount(elemPtr);
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclLindexList --
 *
 *	This procedure handles the 'lindex' command when objc==3.
 *
 * Results:
 *	Returns a pointer to the object extracted, or NULL if an error
 *	occurred.
 *
 * Side effects:
 *	None.
 *
 * Notes:
 *	If objv[1] can be parsed as a list, TclLindexList handles extraction
 *	of the desired element locally. Otherwise, it invokes TclLindexFlat to
 *	treat objv[1] as a scalar.
 *
 *	The reference count of the returned object includes one reference
 *	corresponding to the pointer returned. Thus, the calling code will
 *	usually do something like:
 *		Tcl_SetObjResult(interp, result);
 *		Tcl_DecrRefCount(result);
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclLindexList(interp, listPtr, argPtr)
    Tcl_Interp* interp;		/* Tcl interpreter */
    Tcl_Obj* listPtr;		/* List being unpacked */
    Tcl_Obj* argPtr;		/* Index or index list */
{

    Tcl_Obj **elemPtrs;		/* Elements of the list being manipulated. */
    int listLen;		/* Length of the list being manipulated. */
    int index;			/* Index into the list. */
    int result;			/* Result returned from a Tcl library call. */
    int i;			/* Current index number. */
    Tcl_Obj **indices;		/* Array of list indices. */
    int indexCount;		/* Size of the array of list indices. */
    Tcl_Obj *oldListPtr;	/* Temp location to preserve the list pointer
				 * when replacing it with a sublist. */

    /*
     * Determine whether argPtr designates a list or a single index. We have
     * to be careful about the order of the checks to avoid repeated
     * shimmering; see TIP#22 and TIP#33 for the details.
     */

    if (argPtr->typePtr != &tclListType
	    && TclGetIntForIndex(NULL , argPtr, 0, &index) == TCL_OK) {
	/*
	 * argPtr designates a single index.
	 */

	return TclLindexFlat(interp, listPtr, 1, &argPtr);
    }

    if (Tcl_ListObjGetElements(NULL, argPtr, &indexCount, &indices) != TCL_OK){
	/*
	 * argPtr designates something that is neither an index nor a
	 * well-formed list. Report the error via TclLindexFlat.
	 */

	return TclLindexFlat(interp, listPtr, 1, &argPtr);
    }

    /*
     * Record the reference to the list that we are maintaining in the
     * activation record.
     */

    Tcl_IncrRefCount(listPtr);

    /*
     * argPtr designates a list, and the 'else if' above has parsed it into
     * indexCount and indices.
     */

    for (i=0 ; i<indexCount ; i++) {
	/*
	 * Convert the current listPtr to a list if necessary.
	 */

	result = Tcl_ListObjGetElements(interp, listPtr, &listLen, &elemPtrs);
	if (result != TCL_OK) {
	    Tcl_DecrRefCount(listPtr);
	    return NULL;
	}

	/*
	 * Get the index from indices[i]
	 */

	result = TclGetIntForIndex(interp, indices[i], /*endValue*/ listLen-1,
		&index);
	if (result != TCL_OK) {
	    /*
	     * Index could not be parsed
	     */

	    Tcl_DecrRefCount(listPtr);
	    return NULL;

	} else if (index<0 || index>=listLen) {
	    /*
	     * Index is out of range
	     */

	    Tcl_DecrRefCount(listPtr);
	    listPtr = Tcl_NewObj();
	    Tcl_IncrRefCount(listPtr);
	    return listPtr;
	}

	/*
	 * Make sure listPtr still refers to a list object. If it shared a
	 * Tcl_Obj structure with the arguments, then it might have just been
	 * converted to something else.
	 */

	if (listPtr->typePtr != &tclListType) {
	    result = Tcl_ListObjGetElements(interp, listPtr, &listLen,
		    &elemPtrs);
	    if (result != TCL_OK) {
		Tcl_DecrRefCount(listPtr);
		return NULL;
	    }
	}

	/*
	 * Extract the pointer to the appropriate element
	 */

	oldListPtr = listPtr;
	listPtr = elemPtrs[index];
	Tcl_IncrRefCount(listPtr);
	Tcl_DecrRefCount(oldListPtr);

	/*
	 * The work we did above may have caused the internal rep of *argPtr
	 * to change to something else. Get it back.
	 */

	result = Tcl_ListObjGetElements(interp, argPtr, &indexCount, &indices);
	if (result != TCL_OK) {
	    /*
	     * This can't happen unless some extension corrupted a Tcl_Obj.
	     */

	    Tcl_DecrRefCount(listPtr);
	    return NULL;
	}
    }

    /*
     * Return the last object extracted. Its reference count will include the
     * reference being returned.
     */

    return listPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclLindexFlat --
 *
 *	This procedure handles the 'lindex' command, given that the arguments
 *	to the command are known to be a flat list.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 * Notes:
 *	This procedure is called from either tclExecute.c or Tcl_LindexObjCmd
 *	whenever either is presented with objc==2 or objc>=4. It is also
 *	called from TclLindexList for the objc==3 case once it is determined
 *	that objv[2] cannot be parsed as a list.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclLindexFlat(interp, listPtr, indexCount, indexArray)
    Tcl_Interp *interp;		/* Tcl interpreter */
    Tcl_Obj *listPtr;		/* Tcl object representing the list */
    int indexCount;		/* Count of indices */
    Tcl_Obj *CONST indexArray[];
				/* Array of pointers to Tcl objects
				 * representing the indices in the list. */
{
    int i;			/* Current list index. */
    int result;			/* Result of Tcl library calls. */
    int listLen;		/* Length of the current list being
				 * processed. */
    Tcl_Obj** elemPtrs;		/* Array of pointers to the elements of the
				 * current list. */
    int index;			/* Parsed version of the current element of
				 * indexArray. */
    Tcl_Obj* oldListPtr;	/* Temporary to hold listPtr so that its ref
				 * count can be decremented. */

    /*
     * Record the reference to the 'listPtr' object that we are maintaining in
     * the C activation record.
     */

    Tcl_IncrRefCount(listPtr);

    for (i=0 ; i<indexCount ; i++) {
	/*
	 * Convert the current listPtr to a list if necessary.
	 */

	result = Tcl_ListObjGetElements(interp, listPtr, &listLen, &elemPtrs);
	if (result != TCL_OK) {
	    Tcl_DecrRefCount(listPtr);
	    return NULL;
	}

	/*
	 * Get the index from objv[i].
	 */

	result = TclGetIntForIndex(interp, indexArray[i],
		/*endValue*/ listLen-1, &index);
	if (result != TCL_OK) {
	    /*
	     * Index could not be parsed.
	     */

	    Tcl_DecrRefCount(listPtr);
	    return NULL;

	} else if (index<0 || index>=listLen) {
	    /*
	     * Index is out of range.
	     */

	    Tcl_DecrRefCount(listPtr);
	    listPtr = Tcl_NewObj();
	    Tcl_IncrRefCount(listPtr);
	    return listPtr;
	}

	/*
	 * Make sure listPtr still refers to a list object. It might have been
	 * converted to something else above if objv[1] overlaps with one of
	 * the other parameters.
	 */

	if (listPtr->typePtr != &tclListType) {
	    result = Tcl_ListObjGetElements(interp, listPtr, &listLen,
		    &elemPtrs);
	    if (result != TCL_OK) {
		Tcl_DecrRefCount(listPtr);
		return NULL;
	    }
	}

	/*
	 * Extract the pointer to the appropriate element.
	 */

	oldListPtr = listPtr;
	listPtr = elemPtrs[index];
	Tcl_IncrRefCount(listPtr);
	Tcl_DecrRefCount(oldListPtr);
    }

    return listPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LinsertObjCmd --
 *
 *	This object-based procedure is invoked to process the "linsert" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A new Tcl list object formed by inserting zero or more elements into a
 *	list.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_LinsertObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    register int objc;		/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Obj *listPtr;
    int index, isDuplicate, len, result;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "list index element ?element ...?");
	return TCL_ERROR;
    }

    result = Tcl_ListObjLength(interp, objv[1], &len);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Get the index. "end" is interpreted to be the index after the last
     * element, such that using it will cause any inserted elements to be
     * appended to the list.
     */

    result = TclGetIntForIndex(interp, objv[2], /*end*/ len, &index);
    if (result != TCL_OK) {
	return result;
    }
    if (index > len) {
	index = len;
    }

    /*
     * If the list object is unshared we can modify it directly. Otherwise we
     * create a copy to modify: this is "copy on write".
     */

    listPtr = objv[1];
    isDuplicate = 0;
    if (Tcl_IsShared(listPtr)) {
	listPtr = Tcl_DuplicateObj(listPtr);
	isDuplicate = 1;
    }

    if ((objc == 4) && (index == len)) {
	/*
	 * Special case: insert one element at the end of the list.
	 */

	result = Tcl_ListObjAppendElement(interp, listPtr, objv[3]);
    } else if (objc > 3) {
	result = Tcl_ListObjReplace(interp, listPtr, index, 0,
				    (objc-3), &(objv[3]));
    }
    if (result != TCL_OK) {
	if (isDuplicate) {
	    Tcl_DecrRefCount(listPtr); /* free unneeded obj */
	}
	return result;
    }

    /*
     * Set the interpreter's object result.
     */

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ListObjCmd --
 *
 *	This procedure is invoked to process the "list" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_ListObjCmd(dummy, interp, objc, objv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    register int objc;			/* Number of arguments. */
    register Tcl_Obj *CONST objv[];	/* The argument objects. */
{
    /*
     * If there are no list elements, the result is an empty object.
     * Otherwise set the interpreter's result object to be a list object.
     */

    if (objc > 1) {
	Tcl_SetObjResult(interp, Tcl_NewListObj((objc-1), &(objv[1])));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LlengthObjCmd --
 *
 *	This object-based procedure is invoked to process the "llength" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_LlengthObjCmd(dummy, interp, objc, objv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments. */
    register Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int listLen, result;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "list");
	return TCL_ERROR;
    }

    result = Tcl_ListObjLength(interp, objv[1], &listLen);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Set the interpreter's object result to an integer object holding the
     * length.
     */

    Tcl_SetObjResult(interp, Tcl_NewIntObj(listLen));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LrangeObjCmd --
 *
 *	This procedure is invoked to process the "lrange" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_LrangeObjCmd(notUsed, interp, objc, objv)
    ClientData notUsed;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments. */
    register Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    Tcl_Obj *listPtr;
    Tcl_Obj **elemPtrs;
    int listLen, first, last, numElems, result;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "list first last");
	return TCL_ERROR;
    }

    /*
     * Make sure the list argument is a list object and get its length and a
     * pointer to its array of element pointers.
     */

    listPtr = objv[1];
    result = Tcl_ListObjGetElements(interp, listPtr, &listLen, &elemPtrs);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Get the first and last indexes.
     */

    result = TclGetIntForIndex(interp, objv[2], /*endValue*/ (listLen - 1),
	    &first);
    if (result != TCL_OK) {
	return result;
    }
    if (first < 0) {
	first = 0;
    }

    result = TclGetIntForIndex(interp, objv[3], /*endValue*/ (listLen - 1),
	    &last);
    if (result != TCL_OK) {
	return result;
    }
    if (last >= listLen) {
	last = (listLen - 1);
    }

    if (first > last) {
	return TCL_OK;		/* the result is an empty object */
    }

    /*
     * Make sure listPtr still refers to a list object. It might have been
     * converted to an int above if the argument objects were shared.
     */

    if (listPtr->typePtr != &tclListType) {
	result = Tcl_ListObjGetElements(interp, listPtr, &listLen,
		&elemPtrs);
	if (result != TCL_OK) {
	    return result;
	}
    }

    /*
     * Extract a range of fields. We modify the interpreter's result object to
     * be a list object containing the specified elements.
     */

    numElems = (last - first + 1);
    Tcl_SetObjResult(interp, Tcl_NewListObj(numElems, &(elemPtrs[first])));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LrepeatObjCmd --
 *
 *	This procedure is invoked to process the "lrepeat" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_LrepeatObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    register int objc;		/* Number of arguments. */
    register Tcl_Obj *CONST objv[];
				/* The argument objects. */
{
    int elementCount, i, result;
    Tcl_Obj *listPtr, **dataArray;
    List *listRepPtr;

    /*
     * Check arguments for legality:
     *		lrepeat posInt value ?value ...?
     */

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "positiveCount value ?value ...?");
	return TCL_ERROR;
    }
    elementCount = 0;
    result = Tcl_GetIntFromObj(interp, objv[1], &elementCount);
    if (result == TCL_ERROR) {
	return TCL_ERROR;
    }
    if (elementCount < 1) {
	Tcl_AppendResult(interp, "must have a count of at least 1", NULL);
	return TCL_ERROR;
    }

    /*
     * Skip forward to the interesting arguments now we've finished parsing.
     */

    objc -= 2;
    objv += 2;

    /*
     * Get an empty list object that is allocated large enough to hold each
     * init value elementCount times.
     */

    listPtr = Tcl_NewListObj(elementCount*objc, NULL);
    listRepPtr = (List *) listPtr->internalRep.twoPtrValue.ptr1;
    listRepPtr->elemCount = elementCount*objc;
    dataArray = &listRepPtr->elements;

    /*
     * Set the elements. Note that we handle the common degenerate case of a
     * single value being repeated separately to permit the compiler as much
     * room as possible to optimize a loop that might be run a very large
     * number of times.
     */

    if (objc == 1) {
	register Tcl_Obj *tmpPtr = objv[0];

	tmpPtr->refCount += elementCount;
	for (i=0 ; i<elementCount ; i++) {
	    dataArray[i] = tmpPtr;
	}
    } else {
	int j, k = 0;

	for (i=0 ; i<elementCount ; i++) {
	    for (j=0 ; j<objc ; j++) {
		Tcl_IncrRefCount(objv[j]);
		dataArray[k++] = objv[j];
	    }
	}
    }

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LreplaceObjCmd --
 *
 *	This object-based procedure is invoked to process the "lreplace" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A new Tcl list object formed by replacing zero or more elements of a
 *	list.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_LreplaceObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    register Tcl_Obj *listPtr;
    int isDuplicate, first, last, listLen, numToDelete, result;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"list first last ?element element ...?");
	return TCL_ERROR;
    }

    result = Tcl_ListObjLength(interp, objv[1], &listLen);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Get the first and last indexes. "end" is interpreted to be the index
     * for the last element, such that using it will cause that element to be
     * included for deletion.
     */

    result = TclGetIntForIndex(interp, objv[2], /*end*/ (listLen - 1), &first);
    if (result != TCL_OK) {
	return result;
    }

    result = TclGetIntForIndex(interp, objv[3], /*end*/ (listLen - 1), &last);
    if (result != TCL_OK) {
	return result;
    }

    if (first < 0) {
    	first = 0;
    }

    /*
     * Complain if the user asked for a start element that is greater than the
     * list length. This won't ever trigger for the "end*" case as that will
     * be properly constrained by TclGetIntForIndex because we use listLen-1
     * (to allow for replacing the last elem).
     */

    if ((first >= listLen) && (listLen > 0)) {
	Tcl_AppendResult(interp, "list doesn't contain element ",
		TclGetString(objv[2]), (int *) NULL);
	return TCL_ERROR;
    }
    if (last >= listLen) {
    	last = (listLen - 1);
    }
    if (first <= last) {
	numToDelete = (last - first + 1);
    } else {
	numToDelete = 0;
    }

    /*
     * If the list object is unshared we can modify it directly, otherwise we
     * create a copy to modify: this is "copy on write".
     */

    listPtr = objv[1];
    isDuplicate = 0;
    if (Tcl_IsShared(listPtr)) {
	listPtr = Tcl_DuplicateObj(listPtr);
	isDuplicate = 1;
    }
    if (objc > 4) {
	result = Tcl_ListObjReplace(interp, listPtr, first, numToDelete,
		(objc-4), &(objv[4]));
    } else {
	result = Tcl_ListObjReplace(interp, listPtr, first, numToDelete,
		0, NULL);
    }
    if (result != TCL_OK) {
	if (isDuplicate) {
	    Tcl_DecrRefCount(listPtr); /* free unneeded obj */
	}
	return result;
    }

    /*
     * Set the interpreter's object result.
     */

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LsearchObjCmd --
 *
 *	This procedure is invoked to process the "lsearch" Tcl command. See
 *	the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LsearchObjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument values. */
{
    char *bytes, *patternBytes;
    int i, match, mode, index, result, listc, length, elemLen;
    int dataType, isIncreasing, lower, upper, patInt, objInt, offset;
    int allMatches, inlineReturn, negatedMatch, returnSubindices, noCase;
    double patDouble, objDouble;
    SortInfo sortInfo;
    Tcl_Obj *patObj, **listv, *listPtr, *startPtr, *itemPtr;
    Tcl_RegExp regexp = NULL;
    static CONST char *options[] = {
	"-all",	    "-ascii",   "-decreasing", "-dictionary",
	"-exact",   "-glob",    "-increasing", "-index",
	"-inline",  "-integer", "-nocase",     "-not",
	"-real",    "-regexp",  "-sorted",     "-start",
	"-subindices",
	NULL
    };
    enum options {
	LSEARCH_ALL, LSEARCH_ASCII, LSEARCH_DECREASING, LSEARCH_DICTIONARY,
	LSEARCH_EXACT, LSEARCH_GLOB, LSEARCH_INCREASING, LSEARCH_INDEX,
	LSEARCH_INLINE, LSEARCH_INTEGER, LSEARCH_NOCASE, LSEARCH_NOT,
	LSEARCH_REAL, LSEARCH_REGEXP, LSEARCH_SORTED, LSEARCH_START,
	LSEARCH_SUBINDICES
    };
    enum datatypes {
	ASCII, DICTIONARY, INTEGER, REAL
    };
    enum modes {
	EXACT, GLOB, REGEXP, SORTED
    };
    SortStrCmpFn_t strCmpFn = strcmp;

    mode = GLOB;
    dataType = ASCII;
    isIncreasing = 1;
    allMatches = 0;
    inlineReturn = 0;
    returnSubindices = 0;
    negatedMatch = 0;
    listPtr = NULL;
    startPtr = NULL;
    offset = 0;
    noCase = 0;
    sortInfo.compareCmdPtr = NULL;
    sortInfo.isIncreasing = 0;
    sortInfo.sortMode = 0;
    sortInfo.interp = interp;
    sortInfo.resultCode = TCL_OK;
    sortInfo.indexv = NULL;
    sortInfo.indexc = 0;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "?options? list pattern");
	return TCL_ERROR;
    }

    for (i = 1; i < objc-2; i++) {
	if (Tcl_GetIndexFromObj(interp, objv[i], options, "option", 0, &index)
		!= TCL_OK) {
	    if (startPtr != NULL) {
		Tcl_DecrRefCount(startPtr);
	    }
	    if (sortInfo.indexc > 1) {
		ckfree((char *) sortInfo.indexv);
	    }
	    return TCL_ERROR;
	}
	switch ((enum options) index) {
	case LSEARCH_ALL:		/* -all */
	    allMatches = 1;
	    break;
	case LSEARCH_ASCII:		/* -ascii */
	    dataType = ASCII;
	    break;
	case LSEARCH_DECREASING:	/* -decreasing */
	    isIncreasing = 0;
	    break;
	case LSEARCH_DICTIONARY:	/* -dictionary */
	    dataType = DICTIONARY;
	    break;
	case LSEARCH_EXACT:		/* -increasing */
	    mode = EXACT;
	    break;
	case LSEARCH_GLOB:		/* -glob */
	    mode = GLOB;
	    break;
	case LSEARCH_INCREASING:	/* -increasing */
	    isIncreasing = 1;
	    break;
	case LSEARCH_INLINE:		/* -inline */
	    inlineReturn = 1;
	    break;
	case LSEARCH_INTEGER:		/* -integer */
	    dataType = INTEGER;
	    break;
	case LSEARCH_NOCASE:		/* -nocase */
	    strCmpFn = strcasecmp;
	    noCase = 1;
	    break;
	case LSEARCH_NOT:		/* -not */
	    negatedMatch = 1;
	    break;
	case LSEARCH_REAL:		/* -real */
	    dataType = REAL;
	    break;
	case LSEARCH_REGEXP:		/* -regexp */
	    mode = REGEXP;
	    break;
	case LSEARCH_SORTED:		/* -sorted */
	    mode = SORTED;
	    break;
	case LSEARCH_SUBINDICES:	/* -subindices */
	    returnSubindices = 1;
	    break;
	case LSEARCH_START:		/* -start */
	    /*
	     * If there was a previous -start option, release its saved index
	     * because it will either be replaced or there will be an error.
	     */

	    if (startPtr != NULL) {
		Tcl_DecrRefCount(startPtr);
	    }
	    if (i > objc-4) {
		if (sortInfo.indexc > 1) {
		    ckfree((char *) sortInfo.indexv);
		}
		Tcl_AppendResult(interp, "missing starting index", NULL);
		return TCL_ERROR;
	    }
	    i++;
	    if (objv[i] == objv[objc - 2]) {
		/*
		 * Take copy to prevent shimmering problems. Note that it
		 * does not matter if the index obj is also a component of the
		 * list being searched. We only need to copy where the list
		 * and the index are one-and-the-same.
		 */

		startPtr = Tcl_DuplicateObj(objv[i]);
	    } else {
		startPtr = objv[i];
		Tcl_IncrRefCount(startPtr);
	    }
	    break;
	case LSEARCH_INDEX: {		/* -index */
	    Tcl_Obj **indices;
	    int j;
	    if (sortInfo.indexc > 1) {
		ckfree((char *) sortInfo.indexv);
	    }
	    if (i > objc-4) {
		if (startPtr != NULL) {
		    Tcl_DecrRefCount(startPtr);
		}
		Tcl_AppendResult(interp,
			"\"-index\" option must be followed by list index",
			NULL);
		return TCL_ERROR;
	    }

	    /*
	     * Store the extracted indices for processing by sublist
	     * extraction. Note that we don't do this using objects because
	     * that has shimmering problems.
	     */

	    i++;
	    if (Tcl_ListObjGetElements(interp, objv[i],
		    &sortInfo.indexc, &indices) != TCL_OK) {
		if (startPtr != NULL) {
		    Tcl_DecrRefCount(startPtr);
		}
		return TCL_ERROR;
	    }
	    switch (sortInfo.indexc) {
	    case 0:
		sortInfo.indexv = NULL;
		break;
	    case 1:
		sortInfo.indexv = &sortInfo.singleIndex;
		break;
	    default:
		sortInfo.indexv = (int *)
			ckalloc(sizeof(int) * sortInfo.indexc);
	    }

	    /*
	     * Fill the array by parsing each index. We don't know whether
	     * their scale is sensible yet, but we at least perform the
	     * syntactic check here.
	     */

	    for (j=0 ; j<sortInfo.indexc ; j++) {
		if (TclGetIntForIndex(interp, indices[j], SORTIDX_END,
			&sortInfo.indexv[j]) != TCL_OK) {
		    if (sortInfo.indexc > 1) {
			ckfree((char *) sortInfo.indexv);
		    }
		    TclFormatToErrorInfo(interp,
			    "\n    (-index option item number %d)", j);
		    return TCL_ERROR;
		}
	    }
	    break;
	}
	}
    }

    /*
     * Subindices only make sense if asked for with -index option set.
     */

    if (returnSubindices && sortInfo.indexc==0) {
	if (startPtr != NULL) {
	    Tcl_DecrRefCount(startPtr);
	}
	Tcl_AppendResult(interp,
		"-subindices cannot be used without -index option", NULL);
	return TCL_ERROR;
    }

    if ((enum modes) mode == REGEXP) {
	/*
	 * We can shimmer regexp/list if listv[i] == pattern, so get the
	 * regexp rep before the list rep. First time round, omit the interp
	 * and hope that the compilation will succeed. If it fails, we'll
	 * recompile in "expensive" mode with a place to put error messages.
	 */

	regexp = Tcl_GetRegExpFromObj(NULL, objv[objc - 1],
		TCL_REG_ADVANCED | TCL_REG_NOSUB |
		(noCase ? TCL_REG_NOCASE : 0));
	if (regexp == NULL) {
	    /*
	     * Failed to compile the RE. Try again without the TCL_REG_NOSUB
	     * flag in case the RE had sub-expressions in it [Bug 1366683].
	     * If this fails, an error message will be left in the
	     * interpreter.
	     */

	    regexp = Tcl_GetRegExpFromObj(interp, objv[objc - 1],
		    TCL_REG_ADVANCED | (noCase ? TCL_REG_NOCASE : 0));
	}

	if (regexp == NULL) {
	    if (startPtr != NULL) {
		Tcl_DecrRefCount(startPtr);
	    }
	    if (sortInfo.indexc > 1) {
		ckfree((char *) sortInfo.indexv);
	    }
	    return TCL_ERROR;
	}
    }

    /*
     * Make sure the list argument is a list object and get its length and a
     * pointer to its array of element pointers.
     */

    result = Tcl_ListObjGetElements(interp, objv[objc - 2], &listc, &listv);
    if (result != TCL_OK) {
	if (startPtr != NULL) {
	    Tcl_DecrRefCount(startPtr);
	}
	if (sortInfo.indexc > 1) {
	    ckfree((char *) sortInfo.indexv);
	}
	return result;
    }

    /*
     * Get the user-specified start offset.
     */

    if (startPtr) {
	result = TclGetIntForIndex(interp, startPtr, listc-1, &offset);
	Tcl_DecrRefCount(startPtr);
	if (result != TCL_OK) {
	    if (sortInfo.indexc > 1) {
		ckfree((char *) sortInfo.indexv);
	    }
	    return result;
	}
	if (offset < 0) {
	    offset = 0;
	}

	/*
	 * If the search started past the end of the list, we just return a
	 * "did not match anything at all" result straight away. [Bug 1374778]
	 */

	if (offset > listc-1) {
	    if (sortInfo.indexc > 1) {
		ckfree((char *) sortInfo.indexv);
	    }
	    if (allMatches || inlineReturn) {
		Tcl_ResetResult(interp);
	    } else {
		Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
	    }
	    return TCL_OK;
	}
    }

    patObj = objv[objc - 1];
    patternBytes = NULL;
    if ((enum modes) mode == EXACT || (enum modes) mode == SORTED) {
	switch ((enum datatypes) dataType) {
	case ASCII:
	case DICTIONARY:
	    patternBytes = Tcl_GetStringFromObj(patObj, &length);
	    break;
	case INTEGER:
	    result = Tcl_GetIntFromObj(interp, patObj, &patInt);
	    if (result != TCL_OK) {
		if (sortInfo.indexc > 1) {
		    ckfree((char *) sortInfo.indexv);
		}
		return result;
	    }
	    break;
	case REAL:
	    result = Tcl_GetDoubleFromObj(interp, patObj, &patDouble);
	    if (result != TCL_OK) {
		if (sortInfo.indexc > 1) {
		    ckfree((char *) sortInfo.indexv);
		}
		return result;
	    }
	    break;
	}
    } else {
	patternBytes = Tcl_GetStringFromObj(patObj, &length);
    }

    /*
     * Set default index value to -1, indicating failure; if we find the item
     * in the course of our search, index will be set to the correct value.
     */

    index = -1;
    match = 0;

    if ((enum modes) mode == SORTED && !allMatches && !negatedMatch) {
	/*
	 * If the data is sorted, we can do a more intelligent search. Note
	 * that there is no point in being smart when -all was specified; in
	 * that case, we have to look at all items anyway, and there is no
	 * sense in doing this when the match sense is inverted.
	 */

	lower = offset - 1;
	upper = listc;
	while (lower + 1 != upper && sortInfo.resultCode == TCL_OK) {
	    i = (lower + upper)/2;
	    itemPtr = SelectObjFromSublist(listv[i], &sortInfo);
	    if (sortInfo.resultCode != TCL_OK) {
		if (sortInfo.indexc > 1) {
		    ckfree((char *) sortInfo.indexv);
		}
		return sortInfo.resultCode;
	    }
	    switch ((enum datatypes) dataType) {
	    case ASCII:
		bytes = TclGetString(itemPtr);
		match = strCmpFn(patternBytes, bytes);
		break;
	    case DICTIONARY:
		bytes = TclGetString(itemPtr);
		match = DictionaryCompare(patternBytes, bytes);
		break;
	    case INTEGER:
		result = Tcl_GetIntFromObj(interp, itemPtr, &objInt);
		if (result != TCL_OK) {
		    if (sortInfo.indexc > 1) {
			ckfree((char *) sortInfo.indexv);
		    }
		    return result;
		}
		if (patInt == objInt) {
		    match = 0;
		} else if (patInt < objInt) {
		    match = -1;
		} else {
		    match = 1;
		}
		break;
	    case REAL:
		result = Tcl_GetDoubleFromObj(interp, itemPtr, &objDouble);
		if (result != TCL_OK) {
		    if (sortInfo.indexc > 1) {
			ckfree((char *) sortInfo.indexv);
		    }
		    return result;
		}
		if (patDouble == objDouble) {
		    match = 0;
		} else if (patDouble < objDouble) {
		    match = -1;
		} else {
		    match = 1;
		}
		break;
	    }
	    if (match == 0) {
		/*
		 * Normally, binary search is written to stop when it finds a
		 * match. If there are duplicates of an element in the list,
		 * our first match might not be the first occurance.
		 * Consider: 0 0 0 1 1 1 2 2 2
		 *
		 * To maintain consistancy with standard lsearch semantics, we
		 * must find the leftmost occurance of the pattern in the
		 * list. Thus we don't just stop searching here. This
		 * variation means that a search always makes log n
		 * comparisons (normal binary search might "get lucky" with an
		 * early comparison).
		 */

		index = i;
		upper = i;
	    } else if (match > 0) {
		if (isIncreasing) {
		    lower = i;
		} else {
		    upper = i;
		}
	    } else {
		if (isIncreasing) {
		    upper = i;
		} else {
		    lower = i;
		}
	    }
	}

    } else {
	/*
	 * We need to do a linear search, because (at least one) of:
	 *   - our matcher can only tell equal vs. not equal
	 *   - our matching sense is negated
	 *   - we're building a list of all matched items
	 */

	if (allMatches) {
	    listPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	}
	for (i = offset; i < listc; i++) {
	    match = 0;
	    itemPtr = SelectObjFromSublist(listv[i], &sortInfo);
	    if (sortInfo.resultCode != TCL_OK) {
		if (listPtr != NULL) {
		    Tcl_DecrRefCount(listPtr);
		}
		if (sortInfo.indexc > 1) {
		    ckfree((char *) sortInfo.indexv);
		}
		return sortInfo.resultCode;
	    }
	    switch ((enum modes) mode) {
	    case SORTED:
	    case EXACT:
		switch ((enum datatypes) dataType) {
		case ASCII:
		    bytes = Tcl_GetStringFromObj(itemPtr, &elemLen);
		    if (length == elemLen) {
			/*
			 * This split allows for more optimal compilation of
			 * memcmp/
			 */

			if (noCase) {
			    match = (strcasecmp(bytes, patternBytes) == 0);
			} else {
			    match = (memcmp(bytes, patternBytes,
				    (size_t) length) == 0);
			}
		    }
		    break;
		case DICTIONARY:
		    bytes = TclGetString(itemPtr);
		    match = (DictionaryCompare(bytes, patternBytes) == 0);
		    break;

		case INTEGER:
		    result = Tcl_GetIntFromObj(interp, itemPtr, &objInt);
		    if (result != TCL_OK) {
			if (listPtr != NULL) {
			    Tcl_DecrRefCount(listPtr);
			}
			if (sortInfo.indexc > 1) {
			    ckfree((char *) sortInfo.indexv);
			}
			return result;
		    }
		    match = (objInt == patInt);
		    break;

		case REAL:
		    result = Tcl_GetDoubleFromObj(interp, itemPtr, &objDouble);
		    if (result != TCL_OK) {
			if (listPtr) {
			    Tcl_DecrRefCount(listPtr);
			}
			if (sortInfo.indexc > 1) {
			    ckfree((char *) sortInfo.indexv);
			}
			return result;
		    }
		    match = (objDouble == patDouble);
		    break;
		}
		break;

	    case GLOB:
		match = Tcl_StringCaseMatch(TclGetString(itemPtr),
			patternBytes, noCase);
		break;
	    case REGEXP:
		match = Tcl_RegExpExecObj(interp, regexp, itemPtr, 0, 0, 0);
		if (match < 0) {
		    Tcl_DecrRefCount(patObj);
		    if (listPtr != NULL) {
			Tcl_DecrRefCount(listPtr);
		    }
		    if (sortInfo.indexc > 1) {
			ckfree((char *) sortInfo.indexv);
		    }
		    return TCL_ERROR;
		}
		break;
	    }

	    /*
	     * Invert match condition for -not
	     */

	    if (negatedMatch) {
		match = !match;
	    }
	    if (!match) {
		continue;
	    }
	    if (!allMatches) {
		index = i;
		break;
	    } else if (inlineReturn) {
		/*
		 * Note that these appends are not expected to fail.
		 */

		if (returnSubindices) {
		    itemPtr = SelectObjFromSublist(listv[i], &sortInfo);
		} else {
		    itemPtr = listv[i];
		}
		Tcl_ListObjAppendElement(interp, listPtr, itemPtr);
	    } else if (returnSubindices) {
		int j;
		itemPtr = Tcl_NewIntObj(i);
		for (j=0 ; j<sortInfo.indexc ; j++) {
		    Tcl_ListObjAppendElement(interp, itemPtr,
			    Tcl_NewIntObj(sortInfo.indexv[j]));
		}
		Tcl_ListObjAppendElement(interp, listPtr, itemPtr);
	    } else {
		Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(i));
	    }
	}
    }

    /*
     * Return everything or a single value.
     */

    if (allMatches) {
	Tcl_SetObjResult(interp, listPtr);
    } else if (!inlineReturn) {
	if (returnSubindices) {
	    int j;
	    itemPtr = Tcl_NewIntObj(index);
	    for (j=0 ; j<sortInfo.indexc ; j++) {
		Tcl_ListObjAppendElement(interp, itemPtr,
			Tcl_NewIntObj(sortInfo.indexv[j]));
	    }
	    Tcl_SetObjResult(interp, itemPtr);
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(index));
	}
    } else if (index < 0) {
	/*
	 * Is this superfluous? The result should be a blank object by
	 * default...
	 */

	Tcl_SetObjResult(interp, Tcl_NewObj());
    } else {
	Tcl_SetObjResult(interp, listv[index]);
    }

    /*
     * Cleanup the index list array.
     */

    if (sortInfo.indexc > 1) {
	ckfree((char *) sortInfo.indexv);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LsetObjCmd --
 *
 *	This procedure is invoked to process the "lset" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LsetObjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument values. */
{
    Tcl_Obj* listPtr;		/* Pointer to the list being altered. */
    Tcl_Obj* finalValuePtr;	/* Value finally assigned to the variable. */

    /*
     * Check parameter count.
     */

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "listVar index ?index...? value");
	return TCL_ERROR;
    }

    /*
     * Look up the list variable's value.
     */

    listPtr = Tcl_ObjGetVar2(interp, objv[1], (Tcl_Obj *) NULL,
	    TCL_LEAVE_ERR_MSG);
    if (listPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Substitute the value in the value. Return either the value or else an
     * unshared copy of it.
     */

    if (objc == 4) {
	finalValuePtr = TclLsetList(interp, listPtr, objv[2], objv[3]);
    } else {
	finalValuePtr = TclLsetFlat(interp, listPtr, objc-3, objv+2,
		objv[objc-1]);
    }

    /*
     * If substitution has failed, bail out.
     */

    if (finalValuePtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Finally, update the variable so that traces fire.
     */

    listPtr = Tcl_ObjSetVar2(interp, objv[1], NULL, finalValuePtr,
	    TCL_LEAVE_ERR_MSG);
    Tcl_DecrRefCount(finalValuePtr);
    if (listPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Return the new value of the variable as the interpreter result.
     */

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LsortObjCmd --
 *
 *	This procedure is invoked to process the "lsort" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LsortObjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument values. */
{
    int i, index, unique, indices;
    Tcl_Obj *resultPtr;
    int length;
    Tcl_Obj *cmdPtr, **listObjPtrs;
    SortElement *elementArray;
    SortElement *elementPtr;
    SortInfo sortInfo;		/* Information about this sort that needs to
				 * be passed to the comparison function. */
    static CONST char *switches[] = {
	"-ascii", "-command", "-decreasing", "-dictionary", "-increasing",
	"-index", "-indices", "-integer", "-nocase", "-real", "-unique",
	(char *) NULL
    };
    enum Lsort_Switches {
	LSORT_ASCII, LSORT_COMMAND, LSORT_DECREASING, LSORT_DICTIONARY,
	LSORT_INCREASING, LSORT_INDEX, LSORT_INDICES, LSORT_INTEGER,
	LSORT_NOCASE, LSORT_REAL, LSORT_UNIQUE
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?options? list");
	return TCL_ERROR;
    }

    /*
     * Parse arguments to set up the mode for the sort.
     */

    sortInfo.isIncreasing = 1;
    sortInfo.sortMode = SORTMODE_ASCII;
    sortInfo.strCmpFn = strcmp;
    sortInfo.indexv = NULL;
    sortInfo.indexc = 0;
    sortInfo.interp = interp;
    sortInfo.resultCode = TCL_OK;
    cmdPtr = NULL;
    unique = 0;
    indices = 0;
    for (i = 1; i < objc-1; i++) {
	if (Tcl_GetIndexFromObj(interp, objv[i], switches, "option", 0,
		&index) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch ((enum Lsort_Switches) index) {
	case LSORT_ASCII:
	    sortInfo.sortMode = SORTMODE_ASCII;
	    break;
	case LSORT_COMMAND:
	    if (i == (objc-2)) {
		if (sortInfo.indexc > 1) {
		    ckfree((char *) sortInfo.indexv);
		}
		Tcl_AppendResult(interp,
			"\"-command\" option must be followed ",
			"by comparison command", NULL);
		return TCL_ERROR;
	    }
	    sortInfo.sortMode = SORTMODE_COMMAND;
	    cmdPtr = objv[i+1];
	    i++;
	    break;
	case LSORT_DECREASING:
	    sortInfo.isIncreasing = 0;
	    break;
	case LSORT_DICTIONARY:
	    sortInfo.sortMode = SORTMODE_DICTIONARY;
	    break;
	case LSORT_INCREASING:
	    sortInfo.isIncreasing = 1;
	    break;
	case LSORT_INDEX: {
	    int j;
	    Tcl_Obj **indices;

	    if (sortInfo.indexc > 1) {
		ckfree((char *) sortInfo.indexv);
	    }
	    if (i == (objc-2)) {
		Tcl_AppendResult(interp, "\"-index\" option must be ",
			"followed by list index", NULL);
		return TCL_ERROR;
	    }

	    /*
	     * Take copy to prevent shimmering problems.
	     */

	    if (Tcl_ListObjGetElements(interp, objv[i+1], &sortInfo.indexc,
		    &indices) != TCL_OK) {
		return TCL_ERROR;
	    }
	    switch (sortInfo.indexc) {
	    case 0:
		sortInfo.indexv = NULL;
		break;
	    case 1:
		sortInfo.indexv = &sortInfo.singleIndex;
		break;
	    default:
		sortInfo.indexv = (int *)
			ckalloc(sizeof(int) * sortInfo.indexc);
	    }

	    /*
	     * Fill the array by parsing each index. We don't know whether
	     * their scale is sensible yet, but we at least perform the
	     * syntactic check here.
	     */

	    for (j=0 ; j<sortInfo.indexc ; j++) {
		if (TclGetIntForIndex(interp, indices[j], SORTIDX_END,
			&sortInfo.indexv[j]) != TCL_OK) {
		    if (sortInfo.indexc > 1) {
			ckfree((char *) sortInfo.indexv);
		    }
		    TclFormatToErrorInfo(interp,
			    "\n    (-index option item number %d)", j);
		    return TCL_ERROR;
		}
	    }
	    i++;
	    break;
	}
	case LSORT_INTEGER:
	    sortInfo.sortMode = SORTMODE_INTEGER;
	    break;
	case LSORT_NOCASE:
	    sortInfo.strCmpFn = strcasecmp;
	    break;
	case LSORT_REAL:
	    sortInfo.sortMode = SORTMODE_REAL;
	    break;
	case LSORT_UNIQUE:
	    unique = 1;
	    break;
	case LSORT_INDICES:
	    indices = 1;
	    break;
	}
    }

    if (sortInfo.sortMode == SORTMODE_COMMAND) {
	/*
	 * The existing command is a list. We want to flatten it, append two
	 * dummy arguments on the end, and replace these arguments later.
	 */

	Tcl_Obj *newCommandPtr = Tcl_DuplicateObj(cmdPtr);
	Tcl_Obj *newObjPtr = Tcl_NewObj();

	Tcl_IncrRefCount(newCommandPtr);
	if (Tcl_ListObjAppendElement(interp, newCommandPtr, newObjPtr)
		!= TCL_OK) {
	    Tcl_DecrRefCount(newCommandPtr);
	    Tcl_IncrRefCount(newObjPtr);
	    Tcl_DecrRefCount(newObjPtr);
	    if (sortInfo.indexc > 1) {
		ckfree((char *) sortInfo.indexv);
	    }
	    return TCL_ERROR;
	}
	Tcl_ListObjAppendElement(interp, newCommandPtr, Tcl_NewObj());
	sortInfo.compareCmdPtr = newCommandPtr;
    }

    sortInfo.resultCode = Tcl_ListObjGetElements(interp, objv[objc-1],
	    &length, &listObjPtrs);
    if (sortInfo.resultCode != TCL_OK || length <= 0) {
	goto done;
    }
    elementArray = (SortElement *) ckalloc(length * sizeof(SortElement));
    for (i=0; i < length; i++){
	elementArray[i].objPtr = listObjPtrs[i];
	elementArray[i].count = 0;
	elementArray[i].nextPtr = &elementArray[i+1];
    }
    elementArray[length-1].nextPtr = NULL;
    elementPtr = MergeSort(elementArray, &sortInfo);
    if (sortInfo.resultCode == TCL_OK) {
	resultPtr = Tcl_NewObj();
	if (unique) {
	    if (indices) {
		for (; elementPtr != NULL ; elementPtr = elementPtr->nextPtr) {
		    if (elementPtr->count == 0) {
			Tcl_ListObjAppendElement(interp, resultPtr,
				Tcl_NewIntObj(elementPtr - &elementArray[0]));
		    }
		}
	    } else {
		for (; elementPtr != NULL; elementPtr = elementPtr->nextPtr) {
		    if (elementPtr->count == 0) {
			Tcl_ListObjAppendElement(interp, resultPtr,
				elementPtr->objPtr);
		    }
		}
	    }
	} else {
	    if (indices) {
		for (; elementPtr != NULL ; elementPtr = elementPtr->nextPtr) {
		    Tcl_ListObjAppendElement(interp, resultPtr,
			    Tcl_NewIntObj(elementPtr - &elementArray[0]));
		}
	    } else {
		for (; elementPtr != NULL; elementPtr = elementPtr->nextPtr){
		    Tcl_ListObjAppendElement(interp, resultPtr,
			    elementPtr->objPtr);
		}
	    }
	}
	Tcl_SetObjResult(interp, resultPtr);
    }
    ckfree((char*) elementArray);

    done:
    if (sortInfo.sortMode == SORTMODE_COMMAND) {
	Tcl_DecrRefCount(sortInfo.compareCmdPtr);
	sortInfo.compareCmdPtr = NULL;
    }
    if (sortInfo.indexc > 1) {
	ckfree((char *) sortInfo.indexv);
    }
    return sortInfo.resultCode;
}

/*
 *----------------------------------------------------------------------
 *
 * MergeSort -
 *
 *	This procedure sorts a linked list of SortElement structures use the
 *	merge-sort algorithm.
 *
 * Results:
 *	A pointer to the head of the list after sorting is returned.
 *
 * Side effects:
 *	None, unless a user-defined comparison command does something weird.
 *
 *----------------------------------------------------------------------
 */

static SortElement *
MergeSort(headPtr, infoPtr)
    SortElement *headPtr;	/* First element on the list. */
    SortInfo *infoPtr;		/* Information needed by the comparison
				 * operator. */
{
    /*
     * The subList array below holds pointers to temporary lists built during
     * the merge sort. Element i of the array holds a list of length 2**i.
     */

#   define NUM_LISTS 30
    SortElement *subList[NUM_LISTS];
    SortElement *elementPtr;
    int i;

    for (i=0 ; i<NUM_LISTS ; i++) {
	subList[i] = NULL;
    }
    while (headPtr != NULL) {
	elementPtr = headPtr;
	headPtr = headPtr->nextPtr;
	elementPtr->nextPtr = 0;
	for (i=0 ; i<NUM_LISTS && subList[i]!=NULL ; i++) {
	    elementPtr = MergeLists(subList[i], elementPtr, infoPtr);
	    subList[i] = NULL;
	}
	if (i >= NUM_LISTS) {
	    i = NUM_LISTS-1;
	}
	subList[i] = elementPtr;
    }
    elementPtr = NULL;
    for (i=0 ; i<NUM_LISTS ; i++) {
	elementPtr = MergeLists(subList[i], elementPtr, infoPtr);
    }
    return elementPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * MergeLists -
 *
 *	This procedure combines two sorted lists of SortElement structures
 *	into a single sorted list.
 *
 * Results:
 *	The unified list of SortElement structures.
 *
 * Side effects:
 *	None, unless a user-defined comparison command does something weird.
 *
 *----------------------------------------------------------------------
 */

static SortElement *
MergeLists(leftPtr, rightPtr, infoPtr)
    SortElement *leftPtr;	/* First list to be merged; may be NULL. */
    SortElement *rightPtr;	/* Second list to be merged; may be NULL. */
    SortInfo *infoPtr;		/* Information needed by the comparison
				 * operator. */
{
    SortElement *headPtr;
    SortElement *tailPtr;
    int cmp;

    if (leftPtr == NULL) {
	return rightPtr;
    }
    if (rightPtr == NULL) {
	return leftPtr;
    }
    cmp = SortCompare(leftPtr->objPtr, rightPtr->objPtr, infoPtr);
    if (cmp > 0) {
	tailPtr = rightPtr;
	rightPtr = rightPtr->nextPtr;
    } else {
	if (cmp == 0) {
	    leftPtr->count++;
	}
	tailPtr = leftPtr;
	leftPtr = leftPtr->nextPtr;
    }
    headPtr = tailPtr;
    while ((leftPtr != NULL) && (rightPtr != NULL)) {
	cmp = SortCompare(leftPtr->objPtr, rightPtr->objPtr, infoPtr);
	if (cmp > 0) {
	    tailPtr->nextPtr = rightPtr;
	    tailPtr = rightPtr;
	    rightPtr = rightPtr->nextPtr;
	} else {
	    if (cmp == 0) {
		leftPtr->count++;
	    }
	    tailPtr->nextPtr = leftPtr;
	    tailPtr = leftPtr;
	    leftPtr = leftPtr->nextPtr;
	}
    }
    if (leftPtr != NULL) {
	tailPtr->nextPtr = leftPtr;
    } else {
	tailPtr->nextPtr = rightPtr;
    }
    return headPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * SortCompare --
 *
 *	This procedure is invoked by MergeLists to determine the proper
 *	ordering between two elements.
 *
 * Results:
 *	A negative results means the the first element comes before the
 *	second, and a positive results means that the second element should
 *	come first. A result of zero means the two elements are equal and it
 *	doesn't matter which comes first.
 *
 * Side effects:
 *	None, unless a user-defined comparison command does something weird.
 *
 *----------------------------------------------------------------------
 */

static int
SortCompare(objPtr1, objPtr2, infoPtr)
    Tcl_Obj *objPtr1, *objPtr2;	/* Values to be compared. */
    SortInfo *infoPtr;		/* Information passed from the top-level
				 * "lsort" command. */
{
    int order;

    order = 0;
    if (infoPtr->resultCode != TCL_OK) {
	/*
	 * Once an error has occurred, skip any future comparisons so as to
	 * preserve the error message in sortInterp->result.
	 */

	return order;
    }

    objPtr1 = SelectObjFromSublist(objPtr1, infoPtr);
    if (infoPtr->resultCode != TCL_OK) {
	return order;
    }
    objPtr2 = SelectObjFromSublist(objPtr2, infoPtr);
    if (infoPtr->resultCode != TCL_OK) {
	return order;
    }

    if (infoPtr->sortMode == SORTMODE_ASCII) {
	order = infoPtr->strCmpFn(TclGetString(objPtr1), TclGetString(objPtr2));
    } else if (infoPtr->sortMode == SORTMODE_DICTIONARY) {
	order = DictionaryCompare(
		TclGetString(objPtr1), TclGetString(objPtr2));
    } else if (infoPtr->sortMode == SORTMODE_INTEGER) {
	long a, b;

	if ((Tcl_GetLongFromObj(infoPtr->interp, objPtr1, &a) != TCL_OK)
		|| (Tcl_GetLongFromObj(infoPtr->interp, objPtr2, &b)
		!= TCL_OK)) {
	    infoPtr->resultCode = TCL_ERROR;
	    return order;
	}
	if (a > b) {
	    order = 1;
	} else if (b > a) {
	    order = -1;
	}
    } else if (infoPtr->sortMode == SORTMODE_REAL) {
	double a, b;

	if (Tcl_GetDoubleFromObj(infoPtr->interp, objPtr1, &a) != TCL_OK
		|| Tcl_GetDoubleFromObj(infoPtr->interp,objPtr2,&b) != TCL_OK){
	    infoPtr->resultCode = TCL_ERROR;
	    return order;
	}
	if (a > b) {
	    order = 1;
	} else if (b > a) {
	    order = -1;
	}
    } else {
	Tcl_Obj **objv, *paramObjv[2];
	int objc;

	paramObjv[0] = objPtr1;
	paramObjv[1] = objPtr2;

	/*
	 * We made space in the command list for the two things to compare.
	 * Replace them and evaluate the result.
	 */

	Tcl_ListObjLength(infoPtr->interp, infoPtr->compareCmdPtr, &objc);
	Tcl_ListObjReplace(infoPtr->interp, infoPtr->compareCmdPtr, objc - 2,
		2, 2, paramObjv);
	Tcl_ListObjGetElements(infoPtr->interp, infoPtr->compareCmdPtr,
		&objc, &objv);

	infoPtr->resultCode = Tcl_EvalObjv(infoPtr->interp, objc, objv, 0);

	if (infoPtr->resultCode != TCL_OK) {
	    Tcl_AddErrorInfo(infoPtr->interp,
		    "\n    (-compare command)");
	    return order;
	}

	/*
	 * Parse the result of the command.
	 */

	if (Tcl_GetIntFromObj(infoPtr->interp,
		Tcl_GetObjResult(infoPtr->interp), &order) != TCL_OK) {
	    Tcl_ResetResult(infoPtr->interp);
	    Tcl_AppendResult(infoPtr->interp,
		    "-compare command returned non-integer result", NULL);
	    infoPtr->resultCode = TCL_ERROR;
	    return order;
	}
    }
    if (!infoPtr->isIncreasing) {
	order = -order;
    }
    return order;
}

/*
 *----------------------------------------------------------------------
 *
 * DictionaryCompare
 *
 *	This function compares two strings as if they were being used in an
 *	index or card catalog. The case of alphabetic characters is ignored,
 *	except to break ties. Thus "B" comes before "b" but after "a". Also,
 *	integers embedded in the strings compare in numerical order. In other
 *	words, "x10y" comes after "x9y", not * before it as it would when
 *	using strcmp().
 *
 * Results:
 *	A negative result means that the first element comes before the
 *	second, and a positive result means that the second element should
 *	come first. A result of zero means the two elements are equal and it
 *	doesn't matter which comes first.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
DictionaryCompare(left, right)
    char *left, *right;		/* The strings to compare. */
{
    Tcl_UniChar uniLeft, uniRight, uniLeftLower, uniRightLower;
    int diff, zeros;
    int secondaryDiff = 0;

    while (1) {
	if (isdigit(UCHAR(*right))		/* INTL: digit */
		&& isdigit(UCHAR(*left))) {	/* INTL: digit */
	    /*
	     * There are decimal numbers embedded in the two strings. Compare
	     * them as numbers, rather than strings. If one number has more
	     * leading zeros than the other, the number with more leading
	     * zeros sorts later, but only as a secondary choice.
	     */

	    zeros = 0;
	    while ((*right == '0') && (isdigit(UCHAR(right[1])))) {
		right++;
		zeros--;
	    }
	    while ((*left == '0') && (isdigit(UCHAR(left[1])))) {
		left++;
		zeros++;
	    }
	    if (secondaryDiff == 0) {
		secondaryDiff = zeros;
	    }

	    /*
	     * The code below compares the numbers in the two strings without
	     * ever converting them to integers. It does this by first
	     * comparing the lengths of the numbers and then comparing the
	     * digit values.
	     */

	    diff = 0;
	    while (1) {
		if (diff == 0) {
		    diff = UCHAR(*left) - UCHAR(*right);
		}
		right++;
		left++;
		if (!isdigit(UCHAR(*right))) {		/* INTL: digit */
		    if (isdigit(UCHAR(*left))) {	/* INTL: digit */
			return 1;
		    } else {
			/*
			 * The two numbers have the same length. See if their
			 * values are different.
			 */

			if (diff != 0) {
			    return diff;
			}
			break;
		    }
		} else if (!isdigit(UCHAR(*left))) {	/* INTL: digit */
		    return -1;
		}
	    }
	    continue;
	}

	/*
	 * Convert character to Unicode for comparison purposes. If either
	 * string is at the terminating null, do a byte-wise comparison and
	 * bail out immediately.
	 */

	if ((*left != '\0') && (*right != '\0')) {
	    left += Tcl_UtfToUniChar(left, &uniLeft);
	    right += Tcl_UtfToUniChar(right, &uniRight);

	    /*
	     * Convert both chars to lower for the comparison, because
	     * dictionary sorts are case insensitve. Covert to lower, not
	     * upper, so chars between Z and a will sort before A (where most
	     * other interesting punctuations occur)
	     */

	    uniLeftLower = Tcl_UniCharToLower(uniLeft);
	    uniRightLower = Tcl_UniCharToLower(uniRight);
	} else {
	    diff = UCHAR(*left) - UCHAR(*right);
	    break;
	}

	diff = uniLeftLower - uniRightLower;
	if (diff) {
	    return diff;
	} else if (secondaryDiff == 0) {
	    if (Tcl_UniCharIsUpper(uniLeft) && Tcl_UniCharIsLower(uniRight)) {
		secondaryDiff = -1;
	    } else if (Tcl_UniCharIsUpper(uniRight)
		    && Tcl_UniCharIsLower(uniLeft)) {
		secondaryDiff = 1;
	    }
	}
    }
    if (diff == 0) {
	diff = secondaryDiff;
    }
    return diff;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectObjFromSublist --
 *
 *	This procedure is invoked from lsearch and SortCompare. It is used
 *	for implementing the -index option, for the lsort and lsearch
 *	commands.
 *
 * Results:
 *	Returns NULL if a failure occurs, and sets the result in the infoPtr.
 *	Otherwise returns the Tcl_Obj* to the item.
 *
 * Side effects:
 *	None.
 *
 * Note:
 *	No reference counting is done, as the result is only used internally
 *	and never passed directly to user code.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
SelectObjFromSublist(objPtr, infoPtr)
    Tcl_Obj *objPtr;		/* Obj to select sublist from. */
    SortInfo *infoPtr;		/* Information passed from the top-level
				 * "lsearch" or "lsort" command. */
{
    int i;

    /*
     * Quick check for case when no "-index" option is there.
     */

    if (infoPtr->indexc == 0) {
	return objPtr;
    }

    /*
     * Iterate over the indices, traversing through the nested sublists as we
     * go.
     */

    for (i=0 ; i<infoPtr->indexc ; i++) {
	int listLen, index;
	Tcl_Obj *currentObj;

	if (Tcl_ListObjLength(infoPtr->interp, objPtr,
		&listLen) != TCL_OK) {
	    infoPtr->resultCode = TCL_ERROR;
	    return NULL;
	}
	index = infoPtr->indexv[i];

	/*
	 * Adjust for end-based indexing.
	 */

	if (index < SORTIDX_NONE) {
	    index += listLen + 1;
	}

	if (Tcl_ListObjIndex(infoPtr->interp, objPtr, index,
		&currentObj) != TCL_OK) {
	    infoPtr->resultCode = TCL_ERROR;
	    return NULL;
	}
	if (currentObj == NULL) {
	    char buffer[TCL_INTEGER_SPACE];
	    TclFormatInt(buffer, index);
	    Tcl_AppendResult(infoPtr->interp,
		    "element ", buffer, " missing from sublist \"",
		    TclGetString(objPtr), "\"", (char *) NULL);
	    infoPtr->resultCode = TCL_ERROR;
	    return NULL;
	}
	objPtr = currentObj;
    }
    return objPtr;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
