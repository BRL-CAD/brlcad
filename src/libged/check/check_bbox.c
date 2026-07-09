/*                   C H E C K _ B B O X . C
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/**
 * @file check_bbox.c
 *
 * "check bbox" subcommand – reports the axis-aligned bounding box of the
 * specified objects without firing any rays.
 *
 * This closes the one feature gap between 'check' and 'gqa': gqa has
 * always supported '-A b' (bounding box), which check lacked.  Unlike
 * the other check subcommands, this one does not call perform_raytracing;
 * it delegates to analyze_bbox() which only runs rt_prep.
 */

#include "common.h"

#include "vmath.h"
#include "ged.h"
#include "analyze.h"

#include "../ged_private.h"
#include "./check_private.h"


int
check_format_bbox(struct ged *gedp,
		  struct analyze_results *res,
		  struct check_parameters *options)
{
    size_t i;
    vect_t span;
    double lv  = options->units[LINE]->val;
    double lv2 = lv * lv;

    VSUB2(span, res->bbox_max, res->bbox_min);

    bu_vls_printf(gedp->ged_result_str, "Bounding Box:\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\t  min: (%g, %g, %g) %s\n",
		  res->bbox_min[X] / lv, res->bbox_min[Y] / lv, res->bbox_min[Z] / lv,
		  options->units[LINE]->name);
    bu_vls_printf(gedp->ged_result_str,
		  "\t  max: (%g, %g, %g) %s\n",
		  res->bbox_max[X] / lv, res->bbox_max[Y] / lv, res->bbox_max[Z] / lv,
		  options->units[LINE]->name);
    bu_vls_printf(gedp->ged_result_str,
		  "\t span: (%g, %g, %g) %s\n",
		  span[X] / lv, span[Y] / lv, span[Z] / lv,
		  options->units[LINE]->name);
    bu_vls_printf(gedp->ged_result_str,
		  "\t face areas (YZ, XZ, XY): %g, %g, %g %s^2\n",
		  span[Y] * span[Z] / lv2,
		  span[X] * span[Z] / lv2,
		  span[X] * span[Y] / lv2,
		  options->units[LINE]->name);

    if (options->print_per_region_stats && res->n_objects > 0) {
	bu_vls_printf(gedp->ged_result_str, "\nPer-object bounding boxes:\n");
	for (i = 0; i < res->n_objects; i++) {
	    vect_t ospan;
	    VSUB2(ospan, res->objects[i].bbox_max, res->objects[i].bbox_min);
	    bu_vls_printf(gedp->ged_result_str,
			  "\t%s: min(%g,%g,%g) max(%g,%g,%g) span(%g,%g,%g) %s\n",
			  res->objects[i].name,
			  res->objects[i].bbox_min[X] / lv,
			  res->objects[i].bbox_min[Y] / lv,
			  res->objects[i].bbox_min[Z] / lv,
			  res->objects[i].bbox_max[X] / lv,
			  res->objects[i].bbox_max[Y] / lv,
			  res->objects[i].bbox_max[Z] / lv,
			  ospan[X] / lv, ospan[Y] / lv, ospan[Z] / lv,
			  options->units[LINE]->name);
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
