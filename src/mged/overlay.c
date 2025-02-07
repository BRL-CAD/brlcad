/*                       O V E R L A Y . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2025 United States Government as represented by
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
/** @file mged/overlay.c
 *
 */

#include "common.h"

#include <math.h>
#include <signal.h>

#include "vmath.h"
#include "raytrace.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

/* Usage:  overlay file.plot3 [name] */
int
cmd_overlay(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int ret;
    Tcl_DString ds;

    if (s->gedp == GED_NULL)
	return TCL_OK;

    Tcl_DStringInit(&ds);

    if (s->gedp->ged_gvp)
	s->gedp->ged_gvp->dmp = (void *)s->mged_curr_dm->dm_dmp;
    ret = ged_exec(s->gedp, argc, argv);
    Tcl_DStringAppend(&ds, bu_vls_addr(s->gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret & GED_HELP)
	return TCL_OK;

    if (ret != BRLCAD_OK)
	return TCL_ERROR;

    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    return ret;
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
