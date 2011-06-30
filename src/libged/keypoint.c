/*                         K E Y P O I N T . C
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
/** @file libged/keypoint.c
 *
 * The keypoint command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_keypoint(struct ged *gedp, int argc, const char *argv[])
{
    point_t keypoint;
    static const char *usage = "[x y z]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get keypoint */
    if (argc == 1) {
	VSCALE(keypoint, gedp->ged_gvp->gv_keypoint, gedp->ged_wdbp->dbip->dbi_base2local);
	bn_encode_vect(gedp->ged_result_str, keypoint);

	return GED_OK;
    }

    if (argc != 2 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* set view keypoint */
    if (argc == 2) {
	if (bn_decode_vect(keypoint, argv[1]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    } else {
	if (sscanf(argv[1], "%lf", &keypoint[X]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_keypoint: bad X value - %s\n", argv[1]);
	    return GED_ERROR;
	}

	if (sscanf(argv[2], "%lf", &keypoint[Y]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_keypoint: bad Y value - %s\n", argv[2]);
	    return GED_ERROR;
	}

	if (sscanf(argv[3], "%lf", &keypoint[Z]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_keypoint: bad Z value - %s\n", argv[3]);
	    return GED_ERROR;
	}
    }

    VSCALE(gedp->ged_gvp->gv_keypoint, keypoint, gedp->ged_wdbp->dbip->dbi_local2base);

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
