/*
 * bltBeep.c --
 *
 * Copyright 1993-1998 Lucent Technologies, Inc.
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
 * software.  */

#include "bltInt.h"

#ifndef NO_BEEP
/*
 *----------------------------------------------------------------------
 *
 * Blt_BeepCmd --
 *
 *	This procedure is invoked to process the "bell" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_CmdProc BeepCmd;

/* ARGSUSED */
static int
BeepCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter.*/
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    int percent;

    if (argc > 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
	    argv[0], " ?volumePercent?\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (argc == 1) {
	percent = 50;		/* Default setting */
    } else if (argc == 2) {
	if (Tcl_GetInt(interp, argv[1], &percent) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((percent < -100) || (percent > 100)) {
	    Tcl_AppendResult(interp, "bad volume percentage value \"",
		argv[1], "\"", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    XBell(Tk_Display(Tk_MainWindow(interp)), percent);
    return TCL_OK;
}

int
Blt_BeepInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec =
    {"beep", BeepCmd,};

    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#endif /* NO_BEEP */
