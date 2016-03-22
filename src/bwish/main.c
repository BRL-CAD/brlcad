/*                          M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2016 United States Government as represented by
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
#include "tk.h"
#endif

#include "vmath.h"
#include "tclcad.h"

extern int cmdInit(Tcl_Interp *interp);
extern void Cad_Main(int argc, char **argv, Tcl_AppInitProc (*appInitProc), Tcl_Interp *interp);

#ifdef BWISH
Tk_Window tkwin;
#endif

Tcl_Interp *INTERP;


static int
Cad_AppInit(Tcl_Interp *interp)
{
    struct bu_vls tlog = BU_VLS_INIT_ZERO;
    int status = TCL_OK;
#ifdef BWISH
    status = tclcad_init(interp, 1, &tlog);
#else
    status = tclcad_init(interp, 0, &tlog);
#endif
    if (status == TCL_ERROR) {
	bu_log("tclcad_init failure:\n%s\n", bu_vls_addr(&tlog));
	bu_vls_free(&tlog);
	return TCL_ERROR;
    }

    /* register bwish/btclsh commands */
    cmdInit(interp);

    bu_vls_free(&tlog);
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
