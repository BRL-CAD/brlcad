/*                         S N A P . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @file libged/eye.c
 *
 * The eye command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/opt.h"
#include "bu/vls.h"
#include "../ged_private.h"

int
ged_view_snap(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[options] x y [z]";
    int print_help = 0;
    int use_grid = 0;
    int use_lines = 0;
    int use_objects = 0;
    point_t pt = VINIT_ZERO;
    int pt_model = 0;
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[5];
    BU_OPT(d[0], "h", "help",      "",  NULL,  &print_help,   "Print help and exit");
    BU_OPT(d[1], "g", "grid",      "",  NULL,  &use_grid,     "Snap to the view grid");
    BU_OPT(d[2], "l",  "lines",    "",  NULL,  &use_lines,    "Snap to the view lines");
    BU_OPT(d[3], "o",  "objs",     "",  NULL,  &use_objects,  "Snap to drawn object wireframes");
    BU_OPT_NULL(d[4]);

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* must be wanting help */
    if (argc < 1) {
	_ged_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;
    if (argc != 2 && argc != 3) {
        _ged_cmd_help(gedp, usage, d);
        return GED_ERROR;
    }

    /* We need at least X and Y regardless */
    if (bu_opt_fastf_t(&msg, 1, (const char **)&argv[0], (void *)&pt[X]) == -1) {
	bu_vls_printf(gedp->ged_result_str, "problem reading X value %s: %s\n", argv[0], bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return GED_ERROR;
    }
    if (bu_opt_fastf_t(&msg, 1, (const char **)&argv[1], (void *)&pt[Y]) == -1) {
	bu_vls_printf(gedp->ged_result_str, "problem reading Y value %s: %s\n", argv[1], bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return GED_ERROR;
    }
    /* If we've got ONLY X and Y, assume a view coordinate.  Otherwise, assume
     * the incoming point is 3D. */
    if (argc == 3) {
	pt_model = 1;
	if (bu_opt_fastf_t(&msg, 1, (const char **)&argv[2], (void *)&pt[Z]) == -1) {
	    bu_vls_printf(gedp->ged_result_str, "problem reading Z value %s: %s\n", argv[2], bu_vls_cstr(&msg));
	    bu_vls_free(&msg);
	    return GED_ERROR;
	}
    }


    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);


    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (pt_model) {
	bu_vls_printf(gedp->ged_result_str, "%g %g %g", pt[X], pt[Y], pt[Z]);
    } else {
	bu_vls_printf(gedp->ged_result_str, "%g %g", pt[X], pt[Y]);
    }

    bu_vls_free(&msg);
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
