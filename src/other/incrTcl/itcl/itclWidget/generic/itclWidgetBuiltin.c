/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tcl]
 *  DESCRIPTION:  Object-Oriented Extensions to Tcl
 *
 *  These procedures handle built-in class methods, including the
 *  "installhull" method for package ItclWidget
 *
 * This implementation is based mostly on the ideas of snit
 * whose author is William Duquette.
 *
 * ========================================================================
 *  Author: Arnulf Wiedemann
 *
 *     RCS:  $Id$
 * ========================================================================
 *           Copyright (c) 2007 Arnulf Wiedemann
 * ------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#include "itclWidgetInt.h"
#include <tk.h>

/*
 *  Standard list of built-in methods for all objects.
 */
typedef struct BiMethod {
    char* name;              /* method name */
    char* usage;             /* string describing usage */
    char* registration;      /* registration name for C proc */
    Tcl_ObjCmdProc *proc;    /* implementation C proc */
} BiMethod;

static BiMethod BiMethodList[] = {
    { "installhull", "using widgetType ?arg ...?",
                   "@itcl-builtin-installhull",  Itcl_BiInstallHullCmd },
};
static int BiMethodListLen = sizeof(BiMethodList)/sizeof(BiMethod);

Tcl_CommandTraceProc ItclHullContentsDeleted;



/*
 * ------------------------------------------------------------------------
 *  Itcl_WidgetBiInit()
 *
 *  Creates a namespace full of built-in methods/procs for [incr Tcl]
 *  classes.  This includes things like the "isa" method and "info"
 *  for querying class info.  Usually invoked by Itcl_Init() when
 *  [incr Tcl] is first installed into an interpreter.
 *
 *  Returns TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
void ItclHullContentsDeleted(
    ClientData clientData,
    Tcl_Interp *interp,
    const char *oldName,
    const char *newName,
    int flags)
{
    Tcl_HashEntry *hPtr;
    ItclObjectInfo *infoPtr;
    ItclObject *ioPtr;
    int result;

    ioPtr = (ItclObject *)clientData;
    infoPtr = Tcl_GetAssocData(interp, ITCL_INTERP_DATA, NULL);
    hPtr = Tcl_FindHashEntry(&infoPtr->objects, (char *)ioPtr);
    if (hPtr == NULL) {
       /* object has already been deleted */
       return;
    }
    if (newName == NULL) {
        /* delete the object which has this as an itcl_hull contents */
        result = Itcl_RenameCommand(ioPtr->interp,
	        Tcl_GetString(ioPtr->origNamePtr), NULL);
/*
fprintf(stderr, "RES!%d!%s!\n", result, Tcl_GetStringResult(ioPtr->iclsPtr->interp));
*/
    }
}

/*
 * ------------------------------------------------------------------------
 *  Itcl_WidgetBiInit()
 *
 *  Creates a namespace full of built-in methods/procs for [incr Tcl]
 *  classes.  This includes things like the "isa" method and "info"
 *  for querying class info.  Usually invoked by Itcl_Init() when
 *  [incr Tcl] is first installed into an interpreter.
 *
 *  Returns TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
int
Itcl_WidgetBiInit(
    Tcl_Interp *interp,      /* current interpreter */
    ItclObjectInfo *infoPtr)
{
    Tcl_DString buffer;
    int i;

    /*
     *  "::itcl::builtin" commands.
     *  These commands are imported into each class
     *  just before the class definition is parsed.
     */
    Tcl_DStringInit(&buffer);
    for (i=0; i < BiMethodListLen; i++) {
	Tcl_DStringSetLength(&buffer, 0);
	Tcl_DStringAppend(&buffer, "::itcl::builtin::", -1);
	Tcl_DStringAppend(&buffer, BiMethodList[i].name, -1);
        Tcl_CreateObjCommand(interp, Tcl_DStringValue(&buffer),
	        BiMethodList[i].proc, (ClientData)infoPtr,
		(Tcl_CmdDeleteProc*)NULL);
    }
    Tcl_DStringFree(&buffer);
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_InstallWidgetBiMethods()
 *
 *  Invoked when a class is first created, just after the class
 *  definition has been parsed, to add definitions for built-in
 *  methods to the class.  If a method already exists in the class
 *  with the same name as the built-in, then the built-in is skipped.
 *  Otherwise, a method definition for the built-in method is added.
 *
 *  Returns TCL_OK if successful, or TCL_ERROR (along with an error
 *  message in the interpreter) if anything goes wrong.
 * ------------------------------------------------------------------------
 */
int
Itcl_InstallWidgetBiMethods(
    Tcl_Interp *interp,      /* current interpreter */
    ItclClass *iclsPtr)      /* class definition to be updated */
{
    int result = TCL_OK;
    Tcl_HashEntry *hPtr = NULL;

    int i;
    ItclHierIter hier;
    ItclClass *superPtr;

    /*
     *  Scan through all of the built-in methods and see if
     *  that method already exists in the class.  If not, add
     *  it in.
     *
     *  TRICKY NOTE:  The virtual tables haven't been built yet,
     *    so look for existing methods the hard way--by scanning
     *    through all classes.
     */
    Tcl_Obj *objPtr = Tcl_NewStringObj("", 0);
    for (i=0; i < BiMethodListLen; i++) {
        Itcl_InitHierIter(&hier, iclsPtr);
	Tcl_SetStringObj(objPtr, BiMethodList[i].name, -1);
        superPtr = Itcl_AdvanceHierIter(&hier);
        while (superPtr) {
            hPtr = Tcl_FindHashEntry(&superPtr->functions, (char *)objPtr);
            if (hPtr) {
                break;
            }
            superPtr = Itcl_AdvanceHierIter(&hier);
        }
        Itcl_DeleteHierIter(&hier);

        if (!hPtr) {
            result = Itcl_CreateMethod(interp, iclsPtr,
	        Tcl_NewStringObj(BiMethodList[i].name, -1),
                BiMethodList[i].usage, BiMethodList[i].registration);

            if (result != TCL_OK) {
                break;
            }
        }
    }
    return result;
}

/*
 * ------------------------------------------------------------------------
 *  Itcl_BiInstallHullCmd()
 *
 *  Invoked whenever the user issues the "installhull" method for an object.
 *  Handles the following syntax:
 *
 *    installhull using <widgetType> ?arg ...?
 *    installhull name
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
int
Itcl_BiInstallHullCmd(
    ClientData clientData,   /* ItclObjectInfo *Ptr */
    Tcl_Interp *interp,      /* current interpreter */
    int objc,                /* number of arguments */
    Tcl_Obj *const objv[])   /* argument objects */
{
    FOREACH_HASH_DECLS;
    Tcl_Obj *namePtr;
    Tcl_Obj *classNamePtr;
    Tcl_Var varPtr;
    Tcl_DString buffer;
    Tcl_Obj **newObjv;
    Tk_Window tkMainWin;
    Tk_Window tkWin;
    ItclClass *contextIclsPtr;
    ItclObject *contextIoPtr;
    ItclObjectInfo *infoPtr;
    ItclVariable *ivPtr;
    ItclOption *ioptPtr;
    ItclDelegatedOption *idoPtr;
    const char *val;
    const char *itclHull;
    const char *component;
    const char *widgetType;
    const char *className;
    const char *widgetName;
    const char *origWidgetName;
    char *token;
    int newObjc;
    int lgth;
    int i;
    int iOpts;
    int noOptionSet;
    int shortForm;
    int numOptArgs;
    int optsStartIdx;
    int result;

    result = TCL_OK;
    tkWin = NULL;
    component = NULL;
    ItclShowArgs(1, "Itcl_BiInstallHullCmd", objc, objv);
    infoPtr = (ItclObjectInfo *)clientData;
    if (infoPtr->buildingWidget) {
	contextIoPtr = infoPtr->currIoPtr;
        contextIclsPtr = contextIoPtr->iclsPtr;
    } else {
        /*
         *  Make sure that this command is being invoked in the proper
         *  context.
         */
        contextIclsPtr = NULL;
        if (Itcl_GetContext(interp, &contextIclsPtr, &contextIoPtr) != TCL_OK) {
            return TCL_ERROR;
        }
    }

    if (contextIoPtr == NULL) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "cannot installhull without an object context", 
            (char*)NULL);
        return TCL_ERROR;
    }
    if (objc < 3) {
	if (objc != 2) {
            token = Tcl_GetString(objv[0]);
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "wrong # args: should be \"", token,
	        "\" name|using <widgetType> ?arg ...?\"", (char*)NULL);
            return TCL_ERROR;
	}
    }
    shortForm = 0;
    widgetName = Tcl_GetString(contextIoPtr->namePtr);
    origWidgetName = widgetName;
    if (objc == 2) {
        shortForm = 1;
        widgetName = Tcl_GetString(objv[1]);
    }
    const char *wName;
    wName = strstr(widgetName, "::");
    if (wName != NULL) {
        widgetName = wName + 2;
    }

    optsStartIdx = 3;
    if (!shortForm) {
        widgetType = Tcl_GetString(objv[2]);
	classNamePtr = NULL;
	className = NULL;
	if (objc > 3) {
            if (strcmp(Tcl_GetString(objv[3]), "-class") == 0) {
                className = Tcl_GetString(objv[4]);
	        optsStartIdx += 2;
	    }
	}
	if (className == NULL) {
	    classNamePtr = ItclCapitalize(widgetType);
	    className = Tcl_GetString(classNamePtr);
        }
	numOptArgs = objc - optsStartIdx;
	newObjc = 4;
	newObjv = (Tcl_Obj **)ckalloc(sizeof(Tcl_Obj *) *
	        (newObjc + numOptArgs));
	newObjv[0] = Tcl_NewStringObj(widgetType, -1);
	newObjv[1] = Tcl_NewStringObj(widgetName, -1);
	newObjv[2] = Tcl_NewStringObj("-class", -1);
	newObjv[3] = Tcl_NewStringObj(className, -1);
	i = 4;
	iOpts = optsStartIdx;
	for (; iOpts < objc; iOpts++, i++) {
	    newObjv[i] = objv[iOpts];
	    Tcl_IncrRefCount(newObjv[i]);
	}
	ItclShowArgs(1, "HullCreate", newObjc + numOptArgs, newObjv);
        result = Tcl_EvalObjv(interp, newObjc + numOptArgs, newObjv, 0);
	for (i = newObjc + numOptArgs - 1; i > 3; i--) {
	    Tcl_DecrRefCount(newObjv[i]);
	}
	ckfree((char *)newObjv);
	if (classNamePtr != NULL) {
	    Tcl_DecrRefCount(classNamePtr);
	}

	/* now initialize the itcl_options array */
        tkMainWin = Tk_MainWindow(interp);
        tkWin = Tk_NameToWindow(interp, origWidgetName, tkMainWin);
        if (tkWin != NULL) {
            FOREACH_HASH_VALUE(ioptPtr, &contextIclsPtr->options) {
                val = Tk_GetOption(tkWin, Tcl_GetString(
		        ioptPtr->resourceNamePtr),
	                Tcl_GetString(ioptPtr->classNamePtr));
	        if (val != NULL) {
                    val = ItclSetInstanceVar(interp, "itcl_options",
	                    Tcl_GetString(ioptPtr->namePtr), val,
                            contextIoPtr, contextIoPtr->iclsPtr);
	        } else {
	            if (ioptPtr->defaultValuePtr != NULL) {
                        val = ItclSetInstanceVar(interp, "itcl_options",
	                        Tcl_GetString(ioptPtr->namePtr),
		                Tcl_GetString(ioptPtr->defaultValuePtr),
                                contextIoPtr, contextIoPtr->iclsPtr);
	            }
		}
	    }
        }
    }

    /* initialize the itcl_hull variable */
    i = 0;
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, ITCL_WIDGETS_NAMESPACE"::hull", -1);
    lgth = strlen(Tcl_DStringValue(&buffer));
    while (1) {
	Tcl_DStringSetLength(&buffer, lgth);
	i++;
	char buf[10];
	sprintf(buf, "%d", i);
	Tcl_DStringAppend(&buffer, buf, -1);
        Tcl_DStringAppend(&buffer, Tcl_GetString(contextIoPtr->namePtr), -1);
	if (Tcl_FindCommand(interp, Tcl_DStringValue(&buffer), NULL, 0)
	        == NULL) {
            break;
	}
    }
    contextIoPtr->hullWindowNamePtr = Tcl_NewStringObj(widgetName, -1);
/*
fprintf(stderr, "REN!%s!%s!\n", widgetName, Tcl_DStringValue(&buffer)); 
*/
    Itcl_RenameCommand(interp, widgetName,
            Tcl_DStringValue(&buffer));
    result = Tcl_TraceCommand(interp, Tcl_DStringValue(&buffer),
            TCL_TRACE_RENAME|TCL_TRACE_DELETE,
            ItclHullContentsDeleted, contextIoPtr);

    namePtr = Tcl_NewStringObj("itcl_hull", -1);
    Tcl_IncrRefCount(namePtr);
    hPtr = Tcl_FindHashEntry(&contextIoPtr->iclsPtr->variables,
            (char *)namePtr);
    Tcl_DecrRefCount(namePtr);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "cannot find class variable itcl_hull", NULL);
        return TCL_ERROR;
    }
    ivPtr =Tcl_GetHashValue(hPtr);
    if (ivPtr->initted <= 1) {
        ivPtr->initted = 0;
        hPtr = Tcl_FindHashEntry(&contextIoPtr->objectVariables, (char *)ivPtr);
        varPtr = Tcl_GetHashValue(hPtr);
        val = ItclSetInstanceVar(interp, "itcl_hull", NULL,
                Tcl_DStringValue(&buffer), contextIoPtr, contextIoPtr->iclsPtr);
        ivPtr->initted = 2;
        if (val == NULL) {
            Tcl_AppendResult(interp, "cannot set itcl_hull for object \"",
                Tcl_GetString(contextIoPtr->namePtr), "\"", NULL);
            Tcl_DStringFree(&buffer);
            return TCL_ERROR;
        }
    }
    itclHull = ItclGetInstanceVar(interp, "itcl_hull", NULL,
            contextIoPtr, contextIoPtr->iclsPtr);
    /* now initialize the delegated options from the option database */
    if (itclHull != NULL) {
        tkMainWin = Tk_MainWindow(interp);
        tkWin = Tk_NameToWindow(interp, origWidgetName, tkMainWin);
        if (tkWin != NULL) {
            FOREACH_HASH_VALUE(idoPtr, &contextIclsPtr->delegatedOptions) {
		val = NULL;
		if (idoPtr->resourceNamePtr == NULL) {
		    /* that is the case when delegating "*" */
		    continue;
		}
		/* check if not in the command line options */
		/* these have higher priority */
		const char *optionName;
		optionName = Tcl_GetString(idoPtr->namePtr);
		if (idoPtr->asPtr != NULL) {
		    optionName = Tcl_GetString(idoPtr->asPtr);
		}
		noOptionSet = 0;
		for (i = optsStartIdx; i < objc; i += 2) {
		    if (strcmp(optionName, Tcl_GetString(objv[i])) == 0) {
		        noOptionSet = 1;
		        break;
		    }
		}
		if (noOptionSet) {
		    break;
		}
                val = Tk_GetOption(tkWin,
		        Tcl_GetString(idoPtr->resourceNamePtr),
		        idoPtr->classNamePtr == NULL ? NULL :
		        Tcl_GetString(idoPtr->classNamePtr));
	        if (val != NULL) {
	            newObjv = (Tcl_Obj **)ckalloc(sizeof(Tcl_Obj *) * 4);
                    component = ItclGetInstanceVar(interp,
		            Tcl_GetString(idoPtr->icPtr->namePtr), NULL,
                            contextIoPtr, contextIoPtr->iclsPtr);
		    if ((component != NULL) && (strlen(component) > 0)) {
		        newObjv[0] = Tcl_NewStringObj(component, -1);
		        Tcl_IncrRefCount(newObjv[0]);
		        newObjv[1] = Tcl_NewStringObj("configure", -1);
		        Tcl_IncrRefCount(newObjv[1]);
		        if (idoPtr->asPtr == NULL) {
		            newObjv[2] = idoPtr->namePtr;
		        } else {
		            newObjv[2] = idoPtr->asPtr;
		        }
		        Tcl_IncrRefCount(newObjv[2]);
		        newObjv[3] = Tcl_NewStringObj(val, -1);
		        Tcl_IncrRefCount(newObjv[3]);
                        ItclShowArgs(1, "SET OPTION", 4, newObjv);
                        result = Tcl_EvalObjv(interp, 4, newObjv, 0);
		        Tcl_DecrRefCount(newObjv[3]);
		        Tcl_DecrRefCount(newObjv[2]);
		        Tcl_DecrRefCount(newObjv[1]);
		        Tcl_DecrRefCount(newObjv[0]);
	            }
	        }
	    }
        }
    }
    return result;
}
