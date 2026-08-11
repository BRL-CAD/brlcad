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
#include "bg/trimesh.h"

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
	size_t vertex_count)
{
    typedef std::pair<int32_t, int32_t> link_edge;
    std::vector<std::vector<link_edge>> links(vertex_count);
    for (std::array<int32_t, 3> const& triangle : triangles) {
	for (int corner = 0; corner < 3; ++corner) {
	    const int32_t vertex = triangle[corner];
	    if (vertex < 0 || (size_t)vertex >= vertex_count)
		return false;
	    links[(size_t)vertex].push_back(link_edge(
		triangle[(corner + 1) % 3], triangle[(corner + 2) % 3]));
	}
    }
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
	while (!work.empty()) {
	    const int32_t current = work.front();
	    work.pop();
	    std::vector<int32_t> const& neighbors = adjacency[current];
	    if (neighbors.size() != 2)
		return false;
	    for (int32_t neighbor : neighbors) {
		if (reached.insert(neighbor).second)
		    work.push(neighbor);
	    }
	}
	if (reached.size() != adjacency.size())
	    return false;
    }
    return true;
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
    for (std::array<int32_t, 3> const& triangle : triangles) {
	const gte::Vector3<double> normal = gte::Cross(
	    vertices[(size_t)triangle[1]] - vertices[(size_t)triangle[0]],
	    vertices[(size_t)triangle[2]] - vertices[(size_t)triangle[0]]);
	for (int corner = 0; corner < 3; ++corner) {
	    const size_t vertex = (size_t)triangle[corner];
	    normals[vertex] += normal;
	    used[vertex] = true;
	}
    }
    for (size_t vertex = 0; vertex < vertices.size(); ++vertex) {
	if (!used[vertex])
	    continue;
	coordinate_vertices[coordinate_key{
	    vertices[vertex][0], vertices[vertex][1], vertices[vertex][2]
	}].push_back((int32_t)vertex);
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
	for (size_t use = 0; use < group.size(); ++use) {
	    const double displacement = distance * (double)(use + 1) /
		(double)(group.size() + 1);
	    vertices[(size_t)group[use]] += directions[use] * displacement;
	    max_moved = std::max(max_moved, displacement);
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
	std::vector<std::array<int32_t, 3>>& triangles)
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
	    !settings->union_components) {
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
	    fp.method       = gte::MeshHoleFilling<double>::TriangulationMethod::LSCM;
	    fp.autoFallback = true;
	    gte::MeshHoleFilling<double>::FillHoles(verts, tris, fp);
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
	report->repair_iterations = iter + 1;

	/* Convergence: stop when no new faces were added */
	if (tris.size() == nf_before)
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
	if (trimesh_manifold_union(verts, tris)) {
	    report->component_union_applied = 1;
	    if (tris.size() > faces_before_union)
		report->added_faces +=
		    (int)(tris.size() - faces_before_union);
	    else
		report->removed_faces +=
		    (int)(faces_before_union - tris.size());
	}
    }

    /* --- Build output arrays --------------------------------------------- */
    int nv = (int)verts.size();
    int nf = (int)tris.size();

    point_t *out_pts = (point_t *)bu_calloc((size_t)nv, sizeof(point_t),
					    "bg_trimesh_repair verts");
    int *out_faces   = (int *)bu_calloc((size_t)nf * 3, sizeof(int),
					"bg_trimesh_repair faces");

    for (int i = 0; i < nv; i++) {
	out_pts[i][X] = verts[i][0];
	out_pts[i][Y] = verts[i][1];
	out_pts[i][Z] = verts[i][2];
    }
    for (int i = 0; i < nf; i++) {
	out_faces[3*i+0] = tris[i][0];
	out_faces[3*i+1] = tris[i][1];
	out_faces[3*i+2] = tris[i][2];
    }

    *opnts   = out_pts;
    *n_opnts = nv;
    *ofaces   = out_faces;
    *n_ofaces = nf;

    report->output_vertices = nv;
    report->output_faces = nf;
    report->output_area = trimesh_gte_area(verts, tris);

    std::vector<std::array<int32_t, 3>> geometric_check = tris;
    const bool no_geometric_degenerates =
	!trimesh_remove_geometric_degenerate(verts, geometric_check);
    report->solid = no_geometric_degenerates &&
	trimesh_gte_valid_vertex_links(tris, verts.size()) &&
	!bg_trimesh_solid2(nv, nf, (fastf_t *)out_pts, out_faces, NULL);
    if (settings->require_solid && !report->solid) {
	bu_free(out_faces, "bg_trimesh_repair faces");
	bu_free(out_pts, "bg_trimesh_repair verts");
	*ofaces = NULL;
	*n_ofaces = 0;
	*opnts = NULL;
	*n_opnts = 0;
	return -1;
    }
    if (report->solid)
	report->output_volume = bg_trimesh_volume(out_faces, (size_t)nf,
	    out_pts, (size_t)nv);

    return 0;
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
