/*                   D R A W _ C A L C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file draw_calc.cpp
 *
 * Utility functions for miscellaneous draw calculations.
 *
 */

#include "common.h"
#include "bu.h"
#include "brep.h"
#include "raytrace.h"
#include "rtgeom.h"

__BEGIN_DECLS

static fastf_t
brep_avg_curve_bbox_diagonal_len(ON_Brep *brep)
{
    fastf_t avg_curve_len = 0.0;
    int i, num_curves = 0;

    for (i = 0; i < brep->m_E.Count(); ++i) {
	ON_BrepEdge &e = brep->m_E[i];
	const ON_Curve *crv = e.EdgeCurveOf();

	if (!crv->IsLinear()) {
	    ++num_curves;

	    ON_BoundingBox bbox;
	    if (crv->GetTightBoundingBox(bbox)) {
		avg_curve_len += bbox.Diagonal().Length();
	    } else {
		ON_3dVector linear_approx =
		    crv->PointAtEnd() - crv->PointAtStart();
		avg_curve_len += linear_approx.Length();
	    }
	}
    }
    avg_curve_len /= num_curves;

    return avg_curve_len;
}

fastf_t
brep_est_avg_curve_len(struct rt_brep_internal *bi)
{
    return brep_avg_curve_bbox_diagonal_len(bi->brep) * 2.0;
}

__END_DECLS

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
