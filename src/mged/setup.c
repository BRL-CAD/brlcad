/*                         S E T U P . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
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
 */
/** @file setup.c
 *
 *  routines to initialize mged
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <tcl.h>
#include <itcl.h>
#include <string.h>

/* common headers */
#include "machine.h"
#include "bu.h"
#include "bn.h"
#include "raytrace.h"
#include "vmath.h"
#include "tclcad.h"

/* local headers */
#include "./ged.h"


extern void cmd_setup(void);
extern void init_qray(void);

/*
 * Initialize mged, configure the path, set up the tcl interpreter.
 */
void
mged_setup(void)
{
    int try_auto_path = 0;

    int init_tcl = 1;
    int init_itcl = 1;
    struct bu_vls str;
    const char *name = bu_getprogname();

    /* locate our run-time binary (must be called before Tcl_CreateInterp()) */
    if (name) {
	Tcl_FindExecutable(name);
    } else {
	Tcl_FindExecutable("mged");
    }

    /* Create the interpreter */
    interp = Tcl_CreateInterp();

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
	    break;
	}
	init_tcl=0;

	/* warn if tcl_library isn't set by now */
	if (try_auto_path) {
	    tclcad_tcl_library(interp);
	}

	/* Initialize [incr Tcl] */
	Tcl_ResetResult(interp);
	if (init_itcl && Itcl_Init(interp) == TCL_ERROR) {
	    if (!try_auto_path) {
		try_auto_path=1;
		continue;
	    }
	    bu_log("Itcl_Init ERROR:\n%s\n", Tcl_GetStringResult(interp));
	    break;
	}
	Tcl_StaticPackage(interp, "Itcl", Itcl_Init, Itcl_SafeInit);
	init_itcl=0;

	/* don't actually want to loop forever */
	break;

    } /* end iteration over Init() routines that need auto_path */
    Tcl_ResetResult(interp);

    /* Import [incr Tcl] commands into the global namespace. */
    if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp), "::itcl::*", /* allowOverwrite */ 1) != TCL_OK) {
	bu_log("Tcl_Import ERROR: %s\n", Tcl_GetStringResult(interp));
	Tcl_ResetResult(interp);
    }

#ifdef BRLCAD_DEBUG
    /* Initialize libbu */
    Bu_d_Init(interp);

    /* Initialize libbn */
    Bn_d_Init(interp);

    /* Initialize librt (includes database, drawable geometry and view objects) */
    if (Rt_d_Init(interp) == TCL_ERROR) {
	bu_log("Rt_d_Init ERROR: %s\n", Tcl_GetStringResult(interp));
	Tcl_ResetResult(interp);
    }
#else
    /* Initialize libbu */
    Bu_Init(interp);

    /* Initialize libbn */
    Bn_Init(interp);

    /* Initialize librt (includes database, drawable geometry and view objects) */
    if (Rt_Init(interp) == TCL_ERROR) {
	bu_log("Rt_Init ERROR: %s\n", Tcl_GetStringResult(interp));
	Tcl_ResetResult(interp);
    }
#endif

    /* initialize MGED's drawable geometry object */
    dgop = dgo_open_cmd("mged", wdbp);

    view_state->vs_vop = vo_open_cmd("");
    view_state->vs_vop->vo_callback = mged_view_obj_callback;
    view_state->vs_vop->vo_clientData = view_state;
    view_state->vs_vop->vo_scale = 500;
    view_state->vs_vop->vo_size = 2.0 * view_state->vs_vop->vo_scale;
    view_state->vs_vop->vo_invSize = 1.0 / view_state->vs_vop->vo_size;
    MAT_DELTAS_GET_NEG(view_state->vs_orig_pos, view_state->vs_vop->vo_center);

    /* register commands */
    cmd_setup();

    history_setup();
    mged_global_variable_setup(interp);
#if !TRY_NEW_MGED_VARS
    mged_variable_setup(interp);
#endif

    /* Tcl needs to write nulls onto subscripted variable names */
    bu_vls_init(&str);
    bu_vls_printf( &str, "%s(state)", MGED_DISPLAY_VAR );
    Tcl_SetVar(interp, bu_vls_addr(&str), state_str[state], TCL_GLOBAL_ONLY);

    /* initialize "Query Ray" variables */
    init_qray();

    Tcl_ResetResult(interp);

    bu_vls_free(&str);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
