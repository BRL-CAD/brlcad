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
assembled_mesh_collect_candidate(size_t triangle, void *context)
{
    std::vector<size_t> *candidates =
	(std::vector<size_t> *)context;
    candidates->push_back(triangle);
    return true;
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
	const ON_3dVector ab = points[1] - points[0];
	const ON_3dVector ac = points[2] - points[0];
	const ON_3dVector bc = points[2] - points[1];
	const double longest_sq = std::max(ab.LengthSquared(),
	    std::max(ac.LengthSquared(), bc.LengthSquared()));
	const double doubled_area = ON_CrossProduct(ab, ac).Length();
	if (!(longest_sq > 0.0) || !std::isfinite(doubled_area) ||
		doubled_area <= 64.0 *
		std::numeric_limits<double>::epsilon() * longest_sq)
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
    bg_free_trimesh_solid_errors(&errors);
    if (!only_misoriented)
	return -1;

    std::vector<int> original(faces, faces + 3 * face_count);
    std::vector<int> candidate(original);
    if (bg_trimesh_sync(candidate.data(), candidate.data(), face_count) <= 0)
	return -1;

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
	if (changed * 2 == component.size())
	    return -1;
	if (changed * 2 > component.size()) {
	    for (int face : component)
		std::swap(candidate[(size_t)face * 3],
		    candidate[(size_t)face * 3 + 1]);
	}
    }

    if (bg_trimesh_solid2(vertex_count, face_count, vertices,
	    candidate.data(), NULL) != 0)
	return -1;

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
refine_triangulation(struct ON_Brep_CDT_State *s_cdt, cdt_mesh_t *fmesh, int cnt, int rebuild)
{
    if (!s_cdt || !fmesh) return false;

    if (cnt > MAX_TRIANGULATION_ATTEMPTS) {
	std::cerr << "Error: even after " << MAX_TRIANGULATION_ATTEMPTS << " iterations could not successfully refine triangulate face " << fmesh->f_id << " for solidity criteria\n";
	return false;
    }

    // If a previous pass has made changes in which points are active in the
    // surface set, we need to rebuild the whole triangulation.

    if (rebuild && !fmesh->cdt()) {
	bu_log("Fatal failure attempting full retriangulation of face\n");
	return false;
    }

    // A chart triangulation which already satisfies the face invariants does
    // not need the legacy UV repair heuristics.  In particular, applying a
    // best-fit-plane reparameterization to a valid singular chart can
    // reintroduce the degeneracy the chart was constructed to remove.

    const bool topology_chart = cdt_face_uses_topology_chart(
	s_cdt->brep->m_F[fmesh->f_id]);
    if (fmesh->valid(0))
	return true;
    if (fmesh->repair_incorrect_normal_edges() && fmesh->valid(0))
	return true;
    if (fmesh->repair_toleranced_nonmanifold_edges())
	return true;

    /* Periodic seam copies can make distinct chart cells share the same
     * three model-space vertices.  Separate those cells before geometric
     * fold and intersection refinement works on the stitched mesh. */
    size_t collapsed_chart_points = 0;
    for (int attempt = 0; attempt < MAX_CHART_REFINEMENT_ATTEMPTS;
	    ++attempt) {
	const size_t remaining = MAX_CHART_REFINEMENT_POINTS -
	    collapsed_chart_points;
	if (!remaining)
	    break;
	const size_t inserted =
	    fmesh->refine_collapsed_chart_triangles(remaining);
	if (!inserted)
	    break;
	collapsed_chart_points += inserted;
	if (!fmesh->cdt()) {
	    bu_log("Face %d: collapsed chart cell retriangulation failed\n",
		fmesh->f_id);
	    return false;
	}
	if (fmesh->valid(0) || (fmesh->repair_incorrect_normal_edges() &&
		fmesh->valid(0))) {
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
	if (!fmesh->cdt()) {
	    bu_log("Face %d: chart refinement retriangulation failed\n",
		fmesh->f_id);
	    return false;
	}
	if (fmesh->valid(0) || (fmesh->repair_incorrect_normal_edges() &&
		fmesh->valid(0))) {
	    bu_log("Face %d: chart refinement certified after %zu "
		"inserted points\n", fmesh->f_id, refinement_points);
	    return true;
	}
	if (fmesh->repair_toleranced_nonmanifold_edges())
	    return true;
	const size_t current_folds = fmesh->incorrect_normal_count();
	const size_t current_intersections = fmesh->self_intersections(NULL,
	    MAX_CHART_REFINEMENT_POINTS);
	const size_t current_defects = current_folds + current_intersections;
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
	return refine_triangulation(s_cdt, mesh, 0, 0) &&
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
	if (do_triangulation(s_cdt, fi)) {
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
	    s_cdt->diagnostic.result == BREP_CDT_RESULT_REFINEMENT_LIMIT;
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

    const bool have_rigorous_faces = !s_cdt->fmeshes.empty();
    if (!have_rigorous_faces && !settings->use_full_fast_fallback) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces,
	    "mesh repair requires usable rigorous faces or the explicit "
	    "whole-B-Rep fast fallback");
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
    if (have_rigorous_faces && usable_faces.empty() &&
	!settings->use_full_fast_fallback) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1, 0,
	    report->source_failed_faces,
	    "mesh repair has no successfully triangulated B-Rep faces");
	return -1;
    }

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
    std::map<repair_triangle_key, std::set<int>> fast_triangle_brep_faces;
    /* Whole-B-Rep fallback replaces every rigorous face mesh.  Do not spend
     * another bounded refinement cycle assembling those faces after a stage
     * 11 failure only to discard the result below. */
    if (!settings->use_full_fast_fallback && !usable_faces.empty()) {
	if (ON_Brep_CDT_Mesh(&input_faces, &input_face_count, &input_vertices,
	    &input_vertex_count, NULL, NULL, NULL, NULL, s_cdt,
	    (int)usable_faces.size(), usable_faces.data()) < 0 ||
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
		NULL, s_cdt, (int)usable_faces.size(), usable_faces.data()) < 0 ||
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
    const std::vector<int> rigorous_input_brep_faces =
	s_cdt->bot_face_to_brep_face;
    std::set<int> locally_reconstructed_faces;

    std::map<repair_point_key, int> approximate_points;
    for (int point = 0; point < input_vertex_count; ++point) {
	approximate_points[repair_point_key{
	    input_vertices[(size_t)point * 3],
	    input_vertices[(size_t)point * 3 + 1],
	    input_vertices[(size_t)point * 3 + 2]}] = point;
    }
    const auto append_approximate_face = [&](int face_index) {
	int *local_faces = NULL;
	int local_face_count = 0;
	fastf_t *local_vertices = NULL;
	int local_vertex_count = 0;
	int one_face = face_index;
	const bool assembled = ON_Brep_CDT_Mesh(&local_faces,
	    &local_face_count, &local_vertices, &local_vertex_count,
	    NULL, NULL, NULL, NULL, s_cdt, 1, &one_face) == 0 &&
	    local_faces && local_face_count > 0 && local_vertices &&
	    local_vertex_count > 0 && local_vertex_count <=
	    INT_MAX - input_vertex_count && local_face_count <=
	    INT_MAX - input_face_count;
	if (!assembled) {
	    bu_free(local_faces, "approximate face mesh faces");
	    bu_free(local_vertices, "approximate face mesh vertices");
	    return 0;
	}
	std::vector<int> local_to_input((size_t)local_vertex_count, -1);
	std::vector<fastf_t> novel_vertices;
	novel_vertices.reserve((size_t)local_vertex_count * 3);
	for (int point = 0; point < local_vertex_count; ++point) {
	    const repair_point_key key = {
		local_vertices[(size_t)point * 3],
		local_vertices[(size_t)point * 3 + 1],
		local_vertices[(size_t)point * 3 + 2]};
	    const auto existing = approximate_points.find(key);
	    if (existing != approximate_points.end()) {
		local_to_input[(size_t)point] = existing->second;
		continue;
	    }
	    const int new_index = input_vertex_count +
		(int)(novel_vertices.size() / 3);
	    approximate_points[key] = new_index;
	    local_to_input[(size_t)point] = new_index;
	    novel_vertices.insert(novel_vertices.end(),
		&local_vertices[(size_t)point * 3],
		&local_vertices[(size_t)point * 3] + 3);
	}
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
	bu_free(local_faces, "approximate face mesh faces");
	bu_free(local_vertices, "approximate face mesh vertices");
	return local_face_count;
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
    const int best_effort_reference_face_count = input_face_count;
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
	    const int local_face_count =
		append_approximate_face(failed_face);
	    if (!local_face_count)
		continue;
	    locally_reconstructed_faces.insert(failed_face);
	    bu_log("Face %d: retained a locally cleaned surface chart for "
		"repair (%d triangles)\n", failed_face, local_face_count);
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
	report->fast_fallback_used_faces = fast_report.completed_faces;
	report->fast_fallback_failed_faces = fast_report.failed_faces;
	if (!report->fast_fallback_triangles)
	    report->fast_fallback_triangles = fast_face_count;
	report->full_fast_fallback_used = 1;
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

    if (!settings->use_full_fast_fallback &&
	    settings->use_fast_face_fallback && !failed_faces.empty()) {
	const int64_t fast_start = bu_gettime();
	size_t fast_points_used = 0;
	size_t fast_bytes_used = 0;
	std::map<repair_point_key, int> assembled_points;
	for (int point_index = 0; point_index < input_vertex_count;
		++point_index) {
	    assembled_points[repair_point_key{
		input_vertices[(size_t)point_index * 3],
		input_vertices[(size_t)point_index * 3 + 1],
		input_vertices[(size_t)point_index * 3 + 2]}] = point_index;
	}
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
	    if (constraints.constrained_edges) {
		fast_options.trim_sample_count =
		    repair_fast_trim_sample_count;
		fast_options.trim_sample = repair_fast_trim_sample_get;
		fast_options.trim_sample_data = &constraints;
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
		novel_vertices.reserve((size_t)fast_point_count * 3);
		for (int fast_point = 0; fast_point < fast_point_count;
			++fast_point) {
		    const repair_point_key key = {
			fast_points[fast_point][X], fast_points[fast_point][Y],
			fast_points[fast_point][Z]};
		    const auto existing = assembled_points.find(key);
		    if (existing != assembled_points.end()) {
			fast_to_input[(size_t)fast_point] = existing->second;
			continue;
		    }
		    const int new_index = input_vertex_count +
			(int)(novel_vertices.size() / 3);
		    assembled_points[key] = new_index;
		    fast_to_input[(size_t)fast_point] = new_index;
		    novel_vertices.insert(novel_vertices.end(),
			&fast_points[fast_point][X], &fast_points[fast_point][X] + 3);
		}
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
				s_cdt->bot_face_to_brep_face.size())
			    return std::string("rigorous B-Rep face ") +
				std::to_string(s_cdt->bot_face_to_brep_face[
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
	report->mesh.input_area;
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
    const fastf_t guarded_area_change = have_display_reference ?
	report->reference_area_change_percent : report->area_change_percent;
    if (settings->max_area_change_percent > 0.0 &&
	    (!(guarded_area_change <= settings->max_area_change_percent))) {
	bu_free(repaired_faces, "certified repaired faces");
	bu_free(repaired_points, "certified repaired vertices");
	bu_free(input_faces, "repair input faces");
	bu_free(input_vertices, "repair input vertices");
	std::string message = have_display_reference ?
	    "repaired mesh area differs from display reference by " :
	    "repaired mesh area changed by ";
	message += std::to_string(guarded_area_change) +
	    "% (limit " +
	    std::to_string(settings->max_area_change_percent) + "%)";
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }

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
		fast_source->second.size() == 1)
	    source_brep_face = *fast_source->second.begin();
	const ON_3dPoint a(&repaired_vertices[(size_t)triangle[0] * 3]);
	const ON_3dPoint b(&repaired_vertices[(size_t)triangle[1] * 3]);
	const ON_3dPoint c(&repaired_vertices[(size_t)triangle[2] * 3]);
	changed_faces.push_back({face,
	    0.5 * ON_CrossProduct(b - a, c - a).Length(),
	    source_brep_face, local_surface_approximation});
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

    RTree<size_t, double, 3> input_triangle_index;
    const int *reference_input_faces = have_display_reference ?
	display_reference_faces.data() : input_faces;
    const fastf_t *reference_input_vertices = have_display_reference ?
	display_reference_vertices.data() : input_vertices;
    const int surface_reference_face_count = have_display_reference ?
	display_reference_face_count :
	(report->full_fast_fallback_used ? input_face_count :
	best_effort_reference_face_count);
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
	    if (!local_filter.empty())
		matched_local_surface = repair_surface_distance(
		    s_cdt->orig_brep, face_bounds, face_trees, face_contexts,
		    samples[sample], report->allowed_surface_deviation, true,
		    &distance, &used_untrimmed, NULL, NULL, &local_filter);
	    if (!matched_local_surface &&
		    !repair_surface_distance(s_cdt->orig_brep, face_bounds,
		    face_trees, face_contexts, samples[sample],
		    report->allowed_surface_deviation,
		    settings->allow_untrimmed_surface_match != 0, &distance,
		    &used_untrimmed)) {
		if (!repair_input_mesh_distance(input_triangle_index,
			reference_input_vertices, reference_input_faces,
			samples[sample],
			report->allowed_surface_deviation, &distance)) {
		    const double diagnostic_limit =
			report->allowed_surface_deviation <=
			std::numeric_limits<double>::max() / 4.0 ?
			4.0 * report->allowed_surface_deviation :
			report->allowed_surface_deviation;
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
	    report->allowed_surface_deviation) {
	bu_free(repaired_faces, "certified repaired faces");
	bu_free(repaired_points, "certified repaired vertices");
	bu_free(input_faces, "repair input faces");
	bu_free(input_vertices, "repair input vertices");
	std::string message = "repaired surface failed B-Rep deviation check: " +
	    std::to_string(report->deviation_projection_failures) +
	    " projection failures, maximum " +
	    std::to_string(report->max_surface_deviation) +
	    " (limit " +
	    std::to_string(report->allowed_surface_deviation) + ")";
	if (worst_deviation_brep_face >= 0)
	    message += ", nearest display B-Rep face " +
		std::to_string(worst_deviation_brep_face);
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }

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
		report->allowed_surface_deviation, &distance)) {
	    report->coverage_failures++;
	    const double diagnostic_limit =
		report->allowed_surface_deviation <=
		std::numeric_limits<double>::max() / 4.0 ?
		4.0 * report->allowed_surface_deviation :
		report->allowed_surface_deviation;
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
	    report->allowed_surface_deviation) {
	bu_free(repaired_faces, "certified repaired faces");
	bu_free(repaired_points, "certified repaired vertices");
	bu_free(input_faces, "repair input faces");
	bu_free(input_vertices, "repair input vertices");
	std::string message = "repaired surface failed input coverage check: " +
	    std::to_string(report->coverage_failures) +
	    " coverage failures, maximum " +
	    std::to_string(report->max_coverage_deviation) + " (limit " +
	    std::to_string(report->allowed_surface_deviation) + ")";
	if (worst_coverage_brep_face >= 0)
	    message += ", worst B-Rep face " +
		std::to_string(worst_coverage_brep_face);
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }

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
    if (report->poisson_reconstruction_applied)
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
    if (report->approximation_tier >= BREP_CDT_REPAIR_APPROX_FULL_FAST) {
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
	std::to_string(report->mesh.removed_faces) + " faces removed, " +
	std::to_string(report->mesh.added_faces) + " added, " +
	std::to_string(report->subdivided_rigorous_triangles) +
	" rigorous triangles subdivided, " +
	std::to_string(report->missing_rigorous_triangles) +
	" rigorously sampled triangles locally replaced, " +
	std::to_string(report->best_effort_triangles) +
	" best-effort surface triangles retained, " +
	std::to_string(report->changed_faces) +
	" changed triangles, sampled maximum reference deviation " +
	std::to_string(report->max_surface_deviation);
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
