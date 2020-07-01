/*               B V I E W _ P O L Y G O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file bview_polygon.cpp
 *
 * Routines for dealing with bview polygons.
 *
 * TODO - these data types and routines really should be pushed
 * down to libbg...
 *
 */

#include "common.h"

#include "clipper.hpp"

#include "vmath.h"
#include "bn/mat.h"
#include "dm/defines.h"
#include "dm/bview_util.h"

fastf_t
find_polygon_area(bview_polygon *gpoly, fastf_t sf, matp_t model2view, fastf_t size)
{
    size_t j, k, n;
    ClipperLib::Polygon poly;
    fastf_t area = 0.0;

    if (NEAR_ZERO(sf, SMALL_FASTF))
        return 0.0;

    for (j = 0; j < gpoly->gp_num_contours; ++j) {
        n = gpoly->gp_contour[j].gpc_num_points;
        poly.resize(n);
        for (k = 0; k < n; k++) {
            point_t vpoint;

            /* Convert to view coordinates */
            MAT4X3PNT(vpoint, model2view, gpoly->gp_contour[j].gpc_point[k]);

            poly[k].X = (ClipperLib::long64)(vpoint[X] * sf);
            poly[k].Y = (ClipperLib::long64)(vpoint[Y] * sf);
        }

        area += (fastf_t)ClipperLib::Area(poly);
    }

    sf = 1.0/(sf*sf) * size * size;

    return (area * sf);
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
