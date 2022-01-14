/*                C H E C K _ M O M E N T S . C
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

int check_moments(struct current_state *state,
		  struct db_i *dbip,
		  char **tobjtab,
		  int tnobjs,
		  struct check_parameters *options)
{
    int i;
    mat_t moments;

    if (perform_raytracing(state, dbip, tobjtab, tnobjs, ANALYSIS_MASS|ANALYSIS_CENTROIDS|ANALYSIS_MOMENTS)) return GED_ERROR;

    print_verbose_debug(options);

    for (i=0; i < tnobjs; i++){
	struct bu_vls title = BU_VLS_INIT_ZERO;
	analyze_moments(state, tobjtab[i], moments);
	bu_vls_printf(&title, "Moments and Products of Inertia For %s", tobjtab[i]);
	bn_mat_print_vls(bu_vls_addr(&title), moments, _ged_current_gedp->ged_result_str);
    }
    analyze_moments_total(state, moments);
    bn_mat_print_vls("For the Moments and Products of Inertia For\n\tAll Specified Objects",
		     moments, _ged_current_gedp->ged_result_str);
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
