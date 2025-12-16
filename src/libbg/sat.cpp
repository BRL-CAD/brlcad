/*                         S A T . C P P
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
/** @file sat.cpp
 *
 * Uses Geometric Tools to provide various Separating Axis Theorem algorithms
 * for detecting collisions between various types of objects (detection only,
 * no calculations to find points or shapes.)
 */

#include "common.h"

#include <cmath>

#include "Mathematics/IntrLine3AlignedBox3.h"
#include "Mathematics/IntrLine3OrientedBox3.h"
#include "Mathematics/IntrTriangle3OrientedBox3.h"
#include "Mathematics/IntrAlignedBox3OrientedBox3.h"
#include "Mathematics/IntrOrientedBox3OrientedBox3.h"

#include "vmath.h"
#include "bv/defines.h"
#include "bg/sat.h"

using GTF = fastf_t;
using Vec3 = gte::Vector3<GTF>;

static inline Vec3 v3_from_array(const GTF v[3])
{
    return Vec3{v[0], v[1], v[2]};
}

/* Convert a BRL-CAD axis*extent vector into a unit axis + scalar extent.
 * If the vector length is below VUNITIZE_TOL, treat it as a degenerate axis
 * with zero extent and substitute a default axis (1,0,0) to keep GTE's box
 * structure valid. This deviates minimally from original semantics, where
 * degenerate axes would have produced a zero vector post-VUNITIZE. */
static inline void axis_and_extent_from_vect(const vect_t v, Vec3 &axis, GTF &extent)
{
    const GTF x = v[0], y = v[1], z = v[2];
    extent = std::sqrt(x * x + y * y + z * z);
    if (extent > (GTF)VUNITIZE_TOL) {
	axis = Vec3{x / extent, y / extent, z / extent};
    } else {
	axis = Vec3{(GTF)1, (GTF)0, (GTF)0};
	extent = (GTF)0;
    }
}

static inline gte::OrientedBox3<GTF> make_obb(
    const point_t center,
    const vect_t e1, const vect_t e2, const vect_t e3)
{
    gte::OrientedBox3<GTF> obb;
    obb.center = v3_from_array(center);
    axis_and_extent_from_vect(e1, obb.axis[0], obb.extent[0]);
    axis_and_extent_from_vect(e2, obb.axis[1], obb.extent[1]);
    axis_and_extent_from_vect(e3, obb.axis[2], obb.extent[2]);
    return obb;
}

extern "C" {

int
bg_sat_line_aabb(point_t origin, vect_t ldir, point_t aabb_center, vect_t aabb_extent)
{
    gte::Line3<GTF> line;
    line.origin = v3_from_array(origin);
    line.direction = v3_from_array(ldir); // Need not be unit length.

    Vec3 c = v3_from_array(aabb_center);
    Vec3 e = v3_from_array(aabb_extent); // half extents
    gte::AlignedBox3<GTF> box;
    box.min = c - e;
    box.max = c + e;

    gte::TIQuery<GTF, gte::Line3<GTF>, gte::AlignedBox3<GTF>> query;
    auto result = query(line, box);
    return result.intersect ? 1 : 0;
}

int
bg_sat_line_obb(point_t origin, vect_t ldir, point_t obb_center,
		vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3)
{
    gte::Line3<GTF> line;
    line.origin = v3_from_array(origin);
    line.direction = v3_from_array(ldir);

    auto obb = make_obb(obb_center, obb_extent1, obb_extent2, obb_extent3);

    gte::TIQuery<GTF, gte::Line3<GTF>, gte::OrientedBox3<GTF>> query;
    auto result = query(line, obb);
    return result.intersect ? 1 : 0;
}

int
bg_sat_tri_aabb(point_t v1, point_t v2, point_t v3,
		point_t center, vect_t extent)
{
    gte::OrientedBox3<GTF> obb;
    obb.center = v3_from_array(center);
    obb.axis[0] = Vec3{(GTF)1, (GTF)0, (GTF)0};
    obb.axis[1] = Vec3{(GTF)0, (GTF)1, (GTF)0};
    obb.axis[2] = Vec3{(GTF)0, (GTF)0, (GTF)1};
    obb.extent = v3_from_array(extent); // half extents

    gte::Triangle3<GTF> tri;
    tri.v[0] = v3_from_array(v1);
    tri.v[1] = v3_from_array(v2);
    tri.v[2] = v3_from_array(v3);

    gte::TIQuery<GTF, gte::Triangle3<GTF>, gte::OrientedBox3<GTF>> query;
    auto result = query(tri, obb);
    return result.intersect ? 1 : 0;
}

int
bg_sat_tri_obb(point_t v1, point_t v2, point_t v3,
	       point_t obb_center, vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3)
{
    auto obb = make_obb(obb_center, obb_extent1, obb_extent2, obb_extent3);

    gte::Triangle3<GTF> tri;
    tri.v[0] = v3_from_array(v1);
    tri.v[1] = v3_from_array(v2);
    tri.v[2] = v3_from_array(v3);

    gte::TIQuery<GTF, gte::Triangle3<GTF>, gte::OrientedBox3<GTF>> query;
    auto result = query(tri, obb);
    return result.intersect ? 1 : 0;
}

int
bg_sat_aabb_obb(point_t aabb_min, point_t aabb_max,
		point_t obb_center, vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3)
{
    gte::AlignedBox3<GTF> abox;
    abox.min = v3_from_array(aabb_min);
    abox.max = v3_from_array(aabb_max);

    auto obb = make_obb(obb_center, obb_extent1, obb_extent2, obb_extent3);

    gte::TIQuery<GTF, gte::AlignedBox3<GTF>, gte::OrientedBox3<GTF>> query;
    auto result = query(abox, obb);
    return result.intersect ? 1 : 0;
}

int
bg_sat_obb_obb(point_t obb1_center, vect_t obb1_extent1, vect_t obb1_extent2, vect_t obb1_extent3,
	       point_t obb2_center, vect_t obb2_extent1, vect_t obb2_extent2, vect_t obb2_extent3)
{
    auto obb0 = make_obb(obb1_center, obb1_extent1, obb1_extent2, obb1_extent3);
    auto obb1 = make_obb(obb2_center, obb2_extent1, obb2_extent2, obb2_extent3);

    gte::TIQuery<GTF, gte::OrientedBox3<GTF>, gte::OrientedBox3<GTF>> query;
    auto result = query(obb0, obb1);
    return result.intersect ? 1 : 0;
}

} // extern "C"

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
