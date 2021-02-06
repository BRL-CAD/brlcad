/*                C H E C K _ V O L U M E . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2021 United States Government as represented by
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

int check_volume(struct current_state *state,
		 struct db_i *dbip,
		 char **tobjtab,
		 int tnobjs,
		 struct check_parameters *options)
{
    int i;
    FILE *plot_volume = NULL;
    char *name = "volume.plot3";

    if (options->plot_files) {
	plot_volume = fopen(name, "wb");
	if (plot_volume == (FILE *)NULL) {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "cannot open plot file %s\n", name);
	}
	analyze_set_volume_plotfile(state, plot_volume);
    }

    if (perform_raytracing(state, dbip, tobjtab, tnobjs, ANALYSIS_VOLUME)) return GED_ERROR;

    print_verbose_debug(options);
    bu_vls_printf(_ged_current_gedp->ged_result_str, "Volume:\n");

    for (i=0; i < tnobjs; i++){
	fastf_t volume = 0;
	volume = analyze_volume(state, tobjtab[i]);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s %g %s\n", tobjtab[i], volume / options->units[VOL]->val, options->units[VOL]->name);
    }

    bu_vls_printf(_ged_current_gedp->ged_result_str, "\n  Average total volume: %g %s\n", analyze_total_volume(state) / options->units[VOL]->val, options->units[VOL]->name);

    if (options->print_per_region_stats) {
	int num_regions = analyze_get_num_regions(state);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\tregions:\n");
	for (i = 0; i < num_regions; i++) {
	    char *reg_name = NULL;
	    double volume;
	    double high, low;
	    analyze_volume_region(state, i, &reg_name, &volume, &high, &low);
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s volume:%g %s +(%g) -(%g)\n",
			  reg_name,
			  volume/options->units[VOL]->val,
			  options->units[VOL]->name,
			  high/options->units[VOL]->val,
			  low/options->units[VOL]->val);
	}
    }

    if (plot_volume){
	fclose(plot_volume);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\nplot file saved as %s",name);
    }

    return 0;
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
