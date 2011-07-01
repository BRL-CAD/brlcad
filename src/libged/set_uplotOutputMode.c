/*                         S E T _ U P L O T O U T P U T M O D E . C
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
/** @file libged/set_uplotOutputMode.c
 *
 * The set_uplotOutputMode command.
 *
 */

#include <string.h>
#include "ged.h"
#include "plot3.h"

/*
 * Set/get the unix plot output mode
 *
 * Usage:
 * set_uplotOutputMode [binary|text]
 *
 */
int
ged_set_uplotOutputMode(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[binary|text]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Get the plot output mode */
    if (argc == 1) {
	if (gedp->ged_gdp->gd_uplotOutputMode == PL_OUTPUT_MODE_BINARY)
	    bu_vls_printf(gedp->ged_result_str, "binary");
	else
	    bu_vls_printf(gedp->ged_result_str, "text");

	return GED_OK;
    }

    if (argv[1][0] == 'b' &&
	BU_STR_EQUAL("binary", argv[1]))
	gedp->ged_gdp->gd_uplotOutputMode = PL_OUTPUT_MODE_BINARY;
    else if (argv[1][0] == 't' &&
	     BU_STR_EQUAL("text", argv[1]))
	gedp->ged_gdp->gd_uplotOutputMode = PL_OUTPUT_MODE_TEXT;
    else {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }


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
