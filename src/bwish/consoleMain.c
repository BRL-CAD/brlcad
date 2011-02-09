/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "tcl.h"
#include "bio.h"
#include <locale.h>

/*
 * The following #if block allows you to change the AppInit function by
 * using a #define of TCL_LOCAL_APPINIT instead of rewriting this entire
 * file. The #if checks for that #define and uses Tcl_AppInit if it
 * doesn't exist.
 */

#ifndef TCL_LOCAL_APPINIT
#define TCL_LOCAL_APPINIT Tcl_AppInit
#endif
extern int TCL_LOCAL_APPINIT _ANSI_ARGS_((Tcl_Interp *interp));

/*
 * The following #if block allows you to change how Tcl finds the startup
 * script, prime the library or encoding paths, fiddle with the argv,
 * etc., without needing to rewrite Tcl_Main()
 */

#ifdef TCL_LOCAL_MAIN_HOOK
extern int TCL_LOCAL_MAIN_HOOK _ANSI_ARGS_((int *argc, char ***argv));
#endif

extern int Cad_AppInit(Tcl_Interp *interp);

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 * This is the main program for the application.
 *
 * Results:
 * None: Tcl_Main never returns here, so this function never returns
 * either.
 *
 * Side effects:
 * Whatever the application does.
 *
 *----------------------------------------------------------------------
 */

int
main(
     int argc,
     char *argv[])
{
    char *p;

    /*
     * Set up the default locale to be standard "C" locale so parsing is
     * performed correctly.
     */

    setlocale(LC_ALL, "C");

    /*
     * Forward slashes substituted for backslashes.
     */

    for (p = argv[0]; *p != '\0'; p++) {
	if (*p == '\\') {
	    *p = '/';
	}
    }

#ifdef TCL_LOCAL_MAIN_HOOK
    TCL_LOCAL_MAIN_HOOK(&argc, &argv);
#endif

    Tcl_Main(argc, argv, TCL_LOCAL_APPINIT);

    return 0;			/* Needed only to prevent compiler warning. */
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 * This function performs application-specific initialization. Most
 * applications, especially those that incorporate additional packages,
 * will have their own version of this function.
 *
 * Results:
 * Returns a standard Tcl completion code, and leaves an error message in
 * the interp's result if an error occurs.
 *
 * Side effects:
 * Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppInit(
	    Tcl_Interp *interp)		/* Interpreter for application. */
{
    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

    Cad_AppInit(interp);

    Tcl_SetVar(interp, "tcl_rcFileName", "~/tclshrc.tcl", TCL_GLOBAL_ONLY);
    return TCL_OK;
}


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
