/*                      T R I _ P T . C P P
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
/** @file tri_pt.cpp
 *
 * Use Geometric Tools to calculate the distance from a sample point to the
 * closest point on a triangle.  Will also return the coordinates of the
 * closest point if provided with a variable to store them in.
 */

#include "common.h"

#include "Mathematics/DistPointTriangle.h"

#include "vmath.h"
#include "bg/tri_pt.h"

using GTF = fastf_t;
using Vec3 = gte::Vector<3, GTF>;

extern "C" double
bg_tri_closest_pt(point_t *closest_pt, const point_t tp, const point_t V0, const point_t V1, const point_t V2)
{
    gte::DCPPoint3Triangle3<GTF> query;

    Vec3 P;
    P[0] = tp[0]; P[1] = tp[1]; P[2] = tp[2];

    gte::Triangle3<GTF> tri;
    tri.v[0][0] = V0[0]; tri.v[0][1] = V0[1]; tri.v[0][2] = V0[2];
    tri.v[1][0] = V1[0]; tri.v[1][1] = V1[1]; tri.v[1][2] = V1[2];
    tri.v[2][0] = V2[0]; tri.v[2][1] = V2[1]; tri.v[2][2] = V2[2];

    auto result = query(P, tri);

    if (closest_pt) {
	VSET(*closest_pt,
	     result.closest[1][0],
	     result.closest[1][1],
	     result.closest[1][2]);
    }

    // Original API returns distance (not squared)
    return std::sqrt(result.sqrDistance);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
