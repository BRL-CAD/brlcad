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
#include <cmath>
#include <iostream>
#include <iterator>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "bg/chull.h"
#include "bg/tri_pt.h"
#include "bg/tri_tri.h"
#include "./cdt.h"

#define BREP_PLANAR_TOL 0.05
#define MAX_TRIANGULATION_ATTEMPTS 5
#define MAX_CHART_REFINEMENT_ATTEMPTS 16
#define MAX_CHART_REFINEMENT_POINTS 4096
#define MAX_CHART_REFINEMENT_STAGNANT_ATTEMPTS 4
#define MAX_ASSEMBLED_REFINEMENT_ATTEMPTS 16
#define MAX_ASSEMBLED_REFINEMENT_POINTS 4096

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
	assembled_mesh_validation *validation)
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
    if (!validation->nonfinite_vertices && !validation->degenerate_faces) {
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
    std::vector<std::vector<int>> adjacent((size_t)face_count);
    std::map<std::pair<int, int>, int> first_edge_face;
    for (int face = 0; face < face_count; ++face) {
	for (int edge = 0; edge < 3; ++edge) {
	    int vertex_a = original[(size_t)face * 3 + edge];
	    int vertex_b = original[(size_t)face * 3 + (edge + 1) % 3];
	    if (vertex_a > vertex_b)
		std::swap(vertex_a, vertex_b);
	    const std::pair<int, int> key(vertex_a, vertex_b);
	    auto first = first_edge_face.find(key);
	    if (first == first_edge_face.end()) {
		first_edge_face[key] = face;
		continue;
	    }
	    adjacent[(size_t)face].push_back(first->second);
	    adjacent[(size_t)first->second].push_back(face);
	}
    }

    std::vector<int> component_id((size_t)face_count, -1);
    std::vector<std::vector<int>> components;
    for (int seed = 0; seed < face_count; ++seed) {
	if (component_id[(size_t)seed] >= 0)
	    continue;
	const int current_id = (int)components.size();
	components.push_back(std::vector<int>());
	std::queue<int> work;
	work.push(seed);
	component_id[(size_t)seed] = current_id;
	while (!work.empty()) {
	    const int face = work.front();
	    work.pop();
	    components[(size_t)current_id].push_back(face);
	    for (int neighbor : adjacent[(size_t)face]) {
		if (component_id[(size_t)neighbor] >= 0)
		    continue;
		component_id[(size_t)neighbor] = current_id;
		work.push(neighbor);
	    }
	}
    }

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

    std::map<std::pair<int, int>, std::vector<int>> edge_faces;
    for (int face = 0; face < *face_count; ++face) {
	for (int corner = 0; corner < 3; ++corner) {
	    int first = faces[(size_t)face * 3 + corner];
	    int second = faces[(size_t)face * 3 + (corner + 1) % 3];
	    if (first > second)
		std::swap(first, second);
	    edge_faces[std::make_pair(first, second)].push_back(face);
	}
    }

    std::vector<int> component_id((size_t)*face_count, -1);
    std::vector<std::vector<int>> components;
    for (int seed = 0; seed < *face_count; ++seed) {
	if (component_id[(size_t)seed] >= 0)
	    continue;
	const int current_id = (int)components.size();
	components.push_back(std::vector<int>());
	std::queue<int> work;
	work.push(seed);
	component_id[(size_t)seed] = current_id;
	while (!work.empty()) {
	    const int face = work.front();
	    work.pop();
	    components[(size_t)current_id].push_back(face);
	    for (int corner = 0; corner < 3; ++corner) {
		int first = faces[(size_t)face * 3 + corner];
		int second =
		    faces[(size_t)face * 3 + (corner + 1) % 3];
		if (first > second)
		    std::swap(first, second);
		const std::vector<int> &edge_neighbors =
		    edge_faces[std::make_pair(first, second)];
		/* An edge used by more than two triangles is non-manifold.  It
		 * must not join an otherwise closed shell to a local remesh
		 * artifact during component discovery.  Restricting adjacency
		 * to exactly two uses also matches the topological connectivity
		 * required by a valid solid mesh. */
		if (edge_neighbors.size() != 2)
		    continue;
		for (int neighbor : edge_neighbors) {
		    if (component_id[(size_t)neighbor] >= 0)
			continue;
		    component_id[(size_t)neighbor] = current_id;
		    work.push(neighbor);
		}
	    }
	}
    }
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

    /* A partial display tessellation may not itself be closed (for example,
     * another BREP face may have independently failed to tessellate).  We can
     * still remove a disconnected local-remesh island when every one of its
     * vertices and its centroid lies on retained triangles from the same
     * source face.  This is an exact redundancy proof within the tessellation
     * tolerance, not a largest-component guess. */
    if (!retained_face_count && source_faces &&
	    overlap_tolerance > 0.0) {
	size_t largest = 0;
	for (size_t component = 1; component < components.size(); ++component) {
	    if (components[component].size() > components[largest].size())
		largest = component;
	}
	double minimum_failed_distance = DBL_MAX;
	int failed_source_face = -1;
	const auto point_covered =
	    [&](const point_t point, int source_face) {
		double minimum_distance = DBL_MAX;
		for (int candidate : components[largest]) {
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
	    if (component == largest)
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
	retain[largest] = true;
	retained_face_count = components[largest].size();
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

/* Collapse only explicit B-Rep edges whose complete curve is contained in
 * the fixed modeling tolerance.  Endpoint proximity alone is insufficient:
 * a long curve may close back on itself or join two otherwise unrelated
 * sheets.  The NURBS control-polygon length and curve bounding box are both
 * conservative guards for the whole edge.
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

    /* BN_TOL_DIST is an absolute modeling tolerance and can exceed an entire
     * valid micron-scale solid.  Never weld across a distance larger than the
     * minimum feature resolution derived from the caller's tolerance. */
    if (!std::isfinite(s_cdt->absmin) || s_cdt->absmin <= 0.0)
	return 0;
    const double collapse_tolerance = std::min((double)BN_TOL_DIST,
	(double)s_cdt->absmin);
    struct collapse_candidate {
	int edge_index;
	ON_BoundingBox curve_bounds;
	std::vector<ON_3dPoint *> points;
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
	    std::vector<ON_3dPoint *>(members.begin(), members.end())};
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

    std::set<int> accepted_edges;
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
	for (size_t member : members)
	    roots.insert(root(member));
	for (size_t component : roots)
	    grow_bounds(combined, component_bounds[component]);
	const double combined_envelope = combined.Diagonal().Length();
	if (!std::isfinite(combined_envelope) ||
		combined_envelope > collapse_tolerance)
	    continue;

	const size_t combined_root = *roots.begin();
	for (size_t component : roots) {
	    if (component == combined_root)
		continue;
	    parent[component] = combined_root;
	    component_size[combined_root] += component_size[component];
	}
	component_bounds[combined_root] = combined;
	accepted_edges.insert(candidate.edge_index);
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
    s_cdt->collapsed_edges = accepted_edges;

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
    for (int edge_index : accepted_edges) {
	const ON_BrepEdge &edge = s_cdt->brep->m_E[edge_index];
	const bedge_seg_t *segment = *s_cdt->e2polysegs[edge_index].begin();
	bu_log("%s: collapsed sub-tolerance B-Rep edge %d (V%d/V%d, "
	    "control polygon %.17g, envelope %.17g, effective tolerance "
	    "%.17g, BN_TOL_DIST %.17g)\n",
	    s_cdt->name ? s_cdt->name : "BREP", edge_index,
	    edge.m_vi[0], edge.m_vi[1], segment->cp_len,
	    segment->nc->BoundingBox().Diagonal().Length(),
	    collapse_tolerance, (double)BN_TOL_DIST);
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
    ON_BrepVertex &v0 = source.m_V[0];
    ON_BrepVertex &v1 = source.m_V[1];
    ON_BrepVertex &v2 = source.m_V[2];
    ON_BrepVertex &v3 = source.m_V[3];
    ON_BrepVertex &v4 = source.m_V[4];
    ON_BrepVertex &v5 = source.m_V[5];
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
    bedge_seg_t *adjacent = *state->e2polysegs[1].begin();
    state->absmin = 1.0e-5;
    if (collapse_subtolerance_brep_edges(state) != 0) {
	ON_Brep_CDT_Destroy(state);
	return 4;
    }
    state->absmin = BN_TOL_DIST;
    const size_t collapsed = collapse_subtolerance_brep_edges(state);
    int result = 0;
    if (collapsed != 1 || state->collapsed_edges.size() != 1 ||
	    state->collapsed_edges.find(0) == state->collapsed_edges.end() ||
	    state->collapsed_edges.find(3) != state->collapsed_edges.end())
	result = 1;
    else if ((*state->vert_pnts)[0] != expected ||
	    (*state->vert_pnts)[1] != expected ||
	    (*state->vert_pnts)[2] == expected)
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
	    if (!pair_incident_to_shared_edge)
		break;
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
	if (nearest) {
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

int
ON_Brep_CDT_Tessellate(struct ON_Brep_CDT_State *s_cdt, int face_cnt, int *faces)
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
	bu_log("brep is NOT valid, cannot produce watertight mesh\n");
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INVALID_BREP,
	    BREP_CDT_STAGE_TOPOLOGY, -1, 0, 0,
	    "input B-Rep failed OpenNURBS validity checks");
	return -1;
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

	// Do the non-tolerance based initialization splits.
	if (!initialize_edge_segs(s_cdt)) {
	    std::cout << "Initialization failed for edges\n";
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_INITIALIZATION_FAILED,
		BREP_CDT_STAGE_EDGE_INITIALIZATION, -1, 0, 0,
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

    bu_free(valid_faces, "faces");
    bu_free(valid_vertices, "vertices");

    if (invalid) {
	s_cdt->status = BREP_CDT_NON_SOLID;
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_NON_SOLID,
	    BREP_CDT_STAGE_SOLID_VALIDATION, -1, face_successes, 0,
	    "assembled mesh failed closed-solid validation");
	return 1;
    }

    s_cdt->status = BREP_CDT_SOLID;
    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_SUCCESS,
	BREP_CDT_STAGE_SOLID_VALIDATION, -1, face_successes, 0,
	"closed indexed mesh passed incidence, link, and area validation");
    return 0;

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
    if (!faces || !fcnt || !vertices || !vcnt || !s_cdt || !s_cdt->brep) {
	return -1;
    }
    s_cdt->bot_face_to_brep_face.clear();
    s_cdt->bot_face_to_cdt_triangle.clear();

    /* We can ignore the face normals if we want, but if some of the
     * return variables are non-NULL they all need to be non-NULL */
    if (face_normals || fn_cnt || normals || ncnt) {
	if (!face_normals || !fn_cnt || !normals || !ncnt) {
	    return -1;
	}
    }

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

    /* We know now the final triangle set.  We need to build up the set of
     * unique points and normals to generate a mesh containing only the
     * information actually used by the final triangle set. */
    std::set<ON_3dPoint *> vfpnts;
    std::set<ON_3dPoint *> vfnormals;
    std::set<ON_3dPoint *> flip_normals;
    for (size_t fi = 0; fi < active_faces.size(); fi++) {
	cdt_mesh_t *fmesh = &s_cdt->fmeshes[active_faces[fi]];
	const std::vector<size_t> &face_triangles =
	    active_triangles[active_faces[fi]];
	for (size_t ti = 0; ti < face_triangles.size(); ++ti) {
	    triangle_t tri = fmesh->tris_vect[face_triangles[ti]];
	    for (size_t j = 0; j < 3; j++) {
		ON_3dPoint *p3d = fmesh->pnts[tri.v[j]];
		vfpnts.insert(p3d);
		ON_3dPoint *onorm = NULL;
		if (s_cdt->singular_vert_to_norms->find(p3d) != s_cdt->singular_vert_to_norms->end()) {
		    // Use calculated normal for singularity points
		    onorm = (*s_cdt->singular_vert_to_norms)[p3d];
		} else {
		    onorm = fmesh->normals[fmesh->nmap[tri.v[j]]];
		}
		if (onorm) {
		    vfnormals.insert(onorm);
		    if (fmesh->m_bRev) {
			flip_normals.insert(onorm);
		    }
		}
	    }
	}
    }

    //bu_log("tri_cnt: %zd\n", triangle_cnt);

    // We know how many faces, points and normals we need now - initialize BoT containers.
    *fcnt = (int)triangle_cnt;
    *faces = (int *)bu_calloc(triangle_cnt*3, sizeof(int), "new faces array");
    *vcnt = (int)vfpnts.size();
    *vertices = (fastf_t *)bu_calloc(vfpnts.size()*3, sizeof(fastf_t), "new vert array");
    if (normals) {
	*ncnt = (int)vfnormals.size();
	*normals = (fastf_t *)bu_calloc(vfnormals.size()*3, sizeof(fastf_t), "new normals array");
	*fn_cnt = (int)triangle_cnt;
	*face_normals = (int *)bu_calloc(triangle_cnt*3, sizeof(int), "new face_normals array");
    }

    // Populate the arrays and map the ON containers to their corresponding BoT array entries
    std::map<ON_3dPoint *, size_t> point_order;
    for (size_t i = 0; i < s_cdt->w3dpnts->size(); ++i)
	point_order[(*s_cdt->w3dpnts)[i]] = i;
    std::map<ON_3dPoint *, size_t> normal_order;
    for (size_t i = 0; i < s_cdt->w3dnorms->size(); ++i)
	normal_order[(*s_cdt->w3dnorms)[i]] = i;

    const auto stable_point_less = [&point_order](ON_3dPoint *a,
	    ON_3dPoint *b) {
	if (a->x < b->x) return true;
	if (b->x < a->x) return false;
	if (a->y < b->y) return true;
	if (b->y < a->y) return false;
	if (a->z < b->z) return true;
	if (b->z < a->z) return false;
	return point_order.at(a) < point_order.at(b);
    };
    const auto stable_normal_less = [&normal_order](ON_3dPoint *a,
	    ON_3dPoint *b) {
	if (a->x < b->x) return true;
	if (b->x < a->x) return false;
	if (a->y < b->y) return true;
	if (b->y < a->y) return false;
	if (a->z < b->z) return true;
	if (b->z < a->z) return false;
	return normal_order.at(a) < normal_order.at(b);
    };
    std::vector<ON_3dPoint *> ordered_points(vfpnts.begin(), vfpnts.end());
    std::sort(ordered_points.begin(), ordered_points.end(), stable_point_less);
    std::vector<ON_3dPoint *> ordered_normals(vfnormals.begin(),
	vfnormals.end());
    std::sort(ordered_normals.begin(), ordered_normals.end(),
	stable_normal_less);

    // Index vertex points and assign them to the BoT array
    std::map<ON_3dPoint *, int> on_pnt_to_bot_pnt;
    int pnt_ind = 0;
    for (size_t pi = 0; pi < ordered_points.size(); ++pi) {
	ON_3dPoint *vp = ordered_points[pi];
	(*vertices)[pnt_ind*3] = vp->x;
	(*vertices)[pnt_ind*3+1] = vp->y;
	(*vertices)[pnt_ind*3+2] = vp->z;
	on_pnt_to_bot_pnt[vp] = pnt_ind;
	(*s_cdt->bot_pnt_to_on_pnt)[pnt_ind] = vp;
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
    std::map<ON_3dPoint *, int> on_norm_to_bot_norm;
    size_t norm_ind = 0;
    if (normals) {
	for (size_t ni = 0; ni < ordered_normals.size(); ++ni) {
	    ON_3dPoint *vn = ordered_normals[ni];
	    ON_3dVector vnf(*vn);
	    if (flip_normals.find(vn) != flip_normals.end()) {
		vnf = -1 *vnf;
	    }
	    (*normals)[norm_ind*3] = vnf.x;
	    (*normals)[norm_ind*3+1] = vnf.y;
	    (*normals)[norm_ind*3+2] = vnf.z;
	    on_norm_to_bot_norm[vn] = (int)norm_ind;
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
		(*faces)[face_cnt*3 + j] = on_pnt_to_bot_pnt[op];

		if (normals) {
		    ON_3dPoint *onorm;
		    if (s_cdt->singular_vert_to_norms->find(op) != s_cdt->singular_vert_to_norms->end()) {
			// Use calculated normal for singularity points
			onorm = (*s_cdt->singular_vert_to_norms)[op];
		    } else {
			onorm = fmesh->normals[fmesh->nmap[fmesh->p2ind[op]]];
		    }

		    (*face_normals)[face_cnt*3 + j] = on_norm_to_bot_norm[onorm];
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
	    8, NULL) != 1 || face_count != 4)
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

    /* A redundant triangle attached to a closed shell through an edge with
     * three uses is not part of that manifold shell.  This is the compact
     * form of the NIST spherical-cap shading regression: treating the
     * non-manifold edge as ordinary adjacency hid the extra component from
     * the cleanup audit even though downstream BoT topology detected it. */
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
	    nonmanifold_vertices, 5, NULL) != 1 || face_count != 4)
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
