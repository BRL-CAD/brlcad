/*                         R O T _ P O I N T . C
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
/** @file libged/rot_point.c
 *
 * The rot_point command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_rot_point(struct ged *gedp, int argc, const char *argv[])
{
    mat_t invRot;
    point_t point;
    point_t rpoint;
    static const char *usage = "x y z";

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

    if (argc != 2 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 2) {
	if (bn_decode_vect(point, argv[1]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    } else {
	if (sscanf(argv[1], "%lf", &point[X]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_m2v_point: bad X value - %s\n", argv[1]);
	    return GED_ERROR;
	}

	if (sscanf(argv[2], "%lf", &point[Y]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_m2v_point: bad Y value - %s\n", argv[2]);
	    return GED_ERROR;
	}

	if (sscanf(argv[3], "%lf", &point[Z]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_m2v_point: bad Z value - %s\n", argv[3]);
	    return GED_ERROR;
	}
    }

    /* Rotate the incoming point */
    bn_mat_inv(invRot, gedp->ged_gvp->gv_rotation);
    MAT4X3PNT(rpoint, invRot, point);
    bn_encode_vect(gedp->ged_result_str, rpoint);

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
