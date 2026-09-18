/*             T R I M E S H _ S E L F _ I S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file trimesh_self_isect.cpp */

#include "common.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <exception>
#include <vector>

#include "bg/trimesh.h"
#include "Mathematics/MeshValidation.h"

extern "C" int
bg_trimesh_self_isect(const int *faces, size_t num_faces,
	const point_t *points, size_t num_points)
{
    if (num_faces == 0)
	return 0;
    if (!faces || !points || num_points == 0 ||
	num_points > static_cast<size_t>(std::numeric_limits<int32_t>::max()) ||
	num_faces > std::numeric_limits<size_t>::max() / 3)
	return -1;

    try {
	std::vector<gte::Vector3<fastf_t>> vertices;
	vertices.reserve(num_points);
	for (size_t i = 0; i < num_points; ++i) {
	    if (!std::isfinite(points[i][0]) || !std::isfinite(points[i][1]) ||
		!std::isfinite(points[i][2]))
		return -1;
	    vertices.push_back({points[i][0], points[i][1], points[i][2]});
	}

	std::vector<std::array<int32_t, 3>> triangles;
	triangles.reserve(num_faces);
	for (size_t i = 0; i < num_faces; ++i) {
	    std::array<int32_t, 3> tri;
	    for (size_t j = 0; j < 3; ++j) {
		int index = faces[3 * i + j];
		if (index < 0 || static_cast<size_t>(index) >= num_points)
		    return -1;
		tri[j] = static_cast<int32_t>(index);
	    }
	    if (tri[0] == tri[1] || tri[1] == tri[2] || tri[2] == tri[0])
		return -1;
	    auto normal = Cross(vertices[tri[1]] - vertices[tri[0]],
		vertices[tri[2]] - vertices[tri[0]]);
	    fastf_t area_squared = Dot(normal, normal);
	    if (!(area_squared > 0.0) || !std::isfinite(area_squared))
		return -1;
	    triangles.push_back(tri);
	}

	return gte::MeshValidation<fastf_t>::HasSelfIntersections(vertices, triangles) ? 1 : 0;
    } catch (std::exception const&) {
	// A C caller must receive an error code rather than a C++ exception.
	return -1;
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
