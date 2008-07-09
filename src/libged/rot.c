/*                         R O T . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file rot.c
 *
 * The rot command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ged_private.h"

int
ged_rot(struct ged *gedp, int argc, const char *argv[])
{
    vect_t rvec;
    mat_t rmat;
    char coord;
    static const char *usage = "[-m|-v] x y z";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    /* process possible coord flag */
    if (argv[1][0] == '-' && (argv[1][1] == 'v' || argv[1][1] == 'm') && argv[1][2] == '\0') {
	coord = argv[1][1];
	--argc;
	++argv;
    } else
	coord = gedp->ged_gvp->gv_coord;

    if (argc != 2 && argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argc == 2) {
	if (bn_decode_vect(rvec, argv[1]) != 3) {
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}
    } else {
	if (sscanf(argv[1], "%lf", &rvec[X]) < 1) {
	    bu_vls_printf(&gedp->ged_result_str, "ged_vrot: bad X value %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[2], "%lf", &rvec[Y]) < 1) {
	    bu_vls_printf(&gedp->ged_result_str, "ged_vrot: bad Y value %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[3], "%lf", &rvec[Z]) < 1) {
	    bu_vls_printf(&gedp->ged_result_str, "ged_vrot: bad Z value %s\n", argv[3]);
	    return BRLCAD_ERROR;
	}
    }

    VSCALE(rvec, rvec, -1.0);
    bn_mat_angles(rmat, rvec[X], rvec[Y], rvec[Z]);

    return ged_do_rot(gedp, coord, rmat, (int (*)())0);
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
