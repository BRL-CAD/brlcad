/*                T R I M E S H _ R E P A I R . C P P
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
/** @file trimesh_repair.cpp
 *
 * Implementation of bg_trimesh_repair: repair a non-manifold triangle mesh
 * using the Geometric Tools Engine (GTE) mesh repair and hole-filling routines
 * bundled inside libbg.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include <Mathematics/Vector3.h>
#include <Mathematics/MeshRepair.h>
#include <Mathematics/MeshHoleFilling.h>
#include <Mathematics/MeshPreprocessing.h>

#include "manifold/manifold.h"

#include "vmath.h"
#include "bu/malloc.h"
#include "bg/tri_pt.h"
#include "bg/tri_tri.h"
#include "bg/trimesh.h"
#include "RTree.h"

/* --------------------------------------------------------------------------
 * Internal helpers (mirrors analogous helpers in librt/primitives/bot/repair.cpp
 * but operating on bare GTE types rather than rt_bot_internal).
 * -------------------------------------------------------------------------- */

static double
trimesh_gte_bbox_diag(std::vector<gte::Vector3<double>> const& verts)
{
    if (verts.empty())
	return 0.0;

    double mn[3] = { verts[0][0], verts[0][1], verts[0][2] };
    double mx[3] = { verts[0][0], verts[0][1], verts[0][2] };
    for (auto const& v : verts) {
	for (int c = 0; c < 3; c++) {
	    if (v[c] < mn[c]) mn[c] = v[c];
	    if (v[c] > mx[c]) mx[c] = v[c];
	}
    }
    double dx = mx[0]-mn[0], dy = mx[1]-mn[1], dz = mx[2]-mn[2];
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

static double
trimesh_gte_area(std::vector<gte::Vector3<double>> const& verts,
		 std::vector<std::array<int32_t, 3>> const& tris)
{
    double area = 0.0;
    for (auto const& tri : tris) {
	gte::Vector3<double> const& p0 = verts[tri[0]];
	gte::Vector3<double> const& p1 = verts[tri[1]];
	gte::Vector3<double> const& p2 = verts[tri[2]];
	gte::Vector3<double> e1 = p1 - p0;
	gte::Vector3<double> e2 = p2 - p0;
	gte::Vector3<double> cr = gte::Cross(e1, e2);
	area += gte::Length(cr) * 0.5;
    }
    return area;
}

static size_t
trimesh_remove_geometric_degenerate(
	std::vector<gte::Vector3<double>> const& vertices,
	std::vector<std::array<int32_t, 3>>& triangles)
{
    const size_t before = triangles.size();
    triangles.erase(std::remove_if(triangles.begin(), triangles.end(),
	[&vertices](std::array<int32_t, 3> const& triangle) {
	    gte::Vector3<double> e01 =
		vertices[triangle[1]] - vertices[triangle[0]];
	    gte::Vector3<double> e02 =
		vertices[triangle[2]] - vertices[triangle[0]];
	    gte::Vector3<double> e12 =
		vertices[triangle[2]] - vertices[triangle[1]];
	    double longest_squared = std::max(gte::Dot(e01, e01),
		std::max(gte::Dot(e02, e02), gte::Dot(e12, e12)));
	    if (!(longest_squared > 0.0))
		return true;
	    double doubled_area = gte::Length(gte::Cross(e01, e02));
	    return !(doubled_area > 64.0 *
		std::numeric_limits<double>::epsilon() * longest_squared);
	}), triangles.end());
    return before - triangles.size();
}

static bool
trimesh_repair_intersection(const point_t first[3],
	const point_t second[3])
{
    int coplanar = 0;
    point_t start = VINIT_ZERO;
    point_t end = VINIT_ZERO;
    if (!bg_tri_tri_isect_with_line(first[0], first[1], first[2],
	    second[0], second[1], second[2], &coplanar, &start, &end))
	return false;
    if (coplanar)
	return bg_tri_tri_isect_coplanar(first[0], first[1], first[2],
	    second[0], second[1], second[2], 1) > 0;

    double coordinate_scale = 1.0;
    for (int triangle = 0; triangle < 2; ++triangle) {
	const point_t *points = triangle ? second : first;
	for (int corner = 0; corner < 3; ++corner) {
	    for (int axis = 0; axis < 3; ++axis)
		coordinate_scale = std::max(coordinate_scale,
		    std::fabs((double)points[corner][axis]));
	}
    }
    const double endpoint_tolerance = 1024.0 *
	std::numeric_limits<double>::epsilon() * coordinate_scale;
    const auto endpoint_on_both = [&](const point_t endpoint) {
	const double first_distance = bg_tri_closest_pt(NULL, endpoint,
	    first[0], first[1], first[2]);
	const double second_distance = bg_tri_closest_pt(NULL, endpoint,
	    second[0], second[1], second[2]);
	return std::isfinite(first_distance) &&
	    std::isfinite(second_distance) &&
	    first_distance <= endpoint_tolerance &&
	    second_distance <= endpoint_tolerance;
    };
    return endpoint_on_both(start) || endpoint_on_both(end);
}

static bool
trimesh_repair_collect_candidate(size_t triangle, void *context)
{
    std::vector<size_t> *candidates =
	(std::vector<size_t> *)context;
    candidates->push_back(triangle);
    return true;
}

static size_t
trimesh_reject_intersecting_new_components(
	std::vector<gte::Vector3<double>> const& vertices,
	std::vector<std::array<int32_t, 3>>& triangles,
	size_t first_new_face, bool allow_self_intersections)
{
    if (first_new_face >= triangles.size())
	return 0;
    if (allow_self_intersections)
	return 0;

    typedef std::pair<int32_t, int32_t> edge_key;
    const size_t new_face_count = triangles.size() - first_new_face;
    std::vector<size_t> parent(new_face_count);
    for (size_t face = 0; face < new_face_count; ++face)
	parent[face] = face;
    auto find_root = [&parent](size_t member) {
	size_t root = member;
	while (parent[root] != root)
	    root = parent[root];
	while (parent[member] != member) {
	    const size_t next = parent[member];
	    parent[member] = root;
	    member = next;
	}
	return root;
    };
    const auto key = [](int32_t first, int32_t second) {
	return first < second ? edge_key(first, second) :
	    edge_key(second, first);
    };
    std::map<edge_key, size_t> edge_owner;
    for (size_t face = first_new_face; face < triangles.size(); ++face) {
	const size_t member = face - first_new_face;
	for (int edge = 0; edge < 3; ++edge) {
	    const edge_key current = key(triangles[face][edge],
		triangles[face][(edge + 1) % 3]);
	    const auto inserted = edge_owner.insert(
		std::make_pair(current, member));
	    if (inserted.second)
		continue;
	    const size_t first_root = find_root(member);
	    const size_t second_root = find_root(inserted.first->second);
	    if (first_root != second_root)
		parent[std::max(first_root, second_root)] =
		    std::min(first_root, second_root);
	}
    }
    for (size_t face = 0; face < new_face_count; ++face)
	parent[face] = find_root(face);

    std::vector<bool> rejected_component(new_face_count, false);
    RTree<size_t, double, 3> triangle_index;
    for (size_t face = 0; face < triangles.size(); ++face) {
	double minimum[3] = {
	    std::numeric_limits<double>::infinity(),
	    std::numeric_limits<double>::infinity(),
	    std::numeric_limits<double>::infinity()
	};
	double maximum[3] = {
	    -std::numeric_limits<double>::infinity(),
	    -std::numeric_limits<double>::infinity(),
	    -std::numeric_limits<double>::infinity()
	};
	point_t first_points[3];
	for (int corner = 0; corner < 3; ++corner) {
	    const gte::Vector3<double> &point =
		vertices[(size_t)triangles[face][corner]];
	    VSET(first_points[corner], point[0], point[1], point[2]);
	    for (int axis = 0; axis < 3; ++axis) {
		minimum[axis] = std::min(minimum[axis], point[axis]);
		maximum[axis] = std::max(maximum[axis], point[axis]);
	    }
	}
	if (face >= first_new_face) {
	    std::vector<size_t> candidates;
	    triangle_index.Search(minimum, maximum,
		trimesh_repair_collect_candidate, &candidates);
	    for (size_t candidate : candidates) {
		bool adjacent = false;
		for (int first_corner = 0; first_corner < 3 && !adjacent;
			++first_corner) {
		    for (int second_corner = 0; second_corner < 3;
			    ++second_corner) {
			if (triangles[face][first_corner] ==
				triangles[candidate][second_corner]) {
			    adjacent = true;
			    break;
			}
		    }
		}
		if (adjacent)
		    continue;
		point_t second_points[3];
		for (int corner = 0; corner < 3; ++corner) {
		    const gte::Vector3<double> &point =
			vertices[(size_t)triangles[candidate][corner]];
		    VSET(second_points[corner], point[0], point[1], point[2]);
		}
		if (!trimesh_repair_intersection(first_points, second_points))
		    continue;
		rejected_component[parent[face - first_new_face]] = true;
		if (candidate >= first_new_face)
		    rejected_component[parent[candidate - first_new_face]] =
			true;
	    }
	}
	triangle_index.Insert(minimum, maximum, face);
    }

    std::vector<std::array<int32_t, 3>> accepted;
    accepted.reserve(triangles.size());
    accepted.insert(accepted.end(), triangles.begin(),
	triangles.begin() + (ptrdiff_t)first_new_face);
    size_t rejected_faces = 0;
    for (size_t face = first_new_face; face < triangles.size(); ++face) {
	if (rejected_component[parent[face - first_new_face]]) {
	    rejected_faces++;
	    continue;
	}
	accepted.push_back(triangles[face]);
    }
    if (rejected_faces)
	triangles.swap(accepted);
    return rejected_faces;
}

static size_t
trimesh_split_hanging_boundary_edges(
	std::vector<gte::Vector3<double>> const& vertices,
	std::vector<std::array<int32_t, 3>>& triangles, double tolerance)
{
    if (!(tolerance > 0.0) || triangles.empty())
	return 0;
    typedef std::pair<int32_t, int32_t> edge_key;
    std::map<edge_key, size_t> edge_counts;
    const auto key = [](int32_t first, int32_t second) {
	return first < second ? edge_key(first, second) :
	    edge_key(second, first);
    };
    for (std::array<int32_t, 3> const& triangle : triangles) {
	for (int edge = 0; edge < 3; ++edge)
	    edge_counts[key(triangle[edge], triangle[(edge + 1) % 3])]++;
    }
    std::set<edge_key> unmatched_edges;
    std::set<int32_t> boundary_vertices;
    for (auto const& edge : edge_counts) {
	if (edge.second != 1)
	    continue;
	unmatched_edges.insert(edge.first);
	boundary_vertices.insert(edge.first.first);
	boundary_vertices.insert(edge.first.second);
    }
    if (unmatched_edges.empty())
	return 0;

    std::vector<std::array<int32_t, 3>> output;
    output.reserve(triangles.size());
    size_t added = 0;
    const double tolerance_squared = tolerance * tolerance;
    for (std::array<int32_t, 3> const& triangle : triangles) {
	bool split = false;
	for (int edge = 0; edge < 3 && !split; ++edge) {
	    const int32_t first = triangle[edge];
	    const int32_t second = triangle[(edge + 1) % 3];
	    const int32_t opposite = triangle[(edge + 2) % 3];
	    if (unmatched_edges.find(key(first, second)) ==
		    unmatched_edges.end())
		continue;
	    gte::Vector3<double> segment =
		vertices[second] - vertices[first];
	    const double length_squared = gte::Dot(segment, segment);
	    if (!(length_squared > tolerance_squared))
		continue;
	    std::vector<std::pair<double, int32_t>> candidates;
	    for (int32_t point : boundary_vertices) {
		if (point == first || point == second || point == opposite)
		    continue;
		gte::Vector3<double> offset =
		    vertices[point] - vertices[first];
		const double parameter = gte::Dot(offset, segment) /
		    length_squared;
		if (!(parameter > 0.0) || !(parameter < 1.0))
		    continue;
		const double along = parameter * std::sqrt(length_squared);
		if (along <= tolerance ||
		    std::sqrt(length_squared) - along <= tolerance)
		    continue;
		gte::Vector3<double> separation = offset -
		    parameter * segment;
		if (gte::Dot(separation, separation) > tolerance_squared)
		    continue;
		candidates.push_back(std::make_pair(parameter, point));
	    }
	    if (candidates.empty())
		continue;
	    std::sort(candidates.begin(), candidates.end(),
		[](std::pair<double, int32_t> const& a,
			std::pair<double, int32_t> const& b) {
		    if (a.first < b.first)
			return true;
		    if (b.first < a.first)
			return false;
		    return a.second < b.second;
		});
	    int32_t previous = first;
	    for (auto const& candidate : candidates) {
		if (candidate.second == previous)
		    continue;
		output.push_back({previous, candidate.second, opposite});
		previous = candidate.second;
		added++;
	    }
	    output.push_back({previous, second, opposite});
	    split = true;
	}
	if (!split)
	    output.push_back(triangle);
    }
    if (added)
	triangles.swap(output);
    return added;
}

static bool
trimesh_gte_valid_vertex_links(
	std::vector<std::array<int32_t, 3>> const& triangles,
	size_t vertex_count, size_t *invalid_count = NULL)
{
    typedef std::pair<int32_t, int32_t> link_edge;
    std::vector<std::vector<link_edge>> links(vertex_count);
    for (std::array<int32_t, 3> const& triangle : triangles) {
	for (int corner = 0; corner < 3; ++corner) {
	    const int32_t vertex = triangle[corner];
	    if (vertex < 0 || (size_t)vertex >= vertex_count) {
		if (invalid_count)
		    *invalid_count = 1;
		return false;
	    }
	    links[(size_t)vertex].push_back(link_edge(
		triangle[(corner + 1) % 3], triangle[(corner + 2) % 3]));
	}
    }
    size_t invalid_links = 0;
    for (std::vector<link_edge> const& vertex_links : links) {
	if (vertex_links.empty())
	    continue;
	std::map<int32_t, std::vector<int32_t>> adjacency;
	for (link_edge const& edge : vertex_links) {
	    adjacency[edge.first].push_back(edge.second);
	    adjacency[edge.second].push_back(edge.first);
	}
	std::set<int32_t> reached;
	std::queue<int32_t> work;
	work.push(adjacency.begin()->first);
	reached.insert(adjacency.begin()->first);
	bool valid_link = true;
	while (!work.empty()) {
	    const int32_t current = work.front();
	    work.pop();
	    std::vector<int32_t> const& neighbors = adjacency[current];
	    if (neighbors.size() != 2) {
		valid_link = false;
		break;
	    }
	    for (int32_t neighbor : neighbors) {
		if (reached.insert(neighbor).second)
		    work.push(neighbor);
	    }
	}
	if (!valid_link || reached.size() != adjacency.size())
	    invalid_links++;
    }
    if (invalid_count)
	*invalid_count = invalid_links;
    return !invalid_links;
}

static size_t
trimesh_topology_defect_score(
	std::vector<std::array<int32_t, 3>> const& triangles,
	size_t vertex_count)
{
    struct edge_use {
	size_t count = 0;
	int direction = 0;
    };
    typedef std::pair<int32_t, int32_t> edge_key;
    std::map<edge_key, edge_use> edges;
    for (std::array<int32_t, 3> const& triangle : triangles) {
	for (int edge = 0; edge < 3; ++edge) {
	    const int32_t first = triangle[edge];
	    const int32_t second = triangle[(edge + 1) % 3];
	    const edge_key key = first < second ?
		edge_key(first, second) : edge_key(second, first);
	    edge_use &use = edges[key];
	    use.count++;
	    use.direction += first < second ? 1 : -1;
	}
    }
    size_t score = 0;
    for (auto const& edge : edges) {
	const edge_use &use = edge.second;
	if (use.count == 1)
	    score++;
	else if (use.count > 2)
	    score += use.count - 2;
	else if (use.direction != 0)
	    score++;
    }
    size_t invalid_links = 0;
    trimesh_gte_valid_vertex_links(triangles, vertex_count,
	&invalid_links);
    return score + invalid_links;
}

static size_t
trimesh_separate_touching_vertices(
	std::vector<gte::Vector3<double>>& vertices,
	std::vector<std::array<int32_t, 3>> const& triangles,
	double distance, double *maximum_displacement)
{
    if (vertices.empty() || triangles.empty() || !(distance > 0.0))
	return 0;
    typedef std::array<double, 3> coordinate_key;
    std::map<coordinate_key, std::vector<int32_t>> coordinate_vertices;
    std::vector<gte::Vector3<double>> normals(vertices.size(),
	gte::Vector3<double>{0.0, 0.0, 0.0});
    std::vector<bool> used(vertices.size(), false);
    std::vector<std::vector<size_t>> incident_faces(vertices.size());
    for (size_t face = 0; face < triangles.size(); ++face) {
	std::array<int32_t, 3> const& triangle = triangles[face];
	const gte::Vector3<double> normal = gte::Cross(
	    vertices[(size_t)triangle[1]] - vertices[(size_t)triangle[0]],
	    vertices[(size_t)triangle[2]] - vertices[(size_t)triangle[0]]);
	for (int corner = 0; corner < 3; ++corner) {
	    const size_t vertex = (size_t)triangle[corner];
	    normals[vertex] += normal;
	    used[vertex] = true;
	    incident_faces[vertex].push_back(face);
	}
    }
    for (size_t vertex = 0; vertex < vertices.size(); ++vertex) {
	if (!used[vertex])
	    continue;
	coordinate_vertices[coordinate_key{
	    vertices[vertex][0], vertices[vertex][1], vertices[vertex][2]
	}].push_back((int32_t)vertex);
    }

    /* Candidate vertices move by less than distance.  Index boxes expanded by
     * that amount remain conservative for every trial, including movements
     * accepted for earlier duplicate groups. */
    RTree<size_t, double, 3> triangle_index;
    for (size_t face = 0; face < triangles.size(); ++face) {
	double minimum[3] = {
	    std::numeric_limits<double>::infinity(),
	    std::numeric_limits<double>::infinity(),
	    std::numeric_limits<double>::infinity()
	};
	double maximum[3] = {
	    -std::numeric_limits<double>::infinity(),
	    -std::numeric_limits<double>::infinity(),
	    -std::numeric_limits<double>::infinity()
	};
	for (int corner = 0; corner < 3; ++corner) {
	    const gte::Vector3<double> &point =
		vertices[(size_t)triangles[face][corner]];
	    for (int axis = 0; axis < 3; ++axis) {
		minimum[axis] = std::min(minimum[axis], point[axis] - distance);
		maximum[axis] = std::max(maximum[axis], point[axis] + distance);
	    }
	}
	triangle_index.Insert(minimum, maximum, face);
    }

    size_t separated = 0;
    double max_moved = 0.0;
    for (auto &entry : coordinate_vertices) {
	std::vector<int32_t> &group = entry.second;
	if (group.size() < 2)
	    continue;
	std::sort(group.begin(), group.end());
	std::vector<gte::Vector3<double>> directions;
	directions.reserve(group.size());
	bool valid_group = true;
	for (int32_t vertex : group) {
	    gte::Vector3<double> direction = normals[(size_t)vertex];
	    const double length = gte::Length(direction);
	    if (!(length > 0.0) || !std::isfinite(length)) {
		valid_group = false;
		break;
	    }
	    directions.push_back(direction / length);
	}
	if (!valid_group)
	    continue;

	std::set<size_t> affected_faces;
	for (int32_t vertex : group) {
	    affected_faces.insert(incident_faces[(size_t)vertex].begin(),
		incident_faces[(size_t)vertex].end());
	}
	const auto candidate_intersects = [&]() {
	    for (size_t face : affected_faces) {
		double minimum[3] = {
		    std::numeric_limits<double>::infinity(),
		    std::numeric_limits<double>::infinity(),
		    std::numeric_limits<double>::infinity()
		};
		double maximum[3] = {
		    -std::numeric_limits<double>::infinity(),
		    -std::numeric_limits<double>::infinity(),
		    -std::numeric_limits<double>::infinity()
		};
		point_t first_points[3];
		for (int corner = 0; corner < 3; ++corner) {
		    const gte::Vector3<double> &point =
			vertices[(size_t)triangles[face][corner]];
		    VSET(first_points[corner], point[0], point[1], point[2]);
		    for (int axis = 0; axis < 3; ++axis) {
			minimum[axis] = std::min(minimum[axis], point[axis]);
			maximum[axis] = std::max(maximum[axis], point[axis]);
		    }
		}
		std::vector<size_t> candidates;
		triangle_index.Search(minimum, maximum,
		    trimesh_repair_collect_candidate, &candidates);
		for (size_t candidate : candidates) {
		    if (candidate == face ||
			(affected_faces.count(candidate) && candidate < face))
			continue;
		    bool adjacent = false;
		    for (int first_corner = 0;
			    first_corner < 3 && !adjacent; ++first_corner) {
			for (int second_corner = 0; second_corner < 3;
				++second_corner) {
			    if (triangles[face][first_corner] ==
				    triangles[candidate][second_corner]) {
				adjacent = true;
				break;
			    }
			}
		    }
		    if (adjacent)
			continue;
		    point_t second_points[3];
		    for (int corner = 0; corner < 3; ++corner) {
			const gte::Vector3<double> &point =
			    vertices[(size_t)triangles[candidate][corner]];
			VSET(second_points[corner], point[0], point[1], point[2]);
		    }
		    if (trimesh_repair_intersection(first_points,
			    second_points))
			return true;
		}
	    }
	    return false;
	};

	/* Area-weighted inward normals are normally the safest direction, but a
	 * nonconvex fan can cross a neighboring fan when both move inward.  Try a
	 * bounded deterministic set of sign choices and retain only a locally
	 * intersection-free candidate. */
	std::vector<std::vector<bool>> sign_candidates;
	std::set<std::vector<bool>> unique_candidates;
	const auto add_candidate = [&](std::vector<bool> const& signs) {
	    if (unique_candidates.insert(signs).second)
		sign_candidates.push_back(signs);
	};
	add_candidate(std::vector<bool>(group.size(), false));
	for (size_t use = 0; use < group.size(); ++use) {
	    std::vector<bool> signs(group.size(), false);
	    signs[use] = true;
	    add_candidate(signs);
	}
	add_candidate(std::vector<bool>(group.size(), true));
	for (size_t use = 0; use < group.size(); ++use) {
	    std::vector<bool> signs(group.size(), true);
	    signs[use] = false;
	    add_candidate(signs);
	}

	bool accepted = false;
	for (std::vector<bool> const& signs : sign_candidates) {
	    for (size_t use = 0; use < group.size(); ++use) {
		const double displacement = distance * (double)(use + 1) /
		    (double)(group.size() + 1);
		const double sign = signs[use] ? 1.0 : -1.0;
		vertices[(size_t)group[use]] = gte::Vector3<double>{
		    entry.first[0], entry.first[1], entry.first[2]
		} + sign * directions[use] * displacement;
	    }
	    if (!candidate_intersects()) {
		accepted = true;
		break;
	    }
	}
	if (!accepted) {
	    for (int32_t vertex : group) {
		vertices[(size_t)vertex] = gte::Vector3<double>{
		    entry.first[0], entry.first[1], entry.first[2]
		};
	    }
	    continue;
	}
	for (size_t use = 0; use < group.size(); ++use) {
	    max_moved = std::max(max_moved, distance * (double)(use + 1) /
		(double)(group.size() + 1));
	    separated++;
	}
    }
    if (maximum_displacement)
	*maximum_displacement = max_moved;
    return separated;
}

static bool
trimesh_gte_solid(std::vector<gte::Vector3<double>> const& vertices,
	std::vector<std::array<int32_t, 3>> const& triangles)
{
    if (vertices.empty() || triangles.empty() || vertices.size() > INT_MAX ||
	    triangles.size() > INT_MAX)
	return false;
    std::vector<fastf_t> points(vertices.size() * 3);
    std::vector<int> faces(triangles.size() * 3);
    for (size_t vertex = 0; vertex < vertices.size(); ++vertex) {
	for (int axis = 0; axis < 3; ++axis)
	    points[vertex * 3 + (size_t)axis] = vertices[vertex][axis];
    }
    for (size_t face = 0; face < triangles.size(); ++face) {
	for (int corner = 0; corner < 3; ++corner)
	    faces[face * 3 + (size_t)corner] = triangles[face][corner];
    }
    return !bg_trimesh_solid2((int)vertices.size(), (int)triangles.size(),
	points.data(), faces.data(), NULL);
}

static bool
trimesh_manifold_union(
	std::vector<gte::Vector3<double>>& vertices,
	std::vector<std::array<int32_t, 3>>& triangles,
	bool *manifold_accepted, bool perform_union)
{
    manifold::MeshGL64 mesh;
    mesh.vertProperties.reserve(vertices.size() * 3);
    mesh.triVerts.reserve(triangles.size() * 3);
    for (gte::Vector3<double> const& vertex : vertices) {
	mesh.vertProperties.push_back(vertex[0]);
	mesh.vertProperties.push_back(vertex[1]);
	mesh.vertProperties.push_back(vertex[2]);
    }
    for (std::array<int32_t, 3> const& triangle : triangles) {
	for (int corner = 0; corner < 3; ++corner)
	    mesh.triVerts.push_back((uint64_t)triangle[corner]);
    }
    manifold::Manifold input(mesh);
    if (input.Status() != manifold::Manifold::Error::NoError)
	return false;
    if (manifold_accepted)
	*manifold_accepted = true;
    if (!perform_union)
	return false;
    std::vector<manifold::Manifold> components = input.Decompose();
    if (components.size() < 2)
	return false;
    manifold::Manifold united = manifold::Manifold::BatchBoolean(
	components, manifold::OpType::Add);
    if (united.Status() != manifold::Manifold::Error::NoError)
	return false;
    united = united.Simplify();
    if (united.Status() != manifold::Manifold::Error::NoError)
	return false;
    manifold::MeshGL64 result = united.GetMeshGL64();
    if (result.numProp < 3 || result.vertProperties.empty() ||
	    result.triVerts.empty() ||
	    result.vertProperties.size() % result.numProp ||
	    result.triVerts.size() % 3 ||
	    result.vertProperties.size() / result.numProp > INT_MAX ||
	    result.triVerts.size() / 3 > INT_MAX)
	return false;

    const size_t property_vertex_count =
	result.vertProperties.size() / result.numProp;
    if (result.mergeFromVert.size() != result.mergeToVert.size())
	return false;

    /* MeshGL may duplicate a geometric vertex when non-position properties
     * differ.  Its merge vectors are the authoritative indexed topology;
     * dropping them turns otherwise manifold output into coincident cracks
     * and false nonadjacent intersections. */
    std::vector<size_t> parent(property_vertex_count);
    for (size_t vertex = 0; vertex < property_vertex_count; ++vertex)
	parent[vertex] = vertex;
    auto find_root = [&parent](size_t vertex) {
	size_t root = vertex;
	while (parent[root] != root)
	    root = parent[root];
	while (parent[vertex] != vertex) {
	    const size_t next = parent[vertex];
	    parent[vertex] = root;
	    vertex = next;
	}
	return root;
    };
    for (size_t merge = 0; merge < result.mergeFromVert.size(); ++merge) {
	const uint64_t from = result.mergeFromVert[merge];
	const uint64_t to = result.mergeToVert[merge];
	if (from >= property_vertex_count || to >= property_vertex_count)
	    return false;
	const size_t from_root = find_root((size_t)from);
	const size_t to_root = find_root((size_t)to);
	if (from_root != to_root) {
	    const size_t keep = std::min(from_root, to_root);
	    const size_t remove = std::max(from_root, to_root);
	    parent[remove] = keep;
	}
    }
    for (size_t vertex = 0; vertex < property_vertex_count; ++vertex)
	parent[vertex] = find_root(vertex);

    std::vector<int32_t> compact_index(property_vertex_count, -1);
    std::vector<gte::Vector3<double>> union_vertices;
    union_vertices.reserve(property_vertex_count);
    for (uint64_t index : result.triVerts) {
	if (index >= property_vertex_count)
	    return false;
	const size_t root = parent[(size_t)index];
	if (compact_index[root] >= 0)
	    continue;
	if (union_vertices.size() >= (size_t)INT32_MAX)
	    return false;
	compact_index[root] = (int32_t)union_vertices.size();
	gte::Vector3<double> vertex;
	for (int axis = 0; axis < 3; ++axis) {
	    const double coordinate =
		result.vertProperties[root * result.numProp + (size_t)axis];
	    if (!std::isfinite(coordinate))
		return false;
	    vertex[axis] = coordinate;
	}
	union_vertices.push_back(vertex);
    }
    std::vector<std::array<int32_t, 3>> union_triangles(
	result.triVerts.size() / 3);
    for (size_t face = 0; face < union_triangles.size(); ++face) {
	for (int corner = 0; corner < 3; ++corner) {
	    const uint64_t index = result.triVerts[face * 3 + (size_t)corner];
	    union_triangles[face][corner] =
		compact_index[parent[(size_t)index]];
	}
	if (union_triangles[face][0] == union_triangles[face][1] ||
		union_triangles[face][1] == union_triangles[face][2] ||
		union_triangles[face][2] == union_triangles[face][0])
	    return false;
    }
    std::vector<std::array<int32_t, 3>> nondegenerate_check =
	union_triangles;
    if (trimesh_remove_geometric_degenerate(union_vertices,
	    nondegenerate_check))
	return false;
    if (!trimesh_gte_solid(union_vertices, union_triangles) ||
	    !trimesh_gte_valid_vertex_links(union_triangles,
	    union_vertices.size()))
	return false;
    vertices.swap(union_vertices);
    triangles.swap(union_triangles);
    return true;
}

static int
trimesh_repair_export(int **ofaces, int *n_ofaces,
	point_t **opnts, int *n_opnts,
	const std::vector<gte::Vector3<double>> &vertices,
	const std::vector<std::array<int32_t, 3>> &triangles,
	const struct bg_trimesh_repair_settings *settings,
	struct bg_trimesh_repair_report *report)
{
    const int vertex_count = (int)vertices.size();
    const int face_count = (int)triangles.size();
    point_t *output_points = (point_t *)bu_calloc((size_t)vertex_count,
	sizeof(point_t), "bg_trimesh_repair verts");
    int *output_faces = (int *)bu_calloc((size_t)face_count * 3,
	sizeof(int), "bg_trimesh_repair faces");

    for (int vertex = 0; vertex < vertex_count; ++vertex) {
	output_points[vertex][X] = vertices[(size_t)vertex][0];
	output_points[vertex][Y] = vertices[(size_t)vertex][1];
	output_points[vertex][Z] = vertices[(size_t)vertex][2];
    }
    for (int face = 0; face < face_count; ++face) {
	output_faces[3 * face] = triangles[(size_t)face][0];
	output_faces[3 * face + 1] = triangles[(size_t)face][1];
	output_faces[3 * face + 2] = triangles[(size_t)face][2];
    }

    *opnts = output_points;
    *n_opnts = vertex_count;
    *ofaces = output_faces;
    *n_ofaces = face_count;
    report->output_vertices = vertex_count;
    report->output_faces = face_count;
    report->output_area = trimesh_gte_area(vertices, triangles);

    std::vector<std::array<int32_t, 3>> geometric_check = triangles;
    report->geometric_degenerate_faces =
	(int)trimesh_remove_geometric_degenerate(vertices, geometric_check);
    size_t invalid_vertex_links = 0;
    trimesh_gte_valid_vertex_links(triangles, vertices.size(),
	&invalid_vertex_links);
    report->invalid_vertex_links = (int)invalid_vertex_links;
    struct bg_trimesh_solid_errors solid_errors =
	BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    const int not_solid = bg_trimesh_solid2(vertex_count, face_count,
	(fastf_t *)output_points, output_faces, &solid_errors);
    report->unmatched_edges = solid_errors.unmatched.count;
    report->excess_edges = solid_errors.excess.count;
    report->misoriented_edges = solid_errors.misoriented.count;
    if (solid_errors.degenerate.count > report->geometric_degenerate_faces)
	report->geometric_degenerate_faces = solid_errors.degenerate.count;
    bg_free_trimesh_solid_errors(&solid_errors);
    report->solid = !report->geometric_degenerate_faces &&
	!report->invalid_vertex_links && !not_solid;
    if (settings->require_solid && !report->solid) {
	bu_free(output_faces, "bg_trimesh_repair faces");
	bu_free(output_points, "bg_trimesh_repair verts");
	*ofaces = NULL;
	*n_ofaces = 0;
	*opnts = NULL;
	*n_opnts = 0;
	return -1;
    }
    if (report->solid)
	report->output_volume = bg_trimesh_volume(output_faces,
	    (size_t)face_count, output_points, (size_t)vertex_count);
    return 0;
}


/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

extern "C" int
bg_trimesh_repair2(
	int **ofaces, int *n_ofaces,
	point_t **opnts, int *n_opnts,
	const int *ifaces, int n_ifaces,
	const point_t *ipnts, int n_ipnts,
	const struct bg_trimesh_repair_settings *settings,
	struct bg_trimesh_repair_report *report)
{
    struct bg_trimesh_repair_report local_report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    if (!report)
	report = &local_report;
    *report = local_report;
    report->input_vertices = n_ipnts;
    report->input_faces = n_ifaces;

    if (!ofaces || !n_ofaces || !opnts || !n_opnts)
	return -1;
    if (!ifaces || n_ifaces <= 0 || !ipnts || n_ipnts <= 0)
	return -1;
    for (int vertex = 0; vertex < n_ipnts; ++vertex) {
	if (!std::isfinite(ipnts[vertex][X]) ||
		!std::isfinite(ipnts[vertex][Y]) ||
		!std::isfinite(ipnts[vertex][Z]))
	    return -1;
    }
    for (size_t corner = 0; corner < (size_t)n_ifaces * 3; ++corner) {
	if (ifaces[corner] < 0 || ifaces[corner] >= n_ipnts)
	    return -1;
    }

    /* Initialize output pointers */
    *ofaces = NULL;
    *n_ofaces = 0;
    *opnts = NULL;
    *n_opnts = 0;

    struct bg_trimesh_repair_settings default_settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    if (!settings)
	settings = &default_settings;
    report->self_intersections_allowed =
	settings->allow_self_intersections != 0;
    if (!std::isfinite(settings->vertex_tolerance) ||
	    settings->vertex_tolerance < 0.0 ||
	    !std::isfinite(settings->max_component_area) ||
	    settings->max_component_area < 0.0 ||
	    !std::isfinite(settings->max_component_area_percent) ||
	    settings->max_component_area_percent < 0.0 ||
	    !std::isfinite(settings->max_hole_area) ||
	    settings->max_hole_area < 0.0 ||
	    !std::isfinite(settings->max_hole_area_percent) ||
	    settings->max_hole_area_percent < 0.0 ||
	    settings->max_iterations < 0)
	return -1;

    /* A topological solid may still contain a triangle whose distinct
     * vertices are geometrically collinear.  Delay the already-solid return
     * until that independent condition has also been checked. */
    int not_solid = bg_trimesh_solid2(n_ipnts, n_ifaces,
				      (fastf_t *)ipnts, (int *)ifaces,
				      NULL);

    /* Convert input arrays to GTE types. */
    std::vector<gte::Vector3<double>> verts((size_t)n_ipnts);
    for (int i = 0; i < n_ipnts; i++) {
	verts[i][0] = ipnts[i][X];
	verts[i][1] = ipnts[i][Y];
	verts[i][2] = ipnts[i][Z];
    }
    std::vector<std::array<int32_t, 3>> tris((size_t)n_ifaces);
    for (int i = 0; i < n_ifaces; i++) {
	tris[i][0] = ifaces[3*i+0];
	tris[i][1] = ifaces[3*i+1];
	tris[i][2] = ifaces[3*i+2];
    }
    report->input_area = trimesh_gte_area(verts, tris);
    const size_t initial_geometric_degenerate =
	trimesh_remove_geometric_degenerate(verts, tris);
    if (!not_solid && !initial_geometric_degenerate &&
	    !settings->separate_touching_vertices &&
	    !settings->union_components && !settings->require_manifold) {
	report->output_vertices = n_ipnts;
	report->output_faces = n_ifaces;
	report->solid = 1;
	report->output_area = report->input_area;
	report->output_volume = bg_trimesh_volume(ifaces,
	    (size_t)n_ifaces, ipnts, (size_t)n_ipnts);
	return 1;
    }
    report->removed_faces += (int)initial_geometric_degenerate;
    if (tris.empty())
	return -1;

    /* --- Pass 1: initial colocate + degenerate removal ------------------- */
    const double bbox_diag = trimesh_gte_bbox_diag(verts);
    const double vertex_tolerance = settings->vertex_tolerance > 0.0 ?
	settings->vertex_tolerance :
	((bbox_diag > 0.0) ? 1e-8 * bbox_diag : 0.0);
    /* Moving disconnected duplicate-coordinate fans does not require the
     * destructive repair sequence when the input is already an indexed
     * solid.  Recolocating first would merge a point contact into a
     * non-manifold vertex and a later split could reopen seams. */
    if (!not_solid && !initial_geometric_degenerate &&
	    settings->separate_touching_vertices &&
	    !settings->remove_small_components && !settings->fill_holes &&
	    !settings->union_components && !settings->require_manifold) {
	double displacement = 0.0;
	report->separated_vertices = (int)trimesh_separate_touching_vertices(
	    verts, tris, vertex_tolerance, &displacement);
	report->max_vertex_displacement = displacement;
	return trimesh_repair_export(ofaces, n_ofaces, opnts, n_opnts,
	    verts, tris, settings, report);
    }
    {
	gte::MeshRepair<double>::Parameters rp;
	rp.epsilon = vertex_tolerance;
	const size_t before = tris.size();
	gte::MeshRepair<double>::Repair(verts, tris, rp);
	report->removed_faces += (int)(before - tris.size());
    }
    report->removed_faces +=
	(int)trimesh_remove_geometric_degenerate(verts, tris);

    if (tris.empty())
	return -1;

    /* A fast fallback face and a rigorous neighboring face can sample their
     * shared curved edge at different densities.  Colocation alone leaves a
     * long unmatched edge opposite a chain of shorter edges.  Split such
     * boundary edges at nearby boundary vertices before classifying holes, so
     * repair closes T-junctions instead of capping both sides of the same
     * narrow crack. */
    const int max_hanging_iterations = settings->max_iterations > 0 ?
	settings->max_iterations : 10;
    for (int iteration = 0; iteration < max_hanging_iterations; ++iteration) {
	const size_t added = trimesh_split_hanging_boundary_edges(verts, tris,
	    vertex_tolerance);
	if (!added)
	    break;
	report->added_faces += (int)added;
	report->removed_faces +=
	    (int)trimesh_remove_geometric_degenerate(verts, tris);
	if (tris.empty())
	    return -1;
    }

    /* --- Pass 2: remove small disconnected components -------------------- */
    if (settings->remove_small_components) {
	double area = trimesh_gte_area(verts, tris);
	double min_comp_area = settings->max_component_area > SMALL_FASTF ?
	    settings->max_component_area : area *
	    (settings->max_component_area_percent / 100.0);
	if (min_comp_area > 0.0) {
	    size_t nf_before = tris.size();
	    gte::MeshPreprocessing<double>::RemoveSmallComponents(verts, tris, min_comp_area);
	    if (tris.size() != nf_before) {
		report->removed_faces += (int)(nf_before - tris.size());
		/* Re-run basic repair after component removal. */
		double component_bbox_diag = trimesh_gte_bbox_diag(verts);
		gte::MeshRepair<double>::Parameters rp;
		rp.epsilon = settings->vertex_tolerance > 0.0 ?
		    settings->vertex_tolerance :
		    ((component_bbox_diag > 0.0) ?
		    1e-8 * component_bbox_diag : 0.0);
		const size_t before = tris.size();
		gte::MeshRepair<double>::Repair(verts, tris, rp);
		report->removed_faces += (int)(before - tris.size());
	    }
	}
    }

    if (tris.empty())
	return -1;

    /* --- Pass 2b: pre-fill topology normalisation -----------------------
     * The old Geogram-based repair called mesh_repair(MESH_REPAIR_DEFAULT)
     * which includes G4 (connect) + G5 (reorient) + G6 (split non-manifold)
     * BEFORE hole filling.  Without this, GTE's hole-boundary tracer can
     * encounter non-simple (self-touching) boundary loops that it aborts on,
     * leaving those holes unfilled.  Calling the full G4-G6 sequence here
     * fixes non-manifold vertices first so hole detection sees clean,
     * simple boundary loops. */
    {
	std::vector<int32_t> adj;
	gte::MeshRepair<double>::ConnectFacets(tris, adj);
	gte::MeshRepair<double>::ReorientFacetsAntiMoebius(verts, tris, adj);

	gte::MeshRepair<double>::ConnectFacets(tris, adj);
	gte::MeshRepair<double>::SplitNonManifoldVertices(verts, tris, adj);
    }

    /* --- Pass 3 + 4: iterative hole-fill + topology-repair loop -----------
     * Run fill-then-repair repeatedly.  Each post-fill round of G4-G6 may
     * expose previously blocked hole boundaries (non-simple loops caused by
     * non-manifold vertices) so subsequent FillHoles passes can fill them.
     * Stop when no new faces are added (convergence) or after a safety limit.
     *
     * This mirrors the old Geogram approach:
     *   GEO::fill_holes(gm, hole_size);
     *   GEO::mesh_repair(gm, GEO::MESH_REPAIR_DEFAULT);  // G1-G8 incl. G4-G6
     * where a single pass was sufficient because Geogram's fill_holes handles
     * complex boundary loops natively.  GTE's FillHoles requires the extra
     * G4-G6 steps to untangle the topology between iterations. */
    const int max_iterations = settings->max_iterations > 0 ?
	settings->max_iterations : 10;
    for (int iter = 0; iter < max_iterations; ++iter) {
	size_t nf_before = tris.size();

	/* Pass 3: hole filling */
	if (settings->fill_holes) {
	    double area = trimesh_gte_area(verts, tris);

	    double hole_limit = 1e30; /* default: attempt to fill all holes */
	    if (settings->max_hole_area > SMALL_FASTF) {
		hole_limit = (double)settings->max_hole_area;
	    } else if (settings->max_hole_area_percent > SMALL_FASTF) {
		hole_limit = area *
		    ((double)settings->max_hole_area_percent / 100.0);
	    }

	    gte::MeshHoleFilling<double>::Parameters fp;
	    fp.maxArea      = hole_limit;
	    fp.maxEdges     = settings->max_hole_edges;
	    fp.method = gte::MeshHoleFilling<double>::
		TriangulationMethod::PlanarProjection;
	    fp.autoFallback = false;
	    const size_t vertex_count_before_fill = verts.size();
	    gte::MeshHoleFilling<double>::FillHoles(verts, tris, fp);
	    /* A parameter-space-valid cap can still cut through the surrounding
	     * mesh.  Keep independent safe caps, then retry remaining boundaries
	     * with circle-mapped and direct 3-D triangulations. */
	    const size_t planar_rejected =
		trimesh_reject_intersecting_new_components(verts, tris,
		    nf_before, settings->allow_self_intersections != 0);
	    report->rejected_hole_faces += (int)planar_rejected;
	    if (planar_rejected && tris.size() == nf_before)
		verts.resize(vertex_count_before_fill);

	    const size_t lscm_face_start = tris.size();
	    const size_t lscm_vertex_start = verts.size();
	    fp.method = gte::MeshHoleFilling<double>::
		TriangulationMethod::LSCM;
	    fp.autoFallback = false;
	    gte::MeshHoleFilling<double>::FillHoles(verts, tris, fp);
	    const size_t lscm_rejected =
		trimesh_reject_intersecting_new_components(verts, tris,
		    lscm_face_start,
		    settings->allow_self_intersections != 0);
	    report->rejected_hole_faces += (int)lscm_rejected;
	    if (lscm_rejected && tris.size() == lscm_face_start)
		verts.resize(lscm_vertex_start);

	    const size_t ear_face_start = tris.size();
	    const size_t ear_vertex_start = verts.size();
	    fp.method = gte::MeshHoleFilling<double>::
		TriangulationMethod::EarClipping3D;
	    fp.autoFallback = false;
	    fp.maxValidatedEdges = 2048;
	    RTree<size_t, double, 3> ear_triangle_index;
	    size_t indexed_ear_faces = 0;
	    const auto index_ear_faces = [&]() {
		while (indexed_ear_faces < tris.size()) {
		    double minimum[3] = {
			std::numeric_limits<double>::infinity(),
			std::numeric_limits<double>::infinity(),
			std::numeric_limits<double>::infinity()
		    };
		    double maximum[3] = {
			-std::numeric_limits<double>::infinity(),
			-std::numeric_limits<double>::infinity(),
			-std::numeric_limits<double>::infinity()
		    };
		    for (int corner = 0; corner < 3; ++corner) {
			const gte::Vector3<double> &point = verts[(size_t)
			    tris[indexed_ear_faces][corner]];
			for (int axis = 0; axis < 3; ++axis) {
			    minimum[axis] = std::min(minimum[axis], point[axis]);
			    maximum[axis] = std::max(maximum[axis], point[axis]);
			}
		    }
		    ear_triangle_index.Insert(minimum, maximum,
			indexed_ear_faces);
		    indexed_ear_faces++;
		}
	    };
	    index_ear_faces();
	    fp.triangleValidator = [&](const std::array<int32_t, 3> &candidate,
		    const std::vector<std::array<int32_t, 3>> &accepted) {
		index_ear_faces();
		point_t candidate_points[3];
		double minimum[3] = {
		    std::numeric_limits<double>::infinity(),
		    std::numeric_limits<double>::infinity(),
		    std::numeric_limits<double>::infinity()
		};
		double maximum[3] = {
		    -std::numeric_limits<double>::infinity(),
		    -std::numeric_limits<double>::infinity(),
		    -std::numeric_limits<double>::infinity()
		};
		for (int corner = 0; corner < 3; ++corner) {
		    const gte::Vector3<double> &point =
			verts[(size_t)candidate[corner]];
		    VSET(candidate_points[corner], point[0], point[1], point[2]);
		    for (int axis = 0; axis < 3; ++axis) {
			minimum[axis] = std::min(minimum[axis], point[axis]);
			maximum[axis] = std::max(maximum[axis], point[axis]);
		    }
		}
		std::vector<size_t> candidates;
		ear_triangle_index.Search(minimum, maximum,
		    trimesh_repair_collect_candidate, &candidates);
		const auto intersects = [&](const std::array<int32_t, 3> &other) {
		    for (int first_corner = 0; first_corner < 3; ++first_corner) {
			for (int second_corner = 0; second_corner < 3;
				++second_corner) {
			    if (candidate[first_corner] == other[second_corner])
				return false;
			}
		    }
		    point_t other_points[3];
		    for (int corner = 0; corner < 3; ++corner) {
			const gte::Vector3<double> &point =
			    verts[(size_t)other[corner]];
			VSET(other_points[corner], point[0], point[1], point[2]);
		    }
		    return trimesh_repair_intersection(candidate_points,
			other_points);
		};
		for (size_t face : candidates) {
		    if (intersects(tris[face]))
			return false;
		}
		for (const std::array<int32_t, 3> &triangle : accepted) {
		    if (intersects(triangle))
			return false;
		}
		return true;
	    };
	    if (settings->allow_self_intersections)
		fp.triangleValidator = {};
	    gte::MeshHoleFilling<double>::FillHoles(verts, tris, fp);
	    const size_t ear_rejected =
		trimesh_reject_intersecting_new_components(verts, tris,
		    ear_face_start, settings->allow_self_intersections != 0);
	    report->rejected_hole_faces += (int)ear_rejected;
	    if (ear_rejected && tris.size() == ear_face_start)
		verts.resize(ear_vertex_start);

	    /* A non-planar hole can require an interior vertex when every
	     * boundary diagonal intersects the surrounding mesh.  Rebuild the
	     * collision index after the ear attempt, then try one bounded
	     * centroid fan before leaving the boundary open. */
	    ear_triangle_index.RemoveAll();
	    indexed_ear_faces = 0;
	    index_ear_faces();
	    const size_t steiner_face_start = tris.size();
	    const size_t steiner_vertex_start = verts.size();
	    fp.method = gte::MeshHoleFilling<double>::
		TriangulationMethod::SteinerFan;
	    gte::MeshHoleFilling<double>::FillHoles(verts, tris, fp);
	    const size_t steiner_rejected =
		trimesh_reject_intersecting_new_components(verts, tris,
		    steiner_face_start,
		    settings->allow_self_intersections != 0);
	    report->rejected_hole_faces += (int)steiner_rejected;
	    if (steiner_rejected && tris.size() == steiner_face_start)
		verts.resize(steiner_vertex_start);
	    if (tris.size() == nf_before)
		verts.resize(vertex_count_before_fill);
	    report->added_faces += (int)(tris.size() - nf_before);
	}

	if (tris.empty())
	    return -1;
	report->removed_faces +=
	    (int)trimesh_remove_geometric_degenerate(verts, tris);
	if (tris.empty())
	    return -1;

	/* Pass 4: post-fill topology repair (G4+G5+G6+G8) */
	{
	    std::vector<int32_t> adj;
	    gte::MeshRepair<double>::ConnectFacets(tris, adj);
	    gte::MeshRepair<double>::ReorientFacetsAntiMoebius(verts, tris, adj);
	    gte::MeshRepair<double>::ConnectFacets(tris, adj);
	    gte::MeshRepair<double>::SplitNonManifoldVertices(verts, tris, adj);
	}
	gte::MeshPreprocessing<double>::OrientNormals(verts, tris);
	bool welded_progress = false;
	if (!settings->separate_touching_vertices) {
	    const size_t before_score = trimesh_topology_defect_score(tris,
		verts.size());
	    std::vector<gte::Vector3<double>> welded_vertices = verts;
	    std::vector<std::array<int32_t, 3>> welded_triangles = tris;
	    gte::MeshRepair<double>::Parameters rp;
	    rp.epsilon = vertex_tolerance;
	    gte::MeshRepair<double>::Repair(welded_vertices,
		welded_triangles, rp);
	    const size_t after_score = trimesh_topology_defect_score(
		welded_triangles, welded_vertices.size());
	    if (after_score < before_score) {
		if (tris.size() > welded_triangles.size())
		    report->removed_faces +=
			(int)(tris.size() - welded_triangles.size());
		verts.swap(welded_vertices);
		tris.swap(welded_triangles);
		welded_progress = true;
	    }
	}
	report->repair_iterations = iter + 1;

	/* A defect-reducing re-weld can expose a simpler boundary without adding
	 * faces.  Give that boundary one more bounded fill pass before declaring
	 * convergence. */
	if (tris.size() == nf_before && !welded_progress)
	    break;
    }

    /* Splitting a non-manifold boundary vertex before filling gives the hole
     * tracer simple loops, but it leaves duplicate coordinates after those
     * loops have been closed.  Re-weld them only when the resulting indexed
     * mesh is itself a closed solid and every vertex has one circular link.
     * This reconnects intended patch seams while preserving genuinely
     * separate shells that merely touch at a point. */
    if (!settings->separate_touching_vertices) {
	std::vector<gte::Vector3<double>> welded_vertices = verts;
	std::vector<std::array<int32_t, 3>> welded_triangles = tris;
	gte::MeshRepair<double>::Parameters rp;
	rp.epsilon = vertex_tolerance;
	gte::MeshRepair<double>::Repair(welded_vertices, welded_triangles, rp);
	if (trimesh_gte_solid(welded_vertices, welded_triangles) &&
		trimesh_gte_valid_vertex_links(welded_triangles,
		welded_vertices.size())) {
	    if (tris.size() > welded_triangles.size())
		report->removed_faces +=
		    (int)(tris.size() - welded_triangles.size());
	    verts.swap(welded_vertices);
	    tris.swap(welded_triangles);
	}
    }

    if (settings->separate_touching_vertices) {
	double displacement = 0.0;
	report->separated_vertices = (int)trimesh_separate_touching_vertices(
	    verts, tris, vertex_tolerance, &displacement);
	report->max_vertex_displacement = displacement;
    }

    if (settings->union_components) {
	const size_t faces_before_union = tris.size();
	bool manifold_accepted = false;
	if (trimesh_manifold_union(verts, tris, &manifold_accepted, true)) {
	    report->component_union_applied = 1;
	    if (tris.size() > faces_before_union)
		report->added_faces +=
		    (int)(tris.size() - faces_before_union);
	    else
		report->removed_faces +=
		    (int)(faces_before_union - tris.size());
	}
	report->manifold_accepted = manifold_accepted;
    }

    if (settings->require_manifold && !report->manifold_accepted) {
	bool manifold_accepted = false;
	std::vector<gte::Vector3<double>> manifold_vertices = verts;
	std::vector<std::array<int32_t, 3>> manifold_triangles = tris;
	(void)trimesh_manifold_union(manifold_vertices, manifold_triangles,
	    &manifold_accepted, false);
	report->manifold_accepted = manifold_accepted;
	if (!manifold_accepted)
	    return -1;
    }

    return trimesh_repair_export(ofaces, n_ofaces, opnts, n_opnts,
	verts, tris, settings, report);
}

extern "C" int
bg_trimesh_repair(
	int **ofaces, int *n_ofaces,
	point_t **opnts, int *n_opnts,
	const int *ifaces, int n_ifaces,
	const point_t *ipnts, int n_ipnts,
	struct bg_trimesh_repair_opts *opts)
{
    struct bg_trimesh_repair_opts default_opts =
	BG_TRIMESH_REPAIR_OPTS_DEFAULT;
    if (!opts)
	opts = &default_opts;

    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.remove_small_components = 1;
    settings.max_component_area_percent = 3.0;
    settings.fill_holes = 1;
    settings.max_hole_area = opts->max_hole_area;
    settings.max_hole_area_percent = opts->max_hole_area_percent;
    return bg_trimesh_repair2(ofaces, n_ofaces, opnts, n_opnts,
	ifaces, n_ifaces, ipnts, n_ipnts, &settings, NULL);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
