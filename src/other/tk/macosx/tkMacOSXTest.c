/*
 * tkMacOSXTest.c --
 *
 *	Contains commands for platform specific tests for
 *	the Macintosh platform.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2005-2007 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXInt.h"

/*
 * Forward declarations of procedures defined later in this file:
 */

static int		DebuggerCmd (ClientData dummy, Tcl_Interp *interp,
			    int argc, const char **argv);
MODULE_SCOPE int	TkplatformtestInit(Tcl_Interp *interp);

/*
 *----------------------------------------------------------------------
 *
 * TkplatformtestInit --
 *
 *	Defines commands that test platform specific functionality for
 *	Unix platforms.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Defines new commands.
 *
 *----------------------------------------------------------------------
 */

int
TkplatformtestInit(
    Tcl_Interp *interp)		/* Interpreter to add commands to. */
{
    /*
     * Add commands for platform specific tests on MacOS here.
     */

    Tcl_CreateCommand(interp, "debugger", DebuggerCmd,
	    (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DebuggerCmd --
 *
 *	This procedure simply calls the low level debugger.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
DebuggerCmd(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Not used. */
    int argc,				/* Not used. */
    const char **argv)			/* Not used. */
{
    Debugger();
    return TCL_OK;
}
