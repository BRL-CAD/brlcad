/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tcl]
 *  DESCRIPTION:  Object-Oriented Extensions to Tcl
 *
 *  [incr Tcl] provides object-oriented extensions to Tcl, much as
 *  C++ provides object-oriented extensions to C.  It provides a means
 *  of encapsulating related procedures together with their shared data
 *  in a local namespace that is hidden from the outside world.  It
 *  promotes code re-use through inheritance.  More than anything else,
 *  it encourages better organization of Tcl applications through the
 *  object-oriented paradigm, leading to code that is easier to
 *  understand and maintain.
 *
 *  Procedures in this file support the old-style syntax for [incr Tcl]
 *  class definitions:
 *
 *    itcl_class <className> {
 *        inherit <base-class>...
 *
 *        constructor {<arglist>} { <body> }
 *        destructor { <body> }
 *
 *        method <name> {<arglist>} { <body> }
 *        proc <name> {<arglist>} { <body> }
 *
 *        public <varname> ?<init>? ?<config>?
 *        protected <varname> ?<init>?
 *        common <varname> ?<init>?
 *    }
 *
 *  This capability will be removed in a future release, after users
 *  have had a chance to switch over to the new syntax.
 *
 * ========================================================================
 *  AUTHOR:  Michael J. McLennan
 *           Bell Labs Innovations for Lucent Technologies
 *           mmclennan@lucent.com
 *           http://www.tcltk.com/itcl
 *
 *     RCS:  $Id$
 * ========================================================================
 *           Copyright (c) 1993-1998  Lucent Technologies, Inc.
 * ------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#include "itclInt.h"

/*
 *  FORWARD DECLARATIONS
 */
static int ItclOldClassCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));

static int ItclOldMethodCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));
static int ItclOldPublicCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));
static int ItclOldProtectedCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));
static int ItclOldCommonCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));

static int ItclOldBiDeleteCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));
static int ItclOldBiVirtualCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));
static int ItclOldBiPreviousCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));

static int ItclOldBiInfoMethodsCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));
static int ItclOldBiInfoProcsCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));
static int ItclOldBiInfoPublicsCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));
static int ItclOldBiInfoProtectedsCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));
static int ItclOldBiInfoCommonsCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj* const objv[]));


/*
 *  Standard list of built-in methods for old-style objects.
 */
typedef struct BiMethod {
    char* name;              /* method name */
    char* usage;             /* string describing usage */
    char* registration;      /* registration name for C proc */
    Tcl_ObjCmdProc *proc;    /* implementation C proc */
} BiMethod;

static BiMethod BiMethodList[] = {
    { "cget",      "-option",
                   "@itcl-oldstyle-cget",  Itcl_BiCgetCmd },
    { "configure", "?-option? ?value -option value...?",
                   "@itcl-oldstyle-configure",  Itcl_BiConfigureCmd },
    { "delete",    "",
                   "@itcl-oldstyle-delete",  ItclOldBiDeleteCmd },
    { "isa",       "className",
                   "@itcl-oldstyle-isa",  Itcl_BiIsaCmd },
};
static int BiMethodListLen = sizeof(BiMethodList)/sizeof(BiMethod);


/*
 * ------------------------------------------------------------------------
 *  Itcl_OldInit()
 *
 *  Invoked by Itcl_Init() whenever a new interpeter is created to add
 *  [incr Tcl] facilities.  Adds the commands needed for backward
 *  compatibility with previous releases of [incr Tcl].
 * ------------------------------------------------------------------------
 */
int
Itcl_OldInit(interp,info)
    Tcl_Interp *interp;     /* interpreter to be updated */
    ItclObjectInfo *info;   /* info regarding all known objects */
{
    int i;
    Tcl_Namespace *parserNs, *oldBiNs;

    /*
     *  Declare all of the old-style built-in methods as C procedures.
     */
    for (i=0; i < BiMethodListLen; i++) {
        if (Itcl_RegisterObjC(interp,
                BiMethodList[i].registration+1, BiMethodList[i].proc,
                (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL) != TCL_OK) {

            return TCL_ERROR;
        }
    }

    /*
     *  Create the "itcl::old-parser" namespace for backward
     *  compatibility, to handle the old-style class definitions.
     */
    parserNs = Tcl_CreateNamespace(interp, "::itcl::old-parser",
        (ClientData)info, Itcl_ReleaseData);

    if (!parserNs) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            " (cannot initialize itcl old-style parser)",
            (char*)NULL);
        return TCL_ERROR;
    }
    Itcl_PreserveData((ClientData)info);

    /*
     *  Add commands for parsing old-style class definitions.
     */
    Tcl_CreateObjCommand(interp, "::itcl::old-parser::inherit",
        Itcl_ClassInheritCmd, (ClientData)info, (Tcl_CmdDeleteProc*)NULL);

    Tcl_CreateObjCommand(interp, "::itcl::old-parser::constructor",
        Itcl_ClassConstructorCmd, (ClientData)info, (Tcl_CmdDeleteProc*)NULL);

    Tcl_CreateObjCommand(interp, "::itcl::old-parser::destructor",
        Itcl_ClassDestructorCmd, (ClientData)info, (Tcl_CmdDeleteProc*)NULL);

    Tcl_CreateObjCommand(interp, "::itcl::old-parser::method",
        ItclOldMethodCmd, (ClientData)info, (Tcl_CmdDeleteProc*)NULL);

    Tcl_CreateObjCommand(interp, "::itcl::old-parser::proc",
        Itcl_ClassProcCmd, (ClientData)info, (Tcl_CmdDeleteProc*)NULL);

    Tcl_CreateObjCommand(interp, "::itcl::old-parser::public",
        ItclOldPublicCmd, (ClientData)info, (Tcl_CmdDeleteProc*)NULL);

    Tcl_CreateObjCommand(interp, "::itcl::old-parser::protected",
        ItclOldProtectedCmd, (ClientData)info, (Tcl_CmdDeleteProc*)NULL);

    Tcl_CreateObjCommand(interp, "::itcl::old-parser::common",
        ItclOldCommonCmd, (ClientData)info, (Tcl_CmdDeleteProc*)NULL);

    /*
     *  Set the runtime variable resolver for the parser namespace,
     *  to control access to "common" data members while parsing
     *  the class definition.
     */
    Tcl_SetNamespaceResolvers(parserNs, (Tcl_ResolveCmdProc*)NULL,
        Itcl_ParseVarResolver, (Tcl_ResolveCompiledVarProc*)NULL);

    /*
     *  Create the "itcl::old-builtin" namespace for backward
     *  compatibility with the old-style built-in commands.
     */
    Tcl_CreateObjCommand(interp, "::itcl::old-builtin::virtual",
        ItclOldBiVirtualCmd, (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL);

    Tcl_CreateObjCommand(interp, "::itcl::old-builtin::previous",
        ItclOldBiPreviousCmd, (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL);

    if (Itcl_CreateEnsemble(interp, "::itcl::old-builtin::info") != TCL_OK) {
        return TCL_ERROR;
    }

    if (Itcl_AddEnsemblePart(interp, "::itcl::old-builtin::info",
            "class", "", Itcl_BiInfoClassCmd,
            (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL)
            != TCL_OK ||

        Itcl_AddEnsemblePart(interp, "::itcl::old-builtin::info",
            "inherit", "", Itcl_BiInfoInheritCmd,
            (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL)
            != TCL_OK ||

        Itcl_AddEnsemblePart(interp, "::itcl::old-builtin::info",
            "heritage", "", Itcl_BiInfoHeritageCmd,
            (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL)
            != TCL_OK ||

        Itcl_AddEnsemblePart(interp, "::itcl::old-builtin::info",
            "method", "?methodName? ?-args? ?-body?",
            ItclOldBiInfoMethodsCmd,
            (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL)
            != TCL_OK ||

        Itcl_AddEnsemblePart(interp, "::itcl::old-builtin::info",
            "proc", "?procName? ?-args? ?-body?",
            ItclOldBiInfoProcsCmd,
            (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL)
            != TCL_OK ||

        Itcl_AddEnsemblePart(interp, "::itcl::old-builtin::info",
            "public", "?varName? ?-init? ?-value? ?-config?",
            ItclOldBiInfoPublicsCmd,
            (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL)
            != TCL_OK ||

        Itcl_AddEnsemblePart(interp, "::itcl::old-builtin::info",
            "protected", "?varName? ?-init? ?-value?",
            ItclOldBiInfoProtectedsCmd,
            (ClientData)NULL,(Tcl_CmdDeleteProc*)NULL)
            != TCL_OK ||

        Itcl_AddEnsemblePart(interp, "::itcl::old-builtin::info",
            "common", "?varName? ?-init? ?-value?",
            ItclOldBiInfoCommonsCmd,
            (ClientData)NULL,(Tcl_CmdDeleteProc*)NULL)
            != TCL_OK ||

        Itcl_AddEnsemblePart(interp, "::itcl::old-builtin::info",
            "args", "procname", Itcl_BiInfoArgsCmd,
            (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL)
            != TCL_OK ||

        Itcl_AddEnsemblePart(interp, "::itcl::old-builtin::info",
            "body", "procname", Itcl_BiInfoBodyCmd,
            (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL)
            != TCL_OK
    ) {
        return TCL_ERROR;
    }

    /*
     *  Plug in an "@error" handler to handle other options from
     *  the usual info command.
     */
    if (Itcl_AddEnsemblePart(interp, "::itcl::old-builtin::info",
            "@error", (char*)NULL, Itcl_DefaultInfoCmd,
            (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL)
            != TCL_OK
    ) {
        return TCL_ERROR;
    }

    oldBiNs = Tcl_FindNamespace(interp, "::itcl::old-builtin",
        (Tcl_Namespace*)NULL, TCL_LEAVE_ERR_MSG);

    if (!oldBiNs ||
        Tcl_Export(interp, oldBiNs, "*", /* resetListFirst */ 1) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Install the "itcl_class" and "itcl_info" commands into
     *  the global scope.  This supports the old syntax for
     *  backward compatibility.
     */
    Tcl_CreateObjCommand(interp, "::itcl_class", ItclOldClassCmd,
        (ClientData)info, Itcl_ReleaseData);
    Itcl_PreserveData((ClientData)info);


    if (Itcl_CreateEnsemble(interp, "::itcl_info") != TCL_OK) {
        return TCL_ERROR;
    }

    if (Itcl_AddEnsemblePart(interp, "::itcl_info",
            "classes", "?pattern?",
            Itcl_FindClassesCmd, (ClientData)info, Itcl_ReleaseData)
            != TCL_OK) {
        return TCL_ERROR;
    }
    Itcl_PreserveData((ClientData)info);

    if (Itcl_AddEnsemblePart(interp, "::itcl_info",
            "objects", "?-class className? ?-isa className? ?pattern?",
            Itcl_FindObjectsCmd, (ClientData)info, Itcl_ReleaseData)
            != TCL_OK) {
        return TCL_ERROR;
    }
    Itcl_PreserveData((ClientData)info);

    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_InstallOldBiMethods()
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
Itcl_InstallOldBiMethods(interp, cdefn)
    Tcl_Interp *interp;      /* current interpreter */
    ItclClass *cdefn;        /* class definition to be updated */
{
    int result = TCL_OK;

    int i;
    ItclHierIter hier;
    ItclClass *cdPtr;
    Tcl_HashEntry *entry;

    /*
     *  Scan through all of the built-in methods and see if
     *  that method already exists in the class.  If not, add
     *  it in.
     *
     *  TRICKY NOTE:  The virtual tables haven't been built yet,
     *    so look for existing methods the hard way--by scanning
     *    through all classes.
     */
    for (i=0; i < BiMethodListLen; i++) {
        Itcl_InitHierIter(&hier, cdefn);
        cdPtr = Itcl_AdvanceHierIter(&hier);

        entry = NULL;
        while (cdPtr) {
            entry = Tcl_FindHashEntry(&cdPtr->functions, BiMethodList[i].name);
            if (entry) {
                break;
            }
            cdPtr = Itcl_AdvanceHierIter(&hier);
        }
        Itcl_DeleteHierIter(&hier);

        if (!entry) {
            result = Itcl_CreateMethod(interp, cdefn, BiMethodList[i].name,
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
 *  ItclOldClassCmd()
 *
 *  Invoked by Tcl whenever the user issues a "itcl_class" command to
 *  specify a class definition.  Handles the following syntax:
 *
 *    itcl_class <className> {
 *        inherit <base-class>...
 *
 *        constructor {<arglist>} { <body> }
 *        destructor { <body> }
 *
 *        method <name> {<arglist>} { <body> }
 *        proc <name> {<arglist>} { <body> }
 *
 *        public <varname> ?<init>? ?<config>?
 *        protected <varname> ?<init>?
 *        common <varname> ?<init>?
 *    }
 *
 *  NOTE:  This command is will only be provided for a limited time,
 *         to support backward compatibility with the old-style
 *         [incr Tcl] syntax.  Users should convert their scripts
 *         to use the newer syntax (Itcl_ClassCmd()) as soon as possible.
 *
 * ------------------------------------------------------------------------
 */
static int
ItclOldClassCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* info for all known objects */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    ItclObjectInfo* info = (ItclObjectInfo*)clientData;

    int result;
    char *className;
    Tcl_Namespace *parserNs;
    ItclClass *cdefnPtr;
    Tcl_HashEntry* entry;
    ItclMemberFunc *mfunc;
    Tcl_CallFrame frame;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "name { definition }");
        return TCL_ERROR;
    }
    className = Tcl_GetStringFromObj(objv[1], (int*)NULL);

    /*
     *  Find the namespace to use as a parser for the class definition.
     *  If for some reason it is destroyed, bail out here.
     */
    parserNs = Tcl_FindNamespace(interp, "::itcl::old-parser",
        (Tcl_Namespace*)NULL, TCL_LEAVE_ERR_MSG);

    if (parserNs == NULL) {
        char msg[256];
        sprintf(msg, "\n    (while parsing class definition for \"%.100s\")",
            className);
        Tcl_AddErrorInfo(interp, msg);
        return TCL_ERROR;
    }

    /*
     *  Try to create the specified class and its namespace.
     */
    if (Itcl_CreateClass(interp, className, info, &cdefnPtr) != TCL_OK) {
        return TCL_ERROR;
    }
    cdefnPtr->flags |= ITCL_OLD_STYLE;

    /*
     *  Import the built-in commands from the itcl::old-builtin
     *  and itcl::builtin namespaces.  Do this before parsing the
     *  class definition, so methods/procs can override the built-in
     *  commands.
     */
    result = Tcl_Import(interp, cdefnPtr->namesp, "::itcl::builtin::*",
        /* allowOverwrite */ 1);

    if (result == TCL_OK) {
        result = Tcl_Import(interp, cdefnPtr->namesp, "::itcl::old-builtin::*",
            /* allowOverwrite */ 1);
    }

    if (result != TCL_OK) {
        char msg[256];
        sprintf(msg, "\n    (while installing built-in commands for class \"%.100s\")", className);
        Tcl_AddErrorInfo(interp, msg);

        Tcl_DeleteNamespace(cdefnPtr->namesp);
        return TCL_ERROR;
    }

    /*
     *  Push this class onto the class definition stack so that it
     *  becomes the current context for all commands in the parser.
     *  Activate the parser and evaluate the class definition.
     */
    Itcl_PushStack((ClientData)cdefnPtr, &info->cdefnStack);

    result = Tcl_PushCallFrame(interp, &frame, parserNs,
        /* isProcCallFrame */ 0);

    if (result == TCL_OK) {
        result = Tcl_EvalObj(interp, objv[2]);
        Tcl_PopCallFrame(interp);
    }
    Itcl_PopStack(&info->cdefnStack);

    if (result != TCL_OK) {
        char msg[256];
        sprintf(msg, "\n    (class \"%.200s\" body line %d)",
            className, interp->errorLine);
        Tcl_AddErrorInfo(interp, msg);

        Tcl_DeleteNamespace(cdefnPtr->namesp);
        return TCL_ERROR;
    }

    /*
     *  At this point, parsing of the class definition has succeeded.
     *  Add built-in methods such as "configure" and "cget"--as long
     *  as they don't conflict with those defined in the class.
     */
    if (Itcl_InstallOldBiMethods(interp, cdefnPtr) != TCL_OK) {
        Tcl_DeleteNamespace(cdefnPtr->namesp);
        return TCL_ERROR;
    }

    /*
     *  See if this class has a "constructor", and if it does, mark
     *  it as "old-style".  This will allow the "config" argument
     *  to work.
     */
    entry = Tcl_FindHashEntry(&cdefnPtr->functions, "constructor");
    if (entry) {
        mfunc = (ItclMemberFunc*)Tcl_GetHashValue(entry);
        mfunc->member->flags |= ITCL_OLD_STYLE;
    }

    /*
     *  Build the virtual tables for this class.
     */
    Itcl_BuildVirtualTables(cdefnPtr);

    Tcl_ResetResult(interp);
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  ItclOldMethodCmd()
 *
 *  Invoked by Tcl during the parsing of a class definition whenever
 *  the "method" command is invoked to define an object method.
 *  Handles the following syntax:
 *
 *      method <name> {<arglist>} {<body>}
 *
 * ------------------------------------------------------------------------
 */
static int
ItclOldMethodCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* info for all known objects */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    ItclObjectInfo *info = (ItclObjectInfo*)clientData;
    ItclClass *cdefn = (ItclClass*)Itcl_PeekStack(&info->cdefnStack);

    char *name, *arglist, *body;
    Tcl_HashEntry *entry;
    ItclMemberFunc *mfunc;

    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "name args body");
        return TCL_ERROR;
    }

    name = Tcl_GetStringFromObj(objv[1], (int*)NULL);
    if (Tcl_FindHashEntry(&cdefn->functions, name)) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "\"", name, "\" already defined in class \"", cdefn->name, "\"",
            (char*)NULL);
        return TCL_ERROR;
    }

    arglist = Tcl_GetStringFromObj(objv[2], (int*)NULL);
    body    = Tcl_GetStringFromObj(objv[3], (int*)NULL);

    if (Itcl_CreateMethod(interp, cdefn, name, arglist, body) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Find the method that was just created and mark it as an
     *  "old-style" method, so that the magic "config" argument
     *  will be allowed to work.  This is done for backward-
     *  compatibility with earlier releases.  In the latest version,
     *  use of the "config" argument is discouraged.
     */
    entry = Tcl_FindHashEntry(&cdefn->functions, name);
    if (entry) {
        mfunc = (ItclMemberFunc*)Tcl_GetHashValue(entry);
        mfunc->member->flags |= ITCL_OLD_STYLE;
    }

    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  ItclOldPublicCmd()
 *
 *  Invoked by Tcl during the parsing of a class definition whenever
 *  the "public" command is invoked to define a public variable.
 *  Handles the following syntax:
 *
 *      public <varname> ?<init>? ?<config>?
 *
 * ------------------------------------------------------------------------
 */
static int
ItclOldPublicCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* info for all known objects */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    ItclObjectInfo *info = (ItclObjectInfo*)clientData;
    ItclClass *cdefnPtr = (ItclClass*)Itcl_PeekStack(&info->cdefnStack);

    char *name, *init, *config;
    ItclVarDefn *vdefn;

    if ((objc < 2) || (objc > 4)) {
        Tcl_WrongNumArgs(interp, 1, objv, "varname ?init? ?config?");
        return TCL_ERROR;
    }

    /*
     *  Make sure that the variable name does not contain anything
     *  goofy like a "::" scope qualifier.
     */
    name = Tcl_GetStringFromObj(objv[1], (int*)NULL);
    if (strstr(name, "::")) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "bad variable name \"", name, "\"",
            (char*)NULL);
        return TCL_ERROR;
    }

    init   = NULL;
    config = NULL;
    if (objc >= 3) {
        init = Tcl_GetStringFromObj(objv[2], (int*)NULL);
    }
    if (objc >= 4) {
        config = Tcl_GetStringFromObj(objv[3], (int*)NULL);
    }

    if (Itcl_CreateVarDefn(interp, cdefnPtr, name, init, config,
        &vdefn) != TCL_OK) {

        return TCL_ERROR;
    }

    vdefn->member->protection = ITCL_PUBLIC;

    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  ItclOldProtectedCmd()
 *
 *  Invoked by Tcl during the parsing of a class definition whenever
 *  the "protected" command is invoked to define a protected variable.
 *  Handles the following syntax:
 *
 *      protected <varname> ?<init>?
 *
 * ------------------------------------------------------------------------
 */
static int
ItclOldProtectedCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* info for all known objects */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    ItclObjectInfo *info = (ItclObjectInfo*)clientData;
    ItclClass *cdefnPtr = (ItclClass*)Itcl_PeekStack(&info->cdefnStack);

    char *name, *init;
    ItclVarDefn *vdefn;

    if ((objc < 2) || (objc > 3)) {
        Tcl_WrongNumArgs(interp, 1, objv, "varname ?init?");
        return TCL_ERROR;
    }

    /*
     *  Make sure that the variable name does not contain anything
     *  goofy like a "::" scope qualifier.
     */
    name = Tcl_GetStringFromObj(objv[1], (int*)NULL);
    if (strstr(name, "::")) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "bad variable name \"", name, "\"",
            (char*)NULL);
        return TCL_ERROR;
    }

    if (objc == 3) {
        init = Tcl_GetStringFromObj(objv[2], (int*)NULL);
    } else {
        init = NULL;
    }

    if (Itcl_CreateVarDefn(interp, cdefnPtr, name, init, (char*)NULL,
        &vdefn) != TCL_OK) {

        return TCL_ERROR;
    }

    vdefn->member->protection = ITCL_PROTECTED;

    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  ItclOldCommonCmd()
 *
 *  Invoked by Tcl during the parsing of a class definition whenever
 *  the "common" command is invoked to define a variable that is
 *  common to all objects in the class.  Handles the following syntax:
 *
 *      common <varname> ?<init>?
 *
 * ------------------------------------------------------------------------
 */
static int
ItclOldCommonCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* info for all known objects */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    ItclObjectInfo *info = (ItclObjectInfo*)clientData;
    ItclClass *cdefnPtr = (ItclClass*)Itcl_PeekStack(&info->cdefnStack);

    int newEntry;
    char *name, *init;
    ItclVarDefn *vdefn;
    Tcl_HashEntry *entry;
    Namespace *nsPtr;
    Var *varPtr;

    if ((objc < 2) || (objc > 3)) {
        Tcl_WrongNumArgs(interp, 1, objv, "varname ?init?");
        return TCL_ERROR;
    }

    /*
     *  Make sure that the variable name does not contain anything
     *  goofy like a "::" scope qualifier.
     */
    name = Tcl_GetStringFromObj(objv[1], (int*)NULL);
    if (strstr(name, "::")) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "bad variable name \"", name, "\"",
            (char*)NULL);
        return TCL_ERROR;
    }

    if (objc == 3) {
        init = Tcl_GetStringFromObj(objv[2], (int*)NULL);
    } else {
        init = NULL;
    }

    if (Itcl_CreateVarDefn(interp, cdefnPtr, name, init, (char*)NULL,
        &vdefn) != TCL_OK) {

        return TCL_ERROR;
    }

    vdefn->member->protection = ITCL_PROTECTED;
    vdefn->member->flags |= ITCL_COMMON;

    /*
     *  Create the variable in the namespace associated with the
     *  class.  Do this the hard way, to avoid the variable resolver
     *  procedures.  These procedures won't work until we rebuild
     *  the virtual tables below.
     */
    nsPtr = (Namespace*)cdefnPtr->namesp;
    entry = Tcl_CreateHashEntry(&nsPtr->varTable,
        vdefn->member->name, &newEntry);

    varPtr = _TclNewVar();
    varPtr->hPtr = entry;
    varPtr->nsPtr = nsPtr;
    varPtr->refCount++;   /* protect from being deleted */

    Tcl_SetHashValue(entry, varPtr);

    /*
     *  TRICKY NOTE:  Make sure to rebuild the virtual tables for this
     *    class so that this variable is ready to access.  The variable
     *    resolver for the parser namespace needs this info to find the
     *    variable if the developer tries to set it within the class
     *    definition.
     *
     *  If an initialization value was specified, then initialize
     *  the variable now.
     */
    Itcl_BuildVirtualTables(cdefnPtr);

    if (init) {
        init = Tcl_SetVar(interp, vdefn->member->name, init,
            TCL_NAMESPACE_ONLY);

        if (!init) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "cannot initialize common variable \"",
                vdefn->member->name, "\"",
                (char*)NULL);
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  ItclOldDeleteCmd()
 *
 *  Invokes the destructors, and deletes the object that invoked this
 *  operation.  If an error is encountered during destruction, the
 *  delete operation is aborted.  Handles the following syntax:
 *
 *     <objName> delete
 *
 *  When an object is successfully deleted, it is removed from the
 *  list of known objects, and its access command is deleted.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
ItclOldBiDeleteCmd(dummy, interp, objc, objv)
    ClientData dummy;     /* not used */
    Tcl_Interp *interp;   /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    ItclClass *contextClass;
    ItclObject *contextObj;

    if (objc != 1) {
        Tcl_WrongNumArgs(interp, 1, objv, "");
        return TCL_ERROR;
    }

    /*
     *  If there is an object context, then destruct the object
     *  and delete it.
     */
    if (Itcl_GetContext(interp, &contextClass, &contextObj) != TCL_OK) {
        return TCL_ERROR;
    }

    if (!contextObj) {
        Tcl_SetResult(interp, "improper usage: should be \"object delete\"",
            TCL_STATIC);
        return TCL_ERROR;
    }

    if (Itcl_DeleteObject(interp, contextObj) != TCL_OK) {
        return TCL_ERROR;
    }

    Tcl_ResetResult(interp);
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  ItclOldVirtualCmd()
 *
 *  Executes the remainder of its command line arguments in the
 *  most-specific class scope for the current object.  If there is
 *  no object context, this fails.
 *
 *  NOTE:  All methods are now implicitly virtual, and there are
 *    much better ways to manipulate scope.  This command is only
 *    provided for backward-compatibility, and should be avoided.
 *
 *  Returns a status TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
ItclOldBiVirtualCmd(dummy, interp, objc, objv)
    ClientData dummy;        /* not used */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    int result;
    ItclClass *contextClass;
    ItclObject *contextObj;
    ItclContext context;

    if (objc == 1) {
        Tcl_WrongNumArgs(interp, 1, objv, "command ?args...?");
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "\n  This command will be removed soon.",
            "\n  Commands are now virtual by default.",
            (char*)NULL);
        return TCL_ERROR;
    }

    /*
     *  If there is no object context, then return an error.
     */
    if (Itcl_GetContext(interp, &contextClass, &contextObj) != TCL_OK) {
        return TCL_ERROR;
    }
    if (!contextObj) {
        Tcl_ResetResult(interp);
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "cannot use \"virtual\" without an object context\n",
            "  This command will be removed soon.\n",
            "  Commands are now virtual by default.",
            (char*)NULL);
        return TCL_ERROR;
    }

    /*
     *  Install the most-specific namespace for this object, with
     *  the object context as clientData.  Invoke the rest of the
     *  args as a command in that namespace.
     */
    if (Itcl_PushContext(interp, (ItclMember*)NULL, contextObj->classDefn,
        contextObj, &context) != TCL_OK) {

        return TCL_ERROR;
    }

    result = Itcl_EvalArgs(interp, objc-1, objv+1);
    Itcl_PopContext(interp, &context);

    return result;
}


/*
 * ------------------------------------------------------------------------
 *  ItclOldPreviousCmd()
 *
 *  Executes the remainder of its command line arguments in the
 *  previous class scope (i.e., the next scope up in the heritage
 *  list).
 *
 *  NOTE:  There are much better ways to manipulate scope.  This
 *    command is only provided for backward-compatibility, and should
 *    be avoided.
 *
 *  Returns a status TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
ItclOldBiPreviousCmd(dummy, interp, objc, objv)
    ClientData dummy;        /* not used */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    int result;
    char *name;
    ItclClass *contextClass, *base;
    ItclObject *contextObj;
    ItclMember *member;
    ItclMemberFunc *mfunc;
    Itcl_ListElem *elem;
    Tcl_HashEntry *entry;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "command ?args...?");
        return TCL_ERROR;
    }

    /*
     *  If the current context is not a class namespace,
     *  return an error.
     */
    if (Itcl_GetContext(interp, &contextClass, &contextObj) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Get the heritage information for this class and move one
     *  level up in the hierarchy.  If there is no base class,
     *  return an error.
     */
    elem = Itcl_FirstListElem(&contextClass->bases);
    if (!elem) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "no previous class in inheritance hierarchy for \"",
            contextClass->name, "\"",
            (char*)NULL);
        return TCL_ERROR;
    }
    base = (ItclClass*)Itcl_GetListValue(elem);

    /*
     *  Look in the command resolution table for the base class
     *  to find the desired method.
     */
    name = Tcl_GetStringFromObj(objv[1], (int*)NULL);
    entry = Tcl_FindHashEntry(&base->resolveCmds, name);
    if (!entry) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "invalid command name \"", base->name, "::", name, "\"",
            (char*)NULL);
        return TCL_ERROR;
    }

    mfunc = (ItclMemberFunc*)Tcl_GetHashValue(entry);
    member = mfunc->member;

    /*
     *  Make sure that this method is accessible.
     */
    if (mfunc->member->protection != ITCL_PUBLIC) {
        Tcl_Namespace *contextNs = Itcl_GetTrueNamespace(interp,
            member->classDefn->info);

        if (!Itcl_CanAccessFunc(mfunc, contextNs)) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "can't access \"", member->fullname, "\": ",
                Itcl_ProtectionStr(member->protection), " function",
                (char*)NULL);
            return TCL_ERROR;
        }
    }

    /*
     *  Invoke the desired method by calling Itcl_EvalMemberCode.
     *  directly.  This bypasses the virtual behavior built into
     *  the usual Itcl_ExecMethod handler.
     */
    result = Itcl_EvalMemberCode(interp, mfunc, member, contextObj,
        objc-1, objv+1);

    result = Itcl_ReportFuncErrors(interp, mfunc, contextObj, result);

    return result;
}


/*
 * ------------------------------------------------------------------------
 *  ItclOldBiInfoMethodsCmd()
 *
 *  Returns information regarding methods for an object.  This command
 *  can be invoked with or without an object context:
 *
 *    <objName> info...   <= returns info for most-specific class
 *    info...             <= returns info for active namespace
 *
 *  Handles the following syntax:
 *
 *    info method ?methodName? ?-args? ?-body?
 *
 *  If the ?methodName? is not specified, then a list of all known
 *  methods is returned.  Otherwise, the information (args/body) for
 *  a specific method is returned.  Returns a status TCL_OK/TCL_ERROR
 *  to indicate success/failure.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
ItclOldBiInfoMethodsCmd(dummy, interp, objc, objv)
    ClientData dummy;        /* not used */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    char *methodName = NULL;
    int methodArgs = 0;
    int methodBody = 0;

    char *token;
    ItclClass *contextClass, *cdefn;
    ItclObject *contextObj;
    ItclHierIter hier;
    Tcl_HashSearch place;
    Tcl_HashEntry *entry;
    ItclMemberFunc *mfunc;
    ItclMemberCode *mcode;
    Tcl_Obj *objPtr, *listPtr;

    /*
     *  If this command is not invoked within a class namespace,
     *  signal an error.
     */
    if (Itcl_GetContext(interp, &contextClass, &contextObj) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  If there is an object context, then use the most-specific
     *  class for the object.  Otherwise, use the current class
     *  namespace.
     */
    if (contextObj) {
        contextClass = contextObj->classDefn;
    }

    /*
     *  Process args:  ?methodName? ?-args? ?-body?
     */
    objv++;  /* skip over command name */
    objc--;

    if (objc > 0) {
        methodName = Tcl_GetStringFromObj(*objv, (int*)NULL);
        objc--; objv++;
    }
    for ( ; objc > 0; objc--, objv++) {
        token = Tcl_GetStringFromObj(*objv, (int*)NULL);
        if (strcmp(token, "-args") == 0)
            methodArgs = ~0;
        else if (strcmp(token, "-body") == 0)
            methodBody = ~0;
        else {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "bad option \"", token, "\": should be -args or -body",
                (char*)NULL);
            return TCL_ERROR;
        }
    }

    /*
     *  Return info for a specific method.
     */
    if (methodName) {
        entry = Tcl_FindHashEntry(&contextClass->resolveCmds, methodName);
        if (entry) {
            int i, valc = 0;
            Tcl_Obj *valv[5];

            mfunc = (ItclMemberFunc*)Tcl_GetHashValue(entry);
            if ((mfunc->member->flags & ITCL_COMMON) != 0) {
                return TCL_OK;
            }

            /*
             *  If the implementation has not yet been defined,
             *  autoload it now.
             */
            if (Itcl_GetMemberCode(interp, mfunc->member) != TCL_OK) {
                return TCL_ERROR;
            }
            mcode = mfunc->member->code;

            if (!methodArgs && !methodBody) {
                objPtr = Tcl_NewStringObj(mfunc->member->classDefn->name, -1);
                Tcl_AppendToObj(objPtr, "::", -1);
                Tcl_AppendToObj(objPtr, mfunc->member->name, -1);
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
                methodArgs = methodBody = ~0;
            }
            if (methodArgs) {
                if (mcode->arglist) {
                    objPtr = Itcl_ArgList(mcode->argcount, mcode->arglist);
                    Tcl_IncrRefCount(objPtr);
                    valv[valc++] = objPtr;
                }
                else {
                    objPtr = Tcl_NewStringObj("", -1);
                    Tcl_IncrRefCount(objPtr);
                    valv[valc++] = objPtr;
                }
            }
            if (methodBody) {
                objPtr = mcode->procPtr->bodyPtr;
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
            }

            /*
             *  If the result list has a single element, then
             *  return it using Tcl_SetResult() so that it will
             *  look like a string and not a list with one element.
             */
            if (valc == 1) {
                objPtr = valv[0];
            } else {
                objPtr = Tcl_NewListObj(valc, valv);
            }
            Tcl_SetObjResult(interp, objPtr);

            for (i=0; i < valc; i++) {
                Tcl_DecrRefCount(valv[i]);
            }
        }
    }

    /*
     *  Return the list of available methods.
     */
    else {
        listPtr = Tcl_NewListObj(0, (Tcl_Obj* const*)NULL);

        Itcl_InitHierIter(&hier, contextClass);
        while ((cdefn=Itcl_AdvanceHierIter(&hier)) != NULL) {
            entry = Tcl_FirstHashEntry(&cdefn->functions, &place);
            while (entry) {
                mfunc = (ItclMemberFunc*)Tcl_GetHashValue(entry);

                if ((mfunc->member->flags & ITCL_COMMON) == 0) {
                    objPtr = Tcl_NewStringObj(mfunc->member->classDefn->name, -1);
                    Tcl_AppendToObj(objPtr, "::", -1);
                    Tcl_AppendToObj(objPtr, mfunc->member->name, -1);

                    Tcl_ListObjAppendElement((Tcl_Interp*)NULL, listPtr,
                        objPtr);
                }
                entry = Tcl_NextHashEntry(&place);
            }
        }
        Itcl_DeleteHierIter(&hier);

        Tcl_SetObjResult(interp, listPtr);
    }
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  ItclOldBiInfoProcsCmd()
 *
 *  Returns information regarding procs for a class.  This command
 *  can be invoked with or without an object context:
 *
 *    <objName> info...   <= returns info for most-specific class
 *    info...             <= returns info for active namespace
 *
 *  Handles the following syntax:
 *
 *    info proc ?procName? ?-args? ?-body?
 *
 *  If the ?procName? is not specified, then a list of all known
 *  procs is returned.  Otherwise, the information (args/body) for
 *  a specific proc is returned.  Returns a status TCL_OK/TCL_ERROR
 *  to indicate success/failure.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
ItclOldBiInfoProcsCmd(dummy, interp, objc, objv)
    ClientData dummy;     /* not used */
    Tcl_Interp *interp;   /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    char *procName = NULL;
    int procArgs = 0;
    int procBody = 0;

    char *token;
    ItclClass *contextClass, *cdefn;
    ItclObject *contextObj;
    ItclHierIter hier;
    Tcl_HashSearch place;
    Tcl_HashEntry *entry;
    ItclMemberFunc *mfunc;
    ItclMemberCode *mcode;
    Tcl_Obj *objPtr, *listPtr;

    /*
     *  If this command is not invoked within a class namespace,
     *  signal an error.
     */
    if (Itcl_GetContext(interp, &contextClass, &contextObj) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  If there is an object context, then use the most-specific
     *  class for the object.  Otherwise, use the current class
     *  namespace.
     */
    if (contextObj) {
        contextClass = contextObj->classDefn;
    }

    /*
     *  Process args:  ?procName? ?-args? ?-body?
     */
    objv++;  /* skip over command name */
    objc--;

    if (objc > 0) {
        procName = Tcl_GetStringFromObj(*objv, (int*)NULL);
        objc--; objv++;
    }
    for ( ; objc > 0; objc--, objv++) {
        token = Tcl_GetStringFromObj(*objv, (int*)NULL);
        if (strcmp(token, "-args") == 0)
            procArgs = ~0;
        else if (strcmp(token, "-body") == 0)
            procBody = ~0;
        else {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "bad option \"", token, "\": should be -args or -body",
                (char*)NULL);
            return TCL_ERROR;
        }
    }

    /*
     *  Return info for a specific proc.
     */
    if (procName) {
        entry = Tcl_FindHashEntry(&contextClass->resolveCmds, procName);
        if (entry) {
            int i, valc = 0;
            Tcl_Obj *valv[5];

            mfunc = (ItclMemberFunc*)Tcl_GetHashValue(entry);
            if ((mfunc->member->flags & ITCL_COMMON) == 0) {
                return TCL_OK;
            }

            /*
             *  If the implementation has not yet been defined,
             *  autoload it now.
             */
            if (Itcl_GetMemberCode(interp, mfunc->member) != TCL_OK) {
                return TCL_ERROR;
            }
            mcode = mfunc->member->code;

            if (!procArgs && !procBody) {
                objPtr = Tcl_NewStringObj(mfunc->member->fullname, -1);
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
                procArgs = procBody = ~0;
            }
            if (procArgs) {
                if (mcode->arglist) {
                    objPtr = Itcl_ArgList(mcode->argcount, mcode->arglist);
                    Tcl_IncrRefCount(objPtr);
                    valv[valc++] = objPtr;
                }
                else {
                    objPtr = Tcl_NewStringObj("", -1);
                    Tcl_IncrRefCount(objPtr);
                    valv[valc++] = objPtr;
                }
            }
            if (procBody) {
                objPtr = mcode->procPtr->bodyPtr;
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
            }

            /*
             *  If the result list has a single element, then
             *  return it using Tcl_SetResult() so that it will
             *  look like a string and not a list with one element.
             */
            if (valc == 1) {
                objPtr = valv[0];
            } else {
                objPtr = Tcl_NewListObj(valc, valv);
            }
            Tcl_SetObjResult(interp, objPtr);

            for (i=0; i < valc; i++) {
                Tcl_DecrRefCount(valv[i]);
            }
        }
    }

    /*
     *  Return the list of available procs.
     */
    else {
        listPtr = Tcl_NewListObj(0, (Tcl_Obj* const*)NULL);

        Itcl_InitHierIter(&hier, contextClass);
        while ((cdefn=Itcl_AdvanceHierIter(&hier)) != NULL) {
            entry = Tcl_FirstHashEntry(&cdefn->functions, &place);
            while (entry) {
                mfunc = (ItclMemberFunc*)Tcl_GetHashValue(entry);

                if ((mfunc->member->flags & ITCL_COMMON) != 0) {
                    objPtr = Tcl_NewStringObj(mfunc->member->classDefn->name, -1);
                    Tcl_AppendToObj(objPtr, "::", -1);
                    Tcl_AppendToObj(objPtr, mfunc->member->name, -1);

                    Tcl_ListObjAppendElement((Tcl_Interp*)NULL, listPtr,
                        objPtr);
                }
                entry = Tcl_NextHashEntry(&place);
            }
        }
        Itcl_DeleteHierIter(&hier);

        Tcl_SetObjResult(interp, listPtr);
    }
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  ItclOldBiInfoPublicsCmd()
 *
 *  Sets the interpreter result to contain information for public
 *  variables in the class.  Handles the following syntax:
 *
 *     info public ?varName? ?-init? ?-value? ?-config?
 *
 *  If the ?varName? is not specified, then a list of all known public
 *  variables is returned.  Otherwise, the information (init/value/config)
 *  for a specific variable is returned.  Returns a status
 *  TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
ItclOldBiInfoPublicsCmd(dummy, interp, objc, objv)
    ClientData dummy;     /* not used */
    Tcl_Interp *interp;   /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    char *varName = NULL;
    int varInit = 0;
    int varCheck = 0;
    int varValue = 0;

    char *token, *val;
    ItclClass *contextClass;
    ItclObject *contextObj;

    ItclClass *cdPtr;
    ItclVarLookup *vlookup;
    ItclVarDefn *vdefn;
    ItclMember *member;
    ItclHierIter hier;
    Tcl_HashEntry *entry;
    Tcl_HashSearch place;
    Tcl_Obj *objPtr, *listPtr;

    /*
     *  If this command is not invoked within a class namespace,
     *  signal an error.
     */
    if (Itcl_GetContext(interp, &contextClass, &contextObj) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Process args:  ?varName? ?-init? ?-value? ?-config?
     */
    objv++;  /* skip over command name */
    objc--;

    if (objc > 0) {
        varName = Tcl_GetStringFromObj(*objv, (int*)NULL);
        objc--; objv++;
    }
    for ( ; objc > 0; objc--, objv++) {
        token = Tcl_GetStringFromObj(*objv, (int*)NULL);
        if (strcmp(token, "-init") == 0)
            varInit = ~0;
        else if (strcmp(token, "-value") == 0)
            varValue = ~0;
        else if (strcmp(token, "-config") == 0)
            varCheck = ~0;
        else {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "bad option \"", token,
                "\": should be -init, -value or -config",
                (char*)NULL);
            return TCL_ERROR;
        }
    }

    /*
     *  Return info for a specific variable.
     */
    if (varName) {
        vlookup = NULL;
        entry = Tcl_FindHashEntry(&contextClass->resolveVars, varName);
        if (entry) {
            vlookup = (ItclVarLookup*)Tcl_GetHashValue(entry);
            if (vlookup->vdefn->member->protection != ITCL_PUBLIC) {
                vlookup = NULL;
            }
        }

        if (vlookup) {
            int i, valc = 0;
            Tcl_Obj *valv[5];

            member = vlookup->vdefn->member;

            if (!varInit && !varCheck && !varValue) {
                objPtr = Tcl_NewStringObj(member->classDefn->name, -1);
                Tcl_AppendToObj(objPtr, "::", -1);
                Tcl_AppendToObj(objPtr, member->name, -1);
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
                varInit = varCheck = varValue = ~0;
            }
            if (varInit) {
                val = (vlookup->vdefn->init) ? vlookup->vdefn->init : "";
                objPtr = Tcl_NewStringObj(val, -1);
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
            }
            if (varValue) {
                val = Itcl_GetInstanceVar(interp, member->fullname,
                    contextObj, contextObj->classDefn);

                if (!val) {
                    val = "<undefined>";
                }
                objPtr = Tcl_NewStringObj(val, -1);
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
            }

            if (varCheck) {
                if (member->code && member->code->procPtr->bodyPtr) {
                    objPtr = member->code->procPtr->bodyPtr;
                } else {
                    objPtr = Tcl_NewStringObj("", -1);
                }
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
            }

            /*
             *  If the result list has a single element, then
             *  return it using Tcl_SetResult() so that it will
             *  look like a string and not a list with one element.
             */
            if (valc == 1) {
                objPtr = valv[0];
            } else {
                objPtr = Tcl_NewListObj(valc, valv);
            }
            Tcl_SetObjResult(interp, objPtr);

            for (i=0; i < valc; i++) {
                Tcl_DecrRefCount(valv[i]);
            }
        }
    }

    /*
     *  Return the list of public variables.
     */
    else {
        listPtr = Tcl_NewListObj(0, (Tcl_Obj* const*)NULL);

        Itcl_InitHierIter(&hier, contextClass);
        cdPtr = Itcl_AdvanceHierIter(&hier);
        while (cdPtr != NULL) {
            entry = Tcl_FirstHashEntry(&cdPtr->variables, &place);
            while (entry) {
                vdefn = (ItclVarDefn*)Tcl_GetHashValue(entry);
                member = vdefn->member;

                if ((member->flags & ITCL_COMMON) == 0 &&
                     member->protection == ITCL_PUBLIC) {

                    objPtr = Tcl_NewStringObj(member->classDefn->name, -1);
                    Tcl_AppendToObj(objPtr, "::", -1);
                    Tcl_AppendToObj(objPtr, member->name, -1);

                    Tcl_ListObjAppendElement((Tcl_Interp*)NULL, listPtr,
                        objPtr);
                }
                entry = Tcl_NextHashEntry(&place);
            }
            cdPtr = Itcl_AdvanceHierIter(&hier);
        }
        Itcl_DeleteHierIter(&hier);

        Tcl_SetObjResult(interp, listPtr);
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  ItclOldBiInfoProtectedsCmd()
 *
 *  Sets the interpreter result to contain information for protected
 *  variables in the class.  Handles the following syntax:
 *
 *     info protected ?varName? ?-init? ?-value?
 *
 *  If the ?varName? is not specified, then a list of all known public
 *  variables is returned.  Otherwise, the information (init/value)
 *  for a specific variable is returned.  Returns a status
 *  TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
ItclOldBiInfoProtectedsCmd(dummy, interp, objc, objv)
    ClientData dummy;     /* not used */
    Tcl_Interp *interp;   /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    char *varName = NULL;
    int varInit = 0;
    int varValue = 0;

    char *token, *val;
    ItclClass *contextClass;
    ItclObject *contextObj;

    ItclClass *cdPtr;
    ItclVarLookup *vlookup;
    ItclVarDefn *vdefn;
    ItclMember *member;
    ItclHierIter hier;
    Tcl_HashEntry *entry;
    Tcl_HashSearch place;
    Tcl_Obj *objPtr, *listPtr;

    /*
     *  If this command is not invoked within a class namespace,
     *  signal an error.
     */
    if (Itcl_GetContext(interp, &contextClass, &contextObj) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Process args:  ?varName? ?-init? ?-value?
     */
    objv++;  /* skip over command name */
    objc--;

    if (objc > 0) {
        varName = Tcl_GetStringFromObj(*objv, (int*)NULL);
        objc--; objv++;
    }
    for ( ; objc > 0; objc--, objv++) {
        token = Tcl_GetStringFromObj(*objv, (int*)NULL);
        if (strcmp(token, "-init") == 0)
            varInit = ~0;
        else if (strcmp(token, "-value") == 0)
            varValue = ~0;
        else {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "bad option \"", token, "\": should be -init or -value",
                (char*)NULL);
            return TCL_ERROR;
        }
    }

    /*
     *  Return info for a specific variable.
     */
    if (varName) {
        vlookup = NULL;
        entry = Tcl_FindHashEntry(&contextClass->resolveVars, varName);
        if (entry) {
            vlookup = (ItclVarLookup*)Tcl_GetHashValue(entry);
            if (vlookup->vdefn->member->protection != ITCL_PROTECTED) {
                vlookup = NULL;
            }
        }

        if (vlookup) {
            int i, valc = 0;
            Tcl_Obj *valv[5];

            member = vlookup->vdefn->member;

            if (!varInit && !varValue) {
                objPtr = Tcl_NewStringObj(member->classDefn->name, -1);
                Tcl_AppendToObj(objPtr, "::", -1);
                Tcl_AppendToObj(objPtr, member->name, -1);
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
                varInit = varValue = ~0;
            }

            /*
             *  If this is the built-in "this" variable, then
             *  report the object name as its initialization string.
             */
            if (varInit) {
                if ((member->flags & ITCL_THIS_VAR) != 0) {
                    if (contextObj && contextObj->accessCmd) {
                        objPtr = Tcl_NewStringObj("", -1);
                        Tcl_IncrRefCount(objPtr);
                        Tcl_GetCommandFullName(contextObj->classDefn->interp,
                            contextObj->accessCmd, objPtr);
                        valv[valc++] = objPtr;
                    }
                    else {
                        objPtr = Tcl_NewStringObj("<objectName>", -1);
                        Tcl_IncrRefCount(objPtr);
                        valv[valc++] = objPtr;
                    }
                }
                else {
                    val = (vlookup->vdefn->init) ? vlookup->vdefn->init : "";
                    objPtr = Tcl_NewStringObj(val, -1);
                    Tcl_IncrRefCount(objPtr);
                    valv[valc++] = objPtr;
                }
            }

            if (varValue) {
                val = Itcl_GetInstanceVar(interp, member->fullname,
                    contextObj, contextObj->classDefn);

                if (!val) {
                    val = "<undefined>";
                }
                objPtr = Tcl_NewStringObj(val, -1);
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
            }

            /*
             *  If the result list has a single element, then
             *  return it using Tcl_SetResult() so that it will
             *  look like a string and not a list with one element.
             */
            if (valc == 1) {
                objPtr = valv[0];
            } else {
                objPtr = Tcl_NewListObj(valc, valv);
            }
            Tcl_SetObjResult(interp, objPtr);

            for (i=0; i < valc; i++) {
                Tcl_DecrRefCount(valv[i]);
            }
        }
    }

    /*
     *  Return the list of public variables.
     */
    else {
        listPtr = Tcl_NewListObj(0, (Tcl_Obj* const*)NULL);

        Itcl_InitHierIter(&hier, contextClass);
        cdPtr = Itcl_AdvanceHierIter(&hier);
        while (cdPtr != NULL) {
            entry = Tcl_FirstHashEntry(&cdPtr->variables, &place);
            while (entry) {
                vdefn = (ItclVarDefn*)Tcl_GetHashValue(entry);
                member = vdefn->member;

                if ((member->flags & ITCL_COMMON) == 0 &&
                     member->protection == ITCL_PROTECTED) {

                    objPtr = Tcl_NewStringObj(member->classDefn->name, -1);
                    Tcl_AppendToObj(objPtr, "::", -1);
                    Tcl_AppendToObj(objPtr, member->name, -1);

                    Tcl_ListObjAppendElement((Tcl_Interp*)NULL, listPtr,
                        objPtr);
                }
                entry = Tcl_NextHashEntry(&place);
            }
            cdPtr = Itcl_AdvanceHierIter(&hier);
        }
        Itcl_DeleteHierIter(&hier);

        Tcl_SetObjResult(interp, listPtr);
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  ItclOldBiInfoCommonsCmd()
 *
 *  Sets the interpreter result to contain information for common
 *  variables in the class.  Handles the following syntax:
 *
 *     info common ?varName? ?-init? ?-value?
 *
 *  If the ?varName? is not specified, then a list of all known common
 *  variables is returned.  Otherwise, the information (init/value)
 *  for a specific variable is returned.  Returns a status
 *  TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
ItclOldBiInfoCommonsCmd(dummy, interp, objc, objv)
    ClientData dummy;     /* not used */
    Tcl_Interp *interp;   /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *const objv[];   /* argument objects */
{
    char *varName = NULL;
    int varInit = 0;
    int varValue = 0;

    char *token, *val;
    ItclClass *contextClass;
    ItclObject *contextObj;

    ItclClass *cdPtr;
    ItclVarDefn *vdefn;
    ItclVarLookup *vlookup;
    ItclMember *member;
    ItclHierIter hier;
    Tcl_HashEntry *entry;
    Tcl_HashSearch place;
    Tcl_Obj *objPtr, *listPtr;

    /*
     *  If this command is not invoked within a class namespace,
     *  signal an error.
     */
    if (Itcl_GetContext(interp, &contextClass, &contextObj) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Process args:  ?varName? ?-init? ?-value?
     */
    objv++;  /* skip over command name */
    objc--;

    if (objc > 0) {
        varName = Tcl_GetStringFromObj(*objv, (int*)NULL);
        objc--; objv++;
    }
    for ( ; objc > 0; objc--, objv++) {
        token = Tcl_GetStringFromObj(*objv, (int*)NULL);
        if (strcmp(token, "-init") == 0)
            varInit = ~0;
        else if (strcmp(token, "-value") == 0)
            varValue = ~0;
        else {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "bad option \"", token, "\": should be -init or -value",
                (char*)NULL);
            return TCL_ERROR;
        }
    }

    /*
     *  Return info for a specific variable.
     */
    if (varName) {
        vlookup = NULL;
        entry = Tcl_FindHashEntry(&contextClass->resolveVars, varName);
        if (entry) {
            vlookup = (ItclVarLookup*)Tcl_GetHashValue(entry);
            if (vlookup->vdefn->member->protection != ITCL_PROTECTED) {
                vlookup = NULL;
            }
        }

        if (vlookup) {
            int i, valc = 0;
            Tcl_Obj *valv[5];

            member = vlookup->vdefn->member;

            if (!varInit && !varValue) {
                objPtr = Tcl_NewStringObj(member->classDefn->name, -1);
                Tcl_AppendToObj(objPtr, "::", -1);
                Tcl_AppendToObj(objPtr, member->name, -1);
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
                varInit = varValue = ~0;
            }
            if (varInit) {
                val = (vlookup->vdefn->init) ? vlookup->vdefn->init : "";
                objPtr = Tcl_NewStringObj(val, -1);
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
            }

            if (varValue) {
                val = Itcl_GetCommonVar(interp, member->fullname,
                    contextObj->classDefn);

                if (!val) {
                    val = "<undefined>";
                }
                objPtr = Tcl_NewStringObj(val, -1);
                Tcl_IncrRefCount(objPtr);
                valv[valc++] = objPtr;
            }

            /*
             *  If the result list has a single element, then
             *  return it using Tcl_SetResult() so that it will
             *  look like a string and not a list with one element.
             */
            if (valc == 1) {
                objPtr = valv[0];
            } else {
                objPtr = Tcl_NewListObj(valc, valv);
            }
            Tcl_SetObjResult(interp, objPtr);

            for (i=0; i < valc; i++) {
                Tcl_DecrRefCount(valv[i]);
            }
        }
    }

    /*
     *  Return the list of public variables.
     */
    else {
        listPtr = Tcl_NewListObj(0, (Tcl_Obj* const*)NULL);

        Itcl_InitHierIter(&hier, contextClass);
        cdPtr = Itcl_AdvanceHierIter(&hier);
        while (cdPtr != NULL) {
            entry = Tcl_FirstHashEntry(&cdPtr->variables, &place);
            while (entry) {
                vdefn = (ItclVarDefn*)Tcl_GetHashValue(entry);
                member = vdefn->member;

                if ((member->flags & ITCL_COMMON) &&
                     member->protection == ITCL_PROTECTED) {

                    objPtr = Tcl_NewStringObj(member->classDefn->name, -1);
                    Tcl_AppendToObj(objPtr, "::", -1);
                    Tcl_AppendToObj(objPtr, member->name, -1);

                    Tcl_ListObjAppendElement((Tcl_Interp*)NULL, listPtr,
                        objPtr);
                }
                entry = Tcl_NextHashEntry(&place);
            }
            cdPtr = Itcl_AdvanceHierIter(&hier);
        }
        Itcl_DeleteHierIter(&hier);

        Tcl_SetObjResult(interp, listPtr);
    }
    return TCL_OK;
}
