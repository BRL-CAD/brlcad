/*                        P O L Y I F . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2016 United States Government as represented by
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

#include <string.h>
#include <math.h>
#include <signal.h>

#include "vmath.h"
#include "raytrace.h"

#include "./mged.h"
#include "./mged_dm.h"

/*
 * Experimental interface for writing binary polygons that represent
 * the current (evaluated) view.
 *
 * Usage:  polybinout file
 */
int
f_polybinout(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    FILE *fp;
    struct polygon_header ph;

    ph.npts = 0;

    if (argc < 2 || 2 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help polybinout");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((fp = fopen(argv[1], "w")) == NULL) {
	perror(argv[1]);
	return TCL_ERROR;
    }

    dl_polybinout(gedp->ged_gdp->gd_headDisplay, &ph, fp);

    fclose(fp);
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
