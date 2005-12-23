/*
 * bltNsUtil.c --
 *
 *	This module implements utility procedures for namespaces
 *	in the BLT toolkit.
 *
 * Copyright 1991-1998 Lucent Technologies, Inc.
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
 */

#include "bltInt.h"
#include "bltList.h"

/* Namespace related routines */

typedef struct {
    char *result;
    Tcl_FreeProc *freeProc;
    int errorLine;
    Tcl_HashTable commandTable;
    Tcl_HashTable mathFuncTable;

    Tcl_HashTable globalTable;	/* This is the only field we care about */

    int nLevels;
    int maxNestingDepth;
} TclInterp;



/*
 * ----------------------------------------------------------------------
 *
 * Blt_GetVariableNamespace --
 *
 *	Returns the namespace context of the vector variable.  If NULL,
 *	this indicates that the variable is local to the call frame.
 *
 *	Note the ever-dangerous manner in which we get this information.
 *	All of these structures are "private".   Now who's calling Tcl
 *	an "extension" language?
 *
 * Results:
 *	Returns the context of the namespace in an opaque type.
 *
 * ----------------------------------------------------------------------
 */

#if (TCL_MAJOR_VERSION == 7)

#ifdef ITCL_NAMESPACES

struct VarTrace;
struct ArraySearch;
struct NamespCacheRef;

typedef struct VarStruct Var;

struct VarStruct {
    int valueLength;
    int valueSpace;
    union {
	char *string;
	Tcl_HashTable *tablePtr;
	Var *upvarPtr;
    } value;
    Tcl_HashEntry *hPtr;
    int refCount;
    struct VarTrace *tracePtr;
    struct ArraySearch *searchPtr;
    int flags;

    /* >>>>>>>>>> stuff for [incr Tcl] namespaces <<<<<<<<<< */

    char *name;
    int protection;

    Itcl_Namespace *namesp;
    struct NamespCacheRef *cacheInfo;

} Var;


Tcl_Namespace *
Tcl_FindNamespace(interp, name)
    Tcl_Interp *interp;
    char *name;
{
    Itcl_Namespace nsToken;

    if (Itcl_FindNamesp(interp, name, 0, &nsToken) != TCL_OK) {
	Tcl_ResetResult(interp);
	return NULL;
    }
    return (Tcl_Namespace *) nsToken;
}

Tcl_Namespace *
Tcl_GetGlobalNamespace(interp)
    Tcl_Interp *interp;
{
    return (Tcl_Namespace *) Itcl_GetGlobalNamesp(interp);
}

Tcl_Namespace *
Tcl_GetCurrentNamespace(interp)
    Tcl_Interp *interp;
{
    return (Tcl_Namespace *) Itcl_GetActiveNamesp(interp);
}

Tcl_Namespace *
Blt_GetCommandNamespace(interp, cmdToken)
    Tcl_Interp *interp;
    Tcl_Command cmdToken;
{
    return (Tcl_Namespace *)interp;
}

Tcl_Namespace *
Blt_GetVariableNamespace(interp, name)
    Tcl_Interp *interp;
    CONST char *name;
{
    Tcl_Var varToken;
    Var *varPtr;

    if (Itcl_FindVariable(interp, name, 0, &varToken) != TCL_OK) {
	return NULL;
    }
    varPtr = (Var *) varToken;
    if (varPtr == NULL) {
	return NULL;
    }
    return (Tcl_Namespace *) varPtr->namesp;
}

Tcl_CallFrame *
Blt_EnterNamespace(interp, nsPtr)
    Tcl_Interp *interp;
    Tcl_Namespace *nsPtr;
{
    Itcl_Namespace nsToken = (Itcl_Namespace) nsPtr;

    return (Tcl_CallFrame *) Itcl_ActivateNamesp(interp, nsToken);
}

void
Blt_LeaveNamespace(interp, framePtr)
    Tcl_Interp *interp;
    Tcl_CallFrame *framePtr;
{
    Itcl_DeactivateNamesp(interp, (Itcl_ActiveNamespace) framePtr);
}

#else

Tcl_Namespace *
Blt_GetCommandNamespace(interp, cmdToken)
    Tcl_Interp *interp;
    Tcl_Command cmdToken;
{
    return (Tcl_Namespace *)interp;
}

Tcl_Namespace *
Blt_GetVariableNamespace(interp, name)
    Tcl_Interp *interp;
    CONST char *name;
{
    TclInterp *iPtr = (TclInterp *) interp;

    return (Tcl_Namespace *)
	Tcl_FindHashEntry(&(iPtr->globalTable), (char *)name);
}

Tcl_CallFrame *
Blt_EnterNamespace(interp, nsPtr)
    Tcl_Interp *interp;
    Tcl_Namespace *nsPtr;	/* Not used. */
{
    return NULL;
}

void
Blt_LeaveNamespace(interp, framePtr)
    Tcl_Interp *interp;
    Tcl_CallFrame *framePtr;
{
    /* empty */
}

Tcl_Namespace *
Tcl_GetGlobalNamespace(interp)
    Tcl_Interp *interp;
{
    return (Tcl_Namespace *) interp;
}

Tcl_Namespace *
Tcl_GetCurrentNamespace(interp)
    Tcl_Interp *interp;
{
    return (Tcl_Namespace *) interp;
}

Tcl_Namespace *
Tcl_FindNamespace(interp, name)
    Tcl_Interp *interp;
    char *name;
{
    return (Tcl_Namespace *) interp;
}

#endif /* ITCL_NAMESPACES */

#else

/*
 * A Command structure exists for each command in a namespace. The
 * Tcl_Command opaque type actually refers to these structures.
 */

typedef struct CompileProcStruct CompileProc;
typedef struct ImportRefStruct ImportRef;

typedef struct {
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
    Tcl_Namespace *nsPtr;	/* Points to the namespace containing this
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
    int deleted;		/* Means that the command is in the process
				 * of being deleted (its deleteProc is
				 * currently executing). Other attempts to
				 * delete the command should be ignored. */
    ImportRef *importRefPtr;	/* List of each imported Command created in
				 * another namespace when this command is
				 * imported. These imported commands
				 * redirect invocations back to this
				 * command. The list is used to remove all
				 * those imported commands when deleting
				 * this "real" command. */
} Command;


struct VarTrace;
struct ArraySearch;

typedef struct VarStruct Var;

struct VarStruct {
    union {
	Tcl_Obj *objPtr;
	Tcl_HashTable *tablePtr;
	Var *linkPtr;
    } value;
    char *name;
    Tcl_Namespace *nsPtr;
    Tcl_HashEntry *hPtr;
    int refCount;
    struct VarTrace *tracePtr;
    struct ArraySearch *searchPtr;
    int flags;
};

extern Var *TclLookupVar _ANSI_ARGS_((Tcl_Interp *interp, CONST char *part1, 
	CONST char *part2, int flags, char *mesg, int p1Flags, int p2Flags, 
	Var ** varPtrPtr));

#define VAR_SCALAR		0x1
#define VAR_ARRAY		0x2
#define VAR_LINK		0x4
#define VAR_UNDEFINED	        0x8
#define VAR_IN_HASHTABLE	0x10
#define VAR_TRACE_ACTIVE	0x20
#define VAR_ARRAY_ELEMENT	0x40
#define VAR_NAMESPACE_VAR	0x80

#define VAR_ARGUMENT		0x100
#define VAR_TEMPORARY		0x200
#define VAR_RESOLVED		0x400


Tcl_HashTable *
Blt_GetArrayVariableTable(interp, varName, flags)
    Tcl_Interp *interp;
    CONST char *varName;
    int flags;
{
    Var *varPtr, *arrayPtr;

    varPtr = TclLookupVar(interp, varName, (char *)NULL, flags, "read",
	FALSE, FALSE, &arrayPtr);
    if ((varPtr == NULL) || ((varPtr->flags & VAR_ARRAY) == 0)) {
	return NULL;
    }
    return varPtr->value.tablePtr;
}

Tcl_Namespace *
Blt_GetVariableNamespace(interp, name)
    Tcl_Interp *interp;
    CONST char *name;
{
    Var *varPtr;

    varPtr = (Var *)Tcl_FindNamespaceVar(interp, (char *)name, 
	(Tcl_Namespace *)NULL, 0);
    if (varPtr == NULL) {
	return NULL;
    }
    return varPtr->nsPtr;
}

/*ARGSUSED*/
Tcl_Namespace *
Blt_GetCommandNamespace(interp, cmdToken)
    Tcl_Interp *interp;		/* Not used. */
    Tcl_Command cmdToken;
{
    Command *cmdPtr = (Command *)cmdToken;

    return (Tcl_Namespace *)cmdPtr->nsPtr;
}

Tcl_CallFrame *
Blt_EnterNamespace(interp, nsPtr)
    Tcl_Interp *interp;
    Tcl_Namespace *nsPtr;
{
    Tcl_CallFrame *framePtr;

    framePtr = Blt_Malloc(sizeof(Tcl_CallFrame));
    assert(framePtr);
    if (Tcl_PushCallFrame(interp, framePtr, (Tcl_Namespace *)nsPtr, 0)
	!= TCL_OK) {
	Blt_Free(framePtr);
	return NULL;
    }
    return framePtr;
}

void
Blt_LeaveNamespace(interp, framePtr)
    Tcl_Interp *interp;
    Tcl_CallFrame *framePtr;
{
    Tcl_PopCallFrame(interp);
    Blt_Free(framePtr);
}

#endif /* TCL_MAJOR_VERSION == 7 */

int
Blt_ParseQualifiedName(interp, qualName, nsPtrPtr, namePtrPtr)
    Tcl_Interp *interp;
    CONST char *qualName;
    Tcl_Namespace **nsPtrPtr;
    CONST char **namePtrPtr;
{
    register char *p, *colon;
    Tcl_Namespace *nsPtr;

    colon = NULL;
    p = (char *)(qualName + strlen(qualName));
    while (--p > qualName) {
	if ((*p == ':') && (*(p - 1) == ':')) {
	    p++;		/* just after the last "::" */
	    colon = p - 2;
	    break;
	}
    }
    if (colon == NULL) {
	*nsPtrPtr = NULL;
	*namePtrPtr = (char *)qualName;
	return TCL_OK;
    }
    *colon = '\0';
    if (qualName[0] == '\0') {
	nsPtr = Tcl_GetGlobalNamespace(interp);
    } else {
	nsPtr = Tcl_FindNamespace(interp, (char *)qualName, 
		(Tcl_Namespace *)NULL, 0);
    }
    *colon = ':';
    if (nsPtr == NULL) {
	return TCL_ERROR;
    }
    *nsPtrPtr = nsPtr;
    *namePtrPtr = p;
    return TCL_OK;
}

char *
Blt_GetQualifiedName(nsPtr, name, resultPtr)
    Tcl_Namespace *nsPtr;
    CONST char *name;
    Tcl_DString *resultPtr;
{
    Tcl_DStringInit(resultPtr);
#if (TCL_MAJOR_VERSION > 7)
    if ((nsPtr->fullName[0] != ':') || (nsPtr->fullName[1] != ':') ||
	(nsPtr->fullName[2] != '\0')) {
	Tcl_DStringAppend(resultPtr, nsPtr->fullName, -1);
    }
#endif
    Tcl_DStringAppend(resultPtr, "::", -1);
    Tcl_DStringAppend(resultPtr, (char *)name, -1);
    return Tcl_DStringValue(resultPtr);
}


#if (TCL_MAJOR_VERSION > 7)

typedef struct {
    Tcl_HashTable clientTable;

    /* Original clientdata and delete procedure. */
    ClientData origClientData;
    Tcl_NamespaceDeleteProc *origDeleteProc;

} Callback;

static Tcl_CmdProc NamespaceDeleteCmd;
static Tcl_NamespaceDeleteProc NamespaceDeleteNotify;

#define NS_DELETE_CMD	"#NamespaceDeleteNotifier"

/*ARGSUSED*/
static int
NamespaceDeleteCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/*  */
    int argc;
    char **argv;
{
    Tcl_AppendResult(interp, "command \"", argv[0], "\" shouldn't be invoked",
	(char *)NULL);
    return TCL_ERROR;
}

static void
NamespaceDeleteNotify(clientData)
    ClientData clientData;
{
    Blt_List list;
    Blt_ListNode node;
    Tcl_CmdDeleteProc *deleteProc;

    list = (Blt_List)clientData;
    for (node = Blt_ListFirstNode(list); node != NULL;
	node = Blt_ListNextNode(node)) {
	deleteProc = (Tcl_CmdDeleteProc *)Blt_ListGetValue(node);
	clientData = (ClientData)Blt_ListGetKey(node);
	(*deleteProc) (clientData);
    }
    Blt_ListDestroy(list);
}

void
Blt_DestroyNsDeleteNotify(interp, nsPtr, clientData)
    Tcl_Interp *interp;
    Tcl_Namespace *nsPtr;
    ClientData clientData;
{
    Blt_List list;
    Blt_ListNode node;
    char *string;
    Tcl_CmdInfo cmdInfo;

    string = Blt_Malloc(sizeof(nsPtr->fullName) + strlen(NS_DELETE_CMD) + 4);
    strcpy(string, nsPtr->fullName);
    strcat(string, "::");
    strcat(string, NS_DELETE_CMD);
    if (!Tcl_GetCommandInfo(interp, string, &cmdInfo)) {
	goto done;
    }
    list = (Blt_List)cmdInfo.clientData;
    node = Blt_ListGetNode(list, clientData);
    if (node != NULL) {
	Blt_ListDeleteNode(node);
    }
  done:
    Blt_Free(string);
}

int
Blt_CreateNsDeleteNotify(interp, nsPtr, clientData, deleteProc)
    Tcl_Interp *interp;
    Tcl_Namespace *nsPtr;
    ClientData clientData;
    Tcl_CmdDeleteProc *deleteProc;
{
    Blt_List list;
    char *string;
    Tcl_CmdInfo cmdInfo;

    string = Blt_Malloc(sizeof(nsPtr->fullName) + strlen(NS_DELETE_CMD) + 4);
    strcpy(string, nsPtr->fullName);
    strcat(string, "::");
    strcat(string, NS_DELETE_CMD);
    if (!Tcl_GetCommandInfo(interp, string, &cmdInfo)) {
	list = Blt_ListCreate(BLT_ONE_WORD_KEYS);
	Blt_CreateCommand(interp, string, NamespaceDeleteCmd, list, 
		NamespaceDeleteNotify);
    } else {
	list = (Blt_List)cmdInfo.clientData;
    }
    Blt_Free(string);
    Blt_ListAppend(list, clientData, (ClientData)deleteProc);
    return TCL_OK;
}

#endif /* TCL_MAJOR_VERSION > 7 */

#if (TCL_VERSION_NUMBER < _VERSION(8,0,0)) 

/*
 *----------------------------------------------------------------------
 *
 * Blt_CreateCommand --
 *
 *	Like Tcl_CreateCommand, but creates command in current namespace
 *	instead of global, if one isn't defined.  Not a problem with
 *	[incr Tcl] namespaces.
 *
 * Results:
 *	The return value is a token for the command, which can
 *	be used in future calls to Tcl_GetCommandName.
 *
 *----------------------------------------------------------------------
 */
Tcl_Command
Blt_CreateCommand(interp, cmdName, proc, clientData, deleteProc)
    Tcl_Interp *interp;		/* Token for command interpreter returned by
				 * a previous call to Tcl_CreateInterp. */
    CONST char *cmdName;	/* Name of command. If it contains namespace
				 * qualifiers, the new command is put in the
				 * specified namespace; otherwise it is put
				 * in the global namespace. */
    Tcl_CmdProc *proc;		/* Procedure to associate with cmdName. */
    ClientData clientData;	/* Arbitrary value passed to string proc. */
    Tcl_CmdDeleteProc *deleteProc;
				/* If not NULL, gives a procedure to call
				 * when this command is deleted. */
{
    return Tcl_CreateCommand(interp, (char *)cmdName, proc, clientData, 
	deleteProc);
}

/*ARGSUSED*/
Tcl_Command
Tcl_FindCommand(interp, cmdName, nsPtr, flags)
    Tcl_Interp *interp;
    char *cmdName;
    Tcl_Namespace *nsPtr;	/* Not used. */
    int flags;			/* Not used. */
{
    Tcl_HashEntry *hPtr;
    TclInterp *iPtr = (TclInterp *) interp;

    hPtr = Tcl_FindHashEntry(&iPtr->commandTable, cmdName);
    if (hPtr == NULL) {
	return NULL;
    }
    return (Tcl_Command) Tcl_GetHashValue(hPtr);
}

#endif /* TCL_MAJOR_VERSION <= 7 */

#if (TCL_VERSION_NUMBER >= _VERSION(8,0,0)) 
/*
 *----------------------------------------------------------------------
 *
 * Blt_CreateCommand --
 *
 *	Like Tcl_CreateCommand, but creates command in current namespace
 *	instead of global, if one isn't defined.  Not a problem with
 *	[incr Tcl] namespaces.
 *
 * Results:
 *	The return value is a token for the command, which can
 *	be used in future calls to Tcl_GetCommandName.
 *
 *----------------------------------------------------------------------
 */
Tcl_Command
Blt_CreateCommand(interp, cmdName, proc, clientData, deleteProc)
    Tcl_Interp *interp;		/* Token for command interpreter returned by
				 * a previous call to Tcl_CreateInterp. */
    CONST char *cmdName;	/* Name of command. If it contains namespace
				 * qualifiers, the new command is put in the
				 * specified namespace; otherwise it is put
				 * in the global namespace. */
    Tcl_CmdProc *proc;		/* Procedure to associate with cmdName. */
    ClientData clientData;	/* Arbitrary value passed to string proc. */
    Tcl_CmdDeleteProc *deleteProc;
    /* If not NULL, gives a procedure to call

				 * when this command is deleted. */
{
    register CONST char *p;

    p = cmdName + strlen(cmdName);
    while (--p > cmdName) {
	if ((*p == ':') && (*(p - 1) == ':')) {
	    p++;		/* just after the last "::" */
	    break;
	}
    }
    if (cmdName == p) {
	Tcl_DString dString;
	Tcl_Namespace *nsPtr;
	Tcl_Command cmdToken;

	Tcl_DStringInit(&dString);
	nsPtr = Tcl_GetCurrentNamespace(interp);
	Tcl_DStringAppend(&dString, nsPtr->fullName, -1);
	Tcl_DStringAppend(&dString, "::", -1);
	Tcl_DStringAppend(&dString, cmdName, -1);
	cmdToken = Tcl_CreateCommand(interp, Tcl_DStringValue(&dString), proc,
	    clientData, deleteProc);
	Tcl_DStringFree(&dString);
	return cmdToken;
    }
    return Tcl_CreateCommand(interp, (char *)cmdName, proc, clientData, 
	deleteProc);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_CreateCommandObj --
 *
 *	Like Tcl_CreateCommand, but creates command in current namespace
 *	instead of global, if one isn't defined.  Not a problem with
 *	[incr Tcl] namespaces.
 *
 * Results:
 *	The return value is a token for the command, which can
 *	be used in future calls to Tcl_GetCommandName.
 *
 *----------------------------------------------------------------------
 */
Tcl_Command
Blt_CreateCommandObj(interp, cmdName, proc, clientData, deleteProc)
    Tcl_Interp *interp;		/* Token for command interpreter returned by
				 * a previous call to Tcl_CreateInterp. */
    CONST char *cmdName;	/* Name of command. If it contains namespace
				 * qualifiers, the new command is put in the
				 * specified namespace; otherwise it is put
				 * in the global namespace. */
    Tcl_ObjCmdProc *proc;	/* Procedure to associate with cmdName. */
    ClientData clientData;	/* Arbitrary value passed to string proc. */
    Tcl_CmdDeleteProc *deleteProc;
				/* If not NULL, gives a procedure to call
				 * when this command is deleted. */
{
    register CONST char *p;

    p = cmdName + strlen(cmdName);
    while (--p > cmdName) {
	if ((*p == ':') && (*(p - 1) == ':')) {
	    p++;		/* just after the last "::" */
	    break;
	}
    }
    if (cmdName == p) {
	Tcl_DString dString;
	Tcl_Namespace *nsPtr;
	Tcl_Command cmdToken;

	Tcl_DStringInit(&dString);
	nsPtr = Tcl_GetCurrentNamespace(interp);
	Tcl_DStringAppend(&dString, nsPtr->fullName, -1);
	Tcl_DStringAppend(&dString, "::", -1);
	Tcl_DStringAppend(&dString, cmdName, -1);
	cmdToken = Tcl_CreateObjCommand(interp, Tcl_DStringValue(&dString), 
		proc, clientData, deleteProc);
	Tcl_DStringFree(&dString);
	return cmdToken;
    }
    return Tcl_CreateObjCommand(interp, (char *)cmdName, proc, clientData, 
	deleteProc);
}
#endif /* TCL_VERSION_NUMBER < 8.0.0 */
