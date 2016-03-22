/*                   T C L C A D _ I N I T . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2016 United States Government as represented by
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
/** @file tclcad_init.c
 *
 * Functions for initializing Tcl environments.
 *
 */


#include "common.h"
#include <tcl.h>
#ifdef HAVE_TK
#  include <tk.h>
#endif
#include "tclcad.h"

/* avoid including itcl.h/itk.h due to their usage of internal headers */
extern int Itcl_Init(Tcl_Interp *);
#ifdef HAVE_TK
extern int Itk_Init(Tcl_Interp *);
#endif

int
tclcad_init(Tcl_Interp *interp, int flags, struct bu_vls *tlog)
{
    int try_auto_path = 0;
    int init_tcl = 1;
    int init_itcl = 1;
#ifdef HAVE_TK
    int init_tk = 1;
    int init_itk = 1;
#endif
    int ret = TCL_OK;

    if (!interp) return TCL_ERROR;

    /* a two-pass init loop.  the first pass just tries default init
     * routines while the second calls tclcad_auto_path() to help it
     * find other, potentially uninstalled, resources.
     */
    while (1) {

	/* not called first time through, give Tcl_Init() a chance */
	if (try_auto_path) {
	    /* Locate the BRL-CAD-specific Tcl scripts, set the auto_path */
	    tclcad_auto_path(interp);
	}

	/* Initialize Tcl */
	Tcl_ResetResult(interp);
	if (init_tcl && Tcl_Init(interp) == TCL_ERROR) {
	    if (!try_auto_path) {
		try_auto_path = 1;
		continue;
	    }
	    if (tlog) bu_vls_printf(tlog, "Tcl_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    ret = TCL_ERROR;
	    break;
	}
	init_tcl = 0;

#ifdef HAVE_TK
	if (flags & TCLCAD_GUI_INIT) {
	    /* Initialize Tk */
	    Tcl_ResetResult(interp);
	    if (init_tk && Tk_Init(interp) == TCL_ERROR) {
		if (!try_auto_path) {
		    try_auto_path=1;
		    continue;
		}
		if (tlog) bu_vls_printf(tlog, "Tk_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
		ret = TCL_ERROR;
		break;
	    }
	    Tcl_StaticPackage(interp, "Tk", Tk_Init, Tk_SafeInit);
	    init_tk=0;
	}
#endif

	/* Initialize [incr Tcl] */
	Tcl_ResetResult(interp);
	/* NOTE: Calling "package require Itcl" here is apparently
	 * insufficient without other changes elsewhere (per MGED comment,
	 * but package require should work - why doesn't it??).
	 */
	if (init_itcl && Itcl_Init(interp) == TCL_ERROR) {
	    if (!try_auto_path) {
		Tcl_Namespace *nsp;

		try_auto_path = 1;
		/* Itcl_Init() leaves initialization in a bad state
		 * and can cause retry failures.  cleanup manually.
		 */
		Tcl_DeleteCommand(interp, "::itcl::class");
		nsp = Tcl_FindNamespace(interp, "::itcl", NULL, 0);
		if (nsp)
		    Tcl_DeleteNamespace(nsp);
		continue;
	    }
	    if (tlog) bu_vls_printf(tlog, "Itcl_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    ret = TCL_ERROR;
	    break;
	}
	init_itcl = 0;

#ifdef HAVE_TK
	if (flags & TCLCAD_GUI_INIT) {
	    /* Initialize [incr Tk] */
	    Tcl_ResetResult(interp);
	    if (init_itk && Itk_Init(interp) == TCL_ERROR) {
		if (!try_auto_path) {
		    try_auto_path=1;
		    continue;
		}
		if (tlog) bu_vls_printf(tlog, "Itk_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
		ret = TCL_ERROR;
		break;
	    }
	    init_itk=0;
	}
#endif

	/* don't actually want to loop forever */
	break;

    } /* end iteration over Init() routines that need auto_path */
    Tcl_ResetResult(interp);

    /* if we haven't loaded by now, load auto_path so we find our tclscripts */
    if (!try_auto_path) {
	/* Locate the BRL-CAD-specific Tcl scripts */
	tclcad_auto_path(interp);
    }

    /* Initialize libbu */
    if (Bu_Init(interp) == TCL_ERROR) {
	if (tlog) bu_vls_printf(tlog, "Bu_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	ret = TCL_ERROR;
	Tcl_ResetResult(interp);
    }

    /* Initialize libbn */
    if (Bn_Init(interp) == TCL_ERROR) {
	if (tlog) bu_vls_printf(tlog, "Bn_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	ret = TCL_ERROR;
	Tcl_ResetResult(interp);
    }

    /* Initialize librt */
    if (Rt_Init(interp) == TCL_ERROR) {
	if (tlog) bu_vls_printf(tlog, "Rt_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	ret = TCL_ERROR;
	Tcl_ResetResult(interp);
    }
    Tcl_StaticPackage(interp, "Rt", Rt_Init, (Tcl_PackageInitProc *) NULL);

    if (flags & TCLCAD_DM_INIT) {
	/* Initialize libdm */
	if (Dm_Init(interp) == TCL_ERROR) {
	    if (tlog) bu_vls_printf(tlog, "Dm_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    ret = TCL_ERROR;
	    Tcl_ResetResult(interp);
	}
	Tcl_StaticPackage(interp, "Dm", (int (*)(struct Tcl_Interp *))Dm_Init, (Tcl_PackageInitProc *) NULL);

	/* Initialize libfb */
	if (Fb_Init(interp) == TCL_ERROR) {
	    if (tlog) bu_vls_printf(tlog, "Fb_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    ret = TCL_ERROR;
	    Tcl_ResetResult(interp);
	}
	Tcl_StaticPackage(interp, "Fb", Fb_Init, (Tcl_PackageInitProc *) NULL);
    }

    /* Initialize libged */
    if (Go_Init(interp) == TCL_ERROR) {
	if (tlog) bu_vls_printf(tlog, "Ged_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	ret = TCL_ERROR;
	Tcl_ResetResult(interp);
    }

    /* Initialize history */
    if (flags & TCLCAD_HIST_INIT) {
	Cho_Init(interp);
    }

#ifdef HAVE_TK
    /* If we're doing Tk, make sure we have a window */
    if (flags & TCLCAD_GUI_INIT) {
	Tk_Window tkwin = Tk_MainWindow(interp);
	if (!tkwin) ret = TCL_ERROR;
    }
#endif

    return ret;
}

void
tclcad_set_argv(Tcl_Interp *interp, int argc, const char *argv)
{
    char buf[TCL_INTEGER_SPACE] = {0};
    char *args;
    Tcl_DString argString;

    /* argc */
    sprintf(buf, "%ld", (long)(argc));
    Tcl_SetVar(interp, "argc", buf, TCL_GLOBAL_ONLY);

    /* argv */
    args = Tcl_Merge(argc-1, (const char * const *)argv);
    Tcl_ExternalToUtfDString(NULL, args, -1, &argString);
    Tcl_SetVar(interp, "argv", Tcl_DStringValue(&argString), TCL_GLOBAL_ONLY);
    Tcl_DStringFree(&argString);
    ckfree(args);
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
