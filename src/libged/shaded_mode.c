/*                         S H A D E D _ M O D E . C
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
/** @file libged/shaded_mode.c
 *
 * The shaded_mode command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"

/*
 * Set/get the shaded mode.
 *
 * Usage:
 * shaded_mode [0|1|2]
 *
 */
int
ged_shaded_mode(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[0|1|2]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* get shaded mode */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gdp->gd_shaded_mode);
	return GED_OK;
    }

    /* set shaded mode */
    if (argc == 2) {
	int shaded_mode;

	if (sscanf(argv[1], "%d", &shaded_mode) != 1)
	    goto bad;

	if (shaded_mode < 0 || 2 < shaded_mode)
	    goto bad;

	gedp->ged_gdp->gd_shaded_mode = shaded_mode;
	return GED_OK;
    }

bad:
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
