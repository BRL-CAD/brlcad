/*                    B O M B A R D I E R . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @file util/bombardier.c
 *
 * BRL-CAD Crash Reporter - this program takes a given log file and,
 * through a gui or non-interactively, will send back the results of a
 * crash report to brlcad.org so the stack trace can be reviewed (and
 * hopefully prevented).
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "tcl.h"
#include "tk.h"

#include "tclcad.h"
#include "bu.h"

#include "./bombardier.h"


/* here be bombardier's gui in all its glory
 */
static void
init_crash_reporter(struct bu_vls *s)
{
    if (!s)
	return;

    bu_vls_printf(s, "\
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
\n");

    bu_vls_printf(s, "\
wm withdraw . \n\
update \n\
set msg \"The application $application quit rather unexpectedly.\n\nWe hate it when that happens at least as much as you do.  Please let us know about it so we can fix the problem.\" \n\
set response [tk_dialog .supg {Ooops...} $msg bombardier 1 {Close} {Report...}] \n\
if {$response == 0} { \n\
  destroy . \n\
  exit \n\
} \n\
tk appname \"BRL-CAD Crash Report\" \n\
wm title . \"BRL-CAD Crash Report\" \n\
\n");

    bu_vls_printf(s, "\
frame .f \n\
frame .f_top \n\
frame .f_bottom \n");

    bu_vls_printf(s, "\
label .l_top -text {Crash details and system information:} \n\
label .l_bottom -text {Please describe what you were doing when the crash occurred:} \n\
label .l_info -justify left -text {Your crash report will help BRL-CAD improve.  No other information is sent with this report other that what is shown.  You will not be contacted in response to this report unless you file a formal bug report to the project bug tracker.  See http://brlcad.org for details.} \n\
.l_info configure -wraplength 500 \n");

    bu_vls_printf(s, "\
text .t_top -width 80 -height 24 -borderwidth 1 -relief sunken -maxundo 0 -undo 1 \n\
text .t_bottom -width 80 -height 8 -borderwidth 1 -relief sunken -maxundo 0 -undo 1 \n\
scrollbar .s_top -orient vert \n\
scrollbar .s_bottom -orient vert \n\
.t_top conf -yscrollcommand {.s_top set} \n\
.s_top conf -command {.t_top yview} \n\
.t_bottom conf -yscrollcommand {.s_bottom set} \n\
.s_bottom conf -command {.t_bottom yview} \n\
button .button_yes -text {Send to BRL-CAD developers...} -command {sendlog; destroy .} \n\
\n");

    bu_vls_printf(s, "\
pack .t_top -in .f_top -side left -expand 1 -fill both \n\
pack .s_top -in .f_top -side right -fill y \n\
pack .l_top -in .f -anchor w \n\
pack .f_top -in .f -pady {0 10} -expand 1 -fill both \n\
pack .t_bottom -in .f_bottom -side left -expand 1 -fill both \n\
pack .s_bottom -in .f_bottom -side right -fill y \n\
pack .l_bottom -in .f -anchor w \n\
pack .f_bottom -in .f -pady {0 10} -fill both \n\
pack .l_info -in .f -fill x \n\
pack .button_yes -in .f -pady 10 -side right \n\
pack .f -padx 16 -pady 16 -expand 1 -fill both \n\
\n");

    bu_vls_printf(s, "\
update \n\
.t_top insert end $report \n\
\n\
wm deiconify . \n\
update \n\
wm minsize . [lindex [split [wm geometry .] x+] 0] [lindex [split [wm geometry .] x+] 1] \n\
.l_info configure -wraplength [expr [lindex [split [wm geometry .] x+] 0] - 20] \n\
wm state . normal \n");
}


/* crash report file(s) specified on the command line are stored in
 * here and released when the application terminates via an atexit
 * handler.
 */
static struct bu_vls *report = NULL;


/* used as an atexit handler to release our report memory since
 * Tk_Main never returns
 */
static void
_free_report_memory()
{
    /* release our report string */
    if (report) {
	bu_vls_free(report);
	bu_free(report, "report");
    }
}


/* loads the specified file into our report buffer
 */
static void
load_file(const char *filename)
{
    FILE *fp = NULL;
    struct bu_vls buffer;

    if (!bu_file_exists(filename)) {
	return;
    }

    fp = fopen(filename, "r");
    if (!fp) {
	perror("unable to open file");
	bu_log("ERROR: unable to open log file [%s]\n", filename);
	return;
    }

    bu_vls_init(&buffer);
    if (!report) {
	BU_GETSTRUCT(report, bu_vls);
	bu_vls_init(report);
	atexit(_free_report_memory);
    }

    /* read in the file */
    while (bu_vls_gets(&buffer, fp) != -1) {
	/* total bleh, but it does the job */
	bu_vls_printf(report, "%s\n", bu_vls_addr(&buffer));
	bu_vls_trunc(&buffer, 0);
    }

    bu_vls_free(&buffer);

    (void)fclose(fp);
    return;
}


/* the workhorse that actually sends the report in
 */
static int
sendlog(ClientData data, Tcl_Interp *UNUSED(interp), int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct bu_vls *daBomb = (struct bu_vls *)data;

    bu_log("Pretending that we're sending the log file to brlcad.org\n");
    bu_log("REPORT:\n%V\n", daBomb);

    return TCL_OK;
}


/* main app initialization where Tcl/Tk is initialized, the gui is
 * created, and displayed.
 */
static int
init(Tcl_Interp *interp)
{
    char *c;
    struct bu_vls appname;
    struct bu_vls crash_reporter;

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

    Tk_DefineBitmap(interp, "bombardier", (const char *)bomb_icon_bits, bomb_icon_width, bomb_icon_height);

    if (!report) {
	bu_log("Initialization error, report is NULL\n");
	return TCL_ERROR;
    }

    bu_vls_init(&appname);

    /* try to pull the command name from the report */
    c = bu_vls_addr(report);
    while (c[0] != '\0') {
	if (strncmp(c, "Command:", 8) == 0) {
	    c+=8;
	    while (c[0] != '\0' && isspace(c[0])) {
		c++;
	    }
	    while (c[0] != '\0' && !isspace(c[0])) {
		bu_vls_putc(&appname, c[0]);
		c++;
	    }
	    break;
	}
	c++;
    }

    if (bu_vls_addr(&appname)[0] != '\0') {
	Tcl_SetVar(interp, "application", bu_vls_addr(&appname), 0);
    } else {
	Tcl_SetVar(interp, "application", "[UNKNOWN]", 0);
    }
    bu_vls_free(&appname);

    if (bu_vls_addr(report)[0] != '\0') {
	Tcl_SetVar(interp, "report", bu_vls_addr(report), 0);
    } else {
	Tcl_SetVar(interp, "report", "ERROR", 0);
    }

    /* FIXME: why are we stashing this in a variable? */
    bu_vls_init(&crash_reporter);
    init_crash_reporter(&crash_reporter);
    Tcl_SetVar(interp, "script", bu_vls_addr(&crash_reporter), 0);
    bu_vls_free(&crash_reporter);

    Tcl_CreateCommand(interp, "sendlog", sendlog, (ClientData)report, NULL);

    if (Tcl_Eval(interp, bu_vls_addr(&crash_reporter)) != TCL_OK) {
	bu_log("ERROR: Unable to initialize\n");
	return TCL_ERROR;
    }

    return TCL_OK;
}


int
main(int argc, char *argv[])
{
    int tkargc = 1;
    char *tkargv[2] = {NULL, NULL};
    tkargv[0] = argv[0];

    if (argc <= 1) {
	bu_exit(1, "Usage: %s logfile(s)\n", argv[0]);
    }

    /* load all file arguments into our buffer */
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

    /* no tcl shell prompt */
    fclose(stdout);

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
