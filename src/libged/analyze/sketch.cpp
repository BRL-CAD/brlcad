/*                        A R S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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

#include "vmath.h"

#include "rt/geom.h"
#include "ged/defines.h"
#include "../ged_private.h"
#include "./ged_analyze.h"

void
analyze_sketch(struct ged *gedp, const struct rt_db_internal *ip)
{
    fastf_t area = -1;
    point_t centroid;

    if (OBJ[ID_SKETCH].ft_surf_area)
	OBJ[ID_SKETCH].ft_surf_area(&area, ip);

    if (area > 0.0) {
	bu_vls_printf(gedp->ged_result_str, "\nTotal Area: %10.8f",
		      area
		      * gedp->dbip->dbi_local2base
		      * gedp->dbip->dbi_local2base
	    );
    }

    if (OBJ[ID_SKETCH].ft_centroid) {
	OBJ[ID_SKETCH].ft_centroid(&centroid, ip);
	bu_vls_printf(gedp->ged_result_str, "\n    Centroid: (%g, %g, %g)\n",
		      centroid[X] * gedp->dbip->dbi_base2local,
		      centroid[Y] * gedp->dbip->dbi_base2local,
		      centroid[Z] * gedp->dbip->dbi_base2local);
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
