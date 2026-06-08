/*                     C H U L L 3 D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
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
/** @file chull3d.cpp
 *
 * Interface to the Quickhull algorithm code for creating a convex
 * hull from a set of faces and vertices.
 */

#include "common.h"

#include <set>
#include <map>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <iomanip>

#include <float.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "QuickHull.hpp"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bn/tol.h"
#include "bg/chull.h"

static std::string
_chull3d_point_key(const fastf_t x, const fastf_t y, const fastf_t z)
{
    std::ostringstream pkey;
    pkey << std::setprecision(17) << x << "|" << y << "|" << z;
    return pkey.str();
}

static int
_chull3d_find_input_idx(const point_t *input_points_3d, int num_input_pnts, const quickhull::Vector3<fastf_t> &v, std::unordered_map<std::string, std::vector<int>> &input_idx_map)
{
    /* Fast path for exact coordinate matches */
    std::string key = _chull3d_point_key(v.x, v.y, v.z);
    auto m_it = input_idx_map.find(key);
    if (m_it != input_idx_map.end() && !m_it->second.empty()) {
	int idx = m_it->second.back();
	m_it->second.pop_back();
	return idx;
    }

    /* Fallback for near-equal comparisons */
    for (int i = 0; i < num_input_pnts; i++) {
	if (NEAR_EQUAL(input_points_3d[i][0], v.x, BN_TOL_DIST)
		&& NEAR_EQUAL(input_points_3d[i][1], v.y, BN_TOL_DIST)
		&& NEAR_EQUAL(input_points_3d[i][2], v.z, BN_TOL_DIST)) {
	    return i;
	}
    }

    return -1;
}

extern "C" int
bg_3d_chull(int **faces, int *num_faces, point_t **vertices, int *num_vertices,
	    const point_t *input_points_3d, int num_input_pnts)
{
    int pnt_cnt = 0;
    int f_ind = 0;
    if (!faces || !num_faces || !vertices || !num_vertices || !input_points_3d) return 0;

    /* TODO - handle non-3d cases */
    if (num_input_pnts < 4) return 0;

    /* Get the convex hull */
    std::set<size_t> active;
    std::map<size_t, int> vmap;
    quickhull::QuickHull<fastf_t> qh;
    std::vector<quickhull::Vector3<fastf_t>> pc;
    for (int i = 0; i < num_input_pnts; i++) {
	quickhull::Vector3<fastf_t> p(input_points_3d[i][0], input_points_3d[i][1], input_points_3d[i][2]);
	pc.push_back(p);
    }
    fastf_t max_coord = 0.0;
    for (int i = 0; i < num_input_pnts; i++) {
	for (int j = 0; j < 3; j++) {
	    if (std::abs(input_points_3d[i][j]) > max_coord) {
		max_coord = std::abs(input_points_3d[i][j]);
	    }
	}
    }
    if (max_coord < 1e-10) max_coord = 1.0;

    auto hull = qh.getConvexHull(&pc[0].x, pc.size(), true, false, BN_TOL_DIST / max_coord);

    /* Convert QuickHull data to BoT arrays */
    auto indexBuffer = hull.getIndexBuffer();
    auto vertexBuffer = hull.getVertexBuffer();

    (*num_vertices) = (int)vertexBuffer.size();
    (*num_faces) = (int)(indexBuffer.size()/3);
    (*vertices) = (point_t *)bu_calloc(*num_vertices, sizeof(point_t), "new pnt array");
    (*faces) = (int *)bu_calloc(*num_faces *3 , sizeof(int), "new face array");

    for (auto it = indexBuffer.begin(); it != indexBuffer.end(); it++) {
	(*faces)[f_ind] = (int)(*it);
	f_ind++;
    }

    for (auto it = vertexBuffer.begin(); it != vertexBuffer.end(); it++) {
	quickhull::Vector3<fastf_t> v = *it;
	(*vertices)[pnt_cnt][0] = v.x;
	(*vertices)[pnt_cnt][1] = v.y;
	(*vertices)[pnt_cnt][2] = v.z;
	pnt_cnt++;
    }

    /* Flip faces */
    for (int i = 0; i < (*num_faces); i++) {
	int itmp = (*faces)[3*i+1];
	(*faces)[3*i+1] = (*faces)[3*i+2];
	(*faces)[3*i+2] = itmp;
    }

    return 3;
}

extern "C" int
bg_3d_chull2(int **faces, int *num_faces, int **vertices, int *num_vertices,
	     const point_t *input_points_3d, int num_input_pnts)
{
    int f_ind = 0;
    if (!faces || !num_faces || !vertices || !num_vertices || !input_points_3d) return 0;

    /* TODO - handle non-3d cases */
    if (num_input_pnts < 4) return 0;

    quickhull::QuickHull<fastf_t> qh;
    std::vector<quickhull::Vector3<fastf_t>> pc;
    for (int i = 0; i < num_input_pnts; i++) {
	quickhull::Vector3<fastf_t> p(input_points_3d[i][0], input_points_3d[i][1], input_points_3d[i][2]);
	pc.push_back(p);
    }
    fastf_t max_coord = 0.0;
    for (int i = 0; i < num_input_pnts; i++) {
	for (int j = 0; j < 3; j++) {
	    if (std::abs(input_points_3d[i][j]) > max_coord) {
		max_coord = std::abs(input_points_3d[i][j]);
	    }
	}
    }
    if (max_coord < 1e-10) max_coord = 1.0;

    auto hull = qh.getConvexHull(&pc[0].x, pc.size(), true, false, BN_TOL_DIST / max_coord);

    auto indexBuffer = hull.getIndexBuffer();
    auto vertexBuffer = hull.getVertexBuffer();

    std::unordered_map<std::string, std::vector<int>> input_idx_map;
    for (int i = 0; i < num_input_pnts; i++) {
	input_idx_map[_chull3d_point_key(input_points_3d[i][0], input_points_3d[i][1], input_points_3d[i][2])].push_back(i);
    }

    std::vector<int> vmap;
    vmap.reserve(vertexBuffer.size());
    std::set<int> unique_input_indices;
    for (auto it = vertexBuffer.begin(); it != vertexBuffer.end(); it++) {
	int input_idx = _chull3d_find_input_idx(input_points_3d, num_input_pnts, *it, input_idx_map);
	if (input_idx < 0) {
	    bu_log("bg_3d_chull2: failed to map hull vertex (%g, %g, %g) back to input point\n", it->x, it->y, it->z);
	    return 0;
	}
	vmap.push_back(input_idx);
	unique_input_indices.insert(input_idx);
    }

    (*num_faces) = (int)(indexBuffer.size() / 3);
    (*faces) = (int *)bu_calloc(indexBuffer.size(), sizeof(int), "new face array");
    for (auto it = indexBuffer.begin(); it != indexBuffer.end(); it++) {
	(*faces)[f_ind] = vmap[(size_t)(*it)];
	f_ind++;
    }

    /* Flip faces */
    for (int i = 0; i < (*num_faces); i++) {
	int itmp = (*faces)[3*i+1];
	(*faces)[3*i+1] = (*faces)[3*i+2];
	(*faces)[3*i+2] = itmp;
    }

    (*num_vertices) = (int)unique_input_indices.size();
    (*vertices) = (int *)bu_calloc(*num_vertices, sizeof(int), "new hull vertex index array");
    int vi = 0;
    for (auto it = unique_input_indices.begin(); it != unique_input_indices.end(); it++) {
	(*vertices)[vi] = *it;
	vi++;
    }

    return 3;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
