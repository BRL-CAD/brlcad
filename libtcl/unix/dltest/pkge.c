/* 
 * pkge.c --
 *
 *	This file contains a simple Tcl package "pkge" that is intended
 *	for testing the Tcl dynamic loading facilities.  Its Init
 *	procedure returns an error in order to test how this is handled.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tcl.h"

/*
 * Prototypes for procedures defined later in this file:
 */

static int	Pkgd_SubCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
static int	Pkgd_UnsafeCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));

/*
 *----------------------------------------------------------------------
 *
 * Pkge_Init --
 *
 *	This is a package initialization procedure, which is called
 *	by Tcl when this package is to be added to an interpreter.
 *
 * Results:
 *	Returns TCL_ERROR and leaves an error message in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Pkge_Init(interp)
    Tcl_Interp *interp;		/* Interpreter in which the package is
				 * to be made available. */
{
    static char script[] = "if 44 {open non_existent}";
    if (Tcl_InitStubs(interp, TCL_VERSION, 1) == NULL) {
	return TCL_ERROR;
    }
    return Tcl_Eval(interp, script);
}
