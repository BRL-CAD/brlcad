/*                C H E C K _ C E N T R O I D . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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

#include "common.h"

#include "ged.h"

#include "../ged_private.h"
#include "./check_private.h"

int check_centroid(struct current_state *state,
		   struct db_i *dbip,
		   char **tobjtab,
		   int tnobjs,
		   struct check_parameters *options)
{
    int i;
    point_t centroid;

    if (perform_raytracing(state, dbip, tobjtab, tnobjs, ANALYSIS_MASS|ANALYSIS_CENTROIDS)) return GED_ERROR;

    print_verbose_debug(options);
    bu_vls_printf(_ged_current_gedp->ged_result_str, "Centroid:\n");

    for (i=0; i < tnobjs; i++){
	analyze_centroid(state, tobjtab[i], centroid);
	VSCALE(centroid, centroid, 1/options->units[LINE]->val);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\t\t%s: (%g %g %g) %s\n", tobjtab[i], V3ARGS(centroid), options->units[LINE]->name);
    }
    analyze_total_centroid(state, centroid);
    VSCALE(centroid, centroid, 1/options->units[LINE]->val);
    bu_vls_printf(_ged_current_gedp->ged_result_str, "\n  Average centroid: (%g %g %g) %s\n", V3ARGS(centroid), options->units[LINE]->name);

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
