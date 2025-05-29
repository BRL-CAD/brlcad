/*                 T R I M E S H _ D I F F . C P P
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
/** @file trimesh_diff.cpp
 *
 * Given two arrays of faces and points, determine if they define
 * the same mesh with tolerance.
 *
 */

#include "common.h"

//#include <map>
//#include <set>
//#include <functional>
#include <algorithm>
#include <unordered_map>
#include <vector>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/trimesh.h"

class tm_diff_vert {
    public:
	fastf_t x;
	fastf_t y;
	fastf_t z;
	size_t orig_ind;
};

bool vcmp(const tm_diff_vert &v1, const tm_diff_vert &v2) {
    if (!NEAR_EQUAL(v1.x, v2.x, SMALL_FASTF))
	return v1.x < v2.x;
    if (!NEAR_EQUAL(v1.y, v2.y, SMALL_FASTF))
	return v1.y < v2.y;
    return v1.z < v2.z;
}

bool vequal(const tm_diff_vert &v1, const tm_diff_vert &v2, fastf_t tol) {
    if (!NEAR_EQUAL(v1.x, v2.x, tol))
	return false;
    if (!NEAR_EQUAL(v1.y, v2.y, tol))
	return false;
    if (!NEAR_EQUAL(v1.z, v2.z, tol))
	return false;
    return true;
}

extern "C" int
bg_trimesh_diff(
	const int *f1, size_t num_f1, const point_t *p1, size_t num_p1,
        const int *f2, size_t num_f2, const point_t *p2, size_t num_p2,
	fastf_t dist_tol
	)
{
    if (!f1 || !p1 || !f2 || !p2)
	return 1;
    if (num_f1 != num_f2 || num_p1 != num_p2)
	return 1;

    // Trivial case - empty meshes
    if (!num_p1)
	return 0;

    // Next, compare the sorted vertices.  If they don't line up, there is no
    // possibility that we have the same mesh.
    std::vector<tm_diff_vert> pv1;
    for (size_t i = 0; i < num_p1; i++) {
	tm_diff_vert dv;
	dv.x = p1[i][X];
	dv.y = p1[i][Y];
	dv.z = p1[i][Z];
	dv.orig_ind = i;
	pv1.push_back(dv);
    }
    std::vector<tm_diff_vert> pv2;
    for (size_t i = 0; i < num_p2; i++) {
	tm_diff_vert dv;
	dv.x = p2[i][X];
	dv.y = p2[i][Y];
	dv.z = p2[i][Z];
	dv.orig_ind = i;
	pv2.push_back(dv);
    }

    // Sort both vectors
    std::sort(pv1.begin(), pv1.end(), vcmp);
    std::sort(pv2.begin(), pv2.end(), vcmp);

    for (size_t i = 0; i < pv1.size(); i++) {
	if (!vequal(pv1[i], pv2[i], dist_tol))
	    return 1;
    }

    // If we have no faces, vertices are all we have to go on and
    // we have passed those checks.
    if (!num_f1)
	return 0;

    // Next up is topology.  faces reference original points arrays, which may
    // not line up - make a map from the old arrays to the new (which do line
    // up) and use that to set up triangles
    std::unordered_map<size_t, size_t> ind_map_1;
    for (size_t i = 0; i < pv1.size(); i++)
	ind_map_1[i] = pv1[i].orig_ind;
    std::unordered_map<size_t, size_t> ind_map_2;
    for (size_t i = 0; i < pv2.size(); i++)
	ind_map_2[i] = pv2[i].orig_ind;

    for (size_t i = 0; i < num_f1; i++) {
	size_t tri_1[3];
	tri_1[0] = ind_map_1[f1[3*i+0]];
	tri_1[1] = ind_map_1[f1[3*i+1]];
	tri_1[2] = ind_map_1[f1[3*i+2]];

	// Tri2 also has three vertices, but if our starting index is offset
	// the comparison logic is a pain - just repeat the first two vertices
	// so a simple offset will let us compare all configurations.
	size_t tri_2[5];
	tri_2[0] = ind_map_2[f1[3*i+0]];
	tri_2[1] = ind_map_2[f1[3*i+1]];
	tri_2[2] = ind_map_2[f1[3*i+2]];
	tri_2[3] = tri_2[0];
	tri_2[4] = tri_2[1];

	// Find the first vertex of tri_1 in tri_2
	int offset = -1;
	for (int j = 0; j < 3; j++) {
	    if (tri_1[0] == tri_2[j]) {
		offset = j;
		break;
	    }
	}

	// If we couldn't find the first vertex of tri_1 in tri_2's vertices,
	// the faces cannot be referring to the same vertex and we have a
	// different mesh
	if (offset < 0) {
	    return 1;
	}

	// We have a match at offset, check that the next two are also equal.
	// If neither matches, we don't have a matching triangle and we are
	// different.
	if (tri_1[1] != tri_2[offset+1])
	    return 1;
	if (tri_1[2] != tri_2[offset+2])
	    return 1;
    }

    // Verts matched, all triangles found - not different
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
