/*                     L S E G _ P T . C P P
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
/** @file lseg_pt.cpp
 *
 * Uses Geometric Tools gte::DCPPoint3Segment3 to compute closest point and
 * squared distance.
 */

#include "common.h"

#include "Mathematics/DistPointSegment.h"

#include "vmath.h"
#include "bg/lseg.h"

using GTF = fastf_t;
using Vec3 = gte::Vector<3, GTF>;

extern "C" double
bg_distsq_lseg3_pt(point_t *c, const point_t P0, const point_t P1, const point_t Q)
{
    gte::Segment3<GTF> seg;
    seg.p[0][0] = P0[0]; seg.p[0][1] = P0[1]; seg.p[0][2] = P0[2];
    seg.p[1][0] = P1[0]; seg.p[1][1] = P1[1]; seg.p[1][2] = P1[2];

    Vec3 point;
    point[0] = Q[0]; point[1] = Q[1]; point[2] = Q[2];

    gte::DCPPoint3Segment3<GTF> query;
    auto result = query(point, seg);

    if (c) {
	VSET(*c,
	     result.closest[1][0],
	     result.closest[1][1],
	     result.closest[1][2]);
    }
    return result.sqrDistance;
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
