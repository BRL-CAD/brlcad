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

    /* Locate the BRL-CAD-specific Tcl scripts, set the auto_path */
    tclcad_auto_path(interp);

    /* This runs the init.tcl script */
    if( Tcl_Init(interp) == TCL_ERROR )
	bu_log("Tcl_Init error %s\n", interp->result);

    /* Initialize [incr Tcl] */
    if (Itcl_Init(interp) == TCL_ERROR)
	bu_log("Itcl_Init error %s\n", interp->result);

    /* Import [incr Tcl] commands into the global namespace. */
    if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
		   "::itcl::*", /* allowOverwrite */ 1) != TCL_OK)
	bu_log("Tcl_Import error %s\n", interp->result);

#if 0 /* FIXME: disabled.  defined(HAVE_GETENV) */
    /* append our own bin dir to (the end of) our search path.
     * this must be performed before the Tcl interpreter is
     * created, or we'll need to use Tcl_PutEnv() instead of
     * setenv().
     */
    {
	struct bu_vls newpath;
	const char *path = getenv("PATH");
	const char *binpath = bu_brlcad_root("bin", 1);
	int set = 0;

	if (binpath) {

	    bu_vls_init(&newpath);

	    if (path) {
		if (path[strlen(path)-1] == ':') {
		    bu_vls_printf(&newpath, "PATH=%s%s", path, binpath);
		} else {
		    bu_vls_printf(&newpath, "PATH=%s:%s", path, binpath);
		}
	    } else {
		bu_vls_printf(&newpath, "PATH=%s", binpath);
	    }

#  ifdef HAVE_PUTENV
	    set = putenv(bu_vls_addr(&newpath));
#  else
#    ifdef HAVE_SETENV
	    /* skip the "PATH=" in the newpath */
	    set = setenv("PATH", bu_vls_addr(&newpath)+5, 1);
#    else
#      error "No putenv or setenv available.. don't know how to set environment variables."
#    endif
#  endif
	    Tcl_PutEnv(bu_vls_addr(&newpath));

	    if (set != 0) {
		perror("unable to modify PATH");
	    }
	    bu_vls_free(&newpath);
	}
    }
#endif

#ifdef BRLCAD_DEBUG
    /* Initialize libbu */
    Bu_d_Init(interp);

    /* Initialize libbn */
    Bn_d_Init(interp);

    /* Initialize librt (includes database, drawable geometry and view objects) */
    if (Rt_d_Init(interp) == TCL_ERROR) {
	bu_log("Rt_d_Init error %s\n", interp->result);
    }
#else
    /* Initialize libbu */
    Bu_Init(interp);

    /* Initialize libbn */
    Bn_Init(interp);

    /* Initialize librt (includes database, drawable geometry and view objects) */
    if (Rt_Init(interp) == TCL_ERROR) {
	bu_log("Rt_Init error %s\n", interp->result);
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
