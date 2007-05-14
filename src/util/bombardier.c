/*                    B O M B A R D I E R . C
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file bombardier.c
 *
 * BRL-CAD Crash Reporter - this program takes a given log file and,
 * through a gui or non-interactively, will send back the results of a
 * crash report to brlcad.org so the stack trace can be reviewed (and
 * hopefully prevented).
 *
 * Author -
 *   Christopher Sean Morrison
 *
 * Source -
 *   BRL-CAD Open Source
 */

#include <stdlib.h>
#include <stdio.h>

#include "tcl.h"
#include "tk.h"

static char *crash_reporter="\
wm withdraw . \n\
tk appname \"BRL-CAD Crash Reporter\" \n\
pack [button .b -text Hello] \n\
wm deiconify . \n\
wm state . \n\
";

#define MAX_BUFLEN 4096
#define MAX_REPORT 1024 * 1024
static char report[MAX_REPORT] = {0};
static void
load_file(const char *filename)
{
    FILE *fp = NULL;
    char buffer[MAX_BUFLEN] = {0};

    if (!bu_file_exists(filename)) {
	return;
    }

    fp = fopen(filename, "r");
    if (!fp) {
	perror("unable to open file");
	bu_log("ERROR: unable to open log file [%s]\n", filename);
	return;
    }

    /* read in the file */
    while (bu_fgets(buffer, MAX_BUFLEN, fp)) {
	/* bleh, but it works */
	snprintf(report, MAX_REPORT, "%s%s", report, buffer);
    }

    (void)fclose(fp);
    return;
}


static int
init(Tcl_Interp *interp)
{
    /* locate brl-cad specific scripts (or uninstalled tcl/tk stuff) */
    tclcad_auto_path(interp);

    if (Tcl_Init(interp) == TCL_ERROR) {
	bu_log("Tcl_Init error %s\n", interp->result);
	return TCL_ERROR;
    }

    if (Tk_Init(interp) == TCL_ERROR) {
	bu_log("Tk_Init error %s\n", interp->result);
	return TCL_ERROR;
    }

    if (Tcl_Eval(interp, crash_reporter) != TCL_OK ) {
	bu_log("ERROR: Unable to initialize\n");
	return TCL_ERROR;
    }

    return TCL_OK;
}


int
main(int argc, char *argv[])
{
    int i;
    int tkargc = 1;
    char *tkargv[2] = {NULL, NULL};
    tkargv[0] = argv[0];

    if (argc <= 1) {
	bu_log("Usage: %s logfile(s)\n", argv[0]);
	exit(1);
    }

    while (argc > 1) {
	if (!bu_file_exists(argv[1])) {
	    bu_log("WARNING: Log file [%s] does not exist\n", argv[1]);
	} else {
	    bu_log("Processing %s\n", argv[1]);
	    load_file(argv[1]);
	}
	argc--;
	argv++;
    }

    bu_log("REPORT IS:\n%s", report);

    /* no tcl shell prompt */
    close(0);

    /* let the fun begin */
    Tk_Main(tkargc, tkargv, init);

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
