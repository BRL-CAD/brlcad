/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tk]
 *  DESCRIPTION:  Building mega-widgets with [incr Tcl]
 *
 *  [incr Tk] provides a framework for building composite "mega-widgets"
 *  using [incr Tcl] classes.  It defines a set of base classes that are
 *  specialized to create all other widgets.
 *
 *  This file defines the initialization and facilities common to all
 *  mega-widgets.
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
#include "itkInt.h"

/*
 * ------------------------------------------------------------------------
 *  Itk_ConfigBodyCmd()
 *
 *  Replacement for the usual "itcl::configbody" command.  Recognizes
 *  mega-widget options included in a class definition.  Options are
 *  identified by their "switch" name, but without the "-" prefix:
 *
 *    itcl::configbody <class>::<itkOption> <body>
 *
 *  Handles bodies for public variables as well:
 *
 *    itcl::configbody <class>::<publicVar> <body>
 *
 *  If an <itkOption> is found, it has priority over public variables.
 *  If <body> has the form "@name" then it is treated as a reference
 *  to a C handling procedure; otherwise, it is taken as a body of
 *  Tcl statements.
 *
 *  Returns TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
int
Itk_ConfigBodyCmd(
    ClientData dummy,        /* unused */
    Tcl_Interp *interp,      /* current interpreter */
    int objc,                /* number of arguments */
    Tcl_Obj *CONST objv[])   /* argument objects */
{
    int result = TCL_OK;

    char *token;
    char *head;
    char *tail;
    ItclClass *iclsPtr;
    ItclMemberCode *mcode;
    ItkClassOptTable *optTable;
    Tcl_HashEntry *entry;
    ItkClassOption *opt;
    Tcl_DString buffer;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "class::option body");
        return TCL_ERROR;
    }

    /*
     *  Parse the member name "namesp::namesp::class::option".
     *  Make sure that a class name was specified, and that the
     *  class exists.
     */
    token = Tcl_GetString(objv[1]);
    Itcl_ParseNamespPath(token, &buffer, &head, &tail);

    if (!head || *head == '\0') {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "missing class specifier for body declaration \"", token, "\"",
                (char*)NULL);
        result = TCL_ERROR;
        goto configBodyCmdDone;
    }

    iclsPtr = Itcl_FindClass(interp, head, /* autoload */ 1);
    if (iclsPtr == NULL) {
        result = TCL_ERROR;
        goto configBodyCmdDone;
    }

    /*
     *  Look first for a configuration option with that name.
     *  If it is not found, assume the reference is for a public
     *  variable, and use the usual "configbody" implementation
     *  to handle it.
     */
    optTable = Itk_FindClassOptTable(iclsPtr);
    opt = NULL;

    if (optTable) {
        Tcl_DString optName;

        Tcl_DStringInit(&optName);
        Tcl_DStringAppend(&optName, "-", -1);
        Tcl_DStringAppend(&optName, tail, -1);
        entry = Tcl_FindHashEntry(&optTable->options,
            Tcl_DStringValue(&optName));

        if (entry) {
            opt = (ItkClassOption*)Tcl_GetHashValue(entry);
        }
        Tcl_DStringFree(&optName);
    }

    if (opt == NULL) {
        result = Itcl_ConfigBodyCmd(dummy, interp, objc, objv);
        goto configBodyCmdDone;
    }

    /*
     *  Otherwise, change the implementation for this option.
     */
    token = Tcl_GetString(objv[2]);

    if (Itcl_CreateMemberCode(interp, iclsPtr, (char*)NULL, token,
        &mcode) != TCL_OK) {

        result = TCL_ERROR;
        goto configBodyCmdDone;
    }

    Itcl_PreserveData((ClientData)mcode);
#ifdef NOTDEF
    Itcl_EventuallyFree((ClientData)mcode, Itcl_DeleteMemberCode);
#endif

    if (opt->codePtr) {
        Itcl_ReleaseData((ClientData)opt->codePtr);
    }
    opt->codePtr = mcode;

configBodyCmdDone:
    Tcl_DStringFree(&buffer);
    return result;
}
