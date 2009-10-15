/*                         S C A L E . C
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
/** @file scale.c
 *
 * The scale command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
_ged_scale_args(struct ged *gedp, int argc, const char *argv[], fastf_t *sf)
{
    static const char *usage = "sf";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (sscanf(argv[1], "%lf", sf) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "bad scale factor - %s", argv[1]);
	return GED_ERROR;
    }

    return GED_OK;
}

int
_ged_scale(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    fastf_t sf;

    if ((ret = _ged_scale_args(gedp, argc, argv, &sf)) != GED_OK)
	return ret;

    if (sf <= SMALL_FASTF || INFINITY < sf)
	return GED_OK;

    /* scale the view */
    gedp->ged_gvp->gv_scale *= sf;

    if (gedp->ged_gvp->gv_scale < RT_MINVIEWSIZE)
	gedp->ged_gvp->gv_scale = RT_MINVIEWSIZE;
    gedp->ged_gvp->gv_size = 2.0 * gedp->ged_gvp->gv_scale;
    gedp->ged_gvp->gv_isize = 1.0 / gedp->ged_gvp->gv_size;
    ged_view_update(gedp->ged_gvp);

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
