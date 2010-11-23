/*                          M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2010 United States Government as represented by
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
/** @file main.c
 *
 *  This file provides the main() function for both BWISH and BTCLSH.
 *  While initializing Tcl, Itcl and various BRL-CAD libraries it sets
 *  things up to provide command history and command line editing.
 *
 */

#include "common.h"

#include "tcl.h"

#ifdef BWISH
#  include "tk.h"
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
#define IMPORT_ITK	1
#define IMPORT_IWIDGETS	1
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
    /* Go ahead and add the BRL-CAD paths */
    tclcad_auto_path(interp);
#ifdef BWISH
    int init_tk = 1;
    int init_itk = 1;
#endif
    if (Tcl_Init(interp) == TCL_ERROR) {
      return TCL_ERROR;
    }

#ifdef BWISH
    if (Tk_Init(interp) == TCL_ERROR) {
      return TCL_ERROR;
    }
#endif


    /* Initialize Itcl */
    if (Tcl_Eval(interp, "package require Itcl") != TCL_OK) {
	bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

#ifdef BWISH
    /* Initialize Itk */
    if (Tcl_Eval(interp, "package require Itk") != TCL_OK) {
	bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }
#endif  /* BWISH */

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
	bu_log("Ged_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
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
    struct bu_vls str;
    bu_vls_init(&str);
    bu_vls_printf(&str, "lappend auto_path {%s}", TCL_SYSTEM_AUTO_PATH);
    /* Create the interpreter */
    INTERP = Tcl_CreateInterp();
    Tcl_FindExecutable(argv[0]);
    if(bu_vls_strlen(&str) > 20) {
	    Tcl_Eval(INTERP, bu_vls_addr(&str));
	    bu_log("Tcl_Eval Result:\n%s\n", Tcl_GetStringResult(INTERP));
    }
    bu_vls_free(&str);
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
