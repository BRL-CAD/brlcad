/*                C H E C K _ W E I G H T . C
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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

int check_weight(struct current_state *state,
		 struct db_i *dbip,
		 char **tobjtab,
		 int tnobjs,
		 struct check_parameters *options)
{
    int i;

    if (perform_raytracing(state, dbip, tobjtab, tnobjs, ANALYSIS_WEIGHT)) return GED_ERROR;

    print_verbose_debug(options);
    bu_vls_printf(_ged_current_gedp->ged_result_str, "Weight:\n");

    for (i=0; i < tnobjs; i++){
	fastf_t weight = 0;
	weight = analyze_weight(state, tobjtab[i]);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s %g %s\n", tobjtab[i], weight / options->units[WGT]->val, options->units[WGT]->name);
    }

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
