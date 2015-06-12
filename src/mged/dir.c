/*                           D I R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2014 United States Government as represented by
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

#include "common.h"

#include <signal.h>
#include <string.h>

#include "tcl.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "rt/db4.h"

#include "./mged.h"
#include "./mged_dm.h"


/*
 * Debugging aid:  dump memory maps
 */
int
f_memprint(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    CHECK_DBI_NULL;

    if (argc < 1 || 1 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	if (argv && argc > 1)
	    bu_log("Unexpected parameter [%s]\n", argv[1]);
	bu_vls_printf(&vls, "help memprint");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_log("Database free-storage map:\n");
    rt_memprint(&(dbip->dbi_freep));

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
