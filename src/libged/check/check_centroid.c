/*                C H E C K _ C E N T R O I D . C
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

int check_format_centroid(struct ged *gedp,
			  struct analyze_results *res,
			  struct check_parameters *options)
{
    size_t i;
    point_t cen;
    double inv_scale = 1.0 / options->units[LINE]->val;

    bu_vls_printf(gedp->ged_result_str, "Centroid:\n");

    for (i = 0; i < res->n_objects; i++) {
	VMOVE(cen, res->objects[i].centroid);
	VSCALE(cen, cen, inv_scale);
	bu_vls_printf(gedp->ged_result_str, "\t\t%s: (%g %g %g) %s\n",
		      res->objects[i].name,
		      V3ARGS(cen),
		      options->units[LINE]->name);
    }

    VMOVE(cen, res->centroid);
    VSCALE(cen, cen, inv_scale);
    bu_vls_printf(gedp->ged_result_str,
		  "\n  Average centroid: (%g %g %g) %s\n",
		  V3ARGS(cen), options->units[LINE]->name);

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
