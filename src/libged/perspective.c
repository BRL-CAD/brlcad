/*                         P E R S P E C T I V E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file libged/perspective.c
 *
 * The perspective command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_perspective(struct ged *gedp, int argc, const char *argv[])
{
    /* intentionally double for scan */
    double perspective;
    static const char *usage = "[angle]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get the perspective angle */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_gvp->gv_perspective);
	return GED_OK;
    }

    /* set perspective angle */
    if (argc == 2) {
	if (sscanf(argv[1], "%lf", &perspective) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "bad perspective angle - %s", argv[1]);
	    return GED_ERROR;
	}

	gedp->ged_gvp->gv_perspective = perspective;

	if (SMALL_FASTF < gedp->ged_gvp->gv_perspective) {
	    ged_persp_mat(gedp->ged_gvp->gv_pmat, gedp->ged_gvp->gv_perspective,
			  (fastf_t)1.0f, (fastf_t)0.01f, (fastf_t)1.0e10f, (fastf_t)1.0f);
	} else {
	    MAT_COPY(gedp->ged_gvp->gv_pmat, bn_mat_identity);
	}

	ged_view_update(gedp->ged_gvp);

	return GED_OK;
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
