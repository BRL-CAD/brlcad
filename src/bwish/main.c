/*                          M A I N . C
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
/** @file bwish/main.c
 *
 * This file provides the main() function for both BWISH and BTCLSH.
 * While initializing Tcl, Itcl and various BRL-CAD libraries it sets
 * things up to provide command history and command line editing.
 *
 */

#include "common.h"

#include "tcl.h"

#ifdef BWISH
#  include "itk.h"
#else
#  include "itcl.h"
#endif

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "ged.h"
#ifdef BWISH
#  include "dm.h"
#endif
#include "tclcad.h"


/* NOTE: it's probably a bad idea to import itcl/itk/iwidgets into
 * the global namespace..  allow for easy means to disable the import.
 */
#define IMPORT_ITCL	1
#ifdef BWISH
#  define IMPORT_ITK	1
#  define IMPORT_IWIDGETS	1
#endif

extern int cmdInit(Tcl_Interp *interp);
extern void Cad_Main(int argc, char **argv, Tcl_AppInitProc (*appInitProc), Tcl_Interp *interp);

#ifdef BWISH
Tk_Window tkwin;
#endif

Tcl_Interp *INTERP;


static int
Cad_AppInit(Tcl_Interp *interp)
{
    int try_auto_path = 0;

    int init_tcl = 1;
    int init_itcl = 1;
#ifdef BWISH
    int init_tk = 1;
    int init_itk = 1;
#endif

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
		try_auto_path=1;
		continue;
	    }
	    bu_log("Tcl_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    return TCL_ERROR;
	}
	init_tcl=0;

#ifdef BWISH
	/* Initialize Tk */
	Tcl_ResetResult(interp);
	if (init_tk && Tk_Init(interp) == TCL_ERROR) {
	    if (!try_auto_path) {
		try_auto_path=1;
		continue;
	    }
	    bu_log("Tk_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    return TCL_ERROR;
	}
	Tcl_StaticPackage(interp, "Tk", Tk_Init, Tk_SafeInit);
	init_tk=0;
#endif

	/* Initialize [incr Tcl] */
	Tcl_ResetResult(interp);
	/* NOTE: Calling "package require Itcl" here is apparently
	 * insufficient without other changes elsewhere.  The
	 * Combination Editor in mged fails with an iwidgets class
	 * already loaded error if we don't perform Itcl_Init() here.
	 */
	if (init_itcl && Itcl_Init(interp) == TCL_ERROR) {
	    if (!try_auto_path) {
		Tcl_Namespace *nsp;

		try_auto_path=1;
		/* Itcl_Init() leaves initialization in a bad state
		 * and can cause retry failures.  cleanup manually.
		 */
		Tcl_DeleteCommand(interp, "::itcl::class");
		nsp = Tcl_FindNamespace(interp, "::itcl", NULL, 0);
		if(nsp != NULL)
		    Tcl_DeleteNamespace(nsp);
		continue;
	    }
	    bu_log("Itcl_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    return TCL_ERROR;
	}
	init_itcl=0;

#ifdef BWISH
	/* Initialize [incr Tk] */
	Tcl_ResetResult(interp);
	if (init_itk && Itk_Init(interp) == TCL_ERROR) {
	    if (!try_auto_path) {
		try_auto_path=1;
		continue;
	    }
	    bu_log("Itk_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    return TCL_ERROR;
	}
	init_itk=0;
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

#ifdef IMPORT_ITCL
    /* Import [incr Tcl] commands into the global namespace. */
    if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		   "::itcl::*", /* allowOverwrite */ 1) != TCL_OK) {
	bu_log("Tcl_Import ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }
#endif /* IMPORT_ITCL */

#ifdef BWISH

#  ifdef IMPORT_ITK
    /* Import [incr Tk] commands into the global namespace */
    if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		   "::itk::*", /* allowOverwrite */ 1) != TCL_OK) {
	bu_log("Tcl_Import ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }
#  endif  /* IMPORT_ITK */

    /* Initialize the Iwidgets package */
    if (Tcl_Eval(interp, "package require Iwidgets") != TCL_OK) {
	bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

#  ifdef IMPORT_IWIDGETS
    /* Import iwidgets into the global namespace */
    if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		   "::iwidgets::*", /* allowOverwrite */ 1) != TCL_OK) {
	bu_log("Tcl_Import ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }
#  endif  /* IMPORT_IWIDGETS */

#endif  /* BWISH */

#  ifdef IMPORT_ITCL
    if (Tcl_Eval(interp, "auto_mkindex_parser::slavehook { _%@namespace import -force ::itcl::* }") != TCL_OK) {
	bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }
#  endif

#ifdef BWISH
#  ifdef IMPORT_ITCL
    if (Tcl_Eval(interp, "auto_mkindex_parser::slavehook { _%@namespace import -force ::tk::* }") != TCL_OK) {
	bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }
#  endif
#  ifdef IMPORT_ITK
    if (Tcl_Eval(interp, "auto_mkindex_parser::slavehook { _%@namespace import -force ::itk::* }") != TCL_OK) {
	bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }
#  endif

    /* Initialize libdm */
    if (Dm_Init(interp) == TCL_ERROR) {
	bu_log("Dm_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "Dm", Dm_Init, (Tcl_PackageInitProc *) NULL);

    /* Initialize libfb */
    if (Fb_Init(interp) == TCL_ERROR) {
	bu_log("Fb_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "Fb", Fb_Init, (Tcl_PackageInitProc *) NULL);

#endif

    /* Initialize libbu */
    if (Bu_Init(interp) == TCL_ERROR) {
	bu_log("Bu_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    /* Initialize libbn */
    if (Bn_Init(interp) == TCL_ERROR) {
	bu_log("Bn_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    /* Initialize librt */
    if (Rt_Init(interp) == TCL_ERROR) {
	bu_log("Rt_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "Rt", Rt_Init, (Tcl_PackageInitProc *) NULL);

    /* Initialize libtclcad's GED Object */
    if (Go_Init(interp) == TCL_ERROR) {
	bu_log("Go_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

#ifdef BWISH
    if ((tkwin = Tk_MainWindow(interp)) == NULL)
	return TCL_ERROR;
#endif

    /* register bwish/btclsh commands */
    cmdInit(interp);

    return TCL_OK;
}


int
main(int argc, char **argv)
{
    /* Create the interpreter */
    INTERP = Tcl_CreateInterp();
    Tcl_FindExecutable(argv[0]);
    Cad_Main(argc, argv, Cad_AppInit, INTERP);

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
