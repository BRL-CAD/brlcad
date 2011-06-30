/*                         V I E W . C
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
/** @file libged/view.c
 *
 * The view command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_view(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "quat|ypr|aet|center|eye|size [args]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "quat")) {
	return ged_quat(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "ypr")) {
	return ged_ypr(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "aet")) {
	return ged_aet(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "center")) {
	return ged_center(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "eye")) {
	return ged_eye(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "size")) {
	return ged_size(gedp, argc-1, argv+1);
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
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
