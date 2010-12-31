/*                          Q R A Y . C
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
 */
/** @file qray.c
 *
 * Routines to set and get "Query Ray" variables.
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "dg.h"
#include "./mged.h"
#include "./mged_dm.h"
#include "./qray.h"

int
f_qray(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int ret;
    Tcl_DString ds;

    if (gedp == GED_NULL)
	return TCL_OK;

    Tcl_DStringInit(&ds);

    ret = ged_qray(gedp, argc, (const char **)argv);
    Tcl_DStringAppend(&ds, bu_vls_addr(&gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret != GED_OK)
	return TCL_ERROR;

    return TCL_OK;
}


void
init_qray(void)
{
    ged_init_qray(gedp->ged_gdp);
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
