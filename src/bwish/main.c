/*                          M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2016 United States Government as represented by
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
/** @file bwish/main.c
 *
 * This file provides the main() function for both BWISH and BTCLSH.
 * While initializing Tcl, Itcl and various BRL-CAD libraries it sets
 * things up to provide command history and command line editing.
 *
 */

#include "common.h"
#include <stdio.h>
#include <ctype.h>
#include "tcl.h"
#ifdef BWISH
#include "tk.h"
#endif

#include "libtermio.h"

#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h> /* for select */
#endif

#include "vmath.h"
#include "tclcad.h"

extern int cmdInit(Tcl_Interp *interp);

#ifdef BWISH
Tk_Window tkwin;
#endif

Tcl_Interp *INTERP;

/* We need to be careful about tty resetting - xcodebuild
 * and resetTty were locking up.  Add a tty check
 * along the lines of
 * http://stackoverflow.com/questions/1594251/how-to-check-if-stdin-is-still-opened-without-blocking
 */
#ifdef HAVE_SYS_SELECT_H
int tty_usable(int fd) {
   fd_set fdset;
   struct timeval timeout;
   FD_ZERO(&fdset);
   FD_SET(fd, &fdset);
   timeout.tv_sec = 0;
   timeout.tv_usec = 1;
   return select(fd+1, &fdset, NULL, NULL, &timeout) == 1 ? 1 : 0;
}
#endif

/* defined in input.c */
extern void initInput(void);

#ifdef BWISH
/* defined in libtk/(unix|win|mac)/tk(Unix|Win|Mac)Init.c */
void TkpDisplayWarning();
#endif

void Cad_Exit(int status);

#ifdef BWISH
#  define CAD_RCFILENAME "~/.bwishrc"
#else
#  define CAD_RCFILENAME "~/.btclshrc"
#endif

void
Cad_Exit(int status)
{
#ifdef HAVE_SYS_SELECT_H
    if (tty_usable(fileno(stdin))) {
      reset_Tty(fileno(stdin));
    }
#else
    reset_Tty(fileno(stdin));
#endif
    Tcl_Exit(status);
}


int
main(int argc, char **argv)
{
    struct bu_vls tlog = BU_VLS_INIT_ZERO;
    int status = TCL_OK;
    char *filename = NULL;
    char *args = NULL;
    char buf[TCL_INTEGER_SPACE] = {0};
    Tcl_DString argString;


    /* Create the interpreter */
    INTERP = Tcl_CreateInterp();
    Tcl_FindExecutable(argv[0]);

    bu_setprogname(argv[0]);

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
    Tcl_SetVar(INTERP, "argv", Tcl_DStringValue(&argString), TCL_GLOBAL_ONLY);
    Tcl_DStringFree(&argString);
    ckfree(args);

    if (filename == NULL) {
	(void)Tcl_ExternalToUtfDString(NULL, argv[0], -1, &argString);
    } else {
	filename = Tcl_ExternalToUtfDString(NULL, filename, -1, &argString);
    }

    sprintf(buf, "%ld", (long)(argc-1));
    Tcl_SetVar(INTERP, "argc", buf, TCL_GLOBAL_ONLY);
    Tcl_SetVar(INTERP, "argv0", Tcl_DStringValue(&argString), TCL_GLOBAL_ONLY);

#ifdef BWISH
    status = tclcad_init(INTERP, 1, &tlog);
#else
    status = tclcad_init(INTERP, 0, &tlog);
#endif
    if (status == TCL_ERROR) {
#ifdef BWISH
	struct bu_vls errstr = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&errstr, "Application initialization failed:\n%s\n", bu_vls_addr(&tlog));
	TkpDisplayWarning(bu_vls_addr(&errstr), "ERROR");
	bu_vls_free(&errstr);
#else
	bu_log("tclcad_init failure:\n%s\n", bu_vls_addr(&tlog));
#endif
    } else {
	/* register bwish/btclsh commands */
	cmdInit(INTERP);
    }
    bu_vls_free(&tlog);

    if (filename != NULL) {
	int fstatus;

	/* ??? need to arrange for a bu_log handler and or handlers
	 * for stdout/stderr?
	 */
#ifdef HAVE_SYS_SELECT_H
	if (tty_usable(fileno(stdin))) {
	  save_Tty(fileno(stdin));
	}
#else
	save_Tty(fileno(stdin));
#endif
	Tcl_ResetResult(INTERP);
	fstatus = Tcl_EvalFile(INTERP, filename);
	if (fstatus != TCL_OK) {
	    Tcl_AddErrorInfo(INTERP, "");
#ifdef BWISH
	    TkpDisplayWarning(Tcl_GetVar(INTERP, "errorInfo",
					 TCL_GLOBAL_ONLY), "Error in startup script");
#else
	    bu_log("Error in startup script: %s\n", Tcl_GetVar(INTERP, "errorInfo", TCL_GLOBAL_ONLY));
#endif
	}

#ifndef BWISH
	Cad_Exit(fstatus);
#endif
    } else {
	/* We're running interactively. */
	/* Set up to handle commands from user as well as
	   provide a command line editing capability. */
	initInput();

	/* Set the name of the startup file. */
	Tcl_SetVar(INTERP, "tcl_rcFileName", CAD_RCFILENAME, TCL_GLOBAL_ONLY);

	/* Source the startup file if it exists. */
	Tcl_SourceRCFile(INTERP);
    }

    Tcl_DStringFree(&argString);

#ifdef BWISH
    while (Tk_GetNumMainWindows() > 0) {
#else
    while (1) {
#endif
	Tcl_DoOneEvent(0);
    }

    Cad_Exit(TCL_OK);

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
