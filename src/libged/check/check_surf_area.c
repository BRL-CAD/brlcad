/*                C H E C K _ S U R F _ A R E A . C
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

int check_surf_area(struct current_state *state,
		    struct db_i *dbip,
		    char **tobjtab,
		    int tnobjs,
		    struct check_parameters *options)
{
    int i;
    double units = options->units[LINE]->val*options->units[LINE]->val;
    if (perform_raytracing(state, dbip, tobjtab, tnobjs, ANALYSIS_SURF_AREA)) return GED_ERROR;

    print_verbose_debug(options);

    bu_vls_printf(_ged_current_gedp->ged_result_str, "Surface Area:\n");

    for (i=0; i < tnobjs; i++){
	fastf_t surf_area;
	surf_area = analyze_surf_area(state, tobjtab[i]);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s %g %s^2\n",
		      tobjtab[i],
		      surf_area/units,
		      options->units[LINE]->name);
    }

    bu_vls_printf(_ged_current_gedp->ged_result_str, "\n  Average total surface area: %g %s^2\n", analyze_total_surf_area(state)/units, options->units[LINE]->name);

    if (options->print_per_region_stats) {
	int num_regions = analyze_get_num_regions(state);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\tregions:\n");
	for (i = 0; i < num_regions; i++) {
	    char *reg_name = NULL;
	    double surf_area;
	    double high, low;
	    analyze_surf_area_region(state, i, &reg_name, &surf_area, &high, &low);
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s surf_area:%g %s^2 +(%g) -(%g)\n",
			  reg_name,
			  surf_area/units,
			  options->units[LINE]->name,
			  high/units,
			  low/units);
	}
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
