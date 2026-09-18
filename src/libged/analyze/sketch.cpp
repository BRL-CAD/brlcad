/*                        A R S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
/** @file ars.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include <cmath>

#include "vmath.h"

#include "rt/geom.h"
#include "ged/defines.h"
#include "../ged_private.h"
#include "./ged_analyze.h"

int
analyze_sketch(struct ged *gedp, const struct rt_db_internal *ip)
{
    const fastf_t metric_error = -1.0;
    fastf_t area = metric_error;
    point_t centroid;
    point_t error_centroid;
    int status = BRLCAD_OK;

    if (OBJ[ID_SKETCH].ft_surf_area) {
	OBJ[ID_SKETCH].ft_surf_area(&area, ip);
	if (!std::isfinite(area) || NEAR_EQUAL(area, metric_error, SMALL_FASTF)) {
	    bu_vls_printf(gedp->ged_result_str, "\nTotal Area: COULD NOT DETERMINE");
	    status = BRLCAD_ERROR;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\nTotal Area: %10.8f",
			  area
			 * gedp->dbip->dbi_local2base
			 * gedp->dbip->dbi_local2base
			 );
	}
    }

    if (OBJ[ID_SKETCH].ft_centroid) {
	VSETALL(centroid, metric_error);
	VSETALL(error_centroid, metric_error);
	OBJ[ID_SKETCH].ft_centroid(&centroid, ip);
	if (std::isfinite(centroid[X]) && std::isfinite(centroid[Y]) &&
	    std::isfinite(centroid[Z]) &&
	    !VNEAR_EQUAL(centroid, error_centroid, SMALL_FASTF)) {
	    bu_vls_printf(gedp->ged_result_str, "\n    Centroid: (%g, %g, %g)\n",
			  centroid[X] * gedp->dbip->dbi_base2local,
			  centroid[Y] * gedp->dbip->dbi_base2local,
			  centroid[Z] * gedp->dbip->dbi_base2local);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\n    Centroid: COULD NOT DETERMINE\n");
	    status = BRLCAD_ERROR;
	}
    }
    return status;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
