/*
 * tclOO.c --
 *
 *	This file contains the object-system core (NB: not Tcl_Obj, but ::oo)
 *
 * Copyright (c) 2005-2008 by Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "tclInt.h"
#include "tclOOInt.h"

/*
 * Commands in oo::define.
 */

static const struct {
    const char *name;
    Tcl_ObjCmdProc *objProc;
    int flag;
} defineCmds[] = {
    {"constructor", TclOODefineConstructorObjCmd, 0},
    {"deletemethod", TclOODefineDeleteMethodObjCmd, 0},
    {"destructor", TclOODefineDestructorObjCmd, 0},
    {"export", TclOODefineExportObjCmd, 0},
    {"filter", TclOODefineFilterObjCmd, 0},
    {"forward", TclOODefineForwardObjCmd, 0},
    {"method", TclOODefineMethodObjCmd, 0},
    {"mixin", TclOODefineMixinObjCmd, 0},
    {"renamemethod", TclOODefineRenameMethodObjCmd, 0},
    {"self", TclOODefineSelfObjCmd, 0},
    {"superclass", TclOODefineSuperclassObjCmd, 0},
    {"unexport", TclOODefineUnexportObjCmd, 0},
    {"variable", TclOODefineVariablesObjCmd, 0},
    {NULL, NULL, 0}
}, objdefCmds[] = {
    {"class", TclOODefineClassObjCmd, 1},
    {"deletemethod", TclOODefineDeleteMethodObjCmd, 1},
    {"export", TclOODefineExportObjCmd, 1},
    {"filter", TclOODefineFilterObjCmd, 1},
    {"forward", TclOODefineForwardObjCmd, 1},
    {"method", TclOODefineMethodObjCmd, 1},
    {"mixin", TclOODefineMixinObjCmd, 1},
    {"renamemethod", TclOODefineRenameMethodObjCmd, 1},
    {"unexport", TclOODefineUnexportObjCmd, 1},
    {"variable", TclOODefineVariablesObjCmd, 1},
    {NULL, NULL, 0}
};

/*
 * What sort of size of things we like to allocate.
 */

#define ALLOC_CHUNK 8

/*
 * Function declarations for things defined in this file.
 */

static Class *		AllocClass(Tcl_Interp *interp, Object *useThisObj);
static Object *		AllocObject(Tcl_Interp *interp, const char *nameStr,
			    const char *nsNameStr);
static int		CloneClassMethod(Tcl_Interp *interp, Class *clsPtr,
			    Method *mPtr, Tcl_Obj *namePtr,
			    Method **newMPtrPtr);
static int		CloneObjectMethod(Tcl_Interp *interp, Object *oPtr,
			    Method *mPtr, Tcl_Obj *namePtr);
static void		DeletedDefineNamespace(ClientData clientData);
static void		DeletedObjdefNamespace(ClientData clientData);
static void		DeletedHelpersNamespace(ClientData clientData);
static int		FinalizeAlloc(ClientData data[],
			    Tcl_Interp *interp, int result);
static int		FinalizeNext(ClientData data[],
			    Tcl_Interp *interp, int result);
static int		FinalizeObjectCall(ClientData data[],
			    Tcl_Interp *interp, int result);
static void		InitFoundation(Tcl_Interp *interp);
static void		KillFoundation(ClientData clientData,
			    Tcl_Interp *interp);
static void		ObjectNamespaceDeleted(ClientData clientData);
static void		ObjectRenamedTrace(ClientData clientData,
			    Tcl_Interp *interp, const char *oldName,
			    const char *newName, int flags);
static void		ReleaseClassContents(Tcl_Interp *interp,Object *oPtr);

static int		PublicObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);
static int		PublicNRObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);
static int		PrivateObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);
static int		PrivateNRObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);

/*
 * Methods in the oo::object and oo::class classes. First, we define a helper
 * macro that makes building the method type declaration structure a lot
 * easier. No point in making life harder than it has to be!
 *
 * Note that the core methods don't need clone or free proc callbacks.
 */

#define DCM(name,visibility,proc) \
    {name,visibility,\
	{TCL_OO_METHOD_VERSION_CURRENT,"core method: "#name,proc,NULL,NULL}}

static const DeclaredClassMethod objMethods[] = {
    DCM("destroy", 1,	TclOO_Object_Destroy),
    DCM("eval", 0,	TclOO_Object_Eval),
    DCM("unknown", 0,	TclOO_Object_Unknown),
    DCM("variable", 0,	TclOO_Object_LinkVar),
    DCM("varname", 0,	TclOO_Object_VarName),
    {NULL, 0, {0, NULL, NULL, NULL, NULL}}
}, clsMethods[] = {
    DCM("create", 1,	TclOO_Class_Create),
    DCM("new", 1,	TclOO_Class_New),
    DCM("createWithNamespace", 0, TclOO_Class_CreateNs),
    {NULL, 0, {0, NULL, NULL, NULL, NULL}}
};

static char initScript[] =
    "namespace eval ::oo { variable version " TCLOO_VERSION " };"
    "namespace eval ::oo { variable patchlevel " TCLOO_PATCHLEVEL " };";
/*     "tcl_findLibrary tcloo $oo::version $oo::version" */
/*     " tcloo.tcl OO_LIBRARY oo::library;"; */

MODULE_SCOPE const TclOOStubs * const tclOOConstStubPtr;

/*
 * Convenience macro for getting the foundation from an interpreter.
 */

#define GetFoundation(interp) \
	((Foundation *)((Interp *)(interp))->objectFoundation)

/*
 * ----------------------------------------------------------------------
 *
 * TclOOInit --
 *
 *	Called to initialise the OO system within an interpreter.
 *
 * Result:
 *	TCL_OK if the setup succeeded. Currently assumed to always work.
 *
 * Side effects:
 *	Creates namespaces, commands, several classes and a number of
 *	callbacks. Upon return, the OO system is ready for use.
 *
 * ----------------------------------------------------------------------
 */

int
TclOOInit(
    Tcl_Interp *interp)		/* The interpreter to install into. */
{
    /*
     * Build the core of the OO system.
     */

    InitFoundation(interp);

    /*
     * Run our initialization script and, if that works, declare the package
     * to be fully provided.
     */

    if (Tcl_Eval(interp, initScript) != TCL_OK) {
	return TCL_ERROR;
    }

    return Tcl_PkgProvideEx(interp, "TclOO", TCLOO_VERSION,
	    (ClientData) tclOOConstStubPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOGetFoundation --
 *
 *	Get a reference to the OO core class system.
 *
 * ----------------------------------------------------------------------
 */

Foundation *
TclOOGetFoundation(
    Tcl_Interp *interp)
{
    return GetFoundation(interp);
}

/*
 * ----------------------------------------------------------------------
 *
 * InitFoundation --
 *
 *	Set up the core of the OO core class system. This is a structure
 *	holding references to the magical bits that need to be known about in
 *	other places, plus the oo::object and oo::class classes.
 *
 * ----------------------------------------------------------------------
 */

static void
InitFoundation(
    Tcl_Interp *interp)
{
    static Tcl_ThreadDataKey tsdKey;
    ThreadLocalData *tsdPtr =
	    Tcl_GetThreadData(&tsdKey, sizeof(ThreadLocalData));
    Foundation *fPtr = (Foundation *) ckalloc(sizeof(Foundation));
    Tcl_Obj *namePtr, *argsPtr, *bodyPtr;
    Tcl_DString buffer;
    int i;

    /*
     * Initialize the structure that holds the OO system core. This is
     * attached to the interpreter via an assocData entry; not very efficient,
     * but the best we can do without hacking the core more.
     */

    memset(fPtr, 0, sizeof(Foundation));
    ((Interp *) interp)->objectFoundation = fPtr;
    fPtr->interp = interp;
    fPtr->ooNs = Tcl_CreateNamespace(interp, "::oo", fPtr, NULL);
    Tcl_Export(interp, fPtr->ooNs, "[a-z]*", 1);
    fPtr->defineNs = Tcl_CreateNamespace(interp, "::oo::define", fPtr,
	    DeletedDefineNamespace);
    fPtr->objdefNs = Tcl_CreateNamespace(interp, "::oo::objdefine", fPtr,
	    DeletedObjdefNamespace);
    fPtr->helpersNs = Tcl_CreateNamespace(interp, "::oo::Helpers", fPtr,
	    DeletedHelpersNamespace);
    fPtr->epoch = 0;
    fPtr->tsdPtr = tsdPtr;
    fPtr->unknownMethodNameObj = Tcl_NewStringObj("unknown", -1);
    fPtr->constructorName = Tcl_NewStringObj("<constructor>", -1);
    fPtr->destructorName = Tcl_NewStringObj("<destructor>", -1);
    Tcl_IncrRefCount(fPtr->unknownMethodNameObj);
    Tcl_IncrRefCount(fPtr->constructorName);
    Tcl_IncrRefCount(fPtr->destructorName);
    Tcl_NRCreateCommand(interp, "::oo::UpCatch", TclOOUpcatchCmd,
	    TclOONRUpcatch, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::oo::UnknownDefinition",
	    TclOOUnknownDefinition, NULL, NULL);
    namePtr = Tcl_NewStringObj("::oo::UnknownDefinition", -1);
    Tcl_SetNamespaceUnknownHandler(interp, fPtr->defineNs, namePtr);
    Tcl_SetNamespaceUnknownHandler(interp, fPtr->objdefNs, namePtr);

    /*
     * Create the subcommands in the oo::define and oo::objdefine spaces.
     */

    Tcl_DStringInit(&buffer);
    for (i=0 ; defineCmds[i].name ; i++) {
	Tcl_DStringAppend(&buffer, "::oo::define::", 14);
	Tcl_DStringAppend(&buffer, defineCmds[i].name, -1);
	Tcl_CreateObjCommand(interp, Tcl_DStringValue(&buffer),
		defineCmds[i].objProc, INT2PTR(defineCmds[i].flag), NULL);
	Tcl_DStringFree(&buffer);
    }
    for (i=0 ; objdefCmds[i].name ; i++) {
	Tcl_DStringAppend(&buffer, "::oo::objdefine::", 17);
	Tcl_DStringAppend(&buffer, objdefCmds[i].name, -1);
	Tcl_CreateObjCommand(interp, Tcl_DStringValue(&buffer),
		objdefCmds[i].objProc, INT2PTR(objdefCmds[i].flag), NULL);
	Tcl_DStringFree(&buffer);
    }

    Tcl_CallWhenDeleted(interp, KillFoundation, NULL);

    /*
     * Create the objects at the core of the object system. These need to be
     * spliced manually.
     */

    fPtr->objectCls = AllocClass(interp,
	    AllocObject(interp, "::oo::object", NULL));
    fPtr->classCls = AllocClass(interp,
	    AllocObject(interp, "::oo::class", NULL));
    fPtr->objectCls->thisPtr->selfCls = fPtr->classCls;
    fPtr->objectCls->thisPtr->flags |= ROOT_OBJECT;
    fPtr->objectCls->superclasses.num = 0;
    ckfree((char *) fPtr->objectCls->superclasses.list);
    fPtr->objectCls->superclasses.list = NULL;
    fPtr->classCls->thisPtr->selfCls = fPtr->classCls;
    TclOOAddToInstances(fPtr->objectCls->thisPtr, fPtr->classCls);
    TclOOAddToInstances(fPtr->classCls->thisPtr, fPtr->classCls);
    AddRef(fPtr->objectCls->thisPtr);
    AddRef(fPtr->objectCls);

    /*
     * Basic method declarations for the core classes.
     */

    for (i=0 ; objMethods[i].name ; i++) {
	TclOONewBasicMethod(interp, fPtr->objectCls, &objMethods[i]);
    }
    for (i=0 ; clsMethods[i].name ; i++) {
	TclOONewBasicMethod(interp, fPtr->classCls, &clsMethods[i]);
    }

    /*
     * Finish setting up the class of classes by marking the 'new' method as
     * private; classes, unlike general objects, must have explicit names. We
     * also need to create the constructor for classes.
     *
     * The 0xDeadBeef is a special signal to the errorInfo logger that is used
     * by constructors that stops it from generating extra error information
     * that is confusing.
     */

    namePtr = Tcl_NewStringObj("new", -1);
    Tcl_NewInstanceMethod(interp, (Tcl_Object) fPtr->classCls->thisPtr,
	    namePtr /* keeps ref */, 0 /* ==private */, NULL, NULL);

    argsPtr = Tcl_NewStringObj("{definitionScript {}}", -1);
    Tcl_IncrRefCount(argsPtr);
    bodyPtr = Tcl_NewStringObj(
	    "set script [list ::oo::define [self] $definitionScript];"
	    "lassign [::oo::UpCatch $script] msg opts\n"
	    "if {[dict get $opts -code] == 1} {"
	    "    dict set opts -errorline 0xDeadBeef\n"
	    "}\n"
	    "return -options $opts $msg", -1);
    fPtr->classCls->constructorPtr = TclOONewProcMethod(interp,
	    fPtr->classCls, 0, NULL, argsPtr, bodyPtr, NULL);
    Tcl_DecrRefCount(argsPtr);

    /*
     * Create non-object commands and plug ourselves into the Tcl [info]
     * ensemble.
     */

    Tcl_CreateObjCommand(interp, "::oo::Helpers::next", TclOONextObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "::oo::Helpers::self", TclOOSelfObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "::oo::define", TclOODefineObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "::oo::objdefine", TclOOObjDefObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "::oo::copy", TclOOCopyObjectCmd, NULL,NULL);
    TclOOInitInfo(interp);
}

/*
 * ----------------------------------------------------------------------
 *
 * DeletedDefineNamespace, DeletedObjdefNamespace, DeletedHelpersNamespace --
 *
 *	Simple helpers used to clear fields of the foundation when they no
 *	longer hold useful information.
 *
 * ----------------------------------------------------------------------
 */

static void
DeletedDefineNamespace(
    ClientData clientData)
{
    Foundation *fPtr = clientData;

    fPtr->defineNs = NULL;
}

static void
DeletedObjdefNamespace(
    ClientData clientData)
{
    Foundation *fPtr = clientData;

    fPtr->objdefNs = NULL;
}

static void
DeletedHelpersNamespace(
    ClientData clientData)
{
    Foundation *fPtr = clientData;

    fPtr->helpersNs = NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * KillFoundation --
 *
 *	Delete those parts of the OO core that are not deleted automatically
 *	when the objects and classes themselves are destroyed.
 *
 * ----------------------------------------------------------------------
 */

static void
KillFoundation(
    ClientData clientData,	/* Pointer to the OO system foundation
				 * structure. */
    Tcl_Interp *interp)		/* The interpreter containing the OO system
				 * foundation. */
{
    Foundation *fPtr = GetFoundation(interp);

    DelRef(fPtr->objectCls->thisPtr);
    DelRef(fPtr->objectCls);
    Tcl_DecrRefCount(fPtr->unknownMethodNameObj);
    Tcl_DecrRefCount(fPtr->constructorName);
    Tcl_DecrRefCount(fPtr->destructorName);
    ckfree((char *) fPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * AllocObject --
 *
 *	Allocate an object of basic type. Does not splice the object into its
 *	class's instance list.
 *
 * ----------------------------------------------------------------------
 */

static Object *
AllocObject(
    Tcl_Interp *interp,		/* Interpreter within which to create the
				 * object. */
    const char *nameStr,	/* The name of the object to create, or NULL
				 * if the OO system should pick the object
				 * name itself (equal to the namespace
				 * name). */
    const char *nsNameStr)	/* The name of the namespace to create, or
				 * NULL if the OO system should pick a unique
				 * name itself. If this is non-NULL but names
				 * a namespace that already exists, the effect
				 * will be the same as if this was NULL. */
{
    Foundation *fPtr = GetFoundation(interp);
    Tcl_DString buffer;
    Object *oPtr;
    int creationEpoch;

    oPtr = (Object *) ckalloc(sizeof(Object));
    memset(oPtr, 0, sizeof(Object));

    /*
     * Every object has a namespace; make one. Note that this also normally
     * computes the creation epoch value for the object, a sequence number
     * that is unique to the object (and which allows us to manage method
     * caching without comparing pointers).
     *
     * When creating a namespace, we first check to see if the caller
     * specified the name for the namespace. If not, we generate namespace
     * names using the epoch until such time as a new namespace is actually
     * created.
     */

    if (nsNameStr != NULL) {
	oPtr->namespacePtr = Tcl_CreateNamespace(interp, nsNameStr, oPtr,
		ObjectNamespaceDeleted);
	if (oPtr->namespacePtr != NULL) {
	    creationEpoch = ++fPtr->tsdPtr->nsCount;
	    goto configNamespace;
	}
	Tcl_ResetResult(interp);
    }

    while (1) {
	char objName[10 + TCL_INTEGER_SPACE];

	sprintf(objName, "::oo::Obj%d", ++fPtr->tsdPtr->nsCount);
	oPtr->namespacePtr = Tcl_CreateNamespace(interp, objName, oPtr,
		ObjectNamespaceDeleted);
	if (oPtr->namespacePtr != NULL) {
	    creationEpoch = fPtr->tsdPtr->nsCount;
	    break;
	}

	/*
	 * Could not make that namespace, so we make another. But first we
	 * have to get rid of the error message from Tcl_CreateNamespace,
	 * since that's something that should not be exposed to the user.
	 */

	Tcl_ResetResult(interp);
    }

    /*
     * Make the namespace know about the helper commands. This grants access
     * to the [self] and [next] commands.
     */

  configNamespace:
    if (fPtr->helpersNs != NULL) {
	TclSetNsPath((Namespace *) oPtr->namespacePtr, 1, &fPtr->helpersNs);
    }
    TclOOSetupVariableResolver(oPtr->namespacePtr);

    /*
     * Suppress use of compiled versions of the commands in this object's
     * namespace and its children; causes wrong behaviour without expensive
     * recompilation. [Bug 2037727]
     */

    ((Namespace *) oPtr->namespacePtr)->flags |= NS_SUPPRESS_COMPILATION;

    /*
     * Fill in the rest of the non-zero/NULL parts of the structure.
     */

    oPtr->fPtr = fPtr;
    oPtr->selfCls = fPtr->objectCls;
    oPtr->creationEpoch = creationEpoch;
    oPtr->refCount = 1;
    oPtr->flags = USE_CLASS_CACHE;

    /*
     * Finally, create the object commands and initialize the trace on the
     * public command (so that the object structures are deleted when the
     * command is deleted).
     */

    if (nameStr) {
	if (nameStr[0] != ':' || nameStr[1] != ':') {
	    Tcl_DStringInit(&buffer);
	    Tcl_DStringAppend(&buffer,
		    Tcl_GetCurrentNamespace(interp)->fullName, -1);
	    Tcl_DStringAppend(&buffer, "::", 2);
	    Tcl_DStringAppend(&buffer, nameStr, -1);
	    oPtr->command = Tcl_CreateObjCommand(interp,
		    Tcl_DStringValue(&buffer), PublicObjectCmd, oPtr, NULL);
	    Tcl_DStringFree(&buffer);
	} else {
	    oPtr->command = Tcl_CreateObjCommand(interp, nameStr,
		    PublicObjectCmd, oPtr, NULL);
	}
    } else {
	oPtr->command = Tcl_CreateObjCommand(interp,
		oPtr->namespacePtr->fullName, PublicObjectCmd, oPtr, NULL);
    }
    ((Command *) oPtr->command)->nreProc = PublicNRObjectCmd;

    /*
     * Access the namespace command table directly when creating "my" to avoid
     * a bottleneck in string manipulation.
     */

    {
	register Command *cmdPtr = (Command *) ckalloc(sizeof(Command));

	memset(cmdPtr, 0, sizeof(Command));
	cmdPtr->nsPtr = (Namespace *) oPtr->namespacePtr;
	cmdPtr->hPtr = Tcl_CreateHashEntry(&cmdPtr->nsPtr->cmdTable, "my",
		&creationEpoch /*ignored*/ );
	cmdPtr->refCount = 1;
	cmdPtr->objProc = PrivateObjectCmd;
	cmdPtr->objClientData = oPtr;
	cmdPtr->proc = TclInvokeObjectCommand;
	cmdPtr->clientData = cmdPtr;
	cmdPtr->nreProc = PrivateNRObjectCmd;
	Tcl_SetHashValue(cmdPtr->hPtr, cmdPtr);
    }

    Tcl_TraceCommand(interp, TclGetString(TclOOObjectName(interp, oPtr)),
	    TCL_TRACE_RENAME|TCL_TRACE_DELETE, ObjectRenamedTrace, oPtr);

    return oPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * ObjectRenamedTrace --
 *
 *	This callback is triggered when the object is deleted by any
 *	mechanism. It runs the destructors and arranges for the actual cleanup
 *	of the object's namespace, which in turn triggers cleansing of the
 *	object data structures.
 *
 * ----------------------------------------------------------------------
 */

static void
ObjectRenamedTrace(
    ClientData clientData,	/* The object being deleted. */
    Tcl_Interp *interp,		/* The interpreter containing the object. */
    const char *oldName,	/* What the object was (last) called. */
    const char *newName,	/* Always NULL. */
    int flags)			/* Why was the object deleted? */
{
    Object *oPtr = clientData;
    Class *clsPtr;
    CallContext *contextPtr;

    /*
     * If this is a rename and not a delete of the object, we just flush the
     * cache of the object name.
     */

    if (flags & TCL_TRACE_RENAME) {
	if (oPtr->cachedNameObj) {
	    Tcl_DecrRefCount(oPtr->cachedNameObj);
	    oPtr->cachedNameObj = NULL;
	}
	return;
    }

    /*
     * Oh dear, the object really is being deleted. Handle this by running the
     * destructors and deleting the object's namespace, which in turn causes
     * the real object structures to be deleted.
     */

    AddRef(oPtr);
    oPtr->flags |= OBJECT_DELETED;

    contextPtr = TclOOGetCallContext(oPtr, NULL, DESTRUCTOR, NULL);
    if (contextPtr != NULL) {
	int result;
	Tcl_InterpState state;

	contextPtr->callPtr->flags |= DESTRUCTOR;
	contextPtr->skip = 0;
	state = Tcl_SaveInterpState(interp, TCL_OK);
	result = Tcl_NRCallObjProc(interp, TclOOInvokeContext, contextPtr, 0,
		NULL);
	if (result != TCL_OK) {
	    Tcl_BackgroundError(interp);
	}
	Tcl_RestoreInterpState(interp, state);
	TclOODeleteContext(contextPtr);
    }

    /*
     * OK, the destructor's been run. Time to splat the class data (if any)
     * and nuke the namespace (which triggers the final crushing of the object
     * structure itself).
     */

    clsPtr = oPtr->classPtr;
    if (clsPtr != NULL) {
	AddRef(clsPtr);
	ReleaseClassContents(interp, oPtr);
    }
    Tcl_DeleteNamespace(oPtr->namespacePtr);
    if (clsPtr) {
	DelRef(clsPtr);
    }
    DelRef(oPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * ReleaseClassContents --
 *
 *	Tear down the special class data structure, including deleting all
 *	dependent classes and objects.
 *
 * ----------------------------------------------------------------------
 */

static void
ReleaseClassContents(
    Tcl_Interp *interp,		/* The interpreter containing the class. */
    Object *oPtr)		/* The object representing the class. */
{
    int i, n;
    Class *clsPtr = oPtr->classPtr, **list;
    Object **insts;

    /*
     * Must empty list before processing the members of the list so that
     * things happen in the correct order even if something tries to play
     * fast-and-loose.
     */

    list = clsPtr->mixinSubs.list;
    n = clsPtr->mixinSubs.num;
    clsPtr->mixinSubs.list = NULL;
    clsPtr->mixinSubs.num = 0;
    clsPtr->mixinSubs.size = 0;
    for (i=0 ; i<n ; i++) {
	AddRef(list[i]);
	AddRef(list[i]->thisPtr);
    }
    for (i=0 ; i<n ; i++) {
	if (!(list[i]->thisPtr->flags & OBJECT_DELETED)) {
	    list[i]->thisPtr->flags |= OBJECT_DELETED;
	    Tcl_DeleteCommandFromToken(interp, list[i]->thisPtr->command);
	}
	DelRef(list[i]->thisPtr);
	DelRef(list[i]);
    }
    if (list != NULL) {
	ckfree((char *) list);
    }

    list = clsPtr->subclasses.list;
    n = clsPtr->subclasses.num;
    clsPtr->subclasses.list = NULL;
    clsPtr->subclasses.num = 0;
    clsPtr->subclasses.size = 0;
    for (i=0 ; i<n ; i++) {
	AddRef(list[i]);
	AddRef(list[i]->thisPtr);
    }
    for (i=0 ; i<n ; i++) {
	if (!(list[i]->thisPtr->flags & OBJECT_DELETED)) {
	    list[i]->thisPtr->flags |= OBJECT_DELETED;
	    Tcl_DeleteCommandFromToken(interp, list[i]->thisPtr->command);
	}
	DelRef(list[i]->thisPtr);
	DelRef(list[i]);
    }
    if (list != NULL) {
	ckfree((char *) list);
    }

    insts = clsPtr->instances.list;
    n = clsPtr->instances.num;
    clsPtr->instances.list = NULL;
    clsPtr->instances.num = 0;
    clsPtr->instances.size = 0;
    for (i=0 ; i<n ; i++) {
	AddRef(insts[i]);
    }
    for (i=0 ; i<n ; i++) {
	if (!(insts[i]->flags & OBJECT_DELETED)) {
	    insts[i]->flags |= OBJECT_DELETED;
	    Tcl_DeleteCommandFromToken(interp, insts[i]->command);
	}
	DelRef(insts[i]);
    }
    if (insts != NULL) {
	ckfree((char *) insts);
    }

    if (clsPtr->constructorChainPtr) {
	TclOODeleteChain(clsPtr->constructorChainPtr);
	clsPtr->constructorChainPtr = NULL;
    }
    if (clsPtr->destructorChainPtr) {
	TclOODeleteChain(clsPtr->destructorChainPtr);
	clsPtr->destructorChainPtr = NULL;
    }
    if (clsPtr->classChainCache) {
	FOREACH_HASH_DECLS;
	CallChain *callPtr;

	FOREACH_HASH_VALUE(callPtr, clsPtr->classChainCache) {
	    TclOODeleteChain(callPtr);
	}
	Tcl_DeleteHashTable(clsPtr->classChainCache);
	ckfree((char *) clsPtr->classChainCache);
	clsPtr->classChainCache = NULL;
    }

    if (clsPtr->filters.num) {
	Tcl_Obj *filterObj;

	FOREACH(filterObj, clsPtr->filters) {
	    Tcl_DecrRefCount(filterObj);
	}
	ckfree((char *) clsPtr->filters.list);
	clsPtr->filters.num = 0;
    }


    if (clsPtr->metadataPtr != NULL) {
	FOREACH_HASH_DECLS;
	Tcl_ObjectMetadataType *metadataTypePtr;
	ClientData value;

	FOREACH_HASH(metadataTypePtr, value, clsPtr->metadataPtr) {
	    metadataTypePtr->deleteProc(value);
	}
	Tcl_DeleteHashTable(clsPtr->metadataPtr);
	ckfree((char *) clsPtr->metadataPtr);
	clsPtr->metadataPtr = NULL;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ObjectNamespaceDeleted --
 *
 *	Callback when the object's namespace is deleted. Used to clean up the
 *	data structures associated with the object. The complicated bit is
 *	that this can sometimes happen before the object's command is deleted
 *	(interpreter teardown is complex!)
 *
 * ----------------------------------------------------------------------
 */

static void
ObjectNamespaceDeleted(
    ClientData clientData)	/* Pointer to the class whose namespace is
				 * being deleted. */
{
    Object *oPtr = clientData;
    FOREACH_HASH_DECLS;
    Class *clsPtr = oPtr->classPtr, *mixinPtr;
    Method *mPtr;
    Tcl_Obj *filterObj, *variableObj;
    int i, preserved = !(oPtr->flags & OBJECT_DELETED);

    /*
     * Instruct everyone to no longer use any allocated fields of the object.
     */

    if (preserved) {
	AddRef(oPtr);
	if (clsPtr != NULL) {
	    AddRef(clsPtr);
	    ReleaseClassContents(NULL, oPtr);
	}
    }
    oPtr->flags |= OBJECT_DELETED;

    /*
     * Splice the object out of its context. After this, we must *not* call
     * methods on the object.
     */

    if (!(oPtr->flags & ROOT_OBJECT)) {
	TclOORemoveFromInstances(oPtr, oPtr->selfCls);
    }

    FOREACH(mixinPtr, oPtr->mixins) {
	TclOORemoveFromInstances(oPtr, mixinPtr);
    }
    if (i) {
	ckfree((char *) oPtr->mixins.list);
    }

    FOREACH(filterObj, oPtr->filters) {
	Tcl_DecrRefCount(filterObj);
    }
    if (i) {
	ckfree((char *) oPtr->filters.list);
    }

    if (oPtr->methodsPtr) {
	FOREACH_HASH_VALUE(mPtr, oPtr->methodsPtr) {
	    TclOODelMethodRef(mPtr);
	}
	Tcl_DeleteHashTable(oPtr->methodsPtr);
	ckfree((char *) oPtr->methodsPtr);
    }

    FOREACH(variableObj, oPtr->variables) {
	Tcl_DecrRefCount(variableObj);
    }
    if (i) {
	ckfree((char *) oPtr->variables.list);
    }

    if (oPtr->chainCache) {
	TclOODeleteChainCache(oPtr->chainCache);
    }

    if (oPtr->cachedNameObj) {
	Tcl_DecrRefCount(oPtr->cachedNameObj);
	oPtr->cachedNameObj = NULL;
    }

    if (oPtr->metadataPtr != NULL) {
	Tcl_ObjectMetadataType *metadataTypePtr;
	ClientData value;

	FOREACH_HASH(metadataTypePtr, value, oPtr->metadataPtr) {
	    metadataTypePtr->deleteProc(value);
	}
	Tcl_DeleteHashTable(oPtr->metadataPtr);
	ckfree((char *) oPtr->metadataPtr);
	oPtr->metadataPtr = NULL;
    }

    if (clsPtr != NULL) {
	Class *superPtr, *mixinPtr;

	if (clsPtr->metadataPtr != NULL) {
	    FOREACH_HASH_DECLS;
	    Tcl_ObjectMetadataType *metadataTypePtr;
	    ClientData value;

	    FOREACH_HASH(metadataTypePtr, value, clsPtr->metadataPtr) {
		metadataTypePtr->deleteProc(value);
	    }
	    Tcl_DeleteHashTable(clsPtr->metadataPtr);
	    ckfree((char *) clsPtr->metadataPtr);
	    clsPtr->metadataPtr = NULL;
	}

	FOREACH(filterObj, clsPtr->filters) {
	    Tcl_DecrRefCount(filterObj);
	}
	if (i) {
	    ckfree((char *) clsPtr->filters.list);
	    clsPtr->filters.num = 0;
	}
	FOREACH(mixinPtr, clsPtr->mixins) {
	    if (!(mixinPtr->thisPtr->flags & OBJECT_DELETED)) {
		TclOORemoveFromMixinSubs(clsPtr, mixinPtr);
	    }
	}
	if (i) {
	    ckfree((char *) clsPtr->mixins.list);
	    clsPtr->mixins.num = 0;
	}
	FOREACH(superPtr, clsPtr->superclasses) {
	    if (!(superPtr->thisPtr->flags & OBJECT_DELETED)) {
		TclOORemoveFromSubclasses(clsPtr, superPtr);
	    }
	}
	if (i) {
	    ckfree((char *) clsPtr->superclasses.list);
	    clsPtr->superclasses.num = 0;
	}
	if (clsPtr->subclasses.list) {
	    ckfree((char *) clsPtr->subclasses.list);
	    clsPtr->subclasses.num = 0;
	}
	if (clsPtr->instances.list) {
	    ckfree((char *) clsPtr->instances.list);
	    clsPtr->instances.num = 0;
	}
	if (clsPtr->mixinSubs.list) {
	    ckfree((char *) clsPtr->mixinSubs.list);
	    clsPtr->mixinSubs.num = 0;
	}

	FOREACH_HASH_VALUE(mPtr, &clsPtr->classMethods) {
	    TclOODelMethodRef(mPtr);
	}
	Tcl_DeleteHashTable(&clsPtr->classMethods);
	TclOODelMethodRef(clsPtr->constructorPtr);
	TclOODelMethodRef(clsPtr->destructorPtr);

	FOREACH(variableObj, clsPtr->variables) {
	    Tcl_DecrRefCount(variableObj);
	}
	if (i) {
	    ckfree((char *) clsPtr->variables.list);
	}

	DelRef(clsPtr);
    }

    /*
     * Delete the object structure itself.
     */

    DelRef(oPtr);
    if (preserved) {
	if (clsPtr) {
	    DelRef(clsPtr);
	}
	DelRef(oPtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOORemoveFromInstances --
 *
 *	Utility function to remove an object from the list of instances within
 *	a class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOORemoveFromInstances(
    Object *oPtr,		/* The instance to remove. */
    Class *clsPtr)		/* The class (possibly) containing the
				 * reference to the instance. */
{
    int i;
    Object *instPtr;

    FOREACH(instPtr, clsPtr->instances) {
	if (oPtr == instPtr) {
	    goto removeInstance;
	}
    }
    return;

  removeInstance:
    clsPtr->instances.num--;
    if (i < clsPtr->instances.num) {
	clsPtr->instances.list[i] =
		clsPtr->instances.list[clsPtr->instances.num];
    }
    clsPtr->instances.list[clsPtr->instances.num] = NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOAddToInstances --
 *
 *	Utility function to add an object to the list of instances within a
 *	class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOOAddToInstances(
    Object *oPtr,		/* The instance to add. */
    Class *clsPtr)		/* The class to add the instance to. It is
				 * assumed that the class is not already
				 * present as an instance in the class. */
{
    if (clsPtr->instances.num >= clsPtr->instances.size) {
	clsPtr->instances.size += ALLOC_CHUNK;
	if (clsPtr->instances.size == ALLOC_CHUNK) {
	    clsPtr->instances.list = (Object **)
		    ckalloc(sizeof(Object *) * ALLOC_CHUNK);
	} else {
	    clsPtr->instances.list = (Object **)
		    ckrealloc((char *) clsPtr->instances.list,
		    sizeof(Object *) * clsPtr->instances.size);
	}
    }
    clsPtr->instances.list[clsPtr->instances.num++] = oPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOORemoveFromSubclasses --
 *
 *	Utility function to remove a class from the list of subclasses within
 *	another class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOORemoveFromSubclasses(
    Class *subPtr,		/* The subclass to remove. */
    Class *superPtr)		/* The superclass to (possibly) remove the
				 * subclass reference from. */
{
    int i;
    Class *subclsPtr;

    FOREACH(subclsPtr, superPtr->subclasses) {
	if (subPtr == subclsPtr) {
	    goto removeSubclass;
	}
    }
    return;

  removeSubclass:
    superPtr->subclasses.num--;
    if (i < superPtr->subclasses.num) {
	superPtr->subclasses.list[i] =
		superPtr->subclasses.list[superPtr->subclasses.num];
    }
    superPtr->subclasses.list[superPtr->subclasses.num] = NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOAddToSubclasses --
 *
 *	Utility function to add a class to the list of subclasses within
 *	another class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOOAddToSubclasses(
    Class *subPtr,		/* The subclass to add. */
    Class *superPtr)		/* The superclass to add the subclass to. It
				 * is assumed that the class is not already
				 * present as a subclass in the superclass. */
{
    if (superPtr->subclasses.num >= superPtr->subclasses.size) {
	superPtr->subclasses.size += ALLOC_CHUNK;
	if (superPtr->subclasses.size == ALLOC_CHUNK) {
	    superPtr->subclasses.list = (Class **)
		    ckalloc(sizeof(Class *) * ALLOC_CHUNK);
	} else {
	    superPtr->subclasses.list = (Class **)
		    ckrealloc((char *) superPtr->subclasses.list,
		    sizeof(Class *) * superPtr->subclasses.size);
	}
    }
    superPtr->subclasses.list[superPtr->subclasses.num++] = subPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOORemoveFromMixinSubs --
 *
 *	Utility function to remove a class from the list of mixinSubs within
 *	another class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOORemoveFromMixinSubs(
    Class *subPtr,		/* The subclass to remove. */
    Class *superPtr)		/* The superclass to (possibly) remove the
				 * subclass reference from. */
{
    int i;
    Class *subclsPtr;

    FOREACH(subclsPtr, superPtr->mixinSubs) {
	if (subPtr == subclsPtr) {
	    goto removeSubclass;
	}
    }
    return;

  removeSubclass:
    superPtr->mixinSubs.num--;
    if (i < superPtr->mixinSubs.num) {
	superPtr->mixinSubs.list[i] =
		superPtr->mixinSubs.list[superPtr->mixinSubs.num];
    }
    superPtr->mixinSubs.list[superPtr->mixinSubs.num] = NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOAddToMixinSubs --
 *
 *	Utility function to add a class to the list of mixinSubs within
 *	another class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOOAddToMixinSubs(
    Class *subPtr,		/* The subclass to add. */
    Class *superPtr)		/* The superclass to add the subclass to. It
				 * is assumed that the class is not already
				 * present as a subclass in the superclass. */
{
    if (superPtr->mixinSubs.num >= superPtr->mixinSubs.size) {
	superPtr->mixinSubs.size += ALLOC_CHUNK;
	if (superPtr->mixinSubs.size == ALLOC_CHUNK) {
	    superPtr->mixinSubs.list = (Class **)
		    ckalloc(sizeof(Class *) * ALLOC_CHUNK);
	} else {
	    superPtr->mixinSubs.list = (Class **)
		    ckrealloc((char *) superPtr->mixinSubs.list,
		    sizeof(Class *) * superPtr->mixinSubs.size);
	}
    }
    superPtr->mixinSubs.list[superPtr->mixinSubs.num++] = subPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * AllocClass --
 *
 *	Allocate a basic class. Does not splice the class object into its
 *	class's instance list.
 *
 * ----------------------------------------------------------------------
 */

static Class *
AllocClass(
    Tcl_Interp *interp,		/* Interpreter within which to allocate the
				 * class. */
    Object *useThisObj)		/* Object that is to act as the class
				 * representation, or NULL if a new object
				 * (with automatic name) is to be used. */
{
    Foundation *fPtr = GetFoundation(interp);
    Class *clsPtr = (Class *) ckalloc(sizeof(Class));

    /*
     * Make an object if we haven't been given one.
     */

    memset(clsPtr, 0, sizeof(Class));
    if (useThisObj == NULL) {
	clsPtr->thisPtr = AllocObject(interp, NULL, NULL);
    } else {
	clsPtr->thisPtr = useThisObj;
    }

    /*
     * Configure the namespace path for the class's object.
     */

    if (fPtr->helpersNs != NULL) {
	Tcl_Namespace *path[2];

	path[0] = fPtr->helpersNs;
	path[1] = fPtr->ooNs;
	TclSetNsPath((Namespace *) clsPtr->thisPtr->namespacePtr, 2, path);
    } else {
	TclSetNsPath((Namespace *) clsPtr->thisPtr->namespacePtr, 1,
		&fPtr->ooNs);
    }

    /*
     * Class objects inherit from the class of classes unless they inherit
     * from some subclass of it. Enforce this right now.
     */

    clsPtr->thisPtr->selfCls = fPtr->classCls;

    /*
     * Classes are subclasses of oo::object, i.e. the objects they create are
     * objects.
     */

    clsPtr->superclasses.num = 1;
    clsPtr->superclasses.list = (Class **) ckalloc(sizeof(Class *));
    clsPtr->superclasses.list[0] = fPtr->objectCls;

    /*
     * Finish connecting the class structure to the object structure.
     */

    clsPtr->thisPtr->classPtr = clsPtr;

    /*
     * That's the complicated bit. Now fill in the rest of the non-zero/NULL
     * fields.
     */

    clsPtr->refCount = 1;
    Tcl_InitObjHashTable(&clsPtr->classMethods);
    return clsPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * Tcl_NewObjectInstance --
 *
 *	Allocate a new instance of an object.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Object
Tcl_NewObjectInstance(
    Tcl_Interp *interp,		/* Interpreter context. */
    Tcl_Class cls,		/* Class to create an instance of. */
    const char *nameStr,	/* Name of object to create, or NULL to ask
				 * the code to pick its own unique name. */
    const char *nsNameStr,	/* Name of namespace to create inside object,
				 * or NULL to ask the code to pick its own
				 * unique name. */
    int objc,			/* Number of arguments. Negative value means
				 * do not call constructor. */
    Tcl_Obj *const *objv,	/* Argument list. */
    int skip)			/* Number of arguments to _not_ pass to the
				 * constructor. */
{
    register Class *classPtr = (Class *) cls;
    Foundation *fPtr = GetFoundation(interp);
    Object *oPtr;

    /*
     * Check if we're going to create an object over an existing command;
     * that's not allowed.
     */

    if (nameStr && Tcl_FindCommand(interp, nameStr, NULL,
	    TCL_NAMESPACE_ONLY)) {
	Tcl_AppendResult(interp, "can't create object \"", nameStr,
		"\": command already exists with that name", NULL);
	return NULL;
    }

    /*
     * Create the object.
     */

    oPtr = AllocObject(interp, nameStr, nsNameStr);
    oPtr->selfCls = classPtr;
    TclOOAddToInstances(oPtr, classPtr);

    /*
     * Check to see if we're really creating a class. If so, allocate the
     * class structure as well.
     */

    if (TclOOIsReachable(fPtr->classCls, classPtr)) {
	/*
	 * Is a class, so attach a class structure. Note that the AllocClass
	 * function splices the structure into the object, so we don't have
	 * to. Once that's done, we need to repatch the object to have the
	 * right class since AllocClass interferes with that.
	 */

	AllocClass(interp, oPtr);
	oPtr->selfCls = classPtr;
	TclOOAddToSubclasses(oPtr->classPtr, fPtr->objectCls);
    }

    /*
     * Run constructors, except when objc < 0 (a special flag case used for
     * object cloning only).
     */

    if (objc >= 0) {
	CallContext *contextPtr =
		TclOOGetCallContext(oPtr, NULL, CONSTRUCTOR, NULL);

	if (contextPtr != NULL) {
	    int result, flags;
	    Tcl_InterpState state;

	    AddRef(oPtr);
	    state = Tcl_SaveInterpState(interp, TCL_OK);
	    contextPtr->callPtr->flags |= CONSTRUCTOR;
	    contextPtr->skip = skip;
	    result = Tcl_NRCallObjProc(interp, TclOOInvokeContext, contextPtr,
		    objc, objv);
	    flags = oPtr->flags;

	    /*
	     * It's an error if the object was whacked in the constructor.
	     * Force this if it isn't already an error (don't want to lose
	     * errors by accident...) [Bug 2903011]
	     */

	    if (result != TCL_ERROR && (flags & OBJECT_DELETED)) {
		Tcl_SetResult(interp, "object deleted in constructor",
			TCL_STATIC);
		result = TCL_ERROR;
	    }
	    TclOODeleteContext(contextPtr);
	    DelRef(oPtr);
	    if (result != TCL_OK) {
		Tcl_DiscardInterpState(state);

		/*
		 * Take care to not delete a deleted object; that would be
		 * bad. [Bug 2903011]
		 */

		if (!(flags & OBJECT_DELETED)) {
		    Tcl_DeleteCommandFromToken(interp, oPtr->command);
		}
		return NULL;
	    }
	    Tcl_RestoreInterpState(interp, state);
	}
    }

    return (Tcl_Object) oPtr;
}

int
TclNRNewObjectInstance(
    Tcl_Interp *interp,		/* Interpreter context. */
    Tcl_Class cls,		/* Class to create an instance of. */
    const char *nameStr,	/* Name of object to create, or NULL to ask
				 * the code to pick its own unique name. */
    const char *nsNameStr,	/* Name of namespace to create inside object,
				 * or NULL to ask the code to pick its own
				 * unique name. */
    int objc,			/* Number of arguments. Negative value means
				 * do not call constructor. */
    Tcl_Obj *const *objv,	/* Argument list. */
    int skip,			/* Number of arguments to _not_ pass to the
				 * constructor. */
    Tcl_Object *objectPtr)	/* Place to write the object reference upon
				 * successful allocation. */
{
    register Class *classPtr = (Class *) cls;
    Foundation *fPtr = GetFoundation(interp);
    CallContext *contextPtr;
    Tcl_InterpState state;
    Object *oPtr;

    /*
     * Check if we're going to create an object over an existing command;
     * that's not allowed.
     */

    if (nameStr && Tcl_FindCommand(interp, nameStr, NULL,
	    TCL_NAMESPACE_ONLY)) {
	Tcl_AppendResult(interp, "can't create object \"", nameStr,
		"\": command already exists with that name", NULL);
	return TCL_ERROR;
    }

    /*
     * Create the object.
     */

    oPtr = AllocObject(interp, nameStr, nsNameStr);
    oPtr->selfCls = classPtr;
    TclOOAddToInstances(oPtr, classPtr);

    /*
     * Check to see if we're really creating a class. If so, allocate the
     * class structure as well.
     */

    if (TclOOIsReachable(fPtr->classCls, classPtr)) {
	/*
	 * Is a class, so attach a class structure. Note that the AllocClass
	 * function splices the structure into the object, so we don't have
	 * to. Once that's done, we need to repatch the object to have the
	 * right class since AllocClass interferes with that.
	 */

	AllocClass(interp, oPtr);
	oPtr->selfCls = classPtr;
	TclOOAddToSubclasses(oPtr->classPtr, fPtr->objectCls);
    }

    /*
     * Run constructors, except when objc < 0 (a special flag case used for
     * object cloning only). If there aren't any constructors, we do nothing.
     */

    if (objc < 0) {
	*objectPtr = (Tcl_Object) oPtr;
	return TCL_OK;
    }
    contextPtr = TclOOGetCallContext(oPtr, NULL, CONSTRUCTOR, NULL);
    if (contextPtr == NULL) {
	*objectPtr = (Tcl_Object) oPtr;
	return TCL_OK;
    }

    AddRef(oPtr);
    state = Tcl_SaveInterpState(interp, TCL_OK);
    contextPtr->callPtr->flags |= CONSTRUCTOR;
    contextPtr->skip = skip;

    /*
     * Fire off the constructors non-recursively.
     */

    TclNRAddCallback(interp, FinalizeAlloc, contextPtr, oPtr, state,
	    objectPtr);
    TclPushTailcallPoint(interp);
    return TclOOInvokeContext(contextPtr, interp, objc, objv);
}

static int
FinalizeAlloc(
    ClientData data[],
    Tcl_Interp *interp,
    int result)
{
    CallContext *contextPtr = data[0];
    Object *oPtr = data[1];
    Tcl_InterpState state = data[2];
    Tcl_Object *objectPtr = data[3];
    int flags = oPtr->flags;

    /*
     * It's an error if the object was whacked in the constructor. Force this
     * if it isn't already an error (don't want to lose errors by accident...)
     * [Bug 2903011]
     */

    if (result != TCL_ERROR && (flags & OBJECT_DELETED)) {
	Tcl_SetResult(interp, "object deleted in constructor", TCL_STATIC);
	result = TCL_ERROR;
    }
    TclOODeleteContext(contextPtr);
    DelRef(oPtr);
    if (result != TCL_OK) {
	Tcl_DiscardInterpState(state);

	/*
	 * Take care to not delete a deleted object; that would be bad. [Bug
	 * 2903011]
	 */

	if (!(flags & OBJECT_DELETED)) {
	    Tcl_DeleteCommandFromToken(interp, oPtr->command);
	}
	return TCL_ERROR;
    }
    Tcl_RestoreInterpState(interp, state);
    *objectPtr = (Tcl_Object) oPtr;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * Tcl_CopyObjectInstance --
 *
 *	Creates a copy of an object. Does not copy the backing namespace,
 *	since the correct way to do that (e.g., shallow/deep) depends on the
 *	object/class's own policies.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Object
Tcl_CopyObjectInstance(
    Tcl_Interp *interp,
    Tcl_Object sourceObject,
    const char *targetName,
    const char *targetNamespaceName)
{
    Object *oPtr = (Object *) sourceObject, *o2Ptr;
    FOREACH_HASH_DECLS;
    Method *mPtr;
    Class *mixinPtr;
    Tcl_Obj *keyPtr, *filterObj;
    int i;

    /*
     * Sanity checks.
     */

    if (targetName == NULL && oPtr->classPtr != NULL) {
	Tcl_AppendResult(interp, "must supply a name when copying a class",
		NULL);
	return NULL;
    }
    if (oPtr->classPtr == GetFoundation(interp)->classCls) {
	Tcl_AppendResult(interp, "may not clone the class of classes", NULL);
	return NULL;
    }

    /*
     * Build the instance. Note that this does not run any constructors.
     */

    o2Ptr = (Object *) Tcl_NewObjectInstance(interp,
	    (Tcl_Class) oPtr->selfCls, targetName, targetNamespaceName, -1,
	    NULL, -1);
    if (o2Ptr == NULL) {
	return NULL;
    }

    /*
     * Copy the object-local methods to the new object.
     */

    if (oPtr->methodsPtr) {
	FOREACH_HASH(keyPtr, mPtr, oPtr->methodsPtr) {
	    if (CloneObjectMethod(interp, o2Ptr, mPtr, keyPtr) != TCL_OK) {
		Tcl_DeleteCommandFromToken(interp, o2Ptr->command);
		return NULL;
	    }
	}
    }

    /*
     * Copy the object's mixin references to the new object.
     */

    FOREACH(mixinPtr, o2Ptr->mixins) {
	if (mixinPtr != o2Ptr->selfCls) {
	    TclOORemoveFromInstances(o2Ptr, mixinPtr);
	}
    }
    DUPLICATE(o2Ptr->mixins, oPtr->mixins, Class *);
    FOREACH(mixinPtr, o2Ptr->mixins) {
	if (mixinPtr != o2Ptr->selfCls) {
	    TclOOAddToInstances(o2Ptr, mixinPtr);
	}
    }

    /*
     * Copy the object's filter list to the new object.
     */

    DUPLICATE(o2Ptr->filters, oPtr->filters, Tcl_Obj *);
    FOREACH(filterObj, o2Ptr->filters) {
	Tcl_IncrRefCount(filterObj);
    }

    /*
     * Copy the object's flags to the new object, clearing those that must be
     * kept object-local. The duplicate is never deleted at this point, nor is
     * it the root of the object system or in the midst of processing a filter
     * call.
     */

    o2Ptr->flags = oPtr->flags & ~(
	    OBJECT_DELETED | ROOT_OBJECT | FILTER_HANDLING);

    /*
     * Copy the object's metadata.
     */

    if (oPtr->metadataPtr != NULL) {
	Tcl_ObjectMetadataType *metadataTypePtr;
	ClientData value, duplicate;

	FOREACH_HASH(metadataTypePtr, value, oPtr->metadataPtr) {
	    if (metadataTypePtr->cloneProc == NULL) {
		duplicate = value;
	    } else {
		if (metadataTypePtr->cloneProc(interp, value,
			&duplicate) != TCL_OK) {
		    Tcl_DeleteCommandFromToken(interp, o2Ptr->command);
		    return NULL;
		}
	    }
	    if (duplicate != NULL) {
		Tcl_ObjectSetMetadata((Tcl_Object) o2Ptr, metadataTypePtr,
			duplicate);
	    }
	}
    }

    /*
     * Copy the class, if present. Note that if there is a class present in
     * the source object, there must also be one in the copy.
     */

    if (oPtr->classPtr != NULL) {
	Class *clsPtr = oPtr->classPtr;
	Class *cls2Ptr = o2Ptr->classPtr;
	Class *superPtr;

	/*
	 * Copy the class flags across.
	 */

	cls2Ptr->flags = clsPtr->flags;

	/*
	 * Ensure that the new class's superclass structure is the same as the
	 * old class's.
	 */

	FOREACH(superPtr, cls2Ptr->superclasses) {
	    TclOORemoveFromSubclasses(cls2Ptr, superPtr);
	}
	if (cls2Ptr->superclasses.num) {
	    cls2Ptr->superclasses.list = (Class **)
		    ckrealloc((char *) cls2Ptr->superclasses.list,
		    sizeof(Class *) * clsPtr->superclasses.num);
	} else {
	    cls2Ptr->superclasses.list = (Class **)
		    ckalloc(sizeof(Class *) * clsPtr->superclasses.num);
	}
	memcpy(cls2Ptr->superclasses.list, clsPtr->superclasses.list,
		sizeof(Class *) * clsPtr->superclasses.num);
	cls2Ptr->superclasses.num = clsPtr->superclasses.num;
	FOREACH(superPtr, cls2Ptr->superclasses) {
	    TclOOAddToSubclasses(cls2Ptr, superPtr);
	}

	/*
	 * Duplicate the source class's filters.
	 */

	DUPLICATE(cls2Ptr->filters, clsPtr->filters, Tcl_Obj *);
	FOREACH(filterObj, cls2Ptr->filters) {
	    Tcl_IncrRefCount(filterObj);
	}

	/*
	 * Duplicate the source class's mixins (which cannot be circular
	 * references to the duplicate).
	 */

	FOREACH(mixinPtr, cls2Ptr->mixins) {
	    TclOORemoveFromMixinSubs(cls2Ptr, mixinPtr);
	}
	if (cls2Ptr->mixins.num != 0) {
	    ckfree((char *) clsPtr->mixins.list);
	}
	DUPLICATE(cls2Ptr->mixins, clsPtr->mixins, Class *);
	FOREACH(mixinPtr, cls2Ptr->mixins) {
	    TclOOAddToMixinSubs(cls2Ptr, mixinPtr);
	}

	/*
	 * Duplicate the source class's methods, constructor and destructor.
	 */

	FOREACH_HASH(keyPtr, mPtr, &clsPtr->classMethods) {
	    if (CloneClassMethod(interp, cls2Ptr, mPtr, keyPtr,
		    NULL) != TCL_OK) {
		Tcl_DeleteCommandFromToken(interp, o2Ptr->command);
		return NULL;
	    }
	}
	if (clsPtr->constructorPtr) {
	    if (CloneClassMethod(interp, cls2Ptr, clsPtr->constructorPtr,
		    NULL, &cls2Ptr->constructorPtr) != TCL_OK) {
		Tcl_DeleteCommandFromToken(interp, o2Ptr->command);
		return NULL;
	    }
	}
	if (clsPtr->destructorPtr) {
	    if (CloneClassMethod(interp, cls2Ptr, clsPtr->destructorPtr, NULL,
		    &cls2Ptr->destructorPtr) != TCL_OK) {
		Tcl_DeleteCommandFromToken(interp, o2Ptr->command);
		return NULL;
	    }
	}

	/*
	 * Duplicate the class's metadata.
	 */

	if (clsPtr->metadataPtr != NULL) {
	    Tcl_ObjectMetadataType *metadataTypePtr;
	    ClientData value, duplicate;

	    FOREACH_HASH(metadataTypePtr, value, clsPtr->metadataPtr) {
		if (metadataTypePtr->cloneProc == NULL) {
		    duplicate = value;
		} else {
		    if (metadataTypePtr->cloneProc(interp, value,
			    &duplicate) != TCL_OK) {
			Tcl_DeleteCommandFromToken(interp, o2Ptr->command);
			return NULL;
		    }
		}
		if (duplicate != NULL) {
		    Tcl_ClassSetMetadata((Tcl_Class) cls2Ptr, metadataTypePtr,
			    duplicate);
		}
	    }
	}
    }

    return (Tcl_Object) o2Ptr;
}

/*
 * ----------------------------------------------------------------------
 *
 * CloneObjectMethod, CloneClassMethod --
 *
 *	Helper functions used for cloning methods. They work identically to
 *	each other, except for the difference between them in how they
 *	register the cloned method on a successful clone.
 *
 * ----------------------------------------------------------------------
 */

static int
CloneObjectMethod(
    Tcl_Interp *interp,
    Object *oPtr,
    Method *mPtr,
    Tcl_Obj *namePtr)
{
    if (mPtr->typePtr == NULL) {
	Tcl_NewInstanceMethod(interp, (Tcl_Object) oPtr, namePtr,
		mPtr->flags & PUBLIC_METHOD, NULL, NULL);
    } else if (mPtr->typePtr->cloneProc) {
	ClientData newClientData;

	if (mPtr->typePtr->cloneProc(interp, mPtr->clientData,
		&newClientData) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_NewInstanceMethod(interp, (Tcl_Object) oPtr, namePtr,
		mPtr->flags & PUBLIC_METHOD, mPtr->typePtr, newClientData);
    } else {
	Tcl_NewInstanceMethod(interp, (Tcl_Object) oPtr, namePtr,
		mPtr->flags & PUBLIC_METHOD, mPtr->typePtr, mPtr->clientData);
    }
    return TCL_OK;
}

static int
CloneClassMethod(
    Tcl_Interp *interp,
    Class *clsPtr,
    Method *mPtr,
    Tcl_Obj *namePtr,
    Method **m2PtrPtr)
{
    Method *m2Ptr;

    if (mPtr->typePtr == NULL) {
	m2Ptr = (Method *) Tcl_NewMethod(interp, (Tcl_Class) clsPtr,
		namePtr, mPtr->flags & PUBLIC_METHOD, NULL, NULL);
    } else if (mPtr->typePtr->cloneProc) {
	ClientData newClientData;

	if (mPtr->typePtr->cloneProc(interp, mPtr->clientData,
		&newClientData) != TCL_OK) {
	    return TCL_ERROR;
	}
	m2Ptr = (Method *) Tcl_NewMethod(interp, (Tcl_Class) clsPtr,
		namePtr, mPtr->flags & PUBLIC_METHOD, mPtr->typePtr,
		newClientData);
    } else {
	m2Ptr = (Method *) Tcl_NewMethod(interp, (Tcl_Class) clsPtr,
		namePtr, mPtr->flags & PUBLIC_METHOD, mPtr->typePtr,
		mPtr->clientData);
    }
    if (m2PtrPtr != NULL) {
	*m2PtrPtr = m2Ptr;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * Tcl_ClassGetMetadata, Tcl_ClassSetMetadata, Tcl_ObjectGetMetadata,
 * Tcl_ObjectSetMetadata --
 *
 *	Metadata management API. The metadata system allows code in extensions
 *	to attach arbitrary non-NULL pointers to objects and classes without
 *	the different things that might be interested being able to interfere
 *	with each other. Apart from non-NULL-ness, these routines attach no
 *	interpretation to the meaning of the metadata pointers.
 *
 *	The Tcl_*GetMetadata routines get the metadata pointer attached that
 *	has been related with a particular type, or NULL if no metadata
 *	associated with the given type has been attached.
 *
 *	The Tcl_*SetMetadata routines set or delete the metadata pointer that
 *	is related to a particular type. The value associated with the type is
 *	deleted (if present; no-op otherwise) if the value is NULL, and
 *	attached (replacing the previous value, which is deleted if present)
 *	otherwise. This means it is impossible to attach a NULL value for any
 *	metadata type.
 *
 * ----------------------------------------------------------------------
 */

ClientData
Tcl_ClassGetMetadata(
    Tcl_Class clazz,
    const Tcl_ObjectMetadataType *typePtr)
{
    Class *clsPtr = (Class *) clazz;
    Tcl_HashEntry *hPtr;

    /*
     * If there's no metadata store attached, the type in question has
     * definitely not been attached either!
     */

    if (clsPtr->metadataPtr == NULL) {
	return NULL;
    }

    /*
     * There is a metadata store, so look in it for the given type.
     */

    hPtr = Tcl_FindHashEntry(clsPtr->metadataPtr, (char *) typePtr);

    /*
     * Return the metadata value if we found it, otherwise NULL.
     */

    if (hPtr == NULL) {
	return NULL;
    }
    return Tcl_GetHashValue(hPtr);
}

void
Tcl_ClassSetMetadata(
    Tcl_Class clazz,
    const Tcl_ObjectMetadataType *typePtr,
    ClientData metadata)
{
    Class *clsPtr = (Class *) clazz;
    Tcl_HashEntry *hPtr;
    int isNew;

    /*
     * Attach the metadata store if not done already.
     */

    if (clsPtr->metadataPtr == NULL) {
	if (metadata == NULL) {
	    return;
	}
	clsPtr->metadataPtr = (Tcl_HashTable*) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(clsPtr->metadataPtr, TCL_ONE_WORD_KEYS);
    }

    /*
     * If the metadata is NULL, we're deleting the metadata for the type.
     */

    if (metadata == NULL) {
	hPtr = Tcl_FindHashEntry(clsPtr->metadataPtr, (char *) typePtr);
	if (hPtr != NULL) {
	    typePtr->deleteProc(Tcl_GetHashValue(hPtr));
	    Tcl_DeleteHashEntry(hPtr);
	}
	return;
    }

    /*
     * Otherwise we're attaching the metadata. Note that if there was already
     * some metadata attached of this type, we delete that first.
     */

    hPtr = Tcl_CreateHashEntry(clsPtr->metadataPtr, (char *) typePtr, &isNew);
    if (!isNew) {
	typePtr->deleteProc(Tcl_GetHashValue(hPtr));
    }
    Tcl_SetHashValue(hPtr, metadata);
}

ClientData
Tcl_ObjectGetMetadata(
    Tcl_Object object,
    const Tcl_ObjectMetadataType *typePtr)
{
    Object *oPtr = (Object *) object;
    Tcl_HashEntry *hPtr;

    /*
     * If there's no metadata store attached, the type in question has
     * definitely not been attached either!
     */

    if (oPtr->metadataPtr == NULL) {
	return NULL;
    }

    /*
     * There is a metadata store, so look in it for the given type.
     */

    hPtr = Tcl_FindHashEntry(oPtr->metadataPtr, (char *) typePtr);

    /*
     * Return the metadata value if we found it, otherwise NULL.
     */

    if (hPtr == NULL) {
	return NULL;
    }
    return Tcl_GetHashValue(hPtr);
}

void
Tcl_ObjectSetMetadata(
    Tcl_Object object,
    const Tcl_ObjectMetadataType *typePtr,
    ClientData metadata)
{
    Object *oPtr = (Object *) object;
    Tcl_HashEntry *hPtr;
    int isNew;

    /*
     * Attach the metadata store if not done already.
     */

    if (oPtr->metadataPtr == NULL) {
	if (metadata == NULL) {
	    return;
	}
	oPtr->metadataPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(oPtr->metadataPtr, TCL_ONE_WORD_KEYS);
    }

    /*
     * If the metadata is NULL, we're deleting the metadata for the type.
     */

    if (metadata == NULL) {
	hPtr = Tcl_FindHashEntry(oPtr->metadataPtr, (char *) typePtr);
	if (hPtr != NULL) {
	    typePtr->deleteProc(Tcl_GetHashValue(hPtr));
	    Tcl_DeleteHashEntry(hPtr);
	}
	return;
    }

    /*
     * Otherwise we're attaching the metadata. Note that if there was already
     * some metadata attached of this type, we delete that first.
     */

    hPtr = Tcl_CreateHashEntry(oPtr->metadataPtr, (char *) typePtr, &isNew);
    if (!isNew) {
	typePtr->deleteProc(Tcl_GetHashValue(hPtr));
    }
    Tcl_SetHashValue(hPtr, metadata);
}

/*
 * ----------------------------------------------------------------------
 *
 * PublicObjectCmd, PrivateObjectCmd, TclOOInvokeObject --
 *
 *	Main entry point for object invokations. The Public* and Private*
 *	wrapper functions (implementations of both object instance commands
 *	and [my]) are just thin wrappers round the main TclOOObjectCmdCore
 *	function. Note that the core is function is NRE-aware.
 *
 * ----------------------------------------------------------------------
 */

static int
PublicObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    return Tcl_NRCallObjProc(interp, PublicNRObjectCmd, clientData,objc,objv);
}

static int
PublicNRObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    return TclOOObjectCmdCore(clientData, interp, objc, objv, PUBLIC_METHOD,
	    NULL);
}

static int
PrivateObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    return Tcl_NRCallObjProc(interp, PrivateNRObjectCmd,clientData,objc,objv);
}

static int
PrivateNRObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    return TclOOObjectCmdCore(clientData, interp, objc, objv, 0, NULL);
}

int
TclOOInvokeObject(
    Tcl_Interp *interp,		/* Interpreter for commands, variables,
				 * results, error reporting, etc. */
    Tcl_Object object,		/* The object to invoke. */
    Tcl_Class startCls,		/* Where in the class chain to start the
				 * invoke from, or NULL to traverse the whole
				 * chain including filters. */
    int publicPrivate,		/* Whether this is an invoke from a public
				 * context (PUBLIC_METHOD), a private context
				 * (PRIVATE_METHOD), or a *really* private
				 * context (any other value; conventionally
				 * 0). */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Array of argument objects. It is assumed
				 * that the name of the method to invoke will
				 * be at index 1. */
{
    switch (publicPrivate) {
    case PUBLIC_METHOD:
	return TclOOObjectCmdCore((Object *) object, interp, objc, objv,
		PUBLIC_METHOD, (Class *) startCls);
    case PRIVATE_METHOD:
	return TclOOObjectCmdCore((Object *) object, interp, objc, objv,
		PRIVATE_METHOD, (Class *) startCls);
    default:
	return TclOOObjectCmdCore((Object *) object, interp, objc, objv, 0,
		(Class *) startCls);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOObjectCmdCore, FinalizeObjectCall --
 *
 *	Main function for object invokations. Does call chain creation,
 *	management and invokation. The function FinalizeObjectCall exists to
 *	clean up after the non-recursive processing of TclOOObjectCmdCore.
 *
 * ----------------------------------------------------------------------
 */

int
TclOOObjectCmdCore(
    Object *oPtr,		/* The object being invoked. */
    Tcl_Interp *interp,		/* The interpreter containing the object. */
    int objc,			/* How many arguments are being passed in. */
    Tcl_Obj *const *objv,	/* The array of arguments. */
    int flags,			/* Whether this is an invokation through the
				 * public or the private command interface. */
    Class *startCls)		/* Where to start in the call chain, or NULL
				 * if we are to start at the front with
				 * filters and the object's methods (which is
				 * the normal case). */
{
    CallContext *contextPtr;
    Tcl_Obj *methodNamePtr;
    int result;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "method ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * Give plugged in code a chance to remap the method name.
     */

    methodNamePtr = objv[1];
    if (oPtr->mapMethodNameProc != NULL) {
	register Class **startClsPtr = &startCls;
	Tcl_Obj *mappedMethodName = Tcl_DuplicateObj(methodNamePtr);

	result = oPtr->mapMethodNameProc(interp, (Tcl_Object) oPtr,
		(Tcl_Class *) startClsPtr, mappedMethodName);
	if (result != TCL_OK) {
	    Tcl_DecrRefCount(mappedMethodName);
	    if (result == TCL_BREAK) {
		goto noMapping;
	    } else if (result == TCL_ERROR) {
		Tcl_AddErrorInfo(interp, "\n    (while mapping method name)");
	    }
	    return result;
	}

	/*
	 * Get the call chain for the remapped name.
	 */

	Tcl_IncrRefCount(mappedMethodName);
	contextPtr = TclOOGetCallContext(oPtr, mappedMethodName,
		flags | (oPtr->flags & FILTER_HANDLING), methodNamePtr);
	Tcl_DecrRefCount(mappedMethodName);
	if (contextPtr == NULL) {
	    Tcl_AppendResult(interp, "impossible to invoke method \"",
		    TclGetString(methodNamePtr),
		    "\": no defined method or unknown method", NULL);
	    return TCL_ERROR;
	}
    } else {
	/*
	 * Get the call chain.
	 */

    noMapping:
	contextPtr = TclOOGetCallContext(oPtr, methodNamePtr,
		flags | (oPtr->flags & FILTER_HANDLING), NULL);
	if (contextPtr == NULL) {
	    Tcl_AppendResult(interp, "impossible to invoke method \"",
		    TclGetString(methodNamePtr),
		    "\": no defined method or unknown method", NULL);
	    return TCL_ERROR;
	}
    }

    /*
     * Check to see if we need to apply magical tricks to start part way
     * through the call chain.
     */

    if (startCls != NULL) {
	for (; contextPtr->index < contextPtr->callPtr->numChain;
		contextPtr->index++) {
	    register struct MInvoke *miPtr =
		    &contextPtr->callPtr->chain[contextPtr->index];

	    if (miPtr->isFilter) {
		continue;
	    }
	    if (miPtr->mPtr->declaringClassPtr == startCls) {
		break;
	    }
	}
	if (contextPtr->index >= contextPtr->callPtr->numChain) {
	    result = TCL_ERROR;
	    Tcl_SetResult(interp, "no valid method implementation",
		    TCL_STATIC);
	    TclOODeleteContext(contextPtr);
	    return TCL_ERROR;
	}
    }

    /*
     * Invoke the call chain, locking the object structure against deletion
     * for the duration.
     */

    AddRef(oPtr);
    TclNRAddCallback(interp, FinalizeObjectCall, contextPtr,oPtr, NULL,NULL);
    return TclOOInvokeContext(contextPtr, interp, objc, objv);
}

static int
FinalizeObjectCall(
    ClientData data[],
    Tcl_Interp *interp,
    int result)
{
    register CallContext *contextPtr = data[0];
    register Object *oPtr = data[1];

    /*
     * Dispose of the call chain and drop the lock on the object's structure.
     */

    TclOODeleteContext(contextPtr);
    DelRef(oPtr);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * Tcl_ObjectContextInvokeNext, TclNRObjectContextInvokeNext, FinalizeNext --
 *
 *	Invokes the next stage of the call chain described in an object
 *	context. This is the core of the implementation of the [next] command.
 *	Does not do management of the call-frame stack. Available in public
 *	(standard API) and private (NRE-aware) forms. FinalizeNext is a
 *	private function used to clean up in the NRE case.
 *
 * ----------------------------------------------------------------------
 */

int
Tcl_ObjectContextInvokeNext(
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    int objc,
    Tcl_Obj *const *objv,
    int skip)
{
    CallContext *contextPtr = (CallContext *) context;
    int savedIndex = contextPtr->index;
    int savedSkip = contextPtr->skip;
    int result;

    if (contextPtr->index+1 >= contextPtr->callPtr->numChain) {
	/*
	 * We're at the end of the chain; generate an error message unless the
	 * interpreter is being torn down, in which case we might be getting
	 * here because of methods/destructors doing a [next] (or equivalent)
	 * unexpectedly.
	 */

	const char *methodType;

	if (Tcl_InterpDeleted(interp)) {
	    return TCL_OK;
	}

	if (contextPtr->callPtr->flags & CONSTRUCTOR) {
	    methodType = "constructor";
	} else if (contextPtr->callPtr->flags & DESTRUCTOR) {
	    methodType = "destructor";
	} else {
	    methodType = "method";
	}

	Tcl_AppendResult(interp, "no next ", methodType, " implementation",
		NULL);
	return TCL_ERROR;
    }

    /*
     * Advance to the next method implementation in the chain in the method
     * call context while we process the body. However, need to adjust the
     * argument-skip control because we're guaranteed to have a single prefix
     * arg (i.e., 'next') and not the variable amount that can happen because
     * method invokations (i.e., '$obj meth' and 'my meth'), constructors
     * (i.e., '$cls new' and '$cls create obj') and destructors (no args at
     * all) come through the same code.
     */

    contextPtr->index++;
    contextPtr->skip = skip;

    /*
     * Invoke the (advanced) method call context in the caller context.
     */

    result = Tcl_NRCallObjProc(interp, TclOOInvokeContext, contextPtr, objc,
	    objv);

    /*
     * Restore the call chain context index as we've finished the inner invoke
     * and want to operate in the outer context again.
     */

    contextPtr->index = savedIndex;
    contextPtr->skip = savedSkip;

    return result;
}

int
TclNRObjectContextInvokeNext(
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    int objc,
    Tcl_Obj *const *objv,
    int skip)
{
    register CallContext *contextPtr = (CallContext *) context;

    if (contextPtr->index+1 >= contextPtr->callPtr->numChain) {
	/*
	 * We're at the end of the chain; generate an error message unless the
	 * interpreter is being torn down, in which case we might be getting
	 * here because of methods/destructors doing a [next] (or equivalent)
	 * unexpectedly.
	 */

	const char *methodType;

	if (Tcl_InterpDeleted(interp)) {
	    return TCL_OK;
	}

	if (contextPtr->callPtr->flags & CONSTRUCTOR) {
	    methodType = "constructor";
	} else if (contextPtr->callPtr->flags & DESTRUCTOR) {
	    methodType = "destructor";
	} else {
	    methodType = "method";
	}

	Tcl_AppendResult(interp, "no next ", methodType, " implementation",
		NULL);
	return TCL_ERROR;
    }

    /*
     * Advance to the next method implementation in the chain in the method
     * call context while we process the body. However, need to adjust the
     * argument-skip control because we're guaranteed to have a single prefix
     * arg (i.e., 'next') and not the variable amount that can happen because
     * method invokations (i.e., '$obj meth' and 'my meth'), constructors
     * (i.e., '$cls new' and '$cls create obj') and destructors (no args at
     * all) come through the same code.
     */

    TclNRAddCallback(interp, FinalizeNext, contextPtr,
	    INT2PTR(contextPtr->index), INT2PTR(contextPtr->skip), NULL);
    contextPtr->index++;
    contextPtr->skip = skip;

    /*
     * Invoke the (advanced) method call context in the caller context.
     */

    return TclOOInvokeContext(contextPtr, interp, objc, objv);
}

static int
FinalizeNext(
    ClientData data[],
    Tcl_Interp *interp,
    int result)
{
    CallContext *contextPtr = data[0];

    /*
     * Restore the call chain context index as we've finished the inner invoke
     * and want to operate in the outer context again.
     */

    contextPtr->index = PTR2INT(data[1]);
    contextPtr->skip = PTR2INT(data[2]);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * Tcl_GetObjectFromObj --
 *
 *	Utility function to get an object from a Tcl_Obj containing its name.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Object
Tcl_GetObjectFromObj(
    Tcl_Interp *interp,		/* Interpreter in which to locate the object.
				 * Will have an error message placed in it if
				 * the name does not refer to an object. */
    Tcl_Obj *objPtr)		/* The name of the object to look up, which is
				 * exactly the name of its public command. */
{
    Command *cmdPtr = (Command *) Tcl_GetCommandFromObj(interp, objPtr);

    if (cmdPtr == NULL) {
	goto notAnObject;
    }
    if (cmdPtr->objProc != PublicObjectCmd) {
	cmdPtr = (Command *) TclGetOriginalCommand((Tcl_Command) cmdPtr);
	if (cmdPtr == NULL || cmdPtr->objProc != PublicObjectCmd) {
	    goto notAnObject;
	}
    }
    return cmdPtr->objClientData;

  notAnObject:
    Tcl_AppendResult(interp, TclGetString(objPtr),
	    " does not refer to an object", NULL);
    return NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOIsReachable --
 *
 *	Utility function that tests whether a class is a subclass (whether
 *	directly or indirectly) of another class.
 *
 * ----------------------------------------------------------------------
 */

int
TclOOIsReachable(
    Class *targetPtr,
    Class *startPtr)
{
    int i;
    Class *superPtr;

  tailRecurse:
    if (startPtr == targetPtr) {
	return 1;
    }
    if (startPtr->superclasses.num == 1 && startPtr->mixins.num == 0) {
	startPtr = startPtr->superclasses.list[0];
	goto tailRecurse;
    }
    FOREACH(superPtr, startPtr->superclasses) {
	if (TclOOIsReachable(targetPtr, superPtr)) {
	    return 1;
	}
    }
    FOREACH(superPtr, startPtr->mixins) {
	if (TclOOIsReachable(targetPtr, superPtr)) {
	    return 1;
	}
    }
    return 0;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOObjectName, Tcl_GetObjectName --
 *
 *	Utility function that returns the name of the object. Note that this
 *	simplifies cache management by keeping the code to do it in one place
 *	and not sprayed all over. The value returned always has a reference
 *	count of at least one.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Obj *
TclOOObjectName(
    Tcl_Interp *interp,
    Object *oPtr)
{
    Tcl_Obj *namePtr;

    if (oPtr->cachedNameObj) {
	return oPtr->cachedNameObj;
    }
    namePtr = Tcl_NewObj();
    Tcl_GetCommandFullName(interp, oPtr->command, namePtr);
    Tcl_IncrRefCount(namePtr);
    oPtr->cachedNameObj = namePtr;
    return namePtr;
}

Tcl_Obj *
Tcl_GetObjectName(
    Tcl_Interp *interp,
    Tcl_Object object)
{
    return TclOOObjectName(interp, (Object *) object);
}

/*
 * ----------------------------------------------------------------------
 *
 * assorted trivial 'getter' functions
 *
 * ----------------------------------------------------------------------
 */

Tcl_Method
Tcl_ObjectContextMethod(
    Tcl_ObjectContext context)
{
    CallContext *contextPtr = (CallContext *) context;
    return (Tcl_Method) contextPtr->callPtr->chain[contextPtr->index].mPtr;
}

int
Tcl_ObjectContextIsFiltering(
    Tcl_ObjectContext context)
{
    CallContext *contextPtr = (CallContext *) context;
    return contextPtr->callPtr->chain[contextPtr->index].isFilter;
}

Tcl_Object
Tcl_ObjectContextObject(
    Tcl_ObjectContext context)
{
    return (Tcl_Object) ((CallContext *)context)->oPtr;
}

int
Tcl_ObjectContextSkippedArgs(
    Tcl_ObjectContext context)
{
    return ((CallContext *)context)->skip;
}

Tcl_Namespace *
Tcl_GetObjectNamespace(
    Tcl_Object object)
{
    return ((Object *)object)->namespacePtr;
}

Tcl_Command
Tcl_GetObjectCommand(
    Tcl_Object object)
{
    return ((Object *)object)->command;
}

Tcl_Class
Tcl_GetObjectAsClass(
    Tcl_Object object)
{
    return (Tcl_Class) ((Object *)object)->classPtr;
}

int
Tcl_ObjectDeleted(
    Tcl_Object object)
{
    return (((Object *)object)->flags & OBJECT_DELETED) ? 1 : 0;
}

Tcl_Object
Tcl_GetClassAsObject(
    Tcl_Class clazz)
{
    return (Tcl_Object) ((Class *)clazz)->thisPtr;
}

Tcl_ObjectMapMethodNameProc
Tcl_ObjectGetMethodNameMapper(
    Tcl_Object object)
{
    return ((Object *) object)->mapMethodNameProc;
}

void
Tcl_ObjectSetMethodNameMapper(
    Tcl_Object object,
    Tcl_ObjectMapMethodNameProc mapMethodNameProc)
{
    ((Object *) object)->mapMethodNameProc = mapMethodNameProc;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
