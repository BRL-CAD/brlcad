
/* 
 * bltWinDde.c --
 *
 *	This file provides procedures that implement the "send"
 *	command, allowing commands to be passed from interpreter
 *	to interpreter.
 *
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "bltInt.h"

#ifndef NO_DDE

#include <ddeml.h>

/* 
 * The following structure is used to keep track of the interpreters
 * registered by this process.
 */

typedef struct RegisteredInterp {
    struct RegisteredInterp *nextPtr;
				/* The next interp this application knows
				 * about. */
    Tcl_Interp *interp;		/* The interpreter attached to this name. */
    char name[1];		/* Interpreter's name. Malloc-ed as
				 * part of the structure. */
} RegisteredInterp;

/*
 * Used to keep track of conversations.
 */

typedef struct Conversation {
    struct Conversation *nextPtr;
				/* The next conversation in the list. */
    RegisteredInterp *riPtr;	/* The info we know about the conversation. */
    HCONV hConv;		/* The DDE handle for this conversation. */
    Tcl_Obj *returnPackagePtr;	/* The result package for this conversation. */

} Conversation;

static Conversation *conversations; /* A list of conversations currently
				     * being processed. */
static RegisteredInterp *interps; /* List of all interpreters registered
				   * in the current process. */
static HSZ globalService;		
static DWORD instance;		/* The application instance handle given
				 * to us by DdeInitialize. */ 
static int isServer;

#define TCL_DDE_VERSION		"1.2"
#define TCL_DDE_PACKAGE_NAME	"dde"
#define TCL_DDE_SERVICE_NAME	"TclEval"

/*
 * Forward declarations for procedures defined later in this file.
 */

static Tcl_Obj *ExecuteRemoteObject _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_Obj *objPtr));
static int MakeConnection _ANSI_ARGS_((Tcl_Interp *interp, char *name, 
	HCONV *convPtr));
static HDDEDATA CALLBACK ServerProc _ANSI_ARGS_((UINT uType, UINT uFmt, 
	HCONV hConv, HSZ topic, HSZ item, HDDEDATA hData, DWORD dwData1, 
	DWORD dwData2));

static Tcl_ExitProc ExitProc;
static Tcl_CmdDeleteProc DeleteProc;
static void SetError _ANSI_ARGS_((Tcl_Interp *interp));

static Tcl_ObjCmdProc DdeObjCmd;

/*
 *----------------------------------------------------------------------
 *
 * Initialize --
 *
 *	Initialize the global DDE instance.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Registers the DDE server proc.
 *
 *----------------------------------------------------------------------
 */

static void
Initialize(void)
{
    int nameFound = 0;

    /*
     * See if the application is already registered; if so, remove its
     * current name from the registry. The deletion of the command
     * will take care of disposing of this entry.
     */

    if (interps != NULL) {
	nameFound = 1;
    }

    /*
     * Make sure that the DDE server is there. This is done only once,
     * add an exit handler tear it down.
     */

    if (instance == 0) {
	if (instance == 0) {
	    unsigned int flags;

	    flags = (CBF_SKIP_REGISTRATIONS | CBF_SKIP_UNREGISTRATIONS | 
		    CBF_FAIL_POKES);
	    if (DdeInitialize(&instance, ServerProc, flags, 0) 
		!= DMLERR_NO_ERROR) {
		instance = 0;
	    }
	}
    }
    if ((globalService == 0) && (nameFound != 0)) {
	if ((globalService == 0) && (nameFound != 0)) {
	    isServer = TRUE;
	    Tcl_CreateExitHandler(ExitProc, NULL);
	    globalService = DdeCreateStringHandle(instance, 
		TCL_DDE_SERVICE_NAME, 0);
	    DdeNameService(instance, globalService, 0L, DNS_REGISTER);
	} else {
	    isServer = FALSE;
	}
    }
}    

/*
 *--------------------------------------------------------------
 *
 * SetServerName --
 *
 *	This procedure is called to associate an ASCII name with a Dde
 *	server.  If the interpreter has already been named, the
 *	name replaces the old one.
 *
 * Results:
 *	The return value is the name actually given to the interp.
 *	This will normally be the same as name, but if name was already
 *	in use for a Dde Server then a name of the form "name #2" will
 *	be chosen,  with a high enough number to make the name unique.
 *
 * Side effects:
 *	Registration info is saved, thereby allowing the "send" command
 *	to be used later to invoke commands in the application.  In
 *	addition, the "send" command is created in the application's
 *	interpreter.  The registration will be removed automatically
 *	if the interpreter is deleted or the "send" command is removed.
 *
 *--------------------------------------------------------------
 */

static char *
SetServerName(
    Tcl_Interp *interp,
    char *name			/* The name that will be used to
				 * refer to the interpreter in later
				 * "send" commands.  Must be globally
				 * unique. */
    )
{
    int suffix, offset;
    RegisteredInterp *riPtr, *prevPtr;
    Tcl_DString dString;

    /*
     * See if the application is already registered; if so, remove its
     * current name from the registry. The deletion of the command
     * will take care of disposing of this entry.
     */

    for (riPtr = interps, prevPtr = NULL; riPtr != NULL; 
	    prevPtr = riPtr, riPtr = riPtr->nextPtr) {
	if (riPtr->interp == interp) {
	    if (name != NULL) {
		if (prevPtr == NULL) {
		    interps = interps->nextPtr;
		} else {
		    prevPtr->nextPtr = riPtr->nextPtr;
		}
		break;
	    } else {
		/*
		 * the name was NULL, so the caller is asking for
		 * the name of the current interp.
		 */

		return riPtr->name;
	    }
	}
    }

    if (name == NULL) {
	/*
	 * the name was NULL, so the caller is asking for
	 * the name of the current interp, but it doesn't
	 * have a name.
	 */

	return "";
    }
    
    /*
     * Pick a name to use for the application.  Use "name" if it's not
     * already in use.  Otherwise add a suffix such as " #2", trying
     * larger and larger numbers until we eventually find one that is
     * unique.
     */

    suffix = 1;
    offset = 0;
    Tcl_DStringInit(&dString);

    /*
     * We have found a unique name. Now add it to the registry.
     */

    riPtr = Blt_Malloc(sizeof(RegisteredInterp) + strlen(name));
    riPtr->interp = interp;
    riPtr->nextPtr = interps;
    interps = riPtr;
    strcpy(riPtr->name, name);

    Tcl_CreateObjCommand(interp, "dde", DdeObjCmd, riPtr, DeleteProc);
    if (Tcl_IsSafe(interp)) {
	Tcl_HideCommand(interp, "dde", "dde");
    }
    Tcl_DStringFree(&dString);

    /*
     * re-initialize with the new name
     */
    Initialize();
    
    return riPtr->name;
}

/*
 *--------------------------------------------------------------
 *
 * DeleteProc
 *
 *	This procedure is called when the command "dde" is destroyed.
 *
 * Results:
 *	none
 *
 * Side effects:
 *	The interpreter given by riPtr is unregistered.
 *
 *--------------------------------------------------------------
 */

static void
DeleteProc(clientData)
    ClientData clientData;	/* The interp we are deleting passed
				 * as ClientData. */
{
    RegisteredInterp *riPtr = clientData;
    RegisteredInterp *searchPtr, *prevPtr;

    for (searchPtr = interps, prevPtr = NULL;
	    (searchPtr != NULL) && (searchPtr != riPtr);
	    prevPtr = searchPtr, searchPtr = searchPtr->nextPtr) {
	/*
	 * Empty loop body.
	 */
    }

    if (searchPtr != NULL) {
	if (prevPtr == NULL) {
	    interps = interps->nextPtr;
	} else {
	    prevPtr->nextPtr = searchPtr->nextPtr;
	}
    }
    Tcl_EventuallyFree(clientData, TCL_DYNAMIC);
}

/*
 *--------------------------------------------------------------
 *
 * ExecuteRemoteObject --
 *
 *	Takes the package delivered by DDE and executes it in
 *	the server's interpreter.
 *
 * Results:
 *	A list Tcl_Obj * that describes what happened. The first
 *	element is the numerical return code (TCL_ERROR, etc.).
 *	The second element is the result of the script. If the
 *	return result was TCL_ERROR, then the third element
 *	will be the value of the global "errorCode", and the
 *	fourth will be the value of the global "errorInfo".
 *	The return result will have a refCount of 0.
 *
 * Side effects:
 *	A Tcl script is run, which can cause all kinds of other
 *	things to happen.
 *
 *--------------------------------------------------------------
 */

static Tcl_Obj *
ExecuteRemoteObject(
    Tcl_Interp *interp,		/* Remote interpreter. */
    Tcl_Obj *objPtr)		/* The object to execute. */
{
    Tcl_Obj *listObjPtr;
    int result;

    result = Tcl_GlobalEval(interp, Tcl_GetString(objPtr));
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    Tcl_ListObjAppendElement(NULL, listObjPtr, Tcl_NewIntObj(result));
    Tcl_ListObjAppendElement(NULL, listObjPtr, Tcl_GetObjResult(interp));
    if (result == TCL_ERROR) {
	char *value;
	Tcl_Obj *objPtr;

	value = Tcl_GetVar2(interp, "errorCode", NULL, TCL_GLOBAL_ONLY);
	objPtr = Tcl_NewStringObj(value, -1);
	Tcl_ListObjAppendElement(NULL, listObjPtr, objPtr);
	value = Tcl_GetVar2(interp, "errorInfo", NULL, TCL_GLOBAL_ONLY);
	objPtr = Tcl_NewStringObj(value, -1);
        Tcl_ListObjAppendElement(NULL, listObjPtr, objPtr);
    }
    return listObjPtr;
}

/*
 *--------------------------------------------------------------
 *
 * ServerProc --
 *
 *	Handles all transactions for this server. Can handle
 *	execute, request, and connect protocols. Dde will
 *	call this routine when a client attempts to run a dde
 *	command using this server.
 *
 * Results:
 *	A DDE Handle with the result of the dde command.
 *
 * Side effects:
 *	Depending on which command is executed, arbitrary
 *	Tcl scripts can be run.
 *
 *--------------------------------------------------------------
 */

static HDDEDATA CALLBACK
ServerProc (
    UINT uType,			/* The type of DDE transaction we
				 * are performing. */
    UINT uFmt,			/* The format that data is sent or
				 * received. */
    HCONV hConv,		/* The conversation associated with the 
				 * current transaction. */
    HSZ topic,			/* A string handle. Transaction-type 
				 * dependent. */
    HSZ item,			/* A string handle. Transaction-type 
				 * dependent. */
    HDDEDATA hData,		/* DDE data. Transaction-type dependent. */
    DWORD dwData1,		/* Transaction-dependent data. */
    DWORD dwData2)		/* Transaction-dependent data. */
{
    Tcl_DString dString;
    char *utilString;
    Tcl_Obj *objPtr;
    HDDEDATA code = NULL;
    RegisteredInterp *riPtr;
    Conversation *convPtr, *prevConvPtr;

    switch(uType) {
    case XTYP_CONNECT:
	{
	    int length;

	    /*
	     * Dde is trying to initialize a conversation with us. Check
	     * and make sure we have a valid topic.
	     */

	    length = DdeQueryString(instance, topic, NULL, 0, 0);
	    Tcl_DStringInit(&dString);
	    Tcl_DStringSetLength(&dString, length);
	    utilString = Tcl_DStringValue(&dString);
	    DdeQueryString(instance, topic, utilString, length + 1, 
			   CP_WINANSI);

	    for (riPtr = interps; riPtr != NULL; riPtr = riPtr->nextPtr) {
		if (strcasecmp(utilString, riPtr->name) == 0) {
		    Tcl_DStringFree(&dString);
		    return (HDDEDATA) TRUE;
		}
	    }

	    Tcl_DStringFree(&dString);
	    return (HDDEDATA) FALSE;
	}
    case XTYP_CONNECT_CONFIRM: 
	{
	    DWORD length;

	    /*
	     * Dde has decided that we can connect, so it gives us a 
	     * conversation handle. We need to keep track of it
	     * so we know which execution result to return in an
	     * XTYP_REQUEST.
	     */

	    length = DdeQueryString(instance, topic, NULL, 0, 0);
	    Tcl_DStringInit(&dString);
	    Tcl_DStringSetLength(&dString, length);
	    utilString = Tcl_DStringValue(&dString);
	    DdeQueryString(instance, topic, utilString, length + 1, 
		    CP_WINANSI);
	    for (riPtr = interps; riPtr != NULL; riPtr = riPtr->nextPtr) {
		if (strcasecmp(riPtr->name, utilString) == 0) {
		    convPtr = Blt_Malloc(sizeof(Conversation));
		    convPtr->nextPtr = conversations;
		    convPtr->returnPackagePtr = NULL;
		    convPtr->hConv = hConv;
		    convPtr->riPtr = riPtr;
		    conversations = convPtr;
		    break;
		}
	    }
	    Tcl_DStringFree(&dString);
	    return (HDDEDATA) TRUE;
	}
    case XTYP_DISCONNECT:
	{
	    /*
	     * The client has disconnected from our server. Forget this
	     * conversation.
	     */

	    for (convPtr = conversations, prevConvPtr = NULL;
		    convPtr != NULL; 
		    prevConvPtr = convPtr, convPtr = convPtr->nextPtr) {
		if (hConv == convPtr->hConv) {
		    if (prevConvPtr == NULL) {
			conversations = convPtr->nextPtr;
		    } else {
			prevConvPtr->nextPtr = convPtr->nextPtr;
		    }
		    if (convPtr->returnPackagePtr != NULL) {
			Tcl_DecrRefCount(convPtr->returnPackagePtr);
		    }
		    Blt_Free(convPtr);
		    break;
		}
	    }
	    return (HDDEDATA) TRUE;
	}
    case XTYP_REQUEST:
	{
	    int length;

	    /*
	     * This could be either a request for a value of a Tcl variable,
	     * or it could be the send command requesting the results of the
	     * last execute.
	     */

	    if (uFmt != CF_TEXT) {
		return (HDDEDATA) FALSE;
	    }

	    code = (HDDEDATA) FALSE;
	    for (convPtr = conversations; (convPtr != NULL)
		    && (convPtr->hConv != hConv); convPtr = convPtr->nextPtr) {
		/*
		 * Empty loop body.
		 */
	    }

	    if (convPtr != NULL) {
		length = DdeQueryString(instance, item, NULL, 0, CP_WINANSI);
		Tcl_DStringInit(&dString);
		Tcl_DStringSetLength(&dString, length);
		utilString = Tcl_DStringValue(&dString);
		DdeQueryString(instance, item, utilString, length + 1, 
			CP_WINANSI);
		if (strcasecmp(utilString, "$TCLEVAL$EXECUTE$RESULT") == 0) {
		    char *value;

		    value = Tcl_GetStringFromObj(convPtr->returnPackagePtr, 
				 &length);
		    code = DdeCreateDataHandle(instance, value, length+1, 0, 
			item, CF_TEXT, 0);
		} else {
		    char *value;

		    value = Tcl_GetVar2(convPtr->riPtr->interp, utilString, 
				NULL, TCL_GLOBAL_ONLY);
		    if (value != NULL) {
			length = strlen(value);
			code = DdeCreateDataHandle(instance, value, length+1,
				0, item, CF_TEXT, 0);
		    } else {
			code = NULL;
		    }
		}
		Tcl_DStringFree(&dString);
	    }
	    return code;
	}
    case XTYP_EXECUTE: 
	{
	    DWORD length;
	    /*
	     * Execute this script. The results will be saved into
	     * a list object which will be retreived later. See
	     * ExecuteRemoteObject.
	     */

	    Tcl_Obj *returnPackagePtr;

	    for (convPtr = conversations; (convPtr != NULL)
		    && (convPtr->hConv != hConv); convPtr = convPtr->nextPtr) {
		/*
		 * Empty loop body.
		 */

	    }

	    if (convPtr == NULL) {
		return (HDDEDATA) DDE_FNOTPROCESSED;
	    }

	    utilString = (char *) DdeAccessData(hData, &length);
	    objPtr = Tcl_NewStringObj(utilString, -1);
	    Tcl_IncrRefCount(objPtr);
	    DdeUnaccessData(hData);
	    if (convPtr->returnPackagePtr != NULL) {
		Tcl_DecrRefCount(convPtr->returnPackagePtr);
	    }
	    convPtr->returnPackagePtr = NULL;
	    returnPackagePtr = ExecuteRemoteObject(convPtr->riPtr->interp, 
		objPtr);
	    for (convPtr = conversations; (convPtr != NULL)
 		    && (convPtr->hConv != hConv); convPtr = convPtr->nextPtr) {
		/*
		 * Empty loop body.
		 */

	    }
	    if (convPtr != NULL) {
		Tcl_IncrRefCount(returnPackagePtr);
		convPtr->returnPackagePtr = returnPackagePtr;
	    }
	    Tcl_DecrRefCount(objPtr);
	    if (returnPackagePtr == NULL) {
		return (HDDEDATA) DDE_FNOTPROCESSED;
	    } else {
		return (HDDEDATA) DDE_FACK;
	    }
	}
    case XTYP_WILDCONNECT: 
	{
	    DWORD length;

	    /*
	     * Dde wants a list of services and topics that we support.
	     */

	    HSZPAIR *returnPtr;
	    int i;
	    int numItems;

	    for (i = 0, riPtr = interps; riPtr != NULL;
		    i++, riPtr = riPtr->nextPtr) {
		/*
		 * Empty loop body.
		 */

	    }

	    numItems = i;
	    code = DdeCreateDataHandle(instance, NULL, 
		(numItems + 1) * sizeof(HSZPAIR), 0, 0, 0, 0);
	    returnPtr = (HSZPAIR *) DdeAccessData(code, &length);
	    for (i = 0, riPtr = interps; i < numItems; 
		    i++, riPtr = riPtr->nextPtr) {
		returnPtr[i].hszSvc = DdeCreateStringHandle(
                        instance, "TclEval", CP_WINANSI);
		returnPtr[i].hszTopic = DdeCreateStringHandle(
                        instance, riPtr->name, CP_WINANSI);
	    }
	    returnPtr[i].hszSvc = NULL;
	    returnPtr[i].hszTopic = NULL;
	    DdeUnaccessData(code);
	    return code;
	}
    }
    return NULL;
}

/*
 *--------------------------------------------------------------
 *
 * ExitProc --
 *
 *	Gets rid of our DDE server when we go away.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The DDE server is deleted.
 *
 *--------------------------------------------------------------
 */

static void
ExitProc(
    ClientData clientData)	    /* Not used in this handler. */
{
    DdeNameService(instance, NULL, 0, DNS_UNREGISTER);
    DdeUninitialize(instance);
    instance = 0;
}

/*
 *--------------------------------------------------------------
 *
 * MakeConnection --
 *
 *	This procedure is a utility used to connect to a DDE
 *	server when given a server name and a topic name.
 *
 * Results:
 *	A standard Tcl result.
 *	
 *
 * Side effects:
 *	Passes back a conversation through ddeConvPtr
 *
 *--------------------------------------------------------------
 */

static int
MakeConnection(
    Tcl_Interp *interp,		/* Used to report errors. */
    char *name,			/* The connection to use. */
    HCONV *convPtr)
{
    HSZ topic, service;
    HCONV conv;
    
    service = DdeCreateStringHandle(instance, "TclEval", 0);
    topic = DdeCreateStringHandle(instance, name, 0);

    conv = DdeConnect(instance, service, topic, NULL);
    DdeFreeStringHandle(instance, service);
    DdeFreeStringHandle(instance, topic);

    if (conv == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "no registered server named \"", name, 
		"\"", (char *) NULL);
	}
	return TCL_ERROR;
    }

    *convPtr = conv;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * SetError --
 *
 *	Sets the interp result to a cogent error message
 *	describing the last DDE error.
 *
 * Results:
 *	None.
 *	
 *
 * Side effects:
 *	The interp's result object is changed.
 *
 *--------------------------------------------------------------
 */

static void
SetError(
    Tcl_Interp *interp)	    /* The interp to put the message in.*/
{
    int err;
    char *mesg;

    err = DdeGetLastError(instance);
    switch (err) {
	case DMLERR_DATAACKTIMEOUT:
	case DMLERR_EXECACKTIMEOUT:
	case DMLERR_POKEACKTIMEOUT:
	    mesg = "remote interpreter did not respond";
	    break;

	case DMLERR_BUSY:
	    mesg = "remote server is busy";
	    break;

	case DMLERR_NOTPROCESSED:
	    mesg = "remote server cannot handle this command";
	    break;

	default:
	    mesg = "dde command failed";
	    break;
    }
    Tcl_SetResult(interp, mesg, TCL_VOLATILE);
}

/*
 *--------------------------------------------------------------
 *
 * DdeObjCmd --
 *
 *	This procedure is invoked to process the "dde" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

static int
DdeObjCmd(
    ClientData clientData,	/* Used only for deletion */
    Tcl_Interp *interp,		/* The interp we are sending from */
    int objc,			/* Number of arguments */
    Tcl_Obj *CONST objv[])	/* The arguments */
{
    enum {
	DDE_SERVERNAME,
	DDE_EXECUTE,
	DDE_POKE,
	DDE_REQUEST,
	DDE_SERVICES,
	DDE_EVAL
    };

    static char *commands[] = {
	"servername", "execute", "poke", "request", "services", "eval", 
	(char *) NULL
    };
    static char *options[] = {
	"-async", (char *) NULL
    };
    int index, argIndex;
    int async = 0, binary = 0;
    int result = TCL_OK;
    HSZ service = NULL;
    HSZ topic = NULL;
    HSZ item = NULL;
    HDDEDATA data = NULL;
    HDDEDATA itemData = NULL;
    HCONV hConv = NULL;
    HSZ cookie = 0;
    char *serviceName, *topicName, *itemString, *dataString;
    char *string;
    int firstArg, length, dataLength;
    HDDEDATA code;
    RegisteredInterp *riPtr;
    Tcl_Interp *sendInterp;
    Tcl_Obj *objPtr;

    /*
     * Initialize DDE server/client
     */
    
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, 
		"?-async? serviceName topicName value");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], commands, "command", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    serviceName = NULL;		/* Suppress compiler warning. */
    firstArg = 1;
    switch (index) {
    case DDE_SERVERNAME:
	if ((objc != 3) && (objc != 2)) {
	    Tcl_WrongNumArgs(interp, 1, objv, "servername ?serverName?");
	    return TCL_ERROR;
	}
	firstArg = (objc - 1);
	break;
	
    case DDE_EXECUTE:
	if ((objc < 5) || (objc > 6)) {
	    Tcl_WrongNumArgs(interp, 1, objv, 
			"execute ?-async? serviceName topicName value");
	    return TCL_ERROR;
	}
	if (Tcl_GetIndexFromObj(NULL, objv[2], options, "option", 0,
				&argIndex) != TCL_OK) {
	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 1, objv,
			"execute ?-async? serviceName topicName value");
		return TCL_ERROR;
	    }
	    async = 0;
	    firstArg = 2;
	} else {
	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 1, objv,
			 "execute ?-async? serviceName topicName value");
		return TCL_ERROR;
	    }
	    async = 1;
	    firstArg = 3;
	}
	break;
    case DDE_POKE:
	if (objc != 6) {
	    Tcl_WrongNumArgs(interp, 1, objv,
			"poke serviceName topicName item value");
	    return TCL_ERROR;
	}
	firstArg = 2;
	break;

    case DDE_REQUEST:
	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 1, objv,
			     "request serviceName topicName value");
	    return TCL_ERROR;
	}
	binary = 0;
	firstArg = 2;
	break;

    case DDE_SERVICES:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 1, objv,
			     "services serviceName topicName");
	    return TCL_ERROR;
	}
	firstArg = 2;
	break;

    case DDE_EVAL:
	if (objc < 4) {
	    Tcl_WrongNumArgs(interp, 1, objv, 
			     "eval ?-async? serviceName args");
	    return TCL_ERROR;
	}
	if (Tcl_GetIndexFromObj(NULL, objv[2], options, "option", 0,
				&argIndex) != TCL_OK) {
	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 1, objv,
				 "eval ?-async? serviceName args");
		return TCL_ERROR;
	    }
	    async = 0;
	    firstArg = 2;
	} else {
	    if (objc < 5) {
		Tcl_WrongNumArgs(interp, 1, objv,
				 "eval ?-async? serviceName args");
		return TCL_ERROR;
	    }
	    async = 1;
	    firstArg = 3;
	}
	break;
    }

    Initialize();

    if (firstArg != 1) {
	serviceName = Tcl_GetStringFromObj(objv[firstArg], &length);
    } else {
	length = 0;
    }

    if (length == 0) {
	serviceName = NULL;
    } else if ((index != DDE_SERVERNAME) && (index != DDE_EVAL)) {
	service = DdeCreateStringHandle(instance, serviceName,
		CP_WINANSI);
    }

    if ((index != DDE_SERVERNAME) &&(index != DDE_EVAL)) {
	topicName = Tcl_GetStringFromObj(objv[firstArg + 1], &length);
	if (length == 0) {
	    topicName = NULL;
	} else {
	    topic = DdeCreateStringHandle(instance, topicName, CP_WINANSI);
	}
    }

    switch (index) {
    case DDE_SERVERNAME: 
	serviceName = SetServerName(interp, serviceName);
	if (serviceName != NULL) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp),
			     serviceName, -1);
	} else {
	    Tcl_ResetResult(interp);
	}
	break;

    case DDE_EXECUTE: 
	{
	    dataString = Tcl_GetStringFromObj(objv[firstArg + 2], &dataLength);
	    if (dataLength == 0) {
		Tcl_SetStringObj(Tcl_GetObjResult(interp),
				 "cannot execute null data", -1);
		result = TCL_ERROR;
		break;
	    }
	    hConv = DdeConnect(instance, service, topic, NULL);
	    DdeFreeStringHandle(instance, service);
	    DdeFreeStringHandle(instance, topic);

	    if (hConv == NULL) {
		SetError(interp);
		result = TCL_ERROR;
		break;
	    }

	    data = DdeCreateDataHandle(instance, dataString, dataLength + 1, 
		0, 0, CF_TEXT, 0);
	    if (data != NULL) {
		if (async) {
		    DWORD status;

		    DdeClientTransaction((LPBYTE) data, 0xFFFFFFFF, hConv, 0, 
			    CF_TEXT, XTYP_EXECUTE, TIMEOUT_ASYNC, &status);
		    DdeAbandonTransaction(instance, hConv, status);
		} else {
		    code = DdeClientTransaction((LPBYTE) data, 0xFFFFFFFF,
			    hConv, 0, CF_TEXT, XTYP_EXECUTE, 30000, NULL);
		    if (code == 0) {
			SetError(interp);
			result = TCL_ERROR;
		    }
		}
		DdeFreeDataHandle(data);
	    } else {
		SetError(interp);
		result = TCL_ERROR;
	    }
	    break;
	}
    case DDE_REQUEST: 
	{
	    itemString = Tcl_GetStringFromObj(objv[firstArg + 2], &length);
	    if (length == 0) {
		Tcl_SetStringObj(Tcl_GetObjResult(interp),
			"cannot request value of null data", -1);
		return TCL_ERROR;
	    }
	    hConv = DdeConnect(instance, service, topic, NULL);
	    DdeFreeStringHandle(instance, service);
	    DdeFreeStringHandle(instance, topic);
	    
	    if (hConv == NULL) {
		SetError(interp);
		result = TCL_ERROR;
	    } else {
		item = DdeCreateStringHandle(instance, itemString, CP_WINANSI);
		if (item != NULL) {
		    data = DdeClientTransaction(NULL, 0, hConv, item, CF_TEXT,
			XTYP_REQUEST, 5000, NULL);
		    if (data == NULL) {
			SetError(interp);
			result = TCL_ERROR;
		    } else {
			Tcl_Obj *objPtr;
			DWORD dataLength;

			dataString = DdeAccessData(data, &dataLength);
			objPtr = Tcl_NewStringObj(dataString, -1);
			DdeUnaccessData(data);
			DdeFreeDataHandle(data);
			Tcl_SetObjResult(interp, objPtr);
		    }
		} else {
		    SetError(interp);
		    result = TCL_ERROR;
		}
	    }

	    break;
	}
    case DDE_POKE: 
	{
	    itemString = Tcl_GetStringFromObj(objv[firstArg + 2], &length);
	    if (length == 0) {
		Tcl_SetStringObj(Tcl_GetObjResult(interp),
			"cannot have a null item", -1);
		return TCL_ERROR;
	    }
	    dataString = Tcl_GetStringFromObj(objv[firstArg + 3], &length);
	    
	    hConv = DdeConnect(instance, service, topic, NULL);
	    DdeFreeStringHandle(instance, service);
	    DdeFreeStringHandle(instance, topic);

	    if (hConv == NULL) {
		SetError(interp);
		result = TCL_ERROR;
	    } else {
		item = DdeCreateStringHandle(instance, itemString,
			CP_WINANSI);
		if (item != NULL) {
		    data = DdeClientTransaction(dataString,length+1,
			    hConv, item, CF_TEXT, XTYP_POKE, 5000, NULL);
		    if (data == NULL) {
			SetError(interp);
			result = TCL_ERROR;
		    }
		} else {
		    SetError(interp);
		    result = TCL_ERROR;
		}
	    }
	    break;
	}

    case DDE_SERVICES: 
	{
	    HCONVLIST hConvList;
	    CONVINFO convInfo;
	    Tcl_Obj *convListObjPtr, *elementObjPtr;
	    Tcl_DString dString;
	    char *name;
	    
	    convInfo.cb = sizeof(CONVINFO);
	    hConvList = DdeConnectList(instance, service, 
                    topic, 0, NULL);
	    DdeFreeStringHandle(instance, service);
	    DdeFreeStringHandle(instance, topic);
	    hConv = 0;
	    convListObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
	    Tcl_DStringInit(&dString);

	    while (hConv = DdeQueryNextServer(hConvList, hConv), hConv != 0) {
		elementObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
		DdeQueryConvInfo(hConv, QID_SYNC, &convInfo);
		length = DdeQueryString(instance, 
                        convInfo.hszSvcPartner, NULL, 0, CP_WINANSI);
		Tcl_DStringSetLength(&dString, length);
		name = Tcl_DStringValue(&dString);
		DdeQueryString(instance, convInfo.hszSvcPartner, 
                        name, length + 1, CP_WINANSI);
		Tcl_ListObjAppendElement(interp, elementObjPtr,
			Tcl_NewStringObj(name, length));
		length = DdeQueryString(instance, convInfo.hszTopic,
			NULL, 0, CP_WINANSI);
		Tcl_DStringSetLength(&dString, length);
		name = Tcl_DStringValue(&dString);
		DdeQueryString(instance, convInfo.hszTopic, name,
			length + 1, CP_WINANSI);
		Tcl_ListObjAppendElement(interp, elementObjPtr,
			Tcl_NewStringObj(name, length));
		Tcl_ListObjAppendElement(interp, convListObjPtr,
			elementObjPtr);
	    }
	    DdeDisconnectList(hConvList);
	    Tcl_SetObjResult(interp, convListObjPtr);
	    Tcl_DStringFree(&dString);
	    break;
	}
    case DDE_EVAL: 
	{
	    objc -= (async + 3);
	    ((Tcl_Obj **) objv) += (async + 3);

            /*
	     * See if the target interpreter is local.  If so, execute
	     * the command directly without going through the DDE
	     * server.  Don't exchange objects between interps.  The
	     * target interp could compile an object, producing a
	     * bytecode structure that refers to other objects owned
	     * by the target interp.  If the target interp is then
	     * deleted, the bytecode structure would be referring to
	     * deallocated objects.
	     */
	    
	    for (riPtr = interps; riPtr != NULL;
		 riPtr = riPtr->nextPtr) {
		if (strcasecmp(serviceName, riPtr->name) == 0) {
		    break;
		}
	    }

	    if (riPtr != NULL) {
		/*
		 * This command is to a local interp. No need to go through
		 * the server.
		 */
		
		Tcl_Preserve(riPtr);
		sendInterp = riPtr->interp;
		Tcl_Preserve(sendInterp);
		
		/*
		 * Don't exchange objects between interps.  The target interp
		 * would compile an object, producing a bytecode structure that
		 * refers to other objects owned by the target interp.  If the
		 * target interp is then deleted, the bytecode structure would
		 * be referring to deallocated objects.
		 */

		if (objc == 1) {
		    result = Tcl_GlobalEval(sendInterp,Tcl_GetString(objv[0]));
		} else {
		    objPtr = Tcl_ConcatObj(objc, objv);
		    Tcl_IncrRefCount(objPtr);
		    result = Tcl_GlobalEval(sendInterp, Tcl_GetString(objPtr));
		    Tcl_DecrRefCount(objPtr);
		}
		if (interp != sendInterp) {
		    if (result == TCL_ERROR) {
			char *value;
			/*
			 * An error occurred, so transfer error information
			 * from the destination interpreter back to our
			 * interpreter.
			 */
			
			Tcl_ResetResult(interp);
			value = Tcl_GetVar2(sendInterp, "errorInfo", NULL, 
				TCL_GLOBAL_ONLY);
			Tcl_AddObjErrorInfo(interp, value, length);
			
			value = Tcl_GetVar2(sendInterp, "errorCode", NULL,
				TCL_GLOBAL_ONLY);
			Tcl_SetErrorCode(interp, value, (char *)NULL);
		    }
		    Tcl_SetObjResult(interp, Tcl_GetObjResult(sendInterp));
		}
		Tcl_Release(riPtr);
		Tcl_Release(sendInterp);
	    } else {
		/*
		 * This is a non-local request. Send the script to the server
		 * and poll it for a result.
		 */
		
		if (MakeConnection(interp, serviceName, &hConv) != TCL_OK) {
		    goto error;
		}
		
		objPtr = Tcl_ConcatObj(objc, objv);
		string = Tcl_GetStringFromObj(objPtr, &length);
		itemData = DdeCreateDataHandle(instance, string,
			length+1, 0, 0, CF_TEXT, 0);
		
		if (async) {
		    DWORD status;

		    data = DdeClientTransaction((LPBYTE) itemData, 0xFFFFFFFF,
			hConv, 0, CF_TEXT, XTYP_EXECUTE, TIMEOUT_ASYNC,
			&status);
		    DdeAbandonTransaction(instance, hConv, status);
		} else {
		    data = DdeClientTransaction((LPBYTE) itemData,
			    0xFFFFFFFF, hConv, 0,
			    CF_TEXT, XTYP_EXECUTE, 30000, NULL);
		    if (data != 0) {
			
			cookie = DdeCreateStringHandle(instance, 
				"$TCLEVAL$EXECUTE$RESULT", CP_WINANSI);
			data = DdeClientTransaction(NULL, 0, hConv,
				cookie, CF_TEXT, XTYP_REQUEST, 30000, NULL);
		    }
		}

		Tcl_DecrRefCount(objPtr);
		
		if (data == 0) {
		    SetError(interp);
		    goto errorNoResult;
		}
		
		if (async == 0) {
		    Tcl_Obj *resultPtr;
		    
		    /*
		     * The return handle has a two or four element list in
		     * it. The first element is the return code (TCL_OK,
		     * TCL_ERROR, etc.). The second is the result of the
		     * script. If the return code is TCL_ERROR, then the third
		     * element is the value of the variable "errorCode", and
		     * the fourth is the value of the variable "errorInfo".
		     */
		    
		    resultPtr = Tcl_NewObj();
		    length = DdeGetData(data, NULL, 0, 0);
		    Tcl_SetObjLength(resultPtr, length);
		    string = Tcl_GetString(resultPtr);
		    DdeGetData(data, string, length, 0);
		    Tcl_SetObjLength(resultPtr, strlen(string));
		    
		    if (Tcl_ListObjIndex(NULL, resultPtr, 0, &objPtr)
			    != TCL_OK) {
			Tcl_DecrRefCount(resultPtr);
			goto error;
		    }
		    if (Tcl_GetIntFromObj(NULL, objPtr, &result) != TCL_OK) {
			Tcl_DecrRefCount(resultPtr);
			goto error;
		    }
		    if (result == TCL_ERROR) {
			Tcl_ResetResult(interp);

			if (Tcl_ListObjIndex(NULL, resultPtr, 3, &objPtr)
				!= TCL_OK) {
			    Tcl_DecrRefCount(resultPtr);
			    goto error;
			}
			length = -1;
			string = Tcl_GetStringFromObj(objPtr, &length);
			Tcl_AddObjErrorInfo(interp, string, length);
			
			Tcl_ListObjIndex(NULL, resultPtr, 2, &objPtr);
			Tcl_SetObjErrorCode(interp, objPtr);
		    }
		    if (Tcl_ListObjIndex(NULL, resultPtr, 1, &objPtr)
			    != TCL_OK) {
			Tcl_DecrRefCount(resultPtr);
			goto error;
		    }
		    Tcl_SetObjResult(interp, objPtr);
		    Tcl_DecrRefCount(resultPtr);
		}
	    }
	}
    }
    if (cookie != NULL) {
	DdeFreeStringHandle(instance, cookie);
    }
    if (item != NULL) {
	DdeFreeStringHandle(instance, item);
    }
    if (itemData != NULL) {
	DdeFreeDataHandle(itemData);
    }
    if (data != NULL) {
	DdeFreeDataHandle(data);
    }
    if (hConv != NULL) {
	DdeDisconnect(hConv);
    }
    return result;

    error:
    Tcl_SetStringObj(Tcl_GetObjResult(interp), 
	     "invalid data returned from server", -1);

    errorNoResult:
    if (cookie != NULL) {
	DdeFreeStringHandle(instance, cookie);
    }
    if (item != NULL) {
	DdeFreeStringHandle(instance, item);
    }
    if (itemData != NULL) {
	DdeFreeDataHandle(itemData);
    }
    if (data != NULL) {
	DdeFreeDataHandle(data);
    }
    if (hConv != NULL) {
	DdeDisconnect(hConv);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_DdeInit --
 *
 *	This procedure initializes the dde command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Blt_DdeInit(interp)
    Tcl_Interp *interp;
{
    Tcl_CreateObjCommand(interp, "dde", DdeObjCmd, NULL, NULL);
    conversations = NULL;
    interps = NULL;
    Tcl_CreateExitHandler(ExitProc, NULL);
    return Tcl_PkgProvide(interp, TCL_DDE_PACKAGE_NAME, TCL_DDE_VERSION);
}

#endif /* NO_DDE */
