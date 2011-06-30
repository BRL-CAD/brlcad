/*                          C A D A P P I N I T . C
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
/** @file bwish/cadAppInit.c
 *
 * This file initializes Itcl/Itk and various BRL-CAD libraries.
 *
 */

#include "common.h"

#include "bin.h"
#include "bio.h"

#include "itcl.h"

#ifdef BWISH
#  include "itk.h"
#  include "dm.h"
#  include "fb.h"
#endif

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "ged.h"
#include "tclcad.h"

/* NOTE: it's probably a bad idea to import itcl/itk/iwidgets into the
 * global namespace..  allow for easy means to disable the import.
 */
#define IMPORT_ITCL	0
#define IMPORT_ITK	0
#define IMPORT_IWIDGETS	0

int
Cad_AppInit(Tcl_Interp *interp)
{
/* Locate the BRL-CAD-specific Tcl scripts, set the auto_path */
    tclcad_auto_path(interp);

/* Initialize [incr Tcl] */
    /* NOTE: Calling "package require Itcl" here is apparently
     * insufficient without other changes elsewhere.  The Combination
     * Editor in mged fails with an iwidgets class already loaded
     * error if we don't perform Itcl_Init() here.
     */
    if (Itcl_Init(interp) == TCL_ERROR) {
	bu_log("Itcl_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

#ifdef BWISH
/* Initialize [incr Tk] */
    if (Itk_Init(interp) == TCL_ERROR) {
	bu_log("Itk_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }
#endif

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

/* Initialize libfb */
    if (Fb_Init(interp) == TCL_ERROR) {
	bu_log("Fb_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }
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

    /* Initialize libtclcad's GED Object */
    if (Go_Init(interp) == TCL_ERROR) {
	bu_log("Go_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    return TCL_OK;
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
