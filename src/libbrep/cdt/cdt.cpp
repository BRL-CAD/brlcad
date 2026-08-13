/*                        C D T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2026 United States Government as represented by
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
/** @addtogroup libbrep */
/** @{ */
/** @file cdt.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "bg/chull.h"
#include "bg/spsr.h"
#include "bg/tri_pt.h"
#include "bg/tri_tri.h"
#include "brep/surfacetree.h"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#include "../../libbg/detria.hpp"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif
#include "./cdt.h"

#define BREP_PLANAR_TOL 0.05
#define MAX_TRIANGULATION_ATTEMPTS 5
#define MAX_CHART_REFINEMENT_ATTEMPTS 16
#define MAX_CHART_REFINEMENT_POINTS 4096
#define MAX_CHART_REFINEMENT_STAGNANT_ATTEMPTS 4
#define MAX_ASSEMBLED_REFINEMENT_ATTEMPTS 16
#define MAX_ASSEMBLED_REFINEMENT_POINTS 4096
#define MAX_SHARED_EDGE_REFINEMENT_DISTANCE_RATIO 0.05
#define MAX_AUTOMATIC_LOCAL_REPAIR_FACES 8192
#define MAX_BEST_EFFORT_FOLD_DIVISOR 20
#define MAX_BOUNDARY_STRIP_CORRESPONDENCE_TESTS 16777216
#define MAX_BOUNDARY_STRIP_REFINEMENT_POINTS 1048576

struct assembled_mesh_validation {
    size_t invalid_indices = 0;
    size_t nonfinite_vertices = 0;
    size_t unused_vertices = 0;
    size_t degenerate_faces = 0;
    size_t invalid_vertex_links = 0;
    size_t intersecting_triangle_pairs = 0;
    int first_intersection[2] = {-1, -1};
    bool first_intersection_point_valid = false;
    double first_intersection_point[3] = {0.0, 0.0, 0.0};
};

static bool
repair_hybrid_fallback_preflight(size_t geometric_degenerate_faces,
	size_t invalid_vertex_links, size_t local_repair_limit)
{
    const size_t link_limit = geometric_degenerate_faces <= SIZE_MAX / 4 ?
	4 * geometric_degenerate_faces : SIZE_MAX;
    return geometric_degenerate_faces > 0 && local_repair_limit > 0 &&
	invalid_vertex_links > local_repair_limit &&
	invalid_vertex_links > link_limit;
}

static bool
assembled_mesh_collect_candidate(size_t triangle, void *context)
{
    std::vector<size_t> *candidates =
	(std::vector<size_t> *)context;
    candidates->push_back(triangle);
    return true;
}

static bool
assembled_mesh_triangle_degenerate(const ON_3dPoint points[3])
{
    const ON_3dVector ab = points[1] - points[0];
    const ON_3dVector ac = points[2] - points[0];
    const ON_3dVector bc = points[2] - points[1];
    const double longest_sq = std::max(ab.LengthSquared(),
	std::max(ac.LengthSquared(), bc.LengthSquared()));
    const double doubled_area = ON_CrossProduct(ab, ac).Length();
    return !(longest_sq > 0.0) || !std::isfinite(doubled_area) ||
	doubled_area <= 64.0 * std::numeric_limits<double>::epsilon() *
	longest_sq;
}

static bool
assembled_mesh_validate(int vertex_count, int face_count,
	const fastf_t *vertices, const int *faces,
	assembled_mesh_validation *validation, bool check_intersections = true)
{
    if (!validation || vertex_count <= 0 || face_count <= 0 ||
	    !vertices || !faces)
	return false;
    *validation = assembled_mesh_validation();

    typedef std::pair<int, int> mesh_edge;
    std::vector<std::vector<mesh_edge>> vertex_link_edges(
	(size_t)vertex_count);
    std::vector<bool> used_vertices((size_t)vertex_count, false);
    for (int vertex = 0; vertex < vertex_count; ++vertex) {
	for (int coordinate = 0; coordinate < 3; ++coordinate) {
	    if (!std::isfinite(vertices[3 * vertex + coordinate])) {
		validation->nonfinite_vertices++;
		break;
	    }
	}
    }

    for (int face = 0; face < face_count; ++face) {
	ON_3dPoint points[3];
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = faces[3 * face + corner];
	    if (vertex < 0 || vertex >= vertex_count) {
		validation->invalid_indices++;
		return false;
	    }
	    used_vertices[(size_t)vertex] = true;
	    points[corner] = ON_3dPoint(vertices[3 * vertex],
		vertices[3 * vertex + 1], vertices[3 * vertex + 2]);
	}
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = faces[3 * face + corner];
	    vertex_link_edges[(size_t)vertex].push_back(mesh_edge(
		faces[3 * face + (corner + 1) % 3],
		faces[3 * face + (corner + 2) % 3]));
	}
	if (assembled_mesh_triangle_degenerate(points))
	    validation->degenerate_faces++;
    }

    for (bool used : used_vertices) {
	if (!used)
	    validation->unused_vertices++;
    }

    for (int vertex = 0; vertex < vertex_count; ++vertex) {
	const std::vector<mesh_edge> &links =
	    vertex_link_edges[(size_t)vertex];
	if (links.empty())
	    continue;
	std::map<int, std::vector<int>> adjacency;
	for (const mesh_edge &link : links) {
	    adjacency[link.first].push_back(link.second);
	    adjacency[link.second].push_back(link.first);
	}
	std::set<int> reached;
	std::queue<int> work;
	work.push(adjacency.begin()->first);
	reached.insert(adjacency.begin()->first);
	bool valid_link = true;
	while (!work.empty()) {
	    const int current = work.front();
	    work.pop();
	    const std::vector<int> &neighbors = adjacency[current];
	    valid_link = valid_link && neighbors.size() == 2;
	    for (int neighbor : neighbors) {
		if (reached.insert(neighbor).second)
		    work.push(neighbor);
	    }
	}
	if (!valid_link || reached.size() != adjacency.size())
	    validation->invalid_vertex_links++;
    }

    /* Edge incidence and vertex links do not detect two otherwise valid
     * closed sheets crossing away from shared topology.  Query an R-tree of
     * prior triangle bounds and apply the validated library predicate only to
     * nonadjacent candidates.  One certified counterexample is sufficient
     * to fail closed and bounds work on malformed, heavily overlapping
     * meshes. */
    if (check_intersections && !validation->nonfinite_vertices &&
	    !validation->degenerate_faces) {
	RTree<size_t, double, 3> triangle_index;
	for (int face = 0; face < face_count; ++face) {
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
		const int vertex = faces[3 * face + corner];
		VSET(first_points[corner], vertices[3 * vertex],
		    vertices[3 * vertex + 1], vertices[3 * vertex + 2]);
		for (int axis = 0; axis < 3; ++axis) {
		    minimum[axis] = std::min(minimum[axis],
			(double)first_points[corner][axis]);
		    maximum[axis] = std::max(maximum[axis],
			(double)first_points[corner][axis]);
		}
	    }
	    std::vector<size_t> candidates;
	    triangle_index.Search(minimum, maximum,
		assembled_mesh_collect_candidate, &candidates);
	    std::sort(candidates.begin(), candidates.end());
	    for (size_t candidate : candidates) {
		const int *first = &faces[3 * face];
		const int *second = &faces[3 * candidate];
		bool adjacent = false;
		for (int i = 0; i < 3 && !adjacent; ++i) {
		    for (int j = 0; j < 3; ++j) {
			if (first[i] == second[j]) {
			    adjacent = true;
			    break;
			}
		    }
		}
		if (adjacent)
		    continue;
		point_t second_points[3];
		for (int corner = 0; corner < 3; ++corner) {
		    const int vertex = second[corner];
		    VSET(second_points[corner], vertices[3 * vertex],
			vertices[3 * vertex + 1],
			vertices[3 * vertex + 2]);
		}
		const bool intersects = cdt_tri_tri_intersection(first_points,
		    second_points);
		if (!intersects)
		    continue;
		validation->intersecting_triangle_pairs = 1;
		validation->first_intersection[0] = (int)candidate;
		validation->first_intersection[1] = face;
		int coplanar = 0;
		point_t start = VINIT_ZERO;
		point_t end = VINIT_ZERO;
		if (bg_tri_tri_isect_with_line(first_points[0],
			first_points[1], first_points[2], second_points[0],
			second_points[1], second_points[2], &coplanar,
			&start, &end) && !coplanar) {
		    for (int axis = 0; axis < 3; ++axis)
			validation->first_intersection_point[axis] =
			    0.5 * (start[axis] + end[axis]);
		    validation->first_intersection_point_valid = true;
		}
		break;
	    }
	    if (validation->intersecting_triangle_pairs)
		break;
	    triangle_index.Insert(minimum, maximum, (size_t)face);
	}
    }

    return !validation->invalid_indices && !validation->nonfinite_vertices &&
	!validation->unused_vertices && !validation->degenerate_faces &&
	!validation->invalid_vertex_links &&
	!validation->intersecting_triangle_pairs;
}

int
cdt_test_assembled_mesh_validation(void)
{
    fastf_t vertices[] = {
	0.0, 0.0, 0.0,
	1.0, 0.0, 0.0,
	0.0, 1.0, 0.0,
	0.0, 0.0, 1.0,
	-1.0, 0.0, 0.0,
	0.0, -1.0, 0.0,
	0.0, 0.0, -1.0
    };
    int tetrahedron[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3
    };
    assembled_mesh_validation validation;
    if (!assembled_mesh_validate(4, 4, vertices, tetrahedron,
	    &validation))
	return 1;

    /* Two otherwise closed tetrahedra meeting only at vertex zero have two
     * disjoint cycles in that vertex's link.  Edge-incidence checks alone do
     * not detect this bow-tie topology. */
    int bow_tie[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3,
	0, 5, 4, 0, 4, 6, 0, 6, 5, 4, 5, 6
    };
    if (assembled_mesh_validate(7, 8, vertices, bow_tie, &validation) ||
	    validation.invalid_vertex_links != 1)
	return 2;

    fastf_t vertices_with_unused[24];
    std::copy(vertices, vertices + 21, vertices_with_unused);
    vertices_with_unused[21] = 2.0;
    vertices_with_unused[22] = 2.0;
    vertices_with_unused[23] = 2.0;
    if (assembled_mesh_validate(8, 4, vertices_with_unused, tetrahedron,
	    &validation) || validation.unused_vertices != 4)
	return 3;

    int degenerate[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 0, 0, 1
    };
    if (assembled_mesh_validate(4, 4, vertices, degenerate, &validation) ||
	    validation.degenerate_faces != 1)
	return 4;

    fastf_t crossing_vertices[] = {
	0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0, 0.0, 1.0,
	0.25, 0.25, 0.25, 1.25, 0.25, 0.25,
	0.25, 1.25, 0.25, 0.25, 0.25, 1.25
    };
    int crossing_tetrahedra[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3,
	4, 6, 5, 4, 5, 7, 4, 7, 6, 5, 6, 7
    };
    if (assembled_mesh_validate(8, 8, crossing_vertices,
	    crossing_tetrahedra, &validation) ||
	    validation.intersecting_triangle_pairs != 1 ||
	    validation.first_intersection[0] < 0 ||
	    validation.first_intersection[1] < 0 ||
	    !validation.first_intersection_point_valid)
	return 5;

    /* Adjacent face chords can have touching bounding boxes and separated,
     * collinear boundary intervals.  The base predicate's coarse epsilon
     * reports this NIST-derived configuration as an intersection, but its
     * reported endpoint does not lie on both triangles. */
    point_t separated_first[3] = {
	{514.97292083479124, -327.29326229964499, 77.978026208414306},
	{515.54826425965757, -326.98097089301785, 78.005348166208819},
	{515.6018756882213, -326.66867948639072, 78.032670124003332}
    };
    point_t separated_second[3] = {
	{515.45117516473545, -327.29326229964499, 77.978026208414306},
	{515.45520000264924, -327.36064849937765, 77.971900884443286},
	{515.46589947082487, -327.3585443137215, 77.97209910798874}
    };
    if (cdt_tri_tri_intersection(separated_first, separated_second))
	return 6;

    return 0;
}

static bool
cdt_tolerance_valid(const struct bg_tess_tol &tol)
{
    const fastf_t values[] = {
	tol.abs, tol.rel, tol.norm, tol.absmax, tol.absmin,
	tol.relmax, tol.relmin, tol.rel_lmax, tol.rel_lmin
    };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
	if (!std::isfinite(values[i]) || values[i] < -1.0)
	    return false;
    }
    if (tol.norm > ON_PI)
	return false;
    if (tol.absmax > 0.0 && tol.absmin > 0.0 &&
	    tol.absmax < tol.absmin)
	return false;
    if (tol.relmax > 0.0 && tol.relmin > 0.0 &&
	    tol.relmax < tol.relmin)
	return false;
    if (tol.rel_lmax > 0.0 && tol.rel_lmin > 0.0 &&
	    tol.rel_lmax < tol.rel_lmin)
	return false;
    return true;
}

/* cpolyedge_t containers historically used the default pointer comparator.
 * That is adequate for membership, but it must not determine the order of
 * geometry-changing operations: allocator layout can then change the order in
 * which singular trims are subdivided and, consequently, the final surface
 * sampling.  Every singular segment has an authoritative STEP trim index,
 * parameter interval, and pair of face-local topology vertices, which together
 * provide a stable processing order. */
static bool
singular_edge_process_less(const cpolyedge_t *a, const cpolyedge_t *b)
{
    if (a == b)
	return false;
    if (!a)
	return true;
    if (!b)
	return false;
    if (a->trim_ind != b->trim_ind)
	return a->trim_ind < b->trim_ind;
    if (a->trim_start < b->trim_start)
	return true;
    if (b->trim_start < a->trim_start)
	return false;
    if (a->trim_end < b->trim_end)
	return true;
    if (b->trim_end < a->trim_end)
	return false;
    if (a->v2d[0] != b->v2d[0])
	return a->v2d[0] < b->v2d[0];
    return a->v2d[1] < b->v2d[1];
}

struct compact_mesh_edge_use {
    int first;
    int second;
    int face;
};

static int
compact_mesh_component_root(std::vector<int> &parents, int item)
{
    int root = item;
    while (parents[(size_t)root] >= 0)
	root = parents[(size_t)root];
    while (item != root) {
	const int next = parents[(size_t)item];
	parents[(size_t)item] = root;
	item = next;
    }
    return root;
}

/* Find face components through exactly two-use edges.  Sorting a packed edge
 * array is substantially smaller than storing millions of edge keys and tiny
 * neighbor vectors in separately allocated tree nodes. */
static void
compact_mesh_face_components(const int *faces, int face_count,
	std::vector<int> &component_id,
	std::vector<std::vector<int>> &components)
{
    component_id.assign((size_t)face_count, -1);
    components.clear();
    if (!faces || face_count <= 0)
	return;

    std::vector<compact_mesh_edge_use> edge_uses;
    edge_uses.reserve((size_t)face_count * 3);
    for (int face = 0; face < face_count; ++face) {
	for (int corner = 0; corner < 3; ++corner) {
	    int first = faces[(size_t)face * 3 + corner];
	    int second = faces[(size_t)face * 3 + (corner + 1) % 3];
	    if (first > second)
		std::swap(first, second);
	    compact_mesh_edge_use use = {first, second, face};
	    edge_uses.push_back(use);
	}
    }
    std::sort(edge_uses.begin(), edge_uses.end(),
	[](const compact_mesh_edge_use &a,
	    const compact_mesh_edge_use &b) {
	    if (a.first != b.first)
		return a.first < b.first;
	    if (a.second != b.second)
		return a.second < b.second;
	    return a.face < b.face;
	});

    std::vector<int> parents((size_t)face_count, -1);
    for (size_t begin = 0; begin < edge_uses.size();) {
	size_t end = begin + 1;
	while (end < edge_uses.size() &&
		edge_uses[end].first == edge_uses[begin].first &&
		edge_uses[end].second == edge_uses[begin].second)
	    ++end;
	if (end - begin == 2) {
	    int first_root = compact_mesh_component_root(parents,
		edge_uses[begin].face);
	    int second_root = compact_mesh_component_root(parents,
		edge_uses[begin + 1].face);
	    if (first_root != second_root) {
		if (parents[(size_t)first_root] >
			parents[(size_t)second_root])
		    std::swap(first_root, second_root);
		parents[(size_t)first_root] += parents[(size_t)second_root];
		parents[(size_t)second_root] = first_root;
	    }
	}
	begin = end;
    }

    std::vector<int> root_component((size_t)face_count, -1);
    for (int face = 0; face < face_count; ++face) {
	const int root = compact_mesh_component_root(parents, face);
	if (root_component[(size_t)root] < 0) {
	    root_component[(size_t)root] = (int)components.size();
	    components.push_back(std::vector<int>());
	}
	const int component = root_component[(size_t)root];
	component_id[(size_t)face] = component;
	components[(size_t)component].push_back(face);
    }
}

/* Correct triangle winding only after the complete mesh proves closed and
 * manifold and misorientation is its sole defect.  bg_trimesh_sync establishes
 * a consistent orientation for each connected component, but its seed may
 * choose either of the two possible component orientations.  Preserve the
 * OpenNURBS face orientation by selecting the result that changes the fewest
 * source triangles in each component.  A tied component has no authoritative
 * majority, so leave the mesh unchanged rather than guessing.
 *
 * This function is transactional: faces and face normals are not modified
 * unless the candidate subsequently passes the complete solid test. */
static int
closed_mesh_orientation_sync(int *faces, int face_count,
	fastf_t *vertices, int vertex_count, int *face_normals)
{
    const bool debug_repair_topology =
	getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY") != NULL;
    if (!faces || !vertices || face_count < 4 || vertex_count < 4)
	return -1;

    struct bg_trimesh_solid_errors errors = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    const int initial_invalid = bg_trimesh_solid2(vertex_count, face_count,
	vertices, faces, &errors);
    if (!initial_invalid) {
	bg_free_trimesh_solid_errors(&errors);
	return 0;
    }

    const bool only_misoriented = errors.misoriented.count > 0 &&
	errors.degenerate.count == 0 && errors.unmatched.count == 0 &&
	errors.excess.count == 0;
    if (debug_repair_topology)
	bu_log("Orientation sync input: degenerate %d, unmatched %d, "
	    "excess %d, misoriented %d\n", errors.degenerate.count,
	    errors.unmatched.count, errors.excess.count,
	    errors.misoriented.count);
    bg_free_trimesh_solid_errors(&errors);
    if (!only_misoriented)
	return -1;

    std::vector<int> original(faces, faces + 3 * face_count);
    std::vector<int> candidate(original);

    if (bg_trimesh_sync(candidate.data(), candidate.data(), face_count) <= 0) {
	if (debug_repair_topology)
	    bu_log("Orientation sync could not establish face adjacency\n");
	return -1;
    }

    /* Determine the connected closed-shell components from shared edges. */
    std::vector<int> component_id;
    std::vector<std::vector<int>> components;
    compact_mesh_face_components(original.data(), face_count,
	component_id, components);

    for (const std::vector<int> &component : components) {
	size_t changed = 0;
	for (int face : component) {
	    const size_t offset = (size_t)face * 3;
	    if (candidate[offset] != original[offset] ||
		    candidate[offset + 1] != original[offset + 1] ||
		    candidate[offset + 2] != original[offset + 2])
		changed++;
	}
	if (changed * 2 == component.size()) {
	    if (debug_repair_topology)
		bu_log("Orientation sync component tied: %zu/%zu changed\n",
		    changed, component.size());
	    return -1;
	}
	if (changed * 2 > component.size()) {
	    for (int face : component)
		std::swap(candidate[(size_t)face * 3],
		    candidate[(size_t)face * 3 + 1]);
	}
    }

    struct bg_trimesh_solid_errors candidate_errors =
	BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    const int candidate_invalid = bg_trimesh_solid2(vertex_count,
	face_count, vertices, candidate.data(), &candidate_errors);
    if (candidate_invalid != 0) {
	if (debug_repair_topology)
	    bu_log("Orientation sync candidate remained non-solid: "
		"degenerate %d, unmatched %d, excess %d, misoriented %d\n",
		candidate_errors.degenerate.count,
		candidate_errors.unmatched.count,
		candidate_errors.excess.count,
		candidate_errors.misoriented.count);
	bg_free_trimesh_solid_errors(&candidate_errors);
	return -1;
    }
    bg_free_trimesh_solid_errors(&candidate_errors);

    int changed_count = 0;
    for (int face = 0; face < face_count; ++face) {
	const size_t offset = (size_t)face * 3;
	if (candidate[offset] == original[offset] &&
		candidate[offset + 1] == original[offset + 1] &&
		candidate[offset + 2] == original[offset + 2])
	    continue;
	changed_count++;
	if (face_normals)
	    std::swap(face_normals[offset], face_normals[offset + 1]);
    }

    std::copy(candidate.begin(), candidate.end(), faces);
    return changed_count;
}

/* A failed local face remesh can leave a few open triangle islands in addition
 * to the useful tessellation of the source solid.  Those islands are removed
 * only under one of two proofs: every retained component is independently
 * closed and solid, or (for a partial display mesh) every point of an island
 * is already covered within tolerance by retained triangles from the same
 * source BREP face.  Valid disjoint shells are retained, and no change is made
 * if neither proof succeeds.
 *
 * The operation is transactional with respect to the caller's face arrays.
 * Vertex arrays may retain now-unused entries; that is legal for a BoT and
 * keeps the face-normal and point ownership maps stable. */
static int
closed_mesh_component_filter(int *faces, int *face_count,
	fastf_t *vertices, int vertex_count, int *face_normals,
	int *source_faces = NULL, double overlap_tolerance = 0.0,
	bool source_is_solid = true, bool emit_diagnostics = true,
	size_t *source_triangles = NULL)
{
    if (!faces || !face_count || !vertices || *face_count < 2 ||
	    vertex_count < 4)
	return 0;

    /* Validate all indices before making the first transactional edit. */
    for (int face = 0; face < *face_count; ++face) {
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = faces[(size_t)face * 3 + corner];
	    if (vertex < 0 || vertex >= vertex_count)
		return 0;
	}
    }

    /* Distinct face-mesh vertices may collapse to the same shared output
     * point.  Cull the resulting zero-area index triangles before component
     * discovery; otherwise their repeated edge can falsely bridge two
     * components in this audit even though rt_bot_split correctly ignores
     * that bridge. */
    const int original_face_count = *face_count;
    std::vector<int> compact_faces;
    std::vector<int> compact_normals;
    std::vector<int> compact_sources;
    std::vector<size_t> compact_triangles;
    compact_faces.reserve((size_t)original_face_count * 3);
    if (face_normals)
	compact_normals.reserve((size_t)original_face_count * 3);
    if (source_faces)
	compact_sources.reserve((size_t)original_face_count);
    if (source_triangles)
	compact_triangles.reserve((size_t)original_face_count);
    for (int face = 0; face < original_face_count; ++face) {
	const size_t offset = (size_t)face * 3;
	if (faces[offset] == faces[offset + 1] ||
		faces[offset + 1] == faces[offset + 2] ||
		faces[offset + 2] == faces[offset])
	    continue;
	for (int corner = 0; corner < 3; ++corner) {
	    compact_faces.push_back(faces[offset + corner]);
	    if (face_normals)
		compact_normals.push_back(face_normals[offset + corner]);
	}
	if (source_faces)
	    compact_sources.push_back(source_faces[face]);
	if (source_triangles)
	    compact_triangles.push_back(source_triangles[face]);
    }
    int removed_count = original_face_count -
	(int)(compact_faces.size() / 3);
    if (removed_count) {
	std::copy(compact_faces.begin(), compact_faces.end(), faces);
	if (face_normals)
	    std::copy(compact_normals.begin(), compact_normals.end(),
		face_normals);
	*face_count = (int)(compact_faces.size() / 3);
	if (source_faces)
	    std::copy(compact_sources.begin(), compact_sources.end(),
		source_faces);
	if (source_triangles)
	    std::copy(compact_triangles.begin(), compact_triangles.end(),
		source_triangles);
    }
    if (*face_count < 2)
	return removed_count;

    std::vector<int> component_id;
    std::vector<std::vector<int>> components;
    compact_mesh_face_components(faces, *face_count, component_id,
	components);
    if (components.size() < 2)
	return removed_count;

    std::vector<bool> retain(components.size(), false);
    size_t retained_face_count = 0;
    for (size_t component = 0;
	    source_is_solid && component < components.size(); ++component) {
	std::vector<int> component_faces;
	component_faces.reserve(components[component].size() * 3);
	for (int face : components[component]) {
	    const size_t offset = (size_t)face * 3;
	    component_faces.push_back(faces[offset]);
	    component_faces.push_back(faces[offset + 1]);
	    component_faces.push_back(faces[offset + 2]);
	}
	if (component_faces.size() >= 12 &&
		bg_trimesh_solid2(vertex_count,
		    (int)components[component].size(), vertices,
		    component_faces.data(), NULL) == 0) {
	    retain[component] = true;
	    retained_face_count += components[component].size();
	}
    }
    if (retained_face_count == (size_t)*face_count)
	return removed_count;

    /* Closed sub-shells are not by themselves proof that every open
     * component is redundant.  In particular, a valid multi-face solid can
     * contain a small independently closed feature alongside a much larger
     * component whose shared-edge assembly is defective.  Preserve the full
     * candidate unless the same-face overlap proof below can account for
     * every discarded triangle. */
    if (retained_face_count && (!source_faces || !(overlap_tolerance > 0.0)))
	return removed_count;

    /* Open local-remesh islands may be removed only when every vertex and
     * centroid lies on retained triangles from the same source face.  This is
     * an explicit redundancy proof within the tessellation tolerance, whether
     * the retained reference is a certified closed shell or, for a partial
     * display mesh, the largest open component. */
    if (source_faces && overlap_tolerance > 0.0) {
	size_t largest = 0;
	if (!retained_face_count) {
	    for (size_t component = 1; component < components.size(); ++component) {
		if (components[component].size() > components[largest].size())
		    largest = component;
	    }
	    retain[largest] = true;
	    retained_face_count = components[largest].size();
	}
	double minimum_failed_distance = DBL_MAX;
	int failed_source_face = -1;
	const auto point_covered =
	    [&](const point_t point, int source_face) {
		double minimum_distance = DBL_MAX;
		for (int candidate = 0; candidate < *face_count; ++candidate) {
		    if (!retain[(size_t)component_id[(size_t)candidate]])
			continue;
		    if (source_faces[candidate] != source_face)
			continue;
		    point_t triangle[3];
		    for (int corner = 0; corner < 3; ++corner) {
			const int vertex =
			    faces[(size_t)candidate * 3 + corner];
			VMOVE(triangle[corner],
			    &vertices[(size_t)vertex * 3]);
		    }
		    const double distance = bg_tri_closest_pt(NULL, point,
			triangle[0], triangle[1], triangle[2]);
		    minimum_distance = std::min(minimum_distance, distance);
		    if (distance <= overlap_tolerance)
			return true;
		}
		minimum_failed_distance = minimum_distance;
		failed_source_face = source_face;
		return false;
	    };
	bool redundant = true;
	for (size_t component = 0;
		component < components.size() && redundant; ++component) {
	    if (retain[component])
		continue;
	    for (int face : components[component]) {
		point_t centroid = VINIT_ZERO;
		for (int corner = 0; corner < 3; ++corner) {
		    const int vertex = faces[(size_t)face * 3 + corner];
		    point_t point;
		    VMOVE(point, &vertices[(size_t)vertex * 3]);
		    if (!point_covered(point, source_faces[face])) {
			redundant = false;
			break;
		    }
		    VADD2(centroid, centroid, point);
		}
		VSCALE(centroid, centroid, 1.0 / 3.0);
		if (redundant &&
			!point_covered(centroid, source_faces[face]))
		    redundant = false;
		if (!redundant)
		    break;
	    }
	}
	if (!redundant) {
	    if (!emit_diagnostics)
		return removed_count;
	    bu_log("CDT retained an open disconnected component because source "
		"face %d was not redundantly covered (distance %.17g, "
		"tolerance %.17g)\n", failed_source_face,
		minimum_failed_distance, overlap_tolerance);
	    return removed_count;
	}
    }
    if (!retained_face_count)
	return removed_count;

    std::vector<int> candidate_faces;
    std::vector<int> candidate_normals;
    std::vector<int> candidate_sources;
    std::vector<size_t> candidate_triangles;
    candidate_faces.reserve(retained_face_count * 3);
    if (face_normals)
	candidate_normals.reserve(retained_face_count * 3);
    if (source_faces)
	candidate_sources.reserve(retained_face_count);
    if (source_triangles)
	candidate_triangles.reserve(retained_face_count);
    for (int face = 0; face < *face_count; ++face) {
	if (!retain[(size_t)component_id[(size_t)face]])
	    continue;
	const size_t offset = (size_t)face * 3;
	for (int corner = 0; corner < 3; ++corner) {
	    candidate_faces.push_back(faces[offset + corner]);
	    if (face_normals)
		candidate_normals.push_back(face_normals[offset + corner]);
	}
	if (source_faces)
	    candidate_sources.push_back(source_faces[face]);
	if (source_triangles)
	    candidate_triangles.push_back(source_triangles[face]);
    }
    if (!source_faces &&
	    bg_trimesh_solid2(vertex_count, (int)retained_face_count,
		vertices, candidate_faces.data(), NULL) != 0)
	return removed_count;

    const int removed = removed_count +
	*face_count - (int)retained_face_count;
    std::copy(candidate_faces.begin(), candidate_faces.end(), faces);
    if (face_normals)
	std::copy(candidate_normals.begin(), candidate_normals.end(),
	    face_normals);
    if (source_faces)
	std::copy(candidate_sources.begin(), candidate_sources.end(),
	    source_faces);
    if (source_triangles)
	std::copy(candidate_triangles.begin(), candidate_triangles.end(),
	    source_triangles);
    *face_count = (int)retained_face_count;
    return removed;
}

// TODO - get rid of all BN_TOL_DIST-only tolerances - if the object is
// very small, that distance is too big (e.g. for linearity testing).

static ON_3dVector
bseg_tangent(struct ON_Brep_CDT_State *s_cdt, bedge_seg_t *bseg, double eparam, double t1param, double t2param)
{
    ON_3dPoint tmp;
    ON_3dVector tangent = ON_3dVector::UnsetVector;
    if (!bseg->nc->EvTangent(eparam, tmp, tangent)) {
	if (t1param < DBL_MAX && t2param < DBL_MAX) {
	    // If the edge curve failed, average tangents from trims
	    ON_BrepEdge& edge = s_cdt->brep->m_E[bseg->edge_ind];
	    ON_BrepTrim *trim1 = edge.Trim(0);
	    ON_BrepTrim *trim2 = edge.Trim(1);
	    int evals = 0;
	    ON_3dPoint tmp1, tmp2;
	    ON_3dVector trim1_tangent, trim2_tangent;
	    evals += (trim1->EvTangent(t1param, tmp1, trim1_tangent)) ? 1 : 0;
	    evals += (trim2->EvTangent(t2param, tmp2, trim2_tangent)) ? 1 : 0;
	    if (evals == 2) {
		tangent = (trim1_tangent + trim2_tangent) / 2;
	    } else {
		tangent = ON_3dVector::UnsetVector;
	    }
	}
    }

    return tangent;
}


static bool
refine_triangulation(struct ON_Brep_CDT_State *s_cdt, cdt_mesh_t *fmesh,
	int cnt, int rebuild, bool allow_general_boundary_cleanup = false)
{
    if (!s_cdt || !fmesh) return false;

    const auto time_limit_reached = [&]() {
	if (!s_cdt->face_deadline || bu_gettime() < s_cdt->face_deadline)
	    return false;
	const std::string message = "rigorous face triangulation exceeded its " +
	    std::to_string(s_cdt->max_face_time_ms) +
	    " ms wall-clock limit during adaptive refinement";
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REFINEMENT_LIMIT,
	    BREP_CDT_STAGE_ADAPTIVE_REFINEMENT, fmesh->f_id, 0, 1,
	    message.c_str());
	return true;
    };
    if (time_limit_reached())
	return false;

    if (cnt > MAX_TRIANGULATION_ATTEMPTS) {
	std::cerr << "Error: even after " << MAX_TRIANGULATION_ATTEMPTS << " iterations could not successfully refine triangulate face " << fmesh->f_id << " for solidity criteria\n";
	return false;
    }

    // If a previous pass has made changes in which points are active in the
    // surface set, we need to rebuild the whole triangulation.

    if (rebuild && !fmesh->cdt(allow_general_boundary_cleanup)) {
	bu_log("Fatal failure attempting full retriangulation of face\n");
	return false;
    }

    // A chart triangulation which already satisfies the face invariants does
    // not need the legacy UV repair heuristics.  In particular, applying a
    // best-fit-plane reparameterization to a valid singular chart can
    // reintroduce the degeneracy the chart was constructed to remove.

    const bool topology_chart = cdt_face_uses_topology_chart(
	s_cdt->brep->m_F[fmesh->f_id]);
    const auto valid_or_repair_normals = [&]() {
	if (fmesh->valid(0, false))
	    return fmesh->self_intersections(NULL, 1) == 0;
	return fmesh->repair_incorrect_normal_edges() && fmesh->valid(0);
    };
    if (valid_or_repair_normals())
	return true;
    if (fmesh->repair_toleranced_nonmanifold_edges())
	return true;

    /* Periodic seam copies can make distinct chart cells share the same
     * three model-space vertices.  Separate those cells before geometric
     * fold and intersection refinement works on the stitched mesh. */
    size_t collapsed_chart_points = 0;
    for (int attempt = 0; attempt < MAX_CHART_REFINEMENT_ATTEMPTS;
	    ++attempt) {
	if (time_limit_reached())
	    return false;
	const size_t remaining = MAX_CHART_REFINEMENT_POINTS -
	    collapsed_chart_points;
	if (!remaining)
	    break;
	const size_t inserted =
	    fmesh->refine_collapsed_chart_triangles(remaining);
	if (!inserted)
	    break;
	collapsed_chart_points += inserted;
	if (!fmesh->cdt(allow_general_boundary_cleanup)) {
	    bu_log("Face %d: collapsed chart cell retriangulation failed\n",
		fmesh->f_id);
	    return false;
	}
	if (time_limit_reached())
	    return false;
	if (valid_or_repair_normals()) {
	    bu_log("Face %d: separated collapsed periodic chart cells after "
		"%zu inserted points\n", fmesh->f_id,
		collapsed_chart_points);
	    return true;
	}
	if (fmesh->repair_toleranced_nonmanifold_edges())
	    return true;
    }

    const size_t initial_folds = fmesh->incorrect_normal_count();
    const size_t initial_intersections = fmesh->self_intersections(NULL,
	MAX_CHART_REFINEMENT_POINTS);
    const size_t initial_defects = initial_folds + initial_intersections;
    size_t refinement_points = 0;
    int refinement_attempts = 0;
    bool refinement_stalled = false;
    bool refinement_no_progress = false;
    size_t best_defects = initial_defects;
    int stagnant_attempts = 0;
    for (int attempt = 0; attempt < MAX_CHART_REFINEMENT_ATTEMPTS;
	    ++attempt) {
	if (time_limit_reached())
	    return false;
	const size_t remaining = MAX_CHART_REFINEMENT_POINTS -
	    refinement_points;
	if (!remaining)
	    break;
	refinement_attempts++;
	size_t inserted = fmesh->refine_self_intersections(remaining);
	if (inserted < remaining)
	    inserted += fmesh->refine_incorrect_normals(remaining - inserted);
	if (!inserted) {
	    refinement_stalled = true;
	    break;
	}
	refinement_points += inserted;
	if (!fmesh->cdt(allow_general_boundary_cleanup)) {
	    bu_log("Face %d: chart refinement retriangulation failed\n",
		fmesh->f_id);
	    return false;
	}
	if (time_limit_reached())
	    return false;
	if (valid_or_repair_normals()) {
	    bu_log("Face %d: chart refinement certified after %zu "
		"inserted points\n", fmesh->f_id, refinement_points);
	    return true;
	}
	if (fmesh->repair_toleranced_nonmanifold_edges())
	    return true;
	const size_t current_folds = fmesh->incorrect_normal_count();
	/* Progress only requires knowing whether the defect count improved.
	 * Once enough intersections have been found to equal the previous best,
	 * continuing the complete pair scan cannot change that decision. */
	size_t current_intersections = 0;
	size_t current_defects = best_defects;
	if (current_folds < best_defects) {
	    const size_t improvement_limit = best_defects - current_folds;
	    current_intersections = fmesh->self_intersections(NULL,
		improvement_limit);
	    if (current_intersections < improvement_limit)
		current_defects = current_folds + current_intersections;
	}
	if (current_defects < best_defects) {
	    best_defects = current_defects;
	    stagnant_attempts = 0;
	} else {
	    stagnant_attempts++;
	    if (stagnant_attempts >=
		    MAX_CHART_REFINEMENT_STAGNANT_ATTEMPTS) {
		refinement_no_progress = true;
		break;
	    }
	}
    }
    if (fmesh->repair_toleranced_nonmanifold_edges())
	return true;
    if (refinement_points) {
	const size_t remaining_folds = fmesh->incorrect_normal_count();
	const size_t remaining_intersections = fmesh->self_intersections(NULL,
	    MAX_CHART_REFINEMENT_POINTS);
	const std::string remaining_summary = std::to_string(remaining_folds) +
	    " folded triangles and " +
	    std::to_string(remaining_intersections) +
	    " intersecting triangle pairs";
	const std::string initial_summary = std::to_string(initial_folds) +
	    " folded triangles and " +
	    std::to_string(initial_intersections) +
	    " intersecting triangle pairs";
	const std::string message = refinement_no_progress ?
	    "adaptive chart refinement made no progress after " +
		std::to_string(refinement_attempts) + " rounds and " +
		std::to_string(refinement_points) + " points; best " +
		std::to_string(best_defects) +
		" geometric defects, remaining " + remaining_summary :
	    refinement_stalled ?
	    "adaptive chart refinement stalled after " +
		std::to_string(refinement_points) + " points with " +
		remaining_summary + " (initially " + initial_summary + ")" :
	    "adaptive chart refinement reached its limit after " +
		std::to_string(refinement_attempts) + " rounds and " +
		std::to_string(refinement_points) + " points with " +
		remaining_summary + " (initially " + initial_summary + ")";
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REFINEMENT_LIMIT,
	    BREP_CDT_STAGE_ADAPTIVE_REFINEMENT, fmesh->f_id, 0, 1,
	    message.c_str());
	return false;
    }

    if (topology_chart) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_CHART_FAILED,
	    BREP_CDT_STAGE_CHART_CONSTRUCTION, fmesh->f_id, 0, 1,
	    "topology chart did not preserve the face validity invariants");
	return false;
    }

    // Now, the hard part - create local subsets, remesh them, and replace the original
    // triangles with the new ones.
    if (!fmesh->repair()) {
	bu_log("Face %d: repair FAILED!\n", fmesh->f_id);
	return false;
    }

    return true;
}

static bool
loop_edges(cdt_mesh_t *fmesh, cpolygon_t *loop)
{
    if (!fmesh || !loop)
	return false;
    cpolyedge_t *edge = loop->first_edge();
    if (!edge)
	return false;
    cpolyedge_t *first = edge;
    size_t edge_count = 0;
    const auto mesh_point = [&](long polygon_point) {
	const auto native = loop->p2o.find(polygon_point);
	if (native == loop->p2o.end())
	    return -1L;
	const auto point_3d = fmesh->p2d3d.find(native->second);
	if (point_3d == fmesh->p2d3d.end() || point_3d->second < 0 ||
		(size_t)point_3d->second >= fmesh->pnts.size())
	    return -1L;
	const auto canonical = fmesh->p2ind.find(
	    fmesh->pnts[(size_t)point_3d->second]);
	return canonical == fmesh->p2ind.end() ? -1L : canonical->second;
    };

    do {
	const long start = mesh_point(edge->v2d[0]);
	const long end = mesh_point(edge->v2d[1]);
	if (start < 0 || end < 0) {
	    bu_log("face %d has an incomplete boundary point mapping\n",
		fmesh->f_id);
	    return false;
	}
	fmesh->ep.insert(start);
	fmesh->ep.insert(end);
	if (start != end) {
	    const uedge_t boundary_edge(start, end);
	    fmesh->brep_edges.insert(boundary_edge);
	    fmesh->ue2b_map[boundary_edge] = edge->eseg;
	}
	edge = edge->next;
	edge_count++;
	if (!edge || edge_count > loop->poly.size()) {
	    std::cerr << "infinite loop when reading loop edges\n";
	    return false;
	}
    } while (edge != first);
    return true;
}

static bool
do_triangulation(struct ON_Brep_CDT_State *s_cdt, int fi)
{
    ON_BrepFace &face = s_cdt->brep->m_F[fi];
    s_cdt->face_deadline = s_cdt->max_face_time_ms > 0 ?
	bu_gettime() + (int64_t)s_cdt->max_face_time_ms * 1000 : 0;

    // Document the min and max segment lengths - used to guide surface sampling
    int loop_cnt = face.LoopCount();
    double min_edge_seg_len = DBL_MAX;
    double max_edge_seg_len = 0;
    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	for (int lti = 0; lti < loop->TrimCount(); lti++) {
	    ON_BrepTrim *trim = loop->Trim(lti);
	    ON_BrepEdge *edge = trim->Edge();
	    if (!edge) continue;
	    const ON_Curve* crv = edge->EdgeCurveOf();
	    if (!crv) continue;
	    std::set<bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge->m_edge_index];
	    if (!epsegs.size()) continue;
	    std::set<bedge_seg_t *>::iterator e_it;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		bedge_seg_t *b = *e_it;
		double seg_dist = b->e_start->DistanceTo(*b->e_end);
		min_edge_seg_len = (min_edge_seg_len > seg_dist) ? seg_dist : min_edge_seg_len;
		max_edge_seg_len = (max_edge_seg_len < seg_dist) ? seg_dist : max_edge_seg_len;
	    }
	}
    }
    (*s_cdt->min_edge_seg_len)[face.m_face_index] = min_edge_seg_len;
    (*s_cdt->max_edge_seg_len)[face.m_face_index] = max_edge_seg_len;

    // Sample the surface, independent of the trimming curves, to get points that
    // will tie the mesh to the interior surface.
    if (!GetInteriorPoints(s_cdt, face.m_face_index)) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REFINEMENT_LIMIT,
	    BREP_CDT_STAGE_ADAPTIVE_REFINEMENT, face.m_face_index, 0, 1,
	    "initial surface subdivision exceeded the face point limit");
	return false;
    }

    cdt_mesh_t *fmesh = &s_cdt->fmeshes[face.m_face_index];
    fmesh->brep = s_cdt->brep;
    fmesh->p_cdt = (void *)s_cdt;
    fmesh->name = s_cdt->name;
    fmesh->f_id = face.m_face_index;
    fmesh->m_bRev = face.m_bRev;

    // cdt() records the final topology-chart boundary.  Clear stale native
    // loop data first, then add the original segment associations below for
    // edges that still have a one-to-one B-Rep representation.
    fmesh->brep_edges.clear();
    fmesh->chart_boundary_edges.clear();
    fmesh->ue2b_map.clear();

    // Mark singular vertices before orienting the initial triangles.  Their
    // averaged vertex normals are not a reliable face-local orientation
    // signal at a pole.
    fmesh->sv.clear();
    for (size_t i = 0; i < fmesh->pnts.size(); i++) {
	ON_3dPoint *p3d = fmesh->pnts[i];
	if (s_cdt->singular_vert_to_norms->find(p3d) != s_cdt->singular_vert_to_norms->end()) {
	    fmesh->sv.insert(fmesh->p2ind[p3d]);
	}
    }

    if (!fmesh->cdt()) {
	bu_log("Face %d: initial CDT (fmesh->cdt) FAILED\n", face.m_face_index);
	return false;
    }

    // List native loop edges in addition to the chart boundary.
    bool loops_mapped = loop_edges(fmesh, &fmesh->outer_loop);
    std::map<int, cpolygon_t*>::iterator i_it;
    for (i_it = fmesh->inner_loops.begin(); i_it != fmesh->inner_loops.end(); i_it++) {
	loops_mapped = loop_edges(fmesh, i_it->second) && loops_mapped;
    }
    if (!loops_mapped) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_CERTIFICATION_FAILED,
	    BREP_CDT_STAGE_FACE_TRIANGULATION, face.m_face_index, 0, 1,
	    "native loop boundary did not map to mesh vertices");
	return false;
    }
    fmesh->boundary_edges_update();

    /* The libbg triangulation is not guaranteed to have all the properties
     * we want out of the box - trigger a series of checks. */
    if (!refine_triangulation(s_cdt, fmesh, 0, 0))
	return false;
    const size_t geometric_degenerates =
	fmesh->geometric_degenerate_count();
    if (geometric_degenerates) {
	const std::string message = "face mesh contains " +
	    std::to_string(geometric_degenerates) +
	    " scale-aware zero-area triangles";
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_CERTIFICATION_FAILED,
	    BREP_CDT_STAGE_FACE_TRIANGULATION, face.m_face_index, 0, 1,
	    message.c_str());
	return false;
    }
    return true;
}

struct repair_toleranced_edge_tube {
    int edge_index;
    double tolerance;
    std::unique_ptr<ON_NurbsCurve> curve;
};

static std::vector<repair_toleranced_edge_tube>
repair_face_edge_tubes(const ON_BrepFace &face, double allowed)
{
    std::vector<repair_toleranced_edge_tube> edge_tubes;
    std::set<int> visited_edges;
    for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	const ON_BrepLoop *loop = face.Loop(loop_index);
	if (!loop)
	    continue;
	for (int trim_index = 0; trim_index < loop->TrimCount();
		++trim_index) {
	    const ON_BrepTrim *trim = loop->Trim(trim_index);
	    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	    if (!edge || !visited_edges.insert(edge->m_edge_index).second ||
		    !std::isfinite(edge->m_tolerance) ||
		    edge->m_tolerance <= allowed ||
		    NEAR_EQUAL(edge->m_tolerance, ON_UNSET_VALUE,
			ON_ZERO_TOLERANCE))
		continue;
	    std::unique_ptr<ON_NurbsCurve> curve(edge->NurbsCurve());
	    if (!curve || !curve->IsValid())
		continue;
	    /* Keep automatic interpretations local even when an imported edge
	     * carries an effectively unbounded tolerance. */
	    const double tube_limit = std::min(edge->m_tolerance,
		4.0 * allowed);
	    edge_tubes.push_back({edge->m_edge_index, tube_limit,
		std::move(curve)});
	}
    }
    return edge_tubes;
}

static bool
repair_point_in_edge_tube(const repair_toleranced_edge_tube &tube,
	const ON_3dPoint &point)
{
    if (!point.IsValid() || !tube.curve)
	return false;
    const ON_Interval domain = tube.curve->Domain();
    double parameter = DBL_MAX;
    if (!ON_NurbsCurve_GetClosestPoint(&parameter, tube.curve.get(), point,
	    tube.tolerance, &domain))
	return false;
    const ON_3dPoint closest = tube.curve->PointAt(parameter);
    return closest.IsValid() && closest.DistanceTo(point) <= tube.tolerance;
}

static bool
repair_edge_tube_supports(
	const std::vector<repair_toleranced_edge_tube> &edge_tubes,
	double allowed, const ON_3dPoint &surface_point,
	const ON_3dPoint &mesh_point, double deviation)
{
    if (deviation <= allowed)
	return true;
    for (const repair_toleranced_edge_tube &tube : edge_tubes) {
	if (deviation <= tube.tolerance &&
		repair_point_in_edge_tube(tube, surface_point) &&
		repair_point_in_edge_tube(tube, mesh_point))
	    return true;
    }
    return false;
}

int
cdt_test_repair_edge_tube(void)
{
    ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(10.0, 0.0, 0.0),
	ON_3dPoint(10.0, 10.0, 0.0), ON_3dPoint(0.0, 10.0, 0.0),
	ON_3dPoint(0.0, 0.0, 10.0), ON_3dPoint(10.0, 0.0, 10.0),
	ON_3dPoint(10.0, 10.0, 10.0), ON_3dPoint(0.0, 10.0, 10.0)
    };
    std::unique_ptr<ON_Brep> box(ON_BrepBox(corners));
    if (!box || box->m_F.Count() < 1)
	return 1;
    ON_BrepFace &face = box->m_F[0];
    ON_BrepLoop *loop = face.OuterLoop();
    ON_BrepTrim *trim = loop && loop->TrimCount() ? loop->Trim(0) : NULL;
    ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
    if (!edge)
	return 2;
    edge->m_tolerance = 5.0;
    std::vector<repair_toleranced_edge_tube> tubes =
	repair_face_edge_tubes(face, 2.0);
    if (tubes.size() != 1 || !tubes[0].curve ||
	    !NEAR_EQUAL(tubes[0].tolerance, 5.0, ON_ZERO_TOLERANCE))
	return 3;
    const ON_Interval domain = tubes[0].curve->Domain();
    const ON_3dPoint first = tubes[0].curve->PointAt(domain.Min());
    const ON_3dPoint last = tubes[0].curve->PointAt(domain.Max());
    const ON_3dPoint middle = tubes[0].curve->PointAt(domain.Mid());
    ON_3dVector tangent = last - first;
    ON_3dVector axis = std::fabs(tangent.x) < 0.9 * tangent.Length() ?
	ON_3dVector(1.0, 0.0, 0.0) : ON_3dVector(0.0, 1.0, 0.0);
    ON_3dVector offset = ON_CrossProduct(tangent, axis);
    if (!middle.IsValid() || !offset.Unitize())
	return 4;
    const ON_3dPoint surface_point = middle + 3.0 * offset;
    const ON_3dPoint mesh_point = middle - offset;
    if (!repair_edge_tube_supports(tubes, 2.0, surface_point,
	    mesh_point, surface_point.DistanceTo(mesh_point)))
	return 5;
    const ON_3dPoint outside = middle + 6.0 * offset;
    if (repair_edge_tube_supports(tubes, 2.0, outside, mesh_point,
	    outside.DistanceTo(mesh_point)))
	return 6;
    edge->m_tolerance = 50.0;
    tubes = repair_face_edge_tubes(face, 2.0);
    return tubes.size() == 1 &&
	NEAR_EQUAL(tubes[0].tolerance, 8.0, ON_ZERO_TOLERANCE) ? 0 : 7;
}

/* Repair may make a narrowly scoped interpretation of a weakly-simple trim
 * loop after the topology-preserving rigorous path has failed.  Clipper
 * cleans the filled chart set, while topology_preserving_clean_triangulation
 * requires the resulting boundary to remain covered by the original chart
 * segments and maps any new point back onto this face's surface.  Keep this
 * retry out of ordinary tessellation so an accepted result is reported as an
 * approximation with the responsible B-Rep face and edges. */
static bool
repair_surface_deviation_triangles(cdt_mesh_t *mesh, double allowed,
	std::vector<triangle_t> &problematic, double *maximum_deviation,
	bool accept_edge_tubes)
{
    problematic.clear();
    if (maximum_deviation)
	*maximum_deviation = 0.0;
    if (!mesh || !mesh->brep || mesh->f_id < 0 ||
	    mesh->f_id >= mesh->brep->m_F.Count() ||
	    mesh->m_face_charts.empty() || !(allowed > 0.0) ||
	    !std::isfinite(allowed))
	return false;
    const ON_Surface *surface = mesh->brep->m_F[mesh->f_id].SurfaceOf();
    if (!surface)
	return false;
    const std::set<uedge_t> boundary_edges = mesh->get_boundary_edges();

    /* A valid B-Rep may explicitly declare that an edge and an incident
     * surface disagree by more than the requested tessellation tolerance.
     * A locally inferred repair is allowed to bridge that discrepancy, but
     * only inside the uncertain edge's own bounded neighborhood.  Requiring
     * both the surface sample and its closest mesh point to lie in the same
     * edge tube prevents a large edge tolerance from relaxing the rest of the
     * face. */
    const ON_BrepFace &brep_face = mesh->brep->m_F[mesh->f_id];
    std::vector<repair_toleranced_edge_tube> edge_tubes;
    if (accept_edge_tubes)
	edge_tubes = repair_face_edge_tubes(brep_face, allowed);
    typedef std::array<long, 3> vertex_key;
    std::map<vertex_key, triangle_t> active;
    for (const triangle_t &triangle : mesh->tris_vect) {
	if (!mesh->tri_active(triangle.ind))
	    continue;
	vertex_key key = {triangle.v[0], triangle.v[1], triangle.v[2]};
	std::sort(key.begin(), key.end());
	active[key] = triangle;
    }
    std::set<size_t> selected;
    bool evaluated = false;
    for (const triangle_t &native : mesh->tris_2d) {
	long mapped[3] = {-1, -1, -1};
	bool have_mapping = true;
	for (int corner = 0; corner < 3; ++corner) {
	    const auto point_3d = mesh->p2d3d.find(native.v[corner]);
	    if (point_3d == mesh->p2d3d.end() || point_3d->second < 0 ||
		    (size_t)point_3d->second >= mesh->pnts.size()) {
		have_mapping = false;
		break;
	    }
	    const auto canonical = mesh->p2ind.find(
		mesh->pnts[(size_t)point_3d->second]);
	    if (canonical == mesh->p2ind.end()) {
		have_mapping = false;
		break;
	    }
	    mapped[corner] = canonical->second;
	}
	if (!have_mapping)
	    continue;
	vertex_key key = {mapped[0], mapped[1], mapped[2]};
	std::sort(key.begin(), key.end());
	const auto triangle_entry = active.find(key);
	if (triangle_entry == active.end())
	    continue;
	const long native_vertices[3] = {
	    native.v[0], native.v[1], native.v[2]};
	ON_2dPoint sample = ON_2dPoint::UnsetPoint;
	for (const cdt_face_chart &chart : mesh->m_face_charts) {
	    if (chart.triangle_interior_sample(native_vertices, sample))
		break;
	}
	if (!sample.IsValid())
	    continue;
	const ON_3dPoint surface_point = surface->PointAt(sample.x, sample.y);
	if (!surface_point.IsValid())
	    continue;
	point_t test_point;
	point_t corners[3];
	VSET(test_point, surface_point.x, surface_point.y, surface_point.z);
	for (int corner = 0; corner < 3; ++corner)
	    VMOVE(corners[corner], *mesh->pnts[(size_t)mapped[corner]]);
	point_t closest_point;
	const double distance = bg_tri_closest_pt(&closest_point, test_point,
	    corners[0], corners[1], corners[2]);
	if (!std::isfinite(distance))
	    continue;
	evaluated = true;
	double triangle_deviation = distance;
	bool unsupported_deviation = accept_edge_tubes &&
	    !repair_edge_tube_supports(edge_tubes, allowed, surface_point,
		ON_3dPoint(closest_point), distance);
	int maximum_edge = -1;
	for (int edge = 0; edge < 3; ++edge) {
	    const long native_edge[2] = {
		native.v[edge], native.v[(edge + 1) % 3]};
	    ON_2dPoint edge_sample = ON_2dPoint::UnsetPoint;
	    ON_2dPoint chart_sample = ON_2dPoint::UnsetPoint;
	    for (const cdt_face_chart &chart : mesh->m_face_charts) {
		if (chart.edge_midpoint_sample(native_edge, edge_sample,
			chart_sample))
		    break;
	    }
	    if (!edge_sample.IsValid())
		continue;
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const ON_Interval domain = surface->Domain(direction);
		const double period = domain.Length();
		double &coordinate = direction ? edge_sample.y : edge_sample.x;
		coordinate = domain.Min() + std::fmod(
		    coordinate - domain.Min(), period);
		if (coordinate < domain.Min())
		    coordinate += period;
	    }
	    const ON_3dPoint edge_surface = surface->PointAt(
		edge_sample.x, edge_sample.y);
	    if (!edge_surface.IsValid())
		continue;
	    const ON_3dPoint &first = *mesh->pnts[(size_t)mapped[edge]];
	    const ON_3dPoint &second = *mesh->pnts[
		(size_t)mapped[(edge + 1) % 3]];
	    const ON_3dVector chord = second - first;
	    const double chord_length_sq = chord * chord;
	    double parameter = chord_length_sq > DBL_EPSILON ?
		((edge_surface - first) * chord) / chord_length_sq : 0.0;
	    parameter = std::max(0.0, std::min(1.0, parameter));
	    const ON_3dPoint edge_mesh_point = first + parameter * chord;
	    const double edge_deviation = edge_surface.DistanceTo(
		edge_mesh_point);
	    unsupported_deviation = unsupported_deviation ||
		(accept_edge_tubes && !repair_edge_tube_supports(edge_tubes,
		    allowed, edge_surface, edge_mesh_point, edge_deviation));
	    if (edge_deviation > triangle_deviation) {
		triangle_deviation = edge_deviation;
		maximum_edge = edge;
	    }
	}
	const bool new_maximum = maximum_deviation &&
	    triangle_deviation > *maximum_deviation;
	if (maximum_deviation)
	    *maximum_deviation = std::max(*maximum_deviation,
		triangle_deviation);
	if (new_maximum && triangle_deviation > allowed &&
		getenv("BRLCAD_CDT_DUMP_FAILURES") &&
		getenv("BRLCAD_CDT_DUMP_FAILURES")[0] &&
		!BU_STR_EQUAL(getenv("BRLCAD_CDT_DUMP_FAILURES"), "0")) {
	    const uedge_t maximum_mesh_edge = maximum_edge >= 0 ?
		uedge_t(mapped[maximum_edge], mapped[(maximum_edge + 1) % 3]) :
		uedge_t(-1, -1);
	    const bool boundary = maximum_edge >= 0 &&
		boundary_edges.find(maximum_mesh_edge) != boundary_edges.end();
	    const bool brep_edge = maximum_edge >= 0 &&
		mesh->brep_edges.find(maximum_mesh_edge) !=
		mesh->brep_edges.end();
	    const auto segment_entry = mesh->ue2b_map.find(maximum_mesh_edge);
	    const int brep_edge_index = segment_entry != mesh->ue2b_map.end() &&
		segment_entry->second ? segment_entry->second->edge_ind : -1;
	    const double brep_edge_tolerance = brep_edge_index >= 0 &&
		brep_edge_index < mesh->brep->m_E.Count() ?
		mesh->brep->m_E[brep_edge_index].m_tolerance : -1.0;
	    bu_log("Face %d: local surface deviation %.17g at triangle %zu "
		"(%ld,%ld,%ld), source=%s edge=%d boundary=%d brep=%d "
		"brep_edge=%d tolerance=%.17g\n",
		mesh->f_id, triangle_deviation, triangle_entry->second.ind,
		mapped[0], mapped[1], mapped[2],
		maximum_edge < 0 ? "interior" : "edge", maximum_edge,
		boundary ? 1 : 0, brep_edge ? 1 : 0, brep_edge_index,
		brep_edge_tolerance);
	}
	if (triangle_deviation > allowed &&
		(!accept_edge_tubes || unsupported_deviation) &&
		selected.insert(triangle_entry->second.ind).second)
	    problematic.push_back(triangle_entry->second);
    }
    return evaluated;
}

static bool
repair_approximate_face_triangulation(struct ON_Brep_CDT_State *s_cdt,
	int face_index, double allowed_deviation)
{
    if (!s_cdt || !s_cdt->brep || face_index < 0 ||
	    face_index >= s_cdt->brep->m_F.Count())
	return false;
    const auto entry = s_cdt->fmeshes.find(face_index);
    if (entry == s_cdt->fmeshes.end())
	return false;
    cdt_mesh_t *mesh = &entry->second;
    const struct brep_cdt_diagnostic source_diagnostic =
	s_cdt->failed_face_diagnostics[face_index];
    if (source_diagnostic.stage != BREP_CDT_STAGE_PSLG_VALIDATION &&
	    source_diagnostic.stage != BREP_CDT_STAGE_DETRIA)
	return false;
    const auto rebuild = [&]() {
	mesh->brep_edges.clear();
	mesh->chart_boundary_edges.clear();
	mesh->ue2b_map.clear();
	if (!mesh->cdt(true))
	    return false;
	bool loops_mapped = loop_edges(mesh, &mesh->outer_loop);
	for (const auto &inner : mesh->inner_loops)
	    loops_mapped = loop_edges(mesh, inner.second) && loops_mapped;
	if (!loops_mapped)
	    return false;
	mesh->boundary_edges_update();
	return refine_triangulation(s_cdt, mesh, 0, 0, true) &&
	    !mesh->geometric_degenerate_count();
    };
    if (!rebuild()) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_CERTIFICATION_FAILED,
	    BREP_CDT_STAGE_FACE_TRIANGULATION, face_index, 0, 1,
	    "locally reconstructed face did not pass mesh validation");
	return false;
    }
    size_t inserted_points = 0;
    int refinement_rounds = 0;
    double maximum_deviation = 0.0;
    while (refinement_rounds < MAX_CHART_REFINEMENT_ATTEMPTS &&
	    inserted_points < MAX_CHART_REFINEMENT_POINTS) {
	std::vector<triangle_t> problematic;
	if (!repair_surface_deviation_triangles(mesh, allowed_deviation,
		problematic, &maximum_deviation, false) || problematic.empty())
	    break;
	const bool dump_refinement = getenv("BRLCAD_CDT_DUMP_FAILURES") &&
	    getenv("BRLCAD_CDT_DUMP_FAILURES")[0] &&
	    !BU_STR_EQUAL(getenv("BRLCAD_CDT_DUMP_FAILURES"), "0");
	if (dump_refinement)
	    bu_log("Face %d: local surface refinement round %d found %zu "
		"triangles over %.17g (maximum %.17g)\n", face_index,
		refinement_rounds + 1, problematic.size(),
		allowed_deviation, maximum_deviation);
	size_t inserted = mesh->split_problem_triangle_edges(problematic,
	    MAX_CHART_REFINEMENT_POINTS - inserted_points);
	if (!inserted)
	    inserted = mesh->refine_problem_triangles(problematic,
		MAX_CHART_REFINEMENT_POINTS - inserted_points);
	if (dump_refinement)
	    bu_log("Face %d: local surface refinement round %d inserted %zu "
		"points\n", face_index, refinement_rounds + 1, inserted);
	if (!inserted)
	    break;
	inserted_points += inserted;
	refinement_rounds++;
	if (!rebuild())
	    return false;
    }
    std::vector<triangle_t> remaining;
    if (!repair_surface_deviation_triangles(mesh, allowed_deviation,
	    remaining, &maximum_deviation, true) || !remaining.empty()) {
	bu_log("Face %d: local surface refinement left %zu triangles over "
	    "deviation %.17g after %d rounds and %zu points\n", face_index,
	    remaining.size(), allowed_deviation, refinement_rounds,
	    inserted_points);
	return false;
    }
    if (inserted_points)
	bu_log("Face %d: locally refined the reconstructed surface with %zu "
	    "points in %d rounds (maximum sampled deviation %.17g)\n",
	    face_index, inserted_points, refinement_rounds,
	    maximum_deviation);
    return true;
}

static void
apply_derived_point_welds(struct ON_Brep_CDT_State *s_cdt,
	const std::map<ON_3dPoint *, ON_3dPoint *> &welds)
{
    if (!s_cdt || welds.empty())
	return;
    const auto canonical = [&](ON_3dPoint *point) {
	ON_3dPoint *current = point;
	std::set<ON_3dPoint *> visited;
	while (current && visited.insert(current).second) {
	    const auto next = welds.find(current);
	    if (next == welds.end() || next->second == current)
		break;
	    current = next->second;
	}
	return current;
    };

    for (auto &vertex : *s_cdt->vert_pnts)
	vertex.second = canonical(vertex.second);
    std::set<ON_3dPoint *> canonical_edge_points;
    for (ON_3dPoint *point : *s_cdt->edge_pnts)
	canonical_edge_points.insert(canonical(point));
    s_cdt->edge_pnts->swap(canonical_edge_points);
    for (auto &face : s_cdt->fmeshes) {
	cdt_mesh_t &mesh = face.second;
	for (ON_3dPoint *&point : mesh.pnts)
	    point = canonical(point);
	mesh.p2ind.clear();
	for (size_t i = 0; i < mesh.pnts.size(); ++i)
	    mesh.p2ind[mesh.pnts[i]] = (long)i;
    }
    for (auto &edge_segments : s_cdt->e2polysegs) {
	for (bedge_seg_t *segment : edge_segments.second) {
	    if (!segment)
		continue;
	    segment->e_start = canonical(segment->e_start);
	    segment->e_end = canonical(segment->e_end);
	    segment->e_root_start = canonical(segment->e_root_start);
	    segment->e_root_end = canonical(segment->e_root_end);
	}
    }
    for (auto &face_poles : s_cdt->strim_pnts) {
	for (auto &pole : face_poles.second)
	    pole.second = canonical(pole.second);
    }
    std::map<ON_3dPoint *, ON_3dPoint *> canonical_singular_norms;
    for (const auto &normal : *s_cdt->singular_vert_to_norms) {
	ON_3dPoint *point = canonical(normal.first);
	if (point && canonical_singular_norms.find(point) ==
		canonical_singular_norms.end())
	    canonical_singular_norms[point] = normal.second;
    }
    s_cdt->singular_vert_to_norms->swap(canonical_singular_norms);
}

/* Weld distinct topology vertices only when the same trimmed face places
 * them at one singular side of its surface.  This repairs imported sphere
 * caps which visit a geometric pole with more than one B-Rep vertex identity.
 * Proximity alone is not sufficient: candidates must share both a face and
 * a specific analytic surface singularity, and each component remains inside
 * the caller's scale-limited modeling tolerance. */
static size_t
collapse_coincident_surface_poles(struct ON_Brep_CDT_State *s_cdt)
{
    if (!s_cdt || !s_cdt->brep || !s_cdt->vert_pnts ||
	    !std::isfinite(s_cdt->absmin) || s_cdt->absmin <= 0.0)
	return 0;
    const double tolerance = std::min((double)BN_TOL_DIST,
	(double)s_cdt->absmin);
    if (!(tolerance > 0.0))
	return 0;

    std::map<ON_3dPoint *, ON_3dPoint *> welds;
    size_t welded_vertices = 0;
    for (int face_index = 0; face_index < s_cdt->brep->m_F.Count();
	    ++face_index) {
	const ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	const ON_Surface *surface = face.SurfaceOf();
	if (!surface)
	    continue;
	for (int side = 0; side < 4; ++side) {
	    if (!surface->IsSingular(side))
		continue;
	    const int open_direction = (side == 0 || side == 2) ? 1 : 0;
	    const int closed_direction = 1 - open_direction;
	    ON_2dPoint pole_uv;
	    pole_uv[open_direction] = (side == 0 || side == 3) ?
		surface->Domain(open_direction).Min() :
		surface->Domain(open_direction).Max();
	    pole_uv[closed_direction] =
		surface->Domain(closed_direction).Mid();
	    const ON_3dPoint pole = surface->PointAt(pole_uv.x, pole_uv.y);
	    if (!pole.IsValid())
		continue;

	    std::set<int> face_vertices;
	    std::map<int, std::vector<const ON_BrepTrim *>> vertex_trims;
	    for (int loop_index = 0; loop_index < face.LoopCount();
		    ++loop_index) {
		const ON_BrepLoop *loop = face.Loop(loop_index);
		if (!loop)
		    continue;
		for (int trim_index = 0; trim_index < loop->TrimCount();
			++trim_index) {
		    const ON_BrepTrim *trim = loop->Trim(trim_index);
		    if (!trim)
			continue;
		    for (int endpoint = 0; endpoint < 2; ++endpoint) {
			const int vertex_index = trim->m_vi[endpoint];
			const auto point = s_cdt->vert_pnts->find(vertex_index);
			if (point != s_cdt->vert_pnts->end() && point->second &&
				point->second->DistanceTo(pole) <= tolerance) {
			    face_vertices.insert(vertex_index);
			    if (trim->m_ei >= 0)
				vertex_trims[vertex_index].push_back(trim);
			}
		    }
		}
	    }
	    if (face_vertices.size() < 2)
		continue;

	    struct pole_component {
		ON_BoundingBox bounds;
		std::vector<int> vertices;
	    };
	    std::vector<pole_component> components;
	    for (int vertex_index : face_vertices) {
		ON_3dPoint *point = (*s_cdt->vert_pnts)[vertex_index];
		bool joined = false;
		for (pole_component &component : components) {
		    ON_BoundingBox combined = component.bounds;
		    combined.Set(*point, true);
		    if (combined.Diagonal().Length() > tolerance)
			continue;
		    component.bounds = combined;
		    component.vertices.push_back(vertex_index);
		    joined = true;
		    break;
		}
		if (!joined) {
		    pole_component component;
		    component.bounds = ON_BoundingBox(*point, *point);
		    component.vertices.push_back(vertex_index);
		    components.push_back(component);
		}
	    }
	    for (const pole_component &component : components) {
		if (component.vertices.size() < 2)
		    continue;
		std::vector<int> parents(component.vertices.size(), -1);
		const auto root = [&parents](int item) {
		    int representative = item;
		    while (parents[(size_t)representative] >= 0)
			representative = parents[(size_t)representative];
		    while (item != representative) {
			const int next = parents[(size_t)item];
			parents[(size_t)item] = representative;
			item = next;
		    }
		    return representative;
		};
		for (size_t first = 0; first < component.vertices.size();
			++first) {
		    for (size_t second = first + 1;
			    second < component.vertices.size(); ++second) {
			bool retrace = false;
			for (const ON_BrepTrim *first_trim :
				vertex_trims[component.vertices[first]]) {
			    for (const ON_BrepTrim *second_trim :
				    vertex_trims[component.vertices[second]]) {
				retrace = cdt_trim_pcurves_retrace(first_trim,
				    second_trim);
				if (retrace)
				    break;
			    }
			    if (retrace)
				break;
			}
			if (!retrace)
			    continue;
			int first_root = root((int)first);
			int second_root = root((int)second);
			if (first_root == second_root)
			    continue;
			if (parents[(size_t)first_root] >
				parents[(size_t)second_root])
			    std::swap(first_root, second_root);
			parents[(size_t)first_root] +=
			    parents[(size_t)second_root];
			parents[(size_t)second_root] = first_root;
		    }
		}
		std::map<int, std::vector<int>> weld_components;
		for (size_t vertex = 0; vertex < component.vertices.size();
			++vertex)
		    weld_components[root((int)vertex)].push_back(
			component.vertices[vertex]);
		for (const auto &weld_component : weld_components) {
		    if (weld_component.second.size() < 2)
			continue;
		    const int representative_index = *std::min_element(
			weld_component.second.begin(),
			weld_component.second.end());
		    ON_3dPoint *representative =
			(*s_cdt->vert_pnts)[representative_index];
		    for (int vertex_index : weld_component.second) {
			ON_3dPoint *point =
			    (*s_cdt->vert_pnts)[vertex_index];
			if (point != representative &&
				welds.emplace(point, representative).second)
			    welded_vertices++;
		    }
		}
	    }
	}
    }
    if (welds.empty())
	return 0;
    for (const auto &weld : welds)
	s_cdt->collapsed_edge_pnts[weld.first] = weld.second;
    apply_derived_point_welds(s_cdt, welds);
    bu_log("%s: welded %zu coincident surface-pole B-Rep vert%s within "
	"%.17g\n", s_cdt->name ? s_cdt->name : "BREP",
	welded_vertices, welded_vertices == 1 ? "ex" : "ices", tolerance);
    return welded_vertices;
}

/* Collapse only explicit B-Rep edges whose complete curve is contained in
 * the effective modeling tolerance.  Honor a larger finite tolerance
 * declared by the edge, but never exceed the caller's feature resolution.
 * Endpoint proximity alone is insufficient: a long curve may close back on
 * itself or join two otherwise unrelated sheets.  The NURBS control-polygon
 * length and curve bounding box are both conservative guards for the whole
 * edge.
 *
 * This is derived mesh state.  It deliberately leaves the source and working
 * B-Reps untouched and welds the already shared edge samples in every
 * incident face mesh.  Components retain a bounded envelope so a chain of
 * individually short edges cannot transitively collapse a wider feature. */
static size_t
collapse_subtolerance_brep_edges(struct ON_Brep_CDT_State *s_cdt)
{
    if (!s_cdt || !s_cdt->brep || !s_cdt->w3dpnts ||
	    s_cdt->w3dpnts->empty())
	return 0;

    /* BN_TOL_DIST and imported edge tolerances can exceed an entire valid
     * micron-scale solid.  Never weld across a distance larger than the
     * minimum feature resolution derived from the caller's tolerance. */
    if (!std::isfinite(s_cdt->absmin) || s_cdt->absmin <= 0.0)
	return 0;
    struct collapse_candidate {
	int edge_index;
	ON_BoundingBox curve_bounds;
	std::vector<ON_3dPoint *> points;
	double tolerance;
    };
    std::vector<collapse_candidate> candidates;
    std::set<ON_3dPoint *> candidate_point_set;
    for (int edge_index = 0; edge_index < s_cdt->brep->m_E.Count();
	    ++edge_index) {
	ON_BrepEdge &edge = s_cdt->brep->m_E[edge_index];
	if (edge.m_vi[0] < 0 || edge.m_vi[1] < 0 ||
		edge.m_vi[0] == edge.m_vi[1])
	    continue;
	const auto segments = s_cdt->e2polysegs.find(edge_index);
	if (segments == s_cdt->e2polysegs.end() ||
		segments->second.empty())
	    continue;
	bedge_seg_t *first_segment = *segments->second.begin();
	double modeling_tolerance = BN_TOL_DIST;
	if (std::isfinite(edge.m_tolerance) && edge.m_tolerance > 0.0 &&
		!NEAR_EQUAL(edge.m_tolerance, ON_UNSET_VALUE,
		    ON_ZERO_TOLERANCE))
	    modeling_tolerance = std::max(modeling_tolerance,
		(double)edge.m_tolerance);
	const double collapse_tolerance = std::min(
	    (double)s_cdt->absmin, modeling_tolerance);
	if (!first_segment || !first_segment->nc ||
		!std::isfinite(first_segment->cp_len) ||
		first_segment->cp_len <= 0.0 ||
		first_segment->cp_len > collapse_tolerance)
	    continue;
	const ON_BoundingBox curve_bounds =
	    first_segment->nc->BoundingBox();
	if (!curve_bounds.IsValid())
	    continue;
	const double envelope = curve_bounds.Diagonal().Length();
	if (!std::isfinite(envelope) || envelope > collapse_tolerance)
	    continue;

	ON_3dPoint *first_vertex = (*s_cdt->vert_pnts)[edge.m_vi[0]];
	ON_3dPoint *second_vertex = (*s_cdt->vert_pnts)[edge.m_vi[1]];
	if (!first_vertex || !second_vertex ||
		s_cdt->singular_vert_to_norms->find(first_vertex) !=
		s_cdt->singular_vert_to_norms->end() ||
		s_cdt->singular_vert_to_norms->find(second_vertex) !=
		s_cdt->singular_vert_to_norms->end())
	    continue;

	std::set<ON_3dPoint *> members;
	for (bedge_seg_t *segment : segments->second) {
	    if (!segment || !segment->e_start || !segment->e_end)
		continue;
	    members.insert(segment->e_start);
	    members.insert(segment->e_end);
	}
	if (members.size() < 2)
	    continue;
	collapse_candidate candidate = {edge_index, curve_bounds,
	    std::vector<ON_3dPoint *>(members.begin(), members.end()),
	    collapse_tolerance};
	candidates.push_back(candidate);
	candidate_point_set.insert(members.begin(), members.end());
    }

    if (candidates.empty())
	return 0;

    /* Tiny explicit edges are rare.  Allocate union-find state only for
     * samples referenced by pre-screened candidates, not for every surface
     * and refinement point in a potentially million-point tessellation. */
    std::vector<ON_3dPoint *> candidate_points;
    candidate_points.reserve(candidate_point_set.size());
    for (ON_3dPoint *point : *s_cdt->w3dpnts) {
	if (candidate_point_set.find(point) != candidate_point_set.end())
	    candidate_points.push_back(point);
    }
    if (candidate_points.size() != candidate_point_set.size())
	return 0;
    const size_t point_count = candidate_points.size();
    std::map<ON_3dPoint *, size_t> point_index;
    for (size_t i = 0; i < point_count; ++i)
	point_index[candidate_points[i]] = i;

    std::vector<size_t> parent(point_count);
    std::vector<size_t> component_size(point_count, 1);
    std::vector<double> component_tolerance(point_count, DBL_MAX);
    std::vector<ON_BoundingBox> component_bounds;
    component_bounds.reserve(point_count);
    for (size_t i = 0; i < point_count; ++i) {
	parent[i] = i;
	component_bounds.push_back(ON_BoundingBox(*candidate_points[i],
	    *candidate_points[i]));
    }
    const auto root = [&](size_t point) {
	size_t current = point;
	while (parent[current] != current)
	    current = parent[current];
	return current;
    };
    const auto grow_bounds = [](ON_BoundingBox &target,
	    const ON_BoundingBox &source) {
	target.Set(source.Min(), true);
	target.Set(source.Max(), true);
    };

    std::map<int, double> accepted_edges;
    for (const collapse_candidate &candidate : candidates) {
	std::set<size_t> members;
	for (ON_3dPoint *point : candidate.points) {
	    const auto index = point_index.find(point);
	    if (index != point_index.end())
		members.insert(index->second);
	}
	if (members.size() < 2)
	    continue;

	std::set<size_t> roots;
	ON_BoundingBox combined = candidate.curve_bounds;
	double combined_tolerance = candidate.tolerance;
	for (size_t member : members)
	    roots.insert(root(member));
	for (size_t component : roots) {
	    grow_bounds(combined, component_bounds[component]);
	    combined_tolerance = std::min(combined_tolerance,
		component_tolerance[component]);
	}
	const double combined_envelope = combined.Diagonal().Length();
	if (!std::isfinite(combined_envelope) ||
		combined_envelope > combined_tolerance)
	    continue;

	const size_t combined_root = *roots.begin();
	for (size_t component : roots) {
	    if (component == combined_root)
		continue;
	    parent[component] = combined_root;
	    component_size[combined_root] += component_size[component];
	}
	component_bounds[combined_root] = combined;
	component_tolerance[combined_root] = combined_tolerance;
	accepted_edges[candidate.edge_index] = candidate.tolerance;
    }

    if (accepted_edges.empty())
	return 0;

    std::map<ON_3dPoint *, int> topology_vertex;
    for (const auto &vertex : *s_cdt->vert_pnts)
	topology_vertex[vertex.second] = vertex.first;
    std::map<size_t, std::vector<size_t>> components;
    for (size_t i = 0; i < point_count; ++i) {
	const size_t component = root(i);
	if (component_size[component] > 1)
	    components[component].push_back(i);
    }

    s_cdt->collapsed_edge_pnts.clear();
    for (const auto &component : components) {
	size_t representative = component.second.front();
	int representative_vertex = std::numeric_limits<int>::max();
	for (size_t member : component.second) {
	    ON_3dPoint *point = candidate_points[member];
	    const auto vertex = topology_vertex.find(point);
	    if (vertex != topology_vertex.end() &&
		    vertex->second < representative_vertex) {
		representative = member;
		representative_vertex = vertex->second;
	    } else if (representative_vertex ==
		    std::numeric_limits<int>::max() &&
		    member < representative) {
		representative = member;
	    }
	}
	ON_3dPoint *representative_point = candidate_points[representative];
	for (size_t member : component.second)
	    s_cdt->collapsed_edge_pnts[candidate_points[member]] =
		representative_point;
    }
    s_cdt->collapsed_edges.clear();
    for (const auto &accepted : accepted_edges)
	s_cdt->collapsed_edges.insert(accepted.first);

    for (auto &vertex : *s_cdt->vert_pnts) {
	const auto canonical = s_cdt->collapsed_edge_pnts.find(vertex.second);
	if (canonical != s_cdt->collapsed_edge_pnts.end())
	    vertex.second = canonical->second;
    }
    std::set<ON_3dPoint *> canonical_edge_points;
    for (ON_3dPoint *point : *s_cdt->edge_pnts) {
	const auto canonical = s_cdt->collapsed_edge_pnts.find(point);
	canonical_edge_points.insert(canonical ==
		s_cdt->collapsed_edge_pnts.end() ? point : canonical->second);
    }
    s_cdt->edge_pnts->swap(canonical_edge_points);
    for (auto &face : s_cdt->fmeshes) {
	cdt_mesh_t &mesh = face.second;
	for (ON_3dPoint *&point : mesh.pnts) {
	    const auto canonical = s_cdt->collapsed_edge_pnts.find(point);
	    if (canonical != s_cdt->collapsed_edge_pnts.end())
		point = canonical->second;
	}
	mesh.p2ind.clear();
	for (size_t i = 0; i < mesh.pnts.size(); ++i)
	    mesh.p2ind[mesh.pnts[i]] = (long)i;
    }

    /* Adjacent ordinary edges share the collapsed topology vertices.  Weld
     * every segment endpoint that references one of those vertices, not just
     * the segments belonging to the tiny edge itself, so later shared-edge
     * refinement cannot reintroduce the discarded point identity. */
    for (auto &edge_segments : s_cdt->e2polysegs) {
	for (bedge_seg_t *segment : edge_segments.second) {
	    if (!segment)
		continue;
	    const auto start = s_cdt->collapsed_edge_pnts.find(
		segment->e_start);
	    const auto end = s_cdt->collapsed_edge_pnts.find(segment->e_end);
	    const auto root_start = s_cdt->collapsed_edge_pnts.find(
		segment->e_root_start);
	    const auto root_end = s_cdt->collapsed_edge_pnts.find(
		segment->e_root_end);
	    if (start != s_cdt->collapsed_edge_pnts.end())
		segment->e_start = start->second;
	    if (end != s_cdt->collapsed_edge_pnts.end())
		segment->e_end = end->second;
	    if (root_start != s_cdt->collapsed_edge_pnts.end())
		segment->e_root_start = root_start->second;
	    if (root_end != s_cdt->collapsed_edge_pnts.end())
		segment->e_root_end = root_end->second;
	}
    }
    for (const auto &accepted : accepted_edges) {
	const int edge_index = accepted.first;
	const ON_BrepEdge &edge = s_cdt->brep->m_E[edge_index];
	const bedge_seg_t *segment = *s_cdt->e2polysegs[edge_index].begin();
	bu_log("%s: collapsed sub-tolerance B-Rep edge %d (V%d/V%d, "
	    "control polygon %.17g, envelope %.17g, effective tolerance "
	    "%.17g, BN_TOL_DIST %.17g)\n",
	    s_cdt->name ? s_cdt->name : "BREP", edge_index,
	    edge.m_vi[0], edge.m_vi[1], segment->cp_len,
	    segment->nc->BoundingBox().Diagonal().Length(),
	    accepted.second, (double)BN_TOL_DIST);
    }
    return accepted_edges.size();
}

int
cdt_test_subtolerance_edge_collapse(void)
{
    ON_Brep source;
    source.NewVertex(ON_3dPoint(0.0, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(0.0004, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(0.0008, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(100.0, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(200.0, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(200.0001, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(400.0, 0.0, 0.0));
    source.NewVertex(ON_3dPoint(400.001, 0.0, 0.0));
    ON_BrepVertex &v0 = source.m_V[0];
    ON_BrepVertex &v1 = source.m_V[1];
    ON_BrepVertex &v2 = source.m_V[2];
    ON_BrepVertex &v3 = source.m_V[3];
    ON_BrepVertex &v4 = source.m_V[4];
    ON_BrepVertex &v5 = source.m_V[5];
    ON_BrepVertex &v6 = source.m_V[6];
    ON_BrepVertex &v7 = source.m_V[7];
    source.NewEdge(v1, v0, source.AddEdgeCurve(new ON_LineCurve(
	v1.Point(), v0.Point())));
    source.NewEdge(v1, v2, source.AddEdgeCurve(new ON_LineCurve(
	v1.Point(), v2.Point())));
    source.NewEdge(v2, v3, source.AddEdgeCurve(new ON_LineCurve(
	v2.Point(), v3.Point())));
    /* These endpoints are close, but the curve takes a long detour.  Whole
     * curve bounds must reject it despite endpoint proximity. */
    ON_3dPointArray detour_points;
    detour_points.Append(v4.Point());
    detour_points.Append(ON_3dPoint(300.0, 100.0, 0.0));
    detour_points.Append(v5.Point());
    source.NewEdge(v4, v5, source.AddEdgeCurve(new ON_PolylineCurve(
	detour_points)));
    ON_BrepEdge &declared_tolerant = source.NewEdge(v6, v7,
	source.AddEdgeCurve(new ON_LineCurve(v6.Point(), v7.Point())));
    declared_tolerant.m_tolerance = 0.01;

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(&source,
	"sub-tolerance edge test");
    state->brep = new ON_Brep(source);
    for (int vertex_index = 0; vertex_index < state->brep->m_V.Count();
	    ++vertex_index) {
	ON_3dPoint *point = new ON_3dPoint(
	    state->brep->m_V[vertex_index].Point());
	(*state->vert_pnts)[vertex_index] = point;
	CDT_Add3DPnt(state, point, -1, vertex_index, -1, -1, -1, -1);
	state->edge_pnts->insert(point);
    }
    for (int edge_index = 0; edge_index < state->brep->m_E.Count();
	    ++edge_index) {
	ON_BrepEdge &edge = state->brep->m_E[edge_index];
	bedge_seg_t *segment = new bedge_seg_t;
	segment->edge_ind = edge_index;
	segment->brep = state->brep;
	segment->p_cdt = state;
	segment->nc = edge.EdgeCurveOf()->NurbsCurve();
	segment->cp_len = segment->nc->ControlPolygonLength();
	segment->e_start = (*state->vert_pnts)[edge.m_vi[0]];
	segment->e_end = (*state->vert_pnts)[edge.m_vi[1]];
	segment->e_root_start = segment->e_start;
	segment->e_root_end = segment->e_end;
	state->e2polysegs[edge_index].insert(segment);
    }

    ON_3dPoint *expected = (*state->vert_pnts)[0];
    ON_3dPoint *expected_declared = (*state->vert_pnts)[6];
    bedge_seg_t *adjacent = *state->e2polysegs[1].begin();
    state->absmin = 1.0e-5;
    if (collapse_subtolerance_brep_edges(state) != 0) {
	ON_Brep_CDT_Destroy(state);
	return 4;
    }
    state->absmin = 0.02;
    const size_t collapsed = collapse_subtolerance_brep_edges(state);
    int result = 0;
    if (collapsed != 2 || state->collapsed_edges.size() != 2 ||
	    state->collapsed_edges.find(0) == state->collapsed_edges.end() ||
	    state->collapsed_edges.find(3) != state->collapsed_edges.end() ||
	    state->collapsed_edges.find(4) == state->collapsed_edges.end())
	result = 1;
    else if ((*state->vert_pnts)[0] != expected ||
	    (*state->vert_pnts)[1] != expected ||
	    (*state->vert_pnts)[2] == expected ||
	    (*state->vert_pnts)[6] != expected_declared ||
	    (*state->vert_pnts)[7] != expected_declared)
	result = 2;
    else if (adjacent->e_start != expected ||
	    adjacent->e_root_start != expected)
	result = 3;

    ON_Brep_CDT_Destroy(state);
    return result;
}

ON_3dVector
calc_trim_vnorm(ON_BrepVertex& v, ON_BrepTrim *trim)
{
    ON_3dPoint t1, t2;
    ON_3dVector v1 = ON_3dVector::UnsetVector;
    ON_3dVector v2 = ON_3dVector::UnsetVector;
    ON_3dVector trim_norm = ON_3dVector::UnsetVector;

    ON_Interval trange = trim->Domain();
    ON_3dPoint t_2d1 = trim->PointAt(trange[0]);
    ON_3dPoint t_2d2 = trim->PointAt(trange[1]);

    ON_Plane fplane;
    const ON_Surface *s = trim->SurfaceOf();
    double ptol = s->BoundingBox().Diagonal().Length()*0.001;
    ptol = (ptol < BREP_PLANAR_TOL) ? ptol : BREP_PLANAR_TOL;
    if (s->IsPlanar(&fplane, ptol)) {
	trim_norm = fplane.Normal();
	if (trim->Face()->m_bRev) {
	    trim_norm = trim_norm * -1;
	}
    } else {
	int ev1 = 0;
	int ev2 = 0;
	if (surface_EvNormal(s, t_2d1.x, t_2d1.y, t1, v1)) {
	    if (trim->Face()->m_bRev) {
		v1 = v1 * -1;
	    }
	    ev1 = 1;
	}
	if (surface_EvNormal(s, t_2d2.x, t_2d2.y, t2, v2)) {
	    if (trim->Face()->m_bRev) {
		v2 = v2 * -1;
	    }
	    ev2 = 1;
	}
	// If we got both of them, go with the closest one
	if (ev1 && ev2) {
	    trim_norm = (v.Point().DistanceTo(t1) < v.Point().DistanceTo(t2)) ? v1 : v2;
	}

	if (ev1 && !ev2) {
	    trim_norm = v1;
	}

	if (!ev1 && ev2) {
	    trim_norm = v2;
	}
    }

    return trim_norm;
}

static void
calc_singular_vert_norm(struct ON_Brep_CDT_State *s_cdt, int index)
{
    ON_BrepVertex& v = s_cdt->brep->m_V[index];
    int have_calculated = 0;
    ON_3dVector vnrml(0.0, 0.0, 0.0);

    if (s_cdt->singular_vert_to_norms->find((*s_cdt->vert_pnts)[index]) != (s_cdt->singular_vert_to_norms->end())) {
	// Already processed this one
	return;
    }

    //bu_log("Processing vert %d (%f %f %f)\n", index, v.Point().x, v.Point().y, v.Point().z);

    for (int eind = 0; eind != v.EdgeCount(); eind++) {
	ON_3dVector trim1_norm = ON_3dVector::UnsetVector;
	ON_3dVector trim2_norm = ON_3dVector::UnsetVector;
	ON_BrepEdge& edge = s_cdt->brep->m_E[v.m_ei[eind]];
	if (edge.TrimCount() != 2) {
	    // Don't know what to do with this yet... skip.
	    continue;
	}
	ON_BrepTrim *trim1 = edge.Trim(0);
	ON_BrepTrim *trim2 = edge.Trim(1);

	if (trim1->m_type != ON_BrepTrim::singular) {
	    trim1_norm = calc_trim_vnorm(v, trim1);
	}
	if (trim2->m_type != ON_BrepTrim::singular) {
	    trim2_norm = calc_trim_vnorm(v, trim2);
	}

	// If one of the normals is unset and the other comes from a plane, use it
	if (trim1_norm == ON_3dVector::UnsetVector && trim2_norm != ON_3dVector::UnsetVector) {
	    const ON_Surface *s2 = trim2->SurfaceOf();
	    if (!s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		continue;
	    }
	    trim1_norm = trim2_norm;
	}
	if (trim1_norm != ON_3dVector::UnsetVector && trim2_norm == ON_3dVector::UnsetVector) {
	    const ON_Surface *s1 = trim1->SurfaceOf();
	    if (!s1->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		continue;
	    }
	    trim2_norm = trim1_norm;
	}

	// If we have disagreeing normals and one of them is from a planar surface, go
	// with that one
	if (NEAR_EQUAL(ON_DotProduct(trim1_norm, trim2_norm), -1, VUNITIZE_TOL)) {
	    const ON_Surface *s1 = trim1->SurfaceOf();
	    const ON_Surface *s2 = trim2->SurfaceOf();
	    if (!s1->IsPlanar(NULL, ON_ZERO_TOLERANCE) && !s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		// Normals severely disagree, no planar surface to fall back on - can't use this
		continue;
	    }
	    if (s1->IsPlanar(NULL, ON_ZERO_TOLERANCE) && s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		// Two disagreeing planes - can't use this
		continue;
	    }
	    if (s1->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		trim2_norm = trim1_norm;
	    }
	    if (s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		trim1_norm = trim2_norm;
	    }
	}

	// Add the normals to the vnrml total
	vnrml += trim1_norm;
	vnrml += trim2_norm;
	have_calculated = 1;

    }
    if (!have_calculated) {
	return;
    }

    // Average all the successfully calculated normals into a new unit normal
    vnrml.Unitize();

    // We store this as a point to keep C++ happy...  If we try to
    // propagate the ON_3dVector type through all the CDT logic it
    // triggers issues with the compile.
    (*s_cdt->vert_avg_norms)[index] = new ON_3dPoint(vnrml);
    s_cdt->w3dnorms->push_back((*s_cdt->vert_avg_norms)[index]);

    // If we have a vertex normal, add it to the map which will allow us
    // to ascertain if a given point has such a normal.  This will allow
    // a point-based check even if we don't know a vertex index locally
    // in the code.
    (*s_cdt->singular_vert_to_norms)[(*s_cdt->vert_pnts)[index]] = (*s_cdt->vert_avg_norms)[index];

}

/* The face-local certificates cannot detect chords from distinct faces that
 * cross in model space.  Once final assembly identifies such a pair, feed the
 * exact source triangles back into their native surface charts.  Inserting an
 * interior surface sample shortens only those chords; genuine intersections
 * therefore remain and exhaust the bounded refinement budget, while
 * approximation-induced crossings converge without changing B-Rep topology. */
static size_t
refine_assembled_intersection(struct ON_Brep_CDT_State *s_cdt,
	const assembled_mesh_validation &validation, size_t max_points)
{
    if (!s_cdt || !max_points ||
	    !validation.intersecting_triangle_pairs)
	return 0;
    int source_faces[2] = {-1, -1};
    bool prefer_triangle_edge_split = false;
    ON_3dPoint intersection(validation.first_intersection_point[0],
	validation.first_intersection_point[1],
	validation.first_intersection_point[2]);
    for (int pair_index = 0; pair_index < 2; ++pair_index) {
	const int bot_triangle = validation.first_intersection[pair_index];
	if (bot_triangle >= 0 && (size_t)bot_triangle <
		s_cdt->bot_face_to_brep_face.size())
	    source_faces[pair_index] = s_cdt->bot_face_to_brep_face[
		(size_t)bot_triangle];
    }

    /* Adjacent surface approximations most often cross in a thin layer next
     * to their shared curved edge.  Splitting the nearest shared master-edge
     * segment is preferable to perturbing either face interior: both charts
     * receive the same authoritative 3-D point and watertightness is retained
     * by construction. */
    if (validation.first_intersection_point_valid && source_faces[0] >= 0 &&
	    source_faces[1] >= 0 && source_faces[0] != source_faces[1]) {
	const auto face_edges = [&](int face_index) {
	    std::set<int> result;
	    const ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	    for (int loop_index = 0; loop_index < face.LoopCount();
		    ++loop_index) {
		const ON_BrepLoop *loop = face.Loop(loop_index);
		if (!loop)
		    continue;
		for (int trim_index = 0; trim_index < loop->TrimCount();
			++trim_index) {
		    const ON_BrepTrim *trim = loop->Trim(trim_index);
		    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		    if (edge)
			result.insert(edge->m_edge_index);
		}
	    }
	    return result;
	};
	const std::set<int> first_edges = face_edges(source_faces[0]);
	const std::set<int> second_edges = face_edges(source_faces[1]);
	std::vector<int> shared_edges;
	std::set_intersection(first_edges.begin(), first_edges.end(),
	    second_edges.begin(), second_edges.end(),
	    std::back_inserter(shared_edges));
	const std::set<int> shared_edge_set(shared_edges.begin(),
	    shared_edges.end());
	bool pair_incident_to_shared_edge = !shared_edge_set.empty();
	double pair_triangle_scale = 0.0;
	for (int pair_index = 0; pair_index < 2; ++pair_index) {
	    const int bot_triangle = validation.first_intersection[pair_index];
	    const int face = source_faces[pair_index];
	    bool incident = false;
	    if (bot_triangle >= 0 && face >= 0 &&
		    (size_t)bot_triangle <
		    s_cdt->bot_face_to_cdt_triangle.size()) {
		const auto mesh_entry = s_cdt->fmeshes.find(face);
		const size_t local_triangle =
		    s_cdt->bot_face_to_cdt_triangle[(size_t)bot_triangle];
		if (mesh_entry != s_cdt->fmeshes.end() &&
			local_triangle < mesh_entry->second.tris_vect.size()) {
		    const triangle_t &triangle =
			mesh_entry->second.tris_vect[local_triangle];
		    for (int edge = 0; edge < 3; ++edge) {
			const uedge_t candidate(triangle.v[edge],
			    triangle.v[(edge + 1) % 3]);
			if (candidate.v[0] >= 0 && candidate.v[1] >= 0 &&
				(size_t)candidate.v[0] <
				mesh_entry->second.pnts.size() &&
				(size_t)candidate.v[1] <
				mesh_entry->second.pnts.size()) {
			    const ON_3dPoint *first_point =
				mesh_entry->second.pnts[(size_t)candidate.v[0]];
			    const ON_3dPoint *second_point =
				mesh_entry->second.pnts[(size_t)candidate.v[1]];
			    if (first_point && second_point)
				pair_triangle_scale = std::max(
				    pair_triangle_scale,
				    first_point->DistanceTo(*second_point));
			}
			const auto boundary =
			    mesh_entry->second.ue2b_map.find(candidate);
			if (boundary != mesh_entry->second.ue2b_map.end() &&
				boundary->second &&
				shared_edge_set.find(
				boundary->second->edge_ind) !=
				shared_edge_set.end()) {
			    incident = true;
			    break;
			}
		    }
		}
	    }
	    pair_incident_to_shared_edge =
		pair_incident_to_shared_edge && incident;
	}
	prefer_triangle_edge_split = !pair_incident_to_shared_edge;
	bedge_seg_t *nearest = NULL;
	double nearest_distance = DBL_MAX;
	for (int edge : shared_edges) {
	    const auto segments = s_cdt->e2polysegs.find(edge);
	    if (segments == s_cdt->e2polysegs.end())
		continue;
	    for (bedge_seg_t *segment : segments->second) {
		if (!segment || !segment->e_start || !segment->e_end)
		    continue;
		const ON_3dPoint closest = ON_Line(*segment->e_start,
		    *segment->e_end).ClosestPointTo(intersection);
		const double distance = closest.DistanceTo(intersection);
		if (distance < nearest_distance) {
		    nearest_distance = distance;
		    nearest = segment;
		}
	    }
	}
	/* The intersecting chords can be one element inward from their common
	 * boundary, so requiring direct incidence misses precisely the coarse
	 * approximation that this retry is meant to repair.  Still reject a
	 * remote shared edge: an intersection farther away than the local chord's
	 * sag scale is not plausibly caused by that boundary approximation. */
	const bool near_shared_edge = nearest &&
	    (pair_incident_to_shared_edge ||
	    nearest_distance <= MAX_SHARED_EDGE_REFINEMENT_DISTANCE_RATIO *
	    pair_triangle_scale + BN_TOL_DIST);
	if (near_shared_edge) {
	    const std::set<bedge_seg_t *> split = split_edge_seg(s_cdt,
		nearest, 1, NULL, 1);
	    if (!split.empty()) {
		std::set<int> changed_faces = {
		    source_faces[0], source_faces[1]
		};
		for (int face : changed_faces) {
		    cdt_mesh_t &mesh = s_cdt->fmeshes[face];
		    mesh.brep_edges.clear();
		    mesh.chart_boundary_edges.clear();
		    mesh.ue2b_map.clear();
		    if (!mesh.cdt())
			return 0;
		    bool mapped = loop_edges(&mesh, &mesh.outer_loop);
		    for (const auto &inner : mesh.inner_loops)
			mapped = loop_edges(&mesh, inner.second) && mapped;
		    if (!mapped || !refine_triangulation(s_cdt, &mesh, 0, 0) ||
			    mesh.geometric_degenerate_count())
			return 0;
		}
		return 1;
	    }
	}
    }

    std::map<int, std::vector<triangle_t>> targets;
    for (int pair_index = 0; pair_index < 2; ++pair_index) {
	const int bot_triangle = validation.first_intersection[pair_index];
	if (bot_triangle < 0 || (size_t)bot_triangle >=
		s_cdt->bot_face_to_brep_face.size() ||
		(size_t)bot_triangle >=
		s_cdt->bot_face_to_cdt_triangle.size())
	    continue;
	const int face = s_cdt->bot_face_to_brep_face[
	    (size_t)bot_triangle];
	const size_t local_triangle = s_cdt->bot_face_to_cdt_triangle[
	    (size_t)bot_triangle];
	auto mesh = s_cdt->fmeshes.find(face);
	if (mesh == s_cdt->fmeshes.end() ||
		local_triangle >= mesh->second.tris_vect.size())
	    continue;
	targets[face].push_back(mesh->second.tris_vect[local_triangle]);
    }

    size_t inserted = 0;
    std::vector<int> changed_faces;
    if (prefer_triangle_edge_split) {
	for (auto &entry : targets) {
	    cdt_mesh_t &mesh = s_cdt->fmeshes[entry.first];
	    inserted = mesh.split_problem_triangle_edges(entry.second, 1,
		validation.first_intersection_point_valid ? &intersection : NULL);
	    if (!inserted)
		continue;
	    if (mesh.valid(0) || (mesh.repair_incorrect_normal_edges() &&
		    mesh.valid(0)))
		return inserted;
	    return 0;
	}
    }
    for (auto &entry : targets) {
	if (inserted >= max_points)
	    break;
	cdt_mesh_t &mesh = s_cdt->fmeshes[entry.first];
	const size_t face_inserted = mesh.refine_problem_triangles(
	    entry.second, max_points - inserted);
	if (!face_inserted)
	    continue;
	inserted += face_inserted;
	changed_faces.push_back(entry.first);
    }
    if (!inserted)
	return 0;

    for (int face : changed_faces) {
	cdt_mesh_t &mesh = s_cdt->fmeshes[face];
	if (!mesh.cdt()) {
	    return 0;
	}
	if (!refine_triangulation(s_cdt, &mesh, 0, 0)) {
	    return 0;
	}
	if (mesh.geometric_degenerate_count()) {
	    return 0;
	}
    }
    return inserted;
}

typedef std::pair<int, int> assembled_edge_t;
typedef std::pair<assembled_edge_t, std::set<int, std::greater<int>>>
    assembled_shared_chord_t;

static std::vector<assembled_shared_chord_t>
assembled_shared_chords(int vertex_count, int face_count, const int *faces,
	const std::vector<int> &source_faces)
{
    struct chord_incidence {
	size_t count = 0;
	std::set<int, std::greater<int>> source_faces;
    };
    std::vector<assembled_shared_chord_t> shared;
    if (vertex_count <= 0 || face_count <= 0 || !faces ||
	    source_faces.size() != (size_t)face_count)
	return shared;
    std::map<assembled_edge_t, chord_incidence> incidence;
    for (int triangle = 0; triangle < face_count; ++triangle) {
	for (int corner = 0; corner < 3; ++corner) {
	    int first = faces[3 * triangle + corner];
	    int second = faces[3 * triangle + (corner + 1) % 3];
	    if (first < 0 || second < 0 || first >= vertex_count ||
		    second >= vertex_count || first == second)
		continue;
	    if (second < first)
		std::swap(first, second);
	    chord_incidence &entry = incidence[{first, second}];
	    entry.count++;
	    entry.source_faces.insert(source_faces[(size_t)triangle]);
	}
    }
    for (const auto &entry : incidence) {
	if (entry.second.count > 2 && entry.second.source_faces.size() > 1)
	    shared.push_back({entry.first, entry.second.source_faces});
    }
    return shared;
}

int
cdt_test_assembled_shared_chords(void)
{
    const int common_diagonal[] = {
	0, 1, 2, 0, 2, 3,
	2, 1, 0, 3, 2, 0
    };
    const std::vector<int> two_faces = {0, 0, 1, 1};
    const std::vector<assembled_shared_chord_t> shared =
	assembled_shared_chords(4, 4, common_diagonal, two_faces);
    if (shared.size() != 1 || shared[0].first != assembled_edge_t(0, 2) ||
	    shared[0].second !=
	    std::set<int, std::greater<int>>({1, 0}))
	return 1;

    const int distinct_diagonals[] = {
	0, 1, 2, 0, 2, 3,
	1, 0, 3, 3, 2, 1
    };
    if (!assembled_shared_chords(4, 4, distinct_diagonals,
	    two_faces).empty())
	return 2;
    const std::vector<int> one_face = {0, 0, 0, 0};
    return assembled_shared_chords(4, 4, common_diagonal,
	one_face).empty() ? 0 : 3;
}

/* Two thin faces can legitimately share all of their B-Rep boundary points.
 * If their independent CDTs choose the same non-boundary chord, however, that
 * chord has four incident triangles in the assembled mesh and its endpoint
 * links cease to be disks.  Refine one incident face at the chord midpoint.
 * The sample is evaluated on that face's source surface, leaves the
 * authoritative B-Rep boundary untouched, and removes only the accidental
 * cross-face chord identity. */
static size_t
refine_assembled_shared_chords(struct ON_Brep_CDT_State *s_cdt,
	int vertex_count, int face_count, const int *faces,
	size_t max_points)
{
    if (!s_cdt || vertex_count <= 0 || face_count <= 0 || !faces ||
	    !max_points ||
	    s_cdt->bot_pnt_to_on_pnt->size() != (size_t)vertex_count ||
	    s_cdt->bot_face_to_brep_face.size() != (size_t)face_count)
	return 0;
    const std::vector<assembled_shared_chord_t> shared =
	assembled_shared_chords(vertex_count, face_count, faces,
	    s_cdt->bot_face_to_brep_face);

    size_t inserted = 0;
    std::set<int> changed_faces;
    for (const assembled_shared_chord_t &edge_entry : shared) {
	if (inserted >= max_points)
	    break;
	ON_3dPoint *first_point = (*s_cdt->bot_pnt_to_on_pnt)[
	    (size_t)edge_entry.first.first];
	ON_3dPoint *second_point = (*s_cdt->bot_pnt_to_on_pnt)[
	    (size_t)edge_entry.first.second];
	if (!first_point || !second_point)
	    continue;
	const ON_3dPoint midpoint = 0.5 * (*first_point + *second_point);
	for (int face : edge_entry.second) {
	    const auto found_mesh = s_cdt->fmeshes.find(face);
	    if (found_mesh == s_cdt->fmeshes.end())
		continue;
	    cdt_mesh_t &mesh = found_mesh->second;
	    const auto first_local = mesh.p2ind.find(first_point);
	    const auto second_local = mesh.p2ind.find(second_point);
	    if (first_local == mesh.p2ind.end() ||
		    second_local == mesh.p2ind.end() ||
		    first_local->second == second_local->second)
		continue;
	    const uedge_t local_edge(first_local->second,
		second_local->second);
	    if (mesh.brep_edges.find(local_edge) != mesh.brep_edges.end() ||
		    mesh.chart_boundary_edges.find(local_edge) !=
		    mesh.chart_boundary_edges.end())
		continue;
	    const auto local_incidence = mesh.uedges2tris.find(local_edge);
	    if (local_incidence == mesh.uedges2tris.end() ||
		    local_incidence->second.size() != 2)
		continue;
	    const size_t local_triangle = *local_incidence->second.begin();
	    if (local_triangle >= mesh.tris_vect.size())
		continue;
	    const std::vector<triangle_t> target = {
		mesh.tris_vect[local_triangle]
	    };
	    if (!mesh.split_problem_triangle_edges(target, 1, &midpoint,
		    &local_edge))
		continue;
	    inserted++;
	    changed_faces.insert(face);
	    break;
	}
    }
    if (!inserted)
	return 0;

    for (int face : changed_faces) {
	cdt_mesh_t &mesh = s_cdt->fmeshes[face];
	if (mesh.valid(0) || (mesh.repair_incorrect_normal_edges() &&
		mesh.valid(0)))
	    continue;
	return 0;
    }
    const char *dump = getenv("BRLCAD_CDT_DUMP_FAILURES");
    if (dump && dump[0] && !BU_STR_EQUAL(dump, "0"))
	bu_log("Refined %zu of %zu accidental cross-face shared chords on "
	    "%zu faces\n", inserted, shared.size(), changed_faces.size());
    return inserted;
}

static bool
brep_cdt_relaxed_topology_safe(const ON_Brep *brep, std::string *reason)
{
    const auto fail = [reason](const std::string &message) {
	if (reason)
	    *reason = message;
	return false;
    };
    if (!brep || brep->m_F.Count() <= 0 || brep->m_V.Count() <= 0)
	return fail("missing faces or vertices");
    for (int edge_index = 0; edge_index < brep->m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = brep->m_E[edge_index];
	if (edge.m_edge_index != edge_index || edge.TrimCount() != 2 ||
		!edge.EdgeCurveOf())
	    return fail("edge " + std::to_string(edge_index) +
		" lacks paired trims, a curve, or stable indexing");
	if (!edge.Vertex(0) || !edge.Vertex(1) ||
		edge.Vertex(0)->m_vertex_index < 0 ||
		edge.Vertex(0)->m_vertex_index >= brep->m_V.Count() ||
		edge.Vertex(1)->m_vertex_index < 0 ||
		edge.Vertex(1)->m_vertex_index >= brep->m_V.Count())
	    return fail("edge " + std::to_string(edge_index) +
		" has invalid endpoint references");
	for (int edge_trim = 0; edge_trim < edge.TrimCount(); ++edge_trim) {
	    const ON_BrepTrim *trim = edge.Trim(edge_trim);
	    if (!trim || trim->m_trim_index < 0 ||
		    trim->m_trim_index >= brep->m_T.Count() ||
		    trim->m_ei != edge_index || !trim->TrimCurveOf())
		return fail("edge " + std::to_string(edge_index) +
		    " has an invalid trim reference");
	}
    }
    for (int face_index = 0; face_index < brep->m_F.Count(); ++face_index) {
	const ON_BrepFace &face = brep->m_F[face_index];
	if (face.m_face_index != face_index)
	    return fail("face " + std::to_string(face_index) +
		" lacks stable indexing");
	if (!face.SurfaceOf())
	    return fail("face " + std::to_string(face_index) +
		" lacks a surface");
	if (face.LoopCount() <= 0)
	    return fail("face " + std::to_string(face_index) +
		" has no trim loops");
	for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	    const ON_BrepLoop *loop = face.Loop(loop_index);
	    if (!loop || loop->m_loop_index < 0 ||
		    loop->m_loop_index >= brep->m_L.Count() ||
		    loop->Face() != &face || loop->TrimCount() <= 0)
		return fail("face " + std::to_string(face_index) +
		    " has an invalid loop reference");
	    for (int loop_trim = 0; loop_trim < loop->TrimCount(); ++loop_trim) {
		const ON_BrepTrim *trim = loop->Trim(loop_trim);
		if (!trim || trim->m_trim_index < 0 ||
			trim->m_trim_index >= brep->m_T.Count() ||
			trim->Loop() != loop || trim->Face() != &face ||
			!trim->Vertex(0) || !trim->Vertex(1) ||
			trim->Vertex(0)->m_vertex_index < 0 ||
			trim->Vertex(0)->m_vertex_index >= brep->m_V.Count() ||
			trim->Vertex(1)->m_vertex_index < 0 ||
			trim->Vertex(1)->m_vertex_index >= brep->m_V.Count() ||
			!trim->Domain().IsIncreasing())
		    return fail("face " + std::to_string(face_index) +
			" has an invalid trim or vertex reference");
		if (trim->m_type == ON_BrepTrim::singular) {
		    if (trim->m_ei >= 0)
			return fail("face " + std::to_string(face_index) +
			    " has a singular trim with an edge");
		} else if (trim->m_ei < 0 ||
			trim->m_ei >= brep->m_E.Count() ||
			!trim->TrimCurveOf()) {
		    return fail("face " + std::to_string(face_index) +
			" has an unpaired nonsingular trim");
		}
	    }
	}
    }
    return true;
}

static size_t
brep_cdt_promote_missing_outer_loops(ON_Brep *brep)
{
    if (!brep)
	return 0;
    size_t promoted = 0;
    for (int face_index = 0; face_index < brep->m_F.Count(); ++face_index) {
	ON_BrepFace &face = brep->m_F[face_index];
	if (face.OuterLoop() || face.LoopCount() <= 0)
	    continue;
	ON_BrepLoop *best_loop = NULL;
	double best_area = -1.0;
	for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	    ON_BrepLoop *loop = face.Loop(loop_index);
	    if (!loop)
		continue;
	    ON_BoundingBox bounds;
	    bool have_bounds = false;
	    for (int trim_index = 0; trim_index < loop->TrimCount();
		    ++trim_index) {
		const ON_BrepTrim *trim = loop->Trim(trim_index);
		if (!trim)
		    continue;
		ON_BoundingBox trim_bounds;
		if (!trim->GetBoundingBox(trim_bounds, false))
		    continue;
		if (!have_bounds) {
		    bounds = trim_bounds;
		    have_bounds = true;
		} else {
		    bounds.Union(trim_bounds);
		}
	    }
	    const double area = have_bounds ?
		fabs((bounds.m_max.x - bounds.m_min.x) *
		    (bounds.m_max.y - bounds.m_min.y)) : 0.0;
	    if (!best_loop || area > best_area) {
		best_loop = loop;
		best_area = area;
	    }
	}
	if (!best_loop)
	    continue;
	best_loop->m_type = ON_BrepLoop::outer;
	promoted++;
	bu_log("relaxed rigorous tessellation promoted loop %d on face %d "
	    "to outer\n", best_loop->m_loop_index, face_index);
    }
    return promoted;
}

static int
brep_cdt_tessellate(struct ON_Brep_CDT_State *s_cdt, int face_cnt,
	int *faces, bool try_invalid_brep)
{

    if (!s_cdt || !s_cdt->orig_brep)
	return -1;

    /* A call is a new transaction.  Never retain a previous mesh after a
     * failed retry or mix it with newly requested faces. */
    if (s_cdt->brep || s_cdt->w3dpnts->size())
	cdt_state_reset(s_cdt);
    s_cdt->status = BREP_CDT_FAILED;
    s_cdt->failed_face_indices.clear();
    s_cdt->failed_face_diagnostics.clear();
    s_cdt->repair_source_valid = false;
    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INITIALIZATION_FAILED,
	BREP_CDT_STAGE_INPUT, -1, 0, 0, "tessellation started");

    if (!cdt_tolerance_valid(s_cdt->tol)) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_TOLERANCE,
	    BREP_CDT_STAGE_INPUT, -1, 0, 0,
	    "invalid or contradictory tessellation tolerance");
	return -1;
    }

    if (face_cnt < 0 || (face_cnt > 0 && !faces)) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
	    BREP_CDT_STAGE_INPUT, -1, 0, 0,
	    "invalid face selection");
	return -1;
    }
    for (int i = 0; i < face_cnt; ++i) {
	if (faces[i] < 0 || faces[i] >= s_cdt->orig_brep->m_F.Count()) {
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
		BREP_CDT_STAGE_INPUT, faces[i], 0, 0,
		"selected face index is out of range");
	    return -1;
	}
    }

    ON_wString wstr;
    ON_TextLog tl(wstr);

    // Check for any conditions that are show-stoppers
    ON_wString wonstr;
    ON_TextLog vout(wonstr);
    if (!s_cdt->orig_brep->IsValid(&vout)) {
	if (!try_invalid_brep) {
	    bu_log("brep is NOT valid, cannot produce watertight mesh\n");
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
		BREP_CDT_STAGE_TOPOLOGY, -1, 0, 0,
		"input B-Rep failed OpenNURBS validity checks");
	    return -1;
	}
	std::string relaxed_failure;
	if (!brep_cdt_relaxed_topology_safe(s_cdt->orig_brep,
		&relaxed_failure)) {
	    const std::string message =
		"invalid B-Rep failed relaxed referential-safety checks: " +
		relaxed_failure;
	    bu_log("%s\n", message.c_str());
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
		BREP_CDT_STAGE_TOPOLOGY, -1, 0, 0,
		message.c_str());
	    return -1;
	}
    }
    bool oriented = false;
    bool boundary = true;
    if (!s_cdt->orig_brep->IsManifold(&oriented, &boundary) || boundary ||
	    !oriented || !s_cdt->orig_brep->IsSolid()) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
	    BREP_CDT_STAGE_TOPOLOGY, -1, 0, 0,
	    "input B-Rep is not a closed oriented manifold solid");
	return -1;
    }

    // For now, edges must have 2 and only 2 trims for this to work.
    for (int index = 0; index < s_cdt->orig_brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = s_cdt->orig_brep->m_E[index];
	if (edge.TrimCount() != 2) {
	    bu_log("Edge %d trim count: %d - can't (yet) do watertight meshing\n", edge.m_edge_index, edge.TrimCount());
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
		BREP_CDT_STAGE_TOPOLOGY, -1, 0, 0,
		"B-Rep edge does not have exactly two trims");
	    return -1;
	}
    }

    // We may be changing the ON_Brep data, so work on a copy
    // rather than the original object
    if (!s_cdt->brep) {

	s_cdt->brep = new ON_Brep(*s_cdt->orig_brep);
	if (try_invalid_brep)
	    brep_cdt_promote_missing_outer_loops(s_cdt->brep);

	// Attempt to minimize situations where 2D and 3D distances get out of sync
	// by shrinking the surfaces down to the active area of the face
	s_cdt->brep->ShrinkSurfaces();

    }

    ON_Brep* brep = s_cdt->brep;

    // If this is the first time through, there are a number of once-per-conversion
    // operations to take care of.
    if (!s_cdt->w3dpnts->size()) {

	// Translate global relative tolerances into physical dimensions based
	// on the BRep bounding box
	cdt_tol_global_calc(s_cdt);

	/* We want to use ON_3dPoint pointers and BrepVertex points, but
	 * vert->Point() produces a temporary address.  If this is our first time
	 * through, make stable copies of the Vertex points. */
	for (int index = 0; index < brep->m_V.Count(); index++) {
	    ON_BrepVertex& v = brep->m_V[index];
	    (*s_cdt->vert_pnts)[index] = new ON_3dPoint(v.Point());
	    CDT_Add3DPnt(s_cdt, (*s_cdt->vert_pnts)[index], -1, v.m_vertex_index, -1, -1, -1, -1);
	    // topologically, any vertex point will be on edges
	    s_cdt->edge_pnts->insert((*s_cdt->vert_pnts)[index]);
	}

	/* If this is the first time through, check for singular trims.  For
	 * vertices associated with such a trim get vertex normals that are the
	 * average of the surface normals at the junction from faces that don't
	 * use a singular trim to reference the vertex.
	 */
	for (int index = 0; index < brep->m_T.Count(); index++) {
	    ON_BrepTrim &trim = s_cdt->brep->m_T[index];
	    if (trim.m_type == ON_BrepTrim::singular) {
		ON_BrepVertex *v1 = trim.Vertex(0);
		ON_BrepVertex *v2 = trim.Vertex(1);
		calc_singular_vert_norm(s_cdt, v1->m_vertex_index);
		calc_singular_vert_norm(s_cdt, v2->m_vertex_index);
	    }
	}

	// Set up the edge containers that will manage the edge subdivision.
	initialize_edge_containers(s_cdt);

	// Next, for each face and each loop in each face define the initial
	// loop polygons.  Note there is no splitting of edges at this point -
	// we are simply establishing the initial closed polygons.
	if (!initialize_loop_polygons(s_cdt)) {
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INITIALIZATION_FAILED,
		BREP_CDT_STAGE_EDGE_INITIALIZATION, -1, 0, 0,
		"failed to initialize trim loop polygons");
	    return -1;
	}

	// Initialize the tangents.
	std::map<int, std::set<bedge_seg_t *>>::iterator epoly_it;
	for (epoly_it = s_cdt->e2polysegs.begin(); epoly_it != s_cdt->e2polysegs.end(); epoly_it++) {
	    std::set<bedge_seg_t *>::iterator seg_it;
	    for (seg_it = epoly_it->second.begin(); seg_it != epoly_it->second.end(); seg_it++) {
		bedge_seg_t *bseg = *seg_it;
		double ts1 = bseg->tseg1->trim_start;
		double ts2 = bseg->tseg2->trim_start;
		bseg->tan_start = bseg_tangent(s_cdt, bseg, bseg->edge_start, ts1, ts2);

		double te1 = bseg->tseg1->trim_end;
		double te2 = bseg->tseg2->trim_end;
		bseg->tan_end = bseg_tangent(s_cdt, bseg, bseg->edge_end, te1, te2);
	}
    }

    char pole_split_message[256] = {0};
    if (!split_edges_at_surface_poles(s_cdt, pole_split_message,
	    sizeof(pole_split_message))) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INITIALIZATION_FAILED,
	    BREP_CDT_STAGE_EDGE_INITIALIZATION, -1, 0, 0,
	    pole_split_message[0] ? pole_split_message :
	    "failed to split shared edges at surface poles");
	return -1;
    }

    // Do the non-tolerance based initialization splits.
    char edge_init_message[256] = {0};
    if (!initialize_edge_segs(s_cdt, edge_init_message,
	    sizeof(edge_init_message))) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INITIALIZATION_FAILED,
	    BREP_CDT_STAGE_EDGE_INITIALIZATION, -1, 0, 0,
	    edge_init_message[0] ? edge_init_message :
	    "failed to initialize shared edge segments");
	return -1;
    }

	// If edge segments are too close together in 2D space compared to their
	// length, it is difficult to mesh them successfully.  Refine edges that
	// are close to other edges.
	refine_close_edges(s_cdt);

#if 1
	// On to tolerance based splitting.  Process the non-linear edges first -
	// we will need information from them to handle the linear edges
	tol_curved_edges_split(s_cdt);

	// After the initial curve split, make another pass looking for curved
	// edges sharing a vertex.  We want larger curves to refine close to the
	// median segment length of the smaller ones, since this situation can be a
	// sign that the surface will generate small triangles near large ones.
	curved_edges_refine(s_cdt);

	// Now, process the linear edges
	char message[256] = {0};
	if (!tol_linear_edges_split(s_cdt, message, sizeof(message))) {
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REFINEMENT_LIMIT,
		BREP_CDT_STAGE_EDGE_INITIALIZATION, -1, 0, 0,
		message);
	    return -1;
	}
#endif

	// Split singularity trims in 2D to provide an easier input to the 2D CDT logic.  NOTE: these
	// splits will produce degenerate (zero area, two identical vertex) triangles in 3D that have
	// to be cleaned up.
	while (s_cdt->unsplit_singular_edges.size()) {
	    std::queue<cpolyedge_t *> w1, w2;
	    std::queue<cpolyedge_t *> *wq, *nq, *tmpq;
	    int cnt = 0;
	    wq = &w1;
	    nq = &w2;
	    std::set<cpolyedge_t *>::iterator first =
		std::min_element(s_cdt->unsplit_singular_edges.begin(),
			s_cdt->unsplit_singular_edges.end(),
			singular_edge_process_less);
	    nq->push(*first);
	    s_cdt->unsplit_singular_edges.erase(first);
	    while (cnt < 6) {
		cnt = 0;
		tmpq = wq;
		wq = nq;
		nq = tmpq;
		while (!wq->empty()) {
		    cpolyedge_t *ce = wq->front();
		    wq->pop();
		    std::set<cpolyedge_t *> nedges = split_singular_seg(s_cdt, ce, 0);
		    std::vector<cpolyedge_t *> ordered_edges(nedges.begin(),
			    nedges.end());
		    std::sort(ordered_edges.begin(), ordered_edges.end(),
			    singular_edge_process_less);
		    for (std::vector<cpolyedge_t *>::const_iterator n_it =
			    ordered_edges.begin(); n_it != ordered_edges.end(); ++n_it) {
			nq->push(*n_it);
			cnt++;
		    }
		}
	    }
	}

	/* A valid solid may contain a distinct topology edge whose complete
	 * curve is smaller than the fixed modeling tolerance.  Weld only those
	 * explicitly proven edges in derived mesh state, before any face chart
	 * is built. */
    collapse_subtolerance_brep_edges(s_cdt);

    /* Some valid imports use distinct B-Rep vertex identities for repeated
     * visits to one geometric surface pole.  Weld only face-local,
     * singularity-proven coincidences before constructing pole charts. */
    collapse_coincident_surface_poles(s_cdt);

    /* When a safely welded pole makes two distinct analytic seam edges share
     * both endpoints, prove their master curves coincide within modeling
     * tolerance and give them a common refinement sequence. */
    std::map<ON_3dPoint *, ON_3dPoint *> coincident_edge_welds;
    if (synchronize_coincident_edge_samples(s_cdt,
	    coincident_edge_welds)) {
	for (const auto &weld : coincident_edge_welds)
	    s_cdt->collapsed_edge_pnts[weld.first] = weld.second;
	apply_derived_point_welds(s_cdt, coincident_edge_welds);
    }

    // Rebuild finalized 2D RTrees for faces (needed for surface processing)
	finalize_rtrees(s_cdt);
	s_cdt->tolerance_changed = false;
    } else {
	/* Clear the mesh state, if this container was previously used */
    }

    // Process all of the faces we have been instructed to process, or (default) all faces.
    // Keep track of failures and successes.
    int face_failures = 0;
    int face_successes = 0;
    int first_failed_face = -1;
    int fc = ((face_cnt == 0) || !faces) ? s_cdt->brep->m_F.Count() : face_cnt;
    for (int i = 0; i < fc; i++) {
	int fi = ((face_cnt == 0) || !faces) ? i : faces[i];
	const auto inconsistent = s_cdt->inconsistent_edge_faces.find(fi);
	bool face_triangulated = false;
	if (inconsistent != s_cdt->inconsistent_edge_faces.end()) {
	    const std::string message = "face pullback for closed B-Rep edge " +
		std::to_string(inconsistent->second.first) +
		" misses the authoritative shared edge by " +
		std::to_string(inconsistent->second.second);
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_GEOMETRIC_FAILED,
		BREP_CDT_STAGE_GEOMETRIC_VALIDATION, fi, 0, 1,
		message.c_str());
	} else {
	    face_triangulated = do_triangulation(s_cdt, fi);
	}
	if (face_triangulated) {
	    face_successes++;
	} else {
	    if (s_cdt->diagnostic.face_index != fi ||
		    s_cdt->diagnostic.result >= 0) {
		cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_FACE_FAILED,
		    BREP_CDT_STAGE_FACE_TRIANGULATION, fi, 0, 1,
		    "face triangulation failed without a specific diagnostic");
	    }
	    s_cdt->failed_face_indices.push_back(fi);
	    s_cdt->failed_face_diagnostics[fi] = s_cdt->diagnostic;
	    if (first_failed_face < 0)
		first_failed_face = fi;
	    face_failures++;
	}
    }

    // If we only tessellated some of the faces, we don't have the
    // full solid mesh yet (by definition).  Return accordingly.
    if (face_failures || !face_successes || face_successes < s_cdt->brep->m_F.Count()) {
	const bool specific_failure =
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_CHART_FAILED ||
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_INVALID_PSLG ||
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_DETRIA_FAILED ||
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_CERTIFICATION_FAILED ||
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_REFINEMENT_LIMIT ||
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_GEOMETRIC_FAILED;
	if (face_successes) {
	    s_cdt->status = face_successes;
	    if (specific_failure) {
		s_cdt->diagnostic.completed_faces = face_successes;
		s_cdt->diagnostic.failed_faces = face_failures;
	    } else {
		cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_PARTIAL,
		    BREP_CDT_STAGE_FACE_TRIANGULATION, first_failed_face,
		    face_successes, face_failures,
		    "only a subset of B-Rep faces was triangulated");
	    }
	    return face_successes;
	}
	s_cdt->status = BREP_CDT_FAILED;
	if (specific_failure) {
	    s_cdt->diagnostic.completed_faces = 0;
	    s_cdt->diagnostic.failed_faces = face_failures;
	} else {
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_FACE_FAILED,
		BREP_CDT_STAGE_FACE_TRIANGULATION, first_failed_face,
		0, face_failures, "no B-Rep faces were triangulated");
	}
	return -1;
    }

    /* We've got face meshes and no reported failures - check to see if we have a
     * solid mesh */
    int valid_fcnt, valid_vcnt;
    int *valid_faces = NULL;
    fastf_t *valid_vertices = NULL;
    assembled_mesh_validation mesh_validation;
    size_t assembled_refinement_points = 0;
    int assembled_refinement_attempts = 0;
    while (true) {
	if (ON_Brep_CDT_Mesh(&valid_faces, &valid_fcnt, &valid_vertices,
		&valid_vcnt, NULL, NULL, NULL, NULL, s_cdt, 0, NULL) < 0) {
	    s_cdt->status = BREP_CDT_FAILED;
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_MESH_EXPORT_FAILED,
		BREP_CDT_STAGE_MESH_ASSEMBLY, -1, face_successes, 0,
		"failed to assemble indexed triangle mesh");
	    return -1;
	}
	if (assembled_mesh_validate(valid_vcnt, valid_fcnt, valid_vertices,
		valid_faces, &mesh_validation))
	    break;

	const bool only_intersection =
	    mesh_validation.intersecting_triangle_pairs &&
	    !mesh_validation.invalid_indices &&
	    !mesh_validation.nonfinite_vertices &&
	    !mesh_validation.unused_vertices &&
	    !mesh_validation.degenerate_faces &&
	    !mesh_validation.invalid_vertex_links;
	size_t inserted = 0;
	if (mesh_validation.invalid_vertex_links &&
		assembled_refinement_attempts <
		MAX_ASSEMBLED_REFINEMENT_ATTEMPTS &&
		assembled_refinement_points <
		MAX_ASSEMBLED_REFINEMENT_POINTS) {
	    inserted = refine_assembled_shared_chords(s_cdt, valid_vcnt,
		valid_fcnt, valid_faces, MAX_ASSEMBLED_REFINEMENT_POINTS -
		assembled_refinement_points);
	}
	if (only_intersection && assembled_refinement_attempts <
		MAX_ASSEMBLED_REFINEMENT_ATTEMPTS &&
		assembled_refinement_points <
		MAX_ASSEMBLED_REFINEMENT_POINTS) {
	    inserted = refine_assembled_intersection(s_cdt, mesh_validation,
		MAX_ASSEMBLED_REFINEMENT_POINTS -
		assembled_refinement_points);
	}
	if (inserted) {
	    assembled_refinement_points += inserted;
	    assembled_refinement_attempts++;
	    bu_free(valid_faces, "faces");
	    bu_free(valid_vertices, "vertices");
	    valid_faces = NULL;
	    valid_vertices = NULL;
	    continue;
	}

	std::string message =
	    "assembled mesh failed geometric/link validation: indices " +
	    std::to_string(mesh_validation.invalid_indices) +
	    ", nonfinite vertices " +
	    std::to_string(mesh_validation.nonfinite_vertices) +
	    ", unused vertices " +
	    std::to_string(mesh_validation.unused_vertices) +
	    ", degenerate faces " +
	    std::to_string(mesh_validation.degenerate_faces) +
	    ", invalid vertex links " +
	    std::to_string(mesh_validation.invalid_vertex_links) +
	    ", intersecting triangle pairs " +
	    std::to_string(mesh_validation.intersecting_triangle_pairs) +
	    (mesh_validation.intersecting_triangle_pairs ?
	    " (first " + std::to_string(
		mesh_validation.first_intersection[0]) + ", " +
		std::to_string(mesh_validation.first_intersection[1]) + ")" :
	    "");
	if (mesh_validation.intersecting_triangle_pairs &&
		mesh_validation.first_intersection[0] >= 0 &&
		mesh_validation.first_intersection[1] >= 0 &&
		(size_t)mesh_validation.first_intersection[0] <
		s_cdt->bot_face_to_brep_face.size() &&
		(size_t)mesh_validation.first_intersection[1] <
		s_cdt->bot_face_to_brep_face.size()) {
	    message += " from B-Rep faces " + std::to_string(
		s_cdt->bot_face_to_brep_face[(size_t)
		mesh_validation.first_intersection[0]]) + ", " +
		std::to_string(s_cdt->bot_face_to_brep_face[(size_t)
		mesh_validation.first_intersection[1]]);
	}
	if (assembled_refinement_points)
	    message += " after " + std::to_string(
		assembled_refinement_attempts) + " refinement rounds and " +
		std::to_string(assembled_refinement_points) +
		" inserted points";
	bu_free(valid_faces, "faces");
	bu_free(valid_vertices, "vertices");
	s_cdt->status = BREP_CDT_FAILED;
	cdt_diagnostic_set(s_cdt,
	    mesh_validation.intersecting_triangle_pairs ?
	    BREP_CDT_RESULT_GEOMETRIC_FAILED :
	    BREP_CDT_RESULT_CERTIFICATION_FAILED,
	    mesh_validation.intersecting_triangle_pairs ?
	    BREP_CDT_STAGE_GEOMETRIC_VALIDATION :
	    BREP_CDT_STAGE_SOLID_VALIDATION,
	    -1, face_successes, 0, message.c_str());
	return -1;
    }
    if (assembled_refinement_points)
	bu_log("%s: assembled mesh certified after %d cross-face refinement "
	    "rounds and %zu inserted points\n",
	    s_cdt->name ? s_cdt->name : "BREP",
	    assembled_refinement_attempts, assembled_refinement_points);

    struct bg_trimesh_solid_errors se = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    int invalid = bg_trimesh_solid2(valid_vcnt, valid_fcnt, valid_vertices, valid_faces, &se);

    if (invalid) {
	trimesh_error_report(s_cdt, valid_fcnt, valid_vcnt, valid_faces, valid_vertices, &se);
    }

    bg_free_trimesh_solid_errors(&se);

    if (invalid) {
	bu_free(valid_faces, "faces");
	bu_free(valid_vertices, "vertices");
	s_cdt->status = BREP_CDT_NON_SOLID;
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_NON_SOLID,
	    BREP_CDT_STAGE_SOLID_VALIDATION, -1, face_successes, 0,
	    "assembled mesh failed closed-solid validation");
	return 1;
    }

    s_cdt->certified_faces = valid_faces;
    s_cdt->certified_face_count = valid_fcnt;
    s_cdt->certified_vertices = valid_vertices;
    s_cdt->certified_vertex_count = valid_vcnt;
    s_cdt->certified_repaired = false;
    s_cdt->status = BREP_CDT_SOLID;
    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_SUCCESS,
	BREP_CDT_STAGE_SOLID_VALIDATION, -1, face_successes, 0,
	"closed indexed mesh passed incidence, link, and area validation");
    return 0;

}

int
ON_Brep_CDT_Tessellate(struct ON_Brep_CDT_State *s_cdt, int face_cnt,
	int *faces)
{
    if (s_cdt) {
	s_cdt->allow_bounded_edge_approximation = false;
	s_cdt->bounded_edge_approximation_tolerance = 0.0;
    }
    return brep_cdt_tessellate(s_cdt, face_cnt, faces, false);
}

typedef std::array<double, 3> repair_point_key;
typedef std::array<repair_point_key, 3> repair_triangle_key;
typedef std::array<repair_point_key, 2> repair_triangle_edge_key;

static repair_triangle_key
repair_triangle_coordinates(const fastf_t *vertices, const int *face)
{
    repair_triangle_key key;
    for (int corner = 0; corner < 3; ++corner) {
	const int vertex = face[corner];
	key[(size_t)corner] = {
	    vertices[(size_t)vertex * 3],
	    vertices[(size_t)vertex * 3 + 1],
	    vertices[(size_t)vertex * 3 + 2]
	};
    }
    std::sort(key.begin(), key.end());
    return key;
}

static repair_triangle_key
repair_triangle_coordinates(const cdt_mesh_t &mesh,
	const triangle_t &triangle)
{
    repair_triangle_key key;
    for (int corner = 0; corner < 3; ++corner) {
	const ON_3dPoint &point = *mesh.pnts[(size_t)triangle.v[corner]];
	key[(size_t)corner] = {point.x, point.y, point.z};
    }
    std::sort(key.begin(), key.end());
    return key;
}

static repair_triangle_edge_key
repair_triangle_edge(const repair_point_key &first,
	const repair_point_key &second)
{
    repair_triangle_edge_key edge = {first, second};
    if (edge[1] < edge[0])
	std::swap(edge[0], edge[1]);
    return edge;
}

static double
repair_triangle_key_area(const repair_triangle_key &triangle)
{
    return 0.5 * ON_CrossProduct(
	ON_3dPoint(triangle[1].data()) - ON_3dPoint(triangle[0].data()),
	ON_3dPoint(triangle[2].data()) - ON_3dPoint(triangle[0].data()))
	.Length();
}

static bool
repair_triangle_third_point(const repair_triangle_key &triangle,
	const repair_triangle_edge_key &edge, repair_point_key *third)
{
    bool have_first = false;
    bool have_second = false;
    int third_index = -1;
    for (int corner = 0; corner < 3; ++corner) {
	if (!have_first && triangle[(size_t)corner] == edge[0]) {
	    have_first = true;
	    continue;
	}
	if (!have_second && triangle[(size_t)corner] == edge[1]) {
	    have_second = true;
	    continue;
	}
	third_index = corner;
    }
    if (!have_first || !have_second || third_index < 0)
	return false;
    if (third)
	*third = triangle[(size_t)third_index];
    return true;
}

/* Mesh repair resolves a hanging boundary sample by replacing each incident
 * triangle with two triangles that use the same new edge point.  That is a
 * topology refinement, not permission to discard certified surface geometry.
 * Recognize it only when all original corners remain and the new point is
 * strictly inside one original edge within a scale-limited chord tolerance. */
static bool
repair_triangle_edge_split(const repair_triangle_key &original,
	const repair_triangle_key &first, const repair_triangle_key &second)
{
    for (int split_first = 0; split_first < 3; ++split_first) {
	const int split_second = (split_first + 1) % 3;
	const int opposite = (split_first + 2) % 3;
	const repair_triangle_edge_key first_side = repair_triangle_edge(
	    original[(size_t)split_first], original[(size_t)opposite]);
	const repair_triangle_edge_key second_side = repair_triangle_edge(
	    original[(size_t)split_second], original[(size_t)opposite]);
	repair_point_key first_new;
	repair_point_key second_new;
	if (!repair_triangle_third_point(first, first_side, &first_new) ||
		!repair_triangle_third_point(second, second_side,
		&second_new) || first_new != second_new) {
	    if (!repair_triangle_third_point(second, first_side, &first_new) ||
		    !repair_triangle_third_point(first, second_side,
		    &second_new) || first_new != second_new)
		continue;
	}
	const ON_3dPoint a(original[(size_t)split_first].data());
	const ON_3dPoint b(original[(size_t)split_second].data());
	const ON_3dPoint p(first_new.data());
	const ON_3dVector edge = b - a;
	const double edge_squared = edge.LengthSquared();
	if (!(edge_squared > 0.0) || !std::isfinite(edge_squared))
	    continue;
	const double parameter = ((p - a) * edge) / edge_squared;
	if (!(parameter > 1e-12 && parameter < 1.0 - 1e-12))
	    continue;
	const ON_3dPoint closest = a + parameter * edge;
	const double edge_length = std::sqrt(edge_squared);
	double coordinate_scale = 1.0;
	for (int corner = 0; corner < 3; ++corner) {
	    for (int axis = 0; axis < 3; ++axis)
		coordinate_scale = std::max(coordinate_scale,
		    std::fabs(original[(size_t)corner][(size_t)axis]));
	}
	const double roundoff = 4096.0 *
	    std::numeric_limits<double>::epsilon() * coordinate_scale;
	const double chord_tolerance = std::min((double)BN_TOL_DIST,
	    std::max(roundoff, 2e-6 * edge_length));
	if (p.DistanceTo(closest) > chord_tolerance)
	    continue;
	const double original_area = repair_triangle_key_area(original);
	const double split_area = repair_triangle_key_area(first) +
	    repair_triangle_key_area(second);
	const double area_tolerance = std::max(
	    1e-12 * std::max(1.0, original_area),
	    chord_tolerance * std::max(
		a.DistanceTo(ON_3dPoint(original[(size_t)opposite].data())),
		b.DistanceTo(ON_3dPoint(original[(size_t)opposite].data()))));
	if (std::fabs(split_area - original_area) <= area_tolerance)
	    return true;
    }
    return false;
}

int
cdt_test_repair_triangle_split(void)
{
    const repair_triangle_key first_original = {{
	{{173.37886982212009, 26.030906428894703, 247.41309997758779}},
	{{173.38489474529828, 26.030906428894696, 247.41569829294829}},
	{{173.45939382361800, 26.118986752580415, 247.43600234096598}}
    }};
    const repair_point_key split = {{
	173.38314307677948, 26.030906428894706, 247.41494286574473}};
    repair_triangle_key first_half = {{
	first_original[0], split, first_original[2]}};
    repair_triangle_key second_half = {{
	split, first_original[1], first_original[2]}};
    std::sort(first_half.begin(), first_half.end());
    std::sort(second_half.begin(), second_half.end());
    if (!repair_triangle_edge_split(first_original, first_half, second_half))
	return 1;
    const repair_triangle_key second_original = {{
	{{173.38100471667130, 26.030906428894703, 247.41440728443774}},
	{{173.38475097989038, 26.030906428894703, 247.41534557967435}},
	{{173.43320650492041, 25.986308181821187, 247.42833899473351}}
    }};
    repair_triangle_key third_half = {{
	second_original[0], split, second_original[2]}};
    repair_triangle_key fourth_half = {{
	split, second_original[1], second_original[2]}};
    std::sort(third_half.begin(), third_half.end());
    std::sort(fourth_half.begin(), fourth_half.end());
    if (!repair_triangle_edge_split(second_original, third_half,
	    fourth_half))
	return 2;
    repair_triangle_key displaced = second_half;
    for (repair_point_key &point : displaced) {
	if (point == split) {
	    point[1] += 1e-3;
	    break;
	}
    }
    std::sort(displaced.begin(), displaced.end());
    if (repair_triangle_edge_split(first_original, first_half, displaced))
	return 3;
    repair_triangle_key missing_corner = second_half;
    missing_corner[0] = {{0.0, 0.0, 0.0}};
    std::sort(missing_corner.begin(), missing_corner.end());
    return repair_triangle_edge_split(first_original, first_half,
	missing_corner) ? 4 : 0;
}

static double
repair_bbox_distance(const ON_BoundingBox &bbox, const ON_3dPoint &point)
{
    double squared = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
	double delta = 0.0;
	if (point[axis] < bbox.m_min[axis])
	    delta = bbox.m_min[axis] - point[axis];
	else if (point[axis] > bbox.m_max[axis])
	    delta = point[axis] - bbox.m_max[axis];
	squared += delta * delta;
    }
    return std::sqrt(squared);
}

static bool
repair_surface_distance(const ON_Brep *brep,
	const std::vector<ON_BoundingBox> &face_bounds,
	std::vector<std::unique_ptr<brlcad::SurfaceTree>> &face_trees,
	std::vector<std::unique_ptr<brlcad::PullbackContext>> &face_contexts,
	const ON_3dPoint &point, double allowed, bool allow_untrimmed,
	double *distance, bool *used_untrimmed,
	ON_3dPoint *closest_point = NULL, int *closest_face = NULL,
	const std::set<int> *face_filter = NULL)
{
    if (!brep || !distance || !used_untrimmed || !(allowed > 0.0) ||
	    !std::isfinite(allowed))
	return false;

    double closest_distance = std::numeric_limits<double>::infinity();
    bool closest_was_untrimmed = false;
    ON_3dPoint closest_projection = ON_3dPoint::UnsetPoint;
    int closest_face_index = -1;
    for (int face_index = 0; face_index < brep->m_F.Count(); ++face_index) {
	if (face_filter && !face_filter->empty() &&
		face_filter->find(face_index) == face_filter->end())
	    continue;
	const ON_BrepFace &face = brep->m_F[face_index];
	if (!face.SurfaceOf() || !face_bounds[(size_t)face_index].IsValid() ||
		repair_bbox_distance(face_bounds[(size_t)face_index], point) >
		allowed)
	    continue;
	/* When an underlying-surface match is explicitly acceptable, a trimmed
	 * SurfaceTree supplies no additional acceptance information.  Some
	 * imported faces have extremely costly or pathological trim trees, so
	 * project directly to the surface and conservatively report the match as
	 * untrimmed.  The caller's distance, area, coverage, and solid gates remain
	 * unchanged. */
	if (allow_untrimmed) {
	    if (!face_contexts[(size_t)face_index])
		face_contexts[(size_t)face_index] =
		    std::unique_ptr<brlcad::PullbackContext>(
			new brlcad::PullbackContext());
	    ON_2dPoint uv;
	    ON_3dPoint projected = ON_3dPoint::UnsetPoint;
	    double candidate_distance =
		std::numeric_limits<double>::infinity();
	    const bool projected_point = face_contexts[(size_t)face_index]->
		SurfaceClosestPoint(face.SurfaceOf(), point, uv, projected,
		    candidate_distance, 0, BREP_SAME_POINT_TOLERANCE, allowed);
	    if (projected_point && std::isfinite(candidate_distance) &&
		    candidate_distance <= allowed &&
		    candidate_distance < closest_distance) {
		closest_distance = candidate_distance;
		closest_was_untrimmed = true;
		closest_projection = projected;
		closest_face_index = face_index;
	    }
	    continue;
	}
	if (!face_trees[(size_t)face_index]) {
	    face_trees[(size_t)face_index] =
		std::unique_ptr<brlcad::SurfaceTree>(
		    new brlcad::SurfaceTree(&face, true));
	}
	brlcad::SurfaceTree *tree = face_trees[(size_t)face_index].get();
	ON_2dPoint uv;
	ON_3dPoint projected = ON_3dPoint::UnsetPoint;
	double candidate_distance =
	    std::numeric_limits<double>::infinity();
	bool projected_point = tree->Valid() &&
	    brlcad::get_closest_point(uv, face, point, tree);
	if (projected_point) {
	    projected = face.SurfaceOf()->PointAt(uv.x, uv.y);
	    candidate_distance = projected.DistanceTo(point);
	}
	const auto outside_face_trim = [&](const ON_2dPoint &test_uv) {
	    const brlcad::BRNode *closest_trim = NULL;
	    double trim_distance = -1.0;
	    return !tree->Valid() || tree->getRootNode()->isTrimmed(test_uv,
		&closest_trim, trim_distance, BREP_EDGE_MISS_TOLERANCE);
	};
	bool outside_trim = !projected_point || outside_face_trim(uv);
	/* The face tree is optimized for a well-formed trimmed region.  A weak
	 * or self-touching trim can return a distant local surface point or mark
	 * the useful projection outside without the search itself failing.  In
	 * those cases also ask the underlying-surface solver and retain the
	 * closer result; the ordinary trimmed/untrimmed policy is applied to the
	 * winning UV below. */
	if (!projected_point || candidate_distance > allowed || outside_trim) {
	    if (!face_contexts[(size_t)face_index])
		face_contexts[(size_t)face_index] =
		    std::unique_ptr<brlcad::PullbackContext>(
			new brlcad::PullbackContext());
	    ON_2dPoint fallback_uv;
	    ON_3dPoint fallback_projection = ON_3dPoint::UnsetPoint;
	    double fallback_distance =
		std::numeric_limits<double>::infinity();
	    const bool fallback_point = face_contexts[(size_t)face_index]->
		SurfaceClosestPoint(face.SurfaceOf(), point, fallback_uv,
		    fallback_projection, fallback_distance, 0,
		    BREP_SAME_POINT_TOLERANCE, allowed);
	    if (fallback_point && std::isfinite(fallback_distance) &&
		    (!projected_point || fallback_distance < candidate_distance)) {
		projected_point = true;
		uv = fallback_uv;
		projected = fallback_projection;
		candidate_distance = fallback_distance;
		outside_trim = outside_face_trim(uv);
	    }
	}
	if (!projected_point)
	    continue;
	if (!std::isfinite(candidate_distance) ||
		candidate_distance > allowed ||
		candidate_distance >= closest_distance)
	    continue;
	if (outside_trim && !allow_untrimmed)
	    continue;
	closest_distance = candidate_distance;
	closest_was_untrimmed = outside_trim;
	closest_projection = projected;
	closest_face_index = face_index;
    }

    if (!std::isfinite(closest_distance))
	return false;
    *distance = closest_distance;
    *used_untrimmed = closest_was_untrimmed;
    if (closest_point)
	*closest_point = closest_projection;
    if (closest_face)
	*closest_face = closest_face_index;
    return true;
}

static bool
repair_input_mesh_distance(RTree<size_t, double, 3> &triangle_index,
	const fastf_t *vertices, const int *faces, const ON_3dPoint &point,
	double allowed, double *distance, size_t *closest_face = NULL)
{
    if (!vertices || !faces || !distance || !(allowed > 0.0))
	return false;
    double minimum[3] = {
	point.x - allowed, point.y - allowed, point.z - allowed
    };
    double maximum[3] = {
	point.x + allowed, point.y + allowed, point.z + allowed
    };
    std::vector<size_t> candidates;
    triangle_index.Search(minimum, maximum,
	assembled_mesh_collect_candidate, &candidates);
    double closest = std::numeric_limits<double>::infinity();
    size_t closest_triangle = std::numeric_limits<size_t>::max();
    point_t test_point;
    VSET(test_point, point.x, point.y, point.z);
    for (size_t candidate : candidates) {
	point_t triangle[3];
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = faces[candidate * 3 + (size_t)corner];
	    VSET(triangle[corner], vertices[(size_t)vertex * 3],
		vertices[(size_t)vertex * 3 + 1],
		vertices[(size_t)vertex * 3 + 2]);
	}
	const double candidate_distance = bg_tri_closest_pt(NULL, test_point,
	    triangle[0], triangle[1], triangle[2]);
	if (candidate_distance < closest) {
	    closest = candidate_distance;
	    closest_triangle = candidate;
	}
    }
    if (!std::isfinite(closest) || closest > allowed)
	return false;
    *distance = closest;
    if (closest_face)
	*closest_face = closest_triangle;
    return true;
}

struct repair_changed_face {
    int index;
    double area;
    int source_brep_face = -1;
    bool local_surface_approximation = false;
    bool direct_surface_samples = false;
    double direct_surface_deviation = 0.0;
};

struct repair_fast_face_range {
    size_t first_face = 0;
    size_t face_count = 0;
    size_t first_point = 0;
    size_t point_count = 0;
    bool present = false;
};

struct repair_fast_trim_sample {
    fastf_t parameter;
    fastf_t uv[2];
    fastf_t point[3];
    ON_3dPoint *source_point = NULL;
};

struct repair_fast_constraint_store {
    int face_index = -1;
    std::map<int, std::vector<repair_fast_trim_sample>> trims;
    size_t constrained_edges = 0;
    size_t constrained_samples = 0;
};

static size_t
repair_fast_trim_sample_count(int face_index, int trim_index, void *data)
{
    const repair_fast_constraint_store *store =
	(const repair_fast_constraint_store *)data;
    if (!store || store->face_index != face_index)
	return 0;
    const auto samples = store->trims.find(trim_index);
    return samples == store->trims.end() ? 0 : samples->second.size();
}

static int
repair_fast_trim_sample_get(int face_index, int trim_index,
	size_t sample_index, fastf_t *parameter, point2d_t uv, point_t point,
	void *data)
{
    const repair_fast_constraint_store *store =
	(const repair_fast_constraint_store *)data;
    if (!store || store->face_index != face_index || !parameter || !uv ||
	    !point)
	return -1;
    const auto samples = store->trims.find(trim_index);
    if (samples == store->trims.end() ||
	    sample_index >= samples->second.size())
	return -1;
    const repair_fast_trim_sample &sample =
	samples->second[sample_index];
    *parameter = sample.parameter;
    V2MOVE(uv, sample.uv);
    VMOVE(point, sample.point);
    return 0;
}

static const void *
repair_fast_trim_sample_source(int face_index, int trim_index,
	size_t sample_index, void *data)
{
    const repair_fast_constraint_store *store =
	(const repair_fast_constraint_store *)data;
    if (!store || store->face_index != face_index)
	return NULL;
    const auto samples = store->trims.find(trim_index);
    if (samples == store->trims.end() ||
	    sample_index >= samples->second.size())
	return NULL;
    return samples->second[sample_index].source_point;
}

static void
repair_fast_point_source(int UNUSED(face_index), size_t point_index,
	const void *source, void *data)
{
    std::vector<ON_3dPoint *> *sources =
	(std::vector<ON_3dPoint *> *)data;
    if (!sources)
	return;
    if (sources->size() <= point_index)
	sources->resize(point_index + 1, NULL);
    (*sources)[point_index] = (ON_3dPoint *)source;
}

static bool
repair_fast_trim_sample_append(std::vector<repair_fast_trim_sample> &samples,
	const cdt_mesh_t &mesh, const cpolyedge_t *segment, int endpoint,
	ON_3dPoint **source_point)
{
    if (!segment || !segment->polygon || endpoint < 0 || endpoint > 1)
	return false;
    const auto native = segment->polygon->p2o.find(
	segment->v2d[endpoint]);
    if (native == segment->polygon->p2o.end() || native->second < 0 ||
	    (size_t)native->second >= mesh.m_pnts_2d.size())
	return false;
    const auto point3d = mesh.p2d3d.find(native->second);
    if (point3d == mesh.p2d3d.end() || point3d->second < 0 ||
	    (size_t)point3d->second >= mesh.pnts.size() ||
	    !mesh.pnts[(size_t)point3d->second])
	return false;
    ON_3dPoint *p3d = mesh.pnts[(size_t)point3d->second];
    const std::pair<double, double> &p2d =
	mesh.m_pnts_2d[(size_t)native->second];
    const fastf_t parameter = endpoint ? segment->trim_end :
	segment->trim_start;
    if (!std::isfinite(parameter) || !std::isfinite(p2d.first) ||
	    !std::isfinite(p2d.second) || !p3d->IsValid())
	return false;
    repair_fast_trim_sample sample;
    sample.parameter = parameter;
    V2SET(sample.uv, p2d.first, p2d.second);
    VSET(sample.point, p3d->x, p3d->y, p3d->z);
    sample.source_point = p3d;
    samples.push_back(sample);
    if (source_point)
	*source_point = p3d;
    return true;
}

static repair_fast_constraint_store
repair_fast_face_constraints(struct ON_Brep_CDT_State *s_cdt,
	int face_index)
{
    repair_fast_constraint_store store;
    store.face_index = face_index;
    if (!s_cdt || !s_cdt->brep || face_index < 0 ||
	    face_index >= s_cdt->brep->m_F.Count())
	return store;
    const auto mesh_entry = s_cdt->fmeshes.find(face_index);
    if (mesh_entry == s_cdt->fmeshes.end())
	return store;
    cdt_mesh_t &mesh = mesh_entry->second;
    const ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    const ON_BrepLoop *outer = face.OuterLoop();
    if (!outer || mesh.outer_loop.poly.empty())
	return store;
    for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	const ON_BrepLoop *loop = face.Loop(loop_index);
	if (!loop || loop == outer)
	    continue;
	const auto polygon = mesh.inner_loops.find(loop_index);
	if (polygon == mesh.inner_loops.end() || !polygon->second ||
		polygon->second->poly.empty())
	    return store;
    }
    std::map<int, std::map<fastf_t, repair_fast_trim_sample>> trim_points;
    std::set<int> unusable_trims;
    for (cpolyedge_t *segment : cdt_face_polyedges(s_cdt, face_index)) {
	if (!segment || segment->trim_ind < 0 ||
		segment->trim_ind >= s_cdt->brep->m_T.Count())
	    continue;
	const ON_BrepTrim &trim =
	    s_cdt->brep->m_T[segment->trim_ind];
	if (trim.m_ei < 0 || trim.m_ei >= s_cdt->brep->m_E.Count())
	    continue;
	const ON_BrepEdge &edge = s_cdt->brep->m_E[trim.m_ei];
	bool sampled_neighbor = false;
	for (int trim_index = 0; trim_index < edge.TrimCount(); ++trim_index) {
	    const ON_BrepTrim *other = edge.Trim(trim_index);
	    const ON_BrepFace *other_face = other ? other->Face() : NULL;
	    if (!other_face || other_face->m_face_index == face_index)
		continue;
	    const int other_index = other_face->m_face_index;
	    sampled_neighbor = other_index >= 0 &&
		s_cdt->fmeshes.find(other_index) != s_cdt->fmeshes.end();
	    if (sampled_neighbor)
		break;
	}
	/* Edge subdivision is global and precedes face triangulation.  It is
	 * therefore authoritative even when both adjacent faces later fail:
	 * giving each local fast patch the same 3-D samples prevents repair from
	 * having to close an artificial failed-face/failed-face seam. */
	if (!sampled_neighbor || unusable_trims.find(segment->trim_ind) !=
		unusable_trims.end())
	    continue;
	std::vector<repair_fast_trim_sample> additions;
	if (!repair_fast_trim_sample_append(additions, mesh, segment, 0,
		NULL) || !repair_fast_trim_sample_append(additions, mesh,
		segment, 1, NULL)) {
	    store.trims.erase(segment->trim_ind);
	    trim_points.erase(segment->trim_ind);
	    unusable_trims.insert(segment->trim_ind);
	    continue;
	}
	std::map<fastf_t, repair_fast_trim_sample> &points =
	    trim_points[segment->trim_ind];
	for (const repair_fast_trim_sample &sample : additions) {
	    const auto existing = points.find(sample.parameter);
	    if (existing == points.end()) {
		points.emplace(sample.parameter, sample);
		continue;
	    }
	    const repair_fast_trim_sample &prior = existing->second;
	    const double delta_3d = std::hypot(std::hypot(
		prior.point[X] - sample.point[X],
		prior.point[Y] - sample.point[Y]),
		prior.point[Z] - sample.point[Z]);
	    const double delta_uv = std::hypot(
		prior.uv[X] - sample.uv[X],
		prior.uv[Y] - sample.uv[Y]);
	    if (delta_3d > ON_ZERO_TOLERANCE ||
		    delta_uv > ON_ZERO_TOLERANCE) {
		trim_points.erase(segment->trim_ind);
		unusable_trims.insert(segment->trim_ind);
		break;
	    }
	}
    }
    for (const auto &trim_points_entry : trim_points) {
	if (unusable_trims.find(trim_points_entry.first) !=
		unusable_trims.end() || trim_points_entry.second.size() < 2)
	    continue;
	const ON_BrepTrim &trim = s_cdt->brep->m_T[trim_points_entry.first];
	const ON_Interval domain = trim.Domain();
	const double parameter_scale = std::max(1.0, std::max(
	    std::fabs(domain.Min()), std::fabs(domain.Max())));
	const double parameter_tolerance = 256.0 *
	    std::numeric_limits<double>::epsilon() * parameter_scale;
	if (std::fabs(trim_points_entry.second.begin()->first - domain.Min()) >
		parameter_tolerance || std::fabs(
		trim_points_entry.second.rbegin()->first - domain.Max()) >
		parameter_tolerance)
	    continue;
	std::vector<repair_fast_trim_sample> &samples =
	    store.trims[trim_points_entry.first];
	for (const auto &sample : trim_points_entry.second)
	    samples.push_back(sample.second);
	store.constrained_edges++;
	store.constrained_samples += samples.size();
    }
    return store;
}

struct repair_boundary_patch {
    std::vector<int> faces;
    std::vector<fastf_t> vertices;
    std::vector<ON_3dPoint *> source_points;
    std::vector<double> direct_surface_deviations;
    size_t constrained_edges = 0;
    size_t constrained_samples = 0;
    size_t periodic_u_samples = 0;
    size_t periodic_v_samples = 0;
    size_t strip_interior_rows = 0;
    int strip_correspondence_step = 0;
    size_t strip_correspondence_shift = 0;
    bool analytic_torus_strip = false;
    bool analytic_cylinder_strip = false;
};

typedef std::pair<ON_3dPoint *, ON_3dPoint *> repair_source_edge;

static repair_source_edge
repair_ordered_source_edge(ON_3dPoint *first, ON_3dPoint *second)
{
    if (std::less<ON_3dPoint *>()(second, first))
	std::swap(first, second);
    return repair_source_edge(first, second);
}

/* A cleaned chart must not invent a second boundary inside a face.  Compare
 * its source-point boundary to a topology-defined disk built from the exact
 * shared-edge samples.  Interior edges must occur once in each direction and
 * every boundary vertex must have degree two. */
static bool
repair_patch_source_boundary(const int *faces, int face_count,
	const std::vector<ON_3dPoint *> &sources,
	std::set<repair_source_edge> &boundary)
{
    boundary.clear();
    if (!faces || face_count <= 0 || sources.size() < 3)
	return false;
    struct edge_use {
	int count = 0;
	int direction = 0;
    };
    std::map<std::pair<int, int>, edge_use> edges;
    for (int face = 0; face < face_count; ++face) {
	for (int corner = 0; corner < 3; ++corner) {
	    const int from = faces[(size_t)face * 3 + corner];
	    const int to = faces[(size_t)face * 3 + (corner + 1) % 3];
	    if (from < 0 || to < 0 || from == to ||
		    (size_t)from >= sources.size() ||
		    (size_t)to >= sources.size())
		return false;
	    const std::pair<int, int> edge = from < to ?
		std::make_pair(from, to) : std::make_pair(to, from);
	    edge_use &use = edges[edge];
	    use.count++;
	    use.direction += from < to ? 1 : -1;
	}
    }
    std::map<int, int> boundary_degree;
    for (const auto &entry : edges) {
	const edge_use &use = entry.second;
	if (use.count == 2 && use.direction == 0)
	    continue;
	if (use.count != 1)
	    return false;
	ON_3dPoint *first = sources[(size_t)entry.first.first];
	ON_3dPoint *second = sources[(size_t)entry.first.second];
	if (!first || !second || first == second)
	    return false;
	boundary.insert(repair_ordered_source_edge(first, second));
	boundary_degree[entry.first.first]++;
	boundary_degree[entry.first.second]++;
    }
    if (boundary.empty())
	return false;
    for (const auto &entry : boundary_degree) {
	if (entry.second != 2)
	    return false;
    }
    return true;
}

static bool
repair_patch_matches_source_boundary(const int *faces, int face_count,
	const std::vector<ON_3dPoint *> &sources,
	const repair_boundary_patch &reference)
{
    std::set<repair_source_edge> candidate_boundary;
    std::set<repair_source_edge> reference_boundary;
    return repair_patch_source_boundary(faces, face_count, sources,
	    candidate_boundary) && repair_patch_source_boundary(
	    reference.faces.data(), (int)(reference.faces.size() / 3),
	    reference.source_points, reference_boundary) &&
	    candidate_boundary == reference_boundary;
}

static bool
repair_topological_disk_stage(int stage)
{
    return stage == BREP_CDT_STAGE_FACE_TRIANGULATION ||
	stage == BREP_CDT_STAGE_PSLG_VALIDATION ||
	stage == BREP_CDT_STAGE_CHART_CONSTRUCTION;
}

int
cdt_test_repair_patch_boundary(void)
{
    ON_3dPoint points[4] = {
	ON_3dPoint(0.0, 0.0, 0.0),
	ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0),
	ON_3dPoint(0.0, 1.0, 0.0)
    };
    repair_boundary_patch reference;
    reference.faces = {
	0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4
    };
    reference.source_points = {
	&points[0], &points[1], &points[2], &points[3], NULL
    };
    const std::vector<ON_3dPoint *> sources = {
	&points[0], &points[1], &points[2], &points[3]
    };
    const int valid[] = {0, 1, 2, 0, 2, 3};
    if (!repair_patch_matches_source_boundary(valid, 2, sources,
	    reference))
	return 1;
    const int open[] = {0, 1, 2};
    if (repair_patch_matches_source_boundary(open, 1, sources,
	    reference))
	return 2;
    const int misoriented[] = {0, 1, 2, 0, 3, 2};
    if (repair_patch_matches_source_boundary(misoriented, 2, sources,
	    reference))
	return 3;
    std::vector<ON_3dPoint *> missing_source = sources;
    missing_source[2] = NULL;
    if (repair_patch_matches_source_boundary(valid, 2, missing_source,
	    reference))
	return 4;
    return repair_topological_disk_stage(
	BREP_CDT_STAGE_FACE_TRIANGULATION) &&
	repair_topological_disk_stage(BREP_CDT_STAGE_PSLG_VALIDATION) &&
	repair_topological_disk_stage(BREP_CDT_STAGE_CHART_CONSTRUCTION) &&
	!repair_topological_disk_stage(BREP_CDT_STAGE_ADAPTIVE_REFINEMENT) ?
	0 : 5;
}

/* Some imported faces have a trustworthy topological disk boundary but no
 * globally consistent pullback: loose incident edge curves can cross on the
 * source surface even though their authoritative 3-D samples form the local
 * boundary expected by adjacent faces.  Preserve every shared sample and use
 * the loop order itself as the disk embedding.  A single interior point keeps
 * the interpretation bounded to this failed face and avoids the unreliable
 * best-fit-plane projection historically used for this case. */
static bool
repair_constrained_topological_disk(struct ON_Brep_CDT_State *s_cdt,
	int face_index, const repair_fast_constraint_store &constraints,
	size_t max_points, repair_boundary_patch &patch)
{
    patch = repair_boundary_patch();
    const auto reject = [&](const char *reason) {
	if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY"))
	    bu_log("Face %d: topological disk rejected: %s\n", face_index,
		reason);
	patch = repair_boundary_patch();
	return false;
    };
    if (!s_cdt || !s_cdt->brep || face_index < 0 ||
	    face_index >= s_cdt->brep->m_F.Count() || max_points < 4 ||
	    constraints.face_index != face_index)
	return reject("invalid input state or point limit");
    const ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    const ON_BrepLoop *outer = face.OuterLoop();
    const ON_Surface *surface = face.SurfaceOf();
    if (!outer || !surface || face.LoopCount() != 1 ||
	    outer->TrimCount() < 3)
	return reject("face is not one topological disk loop");

    struct disk_point {
	ON_2dPoint uv;
	ON_3dPoint point;
	ON_3dPoint *source_point;
    };
    std::vector<disk_point> boundary;
    size_t constrained_edges = 0;
    size_t constrained_samples = 0;
    const double coordinate_scale = std::max(1.0,
	s_cdt->brep->BoundingBox().Diagonal().Length());
    const double join_tolerance = 4096.0 *
	std::numeric_limits<double>::epsilon() * coordinate_scale;
    const auto same_point = [&](const ON_3dPoint &first,
	    const ON_3dPoint &second) {
	return first.DistanceTo(second) <= join_tolerance;
    };
    for (int trim_index = 0; trim_index < outer->TrimCount();
	    ++trim_index) {
	const ON_BrepTrim *trim = outer->Trim(trim_index);
	if (!trim)
	    return reject("missing outer-loop trim");
	if (trim->m_type == ON_BrepTrim::singular)
	    return reject("outer loop contains a singular trim");
	const auto stored = constraints.trims.find(trim->m_trim_index);
	if (stored == constraints.trims.end() || stored->second.size() < 2)
	    return reject("outer trim lacks authoritative samples");
	const std::vector<repair_fast_trim_sample> &samples = stored->second;
	const ON_2dPoint trim_start = trim->PointAt(trim->Domain().Min());
	const double front_distance = std::hypot(
	    samples.front().uv[X] - trim_start.x,
	    samples.front().uv[Y] - trim_start.y);
	const double back_distance = std::hypot(
	    samples.back().uv[X] - trim_start.x,
	    samples.back().uv[Y] - trim_start.y);
	const bool reverse = back_distance < front_distance;
	for (size_t i = 0; i < samples.size(); ++i) {
	    const repair_fast_trim_sample &sample = reverse ?
		samples[samples.size() - 1 - i] : samples[i];
	    const disk_point next = {
		ON_2dPoint(sample.uv[X], sample.uv[Y]),
		ON_3dPoint(sample.point),
		sample.source_point
	    };
	    if (!next.uv.IsValid() || !next.point.IsValid())
		return reject("outer trim has an invalid sample");
	    if (i == 0 && !boundary.empty() &&
		    !same_point(boundary.back().point, next.point))
		return reject("successive outer trims do not join");
	    if (!boundary.empty() && same_point(boundary.back().point,
		    next.point))
		continue;
	    boundary.push_back(next);
	}
	constrained_edges++;
	constrained_samples += samples.size();
    }
    const bool closed_boundary = boundary.size() > 1 &&
	same_point(boundary.front().point, boundary.back().point);
    if (closed_boundary)
	boundary.pop_back();
    if (!closed_boundary || boundary.size() < 3 ||
	    boundary.size() + 1 > max_points)
	return reject("outer sample ring is open, small, or over limit");
    std::set<repair_point_key> distinct_boundary;
    for (const disk_point &point : boundary) {
	const repair_point_key key = {
	    point.point.x, point.point.y, point.point.z
	};
	if (!distinct_boundary.insert(key).second)
	    return reject("outer sample ring revisits a 3-D point");
    }

    ON_2dPoint center_uv(0.0, 0.0);
    ON_3dPoint boundary_center(0.0, 0.0, 0.0);
    for (const disk_point &point : boundary) {
	center_uv += point.uv;
	boundary_center += point.point;
    }
    center_uv /= (double)boundary.size();
    boundary_center /= (double)boundary.size();
    ON_3dPoint center = surface->PointAt(center_uv.x, center_uv.y);
    const double allowed_center_offset = std::isfinite(s_cdt->absmax) &&
	s_cdt->absmax > 0.0 ? std::max((double)BN_TOL_DIST,
	(double)s_cdt->absmax) : (double)BN_TOL_DIST;
    if (!center.IsValid() || center.DistanceTo(boundary_center) >
	    4.0 * allowed_center_offset)
	center = boundary_center;
    if (!center.IsValid())
	return reject("could not construct a valid interior point");

    patch.vertices.reserve((boundary.size() + 1) * 3);
    patch.source_points.reserve(boundary.size() + 1);
    for (const disk_point &point : boundary) {
	patch.vertices.push_back(point.point.x);
	patch.vertices.push_back(point.point.y);
	patch.vertices.push_back(point.point.z);
	patch.source_points.push_back(point.source_point);
    }
    const int center_index = (int)boundary.size();
    patch.vertices.push_back(center.x);
    patch.vertices.push_back(center.y);
    patch.vertices.push_back(center.z);
    patch.source_points.push_back(NULL);

    ON_3dVector fan_normal(0.0, 0.0, 0.0);
    const double area_tolerance = 4096.0 *
	std::numeric_limits<double>::epsilon() * coordinate_scale *
	coordinate_scale;
    for (size_t i = 0; i < boundary.size(); ++i) {
	const size_t next = (i + 1) % boundary.size();
	const ON_3dVector normal = ON_CrossProduct(
	    boundary[next].point - boundary[i].point,
	    center - boundary[i].point);
	if (!(normal.Length() > area_tolerance)) {
	    return reject("boundary edge and interior point are collinear");
	}
	fan_normal += normal;
	patch.faces.push_back((int)i);
	patch.faces.push_back((int)next);
	patch.faces.push_back(center_index);
    }
    ON_3dPoint surface_point;
    ON_3dVector surface_normal;
    if (surface_EvNormal(surface, center_uv.x, center_uv.y,
	    surface_point, surface_normal)) {
	if (face.m_bRev)
	    surface_normal = -surface_normal;
	if (fan_normal * surface_normal < 0.0) {
	    for (size_t triangle = 0; triangle < patch.faces.size() / 3;
		    ++triangle)
		std::swap(patch.faces[triangle * 3 + 1],
		    patch.faces[triangle * 3 + 2]);
	}
    }
    patch.constrained_edges = constrained_edges;
    patch.constrained_samples = constrained_samples;
    return true;
}

struct repair_degenerate_neighborhood_stats {
    int components = 0;
    int removed_faces = 0;
    int added_faces = 0;
    int boundary_edges = 0;
    double max_center_offset = 0.0;
    bool inferred_topology = false;
    std::set<int> source_faces;
};

static bool
repair_internal_face_seam(const ON_BrepTrim *trim,
	const ON_BrepFace &face)
{
    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
    if (!edge || edge->TrimCount() < 2)
	return false;
    for (int edge_trim = 0; edge_trim < edge->TrimCount(); ++edge_trim) {
	const ON_BrepTrim *other = edge->Trim(edge_trim);
	if (!other || other->Face() != &face)
	    return false;
    }
    return true;
}


/* Removing the artificial seam from a periodic side face leaves two closed
 * outer boundary rings rather than one polygon outline.  Cut both rings at a
 * common periodic phase, form a rectangular chart, and retain every other
 * ring as a hole.  Duplicate chart endpoints map back to the same indexed 3-D
 * vertices, making the two artificial seam edges one ordinary interior edge
 * in the resulting patch. */
static bool
repair_periodic_annulus_chart(
	const std::vector<detria::PointD> &surface_points,
	const std::vector<std::vector<int>> &surface_rings,
	const std::vector<int> &point_vertices, int closed_direction,
	double period, const fastf_t *vertices, int vertex_count,
	double reference_area,
	bool debug_topology, std::vector<int> &patch_faces)
{
    patch_faces.clear();
    if (!vertices || vertex_count <= 0 || surface_rings.size() < 3 ||
	    point_vertices.size() != surface_points.size() ||
	    (closed_direction != 0 && closed_direction != 1) ||
	    !(period > 0.0) || !std::isfinite(period))
	return false;
    size_t boundary_points = 0;
    double open_min = DBL_MAX;
    double open_max = -DBL_MAX;
    for (const std::vector<int> &ring : surface_rings) {
	if (ring.size() < 3)
	    return false;
	boundary_points += ring.size();
	for (int point : ring) {
	    if (point < 0 || (size_t)point >= surface_points.size())
		return false;
	    const int vertex = point_vertices[(size_t)point];
	    const detria::PointD &uv = surface_points[(size_t)point];
	    if (vertex < 0 || vertex >= vertex_count ||
		    !std::isfinite(uv.x) || !std::isfinite(uv.y))
		return false;
	    const double open = closed_direction ?
		uv.x : uv.y;
	    open_min = std::min(open_min, open);
	    open_max = std::max(open_max, open);
	}
    }
    const double open_scale = std::max(1.0, open_max - open_min);
    const double open_tolerance = 1.0e-6 * open_scale;
    std::vector<size_t> strip_rings;
    std::vector<double> open_means(surface_rings.size(), 0.0);
    for (size_t ring_index = 0; ring_index < surface_rings.size();
	    ++ring_index) {
	double closed_min = DBL_MAX;
	double closed_max = -DBL_MAX;
	double ring_open_min = DBL_MAX;
	double ring_open_max = -DBL_MAX;
	for (int point : surface_rings[ring_index]) {
	    const detria::PointD &uv = surface_points[(size_t)point];
	    const double closed = closed_direction ? uv.y : uv.x;
	    const double open = closed_direction ? uv.x : uv.y;
	    closed_min = std::min(closed_min, closed);
	    closed_max = std::max(closed_max, closed);
	    ring_open_min = std::min(ring_open_min, open);
	    ring_open_max = std::max(ring_open_max, open);
	    open_means[ring_index] += open;
	}
	open_means[ring_index] /= (double)surface_rings[ring_index].size();
	if (debug_topology)
	    bu_log("Periodic annulus ring %zu: closed span %.17g, open "
		"span %.17g\n", ring_index, closed_max - closed_min,
		ring_open_max - ring_open_min);
	/* Both longitudinal boundaries wind almost once around the periodic
	 * surface.  They need not be iso-curves: imported product features may
	 * make either rim rise and fall substantially in the open direction. */
	if (closed_max - closed_min >= 0.75 * period)
	    strip_rings.push_back(ring_index);
    }
    if (strip_rings.size() != 2)
	return false;
    if (open_means[strip_rings[1]] < open_means[strip_rings[0]])
	std::swap(strip_rings[0], strip_rings[1]);
    const double lower_open = open_means[strip_rings[0]];
    const double upper_open = open_means[strip_rings[1]];
    if (!(upper_open - lower_open > open_tolerance))
	return false;

    const auto closed_coordinate = [&](int point) {
	return closed_direction ? surface_points[(size_t)point].y :
	    surface_points[(size_t)point].x;
    };
    const auto open_coordinate = [&](int point) {
	return closed_direction ? surface_points[(size_t)point].x :
	    surface_points[(size_t)point].y;
    };
    const auto ordered_ring = [&](size_t ring_index) {
	std::vector<int> ordered = surface_rings[ring_index];
	if (closed_coordinate(ordered.back()) <
		closed_coordinate(ordered.front()))
	    std::reverse(ordered.begin(), ordered.end());
	return ordered;
    };
    const std::vector<int> lower = ordered_ring(strip_rings[0]);
    const std::vector<int> upper = ordered_ring(strip_rings[1]);
    std::vector<size_t> holes;
    for (size_t ring_index = 0; ring_index < surface_rings.size();
	    ++ring_index) {
	if (ring_index != strip_rings[0] && ring_index != strip_rings[1])
	    holes.push_back(ring_index);
    }

    std::vector<int> best_faces;
    double best_area = 0.0;
    double best_delta = std::numeric_limits<double>::infinity();
    const double phase_margin = 4096.0 *
	std::numeric_limits<double>::epsilon();
    const size_t cut_attempts = std::min((size_t)64, lower.size());
    for (size_t attempt = 0; attempt < cut_attempts; ++attempt) {
	const size_t cut = attempt * lower.size() / cut_attempts;
	std::vector<int> lower_order(lower.size());
	for (size_t point = 0; point < lower.size(); ++point)
	    lower_order[point] = lower[(cut + point) % lower.size()];
	const double origin = closed_coordinate(lower_order.front());
	size_t upper_cut = 0;
	double upper_cut_distance = DBL_MAX;
	for (size_t point = 0; point < upper.size(); ++point) {
	    double delta = (closed_coordinate(upper[point]) - origin) / period;
	    delta -= std::round(delta);
	    if (std::fabs(delta) < upper_cut_distance) {
		upper_cut_distance = std::fabs(delta);
		upper_cut = point;
	    }
	}
	std::vector<int> upper_order(upper.size());
	for (size_t point = 0; point < upper.size(); ++point)
	    upper_order[point] = upper[(upper_cut + point) % upper.size()];

	const auto ring_phases = [&](const std::vector<int> &ordered,
		std::vector<double> &phases) {
	    phases.resize(ordered.size());
	    double prior = 0.0;
	    for (size_t point = 0; point < ordered.size(); ++point) {
		double phase = (closed_coordinate(ordered[point]) - origin) /
		    period;
		if (!point)
		    phase -= std::round(phase);
		else {
		    phase += std::round(prior - phase);
		    if (phase <= prior + phase_margin)
			phase += 1.0;
		}
		if (phase < -phase_margin || phase >= 1.0 - phase_margin)
		    return false;
		phases[point] = std::max(0.0, phase);
		prior = phase;
	    }
	    return true;
	};
	std::vector<double> lower_phases;
	std::vector<double> upper_phases;
	if (!ring_phases(lower_order, lower_phases) ||
		!ring_phases(upper_order, upper_phases))
	    continue;

	std::vector<detria::PointD> chart_points;
	std::vector<int> chart_vertices;
	std::vector<int> outline;
	std::vector<std::vector<int>> chart_holes;
	chart_points.reserve(boundary_points + 2);
	chart_vertices.reserve(boundary_points + 2);
	for (size_t point = 0; point < lower_order.size(); ++point) {
	    outline.push_back((int)chart_points.size());
	    chart_points.push_back({lower_phases[point], 0.0});
	    chart_vertices.push_back(
		point_vertices[(size_t)lower_order[point]]);
	}
	outline.push_back((int)chart_points.size());
	chart_points.push_back({1.0, 0.0});
	chart_vertices.push_back(
	    point_vertices[(size_t)lower_order.front()]);
	const int upper_base = (int)chart_points.size();
	for (size_t point = 0; point < upper_order.size(); ++point) {
	    chart_points.push_back({upper_phases[point], 1.0});
	    chart_vertices.push_back(
		point_vertices[(size_t)upper_order[point]]);
	}
	const int upper_duplicate = (int)chart_points.size();
	chart_points.push_back({1.0, 1.0});
	chart_vertices.push_back(
	    point_vertices[(size_t)upper_order.front()]);
	outline.push_back(upper_duplicate);
	for (size_t point = upper_order.size(); point > 0; --point)
	    outline.push_back(upper_base + (int)point - 1);

	bool holes_valid = true;
	for (size_t ring_index : holes) {
	    std::vector<double> phases(surface_rings[ring_index].size());
	    double phase_min = DBL_MAX;
	    double phase_max = -DBL_MAX;
	    for (size_t point = 0; point < surface_rings[ring_index].size();
		    ++point) {
		const int source_point = surface_rings[ring_index][point];
		double phase = (closed_coordinate(source_point) - origin) /
		    period;
		if (point)
		    phase += std::round(phases[point - 1] - phase);
		phases[point] = phase;
		phase_min = std::min(phase_min, phase);
		phase_max = std::max(phase_max, phase);
	    }
	    const double shift = std::floor(0.5 * (phase_min + phase_max));
	    for (double &phase : phases)
		phase -= shift;
	    phase_min -= shift;
	    phase_max -= shift;
	    if (phase_min <= phase_margin ||
		    phase_max >= 1.0 - phase_margin) {
		holes_valid = false;
		break;
	    }
	    std::vector<int> chart_hole;
	    chart_hole.reserve(surface_rings[ring_index].size());
	    for (size_t point = 0; point < surface_rings[ring_index].size();
		    ++point) {
		const int source_point = surface_rings[ring_index][point];
		const double y = (open_coordinate(source_point) - lower_open) /
		    (upper_open - lower_open);
		if (y <= phase_margin || y >= 1.0 - phase_margin) {
		    holes_valid = false;
		    break;
		}
		chart_hole.push_back((int)chart_points.size());
		chart_points.push_back({phases[point], y});
		chart_vertices.push_back(point_vertices[(size_t)source_point]);
	    }
	    if (!holes_valid)
		break;
	    chart_holes.push_back(std::move(chart_hole));
	}
	if (!holes_valid || chart_points.size() != boundary_points + 2)
	    continue;

	const auto signed_area = [&](const std::vector<int> &ring) {
	    double area = 0.0;
	    for (size_t point = 0; point < ring.size(); ++point) {
		const detria::PointD &first = chart_points[(size_t)ring[point]];
		const detria::PointD &second = chart_points[(size_t)ring[
		    (point + 1) % ring.size()]];
		area += first.x * second.y - second.x * first.y;
	    }
	    return area;
	};
	if (signed_area(outline) < 0.0)
	    std::reverse(outline.begin(), outline.end());
	for (std::vector<int> &hole : chart_holes) {
	    if (signed_area(hole) > 0.0)
		std::reverse(hole.begin(), hole.end());
	}
	detria::Triangulation<detria::PointD, int> triangulation;
	triangulation.setPoints(chart_points);
	triangulation.addOutline(outline);
	for (const std::vector<int> &hole : chart_holes)
	    triangulation.addHole(hole);
	try {
	    if (!triangulation.triangulate(true))
		continue;
	} catch (...) {
	    continue;
	}
	std::vector<int> candidate;
	bool valid = true;
	triangulation.forEachTriangle(
	    [&](const detria::Triangle<int> triangle) {
		if (!valid)
		    return;
		const int mapped[3] = {
		    chart_vertices[(size_t)triangle.x],
		    chart_vertices[(size_t)triangle.y],
		    chart_vertices[(size_t)triangle.z]};
		if (mapped[0] == mapped[1] || mapped[1] == mapped[2] ||
			mapped[2] == mapped[0]) {
		    valid = false;
		    return;
		}
		candidate.insert(candidate.end(), mapped, mapped + 3);
	    }, true);
	const size_t expected_faces = boundary_points +
	    2 * surface_rings.size() - 4;
	if (!valid || candidate.size() / 3 != expected_faces)
	    continue;
	std::map<std::pair<int, int>, size_t> uses;
	double area = 0.0;
	for (size_t face = 0; valid && face < candidate.size() / 3; ++face) {
	    ON_3dPoint points[3];
	    for (int corner = 0; corner < 3; ++corner) {
		const int vertex = candidate[face * 3 + (size_t)corner];
		if (vertex < 0) {
		    valid = false;
		    break;
		}
		points[corner] = ON_3dPoint(
		    &vertices[(size_t)vertex * 3]);
		uses[std::minmax(vertex,
		    candidate[face * 3 + (size_t)((corner + 1) % 3)])]++;
	    }
	    if (!valid)
		break;
	    const ON_3dVector first = points[1] - points[0];
	    const ON_3dVector second = points[2] - points[0];
	    const double longest_squared = std::max(first.LengthSquared(),
		std::max(second.LengthSquared(),
		    points[1].DistanceToSquared(points[2])));
	    const double doubled_area = ON_CrossProduct(first, second).Length();
	    if (!(longest_squared > 0.0) || !std::isfinite(doubled_area) ||
		    doubled_area <= 64.0 *
		    std::numeric_limits<double>::epsilon() * longest_squared) {
		valid = false;
		break;
	    }
	    area += 0.5 * doubled_area;
	}
	std::set<std::pair<int, int>> source_edges;
	for (const std::vector<int> &ring : surface_rings) {
	    for (size_t point = 0; point < ring.size(); ++point) {
		const int first = point_vertices[(size_t)ring[point]];
		const int second = point_vertices[(size_t)ring[
		    (point + 1) % ring.size()]];
		source_edges.insert(std::minmax(first, second));
	    }
	}
	for (const auto &edge : uses) {
	    const size_t expected = source_edges.find(edge.first) !=
		source_edges.end() ? 1 : 2;
	    if (edge.second != expected) {
		valid = false;
		break;
	    }
	}
	for (const auto &edge : source_edges) {
	    if (uses[edge] != 1) {
		valid = false;
		break;
	    }
	}
	if (!valid)
	    continue;
	const double delta = std::fabs(area - reference_area);
	if (delta < best_delta) {
	    best_delta = delta;
	    best_area = area;
	    best_faces.swap(candidate);
	}
    }
    const double area_scale = std::max(reference_area,
	std::numeric_limits<double>::min());
    if (best_faces.empty() || !std::isfinite(best_delta) ||
	    best_delta > 4.0 * area_scale)
	return false;
    patch_faces.swap(best_faces);
    if (debug_topology)
	bu_log("Rigorous-boundary periodic annulus chart accepted %zu "
	    "rings, %zu triangles, area %.17g (reference %.17g)\n",
	    surface_rings.size(), patch_faces.size() / 3, best_area,
	    reference_area);
    return true;
}


/* When a failed-face triangulation cannot reproduce its authoritative
 * neighbors, discard only that approximate face and recover the exact indexed
 * rings exposed by the retained rigorous prefix.  Close one ring with a
 * bounded fan.  For holes, try the source chart, topology-preserving Clipper
 * cleanup, a bounded inferred chart, and finally a certified common plane.
 * No path moves or welds a retained rigorous vertex. */
static bool
repair_failed_face_from_rigorous_boundary(
	struct ON_Brep_CDT_State *s_cdt, int **faces, int *face_count,
	fastf_t **vertices, int *vertex_count, int rigorous_face_count,
	std::vector<int> &source_faces, double allowed_deviation,
	size_t max_boundary_edges, repair_degenerate_neighborhood_stats *stats,
	int requested_source_face = -1,
	const std::set<std::pair<int, int>> *requested_boundary = NULL,
	bool require_solid = true, std::vector<int> *vertex_remap = NULL)
{
    const bool debug_topology = getenv(
	"BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY") != NULL;
    if (!s_cdt || !s_cdt->orig_brep || !faces || !face_count ||
	    !vertices || !vertex_count || !*faces || !*vertices ||
	    rigorous_face_count <= 0 || rigorous_face_count >= *face_count ||
	    source_faces.size() != (size_t)*face_count ||
	    !(allowed_deviation > 0.0) || !stats ||
	    (requested_boundary && requested_source_face < 0)) {
	if (debug_topology)
	    bu_log("Rigorous-boundary face repair unavailable: state %d, "
		"faces %d/%d, vertices %d, sources %zu, deviation %.17g, "
		"stats %d, requested source %d\n", s_cdt &&
		s_cdt->orig_brep && faces && face_count && *faces ? 1 : 0,
		rigorous_face_count, face_count ? *face_count : -1,
		vertex_count ? *vertex_count : -1, source_faces.size(),
		allowed_deviation, stats ? 1 : 0, requested_source_face);
	return false;
    }
    const auto reject_boundary = [&](const char *reason) {
	if (debug_topology)
	    bu_log("Rigorous-boundary face repair rejected: %s\n", reason);
	return false;
    };

    std::set<int> approximate_sources;
    for (int face = rigorous_face_count; face < *face_count; ++face) {
	if (source_faces[(size_t)face] < 0)
	    return reject_boundary("an approximate triangle has no source face");
	approximate_sources.insert(source_faces[(size_t)face]);
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = (*faces)[(size_t)face * 3 + corner];
	    if (vertex < 0 || vertex >= *vertex_count)
		return false;
	}
    }
    if ((requested_source_face < 0 && approximate_sources.size() != 1) ||
	    (requested_source_face >= 0 && approximate_sources.find(
		requested_source_face) == approximate_sources.end()))
	return reject_boundary("the approximate suffix does not identify the "
	    "requested source face");
    const int source_face = requested_source_face >= 0 ?
	requested_source_face : *approximate_sources.begin();
    if (source_face < 0 || source_face >= s_cdt->orig_brep->m_F.Count())
	return reject_boundary("the source face index is out of range");

    typedef std::pair<int, int> mesh_edge;
    struct edge_use {
	int face;
	int from;
	int to;
    };
    std::map<mesh_edge, std::vector<edge_use>> incidence;
    const int incidence_face_count = requested_boundary ? *face_count :
	rigorous_face_count;
    for (int face = 0; face < incidence_face_count; ++face) {
	if (requested_boundary && face >= rigorous_face_count &&
		source_faces[(size_t)face] == source_face)
	    continue;
	for (int corner = 0; corner < 3; ++corner) {
	    const int from = (*faces)[(size_t)face * 3 + corner];
	    const int to = (*faces)[(size_t)face * 3 + (corner + 1) % 3];
	    if (from == to)
		continue;
	    if (from < 0 || to < 0 || from >= *vertex_count ||
		    to >= *vertex_count)
		return reject_boundary("mesh incidence has an invalid vertex");
	    incidence[std::minmax(from, to)].push_back({face, from, to});
	}
    }
    std::vector<edge_use> boundary;
    std::set<mesh_edge> selected_boundary;
    for (const auto &edge : incidence) {
	if (requested_boundary && requested_boundary->find(edge.first) ==
		requested_boundary->end())
	    continue;
	if (edge.second.size() > 2) {
	    if (debug_topology) {
		bu_log("Rigorous-boundary retained edge %d-%d has %zu uses:",
		    edge.first.first, edge.first.second, edge.second.size());
		for (const edge_use &use : edge.second)
		    bu_log(" triangle %d/source %d [%d,%d,%d]", use.face,
			(size_t)use.face < source_faces.size() ?
			source_faces[(size_t)use.face] : -1,
			(*faces)[(size_t)use.face * 3],
			(*faces)[(size_t)use.face * 3 + 1],
			(*faces)[(size_t)use.face * 3 + 2]);
		bu_log("\n");
		for (int vertex : {edge.first.first, edge.first.second}) {
		    ON_3dPoint *point = s_cdt->bot_pnt_to_on_pnt &&
			(size_t)vertex < s_cdt->bot_pnt_to_on_pnt->size() ?
			(*s_cdt->bot_pnt_to_on_pnt)[(size_t)vertex] : NULL;
		    const cdt_audit_info *audit = NULL;
		    if (point && s_cdt->pnt_audit_info) {
			const auto found = s_cdt->pnt_audit_info->find(point);
			if (found != s_cdt->pnt_audit_info->end())
			    audit = &found->second;
		    }
		    bu_log("  vertex %d face/trim/edge %d/%d/%d\n",
			vertex, audit ? audit->face_index : -1,
			audit ? audit->trim_index : -1,
			audit ? audit->edge_index : -1);
		}
	    }
	    return reject_boundary("retained mesh incidence is nonmanifold");
	}
	if (edge.second.size() == 1) {
	    const edge_use &retained = edge.second.front();
	    boundary.push_back({retained.face, retained.to, retained.from});
	    selected_boundary.insert(edge.first);
	}
    }
    if (requested_boundary && selected_boundary != *requested_boundary) {
	if (debug_topology)
	    bu_log("Face %d: requested %zu rigorous boundary edges but "
		"found %zu in mesh incidence\n", source_face,
		requested_boundary->size(), selected_boundary.size());
	if (debug_topology) {
	    size_t logged = 0;
	    for (const mesh_edge &edge : *requested_boundary) {
		if (selected_boundary.find(edge) == selected_boundary.end() &&
			logged++ < 8)
		    bu_log("  requested-only boundary edge %d-%d\n",
			edge.first, edge.second);
	    }
	}
	return reject_boundary("the requested retained boundary is incomplete");
    }
    if (debug_topology && requested_boundary)
	bu_log("Face %d: selected all %zu indexed source boundary edges\n",
	    source_face, requested_boundary->size());
    if (boundary.size() < 3 || boundary.size() > max_boundary_edges)
	return reject_boundary("the retained boundary size is outside limits");

    std::vector<std::vector<int>> rings;
    std::vector<bool> used(boundary.size(), false);
    if (requested_boundary) {
	std::map<int, std::vector<size_t>> adjacency;
	for (size_t edge = 0; edge < boundary.size(); ++edge) {
	    adjacency[boundary[edge].from].push_back(edge);
	    adjacency[boundary[edge].to].push_back(edge);
	}
	for (const auto &vertex : adjacency) {
	    if (vertex.second.size() != 2)
		return reject_boundary("an undirected boundary vertex does not "
		    "have degree two");
	}
	for (size_t first_edge = 0; first_edge < boundary.size(); ++first_edge) {
	    if (used[first_edge])
		continue;
	    std::vector<int> ring;
	    int current = boundary[first_edge].from;
	    const int start = current;
	    for (size_t count = 0; count < boundary.size(); ++count) {
		const std::vector<size_t> &incident = adjacency[current];
		const size_t edge = !used[incident[0]] ? incident[0] :
		    (!used[incident[1]] ? incident[1] : boundary.size());
		if (edge >= boundary.size())
		    break;
		used[edge] = true;
		ring.push_back(current);
		current = boundary[edge].from == current ?
		    boundary[edge].to : boundary[edge].from;
		if (current == start)
		    break;
	    }
	    if (current != start || ring.size() < 3)
		return reject_boundary("an undirected boundary chain is not a "
		    "closed ring");
	    rings.push_back(std::move(ring));
	}
    } else {
	std::map<int, size_t> outgoing;
	std::map<int, size_t> incoming;
	for (size_t edge = 0; edge < boundary.size(); ++edge) {
	    if (!outgoing.emplace(boundary[edge].from, edge).second ||
		    !incoming.emplace(boundary[edge].to, edge).second)
		return reject_boundary("a directed boundary vertex is branched");
	}
	if (outgoing.size() != boundary.size() ||
		incoming.size() != boundary.size())
	    return reject_boundary("directed boundary incidence is incomplete");
	for (const auto &vertex : outgoing) {
	    if (incoming.find(vertex.first) == incoming.end())
		return reject_boundary("a directed boundary vertex has no "
		    "incoming edge");
	}
	for (size_t first_edge = 0; first_edge < boundary.size(); ++first_edge) {
	    if (used[first_edge])
		continue;
	    std::vector<int> ring;
	    int current = boundary[first_edge].from;
	    const int start = current;
	    for (size_t count = 0; count < boundary.size(); ++count) {
		const auto found = outgoing.find(current);
		if (found == outgoing.end() || used[found->second])
		    break;
		used[found->second] = true;
		ring.push_back(current);
		current = boundary[found->second].to;
		if (current == start)
		    break;
	    }
	    if (current != start || ring.size() < 3)
		return reject_boundary("a directed boundary chain is not a "
		    "closed ring");
	    rings.push_back(std::move(ring));
	}
    }
    if (rings.empty() ||
	    std::find(used.begin(), used.end(), false) != used.end())
	return reject_boundary("not every retained boundary edge belongs to a "
	    "closed ring");

    const fastf_t *input_vertices = *vertices;
    if (debug_topology && requested_boundary) {
	for (size_t ring_index = 0; ring_index < rings.size(); ++ring_index) {
	    double perimeter = 0.0;
	    ON_BoundingBox bbox;
	    for (size_t point = 0; point < rings[ring_index].size(); ++point) {
		const ON_3dPoint first(&input_vertices[(size_t)
		    rings[ring_index][point] * 3]);
		const ON_3dPoint second(&input_vertices[(size_t)
		    rings[ring_index][(point + 1) % rings[ring_index].size()] * 3]);
		perimeter += first.DistanceTo(second);
		bbox.Set(first, true);
	    }
	    bu_log("Face %d: mesh ring %zu has %zu vertices, perimeter %.17g, "
		"bbox diagonal %.17g\n", source_face, ring_index,
		rings[ring_index].size(), perimeter,
		bbox.IsValid() ? bbox.Diagonal().Length() : -1.0);
	}
    }
    double approximate_face_area = 0.0;
    int replaced_face_count = 0;
    for (int face = rigorous_face_count; face < *face_count; ++face) {
	if (source_faces[(size_t)face] != source_face)
	    continue;
	replaced_face_count++;
	const ON_3dPoint first(&input_vertices[(size_t)
	    (*faces)[(size_t)face * 3] * 3]);
	const ON_3dPoint second(&input_vertices[(size_t)
	    (*faces)[(size_t)face * 3 + 1] * 3]);
	const ON_3dPoint third(&input_vertices[(size_t)
	    (*faces)[(size_t)face * 3 + 2] * 3]);
	approximate_face_area += 0.5 *
	    ON_CrossProduct(second - first, third - first).Length();
    }
    if (!replaced_face_count)
	return false;
    const std::vector<int> &ring = rings.front();
    const ON_BrepFace &brep_face = s_cdt->orig_brep->m_F[source_face];
    const ON_Surface *surface = brep_face.SurfaceOf();
    std::vector<int> patch_faces;
    std::vector<fastf_t> added_vertices;
    double center_distance = 0.0;
    bool inferred_topology = false;
    if (rings.size() == 1) {
	ON_3dPoint average(0.0, 0.0, 0.0);
	for (int vertex : ring)
	    average += ON_3dPoint(&input_vertices[(size_t)vertex * 3]);
	average /= (double)ring.size();
	std::vector<ON_3dPoint> center_candidates;
	center_candidates.push_back(average);
	if (surface) {
	    const ON_Interval u = surface->Domain(0);
	    const ON_Interval v = surface->Domain(1);
	    const double fractions[3] = {0.5, 0.25, 0.75};
	    for (double uf : fractions) {
		for (double vf : fractions) {
		    const ON_3dPoint point = surface->PointAt(
			(1.0 - uf) * u.Min() + uf * u.Max(),
			(1.0 - vf) * v.Min() + vf * v.Max());
		    if (point.IsValid())
			center_candidates.push_back(point);
		}
	    }
	}
	const auto valid_center = [&](const ON_3dPoint &center) {
	    if (!center.IsValid())
		return false;
	    for (size_t point = 0; point < ring.size(); ++point) {
		const ON_3dPoint first(&input_vertices[
		    (size_t)ring[point] * 3]);
		const ON_3dPoint second(&input_vertices[
		    (size_t)ring[(point + 1) % ring.size()] * 3]);
		const ON_3dVector edge = second - first;
		const ON_3dVector radial = center - first;
		const double longest_squared = std::max(edge.LengthSquared(),
		    std::max(radial.LengthSquared(),
			center.DistanceToSquared(second)));
		if (!(longest_squared > 0.0) ||
			ON_CrossProduct(edge, radial).Length() <= 64.0 *
			std::numeric_limits<double>::epsilon() *
			longest_squared)
		    return false;
	    }
	    return true;
	};
	ON_3dPoint center = ON_3dPoint::UnsetPoint;
	center_distance = std::numeric_limits<double>::infinity();
	for (const ON_3dPoint &candidate : center_candidates) {
	    if (!valid_center(candidate))
		continue;
	    const double distance = candidate.DistanceTo(average);
	    if (distance < center_distance) {
		center = candidate;
		center_distance = distance;
	    }
	}
	if (!center.IsValid()) {
	    if (debug_topology)
		bu_log("Rigorous-boundary face repair rejected: %zu-edge "
		    "ring has no nondegenerate interior candidate\n",
		    ring.size());
	    return false;
	}
	const int center_index = *vertex_count;
	added_vertices = {center.x, center.y, center.z};
	for (size_t point = 0; point < ring.size(); ++point) {
	    patch_faces.push_back(ring[point]);
	    patch_faces.push_back(ring[(point + 1) % ring.size()]);
	    patch_faces.push_back(center_index);
	}
    } else {
	/* Every chart below is used only to choose triangle connectivity.  The
	 * output points are always the exact 3-D vertices supplied by rigorous
	 * neighboring faces. */
	const auto triangulate_chart = [&](const std::vector<detria::PointD>
		&points, const std::vector<std::vector<int>> &contours,
		const std::vector<int> &point_vertices, bool log_chart) {
	    if (points.size() != boundary.size() || contours.size() < 2)
		return false;
	    if (point_vertices.size() != points.size())
		return false;
	    size_t outline = 0;
	    double largest_area = 0.0;
	    std::vector<double> signed_areas(contours.size(), 0.0);
	    for (size_t contour = 0; contour < contours.size(); ++contour) {
		for (size_t point = 0; point < contours[contour].size(); ++point) {
		    const detria::PointD &first = points[(size_t)
			contours[contour][point]];
		    const detria::PointD &second = points[(size_t)
			contours[contour][(point + 1) %
			    contours[contour].size()]];
		    signed_areas[contour] += first.x * second.y -
			second.x * first.y;
		}
		if (std::fabs(signed_areas[contour]) > largest_area) {
		    largest_area = std::fabs(signed_areas[contour]);
		    outline = contour;
		}
	    }
	    if (!(largest_area > 0.0))
		return false;
	    if (debug_topology && log_chart) {
		bu_log("Rigorous-boundary chart: %zu points in %zu rings; "
		    "outline %zu, signed areas", points.size(), contours.size(),
		    outline);
		for (double area : signed_areas)
		    bu_log(" %.17g", 0.5 * area);
		bu_log("\n");
	    }
	    std::vector<std::vector<int>> oriented = contours;
	    if (signed_areas[outline] < 0.0)
		std::reverse(oriented[outline].begin(),
		    oriented[outline].end());
	    for (size_t contour = 0; contour < oriented.size(); ++contour) {
		if (contour != outline && signed_areas[contour] > 0.0)
		    std::reverse(oriented[contour].begin(),
			oriented[contour].end());
	    }
	    detria::Triangulation<detria::PointD, int> triangulation;
	    triangulation.setPoints(points);
	    triangulation.addOutline(oriented[outline]);
	    for (size_t contour = 0; contour < oriented.size(); ++contour) {
		if (contour != outline)
		    triangulation.addHole(oriented[contour]);
	    }
	    try {
		if (!triangulation.triangulate(true)) {
		    if (debug_topology && log_chart)
			bu_log("Rigorous-boundary chart rejected by detria: "
			    "%s\n", triangulation.getErrorMessage().c_str());
		    return false;
		}
	    } catch (...) {
		if (debug_topology && log_chart)
		    bu_log("Rigorous-boundary chart raised a detria "
			"exception\n");
		return false;
	    }
	    std::vector<int> result;
	    triangulation.forEachTriangle(
		[&](const detria::Triangle<int> triangle) {
		    result.push_back(point_vertices[(size_t)triangle.x]);
		    result.push_back(point_vertices[(size_t)triangle.y]);
		    result.push_back(point_vertices[(size_t)triangle.z]);
		}, true);
	    const size_t expected_faces = boundary.size() +
		2 * (contours.size() - 1) - 2;
	    if (result.size() / 3 != expected_faces)
		return false;
	    patch_faces.swap(result);
	    return true;
	};
	const auto triangulate_clean_chart = [&] (
		const std::vector<detria::PointD> &points,
		const std::vector<std::vector<int>> &contours,
		const std::vector<int> &point_vertices) {
	    if (points.size() != boundary.size() || contours.size() < 2 ||
		    point_vertices.size() != points.size())
		return false;
	    size_t outline = 0;
	    double largest_area = 0.0;
	    for (size_t contour = 0; contour < contours.size(); ++contour) {
		double area = 0.0;
		for (size_t point = 0; point < contours[contour].size(); ++point) {
		    const detria::PointD &first = points[(size_t)
			contours[contour][point]];
		    const detria::PointD &second = points[(size_t)
			contours[contour][(point + 1) %
			    contours[contour].size()]];
		    area += first.x * second.y - second.x * first.y;
		}
		if (std::fabs(area) > largest_area) {
		    largest_area = std::fabs(area);
		    outline = contour;
		}
	    }
	    if (!(largest_area > 0.0))
		return false;
	    std::vector<const int *> hole_arrays;
	    std::vector<size_t> hole_counts;
	    for (size_t contour = 0; contour < contours.size(); ++contour) {
		if (contour == outline)
		    continue;
		hole_arrays.push_back(contours[contour].data());
		hole_counts.push_back(contours[contour].size());
	    }
	    std::vector<fastf_t> input_points(points.size() * 2);
	    double uv_min[2] = {INFINITY, INFINITY};
	    double uv_max[2] = {-INFINITY, -INFINITY};
	    for (size_t point = 0; point < points.size(); ++point) {
		input_points[2 * point] = points[point].x;
		input_points[2 * point + 1] = points[point].y;
		uv_min[X] = std::min(uv_min[X], points[point].x);
		uv_min[Y] = std::min(uv_min[Y], points[point].y);
		uv_max[X] = std::max(uv_max[X], points[point].x);
		uv_max[Y] = std::max(uv_max[Y], points[point].y);
	    }
	    int *clean_faces = NULL;
	    int clean_face_count = 0;
	    point2d_t *clean_points = NULL;
	    int clean_point_count = 0;
	    std::vector<int> clean_constraints;
	    clean_constraints.reserve(boundary.size() * 2);
	    for (const std::vector<int> &contour : contours) {
		for (size_t point = 0; point < contour.size(); ++point) {
		    clean_constraints.push_back(contour[point]);
		    clean_constraints.push_back(
			contour[(point + 1) % contour.size()]);
		}
	    }
	    const int clean_result =
		bg_nested_poly_triangulate_clean_constraints(
		&clean_faces, &clean_face_count, &clean_points,
		&clean_point_count, contours[outline].data(),
		contours[outline].size(), hole_arrays.data(),
		hole_counts.data(), hole_arrays.size(), NULL, 0,
		clean_constraints.data(), boundary.size(),
		(const point2d_t *)input_points.data(), points.size());
	    if (debug_topology)
		bu_log("Rigorous-boundary Clipper cleanup returned %d with "
		    "%d points and %d triangles from %zu points\n",
		    clean_result, clean_point_count, clean_face_count,
		    points.size());
	    bool valid = clean_result == BRLCAD_OK && clean_faces &&
		clean_face_count > 0 && clean_points && clean_point_count > 0;
	    const double match_tolerance = std::max(
		(double)BREP_SAME_POINT_TOLERANCE,
		4.0 * std::max(uv_max[X] - uv_min[X],
		    uv_max[Y] - uv_min[Y]) / CLIPPER_MAX);
	    std::vector<int> clean_to_vertex((size_t)std::max(0,
		clean_point_count), -1);
	    for (int clean = 0; valid && clean < clean_point_count; ++clean) {
		int match = -1;
		for (size_t point = 0; point < points.size(); ++point) {
		    if (!NEAR_EQUAL(clean_points[clean][X], points[point].x,
			    match_tolerance) ||
			!NEAR_EQUAL(clean_points[clean][Y], points[point].y,
			    match_tolerance))
			continue;
		    if (match >= 0 && point_vertices[(size_t)match] !=
			    point_vertices[point]) {
			if (debug_topology)
			    bu_log("Rigorous-boundary Clipper point %d at "
				"%.17g,%.17g merges mesh vertices %d and %d\n",
				clean, clean_points[clean][X],
				clean_points[clean][Y],
				point_vertices[(size_t)match],
				point_vertices[point]);
			valid = false;
			break;
		    }
		    match = (int)point;
		}
		if (match < 0) {
		    if (debug_topology)
			bu_log("Rigorous-boundary Clipper introduced point %d "
			    "at %.17g,%.17g\n", clean,
			    clean_points[clean][X], clean_points[clean][Y]);
		    valid = false;
		    break;
		}
		clean_to_vertex[(size_t)clean] =
		    point_vertices[(size_t)match];
	    }
	    std::vector<int> result;
	    result.reserve(valid ? (size_t)clean_face_count * 3 : 0);
	    for (int corner = 0; valid && corner < clean_face_count * 3;
		    ++corner) {
		const int point = clean_faces[corner];
		valid = point >= 0 && point < clean_point_count &&
		    clean_to_vertex[(size_t)point] >= 0;
		if (valid)
		    result.push_back(clean_to_vertex[(size_t)point]);
	    }
	    /* Clipper deliberately removes redundant collinear contour points.
	     * They are not redundant to the neighboring rigorous triangles: each
	     * is an indexed seam vertex.  Refine a cleaned boundary chord back
	     * through every omitted original contour vertex before the 3-D solid
	     * transaction is evaluated. */
	    std::vector<std::vector<int>> original_contours(contours.size());
	    for (size_t contour = 0; valid && contour < contours.size();
		    ++contour) {
		for (int point : contours[contour])
		    original_contours[contour].push_back(
			point_vertices[(size_t)point]);
	    }
	    typedef std::pair<int, int> chart_edge;
	    for (size_t refinement = 0; valid && refinement < boundary.size();
		    ++refinement) {
		std::map<chart_edge, std::vector<std::pair<int, int>>> uses;
		std::set<int> used_vertices;
		for (size_t face = 0; face < result.size() / 3; ++face) {
		    for (int corner = 0; corner < 3; ++corner) {
			const int from = result[face * 3 + (size_t)corner];
			const int to = result[face * 3 +
			    (size_t)((corner + 1) % 3)];
			uses[std::minmax(from, to)].push_back(
			    {(int)face, corner});
			used_vertices.insert(from);
		    }
		}
		bool refined = false;
		for (const auto &edge : uses) {
		    if (edge.second.size() != 1 || refined)
			continue;
		    const int face = edge.second.front().first;
		    const int corner = edge.second.front().second;
		    const int from = result[(size_t)face * 3 + corner];
		    const int to = result[(size_t)face * 3 +
			(corner + 1) % 3];
		    std::vector<int> selected_path;
		    size_t candidate_paths = 0;
		    for (const std::vector<int> &contour : original_contours) {
			auto first = std::find(contour.begin(), contour.end(), from);
			auto second = std::find(contour.begin(), contour.end(), to);
			if (first == contour.end() || second == contour.end())
			    continue;
			const size_t first_index = first - contour.begin();
			const size_t second_index = second - contour.begin();
			for (int direction : {1, -1}) {
			    std::vector<int> path(1, from);
			    size_t point = first_index;
			    while (point != second_index &&
				    path.size() <= contour.size()) {
				point = direction > 0 ?
				    (point + 1) % contour.size() :
				    (point + contour.size() - 1) %
				    contour.size();
				path.push_back(contour[point]);
			    }
			    if (path.size() <= 2 || path.back() != to)
				continue;
			    bool omitted = true;
			    for (size_t internal = 1;
				    internal + 1 < path.size(); ++internal)
				omitted = omitted && used_vertices.find(
				    path[internal]) == used_vertices.end();
			    if (!omitted)
				continue;
			    selected_path = std::move(path);
			    candidate_paths++;
			}
		    }
		    if (candidate_paths != 1)
			continue;
		    const int third = result[(size_t)face * 3 +
			(corner + 2) % 3];
		    result[(size_t)face * 3] = selected_path[0];
		    result[(size_t)face * 3 + 1] = selected_path[1];
		    result[(size_t)face * 3 + 2] = third;
		    for (size_t point = 1; point + 1 < selected_path.size();
			    ++point) {
			result.push_back(selected_path[point]);
			result.push_back(selected_path[point + 1]);
			result.push_back(third);
		    }
		    refined = true;
		}
		if (!refined)
		    break;
	    }
	    std::map<chart_edge, size_t> final_uses;
	    for (size_t face = 0; valid && face < result.size() / 3; ++face) {
		for (int corner = 0; corner < 3; ++corner) {
		    const int from = result[face * 3 + (size_t)corner];
		    const int to = result[face * 3 +
			(size_t)((corner + 1) % 3)];
		    final_uses[std::minmax(from, to)]++;
		}
	    }
	    size_t missing_constraints = 0;
	    for (const std::vector<int> &contour : original_contours) {
		for (size_t point = 0; point < contour.size(); ++point) {
		    if (final_uses[std::minmax(contour[point],
			    contour[(point + 1) % contour.size()])] != 1)
			missing_constraints++;
		}
	    }
	    valid = valid && !missing_constraints;
	    if (debug_topology && missing_constraints)
		bu_log("Rigorous-boundary Clipper result omitted %zu of %zu "
		    "exact boundary constraints\n", missing_constraints,
		    boundary.size());
	    if (clean_faces)
		bu_free(clean_faces, "rigorous-boundary clean chart faces");
	    if (clean_points)
		bu_free(clean_points, "rigorous-boundary clean chart points");
	    if (!valid)
		return false;
	    patch_faces.swap(result);
	if (debug_topology)
		bu_log("Rigorous-boundary chart accepted a Clipper-cleaned "
		    "%zu-triangle topology without moving boundary points\n",
		    patch_faces.size() / 3);
	    return true;
	};

	/* A valid surface chart is preferable to any spatial projection.  It
	 * permits nonplanar rings while retaining the exact 3-D boundary.  The
	 * closest-point parameters are used only as a triangulation chart; all
	 * output vertices remain the rigorous mesh vertices. */
	std::vector<detria::PointD> surface_points;
	std::vector<std::vector<int>> surface_rings(rings.size());
	std::vector<int> surface_to_vertex;
	size_t surface_outline = 0;
	double surface_outline_area = 0.0;
	bool surface_chart_available = false;
	if (surface) {
	    bool chart_valid = true;
	    for (size_t ring_index = 0; chart_valid &&
		    ring_index < rings.size(); ++ring_index) {
		for (int vertex : rings[ring_index]) {
		    const ON_3dPoint point(&input_vertices[(size_t)vertex * 3]);
		    ON_2dPoint uv;
		    ON_3dPoint closest;
		    double distance = DBL_MAX;
		    chart_valid = surface_GetClosestPoint3dFirstOrder(surface,
			point, uv, closest, distance, 0, ON_ZERO_TOLERANCE,
			allowed_deviation) && uv.IsValid() && closest.IsValid() &&
			std::isfinite(distance) && distance <= allowed_deviation;
		    if (!chart_valid)
			break;
		    surface_rings[ring_index].push_back(
			(int)surface_points.size());
		    surface_points.push_back({uv.x, uv.y});
		    surface_to_vertex.push_back(vertex);
		}
	    }
	    for (int direction = 0; chart_valid && direction < 2;
		    ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const ON_Interval domain = surface->Domain(direction);
		const double period = domain.Length();
		if (!(period > 0.0)) {
		    chart_valid = false;
		    break;
		}
		for (const std::vector<int> &surface_ring : surface_rings) {
		    for (size_t point = 1; point < surface_ring.size(); ++point) {
			double &coordinate = direction ?
			    surface_points[(size_t)surface_ring[point]].y :
			    surface_points[(size_t)surface_ring[point]].x;
			const double prior = direction ? surface_points[(size_t)
			    surface_ring[point - 1]].y : surface_points[(size_t)
			    surface_ring[point - 1]].x;
			coordinate += std::round((prior - coordinate) / period) *
			    period;
		    }
		}
	    }
	    for (size_t ring_index = 0; chart_valid &&
		    ring_index < surface_rings.size(); ++ring_index) {
		double area = 0.0;
		const std::vector<int> &surface_ring =
		    surface_rings[ring_index];
		for (size_t point = 0; point < surface_ring.size(); ++point) {
		    const detria::PointD &first = surface_points[(size_t)
			surface_ring[point]];
		    const detria::PointD &second = surface_points[(size_t)
			surface_ring[(point + 1) % surface_ring.size()]];
		    area += first.x * second.y - second.x * first.y;
		}
		if (std::fabs(area) > surface_outline_area) {
		    surface_outline_area = std::fabs(area);
		    surface_outline = ring_index;
		}
	    }
	    for (int direction = 0; chart_valid && direction < 2;
		    ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const double period = surface->Domain(direction).Length();
		double outline_center = 0.0;
		for (int point : surface_rings[surface_outline])
		    outline_center += direction ?
			surface_points[(size_t)point].y :
			surface_points[(size_t)point].x;
		outline_center /=
		    (double)surface_rings[surface_outline].size();
		for (size_t ring_index = 0; ring_index < surface_rings.size();
			ring_index++) {
		    if (ring_index == surface_outline)
			continue;
		    double ring_center = 0.0;
		    for (int point : surface_rings[ring_index])
			ring_center += direction ?
			    surface_points[(size_t)point].y :
			    surface_points[(size_t)point].x;
		    ring_center /= (double)surface_rings[ring_index].size();
		    const double shift = std::round((outline_center -
			ring_center) / period) * period;
		    for (int point : surface_rings[ring_index]) {
			double &coordinate = direction ?
			    surface_points[(size_t)point].y :
			    surface_points[(size_t)point].x;
			coordinate += shift;
		    }
		}
	    }
	    if (debug_topology)
		bu_log("Rigorous-boundary surface chart projection %s "
		    "(closed %d/%d)\n", chart_valid ? "succeeded" : "failed",
		    surface->IsClosed(0), surface->IsClosed(1));
	    surface_chart_available = chart_valid;
	    if (chart_valid && !triangulate_chart(surface_points,
		    surface_rings, surface_to_vertex, true))
		(void)triangulate_clean_chart(surface_points, surface_rings,
		    surface_to_vertex);
	}

	if (patch_faces.empty() && surface_chart_available && surface) {
	    for (int direction = 0; direction < 2 && patch_faces.empty();
		    ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const double period = surface->Domain(direction).Length();
		if (repair_periodic_annulus_chart(surface_points,
			surface_rings, surface_to_vertex, direction, period,
			input_vertices, *vertex_count,
			approximate_face_area, debug_topology, patch_faces))
		    inferred_topology = true;
	    }
	}

	/* If the imported parameter curves cross, preserve their useful ring
	 * placement but replace the broken metric with a simple topological
	 * disk-with-holes chart.  Ring order and every exact 3-D boundary vertex
	 * remain authoritative; only the interior connectivity is inferred.  The
	 * assembled solid and surface-deviation gates below decide whether that
	 * interpretation is geometrically defensible. */
	if (patch_faces.empty() && surface_chart_available &&
		surface_outline < surface_rings.size()) {
	    std::vector<detria::PointD> topology_points(surface_points.size());
	    std::vector<detria::PointD> centers(surface_rings.size());
	    std::vector<double> chart_areas(surface_rings.size(), 0.0);
	    std::vector<double> spatial_perimeters(surface_rings.size(), 0.0);
	    for (size_t ring_index = 0; ring_index < surface_rings.size();
		    ++ring_index) {
		const std::vector<int> &surface_ring =
		    surface_rings[ring_index];
		for (int point : surface_ring) {
		    centers[ring_index].x += surface_points[(size_t)point].x;
		    centers[ring_index].y += surface_points[(size_t)point].y;
		}
		centers[ring_index].x /= (double)surface_ring.size();
		centers[ring_index].y /= (double)surface_ring.size();
		for (size_t point = 0; point < surface_ring.size(); ++point) {
		    const detria::PointD &first = surface_points[(size_t)
			surface_ring[point]];
		    const detria::PointD &second = surface_points[(size_t)
			surface_ring[(point + 1) % surface_ring.size()]];
		    chart_areas[ring_index] += first.x * second.y -
			second.x * first.y;
		    const ON_3dPoint spatial_first(&input_vertices[(size_t)
			surface_to_vertex[(size_t)surface_ring[point]] * 3]);
		    const ON_3dPoint spatial_second(&input_vertices[(size_t)
			surface_to_vertex[(size_t)surface_ring[
			    (point + 1) % surface_ring.size()]] * 3]);
		    spatial_perimeters[ring_index] +=
			spatial_first.DistanceTo(spatial_second);
		}
		chart_areas[ring_index] =
		    0.5 * std::fabs(chart_areas[ring_index]);
	    }
	    const size_t topology_outline = (size_t)std::distance(
		spatial_perimeters.begin(), std::max_element(
		    spatial_perimeters.begin(), spatial_perimeters.end()));
	    double outer_span = 0.0;
	    for (int point : surface_rings[topology_outline]) {
		outer_span = std::max(outer_span, std::fabs(
		    surface_points[(size_t)point].x -
		    centers[topology_outline].x));
		outer_span = std::max(outer_span, std::fabs(
		    surface_points[(size_t)point].y -
		    centers[topology_outline].y));
	    }
	    const double topology_area_scale = std::max(
		chart_areas[topology_outline], outer_span * outer_span);
	    bool topology_valid = outer_span > 0.0 &&
		std::isfinite(outer_span) && topology_area_scale > 0.0;
	    std::vector<double> radii(surface_rings.size(), 0.0);
	    std::vector<detria::PointD> topology_centers(surface_rings.size());
	    for (size_t ring_index = 0; topology_valid &&
		    ring_index < surface_rings.size(); ++ring_index) {
		const bool outline = ring_index == topology_outline;
		const detria::PointD source_center = centers[ring_index];
		double radius = outline ? 1.0 : std::max(0.02,
		    std::min(0.18, 0.35 * std::sqrt(chart_areas[ring_index] /
			topology_area_scale)));
		radii[ring_index] = radius;
		detria::PointD chart_center = {0.0, 0.0};
		if (!outline) {
		    chart_center.x = (centers[ring_index].x -
			centers[topology_outline].x) / outer_span;
		    chart_center.y = (centers[ring_index].y -
			centers[topology_outline].y) / outer_span;
		    const double center_length = std::hypot(chart_center.x,
			chart_center.y);
		    const double maximum_center = 0.78 - radius;
		    if (center_length > maximum_center && center_length > 0.0) {
			chart_center.x *= maximum_center / center_length;
			chart_center.y *= maximum_center / center_length;
		    }
		}
		topology_centers[ring_index] = chart_center;
		const std::vector<int> &surface_ring =
		    surface_rings[ring_index];
		std::vector<double> lengths(surface_ring.size() + 1, 0.0);
		for (size_t point = 0; point < surface_ring.size(); ++point) {
		    const ON_3dPoint first(&input_vertices[(size_t)
			surface_to_vertex[(size_t)surface_ring[point]] * 3]);
		    const ON_3dPoint second(&input_vertices[(size_t)
			surface_to_vertex[(size_t)surface_ring[
			    (point + 1) % surface_ring.size()]] * 3]);
		    lengths[point + 1] = lengths[point] +
			first.DistanceTo(second);
		}
		if (!(lengths.back() > 0.0) || !std::isfinite(lengths.back())) {
		    topology_valid = false;
		    break;
		}
		const detria::PointD &first = surface_points[(size_t)
		    surface_ring.front()];
		const double phase = std::atan2(first.y -
		    source_center.y, first.x - source_center.x);
		const double direction = outline ? 1.0 : -1.0;
		for (size_t point = 0; point < surface_ring.size(); ++point) {
		    const double angle = phase + direction * 2.0 *
			std::acos(-1.0) *
			lengths[point] / lengths.back();
		    topology_points[(size_t)surface_ring[point]] = {
			chart_center.x + radius * std::cos(angle),
			chart_center.y + radius * std::sin(angle)};
		}
	    }
	    for (size_t first = 0; topology_valid &&
		    first < surface_rings.size(); ++first) {
		if (first == topology_outline)
		    continue;
		for (size_t second = first + 1; second < surface_rings.size();
			++second) {
		    if (second == topology_outline)
			continue;
		    topology_valid = std::hypot(topology_centers[first].x -
			topology_centers[second].x,
			topology_centers[first].y -
			topology_centers[second].y) >
			radii[first] + radii[second] + 0.02;
		    if (!topology_valid)
			break;
		}
	    }
	    std::vector<size_t> hole_rings;
	    for (size_t ring_index = 0; ring_index < surface_rings.size();
		    ++ring_index) {
		if (ring_index != topology_outline)
		    hole_rings.push_back(ring_index);
	    }
	    const size_t chart_candidates = hole_rings.size() <= 2 ? 2049 : 1;
	    std::vector<int> best_faces;
	    double best_area = 0.0;
	    double best_area_delta = std::numeric_limits<double>::infinity();
	    for (size_t candidate = 0; topology_valid &&
		    candidate < chart_candidates; ++candidate) {
		std::vector<detria::PointD> rotated = topology_points;
		std::vector<detria::PointD> candidate_centers =
		    topology_centers;
		std::vector<double> phase_angles(hole_rings.size(), 0.0);
		for (size_t hole = 0; hole < hole_rings.size(); ++hole) {
		    const size_t ring_index = hole_rings[hole];
		    if (candidate) {
			unsigned long long bits = (unsigned long long)candidate +
			    0x9e3779b97f4a7c15ULL * (hole + 1);
			bits = (bits ^ (bits >> 30)) *
			    0xbf58476d1ce4e5b9ULL;
			bits = (bits ^ (bits >> 27)) *
			    0x94d049bb133111ebULL;
			bits ^= bits >> 31;
			const double direction = 2.0 * std::acos(-1.0) *
			    (double)(bits & 0xffffULL) / 65536.0;
			const double distance = 0.12 + 0.54 *
			    (double)((bits >> 16) & 0xffffULL) / 65535.0;
			candidate_centers[ring_index] = {
			    distance * std::cos(direction),
			    distance * std::sin(direction)};
			phase_angles[hole] = 2.0 * std::acos(-1.0) *
			    (double)((bits >> 32) & 0xffffULL) / 65536.0;
		    }
		}
		bool separated = true;
		for (size_t first = 0; separated &&
			hole_rings.size() > 1 && first + 1 < hole_rings.size();
			++first) {
		    for (size_t second = first + 1;
			    second < hole_rings.size(); ++second) {
			const size_t first_ring = hole_rings[first];
			const size_t second_ring = hole_rings[second];
			separated = std::hypot(
			    candidate_centers[first_ring].x -
				candidate_centers[second_ring].x,
			    candidate_centers[first_ring].y -
				candidate_centers[second_ring].y) >
			    radii[first_ring] + radii[second_ring] + 0.02;
			if (!separated)
			    break;
		    }
		}
		if (!separated)
		    continue;
		for (size_t hole = 0; hole < hole_rings.size(); ++hole) {
		    const size_t ring_index = hole_rings[hole];
		    const double angle = phase_angles[hole];
		    const double cosine = std::cos(angle);
		    const double sine = std::sin(angle);
		    for (int point : surface_rings[ring_index]) {
			const double x = rotated[(size_t)point].x -
			    topology_centers[ring_index].x;
			const double y = rotated[(size_t)point].y -
			    topology_centers[ring_index].y;
			rotated[(size_t)point].x =
			    candidate_centers[ring_index].x +
			    cosine * x - sine * y;
			rotated[(size_t)point].y =
			    candidate_centers[ring_index].y +
			    sine * x + cosine * y;
		    }
		}
		patch_faces.clear();
		if (!triangulate_chart(rotated, surface_rings,
			surface_to_vertex, false))
		    continue;
		double patch_area = 0.0;
		bool nondegenerate = true;
		for (size_t face = 0; face < patch_faces.size() / 3; ++face) {
		    const ON_3dPoint first(&input_vertices[(size_t)
			patch_faces[face * 3] * 3]);
		    const ON_3dPoint second(&input_vertices[(size_t)
			patch_faces[face * 3 + 1] * 3]);
		    const ON_3dPoint third(&input_vertices[(size_t)
			patch_faces[face * 3 + 2] * 3]);
		    const ON_3dVector first_edge = second - first;
		    const ON_3dVector second_edge = third - first;
		    const double longest_squared = std::max(
			first_edge.LengthSquared(), std::max(
			    second_edge.LengthSquared(),
			    third.DistanceToSquared(second)));
		    const double doubled_area =
			ON_CrossProduct(first_edge, second_edge).Length();
		    nondegenerate = longest_squared > 0.0 &&
			std::isfinite(doubled_area) && doubled_area > 64.0 *
			std::numeric_limits<double>::epsilon() * longest_squared;
		    if (!nondegenerate)
			break;
		    patch_area += 0.5 * doubled_area;
		}
		if (!nondegenerate)
		    continue;
		const double area_delta =
		    std::fabs(patch_area - approximate_face_area);
		if (area_delta < best_area_delta) {
		    best_faces = patch_faces;
		    best_area = patch_area;
		    best_area_delta = area_delta;
		}
		if (best_area_delta <= 0.01 * approximate_face_area)
		    break;
	    }
	    const double area_scale = std::max(approximate_face_area,
		std::numeric_limits<double>::min());
	    /* The approximate face may omit most of a crossed multi-ring region,
	     * so admit exact-boundary candidates up to five times its area.  This
	     * only bounds candidate generation: the complete repaired mesh must
	     * still pass the caller's area, deviation, reverse-coverage, and
	     * solid-validation limits before it can be returned.
	     */
	    const double candidate_area_ratio_limit = 5.0;
	    const bool area_supported = std::isfinite(best_area_delta) &&
		best_area_delta <= (candidate_area_ratio_limit - 1.0) * area_scale;
	    if (area_supported)
		patch_faces.swap(best_faces);
	    else
		patch_faces.clear();
	    inferred_topology = !patch_faces.empty();
	    if (!patch_faces.empty() && debug_topology)
		bu_log("Rigorous-boundary repair inferred a disk-with-holes "
		    "chart for %zu exact rings; selected area %.17g from "
		    "%zu chart candidates (reference %.17g)\n", rings.size(),
		    best_area, chart_candidates, approximate_face_area);
	    else if (debug_topology && !best_faces.empty())
		bu_log("Rigorous-boundary disk-with-holes chart rejected: "
		    "area %.17g differs from reference %.17g by %.3f%%\n",
		    best_area, approximate_face_area,
		    100.0 * best_area_delta / area_scale);
	}

	if (patch_faces.empty()) {
	ON_3dPoint average(0.0, 0.0, 0.0);
	double coordinate_scale = 1.0;
	for (const std::vector<int> &boundary_ring : rings) {
	    for (int vertex : boundary_ring) {
		const ON_3dPoint point(&input_vertices[(size_t)vertex * 3]);
		average += point;
		for (int axis = 0; axis < 3; ++axis)
		    coordinate_scale = std::max(coordinate_scale,
			std::fabs(point[axis]));
	    }
	}
	average /= (double)boundary.size();
	std::vector<ON_3dVector> ring_areas(rings.size(),
	    ON_3dVector(0.0, 0.0, 0.0));
	size_t outer_ring = 0;
	for (size_t ring_index = 0; ring_index < rings.size(); ++ring_index) {
	    const std::vector<int> &boundary_ring = rings[ring_index];
	    ON_3dPoint ring_center(0.0, 0.0, 0.0);
	    for (int vertex : boundary_ring)
		ring_center += ON_3dPoint(
		    &input_vertices[(size_t)vertex * 3]);
	    ring_center /= (double)boundary_ring.size();
	    for (size_t point = 0; point < boundary_ring.size(); ++point) {
		const ON_3dPoint first(&input_vertices[
		    (size_t)boundary_ring[point] * 3]);
		const ON_3dPoint second(&input_vertices[(size_t)boundary_ring[
		    (point + 1) % boundary_ring.size()] * 3]);
		ring_areas[ring_index] += ON_CrossProduct(first - ring_center,
		    second - ring_center);
	    }
	    if (ring_areas[ring_index].LengthSquared() >
		    ring_areas[outer_ring].LengthSquared())
		outer_ring = ring_index;
	}
	ON_3dVector normal = ring_areas[outer_ring];
	if (!normal.Unitize())
	    return false;
	ON_3dVector xaxis(0.0, 0.0, 0.0);
	for (const std::vector<int> &boundary_ring : rings) {
	    for (size_t point = 0; point < boundary_ring.size(); ++point) {
		ON_3dVector edge = ON_3dPoint(&input_vertices[(size_t)
		    boundary_ring[(point + 1) % boundary_ring.size()] * 3]) -
		    ON_3dPoint(&input_vertices[(size_t)boundary_ring[point] * 3]);
		edge -= (edge * normal) * normal;
		if (edge.LengthSquared() > xaxis.LengthSquared())
		    xaxis = edge;
	    }
	}
	if (!xaxis.Unitize())
	    return false;
	ON_3dVector yaxis = ON_CrossProduct(normal, xaxis);
	if (!yaxis.Unitize())
	    return false;
	double patch_scale = 0.0;
	for (const std::vector<int> &boundary_ring : rings) {
	    for (int vertex : boundary_ring)
		patch_scale = std::max(patch_scale, average.DistanceTo(
		    ON_3dPoint(&input_vertices[(size_t)vertex * 3])));
	}
	const double plane_tolerance = std::max(
	    4096.0 * std::numeric_limits<double>::epsilon() *
		coordinate_scale,
	    std::min(0.05 * allowed_deviation, 1.0e-4 * patch_scale));
	std::vector<detria::PointD> chart_points;
	std::vector<std::vector<int>> chart_rings(rings.size());
	std::vector<int> chart_to_vertex;
	for (size_t ring_index = 0; ring_index < rings.size(); ++ring_index) {
	    for (int vertex : rings[ring_index]) {
		const ON_3dPoint point(&input_vertices[(size_t)vertex * 3]);
		if (std::fabs((point - average) * normal) > plane_tolerance) {
		    if (debug_topology)
			bu_log("Rigorous-boundary face repair rejected: %zu "
			    "rings are nonplanar beyond %.17g\n", rings.size(),
			    plane_tolerance);
		    return false;
		}
		chart_rings[ring_index].push_back((int)chart_points.size());
		chart_points.push_back({(point - average) * xaxis,
		    (point - average) * yaxis});
		chart_to_vertex.push_back(vertex);
	    }
	}
	if (!triangulate_chart(chart_points, chart_rings, chart_to_vertex,
		true))
	    return false;
    }
    }

    std::vector<int> candidate_faces;
    std::vector<int> candidate_sources;
    candidate_faces.reserve((size_t)(*face_count - replaced_face_count) *
	3 + patch_faces.size());
    candidate_sources.reserve((size_t)(*face_count - replaced_face_count) +
	patch_faces.size() / 3);
    for (int face = 0; face < *face_count; ++face) {
	if (face >= rigorous_face_count &&
		source_faces[(size_t)face] == source_face)
	    continue;
	candidate_faces.insert(candidate_faces.end(),
	    &(*faces)[(size_t)face * 3],
	    &(*faces)[(size_t)face * 3] + 3);
	candidate_sources.push_back(source_faces[(size_t)face]);
    }
    std::vector<fastf_t> candidate_vertices(*vertices,
	*vertices + (size_t)*vertex_count * 3);
    candidate_vertices.insert(candidate_vertices.end(),
	added_vertices.begin(), added_vertices.end());
    candidate_faces.insert(candidate_faces.end(), patch_faces.begin(),
	patch_faces.end());
    candidate_sources.insert(candidate_sources.end(),
	patch_faces.size() / 3, source_face);

    std::vector<int> synchronized_faces(candidate_faces.size());
    if (bg_trimesh_sync(synchronized_faces.data(), candidate_faces.data(),
	    (int)(candidate_faces.size() / 3)) >= 0)
	candidate_faces.swap(synchronized_faces);
    std::vector<int> remap(candidate_vertices.size() / 3, -1);
    int compact_count = 0;
    for (int &vertex : candidate_faces) {
	if (vertex < 0 || (size_t)vertex >= remap.size())
	    return false;
	if (remap[(size_t)vertex] < 0)
	    remap[(size_t)vertex] = compact_count++;
	vertex = remap[(size_t)vertex];
    }
    std::vector<fastf_t> compact_vertices((size_t)compact_count * 3);
    for (size_t old = 0; old < remap.size(); ++old) {
	if (remap[old] >= 0)
	    memcpy(&compact_vertices[(size_t)remap[old] * 3],
		&candidate_vertices[old * 3], 3 * sizeof(fastf_t));
    }
    assembled_mesh_validation validation;
    (void)assembled_mesh_validate(compact_count,
	(int)(candidate_faces.size() / 3), compact_vertices.data(),
	candidate_faces.data(), &validation, false);
    struct bg_trimesh_solid_errors solid_errors =
	BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    const int solid_result = bg_trimesh_solid2(compact_count,
	(int)(candidate_faces.size() / 3), compact_vertices.data(),
	candidate_faces.data(), &solid_errors);
    size_t deferred_degenerate_faces = 0;
    if (!require_solid && validation.degenerate_faces) {
	for (size_t face = 0; face < candidate_sources.size(); ++face) {
	    if (candidate_sources[face] < 0 ||
		    candidate_sources[face] == source_face)
		continue;
	    ON_3dPoint points[3];
	    for (int corner = 0; corner < 3; ++corner) {
		const int vertex = candidate_faces[3 * face + corner];
		points[corner] = ON_3dPoint(
		    &compact_vertices[(size_t)vertex * 3]);
	    }
	    if (assembled_mesh_triangle_degenerate(points))
		++deferred_degenerate_faces;
	}
    }
    if (validation.invalid_indices || validation.nonfinite_vertices ||
	    validation.unused_vertices ||
	    validation.degenerate_faces > deferred_degenerate_faces ||
	    (require_solid && validation.invalid_vertex_links) ||
	    (require_solid && solid_result)) {
	if (debug_topology)
	    bu_log("Rigorous-boundary face repair rejected after assembly: "
		"boundary %zu, degenerate %zu, invalid links %zu, "
		"unmatched %d, excess %d, misoriented %d\n",
		ring.size(), validation.degenerate_faces,
		validation.invalid_vertex_links, solid_errors.unmatched.count,
		solid_errors.excess.count, solid_errors.misoriented.count);
	bg_free_trimesh_solid_errors(&solid_errors);
	return false;
    }
    bg_free_trimesh_solid_errors(&solid_errors);

    int *new_faces = (int *)bu_malloc(candidate_faces.size() * sizeof(int),
	"rigorous-boundary repaired faces");
    fastf_t *new_vertices = (fastf_t *)bu_malloc(
	compact_vertices.size() * sizeof(fastf_t),
	"rigorous-boundary repaired vertices");
    memcpy(new_faces, candidate_faces.data(),
	candidate_faces.size() * sizeof(int));
    memcpy(new_vertices, compact_vertices.data(),
	compact_vertices.size() * sizeof(fastf_t));
    bu_free(*faces, "pre-rigorous-boundary repair faces");
    bu_free(*vertices, "pre-rigorous-boundary repair vertices");
    stats->components = 1;
    stats->removed_faces = replaced_face_count;
    stats->added_faces = (int)(patch_faces.size() / 3);
    stats->boundary_edges = (int)boundary.size();
    stats->max_center_offset = center_distance;
    stats->inferred_topology = inferred_topology;
    *faces = new_faces;
    *face_count = (int)(candidate_faces.size() / 3);
    *vertices = new_vertices;
    *vertex_count = compact_count;
    source_faces.swap(candidate_sources);
    stats->source_faces.insert(source_face);
    if (vertex_remap)
	vertex_remap->assign(remap.begin(), remap.end());
    return true;
}

static int repair_near_boundary_pair_contract(void);
static int repair_adaptive_hole_retry_contract(void);
static int repair_weld_roundoff_contract(void);
static bool repair_degenerate_approximate_neighborhoods(int **, int *,
    fastf_t **, int *, int, std::vector<int> &, double, size_t,
    repair_degenerate_neighborhood_stats *);

static int
repair_degenerate_neighborhood_contract(void)
{
    const fastf_t input_vertices[8][3] = {
	{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0}, {0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0},
	{1.0, 1.0, 1.0}, {0.0, 1.0, 1.0}
    };
    const int input_faces[14][3] = {
	{0, 2, 1}, {0, 3, 2},
	{0, 1, 5}, {0, 5, 4},
	{1, 2, 6}, {1, 6, 5},
	{2, 3, 7}, {2, 7, 6},
	{3, 0, 4}, {3, 4, 7},
	{4, 5, 6}, {4, 6, 7},
	/* A bad fallback can repeat the same collapsed sliver.  Its sole
	 * surviving edge is already paired by the sound top and side faces. */
	{4, 5, 4}, {4, 5, 4}
    };
    int face_count = 14;
    int vertex_count = 8;
    int *faces = (int *)bu_malloc(sizeof(input_faces),
	"degenerate neighborhood contract faces");
    fastf_t *vertices = (fastf_t *)bu_malloc(sizeof(input_vertices),
	"degenerate neighborhood contract vertices");
    memcpy(faces, input_faces, sizeof(input_faces));
    memcpy(vertices, input_vertices, sizeof(input_vertices));
    std::vector<int> sources(14, -1);
    for (int face = 10; face < 14; ++face)
	sources[(size_t)face] = 0;
    repair_degenerate_neighborhood_stats stats;
    const bool repaired = repair_degenerate_approximate_neighborhoods(
	&faces, &face_count, &vertices, &vertex_count, 10, sources, 0.1,
	16, &stats);
    assembled_mesh_validation validation;
    const bool valid = repaired && face_count == 12 && vertex_count == 8 &&
	stats.components == 1 && stats.removed_faces == 2 &&
	stats.added_faces == 0 && stats.source_faces == std::set<int>({0}) &&
	sources.size() == 12 && assembled_mesh_validate(vertex_count,
	face_count, vertices, faces, &validation, false) &&
	!validation.degenerate_faces && !validation.invalid_vertex_links &&
	!bg_trimesh_solid2(vertex_count, face_count, vertices, faces, NULL);
    bu_free(faces, "degenerate neighborhood contract result faces");
    bu_free(vertices, "degenerate neighborhood contract result vertices");
    if (!valid)
	return 1;

    const int growth_faces[14][3] = {
	{0, 2, 1}, {0, 3, 2},
	{0, 1, 5}, {0, 5, 4},
	{1, 2, 6}, {1, 6, 5},
	{2, 3, 7}, {2, 7, 6},
	{3, 0, 4}, {3, 4, 7},
	{4, 5, 6}, {4, 6, 7},
	/* The duplicate triangle makes the shared diagonal singular.  Absorb
	 * that approximate edge star and stop at the four rigorous side edges. */
	{4, 5, 6}, {4, 6, 4}
    };
    face_count = 14;
    vertex_count = 8;
    faces = (int *)bu_malloc(sizeof(growth_faces),
	"growing neighborhood contract faces");
    vertices = (fastf_t *)bu_malloc(sizeof(input_vertices),
	"growing neighborhood contract vertices");
    memcpy(faces, growth_faces, sizeof(growth_faces));
    memcpy(vertices, input_vertices, sizeof(input_vertices));
    sources.assign(14, -1);
    for (int face = 10; face < 14; ++face)
	sources[(size_t)face] = 0;
    stats = repair_degenerate_neighborhood_stats();
    const bool grown = repair_degenerate_approximate_neighborhoods(&faces,
	&face_count, &vertices, &vertex_count, 10, sources, 0.1, 16,
	&stats);
    validation = assembled_mesh_validation();
    const bool grown_valid = grown && face_count == 14 &&
	vertex_count == 9 && stats.components == 1 &&
	stats.removed_faces == 4 && stats.added_faces == 4 &&
	stats.source_faces == std::set<int>({0}) && sources.size() == 14 &&
	assembled_mesh_validate(vertex_count, face_count, vertices, faces,
	    &validation, false) && !validation.degenerate_faces &&
	!validation.invalid_vertex_links &&
	!bg_trimesh_solid2(vertex_count, face_count, vertices, faces, NULL);
    bu_free(faces, "growing neighborhood contract result faces");
    bu_free(vertices, "growing neighborhood contract result vertices");
    return grown_valid ? 0 : 2;
}

static int
repair_multi_ring_boundary_contract(struct ON_Brep_CDT_State *state)
{
    const fastf_t input_vertices[16][3] = {
	{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0}, {0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0},
	{1.0, 1.0, 1.0}, {0.0, 1.0, 1.0},
	{0.3, 0.3, 0.0}, {0.7, 0.3, 0.0},
	{0.7, 0.7, 0.0}, {0.3, 0.7, 0.0},
	{0.3, 0.3, 1.0}, {0.7, 0.3, 1.0},
	{0.7, 0.7, 1.0}, {0.3, 0.7, 1.0}
    };
    const int input_faces[25][3] = {
	/* Bottom annulus. */
	{0, 9, 1}, {0, 8, 9}, {1, 10, 2}, {1, 9, 10},
	{2, 11, 3}, {2, 10, 11}, {3, 8, 0}, {3, 11, 8},
	/* Outer sides. */
	{0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5},
	{2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7},
	/* Inner sides, oriented toward the through-hole. */
	{8, 13, 9}, {8, 12, 13}, {9, 14, 10}, {9, 13, 14},
	{10, 15, 11}, {10, 14, 15}, {11, 12, 8}, {11, 15, 12},
	/* Untrusted partial top face. */
	{4, 5, 6}
    };
    int face_count = 25;
    int vertex_count = 16;
    int *faces = (int *)bu_malloc(sizeof(input_faces),
	"multi-ring boundary test faces");
    fastf_t *vertices = (fastf_t *)bu_malloc(sizeof(input_vertices),
	"multi-ring boundary test vertices");
    memcpy(faces, input_faces, sizeof(input_faces));
    memcpy(vertices, input_vertices, sizeof(input_vertices));
    std::vector<int> sources(25, -1);
    sources[24] = 0;
    repair_degenerate_neighborhood_stats stats;
    const bool repaired = repair_failed_face_from_rigorous_boundary(state,
	&faces, &face_count, &vertices, &vertex_count, 24, sources, 0.1,
	16, &stats);
    assembled_mesh_validation validation;
    const bool valid = repaired && face_count == 32 && vertex_count == 16 &&
	stats.removed_faces == 1 && stats.added_faces == 8 &&
	stats.boundary_edges == 8 && !stats.inferred_topology &&
	sources.size() == 32 &&
	assembled_mesh_validate(vertex_count, face_count, vertices, faces,
	    &validation, false) && !validation.degenerate_faces &&
	!validation.invalid_vertex_links &&
	!bg_trimesh_solid2(vertex_count, face_count, vertices, faces, NULL);
    bu_free(faces, "multi-ring boundary test result faces");
    bu_free(vertices, "multi-ring boundary test result vertices");
    if (!valid)
	return 1;

    /* A nonplanar ring may cross in the source surface's projection even
     * though its 3-D edges do not intersect.  Exercise the bounded abstract
     * chart used when neither that projection nor a best-fit plane is a
     * usable triangulation domain. */
    fastf_t twisted_vertices[16][3];
    memcpy(twisted_vertices, input_vertices, sizeof(twisted_vertices));
    VSET(twisted_vertices[4], 0.0, 0.0, 1.0);
    VSET(twisted_vertices[5], 1.0, 1.0, 1.0);
    VSET(twisted_vertices[6], 0.0, 1.0, 2.0);
    VSET(twisted_vertices[7], 1.0, 0.0, 2.0);
    const int twisted_input_faces[32][3] = {
	{0, 9, 1}, {0, 8, 9}, {1, 10, 2}, {1, 9, 10},
	{2, 11, 3}, {2, 10, 11}, {3, 8, 0}, {3, 11, 8},
	{0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5},
	{2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7},
	{8, 13, 9}, {8, 12, 13}, {9, 14, 10}, {9, 13, 14},
	{10, 15, 11}, {10, 14, 15}, {11, 12, 8}, {11, 15, 12},
	/* Supply only an area reference.  The crossing spatial ring still
	 * requires the inferred chart to choose replacement connectivity. */
	{4, 5, 13}, {4, 13, 12}, {5, 6, 14}, {5, 14, 13},
	{6, 7, 15}, {6, 15, 14}, {7, 4, 12}, {7, 12, 15}
    };
    face_count = 32;
    vertex_count = 16;
    faces = (int *)bu_malloc(sizeof(twisted_input_faces),
	"twisted multi-ring boundary test faces");
    vertices = (fastf_t *)bu_malloc(sizeof(twisted_vertices),
	"twisted multi-ring boundary test vertices");
    memcpy(faces, twisted_input_faces, sizeof(twisted_input_faces));
    memcpy(vertices, twisted_vertices, sizeof(twisted_vertices));
    sources.assign(32, -1);
    std::fill(sources.begin() + 24, sources.end(), 5);
    stats = repair_degenerate_neighborhood_stats();
    const bool twisted_repaired = repair_failed_face_from_rigorous_boundary(
	state, &faces, &face_count, &vertices, &vertex_count, 24, sources,
	10.0, 16, &stats);
    validation = assembled_mesh_validation();
    const bool twisted_valid = twisted_repaired && face_count == 32 &&
	vertex_count == 16 && stats.removed_faces == 8 &&
	stats.added_faces == 8 && stats.boundary_edges == 8 &&
	stats.inferred_topology && sources.size() == 32 &&
	assembled_mesh_validate(vertex_count, face_count, vertices, faces,
	    &validation, false) && !validation.degenerate_faces &&
	!validation.invalid_vertex_links &&
	!bg_trimesh_solid2(vertex_count, face_count, vertices, faces, NULL);
    bu_free(faces, "twisted multi-ring boundary test result faces");
    bu_free(vertices, "twisted multi-ring boundary test result vertices");
    return twisted_valid ? 0 : 2;
}

static int
repair_multi_source_boundary_contract(struct ON_Brep_CDT_State *state)
{
    const fastf_t input_vertices[8][3] = {
	{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0}, {0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0},
	{1.0, 1.0, 1.0}, {0.0, 1.0, 1.0}
    };
    const int input_faces[12][3] = {
	/* Four retained side faces. */
	{0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5},
	{2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7},
	/* Two independently untrusted caps.  The second retains only
	 * degenerate placeholders until its transaction runs. */
	{0, 2, 1}, {0, 3, 2}, {4, 5, 5}, {4, 6, 6}
    };
    int face_count = 12;
    int vertex_count = 8;
    int *faces = (int *)bu_malloc(sizeof(input_faces),
	"multi-source boundary test faces");
    fastf_t *vertices = (fastf_t *)bu_malloc(sizeof(input_vertices),
	"multi-source boundary test vertices");
    memcpy(faces, input_faces, sizeof(input_faces));
    memcpy(vertices, input_vertices, sizeof(input_vertices));
    std::vector<int> sources(12, -1);
    sources[8] = sources[9] = 0;
    sources[10] = sources[11] = 1;
    std::set<std::pair<int, int>> first_boundary = {
	{0, 1}, {1, 2}, {2, 3}, {0, 3}
    };
    std::set<std::pair<int, int>> second_boundary = {
	{4, 5}, {5, 6}, {6, 7}, {4, 7}
    };
    repair_degenerate_neighborhood_stats first_stats;
    std::vector<int> remap;
    const bool first_repaired = repair_failed_face_from_rigorous_boundary(
	state, &faces, &face_count, &vertices, &vertex_count, 8, sources,
	10.0, 16, &first_stats, 0, &first_boundary, false, &remap);
    if (first_repaired) {
	std::set<std::pair<int, int>> mapped;
	for (const std::pair<int, int> &edge : second_boundary) {
	    if ((size_t)edge.first >= remap.size() ||
		    (size_t)edge.second >= remap.size() ||
		    remap[(size_t)edge.first] < 0 ||
		    remap[(size_t)edge.second] < 0)
		continue;
	    mapped.insert(std::minmax(remap[(size_t)edge.first],
		remap[(size_t)edge.second]));
	}
	second_boundary.swap(mapped);
    }
    repair_degenerate_neighborhood_stats second_stats;
    const bool second_repaired = first_repaired &&
	repair_failed_face_from_rigorous_boundary(state, &faces, &face_count,
	    &vertices, &vertex_count, 8, sources, 10.0, 16,
	    &second_stats, 1, &second_boundary, true, NULL);
    assembled_mesh_validation validation;
    const bool valid = second_repaired && face_count == 16 &&
	vertex_count == 10 && first_stats.removed_faces == 2 &&
	first_stats.added_faces == 4 && second_stats.removed_faces == 2 &&
	second_stats.added_faces == 4 && sources.size() == 16 &&
	assembled_mesh_validate(vertex_count, face_count, vertices, faces,
	    &validation, false) && !validation.degenerate_faces &&
	!validation.invalid_vertex_links &&
	!bg_trimesh_solid2(vertex_count, face_count, vertices, faces, NULL);
    bu_free(faces, "multi-source boundary test result faces");
    bu_free(vertices, "multi-source boundary test result vertices");
    return valid ? 0 : 1;
}


static int
repair_periodic_annulus_contract(void)
{
    const size_t circle_points = 8;
    const double pi = std::acos(-1.0);
    std::vector<fastf_t> vertices;
    std::vector<detria::PointD> surface_points;
    std::vector<int> point_vertices;
    std::vector<std::vector<int>> rings(3);
    for (int level = 0; level < 2; ++level) {
	for (size_t point = 0; point < circle_points; ++point) {
	    const double angle = 2.0 * pi * (double)point /
		(double)circle_points;
	    const int index = (int)(vertices.size() / 3);
	    vertices.insert(vertices.end(), {std::cos(angle),
		std::sin(angle), (double)level});
	    surface_points.push_back({angle, (double)level});
	    point_vertices.push_back(index);
	    rings[(size_t)level].push_back(index);
	}
    }
    const double hole_chart[4][2] = {
	{0.25, 0.4}, {0.35, 0.4}, {0.35, 0.6}, {0.25, 0.6}
    };
    for (const auto &point : hole_chart) {
	const double angle = 2.0 * pi * point[0];
	const int index = (int)(vertices.size() / 3);
	vertices.insert(vertices.end(), {std::cos(angle), std::sin(angle),
	    point[1]});
	surface_points.push_back({angle, point[1]});
	point_vertices.push_back(index);
	rings[2].push_back(index);
    }
    std::vector<int> faces;
    if (!repair_periodic_annulus_chart(surface_points, rings,
	    point_vertices, 0, 2.0 * pi, vertices.data(),
	    (int)(vertices.size() / 3), 2.0 * pi, false, faces))
	return 1;
    if (faces.size() / 3 != 22)
	return 2;
    assembled_mesh_validation validation;
    (void)assembled_mesh_validate((int)(vertices.size() / 3),
	(int)(faces.size() / 3), vertices.data(), faces.data(),
	&validation, false);
    return validation.invalid_indices || validation.nonfinite_vertices ||
	validation.unused_vertices || validation.degenerate_faces ? 3 : 0;
}

int
cdt_test_repair_rigorous_boundary(void)
{
    ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0),
	ON_3dPoint(0.0, 0.0, 1.0), ON_3dPoint(1.0, 0.0, 1.0),
	ON_3dPoint(1.0, 1.0, 1.0), ON_3dPoint(0.0, 1.0, 1.0)
    };
    std::unique_ptr<ON_Brep> source(ON_BrepBox(corners));
    if (!source || source->m_F.Count() < 1)
	return 1;
    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(source.get(),
	"rigorous boundary repair contract");
    if (!state)
	return 2;

    const int input_faces[12][3] = {
	{0, 2, 1}, {0, 3, 2},
	{0, 1, 5}, {0, 5, 4},
	{1, 2, 6}, {1, 6, 5},
	{2, 3, 7}, {2, 7, 6},
	{3, 0, 4}, {3, 4, 7},
	/* The failed top face contains one collapsed triangle and an
	 * incomplete diagonal.  Neither is allowed to define the repair
	 * boundary. */
	{4, 5, 5}, {4, 6, 7}
    };
    int face_count = 12;
    int vertex_count = 8;
    int *faces = (int *)bu_malloc(sizeof(input_faces),
	"rigorous boundary test faces");
    fastf_t *vertices = (fastf_t *)bu_malloc(
	(size_t)vertex_count * 3 * sizeof(fastf_t),
	"rigorous boundary test vertices");
    memcpy(faces, input_faces, sizeof(input_faces));
    for (int vertex = 0; vertex < vertex_count; ++vertex) {
	vertices[(size_t)vertex * 3] = corners[vertex].x;
	vertices[(size_t)vertex * 3 + 1] = corners[vertex].y;
	vertices[(size_t)vertex * 3 + 2] = corners[vertex].z;
    }
    std::vector<int> sources(12, -1);
    sources[10] = 0;
    sources[11] = 0;
    repair_degenerate_neighborhood_stats stats;
    const bool repaired = repair_failed_face_from_rigorous_boundary(state,
	&faces, &face_count, &vertices, &vertex_count, 10, sources, 0.1,
	16, &stats);
    assembled_mesh_validation validation;
    const bool valid = repaired && face_count == 14 && vertex_count == 9 &&
	stats.components == 1 && stats.removed_faces == 2 &&
	stats.added_faces == 4 && sources.size() == 14 &&
	assembled_mesh_validate(vertex_count, face_count, vertices, faces,
	    &validation, false) && !validation.degenerate_faces &&
	!validation.invalid_vertex_links &&
	!bg_trimesh_solid2(vertex_count, face_count, vertices, faces, NULL);
    bu_free(faces, "rigorous boundary test result faces");
    bu_free(vertices, "rigorous boundary test result vertices");
    const int multi_ring_result = valid ?
	repair_multi_ring_boundary_contract(state) : 0;
    const int multi_source_result = valid && !multi_ring_result ?
	repair_multi_source_boundary_contract(state) : 0;
    const int periodic_annulus_result = valid && !multi_ring_result &&
	!multi_source_result ? repair_periodic_annulus_contract() : 0;
    int seam_contract_result = 0;
    const ON_Cylinder cylinder(ON_Circle(ON_xy_plane, 2.0), 5.0);
    std::unique_ptr<ON_Brep> cylinder_brep(
	ON_BrepCylinder(cylinder, true, true));
    bool found_internal_seam = false;
    bool found_shared_edge = false;
    if (!cylinder_brep) {
	seam_contract_result = 1;
    } else {
	for (int face_index = 0;
		face_index < cylinder_brep->m_F.Count(); ++face_index) {
	    const ON_BrepFace &face = cylinder_brep->m_F[face_index];
	    for (int loop_index = 0; loop_index < face.LoopCount();
		    ++loop_index) {
		const ON_BrepLoop *loop = face.Loop(loop_index);
		for (int trim_index = 0; loop &&
			trim_index < loop->TrimCount(); ++trim_index) {
		    const ON_BrepTrim *trim = loop->Trim(trim_index);
		    if (!trim || trim->m_type == ON_BrepTrim::singular)
			continue;
		    const bool internal = repair_internal_face_seam(trim, face);
		    if (trim->m_type == ON_BrepTrim::seam) {
			found_internal_seam = found_internal_seam || internal;
			seam_contract_result = internal ? seam_contract_result : 2;
		    } else if (trim->Edge() && trim->Edge()->TrimCount() == 2) {
			found_shared_edge = true;
			seam_contract_result = internal ? 3 : seam_contract_result;
		    }
		}
	    }
	}
	if (!found_internal_seam || !found_shared_edge)
	    seam_contract_result = 4;
    }
    ON_Brep_CDT_Destroy(state);
    if (!valid)
	return 3;
    if (multi_ring_result)
	return 4;
    if (multi_source_result)
	return 5;
    if (periodic_annulus_result)
	return 7;
    if (seam_contract_result)
	return 6;
    const int pair_result = repair_near_boundary_pair_contract();
    if (pair_result)
	return 10 + pair_result;
    const int neighborhood_result = repair_degenerate_neighborhood_contract();
    if (neighborhood_result)
	return 20 + neighborhood_result;
    const int weld_result = repair_weld_roundoff_contract();
    if (weld_result)
	return 30 + weld_result;
    const int adaptive_hole_result = repair_adaptive_hole_retry_contract();
    if (adaptive_hole_result)
	return 40 + adaptive_hole_result;
    /* A few flat triangles tying together hundreds of disconnected links are
     * a poor generic hole-fill candidate regardless of the total mesh size.
     * A proportionate local defect population must remain repair-eligible. */
    if (!repair_hybrid_fallback_preflight(23, 458, 256))
	return 50;
    if (repair_hybrid_fallback_preflight(236, 458, 256))
	return 51;
    if (!repair_hybrid_fallback_preflight(136, 1044, 256))
	return 52;
    if (repair_hybrid_fallback_preflight(23, 200, 256))
	return 53;
    return 0;
}

/* Pair only opposite open edges whose endpoints are locally coincident.
 * This is narrower than whole-mesh coordinate welding: distinct rigorous
 * vertices are never merged, and no edit is accepted unless every indexed
 * edge becomes two-sided. */
static bool
repair_pair_near_boundary_cracks(int *faces, int face_count,
	const fastf_t *vertices, int vertex_count, int rigorous_face_count,
	double allowed_deviation, size_t *welded_vertices)
{
    const bool debug_topology = getenv(
	"BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY") != NULL;
    if (!faces || face_count <= 0 || !vertices || vertex_count <= 0 ||
	    rigorous_face_count < 0 || rigorous_face_count > face_count ||
	    !(allowed_deviation > 0.0) || !welded_vertices)
	return false;
    *welded_vertices = 0;
    typedef std::pair<int, int> mesh_edge;
    struct directed_edge {
	int face;
	int from;
	int to;
    };
    struct pairing_candidate {
	size_t edge = 0;
	double distance = std::numeric_limits<double>::infinity();
	bool same_direction = false;
    };
    ON_BoundingBox bounds;
    for (int vertex = 0; vertex < vertex_count; ++vertex)
	bounds.Set(ON_3dPoint(&vertices[(size_t)vertex * 3]), true);
    const double scale = bounds.IsValid() ?
	std::max(1.0, bounds.Diagonal().Length()) : 1.0;
    const double weld_tolerance = std::max(
	1024.0 * std::numeric_limits<double>::epsilon() * scale,
	std::min(0.01 * allowed_deviation, 1.0e-4 * scale));
    std::vector<int> candidate(faces, faces + (size_t)face_count * 3);
    std::map<mesh_edge, std::vector<directed_edge>> incidence;
    std::set<mesh_edge> open_keys;
    size_t non_two_edges = 0;
    size_t excess_edges = 0;
    auto update_incidence_status = [&](const mesh_edge &key,
	    size_t old_uses, size_t new_uses) {
	if (old_uses != 0 && old_uses != 2)
	    non_two_edges--;
	if (old_uses > 2)
	    excess_edges--;
	if (old_uses == 1)
	    open_keys.erase(key);
	if (new_uses != 0 && new_uses != 2)
	    non_two_edges++;
	if (new_uses > 2)
	    excess_edges++;
	if (new_uses == 1)
	    open_keys.insert(key);
    };
    auto add_incidence = [&](const directed_edge &edge) {
	if (edge.from == edge.to)
	    return;
	const mesh_edge key = std::minmax(edge.from, edge.to);
	std::vector<directed_edge> &uses = incidence[key];
	const size_t old_uses = uses.size();
	uses.push_back(edge);
	update_incidence_status(key, old_uses, uses.size());
    };
    auto remove_incidence = [&](const directed_edge &edge) {
	if (edge.from == edge.to)
	    return true;
	const mesh_edge key = std::minmax(edge.from, edge.to);
	auto found = incidence.find(key);
	if (found == incidence.end())
	    return false;
	std::vector<directed_edge> &uses = found->second;
	auto use = std::find_if(uses.begin(), uses.end(),
	    [&](const directed_edge &candidate_edge) {
		return candidate_edge.face == edge.face &&
		    candidate_edge.from == edge.from &&
		    candidate_edge.to == edge.to;
	    });
	if (use == uses.end())
	    return false;
	const size_t old_uses = uses.size();
	*use = uses.back();
	uses.pop_back();
	update_incidence_status(key, old_uses, uses.size());
	if (uses.empty())
	    incidence.erase(found);
	return true;
    };
    for (int face = 0; face < face_count; ++face) {
	for (int corner = 0; corner < 3; ++corner) {
	    const int from = candidate[(size_t)face * 3 + corner];
	    const int to = candidate[(size_t)face * 3 + (corner + 1) % 3];
	    if (from < 0 || from >= vertex_count || to < 0 ||
		    to >= vertex_count)
		return false;
	    add_incidence({face, from, to});
	}
    }
    /* Only approximate faces may be reindexed.  Record their corners once so
     * a weld can update its local star without scanning every triangle or
     * reconstructing the full incidence map. */
    std::vector<std::vector<size_t>> approximate_corners(
	(size_t)vertex_count);
    for (size_t corner = (size_t)rigorous_face_count * 3;
	    corner < candidate.size(); ++corner)
	approximate_corners[(size_t)candidate[corner]].push_back(corner);
    std::vector<unsigned int> affected_generation((size_t)face_count, 0);
    unsigned int generation = 0;
    const int maximum_pairings = std::min(face_count, 256);
    for (int iteration = 0; iteration < maximum_pairings; ++iteration) {
	std::vector<directed_edge> open_edges;
	open_edges.reserve(open_keys.size());
	for (const mesh_edge &key : open_keys)
	    open_edges.push_back(incidence.find(key)->second.front());
	if (!non_two_edges) {
	    std::vector<int> synchronized(candidate.size());
	    if (bg_trimesh_sync(synchronized.data(), candidate.data(),
		    face_count) < 0) {
		if (debug_topology)
		    bu_log("Near-boundary pairing rejected: paired mesh is "
			"not orientable\n");
		return false;
	    }
	    memcpy(faces, synchronized.data(), synchronized.size() *
		sizeof(int));
	    return *welded_vertices > 0;
	}
	struct endpoint_order {
	    double x;
	    size_t edge;
	};
	std::vector<endpoint_order> ordered_endpoints;
	ordered_endpoints.reserve(open_edges.size() * 2);
	for (size_t edge = 0; edge < open_edges.size(); ++edge) {
	    ordered_endpoints.push_back({vertices[
		(size_t)open_edges[edge].from * 3], edge});
	    ordered_endpoints.push_back({vertices[
		(size_t)open_edges[edge].to * 3], edge});
	}
	std::sort(ordered_endpoints.begin(), ordered_endpoints.end(),
	    [](const endpoint_order &first, const endpoint_order &second) {
		if (first.x < second.x)
		    return true;
		if (second.x < first.x)
		    return false;
		return first.edge < second.edge;
	    });
	/* Indexed excess edges can balance an odd number of open edges before
	 * the fallback-side vertex is reassigned.  Update incidence after each
	 * pairing instead of assuming the initial open set is disjoint. */
	if (open_edges.size() < 2) {
	    if (debug_topology)
		bu_log("Near-boundary pairing rejected at iteration %d: "
		    "%zu open and %zu excess indexed edges\n", iteration,
		    open_edges.size(), excess_edges);
	    return false;
	}
	bool paired = false;
	for (size_t first = 0; first < open_edges.size() && !paired;
		first++) {
	    const ON_3dPoint first_from(&vertices[
		(size_t)open_edges[first].from * 3]);
	    const ON_3dPoint first_to(&vertices[
		(size_t)open_edges[first].to * 3]);
	    pairing_candidate best;
	    best.edge = open_edges.size();
	    std::vector<size_t> nearby_edges;
	    const endpoint_order lower = {first_from.x - weld_tolerance, 0};
	    auto endpoint_it = std::lower_bound(ordered_endpoints.begin(),
		ordered_endpoints.end(), lower,
		[](const endpoint_order &candidate_endpoint,
			const endpoint_order &limit) {
		    return candidate_endpoint.x < limit.x;
		});
	    for (; endpoint_it != ordered_endpoints.end() &&
		    endpoint_it->x <= first_from.x + weld_tolerance;
		    ++endpoint_it) {
		const size_t second = endpoint_it->edge;
		if (second <= first)
		    continue;
		const ON_3dPoint second_from(&vertices[
		    (size_t)open_edges[second].from * 3]);
		const ON_3dPoint second_to(&vertices[
		    (size_t)open_edges[second].to * 3]);
		const auto endpoint_box_match = [&](const ON_3dPoint &point) {
		    return std::fabs(first_from.y - point.y) <= weld_tolerance &&
			std::fabs(first_from.z - point.z) <= weld_tolerance;
		};
		if (endpoint_box_match(second_from) ||
			endpoint_box_match(second_to))
		    nearby_edges.push_back(second);
	    }
	    std::sort(nearby_edges.begin(), nearby_edges.end());
	    nearby_edges.erase(std::unique(nearby_edges.begin(),
		nearby_edges.end()), nearby_edges.end());
	    for (size_t second : nearby_edges) {
		if (open_edges[first].face < rigorous_face_count &&
			open_edges[second].face < rigorous_face_count)
		    continue;
		const ON_3dPoint second_from(&vertices[
		    (size_t)open_edges[second].from * 3]);
		const ON_3dPoint second_to(&vertices[
		    (size_t)open_edges[second].to * 3]);
		const double opposite_distance = std::max(
		    first_from.DistanceTo(second_to),
		    first_to.DistanceTo(second_from));
		const double same_distance = std::max(
		    first_from.DistanceTo(second_from),
		    first_to.DistanceTo(second_to));
		const bool same_direction =
		    same_distance < opposite_distance;
		const double distance = same_direction ? same_distance :
		    opposite_distance;
		if (distance <= weld_tolerance && distance < best.distance) {
		    best.distance = distance;
		    best.edge = second;
		    best.same_direction = same_direction;
		}
	    }
	    if (best.edge == open_edges.size())
		continue;
	    const directed_edge &target =
		open_edges[first].face < rigorous_face_count ?
		open_edges[first] : open_edges[best.edge];
	    const directed_edge &approximate =
		open_edges[first].face < rigorous_face_count ?
		open_edges[best.edge] : open_edges[first];
	    if (approximate.face < rigorous_face_count)
		continue;
	    const int old_vertices[2] = {
		approximate.from, approximate.to
	    };
	    const int new_vertices[2] = {
		best.same_direction ? target.from : target.to,
		best.same_direction ? target.to : target.from
	    };
	    if (debug_topology)
		bu_log("Near-boundary pairing iteration %d: approximate "
		    "edge %d->%d (face %d) to %d->%d (face %d), "
		    "distance %.17g\n", iteration, approximate.from,
		    approximate.to, approximate.face, target.to,
		    target.from, target.face, best.distance);
	    std::vector<size_t> old_corners[2];
	    old_corners[0].swap(
		approximate_corners[(size_t)old_vertices[0]]);
	    old_corners[1].swap(
		approximate_corners[(size_t)old_vertices[1]]);
	    std::vector<int> affected_faces;
	    if (++generation == 0) {
		std::fill(affected_generation.begin(),
		    affected_generation.end(), 0);
		generation = 1;
	    }
	    for (int endpoint = 0; endpoint < 2; ++endpoint) {
		for (size_t corner : old_corners[endpoint]) {
		    const int face = (int)(corner / 3);
		    if (affected_generation[(size_t)face] == generation)
			continue;
		    affected_generation[(size_t)face] = generation;
		    affected_faces.push_back(face);
		}
	    }
	    for (int face : affected_faces) {
		for (int corner = 0; corner < 3; ++corner) {
		    const size_t index = (size_t)face * 3 + corner;
		    if (!remove_incidence({face, candidate[index],
			    candidate[(size_t)face * 3 + (corner + 1) % 3]}))
			return false;
		}
	    }
	    for (int endpoint = 0; endpoint < 2; ++endpoint) {
		for (size_t corner : old_corners[endpoint]) {
		    candidate[corner] = new_vertices[endpoint];
		    approximate_corners[(size_t)new_vertices[endpoint]].
			push_back(corner);
		}
	    }
	    for (int face : affected_faces) {
		for (int corner = 0; corner < 3; ++corner) {
		    const size_t index = (size_t)face * 3 + corner;
		    add_incidence({face, candidate[index],
			candidate[(size_t)face * 3 + (corner + 1) % 3]});
		}
	    }
	    *welded_vertices += old_vertices[0] != new_vertices[0] ? 1 : 0;
	    *welded_vertices += old_vertices[1] != new_vertices[1] ? 1 : 0;
	    paired = true;
	}
	if (!paired) {
	    if (debug_topology) {
		bu_log("Near-boundary pairing rejected: no rigorous/approximate "
		    "edge pair lies within %.17g\n", weld_tolerance);
		for (const directed_edge &open : open_edges) {
		    const ON_3dPoint from(&vertices[(size_t)open.from * 3]);
		    const ON_3dPoint to(&vertices[(size_t)open.to * 3]);
		    double nearest_distance =
			std::numeric_limits<double>::infinity();
		    mesh_edge nearest(-1, -1);
		    size_t nearest_uses = 0;
		    for (const auto &edge : incidence) {
			const mesh_edge open_key(std::min(open.from, open.to),
			    std::max(open.from, open.to));
			if (edge.first == open_key)
			    continue;
			const ON_3dPoint first(&vertices[
			    (size_t)edge.first.first * 3]);
			const ON_3dPoint second(&vertices[
			    (size_t)edge.first.second * 3]);
			const double distance = std::min(
			    std::max(from.DistanceTo(first),
				to.DistanceTo(second)),
			    std::max(from.DistanceTo(second),
				to.DistanceTo(first)));
			if (distance < nearest_distance) {
			    nearest_distance = distance;
			    nearest = edge.first;
			    nearest_uses = edge.second.size();
			}
		    }
		    bu_log("  open edge %d->%d face %d (%s), nearest "
			"%d-%d has %zu uses at %.17g\n", open.from,
			open.to, open.face,
			open.face < rigorous_face_count ? "rigorous" :
			"approximate", nearest.first, nearest.second,
			nearest_uses, nearest_distance);
		}
	    }
	    return false;
	}
    }
    if (debug_topology)
	bu_log("Near-boundary pairing rejected: iteration limit reached\n");
    return false;
}

static int
repair_near_boundary_pair_contract(void)
{
    fastf_t vertices[10][3] = {
	{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0}, {0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0},
	{1.0, 1.0, 1.0}, {0.0, 1.0, 1.0},
	{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0}
    };
    int faces[12][3] = {
	{0, 2, 1}, {0, 3, 2},
	{0, 1, 5}, {0, 5, 4},
	{1, 2, 6}, {1, 6, 5},
	{2, 3, 7}, {2, 7, 6},
	{3, 0, 4}, {3, 4, 7},
	{8, 9, 6}, {8, 6, 7}
    };
    size_t paired = 0;
    if (!repair_pair_near_boundary_cracks(&faces[0][0], 12,
	    &vertices[0][0], 10, 10, 0.1, &paired) || paired != 2)
	return 1;
    if (bg_trimesh_solid2(10, 12, &vertices[0][0], &faces[0][0], NULL))
	return 2;

    /* A failed-face patch may carry the same provisional boundary winding as
     * its rigorous neighbor.  Coordinate pairing is still unambiguous; the
     * completed shell must be synchronized transactionally afterward. */
    int same_direction_faces[12][3] = {
	{0, 2, 1}, {0, 3, 2},
	{0, 1, 5}, {0, 5, 4},
	{1, 2, 6}, {1, 6, 5},
	{2, 3, 7}, {2, 7, 6},
	{3, 0, 4}, {3, 4, 7},
	{8, 6, 9}, {8, 7, 6}
    };
    paired = 0;
    if (!repair_pair_near_boundary_cracks(&same_direction_faces[0][0], 12,
	    &vertices[0][0], 10, 10, 0.1, &paired) || paired != 2)
	return 3;
    if (bg_trimesh_solid2(10, 12, &vertices[0][0],
	    &same_direction_faces[0][0], NULL))
	return 4;

    fastf_t fully_separated_vertices[12][3] = {
	{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0}, {0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0},
	{1.0, 1.0, 1.0}, {0.0, 1.0, 1.0},
	{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0},
	{1.0, 1.0, 1.0}, {0.0, 1.0, 1.0}
    };
    int fully_separated_faces[12][3] = {
	{0, 2, 1}, {0, 3, 2},
	{0, 1, 5}, {0, 5, 4},
	{1, 2, 6}, {1, 6, 5},
	{2, 3, 7}, {2, 7, 6},
	{3, 0, 4}, {3, 4, 7},
	{8, 9, 10}, {8, 10, 11}
    };
    paired = 0;
    if (!repair_pair_near_boundary_cracks(
	    &fully_separated_faces[0][0], 12,
	    &fully_separated_vertices[0][0], 12, 10, 0.1, &paired) ||
	    paired != 4)
	return 5;
    return bg_trimesh_solid2(12, 12, &fully_separated_vertices[0][0],
	&fully_separated_faces[0][0], NULL) ? 6 : 0;
}


/* The display tessellator emits each face independently.  Shared boundary
 * samples therefore commonly have different indices and may differ by a few
 * floating-point ulps even when they denote the same 3-D point.  Normalize
 * only that coordinate roundoff before indexed neighborhood analysis.  The
 * radius is deliberately independent of the user's geometric repair
 * tolerance, and every point maps directly to a representative within the
 * radius so a chain cannot accumulate displacement. */
static size_t
repair_weld_roundoff_vertices(int *faces, int face_count,
	const fastf_t *vertices, int vertex_count)
{
    if (!faces || face_count <= 0 || !vertices || vertex_count <= 0)
	return 0;

    ON_BoundingBox bounds;
    for (int vertex = 0; vertex < vertex_count; ++vertex) {
	const ON_3dPoint point(&vertices[(size_t)vertex * 3]);
	if (!point.IsValid())
	    return 0;
	if (vertex)
	    bounds.Set(point, true);
	else
	    bounds.Set(point, false);
    }
    if (!bounds.IsValid())
	return 0;
    double coordinate_scale = std::max(1.0, bounds.Diagonal().Length());
    for (int axis = 0; axis < 3; ++axis) {
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_min[axis]));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_max[axis]));
    }
    const double tolerance = 256.0 *
	std::numeric_limits<double>::epsilon() * coordinate_scale;
    if (!(tolerance > 0.0) || !std::isfinite(tolerance))
	return 0;
    const double tolerance_squared = tolerance * tolerance;

    typedef std::array<int64_t, 3> weld_cell;
    std::map<weld_cell, std::vector<int>> cells;
    std::vector<int> remap((size_t)vertex_count, -1);
    size_t welded = 0;
    for (int vertex = 0; vertex < vertex_count; ++vertex) {
	const ON_3dPoint point(&vertices[(size_t)vertex * 3]);
	weld_cell cell;
	for (int axis = 0; axis < 3; ++axis) {
	    const long double offset =
		((long double)point[axis] - bounds.m_min[axis]) / tolerance;
	    if (!(offset >= 0.0L) ||
		    offset > (long double)std::numeric_limits<int64_t>::max())
		return 0;
	    cell[(size_t)axis] = (int64_t)std::floor(offset);
	}
	int representative = -1;
	double best_distance = std::numeric_limits<double>::infinity();
	for (int dx = -1; dx <= 1; ++dx) {
	    for (int dy = -1; dy <= 1; ++dy) {
		for (int dz = -1; dz <= 1; ++dz) {
		    const weld_cell neighbor = {{cell[0] + dx,
			cell[1] + dy, cell[2] + dz}};
		    const auto found = cells.find(neighbor);
		    if (found == cells.end())
			continue;
		    for (int candidate : found->second) {
			const ON_3dPoint prior(
			    &vertices[(size_t)candidate * 3]);
			const double distance = (point - prior).LengthSquared();
			if (distance <= tolerance_squared &&
				distance < best_distance) {
			    representative = candidate;
			    best_distance = distance;
			}
		    }
		}
	    }
	}
	if (representative < 0) {
	    representative = vertex;
	    cells[cell].push_back(vertex);
	} else {
	    welded++;
	}
	remap[(size_t)vertex] = representative;
    }
    for (int corner = 0; corner < face_count * 3; ++corner) {
	const int vertex = faces[corner];
	if (vertex < 0 || vertex >= vertex_count)
	    return 0;
	faces[corner] = remap[(size_t)vertex];
    }
    return welded;
}


static int
repair_weld_roundoff_contract(void)
{
    const fastf_t corners[8][3] = {
	{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0}, {0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0},
	{1.0, 1.0, 1.0}, {0.0, 1.0, 1.0}
    };
    const int cube[12][3] = {
	{0, 2, 1}, {0, 3, 2}, {0, 1, 5}, {0, 5, 4},
	{1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6},
	{3, 0, 4}, {3, 4, 7}, {4, 5, 6}, {4, 6, 7}
    };
    fastf_t vertices[36][3];
    int faces[12][3];
    for (int face = 0; face < 12; ++face) {
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = face * 3 + corner;
	    const int source = cube[face][corner];
	    for (int axis = 0; axis < 3; ++axis) {
		vertices[vertex][axis] = corners[source][axis];
		if (face && ((face + corner + axis) % 2))
		    vertices[vertex][axis] = std::nextafter(
			vertices[vertex][axis], 2.0);
	    }
	    faces[face][corner] = vertex;
	}
    }
    const size_t welded = repair_weld_roundoff_vertices(&faces[0][0], 12,
	&vertices[0][0], 36);
    std::set<int> used;
    for (const auto &face : faces)
	used.insert(face, face + 3);
    if (welded != 28 || used.size() != 8 ||
	    bg_trimesh_solid2(36, 12, &vertices[0][0], &faces[0][0], NULL))
	return 1;

    const double tolerance = 256.0 *
	std::numeric_limits<double>::epsilon();
    fastf_t chain_vertices[3][3] = {
	{0.0, 0.0, 0.0}, {0.75 * tolerance, 0.0, 0.0},
	{1.5 * tolerance, 0.0, 0.0}
    };
    int chain_face[3] = {0, 1, 2};
    if (repair_weld_roundoff_vertices(chain_face, 1,
	    &chain_vertices[0][0], 3) != 1 || chain_face[0] != chain_face[1] ||
	    chain_face[0] == chain_face[2])
	return 2;
    return 0;
}


/* A constrained fallback can be topologically closed while containing small
 * regions whose source face collapsed to a line.  Removing those triangles
 * globally reopens every seam.  Instead, replace only complete edge-connected
 * degenerate neighborhoods, retaining every perimeter index.  A simple
 * boundary is spanned from its centroid when possible; if the ring itself is
 * numerically flat, move only the new interior point a bounded distance in a
 * tangent direction inferred from the retained neighboring triangles. */
static bool
repair_degenerate_approximate_neighborhoods(int **faces, int *face_count,
	fastf_t **vertices, int *vertex_count, int rigorous_face_count,
	std::vector<int> &source_faces, double allowed_deviation,
	size_t max_boundary_edges, repair_degenerate_neighborhood_stats *stats)
{
    const bool debug_topology = getenv(
	"BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY") != NULL;
    if (!faces || !face_count || !vertices || !vertex_count || !*faces ||
	    !*vertices || *face_count <= 0 || *vertex_count <= 0 ||
	    rigorous_face_count < 0 || rigorous_face_count > *face_count ||
	    source_faces.size() != (size_t)*face_count ||
	    !(allowed_deviation > 0.0) || !std::isfinite(allowed_deviation) ||
	    !stats)
	{
	    if (debug_topology)
		bu_log("Degenerate neighborhood repair rejected: invalid call "
		    "arguments (allowed %.17g, sources %zu, faces %d)\n",
		    allowed_deviation, source_faces.size(),
		    face_count ? *face_count : -1);
	    return false;
	}
    *stats = repair_degenerate_neighborhood_stats();

    typedef std::pair<int, int> mesh_edge;
    struct edge_use {
	int face;
	int from;
	int to;
    };
    const int input_face_count = *face_count;
    const int input_vertex_count = *vertex_count;
    const int *input_faces = *faces;
    const fastf_t *input_vertices = *vertices;
    std::vector<bool> degenerate((size_t)input_face_count, false);
    std::vector<int> degenerate_faces;
    std::map<mesh_edge, std::vector<edge_use>> edge_uses;
    const auto is_geometric_degenerate = [&](int face) {
	ON_3dPoint points[3];
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = input_faces[(size_t)face * 3 + corner];
	    if (vertex < 0 || vertex >= input_vertex_count)
		return true;
	    points[corner] = ON_3dPoint(
		&input_vertices[(size_t)vertex * 3]);
	}
	const ON_3dVector e01 = points[1] - points[0];
	const ON_3dVector e02 = points[2] - points[0];
	const ON_3dVector e12 = points[2] - points[1];
	const double longest_squared = std::max(e01.LengthSquared(),
	    std::max(e02.LengthSquared(), e12.LengthSquared()));
	const double doubled_area = ON_CrossProduct(e01, e02).Length();
	return !(longest_squared > 0.0) || !std::isfinite(doubled_area) ||
	    !(doubled_area > 64.0 *
	    std::numeric_limits<double>::epsilon() * longest_squared);
    };
    for (int face = 0; face < input_face_count; ++face) {
	degenerate[(size_t)face] = is_geometric_degenerate(face);
	if (degenerate[(size_t)face]) {
	    if (face < rigorous_face_count || source_faces[(size_t)face] < 0) {
		if (debug_topology)
		    bu_log("Degenerate neighborhood repair rejected: triangle "
			"%d is %s (rigorous prefix %d, source %d)\n", face,
			face < rigorous_face_count ? "rigorous" :
			"unattributed", rigorous_face_count,
			source_faces[(size_t)face]);
		return false;
	    }
	    degenerate_faces.push_back(face);
	}
	for (int corner = 0; corner < 3; ++corner) {
	    const int from = input_faces[(size_t)face * 3 + corner];
	    const int to = input_faces[(size_t)face * 3 +
		(corner + 1) % 3];
	    if (from == to)
		continue;
	    edge_uses[std::minmax(from, to)].push_back({face, from, to});
	}
    }
    if (degenerate_faces.empty())
	return false;
    /* Do not let an unrelated open edge suppress every recoverable local
     * component.  The candidate is still transactional and must pass the
     * complete indexed-solid gate below before any replacement commits. */
    size_t irregular_edges = 0;
    for (const auto &edge : edge_uses) {
	if (edge.second.size() != 2)
	    irregular_edges++;
    }
    if (debug_topology && irregular_edges)
	bu_log("Degenerate neighborhood repair: retaining %zu unrelated "
	    "irregular edges for final transactional validation\n",
	    irregular_edges);

    std::vector<int> local_index((size_t)input_face_count, -1);
    for (size_t local = 0; local < degenerate_faces.size(); ++local)
	local_index[(size_t)degenerate_faces[local]] = (int)local;
    std::vector<int> parents(degenerate_faces.size());
    std::iota(parents.begin(), parents.end(), 0);
    const auto component_root = [&](int item) {
	int root = item;
	while (parents[(size_t)root] != root)
	    root = parents[(size_t)root];
	while (parents[(size_t)item] != item) {
	    const int next = parents[(size_t)item];
	    parents[(size_t)item] = root;
	    item = next;
	}
	return root;
    };
    for (const auto &edge : edge_uses) {
	int first_local = -1;
	std::set<int> joined_faces;
	for (const edge_use &use : edge.second) {
	    if (!degenerate[(size_t)use.face] ||
		    !joined_faces.insert(use.face).second)
		continue;
	    const int local = local_index[(size_t)use.face];
	    if (first_local < 0) {
		first_local = local;
		continue;
	    }
	    const int first = component_root(first_local);
	    const int second = component_root(local);
	    if (first != second)
		parents[(size_t)second] = first;
	}
    }
    std::map<int, std::vector<int>> components;
    for (size_t local = 0; local < degenerate_faces.size(); ++local)
	components[component_root((int)local)].push_back(
	    degenerate_faces[local]);

    struct replacement_patch {
	std::vector<int> faces;
	std::vector<fastf_t> point;
	int source_face = -1;
	double center_offset = 0.0;
    };
    std::vector<replacement_patch> patches;
    std::vector<bool> remove_face((size_t)input_face_count, false);
    for (const auto &component_entry : components) {
	std::vector<int> component = component_entry.second;
	const size_t previously_absorbed = std::count_if(component.begin(),
	    component.end(), [&](int face) {
		return remove_face[(size_t)face];
	    });
	if (previously_absorbed == component.size())
	    continue;
	if (previously_absorbed) {
	    if (debug_topology)
		bu_log("Degenerate neighborhood repair rejected: a prior "
		    "bounded patch absorbed %zu/%zu triangles from another "
		    "component\n", previously_absorbed, component.size());
	    return false;
	}
	std::set<int> component_faces(component.begin(), component.end());
	std::set<int> component_sources;
	for (int face : component)
	    component_sources.insert(source_faces[(size_t)face]);
	std::vector<edge_use> boundary;
	std::map<mesh_edge, int> retained_neighbors;
	const size_t max_component_faces = std::max((size_t)16,
	    max_boundary_edges <= SIZE_MAX / 4 ? 4 * max_boundary_edges :
	    max_boundary_edges);
	for (;;) {
	    boundary.clear();
	    retained_neighbors.clear();
	    std::set<int> forced_grow_faces;
	    for (const auto &edge : edge_uses) {
		bool component_incident = false;
		std::vector<edge_use> retained_uses;
		for (const edge_use &use : edge.second) {
		    if (component_faces.find(use.face) !=
			    component_faces.end())
			component_incident = true;
		    else if (!remove_face[(size_t)use.face])
			retained_uses.push_back(use);
		}
		if (!component_incident || retained_uses.empty())
		    continue;
		/* A collapsed triangle can traverse its only surviving edge
		 * twice.  If two sound triangles already use that edge
		 * oppositely, deleting the collapsed island exposes no boundary
		 * and needs no patch. */
		if (retained_uses.size() == 2 &&
			retained_uses[0].from == retained_uses[1].to &&
			retained_uses[0].to == retained_uses[1].from)
		    continue;
		if (retained_uses.size() != 1) {
		    bool approximate_star = true;
		    for (const edge_use &retained : retained_uses) {
			if (retained.face < rigorous_face_count ||
				retained.face >= input_face_count ||
				source_faces[(size_t)retained.face] < 0) {
			    approximate_star = false;
			    break;
			}
			forced_grow_faces.insert(retained.face);
		    }
		    if (!approximate_star) {
			if (debug_topology)
			    bu_log("Degenerate neighborhood repair rejected: "
				"edge %d-%d has %zu retained uses including a "
				"rigorous triangle\n", edge.first.first,
				edge.first.second, retained_uses.size());
			return false;
		    }
		    continue;
		}
		const edge_use &retained = retained_uses.front();
		boundary.push_back({component.front(), retained.to,
		    retained.from});
		retained_neighbors[edge.first] = retained.face;
	    }

	    if (!forced_grow_faces.empty()) {
		if (component.size() + forced_grow_faces.size() >
			max_component_faces)
		    return false;
		for (int face : forced_grow_faces) {
		    if (remove_face[(size_t)face])
			return false;
		    if (component_faces.insert(face).second) {
			component.push_back(face);
			component_sources.insert(source_faces[(size_t)face]);
		    }
		}
		continue;
	    }
	    bool boundary_ready = boundary.empty() || boundary.size() >= 3;
	    std::map<int, size_t> boundary_degree;
	    for (const edge_use &edge : boundary) {
		boundary_degree[edge.from]++;
		boundary_degree[edge.to]++;
	    }
	    for (const auto &vertex : boundary_degree)
		boundary_ready = boundary_ready && !(vertex.second % 2);
	    if (boundary_ready)
		break;
	    if (boundary.size() > max_boundary_edges ||
		    component.size() >= max_component_faces) {
		if (debug_topology)
		    bu_log("Degenerate neighborhood repair rejected: bounded "
			"growth reached %zu triangles and %zu boundary edges\n",
			component.size(), boundary.size());
		return false;
	    }

	    std::set<int> grow_faces;
	    for (const auto &neighbor : retained_neighbors) {
		const int face = neighbor.second;
		if (face < rigorous_face_count || face >= input_face_count ||
			source_faces[(size_t)face] < 0 ||
			component_faces.find(face) != component_faces.end())
		    continue;
		grow_faces.insert(face);
	    }
	    if (grow_faces.empty() || component.size() + grow_faces.size() >
		    max_component_faces) {
		if (debug_topology) {
		    bu_log("Degenerate neighborhood repair rejected: %zu-edge "
			"boundary cannot grow without changing rigorous triangles\n",
			boundary.size());
		    for (const auto &vertex : boundary_degree) {
			if (!(vertex.second % 2))
			    continue;
			const ON_3dPoint point(&input_vertices[
			    (size_t)vertex.first * 3]);
			double nearest_distance =
			    std::numeric_limits<double>::infinity();
			int nearest_vertex = -1;
			for (const auto &candidate : boundary_degree) {
			    if (candidate.first == vertex.first ||
				    !(candidate.second % 2))
				continue;
			    const ON_3dPoint other(&input_vertices[
				(size_t)candidate.first * 3]);
			    if (point.DistanceTo(other) < nearest_distance) {
				nearest_distance = point.DistanceTo(other);
				nearest_vertex = candidate.first;
			    }
			}
			bu_log("  odd boundary vertex %d degree %zu; nearest %d "
			    "at %.17g\n", vertex.first, vertex.second,
			    nearest_vertex, nearest_distance);
		    }
		}
		return false;
	    }
	    for (int face : grow_faces) {
		if (remove_face[(size_t)face])
		    return false;
		component.push_back(face);
		component_faces.insert(face);
		component_sources.insert(source_faces[(size_t)face]);
	    }
	}
	for (int face : component)
	    remove_face[(size_t)face] = true;
	stats->source_faces.insert(component_sources.begin(),
	    component_sources.end());
	/* A zero-boundary component is a closed zero-area island.  It has no
	 * edge attachment to preserve and may be discarded transactionally. */
	if (boundary.empty())
	    continue;
	if (boundary.size() < 3 || boundary.size() > max_boundary_edges) {
	    if (debug_topology) {
		bu_log("Degenerate neighborhood repair rejected: component "
		    "with %zu triangles has %zu boundary edges\n",
		    component.size(), boundary.size());
		for (int face : component) {
		    bu_log("  component triangle %d source %d: %d %d %d\n",
			face, source_faces[(size_t)face],
			input_faces[(size_t)face * 3],
			input_faces[(size_t)face * 3 + 1],
			input_faces[(size_t)face * 3 + 2]);
		}
		for (const auto &edge : edge_uses) {
		    bool incident = false;
		    for (const edge_use &use : edge.second) {
			if (component_faces.find(use.face) !=
				component_faces.end()) {
			    incident = true;
			    break;
			}
		    }
		    if (!incident)
			continue;
		    std::string uses;
		    for (const edge_use &use : edge.second) {
			if (!uses.empty())
			    uses += ",";
			uses += std::to_string(use.face) + ":" +
			    std::to_string(use.from) + "->" +
			    std::to_string(use.to);
		    }
		    bu_log("  incident edge %d-%d uses %s\n",
			edge.first.first, edge.first.second, uses.c_str());
		}
	    }
	    return false;
	}
	std::map<int, std::vector<std::pair<int, size_t>>>
	    boundary_adjacency;
	for (size_t edge_index = 0; edge_index < boundary.size();
		edge_index++) {
	    const edge_use &edge = boundary[edge_index];
	    boundary_adjacency[edge.from].push_back({edge.to, edge_index});
	    boundary_adjacency[edge.to].push_back({edge.from, edge_index});
	}
	for (const auto &vertex : boundary_adjacency) {
	    if (vertex.second.empty() || vertex.second.size() % 2) {
		if (debug_topology) {
		    const ON_3dPoint point(&input_vertices[
			(size_t)vertex.first * 3]);
		    double nearest_distance =
			std::numeric_limits<double>::infinity();
		    int nearest_vertex = -1;
		    for (const auto &candidate : boundary_adjacency) {
			if (candidate.first == vertex.first ||
				candidate.second.size() % 2 == 0)
			    continue;
			const ON_3dPoint candidate_point(&input_vertices[
			    (size_t)candidate.first * 3]);
			const double distance = point.DistanceTo(candidate_point);
			if (distance < nearest_distance) {
			    nearest_distance = distance;
			    nearest_vertex = candidate.first;
			}
		    }
		    bu_log("Degenerate neighborhood repair rejected: %zu-edge "
			"boundary has odd degree %zu at vertex %d; nearest odd "
			"vertex %d is %.17g away\n", boundary.size(),
			vertex.second.size(), vertex.first, nearest_vertex,
			nearest_distance);
		}
		return false;
	    }
	}
	/* Hierholzer's construction permits weakly-simple boundaries that touch
	 * at a vertex.  The fan may retain a pinched link there; the later
	 * topology-only split duplicates that point without moving it. */
	std::vector<bool> used_boundary(boundary.size(), false);
	std::vector<int> stack;
	std::vector<int> circuit;
	const int start = boundary_adjacency.begin()->first;
	stack.push_back(start);
	while (!stack.empty()) {
	    const int current = stack.back();
	    auto &neighbors = boundary_adjacency[current];
	    while (!neighbors.empty() &&
		    used_boundary[neighbors.back().second])
		neighbors.pop_back();
	    if (neighbors.empty()) {
		circuit.push_back(current);
		stack.pop_back();
		continue;
	    }
	    const std::pair<int, size_t> next = neighbors.back();
	    neighbors.pop_back();
	    used_boundary[next.second] = true;
	    stack.push_back(next.first);
	}
	if (circuit.size() != boundary.size() + 1 ||
		circuit.front() != circuit.back() ||
		std::find(used_boundary.begin(), used_boundary.end(), false) !=
		used_boundary.end()) {
	    if (debug_topology)
		bu_log("Degenerate neighborhood repair rejected: %zu-edge "
		    "boundary is not one connected Euler circuit\n",
		    boundary.size());
	    return false;
	}
	std::reverse(circuit.begin(), circuit.end());
	std::vector<int> ring(circuit.begin(), circuit.end() - 1);

	ON_3dPoint center(0.0, 0.0, 0.0);
	ON_3dVector average_normal(0.0, 0.0, 0.0);
	ON_3dVector average_outside(0.0, 0.0, 0.0);
	double local_scale = 0.0;
	for (size_t point = 0; point < ring.size(); ++point) {
	    const int first = ring[point];
	    const int second = ring[(point + 1) % ring.size()];
	    const ON_3dPoint a(&input_vertices[(size_t)first * 3]);
	    const ON_3dPoint b(&input_vertices[(size_t)second * 3]);
	    center += a;
	    local_scale = std::max(local_scale, a.DistanceTo(b));
	    const auto retained = retained_neighbors.find(
		std::minmax(first, second));
	    if (retained == retained_neighbors.end()) {
		if (debug_topology)
		    bu_log("Degenerate neighborhood repair rejected: ring edge "
			"%d-%d lacks a retained neighbor\n", first, second);
		return false;
	    }
	    const int retained_face = retained->second;
	    ON_3dPoint triangle[3];
	    int third = -1;
	    for (int corner = 0; corner < 3; ++corner) {
		const int vertex = input_faces[(size_t)retained_face * 3 + corner];
		triangle[corner] = ON_3dPoint(
		    &input_vertices[(size_t)vertex * 3]);
		if (vertex != first && vertex != second)
		    third = vertex;
	    }
	    ON_3dVector normal = ON_CrossProduct(triangle[1] - triangle[0],
		triangle[2] - triangle[0]);
	    if (normal.Unitize())
		average_normal += normal;
	    if (third >= 0) {
		const ON_3dPoint outside(
		    &input_vertices[(size_t)third * 3]);
		average_outside += outside - (a + b) / 2.0;
	    }
	}
	center /= (double)ring.size();
	if (!(local_scale > 0.0)) {
	    if (debug_topology)
		bu_log("Degenerate neighborhood repair rejected: %zu-edge "
		    "boundary has no usable scale\n", ring.size());
	    return false;
	}
	if (!average_normal.Unitize()) {
	    ON_3dVector boundary_normal(0.0, 0.0, 0.0);
	    for (size_t point = 0; point < ring.size(); ++point) {
		const ON_3dPoint a(&input_vertices[
		    (size_t)ring[point] * 3]);
		const ON_3dPoint b(&input_vertices[
		    (size_t)ring[(point + 1) % ring.size()] * 3]);
		boundary_normal += ON_CrossProduct(a - center, b - center);
	    }
	    if (!boundary_normal.Unitize()) {
		if (debug_topology)
		    bu_log("Degenerate neighborhood repair rejected: %zu-edge "
			"boundary has no retained or ring normal\n", ring.size());
		return false;
	    }
	    average_normal = boundary_normal;
	}
	ON_3dVector axis(0.0, 0.0, 0.0);
	for (size_t point = 0; point < ring.size(); ++point) {
	    const ON_3dPoint a(&input_vertices[(size_t)ring[point] * 3]);
	    const ON_3dPoint b(&input_vertices[(size_t)ring[
		(point + 1) % ring.size()] * 3]);
	    if ((b - a).LengthSquared() > axis.LengthSquared())
		axis = b - a;
	}
	if (!axis.Unitize()) {
	    if (debug_topology)
		bu_log("Degenerate neighborhood repair rejected: %zu-edge "
		    "boundary has no usable axis\n", ring.size());
	    return false;
	}
	ON_3dVector tangent = ON_CrossProduct(average_normal, axis);
	if (!tangent.Unitize()) {
	    if (debug_topology)
		bu_log("Degenerate neighborhood repair rejected: %zu-edge "
		    "boundary has no usable tangent\n", ring.size());
	    return false;
	}
	if (average_outside * tangent > 0.0)
	    tangent = -tangent;

	const auto valid_center = [&](const ON_3dPoint &candidate) {
	    for (size_t point = 0; point < ring.size(); ++point) {
		const ON_3dPoint a(&input_vertices[(size_t)ring[point] * 3]);
		const ON_3dPoint b(&input_vertices[(size_t)ring[
		    (point + 1) % ring.size()] * 3]);
		const ON_3dVector e01 = b - a;
		const ON_3dVector e02 = candidate - a;
		const ON_3dVector e12 = candidate - b;
		const double longest_squared = std::max(e01.LengthSquared(),
		    std::max(e02.LengthSquared(), e12.LengthSquared()));
		const double doubled_area = ON_CrossProduct(e01, e02).Length();
		if (!(longest_squared > 0.0) ||
			!(doubled_area > 64.0 *
			std::numeric_limits<double>::epsilon() *
			longest_squared))
		    return false;
	    }
	    return true;
	};
	ON_3dPoint patch_center = center;
	double center_offset = 0.0;
	if (!valid_center(patch_center)) {
	    const double maximum_offset = 0.25 * allowed_deviation;
	    double offset = std::min(maximum_offset, std::max(
		(double)BN_TOL_DIST, local_scale * 1.0e-6));
	    bool found = false;
	    while (offset > 0.0 && offset <= maximum_offset && !found) {
		for (int sign : {1, -1}) {
		    const ON_3dPoint candidate = center +
			(double)sign * offset * tangent;
		    if (!valid_center(candidate))
			continue;
		    patch_center = candidate;
		    center_offset = offset;
		    found = true;
		    break;
		}
		if (offset > maximum_offset / 4.0)
		    break;
		offset *= 4.0;
	    }
	    if (!found) {
		if (debug_topology)
		    bu_log("Degenerate neighborhood repair rejected: %zu-edge "
			"boundary cannot form nondegenerate fan within %.17g\n",
			ring.size(), maximum_offset);
		return false;
	    }
	}
	replacement_patch patch;
	patch.source_face = component_sources.size() == 1 ?
	    *component_sources.begin() : -1;
	patch.center_offset = center_offset;
	patch.point = {patch_center.x, patch_center.y, patch_center.z};
	patch.faces.reserve(ring.size() * 3);
	for (size_t point = 0; point < ring.size(); ++point) {
	    patch.faces.push_back(ring[point]);
	    patch.faces.push_back(ring[(point + 1) % ring.size()]);
	    patch.faces.push_back(-1);
	}
	patches.push_back(std::move(patch));
    }

    std::vector<int> candidate_faces;
    std::vector<int> candidate_sources;
    candidate_faces.reserve((size_t)input_face_count * 3);
    candidate_sources.reserve((size_t)input_face_count);
    for (int face = 0; face < input_face_count; ++face) {
	if (remove_face[(size_t)face])
	    continue;
	candidate_faces.insert(candidate_faces.end(),
	    &input_faces[(size_t)face * 3],
	    &input_faces[(size_t)face * 3] + 3);
	candidate_sources.push_back(source_faces[(size_t)face]);
    }
    std::vector<fastf_t> candidate_vertices(input_vertices,
	input_vertices + (size_t)input_vertex_count * 3);
    for (replacement_patch &patch : patches) {
	const int center_index = (int)(candidate_vertices.size() / 3);
	candidate_vertices.insert(candidate_vertices.end(), patch.point.begin(),
	    patch.point.end());
	for (size_t corner = 2; corner < patch.faces.size(); corner += 3)
	    patch.faces[corner] = center_index;
	candidate_faces.insert(candidate_faces.end(), patch.faces.begin(),
	    patch.faces.end());
	candidate_sources.insert(candidate_sources.end(),
	    patch.faces.size() / 3, patch.source_face);
	stats->added_faces += (int)(patch.faces.size() / 3);
	stats->max_center_offset = std::max(stats->max_center_offset,
	    patch.center_offset);
    }

    std::vector<int> synchronized_faces(candidate_faces.size());
    if (bg_trimesh_sync(synchronized_faces.data(), candidate_faces.data(),
	    (int)(candidate_faces.size() / 3)) >= 0)
	candidate_faces.swap(synchronized_faces);

    /* Compact unused vertices so the independent whole-mesh validation can
     * distinguish a sound local replacement from a merely edge-closed one. */
    std::vector<int> remap(candidate_vertices.size() / 3, -1);
    int compact_count = 0;
    for (int &vertex : candidate_faces) {
	if (vertex < 0 || (size_t)vertex >= remap.size()) {
	    if (debug_topology)
		bu_log("Degenerate neighborhood repair rejected: replacement "
		    "index %d outside %zu points\n", vertex, remap.size());
	    return false;
	}
	if (remap[(size_t)vertex] < 0)
	    remap[(size_t)vertex] = compact_count++;
	vertex = remap[(size_t)vertex];
    }
    std::vector<fastf_t> compact_vertices((size_t)compact_count * 3);
    for (size_t old = 0; old < remap.size(); ++old) {
	if (remap[old] < 0)
	    continue;
	memcpy(&compact_vertices[(size_t)remap[old] * 3],
	    &candidate_vertices[old * 3], 3 * sizeof(fastf_t));
    }
    assembled_mesh_validation validation;
    (void)assembled_mesh_validate(compact_count,
	(int)(candidate_faces.size() / 3), compact_vertices.data(),
	candidate_faces.data(), &validation, false);
    struct bg_trimesh_solid_errors solid_errors =
	BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    (void)bg_trimesh_solid2(compact_count,
	(int)(candidate_faces.size() / 3), compact_vertices.data(),
	candidate_faces.data(), &solid_errors);
    const bool edge_closed = !solid_errors.degenerate.count &&
	!solid_errors.unmatched.count && !solid_errors.excess.count &&
	!solid_errors.misoriented.count;
    if (validation.invalid_indices || validation.nonfinite_vertices ||
	    validation.unused_vertices || validation.degenerate_faces ||
	    !edge_closed) {
	if (debug_topology)
	    bu_log("Degenerate neighborhood repair rejected after assembly: "
		"invalid %zu, nonfinite %zu, unused %zu, degenerate %zu, "
		"invalid links %zu, unmatched %d, excess %d, misoriented "
		"%d\n", validation.invalid_indices,
		validation.nonfinite_vertices, validation.unused_vertices,
		validation.degenerate_faces, validation.invalid_vertex_links,
		solid_errors.unmatched.count, solid_errors.excess.count,
		solid_errors.misoriented.count);
	bg_free_trimesh_solid_errors(&solid_errors);
	return false;
    }
    bg_free_trimesh_solid_errors(&solid_errors);

    int *new_faces = (int *)bu_malloc(candidate_faces.size() * sizeof(int),
	"locally repaired degenerate faces");
    fastf_t *new_vertices = (fastf_t *)bu_malloc(
	compact_vertices.size() * sizeof(fastf_t),
	"locally repaired degenerate vertices");
    memcpy(new_faces, candidate_faces.data(),
	candidate_faces.size() * sizeof(int));
    memcpy(new_vertices, compact_vertices.data(),
	compact_vertices.size() * sizeof(fastf_t));
    bu_free(*faces, "pre-degenerate-neighborhood repair faces");
    bu_free(*vertices, "pre-degenerate-neighborhood repair vertices");
    *faces = new_faces;
    *face_count = (int)(candidate_faces.size() / 3);
    *vertices = new_vertices;
    *vertex_count = compact_count;
    source_faces.swap(candidate_sources);
    stats->components = (int)components.size();
    stats->removed_faces = (int)std::count(remove_face.begin(),
	remove_face.end(), true);
    return true;
}

/* A failed periodic side face may still have two authoritative closed edge
 * loops supplied by rigorous neighboring faces.  Join those loops in a
 * synthetic rectangular chart while keeping their exact shared 3-D samples.
 * If the boundary-only chords do not meet the requested fidelity, insert
 * bounded rows evaluated directly on the failed source surface.  This avoids
 * projecting a twisted 3-D patch onto one plane while retaining the source
 * face's seam topology.  General holed or partly constrained faces remain the
 * responsibility of the other bounded recovery paths. */
static bool
repair_constrained_periodic_strip(struct ON_Brep_CDT_State *s_cdt,
	int face_index, const repair_fast_constraint_store &constraints,
	size_t max_points, fastf_t allowed_deviation, int64_t deadline,
	repair_boundary_patch &patch, int required_step = 0,
	size_t required_shift = std::numeric_limits<size_t>::max())
{
    patch = repair_boundary_patch();
    if (!s_cdt || !s_cdt->brep || face_index < 0 ||
	    face_index >= s_cdt->brep->m_F.Count() ||
	    constraints.face_index != face_index ||
	    constraints.trims.size() < 2 || !(allowed_deviation > 0.0) ||
	    !std::isfinite(allowed_deviation))
	return false;

    max_points = std::min(max_points,
	(size_t)MAX_BOUNDARY_STRIP_REFINEMENT_POINTS);

    const ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    const ON_BrepLoop *outer = face.OuterLoop();
    const ON_Surface *surface = face.SurfaceOf();
    const auto reject = [&](const char *reason) {
	if (getenv("BRLCAD_CDT_DEBUG_STRIP") &&
		getenv("BRLCAD_CDT_DEBUG_STRIP")[0] &&
		!BU_STR_EQUAL(getenv("BRLCAD_CDT_DEBUG_STRIP"), "0"))
	    bu_log("Face %d: periodic strip rejected: %s\n", face_index,
		reason);
	return false;
    };
    if (!outer || !surface || face.LoopCount() != 1 ||
	    outer->TrimCount() < 4)
	return reject("not one sufficiently bounded outer loop");

    struct boundary_path {
	std::vector<repair_fast_trim_sample> samples;
	size_t constrained_edges = 0;
	size_t constrained_samples = 0;
    };
    std::vector<boundary_path> boundaries;
    std::vector<const ON_BrepTrim *> seam_boundaries;
    int seam_trims = 0;
    bool prior_constrained = false;
    bool first_constrained = false;
    bool last_constrained = false;
    const auto append_path = [&](boundary_path &path,
	    const std::vector<repair_fast_trim_sample> &samples,
	    size_t edge_count, size_t sample_count) {
	if (samples.empty())
	    return false;
	if (path.samples.empty()) {
	    path.samples = samples;
	} else {
	    const repair_point_key prior = {path.samples.back().point[X],
		path.samples.back().point[Y], path.samples.back().point[Z]};
	    const repair_point_key next = {samples.front().point[X],
		samples.front().point[Y], samples.front().point[Z]};
	    if (prior != next)
		return false;
	    path.samples.insert(path.samples.end(), samples.begin() + 1,
		samples.end());
	}
	path.constrained_edges += edge_count;
	path.constrained_samples += sample_count;
	return true;
    };
    for (int trim_index = 0; trim_index < outer->TrimCount(); ++trim_index) {
	const ON_BrepTrim *trim = outer->Trim(trim_index);
	if (!trim)
	    return reject("missing outer trim");
	const auto constrained = constraints.trims.find(trim->m_trim_index);
	if (constrained != constraints.trims.end()) {
	    const ON_BrepEdge *edge = trim->Edge();
	    if (!edge || constrained->second.size() < 2)
		return reject("constrained boundary edge is not fully sampled");
	    if (!prior_constrained)
		boundaries.push_back(boundary_path());
	    if (!append_path(boundaries.back(), constrained->second, 1,
		    constrained->second.size()))
		return reject("adjacent constrained boundary edges do not meet");
	    prior_constrained = true;
	    first_constrained = first_constrained || trim_index == 0;
	    last_constrained = trim_index + 1 == outer->TrimCount();
	    continue;
	}
	if (trim->m_type != ON_BrepTrim::seam)
	    return reject("unconstrained trim is not a seam");
	seam_boundaries.push_back(trim);
	seam_trims++;
	prior_constrained = false;
	last_constrained = false;
    }
    if (first_constrained && last_constrained && boundaries.size() > 1) {
	boundary_path joined = std::move(boundaries.back());
	if (!append_path(joined, boundaries.front().samples,
		boundaries.front().constrained_edges,
		boundaries.front().constrained_samples))
	    return reject("wrapped constrained boundary edges do not meet");
	boundaries.front() = std::move(joined);
	boundaries.pop_back();
    }
    if (boundaries.size() != 2 || seam_trims < 2)
	return reject("topology is not a two-boundary periodic strip");

    int closed_direction = -1;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const double period = surface->Domain(direction).Length();
	if (!(period > ON_ZERO_TOLERANCE) || !std::isfinite(period))
	    continue;
	bool spans_period = true;
	for (const boundary_path &boundary : boundaries) {
	    const std::vector<repair_fast_trim_sample> &samples =
		boundary.samples;
	    spans_period = spans_period && std::fabs(
		samples.back().uv[direction] -
		samples.front().uv[direction]) >= 0.5 * period;
	}
	if (spans_period) {
	    closed_direction = direction;
	    break;
	}
    }
    /* Some imported NURBS patches encode a cut explicitly with two
     * oppositely oriented trims of one edge, even though IsClosed() is
     * false.  The paired seam and two closed shared paths define a strip;
     * an analytic surface gate below supplies a trustworthy parameterization
     * without relying on the malformed source p-curves. */
    const bool explicit_seam_topology = seam_boundaries.size() == 2 &&
	seam_boundaries[0]->m_ei >= 0 &&
	seam_boundaries[0]->m_ei == seam_boundaries[1]->m_ei &&
	seam_boundaries[0]->m_vi[0] == seam_boundaries[1]->m_vi[1] &&
	seam_boundaries[0]->m_vi[1] == seam_boundaries[1]->m_vi[0];
    if (closed_direction < 0 && !explicit_seam_topology)
	return reject("boundary paths do not span a closed or explicit seam");

    for (const boundary_path &boundary : boundaries) {
	const std::vector<repair_fast_trim_sample> &samples =
	    boundary.samples;
	const repair_point_key first = {samples.front().point[X],
	    samples.front().point[Y], samples.front().point[Z]};
	const repair_point_key last = {samples.back().point[X],
	    samples.back().point[Y], samples.back().point[Z]};
	if (first != last)
	    return reject("closed boundary endpoints use different points");
    }

    const char *strip_debug_setting = getenv("BRLCAD_CDT_DEBUG_STRIP");
    const bool debug_strip = strip_debug_setting && strip_debug_setting[0] &&
	!BU_STR_EQUAL(strip_debug_setting, "0");

    const std::vector<repair_fast_trim_sample> &first_samples =
	boundaries[0].samples;
    const std::vector<repair_fast_trim_sample> &second_samples =
	boundaries[1].samples;
    if (first_samples.size() < 4 || second_samples.size() < 4)
	return reject("closed boundary sample counts are too small");
    const size_t first_segments = first_samples.size() - 1;
    const size_t second_segments = second_samples.size() - 1;
    if (debug_strip)
	bu_log("Face %d: periodic strip boundary segments %zu/%zu\n",
	    face_index, first_segments, second_segments);
    if (first_segments > max_points ||
	    second_segments > max_points - first_segments ||
	    first_segments > (size_t)INT_MAX - 2 ||
	    second_segments > (size_t)INT_MAX - 2 - first_segments)
	return reject("boundary strip exceeds the configured point limit");
    if (first_segments > MAX_BOUNDARY_STRIP_CORRESPONDENCE_TESTS /
	    second_segments)
	return reject("boundary correspondence work exceeds its limit");

    /* Invalid p-curves may assign the right edge points to inconsistent
     * periodic images or even traverse the two rings in the same chart
     * direction.  The shared 3-D edge order remains authoritative.  Find the
     * cyclic orientation which keeps corresponding boundary points closest,
     * then give the local strip a synthetic rectangular chart. */
    long double best_score = std::numeric_limits<long double>::infinity();
    size_t best_shift = 0;
    int best_step = 1;

    for (int step : {1, -1}) {
	if (required_step && step != required_step)
	    continue;
	for (size_t shift = 0; shift < second_segments; ++shift) {
	    if (required_shift != std::numeric_limits<size_t>::max() &&
		    shift != required_shift % second_segments)
		continue;
	    long double score = 0.0L;
	    for (size_t point = 0; point < first_segments; ++point) {
		const size_t phase = (size_t)std::llround((long double)point *
		    (long double)second_segments /
		    (long double)first_segments) % second_segments;
		const long signed_index = (long)shift + (long)step *
		    (long)phase;
		const size_t second_index = (size_t)((signed_index %
		    (long)second_segments + (long)second_segments) %
		    (long)second_segments);
		for (int axis = 0; axis < 3; ++axis) {
		    const long double delta =
			(long double)first_samples[point].point[axis] -
			(long double)second_samples[second_index].point[axis];
		    score += delta * delta;
		}
	    }
	    if (score < best_score) {
		best_score = score;
		best_shift = shift;
		best_step = step;
	    }
	}
    }
    if (!std::isfinite(best_score))
	return reject("closed boundary correspondence is non-finite");
    if (debug_strip)
	bu_log("Face %d: periodic strip correspondence shift %zu, step %d, "
	    "score %.17g, explicit seam %d\n", face_index, best_shift,
	    best_step, (double)best_score, explicit_seam_topology);
    const auto corresponding_second = [&](size_t point) {
	const long signed_index = (long)best_shift +
	    (long)best_step * (long)(point % second_segments);
	return (size_t)((signed_index % (long)second_segments +
	    (long)second_segments) % (long)second_segments);
    };

    ON_Torus strip_torus;
    ON_Cylinder strip_cylinder;
    const double analytic_recognition_tolerance = std::min(
	(double)BN_TOL_DIST, 0.25 * (double)allowed_deviation);
    const bool toroidal_seam_strip = explicit_seam_topology &&
	analytic_recognition_tolerance > ON_ZERO_TOLERANCE &&
	surface->IsTorus(&strip_torus, analytic_recognition_tolerance);
    const bool cylindrical_seam_strip = explicit_seam_topology &&
	closed_direction < 0 &&
	!toroidal_seam_strip &&
	analytic_recognition_tolerance > ON_ZERO_TOLERANCE &&
	surface->IsCylinder(&strip_cylinder, analytic_recognition_tolerance);
    const bool generic_closed_seam_strip = explicit_seam_topology &&
	closed_direction < 0 && !toroidal_seam_strip &&
	!cylindrical_seam_strip &&
	(surface->IsClosed(0) || surface->IsClosed(1) ||
	 ON_RevSurface::Cast(surface) != NULL);
    if (closed_direction < 0 && !toroidal_seam_strip &&
	    !cylindrical_seam_strip && !generic_closed_seam_strip)
	return reject("explicit nonperiodic seam is not a recognized analytic "
	    "surface");

    /* A closed surface or revolution may carry an explicit seam while its
     * damaged boundary paths cover only part of the native period.  The
     * synthetic strip chart does not require those paths to wind around the
     * full surface.  Accept the general surface in that case only when every
     * authoritative boundary sample is already within the caller's fidelity
     * allowance; the existing adaptive rows and deviation checks then bound
     * the interpreted interior. */
    if (generic_closed_seam_strip) {
	for (const boundary_path &boundary : boundaries) {
	    for (const repair_fast_trim_sample &sample : boundary.samples) {
		const ON_3dPoint uv_point = surface->PointAt(sample.uv[X],
		    sample.uv[Y]);
		if (!uv_point.IsValid() || uv_point.DistanceTo(
			ON_3dPoint(sample.point)) > allowed_deviation)
		    return reject("shared boundary misses the closed surface");
	    }
	}
    }

    std::vector<double> first_major;
    std::vector<double> first_minor;
    std::vector<double> second_major;
    std::vector<double> second_minor;
    std::vector<double> torus_first_phase;
    std::vector<double> torus_second_phase;
    double torus_first_cross = 0.0;
    double torus_second_cross = 0.0;
    bool torus_phase_is_major = false;
    double torus_orientation_sign = 1.0;
    std::vector<double> cylinder_first_phase;
    std::vector<double> cylinder_second_phase;
    double cylinder_first_height = 0.0;
    double cylinder_second_height = 0.0;
    double cylinder_orientation_sign = 1.0;
    const auto unwrap_angle = [](double angle, double prior) {
	return angle + std::round((prior - angle) / (2.0 * ON_PI)) *
	    (2.0 * ON_PI);
    };
    const auto torus_angles = [&](const ON_3dPoint &point,
	    double &major, double &minor) {
	const ON_3dVector relative = point - strip_torus.Center();
	const double plane_x = relative * strip_torus.plane.xaxis;
	const double plane_y = relative * strip_torus.plane.yaxis;
	const double plane_z = relative * strip_torus.plane.zaxis;
	if (!std::isfinite(plane_x) || !std::isfinite(plane_y) ||
		!std::isfinite(plane_z))
	    return false;
	major = std::atan2(plane_y, plane_x);
	minor = std::atan2(plane_z, std::hypot(plane_x, plane_y) -
	    strip_torus.MajorRadius());
	return std::isfinite(major) && std::isfinite(minor);
    };
    const auto torus_boundary_angles = [&](size_t segment_count,
	    const auto &sample, std::vector<double> &major,
	    std::vector<double> &minor) {
	major.resize(segment_count + 1);
	minor.resize(segment_count + 1);
	for (size_t point = 0; point <= segment_count; ++point) {
	    const ON_3dPoint boundary_point(sample(point).point);
	    if (!torus_angles(boundary_point, major[point], minor[point]))
		return false;
	    const double miss = strip_torus.PointAt(major[point],
		minor[point]).DistanceTo(boundary_point);
	    if (debug_strip && miss > analytic_recognition_tolerance)
		bu_log("Face %d: torus boundary point %zu misses by %.17g "
		    "(limit %.17g, angles %.17g %.17g)\n", face_index,
		    point, miss, analytic_recognition_tolerance, major[point],
		    minor[point]);
	    if (miss > analytic_recognition_tolerance)
		return false;
	    if (point) {
		major[point] = unwrap_angle(major[point], major[point - 1]);
		minor[point] = unwrap_angle(minor[point], minor[point - 1]);
	    }
	}
	return true;
    };
    if (toroidal_seam_strip) {
	const auto sample_torus_boundaries = [&]() {
	    return torus_boundary_angles(first_segments,
		[&](size_t phase) -> const repair_fast_trim_sample & {
		    return first_samples[phase];
		}, first_major, first_minor) &&
		torus_boundary_angles(second_segments,
		[&](size_t phase) -> const repair_fast_trim_sample & {
		    return second_samples[corresponding_second(phase)];
		}, second_major, second_minor);
	};
	if (!sample_torus_boundaries())
	    return reject("shared boundary misses the recognized torus");
	const double angular_tolerance = std::max(1.0e-8,
	    analytic_recognition_tolerance /
	    std::max(strip_torus.MinorRadius(), ON_ZERO_TOLERANCE));
	const auto angle_range = [](const std::vector<double> &angles) {
	    const auto range = std::minmax_element(angles.begin(), angles.end());
	    return *range.second - *range.first;
	};
	const double first_major_winding = first_major.back() -
	    first_major.front();
	const double first_minor_winding = first_minor.back() -
	    first_minor.front();
	const bool major_circle = std::fabs(std::fabs(first_major_winding) -
	    2.0 * ON_PI) <= 0.1 * ON_PI &&
	    angle_range(first_minor) <= angular_tolerance;
	const bool minor_circle = std::fabs(std::fabs(first_minor_winding) -
	    2.0 * ON_PI) <= 0.1 * ON_PI &&
	    angle_range(first_major) <= angular_tolerance;
	if (major_circle == minor_circle)
	    return reject("first torus boundary is not one simple isocircle");
	torus_phase_is_major = major_circle;
	const auto assign_torus_coordinates = [&]() {
	    torus_first_phase = torus_phase_is_major ? first_major :
		first_minor;
	    torus_second_phase = torus_phase_is_major ? second_major :
		second_minor;
	    const std::vector<double> &first_cross = torus_phase_is_major ?
		first_minor : first_major;
	    const std::vector<double> &second_cross = torus_phase_is_major ?
		second_minor : second_major;
	    torus_first_cross = std::accumulate(first_cross.begin(),
		first_cross.end(), 0.0) / (double)first_cross.size();
	    torus_second_cross = std::accumulate(second_cross.begin(),
		second_cross.end(), 0.0) / (double)second_cross.size();
	    torus_second_cross = unwrap_angle(torus_second_cross,
		torus_first_cross);
	    for (double angle : first_cross) {
		if (std::fabs(angle - torus_first_cross) > angular_tolerance)
		    return false;
	    }
	    for (double angle : second_cross) {
		angle = unwrap_angle(angle, torus_second_cross);
		if (std::fabs(angle - torus_second_cross) > angular_tolerance)
		    return false;
	    }
	    torus_second_phase.front() = unwrap_angle(
		torus_second_phase.front(), torus_first_phase.front());
	    for (size_t point = 1; point < torus_second_phase.size(); ++point)
		torus_second_phase[point] = unwrap_angle(
		    torus_second_phase[point], torus_second_phase[point - 1]);
	    return true;
	};
	if (!assign_torus_coordinates())
	    return reject("second torus boundary is not the same isocircle type");
	double first_winding = torus_first_phase.back() -
	    torus_first_phase.front();
	double second_winding = torus_second_phase.back() -
	    torus_second_phase.front();
	if (first_winding * second_winding <= 0.0) {
	    best_step = -best_step;
	    if (!sample_torus_boundaries() || !assign_torus_coordinates())
		return reject("opposite torus boundary traversal is unusable");
	    first_winding = torus_first_phase.back() -
		torus_first_phase.front();
	    second_winding = torus_second_phase.back() -
		torus_second_phase.front();
	}
	if (debug_strip)
	    bu_log("Face %d: analytic torus candidate %s cross "
		"%.17g/%.17g, phase windings %.17g/%.17g\n", face_index,
		torus_phase_is_major ? "minor" : "major",
		torus_first_cross, torus_second_cross, first_winding,
		second_winding);
	if (std::fabs(std::fabs(first_winding) - 2.0 * ON_PI) >
		0.1 * ON_PI || std::fabs(std::fabs(second_winding) -
		2.0 * ON_PI) > 0.1 * ON_PI ||
		first_winding * second_winding <= 0.0)
	    return reject("torus boundary phases do not wind together");
	const double cross_span = torus_second_cross - torus_first_cross;
	/* Two points on a torus admit a short and a long angular route.  The
	 * B-Rep's two distinct seam vertices identify the simple imported patch;
	 * accepting only the route no longer than pi avoids silently choosing a
	 * complementary wrap when its damaged p-curves cannot disambiguate it. */
	if (std::fabs(cross_span) <= angular_tolerance ||
		std::fabs(cross_span) > ON_PI + angular_tolerance)
	    return reject("toroidal seam span is empty or ambiguous");
	ON_3dPoint orientation_point;
	ON_3dVector orientation_normal;
	double orientation_major = 0.0;
	double orientation_minor = 0.0;
	if (!surface_EvNormal(surface, surface->Domain(0).Mid(),
		surface->Domain(1).Mid(), orientation_point,
		orientation_normal) || !orientation_normal.IsValid() ||
		!torus_angles(orientation_point, orientation_major,
		orientation_minor))
	    return reject("toroidal seam has no stable surface orientation");
	if (face.m_bRev)
	    orientation_normal.Reverse();
	const ON_3dVector torus_normal = strip_torus.NormalAt(
	    orientation_major, orientation_minor);
	if (!torus_normal.IsValid() ||
		std::fabs(torus_normal * orientation_normal) <=
		ON_ZERO_TOLERANCE)
	    return reject("toroidal seam orientation is tangent");
	torus_orientation_sign = torus_normal * orientation_normal > 0.0 ?
	    1.0 : -1.0;
	if (debug_strip)
	    bu_log("Face %d: analytic torus strip cross span %.17g, "
		"phase windings %.17g/%.17g\n", face_index, cross_span,
		first_winding, second_winding);
    }

    const auto cylinder_coordinates = [&](const ON_3dPoint &point,
	    double &angle, double &height) {
	if (!strip_cylinder.IsValid() || !point.IsValid())
	    return false;
	const ON_3dVector offset = point -
	    strip_cylinder.circle.plane.origin;
	const double x = offset * strip_cylinder.circle.plane.xaxis;
	const double y = offset * strip_cylinder.circle.plane.yaxis;
	const double radial = std::hypot(x, y);
	height = offset * strip_cylinder.circle.plane.zaxis;
	if (!(radial > 0.0) || !std::isfinite(radial) ||
		!std::isfinite(height))
	    return false;
	angle = std::atan2(y, x);
	return std::isfinite(angle);
    };
    const auto cylinder_boundary_coordinates = [&](size_t segment_count,
	    const auto &sample, std::vector<double> &phase,
	    std::vector<double> &heights) {
	phase.resize(segment_count + 1);
	heights.resize(segment_count + 1);
	for (size_t point = 0; point <= segment_count; ++point) {
	    const ON_3dPoint boundary_point(sample(point).point);
	    if (!cylinder_coordinates(boundary_point, phase[point],
		    heights[point]))
		return false;
	    const double miss = strip_cylinder.PointAt(phase[point],
		heights[point]).DistanceTo(boundary_point);
	    if (debug_strip && miss > analytic_recognition_tolerance)
		bu_log("Face %d: cylinder boundary point %zu misses by "
		    "%.17g (limit %.17g)\n", face_index, point, miss,
		    analytic_recognition_tolerance);
	    if (miss > analytic_recognition_tolerance)
		return false;
	    if (point)
		phase[point] = unwrap_angle(phase[point], phase[point - 1]);
	}
	return true;
    };
    if (cylindrical_seam_strip) {
	std::vector<double> first_heights;
	std::vector<double> second_heights;
	const auto sample_cylinder_boundaries = [&]() {
	    return cylinder_boundary_coordinates(first_segments,
		[&](size_t phase) -> const repair_fast_trim_sample & {
		    return first_samples[phase];
		}, cylinder_first_phase, first_heights) &&
		cylinder_boundary_coordinates(second_segments,
		[&](size_t phase) -> const repair_fast_trim_sample & {
		    return second_samples[corresponding_second(phase)];
		}, cylinder_second_phase, second_heights);
	};
	const auto assign_cylinder_heights = [&]() {
	    cylinder_first_height = std::accumulate(first_heights.begin(),
		first_heights.end(), 0.0) / (double)first_heights.size();
	    cylinder_second_height = std::accumulate(second_heights.begin(),
		second_heights.end(), 0.0) / (double)second_heights.size();
	    for (double height : first_heights) {
		if (std::fabs(height - cylinder_first_height) >
			analytic_recognition_tolerance)
		    return false;
	    }
	    for (double height : second_heights) {
		if (std::fabs(height - cylinder_second_height) >
			analytic_recognition_tolerance)
		    return false;
	    }
	    return true;
	};
	if (!sample_cylinder_boundaries() || !assign_cylinder_heights())
	    return reject("shared boundary misses one cylinder isocircle");
	double first_winding = cylinder_first_phase.back() -
	    cylinder_first_phase.front();
	double second_winding = cylinder_second_phase.back() -
	    cylinder_second_phase.front();
	if (first_winding * second_winding <= 0.0) {
	    best_step = -best_step;
	    if (!sample_cylinder_boundaries() ||
		    !assign_cylinder_heights())
		return reject("opposite cylinder boundary traversal is unusable");
	    first_winding = cylinder_first_phase.back() -
		cylinder_first_phase.front();
	    second_winding = cylinder_second_phase.back() -
		cylinder_second_phase.front();
	}
	if (std::fabs(std::fabs(first_winding) - 2.0 * ON_PI) >
		0.1 * ON_PI || std::fabs(std::fabs(second_winding) -
		2.0 * ON_PI) > 0.1 * ON_PI ||
		first_winding * second_winding <= 0.0)
	    return reject("cylinder boundary phases do not wind together");
	if (std::fabs(cylinder_second_height - cylinder_first_height) <=
		ON_ZERO_TOLERANCE)
	    return reject("cylinder seam strip has no axial span");
	ON_3dPoint orientation_point;
	ON_3dVector orientation_normal;
	double orientation_angle = 0.0;
	double orientation_height = 0.0;
	if (!surface_EvNormal(surface, surface->Domain(0).Mid(),
		surface->Domain(1).Mid(), orientation_point,
		orientation_normal) || !orientation_normal.IsValid() ||
		!cylinder_coordinates(orientation_point, orientation_angle,
		    orientation_height))
	    return reject("cylinder seam has no stable surface orientation");
	if (face.m_bRev)
	    orientation_normal.Reverse();
	const ON_3dVector cylinder_normal = std::cos(orientation_angle) *
	    strip_cylinder.circle.plane.xaxis + std::sin(orientation_angle) *
	    strip_cylinder.circle.plane.yaxis;
	if (!cylinder_normal.IsValid() ||
		std::fabs(cylinder_normal * orientation_normal) <=
		ON_ZERO_TOLERANCE)
	    return reject("cylinder seam orientation is tangent");
	cylinder_orientation_sign = cylinder_normal * orientation_normal > 0.0 ?
	    1.0 : -1.0;
	if (debug_strip)
	    bu_log("Face %d: analytic cylinder strip heights %.17g/%.17g, "
		"phase windings %.17g/%.17g\n", face_index,
		cylinder_first_height, cylinder_second_height, first_winding,
		second_winding);
    }

    const ON_Interval closed_domain = closed_direction >= 0 ?
	surface->Domain(closed_direction) : ON_Interval(0.0, 1.0);
    const double closed_period = closed_direction >= 0 ?
	closed_domain.Length() : 0.0;
    const auto unwrap_boundary = [&](size_t segment_count,
	    const auto &sample) {
	std::vector<ON_2dPoint> result(segment_count + 1);
	for (size_t phase = 0; phase <= segment_count; ++phase) {
	    const repair_fast_trim_sample &source = sample(phase);
	    result[phase] = ON_2dPoint(source.uv[X], source.uv[Y]);
	    if (!phase || closed_direction < 0)
		continue;
	    double &coordinate = closed_direction ? result[phase].y :
		result[phase].x;
	    const double prior = closed_direction ? result[phase - 1].y :
		result[phase - 1].x;
	    coordinate += std::round((prior - coordinate) /
		closed_period) * closed_period;
	}
	return result;
    };
    std::vector<ON_2dPoint> first_uv = unwrap_boundary(first_segments,
	[&](size_t phase) -> const repair_fast_trim_sample & {
	    return first_samples[phase];
	});
    std::vector<ON_2dPoint> second_uv = unwrap_boundary(second_segments,
	[&](size_t phase) -> const repair_fast_trim_sample & {
	    return second_samples[corresponding_second(phase)];
	});
    if (closed_direction >= 0) {
	double &second_closed_start = closed_direction ? second_uv[0].y :
	    second_uv[0].x;
	const double first_closed_start = closed_direction ? first_uv[0].y :
	    first_uv[0].x;
	const double second_shift = std::round((first_closed_start -
	    second_closed_start) / closed_period) * closed_period;
	for (ON_2dPoint &uv : second_uv) {
	    double &coordinate = closed_direction ? uv.y : uv.x;
	    coordinate += second_shift;
	}
    }

    const auto boundary_uv = [](const std::vector<ON_2dPoint> &samples,
	    double phase) {
	if (phase <= 0.0)
	    return samples.front();
	if (phase >= 1.0)
	    return samples.back();
	const double position = phase * (double)(samples.size() - 1);
	const size_t first = std::min(samples.size() - 2,
	    (size_t)std::floor(position));
	const double fraction = position - (double)first;
	return ON_2dPoint((1.0 - fraction) * samples[first].x +
	    fraction * samples[first + 1].x,
	    (1.0 - fraction) * samples[first].y +
	    fraction * samples[first + 1].y);
    };
    const auto boundary_angle = [](const std::vector<double> &samples,
	    double phase) {
	if (phase <= 0.0)
	    return samples.front();
	if (phase >= 1.0)
	    return samples.back();
	const double position = phase * (double)(samples.size() - 1);
	const size_t first = std::min(samples.size() - 2,
	    (size_t)std::floor(position));
	const double fraction = position - (double)first;
	return (1.0 - fraction) * samples[first] +
	    fraction * samples[first + 1];
    };
    const auto torus_parameters = [&](double phase, double across,
	    double &major, double &minor) {
	const double phase_angle = (1.0 - across) *
	    boundary_angle(torus_first_phase, phase) + across *
	    boundary_angle(torus_second_phase, phase);
	const double cross_angle = (1.0 - across) * torus_first_cross +
	    across * torus_second_cross;
	major = torus_phase_is_major ? phase_angle : cross_angle;
	minor = torus_phase_is_major ? cross_angle : phase_angle;
    };
    const auto cylinder_parameters = [&](double phase, double across,
	    double &angle, double &height) {
	angle = (1.0 - across) *
	    boundary_angle(cylinder_first_phase, phase) + across *
	    boundary_angle(cylinder_second_phase, phase);
	height = (1.0 - across) * cylinder_first_height +
	    across * cylinder_second_height;
    };
    const auto wrapped_uv = [&](ON_2dPoint uv) {
	if (closed_direction < 0)
	    return uv;
	double &coordinate = closed_direction ? uv.y : uv.x;
	coordinate = closed_domain.Min() + std::fmod(coordinate -
	    closed_domain.Min(), closed_period);
	if (coordinate < closed_domain.Min())
	    coordinate += closed_period;
	return uv;
    };

    std::set<double> interior_phases;
    for (size_t point = 0; point <= first_segments; ++point)
	interior_phases.insert((double)point / (double)first_segments);
    for (size_t point = 0; point <= second_segments; ++point)
	interior_phases.insert((double)point / (double)second_segments);

    const char *strip_build_failure = NULL;
    const auto build_strip = [&](size_t interior_rows,
	    repair_boundary_patch &candidate, double &maximum_deviation) {
	candidate = repair_boundary_patch();
	maximum_deviation = 0.0;
	strip_build_failure = NULL;
	const size_t outline_count = first_segments + second_segments + 2;
	const size_t interior_count = interior_rows * interior_phases.size();
	if (outline_count > max_points || interior_count >
		max_points - outline_count || outline_count + interior_count >
		(size_t)INT_MAX) {
	    strip_build_failure = "point limit";
	    return false;
	}
	std::vector<detria::PointD> chart_points;
	std::vector<ON_2dPoint> source_uv;
	std::vector<ON_3dPoint> source_points;
	std::vector<ON_3dPoint *> point_sources;
	chart_points.reserve(outline_count + interior_count);
	source_uv.reserve(outline_count + interior_count);
	source_points.reserve(outline_count + interior_count);
	point_sources.reserve(outline_count + interior_count);
	for (size_t point = 0; point <= first_segments; ++point) {
	    const double phase = (double)point / (double)first_segments;
	    chart_points.push_back({phase, 0.0});
	    source_uv.push_back(first_uv[point]);
	    source_points.emplace_back(first_samples[point].point);
	    point_sources.push_back(first_samples[point].source_point);
	}
	for (size_t point = second_segments + 1; point > 0; --point) {
	    const size_t phase_index = point - 1;
	    const double phase = (double)phase_index /
		(double)second_segments;
	    chart_points.push_back({phase, 1.0});
	    source_uv.push_back(second_uv[phase_index]);
	    source_points.emplace_back(second_samples[
		corresponding_second(phase_index)].point);
	    point_sources.push_back(second_samples[
		corresponding_second(phase_index)].source_point);
	}
	for (size_t row = 1; row <= interior_rows; ++row) {
	    const double y = (double)row / (double)(interior_rows + 1);
	    const size_t row_start = source_points.size();
	    for (double phase : interior_phases) {
		const ON_2dPoint lower = boundary_uv(first_uv, phase);
		const ON_2dPoint upper = boundary_uv(second_uv, phase);
		const ON_2dPoint uv((1.0 - y) * lower.x + y * upper.x,
		    (1.0 - y) * lower.y + y * upper.y);
		chart_points.push_back({phase, y});
		source_uv.push_back(uv);
		if (phase >= 1.0 && source_points.size() > row_start) {
		    source_points.push_back(source_points[row_start]);
		    point_sources.push_back(NULL);
		    continue;
		}
		if (toroidal_seam_strip) {
		    double major = 0.0;
		    double minor = 0.0;
		    torus_parameters(phase, y, major, minor);
		    const ON_3dPoint surface_point =
			strip_torus.PointAt(major, minor);
		    if (!surface_point.IsValid()) {
			strip_build_failure = "invalid analytic torus sample";
			return false;
		    }
		    source_points.push_back(surface_point);
		    point_sources.push_back(NULL);
		    continue;
		}
		if (cylindrical_seam_strip) {
		    double angle = 0.0;
		    double height = 0.0;
		    cylinder_parameters(phase, y, angle, height);
		    const ON_3dPoint surface_point =
			strip_cylinder.PointAt(angle, height);
		    if (!surface_point.IsValid()) {
			strip_build_failure = "invalid analytic cylinder sample";
			return false;
		    }
		    source_points.push_back(surface_point);
		    point_sources.push_back(NULL);
		    continue;
		}
		const ON_2dPoint native_uv = wrapped_uv(uv);
		const ON_3dPoint surface_point = surface->PointAt(native_uv.x,
		    native_uv.y);
		if (!surface_point.IsValid()) {
		    strip_build_failure = "invalid interior surface sample";
		    return false;
		}
		source_points.push_back(surface_point);
		point_sources.push_back(NULL);
	    }
	}

	/* Points at phase zero and one lie on the artificial chart seam.  Make
	 * them explicit outline vertices; otherwise detria correctly rejects
	 * them as unconstrained points lying on the two vertical outline edges. */
	std::vector<int> outline;
	outline.reserve(outline_count + 2 * interior_rows);
	for (size_t point = 0; point <= first_segments; ++point)
	    outline.push_back((int)point);
	const size_t interior_phase_count = interior_phases.size();
	for (size_t row = 0; row < interior_rows; ++row)
	    outline.push_back((int)(outline_count + row *
		interior_phase_count + interior_phase_count - 1));
	for (size_t point = 0; point <= second_segments; ++point)
	    outline.push_back((int)(first_segments + 1 + point));
	for (size_t row = interior_rows; row > 0; --row)
	    outline.push_back((int)(outline_count + (row - 1) *
		interior_phase_count));
	detria::Triangulation<detria::PointD, int> triangulation;
	triangulation.setPoints(chart_points);
	triangulation.addOutline(outline);
	try {
	    if (!triangulation.triangulate(true)) {
		strip_build_failure = "CDT rejected interior samples";
		return false;
	    }
	} catch (...) {
	    strip_build_failure = "CDT raised an exception";
	    return false;
	}

	std::map<repair_point_key, int> local_generated_points;
	std::unordered_map<ON_3dPoint *, int> local_source_points;
	std::vector<int> chart_to_local(chart_points.size(), -1);
	for (size_t point = 0; point < source_points.size(); ++point) {
	    const ON_3dPoint &source = source_points[point];
	    ON_3dPoint *source_id = point_sources[point];
	    int existing_index = -1;
	    if (source_id) {
		const auto existing = local_source_points.find(source_id);
		if (existing != local_source_points.end())
		    existing_index = existing->second;
	    } else {
		const repair_point_key key = {source.x, source.y, source.z};
		const auto existing = local_generated_points.find(key);
		if (existing != local_generated_points.end())
		    existing_index = existing->second;
	    }
	    if (existing_index >= 0) {
		chart_to_local[point] = existing_index;
		continue;
	    }
	    const int local_index = (int)(candidate.vertices.size() / 3);
	    if (source_id) {
		local_source_points[source_id] = local_index;
	    } else {
		const repair_point_key key = {source.x, source.y, source.z};
		local_generated_points[key] = local_index;
	    }
	    chart_to_local[point] = local_index;
	    candidate.vertices.push_back(source.x);
	    candidate.vertices.push_back(source.y);
	    candidate.vertices.push_back(source.z);
	    candidate.source_points.push_back(source_id);
	}

	bool valid = true;
	triangulation.forEachTriangle(
	    [&](const detria::Triangle<int> triangle) {
		if (!valid)
		    return;
		const int chart[3] = {triangle.x, triangle.y, triangle.z};
		int local[3] = {chart_to_local[(size_t)triangle.x],
		    chart_to_local[(size_t)triangle.y],
		    chart_to_local[(size_t)triangle.z]};
		if (local[0] == local[1] || local[1] == local[2] ||
			local[2] == local[0]) {
		    strip_build_failure = "collapsed local triangle";
		    valid = false;
		    return;
		}
		ON_3dPoint points[3];
		ON_2dPoint centroid_uv(0.0, 0.0);
		ON_2dPoint centroid_chart(0.0, 0.0);
		for (int corner = 0; corner < 3; ++corner) {
		    points[corner] = ON_3dPoint(&candidate.vertices[
			(size_t)local[corner] * 3]);
		    centroid_uv.x += source_uv[(size_t)chart[corner]].x / 3.0;
		    centroid_uv.y += source_uv[(size_t)chart[corner]].y / 3.0;
		    centroid_chart.x += chart_points[(size_t)chart[corner]].x /
			3.0;
		    centroid_chart.y += chart_points[(size_t)chart[corner]].y /
			3.0;
		}
		const ON_3dVector cross = ON_CrossProduct(points[1] - points[0],
		    points[2] - points[0]);
		const double longest_squared = std::max(
		    points[0].DistanceToSquared(points[1]), std::max(
		    points[1].DistanceToSquared(points[2]),
		    points[2].DistanceToSquared(points[0])));
		if (!(longest_squared > 0.0) || !cross.IsValid() ||
			cross.Length() <= 64.0 *
			std::numeric_limits<double>::epsilon() *
			longest_squared) {
		    strip_build_failure = "geometrically degenerate triangle";
		    valid = false;
		    return;
		}
		ON_3dVector surface_normal;
		if (toroidal_seam_strip) {
		    double major = 0.0;
		    double minor = 0.0;
		    torus_parameters(centroid_chart.x, centroid_chart.y,
			major, minor);
		    surface_normal = torus_orientation_sign *
			strip_torus.NormalAt(major, minor);
		} else if (cylindrical_seam_strip) {
		    double angle = 0.0;
		    double height = 0.0;
		    cylinder_parameters(centroid_chart.x, centroid_chart.y,
			angle, height);
		    surface_normal = cylinder_orientation_sign *
			(std::cos(angle) * strip_cylinder.circle.plane.xaxis +
			 std::sin(angle) * strip_cylinder.circle.plane.yaxis);
		} else {
		    ON_3dPoint normal_point;
		    const ON_2dPoint normal_uv = wrapped_uv(centroid_uv);
		    if (!surface_EvNormal(surface, normal_uv.x, normal_uv.y,
			    normal_point, surface_normal) ||
			    !surface_normal.IsValid()) {
			strip_build_failure = "invalid interior surface normal";
			valid = false;
			return;
		    }
		    if (face.m_bRev)
			surface_normal.Reverse();
		}
		if (!surface_normal.IsValid()) {
		    strip_build_failure = "invalid reconstructed surface normal";
		    valid = false;
		    return;
		}
		if (cross * surface_normal < 0.0)
		    std::swap(local[1], local[2]);

		const double weights[4][3] = {
		    {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0},
		    {0.5, 0.5, 0.0}, {0.0, 0.5, 0.5},
		    {0.5, 0.0, 0.5}
		};
		double triangle_deviation = 0.0;
		for (int sample = 0; sample < 4; ++sample) {
		    ON_2dPoint uv(0.0, 0.0);
		    ON_2dPoint chart_sample(0.0, 0.0);
		    ON_3dPoint mesh_point(0.0, 0.0, 0.0);
		    for (int corner = 0; corner < 3; ++corner) {
			uv.x += weights[sample][corner] *
			    source_uv[(size_t)chart[corner]].x;
			uv.y += weights[sample][corner] *
			    source_uv[(size_t)chart[corner]].y;
			chart_sample.x += weights[sample][corner] *
			    chart_points[(size_t)chart[corner]].x;
			chart_sample.y += weights[sample][corner] *
			    chart_points[(size_t)chart[corner]].y;
			mesh_point.x += weights[sample][corner] * points[corner].x;
			mesh_point.y += weights[sample][corner] * points[corner].y;
			mesh_point.z += weights[sample][corner] * points[corner].z;
		    }
		    ON_3dPoint surface_point;
		    if (toroidal_seam_strip) {
			double major = 0.0;
			double minor = 0.0;
			torus_parameters(chart_sample.x, chart_sample.y,
			    major, minor);
			surface_point = strip_torus.PointAt(major, minor);
		    } else if (cylindrical_seam_strip) {
			double angle = 0.0;
			double height = 0.0;
			cylinder_parameters(chart_sample.x, chart_sample.y,
			    angle, height);
			surface_point = strip_cylinder.PointAt(angle, height);
		    } else {
			const ON_2dPoint native_uv = wrapped_uv(uv);
			surface_point = surface->PointAt(native_uv.x, native_uv.y);
		    }
		    if (!surface_point.IsValid()) {
			strip_build_failure = "invalid surface deviation sample";
			valid = false;
			return;
		    }
		    double deviation = mesh_point.DistanceTo(surface_point);
		    if (toroidal_seam_strip || cylindrical_seam_strip)
			deviation += analytic_recognition_tolerance;
		    triangle_deviation = std::max(triangle_deviation,
			deviation);
		}
		maximum_deviation = std::max(maximum_deviation,
		    triangle_deviation);
		candidate.faces.insert(candidate.faces.end(), local, local + 3);
		candidate.direct_surface_deviations.push_back(
		    triangle_deviation);
	    }, true);
	const size_t boundary_count = outline_count + 2 * interior_rows;
	const size_t interior_point_count = interior_count -
	    2 * interior_rows;
	const size_t expected_faces = boundary_count +
	    2 * interior_point_count - 2;
	if (!valid || candidate.faces.size() / 3 != expected_faces) {
	    if (!strip_build_failure)
		strip_build_failure = "incomplete interior triangulation";
	    return false;
	}
	for (const boundary_path &boundary : boundaries) {
	    candidate.constrained_edges += boundary.constrained_edges;
	    candidate.constrained_samples += boundary.constrained_samples;
	}
	candidate.strip_interior_rows = interior_rows;
	candidate.strip_correspondence_step = best_step;
	candidate.strip_correspondence_shift = best_shift;
	candidate.analytic_torus_strip = toroidal_seam_strip;
	candidate.analytic_cylinder_strip = cylindrical_seam_strip;
	return true;
    };

    repair_boundary_patch best_patch;
    double best_deviation = std::numeric_limits<double>::infinity();
    for (size_t interior_rows = 0; interior_rows <= 255;
	    interior_rows = 2 * interior_rows + 1) {
	if (deadline > 0 && bu_gettime() >= deadline)
	    break;
	repair_boundary_patch candidate;
	double maximum_deviation = 0.0;
	if (!build_strip(interior_rows, candidate, maximum_deviation)) {
	    if (getenv("BRLCAD_CDT_DEBUG_STRIP") &&
		    getenv("BRLCAD_CDT_DEBUG_STRIP")[0] &&
		    !BU_STR_EQUAL(getenv("BRLCAD_CDT_DEBUG_STRIP"), "0"))
		bu_log("Face %d: periodic strip refinement failed with %zu "
		    "interior rows: %s\n", face_index, interior_rows,
		    strip_build_failure ? strip_build_failure : "unknown reason");
	    if (!interior_rows)
		return reject("chart triangulation failed");
	    break;
	}
	if (getenv("BRLCAD_CDT_DEBUG_STRIP") &&
		getenv("BRLCAD_CDT_DEBUG_STRIP")[0] &&
		!BU_STR_EQUAL(getenv("BRLCAD_CDT_DEBUG_STRIP"), "0"))
	    bu_log("Face %d: periodic strip %zu interior rows, %zu points, "
		"%zu triangles, maximum deviation %.17g (limit %.17g)\n",
		face_index, interior_rows, candidate.vertices.size() / 3,
		candidate.faces.size() / 3, maximum_deviation,
		(double)allowed_deviation);
	if (maximum_deviation < best_deviation) {
	    best_deviation = maximum_deviation;
	    best_patch = std::move(candidate);
	}
	if (best_deviation <= allowed_deviation) {
	    patch = std::move(best_patch);
	    return true;
	}
    }
    if (best_patch.faces.empty())
	return reject("triangulated strip is incomplete or degenerate");
    patch = std::move(best_patch);
    return true;
}

/* Build a disk-topology spherical patch from one exact closed boundary ring.
 * The ring need not follow the sphere's native parameter poles.  Concentric
 * spherical rows therefore avoid both a planar projection and a global chart
 * whose artificial pole cut may cross the requested cap. */
static bool
repair_spherical_cap_mesh(const ON_BrepFace &face, const ON_Sphere &sphere,
	const std::vector<repair_fast_trim_sample> &boundary,
	size_t max_points, fastf_t allowed_deviation,
	fastf_t recognized_surface_deviation, int64_t deadline,
	repair_boundary_patch &patch,
	const ON_3dVector *retained_axis_hint = NULL)
{
    patch = repair_boundary_patch();
    const ON_Surface *surface = face.SurfaceOf();
    const ON_3dPoint center = sphere.Center();
    const double radius = sphere.Radius();
    const size_t segments = boundary.size();
    const char *debug_setting = getenv("BRLCAD_CDT_DEBUG_CAP");
    const bool debug_cap = debug_setting && debug_setting[0] &&
	!BU_STR_EQUAL(debug_setting, "0");
    const auto reject = [&](const char *reason) {
	if (debug_cap)
	    bu_log("spherical cap rejected: %s\n", reason);
	return false;
    };
    if (!surface || segments < 3 || !(radius > ON_ZERO_TOLERANCE) ||
	    !std::isfinite(radius) || !(allowed_deviation > 0.0) ||
	    !std::isfinite(allowed_deviation) ||
	    recognized_surface_deviation < 0.0 ||
	    !std::isfinite(recognized_surface_deviation))
	return reject("invalid surface, sphere, boundary, or tolerance");
    max_points = std::min(max_points,
	(size_t)MAX_BOUNDARY_STRIP_REFINEMENT_POINTS);
    if (max_points <= segments + 1)
	return reject("point limit smaller than boundary");

    std::vector<ON_3dPoint> boundary_points(segments);
    for (size_t point = 0; point < segments; ++point) {
	boundary_points[point] = ON_3dPoint(boundary[point].point);
	if (!boundary_points[point].IsValid() ||
		std::fabs(boundary_points[point].DistanceTo(center) - radius) >
		allowed_deviation - recognized_surface_deviation)
	    return reject("boundary point misses recognized sphere");
    }

    ON_3dVector phase_axis = ON_3dVector::UnsetVector;
    ON_3dVector retained_axis = ON_3dVector::UnsetVector;
    double plane_offset = std::numeric_limits<double>::quiet_NaN();
    if (retained_axis_hint) {
	/* The B-Rep's singular vertex selects the physical pole.  This permits
	 * a nonplanar but star-shaped spherical boundary while avoiding any
	 * best-fit-plane interpretation. */
	retained_axis = *retained_axis_hint;
	phase_axis = retained_axis;
	retained_axis.Unitize();
	bool pole_on_boundary = false;
	for (const ON_3dPoint &point : boundary_points) {
	    ON_3dVector radial = point - center;
	    radial.Unitize();
	    pole_on_boundary = pole_on_boundary ||
		(radial - retained_axis).Length() <=
		256.0 * std::numeric_limits<double>::epsilon();
	}
	if (pole_on_boundary) {
	    /* Some importers create a second topology vertex at the singular
	     * pole and also put that same 3-D point on the physical boundary.
	     * It cannot serve as an interior fan apex.  For the resulting
	     * hemisphere-like polygon, the exact boundary's oriented spherical
	     * area supplies a nonaxial interior direction. */
	    ON_3dVector oriented_area(0.0, 0.0, 0.0);
	    for (size_t point = 0; point < segments; ++point) {
		const ON_3dVector first = boundary_points[point] - center;
		const ON_3dVector second =
		    boundary_points[(point + 1) % segments] - center;
		oriented_area += ON_CrossProduct(first, second);
	    }
	    if (!oriented_area.IsValid() ||
		    oriented_area.Length() <= ON_ZERO_TOLERANCE)
		return reject("pole-boundary cap has no oriented area");
	    oriented_area.Unitize();
	    retained_axis = oriented_area;
	    phase_axis = retained_axis;
	}
    } else {
	ON_3dVector oriented_area(0.0, 0.0, 0.0);
	for (size_t point = 0; point < segments; ++point) {
	    const ON_3dVector first = boundary_points[point] - center;
	    const ON_3dVector second =
		boundary_points[(point + 1) % segments] - center;
	    oriented_area += ON_CrossProduct(first, second);
	}
	if (!oriented_area.IsValid() ||
		oriented_area.Length() <= ON_ZERO_TOLERANCE)
	    return reject("boundary has no stable oriented area");
	ON_3dVector plane_normal = oriented_area;
	plane_normal.Unitize();
	plane_offset = 0.0;
	for (const ON_3dPoint &point : boundary_points)
	    plane_offset += (point - center) * plane_normal;
	plane_offset /= (double)segments;
	/* The plane is used to decide which side of the ring is absent.  Keep
	 * that classification tied to source-recognition accuracy rather than
	 * allowing the (potentially much larger) output chord tolerance to turn
	 * a visibly warped ring into a nominal cap. */
	const double plane_tolerance = std::max((double)BN_TOL_DIST,
	    (double)recognized_surface_deviation);
	if (std::fabs(plane_offset) >= radius - plane_tolerance)
	    return reject("boundary plane does not define a bounded cap");
	for (const ON_3dPoint &point : boundary_points) {
	    if (std::fabs((point - center) * plane_normal - plane_offset) >
		    plane_tolerance)
		return reject("boundary points are not coplanar");
	}
	/* An inner ring removes the cap on its plane-offset side.  A great-circle
	 * ring has no offset side; its exact loop orientation selects one of the
	 * two otherwise geometrically equivalent hemispheres. */
	retained_axis = std::fabs(plane_offset) <= plane_tolerance ?
	    plane_normal : (plane_offset > 0.0 ? -plane_normal : plane_normal);
	phase_axis = plane_normal;
    }
    if (!retained_axis.IsValid() ||
	    retained_axis.Length() <= ON_ZERO_TOLERANCE)
	return reject("retained cap direction is invalid");
    retained_axis.Unitize();
    phase_axis.Unitize();

    const double axial_tolerance = std::max((double)ON_ZERO_TOLERANCE,
	256.0 * std::numeric_limits<double>::epsilon() * radius);
    size_t phase_start = segments;
    ON_3dVector phase_x = ON_3dVector::UnsetVector;
    for (size_t point = 0; point < segments; ++point) {
	ON_3dVector transverse = boundary_points[point] - center;
	transverse -= (transverse * phase_axis) * phase_axis;
	if (transverse.Length() > axial_tolerance) {
	    phase_start = point;
	    phase_x = transverse;
	    break;
	}
    }
    if (phase_start == segments || !phase_x.IsValid())
	return reject("boundary phase basis is singular");
    phase_x.Unitize();
    ON_3dVector phase_y = ON_CrossProduct(phase_axis, phase_x);
    phase_y.Unitize();
    double previous_angle = std::atan2(
	(boundary_points[phase_start] - center) * phase_y,
	(boundary_points[phase_start] - center) * phase_x);
    double winding = 0.0;
    int winding_direction = 0;
    for (size_t offset = 1; offset <= segments; ++offset) {
	const size_t point = (phase_start + offset) % segments;
	const ON_3dVector radial =
	    boundary_points[point] - center;
	ON_3dVector transverse = radial - (radial * phase_axis) * phase_axis;
	/* Longitude is undefined at either pole.  Preserve the exact boundary
	 * point, but omit it from the phase-order certificate. */
	if (transverse.Length() <= axial_tolerance)
	    continue;
	double angle = std::atan2(radial * phase_y, radial * phase_x);
	double delta = angle - previous_angle;
	while (delta <= -ON_PI)
	    delta += 2.0 * ON_PI;
	while (delta > ON_PI)
	    delta -= 2.0 * ON_PI;
	/* A valid spherical polygon may contain a meridional boundary edge.
	 * Its consecutive points have the same phase about the retained pole;
	 * they still form a nondegenerate spherical chord and do not reverse the
	 * ring's winding. */
	if (std::fabs(delta) <= ON_ZERO_TOLERANCE) {
	    previous_angle = angle;
	    continue;
	}
	const int direction = delta > 0.0 ? 1 : -1;
	if (winding_direction && direction != winding_direction)
	    return reject("boundary phase order reverses");
	winding_direction = direction;
	winding += delta;
	previous_angle = angle;
    }
    if (std::fabs(std::fabs(winding) - 2.0 * ON_PI) > 0.1 * ON_PI)
	return reject("boundary does not wind once around its selected axis");
    if (debug_cap)
	bu_log("spherical cap: %zu boundary samples, %s %.17g, winding "
	    "%.17g\n", segments, retained_axis_hint ? "pole selected" :
	    "plane offset", retained_axis_hint ? 1.0 : plane_offset, winding);

    ON_3dPoint orientation_point;
    ON_3dVector orientation_normal;
    if (!surface_EvNormal(surface, surface->Domain(0).Mid(),
	    surface->Domain(1).Mid(), orientation_point,
	    orientation_normal) || !orientation_normal.IsValid())
	return reject("source surface has no usable orientation normal");
    if (face.m_bRev)
	orientation_normal.Reverse();
    const double orientation_dot =
	(orientation_point - center) * orientation_normal;
    if (std::fabs(orientation_dot) <= ON_ZERO_TOLERANCE)
	return reject("source surface orientation is tangent to sphere");
    const double orientation_sign = orientation_dot > 0.0 ? 1.0 : -1.0;

    const auto build_cap = [&](size_t interior_rows,
	    repair_boundary_patch &candidate, double &maximum_deviation) {
	candidate = repair_boundary_patch();
	maximum_deviation = 0.0;
	if (interior_rows > (max_points - 1) / segments ||
		segments > max_points - interior_rows * segments - 1)
	    return false;
	std::vector<ON_3dPoint> points;
	points.reserve(segments * (interior_rows + 1) + 1);
	points.insert(points.end(), boundary_points.begin(),
	    boundary_points.end());
	for (size_t row = 1; row <= interior_rows; ++row) {
	    const double fraction = (double)row /
		(double)(interior_rows + 1);
	    for (const ON_3dPoint &boundary_point : boundary_points) {
		ON_3dVector boundary_radial = boundary_point - center;
		boundary_radial.Unitize();
		const double cosine = std::max(-1.0, std::min(1.0,
		    boundary_radial * retained_axis));
		const double angle = std::acos(cosine);
		const double sine = std::sin(angle);
		if (!(angle > ON_ZERO_TOLERANCE) ||
			angle >= ON_PI - ON_ZERO_TOLERANCE ||
			std::fabs(sine) <= ON_ZERO_TOLERANCE)
		    return false;
		ON_3dVector radial =
		    (std::sin((1.0 - fraction) * angle) / sine) *
		    boundary_radial + (std::sin(fraction * angle) / sine) *
		    retained_axis;
		if (!radial.IsValid() || radial.Length() <= ON_ZERO_TOLERANCE)
		    return false;
		radial.Unitize();
		points.push_back(center + radius * radial);
	    }
	}
	const int apex = (int)points.size();
	points.push_back(center + radius * retained_axis);
	candidate.vertices.reserve(points.size() * 3);
	for (const ON_3dPoint &point : points) {
	    candidate.vertices.push_back(point.x);
	    candidate.vertices.push_back(point.y);
	    candidate.vertices.push_back(point.z);
	}
	const auto append_triangle = [&](int first, int second, int third) {
	    int triangle[3] = {first, second, third};
	    ON_3dPoint triangle_points[3] = {points[(size_t)first],
		points[(size_t)second], points[(size_t)third]};
	    ON_3dVector cross = ON_CrossProduct(
		triangle_points[1] - triangle_points[0],
		triangle_points[2] - triangle_points[0]);
	    const double longest_squared = std::max(
		triangle_points[0].DistanceToSquared(triangle_points[1]),
		std::max(triangle_points[1].DistanceToSquared(
		triangle_points[2]), triangle_points[2].DistanceToSquared(
		triangle_points[0])));
	    if (!(longest_squared > 0.0) || !cross.IsValid() ||
		    cross.Length() <= 64.0 *
		    std::numeric_limits<double>::epsilon() * longest_squared)
		return false;
	    const ON_3dPoint centroid = (triangle_points[0] +
		triangle_points[1] + triangle_points[2]) / 3.0;
	    const ON_3dVector expected = orientation_sign *
		(centroid - center);
	    if (!expected.IsValid() || expected.Length() <= ON_ZERO_TOLERANCE)
		return false;
	    if (cross * expected < 0.0) {
		std::swap(triangle[1], triangle[2]);
		std::swap(triangle_points[1], triangle_points[2]);
	    }
	    const double weights[4][3] = {
		{1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0},
		{0.5, 0.5, 0.0}, {0.0, 0.5, 0.5},
		{0.5, 0.0, 0.5}
	    };
	    double triangle_deviation = 0.0;
	    for (int sample = 0; sample < 4; ++sample) {
		ON_3dPoint mesh_point(0.0, 0.0, 0.0);
		for (int corner = 0; corner < 3; ++corner) {
		    mesh_point.x += weights[sample][corner] *
			triangle_points[corner].x;
		    mesh_point.y += weights[sample][corner] *
			triangle_points[corner].y;
		    mesh_point.z += weights[sample][corner] *
			triangle_points[corner].z;
		}
		triangle_deviation = std::max(triangle_deviation,
		    std::fabs(mesh_point.DistanceTo(center) - radius) +
		    recognized_surface_deviation);
	    }
	    maximum_deviation = std::max(maximum_deviation,
		triangle_deviation);
	    candidate.faces.insert(candidate.faces.end(), triangle,
		triangle + 3);
	    candidate.direct_surface_deviations.push_back(
		triangle_deviation);
	    return true;
	};
	for (size_t row = 0; row < interior_rows; ++row) {
	    const size_t lower = row * segments;
	    const size_t upper = (row + 1) * segments;
	    for (size_t point = 0; point < segments; ++point) {
		const size_t next = (point + 1) % segments;
		if (!append_triangle((int)(lower + point),
			(int)(lower + next), (int)(upper + next)) ||
			!append_triangle((int)(lower + point),
			(int)(upper + next), (int)(upper + point)))
		    return false;
	    }
	}
	const size_t last_ring = interior_rows * segments;
	for (size_t point = 0; point < segments; ++point) {
	    const size_t next = (point + 1) % segments;
	    if (!append_triangle((int)(last_ring + point),
		    (int)(last_ring + next), apex))
		return false;
	}
	candidate.strip_interior_rows = interior_rows;
	return true;
    };

    repair_boundary_patch best_patch;
    double best_deviation = std::numeric_limits<double>::infinity();
    for (size_t interior_rows = 0; interior_rows <= 255;
	    interior_rows = 2 * interior_rows + 1) {
	if (deadline > 0 && bu_gettime() >= deadline)
	    break;
	repair_boundary_patch candidate;
	double maximum_deviation = 0.0;
	if (!build_cap(interior_rows, candidate, maximum_deviation))
	{
	    if (debug_cap)
		bu_log("spherical cap: %zu interior rows failed\n",
		    interior_rows);
	    continue;
	}
	if (debug_cap)
	    bu_log("spherical cap: %zu interior rows, %zu triangles, "
		"maximum deviation %.17g (limit %.17g)\n", interior_rows,
		candidate.faces.size() / 3, maximum_deviation,
		(double)allowed_deviation);
	if (maximum_deviation < best_deviation) {
	    best_deviation = maximum_deviation;
	    best_patch = std::move(candidate);
	}
	if (best_deviation <= allowed_deviation) {
	    patch = std::move(best_patch);
	    return true;
	}
    }
    if (best_patch.faces.empty())
	return false;
    patch = std::move(best_patch);
    return true;
}

/* Reconstruct a complete analytic sphere as two spherical caps sharing one
 * generated equator.  This is reserved for a closed, single-face component
 * whose only B-Rep edge is an artificial pole-to-pole seam; no neighboring
 * face boundary is discarded. */
static bool
repair_full_spherical_mesh(const ON_BrepFace &face, const ON_Sphere &sphere,
	const ON_3dVector &pole_axis, size_t max_points,
	fastf_t allowed_deviation, fastf_t recognized_surface_deviation,
	int64_t deadline, repair_boundary_patch &patch)
{
    patch = repair_boundary_patch();
    const double radius = sphere.Radius();
    const double available_deviation = allowed_deviation -
	recognized_surface_deviation;
    max_points = std::min(max_points,
	(size_t)MAX_BOUNDARY_STRIP_REFINEMENT_POINTS);
    if (!(radius > ON_ZERO_TOLERANCE) || !std::isfinite(radius) ||
	    !(available_deviation > 0.0) || max_points < 18)
	return false;

    ON_3dVector axis = pole_axis;
    if (!axis.IsValid() || axis.Length() <= ON_ZERO_TOLERANCE)
	return false;
    axis.Unitize();
    ON_3dVector reference;
    if (std::fabs(axis.x) <= std::fabs(axis.y) &&
	    std::fabs(axis.x) <= std::fabs(axis.z))
	reference = ON_3dVector::XAxis;
    else if (std::fabs(axis.y) <= std::fabs(axis.z))
	reference = ON_3dVector::YAxis;
    else
	reference = ON_3dVector::ZAxis;
    ON_3dVector equator_x = ON_CrossProduct(axis, reference);
    if (!equator_x.Unitize())
	return false;
    ON_3dVector equator_y = ON_CrossProduct(axis, equator_x);
    if (!equator_y.Unitize())
	return false;

    const double target_deviation = std::min(radius,
	0.25 * available_deviation);
    const double half_angle = std::acos(std::max(-1.0,
	std::min(1.0, 1.0 - target_deviation / radius)));
    if (!(half_angle > 0.0) || !std::isfinite(half_angle))
	return false;
    size_t segments = std::max((size_t)8,
	(size_t)std::ceil(ON_PI / half_angle));
    if (segments > max_points - 2)
	return false;

    std::vector<repair_fast_trim_sample> equator(segments);
    for (size_t point = 0; point < segments; ++point) {
	const double angle = 2.0 * ON_PI * (double)point /
	    (double)segments;
	const ON_3dPoint sphere_point = sphere.Center() + radius *
	    (std::cos(angle) * equator_x + std::sin(angle) * equator_y);
	VSET(equator[point].point, sphere_point.x, sphere_point.y,
	    sphere_point.z);
	equator[point].parameter = angle;
	V2SET(equator[point].uv, 0.0, 0.0);
    }

    const size_t cap_point_limit = (max_points + segments) / 2;
    repair_boundary_patch first;
    repair_boundary_patch second;
    ON_3dVector opposite_axis = -axis;
    if (!repair_spherical_cap_mesh(face, sphere, equator,
	    cap_point_limit, allowed_deviation,
	    recognized_surface_deviation, deadline, first, &axis) ||
	    !repair_spherical_cap_mesh(face, sphere, equator,
	    cap_point_limit, allowed_deviation,
	    recognized_surface_deviation, deadline, second,
	    &opposite_axis))
	return false;
    const size_t first_points = first.vertices.size() / 3;
    const size_t second_points = second.vertices.size() / 3;
    if (first_points < segments + 1 || second_points < segments + 1 ||
	    first_points + second_points - segments > max_points)
	return false;

    const size_t interior_rows = std::max(first.strip_interior_rows,
	second.strip_interior_rows);
    patch = std::move(first);
    std::vector<int> second_to_patch(second_points, -1);
    for (size_t point = 0; point < segments; ++point)
	second_to_patch[point] = (int)point;
    for (size_t point = segments; point < second_points; ++point) {
	second_to_patch[point] = (int)(patch.vertices.size() / 3);
	patch.vertices.insert(patch.vertices.end(),
	    second.vertices.begin() + (ptrdiff_t)(point * 3),
	    second.vertices.begin() + (ptrdiff_t)(point * 3 + 3));
    }
    patch.faces.reserve(patch.faces.size() + second.faces.size());
    for (int point : second.faces) {
	if (point < 0 || (size_t)point >= second_to_patch.size())
	    return false;
	patch.faces.push_back(second_to_patch[(size_t)point]);
    }
    patch.direct_surface_deviations.insert(
	patch.direct_surface_deviations.end(),
	second.direct_surface_deviations.begin(),
	second.direct_surface_deviations.end());
    patch.strip_interior_rows = interior_rows;
    return !patch.faces.empty() &&
	patch.direct_surface_deviations.size() == patch.faces.size() / 3;
}

static bool
repair_closed_spherical_surface(struct ON_Brep_CDT_State *s_cdt,
	int face_index, size_t max_points, fastf_t allowed_deviation,
	int64_t deadline, repair_boundary_patch &patch)
{
    patch = repair_boundary_patch();
    if (!s_cdt || !s_cdt->brep || face_index < 0 ||
	    face_index >= s_cdt->brep->m_F.Count() ||
	    !(allowed_deviation > 0.0))
	return false;
    const ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    const ON_Surface *surface = face.SurfaceOf();
    const ON_BrepLoop *outer = face.OuterLoop();
    if (!surface || face.LoopCount() != 1 || !outer ||
	    outer->TrimCount() != 2)
	return false;
    const ON_BrepTrim *first = outer->Trim(0);
    const ON_BrepTrim *second = outer->Trim(1);
    if (!first || !second || first->m_type != ON_BrepTrim::seam ||
	    second->m_type != ON_BrepTrim::seam || first->m_ei < 0 ||
	    first->m_ei != second->m_ei ||
	    first->m_vi[0] != second->m_vi[1] ||
	    first->m_vi[1] != second->m_vi[0] ||
	    first->m_vi[0] == first->m_vi[1])
	return false;
    const ON_BrepEdge *edge = first->Edge();
    if (!edge || edge->TrimCount() != 2 || second->Edge() != edge)
	return false;
    for (int trim_index = 0; trim_index < edge->TrimCount(); ++trim_index) {
	const ON_BrepTrim *trim = edge->Trim(trim_index);
	if (!trim || trim->Face() != &face)
	    return false;
    }

    ON_Sphere sphere;
    const double sphere_tolerance = std::min((double)BN_TOL_DIST,
	0.25 * (double)allowed_deviation);
    if (!(sphere_tolerance > ON_ZERO_TOLERANCE) ||
	    !surface->IsSphere(&sphere, sphere_tolerance))
	return false;
    const ON_BrepVertex *first_vertex = edge->Vertex(0);
    const ON_BrepVertex *second_vertex = edge->Vertex(1);
    if (!first_vertex || !second_vertex)
	return false;
    const ON_3dPoint first_pole = first_vertex->Point();
    const ON_3dPoint second_pole = second_vertex->Point();
    const double topology_tolerance = std::min((double)allowed_deviation,
	std::max(sphere_tolerance, std::max((double)edge->m_tolerance,
	std::max((double)first_vertex->Tolerance(),
	(double)second_vertex->Tolerance()))));
    if (!first_pole.IsValid() || !second_pole.IsValid() ||
	    std::fabs(first_pole.DistanceTo(sphere.Center()) -
	    sphere.Radius()) > topology_tolerance ||
	    std::fabs(second_pole.DistanceTo(sphere.Center()) -
	    sphere.Radius()) > topology_tolerance ||
	    ((first_pole + second_pole) / 2.0).DistanceTo(sphere.Center()) >
	    topology_tolerance)
	return false;

    const ON_3dVector pole_axis = first_pole - sphere.Center();
    if (!repair_full_spherical_mesh(face, sphere, pole_axis, max_points,
	    allowed_deviation, sphere_tolerance, deadline, patch))
	return false;
    patch.constrained_edges = 1;
    patch.constrained_samples = 2;
    return true;
}

static bool
repair_constrained_spherical_cap(struct ON_Brep_CDT_State *s_cdt,
	int face_index, const repair_fast_constraint_store &constraints,
	size_t max_points, fastf_t allowed_deviation, int64_t deadline,
	repair_boundary_patch &patch)
{
    patch = repair_boundary_patch();
    const char *debug_setting = getenv("BRLCAD_CDT_DEBUG_CAP");
    const bool debug_cap = debug_setting && debug_setting[0] &&
	!BU_STR_EQUAL(debug_setting, "0");
    if (!s_cdt || !s_cdt->brep || face_index < 0 ||
	    face_index >= s_cdt->brep->m_F.Count() ||
	    constraints.face_index != face_index || constraints.trims.empty()) {
	if (debug_cap && s_cdt && s_cdt->brep &&
		face_index >= 0 && face_index < s_cdt->brep->m_F.Count())
	    bu_log("Face %d: spherical cap has no constrained trim samples\n",
		face_index);
	return false;
	}
    const ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface || (face.LoopCount() != 1 && face.LoopCount() != 2))
	return false;
    ON_Sphere sphere;
    const double sphere_tolerance = std::min((double)BN_TOL_DIST,
	0.25 * (double)allowed_deviation);
    if (!(sphere_tolerance > ON_ZERO_TOLERANCE) ||
	    !surface->IsSphere(&sphere, sphere_tolerance)) {
	if (debug_cap)
	    bu_log("Face %d: constrained cap surface is not a recognized "
		"sphere\n", face_index);
	return false;
	}

    const ON_BrepLoop *outer = face.OuterLoop();
    if (!outer)
	return false;

    std::vector<repair_fast_trim_sample> boundary;
    const auto append_boundary_trim = [&](const ON_BrepTrim *trim) {
	const auto samples = trim ? constraints.trims.find(
	    trim->m_trim_index) : constraints.trims.end();
	if (!trim || samples == constraints.trims.end() ||
		samples->second.size() < 2) {
	    if (debug_cap)
		bu_log("Face %d: spherical cap missing constrained trim %d\n",
		    face_index, trim ? trim->m_trim_index : -1);
	    return false;
	}
	if (boundary.empty()) {
	    boundary = samples->second;
	    return true;
	}
	const repair_point_key prior = {boundary.back().point[X],
	    boundary.back().point[Y], boundary.back().point[Z]};
	const repair_point_key next = {samples->second.front().point[X],
	    samples->second.front().point[Y],
	    samples->second.front().point[Z]};
	if (prior != next) {
	    if (debug_cap)
		bu_log("Face %d: spherical cap trim %d does not join prior "
		    "constraint\n", face_index, trim->m_trim_index);
	    return false;
	}
	boundary.insert(boundary.end(), samples->second.begin() + 1,
	    samples->second.end());
	return true;
    };

    ON_3dVector retained_axis_hint = ON_3dVector::UnsetVector;
    bool have_retained_axis_hint = false;
    if (face.LoopCount() == 2) {
	const ON_BrepLoop *inner = NULL;
	for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	    const ON_BrepLoop *loop = face.Loop(loop_index);
	    if (!loop)
		return false;
	    if (loop == outer) {
		for (int trim_index = 0; trim_index < loop->TrimCount();
			trim_index++) {
		    const ON_BrepTrim *trim = loop->Trim(trim_index);
		    if (!trim || trim->m_type != ON_BrepTrim::seam ||
			    constraints.trims.find(trim->m_trim_index) !=
			    constraints.trims.end())
			return false;
		}
		continue;
	    }
	    if (loop->m_type != ON_BrepLoop::inner || inner)
		return false;
	    inner = loop;
	}
	if (!inner || outer->TrimCount() < 2 || inner->TrimCount() < 1 ||
		constraints.trims.size() != (size_t)inner->TrimCount())
	    return false;
	for (int trim_index = 0; trim_index < inner->TrimCount(); ++trim_index) {
	    if (!append_boundary_trim(inner->Trim(trim_index)))
		return false;
	}
	} else {
	const int trim_count = outer->TrimCount();
	if (trim_count < 1)
	    return false;
	bool physical_ring_with_singular_gaps = !constraints.trims.empty();
	for (int trim_index = 0; trim_index < trim_count; ++trim_index) {
	    const ON_BrepTrim *trim = outer->Trim(trim_index);
	    if (trim && constraints.trims.find(trim->m_trim_index) !=
		    constraints.trims.end())
		continue;
	    physical_ring_with_singular_gaps = trim &&
		trim->m_type == ON_BrepTrim::singular && trim->m_ei < 0 &&
		trim->m_vi[0] == trim->m_vi[1];
	    if (!physical_ring_with_singular_gaps)
		break;
	}
	if (physical_ring_with_singular_gaps) {
	    /* A wholly physical spherical boundary does not need a chart.  Some
	     * importers insert zero-length singular trims between its real edges;
	     * those carry no 3-D boundary geometry.  Preserve every authoritative
	     * shared-edge sample in loop order and let the exact joined ring select
	     * the bounded analytic cap. */
	    for (int trim_index = 0; trim_index < trim_count; ++trim_index) {
		const ON_BrepTrim *trim = outer->Trim(trim_index);
		if (trim && constraints.trims.find(trim->m_trim_index) ==
			constraints.trims.end())
		    continue;
		if (!append_boundary_trim(trim))
		    return false;
	    }
	} else {
	    /* A spherical face may express a physical closed ring and then make
	     * an artificial excursion from that ring to a pole and back.
	     * Recognize exactly one constrained ring followed by seam, singular,
	     * seam trims; the singular vertex chooses the intended cap. */
	    if (trim_count < 4 ||
		    constraints.trims.size() >= (size_t)trim_count)
		return false;
	    int constrained_start = -1;
	    int constrained_starts = 0;
	    for (int trim_index = 0; trim_index < trim_count; ++trim_index) {
		const ON_BrepTrim *trim = outer->Trim(trim_index);
		const ON_BrepTrim *prior = outer->Trim(
		    (trim_index + trim_count - 1) % trim_count);
		if (!trim || !prior)
		    return false;
		const bool constrained = constraints.trims.find(
		    trim->m_trim_index) != constraints.trims.end();
		const bool prior_constrained = constraints.trims.find(
		    prior->m_trim_index) != constraints.trims.end();
		if (constrained && !prior_constrained) {
		    constrained_start = trim_index;
		    constrained_starts++;
		}
	    }
	    if (constrained_starts != 1) {
		if (debug_cap)
		    bu_log("Face %d: spherical cap has %d constrained runs among "
			"%d trims (%zu constrained)\n", face_index,
			constrained_starts, trim_count,
			constraints.trims.size());
		return false;
	    }
	    std::vector<const ON_BrepTrim *> artificial;
	    bool reached_artificial = false;
	    size_t constrained_count = 0;
	    for (int offset = 0; offset < trim_count; ++offset) {
		const ON_BrepTrim *trim = outer->Trim(
		    (constrained_start + offset) % trim_count);
		const bool constrained = trim && constraints.trims.find(
		    trim->m_trim_index) != constraints.trims.end();
		if (constrained) {
		    if (reached_artificial || !append_boundary_trim(trim))
			return false;
		    constrained_count++;
		} else {
		    reached_artificial = true;
		    artificial.push_back(trim);
		}
	    }
	    if (constrained_count != constraints.trims.size() ||
		    artificial.size() != 3 || !artificial[0] ||
		    !artificial[1] || !artificial[2] ||
		    artificial[0]->m_type != ON_BrepTrim::seam ||
		    artificial[1]->m_type != ON_BrepTrim::singular ||
		    artificial[2]->m_type != ON_BrepTrim::seam ||
		    artificial[0]->m_ei < 0 ||
		    artificial[0]->m_ei != artificial[2]->m_ei ||
		    artificial[1]->m_ei >= 0 ||
		    artificial[1]->m_vi[0] != artificial[1]->m_vi[1] ||
		    artificial[0]->m_vi[1] != artificial[1]->m_vi[0] ||
		    artificial[2]->m_vi[0] != artificial[1]->m_vi[0] ||
		    artificial[0]->m_vi[0] != artificial[2]->m_vi[1]) {
		if (debug_cap)
		    bu_log("Face %d: spherical cap topology rejected (%zu "
			"ring, %zu constrained, %zu artificial)\n",
			face_index, constrained_count, constraints.trims.size(),
			artificial.size());
		return false;
	    }
	    const int pole_index = artificial[1]->m_vi[0];
	    if (pole_index < 0 || pole_index >= s_cdt->brep->m_V.Count())
		return false;
	    const ON_BrepVertex &pole_vertex = s_cdt->brep->m_V[pole_index];
	    const ON_3dPoint pole = pole_vertex.Point();
	    const double vertex_tolerance = pole_vertex.Tolerance() >= 0.0 &&
		std::isfinite(pole_vertex.Tolerance()) ?
		pole_vertex.Tolerance() : 0.0;
	    const double pole_tolerance = std::min((double)allowed_deviation,
		std::max(sphere_tolerance, vertex_tolerance));
	    if (!pole.IsValid() ||
		    std::fabs(pole.DistanceTo(sphere.Center()) -
		    sphere.Radius()) > pole_tolerance)
		return false;
	    retained_axis_hint = pole - sphere.Center();
	    have_retained_axis_hint = true;
	}
    }
    if (boundary.size() < 4)
	return false;
    const repair_point_key first = {boundary.front().point[X],
	boundary.front().point[Y], boundary.front().point[Z]};
    const repair_point_key last = {boundary.back().point[X],
	boundary.back().point[Y], boundary.back().point[Z]};
    if (first != last)
	return false;
    boundary.pop_back();
    if (!repair_spherical_cap_mesh(face, sphere, boundary, max_points,
	    allowed_deviation, sphere_tolerance, deadline, patch,
	    have_retained_axis_hint ? &retained_axis_hint : NULL))
	return false;
    patch.constrained_edges = constraints.constrained_edges;
    patch.constrained_samples = constraints.constrained_samples;
    return true;
}

/* A face covering an entire doubly periodic surface has no trustworthy
 * exterior polygon to project.  Its periodic topology is nevertheless
 * unambiguous: sample one shared-index tensor grid, omit both duplicate end
 * rows, and wrap each row and column.  Adaptive chord and cell-center checks
 * determine the grid density.  This path is intentionally limited to faces
 * with no non-seam trims and verified coincident parameter boundaries, so it
 * cannot erase a real trimmed boundary. */
static bool
repair_closed_periodic_surface(struct ON_Brep_CDT_State *s_cdt,
	int face_index, size_t max_points, fastf_t allowed_deviation,
	int64_t deadline, repair_boundary_patch &patch)
{
    patch = repair_boundary_patch();
    if (!s_cdt || !s_cdt->brep || face_index < 0 ||
	    face_index >= s_cdt->brep->m_F.Count() || max_points < 64 ||
	    !(allowed_deviation > 0.0) ||
	    !std::isfinite(allowed_deviation))
	return false;

    const ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    const ON_Surface *surface = face.SurfaceOf();
    const char *debug_setting = getenv("BRLCAD_CDT_DEBUG_GRID");
    const bool debug_grid = debug_setting && debug_setting[0] &&
	!BU_STR_EQUAL(debug_setting, "0");
    const auto reject = [&](const char *reason) {
	if (debug_grid)
	    bu_log("Face %d: periodic grid rejected: %s\n", face_index,
		reason);
	return false;
    };
    if (!surface || face.LoopCount() > 1)
	return reject("missing surface or multiple loops");
    int seam_trims = 0;
    for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	const ON_BrepLoop *loop = face.Loop(loop_index);
	if (!loop)
	    return reject("missing loop");
	for (int trim_index = 0; trim_index < loop->TrimCount(); ++trim_index) {
	    const ON_BrepTrim *trim = loop->Trim(trim_index);
	    if (!trim || trim->m_type != ON_BrepTrim::seam)
		return reject("face has a non-seam trim");
	    seam_trims++;
	}
    }
    if (face.LoopCount() && seam_trims < 4)
	return reject("fewer than four seam trims");
    ON_Interval domains[2] = {surface->Domain(0), surface->Domain(1)};
    std::vector<double> seam_sides[2];
    for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	const ON_BrepLoop *loop = face.Loop(loop_index);
	for (int trim_index = 0; loop && trim_index < loop->TrimCount();
		++trim_index) {
	    const ON_BrepTrim *trim = loop->Trim(trim_index);
	    const ON_Interval trim_domain = trim->Domain();
	    const ON_2dPoint first = trim->PointAt(trim_domain.Min());
	    const ON_2dPoint last = trim->PointAt(trim_domain.Max());
	    if (!first.IsValid() || !last.IsValid())
		return reject("invalid seam endpoints");
	    const double delta[2] = {std::fabs(last.x - first.x),
		std::fabs(last.y - first.y)};
	    if (delta[0] <= 0.01 * std::max(delta[1], ON_ZERO_TOLERANCE))
		seam_sides[0].push_back(0.5 * (first.x + last.x));
	    if (delta[1] <= 0.01 * std::max(delta[0], ON_ZERO_TOLERANCE))
		seam_sides[1].push_back(0.5 * (first.y + last.y));
	}
    }
    for (int direction = 0; direction < 2; ++direction) {
	if (surface->IsClosed(direction))
	    continue;
	if (seam_sides[direction].size() < 2)
	    return reject("seam trims do not define two parameter sides");
	const auto extrema = std::minmax_element(seam_sides[direction].begin(),
	    seam_sides[direction].end());
	domains[direction] = ON_Interval(*extrema.first, *extrema.second);
    }
    if (!(domains[0].Length() > ON_ZERO_TOLERANCE) ||
	    !(domains[1].Length() > ON_ZERO_TOLERANCE) ||
	    !std::isfinite(domains[0].Length()) ||
	    !std::isfinite(domains[1].Length()))
	return reject("invalid surface domains");

    /* Some imported NURBS surfaces have four explicit seam trims but do not
     * set OpenNURBS' intrinsic closed flags.  Treat the topology as periodic
     * only after independently sampling both pairs of parameter boundaries. */
    const double closure_tolerance = std::min((double)allowed_deviation,
	(double)BN_TOL_DIST);
    for (int direction = 0; direction < 2; ++direction) {
	if (surface->IsClosed(direction))
	    continue;
	const int other = 1 - direction;
	for (int sample = 0; sample <= 32; ++sample) {
	    const double other_parameter = domains[other].ParameterAt(
		(double)sample / 32.0);
	    const ON_3dPoint minimum = direction == 0 ? surface->PointAt(
		domains[0].Min(), other_parameter) : surface->PointAt(
		other_parameter, domains[1].Min());
	    const ON_3dPoint maximum = direction == 0 ? surface->PointAt(
		domains[0].Max(), other_parameter) : surface->PointAt(
		other_parameter, domains[1].Max());
	    if (!minimum.IsValid() || !maximum.IsValid() ||
		    minimum.DistanceTo(maximum) > closure_tolerance) {
		if (debug_grid)
		    bu_log("Face %d: direction %d closure miss %.17g at "
			"sample %d (limit %.17g)\n", face_index, direction,
			minimum.DistanceTo(maximum), sample,
			closure_tolerance);
		return reject("parameter boundaries do not coincide");
	    }
	}
    }

    size_t u_samples = 8;
    size_t v_samples = 8;
    const double target_error = 0.25 * allowed_deviation;
    std::vector<ON_3dPoint> points;
    const auto parameter = [&](int direction, size_t sample,
	    size_t count) {
	return domains[direction].ParameterAt((double)sample / (double)count);
    };
    const auto midpoint_error = [](const ON_3dPoint &first,
	    const ON_3dPoint &second, const ON_3dPoint &actual) {
	const ON_3dPoint chord(0.5 * (first.x + second.x),
	    0.5 * (first.y + second.y), 0.5 * (first.z + second.z));
	return chord.DistanceTo(actual);
    };

    bool converged = false;
    for (int attempt = 0; attempt < 16; ++attempt) {
	if ((deadline > 0 && bu_gettime() >= deadline) ||
		u_samples > max_points / v_samples)
	    return false;
	points.resize(u_samples * v_samples);
	for (size_t v = 0; v < v_samples; ++v) {
	    for (size_t u = 0; u < u_samples; ++u) {
		ON_3dPoint point = surface->PointAt(parameter(0, u,
		    u_samples), parameter(1, v, v_samples));
		if (!point.IsValid())
		    return false;
		points[v * u_samples + u] = point;
	    }
	}

	double u_error = 0.0;
	double v_error = 0.0;
	double center_error = 0.0;
	for (size_t v = 0; v < v_samples; ++v) {
	    const size_t next_v = (v + 1) % v_samples;
	    for (size_t u = 0; u < u_samples; ++u) {
		const size_t next_u = (u + 1) % u_samples;
		const ON_3dPoint &p00 = points[v * u_samples + u];
		const ON_3dPoint &p10 = points[v * u_samples + next_u];
		const ON_3dPoint &p01 = points[next_v * u_samples + u];
		const ON_3dPoint &p11 =
		    points[next_v * u_samples + next_u];
		const ON_3dPoint u_mid = surface->PointAt(parameter(0,
		    2 * u + 1, 2 * u_samples), parameter(1, v,
		    v_samples));
		const ON_3dPoint v_mid = surface->PointAt(parameter(0, u,
		    u_samples), parameter(1, 2 * v + 1,
		    2 * v_samples));
		const ON_3dPoint center = surface->PointAt(parameter(0,
		    2 * u + 1, 2 * u_samples), parameter(1, 2 * v + 1,
		    2 * v_samples));
		if (!u_mid.IsValid() || !v_mid.IsValid() || !center.IsValid())
		    return false;
		u_error = std::max(u_error, midpoint_error(p00, p10, u_mid));
		v_error = std::max(v_error, midpoint_error(p00, p01, v_mid));
		const ON_3dPoint bilinear(
		    0.25 * (p00.x + p10.x + p01.x + p11.x),
		    0.25 * (p00.y + p10.y + p01.y + p11.y),
		    0.25 * (p00.z + p10.z + p01.z + p11.z));
		center_error = std::max(center_error,
		    bilinear.DistanceTo(center));
	    }
	    if (deadline > 0 && bu_gettime() >= deadline)
		return false;
	}
	if (u_error <= target_error && v_error <= target_error &&
		center_error <= target_error) {
	    converged = true;
	    break;
	}
	const bool refine_center = center_error > target_error;
	const bool refine_u = u_error > target_error || refine_center;
	const bool refine_v = v_error > target_error || refine_center;
	if ((refine_u && u_samples > max_points / 2) ||
		(refine_v && v_samples > max_points / 2))
	    return false;
	const size_t next_u = refine_u ? 2 * u_samples : u_samples;
	const size_t next_v = refine_v ? 2 * v_samples : v_samples;
	if (next_u > max_points / next_v)
	    return false;
	u_samples = next_u;
	v_samples = next_v;
    }

    if (!converged || points.size() > (size_t)INT_MAX ||
	    points.size() > (size_t)INT_MAX / 2)
	return reject("adaptive sampling did not converge within its limits");

    patch.vertices.reserve(points.size() * 3);
    for (const ON_3dPoint &point : points) {
	patch.vertices.push_back(point.x);
	patch.vertices.push_back(point.y);
	patch.vertices.push_back(point.z);
    }
    patch.faces.reserve(points.size() * 6);
    for (size_t v = 0; v < v_samples; ++v) {
	const size_t next_v = (v + 1) % v_samples;
	for (size_t u = 0; u < u_samples; ++u) {
	    const size_t next_u = (u + 1) % u_samples;
	    int triangles[2][3] = {
		{(int)(v * u_samples + u),
		 (int)(v * u_samples + next_u),
		 (int)(next_v * u_samples + next_u)},
		{(int)(v * u_samples + u),
		 (int)(next_v * u_samples + next_u),
		 (int)(next_v * u_samples + u)}
	    };
	    ON_3dPoint surface_point;
	    ON_3dVector surface_normal;
	    if (!surface_EvNormal(surface, parameter(0, 2 * u + 1,
		    2 * u_samples), parameter(1, 2 * v + 1,
		    2 * v_samples), surface_point, surface_normal) ||
		    !surface_normal.IsValid())
		return false;
	    if (face.m_bRev)
		surface_normal.Reverse();
	    for (int triangle_index = 0; triangle_index < 2;
		    ++triangle_index) {
		int *triangle = triangles[triangle_index];
		double triangle_uv[3][2];
		const double cell_uv[4][2] = {
		    {parameter(0, u, u_samples),
		     parameter(1, v, v_samples)},
		    {parameter(0, u + 1, u_samples),
		     parameter(1, v, v_samples)},
		    {parameter(0, u + 1, u_samples),
		     parameter(1, v + 1, v_samples)},
		    {parameter(0, u, u_samples),
		     parameter(1, v + 1, v_samples)}
		};
		const int uv_indices[2][3] = {{0, 1, 2}, {0, 2, 3}};
		for (int corner = 0; corner < 3; ++corner) {
		    triangle_uv[corner][0] =
			cell_uv[uv_indices[triangle_index][corner]][0];
		    triangle_uv[corner][1] =
			cell_uv[uv_indices[triangle_index][corner]][1];
		}
		const ON_3dPoint &first = points[(size_t)triangle[0]];
		const ON_3dPoint &second = points[(size_t)triangle[1]];
		const ON_3dPoint &third = points[(size_t)triangle[2]];
		const ON_3dVector cross = ON_CrossProduct(second - first,
		    third - first);
		const double longest_squared = std::max(
		    first.DistanceToSquared(second), std::max(
		    second.DistanceToSquared(third),
		    third.DistanceToSquared(first)));
		if (!(longest_squared > 0.0) || !cross.IsValid() ||
			cross.Length() <= 64.0 *
			std::numeric_limits<double>::epsilon() *
			longest_squared)
		    return false;
		if (cross * surface_normal < 0.0) {
		    std::swap(triangle[1], triangle[2]);
		    std::swap(triangle_uv[1][0], triangle_uv[2][0]);
		    std::swap(triangle_uv[1][1], triangle_uv[2][1]);
		}
		const double weights[4][3] = {
		    {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0},
		    {0.5, 0.5, 0.0}, {0.0, 0.5, 0.5},
		    {0.5, 0.0, 0.5}
		};
		double direct_deviation = 0.0;
		for (int sample = 0; sample < 4; ++sample) {
		    double uv[2] = {0.0, 0.0};
		    ON_3dPoint mesh_sample(0.0, 0.0, 0.0);
		    for (int corner = 0; corner < 3; ++corner) {
			uv[0] += weights[sample][corner] *
			    triangle_uv[corner][0];
			uv[1] += weights[sample][corner] *
			    triangle_uv[corner][1];
			const ON_3dPoint &point =
			    points[(size_t)triangle[corner]];
			mesh_sample.x += weights[sample][corner] * point.x;
			mesh_sample.y += weights[sample][corner] * point.y;
			mesh_sample.z += weights[sample][corner] * point.z;
		    }
		    const ON_3dPoint source_sample = surface->PointAt(uv[0],
			uv[1]);
		    if (!source_sample.IsValid())
			return false;
		    direct_deviation = std::max(direct_deviation,
			mesh_sample.DistanceTo(source_sample));
		}
		if (direct_deviation > allowed_deviation)
		    return false;
		patch.faces.insert(patch.faces.end(), triangle,
		    triangle + 3);
		patch.direct_surface_deviations.push_back(direct_deviation);
	    }
	    if (deadline > 0 && bu_gettime() >= deadline)
		return false;
	}
    }

    assembled_mesh_validation validation;
    if (!assembled_mesh_validate((int)points.size(),
	    (int)(patch.faces.size() / 3), patch.vertices.data(),
	    patch.faces.data(), &validation, false) ||
	    validation.invalid_indices || validation.nonfinite_vertices ||
	    validation.unused_vertices || validation.degenerate_faces ||
	    validation.invalid_vertex_links || bg_trimesh_solid2(
	    (int)points.size(), (int)(patch.faces.size() / 3),
	    patch.vertices.data(), patch.faces.data(), NULL)) {
	patch = repair_boundary_patch();
	return reject("wrapped grid did not form a valid indexed solid");
    }
    patch.periodic_u_samples = u_samples;
    patch.periodic_v_samples = v_samples;
    return true;
}

static int
repair_topological_disk_contract(void)
{
    ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(4.0, 0.0, 0.0),
	ON_3dPoint(4.0, 3.0, 0.0), ON_3dPoint(0.0, 3.0, 0.0),
	ON_3dPoint(0.0, 0.0, 2.0), ON_3dPoint(4.0, 0.0, 2.0),
	ON_3dPoint(4.0, 3.0, 2.0), ON_3dPoint(0.0, 3.0, 2.0)
    };
    std::unique_ptr<ON_Brep> source(ON_BrepBox(corners));
    if (!source || source->m_F.Count() < 1)
	return 1;
    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(source.get(),
	"topological disk contract");
    if (state)
	state->brep = new ON_Brep(*source);
    if (!state || !state->brep) {
	ON_Brep_CDT_Destroy(state);
	return 2;
    }
    const ON_BrepFace &face = state->brep->m_F[0];
    const ON_BrepLoop *outer = face.OuterLoop();
    const ON_Surface *surface = face.SurfaceOf();
    if (!outer || !surface || outer->TrimCount() < 3) {
	ON_Brep_CDT_Destroy(state);
	return 3;
    }

    repair_fast_constraint_store constraints;
    constraints.face_index = 0;
    for (int trim_index = 0; trim_index < outer->TrimCount();
	    ++trim_index) {
	const ON_BrepTrim *trim = outer->Trim(trim_index);
	if (!trim) {
	    ON_Brep_CDT_Destroy(state);
	    return 4;
	}
	std::vector<repair_fast_trim_sample> &samples =
	    constraints.trims[trim->m_trim_index];
	const ON_Interval domain = trim->Domain();
	for (int sample_index = 0; sample_index < 3; ++sample_index) {
	    repair_fast_trim_sample sample;
	    sample.parameter = domain.ParameterAt(0.5 * sample_index);
	    ON_2dPoint uv = trim->PointAt(sample.parameter);
	    const ON_3dPoint point = surface->PointAt(uv.x, uv.y);
	    /* Interior pullbacks are deliberately unusable as a planar polygon.
	     * The exact 3-D boundary and its B-Rep loop order remain sufficient. */
	    if (sample_index == 1) {
		uv.x += trim_index % 2 ? 100.0 : -100.0;
		uv.y += trim_index % 2 ? -75.0 : 75.0;
	    }
	    V2SET(sample.uv, uv.x, uv.y);
	    VSET(sample.point, point.x, point.y, point.z);
	    samples.push_back(sample);
	}
	constraints.constrained_edges++;
	constraints.constrained_samples += samples.size();
    }

    repair_boundary_patch patch;
    bool valid = repair_constrained_topological_disk(state, 0, constraints,
	64, patch);
    const size_t point_count = patch.vertices.size() / 3;
    const size_t face_count = patch.faces.size() / 3;
    valid = valid && point_count >= 4 && face_count + 1 == point_count &&
	patch.constrained_edges == (size_t)outer->TrimCount() &&
	patch.constrained_samples == (size_t)outer->TrimCount() * 3;
    std::map<repair_triangle_edge_key, int> edge_uses;
    const int center = (int)point_count - 1;
    for (size_t triangle = 0; valid && triangle < face_count; ++triangle) {
	bool has_center = false;
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = patch.faces[triangle * 3 + corner];
	    valid = vertex >= 0 && (size_t)vertex < point_count;
	    has_center = has_center || vertex == center;
	    const int next = patch.faces[triangle * 3 + (corner + 1) % 3];
	    if (!valid || next < 0 || (size_t)next >= point_count)
		break;
	    repair_point_key first = {
		patch.vertices[(size_t)vertex * 3],
		patch.vertices[(size_t)vertex * 3 + 1],
		patch.vertices[(size_t)vertex * 3 + 2]
	    };
	    repair_point_key second = {
		patch.vertices[(size_t)next * 3],
		patch.vertices[(size_t)next * 3 + 1],
		patch.vertices[(size_t)next * 3 + 2]
	    };
	    edge_uses[repair_triangle_edge(first, second)]++;
	}
	valid = valid && has_center;
    }
    size_t boundary_edges = 0;
    size_t interior_edges = 0;
    for (const auto &edge : edge_uses) {
	boundary_edges += edge.second == 1 ? 1 : 0;
	interior_edges += edge.second == 2 ? 1 : 0;
	valid = valid && (edge.second == 1 || edge.second == 2);
    }
    valid = valid && boundary_edges == face_count &&
	interior_edges == face_count;

    repair_fast_constraint_store incomplete = constraints;
    incomplete.trims.erase(outer->Trim(0)->m_trim_index);
    repair_boundary_patch rejected;
    valid = valid && !repair_constrained_topological_disk(state, 0,
	incomplete, 64, rejected);
    ON_Brep_CDT_Destroy(state);
    return valid ? 0 : 5;
}

int
cdt_test_repair_periodic_strip(void)
{
    const int disk_contract = repair_topological_disk_contract();
    if (disk_contract)
	return 20 + disk_contract;
    const ON_Cylinder cylinder(ON_Circle(ON_xy_plane, 5.0), 2.0);
    std::unique_ptr<ON_Brep> source(ON_BrepCylinder(cylinder, true, true));
    if (!source || !source->IsValid() || !source->IsSolid())
	return 1;
    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(source.get(),
	"periodic strip contract");
    if (state)
	state->brep = new ON_Brep(*source);
    if (!state || !state->brep) {
	ON_Brep_CDT_Destroy(state);
	return 2;
    }

    int side_index = -1;
    for (int face_index = 0; face_index < state->brep->m_F.Count();
	    ++face_index) {
	const ON_BrepFace &candidate = state->brep->m_F[face_index];
	const ON_BrepLoop *loop = candidate.OuterLoop();
	int seams = 0;
	for (int trim = 0; loop && trim < loop->TrimCount(); ++trim)
	    seams += loop->Trim(trim) && loop->Trim(trim)->m_type ==
		ON_BrepTrim::seam ? 1 : 0;
	if (seams >= 2) {
	    side_index = face_index;
	    break;
	}
    }
    if (side_index < 0) {
	ON_Brep_CDT_Destroy(state);
	return 3;
    }

    const ON_BrepFace &side = state->brep->m_F[side_index];
    const ON_BrepLoop *outer = side.OuterLoop();
    const auto make_constraints = [&](int first_segments,
	    int later_segments) {
	repair_fast_constraint_store result;
	result.face_index = side_index;
	const ON_Surface *surface = side.SurfaceOf();
	for (int trim_index = 0; outer && trim_index < outer->TrimCount();
		++trim_index) {
	    const ON_BrepTrim *trim = outer->Trim(trim_index);
	    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	    if (!trim || !edge || !edge->IsClosed() ||
		    trim->m_type == ON_BrepTrim::seam)
		continue;
	    const int segment_count = result.trims.empty() ?
		first_segments : later_segments;
	    std::vector<repair_fast_trim_sample> &samples =
		result.trims[trim->m_trim_index];
	    const ON_Interval domain = trim->Domain();
	    for (int sample_index = 0; sample_index <= segment_count;
		    sample_index++) {
		repair_fast_trim_sample sample;
		sample.parameter = domain.ParameterAt(
		    (double)sample_index / (double)segment_count);
		const ON_2dPoint uv = trim->PointAt(sample.parameter);
		const ON_3dPoint point = surface->PointAt(uv.x, uv.y);
		V2SET(sample.uv, uv.x, uv.y);
		VSET(sample.point, point.x, point.y, point.z);
		if (sample_index == segment_count)
		    VMOVE(sample.point, samples.front().point);
		samples.push_back(sample);
	    }
	    result.constrained_edges++;
	    result.constrained_samples += samples.size();
	}
	return result;
    };
    repair_fast_constraint_store constraints = make_constraints(8, 10);

    repair_boundary_patch patch;
    const bool reconstructed = repair_constrained_periodic_strip(state,
	side_index, constraints, 64, 10.0, 0, patch);
    bool valid = reconstructed && patch.faces.size() == 18 * 3 &&
	patch.vertices.size() == 18 * 3 &&
	patch.constrained_edges == 2 && patch.constrained_samples == 20;
    for (size_t face = 0; valid && face < patch.faces.size() / 3; ++face) {
	ON_3dPoint points[3];
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = patch.faces[face * 3 + (size_t)corner];
	    valid = vertex >= 0 &&
		(size_t)vertex < patch.vertices.size() / 3;
	    if (!valid)
		break;
	    points[corner] = ON_3dPoint(
		&patch.vertices[(size_t)vertex * 3]);
	}
	valid = valid && ON_CrossProduct(points[1] - points[0],
	    points[2] - points[0]).Length() > ON_ZERO_TOLERANCE;
    }
    const ON_Surface *old_surface = side.SurfaceOf();
    if (!valid || side.m_si < 0 || !old_surface) {
	ON_Brep_CDT_Destroy(state);
	return 4;
    }
    ON_RevSurface *barrel = ON_RevSurface::Cast(
	state->brep->m_S[side.m_si]);
    if (!barrel || !barrel->m_curve) {
	ON_Brep_CDT_Destroy(state);
	return 4;
    }
    /* Model the imported case whose analytic cylinder is cut just short of
     * its nominal full revolution.  Its paired seam topology and exact
     * neighboring edge samples remain authoritative even though IsClosed()
     * no longer describes the surface parameterization. */
    const ON_Interval full_angle = barrel->m_angle;
    barrel->m_angle.Set(full_angle.Min(), full_angle.Max() - 1.0e-6);
    barrel->DestroyRuntimeCache(true);
    repair_boundary_patch explicit_cylinder_patch;
    const bool explicit_cylinder = !old_surface->IsClosed(0) &&
	old_surface->IsCylinder(NULL, 0.0005) &&
	repair_constrained_periodic_strip(state, side_index, constraints, 64,
	    0.06, 0, explicit_cylinder_patch);
    valid = explicit_cylinder &&
	explicit_cylinder_patch.analytic_cylinder_strip &&
	explicit_cylinder_patch.faces.size() == 18 * 3 &&
	explicit_cylinder_patch.vertices.size() == 18 * 3 &&
	explicit_cylinder_patch.constrained_edges == 2 &&
	explicit_cylinder_patch.constrained_samples == 20;
    barrel->m_angle = full_angle;
    barrel->DestroyRuntimeCache(true);
    if (!valid) {
	ON_Brep_CDT_Destroy(state);
	return 5;
    }
    const ON_Interval height_domain = old_surface->Domain(1);
    ON_NurbsCurve *profile = ON_NurbsCurve::New(3, false, 3, 3);
    profile->MakeClampedUniformKnotVector(1.0);
    profile->SetCV(0, ON_3dPoint(5.0, 0.0, 0.0));
    profile->SetCV(1, ON_3dPoint(8.0, 0.0, 1.0));
    profile->SetCV(2, ON_3dPoint(5.0, 0.0, 2.0));
    profile->SetDomain(height_domain.Min(), height_domain.Max());
    delete barrel->m_curve;
    barrel->m_curve = profile;
    barrel->DestroyRuntimeCache(true);
    constraints = make_constraints(64, 64);
    repair_fast_constraint_store partial_constraints = constraints;
    for (auto &trim_entry : partial_constraints.trims) {
	std::vector<repair_fast_trim_sample> &samples = trim_entry.second;
	const size_t segment_count = samples.size() - 1;
	const double constant_v = samples.front().uv[Y];
	const ON_Interval angular_domain = barrel->Domain(0);
	for (size_t point = 0; point <= segment_count; ++point) {
	    const double fraction = point <= segment_count / 2 ?
		2.0 * (double)point / (double)segment_count :
		2.0 * (double)(segment_count - point) /
		(double)segment_count;
	    const double u = angular_domain.ParameterAt(0.5 * fraction);
	    const ON_3dPoint surface_point = barrel->PointAt(u, constant_v);
	    V2SET(samples[point].uv, u, constant_v);
	    VSET(samples[point].point, surface_point.x, surface_point.y,
		surface_point.z);
	}
    }
    repair_boundary_patch partial_patch;
    const bool partial_revolution = repair_constrained_periodic_strip(state,
	side_index, partial_constraints, 4096, 10.0, 0, partial_patch);
    valid = partial_revolution && !partial_patch.analytic_torus_strip &&
	!partial_patch.analytic_cylinder_strip &&
	partial_patch.constrained_edges == 2 &&
	partial_patch.constrained_samples == 130 &&
	partial_patch.faces.size() == 128 * 3;
    if (!valid) {
	ON_Brep_CDT_Destroy(state);
	return 6;
    }
    repair_boundary_patch refined_patch;
    const bool refined = repair_constrained_periodic_strip(state,
	side_index, constraints, 4096, 0.04, 0, refined_patch);
    valid = refined && refined_patch.strip_interior_rows > 0 &&
	refined_patch.constrained_edges == 2 &&
	refined_patch.constrained_samples == 130 &&
	refined_patch.faces.size() > 126 * 3 &&
	refined_patch.direct_surface_deviations.size() ==
	refined_patch.faces.size() / 3;
    for (double deviation : refined_patch.direct_surface_deviations)
	valid = valid && std::isfinite(deviation) && deviation <= 0.04;
    ON_Brep_CDT_Destroy(state);
    if (!valid)
	return 6;

    const ON_Sphere cap_sphere(ON_3dPoint::Origin, 1.0);
    std::unique_ptr<ON_Brep> cap_brep(ON_BrepSphere(cap_sphere));
    if (!cap_brep || cap_brep->m_F.Count() != 1)
	return 6;
    std::vector<repair_fast_trim_sample> cap_boundary(32);
    const double cap_plane = -0.2;
    const double cap_radius = std::sqrt(1.0 - cap_plane * cap_plane);
    for (size_t point = 0; point < cap_boundary.size(); ++point) {
	const double angle = 2.0 * ON_PI * (double)point /
	    (double)cap_boundary.size();
	VSET(cap_boundary[point].point, cap_radius * std::cos(angle),
	    cap_plane, cap_radius * std::sin(angle));
    }
    repair_boundary_patch cap_patch;
    const bool cap_reconstructed = repair_spherical_cap_mesh(
	cap_brep->m_F[0], cap_sphere, cap_boundary, 4096, 0.01, 0.0, 0,
	cap_patch);
    valid = cap_reconstructed && cap_patch.strip_interior_rows > 0 &&
	cap_patch.vertices.size() / 3 == cap_boundary.size() *
	(cap_patch.strip_interior_rows + 1) + 1 &&
	cap_patch.faces.size() / 3 == cap_boundary.size() *
	(2 * cap_patch.strip_interior_rows + 1) &&
	cap_patch.direct_surface_deviations.size() ==
	cap_patch.faces.size() / 3;
    for (double deviation : cap_patch.direct_surface_deviations)
	valid = valid && std::isfinite(deviation) && deviation <= 0.01;
    if (!valid)
	return 6;

    std::vector<repair_fast_trim_sample> pole_boundary(32);
    for (size_t point = 0; point < pole_boundary.size(); ++point) {
	double phase = 2.0 * ON_PI * (double)point /
	    (double)pole_boundary.size();
	if (point == 8)
	    phase = 2.0 * ON_PI * 7.0 / (double)pole_boundary.size();
	const double polar = 0.9 + 0.12 * std::cos(3.0 * phase) +
	    (point == 8 ? 0.08 : 0.0);
	VSET(pole_boundary[point].point, std::sin(polar) * std::cos(phase),
	    std::cos(polar), std::sin(polar) * std::sin(phase));
    }
    const ON_3dVector pole_axis(0.0, 1.0, 0.0);
    repair_boundary_patch pole_patch;
    const bool pole_reconstructed = repair_spherical_cap_mesh(
	cap_brep->m_F[0], cap_sphere, pole_boundary, 4096, 0.2, 0.0, 0,
	pole_patch, &pole_axis);
    valid = pole_reconstructed && pole_patch.faces.size() / 3 ==
	pole_boundary.size() && pole_patch.direct_surface_deviations.size() ==
	pole_patch.faces.size() / 3;
    for (double deviation : pole_patch.direct_surface_deviations)
	valid = valid && std::isfinite(deviation) && deviation <= 0.2;
    if (!valid)
	return 7;

    std::vector<repair_fast_trim_sample> axial_boundary(32);
    for (size_t point = 0; point < axial_boundary.size(); ++point) {
	const double angle = 2.0 * ON_PI * (double)point /
	    (double)axial_boundary.size();
	VSET(axial_boundary[point].point, std::cos(angle),
	    std::sin(angle), 0.0);
    }
    const ON_3dVector boundary_pole_axis(1.0, 0.0, 0.0);
    repair_boundary_patch axial_patch;
    const bool axial_reconstructed = repair_spherical_cap_mesh(
	cap_brep->m_F[0], cap_sphere, axial_boundary, 4096, 0.3, 0.0, 0,
	axial_patch, &boundary_pole_axis);
    valid = axial_reconstructed && axial_patch.faces.size() / 3 ==
	axial_boundary.size() &&
	axial_patch.direct_surface_deviations.size() ==
	axial_patch.faces.size() / 3;
    for (double deviation : axial_patch.direct_surface_deviations)
	valid = valid && std::isfinite(deviation) && deviation <= 0.3;
    if (!valid)
	return 8;

    repair_boundary_patch hemisphere_patch;
    const bool hemisphere_reconstructed = repair_spherical_cap_mesh(
	cap_brep->m_F[0], cap_sphere, axial_boundary, 4096, 0.3, 0.0, 0,
	hemisphere_patch);
    valid = hemisphere_reconstructed &&
	hemisphere_patch.faces.size() / 3 == axial_boundary.size() &&
	hemisphere_patch.direct_surface_deviations.size() ==
	hemisphere_patch.faces.size() / 3;
    for (double deviation : hemisphere_patch.direct_surface_deviations)
	valid = valid && std::isfinite(deviation) && deviation <= 0.3;
    if (!valid)
	return 8;

    repair_boundary_patch full_sphere_patch;
    const bool full_sphere_reconstructed = repair_full_spherical_mesh(
	cap_brep->m_F[0], cap_sphere, pole_axis, 4096, 0.01, 0.0, 0,
	full_sphere_patch);
    const int full_sphere_points =
	(int)(full_sphere_patch.vertices.size() / 3);
    const int full_sphere_faces = (int)(full_sphere_patch.faces.size() / 3);
    valid = full_sphere_reconstructed && full_sphere_points > 0 &&
	full_sphere_faces > 0 &&
	full_sphere_patch.direct_surface_deviations.size() ==
	(size_t)full_sphere_faces &&
	!bg_trimesh_solid2(full_sphere_points, full_sphere_faces,
	    full_sphere_patch.vertices.data(),
	    full_sphere_patch.faces.data(), NULL);
    for (double deviation : full_sphere_patch.direct_surface_deviations)
	valid = valid && std::isfinite(deviation) && deviation <= 0.01;
    if (!valid)
	return 9;

    ON_Surface *closed_sphere_surface =
	cap_brep->m_S[0]->DuplicateSurface();
    std::unique_ptr<ON_Brep> closed_sphere_brep(new ON_Brep);
    if (!closed_sphere_surface || !closed_sphere_brep)
	return 10;
    ON_BrepFace &closed_sphere_face = closed_sphere_brep->NewFace(
	closed_sphere_brep->AddSurface(closed_sphere_surface));
    ON_BrepLoop &closed_sphere_loop = closed_sphere_brep->NewLoop(
	ON_BrepLoop::outer, closed_sphere_face);
    int sphere_open_direction = -1;
    for (int singular_side = 0; singular_side < 4; ++singular_side) {
	if (closed_sphere_surface->IsSingular(singular_side)) {
	    sphere_open_direction = (singular_side == 0 || singular_side == 2) ?
		1 : 0;
	    break;
	}
    }
    if (sphere_open_direction < 0)
	return 10;
    const int sphere_angular_direction = 1 - sphere_open_direction;
    const ON_Interval sphere_open_domain =
	closed_sphere_surface->Domain(sphere_open_direction);
    const double sphere_seam = closed_sphere_surface->Domain(
	sphere_angular_direction).Mid();
    ON_2dPoint low_uv(0.0, 0.0);
    ON_2dPoint high_uv(0.0, 0.0);
    low_uv[sphere_angular_direction] = sphere_seam;
    high_uv[sphere_angular_direction] = sphere_seam;
    low_uv[sphere_open_direction] = sphere_open_domain.Min();
    high_uv[sphere_open_direction] = sphere_open_domain.Max();
    ON_BrepVertex &low_vertex = closed_sphere_brep->NewVertex(
	closed_sphere_surface->PointAt(low_uv.x, low_uv.y), 1.0e-8);
    ON_BrepVertex &high_vertex = closed_sphere_brep->NewVertex(
	closed_sphere_surface->PointAt(high_uv.x, high_uv.y), 1.0e-8);
    ON_Curve *sphere_seam_curve = closed_sphere_surface->IsoCurve(
	sphere_open_direction, sphere_seam);
    if (!sphere_seam_curve)
	return 10;
    if (sphere_seam_curve->PointAtStart().DistanceTo(low_vertex.Point()) >
	    sphere_seam_curve->PointAtEnd().DistanceTo(low_vertex.Point()))
	sphere_seam_curve->Reverse();
    ON_BrepEdge &sphere_seam_edge = closed_sphere_brep->NewEdge(low_vertex,
	high_vertex, closed_sphere_brep->AddEdgeCurve(sphere_seam_curve));
    sphere_seam_edge.m_tolerance = 1.0e-8;
    const auto add_sphere_seam = [&](bool reversed) {
	const ON_2dPoint start = reversed ? high_uv : low_uv;
	const ON_2dPoint end = reversed ? low_uv : high_uv;
	ON_LineCurve *trim_curve = new ON_LineCurve(start, end);
	trim_curve->SetDomain(0.0, 1.0);
	ON_BrepTrim &trim = closed_sphere_brep->NewTrim(sphere_seam_edge,
	    reversed, closed_sphere_loop,
	    closed_sphere_brep->AddTrimCurve(trim_curve));
	trim.m_type = ON_BrepTrim::seam;
	trim.m_iso = closed_sphere_surface->IsIsoparametric(*trim_curve);
	trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-8;
    };
    add_sphere_seam(false);
    add_sphere_seam(true);
    state = ON_Brep_CDT_Create(closed_sphere_brep.get(),
	"closed sphere repair contract");
    if (state)
	state->brep = new ON_Brep(*closed_sphere_brep);
    repair_boundary_patch closed_sphere_patch;
    const bool closed_sphere_reconstructed = state && state->brep &&
	repair_closed_spherical_surface(state, 0, 4096, 0.01, 0,
	    closed_sphere_patch);
    const int closed_sphere_points =
	(int)(closed_sphere_patch.vertices.size() / 3);
    const int closed_sphere_faces =
	(int)(closed_sphere_patch.faces.size() / 3);
    valid = closed_sphere_reconstructed &&
	closed_sphere_patch.constrained_edges == 1 &&
	closed_sphere_patch.constrained_samples == 2 &&
	closed_sphere_points > 0 && closed_sphere_faces > 0 &&
	!bg_trimesh_solid2(closed_sphere_points, closed_sphere_faces,
	    closed_sphere_patch.vertices.data(),
	    closed_sphere_patch.faces.data(), NULL);
    ON_Brep_CDT_Destroy(state);
    if (!valid)
	return 10;

    ON_Circle major(ON_xy_plane, 9.0);
    ON_Torus torus(major, 2.5);
    ON_Circle minor = torus.MinorCircleRadians(0.0);
    const auto test_torus_strip = [&](ON_RevSurface *source_surface) {
	std::unique_ptr<ON_Brep> strip_brep(ON_BrepRevSurface(source_surface,
	    false, false, NULL));
	if (!strip_brep) {
	    delete source_surface;
	    return false;
	}
	struct ON_Brep_CDT_State *strip_state = ON_Brep_CDT_Create(
	    strip_brep.get(), "explicit torus seam repair contract");
	if (strip_state)
	    strip_state->brep = new ON_Brep(*strip_brep);
	if (!strip_state || !strip_state->brep ||
		strip_state->brep->m_F.Count() != 1) {
	    ON_Brep_CDT_Destroy(strip_state);
	    return false;
	}
	const ON_BrepFace &strip_face = strip_state->brep->m_F[0];
	const ON_BrepLoop *strip_loop = strip_face.OuterLoop();
	repair_fast_constraint_store strip_constraints;
	strip_constraints.face_index = 0;
	for (int trim_index = 0; strip_loop &&
		trim_index < strip_loop->TrimCount(); ++trim_index) {
	    const ON_BrepTrim *trim = strip_loop->Trim(trim_index);
	    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	    if (!trim || !edge || !edge->IsClosed() ||
		    trim->m_type == ON_BrepTrim::seam)
		continue;
	    std::vector<repair_fast_trim_sample> &samples =
		strip_constraints.trims[trim->m_trim_index];
	    const ON_Interval domain = trim->Domain();
	    for (int sample_index = 0; sample_index <= 32; ++sample_index) {
		repair_fast_trim_sample sample;
		sample.parameter = domain.ParameterAt(
		    (double)sample_index / 32.0);
		const ON_2dPoint uv = trim->PointAt(sample.parameter);
		const ON_3dPoint point = strip_face.SurfaceOf()->PointAt(
		    uv.x, uv.y);
		V2SET(sample.uv, uv.x, uv.y);
		VSET(sample.point, point.x, point.y, point.z);
		if (sample_index == 32)
		    VMOVE(sample.point, samples.front().point);
		samples.push_back(sample);
	    }
	    strip_constraints.constrained_edges++;
	    strip_constraints.constrained_samples += samples.size();
	}
	repair_boundary_patch strip_patch;
	const bool strip_reconstructed = repair_constrained_periodic_strip(
	    strip_state, 0, strip_constraints, 8192, 0.06, 0,
	    strip_patch);
	bool strip_valid = strip_reconstructed &&
	    strip_patch.analytic_torus_strip &&
	    strip_patch.constrained_edges == 2 &&
	    strip_patch.constrained_samples == 66 &&
	    strip_patch.direct_surface_deviations.size() ==
	    strip_patch.faces.size() / 3;
	for (double deviation : strip_patch.direct_surface_deviations)
	    strip_valid = strip_valid && std::isfinite(deviation) &&
		deviation <= 0.06;
	ON_Brep_CDT_Destroy(strip_state);
	return strip_valid;
    };
    ON_RevSurface *minor_circle_strip = new ON_RevSurface();
    minor_circle_strip->m_angle.Set(0.0, 0.5 * ON_PI);
    minor_circle_strip->m_t = minor_circle_strip->m_angle;
    minor_circle_strip->m_curve = new ON_ArcCurve(minor);
    minor_circle_strip->m_axis.from = torus.plane.origin;
    minor_circle_strip->m_axis.to = torus.plane.origin + torus.plane.zaxis;
    minor_circle_strip->m_bTransposed = false;
    if (!test_torus_strip(minor_circle_strip))
	return 11;

    ON_RevSurface *major_circle_strip = new ON_RevSurface();
    major_circle_strip->m_angle.Set(0.0, 2.0 * ON_PI);
    major_circle_strip->m_t = major_circle_strip->m_angle;
    major_circle_strip->m_curve = new ON_ArcCurve(ON_Arc(minor,
	ON_Interval(-0.4, 0.7)));
    major_circle_strip->m_axis.from = torus.plane.origin;
    major_circle_strip->m_axis.to = torus.plane.origin + torus.plane.zaxis;
    major_circle_strip->m_bTransposed = false;
    if (!test_torus_strip(major_circle_strip))
	return 12;

    ON_RevSurface *revolution = new ON_RevSurface();
    revolution->m_angle.Set(0.0, 2.0 * ON_PI);
    revolution->m_t = revolution->m_angle;
    revolution->m_curve = new ON_ArcCurve(minor);
    revolution->m_axis.from = torus.plane.origin;
    revolution->m_axis.to = torus.plane.origin + torus.plane.zaxis;
    revolution->m_bTransposed = false;
    std::unique_ptr<ON_Brep> torus_brep(ON_BrepRevSurface(revolution,
	false, false, NULL));
    if (!torus_brep) {
	delete revolution;
	return 13;
    }
    state = ON_Brep_CDT_Create(torus_brep.get(),
	"doubly periodic repair contract");
    if (state)
	state->brep = new ON_Brep(*torus_brep);
    if (!state || !state->brep || state->brep->m_F.Count() != 1) {
	ON_Brep_CDT_Destroy(state);
	return 14;
    }
    repair_boundary_patch grid;
    const bool grid_reconstructed = repair_closed_periodic_surface(state, 0,
	65536, 0.1, 0, grid);
    const int grid_points = (int)(grid.vertices.size() / 3);
    const int grid_faces = (int)(grid.faces.size() / 3);
    valid = grid_reconstructed && grid_points >= 64 && grid_faces > 0 &&
	grid.direct_surface_deviations.size() == (size_t)grid_faces &&
	!bg_trimesh_solid2(grid_points, grid_faces, grid.vertices.data(),
	    grid.faces.data(), NULL);
    for (double deviation : grid.direct_surface_deviations)
	valid = valid && std::isfinite(deviation) && deviation <= 0.1;
    ON_Brep_CDT_Destroy(state);
	return valid ? 0 : 15;
}

static void
repair_fast_face_output(int face_index, size_t first_face,
	size_t face_count, size_t first_point, size_t point_count, void *data)
{
    std::vector<repair_fast_face_range> *ranges =
	(std::vector<repair_fast_face_range> *)data;
    if (!ranges || face_index < 0 || (size_t)face_index >= ranges->size())
	return;
    repair_fast_face_range &range = (*ranges)[(size_t)face_index];
    range.first_face = first_face;
    range.face_count = face_count;
    range.first_point = first_point;
    range.point_count = point_count;
    range.present = face_count > 0 && point_count > 0;
}

static int
repair_component_root(std::vector<int> &parents, int face)
{
    int root = face;
    while (parents[(size_t)root] != root)
	root = parents[(size_t)root];
    while (parents[(size_t)face] != face) {
	const int next = parents[(size_t)face];
	parents[(size_t)face] = root;
	face = next;
    }
    return root;
}

static std::vector<int>
repair_brep_face_components(const ON_Brep *brep)
{
    const int face_count = brep ? brep->m_F.Count() : 0;
    std::vector<int> parents((size_t)std::max(0, face_count));
    std::iota(parents.begin(), parents.end(), 0);
    if (!brep)
	return parents;

    std::unordered_map<int, int> edge_owner;
    for (int trim_index = 0; trim_index < brep->m_T.Count(); ++trim_index) {
	const ON_BrepTrim &trim = brep->m_T[trim_index];
	const ON_BrepFace *face = trim.Face();
	if (trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count() || !face ||
		face->m_face_index < 0 || face->m_face_index >= face_count)
	    continue;
	const int face_index = face->m_face_index;
	const auto inserted = edge_owner.emplace(trim.m_ei, face_index);
	if (inserted.second)
	    continue;
	const int first_root = repair_component_root(parents,
	    inserted.first->second);
	const int second_root = repair_component_root(parents, face_index);
	if (first_root != second_root)
	    parents[(size_t)second_root] = first_root;
    }
    for (int face = 0; face < face_count; ++face)
	parents[(size_t)face] = repair_component_root(parents, face);
    return parents;
}

static bool
repair_brep_components_are_closed(const ON_Brep *brep,
	const std::vector<int> &components)
{
    if (!brep || components.size() != (size_t)brep->m_F.Count())
	return false;
    std::vector<int> edge_uses((size_t)brep->m_E.Count(), 0);
    std::vector<int> edge_component((size_t)brep->m_E.Count(), -1);
    for (int trim_index = 0; trim_index < brep->m_T.Count(); ++trim_index) {
	const ON_BrepTrim &trim = brep->m_T[trim_index];
	if (trim.m_type == ON_BrepTrim::singular)
	    continue;
	const ON_BrepFace *face = trim.Face();
	if (trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count() || !face ||
		face->m_face_index < 0 ||
		(size_t)face->m_face_index >= components.size())
	    return false;
	const int component = components[(size_t)face->m_face_index];
	if (edge_component[(size_t)trim.m_ei] >= 0 &&
		edge_component[(size_t)trim.m_ei] != component)
	    return false;
	edge_component[(size_t)trim.m_ei] = component;
	edge_uses[(size_t)trim.m_ei]++;
    }
    for (int uses : edge_uses) {
	if (uses && uses != 2)
	    return false;
    }
    return true;
}

struct repair_poisson_sample {
    fastf_t point[3];
    fastf_t normal[3];
};

struct repair_poisson_triangle {
    size_t face;
    int component;
    double area;
    fastf_t normal[3];
    fastf_t corner_normals[3][3];
};

static double
repair_radical_inverse(size_t index)
{
    double inverse = 0.0;
    double fraction = 0.5;
    while (index) {
	if (index & 1)
	    inverse += fraction;
	index >>= 1;
	fraction *= 0.5;
    }
    return inverse;
}

static double
repair_mesh_area(const fastf_t *vertices, const int *faces, int face_count)
{
    double area = 0.0;
    for (int face = 0; face < face_count; ++face) {
	const int *triangle = &faces[(size_t)face * 3];
	const ON_3dPoint a(&vertices[(size_t)triangle[0] * 3]);
	const ON_3dPoint b(&vertices[(size_t)triangle[1] * 3]);
	const ON_3dPoint c(&vertices[(size_t)triangle[2] * 3]);
	area += 0.5 * ON_CrossProduct(b - a, c - a).Length();
    }
    return area;
}

/* Raising the normal hole-size limit before the mixed mesh is assembled also
 * expands the much more expensive rigorous-boundary search.  Retry only the
 * final mesh repair, and only when its remaining defects prove that the
 * requested ceiling can cover all open edges.  Excess or misoriented edges
 * describe non-hole topology and are deliberately ineligible. */
static size_t
repair_adaptive_hole_edge_budget(
	const struct brep_cdt_repair_settings *settings,
	const struct bg_trimesh_repair_report *mesh_report)
{
    if (!settings || !mesh_report || !settings->mesh.fill_holes ||
	    settings->max_adaptive_hole_edges <=
	    settings->mesh.max_hole_edges || mesh_report->unmatched_edges <= 0 ||
	    (size_t)mesh_report->unmatched_edges <=
	    settings->mesh.max_hole_edges ||
	    (size_t)mesh_report->unmatched_edges >
	    settings->max_adaptive_hole_edges || mesh_report->excess_edges ||
	    mesh_report->misoriented_edges)
	return 0;
    return (size_t)mesh_report->unmatched_edges;
}

static int
repair_adaptive_hole_retry_contract(void)
{
    struct brep_cdt_repair_settings settings =
	BREP_CDT_REPAIR_SETTINGS_INIT;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    settings.mesh.fill_holes = 1;
    settings.mesh.max_hole_edges = 256;
    settings.max_adaptive_hole_edges = 4096;
    report.unmatched_edges = 1974;
    if (repair_adaptive_hole_edge_budget(&settings, &report) != 1974)
	return 1;
    report.excess_edges = 1;
    if (repair_adaptive_hole_edge_budget(&settings, &report))
	return 2;
    report.excess_edges = 0;
    report.misoriented_edges = 1;
    if (repair_adaptive_hole_edge_budget(&settings, &report))
	return 3;
    report.misoriented_edges = 0;
    report.unmatched_edges = 4097;
    if (repair_adaptive_hole_edge_budget(&settings, &report))
	return 4;
    settings.max_adaptive_hole_edges = 0;
    report.unmatched_edges = 1974;
    return repair_adaptive_hole_edge_budget(&settings, &report) ? 5 : 0;
}

static void
repair_added_patch_stats(struct brep_cdt_repair_report *report,
	const fastf_t *input_vertices, const int *input_faces,
	int input_face_count, const fastf_t *output_vertices,
	const int *output_faces, int output_face_count)
{
    if (!report || !input_vertices || !input_faces || input_face_count <= 0 ||
	    !output_vertices || !output_faces || output_face_count <= 0)
	return;
    std::set<repair_triangle_key> input_triangles;
    for (int face = 0; face < input_face_count; ++face)
	input_triangles.insert(repair_triangle_coordinates(input_vertices,
	    &input_faces[(size_t)face * 3]));
    std::vector<int> added_faces;
    for (int face = 0; face < output_face_count; ++face) {
	if (input_triangles.find(repair_triangle_coordinates(output_vertices,
		&output_faces[(size_t)face * 3])) == input_triangles.end())
	    added_faces.push_back(face);
    }
    typedef std::pair<int, int> repair_edge_key;
    std::map<repair_edge_key, std::vector<int>> edge_faces;
    for (size_t local_face = 0; local_face < added_faces.size();
	    ++local_face) {
	const int *triangle = &output_faces[
	    (size_t)added_faces[local_face] * 3];
	for (int corner = 0; corner < 3; ++corner) {
	    int first = triangle[corner];
	    int second = triangle[(corner + 1) % 3];
	    if (second < first)
		std::swap(first, second);
	    edge_faces[{first, second}].push_back((int)local_face);
	}
    }
    std::vector<std::vector<int>> neighbors(added_faces.size());
    for (const auto &edge : edge_faces) {
	for (size_t first = 0; first < edge.second.size(); ++first) {
	    for (size_t second = first + 1; second < edge.second.size();
		    ++second) {
		neighbors[(size_t)edge.second[first]].push_back(
		    edge.second[second]);
		neighbors[(size_t)edge.second[second]].push_back(
		    edge.second[first]);
	    }
	}
    }
    std::vector<bool> visited(added_faces.size(), false);
    for (size_t seed = 0; seed < added_faces.size(); ++seed) {
	if (visited[seed])
	    continue;
	report->added_patch_components++;
	std::queue<int> pending;
	pending.push((int)seed);
	visited[seed] = true;
	int patch_faces = 0;
	double patch_area = 0.0;
	while (!pending.empty()) {
	    const int local_face = pending.front();
	    pending.pop();
	    const int *triangle = &output_faces[
		(size_t)added_faces[(size_t)local_face] * 3];
	    const ON_3dPoint a(&output_vertices[(size_t)triangle[0] * 3]);
	    const ON_3dPoint b(&output_vertices[(size_t)triangle[1] * 3]);
	    const ON_3dPoint c(&output_vertices[(size_t)triangle[2] * 3]);
	    patch_area += 0.5 * ON_CrossProduct(b - a, c - a).Length();
	    patch_faces++;
	    for (int neighbor : neighbors[(size_t)local_face]) {
		if (visited[(size_t)neighbor])
		    continue;
		visited[(size_t)neighbor] = true;
		pending.push(neighbor);
	    }
	}
	if (patch_area > report->largest_added_patch_area) {
	    report->largest_added_patch_area = patch_area;
	    report->largest_added_patch_faces = patch_faces;
	}
    }
}

struct repair_missing_patch_report {
    int components = 0;
    int largest_faces = 0;
    size_t largest_boundary_edges = 0;
    double total_area = 0.0;
    double largest_area = 0.0;
};

/* A local repair may replace a certified triangle neighborhood only under the
 * same explicit geometric limits used to fill a hole.  Coordinate edges are
 * intentional here: the repair transaction colocates equivalent boundary
 * samples before operating, so index identity is no longer authoritative. */
static bool
repair_missing_patch_bounded(
	const std::vector<repair_triangle_key> &triangles,
	double input_area, const struct bg_trimesh_repair_settings &settings,
	repair_missing_patch_report *report)
{
    if (report)
	*report = repair_missing_patch_report();
    if (triangles.empty())
	return true;
    if (!settings.fill_holes || settings.max_hole_edges < 3 ||
	    !(input_area > 0.0) || !std::isfinite(input_area))
	return false;
    double area_limit = 0.0;
    if (settings.max_hole_area > 0.0)
	area_limit = settings.max_hole_area;
    else if (settings.max_hole_area_percent > 0.0)
	area_limit = input_area * settings.max_hole_area_percent / 100.0;
    if (!(area_limit > 0.0) || !std::isfinite(area_limit))
	return false;

    std::vector<int> parents(triangles.size(), -1);
    const auto root = [&](int item) {
	int result = item;
	while (parents[(size_t)result] >= 0)
	    result = parents[(size_t)result];
	return result;
    };
    const auto unite = [&](int first, int second) {
	int first_root = root(first);
	int second_root = root(second);
	if (first_root == second_root)
	    return;
	if (parents[(size_t)first_root] > parents[(size_t)second_root])
	    std::swap(first_root, second_root);
	parents[(size_t)first_root] += parents[(size_t)second_root];
	parents[(size_t)second_root] = first_root;
    };
    std::map<repair_triangle_edge_key, std::vector<int>> edge_faces;
    for (size_t face = 0; face < triangles.size(); ++face) {
	for (int corner = 0; corner < 3; ++corner)
	    edge_faces[repair_triangle_edge(triangles[face][(size_t)corner],
		triangles[face][(size_t)((corner + 1) % 3)])].push_back(
		(int)face);
    }
    for (const auto &edge : edge_faces) {
	/* The neighborhood may be the very non-manifold source topology that
	 * repair must replace.  Keep the accepted ambiguity local and bounded;
	 * the independently validated output must still have exactly two uses. */
	if (edge.second.size() > 4)
	    return false;
	for (size_t face = 1; face < edge.second.size(); ++face)
	    unite(edge.second[0], edge.second[face]);
    }
    std::map<int, int> component_faces;
    std::map<int, size_t> component_boundary_edges;
    std::map<int, double> component_area;
    for (size_t face = 0; face < triangles.size(); ++face) {
	const int component = root((int)face);
	const double area = repair_triangle_key_area(triangles[face]);
	if (!(area > 0.0) || !std::isfinite(area))
	    return false;
	component_faces[component]++;
	component_area[component] += area;
    }
    for (const auto &edge : edge_faces) {
	if (edge.second.size() == 1)
	    component_boundary_edges[root(edge.second[0])]++;
    }
    const size_t face_limit = settings.max_hole_edges > SIZE_MAX / 4 ?
	SIZE_MAX : 4 * settings.max_hole_edges;
    repair_missing_patch_report result;
    result.components = (int)component_faces.size();
    for (const auto &component : component_faces) {
	const size_t boundary = component_boundary_edges[component.first];
	const double area = component_area[component.first];
	if ((size_t)component.second > face_limit || area > area_limit ||
		(boundary > 0 &&
		(boundary < 3 || boundary > settings.max_hole_edges)))
	    return false;
	result.total_area += area;
	if (component.second > result.largest_faces)
	    result.largest_faces = component.second;
	result.largest_boundary_edges = std::max(
	    result.largest_boundary_edges, boundary);
	result.largest_area = std::max(result.largest_area, area);
    }
    if (result.total_area > area_limit)
	return false;
    if (report)
	*report = result;
    return true;
}

static size_t
repair_quarantine_duplicate_source_faces(int *faces, int face_count,
	const fastf_t *vertices, int *rigorous_face_count,
	std::vector<int> &sources, std::set<int> &quarantined_sources,
	size_t *exact_duplicate_count = NULL,
	std::map<int, std::pair<size_t, size_t>> *source_counts = NULL)
{
    quarantined_sources.clear();
    if (exact_duplicate_count)
	*exact_duplicate_count = 0;
    if (source_counts)
	source_counts->clear();
    if (!faces || face_count <= 0 || !vertices || !rigorous_face_count ||
	    *rigorous_face_count <= 0 || *rigorous_face_count > face_count ||
	    sources.size() != (size_t)face_count)
	return 0;

    std::map<repair_triangle_key, std::vector<int>> retained_by_triangle;
    std::map<int, size_t> source_triangle_counts;
    std::map<int, size_t> source_duplicate_counts;
    size_t duplicate_count = 0;
    for (int face = 0; face < *rigorous_face_count; ++face) {
	const repair_triangle_key key = repair_triangle_coordinates(vertices,
	    &faces[(size_t)face * 3]);
	const int source = sources[(size_t)face];
	source_triangle_counts[source]++;
	const ON_3dPoint a(&vertices[(size_t)faces[(size_t)face * 3] * 3]);
	const ON_3dPoint b(&vertices[(size_t)faces[(size_t)face * 3 + 1] * 3]);
	const ON_3dPoint c(&vertices[(size_t)faces[(size_t)face * 3 + 2] * 3]);
	ON_3dVector normal = ON_CrossProduct(b - a, c - a);
	if (!normal.Unitize()) {
	    retained_by_triangle[key].push_back(face);
	    continue;
	}
	bool duplicate = false;
	for (int prior : retained_by_triangle[key]) {
	    if (sources[(size_t)prior] == source)
		continue;
	    const ON_3dPoint pa(&vertices[(size_t)
		faces[(size_t)prior * 3] * 3]);
	    const ON_3dPoint pb(&vertices[(size_t)
		faces[(size_t)prior * 3 + 1] * 3]);
	    const ON_3dPoint pc(&vertices[(size_t)
		faces[(size_t)prior * 3 + 2] * 3]);
	    ON_3dVector prior_normal = ON_CrossProduct(pb - pa, pc - pa);
	    if (prior_normal.Unitize() && prior_normal * normal >
		    1.0 - 64.0 * std::numeric_limits<double>::epsilon()) {
		duplicate = true;
		break;
	    }
	}
	if (duplicate) {
	    quarantined_sources.insert(source);
	    source_duplicate_counts[source]++;
	    duplicate_count++;
	} else {
	    retained_by_triangle[key].push_back(face);
	}
    }
    if (!duplicate_count)
	return 0;

    std::vector<int> reordered_faces;
    std::vector<int> reordered_sources;
    reordered_faces.reserve((size_t)face_count * 3);
    reordered_sources.reserve((size_t)face_count);
    const auto append_face = [&](int face) {
	reordered_faces.insert(reordered_faces.end(),
	    &faces[(size_t)face * 3], &faces[(size_t)face * 3] + 3);
	reordered_sources.push_back(sources[(size_t)face]);
    };
    for (int face = 0; face < *rigorous_face_count; ++face) {
	if (quarantined_sources.find(sources[(size_t)face]) ==
		quarantined_sources.end())
	    append_face(face);
    }
    const int retained_rigorous_count = (int)reordered_sources.size();
    for (int face = 0; face < *rigorous_face_count; ++face) {
	if (quarantined_sources.find(sources[(size_t)face]) !=
		quarantined_sources.end())
	    append_face(face);
    }
    for (int face = *rigorous_face_count; face < face_count; ++face)
	append_face(face);
    memcpy(faces, reordered_faces.data(),
	reordered_faces.size() * sizeof(int));
    sources.swap(reordered_sources);
    *rigorous_face_count = retained_rigorous_count;
    if (exact_duplicate_count)
	*exact_duplicate_count = duplicate_count;
    size_t quarantined_triangles = 0;
    for (int source : quarantined_sources) {
	quarantined_triangles += source_triangle_counts[source];
	if (source_counts)
	    (*source_counts)[source] = std::make_pair(
		source_duplicate_counts[source], source_triangle_counts[source]);
    }
    return quarantined_triangles;
}

int
cdt_test_repair_duplicate_quarantine(void)
{
    const fastf_t vertices[] = {
	0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
	1.0, 1.0, 0.0, 0.0, 1.0, 0.0,
	2.0, 0.0, 0.0, 2.0, 1.0, 0.0
    };
    int faces[] = {
	0, 1, 2, 0, 1, 2, 0, 2, 3, 1, 4, 5, 0, 3, 2
    };
    std::vector<int> sources = {0, 1, 1, 2, 9};
    int rigorous_faces = 4;
    std::set<int> quarantined;
    size_t duplicates = 0;
    std::map<int, std::pair<size_t, size_t>> counts;
    const size_t moved = repair_quarantine_duplicate_source_faces(faces, 5,
	vertices, &rigorous_faces, sources, quarantined, &duplicates,
	&counts);
    if (moved != 2 || duplicates != 1 || rigorous_faces != 2 ||
	    quarantined != std::set<int>({1}) || sources !=
	    std::vector<int>({0, 2, 1, 1, 9}) || counts[1].first != 1 ||
	    counts[1].second != 2)
	return 1;
    if (faces[0] != 0 || faces[1] != 1 || faces[2] != 2 ||
	    faces[3] != 1 || faces[4] != 4 || faces[5] != 5)
	return 2;

    int opposite[] = {0, 1, 2, 0, 2, 1};
    sources = {0, 1};
    rigorous_faces = 2;
    if (repair_quarantine_duplicate_source_faces(opposite, 2, vertices,
	    &rigorous_faces, sources, quarantined) != 0 ||
	    rigorous_faces != 2 || !quarantined.empty())
	return 3;
    return 0;
}

int
cdt_test_repair_patch_limits(void)
{
    repair_triangle_key first = {{{{0.0, 0.0, 0.0}},
	{{1.0, 0.0, 0.0}}, {{0.0, 1.0, 0.0}}}};
    repair_triangle_key second = {{{{1.0, 0.0, 0.0}},
	{{1.0, 1.0, 0.0}}, {{0.0, 1.0, 0.0}}}};
    std::sort(first.begin(), first.end());
    std::sort(second.begin(), second.end());
    const std::vector<repair_triangle_key> patch = {first, second};
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.fill_holes = 1;
    settings.max_hole_edges = 4;
    settings.max_hole_area_percent = 2.0;
    repair_missing_patch_report report;
    if (!repair_missing_patch_bounded(patch, 100.0, settings, &report) ||
	    report.components != 1 || report.largest_faces != 2 ||
	    report.largest_boundary_edges != 4 ||
	    std::fabs(report.total_area - 1.0) > 1e-12)
	return 1;
    settings.max_hole_area_percent = 0.5;
    if (repair_missing_patch_bounded(patch, 100.0, settings, NULL))
	return 2;
    settings.max_hole_area_percent = 2.0;
    settings.max_hole_edges = 3;
    if (repair_missing_patch_bounded(patch, 100.0, settings, NULL))
	return 3;
    settings.max_hole_edges = 16;
    settings.max_hole_area_percent = 100.0;
    std::vector<repair_triangle_key> excessive_valence;
    for (int triangle = 0; triangle < 5; ++triangle) {
	repair_triangle_key candidate = {{{{0.0, 0.0, 0.0}},
	    {{1.0, 0.0, 0.0}},
	    {{0.5, (double)triangle + 1.0, 0.0}}}};
	std::sort(candidate.begin(), candidate.end());
	excessive_valence.push_back(candidate);
    }
    return repair_missing_patch_bounded(excessive_valence, 100.0,
	settings, NULL) ? 4 : 0;
}

static int
brep_cdt_repair_attempt(struct ON_Brep_CDT_State *s_cdt,
	const struct brep_cdt_repair_settings *settings,
	struct brep_cdt_repair_report *report, bool area_weighted_samples,
	bool closure_biased_poisson, bool automatic_local_repair)
{
    struct brep_cdt_repair_report local_report =
	BREP_CDT_REPAIR_REPORT_INIT;
    if (!report)
	report = &local_report;
    *report = local_report;
    if (!s_cdt || !s_cdt->orig_brep)
	return -1;
    struct brep_cdt_repair_settings default_settings =
	BREP_CDT_REPAIR_SETTINGS_INIT;
    if (!settings)
	settings = &default_settings;

    if (!s_cdt->repair_source_valid) {
	s_cdt->repair_source_diagnostic = s_cdt->diagnostic;
	s_cdt->repair_source_valid = true;
    }
    report->source_diagnostic = s_cdt->repair_source_diagnostic;
    report->source_failed_faces = (int)s_cdt->failed_face_indices.size();
    report->bounded_edge_approximation_edges = (int)std::min(
	s_cdt->approximated_edges.size(), (size_t)INT_MAX);
    for (const auto &edge : s_cdt->approximated_edges)
	report->max_bounded_edge_deviation = std::max(
	    report->max_bounded_edge_deviation, edge.second);
    if (s_cdt->status == BREP_CDT_SOLID && s_cdt->certified_faces &&
	    s_cdt->certified_vertices &&
	    !settings->mesh.require_manifold &&
	    !settings->mesh.union_components &&
	    s_cdt->approximated_edges.empty()) {
	report->mesh.input_vertices = s_cdt->certified_vertex_count;
	report->mesh.input_faces = s_cdt->certified_face_count;
	report->mesh.output_vertices = s_cdt->certified_vertex_count;
	report->mesh.output_faces = s_cdt->certified_face_count;
	report->mesh.solid = 1;
	return 1;
    }
    if (!std::isfinite(settings->max_surface_deviation) ||
	    settings->max_surface_deviation < 0.0 ||
	    !std::isfinite(settings->max_area_change_percent) ||
	    settings->max_area_change_percent < 0.0 ||
	    !std::isfinite(settings->relaxed_fidelity_factor) ||
	    settings->relaxed_fidelity_factor < 0.0 ||
	    (settings->relaxed_fidelity_factor > 0.0 &&
	    (settings->relaxed_fidelity_factor < 1.0 ||
	    settings->relaxed_fidelity_factor > 4.0)) ||
	    (settings->max_adaptive_hole_edges > 0 &&
	    settings->max_adaptive_hole_edges <
	    settings->mesh.max_hole_edges) ||
	    (settings->use_poisson_reconstruction &&
	    (!settings->use_full_fast_fallback || settings->poisson_depth < 5 ||
	    settings->poisson_depth > 10 ||
	    !settings->max_poisson_components ||
	    !std::isfinite(settings->poisson_scale) ||
	    settings->poisson_scale < 1.0 ||
	    settings->poisson_scale > 2.0)) ||
	    ((settings->use_fast_face_fallback ||
	    settings->use_full_fast_fallback ||
	    settings->use_full_fast_fallback_if_needed) &&
	    (!settings->max_fast_points || !settings->max_fast_result_bytes ||
	    settings->max_fast_time_ms <= 0))) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, "invalid mesh repair limits");
	return -1;
    }
    if (settings->mesh.fill_holes &&
	    (settings->mesh.max_hole_edges < 3 ||
	    (!(settings->mesh.max_hole_area > 0.0) &&
	    !(settings->mesh.max_hole_area_percent > 0.0)))) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces,
	    "B-Rep hole repair requires explicit edge and area limits");
	return -1;
    }
    if (settings->mesh.remove_small_components &&
	    !(settings->mesh.max_component_area > 0.0) &&
	    !(settings->mesh.max_component_area_percent > 0.0)) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces,
	    "B-Rep component removal requires an explicit area limit");
	return -1;
    }

    std::set<int> failed_faces(s_cdt->failed_face_indices.begin(),
	s_cdt->failed_face_indices.end());
    std::set<int> approximation_faces(failed_faces.begin(),
	failed_faces.end());
    std::set<int> bounded_edge_faces;
    for (const auto &approximation : s_cdt->approximated_edges) {
	if (approximation.first < 0 ||
		approximation.first >= s_cdt->orig_brep->m_E.Count())
	    continue;
	ON_BrepEdge &edge =
	    s_cdt->orig_brep->m_E[approximation.first];
	for (int trim_index = 0; trim_index < edge.TrimCount(); ++trim_index) {
	    ON_BrepTrim *trim = edge.Trim(trim_index);
	    ON_BrepFace *face = trim ? trim->Face() : NULL;
	    if (face)
		bounded_edge_faces.insert(face->m_face_index);
	}
    }
    approximation_faces.insert(bounded_edge_faces.begin(),
	bounded_edge_faces.end());
    report->bounded_edge_approximation_faces = (int)std::min(
	bounded_edge_faces.size(), (size_t)INT_MAX);
    for (int face = 0; face < s_cdt->orig_brep->m_F.Count(); ++face) {
	if (s_cdt->fmeshes.find(face) == s_cdt->fmeshes.end())
	    approximation_faces.insert(face);
    }
    std::vector<int> usable_faces;
    for (const auto &face : s_cdt->fmeshes) {
	if (failed_faces.find(face.first) != failed_faces.end())
	    continue;
	if (face.second.tris_tree.Count())
	    usable_faces.push_back(face.first);
	else
	    approximation_faces.insert(face.first);
    }
    /* The complete export path performs transactional component filtering
     * and winding synchronization.  Passing an explicit list containing
     * every face looks equivalent, but intentionally selects the partial
     * export path and can reintroduce misoriented edges into repair input. */
    const bool complete_usable_mesh = usable_faces.size() ==
	(size_t)s_cdt->orig_brep->m_F.Count();
    const int usable_selection_count = complete_usable_mesh ? 0 :
	(int)usable_faces.size();
    int *usable_selection = complete_usable_mesh ? NULL :
	usable_faces.data();
    int *input_faces = NULL;
    int input_face_count = 0;
    fastf_t *input_vertices = NULL;
    int input_vertex_count = 0;
    std::vector<int> display_reference_faces;
    std::vector<fastf_t> display_reference_vertices;
    std::vector<int> display_reference_brep_faces;
    int display_reference_face_count = 0;
    std::map<repair_triangle_key, std::set<int>>
	local_chart_triangle_brep_faces;
    std::map<repair_triangle_key, double>
	direct_surface_triangle_deviations;
    std::map<repair_triangle_key,
	std::vector<std::pair<int, triangle_t>>>
	best_effort_surface_triangles;
    std::map<repair_triangle_key, std::set<int>> fast_triangle_brep_faces;
    /* Whole-B-Rep fallback replaces every rigorous face mesh.  Do not spend
     * another bounded refinement cycle assembling those faces after a stage
     * 11 failure only to discard the result below. */
    if (!settings->use_full_fast_fallback && !usable_faces.empty()) {
	if (ON_Brep_CDT_Mesh(&input_faces, &input_face_count, &input_vertices,
	    &input_vertex_count, NULL, NULL, NULL, NULL, s_cdt,
	    usable_selection_count, usable_selection) < 0 ||
	    input_face_count <= 0 || input_vertex_count <= 0) {
	    if (input_faces)
		bu_free(input_faces, "repair input faces");
	    if (input_vertices)
		bu_free(input_vertices, "repair input vertices");
	    input_faces = NULL;
	    input_vertices = NULL;
	    input_face_count = 0;
	    input_vertex_count = 0;
	    if (!settings->use_full_fast_fallback) {
		cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
		    BREP_CDT_STAGE_MESH_REPAIR, -1,
		    report->source_diagnostic.completed_faces,
		    report->source_failed_faces,
		    "failed to assemble the usable face meshes for repair");
		return -1;
	    }
	}

	/* A failed face previously prevented the final assembled-mesh pass from
	 * running at all, so repair could close the missing patch only to expose
	 * an approximation crossing between two otherwise successful faces.
	 * Refine those rigorous chords before introducing any repair geometry.
	 * Open-boundary link failures are expected here and ignored; only the
	 * independently detected nonadjacent triangle intersection drives this
	 * bounded retry. */
	size_t partial_refinement_points = 0;
	int partial_refinement_attempts = 0;
	while (input_face_count > 0 && input_vertex_count > 0 &&
	    partial_refinement_attempts <
	    MAX_ASSEMBLED_REFINEMENT_ATTEMPTS &&
	    partial_refinement_points < MAX_ASSEMBLED_REFINEMENT_POINTS) {
	    assembled_mesh_validation partial_validation;
	    assembled_mesh_validate(input_vertex_count, input_face_count,
		input_vertices, input_faces, &partial_validation,
		!settings->mesh.allow_self_intersections);
	    if (!partial_validation.intersecting_triangle_pairs)
		break;
	    const size_t inserted = refine_assembled_intersection(s_cdt,
		partial_validation, MAX_ASSEMBLED_REFINEMENT_POINTS -
		partial_refinement_points);
	    if (!inserted)
		break;
	    partial_refinement_points += inserted;
	    partial_refinement_attempts++;
	    bu_free(input_faces, "pre-repair intersecting faces");
	    bu_free(input_vertices, "pre-repair intersecting vertices");
	    input_faces = NULL;
	    input_vertices = NULL;
	    if (ON_Brep_CDT_Mesh(&input_faces, &input_face_count,
		&input_vertices, &input_vertex_count, NULL, NULL, NULL,
		NULL, s_cdt, usable_selection_count,
		usable_selection) < 0 ||
		input_face_count <= 0 || input_vertex_count <= 0) {
		if (input_faces)
		    bu_free(input_faces, "failed pre-repair faces");
		if (input_vertices)
		    bu_free(input_vertices, "failed pre-repair vertices");
		input_faces = NULL;
		input_vertices = NULL;
		input_face_count = 0;
		input_vertex_count = 0;
		if (!settings->use_full_fast_fallback) {
		    cdt_diagnostic_set(s_cdt,
			BREP_CDT_RESULT_REPAIR_FAILED,
			BREP_CDT_STAGE_MESH_REPAIR, -1,
			report->source_diagnostic.completed_faces,
			report->source_failed_faces,
			"failed to reassemble cross-face refined repair input");
		    return -1;
		}
		break;
	    }
	}
    }
    int rigorous_input_face_count = input_face_count;
    std::vector<int> rigorous_input_brep_faces =
	s_cdt->bot_face_to_brep_face;
    std::vector<int> input_face_brep_sources = rigorous_input_brep_faces;
    if (input_face_brep_sources.size() != (size_t)input_face_count)
	input_face_brep_sources.assign((size_t)input_face_count, -1);
    std::set<int> locally_reconstructed_faces;

    std::unordered_map<ON_3dPoint *, int> approximate_source_points;
    if (s_cdt->bot_pnt_to_on_pnt &&
	    s_cdt->bot_pnt_to_on_pnt->size() ==
	    (size_t)input_vertex_count) {
	for (int point = 0; point < input_vertex_count; ++point) {
	    ON_3dPoint *source = (*s_cdt->bot_pnt_to_on_pnt)[
		(size_t)point];
	    if (source)
		approximate_source_points[source] = point;
	}
    }
    const auto append_approximate_mesh = [&](int face_index,
	    const int *local_faces, int local_face_count,
	    const fastf_t *local_vertices, int local_vertex_count,
	    const std::vector<ON_3dPoint *> *local_source_points = NULL) {
	if (!local_faces || local_face_count <= 0 || !local_vertices ||
		local_vertex_count <= 0 || local_vertex_count >
		INT_MAX - input_vertex_count || local_face_count >
		INT_MAX - input_face_count || (local_source_points &&
		local_source_points->size() != (size_t)local_vertex_count))
	    return 0;
	for (int corner = 0; corner < local_face_count * 3; ++corner) {
	    if (local_faces[corner] < 0 ||
		    local_faces[corner] >= local_vertex_count)
		return 0;
	}
	std::vector<int> local_to_input((size_t)local_vertex_count, -1);
	std::vector<fastf_t> novel_vertices;
	std::map<repair_point_key, int> local_generated_points;
	novel_vertices.reserve((size_t)local_vertex_count * 3);
	size_t source_vertices = 0;
	size_t matched_source_vertices = 0;
	for (int point = 0; point < local_vertex_count; ++point) {
	    ON_3dPoint *source = local_source_points ?
		(*local_source_points)[(size_t)point] : NULL;
	    source_vertices += source ? 1 : 0;
	    const repair_point_key key = {
		local_vertices[(size_t)point * 3],
		local_vertices[(size_t)point * 3 + 1],
		local_vertices[(size_t)point * 3 + 2]};
	    if (source) {
		const auto existing = approximate_source_points.find(source);
		if (existing != approximate_source_points.end()) {
		    local_to_input[(size_t)point] = existing->second;
		    matched_source_vertices++;
		    continue;
		}
	    } else {
		const auto existing = local_generated_points.find(key);
		if (existing != local_generated_points.end()) {
		    local_to_input[(size_t)point] = existing->second;
		    continue;
		}
	    }
	    const int new_index = input_vertex_count +
		(int)(novel_vertices.size() / 3);
	    if (source)
		approximate_source_points[source] = new_index;
	    else
		local_generated_points[key] = new_index;
	    local_to_input[(size_t)point] = new_index;
	    novel_vertices.insert(novel_vertices.end(),
		&local_vertices[(size_t)point * 3],
		&local_vertices[(size_t)point * 3] + 3);
	}
	if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY"))
	    bu_log("Face %d: matched %zu/%zu authoritative repair vertices "
		"by CDT point identity\n", face_index, matched_source_vertices,
		source_vertices);
	const size_t new_vertex_count = (size_t)input_vertex_count +
	    novel_vertices.size() / 3;
	const size_t new_face_count = (size_t)input_face_count +
	    (size_t)local_face_count;
	input_vertices = (fastf_t *)bu_realloc(input_vertices,
	    new_vertex_count * 3 * sizeof(fastf_t),
	    "repair input vertices with approximate face");
	input_faces = (int *)bu_realloc(input_faces,
	    new_face_count * 3 * sizeof(int),
	    "repair input faces with approximate face");
	if (!novel_vertices.empty())
	    memcpy(&input_vertices[(size_t)input_vertex_count * 3],
		novel_vertices.data(), novel_vertices.size() * sizeof(fastf_t));
	for (int corner = 0; corner < local_face_count * 3; ++corner)
	    input_faces[(size_t)input_face_count * 3 + corner] =
		local_to_input[(size_t)local_faces[corner]];
	for (int face = 0; face < local_face_count; ++face) {
	    const repair_triangle_key key = repair_triangle_coordinates(
		input_vertices,
		&input_faces[((size_t)input_face_count + face) * 3]);
	    local_chart_triangle_brep_faces[key].insert(face_index);
	}
	input_vertex_count = (int)new_vertex_count;
	input_face_count = (int)new_face_count;
	input_face_brep_sources.insert(input_face_brep_sources.end(),
	    (size_t)local_face_count, face_index);
	return local_face_count;
    };
    const auto approximate_mesh_orientable = [&](const int *local_faces,
	    int local_face_count, const fastf_t *local_vertices,
	    int local_vertex_count,
	    const std::vector<ON_3dPoint *> *local_source_points) {
	if (!local_faces || local_face_count <= 0 || !local_vertices ||
		local_vertex_count <= 0 || (local_source_points &&
		local_source_points->size() != (size_t)local_vertex_count))
	    return false;
	std::unordered_map<ON_3dPoint *, int> source_points =
	    approximate_source_points;
	std::map<repair_point_key, int> generated_points;
	std::vector<int> local_to_input((size_t)local_vertex_count, -1);
	int next_vertex = input_vertex_count;
	for (int point = 0; point < local_vertex_count; ++point) {
	    ON_3dPoint *source = local_source_points ?
		(*local_source_points)[(size_t)point] : NULL;
	    const repair_point_key key = {
		local_vertices[(size_t)point * 3],
		local_vertices[(size_t)point * 3 + 1],
		local_vertices[(size_t)point * 3 + 2]};
	    if (source) {
		const auto existing = source_points.find(source);
		if (existing != source_points.end()) {
		    local_to_input[(size_t)point] = existing->second;
		    continue;
		}
		source_points[source] = next_vertex;
	    } else {
		const auto existing = generated_points.find(key);
		if (existing != generated_points.end()) {
		    local_to_input[(size_t)point] = existing->second;
		    continue;
		}
		generated_points[key] = next_vertex;
	    }
	    local_to_input[(size_t)point] = next_vertex++;
	}
	std::vector<int> combined(input_faces,
	    input_faces + (size_t)input_face_count * 3);
	combined.reserve(combined.size() + (size_t)local_face_count * 3);
	for (int corner = 0; corner < local_face_count * 3; ++corner) {
	    const int local = local_faces[corner];
	    if (local < 0 || local >= local_vertex_count)
		return false;
	    combined.push_back(local_to_input[(size_t)local]);
	}
	std::vector<int> synchronized(combined.size());
	return bg_trimesh_sync(synchronized.data(), combined.data(),
	    input_face_count + local_face_count) >= 0;
    };
    const auto append_approximate_face = [&](int face_index,
	    const repair_boundary_patch *reference_boundary = NULL) {
	int *local_faces = NULL;
	int local_face_count = 0;
	fastf_t *local_vertices = NULL;
	int local_vertex_count = 0;
	std::vector<ON_3dPoint *> local_source_points;
	int one_face = face_index;
	bool assembled = ON_Brep_CDT_Mesh(&local_faces,
	    &local_face_count, &local_vertices, &local_vertex_count,
	    NULL, NULL, NULL, NULL, s_cdt, 1, &one_face) == 0;
	if (assembled && s_cdt->bot_pnt_to_on_pnt &&
		s_cdt->bot_pnt_to_on_pnt->size() ==
		(size_t)local_vertex_count)
	    local_source_points = *s_cdt->bot_pnt_to_on_pnt;
	if (assembled && reference_boundary &&
		!repair_patch_matches_source_boundary(local_faces,
		    local_face_count, local_source_points,
		    *reference_boundary)) {
	    if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY"))
		bu_log("Face %d: rejected a cleaned chart whose open edges "
		    "do not match its authoritative boundary\n", face_index);
	    assembled = false;
	}
	const int appended = assembled ? append_approximate_mesh(face_index,
	    local_faces, local_face_count, local_vertices,
	    local_vertex_count, local_source_points.empty() ? NULL :
	    &local_source_points) : 0;
	bu_free(local_faces, "approximate face mesh faces");
	bu_free(local_vertices, "approximate face mesh vertices");
	return appended;
    };

    /* Adaptive refinement may stop with a complete, intersection-free face
     * whose only remaining defect is disagreement with the B-Rep normal.
     * When the caller explicitly accepts self intersections, retain that
     * densely sampled surface interpretation as repair input.  It is appended
     * after the certified prefix and tagged as approximate, so it can never be
     * reported as rigorous geometry.  Earlier chart and PSLG failures remain
     * ineligible because they do not provide a trustworthy local topology. */
    int best_effort_face_count = 0;
    int best_effort_triangle_count = 0;
    size_t best_effort_fold_count = 0;
    std::set<int> best_effort_face_indices;
    if (!settings->use_full_fast_fallback &&
	    settings->use_fast_face_fallback &&
	    settings->mesh.allow_self_intersections) {
	for (int failed_face : failed_faces) {
	    const auto diagnostic =
		s_cdt->failed_face_diagnostics.find(failed_face);
	    const auto mesh = s_cdt->fmeshes.find(failed_face);
	    if (diagnostic == s_cdt->failed_face_diagnostics.end() ||
		    diagnostic->second.stage !=
		    BREP_CDT_STAGE_ADAPTIVE_REFINEMENT ||
		    diagnostic->second.result !=
		    BREP_CDT_RESULT_REFINEMENT_LIMIT ||
		    mesh == s_cdt->fmeshes.end() ||
		    !mesh->second.tris_tree.Count() ||
		    mesh->second.geometric_degenerate_count() ||
		    mesh->second.self_intersections(NULL, 1))
		continue;
	    const size_t folds = mesh->second.incorrect_normal_count();
	    const size_t triangle_count = mesh->second.tris_tree.Count();
	    if (folds > std::max((size_t)1, triangle_count /
		    MAX_BEST_EFFORT_FOLD_DIVISOR))
		continue;
	    const int appended = append_approximate_face(failed_face);
	    if (!appended)
		continue;
	    RTree<size_t, double, 3>::Iterator active;
	    mesh->second.tris_tree.GetFirst(active);
	    while (!active.IsNull()) {
		const size_t triangle_index = *active;
		if (triangle_index < mesh->second.tris_vect.size()) {
		    const triangle_t &triangle =
			mesh->second.tris_vect[triangle_index];
		    best_effort_surface_triangles[
			repair_triangle_coordinates(mesh->second, triangle)]
			.push_back(std::make_pair(failed_face, triangle));
		}
		++active;
	    }
	    locally_reconstructed_faces.insert(failed_face);
	    best_effort_face_indices.insert(failed_face);
	    best_effort_face_count++;
	    best_effort_triangle_count += appended;
	    best_effort_fold_count += folds;
	    bu_log("Face %d: retained the best adaptive surface mesh for "
		"repair (%d triangles, %zu folded)\n", failed_face,
		appended, folds);
	}
    }
    report->best_effort_faces = best_effort_face_count;
    report->best_effort_triangles = best_effort_triangle_count;
    report->best_effort_folded_triangles = (int)std::min(
	best_effort_fold_count, (size_t)INT_MAX);

    /* Before falling back to the display triangulator, retry failed faces by
     * cleaning only their weakly-simple chart boundary.  Successful faces
     * retain the rigorous surface sampling and exact shared-edge points; any
     * changed local boundary is still constrained to the original chart
     * segments.  Assemble them after the rigorous prefix so preservation and
     * fidelity accounting continue to distinguish certified input. */
    if (!settings->use_full_fast_fallback &&
	    settings->use_fast_face_fallback && !failed_faces.empty()) {
	const int64_t reconstruction_start = bu_gettime();
	const double reconstruction_deviation =
	    settings->max_surface_deviation > 0.0 ?
	    settings->max_surface_deviation : s_cdt->absmax;
	for (int failed_face : failed_faces) {
	    if (locally_reconstructed_faces.find(failed_face) !=
		    locally_reconstructed_faces.end())
		continue;
	    if ((bu_gettime() - reconstruction_start) / 1000 >=
		    settings->max_fast_time_ms)
		break;
	    if (!repair_approximate_face_triangulation(s_cdt, failed_face,
		    reconstruction_deviation))
		continue;
	    repair_boundary_patch reference_boundary;
	    const repair_fast_constraint_store constraints =
		repair_fast_face_constraints(s_cdt, failed_face);
	    const bool has_reference_boundary =
		repair_constrained_topological_disk(s_cdt, failed_face,
		    constraints, settings->max_fast_points,
		    reference_boundary);
	    const int local_face_count =
		append_approximate_face(failed_face, has_reference_boundary ?
		    &reference_boundary : NULL);
	    if (!local_face_count)
		continue;
	    locally_reconstructed_faces.insert(failed_face);
	    bu_log("Face %d: retained a locally cleaned surface chart for "
		"repair (%d triangles)\n", failed_face, local_face_count);
	}
    }

    /* Prefer topology-defined periodic reconstructions over an independent
     * display triangulation.  A side face joins rigorous neighboring loops;
     * a seam-only doubly periodic face uses a wrapped adaptive grid.  Both
     * leave the later fidelity checks to decide whether the deliberately
     * sparse local interpretation is close enough. */
    if (!settings->use_full_fast_fallback &&
	    settings->use_fast_face_fallback && !failed_faces.empty()) {
	const int64_t strip_start = bu_gettime();
	const int64_t strip_deadline = settings->max_fast_time_ms <=
	    (INT64_MAX - strip_start) / 1000 ? strip_start +
	    (int64_t)settings->max_fast_time_ms * 1000 : INT64_MAX;
	for (int failed_face : failed_faces) {
	    if (locally_reconstructed_faces.find(failed_face) !=
		    locally_reconstructed_faces.end())
		continue;
	    if ((bu_gettime() - strip_start) / 1000 >=
		    settings->max_fast_time_ms)
		break;
	    const repair_fast_constraint_store constraints =
		repair_fast_face_constraints(s_cdt, failed_face);
	    repair_boundary_patch patch;
	    const fastf_t allowed_deviation = settings->max_surface_deviation >
		0.0 ? settings->max_surface_deviation : s_cdt->absmax;
	    bool boundary_strip = repair_constrained_periodic_strip(s_cdt,
		failed_face, constraints, settings->max_fast_points,
		allowed_deviation, strip_deadline, patch);
	    if (boundary_strip && !approximate_mesh_orientable(
		    patch.faces.data(), (int)(patch.faces.size() / 3),
		    patch.vertices.data(), (int)(patch.vertices.size() / 3),
		    patch.source_points.empty() ? NULL :
		    &patch.source_points)) {
		const int alternate_step = -patch.strip_correspondence_step;
		repair_boundary_patch alternate;
		if (alternate_step && repair_constrained_periodic_strip(s_cdt,
			failed_face, constraints, settings->max_fast_points,
			allowed_deviation, strip_deadline, alternate,
			alternate_step, patch.strip_correspondence_shift) &&
			alternate.strip_correspondence_step == alternate_step &&
			approximate_mesh_orientable(alternate.faces.data(),
			    (int)(alternate.faces.size() / 3),
			    alternate.vertices.data(),
			    (int)(alternate.vertices.size() / 3),
			    alternate.source_points.empty() ? NULL :
			    &alternate.source_points)) {
		    patch = std::move(alternate);
		    bu_log("Face %d: selected the orientable periodic boundary "
			"correspondence\n", failed_face);
		} else {
		    boundary_strip = false;
		}
	    }
	    const bool spherical_cap = !boundary_strip &&
		repair_constrained_spherical_cap(s_cdt, failed_face,
		    constraints, settings->max_fast_points,
		    allowed_deviation, strip_deadline, patch);
	    const bool full_sphere = !boundary_strip && !spherical_cap &&
		repair_closed_spherical_surface(s_cdt, failed_face,
		    settings->max_fast_points, allowed_deviation,
		    strip_deadline, patch);
	    const bool periodic_grid = !boundary_strip && !spherical_cap &&
		!full_sphere &&
		repair_closed_periodic_surface(s_cdt, failed_face,
		    settings->max_fast_points, allowed_deviation,
		    strip_deadline, patch);
	    if (!boundary_strip && !spherical_cap && !full_sphere &&
		    !periodic_grid)
		continue;
	    const size_t patch_points = patch.vertices.size() / 3;
	    const size_t patch_faces = patch.faces.size() / 3;
	    const size_t patch_bytes = patch.vertices.size() *
		sizeof(fastf_t) + patch.faces.size() * sizeof(int) +
		patch.direct_surface_deviations.size() * sizeof(double);
	    if (patch_points > settings->max_fast_points ||
		    patch_bytes > settings->max_fast_result_bytes ||
		    patch_points > (size_t)INT_MAX ||
		    patch_faces > (size_t)INT_MAX ||
		    (!patch.direct_surface_deviations.empty() &&
		    patch.direct_surface_deviations.size() != patch_faces))
		continue;
	    const int appended = append_approximate_mesh(failed_face,
		patch.faces.data(), (int)patch_faces, patch.vertices.data(),
		(int)patch_points, patch.source_points.empty() ? NULL :
		&patch.source_points);
	    if (!appended)
		continue;
	    if (patch.direct_surface_deviations.size() == patch_faces) {
		for (size_t triangle = 0; triangle < patch_faces; ++triangle) {
		    const repair_triangle_key key = repair_triangle_coordinates(
			patch.vertices.data(), &patch.faces[triangle * 3]);
		    direct_surface_triangle_deviations[key] =
			patch.direct_surface_deviations[triangle];
		}
	    }
	    locally_reconstructed_faces.insert(failed_face);
	    if (boundary_strip) {
		report->boundary_strip_faces++;
		report->boundary_strip_triangles += appended;
		report->boundary_strip_constrained_edges +=
		    patch.constrained_edges;
		report->boundary_strip_constrained_samples +=
		    patch.constrained_samples;
		bu_log("Face %d: joined two rigorous %s boundaries with a "
		    "local %d-triangle strip (%zu constrained samples, "
		    "%zu interior rows)\n", failed_face,
		    patch.analytic_torus_strip ? "torus-isocircle" :
		    (patch.analytic_cylinder_strip ? "cylinder-isocircle" :
		    "periodic"), appended,
		    patch.constrained_samples, patch.strip_interior_rows);
	    } else if (full_sphere) {
		bu_log("Face %d: reconstructed a complete analytic sphere "
		    "across its artificial seam with %zu interior rows "
		    "(%d triangles)\n", failed_face,
		    patch.strip_interior_rows, appended);
	    } else if (spherical_cap) {
		bu_log("Face %d: reconstructed a spherical cap from %zu "
		    "constrained samples with %zu interior rows "
		    "(%d triangles)\n", failed_face,
		    patch.constrained_samples, patch.strip_interior_rows,
		    appended);
	    } else {
		bu_log("Face %d: reconstructed a seam-only doubly periodic "
		    "surface with a %zux%zu grid (%d triangles)\n",
		    failed_face, patch.periodic_u_samples,
		    patch.periodic_v_samples, appended);
	    }
	}
    }

    /* If a one-loop face has no consistent chart triangulation but every
     * shared edge has authoritative global samples, the loop topology still
     * defines a disk.  Preserve that exact boundary and span only the missing
     * face.  This intentionally approximate interpretation is restricted to
     * pre-refinement failures with a complete one-loop authoritative
     * boundary; later mesh and fidelity checks remain the acceptance gate. */
    if (!settings->use_full_fast_fallback &&
	    settings->use_fast_face_fallback && !failed_faces.empty()) {
	const int64_t disk_start = bu_gettime();
	for (int failed_face : failed_faces) {
	    if (locally_reconstructed_faces.find(failed_face) !=
		    locally_reconstructed_faces.end())
		continue;
	    if ((bu_gettime() - disk_start) / 1000 >=
		    settings->max_fast_time_ms)
		break;
	    const auto diagnostic =
		s_cdt->failed_face_diagnostics.find(failed_face);
	    if (diagnostic == s_cdt->failed_face_diagnostics.end() ||
		    !repair_topological_disk_stage(diagnostic->second.stage))
		continue;
	    const repair_fast_constraint_store constraints =
		repair_fast_face_constraints(s_cdt, failed_face);
	    repair_boundary_patch patch;
	    if (!repair_constrained_topological_disk(s_cdt, failed_face,
		    constraints, settings->max_fast_points, patch))
		continue;
	    const size_t patch_points = patch.vertices.size() / 3;
	    const size_t patch_faces = patch.faces.size() / 3;
	    const size_t patch_bytes = patch.vertices.size() *
		sizeof(fastf_t) + patch.faces.size() * sizeof(int);
	    if (patch_bytes > settings->max_fast_result_bytes ||
		    patch_points > (size_t)INT_MAX ||
		    patch_faces > (size_t)INT_MAX)
		continue;
	    const int appended = append_approximate_mesh(failed_face,
		patch.faces.data(), (int)patch_faces, patch.vertices.data(),
		(int)patch_points, patch.source_points.empty() ? NULL :
		&patch.source_points);
	    if (!appended)
		continue;
	    locally_reconstructed_faces.insert(failed_face);
	    report->topological_disk_faces++;
	    report->topological_disk_triangles += appended;
	    report->topological_disk_constrained_edges +=
		patch.constrained_edges;
	    report->topological_disk_constrained_samples +=
		patch.constrained_samples;
	    bu_log("Face %d: spanned an inconsistent B-Rep disk with "
		"%zu authoritative boundary samples (%d triangles)\n",
		failed_face, patch.constrained_samples, appended);
	}
    }

    if (settings->use_full_fast_fallback) {
	report->fast_fallback_attempted_faces =
	    s_cdt->orig_brep->m_F.Count();
	struct brep_cdt_fast_options fast_options;
	brep_cdt_fast_options_default(&fast_options);
	fast_options.max_workers = 1;
	fast_options.max_points = settings->max_fast_points;
	fast_options.max_result_bytes = settings->max_fast_result_bytes;
	fast_options.max_time_ms = settings->max_fast_time_ms;
	fast_options.allow_partial = 1;
	std::vector<repair_fast_face_range> fast_face_ranges(
	    (size_t)s_cdt->orig_brep->m_F.Count());
	fast_options.face_output = repair_fast_face_output;
	fast_options.face_output_data = &fast_face_ranges;
	struct brep_cdt_fast_report fast_report = {};
	int *fast_faces = NULL;
	int fast_face_count = 0;
	vect_t *fast_normals = NULL;
	point_t *fast_points = NULL;
	int fast_point_count = 0;
	struct bn_tol model_tolerance = BN_TOL_INIT_TOL;
	const int fast_result = brep_cdt_fast_ex(&fast_faces,
	    &fast_face_count, &fast_normals, &fast_points,
	    &fast_point_count, s_cdt->orig_brep, -1, &s_cdt->tol,
	    &model_tolerance, &fast_options, &fast_report);
	bool valid_fast_mesh =
	    (fast_result == BREP_CDT_FAST_OK ||
	    fast_result == BREP_CDT_FAST_PARTIAL) && fast_faces &&
	    fast_face_count > 0 && fast_normals && fast_points &&
	    fast_point_count > 0 &&
	    fast_face_count <= INT_MAX / 3 &&
	    (size_t)fast_point_count <= settings->max_fast_points &&
	    (!settings->use_poisson_reconstruction ||
	    (size_t)fast_face_count <= settings->max_fast_points) &&
	    fast_report.result_bytes <= settings->max_fast_result_bytes;
	for (int corner = 0; valid_fast_mesh &&
		corner < fast_face_count * 3; ++corner) {
	    valid_fast_mesh = fast_faces[corner] >= 0 &&
		fast_faces[corner] < fast_point_count;
	}
	for (int point = 0; valid_fast_mesh && point < fast_point_count;
		++point) {
	    valid_fast_mesh = std::isfinite(fast_points[point][X]) &&
		std::isfinite(fast_points[point][Y]) &&
		std::isfinite(fast_points[point][Z]);
	}
	for (int corner = 0; valid_fast_mesh &&
		corner < fast_face_count * 3; ++corner) {
	    valid_fast_mesh = std::isfinite(fast_normals[corner][X]) &&
		std::isfinite(fast_normals[corner][Y]) &&
		std::isfinite(fast_normals[corner][Z]);
	}
	if (!valid_fast_mesh) {
	    report->fast_fallback_failed_faces =
		std::max(1, fast_report.failed_faces);
	    bu_free(fast_faces, "repair full fast fallback faces");
	    bu_free(fast_normals, "repair full fast fallback normals");
	    bu_free(fast_points, "repair full fast fallback points");
	    bu_free(input_faces, "repair rigorous input faces");
	    bu_free(input_vertices, "repair rigorous input vertices");
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
		BREP_CDT_STAGE_MESH_REPAIR, -1,
		report->source_diagnostic.completed_faces,
		report->source_failed_faces,
		"whole-B-Rep fast fallback did not produce usable geometry");
	    return -1;
	}
	if (settings->use_poisson_reconstruction) {
	    report->poisson_reconstruction_attempted = 1;
	    report->fast_fallback_triangles = fast_face_count;
	    display_reference_faces.assign(fast_faces,
		fast_faces + (size_t)fast_face_count * 3);
	    display_reference_vertices.assign((fastf_t *)fast_points,
		(fastf_t *)fast_points + (size_t)fast_point_count * 3);
	    display_reference_face_count = fast_face_count;
	    display_reference_brep_faces.assign((size_t)fast_face_count, -1);
	    for (size_t brep_face = 0; brep_face < fast_face_ranges.size();
		    ++brep_face) {
		const repair_fast_face_range &range =
		    fast_face_ranges[brep_face];
		if (!range.present || range.first_face >
			(size_t)fast_face_count || range.face_count >
			(size_t)fast_face_count - range.first_face)
		    continue;
		for (size_t face = range.first_face;
			face < range.first_face + range.face_count; ++face)
		    display_reference_brep_faces[face] = (int)brep_face;
	    }
	    std::vector<int> face_components =
		repair_brep_face_components(s_cdt->orig_brep);
	    const bool closed_face_components =
		repair_brep_components_are_closed(s_cdt->orig_brep,
		    face_components);
	    std::vector<repair_poisson_triangle> poisson_triangles;
	    double poisson_triangle_area = 0.0;
	    bool valid_component_ranges = face_components.size() ==
		fast_face_ranges.size();
	    for (size_t brep_face = 0; valid_component_ranges &&
		    brep_face < fast_face_ranges.size(); ++brep_face) {
		const repair_fast_face_range &range =
		    fast_face_ranges[brep_face];
		if (!range.present)
		    continue;
		if (range.first_face > (size_t)fast_face_count ||
			range.face_count > (size_t)fast_face_count -
			range.first_face ||
			range.first_point > (size_t)fast_point_count ||
			range.point_count > (size_t)fast_point_count -
			range.first_point) {
		    valid_component_ranges = false;
		    break;
		}
		const int component = closed_face_components ?
		    face_components[brep_face] : 0;
		for (size_t face = range.first_face;
			face < range.first_face + range.face_count; ++face) {
		    const int *triangle = &fast_faces[face * 3];
		    for (int corner = 0; corner < 3; ++corner) {
			if (triangle[corner] < (int)range.first_point ||
				triangle[corner] >= (int)(range.first_point +
				range.point_count)) {
			    valid_component_ranges = false;
			    break;
			}
		    }
		    if (!valid_component_ranges)
			break;
		    const point_t &a = fast_points[triangle[0]];
		    const point_t &b = fast_points[triangle[1]];
		    const point_t &c = fast_points[triangle[2]];
		    vect_t ab, ac, normal;
		    VSUB2(ab, b, a);
		    VSUB2(ac, c, a);
		    VCROSS(normal, ab, ac);
		    const fastf_t normal_length = MAGNITUDE(normal);
		    if (!(normal_length > SMALL_FASTF) ||
			    !std::isfinite(normal_length))
			continue;
		    repair_poisson_triangle sample_triangle;
		    sample_triangle.face = face;
		    sample_triangle.component = component;
		    sample_triangle.area = 0.5 * normal_length;
		    /* Fast-CDT winding is face-local.  Its corner normals include
		     * the B-Rep face reversal and therefore provide the coherent
		     * outward orientation Poisson reconstruction requires. */
		    VSETALL(sample_triangle.normal, 0.0);
		    for (int corner = 0; corner < 3; ++corner) {
			VMOVE(sample_triangle.corner_normals[corner],
			    fast_normals[face * 3 + (size_t)corner]);
			const fastf_t corner_normal_length = MAGNITUDE(
			    sample_triangle.corner_normals[corner]);
			if (corner_normal_length > SMALL_FASTF &&
				std::isfinite(corner_normal_length)) {
			    VSCALE(sample_triangle.corner_normals[corner],
				sample_triangle.corner_normals[corner],
				1.0 / corner_normal_length);
			} else {
			    VSCALE(sample_triangle.corner_normals[corner], normal,
				1.0 / normal_length);
			}
			VADD2(sample_triangle.normal, sample_triangle.normal,
			    sample_triangle.corner_normals[corner]);
		    }
		    const fastf_t surface_normal_length =
			MAGNITUDE(sample_triangle.normal);
		    if (surface_normal_length > SMALL_FASTF &&
			    std::isfinite(surface_normal_length)) {
			VSCALE(sample_triangle.normal, sample_triangle.normal,
			    1.0 / surface_normal_length);
		    } else {
			VSCALE(sample_triangle.normal, normal,
			    1.0 / normal_length);
		    }
		    poisson_triangle_area += sample_triangle.area;
		    poisson_triangles.push_back(sample_triangle);
		}
	    }
	    std::map<int, std::vector<repair_poisson_sample>> component_samples;
	    std::vector<size_t> triangle_sample_counts(
		poisson_triangles.size(), 1);
	    if (!poisson_triangles.empty() && poisson_triangle_area > 0.0 &&
		    std::isfinite(poisson_triangle_area)) {
		const size_t point_capacity = std::min(settings->max_fast_points,
		    (size_t)INT_MAX);
		const size_t desired_extra = area_weighted_samples ?
		    poisson_triangles.size() : 0;
		const size_t extra_budget = point_capacity >
		    poisson_triangles.size() ?
		    std::min(desired_extra, point_capacity -
			poisson_triangles.size()) : 0;
		size_t triangle = 0;
		long double cumulative_area = poisson_triangles[0].area;
		for (size_t extra = 0; extra < extra_budget; ++extra) {
		    const long double target =
			((long double)extra + 0.5L) /
			(long double)extra_budget *
			(long double)poisson_triangle_area;
		    while (triangle + 1 < poisson_triangles.size() &&
			    cumulative_area <= target) {
			triangle++;
			cumulative_area += poisson_triangles[triangle].area;
		    }
		    triangle_sample_counts[triangle]++;
		}
	    }
	    for (size_t triangle_index = 0;
		    triangle_index < poisson_triangles.size(); ++triangle_index) {
		const repair_poisson_triangle &sample_triangle =
		    poisson_triangles[triangle_index];
		const int *triangle = &fast_faces[sample_triangle.face * 3];
		const point_t &a = fast_points[triangle[0]];
		const point_t &b = fast_points[triangle[1]];
		const point_t &c = fast_points[triangle[2]];
		std::vector<repair_poisson_sample> &samples =
		    component_samples[sample_triangle.component];
		const size_t sample_count =
		    triangle_sample_counts[triangle_index];
		for (size_t sample_index = 0; sample_index < sample_count;
			sample_index++) {
		    repair_poisson_sample sample;
		    double first_weight = 1.0 / 3.0;
		    double second_weight = 1.0 / 3.0;
		    double final_weight = 1.0 / 3.0;
		    if (sample_count == 1) {
			VADD3(sample.point, a, b, c);
			VSCALE(sample.point, sample.point, 1.0 / 3.0);
		    } else {
			const double root = std::sqrt(
			    ((double)sample_index + 0.5) /
			    (double)sample_count);
			const double third_weight = repair_radical_inverse(
			    sample_index);
			first_weight = 1.0 - root;
			second_weight = root *
			    (1.0 - third_weight);
			final_weight = root * third_weight;
			for (int axis = 0; axis < 3; ++axis) {
			    sample.point[axis] = first_weight * a[axis] +
				second_weight * b[axis] +
				final_weight * c[axis];
			}
		    }
		    for (int axis = 0; axis < 3; ++axis) {
			sample.normal[axis] = first_weight *
			    sample_triangle.corner_normals[0][axis] +
			    second_weight *
			    sample_triangle.corner_normals[1][axis] +
			    final_weight *
			    sample_triangle.corner_normals[2][axis];
		    }
		    const fastf_t sample_normal_length = MAGNITUDE(sample.normal);
		    if (sample_normal_length > SMALL_FASTF &&
			    std::isfinite(sample_normal_length)) {
			VSCALE(sample.normal, sample.normal,
			    1.0 / sample_normal_length);
		    } else {
			VMOVE(sample.normal, sample_triangle.normal);
		    }
		    samples.push_back(sample);
		}
	    }
	    report->poisson_components = (int)component_samples.size();
	    report->poisson_area_sampling_applied =
		area_weighted_samples ? 1 : 0;
	    report->poisson_boundary_fallback_applied =
		closure_biased_poisson ? 1 : 0;
	    for (const auto &sample_set : component_samples)
		report->poisson_input_points += (int)sample_set.second.size();
	    struct bg_3d_spsr_opts poisson_options = BG_3D_SPSR_OPTS_DEFAULT;
	    poisson_options.depth = settings->poisson_depth;
	    poisson_options.full_depth = std::min(5,
		settings->poisson_depth);
	    poisson_options.threads = 1;
	    poisson_options.scale = settings->poisson_scale;
	    if (closure_biased_poisson) {
		poisson_options.btype = BG_3D_SPSR_BOUNDARY_DIRICHLET;
		poisson_options.point_weight = 32.0;
		poisson_options.exact = 0;
	    }
	    report->poisson_scale = settings->poisson_scale;
	    std::vector<int> combined_poisson_faces;
	    std::vector<fastf_t> combined_poisson_points;
	    bool valid_poisson = valid_component_ranges &&
		!component_samples.empty() && component_samples.size() <=
		settings->max_poisson_components;
	    std::string poisson_failure =
		"bounded component Poisson reconstruction did not produce "
		"usable geometry";
	    if (valid_component_ranges && component_samples.size() >
		    settings->max_poisson_components) {
		poisson_failure = "Poisson reconstruction found " +
		    std::to_string(component_samples.size()) +
		    " face components (limit " +
		    std::to_string(settings->max_poisson_components) + ")";
	    }
	    for (const auto &sample_set : component_samples) {
		if (!valid_poisson)
		    break;
		const std::vector<repair_poisson_sample> &samples =
		    sample_set.second;
		if (samples.size() <= 3 || samples.size() > (size_t)INT_MAX) {
		    valid_poisson = false;
		    poisson_failure = "a B-Rep face component supplied only " +
			std::to_string(samples.size()) +
			" usable Poisson samples";
		    break;
		}
		point_t *poisson_samples = (point_t *)bu_malloc(
		    samples.size() * sizeof(point_t),
		    "component Poisson centroid samples");
		vect_t *poisson_sample_normals = (vect_t *)bu_malloc(
		    samples.size() * sizeof(vect_t),
		    "component Poisson face normals");
		for (size_t sample = 0; sample < samples.size(); ++sample) {
		    VMOVE(poisson_samples[sample], samples[sample].point);
		    VMOVE(poisson_sample_normals[sample],
			samples[sample].normal);
		}
		int *component_faces = NULL;
		int component_face_count = 0;
		point_t *component_points = NULL;
		int component_point_count = 0;
		const int poisson_result = bg_3d_spsr(&component_faces,
		    &component_face_count, &component_points,
		    &component_point_count, poisson_samples,
		    poisson_sample_normals, (int)samples.size(),
		    &poisson_options);
		bu_free(poisson_samples,
		    "component Poisson centroid samples");
		bu_free(poisson_sample_normals,
		    "component Poisson face normals");
		valid_poisson = poisson_result == 0 && component_faces &&
		    component_face_count > 0 && component_face_count <=
		    INT_MAX / 3 && component_points && component_point_count > 0;
		for (int corner = 0; valid_poisson &&
			corner < component_face_count * 3; ++corner) {
		    valid_poisson = component_faces[corner] >= 0 &&
			component_faces[corner] < component_point_count;
		}
		for (int point = 0; valid_poisson &&
			point < component_point_count; ++point) {
		    valid_poisson = std::isfinite(component_points[point][X]) &&
			std::isfinite(component_points[point][Y]) &&
			std::isfinite(component_points[point][Z]);
		}
		const size_t combined_point_count =
		    combined_poisson_points.size() / 3;
		const size_t combined_face_count =
		    combined_poisson_faces.size() / 3;
		const size_t new_point_count = combined_point_count +
		    (valid_poisson ? (size_t)component_point_count : 0);
		const size_t new_face_count = combined_face_count +
		    (valid_poisson ? (size_t)component_face_count : 0);
		const size_t new_bytes = new_point_count * sizeof(point_t) +
		    new_face_count * 3 * sizeof(int);
		valid_poisson = valid_poisson && new_point_count <=
		    settings->max_fast_points && new_point_count <=
		    (size_t)INT_MAX && new_face_count <= (size_t)INT_MAX &&
		    new_bytes <= settings->max_fast_result_bytes;
		if (valid_poisson) {
		    for (int corner = 0; corner < component_face_count * 3;
			    ++corner) {
			combined_poisson_faces.push_back(
			    component_faces[corner] +
			    (int)combined_point_count);
		    }
		    const fastf_t *component_coordinates =
			(const fastf_t *)component_points;
		    combined_poisson_points.insert(
			combined_poisson_points.end(), component_coordinates,
			component_coordinates +
			(size_t)component_point_count * 3);
		}
		bu_free(component_faces, "component Poisson repair faces");
		bu_free(component_points, "component Poisson repair points");
	    }
	    if (!valid_poisson) {
		bu_free(fast_faces, "Poisson source faces");
		bu_free(fast_normals, "Poisson source normals");
		bu_free(fast_points, "Poisson source points");
		bu_free(input_faces, "repair rigorous input faces");
		bu_free(input_vertices, "repair rigorous input vertices");
		cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
		    BREP_CDT_STAGE_MESH_REPAIR, -1,
		    report->source_diagnostic.completed_faces,
		    report->source_failed_faces,
		    poisson_failure.c_str());
		return -1;
	    }
	    int *poisson_faces = (int *)bu_malloc(
		combined_poisson_faces.size() * sizeof(int),
		"combined Poisson repair faces");
	    point_t *poisson_points = (point_t *)bu_malloc(
		(combined_poisson_points.size() / 3) * sizeof(point_t),
		"combined Poisson repair points");
	    memcpy(poisson_faces, combined_poisson_faces.data(),
		combined_poisson_faces.size() * sizeof(int));
	    memcpy(poisson_points, combined_poisson_points.data(),
		combined_poisson_points.size() * sizeof(fastf_t));
	    const int poisson_face_count =
		(int)(combined_poisson_faces.size() / 3);
	    const int poisson_point_count =
		(int)(combined_poisson_points.size() / 3);
	    bu_free(fast_faces, "Poisson source faces");
	    bu_free(fast_points, "Poisson source points");
	    fast_faces = poisson_faces;
	    fast_face_count = poisson_face_count;
	    fast_points = poisson_points;
	    fast_point_count = poisson_point_count;
	    report->poisson_reconstruction_applied = 1;
	    report->poisson_output_points = poisson_point_count;
	    report->poisson_output_faces = poisson_face_count;
	}
	bu_free(fast_normals, "repair full fast fallback normals");
	bu_free(input_faces, "repair rigorous input faces");
	bu_free(input_vertices, "repair rigorous input vertices");
	input_faces = fast_faces;
	input_face_count = fast_face_count;
	input_vertices = (fastf_t *)fast_points;
	input_vertex_count = fast_point_count;
	rigorous_input_face_count = 0;
	input_face_brep_sources.assign((size_t)fast_face_count, -1);
	if (!report->poisson_reconstruction_applied) {
	    for (size_t brep_face = 0; brep_face < fast_face_ranges.size();
		    ++brep_face) {
		const repair_fast_face_range &range =
		    fast_face_ranges[brep_face];
		if (!range.present || range.first_face >
			(size_t)fast_face_count || range.face_count >
			(size_t)fast_face_count - range.first_face)
		    continue;
		for (size_t face = range.first_face;
			face < range.first_face + range.face_count; ++face)
		    input_face_brep_sources[face] = (int)brep_face;
	    }
	}
	report->fast_fallback_used_faces = fast_report.completed_faces;
	report->fast_fallback_failed_faces = fast_report.failed_faces;
	if (!report->fast_fallback_triangles)
	    report->fast_fallback_triangles = fast_face_count;
	report->full_fast_fallback_used = 1;
    }

    if (!settings->use_full_fast_fallback &&
	    settings->use_fast_face_fallback && !failed_faces.empty()) {
	const int64_t fast_start = bu_gettime();
	size_t fast_points_used = 0;
	size_t fast_bytes_used = 0;
	for (int failed_face : failed_faces) {
	    if (locally_reconstructed_faces.find(failed_face) !=
		    locally_reconstructed_faces.end())
		continue;
	    const int64_t elapsed_ms = (bu_gettime() - fast_start) / 1000;
	    if (elapsed_ms >= settings->max_fast_time_ms ||
		    fast_points_used >= settings->max_fast_points ||
		    fast_bytes_used >= settings->max_fast_result_bytes)
		break;
	    report->fast_fallback_attempted_faces++;
	    struct brep_cdt_fast_options fast_options;
	    brep_cdt_fast_options_default(&fast_options);
	    fast_options.max_workers = 1;
	    fast_options.max_points = settings->max_fast_points -
		fast_points_used;
	    fast_options.max_result_bytes = settings->max_fast_result_bytes -
		fast_bytes_used;
	    fast_options.max_time_ms = settings->max_fast_time_ms -
		(long)elapsed_ms;
	    fast_options.allow_partial = 1;
	    repair_fast_constraint_store constraints =
		repair_fast_face_constraints(s_cdt, failed_face);
	    std::vector<ON_3dPoint *> fast_point_sources;
	    if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY")) {
		size_t identified_samples = 0;
		for (const auto &trim : constraints.trims) {
		    for (const repair_fast_trim_sample &sample : trim.second)
			identified_samples += sample.source_point ? 1 : 0;
		}
		bu_log("Face %d: supplied %zu/%zu authoritative fast face "
		    "constraint identities\n", failed_face, identified_samples,
		    constraints.constrained_samples);
	    }
	    if (constraints.constrained_edges) {
		fast_options.trim_sample_count =
		    repair_fast_trim_sample_count;
		fast_options.trim_sample = repair_fast_trim_sample_get;
		fast_options.trim_sample_data = &constraints;
		fast_options.trim_sample_source =
		    repair_fast_trim_sample_source;
		fast_options.point_source = repair_fast_point_source;
		fast_options.point_source_data = &fast_point_sources;
	    }
	    struct brep_cdt_fast_report fast_report = {};
	    int *fast_faces = NULL;
	    int fast_face_count = 0;
	    vect_t *fast_normals = NULL;
	    point_t *fast_points = NULL;
	    int fast_point_count = 0;
	    struct bn_tol model_tolerance = BN_TOL_INIT_TOL;
	    const int fast_result = brep_cdt_fast_ex(&fast_faces,
		&fast_face_count, &fast_normals, &fast_points,
		&fast_point_count, s_cdt->orig_brep, failed_face,
		&s_cdt->tol, &model_tolerance, &fast_options,
		&fast_report);
	    bool valid_fast_mesh =
		(fast_result == BREP_CDT_FAST_OK ||
		 fast_result == BREP_CDT_FAST_PARTIAL) &&
		fast_faces && fast_face_count > 0 && fast_points &&
		fast_point_count > 0;
	    for (int corner = 0; valid_fast_mesh &&
		    corner < fast_face_count * 3; ++corner) {
		valid_fast_mesh = fast_faces[corner] >= 0 &&
		    fast_faces[corner] < fast_point_count;
	    }
	    for (int point = 0; valid_fast_mesh &&
		    point < fast_point_count; ++point) {
		valid_fast_mesh = std::isfinite(fast_points[point][X]) &&
		    std::isfinite(fast_points[point][Y]) &&
		    std::isfinite(fast_points[point][Z]);
	    }
	    const bool within_limits = valid_fast_mesh &&
		(size_t)fast_point_count <=
		    settings->max_fast_points - fast_points_used &&
		    fast_report.result_bytes <=
		    settings->max_fast_result_bytes - fast_bytes_used &&
		(fast_point_count <= INT_MAX - input_vertex_count &&
		fast_face_count <= INT_MAX - input_face_count);
	    if (within_limits) {
		std::vector<int> fast_to_input((size_t)fast_point_count, -1);
		std::vector<fastf_t> novel_vertices;
		std::map<repair_point_key, int> local_generated_points;
		novel_vertices.reserve((size_t)fast_point_count * 3);
		size_t source_vertices = 0;
		size_t matched_source_vertices = 0;
		for (int fast_point = 0; fast_point < fast_point_count;
			++fast_point) {
		    ON_3dPoint *source = (size_t)fast_point <
			fast_point_sources.size() ?
			fast_point_sources[(size_t)fast_point] : NULL;
		    source_vertices += source ? 1 : 0;
		    const repair_point_key key = {
			fast_points[fast_point][X], fast_points[fast_point][Y],
			fast_points[fast_point][Z]};
		    if (source) {
			const auto existing = approximate_source_points.find(source);
			if (existing != approximate_source_points.end()) {
			    fast_to_input[(size_t)fast_point] = existing->second;
			    matched_source_vertices++;
			    continue;
			}
		    } else {
			const auto existing = local_generated_points.find(key);
			if (existing != local_generated_points.end()) {
			    fast_to_input[(size_t)fast_point] = existing->second;
			    continue;
			}
		    }
		    const int new_index = input_vertex_count +
			(int)(novel_vertices.size() / 3);
		    if (source)
			approximate_source_points[source] = new_index;
		    else
			local_generated_points[key] = new_index;
		    fast_to_input[(size_t)fast_point] = new_index;
		    novel_vertices.insert(novel_vertices.end(),
			&fast_points[fast_point][X], &fast_points[fast_point][X] + 3);
		}
		if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY"))
		    bu_log("Face %d: matched %zu/%zu authoritative fast face "
			"vertices by CDT point identity\n", failed_face,
			matched_source_vertices, source_vertices);
		const size_t new_vertex_count = (size_t)input_vertex_count +
		    novel_vertices.size() / 3;
		const size_t new_face_count = (size_t)input_face_count +
		    (size_t)fast_face_count;
		input_vertices = (fastf_t *)bu_realloc(input_vertices,
		    new_vertex_count * 3 * sizeof(fastf_t),
		    "repair input vertices with fast fallback");
		input_faces = (int *)bu_realloc(input_faces,
		    new_face_count * 3 * sizeof(int),
		    "repair input faces with fast fallback");
		if (!novel_vertices.empty())
		    memcpy(&input_vertices[(size_t)input_vertex_count * 3],
			novel_vertices.data(), novel_vertices.size() *
			sizeof(fastf_t));
		for (int corner = 0; corner < fast_face_count * 3; ++corner)
		    input_faces[(size_t)input_face_count * 3 + corner] =
			fast_to_input[(size_t)fast_faces[corner]];
		for (int face = 0; face < fast_face_count; ++face) {
		    const repair_triangle_key key = repair_triangle_coordinates(
			input_vertices,
			&input_faces[((size_t)input_face_count + face) * 3]);
		    fast_triangle_brep_faces[key].insert(failed_face);
		}
		input_vertex_count = (int)new_vertex_count;
		input_face_count += fast_face_count;
		input_face_brep_sources.insert(input_face_brep_sources.end(),
		    (size_t)fast_face_count, failed_face);
		report->fast_fallback_used_faces++;
		report->fast_fallback_triangles += fast_face_count;
		report->fast_fallback_constrained_edges +=
		    constraints.constrained_edges;
		report->fast_fallback_constrained_samples +=
		    constraints.constrained_samples;
		fast_points_used += (size_t)fast_point_count;
		fast_bytes_used += fast_report.result_bytes;
	    } else {
		report->fast_fallback_failed_faces++;
	    }
	    bu_free(fast_faces, "repair fast fallback faces");
	    bu_free(fast_normals, "repair fast fallback normals");
	    bu_free(fast_points, "repair fast fallback points");
	}
    }

    std::set<int> duplicate_sources;
    size_t duplicate_count = 0;
    std::map<int, std::pair<size_t, size_t>> duplicate_source_counts;
    const size_t quarantined_triangles =
	repair_quarantine_duplicate_source_faces(input_faces,
	    input_face_count, input_vertices, &rigorous_input_face_count,
	    input_face_brep_sources, duplicate_sources, &duplicate_count,
	    &duplicate_source_counts);
    if (quarantined_triangles) {
	rigorous_input_brep_faces.assign(input_face_brep_sources.begin(),
	    input_face_brep_sources.begin() + rigorous_input_face_count);
	approximation_faces.insert(duplicate_sources.begin(),
	    duplicate_sources.end());
	bu_log("Quarantined %zu triangles from %zu retained B-Rep faces "
	    "after finding %zu exact, equally oriented duplicates\n",
	    quarantined_triangles, duplicate_sources.size(), duplicate_count);
	if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY")) {
	    for (int source : duplicate_sources)
		bu_log("  source face %d: %zu of %zu triangles were exact "
		    "duplicates\n", source,
		    duplicate_source_counts[source].first,
		    duplicate_source_counts[source].second);
	}
    }

    /* A failed face may still supply a bounded best-effort surface mesh, a
     * cleaned local chart, or a constrained fast triangulation.  Do not reject
     * an empty rigorous prefix until each enabled local recovery path has had
     * an opportunity to contribute geometry. */
    if (input_face_count <= 0 || input_vertex_count <= 0 || !input_faces ||
	    !input_vertices) {
	bu_free(input_faces, "empty repair input faces");
	bu_free(input_vertices, "empty repair input vertices");
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces,
	    "mesh repair did not obtain usable B-Rep face geometry");
	return -1;
    }

    if (automatic_local_repair && input_face_count >
	    MAX_AUTOMATIC_LOCAL_REPAIR_FACES) {
	bu_free(input_faces, "oversized automatic local repair faces");
	bu_free(input_vertices, "oversized automatic local repair vertices");
	std::string message = "automatic local mesh repair skipped " +
	    std::to_string(input_face_count) + " triangles (limit " +
	    std::to_string(MAX_AUTOMATIC_LOCAL_REPAIR_FACES) + ")";
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }

    if (settings->use_full_fast_fallback &&
	    !report->poisson_reconstruction_applied) {
	struct bg_trimesh_solid_errors edge_errors =
	    BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	(void)bg_trimesh_solid2(input_vertex_count, input_face_count,
	    input_vertices, input_faces, &edge_errors);
	const bool edge_closed = !edge_errors.unmatched.count &&
	    !edge_errors.excess.count && !edge_errors.misoriented.count;
	bg_free_trimesh_solid_errors(&edge_errors);
	assembled_mesh_validation pre_weld_validation;
	(void)assembled_mesh_validate(input_vertex_count, input_face_count,
	    input_vertices, input_faces, &pre_weld_validation, false);
	if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY"))
	    bu_log("Whole-display pre-weld: edge closed %d, degenerate %zu, "
		"invalid links %zu, triangles %d\n", (int)edge_closed,
		pre_weld_validation.degenerate_faces,
		pre_weld_validation.invalid_vertex_links, input_face_count);
	std::vector<int> weld_trial;
	size_t welded = 0;
	if (edge_closed && pre_weld_validation.degenerate_faces &&
		pre_weld_validation.invalid_vertex_links) {
	    weld_trial.assign(input_faces,
		input_faces + (size_t)input_face_count * 3);
	    welded = repair_weld_roundoff_vertices(weld_trial.data(),
		input_face_count, input_vertices, input_vertex_count);
	    assembled_mesh_validation post_weld_validation;
	    (void)assembled_mesh_validate(input_vertex_count, input_face_count,
		input_vertices, weld_trial.data(), &post_weld_validation, false);
	    const size_t local_link_limit = std::max((size_t)64,
		post_weld_validation.degenerate_faces <= SIZE_MAX / 4 ?
		4 * post_weld_validation.degenerate_faces : SIZE_MAX);
	    if (!welded || post_weld_validation.invalid_vertex_links >
		    local_link_limit) {
		if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY"))
		    bu_log("Whole-display roundoff normalization declined: %zu "
			"welds leave %zu invalid links (local limit %zu)\n",
			welded, post_weld_validation.invalid_vertex_links,
			local_link_limit);
		welded = 0;
	    } else {
		memcpy(input_faces, weld_trial.data(),
		    weld_trial.size() * sizeof(int));
	    }
	}
	if (welded)
	    bu_log("Normalized %zu whole-display vertices separated only by "
		"coordinate roundoff before local repair\n", welded);
    }

    if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY")) {
	bu_log("Repair input mesh: %d vertices, %d triangles (%d rigorous)\n",
	    input_vertex_count, input_face_count, rigorous_input_face_count);
	assembled_mesh_validation input_validation;
	const bool input_geometric = assembled_mesh_validate(input_vertex_count,
	    input_face_count, input_vertices, input_faces, &input_validation,
	    false);
	struct bg_trimesh_solid_errors input_errors =
	    BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	(void)bg_trimesh_solid2(input_vertex_count, input_face_count,
	    input_vertices, input_faces, &input_errors);
	bu_log("Repair input: geometric %d, unmatched %d, excess %d, "
	    "misoriented %d, invalid links %zu\n", (int)input_geometric,
	    input_errors.unmatched.count, input_errors.excess.count,
	    input_errors.misoriented.count,
	    input_validation.invalid_vertex_links);
	bg_free_trimesh_solid_errors(&input_errors);

	/* Classify geometric degeneracies before generic repair removes them.
	 * Edge-connected components describe the actual neighborhoods that would
	 * be opened; source-face attribution distinguishes a bad fallback patch
	 * from collateral damage to retained rigorous faces. */
	std::vector<int> degenerate_faces;
	std::map<std::pair<int, int>, std::vector<int>> degenerate_edges;
	for (int face = 0; face < input_face_count; ++face) {
	    ON_3dPoint points[3];
	    for (int corner = 0; corner < 3; ++corner) {
		const int vertex = input_faces[(size_t)face * 3 + corner];
		points[corner] = ON_3dPoint(
		    &input_vertices[(size_t)vertex * 3]);
	    }
	    const ON_3dVector e01 = points[1] - points[0];
	    const ON_3dVector e02 = points[2] - points[0];
	    const ON_3dVector e12 = points[2] - points[1];
	    const double longest_squared = std::max(e01.LengthSquared(),
		std::max(e02.LengthSquared(), e12.LengthSquared()));
	    const double doubled_area = ON_CrossProduct(e01, e02).Length();
	    if (longest_squared > 0.0 && std::isfinite(doubled_area) &&
		    doubled_area > 64.0 *
		    std::numeric_limits<double>::epsilon() * longest_squared)
		continue;
	    const int local = (int)degenerate_faces.size();
	    degenerate_faces.push_back(face);
	    for (int corner = 0; corner < 3; ++corner) {
		int first = input_faces[(size_t)face * 3 + corner];
		int second = input_faces[(size_t)face * 3 +
		    (corner + 1) % 3];
		if (second < first)
		    std::swap(first, second);
		degenerate_edges[{first, second}].push_back(local);
	    }
	}
	std::vector<int> parents(degenerate_faces.size());
	std::iota(parents.begin(), parents.end(), 0);
	const auto component_root = [&](int item) {
	    int root = item;
	    while (parents[(size_t)root] != root)
		root = parents[(size_t)root];
	    while (parents[(size_t)item] != item) {
		const int next = parents[(size_t)item];
		parents[(size_t)item] = root;
		item = next;
	    }
	    return root;
	};
	for (const auto &edge : degenerate_edges) {
	    if (edge.second.size() < 2)
		continue;
	    const int first = component_root(edge.second.front());
	    for (size_t use = 1; use < edge.second.size(); ++use) {
		const int later = component_root(edge.second[use]);
		if (later != first)
		    parents[(size_t)later] = first;
	    }
	}
	struct degenerate_component {
	    std::vector<int> triangles;
	    std::set<int> source_faces;
	    size_t boundary_edges = 0;
	};
	std::map<int, degenerate_component> components;
	for (size_t local = 0; local < degenerate_faces.size(); ++local) {
	    const int input_face = degenerate_faces[local];
	    degenerate_component &component = components[
		component_root((int)local)];
	    component.triangles.push_back(input_face);
	    if ((size_t)input_face < input_face_brep_sources.size() &&
		    input_face_brep_sources[(size_t)input_face] >= 0) {
		component.source_faces.insert(
		    input_face_brep_sources[(size_t)input_face]);
		continue;
	    }
	    if (input_face < rigorous_input_face_count &&
		    (size_t)input_face < rigorous_input_brep_faces.size()) {
		component.source_faces.insert(
		    rigorous_input_brep_faces[(size_t)input_face]);
		continue;
	    }
	    const repair_triangle_key key = repair_triangle_coordinates(
		input_vertices, &input_faces[(size_t)input_face * 3]);
	    const auto local_source = local_chart_triangle_brep_faces.find(key);
	    if (local_source != local_chart_triangle_brep_faces.end())
		component.source_faces.insert(local_source->second.begin(),
		    local_source->second.end());
	    const auto fast_source = fast_triangle_brep_faces.find(key);
	    if (fast_source != fast_triangle_brep_faces.end())
		component.source_faces.insert(fast_source->second.begin(),
		    fast_source->second.end());
	}
	for (const auto &edge : degenerate_edges) {
	    if (edge.second.size() != 1)
		continue;
	    components[component_root(edge.second.front())].boundary_edges++;
	}
	std::vector<const degenerate_component *> ordered_components;
	for (const auto &component : components)
	    ordered_components.push_back(&component.second);
	std::sort(ordered_components.begin(), ordered_components.end(),
	    [](const degenerate_component *first,
		const degenerate_component *second) {
		return first->triangles.size() > second->triangles.size();
	    });
	bu_log("Repair input: %zu geometric degenerate triangles in %zu "
	    "edge-connected neighborhoods\n", degenerate_faces.size(),
	    ordered_components.size());
	const size_t logged_components = std::min((size_t)64,
	    ordered_components.size());
	for (size_t index = 0; index < logged_components; ++index) {
	    const degenerate_component &component =
		*ordered_components[index];
	    std::string sources;
	    for (int face : component.source_faces) {
		if (!sources.empty())
		    sources += ",";
		sources += std::to_string(face);
	    }
	    bu_log("Repair degenerate neighborhood %zu: %zu triangles, %zu "
		"boundary edges, B-Rep faces %s\n", index,
		component.triangles.size(), component.boundary_edges,
		sources.empty() ? "unknown" : sources.c_str());
	}
	if (logged_components < ordered_components.size())
	    bu_log("Repair degenerate neighborhoods: omitted %zu smaller "
		"components\n", ordered_components.size() - logged_components);
    }

    /* A locally reconstructed face is oriented from its damaged source
     * surface, whose normals can disagree with the otherwise authoritative
     * closed shell.  Once exact shared-edge identity proves the assembled
     * candidate closed, synchronize winding transactionally before invoking
     * a geometric repair that could split the valid topology. */
    if (!locally_reconstructed_faces.empty()) {
	const int synchronized = closed_mesh_orientation_sync(input_faces,
	    input_face_count, input_vertices, input_vertex_count, NULL);
	if (synchronized > 0)
	    bu_log("Synchronized %d locally reconstructed triangle "
		"orientations after complete closed-mesh validation\n",
	    synchronized);
    }

    const double pre_neighborhood_repair_area = repair_mesh_area(
	input_vertices, input_faces, input_face_count);
    repair_degenerate_neighborhood_stats degenerate_neighborhood_stats;
    bool degenerate_neighborhood_repair = false;
    bool rigorous_boundary_repair = false;
    if (!report->poisson_reconstruction_applied &&
	    (settings->use_fast_face_fallback ||
	    settings->use_full_fast_fallback)) {
	std::vector<int> input_source_faces = input_face_brep_sources;
	if (input_source_faces.size() != (size_t)input_face_count)
	    input_source_faces.assign((size_t)input_face_count, -1);
	for (int face = rigorous_input_face_count; face < input_face_count;
		++face) {
	    if (input_source_faces[(size_t)face] >= 0)
		continue;
	    const repair_triangle_key key = repair_triangle_coordinates(
		input_vertices, &input_faces[(size_t)face * 3]);
	    const auto local_source = local_chart_triangle_brep_faces.find(key);
	    if (local_source != local_chart_triangle_brep_faces.end() &&
		    local_source->second.size() == 1)
		input_source_faces[(size_t)face] =
		    *local_source->second.begin();
	    const auto fast_source = fast_triangle_brep_faces.find(key);
	    if (input_source_faces[(size_t)face] < 0 && fast_source !=
		    fast_triangle_brep_faces.end() &&
		    fast_source->second.size() == 1)
		input_source_faces[(size_t)face] =
		    *fast_source->second.begin();
	}
	const double neighborhood_deviation =
	    settings->max_surface_deviation > 0.0 ?
	    settings->max_surface_deviation : s_cdt->absmax;
	const size_t neighborhood_max_edges =
	    settings->mesh.max_hole_edges > 0 ?
	    (size_t)settings->mesh.max_hole_edges : (size_t)256;
	if (!settings->use_full_fast_fallback) {
	    rigorous_boundary_repair =
		repair_failed_face_from_rigorous_boundary(s_cdt, &input_faces,
		    &input_face_count, &input_vertices, &input_vertex_count,
		    rigorous_input_face_count, input_source_faces,
		    neighborhood_deviation, std::max(neighborhood_max_edges,
			(size_t)1024),
		    &degenerate_neighborhood_stats);
	}
	/* Failed faces expose independent rigorous rings.  Reconstruct each
	 * source transactionally when every one of its exact shared-edge segments
	 * is present in the rigorous prefix.  Intermediate candidates may remain
	 * open only at the other proved source rings; the final source must close
	 * and validate the complete mesh.  For one failed source, this is also the
	 * authoritative requested-boundary path. */
	if (!settings->use_full_fast_fallback &&
		!rigorous_boundary_repair) {
	    std::set<int> approximate_sources;
	    bool attributed = true;
	    for (int face = rigorous_input_face_count;
		    face < input_face_count; ++face) {
		const int source = input_source_faces[(size_t)face];
		if (source < 0) {
		    if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY"))
			bu_log("Source-wise rigorous-boundary repair lacks "
			    "provenance for approximate triangle %d of %d\n",
			    face - rigorous_input_face_count,
			    input_face_count - rigorous_input_face_count);
		    attributed = false;
		    break;
		}
		approximate_sources.insert(source);
	    }
	    typedef std::pair<int, int> repair_mesh_edge;
	    std::map<int, std::set<repair_mesh_edge>> source_boundaries;
	    const bool debug_repair_topology =
		getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY") != NULL;
	    std::map<repair_point_key, int> exact_input_points;
	    for (int point = 0; point < input_vertex_count; ++point) {
		const repair_point_key key = {
		    input_vertices[(size_t)point * 3],
		    input_vertices[(size_t)point * 3 + 1],
		    input_vertices[(size_t)point * 3 + 2]};
		const auto existing = exact_input_points.find(key);
		if (existing == exact_input_points.end())
		    exact_input_points.emplace(key, point);
		else if (existing->second != point)
		    existing->second = -1;
	    }
	    const auto source_index = [&](ON_3dPoint *point) {
		if (!point)
		    return -1;
		auto indexed = approximate_source_points.find(point);
		if (indexed != approximate_source_points.end())
		    return indexed->second;
		const auto canonical = s_cdt->collapsed_edge_pnts.find(point);
		if (canonical != s_cdt->collapsed_edge_pnts.end()) {
		    indexed = approximate_source_points.find(canonical->second);
		    if (indexed != approximate_source_points.end())
			return indexed->second;
		}
		const repair_point_key key = {point->x, point->y, point->z};
		const auto exact = exact_input_points.find(key);
		return exact == exact_input_points.end() ? -1 : exact->second;
	    };
	    for (int source : approximate_sources) {
		if (!attributed || source < 0 ||
			source >= s_cdt->brep->m_F.Count()) {
		    attributed = false;
		    break;
		}
		const ON_BrepFace &brep_face = s_cdt->brep->m_F[source];
		std::set<repair_mesh_edge> &boundary =
		    source_boundaries[source];
		if (debug_repair_topology)
		    bu_log("Face %d: proving %d source loops\n", source,
			brep_face.LoopCount());
		for (int loop_index = 0; attributed &&
			loop_index < brep_face.LoopCount(); ++loop_index) {
		    const ON_BrepLoop *loop = brep_face.Loop(loop_index);
		    if (!loop) {
			attributed = false;
			break;
		    }
		    if (debug_repair_topology)
			bu_log("Face %d: source loop %d type %d has %d trims\n",
			    source, loop_index, (int)loop->m_type,
			    loop->TrimCount());
		    for (int trim_index = 0; attributed &&
			    trim_index < loop->TrimCount(); ++trim_index) {
			const ON_BrepTrim *trim = loop->Trim(trim_index);
			if (!trim) {
			    attributed = false;
			    break;
			}
			if (trim->m_type == ON_BrepTrim::singular)
			    continue;
			/* A seam used only by this face is a chart cut, not a shell
			 * boundary supplied by a retained neighboring face. */
			if (repair_internal_face_seam(trim, brep_face))
			    continue;
			const auto edge_segments =
			    s_cdt->e2polysegs.find(trim->m_ei);
			if (edge_segments == s_cdt->e2polysegs.end() ||
				edge_segments->second.empty()) {
			    if (debug_repair_topology)
				bu_log("Face %d: outer/hole trim %d (edge %d, "
				    "type %d, loop %d) lacks a complete "
				    "authoritative boundary\n", source,
				    trim->m_trim_index, trim->m_ei,
				    (int)trim->m_type, loop_index);
			    attributed = false;
			    break;
			}
			std::map<double, ON_3dPoint *> edge_points;
			std::vector<std::pair<double, double>> edge_intervals;
			ON_3dPoint *root_start = NULL;
			ON_3dPoint *root_end = NULL;
			for (const bedge_seg_t *segment :
				edge_segments->second) {
			    if (!segment || !segment->e_start ||
				    !segment->e_end ||
				    !segment->e_root_start ||
				    !segment->e_root_end ||
				    !std::isfinite(segment->edge_start) ||
				    !std::isfinite(segment->edge_end)) {
				attributed = false;
				break;
			    }
			    if (!root_start) {
				root_start = segment->e_root_start;
				root_end = segment->e_root_end;
			    } else if (root_start != segment->e_root_start ||
				    root_end != segment->e_root_end) {
				attributed = false;
				break;
			    }
			    edge_intervals.push_back(std::minmax(
				segment->edge_start, segment->edge_end));
			    edge_points[segment->edge_start] = segment->e_start;
			    edge_points[segment->edge_end] = segment->e_end;
			}
			if (attributed) {
			    std::sort(edge_intervals.begin(), edge_intervals.end());
			    const double scale = edge_intervals.empty() ? 1.0 :
				std::max(1.0, std::max(std::fabs(
				edge_intervals.front().first), std::fabs(
				edge_intervals.back().second)));
			    const double parameter_tolerance = 4096.0 *
				std::numeric_limits<double>::epsilon() * scale;
			    double covered = edge_intervals.empty() ? DBL_MAX :
				edge_intervals.front().second;
			    bool complete = !edge_intervals.empty();
			    for (size_t interval = 1; complete &&
				    interval < edge_intervals.size(); ++interval) {
				complete = edge_intervals[interval].first <= covered +
				    parameter_tolerance;
				covered = std::max(covered,
				    edge_intervals[interval].second);
			    }
			    const bool endpoints_match = !edge_points.empty() &&
				((edge_points.begin()->second == root_start &&
				edge_points.rbegin()->second == root_end) ||
				(edge_points.begin()->second == root_end &&
				edge_points.rbegin()->second == root_start));
			    complete = complete && endpoints_match;
			    if (!complete) {
				if (debug_repair_topology)
				    bu_log("Face %d: trim %d edge segments do not "
					"form one complete authoritative chain\n",
					source, trim->m_trim_index);
				attributed = false;
			    }
			}
			int prior_index = -1;
			size_t indexed_points = 0;
			for (const auto &edge_point : edge_points) {
			    const int index = source_index(edge_point.second);
			    if (index < 0) {
				if (debug_repair_topology)
				    bu_log("Face %d: trim %d has an unindexed "
					"authoritative edge point\n", source,
					trim->m_trim_index);
				attributed = false;
				break;
			    }
			    indexed_points++;
			    if (prior_index >= 0 && prior_index != index)
				boundary.insert(std::minmax(prior_index, index));
			    prior_index = index;
			}
			if (attributed && indexed_points < 2) {
			    if (debug_repair_topology)
				bu_log("Face %d: trim %d has fewer than two "
				    "indexed authoritative edge points\n", source,
				    trim->m_trim_index);
			    attributed = false;
			}
		    }
		}
		if (boundary.empty()) {
		    if (debug_repair_topology)
			bu_log("Face %d: authoritative boundary is empty\n",
			    source);
		    attributed = false;
		}
	    }
	    if (debug_repair_topology && !attributed)
		bu_log("Source-wise rigorous-boundary repair could not prove "
		    "all approximate source boundaries\n");
	    if (attributed && !approximate_sources.empty()) {
		int trial_face_count = input_face_count;
		int trial_vertex_count = input_vertex_count;
		int *trial_faces = (int *)bu_malloc(
		    (size_t)trial_face_count * 3 * sizeof(int),
		    "multi-source rigorous-boundary trial faces");
		fastf_t *trial_vertices = (fastf_t *)bu_malloc(
		    (size_t)trial_vertex_count * 3 * sizeof(fastf_t),
		    "multi-source rigorous-boundary trial vertices");
		memcpy(trial_faces, input_faces,
		    (size_t)trial_face_count * 3 * sizeof(int));
		memcpy(trial_vertices, input_vertices,
		    (size_t)trial_vertex_count * 3 * sizeof(fastf_t));
		std::vector<int> trial_sources = input_source_faces;
		repair_degenerate_neighborhood_stats combined_stats;
		bool completed = true;
		std::set<int> pending_sources = approximate_sources;
		while (!pending_sources.empty()) {
		    bool advanced = false;
		    for (int source : pending_sources) {
			repair_degenerate_neighborhood_stats source_stats;
			std::vector<int> remap;
			const bool final_source = pending_sources.size() == 1;
			if (!repair_failed_face_from_rigorous_boundary(s_cdt,
				&trial_faces, &trial_face_count, &trial_vertices,
				&trial_vertex_count, rigorous_input_face_count,
				trial_sources, neighborhood_deviation,
				std::max(neighborhood_max_edges, (size_t)4096),
				&source_stats, source,
				&source_boundaries[source], final_source, &remap)) {
			    if (debug_repair_topology)
				bu_log("Face %d: independent rigorous-boundary "
				    "transaction is not currently reconstructable "
				    "(%zu indexed boundary edges)\n", source,
				    source_boundaries[source].size());
			    continue;
			}
			combined_stats.components += source_stats.components;
			combined_stats.removed_faces += source_stats.removed_faces;
			combined_stats.added_faces += source_stats.added_faces;
			combined_stats.boundary_edges += source_stats.boundary_edges;
			combined_stats.max_center_offset = std::max(
			    combined_stats.max_center_offset,
			    source_stats.max_center_offset);
			combined_stats.inferred_topology =
			    combined_stats.inferred_topology ||
			    source_stats.inferred_topology;
			combined_stats.source_faces.insert(source);
			for (auto &source_boundary : source_boundaries) {
			    std::set<repair_mesh_edge> remapped;
			    for (const repair_mesh_edge &edge :
				    source_boundary.second) {
				if ((size_t)edge.first >= remap.size() ||
					(size_t)edge.second >= remap.size()) {
				    completed = false;
				    break;
				}
				if (remap[(size_t)edge.first] < 0 ||
					remap[(size_t)edge.second] < 0) {
				    completed = false;
				    break;
				}
				remapped.insert(std::minmax(
				    remap[(size_t)edge.first],
				    remap[(size_t)edge.second]));
			    }
			    if (!completed)
				break;
			    source_boundary.second.swap(remapped);
			}
			if (!completed)
			    break;
			pending_sources.erase(source);
			advanced = true;
			break;
		    }
		    if (!completed)
			break;
		    if (!advanced) {
			if (debug_repair_topology)
			    bu_log("Source-wise rigorous-boundary repair found no "
				"viable dependency order for %zu faces\n",
				pending_sources.size());
			completed = false;
			break;
		    }
		}
		if (completed) {
		    bu_free(input_faces,
			"pre-multi-source rigorous-boundary faces");
		    bu_free(input_vertices,
			"pre-multi-source rigorous-boundary vertices");
		    input_faces = trial_faces;
		    input_face_count = trial_face_count;
		    input_vertices = trial_vertices;
		    input_vertex_count = trial_vertex_count;
		    input_source_faces.swap(trial_sources);
		    degenerate_neighborhood_stats = combined_stats;
		    rigorous_boundary_repair = true;
		    bu_log("Reconstructed %zu failed B-Rep face%s from "
			"independent rigorous boundary rings\n",
			approximate_sources.size(),
			approximate_sources.size() == 1 ? "" : "s");
		} else {
		    bu_free(trial_faces,
			"failed multi-source rigorous-boundary faces");
		    bu_free(trial_vertices,
			"failed multi-source rigorous-boundary vertices");
		}
	    }
	}
	degenerate_neighborhood_repair = rigorous_boundary_repair;
	if (!degenerate_neighborhood_repair) {
	    size_t paired_boundary_vertices = 0;
	    if (!settings->use_full_fast_fallback &&
		    repair_pair_near_boundary_cracks(input_faces, input_face_count,
		    input_vertices, input_vertex_count,
		    rigorous_input_face_count, neighborhood_deviation,
		    &paired_boundary_vertices))
		bu_log("Paired %zu near-coincident failed-face boundary "
		    "vertices before local neighborhood repair\n",
		    paired_boundary_vertices);
	    degenerate_neighborhood_repair =
		repair_degenerate_approximate_neighborhoods(&input_faces,
		    &input_face_count, &input_vertices, &input_vertex_count,
		    rigorous_input_face_count, input_source_faces,
		    neighborhood_deviation, std::max(neighborhood_max_edges,
			(size_t)1024),
		    &degenerate_neighborhood_stats);
	}
	if (degenerate_neighborhood_repair) {
	    locally_reconstructed_faces.insert(
		degenerate_neighborhood_stats.source_faces.begin(),
		degenerate_neighborhood_stats.source_faces.end());
	    for (int face = 0; face < input_face_count; ++face) {
		const int source = input_source_faces[(size_t)face];
		if (source < 0)
		    continue;
		const repair_triangle_key key = repair_triangle_coordinates(
		    input_vertices, &input_faces[(size_t)face * 3]);
		local_chart_triangle_brep_faces[key].insert(source);
		locally_reconstructed_faces.insert(source);
	    }
	    if (rigorous_boundary_repair) {
		bu_log("Replaced %d failed-face triangles with %d triangles "
		    "spanning a %d-edge boundary supplied entirely by retained "
		    "rigorous faces\n",
		    degenerate_neighborhood_stats.removed_faces,
		    degenerate_neighborhood_stats.added_faces,
		    degenerate_neighborhood_stats.boundary_edges);
	    } else {
		bu_log("Replaced %d degenerate triangles in %d local "
		    "neighborhoods with %d fixed-boundary triangles "
		    "(maximum interior offset %.17g)\n",
		    degenerate_neighborhood_stats.removed_faces,
		    degenerate_neighborhood_stats.components,
		    degenerate_neighborhood_stats.added_faces,
		    degenerate_neighborhood_stats.max_center_offset);
	    }
	}
    }

    /* Removing flat triangles from an otherwise edge-closed hybrid can turn
     * point contacts between many independent vertex fans into a large set of
     * artificial holes.  Give the topology-preserving, transactional local
     * reconstruction above the first opportunity to fix those defects.  If
     * it cannot, and the caller explicitly authorized a whole-display retry,
     * avoid an unbounded sequence of generic hole triangulations when the
     * invalid links are disproportionate both to the flat triangles and to
     * the requested local hole budget. */
    if (settings->use_full_fast_fallback_if_needed &&
	    !settings->use_full_fast_fallback &&
	    !degenerate_neighborhood_repair) {
	assembled_mesh_validation hybrid_validation;
	(void)assembled_mesh_validate(input_vertex_count, input_face_count,
	    input_vertices, input_faces, &hybrid_validation, false);
	struct bg_trimesh_solid_errors hybrid_errors =
	    BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	(void)bg_trimesh_solid2(input_vertex_count, input_face_count,
	    input_vertices, input_faces, &hybrid_errors);
	const bool edge_closed = !hybrid_errors.unmatched.count &&
	    !hybrid_errors.excess.count &&
	    !hybrid_errors.misoriented.count;
	bg_free_trimesh_solid_errors(&hybrid_errors);
	const bool hybrid_storage_valid = !hybrid_validation.invalid_indices &&
	    !hybrid_validation.nonfinite_vertices;
	if (hybrid_storage_valid && edge_closed &&
		repair_hybrid_fallback_preflight(
		    hybrid_validation.degenerate_faces,
		    hybrid_validation.invalid_vertex_links,
		    settings->mesh.max_hole_edges)) {
	    const size_t degenerate_faces =
		hybrid_validation.degenerate_faces;
	    const size_t invalid_links =
		hybrid_validation.invalid_vertex_links;
	    bu_free(input_faces, "disproportionate hybrid repair faces");
	    bu_free(input_vertices,
		"disproportionate hybrid repair vertices");
	    std::string message = "rigorous hybrid preflight declined " +
		std::to_string(input_face_count) + " triangles: " +
		std::to_string(degenerate_faces) + " flat triangles are " +
		"associated with " + std::to_string(invalid_links) +
		" invalid vertex links after bounded local reconstruction";
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
		BREP_CDT_STAGE_MESH_REPAIR, -1,
		report->source_diagnostic.completed_faces,
		report->source_failed_faces, message.c_str());
	    return -1;
	}
    }

    struct bg_trimesh_repair_settings mesh_settings = settings->mesh;
    mesh_settings.require_solid = 1;
    /* Authoritative fast-face constraints reuse the rigorous boundary
     * coordinates exactly.  When the caller did not request a weld radius,
     * allow only coordinate roundoff: the tessellation spacing is an interior
     * sampling target, not permission to merge unrelated vertices throughout
     * every retained rigorous face. */
    if (report->fast_fallback_used_faces &&
	    !(mesh_settings.vertex_tolerance > 0.0)) {
	const ON_BoundingBox bounds = s_cdt->orig_brep->BoundingBox();
	double coordinate_scale = 1.0;
	if (bounds.IsValid()) {
	    coordinate_scale = std::max(coordinate_scale,
		bounds.Diagonal().Length());
	    for (int axis = 0; axis < 3; ++axis) {
		coordinate_scale = std::max(coordinate_scale,
		    std::fabs(bounds.m_min[axis]));
		coordinate_scale = std::max(coordinate_scale,
		    std::fabs(bounds.m_max[axis]));
	    }
	}
	mesh_settings.vertex_tolerance = 256.0 *
	    std::numeric_limits<double>::epsilon() * coordinate_scale;
    }
    int *repaired_faces = NULL;
    int repaired_face_count = 0;
    point_t *repaired_points = NULL;
    int repaired_vertex_count = 0;
    /* A reconstructed mesh may already satisfy every topology and geometry
     * condition.  Preserve it exactly: tolerance welding intended for an
     * open mixed-source mesh can remove valid, very small Poisson facets and
     * create a new hole.  The independent fidelity gates still run below. */
    bool preserve_poisson = false;
    if (report->poisson_reconstruction_applied) {
	assembled_mesh_validation poisson_validation;
	const bool poisson_geometric = assembled_mesh_validate(
	    input_vertex_count, input_face_count, input_vertices,
	    input_faces, &poisson_validation,
	    !settings->mesh.allow_self_intersections);
	preserve_poisson = poisson_geometric && !bg_trimesh_solid2(
	    input_vertex_count, input_face_count, input_vertices,
	    input_faces, NULL);
    }
    int repair_result = 1;
    size_t adaptive_hole_edges = 0;
    if (preserve_poisson) {
	report->mesh.input_vertices = input_vertex_count;
	report->mesh.input_faces = input_face_count;
	report->mesh.input_area = repair_mesh_area(input_vertices,
	    input_faces, input_face_count);
	report->mesh.solid = 1;
    } else {
	repair_result = bg_trimesh_repair2(&repaired_faces,
	    &repaired_face_count, &repaired_points, &repaired_vertex_count,
	    input_faces, input_face_count, (const point_t *)input_vertices,
	    input_vertex_count, &mesh_settings, &report->mesh);
	adaptive_hole_edges = repair_result < 0 ?
	    repair_adaptive_hole_edge_budget(settings, &report->mesh) : 0;
	if (adaptive_hole_edges) {
	    report->adaptive_hole_retry_attempted = 1;
	    report->adaptive_hole_edges = adaptive_hole_edges;
	    mesh_settings.max_hole_edges = adaptive_hole_edges;
	    bu_log("Retrying final mesh repair with a bounded %zu-edge hole "
		"ceiling\n", adaptive_hole_edges);
	    repair_result = bg_trimesh_repair2(&repaired_faces,
		&repaired_face_count, &repaired_points,
		&repaired_vertex_count, input_faces, input_face_count,
		(const point_t *)input_vertices, input_vertex_count,
		&mesh_settings, &report->mesh);
	}
	/* Point-contact separation is useful for Poisson meshes with touching
	 * shells, but can reopen a seam that tolerance welding just closed.
	 * Preserve positions first and perturb contacts only when the caller did
	 * not already request separation and conservative repair cannot produce
	 * a solid. */
	if (repair_result < 0 && settings->use_poisson_reconstruction &&
		!mesh_settings.separate_touching_vertices) {
	    mesh_settings.separate_touching_vertices = 1;
	    repair_result = bg_trimesh_repair2(&repaired_faces,
		&repaired_face_count, &repaired_points,
		&repaired_vertex_count, input_faces, input_face_count,
		(const point_t *)input_vertices, input_vertex_count,
		&mesh_settings, &report->mesh);
	}
    }
    if (degenerate_neighborhood_repair) {
	report->mesh.removed_faces +=
	    degenerate_neighborhood_stats.removed_faces;
	report->mesh.added_faces += degenerate_neighborhood_stats.added_faces;
    }
    if (repair_result == 1) {
	repaired_face_count = input_face_count;
	repaired_vertex_count = input_vertex_count;
	repaired_faces = (int *)bu_malloc((size_t)repaired_face_count * 3 *
	    sizeof(int), "certified repaired faces");
	repaired_points = (point_t *)bu_malloc(
	    (size_t)repaired_vertex_count * sizeof(point_t),
	    "certified repaired vertices");
	memcpy(repaired_faces, input_faces,
	    (size_t)repaired_face_count * 3 * sizeof(int));
	memcpy(repaired_points, input_vertices,
	    (size_t)repaired_vertex_count * sizeof(point_t));
    } else if (repair_result < 0) {
	bu_free(input_faces, "repair input faces");
	bu_free(input_vertices, "repair input vertices");
	std::string message =
	    "bounded triangle mesh repair did not produce a solid: " +
	    std::to_string(report->mesh.unmatched_edges) +
	    " unmatched, " + std::to_string(report->mesh.excess_edges) +
	    " excess, " + std::to_string(report->mesh.misoriented_edges) +
	    " misoriented edges, " +
	    std::to_string(report->mesh.invalid_vertex_links) +
	    " invalid vertex links, " +
	    std::to_string(report->mesh.rejected_hole_faces) +
	    " intersecting cap faces rejected";
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }
    report->allowed_surface_deviation =
	settings->max_surface_deviation > 0.0 ?
	settings->max_surface_deviation : s_cdt->absmax;
    if (!(report->allowed_surface_deviation > 0.0) ||
	    !std::isfinite(report->allowed_surface_deviation))
	report->allowed_surface_deviation = BN_TOL_DIST;
    const bool relaxed_fidelity_enabled =
	settings->relaxed_fidelity_factor >= 1.0;
    const double relaxed_fidelity_factor = relaxed_fidelity_enabled ?
	settings->relaxed_fidelity_factor : 1.0;
    const double validation_surface_deviation =
	report->allowed_surface_deviation * relaxed_fidelity_factor;
    const double validation_area_change_limit =
	settings->max_area_change_percent * relaxed_fidelity_factor;
    if (!(validation_surface_deviation > 0.0) ||
	    !std::isfinite(validation_surface_deviation) ||
	    (settings->max_area_change_percent > 0.0 &&
	    !std::isfinite(validation_area_change_limit))) {
	bu_free(repaired_faces, "invalid-fidelity repaired faces");
	bu_free(repaired_points, "invalid-fidelity repaired vertices");
	bu_free(input_faces, "invalid-fidelity input faces");
	bu_free(input_vertices, "invalid-fidelity input vertices");
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces,
	    "relaxed repair fidelity limit is not finite");
	return -1;
    }
    report->relaxed_fidelity_factor = relaxed_fidelity_enabled ?
	settings->relaxed_fidelity_factor : 0.0;
    report->relaxed_surface_deviation_limit = relaxed_fidelity_enabled ?
	validation_surface_deviation : 0.0;
    report->relaxed_area_change_percent_limit =
	relaxed_fidelity_enabled && settings->max_area_change_percent > 0.0 ?
	validation_area_change_limit : 0.0;
    bool relaxed_fidelity_applied = false;

    std::vector<ON_BoundingBox> face_bounds(
	(size_t)s_cdt->orig_brep->m_F.Count());
    std::vector<std::unique_ptr<brlcad::SurfaceTree>> face_trees(
	(size_t)s_cdt->orig_brep->m_F.Count());
    std::vector<std::unique_ptr<brlcad::PullbackContext>> face_contexts(
	(size_t)s_cdt->orig_brep->m_F.Count());
    for (int face = 0; face < s_cdt->orig_brep->m_F.Count(); ++face)
	face_bounds[(size_t)face] =
	    s_cdt->orig_brep->m_F[face].BoundingBox();

    std::set<repair_triangle_key> rigorous_triangles;
    std::map<repair_triangle_key, size_t> rigorous_triangle_counts;
    for (int face = 0; face < rigorous_input_face_count; ++face) {
	const repair_triangle_key key = repair_triangle_coordinates(
	    input_vertices, &input_faces[(size_t)face * 3]);
	rigorous_triangles.insert(key);
	rigorous_triangle_counts[key]++;
    }
    report->mesh.output_vertices = repaired_vertex_count;
    report->mesh.output_faces = repaired_face_count;
    report->mesh.output_area = repair_mesh_area(
	(const fastf_t *)repaired_points, repaired_faces,
	repaired_face_count);

    assembled_mesh_validation mesh_validation;
    fastf_t *repaired_vertices = (fastf_t *)repaired_points;
    struct bg_trimesh_solid_errors solid_errors =
	BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    bool geometric_valid = assembled_mesh_validate(
	repaired_vertex_count, repaired_face_count, repaired_vertices,
	repaired_faces, &mesh_validation,
	!settings->mesh.allow_self_intersections);
    int not_solid = bg_trimesh_solid2(repaired_vertex_count,
	repaired_face_count, (fastf_t *)repaired_vertices, repaired_faces,
	&solid_errors);
    bg_free_trimesh_solid_errors(&solid_errors);
    /* Once conservative repair has made an indexed solid, duplicate closed
     * fans can still touch at one geometric point.  Separate those contacts
     * only at this stage: doing it before the final weld can reopen seams.
     * Accept the perturbed candidate only after complete validation. */
    if (!geometric_valid && !not_solid &&
	    mesh_validation.intersecting_triangle_pairs &&
	    !mesh_validation.invalid_indices &&
	    !mesh_validation.nonfinite_vertices &&
	    !mesh_validation.unused_vertices &&
	    !mesh_validation.degenerate_faces &&
	    !mesh_validation.invalid_vertex_links &&
	    settings->use_poisson_reconstruction &&
	    !settings->mesh.separate_touching_vertices) {
	struct bg_trimesh_repair_settings separation_settings = mesh_settings;
	separation_settings.fill_holes = 0;
	separation_settings.max_iterations = 1;
	separation_settings.separate_touching_vertices = 1;
	separation_settings.union_components = 0;
	int *separated_faces = NULL;
	int separated_face_count = 0;
	point_t *separated_points = NULL;
	int separated_vertex_count = 0;
	struct bg_trimesh_repair_report separated_report =
	    BG_TRIMESH_REPAIR_REPORT_INIT;
	const int separated_result = bg_trimesh_repair2(&separated_faces,
	    &separated_face_count, &separated_points,
	    &separated_vertex_count, repaired_faces, repaired_face_count,
	    (const point_t *)repaired_vertices, repaired_vertex_count,
	    &separation_settings, &separated_report);
	assembled_mesh_validation separated_validation;
	const bool separated_geometric = separated_result == 0 &&
	    assembled_mesh_validate(separated_vertex_count,
		separated_face_count, (const fastf_t *)separated_points,
		separated_faces, &separated_validation,
		!settings->mesh.allow_self_intersections);
	const bool separated_solid = separated_result == 0 &&
	    !bg_trimesh_solid2(separated_vertex_count,
		separated_face_count, (fastf_t *)separated_points,
		separated_faces, NULL);
	if (separated_geometric && separated_solid) {
	    const struct bg_trimesh_repair_report initial_report =
		report->mesh;
	    separated_report.input_vertices = initial_report.input_vertices;
	    separated_report.input_faces = initial_report.input_faces;
	    separated_report.input_area = initial_report.input_area;
	    separated_report.removed_faces += initial_report.removed_faces;
	    separated_report.added_faces += initial_report.added_faces;
	    separated_report.rejected_hole_faces +=
		initial_report.rejected_hole_faces;
	    separated_report.component_union_applied =
		separated_report.component_union_applied ||
		initial_report.component_union_applied;
	    report->mesh = separated_report;
	    bu_free(repaired_faces, "pre-separation repaired faces");
	    bu_free(repaired_points, "pre-separation repaired vertices");
	    repaired_faces = separated_faces;
	    repaired_face_count = separated_face_count;
	    repaired_points = separated_points;
	    repaired_vertex_count = separated_vertex_count;
	    repaired_vertices = (fastf_t *)repaired_points;
	    mesh_validation = separated_validation;
	    geometric_valid = true;
	    not_solid = 0;
	} else {
	    bu_free(separated_faces, "rejected separated faces");
	    bu_free(separated_points, "rejected separated vertices");
	}
    }
    if (!geometric_valid || not_solid) {
	std::string message =
	    "repaired mesh failed final solid/geometric validation: "
	    "degenerate faces " +
	    std::to_string(mesh_validation.degenerate_faces) +
	    ", invalid vertex links " +
	    std::to_string(mesh_validation.invalid_vertex_links) +
	    ", intersections " +
	    std::to_string(mesh_validation.intersecting_triangle_pairs);
	if (mesh_validation.intersecting_triangle_pairs &&
		mesh_validation.first_intersection[0] >= 0 &&
		mesh_validation.first_intersection[1] >= 0) {
	    std::set<repair_triangle_key> fast_triangles;
	    for (int face = rigorous_input_face_count;
		    face < input_face_count; ++face) {
		const repair_triangle_key key = repair_triangle_coordinates(
		    input_vertices, &input_faces[(size_t)face * 3]);
		fast_triangles.insert(key);
	    }
	    const auto provenance = [&](int face) {
		const repair_triangle_key key = repair_triangle_coordinates(
		    repaired_vertices, &repaired_faces[(size_t)face * 3]);
		if (rigorous_triangles.find(key) != rigorous_triangles.end()) {
		    for (int input_face = 0;
			    input_face < rigorous_input_face_count; ++input_face) {
			if (key != repair_triangle_coordinates(input_vertices,
				&input_faces[(size_t)input_face * 3]))
			    continue;
			if ((size_t)input_face <
				rigorous_input_brep_faces.size())
			    return std::string("rigorous B-Rep face ") +
				std::to_string(rigorous_input_brep_faces[
				(size_t)input_face]);
			break;
		    }
		    return std::string("rigorous");
		}
		if (fast_triangles.find(key) != fast_triangles.end())
		    return std::string("fast");
		return std::string("repair");
	    };
	    message += " (first " + std::to_string(
		mesh_validation.first_intersection[0]) + " [" +
		provenance(mesh_validation.first_intersection[0]) + "], " +
		std::to_string(mesh_validation.first_intersection[1]) + " [" +
		provenance(mesh_validation.first_intersection[1]) + "])";
	    if (mesh_validation.first_intersection_point_valid) {
		message += " near (" + std::to_string(
		    mesh_validation.first_intersection_point[0]) + ", " +
		    std::to_string(mesh_validation.first_intersection_point[1]) +
		    ", " + std::to_string(
		    mesh_validation.first_intersection_point[2]) + ")";
	    }
	}
	bu_free(repaired_faces, "certified repaired faces");
	bu_free(repaired_points, "certified repaired vertices");
	bu_free(input_faces, "repair input faces");
	bu_free(input_vertices, "repair input vertices");
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }

    report->mesh.output_volume = bg_trimesh_volume(repaired_faces,
	(size_t)repaired_face_count, repaired_points,
	(size_t)repaired_vertex_count);
    repair_added_patch_stats(report, input_vertices, input_faces,
	input_face_count, repaired_vertices, repaired_faces,
	repaired_face_count);

    /* A local fallback is allowed to add geometry inside failed-face
     * neighborhoods, but it may not silently simplify unrelated certified
     * faces.  Compare triangle coordinates rather than indices so harmless
     * reindexing, reorientation, and vertex duplication remain acceptable.
     * A bounded edge split is also preservation: mesh repair uses it to mate
     * a rigorous triangle to an already certified hanging boundary sample. */
    std::map<repair_triangle_key, size_t> output_triangle_counts;
    for (int face = 0; face < repaired_face_count; ++face) {
	const repair_triangle_key key = repair_triangle_coordinates(
	    repaired_vertices, &repaired_faces[(size_t)face * 3]);
	output_triangle_counts[key]++;
    }
    std::vector<repair_triangle_key> missing_rigorous_keys;
    std::set<repair_triangle_edge_key> missing_rigorous_edges;
    for (const auto &required : rigorous_triangle_counts) {
	const auto found = output_triangle_counts.find(required.first);
	const size_t available = found == output_triangle_counts.end() ? 0 :
	    found->second;
	for (size_t occurrence = available; occurrence < required.second;
		++occurrence)
	    missing_rigorous_keys.push_back(required.first);
	if (available >= required.second)
	    continue;
	for (int corner = 0; corner < 3; ++corner)
	    missing_rigorous_edges.insert(repair_triangle_edge(
		required.first[(size_t)corner],
		required.first[(size_t)((corner + 1) % 3)]));
    }
    std::map<repair_triangle_edge_key, std::vector<int>>
	output_faces_by_missing_edge;
    std::vector<repair_triangle_key> output_triangle_keys;
    if (!missing_rigorous_edges.empty()) {
	output_triangle_keys.resize((size_t)repaired_face_count);
	for (int face = 0; face < repaired_face_count; ++face) {
	    output_triangle_keys[(size_t)face] =
		repair_triangle_coordinates(repaired_vertices,
		&repaired_faces[(size_t)face * 3]);
	    const repair_triangle_key &triangle = output_triangle_keys[
		(size_t)face];
	    for (int corner = 0; corner < 3; ++corner) {
		const repair_triangle_edge_key edge = repair_triangle_edge(
		    triangle[(size_t)corner],
		    triangle[(size_t)((corner + 1) % 3)]);
		if (missing_rigorous_edges.find(edge) !=
			missing_rigorous_edges.end())
		    output_faces_by_missing_edge[edge].push_back(face);
	    }
	}
    }
    std::set<int> subdivision_faces_used;
    std::set<int> reconciled_rigorous_faces;
    std::vector<repair_triangle_key> unresolved_rigorous_keys;
    std::map<repair_triangle_key, std::set<int>> missing_triangle_sources;
    if (!missing_rigorous_keys.empty()) {
	const std::set<repair_triangle_key> missing_unique(
	    missing_rigorous_keys.begin(), missing_rigorous_keys.end());
	for (int face = 0; face < rigorous_input_face_count; ++face) {
	    const repair_triangle_key key = repair_triangle_coordinates(
		input_vertices, &input_faces[(size_t)face * 3]);
	    if (missing_unique.find(key) == missing_unique.end() ||
		    (size_t)face >= rigorous_input_brep_faces.size())
		continue;
	    missing_triangle_sources[key].insert(
		rigorous_input_brep_faces[(size_t)face]);
	}
    }
    const auto collect_source_faces = [&](const repair_triangle_key &key,
	    std::set<int> &faces) {
	const auto source = missing_triangle_sources.find(key);
	if (source == missing_triangle_sources.end())
	    return;
	faces.insert(source->second.begin(), source->second.end());
    };
    for (const repair_triangle_key &required : missing_rigorous_keys) {
	std::array<repair_triangle_edge_key, 3> edges;
	for (int corner = 0; corner < 3; ++corner)
	    edges[(size_t)corner] = repair_triangle_edge(
		required[(size_t)corner],
		required[(size_t)((corner + 1) % 3)]);
	bool subdivided = false;
	for (int first_edge = 0; first_edge < 3 && !subdivided;
		++first_edge) {
	    const auto first_candidates = output_faces_by_missing_edge.find(
		edges[(size_t)first_edge]);
	    if (first_candidates == output_faces_by_missing_edge.end())
		continue;
	    for (int second_edge = first_edge + 1;
		    second_edge < 3 && !subdivided; ++second_edge) {
		const auto second_candidates =
		    output_faces_by_missing_edge.find(
		    edges[(size_t)second_edge]);
		if (second_candidates == output_faces_by_missing_edge.end())
		    continue;
		for (int first_face : first_candidates->second) {
		    if (subdivision_faces_used.find(first_face) !=
			    subdivision_faces_used.end() ||
			    rigorous_triangles.find(output_triangle_keys[
			    (size_t)first_face]) != rigorous_triangles.end())
			continue;
		    for (int second_face : second_candidates->second) {
			if (first_face == second_face ||
				subdivision_faces_used.find(second_face) !=
				subdivision_faces_used.end() ||
				rigorous_triangles.find(output_triangle_keys[
				(size_t)second_face]) !=
				rigorous_triangles.end())
			    continue;
			if (!repair_triangle_edge_split(required,
				output_triangle_keys[(size_t)first_face],
				output_triangle_keys[(size_t)second_face]))
			    continue;
			subdivision_faces_used.insert(first_face);
			subdivision_faces_used.insert(second_face);
			report->subdivided_rigorous_triangles++;
			std::set<int> source_faces;
			collect_source_faces(required, source_faces);
			reconciled_rigorous_faces.insert(source_faces.begin(),
			    source_faces.end());
			local_chart_triangle_brep_faces[
			    output_triangle_keys[(size_t)first_face]].insert(
			    source_faces.begin(), source_faces.end());
			local_chart_triangle_brep_faces[
			    output_triangle_keys[(size_t)second_face]].insert(
			    source_faces.begin(), source_faces.end());
			subdivided = true;
			break;
		    }
		}
	    }
	}
	if (!subdivided)
	    unresolved_rigorous_keys.push_back(required);
    }
    approximation_faces.insert(reconciled_rigorous_faces.begin(),
	reconciled_rigorous_faces.end());
    const size_t missing_rigorous_triangles =
	unresolved_rigorous_keys.size();
    double missing_rigorous_area = 0.0;
    std::set<int> missing_rigorous_brep_faces;
    for (const repair_triangle_key &missing : unresolved_rigorous_keys) {
	missing_rigorous_area += repair_triangle_key_area(missing);
	collect_source_faces(missing, missing_rigorous_brep_faces);
    }
    report->missing_rigorous_triangles = (int)std::min(
	missing_rigorous_triangles, (size_t)INT_MAX);
    report->retained_rigorous_triangles = rigorous_input_face_count -
	report->missing_rigorous_triangles;
    bool bounded_local_replacement = false;
    repair_missing_patch_report missing_patch_report;
    if (missing_rigorous_triangles &&
	    !settings->use_full_fast_fallback &&
	    !settings->use_poisson_reconstruction &&
	    !settings->mesh.union_components) {
	bounded_local_replacement = repair_missing_patch_bounded(
	    unresolved_rigorous_keys, report->mesh.input_area,
	    settings->mesh, &missing_patch_report);
    }
    if (missing_rigorous_triangles &&
	    !settings->use_full_fast_fallback &&
	    !settings->use_poisson_reconstruction &&
	    !settings->mesh.union_components &&
	    !bounded_local_replacement) {
	bu_free(repaired_faces, "nonlocal repaired faces");
	bu_free(repaired_points, "nonlocal repaired vertices");
	bu_free(input_faces, "nonlocal repair input faces");
	bu_free(input_vertices, "nonlocal repair input vertices");
	const std::string message = "local mesh repair would replace " +
	    std::to_string(missing_rigorous_triangles) +
	    " certified rigorous triangle" +
	    (missing_rigorous_triangles == 1 ? "" : "s") +
	    " with total area " + std::to_string(missing_rigorous_area) +
	    " from B-Rep face" +
	    (missing_rigorous_brep_faces.size() == 1 ? " " : "s ") +
	    ([&]() {
		std::string faces;
		for (int face : missing_rigorous_brep_faces) {
		    if (!faces.empty())
			faces += ",";
		    faces += std::to_string(face);
		}
		return faces;
	    })();
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }
    if (bounded_local_replacement) {
	report->replaced_rigorous_components =
	    missing_patch_report.components;
	report->largest_replaced_rigorous_triangles =
	    missing_patch_report.largest_faces;
	report->largest_replaced_boundary_edges =
	    missing_patch_report.largest_boundary_edges;
	report->replaced_rigorous_area = missing_patch_report.total_area;
	report->largest_replaced_rigorous_area =
	    missing_patch_report.largest_area;
	approximation_faces.insert(missing_rigorous_brep_faces.begin(),
	    missing_rigorous_brep_faces.end());
    }

    const bool have_display_reference = display_reference_face_count > 0 &&
	!display_reference_faces.empty() &&
	!display_reference_vertices.empty();
    report->reference_area = have_display_reference ?
	repair_mesh_area(display_reference_vertices.data(),
	    display_reference_faces.data(), display_reference_face_count) :
	(degenerate_neighborhood_repair ? pre_neighborhood_repair_area :
	 report->mesh.input_area);
    if (report->reference_area > 0.0 &&
	    std::isfinite(report->reference_area) &&
	    std::isfinite(report->mesh.output_area)) {
	report->reference_area_change_percent = 100.0 * std::fabs(
	    report->mesh.output_area - report->reference_area) /
	    report->reference_area;
    } else {
	report->reference_area_change_percent =
	    std::numeric_limits<fastf_t>::infinity();
    }
    if (report->mesh.input_area > 0.0 &&
	    std::isfinite(report->mesh.input_area) &&
	    std::isfinite(report->mesh.output_area)) {
	report->area_change_percent = 100.0 * std::fabs(
	    report->mesh.output_area - report->mesh.input_area) /
	    report->mesh.input_area;
    } else {
	report->area_change_percent =
	    std::numeric_limits<fastf_t>::infinity();
    }
    const bool use_reference_area = have_display_reference ||
	degenerate_neighborhood_stats.inferred_topology;
    const fastf_t guarded_area_change = use_reference_area ?
	report->reference_area_change_percent : report->area_change_percent;
    if (settings->max_area_change_percent > 0.0 &&
	    (!(guarded_area_change <= validation_area_change_limit))) {
	bu_free(repaired_faces, "certified repaired faces");
	bu_free(repaired_points, "certified repaired vertices");
	bu_free(input_faces, "repair input faces");
	bu_free(input_vertices, "repair input vertices");
	std::string message = use_reference_area ?
	    "repaired mesh area differs from display reference by " :
	    "repaired mesh area changed by ";
	message += std::to_string(guarded_area_change) +
	    "% (strict limit " +
	    std::to_string(settings->max_area_change_percent);
	if (relaxed_fidelity_enabled)
	    message += "%, relaxed limit " +
		std::to_string(validation_area_change_limit);
	message += "%)";
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }
    if (settings->max_area_change_percent > 0.0 &&
	    guarded_area_change > settings->max_area_change_percent)
	relaxed_fidelity_applied = true;

    std::vector<repair_changed_face> changed_faces;
    for (int face = 0; face < repaired_face_count; ++face) {
	const int *triangle = &repaired_faces[(size_t)face * 3];
	const repair_triangle_key key = repair_triangle_coordinates(
	    repaired_vertices, triangle);
	if (rigorous_triangles.find(key) != rigorous_triangles.end())
	    continue;
	int source_brep_face = -1;
	bool local_surface_approximation = false;
	const auto local_source = local_chart_triangle_brep_faces.find(key);
	if (local_source != local_chart_triangle_brep_faces.end() &&
		local_source->second.size() == 1) {
	    source_brep_face = *local_source->second.begin();
	    local_surface_approximation = true;
	}
	const auto fast_source = fast_triangle_brep_faces.find(key);
	if (source_brep_face < 0 && fast_source !=
		fast_triangle_brep_faces.end() &&
		fast_source->second.size() == 1) {
	    source_brep_face = *fast_source->second.begin();
	    /* The constrained fallback records an unambiguous source face.
	     * Test that surface first before searching the entire B-Rep. */
	    local_surface_approximation = true;
	}
	const ON_3dPoint a(&repaired_vertices[(size_t)triangle[0] * 3]);
	const ON_3dPoint b(&repaired_vertices[(size_t)triangle[1] * 3]);
	const ON_3dPoint c(&repaired_vertices[(size_t)triangle[2] * 3]);
	const auto direct_surface = direct_surface_triangle_deviations.find(key);
	changed_faces.push_back({face,
	    0.5 * ON_CrossProduct(b - a, c - a).Length(),
	    source_brep_face, local_surface_approximation,
	    direct_surface != direct_surface_triangle_deviations.end(),
	    direct_surface != direct_surface_triangle_deviations.end() ?
	    direct_surface->second : 0.0});
    }
    report->changed_faces = (int)changed_faces.size();

    const size_t sample_limit = settings->max_deviation_samples ?
	settings->max_deviation_samples : 4096;
    std::vector<repair_changed_face> sampled_changed_faces;
    if (changed_faces.size() <= sample_limit) {
	sampled_changed_faces = changed_faces;
    } else {
	/* Cover the full deterministic output order rather than testing only
	 * the largest triangles.  Always include the largest changed triangle,
	 * then distribute the remaining centroid samples across the mesh. */
	sampled_changed_faces.reserve(sample_limit);
	for (size_t sample = 0; sample < sample_limit; ++sample) {
	    const long double position_fraction =
		((long double)sample + 0.5L) /
		(long double)sample_limit;
	    const size_t position = std::min(changed_faces.size() - 1,
		(size_t)(position_fraction *
		(long double)changed_faces.size()));
	    sampled_changed_faces.push_back(changed_faces[position]);
	}
	const repair_changed_face largest = *std::max_element(
	    changed_faces.begin(), changed_faces.end(),
	    [](const repair_changed_face &first,
		    const repair_changed_face &second) {
		return first.area < second.area;
	    });
	bool sampled_largest = false;
	for (const repair_changed_face &sampled : sampled_changed_faces)
	    sampled_largest = sampled_largest || sampled.index == largest.index;
	if (!sampled_largest)
	    sampled_changed_faces[0] = largest;
    }

    /* A retained adaptive face already has authoritative chart UVs.  Its
     * chord samples can be bounded by evaluating the corresponding surface
     * parameters directly, avoiding a global closest-point solve.  This is a
     * conservative proof: use it only when every coordinate-identical chart
     * triangle agrees and the upper bound already satisfies the requested
     * fidelity limit. */
    for (repair_changed_face &changed : sampled_changed_faces) {
	if (changed.direct_surface_samples)
	    continue;
	const repair_triangle_key key = repair_triangle_coordinates(
	    repaired_vertices,
	    &repaired_faces[(size_t)changed.index * 3]);
	const auto candidates = best_effort_surface_triangles.find(key);
	if (candidates == best_effort_surface_triangles.end() ||
		candidates->second.empty())
	    continue;
	bool proved = true;
	double maximum = 0.0;
	for (const auto &candidate : candidates->second) {
	    const auto mesh = s_cdt->fmeshes.find(candidate.first);
	    double deviation = 0.0;
	    if (mesh == s_cdt->fmeshes.end() ||
		    !mesh->second.surface_triangle_deviation(candidate.second,
			&deviation)) {
		proved = false;
		break;
	    }
	    maximum = std::max(maximum, deviation);
	}
	if (proved && maximum <= validation_surface_deviation) {
	    changed.direct_surface_samples = true;
	    changed.direct_surface_deviation = maximum;
	}
    }

    RTree<size_t, double, 3> input_triangle_index;
    const int *reference_input_faces = have_display_reference ?
	display_reference_faces.data() : input_faces;
    const fastf_t *reference_input_vertices = have_display_reference ?
	display_reference_vertices.data() : input_vertices;
    /* Every nonrigorous face present here entered through an explicitly
     * enabled, provenance-tagged local fallback.  After source-surface
     * projection fails, let a small topology repair justify its changed
     * samples against that bounded local interpretation.  The reverse
     * coverage and area gates below still prevent accepting a missing or
     * wholesale replacement region. */
    const int surface_reference_face_count = have_display_reference ?
	display_reference_face_count : input_face_count;
    for (int face = 0; face < surface_reference_face_count; ++face) {
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
	    const int vertex = reference_input_faces[(size_t)face * 3 + corner];
	    for (int axis = 0; axis < 3; ++axis) {
		const double coordinate =
		    reference_input_vertices[(size_t)vertex * 3 + axis];
		minimum[axis] = std::min(minimum[axis], coordinate);
		maximum[axis] = std::max(maximum[axis], coordinate);
	    }
	}
	input_triangle_index.Insert(minimum, maximum, (size_t)face);
    }

    std::sort(sampled_changed_faces.begin(), sampled_changed_faces.end(),
	[](const repair_changed_face &first,
		const repair_changed_face &second) {
	    if (first.area > second.area)
		return true;
	    if (second.area > first.area)
		return false;
	    return first.index < second.index;
	});
    size_t remaining_extra_samples =
	sample_limit - sampled_changed_faces.size();
    double squared_distance_sum = 0.0;
    int worst_deviation_brep_face = -1;
    const std::set<int> local_surface_faces(
	locally_reconstructed_faces.begin(), locally_reconstructed_faces.end());
    for (const repair_changed_face &changed : sampled_changed_faces) {
	const int *triangle = &repaired_faces[(size_t)changed.index * 3];
	const ON_3dPoint a(&repaired_vertices[(size_t)triangle[0] * 3]);
	const ON_3dPoint b(&repaired_vertices[(size_t)triangle[1] * 3]);
	const ON_3dPoint c(&repaired_vertices[(size_t)triangle[2] * 3]);
	std::array<ON_3dPoint, 4> samples = {
	    (a + b + c) / 3.0,
	    (a + b) / 2.0,
	    (b + c) / 2.0,
	    (c + a) / 2.0
	};
	const size_t face_sample_count = 1 +
	    std::min((size_t)3, remaining_extra_samples);
	remaining_extra_samples -= face_sample_count - 1;
	for (size_t sample = 0; sample < face_sample_count; ++sample) {
	    double distance = 0.0;
	    bool used_untrimmed = false;
	    report->deviation_samples++;
	    bool matched_local_surface = false;
	    std::set<int> local_filter;
	    if (changed.local_surface_approximation &&
		    changed.source_brep_face >= 0)
		local_filter.insert(changed.source_brep_face);
	    else if (changed.source_brep_face < 0 &&
		    !local_surface_faces.empty())
		local_filter = local_surface_faces;
	    if (changed.direct_surface_samples) {
		distance = changed.direct_surface_deviation;
		used_untrimmed = true;
		matched_local_surface = distance <=
		    validation_surface_deviation;
		if (changed.source_brep_face >= 0 &&
			best_effort_face_indices.find(changed.source_brep_face) !=
			best_effort_face_indices.end()) {
		    report->best_effort_reference_samples++;
		    report->max_best_effort_surface_deviation = std::max(
			report->max_best_effort_surface_deviation,
			(fastf_t)distance);
		}
	    } else if (!local_filter.empty()) {
		matched_local_surface = repair_surface_distance(
		    s_cdt->orig_brep, face_bounds, face_trees, face_contexts,
		    samples[sample], validation_surface_deviation, true,
		    &distance, &used_untrimmed, NULL, NULL, &local_filter);
	    }
	    if (!matched_local_surface &&
		    !repair_surface_distance(s_cdt->orig_brep, face_bounds,
		    face_trees, face_contexts, samples[sample],
		    validation_surface_deviation,
		    settings->allow_untrimmed_surface_match != 0, &distance,
		    &used_untrimmed)) {
		if (!repair_input_mesh_distance(input_triangle_index,
			reference_input_vertices, reference_input_faces,
			samples[sample], validation_surface_deviation,
			&distance)) {
		    const double diagnostic_limit =
			validation_surface_deviation <=
			std::numeric_limits<double>::max() / 4.0 ?
			4.0 * validation_surface_deviation :
			validation_surface_deviation;
		    double surface_distance =
			std::numeric_limits<double>::infinity();
		    double mesh_distance =
			std::numeric_limits<double>::infinity();
		    bool diagnostic_untrimmed = false;
		    int surface_face = -1;
		    size_t mesh_face = std::numeric_limits<size_t>::max();
		    const bool have_surface_distance = repair_surface_distance(
			s_cdt->orig_brep, face_bounds, face_trees, face_contexts,
			samples[sample], diagnostic_limit,
			settings->allow_untrimmed_surface_match != 0,
			&surface_distance, &diagnostic_untrimmed, NULL,
			&surface_face);
		    const bool have_mesh_distance = repair_input_mesh_distance(
			input_triangle_index, reference_input_vertices,
			reference_input_faces, samples[sample], diagnostic_limit,
			&mesh_distance, &mesh_face);
		    if (have_surface_distance || have_mesh_distance) {
			int diagnostic_face = -1;
			if (have_surface_distance) {
			    distance = surface_distance;
			    diagnostic_face = surface_face;
			} else {
			    distance = mesh_distance;
			    if (have_display_reference && mesh_face <
				    display_reference_brep_faces.size())
				diagnostic_face = display_reference_brep_faces[
				    mesh_face];
			}
			if (have_surface_distance && have_mesh_distance &&
				mesh_distance < distance) {
			    distance = mesh_distance;
			    diagnostic_face = -1;
			    if (have_display_reference && mesh_face <
				    display_reference_brep_faces.size())
				diagnostic_face = display_reference_brep_faces[
				    mesh_face];
			}
			if (distance > report->max_surface_deviation) {
			    report->max_surface_deviation = distance;
			    worst_deviation_brep_face = diagnostic_face;
			}
		    }
		    report->deviation_projection_failures++;
		    continue;
		}
		report->input_mesh_surface_samples++;
		if (changed.source_brep_face >= 0 &&
			best_effort_face_indices.find(changed.source_brep_face) !=
			best_effort_face_indices.end()) {
		    /* The approximation is accepted against the retained surface
		     * mesh, but still quantify how far its chord samples lie from
		     * the source face.  A miss means no face projection was found
		     * within four requested tolerances; report it without confusing
		     * this explicitly tagged interpretation with an exact match. */
		    const double support_limit =
			report->allowed_surface_deviation <=
			std::numeric_limits<double>::max() / 4.0 ?
			4.0 * report->allowed_surface_deviation :
			report->allowed_surface_deviation;
		    std::set<int> support_filter = {
			changed.source_brep_face
		    };
		    double support_distance =
			std::numeric_limits<double>::infinity();
		    bool support_untrimmed = false;
		    if (!repair_surface_distance(s_cdt->orig_brep,
			    face_bounds, face_trees, face_contexts,
			    samples[sample], support_limit, true,
			    &support_distance, &support_untrimmed, NULL, NULL,
			    &support_filter)) {
			report->best_effort_reference_failures++;
			continue;
		    }
		    report->best_effort_reference_samples++;
		    report->max_best_effort_surface_deviation = std::max(
			report->max_best_effort_surface_deviation,
			(fastf_t)support_distance);
		}
	    }
	    if (used_untrimmed)
		report->untrimmed_surface_samples++;
	    report->max_surface_deviation = std::max(
		report->max_surface_deviation, (fastf_t)distance);
	    squared_distance_sum += distance * distance;
	}
    }
    const size_t projected_samples = report->deviation_samples -
	report->deviation_projection_failures;
    if (projected_samples)
	report->rms_surface_deviation = std::sqrt(
	    squared_distance_sum / (double)projected_samples);

    if (report->deviation_projection_failures ||
	    report->max_surface_deviation >
	    validation_surface_deviation) {
	bu_free(repaired_faces, "certified repaired faces");
	bu_free(repaired_points, "certified repaired vertices");
	bu_free(input_faces, "repair input faces");
	bu_free(input_vertices, "repair input vertices");
	std::string message = "repaired surface failed B-Rep deviation check: " +
	    std::to_string(report->deviation_projection_failures) +
	    " projection failures, maximum " +
	    std::to_string(report->max_surface_deviation) +
	    " (strict limit " +
	    std::to_string(report->allowed_surface_deviation);
	if (relaxed_fidelity_enabled)
	    message += ", relaxed limit " +
		std::to_string(validation_surface_deviation);
	message += ")";
	if (worst_deviation_brep_face >= 0)
	    message += ", nearest display B-Rep face " +
		std::to_string(worst_deviation_brep_face);
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }
    if (report->max_surface_deviation >
	    report->allowed_surface_deviation)
	relaxed_fidelity_applied = true;

    /* Output-to-source deviation alone can accept a small reconstructed
     * subset of a much larger model, particularly when matching underlying
     * untrimmed surfaces is explicitly allowed.  Sample the display or
     * rigorous input in the reverse direction so missing target regions are
     * also bounded by the same fidelity limit. */
    RTree<size_t, double, 3> repaired_triangle_index;
    for (int face = 0; face < repaired_face_count; ++face) {
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
	    const int vertex = repaired_faces[(size_t)face * 3 + corner];
	    for (int axis = 0; axis < 3; ++axis) {
		const double coordinate =
		    repaired_vertices[(size_t)vertex * 3 + axis];
		minimum[axis] = std::min(minimum[axis], coordinate);
		maximum[axis] = std::max(maximum[axis], coordinate);
	    }
	}
	repaired_triangle_index.Insert(minimum, maximum, (size_t)face);
    }
    std::vector<repair_changed_face> reference_faces;
    const int coverage_reference_face_count = have_display_reference ?
	display_reference_face_count : input_face_count;
    reference_faces.reserve((size_t)coverage_reference_face_count);
    for (int face = 0; face < coverage_reference_face_count; ++face) {
	const int *triangle = &reference_input_faces[(size_t)face * 3];
	const ON_3dPoint a(&reference_input_vertices[
	    (size_t)triangle[0] * 3]);
	const ON_3dPoint b(&reference_input_vertices[
	    (size_t)triangle[1] * 3]);
	const ON_3dPoint c(&reference_input_vertices[
	    (size_t)triangle[2] * 3]);
	reference_faces.push_back({face,
	    0.5 * ON_CrossProduct(b - a, c - a).Length()});
    }
    std::vector<repair_changed_face> sampled_reference_faces;
    if (reference_faces.size() <= sample_limit) {
	sampled_reference_faces = reference_faces;
    } else {
	sampled_reference_faces.reserve(sample_limit);
	for (size_t sample = 0; sample < sample_limit; ++sample) {
	    const long double fraction = ((long double)sample + 0.5L) /
		(long double)sample_limit;
	    const size_t position = std::min(reference_faces.size() - 1,
		(size_t)(fraction * (long double)reference_faces.size()));
	    sampled_reference_faces.push_back(reference_faces[position]);
	}
	const repair_changed_face largest = *std::max_element(
	    reference_faces.begin(), reference_faces.end(),
	    [](const repair_changed_face &first,
		    const repair_changed_face &second) {
		return first.area < second.area;
	    });
	bool sampled_largest = false;
	for (const repair_changed_face &sampled : sampled_reference_faces)
	    sampled_largest = sampled_largest || sampled.index == largest.index;
	if (!sampled_largest)
	    sampled_reference_faces[0] = largest;
    }
    double coverage_squared_distance_sum = 0.0;
    int worst_coverage_brep_face = -1;
    const auto sample_output_coverage = [&](const ON_3dPoint &sample,
	    int source_brep_face) {
	double distance = 0.0;
	report->coverage_samples++;
	if (!repair_input_mesh_distance(repaired_triangle_index,
		repaired_vertices, repaired_faces, sample,
		validation_surface_deviation, &distance)) {
	    report->coverage_failures++;
	    const double diagnostic_limit =
		validation_surface_deviation <=
		std::numeric_limits<double>::max() / 4.0 ?
		4.0 * validation_surface_deviation :
		validation_surface_deviation;
	    if (repair_input_mesh_distance(repaired_triangle_index,
		    repaired_vertices, repaired_faces, sample,
		    diagnostic_limit, &distance) && distance >
		    report->max_coverage_deviation) {
		report->max_coverage_deviation = distance;
		worst_coverage_brep_face = source_brep_face;
	    }
	    return;
	}
	report->max_coverage_deviation = std::max(
	    report->max_coverage_deviation, (fastf_t)distance);
	coverage_squared_distance_sum += distance * distance;
    };

    /* The ordinary deterministic coverage budget may omit a few triangles
     * from a large mesh.  A locally replaced neighborhood is exceptional and
     * small by construction, so explicitly test its centroid and edge
     * midpoints against the final mesh before accepting the interpretation. */
    if (bounded_local_replacement) {
	for (const repair_triangle_key &missing : unresolved_rigorous_keys) {
	    const ON_3dPoint a(missing[0].data());
	    const ON_3dPoint b(missing[1].data());
	    const ON_3dPoint c(missing[2].data());
	    int source_brep_face = -1;
	    const auto source = missing_triangle_sources.find(missing);
	    if (source != missing_triangle_sources.end() &&
		    source->second.size() == 1)
		source_brep_face = *source->second.begin();
	    sample_output_coverage((a + b + c) / 3.0, source_brep_face);
	    sample_output_coverage((a + b) / 2.0, source_brep_face);
	    sample_output_coverage((b + c) / 2.0, source_brep_face);
	    sample_output_coverage((c + a) / 2.0, source_brep_face);
	}
    }
    size_t remaining_coverage_extra =
	sample_limit - sampled_reference_faces.size();
    for (const repair_changed_face &reference : sampled_reference_faces) {
	const int *triangle =
	    &reference_input_faces[(size_t)reference.index * 3];
	const ON_3dPoint a(&reference_input_vertices[
	    (size_t)triangle[0] * 3]);
	const ON_3dPoint b(&reference_input_vertices[
	    (size_t)triangle[1] * 3]);
	const ON_3dPoint c(&reference_input_vertices[
	    (size_t)triangle[2] * 3]);
	std::array<ON_3dPoint, 4> samples = {
	    (a + b + c) / 3.0,
	    (a + b) / 2.0,
	    (b + c) / 2.0,
	    (c + a) / 2.0
	};
	const size_t face_sample_count = 1 +
	    std::min((size_t)3, remaining_coverage_extra);
	remaining_coverage_extra -= face_sample_count - 1;
	int source_brep_face = -1;
	if (have_display_reference && reference.index >= 0 &&
		(size_t)reference.index < display_reference_brep_faces.size())
	    source_brep_face = display_reference_brep_faces[
		(size_t)reference.index];
	else if (!have_display_reference && reference.index >= 0 &&
		(size_t)reference.index < rigorous_input_brep_faces.size())
	    source_brep_face = rigorous_input_brep_faces[
		(size_t)reference.index];
	for (size_t sample = 0; sample < face_sample_count; ++sample) {
	    sample_output_coverage(samples[sample], source_brep_face);
	}
    }
    const size_t covered_samples = report->coverage_samples -
	report->coverage_failures;
    if (covered_samples)
	report->rms_coverage_deviation = std::sqrt(
	    coverage_squared_distance_sum / (double)covered_samples);
    if (report->coverage_failures || report->max_coverage_deviation >
	    validation_surface_deviation) {
	bu_free(repaired_faces, "certified repaired faces");
	bu_free(repaired_points, "certified repaired vertices");
	bu_free(input_faces, "repair input faces");
	bu_free(input_vertices, "repair input vertices");
	std::string message = "repaired surface failed input coverage check: " +
	    std::to_string(report->coverage_failures) +
	    " coverage failures, maximum " +
	    std::to_string(report->max_coverage_deviation) +
	    " (strict limit " +
	    std::to_string(report->allowed_surface_deviation);
	if (relaxed_fidelity_enabled)
	    message += ", relaxed limit " +
		std::to_string(validation_surface_deviation);
	message += ")";
	if (worst_coverage_brep_face >= 0)
	    message += ", worst B-Rep face " +
		std::to_string(worst_coverage_brep_face);
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }
    if (report->max_coverage_deviation >
	    report->allowed_surface_deviation)
	relaxed_fidelity_applied = true;
    report->relaxed_fidelity_applied = relaxed_fidelity_applied ? 1 : 0;

    bu_free(input_faces, "repair input faces");
    bu_free(input_vertices, "repair input vertices");
    if (s_cdt->certified_faces)
	bu_free(s_cdt->certified_faces, "certified faces");
    if (s_cdt->certified_vertices)
	bu_free(s_cdt->certified_vertices, "certified vertices");
    if (s_cdt->certified_face_normals)
	bu_free(s_cdt->certified_face_normals, "certified face normals");
    if (s_cdt->certified_normals)
	bu_free(s_cdt->certified_normals, "certified normals");
    s_cdt->certified_faces = repaired_faces;
    s_cdt->certified_face_count = repaired_face_count;
    s_cdt->certified_vertices = (fastf_t *)repaired_points;
    s_cdt->certified_vertex_count = repaired_vertex_count;
    s_cdt->certified_face_normals = NULL;
    s_cdt->certified_face_normal_count = 0;
    s_cdt->certified_normals = NULL;
    s_cdt->certified_normal_count = 0;
    s_cdt->certified_repaired = true;
    s_cdt->status = BREP_CDT_SOLID;
    if (report->relaxed_fidelity_applied)
	report->approximation_tier =
	    BREP_CDT_REPAIR_APPROX_RELAXED_FIDELITY;
    else if (report->poisson_reconstruction_applied)
	report->approximation_tier = BREP_CDT_REPAIR_APPROX_POISSON;
    else if (report->full_fast_fallback_used)
	report->approximation_tier = BREP_CDT_REPAIR_APPROX_FULL_FAST;
    else if (report->mesh.added_faces > 0 ||
	    !locally_reconstructed_faces.empty() ||
	    bounded_local_replacement ||
	    !s_cdt->approximated_edges.empty())
	report->approximation_tier = BREP_CDT_REPAIR_APPROX_LOCAL_MESH;
    else if (report->fast_fallback_used_faces > 0)
	report->approximation_tier =
	    BREP_CDT_REPAIR_APPROX_CONSTRAINED_FACE;
    if (report->full_fast_fallback_used ||
	    report->poisson_reconstruction_applied ||
	    (report->relaxed_fidelity_applied &&
	    approximation_faces.empty())) {
	approximation_faces.clear();
	for (int face = 0; face < s_cdt->orig_brep->m_F.Count(); ++face)
	    approximation_faces.insert(face);
    }
    std::set<int> approximation_edges;
    for (int face_index : approximation_faces) {
	if (face_index < 0 || face_index >= s_cdt->orig_brep->m_F.Count())
	    continue;
	const ON_BrepFace &face = s_cdt->orig_brep->m_F[face_index];
	for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	    const ON_BrepLoop *loop = face.Loop(loop_index);
	    if (!loop)
		continue;
	    for (int trim_index = 0; trim_index < loop->TrimCount();
		    ++trim_index) {
		const ON_BrepTrim *trim = loop->Trim(trim_index);
		if (trim && trim->m_ei >= 0)
		    approximation_edges.insert(trim->m_ei);
	    }
	}
    }
    report->approximation_faces = (int)std::min(
	approximation_faces.size(), (size_t)INT_MAX);
    report->approximation_edges = (int)std::min(
	approximation_edges.size(), (size_t)INT_MAX);
    if (settings->provenance && report->approximation_tier !=
	    BREP_CDT_REPAIR_APPROX_NONE) {
	const std::vector<int> face_indices(approximation_faces.begin(),
	    approximation_faces.end());
	const std::vector<int> edge_indices(approximation_edges.begin(),
	    approximation_edges.end());
	settings->provenance(report->approximation_tier,
	    face_indices.empty() ? NULL : face_indices.data(),
	    face_indices.size(), edge_indices.empty() ? NULL :
	    edge_indices.data(), edge_indices.size(),
	    settings->provenance_data);
    }
    std::string message = "certified repaired mesh: " +
	std::to_string(report->mesh.removed_faces) + " removed, " +
	std::to_string(report->mesh.added_faces) + " added, " +
	std::to_string(report->changed_faces) +
	" changed; maximum sampled deviation " +
	std::to_string(report->max_surface_deviation);
    if (report->relaxed_fidelity_applied)
	message += "; relaxed fidelity factor " +
	    std::to_string(report->relaxed_fidelity_factor);
    if (report->adaptive_hole_retry_attempted)
	message += "; adaptive hole edge ceiling " +
	    std::to_string(report->adaptive_hole_edges);
    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIRED,
	BREP_CDT_STAGE_MESH_REPAIR, -1,
	report->source_diagnostic.completed_faces,
	report->source_failed_faces, message.c_str());
    return 0;
}

int
ON_Brep_CDT_Repair(struct ON_Brep_CDT_State *s_cdt,
	const struct brep_cdt_repair_settings *settings,
	struct brep_cdt_repair_report *report)
{
    struct brep_cdt_repair_report local_report =
	BREP_CDT_REPAIR_REPORT_INIT;
    struct brep_cdt_repair_report *active_report = report ? report :
	&local_report;
    bool relaxed_tessellation_attempted = false;
    bool relaxed_tessellation_certified = false;
    int relaxed_tessellation_completed_faces = 0;
    bool bounded_edge_retry_attempted = false;
    bool bounded_edge_retry_certified = false;
    int bounded_edge_retry_completed_faces = 0;
    if (s_cdt && settings && settings->mesh.fill_holes &&
	    !settings->use_full_fast_fallback && s_cdt->orig_brep &&
	    s_cdt->diagnostic.result ==
	    BREP_CDT_RESULT_INITIALIZATION_FAILED &&
	    s_cdt->diagnostic.stage == BREP_CDT_STAGE_EDGE_INITIALIZATION &&
	    strstr(s_cdt->diagnostic.message, "could not be split")) {
	const struct brep_cdt_diagnostic source_diagnostic =
	    s_cdt->diagnostic;
	double approximation_tolerance = settings->max_surface_deviation;
	if (!(approximation_tolerance > 0.0) ||
		!std::isfinite(approximation_tolerance))
	    approximation_tolerance = s_cdt->absmax;
	if (approximation_tolerance > 0.0 &&
		std::isfinite(approximation_tolerance)) {
	    bounded_edge_retry_attempted = true;
	    s_cdt->allow_bounded_edge_approximation = true;
	    s_cdt->bounded_edge_approximation_tolerance =
		approximation_tolerance;
	    const int retry_result = brep_cdt_tessellate(s_cdt, 0, NULL,
		false);
	    s_cdt->allow_bounded_edge_approximation = false;
	    s_cdt->bounded_edge_approximation_tolerance = 0.0;
	    bounded_edge_retry_certified = retry_result == 0;
	    bounded_edge_retry_completed_faces =
		s_cdt->diagnostic.completed_faces;
	    s_cdt->repair_source_diagnostic = source_diagnostic;
	    s_cdt->repair_source_valid = true;
	}
    }
    if (s_cdt && settings && settings->try_invalid_brep &&
	    !settings->use_full_fast_fallback && !s_cdt->brep &&
	    s_cdt->fmeshes.empty() &&
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_INVALID_BREP &&
	    s_cdt->diagnostic.stage == BREP_CDT_STAGE_TOPOLOGY) {
	const struct brep_cdt_diagnostic source_diagnostic =
	    s_cdt->diagnostic;
	relaxed_tessellation_attempted = true;
	const int relaxed_result = brep_cdt_tessellate(s_cdt, 0, NULL, true);
	relaxed_tessellation_certified = relaxed_result == 0;
	relaxed_tessellation_completed_faces =
	    s_cdt->diagnostic.completed_faces;
	if (!s_cdt->failed_face_indices.empty()) {
	    std::string failures = "relaxed rigorous tessellation failed faces";
	    for (int failed_face : s_cdt->failed_face_indices)
		failures += " " + std::to_string(failed_face);
	    bu_log("%s\n", failures.c_str());
	}
	s_cdt->repair_source_diagnostic = source_diagnostic;
	s_cdt->repair_source_valid = true;
    }
    const auto run_repair_attempt = [&](const brep_cdt_repair_settings *opts,
	    bool area_weighted, bool closure_biased, bool automatic_local) {
	int result = brep_cdt_repair_attempt(s_cdt, opts, active_report,
	    area_weighted, closure_biased, automatic_local);
	active_report->relaxed_tessellation_attempted =
	    relaxed_tessellation_attempted ? 1 : 0;
	active_report->relaxed_tessellation_completed_faces =
	    relaxed_tessellation_completed_faces;
	active_report->bounded_edge_retry_attempted =
	    bounded_edge_retry_attempted ? 1 : 0;
	active_report->bounded_edge_retry_completed_faces =
	    bounded_edge_retry_completed_faces;
	if (relaxed_tessellation_certified && result == 1) {
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIRED,
		BREP_CDT_STAGE_MESH_REPAIR, -1,
		relaxed_tessellation_completed_faces, 0,
		"relaxed rigorous tessellation certified an invalid B-Rep");
	    result = 0;
	}
	if (bounded_edge_retry_certified && result == 1) {
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIRED,
		BREP_CDT_STAGE_MESH_REPAIR, -1,
		bounded_edge_retry_completed_faces, 0,
		"bounded shared-edge approximation certified the B-Rep");
	    result = 0;
	}
	return result;
    };
    const bool valid_poisson_request = settings &&
	settings->use_poisson_reconstruction &&
	settings->use_full_fast_fallback && settings->poisson_depth >= 5 &&
	settings->poisson_depth <= 10 && settings->max_poisson_components &&
	settings->max_fast_points && settings->max_fast_result_bytes &&
	settings->max_fast_time_ms > 0 &&
	std::isfinite(settings->poisson_scale) &&
	!(settings->poisson_scale > 0.0) &&
	!(settings->poisson_scale < 0.0);
    if (valid_poisson_request) {
	/* Preserve the display tessellation whenever bounded local repair can
	 * certify it.  Poisson remains available below for the harder cases, but
	 * should not replace a close mesh merely because the caller enabled the
	 * last-resort tier. */
	struct brep_cdt_repair_settings local_settings = *settings;
	local_settings.use_poisson_reconstruction = 0;
	const int local_result = run_repair_attempt(&local_settings, false,
	    false, true);
	if (local_result >= 0)
	    return local_result;
    }
    const bool automatic_scale = settings &&
	settings->use_poisson_reconstruction &&
	std::isfinite(settings->poisson_scale) &&
	!(settings->poisson_scale > 0.0) &&
	!(settings->poisson_scale < 0.0);
    if (!automatic_scale) {
	const bool automatic_full_fast = settings &&
	    settings->use_full_fast_fallback_if_needed &&
	    !settings->use_full_fast_fallback &&
	    !settings->use_poisson_reconstruction;
	int result = run_repair_attempt(settings, false, false, false);
	struct brep_cdt_repair_report rigorous_report = *active_report;
	if (automatic_full_fast) {
	    active_report->rigorous_first_attempted = 1;
	    active_report->rigorous_first_result = result;
	    active_report->rigorous_first_fast_faces =
		rigorous_report.fast_fallback_used_faces;
	    active_report->rigorous_first_constrained_edges =
		rigorous_report.fast_fallback_constrained_edges;
	    active_report->rigorous_first_constrained_samples =
		rigorous_report.fast_fallback_constrained_samples;
	    active_report->rigorous_first_reference_area =
		rigorous_report.reference_area;
	    active_report->rigorous_first_output_area =
		rigorous_report.mesh.output_area;
	    active_report->rigorous_first_area_change_percent =
		rigorous_report.reference_area_change_percent;
	}
	if (result < 0 && automatic_full_fast) {
	    struct brep_cdt_repair_settings fallback_settings = *settings;
	    fallback_settings.use_full_fast_fallback = 1;
	    fallback_settings.use_full_fast_fallback_if_needed = 0;
	    fallback_settings.try_invalid_brep = 0;
	    result = run_repair_attempt(&fallback_settings, false, false,
		false);
	    active_report->rigorous_first_attempted = 1;
	    active_report->rigorous_first_result = -1;
	    active_report->rigorous_first_fast_faces =
		rigorous_report.fast_fallback_used_faces;
	    active_report->rigorous_first_constrained_edges =
		rigorous_report.fast_fallback_constrained_edges;
	    active_report->rigorous_first_constrained_samples =
		rigorous_report.fast_fallback_constrained_samples;
	    active_report->rigorous_first_reference_area =
		rigorous_report.reference_area;
	    active_report->rigorous_first_output_area =
		rigorous_report.mesh.output_area;
	    active_report->rigorous_first_area_change_percent =
		rigorous_report.reference_area_change_percent;
	}
	if (active_report->poisson_reconstruction_attempted)
	    active_report->poisson_attempts = 1;
	return result;
    }

    struct brep_cdt_repair_settings attempt_settings = *settings;
    int attempts = 0;
    const auto run_attempt = [&](double scale, bool area_weighted,
	    bool closure_biased) {
	attempt_settings.poisson_scale = scale;
	const int attempt_result = run_repair_attempt(&attempt_settings,
	    area_weighted, closure_biased, false);
	if (active_report->poisson_reconstruction_attempted)
	    attempts++;
	active_report->poisson_attempts = attempts;
	return attempt_result;
    };
    const auto sampling_retry_needed = [&](const brep_cdt_repair_report
	    *candidate) {
	return candidate->coverage_failures > 0 ||
	    (settings->max_area_change_percent > 0.0 &&
	    candidate->reference_area_change_percent >
	    settings->max_area_change_percent);
    };
    const auto closure_retry_needed = [](const brep_cdt_repair_report
	    *candidate) {
	return candidate->poisson_reconstruction_applied &&
	    candidate->mesh.unmatched_edges > 0 &&
	    !(candidate->reference_area > 0.0);
    };

    int result = run_attempt(1.1, false, false);
    if (result >= 0 || !active_report->poisson_reconstruction_applied)
	return result;
    bool sampling_failure_seen = sampling_retry_needed(active_report);
    bool closure_failure_seen = closure_retry_needed(active_report);

    result = run_attempt(1.2, false, false);
    if (result >= 0)
	return result;
    sampling_failure_seen = sampling_failure_seen ||
	sampling_retry_needed(active_report);
    closure_failure_seen = closure_failure_seen ||
	closure_retry_needed(active_report);
    if (sampling_failure_seen) {
	result = run_attempt(1.1, true, false);
	if (result >= 0 || !active_report->poisson_reconstruction_applied)
	    return result;
	closure_failure_seen = closure_failure_seen ||
	    closure_retry_needed(active_report);
	result = run_attempt(1.2, true, false);
	if (result >= 0)
	    return result;
	closure_failure_seen = closure_failure_seen ||
	    closure_retry_needed(active_report);
    }
    if (!closure_failure_seen)
	return result;

    result = run_attempt(1.1, false, true);
    if (result >= 0 || !active_report->poisson_reconstruction_applied)
	return result;
    bool closure_sampling_failure = sampling_retry_needed(active_report) ||
	closure_retry_needed(active_report);

    result = run_attempt(1.2, false, true);
    if (result >= 0)
	return result;
    closure_sampling_failure = closure_sampling_failure ||
	sampling_retry_needed(active_report) ||
	closure_retry_needed(active_report);
    if (!closure_sampling_failure)
	return result;

    result = run_attempt(1.1, true, true);
    if (result >= 0 || !active_report->poisson_reconstruction_applied)
	return result;
    return run_attempt(1.2, true, true);
}

static bool
cache_certified_normals(struct ON_Brep_CDT_State *s_cdt)
{
    if (!s_cdt || !s_cdt->certified_faces ||
	    s_cdt->certified_face_count <= 0 ||
	    !s_cdt->certified_vertices ||
	    s_cdt->certified_vertex_count <= 0)
	return false;
    if (s_cdt->certified_face_normals && s_cdt->certified_normals &&
	    s_cdt->certified_face_normal_count > 0 &&
	    s_cdt->certified_normal_count > 0)
	return true;

    if (s_cdt->certified_repaired) {
	std::vector<ON_3dVector> accumulated(
	    (size_t)s_cdt->certified_vertex_count, ON_3dVector::ZeroVector);
	std::vector<ON_3dVector> fallback(
	    (size_t)s_cdt->certified_vertex_count, ON_3dVector::ZeroVector);
	for (int face = 0; face < s_cdt->certified_face_count; ++face) {
	    int indices[3];
	    ON_3dPoint points[3];
	    for (int corner = 0; corner < 3; ++corner) {
		indices[corner] =
		    s_cdt->certified_faces[(size_t)face * 3 + corner];
		if (indices[corner] < 0 || indices[corner] >=
			s_cdt->certified_vertex_count)
		    return false;
		points[corner] = ON_3dPoint(
		    s_cdt->certified_vertices[(size_t)indices[corner] * 3],
		    s_cdt->certified_vertices[(size_t)indices[corner] * 3 + 1],
		    s_cdt->certified_vertices[(size_t)indices[corner] * 3 + 2]);
	    }
	    const ON_3dVector normal = ON_CrossProduct(points[1] - points[0],
		points[2] - points[0]);
	    if (!normal.IsValid() || !(normal.LengthSquared() > 0.0))
		return false;
	    for (int corner = 0; corner < 3; ++corner) {
		accumulated[(size_t)indices[corner]] += normal;
		if (!(fallback[(size_t)indices[corner]].LengthSquared() > 0.0))
		    fallback[(size_t)indices[corner]] = normal;
	    }
	}

	fastf_t *normals = (fastf_t *)bu_malloc(
	    (size_t)s_cdt->certified_vertex_count * 3 * sizeof(fastf_t),
	    "certified repaired normals");
	for (int vertex = 0; vertex < s_cdt->certified_vertex_count; ++vertex) {
	    ON_3dVector normal = accumulated[(size_t)vertex];
	    if (!normal.Unitize()) {
		normal = fallback[(size_t)vertex];
		if (!normal.Unitize()) {
		    bu_free(normals, "certified repaired normals");
		    return false;
		}
	    }
	    normals[(size_t)vertex * 3] = normal.x;
	    normals[(size_t)vertex * 3 + 1] = normal.y;
	    normals[(size_t)vertex * 3 + 2] = normal.z;
	}
	const size_t corner_count =
	    (size_t)s_cdt->certified_face_count * 3;
	int *face_normals = (int *)bu_malloc(corner_count * sizeof(int),
	    "certified repaired face normals");
	memcpy(face_normals, s_cdt->certified_faces,
	    corner_count * sizeof(int));
	s_cdt->certified_face_normals = face_normals;
	s_cdt->certified_face_normal_count = s_cdt->certified_face_count;
	s_cdt->certified_normals = normals;
	s_cdt->certified_normal_count = s_cdt->certified_vertex_count;
	return true;
    }

    if (s_cdt->bot_face_to_brep_face.size() !=
	    (size_t)s_cdt->certified_face_count ||
	    s_cdt->bot_face_to_cdt_triangle.size() !=
	    (size_t)s_cdt->certified_face_count)
	return false;

    const size_t corner_count =
	(size_t)s_cdt->certified_face_count * 3;
    std::vector<ON_3dPoint *> corner_normals(corner_count, NULL);
    std::vector<ON_3dPoint *> ordered_normals;
    ordered_normals.reserve(s_cdt->w3dnorms->size());
    std::unordered_map<ON_3dPoint *, size_t> normal_order;
    normal_order.reserve(s_cdt->w3dnorms->size());
    std::unordered_set<ON_3dPoint *> flip_normals;

    for (int face = 0; face < s_cdt->certified_face_count; ++face) {
	const int brep_face = s_cdt->bot_face_to_brep_face[(size_t)face];
	const size_t triangle_index =
	    s_cdt->bot_face_to_cdt_triangle[(size_t)face];
	const auto face_mesh = s_cdt->fmeshes.find(brep_face);
	if (face_mesh == s_cdt->fmeshes.end() ||
		triangle_index >= face_mesh->second.tris_vect.size())
	    return false;
	const cdt_mesh_t &mesh = face_mesh->second;
	const triangle_t &triangle = mesh.tris_vect[triangle_index];
	for (int corner = 0; corner < 3; ++corner) {
	    const int output_vertex =
		s_cdt->certified_faces[(size_t)face * 3 + corner];
	    if (output_vertex < 0 || (size_t)output_vertex >=
		    s_cdt->bot_pnt_to_on_pnt->size())
		return false;
	    ON_3dPoint *source_point =
		(*s_cdt->bot_pnt_to_on_pnt)[(size_t)output_vertex];
	    if (!source_point)
		return false;
	    int local_corner = -1;
	    for (int candidate = 0; candidate < 3; ++candidate) {
		const long point_index = triangle.v[candidate];
		if (point_index >= 0 && (size_t)point_index < mesh.pnts.size() &&
			mesh.pnts[(size_t)point_index] == source_point) {
		    local_corner = candidate;
		    break;
		}
	    }
	    if (local_corner < 0)
		return false;

	    const long point_index = triangle.v[local_corner];
	    ON_3dPoint *normal = NULL;
	    const auto singular =
		s_cdt->singular_vert_to_norms->find(source_point);
	    if (singular != s_cdt->singular_vert_to_norms->end()) {
		normal = singular->second;
	    } else {
		const auto normal_map = mesh.nmap.find(point_index);
		if (normal_map == mesh.nmap.end() || normal_map->second < 0 ||
			(size_t)normal_map->second >= mesh.normals.size())
		    return false;
		normal = mesh.normals[(size_t)normal_map->second];
	    }
	    if (!normal)
		return false;
	    corner_normals[(size_t)face * 3 + corner] = normal;
	    if (normal_order.emplace(normal, normal_order.size()).second)
		ordered_normals.push_back(normal);
	    if (mesh.m_bRev)
		flip_normals.insert(normal);
	}
    }

    std::sort(ordered_normals.begin(), ordered_normals.end(),
	[&normal_order](ON_3dPoint *a, ON_3dPoint *b) {
	    if (a->x < b->x) return true;
	    if (b->x < a->x) return false;
	    if (a->y < b->y) return true;
	    if (b->y < a->y) return false;
	    if (a->z < b->z) return true;
	    if (b->z < a->z) return false;
	    return normal_order.at(a) < normal_order.at(b);
	});
    for (size_t i = 0; i < ordered_normals.size(); ++i)
	normal_order[ordered_normals[i]] = i;

    int *face_normals = (int *)bu_malloc(corner_count * sizeof(int),
	"certified face normals");
    fastf_t *normals = (fastf_t *)bu_malloc(ordered_normals.size() * 3 *
	sizeof(fastf_t), "certified normals");
    for (size_t i = 0; i < corner_count; ++i)
	face_normals[i] = (int)normal_order.at(corner_normals[i]);
    for (size_t i = 0; i < ordered_normals.size(); ++i) {
	ON_3dVector normal(*ordered_normals[i]);
	if (flip_normals.find(ordered_normals[i]) != flip_normals.end())
	    normal = -normal;
	normals[i * 3] = normal.x;
	normals[i * 3 + 1] = normal.y;
	normals[i * 3 + 2] = normal.z;
    }

    s_cdt->certified_face_normals = face_normals;
    s_cdt->certified_face_normal_count = s_cdt->certified_face_count;
    s_cdt->certified_normals = normals;
    s_cdt->certified_normal_count = (int)ordered_normals.size();
    return true;
}

// Generate a BoT with normals.
int
ON_Brep_CDT_Mesh(
	int **faces, int *fcnt,
	fastf_t **vertices, int *vcnt,
	int **face_normals, int *fn_cnt,
	fastf_t **normals, int *ncnt,
	struct ON_Brep_CDT_State *s_cdt,
	int exp_face_cnt, int *exp_faces)
{
    size_t triangle_cnt = 0;
    if (!faces || !fcnt || !vertices || !vcnt || !s_cdt) {
	return -1;
    }
    /* We can ignore the face normals if we want, but if some of the
     * return variables are non-NULL they all need to be non-NULL */
    if (face_normals || fn_cnt || normals || ncnt) {
	if (!face_normals || !fn_cnt || !normals || !ncnt) {
	    return -1;
	}
    }

    *faces = NULL;
    *fcnt = 0;
    *vertices = NULL;
    *vcnt = 0;
    if (face_normals) {
	*face_normals = NULL;
	*fn_cnt = 0;
	*normals = NULL;
	*ncnt = 0;
    }

    const bool full_mesh = !exp_face_cnt || !exp_faces;
    const bool no_normals = !face_normals && !fn_cnt && !normals && !ncnt;
    if (full_mesh && s_cdt->status == BREP_CDT_SOLID &&
	    s_cdt->certified_faces && s_cdt->certified_vertices &&
	    s_cdt->certified_face_count > 0 &&
	    s_cdt->certified_vertex_count > 0) {
	if (!no_normals && !cache_certified_normals(s_cdt))
	    return -1;
	const size_t face_bytes = (size_t)s_cdt->certified_face_count * 3 *
	    sizeof(int);
	const size_t vertex_bytes = (size_t)s_cdt->certified_vertex_count * 3 *
	    sizeof(fastf_t);
	*faces = (int *)bu_malloc(face_bytes, "cached certified faces");
	*vertices = (fastf_t *)bu_malloc(vertex_bytes,
	    "cached certified vertices");
	memcpy(*faces, s_cdt->certified_faces, face_bytes);
	memcpy(*vertices, s_cdt->certified_vertices, vertex_bytes);
	*fcnt = s_cdt->certified_face_count;
	*vcnt = s_cdt->certified_vertex_count;
	if (!no_normals) {
	    const size_t face_normal_bytes =
		(size_t)s_cdt->certified_face_normal_count * 3 * sizeof(int);
	    const size_t normal_bytes =
		(size_t)s_cdt->certified_normal_count * 3 * sizeof(fastf_t);
	    *face_normals = (int *)bu_malloc(face_normal_bytes,
		"cached certified face normals");
	    *normals = (fastf_t *)bu_malloc(normal_bytes,
		"cached certified normals");
	    memcpy(*face_normals, s_cdt->certified_face_normals,
		face_normal_bytes);
	    memcpy(*normals, s_cdt->certified_normals, normal_bytes);
	    *fn_cnt = s_cdt->certified_face_normal_count;
	    *ncnt = s_cdt->certified_normal_count;
	}
	return 0;
    }
    if (!s_cdt->brep)
	return -1;

    s_cdt->bot_face_to_brep_face.clear();
    s_cdt->bot_face_to_cdt_triangle.clear();

    std::vector<int> active_faces;
    if (!exp_face_cnt || !exp_faces) {
	for (int face_index = 0; face_index < s_cdt->brep->m_F.Count(); face_index++) {
	    active_faces.push_back(face_index);
	}
    } else {
	for (int i = 0; i < exp_face_cnt; i++) {
	    if (exp_faces[i] < 0 || exp_faces[i] >= s_cdt->brep->m_F.Count())
		return -1;
	    active_faces.push_back(exp_faces[i]);
	}
    }
    std::sort(active_faces.begin(), active_faces.end());
    active_faces.erase(std::unique(active_faces.begin(), active_faces.end()),
	active_faces.end());

    /* RTree traversal order is an implementation detail.  Snapshot active
     * triangle IDs and order them by stable face and creation IDs. */
    std::map<int, std::vector<size_t>> active_triangles;
    for (size_t fi = 0; fi < active_faces.size(); fi++) {
	cdt_mesh_t *fmesh = &s_cdt->fmeshes[active_faces[fi]];
	RTree<size_t, double, 3>::Iterator tree_it;
	fmesh->tris_tree.GetFirst(tree_it);
	while (!tree_it.IsNull()) {
	    active_triangles[active_faces[fi]].push_back(*tree_it);
	    ++tree_it;
	}
	std::sort(active_triangles[active_faces[fi]].begin(),
	    active_triangles[active_faces[fi]].end());
	triangle_cnt += active_triangles[active_faces[fi]].size();
    }
    if (!triangle_cnt || triangle_cnt > INT_MAX)
	return -1;

    /* We know now the final triangle set.  We need to build up the set of
     * unique points and normals to generate a mesh containing only the
     * information actually used by the final triangle set. */
    typedef std::pair<ON_3dPoint *, size_t> ordered_point_entry;
    std::vector<ordered_point_entry> ordered_points;
    std::vector<ordered_point_entry> ordered_normals;
    std::unordered_set<ON_3dPoint *> point_seen;
    std::unordered_set<ON_3dPoint *> normal_seen;
    std::unordered_set<ON_3dPoint *> flip_normals;
    const size_t max_corner_count = triangle_cnt > SIZE_MAX / 3 ?
	SIZE_MAX : triangle_cnt * 3;
    point_seen.reserve(std::min(max_corner_count, s_cdt->w3dpnts->size()));
    ordered_points.reserve(std::min(max_corner_count,
	s_cdt->w3dpnts->size()));
    if (normals) {
	normal_seen.reserve(std::min(max_corner_count,
	    s_cdt->w3dnorms->size()));
	ordered_normals.reserve(std::min(max_corner_count,
	    s_cdt->w3dnorms->size()));
    }
    for (size_t fi = 0; fi < active_faces.size(); fi++) {
	cdt_mesh_t *fmesh = &s_cdt->fmeshes[active_faces[fi]];
	const std::vector<size_t> &face_triangles =
	    active_triangles[active_faces[fi]];
	for (size_t ti = 0; ti < face_triangles.size(); ++ti) {
	    triangle_t tri = fmesh->tris_vect[face_triangles[ti]];
	    for (size_t j = 0; j < 3; j++) {
		ON_3dPoint *p3d = fmesh->pnts[tri.v[j]];
		if (point_seen.insert(p3d).second)
		    ordered_points.push_back(std::make_pair(p3d,
			ordered_points.size()));
		if (normals) {
		    ON_3dPoint *onorm = NULL;
		    if (s_cdt->singular_vert_to_norms->find(p3d) !=
			    s_cdt->singular_vert_to_norms->end()) {
			// Use calculated normal for singularity points
			onorm = (*s_cdt->singular_vert_to_norms)[p3d];
		    } else {
			onorm = fmesh->normals[fmesh->nmap[tri.v[j]]];
		    }
		    if (onorm) {
			if (normal_seen.insert(onorm).second)
			    ordered_normals.push_back(std::make_pair(onorm,
				ordered_normals.size()));
			if (fmesh->m_bRev)
			    flip_normals.insert(onorm);
		    }
		}
	    }
	}
    }

    //bu_log("tri_cnt: %zd\n", triangle_cnt);

    const auto stable_point_less = [](const ordered_point_entry &a_entry,
	    const ordered_point_entry &b_entry) {
	ON_3dPoint *a = a_entry.first;
	ON_3dPoint *b = b_entry.first;
	if (a->x < b->x) return true;
	if (b->x < a->x) return false;
	if (a->y < b->y) return true;
	if (b->y < a->y) return false;
	if (a->z < b->z) return true;
	if (b->z < a->z) return false;
	return a_entry.second < b_entry.second;
    };
    const auto stable_normal_less = [](const ordered_point_entry &a_entry,
	    const ordered_point_entry &b_entry) {
	ON_3dPoint *a = a_entry.first;
	ON_3dPoint *b = b_entry.first;
	if (a->x < b->x) return true;
	if (b->x < a->x) return false;
	if (a->y < b->y) return true;
	if (b->y < a->y) return false;
	if (a->z < b->z) return true;
	if (b->z < a->z) return false;
	return a_entry.second < b_entry.second;
    };
    std::sort(ordered_points.begin(), ordered_points.end(), stable_point_less);
    std::sort(ordered_normals.begin(), ordered_normals.end(),
	stable_normal_less);

    if (ordered_points.empty() || ordered_points.size() > INT_MAX ||
	(normals && (ordered_normals.empty() ||
	ordered_normals.size() > INT_MAX)))
	return -1;

    /* Release collection hashes before allocating the final arrays and
     * pointer-to-index maps.  On million-triangle meshes, retaining both
     * generations at once needlessly adds hundreds of MiB to peak use. */
    std::unordered_set<ON_3dPoint *>().swap(point_seen);
    std::unordered_set<ON_3dPoint *>().swap(normal_seen);

    // We know how many faces, points and normals we need now - initialize BoT containers.
    *fcnt = (int)triangle_cnt;
    *faces = (int *)bu_calloc(triangle_cnt*3, sizeof(int), "new faces array");
    *vcnt = (int)ordered_points.size();
    *vertices = (fastf_t *)bu_calloc(ordered_points.size()*3,
	sizeof(fastf_t), "new vert array");
    if (normals) {
	*ncnt = (int)ordered_normals.size();
	*normals = (fastf_t *)bu_calloc(ordered_normals.size()*3,
	    sizeof(fastf_t), "new normals array");
	*fn_cnt = (int)triangle_cnt;
	*face_normals = (int *)bu_calloc(triangle_cnt*3, sizeof(int),
	    "new face_normals array");
    }

    // Index vertex points and assign them to the BoT array
    std::unordered_map<ON_3dPoint *, int> on_pnt_to_bot_pnt;
    on_pnt_to_bot_pnt.reserve(ordered_points.size());
    s_cdt->bot_pnt_to_on_pnt->assign(ordered_points.size(), NULL);
    int pnt_ind = 0;
    for (size_t pi = 0; pi < ordered_points.size(); ++pi) {
	ON_3dPoint *vp = ordered_points[pi].first;
	(*vertices)[pnt_ind*3] = vp->x;
	(*vertices)[pnt_ind*3+1] = vp->y;
	(*vertices)[pnt_ind*3+2] = vp->z;
	on_pnt_to_bot_pnt.emplace(vp, pnt_ind);
	(*s_cdt->bot_pnt_to_on_pnt)[(size_t)pnt_ind] = vp;
	pnt_ind++;
    }

    // Index vertex normal vectors and assign them to the BoT array.  Normal
    // vectors are not always uniquely mapped to vertices (consider, for
    // example, the triangles joining at a sharp edge of a box), but what we
    // are doing here is establishing unique integer identifiers for all normal
    // vectors.  In other words, this is not a unique vertex to normal mapping,
    // but a normal vector to unique index mapping.
    //
    // The mapping of 2D triangle point to its associated normal is the
    // responsibility of the  p2t_to_on3_norm_map container
    std::unordered_map<ON_3dPoint *, int> on_norm_to_bot_norm;
    size_t norm_ind = 0;
    if (normals) {
	on_norm_to_bot_norm.reserve(ordered_normals.size());
	for (size_t ni = 0; ni < ordered_normals.size(); ++ni) {
	    ON_3dPoint *vn = ordered_normals[ni].first;
	    ON_3dVector vnf(*vn);
	    if (flip_normals.find(vn) != flip_normals.end()) {
		vnf = -1 *vnf;
	    }
	    (*normals)[norm_ind*3] = vnf.x;
	    (*normals)[norm_ind*3+1] = vnf.y;
	    (*normals)[norm_ind*3+2] = vnf.z;
	    on_norm_to_bot_norm.emplace(vn, (int)norm_ind);
	    norm_ind++;
	}
    }

    // Iterate over faces, adding points and faces to BoT container.  Note: all
    // 3D points should be geometrically unique in this final container.
    int face_cnt = 0;
    std::vector<int> output_face_ids;
    std::vector<size_t> output_triangle_ids;
    output_face_ids.reserve(triangle_cnt);
    output_triangle_ids.reserve(triangle_cnt);
    for (size_t fi = 0; fi < active_faces.size(); fi++) {
	cdt_mesh_t *fmesh = &s_cdt->fmeshes[active_faces[fi]];
	const std::vector<size_t> &face_triangles =
	    active_triangles[active_faces[fi]];
	for (size_t ti = 0; ti < face_triangles.size(); ++ti) {
	    triangle_t tri = fmesh->tris_vect[face_triangles[ti]];
	    for (size_t j = 0; j < 3; j++) {
		ON_3dPoint *op = fmesh->pnts[tri.v[j]];
		const auto output_point = on_pnt_to_bot_pnt.find(op);
		if (output_point == on_pnt_to_bot_pnt.end())
		    return -1;
		(*faces)[face_cnt*3 + j] = output_point->second;

		if (normals) {
		    ON_3dPoint *onorm;
		    if (s_cdt->singular_vert_to_norms->find(op) != s_cdt->singular_vert_to_norms->end()) {
			// Use calculated normal for singularity points
			onorm = (*s_cdt->singular_vert_to_norms)[op];
		    } else {
			onorm = fmesh->normals[fmesh->nmap[fmesh->p2ind[op]]];
		    }

		    const auto output_normal =
			on_norm_to_bot_norm.find(onorm);
		    if (output_normal == on_norm_to_bot_norm.end())
			return -1;
		    (*face_normals)[face_cnt*3 + j] =
			output_normal->second;
		}
	    }

	    output_face_ids.push_back(fmesh->f_id);
	    output_triangle_ids.push_back(face_triangles[ti]);
	    face_cnt++;
	}
    }

    /* A complete, topologically solid BREP supplies authoritative shell
     * orientation.  If tessellation produced an otherwise closed/manifold
     * mesh with inconsistent triangle winding, repair that winding only when
     * the fully synchronized candidate validates as a solid.  Partial/open
     * meshes retain their original per-face ordering. */
    if (!exp_face_cnt || !exp_faces) {
	const bool source_is_solid = s_cdt->orig_brep &&
	    s_cdt->orig_brep->IsSolid();
	const int removed = closed_mesh_component_filter(*faces, fcnt,
	    *vertices, *vcnt, face_normals ? *face_normals : NULL,
	    output_face_ids.data(), std::max(s_cdt->absmin,
		ON_ZERO_TOLERANCE), source_is_solid, true,
	    output_triangle_ids.data());
	if (removed > 0) {
	    bu_log("%s: removed %d redundant open triangle artifact%s after "
		"transactional component validation\n",
		s_cdt->name ? s_cdt->name : "BREP", removed,
		removed == 1 ? "" : "s");
	}
	if (source_is_solid) {
	    const int synchronized = closed_mesh_orientation_sync(*faces, *fcnt,
		*vertices, *vcnt, face_normals ? *face_normals : NULL);
	    if (synchronized > 0) {
		bu_log("%s: synchronized %d triangle orientations after complete closed-mesh validation\n",
		    s_cdt->name ? s_cdt->name : "BREP", synchronized);
	    }
	}

	/* Component filtering intentionally keeps vertex indices stable while it
	 * edits the face array.  Once that transaction and orientation repair are
	 * complete, compact the exported vertex array and its source-point map so
	 * rigorous assembled validation does not mistake legal stale BoT entries
	 * for a geometric failure. */
	std::vector<int> vertex_remap((size_t)*vcnt, -1);
	int compact_vertex_count = 0;
	for (int face = 0; face < *fcnt; ++face) {
	    for (int corner = 0; corner < 3; ++corner) {
		const int vertex = (*faces)[(size_t)face * 3 + corner];
		if (vertex >= 0 && vertex < *vcnt &&
			vertex_remap[(size_t)vertex] < 0)
		    vertex_remap[(size_t)vertex] = compact_vertex_count++;
	    }
	}
	if (compact_vertex_count < *vcnt) {
	    std::vector<ON_3dPoint *> compact_point_map(
		(size_t)compact_vertex_count, NULL);
	    for (int old_vertex = 0; old_vertex < *vcnt; ++old_vertex) {
		const int new_vertex = vertex_remap[(size_t)old_vertex];
		if (new_vertex < 0)
		    continue;
		for (int axis = 0; axis < 3; ++axis)
		    (*vertices)[(size_t)new_vertex * 3 + axis] =
			(*vertices)[(size_t)old_vertex * 3 + axis];
		if ((size_t)old_vertex < s_cdt->bot_pnt_to_on_pnt->size())
		    compact_point_map[(size_t)new_vertex] =
			(*s_cdt->bot_pnt_to_on_pnt)[(size_t)old_vertex];
	    }
	    for (int face = 0; face < *fcnt; ++face) {
		for (int corner = 0; corner < 3; ++corner) {
		    int &vertex = (*faces)[(size_t)face * 3 + corner];
		    vertex = vertex_remap[(size_t)vertex];
		}
	    }
	    s_cdt->bot_pnt_to_on_pnt->swap(compact_point_map);
	    *vcnt = compact_vertex_count;
	}
    }

    output_face_ids.resize((size_t)*fcnt);
    output_triangle_ids.resize((size_t)*fcnt);
    s_cdt->bot_face_to_brep_face = output_face_ids;
    s_cdt->bot_face_to_cdt_triangle = output_triangle_ids;

    return 0;
}

int
cdt_test_spurious_components(void)
{
    fastf_t vertices[] = {
	0.0, 0.0, 0.0,
	1.0, 0.0, 0.0,
	0.0, 1.0, 0.0,
	0.0, 0.0, 1.0,
	2.0, 0.0, 0.0,
	3.0, 0.0, 0.0,
	2.0, 1.0, 0.0,
	4.0, 0.0, 0.0
    };
    int tetra_and_open[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3,
	4, 5, 6
    };
    int face_count = 5;
    if (closed_mesh_component_filter(tetra_and_open, &face_count, vertices,
	    8, NULL) != 0 || face_count != 5)
	return 1;

    int two_open_components[] = {0, 1, 2, 4, 5, 6};
    face_count = 2;
    if (closed_mesh_component_filter(two_open_components, &face_count,
	    vertices, 8, NULL) != 0 || face_count != 2)
	return 2;

    fastf_t two_tetra_vertices[] = {
	0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0, 0.0, 1.0,
	3.0, 0.0, 0.0, 4.0, 0.0, 0.0,
	3.0, 1.0, 0.0, 3.0, 0.0, 1.0
    };
    int two_tetra_faces[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3,
	4, 6, 5, 4, 5, 7, 4, 7, 6, 5, 6, 7
    };
    face_count = 8;
    if (closed_mesh_component_filter(two_tetra_faces, &face_count,
	    two_tetra_vertices, 8, NULL) != 0 || face_count != 8)
	return 3;

    fastf_t open_duplicate_vertices[] = {
	0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
	1.0, 1.0, 0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
	1.0, 1.0, 0.0
    };
    int open_duplicate_faces[] = {
	0, 1, 2, 0, 2, 3, 4, 5, 6
    };
    int open_duplicate_sources[] = {7, 7, 7};
    size_t open_duplicate_triangles[] = {10, 11, 12};
    face_count = 3;
    if (closed_mesh_component_filter(open_duplicate_faces, &face_count,
	    open_duplicate_vertices, 7, NULL, open_duplicate_sources,
	    1.0e-12, true, true, open_duplicate_triangles) != 1 ||
	    face_count != 2 || open_duplicate_triangles[0] != 10 ||
	    open_duplicate_triangles[1] != 11)
	return 4;

    open_duplicate_vertices[20] = 0.1;
    int distinct_faces[] = {0, 1, 2, 0, 2, 3, 4, 5, 6};
    face_count = 3;
    if (closed_mesh_component_filter(distinct_faces, &face_count,
	    open_duplicate_vertices, 7, NULL, open_duplicate_sources,
	    1.0e-6, true, false) != 0 || face_count != 3)
	return 5;

    /* An open triangle attached to a closed shell through an edge with three
     * uses is not enough by itself to prove redundancy.  Without source-face
     * coverage metadata the cleanup must preserve the candidate and let the
     * rigorous topology audit reject it. */
    fastf_t nonmanifold_vertices[] = {
	0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0, 0.0, 1.0,
	1.0, 0.0, 0.0
    };
    int nonmanifold_faces[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3,
	0, 2, 4
    };
    face_count = 5;
    if (closed_mesh_component_filter(nonmanifold_faces, &face_count,
	    nonmanifold_vertices, 5, NULL) != 0 || face_count != 5)
	return 6;

    /* Face-local vertices can collapse to one shared output vertex.  The
     * resulting index-degenerate triangle must not survive into a BoT or
     * distort component discovery. */
    int degenerate_faces[] = {
	0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3,
	4, 4, 1
    };
    face_count = 5;
    if (closed_mesh_component_filter(degenerate_faces, &face_count,
	    nonmanifold_vertices, 5, NULL) != 1 || face_count != 4)
	return 7;

    int invalid_faces[] = {0, 1, 2, 3, 3, 99};
    const int invalid_original[] = {0, 1, 2, 3, 3, 99};
    face_count = 2;
    if (closed_mesh_component_filter(invalid_faces, &face_count,
	    nonmanifold_vertices, 5, NULL) != 0 || face_count != 2 ||
	    !std::equal(invalid_faces, invalid_faces + 6, invalid_original))
	return 8;

    return 0;
}

// TODO - need a function that not only checks the integrity of
// each face mesh, but also validates all the boundary edge related
// information needed to stitch them together.  This is beyond the
// scope of the face mesh validity routines, so we'll have to do it
// at the CDT state level.
bool
CDT_Audit(struct ON_Brep_CDT_State *s_cdt)
{
    bool ret = true;
    if (!s_cdt) return false;

    std::map<int, std::set<bedge_seg_t *>>::iterator ps_it;

    int bedge_cnt = 0;

    for (ps_it = s_cdt->e2polysegs.begin(); ps_it != s_cdt->e2polysegs.end(); ps_it++) {
	/* A mesh-only collapsed edge has no triangle edge by construction. */
	if (s_cdt->collapsed_edges.find(ps_it->first) !=
		s_cdt->collapsed_edges.end())
	    continue;
	std::set<bedge_seg_t *>::iterator b_it;
	for (b_it = ps_it->second.begin(); b_it != ps_it->second.end(); b_it++) {
	    bedge_seg_t *eseg = *b_it;
	    std::vector<std::pair<cdt_mesh_t *,uedge_t>> uedges = eseg->uedges();
	    cdt_mesh_t *fmesh_f1 = uedges[0].first;
	    cdt_mesh_t *fmesh_f2 = uedges[1].first;
	    uedge_t ue1 = uedges[0].second;
	    uedge_t ue2 = uedges[1].second;
	    std::set<size_t> f1_tris = fmesh_f1->uedges2tris[ue1];
	    std::set<size_t> f2_tris = fmesh_f2->uedges2tris[ue2];

	    bool bad_tris = false;

	    bad_tris = (bad_tris || ((fmesh_f1->f_id != fmesh_f2->f_id) && (f1_tris.size() != 1 || f2_tris.size() != 1)));
	    bad_tris = (bad_tris || ((fmesh_f1->f_id == fmesh_f2->f_id) && ((f1_tris.size() != 0 && f1_tris.size() != 2) || (f2_tris.size() != 0 && f2_tris.size() != 2) )));
	    bad_tris = (bad_tris || (!f1_tris.size() && !f2_tris.size()));

	    if (bad_tris) {
		if (fmesh_f1->f_id == fmesh_f2->f_id) {
		    if (f1_tris.size() == 1 || (!f1_tris.size() && !f2_tris.size())) {
			std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f1->name << "," << fmesh_f1->f_id << "\n";
			std::cerr << "uedge: " << ue1.v[0] << "," << ue1.v[1] << "\n";
			std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh_f1->f_id) + std::string(".plot3");
			fmesh_f1->tris_plot(fpname.c_str());
		    }
		    if (f2_tris.size() == 1 || (!f1_tris.size() && !f2_tris.size())) {
			std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f2->name << "," << fmesh_f2->f_id  << "\n";
			std::cerr << "uedge: " << ue2.v[0] << "," << ue2.v[1] << "\n";
			std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh_f2->f_id) + std::string(".plot3");
			fmesh_f2->tris_plot(fpname.c_str());
		    }
		} else {
		    if (f1_tris.size() == 0) {
			std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f1->name << "," << fmesh_f1->f_id << "\n";
			std::cerr << "uedge: " << ue1.v[0] << "," << ue1.v[1] << "\n";
			std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh_f1->f_id) + std::string(".plot3");
			fmesh_f1->tris_plot(fpname.c_str());
		    }
		    if (f2_tris.size() == 0) {
			std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f2->name << "," << fmesh_f2->f_id  << "\n";
			std::cerr << "uedge: " << ue2.v[0] << "," << ue2.v[1] << "\n";
			std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh_f2->f_id) + std::string(".plot3");
			fmesh_f2->tris_plot(fpname.c_str());
		    }
		}
		std::string ename = std::string(s_cdt->name) + std::string("_edge_") + std::to_string(bedge_cnt) + std::string(".plot3");
		eseg->plot(ename.c_str());
		ret = false;
	    }
	    bedge_cnt++;
	}
    }

    return ret;
}

/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
