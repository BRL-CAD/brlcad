/*                          U N I X M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file bwish/unixMain.c
 *
 * This file provides the main() function for
 * BWISH and BTCLSH on Unix platforms.
 *
 */
/*
 * This file originated from Sun Microsystems, Inc. and was
 * modified for use within BRL-CAD.
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "tk.h"
#include "locale.h"

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 * This is the main program for BWISH.
 *
 * Results:
 * None: Tk_Main never returns here, so this procedure never returns
 * either.
 *
 * Side effects:
 * Whatever the application does.
 *
 *----------------------------------------------------------------------
 */

int
main(int argc,
     char **argv)
{
    /*
     * The following #if block allows you to change the AppInit function by
     * using a #define of TCL_LOCAL_APPINIT instead of rewriting this entire
     * file. The #if checks for that #define and uses Tcl_AppInit if it
     * doesn't exist.
     */

#ifndef TK_LOCAL_APPINIT
#define TK_LOCAL_APPINIT Tcl_AppInit
#endif
    extern int TK_LOCAL_APPINIT _ANSI_ARGS_((Tcl_Interp *interp));

    /*
     * The following #if block allows you to change how Tcl finds the startup
     * script, prime the library or encoding paths, fiddle with the argv,
     * etc., without needing to rewrite Tk_Main()
     */

#ifdef TK_LOCAL_MAIN_HOOK
    extern int TK_LOCAL_MAIN_HOOK _ANSI_ARGS_((int *argc, char ***argv));
    TK_LOCAL_MAIN_HOOK(&argc, &argv);
#endif

#ifdef BWISH
    Tk_Main(argc, argv, TK_LOCAL_APPINIT);
#else
    Tcl_Main(argc, argv, TK_LOCAL_APPINIT);
#endif

    return 0;			/* Needed only to prevent compiler warning. */
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 * This procedure performs application-specific initialization. Most
 * applications, especially those that incorporate additional packages,
 * will have their own version of this procedure.
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
Tcl_AppInit(Tcl_Interp *interp)
{
    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

#ifdef BWISH
    if (Tk_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
#endif

    Cad_AppInit(interp);

    /*
     * Specify a user-specific startup file to invoke if the application is
     * run interactively. Typically the startup file is "~/.apprc" where "app"
     * is the name of the application. If this line is deleted then no user-
     * -specific startup file will be run under any conditions.
     */

#ifdef BWISH
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.bwishrc", TCL_GLOBAL_ONLY);
#else
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.btclshrc", TCL_GLOBAL_ONLY);
#endif

    return TCL_OK;
}


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
