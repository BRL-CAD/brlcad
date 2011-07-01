/*                         T R A . C
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
/** @file libged/tra.c
 *
 * The tra command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_tra_args(struct ged *gedp, int argc, const char *argv[], char *coord, vect_t tvec)
{
    static const char *usage = "[-m|-v] x y z";

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

    /* process possible coord flag */
    if (argv[1][0] == '-' && (argv[1][1] == 'v' || argv[1][1] == 'm') && argv[1][2] == '\0') {
	*coord = argv[1][1];
	--argc;
	++argv;
    } else
	*coord = gedp->ged_gvp->gv_coord;

    if (argc != 2 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 2) {
	if (bn_decode_vect(tvec, argv[1]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    } else {
	if (sscanf(argv[1], "%lf", &tvec[X]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad X value %s\n", argv[0], argv[1]);
	    return GED_ERROR;
	}

	if (sscanf(argv[2], "%lf", &tvec[Y]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad Y value %s\n", argv[0], argv[2]);
	    return GED_ERROR;
	}

	if (sscanf(argv[3], "%lf", &tvec[Z]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad Z value %s\n", argv[0], argv[3]);
	    return GED_ERROR;
	}
    }

    return GED_OK;
}


int
ged_tra(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    char coord;
    vect_t tvec;

    if ((ret = ged_tra_args(gedp, argc, argv, &coord, tvec)) != GED_OK)
	return ret;

    return _ged_do_tra(gedp, coord, tvec, (int (*)())0);
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
