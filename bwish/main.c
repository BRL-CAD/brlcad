/*
 *				M A I N . C
 *
 *  This file provides the main() function for both BWISH and BTCLSH.
 *  While initializing Tcl, Itcl and various BRLCAD libraries it sets
 *  things up to provide command history and command line editing.
 *
 *  Author -
 *	  Robert G. Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 *
 */

#include "conf.h"
#ifdef BWISH
#include "itk.h"
#else
#include "itcl.h"
#include "tclInt.h"
#endif

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#ifdef BWISH
#include "dm.h"
#endif

extern int cmdInit();
extern void Cad_Main();
extern void Tk_CreateCanvasBezierType();

static int Cad_AppInit();
#ifdef BWISH
Tk_Window tkwin;
#endif

Tcl_Interp *interp;

int
main(argc, argv)
     int argc;
     char **argv;
{
	/* Create the interpreter */
	interp = Tcl_CreateInterp();
	Cad_Main(argc, argv, Cad_AppInit, interp);

	return 0;
}

static int
Cad_AppInit(interp)
     Tcl_Interp *interp;
{
	struct bu_vls vls;
	char *pathname;

	/* Initialize Tcl */
	if (Tcl_Init(interp) == TCL_ERROR) {
		bu_log("Tcl_Init error %s\n", interp->result);
		return TCL_ERROR;
	}

#ifdef BWISH
	/* Initialize Tk */
	if (Tk_Init(interp) == TCL_ERROR) {
		bu_log("Tk_Init error %s\n", interp->result);
		return TCL_ERROR;
	} 
	Tcl_StaticPackage(interp, "Tk", Tk_Init, Tk_SafeInit);
#endif

	/* Initialize [incr Tcl] */
	if (Itcl_Init(interp) == TCL_ERROR) {
		bu_log("Itcl_Init error %s\n", interp->result);
		return TCL_ERROR;
	}
	Tcl_StaticPackage(interp, "Itcl", Itcl_Init, Itcl_SafeInit);

#ifdef BWISH
	/* Initialize [incr Tk] */
	if (Itk_Init(interp) == TCL_ERROR) {
		bu_log("Itk_Init error %s\n", interp->result);
		return TCL_ERROR;
	}
	Tcl_StaticPackage(interp, "Itk", Itk_Init, (Tcl_PackageInitProc *) NULL);
#endif

	/* Import [incr Tcl] commands into the global namespace. */
	if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		       "::itcl::*", /* allowOverwrite */ 1) != TCL_OK) {
		bu_log("Tcl_Import error %s\n", interp->result);
		return TCL_ERROR;
	}

#ifdef BWISH
	/* Import [incr Tk] commands into the global namespace */
	if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		       "::itk::*", /* allowOverwrite */ 1) != TCL_OK) {
		bu_log("Tcl_Import error %s\n", interp->result);
		return TCL_ERROR;
	}

	/* Initialize the Iwidgets package */
	if (Tcl_Eval(interp, "package require Iwidgets") != TCL_OK) {
		bu_log("Tcl_Eval error %s\n", interp->result);
		return TCL_ERROR;
	}

	/* Import iwidgets into the global namespace */
	if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		       "::iwidgets::*", /* allowOverwrite */ 1) != TCL_OK) {
		bu_log("Tcl_Import error %s\n", interp->result);
		return TCL_ERROR;
	}
#endif

	if (Tcl_Eval(interp, "auto_mkindex_parser::slavehook { _%@namespace import -force ::itcl::* }") != TCL_OK) {
	  return TCL_ERROR;
	}
	if (Tcl_Eval(interp, "auto_mkindex_parser::slavehook { _%@namespace import -force ::tk::* }") != TCL_OK) {
	  return TCL_ERROR;
	}
	if (Tcl_Eval(interp, "auto_mkindex_parser::slavehook { _%@namespace import -force ::itk::* }") != TCL_OK) {
	  return TCL_ERROR;
	}

#ifdef BWISH
	/* Initialize libdm */
	if (Dm_Init(interp) == TCL_ERROR) {
		bu_log("Dm_Init error %s\n", interp->result);
		return TCL_ERROR;
	}
	Tcl_StaticPackage(interp, "Dm", Dm_Init, (Tcl_PackageInitProc *) NULL);

	/* Initialize libfb */
	if (Fb_Init(interp) == TCL_ERROR) {
		bu_log("Fb_Init error %s\n", interp->result);
		return TCL_ERROR;
	}
	Tcl_StaticPackage(interp, "Fb", Fb_Init, (Tcl_PackageInitProc *) NULL);
#endif

	/* Initialize libbu */
	if (Bu_Init(interp) == TCL_ERROR) {
		bu_log("Bu_Init error %s\n", interp->result);
		return TCL_ERROR;
	}

	/* Initialize libbn */
	if (Bn_Init(interp) == TCL_ERROR) {
		bu_log("Bn_Init error %s\n", interp->result);
		return TCL_ERROR;
	}

	/* Initialize librt */
	if (Rt_Init(interp) == TCL_ERROR) {
		bu_log("Rt_Init error %s\n", interp->result);
		return TCL_ERROR;
	}
	Tcl_StaticPackage(interp, "Rt", Rt_Init, (Tcl_PackageInitProc *) NULL);

#ifdef BWISH
	if ((tkwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;

	/* Add Bezier Curves to the canvas widget */
	Tk_CreateCanvasBezierType();
#endif

	/* Locate the BRL-CAD-specific Tcl scripts */
	pathname = bu_brlcad_path("");

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "lappend auto_path %stclscripts %stclscripts/lib %stclscripts/util",
		      pathname, pathname, pathname);
	(void)Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	/* register bwish/btclsh commands */
	cmdInit(interp);

	return TCL_OK;
}
