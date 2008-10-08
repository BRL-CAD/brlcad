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
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "tcl.h"
#include "tk.h"

#include "tclcad.h"
#include "bu.h"


static char *crash_reporter="\
catch {console hide} meh \n\
\n\
update \n\
event add <<Cut>> <Command-Key-x> <Control-Key-x> \n\
event add <<Copy>> <Command-Key-c> <Control-Key-c> \n\
event add <<Paste>> <Command-Key-v> <Control-Key-v> \n\
event add <<PasteSelection>> <ButtonRelease-2> \n\
event add <<Clear>> <Clear> \n\
event add <<Undo>> <Command-Key-z> <Control-Key-z> \n\
event add <<Redo>> <Command-Key-y> <Control-Key-y> <Control-Key-Z> \n\
bind . <Command-Key-q> {destroy .} \n\
bind . <Control-Key-q> {destroy .} \n\
bind . <Command-Key-w> {destroy .} \n\
\n\
wm withdraw . \n\
update \n\
tk appname \"BRL-CAD Crash Report\" \n\
wm title . \"BRL-CAD Crash Report\" \n\
\n\
frame .f \n\
frame .f_top \n\
frame .f_bottom \n\
label .l_top -text {Crash details and system information:} \n\
label .l_bottom -text {Please describe what you were doing when the problem happened:} \n\
label .l_info -justify left -text {Your crash report will help BRL-CAD improve.  No other information is sent with this report other that what is shown.  You will not be contacted back in response to this report unless you file a formal bug report.} \n\
.l_info configure -wraplength 500 \n\
text .t_top -width 80 -height 24 -borderwidth 1 -relief sunken -maxundo 0 -undo 1 \n\
text .t_bottom -width 80 -height 8 -borderwidth 1 -relief sunken -maxundo 0 -undo 1 \n\
scrollbar .s_top -orient vert \n\
scrollbar .s_bottom -orient vert \n\
.t_top conf -yscrollcommand {.s_top set} \n\
.s_top conf -command {.t_top yview} \n\
.t_bottom conf -yscrollcommand {.s_bottom set} \n\
.s_bottom conf -command {.t_bottom yview} \n\
button .b -text {Send to BRL-CAD developers...} -command {destroy .} \n\
\n\
pack .t_top -in .f_top -side left -expand 1 -fill both \n\
pack .s_top -in .f_top -side right -fill y \n\
pack .l_top -in .f -anchor w \n\
pack .f_top -in .f -pady {0 10} -expand 1 -fill both \n\
pack .t_bottom -in .f_bottom -side left -expand 1 -fill both \n\
pack .s_bottom -in .f_bottom -side right -fill y \n\
pack .l_bottom -in .f -anchor w \n\
pack .f_bottom -in .f -pady {0 10} -fill both \n\
pack .l_info -in .f -fill x \n\
pack .b -in .f -pady 10 -side right \n\
pack .f -padx 16 -pady 16 -expand 1 -fill both \n\
\n\
update \n\
.t_top insert end $report \n\
\n\
wm deiconify . \n\
update \n\
wm minsize . [lindex [split [wm geometry .] x+] 0] [lindex [split [wm geometry .] x+] 1] \n\
.l_info configure -wraplength [expr [lindex [split [wm geometry .] x+] 0] - 20] \n\
wm state . normal \n\
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
	/* total bleh, but it does the job */
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
	bu_log("Tcl_Init error %s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    if (Tk_Init(interp) == TCL_ERROR) {
	bu_log("Tk_Init error %s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    Tcl_SetVar(interp, "report", report, 0);
    Tcl_SetVar(interp, "script", crash_reporter, 0);

    if (Tcl_Eval(interp, crash_reporter) != TCL_OK) {
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
	bu_exit(1, "Usage: %s logfile(s)\n", argv[0]);
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

    /* bu_log("REPORT IS:\n%s", report); */

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
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
