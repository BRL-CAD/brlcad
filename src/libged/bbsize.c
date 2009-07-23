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
    point_t rpp_min, rpp_max;
    point_t obj_min, obj_max;
    int c;
    int	use_air = 1;
    int print_header = 1;
    int print_rpp = 0;
    int print_dim = 0;
    int print_vol = 0;
    int i;
    static const char *usage = "object";
    const char *str;
    double xlen;
    double ylen;
    double zlen;
    double vol;
    
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

    bu_optind = 1;      /* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "dequv")) != EOF) {
	switch (c) {
	    case 'd':
		print_dim = 1;
		break;
	    case 'e':
		print_rpp = 1;
		break;
	    case 'q':
		print_header = 0;
		break;
	    case 'u':
		use_air = 0;
		break;
	    case 'v':
		print_vol = 1;
		break;
	    default:
		bu_vls_printf(&gedp->ged_result_str, "Unrecognized option - %c", c);
		return GED_ERROR;
	}
    }

    /* Don't want to print NO info, so default to dim and vol printout if nothing specified. */
    if (print_rpp == 0 && print_vol == 0 && print_dim == 0) {
	print_dim = 1;
       	print_vol = 1;
    }

    /* skip options processed plus command name, should just leave object names */
    argc -= bu_optind;
    argv += bu_optind;

    VSETALL(rpp_min, MAX_FASTF);
    VSETALL(rpp_max, -MAX_FASTF);
    for (i = 0; i < argc; i++) {
    	if (ged_get_obj_bounds(gedp, argc - i, (const char **)argv+i, use_air, obj_min, obj_max) == GED_ERROR)
    	    return GED_ERROR;
	VMINMAX(rpp_min, rpp_max, (double *)obj_min);
	VMINMAX(rpp_min, rpp_max, (double *)obj_max);
    }

    /* Report Bounding Box Information */
    str = bu_units_string(gedp->ged_wdbp->dbip->dbi_local2base);
    if (!str) str = "Unknown_unit";

    // Print Header
    if (print_header == 1) {
    	bu_vls_printf(&gedp->ged_result_str, "Bounding Box Dimensions, Object(s)");
	for (i = 0; i < argc - 1 ; i++) {
	    bu_vls_printf(&gedp->ged_result_str, " %s, ", argv[i]);
	}
	bu_vls_printf(&gedp->ged_result_str, " %s:\n", argv[argc-1]);
    }

    // Print rpp
    if (print_rpp == 1) {
	bu_vls_printf(&gedp->ged_result_str, "min {%f %f %f} max {%f %f %f}\n", rpp_min[0], rpp_min[1], rpp_min[2], rpp_max[0], rpp_max[1], rpp_max[2]);
    }

    // Print dim info
    if (print_dim == 1) {
    	xlen = fabs(rpp_max[X] - rpp_min[X])*gedp->ged_wdbp->dbip->dbi_base2local;
    	ylen = fabs(rpp_max[Y] - rpp_min[Y])*gedp->ged_wdbp->dbip->dbi_base2local;
    	zlen = fabs(rpp_max[Z] - rpp_min[Z])*gedp->ged_wdbp->dbip->dbi_base2local;
    	bu_vls_printf(&gedp->ged_result_str, "X Length: %.1f %s\nY Length: %.1f %s\nZ Length: %.1f %s\n", xlen, str, ylen, str, zlen, str); 
    }

    if (print_vol == 1) {
	xlen = fabs(rpp_max[X] - rpp_min[X])*gedp->ged_wdbp->dbip->dbi_base2local;
    	ylen = fabs(rpp_max[Y] - rpp_min[Y])*gedp->ged_wdbp->dbip->dbi_base2local;
    	zlen = fabs(rpp_max[Z] - rpp_min[Z])*gedp->ged_wdbp->dbip->dbi_base2local;
    	vol = xlen * ylen * zlen;
    	bu_vls_printf(&gedp->ged_result_str, "Bounding Box Volume: %.1f %s^3\n", vol, str);
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
