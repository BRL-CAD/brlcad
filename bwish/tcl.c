/*
 *				T C L . C
 *
 *  The supporting Tcl routines for BWISH.
 *
 *  Author -
 *	  Robert G. Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 *
 *  Cad_Main --
 *	     Main program for wish-like applications that desire command
 *	     line editing when in interactive mode. Much of this code was
 *	     borrowed from libtk/generic/tkMain.c.
 *
 */

#include "conf.h"
#include <stdio.h>
#include <ctype.h>
#include "tk.h"

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "libtermio.h"

/* defined in cmd.c */
extern void quit();

/* defined in input.c */
extern void initInput();

/* defined in libtk/(unix|win|mac)/tk(Unix|Win|Mac)Init.c */
void TkpDisplayWarning();

#define BWISH_RCFILENAME ".bwishrc"

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
Cad_Main(argc, argv, appInitProc, interp)
     int argc;
     char **argv;
     Tcl_AppInitProc *appInitProc;
     Tcl_Interp *interp;
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
	args = Tcl_Merge(argc-1, argv+1);
	Tcl_ExternalToUtfDString(NULL, args, -1, &argString);
	Tcl_SetVar(interp, "argv", Tcl_DStringValue(&argString), TCL_GLOBAL_ONLY);
	Tcl_DStringFree(&argString);
	ckfree(args);
	sprintf(buf, "%d", argc-1);

	if (filename == NULL) {
		(void)Tcl_ExternalToUtfDString(NULL, argv[0], -1, &argString);
	} else {
		filename = Tcl_ExternalToUtfDString(NULL, filename, -1, &argString);
	}

	Tcl_SetVar(interp, "argc", buf, TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "argv0", Tcl_DStringValue(&argString), TCL_GLOBAL_ONLY);

	/*
	 * Invoke application-specific initialization.
	 */
	if ((*appInitProc)(interp) != TCL_OK) {
	  TkpDisplayWarning(Tcl_GetStringResult(interp),
			    "Application initialization failed");
	}

	if (filename != NULL) {
		/*XXX ??? need to arrange for a bu_log handler and or handlers
		  for stdout/stderr ??? */
		save_Tty(fileno(stdin));
		Tcl_ResetResult(interp);
		status = Tcl_EvalFile(interp, filename);
		if (status != TCL_OK) {
			Tcl_AddErrorInfo(interp, "");
			TkpDisplayWarning(Tcl_GetVar(interp, "errorInfo",
						     TCL_GLOBAL_ONLY), "Error in startup script");
			quit(status);
		}
	} else { /* We're running interactively. */
		/* Set up to handle commands from user as well as
		   provide a command line editing capability. */
		initInput();

		/* Set the name of the startup file. */
		Tcl_SetVar(interp, "tcl_rcFileName", BWISH_RCFILENAME, TCL_GLOBAL_ONLY);

		/* Source the startup file if it exists. */
		Tcl_SourceRCFile(interp);
	}
	Tcl_DStringFree(&argString);

	Tk_MainLoop();
	Tcl_DeleteInterp(interp);
	Tcl_Exit(0);
}
