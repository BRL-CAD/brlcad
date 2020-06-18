/*                       A R C H E R  . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2020 United States Government as represented by
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

#include "common.h"

#include <string.h>

#include "bresource.h"
#include "bnetwork.h"
#include "bio.h"

#include "tcl.h"
#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "bu.h"
#include "tclcad.h"


#ifdef HAVE_WINDOWS_H
int APIENTRY
WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpszCmdLine,
	int nCmdShow)
{
    int argc = __argc;
    char **argv = __argv;
#else
int
main(int argc, const char **argv)
{
#endif

    int status = 0;

#if !defined(HAVE_TK)
    bu_exit(EXIT_FAILURE, "Error: Archer requires Tk.");
#else

    const char *archer_tcl = NULL;
    const char *fullname = NULL;
    struct bu_vls tlog = BU_VLS_INIT_ZERO;
    struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;
    const char *result;
    Tcl_DString temp;
    Tcl_Interp *interp = Tcl_CreateInterp();

    /* initialize progname for run-tim resource finding */
    bu_setprogname(argv[0]);
    bu_vls_sprintf(&tcl_cmd, "set argv0 %s", argv[0]);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    bu_vls_sprintf(&tcl_cmd, "set ::no_bwish 1");
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

    /* Pass on argc/argv - for now, handling all that in Tcl/Tk land */
    argc--; argv++;
    tclcad_set_argv(interp, argc, argv);

#ifdef HAVE_WINDOWS_H
    Tk_InitConsoleChannels(interp);
#endif

    status = tclcad_init(interp, 1, &tlog);

    if (status == TCL_ERROR) {
	/* If we had an init failure this may not work, but we may be launching
	 * from an icon so go ahead and try. */
	bu_vls_sprintf(&tcl_cmd, "tk_messageBox -message \"Error - tclcad init failure: %s\" -type ok", bu_vls_addr(&tlog));
	if (Tcl_Eval(interp, bu_vls_addr(&tcl_cmd)) == TCL_ERROR) {
	    /* go for bu_log as a last resort */
	    bu_log("archer tclcad init failure:\n%s\n", bu_vls_addr(&tlog));
	}
	bu_vls_free(&tlog);
	bu_vls_free(&tcl_cmd);
	bu_exit(1, NULL);
    }
    bu_vls_free(&tlog);

    archer_tcl = bu_brlcad_root("share/tclscripts/archer/archer_launch.tcl", 1);
    Tcl_DStringInit(&temp);
    fullname = Tcl_TranslateFileName(interp, archer_tcl, &temp);
    status = Tcl_EvalFile(interp, fullname);
    Tcl_DStringFree(&temp);

    result = Tcl_GetStringResult(interp);
    if (strlen(result) > 0 && status == TCL_ERROR) {
	bu_log("%s\n", result);
    }
    Tcl_DeleteInterp(interp);

#endif /* HAVE_TK */

    return status;
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
