/*                        P O L Y B I N O U T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
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

#include "common.h"

#include <string.h>
#include <math.h>
#include <signal.h>

#include "vmath.h"
#include "raytrace.h"


#include "ged.h"
#include "./ged_private.h"

/*
 * Interface for writing binary polygons that represent
 * the current (evaluated) view.
 *
 * Usage:  polybinout file
 */
int
ged_polybinout(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    struct polygon_header ph;
    static const char *usage = "Usage:  polybinout file";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
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

    ph.npts = 0;

    if ((fp = fopen(argv[1], "w")) == NULL) {
	perror(argv[1]);
	bu_vls_printf(gedp->ged_result_str, "%s: Unable to open %s for writing\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    dl_polybinout(gedp->ged_gdp->gd_headDisplay, &ph, fp);

    fclose(fp);
    return GED_OK;
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
