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
 *	This software is Copyright (C) 1998-2004 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "common.h"


#ifdef BWISH
#include "itk.h"
#else
#include "itcl.h"
#include "tclInt.h"
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#ifdef BWISH
#include "dm.h"
#endif

/* XXX -- it's probably a bad idea to import itcl/itk/iwidgets into
 * the global namespace..  allow for easy means to disable the import.
 */
#define IMPORT_ITCL	1
#define IMPORT_ITK	1
#define IMPORT_IWIDGETS	1

extern int cmdInit(Tcl_Interp *interp);
extern void Cad_Main(int argc, char **argv, Tcl_AppInitProc (*appInitProc), Tcl_Interp *interp);
extern void Tk_CreateCanvasBezierType();

static int Cad_AppInit(Tcl_Interp *interp);
#ifdef BWISH
Tk_Window tkwin;
#endif

Tcl_Interp *interp;


int
main(int argc, char **argv)
{
	/* Create the interpreter */
	interp = Tcl_CreateInterp();
	Cad_Main(argc, argv, Cad_AppInit, interp);

	return 0;
}

static int
Cad_AppInit(Tcl_Interp *interp)
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

#ifdef IMPORT_TCL
	/* Import [incr Tcl] commands into the global namespace. */
	if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		       "::itcl::*", /* allowOverwrite */ 1) != TCL_OK) {
		bu_log("Tcl_Import error %s\n", interp->result);
		return TCL_ERROR;
	}
#endif

#ifdef BWISH

#  ifdef IMPORT_ITK
	/* Import [incr Tk] commands into the global namespace */
	if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		       "::itk::*", /* allowOverwrite */ 1) != TCL_OK) {
		bu_log("Tcl_Import error %s\n", interp->result);
		return TCL_ERROR;
	}
#  endif  /* IMPORT_ITK */

	/* Initialize the Iwidgets package */
	if (Tcl_Eval(interp, "package require Iwidgets") != TCL_OK) {
		bu_log("Tcl_Eval error %s\n", interp->result);
		return TCL_ERROR;
	}

#  ifdef IMPORT_IWIDGETS
	/* Import iwidgets into the global namespace */
	if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		       "::iwidgets::*", /* allowOverwrite */ 1) != TCL_OK) {
		bu_log("Tcl_Import error %s\n", interp->result);
		return TCL_ERROR;
	}
#  endif  /* IMPORT_IWIDGETS */

#endif  /* BWISH */

	if (Tcl_Eval(interp, "auto_mkindex_parser::slavehook { _%@namespace import -force ::itcl::* }") != TCL_OK) {
	  return TCL_ERROR;
	}

#ifdef BWISH
	if (Tcl_Eval(interp, "auto_mkindex_parser::slavehook { _%@namespace import -force ::tk::* }") != TCL_OK) {
	  return TCL_ERROR;
	}
	if (Tcl_Eval(interp, "auto_mkindex_parser::slavehook { _%@namespace import -force ::itk::* }") != TCL_OK) {
	  return TCL_ERROR;
	}

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
	pathname = bu_brlcad_path("", 1);

	bu_vls_init(&vls);
	if (pathname) {
	  bu_vls_printf(&vls, "lappend auto_path %stclscripts %stclscripts/lib %stclscripts/util",
			pathname, pathname, pathname);
	  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
	} else {
	  /* hunt for the tclscripts a little since we're probably just not installed yet */
	  (void)Tcl_Eval(interp, "lappend auto_path /usr/brlcad/tclscripts /usr/brlcad/tclscripts/lib /usr/brlcad/tclscripts/util tclscripts tclscripts/lib tclscripts/lib/util src/tclscripts src/tclscripts/lib src/tclscripts/util ../tclscripts ../tclscripts/lib ../tclscripts/util ../../tclscripts ../../tclscripts/lib ../../tclscripts/util");
	}
	bu_vls_free(&vls);

	/* register bwish/btclsh commands */
	cmdInit(interp);

	return TCL_OK;
}
