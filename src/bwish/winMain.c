/*                          W I N M A I N . C
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
/** @file bwish/winMain.c
 *
 * This file provides the WinMain() function for BWISH.
 *
 */
/*
 * This file originated from Sun Microsystems, Inc. and was
 * modified for use within BRL-CAD.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkInt.h"
#define WIN32_LEAN_AND_MEAN
#include "bio.h"
#undef WIN32_LEAN_AND_MEAN
#include <locale.h>


/*
 * Forward declarations for procedures defined later in this file:
 */

static void BwishPanic(const char *format, ...);

static BOOL consoleRequired = TRUE;

/*
 * The following #if block allows you to change the AppInit function by using
 * a #define of TCL_LOCAL_APPINIT instead of rewriting this entire file. The
 * #if checks for that #define and uses Tcl_AppInit if it doesn't exist.
 */

#ifndef TK_LOCAL_APPINIT
#define TK_LOCAL_APPINIT Tcl_AppInit
#endif
extern int TK_LOCAL_APPINIT(Tcl_Interp *interp);

/*
 * The following #if block allows you to change how Tcl finds the startup
 * script, prime the library or encoding paths, fiddle with the argv, etc.,
 * without needing to rewrite Tk_Main()
 */

#ifdef TK_LOCAL_MAIN_HOOK
extern int TK_LOCAL_MAIN_HOOK(int *argc, char ***argv);
#endif

extern int Cad_AppInit(Tcl_Interp *interp);

/*
 *----------------------------------------------------------------------
 *
 * WinMain --
 *
 * Main entry point from Windows.
 *
 * Results:
 * Returns false if initialization fails, otherwise it never returns.
 *
 * Side effects:
 * Just about anything, since from here we call arbitrary Tcl code.
 *
 *----------------------------------------------------------------------
 */

int APIENTRY
WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpszCmdLine,
	int nCmdShow)
{
    char **argv;
    int argc;
    char *p;

    Tcl_SetPanicProc(BwishPanic);

    /*
     * Create the console channels and install them as the standard channels.
     * All I/O will be discarded until Tk_CreateConsoleWindow is called to
     * attach the console to a text widget.
     */

    consoleRequired = TRUE;

    /*
     * Set up the default locale to be standard "C" locale so parsing is
     * performed correctly.
     */

    setlocale(LC_ALL, "C");

    /*
     * Get our args from the c-runtime. Ignore lpszCmdLine.
     */

    argc = __argc;
    argv = __argv;

    /*
     * Forward slashes substituted for backslashes.
     */

    for (p = argv[0]; *p != '\0'; p++) {
	if (*p == '\\') {
	    *p = '/';
	}
    }

#ifdef TK_LOCAL_MAIN_HOOK
    TK_LOCAL_MAIN_HOOK(&argc, &argv);
#endif

    Tk_Main(argc, argv, TK_LOCAL_APPINIT);
    return 1;
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
	goto error;
    }

    if (Tk_Init(interp) == TCL_ERROR) {
	goto error;
    }

    /*
     * Initialize the console only if we are running as an interactive
     * application.
     */
    if (consoleRequired) {
	if (Tk_CreateConsoleWindow(interp) == TCL_ERROR) {
	    goto error;
	}
    }

    Cad_AppInit(interp);

    Tcl_SetVar(interp, "tcl_rcFileName", "~/bwishrc.tcl", TCL_GLOBAL_ONLY);
    return TCL_OK;

error:
    MessageBeep(MB_ICONEXCLAMATION);
    MessageBox(NULL, (LPCWSTR)Tcl_GetStringResult(interp), (LPCWSTR)"Error in bwish",
	       MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
    ExitProcess(1);

    /*
     * We won't reach this, but we need the return.
     */

    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * BwishPanic --
 *
 * Display a message and exit.
 *
 * Results:
 * None.
 *
 * Side effects:
 * Exits the program.
 *
 *----------------------------------------------------------------------
 */

void
BwishPanic(const char *format, ...)
{
    va_list argList;
    char buf[1024];

    va_start(argList, format);
    vsnprintf(buf, 1024, format, argList);

    MessageBeep(MB_ICONEXCLAMATION);
    MessageBox(NULL, (LPCWSTR)buf, (LPCWSTR)"Fatal Error in bwish",
	       MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
#ifdef _MSC_VER
    DebugBreak();
#endif
    ExitProcess(1);
}


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
