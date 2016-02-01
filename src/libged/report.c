/*                         R E P O R T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
/** @file libged/report.c
 *
 * The report command.
 *
 */

#include "common.h"
#include <stdlib.h>

#include "ged.h"
#include "./ged_private.h"

/*
 * Returns the solid table & vector list as a string
 *
 * Usage:
 * report [lvl]
 *
 */
int
ged_report(struct ged *gedp, int argc, const char *argv[])
{
    int lvl = 0;
    static const char *usage = "lvl";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    lvl = atoi(argv[1]);

    if (lvl <= 3)
	dl_print_schain(gedp->ged_gdp->gd_headDisplay, gedp->ged_wdbp->dbip, lvl, 0, gedp->ged_result_str);
    else
	dl_print_schain(gedp->ged_gdp->gd_headDisplay, gedp->ged_wdbp->dbip, 0, 1, gedp->ged_result_str);

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
