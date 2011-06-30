/*                         P O V . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/pov.c
 *
 * The pov command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_pov(struct ged *gedp, int argc, const char *argv[])
{
    vect_t center;
    quat_t quat;
    vect_t eye_pos;
    fastf_t scale;
    fastf_t perspective;
    static const char *usage = "center quat scale eye_pos perspective";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /***************** Get the arguments *******************/

    if (bn_decode_vect(center, argv[1]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "ged_pov: bad center - %s\n", argv[1]);
	return TCL_ERROR;
    }

    if (bn_decode_quat(quat, argv[2]) != 4) {
	bu_vls_printf(gedp->ged_result_str, "ged_pov: bad quat - %s\n", argv[2]);
	return TCL_ERROR;
    }

    if (sscanf(argv[3], "%lf", &scale) != 1) {
	bu_vls_printf(gedp->ged_result_str, "ged_pov: bad scale - %s\n", argv[3]);
	return TCL_ERROR;
    }

    if (bn_decode_vect(eye_pos, argv[4]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "ged_pov: bad eye position - %s\n", argv[4]);
	return TCL_ERROR;
    }

    if (sscanf(argv[5], "%lf", &perspective) != 1) {
	bu_vls_printf(gedp->ged_result_str, "ged_pov: bad perspective - %s\n", argv[5]);
	return TCL_ERROR;
    }

    /***************** Use the arguments *******************/

    VSCALE(center, center, gedp->ged_wdbp->dbip->dbi_local2base);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, center);
    quat_quat2mat(gedp->ged_gvp->gv_rotation, quat);
    gedp->ged_gvp->gv_scale = gedp->ged_wdbp->dbip->dbi_local2base * scale;
    VSCALE(eye_pos, eye_pos, gedp->ged_wdbp->dbip->dbi_local2base);
    VMOVE(gedp->ged_gvp->gv_eye_pos, eye_pos);
    gedp->ged_gvp->gv_perspective = perspective;

    ged_view_update(gedp->ged_gvp);

    return GED_OK;
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
