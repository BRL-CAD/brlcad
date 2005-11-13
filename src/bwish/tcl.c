/*                           T C L . C
 * BRL-CAD
 *
 * Copyright (C) 1998-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file tcl.c
 *
 *  The supporting Tcl routines for BWISH and BTCLSH.
 *
 *  Author -
 *	  Robert G. Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *
 *
 *  Cad_Main --
 *	     Main program for wish-like applications that desire command
 *	     line editing when in interactive mode. Much of this code was
 *	     borrowed from libtk/generic/tkMain.c.
 *
 */

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#ifdef BWISH
#  include "tk.h"
#else
#  include "tcl.h"
#endif
#include "tclInt.h"
#include "machine.h"
#include "bu.h"
#include "libtermio.h"

/* defined in input.c */
extern void initInput(void);

#ifdef BWISH
/* defined in libtk/(unix|win|mac)/tk(Unix|Win|Mac)Init.c */
void TkpDisplayWarning();
#endif

void Cad_MainLoop(void);
void Cad_Exit(int status);

#ifdef BWISH
#	define CAD_RCFILENAME "~/.bwishrc"
#else
#	define CAD_RCFILENAME "~/.btclshrc"
#endif

/*
 * Main program for wish-like applications that desire command
 * line editing when in interactive mode. Much of this code was
 * borrowed from libtk/generic/tkMain.c.
 *
 * Results:
 *	None. This procedure never returns (it exits the process when
 *	it's done.
 *
 * Side effects:
 *	This procedure initializes the Tk world and then starts
 *      interpreting commands;  almost anything could happen,
 *      depending on the script being interpreted.
 */
void
Cad_Main(int argc, char **argv, Tcl_AppInitProc (*appInitProc), Tcl_Interp *interp)
{
    char *filename = NULL;
    char *args;
    char buf[TCL_INTEGER_SPACE];
    int status;
    Tcl_DString argString;

    if ((argc > 1) && (argv[1][0] != '-')) {
	filename = argv[1];
	argc--;
	argv++;
    }

    /*
     * Make command-line arguments available in the Tcl variables "argc"
     * and "argv".
     */
    args = Tcl_Merge(argc-1, (const char * const *)argv+1);
    Tcl_ExternalToUtfDString(NULL, args, -1, &argString);
    Tcl_SetVar(interp, "argv", Tcl_DStringValue(&argString), TCL_GLOBAL_ONLY);
    Tcl_DStringFree(&argString);
    ckfree(args);

    if (filename == NULL) {
	(void)Tcl_ExternalToUtfDString(NULL, argv[0], -1, &argString);
    } else {
	filename = Tcl_ExternalToUtfDString(NULL, filename, -1, &argString);
    }

    TclFormatInt(buf, argc-1);
    Tcl_SetVar(interp, "argc", buf, TCL_GLOBAL_ONLY);
    Tcl_SetVar(interp, "argv0", Tcl_DStringValue(&argString), TCL_GLOBAL_ONLY);

    /*
     * Invoke application-specific initialization.
     */
    if ((*appInitProc)(interp) != TCL_OK) {
#ifdef BWISH
	TkpDisplayWarning(Tcl_GetStringResult(interp),
			  "Application initialization failed");
#else
	bu_log( "Application initialization failed: %s", Tcl_GetStringResult(interp));
#endif
    }

    if (filename != NULL) {
	/*XXX ??? need to arrange for a bu_log handler and or handlers
	  for stdout/stderr ??? */
	save_Tty(fileno(stdin));
	Tcl_ResetResult(interp);
	status = Tcl_EvalFile(interp, filename);
	if (status != TCL_OK) {
	    Tcl_AddErrorInfo(interp, "");
#ifdef BWISH
	    TkpDisplayWarning(Tcl_GetVar(interp, "errorInfo",
					 TCL_GLOBAL_ONLY), "Error in startup script");
#else
	    bu_log("Error in startup script: %s\n", Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY));
#endif
	}

#ifndef BWISH
	Cad_Exit(status);
#endif
    } else { /* We're running interactively. */
	/* Set up to handle commands from user as well as
	   provide a command line editing capability. */
	initInput();

	/* Set the name of the startup file. */
	Tcl_SetVar(interp, "tcl_rcFileName", CAD_RCFILENAME, TCL_GLOBAL_ONLY);

	/* Source the startup file if it exists. */
	Tcl_SourceRCFile(interp);
    }

    Tcl_DStringFree(&argString);
    Cad_MainLoop();
    Cad_Exit(TCL_OK);
}

void
Cad_MainLoop(void)
{
#ifdef BWISH
    while (Tk_GetNumMainWindows() > 0)
#else
	while (1)
#endif
	    {
		Tcl_DoOneEvent(0);
	    }
}

void
Cad_Exit(int status)
{
    reset_Tty(fileno(stdin));
    Tcl_Exit(status);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
