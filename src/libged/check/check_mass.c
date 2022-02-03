/*                C H E C K _ M A S S . C
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

int check_mass(struct current_state *state,
		 struct db_i *dbip,
		 char **tobjtab,
		 int tnobjs,
		 struct check_parameters *options)
{
    int i;

    if (perform_raytracing(state, dbip, tobjtab, tnobjs, ANALYSIS_MASS)) return BRLCAD_ERROR;

    print_verbose_debug(options);
    bu_vls_printf(_ged_current_gedp->ged_result_str, "Mass:\n");

    for (i=0; i < tnobjs; i++){
	fastf_t mass = 0;
	mass = analyze_mass(state, tobjtab[i]);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s %g %s\n", tobjtab[i], mass / options->units[MASS]->val, options->units[MASS]->name);
    }

    bu_vls_printf(_ged_current_gedp->ged_result_str, "\n  Average total mass: %g %s\n", analyze_total_mass(state) / options->units[MASS]->val, options->units[MASS]->name);

    if (options->print_per_region_stats) {
	int num_regions = analyze_get_num_regions(state);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\tregions:\n");
	for (i = 0; i < num_regions; i++) {
	    char *reg_name = NULL;
	    double mass;
	    double high, low;
	    analyze_mass_region(state, i, &reg_name, &mass, &high, &low);
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s mass:%g %s +(%g) -(%g)\n",
			  reg_name,
			  mass/options->units[MASS]->val,
			  options->units[MASS]->name,
			  high/options->units[MASS]->val,
			  low/options->units[MASS]->val);
	}
    }

    return BRLCAD_OK;
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
