/*                 C O N C A V E _ H U L L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019 United States Government as represented by
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
/** @file concave_hull.cpp
 *
 * Interface to the concaveman algorithm code for creating a concave
 * hull from a set of faces and vertices.
 */

#include "common.h"

#include <vector>
#include <map>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/chull.h"
#include "./concaveman.hpp"

#include "bn/plot3.h"

extern "C" int
bg_2d_concave_hull(point2d_t** hull, const point2d_t* points_2d, int n)
{
    if (!hull | !points_2d || n <= 0) return 0;

    /* As a first step, we need the convex hull */
    int *convex_hull = NULL;
    int convex_pnts = bg_2d_chull2(&convex_hull, points_2d, n);
    if (!convex_pnts) {
	bu_log("bg_2d_concave_hull: failed to get convex hull, cannot proceed to concave hull\n");
	return 0;
    }

    // Put all points_2d into a vector that preserves their original index position
    // in the data container.
    std::vector<std::array<double,3>> points(n);
    for (auto i = 0; i < n; i++) {
	points[i] = { points_2d[i][X], points_2d[i][Y], (double)i };
    }

    // Assemble concaveman form of convex hull
    std::vector<int> convex_hull_v(convex_pnts+1);
    for (auto i = 0; i < convex_pnts; i++) {
	convex_hull_v[i] = convex_hull[i];
    }
    // We need to explicitly close the bg_2d_chull loop for concaveman
    convex_hull_v[convex_pnts] = convex_hull[0];

    auto concave_points = concaveman<double, 16>(points, convex_hull_v);
    point2d_t *concave_hull = (point2d_t *)bu_calloc(concave_points.size(), sizeof(point2d_t), "concave hull pnts");
    for (auto i = 0; i < (int)concave_points.size(); i++) {
	concave_hull[i][X] = concave_points[i][0];
	concave_hull[i][Y] = concave_points[i][1];
	bu_log("%f, %f: new ind %d, old ind %d\n", concave_hull[i][X], concave_hull[i][Y], i, (int)concave_points[i][2]);
    }

    int concave_pnts_cnt = (int)concave_points.size();
    (*hull) = concave_hull;
    bu_free(convex_hull, "convex hull");
    return concave_pnts_cnt;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

