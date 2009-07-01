/*                         B B S I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file bbsize.c
 *
 * The bbsize command - report the dimensions of the bounding arb8
 * for a given object.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"

#include "./ged_private.h"


int
ged_bbsize(struct ged *gedp, int argc, const char *argv[])
{
    register int i;
    point_t rpp_min, rpp_max;
    int	use_air = 0;
    static const char *usage = "object";
    const char *str;
    
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (MAXARGS < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    i = 1;

    /* look for a USEAIR option */
    if ( ! strcmp(argv[i], "-u") ) {
	use_air = 1;
	i++;
    }

    if (ged_get_obj_bounds(gedp, argc-1, (const char **)argv+1, use_air, rpp_min, rpp_max) == GED_ERROR)
	return GED_ERROR;

    /* Report Bounding Box Information */
    str = bu_units_string(gedp->ged_wdbp->dbip->dbi_local2base);
    if (!str) str = "Unknown_unit";
    bu_vls_printf(&gedp->ged_result_str, "Object %s Bounding Box Dimensions:\n", argv[1]);
    double xlen = fabs(rpp_max[X] - rpp_min[X])*gedp->ged_wdbp->dbip->dbi_base2local;
    double ylen = fabs(rpp_max[Y] - rpp_min[Y])*gedp->ged_wdbp->dbip->dbi_base2local;
    double zlen = fabs(rpp_max[Z] - rpp_min[Z])*gedp->ged_wdbp->dbip->dbi_base2local;
    bu_vls_printf(&gedp->ged_result_str, "X Length: %.1f %s\nY Length: %.1f %s\nZ Length: %.1f %s\n", xlen, str, ylen, str, zlen, str); 
    double vol = xlen * ylen * zlen;
    bu_vls_printf(&gedp->ged_result_str, "Bounding Box Volume: %.1f %s^3\n", vol, str);
   
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
