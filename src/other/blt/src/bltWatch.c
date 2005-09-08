/*
 * bltWatch.c --
 *
 * 	This module implements watch procedure callbacks for Tcl
 *	commands and procedures.
 *
 * Copyright 1994-1998 Lucent Technologies, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 *
 * The "watch" command was created by George Howlett.
 */

#include "bltInt.h"
#include <bltHash.h>
#include "bltSwitch.h"

#define UNKNOWN_RETURN_CODE	5
static char *codeNames[] =
{
    "OK", "ERROR", "RETURN", "BREAK", "CONTINUE"
};

#define WATCH_MAX_LEVEL	10000	/* Maximum depth of Tcl traces. */

enum WatchStates {
    WATCH_STATE_DONT_CARE = -1,	/* Select watch regardless of state */
    WATCH_STATE_IDLE = 0,	/*  */
    WATCH_STATE_ACTIVE = 1
};

typedef struct {
    Tcl_Interp *interp;		/* Interpreter associated with the watch */
    Blt_Uid nameId;		/* Watch identifier */

    /* User-configurable fields */
    enum WatchStates state;	/* Current state of watch: either
				 * WATCH_STATE_IDLE or WATCH_STATE_ACTIVE */
    int maxLevel;		/* Maximum depth of tracing allowed */
    char **preCmd;		/* Procedure to be invoked before the
				 * command is executed (but after
				 * substitutions have occurred). */
    char **postCmd;		/* Procedure to be invoked after the command
				 * is executed. */
    Tcl_Trace trace;		/* Trace handler which activates "pre"
				 * command procedures */
    Tcl_AsyncHandler asyncHandle; /* Async handler which triggers the
				 * "post" command procedure (if one
				 * exists) */
    int active;			/* Indicates if a trace is currently
				 * active.  This prevents recursive
				 * tracing of the "pre" and "post"
				 * procedures. */
    int level;			/* Current level of traced command. */
    char *cmdPtr;		/* Command string before substitutions.
				 * Points to a original command buffer. */
    char *args;			/* Tcl list of the command after
				 * substitutions. List is malloc-ed by
				 * Tcl_Merge. Must be freed in handler
				 * procs */
} Watch;

typedef struct {
    Blt_Uid nameId;		/* Name identifier of the watch */
    Tcl_Interp *interp;		/* Interpreter associated with the
				 * watch */
} WatchKey;

static Blt_HashTable watchTable;
static int refCount = 0;

static Blt_SwitchSpec switchSpecs[] = 
{
    {BLT_SWITCH_LIST, "-precmd", Blt_Offset(Watch, preCmd), 0},
    {BLT_SWITCH_LIST, "-postcmd", Blt_Offset(Watch, postCmd), 0},
    {BLT_SWITCH_BOOLEAN, "-active", Blt_Offset(Watch, state), 0},
    {BLT_SWITCH_INT_NONNEGATIVE, "-maxlevel", Blt_Offset(Watch, maxLevel), 0},
    {BLT_SWITCH_END, NULL, 0, 0}
};

static Tcl_CmdTraceProc PreCmdProc;
static Tcl_AsyncProc PostCmdProc;
static Tcl_CmdProc WatchCmd;
static Tcl_CmdDeleteProc WatchDeleteCmd;

/*
 *----------------------------------------------------------------------
 *
 * PreCmdProc --
 *
 *	Procedure callback for Tcl_Trace. Gets called before the
 * 	command is executed, but after substitutions have occurred.
 *	If a watch procedure is active, it evals a Tcl command.
 *	Activates the "precmd" callback, if one exists.
 *
 *	Stashes some information for the "pre" callback: command
 *	string, substituted argument list, and current level.
 *
 * 	Format of "pre" proc:
 *
 * 	proc beforeCmd { level cmdStr argList } {
 *
 * 	}
 *
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A Tcl_AsyncHandler may be triggered, if a "post" procedure is
 *	defined.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
PreCmdProc(clientData, interp, level, command, cmdProc, cmdClientData,
    argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Not used. */
    int level;			/* Current level */
    char *command;		/* Command before substitution */
    Tcl_CmdProc *cmdProc;	/* Not used. */
    ClientData cmdClientData;	/* Not used. */
    int argc;
    char **argv;		/* Command after parsing, but before
				 * evaluation */
{
    Watch *watchPtr = clientData;

    if (watchPtr->active) {
	return;			/* Don't re-enter from Tcl_Eval below */
    }
    watchPtr->cmdPtr = command;
    watchPtr->level = level;
    /*
     * There's no guarantee that the calls to PreCmdProc will match
     * up with PostCmdProc.  So free up argument lists that are still
     * hanging around before allocating a new one.
     */
    if (watchPtr->args != NULL) {
	Blt_Free(watchPtr->args);
    }
    watchPtr->args = Tcl_Merge(argc, argv);

    if (watchPtr->preCmd != NULL) {
	Tcl_DString buffer;
	char string[200];
	int status;
	register char **p;

	/* Create the "pre" command procedure call */
	Tcl_DStringInit(&buffer);
	for (p = watchPtr->preCmd; *p != NULL; p++) {
	    Tcl_DStringAppendElement(&buffer, *p);
	}
	sprintf(string, "%d", watchPtr->level);
	Tcl_DStringAppendElement(&buffer, string);
	Tcl_DStringAppendElement(&buffer, watchPtr->cmdPtr);
	Tcl_DStringAppendElement(&buffer, watchPtr->args);

	watchPtr->active = 1;
	status = Tcl_Eval(interp, Tcl_DStringValue(&buffer));
	watchPtr->active = 0;

	Tcl_DStringFree(&buffer);
	if (status != TCL_OK) {
	    fprintf(stderr, "%s failed: %s\n", watchPtr->preCmd[0],
		Tcl_GetStringResult(interp));
	}
    }
    /* Set the trigger for the "post" command procedure */
    if (watchPtr->postCmd != NULL) {
	Tcl_AsyncMark(watchPtr->asyncHandle);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PostCmdProc --
 *
 *	Procedure callback for Tcl_AsyncHandler. Gets called after
 *	the command has executed.  It tests for a "post" command, but
 *	you really can't get here, if one doesn't exist.
 *
 *	Save the current contents of interp->result before calling
 *	the "post" command, and restore it afterwards.
 *
 * 	Format of "post" proc:
 *
 * 	proc afterCmd { level cmdStr argList retCode results } {
 *
 *	}
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Memory for argument list is released.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PostCmdProc(clientData, interp, code)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Not used. */
    int code;			/* Completion code of command */
{
    Watch *watchPtr = clientData;

    if (watchPtr->active) {
	return code;
    }
    if (watchPtr->postCmd != NULL) {
	int status;
	Tcl_DString buffer;
	char string[200];
	char *results;
	register char **p;
	char *retCode;
	char *errorCode, *errorInfo;
	errorInfo = errorCode = NULL;

	results = "NO INTERPRETER AVAILABLE";

	/*
	 * ----------------------------------------------------
	 *
	 * Save the state of the interpreter.
	 *
	 * ----------------------------------------------------
	 */
	if (interp != NULL) {
	    errorInfo = (char *)Tcl_GetVar2(interp, "errorInfo", (char *)NULL,
		TCL_GLOBAL_ONLY);
	    if (errorInfo != NULL) {
		errorInfo = Blt_Strdup(errorInfo);
	    }
	    errorCode = (char *)Tcl_GetVar2(interp, "errorCode", (char *)NULL,
		TCL_GLOBAL_ONLY);
	    if (errorCode != NULL) {
		errorCode = Blt_Strdup(errorCode);
	    }
	    results = Blt_Strdup(Tcl_GetStringResult(interp));
	}
	/* Create the "post" command procedure call */
	Tcl_DStringInit(&buffer);
	for (p = watchPtr->postCmd; *p != NULL; p++) {
	    Tcl_DStringAppendElement(&buffer, *p);
	}
	sprintf(string, "%d", watchPtr->level);
	Tcl_DStringAppendElement(&buffer, string);
	Tcl_DStringAppendElement(&buffer, watchPtr->cmdPtr);
	Tcl_DStringAppendElement(&buffer, watchPtr->args);
	if (code < UNKNOWN_RETURN_CODE) {
	    retCode = codeNames[code];
	} else {
	    sprintf(string, "%d", code);
	    retCode = string;
	}
	Tcl_DStringAppendElement(&buffer, retCode);
	Tcl_DStringAppendElement(&buffer, results);

	watchPtr->active = 1;
	status = Tcl_Eval(watchPtr->interp, Tcl_DStringValue(&buffer));
	watchPtr->active = 0;

	Tcl_DStringFree(&buffer);
	Blt_Free(watchPtr->args);
	watchPtr->args = NULL;

	if (status != TCL_OK) {
	    fprintf(stderr, "%s failed: %s\n", watchPtr->postCmd[0],
		Tcl_GetStringResult(watchPtr->interp));
	}
	/*
	 * ----------------------------------------------------
	 *
	 * Restore the state of the interpreter.
	 *
	 * ----------------------------------------------------
	 */
	if (interp != NULL) {
	    if (errorInfo != NULL) {
		Tcl_SetVar2(interp, "errorInfo", (char *)NULL, errorInfo,
		    TCL_GLOBAL_ONLY);
		Blt_Free(errorInfo);
	    }
	    if (errorCode != NULL) {
		Tcl_SetVar2(interp, "errorCode", (char *)NULL, errorCode,
		    TCL_GLOBAL_ONLY);
		Blt_Free(errorCode);
	    }
	    Tcl_SetResult(interp, results, TCL_DYNAMIC);
	}
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * NewWatch --
 *
 *	Creates a new watch. Uses the nameId and interpreter
 *	address to create a unique hash key.  The new watch is
 *	registered into the "watchTable" hash table. Also creates a
 *	Tcl_AsyncHandler for triggering "post" events.
 *
 * Results:
 *	If memory for the watch could be allocated, a pointer to
 *	the new watch is returned.  Otherwise NULL, and interp->result
 *	points to an error message.
 *
 * Side Effects:
 *	A new Tcl_AsyncHandler is created. A new hash table entry
 *	is created. Memory the watch structure is allocated.
 *
 *----------------------------------------------------------------------
 */
static Watch *
NewWatch(interp, name)
    Tcl_Interp *interp;
    char *name;
{
    Watch *watchPtr;
    WatchKey key;
    Blt_HashEntry *hPtr;
    int dummy;

    watchPtr = Blt_Calloc(1, sizeof(Watch));
    if (watchPtr == NULL) {
	Tcl_AppendResult(interp, "can't allocate watch structure", (char *)NULL);
	return NULL;
    }
    watchPtr->state = WATCH_STATE_ACTIVE;
    watchPtr->maxLevel = WATCH_MAX_LEVEL;
    watchPtr->nameId = Blt_GetUid(name);
    watchPtr->interp = interp;
    watchPtr->asyncHandle = Tcl_AsyncCreate(PostCmdProc, watchPtr);
    key.interp = interp;
    key.nameId = watchPtr->nameId;
    hPtr = Blt_CreateHashEntry(&watchTable, (char *)&key, &dummy);
    Blt_SetHashValue(hPtr, watchPtr);
    return watchPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyWatch --
 *
 *	Removes the watch. The resources used by the watch
 *	are released.
 *	  1) If the watch is active, its trace is deleted.
 *	  2) Memory for command strings is free-ed.
 *	  3) Entry is removed from watch registry.
 *	  4) Async handler is deleted.
 *	  5) Memory for watch itself is released.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Everything associated with the watch is freed.
 *
 *----------------------------------------------------------------------
 */
static void
DestroyWatch(watchPtr)
    Watch *watchPtr;
{
    WatchKey key;
    Blt_HashEntry *hPtr;

    Tcl_AsyncDelete(watchPtr->asyncHandle);
    if (watchPtr->state == WATCH_STATE_ACTIVE) {
	Tcl_DeleteTrace(watchPtr->interp, watchPtr->trace);
    }
    if (watchPtr->preCmd != NULL) {
	Blt_Free(watchPtr->preCmd);
    }
    if (watchPtr->postCmd != NULL) {
	Blt_Free(watchPtr->postCmd);
    }
    if (watchPtr->args != NULL) {
	Blt_Free(watchPtr->args);
    }
    key.interp = watchPtr->interp;
    key.nameId = watchPtr->nameId;
    hPtr = Blt_FindHashEntry(&watchTable, (char *)&key);
    Blt_DeleteHashEntry(&watchTable, hPtr);
    Blt_FreeUid(key.nameId);
    Blt_Free(watchPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * NameToWatch --
 *
 *	Searches for the watch represented by the watch name and its
 *	associated interpreter in its directory.
 *
 * Results:
 *	If found, the pointer to the watch structure is returned,
 *	otherwise NULL. If requested, interp-result will be filled
 *	with an error message.
 *
 *----------------------------------------------------------------------
 */
static Watch *
NameToWatch(interp, name, flags)
    Tcl_Interp *interp;
    char *name;
    int flags;
{
    WatchKey key;
    Blt_HashEntry *hPtr;

    key.interp = interp;
    key.nameId = Blt_FindUid(name);
    if (key.nameId != NULL) {
	hPtr = Blt_FindHashEntry(&watchTable, (char *)&key);
	if (hPtr != NULL) {
	    return (Watch *) Blt_GetHashValue(hPtr);
	}
    }
    if (flags & TCL_LEAVE_ERR_MSG) {
	Tcl_AppendResult(interp, "can't find any watch named \"", name, "\"",
	    (char *)NULL);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ListWatches --
 *
 *	Creates a list of all watches in the interpreter.  The
 *	list search may be restricted to selected states by
 *	setting "state" to something other than WATCH_STATE_DONT_CARE.
 *
 * Results:
 *	A standard Tcl result.  Interp->result will contain a list
 *	of all watches matches the state criteria.
 *
 *----------------------------------------------------------------------
 */
static int
ListWatches(interp, state)
    Tcl_Interp *interp;
    enum WatchStates state;	/* Active flag */
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    register Watch *watchPtr;

    for (hPtr = Blt_FirstHashEntry(&watchTable, &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	watchPtr = (Watch *)Blt_GetHashValue(hPtr);
	if (watchPtr->interp == interp) {
	    if ((state == WATCH_STATE_DONT_CARE) ||
		(state == watchPtr->state)) {
		Tcl_AppendElement(interp, (char *)watchPtr->nameId);
	    }
	}
    }
    return TCL_OK;
}
/*
 *----------------------------------------------------------------------
 *
 * ConfigWatch --
 *
 *	Processes argument list of switches and values, setting
 *	Watch fields.
 *
 * Results:
 *	If found, the pointer to the watch structure is returned,
 *	otherwise NULL. If requested, interp-result will be filled
 *	with an error message.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigWatch(watchPtr, interp, argc, argv)
    Watch *watchPtr;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    if (Blt_ProcessSwitches(interp, switchSpecs, argc, argv, (char *)watchPtr,
	 0) < 0) {
	return TCL_ERROR;
    }
    /*
     * If the watch's max depth changed or its state, reset the traces.
     */
    if (watchPtr->trace != (Tcl_Trace) 0) {
	Tcl_DeleteTrace(interp, watchPtr->trace);
	watchPtr->trace = (Tcl_Trace) 0;
    }
    if (watchPtr->state == WATCH_STATE_ACTIVE) {
	watchPtr->trace = Tcl_CreateTrace(interp, watchPtr->maxLevel,
	    PreCmdProc, watchPtr);
    }
    return TCL_OK;
}

/* Tcl interface routines */
/*
 *----------------------------------------------------------------------
 *
 * CreateOp --
 *
 *	Creates a new watch and processes any extra switches.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	A new watch is created.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CreateOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register Watch *watchPtr;

    watchPtr = NameToWatch(interp, argv[2], 0);
    if (watchPtr != NULL) {
	Tcl_AppendResult(interp, "a watch \"", argv[2], "\" already exists",
	    (char *)NULL);
	return TCL_ERROR;
    }
    watchPtr = NewWatch(interp, argv[2]);
    if (watchPtr == NULL) {
	return TCL_ERROR;	/* Can't create new watch */
    }
    return ConfigWatch(watchPtr, interp, argc - 3, argv + 3);
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteOp --
 *
 *	Deletes the watch.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeleteOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register Watch *watchPtr;

    watchPtr = NameToWatch(interp, argv[2], TCL_LEAVE_ERR_MSG);
    if (watchPtr == NULL) {
	return TCL_ERROR;
    }
    DestroyWatch(watchPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ActivateOp --
 *
 *	Activate/deactivates the named watch.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ActivateOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register Watch *watchPtr;
    enum WatchStates state;

    state = (argv[1][0] == 'a') ? WATCH_STATE_ACTIVE : WATCH_STATE_IDLE;
    watchPtr = NameToWatch(interp, argv[2], TCL_LEAVE_ERR_MSG);
    if (watchPtr == NULL) {
	return TCL_ERROR;
    }
    if (state != watchPtr->state) {
	if (watchPtr->trace == (Tcl_Trace) 0) {
	    watchPtr->trace = Tcl_CreateTrace(interp, watchPtr->maxLevel,
		PreCmdProc, watchPtr);
	} else {
	    Tcl_DeleteTrace(interp, watchPtr->trace);
	    watchPtr->trace = (Tcl_Trace) 0;
	}
	watchPtr->state = state;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamesOp --
 *
 *	Returns the names of all watches in the interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
NamesOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    enum WatchStates state;

    state = WATCH_STATE_DONT_CARE;
    if (argc == 3) {
	char c;
	c = argv[2][0];
	if ((c == 'a') && (strcmp(argv[2], "active") == 0)) {
	    state = WATCH_STATE_ACTIVE;
	} else if ((c == 'i') && (strcmp(argv[2], "idle") == 0)) {
	    state = WATCH_STATE_IDLE;
	} else if ((c == 'i') && (strcmp(argv[2], "ignore") == 0)) {
	    state = WATCH_STATE_DONT_CARE;
	} else {
	    Tcl_AppendResult(interp, "bad state \"", argv[2], "\" should be \
\"active\", \"idle\", or \"ignore\"", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    return ListWatches(interp, state);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 *	Convert the range of the pixel values allowed into a list.
 *
 * Results:
 *	The string representation of the limits is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ConfigureOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register Watch *watchPtr;

    watchPtr = NameToWatch(interp, argv[2], TCL_LEAVE_ERR_MSG);
    if (watchPtr == NULL) {
	return TCL_ERROR;
    }
    return ConfigWatch(watchPtr, interp, argc - 3, argv + 3);
}

/*
 *----------------------------------------------------------------------
 *
 * InfoOp --
 *
 *	Convert the limits of the pixel values allowed into a list.
 *
 * Results:
 *	The string representation of the limits is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InfoOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register Watch *watchPtr;
    char string[200];
    register char **p;

    watchPtr = NameToWatch(interp, argv[2], TCL_LEAVE_ERR_MSG);
    if (watchPtr == NULL) {
	return TCL_ERROR;
    }
    if (watchPtr->preCmd != NULL) {
	Tcl_AppendResult(interp, "-precmd", (char *)NULL);
	for (p = watchPtr->preCmd; *p != NULL; p++) {
	    Tcl_AppendResult(interp, " ", *p, (char *)NULL);
	}
    }
    if (watchPtr->postCmd != NULL) {
	Tcl_AppendResult(interp, "-postcmd", (char *)NULL);
	for (p = watchPtr->postCmd; *p != NULL; p++) {
	    Tcl_AppendResult(interp, " ", *p, (char *)NULL);
	}
    }
    sprintf(string, "%d", watchPtr->maxLevel);
    Tcl_AppendResult(interp, "-maxlevel ", string, " ", (char *)NULL);
    Tcl_AppendResult(interp, "-active ",
	(watchPtr->state == WATCH_STATE_ACTIVE)
	? "true" : "false", " ", (char *)NULL);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * WatchCmd --
 *
 *	This procedure is invoked to process the Tcl "blt_watch"
 *	command. See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

static Blt_OpSpec watchOps[] =
{
    {"activate", 1, (Blt_Op)ActivateOp, 3, 3, "watchName",},
    {"configure", 2, (Blt_Op)ConfigureOp, 3, 0,
	"watchName ?options...?"},
    {"create", 2, (Blt_Op)CreateOp, 3, 0, "watchName ?switches?",},
    {"deactivate", 3, (Blt_Op)ActivateOp, 3, 3, "watchName",},
    {"delete", 3, (Blt_Op)DeleteOp, 3, 3, "watchName",},
    {"info", 1, (Blt_Op)InfoOp, 3, 3, "watchName",},
    {"names", 1, (Blt_Op)NamesOp, 2, 3, "?state?",},
};
static int nWatchOps = sizeof(watchOps) / sizeof(Blt_OpSpec);

/*ARGSUSED*/
static int
WatchCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nWatchOps, watchOps, BLT_OP_ARG1, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (clientData, interp, argc, argv);
    return result;
}

/* ARGSUSED */
static void
WatchDeleteCmd(clientData)
    ClientData clientData;	/* Not Used. */
{
    refCount--;
    if (refCount == 0) {
	Blt_DeleteHashTable(&watchTable);
    }
}

/* Public initialization routine */
/*
 *--------------------------------------------------------------
 *
 * Blt_WatchInit --
 *
 *	This procedure is invoked to initialize the Tcl command
 *	"blt_watch".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates the new command and adds a new entry into a
 *	global	Tcl associative array.
 *
 *--------------------------------------------------------------
 */
int
Blt_WatchInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec =
    {"watch", WatchCmd, WatchDeleteCmd};

    if (refCount == 0) {
	Blt_InitHashTable(&watchTable, sizeof(WatchKey) / sizeof(int));
    }
    refCount++;

    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}
