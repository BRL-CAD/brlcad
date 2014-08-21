/*                        T C L C A D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file libtclcad/tclcad.c
 *
 * Initialize BRL-CAD's Tcl interface.
 *
 */

#include "common.h"

#define RESOURCE_INCLUDED 1
#include <tcl.h>
#ifdef HAVE_TK
#  include <tk.h>
#endif

#include "bio.h"

#include "bu.h"
#include "dm.h"
#include "fb.h"
#include "bn.h"
#include "vmath.h"
#include "raytrace.h"
#include "tclcad.h"

/* Private headers */
#include "brlcad_version.h"
#include "tclcad_private.h"


/* defined in cmdhist_obj.c */
extern int Cho_Init(Tcl_Interp *interp);


int
library_initialized(int setit)
{
    static int initialized = 0;
    if (setit)
	initialized = 1;

    return initialized;
}


static int
wrapper_func(ClientData data, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct bu_cmdtab *ctp = (struct bu_cmdtab *)data;

    return ctp->ct_func(interp, argc, argv);
}


void
tclcad_register_cmds(Tcl_Interp *interp, struct bu_cmdtab *cmds)
{
    struct bu_cmdtab *ctp = NULL;

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	(void)Tcl_CreateCommand(interp, ctp->ct_name, wrapper_func, (ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
    }
}


int
Tclcad_Init(Tcl_Interp *interp)
{
    if (library_initialized(0))
	return TCL_OK;

    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

#ifdef HAVE_TK
    if (Tk_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
#endif

    /* Locate the BRL-CAD-specific Tcl scripts, set the auto_path */
    tclcad_auto_path(interp);

    /* Initialize [incr Tcl] */
    if (tclcad_eval(interp, "package require Itcl", NULL) != TCL_OK) {
      bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
      return TCL_ERROR;
    }

#ifdef HAVE_TK
    /* Initialize [incr Tk] */
    if (tclcad_eval(interp, "package require Itk", NULL) != TCL_OK) {
      bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
      return TCL_ERROR;
    }
#endif

    /* Initialize the Iwidgets package */
    if (tclcad_eval(interp, "package require Iwidgets", NULL) != TCL_OK) {
	bu_log("Tcl_Eval ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

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

    /* Initialize the GED object */
    if (Go_Init(interp) == TCL_ERROR) {
	bu_log("Go_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	return TCL_ERROR;
    }

    /* initialize command history objects */
    Cho_Init(interp);

    Tcl_PkgProvide(interp, "Tclcad", brlcad_version());

    (void)library_initialized(1);

    return TCL_OK;
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
