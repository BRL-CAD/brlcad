/*                C H E C K _ M A S S . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
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

int check_format_mass(struct ged *gedp,
		      struct analyze_results *res,
		      struct check_parameters *options)
{
    size_t i;

    bu_vls_printf(gedp->ged_result_str, "Mass:\n");

    for (i = 0; i < res->n_objects; i++) {
	bu_vls_printf(gedp->ged_result_str, "\t%s %g %s\n",
		      res->objects[i].name,
		      res->objects[i].mass / options->units[MASS]->val,
		      options->units[MASS]->name);
    }

    bu_vls_printf(gedp->ged_result_str,
		  "\n  Average total mass: %g %s\n",
		  res->total_mass / options->units[MASS]->val,
		  options->units[MASS]->name);

    if (options->print_per_region_stats) {
	bu_vls_printf(gedp->ged_result_str, "\tregions:\n");
	for (i = 0; i < res->n_regions; i++) {
	    bu_vls_printf(gedp->ged_result_str, "\t%s mass:%g %s\n",
			  res->regions[i].name,
			  res->regions[i].mass / options->units[MASS]->val,
			  options->units[MASS]->name);
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
